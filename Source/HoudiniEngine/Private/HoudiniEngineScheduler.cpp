/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniEngineScheduler.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"

const uint32
FHoudiniEngineScheduler::InitialTaskSize = 256u;

// Update frequency in (ms) for polling the scheduler
const float
FHoudiniEngineScheduler::UpdateFrequency = 0.1f;

FHoudiniEngineScheduler::FHoudiniEngineScheduler()
	: WakeUpEvent(FEventRef(EEventMode::AutoReset))
	, Tasks(nullptr)
	, PositionWrite(0u)
	, PositionRead(0u)
	, bStopping(false)
{
	//  Make sure size is power of two.
	TaskCount = FPlatformMath::RoundUpToPowerOfTwo(FHoudiniEngineScheduler::InitialTaskSize);

	if (TaskCount)
	{
		// Allocate buffer to store all tasks.
		Tasks = static_cast<FHoudiniEngineTask *>(FMemory::Malloc(TaskCount * sizeof(FHoudiniEngineTask)));

		if (Tasks)
		{
			// Zero memory.
			FMemory::Memset(Tasks, 0x0, TaskCount * sizeof(FHoudiniEngineTask));
		}
	}
}

FHoudiniEngineScheduler::~FHoudiniEngineScheduler()
{
	if (TaskCount)
	{
		FMemory::Free(Tasks);
		Tasks = nullptr;
	}
}

void
FHoudiniEngineScheduler::TaskDescription(
	FHoudiniEngineTaskInfo & TaskInfo,
	const FString & ActorName,
	const FString & StatusString)
{
	FFormatNamedArguments Args;

	if (!ActorName.IsEmpty())
	{
		Args.Add(TEXT("AssetName"), FText::FromString(ActorName));
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));
		TaskInfo.StatusText =
			FText::Format(NSLOCTEXT("TaskDescription", "TaskDescriptionProgress", "{AssetName} :\n{AssetStatus}"), Args);
	}
	else
	{
		Args.Add(TEXT("AssetStatus"), FText::FromString(StatusString));
		TaskInfo.StatusText =
			FText::Format(NSLOCTEXT("TaskDescription", "TaskDescriptionProgress", "{AssetStatus}"), Args);
	}
}

