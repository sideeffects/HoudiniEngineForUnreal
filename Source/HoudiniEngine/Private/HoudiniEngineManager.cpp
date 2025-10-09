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

#include "HoudiniEngineManager.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetBlueprintComponent.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniParameterTranslator.h"
#include "HoudiniPDGManager.h"
#include "HoudiniInputTranslator.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniHandleTranslator.h"
#include "HoudiniLandscapeRuntimeUtils.h"

#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
	#include "LevelInstance/LevelInstanceInterface.h"
#endif

#if WITH_EDITOR
	#include "Editor.h"
	#include "EditorViewportClient.h"
	#include "Kismet/KismetMathLibrary.h"

	//#include "UnrealEd.h"
	#include "UnrealEdGlobals.h"
	#include "Editor/UnrealEdEngine.h"
	#include "IPackageAutoSaver.h"
#endif

static TAutoConsoleVariable<float> CVarHoudiniEngineTickTimeLimit(
	TEXT("HoudiniEngine.TickTimeLimit"),
	1.0,
	TEXT("Time limit after which HDA processing will be stopped, until the next tick of the Houdini Engine Manager.\n")
	TEXT("<= 0.0: No Limit\n")
	TEXT("1.0: Default\n")
);

static TAutoConsoleVariable<float> CVarHoudiniEngineLiveSyncTickTime(
	TEXT("HoudiniEngine.LiveSyncTickTime"),
	1.0,
	TEXT("Frequency at which to look for update when using Session Sync.\n")
	TEXT("<= 0.0: No Limit\n")
	TEXT("1.0: Default\n")
);

FHoudiniEngineManager::FHoudiniEngineManager()
	: CurrentIndex(0)
	, ComponentCount(0)
	, bMustStopTicking(false)
	, SyncedHoudiniViewportPivotPosition(FVector::ZeroVector)
	, SyncedHoudiniViewportQuat(FQuat::Identity)
	, SyncedHoudiniViewportOffset(0.0f)
	, SyncedUnrealViewportPosition(FVector::ZeroVector)
	, SyncedUnrealViewportRotation(FRotator::ZeroRotator)
	, SyncedUnrealViewportLookatPosition(FVector::ZeroVector)
	, ZeroOffsetValue(0.f)
	, bOffsetZeroed(false)
{

}

FHoudiniEngineManager::~FHoudiniEngineManager()
{
	PDGManager.StopBGEOCommandletAndEndpoint();
}

void 
FHoudiniEngineManager::StartHoudiniTicking()
{
	// If we have no timer delegate spawned, spawn one.
	if (!TickerHandle.IsValid() && GEditor)
	{
		// We use the ticker manager so we get ticked once per frame, no more.
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FHoudiniEngineManager::Tick));		

		// Grab current time for delayed notification.
		FHoudiniEngine::Get().SetHapiNotificationStartedTime(FPlatformTime::Seconds());
	}
}

void 
FHoudiniEngineManager::StopHoudiniTicking()
{
	if (TickerHandle.IsValid() && GEditor)
	{
		if (IsInGameThread())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();

			// Reset time for delayed notification.
			FHoudiniEngine::Get().SetHapiNotificationStartedTime(0.0);

			bMustStopTicking = false;
		}
		else
		{
			// We can't stop ticking now as we're not in the game Thread,
			// and accessing the timer would crash, indicate that we want to stop ticking asap
			// This can happen when loosing a session due to a Houdini crash
			bMustStopTicking = true;
		}
	}
}

bool FHoudiniEngineManager::IsTicking() const
{
	return TickerHandle.IsValid();
}

bool
FHoudiniEngineManager::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::Tick);

	EnableEditorAutoSave(nullptr);

	FHoudiniEngine::Get().TickCookingNotification(DeltaTime);

	if (bMustStopTicking)
	{
		// Ticking should be stopped immediately
		StopHoudiniTicking();
		return true;
	}

	// COOKABLE LOOP

	// Build a set of cookables that need to be processed
	// 1 - selected Cookables with Components
	// 2 - "Active" Cookables
	// 3 - The "next" inactive Cookable
	TArray<UHoudiniCookable*> CookablesToProcess;
	if (FHoudiniEngineRuntime::IsInitialized())
	{
		FHoudiniEngineRuntime::Get().CleanUpRegisteredHoudiniCookables();
		CookableCount = FHoudiniEngineRuntime::Get().GetRegisteredHoudiniCookableCount();

		// Wrap around if needed
		if (CurrentCookableIndex >= CookableCount)
			CurrentCookableIndex = 0;

		for (uint32 nIdx = 0; nIdx < CookableCount; nIdx++)
		{
			UHoudiniCookable* CurrentCookable = FHoudiniEngineRuntime::Get().GetRegisteredHoudiniCookableAt(nIdx);
			if (!CurrentCookable || !CurrentCookable->IsValidLowLevelFast())
			{
				// Invalid cookable, do not process
				continue;
			}
			else if (!IsValid(CurrentCookable) || CurrentCookable->GetCurrentState() == EHoudiniAssetState::Deleting)
			{
				// cookable being deleted, do not process
				continue;
			}

			{
				UWorld* World = CurrentCookable->GetWorld();
				if (World && (World->IsPlayingReplay() || World->IsPlayInEditor()))
				{
					UCookableProxyData* ProxyData = CurrentCookable->GetProxyData();
					if (ProxyData && !ProxyData->bAllowPlayInEditorRefinement)
					{
						// This cookable's component's world is current in PIE and this HDA is NOT allowed to cook / refine in PIE.
						continue;
					}
				}
			}

			// TODO COOKABLE ??
			if (!CurrentCookable->bFullyLoaded)
			{
				/*
				// TODO COOKABLE: BP Support
				// Let the component figure out whether it's fully loaded or not.
				CurrentCookable->HoudiniEngineTick();
				if (!CurrentCookable->IsFullyLoaded())
					continue; // We need to wait some more.
				*/

				// For non BP case - just set fully loaded
				CurrentCookable->bFullyLoaded = true;
			}

			/*
			* // TODO COOKABLE: BP Support
			if (!CurrentCookable->IsFullyLoaded())
			{
				// Let the component figure out whether it's fully loaded or not.
				CurrentCookable->HoudiniEngineTick();
				if (!CurrentCookable->IsFullyLoaded())
					continue; // We need to wait some more.
			}

			if (!CurrentComponent->IsValidComponent())
			{
				// This component is no longer valid. Prevent it from being processed, and remove it.
				FHoudiniEngineRuntime::Get().UnRegisterHoudiniComponent(CurrentComponent);
				continue;
			}
			*/

			AActor* Owner = CurrentCookable->GetOwner();
			if (Owner && Owner->IsSelectedInEditor())
			{
				// 1. Add selected cookables
				CookablesToProcess.Add(CurrentCookable);
			}
			else if (CurrentCookable->GetCurrentState() != EHoudiniAssetState::NeedInstantiation
				&& CurrentCookable->GetCurrentState() != EHoudiniAssetState::None)
			{
				// 2. Add "Active" cookables, the only two non-active states are:
				// NeedInstantiation (loaded, not instantiated in H yet, not modified)
				// None (no processing currently)
				CookablesToProcess.Add(CurrentCookable);
			}
			else if (nIdx == CurrentCookableIndex)
			{
				// 3. Add the "Current" cookable
				CookablesToProcess.Add(CurrentCookable);
			}
			if (CurrentCookable->GetCurrentState() == EHoudiniAssetState::Dormant)
			{
				CurrentCookable->UpdateDormantStatus();
			}
			// Set the LastTickTime on the "current" HAC to 0 to ensure it's treated first
			if (nIdx == CurrentIndex)
			{
				CurrentCookable->LastTickTime = 0.0;
			}
		}

		// Increment the current index for the next tick
		CurrentCookableIndex++;
	}

	// Sort the components by last tick time
	CookablesToProcess.Sort([](const UHoudiniCookable& A, const UHoudiniCookable& B) { return A.LastTickTime < B.LastTickTime; });

	// Time limit for processing
	double dCookableProcessTimeLimit = CVarHoudiniEngineTickTimeLimit.GetValueOnAnyThread();
	double dCookableProcessStartTime = FPlatformTime::Seconds();

	// Process all the cookables in the list
	for (UHoudiniCookable* CurrentCookable : CookablesToProcess)
	{
		double dNow = FPlatformTime::Seconds();
		if (dCookableProcessTimeLimit > 0.0
			&& dNow - dCookableProcessStartTime > dCookableProcessTimeLimit)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Manager: Stopped processing after %f seconds."), (dNow - dCookableProcessStartTime));
			break;
		}

		// Update the tick time for this cookable
		CurrentCookable->LastTickTime = dNow;

		// Handle template processing (for BP) first
		// We don't want to the template component processing to trigger session creation
		if (CurrentCookable->GetCurrentState() == EHoudiniAssetState::ProcessTemplate)
		{
			// TODO COOKABLE: PROCESSBP TEMPLATE
			continue;
		}

		// Process the cookable
		bool bKeepProcessing = true;
		while (bKeepProcessing)
		{
			// See if we should start the default "first" session
			if (CurrentCookable->ShouldTryToStartFirstSession())
			{
				AutoStartFirstSessionIfNeeded();
			}

			EHoudiniAssetState PrevState = CurrentCookable->GetCurrentState();
			ProcessCookable(CurrentCookable);
			EHoudiniAssetState NewState = CurrentCookable->GetCurrentState();

			// In order to process components faster / with less ticks,
			// we may continue processing the component if it ends up in certain states
			switch (NewState)
			{
				case EHoudiniAssetState::NewHDA:
				case EHoudiniAssetState::PreInstantiation:
				case EHoudiniAssetState::PreCook:
				case EHoudiniAssetState::PostCook:
				case EHoudiniAssetState::PreProcess:
				case EHoudiniAssetState::Processing:
					bKeepProcessing = true;
					break;

				case EHoudiniAssetState::NeedInstantiation:
				case EHoudiniAssetState::Instantiating:
				case EHoudiniAssetState::Cooking:
				case EHoudiniAssetState::None:
				case EHoudiniAssetState::ProcessTemplate:
				case EHoudiniAssetState::NeedRebuild:
				case EHoudiniAssetState::NeedDelete:
				case EHoudiniAssetState::Deleting:
				case EHoudiniAssetState::Dormant:
					bKeepProcessing = false;
					break;
			}

			// Stop processing if the state hasn't changed
			// for example, if we're waiting for HDA inputs to finish cooking/instantiating
			if (PrevState == NewState)
				bKeepProcessing = false;

			dNow = FPlatformTime::Seconds();
			if (dCookableProcessTimeLimit > 0.0 && dNow - dCookableProcessStartTime > dCookableProcessTimeLimit)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Manager: Stopped processing after %f seconds."), (dNow - dCookableProcessStartTime));
				break;
			}

			// Update the tick time for this component
			CurrentCookable->LastTickTime = dNow;
		}
