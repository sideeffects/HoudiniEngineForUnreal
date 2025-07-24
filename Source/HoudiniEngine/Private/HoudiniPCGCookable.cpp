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
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "UObject/FastReferenceCollector.h"
#include "HoudiniEngineRuntime.h"

#include "HoudiniPCGCookable.h"
#include "HoudiniParameter.h"
#include <HoudiniParameterString.h>
#include <HoudiniParameterInt.h>
#include <HoudiniParameterFloat.h>

#include "HoudiniEngineManager.h"
#include "HoudiniPCGTranslator.h"
#include "Misc/StringBuilder.h"
#include "HoudiniPCGManagedResource.h"
#include "HoudiniOutputTranslator.h"
#include <HoudiniParameterToggle.h>
#include <HoudiniEngineUtils.h>
#include "HoudiniPCGDataObject.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniFoliageTools.h"
#include "Materials/Material.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "Landscape.h"

#define LOCTEXT_NAMESPACE "PCGCachedCookable"


UHoudiniPCGCookable::UHoudiniPCGCookable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniPCGCookable::~UHoudiniPCGCookable()
{
}

void
UHoudiniPCGCookable::OnCookingComplete(bool bSuccess)
{
	// Ignore this callback when processing PDG.
	if (bIsCookingPDG)
		return;

	OnCookingCompleteInternal(bSuccess);
}

void
UHoudiniPCGCookable::Rebuild()
{
	if(!IsValid(this->Cookable))
		return;

	this->State = EPCGCookableState::Initializing;
	this->Cookable->MarkAsNeedRebuild();
}

void
UHoudiniPCGCookable::OnCookingCompleteInternal(bool bSuccess)
{
	switch (this->State)
	{
	case EPCGCookableState::Initializing:
		HOUDINI_PCG_MESSAGE(TEXT("(%p)       Set to EPCGCookableState::Initialized"), this);
		this->State = EPCGCookableState::Initialized;
		if(OnInitializedDelegate.IsBound())
			OnInitializedDelegate.Broadcast(this, bSuccess);

		break;

	default:
		HOUDINI_PCG_MESSAGE(TEXT("(%p)       Set to EPCGCookableState::CookingComplete"), this);
		this->State = EPCGCookableState::CookingComplete;

		if(!bSuccess && !bIsCookingPDG)
			AddCookError(TEXT("Houdini cook returned errors."));

		if(OnPostOutputProcessingDelegate.IsBound())
			OnPostOutputProcessingDelegate.Broadcast(this, bSuccess);
		break;
	}

}

void
UHoudiniPCGCookable::PostLoad()
{
	Super::PostLoad();

	State = EPCGCookableState::Loaded;

	if (this->Cookable)
	{
		auto OutputDelegateHandle = Cookable->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniCookable* _HC, bool  bSuccess)
			{
				this->OnCookingComplete(bSuccess);
			});
	}
}

void
UHoudiniPCGCookable::PostEditImport()
{
	Super::PostEditImport();

	State = EPCGCookableState::Loaded;

	if(this->Cookable)
	{
		auto OutputDelegateHandle = Cookable->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniCookable* _HC, bool  bSuccess)
			{
				this->OnCookingComplete(bSuccess);
			});
	}
}


void
UHoudiniPCGCookable::CreateHoudiniCookable(UHoudiniAsset* Asset, UHoudiniPCGSettings* Owner, UHoudiniPCGComponent* Component)
{
	HOUDINI_PCG_MESSAGE(TEXT("(%p) UHoudiniPCGCookable::CreateHoudiniCookable"), this);

	TrackedObjects.Empty();

	Cookable = NewObject<UHoudiniCookable>(this, NAME_None, RF_Public);
	State = EPCGCookableState::Initializing;
	auto OutputDelegateHandle = Cookable->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniCookable* _HC, bool  bSuccess)
		{
			this->OnCookingComplete(bSuccess);
		});

	Cookable->SetDoSlateNotifications(false);
	Cookable->SetAllowUpdateEditorProperties(false);
	Cookable->SetParameterSupported(true);
	Cookable->SetInputSupported(true);
	Cookable->SetComponentSupported(Component ? true : false);
	Cookable->SetEnableProxyStaticMeshOverride(false);
	Cookable->SetOverrideGlobalProxyStaticMeshSettings(true);
	Cookable->SetAutoCook(false);
	Cookable->GetParameterData()->bCookOnParameterChange = false;
	Cookable->GetInputData()->bCookOnInputChange = false;
	Cookable->SetPDGSupported(true);
	Cookable->SetBakingSupported(true);
	Cookable->SetProxySupported(true);

	if(Component)
	{
		Cookable->SetComponent(Component);
	}

	Cookable->SetHoudiniAssetSupported(true);
	UCookableHoudiniAssetData* HAD = Cookable->GetHoudiniAssetData();
	HAD->HoudiniAsset = Asset;
	this->State = EPCGCookableState::None;
}

