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

#include "HoudiniEngine.h"

#include "HoudiniEnginePrivatePCH.h"

#include "HoudiniApi.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniEngineManager.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniAssetComponent.h"
#if defined(HOUDINI_USE_PCG)
#include "HoudiniPCGCookable.h"
#endif
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputManagerImpl.h"
#include "HAPI/HAPI_Version.h"

#include "Modules/ModuleManager.h"
#include "Misc/ScopeLock.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "ISettingsModule.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"
#include "Logging/LogMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"

#if WITH_EDITOR
	#include "Widgets/Notifications/SNotificationList.h"
	#include "Framework/Notifications/NotificationManager.h"
#endif

#define LOCTEXT_NAMESPACE "HoudiniEngine"

IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine)
DEFINE_LOG_CATEGORY( LogHoudiniEngine );

FHoudiniEngine *
FHoudiniEngine::HoudiniEngineInstance = nullptr;

FHoudiniEngine::FHoudiniEngine()
	: LicenseType(HAPI_LICENSE_NONE)
	, HoudiniEngineSchedulerThread(nullptr)
	, HoudiniEngineScheduler(nullptr)
	, HoudiniEngineManagerThread(nullptr)
	, HoudiniEngineManager(nullptr)
	//, bHAPIVersionMismatch(false)
	, bEnableCookingGlobal(true)
	, UIRefreshCountWhenPauseCooking(0)
	, bFirstSessionCreated(false)
	, bEnableSessionSync(false)
	, bCookUsingHoudiniTime(true)
	, bSyncViewport(false)
	, bSyncHoudiniViewport(true)
	, bSyncUnrealViewport(false)
	, HoudiniLogoStaticMesh(nullptr)
	, HoudiniDefaultMaterial(nullptr)
	, HoudiniTemplateMaterial(nullptr)
	, HoudiniLogoBrush(nullptr)
	, HoudiniDefaultReferenceMesh(nullptr)
	, HoudiniDefaultReferenceMeshMaterial(nullptr)
	, HAPIPerfomanceProfileID(-1)
{
	SetSessionStatus(EHoudiniSessionStatus::Invalid);

#if WITH_EDITOR
	HapiNotificationStarted = 0.0;
	TimeSinceLastPersistentNotification = 0.0;
#endif
}

FHoudiniEngine&
FHoudiniEngine::Get()
{
	check(FHoudiniEngine::HoudiniEngineInstance);
	return *FHoudiniEngine::HoudiniEngineInstance;
}

bool
FHoudiniEngine::IsInitialized()
{
	return FHoudiniEngine::HoudiniEngineInstance != nullptr && FHoudiniEngineUtils::IsInitialized();
}

void 
FHoudiniEngine::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine module..."));

#if WITH_EDITOR
	// Register settings.
	if (ISettingsModule * SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project", "Plugins", "HoudiniEngine",
			LOCTEXT("RuntimeSettingsName", "Houdini Engine"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the HoudiniEngine plugin"),
			GetMutableDefault< UHoudiniRuntimeSettings >());
	}
