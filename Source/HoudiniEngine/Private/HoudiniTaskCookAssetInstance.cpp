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


FHoudiniTaskCookAssetInstance::FHoudiniTaskCookAssetInstance(IHoudiniTaskCookAssetInstanceCallback* InHoudiniTaskCookAssetInstanceCallback, UHoudiniAssetInstance* InHoudiniAssetInstance) :
	HoudiniTaskCookAssetInstanceCallback(InHoudiniTaskCookAssetInstanceCallback),
	HoudiniAssetInstance(InHoudiniAssetInstance)
{

}


FHoudiniTaskCookAssetInstance::~FHoudiniTaskCookAssetInstance()
{

}


void 
FHoudiniTaskCookAssetInstance::RemoveNotification()
{
	// If we have a notification object, tell engine to remove it.
	if(NotificationInfo.IsValid())
	{
		FHoudiniEngine::Get().RemoveNotification(NotificationInfo.Get());
	}
}


uint32 
FHoudiniTaskCookAssetInstance::RunErrorCleanUp(HAPI_Result Result)
{
	// Notify callback that a certain error occurred during cooking.
	HoudiniTaskCookAssetInstanceCallback->NotifyAssetInstanceCookingFailed(HoudiniAssetInstance, Result);
	RemoveNotification();
	
	return 0;
}


uint32 
FHoudiniTaskCookAssetInstance::Run()
{
	HOUDINI_LOG_MESSAGE(TEXT("HoudiniTaskCookAssetInstance Asynchronous Cooking Started."));

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("HoudiniTaskCookAssetInstance failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));
		return RunErrorCleanUp(HAPI_RESULT_NOT_INITIALIZED);
	}

	// Get asset associated with this instance.
	UHoudiniAsset* HoudiniAsset = HoudiniAssetInstance->GetHoudiniAsset();

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	
	// Load asset library from given buffer.
	HAPI_AssetLibraryId AssetLibraryId = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_LoadAssetLibraryFromMemory(reinterpret_cast<const char*>(HoudiniAsset->GetAssetBytes()), HoudiniAsset->GetAssetBytesCount(), &AssetLibraryId), RunErrorCleanUp(Result));

	// Retrieve number of assets contained in this library.
	int32 AssetCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssetCount(AssetLibraryId, &AssetCount), RunErrorCleanUp(Result));

	// Retrieve available assets. 
	std::vector<int> AssetNames(AssetCount, 0);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssets(AssetLibraryId, &AssetNames[0], AssetCount), RunErrorCleanUp(Result));

	// If we have assets, instantiate first one.
	std::string AssetName;
	if(AssetCount && FHoudiniEngineUtils::GetAssetName(AssetNames[0], AssetName))
	{
		FString AssetNameString = ANSI_TO_TCHAR(AssetName.c_str());
		FFormatNamedArguments Args;
		Args.Add(TEXT("AssetName"), FText::FromString(AssetNameString));
		FText ProgressMessage = FText::Format(NSLOCTEXT("AssetBaking", "AssetBakingInProgress", "Cooking Houdini Asset ({AssetName})"), Args);
		FHoudiniEngineNotificationInfo* Notification = new FHoudiniEngineNotificationInfo(ProgressMessage);
		FHoudiniEngine::Get().AddNotification(Notification);
		NotificationInfo = MakeShareable(Notification);
		
		HAPI_AssetId AssetId = -1;
		bool CookOnLoad = true;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_InstantiateAsset(&AssetName[0], CookOnLoad, &AssetId), RunErrorCleanUp(Result));

		// We need to spin until cooking is finished.
		while(true)
		{
			int Status = HAPI_STATE_STARTING_COOK;
			HOUDINI_CHECK_ERROR(HAPI_GetStatus(HAPI_STATUS_STATE, &Status));

			if(HAPI_STATE_READY == Status)
			{
				HoudiniAssetInstance->SetCooking(false);
				HoudiniAssetInstance->SetCooked(true);
				HoudiniAssetInstance->SetAssetId(AssetId);

				// Cooking has been successful.
				HoudiniTaskCookAssetInstanceCallback->NotifyAssetInstanceCookingFinished(HoudiniAssetInstance, AssetId, AssetName);
				break;
			}
			else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
			{
				// There was an error while cooking.
				HOUDINI_LOG_ERROR(TEXT("HoudiniTaskCookAsset failed: Asset cooked with errors."));
				HoudiniTaskCookAssetInstanceCallback->NotifyAssetInstanceCookingFailed(HoudiniAssetInstance, HAPI_RESULT_FAILURE);
				break;
			}

			// We want to yield for a bit.
			FPlatformProcess::Sleep(0.01f);
		}
	}

	RemoveNotification();
	return 0;
}
