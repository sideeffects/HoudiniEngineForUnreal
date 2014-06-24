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


const FString
UHoudiniAssetInstance::GetAssetName() const
{
	return AssetName;
}


void
UHoudiniAssetInstance::SetAssetName(const FString& Name)
{
	AssetName = Name;
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