#endif

	// Before starting the module, we need to locate and load HAPI library.
	{
		void * HAPILibraryHandle = FHoudiniEngineUtils::LoadLibHAPI(LibHAPILocation);
		if ( HAPILibraryHandle )
		{
			FHoudiniApi::InitializeHAPI( HAPILibraryHandle );
		}
		else
		{
			// Get platform specific name of libHAPI.
			FString LibHAPIName = FHoudiniEngineRuntimeUtils::GetLibHAPIName();
			HOUDINI_LOG_MESSAGE(TEXT("Failed locating or loading %s"), *LibHAPIName);
		}
	}

	// Create static mesh Houdini logo.
	HoudiniLogoStaticMesh = LoadObject<UStaticMesh>(
		nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_LOGO, nullptr, LOAD_None, nullptr);
	if (HoudiniLogoStaticMesh.IsValid())
		HoudiniLogoStaticMesh->AddToRoot();

	// Create default material.
	HoudiniDefaultMaterial = LoadObject<UMaterial>(
		nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_MATERIAL, nullptr, LOAD_None, nullptr);
	if (HoudiniDefaultMaterial.IsValid())
		HoudiniDefaultMaterial->AddToRoot();

	HoudiniTemplateMaterial = LoadObject<UMaterial>(
		nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_TEMPLATE_MATERIAL, nullptr, LOAD_None, nullptr);
	if (HoudiniTemplateMaterial.IsValid())
		HoudiniTemplateMaterial->AddToRoot();

	// Houdini Logo Brush
	FString Icon128FilePath = FHoudiniEngineUtils::GetHoudiniEnginePluginDir() / TEXT("Resources/Icon128.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*Icon128FilePath))
	{
		const FName BrushName(*Icon128FilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine Logo Brush
	FString HEIcon128FilePath = FHoudiniEngineUtils::GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_logo_128.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*HEIcon128FilePath))
	{
		const FName BrushName(*HEIcon128FilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineLogoBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Create Houdini default reference mesh
	HoudiniDefaultReferenceMesh = LoadObject<UStaticMesh>(
		nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_DEFAULT_REFERENCE_MESH, nullptr, LOAD_None, nullptr);
	if (HoudiniDefaultReferenceMesh.IsValid())
		HoudiniDefaultReferenceMesh->AddToRoot();
	
	// Create Houdini default reference mesh material
	HoudiniDefaultReferenceMeshMaterial = LoadObject<UMaterial>
		(nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_DEFAULT_REFERENCE_MESH_MATERIAL, nullptr, LOAD_None, nullptr);
	if (HoudiniDefaultReferenceMeshMaterial.IsValid())
		HoudiniDefaultReferenceMeshMaterial->AddToRoot();

	// We do not automatically try to start a session when starting up the module now.
	bFirstSessionCreated = false;

	// Create HAPI scheduler and processing thread.
	HoudiniEngineScheduler = new FHoudiniEngineScheduler();
	HoudiniEngineSchedulerThread = FRunnableThread::Create(
		HoudiniEngineScheduler, TEXT("HoudiniSchedulerThread"), 0, TPri_Normal);

	// Create Houdini Asset Manager
	HoudiniEngineManager = new FHoudiniEngineManager();

	// Create Unreal Object Input manager and its implementation (the singleton takes ownership of the implementation)
	FUnrealObjectInputManager::SetSingleton(new FUnrealObjectInputManagerImpl());

	// Set the session status to Not Started
	SetSessionStatus(EHoudiniSessionStatus::NotStarted);

	// Set the default value for pausing houdini engine cooking
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bEnableCookingGlobal = !HoudiniRuntimeSettings->bPauseCookingOnStart;

	// Check if a null session is set
	bool bNoneSession = (HoudiniRuntimeSettings->SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_None);
	if (bNoneSession)
		SetSessionStatus(EHoudiniSessionStatus::None);

	// Initialize the singleton with this instance
	FHoudiniEngine::HoudiniEngineInstance = this;

	// See if we need to start the manager ticking if needed
	// Dont tick if we failed to load HAPI, if cooking is disabled or if we're using a null session
	if (FHoudiniApi::IsHAPIInitialized())
	{
		if (bEnableCookingGlobal && !bNoneSession)
		{
			PostEngineInitCallback = FCoreDelegates::OnPostEngineInit.AddLambda([]()
			{
				FHoudiniEngine& HEngine = FHoudiniEngine::Get();
				HEngine.UnregisterPostEngineInitCallback();
				FHoudiniEngineManager* const Manager = HEngine.GetHoudiniEngineManager();
				if (Manager)
					Manager->StartHoudiniTicking();
			});
		}
	}
}

void 
FHoudiniEngine::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine module."));

	// We no longer need the Houdini logo static mesh.
	if (HoudiniLogoStaticMesh.IsValid())
	{
		HoudiniLogoStaticMesh->RemoveFromRoot();
		HoudiniLogoStaticMesh = nullptr;
	}

	// We no longer need the Houdini default material.
	if (HoudiniDefaultMaterial.IsValid())
	{
		HoudiniDefaultMaterial->RemoveFromRoot();
		HoudiniDefaultMaterial = nullptr;
	}

	// We no longer need the Houdini default material.
	if (HoudiniTemplateMaterial.IsValid())
	{
		HoudiniTemplateMaterial->RemoveFromRoot();
		HoudiniTemplateMaterial = nullptr;
	}

	// We no longer need the Houdini default reference mesh
	if (HoudiniDefaultReferenceMesh.IsValid()) 
	{
		HoudiniDefaultReferenceMesh->RemoveFromRoot();
		HoudiniDefaultReferenceMesh = nullptr;
	}

	// We no longer need the Houdini default reference mesh material
	if (HoudiniDefaultReferenceMeshMaterial.IsValid()) 
	{
		HoudiniDefaultReferenceMeshMaterial->RemoveFromRoot();
		HoudiniDefaultReferenceMeshMaterial = nullptr;
	}
	/*
	// We no longer need Houdini digital asset used for loading bgeo files.
	if (HoudiniBgeoAsset.IsValid())
	{
		HoudiniBgeoAsset->RemoveFromRoot();
		HoudiniBgeoAsset = nullptr;
	}
	*/

	/*
	// Stop Houdini Session sync if it is still running!
	FProcHandle PreviousHESS = GetHESSProcHandle();
	if (FPlatformProcess::IsProcRunning(PreviousHESS))
	{
		FPlatformProcess::TerminateProc(PreviousHESS, true);
	}
	*/

#if WITH_EDITOR
	// Unregister settings.
	ISettingsModule * SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
		SettingsModule->UnregisterSettings("Project", "Plugins", "HoudiniEngine");
#endif

	// Destroy the Unreal Object Input manager
	FUnrealObjectInputManager::DestroySingleton();

	// Do scheduler and thread clean up.
	if (HoudiniEngineScheduler)
		HoudiniEngineScheduler->Stop();

	if (HoudiniEngineSchedulerThread)
	{
		//HoudiniEngineSchedulerThread->Kill( true );
		HoudiniEngineSchedulerThread->WaitForCompletion();

		delete HoudiniEngineSchedulerThread;
		HoudiniEngineSchedulerThread = nullptr;
	}

	if ( HoudiniEngineScheduler )
	{
		delete HoudiniEngineScheduler;
		HoudiniEngineScheduler = nullptr;
	}

	// Do manager clean up.
	if (HoudiniEngineManager)
		HoudiniEngineManager->StopHoudiniTicking();

	if (HoudiniEngineManager)
	{
		delete HoudiniEngineManager;
		HoudiniEngineManager = nullptr;
	}

	// Perform HAPI finalization.
	if ( FHoudiniApi::IsHAPIInitialized() )
	{
		// Only cleanup if we're not using SessionSync!
		if (!bEnableSessionSync)
			FHoudiniApi::Cleanup(GetSession());

		FHoudiniApi::CloseSession(GetSession());
		SessionStatus = EHoudiniSessionStatus::Invalid;
	}

	FHoudiniApi::FinalizeHAPI();

	FHoudiniEngine::HoudiniEngineInstance = nullptr;
}

void
FHoudiniEngine::AddTask(const FHoudiniEngineTask & InTask)
{
	if ( HoudiniEngineScheduler )
		HoudiniEngineScheduler->AddTask(InTask);

	FScopeLock ScopeLock(&CriticalSection);
	FHoudiniEngineTaskInfo TaskInfo;
	TaskInfo.TaskType = InTask.TaskType;
	TaskInfo.TaskState = EHoudiniEngineTaskState::Working;

	TaskInfos.Add(InTask.HapiGUID, TaskInfo);
}

void
FHoudiniEngine::AddTaskInfo(const FGuid& InHapiGUID, const FHoudiniEngineTaskInfo & InTaskInfo)
{
	FScopeLock ScopeLock(&CriticalSection);
	TaskInfos.Add(InHapiGUID, InTaskInfo);
}

void
FHoudiniEngine::RemoveTaskInfo(const FGuid& InHapiGUID)
{
	FScopeLock ScopeLock(&CriticalSection);
	TaskInfos.Remove(InHapiGUID);
}

bool
FHoudiniEngine::RetrieveTaskInfo(const FGuid& InHapiGUID, FHoudiniEngineTaskInfo & OutTaskInfo)
{
	FScopeLock ScopeLock(&CriticalSection);

	if (TaskInfos.Contains(InHapiGUID))
	{
		OutTaskInfo = TaskInfos[InHapiGUID];
		return true;
	}

	return false;
}

const FString &
FHoudiniEngine::GetLibHAPILocation() const
{
	return LibHAPILocation;
}

const FString
FHoudiniEngine::GetHoudiniExecutable()
{
	FString HoudiniExecutable = TEXT("houdini");
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
	{
		switch (HoudiniRuntimeSettings->HoudiniExecutable)
		{
		case EHoudiniExecutableType::HRSHE_HoudiniFX:
			HoudiniExecutable = TEXT("houdinifx");
			break;

		case EHoudiniExecutableType::HRSHE_HoudiniCore:
			HoudiniExecutable = TEXT("houdinicore");
			break;

		case EHoudiniExecutableType::HRSHE_HoudiniIndie:
			HoudiniExecutable = TEXT("hindie");
			break;

		default:
		case EHoudiniExecutableType::HRSHE_Houdini:
			HoudiniExecutable = TEXT("houdini");
			break;

		}
	}

	return HoudiniExecutable;
}

const HAPI_Session*
FHoudiniEngine::GetSession(int32 Index) const
{
	if (!Sessions.IsValidIndex(Index))
	{
		return nullptr;
	}

	return Sessions[Index].type == HAPI_SESSION_MAX
		? nullptr
		: &Sessions[Index];
}

const EHoudiniSessionStatus&
FHoudiniEngine::GetSessionStatus() const
{
	return SessionStatus;
}

bool
FHoudiniEngine::GetSessionStatusAndColor(
	FString& OutStatusString, FLinearColor& OutStatusColor)
{
	OutStatusString = FString();
	OutStatusColor = FLinearColor::White;

	switch (SessionStatus)
	{
	case EHoudiniSessionStatus::NotStarted:
		// Session not initialized yet
		OutStatusString = TEXT("Houdini Engine Session - Not Started");
		OutStatusColor = FLinearColor::White;
		break;

	case EHoudiniSessionStatus::Connected:
		// Session successfully started
		OutStatusString = TEXT("Houdini Engine Session READY");
		OutStatusColor = FLinearColor::Green;
		break;
	case EHoudiniSessionStatus::Stopped:
		// Session stopped
		OutStatusString = TEXT("Houdini Engine Session STOPPED");
		OutStatusColor = FLinearColor(1.0f, 0.5f, 0.0f);
		break;
	case EHoudiniSessionStatus::Failed:
		// Session failed to be created/connected
		OutStatusString = TEXT("Houdini Engine Session FAILED");
		OutStatusColor = FLinearColor::Red;
		break;
	case EHoudiniSessionStatus::Lost:
		// Session Lost (HARS/Houdini Crash?)
		OutStatusString = TEXT("Houdini Engine Session LOST");
		OutStatusColor = FLinearColor::Red;
		break;
	case EHoudiniSessionStatus::NoLicense:
		// Failed to acquire a license
		OutStatusString = TEXT("Houdini Engine Session FAILED - No License");
		OutStatusColor = FLinearColor::Red;
		break;
	case EHoudiniSessionStatus::Connecting:
		// Failed to acquire a license
		OutStatusString = TEXT("Houdini Engine Session CONNECTING");
		OutStatusColor = FLinearColor::White;
		break;
	case EHoudiniSessionStatus::None:
		// Session type set to None
		OutStatusString = TEXT("Houdini Engine Session DISABLED");
		OutStatusColor = FLinearColor::White;
		break;
	default:
	case EHoudiniSessionStatus::Invalid:
		OutStatusString = TEXT("Houdini Engine Session INVALID");
		OutStatusColor = FLinearColor::Red;
		break;
	}

	// Handle a few specific case for active session
	if (SessionStatus == EHoudiniSessionStatus::Connected)
	{
		bool bPaused = !FHoudiniEngine::Get().IsCookingEnabled();
		bool bSSync = FHoudiniEngine::Get().IsSessionSyncEnabled();
		if (bPaused)
		{
			OutStatusString = TEXT("Houdini Engine Session PAUSED");
			OutStatusColor = FLinearColor::Yellow;
		}
		/*
		else if (bSSync)
		{
			OutStatusString = TEXT("Houdini Engine Session Sync READY");
			OutStatusColor = FLinearColor::Blue;
		}
		*/
	}

	return true;
}




void
FHoudiniEngine::SetSessionStatus(const EHoudiniSessionStatus& InSessionStatus)
{
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings->SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_None)
	{
		// Check for none sessions first
		SessionStatus = EHoudiniSessionStatus::None;
		return;
	}

	if (!bFirstSessionCreated)
	{
		// Don't change the status unless we've attempted to start the session once
		SessionStatus = EHoudiniSessionStatus::NotStarted;
		return;
	}

	switch (InSessionStatus)
	{
		case EHoudiniSessionStatus::NotStarted:
		case EHoudiniSessionStatus::NoLicense:
		case EHoudiniSessionStatus::Lost:
		case EHoudiniSessionStatus::None:
		case EHoudiniSessionStatus::Invalid:
		case EHoudiniSessionStatus::Connected:
		case EHoudiniSessionStatus::Connecting:
		{
			SessionStatus = InSessionStatus;
		}
		break;

		case EHoudiniSessionStatus::Stopped:
		{
			// Only set to stop status if the session was valid
			if (SessionStatus == EHoudiniSessionStatus::Connected)
				SessionStatus = EHoudiniSessionStatus::Stopped;
		}
		break;

		case EHoudiniSessionStatus::Failed:
		{
			// Preserve No License / Lost status
			if (SessionStatus != EHoudiniSessionStatus::NoLicense && SessionStatus != EHoudiniSessionStatus::Lost)
				SessionStatus = EHoudiniSessionStatus::Failed;
		}
		break;
	}	
}

