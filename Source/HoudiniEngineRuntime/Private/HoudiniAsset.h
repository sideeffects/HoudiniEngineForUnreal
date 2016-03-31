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
#include "HoudiniAsset.generated.h"


class UThumbnailInfo;
class UAssetImportData;
class UHoudiniAssetInstance;
class UHoudiniAssetComponent;


UCLASS(EditInlineNew, config=Engine)
class HOUDINIENGINERUNTIME_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

/** UObject methods. **/
public:

	virtual void FinishDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

public:

	/** Initialize this asset from given buffer / file. **/
	void CreateAsset(const uint8* BufferStart, const uint8* BufferEnd, const FString& InFileName);

	/** Return buffer containing the raw Houdini OTL data. **/
	const uint8* GetAssetBytes() const;

	/** Return path of the corresponding OTL/HDA file. **/
	const FString& GetAssetFileName() const;

	/** Return the size in bytes of raw Houdini OTL data. **/
	uint32 GetAssetBytesCount() const;

	/** Returns true if this asset contains Houdini logo. **/
	bool IsPreviewHoudiniLogo() const;

	/** Return true if this asset is a limited commercial asset. **/
	bool IsAssetLimitedCommercial() const;

	/** Return true if this asset is a non commercial asset. **/
	bool IsAssetNonCommercial() const;

public:

	/** Create an asset instance. **/
	UHoudiniAssetInstance* CreateHoudiniAssetInstance(UObject* Outer);

public:

	/** Filename of the OTL. **/
	UPROPERTY()
	FString AssetFileName;

	/** Information for thumbnail rendering. */
	UPROPERTY()
	UThumbnailInfo* ThumbnailInfo;

#if WITH_EDITORONLY_DATA

	/** Importing data and options used for this Houdini asset. */
	UPROPERTY(Category=ImportSettings, VisibleAnywhere, Instanced)
	UAssetImportData* AssetImportData;

#endif

protected:

	/** Current version of the asset. **/
	static const uint32 PersistenceFormatVersion;

protected:

	/** Buffer containing raw Houdini OTL data. **/
	uint8* AssetBytes;

	/** Field containing the size of raw Houdini OTL data in bytes. **/
	uint32 AssetBytesCount;

	/** Version of the asset file format. **/
	uint32 FileFormatVersion;

	/** Flags used by this asset. **/
	union
	{
		struct
		{
			/** Flag which is set to true when preview geometry contains Houdini logo. **/
			uint32 bPreviewHoudiniLogo : 1;

			/** Flag which indicates if this is a limited commercial license asset. **/
			uint32 bAssetLimitedCommercial : 1;

			/** Flag which indicates if this is a non-commercial license asset. **/
			uint32 bAssetNonCommercial : 1;
		};

		uint32 HoudiniAssetFlagsPacked;
	};
};
