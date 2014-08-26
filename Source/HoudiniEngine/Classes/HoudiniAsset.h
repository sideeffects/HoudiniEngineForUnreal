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
class FHoudiniAssetObjectGeo;
class UHoudiniAssetComponent;

UCLASS(Blueprintable, BlueprintType, EditInlineNew, config=Editor)
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UHoudiniAsset(const FPostConstructInitializeProperties& PCIP,
				  const uint8*& BufferStart,
				  const uint8* BufferEnd,
				  const FString& InFileName);

public: /** UObject methods. **/

	virtual void FinishDestroy() override;
	virtual void Serialize(FArchive& Ar) override;

public:

	/** Return buffer containing the raw Houdini OTL data. **/
	const uint8* GetAssetBytes() const;

	/** Return the size in bytes of raw Houdini OTL data. **/
	uint32 GetAssetBytesCount() const;

	/** Returns true if this asset contains Houdini logo. **/
	bool IsPreviewHoudiniLogo() const;

public:

	/** Filename of the OTL. **/
	UPROPERTY()
	FString OTLFileName;

	/** Holds this asset's name. **/
	UPROPERTY()
	FString AssetName;

	/** Information for thumbnail rendering. */
	UPROPERTY()
	UThumbnailInfo* ThumbnailInfo;

protected:

	/** Current version of the asset. **/
	static const int32 PersistenceFormatVersion;

protected:

	/** Preview geometry. **/
	TArray<FHoudiniAssetObjectGeo*> PreviewObjectGeos;

	/** Buffer containing raw Houdini OTL data. **/
	uint8* AssetBytes;

	/** Field containing the size of raw Houdini OTL data in bytes. **/
	uint32 AssetBytesCount;

	/** Flag which is set to true when preview geometry contains Houdini logo. **/
	bool bPreviewHoudiniLogo;
};
