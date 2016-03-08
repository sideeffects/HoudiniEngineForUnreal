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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


const uint32
FHoudiniEngineScheduler::InitialTaskSize = 256u;


FHoudiniEngineScheduler::FHoudiniEngineScheduler() :
	Tasks(nullptr),
	PositionWrite(0u),
	PositionRead(0u),
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
FHoudiniEngineScheduler::TaskDescription(FHoudiniEngineTaskInfo& TaskInfo, const FString& ActorName,
	const FString& StatusString)
{
	FFormatNamedArguments Args;

	if(!ActorName.IsEmpty())
	{
		Args.Add(TEXT("AssetName"), FText::FromString(ActorName));
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));
		TaskInfo.StatusText =
			FText::Format(
				NSLOCTEXT("TaskDescription", "TaskDescriptionProgress", "({AssetName}) : ({AssetStatus})"), Args);
	}
	else
	{
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));
		TaskInfo.StatusText =
			FText::Format(NSLOCTEXT("TaskDescription", "TaskDescriptionProgress", "({AssetStatus})"), Args);
	}
}


void
FHoudiniEngineScheduler::TaskInstantiateAsset(const FHoudiniEngineTask& Task)
{
	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Instantiation Started. Component = 0x%x, HoudiniAsset = 0x%x, HapiGUID = %s"),
		Task.AssetComponent.Get(), Task.Asset.Get(), *Task.HapiGUID.ToString() );

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("TaskInstantiateAsset failed: %s"),
			*FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));

		AddResponseMessageTaskInfo(HAPI_RESULT_NOT_INITIALIZED, EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedInstantiationWithErrors, -1, Task, TEXT("HAPI is not initialized."));

		return;
	}

	if(!Task.Asset.IsValid())
	{
		// Asset is no longer valid, return.
		AddResponseMessageTaskInfo(HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedInstantiationWithErrors, -1, Task, TEXT("Asset is no longer valid."));

		return;
	}

	if(Task.AssetHapiName < 0)
	{
		// Asset is no longer valid, return.
		AddResponseMessageTaskInfo(HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedInstantiationWithErrors, -1, Task, TEXT("Asset name is invalid."));

		return;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	UHoudiniAsset* HoudiniAsset = Task.Asset.Get();
	int32 AssetCount = 0;
	HAPI_AssetId AssetId = -1;
	std::string AssetNameString;
	double LastUpdateTime;

	FHoudiniEngineString HoudiniEngineString(Task.AssetHapiName);
	if(HoudiniEngineString.ToStdString(AssetNameString))
	{
		// Translate asset name into Unreal string.
		FString AssetName = ANSI_TO_TCHAR(AssetNameString.c_str());

		// Initialize last update time.
		LastUpdateTime = FPlatformTime::Seconds();

		// We instantiate without cooking.
		Result = FHoudiniApi::InstantiateAsset(FHoudiniEngine::Get().GetSession(), &AssetNameString[0], false, &AssetId);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			AddResponseMessageTaskInfo(Result, EHoudiniEngineTaskType::AssetInstantiation,
				EHoudiniEngineTaskState::FinishedInstantiationWithErrors, -1, Task, TEXT("Error instantiating asset."));

			return;
		}

		// Add processing notification.
		FHoudiniEngineTaskInfo TaskInfo(HAPI_RESULT_SUCCESS, -1, EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::Processing);

		TaskInfo.bLoadedComponent = Task.bLoadedComponent;
		TaskDescription(TaskInfo, Task.ActorName, TEXT("Started Instantiation"));
		FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

		// We need to spin until instantiation is finished.
		while(true)
		{
			int Status = HAPI_STATE_STARTING_COOK;
			HOUDINI_CHECK_ERROR(&Result, FHoudiniApi::GetStatus(
				FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status) );

			if(HAPI_STATE_READY == Status)
			{
				// Cooking has been successful.
				AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetInstantiation,
					EHoudiniEngineTaskState::FinishedInstantiation, AssetId, Task,
					TEXT("Finished Instantiation."));

				break;
			}
			else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
			{
				// There was an error while instantiating.
				AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetInstantiation,
					EHoudiniEngineTaskState::FinishedInstantiationWithErrors, AssetId, Task,
					TEXT("Finished Instantiation with Errors."));

				break;
			}

			static const double NotificationUpdateFrequency = 0.5;
			if((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
			{
				// Reset update time.
				LastUpdateTime = FPlatformTime::Seconds();

				const FString& CookStateMessage = FHoudiniEngineUtils::GetCookState();

				AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetInstantiation,
					EHoudiniEngineTaskState::Processing, AssetId, Task,
					CookStateMessage);
			}

			// We want to yield.
			FPlatformProcess::Sleep(0.0f);
		}
	}
	else
	{
		AddResponseMessageTaskInfo(HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedInstantiationWithErrors, -1, Task, TEXT("Error retrieving asset name."));

		return;
	}
}


