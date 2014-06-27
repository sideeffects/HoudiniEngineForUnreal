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

class FHoudiniEngine : public IHoudiniEngine, public FTickableEditorObject
{

public: /** IModuleInterface methods. **/

	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;

public: /** IHoudiniEngine methods. **/

	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const OVERRIDE;
	virtual void AddTask(const FHoudiniEngineTask& Task) OVERRIDE;

	virtual void AddNotification(FHoudiniEngineNotificationInfo* Notification) OVERRIDE;
	virtual void RemoveNotification(FHoudiniEngineNotificationInfo* Notification) OVERRIDE;
	virtual void UpdateNotification(FHoudiniEngineNotificationInfo* Notification) OVERRIDE;

public: /** FTickableEditorObject methods. **/

	virtual void Tick(float DeltaTime) OVERRIDE;
	virtual bool IsTickable() const OVERRIDE;
	virtual TStatId GetStatId() const OVERRIDE;

public:

	/** App identifier string. **/
	static const FName HoudiniEngineAppIdentifier;

public:

	/** Return singleton instance of Houdini Engine, used internally. **/
	static FHoudiniEngine& Get();

private:

	/** Register AssetType action. **/
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

private:

	/** Singleton instance of Houdini Engine. **/
	static FHoudiniEngine* HoudiniEngineInstance;

private:

	/** Map of currently active notifications. **/
	TMap<FHoudiniEngineNotificationInfo*, TWeakPtr<SNotificationItem> > Notifications;

	/** Queue of notifications we need to process and submit to Slate. **/
	TArray<FHoudiniEngineNotificationInfo*> QueuedNotifications;

	/** AssetType actions associated with Houdini asset. **/
	TArray<TSharedPtr<IAssetTypeActions> > AssetTypeActions;

	/** Scheduler used to schedule HAPI instantiation and cook tasks. **/
	TSharedPtr<FHoudiniEngineScheduler> HoudiniEngineScheduler;

	/** Broker associated with Houdini asset. **/
	TSharedPtr<IComponentAssetBroker> HoudiniAssetBroker;

	/** Houdini logo brush. **/
	TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;

	/** Synchronization primitive used to control access to notifications. **/
	FCriticalSection CriticalSection;

	/** Thread used to execute the scheduler. **/
	FRunnableThread* HoudiniEngineSchedulerThread;
};
