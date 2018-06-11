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
*/

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeUtils.h"
#include "HoudiniEngineInstancerUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniRuntimeSettings.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/ScopeLock.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/Material.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 


const FName FHoudiniEngine::HoudiniEngineAppIdentifier = FName( TEXT( "HoudiniEngineApp" ) );

IMPLEMENT_MODULE( FHoudiniEngine, HoudiniEngineRuntime );
DEFINE_LOG_CATEGORY( LogHoudiniEngine );

FHoudiniEngine *
FHoudiniEngine::HoudiniEngineInstance = nullptr;

FHoudiniEngine::FHoudiniEngine()
    : HoudiniLogoStaticMesh( nullptr )
    , HoudiniDefaultMaterial( nullptr )
    , HoudiniBgeoAsset( nullptr )
    , HoudiniEngineSchedulerThread( nullptr )
    , HoudiniEngineScheduler( nullptr )
    , EnableCookingGlobal( true )
{
    Session.type = HAPI_SESSION_MAX;
    Session.id = -1;
}

#if WITH_EDITOR

TSharedPtr< FSlateDynamicImageBrush >
FHoudiniEngine::GetHoudiniLogoBrush() const
{
    return HoudiniLogoBrush;
}

#endif

TWeakObjectPtr<UStaticMesh>
FHoudiniEngine::GetHoudiniLogoStaticMesh() const
{
    return HoudiniLogoStaticMesh;
}

TWeakObjectPtr<UMaterial>
FHoudiniEngine::GetHoudiniDefaultMaterial() const
{
    return HoudiniDefaultMaterial;
}

TWeakObjectPtr<UHoudiniAsset>
FHoudiniEngine::GetHoudiniBgeoAsset() const
{
    return HoudiniBgeoAsset;
}

bool
FHoudiniEngine::CheckHapiVersionMismatch() const
{
    return bHAPIVersionMismatch;
}

const FString &
FHoudiniEngine::GetLibHAPILocation() const
{
    return LibHAPILocation;
}

HAPI_Result
FHoudiniEngine::GetHapiState() const
{
    return HAPIState;
}

void
FHoudiniEngine::SetHapiState( HAPI_Result Result )
{
    HAPIState = Result;
}

const HAPI_Session *
FHoudiniEngine::GetSession() const
{
    return Session.type == HAPI_SESSION_MAX ? nullptr : &Session;
}

