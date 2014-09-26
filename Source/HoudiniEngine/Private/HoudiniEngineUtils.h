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

class UTexture2D;
class UHoudiniAsset;
class UStaticMesh;
class FArchive;
struct FRawMesh;
class UHoudiniAssetMaterial;
class FHoudiniAssetObjectGeo;

struct FHoudiniEngineUtils
{
public:

	/** Return a string description of error from a given error code. **/
	static const FString GetErrorDescription(HAPI_Result Result);

	/** Return a string error description. **/
	static const FString GetErrorDescription();

	/** Return true if module has been properly initialized. **/
	static bool IsInitialized();

	/** Return true if asset is valid. **/
	static bool IsHoudiniAssetValid(HAPI_AssetId AssetId);

	/** Destroy asset, returns the status. **/
	static bool DestroyHoudiniAsset(HAPI_AssetId AssetId);

	/** Return specified string. **/
	static bool GetHoudiniString(int Name, FString& NameString);

	/** Return name of Houdini asset. **/
	static bool GetHoudiniAssetName(HAPI_AssetId AssetId, FString& NameString);

	/** Convert Houdini color to Unreal FColor and return number of channels. **/
	static int ConvertHoudiniColorRGB(const float* HoudiniColorRGB, FColor& UnrealColor);
	static int ConvertHoudiniColorRGBA(const float* HoudiniColorRGBA, FColor& UnrealColor);

	/** Convert Unreal FColor to Houdini color and return number of channels. **/
	static int ConvertUnrealColorRGB(const FColor& UnrealColor, float* HoudiniColorRGB);
	static int ConvertUnrealColorRGBA(const FColor& UnrealColor, float* HoudiniColorRGBA);

	/** Construct static mesh which represents Houdini logo geometry. **/
	static UStaticMesh* CreateStaticMeshHoudiniLogo();

	/** Construct static meshes for a given Houdini asset. Flag controls whether one mesh or multiple meshes will be created. **/
	static bool CreateStaticMeshesFromHoudiniAsset(HAPI_AssetId AssetId, UHoudiniAsset* HoudiniAsset, UPackage* Package, 
												   TArray<UStaticMesh*>& StaticMeshes, bool bSplit = false);

	static bool CreateStaticMeshesFromHoudiniAsset(HAPI_AssetId AssetId, UHoudiniAsset* HoudiniAsset, UPackage* Package, 
												   const TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesIn,
												   TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesOut);

	/** Save a given static mesh in raw form. **/
	static void SaveRawStaticMesh(UStaticMesh* StaticMesh, FArchive& Ar);

	/** Load raw mesh from archive and create static mesh from it. **/
	static UStaticMesh* LoadRawStaticMesh(UHoudiniAsset* HoudiniAsset, UPackage* Package, int32 MeshCounter, FArchive& Ar);

	/** Bake static mesh. **/
	static UStaticMesh* BakeStaticMesh(UHoudiniAsset* HoudiniAsset, UStaticMesh* StaticMesh, int32 MeshCounter);

public:

	/** HAPI : Return true if given asset id is valid. **/
	static bool IsValidAssetId(HAPI_AssetId AssetId);

	/** HAPI : Marshaling, extract geometry and create input asset form it. Connect to given host asset and return new asset id. **/
	static bool HapiCreateAndConnectAsset(HAPI_AssetId HostAssetId, int InputIndex, UStaticMesh* Mesh, HAPI_AssetId& ConnectedAssetId);

	/** HAPI : Marshaling, disconnect given host asset and return new asset id. **/
	static bool HapiDisconnectAsset(HAPI_AssetId HostAssetId, int InputIndex);

	/** HAPI : Return all group names for a given Geo. **/
	static bool HapiGetGroupNames(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_GroupType GroupType,
								  std::vector<std::string>& GroupNames);

	/** HAPI : Return all group names for a given Geo. **/
	static bool HapiGetGroupNames(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_GroupType GroupType,
								  TArray<FString>& GroupNames);

	/** HAPI : Get string name for a given handle. **/
	static bool HapiGetString(int Name, std::string& NameString);

