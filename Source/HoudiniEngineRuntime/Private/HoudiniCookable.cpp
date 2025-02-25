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

#include "HoudiniCookable.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniStaticMeshComponent.h"
#if WITH_EDITOR
	#include "HoudiniEditorAssetStateSubsystemInterface.h"
#endif

#include "Components/SplineComponent.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "TimerManager.h"

UHoudiniParameter*
UCookableParameterData::FindMatchingParameter(UHoudiniParameter* InOtherParam)
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



//
// HOUDINI ASSET DATA
//
UCookableHoudiniAssetData::UCookableHoudiniAssetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, HoudiniAsset(nullptr)
	, SubAssetIndex(-1)
	, HapiAssetName(TEXT(""))
{

}



//
// PARAMETER DATA
//
UCookableParameterData::UCookableParameterData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCookOnParameterChange(true)
	, bParameterDefinitionUpdateNeeded(false)
{

}



//
// INPUT DATA
//
UCookableInputData::UCookableInputData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCookOnInputChange(true)
{

}


bool
UCookableInputData::NeedsToWaitForInputHoudiniAssets()
{
	for (auto& CurrentInput : Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		EHoudiniInputType CurrentInputType = CurrentInput->GetInputType();
		if (!CurrentInput->IsAssetInput())
			continue;

		TArray<TObjectPtr<UHoudiniInputObject>>* ObjectArray = CurrentInput->GetHoudiniInputObjectArray(CurrentInputType);
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



//
// OUTPUT DATA
//
UCookableOutputData::UCookableOutputData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TemporaryCookFolder()
	, bHasWorldOutputs()
	, bOutputless(false)
	, bOutputTemplateGeos(false)
	, bUseOutputNodes(true)
	, bSplitMeshSupport(false)
	, bEnableCurveEditing(false)
	, HoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToActor)
	, BakeFolder()
	, BakeAfterNextCook(EHoudiniBakeAfterNextCook::Disabled)
	, bRemoveOutputAfterBake(false)
	, bRecenterBakedActors(false)
	, bReplacePreviousBake(false)
	, ActorBakeOption(EHoudiniEngineActorBakeOption::OneActorPerComponent)
	, bLandscapeUseTempLayers(false)
	, bHasProxyMeshSupport(true)
	, bNoProxyMeshNextCookRequested(false)
	, bOverrideGlobalProxyStaticMeshSettings(false)
	, bEnableProxyStaticMeshOverride(false)
	, bEnableProxyStaticMeshRefinementByTimerOverride(true)
	, ProxyMeshAutoRefineTimeoutSecondsOverride(10.0f)
	, bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(true)
	, bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(true)
	, bAllowPlayInEditorRefinement(false)
{
	StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	StaticMeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();

	// Initialize default proxy settings
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings)
	{
		bEnableProxyStaticMeshOverride = HoudiniRuntimeSettings->bEnableProxyStaticMesh;
		bEnableProxyStaticMeshRefinementByTimerOverride = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementByTimer;
		ProxyMeshAutoRefineTimeoutSecondsOverride = HoudiniRuntimeSettings->ProxyMeshAutoRefineTimeoutSeconds;
		bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreSaveWorld;
		bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementOnPreBeginPIE;
	}
}

bool
UCookableOutputData::IsProxyStaticMeshEnabled() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
	{
		return bEnableProxyStaticMeshOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
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
UCookableOutputData::IsBakeAfterNextCookEnabled() const 
{ 
	// Returns true if the asset should be bake after the next cook
	return BakeAfterNextCook != EHoudiniBakeAfterNextCook::Disabled; 
}

bool
UCookableOutputData::IsProxyStaticMeshRefinementByTimerEnabled() const
{
	if (bOverrideGlobalProxyStaticMeshSettings)
		return bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementByTimerOverride;

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings)
		return HoudiniRuntimeSettings->bEnableProxyStaticMesh && HoudiniRuntimeSettings->bEnableProxyStaticMeshRefinementByTimer;

	return false;
}

FString
UCookableOutputData::GetBakeFolderOrDefault() const
{
	return !BakeFolder.Path.IsEmpty() ? BakeFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
}

FString
UCookableOutputData::GetTemporaryCookFolderOrDefault() const
{
	return !TemporaryCookFolder.Path.IsEmpty() ? TemporaryCookFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
}



//
// COMPONENT DATA
//
UCookableComponentData::UCookableComponentData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastComponentTransform(FTransform())
	, bHasComponentTransformChanged(false)
	, bUploadTransformsToHoudiniEngine(true)
	, bCookOnTransformChange(false)
	, LastLiveSyncPingTime(0.0)
{

}



//
// PDG DATA
//
UCookablePDGData::UCookablePDGData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsPDGAssetLinkInitialized(false)
{
	PDGAssetLink = nullptr;
}

void
UCookablePDGData::SetPDGAssetLink(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	// Check the object validity
	if (!IsValid(InPDGAssetLink))
		return;

	// If it is the same object, do nothing.
	if (InPDGAssetLink == PDGAssetLink)
		return;

	PDGAssetLink = InPDGAssetLink;
}



