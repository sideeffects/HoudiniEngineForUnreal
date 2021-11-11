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

#include "Framework/Commands/Commands.h"
#include "Misc/SlowTask.h"
#include "Delegates/IDelegateInstance.h"
#include "HoudiniEngineRuntimeCommon.h"

class UHoudiniAssetComponent;
class AHoudiniAssetActor;
struct FSlowTask;

// Class containing commands for Houdini Engine actions
class FHoudiniEngineCommands : public TCommands<FHoudiniEngineCommands>
{
public:
	// Multi-cast delegate type for broadcasting when proxy mesh refinement of a HAC is complete. 
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHoudiniProxyMeshesRefinedDelegate, UHoudiniAssetComponent* const, const EHoudiniProxyRefineResult);
	
	FHoudiniEngineCommands();

	// TCommand<> interface
	virtual void RegisterCommands() override;

public:

	// Menu action called to save a HIP file.
	static void SaveHIPFile();

	// Menu action called to report a bug.
	static void ReportBug();

	// Menu action called to open the current scene in Houdini.
	static void OpenInHoudini();

	// Menu action called to clean up all unused files in the cook temp folder
	static void CleanUpTempFolder();

	// Menu action to bake/replace all current Houdini Assets with blueprints
	static void BakeAllAssets();

	// Helper function for baking/replacing the current select Houdini Assets with blueprints
	static void BakeSelection();

	// Helper function for restarting the current Houdini Engine session.
	static void RestartSession();

	// Menu action to pause cooking for all Houdini Assets 
	static void PauseAssetCooking();

	// Helper delegate used to get the current state of PauseAssetCooking.
	static bool IsAssetCookingPaused();

	// Helper function for recooking all assets in the current level
	static void RecookAllAssets();

	// Helper function for rebuilding all assets in the current level
	static void RebuildAllAssets();

	// Helper function for recooking selected assets
	static void RecookSelection();

	// Helper function for rebuilding selected assets
	static void RebuildSelection();

	// Helper function for rebuilding selected assets
	static void RecentreSelection();

	// Helper function for starting Houdini in Sesion Sync mode
	static void OpenSessionSync();

	// Helper function for closing the current Houdini Sesion Sync
	static void CloseSessionSync();
	
	// returns true if the current HE session is valid
	static bool IsSessionValid();

	// Returns true if the current Session Sync process is still running
	static bool IsSessionSyncProcessValid();

	static int32 GetViewportSync();

	static void SetViewportSync(const int32& ViewportSync);

	static void CreateSession();

	static void ConnectSession();

	static void StopSession();

	static void ShowInstallInfo();

	static void ShowPluginSettings();

	static void OnlineDocumentation();

	static void OnlineForum();

	// Helper function for building static meshes for all assets using HoudiniStaticMesh
	// If bSilent is false, show a progress dialog.
	// If bRefineAll is true, then all components with HoudiniStaticMesh meshes will be
	// refined to UStaticMesh. Otherwise, bOnPreSaveWorld and bOnPrePIEBeginPlay is checked
	// against the settings of the component to determine if refinement should take place.
	// If bOnPreSaveWorld is true, then OnPreSaveWorld should be the World that is being saved. In
	// that case, only proxy meshes attached to components from that world will be refined.
	static EHoudiniProxyRefineRequestResult RefineHoudiniProxyMeshesToStaticMeshes(bool bOnlySelectedActors, bool bSilent=false, bool bRefineAll=true, bool bOnPreSaveWorld=false, UWorld *PreSaveWorld=nullptr, bool bOnPrePIEBeginPlay=false);

	// Refine all proxy meshes on UHoudiniAssetCompoments of InActorsToRefine.
	static EHoudiniProxyRefineRequestResult RefineHoudiniProxyMeshActorArrayToStaticMeshes(const TArray<AHoudiniAssetActor*>& InActorsToRefine, bool bSilent=false);

	static void StartPDGCommandlet();

	static void StopPDGCommandlet();

	static bool IsPDGCommandletRunningOrConnected();

	// Returns true if the commandlet is enabled in the settings
	static bool IsPDGCommandletEnabled();

	// Set the bPDGAsyncCommandletImportEnabled in the settings
	static bool SetPDGCommandletEnabled(bool InEnabled);	

	static FDelegateHandle& GetOnPostSaveWorldRefineProxyMeshesHandle() { return OnPostSaveWorldRefineProxyMeshesHandle; }

	static FOnHoudiniProxyMeshesRefinedDelegate& GetOnHoudiniProxyMeshesRefinedDelegate() { return OnHoudiniProxyMeshesRefinedDelegate; }

