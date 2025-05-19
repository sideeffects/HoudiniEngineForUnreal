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

#include "UObject/TextProperty.h"
#include "PCGParamData.h"



bool UHoudiniPCGDataObject::operator==(const UHoudiniPCGDataObject& Other) const
{
	// very simple, optimize?
	return (this->Attributes == Other.Attributes);
}

bool UHoudiniPCGDataObject::operator!=(const UHoudiniPCGDataObject& Other) const
{
	return !(*this == Other);
}

void UHoudiniPCGDataObject::Initialize(const UPCGData* PCGData, const TSet<FString> & Tags)
{
	PCGDataType = PCGData->GetDataType();
	PCGTags = Tags;
	if(PCGData->IsA<UPCGParamData>())
		Initialize(Cast<UPCGParamData>(PCGData));
	else if(PCGData->IsA<UPCGPointData>())
		Initialize(Cast<UPCGPointData>(PCGData));
	else if(PCGData->IsA<UPCGSplineData>())
		Initialize(Cast<UPCGSplineData>(PCGData));
}

void UHoudiniPCGDataObject::Initialize(const UPCGSplineData* PCGSplineData)
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

void UHoudiniPCGDataObject::Initialize(const UPCGPointData* PCGPointData)
{
	const UPCGMetadata* Metadata = PCGPointData->ConstMetadata();
	this->PCGDataType = PCGPointData->GetDataType();

	const TArray<FPCGPoint> & Points = PCGPointData->GetPoints();

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

	AddMetaDataAttributes(Metadata, Points.Num());
}

void UHoudiniPCGDataObject::Initialize(const UPCGParamData* PCGParamData)
{
	const UPCGMetadata* Metadata = PCGParamData->ConstMetadata();
	AddMetaDataAttributes(Metadata, 0);
}

void UHoudiniPCGDataObject::AddMetaDataAttributes(const UPCGMetadata* ParamMetadata, int DefaultNumRows)
{
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;

	ParamMetadata->GetAttributes(AttributeNames, AttributeTypes);

	
	for(int AttrIndex = 0; AttrIndex < AttributeTypes.Num(); AttrIndex++)
	{
		EPCGMetadataTypes AttrType = AttributeTypes[AttrIndex];
		const FString & AttrName = AttributeNames[AttrIndex].ToString();

		const FPCGMetadataAttributeBase* AttrBase = ParamMetadata->GetConstAttribute(AttributeNames[AttrIndex]);

		const UPCGMetadata* Metadata = AttrBase->GetMetadata();
		int NumRows = Metadata->GetItemCountForChild();

		// Normally Metadata->GetItemCountForChild() will return the number of rows of metadata, however, when reading
		// points, if the attributes just contain a default value, this will return zero. So this function takes
		// DefaultNumRows which should equal the number of points for point date.
		if(NumRows == 0)
			NumRows = DefaultNumRows;

		switch(AttrType)
		{
		case EPCGMetadataTypes::Float:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<float>*>(AttrBase);
			auto AttrDest = CreateAttributeFloat(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
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
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
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
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
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
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
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
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index) ? 1 : 0;
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector2:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector2d>*>(AttrBase);
			auto AttrDest = CreateAttributeVector2d(AttrName);

			AttrDest->Values.SetNum(NumRows * 2);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector>*>(AttrBase);
			auto AttrDest = CreateAttributeVector3d(AttrName);

			AttrDest->Values.SetNum(NumRows * 3);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector4:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector4d>*>(AttrBase);
			auto AttrDest = CreateAttributeVector4d(AttrName);

			AttrDest->Values.SetNum(NumRows * 4);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Quaternion:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FQuat>*>(AttrBase);
			auto AttrDest = CreateAttributeVector4d(AttrName);

			AttrDest->Values.SetNum(NumRows * 4);
			for(int Index = 0; Index < NumRows; Index++)
			{
				FQuat Quat = Attr->GetValueFromItemKey(Index);
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

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Name:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FName>*>(AttrBase);
			auto AttrDest = CreateAttributeString(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::SoftObjectPath:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(AttrBase);
			auto AttrDest = CreateAttributeSoftObjectPath(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::SoftClassPath:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(AttrBase);
			auto AttrDest = CreateAttributeSoftClassPath(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < NumRows; Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index).ToString();
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
		if(Attr->AttrName == AttrName)
			return Attr.Get();
	}
	return nullptr;
}

UHoudiniPCGDataAttributeFloat* UHoudiniPCGDataObject::CreateAttributeFloat(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeFloat>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeDouble * UHoudiniPCGDataObject::CreateAttributeDouble(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeDouble>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeInt* UHoudiniPCGDataObject::CreateAttributeInt(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeInt>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeInt64* UHoudiniPCGDataObject::CreateAttributeInt64(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeInt64>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeString* UHoudiniPCGDataObject::CreateAttributeString(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeString>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeVector2d * UHoudiniPCGDataObject::CreateAttributeVector2d(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeVector2d>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeVector3d* UHoudiniPCGDataObject::CreateAttributeVector3d(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeVector3d>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeVector4d* UHoudiniPCGDataObject::CreateAttributeVector4d(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeVector4d>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeQuat * UHoudiniPCGDataObject::CreateAttributeQuat(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeQuat>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeSoftObjectPath * UHoudiniPCGDataObject::CreateAttributeSoftObjectPath(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeSoftObjectPath>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

UHoudiniPCGDataAttributeSoftClassPath* UHoudiniPCGDataObject::CreateAttributeSoftClassPath(const FString& AttributeName)
{
	auto* Result = NewObject<UHoudiniPCGDataAttributeSoftClassPath>(this, FName(AttributeName));
	Result->AttrName = FName(AttributeName);
	return Result;
}

int UHoudiniPCGDataObject::GetNumRows() const
{
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