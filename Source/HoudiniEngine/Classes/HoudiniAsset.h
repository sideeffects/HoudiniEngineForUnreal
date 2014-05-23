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
#include "HoudiniAsset.generated.h"

class UThumbnailInfo;
class UHoudiniAssetManager;

UCLASS(Blueprintable, BlueprintType, EditInlineNew, HeaderGroup=HoudiniAsset, config=Editor) // collapsecategories, hidecategories=Object
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	friend class UHoudiniAssetManager;

	GENERATED_UCLASS_BODY()
		
public:

	/** Given the provided raw OTL data, allocate sufficient buffer and store it. **/
	bool InitializeAsset(UHoudiniAssetManager* AssetManager, const uint8*& Buffer, const uint8* BufferEnd);
	
public: /** UObject methods. **/

	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void BeginDestroy() OVERRIDE;
	virtual void FinishDestroy() OVERRIDE;

public:

	/** Return buffer containing the raw Houdini OTL data. **/
	const uint8* GetAssetBytes() const;

	/** Return the size in bytes of raw Houdini OTL data. **/
	uint32 GetAssetBytesCount() const;

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

	/** Holds internal asset handle. **/
	HAPI_AssetId AssetId;

	/** Holds internal asset name. **/
	std::string AssetInternalName;

	/** Is set to true if this asset is being cooked. **/
	bool bIsCooking;

	/** Is set to true if this asset has been cooked. **/
	bool bHasBeenCooked;
};
