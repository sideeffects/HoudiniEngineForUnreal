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

#if defined(HOUDINI_USE_PCG)

#include "HoudiniPCGNode.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineManager.h"
#include "HoudiniPCGUtils.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "Async/PCGAsyncLoadingContext.h"
#include "PCGParamData.h"
#include "HAPI/HAPI_Common.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "UObject/FastReferenceCollector.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniInput.h"
#include "HoudiniPCGCookable.h"
#include "HoudiniPCGManagedResource.h"

#define LOCTEXT_NAMESPACE "UHoudiniDigitalAssetPCGSettings"


void UHoudiniDigitalAssetPCGSettings::PostLoad()
{
	Super::PostLoad();
}

void UHoudiniDigitalAssetPCGSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if(!HoudiniAsset)
	{
		// Dynamic tracking or null settings
		return;
	}

	FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(HoudiniAsset.GetPath());

	OutKeysToSettings.FindOrAdd(Key).Emplace(this, /*bCulling=*/false);
}

void UHoudiniDigitalAssetPCGSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}

FString UHoudiniDigitalAssetPCGSettings::GetAdditionalTitleInformation() const
{
	return FString::Printf(TEXT("%s"), HoudiniAsset ? *HoudiniAsset.GetFName().ToString() : TEXT("None"));
}

TArray<FPCGPinProperties> UHoudiniDigitalAssetPCGSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	for(int Index = 0; Index < Outputs.Num(); Index++)
	{
		PinProperties.Emplace(GetOutputPinName(Index), EPCGDataType::Param | EPCGDataType::Point, false);
	}
	return PinProperties;
}

FName UHoudiniDigitalAssetPCGSettings::GetOutputPinName(int Index) const
{
	return FName(*FString::Printf(TEXT("Output %d"), Index));
}

TArray<FPCGPinProperties> UHoudiniDigitalAssetPCGSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	for(int Index = 0; Index < Inputs.Num(); Index++)
	{
		FString PinName = FHoudiniPCGUtils::GetHDAInputName(Index);
		FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(FName(PinName), EPCGDataType::Any, /*bAllowMultipleConnections=*/false);
		InputPinProperty.SetNormalPin();
	}

	if (bExposeParameters)
	{
		FString PinName = FHoudiniPCGUtils::ParameterInputPinName;
		FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(FName(PinName), EPCGDataType::Any, /*bAllowMultipleConnections=*/false);
		InputPinProperty.SetNormalPin();
	}

	return PinProperties;
}


FPCGElementPtr UHoudiniDigitalAssetPCGSettings::CreateElement() const
{
	return MakeShared<FHoudiniDigitalAssetPCGElement>();
}

void UHoudiniDigitalAssetPCGSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if(PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniDigitalAssetPCGSettings, HoudiniAsset))
	{
		InstantiatePCGEditorHDA();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UHoudiniDigitalAssetPCGSettings::InstantiatePCGEditorHDA()
{
	if(HoudiniAsset == nullptr)
	{
		this->Inputs.SetNum(0);
		this->Outputs.SetNum(0);
		return;
	}

	ParameterCookable = NewObject<UHoudiniCookable>(GetTransientPackage());

	FHoudiniEngineManager* HEM = FHoudiniEngine::Get().GetHoudiniEngineManager();
	HEM->AutoStartFirstSessionIfNeeded();

	ParameterCookable->SetHoudiniAssetSupported(true);
	ParameterCookable->SetHoudiniAsset(this->HoudiniAsset);
	ParameterCookable->SetParameterSupported(true);
	ParameterCookable->SetInputSupported(true);
	ParameterCookable->SetOutputSupported(true);
	ParameterCookable->MarkAsNeedCook();

	do
	{
		HEM->ProcessCookable(ParameterCookable);
	} while(ParameterCookable->GetCurrentState() != EHoudiniAssetState::None);

	PopulateInputsAndOutputs();
}

void UHoudiniDigitalAssetPCGSettings::PopulateInputsAndOutputs()
{
	UCookableInputData* InputData = ParameterCookable->GetInputData();
	this->Inputs.SetNum(InputData ? ParameterCookable->GetInputData()->Inputs.Num() : 0);
	FProperty* Prop = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UHoudiniDigitalAssetPCGSettings, Inputs));
	if(Prop)
	{
		FPropertyChangedEvent PropertyChangedEvent(Prop);
		PostEditChangeProperty(PropertyChangedEvent);
	}

	UCookableOutputData* OutputData = ParameterCookable->GetOutputData();
	if (OutputData == nullptr)
	{
		this->Outputs.SetNum(0);
	}
	else
	{
		int NumOutputs = 1;
		NumOutputs += ParameterCookable->GetOutputData()->Outputs.Num();
			this->Outputs.SetNum(OutputData ? ParameterCookable->GetOutputData()->Outputs.Num() : 0);
		Prop = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UHoudiniDigitalAssetPCGSettings, Outputs));
		if(Prop)
		{
			FPropertyChangedEvent PropertyChangedEvent(Prop);
			PostEditChangeProperty(PropertyChangedEvent);
		}
	}
	int NumOutputs = 1;
	NumOutputs += OutputData ? OutputData->Outputs.Num() : 0;
	this->Outputs.SetNum(NumOutputs);
	Prop = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UHoudiniDigitalAssetPCGSettings, Outputs));
	if(Prop)
	{
		FPropertyChangedEvent PropertyChangedEvent(Prop);
		PostEditChangeProperty(PropertyChangedEvent);
	}

	this->Modify();
}

