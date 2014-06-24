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


FHoudiniTaskCookAsset::FHoudiniTaskCookAsset(IHoudiniTaskCookAssetCallback* InHoudiniTaskCookAssetCallback, UHoudiniAssetInstance* InHoudiniAssetInstance) :
HoudiniTaskCookAssetCallback(InHoudiniTaskCookAssetCallback),
HoudiniAssetInstance(InHoudiniAssetInstance),
LastUpdateTime(0.0)
{

}


FHoudiniTaskCookAsset::~FHoudiniTaskCookAsset()
{

}


void
FHoudiniTaskCookAsset::RemoveNotification()
{
	// If we have a notification object, tell engine to remove it.
	if(NotificationInfo.IsValid())
	{
		FHoudiniEngine::Get().RemoveNotification(NotificationInfo.Get());
	}
}


uint32
FHoudiniTaskCookAsset::RunErrorCleanUp(HAPI_Result Result)
{
	// Notify callback that a certain error occurred during cooking.
	HoudiniTaskCookAssetCallback->NotifyAssetCookingFailed(HoudiniAssetInstance, Result);
	RemoveNotification();

	return 0;
}


void
FHoudiniTaskCookAsset::UpdateNotification(const FString& StatusString)
{
	if(NotificationInfo.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("AssetName"), FText::FromString(HoudiniAssetInstance->GetAssetName()));
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));

		NotificationInfo->Text = FText::Format(NSLOCTEXT("AssetBaking", "AssetBakingInProgress", "({AssetName}) : ({AssetStatus})"), Args);

		// Ask engine to update notification.
		FHoudiniEngine::Get().UpdateNotification(NotificationInfo.Get());
	}
}


uint32
FHoudiniTaskCookAsset::Run()
{
	HOUDINI_LOG_MESSAGE(TEXT("FHoudiniTaskCookAsset Asynchronous Cooking Started."));

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("HoudiniTaskCookAsset failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));
		return RunErrorCleanUp(HAPI_RESULT_NOT_INITIALIZED);
	}

	// Get asset id.
	HAPI_AssetId AssetId = HoudiniAssetInstance->GetAssetId();

	// Retrieve instance of Houdini Engine, this is used for notification submission.
	FHoudiniEngine& HoudiniEngine = FHoudiniEngine::Get();

	// Construct initial notification message.
	NotificationInfo = MakeShareable(new FHoudiniEngineNotificationInfo());
	HoudiniEngine.AddNotification(NotificationInfo.Get());
	UpdateNotification(TEXT("Cook started"));

	// Initialize last update time.
	LastUpdateTime = FPlatformTime::Seconds();

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HOUDINI_CHECK_ERROR(&Result, HAPI_CookAsset(HoudiniAssetInstance->GetAssetId(), nullptr));
	if(HAPI_RESULT_SUCCESS != Result)
	{
		RunErrorCleanUp(Result);
	}

	// We need to spin until cooking is finished.
	while(true)
	{
		int Status = HAPI_STATE_STARTING_COOK;
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetStatus(HAPI_STATUS_COOK_STATE, &Status));

		if(HAPI_STATE_READY == Status)
		{
			HoudiniAssetInstance->SetCooking(false);
			HoudiniAssetInstance->SetCooked(true);
			HoudiniAssetInstance->SetAssetId(AssetId);

			// Cooking has been successful.
			UpdateNotification(TEXT("Cook finished"));
			HoudiniTaskCookAssetCallback->NotifyAssetCookingFinished(HoudiniAssetInstance, AssetId);
			break;
		}
		else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
		{
			// There was an error while cooking.
			HOUDINI_LOG_ERROR(TEXT("HoudiniTaskCookAsset failed: Asset cooked with errors."));
			UpdateNotification(TEXT("Cook finished with errors"));
			HoudiniTaskCookAssetCallback->NotifyAssetCookingFailed(HoudiniAssetInstance, HAPI_RESULT_FAILURE);
			break;
		}

		static const double NotificationUpdateFrequency = 1.0;
		if((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
		{
			// Reset update time.
			LastUpdateTime = FPlatformTime::Seconds();

			// Retrieve status string.
			int StatusStringBufferLength = 0;
			HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusStringBufLength(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS, &StatusStringBufferLength), RunErrorCleanUp(Result));
			std::vector<char> StatusStringBuffer(StatusStringBufferLength, '\0');
			HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusString(HAPI_STATUS_COOK_STATE, &StatusStringBuffer[0]), RunErrorCleanUp(Result));
			FString StatusString = ANSI_TO_TCHAR(&StatusStringBuffer[0]);

			UpdateNotification(StatusString);
		}

		// We want to yield for a bit.
		FPlatformProcess::Sleep(0.01f);
	}

	FPlatformProcess::Sleep(1.0f);
	RemoveNotification();
	return 0;
}