public:

	// UI Action to create a Houdini Engine Session 
	TSharedPtr<FUICommandInfo> _CreateSession;
	// UI Action to connect to a Houdini Engine Session 
	TSharedPtr<FUICommandInfo> _ConnectSession;
	// UI Action to stop  the current Houdini Engine Session 
	TSharedPtr<FUICommandInfo> _StopSession;
	// UI Action to restart the current Houdini Engine Session 
	TSharedPtr<FUICommandInfo> _RestartSession;
	// UI Action to open Houdini Session Sync
	TSharedPtr<FUICommandInfo> _OpenSessionSync;
	// UI Action to open Houdini Session Sync
	TSharedPtr<FUICommandInfo> _CloseSessionSync;

	// UI Action to disable viewport sync
	TSharedPtr<FUICommandInfo> _ViewportSyncNone;
	// UI Action to enable unreal viewport sync
	TSharedPtr<FUICommandInfo> _ViewportSyncUnreal;
	// UI Action to enable houdini viewport sync
	TSharedPtr<FUICommandInfo> _ViewportSyncHoudini;
	// UI Action to enable bidirectionnal viewport sync
	TSharedPtr<FUICommandInfo> _ViewportSyncBoth;

	//
	TSharedPtr<FUICommandInfo> _InstallInfo;
	//
	TSharedPtr<FUICommandInfo> _PluginSettings;

	// Menu action called to open the current scene in Houdini.
	TSharedPtr<FUICommandInfo> _OpenInHoudini;
	// Menu action called to save a HIP file.
	TSharedPtr<FUICommandInfo> _SaveHIPFile;
	// Menu action called to clean up all unused files in the cook temp folder
	TSharedPtr<FUICommandInfo> _CleanUpTempFolder;

	//
	TSharedPtr<FUICommandInfo> _OnlineDoc;
	//
	TSharedPtr<FUICommandInfo> _OnlineForum;
	// Menu action called to report a bug.
	TSharedPtr<FUICommandInfo> _ReportBug;

	// UI Action to recook all HDA
	TSharedPtr<FUICommandInfo> _CookAll;
	// UI Action to recook the current world selection 
	TSharedPtr<FUICommandInfo> _CookSelected;
	// Menu action to bake/replace all current Houdini Assets with blueprints
	TSharedPtr<FUICommandInfo> _BakeAll;
	// UI Action to bake and replace the current world selection 
	TSharedPtr<FUICommandInfo> _BakeSelected;
	// UI Action to rebuild all HDA
	TSharedPtr<FUICommandInfo> _RebuildAll;
	// UI Action to rebuild the current world selection 
	TSharedPtr<FUICommandInfo> _RebuildSelected;
	// UI Action for building static meshes for all assets using HoudiniStaticMesh
	TSharedPtr<FUICommandInfo> _RefineAll;
	// UI Action for building static meshes for selected assets using HoudiniStaticMesh
	TSharedPtr<FUICommandInfo> _RefineSelected;
	// Menu action to pause cooking for all Houdini Assets 
	TSharedPtr<FUICommandInfo> _PauseAssetCooking;

	// UI Action to recentre the current selection 
	TSharedPtr<FUICommandInfo> _RecentreSelected;

	// Start PDG/BGEO commandlet
	TSharedPtr<FUICommandInfo> _StartPDGCommandlet;
	// Stop PDG/BGEO commandlet
	TSharedPtr<FUICommandInfo> _StopPDGCommandlet;
	// Is PDG/BGEO commandlet enabled
	TSharedPtr<FUICommandInfo> _IsPDGCommandletEnabled;

