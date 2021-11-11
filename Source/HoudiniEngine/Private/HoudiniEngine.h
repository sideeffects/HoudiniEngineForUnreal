/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "HAPI/HAPI_Common.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniRuntimeSettings.h"

#include "Modules/ModuleInterface.h"

class FRunnableThread;
class FHoudiniEngineScheduler;
class FHoudiniEngineManager;
class UHoudiniAssetComponent;
class UStaticMesh;
class UMaterial;

struct FSlateDynamicImageBrush;

enum class EHoudiniBGEOCommandletStatus : uint8;

UENUM()
enum class EHoudiniSessionStatus : int8
{
	Invalid = -1,

	NotStarted,		// Session not initialized yet
	Connected,		// Session successfully started
	None,			// Session type set to None
	Stopped,		// Session stopped
	Failed,			// Session failed to connect
	Lost,			// Session Lost (HARS/Houdini Crash?)
	NoLicense,		// Failed to acquire a license
};

// Not using the IHoudiniEngine interface for now
class HOUDINIENGINE_API FHoudiniEngine : public IModuleInterface
{
	public:

		FHoudiniEngine();

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		// Return singleton instance of Houdini Engine, used internally.
		static FHoudiniEngine & Get();
		// Return true if singleton instance has been created.
		static bool IsInitialized();

		// Return the location of the currently loaded LibHAPI
		virtual const FString& GetLibHAPILocation() const;

		// Return the houdini executable to use
		static const FString GetHoudiniExecutable();

		// Session accessor
		virtual const HAPI_Session* GetSession() const;

		virtual const EHoudiniSessionStatus& GetSessionStatus() const;

		virtual void SetSessionStatus(const EHoudiniSessionStatus& InSessionStatus);

		// Default cook options
		static HAPI_CookOptions GetDefaultCookOptions();

		// Creates a new session
		bool StartSession(
			HAPI_Session*& SessionPtr,
			const bool& StartAutomaticServer,
			const float& AutomaticServerTimeout,
			const EHoudiniRuntimeSettingsSessionType& SessionType,
			const FString& ServerPipeName,
			const int32& ServerPort,
			const FString& ServerHost);

		// Stop the current session if it is valid
		bool StopSession(HAPI_Session*& SessionPtr);

		// Creates a session sync session
		bool SessionSyncConnect(
			const EHoudiniRuntimeSettingsSessionType& SessionType,
			const FString& ServerPipeName,
			const FString& ServerHost,
			const int32& ServerPort);

		// Stops the current session
		bool StopSession();
		// Stops, then creates a new session
		bool RestartSession();
		// Creates a session, start HARS
		bool CreateSession(const EHoudiniRuntimeSettingsSessionType& SessionType, FName OverrideServerPipeName=NAME_None);
		// Connect to an existing HE session
		bool ConnectSession(const EHoudiniRuntimeSettingsSessionType& SessionType);

		// Starts the HoudiniEngineManager ticking
		void StartTicking();
		// Stops the HoudiniEngineManager ticking and invalidate the session
		void StopTicking();

		bool IsTicking() const;

		// Initialize HAPI
		bool InitializeHAPISession();

		// Indicate to the plugin that the session is now invalid (HAPI has likely crashed...)
		void OnSessionLost();

		bool CreateTaskSlateNotification(
			const FText& InText,
			const bool& bForceNow = false,
			const float& NotificationExpire = HAPI_UNREAL_NOTIFICATION_EXPIRE,
			const float& NotificationFadeOut = HAPI_UNREAL_NOTIFICATION_FADEOUT);

		bool UpdateTaskSlateNotification(const FText& InText);
		bool FinishTaskSlateNotification(const FText& InText);

		// Only update persistent notification if cooking notification has been enabled in the settings.
		bool UpdateCookingNotification(const FText& InText, const bool bExpireAndFade);

		// Update persistent notification irrespective of any notification enable/disable settings. 
		bool UpdatePersistentNotification(const FText& InText, const bool bExpireAndFade);

		// If the time since last persistent notification has expired, fade out the persistent notification.
		void TickPersistentNotification(float DeltaTime);

		void SetHapiNotificationStartedTime(const double& InTime) { HapiNotificationStarted = InTime; };

