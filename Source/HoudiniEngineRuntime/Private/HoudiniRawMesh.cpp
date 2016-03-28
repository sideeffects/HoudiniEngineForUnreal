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
#include "HoudiniGeoPartObject.h"


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject)
{

}


FHoudiniRawMesh::FHoudiniRawMesh(const FHoudiniRawMesh& HoudiniRawMesh)
{

}


bool
FHoudiniRawMesh::CreateRawMesh(FRawMesh& RawMesh) const
{
	return false;
}


bool
FHoudiniRawMesh::RebuildRawMesh(FRawMesh& RawMesh) const
{
	return false;
}