void
UHoudiniPCGCookable::Instantiate()
{
	HOUDINI_PCG_MESSAGE(TEXT("(%p) UHoudiniPCGCookable::Instantiate"), this);
	this->State = EPCGCookableState::WaitingForSession;
	FHoudiniPCGUtils::StartSessionAsync();
}

void
UHoudiniPCGCookable::InvalidateCookable()
{
	EHoudiniClearFlags ClearFlags = EHoudiniClearFlags::EHoudiniClear_Actors;

	if(bAutomaticallyDeleteAssets)
	{
		ClearFlags |= EHoudiniClearFlags::EHoudiniClear_Assets | EHoudiniClearFlags::EHoudiniClear_LandscapeLayers;
	}

	FHoudiniOutputTranslator::ClearAndRemoveOutputs(this->Cookable->GetOutputs(), ClearFlags);

	if(UHoudiniPDGAssetLink * PDGAssetLink = Cookable->GetPDGAssetLink())
	{
		auto*  TOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();
		if(IsValid(TOPNetwork))
		{
			PDGAssetLink->ClearTOPNetworkWorkItemResults(TOPNetwork);
		}
		TOPNetwork = nullptr;
	}

	if(!IsValid(this->Cookable.Get()))
		return;

	this->Cookable->OnDestroy(true);
	this->Cookable = nullptr; 
}

bool
UHoudiniPCGCookable::ApplyParametersToCookable(const FPCGContext* Context)
{
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(FName(FHoudiniPCGUtils::ParameterInputPinName));

	bool bChanged = false;
	for(auto& TaggedData : Inputs)
	{
		bChanged |= ApplyParametersToCookable(TaggedData.Data);
	}

	if (bChanged)
	{
		// Changing the parameters will start a cook.
		State = EPCGCookableState::Cooking;
	}
	return bChanged;
}


bool
UHoudiniPCGCookable::ApplyInputsToCookable(const FPCGContext* Context)
{
	int NumInputs = this->Cookable->GetNumInputs();

	for(int Index = 0; Index < NumInputs; Index++)
	{
		UHoudiniInput* Input = this->Cookable->GetInputAt(Index);

		FString InputName = FHoudiniPCGUtils::GetHDAInputName(Index);

		const TArray<FPCGTaggedData>& ContextInputData = Context->InputData.GetInputsByPin(FName(InputName));

		if(ContextInputData.IsEmpty())
			continue;

		// See if the input contains objects, going through each tagged data.

		TArray<FString> UnrealObjectPaths;
		for(const FPCGTaggedData& InputData : ContextInputData)
		{
			const UPCGMetadata* Metadata = InputData.Data->ConstMetadata();
			TArray<FString> UnrealObjects = GetUnrealObjectPaths(Context, Metadata);
			if(!UnrealObjects.IsEmpty())
			{
				UnrealObjectPaths.Append(UnrealObjects);
			}
		}

		switch (Input->GetInputType())
		{
		case EHoudiniInputType::PCGInput:
			{
				if(ContextInputData.Num())
				{
					UHoudiniPCGDataCollection* DataCollection = NewObject<UHoudiniPCGDataCollection>(this);

					for(const FPCGTaggedData& InputData : ContextInputData)
					{
						UHoudiniPCGDataObject* PCGDataObject = GetPCGDataObjects(InputData);
						if(!PCGDataObject)
							continue;

						DataCollection->AddObject(PCGDataObject);
					}

					if(DataCollection)
						bInputsChanged |= ApplyInputAsPCGData(Input, { DataCollection });
					else
						bInputsChanged |= ApplyInputAsPCGData(Input, { });
				}
			}
			break;
		case EHoudiniInputType::Geometry:
			{
				// Looks like we have Unreal objects, so set those on the current input.
				bInputsChanged |= ApplyInputAsUnrealObjects(Context, Input, UnrealObjectPaths);
			}
			break;
		case EHoudiniInputType::World:
			{
				// Looks like we have Unreal objects, so set those on the current input.
				bInputsChanged |= ApplyInputAsUnrealObjects(Context, Input, UnrealObjectPaths);
			}
			break;
		default:
			break;

		}
	}

	return bInputsChanged;
}

void
UHoudiniPCGCookable::AddTrackedObjects(const FPCGContext* Context)
{
	FPCGDynamicTrackingHelper DynamicTracking;
	DynamicTracking.EnableAndInitialize(Context, TrackedObjects.Num());
	for(auto& TrackedObject : TrackedObjects)
	{
		DynamicTracking.AddToTracking(FPCGSelectionKey::CreateFromPath(TrackedObject), false);
	}
	DynamicTracking.Finalize(Context);
	TrackedObjects.Empty();
}