//
// HOUDINI COOKABLE
//
UHoudiniCookable::UHoudiniCookable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// TODO COOKABLE
	NodeId = -1;
	CurrentState = EHoudiniAssetState::NewHDA;
	CurrentStateResult = EHoudiniAssetStateResult::None;
	CookCount = 0;
	Name = FString();

	// Create unique cookable GUID.
	CookableGUID = FGuid::NewGuid();
	
	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID.Invalidate();

	bHasBeenLoaded = false;
	bHasBeenDuplicated = false;
	bPendingDelete = false;
	bRecookRequested = false;
	bRebuildRequested = false;
	bEnableCooking = true;
	bForceNeedUpdate = false;
	bLastCookSuccess = false;
	//bBlueprintStructureModified = false;
	//bBlueprintModified = false;
	bFullyLoaded = false;
	LastTickTime = 0.0;
	//LastLiveSyncPingTime = 0.0;

	bHasHoudiniAsset = false;
/*	HoudiniAssetData = NewObject<UCookableHoudiniAssetData>(
		this, UCookableHoudiniAssetData::StaticClass(), NAME_None, RF_NoFlags);*/
	HoudiniAssetData = CreateDefaultSubobject<UCookableHoudiniAssetData>(TEXT("HoudiniAssetData"));

	bHasInputs = false;
/*	InputData = NewObject<UCookableInputData>(
		this, UCookableInputData::StaticClass(), NAME_None, RF_NoFlags);*/
	InputData = CreateDefaultSubobject<UCookableInputData>(TEXT("InputData"));

	bHasParameters = false;
/*	ParameterData = NewObject<UCookableParameterData>(
		this, UCookableParameterData::StaticClass(), NAME_None, RF_NoFlags);*/
	ParameterData = CreateDefaultSubobject<UCookableParameterData>(TEXT("ParameterData"));
	
	bHasComponent = false;
/*	ComponentData = NewObject<UCookableComponentData>(
		this, UCookableComponentData::StaticClass(), NAME_None, RF_NoFlags);*/
	ComponentData = CreateDefaultSubobject<UCookableComponentData>(TEXT("ComponentData"));

	bHasOutputs = false;
/*	OutputData = NewObject<UCookableOutputData>(
		this, UCookableOutputData::StaticClass(), NAME_None, RF_NoFlags);*/
	OutputData = CreateDefaultSubobject<UCookableOutputData>(TEXT("OutputData"));

	bHasPDG = false;
/*	PDGData = NewObject<UCookablePDGData>(
		this, UCookablePDGData::StaticClass(), NAME_None, RF_NoFlags);*/
	PDGData = CreateDefaultSubobject<UCookablePDGData>(TEXT("PDGData"));

	bNeedToUpdateEditorProperties = false;

	/*
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
	*/
}

UHoudiniCookable::~UHoudiniCookable()
{
	// TODO COOKABLE
	// Unregister ourself so our houdini nodes can be deleted.
	//FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(this);
}

UHoudiniAsset*
UHoudiniCookable::GetHoudiniAsset()
{
	return IsHoudiniAssetSupported() ? HoudiniAssetData->HoudiniAsset : nullptr;
}

UHoudiniPDGAssetLink*
UHoudiniCookable::GetPDGAssetLink()
{ 
	return IsPDGSupported() ? PDGData->PDGAssetLink : nullptr; 
}

FString
UHoudiniCookable::GetHoudiniAssetName() const
{
	if (!IsHoudiniAssetSupported())
		return FString();

	return IsValid(HoudiniAssetData->HoudiniAsset) ? HoudiniAssetData->HoudiniAsset->GetName() : TEXT("");
}

USceneComponent*
UHoudiniCookable::GetComponent() const
{
	if (!IsComponentSupported())
		return nullptr;

	return ComponentData->Component.Get();
}

AActor*
UHoudiniCookable::GetOwner() const
{
	USceneComponent* Comp = GetComponent();
	if (!Comp)
		return nullptr;

	return Comp->GetOwner();
}


UWorld*
UHoudiniCookable::GetWorld() const
{
	// TODO COOKABLE:
	// ?? return GetComponent()->GetWold() first? though it should be same...
	return GetOwner() ? GetOwner()->GetWorld() : nullptr;
}

FDirectoryPath
UHoudiniCookable::GetBakeFolder() const
{
	if (!IsOutputSupported())
		return FDirectoryPath();

	return OutputData->BakeFolder;
}

FDirectoryPath
UHoudiniCookable::GetTemporaryCookFolder() const
{
	if (!IsOutputSupported())
		return FDirectoryPath();

	return OutputData->TemporaryCookFolder;
}

FString
UHoudiniCookable::GetTemporaryCookFolderOrDefault()
{
	if (!IsOutputSupported())
		return FString();

	return !OutputData->TemporaryCookFolder.Path.IsEmpty() ? OutputData->TemporaryCookFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
}

