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
#include "HoudiniCookable.h"
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
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	bool bLegacyComponent = false;
	bool bV2Component = false;
	if (Ar.IsLoading())
	{
		int32 Ver = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE && Ver >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
		{
			bLegacyComponent = true;
		}
		else if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V3_BASE)
		{
			// V2 HAC component - enable this so we can transfer our data to the cookable during PostLoad()
			bMigrateDataToCookableOnPostLoad = true;

			HOUDINI_LOG_MESSAGE(TEXT("Loading deprecated version of UHoudiniAssetComponent : V2 HAC will be converted to Cookable."));
		}
	}

	// 
	Super::Serialize(Ar);

	if (bLegacyComponent)
	{
		int64 InitialOffset = Ar.Tell();

		// We will just skip the v1 HAC data
		HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniAssetComponent : serialization will be skipped."));

		// Skip old Serialized data
		if (FLinker* Linker = Ar.GetLinker())
		{
			int32 const ExportIndex = this->GetLinkerIndex();
			FObjectExport& Export = Linker->ExportMap[ExportIndex];
			Ar.Seek(InitialOffset + Export.SerialSize);
			return;
		}
	}
}


UHoudiniAssetComponent::UHoudiniAssetComponent(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	HoudiniAsset_DEPRECATED = nullptr;	
	bCookOnParameterChange_DEPRECATED = true;
	bUploadTransformsToHoudiniEngine_DEPRECATED = true;
	bCookOnTransformChange_DEPRECATED = false;
	//bUseNativeHoudiniMaterials = true;
	bCookOnAssetInputCook_DEPRECATED = true;

	AssetId_DEPRECATED = -1;
	AssetState_DEPRECATED = EHoudiniAssetState::NewHDA;
	AssetStateResult_DEPRECATED = EHoudiniAssetStateResult::None;
	AssetCookCount_DEPRECATED = 0;
	
	SubAssetIndex_DEPRECATED = -1;

	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID_DEPRECATED.Invalidate();

	HapiAssetName_DEPRECATED = FString();

	// Create unique component GUID.
	ComponentGUID_DEPRECATED = FGuid::NewGuid();
	LastComponentTransform_DEPRECATED = FTransform();

	bUploadTransformsToHoudiniEngine_DEPRECATED = true;

	bHasBeenLoaded_DEPRECATED = false;
	bHasBeenDuplicated_DEPRECATED = false;
	bPendingDelete_DEPRECATED = false;
	bRecookRequested_DEPRECATED = false;
	bRebuildRequested_DEPRECATED = false;
	bEnableCooking_DEPRECATED = true;
	bForceNeedUpdate_DEPRECATED = false;
	bLastCookSuccess_DEPRECATED = false;
	bBlueprintStructureModified = false;
	bBlueprintModified = false;

	//bEditorPropertiesNeedFullUpdate = true;

	// Folder used for cooking, the value is initialized by Output Translator
	// TemporaryCookFolder.Path = HAPI_UNREAL_DEFAULT_TEMP_COOK_FOLDER;
	
	// Folder used for baking this asset's outputs, the value is initialized by Output Translator
	// BakeFolder.Path = HAPI_UNREAL_DEFAULT_BAKE_FOLDER;

	bHasComponentTransformChanged_DEPRECATED = false;

	bFullyLoaded_DEPRECATED = false;

	bOutputless_DEPRECATED = false;
	bOutputTemplateGeos_DEPRECATED = false;
	bUseOutputNodes_DEPRECATED = true;
	PDGAssetLink_DEPRECATED = nullptr;

	bOverrideGlobalProxyStaticMeshSettings_DEPRECATED = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings)
	{
		bEnableProxyStaticMeshOverride_DEPRECATED = HoudiniRuntimeSettings->bEnableProxyStaticMesh;
		bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementByTimer;
		ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED = HoudiniRuntimeSettings->ProxyMeshAutoRefineTimeoutSeconds;
		bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreSaveWorld;
		bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreBeginPIE;
	}
	else
	{
		bEnableProxyStaticMeshOverride_DEPRECATED = false; 
		bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED = true; 
		ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED = 10.0f;
		bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED = true; 
		bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED = true;
	}
	
	bNoProxyMeshNextCookRequested_DEPRECATED = false;
	BakeAfterNextCook_DEPRECATED = EHoudiniBakeAfterNextCook::Disabled;

#if WITH_EDITORONLY_DATA
	bGenerateMenuExpanded_DEPRECATED = true;
	bBakeMenuExpanded_DEPRECATED = true;
	bAssetOptionMenuExpanded_DEPRECATED = true;
	bHelpAndDebugMenuExpanded_DEPRECATED = true;

	HoudiniEngineBakeOption_DEPRECATED = EHoudiniEngineBakeOption::ToActor;

	bRemoveOutputAfterBake_DEPRECATED = false;
	bRecenterBakedActors_DEPRECATED = false;
	bReplacePreviousBake_DEPRECATED = false;
	ActorBakeOption_DEPRECATED = EHoudiniEngineActorBakeOption::OneActorPerComponent;
	bAllowPlayInEditorRefinement_DEPRECATED = false;
	bNeedToUpdateEditorProperties_DEPRECATED = false;
	bLandscapeUseTempLayers_DEPRECATED = false;
	bEnableCurveEditing_DEPRECATED = true;
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

	LastTickTime_DEPRECATED = 0.0;
	LastLiveSyncPingTime_DEPRECATED = 0.0;

	// Initialize the default SM Build settings with the plugin's settings default values
	StaticMeshBuildSettings_DEPRECATED = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();

	//bWantsOnUpdateTransform = true;

	bIsPDGAssetLinkInitialized_DEPRECATED = false;

	bMigrateDataToCookableOnPostLoad = false;
}

UHoudiniAssetComponent::~UHoudiniAssetComponent()
{
	// Unregister ourself so our houdini node can be delete.

	// This gets called in UnRegisterHoudiniComponent, with appropriate checks. Don't call it here.
	//FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(AssetId, true);

	//FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(GetCookable());
}

void UHoudiniAssetComponent::PostInitProperties()
{
	Super::PostInitProperties();

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
	{
		// Copy default static mesh generation parameters from settings.
		StaticMeshGenerationProperties_DEPRECATED.bGeneratedDoubleSidedGeometry = HoudiniRuntimeSettings->bDoubleSidedGeometry;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedPhysMaterial = HoudiniRuntimeSettings->PhysMaterial;
		StaticMeshGenerationProperties_DEPRECATED.DefaultBodyInstance = HoudiniRuntimeSettings->DefaultBodyInstance;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedCollisionTraceFlag = HoudiniRuntimeSettings->CollisionTraceFlag;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedLightMapCoordinateIndex = HoudiniRuntimeSettings->LightMapCoordinateIndex;
		StaticMeshGenerationProperties_DEPRECATED.bGeneratedUseMaximumStreamingTexelRatio = HoudiniRuntimeSettings->bUseMaximumStreamingTexelRatio;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedStreamingDistanceMultiplier = HoudiniRuntimeSettings->StreamingDistanceMultiplier;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedWalkableSlopeOverride = HoudiniRuntimeSettings->WalkableSlopeOverride;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedFoliageDefaultSettings = HoudiniRuntimeSettings->FoliageDefaultSettings;
		StaticMeshGenerationProperties_DEPRECATED.GeneratedAssetUserData = HoudiniRuntimeSettings->AssetUserData;
	}

	// Register ourself to the HER singleton
	FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(GetCookable());
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
	if (GetCookable())
		return GetCookable()->GetHoudiniAsset();

	return HoudiniAsset_DEPRECATED;
}

FString
UHoudiniAssetComponent::GetHoudiniAssetName() const
{
	return IsValid(GetHoudiniAsset()) ? GetHoudiniAsset()->GetName() : TEXT("");
}