bool
UHoudiniPCGCookable::ApplyParametersToCookable(const UPCGData* Data)
{
	const UPCGMetadata* Metadata = Data->ConstMetadata();

	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;

	Metadata->GetAttributes(AttributeNames, AttributeTypes);
	TSet<FString> AttributeSet;
	for (FName & AttrName : AttributeNames)
	{
		AttributeSet.Add(AttrName.ToString());
	}

	bool bChanged = false;

	for (auto & Parameter : this->Cookable->GetParameterData()->Parameters)
	{
		FString ParameterName = Parameter->GetParameterName();
		if (!AttributeSet.Contains(ParameterName))
			continue;

		FHoudiniPCGAttributes Attributes(Metadata, FName(ParameterName));
		if(UHoudiniParameterString* ParameterString = Cast<UHoudiniParameterString>(Parameter))
		{
			TArray<FString> Values = FHoudiniPCGUtils::GetValueAsString(ParameterString->GetDefaultValues(), Attributes, 0);
			bChanged |= ParameterString->SetValuesIfChanged(Values);
		}
		else if(UHoudiniParameterFloat* ParameterFloat = Cast<UHoudiniParameterFloat>(Parameter))
		{
			TArray<float> Values = FHoudiniPCGUtils::GetValueAsFloat(ParameterFloat->GetDefaultValues(), Attributes, 0);
			bChanged |= ParameterFloat->SetValuesIfChanged(Values);
		}
		else if(UHoudiniParameterInt* ParameterInt = Cast<UHoudiniParameterInt>(Parameter))
		{
			TArray<int> Values = FHoudiniPCGUtils::GetValueAsInt(ParameterInt->GetDefaultValues(), Attributes, 0);
			bChanged |= ParameterInt->SetValuesIfChanged(Values);
		}
		else if(UHoudiniParameterToggle* ParameterToggle = Cast<UHoudiniParameterToggle>(Parameter))
		{
			TArray<int> Values = FHoudiniPCGUtils::GetValueAsInt(ParameterToggle->GetDefaultValues(), Attributes, 0);
			bChanged |= ParameterToggle->SetValuesIfChanged(Values);
		}
	}

	return bChanged;
}

void
UHoudiniPCGCookable::DestroyCookable(UWorld * World)
{
	HOUDINI_PCG_MESSAGE(TEXT("UHoudiniPCGCookable::DestroyCookable (%p)"), this);

	DeleteBakedOutput(World);
	InvalidateCookable();
}

void
UHoudiniPCGCookable::ProcessBakedOutputs(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const FHoudiniBakedOutput* HoudiniOutput)
{
	if(FHoudiniPCGUtils::HasPCGOutputs(HoudiniOutput))
	{
		CopyBakedPCGOutputDataToPinData(Context, OutputPinName, TagName, HoudiniOutput);
	}
	else
	{
		CreateOutputPinFromBakedData(Context, OutputPinName, TagName, HoudiniOutput);
	}
}

void
UHoudiniPCGCookable::ProcessCookedOutput(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput)
{
	if(FHoudiniPCGUtils::HasPCGOutputs(HoudiniOutput))
	{
		CopyCookedPCGOutputDataToPinData(Context, OutputPinName, TagName, HoudiniOutput);
	}
	else
	{
		CreateOutputPinFromCookedData(Context, OutputPinName, TagName, HoudiniOutput);
	}
}

void
UHoudiniPCGCookable::CopyCookedPCGOutputDataToPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput)
{
	for(auto& It : HoudiniOutput->GetOutputObjects())
	{
		auto& Object = It.Value;

		if(UHoudiniPCGOutputData* PCGOutputData = Cast<UHoudiniPCGOutputData>(Object.OutputObject))
		{
			CopyPCGOutputDataToPinData(Context, OutputPinName, TagName, PCGOutputData);
		}
	}
}

void
UHoudiniPCGCookable::CopyBakedPCGOutputDataToPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const FHoudiniBakedOutput* HoudiniOutput)
{
	for(auto& It : HoudiniOutput->BakedOutputObjects)
	{
		auto& Object = It.Value;

		if(UHoudiniPCGOutputData* PCGOutputData = Cast<UHoudiniPCGOutputData>(Object.PCGOutputData))
		{
			CopyPCGOutputDataToPinData(Context, OutputPinName, TagName, PCGOutputData);
		}
	}
}

