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
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniAssetComponent.h"
#include "HAPI/HAPI_Version.h"

#include "Modules/ModuleManager.h"
#include "Misc/ScopeLock.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "ISettingsModule.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"
#include "Logging/LogMacros.h"

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
{
	Session.type = HAPI_SESSION_MAX;
	Session.id = -1;

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

#if WITH_EDITOR
	// Unregister settings.
	ISettingsModule * SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
		SettingsModule->UnregisterSettings("Project", "Plugins", "HoudiniEngine");
#endif

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

/*
void
FHoudiniEngine::AddHoudiniAssetComponent(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return;

	if (HoudiniEngineManager)
		HoudiniEngineManager->AddComponent(HAC);
}
*/

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

const HAPI_Session *
FHoudiniEngine::GetSession() const
{
	return Session.type == HAPI_SESSION_MAX ? nullptr : &Session;
}

const EHoudiniSessionStatus&
FHoudiniEngine::GetSessionStatus() const
{
	return SessionStatus;
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
FHoudiniEngine::StartSession(HAPI_Session*& SessionPtr,
	const bool& StartAutomaticServer,
	const float& AutomaticServerTimeout,
	const EHoudiniRuntimeSettingsSessionType& SessionType,
	const FString& ServerPipeName,
	const int32& ServerPort,
	const FString& ServerHost)
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	// Only start a new Session if we dont already have a valid one
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(SessionPtr))
		return true;

	// Set the HAPI_CLIENT_NAME environment variable to "unreal"
	// We need to do this before starting HARS.
	FPlatformMisc::SetEnvironmentVar(TEXT("HAPI_CLIENT_NAME"), TEXT("unreal"));

	HAPI_Result SessionResult = HAPI_RESULT_FAILURE;

	HAPI_ThriftServerOptions ServerOptions;
	FMemory::Memzero< HAPI_ThriftServerOptions >(ServerOptions);
	ServerOptions.autoClose = true;
	ServerOptions.timeoutMs = AutomaticServerTimeout;

	// Unless we automatically start the server,
	// consider we're in SessionSync mode
	bEnableSessionSync = true;

	auto UpdatePathForServer = [&]
	{
		// Modify our PATH so that HARC will find HARS.exe
		const TCHAR* PathDelimiter = FPlatformMisc::GetPathVarDelimiter();

		FString OrigPathVar = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));

		FString ModifiedPath =
#if PLATFORM_MAC
			// On Mac our binaries are split between two folders
			LibHAPILocation + TEXT("/../Resources/bin") + PathDelimiter +
#endif
			LibHAPILocation + PathDelimiter + OrigPathVar;

		FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *ModifiedPath);
	};


	// Clear the connection error before starting a new session
	if(SessionType != EHoudiniRuntimeSettingsSessionType::HRSST_None)
		FHoudiniApi::ClearConnectionError();

	switch (SessionType)
	{
		case EHoudiniRuntimeSettingsSessionType::HRSST_Socket:
		{
			// Try to connect to an existing socket session first
			SessionResult = FHoudiniApi::CreateThriftSocketSession(
				SessionPtr, TCHAR_TO_UTF8(*ServerHost), ServerPort );

			// Start a session and try to connect to it if we failed
			if ( StartAutomaticServer && SessionResult != HAPI_RESULT_SUCCESS )
			{
				UpdatePathForServer();
				FHoudiniApi::StartThriftSocketServer(
					&ServerOptions, ServerPort, nullptr);

				// We've started the server manually, disable session sync
				bEnableSessionSync = false;

				SessionResult = FHoudiniApi::CreateThriftSocketSession(
					SessionPtr, TCHAR_TO_UTF8(*ServerHost), ServerPort);
			}
		}
		break;

		case EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe:
		{
			// Try to connect to an existing pipe session first
			SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
				SessionPtr, TCHAR_TO_UTF8(*ServerPipeName) );

			// Start a session and try to connect to it if we failed
			if (StartAutomaticServer && SessionResult != HAPI_RESULT_SUCCESS)
			{
				UpdatePathForServer();
				FHoudiniApi::StartThriftNamedPipeServer(
					&ServerOptions, TCHAR_TO_UTF8(*ServerPipeName), nullptr);

				// We've started the server manually, disable session sync
				bEnableSessionSync = false;

				SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
					SessionPtr, TCHAR_TO_UTF8(*ServerPipeName));
			}
		}
		break;

		case EHoudiniRuntimeSettingsSessionType::HRSST_None:
		{
			HOUDINI_LOG_MESSAGE(TEXT("Session type set to None, Cooking is disabled."));
			// Disable session sync
			bEnableSessionSync = false;
			break;
		}

		case EHoudiniRuntimeSettingsSessionType::HRSST_InProcess:
			// As of Unreal 4.19, InProcess sessions are not supported anymore
			SessionResult = FHoudiniApi::CreateInProcessSession(SessionPtr);
			// Disable session sync
			bEnableSessionSync = false;
			break;

		default:
			HOUDINI_LOG_ERROR(TEXT("Unsupported Houdini Engine session type"));
			// Disable session sync
			bEnableSessionSync = false;
			break;
	}

	// Stop here if we used a none session
	if (SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_None)
		return false;

	FHoudiniEngine::Get().SetFirstSessionCreated(true);

	if (SessionResult != HAPI_RESULT_SUCCESS || !SessionPtr)
	{
		// Disable session sync as well?
		bEnableSessionSync = false;

		if (SessionType != EHoudiniRuntimeSettingsSessionType::HRSST_InProcess)
		{
			FString ConnectionError = FHoudiniEngineUtils::GetConnectionError();
			if(!ConnectionError.IsEmpty())
				HOUDINI_LOG_ERROR(TEXT("Houdini Engine Session failed to connect -  %s"), *ConnectionError);
		}

		return false;
	}		

	// Update this session's license type
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetSessionEnvInt(
		SessionPtr, HAPI_SESSIONENVINT_LICENSE, (int32 *)&LicenseType));

	return true;
}

