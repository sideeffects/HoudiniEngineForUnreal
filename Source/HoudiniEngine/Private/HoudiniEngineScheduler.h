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

#pragma once

#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"

class FHoudiniEngineScheduler : public FRunnable, FSingleThreadRunnable
{
public:

	FHoudiniEngineScheduler();
	virtual ~FHoudiniEngineScheduler();

	// FRunnable methods.
	virtual uint32 Run() override;
	virtual void Stop() override;
	FSingleThreadRunnable * GetSingleThreadInterface() override;

	// FSingleThreadRunnable methods.
	virtual void Tick() override;

	bool HasPendingTasks();

	// Adds a task.
	void AddTask(const FHoudiniEngineTask & Task);

	// Adds instantiation response task info.
	void AddResponseTaskInfo(
		HAPI_Result Result, 
		EHoudiniEngineTaskType TaskType,
		EHoudiniEngineTaskState TaskState,
		HAPI_NodeId AssetId, 
		const FHoudiniEngineTask & Task);

	void AddResponseMessageTaskInfo(
		HAPI_Result Result,
		EHoudiniEngineTaskType TaskType,
		EHoudiniEngineTaskState TaskState,
		HAPI_NodeId AssetId, 
		const FHoudiniEngineTask & Task,
		const FString & ErrorMessage);

protected:

	// Process queued tasks. 
	void ProcessQueuedTasks();

	// Task : instantiate an asset. 
	void TaskInstantiateAsset(const FHoudiniEngineTask & Task);

	// Task : cook an asset. 
	void TaskCookAsset(const FHoudiniEngineTask & Task);

	// Create description of task's state. 
	void TaskDescription(
		FHoudiniEngineTaskInfo & Task,
		const FString & ActorName,
		const FString & StatusString);

	// Delete an asset. 
	void TaskDeleteAsset(const FHoudiniEngineTask & Task);

	// Process the result of a sucesfull cook
	void TaskProccessAsset(const FHoudiniEngineTask & Task);

private:

	// Initial number of tasks in our circular queue. 
	static const uint32 InitialTaskSize;

	// Frequency update (sleep time between each update)
	static const float UpdateFrequency;

	// Synchronization primitive. 
	FCriticalSection CriticalSection;

	// Event to wake up thread when tasks become available. 
	FEventRef WakeUpEvent;

	// List of scheduled tasks. 
	FHoudiniEngineTask* Tasks;

	// Head of the circular queue. 
	uint32 PositionWrite;

	// Tail of the circular queue. 
	uint32 PositionRead;

	// Size of the circular queue. 
	uint32 TaskCount;

	// Stopping flag. 
	bool bStopping;
};
