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


UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP) : 
	Super(PCIP),
	AssetBytes(nullptr),
	AssetBytesCount(0)
{

}


UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const uint8*& BufferStart, const uint8* BufferEnd) : 
	Super(PCIP),
	AssetBytes(nullptr),
	AssetBytesCount(0)
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

	Super::FinishDestroy();
}