#if WITH_EDITORONLY_DATA
		// See if we need to update this HDA's details panel
		if (CurrentCookable->bNeedToUpdateEditorProperties && CurrentCookable->bAllowUpdateEditorProperties)
		{
			// Only do an update if the HAC is selected
			bool bDoUpdateProperties = false;
			AActor* Owner = CurrentCookable->GetOwner();
			if (Owner && Owner->IsSelectedInEditor())
				bDoUpdateProperties = true;
			else if (!CurrentCookable->AssetEditorId.IsNone())
				bDoUpdateProperties = true;

			if (bDoUpdateProperties)
				FHoudiniEngineUtils::UpdateEditorProperties(true);

			CurrentCookable->bNeedToUpdateEditorProperties = false;
		}
#endif
	}

	//
	// Node Deletion
	//

	// Handle node delete
	if (FHoudiniEngineRuntime::IsInitialized())
	{
		int32 PendingDeleteCount = FHoudiniEngineRuntime::Get().GetNodeIdsPendingDeleteCount();
		for (int32 DeleteIdx = PendingDeleteCount - 1; DeleteIdx >= 0; DeleteIdx--)
		{
			HAPI_NodeId NodeIdToDelete = (HAPI_NodeId)FHoudiniEngineRuntime::Get().GetNodeIdsPendingDeleteAt(DeleteIdx);
			FGuid HapiDeletionGUID;
			bool bShouldDeleteParent = FHoudiniEngineRuntime::Get().IsParentNodePendingDelete(NodeIdToDelete);
			if (StartTaskAssetDelete(NodeIdToDelete, HapiDeletionGUID, bShouldDeleteParent))
			{
				FHoudiniEngineRuntime::Get().RemoveNodeIdPendingDeleteAt(DeleteIdx);
				if (bShouldDeleteParent)
					FHoudiniEngineRuntime::Get().RemoveParentNodePendingDelete(NodeIdToDelete);
			}
		}
	}

	// Update PDG Contexts and asset link if needed
	PDGManager.Update();

	// Session Sync Updates
	if (FHoudiniEngine::Get().IsSessionSyncEnabled())
	{
		// See if the session sync settings have changed on the houdini side, update ours if they did
		FHoudiniEngine::Get().UpdateSessionSyncInfoFromHoudini();
#if WITH_EDITOR
		// Update the Houdini viewport from unreal if needed
		if (FHoudiniEngine::Get().IsSyncViewportEnabled())
		{
			// Sync the Houdini viewport to Unreal
			if (!SyncHoudiniViewportToUnreal())
			{
				// If the unreal viewport hasnt changed, 
				// See if we need to sync the Unreal viewport from Houdini's
				SyncUnrealViewportToHoudini();
			}
		}
#endif
	}
	else 
	{
		// reset zero offset variables when session sync is off
		if (ZeroOffsetValue != 0.f) 
			ZeroOffsetValue = 0.f;
		
		if (bOffsetZeroed)
			bOffsetZeroed = false;
	}

	return true;
}

void
FHoudiniEngineManager::AutoStartFirstSessionIfNeeded()
{
	// See if we should start the default "first" session
	if (FHoudiniEngine::Get().GetSession() || FHoudiniEngine::Get().GetFirstSessionCreated())
		return;

	FString StatusText = TEXT("Initializing Houdini Engine...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

	// Indicates that we've tried to start the session once no matter if it failed or succeed
	FHoudiniEngine::Get().SetFirstSessionCreated(true);

	// Attempt to restart the session
	if (!FHoudiniEngine::Get().RestartSession())
	{
		// We failed to start the session
		// Stop ticking until it's manually restarted
		StopHoudiniTicking();

		StatusText = TEXT("Houdini Engine failed to initialize.");
	}
	else
	{
		StatusText = TEXT("Houdini Engine successfully initialized.");
	}

	// Finish the notification and display the results
	FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(StatusText));
}


void
FHoudiniEngineManager::ProcessCookable(UHoudiniCookable* HC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable);

	if (!IsValid(HC))
		return;

	// No need to process an HDA cookable that is not tied to an HDA yet..
	if (HC->IsHoudiniAssetSupported() && !HC->HoudiniAssetData->HoudiniAsset)
		return;

	UHoudiniAssetComponent* MyHAC = HC->IsComponentSupported() ? Cast<UHoudiniAssetComponent>(HC->ComponentData->Component) : nullptr;
	UHoudiniNodeSyncComponent* MyHNSC = HC->IsComponentSupported() ? Cast<UHoudiniNodeSyncComponent>(HC->ComponentData->Component) : nullptr;
	UHoudiniAssetBlueprintComponent* MyHABC = HC->IsComponentSupported() ? Cast<UHoudiniAssetBlueprintComponent>(HC->ComponentData->Component) : nullptr;

	const EHoudiniAssetState CurrentStateToProcess = HC->GetCurrentState();

	// If cooking is paused, stay in the current state until cooking's resumed, unless we are in NewHDA
	if (!FHoudiniEngine::Get().IsCookingEnabled() && CurrentStateToProcess != EHoudiniAssetState::NewHDA)
	{
		// Refresh UI when pause cooking
		if (!FHoudiniEngine::Get().HasUIFinishRefreshingWhenPausingCooking())
		{
#if WITH_EDITORONLY_DATA
			// Trigger a details panel update if the Houdini asset actor is selected
			if (HC->IsOwnerSelected())
				HC->bNeedToUpdateEditorProperties = true;
#endif

			// Finished refreshing UI of one HDA.
			FHoudiniEngine::Get().RefreshUIDisplayedWhenPauseCooking();
		}

		// Prevent any other state change to happen
		return;
	}

	switch (CurrentStateToProcess)
	{
		case EHoudiniAssetState::NeedInstantiation:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - NeedInstantiation);

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
			// If this cookable is part of an uneditable level instance, mark it as dormant.
			auto* LevelInstance = HC->GetLevelInstance();
			if (LevelInstance && !LevelInstance->IsEditing())
			{
				HC->SetCurrentState(EHoudiniAssetState::Dormant);
				break;
			}