void
UHoudiniPCGCookable::CopyPCGOutputDataToPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniPCGOutputData* PCGOutputData)
{
	TArray<FPCGTaggedData>& TaggedDataArray = Context->OutputData.TaggedData;

	if(PCGOutputData->PointParams)
	{
		FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
		TaggedOutput.Data = PCGOutputData->PointParams;
		TaggedOutput.Pin = OutputPinName;
		TaggedOutput.Tags.Add(TEXT("Points"));
		TaggedOutput.Tags.Add(TagName);
	}

	if(PCGOutputData->VertexParams)
	{
		FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
		TaggedOutput.Data = PCGOutputData->VertexParams;
		TaggedOutput.Pin = OutputPinName;
		TaggedOutput.Tags.Add(TEXT("Vertices"));
		TaggedOutput.Tags.Add(TagName);
	}

	if(PCGOutputData->PrimsParams)
	{
		FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
		TaggedOutput.Data = PCGOutputData->PrimsParams;
		TaggedOutput.Pin = OutputPinName;
		TaggedOutput.Tags.Add(TEXT("Primitives"));
		TaggedOutput.Tags.Add(TagName);
	}

	if(PCGOutputData->DetailsParams)
	{
		FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
		TaggedOutput.Data = PCGOutputData->DetailsParams;
		TaggedOutput.Pin = OutputPinName;
		TaggedOutput.Tags.Add(TEXT("Details"));
		TaggedOutput.Tags.Add(TagName);
	}

	if(!PCGOutputData->SplineParams.IsEmpty())
	{
		for (auto Spline : PCGOutputData->SplineParams)
		{
			FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
			TaggedOutput.Data = Spline;
			TaggedOutput.Pin = OutputPinName;
			TaggedOutput.Tags.Add(TEXT("Spline"));
		}
	}
}


void
UHoudiniPCGCookable::CreateOutputPinFromBakedData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const FHoudiniBakedOutput* HoudiniOutput)
{
	TArray<FHoudiniPCGObjectOutput> Outputs = FHoudiniPCGUtils::GetPCGOutputData(HoudiniOutput);

	CreateOutputPinData(Context, OutputPinName, TagName, Outputs);
}

void
UHoudiniPCGCookable::CreateOutputPinFromCookedData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput)
{
	TArray<FHoudiniPCGObjectOutput> Outputs = FHoudiniPCGUtils::GetPCGOutputData(HoudiniOutput);

	CreateOutputPinData(Context, OutputPinName, TagName, Outputs);
}

void
UHoudiniPCGCookable::CreateOutputPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const TArray<FHoudiniPCGObjectOutput> & Outputs)
{
	UPCGParamData* ParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* Metadata = ParamData->MutableMetadata();

	constexpr bool bAllowsInterpolation = false;
	constexpr bool bOverrideParent = false;

	const FName PCGOutputIndexName = FName(TEXT("OutputObjectIndex"));
	const FName PCGOutputTypeName = FName(TEXT("Type"));
	const FName PCGOutputComponentName = FName(TEXT("Component"));
	const FName PCGOutputActorName = FName(TEXT("Actor"));
	const FName PCGOutputObjectName = FName(TEXT("Object"));

	Metadata->CreateInteger32Attribute(PCGOutputIndexName, 0, bAllowsInterpolation, bOverrideParent);
	Metadata->CreateStringAttribute(PCGOutputTypeName, FString(), bAllowsInterpolation, bOverrideParent);
	Metadata->CreateSoftObjectPathAttribute(PCGOutputComponentName, FString(), bAllowsInterpolation, bOverrideParent);
	Metadata->CreateSoftObjectPathAttribute(PCGOutputActorName, FString(), bAllowsInterpolation, bOverrideParent);
	Metadata->CreateSoftObjectPathAttribute(PCGOutputObjectName, FString(), bAllowsInterpolation, bOverrideParent);

	for(int Row = 0; Row < Outputs.Num(); Row++)
	{
		const PCGMetadataEntryKey EntryKey = ParamData->Metadata->AddEntry();
		const auto& Output = Outputs[Row];

		FPCGMetadataAttribute<int32>* IntAttr = Metadata->GetMutableTypedAttribute<int32>(PCGOutputIndexName);
		IntAttr->SetValue(Row, Output.OutputObjectIndex);

		FPCGMetadataAttribute<FString>* StrAttr = Metadata->GetMutableTypedAttribute<FString>(PCGOutputTypeName);
		StrAttr->SetValue(Row, Output.OutputType);

		FPCGMetadataAttribute<FSoftObjectPath>* PathAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputComponentName);
		PathAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputComponentName);
		PathAttr->SetValue(Row, Output.ComponentPath);

		PathAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputActorName);
		PathAttr->SetValue(Row, Output.ActorPath);

		PathAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGOutputObjectName);
		PathAttr->SetValue(Row, Output.ObjectPath);

	}

	TArray<FPCGTaggedData>& TaggedDataArray = Context->OutputData.TaggedData;
	FPCGTaggedData& TaggedOutput = TaggedDataArray.Emplace_GetRef();
	TaggedOutput.Data = ParamData;
	TaggedOutput.Pin = OutputPinName;
	TaggedOutput.Tags.Add(TagName);
}

