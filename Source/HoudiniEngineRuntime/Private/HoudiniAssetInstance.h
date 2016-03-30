/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Represents a Houdini Asset in the UE4 Content Browser.
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
#include "HoudiniAssetInstance.generated.h"


class UHoudiniAsset;
class UHoudiniAssetInput;
class FHoudiniEngineString;
class UHoudiniAssetParameter;
struct FHoudiniGeoPartObject;


UCLASS(EditInlineNew, config=Engine)
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstance : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UHoudiniAssetInstance();

public:

	/** Create an asset instance. **/
	static UHoudiniAssetInstance* CreateAssetInstance(UObject* Outer, UHoudiniAsset* HoudiniAsset);

public:

	/** Return current referenced Houdini asset. **/
	UHoudiniAsset* GetHoudiniAsset() const;

	/** Return true if asset id is valid. **/
	bool IsValidAssetInstance() const;

	/** Return number of times this asset has been cooked. **/
	int32 GetAssetCookCount() const;

public:

	/** Instantiate the asset synchronously. **/
	bool InstantiateAsset(const FHoudiniEngineString& AssetNameToInstantiate, bool* bInstantiatedWithErrors = nullptr);
	bool InstantiateAsset(bool* bInstantiatedWithErrors = nullptr);

	/** Cook asset synchronously. **/
	bool CookAsset(bool* bCookedWithErrors = nullptr);

	/** Delete asset synchronously. **/
	bool DeleteAsset();

public:

	/** Instantiate the asset asynchronously, through the scheduler. **/
	bool InstantiateAssetAsync();

	/** Cook the asset asynchronously, through the scheduler. **/
	bool CookAssetAsync();

	/** Return true if asset is being asynchronously instantiated or cooked. **/
	bool IsAssetBeingAsyncInstantiatedOrCooked() const;

	/** Check if the asset has finished asynchronous instantiation. **/
	bool HasAssetFinishedAsyncInstantiation(bool* bInstantiatedWithErrors = nullptr) const;

	/** Check if the asset has finished asynchronous cooking. **/
	bool HasAssetFinishedAsyncCooking(bool* bCookedWithErrors = nullptr) const;

	/** Delete asset asynchronously. **/
	bool DeleteAssetAsync();

public:

	/** Retrieve list of geo part objects. **/
	bool GetGeoPartObjects(TArray<FHoudiniGeoPartObject>& GeoPartObjects) const;

public:

	/** Retrieve asset transform. **/
	bool HapiGetAssetTransform(FTransform& Transform) const;

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	virtual void FinishDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** Called after successful instantiation. **/
	void PostInstantiateAsset();

	/** Called after successful cook. **/
	void PostCookAsset();

protected:

	/** Retrieve node id of this asset. **/
	HAPI_NodeId HapiGetNodeId() const;

	/** Retrieve asset info structure. **/
	bool HapiGetAssetInfo(HAPI_AssetInfo& AssetInfo) const;

	/** Retrieve all object info structures. **/
	bool HapiGetObjectInfos(TArray<HAPI_ObjectInfo>& ObjectInfos) const;

	/** Retrieve all object transforms. **/
	bool HapiGetObjectTransforms(TArray<FTransform>& ObjectTransforms) const;

	/** Retrieve geo info structure. **/
	bool HapiGetGeoInfo(HAPI_ObjectId ObjectId, int32 GeoIdx, HAPI_GeoInfo& GeoInfo) const;

	/** Retrieve part info structure. **/
	bool HapiGetPartInfo(HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, int32 PartIdx, HAPI_PartInfo& PartInfo) const;

	/** Get asset preset. **/
	bool HapiGetAssetPreset(TArray<char>& PresetBuffer) const;

	/** Set asset preset. **/
	bool HapiSetAssetPreset(const TArray<char>& PresetBuffer) const;

protected:

	/** Corresponding Houdini asset. **/
	UHoudiniAsset* HoudiniAsset;

	/** Name of the instantiated asset. **/
	FString InstantiatedAssetName;

	/** Id of instantiated asset. **/
	volatile HAPI_AssetId AssetId;

	/** Number of times this asset has been cooked. **/
	volatile int32 AssetCookCount;

	/** Is set to true while asset is being asynchronously instantiated or cooked. **/
	volatile int32 bIsAssetBeingAsyncInstantiatedOrCooked;

protected:

	/** Inputs of this asset. **/
	//TArray<UHoudiniAssetInput*> Inputs;

	/** Parameters of this asset. **/
	TMap<FString, UHoudiniAssetParameter*> Parameters;

	/** Buffer to hold default preset. **/
	TArray<char> DefaultPresetBuffer;

protected:

	/** Flags used by this object. **/
	uint32 HoudiniAssetInstanceFlagsPacked;

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniAssetInstanceVersion;
};
