/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Instance of an asset.
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

UCLASS(Blueprintable, BlueprintType, EditInlineNew, config=Editor)
class HOUDINIENGINE_API UHoudiniAssetInstance : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Return true if this asset is initialized. **/
	bool IsInitialized() const;

	/** Return internal asset id. **/
	HAPI_AssetId GetAssetId() const;

	/** Set internal asset id. **/
	void SetAssetId(HAPI_AssetId InAssetId);

	/** Set Houdini asset. **/
	void SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset);

	/** Return Houdini asset. **/
	UHoudiniAsset* GetHoudiniAsset() const;

	/** Return asset name. **/
	const FString GetAssetName() const;

	/** Set asset name. **/
	void SetAssetName(const FString& Name);

protected:

	/** Holds Houdini asset we are instantiating. **/
	UHoudiniAsset* HoudiniAsset;

	/* Synchronization primitive used to control access. **/
	mutable FCriticalSection CriticalSection;

	/** Holds asset name. **/
	FString AssetName;

	/** Holds internal asset handle. **/
	HAPI_AssetId volatile AssetId;
};
