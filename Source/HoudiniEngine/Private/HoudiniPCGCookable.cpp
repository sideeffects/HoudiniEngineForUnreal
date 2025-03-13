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
#include "HoudiniPCGNode.h"

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

#include "HoudiniPCGCookable.h"
#include "HoudiniParameter.h"
#include <HoudiniParameterString.h>
#include <HoudiniParameterInt.h>
#include <HoudiniParameterFloat.h>

#include "HoudiniEngine.h"
#include "HoudiniEngineManager.h"
#include "HoudiniPCGTranslator.h"
#include "Misc/StringBuilder.h"
#include "HoudiniPCGManagedResource.h"

#define LOCTEXT_NAMESPACE "PCGCachedCookable"

UHoudiniPCGCookable::UHoudiniPCGCookable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniPCGCookable::~UHoudiniPCGCookable()
{
}

void UHoudiniPCGCookable::Cook()
{
	// Ensure outputs are enabled. They may be disabled during instantiation.
	Cookable->SetOutputSupported(true);

	State = EPCGCookableState::Cooking;
	FHoudiniEngineManager* HEM = FHoudiniEngine::Get().GetHoudiniEngineManager();
	HEM->AutoStartFirstSessionIfNeeded();

	Cookable->MarkAsNeedCook();
}

void UHoudiniPCGCookable::Instantiate(UHoudiniAsset* Asset, UHoudiniDigitalAssetPCGSettings* Owner, UHoudiniPCGComponent* Component)
{
	Cookable = NewObject<UHoudiniCookable>();
	State = EPCGCookableState::Initializing;
	auto OutputDelegateHandle = Cookable->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniCookable* _HC, bool  bSuccess)
		{
			if (this->State == EPCGCookableState::Initializing)
				this->State = EPCGCookableState::Initialized;
			else if(this->State == EPCGCookableState::Cooking)
				this->State = EPCGCookableState::Done;
		});

	Cookable->SetParameterSupported(true);
	Cookable->SetInputSupported(true);
	Cookable->SetOutputSupported(false); // Don't produce outputs in Unreal during instantiate.
	Cookable->SetComponentSupported(Component ? true : false);
	if (Component)
	{
		Cookable->SetComponent(Component);
	}

	Cookable->SetHoudiniAssetSupported(true);
	UCookableHoudiniAssetData* HAD = Cookable->GetHoudiniAssetData();
	HAD->HoudiniAsset = Asset;

	FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(Cookable.Get());
}

UHoudiniPCGCookable* FindCacheEntry(FString& CacheId);

void UHoudiniPCGCookable::InvalidateCookable()
{
	if (IsValid(this->Cookable.Get()))
	{
		FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(this->Cookable.Get());
		this->Cookable = nullptr; // Garbage Collection will clean this up.
	}
}

bool UHoudiniPCGCookable::ApplyParametersToCookable(FPCGContext* Context)
{
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(FName(FHoudiniPCGUtils::ParameterInputPinName));

	bool bChanged = false;
	for(auto& TaggedData : Inputs)
	{
		bChanged |= ApplyParametersToCookable(TaggedData.Data, Context);
	}
	return bChanged;
}


bool UHoudiniPCGCookable::ApplyInputsToCookable(FPCGContext* Context)
{
	int NumInputs = this->Cookable->GetNumInputs();

	bool bInputsChanged = false;

	for(int Index = 0; Index < NumInputs; Index++)
	{
		FString InputName = FHoudiniPCGUtils::GetHDAInputName(Index);

		const TArray<FPCGTaggedData>& ContextInputData = Context->InputData.GetInputsByPin(FName(InputName));

		for(const FPCGTaggedData& InputData : ContextInputData)
		{
			auto InputType = FHoudiniPCGUtils::GetInputType(InputData.Data);
			const UPCGMetadata* Metadata = InputData.Data->ConstMetadata();

			UHoudiniInput* Input = this->Cookable->GetInputAt(Index);
			switch(InputType)
			{
			case EHoudiniPCGInputType::UnrealObjects:
				bInputsChanged |= ApplyInputAsUnrealObjects(Input, Metadata);
				break;
			case EHoudiniPCGInputType::PCGData:
				bInputsChanged |= ApplyInputAsPCGData(Input, InputData.Data);
				break;
			case EHoudiniPCGInputType::None:
				HOUDINI_LOG_ERROR(TEXT("Unknown input type."));
				break;
			}
		}
	}
	return bInputsChanged;
}

