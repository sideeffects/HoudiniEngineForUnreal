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
#include "HoudiniOutputTranslator.h"
#include <HoudiniParameterToggle.h>
#include <HoudiniEngineUtils.h>
#include "HoudiniPCGDataObject.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "PCGCachedCookable"


UHoudiniPCGCookable::UHoudiniPCGCookable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniPCGCookable::~UHoudiniPCGCookable()
{
}

void UHoudiniPCGCookable::OnCookingComplete(bool bSuccess)
{
	HOUDINI_PCG_MESSAGE(TEXT("(%p) UHoudiniPCGCookable::OnCookingComplete"), this);

	if(this->State == EPCGCookableState::Initializing)
	{
		HOUDINI_PCG_MESSAGE(TEXT("(%p)       Set to EPCGCookableState::Initialized"), this);
		this->State = EPCGCookableState::Initialized;
	}
	else if(this->State == EPCGCookableState::Cooking)
	{
		HOUDINI_PCG_MESSAGE(TEXT("(%p)       Set to EPCGCookableState::Done"), this);
		this->State = EPCGCookableState::Done;
	}
	else
	{
		// We were not expecting a cooking operation to complete. This is caused the HDA being modified
		// most likely via session sync. So regen.
		if (IsValid(PCGComponent))
		{
			PCGComponent->Generate();
		}

	}
}

void UHoudiniPCGCookable::CreateHoudiniCookable(UHoudiniAsset* Asset, UHoudiniPCGSettings* Owner, UHoudiniPCGComponent* Component)
{
	HOUDINI_PCG_MESSAGE(TEXT("(%p) UHoudiniPCGCookable::CreateHoudiniCookable"), this);

	TrackedObjects.Empty();

	Cookable = NewObject<UHoudiniCookable>(this);
	State = EPCGCookableState::Initializing;
	auto OutputDelegateHandle = Cookable->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniCookable* _HC, bool  bSuccess)
		{
			this->OnCookingComplete(bSuccess);
		});

	Cookable->SetSlateNotifications(false);
	Cookable->SetUpdateEditorProperties(false);
	Cookable->SetParameterSupported(true);
	Cookable->SetInputSupported(true);
	Cookable->SetComponentSupported(Component ? true : false);
	Cookable->SetEnableProxyStaticMeshOverride(false);
	Cookable->SetOverrideGlobalProxyStaticMeshSettings(true);
	Cookable->SetAutoCook(false);
	// Disable auto-cook, it improve this logic.
	Cookable->GetParameterData()->bCookOnParameterChange = false;
	Cookable->GetInputData()->bCookOnInputChange = false;

	if(Component)
	{
		Cookable->SetComponent(Component);
	}

	Cookable->SetHoudiniAssetSupported(true);
	UCookableHoudiniAssetData* HAD = Cookable->GetHoudiniAssetData();
	HAD->HoudiniAsset = Asset;
	this->State = EPCGCookableState::None;
}

void UHoudiniPCGCookable::Instantiate()
{
	HOUDINI_PCG_MESSAGE(TEXT("(%p) UHoudiniPCGCookable::Instantiate"), this);
	this->State = EPCGCookableState::WaitingForSession;
	FHoudiniPCGUtils::StartSessionAsync();
}

void
UHoudiniPCGCookable::InvalidateCookable()
{
	FHoudiniOutputTranslator::ClearAndRemoveOutputs(this->Cookable->GetOutputs(), this->bAutomaticallyDeleteAssets);

	if(!IsValid(this->Cookable.Get()))
		return;

	FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(this->Cookable.Get());

	this->Cookable = nullptr; // Garbage Collection will clean this up.
}

bool
UHoudiniPCGCookable::ApplyParametersToCookable(FPCGContext* Context, bool& bError)
{
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(FName(FHoudiniPCGUtils::ParameterInputPinName));

	bool bChanged = false;
	for(auto& TaggedData : Inputs)
	{
		bChanged |= ApplyParametersToCookable(TaggedData.Data, Context, bError);
	}

	if (bChanged)
	{
		// Changing the parameters will start a cook.
		State = EPCGCookableState::Cooking;
	}
	return bChanged;
}