FString
UHoudiniAssetComponent::GetDisplayName() const
{
	return GetOwner() ? GetOwner()->GetActorNameOrLabel() : GetName();
}

TArray<TObjectPtr<UHoudiniParameter>>&
UHoudiniAssetComponent::GetParameters()
{ 
	if (GetCookable())
		return GetCookable()->GetParameters();

	return Parameters_DEPRECATED; 
}

const TArray<TObjectPtr<UHoudiniParameter>>&
UHoudiniAssetComponent::GetParameters() const
{
	if (GetCookable())
		return GetCookable()->GetParameters();

	return Parameters_DEPRECATED;
}

TArray<TObjectPtr<UHoudiniInput>>&
UHoudiniAssetComponent::GetInputs()
{ 
	if (GetCookable())
		return GetCookable()->GetInputs();

	return Inputs_DEPRECATED;
}

const TArray<TObjectPtr<UHoudiniInput>>&
UHoudiniAssetComponent::GetInputs() const
{
	if (GetCookable())
		return GetCookable()->GetInputs();

	return Inputs_DEPRECATED;
}

TArray<TObjectPtr<UHoudiniOutput>>& 
UHoudiniAssetComponent::GetOutputs()
{ 
	if (GetCookable())
		return GetCookable()->GetOutputs();

	return Outputs_DEPRECATED;
}

TArray<TObjectPtr<UHoudiniHandleComponent>>&
UHoudiniAssetComponent::GetHandleComponents()
{
	if (GetCookable())
		return GetCookable()->GetHandleComponents();

	return HandleComponents_DEPRECATED;
}


void
UHoudiniAssetComponent::GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const
{
	if (GetCookable())
		return GetCookable()->GetOutputs(OutOutputs);

	for (UHoudiniOutput* Output : Outputs_DEPRECATED)
	{
		OutOutputs.Add(Output);
	}
}

TArray<FHoudiniBakedOutput>&
UHoudiniAssetComponent::GetBakedOutputs()
{ 
	if (GetCookable())
		return GetCookable()->GetBakedOutputs();

	return BakedOutputs_DEPRECATED; 
}

const TArray<FHoudiniBakedOutput>&
UHoudiniAssetComponent::GetBakedOutputs() const 
{ 
	if (GetCookable())
		return GetCookable()->GetBakedOutputs();

	return BakedOutputs_DEPRECATED;
}

bool 
UHoudiniAssetComponent::GetSplitMeshSupport() const
{
	if (GetCookable())
		return GetCookable()->GetSplitMeshSupport();

	return bSplitMeshSupport_DEPRECATED;
}

FHoudiniStaticMeshGenerationProperties
UHoudiniAssetComponent::GetStaticMeshGenerationProperties() const
{
	if (GetCookable())
		return GetCookable()->GetStaticMeshGenerationProperties();

	return StaticMeshGenerationProperties_DEPRECATED;
}

FMeshBuildSettings
UHoudiniAssetComponent::GetStaticMeshBuildSettings() const
{
	if (GetCookable())
		return GetCookable()->GetStaticMeshBuildSettings();

	return StaticMeshBuildSettings_DEPRECATED;
}

bool
UHoudiniAssetComponent::IsOverrideGlobalProxyStaticMeshSettings() const
{
	if (GetCookable())
		return GetCookable()->IsOverrideGlobalProxyStaticMeshSettings();

	return bOverrideGlobalProxyStaticMeshSettings_DEPRECATED;
}


