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

#define LOCTEXT_NAMESPACE "UHoudiniDigitalAssetPCGSettings"


void UHoudiniDigitalAssetPCGSettings::PostLoad()
{
	Super::PostLoad();

	if(HoudiniAsset)
		InitializationState = EHoudiniPCGInitState::Done;
}

void UHoudiniDigitalAssetPCGSettings::BeginDestroy()
{
	Super::BeginDestroy();
	InitializationState = EHoudiniPCGInitState::Abort;
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
	switch(InitializationState)
	{
	case EHoudiniPCGInitState::Initializing:
		return TEXT("Initializing... please wait...");

	case EHoudiniPCGInitState::Done:
		return FString::Printf(TEXT("%s"), HoudiniAsset ? *HoudiniAsset.GetFName().ToString() : TEXT("None"));

	case EHoudiniPCGInitState::Error:
		return TEXT("* Error initializing *");
	default:
		return TEXT("Please set HDA");
	}

}

TArray<FPCGPinProperties> UHoudiniDigitalAssetPCGSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	for(int Index = 0; Index < Outputs.Num(); Index++)
	{
		PinProperties.Emplace(GetOutputPinName(Index), EPCGDataType::Param | EPCGDataType::Point | EPCGDataType::Spline, false);
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

	if (bExposeParameters && InitializationState == EHoudiniPCGInitState::Done)
	{
		FString PinName = FHoudiniPCGUtils::ParameterInputPinName;
		FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(FName(PinName), EPCGDataType::Any, /*bAllowMultipleConnections=*/false);
		InputPinProperty.SetNormalPin();
		InputPinProperty.bAllowMultipleData = true;
		InputPinProperty.SetAllowMultipleConnections(true);
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

		if(HoudiniAsset == nullptr)
			InitializationState = EHoudiniPCGInitState::None;
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

	ParameterCookable = nullptr;

	InitializationState = EHoudiniPCGInitState::Initializing;

	Async(EAsyncExecution::ThreadPool, [this]()
	{

		auto Result = FHoudiniPCGUtils::StartSession();
		if(Result == EHoudiniPCGSessionStatus::PCGSessionStatus_Error)
		{
			HOUDINI_LOG_ERROR(TEXT("Could not start Houdini Session"));
			InitializationState = EHoudiniPCGInitState::Error;
			return;
		}

		ParameterCookable = NewObject<UHoudiniCookable>(this);

		ParameterCookable->SetHoudiniAssetSupported(true);
		ParameterCookable->SetHoudiniAsset(this->HoudiniAsset);
		ParameterCookable->SetParameterSupported(true);
		ParameterCookable->SetInputSupported(true);
		ParameterCookable->SetOutputSupported(true);
		ParameterCookable->MarkAsNeedCook();
		ParameterCookable->SetSlateNotifications(false);

		do
		{
			if (InitializationState == EHoudiniPCGInitState::Abort)
				return;

			FHoudiniEngineManager* HEM = FHoudiniEngine::Get().GetHoudiniEngineManager();
			HEM->ProcessCookable(ParameterCookable);
			FPlatformProcess::Sleep(0.1f);

		} while(ParameterCookable->GetCurrentState() != EHoudiniAssetState::None);

		InitializationState = EHoudiniPCGInitState::Done;

		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			// Populating must be done on game thread.
			this->PopulateInputsAndOutputs();
		});

	});

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
	this->Outputs.SetNum(NumOutputs);
	Prop = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UHoudiniDigitalAssetPCGSettings, Outputs));
	if(Prop)
	{
		FPropertyChangedEvent PropertyChangedEvent(Prop);
		PostEditChangeProperty(PropertyChangedEvent);
	}

	this->Modify();
}

FPCGCrc FHoudiniDigitalAssetPCGElement::SetCrc(FPCGContext* Context) const
{
	const UHoudiniDigitalAssetPCGSettings* Settings = Context->GetInputSettings<UHoudiniDigitalAssetPCGSettings>();

	// Calculate the Crc. We include the Stack as this gives us a unique CRC for each loop instance. We do include the inputs CRC
	// as this would force a new cookable everytime inputs change, and we don't want that for performance reasons. Instead, we check
	// if the inputs to the actual HDA changed below.

	// Update CRC for Context. See comment in  FPCGCreateTargetActorElement::ExecuteInternal near code similar to this:
	if(!Context->DependenciesCrc.IsValid())
	{
		FPCGDataCollection EmptyCollection;
		GetDependenciesCrc(EmptyCollection, Settings, Context->SourceComponent.Get(), Context->DependenciesCrc);
	}

	FPCGCrc ResourceCrc = Context->DependenciesCrc;
	FPCGCrc StackCRC = Context->Stack->GetCrc();
	ResourceCrc.Combine(StackCRC);
	return ResourceCrc;
}

