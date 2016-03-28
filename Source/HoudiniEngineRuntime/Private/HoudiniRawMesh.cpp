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


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) :
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(HAPI_AssetId OtherAssetId, FHoudiniGeoPartObject& HoudiniGeoPartObject) :
	HoudiniRawMeshVersion(VER_HOUDINI_ENGINE_RAWMESH_BASE)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniRawMesh& HoudiniRawMesh) :
	HoudiniRawMeshVersion(HoudiniRawMesh.HoudiniRawMeshVersion)
{

}


bool
FHoudiniRawMesh::BuildRawMesh(FRawMesh& RawMesh, bool bFullRebuild) const
{
	return false;
}


void
FHoudiniRawMesh::Serialize(FArchive& Ar)
{
	HoudiniRawMeshVersion = VER_HOUDINI_ENGINE_RAWMESH_AUTOMATIC_VERSION;
	Ar << HoudiniRawMeshVersion;
}
