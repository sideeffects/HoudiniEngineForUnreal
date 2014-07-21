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

class UHoudiniAssetObjectGeo;

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

	/** Construct Houdini geos from a specified asset. **/
	static bool ConstructGeos(HAPI_AssetId AssetId, TArray<FHoudiniAssetObjectGeo*>& HoudiniAssetObjectGeos);

	/** Extract geometry information for a given asset. **/
	static bool GetAssetGeometry(HAPI_AssetId AssetId, TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds);

	/** Extract Houdini logo temporary geometry. **/
	static void GetHoudiniLogoGeometry(TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds);

	/** Compute bounding volume information from triangle data. **/
	static void GetBoundingVolume(const TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds);

	/** Convert Houdini color to Unreal FColor and return number of channels. **/
	static int ConvertHoudiniColorRGB(const float* HoudiniColorRGB, FColor& UnrealColor);
	static int ConvertHoudiniColorRGBA(const float* HoudiniColorRGBA, FColor& UnrealColor);

	/** Convert Unreal FColor to Houdini color and return number of channels. **/
	static int ConvertUnrealColorRGB(const FColor& UnrealColor, float* HoudiniColorRGB);
	static int ConvertUnrealColorRGBA(const FColor& UnrealColor, float* HoudiniColorRGBA);

public:

	/** HAPI : Return all group names for a given Geo. **/
	static bool HapiGetGroupNames(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_GroupType GroupType,
								  std::vector<std::string>& GroupNames);

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
											std::vector<float>& Data, int TupleSize = 0);

protected:

	/** Given current min and max extent vectors, update them from given position if necessary. **/
	static void UpdateBoundingVolumeExtent(const FVector& Vector, FVector& ExtentMin, FVector& ExtentMax);

	/** Transform position of a given triangle using a given transformation matrix. **/
	static void TransformPosition(const FMatrix& TransformMatrix, FHoudiniMeshTriangle& Triangle);
};
