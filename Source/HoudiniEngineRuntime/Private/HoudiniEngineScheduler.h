/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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
        FSingleThreadRunnable * GetSingleThreadInterface() override;

    /** FSingleThreadRunnable methods. **/
    public:

        virtual void Tick() override;

    public:

        /** Add a task. **/
        void AddTask( const FHoudiniEngineTask & Task );

        /** Add instantiation response task info. **/
        void AddResponseTaskInfo(
            HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType,
            EHoudiniEngineTaskState::Type TaskState,
            HAPI_NodeId AssetId, const FHoudiniEngineTask & Task );

        void AddResponseMessageTaskInfo(
            HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType,
            EHoudiniEngineTaskState::Type TaskState, HAPI_NodeId AssetId, const FHoudiniEngineTask & Task,
            const FString & ErrorMessage );

    protected:

        /** Process queued tasks. **/
        void ProcessQueuedTasks();

        /** Task : instantiate an asset. **/
        void TaskInstantiateAsset( const FHoudiniEngineTask & Task );

        /** Task : cook an asset. **/
        void TaskCookAsset( const FHoudiniEngineTask & Task );

        /** Create description of task's state. **/
        void TaskDescription( FHoudiniEngineTaskInfo & Task, const FString & ActorName, const FString & StatusString );

        /** Delete an asset. **/
        void TaskDeleteAsset( const FHoudiniEngineTask & Task );

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
