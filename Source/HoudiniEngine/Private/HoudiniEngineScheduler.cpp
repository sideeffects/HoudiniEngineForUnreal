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


const uint32
FHoudiniEngineScheduler::MaximumTaskSize = 1024;


FHoudiniEngineScheduler::FHoudiniEngineScheduler() :
	Tasks(FHoudiniEngineScheduler::MaximumTaskSize),
	bStopping(false)
{

}


FHoudiniEngineScheduler::~FHoudiniEngineScheduler()
{

}


void
FHoudiniEngineScheduler::AddTask(const FHoudiniEngineTask& Task)
{
	Tasks.Enqueue(Task);
}


void
FHoudiniEngineScheduler::TaskInstantiateAsset(const FHoudiniEngineTask& Task)
{
	HOUDINI_LOG_MESSAGE(TEXT("HAPI Asynchronous Instantiation Started."));

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("TaskInstantiateAsset failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));
		return;
	}

	if(!Task.AssetInstance)
	{
		return;
	}

	// Get asset associated with this instance.
	UHoudiniAsset* HoudiniAsset = Task.AssetInstance->GetHoudiniAsset();

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HAPI_AssetLibraryId AssetLibraryId = 0;
	int32 AssetCount = 0;
	std::vector<int> AssetNames;
	std::string AssetNameString;
	double LastUpdateTime;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_LoadAssetLibraryFromMemory(reinterpret_cast<const char*>(HoudiniAsset->GetAssetBytes()), 
								HoudiniAsset->GetAssetBytesCount(), &AssetLibraryId), void());
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssetCount(AssetLibraryId, &AssetCount), void());
	AssetNames.resize(AssetCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssets(AssetLibraryId, &AssetNames[0], AssetCount), void());

	// If we have assets, instantiate first one.
	if(AssetCount && FHoudiniEngineUtils::GetAssetName(AssetNames[0], AssetNameString))
	{
		// Translate asset name into Unreal string.
		FString AssetName = ANSI_TO_TCHAR(AssetNameString.c_str());

		// Set asset name for the asset instance we are processing.
		Task.AssetInstance->SetAssetName(AssetName);

		// Initialize last update time.
		LastUpdateTime = FPlatformTime::Seconds();

		HAPI_AssetId AssetId = -1;
		bool CookOnLoad = true;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_InstantiateAsset(&AssetNameString[0], CookOnLoad, &AssetId), void());

		// We need to spin until instantiation is finished.
		while(true)
		{
			int Status = HAPI_STATE_STARTING_COOK;
			HOUDINI_CHECK_ERROR(&Result, HAPI_GetStatus(HAPI_STATUS_COOK_STATE, &Status));

			if(HAPI_STATE_READY == Status)
			{
				// Cooking has been successful.
				Task.AssetInstance->SetAssetId(AssetId);

				break;
			}
			else if (HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
			{
				// There was an error while instantiating.
				break;
			}

			static const double NotificationUpdateFrequency = 1.0;
			if((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
			{
				// Reset update time.
				LastUpdateTime = FPlatformTime::Seconds();

				/*
				// Retrieve status string.
				int StatusStringBufferLength = 0;
				HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusStringBufLength(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS, &StatusStringBufferLength), void());
				std::vector<char> StatusStringBuffer(StatusStringBufferLength, '\0');
				HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusString(HAPI_STATUS_COOK_STATE, &StatusStringBuffer[0]), void());
				FString StatusString = ANSI_TO_TCHAR(&StatusStringBuffer[0]);
				*/
			}

			// We want to yield for a bit.
			FPlatformProcess::Sleep(0.0f);
		}
	}
}


void
FHoudiniEngineScheduler::TaskCookAsset(const FHoudiniEngineTask& Task)
{

}


void 
FHoudiniEngineScheduler::ProcessQueuedTasks()
{
	while(!bStopping)
	{
		FHoudiniEngineTask Task;

		while(Tasks.Dequeue(Task))
		{
			switch(Task.TaskType)
			{
				case HoudiniEngineTaskType::AssetInstantiation:
				{
					TaskInstantiateAsset(Task);
					break;
				}

				case HoudiniEngineTaskType::AssetCooking:
				{
					TaskCookAsset(Task);
					break;
				}

				default:
				{
					break;
				}
			}

			if(FPlatformProcess::SupportsMultithreading())
			{
				// We want to yield for a bit.
				FPlatformProcess::Sleep(0.0f);
			}
			else
			{
				// If we are running in single threaded mode, break so we don't block everything else.
				bStopping = true;
				break;
			}
		}
	}
}


uint32
FHoudiniEngineScheduler::Run()
{
	ProcessQueuedTasks();
	return 0;
}


void
FHoudiniEngineScheduler::Stop()
{
	bStopping = true;
}


void
FHoudiniEngineScheduler::Tick()
{
	ProcessQueuedTasks();
}


FSingleThreadRunnable*
FHoudiniEngineScheduler::GetSingleThreadInterface()
{
	return this;
}
