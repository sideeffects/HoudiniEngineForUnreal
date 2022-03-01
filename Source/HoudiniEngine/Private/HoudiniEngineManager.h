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
#include "TimerManager.h"
#include "Containers/Ticker.h"

//#include "HAL/Runnable.h"
//#include "HAL/RunnableThread.h"
//#include "Misc/SingleThreadRunnable.h"

#include "HoudiniPDGManager.h"

class UHoudiniAsset;
class UHoudiniAssetComponent;

struct FHoudiniEngineTaskInfo;
struct FGuid;

enum class EHoudiniAssetState : uint8;

class FHoudiniEngineManager
{
public:

	FHoudiniEngineManager();
	virtual ~FHoudiniEngineManager();

	void StartHoudiniTicking();
	void StopHoudiniTicking();
	bool IsTicking() const;
	
	bool Tick(float DeltaTime);

	// Updates / Process a component
	void ProcessComponent(UHoudiniAssetComponent* HAC);

	// Build UStaticMesh for all UHoudiniStaticMesh in a HAC.
	// This is fired by the OnRefinedMeshesTimerDelegate on a HAC
	void BuildStaticMeshesForAllHoudiniStaticMeshes(UHoudiniAssetComponent* HAC);

	void StartPDGCommandlet()
	{
		if (!IsPDGCommandletRunningOrConnected())
			PDGManager.CreateBGEOCommandletAndEndpoint();
	}

	void StopPDGCommandlet()
	{
		if (IsPDGCommandletRunningOrConnected())
			PDGManager.StopBGEOCommandletAndEndpoint();
	}

	bool IsPDGCommandletRunningOrConnected()
	{
		const EHoudiniBGEOCommandletStatus Status = PDGManager.UpdateAndGetBGEOCommandletStatus();
		return Status == EHoudiniBGEOCommandletStatus::Running || Status == EHoudiniBGEOCommandletStatus::Connected;
	}

	EHoudiniBGEOCommandletStatus GetPDGCommandletStatus() { return PDGManager.UpdateAndGetBGEOCommandletStatus(); }
	
	
protected:

	// Updates a given task's status
	// Returns true if the given task's status was properly found
	bool UpdateTaskStatus(
		FGuid& OutTaskGUID,
		FHoudiniEngineTaskInfo& OutTaskInfo);

	// Start a task to instantiate the given HoudiniAsset
	// Return true if the task was successfully created
	bool StartTaskAssetInstantiation(
		UHoudiniAsset* HoudiniAsset,
		const FString& DisplayName,
		FGuid& OutTaskGUID,
		FString& OutHAPIAssetName);

	// Updates progress of the instantiation task
	// Returns true if a state change should be made
	bool UpdateInstantiating(
		UHoudiniAssetComponent* HAC,
		EHoudiniAssetState& NewState);

	// Start a task to instantiate the Houdini Asset with the given node Id
	// Returns true if the task was successfully created
	bool StartTaskAssetCooking(
		const HAPI_NodeId& AssetId,
		const TArray<HAPI_NodeId>& NodeIdsToCook,
		const FString& DisplayName,
		bool bUseOutputNodes,
		bool bOutputTemplateGeos,
		FGuid& OutTaskGUID);

	// Updates progress of the cooking task
	// Returns true if a state change should be made
	bool UpdateCooking(
		UHoudiniAssetComponent* HAC,
		EHoudiniAssetState& NewState);

	// Called to update template components. 
	bool PreCookTemplate(UHoudiniAssetComponent* HAC);

	// Called to update all houdini nodes/params/inputs before a cook has started
	bool PreCook(UHoudiniAssetComponent* HAC);

	// Called after a cook has finished 
	bool PostCook(
		UHoudiniAssetComponent* HAC,
		const bool& bSuccess,
		const HAPI_NodeId& TaskAssetId);

	bool StartTaskAssetProcess(UHoudiniAssetComponent* HAC);

	bool UpdateProcess(UHoudiniAssetComponent* HAC);

	// Starts a rebuild task (delete then re instantiate)
	// The NodeID should be invalidated after a successful call
	bool StartTaskAssetRebuild(
		const HAPI_NodeId& InAssetId,
		FGuid& OutTaskGUID);

	// Starts a node delete task
	// The NodeID should be invalidated after a successful call
	bool StartTaskAssetDelete(
		const HAPI_NodeId& InAssetId,
		FGuid& OutTaskGUID,
		bool bShouldDeleteParent);

	bool IsCookingEnabledForHoudiniAsset(UHoudiniAssetComponent* HAC);

	// Syncs the houdini viewport to Unreal's viewport
	// Returns true if the Houdini viewport has been modified
	bool SyncHoudiniViewportToUnreal();

	// Syncs the unreal viewport to Houdini's viewport
	// Returns true if the Unreal viewport has been modified
	bool SyncUnrealViewportToHoudini();

	// Disable auto save by setting min time till auto save to the max value
	void DisableEditorAutoSave(const UHoudiniAssetComponent* HAC);

	void EnableEditorAutoSave(const UHoudiniAssetComponent* HAC);

	// Automatically try to start the First HE session if needed
	void AutoStartFirstSessionIfNeeded(UHoudiniAssetComponent* InCurrentHAC);

private:

	// Ticker handle, used for processing HAC.
	FTSTicker::FDelegateHandle TickerHandle;

	// Current position in the array
	uint32 CurrentIndex;

	// Current number of components in the array
	uint32 ComponentCount;

	// Stopping flag. 
	// Indicates that we should stop ticking asap
	bool bMustStopTicking;

	// The PDG Manager, handles all registered PDG Asset Links
	FHoudiniPDGManager PDGManager;

	// For ViewportSync: The camera transform that Hapi and Unreal currently agree with.
	FVector SyncedHoudiniViewportPivotPosition;
	FQuat SyncedHoudiniViewportQuat;
	float SyncedHoudiniViewportOffset;

	FVector SyncedUnrealViewportPosition;
	FRotator SyncedUnrealViewportRotation;
	FVector SyncedUnrealViewportLookatPosition;

	// We need these two variables to get rid of a HAPI bug
	// Note: When sync Houdini to UE, we set the pivot position to be the view position, and offset to be 0.0.
	//       but when we switch to control viewport in Houdini, HAPI returns an H_View with a large offset, but pivot unchanged.
	//       so, we need these two variables to 'zero out' offset.
	float ZeroOffsetValue;		// in HAPI scale
	bool bOffsetZeroed;

	// Indicates which HACs disable auto-saving
	TSet<TWeakObjectPtr<const UHoudiniAssetComponent>> DisableAutoSavingHACs;
};