bool UHoudiniPCGCookable::ApplyInputsToCookable(FPCGContext* Context, bool& bError)
{
	int NumInputs = this->Cookable->GetNumInputs();

	for(int Index = 0; Index < NumInputs; Index++)
	{
		UHoudiniInput* Input = this->Cookable->GetInputAt(Index);

		FString InputName = FHoudiniPCGUtils::GetHDAInputName(Index);

		const TArray<FPCGTaggedData>& ContextInputData = Context->InputData.GetInputsByPin(FName(InputName));

		// See if the input contains objects, going through each tagged data.
		TArray<FString> UnrealObjectPaths;
		for (const FPCGTaggedData& InputData : ContextInputData)
		{
			const UPCGMetadata* Metadata = InputData.Data->ConstMetadata();
			TArray<FString> UnrealObjects = GetUnrealObjectPaths(Context, Metadata, bError);
			if (!bError && !UnrealObjects.IsEmpty())
			{
				UnrealObjectPaths.Append(UnrealObjects);
			}
		}

		if (!UnrealObjectPaths.IsEmpty())
		{
			// Looks like we have Unreal objects, so set those on the current input.
			bInputsChanged |= ApplyInputAsUnrealObjects(Context, Input, UnrealObjectPaths, bError);
		}
		else
		{
			if (ContextInputData.Num())
			{
				UHoudiniPCGDataCollection* DataCollection = NewObject<UHoudiniPCGDataCollection>(this);

				for(const FPCGTaggedData& InputData : ContextInputData)
				{
					UHoudiniPCGDataObject* PCGDataObject = GetPCGDataObjects(Context, InputData);
					if(!PCGDataObject)
						continue;

					DataCollection->AddObject(PCGDataObject);
				}

				if(DataCollection)
					bInputsChanged |= ApplyInputAsPCGData(Context, Input, { DataCollection });
				else
					bInputsChanged |= ApplyInputAsPCGData(Context, Input, { });
			}
			else
			{
				// Do nothing.
			}

		}
	}

	return bInputsChanged;
}

void
UHoudiniPCGCookable::AddTrackedObjects(FPCGContext* Context)
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

bool UHoudiniPCGCookable::ApplyParametersToCookable(const UPCGData* Data, FPCGContext* Context, bool& bError)
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

void UHoudiniPCGCookable::Release()
{
	HOUDINI_PCG_MESSAGE(TEXT("UHoudiniPCGCookable::Release (%p)"), this);

	InvalidateCookable();

}

void UHoudiniPCGCookable::CreateOutputs(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput)
{
	if(FHoudiniPCGUtils::HasPCGOutputs(HoudiniOutput))
	{
		CreateOutputsAsPCGData(Context, OutputPinName, TagName, HoudiniOutput);
	}
	else
	{
		TArray<FHoudiniPCGObjectOutput> Outputs = FHoudiniPCGUtils::GetPCGOutputData(HoudiniOutput);

		CreateOutputsAsObjectReferences(Context, OutputPinName, TagName, Outputs);
	}
}

void UHoudiniPCGCookable::CreateOutputsAsPCGData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput)
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
	}
}

void UHoudiniPCGCookable::CreateOutputsAsObjectReferences(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const TArray<FHoudiniPCGObjectOutput>& Outputs)
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
UHoudiniPCGCookable::ProcessCookableOutput(FPCGContext* Context)
{
	const UHoudiniPCGSettings* Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	if(!this->Cookable->GetOutputData())
		return;

	switch(Settings->OutputType)
	{
	case EHoudiniPCGOutputType::Cook:
	case EHoudiniPCGOutputType::Bake:
	{
		auto& Outputs = this->Cookable->GetOutputData()->Outputs;
		for(int Index = 0; Index < Outputs.Num(); Index++)
		{
			FString Tag = FString::Printf(TEXT("Output-%d"), Index);
			CreateOutputs(Context, Settings->GetOutputPinName(), Tag, this->Cookable->GetOutputData()->Outputs[Index]);
		}
		break;
	}
	default:
		break;
	}

	AddTrackedObjects(Context);
	State = EPCGCookableState::Done;

}


void UHoudiniPCGCookable::CopyParametersAndInputs(const UHoudiniPCGCookable * Other)
{
	bParamsChanged |= Cookable->SetParameterData(Other->Cookable->GetParameterData());
	bInputsChanged |= Cookable->SetInputData(Other->Cookable->GetInputData());
}

bool UHoudiniPCGCookable::UpdateParametersAndInputs(FPCGContext* Context)
{
	bool bError = false;
	Cookable->SetOutputSupported(true);

	const UHoudiniPCGSettings* Settings = nullptr;
	if(Context)
		Settings = Context->GetInputSettings<UHoudiniPCGSettings>();

	Cookable->GetOutputData()->bCreateSceneComponents = Settings ? Settings->bCreateSceneComponents : false;

	if(Context)
	{
		bParamsChanged |= this->ApplyParametersToCookable(Context, bError);
		if(bError)
			return false;

		bInputsChanged |= this->ApplyInputsToCookable(Context, bError);
		if(bError)
			return false;
	}

	//int CurrentCookCount = FHoudiniEngineUtils::HapiGetCookCount(Cookable->GetNodeId());
	//bool bCookCountChanged = this->CookCount != CurrentCookCount;
	//this->CookCount = CurrentCookCount;

	return true;
}

bool UHoudiniPCGCookable::NeedsCook()
{

	bool bHasBeenCooked = State == EPCGCookableState::Done;

	return (bInputsChanged || bParamsChanged || !bHasBeenCooked);
}


