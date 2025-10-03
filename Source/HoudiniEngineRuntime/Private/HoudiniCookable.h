/*
* Copyright (c) <2025> Side Effects Software Inc.
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

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntimeCommon.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetStateTypes.h"
#include "IHoudiniAssetStateEvents.h"

#include "Delegates/DelegateCombinations.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
	#include "LevelInstance/LevelInstanceInterface.h"
#endif

#include "HoudiniCookable.generated.h"

class UHoudiniAsset;
class UHoudiniHandleComponent;
class UHoudiniInput;
class UHoudiniOutput;
class UHoudiniPDGAssetLink;
class UHoudiniParameter;

UCLASS()
class HOUDINIENGINERUNTIME_API UCookableHoudiniAssetData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	UCookableHoudiniAssetData();

	// Houdini Asset associated with this component.			
	UPROPERTY(Category = HoudiniAsset, EditAnywhere)// BlueprintSetter = SetHoudiniAsset, BlueprintReadWrite, )
	TObjectPtr<UHoudiniAsset> HoudiniAsset;

	// Subasset index
	UPROPERTY()
	uint32 SubAssetIndex;

	// The asset name of the selected asset inside the asset library
	UPROPERTY(DuplicateTransient)
	FString HapiAssetName;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UCookableParameterData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	UCookableParameterData();

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniParameter>> Parameters;

	// Used to store the current state of parameters 
	// This allows fast - batch setting of parameters (upon rebuild/load)
	UPROPERTY()
	TArray<int8> ParameterPresetBuffer;

	// Automatically cook when a parameter is changed
	UPROPERTY()
	bool bCookOnParameterChange;

	// Indicates that the parameter state (excluding values) on the HAC and the instantiated node needs to be synced.
	// The most common use for this would be a newly instantiated HDA that has only a default parameter interface
	// from its asset definition, and needs to sync pre-cook.
	UPROPERTY(DuplicateTransient)
	bool bParameterDefinitionUpdateNeeded;

	// Try to find one of our parameter that matches another (name, type, size and enabled)
	UHoudiniParameter* FindMatchingParameter(UHoudiniParameter* InOtherParam);
};


UCLASS()
class HOUDINIENGINERUNTIME_API UCookableInputData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	UCookableInputData();

	// Store data for a cookable's inputs
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniInput>> Inputs;

	// Automatically cook when an input is changed
	UPROPERTY()
	bool bCookOnInputChange; // bCookOnParameterChange

	UPROPERTY()
	bool bCookOnCookableInputCook;

	// TODO: Cookable Move to output data?
	// List of dependent downstream Cookables that have us as an asset input
	UPROPERTY(DuplicateTransient)
	TSet<TObjectPtr<UHoudiniCookable>> DownstreamCookables;

	// Accessors
	int32 GetNumInputs() const { return Inputs.Num(); };
	UHoudiniInput* GetInputAt(const int32& Idx) { return Inputs.IsValidIndex(Idx) ? Inputs[Idx] : nullptr; };

	//
	//bool NeedUpdateInputs() const;
	
	//
	bool NeedsToWaitForInputHoudiniAssets();
};


UCLASS()
class HOUDINIENGINERUNTIME_API UCookableOutputData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	UCookableOutputData();

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniOutput>> Outputs;

	// Any actors that aren't explicitly tracked by output objects
	// should be registered here so that they can be cleaned up.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> UntrackedOutputs;
	
	// Temporary cook folder
	UPROPERTY()
	FDirectoryPath TemporaryCookFolder;

	// Enabling this will prevent producing any output after cooking.
	UPROPERTY()
	bool bOutputless;

	// Enabling this allows scene components to be produced. Used to stop scene components
	// being generated for meshes. Possibly other types in the future.
	UPROPERTY()
	bool bCreateSceneComponents;

	// Enabling this will allow outputing the asset's templated geos
	UPROPERTY()
	bool bOutputTemplateGeos;

	// Enabling this will allow outputing using output nodes
	// Disabling it will either fall back to display flag node (legacy workflow)
	UPROPERTY()
	bool bUseOutputNodes;

	// Whether or not to support multiple mesh outputs on one HDA output. This is currently in Alpha  testing.
	UPROPERTY(Category = "HoudiniMeshGeneration", EditAnywhere, meta = (DisplayPriority = 0))
	bool bSplitMeshSupport = false;
		
	UPROPERTY()
	bool bEnableCurveEditing;

	// Generation properties for the Static Meshes
	UPROPERTY(Category = "HoudiniMeshGeneration", EditAnywhere, meta = (DisplayPriority = 1)/*, meta = (ShowOnlyInnerProperties)*/)
	FHoudiniStaticMeshGenerationProperties StaticMeshGenerationProperties;

	// Build Settings to be used when generating the Static Meshes for this Houdini Asset
	UPROPERTY(Category = "HoudiniMeshGeneration", EditAnywhere, meta = (DisplayPriority = 2))
	FMeshBuildSettings StaticMeshBuildSettings;

	UPROPERTY()
	bool bLandscapeUseTempLayers;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UCookableBakingData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostBakeDelegate, UHoudiniCookable*, bool);

