/*
* Copyright (c) <2025> Side Effects Software Inc.
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

#include "PropertyPathHelpers.h"
#include "Async/Async.h"
#if defined(HOUDINI_USE_PCG)

#include "HoudiniPCGNode.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineManager.h"
#include "HoudiniPCGUtils.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "Async/PCGAsyncLoadingContext.h"
#include "PCGParamData.h"
#include "PCGGraph.h"
#include "HAPI/HAPI_Common.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "UObject/FastReferenceCollector.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniInput.h"
#include "HoudiniPCGCookable.h"
#include "HoudiniPCGManagedResource.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "UHoudiniPCGSettings"


void UHoudiniPCGSettings::SetupCookable()
{
	if(IsValid(ParameterCookable))
	{
		ParameterCookable->OnPostOutputProcessingDelegate.AddLambda([this](UHoudiniPCGCookable* Cookable, bool  bSuccess)
			{
				OnParameterCookableCooked();
			});
		ParameterCookable->OnInitializedDelegate.AddLambda([this](UHoudiniPCGCookable* Cookable, bool  bSuccess)
			{
				OnParameterCookableInitialized();
			});

		SetNodeLabelPrefix();
	}
}

void UHoudiniPCGSettings::OnParameterCookableInitialized()
{
	ForceRefreshUI();
}


void UHoudiniPCGSettings::PostLoad()
{
	Super::PostLoad();
	SetupCookable();
}

void UHoudiniPCGSettings::PostEditImport()
{
	SetupCookable();
}

void UHoudiniPCGSettings::BeginDestroy()
{
	Super::BeginDestroy();
}

void UHoudiniPCGSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if(!HoudiniAsset)
	{
		// Dynamic tracking or null settings
		return;
	}

	FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(HoudiniAsset.GetPath());

	OutKeysToSettings.FindOrAdd(Key).Emplace(this, /*bCulling=*/false);
}

void UHoudiniPCGSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}

FString UHoudiniPCGSettings::GetAdditionalTitleInformation() const
{
	if (!IsValid(ParameterCookable) || !IsValid(ParameterCookable->Cookable))
	{
		return TEXT("No HDA set.");
	}

	auto State = ParameterCookable->State;

	switch(State)
	{
	case EPCGCookableState::WaitingForSession:
		return TEXT("Establishing Houdini Session...");

	case EPCGCookableState::Initializing:
		return TEXT("Initializing...");

	case EPCGCookableState::Cooking:
		return TEXT("Initializing... please wait...");

	case EPCGCookableState::None:
	case EPCGCookableState::CookingComplete:
	case EPCGCookableState::Loaded:
		return FString::Printf(TEXT("%s"), HoudiniAsset ? *HoudiniAsset.GetFName().ToString() : TEXT("None"));

	default:
		return TEXT("* Error initializing *");
	}
}

TArray<FPCGPinProperties> UHoudiniPCGSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(GetOutputPinName(), EPCGDataType::Param | EPCGDataType::Point | EPCGDataType::Spline, false);
	return PinProperties;
}

FName UHoudiniPCGSettings::GetOutputPinName() const
{
	return FName(*FString::Printf(TEXT("Outputs")));
}

TArray<FPCGPinProperties> UHoudiniPCGSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	{
		FString PinName = FHoudiniPCGUtils::ParameterInputPinName;
		FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(FName(PinName), EPCGDataType::Any, /*bAllowMultipleConnections=*/false);
		InputPinProperty.SetNormalPin();
		InputPinProperty.bAllowMultipleData = true;
		InputPinProperty.SetAllowMultipleConnections(true);
	}

	for(int Index = 0; Index < NumInputs; Index++)
	{
		FString PinName = FHoudiniPCGUtils::GetHDAInputName(Index);
		FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(FName(PinName), EPCGDataType::Any, /*bAllowMultipleConnections=*/false);
		InputPinProperty.SetNormalPin();
	}

	return PinProperties;
}


FPCGElementPtr UHoudiniPCGSettings::CreateElement() const
{
	return MakeShared<FHoudiniDigitalAssetPCGElement>();
}

void UHoudiniPCGSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if(PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniPCGSettings, HoudiniAsset))
	{
		if(HoudiniAsset == nullptr)
		{
			if(IsValid(ParameterCookable))
			{
				ParameterCookable = nullptr;
			}
		}

		InstantiateNewParameterCookable();

		FHoudiniEngineRuntimeUtils::ForceDetailsPanelToUpdate();
	}
}

void RefreshDetailsForObject(UObject* TargetObject)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyEditorModule.UpdatePropertyViews({ TargetObject });
}

void UHoudiniPCGSettings::OnParameterCookableCooked()
{
	ForceRefreshUI();
}

void UHoudiniPCGSettings::ForceRefreshUI()
{
	// Very brute force way to refresh the details panel.

	this->Modify();
	this->MarkPackageDirty();
	this->IterationCount++;

	FProperty* Property = FindFProperty<FProperty>(this->GetClass(), GET_MEMBER_NAME_CHECKED(UHoudiniPCGSettings, IterationCount));
	if(Property)
	{
		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		this->PostEditChangeProperty(PropertyChangedEvent);
	}

	FHoudiniEngineRuntimeUtils::ForceDetailsPanelToUpdate();

}

void UHoudiniPCGSettings::InstantiateParameterCookable()
{
	Async(EAsyncExecution::ThreadPool, [this]()
		{
			ParameterCookable->Instantiate();

			do
			{
				// Cook has been cancelled
				if(ParameterCookable == nullptr)
					break;

				if(ParameterCookable->State == EPCGCookableState::Initialized)
					break;

				auto PrevState = ParameterCookable->State;

				ParameterCookable->Update(nullptr);

				if(ParameterCookable->State != PrevState)
				{
					// For UI update.
					this->Modify();
				}
				FPlatformProcess::Sleep(0.1f);

			} while(true);

			if(ParameterCookable)
			{
				// The parameter cookable will not be recooked once its initialized, so set to CookingComplete.
				ParameterCookable->State = EPCGCookableState::CookingComplete;
				AsyncTask(ENamedThreads::GameThread, [this]()
					{
						// Populating must be done on game thread.
						this->PopulateInputsAndOutputs();
					});
			}
		});
}

void UHoudiniPCGSettings::InstantiateNewParameterCookable()
{
	if(HoudiniAsset == nullptr)
	{
		this->Modify();

		// Do not auto reset inputs and outputs. Its annoying when changing HDA for this to break.
		//this->Inputs.SetNum(0);
		//this->Outputs.SetNum(0);

		ParameterCookable = nullptr;
		this->MarkPackageDirty();
		RefreshDetailsForObject(this);
		return;
	}

	ParameterCookable = nullptr;

	ParameterCookable = NewObject<UHoudiniPCGCookable>(this);

	ParameterCookable->CreateHoudiniCookable(HoudiniAsset, nullptr, nullptr);
	ParameterCookable->Cookable->SetOutputSupported(false);
	ParameterCookable->Cookable->SetPDGSupported(true);
	ParameterCookable->Cookable->SetIsPCG(true);
	ParameterCookable->Cookable->GetParameterData()->bCookOnParameterChange = true;

	SetupCookable();

	InstantiateParameterCookable();

}

void UHoudiniPCGSettings::SetNodeLabelPrefix()
{
	if(!IsValid(ParameterCookable))
		return;

	FString NodeLabel;

	// Use Graph name if available (should be!)
	if(this->GetOuter()->GetOuter())
	{
		NodeLabel += this->GetOuter()->GetOuter()->GetName();
		NodeLabel += TEXT("_");
	}

	// Use node name if available (should be!)
	if(this->GetOuter())
	{
		NodeLabel += this->GetOuter()->GetName();
		NodeLabel += TEXT("_");
	}

	ParameterCookable->Cookable->SetNodeLabelPrefix(NodeLabel);
}

void UHoudiniPCGSettings::ResetFromHDA()
{
	this->Modify();

	if(ParameterCookable)
		ParameterCookable->Rebuild();

	this->MarkPackageDirty();
}




void UHoudiniPCGSettings::PopulateInputsAndOutputs()
{
	this->Modify();

	FProperty* Prop = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UHoudiniPCGSettings, IterationCount));
	if(Prop)
	{
		FPropertyChangedEvent PropertyChangedEvent(Prop);
		PostEditChangeProperty(PropertyChangedEvent);
	}

	this->MarkPackageDirty();
}