bool
FHoudiniEngine::SessionSyncConnect(
	const EHoudiniRuntimeSettingsSessionType& SessionType,
	const FString& ServerPipeName,
	const FString& ServerHost,
	const int32& ServerPort)
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	// Only start a new Session if we dont already have a valid one
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(&Session))
		return true;

	// Consider the session failed as long as we dont connect
	SetSessionStatus(EHoudiniSessionStatus::Failed);

	HAPI_Result SessionResult = HAPI_RESULT_FAILURE;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	
	HAPI_ThriftServerOptions ServerOptions;
	FMemory::Memzero< HAPI_ThriftServerOptions >(ServerOptions);
	ServerOptions.autoClose = true;
	ServerOptions.timeoutMs = HoudiniRuntimeSettings->AutomaticServerTimeout;

	switch (SessionType)
	{
	case EHoudiniRuntimeSettingsSessionType::HRSST_Socket:
	{
		// Try to connect to an existing socket session first
		SessionResult = FHoudiniApi::CreateThriftSocketSession(
			&Session, TCHAR_TO_UTF8(*ServerHost), ServerPort);
	}
	break;

	case EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe:
	{
		// Try to connect to an existing pipe session first
		SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
			&Session, TCHAR_TO_UTF8(*ServerPipeName));
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

	// Update this session's license type
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetSessionEnvInt(
		&Session, HAPI_SESSIONENVINT_LICENSE, (int32 *)&LicenseType));

	// Update the default viewport sync settings
	bSyncViewport = HoudiniRuntimeSettings->bSyncViewport;
	bSyncHoudiniViewport = HoudiniRuntimeSettings->bSyncHoudiniViewport;
	bSyncUnrealViewport = HoudiniRuntimeSettings->bSyncUnrealViewport;

	return true;
}

bool
FHoudiniEngine::InitializeHAPISession()
{
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
		&Session,
		&CookOptions,
		bUseCookingThread,
		HoudiniRuntimeSettings->CookingThreadStackSize,
		TCHAR_TO_UTF8(*HoudiniRuntimeSettings->HoudiniEnvironmentFiles),
		TCHAR_TO_UTF8(*HoudiniRuntimeSettings->OtlSearchPath),
		TCHAR_TO_UTF8(*HoudiniRuntimeSettings->DsoSearchPath),
		TCHAR_TO_UTF8(*HoudiniRuntimeSettings->ImageDsoSearchPath),
		TCHAR_TO_UTF8(*HoudiniRuntimeSettings->AudioDsoSearchPath));
	
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
	FHoudiniApi::SetServerEnvString(&Session, HAPI_ENV_CLIENT_NAME, HAPI_UNREAL_CLIENT_NAME);

	if (bEnableSessionSync)
	{
		// Set the session sync infos if needed
		UploadSessionSyncInfoToHoudini();

		// Indicate that Session Sync is enabled
		FString Notification = TEXT("Houdini Engine Session Sync enabled.");
		FHoudiniEngineUtils::CreateSlateNotification(Notification);
		HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Session Sync enabled."));		
	}

	return true;
}


