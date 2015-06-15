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

#include "HoudiniGeoPartObject.h"


class FArchive;
class UTexture2D;
class UStaticMesh;
class UHoudiniAsset;
class AHoudiniAssetActor;
class UHoudiniAssetMaterial;
class UHoudiniAssetComponent;
class FHoudiniAssetObjectGeo;

struct FRawMesh;


struct HOUDINIENGINERUNTIME_API FHoudiniEngineUtils
{
public:

	/** Return a string description of error from a given error code. **/
	static const FString GetErrorDescription(HAPI_Result Result);

	/** Return a string error description. **/
	static const FString GetErrorDescription();

	/** Return a string indicating cook state. **/
	static const FString GetCookState();

	/** Return a string representing cooking result. **/
	static const FString GetCookResult();

	/** Return true if module has been properly initialized. **/
	static bool IsInitialized();

	/** Return necessary buffer size to store preset information for a given asset. **/
	static bool ComputeAssetPresetBufferLength(HAPI_AssetId AssetId, int32& OutBufferLength);

	/** Sets preset data for a given asset. **/
	static bool SetAssetPreset(HAPI_AssetId AssetId, const TArray<char>& PresetBuffer);

	/** Gets preset data for a given asset. **/
	static bool GetAssetPreset(HAPI_AssetId AssetId, TArray<char>& PresetBuffer);

	/** Return true if asset is valid. **/
	static bool IsHoudiniAssetValid(HAPI_AssetId AssetId);

	/** Destroy asset, returns the status. **/
	static bool DestroyHoudiniAsset(HAPI_AssetId AssetId);

	/** Return specified string. **/
	static bool GetHoudiniString(int32 Name, FString& NameString);

	/** HAPI : Get string name for a given handle. **/
	static bool GetHoudiniString(int32 Name, std::string& NameString);

	/** HAPI : Convert Unreal string to ascii one. **/
	static void ConvertUnrealString(const FString& UnrealString, std::string& String);

	/** HAPI : Translate HAPI transform to Unreal one. **/
	static void TranslateHapiTransform(const HAPI_Transform& HapiTransform, FTransform& UnrealTransform);

	/** HAPI : Translate HAPI Euler transform to Unreal one. **/
	static void TranslateHapiTransform(const HAPI_TransformEuler& HapiTransformEuler, FTransform& UnrealTransform);

	/** HAPI : Translate Unreal transform to HAPI one. **/
	static void TranslateUnrealTransform(const FTransform& UnrealTransform, HAPI_Transform& HapiTransform);

	/** HAPI : Translate Unreal transform to HAPI Euler one. **/
	static void TranslateUnrealTransform(const FTransform& UnrealTransform, HAPI_TransformEuler& HapiTransformEuler);

	/** Return name of Houdini asset. **/
	static bool GetHoudiniAssetName(HAPI_AssetId AssetId, FString& NameString);

	/** Construct static meshes for a given Houdini asset. Flag controls whether one mesh or multiple meshes will be **/
	/** created.																									 **/
	static bool CreateStaticMeshesFromHoudiniAsset(UHoudiniAssetComponent* HoudiniAssetComponent, UPackage* Package,
		const TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesIn,
		TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesOut, FTransform& ComponentTransform);

	/** Bake static mesh. **/
	static UStaticMesh* BakeStaticMesh(UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniGeoPartObject& HoudiniGeoPartObject, UStaticMesh* StaticMesh);

	/** Bake single static mesh - this will combine individual objects into one, baking in transformations. **/
	//static UStaticMesh* BakeSingleStaticMesh(UHoudiniAssetComponent* HoudiniAssetComponent, TMap<UStaticMesh*, UStaticMeshComponent*>& StaticMeshComponents);

	/** Extract position information from coords string. **/
	static void ExtractStringPositions(const FString& Positions, TArray<FVector>& OutPositions);

	/** Create string containing positions from a given vector of positions. **/
	static void CreatePositionsString(const TArray<FVector>& Positions, FString& PositionString);

	/** Given raw positions incoming from HAPI, convert them to Unreal's FVector and perform necessary flipping and **/
	/** scaling.																									**/
	static void ConvertScaleAndFlipVectorData(const TArray<float>& DataRaw, TArray<FVector>& DataOut);

