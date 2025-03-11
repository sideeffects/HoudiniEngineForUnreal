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
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDigitalAssetPCGElement::ExecuteInternal);

	//HOUDINI_LOG_MESSAGE(TEXT("ExecuteInternal"));

	check(Context);
	const UHoudiniDigitalAssetPCGSettings* Settings = Context->GetInputSettings<UHoudiniDigitalAssetPCGSettings>();

	// TODO: Add to managed resources
	UHoudiniPCGComponent* Component = UHoudiniPCGComponent::GetOrCreatePCGComponent(Context->SourceComponent.Get());

	if (Component->HoudiniAsset != Settings->HoudiniAsset)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Invalidate Cache due to change in Houdini Asset"));
		Component->CookableCache->Invalidate();
		Component->HoudiniAsset = Settings->HoudiniAsset;
	}

	if(!Settings || !Settings->HoudiniAsset.Get())
		return true;


	bool bChanged = false;

	FString CacheId = UHoudiniPCGCookableCache::GetCacheString(Context);
	UHoudiniPCGCookable* Cookable = Component->CookableCache->FindCookable(CacheId, Settings->HoudiniAsset, Component);
	if (Cookable == nullptr)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Creating cookable"));
		Cookable = Component->CookableCache->CreateCookable(CacheId, Settings->HoudiniAsset, Component);
		Cookable->Cook();
		Cookable->State = EPCGCookableState::Initializing;
		bChanged = true;
	}

	if(!Cookable)
	{
		return false;
	}

	auto Status = Cookable->State;

	switch(Status)
	{
	case EPCGCookableState::Initializing:
	{
		// Not done, so PCG can do another go-around.
		//HOUDINI_LOG_MESSAGE(TEXT("EPCGCookableState::Initializing"));
		return false;
	}
	case EPCGCookableState::Initialized:
	{
		//HOUDINI_LOG_MESSAGE(TEXT("EPCGCookableState::Initialized"));
		// Set inputs and cook.
		bool bInputsChanged = false;
		bInputsChanged |= Cookable->ApplyParametersToCookable(Context);
		bInputsChanged |= Cookable->ApplyInputsToCookable(Context);
		Cookable->Cook();
		return false;
		break;
	}
	case EPCGCookableState::Idle:
	{
		//HOUDINI_LOG_MESSAGE(TEXT("EPCGCookableState::Idle"));
		// If anything change, start a cook, or report done.
		bool bInputsChanged = false;
		bInputsChanged |= Cookable->ApplyParametersToCookable(Context);
		bInputsChanged |= Cookable->ApplyInputsToCookable(Context);
		bChanged |= bInputsChanged;
		if(bChanged || true)
		{
			Cookable->Cook();
			return false;
		}
		else
		{
			Cookable->State = EPCGCookableState::Done;
			return false;
		}

		break;
	}
	case EPCGCookableState::Cooking:
	{
		// Not done, so PCG can do another go-around.
		//HOUDINI_LOG_MESSAGE(TEXT("EPCGCookableState::Cooking"));
		return false;
	}
	case EPCGCookableState::Done:
	{
		//HOUDINI_LOG_MESSAGE(TEXT("EPCGCookableState::Done"));
		if(!Cookable->Cookable->GetOutputData())
			return true;

		FHoudiniPCGManagedResource ManagedResources;
		ManagedResources.Components = NewObject<UPCGManagedComponentList>(Context->SourceComponent.Get());
		ManagedResources.Actors = NewObject<UPCGManagedActors>(Context->SourceComponent.Get());

		switch(Settings->OutputType)
		{
		case EHoudiniPCGOutputType::Cook:
			for(int Index = 0; Index < Cookable->Cookable->GetOutputData()->Outputs.Num(); Index++)
			{

				UHoudiniPCGCookable::CreateOutputs(Context, &ManagedResources, Settings->GetOutputPinName(Index), Cookable->Cookable->GetOutputData()->Outputs[Index]);
			}
			break;

		case EHoudiniPCGOutputType::Bake:
			for(int Index = 0; Index < Cookable->Cookable->GetOutputData()->Outputs.Num(); Index++)
			{
				// TODO: Do actual baking, this is temp.
				UHoudiniPCGCookable::CreateOutputs(Context, &ManagedResources, Settings->GetOutputPinName(Index), Cookable->Cookable->GetOutputData()->Outputs[Index]);
			}
			break;
		default:
			break;
		}

		Cookable->State = EPCGCookableState::Idle;

		Context->SourceComponent->AddToManagedResources(ManagedResources.Components);
		Context->SourceComponent->AddToManagedResources(ManagedResources.Actors);

		break;

	}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
#endif