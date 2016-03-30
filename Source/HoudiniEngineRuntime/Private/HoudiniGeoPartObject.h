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
struct HAPI_GeoInfo;
struct HAPI_PartInfo;
struct HAPI_ObjectInfo;
class FHoudiniEngineString;
struct FHoudiniAttributeObject;


struct HOUDINIENGINERUNTIME_API FHoudiniGeoPartObject
{

public:

	/** Constructors. **/
	FHoudiniGeoPartObject();
	FHoudiniGeoPartObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

	FHoudiniGeoPartObject(const FTransform& InTransform, HAPI_AssetId InAssetId, const HAPI_ObjectInfo& ObjectInfo,
		const HAPI_GeoInfo& GeoInfo, const HAPI_PartInfo& PartInfo);

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

	/** Return part id. **/
	HAPI_PartId GetPartId() const;

public:

	/** Return attribute objects associated with this geo part object. **/
	bool HapiGetAttributeObjects(HAPI_AttributeOwner AttributeOwner,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjects) const;
	bool HapiGetAttributeObjects(HAPI_AssetId OtherAssetId, HAPI_AttributeOwner AttributeOwner,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjects) const;
	bool HapiGetAllAttributeObjects(TMap<FString, FHoudiniAttributeObject>& AttributeObjectsPoint,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsVertex,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsPrimitive,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsDetail) const;
	bool HapiGetAllAttributeObjects(HAPI_AssetId OtherAssetId,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsPoint,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsVertex,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsPrimitive,
		TMap<FString, FHoudiniAttributeObject>& AttributeObjectsDetail) const;

/** HAPI: Other helpers. **/
public:

