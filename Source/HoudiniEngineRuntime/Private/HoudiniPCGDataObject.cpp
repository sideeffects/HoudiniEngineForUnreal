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

#include "HoudiniPCGDataObject.h"

#include <Data/PCGPointData.h>
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#include <Data/PCGPointArrayData.h>
#endif
#include "HoudiniEngineRuntimeUtils.h"
#include "UObject/TextProperty.h"
#include "PCGParamData.h"

void UHoudiniPCGDataAttributeBase::SetAttrName(const FString& Name)
{
	AttrName = FName(Name);
}


const FName& UHoudiniPCGDataAttributeBase::GetAttrName() const
{
	return AttrName;
}

bool UHoudiniPCGDataObject::operator==(const UHoudiniPCGDataObject& Other) const
{
	// very simple, optimize?
	return (this->Attributes == Other.Attributes);
}

bool UHoudiniPCGDataObject::operator!=(const UHoudiniPCGDataObject& Other) const
{
	return !(*this == Other);
}

void UHoudiniPCGDataObject::SetFromPCGData(const UPCGData* PCGData, const TSet<FString> & Tags)
{
	PCGDataType = PCGData->GetDataType();
	PCGTags = Tags;
	if(PCGData->IsA<UPCGParamData>())
		SetFromPCGData(Cast<UPCGParamData>(PCGData));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	else if(PCGData->IsA<UPCGBasePointData>())
		SetFromPCGBasePointData(Cast<UPCGBasePointData>(PCGData));
#else
	else if(PCGData->IsA<UPCGPointData>())
		SetFromPCGData(Cast<UPCGPointData>(PCGData));
#endif
	else if(PCGData->IsA<UPCGSplineData>())
		SetFromPCGData(Cast<UPCGSplineData>(PCGData));

}

void UHoudiniPCGDataObject::SetFromPCGData(const UPCGSplineData* PCGSplineData)
{
	const UPCGMetadata* Metadata = PCGSplineData->ConstMetadata();
	this->PCGDataType = PCGSplineData->GetDataType();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 5
	auto & SplineCurves = PCGSplineData->SplineStruct.SplineCurves;
	auto& Points = SplineCurves.Position.Points;
#else
	auto& Points = PCGSplineData->SplineStruct.GetSplinePointsPosition().Points;
#endif


	auto AttrDest = CreateAttributeVector3d(TEXT("P"));
	AttrDest->Values.SetNum(Points.Num());

	for(int Index = 0; Index < Points.Num(); Index++)
	{
		FVector Position = PCGSplineData->GetTransform().TransformPosition(Points[Index].OutVal);

		AttrDest->Values[Index][0] = Position.X / 100.0f;
		AttrDest->Values[Index][1] = Position.Z / 100.0f;
		AttrDest->Values[Index][2] = Position.Y / 100.0f;
	}
	Attributes.Emplace(MoveTemp(AttrDest));

	bIsClosed = PCGSplineData->SplineStruct.IsClosedLoop();

}