void
FHoudiniEngineScheduler::TaskCookAsset(const FHoudiniEngineTask& Task)
{
	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("TaskCookAsset failed: %s"),
			*FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));

		AddResponseMessageTaskInfo(HAPI_RESULT_NOT_INITIALIZED, EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::FinishedCookingWithErrors, -1, Task, TEXT("HAPI is not initialized."));

		return;
	}

	if(!Task.AssetComponent.IsValid())
	{
		// Asset component is no longer valid, return.
		AddResponseMessageTaskInfo(HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::FinishedCookingWithErrors, -1, Task, TEXT("Asset is no longer valid."));

		return;
	}

	// Retrieve asset id.
	HAPI_AssetId AssetId = Task.AssetComponent->GetAssetId();
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Cooking Started. Component = 0x%x, HoudiniAsset = 0x%x, ")
		TEXT("AssetId = %d, HapiGUID = %s"),
		Task.AssetComponent.Get(), Task.Asset.Get(), AssetId, *Task.HapiGUID.ToString());

	if(-1 == AssetId)
	{
		// We have an invalid asset id.
		HOUDINI_LOG_ERROR(TEXT("TaskCookAsset failed: Invalid Asset Id."));

		AddResponseMessageTaskInfo(HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::FinishedCookingWithErrors, -1, Task, TEXT("Asset has invalid id."));

		return;
	}

	Result = FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		AddResponseMessageTaskInfo(Result, EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::FinishedCookingWithErrors, AssetId, Task, TEXT("Error cooking asset."));

		return;
	}

	// Add processing notification.
	AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
		EHoudiniEngineTaskState::Processing, AssetId, Task, TEXT("Started Cooking"));

	// Initialize last update time.
	double LastUpdateTime = FPlatformTime::Seconds();

	// We need to spin until cooking is finished.
	while(true)
	{
		int32 Status = HAPI_STATE_STARTING_COOK;
		HOUDINI_CHECK_ERROR(&Result, FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status) );

		if(!Task.AssetComponent.IsValid())
		{
			AddResponseMessageTaskInfo(HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::FinishedCookingWithErrors, AssetId, Task,
				TEXT("Component is no longer valid."));

			break;
		}

		if(HAPI_STATE_READY == Status)
		{
			// Cooking has been successful.
			AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::FinishedCooking, AssetId, Task,
				TEXT("Finished Cooking"));

			break;
		}
		else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status || HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
		{
			// There was an error while instantiating.
			AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::FinishedCookingWithErrors, AssetId, Task,
				TEXT("Finished Cooking with Errors"));

			break;
		}

		static const double NotificationUpdateFrequency = 0.5;
		if((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
		{
			// Reset update time.
			LastUpdateTime = FPlatformTime::Seconds();

			// Retrieve status string.
			const FString& CookStateMessage = FHoudiniEngineUtils::GetCookState();

			AddResponseMessageTaskInfo(HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::Processing, AssetId, Task,
				CookStateMessage);
		}

		// We want to yield.
		FPlatformProcess::Sleep(0.0f);
	}
}


void
FHoudiniEngineScheduler::TaskDeleteAsset(const FHoudiniEngineTask& Task)
{
	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Destruction Started. Component = 0x%x, HoudiniAsset = 0x%x, ")
		TEXT("AssetId = %d, HapiGUID = %s"),
		Task.AssetComponent.Get(), Task.Asset.Get(), Task.AssetId, *Task.HapiGUID.ToString() );

	if(FHoudiniEngineUtils::IsHoudiniAssetValid(Task.AssetId))
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(Task.AssetId);
	}

	// We do not insert task info as this is a fire and forget operation.
	// At this point component most likely does not exist.
}


void
FHoudiniEngineScheduler::AddResponseTaskInfo(
	HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType, EHoudiniEngineTaskState::Type TaskState,
	HAPI_AssetId AssetId, const FHoudiniEngineTask& Task)
{
	FHoudiniEngineTaskInfo TaskInfo(Result, AssetId, TaskType, TaskState);
	FString StatusString = FHoudiniEngineUtils::GetErrorDescription();

	TaskInfo.bLoadedComponent = Task.bLoadedComponent;
	TaskDescription(TaskInfo, Task.ActorName, StatusString);
	FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);
}


void
FHoudiniEngineScheduler::AddResponseMessageTaskInfo(
	HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType, EHoudiniEngineTaskState::Type TaskState,
	HAPI_AssetId AssetId, const FHoudiniEngineTask& Task, const FString& ErrorMessage)
{
	FHoudiniEngineTaskInfo TaskInfo(Result, AssetId, TaskType, TaskState);

	TaskInfo.bLoadedComponent = Task.bLoadedComponent;
	TaskDescription(TaskInfo, Task.ActorName, ErrorMessage);
	FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);
}


void
FHoudiniEngineScheduler::ProcessQueuedTasks()
{
	while(!bStopping)
	{
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

			bool bTaskProcessed = true;

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

				case EHoudiniEngineTaskType::AssetDeletion:
				{
					TaskDeleteAsset(Task);
					break;
				}

				default:
				{
					bTaskProcessed = false;
					break;
				}
			}

			if(!bTaskProcessed)
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
		FHoudiniEngineTask* Buffer =
			static_cast<FHoudiniEngineTask*>(FMemory::Malloc(NextTaskCount * sizeof(FHoudiniEngineTask)));

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
