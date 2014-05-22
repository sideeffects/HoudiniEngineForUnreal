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
#include <vector>


TMap<FHoudiniAssetCooking*, UHoudiniAssetComponent*> 
FHoudiniAssetCooking::AssetCookingComponentRegistry;


FCriticalSection
FHoudiniAssetCooking::CriticalSection;


FHoudiniAssetCooking::FHoudiniAssetCooking(UHoudiniAssetComponent* InHoudiniAssetComponent, const UHoudiniAsset* InHoudiniAsset) :
	HoudiniAssetComponent(InHoudiniAssetComponent),
	HoudiniAsset(InHoudiniAsset)
{
	check(HoudiniAsset);
	HOUDINI_LOG_MESSAGE(TEXT("FHoudiniAssetCooking constructor."));
}


FHoudiniAssetCooking::~FHoudiniAssetCooking()
{
	HOUDINI_LOG_MESSAGE(TEXT("FHoudiniAssetCooking destructor."));
}


void 
FHoudiniAssetCooking::RemoveSelfFromRegistryCompleted(HAPI_AssetId AssetId, const char* AssetName)
{
	// We finished cooking, see if component which requested the cooking is still around.
	UHoudiniAssetComponent** StoredHoudiniAssetComponent = AssetCookingComponentRegistry.Find(this);
	if(StoredHoudiniAssetComponent)
	{
		// Component is still in the registry and make sure component pointers match.
		check(*StoredHoudiniAssetComponent == HoudiniAssetComponent);

		// Remove ourselves from the registry.
		FHoudiniAssetCooking::CriticalSection.Lock();
		AssetCookingComponentRegistry.Remove(this);
		FHoudiniAssetCooking::CriticalSection.Unlock();

		HoudiniAssetComponent->CookingCompleted(AssetId, AssetName);
	}
}


void
FHoudiniAssetCooking::RemoveSelfFromRegistryFailed()
{
	// We finished cooking, see if component which requested the cooking is still around.
	UHoudiniAssetComponent** StoredHoudiniAssetComponent = AssetCookingComponentRegistry.Find(this);
	if (StoredHoudiniAssetComponent)
	{
		// Component is still in the registry and make sure component pointers match.
		check(*StoredHoudiniAssetComponent == HoudiniAssetComponent);

		// Remove ourselves from the registry.
		FHoudiniAssetCooking::CriticalSection.Lock();
		AssetCookingComponentRegistry.Remove(this);
		FHoudiniAssetCooking::CriticalSection.Unlock();

		HoudiniAssetComponent->CookingFailed();
	}
}


uint32
FHoudiniAssetCooking::Run()
{
	HOUDINI_LOG_MESSAGE(TEXT("FHoudiniAssetCooking Run."));

	// We are starting cooking, register this cooking instance and component.
	FHoudiniAssetCooking::CriticalSection.Lock();
	AssetCookingComponentRegistry.Add(this, HoudiniAssetComponent);
	FHoudiniAssetCooking::CriticalSection.Unlock();

	{
		HAPI_Result Result = HAPI_RESULT_SUCCESS;

		// Check if HAPI has been initialized.
		Result = HAPI_IsInitialized();
		if (HAPI_RESULT_SUCCESS == Result)
		{
			// Load the Houdini Engine asset library (OTL). This does not instantiate anything inside the Houdini scene.
			HAPI_AssetLibraryId AssetLibraryId = 0;
			Result = HAPI_LoadAssetLibraryFromMemory(reinterpret_cast<const char*>(HoudiniAsset->GetAssetBytes()), HoudiniAsset->GetAssetBytesCount(), &AssetLibraryId);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				// We failed loading Houdini Engine asset library from memory, report.
				RemoveSelfFromRegistryFailed();
				return 0;
			}

			// Retrieve number of assets contained in this library.
			int32 AssetCount = 0;
			Result = HAPI_GetAvailableAssetCount(AssetLibraryId, &AssetCount);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				// We failed to retrieve number of assets, report.
				RemoveSelfFromRegistryFailed();
				return 0;
			}

			// Retrieve available assets. 
			std::vector<int> AssetNames;
			AssetNames.reserve(AssetCount);
			Result = HAPI_GetAvailableAssets(AssetLibraryId, &AssetNames[0], AssetCount);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				// We failed to retrieve available assets, report.
				RemoveSelfFromRegistryFailed();
				return 0;
			}

			// For now we will load first asset only.
			int32 AssetFirstNameLength = 0;
			Result = HAPI_GetStringBufLength(AssetNames[0], &AssetFirstNameLength);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				// We failed to retrieve length of first asset's name, report.
				RemoveSelfFromRegistryFailed();
				return 0;
			}

			// Retrieve name of first asset.
			std::vector<char> AssetFirstName;
			AssetFirstName.reserve(AssetFirstNameLength + 1);
			AssetFirstName[AssetFirstNameLength] = '\0';
			Result = HAPI_GetString(AssetNames[0], &AssetFirstName[0], AssetFirstNameLength);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				// We failed to retrieve asset name, report.
				RemoveSelfFromRegistryFailed();
				return 0;
			}

			// Instantiate the asset.
			HAPI_AssetId AssetId = -1;
			Result = HAPI_InstantiateAsset(&AssetFirstName[0], true, &AssetId);
			if (HAPI_RESULT_SUCCESS != Result)
			{
				// We failed asset instantiation, report.
				RemoveSelfFromRegistryFailed();
				return 0;
			}
			
			// Construct an empty Houdini Engine asset.
			// Notify component that cooking is complete.
			RemoveSelfFromRegistryCompleted(AssetId, (const char*) &AssetFirstName[0]);
		}
		else
		{
			// HAPI has not been initialized.
			HOUDINI_LOG_ERROR(TEXT(" Cannot import Houdini Engine asset, HAPI has not been initialized."));
			RemoveSelfFromRegistryFailed();
			return 0;
		}
	}
	
	return 0;
}
