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
#include "HoudiniPCGDataObject.h"
#include <UnrealObjectInputManager.h>


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
	UHoudiniPCGDataCollection* PCGDataCollection,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	bool bInputNodesCanBeDeleted)
{

	// Create handles for each input node that will be merged together.

	TSet<FUnrealObjectInputHandle> Handles;
	switch (PCGDataCollection->Type)
	{
	case EHoudiniPCGDataType::InputPCGGeometry:
		{
			FUnrealObjectInputHandle Handle = CreateInputNodeForPCGAttrData(InputNodeName, PCGDataCollection, bInputNodesCanBeDeleted);
			Handles.Add(Handle);
		}
		break;

	case EHoudiniPCGDataType::InputPCGSplines:
		{
			TArray<FUnrealObjectInputHandle> SplineHandles = CreateInputNodeForPCGSplineData(InputNodeName, PCGDataCollection, bInputNodesCanBeDeleted);
			Handles.Append(SplineHandles);
		}
		break;

	default:
		break;
	}

	// Merge all nodes into the input.

	const FUnrealObjectInputIdentifier MergeNodeIdentifier(PCGDataCollection, {}, false);
	FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(MergeNodeIdentifier, Handles, OutHandle, true, bInputNodesCanBeDeleted);

	HAPI_NodeId MergeNodeId = FUnrealObjectInputUtils::GetHAPINodeId(MergeNodeIdentifier);

	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(MergeNodeId);
	FUnrealObjectInputUtils::AddNodeOrUpdateNode(MergeNodeIdentifier, MergeNodeId, OutHandle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted);

	return OutHandle.IsValid();
}

void
FUnrealPCGDataTranslator::SetAttributes(UHoudiniPCGDataObject* PCGDataObject, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	if(!PCGDataObject)
		return;

	for(auto& It : PCGDataObject->Attributes)
	{
		UHoudiniPCGDataAttributeBase* Attr = It.Get();

		if (Attr->GetAttrName() == TEXT("__vertex_id") && Owner == HAPI_ATTROWNER_VERTEX)
		{
			auto* AttributeInt = Cast<UHoudiniPCGDataAttributeInt>(Attr);
			if (!AttributeInt)
			{
				HOUDINI_LOG_ERROR(TEXT("__vertex_id must be an integer"));
				return;
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(AttributeInt->Values, InputNodeId, PartId), );
		}
		else if(auto* AttributeFloat = Cast<UHoudiniPCGDataAttributeFloat>(Attr))
			SendToHoudini(AttributeFloat, InputNodeId, PartId, Owner);
		else if(auto* AttributeDouble = Cast<UHoudiniPCGDataAttributeDouble>(Attr))
			SendToHoudini(AttributeDouble, InputNodeId, PartId, Owner);
		else if(auto* AttributeInt = Cast<UHoudiniPCGDataAttributeInt>(Attr))
			SendToHoudini(AttributeInt, InputNodeId, PartId, Owner);
		else if(auto* AttributeInt64 = Cast<UHoudiniPCGDataAttributeInt64>(Attr))
			SendToHoudini(AttributeInt64, InputNodeId, PartId, Owner);
		else if(auto* AttributeVector2d = Cast<UHoudiniPCGDataAttributeVector2d>(Attr))
			SendToHoudini(AttributeVector2d, InputNodeId, PartId, Owner);
		else if(auto* AttributeVector3d = Cast<UHoudiniPCGDataAttributeVector3d>(Attr))
			SendToHoudini(AttributeVector3d, InputNodeId, PartId, Owner);
		else if(auto* AttributeVector4d = Cast<UHoudiniPCGDataAttributeVector4d>(Attr))
			SendToHoudini(AttributeVector4d, InputNodeId, PartId, Owner);
		else if(auto* AttributeString = Cast<UHoudiniPCGDataAttributeString>(Attr))
			SendToHoudini(AttributeString, InputNodeId, PartId, Owner);
		else if(auto* AttributeSoftObjectPath = Cast<UHoudiniPCGDataAttributeSoftObjectPath>(Attr))
			SendToHoudini(AttributeSoftObjectPath, InputNodeId, PartId, Owner);
		else if(auto* AttributeSoftClassPath = Cast<UHoudiniPCGDataAttributeSoftClassPath>(Attr))
			SendToHoudini(AttributeSoftClassPath, InputNodeId, PartId, Owner);
		else
			check(false);
	}
}

