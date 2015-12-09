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

	/** Set to true flag which specifies that Unreal material has been assigned. **/
	void SetUnrealMaterialAssigned();

	/** Return true if this object has Unreal material assigned. **/
	bool HasUnrealMaterialAssigned() const;

	/** Reset flag which specifies that Unreal material has been assigned. **/
	void ResetUnrealMaterialAssigned();

	/** Return true if this object has native Houdini material. **/
	bool HasNativeHoudiniMaterial() const;

	/** Return true if this object has a custom name. **/
	bool HasCustomName() const;

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

			/** Is set to true if referenced Geo Part Object object has native Houdini material. **/
			uint32 bHasNativeHoudiniMaterial : 1;

			/** Is set to true if referenced object has Unreal material assigned. **/
			uint32 bHasUnrealMaterialAssigned : 1;

			/** Is set to true when native material needs to be refetched. **/
			uint32 bNativeHoudiniMaterialRefetch : 1;

			/** Is set to true when referenced object has been loaded during transaction. **/
			uint32 bIsTransacting : 1;

			/** Is set to true when referenced object has a custom name. **/
			uint32 bHasCustomName : 1;
		};

		uint32 HoudiniGeoPartObjectFlagsPacked;
	};
};


/** Function used by hasing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniGeoPartObject& HoudiniGeoPartObject);


/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive& operator<<(FArchive& Ar, FHoudiniGeoPartObject& HoudiniGeoPartObject);

/** Functor used to sort geo part objects. **/
struct HOUDINIENGINERUNTIME_API FHoudiniGeoPartObjectSortPredicate
{
	bool operator()(const FHoudiniGeoPartObject& A, const FHoudiniGeoPartObject& B) const;
};
