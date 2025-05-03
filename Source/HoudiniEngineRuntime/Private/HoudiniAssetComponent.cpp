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

#include "HoudiniAssetComponent.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniInput.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniStaticMeshComponent.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"

#if WITH_EDITOR
#include "HoudiniEditorAssetStateSubsystemInterface.h"
#endif

#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Landscape.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "InstancedFoliageActor.h"
#include "UObject/DevObjectVersion.h"
#include "Serialization/CustomVersion.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/UObjectGlobals.h"
#include "BodySetupEnums.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInstance.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionComponent.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#endif

#include "PrimitiveSceneProxy.h"

#if WITH_EDITOR
	#include "Editor/UnrealEd/Private/GeomFitUtils.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "UObject/Linker.h"
#endif

#include "ComponentReregisterContext.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
	#include "LevelInstance/LevelInstanceInterface.h"
#endif
#include "LevelInstance/LevelInstanceSubsystem.h"

// Macro to update given properties on all children components of the HAC.
#define HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( COMPONENT_CLASS, PROPERTY ) \
    do \
    { \
        TArray<UActorComponent *> ReregisterComponents; \
        TArray<USceneComponent *> LocalAttachChildren;\
        GetChildrenComponents(true, LocalAttachChildren); \
        for (TArray<USceneComponent *>::TConstIterator Iter(LocalAttachChildren); Iter; ++Iter) \
        { \
            COMPONENT_CLASS * Component = Cast<COMPONENT_CLASS>(*Iter); \
            if (Component) \
            { \
                Component->PROPERTY = PROPERTY; \
                ReregisterComponents.Add(Component); \
            } \
        } \
    \
        if (ReregisterComponents.Num() > 0) \
        { \
            FMultiComponentReregisterContext MultiComponentReregisterContext(ReregisterComponents); \
        } \
    } \
    while(0)


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	int64 InitialOffset = Ar.Tell();
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	bool bLegacyComponent = false;
	if (Ar.IsLoading())
	{
		int32 Ver = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE && Ver >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
		{
			bLegacyComponent = true;
		}
	}

	if (bLegacyComponent)
	{
		// Skip the v1 object
		HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniAssetComponent : serialization will be skipped."));

		Super::Serialize(Ar);

		// Skip v1 Serialized data
		if (FLinker* Linker = Ar.GetLinker())
		{
			int32 const ExportIndex = this->GetLinkerIndex();
			FObjectExport& Export = Linker->ExportMap[ExportIndex];
			Ar.Seek(InitialOffset + Export.SerialSize);
			return;
		}
	}
	else
	{
		// Normal v2 serialization
		Super::Serialize(Ar);
	}
}


UHoudiniAssetComponent::UHoudiniAssetComponent(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	HoudiniAsset = nullptr;	
	bCookOnParameterChange = true;
	bUploadTransformsToHoudiniEngine = true;
	bCookOnTransformChange = false;
	//bUseNativeHoudiniMaterials = true;
	bCookOnAssetInputCook = true;

	AssetId = -1;
	AssetState = EHoudiniAssetState::NewHDA;
	AssetStateResult = EHoudiniAssetStateResult::None;
	AssetCookCount = 0;
	
	SubAssetIndex = -1;

	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID.Invalidate();

	HapiAssetName = FString();

	// Create unique component GUID.
	ComponentGUID = FGuid::NewGuid();
	LastComponentTransform = FTransform();

	bUploadTransformsToHoudiniEngine = true;

	bHasBeenLoaded = false;
	bHasBeenDuplicated = false;
	bPendingDelete = false;
	bRecookRequested = false;
	bRebuildRequested = false;
	bEnableCooking = true;
	bForceNeedUpdate = false;
	bLastCookSuccess = false;
	bBlueprintStructureModified = false;
	bBlueprintModified = false;

	//bEditorPropertiesNeedFullUpdate = true;

	// Folder used for cooking, the value is initialized by Output Translator
	// TemporaryCookFolder.Path = HAPI_UNREAL_DEFAULT_TEMP_COOK_FOLDER;
	
	// Folder used for baking this asset's outputs, the value is initialized by Output Translator
	// BakeFolder.Path = HAPI_UNREAL_DEFAULT_BAKE_FOLDER;

	bHasComponentTransformChanged = false;

	bFullyLoaded = false;

	bOutputless = false;
	bOutputTemplateGeos = false;
	bUseOutputNodes = true;
	PDGAssetLink = nullptr;

	bUseDeprecatedRawMeshSupport = false;

	bOverrideGlobalProxyStaticMeshSettings = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings)
	{
		bEnableProxyStaticMeshOverride = HoudiniRuntimeSettings->bEnableProxyStaticMesh;
		bEnableProxyStaticMeshRefinementByTimerOverride = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementByTimer;
		ProxyMeshAutoRefineTimeoutSecondsOverride = HoudiniRuntimeSettings->ProxyMeshAutoRefineTimeoutSeconds;
		bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreSaveWorld;
		bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreBeginPIE;
	}
	else
	{
		bEnableProxyStaticMeshOverride = false; 
		bEnableProxyStaticMeshRefinementByTimerOverride = true; 
		ProxyMeshAutoRefineTimeoutSecondsOverride = 10.0f;
		bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = true; 
		bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = true;
	}
	
	bNoProxyMeshNextCookRequested = false;
	bBakeAfterNextCook_DEPRECATED = false;
	BakeAfterNextCook = EHoudiniBakeAfterNextCook::Disabled;

#if WITH_EDITORONLY_DATA
	bGenerateMenuExpanded = true;
	bBakeMenuExpanded = true;
	bAssetOptionMenuExpanded = true;
	bHelpAndDebugMenuExpanded = true;

	HoudiniEngineBakeOption = EHoudiniEngineBakeOption::ToActor;

	bRemoveOutputAfterBake = false;
	bRecenterBakedActors = false;
	bReplacePreviousBake = false;
	ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerComponent;
	bAllowPlayInEditorRefinement = false;
	bNeedToUpdateEditorProperties = false;
	bLandscapeUseTempLayers = false;
	bEnableCurveEditing = true;
#endif

	//
	// 	Set component properties.
	//

	Mobility = EComponentMobility::Static;

	SetGenerateOverlapEvents(false);

	// Similar to UMeshComponent.
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;

	// This component requires render update.
	bNeverNeedsRenderUpdate = false;

	Bounds = FBox(ForceInitToZero);

	LastTickTime = 0.0;
	LastLiveSyncPingTime = 0.0;

	// Initialize the default SM Build settings with the plugin's settings default values
	StaticMeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();

	//bWantsOnUpdateTransform = true;

	bIsPDGAssetLinkInitialized = false;
}

UHoudiniAssetComponent::~UHoudiniAssetComponent()
{
	// Unregister ourself so our houdini node can be delete.

	// This gets called in UnRegisterHoudiniComponent, with appropriate checks. Don't call it here.
	//FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(AssetId, true);

	FHoudiniEngineRuntime::Get().UnRegisterHoudiniComponent(this);
}

void UHoudiniAssetComponent::PostInitProperties()
{
	Super::PostInitProperties();

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
	{
		// Copy default static mesh generation parameters from settings.
		StaticMeshGenerationProperties.bGeneratedDoubleSidedGeometry = HoudiniRuntimeSettings->bDoubleSidedGeometry;
		StaticMeshGenerationProperties.GeneratedPhysMaterial = HoudiniRuntimeSettings->PhysMaterial;
		StaticMeshGenerationProperties.DefaultBodyInstance = HoudiniRuntimeSettings->DefaultBodyInstance;
		StaticMeshGenerationProperties.GeneratedCollisionTraceFlag = HoudiniRuntimeSettings->CollisionTraceFlag;
		StaticMeshGenerationProperties.GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
		StaticMeshGenerationProperties.GeneratedLightMapCoordinateIndex = HoudiniRuntimeSettings->LightMapCoordinateIndex;
		StaticMeshGenerationProperties.bGeneratedUseMaximumStreamingTexelRatio = HoudiniRuntimeSettings->bUseMaximumStreamingTexelRatio;
		StaticMeshGenerationProperties.GeneratedStreamingDistanceMultiplier = HoudiniRuntimeSettings->StreamingDistanceMultiplier;
		StaticMeshGenerationProperties.GeneratedWalkableSlopeOverride = HoudiniRuntimeSettings->WalkableSlopeOverride;
		StaticMeshGenerationProperties.GeneratedFoliageDefaultSettings = HoudiniRuntimeSettings->FoliageDefaultSettings;
		StaticMeshGenerationProperties.GeneratedAssetUserData = HoudiniRuntimeSettings->AssetUserData;
	}

	// Register ourself to the HER singleton
	RegisterHoudiniComponent(this);
}

UWorld* 
UHoudiniAssetComponent::GetHACWorld() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
		World = GetOwner() ? GetOwner()->GetWorld() : nullptr;

	return World; 
}


UHoudiniAsset *
UHoudiniAssetComponent::GetHoudiniAsset() const
{
	return HoudiniAsset;
}

FString
UHoudiniAssetComponent::GetDisplayName() const
{
	return GetOwner() ? GetOwner()->GetActorNameOrLabel() : GetName();
}

void
UHoudiniAssetComponent::GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const
{
	for (UHoudiniOutput* Output : Outputs)
	{
		OutOutputs.Add(Output);
	}
}

bool 
UHoudiniAssetComponent::IsProxyStaticMeshEnabled() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
	{
		return bEnableProxyStaticMeshOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (HoudiniRuntimeSettings)
		{
			return HoudiniRuntimeSettings->bEnableProxyStaticMesh;
		}
		else
		{
			return false;
		}
	}
}

