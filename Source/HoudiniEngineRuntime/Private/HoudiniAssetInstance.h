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
class UHoudiniAssetParameter;
struct FHoudiniGeoPartObject;
class FHoudiniEngineString;


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
	bool HasValidAssetId() const;

	/** Return number of times this asset has been cooked. **/
	int32 GetAssetCookCount() const;

public:

	/** Instantiate the asset synchronously. **/
	bool Instantiate(const FHoudiniEngineString& AssetNameToInstantiate, bool* bInstantiatedWithErrors = nullptr);
	bool Instantiate(bool* bInstantiatedWithErrors = nullptr);

	/** Cook asset synchronously. **/
	bool Cook(bool* bCookedWithErrors = nullptr);

public:

	/** Instantiate the asset asynchronously, through the scheduler. **/
	bool InstantiateAsync();

	/** Cook the asset asynchronously, through the scheduler. **/
	bool CookAsync();

	/** Return true if asset is being asynchronously instantiated or cooked. **/
	int32 IsBeingAsyncInstantiatedOrCooked() const;

	/** Check if the asset has finished asynchronous instantiation. **/
	bool IsFinishedAsyncInstantiation(bool* bInstantiatedWithErrors = nullptr) const;

	/** Check if the asset has finished asynchronous cooking. **/
	bool IsFinishedAsyncCooking(bool* bCookedWithErrors = nullptr) const;

public:

	/** Retrieve list of geo part objects. **/
	bool GetGeoPartObjects(TArray<FHoudiniGeoPartObject>& GeoPartObjects) const;

	/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	//virtual void FinishDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** Corresponding Houdini asset. **/
	UHoudiniAsset* HoudiniAsset;

	/** Name of the instantiated asset. **/
	FString InstantiatedAssetName;

	/** Id of instantiated asset. **/
	HAPI_AssetId AssetId;

	/** Number of times this asset has been cooked. **/
	int32 AssetCookCount;

	/** Is set to true while asset is being asynchronously instantiated or cooked. **/
	volatile int32 bIsBeingAsyncInstantiatedOrCooked;

protected:

	/** Parameters of this asset. **/
	TMap<FString, UHoudiniAssetParameter*> Parameters;

protected:

	/** Flags used by this object. **/
	uint32 HoudiniAssetInstanceFlagsPacked;

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniAssetInstanceVersion;
};
