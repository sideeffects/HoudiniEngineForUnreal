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


const uint32
UHoudiniAsset::PersistenceFormatVersion = 2u;


UHoudiniAsset::UHoudiniAsset(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	AssetFileName(TEXT("")),
	AssetBytes(nullptr),
	AssetBytesCount(0),
	FileFormatVersion(UHoudiniAsset::PersistenceFormatVersion),
	bPreviewHoudiniLogo(false)
{

}


void
UHoudiniAsset::CreateAsset(const uint8*& BufferStart, const uint8* BufferEnd, const FString& InFileName)
{
	AssetFileName = InFileName;

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


const FString&
UHoudiniAsset::GetAssetFileName() const
{
	return AssetFileName;
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
#if WITH_EDITOR

	FThumbnailRenderingInfo* RenderInfo = UThumbnailManager::Get().GetRenderingInfo(this);
	if(RenderInfo)
	{
		UHoudiniAssetThumbnailRenderer* ThumbnailRenderer =
			CastChecked<UHoudiniAssetThumbnailRenderer>(RenderInfo->Renderer);

		if(ThumbnailRenderer)
		{
			ThumbnailRenderer->RemoveAssetThumbnail(this);
		}
	}

#endif

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

	// Serialize persistence format version.
	Ar << FileFormatVersion;

	// Make sure persistence format version matches.
	if(Ar.IsLoading() && (FileFormatVersion != UHoudiniAsset::PersistenceFormatVersion))
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

	// Serialize flags.
	Ar << HoudiniAssetFlagsPacked;

	// Serialize asset file path.
	Ar << AssetFileName;
}


void
UHoudiniAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add(FAssetRegistryTag("FileName", AssetFileName, FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("FileFormatVersion", FString::FromInt(FileFormatVersion),
		FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Bytes", FString::FromInt(AssetBytesCount), FAssetRegistryTag::TT_Numerical));

	Super::GetAssetRegistryTags(OutTags);
}


bool
UHoudiniAsset::IsSupportedFileFormat() const
{
	return UHoudiniAsset::PersistenceFormatVersion == FileFormatVersion;
}
