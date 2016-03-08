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

	virtual UStaticMesh* GetHoudiniLogoStaticMesh() const override;
	virtual UMaterial* GetHoudiniDefaultMaterial() const override;
	virtual UHoudiniAsset* GetHoudiniBgeoAsset() const override;

#if WITH_EDITOR

	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const override;

#endif

	virtual bool CheckHapiVersionMismatch() const override;
	virtual const FString& GetLibHAPILocation() const override;
	virtual void AddTask(const FHoudiniEngineTask& Task) override;
	virtual void AddTaskInfo(const FGuid HapIGUID, const FHoudiniEngineTaskInfo& TaskInfo) override;
	virtual void RemoveTaskInfo(const FGuid HapIGUID) override;
	virtual bool RetrieveTaskInfo(const FGuid HapIGUID, FHoudiniEngineTaskInfo& TaskInfo) override;
	virtual HAPI_Result GetHapiState() const override;
	virtual void SetHapiState(HAPI_Result Result) override;
	virtual const HAPI_Session* GetSession() const override;

public:

	/** App identifier string. **/
	static const FName HoudiniEngineAppIdentifier;

public:

	/** Return singleton instance of Houdini Engine, used internally. **/
	static FHoudiniEngine& Get();

	/** Return true if singleton instance has been created. **/
	static bool IsInitialized();

private:

	/** Singleton instance of Houdini Engine. **/
	static FHoudiniEngine* HoudiniEngineInstance;

private:

	/** Static mesh used for Houdini logo rendering. **/
	UStaticMesh* HoudiniLogoStaticMesh;

	/** Material used as default material. **/
	UMaterial* HoudiniDefaultMaterial;

	/** Houdini digital asset used for loading the bgeo files. **/
	UHoudiniAsset* HoudiniBgeoAsset;

#if WITH_EDITOR

	/** Houdini logo brush. **/
	TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;

#endif

	/** Synchronization primitive. **/
	FCriticalSection CriticalSection;

	/** Map of task statuses. **/
	TMap<FGuid, FHoudiniEngineTaskInfo> TaskInfos;

	/** Thread used to execute the scheduler. **/
	FRunnableThread* HoudiniEngineSchedulerThread;

	/** Scheduler used to schedule HAPI instantiation and cook tasks. **/
	FHoudiniEngineScheduler* HoudiniEngineScheduler;

	/** Location of libHAPI binary. **/
	FString LibHAPILocation;

	/** Keep current state of HAPI. **/
	HAPI_Result HAPIState;

	/** Is set to true when mismatch between defined and running HAPI versions is detected. **/
	bool bHAPIVersionMismatch;

	/** The Houdini Engine session. **/
	HAPI_Session Session;
};