	/** Returns platform specific name of libHAPI. **/
	static FString HoudiniGetLibHAPIName();

	/** Load libHAPI and return handle to it, also store location of loaded libHAPI in passed argument. **/
	static void* LoadLibHAPI(FString& StoredLibHAPILocation);

	/** Helper function to count number of UV sets in raw mesh. **/
	static int32 CountUVSets(const FRawMesh& RawMesh);

	/** Helper function to extract copied Houdini actor from clipboard. **/
	static AHoudiniAssetActor* LocateClipboardActor();

public:

	/** HAPI : Return true if given asset id is valid. **/
	static bool IsValidAssetId(HAPI_AssetId AssetId);

	/** HAPI : Create curve for input. **/
	static bool HapiCreateCurve(HAPI_AssetId& CurveAssetId);

	/** HAPI : Retrieve Node id from given parameters. **/
	static bool HapiGetNodeId(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_NodeId& NodeId);

	/** HAPI : Marshaling, extract geometry and create input asset form it. Connect to given host asset and return	**/
	/** new asset id.																								**/
	static bool HapiCreateAndConnectAsset(HAPI_AssetId HostAssetId, int32 InputIndex, UStaticMesh* Mesh,
		HAPI_AssetId& ConnectedAssetId);

	/** HAPI : Marshaling, disconnect input asset from a given slot. **/
	static bool HapiDisconnectAsset(HAPI_AssetId HostAssetId, int32 InputIndex);

	/** HAPI : Marshaling, connect input asset to a given slot of host asset. **/
	static bool HapiConnectAsset(HAPI_AssetId AssetIdFrom, HAPI_ObjectId ObjectIdFrom, HAPI_AssetId AssetIdTo,
		int32 InputIndex);

	/** HAPI : Return all group names for a given Geo. **/
	static bool HapiGetGroupNames(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_GroupType GroupType, TArray<FString>& GroupNames);

	/** HAPI : Retrieve group membership. **/
	static bool HapiGetGroupMembership(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, HAPI_GroupType GroupType, const FString& GroupName, TArray<int32>& GroupMembership);

	/** HAPI : Get group count by type. **/
	static int32 HapiGetGroupCountByType(HAPI_GroupType GroupType, HAPI_GeoInfo& GeoInfo);

	/** HAPI : Get element count by group type. **/
	static int32 HapiGetElementCountByGroupType(HAPI_GroupType GroupType, HAPI_PartInfo& PartInfo);

	/** HAPI : Return group membership count. **/
	static int32 HapiCheckGroupMembership(const TArray<int32>& GroupMembership);

	/** HAPI : Check if given attribute exists. **/
	static bool HapiCheckAttributeExists(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, const char* Name, HAPI_AttributeOwner Owner);
	static bool HapiCheckAttributeExists(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
		HAPI_AttributeOwner Owner);

	/** HAPI : Get attribute data as float. **/
	static bool HapiGetAttributeDataAsFloat(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, const char* Name, HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& Data,
		int32 TupleSize = 0);

	static bool HapiGetAttributeDataAsFloat(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& Data, int32 TupleSize = 0);

	/** HAPI : Get attribute data as integer. **/
	static bool HapiGetAttributeDataAsInteger(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, const char* Name, HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& Data,
		int32 TupleSize = 0);

	static bool HapiGetAttributeDataAsInteger(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& Data, int32 TupleSize = 0);

	/** HAPI : Get attribute data as string. **/
	static bool HapiGetAttributeDataAsString(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, const char* Name, HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& Data,
		int32 TupleSize = 0);

	static bool HapiGetAttributeDataAsString(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
		HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& Data, int32 TupleSize = 0);

	/** HAPI : Get parameter data as float. **/
	static bool HapiGetParameterDataAsFloat(HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue,
		float& Value);

	/** HAPI : Get parameter data as integer. **/
	static bool HapiGetParameterDataAsInteger(HAPI_NodeId NodeId, const std::string ParmName, int32 DefaultValue,
		int32& Value);

	/** HAPI : Get parameter data as string. **/
	static bool HapiGetParameterDataAsString(HAPI_NodeId NodeId, const std::string ParmName,
		const FString& DefaultValue, FString& Value);

