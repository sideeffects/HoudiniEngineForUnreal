/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRawMesh.h"
#include "HoudiniRawMeshVersion.h"
#include "HoudiniGeoPartObject.h"


FHoudiniRawMesh::FHoudiniRawMesh(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId) :
	AssetId(InAssetId),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	HoudiniRawMeshFlagsPacked(0u),
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) :
	AssetId(HoudiniGeoPartObject.AssetId),
	ObjectId(HoudiniGeoPartObject.ObjectId),
	GeoId(HoudiniGeoPartObject.GeoId),
	PartId(HoudiniGeoPartObject.PartId),
	HoudiniRawMeshFlagsPacked(0u),
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(HAPI_AssetId OtherAssetId, FHoudiniGeoPartObject& HoudiniGeoPartObject) :
	AssetId(OtherAssetId),
	ObjectId(HoudiniGeoPartObject.ObjectId),
	GeoId(HoudiniGeoPartObject.GeoId),
	PartId(HoudiniGeoPartObject.PartId),
	HoudiniRawMeshFlagsPacked(0u),
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniRawMesh& HoudiniRawMesh) :
	AssetId(HoudiniRawMesh.AssetId),
	ObjectId(HoudiniRawMesh.ObjectId),
	GeoId(HoudiniRawMesh.GeoId),
	PartId(HoudiniRawMesh.PartId),
	HoudiniRawMeshFlagsPacked(0u),
	HoudiniRawMeshVersion(HoudiniRawMesh.HoudiniRawMeshVersion)
{

}


bool
FHoudiniRawMesh::BuildRawMesh(FRawMesh& RawMesh, bool bFullRebuild) const
{
	return false;
}


bool
FHoudiniRawMesh::Serialize(FArchive& Ar)
{
	HoudiniRawMeshVersion = VER_HOUDINI_ENGINE_RAWMESH_AUTOMATIC_VERSION;
	Ar << HoudiniRawMeshVersion;

	Ar << HoudiniRawMeshFlagsPacked;

	Ar << AttributesPoint;
	Ar << AttributesVertex;
	Ar << AttributesPrimitive;
	Ar << AttributesDetail;

	Ar << Vertices;

	return true;
}


uint32
FHoudiniRawMesh::GetTypeHash() const
{
	int32 HashBuffer[4] = { ObjectId, GeoId, PartId, SplitId };
	return FCrc::MemCrc_DEPRECATED((void*) &HashBuffer[0], sizeof(HashBuffer));
}


FArchive&
operator<<(FArchive& Ar, FHoudiniRawMesh& HoudiniRawMesh)
{
	HoudiniRawMesh.Serialize(Ar);
	return Ar;
}


uint32
GetTypeHash(const FHoudiniRawMesh& HoudiniRawMesh)
{
	return HoudiniRawMesh.GetTypeHash();
}


void
FHoudiniRawMesh::ResetAttributesAndVertices()
{
	AttributesPoint.Empty();
	AttributesVertex.Empty();
	AttributesPrimitive.Empty();
	AttributesDetail.Empty();

	Vertices.Empty();
}


bool
FHoudiniRawMesh::LocateAttribute(const FString& AttributeName, FHoudiniAttributeObject& HoudiniAttributeObject) const
{
	if(LocateAttributePoint(AttributeName, HoudiniAttributeObject))
	{
		return true;
	}

	if(LocateAttributeVertex(AttributeName, HoudiniAttributeObject))
	{
		return true;
	}

	if(LocateAttributePrimitive(AttributeName, HoudiniAttributeObject))
	{
		return true;
	}

	if(LocateAttributeDetail(AttributeName, HoudiniAttributeObject))
	{
		return true;
	}

	return false;
}


bool
FHoudiniRawMesh::LocateAttributePoint(const FString& AttributeName,
	FHoudiniAttributeObject& HoudiniAttributeObject) const
{
	const FHoudiniAttributeObject* FoundHoudiniAttributeObject = AttributesPoint.Find(AttributeName);
	if(FoundHoudiniAttributeObject)
	{
		HoudiniAttributeObject = *FoundHoudiniAttributeObject;
		return true;
	}

	return false;
}


bool
FHoudiniRawMesh::LocateAttributeVertex(const FString& AttributeName,
	FHoudiniAttributeObject& HoudiniAttributeObject) const
{
	const FHoudiniAttributeObject* FoundHoudiniAttributeObject = AttributesVertex.Find(AttributeName);
	if(FoundHoudiniAttributeObject)
	{
		HoudiniAttributeObject = *FoundHoudiniAttributeObject;
		return true;
	}

	return false;
}


