/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "UnrealPCGDataTranslator.h"

#include <Data/PCGPointData.h>

#include "UObject/TextProperty.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniInputObject.h"

#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniPCGUtils.h"
#include "HoudiniPCGInputObject.h"


namespace
{
	template<typename T, typename P>
	TArray<T>
		PopulateTArray(TMap<FName, uint8*>::TConstIterator It,
			uint32 NumRows,
			uint32 NumComponents,
			uint32 Offset,
			uint32 ComponentSize,
			const TArray<uint32>& Order)
	{
		TArray<T> Values;
		Values.Reserve(NumRows * NumComponents);
		for(; It; ++It)
		{
			const uint8* Data = It.Value();
			for(uint32 Idx = 0; Idx < NumComponents; ++Idx)
			{
				Values.Add(P::GetPropertyValue(Data + Offset + Order[Idx] * ComponentSize));
			}

		}
		return Values;
	}

	template<typename T, typename P>
	TArray<T>
		PopulateTArray(TMap<FName, uint8*>::TConstIterator It,
			uint32 NumRows,
			uint32 NumComponents,
			uint32 Offset,
			uint32 ComponentSize)
	{
		TArray<uint32> Order;
		Order.Reserve(NumComponents);
		for(uint32 Idx = 0; Idx < NumComponents; ++Idx)
		{
			Order.Add(Idx);
		}
		return PopulateTArray<T, P>(It, NumRows, NumComponents, Offset, ComponentSize, Order);
	}

	TArray<int8>
		PopulateBoolArray(TMap<FName, uint8*>::TConstIterator It,
			const FBoolProperty& Prop,
			uint32 Count,
			uint32 Offset)
	{
		TArray<int8> Values;
		Values.Reserve(Count);
		for(; It; ++It)
		{
			const uint8* Data = It.Value();
			Values.Add(Prop.GetPropertyValue(Data + Offset));
		}
		return Values;
	}
};