	/** HAPI : Retrieve parameter name. **/
	static void HapiRetrieveParameterName(const HAPI_ParmInfo& ParmInfo, FString& ParameterName);

	/** HAPI : Retrieve names of all parameters. **/
	static void HapiRetrieveParameterNames(const std::vector<HAPI_ParmInfo>& ParmInfos,
		std::vector<std::string>& Names);

	/** HAPI : Look for parameter by name and return its index. Return -1 if not found. **/
	static int32 HapiFindParameterByName(const std::string& ParmName, const std::vector<std::string>& Names);

	/** HAPI : Extract image data. **/
	static bool HapiExtractImage(HAPI_ParmId NodeParmId, const HAPI_MaterialInfo& MaterialInfo,
		TArray<char>& ImageBuffer, const char* Type = HAPI_UNREAL_MATERIAL_TEXTURE_MAIN);

	/** HAPI : Return true if given material is transparent. **/
	static bool HapiIsMaterialTransparent(const HAPI_MaterialInfo& MaterialInfo);

	/** HAPI : Create Unreal material and necessary textures. **/
	static UMaterial* HapiCreateMaterial(const HAPI_MaterialInfo& MaterialInfo, UPackage* Package,
		const FString& MeshName, const FRawMesh& RawMesh);

	/** HAPI : Retrieve instance transforms for a specified geo object. **/
	static bool HapiGetInstanceTransforms(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, TArray<FTransform>& Transforms);

	static bool HapiGetInstanceTransforms(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
		TArray<FTransform>& Transforms);

	/** HAPI : Given vertex list, retrieve new vertex list for a scpecified group.									**/
	/** Return number of processed valid index vertices for this split.												**/
	static int32 HapiGetVertexListForGroup(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
		HAPI_PartId PartId, const FString& GroupName, const TArray<int32>& FullVertexList,
		TArray<int32>& NewVertexList, TArray<int32>& AllVertexList);

protected:

	/** Create a package for static mesh. **/
	static UPackage* BakeCreatePackageForStaticMesh(UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniGeoPartObject& HoudiniGeoPartObject, UPackage* Package, FString& MeshName, FGuid& BakeGUID,
		bool bBake = false);

	/** Helper routine to serialize Material interface. **/
	static void Serialize(UMaterialInterface*& MaterialInterface, UPackage* Package, FArchive& Ar);

	/** Helper function to extract colors and store them in a given RawMesh. Returns number of wedges. **/
	static int32 TransferRegularPointAttributesToVertices(const TArray<int32>& VertexList,
		const HAPI_AttributeInfo& AttribInfo, TArray<float>& Data);

#if WITH_EDITOR

	/** Helper routine to check if Raw Mesh contains degenerate triangles. **/
	static bool ContainsDegenerateTriangles(const FRawMesh& RawMesh);

	/** Helper routine to count number of degenerate triangles. **/
	static int32 CountDegenerateTriangles(const FRawMesh& RawMesh);

	/** Helper routine to check invalid lightmap faces. **/
	static bool ContainsInvalidLightmapFaces(const FRawMesh& RawMesh, int32 LightmapSourceIdx);

#endif

	/** Helper function to extract a material name from given material interface. **/
	static char* ExtractMaterialName(UMaterialInterface* MaterialInterface);

	/** Create helper array of material names, we use it for marshalling. **/
	static void CreateFaceMaterialArray(const TArray<UMaterialInterface*>& Materials,
		const TArray<int32>& FaceMaterialIndices, TArray<char*>& OutStaticMeshFaceMaterials);

	/** Delete helper array of material names. **/
	static void DeleteFaceMaterialArray(TArray<char*>& OutStaticMeshFaceMaterials);

	/** Return a specified HAPI status string. **/
	static const FString GetStatusString(HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity);

protected:

#if WITH_EDITOR

	/** Create a texture from given information. **/
	static UTexture2D* CreateUnrealTexture(const HAPI_ImageInfo& ImageInfo, UPackage* Package,
		const FString& TextureName, EPixelFormat PixelFormat, const TArray<char>& ImageBuffer);

	/** Reset streams used by the given RawMesh. **/
	static void ResetRawMesh(FRawMesh& RawMesh);

#endif

public:

	/** Geometry scale values. **/
	static const float ScaleFactorPosition;
	static const float ScaleFactorTranslate;
};
