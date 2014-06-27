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

class FHoudiniEngineScheduler : public FRunnable, FSingleThreadRunnable
{

public:

	FHoudiniEngineScheduler();
	virtual ~FHoudiniEngineScheduler();

public: /** FRunnable methods. **/

	virtual uint32 Run() OVERRIDE;
	virtual void Stop() OVERRIDE;
	FSingleThreadRunnable* GetSingleThreadInterface() OVERRIDE;

public: /** FSingleThreadRunnable methods. **/

	virtual void Tick() OVERRIDE;

public:

	void AddTask(const FHoudiniEngineTask& Task);

protected:

	/** Process queued tasks. **/
	void ProcessQueuedTasks();

	/** Task : instantiate an asset. **/
	void TaskInstantiateAsset(const FHoudiniEngineTask& Task);

	/** Task : cook an asset. **/
	void TaskCookAsset(const FHoudiniEngineTask& Task);

protected:

	/** Maximum number of tasks in our circular queue. **/
	static const uint32 MaximumTaskSize;

protected:

	/** List of scheduled tasks. **/
	TCircularQueue<FHoudiniEngineTask> Tasks;

	/** Stopping flag. **/
	bool bStopping;
};