void
FHoudiniEngine::OnSessionLost()
{
	// Mark the session as invalid
	Session.id = -1;
	Session.type = HAPI_SESSION_MAX;
	SetSessionStatus(EHoudiniSessionStatus::Lost);

	bEnableSessionSync = false;
	HoudiniEngineManager->StopHoudiniTicking();

	// This indicates that we likely have lost the session due to a crash in HARS/Houdini
	FString Notification = TEXT("Houdini Engine Session lost!");
	FHoudiniEngineUtils::CreateSlateNotification(Notification, 2.0, 4.0);

	HOUDINI_LOG_ERROR(TEXT("Houdini Engine Session lost! This could be caused by a crash in HARS."));
}

bool
FHoudiniEngine::StopSession()
{
	HAPI_Session* SessionPtr = &Session;
	return StopSession(SessionPtr);
}

bool
FHoudiniEngine::StopSession(HAPI_Session*& SessionPtr)
{
	// HAPI needs to be initialized
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	if (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(SessionPtr))
	{
		// SessionPtr is valid, clean up and close the session
		FHoudiniApi::Cleanup(SessionPtr);
		FHoudiniApi::CloseSession(SessionPtr);
	}

	Session.id = -1;
	Session.type = HAPI_SESSION_MAX;
	SetSessionStatus(EHoudiniSessionStatus::Stopped);
	bEnableSessionSync = false;

	HoudiniEngineManager->StopHoudiniTicking();

	return true;
}