FHoudiniEngine &
FHoudiniEngine::Get()
{
    check( FHoudiniEngine::HoudiniEngineInstance );
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
    bHAPIVersionMismatch = false;
    HAPIState = HAPI_RESULT_NOT_INITIALIZED;

    HOUDINI_LOG_MESSAGE( TEXT( "Starting the Houdini Engine module." ) );

#if WITH_EDITOR
    // Register settings.
    if( ISettingsModule * SettingsModule = FModuleManager::GetModulePtr< ISettingsModule >( "Settings" ) )
    {
        SettingsModule->RegisterSettings(
            "Project", "Plugins", "HoudiniEngine",
            LOCTEXT( "RuntimeSettingsName", "Houdini Engine" ),
            LOCTEXT( "RuntimeSettingsDescription", "Configure the HoudiniEngine plugin" ),
            GetMutableDefault< UHoudiniRuntimeSettings >() );
    }

    // Before starting the module, we need to locate and load HAPI library.
    {
        void * HAPILibraryHandle = FHoudiniEngineUtils::LoadLibHAPI( LibHAPILocation );

        if ( HAPILibraryHandle )
        {
            FHoudiniApi::InitializeHAPI( HAPILibraryHandle );
        }
        else
        {
            // Get platform specific name of libHAPI.
            FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();
            HOUDINI_LOG_MESSAGE( TEXT( "Failed locating or loading %s" ), *LibHAPIName );
        }
    }

#endif

    // Create static mesh Houdini logo.
    HoudiniLogoStaticMesh = LoadObject< UStaticMesh >(
        nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_LOGO, nullptr, LOAD_None, nullptr );
    if ( HoudiniLogoStaticMesh.IsValid() )
        HoudiniLogoStaticMesh->AddToRoot();

    // Create default material.
    HoudiniDefaultMaterial = LoadObject< UMaterial >(
        nullptr, HAPI_UNREAL_RESOURCE_HOUDINI_MATERIAL, nullptr, LOAD_None, nullptr );
    if ( HoudiniDefaultMaterial.IsValid() )
        HoudiniDefaultMaterial->AddToRoot();

    // Create Houdini digital asset which is used for loading the bgeo files.
    HoudiniBgeoAsset = LoadObject< UHoudiniAsset >(
        nullptr, HAPI_UNREAL_RESOURCE_BGEO_IMPORT, nullptr, LOAD_None, nullptr );
    if ( HoudiniBgeoAsset.IsValid() )
        HoudiniBgeoAsset->AddToRoot();

#if WITH_EDITOR

    if ( !IsRunningCommandlet() && !IsRunningDedicatedServer() )
    {
        // Create Houdini logo brush.
        const TArray< TSharedRef< IPlugin > > Plugins = IPluginManager::Get().GetDiscoveredPlugins();
        for ( auto PluginIt( Plugins.CreateConstIterator() ); PluginIt; ++PluginIt )
        {
            const TSharedRef< IPlugin > & Plugin = *PluginIt;
            if ( Plugin->GetName() == TEXT( "HoudiniEngine" ) )
            {
                FString Icon128FilePath = Plugin->GetBaseDir() / TEXT( "Resources/Icon128.png" );

                if ( FPlatformFileManager::Get().GetPlatformFile().FileExists( *Icon128FilePath ) )
                {
                    const FName BrushName( *Icon128FilePath );
                    const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource( BrushName );

                    if ( Size.X > 0 && Size.Y > 0 )
                    {
                        static const int32 ProgressIconSize = 32;
                        HoudiniLogoBrush = MakeShareable( new FSlateDynamicImageBrush(
                            BrushName, FVector2D( ProgressIconSize, ProgressIconSize ) ) );
                    }
                }

                break;
            }
        }
    }

    // Build and running versions match, we can perform HAPI initialization.
    if ( FHoudiniApi::IsHAPIInitialized() )
    {
        const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

        HAPI_Result SessionResult = HAPI_RESULT_FAILURE;

        HAPI_ThriftServerOptions ServerOptions;
        FMemory::Memzero< HAPI_ThriftServerOptions >( ServerOptions );
        ServerOptions.autoClose = true;
        ServerOptions.timeoutMs = HoudiniRuntimeSettings->AutomaticServerTimeout;

        auto UpdatePathForServer = [&] {
            // Modify our PATH so that HARC will find HARS.exe
            const TCHAR* PathDelimiter = FPlatformMisc::GetPathVarDelimiter();
            const int32 MaxPathVarLen = 32768;
            TCHAR OrigPathVarMem[ MaxPathVarLen ];
            FPlatformMisc::GetEnvironmentVariable( TEXT( "PATH" ), OrigPathVarMem, MaxPathVarLen );
            FString OrigPathVar( OrigPathVarMem );

            FString ModifiedPath =
#if PLATFORM_MAC
            // On Mac our binaries are split between two folders
            LibHAPILocation + TEXT( "/../Resources/bin" ) + PathDelimiter +
#endif
            LibHAPILocation + PathDelimiter + OrigPathVar;

            FPlatformMisc::SetEnvironmentVar( TEXT( "PATH" ), *ModifiedPath );
        };

        switch ( HoudiniRuntimeSettings->SessionType.GetValue() )
        {
            case EHoudiniRuntimeSettingsSessionType::HRSST_InProcess:
            {
                // As of Unreal 4.19, InProcess sessions are not supported anymore
                /*
                SessionResult = FHoudiniApi::CreateInProcessSession(&this->Session);
#if PLATFORM_WINDOWS
                // Workaround for Houdini libtools setting stdout to binary
                FWindowsPlatformMisc::SetUTF8Output();
#endif
                */

                // Create an auto started pipe session instead using default values
                UpdatePathForServer();
                FHoudiniApi::StartThriftNamedPipeServer(
                    &ServerOptions,
                    TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ),
                    nullptr );

                SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
                    &this->Session, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ) );

                break;
            }

            case EHoudiniRuntimeSettingsSessionType::HRSST_Socket:
            {
                if ( HoudiniRuntimeSettings->bStartAutomaticServer )
                {
                    UpdatePathForServer();

                    FHoudiniApi::StartThriftSocketServer( &ServerOptions, HoudiniRuntimeSettings->ServerPort, nullptr );
                }

                SessionResult = FHoudiniApi::CreateThriftSocketSession(
                    &this->Session,
                    TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerHost ),
                    HoudiniRuntimeSettings->ServerPort );

                break;
            }

            case EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe:
            {
                if ( HoudiniRuntimeSettings->bStartAutomaticServer )
                {
                    UpdatePathForServer();

                    FHoudiniApi::StartThriftNamedPipeServer(
                        &ServerOptions,
                        TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ),
                        nullptr );
                }

                SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
                    &this->Session, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ) );

                break;
            }

            default:

                HOUDINI_LOG_ERROR( TEXT( "Unsupported Houdini Engine session type" ) );
        }

        const HAPI_Session * SessionPtr = GetSession();
        if ( SessionResult != HAPI_RESULT_SUCCESS || !SessionPtr )
        {
            if ( ( HoudiniRuntimeSettings->SessionType.GetValue() == EHoudiniRuntimeSettingsSessionType::HRSST_Socket ||
                HoudiniRuntimeSettings->SessionType.GetValue() == EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe ) &&
                ! HoudiniRuntimeSettings->bStartAutomaticServer )
            {
                HOUDINI_LOG_ERROR( TEXT( "Failed to create a Houdini Engine session.  Check that a Houdini Engine Debugger session or HARS server is running" ) );
            }
            else
            {
                HOUDINI_LOG_ERROR( TEXT( "Failed to create a Houdini Engine session" ) );
            }
        }

        // We need to make sure HAPI version is correct.
        int32 RunningEngineMajor = 0;
        int32 RunningEngineMinor = 0;
        int32 RunningEngineApi = 0;

        // Retrieve version numbers for running Houdini Engine.
        FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor );
        FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor );
        FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi );

        // Compare defined and running versions.
        if ( RunningEngineMajor == HAPI_VERSION_HOUDINI_ENGINE_MAJOR &&
           RunningEngineMinor == HAPI_VERSION_HOUDINI_ENGINE_MINOR &&
           RunningEngineApi == HAPI_VERSION_HOUDINI_ENGINE_API )
        {
            HAPI_CookOptions CookOptions;
            FMemory::Memzero< HAPI_CookOptions >( CookOptions );
            CookOptions.curveRefineLOD = 8.0f;
            CookOptions.clearErrorsAndWarnings = false;
            CookOptions.maxVerticesPerPrimitive = 3;
            CookOptions.splitGeosByGroup = false;
            CookOptions.refineCurveToLinear = true;
            CookOptions.handleBoxPartTypes = false;
            CookOptions.handleSpherePartTypes = false;
            CookOptions.splitPointsByVertexAttributes = false;
            CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;

            HAPI_Result Result = FHoudiniApi::Initialize( SessionPtr, &CookOptions, true,
                HoudiniRuntimeSettings->CookingThreadStackSize, 
                TCHAR_TO_UTF8( *HoudiniRuntimeSettings->HoudiniEnvironmentFiles),
                TCHAR_TO_UTF8( *HoudiniRuntimeSettings->OtlSearchPath), 
                TCHAR_TO_UTF8( *HoudiniRuntimeSettings->DsoSearchPath),
                TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ImageDsoSearchPath), 
                TCHAR_TO_UTF8( *HoudiniRuntimeSettings->AudioDsoSearchPath) );
            if ( Result == HAPI_RESULT_SUCCESS )
            {
                HOUDINI_LOG_MESSAGE( TEXT( "Successfully intialized the Houdini Engine API module." ) );
                FHoudiniApi::SetServerEnvString( SessionPtr, HAPI_ENV_CLIENT_NAME, HAPI_UNREAL_CLIENT_NAME );
            }
            else
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Starting up the Houdini Engine API module failed: %s" ),
                    *FHoudiniEngineUtils::GetErrorDescription( Result ) );
            }
        }
        else
        {
            bHAPIVersionMismatch = true;

            HOUDINI_LOG_MESSAGE( TEXT( "Starting up the Houdini Engine API module failed: build and running versions do not match." ) );
            HOUDINI_LOG_MESSAGE(
                TEXT( "Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d" ),
                HAPI_VERSION_HOUDINI_ENGINE_MAJOR, HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API,
                RunningEngineMajor, RunningEngineMinor, RunningEngineApi );
        }

        // Create HAPI scheduler and processing thread.
        HoudiniEngineScheduler = new FHoudiniEngineScheduler();
        HoudiniEngineSchedulerThread = FRunnableThread::Create(
            HoudiniEngineScheduler, TEXT( "HoudiniTaskCookAsset" ), 0, TPri_Normal );

        // Set the default value for pausing houdini engine cooking
        EnableCookingGlobal = !HoudiniRuntimeSettings->bPauseCookingOnStart;
    }

