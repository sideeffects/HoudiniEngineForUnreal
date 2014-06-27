/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"


const FName FHoudiniEngine::HoudiniEngineAppIdentifier = FName(TEXT("HoudiniEngineApp"));


IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine);
DEFINE_LOG_CATEGORY(LogHoudiniEngine);


FHoudiniEngine*
FHoudiniEngine::HoudiniEngineInstance = nullptr;


TSharedPtr<FSlateDynamicImageBrush>
FHoudiniEngine::GetHoudiniLogoBrush() const
{
	return HoudiniLogoBrush;
}


FHoudiniEngine&
FHoudiniEngine::Get()
{
	check(FHoudiniEngine::HoudiniEngineInstance);
	return *FHoudiniEngine::HoudiniEngineInstance;
}


void
FHoudiniEngine::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine module."));

	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FHoudiniAssetTypeActions()));

	// Create and register broker for Houdini asset.
	HoudiniAssetBroker = MakeShareable(new FHoudiniAssetBroker());
	FComponentAssetBrokerage::RegisterBroker(HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true);

	// Register thumbnail renderer for Houdini asset.
	UThumbnailManager::Get().RegisterCustomRenderer(UHoudiniAsset::StaticClass(), UHoudiniAssetThumbnailRenderer::StaticClass());

	// Create Houdini logo brush.
	const TArray<FPluginStatus> Plugins = IPluginManager::Get().QueryStatusForAllPlugins();
	for(auto PluginIt(Plugins.CreateConstIterator()); PluginIt; ++PluginIt)
	{
		const FPluginStatus& PluginStatus = *PluginIt;
		if(PluginStatus.Name == TEXT("HoudiniEngine"))
		{
			if(FPlatformFileManager::Get().GetPlatformFile().FileExists(*PluginStatus.Icon128FilePath))
			{
				const FName BrushName(*PluginStatus.Icon128FilePath);
				const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);

				if(Size.X > 0 && Size.Y > 0)
				{
					static const int ProgressIconSize = 32;
					HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
				}
			}

			break;
		}
	}

	// Perform HAPI initialization.
	HAPI_CookOptions CookOptions = HAPI_CookOptions_Create();
	CookOptions.maxVerticesPerPrimitive = 3;
	HAPI_Result Result = HAPI_Initialize("", "", CookOptions, true, -1);

	if(HAPI_RESULT_SUCCESS == Result)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Successfully intialized the Houdini Engine API module."));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(Result));
	}

	// Create HAPI scheduler and processing thread.
	HoudiniEngineScheduler = new FHoudiniEngineScheduler();
	HoudiniEngineSchedulerThread = FRunnableThread::Create(HoudiniEngineScheduler, TEXT("HoudiniTaskCookAsset"), false, false, 0, TPri_Normal);

	// Store the instance.
	FHoudiniEngine::HoudiniEngineInstance = this;
}


void
FHoudiniEngine::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine module."));

	if(UObjectInitialized())
	{
		// Unregister broker.
		FComponentAssetBrokerage::UnregisterBroker(HoudiniAssetBroker);

		// Unregister thumbnail renderer.
		UThumbnailManager::Get().UnregisterCustomRenderer(UHoudiniAsset::StaticClass());
	}

	// Unregister asset type actions we have previously registered.
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for(int32 Index = 0; Index < AssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions[Index].ToSharedRef());
		}

		AssetTypeActions.Empty();
	}

	// Perform HAPI finalization.
	HAPI_Cleanup();

	// Do scheduler and thread clean up.
	if(HoudiniEngineScheduler)
	{
		HoudiniEngineScheduler->Stop();
	}

	if(HoudiniEngineSchedulerThread)
	{
		//HoudiniEngineSchedulerThread->Kill(true);
		HoudiniEngineSchedulerThread->WaitForCompletion();

		delete HoudiniEngineSchedulerThread;
		HoudiniEngineSchedulerThread = nullptr;
	}

	if(HoudiniEngineScheduler)
	{
		delete HoudiniEngineScheduler;
		HoudiniEngineScheduler = nullptr;
	}
}