FPCGCrc FHoudiniDigitalAssetPCGElement::SetCrc(FPCGContext* Context) const
{
	const UHoudiniPCGSettings* Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	// Calculate the Crc. We include the Stack as this gives us a unique CRC for each loop instance. We do include the inputs CRC
	// as this would force a new cookable everytime inputs change, and we don't want that for performance reasons. Instead, we check
	// if the inputs to the actual HDA changed below.

	// Update CRC for Context. See comment in  FPCGCreateTargetActorElement::ExecuteInternal near code similar to this:
	if(!Context->DependenciesCrc.IsValid())
	{
		FPCGDataCollection EmptyCollection;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		GetDependenciesCrc(FPCGGetDependenciesCrcParams(&EmptyCollection, Settings, Context->ExecutionSource.Get()), Context->DependenciesCrc);
#else
		GetDependenciesCrc(EmptyCollection, Settings, FHoudiniPCGUtils::GetSourceComponent(Context), Context->DependenciesCrc);
#endif

	}

	FPCGCrc ResourceCrc = Context->DependenciesCrc;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	FPCGCrc StackCRC = Context->GetStack()->GetCrc();
#else
	FPCGCrc StackCRC = Context->Stack->GetCrc();
#endif
	ResourceCrc.Combine(StackCRC);
	return ResourceCrc;
}

void FHoudiniDigitalAssetPCGElement::AbortInternal(FPCGContext* Context) const
{
	FPCGCrc ResourceCrc = SetCrc(Context);

	UPCGComponent* SourceComponent = FHoudiniPCGUtils::GetSourceComponent(Context);

	SourceComponent->ForEachManagedResource([ResourceCrc, &Context](UPCGManagedResource* InResource)
		{
			if(!InResource->GetCrc().IsValid() || InResource->GetCrc() != ResourceCrc && InResource->IsA<UPCGManagedResource>())
				return;

			UHoudiniPCGManagedResource* ManagedResource = Cast<UHoudiniPCGManagedResource>(InResource);
			if (ManagedResource)
			{
				// Mark any managed resource as a "bInvalidateResource". The next time the node tried to execute (if the CRC is the same)
				// the flag will be noted and the resource discarded.
				ManagedResource->bInvalidateResource = true;
			}

		});
}

bool FHoudiniDigitalAssetPCGElement::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDigitalAssetAittributesElement::PrepareDataInternal);

	FPCHoudiniDigitalAssetAttributesContext* ThisContext = static_cast<FPCHoudiniDigitalAssetAttributesContext*>(Context);
	check(ThisContext);

	const UHoudiniPCGSettings* Settings = Context->GetInputSettings<UHoudiniPCGSettings>();
	if(!Settings || !Settings->HoudiniAsset)
		return true;

	if(!ThisContext->WasLoadRequested())
	{
		ThisContext->RequestResourceLoad(ThisContext, { Settings->HoudiniAsset.GetPath() }, !Settings->bSynchronousLoad);
	}

	return true;
}

bool FHoudiniDigitalAssetPCGElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDigitalAssetPCGElement::ExecuteInternal);

	FPCHoudiniDigitalAssetAttributesContext* HDAContext = static_cast<FPCHoudiniDigitalAssetAttributesContext*>(Context);
	check(HDAContext);

	// This is the main function for processing PCG nodes. It should return true when processing is complete, otherwise
	// false, which means it will be called again some time in the future (eg. a frame later).
	//
	// When called for the first time, this function creates a UHoudiniPCGManagedResource which is used to keep
	// track of a UHoudiniPCGComponent - there is one UHoudiniPCGComponent per execution of a PCG Node - note
	// that in a PCG loop, this is one per loop. The UHoudiniPCGComponent keeps track of the cookable (and its outputs).

	const UHoudiniPCGSettings* Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	if(!Settings || !Settings->HoudiniAsset.Get())
	{
		HOUDINI_PCG_MESSAGE(TEXT("Settings or Settings->HoudiniAsset is null, not cooking."));
		return true;
	}

	if (!IsValid(Settings->ParameterCookable))
	{
		HOUDINI_PCG_MESSAGE(TEXT("No Parameter Cookable found. Internal error."));
		return true;
	}
	FPCGCrc ResourceCrc = SetCrc(Context);

	//----------------------------------------------------------------------------------------------------------------------------------------
	// See if we have an existing managed resource.
	//----------------------------------------------------------------------------------------------------------------------------------------

	UHoudiniPCGManagedResource* ManagedResource = nullptr;

	UPCGComponent * PCGComponent = FHoudiniPCGUtils::GetSourceComponent(Context);

	PCGComponent->ForEachManagedResource([&ManagedResource, ResourceCrc, &Context](UPCGManagedResource* InResource)
		{
			if(!InResource->GetCrc().IsValid() || InResource->GetCrc() != ResourceCrc && InResource->IsA<UPCGManagedResource>())
				return;

			ManagedResource = Cast<UHoudiniPCGManagedResource>(InResource);
		});

	switch(HDAContext->ContextState)
	{
	case EHoudiniPCGContextState::None:
	{
		HOUDINI_PCG_MESSAGE(TEXT("First time called with context %p"), HDAContext);

		// For now, we must always force the Cookable to be created since the user may have edited the Parameter Cookable
		// without saving the HDA, meaning the HDAs in session sync get out of sync.
		ManagedResource = nullptr;

		// If the Managed Resource is invalid, don't use it.

		if(ManagedResource)
		{
			if(!IsValid(ManagedResource->HoudiniPCGComponent) ||
				!IsValid(ManagedResource->HoudiniPCGComponent->Cookable) ||
				ManagedResource->bInvalidateResource)
			{
				if (ManagedResource->HoudiniPCGComponent)
				{
					HOUDINI_PCG_MESSAGE(TEXT("(%p) Invalid Managed Resource Found, ignoring."), ManagedResource->HoudiniPCGComponent->Cookable.Get());
					ManagedResource->DestroyCookable();
				}
				ManagedResource = nullptr;
			}
		}

		//----------------------------------------------------------------------------------------------------------------------------------------
		// If we didn't find a managed resource (or we ignored the old one), we need create a new one and start a cook.
		//----------------------------------------------------------------------------------------------------------------------------------------

		if(!ManagedResource)
		{
			// No previous resource found, so create a new one and instantiate the HDA. Note that next time Execute is called, this ManagedResource
			// will be found.
			// NOTE: We instantiate, then once the HDA is ready in Houdini, we set parameters and cooked. This seems to be necessary to avoid
			// paramters getting overridden on the first cook. Possibly a slight rework of Houdini Engine Manager could fix this.

			UPCGComponent* SourceComponent = FHoudiniPCGUtils::GetSourceComponent(Context);

			ManagedResource = NewObject<UHoudiniPCGManagedResource>(SourceComponent);
			ManagedResource->PCGComponent = SourceComponent;
			if(ManagedResource->PCGComponent)
			{
				ManagedResource->PCGComponent->GetGraph()->OnGraphChangedDelegate.AddUObject(ManagedResource, &UHoudiniPCGManagedResource::OnGraphChanged);
			}
			ManagedResource->SetCrc(ResourceCrc);
			ManagedResource->MarkAsUsed();
			ManagedResource->HoudiniPCGComponent = UHoudiniPCGComponent::CreatePCGComponent(SourceComponent);
			SourceComponent->AddToManagedResources(ManagedResource);

			UHoudiniPCGCookable * PCGCookable = NewObject<UHoudiniPCGCookable>(ManagedResource->HoudiniPCGComponent);
			PCGCookable->CreateHoudiniCookable(Settings->HoudiniAsset, nullptr, ManagedResource->HoudiniPCGComponent);
			PCGCookable->Cookable->SetIsPCG(true);
			PCGCookable->Cookable->SetLandscapeModificationEnabled(ManagedResource->PCGComponent->bIgnoreLandscapeTracking);
			PCGCookable->Cookable->SetNodeLabelPrefix(TEXT("PCG_Instance_"));
			PCGCookable->Instantiate();
			PCGCookable->bAutomaticallyDeleteAssets = Settings->bAutomaticallyDeleteTempAssets;
			ManagedResource->HoudiniPCGComponent->Cookable = PCGCookable;
			HOUDINI_PCG_MESSAGE(TEXT("(%p) Creating Managed Resource, Instantiating..."), PCGCookable);

			// Return now since instantiation is not instant.
			HDAContext->ContextState = EHoudiniPCGContextState::Instantiating;
			return false;
		}
		else
		{
			// We have a managed resource... update the cookable, and if that triggered a cook, we're done. If not, we can just re-use the last cook.
			// Attempt to apply parameters, inputs. If a cook was started, return - we need to wait for it to complete asynchronouosly.

			ManagedResource->HoudiniPCGComponent->Cookable->CopyParametersAndInputs(Settings->ParameterCookable);
			bool bSuccess = ManagedResource->HoudiniPCGComponent->Cookable->UpdateParametersAndInputs(Context);
			if(!bSuccess)
			{
				HOUDINI_PCG_MESSAGE(TEXT("An error occured, not processing PCG node."));
				return true;
			}

			ManagedResource->MarkAsReused();

			if(ManagedResource->HoudiniPCGComponent->Cookable->NeedsCook())
			{
				// Remove previous baked output before cooking. (Cooked output is already cleaned up).
				UPCGComponent* SourceComponent = FHoudiniPCGUtils::GetSourceComponent(Context);
				ManagedResource->HoudiniPCGComponent->Cookable->DeleteBakedOutput(SourceComponent->GetWorld());

				// Something changed, so we must cook.
				ManagedResource->HoudiniPCGComponent->Cookable->StartCook();
				HOUDINI_PCG_MESSAGE(TEXT("A cook was started."));
				HDAContext->ContextState = EHoudiniPCGContextState::Cooking;
				return false;
			}
			else
			{
				// Nothing changed so we can re-use output as-is.
				HOUDINI_PCG_MESSAGE(TEXT("Nothing Changed: returning Managed Resource."));
				HDAContext->ContextState = EHoudiniPCGContextState::Done;
				return true;
			}
		}
	}
	case EHoudiniPCGContextState::Instantiating:
	{
		UHoudiniPCGCookable* Cookable = ManagedResource->HoudiniPCGComponent->Cookable.Get();
		Cookable->Update(HDAContext);

		if(Cookable->State == EPCGCookableState::Initialized)
		{
			Cookable->Cookable->SetOutputSupported(true);
			Cookable->CopyParametersAndInputs(Settings->ParameterCookable);
			Cookable->UpdateParametersAndInputs(Context);

			if(!Cookable->GetErrors().IsEmpty())
			{
				HDAContext->ContextState = EHoudiniPCGContextState::Done;
				FHoudiniPCGUtils::LogVisualError(Context, Cookable->GetErrors());
				return true;
			}

			if (Cookable->NeedsCook())
			{
				HDAContext->ContextState = EHoudiniPCGContextState::Cooking;
				Cookable->StartCook();
				return false;
			}
			else
			{
				HDAContext->ContextState = EHoudiniPCGContextState::Done;
				return true;
			}
		}
		return false;
	}
	break;
	case EHoudiniPCGContextState::Cooking:
	{
		// Wait for cooking to complete.
		if(!IsValid(ManagedResource->HoudiniPCGComponent))
		{
			// User delete component mid-cook?
			HOUDINI_PCG_MESSAGE(TEXT("Houdini PCG Component lost..."));
			return true;
		}
		UHoudiniPCGCookable* Cookable = ManagedResource->HoudiniPCGComponent->Cookable.Get();
		Cookable->Update(Context);

		if (Cookable->State == EPCGCookableState::CookingComplete)
		{
			if(!Cookable->GetErrors().IsEmpty())
			{
				HDAContext->ContextState = EHoudiniPCGContextState::Done;
				FHoudiniPCGUtils::LogVisualError(Context, Cookable->GetErrors());
				return true;
			}

			if (Settings->OutputType == EHoudiniPCGOutputType::Cook)
			{
				Cookable->ProcessCookedOutput(Context);
			}
			else
			{
				Cookable->Bake();
				Cookable->ProcessBakedOutput(Context);
			}
			return true;
		}
		else
		{
			return false;
		}
	}
	break;
	default:
		break;
	}
	return true;
}

bool
FHoudiniDigitalAssetPCGElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UHoudiniPCGSettings* Settings = Cast<const UHoudiniPCGSettings>(InSettings);
	return Settings->bUsePCGCache;
}

#undef LOCTEXT_NAMESPACE
#endif