#endif

    // Store the instance.
    FHoudiniEngine::HoudiniEngineInstance = this;
}

void
FHoudiniEngine::ShutdownModule()
{
    HOUDINI_LOG_MESSAGE( TEXT( "Shutting down the Houdini Engine module." ) );

    // We no longer need Houdini logo static mesh.
    if ( HoudiniLogoStaticMesh.IsValid() )
    {
        HoudiniLogoStaticMesh->RemoveFromRoot();
        HoudiniLogoStaticMesh = nullptr;
    }

    // We no longer need Houdini default material.
    if ( HoudiniDefaultMaterial.IsValid() )
    {
        HoudiniDefaultMaterial->RemoveFromRoot();
        HoudiniDefaultMaterial = nullptr;
    }

    // We no longer need Houdini digital asset used for loading bgeo files.
    if ( HoudiniBgeoAsset.IsValid() )
    {
        HoudiniBgeoAsset->RemoveFromRoot();
        HoudiniBgeoAsset = nullptr;
    }

#if WITH_EDITOR
    // Unregister settings.
    ISettingsModule * SettingsModule = FModuleManager::GetModulePtr< ISettingsModule >( "Settings" );
    if ( SettingsModule )
        SettingsModule->UnregisterSettings( "Project", "Plugins", "HoudiniEngine" );
#endif

    // Do scheduler and thread clean up.
    if ( HoudiniEngineScheduler )
        HoudiniEngineScheduler->Stop();

    if ( HoudiniEngineSchedulerThread )
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

    // Perform HAPI finalization.
    if ( FHoudiniApi::IsHAPIInitialized() )
    {
        FHoudiniApi::Cleanup( GetSession() );
        FHoudiniApi::CloseSession( GetSession() );
    }

    FHoudiniApi::FinalizeHAPI();
}