bool
FHoudiniRawMesh::LocateAttributePrimitive(const FString& AttributeName,
	FHoudiniAttributeObject& HoudiniAttributeObject) const
{
	const FHoudiniAttributeObject* FoundHoudiniAttributeObject = AttributesPrimitive.Find(AttributeName);
	if(FoundHoudiniAttributeObject)
	{
		HoudiniAttributeObject = *FoundHoudiniAttributeObject;
		return true;
	}

	return false;
}


bool
FHoudiniRawMesh::LocateAttributeDetail(const FString& AttributeName,
	FHoudiniAttributeObject& HoudiniAttributeObject) const
{
	const FHoudiniAttributeObject* FoundHoudiniAttributeObject = AttributesDetail.Find(AttributeName);
	if(FoundHoudiniAttributeObject)
	{
		HoudiniAttributeObject = *FoundHoudiniAttributeObject;
		return true;
	}

	return false;
}


bool
FHoudiniRawMesh::HapiRefetch()
{
	FHoudiniGeoPartObject HoudiniGeoPartObject(AssetId, ObjectId, GeoId, PartId);

	if(!HoudiniGeoPartObject.HapiGetVertices(AssetId, Vertices))
	{
		ResetAttributesAndVertices();
		return false;
	}

	if(!HoudiniGeoPartObject.HapiGetAllAttributeObjects(AttributesPoint, AttributesVertex, AttributesPrimitive,
		AttributesDetail))
	{
		ResetAttributesAndVertices();
		return false;
	}

	return true;
}


bool
FHoudiniRawMesh::HapiGetVertexPositions(TArray<FVector>& VertexPositions, float GeometryScale, bool bSwapYZAxis) const
{
	VertexPositions.Empty();

	FHoudiniAttributeObject HoudiniAttributeObject;

	// Position attribute is always on a point.
	if(!LocateAttributePoint(TEXT(HAPI_UNREAL_ATTRIB_POSITION), HoudiniAttributeObject))
	{
		return false;
	}

	// We now need to fetch attribute data.
	TArray<float> PositionValues;
	int32 TupleSize = 0;
	if(!HoudiniAttributeObject.HapiGetValues(PositionValues, TupleSize))
	{
		return false;
	}

	if(!PositionValues.Num())
	{
		return false;
	}

	if(3 == TupleSize)
	{
		int PositionEntries = PositionValues.Num() / 3;
		VertexPositions.SetNumUninitialized(PositionEntries);

		// We can do direct memory transfer.
		FMemory::Memcpy(VertexPositions.GetData(), PositionValues.GetData(), VertexPositions.Num() * sizeof(FVector));
	}
	else
	{
		// Position attribute is always size 3 in Houdini. Otherwise, a lot of things would break.
		return false;
	}

	for(int32 Idx = 0, Num = VertexPositions.Num(); Idx < Num; ++Idx)
	{
		FVector& VertexPosition = VertexPositions[Idx];
		VertexPosition *= GeometryScale;

		if(bSwapYZAxis)
		{
			Swap(VertexPosition.Y, VertexPosition.Z);
		}
	}

	return true;
}


bool
FHoudiniRawMesh::HapiGetVertexColors(TArray<FColor>& VertexColors) const
{
	VertexColors.Empty();

	FHoudiniAttributeObject HoudiniAttributeObject;

	// Colors can be on any attribute.
	if(!LocateAttribute(TEXT(HAPI_UNREAL_ATTRIB_COLOR), HoudiniAttributeObject))
	{
		return false;
	}

	TArray<float> ColorValues;
	int32 TupleSize = 0;

	if(!HoudiniAttributeObject.HapiGetValuesAsVertex(Vertices, ColorValues, TupleSize))
	{
		return false;
	}

	if(!ColorValues.Num())
	{
		return false;
	}

	if(3 == TupleSize || 4 == TupleSize)
	{
		int32 ColorCount = ColorValues.Num() / TupleSize;
		VertexColors.SetNumUninitialized(ColorCount);

		for(int32 Idx = 0; Idx < ColorCount; ++Idx)
		{
			FLinearColor WedgeColor;

			WedgeColor.R =
				FMath::Clamp(ColorValues[Idx * TupleSize + 0], 0.0f, 1.0f);
			WedgeColor.G =
				FMath::Clamp(ColorValues[Idx * TupleSize + 1], 0.0f, 1.0f);
			WedgeColor.B =
				FMath::Clamp(ColorValues[Idx * TupleSize + 2], 0.0f, 1.0f);

			if(4 == TupleSize)
			{
				// We have alpha.
				WedgeColor.A = FMath::Clamp(ColorValues[Idx * TupleSize + 3], 0.0f, 1.0f);
			}
			else
			{
				WedgeColor.A = 1.0f;
			}

			// Convert linear color to fixed color.
			VertexColors[Idx] = WedgeColor.ToFColor(false);
		}
	}
	else
	{
		// Color is always size 3 or 4 in Houdini.
		return false;
	}

	return true;
}


