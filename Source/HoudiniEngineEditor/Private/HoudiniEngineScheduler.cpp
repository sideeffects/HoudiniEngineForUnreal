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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineScheduler.h"


FHoudiniEngineScheduler::FHoudiniEngineScheduler() :
	bStopping(false)
{

}


FHoudiniEngineScheduler::~FHoudiniEngineScheduler()
{

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


void
FHoudiniEngineScheduler::AddTask(const FHoudiniEngineSchedulerTask& Task)
{
	Tasks.Enqueue(Task);
}


void
FHoudiniEngineScheduler::ProcessQueuedTasks()
{
	while(!bStopping)
	{
		if(!Tasks.IsEmpty())
		{
			// Grab task.
			FHoudiniEngineSchedulerTask Task;
			Tasks.Dequeue(Task);

			switch(Task.TaskType)
			{
				case EHoudiniEngineSchedulerTaskType::AssetInstantiation:
				{
					TaskInstantiateAsset(Task);
					break;
				}

				case EHoudiniEngineSchedulerTaskType::AssetCooking:
				{
					TaskCookAsset(Task);
					break;
				}

				case EHoudiniEngineSchedulerTaskType::AssetDeletion:
				{
					TaskDeleteAsset(Task);
					break;
				}

				default:
				{
					check(false);
				}
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
FHoudiniEngineScheduler::TaskInstantiateAsset(const FHoudiniEngineSchedulerTask& Task)
{

}


void
FHoudiniEngineScheduler::TaskCookAsset(const FHoudiniEngineSchedulerTask& Task)
{

}


void
FHoudiniEngineScheduler::TaskDeleteAsset(const FHoudiniEngineSchedulerTask& Task)
{

}