#endif

			// Do nothing unless the HAC has been updated
			if (HC->NeedUpdate())
			{
				// Call PrePreInstantation on BP component
				if(MyHABC)
					MyHABC->OnPrePreInstantiation();

				HC->bForceNeedUpdate = false;
				// Update the HAC's state
				HC->SetCurrentState(EHoudiniAssetState::PreInstantiation);
			}

			/*
			// TODO COOKABLE: Might not be needed anymore ?
			else if (HC->NeedUpdateInstancedOutputs())
			{
				// Output updates do not recquire the HDA to be instantiated
				FHoudiniOutputTranslator::UpdateChangedOutputs(MyHAC);
			}
			*/

			// Update world input if we have any
			if(HC->IsInputSupported())
				FHoudiniInputTranslator::UpdateWorldInputs(HC->InputData->Inputs, HC->GetOwner());

			break;
		}

		case EHoudiniAssetState::NewHDA:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - NewHDA);

			// Update parameters. Since there is no instantiated node yet, this will only fetch the defaults from
			// the asset definition.
			// 
			// TODO COOKABLE: this only works if we have both an asset AND parameters!
			// 
			if (HC->IsParameterSupported())
			{
				//FHoudiniParameterTranslator::UpdateParameters(HC);
				const bool bForceFullUpdate = HC->HasRebuildBeenRequested() || HC->HasRecookBeenRequested() || HC->IsParameterDefinitionUpdateNeeded();
				const bool bCacheRampParms = !HC->HasBeenLoaded() && !HC->HasBeenDuplicated();

				// Update the parameters
				FHoudiniParameterTranslator::UpdateParameters(
					HC->GetNodeId(),
					HC,
					HC->ParameterData->Parameters,
					HC->IsHoudiniAssetSupported() ? HC->HoudiniAssetData->HoudiniAsset : nullptr,
					HC->IsHoudiniAssetSupported() ? HC->HoudiniAssetData->HapiAssetName : FString(),
					bForceFullUpdate,
					bCacheRampParms,
					HC->bNeedToUpdateEditorProperties);

				// Since the HAC only has the asset definition's default parameter interface, without any asset or node ids,
				// we mark it has requiring a parameter definition sync. This will be carried out pre-cook.
				HC->ParameterData->bParameterDefinitionUpdateNeeded = true;
			}

			HC->SetCurrentState(EHoudiniAssetState::PreInstantiation);

			break;
		}

		case EHoudiniAssetState::PreInstantiation:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - PreInstantiation);

			//
			// TODO COOKABLE: HANDLE ASSET INPUT!!
			// Only proceed forward if we don't need to wait for our input HoudiniAssets to finish cooking/instantiating
			//if (HAC->NeedsToWaitForInputHoudiniAssets())
			//	break;

			// Make sure we empty the nodes to cook array to avoid cook errors caused by stale nodes 
			//if (HC->IsOutputsSupported())
			HC->ClearNodesToCook();

			EHoudiniAssetState NextState = EHoudiniAssetState::NeedInstantiation;

			// TODO COOKABLE: Better handling of NodeSync components!
			if(IsValid(MyHNSC))
			{
				// Directly fetch the node
				HAPI_NodeId FetchNodeId = -1;
				bool bFetchOK = (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeFromPath(
					FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*MyHNSC->GetFetchNodePath()), &FetchNodeId));

				if (bFetchOK)
				{
					// Set the new node ID
					HC->NodeId = FetchNodeId;

					// Assign a unique name to the actor if needed
					FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded(HC->NodeId, HC->GetOwner());

					// Reset the cook counter.
					HC->CookCount = 0;
					//if (HC->IsOutputsSupported())
						HC->ClearNodesToCook();

					// We can go to PreCook
					NextState = EHoudiniAssetState::PreCook;
				}
				else
				{
					// We couldn't create the node, change the state back to NeedInstantiation
					NextState = EHoudiniAssetState::NeedInstantiation;
					HC->bRecookRequested = false;
				}
			}
			else
			{
				// TODO COOKABLE: UPDATE ME! this only supports Cookable with assets...
				if (HC->IsHoudiniAssetSupported())
				{
					FGuid TaskGuid;
					FString HapiAssetName;
					UHoudiniAsset* HoudiniAsset = HC->HoudiniAssetData->HoudiniAsset;
					if (StartTaskAssetInstantiation(HoudiniAsset, HC->GetDisplayName(), HC->GetNodeLabelPrefix(), TaskGuid, HapiAssetName))
					{
						// The cookable is now instantiating
						NextState = EHoudiniAssetState::Instantiating;

						// Update the Task GUID
						HC->HapiGUID = TaskGuid;

						// Update the HapiAssetName
						HC->HoudiniAssetData->HapiAssetName = HapiAssetName;
					}
					else
					{
						// We couldnt instantiate the asset, change the state back to NeedInstantiation
						NextState = EHoudiniAssetState::NeedInstantiation;
					}
				}
			}

			// Update the Cookable's state
			HC->SetCurrentState(NextState);

			break;
		}

		case EHoudiniAssetState::Instantiating:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - Instantiating);
			EHoudiniAssetState NewState = EHoudiniAssetState::Instantiating;
			if (UpdateInstantiating(HC, NewState , HC->bDoSlateNotifications))
			{
				// We need to update the HAC's state
				HC->SetCurrentState(NewState);
				EnableEditorAutoSave(HC);
			}
			else
			{
				DisableEditorAutoSave(HC);
			}
			break;
		}

		case EHoudiniAssetState::PreCook:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - PreCook);
			// Only proceed forward if we don't need to wait for our input
			// HoudiniAssets to finish cooking/instantiating
			if(HC->IsInputSupported() && HC->InputData->NeedsToWaitForInputHoudiniAssets())
				break;

			if(MyHABC)
				MyHABC->OnPrePreCook();

			// Update all the HAPI nodes, parameters, inputs etc...
			PreCook(HC);

			if (MyHABC)
				MyHABC->OnPostPreCook();

			// Create a Cooking task only if necessary
			bool bCookStarted = false;
			if (IsCookingEnabledForCookable(HC))
			{
				TArray<int32> NodesToCook;

				bool bUseOutputNodes = true;
				bool bOutputTemplateGeos = false;
				if (HC->IsOutputSupported())
				{
					bUseOutputNodes = HC->OutputData->bUseOutputNodes;
					bOutputTemplateGeos = HC->OutputData->bOutputTemplateGeos;

					// Gather output nodes for the HAC
					FHoudiniEngineUtils::GatherAllAssetOutputs(
						HC->GetNodeId(),
						bUseOutputNodes,
						bOutputTemplateGeos,
						HC->OutputData->bEnableCurveEditing,
						NodesToCook);
				}


				HC->SetNodeIdsToCook(NodesToCook);

				FGuid TaskGUID = HC->HapiGUID;
				if (StartTaskAssetCooking(
					HC->GetNodeId(),
					NodesToCook,
					HC->GetDisplayName(),
					bUseOutputNodes,
					bOutputTemplateGeos,
					TaskGUID))
				{
					// Updates the cookable's state
					HC->SetCurrentState(EHoudiniAssetState::Cooking);
					HC->HapiGUID = TaskGUID;
					bCookStarted = true;
				}
			}

			if (!bCookStarted)
			{
	#if WITH_EDITORONLY_DATA
				// Just refresh editor properties?
				HC->bNeedToUpdateEditorProperties = true;
	#endif
				HC->SetCurrentState(EHoudiniAssetState::None);
			}
			break;
		}

		case EHoudiniAssetState::Cooking:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - Cooking);
			
			bool bCookSuccess = false; 
			EHoudiniAssetState NewState = EHoudiniAssetState::Cooking;			
			bool state = UpdateCooking(HC->HapiGUID, HC->GetDisplayName(), NewState,  HC->bDoSlateNotifications, bCookSuccess);
			if (state)
			{
				HC->bLastCookSuccess = bCookSuccess;

				// We need to update the HAC's state
				HC->SetCurrentState(NewState);
				EnableEditorAutoSave(HC);
			}
			else
			{
				DisableEditorAutoSave(HC);
			}
			break;
		}

		case EHoudiniAssetState::PostCook:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - PostCook);
			// Handle PostCook
			EHoudiniAssetState NewState = EHoudiniAssetState::None;
			bool bSuccess = HC->bLastCookSuccess;

			HC->HandleOnPreOutputProcessing();

			if(MyHABC)
				MyHABC->OnPreOutputProcessing();

			if (PostCook(HC))
			{
				// Cook was successful, process the results
				NewState = EHoudiniAssetState::PreProcess;
			}
			else
			{
				// Cook failed, skip output processing
				NewState = EHoudiniAssetState::None;
			}
			HC->SetCurrentState(NewState);
			break;
		}

		case EHoudiniAssetState::PreProcess:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - PreProcess);
			StartTaskAssetProcess(HC);
			break;
		}

		case EHoudiniAssetState::Processing:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - Processing);

			UpdateProcess(HC);

			HC->HandleOnPostOutputProcessing();
			if (MyHABC)
			{
				MyHABC->OnPostOutputProcessing();
				FHoudiniEngineUtils::UpdateBlueprintEditor(MyHABC);
			}

			// Update the cook count to prevent a cook loop
			const int32 CookCount = FHoudiniEngineUtils::HapiGetCookCount(HC->GetNodeId());
			HC->CookCount = CookCount;

			HC->SetCurrentState(EHoudiniAssetState::None);

			break;
		}

		case EHoudiniAssetState::None:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - None);

			// Update world input if we have any
			if (HC->IsInputSupported())
				FHoudiniInputTranslator::UpdateWorldInputs(HC->InputData->Inputs, HC->GetOwner());

			// Update our handles if needed
			// This may modify parameters so we need to call this before NeedUpdate
			if(HC->IsComponentSupported())
				FHoudiniHandleTranslator::UpdateHandlesIfNeeded(HC->ComponentData->HandleComponents);

			// Do nothing unless the HAC has been updated
			if (HC->NeedUpdate())
			{
				HC->bForceNeedUpdate = false;

				// Update the HAC's state
				// Cook for valid nodes - instantiate for invalid nodes
				if (FHoudiniEngineUtils::IsHoudiniNodeValid(HC->GetNodeId()))
					HC->SetCurrentState(EHoudiniAssetState::PreCook);
				else
				{
					// Mark as "NeedCook" first to make sure we preserve/upload all params/inputs
					HC->MarkAsNeedCook();
					HC->SetCurrentState(EHoudiniAssetState::PreInstantiation);
				}
			}
			else if ( HC->IsComponentSupported() 
				&& HC->ComponentData->bCookOnTransformChange
				&& HC->ComponentData->bUploadTransformsToHoudiniEngine
				&& HC->ComponentData->bHasComponentTransformChanged)
			{
				FHoudiniEngineUtils::UploadCookableTransform(HC);
			}

			if (HC->IsComponentSupported())
			{
				// See if we need to get an update from Session Sync
				bool bEnableLiveSync = FHoudiniEngine::Get().IsSessionSyncEnabled()
					&& FHoudiniEngine::Get().IsSyncWithHoudiniCookEnabled()
					&& HC->GetCurrentState() == EHoudiniAssetState::None;

				if (MyHNSC)
				{
					bEnableLiveSync = MyHNSC ? MyHNSC->GetLiveSyncEnabled() : false;
				}

				if (bEnableLiveSync)
				{
					double dNow = FPlatformTime::Seconds();
					double dLiveSyncTick = CVarHoudiniEngineLiveSyncTickTime.GetValueOnAnyThread();
					if ((dNow - HC->ComponentData->LastLiveSyncPingTime) > dLiveSyncTick)
					{
						// Update the last live sync ping time for this component
						HC->ComponentData->LastLiveSyncPingTime = dNow;

						int32 CookCount = FHoudiniEngineUtils::HapiGetCookCount(HC->GetNodeId());
						if (CookCount >= 0 && CookCount != HC->CookCount)
						{
							if (HC->bAutoCook)
							{
								// The cook count has changed on the Houdini side,
								// this indicates that the user has changed something in Houdini so we need to trigger an update
								HC->SetCurrentState(EHoudiniAssetState::PreCook);
								// Make sure to update the cookcount to prevent loop cooking
								HC->CookCount = CookCount;
							}
						}
					}
				}
			}
			break;
		}

		case EHoudiniAssetState::NeedRebuild:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - NeedRebuild);
			if(HC->IsParameterSupported() && HC->GetNodeId() >= 0)
			{
				// Make sure no parameters are changed before getting the preset
				bool bCleanParamPreset = false;
				if (FHoudiniParameterTranslator::UploadChangedParameters(
					HC->ParameterData->Parameters, HC->GetNodeId()))
				{
					if (!FHoudiniEngineUtils::GetAssetPreset(HC->GetNodeId(), HC->ParameterData->ParameterPresetBuffer))
					{
						HOUDINI_LOG_WARNING(TEXT("Failed to get the asset's parameter preset, rebuilt asset may have lost its parameters."));
						bCleanParamPreset = true;
					}
				}
				else
				{
					bCleanParamPreset = true;
				}

				// If we failed to update params or get the preset buffer, dont use it
				if (bCleanParamPreset)
					HC->ParameterData->ParameterPresetBuffer.Empty();
			}

			if (!MyHNSC)
			{
				// Do not delete nodes for NodeSync components!
				StartTaskAssetRebuild(HC->GetNodeId(), HC->HapiGUID);
			}

			if (HC->IsPDGSupported())
			{
				// We want to check again for PDG after a rebuild
				HC->PDGData->bIsPDGAssetLinkInitialized = false;
			}

			//HC->MarkAsNeedCook();
			HC->SetCurrentState(EHoudiniAssetState::PreInstantiation);
			break;
		}

		case EHoudiniAssetState::NeedDelete:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - NeedDelete);
			if (!MyHNSC)
			{
				// Do not delete nodes for NodeSync components!
				FGuid HapiDeletionGUID;
				StartTaskAssetDelete(HC->GetNodeId(), HapiDeletionGUID, true);
			}

			// Update the HAC's state
			HC->SetCurrentState(EHoudiniAssetState::Deleting);
			break;
		}

		case EHoudiniAssetState::Deleting:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::ProcessCookable - Deleting);
			break;
		}

		case EHoudiniAssetState::Dormant:
			break;
	}
}