public:

	FOnPostBakeDelegate& GetOnPostBakeDelegate() { return OnPostBakeDelegate; };

	//-----------------------------------
	// BAKE

	// Previously baked outputs
	TArray<FHoudiniBakedOutput> BakedOutputs;

	// Bake Options
	UPROPERTY()
	EHoudiniEngineBakeOption HoudiniEngineBakeOption;

	// Folder used for baking this asset's outputs (unless set by prim/detail attribute on the output). Falls back to
	// the default from the plugin settings if not set.
	UPROPERTY()
	FDirectoryPath BakeFolder;

	// If true, bake the asset after its next cook.
	UPROPERTY(DuplicateTransient)
	EHoudiniBakeAfterNextCook BakeAfterNextCook;

	// If true, then after a successful bake, outputs will be cleared and removed.
	UPROPERTY()
	bool bRemoveOutputAfterBake;

	// If true, recenter baked actors to their bounding box center after bake
	UPROPERTY()
	bool bRecenterBakedActors;

	// If true, replace the previously baked output (if any) instead of creating new objects
	UPROPERTY()
	bool bReplacePreviousBake;

	UPROPERTY()
	EHoudiniEngineActorBakeOption ActorBakeOption;

	// Delegate to broadcast after baking the HAC. Not called when just baking individual outputs directly.
	// Arguments are (HoudiniAssetComponent* HAC, bool bIsSuccessful)
	FOnPostBakeDelegate OnPostBakeDelegate;

};

UCLASS()
class HOUDINIENGINERUNTIME_API UCookableProxyData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	// Declare the delegate that is broadcast when RefineMeshesTimer fires
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRefineMeshesTimerDelegate, UHoudiniCookable*);

	// Called by RefineMeshesTimer when the timer is triggered.
	FOnRefineMeshesTimerDelegate& GetOnRefineMeshesTimerDelegate() { return OnRefineMeshesTimerDelegate; }

	//-----------------------------------
	// PROXY MESH

	// If true, don't build a proxy mesh next cook (regardless of global or override settings),
	// instead build the UStaticMesh directly (if applicable for the output types).
	UPROPERTY(DuplicateTransient)
	bool bNoProxyMeshNextCookRequested;

	// Override the global fast proxy mesh settings
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere/*, meta = (DisplayAfter = "StaticMeshGenerationProperties")*/)
	bool bOverrideGlobalProxyStaticMeshSettings;

	// For StaticMesh outputs: should a fast proxy be created first?
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName = "Enable Proxy Static Mesh", EditCondition = "bOverrideGlobalProxyStaticMeshSettings"))
	bool bEnableProxyStaticMeshOverride;

	// If fast proxy meshes are being created, must it be baked as a StaticMesh after a period of no updates?
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName = "Refine Proxy Static Meshes After a Timeout", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementByTimerOverride;

	// If the option to automatically refine the proxy mesh via a timer has been selected, this controls the timeout in seconds.
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName = "Proxy Mesh Auto Refine Timeout Seconds", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementByTimerOverride"))
	float ProxyMeshAutoRefineTimeoutSecondsOverride;

	// Automatically refine proxy meshes to UStaticMesh before the map is saved
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName = "Refine Proxy Static Meshes When Saving a Map", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;

	// Automatically refine proxy meshes to UStaticMesh before starting a play in editor session
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName = "Refine Proxy Static Meshes On PIE", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;

	UPROPERTY(Transient, DuplicateTransient)
	bool bAllowPlayInEditorRefinement;

	// Timer that is used to trigger creation of UStaticMesh for all mesh outputs
	// that still have UHoudiniStaticMeshes. The timer is cleared on PreCook and reset
	// at the end of the PostCook.
	UPROPERTY()
	FTimerHandle RefineMeshesTimer;

	// Delegate that is used to broadcast when RefineMeshesTimer fires
	FOnRefineMeshesTimerDelegate OnRefineMeshesTimerDelegate;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UCookableComponentData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	UCookableComponentData();

	UPROPERTY(VisibleAnywhere, Category = "Houdini Cookable")
	TWeakObjectPtr<USceneComponent> Component; 
	
	// Used to compare transform changes and whether we need to
	// send transform updates to Houdini.
	UPROPERTY(DuplicateTransient)
	FTransform LastComponentTransform;

	UPROPERTY(Transient, DuplicateTransient)
	bool bHasComponentTransformChanged;

	// Enables uploading of transformation changes back to Houdini Engine.
	UPROPERTY()
	bool bUploadTransformsToHoudiniEngine;

	// Transform changes automatically trigger cooks.
	UPROPERTY()
	bool bCookOnTransformChange;

	// Handles found on this cookable
	UPROPERTY()
	TArray<TObjectPtr<UHoudiniHandleComponent>> HandleComponents;

	// The last timestamp this cookable's component received a session sync update ping
	// used to limit the frequency at which we ping HDAs for session sync updates
	UPROPERTY(Transient)
	double LastLiveSyncPingTime;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UCookablePDGData : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniCookable;