		// Register task for execution.
		virtual void AddTask(const FHoudiniEngineTask & InTask);
		// Register task info.
		virtual void AddTaskInfo(const FGuid& InHapiGUID, const FHoudiniEngineTaskInfo & InTaskInfo);
		// Remove task info.
		virtual void RemoveTaskInfo(const FGuid& InHapiGUID);
		// Remove task info.
		virtual bool RetrieveTaskInfo(const FGuid& InHapiGUID, FHoudiniEngineTaskInfo & OutTaskInfo);
		// Register asset to the manager
		//virtual void AddHoudiniAssetComponent(UHoudiniAssetComponent* HAC);

		// Indicates whether or not cooking is currently enabled
		bool IsCookingEnabled() const;
		// Sets whether or not cooking is currently enabled
		void SetCookingEnabled(const bool& bInEnableCooking);

		// Check if we need to refresh UI when cooking is paused
		bool HasUIFinishRefreshingWhenPausingCooking() const { return UIRefreshCountWhenPauseCooking <= 0; };

		// Reset number of registered HACs when cooking is paused
		void SetUIRefreshCountWhenPauseCooking(const int32& bInCount) { UIRefreshCountWhenPauseCooking = bInCount; };
		// Reduce the count by 1 when an HAC UI is refreshed when cooking is paused
		void RefreshUIDisplayedWhenPauseCooking() { UIRefreshCountWhenPauseCooking -= 1; };

		// Indicates whether or not the first attempt to create a Houdini session was made
		bool GetFirstSessionCreated() const;
		// Sets whether or not the first attempt to create a Houdini session was made
		void SetFirstSessionCreated(const bool& bInStarted) { bFirstSessionCreated = bInStarted; };

		bool IsSessionSyncEnabled() const { return bEnableSessionSync; };

		bool IsSyncWithHoudiniCookEnabled() const;

		bool IsCookUsingHoudiniTimeEnabled() const { return bCookUsingHoudiniTime; };

		bool IsSyncViewportEnabled() const { return bSyncViewport; };

		bool IsSyncHoudiniViewportEnabled() const { return bSyncHoudiniViewport; };

		bool IsSyncUnrealViewportEnabled() const { return bSyncUnrealViewport; };

		// Helper function to update our session sync infos from Houdini's
		void UpdateSessionSyncInfoFromHoudini();

		// Helper function to update Houdini's Session sync infos from ours
		void UploadSessionSyncInfoToHoudini();

		// Sets whether or not viewport sync is enabled
		void SetSyncViewportEnabled(const bool& bInSync) { bSyncViewport = bInSync; };
		// Sets whether or not we want to sync the houdini viewport to unreal's
		void SetSyncHoudiniViewportEnabled(const bool& bInSync) { bSyncHoudiniViewport = bInSync; };
		// Sets whether or not we want to sync unreal's viewport to Houdini's
		void SetSyncUnrealViewportEnabled(const bool& bInSync) { bSyncUnrealViewport = bInSync; };

		// Returns the default Houdini Logo Static Mesh
		virtual TWeakObjectPtr<UStaticMesh> GetHoudiniLogoStaticMesh() const { return HoudiniLogoStaticMesh; };

		// Returns either the default Houdini material or the default template material
		virtual TWeakObjectPtr<UMaterial> GetHoudiniDefaultMaterial(const bool& bIsTemplate) const { return bIsTemplate ? HoudiniTemplateMaterial : HoudiniDefaultMaterial; };

		// Returns the default Houdini material
		virtual TWeakObjectPtr<UMaterial> GetHoudiniDefaultMaterial() const { return HoudiniDefaultMaterial; };
		// Returns the default template Houdini material
		virtual TWeakObjectPtr<UMaterial> GetHoudiniTemplatedMaterial() const { return HoudiniTemplateMaterial; };
		// Returns a shared Ptr to the houdini logo
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const { return HoudiniLogoBrush; };
		// Returns a shared Ptr to the houdini engine logo
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineLogoBrush() const { return HoudiniEngineLogoBrush; };

		// Returns the default Houdini reference mesh
		virtual TWeakObjectPtr<UStaticMesh> GetHoudiniDefaultReferenceMesh() const { return HoudiniDefaultReferenceMesh; };
		// Returns the default Houdini reference mesh material
		virtual TWeakObjectPtr<UMaterial> GetHoudiniDefaultReferenceMeshMaterial() const { return HoudiniDefaultReferenceMeshMaterial; };

		const HAPI_License GetLicenseType() const { return LicenseType; };

		const bool IsLicenseIndie() const { return (LicenseType == HAPI_LICENSE_HOUDINI_ENGINE_INDIE || LicenseType == HAPI_LICENSE_HOUDINI_INDIE); };