void
FHoudiniEngine::AddTask( const FHoudiniEngineTask & Task )
{
    if ( HoudiniEngineScheduler )
        HoudiniEngineScheduler->AddTask( Task );

    FScopeLock ScopeLock( &CriticalSection );
    FHoudiniEngineTaskInfo TaskInfo;
    TaskInfos.Add( Task.HapiGUID, TaskInfo );
}

void
FHoudiniEngine::AddTaskInfo( const FGuid HapIGUID, const FHoudiniEngineTaskInfo & TaskInfo )
{
    FScopeLock ScopeLock( &CriticalSection );
    TaskInfos.Add( HapIGUID, TaskInfo );
}

void
FHoudiniEngine::RemoveTaskInfo( const FGuid HapIGUID )
{
    FScopeLock ScopeLock( &CriticalSection );
    TaskInfos.Remove( HapIGUID );
}

bool
FHoudiniEngine::RetrieveTaskInfo( const FGuid HapIGUID, FHoudiniEngineTaskInfo & TaskInfo )
{
    FScopeLock ScopeLock( &CriticalSection );

    if ( TaskInfos.Contains( HapIGUID ) )
    {
        TaskInfo = TaskInfos[ HapIGUID ];
        return true;
    }

    return false;
}

bool 
FHoudiniEngine::CookNode(
    HAPI_NodeId AssetId, FHoudiniCookParams& HoudiniCookParams,
    bool ForceRebuildStaticMesh, bool ForceRecookAll,
    const TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesIn,
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesOut,
    TMap< FHoudiniGeoPartObject, ALandscape * >& LandscapesIn,
    TMap< FHoudiniGeoPartObject, ALandscape * >& LandscapesOut,
    TMap< FHoudiniGeoPartObject, USceneComponent * >& InstancersIn,
    TMap< FHoudiniGeoPartObject, USceneComponent * >& InstancersOut,
    USceneComponent* ParentComponent, FTransform & ComponentTransform )
{
    // 
    TMap< FHoudiniGeoPartObject, UStaticMesh * > CookResultArray;
    bool bReturn = FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
        AssetId, HoudiniCookParams, ForceRebuildStaticMesh, 
        ForceRecookAll, StaticMeshesIn, CookResultArray, ComponentTransform );

    if ( !bReturn )
        return false;

    // Extract the static mesh and the volumes/heightfields from the CookResultArray
    TArray< FHoudiniGeoPartObject > FoundVolumes;
    TArray< FHoudiniGeoPartObject > FoundInstancers;
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( CookResultArray ); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();

        UStaticMesh * StaticMesh = Iter.Value();
        if ( HoudiniGeoPartObject.IsInstancer() )
            FoundInstancers.Add( HoudiniGeoPartObject );
        else if (HoudiniGeoPartObject.IsPackedPrimitiveInstancer())
            FoundInstancers.Add( HoudiniGeoPartObject );
        else if (HoudiniGeoPartObject.IsCurve())
            continue;
        else if (HoudiniGeoPartObject.IsVolume())
            FoundVolumes.Add( HoudiniGeoPartObject );
        else
            StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
    }