FString
UHoudiniCookable::GetBakeFolderOrDefault()
{
	if (!IsOutputSupported())
		return FString();

	return !OutputData->BakeFolder.Path.IsEmpty() ? OutputData->BakeFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
}

bool
UHoudiniCookable::SetTemporaryCookFolderPath(const FString& NewPath)
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->TemporaryCookFolder.Path.Equals(NewPath))
		return false;

	if (OutputData->TemporaryCookFolder.Path == NewPath)
		return false;

	OutputData->TemporaryCookFolder.Path = NewPath;

	return true;
}

bool
UHoudiniCookable::SetBakeFolderPath(const FString& NewPath)
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->BakeFolder.Path.Equals(NewPath))
		return false;

	if (OutputData->BakeFolder.Path == NewPath)
		return false;

	OutputData->BakeFolder.Path = NewPath;

	return true;
}

bool
UHoudiniCookable::SetTemporaryCookFolder(const FDirectoryPath& InPath)
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->TemporaryCookFolder.Path.Equals(InPath.Path))
		return false;

	OutputData->TemporaryCookFolder = InPath;

	return true;
}

bool
UHoudiniCookable::SetBakeFolder(const FDirectoryPath& InPath)
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->BakeFolder.Path.Equals(InPath.Path))
		return false;

	OutputData->BakeFolder = InPath;

	return true;
}

bool
UHoudiniCookable::IsOwnerSelected() const
{
	return GetOwner() ? GetOwner()->IsSelected() : false;
}

bool
UHoudiniCookable::ShouldTryToStartFirstSession() const
{
	if(IsHoudiniAssetSupported() && !HoudiniAssetData->HoudiniAsset)
		return false;

	// Only try to start the default session if we have an "active" HAC
	switch (CurrentState)
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


#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
ILevelInstanceInterface*
UHoudiniCookable::GetLevelInstance() const
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
UHoudiniCookable::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	// Check the asset validity
	if (!IsValid(InHoudiniAsset))
		return;

	if (!IsHoudiniAssetSupported())
		return;

	// If it is the same asset, do nothing.
	if (InHoudiniAsset == HoudiniAssetData->HoudiniAsset)
		return;

	HoudiniAssetData->HoudiniAsset = InHoudiniAsset;
}

void UHoudiniCookable::SetComponent(USceneComponent* InComp)
{
	if (!IsComponentSupported())
		return;
	
	ComponentData->Component = InComp;
}

void UHoudiniCookable::SetHoudiniAssetComponent(UHoudiniAssetComponent* InComp)
{ 
	if (!IsComponentSupported())
		return;

	ComponentData->Component = InComp;
}

void
UHoudiniCookable::SetCurrentState(EHoudiniAssetState InNewState)
{
	const EHoudiniAssetState OldState = CurrentState;
	CurrentState = InNewState;

#if WITH_EDITOR
	IHoudiniEditorAssetStateSubsystemInterface* const EditorSubsystem = IHoudiniEditorAssetStateSubsystemInterface::Get();
	if (EditorSubsystem)
		EditorSubsystem->NotifyOfHoudiniAssetStateChange(this, OldState, InNewState);
#endif
	HandleOnHoudiniAssetStateChange(this, OldState, InNewState);
}

