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
#include "HoudiniAssetBlueprintComponent.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniStaticMeshComponent.h"
#if WITH_EDITOR
	#include "HoudiniEditorAssetStateSubsystemInterface.h"
#endif

#include "HoudiniParameterInt.h"
#include "Components/SplineComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
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
	, bCookOnCookableInputCook(true)
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
	, bOutputless(false)
	, bCreateSceneComponents (true)
	, bOutputTemplateGeos(false)
	, bUseOutputNodes(true)
	, bSplitMeshSupport(false)
	, bEnableCurveEditing(true)
	, bLandscapeUseTempLayers(false)
{
	StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	StaticMeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
}


//
// BAKE DATA
//
UCookableBakingData::UCookableBakingData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, HoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToActor)
	, BakeFolder()
	, BakeAfterNextCook(EHoudiniBakeAfterNextCook::Disabled)
	, bRemoveOutputAfterBake(false)
	, bRecenterBakedActors(false)
	, bReplacePreviousBake(false)
	, ActorBakeOption(EHoudiniEngineActorBakeOption::OneActorPerComponent)
{

}


//
// PROXY DATA
//
UCookableProxyData::UCookableProxyData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bNoProxyMeshNextCookRequested(false)
	, bOverrideGlobalProxyStaticMeshSettings(false)
	, bEnableProxyStaticMeshOverride(false)
	, bEnableProxyStaticMeshRefinementByTimerOverride(true)
	, ProxyMeshAutoRefineTimeoutSecondsOverride(10.0f)
	, bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(true)
	, bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(true)
	, bAllowPlayInEditorRefinement(false)
{
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

	//bCanDeleteHoudiniNodes = true;

	bHasHoudiniAsset = false;
	HoudiniAssetData = CreateDefaultSubobject<UCookableHoudiniAssetData>(TEXT("HoudiniAssetData")); 
	//HoudiniAssetData = NewObject<UCookableHoudiniAssetData>(this, TEXT("HoudiniAssetData"), RF_Public);

	bHasInputs = false;
	InputData = CreateDefaultSubobject<UCookableInputData>(TEXT("InputData"));

	bHasParameters = false;
	ParameterData = CreateDefaultSubobject<UCookableParameterData>(TEXT("ParameterData"));
	//ParameterData = NewObject<UCookableParameterData>(this, TEXT("ParameterData"), RF_Public);

	bHasComponent = false;
	ComponentData = CreateDefaultSubobject<UCookableComponentData>(TEXT("ComponentData"));

	bHasOutputs = false;
	OutputData = CreateDefaultSubobject<UCookableOutputData>(TEXT("OutputData"));

	bHasPDG = false;
	PDGData = CreateDefaultSubobject<UCookablePDGData>(TEXT("PDGData"));

	bHasBaking = false;
	BakingData = CreateDefaultSubobject<UCookableBakingData>(TEXT("BakingData"));

	bHasProxy = false;
	ProxyData = CreateDefaultSubobject<UCookableProxyData>(TEXT("ProxyData"));

	bNeedToUpdateEditorProperties = false;
	bIsPCG = false;
	bIsLandscapeModification = true;
	bDoSlateNotifications = true;
	bAllowUpdateEditorProperties = true;

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
#if WITH_EDITORONLY_DATA
	bGenerateMenuExpanded = true;
	bBakeMenuExpanded = true;
	bAssetOptionMenuExpanded = true;
	bHelpAndDebugMenuExpanded = true;
#endif

	AssetEditorId = FName();
}

UHoudiniCookable::~UHoudiniCookable()
{
	// Handled by GC
	// Unregister ourself so our houdini nodes can be deleted.
	// FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(this);
}

template <typename Type> bool
HoudiniCheckAndSetValue(Type & Dest, Type & Src)
{
	if(Dest == Src)
		return false;
	Dest = Src;
	return true;
}