void
FHoudiniEngineScheduler::TaskInstantiateAsset(const FHoudiniEngineTask & Task)
{
	FString AssetN;
	FHoudiniEngineString(Task.AssetHapiName).ToFString(AssetN);

	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Instantiation Started for %s: Asset=%s, HoudiniAsset = 0x%x"),
		*Task.ActorName, *AssetN, Task.Asset.Get());

	if (!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(
			TEXT("TaskInstantiateAsset failed for %s: %s"),
			*Task.ActorName,
			*FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));

		AddResponseMessageTaskInfo(
			HAPI_RESULT_NOT_INITIALIZED, 
			EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("HAPI is not initialized."));

		return;
	}

	if (!Task.Asset.IsValid())
	{
		// Asset is no longer valid, return.
		AddResponseMessageTaskInfo(
			HAPI_RESULT_FAILURE, 
			EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("Asset is no longer valid."));

		return;
	}

	if (Task.AssetHapiName < 0)
	{
		// Asset is no longer valid, return.
		AddResponseMessageTaskInfo(
			HAPI_RESULT_FAILURE, 
			EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("Asset name is invalid."));

		return;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	int32 AssetCount = 0;
	HAPI_NodeId AssetId = -1;
	std::string AssetNameString;
	double LastUpdateTime;

	FHoudiniEngineString HoudiniEngineString(Task.AssetHapiName);
	if (!HoudiniEngineString.ToStdString(AssetNameString))
	{
		AddResponseMessageTaskInfo(
			HAPI_RESULT_FAILURE,
			EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("Error retrieving asset name."));

		return;
	}

	// Translate asset name into Unreal string.
	FString AssetName = ANSI_TO_TCHAR(AssetNameString.c_str());

	// Initialize last update time.
	LastUpdateTime = FPlatformTime::Seconds();

	// We instantiate without cooking.
	Result = FHoudiniApi::CreateNode(
		FHoudiniEngine::Get().GetSession(), -1, &AssetNameString[0], nullptr, false, &AssetId);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		AddResponseMessageTaskInfo(
			Result, 
			EHoudiniEngineTaskType::AssetInstantiation,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("Error instantiating asset."));

		return;
	}

	// Add processing notification.
	FHoudiniEngineTaskInfo TaskInfo(
		HAPI_RESULT_SUCCESS, -1, 
		EHoudiniEngineTaskType::AssetInstantiation,
		EHoudiniEngineTaskState::Working);

	//TaskInfo.bLoadedComponent = Task.bLoadedComponent;
	TaskDescription(TaskInfo, Task.ActorName, TEXT("Started Instantiation"));
	FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);

	// We need to spin until instantiation is finished.
	while (true)
	{
		int Status = HAPI_STATE_STARTING_COOK;
		HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status));

		if (Status == HAPI_STATE_READY)
		{
			// Cooking has been successful.
			AddResponseMessageTaskInfo(
				HAPI_RESULT_SUCCESS, 
				EHoudiniEngineTaskType::AssetInstantiation,
				EHoudiniEngineTaskState::Success, AssetId, Task,
				TEXT("Finished Instantiation."));

			break;
		}
		else if (Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
		{
			// There was an error while instantiating.
			FString CookResultString = FHoudiniEngineUtils::GetCookResult();
			int32 CookResult = static_cast<int32>(HAPI_RESULT_SUCCESS);
			FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_RESULT, &CookResult);

			EHoudiniEngineTaskState TaskStateResult = EHoudiniEngineTaskState::FinishedWithFatalError;
			if (Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
				TaskStateResult = EHoudiniEngineTaskState::FinishedWithError;

			AddResponseMessageTaskInfo(
				static_cast<HAPI_Result>(CookResult), 
				EHoudiniEngineTaskType::AssetInstantiation,	
				TaskStateResult,
				AssetId, Task,
				FString::Printf(TEXT("Finished Instantiation with Errors: %s"), *CookResultString));

			break;
		}

		if (Result == HAPI_RESULT_FAILURE)
		{
			// There was an error while getting the status - likely we've lost the session.
			// Log an error and ensure we break cleanly in that case
			HOUDINI_LOG_ERROR(TEXT("Unable to instantiate asset: %s - session failed"), *Task.ActorName);

			EHoudiniEngineTaskState TaskStateResult = EHoudiniEngineTaskState::FinishedWithFatalError;

			AddResponseMessageTaskInfo(
				static_cast<HAPI_Result>(Result),
				EHoudiniEngineTaskType::AssetInstantiation,
				TaskStateResult,
				AssetId, Task,
				TEXT("Unable to instantiate the asset: session failed."));

			return;
		}

		static const double NotificationUpdateFrequency = 0.5;
		if ((FPlatformTime::Seconds() - LastUpdateTime) >= NotificationUpdateFrequency)
		{
			// Reset update time.
			LastUpdateTime = FPlatformTime::Seconds();
			const FString& CookStateMessage = FHoudiniEngineUtils::GetCookState();

			AddResponseMessageTaskInfo(
				HAPI_RESULT_SUCCESS,
				EHoudiniEngineTaskType::AssetInstantiation,
				EHoudiniEngineTaskState::Working,
				AssetId, Task, CookStateMessage);
		}

		// We want to yield.
		FPlatformProcess::SleepNoStats(UpdateFrequency);
	}
}