bool 
UHoudiniAssetComponent::IsProxyStaticMeshEnabled() const
{
	if (GetCookable())
		return GetCookable()->IsProxyStaticMeshEnabled();

	if (bOverrideGlobalProxyStaticMeshSettings_DEPRECATED)
	{
		return bEnableProxyStaticMeshOverride_DEPRECATED;
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
	if (GetCookable())
		return GetCookable()->IsProxyStaticMeshRefinementByTimerEnabled();

	if (bOverrideGlobalProxyStaticMeshSettings_DEPRECATED)
	{
		return bEnableProxyStaticMeshOverride_DEPRECATED && bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED;
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
	if (GetCookable())
		return GetCookable()->GetProxyMeshAutoRefineTimeoutSeconds();

	if (bOverrideGlobalProxyStaticMeshSettings_DEPRECATED)
	{
		return ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED;
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
	if (GetCookable())
		return GetCookable()->IsProxyStaticMeshRefinementOnPreSaveWorldEnabled();

	if (bOverrideGlobalProxyStaticMeshSettings_DEPRECATED)
	{
		return bEnableProxyStaticMeshOverride_DEPRECATED && bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED;
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
	if (GetCookable())
		return GetCookable()->IsProxyStaticMeshRefinementOnPreBeginPIEEnabled();

	if (bOverrideGlobalProxyStaticMeshSettings_DEPRECATED)
	{
		return bEnableProxyStaticMeshOverride_DEPRECATED && bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED;
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
UHoudiniAssetComponent::SetOverrideGlobalProxyStaticMeshSettings(bool InEnable)
{
	if (GetCookable())
		return GetCookable()->SetOverrideGlobalProxyStaticMeshSettings(InEnable);

	bOverrideGlobalProxyStaticMeshSettings_DEPRECATED = InEnable;
}

void
UHoudiniAssetComponent::SetEnableProxyStaticMeshOverride(bool InEnable)
{
	if (GetCookable())
		return GetCookable()->SetEnableProxyStaticMeshOverride(InEnable);

	bEnableProxyStaticMeshOverride_DEPRECATED = InEnable;
}

void
UHoudiniAssetComponent::SetEnableProxyStaticMeshRefinementByTimerOverride(bool InEnable)
{
	if (GetCookable())
		return GetCookable()->SetEnableProxyStaticMeshRefinementByTimerOverride(InEnable);

	bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED = InEnable;
}

void
UHoudiniAssetComponent::SetProxyMeshAutoRefineTimeoutSecondsOverride(float InValue)
{
	if (GetCookable())
		return GetCookable()->SetProxyMeshAutoRefineTimeoutSecondsOverride(InValue);

	ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED = InValue;
}

void
UHoudiniAssetComponent::SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bool InEnable)
{
	if (GetCookable())
		return GetCookable()->SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(InEnable);

	bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED = InEnable;
}

void
UHoudiniAssetComponent::SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bool InEnable)
{
	if (GetCookable())
		return GetCookable()->SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(InEnable);

	bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED = InEnable;
}

void
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset * InHoudiniAsset)
{
	// Check the asset validity
	if (!IsValid(InHoudiniAsset))
		return;

	if (GetCookable())
		GetCookable()->SetHoudiniAsset(InHoudiniAsset);
}


void 
UHoudiniAssetComponent::OnHoudiniAssetChanged()
{
	if (GetCookable())
		return GetCookable()->OnHoudiniAssetChanged();

	// TODO: clear input/params/outputs?
	Parameters_DEPRECATED.Empty();

	// The asset has been changed, mark us as needing to be reinstantiated
	MarkAsNeedInstantiation();

	// Force an update on the next tick
	bForceNeedUpdate_DEPRECATED = true;
}

void
UHoudiniAssetComponent::SetCookingEnabled(const bool& bInCookingEnabled)
{ 
	if (GetCookable())
		GetCookable()->SetCookingEnabled(bInCookingEnabled);

	bEnableCooking_DEPRECATED = bInCookingEnabled; 
}

void
UHoudiniAssetComponent::SetHasBeenLoaded(const bool& InLoaded)
{ 
	if (GetCookable())
		GetCookable()->SetHasBeenLoaded(InLoaded);

	bHasBeenLoaded_DEPRECATED = InLoaded; 
}

void
UHoudiniAssetComponent::SetHasBeenDuplicated(const bool& InDuplicated)
{
	if (GetCookable())
		GetCookable()->SetHasBeenDuplicated(InDuplicated);

	bHasBeenDuplicated_DEPRECATED = InDuplicated; 
}


bool
UHoudiniAssetComponent::NeedUpdateParameters() const
{
	if (GetCookable())
		return GetCookable()->NeedUpdateParameters();

	return false;
}

bool 
UHoudiniAssetComponent::NeedUpdateInputs() const
{
	if (GetCookable())
		return GetCookable()->NeedUpdateInputs();

	return false;
}

bool 
UHoudiniAssetComponent::WasLastCookSuccessful() const 
{ 
	if (GetCookable())
		return GetCookable()->WasLastCookSuccessful();

	return bLastCookSuccess_DEPRECATED; 
}

bool
UHoudiniAssetComponent::IsParameterDefinitionUpdateNeeded() const
{ 
	if (GetCookable())
		return GetCookable()->IsParameterDefinitionUpdateNeeded();

	return bParameterDefinitionUpdateNeeded_DEPRECATED; 
}

FDirectoryPath
UHoudiniAssetComponent::GetBakeFolder() const
{
	if (GetCookable())
		return GetCookable()->GetBakeFolder();

	return BakeFolder_DEPRECATED;
}

FDirectoryPath
UHoudiniAssetComponent::GetTemporaryCookFolder() const
{
	if (GetCookable())
		return GetCookable()->GetTemporaryCookFolder();

	return TemporaryCookFolder_DEPRECATED;
}

FString
UHoudiniAssetComponent::GetBakeFolderOrDefault() const
{
	if (GetCookable())
		return GetCookable()->GetBakeFolderOrDefault();

	return !BakeFolder_DEPRECATED.Path.IsEmpty() ? BakeFolder_DEPRECATED.Path : FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
}

FString
UHoudiniAssetComponent::GetTemporaryCookFolderOrDefault() const
{
	if (GetCookable())
		return GetCookable()->GetTemporaryCookFolderOrDefault();

	return !TemporaryCookFolder_DEPRECATED.Path.IsEmpty() ? TemporaryCookFolder_DEPRECATED.Path : FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
}


bool UHoudiniAssetComponent::NeedBlueprintStructureUpdate() const
{
	// TODO: Add similar flags to inputs, parameters
	return bBlueprintStructureModified;
}

bool UHoudiniAssetComponent::NeedBlueprintUpdate() const
{
	// TODO: Add similar flags to inputs, parameters
	return bBlueprintModified;
}

void
UHoudiniAssetComponent::BeginDestroy()
{
	// Unregister ourself so our houdini node can be deleted
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(GetCookable());

	Super::BeginDestroy();
}

void 
UHoudiniAssetComponent::MarkAsNeedCook()
{
	if (GetCookable())
		return GetCookable()->MarkAsNeedCook();
}

void
UHoudiniAssetComponent::MarkAsNeedRebuild()
{
	if (GetCookable())
		return GetCookable()->MarkAsNeedRebuild();

	return;
}

// Marks the asset as needing to be instantiated
void
UHoudiniAssetComponent::MarkAsNeedInstantiation()
{
	if (GetCookable())
		return GetCookable()->MarkAsNeedInstantiation();
}

void 
UHoudiniAssetComponent::MarkAsBlueprintStructureModified()
{
	bBlueprintStructureModified = true;
}

void 
UHoudiniAssetComponent::MarkAsBlueprintModified()
{
	bBlueprintModified = true;
}

void
UHoudiniAssetComponent::PostLoad()
{
	Super::PostLoad();

	if (bMigrateDataToCookableOnPostLoad)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Loading deprecated version of UHoudiniAssetComponent : V2 HAC will be converted to Cookable."));

		// V2 component - we need to move data to the cookable
		AHoudiniAssetActor* HAA = Cast<AHoudiniAssetActor>(this->GetOwner());
		UHoudiniCookable* HC = HAA ? HAA->GetHoudiniCookable() : nullptr;
		if (!HC)
		{
			HOUDINI_LOG_WARNING(TEXT("Actor has no Cookable."));
		}
		else
		{
			// Move data to the cookable
			if (!TransferDataToCookable(HC))
			{
				HOUDINI_LOG_ERROR(TEXT("Unable to convert v2 Houdini Asset Component to Cookable - will need to be recreated."));
			}
			else
			{
				// Indicate that we are the cookable's component
				HC->SetComponent(this);

				// Set the Cookable as our outer
				// TODO: UE doesn't like doing this on PostLoad (get stuck)
				//this->Rename(nullptr, HC); 
			
				// Once everything is done - set ourselves as a component of the HAA.
				// Why is this needed ?
				HAA->SetRootComponent(this);
				HAA->AddInstanceComponent(this);
			}
		}

		bMigrateDataToCookableOnPostLoad = false;
	}

	// TODO: Cookable ?? Needed??
	//if (GetCookable())
	//	GetCookable()->PostLoad();

	// We still need this PostLoad function as saved v2 component don't have
	// a cookable, dso dont call the cookable's PostLoad function

	// Mark as need instantiation
	MarkAsNeedInstantiation();

	// Component has been loaded, not duplicated
	SetHasBeenDuplicated(false);

	// We need to register ourself
	// TODO: Cookable - clean me up
	if (GetCookable())
	{
		FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(GetCookable());
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("Post Loading an HAC with no Cookable!!! Trouble ahead!!"));
	}


	// Register our PDG Asset link if we have any

	// !!! Do not update rendering while loading, do it when setting up the render state
	// UpdateRenderingInformation();

#if WITH_EDITORONLY_DATA
	auto MaxValue = StaticEnum<EHoudiniEngineBakeOption>()->GetMaxEnumValue() - 1;
	if (static_cast<int>(HoudiniEngineBakeOption_DEPRECATED) > MaxValue)
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Bake Type found, setting to To Actor. Possibly Foliage, which is deprecated, use the unreal_foliage attribute instead."));
		HoudiniEngineBakeOption_DEPRECATED = EHoudiniEngineBakeOption::ToActor;
	}
#endif
}


void
UHoudiniAssetComponent::PostEditImport()
{
	Super::PostEditImport();

	// TODO: Cookable ?? Needed??
	MarkAsNeedInstantiation();

	// Component has been duplicated, not loaded
	// We do need the loaded flag to reapply parameters, inputs
	// and properly update some of the output objects
	SetHasBeenDuplicated(true);

	SetAssetState(EHoudiniAssetState::PreInstantiation);
	SetAssetStateResult(EHoudiniAssetStateResult::None);
}


void
UHoudiniAssetComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	// Just call parent class for now.
	Super::CreateRenderState_Concurrent(Context);
}


void
UHoudiniAssetComponent::OnFullyLoaded()
{
	if (GetCookable())
		GetCookable()->bFullyLoaded = true;

	bFullyLoaded_DEPRECATED = true;
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
 	Super::OnComponentCreated();
}

