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


UHoudiniAssetInstance::UHoudiniAssetInstance(const FPostConstructInitializeProperties& PCIP) : 
	Super(PCIP),
	HoudiniAsset(nullptr),
	AssetInternalName(""),
	AssetId(-1),
	bIsCooking(false),
	bHasBeenCooked(false)
{

}


bool
UHoudiniAssetInstance::IsInitialized() const
{
	return(-1 != AssetId);
}


void 
UHoudiniAssetInstance::SetCooking(bool bCooking)
{
	bIsCooking = bCooking;
}


bool 
UHoudiniAssetInstance::IsCooking() const
{
	return bIsCooking;
}


void 
UHoudiniAssetInstance::SetCooked(bool bCooked)
{
	bHasBeenCooked = bCooked;
}


bool 
UHoudiniAssetInstance::HasBeenCooked() const
{
	return bHasBeenCooked;
}


HAPI_AssetId 
UHoudiniAssetInstance::GetAssetId() const
{
	return(AssetId);
}


void 
UHoudiniAssetInstance::SetAssetId(HAPI_AssetId InAssetId)
{
	AssetId = InAssetId;
}


UHoudiniAsset* 
UHoudiniAssetInstance::GetHoudiniAsset() const
{
	return HoudiniAsset;
}


void 
UHoudiniAssetInstance::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	HoudiniAsset = InHoudiniAsset;
}


/*
bool
UHoudiniAsset::InitializeAsset(UHoudiniAssetManager* AssetManager, const uint8*& Buffer, const uint8* BufferEnd)
{
	// Store the asset manager.
	HoudiniAssetManager = AssetManager;

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

	// Notify asset manager about our creation.
	if(HoudiniAssetManager.IsValid())
	{
		HoudiniAssetManager->NotifyAssetCreated(this);
	}

	return true;
}


void 
UHoudiniAsset::BeginDestroy()
{
	if(HoudiniAssetManager.IsValid())
	{
		// If manager is valid, we need to notify it about our destruction.
		HoudiniAssetManager->NotifyAssetDestroyed(this);
	}

	Super::BeginDestroy();
}


void
UHoudiniAsset::FinishDestroy()
{
	// Reset the asset manager value.
	HoudiniAssetManager.Reset();

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



*/