void
UHoudiniPCGCookable::ProcessCookedOutput(FPCGContext* Context)
{
	const UHoudiniPCGSettings* Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	AddTrackedObjects(Context);

	UCookableOutputData* OutputData = this->Cookable->GetOutputData();
	if(!OutputData)
		return;


	if (UHoudiniPDGAssetLink* PDGAssetLink = this->Cookable->GetPDGAssetLink())
	{
		int Index = 0;
		UTOPNetwork* Network = PDGAssetLink->GetSelectedTOPNetwork();
		for(UTOPNode* Node : Network->AllTOPNodes)
		{
			for (FTOPWorkResult& WorkResult : Node->WorkResult)
			{
				for (FTOPWorkResultObject & ResultObject :  WorkResult.ResultObjects)
				{
					TArray<TObjectPtr<UHoudiniOutput>>& Outputs = ResultObject.GetResultOutputs();
					for (auto & Ptr : Outputs)
					{
						FString Tag = FString::Printf(TEXT("Output-%d"), Index++);
						ProcessCookedOutput(Context, Settings->GetOutputPinName(), Tag, Ptr);
					}
				}
			}
		}
	}
	else
	{
		auto& Outputs = OutputData->Outputs;
		for(int Index = 0; Index < Outputs.Num(); Index++)
		{
			FString Tag = FString::Printf(TEXT("Output-%d"), Index);
			ProcessCookedOutput(Context, Settings->GetOutputPinName(), Tag, Outputs[Index]);
		}
	}
}

void
UHoudiniPCGCookable::ProcessBakedOutput(FPCGContext* Context)
{
	const UHoudiniPCGSettings* Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	if(UHoudiniPDGAssetLink* PDGAssetLink = this->Cookable->GetPDGAssetLink())
	{
		auto& Outputs = this->PDGBakedOutput->BakedOutputs;
		for(int Index = 0; Index < Outputs.Num(); Index++)
		{
			FString Tag = FString::Printf(TEXT("Output-%d"), Index);
			ProcessBakedOutputs(Context, Settings->GetOutputPinName(), Tag, &Outputs[Index]);
		}
	}
	else
	{
		UCookableBakingData* BakingData = this->Cookable->GetBakingData();
		if(!BakingData)
			return;

		auto& Outputs = BakingData->BakedOutputs;
		for(int Index = 0; Index < Outputs.Num(); Index++)
		{
			FString Tag = FString::Printf(TEXT("Output-%d"), Index);
			ProcessBakedOutputs(Context, Settings->GetOutputPinName(), Tag, &Outputs[Index]);
		}
	}

	AddTrackedObjects(Context);
}

void
UHoudiniPCGCookable::CopyParametersAndInputs(const UHoudiniPCGCookable * Other)
{
	bParamsChanged |= Cookable->SetParameterData(Other->Cookable->GetParameterData());
	bInputsChanged |= Cookable->SetInputData(Other->Cookable->GetInputData());

	UHoudiniPDGAssetLink* ThisPDGAssetLink =  Cookable->GetPDGAssetLink();
	UHoudiniPDGAssetLink* OtherPDGAssetLink = Other->Cookable->GetPDGAssetLink();
	if (OtherPDGAssetLink)
	{
		ThisPDGAssetLink->SelectedTOPNetworkIndex = OtherPDGAssetLink->SelectedTOPNetworkIndex;
	}

}

bool
UHoudiniPCGCookable::UpdateParametersAndInputs(FPCGContext* Context)
{
	Cookable->SetOutputSupported(true);

	const UHoudiniPCGSettings* Settings = nullptr;
	if(Context)
		Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	Cookable->GetOutputData()->bCreateSceneComponents = Settings ? Settings->bCreateSceneComponents : false;

	if(Context)
	{
		bParamsChanged |= this->ApplyParametersToCookable(Context);
		bInputsChanged |= this->ApplyInputsToCookable(Context);
	}

	return true;
}

bool
UHoudiniPCGCookable::NeedsCook() const
{
	bool bHasBeenCooked = State == EPCGCookableState::CookingComplete;

	return (bInputsChanged || bParamsChanged || !bHasBeenCooked);
}


