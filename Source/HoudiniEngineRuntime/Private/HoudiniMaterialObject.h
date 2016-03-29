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


class FString;
class UPackage;
struct HAPI_NodeInfo;
struct HAPI_ParmInfo;
struct HAPI_MaterialInfo;
class UHoudiniAssetComponent;
struct FHoudiniParameterObject;


struct HOUDINIENGINERUNTIME_API FHoudiniMaterialObject
{
public:

	FHoudiniMaterialObject();
	FHoudiniMaterialObject(const HAPI_MaterialInfo& MaterialInfo);
	FHoudiniMaterialObject(HAPI_AssetId InAssetId, HAPI_NodeId InNodeId, HAPI_MaterialId InMaterialId);
	FHoudiniMaterialObject(const FHoudiniMaterialObject& HoudiniMaterialObject);

public:

	/** Retrieve corresponding node info structure. **/
	bool HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const;

	/** Retrieve corresponding material info structure. **/
	bool HapiGetMaterialInfo(HAPI_MaterialInfo& MaterialInfo) const;

	/** Return parameter objects associated with this material. **/
	bool HapiGetParameterObjects(TArray<FHoudiniParameterObject>& ParameterObjects) const;

	/** Locate parameter object with a specified name **/
	bool HapiLocateParameterByName(const FString& Name, FHoudiniParameterObject& ResultHoudiniParameterObject) const;

	/** Locate parameter object with a specified label. **/
	bool HapiLocateParameterByLabel(const FString& Label, FHoudiniParameterObject& ResultHoudiniParameterObject) const;

public:

	/** Return true if material exists. **/
	bool HapiCheckMaterialExists() const;

	/** Return true if material has been marked as modified. **/
	bool HapiCheckMaterialChanged() const;

public:

	/** Return material SHOP name. **/
	bool HapiGetMaterialShopName(FString& ShopName) const;

	/** Return true if material is transparent. **/
	bool HapiIsMaterialTransparent() const;

public:

	/** Return true if this is a Substance material. **/
	bool IsSubstance() const;

public:

	/** Create a package for this material. **/
	UPackage* CreateMaterialPackage(UHoudiniAssetComponent* HoudiniAssetComponent, FString& MaterialName,
		bool bBake = false);

protected:

	/** Asset Id associated with this material. **/
	HAPI_AssetId AssetId;

	/** Node Id associated with this material. **/
	HAPI_NodeId NodeId;

	/** Material Id associated with this material. **/
	HAPI_MaterialId MaterialId;

protected:

	/** Flags used by material object. **/
	uint32 HoudiniMaterialObjectFlagsPacked;

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniMaterialObjectVersion;
};