bool
FHoudiniEngine::RestartSession()
{
	HAPI_Session* SessionPtr = &Session;

	FString StatusText = TEXT("Starting the Houdini Engine session...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Make sure we stop the current session if it is still valid
	bool bSuccess = false;
	if (!StopSession(SessionPtr))
	{
		// StopSession returns false only if Houdini is not initialized
		HOUDINI_LOG_ERROR(TEXT("Failed to restart the Houdini Engine session - HAPI Not initialized"));
	}
	else
	{
		// Try to reconnect/start a new session
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (!StartSession(
			SessionPtr, 
			HoudiniRuntimeSettings->bStartAutomaticServer,
			HoudiniRuntimeSettings->AutomaticServerTimeout,
			HoudiniRuntimeSettings->SessionType,
			HoudiniRuntimeSettings->ServerPipeName,
			HoudiniRuntimeSettings->ServerPort,
			HoudiniRuntimeSettings->ServerHost))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to restart the Houdini Engine session - Failed to start the new Session"));
			SetSessionStatus(EHoudiniSessionStatus::Failed);
		}
		else
		{
			// Now initialize HAPI with this session
			if (!InitializeHAPISession())
			{
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

	// Start ticking only if we successfully started the session
	if (bSuccess)
	{
		StartTicking();
		return true;
	}
	else
	{
		StopTicking();
		return false;
	}
}

bool
FHoudiniEngine::CreateSession(const EHoudiniRuntimeSettingsSessionType& SessionType, FName OverrideServerPipeName)
{
	HAPI_Session* SessionPtr = &Session;

	FString StatusText = TEXT("Create the Houdini Engine session...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Make sure we stop the current session if it is still valid
	bool bSuccess = false;

	// Try to reconnect/start a new session
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (!StartSession(
		SessionPtr,
		true,
		HoudiniRuntimeSettings->AutomaticServerTimeout,
		SessionType,
		OverrideServerPipeName == NAME_None ? HoudiniRuntimeSettings->ServerPipeName : OverrideServerPipeName.ToString(),
		HoudiniRuntimeSettings->ServerPort,
		HoudiniRuntimeSettings->ServerHost))
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

	// Start ticking only if we successfully started the session
	if (bSuccess)
	{
		StartTicking();
		return true;
	}
	else
	{
		StopTicking();
		return false;
	}
}

bool
FHoudiniEngine::ConnectSession(const EHoudiniRuntimeSettingsSessionType& SessionType)
{
	HAPI_Session* SessionPtr = &Session;

	FString StatusText = TEXT("Connecting to a Houdini Engine session...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Make sure we stop the current session if it is still valid
	bool bSuccess = false;

	// Try to reconnect/start a new session
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (!StartSession(
		SessionPtr,
		false,
		HoudiniRuntimeSettings->AutomaticServerTimeout,
		SessionType,
		HoudiniRuntimeSettings->ServerPipeName,
		HoudiniRuntimeSettings->ServerPort,
		HoudiniRuntimeSettings->ServerHost))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to connect to the Houdini Engine Session"));
		SetSessionStatus(EHoudiniSessionStatus::Failed);
	}
	else
	{
		// Now initialize HAPI with this session
		if (!InitializeHAPISession())
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to connect to the Houdini Engine session - Failed to initialize HAPI"));
			SetSessionStatus(EHoudiniSessionStatus::Failed);
		}
		else
		{
			bSuccess = true;
			SetSessionStatus(EHoudiniSessionStatus::Connected);
		}
	}

	// Start ticking only if we successfully started the session
	if (bSuccess)
	{
		StartTicking();
		return true;
	}
	else
	{
		StopTicking();
		return false;
	}
}

void
FHoudiniEngine::StartTicking()
{
	// Finish the notification and display the results
	FString StatusText = TEXT("Houdini Engine session connected.");
	FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(StatusText));

	HoudiniEngineManager->StartHoudiniTicking();
}

void
FHoudiniEngine::StopTicking()
{
	// Finish the notification and display the results
	FString StatusText = TEXT("Failed to start the Houdini Engine session...");
	FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(StatusText));

	HoudiniEngineManager->StopHoudiniTicking();

	HAPI_Session* SessionPtr = &Session;
	StopSession(SessionPtr);	
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

		/*
		if (!IsPIEActive())
		*/

		NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		//FSlateNotificationManager::Get().Tick();
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

	//FSlateNotificationManager::Get().Tick();
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
#if WITH_EDITOR
	// Check whether we want to display Slate cooking and instantiation notifications.
	bool bDisplaySlateCookingNotifications = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

	if (!bDisplaySlateCookingNotifications)
		return false;
	
	UpdatePersistentNotification(InText, bExpireAndFade);
	
#endif
	return true;
}

bool
FHoudiniEngine::UpdatePersistentNotification(const FText& InText, const bool bExpireAndFade)
{
#if WITH_EDITOR
	TimeSinceLastPersistentNotification = 0.0;

	if (!PersistentNotificationPtr.IsValid())
	{
		FNotificationInfo Info(InText);
		Info.bFireAndForget = false;
		Info.FadeOutDuration = HAPI_UNREAL_NOTIFICATION_FADEOUT;
		Info.ExpireDuration = HAPI_UNREAL_NOTIFICATION_EXPIRE;
		const TSharedPtr< FSlateDynamicImageBrush > HoudiniBrush = FHoudiniEngine::Get().GetHoudiniEngineLogoBrush();
		if (HoudiniBrush.IsValid())
			Info.Image = HoudiniBrush.Get();


		PersistentNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		//FSlateNotificationManager::Get().Tick();
	}

	TSharedPtr<SNotificationItem> NotificationItem = PersistentNotificationPtr.Pin();

	if (NotificationItem.IsValid())
	{
		// Update the persistent notification.
		NotificationItem->SetText(InText);
		bPersistentAllowExpiry = bExpireAndFade;
	}

	//FSlateNotificationManager::Get().Tick();
#endif

	return true;
}

void FHoudiniEngine::TickPersistentNotification(const float DeltaTime)
{
	if (PersistentNotificationPtr.IsValid() && DeltaTime > 0.0f)
	{
		TimeSinceLastPersistentNotification += DeltaTime;
		if (bPersistentAllowExpiry && TimeSinceLastPersistentNotification > HAPI_UNREAL_NOTIFICATION_EXPIRE)
		{
			TSharedPtr<SNotificationItem> NotificationItem = PersistentNotificationPtr.Pin();
			if (NotificationItem.IsValid())
			{
				NotificationItem->Fadeout();
				PersistentNotificationPtr.Reset();
			}
		}
	}

	// Tick the notification manager
	//FSlateNotificationManager::Get().Tick();
}

void
FHoudiniEngine::UpdateSessionSyncInfoFromHoudini()
{
	if (!bEnableSessionSync)
		return;

	// Set the Session Sync settings to Houdini
	HAPI_SessionSyncInfo SessionSyncInfo;
	//FHoudiniApi::SessionSyncInfo_Create(&SessionSyncInfo);

	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetSessionSyncInfo(&Session, &SessionSyncInfo))
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

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetSessionSyncInfo(&Session, &SessionSyncInfo))
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

bool FHoudiniEngine::IsSyncWithHoudiniCookEnabled() const
{
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	return HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bSyncWithHoudiniCook : false;
}

#undef LOCTEXT_NAMESPACE