void
UHoudiniAssetComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (GetCookable())
	{
		// Call the cookable's OnDestroy
		GetCookable()->OnDestroy(bDestroyingHierarchy);

		// Call our super
		return Super::OnComponentDestroyed(bDestroyingHierarchy);
	}

	HoudiniAsset_DEPRECATED = nullptr;

	// Clear Parameters
	for (TObjectPtr<UHoudiniParameter>& CurrentParm : Parameters_DEPRECATED)
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

	Parameters_DEPRECATED.Empty();

	// Clear Inputs
	for (TObjectPtr<UHoudiniInput>&  CurrentInput : Inputs_DEPRECATED)
	{
		if (!IsValid(CurrentInput))
			continue;

		if (CurrentInput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
			continue;

		// Destroy connected Houdini asset.
		CurrentInput->ConditionalBeginDestroy();
		CurrentInput = nullptr;
	}

	Inputs_DEPRECATED.Empty();

	// Clear Output
	for (TObjectPtr<UHoudiniOutput>& CurrentOutput : Outputs_DEPRECATED)
	{
		if (!IsValid(CurrentOutput))
			continue;

		if (CurrentOutput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
			continue;

		// Destroy all Houdini created socket actors.
		TArray<TObjectPtr<AActor>> & CurCreatedSocketActors = CurrentOutput->GetHoudiniCreatedSocketActors();
		for (auto & CurCreatedActor : CurCreatedSocketActors) 
		{
			if (!IsValid(CurCreatedActor))
				continue;

			CurCreatedActor->Destroy();
		}
		CurCreatedSocketActors.Empty();

		// Detach all Houdini attached socket actors
		TArray<TObjectPtr<AActor>> & CurAttachedSocketActors = CurrentOutput->GetHoudiniAttachedSocketActors();
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

	Outputs_DEPRECATED.Empty();

	// Clear the static mesh bake timer
	ClearRefineMeshesTimer();
	
	// Clear all TOP data and temporary geo/objects from the PDG asset link (if valid)
	if (IsValid(PDGAssetLink_DEPRECATED))
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
				PDGAssetLink_DEPRECATED->Modify();
				PDGAssetLink_DEPRECATED->ClearAllTOPData();
			}
		}
#endif
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void
UHoudiniAssetComponent::RegisterHoudiniComponent(UHoudiniAssetComponent* InComponent)
{
	UHoudiniCookable* MyCookable = GetCookable();
	if (!IsValid(MyCookable))
		return;

	// Registration of this component is wrapped in this virtual function to allow
	// derived classed to override this behaviour.
	FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(MyCookable);
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
UHoudiniAssetComponent::FindParameterByName(const FString& InParamName)
{
	if (GetCookable())
		return GetCookable()->FindParameterByName(InParamName);

	for (auto CurrentParam : Parameters_DEPRECATED)
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
	if (!GetUploadTransformsToHoudiniEngine())
		return;
	
	if (!GetComponentTransform().Equals(GetLastComponentTransform()))
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

	// TODO: COOKABLE - Still working?

	// Changing the Houdini Asset?
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, HoudiniAsset_DEPRECATED))
	{
		OnHoudiniAssetChanged();
	}
	else if (PropertyName == GetRelativeLocationPropertyName()
			|| PropertyName == GetRelativeRotationPropertyName()
			|| PropertyName == GetRelativeScale3DPropertyName())
	{
		SetHasComponentTransformChanged(true);
	}
	else if (PropertyName == 
		GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bOverrideGlobalProxyStaticMeshSettings_DEPRECATED))
	{
		if (GetCookable())
			GetCookable()->SetOverrideGlobalProxyStaticMeshSettings(bOverrideGlobalProxyStaticMeshSettings_DEPRECATED);

		// Reset the timer
		ClearRefineMeshesTimer();
		// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
		SetRefineMeshesTimer();
	}
	else if (PropertyName == 
		GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bEnableProxyStaticMeshOverride_DEPRECATED))
	{
		if (GetCookable())
			GetCookable()->SetEnableProxyStaticMeshOverride(bEnableProxyStaticMeshOverride_DEPRECATED);
	}
	else if (PropertyName == 
		GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED))
	{
		if (GetCookable())
			GetCookable()->SetEnableProxyStaticMeshRefinementByTimerOverride(bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED);

		// Reset the timer
		ClearRefineMeshesTimer();
		// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
		SetRefineMeshesTimer();
	}
	else if (PropertyName ==
		GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED))
	{
		if (GetCookable())
			GetCookable()->SetProxyMeshAutoRefineTimeoutSecondsOverride(ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED);

		// Reset the timer
		ClearRefineMeshesTimer();
		// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
		SetRefineMeshesTimer();
	}
	else if (PropertyName == 
		GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED))
	{
		if (GetCookable())
			GetCookable()->SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED);
	}
	else if (PropertyName == 
		GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED))
	{
		if (GetCookable())
			GetCookable()->SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniAssetComponent, Mobility))
	{
		// Changed GetAttachChildren to 'GetAllDescendants' due to HoudiniMeshSplitInstanceComponent 
		// not propagating property changes to their own child StaticMeshComponents.
		TArray<USceneComponent *> LocalAttachChildren;
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
			// TODO: COOKABLE
			// We are changing one of the mesh generation properties, we need to update all static meshes.
			// As the StaticMeshComponents map contains only top-level static mesh components only, use the StaticMeshes map instead
			for (int Idx = 0; Idx < GetNumOutputs(); Idx++)
			{
				UHoudiniOutput* CurOutput = GetOutputAt(Idx);
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

	if (!IsValid(this))
		return;

	if(!GetCookable())
	{
		HOUDINI_LOG_ERROR(TEXT("PostEditUndo called on a HAC with no cookable!!! Trouble Ahead!!"));
		return;
	}

	// Make sure we are registered with the HER singleton
	// We could be undoing a HoudiniActor delete
	if (!FHoudiniEngineRuntime::Get().IsCookableRegistered(GetCookable()))
	{
		MarkAsNeedInstantiation();

		// Component has been loaded, not duplicated
		SetHasBeenDuplicated(false);

		FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(GetCookable());
	}
}

#endif



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
	if (GetCookable())
		return GetCookable()->SetHasComponentTransformChanged(InHasChanged);

	// Only update the value if we're fully loaded
	// This avoid triggering a recook when loading a level
	if(bFullyLoaded_DEPRECATED)
	{
		bHasComponentTransformChanged_DEPRECATED = InHasChanged;
		LastComponentTransform_DEPRECATED = GetComponentTransform();
	}
}


