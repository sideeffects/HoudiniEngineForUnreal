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

#pragma once

#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"


class FHoudiniEngineScheduler : public FRunnable, FSingleThreadRunnable
{

public:

	FHoudiniEngineScheduler();
	virtual ~FHoudiniEngineScheduler();

/** FRunnable methods. **/
public:

	virtual uint32 Run() override;
	virtual void Stop() override;
	FSingleThreadRunnable* GetSingleThreadInterface() override;

/** FSingleThreadRunnable methods. **/
public:

	virtual void Tick() override;

public:

	/** Add a task. **/
	void AddTask(const FHoudiniEngineTask& Task);

	/** Add instantiation response task info. **/
	void AddResponseTaskInfo(HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType,
		EHoudiniEngineTaskState::Type TaskState, HAPI_AssetId AssetId, const FHoudiniEngineTask& Task);

	void AddResponseMessageTaskInfo(HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType,
		EHoudiniEngineTaskState::Type TaskState, HAPI_AssetId AssetId, const FHoudiniEngineTask& Task,
		const FString& ErrorMessage);

protected:

	/** Process queued tasks. **/
	void ProcessQueuedTasks();

	/** Task : instantiate an asset. **/
	void TaskInstantiateAsset(const FHoudiniEngineTask& Task);

	/** Task : cook an asset. **/
	void TaskCookAsset(const FHoudiniEngineTask& Task);

	/** Create description of task's state. **/
	void TaskDescription(FHoudiniEngineTaskInfo& Task, const FString& ActorName, const FString& StatusString);

	/** Delete an asset. **/
	void TaskDeleteAsset(const FHoudiniEngineTask& Task);

protected:

	/** Initial number of tasks in our circular queue. **/
	static const uint32 InitialTaskSize;

protected:

	/** Synchronization primitive. **/
	FCriticalSection CriticalSection;

	/** List of scheduled tasks. **/
	FHoudiniEngineTask* Tasks;

	/** Head of the circular queue. **/
	uint32 PositionWrite;

	/** Tail of the circular queue. **/
	uint32 PositionRead;

	/** Size of the circular queue. **/
	uint32 TaskCount;

	/** Stopping flag. **/
	bool bStopping;
};