	/** HAPI : Retrieve group membership. **/
	static bool HapiGetGroupMembership(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
									   HAPI_GroupType GroupType, std::string GroupName, std::vector<int>& GroupMembership);

	/** HAPI : Get group count by type. **/
	static int HapiGetGroupCountByType(HAPI_GroupType GroupType, HAPI_GeoInfo& GeoInfo);

	/** HAPI : Get element count by group type. **/
	static int HapiGetElementCountByGroupType(HAPI_GroupType GroupType, HAPI_PartInfo& PartInfo);

	/** HAPI : Return group membership count. **/
	static int HapiCheckGroupMembership(const std::vector<int>& GroupMembership);

	/** HAPI : Check if given attribute exists. **/
	static bool HapiCheckAttributeExists(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
										 const char* Name, HAPI_AttributeOwner Owner);

	/** HAPI : Get attribute data as float. **/
	static bool HapiGetAttributeDataAsFloat(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											TArray<float>& Data, int TupleSize = 0);

	/** HAPI : Get attribute data as integer. **/
	static bool HapiGetAttributeDataAsInteger(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											TArray<int>& Data, int TupleSize = 0);

	/** HAPI : Get attribute data as string. **/
	static bool HapiGetAttributeDataAsString(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											TArray<FString>& Data, int TupleSize = 0);

	/** HAPI : Get parameter data as float. **/
	static float HapiGetParameterDataAsFloat(HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue);

	/** HAPI : Retrieve names of all parameters. **/
	static void HapiRetrieveParameterNames(const std::vector<HAPI_ParmInfo>& ParmInfos, std::vector<std::string>& Names);

	/** HAPI : Look for parameter by name and return its index. Return -1 if not found. **/
	static int HapiFindParameterByName(const std::string& ParmName, const std::vector<std::string>& Names);

	/** HAPI : Extract image data. **/
	static bool HapiExtractImage(HAPI_ParmId NodeParmId, const HAPI_MaterialInfo& MaterialInfo, std::vector<char>& ImageBuffer, const std::string Type = "C A");

	/** HAPI : Return true if given material is transparent. **/
	static bool HapiIsMaterialTransparent(const HAPI_MaterialInfo& MaterialInfo);

	/** HAPI : Create Unreal material and necessary textures. **/
	static UMaterial* HapiCreateMaterial(const HAPI_MaterialInfo& MaterialInfo, UPackage* Package, const FString& MeshName);

protected:

	/** Create a package for static mesh. **/
	static UPackage* BakeCreatePackageForStaticMesh(UHoudiniAsset* HoudiniAsset, UPackage* Package, FString& MeshName, FGuid& BakeGUID, int32 ObjectIdx = -1);

	/** Helper routine to serialize FRawMesh. **/
	static void Serialize(FRawMesh& RawMesh, FArchive& Ar);

	/** Helper function to extract colors and store them in a given RawMesh. **/
	static void TransferRegularPointAttributesToVertices(const TArray<int32>& VertexList, const HAPI_AttributeInfo& AttribInfo, TArray<float>& Data);

	/** Helper routine to check if Raw Mesh contains degenerate triangles. **/
	static bool ContainsDegenerateTriangles(const FRawMesh& RawMesh);

	/** Helper function to extract a material name from given material interface. **/
	static char* ExtractMaterialName(UMaterialInterface* MaterialInterface);

	/** Create helper array of material names, we use it for marshalling. **/
	static void CreateFaceMaterialArray(const TArray<UMaterialInterface*>& Materials, const TArray<int32>& FaceMaterialIndices,
											TArray<char*>& OutStaticMeshFaceMaterials);

	/** Delete helper array of material names. **/
	static void DeleteFaceMaterialArray(TArray<char*>& OutStaticMeshFaceMaterials);

protected:

	/** Create a texture from given information. **/
	static UTexture2D* CreateUnrealTexture(const HAPI_ImageInfo& ImageInfo, EPixelFormat PixelFormat, const std::vector<char>& ImageBuffer);

protected:

	/** Geometry scale values. **/
	static const float ScaleFactorPosition;
	static const float ScaleFactorTranslate;
};