bool 
UHoudiniAssetComponent::IsProxyStaticMeshRefinementByTimerEnabled() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
	{
		return bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementByTimerOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (HoudiniRuntimeSettings)
		{
			return HoudiniRuntimeSettings->bEnableProxyStaticMesh && HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementByTimer;
		}
		else
		{
			return false;
		}
	}
}

float
UHoudiniAssetComponent::GetProxyMeshAutoRefineTimeoutSeconds() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
	{
		return ProxyMeshAutoRefineTimeoutSecondsOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (HoudiniRuntimeSettings)
		{
			return HoudiniRuntimeSettings->ProxyMeshAutoRefineTimeoutSeconds;
		}
		else
		{
			return 5.0f;
		}
	}
}

bool
UHoudiniAssetComponent::IsProxyStaticMeshRefinementOnPreSaveWorldEnabled() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
	{
		return bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (HoudiniRuntimeSettings)
		{
			return HoudiniRuntimeSettings->bEnableProxyStaticMesh && HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreSaveWorld;
		}
		else
		{
			return false;
		}
	}
}

bool 
UHoudiniAssetComponent::IsProxyStaticMeshRefinementOnPreBeginPIEEnabled() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
	{
		return bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (HoudiniRuntimeSettings)
		{
			return HoudiniRuntimeSettings->bEnableProxyStaticMesh && HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreBeginPIE;
		}
		else
		{
			return false;
		}
	}
}


void
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset * InHoudiniAsset)
{
	// Check the asset validity
	if (!IsValid(InHoudiniAsset))
		return;

	// If it is the same asset, do nothing.
	if ( InHoudiniAsset == HoudiniAsset )
		return;

	HoudiniAsset = InHoudiniAsset;
}


void 
UHoudiniAssetComponent::OnHoudiniAssetChanged()
{
	// TODO: clear input/params/outputs?
	Parameters.Empty();

	// The asset has been changed, mark us as needing to be reinstantiated
	MarkAsNeedInstantiation();

	// Force an update on the next tick
	bForceNeedUpdate = true;
}

void UHoudiniAssetComponent::QueuePreCookCallback(const TFunction<void(UHoudiniAssetComponent*)>& CallbackFn)
{
	PreCookCallbacks.Add(CallbackFn);
}

bool
UHoudiniAssetComponent::NeedUpdateParameters() const
{
	// This is being split into a separate function to that it can
	// be called separately for component templates.
	if (!bCookOnParameterChange)
		return false;

	// Go through all our parameters, return true if they have been updated
	for (auto CurrentParm : Parameters)
	{
		if (!IsValid(CurrentParm))
			continue;

		if (!CurrentParm->HasChanged())
			continue;

		// See if the parameter doesn't require an update 
		// (because it has failed to upload previously or has been loaded)
		if (!CurrentParm->NeedsToTriggerUpdate())
			continue;
		
		return true;
	}

	return false;
}

bool 
UHoudiniAssetComponent::NeedUpdateInputs() const
{
	// Go through all our inputs, return true if they have been updated
	for (auto CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		if (!CurrentInput->HasChanged())
			continue;

		// See if the input doesn't require an update 
		// (because it has failed to upload previously or has been loaded)
		if (!CurrentInput->NeedsToTriggerUpdate())
			continue;

		HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniAssetBlueprintComponent::NeedUpdateInputs()] Inputs need update for component: %s"), *(GetPathName()));
		return true;
	}

	return false;
}

bool
UHoudiniAssetComponent::HasPreviousBakeOutput() const
{
	// Look for any bake output objects in the output array
	for (const UHoudiniOutput* Output : Outputs)
	{
		if (!IsValid(Output))
			continue;

		if (BakedOutputs.Num() == 0)
			return false;

		for (const FHoudiniBakedOutput& BakedOutput : BakedOutputs)
		{
			if (BakedOutput.BakedOutputObjects.Num() > 0)
				return true;
		}
	}

	return false;
}

FString
UHoudiniAssetComponent::GetBakeFolderOrDefault() const
{
	return !BakeFolder.Path.IsEmpty() ? BakeFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
}

FString
UHoudiniAssetComponent::GetTemporaryCookFolderOrDefault() const
{
	return !TemporaryCookFolder.Path.IsEmpty() ? TemporaryCookFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
}

bool
UHoudiniAssetComponent::NeedUpdate() const
{	
	TRACE_CPUPROFILER_EVENT_SCOPE(UHoudiniAssetComponent::NeedUpdate);

	if (AssetState != DebugLastAssetState)
	{
		DebugLastAssetState = AssetState;
	}

	// It is important to check this when dealing with Blueprints since the
	// preview components start receiving events from the template component
	// before the preview component have finished initialization.
	if (!IsFullyLoaded())
		return false;

	// We must have a valid asset, unless we're a NodeSync component
	if (!IsValid(HoudiniAsset) && !IsA<UHoudiniNodeSyncComponent>())
		return false;

	if (bForceNeedUpdate || bRecookRequested)
		return true;
	
	// If we don't want to cook on parameter/input change dont bother looking for updates
	if (!bCookOnParameterChange && !bRecookRequested && !bRebuildRequested)
		return false;

	// Check if the HAC's transform has changed and we need to cook because of it
	if (bCookOnTransformChange && bHasComponentTransformChanged && bUploadTransformsToHoudiniEngine)
		return true;

	if (NeedUpdateParameters())
		return true;

	if (NeedUpdateInputs())
		return true;

	// Go through all outputs, filter the editable nodes. Return true if they have been updated.
	for (auto CurrentOutput : Outputs) 
	{
		if (!IsValid(CurrentOutput))
			continue;
		
		// We only care about editable outputs
		if (!CurrentOutput->IsEditableNode())
			continue;

		// Trigger an update if the output object is marked as modified by user.
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurrentOutput->GetOutputObjects();
		for (auto& NextPair : OutputObjects)
		{
			for(auto Component : NextPair.Value.OutputComponents)
			{
			    // For now, only editable curves can trigger update
			    UHoudiniSplineComponent* HoudiniSplineComponent = Cast<UHoudiniSplineComponent>(Component);
			    if (!HoudiniSplineComponent)
				    continue;

			    // Output curves cant trigger an update!
			    if (HoudiniSplineComponent->bIsOutputCurve)
				    continue;

			    if (HoudiniSplineComponent->NeedsToTriggerUpdate())
				    return true;
			}
		}
	}

	return false;
}

void
UHoudiniAssetComponent::PreventAutoUpdates()
{
	// It is important to check this when dealing with Blueprints since the
	// preview components start receiving events from the template component
	// before the preview component have finished initialization.
	if (!IsFullyLoaded())
		return;

	bForceNeedUpdate = false;
	bRecookRequested = false;
	bRebuildRequested = false;
	bHasComponentTransformChanged = false;

	// Go through all our parameters, prevent them from triggering updates
	for (auto CurrentParm : Parameters)
	{
		if (!IsValid(CurrentParm))
			continue;

		// Prevent the parm from triggering an update
		CurrentParm->SetNeedsToTriggerUpdate(false);
	}

	// Same with inputs
	for (auto CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		// Prevent the input from triggering an update
		CurrentInput->SetNeedsToTriggerUpdate(false);
	}

	// Go through all outputs, filter the editable nodes.
	for (auto CurrentOutput : Outputs)
	{
		if (!IsValid(CurrentOutput))
			continue;

		// We only care about editable outputs
		if (!CurrentOutput->IsEditableNode())
			continue;

		// Trigger an update if the output object is marked as modified by user.
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurrentOutput->GetOutputObjects();
		for (auto& NextPair : OutputObjects)
		{
			// For now, only editable curves can trigger update
			for(auto Component : NextPair.Value.OutputComponents)
			{
			    UHoudiniSplineComponent* HoudiniSplineComponent = Cast<UHoudiniSplineComponent>(Component);
			    if (!HoudiniSplineComponent)
				    continue;

			    // Output curves cant trigger an update!
			    if (HoudiniSplineComponent->bIsOutputCurve)
				    continue;

			    HoudiniSplineComponent->SetNeedsToTriggerUpdate(false);
			}
		}
	}
}

// Indicates if any of the HAC's output components needs to be updated (no recook needed)
bool
UHoudiniAssetComponent::NeedOutputUpdate() const
{
	// Go through all outputs
	for (auto CurrentOutput : Outputs)
	{
		if (!IsValid(CurrentOutput))
			continue;

		for (const auto& InstOutput : CurrentOutput->GetInstancedOutputs())
		{
			if (InstOutput.Value.bChanged)
				return true;
		}
	}

	return false;
}

bool UHoudiniAssetComponent::NeedBlueprintStructureUpdate() const
{
	// TODO: Add similar flags to inputs, parametsr
	return bBlueprintStructureModified;
}

bool UHoudiniAssetComponent::NeedBlueprintUpdate() const
{
	// TODO: Add similar flags to inputs, parametsr
	return bBlueprintModified;
}

