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
		FMemory::Memcpy(VertexPositions.GetData(), PositionValues.GetData(), PositionValues.Num() * sizeof(float));
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
	return false;
}


bool
FHoudiniRawMesh::HapiGetVertexNormals(TArray<FVector>& VertexNormals, bool bSwapYZAxis) const
{
	return false;
}


bool
FHoudiniRawMesh::HapiGetVertexUVs(TArray<TArray<FVector2D> >& VertexUVs, bool bPatchUVAxis) const
{
	return false;
}
