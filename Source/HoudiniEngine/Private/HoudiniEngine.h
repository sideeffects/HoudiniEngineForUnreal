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


class UStaticMesh;
class IAssetTools;
class FRunnableThread;
class IAssetTypeActions;
class IComponentAssetBroker;
class FHoudiniEngineScheduler;


class FHoudiniEngine : public IHoudiniEngine
{

/** IModuleInterface methods. **/
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

/** IHoudiniEngine methods. **/
public:

	virtual void RegisterComponentVisualizers() override;
	virtual void UnregisterComponentVisualizers() override;

	virtual UStaticMesh* GetHoudiniLogoStaticMesh() const override;
	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const override;
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

	/** Add menu extension for our module. **/
	void AddHoudiniMenuExtension(FMenuBuilder& MenuBuilder);

public:

	/** Menu action called to save a HIP file. **/
	void SaveHIPFile();

private:

	/** Singleton instance of Houdini Engine. **/
	static FHoudiniEngine* HoudiniEngineInstance;

private:

	/** AssetType actions associated with Houdini asset. **/
	TArray<TSharedPtr<IAssetTypeActions> > AssetTypeActions;

	/** Static mesh used for Houdini logo rendering. **/
	UStaticMesh* HoudiniLogoStaticMesh;

	/** Broker associated with Houdini asset. **/
	TSharedPtr<IComponentAssetBroker> HoudiniAssetBroker;

	/** Houdini logo brush. **/
	TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;

	/** The extender to pass to the level editor to extend it's window menu. **/
	TSharedPtr<FExtender> MainMenuExtender;

	/** Visualizer for our spline component. **/
	TSharedPtr<FComponentVisualizer> SplineComponentVisualizer;

	/** Synchronization primitive. **/
	FCriticalSection CriticalSection;

	/** Map of task statuses. **/
	TMap<FGuid, FHoudiniEngineTaskInfo> TaskInfos;

	/** Thread used to execute the scheduler. **/
	FRunnableThread* HoudiniEngineSchedulerThread;

	/** Scheduler used to schedule HAPI instantiation and cook tasks. **/
	FHoudiniEngineScheduler* HoudiniEngineScheduler;
};
