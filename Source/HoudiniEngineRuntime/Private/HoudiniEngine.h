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
#include "IHoudiniEngine.h"
#include "HoudiniEngineTaskInfo.h"

class UStaticMesh;
class FRunnableThread;
class FHoudiniEngineScheduler;

class HOUDINIENGINERUNTIME_API FHoudiniEngine : public IHoudiniEngine
{
    public:
        FHoudiniEngine();

    /** IModuleInterface methods. **/
    public:

        virtual void StartupModule() override;
        virtual void ShutdownModule() override;

    /** IHoudiniEngine methods. **/
    public:

        virtual UStaticMesh * GetHoudiniLogoStaticMesh() const override;
        virtual UMaterial * GetHoudiniDefaultMaterial() const override;
        virtual UHoudiniAsset * GetHoudiniBgeoAsset() const override;

#if WITH_EDITOR

        virtual TSharedPtr< FSlateDynamicImageBrush > GetHoudiniLogoBrush() const override;

#endif

        virtual bool CheckHapiVersionMismatch() const override;
        virtual const FString & GetLibHAPILocation() const override;
        virtual void AddTask( const FHoudiniEngineTask & Task ) override;
        virtual void AddTaskInfo( const FGuid HapIGUID, const FHoudiniEngineTaskInfo & TaskInfo ) override;
        virtual void RemoveTaskInfo( const FGuid HapIGUID ) override;
        virtual bool RetrieveTaskInfo( const FGuid HapIGUID, FHoudiniEngineTaskInfo & TaskInfo ) override;
        virtual HAPI_Result GetHapiState() const override;
        virtual void SetHapiState( HAPI_Result Result ) override;
        virtual const HAPI_Session * GetSession() const override;

    public:

        /** App identifier string. **/
        static const FName HoudiniEngineAppIdentifier;

    public:

        /** Return singleton instance of Houdini Engine, used internally. **/
        static FHoudiniEngine & Get();

        /** Return true if singleton instance has been created. **/
        static bool IsInitialized();

    private:

        /** Singleton instance of Houdini Engine. **/
        static FHoudiniEngine * HoudiniEngineInstance;

    private:

        /** Static mesh used for Houdini logo rendering. **/
        TWeakObjectPtr<UStaticMesh> HoudiniLogoStaticMesh;

        /** Material used as default material. **/
        TWeakObjectPtr<UMaterial> HoudiniDefaultMaterial;

        /** Houdini digital asset used for loading the bgeo files. **/
        TWeakObjectPtr<UHoudiniAsset> HoudiniBgeoAsset;

#if WITH_EDITOR

        /** Houdini logo brush. **/
        TSharedPtr< FSlateDynamicImageBrush > HoudiniLogoBrush;

#endif

        /** Synchronization primitive. **/
        FCriticalSection CriticalSection;

        /** Map of task statuses. **/
        TMap< FGuid, FHoudiniEngineTaskInfo > TaskInfos;

        /** Thread used to execute the scheduler. **/
        FRunnableThread * HoudiniEngineSchedulerThread;

        /** Scheduler used to schedule HAPI instantiation and cook tasks. **/
        FHoudiniEngineScheduler * HoudiniEngineScheduler;

        /** Location of libHAPI binary. **/
        FString LibHAPILocation;

        /** Keep current state of HAPI. **/
        HAPI_Result HAPIState;

        /** Is set to true when mismatch between defined and running HAPI versions is detected. **/
        bool bHAPIVersionMismatch;

        /** The Houdini Engine session. **/
        HAPI_Session Session;
};