FUnrealObjectInputHandle
FUnrealPCGDataTranslator::CreateInputNode(const FString & Name, UObject * Object, bool bInputNodesCanBeDeleted)
{
	// Create Identifier for this object and handle
	FUnrealObjectInputOptions Options;
	FUnrealObjectInputIdentifier Identifier = FUnrealObjectInputIdentifier(Object, Options, true);
	FUnrealObjectInputHandle Handle;

	if(FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
	{
		return Identifier;
	}


	// Make sure we have a parent node.
	FUnrealObjectInputHandle ParentHandle;

	FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted);
	HAPI_NodeId ParentNodeId = FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle);

	// Create the input node
	FString FinalInputNodeName = Name;
	FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
	HAPI_NodeId NewNodeId = FHoudiniEngineUtils::CreateInputHapiNode(FinalInputNodeName, ParentNodeId);
	if(!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return {};

	// Remove previous node and its parent.
	HAPI_NodeId PreviousInputNodeId = FUnrealObjectInputUtils::GetHAPINodeId(Handle);
	if(PreviousInputNodeId != INDEX_NONE)
	{
		HAPI_NodeId PreviousInputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputNodeId) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);
		}

		if(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputObjectNodeId) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input object node for %s."), *FinalInputNodeName);
		}
	}

	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);
	FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, NewNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted);

	return Handle;
}

TArray<FUnrealObjectInputHandle>
FUnrealPCGDataTranslator::CreateInputNodeForPCGSplineData(const FString& InputNodeName, UHoudiniPCGDataCollection* PCGDataCollection, bool bInputNodesCanBeDeleted)
{
	TArray<FUnrealObjectInputHandle> Results;

	for(auto& SplineObject : PCGDataCollection->Splines)
	{
		FUnrealObjectInputHandle Handle = CreateInputNodeForPCGSplineData(InputNodeName, SplineObject, bInputNodesCanBeDeleted);
		Results.Add(Handle);
	}
	return Results;
}

FUnrealObjectInputHandle
FUnrealPCGDataTranslator::CreateInputNodeForPCGSplineData(const FString& InputNodeName, UHoudiniPCGDataObject* PCGDataObject, bool bInputNodesCanBeDeleted)
{
	FString InputName = InputNodeName + PCGDataObject->GetName();
	FUnrealObjectInputHandle Handle = FUnrealPCGDataTranslator::CreateInputNode(
		InputName,
		PCGDataObject,
		bInputNodesCanBeDeleted);

	if(!Handle.IsValid())
		return Handle;

	HAPI_NodeId NodeId = FUnrealObjectInputUtils::GetHAPINodeId(Handle);

	UHoudiniPCGDataAttributeVector3d * PosAttr = Cast<UHoudiniPCGDataAttributeVector3d>(PCGDataObject->FindAttribute(TEXT("P")));
	if(!PosAttr)
		return {};

	int NumSegments = 1;
	int NumPositions = PosAttr->Values.Num();
	int CurveCounts = NumPositions;

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.type = HAPI_PARTTYPE_CURVE;
	Part.pointCount = NumPositions;
	Part.vertexCount = NumPositions;
	Part.faceCount = NumSegments;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part), {});

	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	CurveInfo.curveType = HAPI_CURVETYPE_LINEAR;
	CurveInfo.curveCount = NumSegments;
	CurveInfo.vertexCount = NumPositions;
	CurveInfo.knotCount = 0;
	CurveInfo.isPeriodic = false;
	CurveInfo.isRational = false;
	CurveInfo.order = 0;
	CurveInfo.hasKnots = false;
	CurveInfo.isClosed = PCGDataObject->bIsClosed;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveInfo(FHoudiniEngine::Get().GetSession(), NodeId, 0, &CurveInfo), {});
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveCounts(FHoudiniEngine::Get().GetSession(), NodeId, Part.id, &CurveCounts, 0, 1), {});

	SendToHoudini(PosAttr, NodeId, Part.id, HAPI_ATTROWNER_POINT);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), {});

	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	CookOptions.maxVerticesPerPrimitive = -1;
	CookOptions.refineCurveToLinear = false;
	static constexpr bool bWaitForCompletion = false;
	FHoudiniEngineUtils::HapiCookNode(NodeId, &CookOptions, bWaitForCompletion);

	return Handle;
}


