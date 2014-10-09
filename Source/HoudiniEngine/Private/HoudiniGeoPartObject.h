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

class FMatrix;
class FArchive;


struct FHoudiniGeoPartObject
{

public:

	/** Constructors. **/
	FHoudiniGeoPartObject();
	FHoudiniGeoPartObject(const FMatrix& InTransform, const FString& InObjectName, const FString& InPartName, HAPI_AssetId InAssetId,
						  HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId, bool bInIsVisible = true, bool bInIsInstancer = false);

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

	/** Return true if this geo part object has just been loaded. **/
	bool IsLoaded() const;

public:

	/** Transform of this geo part object. **/
	FMatrix TransformMatrix;

	/** Name of associated object. **/
	FString ObjectName;

	/** Name of associated part. **/
	FString PartName;

	/** Id of corresponding HAPI Asset. **/
	HAPI_AssetId AssetId;

	/** Id of corresponding HAPI Object. **/
	HAPI_ObjectId ObjectId;

	/** Id of corresponding HAPI Geo. **/
	HAPI_GeoId GeoId;

	/** Id of corresponding HAPI Part. **/
	HAPI_PartId PartId;

	/* Is set to true when referenced object is visible. This is typically used by instancers. **/
	bool bIsVisible;

	/** Is set to true when referenced object is an instancer. **/
	bool bIsInstancer;

	/** Is set to true when referenced object has just been loaded. **/
	bool bIsLoaded;
};


/** Function used by hasing containers to create a unique hash for this type of object. **/
uint32 GetTypeHash(const FHoudiniGeoPartObject& HoudiniGeoPartObject);