void
UHoudiniAssetComponent::SetAssetCookCount(const int32& InCount)
{ 
	if (GetCookable())
		GetCookable()->SetCookCount(InCount);

	AssetCookCount_DEPRECATED = InCount; 
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
	LocalBounds = LocalBounds.TransformBy(LocalToWorld);

	const auto& LocalAttachedChildren = GetAttachChildren();
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

	// Return an empty Box if it is being destroyed
	// This can cause random ensure to trigger when deleting HACs
	if (IsBeingDestroyed() || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
		return BoxBounds;

	
	// Commented out: Creates incorrect focus bounds..
	// Query the bounds for all output objects
	if (GetCookable() && GetCookable()->GetOutputData())
	{
		for (auto& CurOutput : GetCookable()->GetOutputData()->Outputs)
		{
			if (!IsValid(CurOutput))
				continue;

			BoxBounds += CurOutput->GetBounds();
		}
	}

	/*
	// Query the bounds for all our inputs
	// Update! Bug: 148321. Thi casues other issues too. Just don't.
	// Bug: 134158: For some reason using inputs in this manner during cooking will crash the cooker
	// when using World Partition. So ignore inputs during cooking.
	if (!IsRunningCookCommandlet())
	{
		//TArray<TObjectPtr<UHoudiniInput>>& MyInputs = GetInputs();
		for (auto& CurInput : GetInputs())
		{
			if (!IsValid(CurInput))
				continue;

			BoxBounds += CurInput->GetBounds(this->GetHACWorld());
		}
	}	
	*/

	// Query the bounds for all input parameters
	//TArray<TObjectPtr<UHoudiniParameter>>& MyParams = GetParameters();
	for (auto& CurParam : GetParameters()) 
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
	for (auto & CurHandleComp : HandleComponents_DEPRECATED)
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

	/*
	// Commented out: This also created incorrect focus bounds..
	// If nothing was found, init with the asset's location
	if (BoxBounds.GetVolume() == 0.0f)
		BoxBounds += GetComponentLocation();
	*/
	return BoxBounds;
}

#if WITH_EDITORONLY_DATA
EHoudiniEngineBakeOption
UHoudiniAssetComponent::GetHoudiniEngineBakeOption() const
{
	if (GetCookable())
		return GetCookable()->GetHoudiniEngineBakeOption();

	return HoudiniEngineBakeOption_DEPRECATED;
}
#endif

#if WITH_EDITORONLY_DATA
void
UHoudiniAssetComponent::SetHoudiniEngineBakeOption(const EHoudiniEngineBakeOption& InBakeOption)
{
	if (GetCookable())
		return GetCookable()->SetHoudiniEngineBakeOption(InBakeOption);

	if (HoudiniEngineBakeOption_DEPRECATED == InBakeOption)
		return;

	HoudiniEngineBakeOption_DEPRECATED = InBakeOption;
}
#endif

#if WITH_EDITORONLY_DATA
bool
UHoudiniAssetComponent::GetReplacePreviousBake() const
{
	if (GetCookable())
		return GetCookable()->GetReplacePreviousBake();

	return bReplacePreviousBake_DEPRECATED;
}
#endif

#if WITH_EDITORONLY_DATA
void
UHoudiniAssetComponent::SetReplacePreviousBake(bool bInReplace)
{
	if (GetCookable())
		return GetCookable()->SetReplacePreviousBake(bInReplace);

	bReplacePreviousBake_DEPRECATED = bInReplace;
}
#endif


#if WITH_EDITORONLY_DATA
bool
UHoudiniAssetComponent::GetRemoveOutputAfterBake() const
{
	if (GetCookable())
		return GetCookable()->GetRemoveOutputAfterBake();

	return bRemoveOutputAfterBake_DEPRECATED;
}
#endif

#if WITH_EDITORONLY_DATA
void
UHoudiniAssetComponent::SetRemoveOutputAfterBake(bool bInRemove)
{
	if (GetCookable())
		return GetCookable()->SetRemoveOutputAfterBake(bInRemove);

	bRemoveOutputAfterBake_DEPRECATED = bInRemove;
}
#endif

#if WITH_EDITORONLY_DATA
bool
UHoudiniAssetComponent::GetRecenterBakedActors() const
{
	if (GetCookable())
		return GetCookable()->GetRecenterBakedActors();

	return bRecenterBakedActors_DEPRECATED;
}
#endif

#if WITH_EDITORONLY_DATA
void
UHoudiniAssetComponent::SetRecenterBakedActors(bool bInRecenter)
{
	if (GetCookable())
		return GetCookable()->SetRemoveOutputAfterBake(bInRecenter);

	bRecenterBakedActors_DEPRECATED = bInRecenter;
}
#endif

bool
UHoudiniAssetComponent::IsCookingEnabled() const
{ 
	if (GetCookable())
		return GetCookable()->IsCookingEnabled();

	return bEnableCooking_DEPRECATED; 
}

bool
UHoudiniAssetComponent::HasBeenLoaded() const
{
	if (GetCookable())
		return GetCookable()->HasBeenLoaded();

	return bHasBeenLoaded_DEPRECATED;
}

bool
UHoudiniAssetComponent::HasBeenDuplicated() const
{ 
	if (GetCookable())
		return GetCookable()->HasBeenDuplicated();

	return bHasBeenDuplicated_DEPRECATED;
}

bool
UHoudiniAssetComponent::HasRecookBeenRequested() const
{ 
	if (GetCookable())
		return GetCookable()->HasRecookBeenRequested();

	return bRecookRequested_DEPRECATED;
}

bool
UHoudiniAssetComponent::HasRebuildBeenRequested() const 
{ 
	if (GetCookable())
		return GetCookable()->HasRebuildBeenRequested();

	return bRebuildRequested_DEPRECATED; 
}

bool
UHoudiniAssetComponent::GetCookOnParameterChange() const
{
	if (GetCookable())
		return GetCookable()->GetCookOnParameterChange();

	return bCookOnParameterChange_DEPRECATED;
}

bool
UHoudiniAssetComponent::GetCookOnTransformChange() const
{
	if (GetCookable())
		return GetCookable()->GetCookOnTransformChange();

	return bCookOnTransformChange_DEPRECATED;
}

bool
UHoudiniAssetComponent::GetCookOnAssetInputCook() const
{
	if (GetCookable())
		return GetCookable()->GetCookOnCookableInputCook();

	return bCookOnAssetInputCook_DEPRECATED;
}

bool
UHoudiniAssetComponent::IsOutputless() const
{
	if (GetCookable())
		return GetCookable()->IsOutputless();

	return bOutputless_DEPRECATED;
}

bool
UHoudiniAssetComponent::GetUseOutputNodes() const
{
	if (GetCookable())
		return GetCookable()->GetUseOutputNodes();

	return bUseOutputNodes_DEPRECATED;
}

bool
UHoudiniAssetComponent::GetOutputTemplateGeos() const
{
	if (GetCookable())
		return GetCookable()->GetOutputTemplateGeos();

	return bOutputTemplateGeos_DEPRECATED;
}

bool
UHoudiniAssetComponent::GetUploadTransformsToHoudiniEngine() const
{
	if (GetCookable())
		return GetCookable()->GetUploadTransformsToHoudiniEngine();

	return bUploadTransformsToHoudiniEngine_DEPRECATED;
}

FTransform
UHoudiniAssetComponent::GetLastComponentTransform() const
{
	if (GetCookable())
		return GetCookable()->GetLastComponentTransform();

	return LastComponentTransform_DEPRECATED;
}


#if WITH_EDITORONLY_DATA
bool
UHoudiniAssetComponent::GetLandscapeUseTempLayers() const
{
	if (GetCookable())
		return GetCookable()->GetLandscapeUseTempLayers();

	return bLandscapeUseTempLayers_DEPRECATED;
}
#endif

#if WITH_EDITORONLY_DATA
bool
UHoudiniAssetComponent::GetEnableCurveEditing() const
{
	if (GetCookable())
		return GetCookable()->GetEnableCurveEditing();

	return bEnableCurveEditing_DEPRECATED;
}
#endif

void
UHoudiniAssetComponent::SetCookOnParameterChange(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetCookOnParameterChange(bEnable);

	bCookOnParameterChange_DEPRECATED = bEnable;
}

void
UHoudiniAssetComponent::SetCookOnTransformChange(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetCookOnTransformChange(bEnable);

	bCookOnTransformChange_DEPRECATED = bEnable;
}

void
UHoudiniAssetComponent::SetCookOnAssetInputCook(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetCookOnCookableInputCook(bEnable);

	bCookOnAssetInputCook_DEPRECATED = bEnable;
}

void
UHoudiniAssetComponent::SetOutputless(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetOutputless(bEnable);

	bOutputless_DEPRECATED = bEnable;
}

void
UHoudiniAssetComponent::SetUseOutputNodes(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetUseOutputNodes(bEnable);

	bUseOutputNodes_DEPRECATED = bEnable;
}

void
UHoudiniAssetComponent::SetOutputTemplateGeos(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetOutputTemplateGeos(bEnable);

	bOutputTemplateGeos_DEPRECATED = bEnable;
}

void
UHoudiniAssetComponent::SetUploadTransformsToHoudiniEngine(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetUploadTransformsToHoudiniEngine(bEnable);

	bUploadTransformsToHoudiniEngine_DEPRECATED = bEnable;
}

#if WITH_EDITORONLY_DATA
void
UHoudiniAssetComponent::SetLandscapeUseTempLayers(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetLandscapeUseTempLayers(bEnable);

	bLandscapeUseTempLayers_DEPRECATED = bEnable;
}
#endif

#if WITH_EDITORONLY_DATA
void
UHoudiniAssetComponent::SetEnableCurveEditing(bool bEnable)
{
	if (GetCookable())
		return GetCookable()->SetEnableCurveEditing(bEnable);

	bEnableCurveEditing_DEPRECATED = bEnable;
}
#endif

void
UHoudiniAssetComponent::ClearRefineMeshesTimer()
{
	if (GetCookable())
		return GetCookable()->ClearRefineMeshesTimer();

	UWorld *World = GetHACWorld();
	if (!World)
	{
		//HOUDINI_LOG_ERROR(TEXT("Cannot ClearRefineMeshesTimer, World is nullptr!"));
		return;
	}
	
	World->GetTimerManager().ClearTimer(RefineMeshesTimer_DEPRECATED);
}

void
UHoudiniAssetComponent::SetRefineMeshesTimer()
{
	if (GetCookable())
		return GetCookable()->SetRefineMeshesTimer();

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
		World->GetTimerManager().SetTimer(RefineMeshesTimer_DEPRECATED, this, &UHoudiniAssetComponent::OnRefineMeshesTimerFired, 1.0f, false, TimeSeconds);
	}
	else
	{
		World->GetTimerManager().ClearTimer(RefineMeshesTimer_DEPRECATED);
	}
}

