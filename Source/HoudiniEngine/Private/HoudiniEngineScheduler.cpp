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
FHoudiniEngineScheduler::InitialTaskSize = 256;


FHoudiniEngineScheduler::FHoudiniEngineScheduler() :
	Tasks(nullptr),
	PositionWrite(0),
	PositionRead(0),
	bStopping(false)
{
	//  Make sure size is power of two.
	TaskCount = FPlatformMath::RoundUpToPowerOfTwo(FHoudiniEngineScheduler::InitialTaskSize);

	if(TaskCount)
	{
		// Allocate buffer to store all tasks.
		Tasks = static_cast<FHoudiniEngineTask*>(FMemory::Malloc(TaskCount * sizeof(FHoudiniEngineTask)));

		if(Tasks)
		{
			// Zero memory.
			FMemory::Memset(Tasks, 0x0, TaskCount * sizeof(FHoudiniEngineTask));
		}
	}
}


FHoudiniEngineScheduler::~FHoudiniEngineScheduler()
{
	if(TaskCount)
	{
		FMemory::Free(Tasks);
		Tasks = nullptr;
	}
}


void 
FHoudiniEngineScheduler::TaskDescription(FHoudiniEngineTaskInfo& TaskInfo, const FString& ActorName, const FString& StatusString)
{
	FFormatNamedArguments Args;

	if(!ActorName.IsEmpty())
	{	
		Args.Add(TEXT("AssetName"), FText::FromString(ActorName));
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));
		TaskInfo.StatusText = FText::Format(NSLOCTEXT("TaskDescription", "TaskDescriptionProgress", "({AssetName}) : ({AssetStatus})"), Args);
	}
	else
	{
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));
		TaskInfo.StatusText = FText::Format(NSLOCTEXT("TaskDescription", "TaskDescriptionProgress", "({AssetStatus})"), Args);
	}
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

	if(!Task.Asset.IsValid())
	{
		// Asset is no longer valid, return.
		return;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	UHoudiniAsset* HoudiniAsset = Task.Asset.Get();
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

		// Initialize last update time.
		LastUpdateTime = FPlatformTime::Seconds();

		HAPI_AssetId AssetId = -1;
		bool CookOnLoad = true;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_InstantiateAsset(&AssetNameString[0], CookOnLoad, &AssetId), void());

		// Add processing notification.
		FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, -1, EHoudiniEngineTaskType::AssetInstantiation, EHoudiniEngineTaskState::Processing);
		TaskDescription(TaskInfo, Task.ActorName, TEXT("Started Instantiation"));
		FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

		// We need to spin until instantiation is finished.
		while(true)
		{
			int Status = HAPI_STATE_STARTING_COOK;
			HOUDINI_CHECK_ERROR(&Result, HAPI_GetStatus(HAPI_STATUS_COOK_STATE, &Status));

			if(HAPI_STATE_READY == Status)
			{
				// Cooking has been successful.
				FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetInstantiation, EHoudiniEngineTaskState::Finished);
				TaskDescription(TaskInfo, Task.ActorName, TEXT("Finished Instantiation"));
				FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

				break;
			}
			else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
			{
				// There was an error while instantiating.
				FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetInstantiation, EHoudiniEngineTaskState::FinishedWithErrors);
				TaskDescription(TaskInfo, Task.ActorName, TEXT("Finished Instantiation with Errors"));
				FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

				break;
			}

			static const double NotificationUpdateFrequency = 0.5;
			if((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
			{
				// Reset update time.
				LastUpdateTime = FPlatformTime::Seconds();

				// Retrieve status string.
				int StatusStringBufferLength = 0;
				HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusStringBufLength(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS, &StatusStringBufferLength), void());
				std::vector<char> StatusStringBuffer(StatusStringBufferLength, '\0');
				HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusString(HAPI_STATUS_COOK_STATE, &StatusStringBuffer[0]), void());
				FString StatusString = ANSI_TO_TCHAR(&StatusStringBuffer[0]);

				FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetInstantiation, EHoudiniEngineTaskState::Processing);
				TaskDescription(TaskInfo, Task.ActorName, StatusString);
				FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);
			}

			// We want to yield.
			FPlatformProcess::Sleep(0.0f);
		}
	}
}