HAPI_CookOptions
FHoudiniEngine::GetDefaultCookOptions()
{
	// Default CookOptions
	HAPI_CookOptions CookOptions;
	FHoudiniApi::CookOptions_Init(&CookOptions);

	CookOptions.curveRefineLOD = 8.0f;
	CookOptions.clearErrorsAndWarnings = false;
	CookOptions.maxVerticesPerPrimitive = 3;
	CookOptions.splitGeosByGroup = false;
	CookOptions.splitGeosByAttribute = false;
	CookOptions.splitAttrSH = 0;
	CookOptions.refineCurveToLinear = true;
	CookOptions.handleBoxPartTypes = false;
	CookOptions.handleSpherePartTypes = false;
	CookOptions.splitPointsByVertexAttributes = false;
	CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;
	CookOptions.cookTemplatedGeos = true;

	return CookOptions;
}

bool
FHoudiniEngine::StartSessionInternal(
	const bool bStartAutomaticServer,
	const bool bUseSessionSyncForAutomaticServer,
	const float AutomaticServerTimeout,
	const bool bShowNotificationsAndMessages,
	const EHoudiniRuntimeSettingsSessionType SessionType,
	const FString& ServerPipeName,
	const int32 ServerPort,
	const FString& ServerHost,
	const int32 Index,
	const int64 SharedMemoryBufferSize,
	const bool bSharedMemoryCyclicBuffer,
	const bool bEnableSharedMemoryDataTransfer)
{
	auto UpdatePathForServer = [&]
	{
		// Get the existing PATH env var
		FString OrigPathVar = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
		// Make sure we only extend the PATH once!
		if (OrigPathVar.Contains(LibHAPILocation))
			return;

		// Modify our PATH so that HARC will find HARS.exe
		const TCHAR* PathDelimiter = FPlatformMisc::GetPathVarDelimiter();
		FString ModifiedPath =
#if PLATFORM_MAC
			// On Mac our binaries are split between two folders
			LibHAPILocation + TEXT("/../Resources/bin") + PathDelimiter +
#endif
			LibHAPILocation + PathDelimiter + OrigPathVar;

		FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *ModifiedPath);
	};

	auto DisablePerfMon = []
	{
		// Disable the performance monitor to prevent random crashed when under heavy load.
		// TODO: remove me when the race condition in H is fixed.
		FPlatformMisc::SetEnvironmentVar(TEXT("HARS_DISABLE_PERFMON_LOGGING"), TEXT("1"));
	};


	HAPI_ThriftServerOptions ServerOptions;
	FMemory::Memzero<HAPI_ThriftServerOptions>(ServerOptions);
	ServerOptions.autoClose = true;
	ServerOptions.timeoutMs = AutomaticServerTimeout;
	ServerOptions.sharedMemoryBufferSize = SharedMemoryBufferSize;
	ServerOptions.sharedMemoryBufferType = bSharedMemoryCyclicBuffer ? HAPI_THRIFT_SHARED_MEMORY_RING_BUFFER : HAPI_THRIFT_SHARED_MEMORY_FIXED_LENGTH_BUFFER;	

	HAPI_SessionInfo SessionInfo;
	FHoudiniApi::SessionInfo_Init(&SessionInfo);
	SessionInfo.enableSharedMemoryDataTransfer = bEnableSharedMemoryDataTransfer;
	SessionInfo.sharedMemoryBufferSize = ServerOptions.sharedMemoryBufferSize;

	HAPI_Result SessionResult = HAPI_RESULT_FAILURE;
	switch (SessionType)
	{
	case EHoudiniRuntimeSettingsSessionType::HRSST_Socket:
	{
		// Try to connect to an existing socket session first
		SessionResult = FHoudiniApi::CreateThriftSocketSession(
			&Sessions[Index], H_TCHAR_TO_UTF8(*ServerHost), ServerPort, &SessionInfo);

		// Start a session and try to connect to it if we failed
		if (bStartAutomaticServer && SessionResult != HAPI_RESULT_SUCCESS)
		{
			if(!bUseSessionSyncForAutomaticServer)
			{
				// TODO: remove me when the race condition in H is fixed.
				DisablePerfMon();

				UpdatePathForServer();
				FHoudiniApi::StartThriftSocketServer(&ServerOptions, ServerPort, nullptr, nullptr);

				// We've started the server manually, disable session sync
				bEnableSessionSync = false;

				SessionResult = FHoudiniApi::CreateThriftSocketSession(
					&Sessions[Index], H_TCHAR_TO_UTF8(*ServerHost), ServerPort, &SessionInfo);
			}
			else
			{
				return false;
			}
		}
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe:
	{
		// Try to connect to an existing pipe session first
		SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
			&Sessions[Index], H_TCHAR_TO_UTF8(*ServerPipeName), &SessionInfo);

		// Start a session and try to connect to it if we failed
		if (bStartAutomaticServer && SessionResult != HAPI_RESULT_SUCCESS)
		{
			if(!bUseSessionSyncForAutomaticServer)
			{
				// TODO: remove me when the race condition in H is fixed.
				DisablePerfMon();

				UpdatePathForServer();
				FHoudiniApi::StartThriftNamedPipeServer(
					&ServerOptions, H_TCHAR_TO_UTF8(*ServerPipeName), nullptr, nullptr);

				// We've started the server manually, disable session sync
				bEnableSessionSync = false;

				SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(&Sessions[Index], H_TCHAR_TO_UTF8(*ServerPipeName), &SessionInfo);
			}
			else
			{
				return false;
			}
		}
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_MemoryBuffer:
	{
		// Try to connect to an existing pipe session first
		SessionInfo.sharedMemoryBufferSize = ServerOptions.sharedMemoryBufferSize;
		SessionInfo.sharedMemoryBufferType = ServerOptions.sharedMemoryBufferType;

		// Make sure memory buffer size make sense (between 1MB and 128GB)
		if ((SessionInfo.sharedMemoryBufferSize < 1) || (SessionInfo.sharedMemoryBufferSize > 131072))
		{
			HOUDINI_LOG_ERROR(TEXT("Invalid Shared Memory Buffer size!"));
			SessionResult = HAPI_RESULT_FAILURE;
			break;
		}
		
		SessionResult = FHoudiniApi::CreateThriftSharedMemorySession(
			&Sessions[Index], H_TCHAR_TO_UTF8(*ServerPipeName), &SessionInfo);

		// Start a session and try to connect to it if we failed
		if (bStartAutomaticServer && SessionResult != HAPI_RESULT_SUCCESS)
		{
			if(!bUseSessionSyncForAutomaticServer)
			{
				// TODO: remove me when the race condition in H is fixed.
				DisablePerfMon();

				UpdatePathForServer();
				HAPI_ProcessId ServerProcID = -1;
				HAPI_Result ServerResult = FHoudiniApi::StartThriftSharedMemoryServer(
					&ServerOptions, H_TCHAR_TO_UTF8(*ServerPipeName), &ServerProcID, nullptr);
				if(ServerResult == HAPI_RESULT_SUCCESS)
				{
					// We've started the server manually, disable session sync
					bEnableSessionSync = false;

					SessionResult = FHoudiniApi::CreateThriftSharedMemorySession(
						&Sessions[Index], H_TCHAR_TO_UTF8(*ServerPipeName), &SessionInfo);
				}
			}
			else
			{
				return false;
			}
		}
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_None:
	{
		HOUDINI_LOG_MESSAGE(TEXT("Session type set to None, Cooking is disabled."));
		// Disable session sync
		bEnableSessionSync = false;
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_InProcess:
	{
		// As of Unreal 4.19, InProcess sessions are not supported anymore
		SessionResult = FHoudiniApi::CreateInProcessSession(&Sessions[Index], &SessionInfo);
		// Disable session sync
		bEnableSessionSync = false;
	}
	break;

	default:
	{
		HOUDINI_LOG_ERROR(TEXT("Unsupported Houdini Engine session type"));
		// Disable session sync
		bEnableSessionSync = false;
	}
	break;
	}

	// Stop here if we used a none session
	if (SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_None)
		return false;

	FHoudiniEngine::Get().SetFirstSessionCreated(true);

	if (SessionResult != HAPI_RESULT_SUCCESS || Sessions.IsEmpty() || !&Sessions[Index])
	{
		// Disable session sync as well?
		bEnableSessionSync = false;

		if (SessionType != EHoudiniRuntimeSettingsSessionType::HRSST_InProcess && !bUseSessionSyncForAutomaticServer)
		{
			FString ConnectionError = FHoudiniEngineUtils::GetConnectionError();
			if (!ConnectionError.IsEmpty() && bShowNotificationsAndMessages)
				HOUDINI_LOG_ERROR(TEXT("Houdini Engine Session failed to connect -  %s"), *ConnectionError);
		}

		return false;
	}

	return true;
}

bool
FHoudiniEngine::StartSessionsInternal(
	const bool bStartAutomaticServer,
	const bool bUseSessionSyncForAutomaticServer,
	const float AutomaticServerTimeout,
	const bool bShowNotificationsAndMessages,
	const EHoudiniRuntimeSettingsSessionType SessionType,
	const int32 MaxNumSessions,
	const FString& ServerPipeName,
	const int32 ServerPort,
	const FString& ServerHost,
	const int64 SharedMemoryBufferSize,
	const bool bSharedMemoryCyclicBuffer,
	const bool bEnableSharedMemoryDataTransfer)
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	// Only start a new Session if we dont already have a valid one
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(GetSession()))
		return true;

	// Set the HAPI_CLIENT_NAME environment variable to "unreal"
	// We need to do this before starting HARS.
	FPlatformMisc::SetEnvironmentVar(TEXT("HAPI_CLIENT_NAME"), TEXT("unreal"));

	// Set custom $HOME env var if it's been specified
	FHoudiniEngineRuntimeUtils::SetHoudiniHomeEnvironmentVariable();

	// Unless we automatically start the server,
	// consider we're in SessionSync mode
	bEnableSessionSync = true;

	// Clear the connection error before starting new sessions
	if(SessionType != EHoudiniRuntimeSettingsSessionType::HRSST_None)
		FHoudiniApi::ClearConnectionError();


	// Setup number of sessions.
	int NumSessions = MaxNumSessions;
	if(SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_MemoryBuffer && MaxNumSessions > 1)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Limiting Number of Sessions to 1 when using Shared Memory."));
		NumSessions = 1;
	}
	Sessions.Empty(NumSessions);

	bool bSuccess = false;

	// Create the sessions...
	for (int32 i = 0; i < NumSessions; ++i)
	{
		Sessions.Emplace();

		bSuccess = StartSessionInternal(
			bStartAutomaticServer,
			bUseSessionSyncForAutomaticServer,
			AutomaticServerTimeout,
			bShowNotificationsAndMessages,
			SessionType,
			ServerPipeName,
			ServerPort,
			ServerHost,
			i,
			SharedMemoryBufferSize,
			bSharedMemoryCyclicBuffer,
			bEnableSharedMemoryDataTransfer);

		if (!bSuccess)
		{
			Sessions.Empty();
			break;
		}
	}

	if (bStartAutomaticServer && bUseSessionSyncForAutomaticServer && !bSuccess)
	{
		// We hit this if we failed to connect to an existing server and we've enabled automatic
		// SessionSync.
		bSuccess = OpenSessionSync(true);
		if(!bSuccess)
			return false;
	}

	// Update this session's license type
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetSessionEnvInt(GetSession(), HAPI_SESSIONENVINT_LICENSE, (int32*)&LicenseType));

	return true;
}