bool 
FHoudiniEngineManager::StartTaskAssetInstantiation(
	UHoudiniAsset* HoudiniAsset, 
	const FString& DisplayName,
	const FString& NodeLabelPrefix,
	FGuid& OutTaskGUID, 
	FString& OutHAPIAssetName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::StartTaskAssetInstantiation);

	// Make sure we have a valid session before attempting anything
	if (!FHoudiniEngine::Get().GetSession())
		return false;

	OutTaskGUID.Invalidate();
	
	// Load the HDA file
	if (!IsValid(HoudiniAsset))
	{
		HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - null or invalid Houdini Asset."));
		return false;
	}

	HAPI_AssetLibraryId AssetLibraryId = -1;
	if (!FHoudiniEngineUtils::LoadHoudiniAsset(HoudiniAsset, AssetLibraryId) )
	{
		HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - could not load Houdini Asset."));
		return false;
	}

	// Handle hda files that contain multiple assets
	TArray<HAPI_StringHandle> AssetNames;
	if (!FHoudiniEngineUtils::GetSubAssetNames(AssetLibraryId, AssetNames))
	{
		HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - unable to retrieve asset names."));
		return false;
	}

	// By default, assume we want to load the first Asset
	HAPI_StringHandle PickedAssetName = AssetNames[0];

#if WITH_EDITOR
	// Should we show the multi asset dialog?
	bool bShowMultiAssetDialog = false;

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings && AssetNames.Num() > 1)
		bShowMultiAssetDialog = HoudiniRuntimeSettings->bShowMultiAssetDialog;

	if (bShowMultiAssetDialog )
	{
		if(!FHoudiniEngineUtils::OpenSubassetSelectionWindow(AssetNames, PickedAssetName))
		{
			HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - no asset choosen in the selection window."));
			return false;
		}
	}
#endif

	// Give the HAC a new GUID to identify this request.
	OutTaskGUID = FGuid::NewGuid();

	// Create a new instantiation task
	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetInstantiation, OutTaskGUID);
	Task.Asset = HoudiniAsset;
	Task.ActorName = DisplayName;
	//Task.bLoadedComponent = bLocalLoadedComponent;
	Task.AssetLibraryId = AssetLibraryId;
	Task.AssetHapiName = PickedAssetName;
	Task.NodeLabelPrefix = NodeLabelPrefix;

	FHoudiniEngineString(PickedAssetName).ToFString(OutHAPIAssetName);

	// Add the task to the stack
	FHoudiniEngine::Get().AddTask(Task);

	return true;
}

