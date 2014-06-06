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
	AssetBytesCount(0)/*,
	AssetId(-1),
	bIsCooking(false),
	bHasBeenCooked(false)*/
{

}


UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const uint8*& Buffer, const uint8* BufferEnd) : 
	Super(PCIP),
	AssetBytes(nullptr),
	AssetBytesCount(0)/*,
	AssetId(-1),
	bIsCooking(false),
	bHasBeenCooked(false)*/
{
	// Calculate buffer size.
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


/*
void 
UHoudiniAsset::BeginDestroy()
{
	//if(HoudiniAssetManager.IsValid())
	//{
	//	// If manager is valid, we need to notify it about our destruction.
	//	HoudiniAssetManager->NotifyAssetDestroyed(this);
	//}

	Super::BeginDestroy();
}


void
UHoudiniAsset::FinishDestroy()
{
	// Reset the asset manager value.
	//HoudiniAssetManager.Reset();

	if(AssetBytes)
	{
		FMemory::Free(AssetBytes);
		AssetBytes = nullptr;
		AssetBytesCount = 0;
	}

	if(-1 != AssetId)
	{
		// We need to release the internal asset.
		HOUDINI_CHECK_ERROR(HAPI_DestroyAsset(AssetId));
	}
	
	Super::FinishDestroy();
}


void
UHoudiniAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize asset name.
	Ar << AssetName;

	// Serialize the number of bytes of raw Houdini OTL data.
	Ar << AssetBytesCount;

	// Serialize the raw Houdini OTL data.
	if(AssetBytesCount && AssetBytes)
	{
		Ar.Serialize(AssetBytes, AssetBytesCount);
	}
}
*/

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
