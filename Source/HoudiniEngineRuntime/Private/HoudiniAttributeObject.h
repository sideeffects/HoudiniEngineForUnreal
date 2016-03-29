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

#include "HoudiniEngineString.h"


class FString;
class FArchive;
struct HAPI_AttributeInfo;
struct FHoudiniGeoPartObject;
struct FHoudiniAttributeObject;


struct HOUDINIENGINERUNTIME_API FHoudiniAttributeObject
{
public:

	FHoudiniAttributeObject();
	FHoudiniAttributeObject(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* InAttributeName,
		const HAPI_AttributeInfo& AttributeInfo);
	FHoudiniAttributeObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId,
		const char* InAttributeName, const HAPI_AttributeInfo& AttributeInfo);
	FHoudiniAttributeObject(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const FString& InAttributeName,
		const HAPI_AttributeInfo& AttributeInfo);
	FHoudiniAttributeObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId,
		const FString& InAttributeName, const HAPI_AttributeInfo& AttributeInfo);
	FHoudiniAttributeObject(const FHoudiniAttributeObject& HoudiniAttributeObject);

public:

	/** Return attribute data as an integer array. **/
	bool HapiGetValues(TArray<int32>& Values, int32& TupleSize) const;

	/** Return attribute data as a float array. **/
	bool HapiGetValues(TArray<float>& Values, int32& TupleSize) const;

	/** Return attribute data as a string array. **/
	bool HapiGetValues(TArray<FHoudiniEngineString>& Values, int32& TupleSize) const;
	bool HapiGetValues(TArray<FString>& Values, int32& TupleSize) const;

public:

	/** Return attribute data as an integer array transfered to vertex values. **/
	bool HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<int32>& Values, int32& TupleSize) const;

	/** Return attribute data as a float array transfered to vertex values. **/
	bool HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<float>& Values, int32& TupleSize) const;

	/** Return attribute data as a string array transfered to vertex values. **/
	bool HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<FHoudiniEngineString>& Values,
		int32& TupleSize) const;
	bool HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<FString>& Values, int32& TupleSize) const;

public:

	/** Return true if this attribute is an array. **/
	bool HapiIsArray() const;

	/** Return true if this attribute exists. **/
	bool HapiExists() const;

	/** Return tuple size of the attribute. **/
	int32 HapiGetTupleSize() const;

public:

	/** Update attribute value(s). **/
	bool HapiRefetch();

public:

	/** Serialization. **/
	bool Serialize(FArchive& Ar);

	/** Return hash value for this object, used when using this object as a key inside hashing containers. **/
	uint32 GetTypeHash() const;

protected:

	/** HAPI: Retrieve attribute info structure. **/
	bool HapiGetAttributeInfo(HAPI_AttributeInfo& AttributeInfo) const;

protected:

	/** Raw data for this attribute. **/
	TArray<uint8> Value;

	/** Number of entries. **/
	int32 ValueCount;

	/** Size of each entry. **/
	int32 ValueSize;

protected:

	/** Name of the attribute. **/
	FString AttributeName;

	/** Attribute association. **/
	HAPI_AttributeOwner AttributeOwner;

	/** Attribute storage type. **/
	HAPI_StorageType StorageType;

	/** Associated HAPI ids. **/
	HAPI_AssetId AssetId;
	HAPI_ObjectId ObjectId;
	HAPI_GeoId GeoId;
	HAPI_PartId PartId;

protected:

	/** Flags used by attribute object. **/
	uint32 HoudiniAttributeObjectFlagsPacked;

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniAttributeObjectVersion;
};


/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniAttributeObject& HoudiniAttributeObject);

/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive& operator<<(FArchive& Ar, FHoudiniAttributeObject& HoudiniAttributeObject);
