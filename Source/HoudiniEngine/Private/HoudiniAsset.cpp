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

#include "HoudiniEnginePrivatePCH.h"


const int32 
UHoudiniAsset::PersistenceFormatVersion = 0;


UHoudiniAsset::UHoudiniAsset(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	AssetBytes(nullptr),
	AssetBytesCount(0),
	bPreviewHoudiniLogo(false)
{

}


UHoudiniAsset::UHoudiniAsset(const FObjectInitializer& ObjectInitializer,
							 const uint8*& BufferStart,
							 const uint8* BufferEnd,
							 const FString& InFileName) :
	Super(ObjectInitializer),
	OTLFileName(InFileName),
	AssetBytes(nullptr),
	AssetBytesCount(0),
	bPreviewHoudiniLogo(false)
{
	// Calculate buffer size.
	AssetBytesCount = BufferEnd - BufferStart;

	if(AssetBytesCount)
	{
		// Allocate buffer to store OTL raw data.
		AssetBytes = static_cast<uint8*>(FMemory::Malloc(AssetBytesCount));

		if(AssetBytes)
		{
			// Copy data into a newly allocated buffer.
			FMemory::Memcpy(AssetBytes, BufferStart, AssetBytesCount);
		}
	}
}


const uint8*
UHoudiniAsset::GetAssetBytes() const
{
	return AssetBytes;
}


uint32
UHoudiniAsset::GetAssetBytesCount() const
{
	return AssetBytesCount;
}


bool
UHoudiniAsset::IsPreviewHoudiniLogo() const
{
	return bPreviewHoudiniLogo;
}


void
UHoudiniAsset::FinishDestroy()
{
	FThumbnailRenderingInfo* RenderInfo = UThumbnailManager::Get().GetRenderingInfo(this);
	if(RenderInfo)
	{
		UHoudiniAssetThumbnailRenderer* ThumbnailRenderer = CastChecked<UHoudiniAssetThumbnailRenderer>(RenderInfo->Renderer);
		if(ThumbnailRenderer)
		{
			ThumbnailRenderer->RemoveAssetThumbnail(this);
		}
	}

	// Release buffer which was used to store raw OTL data.
	if(AssetBytes)
	{
		FMemory::Free(AssetBytes);
		AssetBytes = nullptr;
		AssetBytesCount = 0;
	}

	Super::FinishDestroy();
}


void
UHoudiniAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Properties will get serialized.

	// Read persistence format version.
	int32 FormatVersion = UHoudiniAsset::PersistenceFormatVersion;
	Ar << FormatVersion;

	// Make sure persistence format version matches.
	if(FormatVersion != UHoudiniAsset::PersistenceFormatVersion)
	{
		return;
	}

	{
		Ar << AssetBytesCount;

		if(Ar.IsLoading())
		{
			// If buffer was previously used, release it.
			if(AssetBytes)
			{
				FMemory::Free(AssetBytes);
				AssetBytes = nullptr;
			}

			// Allocate sufficient space to read stored raw OTL data.
			if(AssetBytesCount)
			{
				AssetBytes = static_cast<uint8*>(FMemory::Malloc(AssetBytesCount));
			}
		}

		if(AssetBytes && AssetBytesCount)
		{
			Ar.Serialize(AssetBytes, AssetBytesCount);
		}
	}

	// Serialize whether we are storing logo preview or not.
	Ar << bPreviewHoudiniLogo;
}