public:

	UCookablePDGData();

	void SetPDGAssetLink(UHoudiniPDGAssetLink* InPDGAssetLink);

	UPROPERTY()
	TObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink;

	UPROPERTY(DuplicateTransient, Transient)
	bool bIsPDGAssetLinkInitialized;
};





UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniCookable : public UObject, public IHoudiniAssetStateEvents
{
	GENERATED_UCLASS_BODY()

	// Declare translators as friend so they can easily directly modify
	// Inputs, outputs and parameters
	friend class FHoudiniEngineManager;
	friend struct FHoudiniEngineUtils;
	friend struct FHoudiniOutputTranslator;
	//friend struct FHoudiniInputTranslator;
	//friend struct FHoudiniSplineTranslator;
	friend struct FHoudiniParameterTranslator;
	//friend struct FHoudiniPDGManager;
	friend struct FHoudiniHandleTranslator;
	friend class UHoudiniAssetComponent;

	// Delegate for when EHoudiniAssetState changes from InFromState to InToState on a HoudiniCookable (InHC)
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCookableStateChangeDelegate, UHoudiniCookable*, const EHoudiniAssetState, const EHoudiniAssetState);
	// Delegate for when EHoudiniAssetState changes from InFromState to InToState on a Houdini Asset Component (InHAC).
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAssetStateChangeDelegate, UHoudiniCookable*, const EHoudiniAssetState, const EHoudiniAssetState);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreInstantiationDelegate, UHoudiniCookable*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreCookDelegate, UHoudiniCookable*);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostCookDelegate, UHoudiniCookable*, bool);	
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreOutputProcessingDelegate, UHoudiniCookable*, bool);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostOutputProcessingDelegate, UHoudiniCookable*, bool);
	

