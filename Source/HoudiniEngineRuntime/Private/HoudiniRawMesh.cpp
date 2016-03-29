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

