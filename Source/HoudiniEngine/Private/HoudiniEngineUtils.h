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

	/** Construct Houdini Logo geo. **/
	static FHoudiniAssetObjectGeo* ConstructLogoGeo();

	/** Construct Houdini geos from a specified asset. **/
	static bool ConstructGeos(HAPI_AssetId AssetId, UPackage* Package, TArray<FHoudiniAssetObjectGeo*>& PreviousObjectGeos, TArray<FHoudiniAssetObjectGeo*>& NewObjectGeos);

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

	/** HAPI : Check if Unreal specific packed tangent attribute exists. **/
	static bool HapiCheckAttributeExistsPackedTangent(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId);

	/** HAPI : Get attribute data as float. **/
	static bool HapiGetAttributeDataAsFloat(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											std::vector<float>& Data, int TupleSize = 0);

	static bool HapiGetAttributeDataAsFloat(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											TArray<float>& Data, int TupleSize = 0);

	/** HAPI : Get attribute data as integer. **/
	static bool HapiGetAttributeDataAsInteger(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											std::vector<int>& Data, int TupleSize = 0);

	static bool HapiGetAttributeDataAsInteger(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											TArray<int>& Data, int TupleSize = 0);

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

public:

	/** Given a collection of geometry, create a single static mesh. **/
	static void CreateStaticMeshes(UHoudiniAsset* HoudiniAsset, const TArray<FHoudiniAssetObjectGeo*>& ObjectGeos, 
								   TArray<UStaticMesh*>& StaticMeshes, bool bSplit = false);

protected:

	/** Create a package for static mesh. **/
	static UPackage* BakeCreatePackageForStaticMesh(UHoudiniAsset* HoudiniAsset, UPackage* Package, FString& MeshName, FGuid& BakeGUID, int32 ObjectIdx = -1);

protected:

	/** Helper function which extracts texture coordinates. **/
	static bool ExtractTextureCoordinates(FHoudiniMeshVertex* Vertices, int TriangleIdx, const std::vector<int>& VertexList,
										  const HAPI_AttributeInfo& AttribInfo, const std::vector<float>& Data, int Channel);

	/** Helper function which extracts normals. **/
	static bool ExtractNormals(FHoudiniMeshVertex* Vertices, int TriangleIdx, const std::vector<int>& VertexList,
							   const HAPI_AttributeInfo& AttribInfo, const std::vector<float>& Data);

	/** Helper function which extracts colors. **/
	static bool ExtractColors(FHoudiniMeshVertex* Vertices, int TriangleIdx, const std::vector<int>& VertexList,
							   const HAPI_AttributeInfo& AttribInfo, const std::vector<float>& Data);

	/** Helper function to compute tangents. **/
	static bool ComputePackedTangents(FHoudiniMeshVertex* Vertices);

	/** Helper function to compute Unreal specific packed tangents. **/
	static void ComputePackedTangents(const TArray<FVector>& Positions, const TArray<int32>& Indices,
									  TArray<FPackedNormal>& TangentXs, TArray<FPackedNormal>& TangentZs);

	/** Helper function which extracts Unreal specific packed tangents. **/
	static bool ExtractPackedTangents(FHoudiniMeshVertex* Vertices, int TriangleIdx, const std::vector<int>& VertexList,
									  const HAPI_AttributeInfo& AttribInfo, const std::vector<int>& Data, int Channel);

protected:

	/** Given current min and max extent vectors, update them from given position if necessary. **/
	static void UpdateBoundingVolumeExtent(const FVector& Vector, FVector& ExtentMin, FVector& ExtentMax);

	/** Transform position of a given triangle using a given transformation matrix. **/
	static void TransformPosition(const FMatrix& TransformMatrix, FHoudiniMeshVertex* Vertices);

	/** Create a texture from given information. **/
	static UTexture2D* CreateUnrealTexture(const HAPI_ImageInfo& ImageInfo, EPixelFormat PixelFormat, const std::vector<char>& ImageBuffer);

	/** Create cache maps. These are used to avoid geometry and material reconstruction when nothing has changed **/
	/** between the cooks.                                                                                       **/
	static void CreateCachedMaps(const TArray<FHoudiniAssetObjectGeo*>& PreviousObjectGeos,
								 TMap<HAPI_ObjectId, TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> > >& CachedObjectMap);

	/** Look up cached Houdini geo object based on HAPI object id, geo id and part id. **/
	static FHoudiniAssetObjectGeo* RetrieveCachedHoudiniObjectGeo(HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
																  const TMap<HAPI_ObjectId, TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> > >& CMap);

protected:

	/** Geometry scale values. **/
	static const float ScaleFactorPosition;
	static const float ScaleFactorTranslate;
};