public:

	virtual ~UHoudiniCookable();

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------

	int32 GetNodeId() const { return NodeId; };
	void SetNodeId(int InNodeId) { NodeId = InNodeId;  }

	EHoudiniAssetState GetCurrentState() const { return CurrentState; };
	EHoudiniAssetStateResult GetCurrentStateResult() const { return CurrentStateResult; };
	//virtual FString GetHoudiniAssetName() const;

	// Feature accessors
	UCookableHoudiniAssetData* GetHoudiniAssetData() { return IsHoudiniAssetSupported() ? HoudiniAssetData : nullptr; };
	UCookableParameterData* GetParameterData() { return IsParameterSupported() ? ParameterData : nullptr; };
	UCookableInputData* GetInputData() { return IsInputSupported() ? InputData : nullptr; };
	UCookableOutputData* GetOutputData() { return IsOutputSupported() ? OutputData : nullptr;};
	UCookableComponentData* GetComponentData() { return IsComponentSupported() ? ComponentData : nullptr; };
	UCookablePDGData* GetPDGData() { return IsPDGSupported() ? PDGData : nullptr; };
	UCookableBakingData* GetBakingData() { return IsBakingSupported() ? BakingData : nullptr; };
	UCookableProxyData* GetProxyData() { return IsProxySupported() ? ProxyData : nullptr; };

	bool SetParameterData(UCookableParameterData * ParameterData);
	bool SetInputData(UCookableInputData*);

	USceneComponent* GetComponent() const;
	AActor* GetOwner() const;
	UWorld* GetWorld() const;
	ULevel* GetLevel() const;
	bool IsOwnerSelected() const;
	UHoudiniAsset* GetHoudiniAsset();
	UHoudiniPDGAssetLink* GetPDGAssetLink();
	FString GetHoudiniAssetName() const;

	FGuid& GetHapiGUID();
	FString GetHapiAssetName() const;
	FGuid GetCookableGUID() const;

	bool IsCookingEnabled() const { return bEnableCooking; };
	bool HasBeenLoaded() const { return bHasBeenLoaded; };
	bool HasBeenDuplicated() const { return bHasBeenDuplicated; };
	bool HasRecookBeenRequested() const { return bRecookRequested; };
	bool HasRebuildBeenRequested() const { return bRebuildRequested; };
	bool IsInstantiatingOrCooking() const;

	bool GetCookOnParameterChange() const;
	bool GetCookOnTransformChange() const;
	bool GetCookOnInputChange() const;
	bool GetCookOnCookableInputCook() const;
	bool IsOutputless() const;
	bool GetUseOutputNodes() const;
	bool GetOutputTemplateGeos() const;
	bool GetUploadTransformsToHoudiniEngine() const;
	bool GetLandscapeUseTempLayers() const;
	bool GetEnableCurveEditing() const;
	bool GetSplitMeshSupport() const;

	// Returns the last cached transform for the component - identity if not supported
	FTransform GetLastComponentTransform() const;

	// Returns the current component transform - identity if not supported
	FTransform GetComponentTransform() const;

	FHoudiniStaticMeshGenerationProperties GetStaticMeshGenerationProperties() const;
	FMeshBuildSettings GetStaticMeshBuildSettings() const;

	FHoudiniStaticMeshGenerationProperties& GetStaticMeshGenerationProperties();
	FMeshBuildSettings& GetStaticMeshBuildSettings();

	// Feature data accessors
	int32 GetNumInputs() const { return IsInputSupported() ? InputData->Inputs.Num() : 0; };
	int32 GetNumOutputs() const { return IsOutputSupported() ? OutputData->Outputs.Num() : 0; };
	int32 GetNumParameters() const { return IsParameterSupported() ? ParameterData->Parameters.Num() : 0; };
	int32 GetNumHandles() const { return IsComponentSupported() ? ComponentData->HandleComponents.Num() : 0; };

	UHoudiniInput* GetInputAt(const int32& Idx) { return IsInputSupported() ? (InputData->Inputs.IsValidIndex(Idx) ? InputData->Inputs[Idx] : nullptr) : nullptr; };
	UHoudiniOutput* GetOutputAt(const int32& Idx) { return IsOutputSupported() ? (OutputData->Outputs.IsValidIndex(Idx) ? OutputData->Outputs[Idx] : nullptr) : nullptr;};
	UHoudiniParameter* GetParameterAt(const int32& Idx) { return IsParameterSupported() ? (ParameterData->Parameters.IsValidIndex(Idx) ? ParameterData->Parameters[Idx] : nullptr) : nullptr;};
	UHoudiniHandleComponent* GetHandleComponentAt(const int32& Idx) { return IsComponentSupported() ? (ComponentData->HandleComponents.IsValidIndex(Idx) ? ComponentData->HandleComponents[Idx] : nullptr) : nullptr; };

	// Try to find one of our parameter that matches another (name, type, size and enabled)
	UHoudiniParameter* FindMatchingParameter(UHoudiniParameter* InOtherParam);
	// Try to find one of our input that matches another one (name, isobjpath, index / parmId)
	UHoudiniInput* FindMatchingInput(UHoudiniInput* InOtherInput);
	// Try to find one of our handle that matches another one (name and handle type)
	UHoudiniHandleComponent* FindMatchingHandle(UHoudiniHandleComponent* InOtherHandle);
	// Finds a parameter by name
	UHoudiniParameter* FindParameterByName(const FString& InParamName);

	// Output bake folder accessor
	FDirectoryPath GetBakeFolder() const;
	// Output temp folder accessor
	FDirectoryPath GetTemporaryCookFolder() const;
	// Returns the TemporaryCookFolder, if it is not empty. Otherwise returns the plugin default temporary
	// cook folder. This function does not take the unreal_temp_folder attribute into account.
	FString GetTemporaryCookFolderOrDefault() const;
	// Returns the BakeFolder, if it is not empty. Otherwise returns the plugin default bake folder. This
	// function does not take the unreal_bake_folder attribute into account.
	FString GetBakeFolderOrDefault() const;

	// Returns true if a parameter definition update (excluding values) is needed.
	bool IsParameterDefinitionUpdateNeeded() const { return IsParameterSupported() ? ParameterData->bParameterDefinitionUpdateNeeded : false; };

	FString GetDisplayName() const;

	// Returns true if this cookable should try to start a session
	virtual bool ShouldTryToStartFirstSession() const;

	// Needed for BP support
	bool IsFullyLoaded() const { return bFullyLoaded; };
	// Whether this component is currently open in a Blueprint editor. This
	// method is overridden by HoudiniAssetBlueprintComponent.
	virtual bool HasOpenEditor() const { return false; };

	int32 GetCookCount() const { return CookCount; };

	bool WasLastCookSuccessful() const;

	// TODO COOKABLE: Move to component?
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
	ILevelInstanceInterface* GetLevelInstance() const;