void UHoudiniPCGDataObject::SetFromPCGData(const UPCGPointData* PCGPointData)
{
	const UPCGMetadata* Metadata = PCGPointData->ConstMetadata();
	this->PCGDataType = PCGPointData->GetDataType();

	const TArray<FPCGPoint>& Points = PCGPointData->GetPoints();

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("P"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Position = Points[Index].Transform.GetLocation();
			AttrDest->Values[Index][0] = Position.X / 100.0f;
			AttrDest->Values[Index][1] = Position.Z / 100.0f;
			AttrDest->Values[Index][2] = Position.Y / 100.0f;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("Scale"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Scale = Points[Index].Transform.GetScale3D();
			AttrDest->Values[Index][0] = Scale.X;
			AttrDest->Values[Index][1] = Scale.Z;
			AttrDest->Values[Index][2] = Scale.Y;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("BoundsMin"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Value = Points[Index].BoundsMin;
			AttrDest->Values[Index][0] = Value.X / 100.0;
			AttrDest->Values[Index][1] = Value.Z / 100.0;
			AttrDest->Values[Index][2] = Value.Y / 100.0;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("BoundsMax"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Value = Points[Index].BoundsMin;
			AttrDest->Values[Index][0] = Value.X / 100.0;
			AttrDest->Values[Index][1] = Value.Z / 100.0;
			AttrDest->Values[Index][2] = Value.Y / 100.0;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector4d(TEXT("Cd"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector4d Value = Points[Index].Color;
			AttrDest->Values[Index][0] = Value.X;
			AttrDest->Values[Index][1] = Value.Y;
			AttrDest->Values[Index][2] = Value.Z;
			AttrDest->Values[Index][3] = Value.W;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector4d(TEXT("orient"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FQuat Rotation = Points[Index].Transform.GetRotation();
			AttrDest->Values[Index][0] = Rotation.X;
			AttrDest->Values[Index][1] = Rotation.Z;
			AttrDest->Values[Index][2] = Rotation.Y;
			AttrDest->Values[Index][3] = -Rotation.W;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeFloat(TEXT("Density"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			AttrDest->Values[Index] = Points[Index].Density;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeFloat(TEXT("Steepness"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			AttrDest->Values[Index] = Points[Index].Steepness;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeInt(TEXT("Seed"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			AttrDest->Values[Index] = Points[Index].Seed;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}


	TArray<int64> Keys;
	Keys.SetNum(Points.Num());
	for(int Index = 0; Index < Keys.Num(); Index++)
	{
		Keys[Index] = Points[Index].MetadataEntry;
	}

	AddMetaDataAttributes(Metadata, Keys);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
void UHoudiniPCGDataObject::SetFromPCGBasePointData(const UPCGBasePointData* PCGPointData)
{
	const UPCGMetadata* Metadata = PCGPointData->ConstMetadata();
	this->PCGDataType = PCGPointData->GetDataType();

	int NumPoints = PCGPointData->GetNumPoints();

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("P"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstTransformValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			FVector Position = ValueRange[Index].GetLocation();
			AttrDest->Values[Index][0] = Position.X / 100.0f;
			AttrDest->Values[Index][1] = Position.Z / 100.0f;
			AttrDest->Values[Index][2] = Position.Y / 100.0f;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("Scale"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstTransformValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			FVector Scale = ValueRange[Index].GetScale3D();
			AttrDest->Values[Index][0] = Scale.X;
			AttrDest->Values[Index][1] = Scale.Z;
			AttrDest->Values[Index][2] = Scale.Y;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("BoundsMin"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstBoundsMinValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			FVector Value = ValueRange[Index];
			AttrDest->Values[Index][0] = Value.X / 100.0;
			AttrDest->Values[Index][1] = Value.Z / 100.0;
			AttrDest->Values[Index][2] = Value.Y / 100.0;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector3d(TEXT("BoundsMax"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstBoundsMaxValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			FVector Value = ValueRange[Index];
			AttrDest->Values[Index][0] = Value.X / 100.0;
			AttrDest->Values[Index][1] = Value.Z / 100.0;
			AttrDest->Values[Index][2] = Value.Y / 100.0;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector4d(TEXT("Cd"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstColorValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			FVector4d Value = ValueRange[Index];
			AttrDest->Values[Index][0] = Value.X;
			AttrDest->Values[Index][1] = Value.Y;
			AttrDest->Values[Index][2] = Value.Z;
			AttrDest->Values[Index][3] = Value.W;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeVector4d(TEXT("orient"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstTransformValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			FQuat Rotation = ValueRange[Index].GetRotation();
			AttrDest->Values[Index][0] = Rotation.X;
			AttrDest->Values[Index][1] = Rotation.Z;
			AttrDest->Values[Index][2] = Rotation.Y;
			AttrDest->Values[Index][3] = -Rotation.W;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeFloat(TEXT("Density"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstDensityValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			AttrDest->Values[Index] = ValueRange[Index];
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeFloat(TEXT("Steepness"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstSteepnessValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			AttrDest->Values[Index] = ValueRange[Index];
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = CreateAttributeInt(TEXT("Seed"));
		AttrDest->Values.SetNum(NumPoints);
		auto ValueRange = PCGPointData->GetConstSeedValueRange();

		for(int Index = 0; Index < NumPoints; Index++)
		{
			AttrDest->Values[Index] = ValueRange[Index];
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	auto ValueRange = PCGPointData->GetConstMetadataEntryValueRange();

	TArray<int64> Keys;
	Keys.SetNum(NumPoints);
	for (int Index = 0; Index < NumPoints; Index++)
	{
		Keys[Index] = ValueRange[Index];
	}

	AddMetaDataAttributes(Metadata, Keys);
}
#endif

void UHoudiniPCGDataObject::SetFromPCGData(const UPCGParamData* PCGParamData)
{
	const UPCGMetadata* Metadata = PCGParamData->ConstMetadata();

	AddMetaDataAttributes(Metadata, {});
}

void UHoudiniPCGDataObject::AddMetaDataAttributes(const UPCGMetadata* ParamMetadata, const TArray<int64>& Keys)
{
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;

	ParamMetadata->GetAttributes(AttributeNames, AttributeTypes);

	const int64 InvalidIndex = -1;

	
	for(int AttrIndex = 0; AttrIndex < AttributeTypes.Num(); AttrIndex++)
	{
		EPCGMetadataTypes AttrType = AttributeTypes[AttrIndex];
		const FString & AttrName = AttributeNames[AttrIndex].ToString();

		const FPCGMetadataAttributeBase* AttrBase = ParamMetadata->GetConstAttribute(AttributeNames[AttrIndex]);

		const UPCGMetadata* Metadata = AttrBase->GetMetadata();

		int NumRows = Metadata->GetItemCountForChild();
		if(!Keys.IsEmpty())
			NumRows = Keys.Num();


		switch(AttrType)
		{
		case EPCGMetadataTypes::Float:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<float>*>(AttrBase);
			auto AttrDest = CreateAttributeFloat(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				if (ValueIndex != InvalidIndex)
					AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Double:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<double>*>(AttrBase);
			auto AttrDest = CreateAttributeDouble(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				if(ValueIndex != InvalidIndex)
					AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Integer32:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<int>*>(AttrBase);

			auto AttrDest = CreateAttributeInt(AttrName);
			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Integer64:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<int64>*>(AttrBase);
			auto AttrDest = CreateAttributeInt64(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Boolean:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<bool>*>(AttrBase);

			auto AttrDest = CreateAttributeInt(AttrName);
			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector2:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector2d>*>(AttrBase);
			auto AttrDest = CreateAttributeVector2d(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector>*>(AttrBase);
			auto AttrDest = CreateAttributeVector3d(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector4:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector4d>*>(AttrBase);
			auto AttrDest = CreateAttributeVector4d(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Quaternion:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FQuat>*>(AttrBase);
			auto AttrDest = CreateAttributeVector4d(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				FQuat Quat = Attr->GetValueFromItemKey(ValueIndex);
				AttrDest->Values[Index].X = Quat.X;
				AttrDest->Values[Index].Y = Quat.Y;
				AttrDest->Values[Index].Z = Quat.Z;
				AttrDest->Values[Index].W = Quat.W;
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::String:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FString>*>(AttrBase);
			auto AttrDest = CreateAttributeString(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Name:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FName>*>(AttrBase);
			auto AttrDest = CreateAttributeString(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::SoftObjectPath:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(AttrBase);
			auto AttrDest = CreateAttributeSoftObjectPath(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::SoftClassPath:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(AttrBase);
			auto AttrDest = CreateAttributeSoftClassPath(AttrName);

			AttrDest->Values.SetNum(NumRows);
			for(int Index = 0; Index < NumRows; Index++)
			{
				int ValueIndex = Keys.IsEmpty() ? Index : Keys[Index];
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(ValueIndex).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		default:
			break;
		}
	}
}


UHoudiniPCGDataAttributeBase* UHoudiniPCGDataObject::FindAttribute(const FString& AttrName)
{
	for (auto& Attr : Attributes)
	{
		if(Attr->GetAttrName() == AttrName)
			return Attr.Get();
	}
	return nullptr;
}

UHoudiniPCGDataAttributeFloat* UHoudiniPCGDataObject::CreateAttributeFloat(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeFloat>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeDouble * UHoudiniPCGDataObject::CreateAttributeDouble(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeDouble>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeInt* UHoudiniPCGDataObject::CreateAttributeInt(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeInt>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeInt64* UHoudiniPCGDataObject::CreateAttributeInt64(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeInt64>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeString* UHoudiniPCGDataObject::CreateAttributeString(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeString>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeVector2d * UHoudiniPCGDataObject::CreateAttributeVector2d(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeVector2d>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeVector3d* UHoudiniPCGDataObject::CreateAttributeVector3d(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeVector3d>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeVector4d* UHoudiniPCGDataObject::CreateAttributeVector4d(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeVector4d>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeQuat * UHoudiniPCGDataObject::CreateAttributeQuat(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeQuat>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeSoftObjectPath * UHoudiniPCGDataObject::CreateAttributeSoftObjectPath(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeSoftObjectPath>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeSoftClassPath* UHoudiniPCGDataObject::CreateAttributeSoftClassPath(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeSoftClassPath>(this, FName(AttributeName));
	Result->SetAttrName(AttributeName);
	return Result;
}

int UHoudiniPCGDataObject::GetNumRows() const
{
	if(Attributes.IsEmpty())
		return 0;
	return Attributes[0]->GetNumValues();
}


bool UHoudiniPCGDataCollection::operator==(const UHoudiniPCGDataCollection& Other) const
{
	auto CheckObjects = [](UHoudiniPCGDataObject* Obj1, UHoudiniPCGDataObject*Obj2)
	{
		if(IsValid(Obj1) && IsValid(Obj2))
		{
			return (*Obj1) == (*Obj2);
		}
		else if(Obj1 == nullptr && Obj2 == nullptr)
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	bool bSame = true;
	bSame &= CheckObjects(this->Details, Other.Details);
	bSame &= CheckObjects(this->Vertices, Other.Vertices);
	bSame &= CheckObjects(this->Primitives, Other.Primitives);
	bSame &= CheckObjects(this->Points, Other.Points);
	return bSame;

	
}

bool UHoudiniPCGDataCollection::operator!=(const UHoudiniPCGDataCollection& Other) const
{
	return !(*this == Other);
}

void UHoudiniPCGDataCollection::AddObject(UHoudiniPCGDataObject* Object)
{
	if(Object->PCGDataType == EPCGDataType::Point)
	{
		Type = EHoudiniPCGDataType::InputPCGGeometry;
		Points = Object;
	}
	else if (Object->PCGDataType == EPCGDataType::Spline)
	{
		Type = EHoudiniPCGDataType::InputPCGSplines;
		Splines.Add(Object);
	}
	else if (Object->PCGTags.Contains(TEXT("Vertices")))
	{
		Type = EHoudiniPCGDataType::InputPCGGeometry;
		Vertices = Object;
	}
	else if(Object->PCGTags.Contains(TEXT("Primitives")))
	{
		Type = EHoudiniPCGDataType::InputPCGGeometry;
		Primitives = Object;
	}
	else if(Object->PCGTags.Contains(TEXT("Details")))
	{
		Type = EHoudiniPCGDataType::InputPCGGeometry;
		Details = Object;
	}
}

UHoudiniPCGOutputData::UHoudiniPCGOutputData(class FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{

}