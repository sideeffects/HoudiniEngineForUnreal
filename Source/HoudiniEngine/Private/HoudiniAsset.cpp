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


UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const uint8*& Buffer, const uint8* BufferEnd) : 
	Super(PCIP),
	AssetBytes(nullptr),
	AssetBytesCount(0)
{
	// Calculate buffer size.

	//FIXME: Maybe BufferStart instead of Buffer?
	AssetBytesCount = BufferEnd - Buffer;

	if(AssetBytesCount)
	{
		AssetBytes = static_cast<uint8*>(FMemory::Malloc(AssetBytesCount));

		if(AssetBytes)
		{
			// Copy data into newly allocated buffer.
			FMemory::Memcpy(AssetBytes, Buffer, AssetBytesCount);
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