void
FHoudiniEngine::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}


void 
FHoudiniEngine::Tick(float DeltaTime)
{
	if(!FHoudiniEngine::HoudiniEngineInstance)
	{
		// Module has not been initialized yet.
		return;
	}

	// We need to record notifications which are being removed.
	TArray<FHoudiniEngineNotificationInfo*> RemovedNotifications;

	// Process existing notifications.
	for(TMap<FHoudiniEngineNotificationInfo*, TWeakPtr<SNotificationItem> >::TConstIterator ParamIter(Notifications); ParamIter; ++ParamIter)
	{
		FHoudiniEngineNotificationInfo* NotificationInfo = ParamIter.Key();

		if(NotificationInfo->bScheduledRemoved)
		{
			TWeakPtr<SNotificationItem> NotificationItem = ParamIter.Value();
			TSharedPtr<SNotificationItem> NotificationPtr = NotificationItem.Pin();
			NotificationPtr->ExpireAndFadeout();

			RemovedNotifications.Add(NotificationInfo);
		}
		else if(NotificationInfo->bScheduledUpdate)
		{
			TWeakPtr<SNotificationItem> NotificationItem = ParamIter.Value();
			TSharedPtr<SNotificationItem> NotificationPtr = NotificationItem.Pin();

			NotificationPtr->SetText(NotificationInfo->Text);
		}
	}

	// Process all notifications scheduled for removal.
	for(int32 Index = 0; Index < RemovedNotifications.Num(); ++Index)
	{
		FHoudiniEngineNotificationInfo* NotificationInfo = RemovedNotifications[Index];
		Notifications.Remove(NotificationInfo);
	}

	{
		FScopeLock ScopeLock(&CriticalSection);
	
		// Process all queued notifications (these are incoming from threads).
		for(int32 Index = 0; Index < QueuedNotifications.Num(); ++Index)
		{
			FHoudiniEngineNotificationInfo* NotificationInfo = QueuedNotifications[Index];

			// Submit this notification to slate.
			TWeakPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(*NotificationInfo);
			Notifications.Add(NotificationInfo, Notification);
		}

		// We processed all notifications.
		QueuedNotifications.Reset();
	}
}


bool
FHoudiniEngine::IsTickable() const
{
	return true;
}


TStatId
FHoudiniEngine::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FHoudiniEngine, STATGROUP_Tickables);
}


void
FHoudiniEngine::AddTask(const FHoudiniEngineTask& Task)
{
	HoudiniEngineScheduler->AddTask(Task);
}


void
FHoudiniEngine::AddNotification(FHoudiniEngineNotificationInfo* Notification)
{
	Notification->bFireAndForget = false;

	if(HoudiniLogoBrush.IsValid())
	{
		Notification->Image = HoudiniLogoBrush.Get();
	}

	// Store this notification information in queue for processing on the render thread.
	{
		FScopeLock ScopeLock(&CriticalSection);
		QueuedNotifications.Add(Notification);
	}
}


void
FHoudiniEngine::RemoveNotification(FHoudiniEngineNotificationInfo* Notification)
{
	{
		FScopeLock ScopeLock(&CriticalSection);

		// If notification is still queued, remove it.
		if(QueuedNotifications.RemoveSingleSwap(Notification))
		{
			return;
		}
	}

	// Flag notification for removal.
	FPlatformAtomics::InterlockedExchange(&Notification->bScheduledRemoved, 1);
}


void
FHoudiniEngine::UpdateNotification(FHoudiniEngineNotificationInfo* Notification)
{
	// Flag notification for update.
	FPlatformAtomics::InterlockedExchange(&Notification->bScheduledUpdate, 1);
}