bool
FHoudiniEngine::SessionSyncConnect(
	const EHoudiniRuntimeSettingsSessionType SessionType,
	const int32 NumSessions,
	const FString& ServerPipeName,
	const FString& ServerHost,
	const int32 ServerPort,
	const int64 BufferSize,
	const bool BufferCyclic)
{
	FScopeLock Lock(&CriticalSection);

	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	// Only start a new Session if we dont already have a valid one
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(GetSession()))
		return true;

	FHoudiniEngine::Get().SetFirstSessionCreated(true);

	// Consider the session failed as long as we dont connect
	SetSessionStatus(EHoudiniSessionStatus::Failed);

	HAPI_Result SessionResult = HAPI_RESULT_FAILURE;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	
	switch (SessionType)
	{
	case EHoudiniRuntimeSettingsSessionType::HRSST_Socket:
	{
		// Try to connect to an existing socket session first
		HAPI_SessionInfo SessionInfo;
		FHoudiniApi::SessionInfo_Init(&SessionInfo);

		Sessions.Empty(NumSessions);
		for (int32 i = 0; i < NumSessions; ++i)
		{
			Sessions.Emplace();
			SessionResult = FHoudiniApi::CreateThriftSocketSession(
				&Sessions[i], H_TCHAR_TO_UTF8(*ServerHost), ServerPort, &SessionInfo);
			if (SessionResult != HAPI_RESULT_SUCCESS)
				break;

		}

	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe:
	{
		// Try to connect to an existing pipe session first
		HAPI_SessionInfo SessionInfo;
		FHoudiniApi::SessionInfo_Init(&SessionInfo);

		Sessions.Empty(NumSessions);
		for (int32 i = 0; i < NumSessions; ++i)
		{
			Sessions.Emplace();
			SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
				&Sessions[i], H_TCHAR_TO_UTF8(*ServerPipeName), &SessionInfo);
			if (SessionResult != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_MemoryBuffer:
	{
		// Try to connect to an existing pipe session first
		HAPI_SessionInfo SessionInfo;
		FHoudiniApi::SessionInfo_Init(&SessionInfo);
		SessionInfo.sharedMemoryBufferSize = BufferSize;
		SessionInfo.sharedMemoryBufferType = BufferCyclic ? HAPI_THRIFT_SHARED_MEMORY_RING_BUFFER : HAPI_THRIFT_SHARED_MEMORY_FIXED_LENGTH_BUFFER;

		Sessions.Empty(NumSessions);
		for (int32 i = 0; i < NumSessions; ++i)
		{
			Sessions.Emplace();
			SessionResult = FHoudiniApi::CreateThriftSharedMemorySession(
				&Sessions[i], H_TCHAR_TO_UTF8(*ServerPipeName), &SessionInfo);
			if (SessionResult != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_None:
	case EHoudiniRuntimeSettingsSessionType::HRSST_InProcess:
	default:
		HOUDINI_LOG_ERROR(TEXT("Unsupported Houdini Engine Session Sync Type!!"));
		bEnableSessionSync = false;
		break;
	}

	if (SessionResult != HAPI_RESULT_SUCCESS)
		return false;

	// Enable session sync
	bEnableSessionSync = true;
	SetSessionStatus(EHoudiniSessionStatus::Connected);

	OnSessionConnected();

	// Update this session's license type
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetSessionEnvInt(
		GetSession(), HAPI_SESSIONENVINT_LICENSE, (int32*)&LicenseType));

	// Update the default viewport sync settings
	bSyncViewport = HoudiniRuntimeSettings->bSyncViewport;
	bSyncHoudiniViewport = HoudiniRuntimeSettings->bSyncHoudiniViewport;
	bSyncUnrealViewport = HoudiniRuntimeSettings->bSyncUnrealViewport;

	return true;
}

bool
FHoudiniEngine::InitializeHAPISession()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngine::InitializeHAPISession);

	// The HAPI stubs needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to initialize HAPI: The Houdini API stubs have not been properly initialized."));
		return false;
	}

	// We need a Valid Session
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(GetSession()))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to initialize HAPI: The session is invalid."));
		return false;
	}

	// Now, initialize HAPI with the new session
	// We need to make sure HAPI version is correct.
	int32 RunningEngineMajor = 0;
	int32 RunningEngineMinor = 0;
	int32 RunningEngineApi = 0;

	// Retrieve version numbers for running Houdini Engine.
	FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
	FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
	FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

	// Compare defined and running versions.
	if (RunningEngineMajor != HAPI_VERSION_HOUDINI_ENGINE_MAJOR
		|| RunningEngineMinor != HAPI_VERSION_HOUDINI_ENGINE_MINOR)
	{
		// Major or minor HAPI version differs, stop here
		HOUDINI_LOG_ERROR(
			TEXT("Starting up the Houdini Engine module failed: built and running versions do not match."));
		HOUDINI_LOG_ERROR(
			TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d"),
			HAPI_VERSION_HOUDINI_ENGINE_MAJOR, HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API,
			RunningEngineMajor, RunningEngineMinor, RunningEngineApi);

		// Display an error message

		// 
		return false;

	}
	else if (RunningEngineApi != HAPI_VERSION_HOUDINI_ENGINE_API)
	{
		// Major/minor HAPIversions match, but only the API version differs,
		// Allow the user to continue but warn him of possible instabilities
		HOUDINI_LOG_WARNING(
			TEXT("Starting up the Houdini Engine module: built and running versions do not match."));
		HOUDINI_LOG_WARNING(
			TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d"),
			HAPI_VERSION_HOUDINI_ENGINE_MAJOR, HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API,
			RunningEngineMajor, RunningEngineMinor, RunningEngineApi);
		HOUDINI_LOG_WARNING(
			TEXT("This could cause instabilities and crashes when using the Houdini Engine plugin"));
	}

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	// Default CookOptions
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();

	bool bUseCookingThread = true;
	HAPI_Result Result = FHoudiniApi::Initialize(
		GetSession(),
		&CookOptions,
		bUseCookingThread,
		HoudiniRuntimeSettings->CookingThreadStackSize,
		H_TCHAR_TO_UTF8(*HoudiniRuntimeSettings->HoudiniEnvironmentFiles),
		H_TCHAR_TO_UTF8(*HoudiniRuntimeSettings->OtlSearchPath),
		H_TCHAR_TO_UTF8(*HoudiniRuntimeSettings->DsoSearchPath),
		H_TCHAR_TO_UTF8(*HoudiniRuntimeSettings->ImageDsoSearchPath),
		H_TCHAR_TO_UTF8(*HoudiniRuntimeSettings->AudioDsoSearchPath));
	
	if (Result == HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Successfully intialized the Houdini Engine module."));
	}
	else if (Result == HAPI_RESULT_ALREADY_INITIALIZED)
	{
		// Reused session? just notify the user
		HOUDINI_LOG_MESSAGE(TEXT("Successfully intialized the Houdini Engine module - HAPI was already initialzed."));
	}
	else
	{
		HOUDINI_LOG_ERROR(
			TEXT("Houdini Engine API initialization failed: %s"),
			*FHoudiniEngineUtils::GetErrorDescription(Result));

		return false;
	}

	// Let HAPI know we are running inside UE4
	FHoudiniApi::SetServerEnvString(GetSession(), HAPI_ENV_CLIENT_NAME, HAPI_UNREAL_CLIENT_NAME);

	if (bEnableSessionSync)
	{
		// Set the session sync infos if needed
		UploadSessionSyncInfoToHoudini();

		// Indicate that Session Sync is enabled
		FString Notification = TEXT("Houdini Engine Session enabled.");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Session enabled."));		
	}

	return true;
}


