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

#pragma once

#include "HoudiniAttributeObject.h"


struct FRawMesh;
struct FHoudiniGeoPartObject;


struct HOUDINIENGINERUNTIME_API FHoudiniRawMesh
{
public:

	FHoudiniRawMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject);
	FHoudiniRawMesh(const FHoudiniRawMesh& HoudiniRawMesh);

public:

	/** Create an Unreal raw mesh from this Houdini raw mesh. **/
	bool CreateRawMesh(FRawMesh& RawMesh) const;

	/** Rebuild existing Unreal raw mesh. **/
	bool RebuildRawMesh(FRawMesh& RawMesh) const;

public:

	/** Maps of Houdini attributes. **/
	TMap<FString, FHoudiniAttributeObject> AttributesPoint;
	TMap<FString, FHoudiniAttributeObject> AttributesVertex;
	TMap<FString, FHoudiniAttributeObject> AttributesPrimitive;
	TMap<FString, FHoudiniAttributeObject> AttributesDetail;

protected:

	/** HAPI ids. **/
	HAPI_AssetId AssetId;
	HAPI_ObjectId ObjectId;
	HAPI_GeoId GeoId;
	HAPI_PartId PartId;
	int32 SplitId;
};
