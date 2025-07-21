/*
* Copyright (c) <2025> Side Effects Software Inc.
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

#include "HoudiniPCGTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniPCGUtils.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "Metadata/PCGMetadata.h"
#include "PCGParamData.h"

bool HasAttribute(HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, const FString & AttrName)
{
	TArray<FString> Attributes = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId, Owner);
	for (FString Attr : Attributes)
	{
		if(Attr == AttrName)
			return true;
	}
	return false;
}


bool FHoudiniPCGTranslator::IsPCGOutput(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	if(HasAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, HOUDINI_PCG_PARAMS_OUTPUT_NAME))
		return true;
	if(HasAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX, HOUDINI_PCG_PARAMS_OUTPUT_NAME))
		return true;
	if(HasAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM, HOUDINI_PCG_PARAMS_OUTPUT_NAME))
		return true;
	if(HasAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, HOUDINI_PCG_PARAMS_OUTPUT_NAME))
		return true;
	return false;
}

UHoudiniPCGOutputData* FHoudiniPCGTranslator::CreatePCGSplinesOutput(UHoudiniOutput* CurOutput)
{
	const auto& HGPO = CurOutput->GetHoudiniGeoPartObjects()[0];

	UHoudiniPCGOutputData* Results = NewObject<UHoudiniPCGOutputData>(CurOutput);

	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveInfo(FHoudiniEngine::Get().GetSession(), HGPO.GeoId, 
		HGPO.PartId, &CurveInfo), nullptr);

	TArray<int> CurveCounts;
	CurveCounts.SetNum(CurveInfo.curveCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveCounts(FHoudiniEngine::Get().GetSession(), 
		HGPO.AssetId, HGPO.PartId, CurveCounts.GetData(), 0, CurveCounts.Num()), nullptr);

	TArray<float> FloatPositions;
	FHoudiniHapiAccessor PositionAccessor(HGPO.AssetId, HGPO.PartId, HAPI_ATTRIB_POSITION);
	PositionAccessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, FloatPositions);

	TArray<FString> Attributes = FHoudiniEngineUtils::GetAttributeNames(
		FHoudiniEngine::Get().GetSession(),
		HGPO.GeoId, HGPO.PartId,
		HAPI_ATTROWNER_POINT);

	TArray<FString> MetaAttributes;
	for(FString Attr : Attributes)
	{
		bool bIgnore = false;
		bIgnore |= Attr == TEXT("P");
		bIgnore |= Attr == TEXT("__vertex_id");

		if(!bIgnore)
		{
			MetaAttributes.Add(Attr);
		}
	}


	int CurveStart = 0;

	for (int CurveIndex = 0; CurveIndex < CurveCounts.Num(); CurveIndex++)
	{
		TArray<FSplinePoint> SplinePoints;
		SplinePoints.SetNum(CurveCounts[CurveIndex]);

		UPCGSplineData* ParamData = NewObject<UPCGSplineData>();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		TArray<int64> EntryKeys;
		EntryKeys.SetNum(SplinePoints.Num());
#endif
		for (int PosIndex = 0; PosIndex < SplinePoints.Num(); PosIndex++)
		{
			int HapiOffset = (CurveStart + PosIndex) * 3;
			FVector Position;
			Position.X = FloatPositions[HapiOffset + 0] * 100.0;
			Position.Y = FloatPositions[HapiOffset + 2] * 100.0;
			Position.Z = FloatPositions[HapiOffset + 1] * 100.0;
			SplinePoints[PosIndex].Position = Position;
			SplinePoints[PosIndex].InputKey = static_cast<float>(PosIndex);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
			EntryKeys[PosIndex] = PosIndex;
#endif
		}

		// Set attributes. Only works on Unreal 5.6.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		UPCGMetadata* MetaData = ParamData->Metadata;
		CreatePCGMetaAttributes(MetaData, MetaAttributes, EntryKeys, HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_POINT, CurveStart, SplinePoints.Num());

		ParamData->Initialize(SplinePoints, CurveInfo.isClosed, FTransform::Identity, EntryKeys);
#else
		ParamData->Initialize(SplinePoints, CurveInfo.isClosed, FTransform::Identity);
#endif
		CurveStart += CurveCounts[CurveIndex];

		Results->SplineParams.Add(ParamData);
	}

	return Results;
}

UHoudiniPCGOutputData* FHoudiniPCGTranslator::CreatePCGParamsOutput(UHoudiniOutput* CurOutput)
{
	const auto& HGPO = CurOutput->GetHoudiniGeoPartObjects()[0];

	UHoudiniPCGOutputData* Results = NewObject<UHoudiniPCGOutputData>(CurOutput);

	Results->DetailsParams = CreatePCGAttributes(HGPO.GeoId, HGPO.PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL);
	Results->PrimsParams = CreatePCGAttributes(HGPO.GeoId, HGPO.PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM);
	Results->VertexParams = CreatePCGAttributes(HGPO.GeoId, HGPO.PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX);
	Results->PointParams = CreatePCGPointData(HGPO.GeoId, HGPO.PartId);
	return Results;
}

void FHoudiniPCGTranslator::CreatePCGFromOutput(UHoudiniOutput* Output)
{
	if(Output->GetHoudiniGeoPartObjects().IsEmpty())
		return;

	const auto& HGPO = Output->GetHoudiniGeoPartObjects()[0];

	UHoudiniPCGOutputData* PCGOutput = nullptr;

	switch (HGPO.PartInfo.Type)
	{
	case EHoudiniPartType::Curve:
		PCGOutput = CreatePCGSplinesOutput(Output);
		break;

	default:
		PCGOutput = CreatePCGParamsOutput(Output);
		break;
	}

	if (PCGOutput)
	{
		FHoudiniOutputObjectIdentifier OutputIdentifier;
		OutputIdentifier.ObjectId = HGPO.ObjectId;
		OutputIdentifier.GeoId = HGPO.GeoId;
		OutputIdentifier.PartId = HGPO.PartId;
		OutputIdentifier.PartName = HGPO.PartName;
		FHoudiniOutputObject& NewOutputObject = Output->GetOutputObjects().FindOrAdd(OutputIdentifier);
		NewOutputObject.OutputObject = PCGOutput;
	}
}

UPCGPointData* FHoudiniPCGTranslator::CreatePCGPointData(HAPI_NodeId NodeId, HAPI_PartId PartId )
{
	UPCGPointData* PointData = NewObject<UPCGPointData>();
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo);

	TArray<FString> Attributes = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId, HAPI_ATTROWNER_POINT);

	TArray<FString> MetaAttributes;

	TArray<FPCGPoint> Points;
	Points.SetNum(PartInfo.pointCount);

	//
	// Process all Houdini attributes that can be directly converted to point data.
	//

	for(int AttrIndex = 0; AttrIndex < Attributes.Num(); AttrIndex++)
	{
		FString& Attribute = Attributes[AttrIndex];
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);
		FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, H_TCHAR_TO_UTF8(*Attribute), HAPI_ATTROWNER_POINT, &AttrInfo);
		FHoudiniHapiAccessor Accessor(NodeId, PartId, H_TCHAR_TO_UTF8(*Attribute));

		if (Attribute.Equals(TEXT("P"), ESearchCase::Type::IgnoreCase))
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				FVector Position = FHoudiniPCGUtils::HoudiniToUnrealPosition(&Values[Index * 3]);
				Points[Index].Transform.SetLocation(Position);
				
			}
		}
		else if(Attribute.Equals(TEXT("orient"), ESearchCase::Type::IgnoreCase))
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				FQuat Quat = FHoudiniPCGUtils::HoudiniToUnrealQuat(&Values[Index * 4]);
				Points[Index].Transform.SetRotation(Quat);

			}
		}
		else if(Attribute.Equals(TEXT("scale"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 3)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				FVector3d Scale = FHoudiniPCGUtils::HoudiniToUnrealVector(&Values[Index * 3]);
				Points[Index].Transform.SetScale3D(Scale);
			}
		}
		else if(Attribute.Equals(TEXT("BoundsMin"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 3)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				FVector3d BoundsMin = FHoudiniPCGUtils::HoudiniToUnrealVector(&Values[Index * 3]);
				Points[Index].BoundsMin = BoundsMin;
			}
		}
		else if(Attribute.Equals(TEXT("BoundsMax"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 3)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				FVector3d BoundsMax = FHoudiniPCGUtils::HoudiniToUnrealVector(&Values[Index * 3]);
				Points[Index].BoundsMax = BoundsMax;
			}
		}
		else if(Attribute.Equals(TEXT("Cd"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 3)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				Points[Index].Color = FVector4(Values[Index * 3 + 0], Values[Index * 3 + 1], Values[Index * 3 + 2], 1.0f);
			}
		}
		else if(Attribute.Equals(TEXT("Cd"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 4)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				Points[Index].Color = FVector4(Values[Index * 4 + 0], Values[Index * 4 + 1], Values[Index * 4 + 2], Values[Index * 4 + 3]);
			}
		}
		else if(Attribute.Equals(TEXT("Steepness"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 1)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				Points[Index].Steepness = Values[Index];
			}
		}
		else if(Attribute.Equals(TEXT("Seed"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 1)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				Points[Index].Seed = Values[Index];
			}
		}
		else if(Attribute.Equals(TEXT("Density"), ESearchCase::Type::IgnoreCase) && AttrInfo.tupleSize == 1)
		{
			TArray<float> Values;
			Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
			for(int Index = 0; Index < Points.Num(); Index++)
			{
				Points[Index].Density = Values[Index];
			}
		}
		else
		{
			MetaAttributes.Add(Attribute);
		}
	}

	// Any unprocessed attributes can now be added as meta data.

	UPCGMetadata* MetaData = PointData->MutableMetadata();

	TArray<int64> EntryKeys;
	EntryKeys.SetNum(Points.Num());

	for(int Index = 0; Index < EntryKeys.Num(); Index++)
	{
		EntryKeys[Index] = MetaData->AddEntry();
		Points[Index].MetadataEntry = EntryKeys[Index];
	}

	CreatePCGMetaAttributes(MetaData, MetaAttributes, EntryKeys, NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT);

	PointData->SetPoints(Points);

	return PointData;

}

UPCGParamData* FHoudiniPCGTranslator::CreatePCGAttributes(HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	TArray<FString> Attributes = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId, Owner);

	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* MetaData = ParamData->MutableMetadata();

	TArray<int64> EntryKeys;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo);

	switch(Owner)
	{
	case HAPI_ATTROWNER_POINT:
		EntryKeys.SetNum(PartInfo.pointCount);
		break;
	case HAPI_ATTROWNER_VERTEX:
		EntryKeys.SetNum(PartInfo.vertexCount);
		break;
	case HAPI_ATTROWNER_PRIM:
		EntryKeys.SetNum(PartInfo.faceCount);
		break;
	case HAPI_ATTROWNER_DETAIL:
		EntryKeys.SetNum(1);
		break;
	default:
		break;
	}

	for(int Index = 0; Index < EntryKeys.Num(); Index++)
		EntryKeys[Index] = MetaData->AddEntry();

	CreatePCGMetaAttributes(MetaData, Attributes, EntryKeys, NodeId, PartId, Owner);
	return ParamData;
}

void FHoudiniPCGTranslator::CreatePCGMetaAttributes(
	UPCGMetadata* MetaData, 
	TArray<FString> & Attributes, 
	const TArray<int64>& EntryKeys, 
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId, 
	HAPI_AttributeOwner Owner,
	int StartIndex,
	int IndexCount)
{

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo);

	for (int AttrIndex = 0; AttrIndex < Attributes.Num(); AttrIndex++)
	{
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);
		FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, H_TCHAR_TO_UTF8(*Attributes[AttrIndex]), Owner, &AttrInfo);

		if (Attributes[AttrIndex] == TEXT("__vertex_id") && PartInfo.vertexCount > 0)
		{
			TArray<int> VertexIds;
			VertexIds.SetNumZeroed(PartInfo.vertexCount);
			FHoudiniApi::GetVertexList(FHoudiniEngine::Get().GetSession(), NodeId, PartId, VertexIds.GetData(), 0, VertexIds.Num());

			MetaData->CreateInteger32Attribute(*Attributes[AttrIndex], 0, false, false);
			FPCGMetadataAttribute<int32>* MetaAttr = MetaData->GetMutableTypedAttribute<int32>(*Attributes[AttrIndex]);
			check(VertexIds.Num() == MetaData->GetItemCountForChild());
			MetaAttr->SetValues(EntryKeys, VertexIds);
			continue;
		}

		switch (AttrInfo.storage)
		{
		case HAPI_STORAGETYPE_UINT8:
		case HAPI_STORAGETYPE_INT8:
		case HAPI_STORAGETYPE_INT16:
		case HAPI_STORAGETYPE_INT:
			CreatePCGInt32Attribute(MetaData, EntryKeys, NodeId, PartId, Owner, FName(Attributes[AttrIndex]), StartIndex, IndexCount);
			break;
		case HAPI_STORAGETYPE_INT64:
			CreatePCGInt64Attribute(MetaData, EntryKeys, NodeId, PartId, Owner, FName(Attributes[AttrIndex]), StartIndex, IndexCount);
			break;
		case HAPI_STORAGETYPE_FLOAT:
			CreatePCGFloatAttribute(MetaData, EntryKeys, NodeId, PartId, Owner, FName(Attributes[AttrIndex]), StartIndex, IndexCount);
			break;
		case HAPI_STORAGETYPE_FLOAT64:
			CreatePCGDoubleAttribute(MetaData, EntryKeys, NodeId, PartId, Owner, FName(Attributes[AttrIndex]), StartIndex, IndexCount);
			break;
		case HAPI_STORAGETYPE_STRING:
			CreatePCGStringAttribute(MetaData, EntryKeys, NodeId, PartId, Owner, FName(Attributes[AttrIndex]), StartIndex, IndexCount);
			break;
		default:
			break;
		}
	}
}

template<typename Type>
TArray<Type> HoudiniPCGGetSelectedTuple(const TArray<Type> & Values, int TupleIndex, int TupleSize)
{
	int ActualCount = Values.Num() / TupleSize;
	ensure(Values.Num() == (ActualCount * TupleSize));

	TArray<Type> Results;
	Results.SetNum(ActualCount);

	for (int Index = 0; Index < ActualCount; Index++)
	{
		Results[Index] = Values[Index * TupleSize + TupleIndex];
	}
	return Results;
}


void
FHoudiniPCGTranslator::CreatePCGInt32Attribute(UPCGMetadata* Metadata, 
	const TArray<int64>& EntryKeys, 
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId, 
	HAPI_AttributeOwner Owner, 
	FName AttrName,
	int StartIndex,
	int Count)
{
	TArray<int> Values;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, H_TCHAR_TO_UTF8(*AttrName.ToString()));
	Accessor.GetAttributeData(Owner, Values, StartIndex, Count);
	if(Values.IsEmpty())
		return;

	HAPI_AttributeInfo AttributeInfo;
	Accessor.GetInfo(AttributeInfo, Owner);
	Values = HoudiniPCGGetSelectedTuple(Values, 0, AttributeInfo.tupleSize);

	Metadata->CreateInteger32Attribute(AttrName, 0, false, false);
	FPCGMetadataAttribute<int32>* MetaAttr = Metadata->GetMutableTypedAttribute<int32>(AttrName);

	MetaAttr->SetValues(EntryKeys, Values);
}

void FHoudiniPCGTranslator::CreatePCGInt64Attribute(UPCGMetadata* Metadata, 
	const TArray<int64>& EntryKeys, 
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId, 
	HAPI_AttributeOwner Owner, 
	FName AttrName,
	int StartIndex,
	int IndexCount)
{
	TArray<int64> Values;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, H_TCHAR_TO_UTF8(*AttrName.ToString()));
	Accessor.GetAttributeData(Owner, Values, StartIndex, IndexCount);
	if(Values.IsEmpty())
		return;

	HAPI_AttributeInfo AttributeInfo;
	Accessor.GetInfo(AttributeInfo, Owner);
	Values = HoudiniPCGGetSelectedTuple(Values, 0, AttributeInfo.tupleSize);

	Metadata->CreateInteger64Attribute(AttrName, 0, false, false);
	FPCGMetadataAttribute<int64>* MetaAttr = Metadata->GetMutableTypedAttribute<int64>(AttrName);

	MetaAttr->SetValues(EntryKeys, Values);
}

void FHoudiniPCGTranslator::CreatePCGFloatAttribute(UPCGMetadata* Metadata, 
	const TArray<int64>& EntryKeys, 
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId, 
	HAPI_AttributeOwner Owner, 
	FName AttrName,
	int StartIndex,
	int IndexCount)
{
	TArray<float> Values;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, H_TCHAR_TO_UTF8(*AttrName.ToString()));
	Accessor.GetAttributeData(Owner, Values, StartIndex, IndexCount);
	if(Values.IsEmpty())
		return;

	int TupleSize = Values.Num() / EntryKeys.Num();

	switch (TupleSize)
	{
	case 1:
		{
			Metadata->CreateFloatAttribute(AttrName, 0, false, false);
			FPCGMetadataAttribute<float>* MetaAttr = Metadata->GetMutableTypedAttribute<float>(AttrName);
			MetaAttr->SetValues(EntryKeys, Values);
		}
		break;
	case 2:
	{
		TArray<FVector2d> ConvertedValues;
		ConvertedValues.SetNum(Values.Num() / TupleSize);
		for (int Index = 0; Index < ConvertedValues.Num(); Index++)
		{
			ConvertedValues[Index][0] = Values[Index * 3 + 0];
			ConvertedValues[Index][1] = Values[Index * 3 + 1];
		}

		Metadata->CreateVector2Attribute(AttrName, FVector2d::ZeroVector, false, false);
		FPCGMetadataAttribute<FVector2d>* MetaAttr = Metadata->GetMutableTypedAttribute<FVector2d>(AttrName);
		MetaAttr->SetValues(EntryKeys, ConvertedValues);
		break;
	}
	case 3:
	{
		TArray<FVector> ConvertedValues;
		ConvertedValues.SetNum(Values.Num() / TupleSize);
		for(int Index = 0; Index < ConvertedValues.Num(); Index++)
		{
			ConvertedValues[Index][0] = Values[Index * 3 + 0];
			ConvertedValues[Index][1] = Values[Index * 3 + 1];
			ConvertedValues[Index][2] = Values[Index * 3 + 2];
		}

		Metadata->CreateVectorAttribute(AttrName, FVector::ZeroVector, false, false);
		FPCGMetadataAttribute<FVector>* MetaAttr = Metadata->GetMutableTypedAttribute<FVector>(AttrName);
		MetaAttr->SetValues(EntryKeys, ConvertedValues);
		break;
	}
	case 4:
	{
		TArray<FVector4d> ConvertedValues;
		ConvertedValues.SetNum(Values.Num() / TupleSize);
		for(int Index = 0; Index < ConvertedValues.Num(); Index++)
		{
			ConvertedValues[Index][0] = Values[Index * 3 + 0];
			ConvertedValues[Index][1] = Values[Index * 3 + 1];
			ConvertedValues[Index][2] = Values[Index * 3 + 2];
		}

		Metadata->CreateVector4Attribute(AttrName, FVector4d::Zero(), false, false);
		FPCGMetadataAttribute<FVector4d>* MetaAttr = Metadata->GetMutableTypedAttribute<FVector4d>(AttrName);
		MetaAttr->SetValues(EntryKeys, ConvertedValues);
		break;
	}
	default:
		break;
	}
}

void FHoudiniPCGTranslator::CreatePCGDoubleAttribute(UPCGMetadata* Metadata, 
	const TArray<int64>& EntryKeys, 
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId, 
	HAPI_AttributeOwner Owner, 
	FName AttrName,
	int StartIndex,
	int IndexCount)
{
	TArray<double> Values;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, H_TCHAR_TO_UTF8(*AttrName.ToString()));
	Accessor.GetAttributeData(Owner, Values, StartIndex, IndexCount);
	if(Values.IsEmpty())
		return;

	HAPI_AttributeInfo AttributeInfo;
	Accessor.GetInfo(AttributeInfo, Owner);
	Values = HoudiniPCGGetSelectedTuple(Values, 0, AttributeInfo.tupleSize);

	Metadata->CreateDoubleAttribute(AttrName, 0, false, false);
	FPCGMetadataAttribute<double>* MetaAttr = Metadata->GetMutableTypedAttribute<double>(AttrName);

	MetaAttr->SetValues(EntryKeys, Values);
}

void FHoudiniPCGTranslator::CreatePCGStringAttribute(
	UPCGMetadata* Metadata, 
	const TArray<int64>& EntryKeys, 
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId, 
	HAPI_AttributeOwner Owner, 
	FName AttrName, 
	int StartIndex,
	int IndexCount)
{
	TArray<FString> Values;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, H_TCHAR_TO_UTF8(*AttrName.ToString()));
	Accessor.GetAttributeData(Owner, Values, StartIndex, IndexCount);
	if(Values.IsEmpty())
		return;

	HAPI_AttributeInfo AttributeInfo;
	Accessor.GetInfo(AttributeInfo, Owner);
	Values = HoudiniPCGGetSelectedTuple(Values, 0, AttributeInfo.tupleSize);

	Metadata->CreateStringAttribute(AttrName, FString(), false, false);
	FPCGMetadataAttribute<FString>* MetaAttr = Metadata->GetMutableTypedAttribute<FString>(AttrName);

	MetaAttr->SetValues(EntryKeys, Values);
}