#if WITH_EDITOR
    // The meshes are already created but we need to create the landscape too
    if ( FoundVolumes.Num() > 0 )
    {
        if ( !FHoudiniLandscapeUtils::CreateAllLandscapes( HoudiniCookParams, FoundVolumes, LandscapesIn, LandscapesOut, -200.0f, 200.0f ) )
            HOUDINI_LOG_WARNING( TEXT("FHoudiniEngine::CookNode : Failed to create landscapes!") );
    }
#endif

    // And the instancers
    if ( FoundInstancers.Num() > 0 )
    {
        if ( !FHoudiniEngineInstancerUtils::CreateAllInstancers(
            HoudiniCookParams, AssetId, FoundInstancers,
            StaticMeshesOut, ParentComponent,
            InstancersIn, InstancersOut ) )
        {
            HOUDINI_LOG_WARNING( TEXT( "FHoudiniEngine::CookNode : Failed to create instancers!" ) );
        }
    }

    if ( StaticMeshesOut.Num() <= 0 && LandscapesOut.Num() <= 0 && InstancersOut.Num() <= 0 )
        return false;

    return true;
}

void 
FHoudiniEngine::SetEnableCookingGlobal(const bool& enableCooking)
{
    EnableCookingGlobal = enableCooking;
}

bool
FHoudiniEngine::GetEnableCookingGlobal()
{
    return EnableCookingGlobal;
}