void
FHoudiniEngineScheduler::TaskCookAsset(const FHoudiniEngineTask& Task)
{
	HOUDINI_LOG_MESSAGE(TEXT("HAPI Asynchronous Cooking Started."));

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("TaskCookAsset failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));
		return;
	}

	if(!Task.AssetComponent.IsValid())
	{
		// Asset component is no longer valid, return.
		return;
	}

	// Retrieve asset id.
	HAPI_AssetId AssetId = Task.AssetComponent->GetAssetId();

	if(-1 == AssetId)
	{
		// We have an invalid asset id.
		HOUDINI_LOG_ERROR(TEXT("TaskCookAsset failed: Invalid Asset Id."));
		return;
	}

	HOUDINI_CHECK_ERROR_RETURN(HAPI_CookAsset(AssetId, nullptr), void());

	// Add processing notification.
	FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetCooking, EHoudiniEngineTaskState::Processing);
	TaskDescription(TaskInfo, Task.ActorName, TEXT("Started Cooking"));
	FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

	// Initialize last update time.
	double LastUpdateTime = FPlatformTime::Seconds();
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// We need to spin until cooking is finished.
	while(true)
	{
		int Status = HAPI_STATE_STARTING_COOK;
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetStatus(HAPI_STATUS_COOK_STATE, &Status));

		if(HAPI_STATE_READY == Status)
		{
			// Cooking has been successful.
			FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetCooking, EHoudiniEngineTaskState::Finished);
			TaskDescription(TaskInfo, Task.ActorName, TEXT("Finished Cooking"));
			FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

			break;
		}
		else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
		{
			// There was an error while instantiating.
			FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetCooking, EHoudiniEngineTaskState::FinishedWithErrors);
			TaskDescription(TaskInfo, Task.ActorName, TEXT("Finished Cooking with Errors"));
			FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

			break;
		}

		static const double NotificationUpdateFrequency = 0.5;
		if((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
		{
			// Reset update time.
			LastUpdateTime = FPlatformTime::Seconds();

			// Retrieve status string.
			int StatusStringBufferLength = 0;
			HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusStringBufLength(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS, &StatusStringBufferLength), void());
			std::vector<char> StatusStringBuffer(StatusStringBufferLength, '\0');
			HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStatusString(HAPI_STATUS_COOK_STATE, &StatusStringBuffer[0]), void());
			FString StatusString = ANSI_TO_TCHAR(&StatusStringBuffer[0]);

			FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, AssetId, EHoudiniEngineTaskType::AssetCooking, EHoudiniEngineTaskState::Processing);
			TaskDescription(TaskInfo, Task.ActorName, StatusString);
			FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);
		}

		// We want to yield.
		FPlatformProcess::Sleep(0.0f);
	}
}


void 
FHoudiniEngineScheduler::ProcessQueuedTasks()
{
	while(!bStopping)
	{
		FHoudiniEngineTask Task;

		while(true)
		{
			FHoudiniEngineTask Task;

			{
				FScopeLock ScopeLock(&CriticalSection);

				// We have no tasks left.
				if(PositionWrite == PositionRead)
				{
					break;
				}

				// Retrieve task.
				Task = Tasks[PositionRead];
				PositionRead++;

				// Wrap around if required.
				PositionRead &= (TaskCount - 1);
			}

			switch(Task.TaskType)
			{
				case EHoudiniEngineTaskType::AssetInstantiation:
				{
					TaskInstantiateAsset(Task);
					break;
				}

				case EHoudiniEngineTaskType::AssetCooking:
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
				// If we are running in single threaded mode, return so we don't block everything else.
				return;
			}
		}
	}
}


void
FHoudiniEngineScheduler::AddTask(const FHoudiniEngineTask& Task)
{
	FScopeLock ScopeLock(&CriticalSection);

	// Check if we need to grow our circular buffer.
	if(PositionWrite + 1 == PositionRead)
	{
		// Calculate next size (next power of two).
		uint32 NextTaskCount = FPlatformMath::RoundUpToPowerOfTwo(TaskCount + 1);

		// Allocate new buffer.
		FHoudiniEngineTask* Buffer = static_cast<FHoudiniEngineTask*>(FMemory::Malloc(NextTaskCount * sizeof(FHoudiniEngineTask)));

		if(!Buffer)
		{
			return;
		}

		// Zero memory.
		FMemory::Memset(Buffer, 0x0, NextTaskCount * sizeof(FHoudiniEngineTask));

		// Copy elements from old buffer to new one.
		if(PositionRead < PositionWrite)
		{
			FMemory::Memcpy(Buffer, Tasks + PositionRead, sizeof(FHoudiniEngineTask) * (PositionWrite - PositionRead));

			// Update index positions.
			PositionRead = 0;
			PositionWrite = PositionWrite - PositionRead;
		}
		else
		{
			FMemory::Memcpy(Buffer, Tasks + PositionRead, sizeof(FHoudiniEngineTask) * (TaskCount - PositionRead));
			FMemory::Memcpy(Buffer + TaskCount - PositionRead, Tasks, sizeof(FHoudiniEngineTask) * PositionWrite);

			// Update index positions.
			PositionRead = 0;
			PositionWrite = TaskCount - PositionRead + PositionWrite;
		}

		// Deallocate old buffer.
		FMemory::Free(Tasks);

		// Bookkeeping.
		Tasks = Buffer;
		TaskCount = NextTaskCount;
	}

	// Store task.
	Tasks[PositionWrite] = Task;
	PositionWrite++;

	// Wrap around if required.
	PositionWrite &= (TaskCount - 1);
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
