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

class FArchive;
class FTransform;


struct FHoudiniGeoPartObject
{

public:

	/** Constructors. **/
	FHoudiniGeoPartObject();
	FHoudiniGeoPartObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);
	FHoudiniGeoPartObject(const FTransform& InTransform, const FString& InObjectName, const FString& InPartName, HAPI_AssetId InAssetId,
						  HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

public:

	/** Return hash value for this object, used when using this object as a key inside hashing containers. **/
	uint32 GetTypeHash() const;

	/** Comparison operator, used by hashing containers. **/
	bool operator==(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const;

	/** Compare based on object and part name. **/
	bool CompareNames(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const;

	/** Serialization. **/
	void Serialize(FArchive& Ar);

	/** Return true if this geo part object corresponds to a valid HAPI object. **/
	bool IsValid() const;

	/** Return true if this geo part object corresponds to an instancer. **/
	bool IsInstancer() const;

	/** Return true if this geo part object is visible. **/
	bool IsVisible() const;

	/** Return true if this geo part object is a curve. **/
	bool IsCurve() const;

	/** Return true if this geo part object has just been loaded. **/
	bool IsLoaded() const;

	/** Return true if this geo part object is editable. **/
	bool IsEditable() const;

	/** Return true if this geo part is used for collision. **/
	bool IsCollidable() const;

	/** Return true if this geo part is used for collision and is renderable. **/
	bool IsRenderCollidable() const;

	/** Return true if corresponding geometry has changed. **/
	bool HasGeoChanged() const;

public:

	/** Get node id for this geo part object. **/
	HAPI_NodeId GetNodeId() const;
	HAPI_NodeId GetNodeId(HAPI_AssetId InAssetId) const;

	/** Return true if this geo part has parameters. **/
	bool HasParameters() const;
	bool HasParameters(HAPI_AssetId InAssetId) const;

public:

	/** Transform of this geo part object. **/
	FTransform TransformMatrix;

	/** Name of associated object. **/
	FString ObjectName;

	/** Name of associated part. **/
	FString PartName;

	/** Name of group which was used for splitting, empty if there's none. **/
	FString SplitName;

	/** Id of corresponding HAPI Asset. **/
	HAPI_AssetId AssetId;

	/** Id of corresponding HAPI Object. **/
	HAPI_ObjectId ObjectId;

	/** Id of corresponding HAPI Geo. **/
	HAPI_GeoId GeoId;

	/** Id of corresponding HAPI Part. **/
	HAPI_PartId PartId;

	/** Id of a split. In most cases this will be 0. **/
	int32 SplitId;

	/** Scale factor used for generated geometry of this geo part object. **/
	float GeneratedGeometryScaleFactor;

	/** Scale factor used for transforms of this geo part object. **/
	float TransformScaleFactor;

	/* Is set to true when referenced object is visible. This is typically used by instancers. **/
	bool bIsVisible;

	/** Is set to true when referenced object is an instancer. **/
	bool bIsInstancer;

	/** Is set to true when referenced object is a curve. **/
	bool bIsCurve;

	/** Is set to true when referenced object is editable. **/
	bool bIsEditable;

	/** Is set to true when geometry has changed. **/
	bool bHasGeoChanged;

	/** Is set to true when referenced object is collidable. **/
	bool bIsCollidable;

	/** Is set to true when referenced object is collidable and is renderable. **/
	bool bIsRenderCollidable;

	/** Is set to true when referenced object has just been loaded. **/
	bool bIsLoaded;
};


/** Function used by hasing containers to create a unique hash for this type of object. **/
uint32 GetTypeHash(const FHoudiniGeoPartObject& HoudiniGeoPartObject);