bool
FHoudiniEngineManager::UpdateInstantiating(UHoudiniCookable* HC, EHoudiniAssetState& NewState, bool bDoNotifications)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::UpdateInstantiating);

	check(HC);

	// Will return true if the cookable's state need to be updated
	NewState = HC->GetCurrentState();
	bool bUpdateState = false;

	// Get the HAC display name for the logs
	FString DisplayName = HC->GetDisplayName();

	// Get the current task's progress
	FHoudiniEngineTaskInfo TaskInfo;
	if (!UpdateTaskStatus(HC->HapiGUID, TaskInfo, bDoNotifications)
		|| TaskInfo.TaskType != EHoudiniEngineTaskType::AssetInstantiation)
	{
		// Couldnt get a valid task info
		HOUDINI_LOG_ERROR(TEXT("    %s Failed to instantiate - invalid task"), *DisplayName);
		NewState = EHoudiniAssetState::NeedInstantiation;
		bUpdateState = true;
		return bUpdateState;
	}

	bool bSuccess = false;
	bool bFinished = false;
	switch (TaskInfo.TaskState)
	{
		case EHoudiniEngineTaskState::Success:
		{
			bSuccess = true;
			bFinished = true;
			break;
		}

		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::FinishedWithError:
		case EHoudiniEngineTaskState::FinishedWithFatalError:
		{
			bSuccess = false;
			bFinished = true;
			break;
		}

		case EHoudiniEngineTaskState::None:
		case EHoudiniEngineTaskState::Working:
		{
			bFinished = false;
			break;
		}
	}

	if (!bFinished)
	{
		// Task is still in progress, nothing to do for now
		return false;
	}

	if (bSuccess && (TaskInfo.AssetId < 0))
	{
		// Task finished successfully but we received an invalid asset ID, error out
		HOUDINI_LOG_ERROR(TEXT("    %s Finished Instantiation but received invalid asset id."), *DisplayName);
		bSuccess = false;
	}

	if (bSuccess)
	{
		HOUDINI_LOG_MESSAGE(TEXT("    %s FinishedInstantiation."), *DisplayName);

		// Set the new node ID
		HC->NodeId = TaskInfo.AssetId;

		// Assign a unique name to the actor if needed
		FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded(HC->NodeId, HC->GetOwner());

		// Reset the cook counter and nodes to cook
		HC->CookCount = 0;
		HC->ClearNodesToCook();

		// If necessary, set asset transform to the component's transform.
		if (HC->IsComponentSupported() 
			&& HC->ComponentData->bUploadTransformsToHoudiniEngine
			&& IsValid(HC->ComponentData->Component.Get()))
		{
			// Retrieve the current component-to-world transform for this component.
			if (!FHoudiniEngineUtils::HapiSetAssetTransform(HC->NodeId, HC->ComponentData->Component->GetComponentTransform()))
				HOUDINI_LOG_MESSAGE(TEXT("Failed to upload the initial Transform back to HAPI."));
		}

		// Initial update/create of inputs
		if (HC->IsInputSupported())
		{
			FHoudiniInputTranslator::UpdateInputs(
				HC->GetNodeId(), 
				HC,
				HC->InputData->Inputs,
				HC->ParameterData->Parameters,
				HC->HasBeenLoaded());
		}


		// Update the HAC's state
		NewState = EHoudiniAssetState::PreCook;
		return true;
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("    %s FinishedInstantiationWithErrors."), *DisplayName);

		bool bLicensingIssue = false;
		switch (TaskInfo.Result)
		{
		case HAPI_RESULT_NO_LICENSE_FOUND:
		case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND:
		{
			// No license / Apprentice license found
			//FHoudiniEngine::Get().SetHapiState(HAPI_RESULT_NO_LICENSE_FOUND);
			FHoudiniEngine::Get().SetSessionStatus(EHoudiniSessionStatus::NoLicense);
			bLicensingIssue = true;
			break;
		}

		case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE:
		case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE:
		case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE:
		{
			bLicensingIssue = true;
			break;
		}

		default:
		{
			break;
		}
		}

		if (bLicensingIssue)
		{
			const FString& StatusMessage = TaskInfo.StatusText.ToString();
			HOUDINI_LOG_MESSAGE(TEXT("%s"), *StatusMessage);

			FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
			FText WarningTitleText = FText::FromString(WarningTitle);
			FString WarningMessage = FString::Printf(TEXT("Houdini License issue - %s."), *StatusMessage);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			FMessageDialog::Debugf(FText::FromString(WarningMessage), WarningTitleText);
#else
			FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);
#endif
		}

		// Reset the cook counter.
		HC->CookCount = 0;

		// Make sure the asset ID is invalid
		HC->NodeId = -1;

		// Prevent the HAC from triggering updates in its current state
		HC->PreventAutoUpdates();

		// Update the HAC's state
		HC->SetCurrentState(EHoudiniAssetState::NeedInstantiation);
		//HAC->AssetStateResult = EHoudiniAssetStateResult::Success;

		return true;
	}
}

bool
FHoudiniEngineManager::StartTaskAssetCooking(
	const HAPI_NodeId& AssetId,
	const TArray<HAPI_NodeId>& NodeIdsToCook,
	const FString& DisplayName,
	bool bUseOutputNodes,
	bool bOutputTemplateGeos,
	FGuid& OutTaskGUID)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::StartTaskAssetCooking);

	// Make sure we have a valid session before attempting anything
	if (!FHoudiniEngine::Get().GetSession())
		return false;

	// Check we have a valid AssetId
	if (AssetId < 0)
		return false;

	// Check this HAC doesn't already have a running task
	if (OutTaskGUID.IsValid())
		return false;

	// Generate a GUID for our new task.
	OutTaskGUID = FGuid::NewGuid();

	// Add a new cook task
	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, OutTaskGUID);
	Task.ActorName = DisplayName;
	Task.AssetId = AssetId;

	if (NodeIdsToCook.Num() > 0)
		Task.OtherNodeIds = NodeIdsToCook;

	Task.bUseOutputNodes = bUseOutputNodes;
	Task.bOutputTemplateGeos = bOutputTemplateGeos;

	FHoudiniEngine::Get().AddTask(Task);

	return true;
}

bool
FHoudiniEngineManager::UpdateCooking(
	FGuid& HapiTaskGUID, const FString& DisplayName, EHoudiniAssetState& OutNewState, bool bDoNotifications, bool& OutSuccess)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::UpdateCooking);

	// Will return true if the asset's state need to be updated
	bool bUpdateState = false;
	OutNewState = EHoudiniAssetState::Cooking;

	// Get the current task's progress
	FHoudiniEngineTaskInfo TaskInfo;
	if (!UpdateTaskStatus(HapiTaskGUID, TaskInfo, bDoNotifications)
		|| TaskInfo.TaskType != EHoudiniEngineTaskType::AssetCooking)
	{
		// Couldnt get a valid task info
		HOUDINI_LOG_ERROR(TEXT("    %s Failed to cook - invalid task"), *DisplayName);
		OutNewState = EHoudiniAssetState::None;
		bUpdateState = true;
		return bUpdateState;
	}

	OutSuccess = false;
	switch (TaskInfo.TaskState)
	{
		case EHoudiniEngineTaskState::Success:
		{
			HOUDINI_LOG_MESSAGE(TEXT("   %s FinishedCooking."), *DisplayName);
			OutSuccess = true;
			bUpdateState = true;
		}
		break;

		case EHoudiniEngineTaskState::FinishedWithError:
		{
			// We finished with cook error, will still try to process the results
			HOUDINI_LOG_MESSAGE(TEXT("   %s FinishedCooking with errors - will try to process the available results."), *DisplayName);
			OutSuccess = true;
			bUpdateState = true;
		}
		break;

		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::FinishedWithFatalError:
		{
			HOUDINI_LOG_MESSAGE(TEXT("   %s FinishedCooking with fatal errors - aborting."), *DisplayName);
			OutSuccess = false;
			bUpdateState = true;
		}
		break;

		case EHoudiniEngineTaskState::None:
		case EHoudiniEngineTaskState::Working:
		{
			// Task is still in progress, nothing to do for now
			// return false so we do not update the state
			bUpdateState = false;
		}
		break;
	}

	// If the task is still in progress, return now
	if (!bUpdateState)
		return false;
	   
	// Handle PostCook
	OutNewState = EHoudiniAssetState::PostCook;
	//HAC->bLastCookSuccess = bSuccess;

	return true;
}


bool
FHoudiniEngineManager::PreCook(UHoudiniCookable* HC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::PreCook);

	if (HC->IsOutputSupported())
	{
		// Remove all Cooked (layers) before cooking so we don't received cooked data in Houdini
		// if a landscape is input back to the HDA.
		for (auto CurOutput : HC->OutputData->Outputs)
		{
			FHoudiniLandscapeRuntimeUtils::DeleteLandscapeCookedData(CurOutput);
		}
	}


	// Handle duplicated HAC
	// We need to clean/duplicate some of the HAC's output data manually here
	if (HC->HasBeenDuplicated())
	{
		HC->UpdatePostDuplicate();
	}

	if (HC->IsParameterSupported())
	{
		FHoudiniParameterTranslator::OnPreCookParameters(HC->ParameterData->Parameters);

		if (HC->HasBeenLoaded() || HC->IsParameterDefinitionUpdateNeeded())
		{
			if (!HC->ParameterData->ParameterPresetBuffer.IsEmpty())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::PreCook - SetPreset);

				// We only apply parameters presets for rebuilds
				if (HC->HasRebuildBeenRequested())
				{
					FHoudiniEngineUtils::SetAssetPreset(HC->GetNodeId(), HC->ParameterData->ParameterPresetBuffer);
				}

				HC->ParameterData->ParameterPresetBuffer.Empty();
			}

			// This will sync parameter definitions but not upload values to HAPI or fetch values for existing parameters
			// in Unreal. It will creating missing parameters in Unreal.
			//FHoudiniParameterTranslator::UpdateLoadedParameters(HAC);

			bool bForceFullUpdate = HC->HasRebuildBeenRequested() || HC->HasRecookBeenRequested() || HC->IsParameterDefinitionUpdateNeeded();
			bool bCacheRampParms = !HC->HasBeenLoaded() && !HC->HasBeenDuplicated();
			FHoudiniParameterTranslator::UpdateLoadedParameters(
				HC->GetNodeId(),
				HC->ParameterData->Parameters,
				HC,
				bForceFullUpdate,
				bCacheRampParms,
				HC->bNeedToUpdateEditorProperties);

			HC->ParameterData->bParameterDefinitionUpdateNeeded = false;
		}
	}

	// Upload the changed/parameters back to HAPI
	// If cooking is disabled, we still try to upload parameters
	if (HC->HasBeenLoaded())
	{
		// // Handle loaded parameters
		// FHoudiniParameterTranslator::UpdateLoadedParameters(HAC);

		if(HC->IsInputSupported())
		{
			// Handle loaded inputs
			FHoudiniInputTranslator::UpdateInputs(
				HC->GetNodeId(), HC, HC->InputData->Inputs, HC->ParameterData->Parameters, HC->HasBeenLoaded());
		}

		if (HC->IsOutputSupported())
		{
			// Handle loaded outputs
			FHoudiniOutputTranslator::UpdateLoadedOutputs(
				HC->GetNodeId(),
				HC->OutputData->Outputs,
				HC->GetComponent());
		}		

		// TODO: Handle loaded curve ?
		// TODO: Handle editable node ?
	}

	if (HC->IsParameterSupported())
	{
		// Try to upload changed parameters
		FHoudiniParameterTranslator::UploadChangedParameters(
			HC->ParameterData->Parameters, HC->GetNodeId());
	}	

	if (HC->IsInputSupported())
	{
		// Try to upload changed inputs
		FHoudiniInputTranslator::UploadChangedInputs(HC->InputData->Inputs, HC->GetOwner());
	}

	if (HC->IsOutputSupported())
	{
		// Try to upload changed editable nodes
		FHoudiniOutputTranslator::UploadChangedEditableOutput(HC->OutputData->Outputs);
	}	

	// Upload the cookable's transform if needed
	if (HC->IsComponentSupported())
	{
		if (HC->ComponentData->bHasComponentTransformChanged
			&& HC->ComponentData->bUploadTransformsToHoudiniEngine)
		{
			FHoudiniEngineUtils::UploadCookableTransform(HC);
		}
	}

	HC->ClearRefineMeshesTimer();

	return true;
}

