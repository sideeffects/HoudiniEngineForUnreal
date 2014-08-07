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

class IAssetTools;
class IAssetTypeActions;
class IComponentAssetBroker;
class FHoudiniEngineScheduler;
class FRunnableThread;
class FHoudiniAssetObjectGeo;

class FHoudiniEngine : public IHoudiniEngine
{

public: /** IModuleInterface methods. **/

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public: /** IHoudiniEngine methods. **/

	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const override;
	virtual TSharedPtr<FHoudiniAssetObjectGeo> GetHoudiniLogoGeo() const override;
	virtual void AddTask(const FHoudiniEngineTask& Task) override;
	virtual void AddTaskInfo(const FGuid HapIGUID, const FHoudiniEngineTaskInfo& TaskInfo) override;
	virtual void RemoveTaskInfo(const FGuid HapIGUID) override;
	virtual bool RetrieveTaskInfo(const FGuid HapIGUID, FHoudiniEngineTaskInfo& TaskInfo) override;

public:

	/** App identifier string. **/
	static const FName HoudiniEngineAppIdentifier;

public:

	/** Return singleton instance of Houdini Engine, used internally. **/
	static FHoudiniEngine& Get();

	/** Return true if singleton instance has been created. **/
	static bool IsInitialized();

private:

	/** Register AssetType action. **/
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

private:

	/** Singleton instance of Houdini Engine. **/
	static FHoudiniEngine* HoudiniEngineInstance;

private:

	/** AssetType actions associated with Houdini asset. **/
	TArray<TSharedPtr<IAssetTypeActions> > AssetTypeActions;

	/** Logo geometry. **/
	TSharedPtr<FHoudiniAssetObjectGeo> HoudiniLogoGeo;

	/** Broker associated with Houdini asset. **/
	TSharedPtr<IComponentAssetBroker> HoudiniAssetBroker;

	/** Houdini logo brush. **/
	TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;

	/** Synchronization primitive. **/
	FCriticalSection CriticalSection;

	/** Map of task statuses. **/
	TMap<FGuid, FHoudiniEngineTaskInfo> TaskInfos;

	/** Thread used to execute the scheduler. **/
	FRunnableThread* HoudiniEngineSchedulerThread;

	/** Scheduler used to schedule HAPI instantiation and cook tasks. **/
	FHoudiniEngineScheduler* HoudiniEngineScheduler;
};