bool 
UHoudiniAssetComponent::NotifyCookedToDownstreamAssets()
{
	// Before notifying, clean up our downstream assets
	// - check that they are still valid
	// - check that we are still connected to one of its asset input
	// - check that the asset has the CookOnAssetInputCook trigger enabled
	TArray<UHoudiniAssetComponent*> DownstreamToDelete;	
	for(auto& CurrentDownstreamHAC : DownstreamHoudiniAssets)
	{
		// Remove the downstream connection by default,
		// unless we actually were properly connected to one of this HDA's input.
		bool bRemoveDownstream = true;
		if (IsValid(CurrentDownstreamHAC))
		{
			// Go through the HAC's input
			for (auto& CurrentDownstreamInput : CurrentDownstreamHAC->Inputs)
			{
				if (!IsValid(CurrentDownstreamInput))
					continue;

				EHoudiniInputType CurrentDownstreamInputType = CurrentDownstreamInput->GetInputType();
				
				// Require an asset input type, not just all World/NewWorld
				if (!CurrentDownstreamInput->IsAssetInput())
					continue;

				// Ensure that we are an input object of that input
				if (!CurrentDownstreamInput->ContainsInputObject(this, CurrentDownstreamInputType))
					continue;

				// We are an input to this HDA
				// Make sure that the 
				if (!CurrentDownstreamInput->GetImportAsReference())
				{
					const TArray<UHoudiniInputObject*>* ObjectArray = CurrentDownstreamInput->GetHoudiniInputObjectArray(CurrentDownstreamInputType);
					if (ObjectArray)
					{
						for (auto& CurrentInputObject : (*ObjectArray))
						{
							if (!IsValid(CurrentInputObject))
								continue;

							if (CurrentInputObject->GetObject() != this)
								continue;

							CurrentInputObject->SetInputNodeId(GetAssetId());
							CurrentInputObject->SetInputObjectNodeId(GetAssetId());
						}
					}
				}

				if (CurrentDownstreamHAC->bCookOnAssetInputCook)
				{
					// Mark that HAC's input has changed
					CurrentDownstreamInput->MarkChanged(true);
				}
				bRemoveDownstream = false;
			}
		}

		if (bRemoveDownstream)
		{
			DownstreamToDelete.Add(CurrentDownstreamHAC);
		}
	}

	for (auto ToDelete : DownstreamToDelete)
	{
		DownstreamHoudiniAssets.Remove(ToDelete);
	}

	return true;
}

bool
UHoudiniAssetComponent::NeedsToWaitForInputHoudiniAssets()
{
	for (auto& CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		EHoudiniInputType CurrentInputType = CurrentInput->GetInputType();
		if (!CurrentInput->IsAssetInput())
			continue;

		TArray<UHoudiniInputObject*>* ObjectArray = CurrentInput->GetHoudiniInputObjectArray(CurrentInputType);
		if (!ObjectArray)
			continue;

		for (auto& CurrentInputObject : (*ObjectArray))
		{
			// Get the input HDA
			UHoudiniAssetComponent* InputHAC = CurrentInputObject 
				? Cast<UHoudiniAssetComponent>(CurrentInputObject->GetObject()) 
				: nullptr;

			if (!InputHAC)
				continue;

			// If the input HDA needs to be instantiated, force him to instantiate
			// if the input HDA is in any other state than None, we need to wait for him
			// to finish whatever it's doing
			if (InputHAC->GetAssetState() == EHoudiniAssetState::NeedInstantiation)
			{
				// Tell the input HAC to instantiate
				InputHAC->SetAssetState(EHoudiniAssetState::PreInstantiation);

				// We need to wait
				return true;
			}
			else if (InputHAC->GetAssetState() != EHoudiniAssetState::None)
			{
				// We need to wait
				return true;
			}
		}
	}

	return false;
}

void
UHoudiniAssetComponent::BeginDestroy()
{
	if (CanDeleteHoudiniNodes())
	{
	}

	// Gets called through UnRegisterHoudiniComponent().
	//FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(AssetId, true);

	// Unregister ourself so our houdini node can be deleted
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniComponent(this);

	Super::BeginDestroy();
}

void 
UHoudiniAssetComponent::MarkAsNeedCook()
{
	// Force the asset state to NeedCook
	//AssetCookCount = 0;
	bHasBeenLoaded = true;
	bPendingDelete = false;
	bRecookRequested = true;
	bRebuildRequested = false;

	//bEditorPropertiesNeedFullUpdate = true;

	// We need to mark all our parameters as changed/trigger update
	for (auto CurrentParam : Parameters)
	{
		if (!IsValid(CurrentParam))
			continue;

		// Do not trigger parameter update for Button/Button strip when recooking
		// As we don't want to trigger the buttons
		if (CurrentParam->IsA<UHoudiniParameterButton>() || CurrentParam->IsA<UHoudiniParameterButtonStrip>())
			continue;

		CurrentParam->MarkChanged(true);
		CurrentParam->SetNeedsToTriggerUpdate(true);
	}

	// We need to mark all of our editable curves as changed
	for (auto Output : Outputs)
	{
		if (!IsValid(Output) || Output->GetType() != EHoudiniOutputType::Curve || !Output->IsEditableNode())
			continue;

		for (auto& OutputObjectEntry : Output->GetOutputObjects())
		{
			FHoudiniOutputObject& OutputObject = OutputObjectEntry.Value;
			if (OutputObject.CurveOutputProperty.CurveOutputType != EHoudiniCurveOutputType::HoudiniSpline)
				continue;

            for(auto Component : OutputObject.OutputComponents)
            {
			    UHoudiniSplineComponent* SplineComponent = Cast<UHoudiniSplineComponent>(Component);
			    if (!IsValid(SplineComponent))
				    continue;

			    // This sets bHasChanged and bNeedsToTriggerUpdate
			    SplineComponent->MarkChanged(true);
            }
		}
	}

	// We need to mark all our inputs as changed/trigger update
	for (auto CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;
		CurrentInput->MarkChanged(true);
		CurrentInput->SetNeedsToTriggerUpdate(true);
		CurrentInput->MarkDataUploadNeeded(true);

		FHoudiniInputObjectSettings CurrentInputSettings(CurrentInput);

		// In addition to marking the input as changed/need update, we also need to make sure that any changes on the
		// Unreal side have been recorded for the input before sending to Houdini. For that we also mark each input
		// object as changed/need update and explicitly call the Update function on each input object. For example, for
		// input actors this would recreate the Houdini input actor components from the actor's components, picking up
		// any new components since the last call to Update.
		TArray<UHoudiniInputObject*>* InputObjectArray = CurrentInput->GetHoudiniInputObjectArray(CurrentInput->GetInputType());
		if (InputObjectArray && InputObjectArray->Num() > 0)
		{
			for (auto CurrentInputObject : *InputObjectArray)
			{
				if (!IsValid(CurrentInputObject))
					continue;

				UObject* const Object = CurrentInputObject->GetObject();
				if (IsValid(Object))
					CurrentInputObject->Update(Object, CurrentInputSettings);

				CurrentInputObject->MarkChanged(true);
				CurrentInputObject->SetNeedsToTriggerUpdate(true);
				CurrentInputObject->MarkTransformChanged(true);
			}
		}
	}

	// Clear the static mesh bake timer
	ClearRefineMeshesTimer();
}

void
UHoudiniAssetComponent::MarkAsNeedRebuild()
{
	// Invalidate the asset ID
	//AssetId = -1;

	// Force the asset state to NeedRebuild
	SetAssetState(EHoudiniAssetState::NeedRebuild);
	AssetStateResult = EHoudiniAssetStateResult::None;

	// Reset some of the asset's flag
	//AssetCookCount = 0;
	bHasBeenLoaded = true;
	bPendingDelete = false;
	bRecookRequested = false;
	bRebuildRequested = true;
	bFullyLoaded = false;

	//bEditorPropertiesNeedFullUpdate = true;
	/*
	// We need to mark all our parameters as changed/trigger update
	for (auto CurrentParam : Parameters)
	{
		if (!IsValid(CurrentParam))
			continue;

		// Do not trigger parameter update for Button/Button strip when rebuilding
		// As we don't want to trigger the buttons
		if (CurrentParam->IsA<UHoudiniParameterButton>() || CurrentParam->IsA<UHoudiniParameterButtonStrip>())
			continue;

		CurrentParam->MarkChanged(true);
		CurrentParam->SetNeedsToTriggerUpdate(true);
	}
	*/

	// We need to mark all of our editable curves as changed
	for (auto Output : Outputs)
	{
		if (!IsValid(Output) || Output->GetType() != EHoudiniOutputType::Curve || !Output->IsEditableNode())
			continue;

		for (auto& OutputObjectEntry : Output->GetOutputObjects())
		{
			FHoudiniOutputObject& OutputObject = OutputObjectEntry.Value;
			if (OutputObject.CurveOutputProperty.CurveOutputType != EHoudiniCurveOutputType::HoudiniSpline)
				continue;

			for(auto Component : OutputObject.OutputComponents)
			{
			    UHoudiniSplineComponent* SplineComponent = Cast<UHoudiniSplineComponent>(Component);
			    if (!IsValid(SplineComponent))
				    continue;

			    // This sets bHasChanged and bNeedsToTriggerUpdate
			    SplineComponent->MarkChanged(true);
			}
		}
	}

	// Uncomment this for building regression tests that need a clean output.
	//Outputs.Empty();

	// We need to mark all our inputs as changed/trigger update
	for (auto CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;
		CurrentInput->MarkChanged(true);
		CurrentInput->SetNeedsToTriggerUpdate(true);
		CurrentInput->MarkDataUploadNeeded(true);
	}

	// Clear the static mesh bake timer
	ClearRefineMeshesTimer();
}