bool FUnrealPCGDataTranslator::CreateInputNodeForPCGData(
	UHoudiniPCGInputObject* PCGData,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	bool bInputNodesCanBeDeleted)
{
	FString FinalInputNodeName = InputNodeName;

	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;

	{
		const FUnrealObjectInputOptions Options;
		Identifier = FUnrealObjectInputIdentifier(PCGData, Options, true);

		FUnrealObjectInputHandle Handle;
		if(FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if(FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			{
				if(!bInputNodesCanBeDeleted)
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		if(FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// Set InputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that InputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if(Handle.IsValid())
		{
			if(!FUnrealObjectInputUtils::GetHAPINodeId(Handle, InputNodeId))
				InputNodeId = -1;
		}
		else
		{
			InputNodeId = -1;
		}
	}

	// Create the input node
	HAPI_NodeId NewNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(FinalInputNodeName, NewNodeId, ParentNodeId), false);

	if(!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;

	HAPI_NodeId PreviousInputNodeId = InputNodeId;
	InputNodeId = NewNodeId;
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);

	if(PreviousInputNodeId >= 0)
	{
		HAPI_NodeId PreviousInputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputNodeId) != HAPI_RESULT_SUCCESS)
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);

		if(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputObjectNodeId) != HAPI_RESULT_SUCCESS)
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input object node for %s."), *FinalInputNodeName);
	}


	CreateInputNodeForPCGParamData(PCGData, InputNodeId);

	return true;
}
#if 0
bool FUnrealPCGDataTranslator::CreateInputNodeForPCGPointData(
	UHoudiniPCGInputObject* PCGInputData,
	HAPI_NodeId& InputNodeId)
{
	if(PCGInputData->Attributes.IsEmpty())
		return true;

	int NumRows = PCGInputData->Attributes[0]->GetNumValues();

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = NumRows;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &Part), false);
	const HAPI_PartId PartId = 0;

	FHoudiniHapiAccessor Accessor;
	HAPI_AttributeInfo Info;
	{
		Accessor.Init(InputNodeId, PartId, HAPI_ATTRIB_POSITION);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 3, PCGPointData->GetNumPoints(), &Info);
		TArray<float> Points;
		Points.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			FHoudiniPCGUtils::UnrealToHoudini(PCGPointData->GetPoint(Index).Transform.GetLocation(), &Points[Index * 3]);
		}

		Accessor.SetAttributeData(Info, Points);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_ATTRIB_NORMAL);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 3, PCGPointData->GetNumPoints(), &Info);
		TArray<float> Normals;
		Normals.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			FHoudiniPCGUtils::UnrealToHoudini(PCGPointData->GetPoint(Index).Transform.GetRotation().GetAxisZ(), &Normals[Index * 3]);
		}

		Accessor.SetAttributeData(Info, Normals);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_ATTRIB_COLOR);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 4, PCGPointData->GetNumPoints(), &Info);
		TArray<float> Colors;
		Colors.SetNum(PCGPointData->GetNumPoints() * 4);
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			auto PCGPoint = PCGPointData->GetPoint(Index);
			Colors[Index * 4 + 0] = PCGPoint.Color.X;
			Colors[Index * 4 + 1] = PCGPoint.Color.Y;
			Colors[Index * 4 + 2] = PCGPoint.Color.Z;
			Colors[Index * 4 + 3] = PCGPoint.Color.W;
		}

		Accessor.SetAttributeData(Info, Colors);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_UNREAL_ATTRIB_PCG_STEEPNESS);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, PCGPointData->GetNumPoints(), &Info);
		TArray<float> Steepness;
		Steepness.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			Steepness[Index] = PCGPointData->GetPoint(Index).Steepness;
		}

		Accessor.SetAttributeData(Info, Steepness);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_UNREAL_ATTRIB_PCG_DENSITY);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, PCGPointData->GetNumPoints(), &Info);
		TArray<float> Density;
		Density.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			Density[Index] = PCGPointData->GetPoint(Index).Steepness;
		}

		Accessor.SetAttributeData(Info, Density);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_UNREAL_ATTRIB_PCG_SEED);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 1, PCGPointData->GetNumPoints(), &Info);
		TArray<float> Seed;
		Seed.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			Seed[Index] = PCGPointData->GetPoint(Index).Seed;
		}

		Accessor.SetAttributeData(Info, Seed);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_UNREAL_ATTRIB_PCG_BOUNDING_BOX_MIN);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 3, PCGPointData->GetNumPoints(), &Info);
		TArray<float> BoundingBoxMin;
		BoundingBoxMin.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			FHoudiniPCGUtils::UnrealToHoudini(PCGPointData->GetPoint(Index).BoundsMin, &BoundingBoxMin[Index * 3]);
		}

		Accessor.SetAttributeData(Info, BoundingBoxMin);
	}

	{
		Accessor.Init(InputNodeId, PartId, HAPI_UNREAL_ATTRIB_PCG_BOUNDING_BOX_MAX);
		Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_FLOAT, 3, PCGPointData->GetNumPoints(), &Info);
		TArray<float> BoundingBoxMax;
		BoundingBoxMax.SetNum(PCGPointData->GetNumPoints());
		for(int Index = 0; Index < PCGPointData->GetNumPoints(); Index++)
		{
			FHoudiniPCGUtils::UnrealToHoudini(PCGPointData->GetPoint(Index).BoundsMin, &BoundingBoxMax[Index * 3]);
		}

		Accessor.SetAttributeData(Info, BoundingBoxMax);
	}

	return true;
}
#endif
bool FUnrealPCGDataTranslator::CreateInputNodeForPCGParamData(
	UHoudiniPCGInputObject* PCGInputData,
	HAPI_NodeId& InputNodeId)
{
	if(PCGInputData->Attributes.IsEmpty())
		return true;

	int NumRows = PCGInputData->Attributes[0]->GetNumValues();

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = PCGInputData->Attributes.Num();
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = NumRows;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &Part), false);

	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_POINT;
	int PartId = Part.id;

	bool bFoundPositionAttr = false;
	for(auto& Attribute : PCGInputData->Attributes)
	{
		FHoudiniPCGInputAttributeDataBase* Attr = Attribute.Get();

		bFoundPositionAttr |= (Attr->Name == TEXT("P"));

		if(Attr->DataType == FHoudiniPCGInputAttributeData<float>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<float> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<double>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<double> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<int>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<int> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<int64>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<int64> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<FVector2d>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<FVector2d> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<FVector>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<FVector> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<FVector4d>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<FVector4d> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<FString>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<FString> *>(Attr), InputNodeId, PartId, Owner);
		else if(Attr->DataType == FHoudiniPCGInputAttributeData<FQuat>::StaticType)
			SendToHoudini(static_cast<FHoudiniPCGInputAttributeData<FQuat> *>(Attr), InputNodeId, PartId, Owner);
		else
		{
			check(false);
		}

	}

	if (!bFoundPositionAttr)
	{
		// We must have a point "P" Attribute. If one was not specified, add one.
		HAPI_AttributeInfo AttrInfo;
		FHoudiniHapiAccessor Accessor(InputNodeId, PartId, "P");
		TArray<float> Positions;
		Positions.SetNumZeroed(NumRows * 3);
		Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 3, NumRows, &AttrInfo);
		Accessor.SetAttributeData(AttrInfo, Positions);
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(InputNodeId), false);
	if(!FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true))
		return false;

	return true;
}