FUnrealObjectInputHandle
FUnrealPCGDataTranslator::CreateInputNodeForPCGAttrData(const FString& InputNodeName,  UHoudiniPCGDataCollection* PCGCollection, bool bInputNodesCanBeDeleted)
{
	if(!IsValid(PCGCollection->Points))
	{
		HOUDINI_PCG_ERROR(TEXT("Not able to process a PCG Data without points"));
		return {};
	}

	FUnrealObjectInputHandle Handle = FUnrealPCGDataTranslator::CreateInputNode(
		InputNodeName,
		PCGCollection,
		bInputNodesCanBeDeleted);

	if(!Handle.IsValid())
		return Handle;

	int NumPoints = PCGCollection->Points ? PCGCollection->Points->GetNumRows() : 0;
	int NumPrims = PCGCollection->Primitives ? PCGCollection->Primitives->GetNumRows() : 0;
	int NumVertices = PCGCollection->Vertices ? PCGCollection->Vertices->GetNumRows() : 0;
	int NumDetails = PCGCollection->Details ? PCGCollection->Details->GetNumRows() : 0;

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = PCGCollection->Points->Attributes.Num();
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = PCGCollection->Primitives ? PCGCollection->Primitives->Attributes.Num() : 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = PCGCollection->Vertices ? PCGCollection->Vertices->Attributes.Num() : 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = PCGCollection->Details ? PCGCollection->Details->Attributes.Num() : 0;
	Part.vertexCount = NumVertices;
	Part.faceCount = NumPrims;
	Part.pointCount = NumPoints;
	Part.type = HAPI_PARTTYPE_MESH;

	HAPI_NodeId NodeId = FUnrealObjectInputUtils::GetHAPINodeId(Handle);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part), {});

	SetAttributes(PCGCollection->Points, NodeId, Part.id, HAPI_ATTROWNER_POINT);
	SetAttributes(PCGCollection->Vertices, NodeId, Part.id, HAPI_ATTROWNER_VERTEX);
	SetAttributes(PCGCollection->Primitives, NodeId, Part.id, HAPI_ATTROWNER_PRIM);
	SetAttributes(PCGCollection->Details, NodeId, Part.id, HAPI_ATTROWNER_DETAIL);

	// We need to generate array of face counts.
	if (Part.faceCount)
	{
		TArray<int32> StaticMeshFaceCounts;
		StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);
		for(int32 n = 0; n < Part.faceCount; n++)
			StaticMeshFaceCounts[n] = 3;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
			StaticMeshFaceCounts, NodeId, 0), {});
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), {});
	if(!FHoudiniEngineUtils::HapiCookNode(NodeId, nullptr, true))
		return {};

	return Handle;
}

FString GetHapiName(FName AttrName)
{
	FString HapiName = AttrName.ToString();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(HapiName);
	return HapiName;
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeFloat * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeDouble * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT64, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeInt * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_INT, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeInt64 * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_INT64, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeString* Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_STRING, 1, Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo, Data->Values);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeSoftObjectPath * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_STRING, 1, Data->Values.Num(), &AttrInfo);

	TArray<FString> StringValues;
	StringValues.Reserve(Data->Values.Num());
	for(auto& Path : Data->Values)
		StringValues.Add(Path.ToString());

	Accessor.SetAttributeData(AttrInfo, StringValues);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeSoftClassPath* Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_STRING, 1, Data->Values.Num(), &AttrInfo);

	TArray<FString> StringValues;
	StringValues.Reserve(Data->Values.Num());
	for(auto& Path : Data->Values)
		StringValues.Add(Path.ToString());


	Accessor.SetAttributeData(AttrInfo, StringValues);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeVector2d * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 2, Data->Values.Num(), &AttrInfo);

	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 2);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		FloatValues[Index * 2 + 0] = Data->Values[Index].X;
		FloatValues[Index * 2 + 1] = Data->Values[Index].Y;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}


void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeVector3d * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 3, Data->Values.Num(), &AttrInfo);

	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 3);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		FloatValues[Index * 3 + 0] = Data->Values[Index].X;
		FloatValues[Index * 3 + 1] = Data->Values[Index].Y;
		FloatValues[Index * 3 + 2] = Data->Values[Index].Z;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeVector4d * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_FLOAT, 4, Data->Values.Num(), &AttrInfo);
	TArray<float> FloatValues;
	FloatValues.SetNum(Data->Values.Num() * 4);
	for(int Index = 0; Index <Data->Values.Num(); Index++)
	{
		FloatValues[Index * 4 + 0] = Data->Values[Index].X;
		FloatValues[Index * 4 + 1] = Data->Values[Index].Y;
		FloatValues[Index * 4 + 2] = Data->Values[Index].Z;
		FloatValues[Index * 4 + 3] = Data->Values[Index].W;
	}
	Accessor.SetAttributeData(AttrInfo, FloatValues);
}

void FUnrealPCGDataTranslator::SendToHoudini(UHoudiniPCGDataAttributeQuat * Data,HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
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

void SendToHoudini(UHoudiniPCGDataAttributeString * Data, HAPI_NodeId InputNodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniHapiAccessor Accessor(InputNodeId, PartId, H_TCHAR_TO_UTF8((*GetHapiName(Data->GetAttrName()))));
	Accessor.AddAttribute(Owner, HAPI_StorageType::HAPI_STORAGETYPE_STRING, 1,Data->Values.Num(), &AttrInfo);
	Accessor.SetAttributeData(AttrInfo,Data->Values);
}