void
FHoudiniEngine::OnSessionLost()
{
	// Mark the session as invalid
	Sessions.Empty();
	SetSessionStatus(EHoudiniSessionStatus::Lost);

	bEnableSessionSync = false;
	HoudiniEngineManager->StopHoudiniTicking();

	// This indicates that we likely have lost the session due to a crash in HARS/Houdini
	FString Notification = TEXT("Houdini Engine Session lost!");
	FHoudiniEngineUtils::CreateSlateNotification(Notification, 2.0, 4.0);

	HOUDINI_LOG_ERROR(TEXT("Houdini Engine Session lost! This could be caused by a crash in HARS."));

	PrintHoudiniCrashLog();
}

void
FHoudiniEngine::PrintHoudiniCrashLog()
{
	// Attempts to find the latest Houdini Crash log and output it to the console.
	// Ignores logs older than MaxAgeInHours old.

	// Try TEMP, then TMP as a fallback. If neither, do nothing.
	FString TempDir = FPlatformMisc::GetEnvironmentVariable(TEXT("TEMP"));
	if(TempDir.IsEmpty())
		TempDir = FPlatformMisc::GetEnvironmentVariable(TEXT("TMP"));

	if(TempDir.IsEmpty())
		return;


	// %TEMP%\houdini_temp
	const FString HoudiniTempDir = FPaths::Combine(TempDir, TEXT("houdini_temp"));
	if(!IFileManager::Get().DirectoryExists(*HoudiniTempDir))
		return;

	// Find crash logs
	TArray<FString> CrashLogs;
	IFileManager::Get().FindFilesRecursive(CrashLogs, *HoudiniTempDir, TEXT("crash*log.txt"), true, false);

	const int MaxAgeInHours = 1;
	const FDateTime NowUtc = FDateTime::UtcNow();
	const FTimespan MaxAge = FTimespan::FromHours(MaxAgeInHours);

	FString LatestPath;
	FDateTime LatestTime = FDateTime::MinValue();

	FTimespan FileAge;

	for(const FString& Path : CrashLogs)
	{
		const FFileStatData Stat = IFileManager::Get().GetStatData(*Path);
		if(!Stat.bIsValid)
		{
			continue;
		}

		const FDateTime ModUtc = Stat.ModificationTime; 
		const FTimespan Age = NowUtc - ModUtc;
		if(Age <= MaxAge && ModUtc > LatestTime)
		{
			LatestTime = ModUtc;
			LatestPath = Path;
			FileAge = Age;
		}
	}

	if(LatestPath.IsEmpty())
		return;

	FString Content;
	if(!FFileHelper::LoadFileToString(Content, *LatestPath))
		return;

	const TCHAR* FormatString = TEXT("%Y-%m-%d %H:%M:%S");

	HOUDINI_LOG_ERROR(TEXT("=== Found a Houdini Crash Log (Latest <%dh) ==="), MaxAgeInHours);
	HOUDINI_LOG_ERROR(TEXT("File: %s"), *LatestPath);
	HOUDINI_LOG_ERROR(TEXT("Time Now (UTC): %s"), *NowUtc.ToString(FormatString));
	HOUDINI_LOG_ERROR(TEXT("Modified (UTC): %s"), *LatestTime.ToString(FormatString));
	HOUDINI_LOG_ERROR(TEXT("File age: %s"), *FileAge.ToString());
	HOUDINI_LOG_ERROR(TEXT("=======================================\n%s"), *Content);
}

bool
FHoudiniEngine::StopSession()
{
	FScopeLock Lock(&CriticalSection);

	return StopSessionInternal();
}

bool
FHoudiniEngine::StopSessionInternal()
{
	FScopeLock Lock(&CriticalSection);

	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	// If the current session is valid, clean up and close the session
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(GetSession()))
	{
		// Only cleanup if we're not using SessionSync!
		if(!bEnableSessionSync)
			FHoudiniApi::Cleanup(GetSession());

		FHoudiniApi::CloseSession(GetSession());
	}

	Sessions.Empty();
	SetSessionStatus(EHoudiniSessionStatus::Stopped);
	bEnableSessionSync = false;

	HoudiniEngineManager->StopHoudiniTicking();

	return true;
}

bool
FHoudiniEngine::RestartSession(bool bShowNotificationsAndMessages)
{
	FScopeLock Lock(&CriticalSection);

	FString StatusText = TEXT("Starting the Houdini Engine session...");
	if (bShowNotificationsAndMessages)
		FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Make sure we stop the current session if it is still valid
	bool bSuccess = false;
	if (!StopSessionInternal())
	{
		// StopSession returns false only if Houdini is not initialized
		HOUDINI_LOG_ERROR(TEXT("Failed to restart the Houdini Engine session - HAPI Not initialized"));
	}
	else
	{
		// Try to reconnect/start a new session
		SetSessionStatus(EHoudiniSessionStatus::Connecting);
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (!StartSessionsInternal(
			HoudiniRuntimeSettings->bStartAutomaticServer,
			HoudiniRuntimeSettings->bUseSessionSyncForAutomaticServer,
			HoudiniRuntimeSettings->AutomaticServerTimeout,
			bShowNotificationsAndMessages,
			HoudiniRuntimeSettings->SessionType,
			HoudiniRuntimeSettings->NumSessions,
			HoudiniRuntimeSettings->ServerPipeName,
			HoudiniRuntimeSettings->ServerPort,
			HoudiniRuntimeSettings->ServerHost,
			HoudiniRuntimeSettings->SharedMemoryBufferSize,
			HoudiniRuntimeSettings->bSharedMemoryBufferCyclic,
			HoudiniRuntimeSettings->bEnableSharedMemoryDataTransfer))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to restart the Houdini Engine session - Failed to start the new Session"));
			SetSessionStatus(EHoudiniSessionStatus::Failed);
		}
		else
		{
			// Now initialize HAPI with this session
			if (!InitializeHAPISession())
			{
				if (bShowNotificationsAndMessages)
					HOUDINI_LOG_ERROR(TEXT("Failed to restart the Houdini Engine session - Failed to initialize HAPI"));	
				SetSessionStatus(EHoudiniSessionStatus::Failed);
			}
			else
			{
				bSuccess = true;
				SetSessionStatus(EHoudiniSessionStatus::Connected);
			}
		}
	}

	OnSessionConnected();

	// Start ticking only if we successfully started the session
	if (bSuccess)
	{
		StartTicking(bShowNotificationsAndMessages);
		return true;
	}
	else
	{
		StopTicking(bShowNotificationsAndMessages);
		return false;
	}
}