// Marks the asset as needing to be instantiated
void
UHoudiniAssetComponent::MarkAsNeedInstantiation()
{
	// Invalidate the asset ID
	AssetId = -1;

	if (Parameters.Num() <= 0 && Inputs.Num() <= 0 && Outputs.Num() <= 0)
	{
		// The asset has no parameters or inputs.
		// This likely indicates it has never cooked/been instantiated.
		// Set its state to NewHDA to force its instantiation
		// so that we can have its parameters/input interface
		SetAssetState(EHoudiniAssetState::NewHDA);
	}
	else
	{
		// The asset has cooked before since we have a parameter/input interface
		// Set its state to need instantiation so that the asset is instantiated
		// after being modified
		SetAssetState(EHoudiniAssetState::NeedInstantiation);
	}

	AssetStateResult = EHoudiniAssetStateResult::None;

	// Reset some of the asset's flag
	AssetCookCount = 0;
	bHasBeenLoaded = true;
	bPendingDelete = false;
	bRecookRequested = false;
	bRebuildRequested = false;
	bFullyLoaded = false;

	//bEditorPropertiesNeedFullUpdate = true;

	// We need to mark all our parameters as changed/not triggering update
	for (auto CurrentParam : Parameters)
	{
		if (CurrentParam)
		{
			CurrentParam->MarkChanged(true);
			CurrentParam->SetNeedsToTriggerUpdate(false);
		}
	}

	// We need to mark all our inputs as changed/not triggering update
	for (auto CurrentInput : Inputs)
	{
		if (CurrentInput)
		{
			CurrentInput->MarkChanged(true);
			CurrentInput->SetNeedsToTriggerUpdate(false);
			CurrentInput->MarkDataUploadNeeded(true);
		}
	}

	/*if (!CanInstantiateAsset())
	{
		AssetState = EHoudiniAssetState::None;
		AssetStateResult = EHoudiniAssetStateResult::None;
	}*/

	// Clear the static mesh bake timer
	ClearRefineMeshesTimer();
}

void UHoudiniAssetComponent::MarkAsBlueprintStructureModified()
{
	bBlueprintStructureModified = true;
}

void UHoudiniAssetComponent::MarkAsBlueprintModified()
{
	bBlueprintModified = true;
}

void
UHoudiniAssetComponent::PostLoad()
{
	Super::PostLoad();

	// Mark as need instantiation
	MarkAsNeedInstantiation();

	// Component has been loaded, not duplicated
	bHasBeenDuplicated = false;

	// We need to register ourself
	RegisterHoudiniComponent(this);

	// Register our PDG Asset link if we have any

	// !!! Do not update rendering while loading, do it when setting up the render state
	// UpdateRenderingInformation();

	// Handle FHoudiniOutputObjects which have the old single component and migrate it to an array.
	bool bFoundDeprecatedComponent = false;
	for(auto Output : Outputs)
	{
	    if (!IsValid(Output))
	        continue;

	    for(auto& It : Output->GetOutputObjects())
	    {
			FHoudiniOutputObject& OutputObject = It.Value;
			if (OutputObject.OutputComponent_DEPRECATED != nullptr)
			{
			    OutputObject.OutputComponents.Add(OutputObject.OutputComponent_DEPRECATED);
				OutputObject.OutputComponent_DEPRECATED = nullptr;
				bFoundDeprecatedComponent = true;
			}
	    }
	}

	if (bFoundDeprecatedComponent)
	{
		// User should see this message once when loading an old asset.
		HOUDINI_LOG_MESSAGE(TEXT("Upgraded deprecated single component in FHoudiniOutputObject"));
	}
#if WITH_EDITORONLY_DATA
	auto MaxValue = StaticEnum<EHoudiniEngineBakeOption>()->GetMaxEnumValue() - 1;

	if (static_cast<int>(HoudiniEngineBakeOption) > MaxValue)
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Bake Type found, setting to To Actor. Possibly Foliage, which is deprecated, use the unreal_foliage attribute instead."));
		HoudiniEngineBakeOption = EHoudiniEngineBakeOption::ToActor;
	}
#endif

	// Handle deprecated bBakeAfterNextCook: default value is false, so if it is true in PostLoad then it was saved
	// as true (pre-deprecation)
	if (bBakeAfterNextCook_DEPRECATED)
	{
		bBakeAfterNextCook_DEPRECATED = false;
		BakeAfterNextCook = EHoudiniBakeAfterNextCook::Always;
	}
}

void
UHoudiniAssetComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	// Just call parent class for now.
	Super::CreateRenderState_Concurrent(Context);
}

void 
UHoudiniAssetComponent::PostEditImport()
{
	Super::PostEditImport();

	MarkAsNeedInstantiation();

	// Component has been duplicated, not loaded
	// We do need the loaded flag to reapply parameters, inputs
	// and properly update some of the output objects
	bHasBeenDuplicated = true;

	//RemoveAllAttachedComponents();

	AssetState = EHoudiniAssetState::PreInstantiation;
	AssetStateResult = EHoudiniAssetStateResult::None;
	
	// TODO?
	// REGISTER?
}

void
UHoudiniAssetComponent::UpdatePostDuplicate()
{
	// TODO:
	// - Keep the output objects/components (remove duplicatetransient on the output object uproperties)
	// - Duplicate created objects (ie SM) and materials
	// - Update the output components to use these instead
	// This should remove the need for a cook on duplicate

	// For now, we simply clean some of the HAC's component manually
	const TArray<USceneComponent*> Children = GetAttachChildren();

	for (auto & NextChild : Children) 
	{
		if (!IsValid(NextChild))
			continue;

		// We don't want to remove components that were added in a Blueprint Template
		if (NextChild->IsCreatedByConstructionScript())
			continue;

		USceneComponent * ComponentToRemove = nullptr;
		if (NextChild->IsA<UStaticMeshComponent>()) 
		{
			// This also covers UStaticMeshComponent derived instancers, such as UInstancedStaticMeshComponent,
			// and UHierarchicalInstancedStaticMeshComponent
			ComponentToRemove = NextChild;
		}
		else if (NextChild->IsA<UHoudiniStaticMeshComponent>())
		{
			ComponentToRemove = NextChild;
		}
		else if (NextChild->IsA<USplineComponent>())
		{
			ComponentToRemove = NextChild;
		}
		else if (NextChild->IsA<UHoudiniMeshSplitInstancerComponent>())
		{
			ComponentToRemove = NextChild;
		}
		else if (NextChild->IsA<UHoudiniInstancedActorComponent>())
		{
			// The actors attched to the HoudiniAssetActor are not duplicated, so we only 
			// have to handle the component.
			ComponentToRemove = NextChild;
		}
		/*  do not destroy attached duplicated editable curves, they are needed to restore editable curves
		else if (NextChild->IsA<UHoudiniSplineComponent>())  
		{
			// Remove duplicated editable curve output's Houdini Spline Component, since they will be re-built at duplication.
			UHoudiniSplineComponent * HoudiniSplineComponent = Cast<UHoudiniSplineComponent>(NextChild);
			if (HoudiniSplineComponent && HoudiniSplineComponent->IsEditableOutputCurve())
				ComponentToRemove = NextChild;
		}
		*/
		if (ComponentToRemove)
		{
			ComponentToRemove->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			ComponentToRemove->UnregisterComponent();
			ComponentToRemove->DestroyComponent();
		}
	}

	// if there is an associated PDG asset link, call its UpdatePostDuplicate to cleanup references to
	// to the original instance's PDG output actors
	if (IsValid(PDGAssetLink))
	{
		PDGAssetLink->UpdatePostDuplicate();
	}
	
	SetHasBeenDuplicated(false);
}

bool UHoudiniAssetComponent::IsInputTypeSupported(EHoudiniInputType InType) const
{
	return true;
}

bool UHoudiniAssetComponent::IsOutputTypeSupported(EHoudiniOutputType InType) const
{
	return true;
}

bool
UHoudiniAssetComponent::IsPreview() const
{
	return bCachedIsPreview;
}

bool UHoudiniAssetComponent::IsValidComponent() const
{
	return true;
}

void UHoudiniAssetComponent::OnFullyLoaded()
{
	bFullyLoaded = true;
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
 	Super::OnComponentCreated();
	/*
	if (!GetOwner() || !GetOwner()->GetWorld())
		return;

	if (StaticMeshes.Num() == 0)
	{
		// Create Houdini logo static mesh and component for it.
		CreateStaticMeshHoudiniLogoResource(StaticMeshes);
	}

	// Create replacement material object.
	if (!HoudiniAssetComponentMaterials)
	{
		HoudiniAssetComponentMaterials =
			NewObject< UHoudiniAssetComponentMaterials >(
				this, UHoudiniAssetComponentMaterials::StaticClass(), NAME_None, RF_Public | RF_Transactional);
	}
	*/
}

