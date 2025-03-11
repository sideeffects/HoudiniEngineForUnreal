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

#include "HoudiniPCGInputObject.h"

#include <Data/PCGPointData.h>

#include "UObject/TextProperty.h"
#include "PCGParamData.h"

// We use StaticType to identify which subclass FHoudiniPCGInputAttributeData was created.
// Since this class has to be defined in HoudiniEngineRuntime we unfotuantely can't use
// HAPI_StorageType instead. Ensure these values are unique. You could use a compile time
// hash, but harder to read, etc.
int FHoudiniPCGInputAttributeData<float>::StaticType = 1;
int FHoudiniPCGInputAttributeData<double>::StaticType = 2;
int FHoudiniPCGInputAttributeData<int>::StaticType = 3;
int FHoudiniPCGInputAttributeData<int64>::StaticType = 4;
int FHoudiniPCGInputAttributeData<FString>::StaticType = 5;
int FHoudiniPCGInputAttributeData<FVector2d>::StaticType = 6;
int FHoudiniPCGInputAttributeData<FVector>::StaticType = 7;
int FHoudiniPCGInputAttributeData<FVector4d>::StaticType = 8;
int FHoudiniPCGInputAttributeData<FQuat>::StaticType = 8;

template<typename Type>
FHoudiniPCGInputAttributeData<Type>::FHoudiniPCGInputAttributeData(const FName& AttributeName)
{
	Name = AttributeName;
	DataType = FHoudiniPCGInputAttributeData<Type>::StaticType;
}

bool UHoudiniPCGInputObject::operator==(const UHoudiniPCGInputObject& Other) const
{
	// very simple, optimize?
	return (this->Attributes == Other.Attributes);
}

bool UHoudiniPCGInputObject::operator!=(const UHoudiniPCGInputObject& Other) const
{
	return !(*this == Other);
}

void UHoudiniPCGInputObject::Initialize(const UPCGData* PCGData)
{
	if(PCGData->IsA<UPCGParamData>())
		Initialize(Cast<UPCGParamData>(PCGData));
	else if(PCGData->IsA<UPCGPointData>())
		Initialize(Cast<UPCGPointData>(PCGData));
}


void UHoudiniPCGInputObject::Initialize(const UPCGPointData* PCGPointData)
{
	const UPCGMetadata* Metadata = PCGPointData->ConstMetadata();

	const TArray<FPCGPoint> & Points = PCGPointData->GetPoints();

	{
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector>>(TEXT("P"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Position = Points[Index].Transform.GetLocation();
			AttrDest->Values[Index][0] = Position.X * 100.0f;
			AttrDest->Values[Index][1] = Position.Z * 100.0f;
			AttrDest->Values[Index][2] = Position.Y * 100.0f;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector>>(TEXT("Scale"));
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
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector4d>>(TEXT("BoundsMin"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Value = Points[Index].BoundsMin;
			AttrDest->Values[Index][0] = Value.X;
			AttrDest->Values[Index][1] = Value.Z;
			AttrDest->Values[Index][2] = Value.Y;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector4d>>(TEXT("BoundsMax"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			FVector Value = Points[Index].BoundsMin;
			AttrDest->Values[Index][0] = Value.X;
			AttrDest->Values[Index][1] = Value.Z;
			AttrDest->Values[Index][2] = Value.Y;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector4d>>(TEXT("Cd"));
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
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector4>>(TEXT("orient"));
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
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<float>>(TEXT("Density"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			AttrDest->Values[Index] = Points[Index].Density;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<float>>(TEXT("Steepness"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			AttrDest->Values[Index] = Points[Index].Steepness;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	{
		auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<int32>>(TEXT("Seed"));
		AttrDest->Values.SetNum(Points.Num());
		for(int Index = 0; Index < Points.Num(); Index++)
		{
			AttrDest->Values[Index] = Points[Index].Seed;
		}
		Attributes.Emplace(MoveTemp(AttrDest));
	}

	AddMetaDataAttributes(Metadata);
}

void UHoudiniPCGInputObject::Initialize(const UPCGParamData* PCGParamData)
{
	const UPCGMetadata* Metadata = PCGParamData->ConstMetadata();
	AddMetaDataAttributes(Metadata);
}

void UHoudiniPCGInputObject::AddMetaDataAttributes(const UPCGMetadata* Metadata)
{
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;

	Metadata->GetAttributes(AttributeNames, AttributeTypes);

	int NumRows = Metadata->GetItemCountForChild();

	for(int AttrIndex = 0; AttrIndex < AttributeTypes.Num(); AttrIndex++)
	{
		EPCGMetadataTypes AttrType = AttributeTypes[AttrIndex];
		const FName& AttrName = AttributeNames[AttrIndex];
		const FPCGMetadataAttributeBase* AttrBase = Metadata->GetConstAttribute(AttrName);

		switch(AttrType)
		{
		case EPCGMetadataTypes::Float:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<float>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<float>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Double:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<double>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<double>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Integer32:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<int>*>(AttrBase);

			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<int32>>(AttrName);
			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Integer64:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<int64>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<int64>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Boolean:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<bool>*>(AttrBase);

			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<int32>>(AttrName);
			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index) ? 1 : 0;
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector2:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector2d>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector2d>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 2);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 3);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Vector4:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FVector4d>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector4d>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 4);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Quaternion:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FQuat>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FVector4d>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 4);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
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
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FString>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index);
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::Name:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FName>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FString>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::SoftObjectPath:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FString>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
			{
				AttrDest->Values[Index] = Attr->GetValueFromItemKey(Index).ToString();
			}

			Attributes.Emplace(MoveTemp(AttrDest));
		}
		break;
		case EPCGMetadataTypes::SoftClassPath:
		{
			auto* Attr = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(AttrBase);
			auto AttrDest = MakeUnique<FHoudiniPCGInputAttributeData<FString>>(AttrName);

			AttrDest->Values.SetNum(NumRows * 1);
			for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
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

