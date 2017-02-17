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
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniApi.h"
#include "HoudiniEngineScheduler.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"

const uint32
FHoudiniEngineScheduler::InitialTaskSize = 256u;

FHoudiniEngineScheduler::FHoudiniEngineScheduler()
    : Tasks( nullptr )
    , PositionWrite( 0u )
    , PositionRead( 0u )
    , bStopping( false )
{
    //  Make sure size is power of two.
    TaskCount = FPlatformMath::RoundUpToPowerOfTwo( FHoudiniEngineScheduler::InitialTaskSize );

    if ( TaskCount )
    {
        // Allocate buffer to store all tasks.
        Tasks = static_cast< FHoudiniEngineTask * >( FMemory::Malloc( TaskCount * sizeof( FHoudiniEngineTask ) ) );

        if ( Tasks )
        {
            // Zero memory.
            FMemory::Memset( Tasks, 0x0, TaskCount * sizeof( FHoudiniEngineTask ) );
        }
    }
}

FHoudiniEngineScheduler::~FHoudiniEngineScheduler()
{
    if ( TaskCount )
    {
        FMemory::Free( Tasks );
        Tasks = nullptr;
    }
}

void
FHoudiniEngineScheduler::TaskDescription(
    FHoudiniEngineTaskInfo & TaskInfo,
    const FString & ActorName,
    const FString & StatusString )
{
    FFormatNamedArguments Args;

    if ( !ActorName.IsEmpty() )
    {
        Args.Add( TEXT( "AssetName" ), FText::FromString( ActorName ) );
        Args.Add( TEXT( "AssetStatus" ), FText::FromString( StatusString ) );
        TaskInfo.StatusText =
            FText::Format( NSLOCTEXT( "TaskDescription", "TaskDescriptionProgress", "({AssetName}) : ({AssetStatus})"), Args );
    }
    else
    {
        Args.Add( TEXT( "AssetStatus" ), FText::FromString( StatusString ) );
        TaskInfo.StatusText =
            FText::Format( NSLOCTEXT( "TaskDescription", "TaskDescriptionProgress", "({AssetStatus})"), Args );
    }
}