void 
FHoudiniEngine::OnSessionConnected()
{
	// This function is called whenever the plugin connects to a new session.
	// It exists because Houdini Cookables need to know when this happens so they can invalidate
	// their HAPI info, eg. node ids, left over from previous sessions.

	// Do the same thing for cookable
	int32 NumCookable = FHoudiniEngineRuntime::Get().GetRegisteredHoudiniCookableCount();
	for (int32 nCurrent = 0; nCurrent < NumCookable; nCurrent++)
	{
		UHoudiniCookable* CurCookable = FHoudiniEngineRuntime::Get().GetRegisteredHoudiniCookableAt(nCurrent);
		if (!CurCookable || !IsValid(CurCookable))
			continue;

		CurCookable->OnSessionConnected();
	}
}

bool
FHoudiniEngine::CreateSession(const EHoudiniRuntimeSettingsSessionType& SessionType, FName OverrideServerPipeName)
{
	FScopeLock Lock(&CriticalSection);

	FString StatusText = TEXT("Create the Houdini Engine session...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Make sure we stop the current session if it is still valid
	bool bSuccess = false;

	// Try to reconnect/start a new session
	constexpr bool bStartAutomaticServer = true;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (!StartSessionsInternal(
		bStartAutomaticServer,
		false, // bUseSessionSyncForAutomaticServer
		HoudiniRuntimeSettings->AutomaticServerTimeout,
		true, // bShowNotificationsAndMessages
		SessionType,
		HoudiniRuntimeSettings->NumSessions,
		OverrideServerPipeName == NAME_None ? HoudiniRuntimeSettings->ServerPipeName : OverrideServerPipeName.ToString(),
		HoudiniRuntimeSettings->ServerPort,
		HoudiniRuntimeSettings->ServerHost,
		HoudiniRuntimeSettings->SharedMemoryBufferSize,
		HoudiniRuntimeSettings->bSharedMemoryBufferCyclic,
		HoudiniRuntimeSettings->bEnableSharedMemoryDataTransfer))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to start the Houdini Engine Session"));
		SetSessionStatus(EHoudiniSessionStatus::Failed);
	}
	else
	{
		// Now initialize HAPI with this session
		if (!InitializeHAPISession())
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to start the Houdini Engine session - Failed to initialize HAPI"));
			SetSessionStatus(EHoudiniSessionStatus::Failed);
		}
		else
		{
			bSuccess = true;
			SetSessionStatus(EHoudiniSessionStatus::Connected);
		}
	}

	// Notify our objects that we've connected to a new session.
	OnSessionConnected();

	// Start ticking only if we successfully started the session
	if (bSuccess)
	{
		StartTicking(true);
		return true;
	}
	else
	{
		StopTicking(true);
		return false;
	}
}

bool
FHoudiniEngine::ConnectSession(bool bShowNotificationsAndMessages)
{
	FScopeLock Lock(&CriticalSection);

	FString StatusText = TEXT("Connecting to a Houdini Engine session...");
	if (bShowNotificationsAndMessages)
		FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Make sure we stop the current session if it is still valid
	bool bSuccess = false;

	// Try to reconnect/start new sessions
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (!StartSessionsInternal(
		false,
		false,
		HoudiniRuntimeSettings->AutomaticServerTimeout,
		bShowNotificationsAndMessages,
		HoudiniRuntimeSettings->SessionType,
		HoudiniRuntimeSettings->NumSessions,
		HoudiniRuntimeSettings->ServerPipeName,
		HoudiniRuntimeSettings->ServerPort,
		HoudiniRuntimeSettings->ServerHost,
		HoudiniRuntimeSettings->SharedMemoryBufferSize,
		HoudiniRuntimeSettings->bSharedMemoryBufferCyclic,
		HoudiniRuntimeSettings->bEnableSharedMemoryDataTransfer))
	{
		if(bShowNotificationsAndMessages)
			HOUDINI_LOG_ERROR(TEXT("Failed to connect to the Houdini Engine Session"));
		SetSessionStatus(EHoudiniSessionStatus::Failed);
	}
	else
	{
		// Now initialize HAPI with this session
		if (!InitializeHAPISession())
		{
			if (bShowNotificationsAndMessages)
				HOUDINI_LOG_ERROR(TEXT("Failed to connect to the Houdini Engine session - Failed to initialize HAPI"));
			SetSessionStatus(EHoudiniSessionStatus::Failed);
		}
		else
		{
			bSuccess = true;
			SetSessionStatus(EHoudiniSessionStatus::Connected);
		}
	}

	// Notify our objects that we've connected to a new session.
	OnSessionConnected();

	// Start ticking only if we successfully started the session
	if (bSuccess)
	{
		StartTicking(bShowNotificationsAndMessages);
		return true;
	}
	else
	{
		StopTicking(bShowNotificationsAndMessages);
		return false;
	}
}

void
FHoudiniEngine::StartTicking(bool bShowNotificationsAndMessages)
{
	// Finish the notification and display the results
	if (bShowNotificationsAndMessages)
	{
		FString StatusText = TEXT("Houdini Engine session connected.");
		FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(StatusText));
	}

	HoudiniEngineManager->StartHoudiniTicking();
}

void
FHoudiniEngine::StopTicking(bool bShowNotificationsAndMessages, bool bStopSession/*=true*/)
{
	if(bShowNotificationsAndMessages)
	{
		// Finish the notification and display the results
		FString StatusText = TEXT("Failed to start the Houdini Engine session...");
		FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(StatusText));
	}
	HoudiniEngineManager->StopHoudiniTicking();

	if(bStopSession)
		StopSessionInternal();
}

bool FHoudiniEngine::IsTicking() const
{
	if (!HoudiniEngineManager)
		return false;
	return HoudiniEngineManager->IsTicking();
}

bool
FHoudiniEngine::IsCookingEnabled() const
{
	return bEnableCookingGlobal;
}

void
FHoudiniEngine::SetCookingEnabled(const bool& bInEnableCooking)
{
	bEnableCookingGlobal = bInEnableCooking;
}

bool
FHoudiniEngine::GetFirstSessionCreated() const
{
	return bFirstSessionCreated;
}

bool
FHoudiniEngine::CreateTaskSlateNotification(
	const FText& InText, const bool& bForceNow, const float& NotificationExpire, const float& NotificationFadeOut)
{
#if WITH_EDITOR
	static double NotificationUpdateFrequency = 2.0f;

	// Check whether we want to display Slate cooking and instantiation notifications.
	bool bDisplaySlateCookingNotifications = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

	if (!bDisplaySlateCookingNotifications)
		return false;

	if (!bForceNow)
	{
		if ((FPlatformTime::Seconds() - HapiNotificationStarted) < NotificationUpdateFrequency)
			return false;
	}

	if (!NotificationPtr.IsValid())
	{
		FNotificationInfo Info(InText);
		Info.bFireAndForget = false;
		Info.FadeOutDuration = NotificationFadeOut;
		Info.ExpireDuration = NotificationExpire;
		TSharedPtr< FSlateDynamicImageBrush > HoudiniBrush = FHoudiniEngine::Get().GetHoudiniEngineLogoBrush();
		if (HoudiniBrush.IsValid())
			Info.Image = HoudiniBrush.Get();

		NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	}
#endif

	return true;
}

bool
FHoudiniEngine::UpdateTaskSlateNotification(const FText& InText)
{
#if WITH_EDITOR
	// task is till running
	// Just update the slate notification
	TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
	if (NotificationItem.IsValid())
		NotificationItem->SetText(InText);
#endif

	return true;
}

