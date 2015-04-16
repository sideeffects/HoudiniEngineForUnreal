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

#include "HoudiniEngineSchedulerTask.h"


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
	void AddTask(const FHoudiniEngineSchedulerTask& Task);

protected:

	/** Process queued tasks. **/
	void ProcessQueuedTasks();

	/** Task : instantiate an asset. **/
	void TaskInstantiateAsset(const FHoudiniEngineSchedulerTask& Task);

	/** Task : cook an asset. **/
	void TaskCookAsset(const FHoudiniEngineSchedulerTask& Task);

	/** Delete an asset. **/
	void TaskDeleteAsset(const FHoudiniEngineSchedulerTask& Task);

protected:

	/** List of tasks. **/
	TQueue<FHoudiniEngineSchedulerTask> Tasks;

	/** Stopping flag. **/
	bool bStopping;
};