void
FHoudiniEngineScheduler::TaskInstantiateAsset( const FHoudiniEngineTask & Task )
{
    FString AssetN;
    FHoudiniEngineString( Task.AssetHapiName ).ToFString( AssetN );

    HOUDINI_LOG_MESSAGE(
        TEXT( "HAPI Asynchronous Instantiation Started for %s: Asset=%s, HoudiniAsset = 0x%x" ),
        *Task.ActorName, *AssetN, Task.Asset.Get() );

    if ( !FHoudiniEngineUtils::IsInitialized() )
    {
        HOUDINI_LOG_ERROR(
            TEXT( "TaskInstantiateAsset failed for %s: %s" ),
            *Task.ActorName,
            *FHoudiniEngineUtils::GetErrorDescription( HAPI_RESULT_NOT_INITIALIZED ) );

        AddResponseMessageTaskInfo(
            HAPI_RESULT_NOT_INITIALIZED, EHoudiniEngineTaskType::AssetInstantiation,
            EHoudiniEngineTaskState::FinishedInstantiationWithErrors,
            -1, Task, TEXT( "HAPI is not initialized." ) );

        return;
    }

    if ( !Task.Asset.IsValid() )
    {
        // Asset is no longer valid, return.
        AddResponseMessageTaskInfo(
            HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetInstantiation,
            EHoudiniEngineTaskState::FinishedInstantiationWithErrors,
            -1, Task, TEXT( "Asset is no longer valid." ) );

        return;
    }

    if ( Task.AssetHapiName < 0 )
    {
        // Asset is no longer valid, return.
        AddResponseMessageTaskInfo(
            HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetInstantiation,
            EHoudiniEngineTaskState::FinishedInstantiationWithErrors,
            -1, Task, TEXT( "Asset name is invalid." ) );

        return;
    }

    HAPI_Result Result = HAPI_RESULT_SUCCESS;
    UHoudiniAsset* HoudiniAsset = Task.Asset.Get();
    int32 AssetCount = 0;
    HAPI_NodeId AssetId = -1;
    std::string AssetNameString;
    double LastUpdateTime;

    FHoudiniEngineString HoudiniEngineString( Task.AssetHapiName );
    if ( HoudiniEngineString.ToStdString( AssetNameString ) )
    {
        // Translate asset name into Unreal string.
        FString AssetName = ANSI_TO_TCHAR( AssetNameString.c_str() );

        // Initialize last update time.
        LastUpdateTime = FPlatformTime::Seconds();

        // We instantiate without cooking.
        Result = FHoudiniApi::CreateNode(
            FHoudiniEngine::Get().GetSession(), -1, &AssetNameString[ 0 ], nullptr, false, &AssetId );
        if ( Result != HAPI_RESULT_SUCCESS )
        {
            AddResponseMessageTaskInfo(
                Result, EHoudiniEngineTaskType::AssetInstantiation,
                EHoudiniEngineTaskState::FinishedInstantiationWithErrors,
                -1, Task, TEXT( "Error instantiating asset." ) );

            return;
        }

        // Add processing notification.
        FHoudiniEngineTaskInfo TaskInfo(
            HAPI_RESULT_SUCCESS, -1, EHoudiniEngineTaskType::AssetInstantiation,
            EHoudiniEngineTaskState::Processing );

        TaskInfo.bLoadedComponent = Task.bLoadedComponent;
        TaskDescription( TaskInfo, Task.ActorName, TEXT( "Started Instantiation" ) );
        FHoudiniEngine::Get().AddTaskInfo( Task.HapiGUID, TaskInfo );

        // We need to spin until instantiation is finished.
        while( true )
        {
            int Status = HAPI_STATE_STARTING_COOK;
            HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetStatus(
                FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status ) );

            if ( Status == HAPI_STATE_READY )
            {
                // Cooking has been successful.
                AddResponseMessageTaskInfo(
                    HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetInstantiation,
                    EHoudiniEngineTaskState::FinishedInstantiation, AssetId, Task,
                    TEXT( "Finished Instantiation." ) );

                break;
            }
            else if ( Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS )
            {
                // There was an error while instantiating.
                FString CookResultString = FHoudiniEngineUtils::GetCookResult();
                int32 CookResult = static_cast<int32>(HAPI_RESULT_SUCCESS);
                FHoudiniApi::GetStatus( FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_RESULT, &CookResult );

                AddResponseMessageTaskInfo(
                    static_cast<HAPI_Result>(CookResult), EHoudiniEngineTaskType::AssetInstantiation,
                    EHoudiniEngineTaskState::FinishedInstantiationWithErrors, AssetId, Task,
                    FString::Printf(TEXT( "Finished Instantiation with Errors: %s" ), *CookResultString ));

                break;
            }

            static const double NotificationUpdateFrequency = 0.5;
            if ( ( FPlatformTime::Seconds() - LastUpdateTime ) >= NotificationUpdateFrequency )
            {
                // Reset update time.
                LastUpdateTime = FPlatformTime::Seconds();

                const FString& CookStateMessage = FHoudiniEngineUtils::GetCookState();

                AddResponseMessageTaskInfo(
                    HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetInstantiation,
                    EHoudiniEngineTaskState::Processing, AssetId, Task,
                    CookStateMessage );
            }

            // We want to yield.
            FPlatformProcess::Sleep( 0.0f );
        }
    }
    else
    {
        AddResponseMessageTaskInfo(
            HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetInstantiation,
            EHoudiniEngineTaskState::FinishedInstantiationWithErrors,
            -1, Task, TEXT( "Error retrieving asset name." ) );

        return;
    }
}