void
UHoudiniCookable::HandleOnHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState)
{
	IHoudiniAssetStateEvents::HandleOnHoudiniAssetStateChange(InHoudiniAssetContext, InFromState, InToState);

	if (InFromState == InToState)
		return;

	if (this != InHoudiniAssetContext)
		return;

	FOnCookableStateChangeDelegate& StateChangeDelegate = GetOnCookableStateChangeDelegate();
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
UHoudiniCookable::HandleOnPreInstantiation()
{
	if (OnPreInstantiationDelegate.IsBound())
		OnPreInstantiationDelegate.Broadcast(this);
}

void
UHoudiniCookable::HandleOnPreCook()
{
	// Process the PreCookCallbacks array first
	for (auto CallbackFn : PreCookCallbacks)
	{
		CallbackFn(this);
	}
	PreCookCallbacks.Empty();

	if (OnPreCookDelegate.IsBound())
		OnPreCookDelegate.Broadcast(this);
}

void
UHoudiniCookable::HandleOnPostCook()
{
	if (OnPostCookDelegate.IsBound())
		OnPostCookDelegate.Broadcast(this, bLastCookSuccess);
}

void
UHoudiniCookable::HandleOnPreOutputProcessing()
{
	if (OnPreOutputProcessingDelegate.IsBound())
	{
		OnPreOutputProcessingDelegate.Broadcast(this, true);
	}
}

void
UHoudiniCookable::HandleOnPostOutputProcessing()
{
	if (OnPostOutputProcessingDelegate.IsBound())
	{
		OnPostOutputProcessingDelegate.Broadcast(this, true);
	}
}

void
UHoudiniCookable::UpdateDormantStatus()
{
#if WITH_EDITOR
	// This function checks if we should go into or out of doermant status.
#if (ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION < 1)
	return;
#else
	ILevelInstanceInterface* LevelInstance = GetLevelInstance();
	if (!LevelInstance)
		return;

	if (GetCurrentState() == EHoudiniAssetState::Dormant)
	{
		// If this HDA was previously dormant, and the level instance is editable, it means
		// the level instance has just been made editable. So reset to a state where the HDA
		// can be used.
		if (LevelInstance->IsEditing())
			SetCurrentState(EHoudiniAssetState::None);
	}
	else if (GetCurrentState() == EHoudiniAssetState::None)
	{
		// If we're not doing anything, and the level instance not editable, flip the state
		// back to dormant. This highlights a potential problem that the user could  commit
		// a level instance before its finished cooking, but I'm not sure we can prevent that.
		if (!LevelInstance->IsEditing())
			SetCurrentState(EHoudiniAssetState::Dormant);
	}
#endif
#endif
}



// Indicates if any of the cookable's outputs needs to be updated (no recook needed)
bool
UHoudiniCookable::NeedUpdateInstancedOutputs() const
{
	if (!IsOutputSupported())
		return false;

	// Go through all outputs
	for (auto CurrentOutput : OutputData->Outputs)
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

bool
UHoudiniCookable::NeedUpdateParameters() const
{
	if (!IsParameterSupported())
		return false;

	// No need to cook on param change
	if(!ParameterData->bCookOnParameterChange)
		return false;

	// Go through all our parameters, return true if they have been updated
	for (auto CurrentParm : ParameterData->Parameters)
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
UHoudiniCookable::NeedUpdateInputs() const
{
	if (!IsInputSupported())
		return false;

	// No need to cook on input change
	if (!InputData->bCookOnInputChange)
		return false;

	// Go through all our inputs, return true if they have been updated
	for (auto CurrentInput : InputData->Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		if (!CurrentInput->HasChanged())
			continue;

		// See if the input doesn't require an update 
		// (because it has failed to upload previously or has been loaded)
		if (!CurrentInput->NeedsToTriggerUpdate())
			continue;

		return true;
	}

	return false;
}

bool
UHoudiniCookable::NeedUpdateOutputs() const
{
	if (!IsOutputSupported())
		return false;

	// Go through all outputs, filter the editable nodes. Return true if they have been updated.
	for (auto CurrentOutput : OutputData->Outputs)
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
			for (auto Component : NextPair.Value.OutputComponents)
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

bool
UHoudiniCookable::NeedUpdate() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHoudiniCookable::NeedUpdate);

	// It is important to check this when dealing with Blueprints since the
	// preview components start receiving events from the template component
	// before the preview component have finished initialization.
	if (!IsFullyLoaded())
		return false;

	/*
	// We must have a valid asset, unless we're a NodeSync component
	if (!IsValid(HoudiniAsset) && !IsA<UHoudiniNodeSyncComponent>())
		return false;
	*/
	// If we support HDAs - we should have one assigned.
	if (IsHoudiniAssetSupported() && !HoudiniAssetData->HoudiniAsset)
		return false;

	if (bForceNeedUpdate || bRecookRequested) // || bRebuildRequested ??
		return true;

	// Check if the HAC's transform has changed and we need to cook because of it
	if (IsComponentSupported() && ComponentData->bHasComponentTransformChanged && ComponentData->bCookOnTransformChange)
		return true;

	// If we don't want to cook on parameter/input change dont bother looking for updates
	//if (!bCookOnParameterChange && !bRecookRequested && !bRebuildRequested)
	//	return false;

	// If we support parameters - see if one has changed
	if(IsParameterSupported() && NeedUpdateParameters())
		return true;

	// If we support inputs - see if one has changed
	if ( IsInputSupported() && NeedUpdateInputs())
		return true;

	// See if our (editable) outputs needs an update
	if ( IsOutputSupported() && NeedUpdateOutputs())
		return true;

	return false;
}


FString
UHoudiniCookable::GetDisplayName() const
{
	if (GetOwner())
		return  GetOwner()->GetActorNameOrLabel();
	
	return GetName();
}

void
UHoudiniCookable::ClearNodesToCook()
{
	NodeIdsToCook.Empty();
	NodesToCookCookCounts.Empty();
}


void
UHoudiniCookable::UpdatePostDuplicate()
{
	if (IsComponentSupported() && IsValid(ComponentData->Component))
	{
		// TODO COOKABLE:
		// - Keep the output objects/components (remove duplicatetransient on the output object uproperties)
		// - Duplicate created objects (ie SM) and materials
		// - Update the output components to use these instead
		// This should remove the need for a cook on duplicate
		
		// For now, we simply clean some of our component's children component manually
		const TArray<USceneComponent*> Children = ComponentData->Component->GetAttachChildren();

		for (auto& NextChild : Children)
		{
			if (!IsValid(NextChild))
				continue;

			// We don't want to remove components that were added in a Blueprint Template
			if (NextChild->IsCreatedByConstructionScript())
				continue;

			USceneComponent* ComponentToRemove = nullptr;
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
			else if (NextChild->IsA<UHoudiniInstancedActorComponent>())
			{
				// The actors attached to the HoudiniAssetActor are not duplicated, so we only 
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
	}
	
	if(IsPDGSupported() && IsValid(PDGData->PDGAssetLink))
	{
		// if there is an associated PDG asset link, call its UpdatePostDuplicate to cleanup references to
		// to the original instance's PDG output actors
		PDGData->PDGAssetLink->UpdatePostDuplicate();
	}

	bHasBeenDuplicated = false;
}

void
UHoudiniCookable::SetHasComponentTransformChanged(bool InHasChanged)
{
	if (!IsComponentSupported())
		return;

	// Only update the value if we're fully loaded
	// This avoid triggering a recook when loading a level
	if (!bFullyLoaded)
		return;

	ComponentData->bHasComponentTransformChanged = InHasChanged;
	ComponentData->LastComponentTransform = ComponentData->Component->GetComponentTransform();
}


void
UHoudiniCookable::ClearRefineMeshesTimer()
{
	if (!IsOutputSupported())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	World->GetTimerManager().ClearTimer(OutputData->RefineMeshesTimer);
}


void
UHoudiniCookable::SetNodeIdsToCook(const TArray<int32>& InNodeIds)
{
	NodeIdsToCook = InNodeIds;

	// Remove stale entries from NodesToCookCookCounts:
	TArray<int32> CachedNodeIds;
	NodesToCookCookCounts.GetKeys(CachedNodeIds);
	for (const int32 CachedNodeId : CachedNodeIds)
	{
		if (!NodeIdsToCook.Contains(CachedNodeId))
		{
			NodesToCookCookCounts.Remove(CachedNodeId);
		}
	}
}

void
UHoudiniCookable::MarkAsNeedCook()
{
	MarkAsNeedRecookOrRebuild(false);
}

void
UHoudiniCookable::MarkAsNeedRebuild()
{
	MarkAsNeedRecookOrRebuild(true);
}

void
UHoudiniCookable::MarkAsNeedRecookOrRebuild(bool bDoRebuild)
{
	if (bDoRebuild)
	{
		// Force the asset state to NeedRebuild
		SetCurrentState(EHoudiniAssetState::NeedRebuild);
		CurrentStateResult = EHoudiniAssetStateResult::None;
	}

	// Reset some of the asset's flag
	bHasBeenLoaded = true;
	bPendingDelete = false;
	bFullyLoaded = false; // ?? not needed? was rebuild only
	// Indicate whether a recook or rebuild has been requested
	bRecookRequested = bDoRebuild ? false : true;
	bRebuildRequested = bDoRebuild ? true : false;

	// TODO COOKABLE: This was somehow only for recook ?
	if (IsParameterSupported() && !bDoRebuild)
	{
		// We need to mark all our parameters as changed/trigger update
		for (auto CurrentParam : ParameterData->Parameters)
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
	}

	if (IsOutputSupported())
	{
		// We need to mark all of our editable curves as changed
		for (auto Output : OutputData->Outputs)
		{
			if (!IsValid(Output) || Output->GetType() != EHoudiniOutputType::Curve || !Output->IsEditableNode())
				continue;

			for (auto& OutputObjectEntry : Output->GetOutputObjects())
			{
				FHoudiniOutputObject& OutputObject = OutputObjectEntry.Value;
				if (OutputObject.CurveOutputProperty.CurveOutputType != EHoudiniCurveOutputType::HoudiniSpline)
					continue;

				for (auto Component : OutputObject.OutputComponents)
				{
					UHoudiniSplineComponent* SplineComponent = Cast<UHoudiniSplineComponent>(Component);
					if (!IsValid(SplineComponent))
						continue;

					// This sets bHasChanged and bNeedsToTriggerUpdate
					SplineComponent->MarkChanged(true);
				}
			}
		}
	}

	if (IsInputSupported())
	{
		// We need to mark all our inputs as changed/trigger update
		for (auto CurrentInput : InputData->Inputs)
		{
			if (!IsValid(CurrentInput))
				continue;

			CurrentInput->MarkChanged(true);
			CurrentInput->SetNeedsToTriggerUpdate(true);
			CurrentInput->MarkDataUploadNeeded(true);

			// TODO COOKABLE: Next was recook only somehow?? 
			if (bDoRebuild)
				continue;

			FHoudiniInputObjectSettings CurrentInputSettings(CurrentInput);

			// In addition to marking the input as changed/need update, we also need to make sure that any changes on the
			// Unreal side have been recorded for the input before sending to Houdini. For that we also mark each input
			// object as changed/need update and explicitly call the Update function on each input object. For example, for
			// input actors this would recreate the Houdini input actor components from the actor's components, picking up
			// any new components since the last call to Update.
			TArray<TObjectPtr<UHoudiniInputObject>>* InputObjectArray = CurrentInput->GetHoudiniInputObjectArray(CurrentInput->GetInputType());
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
	}

	// Clear the static mesh bake timer
	if(IsOutputSupported())
		ClearRefineMeshesTimer();
}


void
UHoudiniCookable::PreventAutoUpdates()
{
	// It is important to check this when dealing with Blueprints since the
	// preview components start receiving events from the template component
	// before the preview component have finished initialization.
	if (!IsFullyLoaded())
		return;

	bForceNeedUpdate = false;
	bRecookRequested = false;
	bRebuildRequested = false;

	if(IsComponentSupported())
		ComponentData->bHasComponentTransformChanged = false;

	if (IsParameterSupported())
	{
		// Go through all our parameters, prevent them from triggering updates
		for (auto CurrentParm : ParameterData->Parameters)
		{
			if (!IsValid(CurrentParm))
				continue;

			// Prevent the parm from triggering an update
			CurrentParm->SetNeedsToTriggerUpdate(false);
		}
	}	

	// Same with inputs
	if (IsInputSupported())
	{
		for (auto CurrentInput : InputData->Inputs)
		{
			if (!IsValid(CurrentInput))
				continue;

			// Prevent the input from triggering an update
			CurrentInput->SetNeedsToTriggerUpdate(false);
		}
	}

	if (IsOutputSupported())
	{
		// Go through all outputs, filter the editable nodes.
		for (auto CurrentOutput : OutputData->Outputs)
		{
			if (!IsValid(CurrentOutput))
				continue;

			// We only care about editable outputs
			if (!CurrentOutput->IsEditableNode())
				continue;

			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurrentOutput->GetOutputObjects();
			for (auto& NextPair : OutputObjects)
			{
				// For now, only editable curves can trigger update
				for (auto Component : NextPair.Value.OutputComponents)
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
}

void
UHoudiniCookable::OnSessionConnected()
{
	if (IsParameterSupported())
	{
		for (auto& Param : ParameterData->Parameters)
			Param->OnSessionConnected();
	}
	
	if (IsInputSupported())
	{
		for (auto& Input : InputData->Inputs)
		{
			Input->OnSessionConnected();
		}
	}

	NodeId = INDEX_NONE;
}


UHoudiniParameter*
UHoudiniCookable::FindMatchingParameter(UHoudiniParameter* InOtherParam)
{
	if (!IsValid(InOtherParam))
		return nullptr;

	if (!IsParameterSupported())
		return nullptr;

	for (auto CurrentParam : ParameterData->Parameters)
	{
		if (!IsValid(CurrentParam))
			continue;

		if (CurrentParam->Matches(*InOtherParam))
			return CurrentParam;
	}

	return nullptr;
}

UHoudiniInput*
UHoudiniCookable::FindMatchingInput(UHoudiniInput* InOtherInput)
{
	if (!IsValid(InOtherInput))
		return nullptr;

	if (!IsInputSupported())
		return nullptr;

	for (auto CurrentInput : InputData->Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		if (CurrentInput->Matches(*InOtherInput))
			return CurrentInput;
	}

	return nullptr;
}

UHoudiniHandleComponent*
UHoudiniCookable::FindMatchingHandle(UHoudiniHandleComponent* InOtherHandle)
{
	if (!IsValid(InOtherHandle))
		return nullptr;

	if (!IsComponentSupported())
		return nullptr;

	for (auto CurrentHandle : ComponentData->HandleComponents)
	{
		if (!IsValid(CurrentHandle))
			continue;

		if (CurrentHandle->Matches(*InOtherHandle))
			return CurrentHandle;
	}

	return nullptr;
}

UHoudiniParameter*
UHoudiniCookable::FindParameterByName(const FString& InParamName)
{
	if (!IsParameterSupported())
		return nullptr;

	for (auto CurrentParam : ParameterData->Parameters)
	{
		if (!IsValid(CurrentParam))
			continue;

		if (CurrentParam->GetParameterName().Equals(InParamName))
			return CurrentParam;
	}

	return nullptr;
}


TArray<TObjectPtr<UHoudiniParameter>>&
UHoudiniCookable::GetParameters()
{
	return ParameterData->Parameters;
}

TArray<TObjectPtr<UHoudiniInput>>&
UHoudiniCookable::GetInputs()
{
	return InputData->Inputs;
}

TArray<TObjectPtr<UHoudiniOutput>>&
UHoudiniCookable::GetOutputs()
{
	return OutputData->Outputs;
}

TArray<TObjectPtr<UHoudiniHandleComponent>>&
UHoudiniCookable::GetHandleComponents()
{
	return ComponentData->HandleComponents;
}


void
UHoudiniCookable::GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const
{
	if (IsOutputSupported())
		return;

	for (UHoudiniOutput* Output : OutputData->Outputs)
	{
		OutOutputs.Add(Output);
	}
}

bool
UHoudiniCookable::IsOverrideGlobalProxyStaticMeshSettings() const
{
	if (!IsOutputSupported())
		return false;

	return OutputData->bOverrideGlobalProxyStaticMeshSettings;
}

bool
UHoudiniCookable::IsProxyStaticMeshEnabled() const
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return OutputData->bEnableProxyStaticMeshOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
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
UHoudiniCookable::IsProxyStaticMeshRefinementByTimerEnabled() const
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return OutputData->bEnableProxyStaticMeshOverride && OutputData->bEnableProxyStaticMeshRefinementByTimerOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
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
UHoudiniCookable::GetProxyMeshAutoRefineTimeoutSeconds() const
{
	if (!IsOutputSupported())
		return 5.0f;

	if (OutputData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return OutputData->ProxyMeshAutoRefineTimeoutSecondsOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
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
UHoudiniCookable::IsProxyStaticMeshRefinementOnPreSaveWorldEnabled() const
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return OutputData->bEnableProxyStaticMeshOverride && OutputData->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
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
UHoudiniCookable::IsProxyStaticMeshRefinementOnPreBeginPIEEnabled() const
{
	if (!IsOutputSupported())
		return false;

	if (OutputData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return OutputData->bEnableProxyStaticMeshOverride && OutputData->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;
	}
	else
	{
		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
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

bool
UHoudiniCookable::HasNoProxyMeshNextCookBeenRequested() const
{
	if (!IsOutputSupported())
		return false;

	return OutputData->bNoProxyMeshNextCookRequested;
}

bool
UHoudiniCookable::HasAnyCurrentProxyOutput() const
{
	if (!IsOutputSupported())
		return false;

	for (const UHoudiniOutput* Output : OutputData->Outputs)
	{
		if (Output->HasAnyCurrentProxy())
		{
			return true;
		}
	}

	return false;
}

bool
UHoudiniCookable::HasAnyProxyOutput() const
{
	if (!IsOutputSupported())
		return false;

	for (const UHoudiniOutput* Output : OutputData->Outputs)
	{
		if (Output->HasAnyProxy())
		{
			return true;
		}
	}

	return false;
}

void 
UHoudiniCookable::SetNoProxyMeshNextCookRequested(bool bInNoProxyMeshNextCookRequested)
{
	if (!IsOutputSupported())
		return;

	OutputData->bNoProxyMeshNextCookRequested = bInNoProxyMeshNextCookRequested; 
}


void
UHoudiniCookable::SetOverrideGlobalProxyStaticMeshSettings(bool InEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bOverrideGlobalProxyStaticMeshSettings = InEnable;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshOverride(bool InEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bEnableProxyStaticMeshOverride = InEnable;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshRefinementByTimerOverride(bool InEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bEnableProxyStaticMeshRefinementByTimerOverride = InEnable;
}

void
UHoudiniCookable::SetProxyMeshAutoRefineTimeoutSecondsOverride(float InValue)
{
	if (!IsOutputSupported())
		return;

	OutputData->ProxyMeshAutoRefineTimeoutSecondsOverride = InValue;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bool InEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = InEnable;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bool InEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = InEnable;
}


void
UHoudiniCookable::SetBakeAfterNextCook(const EHoudiniBakeAfterNextCook InBakeAfterNextCook)
{
	if (!IsOutputSupported())
		return;

	OutputData->BakeAfterNextCook = InBakeAfterNextCook;
}

void
UHoudiniCookable::SetActorBakeOption(const EHoudiniEngineActorBakeOption InBakeOption)
{
	if (!IsOutputSupported())
		return;

	OutputData->ActorBakeOption = InBakeOption;
}

void
UHoudiniCookable::SetAllowPlayInEditorRefinement(bool bEnabled)
{
	if (IsOutputSupported())
		return;

	OutputData->bAllowPlayInEditorRefinement = bEnabled;
}

bool
UHoudiniCookable::IsPlayInEditorRefinementAllowed() const
{
	if (IsOutputSupported())
		return false;

	return OutputData->bAllowPlayInEditorRefinement;
}


bool
UHoudiniCookable::IsHoudiniCookedDataAvailable(bool& bOutNeedsRebuildOrDelete, bool& bOutInvalidState) const
{
	// Get the state of the asset and check if it is pre-cook, cooked, pending delete/rebuild or invalid
	bOutNeedsRebuildOrDelete = false;
	bOutInvalidState = false;
	switch (CurrentState)
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
UHoudiniCookable::IsBakeAfterNextCookEnabled() const 
{
	if (!IsOutputSupported())
		return false;

	return OutputData->BakeAfterNextCook != EHoudiniBakeAfterNextCook::Disabled; 
}

EHoudiniBakeAfterNextCook
UHoudiniCookable::GetBakeAfterNextCook() const
{
	if (!IsOutputSupported())
		return EHoudiniBakeAfterNextCook::Disabled;

	return OutputData->BakeAfterNextCook;
}

EHoudiniEngineActorBakeOption
UHoudiniCookable::GetActorBakeOption() const
{
	return OutputData->ActorBakeOption;
}

TArray<FHoudiniBakedOutput>& 
UHoudiniCookable::GetBakedOutputs() 
{
	return OutputData->BakedOutputs;
}

const TArray<FHoudiniBakedOutput>&
UHoudiniCookable::GetBakedOutputs() const
{ 
	return OutputData->BakedOutputs;
}


EHoudiniEngineBakeOption
UHoudiniCookable::GetHoudiniEngineBakeOption() const
{
	if (!IsOutputSupported())
		return EHoudiniEngineBakeOption::ToActor;

	return OutputData->HoudiniEngineBakeOption;
}

void
UHoudiniCookable::SetHoudiniEngineBakeOption(const EHoudiniEngineBakeOption& InBakeOption)
{
	if (!IsOutputSupported())
		return;

	OutputData->HoudiniEngineBakeOption = InBakeOption;
}

bool
UHoudiniCookable::GetReplacePreviousBake() const
{
	return OutputData->bReplacePreviousBake;
}

void
UHoudiniCookable::SetReplacePreviousBake(bool bInReplace)
{
	if (!IsOutputSupported())
		return;

	OutputData->bReplacePreviousBake = bInReplace;
}

bool
UHoudiniCookable::GetRemoveOutputAfterBake() const
{
	return OutputData->bRemoveOutputAfterBake;
}

void
UHoudiniCookable::SetRemoveOutputAfterBake(bool bInRemove)
{
	if (!IsOutputSupported())
		return;

	OutputData->bRemoveOutputAfterBake = bInRemove;
}

bool
UHoudiniCookable::GetRecenterBakedActors() const
{
	return OutputData->bRecenterBakedActors;
}

void
UHoudiniCookable::SetRecenterBakedActors(bool bInRecenter)
{
	if (!IsOutputSupported())
		return;

	OutputData->bRecenterBakedActors = bInRecenter;
}

bool
UHoudiniCookable::GetCookOnParameterChange() const
{
	return ParameterData->bCookOnParameterChange;
}

bool
UHoudiniCookable::GetCookOnTransformChange() const
{
	return ComponentData->bCookOnTransformChange;
}
/*
bool
UHoudiniCookable::GetCookOnAssetInputCook()
{
	return bCookOnAssetInputCook;
}
*/

bool
UHoudiniCookable::IsOutputless() const
{
	return OutputData->bOutputless;
}

bool
UHoudiniCookable::GetUseOutputNodes() const
{
	return OutputData->bUseOutputNodes;
}

bool
UHoudiniCookable::GetOutputTemplateGeos() const
{
	return OutputData->bOutputTemplateGeos;
}

bool
UHoudiniCookable::GetUploadTransformsToHoudiniEngine() const
{
	return ComponentData->bUploadTransformsToHoudiniEngine;
}

bool
UHoudiniCookable::GetLandscapeUseTempLayers() const
{
	return OutputData->bLandscapeUseTempLayers;
}

bool
UHoudiniCookable::GetEnableCurveEditing() const
{
	return OutputData->bEnableCurveEditing;
}

bool
UHoudiniCookable::GetSplitMeshSupport() const
{
	return OutputData->bSplitMeshSupport;
}


FHoudiniStaticMeshGenerationProperties
UHoudiniCookable::GetStaticMeshGenerationProperties()
{
	return OutputData->StaticMeshGenerationProperties;
}

FMeshBuildSettings
UHoudiniCookable::GetStaticMeshBuildSettings()
{
	return OutputData->StaticMeshBuildSettings;
}

void
UHoudiniCookable::SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InHSMGP)
{
	if (!IsOutputSupported())
		return;

	 OutputData->StaticMeshGenerationProperties = InHSMGP;
};

void
UHoudiniCookable::SetStaticMeshBuildSettings(const FMeshBuildSettings& InMBS)
{
	if (!IsOutputSupported())
		return;

	OutputData->StaticMeshBuildSettings = InMBS;
};

void
UHoudiniCookable::SetCookOnParameterChange(bool bEnable)
{
	if (!IsParameterSupported())
		return;

	ParameterData->bCookOnParameterChange = bEnable;
}

void
UHoudiniCookable::SetCookOnTransformChange(bool bEnable)
{
	if (!IsComponentSupported())
		return;

	ComponentData->bCookOnTransformChange = bEnable;
}

bool
UHoudiniCookable::WasLastCookSuccessful() const
{
	return bLastCookSuccess;
}

/*
void
UHoudiniCookable::SetCookOnAssetInputCook(bool bEnable)
{
	if (!IsParameterSupported())
		return;

	OutputData->bCookOnAssetInputCook = bEnable;
}
*/

void
UHoudiniCookable::SetOutputless(bool bEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bOutputless = bEnable;
}

void
UHoudiniCookable::SetUseOutputNodes(bool bEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bUseOutputNodes = bEnable;
}

void
UHoudiniCookable::SetOutputTemplateGeos(bool bEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bOutputTemplateGeos = bEnable;
}

void
UHoudiniCookable::SetUploadTransformsToHoudiniEngine(bool bEnable)
{
	if (!IsComponentSupported())
		return;

	ComponentData->bUploadTransformsToHoudiniEngine = bEnable;
}

void
UHoudiniCookable::SetLandscapeUseTempLayers(bool bEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bLandscapeUseTempLayers = bEnable;
}

void
UHoudiniCookable::SetEnableCurveEditing(bool bEnable)
{
	if (!IsOutputSupported())
		return;

	OutputData->bEnableCurveEditing = bEnable;
}