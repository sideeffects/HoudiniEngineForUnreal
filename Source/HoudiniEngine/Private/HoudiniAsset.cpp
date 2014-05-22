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
	AssetBytesCount(0),
	AssetId(-1)
{

}


bool
UHoudiniAsset::InitializeAsset(HAPI_AssetId InAssetId, FString Name, const uint8*& Buffer, const uint8* BufferEnd)
{
	// Store asset name.
	AssetName = Name;

	// Copy asset id.
	AssetId = InAssetId;

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

		if(!AssetBytes)
		{
			// Failed allocation.
			return false;
		}

		// Copy data into newly allocated buffer.
		FMemory::Memcpy(AssetBytes, Buffer, AssetBytesCount);
	}

	return true;
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
UHoudiniAsset::IsInitialized() const
{
	return (AssetId != -1);
}
