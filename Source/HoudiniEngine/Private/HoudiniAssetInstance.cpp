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
	AssetId(-1)
{

}


UHoudiniAsset*
UHoudiniAssetInstance::GetHoudiniAsset() const
{
	FPlatformMisc::MemoryBarrier();
	return HoudiniAsset;
}


void
UHoudiniAssetInstance::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	FPlatformAtomics::InterlockedExchangePtr((void**) &HoudiniAsset, InHoudiniAsset);
}


const FString
UHoudiniAssetInstance::GetAssetName() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return AssetName;
}


void
UHoudiniAssetInstance::SetAssetName(const FString& Name)
{
	FScopeLock ScopeLock(&CriticalSection);
	AssetName = Name;
}


bool
UHoudiniAssetInstance::IsInitialized() const
{
	return(-1 != FPlatformAtomics::InterlockedCompareExchange((volatile int32*) &AssetId, -1, -1));
}


HAPI_AssetId
UHoudiniAssetInstance::GetAssetId() const
{
	FPlatformMisc::MemoryBarrier();
	return(AssetId);
}


void
UHoudiniAssetInstance::SetAssetId(HAPI_AssetId InAssetId)
{
	FPlatformAtomics::InterlockedExchange(&AssetId, InAssetId);
}