void
FHoudiniEngineScheduler::TaskCookAsset( const FHoudiniEngineTask & Task )
{
    if ( !FHoudiniEngineUtils::IsInitialized() )
    {
        HOUDINI_LOG_ERROR(
            TEXT( "TaskCookAsset failed for %s: %s"),
            *Task.ActorName,
            *FHoudiniEngineUtils::GetErrorDescription( HAPI_RESULT_NOT_INITIALIZED ) );

        AddResponseMessageTaskInfo(
            HAPI_RESULT_NOT_INITIALIZED, EHoudiniEngineTaskType::AssetCooking,
            EHoudiniEngineTaskState::FinishedCookingWithErrors,
            -1, Task, TEXT( "HAPI is not initialized." ) );

        return;
    }

    if ( !Task.AssetComponent.IsValid() )
    {
        // Asset component is no longer valid, return.
        AddResponseMessageTaskInfo(
            HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetCooking,
            EHoudiniEngineTaskState::FinishedCookingWithErrors,
            -1, Task, TEXT( "Asset is no longer valid." ) );

        return;
    }

    // Retrieve asset id.
    HAPI_NodeId AssetId = Task.AssetComponent->GetAssetId();
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    HOUDINI_LOG_MESSAGE(
        TEXT( "HAPI Asynchronous Cooking Started for %s. Component = 0x%x, AssetId = %d" ),
        *Task.ActorName, Task.AssetComponent.Get(), AssetId );

    if ( AssetId == -1 )
    {
        // We have an invalid asset id.
        HOUDINI_LOG_ERROR( TEXT( "TaskCookAsset failed for %s: Invalid Asset Id." ), *Task.ActorName );

        AddResponseMessageTaskInfo(
            HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetCooking,
            EHoudiniEngineTaskState::FinishedCookingWithErrors,
            -1, Task, TEXT( "Asset has invalid id." ) );

        return;
    }

    Result = FHoudiniApi::CookNode( FHoudiniEngine::Get().GetSession(), AssetId, nullptr );
    if ( Result != HAPI_RESULT_SUCCESS )
    {
        AddResponseMessageTaskInfo(
            Result, EHoudiniEngineTaskType::AssetCooking,
            EHoudiniEngineTaskState::FinishedCookingWithErrors,
            AssetId, Task, TEXT( "Error cooking asset." ) );

        return;
    }

    // Add processing notification.
    AddResponseMessageTaskInfo(
        HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
        EHoudiniEngineTaskState::Processing, AssetId, Task, TEXT( "Started Cooking" ) );

    // Initialize last update time.
    double LastUpdateTime = FPlatformTime::Seconds();

    // We need to spin until cooking is finished.
    while ( true )
    {
        int32 Status = HAPI_STATE_STARTING_COOK;
        HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetStatus(
            FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status ) );

        if ( !Task.AssetComponent.IsValid() )
        {
            AddResponseMessageTaskInfo(
                HAPI_RESULT_FAILURE, EHoudiniEngineTaskType::AssetCooking,
                EHoudiniEngineTaskState::FinishedCookingWithErrors, AssetId, Task,
                TEXT( "Component is no longer valid." ) );

            break;
        }

        if ( Status == HAPI_STATE_READY )
        {
            // Cooking has been successful.
            AddResponseMessageTaskInfo(
                HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
                EHoudiniEngineTaskState::FinishedCooking, AssetId, Task,
                TEXT( "Finished Cooking" ) );

            break;
        }
        else if ( Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS )
        {
            // There was an error while instantiating.
            AddResponseMessageTaskInfo(
                HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
                EHoudiniEngineTaskState::FinishedCookingWithErrors, AssetId, Task,
                TEXT( "Finished Cooking with Errors" ) );

            break;
        }

        static const double NotificationUpdateFrequency = 0.5;
        if ( FPlatformTime::Seconds() - LastUpdateTime >= NotificationUpdateFrequency )
        {
            // Reset update time.
            LastUpdateTime = FPlatformTime::Seconds();

            // Retrieve status string.
            const FString & CookStateMessage = FHoudiniEngineUtils::GetCookState();

            AddResponseMessageTaskInfo(
                HAPI_RESULT_SUCCESS, EHoudiniEngineTaskType::AssetCooking,
                EHoudiniEngineTaskState::Processing, AssetId, Task,
                CookStateMessage );
        }

        // We want to yield.
        FPlatformProcess::Sleep( 0.0f );
    }
}

void
FHoudiniEngineScheduler::TaskDeleteAsset( const FHoudiniEngineTask & Task )
{
    HOUDINI_LOG_MESSAGE(
        TEXT( "HAPI Asynchronous Destruction Started for %s. Component = 0x%x" )
        TEXT( "AssetId = %d" ),
        *Task.ActorName, Task.AssetComponent.Get(), Task.AssetId );

    if ( FHoudiniEngineUtils::IsHoudiniAssetValid( Task.AssetId ) )
        FHoudiniEngineUtils::DestroyHoudiniAsset( Task.AssetId );

    // We do not insert task info as this is a fire and forget operation.
    // At this point component most likely does not exist.
}

void
FHoudiniEngineScheduler::AddResponseTaskInfo(
    HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType, EHoudiniEngineTaskState::Type TaskState,
    HAPI_NodeId AssetId, const FHoudiniEngineTask & Task )
{
    FHoudiniEngineTaskInfo TaskInfo( Result, AssetId, TaskType, TaskState );
    FString StatusString = FHoudiniEngineUtils::GetErrorDescription();

    TaskInfo.bLoadedComponent = Task.bLoadedComponent;
    TaskDescription( TaskInfo, Task.ActorName, StatusString );
    FHoudiniEngine::Get().AddTaskInfo( Task.HapiGUID, TaskInfo );
}