void
UHoudiniAssetComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{

	if (CanDeleteHoudiniNodes())
	{
	}

	// Unregister ourself so our houdini node can be deleted
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniComponent(this);

	HoudiniAsset = nullptr;

	// Clear Parameters
	for (UHoudiniParameter*& CurrentParm : Parameters)
	{
		if (IsValid(CurrentParm))
		{
			CurrentParm->ConditionalBeginDestroy();
		}
		else if (GetHACWorld() != nullptr && GetHACWorld()->WorldType != EWorldType::PIE)
		{
			// TODO unneeded log?
			// Avoid spamming that error when leaving PIE mode
			HOUDINI_LOG_WARNING(TEXT("%s: null parameter when clearing"), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
		}

		CurrentParm = nullptr;
	}

	Parameters.Empty();

	// Clear Inputs
	for (UHoudiniInput*&  CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		if (CurrentInput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
			continue;

		// Destroy connected Houdini asset.
		CurrentInput->ConditionalBeginDestroy();
		CurrentInput = nullptr;
	}

	Inputs.Empty();

	// Clear Output
	for (UHoudiniOutput*& CurrentOutput : Outputs)
	{
		if (!IsValid(CurrentOutput))
			continue;

		if (CurrentOutput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
			continue;

		// Destroy all Houdini created socket actors.
		TArray<AActor*> & CurCreatedSocketActors = CurrentOutput->GetHoudiniCreatedSocketActors();
		for (auto & CurCreatedActor : CurCreatedSocketActors) 
		{
			if (!IsValid(CurCreatedActor))
				continue;

			CurCreatedActor->Destroy();
		}
		CurCreatedSocketActors.Empty();

		// Detach all Houdini attached socket actors
		TArray<AActor*> & CurAttachedSocketActors = CurrentOutput->GetHoudiniAttachedSocketActors();
		for (auto & CurAttachedSocketActor : CurAttachedSocketActors) 
		{
			if (!IsValid(CurAttachedSocketActor))
				continue;

			CurAttachedSocketActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
		}
		CurAttachedSocketActors.Empty();

#if WITH_EDITOR
		// Cleanup landscape splines
		FHoudiniLandscapeRuntimeUtils::DeleteLandscapeSplineCookedData(CurrentOutput);

		// Cleanup landscapes
		FHoudiniLandscapeRuntimeUtils::DeleteLandscapeCookedData(CurrentOutput);

		// Clean up foliage instances
		for (auto& CurrentOutputObject : CurrentOutput->GetOutputObjects())
		{
			for(int Index = 0; Index < CurrentOutputObject.Value.OutputComponents.Num(); Index++)
			{
				auto Component = CurrentOutputObject.Value.OutputComponents[Index];
			    // Foliage instancers store a HISMC in the components
			    UHierarchicalInstancedStaticMeshComponent* FoliageHISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(Component);
			    if (!FoliageHISMC)
				    continue;

			    UStaticMesh* FoliageSM = FoliageHISMC->GetStaticMesh();
			    if (!IsValid(FoliageSM))
				    continue;

			    // If we are a foliage HISMC, then our owner is an Instanced Foliage Actor,
			    // if it is not, then we are just a "regular" HISMC
			    AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(FoliageHISMC->GetOwner());
			    if (!IsValid(InstancedFoliageActor))
				    continue;

			    UFoliageType *FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(FoliageSM);
			    if (!IsValid(FoliageType))
				    continue;

			    if (IsInGameThread() && IsGarbageCollecting())
			    {
				    // TODO: ??
				    // Calling DeleteInstancesForComponent during GC will cause unreal to crash... 
				    HOUDINI_LOG_WARNING(TEXT("%s: Unable to clear foliage instances because of GC"), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
			    }
			    else
			    {
				    // Clean up the instances generated for that component
				    InstancedFoliageActor->DeleteInstancesForComponent(this, FoliageType);
			    }

			    if (FoliageHISMC->GetInstanceCount() > 0)
			    {
				    // If the component still has instances left after the cleanup,
				    // make sure that we dont delete it, as the leftover instances are likely hand-placed
				    CurrentOutputObject.Value.OutputComponents[Index] = nullptr;
			    }
			    else
			    {
				    // Remove the foliage type if it doesn't have any more instances
				    InstancedFoliageActor->RemoveFoliageType(&FoliageType, 1);
			    }
			}
		}
#endif

		CurrentOutput->Clear();
		// Destroy connected Houdini asset.
		CurrentOutput->ConditionalBeginDestroy();
		CurrentOutput = nullptr;
	}

	Outputs.Empty();

	//FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(AssetId, true);
	// Unregister ourself so our houdini node can be delete.
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniComponent(this);

	// Clear the static mesh bake timer
	ClearRefineMeshesTimer();

	
	// Clear all TOP data and temporary geo/objects from the PDG asset link (if valid)
	if (IsValid(PDGAssetLink))
	{
#if WITH_EDITOR
		const UWorld* const World = GetHACWorld();
		if (IsValid(World))
		{
			// Only do this for editor worlds, only interactively (not during engine shutdown or garbage collection)
			if (World->WorldType == EWorldType::Editor && GIsRunning && !GIsGarbageCollecting)
			{
				// In case we are recording a transaction (undo, for example) notify that the object will be
				// modified.
				PDGAssetLink->Modify();
				PDGAssetLink->ClearAllTOPData();
			}
		}
#endif
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UHoudiniAssetComponent::RegisterHoudiniComponent(UHoudiniAssetComponent* InComponent)
{
	// Registration of this component is wrapped in this virtual function to allow
	// derived classed to override this behaviour.
	FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(InComponent);
}

void
UHoudiniAssetComponent::OnRegister()
{
	Super::OnRegister();

	// NOTE: Wait until HoudiniEngineTick() before deciding to mark this object as fully loaded
	// since preview components need to wait for component templates to finish their initialization
	// before being able to perform state transfers.
}

UHoudiniParameter*
UHoudiniAssetComponent::FindMatchingParameter(UHoudiniParameter* InOtherParam)
{
	if (!IsValid(InOtherParam))
		return nullptr;

	for (auto CurrentParam : Parameters)
	{
		if (!IsValid(CurrentParam))
			continue;

		if (CurrentParam->Matches(*InOtherParam))
			return CurrentParam;
	}

	return nullptr;
}

UHoudiniInput*
UHoudiniAssetComponent::FindMatchingInput(UHoudiniInput* InOtherInput)
{
	if (!IsValid(InOtherInput))
		return nullptr;

	for (auto CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		if (CurrentInput->Matches(*InOtherInput))
			return CurrentInput;
	}

	return nullptr;
}

UHoudiniHandleComponent* 
UHoudiniAssetComponent::FindMatchingHandle(UHoudiniHandleComponent* InOtherHandle) 
{
	if (!IsValid(InOtherHandle))
		return nullptr;

	for (auto CurrentHandle : HandleComponents) 
	{
		if (!IsValid(CurrentHandle))
			continue;

		if (CurrentHandle->Matches(*InOtherHandle))
			return CurrentHandle;
	}

	return nullptr;
}

UHoudiniParameter*
UHoudiniAssetComponent::FindParameterByName(const FString& InParamName)
{
	for (auto CurrentParam : Parameters)
	{
		if (!IsValid(CurrentParam))
			continue;

		if (CurrentParam->GetParameterName().Equals(InParamName))
			return CurrentParam;
	}

	return nullptr;
}


void
UHoudiniAssetComponent::OnChildAttached(USceneComponent* ChildComponent)
{
	Super::OnChildAttached(ChildComponent);

	// ... Do corresponding things for other houdini component types.
	// ...
}


void
UHoudiniAssetComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

#if WITH_EDITOR
	if (!bUploadTransformsToHoudiniEngine)
		return;

	if (!GetComponentTransform().Equals(LastComponentTransform))
	{
		// Only set transform changed flag if the transform actually changed.
		// WorldComposition can call ApplyWorldOffset with a zero vector (for example during a map save)
		// which triggers unexpected recooks.
		SetHasComponentTransformChanged(true);
	}
#endif

}

void UHoudiniAssetComponent::HoudiniEngineTick()
{
	if (!IsFullyLoaded())
	{
		OnFullyLoaded();
	}
}


#if WITH_EDITOR
void
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty * Property = PropertyChangedEvent.MemberProperty;
	if (!Property)
		return;

	FName PropertyName = Property->GetFName();

	// Changing the Houdini Asset?
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, HoudiniAsset))
	{
		OnHoudiniAssetChanged();
	}
	else if (PropertyName == GetRelativeLocationPropertyName()
			|| PropertyName == GetRelativeRotationPropertyName()
			|| PropertyName == GetRelativeScale3DPropertyName())
	{
		SetHasComponentTransformChanged(true);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bOverrideGlobalProxyStaticMeshSettings)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bEnableProxyStaticMeshRefinementByTimerOverride)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, ProxyMeshAutoRefineTimeoutSecondsOverride))
	{
		ClearRefineMeshesTimer();
		// Reset the timer
		// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
		SetRefineMeshesTimer();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, Mobility))
	{
		// Changed GetAttachChildren to 'GetAllDescendants' due to HoudiniMeshSplitInstanceComponent 
		// not propagating property changes to their own child StaticMeshComponents.
		TArray< USceneComponent * > LocalAttachChildren;
		GetChildrenComponents(true, LocalAttachChildren);

		// Mobility was changed, we need to update it for all attached components as well.
		for (TArray< USceneComponent * >::TConstIterator Iter(LocalAttachChildren); Iter; ++Iter)
		{
			USceneComponent * SceneComponent = *Iter;
			SceneComponent->SetMobility(Mobility);
		}
	}
	else if (PropertyName == TEXT("bVisible"))
	{
		// Visibility has changed, propagate it to children.
		SetVisibility(IsVisible(), true);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bHiddenInGame))
	{
		// Visibility has changed, propagate it to children.
		SetHiddenInGame(bHiddenInGame, true);
	}
	else
	{
		// TODO:
		// Propagate properties (mobility/visibility etc.. to children components)
		// Look in v1 for: if (Property->HasMetaData(TEXT("Category"))) {} and HOUDINI_UPDATE_ALL_CHILD_COMPONENTS
	}

	if (Property->HasMetaData(TEXT("Category")))
	{
		const FString & Category = Property->GetMetaData(TEXT("Category"));
		static const FString CategoryHoudiniGeneratedStaticMeshSettings = TEXT("HoudiniMeshGeneration");
		static const FString CategoryLighting = TEXT("Lighting");
		static const FString CategoryRendering = TEXT("Rendering");
		static const FString CategoryCollision = TEXT("Collision");
		static const FString CategoryPhysics = TEXT("Physics");
		static const FString CategoryLOD = TEXT("LOD");

		if (CategoryHoudiniGeneratedStaticMeshSettings == Category)
		{
			// We are changing one of the mesh generation properties, we need to update all static meshes.
			// As the StaticMeshComponents map contains only top-level static mesh components only, use the StaticMeshes map instead
			for (UHoudiniOutput* CurOutput : Outputs)
			{
				if (!CurOutput)
					continue;

				for (auto& Pair : CurOutput->GetOutputObjects())
				{
					UStaticMesh* StaticMesh = Cast<UStaticMesh>(Pair.Value.OutputObject);
					if (!IsValid(StaticMesh))
						continue;

					SetStaticMeshGenerationProperties(StaticMesh);
					FHoudiniScopedGlobalSilence ScopedGlobalSilence;
					StaticMesh->Build(true);
					RefreshCollisionChange(*StaticMesh);
				}
			}

			return;
		}
		else if (CategoryLighting == Category)
		{
			if (Property->GetName() == TEXT("CastShadow"))
			{
				// Stop cast-shadow being applied to invisible colliders children
				// This prevent colliders only meshes from casting shadows
				TArray<UActorComponent*> ReregisterComponents;
				{
					TArray<USceneComponent *> LocalAttachChildren;
					GetChildrenComponents(true, LocalAttachChildren);
					for (TArray< USceneComponent * >::TConstIterator Iter(LocalAttachChildren); Iter; ++Iter)
					{
						UStaticMeshComponent * Component = Cast< UStaticMeshComponent >(*Iter);
						if (!IsValid(Component))
							continue;

						/*const FHoudiniGeoPartObject * pGeoPart = StaticMeshes.FindKey(Component->GetStaticMesh());
						if (pGeoPart && pGeoPart->IsCollidable())
						{
							// This is an invisible collision mesh:
							// Do not interfere with lightmap builds - disable shadow casting
							Component->SetCastShadow(false);
						}
						else*/
						{
							// Set normally
							Component->SetCastShadow(CastShadow);
						}

						ReregisterComponents.Add(Component);
					}
				}

				if (ReregisterComponents.Num() > 0)
				{
					FMultiComponentReregisterContext MultiComponentReregisterContext(ReregisterComponents);
				}
			}
			else if (Property->GetName() == TEXT("bCastDynamicShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastDynamicShadow);
			}
			else if (Property->GetName() == TEXT("bCastStaticShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastStaticShadow);
			}
			else if (Property->GetName() == TEXT("bCastVolumetricTranslucentShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastVolumetricTranslucentShadow);
			}
			else if (Property->GetName() == TEXT("bCastInsetShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastInsetShadow);
			}
			else if (Property->GetName() == TEXT("bCastHiddenShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastHiddenShadow);
			}
			else if (Property->GetName() == TEXT("bCastShadowAsTwoSided"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastShadowAsTwoSided);
			}
			/*else if ( Property->GetName() == TEXT( "bLightAsIfStatic" ) )
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bLightAsIfStatic );
			}*/
			else if (Property->GetName() == TEXT("bLightAttachmentsAsGroup"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bLightAttachmentsAsGroup);
			}
			else if (Property->GetName() == TEXT("IndirectLightingCacheQuality"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, IndirectLightingCacheQuality);
			}
		}
		else if (CategoryRendering == Category)
		{
			if (Property->GetName() == TEXT("bVisibleInReflectionCaptures"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bVisibleInReflectionCaptures);
			}
			else if (Property->GetName() == TEXT("bRenderInMainPass"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bRenderInMainPass);
			}
			/*
			else if ( Property->GetName() == TEXT( "bRenderInMono" ) )
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bRenderInMono );
			}
			*/
			else if (Property->GetName() == TEXT("bOwnerNoSee"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bOwnerNoSee);
			}
			else if (Property->GetName() == TEXT("bOnlyOwnerSee"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bOnlyOwnerSee);
			}
			else if (Property->GetName() == TEXT("bTreatAsBackgroundForOcclusion"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bTreatAsBackgroundForOcclusion);
			}
			else if (Property->GetName() == TEXT("bUseAsOccluder"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bUseAsOccluder);
			}
			else if (Property->GetName() == TEXT("bRenderCustomDepth"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bRenderCustomDepth);
			}
			else if (Property->GetName() == TEXT("CustomDepthStencilValue"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, CustomDepthStencilValue);
			}
			else if (Property->GetName() == TEXT("CustomDepthStencilWriteMask"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, CustomDepthStencilWriteMask);
			}
			else if (Property->GetName() == TEXT("TranslucencySortPriority"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, TranslucencySortPriority);
			}
			else if (Property->GetName() == TEXT("bReceivesDecals"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bReceivesDecals);
			}
			else if (Property->GetName() == TEXT("BoundsScale"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, BoundsScale);
			}
			else if (Property->GetName() == TEXT("bUseAttachParentBound"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(USceneComponent, bUseAttachParentBound);
			}
		}
		else if (CategoryCollision == Category)
		{
			if (Property->GetName() == TEXT("bAlwaysCreatePhysicsState"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bAlwaysCreatePhysicsState);
			}
			/*else if ( Property->GetName() == TEXT( "bGenerateOverlapEvents" ) )
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bGenerateOverlapEvents );
			}*/
			else if (Property->GetName() == TEXT("bMultiBodyOverlap"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bMultiBodyOverlap);
			}
			/*
			else if ( Property->GetName() == TEXT( "bCheckAsyncSceneOnMove" ) )
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCheckAsyncSceneOnMove );
			}
			*/
			else if (Property->GetName() == TEXT("bTraceComplexOnMove"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bTraceComplexOnMove);
			}
			else if (Property->GetName() == TEXT("bReturnMaterialOnMove"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bReturnMaterialOnMove);
			}
			else if (Property->GetName() == TEXT("BodyInstance"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, BodyInstance);
			}
			else if (Property->GetName() == TEXT("CanCharacterStepUpOn"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, CanCharacterStepUpOn);
			}
			/*else if ( Property->GetName() == TEXT( "bCanEverAffectNavigation" ) )
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UActorComponent, bCanEverAffectNavigation );
			}*/
		}
		else if (CategoryPhysics == Category)
		{
			if (Property->GetName() == TEXT("bIgnoreRadialImpulse"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bIgnoreRadialImpulse);
			}
			else if (Property->GetName() == TEXT("bIgnoreRadialForce"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bIgnoreRadialForce);
			}
			else if (Property->GetName() == TEXT("bApplyImpulseOnDamage"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bApplyImpulseOnDamage);
			}
			/*
			else if ( Property->GetName() == TEXT( "bShouldUpdatePhysicsVolume" ) )
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( USceneComponent, bShouldUpdatePhysicsVolume );
			}
			*/
		}
		else if (CategoryLOD == Category)
		{
			if (Property->GetName() == TEXT("MinDrawDistance"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, MinDrawDistance);
			}
			else if (Property->GetName() == TEXT("LDMaxDrawDistance"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, LDMaxDrawDistance);
			}
			else if (Property->GetName() == TEXT("CachedMaxDrawDistance"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, CachedMaxDrawDistance);
			}
			else if (Property->GetName() == TEXT("bAllowCullDistanceVolume"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bAllowCullDistanceVolume);
			}
			else if (Property->GetName() == TEXT("DetailMode"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(USceneComponent, DetailMode);
			}
		}
	}
}
#endif


#if WITH_EDITOR
void
UHoudiniAssetComponent::PostEditUndo()
{
	Super::PostEditUndo();

	if (IsValid(this))
	{
		// Make sure we are registered with the HER singleton
		// We could be undoing a HoudiniActor delete
		if (!FHoudiniEngineRuntime::Get().IsComponentRegistered(this))
		{
			MarkAsNeedInstantiation();

			// Component has been loaded, not duplicated
			bHasBeenDuplicated = false;

			RegisterHoudiniComponent(this);
		}
	}
}

#endif

bool
UHoudiniAssetComponent::ShouldTryToStartFirstSession() const
{
	if (!HoudiniAsset)
		return false;

	// Only try to start the default session if we have an "active" HAC
	switch (AssetState)
	{
		case EHoudiniAssetState::NewHDA:
		case EHoudiniAssetState::PreInstantiation:
		case EHoudiniAssetState::Instantiating:
		case EHoudiniAssetState::PreCook:
		case EHoudiniAssetState::Cooking:
			return true;

		case EHoudiniAssetState::NeedInstantiation:
		case EHoudiniAssetState::PostCook:
		case EHoudiniAssetState::PreProcess:
		case EHoudiniAssetState::Processing:
		case EHoudiniAssetState::None:
		case EHoudiniAssetState::NeedRebuild:
		case EHoudiniAssetState::NeedDelete:
		case EHoudiniAssetState::Deleting:
		case EHoudiniAssetState::ProcessTemplate:
		case EHoudiniAssetState::Dormant:
			return false;
	};

	return false;
}


#if WITH_EDITOR
void
UHoudiniAssetComponent::OnActorMoved(AActor* Actor)
{
	if (GetOwner() != Actor)
		return;

	SetHasComponentTransformChanged(true);
}
#endif

void 
UHoudiniAssetComponent::SetHasComponentTransformChanged(const bool& InHasChanged)
{
	// Only update the value if we're fully loaded
	// This avoid triggering a recook when loading a level
	if(bFullyLoaded)
	{
		bHasComponentTransformChanged = InHasChanged;
		LastComponentTransform = GetComponentTransform();
	}
}

void UHoudiniAssetComponent::SetOutputNodeIds(const TArray<int32>& OutputNodes)
{
	NodeIdsToCook = OutputNodes;
	// Remove stale entries from OutputNodeCookCounts:
	TArray<int32> CachedNodeIds;
	OutputNodeCookCounts.GetKeys(CachedNodeIds);
	for(const int32 NodeId : CachedNodeIds)
	{
		if (!NodeIdsToCook.Contains(NodeId))
		{
			OutputNodeCookCounts.Remove(NodeId);
		}
	}
}

void UHoudiniAssetComponent::SetOutputNodeCookCount(const int& NodeId, const int& CookCount)
{
	OutputNodeCookCounts.Add(NodeId, CookCount);
}

bool UHoudiniAssetComponent::HasOutputNodeChanged(const int& NodeId, const int& NewCookCount)
{
	if (!OutputNodeCookCounts.Contains(NodeId))
	{
		return true;
	}
	if (OutputNodeCookCounts[NodeId] == NewCookCount)
	{
		return false;
	}
	return true;
}

void UHoudiniAssetComponent::ClearOutputNodes()
{
	NodeIdsToCook.Empty();
	ClearOutputNodesCookCount();
}

void UHoudiniAssetComponent::ClearOutputNodesCookCount()
{
	OutputNodeCookCounts.Empty();
}

void
UHoudiniAssetComponent::SetPDGAssetLink(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	// Check the object validity
	if (!IsValid(InPDGAssetLink))
		return;

	// If it is the same object, do nothing.
	if (InPDGAssetLink == PDGAssetLink)
		return;

	PDGAssetLink = InPDGAssetLink;
}


FBoxSphereBounds
UHoudiniAssetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds LocalBounds;
	FBox BoundingBox = GetAssetBounds(nullptr, false);
	if (BoundingBox.GetExtent() == FVector3d::ZeroVector)
		BoundingBox = BoundingBox.ExpandBy(1.0);

	LocalBounds = FBoxSphereBounds(BoundingBox);
	// fix for offset bounds - maintain local bounds origin
	LocalBounds.TransformBy(LocalToWorld);

	const auto & LocalAttachedChildren = GetAttachChildren();
	for (int32 Idx = 0; Idx < LocalAttachedChildren.Num(); ++Idx)
	{
		if (!LocalAttachedChildren[Idx])
			continue;

		FBoxSphereBounds ChildBounds = LocalAttachedChildren[Idx]->CalcBounds(LocalToWorld);
		if (!ChildBounds.ContainsNaN())
			LocalBounds = LocalBounds + ChildBounds;
	}

	return LocalBounds;
}


FBox
UHoudiniAssetComponent::GetAssetBounds(UHoudiniInput* IgnoreInput, bool bIgnoreGeneratedLandscape) const
{
	FBox BoxBounds(ForceInitToZero);

	// This function may be called during destruction of the HAC, when the world is not set, so gracefully
	// do nothing in this case.
	if (!IsValid(this->GetHACWorld()))
		return BoxBounds;

	/*
	// Commented out: Creates incorrect focus bounds..
	// Query the bounds for all output objects
	for (auto & CurOutput : Outputs) 
	{
		if (!IsValid(CurOutput))
			continue;

		BoxBounds += CurOutput->GetBounds();
	}
	*/

	// Query the bounds for all our inputs
	// Bug: 134158: For some reason using inputs in this manner during cooking will crash the cooker
	// when using World Partition. So ignore inputs during cooking.
	if (!IsRunningCookCommandlet())
	{
		for (auto& CurInput : Inputs)
		{
			if (!IsValid(CurInput))
				continue;

			BoxBounds += CurInput->GetBounds(this->GetHACWorld());
		}
	}

	// Query the bounds for all input parameters
	for (auto & CurParam : Parameters) 
	{
		if (!IsValid(CurParam))
			continue;

		if (CurParam->GetParameterType() != EHoudiniParameterType::Input)
			continue;

		UHoudiniParameterOperatorPath* InputParam = Cast<UHoudiniParameterOperatorPath>(CurParam);
		if (!IsValid(CurParam))
			continue;

		if (!InputParam->HoudiniInput.IsValid())
			continue;

		BoxBounds += InputParam->HoudiniInput.Get()->GetBounds(this->GetHACWorld());
	}

	// Query the bounds for all our Houdini handles
	for (auto & CurHandleComp : HandleComponents)
	{
		if (!IsValid(CurHandleComp))
			continue;

		BoxBounds += CurHandleComp->GetBounds();
	}
	/*
	// Commented out: Creates incorrect focus bounds..
	// Also scan all our decendants for SMC bounds not just top-level children
	// ( split mesh instances' mesh bounds were not gathered properly )
	TArray<USceneComponent*> LocalAttachedChildren;
	LocalAttachedChildren.Reserve(16);
	GetChildrenComponents(true, LocalAttachedChildren);
	for (int32 Idx = 0; Idx < LocalAttachedChildren.Num(); ++Idx)
	{
		if (!LocalAttachedChildren[Idx])
			continue;

		USceneComponent * pChild = LocalAttachedChildren[Idx];
		if (UStaticMeshComponent * StaticMeshComponent = Cast<UStaticMeshComponent>(pChild))
		{
			if (!IsValid(StaticMeshComponent))
				continue;

			FBox StaticMeshBounds = StaticMeshComponent->Bounds.GetBox();
			if (StaticMeshBounds.IsValid)
				BoxBounds += StaticMeshBounds;
		}
	}
	*/

	// If nothing was found, init with the asset's location
	if (BoxBounds.GetVolume() == 0.0f)
		BoxBounds += GetComponentLocation();

	return BoxBounds;
}

void
UHoudiniAssetComponent::ClearRefineMeshesTimer()
{
	UWorld *World = GetHACWorld();
	if (!World)
	{
		//HOUDINI_LOG_ERROR(TEXT("Cannot ClearRefineMeshesTimer, World is nullptr!"));
		return;
	}
	
	World->GetTimerManager().ClearTimer(RefineMeshesTimer);
}

void
UHoudiniAssetComponent::SetRefineMeshesTimer()
{
	UWorld* World = GetHACWorld();
	if (!World)
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot SetRefineMeshesTimer, World is nullptr!"));
		return;
	}

	// Check if timer-based proxy mesh refinement is enable for this component
	const bool bEnableTimer = IsProxyStaticMeshRefinementByTimerEnabled();
	const float TimeSeconds = GetProxyMeshAutoRefineTimeoutSeconds();
	if (bEnableTimer)
	{
		World->GetTimerManager().SetTimer(RefineMeshesTimer, this, &UHoudiniAssetComponent::OnRefineMeshesTimerFired, 1.0f, false, TimeSeconds);
	}
	else
	{
		World->GetTimerManager().ClearTimer(RefineMeshesTimer);
	}
}

void 
UHoudiniAssetComponent::OnRefineMeshesTimerFired()
{
	HOUDINI_LOG_MESSAGE(TEXT("UHoudiniAssetComponent::OnRefineMeshesTimerFired()"));
	if (OnRefineMeshesTimerDelegate.IsBound())
	{
		OnRefineMeshesTimerDelegate.Broadcast(this);
	}
}

bool
UHoudiniAssetComponent::HasAnyCurrentProxyOutput() const
{
	for (const UHoudiniOutput *Output : Outputs)
	{
		if (Output->HasAnyCurrentProxy())
		{
			return true;
		}
	}

	return false;
}

bool
UHoudiniAssetComponent::HasAnyProxyOutput() const
{
	for (const UHoudiniOutput *Output : Outputs)
	{
		if (Output->HasAnyProxy())
		{
			return true;
		}
	}

	return false;
}

bool
UHoudiniAssetComponent::HasAnyOutputComponent() const
{
	for (UHoudiniOutput *Output : Outputs)
	{
		for(auto& CurrentOutputObject : Output->GetOutputObjects())
		{
			for(auto Component : CurrentOutputObject.Value.OutputComponents)
			{
			    if(Component)
				    return true;
			}
		}
	}

	return false;
}

bool
UHoudiniAssetComponent::HasOutputObject(UObject* InOutputObjectToFind) const
{
	for (const auto& CurOutput : Outputs)
	{
		for (const auto& CurOutputObject : CurOutput->GetOutputObjects())
		{
			if (CurOutputObject.Value.OutputObject == InOutputObjectToFind)
				return true;
			if (CurOutputObject.Value.ProxyObject == InOutputObjectToFind)
				return true;
			if (CurOutputObject.Value.ProxyComponent == InOutputObjectToFind)
				return true;

			for(auto Component : CurOutputObject.Value.OutputComponents)
			{
				if (Component == InOutputObjectToFind)
				    return true;
			}

		}
	}

	return false;
}

bool
UHoudiniAssetComponent::IsHoudiniCookedDataAvailable(bool &bOutNeedsRebuildOrDelete, bool &bOutInvalidState) const
{
	// Get the state of the asset and check if it is pre-cook, cooked, pending delete/rebuild or invalid
	bOutNeedsRebuildOrDelete = false;
	bOutInvalidState = false;
	switch (AssetState)
	{
	case EHoudiniAssetState::NewHDA:
	case EHoudiniAssetState::NeedInstantiation:
	case EHoudiniAssetState::PreInstantiation:
	case EHoudiniAssetState::Instantiating:
	case EHoudiniAssetState::PreCook:
	case EHoudiniAssetState::Cooking:
	case EHoudiniAssetState::PostCook:
	case EHoudiniAssetState::PreProcess:
	case EHoudiniAssetState::Processing:
		return false;
		break;
	case EHoudiniAssetState::None:
		return true;
		break;
	case EHoudiniAssetState::NeedRebuild:
	case EHoudiniAssetState::NeedDelete:
	case EHoudiniAssetState::Deleting:
		bOutNeedsRebuildOrDelete = true;
		break;
	default:
		bOutInvalidState = true;
		break;
	}

	return false;
}

bool
UHoudiniAssetComponent::IsComponentValid() const
{
	if (!IsValidLowLevel())
		return false;

	if (IsTemplate())
		return false;

	if (IsUnreachable())
		return false;

	if (!GetOuter()) //|| !GetOuter()->GetLevel() )
		return false;

	return true;
}

bool
UHoudiniAssetComponent::IsInstantiatingOrCooking() const
{
	return HapiGUID.IsValid();
}


void
UHoudiniAssetComponent::SetStaticMeshGenerationProperties(UStaticMesh* InStaticMesh) const
{
#if WITH_EDITOR
	if (!InStaticMesh)
		return;

	// Make sure static mesh has a new lighting guid.
	InStaticMesh->SetLightingGuid(FGuid::NewGuid());
	InStaticMesh->LODGroup = NAME_None;

	// Set resolution of lightmap.
	InStaticMesh->SetLightMapResolution(StaticMeshGenerationProperties.GeneratedLightMapResolution);

	const FStaticMeshRenderData* InRenderData = InStaticMesh->GetRenderData();
	// Set the global light map coordinate index if it looks valid
	if (InRenderData && InRenderData->LODResources.Num() > 0)
	{
		int32 NumUVs = InRenderData->LODResources[0].GetNumTexCoords();
		if (NumUVs > StaticMeshGenerationProperties.GeneratedLightMapCoordinateIndex)
		{
			InStaticMesh->SetLightMapCoordinateIndex(StaticMeshGenerationProperties.GeneratedLightMapCoordinateIndex);
		}
	}

	// TODO
	// Set method for LOD texture factor computation.
	//InStaticMesh->bUseMaximumStreamingTexelRatio = StaticMeshGenerationProperties.bGeneratedUseMaximumStreamingTexelRatio;
	// Set distance where textures using UV 0 are streamed in/out.  - GOES ON COMPONENT
	// InStaticMesh->StreamingDistanceMultiplier = StaticMeshGenerationProperties.GeneratedStreamingDistanceMultiplier;
	
	// Add user data.
	for (int32 AssetUserDataIdx = 0; AssetUserDataIdx < StaticMeshGenerationProperties.GeneratedAssetUserData.Num(); AssetUserDataIdx++)
		InStaticMesh->AddAssetUserData(StaticMeshGenerationProperties.GeneratedAssetUserData[AssetUserDataIdx]);

	// Create a body setup if needed
	if (!InStaticMesh->GetBodySetup())
		InStaticMesh->CreateBodySetup();

	UBodySetup* BodySetup = InStaticMesh->GetBodySetup();
	if (!BodySetup)
		return;

	// Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
	BodySetup->bDoubleSidedGeometry = StaticMeshGenerationProperties.bGeneratedDoubleSidedGeometry;

	// Assign physical material for simple collision.
	BodySetup->PhysMaterial = StaticMeshGenerationProperties.GeneratedPhysMaterial;

	BodySetup->DefaultInstance.CopyBodyInstancePropertiesFrom(&StaticMeshGenerationProperties.DefaultBodyInstance);

	// Assign collision trace behavior.
	BodySetup->CollisionTraceFlag = StaticMeshGenerationProperties.GeneratedCollisionTraceFlag;

	// Assign walkable slope behavior.
	BodySetup->WalkableSlopeOverride = StaticMeshGenerationProperties.GeneratedWalkableSlopeOverride;

	// We want to use all of geometry for collision detection purposes.
	BodySetup->bMeshCollideAll = true;

#endif
}


void
UHoudiniAssetComponent::UpdatePhysicsState()
{
	// Update physics representation right away.
	RecreatePhysicsState();

	// Changed GetAttachChildren to 'GetAllDescendants' due to HoudiniMeshSplitInstanceComponent
	// not propagating property changes to their own child StaticMeshComponents.
	TArray<USceneComponent *> LocalAttachChildren;
	GetChildrenComponents(true, LocalAttachChildren);
	for (TArray<USceneComponent *>::TConstIterator Iter(LocalAttachChildren); Iter; ++Iter)
	{
		USceneComponent * SceneComponent = *Iter;
		if (IsValid(SceneComponent))
		{
			SceneComponent->RecreatePhysicsState();
		}
	}

	// !!! Do not call UpdateBounds() here as this could cause
	// a loading loop in post load on game builds! 
}


FPrimitiveSceneProxy*
UHoudiniAssetComponent::CreateSceneProxy()
{
	/** Represents a UHoudiniAssetComponent to the scene manager. */
	class FHoudiniAssetSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		FHoudiniAssetSceneProxy(const UHoudiniAssetComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
		{
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
		uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }
	};

	return new FHoudiniAssetSceneProxy(this);
}

void
UHoudiniAssetComponent::SetAssetState(EHoudiniAssetState InNewState)
{
	const EHoudiniAssetState OldState = AssetState;
	AssetState = InNewState;

#if WITH_EDITOR
	IHoudiniEditorAssetStateSubsystemInterface* const EditorSubsystem = IHoudiniEditorAssetStateSubsystemInterface::Get(); 
	if (EditorSubsystem)
		EditorSubsystem->NotifyOfHoudiniAssetStateChange(this, OldState, InNewState);
#endif
	HandleOnHoudiniAssetStateChange(this, OldState, InNewState);
}

void
UHoudiniAssetComponent::HandleOnHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState)
{
	IHoudiniAssetStateEvents::HandleOnHoudiniAssetStateChange(InHoudiniAssetContext, InFromState, InToState);
	
	if (InFromState == InToState)
		return;

	if (this != InHoudiniAssetContext)
		return;

	FOnAssetStateChangeDelegate& StateChangeDelegate = GetOnAssetStateChangeDelegate();
	if (StateChangeDelegate.IsBound())
		StateChangeDelegate.Broadcast(this, InFromState, InToState);

	if (InToState == EHoudiniAssetState::PreInstantiation)
	{
		HandleOnPreInstantiation();
	}
	
	if (InToState == EHoudiniAssetState::PreCook)
	{
		HandleOnPreCook();
	}

	if (InToState == EHoudiniAssetState::PostCook)
	{
		HandleOnPostCook();
	}
		
}

void UHoudiniAssetComponent::HandleOnPreInstantiation()
{
	if (OnPreInstantiationDelegate.IsBound())
		OnPreInstantiationDelegate.Broadcast(this);
}

void
UHoudiniAssetComponent::HandleOnPreCook()
{
	// Process the PreCookCallbacks array first
	for(auto CallbackFn : PreCookCallbacks)
	{
		CallbackFn(this);
	}
	PreCookCallbacks.Empty();
	
	if (OnPreCookDelegate.IsBound())
		OnPreCookDelegate.Broadcast(this);
}

void
UHoudiniAssetComponent::HandleOnPostCook()
{
	if (OnPostCookDelegate.IsBound())
		OnPostCookDelegate.Broadcast(this, bLastCookSuccess);
}

void
UHoudiniAssetComponent::HandleOnPreOutputProcessing()
{
	if (OnPreOutputProcessingDelegate.IsBound())
	{
		OnPreOutputProcessingDelegate.Broadcast(this, true);
	}
}

void
UHoudiniAssetComponent::HandleOnPostOutputProcessing()
{
	if (OnPostOutputProcessingDelegate.IsBound())
	{
		OnPostOutputProcessingDelegate.Broadcast(this, true);
	}
}

void
UHoudiniAssetComponent::HandleOnPostBake(bool bInSuccess)
{
	if (OnPostBakeDelegate.IsBound())
		OnPostBakeDelegate.Broadcast(this, bInSuccess);
}

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
ILevelInstanceInterface*
UHoudiniAssetComponent::GetLevelInstance() const
{
	// Find the level instanced which "owns" this HDA, if it exists.

	AActor* Actor = Cast<AActor>(this->GetOwner());
	if (!Actor)
		return nullptr;

	UWorld* World = Actor->GetWorld();
	if (!World)
		return nullptr;

	ULevelInstanceSubsystem* LevelInstanceSystem = World->GetSubsystem<ULevelInstanceSubsystem>();
	if (!LevelInstanceSystem)
		return nullptr;

	return LevelInstanceSystem->GetOwningLevelInstance(Actor->GetLevel());
}
#endif

void
UHoudiniAssetComponent::UpdateDormantStatus()
{
#if WITH_EDITOR
	// This function checks if we should go into or out of doermant status.
#if (ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION < 1)
	return;
#else
	ILevelInstanceInterface* LevelInstance = GetLevelInstance();
	if (!LevelInstance)
		return;

	if (GetAssetState() == EHoudiniAssetState::Dormant)
	{
		// If this HDA was previously dormant, and the level instance is editable, it means
		// the level instance has just been made editable. So reset to a state where the HDA
		// can be used.
		if (LevelInstance->IsEditing())
			this->SetAssetState(EHoudiniAssetState::None);
	}
	else if (GetAssetState() == EHoudiniAssetState::None)
	{
		// If we're not doing anything, and the level instance not editable, flip the state
		// back to dormant. This highlights a potential problem that the user could  commit
		// a level instance before its finished cooking, but I'm not sure we can prevent that.
		if (!LevelInstance->IsEditing())
			this->SetAssetState(EHoudiniAssetState::Dormant);
	}
#endif
#endif
}