bool UHoudiniPCGCookable::ApplyParametersToCookable(const UPCGData* Data, FPCGContext* Context)
{
	const UPCGMetadata* Metadata = Data->ConstMetadata();

	const FName ParameterNamesId = TEXT("hda_parameter_name");
	const FName ParameterValuesId = TEXT("hda_parameter_value");

	TMap<FString, int> ParameterMap;

	FHoudiniPCGAttributes Attributes(Metadata, ParameterValuesId);

	if(const FPCGMetadataAttribute<FString>* ParameterNames = static_cast<const FPCGMetadataAttribute<FString>*>(Metadata->GetConstAttribute(ParameterNamesId)))
	{
		// Create a map of parameter names to parameters, to avoid an O(n^2) lookup

		for(int Index = 0; Index < Metadata->GetItemCountForChild(); Index++)
		{
			FString ParameterName = ParameterNames->GetValueFromItemKey(Index);
			ParameterMap.Add(ParameterName, Index);
		}
	}

	bool bChanged = false;

	// For each parameter in the HDA we need to set its values. Since parameters may have been removed from PCG input we can't just change the values, we need
	// to consider reverting to defaults. We do this by getting the defaults, over-writing them with any user specified values, and then writing them back
	// only if changed.

	for(auto& Parameter : this->Cookable->GetParameterData()->Parameters)
	{
		const FString& ParameterName = Parameter->GetParameterName();

		if(UHoudiniParameterFloat* ParameterFloat = Cast<UHoudiniParameterFloat>(Parameter))
		{
			TArray<float> Values = ParameterFloat->GetDefaultValues();
			if(int* Index = ParameterMap.Find(ParameterName))
				FHoudiniPCGUtils::GetValueAsFloat(Values[0], *Index, Attributes);

			bChanged |= ParameterFloat->SetValuesIfChanged(Values);
		}
		else if(UHoudiniParameterString* ParameterString = Cast<UHoudiniParameterString>(Parameter))
		{
			TArray<FString> Values = ParameterString->GetDefaultValues();
			if(int* Index = ParameterMap.Find(ParameterName))
				FHoudiniPCGUtils::GetValueAsString(Values[0], *Index, Attributes);

			bChanged |= ParameterString->SetValuesIfChanged(Values);
		}
		else if(UHoudiniParameterInt* ParameterInt = Cast<UHoudiniParameterInt>(Parameter))
		{
			TArray<int> Values = ParameterInt->GetDefaultValues();
			if(int* Index = ParameterMap.Find(ParameterName))
				FHoudiniPCGUtils::GetValueAsInt(Values[0], *Index, Attributes);

			bChanged |= ParameterInt->SetValuesIfChanged(Values);
		}
	}
	return bChanged;
}

void UHoudiniPCGCookable::Release()
{
	FHoudiniOutputTranslator::ClearAndRemoveOutputs(this->Cookable->GetOutputs());

}

void UHoudiniPCGCookable::CreateOutputs(FPCGContext* Context, const FName& OutputPinName, const UHoudiniOutput* HoudiniOutput)
{
	if(FHoudiniPCGUtils::HasPCGOutputs(HoudiniOutput))
	{
		CreateOutputsAsPCGData(Context, OutputPinName, HoudiniOutput);
	}
	else
	{
		CreateOutputsAsObjectReferences(Context, OutputPinName, HoudiniOutput);
	}
}