#endif

	// Indicates if the cookable needs to be updated
	bool NeedUpdate() const;

	// Derived blueprint based components will check whether the template component contains updates that needs to processed.

	// Indicates if the cookable's parameters need an update
	bool NeedUpdateParameters() const;

	// Indicates if the cookable's inputs need an update
	bool NeedUpdateInputs() const;

	// Indicates if any of cookable's outputs need an update
	bool NeedUpdateOutputs() const;

	TArray<TObjectPtr<UHoudiniParameter>>& GetParameters();
	const TArray<TObjectPtr<UHoudiniParameter>>& GetParameters() const;
	TArray<TObjectPtr<UHoudiniInput>>& GetInputs();
	const TArray<TObjectPtr<UHoudiniInput>>& GetInputs() const;
	TArray<TObjectPtr<UHoudiniOutput>>& GetOutputs();
	TArray<TObjectPtr<UHoudiniHandleComponent>>& GetHandleComponents();

	void GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const;
	bool HasAnyOutputComponent() const;

	TArray<int32> GetNodeIdsToCook() const;
	TMap<int32, int32> GetNodesToCookCookCounts() const;

	TArray<FHoudiniBakedOutput>& GetBakedOutputs();
	const TArray<FHoudiniBakedOutput>& GetBakedOutputs() const;
	EHoudiniEngineBakeOption GetHoudiniEngineBakeOption() const;
	bool GetReplacePreviousBake() const;
	bool GetRemoveOutputAfterBake() const;
	bool GetRecenterBakedActors() const;

	// Proxies
	bool IsOverrideGlobalProxyStaticMeshSettings() const;
	bool IsProxyStaticMeshEnabled() const;
	bool IsProxyStaticMeshRefinementByTimerEnabled() const;
	float GetProxyMeshAutoRefineTimeoutSeconds() const;
	bool IsProxyStaticMeshRefinementOnPreSaveWorldEnabled() const;
	bool IsProxyStaticMeshRefinementOnPreBeginPIEEnabled() const;
	bool HasNoProxyMeshNextCookBeenRequested() const;
	bool HasAnyCurrentProxyOutput() const;
	bool HasAnyProxyOutput() const;
	bool IsHoudiniCookedDataAvailable(bool& bOutNeedsRebuildOrDelete, bool& bOutInvalidState) const;
	bool IsBakeAfterNextCookEnabled() const;
	EHoudiniBakeAfterNextCook GetBakeAfterNextCook() const;
	bool IsPlayInEditorRefinementAllowed() const;
	EHoudiniEngineActorBakeOption GetActorBakeOption() const;

	FName GetAssetEditorId() const { return AssetEditorId; };

	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------
 
	// Set asset state	
	void SetCurrentState(EHoudiniAssetState InNewState);
	// Set asset state	
	void SetCurrentStateResult(EHoudiniAssetStateResult InResult);

	void UpdateDormantStatus();

	// Cleans up children components and PDG asset link (if any) after a duplication
	void UpdatePostDuplicate();

	// Clear nodes to cook. This will also clear their cook counts.
	void ClearNodesToCook(); 
	void ClearOutputNodes() { return ClearNodesToCook(); };

	void SetNodeIdsToCook(const TArray<int32>& InNodeIds);
	
	// Clear/disable the RefineMeshesTimer.
	void ClearRefineMeshesTimer();

	void SetHasComponentTransformChanged(bool InHasChanged);

	void MarkAsNeedCook();
	void MarkAsNeedRebuild();
	void MarkAsNeedInstantiation();

	// TODO COOKABLE: protect me!
	void MarkAsNeedRecookOrRebuild(bool bDoRebuild);

	void PreventAutoUpdates();

	void OnDestroy(bool bDestroyingHierarchy);
	void OnSessionConnected();
	void OnHoudiniAssetChanged();

	void SetComponent(USceneComponent* InComp);
	void SetHoudiniAssetComponent(UHoudiniAssetComponent* InComp);
	void SetHoudiniAsset(UHoudiniAsset* InHAsset);

	bool SetTemporaryCookFolderPath(const FString& NewPath);
	bool SetBakeFolderPath(const FString& NewPath);
	bool SetTemporaryCookFolder(const FDirectoryPath& InPath);
	bool SetBakeFolder(const FDirectoryPath& InPath);

	void SetCookingEnabled(const bool& bInCookingEnabled);
	void SetHasBeenLoaded(const bool& InLoaded);
	void SetHasBeenDuplicated(const bool& InDuplicated);
	void SetCookCount(const int32& InCount);
	void SetRecookRequested(const bool& InRecook);
	void SetRebuildRequested(const bool& InRebuild);

	// Set to True to force the next cook to not build a proxy mesh (regardless of global or override settings) and
	// instead build a UStaticMesh directly (if applicable for the output type).
	void SetNoProxyMeshNextCookRequested(bool bInNoProxyMeshNextCookRequested);
	void SetOverrideGlobalProxyStaticMeshSettings(bool InEnable);
	void SetEnableProxyStaticMeshOverride(bool InEnable);
	void SetEnableProxyStaticMeshRefinementByTimerOverride(bool InEnable);
	void SetProxyMeshAutoRefineTimeoutSecondsOverride(float InValue);
	void SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bool InEnable);
	void SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bool InEnable);

	void SetCookOnParameterChange(bool bEnable);
	void SetCookOnTransformChange(bool bEnable);
	void SetCookOnCookableInputCook(bool bEnable);
	void SetOutputless(bool bEnable);
	void SetUseOutputNodes(bool bEnable);
	void SetOutputTemplateGeos(bool bEnable);
	void SetUploadTransformsToHoudiniEngine(bool bEnable);
	void SetLandscapeUseTempLayers(bool bEnable);
	void SetEnableCurveEditing(bool bEnable);

	// Set whether or not bake after cooking (disabled, always or once).
	void SetBakeAfterNextCook(const EHoudiniBakeAfterNextCook InBakeAfterNextCook);
	void SetHoudiniEngineBakeOption(const EHoudiniEngineBakeOption& InBakeOption);
	void SetReplacePreviousBake(bool bInReplace);
	void SetRemoveOutputAfterBake(bool bInRemove);
	void SetRecenterBakedActors(bool bInRecenter);
	void SetAllowPlayInEditorRefinement(bool bEnabled);
	void SetActorBakeOption(const EHoudiniEngineActorBakeOption InBakeOption);

	void SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InHSMGP);
	void SetStaticMeshBuildSettings(const FMeshBuildSettings& InMBS);

	void SetNeedToUpdateEditorProperties(const bool& bNeedToUpdate) { bNeedToUpdateEditorProperties = bNeedToUpdate; };

	bool NotifyCookedToDownstreamCookables();
	void AddDownstreamCookable(UHoudiniCookable* InDownstreamCookable);
	void RemoveDownstreamCookable(UHoudiniCookable* InDownstreamCookable);
	void ClearDownstreamCookable();

	void SetAssetEditorId(const FName& InName) { AssetEditorId = InName; };

	void SetNodeLabelPrefix(const FString& Prefix);

	const FString & GetNodeLabelPrefix() const;

	//------------------------------------------------------------------------------------------------
	// Supported Features
	//------------------------------------------------------------------------------------------------

	// Whether or not this component should be able to delete the Houdini nodes
	// that correspond to the HoudiniAsset when being deregistered. 
	virtual bool CanDeleteHoudiniNodes() const;

	virtual bool IsInputTypeSupported(EHoudiniInputType InType);
	virtual bool IsOutputTypeSupported(EHoudiniOutputType InType);

	// Feature accessors
	virtual bool IsHoudiniAssetSupported() const { return bHasHoudiniAsset && HoudiniAssetData; };
	virtual bool IsParameterSupported() const { return bHasParameters && ParameterData; };
	virtual bool IsInputSupported() const { return bHasInputs && InputData; };
	virtual bool IsOutputSupported() const { return bHasOutputs && OutputData; };
	virtual bool IsComponentSupported() const { return bHasComponent && ComponentData; };
	virtual bool IsPDGSupported() const { return bHasPDG && PDGData; };
	virtual bool IsBakingSupported() const { return IsOutputSupported() && bHasBaking && BakingData; };
	virtual bool IsProxySupported() const { return IsOutputSupported() && bHasProxy && ProxyData; };

	// Needed for BP support
	virtual void NotifyHoudiniRegisterCompleted() {};
	virtual void NotifyHoudiniPreUnregister() {};
	virtual void NotifyHoudiniPostUnregister() {};

	// Feature mutators
	virtual void SetHoudiniAssetSupported(bool bSupport) { bHasHoudiniAsset = bSupport; };
	virtual void SetParameterSupported(bool bSupport) { bHasParameters = bSupport; };
	virtual void SetInputSupported(bool bSupport) { bHasInputs = bSupport; };
	virtual void SetOutputSupported(bool bSupport) { bHasOutputs = bSupport; };
	virtual void SetComponentSupported(bool bSupport) { bHasComponent = bSupport; };
	virtual void SetPDGSupported(bool bSupport) { bHasPDG = bSupport; };
	virtual void SetBakingSupported(bool bSupport) { bHasBaking = bSupport; };
	virtual void SetProxySupported(bool bSupport) { bHasProxy = bSupport; };

	// Turn On/Off Notifications & Unreal UI
	void SetDoSlateNotifications(bool bOnOff) { bDoSlateNotifications = bOnOff;  }
	bool GetDoSlateNotifications() const { return bDoSlateNotifications; }
	void SetAllowUpdateEditorProperties(bool bOnOff) { bAllowUpdateEditorProperties = bOnOff;  }

	void SetAutoCook(bool bOnOff) { bAutoCook = bOnOff;  }
	//------------------------------------------------------------------------------------------------
	// Delegates / Public API
	//------------------------------------------------------------------------------------------------

	//
	// Begin: IHoudiniAssetStateEvents
	//

	virtual void HandleOnHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState) override;

	FORCEINLINE virtual FOnHoudiniAssetStateChange& GetOnHoudiniAssetStateChangeDelegate() override { return OnHoudiniAssetStateChangeDelegate; }
	FOnCookableStateChangeDelegate& GetOnCookableStateChangeDelegate() { return OnCookableStateChangeDelegate; }


	//
	// End: IHoudiniAssetStateEvents
	//

	// Called by HandleOnHoudiniAssetStateChange when entering the PostCook state. Broadcasts OnPostCookDelegate.
	void HandleOnPreInstantiation();
	void HandleOnPreCook();
	void HandleOnPostCook();
	void HandleOnPostBake(bool bInSuccess);

	// Other public API delegates
	void HandleOnPreOutputProcessing();
	void HandleOnPostOutputProcessing();

	void QueuePreCookCallback(const TFunction<void(UHoudiniCookable*)>& CallbackFn);

	void SetRefineMeshesTimer();
	void OnRefineMeshesTimerFired();

	// Delegates
	FOnPreInstantiationDelegate& GetOnPreInstantiationDelegate() { return OnPreInstantiationDelegate; };
	FOnPreCookDelegate& GetOnPreCookDelegate() { return OnPreCookDelegate; };
	FOnPostCookDelegate& GetOnPostCookDelegate() { return OnPostCookDelegate; };
	FOnPreOutputProcessingDelegate& GetOnPreOutputProcessingDelegate() { return OnPreOutputProcessingDelegate; };
	FOnPostOutputProcessingDelegate& GetOnPostOutputProcessingDelegate() { return OnPostOutputProcessingDelegate; };
	FOnAssetStateChangeDelegate& GetOnAssetStateChangeDelegate() { return OnAssetStateChangeDelegate; };

	bool GetIsPCG() { return bIsPCG;  }
	void SetIsPCG(bool bOnfOff) { bIsPCG = bOnfOff;  }

	void SetLandscapeModificationEnabled(bool bOnOff) { bIsLandscapeModification = bOnOff;  }
	bool IsLandscapeModificationEnabled() { return bIsLandscapeModification;  }