bool
FHoudiniEngine::FinishTaskSlateNotification(const FText& InText)
{
#if WITH_EDITOR
	if (NotificationPtr.IsValid())
	{
		TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
		if (NotificationItem.IsValid())
		{
			NotificationItem->SetText(InText);
			NotificationItem->ExpireAndFadeout();

			NotificationPtr.Reset();
		}
	}
#endif

	return true;
}

bool FHoudiniEngine::UpdateCookingNotification(const FText& InText, const bool bExpireAndFade)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngine::UpdateCookingNotification);

#if WITH_EDITOR
	TimeSinceLastPersistentNotification = 0.0;

	// Check whether we want to display Slate cooking and instantiation notifications.
	bool bDisplaySlateCookingNotifications = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

	if (!bDisplaySlateCookingNotifications)
		return false;

	if (!CookingNotificationPtr.IsValid())
	{
		FNotificationInfo Info(InText);
		Info.bFireAndForget = false;
		Info.FadeOutDuration = HAPI_UNREAL_NOTIFICATION_FADEOUT;
		Info.ExpireDuration = HAPI_UNREAL_NOTIFICATION_EXPIRE;
		const TSharedPtr< FSlateDynamicImageBrush > HoudiniBrush = FHoudiniEngine::Get().GetHoudiniEngineLogoBrush();
		if (HoudiniBrush.IsValid())
			Info.Image = HoudiniBrush.Get();

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngine::UpdateCookingNotification__AddNotification);
			CookingNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	TSharedPtr<SNotificationItem> NotificationItem = CookingNotificationPtr.Pin();
	if (NotificationItem.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngine::UpdateCookingNotification__UpdateNotification);

		// Update the persistent notification.
		NotificationItem->SetText(InText);
		
		// Instead of setting the boolean and fading the next tick, just fade & reset now
		if (bExpireAndFade)
		{
			NotificationItem->ExpireAndFadeout();
			CookingNotificationPtr.Reset();
		}
	}

#endif

	return true;
}

void FHoudiniEngine::TickCookingNotification(const float DeltaTime)
{
	if (CookingNotificationPtr.IsValid() && DeltaTime > 0.0f)
		TimeSinceLastPersistentNotification += DeltaTime;
}

void
FHoudiniEngine::UpdateSessionSyncInfoFromHoudini()
{
	if (!bEnableSessionSync)
		return;

	// Set the Session Sync settings to Houdini
	HAPI_SessionSyncInfo SessionSyncInfo;
	//FHoudiniApi::SessionSyncInfo_Create(&SessionSyncInfo);
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetSessionSyncInfo(GetSession(), &SessionSyncInfo))
	{
		bCookUsingHoudiniTime = SessionSyncInfo.cookUsingHoudiniTime;
		bSyncViewport = SessionSyncInfo.syncViewport;
	}	
}

void
FHoudiniEngine::UploadSessionSyncInfoToHoudini()
{
	// No need to set sessionsync info if we're not using session sync
	if (!bEnableSessionSync)
		return;

	// Set the Session Sync settings to Houdini
	HAPI_SessionSyncInfo SessionSyncInfo;
	SessionSyncInfo.cookUsingHoudiniTime = bCookUsingHoudiniTime;
	SessionSyncInfo.syncViewport = bSyncViewport;

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetSessionSyncInfo(GetSession(), &SessionSyncInfo))
		HOUDINI_LOG_WARNING(TEXT("Failed to set the SessionSync Infos."));
}

void
FHoudiniEngine::StartPDGCommandlet()
{
	if (HoudiniEngineManager)
		HoudiniEngineManager->StartPDGCommandlet();
}

void
FHoudiniEngine::StopPDGCommandlet()
{
	if (HoudiniEngineManager)
		HoudiniEngineManager->StopPDGCommandlet();
}

bool
FHoudiniEngine::IsPDGCommandletRunningOrConnected()
{
	if (HoudiniEngineManager)
		return HoudiniEngineManager->IsPDGCommandletRunningOrConnected();
	return false;
}

EHoudiniBGEOCommandletStatus
FHoudiniEngine::GetPDGCommandletStatus()
{
	if (HoudiniEngineManager)
		return HoudiniEngineManager->GetPDGCommandletStatus();
	return EHoudiniBGEOCommandletStatus::NotStarted;
}

void
FHoudiniEngine::UnregisterPostEngineInitCallback()
{
	if (PostEngineInitCallback.IsValid())
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitCallback);
}

bool 
FHoudiniEngine::IsSyncWithHoudiniCookEnabled() const
{
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	return HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bSyncWithHoudiniCook : false;
}

void
FHoudiniEngine::StartHAPIPerformanceMonitoring()
{
	// The HAPI stubs needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to Start a HAPI Performance Monitoring: The Houdini API stubs have not been properly initialized."));
		return;
	}

	// We need a Valid Session
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(GetSession()))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to Start a HAPI Performance Monitoring: The session is invalid."));
		return;
	}

	// Stop the current session if it was already started
	if (HAPIPerfomanceProfileID != -1)
		StopHAPIPerformanceMonitoring(FString());

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::StartPerformanceMonitorProfile(
		GetSession(), "HoudiniEngineForUnreal-HAPI-Profiling", &HAPIPerfomanceProfileID))
	{
		HAPIPerfomanceProfileID = -1;
		HOUDINI_LOG_ERROR(TEXT("Failed to Start a HAPI Performance Monitoring."));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("HAPI Performance Monitoring started."));
	}
}

void
FHoudiniEngine::StopHAPIPerformanceMonitoring(const FString& TraceDirectory)
{
	// The HAPI stubs needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to Start a HAPI Performance Monitoring: The Houdini API stubs have not been properly initialized."));
		return;
	}

	// We need a Valid Session
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(GetSession()))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to Start a HAPI Performance Monitoring: The session is invalid."));
		return;
	}

	if (HAPIPerfomanceProfileID == -1)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to Stop a HAPI Performance Monitoring: no performance profiling session was started."));	
	}

	// Build the filename
	int32 Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	FPlatformTime::SystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
	FString FileName = !TraceDirectory.IsEmpty() ? TraceDirectory + TEXT("\\") : TEXT("");
	FileName += TEXT("HAPI_UE_") + FString::Printf(TEXT("%d%02d%02d_%02d%02d%02d"), Year, Month, Day, Hour, Min, Sec) + TEXT(".hperf");

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::StopPerformanceMonitorProfile(
		GetSession(), HAPIPerfomanceProfileID, H_TCHAR_TO_UTF8(*FileName)))
	{
		HAPIPerfomanceProfileID = -1;
		HOUDINI_LOG_ERROR(TEXT("Failed to Stop HAPI Performance Monitoring."));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("HAPI Performance Monitoring saved - %s."), *FPaths::ConvertRelativePathToFull(FileName));
	}
}


FString
FHoudiniEngine::RegisterNewHoudiniAssetEditor()
{
	FString Identifier = TEXT("HoudiniAssetEditor");
#if WITH_EDITOR	
	if (HoudiniAssetEditorIdentifiers.Num() <= 0)
	{
		HoudiniAssetEditorIdentifiers.Add(0);
		return Identifier;
	}

	// Find the next available index
	int32 Idx = 0;
	for (auto CurId : HoudiniAssetEditorIdentifiers)
	{
		if (CurId == Idx)
		{
			// ID is taken - keep looking
			Idx++;
			continue;
		}
		else
		{
			// We found an available ID - return
			break;
		}
	}

	HoudiniAssetEditorIdentifiers.Add(Idx);
	if(Idx > 0)
		Identifier += FString::FromInt(Idx);
#endif
	return Identifier;
}

void
FHoudiniEngine::UnRegisterHoudiniAssetEditor(const FString& InIdentifier)
{
	// Extract the ID from the string
	FString StringID = InIdentifier.RightChop(18);
	int32 ID = StringID.IsEmpty() ? 0 : FCString::Atoi(*StringID);

	// Remove the Id from the registered asset editor array
	for (int32 Idx = HoudiniAssetEditorIdentifiers.Num() - 1; Idx >= 0; Idx--)
	{
		if (HoudiniAssetEditorIdentifiers[Idx] != ID)
			continue;

		HoudiniAssetEditorIdentifiers.RemoveAt(Idx);
		//break;
	}
}

TArray<FName>
FHoudiniEngine::GetAllHoudiniAssetEditorIdentifier()
{
	TArray<FName> IDArray;
	FString BaseIdentifier = TEXT("HoudiniAssetEditor");

	for (auto CurId : HoudiniAssetEditorIdentifiers)
	{		
		FString CurIDAsString = CurId == 0 ? BaseIdentifier : BaseIdentifier + FString::FromInt(CurId);
		IDArray.Add(FName(*CurIDAsString));
	}

	return IDArray;
}