void
UHoudiniPCGCookable::StartCook()
{
	ensure(NeedsCook());

	bInputsChanged = false;
	bParamsChanged = false;

	Errors.Empty();

	State = EPCGCookableState::Cooking;

	UHoudiniPDGAssetLink* PDGAssetLink = Cookable->GetPDGAssetLink();
	bIsCookingPDG = IsValid(PDGAssetLink);

	if (bIsCookingPDG)
	{
		HOUDINI_PCG_MESSAGE(TEXT("(%p) Starting to Cook with PDG."), this);

		HOUDINI_LOG_MESSAGE(TEXT("################>>> Cookable %p AssetLink %p"), this, PDGAssetLink);
		UWorld* World= this->GetWorld();
		PDGAssetLink->SetOutputWorld(World);
		PDGAssetLink->GetSelectedTOPNetwork();
		auto * TOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();

		TOPNetwork->GetOnPostCookDelegate().AddLambda([this](UTOPNetwork* Link, bool bSuccess)
		{
			if (State == EPCGCookableState::Cooking)
			{
				OnCookingCompleteInternal(bSuccess);
			}
		});
		if(IsValid(TOPNetwork))
		{
			FHoudiniPDGManager::DirtyAll(TOPNetwork);
			FHoudiniPDGManager::CookOutput(TOPNetwork);
		}
	}
	else
	{
		// Non-PDG
		HOUDINI_PCG_MESSAGE(TEXT("(%p) Starting to Cook."), this);
		Cookable->MarkAsNeedCook();
	}
}

void
UHoudiniPCGCookable::Bake()
{
	const bool bInRemoveHACOutputOnSuccess = false;

	if (Cookable->IsPDGSupported() && Cookable->GetPDGAssetLink())
	{
		FHoudiniEngineBakeUtils::BakePDGAssetLink(Cookable->GetPDGAssetLink());

		this->PDGBakedOutput = NewObject<UHoudiniPDGBakeOutput>(this);

		auto AssetLink = Cookable->GetPDGAssetLink();
		UTOPNetwork* TopNetwork = AssetLink->GetSelectedTOPNetwork();
		for (auto Node : TopNetwork->AllTOPNodes)
		{
			for (auto & It : Node->GetBakedWorkResultObjectsOutputs())
			{
				FHoudiniPDGWorkResultObjectBakedOutput & Result = It.Value;
				this->PDGBakedOutput->BakedOutputs.Append(Result.BakedOutputs);
			}
		}
	}
	else
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeCookable(Cookable,
			BakeSettings,
			EHoudiniEngineBakeOption::ToActor,
			bInRemoveHACOutputOnSuccess);
	}
}
void
UHoudiniPCGCookable::Update(FPCGContext* Context)
{
	// This is called every tick during a PCG Cook. It updates internal state based off async operations.
	// The user can cancel the PCG task if this takes too long, so there is no additional bailout mechanism.

	switch(this->State)
	{
	case EPCGCookableState::WaitingForSession:
		if(FHoudiniPCGUtils::SessionStatus == EHoudiniPCGSessionStatus::PCGSessionStatus_Created)
		{
			// A session already existed or was created. Now we can register the cookable with the
			// runtime. This will trigger a cook.
			this->State = EPCGCookableState::Initializing;
			FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(Cookable.Get());
		}
		else if(FHoudiniPCGUtils::SessionStatus == EHoudiniPCGSessionStatus::PCGSessionStatus_Error)
		{
			this->State = EPCGCookableState::Initialized;
		}
		break;

	case EPCGCookableState::Initializing:
		// Still initializing, wait. 
		break;

	case EPCGCookableState::Initialized:
		break;

	case EPCGCookableState::Cooking:
		// Still cooking
		break;

	case EPCGCookableState::CookingComplete:
		// CookingComplete - process results.
		HOUDINI_PCG_MESSAGE(TEXT("DONE cooking Managed Resource (%p)"), this);
		CookCount = FHoudiniEngineUtils::HapiGetCookCount(Cookable->GetNodeId());
		break;

	default:
		// Shouldn't get here.
		HOUDINI_LOG_ERROR(TEXT("Unexpected state: default. PCG Cooking failed."));
		break;
	}
}

TArray<FString>
UHoudiniPCGCookable::GetUnrealObjectPaths(const FPCGContext* Context, const UPCGMetadata* Metadata)
{
	FHoudiniPCGAttributes Attributes(Metadata, FHoudiniPCGUtils::HDAInputObjectName);

	// Extract all soft object paths from the PCG node inputs.

	int NumRows = Attributes.NumRows;
	TArray<FString> NewInputPaths;
	NewInputPaths.Reserve(NumRows);
	for(int Row = 0; Row < NumRows; Row++)
	{
		TArray<FString> DefaultPaths = {};
		TArray<FString> Paths = FHoudiniPCGUtils::GetValueAsString(DefaultPaths, Attributes, Row);
		for(FString Path : Paths)
		{
			if(!Path.IsEmpty())
			{
				UObject* FoundObject = LoadObject<UObject>(nullptr, *Path);
				if(FoundObject)
				{
					NewInputPaths.Add(Path);
					TrackedObjects.Add(FSoftObjectPath(Path));
				}
				else
				{
					FString ErrorText = FString::Printf(TEXT("Input object '%s' could not be found"), *Path);
					FHoudiniPCGUtils::LogVisualError(Context, ErrorText);
					return {};
				}
			}
		}
	}
	return NewInputPaths;
}


