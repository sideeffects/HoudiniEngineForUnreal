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
class IAssetTools;
class FRunnableThread;
class IAssetTypeActions;
class IComponentAssetBroker;
class FHoudiniEngineScheduler;

class HOUDINIENGINE_API FHoudiniEngine : public IHoudiniEngine
{

/** IModuleInterface methods. **/
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

/** IHoudiniEngine methods. **/
public:

	virtual UStaticMesh* GetHoudiniLogoStaticMesh() const override;

#if WITH_EDITOR

	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const override;
	virtual TSharedPtr<ISlateStyle> GetSlateStyle() const override;

#endif

	virtual bool CheckHapiVersionMismatch() const override;
	virtual const FString& GetLibHAPILocation() const override;
	virtual void AddTask(const FHoudiniEngineTask& Task) override;
	virtual void AddTaskInfo(const FGuid HapIGUID, const FHoudiniEngineTaskInfo& TaskInfo) override;
	virtual void RemoveTaskInfo(const FGuid HapIGUID) override;
	virtual bool RetrieveTaskInfo(const FGuid HapIGUID, FHoudiniEngineTaskInfo& TaskInfo) override;
	virtual HAPI_Result GetHapiState() const override;
	virtual void SetHapiState(HAPI_Result Result) override;

public:

	/** App identifier string. **/
	static const FName HoudiniEngineAppIdentifier;

public:

	/** Return singleton instance of Houdini Engine, used internally. **/
	static FHoudiniEngine& Get();

	/** Return true if singleton instance has been created. **/
	static bool IsInitialized();

private:

#if WITH_EDITOR

	/** Register AssetType action. **/
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	/** Add menu extension for our module. **/
	void AddHoudiniMenuExtension(FMenuBuilder& MenuBuilder);

#endif

public:

#if WITH_EDITOR

	/** Menu action called to save a HIP file. **/
	void SaveHIPFile();

	/** Helper delegate used to determine if HIP file save can be executed. **/
	bool CanSaveHIPFile() const;

#endif

private:

	/** Singleton instance of Houdini Engine. **/
	static FHoudiniEngine* HoudiniEngineInstance;

private:

	/** Static mesh used for Houdini logo rendering. **/
	UStaticMesh* HoudiniLogoStaticMesh;

#if WITH_EDITOR

	/** AssetType actions associated with Houdini asset. **/
	TArray<TSharedPtr<IAssetTypeActions> > AssetTypeActions;

	/** Broker associated with Houdini asset. **/
	TSharedPtr<IComponentAssetBroker> HoudiniAssetBroker;

	/** Houdini logo brush. **/
	TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;

	/** The extender to pass to the level editor to extend it's window menu. **/
	TSharedPtr<FExtender> MainMenuExtender;

	/** Slate styleset used by this module. **/
	TSharedPtr<FSlateStyleSet> StyleSet;

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
};