bool FHoudiniDigitalAssetPCGElement::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDigitalAssetAittributesElement::PrepareDataInternal);

	FPCHoudiniDigitalAssetAttributesContext* ThisContext = static_cast<FPCHoudiniDigitalAssetAttributesContext*>(Context);
	check(ThisContext);

	const UHoudiniDigitalAssetPCGSettings* Settings = Context->GetInputSettings<UHoudiniDigitalAssetPCGSettings>();
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
	// This is the main function for processing PCG nodes. It should return true when processing is complete, otherwise
	// false, which means it will be called again some time in the future (eg. a frame later).
	//
	// When called for the first time, this function creates a UHoudiniPCGManagedResource which is used to keep
	// track of a UHoudiniPCGComponent - there is one UHoudiniPCGComponent per execution of a PCG Node - note
	// that in a PCG loop, this is one per loop. The UHoudiniPCGComponent keeps track of the cookable (and its outputs).
	//
	
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDigitalAssetPCGElement::ExecuteInternal);
	check(Context);
	const UHoudiniDigitalAssetPCGSettings* Settings = Context->GetInputSettings<UHoudiniDigitalAssetPCGSettings>();

	if(!Settings || !Settings->HoudiniAsset.Get())
		return true;

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Set CRC.
	//----------------------------------------------------------------------------------------------------------------------------------------

	// Update CRC for Context. See comment in  FPCGCreateTargetActorElement::ExecuteInternal near code similar to this:
	if(!Context->DependenciesCrc.IsValid())
	{
		FPCGDataCollection EmptyCollection;
		GetDependenciesCrc(EmptyCollection, Settings, Context->SourceComponent.Get(), Context->DependenciesCrc);
	}

	// Calculate the Crc. We include the Stack as this gives us a unique CRC for each loop instance. We do include the inputs CRC
	// as this would force a new cookable everytime inputs change, and we don't want that for performance reasons. Instead, we check
	// if the inputs to the actual HDA changed below.

	FPCGCrc ResourceCrc = Context->DependenciesCrc;
	FPCGCrc StackCRC = Context->Stack->GetCrc();
	ResourceCrc.Combine(StackCRC);

	//----------------------------------------------------------------------------------------------------------------------------------------
	// See if we have an existing managed resource.
	//----------------------------------------------------------------------------------------------------------------------------------------

	UHoudiniPCGManagedResource* ManagedResource = nullptr;
	Context->SourceComponent->ForEachManagedResource([&ManagedResource, ResourceCrc, &Context](UPCGManagedResource* InResource)
		{
			if(!InResource->GetCrc().IsValid() || InResource->GetCrc() != ResourceCrc && InResource->IsA<UPCGManagedResource>())
				return;

			ManagedResource = Cast<UHoudiniPCGManagedResource>(InResource);
		});

	//----------------------------------------------------------------------------------------------------------------------------------------
	// If we didn't find a managed resource, we need to cook off a new one.
	//----------------------------------------------------------------------------------------------------------------------------------------

	if (ManagedResource)
	{
		if(!IsValid(ManagedResource->PCGComponent) || !IsValid(ManagedResource->PCGComponent->Cookable))
		{
			// user probably manually deleted components or killed a session. Discard previous managed resource.
			ManagedResource = nullptr;
		}
	}

	if(!ManagedResource)
	{
		// No previous resource found, so create a new one and instantiate the HDA. Note that next time Execute is called, this ManagedResource
		// will be found.

		ManagedResource = NewObject<UHoudiniPCGManagedResource>(Context->SourceComponent.Get());;
		ManagedResource->SetCrc(ResourceCrc);
		ManagedResource->MarkAsUsed();
		ManagedResource->PCGComponent = UHoudiniPCGComponent::CreatePCGComponent(Context->SourceComponent.Get());
		Context->SourceComponent->AddToManagedResources(ManagedResource);

		FHoudiniEngineManager* HEM = FHoudiniEngine::Get().GetHoudiniEngineManager();
		HEM->AutoStartFirstSessionIfNeeded();
		ManagedResource->PCGComponent->Cookable = NewObject<UHoudiniPCGCookable>(ManagedResource->PCGComponent);
		ManagedResource->PCGComponent->Cookable->Instantiate(Settings->HoudiniAsset, nullptr, ManagedResource->PCGComponent);
		ManagedResource->bExecuteInProgress = true;
		return false;
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	// If we found managed resource, and we haven't started cooking, see if we can reuse the result. If not, start a cook
	//----------------------------------------------------------------------------------------------------------------------------------------

	ManagedResource->MarkAsReused();

	if(!ManagedResource->bExecuteInProgress)
	{
		// If the resource is not being executed (ie. not cooking) then see if the inputs changed. If they did change
		// we must start a new cook. If not, we can reuse the existing results.

		bool bInputsChanged = false;
		bInputsChanged |= ManagedResource->PCGComponent->Cookable->ApplyParametersToCookable(Context);
		bInputsChanged |= ManagedResource->PCGComponent->Cookable->ApplyInputsToCookable(Context);

		if(bInputsChanged)
		{
			// Inputs changed so start a new cook and return since cooking will not be instant.
			ManagedResource->PCGComponent->Cookable->Cook();
			return false;
		}

		// Nothing changed so we can re-use output.
		return true;
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	// If we get here, we are waiting on Houdini. So perform state checks.
	//----------------------------------------------------------------------------------------------------------------------------------------

	UHoudiniPCGCookable* Cookable = ManagedResource->PCGComponent->Cookable.Get();

	switch(ManagedResource->PCGComponent->Cookable->State)
	{
	case EPCGCookableState::Initializing:
		// Still initializing, wait.
		return false;

	case EPCGCookableState::Initialized:
		// Initialized - so we set inputs and cook.
		Cookable->ApplyParametersToCookable(Context);
		Cookable->ApplyInputsToCookable(Context);
		Cookable->Cook();
		return false;

	case EPCGCookableState::Cooking:
		// Still cooking, wait.
		return false;

	case EPCGCookableState::Done:
		// Done - process results.
		ProcessCookableOutput(Context, Cookable);
		Cookable->State = EPCGCookableState::Idle;
		ManagedResource->bExecuteInProgress = false;
		return true;

	case EPCGCookableState::Idle:
		// Shouldn't get here since if cooking is complete this function should not be called.
		HOUDINI_LOG_ERROR(TEXT("Unexpected state: Idle. PCG Cooking failed."));
		return true;

	default:
		// Shouldn't get here.
		HOUDINI_LOG_ERROR(TEXT("Unexpected state: default. PCG Cooking failed."));
		return true;
	}
	
	return true;
}

void
FHoudiniDigitalAssetPCGElement::ProcessCookableOutput(FPCGContext* Context, UHoudiniPCGCookable* Cookable) const
{
	const UHoudiniDigitalAssetPCGSettings* Settings = Context->GetInputSettings<UHoudiniDigitalAssetPCGSettings>();

	if(!Cookable->Cookable->GetOutputData())
		return;

	switch(Settings->OutputType)
	{
	case EHoudiniPCGOutputType::Cook:
		for(int Index = 0; Index < Cookable->Cookable->GetOutputData()->Outputs.Num(); Index++)
		{

			UHoudiniPCGCookable::CreateOutputs(Context, Settings->GetOutputPinName(Index), Cookable->Cookable->GetOutputData()->Outputs[Index]);
		}
		break;

	case EHoudiniPCGOutputType::Bake:
		for(int Index = 0; Index < Cookable->Cookable->GetOutputData()->Outputs.Num(); Index++)
		{
			// TODO: Do actual baking, this is temp.
			UHoudiniPCGCookable::CreateOutputs(Context, Settings->GetOutputPinName(Index), Cookable->Cookable->GetOutputData()->Outputs[Index]);
		}
		break;
	default:
		break;
	}
}


bool
FHoudiniDigitalAssetPCGElement::IsCacheable(const UPCGSettings* InSettings) const
{
	return false;
}

#undef LOCTEXT_NAMESPACE
#endif