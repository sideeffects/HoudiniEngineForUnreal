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
class UHoudiniAssetComponent;


UCLASS(Blueprintable, BlueprintType, EditInlineNew, config=Editor)
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UHoudiniAsset(const FObjectInitializer& ObjectInitializer, const uint8*& BufferStart, const uint8* BufferEnd,
		const FString& InFileName);

/** UObject methods. **/
public:

	virtual void FinishDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

public:

	/** Return buffer containing the raw Houdini OTL data. **/
	const uint8* GetAssetBytes() const;

	/** Return path of the corresponding OTL/HDA file. **/
	const FString& GetAssetFileName() const;

	/** Return the size in bytes of raw Houdini OTL data. **/
	uint32 GetAssetBytesCount() const;

	/** Returns true if this asset contains Houdini logo. **/
	bool IsPreviewHoudiniLogo() const;

	/** Return true if file format is matching the latest version. **/
	bool IsSupportedFileFormat() const;

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
		};

		uint32 HoudiniAssetFlagsPacked;
	};
};