void 
UHoudiniAssetComponent::OnRefineMeshesTimerFired()
{
	if (GetCookable())
		return GetCookable()->OnRefineMeshesTimerFired();

	HOUDINI_LOG_MESSAGE(TEXT("UHoudiniAssetComponent::OnRefineMeshesTimerFired()"));
	if (OnRefineMeshesTimerDelegate_DEPRECATED.IsBound())
	{
		OnRefineMeshesTimerDelegate_DEPRECATED.Broadcast(this);
	}
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



void
UHoudiniAssetComponent::SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InHSMGP)
{
	if (GetCookable())
		return GetCookable()->SetStaticMeshGenerationProperties(InHSMGP);

	StaticMeshGenerationProperties_DEPRECATED = InHSMGP;
};

void
UHoudiniAssetComponent::SetStaticMeshGenerationProperties(UStaticMesh* InStaticMesh)
{
#if WITH_EDITOR
	if (!InStaticMesh)
		return;

	// Make sure static mesh has a new lighting guid.
	InStaticMesh->SetLightingGuid(FGuid::NewGuid());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	InStaticMesh->SetLODGroup(NAME_None);
#else
	InStaticMesh->LODGroup = NAME_None;
#endif

	// Set resolution of lightmap.
	InStaticMesh->SetLightMapResolution(StaticMeshGenerationProperties_DEPRECATED.GeneratedLightMapResolution);

	const FStaticMeshRenderData* InRenderData = InStaticMesh->GetRenderData();
	// Set the global light map coordinate index if it looks valid
	if (InRenderData && InRenderData->LODResources.Num() > 0)
	{
		int32 NumUVs = InRenderData->LODResources[0].GetNumTexCoords();
		if (NumUVs > StaticMeshGenerationProperties_DEPRECATED.GeneratedLightMapCoordinateIndex)
		{
			InStaticMesh->SetLightMapCoordinateIndex(StaticMeshGenerationProperties_DEPRECATED.GeneratedLightMapCoordinateIndex);
		}
	}

	// TODO
	// Set method for LOD texture factor computation.
	// InStaticMesh->bUseMaximumStreamingTexelRatio = StaticMeshGenerationProperties.bGeneratedUseMaximumStreamingTexelRatio;
	// Set distance where textures using UV 0 are streamed in/out.  - GOES ON COMPONENT
	// InStaticMesh->StreamingDistanceMultiplier = StaticMeshGenerationProperties.GeneratedStreamingDistanceMultiplier;
	
	// Add user data.
	for (int32 AssetUserDataIdx = 0; AssetUserDataIdx < StaticMeshGenerationProperties_DEPRECATED.GeneratedAssetUserData.Num(); AssetUserDataIdx++)
		InStaticMesh->AddAssetUserData(StaticMeshGenerationProperties_DEPRECATED.GeneratedAssetUserData[AssetUserDataIdx]);

	// Create a body setup if needed
	if (!InStaticMesh->GetBodySetup())
		InStaticMesh->CreateBodySetup();

	UBodySetup* BodySetup = InStaticMesh->GetBodySetup();
	if (!BodySetup)
		return;

	// Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
	BodySetup->bDoubleSidedGeometry = StaticMeshGenerationProperties_DEPRECATED.bGeneratedDoubleSidedGeometry;

	// Assign physical material for simple collision.
	BodySetup->PhysMaterial = StaticMeshGenerationProperties_DEPRECATED.GeneratedPhysMaterial;

	BodySetup->DefaultInstance.CopyBodyInstancePropertiesFrom(&StaticMeshGenerationProperties_DEPRECATED.DefaultBodyInstance);

	// Assign collision trace behavior.
	BodySetup->CollisionTraceFlag = StaticMeshGenerationProperties_DEPRECATED.GeneratedCollisionTraceFlag;

	// Assign walkable slope behavior.
	BodySetup->WalkableSlopeOverride = StaticMeshGenerationProperties_DEPRECATED.GeneratedWalkableSlopeOverride;

	// We want to use all of geometry for collision detection purposes.
	BodySetup->bMeshCollideAll = true;

#endif
}


void
UHoudiniAssetComponent::UpdatePhysicsState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHoudiniAssetComponent::UpdatePhysicsState);

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
	if (GetCookable())
		return GetCookable()->SetCurrentState(InNewState);

	const EHoudiniAssetState OldState = AssetState_DEPRECATED;
	AssetState_DEPRECATED = InNewState;

#if WITH_EDITOR
	IHoudiniEditorAssetStateSubsystemInterface* const EditorSubsystem = IHoudiniEditorAssetStateSubsystemInterface::Get(); 
	if (EditorSubsystem)
		EditorSubsystem->NotifyOfHoudiniAssetStateChange(this, OldState, InNewState);
#endif
	HandleOnHoudiniAssetStateChange(this, OldState, InNewState);
}