void SendToHoudini(FHoudiniPCGInputAttributeData<float>* Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner);

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<float> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}



void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<double> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT64, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<int> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_INT, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<int64> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_INT64, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<FString>* Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_STRING, 1, Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo, Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<FVector2d> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 2,Data->Values.Num(), &AttrInfo);

	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 2);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		FloatValues[Index * 2 + 0] =Data->Values[Index].X;
		FloatValues[Index * 2 + 1] =Data->Values[Index].Y;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}


void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<FVector> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 3,Data->Values.Num(), &AttrInfo);

	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 3);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		// Note, no intentional Unreal swizzling or scaling here, we don't know the type.
		FloatValues[Index * 3 + 0] =Data->Values[Index].X;
		FloatValues[Index * 3 + 1] =Data->Values[Index].Y;
		FloatValues[Index * 3 + 2] =Data->Values[Index].Z;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<FVector4d> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 4, Data->Values.Num(), &AttrInfo);
	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 4);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		// Note, no intentional Unreal swizzling or scaling here, we don't know the type.
		FloatValues[Index * 4 + 0] = Data->Values[Index].X;
		FloatValues[Index * 4 + 1] = Data->Values[Index].Y;
		FloatValues[Index * 4 + 2] = Data->Values[Index].Z;
		FloatValues[Index * 4 + 3] = Data->Values[Index].W;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}

void FUnrealPCGDataTranslator::SendToHoudini(FHoudiniPCGInputAttributeData<FQuat> * Data,HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 4,Data->Values.Num(), &AttrInfo);

	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 4);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		// Note, no intentional Unreal swizzling or scaling here, we don't know the type.
		FVector4d Vector = FHoudiniPCGUtils::UnrealToHoudiniQuat(Data->Values[Index]);
		FloatValues[Index * 4 + 0] = Vector.X;
		FloatValues[Index * 4 + 1] = Vector.Y;
		FloatValues[Index * 4 + 2] = Vector.Z;
		FloatValues[Index * 4 + 3] = Vector.W;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}

void SendToHoudini(FHoudiniPCGInputAttributeData<FString> * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, TCHAR_TO_UTF8((*Data->Name.ToString())));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_STRING, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}