void FHoudiniDigitalAssetPCGElement::AbortInternal(FPCGContext* Context) const
{
	FPCGCrc ResourceCrc = SetCrc(Context);

	Context->SourceComponent->ForEachManagedResource([ResourceCrc, &Context](UPCGManagedResource* InResource)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDigitalAssetPCGElement::ExecuteInternal);

	EHoudiniPCGSessionStatus Status = FHoudiniPCGUtils::StartSessionAsync();
	if (Status == EHoudiniPCGSessionStatus::PCGSessionStatus_Error)
	{
		FHoudiniPCGUtils::LogVisualError(Context, TEXT("Could not create Houdini Session"));
		return true;
	}
	else if(Status == EHoudiniPCGSessionStatus::PCGSessionStatus_Creating)
		return false;

	// This is the main function for processing PCG nodes. It should return true when processing is complete, otherwise
	// false, which means it will be called again some time in the future (eg. a frame later).
	//
	// When called for the first time, this function creates a UHoudiniPCGManagedResource which is used to keep
	// track of a UHoudiniPCGComponent - there is one UHoudiniPCGComponent per execution of a PCG Node - note
	// that in a PCG loop, this is one per loop. The UHoudiniPCGComponent keeps track of the cookable (and its outputs).
	//
	// Possibly the bFirstTimeExecuted isn't needed, and we could move some of this into PrepareDataInternal() or
	// PreExecute(), but Epic's PCG nodes seem to put the logic here, so I'll keep it here for now. Some of the logic
	// is easier this way too.

	FPCHoudiniDigitalAssetAttributesContext* HDAContext = static_cast<FPCHoudiniDigitalAssetAttributesContext*>(Context);
	check(HDAContext);

	const UHoudiniDigitalAssetPCGSettings* Settings = Context->GetInputSettings<UHoudiniDigitalAssetPCGSettings>();

	if(!Settings || !Settings->HoudiniAsset.Get())
	{
		HOUDINI_PCG_MESSAGE(TEXT("Settings or Settings->HoudiniAsset is null, not cooking."));
		return true;
	}


	FPCGCrc ResourceCrc = SetCrc(Context);

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

	if (HDAContext->bFirstTimeExecuted)
	{
		HOUDINI_PCG_MESSAGE(TEXT("First time called with context %p"), HDAContext);

		HDAContext->bFirstTimeExecuted = false;

		// If the Managed Resource is invalid, don't use it.

		if(ManagedResource)
		{
			if(	!IsValid(ManagedResource->HoudiniPCGComponent) || 
				!IsValid(ManagedResource->HoudiniPCGComponent->Cookable) ||
				ManagedResource->bInvalidateResource)
			{
				HOUDINI_PCG_MESSAGE(TEXT("(%p) Invalid Managed Resource Found, ignoring."), ManagedResource);
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

			ManagedResource = NewObject<UHoudiniPCGManagedResource>(Context->SourceComponent.Get());
			ManagedResource->PCGComponent = Context->SourceComponent.Get();
			if(ManagedResource->PCGComponent)
			{
				ManagedResource->PCGComponent->GetGraph()->OnGraphChangedDelegate.AddUObject(ManagedResource, &UHoudiniPCGManagedResource::OnGraphChanged);
			}
			ManagedResource->SetCrc(ResourceCrc);
			ManagedResource->MarkAsUsed();
			ManagedResource->HoudiniPCGComponent = UHoudiniPCGComponent::CreatePCGComponent(Context->SourceComponent.Get());
			Context->SourceComponent->AddToManagedResources(ManagedResource);

			FHoudiniPCGUtils::StartSessionAsync();
			ManagedResource->HoudiniPCGComponent->Cookable = NewObject<UHoudiniPCGCookable>(ManagedResource->HoudiniPCGComponent);
			ManagedResource->HoudiniPCGComponent->Cookable->Instantiate(Settings->HoudiniAsset, nullptr, ManagedResource->HoudiniPCGComponent);
			HOUDINI_PCG_MESSAGE(TEXT("(%p) Creating Managed Resource, Instantiating..."), ManagedResource);

			// Return now since instantiation is not instant.
			return false;
		}

		//----------------------------------------------------------------------------------------------------------------------------------------
		// We have a managed resource... update the cookable, and if that triggered a cook, we're done. If not, we can just re-use the last
		// cook.
		//----------------------------------------------------------------------------------------------------------------------------------------


		// Attempt to apply parameters, inputs. If a cook was started, return - we need to wait for it to complete asynchronouosly.
		bool bError = false;
		bool bCookStarted = ManagedResource->HoudiniPCGComponent->Cookable->UpdateAndCook(Context, bError);

		if (bError)
		{
			HOUDINI_PCG_MESSAGE(TEXT("An error occured, not processing PCG node."));
			return true;
		}

		ManagedResource->MarkAsReused();

		if (bCookStarted)
		{
			// Something changes, so cook is in progress
			HOUDINI_PCG_MESSAGE(TEXT("A cook was started."));
			return false;
		}
		else
		{
			// Nothing changed so we can re-use output as-is.
			HOUDINI_PCG_MESSAGE(TEXT("Nothing Changed: returning Managed Resource."));
			return true;
		}
	}
	else
	{
		//----------------------------------------------------------------------------------------------------------------------------------------
		// Not the first time we've been called with this Context, so wait for the Cookable to cook.
		//----------------------------------------------------------------------------------------------------------------------------------------

		if (!IsValid(ManagedResource->HoudiniPCGComponent))
		{
			// User delete component mid-cook?
			HOUDINI_PCG_MESSAGE(TEXT("Houdini PCG Component lost..."));
			return true;
		}
		UHoudiniPCGCookable* Cookable = ManagedResource->HoudiniPCGComponent->Cookable.Get();
		bool bError = false;
		bool bDone = Cookable->Update(Context, bError);

		if (bError)
		{
			HOUDINI_PCG_MESSAGE(TEXT("An error occured waiting for cook to complete, not processing PCG node."));
			return true;
		}
		return bDone;
	}
}

bool
FHoudiniDigitalAssetPCGElement::IsCacheable(const UPCGSettings* InSettings) const
{
	return false;
}

#undef LOCTEXT_NAMESPACE
#endif