bool
FHoudiniEngineManager::PostCook(UHoudiniCookable* HC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::PostCook);

	// Get the HAC display name for the logs
	FString DisplayName = HC->GetDisplayName();

	//bool bCookSuccess = bLSuccess;
	if (HC->bLastCookSuccess && (HC->GetNodeId() < 0))
	{
		// Task finished successfully but we received an invalid asset ID, error out
		HOUDINI_LOG_ERROR(TEXT("    %s received an invalid asset id - aborting."), *DisplayName);
		HC->bLastCookSuccess = false;
	}

	// Update the asset cook count using the node infos
	const int32 CookCount = FHoudiniEngineUtils::HapiGetCookCount(HC->GetNodeId());
	HC->CookCount = CookCount;

	bool bNeedsToTriggerViewportUpdate = false;
	if (HC->bLastCookSuccess)
	{
		if (HC->bDoSlateNotifications)
			FHoudiniEngine::Get().UpdateCookingNotification(FText::FromString(DisplayName + " :\nProcessing outputs..."), false);

		//
		// PARAMETERS
		//
		if (HC->IsParameterSupported())
		{
			//FHoudiniParameterTranslator::UpdateParameters(HAC);

			// When recooking/rebuilding the HDA, force a full update of all params
			const bool bForceFullUpdate = HC->HasRebuildBeenRequested() || HC->HasRecookBeenRequested() || HC->IsParameterDefinitionUpdateNeeded();
			const bool bCacheRampParms = !HC->HasBeenLoaded() && !HC->HasBeenDuplicated();
			FHoudiniParameterTranslator::UpdateParameters(
				HC->GetNodeId(),
				HC,
				HC->ParameterData->Parameters,
				HC->IsHoudiniAssetSupported() ? HC->HoudiniAssetData->HoudiniAsset : nullptr,
				HC->IsHoudiniAssetSupported() ? HC->HoudiniAssetData->HapiAssetName : FString(),
				bForceFullUpdate,
				bCacheRampParms,
				HC->bNeedToUpdateEditorProperties);
		}

		//
		// INPUTS
		//
		// TODO COOKABLE: Do asset preset after input (obj path param?).
		// Set bLoadedInputs to false so as not to delete outputs.
		if (HC->IsInputSupported())
		{
			// Update our inputs
			FHoudiniInputTranslator::UpdateInputs(
				HC->GetNodeId(),
				HC,
				HC->InputData->Inputs,
				HC->ParameterData->Parameters,
				false);
		}

		// Update the HDA's parameter preset
		// This needs to be done after inputs and parameters updates
		if (HC->IsParameterSupported())
		{
			if (!FHoudiniEngineUtils::GetAssetPreset(HC->GetNodeId(), HC->ParameterData->ParameterPresetBuffer))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to get the asset's preset."));
				HC->ParameterData->ParameterPresetBuffer.Empty();
			}
		}	

		//
		// OUTPUTS
		//
		if (HC->IsOutputSupported())
		{
			// Update our output objects
			// We will process them at the processing stage
			FHoudiniOutputTranslator::UpdateOutputs(HC);

			if(HC->IsProxySupported())
				HC->ProxyData->bNoProxyMeshNextCookRequested = false;
		}

		//
		// HANDLES
		//
		// Handles have to be built after the parameters
		FHoudiniHandleTranslator::BuildHandles(HC);

		// We can clear the duplication flag
		if (HC->HasBeenDuplicated())
		{
			HC->bHasBeenDuplicated = false;
		}
	}

	// Cache the current cook counts of the nodes so that we can more reliable determine
	// whether content has changed next time build outputs.
	for (int32 NodeId : HC->NodeIdsToCook)
	{
		int32 NodeCookCount = FHoudiniEngineUtils::HapiGetCookCount(NodeId);
		HC->NodesToCookCookCounts.Add(NodeId, CookCount);
	}

	// See if we need to initialize the PDG Asset Link for this HDA
	if (HC->IsPDGSupported())
	{
		if (!HC->PDGData->bIsPDGAssetLinkInitialized)
		{
			if (FHoudiniPDGManager::IsPDGAsset(HC->NodeId))
			{
				UHoudiniPDGAssetLink* PDGAssetLink = HC->PDGData->PDGAssetLink;
				if (!PDGManager.InitializePDGAssetLink(HC->NodeId, HC, PDGAssetLink, HC->bHasBeenLoaded))
					HC->PDGData->SetPDGAssetLink(nullptr);
				else
					HC->PDGData->SetPDGAssetLink(PDGAssetLink);
			}

			// Only do this once per cookable - only check again on rebuild
			HC->PDGData->bIsPDGAssetLinkInitialized = true;
		}

		// Notify the PDG manager that the HDA is done cooking
		FHoudiniPDGManager::NotifyAssetCooked(HC->PDGData->PDGAssetLink, HC->bLastCookSuccess);
	}


	// Clear the HasBeenLoaded flag
	if (HC->HasBeenLoaded())
	{
		HC->bHasBeenLoaded = false;
	}

	// If we have downstream cookables, we need to tell them we're done cooking
	HC->NotifyCookedToDownstreamCookables();

	// Clear the rebuild/recook flags
	HC->bRecookRequested = false;
	HC->bRebuildRequested = false;

	//HC->SyncToBlueprintGeneratedClass();

	return HC->bLastCookSuccess;
}


bool
FHoudiniEngineManager::StartTaskAssetProcess(UHoudiniCookable* HC)
{
	HC->SetCurrentState(EHoudiniAssetState::Processing);

	return true;
}

bool
FHoudiniEngineManager::UpdateProcess(UHoudiniCookable* HC)
{
	// We should only process after a succesfull cook
	if (!HC->bLastCookSuccess)
		return false;

	bool bNeedsToTriggerViewportUpdate = false;
	bool bHasHoudiniStaticMeshOutput = false;

	// ?? this was unused
	//bool bForceOutputUpdate = HAC->HasRebuildBeenRequested() || HAC->HasRecookBeenRequested();
	FHoudiniOutputTranslator::ProcessOutputs(HC, bHasHoudiniStaticMeshOutput);
	
	if (HC->IsProxySupported())
		HC->ProxyData->bNoProxyMeshNextCookRequested = false;

	//
	// COMPONENTS
	// 
	// Component updates if supported
	if (HC->IsComponentSupported())
	{
		USceneComponent* MyComponent = Cast<USceneComponent>(HC->GetComponent());
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(HC->GetComponent());

		// Update Physics state.
		if (MyHAC)
			MyHAC->UpdatePhysicsState();

		if (MyComponent)
		{
			// Mark  Render State as dirty
			MyComponent->MarkRenderStateDirty();

			// Since we have new asset, we need to update bounds.
			MyComponent->UpdateBounds();
		}

		if (HC->IsProxySupported())
		{
			// If any outputs have HoudiniStaticMeshes, and if timer based refinement is enabled on the HAC,
			// set the RefineMeshesTimer and ensure BuildStaticMeshesForAllHoudiniStaticMeshes is bound to
			// the RefineMeshesTimerFired delegate of the HAC
			if (bHasHoudiniStaticMeshOutput && HC->IsProxyStaticMeshRefinementByTimerEnabled())
			{
				if (!HC->ProxyData->GetOnRefineMeshesTimerDelegate().IsBoundToObject(this))
					HC->ProxyData->GetOnRefineMeshesTimerDelegate().AddRaw(this, &FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes);
				HC->SetRefineMeshesTimer();
			}
		}
	}

#if WITH_EDITORONLY_DATA
	// Indicate we want to trigger a details panel update
	HC->bNeedToUpdateEditorProperties = true;
#endif

	if (bHasHoudiniStaticMeshOutput)
		bNeedsToTriggerViewportUpdate = true;

	if (bNeedsToTriggerViewportUpdate && GEditor)
	{
		// We need to manually update the viewport with HoudiniMeshProxies
		// if not, modification made in H with the two way debugger wont be visible in Unreal until the viewports gets focus
		GEditor->RedrawAllViewports(false);
	}

	// Indicate we're done processing the asset
	FString DisplayName = HC->GetDisplayName();
	if (HC->bDoSlateNotifications)
		FHoudiniEngine::Get().UpdateCookingNotification(FText::FromString(DisplayName + " :\nFinished processing outputs"), true);

	return true;
}

