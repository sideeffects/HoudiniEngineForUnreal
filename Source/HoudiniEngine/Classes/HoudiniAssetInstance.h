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
	friend class UHoudiniAssetInstanceManager;

	GENERATED_UCLASS_BODY()

public:

	/** Return true if this asset is initialized. **/
	bool IsInitialized() const;

	/** Set cooking status - that is, the asset is in the process of being cooked. **/
	void SetCooking(bool bCooking);

	/** Return cooking status. **/
	bool IsCooking() const;

	/** Set this asset as cooked. **/
	void SetCooked(bool bCooked);

	/** Return true if this asset has been cooked. */
	bool HasBeenCooked() const;

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

	/** Holds asset name. **/
	FString AssetName;

	/** Holds internal asset name. **/
	std::string AssetInternalName;

	/** Holds internal asset handle. **/
	HAPI_AssetId AssetId;

	/** Is set to true if this asset is being cooked. **/
	bool bIsCooking;

	/** Is set to true if this asset has been cooked. **/
	bool bHasBeenCooked;
};