protected:
	// Do any object - specific cleanup required immediately after loading an object.
	// This is not called for newly - created objects, and by default will always execute on the game thread.
	virtual void PostLoad() override;

	virtual void PostEditImport() override;

	virtual void BeginDestroy() override;

protected:

	// Id of the corresponding Houdini node.
	UPROPERTY(DuplicateTransient,Transient)
	int32 NodeId;	// AssetId

	FString NodeLabelPrefix;

	// NEW: The name of the node we're creating/fetching
	// This is NOT an ASSET name - Assets are handled via ASSET DATA
	// TODO COOKABLE: ? path name ?
	FString NodeName;

	// NEW: Indicates if our node should be created when this cookable
	// is instantiated - or if we should simply read data/output from it
	bool bNodeNeedsToBeCreated;

	// Current state (switch to new State?)
	UPROPERTY(DuplicateTransient)
	EHoudiniAssetState CurrentState;	// AssetState

	// Result of the current state
	UPROPERTY(DuplicateTransient)
	EHoudiniAssetStateResult CurrentStateResult;	// AssetStateResult

	// Unique GUID created per cookable
	UPROPERTY(DuplicateTransient)
	FGuid CookableGUID;	// ComponentGUID

	// GUID used to track asynchronous cooking requests of this cookable
	UPROPERTY(DuplicateTransient)
	FGuid HapiGUID;	// HapiGUID
		
	// This cookable's name
	UPROPERTY(DuplicateTransient)
	FString Name;

	// Number of times this has been cooked.
	UPROPERTY(DuplicateTransient)
	int32 CookCount;	// AssetCookCount

	UPROPERTY()
	bool bAutoCook = true;

	UPROPERTY()
	bool bShouldUpdateUI = false;

	// Ids of the nodes that should also be cooked with this cookable
	// This can be used for additional outputs or templated nodes if used.
	UPROPERTY(Transient, DuplicateTransient)
	TArray<int32> NodeIdsToCook;	// NodeIdsToCook
	
	// Cook counts for nodes in the NodeIdsToCook array.
	UPROPERTY(Transient, DuplicateTransient)
	TMap<int32, int32> NodesToCookCookCounts;	// OutputNodeCookCounts

	UPROPERTY(DuplicateTransient)
	bool bPendingDelete;	// bPendingDelete

	UPROPERTY(DuplicateTransient)
	bool bRecookRequested;	// bRecookRequested

	UPROPERTY(DuplicateTransient)
	bool bRebuildRequested;	// bRebuildRequested

	UPROPERTY(DuplicateTransient)
	bool bEnableCooking;	// bEnableCooking

	UPROPERTY(DuplicateTransient)
	bool bForceNeedUpdate;	// bForceNeedUpdate

	UPROPERTY(DuplicateTransient)
	bool bLastCookSuccess;	// bLastCookSuccess

	// The last timestamp this cookable was ticked
	// Used to prioritize/limit the number of Cookable processed per tick
	UPROPERTY(Transient)
	double LastTickTime;	// LastTickTime

	// TODO COOKABLE: Assess if needed? 
	UPROPERTY(DuplicateTransient)
	bool bHasBeenLoaded;	// bHasBeenLoaded	

	// TODO COOKABLE: Assess if needed? 
	UPROPERTY(Transient, DuplicateTransient)
	bool bFullyLoaded;	// bFullyLoaded

	// TODO COOKABLE: Assess if needed?
	// Sometimes, specifically when editing level instances, the Unreal Editor will duplicate the HDA,
	// then duplicate it again, before we get a change to call UpdatePostDuplicate().
	// So bHasBeenDuplicated should not be cleared and is so not marked DuplicateTransient.
	UPROPERTY()
	bool bHasBeenDuplicated;	// bHasBeenDuplicated

	// Indicates whether or not this cookable should update its editor UI
	// This is to prevent successive calls of the function for the same cookables 
	UPROPERTY(Transient, DuplicateTransient)
	bool bNeedToUpdateEditorProperties;


	// Used to enable certain detail panel widgets.
	UPROPERTY()
	bool bIsPCG;

	// Can the cookable modify landscapes? Primarily for PCG.
	UPROPERTY()
	bool bIsLandscapeModification;


	//
	// COOKABLE DATA
	//

	// HOUDINI ASSET
	// Indicates if this cookable is using an HDA
	UPROPERTY()
	bool bHasHoudiniAsset;

	// Structure containing the HDA data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableHoudiniAssetData> HoudiniAssetData;

	// PARAMETERS
	// Indicates if this cookable has parameters
	UPROPERTY()
	bool bHasParameters;

	// Structure containing the parameter data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableParameterData> ParameterData;

	// INPUTS
	// Indicates if this cookable has inputs
	UPROPERTY()
	bool bHasInputs;

	// Structure containing the input data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableInputData> InputData;

	// OUTPUTS
	// Indicates if this cookable has outputs
	UPROPERTY()
	bool bHasOutputs;

	// Structure containing the output data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableOutputData> OutputData;

	// COMPONENTS / TRANSFORM
	// Indicates if this cookable has a component/is placed in the level
	UPROPERTY()
	bool bHasComponent; // bIsInWorld?

	// Structure containing the component's data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableComponentData> ComponentData;

	// PDG
	// Indicates if this cookable has access to PDG
	UPROPERTY()
	bool bHasPDG;

	// Structure containing the PDG data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookablePDGData> PDGData;

	// Baking
	// Indicates if this cookable has access to baking
	UPROPERTY()
	bool bHasBaking;
	// Structure containing the Baking data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableBakingData> BakingData;

	// Proxy
	// Indicates if this cookable canm use proxy meshes
	UPROPERTY()
	bool bHasProxy;
	// Structure containing the Proxy data
	UPROPERTY(EditAnywhere, Category = "Houdini Cookable")
	TObjectPtr<UCookableProxyData> ProxyData;

	//
	// Public API delegates
	//
	
	// Delegate that is broadcast when current State changes
	FOnHoudiniAssetStateChange 	OnHoudiniAssetStateChangeDelegate;

	// Delegate that is broadcast when the current state changes (Cookable version)
	FOnCookableStateChangeDelegate OnCookableStateChangeDelegate;
	 
	// Delegate to broadcast before instantiation
	// Arguments are (HoudiniAssetComponent* HAC)
	FOnPreInstantiationDelegate OnPreInstantiationDelegate;

	// Delegate to broadcast after a post cook event
	// Arguments are (HoudiniAssetComponent* HAC, bool IsSuccessful)
	FOnPreCookDelegate OnPreCookDelegate;

	// Delegate to broadcast after a post cook event
	// Arguments are (HoudiniAssetComponent* HAC, bool IsSuccessful)
	FOnPostCookDelegate OnPostCookDelegate;

	FOnPostOutputProcessingDelegate OnPostOutputProcessingDelegate;
	FOnPreOutputProcessingDelegate OnPreOutputProcessingDelegate;

	// Delegate that is broadcast when the asset state changes (HAC version). ??
	FOnAssetStateChangeDelegate OnAssetStateChangeDelegate;

	// Store any PreCookCallbacks here until the Cookable is ready to process them during the PreCook event.
	TArray<TFunction<void(UHoudiniCookable*)>> PreCookCallbacks;

	UPROPERTY(Transient)
	bool bDoSlateNotifications;

	UPROPERTY(Transient)
	bool bAllowUpdateEditorProperties;

#if WITH_EDITORONLY_DATA
public:

	UPROPERTY()
	bool bGenerateMenuExpanded;

	UPROPERTY()
	bool bBakeMenuExpanded;

	UPROPERTY()
	bool bAssetOptionMenuExpanded;

	UPROPERTY()
	bool bHelpAndDebugMenuExpanded;
#endif

	// Indicates the Id of the AssetEditor viewing this cookable
	// Can be null/empty if not viewded by an AssetEditor
	UPROPERTY()
	FName AssetEditorId;
};