bool
FHoudiniEngineManager::StartTaskAssetRebuild(const HAPI_NodeId& InAssetId, FGuid& OutTaskGUID)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::StartTaskAssetRebuild);
	// Check this HAC doesn't already have a running task
	if (OutTaskGUID.IsValid())
		return false;

	if (InAssetId >= 0)
	{
		// Delete the asset
		if (!StartTaskAssetDelete(InAssetId, OutTaskGUID, true))
		{
			return false;
		}
	}

	// Create a new task GUID for this asset
	OutTaskGUID = FGuid::NewGuid();

	return true;
}

bool
FHoudiniEngineManager::StartTaskAssetDelete(const HAPI_NodeId& InNodeId, FGuid& OutTaskGUID, bool bShouldDeleteParent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::StartTaskAssetDelete);

	if (InNodeId < 0)
		return false;

	// Get the Asset's NodeInfo
	HAPI_NodeInfo AssetNodeInfo;
	FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId, &AssetNodeInfo));
	
	HAPI_NodeId OBJNodeToDelete = InNodeId;
	if (AssetNodeInfo.type == HAPI_NODETYPE_SOP)
	{
		// For SOP Asset, we want to delete their parent's OBJ node
		if (bShouldDeleteParent)
		{
			HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId(OBJNodeToDelete);
			OBJNodeToDelete = ParentId != -1 ? ParentId : OBJNodeToDelete;
		}
	}

	// Generate GUID for our new task.
	OutTaskGUID = FGuid::NewGuid();

	// Create asset deletion task object and submit it for processing.
	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetDeletion, OutTaskGUID);
	Task.AssetId = OBJNodeToDelete;
	FHoudiniEngine::Get().AddTask(Task);

	return true;
}

bool
FHoudiniEngineManager::UpdateTaskStatus(FGuid& OutTaskGUID, FHoudiniEngineTaskInfo& OutTaskInfo, bool bDoNotifications)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::UpdateTaskStatus);

	if (!OutTaskGUID.IsValid())
		return false;
	
	if (!FHoudiniEngine::Get().RetrieveTaskInfo(OutTaskGUID, OutTaskInfo))
	{
		// Task information does not exist
		OutTaskGUID.Invalidate();
		return false;
	}

	if (bDoNotifications && EHoudiniEngineTaskState::None != OutTaskInfo.TaskState)
	{
		FHoudiniEngine::Get().UpdateCookingNotification(OutTaskInfo.StatusText, false);
	}

	switch (OutTaskInfo.TaskState)
	{
		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::FinishedWithError:
		case EHoudiniEngineTaskState::FinishedWithFatalError:
		{
			// If the current task is finished
			// Terminate the slate notification if they exist and delete/invalidate the task
			if(bDoNotifications)
				FHoudiniEngine::Get().UpdateCookingNotification(OutTaskInfo.StatusText, true);
			FHoudiniEngine::Get().RemoveTaskInfo(OutTaskGUID);
			OutTaskGUID.Invalidate();
		}
		break;

		
		case EHoudiniEngineTaskState::Success:
		{
			// End the task
			FHoudiniEngine::Get().RemoveTaskInfo(OutTaskGUID);
			OutTaskGUID.Invalidate();

			// Do not terminate cooking/processing notifications
			if (OutTaskInfo.TaskType == EHoudiniEngineTaskType::AssetCooking || OutTaskInfo.TaskType == EHoudiniEngineTaskType::AssetProcess)
				break;

			// Terminate the current notification
			if (bDoNotifications)
				FHoudiniEngine::Get().UpdateCookingNotification(OutTaskInfo.StatusText, false);

		}
		break;
		
		case EHoudiniEngineTaskState::Working:
		case EHoudiniEngineTaskState::None:
		default:
		{
			break;
		}
	}

	return true;
}


bool
FHoudiniEngineManager::IsCookingEnabledForCookable(UHoudiniCookable* HC)
{
	bool bManualRecook = false;
	bool bComponentEnable = false;
	if (IsValid(HC))
	{
		bManualRecook = HC->HasRecookBeenRequested();
		bComponentEnable = HC->IsCookingEnabled();
	}

	if (bManualRecook)
		return true;

	if (bComponentEnable && FHoudiniEngine::Get().IsCookingEnabled())
		return true;

	return false;
}

void 
FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes(UHoudiniCookable* HC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes);

	if (!IsValid(HC))
	{
		HOUDINI_LOG_ERROR(TEXT("FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes called with HC=nullptr"));
		return;
	}

#if WITH_EDITOR
	AActor *Owner = HC->GetOwner();
	FString Name = Owner ? Owner->GetName() : HC->GetName();

	FScopedSlowTask Progress(2.0f, FText::FromString(FString::Printf(TEXT("Refining Proxy Mesh to Static Mesh on %s"), *Name)));
	Progress.MakeDialog();
	Progress.EnterProgressFrame(1.0f);
#endif

	FHoudiniOutputTranslator::BuildStaticMeshesOnHoudiniProxyMeshOutputs(HC);

#if WITH_EDITOR
	Progress.EnterProgressFrame(1.0f);
#endif
}


/* Unreal's viewport representation rules:
   Viewport location is the actual camera location;
   Lookat position is always right in front of the camera, which means the camera is looking at;
   The rotator rotates the forward vector to a direction & orientation, and this dir and orientation is the camera's;
   The identity direction and orientation of the camera is facing positive X-axis.
*/

/* Hapi's viewport representation rules:
	 The camera is located at a point on the sphere, which the center is the pivot position and the radius is offset;
	 Quat determines the location on the sphere and which direction the camera is facing towards, as well as the camera orientation;
	 The identity location, direction and orientation of the camera is facing positive Z-axis (in Hapi coords);
*/
bool
FHoudiniEngineManager::SyncHoudiniViewportToUnreal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::SyncHoudiniViewportToUnreal);

	if (!FHoudiniEngine::Get().IsSyncHoudiniViewportEnabled())
		return false;