bool
FHoudiniEngine::OpenSessionSync(bool bWaitForCompletion)
{
	//if (!FHoudiniEngine::IsInitialized())
	//	return;

	if(!FHoudiniEngine::Get().StopSession())
	{
		// StopSession returns false only if Houdini is not initialized
		HOUDINI_LOG_ERROR(TEXT("Failed to start Session Sync - HAPI Not initialized"));
		return false;
	}

	// Get the runtime settings to get the session/type and settings
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	EHoudiniRuntimeSettingsSessionType SessionType = HoudiniRuntimeSettings->SessionType;
	FString ServerPipeName = HoudiniRuntimeSettings->ServerPipeName;
	int32 ServerPort = HoudiniRuntimeSettings->ServerPort;
	int64 BufferSize = HoudiniRuntimeSettings->SharedMemoryBufferSize;
	bool BufferCyclic = HoudiniRuntimeSettings->bSharedMemoryBufferCyclic;
	bool SharedMemDataTransfer = HoudiniRuntimeSettings->bEnableSharedMemoryDataTransfer;

	FString SessionSyncArgs = TEXT("-hess=");
	if(SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe)
	{
		// Add the -hess=pipe:hapi argument
		SessionSyncArgs += TEXT("pipe:") + ServerPipeName;
	}
	else if(SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_Socket)
	{
		// Add the -hess=port:9090 argument
		SessionSyncArgs += TEXT("port:") + FString::FromInt(ServerPort);
	}
	else if(SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_MemoryBuffer)
	{
		// -hess=shared:TYPE:SIZE:NAME
		// TYPE specifies the shared memory buffer type. (ring, fixed).
		// SIZE specifies the size of the shared memory buffer in megabytes (MB).
		// NAME specifies the name of the shared memory. Different sessions must have a unique name.
		FString BufferType = BufferCyclic ? TEXT("ring") : TEXT("fixed");
		SessionSyncArgs += TEXT("shared:") + BufferType + TEXT(":") + FString::FromInt(BufferSize) + TEXT(":") + ServerPipeName;
	}
	else
	{
		// Invalid session type
		HOUDINI_LOG_ERROR(TEXT("Failed to start Session Sync - Invalid session type"));
		return false;
	}

#if PLATFORM_MAC || PLATFORM_LINUX
	// Houdini forks into the background by default on macOS and Linux
// so we need to explicitly tell it to not fork.
	SessionSyncArgs += TEXT(" -foreground");
#endif

	// Add a slate notification
	FString Notification = TEXT("Opening Houdini Session Sync...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Opening Houdini Session Sync."));

	// Only launch Houdini in Session sync if we havent started it already!
	FProcHandle PreviousHESS = FHoudiniEngine::Get().GetHESSProcHandle();
	if(!FPlatformProcess::IsProcRunning(PreviousHESS))
	{
		// Start houdini with the -hess commandline args
		const FString ThisLibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
#if PLATFORM_MAC
		const FString HoudiniExeLocationRelativeToLibHAPI = TEXT("/../Resources/bin");
#elif PLATFORM_LINUX
		const FString HoudiniExeLocationRelativeToLibHAPI = TEXT("/../bin");
#elif PLATFORM_WINDOWS
		const FString HoudiniExeLocationRelativeToLibHAPI;
#else
		// Treat an unknown platform the same as Windows for now
		const FString HoudiniExeLocationRelativeToLibHAPI;
#endif

		// Set custom $HOME env var if it's been specified
		FHoudiniEngineRuntimeUtils::SetHoudiniHomeEnvironmentVariable();

		FString HoudiniExecutable = FHoudiniEngine::Get().GetHoudiniExecutable();
		FString HoudiniLocation = ThisLibHAPILocation + HoudiniExeLocationRelativeToLibHAPI + TEXT("/") + HoudiniExecutable;
		HOUDINI_LOG_MESSAGE(TEXT("Path to houdini executable: %s"), *HoudiniLocation);
		FProcHandle HESSHandle = FPlatformProcess::CreateProc(
			*HoudiniLocation,
			*SessionSyncArgs,
			true, false, false,
			nullptr, 0,
			*FPlatformProcess::GetCurrentWorkingDirectory(),
			nullptr, nullptr);

		if(!HESSHandle.IsValid())
		{
			// Try with the steam version executable instead
			HoudiniLocation = ThisLibHAPILocation + HoudiniExeLocationRelativeToLibHAPI + TEXT("/hindie.steam");
			HOUDINI_LOG_MESSAGE(TEXT("Path to hindie.steam executable: %s"), *HoudiniLocation);

			HESSHandle = FPlatformProcess::CreateProc(
				*HoudiniLocation,
				*SessionSyncArgs,
				true, false, false,
				nullptr, 0,
				*FPlatformProcess::GetCurrentWorkingDirectory(),
				nullptr, nullptr);

			if(!HESSHandle.IsValid())
			{
				HOUDINI_LOG_ERROR(TEXT("Failed to launch Houdini in Session Sync mode."));
				return false;
			}
		}

		// Keep track of the SessionSync ProcHandle
		FHoudiniEngine::Get().SetHESSProcHandle(HESSHandle);
	}

	if(!bWaitForCompletion)
	{
		Async(EAsyncExecution::TaskGraphMainThread, [SessionType, ServerPipeName, ServerPort, BufferSize, BufferCyclic]()
			{
				StartAndConnectToSessionSync(SessionType, ServerPipeName, ServerPort, BufferSize, BufferCyclic);
			});
	}
	else
	{
		StartAndConnectToSessionSync(SessionType, ServerPipeName, ServerPort, BufferSize, BufferCyclic);
	}
	return true;
}

bool
FHoudiniEngine::StartAndConnectToSessionSync(
	const EHoudiniRuntimeSettingsSessionType SessionType,
	const FString& ServerPipeName,
	const int32 ServerPort,
	const int64 BufferSize,
	const bool BufferCyclic)
{
	// Use a timeout to avoid waiting indefinitely for H to start in session sync mode
	const double Timeout = 180.0; // 3min
	const double StartTimestamp = FPlatformTime::Seconds();

	const FString ServerHost = TEXT("localhost");
	constexpr int32 NumSessions = 1;
	while(!FHoudiniEngine::Get().SessionSyncConnect(
		SessionType, NumSessions, ServerPipeName, ServerHost, ServerPort, BufferSize, BufferCyclic))
	{
		// Houdini might not be done loading, sleep for one second 
		FPlatformProcess::Sleep(.5f);

		// Check for license error
		int32 HESSReturnCode;
		FProcHandle HESSHandle = FHoudiniEngine::Get().GetHESSProcHandle();
		if(FPlatformProcess::GetProcReturnCode(HESSHandle, &HESSReturnCode))
		{
			FString Notification = TEXT("Failed to start SessionSync...");
			FHoudiniEngineUtils::CreateSlateNotification(Notification);

			switch(HESSReturnCode)
			{
			case 3:
				HOUDINI_LOG_ERROR(TEXT("Failed to start SessionSync - No licenses were available"));
				FHoudiniEngine::Get().SetSessionStatus(EHoudiniSessionStatus::NoLicense);
				return false;
				break;
			default:
				HOUDINI_LOG_ERROR(TEXT("Failed to start SessionSync - Unknown error"));
				FHoudiniEngine::Get().SetSessionStatus(EHoudiniSessionStatus::Failed);
				return false;
				break;
			}
		}

		// Check for the timeout
		if(FPlatformTime::Seconds() - StartTimestamp > Timeout)
		{
			// ... and a log message
			HOUDINI_LOG_ERROR(TEXT("Failed to start SessionSync - Timeout..."));
			return false;
		}
	}

	// Initialize HAPI with this session
	if(!FHoudiniEngine::Get().InitializeHAPISession())
	{
		FHoudiniEngine::Get().StopTicking(true);
		return false;
	}

	// Notify all cookables that they need to instantiate in the new session
	FHoudiniEngineUtils::MarkAllCookablesAsNeedInstantiation();

	// Start ticking
	FHoudiniEngine::Get().StartTicking(true);

	// Add a slate notification
	FString Notification = TEXT("Successfully connected to Session Sync...");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	// ... and a log message
	HOUDINI_LOG_MESSAGE(TEXT("Successfully connected to Session Sync..."));

	return true;
}

#undef LOCTEXT_NAMESPACE