void
UHoudiniAssetComponent::SetAssetStateResult(EHoudiniAssetStateResult InResult)
{
	if (GetCookable())
		return GetCookable()->SetCurrentStateResult(InResult);

	AssetStateResult_DEPRECATED = InResult;
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

void 
UHoudiniAssetComponent::HandleOnPreInstantiation()
{
	if (GetCookable())
		GetCookable()->HandleOnPreInstantiation();

	if (OnPreInstantiationDelegate_DEPRECATED.IsBound())
		OnPreInstantiationDelegate_DEPRECATED.Broadcast(this);
}

void
UHoudiniAssetComponent::HandleOnPreCook()
{
	if (GetCookable())
		GetCookable()->HandleOnPreCook();

	// Process the PreCookCallbacks array first
	for(auto CallbackFn : PreCookCallbacks_DEPRECATED)
	{
		CallbackFn(this);
	}
	PreCookCallbacks_DEPRECATED.Empty();
	
	if (OnPreCookDelegate_DEPRECATED.IsBound())
		OnPreCookDelegate_DEPRECATED.Broadcast(this);
}

void
UHoudiniAssetComponent::HandleOnPostCook()
{
	if (GetCookable())
		GetCookable()->HandleOnPostCook();

	if (OnPostCookDelegate_DEPRECATED.IsBound())
		OnPostCookDelegate_DEPRECATED.Broadcast(this, bLastCookSuccess_DEPRECATED);
}

void
UHoudiniAssetComponent::HandleOnPreOutputProcessing()
{
	if (GetCookable())
		GetCookable()->HandleOnPreOutputProcessing();

	if (OnPreOutputProcessingDelegate_DEPRECATED.IsBound())
	{
		OnPreOutputProcessingDelegate_DEPRECATED.Broadcast(this, true);
	}
}

void
UHoudiniAssetComponent::HandleOnPostOutputProcessing()
{
	if (GetCookable())
		GetCookable()->HandleOnPostOutputProcessing();

	if (OnPostOutputProcessingDelegate_DEPRECATED.IsBound())
	{
		OnPostOutputProcessingDelegate_DEPRECATED.Broadcast(this, true);
	}
}

void
UHoudiniAssetComponent::HandleOnPostBake(bool bInSuccess)
{
	if (GetCookable())
		GetCookable()->HandleOnPostBake(bInSuccess);

	if (OnPostBakeDelegate_DEPRECATED.IsBound())
		OnPostBakeDelegate_DEPRECATED.Broadcast(this, bInSuccess);
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

void UHoudiniAssetComponent::OnSessionConnected()
{
	if (GetCookable())
		GetCookable()->OnSessionConnected();

	for(auto& Param : Parameters_DEPRECATED)
		Param->OnSessionConnected();

	for (auto & Input : Inputs_DEPRECATED)
	{
		Input->OnSessionConnected();
	}

	AssetId_DEPRECATED = INDEX_NONE;
}

void
UHoudiniAssetComponent::ProcessBPTemplate(const bool& InIsGlobalCookingEnabled)
{
	// Handle template processing (for BP)
	if (GetAssetState() != EHoudiniAssetState::ProcessTemplate)
		return;

	if (IsTemplate() && !HasOpenEditor())
	{
		// This component template no longer has an open editor and can be deregistered.
		// TODO: Replace this polling mechanism with an "On Asset Closed" event if we
		// can find one that actually works.
		FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(GetCookable());
		return;
	}

	if (NeedBlueprintStructureUpdate())
	{
		OnBlueprintStructureModified();
	}

	if (NeedBlueprintUpdate())
	{
		OnBlueprintModified();
	}

	if (InIsGlobalCookingEnabled)
	{
		// Only process component template parameter updates when cooking is enabled.
		if (NeedUpdateParameters() || NeedUpdateInputs())
		{
			OnTemplateParametersChanged();
		}
	}
}

int32
UHoudiniAssetComponent::GetAssetId() const 
{ 
	if (GetCookable())
		return GetCookable()->GetNodeId();

	return AssetId_DEPRECATED; 
}

EHoudiniAssetState
UHoudiniAssetComponent::GetAssetState() const
{
	if (GetCookable())
		return GetCookable()->GetCurrentState();

	return AssetState_DEPRECATED;
}

EHoudiniAssetStateResult
UHoudiniAssetComponent::GetAssetStateResult() const
{
	if (GetCookable())
		return GetCookable()->GetCurrentStateResult();

	return AssetStateResult_DEPRECATED;

}

FGuid& 
UHoudiniAssetComponent::GetHapiGUID()
{
	if (GetCookable())
		return GetCookable()->GetHapiGUID();

	return HapiGUID_DEPRECATED; 
}

FString
UHoudiniAssetComponent::GetHapiAssetName() const
{
	if (GetCookable())
		return GetCookable()->GetHapiAssetName();

	return HapiAssetName_DEPRECATED; 
}

FGuid
UHoudiniAssetComponent::GetComponentGUID() const
{
	if (GetCookable())
		return GetCookable()->GetCookableGUID();

	return ComponentGUID_DEPRECATED; 
}

UHoudiniCookable*
UHoudiniAssetComponent::GetCookable() const
{
	UHoudiniCookable* HC = Cast<UHoudiniCookable>(GetOuter());
	if (HC)
		return HC;

	// Try to get the Cookable via our Actor - we might be a loaded v2 HAC
	// This is required for loaded v2 HAC - as the Cookable is not the HAC's outer
	AHoudiniAssetActor* HAA = Cast<AHoudiniAssetActor>(GetOwner());
	if (HAA)
		return HAA->GetHoudiniCookable();

	return nullptr;
}

int32
UHoudiniAssetComponent::GetNumInputs() const
{
	if (GetCookable())
		return GetCookable()->GetNumInputs();

	return Inputs_DEPRECATED.Num();
}

int32
UHoudiniAssetComponent::GetNumOutputs() const
{
	if (GetCookable())
		return GetCookable()->GetNumOutputs();

	return Outputs_DEPRECATED.Num();
}

int32
UHoudiniAssetComponent::GetNumParameters() const
{
	if (GetCookable())
		return GetCookable()->GetNumParameters();

	return Parameters_DEPRECATED.Num();
}

int32
UHoudiniAssetComponent::GetNumHandles() const 
{ 
	if (GetCookable())
		return GetCookable()->GetNumHandles();

	return HandleComponents_DEPRECATED.Num(); 
}

UHoudiniInput*
UHoudiniAssetComponent::GetInputAt(const int32& Idx)
{ 
	if (GetCookable())
		return GetCookable()->GetInputAt(Idx);

	return Inputs_DEPRECATED.IsValidIndex(Idx) ? Inputs_DEPRECATED[Idx] : nullptr;
}

UHoudiniOutput*
UHoudiniAssetComponent::GetOutputAt(const int32& Idx)
{
	if (GetCookable())
		return GetCookable()->GetOutputAt(Idx);

	return Outputs_DEPRECATED.IsValidIndex(Idx) ? Outputs_DEPRECATED[Idx] : nullptr;
}

UHoudiniParameter*
UHoudiniAssetComponent::GetParameterAt(const int32& Idx)
{
	if (GetCookable())
		return GetCookable()->GetParameterAt(Idx);

	return Parameters_DEPRECATED.IsValidIndex(Idx) ? Parameters_DEPRECATED[Idx] : nullptr;
}

UHoudiniHandleComponent*
UHoudiniAssetComponent::GetHandleComponentAt(const int32& Idx)
{
	if (GetCookable())
		return GetCookable()->GetHandleComponentAt(Idx);

	return HandleComponents_DEPRECATED.IsValidIndex(Idx) ? HandleComponents_DEPRECATED[Idx] : nullptr;
}

UHoudiniPDGAssetLink* 
UHoudiniAssetComponent::GetPDGAssetLink()
{
	if (GetCookable())
		return GetCookable()->GetPDGAssetLink();

	return PDGAssetLink_DEPRECATED;
};


bool
UHoudiniAssetComponent::IsFullyLoaded() const
{ 
	if (GetCookable())
		return GetCookable()->IsFullyLoaded();

	return bFullyLoaded_DEPRECATED; 
}



bool
UHoudiniAssetComponent::SetTemporaryCookFolderPath(const FString& NewPath)
{
	if (GetCookable())
		return GetCookable()->SetTemporaryCookFolderPath(NewPath);

	if (TemporaryCookFolder_DEPRECATED.Path.Equals(NewPath))
		return false;

	if (TemporaryCookFolder_DEPRECATED.Path == NewPath)
		return false;

	TemporaryCookFolder_DEPRECATED.Path = NewPath;

	return true;
}

bool
UHoudiniAssetComponent::SetBakeFolderPath(const FString& NewPath)
{
	if (GetCookable())
		return GetCookable()->SetBakeFolderPath(NewPath);

	if (BakeFolder_DEPRECATED.Path.Equals(NewPath))
		return false;

	if (BakeFolder_DEPRECATED.Path == NewPath)
		return false;

	BakeFolder_DEPRECATED.Path = NewPath;

	return true;
}

bool
UHoudiniAssetComponent::SetTemporaryCookFolder(const FDirectoryPath& InPath)
{
	if (GetCookable())
		return GetCookable()->SetTemporaryCookFolder(InPath);

	if (TemporaryCookFolder_DEPRECATED.Path.Equals(InPath.Path))
		return false;

	TemporaryCookFolder_DEPRECATED = InPath;

	return true;
}

bool
UHoudiniAssetComponent::SetBakeFolder(const FDirectoryPath& InPath)
{
	if (GetCookable())
		return GetCookable()->SetBakeFolder(InPath);

	if (BakeFolder_DEPRECATED.Path.Equals(InPath.Path))
		return false;

	BakeFolder_DEPRECATED = InPath;

	return true;
}

bool
UHoudiniAssetComponent::TransferDataToCookable(UHoudiniCookable* HC)
{
	if (!HC)
		return false;

	HC->SetHoudiniAsset(HoudiniAsset_DEPRECATED);

	HC->SetCookOnParameterChange(bCookOnParameterChange_DEPRECATED);
	HC->SetUploadTransformsToHoudiniEngine(bUploadTransformsToHoudiniEngine_DEPRECATED);
	HC->SetCookOnTransformChange(bCookOnTransformChange_DEPRECATED);
	HC->SetCookOnCookableInputCook(bCookOnAssetInputCook_DEPRECATED);
	HC->SetOutputless(bOutputless_DEPRECATED);
	HC->SetOutputTemplateGeos(bOutputTemplateGeos_DEPRECATED);
	HC->SetUseOutputNodes(bUseOutputNodes_DEPRECATED);

	HC->SetTemporaryCookFolder(TemporaryCookFolder_DEPRECATED);
	HC->SetBakeFolder(BakeFolder_DEPRECATED);
	HC->OutputData->bSplitMeshSupport = bSplitMeshSupport_DEPRECATED;
	HC->SetStaticMeshGenerationProperties(StaticMeshGenerationProperties_DEPRECATED);
	HC->SetStaticMeshBuildSettings(StaticMeshBuildSettings_DEPRECATED);

	HC->SetOverrideGlobalProxyStaticMeshSettings(bOverrideGlobalProxyStaticMeshSettings_DEPRECATED);
	HC->SetEnableProxyStaticMeshOverride(bEnableProxyStaticMeshOverride_DEPRECATED);
	HC->SetEnableProxyStaticMeshRefinementByTimerOverride(bEnableProxyStaticMeshRefinementByTimerOverride_DEPRECATED);
	HC->SetProxyMeshAutoRefineTimeoutSecondsOverride(ProxyMeshAutoRefineTimeoutSecondsOverride_DEPRECATED);
	HC->SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride_DEPRECATED);
	HC->SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride_DEPRECATED);

#if WITH_EDITORONLY_DATA
	// bool bGenerateMenuExpanded; // COOKABLE
	// bool bBakeMenuExpanded; // COOKABLE
	// bool bAssetOptionMenuExpanded; // COOKABLE
	// bool bHelpAndDebugMenuExpanded; // COOKABLE

	HC->SetHoudiniEngineBakeOption(HoudiniEngineBakeOption_DEPRECATED);
	HC->SetRemoveOutputAfterBake(bRemoveOutputAfterBake_DEPRECATED);
	HC->SetRecenterBakedActors(bRecenterBakedActors_DEPRECATED);
	HC->SetReplacePreviousBake(bReplacePreviousBake_DEPRECATED);
	HC->SetActorBakeOption(ActorBakeOption_DEPRECATED);
	HC->SetLandscapeUseTempLayers(bLandscapeUseTempLayers_DEPRECATED);
	HC->SetEnableCurveEditing(bEnableCurveEditing_DEPRECATED);
	
	// bool bNeedToUpdateEditorProperties; // COOKABLE
#endif

	// HC->NodeId = AssetId; // COOKABLE - NodeId
	// HC->SetNodeIdsToCook(NodeIdsToCook);
	// HC->NodesToCookCookCounts(OutputNodeCookCounts);
	
	// 
	for (auto& CurHAC : DownstreamHoudiniAssets)
	{
		UHoudiniCookable* CurHC = CurHAC->GetCookable();
		if (!IsValid(CurHC))
			continue;

		HC->InputData->DownstreamCookables.Add(CurHC);
	}
	//DownstreamHoudiniAssets_DEPRECATED.Empty();
	
	// HC->CookableGUID = ComponentGUID;
	// HC->HapiGUID = HapiGUID;
	HC->HoudiniAssetData->HapiAssetName = HapiAssetName_DEPRECATED; // COOKABLE - Name
	// HC->SetCurrentState(AssetState); // COOKABLE
	// EHoudiniAssetState DebugLastAssetState; // NOT COOKABLE
	// HC->SetCurrentStateResult(AssetStateResult); // COOKABLE
	// LastComponentTransform; // COOKABLE - COMPONENT

	HC->HoudiniAssetData->SubAssetIndex = SubAssetIndex_DEPRECATED;
	// HC->SetCookCount(AssetCookCount); // COOKABLE - CookCount
	// HC->SetHasBeenLoaded(bHasBeenLoaded); // COOKABLE
	HC->SetHasBeenDuplicated(bHasBeenDuplicated_DEPRECATED); // COOKABLE
	// HC->bPendingDelete = bPendingDelete;
	// HC->SetRecookRequested(bRecookRequested);
	// HC->SetRebuildRequested(bRebuildRequested);
	// HC->SetCookingEnabled(bEnableCooking);
	// HC->bForceNeedUpdate = bForceNeedUpdate;
	// HC->bLastCookSuccess = bLastCookSuccess;
	
	// HC->ParameterData->bParameterDefinitionUpdateNeeded = bParameterDefinitionUpdateNeeded;
	
	// bBlueprintStructureModified; // NOT COOKABLE
	// bBlueprintModified; // NOT COOKABLE
	
	HC->ParameterData->Parameters = Parameters_DEPRECATED; // COOKABLE - PARAMETERS
	Parameters_DEPRECATED.Empty();

	HC->InputData->Inputs = Inputs_DEPRECATED; // COOKABLE - INPUTS
	Inputs_DEPRECATED.Empty();

	HC->OutputData->Outputs = Outputs_DEPRECATED; // COOKABLE - OUTPUTS
	Outputs_DEPRECATED.Empty();

	HC->BakingData->BakedOutputs = BakedOutputs_DEPRECATED; // COOKABLE - OUTPUTS
	BakedOutputs_DEPRECATED.Empty();

	HC->OutputData->UntrackedOutputs = UntrackedOutputs_DEPRECATED; // COOKABLE - OUTPUTS
	UntrackedOutputs_DEPRECATED.Empty();

	HC->ComponentData->HandleComponents = HandleComponents_DEPRECATED; // COOKABLE - COMPONENT
	HandleComponents_DEPRECATED.Empty();

	// HC->SetHasComponentTransformChanged(bHasComponentTransformChanged); // COOKABLE - COMPONENT
	// HC->bFullyLoaded = bFullyLoaded; // COOKABLE

	HC->PDGData->PDGAssetLink = PDGAssetLink_DEPRECATED;
	PDGAssetLink_DEPRECATED = nullptr;

	// HC->PDGData->bIsPDGAssetLinkInitialized = bIsPDGAssetLinkInitialized; // COOKABLE - PDG
	// HC->OutputData->RefineMeshesTimer = RefineMeshesTimer;  // COOKABLE - OUTPUTS
	// HC->OutputData->OnRefineMeshesTimerDelegate = OnRefineMeshesTimerDelegate; // COOKABLE - OUTPUTS
	// HC->SetNoProxyMeshNextCookRequested(bNoProxyMeshNextCookRequested);
	// HC->SetBakeAfterNextCook(BakeAfterNextCook); // COOKABLE - OUTPUTS

	HC->ParameterData->ParameterPresetBuffer = ParameterPresetBuffer_DEPRECATED;
	ParameterPresetBuffer_DEPRECATED.Empty();

	return true;
}