#if WITH_EDITOR
	// Get the editor viewport LookAt position to spawn the new objects
	if (!GEditor || !GEditor->GetActiveViewport())
		return false;
	
	FEditorViewportClient* ViewportClient = (FEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
	if (!ViewportClient)
		return false;

	// Get the current UE viewport location, lookat position, and rotation
	FVector UnrealViewportPosition = ViewportClient->GetViewLocation();
	FRotator UnrealViewportRotation = ViewportClient->GetViewRotation();
	FVector UnrealViewportLookatPosition = ViewportClient->GetLookAtLocation();
	
	/* Check if the Unreal viewport has changed */
	if (UnrealViewportPosition.Equals(SyncedUnrealViewportPosition) &&
		UnrealViewportRotation.Equals(SyncedUnrealViewportRotation) &&
		UnrealViewportLookatPosition.Equals(SyncedUnrealViewportLookatPosition))
	{
		// No need to sync if the viewport camera hasn't changed
		return false;
	}

	/* Calculate Hapi Quaternion */
	// Initialize Hapi Quat with Unreal Quat.
	// Note that rotations are in general non-commutative ***
	FQuat HapiQuat = UnrealViewportRotation.Quaternion();

	// We're in orbit mode, forward vector is Y-axis
	if (ViewportClient->bUsingOrbitCamera)
	{
		// The forward vector is Y-negative direction when on orbiting mode
		HapiQuat = HapiQuat * FQuat::MakeFromEuler(FVector(0.f, 0.f, 180.f));

		// rotations around X and Y axis are reversed
		float TempX = HapiQuat.X;
		HapiQuat.X = HapiQuat.Y;
		HapiQuat.Y = TempX;
		HapiQuat.W = -HapiQuat.W;

	}
	// We're not in orbiting mode, forward vector is X-axis
	else
	{
		// Rotate the Quat arount Z-axis by 90 degree.
		HapiQuat = HapiQuat * FQuat::MakeFromEuler(FVector(0.f, 0.f, 90.f));
	}


	/* Update Hapi H_View */
	// Note: There are infinte number of H_View representation for current viewport
	//       Each choice of pivot point determines an equivalent representation.
	//       We just find an equivalent when the pivot position is the view position, and offset is 0

	HAPI_Viewport H_View;
	H_View.position[0] = UnrealViewportPosition.X  / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
	H_View.position[1] = UnrealViewportPosition.Z  / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
	H_View.position[2] = UnrealViewportPosition.Y  / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

	// Set HAPI_Offset always 0 when syncing Houdini to UE viewport
	H_View.offset = 0.f;

	H_View.rotationQuaternion[0] = -HapiQuat.X;
	H_View.rotationQuaternion[1] = -HapiQuat.Z;
	H_View.rotationQuaternion[2] = -HapiQuat.Y;
	H_View.rotationQuaternion[3] = HapiQuat.W;

	FHoudiniApi::SetViewport(FHoudiniEngine::Get().GetSession(), &H_View);

	/* Update the Synced viewport values 
	   We need to syced both the viewport representation values in Hapi and UE whenever the viewport is changed.
	   Since the 2 representations are multi-multi correspondence, the values could be changed even though the viewport is not changing. */

	// We need to get the H_Viewport again, since it is possible the value is a different equivalence of what we set.
	HAPI_Viewport Cur_H_View;
	FHoudiniApi::GetViewport(
		FHoudiniEngine::Get().GetSession(), &Cur_H_View);

	// Hapi values are in Houdini coordinate and scale
	SyncedHoudiniViewportPivotPosition = FVector(Cur_H_View.position[0], Cur_H_View.position[1], Cur_H_View.position[2]);
	SyncedHoudiniViewportQuat = FQuat(Cur_H_View.rotationQuaternion[0], Cur_H_View.rotationQuaternion[1], Cur_H_View.rotationQuaternion[2], Cur_H_View.rotationQuaternion[3]);
	SyncedHoudiniViewportOffset = Cur_H_View.offset;

	SyncedUnrealViewportPosition = ViewportClient->GetViewLocation();
	SyncedUnrealViewportRotation = ViewportClient->GetViewRotation();
	SyncedUnrealViewportLookatPosition = ViewportClient->GetLookAtLocation();

	// When sync Houdini to UE, we set offset to be 0.
	// So we need to zero out offset for the next time syncing UE to Houdini
	bOffsetZeroed = true;

	return true;
#endif

	return false;
}


bool
FHoudiniEngineManager::SyncUnrealViewportToHoudini()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineManager::SyncUnrealViewportToHoudini);

	if (!FHoudiniEngine::Get().IsSyncUnrealViewportEnabled())
		return false;

#if WITH_EDITOR
	// Get the editor viewport LookAt position to spawn the new objects
	if (!GEditor || !GEditor->GetActiveViewport())
		return false;

	FEditorViewportClient* ViewportClient = (FEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
	if (!ViewportClient)
		return false;

	// Get the current HAPI_Viewport
	HAPI_Viewport H_View;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetViewport(
		FHoudiniEngine::Get().GetSession(), &H_View))
	{
		return false;
	}


	// Get Hapi viewport's PivotPosition, Offset and Quat,  w.r.t Houdini's coordinate and scale.
	FVector HapiViewportPivotPosition = FVector(H_View.position[0], H_View.position[1], H_View.position[2]);
	float HapiViewportOffset = H_View.offset;
	FQuat HapiViewportQuat = FQuat(H_View.rotationQuaternion[0], H_View.rotationQuaternion[1], H_View.rotationQuaternion[2], H_View.rotationQuaternion[3]);

	/* Check if the Houdini viewport has changed */
	if (SyncedHoudiniViewportPivotPosition.Equals(HapiViewportPivotPosition) &&
		SyncedHoudiniViewportQuat.Equals(HapiViewportQuat) &&
		SyncedHoudiniViewportOffset == HapiViewportOffset)
		{
			// Houdini viewport hasn't changed, nothing to do
			return false;
		}

	// Set zero value of offset when needed
	if (bOffsetZeroed)
	{
		ZeroOffsetValue = H_View.offset;
		bOffsetZeroed = false;
	}


	/* Translate the hapi camera transfrom to Unreal's representation system */

	// Get pivot point in UE's coordinate and scale
	FVector UnrealViewportPivotPosition = FVector(H_View.position[0], H_View.position[2], H_View.position[1]) * HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

	// HAPI bug? After we set the H_View, offset becomes a lot bigger when move the viewport just a little bit in Houdini.
	// But the pivot point doesn't change. Which caused UE viewport jumping far suddenly.
	// So we get rid of this problem by setting the first HAPI_offset value after syncing Houdini viewport as the base.
	
	// Get offset in UE's scale. The actual offset after 'zero out'
	float UnrealOffset = (H_View.offset - ZeroOffsetValue) * HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

	/* Calculate Quaternion in UE */
	// Rotate the resulting Quat around Z-axis by -90 degree.
	// Note that rotation is in general non-commutative ***
	FQuat UnrealQuat = FQuat(H_View.rotationQuaternion[0], H_View.rotationQuaternion[2], H_View.rotationQuaternion[1], -H_View.rotationQuaternion[3]);
	UnrealQuat = UnrealQuat * FQuat::MakeFromEuler(FVector(0.f, 0.f, -90.f));

	FVector UnrealBaseVector(1.f, 0.f, 0.f);   // Forward vector in Unreal viewport

	/* Get UE viewport location*/
	FVector UnrealViewPosition = - UnrealQuat.RotateVector(UnrealBaseVector) * UnrealOffset + UnrealViewportPivotPosition;

	/* Set the viewport's value */
	ViewportClient->SetViewLocation(UnrealViewPosition);
	ViewportClient->SetViewRotation(UnrealQuat.Rotator());

	// Invalidate the viewport
	ViewportClient->Invalidate();

	/* Update the synced viewport values */
	// We need to syced both the viewport representation values in Hapi and UE whenever the viewport is changed.
	// Since the 2 representations are multi-multi correspondence, the values could be changed even though the viewport is not changing.

	// Hapi values are in Houdini coordinate and scale
	SyncedHoudiniViewportPivotPosition = HapiViewportPivotPosition;
	SyncedHoudiniViewportQuat = HapiViewportQuat;
	SyncedHoudiniViewportOffset = HapiViewportOffset;

	SyncedUnrealViewportPosition = ViewportClient->GetViewLocation();
	SyncedUnrealViewportRotation = ViewportClient->GetViewRotation();
	SyncedUnrealViewportLookatPosition = ViewportClient->GetLookAtLocation();

	return true;
#endif
	
	return false;
}


void
FHoudiniEngineManager::DisableEditorAutoSave(const UObject* InObject)
{
#if WITH_EDITOR
	if (!IsValid(InObject))
		return;

	if (!GUnrealEd)
		return;
	
	if (AutosaveDisablerObjects.Contains(InObject))
		return;
	// Add the object to the set
	AutosaveDisablerObjects.Add(InObject);

	// Return if auto-saving has already been disabled by some other objects.
	if (AutosaveDisablerObjects.Num() > 1)
		return;

	// Disable auto-saving by setting min time till auto-save to max float value
	IPackageAutoSaver &AutoSaver = GUnrealEd->GetPackageAutoSaver();
	AutoSaver.ForceMinimumTimeTillAutoSave(TNumericLimits<float>::Max());
#endif
}


void
FHoudiniEngineManager::EnableEditorAutoSave(const UObject* InObject = nullptr)
{
#if WITH_EDITOR
	if (!GUnrealEd)
		return;

	if (!InObject)
	{
		// When the object is null, go through all Objects in the set,
		// and remove them if it has been deleted.
		if (AutosaveDisablerObjects.Num() <= 0)
			return;

		TSet<TWeakObjectPtr<const UObject>> ValidObjects;
		for (auto& CurObject : AutosaveDisablerObjects)
		{
			if (CurObject.IsValid())
			{
				ValidObjects.Add(CurObject);
			}
		}
		AutosaveDisablerObjects = MoveTemp(ValidObjects);
	}
	else
	{
		// Otherwise, remove the HAC from the set
		if (AutosaveDisablerObjects.Contains(InObject))
			AutosaveDisablerObjects.Remove(InObject);
	}

	if (AutosaveDisablerObjects.Num() > 0)
		return;

	// When no HAC disables cooking, reset min time till auto-save to default value, then reset the timer
	IPackageAutoSaver &AutoSaver = GUnrealEd->GetPackageAutoSaver();
	AutoSaver.ResetAutoSaveTimer();
#endif
}