bool
UHoudiniPCGCookable::ApplyInputAsUnrealObjects(const FPCGContext* Context, UHoudiniInput* HoudiniInput, const TArray<FString>& InputObjects)
{
	TArray<FString> NewInputPaths = InputObjects;
	NewInputPaths.Sort();

	// First, get list of the current objects. and compare to last set of objects. if its changed, we need to uploaded.

	TArray<FString> CurrentInputObjects;
	for(int Index = 0; Index < HoudiniInput->GetNumberOfInputObjects(); Index++)
	{
		CurrentInputObjects.Add(HoudiniInput->GetInputObjectAt(Index)->GetPathName());
	}
	CurrentInputObjects.Sort();

	// if inputs not changed, do nothing more.
	bool bThisInputChanged = (CurrentInputObjects != NewInputPaths);
	if(!bThisInputChanged)
		return false;

	HoudiniInput->MarkChanged(true);

	TArray<UObject*> WorldObjects;
	TArray<UObject*> GeometryObjects;
	FString InputName = HoudiniInput->GetInputName();

	for(int Index = 0; Index < NewInputPaths.Num(); Index++)
	{
		UObject* InputObject = StaticLoadObject(UObject::StaticClass(), nullptr, *NewInputPaths[Index]);
		if(IsValid(InputObject) && InputObject->IsA<AActor>())
			WorldObjects.Add(InputObject);
		else
			GeometryObjects.Add(InputObject);
	}

	if (HoudiniInput->GetInputType() == EHoudiniInputType::World)
	{
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, WorldObjects.Num());
		for(int Index = 0; Index < WorldObjects.Num(); Index++)
		{
			HoudiniInput->SetInputObjectAt(EHoudiniInputType::World, Index, WorldObjects[Index]);
		}

		if (GeometryObjects.Num())
		{
			FString ErrorString;

			if(WorldObjects.IsEmpty())
			{
				ErrorString = FString::Printf(TEXT("Input %s Type is set to World, but only found Geometry object."), *InputName);
			}
			else
			{
				ErrorString = FString::Printf(TEXT("Input %s Type is set to World, but found Geometry Objects too."), *InputName);
			}

			if (!ErrorString.IsEmpty())
				FHoudiniPCGUtils::LogVisualError(Context, ErrorString);

		}
	}
	else if(HoudiniInput->GetInputType() == EHoudiniInputType::Geometry)
	{
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, GeometryObjects.Num());
		for(int Index = 0; Index < GeometryObjects.Num(); Index++)
		{
			HoudiniInput->SetInputObjectAt(EHoudiniInputType::Geometry, Index, GeometryObjects[Index]);
		}


		if(WorldObjects.Num())
		{
			FString ErrorString;

			if (GeometryObjects.IsEmpty())
			{
				ErrorString = FString::Printf(TEXT("Input %s Type is set to Geometry, but only found World object."), *InputName);
			}
			else
			{
				ErrorString = FString::Printf(TEXT("Input %s Type is set to Geometry, but found World Objects too."), *InputName);
			}

			if(!ErrorString.IsEmpty())
				FHoudiniPCGUtils::LogVisualError(Context, ErrorString);
		}
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("Did not expect to encounter input type: %s. Ensure input types are set correclty in the PCG Graph."), *HoudiniInput->GetInputTypeAsString());
	}

	return true;
}

UHoudiniPCGDataObject*
UHoudiniPCGCookable::GetPCGDataObjects(const FPCGTaggedData& TaggedData)
{
	UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
	PCGDataObject->SetFromPCGData(TaggedData.Data, TaggedData.Tags);
	return PCGDataObject;
}

bool
UHoudiniPCGCookable::ApplyInputAsPCGData(UHoudiniInput* HoudiniInput, const TArray<UHoudiniPCGDataCollection*>& NewPCGCollections)
{
	// Set PCG data. TODO: Check CRCs from PCG to see if data is actually changed.

	if(HoudiniInput->GetInputType() != EHoudiniInputType::PCGInput)
	{
		HOUDINI_LOG_ERROR(TEXT("World output is set to %s when receiving PCG Data"), *HoudiniInput->GetInputTypeAsString());
		return false;
	}

	HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::PCGInput, NewPCGCollections.Num());

	// Set the objects, if changed
	for (int Index = 0; Index < NewPCGCollections.Num(); Index++)
	{
		HoudiniInput->SetInputObjectAt(EHoudiniInputType::PCGInput, 0, NewPCGCollections[Index]);
	}

	HoudiniInput->MarkChanged(true);

	return true;
}