void
FHoudiniEngineScheduler::TaskCookAsset(const FHoudiniEngineTask & Task)
{
	if (!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(
			TEXT("TaskCookAsset failed for %s: %s"),
			*Task.ActorName,
			*FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));

		AddResponseMessageTaskInfo(
			HAPI_RESULT_NOT_INITIALIZED, 
			EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("HAPI is not initialized."));

		return;
	}

	// Retrieve asset id.
	HAPI_NodeId AssetId = Task.AssetId;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Cooking Started for %s., AssetId = %d"),
		*Task.ActorName, AssetId);

	if (AssetId == -1)
	{
		// We have an invalid asset id.
		HOUDINI_LOG_ERROR(TEXT("TaskCookAsset failed for %s: Invalid Asset Id."), *Task.ActorName);

		AddResponseMessageTaskInfo(
			HAPI_RESULT_FAILURE, 
			EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("Asset has invalid id."));

		return;
	}

	// Get the extra node Ids that we want to process if needed
	TArray<HAPI_NodeId> NodesToCook;
	NodesToCook.Add(AssetId);
	for (auto& CurrentNodeId : Task.OtherNodeIds)
	{
		if (CurrentNodeId < 0)
			continue;

		NodesToCook.AddUnique(CurrentNodeId);
	}

	// Default CookOptions
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();

	EHoudiniEngineTaskState GlobalTaskResult = EHoudiniEngineTaskState::Success;
	for (auto& CurrentNodeId : NodesToCook)
	{
		Result = FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), CurrentNodeId, &CookOptions);
		if (Result != HAPI_RESULT_SUCCESS)
		{
			AddResponseMessageTaskInfo(
				Result,
				EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::FinishedWithFatalError,
				AssetId,
				Task,
				TEXT("Error cooking asset."));

			return;
		}

		// Add processing notification.
		AddResponseMessageTaskInfo(
			HAPI_RESULT_SUCCESS,
			EHoudiniEngineTaskType::AssetCooking,
			EHoudiniEngineTaskState::Working,
			AssetId, Task, TEXT("Started Cooking"));

		// Initialize last update time.
		double LastUpdateTime = FPlatformTime::Seconds();

		// We need to spin until cooking is finished.
		while (true)
		{
			int32 Status = HAPI_STATE_STARTING_COOK;
			HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::GetStatus(
				FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status));

			if (Status == HAPI_STATE_READY)
			{
				// Cooking has been successful.
				// Break to process the next node
				break;
			}
			else if (Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
			{
				GlobalTaskResult = EHoudiniEngineTaskState::FinishedWithFatalError;

				if (Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
					GlobalTaskResult = EHoudiniEngineTaskState::FinishedWithError;

				break;
			}

			if (Result == HAPI_RESULT_FAILURE)
			{
				// There was an error while getting the status - likely we've lost the session.
				// Log an error and ensure we break cleanly in that case
				HOUDINI_LOG_ERROR(TEXT("Unable to cook asset: %s - session failed"), *Task.ActorName);
				GlobalTaskResult = EHoudiniEngineTaskState::FinishedWithFatalError;

				// There was an error while cooking.
				AddResponseMessageTaskInfo(
					HAPI_RESULT_FAILURE,
					EHoudiniEngineTaskType::AssetCooking,
					EHoudiniEngineTaskState::FinishedWithFatalError,
					AssetId,
					Task,
					TEXT("Unable to cook - session failed"));

				return;
			}

			static const double NotificationUpdateFrequency = 0.5;
			if (FPlatformTime::Seconds() - LastUpdateTime >= NotificationUpdateFrequency)
			{
				// Reset update time.
				LastUpdateTime = FPlatformTime::Seconds();

				// Retrieve status string.
				const FString & CookStateMessage = FHoudiniEngineUtils::GetCookState();

				AddResponseMessageTaskInfo(
					HAPI_RESULT_SUCCESS,
					EHoudiniEngineTaskType::AssetCooking,
					EHoudiniEngineTaskState::Working,
					AssetId, Task, CookStateMessage);
			}

			// We want to yield.
			FPlatformProcess::SleepNoStats(UpdateFrequency);
		}
	}	

	switch (GlobalTaskResult)
	{
		case EHoudiniEngineTaskState::Success:
		{
			// Cooking has been successful
			AddResponseMessageTaskInfo(
				HAPI_RESULT_SUCCESS,
				EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::Success,
				AssetId,
				Task,
				TEXT("Finished Cooking"));
		}
		break;

		case EHoudiniEngineTaskState::FinishedWithError:
		{
			// There was an error while Cooking.
			AddResponseMessageTaskInfo(
				HAPI_RESULT_SUCCESS,
				EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::FinishedWithError,
				AssetId,
				Task,
				TEXT("Finished Cooking with Errors"));
		}
		break;

		case EHoudiniEngineTaskState::FinishedWithFatalError:
		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::None:
		case EHoudiniEngineTaskState::Working:
		{
			// There was an error while cooking.
			AddResponseMessageTaskInfo(
				HAPI_RESULT_SUCCESS,
				EHoudiniEngineTaskType::AssetCooking,
				EHoudiniEngineTaskState::FinishedWithFatalError,
				AssetId,
				Task,
				TEXT("Finished Cooking with Fatal Errors"));
		}
		break;
	}
}

void
FHoudiniEngineScheduler::TaskDeleteAsset(const FHoudiniEngineTask & Task)
{
	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Destruction Started for %s. ")
		TEXT("AssetId = %d"),
		*Task.ActorName, Task.AssetId);

	if (FHoudiniEngineUtils::IsHoudiniNodeValid(Task.AssetId))
		FHoudiniEngineUtils::DestroyHoudiniAsset(Task.AssetId);

	// We do not insert task info as this is a fire and forget operation.
	// At this point component most likely does not exist.
}

void
FHoudiniEngineScheduler::AddResponseTaskInfo(
	HAPI_Result Result, EHoudiniEngineTaskType TaskType, EHoudiniEngineTaskState TaskState,
	HAPI_NodeId AssetId, const FHoudiniEngineTask & Task)
{
	FHoudiniEngineTaskInfo TaskInfo(Result, AssetId, TaskType, TaskState);
	FString StatusString = FHoudiniEngineUtils::GetErrorDescription();

	//TaskInfo.bLoadedComponent = Task.bLoadedComponent;

	TaskDescription(TaskInfo, Task.ActorName, StatusString);
	FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);
}