void UHoudiniPCGCookable::CreateOutputsAsPCGData(FPCGContext* Context, const FName& OutputPinName, const UHoudiniOutput* HoudiniOutput)
{
	TArray<FPCGTaggedData>& TaggedDataArray = Context->OutputData.TaggedData;

	for(auto& It : HoudiniOutput->GetOutputObjects())
	{
		auto& Object = It.Value;

		if(UHoudiniPCGOutputData* PCGOutputData = Cast<UHoudiniPCGOutputData>(Object.OutputObject))
		{
			if(PCGOutputData->PointParams)
			{
				FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
				TaggedOutput.Data = PCGOutputData->PointParams;
				TaggedOutput.Pin = OutputPinName;
				TaggedOutput.Tags.Add(TEXT("Points"));
			}

			if(PCGOutputData->VertexParams)
			{
				FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
				TaggedOutput.Data = PCGOutputData->VertexParams;
				TaggedOutput.Pin = OutputPinName;
				TaggedOutput.Tags.Add(TEXT("Vertices"));
			}

			if(PCGOutputData->PrimsParams)
			{
				FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
				TaggedOutput.Data = PCGOutputData->PrimsParams;
				TaggedOutput.Pin = OutputPinName;
				TaggedOutput.Tags.Add(TEXT("Prims"));
			}

			if(PCGOutputData->DetailsParams)
			{
				FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
				TaggedOutput.Data = PCGOutputData->DetailsParams;
				TaggedOutput.Pin = OutputPinName;
				TaggedOutput.Tags.Add(TEXT("Details"));
			}
		}
	}
}

void UHoudiniPCGCookable::CreateOutputsAsObjectReferences(FPCGContext* Context, const FName& OutputPinName, const UHoudiniOutput* HoudiniOutput)
{
	TArray<FHoudiniPCGObjectOutput> Outputs = FHoudiniPCGUtils::GetPCGOutputData(HoudiniOutput);

	UPCGParamData* ParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* Metadata = ParamData->MutableMetadata();

	constexpr bool bAllowsInterpolation = false;
	constexpr bool bOverrideParent = false;

	const FName PCGOutputIndexName = FName(TEXT("OutputIndex"));
	const FName PCGOutputComponentName = FName(TEXT("Component"));
	const FName PCGOutputActorName = FName(TEXT("Actor"));
	const FName PCGOutputObjectName = FName(TEXT("Object"));

	Metadata->CreateInteger32Attribute(PCGOutputIndexName, 0, bAllowsInterpolation, bOverrideParent);
	Metadata->CreateSoftObjectPathAttribute(PCGOutputComponentName, FString(), bAllowsInterpolation, bOverrideParent);
	Metadata->CreateSoftObjectPathAttribute(PCGOutputActorName, FString(), bAllowsInterpolation, bOverrideParent);
	Metadata->CreateSoftObjectPathAttribute(PCGOutputObjectName, FString(), bAllowsInterpolation, bOverrideParent);

	for(int Row = 0; Row < Outputs.Num(); Row++)
	{
		const PCGMetadataEntryKey EntryKey = ParamData->Metadata->AddEntry();
		const auto& Output = Outputs[Row];

		FPCGMetadataAttribute<int32>* IntAttr = Metadata->GetMutableTypedAttribute<int32>(PCGOutputIndexName);
		IntAttr->SetValue(Row, Output.OutputIndex);

		FPCGMetadataAttribute<FSoftObjectPath>* StrAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputComponentName);
		StrAttr->SetValue(Row, Output.ComponentPath);

		StrAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputActorName);
		StrAttr->SetValue(Row, Output.ActorPath);

		StrAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputObjectName);
		StrAttr->SetValue(Row, Output.ObjectPath);

	}

	TArray<FPCGTaggedData>& TaggedDataArray = Context->OutputData.TaggedData;
	FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
	TaggedOutput.Data = ParamData;
	TaggedOutput.Pin = OutputPinName;
	TaggedOutput.Tags.Add(OutputPinName.ToString());
}
#undef LOCTEXT_NAMESPACE