void
UHoudiniPCGCookable::DeleteBakedActor(const FString & ActorPath)
{
	if(ActorPath.IsEmpty())
		return;

	UObject* Actor = StaticLoadObject(UObject::StaticClass(), nullptr, *ActorPath);
	;
	if(AActor* SceneActor = Cast<AActor>(Actor))
	{
		SceneActor->GetWorld()->DestroyActor(SceneActor);
	}
}

void
UHoudiniPCGCookable::DeleteBakedComponent(const FString& ComponentPath)
{
	if(ComponentPath.IsEmpty())
		return;

	UObject* Component = StaticLoadObject(UObject::StaticClass(), nullptr, *ComponentPath);
	;
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SceneComponent->UnregisterComponent();
		SceneComponent->DestroyComponent();
	}
}

void
UHoudiniPCGCookable::DeletePackage(UPackage* Package)
{
	if(!Package)
		return;

	TArray<UObject*> ObjectsToDelete;
	ObjectsToDelete.Add(Package);
	GetObjectsWithOuter(Package, ObjectsToDelete, true);

	ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
}


void
UHoudiniPCGCookable::DeleteBakedObject(const FString& ObjectPath)
{
#if WITH_EDITOR
	if(ObjectPath.IsEmpty())
		return;

	UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if(!IsValid(Object))
		return;

	if(!Object->IsA<UStaticMesh>() && !Object->IsA<UMaterial>())
	{
		// Only delete selected types of objects that should be assets.
		return;
	}

	UPackage* Package = Object->GetPackage();

	if(!Package)
		return;

	FString HoudiniName;

	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(Package, Object, HoudiniName))
	{
		DeletePackage(Package);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Package %s is marked as a baked output, but is not tagged as generated by Houdini. Not deleting."),
			*Package->GetFullName());
	}

#endif
}

void
UHoudiniPCGCookable::DeleteLandscapeLayer(const FString& LandscapePath, TArray<FString> & LandscapeLayers)
{
	FSoftObjectPath Path(LandscapePath);
	ALandscape* Landscape = Cast<ALandscape>(Path.ResolveObject());

	for (auto Layer : LandscapeLayers)
		FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(Landscape, FName(Layer));
}

void
UHoudiniPCGCookable::DeleteFoliage(UWorld * World, UFoliageType * FoliageType, const TArray<FVector> & FoliageInstancePositions)
{
	FHoudiniFoliageTools::RemoveFoliageInstances(World, FoliageType, FoliageInstancePositions);
}

void
UHoudiniPCGCookable::DeleteBakedOutputObject(UWorld* World, FHoudiniBakedOutputObject& BakedOutputObject)
{
	DeleteBakedActor(BakedOutputObject.Actor);
	DeleteBakedObject(BakedOutputObject.BakedObject);
	DeleteBakedComponent(BakedOutputObject.Actor);

	for (FString & ActorPath : BakedOutputObject.InstancedActors)
	{
		DeleteBakedActor(ActorPath);
	}

	for(FString& ActorPath : BakedOutputObject.LevelInstanceActors)
	{
		DeleteBakedActor(ActorPath);
	}
	for(FString& ComponentPath : BakedOutputObject.InstancedComponents)
	{
		DeleteBakedActor(ComponentPath);
	}


	DeleteLandscapeLayer(BakedOutputObject.Landscape, BakedOutputObject.CreatedLandscapeLayers);

	DeleteBakedActor(BakedOutputObject.Actor);

	DeleteFoliage(World, BakedOutputObject.FoliageType.Get(), BakedOutputObject.FoliageInstancePositions);

	for(FString& FoliageActor : BakedOutputObject.FoliageActors)
	{
		DeleteBakedActor(FoliageActor);
	}

	DeleteBakedObject(BakedOutputObject.BakedSkeleton);
	DeleteBakedObject(BakedOutputObject.BakedPhysicsAsset);
}

void
UHoudiniPCGCookable::DeleteBakedOutput(UWorld* World)
{
	if(!IsValid(this->Cookable))
		return;

	TArray<FHoudiniBakedOutput>& BakedOutputs =  this->Cookable->GetBakedOutputs();

	for (FHoudiniBakedOutput & BakedOutput : BakedOutputs)
	{
		for (auto It : BakedOutput.BakedOutputObjects)
		{
			FHoudiniBakedOutputObject & BakedOutputObject =  It.Value;

			DeleteBakedOutputObject(World, BakedOutputObject);
		}
	}
	BakedOutputs.Empty();

	// Delete PDG Output

	if (this->PDGBakedOutput)
	{
		for (auto & BakedOutput : this->PDGBakedOutput->BakedOutputs)
		{
			for(auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& BakedOutputObject = It.Value;

				DeleteBakedOutputObject(World, BakedOutputObject);
			}
		}

		this->PDGBakedOutput->ConditionalBeginDestroy();
		this->PDGBakedOutput = nullptr;
	}

}

#undef LOCTEXT_NAMESPACE
