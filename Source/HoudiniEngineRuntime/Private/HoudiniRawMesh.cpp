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
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) :
	AssetId(HoudiniGeoPartObject.AssetId),
	ObjectId(HoudiniGeoPartObject.ObjectId),
	GeoId(HoudiniGeoPartObject.GeoId),
	PartId(HoudiniGeoPartObject.PartId),
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(HAPI_AssetId OtherAssetId, FHoudiniGeoPartObject& HoudiniGeoPartObject) :
	AssetId(OtherAssetId),
	ObjectId(HoudiniGeoPartObject.ObjectId),
	GeoId(HoudiniGeoPartObject.GeoId),
	PartId(HoudiniGeoPartObject.PartId),
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniRawMesh& HoudiniRawMesh) :
	AssetId(HoudiniRawMesh.AssetId),
	ObjectId(HoudiniRawMesh.ObjectId),
	GeoId(HoudiniRawMesh.GeoId),
	PartId(HoudiniRawMesh.PartId),
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
	return 0;
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