protected:

	// Triage a HoudiniAssetComponent with UHoudiniStaticMesh as needing cooking or if a UStaticMesh can be immediately built
	static void TriageHoudiniAssetComponentsForProxyMeshRefinement(UHoudiniAssetComponent* InHAC, bool bRefineAll, bool bOnPreSaveWorld, UWorld *OnPreSaveWorld, bool bOnPreBeginPIE, TArray<UHoudiniAssetComponent*> &OutToRefine, TArray<UHoudiniAssetComponent*> &OutToCook, TArray<UHoudiniAssetComponent*> &OutSkipped);

	static EHoudiniProxyRefineRequestResult RefineTriagedHoudiniProxyMesehesToStaticMeshes(
		const TArray<UHoudiniAssetComponent*>& InComponentsToRefine,
		const TArray<UHoudiniAssetComponent*>& InComponentsToCook,
		const TArray<UHoudiniAssetComponent*>& InSkippedComponents,
		bool bInSilent=false,
		bool bInRefineAll=true,
		bool bInOnPreSaveWorld=false,
		UWorld* InOnPreSaveWorld=nullptr,
		bool bInOnPrePIEBeginPlay=false);

	// Called in a background thread by RefineHoudiniProxyMeshesToStaticMeshes when some components need to be cooked to generate UStaticMeshes. Checks and waits for
	// cooking of each component to complete, and then calls RefineHoudiniProxyMeshesToStaticMeshesNotifyDone on the main thread.
	static void RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(const TArray<UHoudiniAssetComponent*> &InComponentsToCook, TSharedPtr<FSlowTask, ESPMode::ThreadSafe> InTaskProgress, const uint32 InNumSkippedComponents, bool bInOnPreSaveWorld, UWorld *InOnPreSaveWorld, const TArray<UHoudiniAssetComponent*> &InSuccessfulComponents, const TArray<UHoudiniAssetComponent*> &InFailedComponents, const TArray<UHoudiniAssetComponent*> &InSkippedComponents);

	// Display a notification / end/close progress dialog, when refining mesh proxies to static meshes is complete
	static void RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(const uint32 InNumTotalComponents, FSlowTask* const InTaskProgress, const bool bCancelled, const bool bOnPreSaveWorld, UWorld* const InOnPreSaveWorld, const TArray<UHoudiniAssetComponent*> &InSuccessfulComponents, const TArray<UHoudiniAssetComponent*> &InFailedComponents, const TArray<UHoudiniAssetComponent*> &InSkippedComponents);

	// Handle OnPostSaveWorld for refining proxy meshes: this saves all the dirty UPackages of the UStaticMeshes that were created during RefineHoudiniProxyMeshesToStaticMeshes
	// if it was called as a result of a PreSaveWorld.
	static void RefineProxyMeshesHandleOnPostSaveWorld(const TArray<UHoudiniAssetComponent*> &InSuccessfulComponents, uint32 InSaveFlags, UWorld* InWorld, bool bInSuccess);

	static void SetAllowPlayInEditorRefinement(const TArray<UHoudiniAssetComponent*>& InComponents, bool bEnabled);

	// Helper function used to indicate to all HAC that they need to be instantiated in the new HE session
	// Needs to be call after starting/restarting/connecting/session syncing a HE session..
	static void MarkAllHACsAsNeedInstantiation();

	// Delegate that is set up to refined proxy meshes post save world (it removes itself afterwards)
	static FDelegateHandle OnPostSaveWorldRefineProxyMeshesHandle;

	// Delegate for broadcasting when proxy mesh refinement of a HAC's output is complete.
	static FOnHoudiniProxyMeshesRefinedDelegate OnHoudiniProxyMeshesRefinedDelegate; 
};

