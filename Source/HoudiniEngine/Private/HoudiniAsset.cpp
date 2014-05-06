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
	HoudiniAssetBytes(NULL),
	HoudiniAssetBytesCount(0)
{

}


/*
UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const char* InAssetName, HAPI_AssetId InAssetId) :
	Super(PCIP),
	AssetId(InAssetId)
{
	FUTF8ToTCHAR StringConverter(InAssetName);
	AssetName = StringConverter.Get();
}
*/


bool 
UHoudiniAsset::InitializeStorage(const uint8*& Buffer, const uint8* BufferEnd)
{
	if(HoudiniAssetBytes)
	{
		// Deallocate previously loaded buffer.
		FMemory::Free(HoudiniAssetBytes);
		HoudiniAssetBytes = NULL;
		HoudiniAssetBytesCount = 0;
	}

	// Calculate buffer size.
	HoudiniAssetBytesCount = BufferEnd - Buffer;

	if(HoudiniAssetBytesCount)
	{
		HoudiniAssetBytes = static_cast<uint8*>(FMemory::Malloc(HoudiniAssetBytesCount));

		if(HoudiniAssetBytes)
		{
			// Copy data into newly allocated buffer.
			FMemory::Memcpy(HoudiniAssetBytes, Buffer, HoudiniAssetBytesCount);
			return true;
		}
	}

	return false;
}


void 
UHoudiniAsset::FinishDestroy()
{
	if(HoudiniAssetBytes)
	{
		FMemory::Free(HoudiniAssetBytes);
		HoudiniAssetBytes = NULL;
		HoudiniAssetBytesCount = 0;
	}

	Super::FinishDestroy();
}

void 
UHoudiniAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize the number of bytes of raw Houdini OTL data.
	Ar << HoudiniAssetBytesCount;

	// Serialize the raw Houdini OTL data.
	if(HoudiniAssetBytesCount && HoudiniAssetBytes)
	{
		Ar.Serialize(HoudiniAssetBytes, HoudiniAssetBytesCount);
	}
}
