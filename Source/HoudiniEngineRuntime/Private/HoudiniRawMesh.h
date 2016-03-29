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


class FArchive;
struct FRawMesh;
struct FHoudiniRawMesh;
struct FHoudiniGeoPartObject;


struct HOUDINIENGINERUNTIME_API FHoudiniRawMesh
{
public:

	FHoudiniRawMesh(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);
	FHoudiniRawMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject);
	FHoudiniRawMesh(HAPI_AssetId OtherAssetId, FHoudiniGeoPartObject& HoudiniGeoPartObject);
	FHoudiniRawMesh(const FHoudiniRawMesh& HoudiniRawMesh);

public:

	/** Create an Unreal raw mesh from this Houdini raw mesh. **/
	bool BuildRawMesh(FRawMesh& RawMesh, bool bFullRebuild = true) const;

	/** Serialization. **/
	bool Serialize(FArchive& Ar);

	/** Return hash value for this object, used when using this object as a key inside hashing containers. **/
	uint32 GetTypeHash() const;

public:

	/** Refetch data from Houdini Engine. **/
	bool HapiRefetch();

protected:

	/** Reset attribute maps and index buffer. **/
	void ResetAttributesAndVertices();

protected:

	/** Maps of Houdini attributes. **/
	TMap<FString, FHoudiniAttributeObject> AttributesPoint;
	TMap<FString, FHoudiniAttributeObject> AttributesVertex;
	TMap<FString, FHoudiniAttributeObject> AttributesPrimitive;
	TMap<FString, FHoudiniAttributeObject> AttributesDetail;

	/** Houdini vertices. **/
	TArray<int32> Vertices;

protected:

	/** HAPI ids. **/
	HAPI_AssetId AssetId;
	HAPI_ObjectId ObjectId;
	HAPI_GeoId GeoId;
	HAPI_PartId PartId;
	int32 SplitId;

protected:

	/** Flags used by raw mesh. **/
	uint32 HoudiniRawMeshFlagsPacked;

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniRawMeshVersion;
};


/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniRawMesh& HoudiniRawMesh);

/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive& operator<<(FArchive& Ar, FHoudiniRawMesh& HoudiniRawMesh);
