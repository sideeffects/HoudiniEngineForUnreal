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
		
public:

	/** Given the provided raw OTL data, allocate sufficient buffer and store it. **/
	//bool InitializeAsset(UHoudiniAssetManager* AssetManager, const uint8*& Buffer, const uint8* BufferEnd);
	
public: /** UObject methods. **/

	//virtual void Serialize(FArchive& Ar) OVERRIDE;
	//virtual void BeginDestroy() OVERRIDE;
	//virtual void FinishDestroy() OVERRIDE;

#if 0
public:

	/** Return buffer containing the raw Houdini OTL data. **/
	const uint8* GetAssetBytes() const;

	/** Return the size in bytes of raw Houdini OTL data. **/
	uint32 GetAssetBytesCount() const;

	
	
public:

	/** Holds this asset's name. **/
	UPROPERTY()
	FString AssetName;

	/** Information for thumbnail rendering. */
	UPROPERTY()
	class UThumbnailInfo* ThumbnailInfo;

protected:

	/** Asset manager for this asset. **/
	TWeakObjectPtr<UHoudiniAssetManager> HoudiniAssetManager;

	/** Buffer containing raw Houdini OTL data. **/
	uint8* AssetBytes;

	/** Field containing the size of raw Houdini OTL data in bytes. **/
	uint32 AssetBytesCount;

#endif

protected:

	/** Holds Houdini asset we are instantiating. **/
	UHoudiniAsset* HoudiniAsset;

	/** Holds internal asset name. **/
	std::string AssetInternalName;

	/** Holds internal asset handle. **/
	HAPI_AssetId AssetId;

	/** Is set to true if this asset is being cooked. **/
	bool bIsCooking;

	/** Is set to true if this asset has been cooked. **/
	bool bHasBeenCooked;
};