bool
FHoudiniEngine::StartSession( HAPI_Session*& SessionPtr )
{
    // HAPI needs to be initialized
    if ( !FHoudiniApi::IsHAPIInitialized() )
        return false;

    // Only start a new Session if we dont have a valid one
    if ( HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid( SessionPtr ) )
        return true;

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    HAPI_Result SessionResult = HAPI_RESULT_FAILURE;

    HAPI_ThriftServerOptions ServerOptions;
    FMemory::Memzero< HAPI_ThriftServerOptions >( ServerOptions );
    ServerOptions.autoClose = true;
    ServerOptions.timeoutMs = HoudiniRuntimeSettings->AutomaticServerTimeout;

    auto UpdatePathForServer = [&]
    {
        // Modify our PATH so that HARC will find HARS.exe
        const TCHAR* PathDelimiter = FPlatformMisc::GetPathVarDelimiter();
        const int32 MaxPathVarLen = 32768;
        TCHAR OrigPathVarMem[ MaxPathVarLen ];
        FPlatformMisc::GetEnvironmentVariable( TEXT( "PATH" ), OrigPathVarMem, MaxPathVarLen );
        FString OrigPathVar( OrigPathVarMem );

        FString ModifiedPath =
#if PLATFORM_MAC
        // On Mac our binaries are split between two folders
        LibHAPILocation + TEXT( "/../Resources/bin" ) + PathDelimiter +
#endif
        LibHAPILocation + PathDelimiter + OrigPathVar;

        FPlatformMisc::SetEnvironmentVar( TEXT( "PATH" ), *ModifiedPath );
    };

    switch ( HoudiniRuntimeSettings->SessionType.GetValue() )
    {
        case EHoudiniRuntimeSettingsSessionType::HRSST_InProcess:
        {
            // As of Unreal 4.19, InProcess sessions are not supported anymore
            // Create an auto started pipe session instead using default values
            UpdatePathForServer();
            FHoudiniApi::StartThriftNamedPipeServer(
                &ServerOptions, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ), nullptr );

            SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
                SessionPtr, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ) );
        }
        break;

        case EHoudiniRuntimeSettingsSessionType::HRSST_Socket:
        {
            if ( HoudiniRuntimeSettings->bStartAutomaticServer )
            {
                UpdatePathForServer();
                FHoudiniApi::StartThriftSocketServer(
                    &ServerOptions, HoudiniRuntimeSettings->ServerPort, nullptr );
            }

            SessionResult = FHoudiniApi::CreateThriftSocketSession(
                SessionPtr, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerHost ), HoudiniRuntimeSettings->ServerPort );
        }
        break;

        case EHoudiniRuntimeSettingsSessionType::HRSST_NamedPipe:
        {
            if ( HoudiniRuntimeSettings->bStartAutomaticServer )
            {
                UpdatePathForServer();
                FHoudiniApi::StartThriftNamedPipeServer(
                    &ServerOptions, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ), nullptr );
            }

            SessionResult = FHoudiniApi::CreateThriftNamedPipeSession(
                SessionPtr, TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ServerPipeName ) );
        }
        break;

        default:
            HOUDINI_LOG_ERROR( TEXT( "Unsupported Houdini Engine session type" ) );
            break;
    }

    if ( SessionResult != HAPI_RESULT_SUCCESS || !SessionPtr )
    {
        if ( !HoudiniRuntimeSettings->bStartAutomaticServer )
        {
            HOUDINI_LOG_ERROR( TEXT( "Failed to create a Houdini Engine session.  Check that a Houdini Engine Debugger session or HARS server is running" ) );
        }
        else
        {
            HOUDINI_LOG_ERROR( TEXT( "Failed to create a Houdini Engine session" ) );
        }

        return false;
    }

    // Now, initialize HAPI with the new session
    // We need to make sure HAPI version is correct.
    int32 RunningEngineMajor = 0;
    int32 RunningEngineMinor = 0;
    int32 RunningEngineApi = 0;

    // Retrieve version numbers for running Houdini Engine.
    FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor );
    FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor );
    FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi );

    // Compare defined and running versions.
    if ( RunningEngineMajor != HAPI_VERSION_HOUDINI_ENGINE_MAJOR
        || RunningEngineMinor != HAPI_VERSION_HOUDINI_ENGINE_MINOR
        || RunningEngineApi != HAPI_VERSION_HOUDINI_ENGINE_API )
    {
        bHAPIVersionMismatch = true;

        HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: build and running versions do not match."));
        HOUDINI_LOG_MESSAGE(
            TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d"),
            HAPI_VERSION_HOUDINI_ENGINE_MAJOR, HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API,
            RunningEngineMajor, RunningEngineMinor, RunningEngineApi );

        return false;
    }

    // Default CookOptions
    HAPI_CookOptions CookOptions;
    FMemory::Memzero< HAPI_CookOptions >( CookOptions );
    CookOptions.curveRefineLOD = 8.0f;
    CookOptions.clearErrorsAndWarnings = false;
    CookOptions.maxVerticesPerPrimitive = 3;
    CookOptions.splitGeosByGroup = false;
    CookOptions.refineCurveToLinear = true;
    CookOptions.handleBoxPartTypes = false;
    CookOptions.handleSpherePartTypes = false;
    CookOptions.splitPointsByVertexAttributes = false;
    CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;

    HAPI_Result Result = FHoudiniApi::Initialize( SessionPtr, &CookOptions, true,
        HoudiniRuntimeSettings->CookingThreadStackSize, 
        TCHAR_TO_UTF8( *HoudiniRuntimeSettings->HoudiniEnvironmentFiles),
        TCHAR_TO_UTF8( *HoudiniRuntimeSettings->OtlSearchPath), 
        TCHAR_TO_UTF8( *HoudiniRuntimeSettings->DsoSearchPath),
        TCHAR_TO_UTF8( *HoudiniRuntimeSettings->ImageDsoSearchPath), 
        TCHAR_TO_UTF8( *HoudiniRuntimeSettings->AudioDsoSearchPath) );

    if ( Result != HAPI_RESULT_SUCCESS )
    {
        HOUDINI_LOG_MESSAGE(
            TEXT("Starting up the Houdini Engine API module failed: %s"),
            *FHoudiniEngineUtils::GetErrorDescription( Result ) );

        return false;
    }

    HOUDINI_LOG_MESSAGE( TEXT( "Successfully intialized the Houdini Engine API module." ) );
    FHoudiniApi::SetServerEnvString( SessionPtr, HAPI_ENV_CLIENT_NAME, HAPI_UNREAL_CLIENT_NAME );

    return true;
}

bool
FHoudiniEngine::StopSession( HAPI_Session*& SessionPtr )
{
    // HAPI needs to be initialized
    if ( !FHoudiniApi::IsHAPIInitialized() )
        return false;

    if ( HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid( SessionPtr ) )
    {
        // SessionPtr is valid, clean up and close the session
        FHoudiniApi::Cleanup( SessionPtr );
        FHoudiniApi::CloseSession( SessionPtr );
    }

    return true;
}

bool
FHoudiniEngine::RestartSession()
{
    HAPI_Session* SessionPtr = &Session;
    if ( !StopSession( SessionPtr ) )
        return false;

    if ( !StartSession( SessionPtr ) )
        return false;

    return true;
}

#undef LOCTEXT_NAMESPACE