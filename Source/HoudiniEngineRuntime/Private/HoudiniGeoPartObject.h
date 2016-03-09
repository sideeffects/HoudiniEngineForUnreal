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
struct FTransform;
struct HAPI_ObjectInfo;
class FHoudiniEngineString;

struct HOUDINIENGINERUNTIME_API FHoudiniGeoPartObject
{

public:

	/** Constructors. **/
	FHoudiniGeoPartObject();
	FHoudiniGeoPartObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

	FHoudiniGeoPartObject(const FTransform& InTransform, const FString& InObjectName, const FString& InPartName,
		HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

	FHoudiniGeoPartObject(const FHoudiniGeoPartObject& GeoPartObject, bool bCopyLoaded = false);

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

	/** Return true if this geo part object is a box. **/
	bool IsBox() const;

	/** Return true if this geo part object is a sphere. **/
	bool IsSphere() const;

	/** Return true if this geo part object is a volume. **/
	bool IsVolume() const;

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

	/** Return true if this object has a custom name. **/
	bool HasCustomName() const;

	/** Set custom name. **/
	void SetCustomName(const FString& CustomName);

public:

	/** Return object id. **/
	HAPI_ObjectId GetObjectId() const;

	/** Return geo id. **/
	HAPI_GeoId GetGeoId() const;

/** HAPI: Other helpers. **/
public:

	/** HAPI: Return true if given attribute exists. **/
	bool HapiCheckAttributeExistance(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(const std::string& AttributeName, HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(const char* AttributeName, HAPI_AttributeOwner AttributeOwner) const;

/** HAPI: Object related getters. **/
public:

	/** HAPI: Retrieve corresponding object info structure. **/
	bool HapiObjectGetInfo(HAPI_ObjectInfo& ObjectInfo) const;
	bool HapiObjectGetInfo(HAPI_AssetId OtherAssetId, HAPI_ObjectInfo& ObjectInfo) const;

	/** HAPI: If this is an instancer, return id of instanced object. Return -1 if no such object is found. **/
	HAPI_ObjectId HapiObjectGetToInstanceId() const;

	/** HAPI: Return object name. **/
	FHoudiniEngineString HapiObjectGetName() const;

	/** HAPI: Return object instance path. **/
	FHoudiniEngineString HapiObjectGetInstancePath() const;

	/** HAPI: Return true if object is visible. **/
	bool HapiObjectIsVisible() const;

	/** HAPI: Return true if object is an instancer. **/
	bool HapiObjectIsInstancer() const;

	/** HAPI: Return true if object transform has changed. **/
	bool HapiObjectHasTransformChanged() const;

	/** HAPI: Return true if any of the underlying geos have changed. **/
	bool HapiObjectHaveGeosChanged() const;

	/** HAPI: Get number of geos. **/
	int32 HapiObjectGetGeoCount() const;

	/** HAPI: Get associated node id. This corresponds to id of an obj node. **/
	HAPI_NodeId HapiObjectGetNodeId() const;
	HAPI_NodeId HapiObjectGetNodeId(HAPI_AssetId OtherAssetId) const;

/** HAPI: Geo related getters. **/
public:

	/** HAPI: Retrieve corresponding geo info structure. **/
	bool HapiGeoGetInfo(HAPI_GeoInfo& GeoInfo) const;
	bool HapiGeoGetInfo(HAPI_AssetId OtherAssetId, HAPI_GeoInfo& GeoInfo) const;

	/** HAPI: Return geo type. **/
	HAPI_GeoType HapiGeoGetType() const;

	/** HAPI: Return name of this geo. **/
	FHoudiniEngineString HapiGeoGetName() const;

	/** HAPI: Return geo node id. This corresponds to id of a sop node. **/
	HAPI_NodeId HapiGeoGetNodeId() const;
	HAPI_NodeId HapiGeoGetNodeId(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if this geo is editable. **/
	bool HapiGeoIsEditable() const;

	/** HAPI: Return true if this geo is templated. **/
	bool HapiGeoIsTemplated() const;

	/** HAPI: Return true if this is a display sop geo. **/
	bool HapiGeoIsDisplayGeo() const;

	/** HAPI: Return true if geo has changed. **/
	bool HapiGeoHasChanged() const;

	/** HAPI: Return true if material on this geo has changed. **/
	bool HapiGeoHasMaterialChanged() const;

	/** HAPI: Return number of point groups. **/
	int32 HapiGeoGetPointGroupCount() const;

	/** HAPI: Return number of primitive groups. **/
	int32 HapiGeoGetPrimitiveGroupCount() const;

	/** HAPI: Return number of parts within this geo. **/
	int32 HapiGeoGetPartCount() const;

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

	/** Name of the instancer material, if available. **/
	FString InstancerMaterialName;

	/** Name of attribute material, if available. **/
	FString InstancerAttributeMaterialName;

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

	/** Flags used by geo part object. **/
	union
	{
		struct
		{
			/* Is set to true when referenced object is visible. This is typically used by instancers. **/
			uint32 bIsVisible : 1;

			/** Is set to true when referenced object is an instancer. **/
			uint32 bIsInstancer : 1;

			/** Is set to true when referenced object is a curve. **/
			uint32 bIsCurve : 1;

			/** Is set to true when referenced object is editable. **/
			uint32 bIsEditable : 1;

			/** Is set to true when geometry has changed. **/
			uint32 bHasGeoChanged : 1;

			/** Is set to true when referenced object is collidable. **/
			uint32 bIsCollidable : 1;

			/** Is set to true when referenced object is collidable and is renderable. **/
			uint32 bIsRenderCollidable : 1;

			/** Is set to true when referenced object has just been loaded. **/
			uint32 bIsLoaded : 1;

			/** Unused flags. **/
			uint32 bPlaceHolderFlags : 3;

			/** Is set to true when referenced object has been loaded during transaction. **/
			uint32 bIsTransacting : 1;

			/** Is set to true when referenced object has a custom name. **/
			uint32 bHasCustomName : 1;

			/** Is set to true when referenced object is a box. **/
			uint32 bIsBox : 1;

			/** Is set to true when referenced object is a sphere. **/
			uint32 bIsSphere : 1;

			/** Is set to true when instancer material is available. **/
			uint32 bInstancerMaterialAvailable : 1;

			/** Is set to true when referenced object is a volume. **/
			uint32 bIsVolume : 1;

			/** Is set to true when instancer attribute material is available. **/
			uint32 bInstancerAttributeMaterialAvailable : 1;
		};

		uint32 HoudiniGeoPartObjectFlagsPacked;
	};

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniGeoPartObjectVersion;
};


/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniGeoPartObject& HoudiniGeoPartObject);


/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive& operator<<(FArchive& Ar, FHoudiniGeoPartObject& HoudiniGeoPartObject);


/** Functor used to sort geo part objects. **/
struct HOUDINIENGINERUNTIME_API FHoudiniGeoPartObjectSortPredicate
{
	bool operator()(const FHoudiniGeoPartObject& A, const FHoudiniGeoPartObject& B) const;
};