		// Session Sync ProcHandle accessor
		FProcHandle GetHESSProcHandle() const { return HESS_ProcHandle; };
		void  SetHESSProcHandle(const FProcHandle& InProcHandle) { HESS_ProcHandle = InProcHandle; };

		void StartPDGCommandlet();

		void StopPDGCommandlet();

		bool IsPDGCommandletRunningOrConnected();

		EHoudiniBGEOCommandletStatus GetPDGCommandletStatus();

		FHoudiniEngineManager* GetHoudiniEngineManager() { return HoudiniEngineManager; }

		const FHoudiniEngineManager* GetHoudiniEngineManager() const { return HoudiniEngineManager; }

		void UnregisterPostEngineInitCallback();

	private:

		// Singleton instance of Houdini Engine.
		static FHoudiniEngine * HoudiniEngineInstance;

		// Location of libHAPI binary. 
		FString LibHAPILocation;

		// The Houdini Engine session. 
		HAPI_Session Session;

		// The Houdini Engine session's status
		EHoudiniSessionStatus SessionStatus;

		// The type of HE license used by the current session
		HAPI_License LicenseType;

		// Synchronization primitive.
		FCriticalSection CriticalSection;
		
		// Map of task statuses.
		TMap<FGuid, FHoudiniEngineTaskInfo> TaskInfos;

		// Thread used to execute the scheduler.
		FRunnableThread * HoudiniEngineSchedulerThread;
		// Scheduler used to schedule HAPI instantiation and cook tasks. 
		FHoudiniEngineScheduler * HoudiniEngineScheduler;

		// Thread used to execute the manager.
		FRunnableThread * HoudiniEngineManagerThread;
		// Scheduler used to monitor and process Houdini Asset Components
		FHoudiniEngineManager * HoudiniEngineManager;

		// Process Handle for session sync
		FProcHandle HESS_ProcHandle;

		// Is set to true when mismatch between defined and running HAPI versions is detected. 
		//bool bHAPIVersionMismatch;

		// Global cooking flag, used to pause HEngine while using the editor 
		bool bEnableCookingGlobal;
		// Counter of HACs that need to be refreshed when pause cooking
		int32 UIRefreshCountWhenPauseCooking;

		// Indicates that the first attempt to create a session has been done
		// This is to delay the first "automatic" session creation for the first cook 
		// or instantiation rather than when the module started.
		bool bFirstSessionCreated;

		// Indicates if the current session is a SessionSync one
		bool bEnableSessionSync;

		// If true and we're in SessionSync, keeps the assets on the plugin side synchronized with changes on the Houdini side.
		//bool bSyncWithHoudiniCook;

		// If true and we're in SessionSync, use the Houdini Timeline time to cook assets.
		bool bCookUsingHoudiniTime;

		// If true and we're in Session Sync, the Houdini and Unreal viewport will be synchronized.
		bool bSyncViewport;
		// If true and we're in Session Sync, the Houdini viewport will be synchronized to Unreal's.
		bool bSyncHoudiniViewport;
		// If true and we're in Session Sync, the Unreal viewport will be synchronized to Houdini's.
		bool bSyncUnrealViewport;

		// Static mesh used for Houdini logo rendering.
		TWeakObjectPtr<UStaticMesh> HoudiniLogoStaticMesh;

		// Material used as default material.
		TWeakObjectPtr<UMaterial> HoudiniDefaultMaterial;

		// Material used as default template material.
		TWeakObjectPtr<UMaterial> HoudiniTemplateMaterial;

		// Houdini logo brush.
		TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;
		// Houdini logo brush.
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineLogoBrush;

		// Static mesh used for default mesh reference
		TWeakObjectPtr<UStaticMesh> HoudiniDefaultReferenceMesh;

		// Material used for default mesh reference
		TWeakObjectPtr<UMaterial> HoudiniDefaultReferenceMeshMaterial;

		FDelegateHandle PostEngineInitCallback;

#if WITH_EDITOR
		/** Notification used by this component. **/
		TWeakPtr<class SNotificationItem> NotificationPtr;

		/** Persistent notification. **/
		bool bPersistentAllowExpiry;
		TWeakPtr<class SNotificationItem> PersistentNotificationPtr;
		float TimeSinceLastPersistentNotification;
	
		/** Used to delay notification updates for HAPI asynchronous work. **/
		double HapiNotificationStarted;
#endif
};