	/** HAPI: Return true if given attribute exists on a given owner. **/
	bool HapiCheckAttributeExistance(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(const std::string& AttributeName, HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeOwner AttributeOwner) const;
	bool HapiCheckAttributeExistance(const char* AttributeName, HAPI_AttributeOwner AttributeOwner) const;

	/** HAPI: Get instance transformations. **/
	bool HapiGetInstanceTransforms(HAPI_AssetId OtherAssetId, TArray<FTransform>& AllTransforms) const;
	bool HapiGetInstanceTransforms(TArray<FTransform>& AllTransforms) const;

	/** HAPI: Get Attribute info on a specified owner. **/
	bool HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(const char* AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(const std::string& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(const FString& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo) const;

	/** HAPI: Get Attribute info on any owner. **/
	bool HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeInfo& AttributeInfo)  const;
	bool HapiGetAttributeInfo(const char* AttributeName, HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(const std::string& AttributeName, HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeInfo& AttributeInfo) const;
	bool HapiGetAttributeInfo(const FString& AttributeName, HAPI_AttributeInfo& AttributeInfo) const;

	/** HAPI: Get attribute float data on a specified owner. **/
	bool HapiGetAttributeDataAsFloat(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(const char* AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData,
		int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(const std::string& AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData,
		int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData, int32 TupleSize = 0) const;

	/** HAPI: Get attribute float data on any owner. **/
	bool HapiGetAttributeDataAsFloat(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(const char* AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(const std::string& AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsFloat(const FString& AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<float>& AttributeData, int32 TupleSize = 0) const;

	/** HAPI: Get attribute int data on a specified owner. **/
	bool HapiGetAttributeDataAsInt(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(const char* AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData,
		int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(const std::string& AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData,
		int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData, int32 TupleSize = 0) const;

	/** HAPI: Get attribute int data on any owner. **/
	bool HapiGetAttributeDataAsInt(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(const char* AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(const std::string& AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsInt(const FString& AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<int32>& AttributeData, int32 TupleSize = 0) const;

	/** HAPI: Get attribute string data on a specified owner. **/
	bool HapiGetAttributeDataAsString(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(const char* AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData,
		int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(const std::string& AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData,
		int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData, int32 TupleSize = 0) const;

	/** HAPI: Get attribute string data on any owner. **/
	bool HapiGetAttributeDataAsString(HAPI_AssetId OtherAssetId, const char* AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(const char* AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(const std::string& AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(HAPI_AssetId OtherAssetId, const FString& AttributeName,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& AttributeData, int32 TupleSize = 0) const;
	bool HapiGetAttributeDataAsString(const FString& AttributeName, HAPI_AttributeInfo& ResultAttributeInfo,
		TArray<FString>& AttributeData, int32 TupleSize = 0) const;

	/** HAPI: Get names of all attributes on all owners. **/
	bool HapiGetAllAttributeNames(HAPI_AssetId OtherAssetId, TArray<FString>& AttributeNames) const;
	bool HapiGetAllAttributeNames(TArray<FString>& AttributeNames) const;

	/** HAPI: Get names of all attributes on a given owner. **/
	bool HapiGetAttributeNames(HAPI_AssetId OtherAssetId, HAPI_AttributeOwner AttributeOwner,
		TArray<FString>& AttributeNames) const;
	bool HapiGetAttributeNames(HAPI_AttributeOwner AttributeOwner, TArray<FString>& AttributeNames) const;

	/** HAPI: Get attribute names on point, vertex, detail or primitive. **/
	bool HapiGetPointAttributeNames(HAPI_AssetId OtherAssetId, TArray<FString>& AttributeNames) const;
	bool HapiGetPointAttributeNames(TArray<FString>& AttributeNames) const;
	bool HapiGetVertexAttributeNames(HAPI_AssetId OtherAssetId, TArray<FString>& AttributeNames) const;
	bool HapiGetVertexAttributeNames(TArray<FString>& AttributeNames) const;
	bool HapiGetPrimitiveAttributeNames(HAPI_AssetId OtherAssetId, TArray<FString>& AttributeNames) const;
	bool HapiGetPrimitiveAttributeNames(TArray<FString>& AttributeNames) const;
	bool HapiGetDetailAttributeNames(HAPI_AssetId OtherAssetId, TArray<FString>& AttributeNames) const;
	bool HapiGetDetailAttributeNames(TArray<FString>& AttributeNames) const;

/** HAPI: Object related getters. **/
public:

	/** HAPI: Retrieve corresponding object info structure. **/
	bool HapiObjectGetInfo(HAPI_ObjectInfo& ObjectInfo) const;
	bool HapiObjectGetInfo(HAPI_AssetId OtherAssetId, HAPI_ObjectInfo& ObjectInfo) const;

	/** HAPI: If this is an instancer, return id of instanced object. Return -1 if no such object is found. **/
	HAPI_ObjectId HapiObjectGetToInstanceId() const;
	HAPI_ObjectId HapiObjectGetToInstanceId(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return object name. **/
	FHoudiniEngineString HapiObjectGetName() const;
	FHoudiniEngineString HapiObjectGetName(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return object instance path. **/
	FHoudiniEngineString HapiObjectGetInstancePath() const;
	FHoudiniEngineString HapiObjectGetInstancePath(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if object is visible. **/
	bool HapiObjectIsVisible() const;
	bool HapiObjectIsVisible(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if object is an instancer. **/
	bool HapiObjectIsInstancer() const;
	bool HapiObjectIsInstancer(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if object transform has changed. **/
	bool HapiObjectHasTransformChanged() const;
	bool HapiObjectHasTransformChanged(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if any of the underlying geos have changed. **/
	bool HapiObjectHaveGeosChanged() const;
	bool HapiObjectHaveGeosChanged(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Get number of geos. **/
	int32 HapiObjectGetGeoCount() const;
	int32 HapiObjectGetGeoCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Get associated node id. This corresponds to id of an obj node. **/
	HAPI_NodeId HapiObjectGetNodeId() const;
	HAPI_NodeId HapiObjectGetNodeId(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return unique material id associated with an instancer. **/
	bool HapiObjectGetUniqueInstancerMaterialId(HAPI_MaterialId& MaterialId) const;
	bool HapiObjectGetUniqueInstancerMaterialId(HAPI_AssetId OtherAssetId, HAPI_MaterialId& MaterialId) const;

/** HAPI: Geo related getters. **/
public:

	/** HAPI: Retrieve corresponding geo info structure. **/
	bool HapiGeoGetInfo(HAPI_GeoInfo& GeoInfo) const;
	bool HapiGeoGetInfo(HAPI_AssetId OtherAssetId, HAPI_GeoInfo& GeoInfo) const;

	/** HAPI: Return geo type. **/
	HAPI_GeoType HapiGeoGetType() const;
	HAPI_GeoType HapiGeoGetType(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return name of this geo. **/
	FHoudiniEngineString HapiGeoGetName() const;
	FHoudiniEngineString HapiGeoGetName(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return geo node id. This corresponds to id of a sop node. **/
	HAPI_NodeId HapiGeoGetNodeId() const;
	HAPI_NodeId HapiGeoGetNodeId(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if this geo is editable. **/
	bool HapiGeoIsEditable() const;
	bool HapiGeoIsEditable(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if this geo is templated. **/
	bool HapiGeoIsTemplated() const;
	bool HapiGeoIsTemplated(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if this is a display sop geo. **/
	bool HapiGeoIsDisplayGeo() const;
	bool HapiGeoIsDisplayGeo(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if geo has changed. **/
	bool HapiGeoHasChanged() const;
	bool HapiGeoHasChanged(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if material on this geo has changed. **/
	bool HapiGeoHasMaterialChanged() const;
	bool HapiGeoHasMaterialChanged(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return number of point groups. **/
	int32 HapiGeoGetPointGroupCount() const;
	int32 HapiGeoGetPointGroupCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return number of primitive groups. **/
	int32 HapiGeoGetPrimitiveGroupCount() const;
	int32 HapiGeoGetPrimitiveGroupCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return number of parts within this geo. **/
	int32 HapiGeoGetPartCount() const;
	int32 HapiGeoGetPartCount(HAPI_AssetId OtherAssetId) const;

/** HAPI: Part related getters. **/
public:

	/** HAPI: Retrieve corresponding part info structure. **/
	bool HapiPartGetInfo(HAPI_PartInfo& PartInfo) const;
	bool HapiPartGetInfo(HAPI_AssetId OtherAssetId, HAPI_PartInfo& PartInfo) const;

	/** HAPI: Get name of this part. **/
	FHoudiniEngineString HapiPartGetName() const;
	FHoudiniEngineString HapiPartGetName(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return part type. **/
	HAPI_PartType HapiPartGetType() const;
	HAPI_PartType HapiPartGetType(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return face count. **/
	int32 HapiPartGetFaceCount() const;
	int32 HapiPartGetFaceCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return vertex count. **/
	int32 HapiPartGetVertexCount() const;
	int32 HapiPartGetVertexCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return point count. **/
	int32 HapiPartGetPointCount() const;
	int32 HapiPartGetPointCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return true if this part is used by an instancer. **/
	bool HapiPartIsInstanced() const;
	bool HapiPartIsInstanced(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Number of parts this instancer part is instancing. **/
	int32 HapiPartGetInstancedPartCount() const;
	int32 HapiPartGetInstancedPartCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Number of instances that this instancer part is instancing. **/
	int32 HapiPartGetInstanceCount() const;
	int32 HapiPartGetInstanceCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Get number of point attributes. **/
	int32 HapiPartGetPointAttributeCount() const;
	int32 HapiPartGetPointAttributeCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Get number of vertex attributes. **/
	int32 HapiPartGetVertexAttributeCount() const;
	int32 HapiPartGetVertexAttributeCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Get number of primitive attributes. **/
	int32 HapiPartGetPrimitiveAttributeCount() const;
	int32 HapiPartGetPrimitiveAttributeCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Get number of detail attributes. **/
	int32 HapiPartGetDetailAttributeCount() const;
	int32 HapiPartGetDetailAttributeCount(HAPI_AssetId OtherAssetId) const;

	/** HAPI: Return unique material ids associated with this part. **/
	bool HapiPartGetUniqueMaterialIds(TSet<HAPI_MaterialId>& MaterialIds) const;
	bool HapiPartGetUniqueMaterialIds(HAPI_AssetId OtherAssetId, TSet<HAPI_MaterialId>& MaterialIds) const;

public:

	/** Return list of vertices associated with this geo part object. **/
	bool HapiGetVertices(HAPI_AssetId OtherAssetId, TArray<int32>& Vertices) const;
	bool HapiGetVertices(TArray<int32>& Vertices) const;

public:

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
