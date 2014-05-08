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
	AssetBytes(NULL),
	AssetBytesCount(0)
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
	if(AssetBytes)
	{
		// Deallocate previously loaded buffer.
		FMemory::Free(AssetBytes);
		AssetBytes = NULL;
		AssetBytesCount = 0;
	}

	// Calculate buffer size.
	AssetBytesCount = BufferEnd - Buffer;

	if(AssetBytesCount)
	{
		AssetBytes = static_cast<uint8*>(FMemory::Malloc(AssetBytesCount));

		if(AssetBytes)
		{
			// Copy data into newly allocated buffer.
			FMemory::Memcpy(AssetBytes, Buffer, AssetBytesCount);
			return true;
		}
	}

	return false;
}


void 
UHoudiniAsset::FinishDestroy()
{
	if(AssetBytes)
	{
		FMemory::Free(AssetBytes);
		AssetBytes = NULL;
		AssetBytesCount = 0;
	}

	Super::FinishDestroy();
}


void 
UHoudiniAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize the number of bytes of raw Houdini OTL data.
	Ar << AssetBytesCount;

	// Serialize the raw Houdini OTL data.
	if(AssetBytesCount && AssetBytes)
	{
		Ar.Serialize(AssetBytes, AssetBytesCount);
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