bool
FHoudiniRawMesh::HapiGetVertexNormals(TArray<FVector>& VertexNormals, bool bSwapYZAxis) const
{
	VertexNormals.Empty();

	FHoudiniAttributeObject HoudiniAttributeObject;

	// Colors can be on any attribute.
	if(!LocateAttribute(TEXT(HAPI_UNREAL_ATTRIB_NORMAL), HoudiniAttributeObject))
	{
		return false;
	}

	TArray<float> NormalValues;
	int32 TupleSize = 0;

	if(!HoudiniAttributeObject.HapiGetValuesAsVertex(Vertices, NormalValues, TupleSize))
	{
		return false;
	}

	if(!NormalValues.Num())
	{
		return false;
	}

	if(3 == TupleSize)
	{
		int32 NormalCount = NormalValues.Num() / 3;
		VertexNormals.SetNumUninitialized(NormalCount);

		// We can do direct memory transfer.
		FMemory::Memcpy(VertexNormals.GetData(), NormalValues.GetData(), VertexNormals.Num() * sizeof(FVector));

		if(bSwapYZAxis)
		{
			for(int32 Idx = 0; Idx < NormalCount; ++Idx)
			{
				FVector& NormalVector = VertexNormals[Idx];
				Swap(NormalVector.Y, NormalVector.Z);
			}
		}
	}
	else
	{
		// Normal is always size 3 in Houdini.
		return false;
	}

	return true;
}


bool
FHoudiniRawMesh::HapiGetVertexUVs(TMap<int32, TArray<FVector2D> >& VertexUVs, bool bPatchUVAxis) const
{
	VertexUVs.Empty();

	bool bFoundUVs = false;
	FHoudiniAttributeObject HoudiniAttributeObject;

	// Layer SOP supports up to 16 channels.
	for(int32 UVChannelIdx = 0; UVChannelIdx < 16; ++UVChannelIdx)
	{
		FString UVAttributeName = TEXT(HAPI_UNREAL_ATTRIB_UV);

		if(UVChannelIdx > 0)
		{
			UVAttributeName = FString::Printf(TEXT("%s%d"), *UVAttributeName, (UVChannelIdx + 1));
		}

		// Colors can be on any attribute.
		if(!LocateAttribute(UVAttributeName, HoudiniAttributeObject))
		{
			continue;
		}

		TArray<float> UVValues;
		int32 TupleSize = 0;

		if(!HoudiniAttributeObject.HapiGetValuesAsVertex(Vertices, UVValues, TupleSize))
		{
			continue;
		}

		if(!UVValues.Num() || !TupleSize)
		{
			continue;
		}

		int32 UVCount = UVValues.Num() / TupleSize;
		TArray<FVector2D> UVs;
		UVs.SetNumUninitialized(UVCount);

		if(2 == TupleSize)
		{
			// We can do direct memory transfer.
			FMemory::Memcpy(UVs.GetData(), UVValues.GetData(), UVs.Num() * sizeof(FVector2D));
			
			bFoundUVs = true;
		}
		else if(3 == TupleSize)
		{
			for(int32 Idx = 0; Idx < UVCount; ++Idx)
			{
				// Ignore 3rd coordinate.
				FVector2D UVPoint(UVValues[Idx * TupleSize + 0], UVValues[Idx * TupleSize + 1]);
				UVs[Idx] = UVPoint;
			}

			bFoundUVs = true;
		}
		else
		{
			continue;
		}

		if(bPatchUVAxis)
		{
			for(int32 Idx = 0; Idx < UVCount; ++Idx)
			{
				FVector2D& WedgeUV = UVs[Idx];
				WedgeUV.Y = 1.0f - WedgeUV.Y;
			}
		}

		VertexUVs.Add(UVChannelIdx, UVs);
	}

	return bFoundUVs;
}
