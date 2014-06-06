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

UCLASS(Blueprintable, BlueprintType, EditInlineNew, config=Editor)
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const uint8*& Buffer, const uint8* BufferEnd);
	
public: /** UObject methods. **/

	//virtual void Serialize(FArchive& Ar) OVERRIDE;
	//virtual void BeginDestroy() OVERRIDE;
	//virtual void FinishDestroy() OVERRIDE;

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

	/** Buffer containing raw Houdini OTL data. **/
	uint8* AssetBytes;

	/** Field containing the size of raw Houdini OTL data in bytes. **/
	uint32 AssetBytesCount;
};