void
FHoudiniEngineScheduler::AddResponseMessageTaskInfo(
	HAPI_Result Result, EHoudiniEngineTaskType TaskType, EHoudiniEngineTaskState TaskState,
	HAPI_NodeId AssetId, const FHoudiniEngineTask & Task, const FString & ErrorMessage)
{
	FHoudiniEngineTaskInfo TaskInfo(Result, AssetId, TaskType, TaskState);

	//TaskInfo.bLoadedComponent = Task.bLoadedComponent;

	TaskDescription(TaskInfo, Task.ActorName, ErrorMessage);
	FHoudiniEngine::Get().AddTaskInfo(Task.HapiGUID, TaskInfo);
}

void
FHoudiniEngineScheduler::ProcessQueuedTasks()
{
	while (!bStopping)
	{
		while (true)
		{
			FHoudiniEngineTask Task;

			{
				FScopeLock ScopeLock(&CriticalSection);

				// We have no tasks left.
				if (PositionWrite == PositionRead)
					break;

				// Retrieve task.
				Task = Tasks[PositionRead];
				PositionRead++;

				// Wrap around if required.
				PositionRead &= (TaskCount - 1);
			}

			bool bTaskProcessed = true;

			switch (Task.TaskType)
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

				case EHoudiniEngineTaskType::AssetProcess:
				{
					TaskProccessAsset(Task);
					break;
				}

				default:
				{
					bTaskProcessed = false;
					break;
				}
			}

			if (!bTaskProcessed)
				break;
		}

		if (FPlatformProcess::SupportsMultithreading())
		{
			// We want to yield for a bit.
			WakeUpEvent->Wait(UpdateFrequency * 1000.0f);
		}
		else
		{
			// If we are running in single threaded mode, return so we don't block everything else.
			return;
		}
	}
}

void
FHoudiniEngineScheduler::TaskProccessAsset(const FHoudiniEngineTask & Task)
{
	if (!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(
			TEXT("TaskProccessAsset failed for %s: %s"),
			*Task.ActorName,
			*FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));

		AddResponseMessageTaskInfo(
			HAPI_RESULT_NOT_INITIALIZED,
			EHoudiniEngineTaskType::AssetProcess,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("HAPI is not initialized."));

		return;
	}

	// Retrieve asset id.
	HAPI_NodeId AssetId = Task.AssetId;
	if (AssetId == -1)
	{
		// We have an invalid asset id.
		HOUDINI_LOG_ERROR(TEXT("TaskProcessAsset failed for %s: Invalid Asset Id."), *Task.ActorName);

		AddResponseMessageTaskInfo(
			HAPI_RESULT_FAILURE,
			EHoudiniEngineTaskType::AssetProcess,
			EHoudiniEngineTaskState::FinishedWithFatalError,
			-1, Task, TEXT("Asset has invalid id."));

		return;
	}
		
	HOUDINI_LOG_MESSAGE(
		TEXT("HAPI Asynchronous Processing started for %s., AssetId = %d"),
		*Task.ActorName, AssetId);

	// Add processing notification.
	AddResponseMessageTaskInfo(
		HAPI_RESULT_SUCCESS,
		EHoudiniEngineTaskType::AssetProcess,
		EHoudiniEngineTaskState::Working,
		AssetId, Task, TEXT("Started Cooking"));

	//


	// TODO: Process results!
}

bool FHoudiniEngineScheduler::HasPendingTasks()
{
	FScopeLock ScopeLock(&CriticalSection);
	return (PositionWrite != PositionRead);
}

void
FHoudiniEngineScheduler::AddTask(const FHoudiniEngineTask & Task)
{
	FScopeLock ScopeLock(&CriticalSection);

	// Check if we need to grow our circular buffer.
	if (PositionWrite + 1 == PositionRead)
	{
		// Calculate next size (next power of two).
		uint32 NextTaskCount = FPlatformMath::RoundUpToPowerOfTwo(TaskCount + 1);

		// Allocate new buffer.
		FHoudiniEngineTask * Buffer = static_cast<FHoudiniEngineTask *>(
			FMemory::Malloc(NextTaskCount * sizeof(FHoudiniEngineTask)));

		if (!Buffer)
			return;

		// Zero memory.
		FMemory::Memset(Buffer, 0x0, NextTaskCount * sizeof(FHoudiniEngineTask));

		// Copy elements from old buffer to new one.
		if (PositionRead < PositionWrite)
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

	// Wake up the thread to process the task.
	WakeUpEvent->Trigger();
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

FSingleThreadRunnable *
FHoudiniEngineScheduler::GetSingleThreadInterface()
{
	return this;
}