bool
HoudiniAreObjectsEqual(const UObject* A, const UObject*B)
{
	if(!IsValid(A) && !IsValid(B))
		return true;

	if(!IsValid(A) || !IsValid(B))
		return false;

	if(A->GetClass() != B->GetClass())
		return false;

	for(TFieldIterator<FProperty> PropIt(A->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		const void* ValueA = Property->ContainerPtrToValuePtr<void>(A);
		const void* ValueB = Property->ContainerPtrToValuePtr<void>(B);

		if(!Property->Identical(ValueA, ValueB))
		{
			return false;
		}
	}
	return true;
}


bool 
UHoudiniCookable::SetParameterData(UCookableParameterData* InParameterData)
{
	bool bChanged = false;
	bChanged |= HoudiniCheckAndSetValue(ParameterData->bCookOnParameterChange, InParameterData->bCookOnParameterChange);
	bChanged |= HoudiniCheckAndSetValue(ParameterData->ParameterPresetBuffer, InParameterData->ParameterPresetBuffer);
	bChanged |= HoudiniCheckAndSetValue(ParameterData->bParameterDefinitionUpdateNeeded, InParameterData->bParameterDefinitionUpdateNeeded);

	if(ParameterData->Parameters.Num() != InParameterData->Parameters.Num())
	{
		ParameterData->Parameters.SetNum(InParameterData->Parameters.Num());
		bChanged = true;
	}

	for (int Index = 0; Index < ParameterData->Parameters.Num(); Index++)
	{
		if (!HoudiniAreObjectsEqual(ParameterData->Parameters[Index], InParameterData->Parameters[Index]))
		{
			bChanged = true;
			if(IsValid(InParameterData->Parameters[Index]))
			{

		//		ParameterData->Parameters[Index] = DuplicateObject(InParameterData->Parameters[Index], ParameterData);
				ParameterData->Parameters[Index] = NewObject<UHoudiniParameter>(ParameterData, InParameterData->Parameters[Index].GetClass());
			//	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
				UEngine::CopyPropertiesForUnrelatedObjects(InParameterData->Parameters[Index], ParameterData->Parameters[Index]);

				ParameterData->Parameters[Index]->MarkChanged(true);
			}
			else
			{
				ParameterData->Parameters[Index] = nullptr;
			}
		}
	}

	return bChanged;
}

bool
UHoudiniCookable::SetInputData(UCookableInputData* InInputData)
{
	bool bChanged = false;
	bChanged |= HoudiniCheckAndSetValue(InputData->bCookOnInputChange, InInputData->bCookOnInputChange);
	bChanged |= HoudiniCheckAndSetValue(InputData->bCookOnCookableInputCook, InInputData->bCookOnCookableInputCook);

	if(InputData->Inputs.Num() != InInputData->Inputs.Num())
	{
		InputData->Inputs.SetNum(InInputData->Inputs.Num());
		bChanged = true;
	}

	for(int Index = 0; Index < InputData->Inputs.Num(); Index++)
	{
		if(!HoudiniAreObjectsEqual(InputData->Inputs[Index], InInputData->Inputs[Index]))
		{
			bChanged = true;
			if(IsValid(InInputData->Inputs[Index]))
			{
				InputData->Inputs[Index] = DuplicateObject(InInputData->Inputs[Index], this);
				InputData->Inputs[Index]->MarkChanged(true);
			}
			else
			{
				InputData->Inputs[Index] = nullptr;
			}
		}
	}

	return bChanged;
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
	// Should we return GetComponent()->GetWold() first? it should be same as the Actor's world...
	return GetOwner() ? GetOwner()->GetWorld() : nullptr;
}


ULevel* 
UHoudiniCookable::GetLevel() const
{
	USceneComponent* MyComp = GetComponent();
	AActor* MyOwner = MyComp ? MyComp->GetOwner() : nullptr;

	if (MyOwner)
		return MyOwner->GetLevel();
	
	if(MyComp)
		return MyComp->GetTypedOuter<ULevel>();
	
	return nullptr;
}

FDirectoryPath
UHoudiniCookable::GetBakeFolder() const
{
	return BakingData->BakeFolder;
}

FDirectoryPath
UHoudiniCookable::GetTemporaryCookFolder() const
{
	return OutputData->TemporaryCookFolder;
}

FString
UHoudiniCookable::GetTemporaryCookFolderOrDefault() const
{
	return !OutputData->TemporaryCookFolder.Path.IsEmpty() ? OutputData->TemporaryCookFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
}

FString
UHoudiniCookable::GetBakeFolderOrDefault() const
{
	return !BakingData->BakeFolder.Path.IsEmpty() ? BakingData->BakeFolder.Path : FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
}

FGuid&
UHoudiniCookable::GetHapiGUID()
{
	return HapiGUID;
}

FString
UHoudiniCookable::GetHapiAssetName() const
{
	if(IsHoudiniAssetSupported())
		return HoudiniAssetData->HapiAssetName;

	// TODO COOKABLE: return empty? return name?
	return NodeName;
}

FGuid
UHoudiniCookable::GetCookableGUID() const
{
	return CookableGUID;
}

bool
UHoudiniCookable::SetTemporaryCookFolderPath(const FString& NewPath)
{
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
	if (BakingData->BakeFolder.Path.Equals(NewPath))
		return false;

	if (BakingData->BakeFolder.Path == NewPath)
		return false;

	BakingData->BakeFolder.Path = NewPath;

	return true;
}

bool
UHoudiniCookable::SetTemporaryCookFolder(const FDirectoryPath& InPath)
{
	if (OutputData->TemporaryCookFolder.Path.Equals(InPath.Path))
		return false;

	OutputData->TemporaryCookFolder = InPath;

	return true;
}

bool
UHoudiniCookable::SetBakeFolder(const FDirectoryPath& InPath)
{
	if (BakingData->BakeFolder.Path.Equals(InPath.Path))
		return false;

	BakingData->BakeFolder = InPath;

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

	if (GetComponent())
	{
		// We dont want NodeSync components to automatically start sessions
		if (GetComponent()->IsA<UHoudiniNodeSyncComponent>())
			return false;
	}

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

void
UHoudiniCookable::OnHoudiniAssetChanged()
{
	// TODO: clear input/params/outputs?
	if(IsParameterSupported())
		ParameterData->Parameters.Empty();

	if (IsInputSupported())
		InputData->Inputs.Empty();

	if (IsOutputSupported())
		OutputData->Outputs.Empty();

	// The asset has been changed, mark us as needing to be reinstantiated
	MarkAsNeedInstantiation();

	// Force an update on the next tick
	bForceNeedUpdate = true;
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
UHoudiniCookable::SetCurrentStateResult(EHoudiniAssetStateResult InResult)
{
	CurrentStateResult = InResult;
}

void
UHoudiniCookable::HandleOnHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState)
{
	IHoudiniAssetStateEvents::HandleOnHoudiniAssetStateChange(InHoudiniAssetContext, InFromState, InToState);

	if (InFromState == InToState)
		return;

	if (this != InHoudiniAssetContext)
		return;

	FOnAssetStateChangeDelegate StateChangedDelegate = GetOnAssetStateChangeDelegate();
	if (StateChangedDelegate.IsBound())
		StateChangedDelegate.Broadcast(this, InFromState, InToState);

	FOnCookableStateChangeDelegate& CookableStateChangeDelegate = GetOnCookableStateChangeDelegate();
	if (CookableStateChangeDelegate.IsBound())
		CookableStateChangeDelegate.Broadcast(this, InFromState, InToState);

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
UHoudiniCookable::QueuePreCookCallback(const TFunction<void(UHoudiniCookable*)>& CallbackFn)
{
	PreCookCallbacks.Add(CallbackFn);
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
UHoudiniCookable::HandleOnPostBake(bool bInSuccess)
{
	if (BakingData->OnPostBakeDelegate.IsBound())
		BakingData->OnPostBakeDelegate.Broadcast(this, bInSuccess);
}

void
UHoudiniCookable::UpdateDormantStatus()
{
#if WITH_EDITOR
	// This function checks if we should go into or out of dormant status.
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
UHoudiniCookable::IsInputTypeSupported(EHoudiniInputType InType)
{ 
	if (!IsInputSupported())
		return false;

	if (GetComponent())
	{
		// If we have a component, let it decide what input types are supported
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(GetComponent());
		if (MyHAC)
			return MyHAC->IsInputTypeSupported(InType);
	}

	return true; 
};

bool
UHoudiniCookable::IsOutputTypeSupported(EHoudiniOutputType InType)
{
	if (!IsOutputSupported())
		return false;

	if (GetComponent())
	{
		// If we have a component, let it decide what output types it supprots
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(GetComponent());
		if (MyHAC)
			return MyHAC->IsOutputTypeSupported(InType);
	}

	return true; 
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
	if (IsComponentSupported() && IsValid(ComponentData->Component.Get()))
	{
		// TODO:
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
				if(ComponentToRemove->GetOwner())
					ComponentToRemove->UnregisterComponent();

				ComponentToRemove->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);				
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
	if (!IsProxySupported())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	World->GetTimerManager().ClearTimer(ProxyData->RefineMeshesTimer);
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
	// Indicate whether a recook or rebuild has been requested
	bRecookRequested = bDoRebuild ? false : true;
	bRebuildRequested = bDoRebuild ? true : false;
	// ?? only when doing a rebuild
	if (bDoRebuild)
		bFullyLoaded = false;

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


// Marks the asset as needing to be instantiated
void
UHoudiniCookable::MarkAsNeedInstantiation()
{
	// Invalidate the asset ID
	NodeId = -1;

	if((IsParameterSupported() && ParameterData->Parameters.Num() <= 0)
		&& (IsInputSupported() && InputData->Inputs.Num() <= 0)
		&& (IsOutputSupported() && OutputData->Outputs.Num() <= 0))
	{
		// The asset has no parameters or inputs.
		// This likely indicates it has never cooked/been instantiated.
		// Set its state to NewHDA to force its instantiation
		// so that we can have its parameters/input interface
		SetCurrentState(EHoudiniAssetState::NewHDA);
	}
	else
	{
		// The asset has cooked before since we have a parameter/input interface
		// Set its state to need instantiation so that the asset is instantiated
		// after being modified
		SetCurrentState(EHoudiniAssetState::NeedInstantiation);
	}

	CurrentStateResult = EHoudiniAssetStateResult::None;

	// Reset some of the asset's flag
	CookCount = 0;
	bHasBeenLoaded = true;
	bPendingDelete = false;
	bRecookRequested = false;
	bRebuildRequested = false;
	bFullyLoaded = false;

	//bEditorPropertiesNeedFullUpdate = true;

	if (IsParameterSupported())
	{
		// We need to mark all our parameters as changed/not triggering update
		for (auto CurrentParam : ParameterData->Parameters)
		{
			if (CurrentParam)
			{
				CurrentParam->MarkChanged(true);
				CurrentParam->SetNeedsToTriggerUpdate(false);
			}
		}
	}
	
	if (IsInputSupported())
	{
		// We need to mark all our inputs as changed/not triggering update
		for (auto CurrentInput : InputData->Inputs)
		{
			if (CurrentInput)
			{
				CurrentInput->MarkChanged(true);
				CurrentInput->SetNeedsToTriggerUpdate(false);
				CurrentInput->MarkDataUploadNeeded(true);
			}
		}
	}

	// Clear the static mesh bake timer
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

const TArray<TObjectPtr<UHoudiniParameter>>&
UHoudiniCookable::GetParameters() const
{
	return ParameterData->Parameters;
}

TArray<TObjectPtr<UHoudiniInput>>&
UHoudiniCookable::GetInputs()
{
	return InputData->Inputs;
}

const TArray<TObjectPtr<UHoudiniInput>>&
UHoudiniCookable::GetInputs() const
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
	if (!IsOutputSupported())
		return;

	for (UHoudiniOutput* Output : OutputData->Outputs)
	{
		OutOutputs.Add(Output);
	}
}

TArray<int32>
UHoudiniCookable::GetNodeIdsToCook() const
{
	return NodeIdsToCook;
}

TMap<int32, int32>
UHoudiniCookable::GetNodesToCookCookCounts() const
{
	return NodesToCookCookCounts;
}

bool
UHoudiniCookable::IsOverrideGlobalProxyStaticMeshSettings() const
{
	return ProxyData->bOverrideGlobalProxyStaticMeshSettings;
}

bool
UHoudiniCookable::IsProxyStaticMeshEnabled() const
{
	// BP don't support Proxies for now
	if (GetComponent())
	{
		if (GetComponent()->IsA<UHoudiniAssetBlueprintComponent>())
			return false;
	}

	if (!IsProxySupported())
		return false;

	if (ProxyData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return ProxyData->bEnableProxyStaticMeshOverride;
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
	if (!IsProxySupported())
		return false;

	if (ProxyData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return ProxyData->bEnableProxyStaticMeshOverride && ProxyData->bEnableProxyStaticMeshRefinementByTimerOverride;
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
	if (ProxyData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return ProxyData->ProxyMeshAutoRefineTimeoutSecondsOverride;
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
	if (!IsProxySupported())
		return false;

	if (ProxyData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return ProxyData->bEnableProxyStaticMeshOverride && ProxyData->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;
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
	if (!IsProxySupported())
		return false;

	if (ProxyData->bOverrideGlobalProxyStaticMeshSettings)
	{
		return ProxyData->bEnableProxyStaticMeshOverride && ProxyData->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;
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
UHoudiniCookable::HasAnyOutputComponent() const
{
	if (!IsOutputSupported())
		return false;

	for (UHoudiniOutput* Output : OutputData->Outputs)
	{
		for (auto& CurrentOutputObject : Output->GetOutputObjects())
		{
			for (auto Component : CurrentOutputObject.Value.OutputComponents)
			{
				if (Component)
					return true;
			}
		}
	}

	return false;
}

bool
UHoudiniCookable::HasNoProxyMeshNextCookBeenRequested() const
{
	if (!IsProxySupported())
		return false;

	return ProxyData->bNoProxyMeshNextCookRequested;
}

bool
UHoudiniCookable::HasAnyCurrentProxyOutput() const
{
	if (!IsProxySupported())
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
	ProxyData->bNoProxyMeshNextCookRequested = bInNoProxyMeshNextCookRequested; 
}


void
UHoudiniCookable::SetOverrideGlobalProxyStaticMeshSettings(bool InEnable)
{
	ProxyData->bOverrideGlobalProxyStaticMeshSettings = InEnable;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshOverride(bool InEnable)
{
	ProxyData->bEnableProxyStaticMeshOverride = InEnable;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshRefinementByTimerOverride(bool InEnable)
{
	ProxyData->bEnableProxyStaticMeshRefinementByTimerOverride = InEnable;
}

void
UHoudiniCookable::SetProxyMeshAutoRefineTimeoutSecondsOverride(float InValue)
{
	ProxyData->ProxyMeshAutoRefineTimeoutSecondsOverride = InValue;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bool InEnable)
{
	ProxyData->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = InEnable;
}

void
UHoudiniCookable::SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bool InEnable)
{
	ProxyData->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = InEnable;
}


void
UHoudiniCookable::SetBakeAfterNextCook(const EHoudiniBakeAfterNextCook InBakeAfterNextCook)
{
	BakingData->BakeAfterNextCook = InBakeAfterNextCook;
}

void
UHoudiniCookable::SetActorBakeOption(const EHoudiniEngineActorBakeOption InBakeOption)
{
	BakingData->ActorBakeOption = InBakeOption;
}

void
UHoudiniCookable::SetAllowPlayInEditorRefinement(bool bEnabled)
{
	ProxyData->bAllowPlayInEditorRefinement = bEnabled;
}

bool
UHoudiniCookable::IsPlayInEditorRefinementAllowed() const
{
	return ProxyData->bAllowPlayInEditorRefinement;
}

void
UHoudiniCookable::SetRefineMeshesTimer()
{
	if (!IsProxySupported())
		return;

	UWorld* World = GetWorld();
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
		World->GetTimerManager().SetTimer(ProxyData->RefineMeshesTimer, this, &UHoudiniCookable::OnRefineMeshesTimerFired, 1.0f, false, TimeSeconds);
	}
	else
	{
		World->GetTimerManager().ClearTimer(ProxyData->RefineMeshesTimer);
	}
}

void
UHoudiniCookable::OnRefineMeshesTimerFired()
{
	if (!IsProxySupported())
		return;

	HOUDINI_LOG_MESSAGE(TEXT("UHoudiniAssetComponent::OnRefineMeshesTimerFired()"));
	if (ProxyData->OnRefineMeshesTimerDelegate.IsBound())
	{
		ProxyData->OnRefineMeshesTimerDelegate.Broadcast(this);
	}
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
	return BakingData->BakeAfterNextCook != EHoudiniBakeAfterNextCook::Disabled; 
}

EHoudiniBakeAfterNextCook
UHoudiniCookable::GetBakeAfterNextCook() const
{
	return BakingData->BakeAfterNextCook;
}

EHoudiniEngineActorBakeOption
UHoudiniCookable::GetActorBakeOption() const
{
	return BakingData->ActorBakeOption;
}

TArray<FHoudiniBakedOutput>& 
UHoudiniCookable::GetBakedOutputs() 
{
	return BakingData->BakedOutputs;
}

const TArray<FHoudiniBakedOutput>&
UHoudiniCookable::GetBakedOutputs() const
{ 
	return BakingData->BakedOutputs;
}


EHoudiniEngineBakeOption
UHoudiniCookable::GetHoudiniEngineBakeOption() const
{
	return BakingData->HoudiniEngineBakeOption;
}

void
UHoudiniCookable::SetHoudiniEngineBakeOption(const EHoudiniEngineBakeOption& InBakeOption)
{
	BakingData->HoudiniEngineBakeOption = InBakeOption;
}

bool
UHoudiniCookable::GetReplacePreviousBake() const
{
	return BakingData->bReplacePreviousBake;
}

void
UHoudiniCookable::SetReplacePreviousBake(bool bInReplace)
{
	BakingData->bReplacePreviousBake = bInReplace;
}

bool
UHoudiniCookable::GetRemoveOutputAfterBake() const
{
	return BakingData->bRemoveOutputAfterBake;
}

void
UHoudiniCookable::SetRemoveOutputAfterBake(bool bInRemove)
{
	BakingData->bRemoveOutputAfterBake = bInRemove;
}

bool
UHoudiniCookable::GetRecenterBakedActors() const
{
	return BakingData->bRecenterBakedActors;
}

void
UHoudiniCookable::SetRecenterBakedActors(bool bInRecenter)
{
	BakingData->bRecenterBakedActors = bInRecenter;
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

bool
UHoudiniCookable::GetCookOnInputChange() const
{
	return InputData->bCookOnInputChange;
}

bool
UHoudiniCookable::GetCookOnCookableInputCook() const
{
	return InputData->bCookOnCookableInputCook;
}

bool
UHoudiniCookable::IsInstantiatingOrCooking() const
{
	return HapiGUID.IsValid();
}

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

FTransform
UHoudiniCookable::GetLastComponentTransform() const
{
	return ComponentData->LastComponentTransform;
}

FTransform
UHoudiniCookable::GetComponentTransform() const 
{
	if (!IsComponentSupported())
		return FTransform::Identity;

	if (!GetComponent())
		return FTransform::Identity;

	return GetComponent()->GetComponentTransform();
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
UHoudiniCookable::GetStaticMeshGenerationProperties() const
{
	return OutputData->StaticMeshGenerationProperties;
}

FMeshBuildSettings
UHoudiniCookable::GetStaticMeshBuildSettings() const
{
	return OutputData->StaticMeshBuildSettings;
}

FHoudiniStaticMeshGenerationProperties&
UHoudiniCookable::GetStaticMeshGenerationProperties()
{
	return OutputData->StaticMeshGenerationProperties;
}

FMeshBuildSettings&
UHoudiniCookable::GetStaticMeshBuildSettings()
{
	return OutputData->StaticMeshBuildSettings;
}

bool
UHoudiniCookable::CanDeleteHoudiniNodes() const
{ 
	// TODO: Cookable - Make this a member instead of relying on components?
	// Our Component dictates if we're allowed to delete our nodes
	if (IsComponentSupported() && GetComponent())
	{
		UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(GetComponent());
		if (IsValid(HAC))
			return HAC->CanDeleteHoudiniNodes();
	}

	return true;
}

void
UHoudiniCookable::SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InHSMGP)
{
	 OutputData->StaticMeshGenerationProperties = InHSMGP;
};

void
UHoudiniCookable::SetStaticMeshBuildSettings(const FMeshBuildSettings& InMBS)
{
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

void
UHoudiniCookable::SetCookOnCookableInputCook(bool bEnable)
{
	if (!IsInputSupported())
		return;

	InputData->bCookOnCookableInputCook = bEnable;
}

void
UHoudiniCookable::SetCookingEnabled(const bool& bInCookingEnabled)
{
	bEnableCooking = bInCookingEnabled;
}

void
UHoudiniCookable::SetHasBeenLoaded(const bool& InLoaded)
{
	bHasBeenLoaded = InLoaded;
}

void
UHoudiniCookable::SetHasBeenDuplicated(const bool& InDuplicated)
{
	bHasBeenDuplicated = InDuplicated;
}

void
UHoudiniCookable::SetCookCount(const int32& InCount)
{
	CookCount = InCount;
}

void
UHoudiniCookable::SetRecookRequested(const bool& InRecook)
{
	bRecookRequested = InRecook;
}

void
UHoudiniCookable::SetRebuildRequested(const bool& InRebuild)
{
	bRebuildRequested = InRebuild;
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
	OutputData->bLandscapeUseTempLayers = bEnable;
}

void
UHoudiniCookable::SetEnableCurveEditing(bool bEnable)
{
	OutputData->bEnableCurveEditing = bEnable;
}


void
UHoudiniCookable::OnDestroy(bool bDestroyingHierarchy)
{
	if(IsHoudiniAssetSupported())
		HoudiniAssetData->HoudiniAsset = nullptr;

	if (IsParameterSupported())
	{
		// Clear Parameters
		for (TObjectPtr<UHoudiniParameter>& CurrentParm : ParameterData->Parameters)
		{
			if (IsValid(CurrentParm))
			{
				CurrentParm->ConditionalBeginDestroy();
			}
			else if (GetWorld() != nullptr && GetWorld()->WorldType != EWorldType::PIE)
			{
				// TODO unneeded log?
				// Avoid spamming that error when leaving PIE mode
				HOUDINI_LOG_WARNING(TEXT("%s: null parameter when clearing"), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
			}
			CurrentParm = nullptr;
		}

		ParameterData->Parameters.Empty();
	}

	if (IsInputSupported())
	{
		// Clear Inputs
		for (TObjectPtr<UHoudiniInput>& CurrentInput : InputData->Inputs)
		{
			if (!IsValid(CurrentInput))
				continue;

			if (CurrentInput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
				continue;

			// Destroy connected Houdini asset.
			CurrentInput->ConditionalBeginDestroy();
			CurrentInput = nullptr;
		}

		InputData->Inputs.Empty();
	}

	if (IsOutputSupported())
	{
		// Clear Output
		for (TObjectPtr<UHoudiniOutput>& CurrentOutput : OutputData->Outputs)
		{
			if (!IsValid(CurrentOutput))
				continue;

			if (CurrentOutput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
				continue;

			// Destroy all Houdini created socket actors.
			TArray<TObjectPtr<AActor>>& CurCreatedSocketActors = CurrentOutput->GetHoudiniCreatedSocketActors();
			for (auto& CurCreatedActor : CurCreatedSocketActors)
			{
				if (!IsValid(CurCreatedActor))
					continue;

				CurCreatedActor->Destroy();
			}
			CurCreatedSocketActors.Empty();

			// Detach all Houdini attached socket actors
			TArray<TObjectPtr<AActor>>& CurAttachedSocketActors = CurrentOutput->GetHoudiniAttachedSocketActors();
			for (auto& CurAttachedSocketActor : CurAttachedSocketActors)
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
				for (int Index = 0; Index < CurrentOutputObject.Value.OutputComponents.Num(); Index++)
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

					UFoliageType* FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(FoliageSM);
					if (!IsValid(FoliageType))
						continue;

					if (IsInGameThread() && IsGarbageCollecting())
					{
						// Calling DeleteInstancesForComponent during GC will cause unreal to crash... 
						HOUDINI_LOG_WARNING(TEXT("%s: Unable to clear foliage instances because of GC"), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
					}
					else
					{
						// Clean up the instances generated for that component
						InstancedFoliageActor->DeleteInstancesForComponent(GetComponent(), FoliageType);
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

		OutputData->Outputs.Empty();
	}

	// Clear the static mesh bake timer
	ClearRefineMeshesTimer();

	// Clear all TOP data and temporary geo/objects from the PDG asset link (if valid)
	if (IsPDGSupported() && IsValid(PDGData->PDGAssetLink))
	{
#if WITH_EDITOR
		const UWorld* const World = GetWorld();
		if (IsValid(World))
		{
			// Only do this for editor worlds, only interactively (not during engine shutdown or garbage collection)
			if (World->WorldType == EWorldType::Editor && GIsRunning && !GIsGarbageCollecting)
			{
				// In case we are recording a transaction (undo, for example) notify that the object will be
				// modified.
				PDGData->PDGAssetLink->Modify();
				PDGData->PDGAssetLink->ClearAllTOPData();
				PDGData->PDGAssetLink->ConditionalBeginDestroy();
			}
		}
#endif
	}

	// Unregister ourself so our houdini node can be deleted
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(this);
}

bool
UHoudiniCookable::NotifyCookedToDownstreamCookables()
{
	// Before notifying, clean up our downstream cookables
	// - check that they are still valid
	// - check that we are still connected to one of its inputs
	// - check that the asset has the CookOnAssetInputCook trigger enabled
	TArray<UHoudiniCookable*> DownstreamToDelete;
	for (auto& CurrentDownstreamHC : InputData->DownstreamCookables)
	{
		// Remove the downstream connection by default,
		// unless we actually were properly connected to one of this HDA's input.
		bool bRemoveDownstream = true;
		if (IsValid(CurrentDownstreamHC))
		{
			// Go through the HAC's input
			for (auto& CurrentDownstreamInput : CurrentDownstreamHC->GetInputs())
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
					const TArray<TObjectPtr<UHoudiniInputObject>>* ObjectArray = CurrentDownstreamInput->GetHoudiniInputObjectArray(CurrentDownstreamInputType);
					if (ObjectArray)
					{
						for (auto& CurrentInputObject : (*ObjectArray))
						{
							if (!IsValid(CurrentInputObject))
								continue;

							if (CurrentInputObject->GetObject() != this)
								continue;

							CurrentInputObject->SetInputNodeId(GetNodeId());
							CurrentInputObject->SetInputObjectNodeId(GetNodeId());
						}
					}
				}

				if (CurrentDownstreamHC->GetCookOnCookableInputCook())
				{
					// Mark that HAC's input has changed
					CurrentDownstreamInput->MarkChanged(true);
				}
				bRemoveDownstream = false;
			}
		}

		if (bRemoveDownstream)
		{
			DownstreamToDelete.Add(CurrentDownstreamHC);
		}
	}

	for (auto ToDelete : DownstreamToDelete)
	{
		InputData->DownstreamCookables.Remove(ToDelete);
	}

	return true;
}

void
UHoudiniCookable::AddDownstreamCookable(UHoudiniCookable* InDownstreamCookable)
{
	if (!IsValid(InDownstreamCookable))
		return;

	if (!IsInputSupported())
		return;
		
	InputData->DownstreamCookables.Add(InDownstreamCookable);
}

void
UHoudiniCookable::RemoveDownstreamCookable(UHoudiniCookable* InDownstreamCookable)
{
	if (!IsInputSupported())
		return;

	InputData->DownstreamCookables.Remove(InDownstreamCookable);
}

void
UHoudiniCookable::ClearDownstreamCookable()
{
	InputData->DownstreamCookables.Empty();
}

void
UHoudiniCookable::PostLoad()
{
	Super::PostLoad();

	// Mark as need instantiation
	MarkAsNeedInstantiation();

	// Component has been loaded, not duplicated
	SetHasBeenDuplicated(false);

	// We need to register ourself
	FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(this);

#if WITH_EDITORONLY_DATA
	if (IsBakingSupported())
	{
		// TODO: Not necessary anymore?
		auto MaxValue = StaticEnum<EHoudiniEngineBakeOption>()->GetMaxEnumValue() - 1;
		if (static_cast<int>(BakingData->HoudiniEngineBakeOption) > MaxValue)
		{
			HOUDINI_LOG_WARNING(TEXT("Invalid Bake Type found, setting to To Actor. Possibly Foliage, which is deprecated, use the unreal_foliage attribute instead."));
			BakingData->HoudiniEngineBakeOption = EHoudiniEngineBakeOption::ToActor;
		}
	}
#endif
}

void
UHoudiniCookable::PostEditImport()
{
	Super::PostEditImport();

	MarkAsNeedInstantiation();

	// Component has been duplicated, not loaded
	// We do need the loaded flag to reapply parameters, inputs
	// and properly update some of the output objects
	SetHasBeenDuplicated(true);

	SetCurrentState(EHoudiniAssetState::PreInstantiation);
	SetCurrentStateResult(EHoudiniAssetStateResult::None);
}

void
UHoudiniCookable::BeginDestroy()
{
	OnDestroy(true);

	// Unregister ourself so our houdini node can be deleted
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(this);

	Super::BeginDestroy();
}

void UHoudiniCookable::SetNodeLabelPrefix(const FString& Prefix)
{
	NodeLabelPrefix = Prefix;
}

const FString& UHoudiniCookable::GetNodeLabelPrefix() const
{
	return NodeLabelPrefix;
}