void
FHoudiniEngineScheduler::AddResponseMessageTaskInfo(
    HAPI_Result Result, EHoudiniEngineTaskType::Type TaskType, EHoudiniEngineTaskState::Type TaskState,
    HAPI_NodeId AssetId, const FHoudiniEngineTask & Task, const FString & ErrorMessage )
{
    FHoudiniEngineTaskInfo TaskInfo( Result, AssetId, TaskType, TaskState );

    TaskInfo.bLoadedComponent = Task.bLoadedComponent;
    TaskDescription( TaskInfo, Task.ActorName, ErrorMessage );
    FHoudiniEngine::Get().AddTaskInfo( Task.HapiGUID, TaskInfo );
}

void
FHoudiniEngineScheduler::ProcessQueuedTasks()
{
    while( !bStopping )
    {
        while ( true )
        {
            FHoudiniEngineTask Task;

            {
                FScopeLock ScopeLock( &CriticalSection );

                // We have no tasks left.
                if ( PositionWrite == PositionRead )
                    break;

                // Retrieve task.
                Task = Tasks[ PositionRead ];
                PositionRead++;

                // Wrap around if required.
                PositionRead &= ( TaskCount - 1 );
            }

            bool bTaskProcessed = true;

            switch ( Task.TaskType )
            {
                case EHoudiniEngineTaskType::AssetInstantiation:
                {
                    TaskInstantiateAsset( Task );
                    break;
                }

                case EHoudiniEngineTaskType::AssetCooking:
                {
                    TaskCookAsset( Task );
                    break;
                }

                case EHoudiniEngineTaskType::AssetDeletion:
                {
                    TaskDeleteAsset( Task );
                    break;
                }

                default:
                {
                    bTaskProcessed = false;
                    break;
                }
            }

            if ( !bTaskProcessed )
                break;
        }

        if ( FPlatformProcess::SupportsMultithreading() )
        {
            // We want to yield for a bit.
            FPlatformProcess::Sleep( 0.0f );
        }
        else
        {
            // If we are running in single threaded mode, return so we don't block everything else.
            return;
        }
    }
}

void
FHoudiniEngineScheduler::AddTask( const FHoudiniEngineTask & Task )
{
    FScopeLock ScopeLock( &CriticalSection );

    // Check if we need to grow our circular buffer.
    if ( PositionWrite + 1 == PositionRead )
    {
        // Calculate next size (next power of two).
        uint32 NextTaskCount = FPlatformMath::RoundUpToPowerOfTwo( TaskCount + 1 );

        // Allocate new buffer.
        FHoudiniEngineTask * Buffer = static_cast< FHoudiniEngineTask * >(
            FMemory::Malloc( NextTaskCount * sizeof( FHoudiniEngineTask ) ) );

        if( !Buffer )
            return;

        // Zero memory.
        FMemory::Memset( Buffer, 0x0, NextTaskCount * sizeof( FHoudiniEngineTask ) );

        // Copy elements from old buffer to new one.
        if ( PositionRead < PositionWrite )
        {
            FMemory::Memcpy( Buffer, Tasks + PositionRead, sizeof( FHoudiniEngineTask ) * ( PositionWrite - PositionRead ) );

            // Update index positions.
            PositionRead = 0;
            PositionWrite = PositionWrite - PositionRead;
        }
        else
        {
            FMemory::Memcpy( Buffer, Tasks + PositionRead, sizeof( FHoudiniEngineTask ) * ( TaskCount - PositionRead ) );
            FMemory::Memcpy( Buffer + TaskCount - PositionRead, Tasks, sizeof( FHoudiniEngineTask ) * PositionWrite );

            // Update index positions.
            PositionRead = 0;
            PositionWrite = TaskCount - PositionRead + PositionWrite;
        }

        // Deallocate old buffer.
        FMemory::Free( Tasks );

        // Bookkeeping.
        Tasks = Buffer;
        TaskCount = NextTaskCount;
    }

    // Store task.
    Tasks[ PositionWrite ] = Task;
    PositionWrite++;

    // Wrap around if required.
    PositionWrite &= ( TaskCount - 1 );
}

uint32
FHoudiniEngineScheduler::Run()
{
    ProcessQueuedTasks();
    return 0;
}

void
FHoudiniEngineScheduler::Stop()
{
    bStopping = true;
}

void
FHoudiniEngineScheduler::Tick()
{
    ProcessQueuedTasks();
}

FSingleThreadRunnable *
FHoudiniEngineScheduler::GetSingleThreadInterface()
{
    return this;
}