void UHoudiniPCGCookable::StartCook()
{
	ensure(NeedsCook());
	State = EPCGCookableState::Cooking;
	HOUDINI_PCG_MESSAGE(TEXT("(%p) Inputs not changed on cookable, but forcing cooking to get outputs."), this);
	Cookable->MarkAsNeedCook();
	bInputsChanged = false;
	bParamsChanged = false;
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
		// Still cooking, wait.
		break;

	case EPCGCookableState::Done:
		// Done - process results.
		HOUDINI_PCG_MESSAGE(TEXT("DONE cooking Managed Resource (%p)"), this);
		this->ProcessCookableOutput(Context);
		CookCount = FHoudiniEngineUtils::HapiGetCookCount(Cookable->GetNodeId());
		break;

	default:
		// Shouldn't get here.
		HOUDINI_LOG_ERROR(TEXT("Unexpected state: default. PCG Cooking failed."));
		break;
	}
}

TArray<FString>
UHoudiniPCGCookable::GetUnrealObjectPaths(FPCGContext* Context, const UPCGMetadata* Metadata, bool& bError)
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
					bError = true;
					return {};
				}
			}
		}
	}
	return NewInputPaths;
}


bool
UHoudiniPCGCookable::ApplyInputAsUnrealObjects(FPCGContext* Context, UHoudiniInput* HoudiniInput, const TArray<FString>& InputObjects, bool& bError)
{

	TArray<FString> NewInputPaths = InputObjects;
	NewInputPaths.Sort();

	// Geta list of current input objects.

	TArray<FString> CurrentInputObjects;
	for(int Index = 0; Index < HoudiniInput->GetNumberOfInputObjects(); Index++)
	{
		CurrentInputObjects.Add(HoudiniInput->GetInputObjectAt(Index)->GetPathName());
	}
	CurrentInputObjects.Sort();

	// if inputs changed, set them
	bool bThisInputChanged = (CurrentInputObjects != NewInputPaths);
	if(bThisInputChanged)
	{
		HoudiniInput->MarkChanged(true);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Curve, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::PCGInput, 0);

		TArray<UObject*> WorldObjects;
		TArray<UObject*> GeometryObjects;

		for(int Index = 0; Index < NewInputPaths.Num(); Index++)
		{
			UObject* InputObject = StaticLoadObject(UObject::StaticClass(), nullptr, *NewInputPaths[Index]);
			if(IsValid(InputObject) && InputObject->IsA<AActor>())
				WorldObjects.Add(InputObject);
			else
				GeometryObjects.Add(InputObject);
		}

		if (WorldObjects.Num())
		{
			bool bBlueprintModified;
			HoudiniInput->SetInputType(EHoudiniInputType::World, bBlueprintModified);

			HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, WorldObjects.Num());
			for(int Index = 0; Index < WorldObjects.Num(); Index++)
			{
				HoudiniInput->SetInputObjectAt(EHoudiniInputType::World, Index, WorldObjects[Index]);
			}
		}
		else if (GeometryObjects.Num())
		{
			bool bBlueprintModified;
			HoudiniInput->SetInputType(EHoudiniInputType::Geometry, bBlueprintModified);

			HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, GeometryObjects.Num());
			for(int Index = 0; Index < GeometryObjects.Num(); Index++)
			{
				HoudiniInput->SetInputObjectAt(EHoudiniInputType::Geometry, Index, GeometryObjects[Index]);
			}
		}
	}

	return bThisInputChanged;
}

UHoudiniPCGDataObject*
UHoudiniPCGCookable::GetPCGDataObjects(FPCGContext* Context, const FPCGTaggedData& TaggedData)
{
	UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
	PCGDataObject->Initialize(TaggedData.Data, TaggedData.Tags);
	return PCGDataObject;
}

bool
UHoudiniPCGCookable::ApplyInputAsPCGData(FPCGContext* Context, UHoudiniInput* HoudiniInput, const TArray<UHoudiniPCGDataCollection*>& PCGCollections)
{
	// Was input previously set to Geometry, World, Curve or Geometry? If so, clear it out and set to PCG

	if(HoudiniInput->GetInputType() != EHoudiniInputType::PCGInput)
	{
		int ExistingObjectCount = 0;
		ExistingObjectCount += HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);
		ExistingObjectCount += HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::Curve);
		ExistingObjectCount += HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::World);
		if(ExistingObjectCount > 0)
		{
			// Previous input used non-PCG type, so we must clear them and re-upload.
			HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, 0);
			HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Curve, 0);
			HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, 0);
		}


	}

	// Clear out previous inputs then set the new number. This has the effect of deleting
	// all previous PCG Data and reloading it all. Since PCG Data changes very frequently,
	// this may not be too inefficent, but could look into using CRCs to prevent uploading
	// data that hasn't changed?

	bool bOutBlueprintStructureModified;
	HoudiniInput->SetInputType(EHoudiniInputType::PCGInput, bOutBlueprintStructureModified);
	HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::PCGInput, 0);
	HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::PCGInput, PCGCollections.Num());

	// Set the objects, if changed
	for (int Index = 0; Index < PCGCollections.Num(); Index++)
	{
		HoudiniInput->SetInputObjectAt(EHoudiniInputType::PCGInput, 0, PCGCollections[Index]);
	}

	HoudiniInput->MarkChanged(true);

	return true;
}


#undef LOCTEXT_NAMESPACE
