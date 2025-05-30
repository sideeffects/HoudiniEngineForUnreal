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

#include "HoudiniCookableDetails.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniCookable.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniParameter.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniParameterDetails.h"
#include "HoudiniInput.h"
#include "HoudiniInputDetails.h"
#include "HoudiniHandleDetails.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniOutputDetails.h"
#include "SHoudiniPresets.h"

#include "Chaos/AABB.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Engine/StaticMesh.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailGroup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "UObject/ObjectMacros.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 


TSharedRef< IDetailCustomization >
FHoudiniCookableDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniCookableDetails);
}

FHoudiniCookableDetails::FHoudiniCookableDetails()
{
	OutputDetails = MakeShared<FHoudiniOutputDetails, ESPMode::NotThreadSafe>();
	ParameterDetails = MakeShared<FHoudiniParameterDetails, ESPMode::NotThreadSafe>();
	PDGDetails = MakeShared<FHoudiniPDGDetails, ESPMode::NotThreadSafe>();
	HoudiniEngineDetails = MakeShared<FHoudiniEngineDetails, ESPMode::NotThreadSafe>();
}

void
FHoudiniCookableDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get all components which are being customized.
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	// Components which are being customized.
	TArray<TWeakObjectPtr<UHoudiniCookable>> HoudiniCookable;

	// Extract the Houdini Asset Component to detail
	for (int32 i = 0; i < ObjectsCustomized.Num(); ++i)
	{
		if (!IsValidWeakPointer(ObjectsCustomized[i]))
			continue;

		UObject * Object = ObjectsCustomized[i].Get();
		if (Object)
		{
			UHoudiniCookable* HC = Cast<UHoudiniCookable>(Object);
			if (IsValid(HC))
			{
				HoudiniCookable.Add(HC);
				continue;
			}

			UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(Object);
			if (IsValid(HAC))
			{
				HC = HAC->GetCookable();
				if (IsValid(HC))
					HoudiniCookable.Add(HC);
				
				continue;
			}

			AHoudiniAssetActor* HAA = Cast<AHoudiniAssetActor>(Object);
			if (IsValid(HAA))
			{
				HC = HAA->GetHoudiniCookable();
				if(IsValid(HC))
					HoudiniCookable.Add(HC);

				continue;
			}
		}
	}

	// Check if we'll need to add indie license labels
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();

	// To handle multiselection parameter edit, we try to group the selected components by their houdini assets
	// TODO? ignore multiselection if all are not the same HDA?
	TMap<TWeakObjectPtr<UHoudiniAsset>, TArray<TWeakObjectPtr<UHoudiniCookable>>> HoudiniAssetToCookables;
	for (auto currentHC : HoudiniCookable)
	{
		// Add cookable with no assets
		if (!currentHC->IsHoudiniAssetSupported())
		{
			TArray<TWeakObjectPtr<UHoudiniCookable>>& ValueRef = HoudiniAssetToCookables.FindOrAdd(nullptr);
			ValueRef.Add(currentHC);
			continue;
		}

		TWeakObjectPtr<UHoudiniAsset> HoudiniAsset = currentHC->GetHoudiniAsset();
		TArray<TWeakObjectPtr<UHoudiniCookable>>& ValueRef = HoudiniAssetToCookables.FindOrAdd(HoudiniAsset);
		ValueRef.Add(currentHC);
	}

	for (auto Iter : HoudiniAssetToCookables)
	{
		TArray<TWeakObjectPtr<UHoudiniCookable>> HCs = Iter.Value;
		if (HCs.Num() < 1)
			continue;
			   
		TWeakObjectPtr<UHoudiniCookable> MainCookable = HCs[0];
		if (!IsValidWeakPointer(MainCookable))
			continue;

		// If we have selected more than one component that have different HDAs, 
		// we'll want to separate the param/input/output category for each HDA
		FString MultiSelectionIdentifier = FString();
		if (HoudiniAssetToCookables.Num() > 1)
		{
			MultiSelectionIdentifier = TEXT("(");
			if (MainCookable->GetHoudiniAsset())
				MultiSelectionIdentifier += MainCookable->GetHoudiniAssetName();
			MultiSelectionIdentifier += TEXT(")");
		}

		//
		// HOUDINI ENGINE DETAILS
		//
		CreateHoudiniEngineDetails(DetailBuilder, HCs, MultiSelectionIdentifier);


		//
		// HOUDINI ASSET DETAILS
		//
		if (MainCookable->IsHoudiniAssetSupported())
		{
			CreateHoudiniAssetDetails(DetailBuilder, HCs);
		}
		
		
		//
		// NODE SYNC DETAILS
		//		
		bool bIsNodeSyncComponent = MainCookable->GetComponent() ? MainCookable->GetComponent()->IsA<UHoudiniNodeSyncComponent>() : false;		
		if (bIsNodeSyncComponent)
		{
			CreateNodeSyncDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// PDG ASSET LINK (if available)
		//
		if (MainCookable->IsPDGSupported())
		{
			CreatePDGDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// PARAMETER DETAILS
		//
		if (MainCookable->IsParameterSupported())
		{
			CreateParameterDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// HANDLE DETAILS
		//
		if (MainCookable->IsComponentSupported())
		{
			CreateHandleDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// INPUT DETAILS
		//
		if (MainCookable->IsInputSupported())
		{
			CreateInputDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// OUTPUT DETAILS
		//
		if (MainCookable->IsOutputSupported())
		{
			CreateOutputDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// PROXY SETTINGS
		//
		if (MainCookable->IsProxySupported())
		{
			CreateProxyDetails(DetailBuilder, HCs);
		}

		//
		// MESH BUILD SETTINGS
		// MESH GENERATION PROPERTIES
		//
		if (MainCookable->IsOutputSupported())
		{
			CreateMeshBuildSettingsDetails(DetailBuilder, HCs);
			CreateMeshGenerationDetails(DetailBuilder, HCs);
		}
	}
}

void
FHoudiniCookableDetails::CreateHoudiniEngineDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier,
	const EHoudiniDetailsFlags& DetailsFlags)
{
	FString HoudiniEngineCategoryName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_MAIN);
	HoudiniEngineCategoryName += MultiSelectionIdentifier;

	TSharedPtr<SLayeredImage> OptionsImage = SNew(SLayeredImage)
		.Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
		.ColorAndOpacity(FSlateColor::UseForeground());

	// Create Houdini Engine details category
	IDetailCategoryBuilder& HouEngineCategory =
		DetailBuilder.EditCategory(*HoudiniEngineCategoryName, FText::FromString("Houdini Engine"), ECategoryPriority::Important);

	// If we are running Houdini Engine Indie license, we need to display a special label.
	// Check if we'll need to add indie license labels
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();
	if (bIsIndieLicense)
		FHoudiniEngineDetails::AddIndieLicenseRow(HouEngineCategory);
	else if (bIsEduLicense)
		FHoudiniEngineDetails::AddEducationLicenseRow(HouEngineCategory);

	// Houdini Engine Icon
	HoudiniEngineDetails->CreateHoudiniEngineIconWidget(HouEngineCategory);

	// Widget for HoudiniAsset related actions. Currently only contains things for Presets.
	//HoudiniEngineDetails->CreateHoudiniEngineActionWidget(HouEngineCategory, InCookables);

	// Houdini Engine Session Status
	HoudiniEngineDetails->AddSessionStatusRow(HouEngineCategory);

	// Create Generate Category
	if (DetailsFlags.bGenerateBar)
		HoudiniEngineDetails->CreateGenerateWidgets(HouEngineCategory, InCookables);

	// Create Bake Category
	HoudiniEngineDetails->CreateBakeWidgets(HouEngineCategory, InCookables, DetailsFlags);

	// Create Asset Options Category
	if (DetailsFlags.bAssetOptions)
		HoudiniEngineDetails->CreateAssetOptionsWidgets(HouEngineCategory, InCookables);

	// Create Help and Debug Category
	HoudiniEngineDetails->CreateHelpAndDebugWidgets(HouEngineCategory, InCookables);
}

void
FHoudiniCookableDetails::CreateHoudiniAssetDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables)
{
	if (InCookables.IsEmpty())
		return;

	// Create the HDA details category
	FString AssetCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_HDA);
	IDetailCategoryBuilder& HouAssetCategory =
		DetailBuilder.EditCategory(*AssetCatName, FText::GetEmpty(), ECategoryPriority::Important);

	HoudiniEngineDetails->CreateHoudiniAssetDetails(HouAssetCategory, InCookables);
}


void
FHoudiniCookableDetails::CreateNodeSyncDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
	// If we are working on a node sync component, display its specific options
	FString HoudiniNodeSyncCategoryName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_NODESYNC);
	HoudiniNodeSyncCategoryName += MultiSelectionIdentifier;

	// Create Houdini Engine details category
	IDetailCategoryBuilder& HouNodeSyncCategory =
		DetailBuilder.EditCategory(*HoudiniNodeSyncCategoryName, FText::FromString("Houdini - Node Sync"), ECategoryPriority::Important);
	HoudiniEngineDetails->CreateNodeSyncWidgets(HouNodeSyncCategory, InCookables);
}

void
FHoudiniCookableDetails::CreatePDGDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	UHoudiniPDGAssetLink* HPDGAL = MainCookable->GetPDGAssetLink();
	if (!HPDGAL)
		return;

	FString PDGCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PDG);
	PDGCatName += MultiSelectionIdentifier;

	// Create the PDG Asset Link details category
	IDetailCategoryBuilder& HouPDGCategory =
		DetailBuilder.EditCategory(*PDGCatName, FText::FromString("Houdini - PDG Asset Link"), ECategoryPriority::Important);

	// If we are running Houdini Engine Indie license, we need to display a special label.
	// Check if we'll need to add indie license labels
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();
	if (bIsIndieLicense)
		FHoudiniEngineDetails::AddIndieLicenseRow(HouPDGCategory);
	else if (bIsEduLicense)
		FHoudiniEngineDetails::AddEducationLicenseRow(HouPDGCategory);

	// TODO: Handle multi selection ?
	PDGDetails->CreateWidget(HouPDGCategory, HPDGAL, MainCookable->GetIsPCG());
}

void
FHoudiniCookableDetails::CreateParameterDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
	if(InCookables.IsEmpty())
		return;

	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (MainCookable->GetNumParameters() <= 0)
		return;

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	FString ParamCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PARAMS);
	ParamCatName += MultiSelectionIdentifier;

	// Create the Parameters details category
	IDetailCategoryBuilder& HouParameterCategory =
		DetailBuilder.EditCategory(*ParamCatName, FText::GetEmpty(), ECategoryPriority::Important);

	// If we are running Houdini Engine Indie license, we need to display a special label.
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();
	if (bIsIndieLicense)
		FHoudiniEngineDetails::AddIndieLicenseRow(HouParameterCategory);
	else if (bIsEduLicense)
		FHoudiniEngineDetails::AddEducationLicenseRow(HouParameterCategory);

	// Iterate through the component's parameters
	TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>> JoinedParams;	
	for (int32 ParamIdx = 0; ParamIdx < MainCookable->GetNumParameters(); ParamIdx++)
	{
		// We only want to create root parameters here, they will recursively create child parameters.
		UHoudiniParameter* CurrentParam = MainCookable->GetParameterAt(ParamIdx);
		if (!IsValid(CurrentParam))
			continue;

		// Build an array of edited parameter for multi edit
		JoinedParams.Emplace();
		auto& EditedParams = JoinedParams.Last();
		EditedParams.Add(CurrentParam);

		// Add the corresponding params in the other HAC
		for (int LinkedIdx = 1; LinkedIdx < InCookables.Num(); LinkedIdx++)
		{
			UHoudiniParameter* LinkedParam = InCookables[LinkedIdx]->GetParameterAt(ParamIdx);
			if (!IsValid(LinkedParam))
				continue;

			// Linked params should match the main param! If not try to find one that matches
			if (!LinkedParam->Matches(*CurrentParam))
			{
				LinkedParam = MainCookable->FindMatchingParameter(CurrentParam);
				if (!IsValid(LinkedParam) || LinkedParam->IsChildParameter())
					continue;
			}

			EditedParams.Add(LinkedParam);
		}

		if (ParameterDetails->ShouldJoinNext(*CurrentParam))
		{
			continue;
		}

		ParameterDetails->CreateWidget(HouParameterCategory, JoinedParams);
		JoinedParams.Empty();
	}
}

void
FHoudiniCookableDetails::CreateHandleDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (MainCookable->GetNumHandles() <= 0)
		return;

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	FString HandleCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_HANDLES);
	HandleCatName += MultiSelectionIdentifier;

	// Create the Parameters details category
	IDetailCategoryBuilder& HouHandleCategory =
		DetailBuilder.EditCategory(*HandleCatName, FText::GetEmpty(), ECategoryPriority::Important);

	// If we are running Houdini Engine Indie license, we need to display a special label.
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();
	if (bIsIndieLicense)
		FHoudiniEngineDetails::AddIndieLicenseRow(HouHandleCategory);
	else if (bIsEduLicense)
		FHoudiniEngineDetails::AddEducationLicenseRow(HouHandleCategory);

	// Iterate through the component's Houdini handles
	for (int32 HandleIdx = 0; HandleIdx < MainCookable->GetNumHandles(); ++HandleIdx)
	{
		UHoudiniHandleComponent* CurrentHandleComponent = MainCookable->GetHandleComponentAt(HandleIdx);
		if (!IsValid(CurrentHandleComponent))
			continue;

		TArray<TWeakObjectPtr<UHoudiniHandleComponent>> EditedHandles;
		EditedHandles.Add(CurrentHandleComponent);

		// Add the corresponding params in the other HAC
		for (int LinkedIdx = 1; LinkedIdx < InCookables.Num(); ++LinkedIdx)
		{
			UHoudiniHandleComponent* LinkedHandle = InCookables[LinkedIdx]->GetHandleComponentAt(HandleIdx);
			if (!IsValid(LinkedHandle))
				continue;

			// Linked handles should match the main param, if not try to find one that matches
			if (!LinkedHandle->Matches(*CurrentHandleComponent))
			{
				LinkedHandle = MainCookable->FindMatchingHandle(CurrentHandleComponent);
				if (!IsValid(LinkedHandle))
					continue;
			}

			EditedHandles.Add(LinkedHandle);
		}

		FHoudiniHandleDetails::CreateWidget(HouHandleCategory, EditedHandles);
	}
}

void
FHoudiniCookableDetails::CreateInputDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
	if(InCookables.IsEmpty())
		return;

	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (MainCookable->GetNumInputs() <= 0)
		return;

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	FString InputCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_INPUTS);
	InputCatName += MultiSelectionIdentifier;

	// Create the input details category
	IDetailCategoryBuilder& HouInputCategory =
		DetailBuilder.EditCategory(*InputCatName, FText::GetEmpty(), ECategoryPriority::Important);

	// If we are running Houdini Engine Indie license, we need to display a special label.
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();
	if (bIsIndieLicense)
		FHoudiniEngineDetails::AddIndieLicenseRow(HouInputCategory);
	else if (bIsEduLicense)
		FHoudiniEngineDetails::AddEducationLicenseRow(HouInputCategory);

	// Iterate through the component's inputs
	for (int32 InputIdx = 0; InputIdx < MainCookable->GetNumInputs(); InputIdx++)
	{
		UHoudiniInput* CurrentInput = MainCookable->GetInputAt(InputIdx);
		if (!IsValid(CurrentInput))
			continue;

		// TODO COOKABLE: ?? handle needed ? this is mostly for BP
		if (!MainCookable->IsInputTypeSupported(CurrentInput->GetInputType()))
			continue;

		// Object path parameter inputs are displayed by the ParameterDetails - skip them
		if (CurrentInput->IsObjectPathParameter())
			continue;

		// Build an array of edited inputs for multi edit
		TArray<TWeakObjectPtr<UHoudiniInput>> EditedInputs;
		EditedInputs.Add(CurrentInput);

		// Add the corresponding inputs in the other HAC
		for (int LinkedIdx = 1; LinkedIdx < InCookables.Num(); LinkedIdx++)
		{
			UHoudiniInput* LinkedInput = InCookables[LinkedIdx]->GetInputAt(InputIdx);
			if (!IsValid(LinkedInput))
				continue;

			// Linked params should match the main param! If not try to find one that matches
			if (!LinkedInput->Matches(*CurrentInput))
			{
				LinkedInput = MainCookable->FindMatchingInput(CurrentInput);
				if (!IsValid(LinkedInput))
					continue;
			}

			EditedInputs.Add(LinkedInput);
		}

		FHoudiniInputDetails::CreateWidget(HouInputCategory, EditedInputs);
	}
}

void
FHoudiniCookableDetails::CreateOutputDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;
	/*
	// Still display the output section even if we havent produce any output
	if (MainCookable->GetNumOutputs() <= 0)
		return;
	*/

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	FString OutputCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_OUTPUTS);
	OutputCatName += MultiSelectionIdentifier;

	// Create the output details category
	IDetailCategoryBuilder& HouOutputCategory =
		DetailBuilder.EditCategory(*OutputCatName, FText::GetEmpty(), ECategoryPriority::Important);

	// Iterate through the component's outputs
	for (int32 OutputIdx = 0; OutputIdx < MainCookable->GetNumOutputs(); OutputIdx++)
	{
		UHoudiniOutput* CurrentOutput = MainCookable->GetOutputAt(OutputIdx);
		if (!IsValid(CurrentOutput))
			continue;

		// Build an array of edited outputs for multi edit
		TArray<TWeakObjectPtr<UHoudiniOutput>> EditedOutputs;
		EditedOutputs.Add(CurrentOutput);

		// Add the corresponding outputs in the other HAC
		for (int LinkedIdx = 1; LinkedIdx < InCookables.Num(); LinkedIdx++)
		{
			UHoudiniOutput* LinkedOutput = InCookables[LinkedIdx]->GetOutputAt(OutputIdx);
			if (!IsValid(LinkedOutput))
				continue;

			EditedOutputs.Add(LinkedOutput);
		}

		// TODO: Handle multi selection of outputs like params/inputs?	
		OutputDetails->CreateWidget(HouOutputCategory, EditedOutputs);
	}
}


void 
FHoudiniCookableDetails::CreateProxyDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables)
{
	if (InCookables.Num() <= 0)
		return;

	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (!MainCookable->IsProxySupported())
		return;

	// Create the Proxy details category
	FString ProxyCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_MESHGEN);

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	// OutputCatName += MultiSelectionIdentifier;
	
	IDetailCategoryBuilder& HouProxyCategory =
		DetailBuilder.EditCategory(*ProxyCatName, FText::GetEmpty(), ECategoryPriority::Important);

	FString Label = TEXT("Houdini Proxy Mesh Settings");
	IDetailGroup& ProxyGrp = HouProxyCategory.AddGroup(FName(*Label), FText::FromString(Label));

	// Lambda used to trigger a refine of the cookables if necessary
	auto RefineCookablesIfNeeded = [InCookables]()
	{
		TArray<AHoudiniAssetActor*> ActorsToRefine;
		for (auto CurCookable : InCookables)
		{
			if (!IsValidWeakPointer(CurCookable))
				continue;

			AHoudiniAssetActor* CurActor = Cast<AHoudiniAssetActor>(CurCookable->GetOwner());
			if (!IsValid(CurActor))
				continue;

			if(!CurCookable->IsProxyStaticMeshEnabled())
				ActorsToRefine.Add(CurActor);
		}
		
		FHoudiniEngineUtils::RefineHoudiniProxyMeshActorArrayToStaticMeshes(ActorsToRefine);
	};

	//
	// Override Global Proxy Mesh Setting
	//
	//HouProxyCategory.AddCustomRow()
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Override Global Proxy Mesh Setting"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->IsOverrideGlobalProxyStaticMeshSettings() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, RefineCookablesIfNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const bool bNewState = (NewState == ECheckBoxState::Checked);
				if (MainCookable->IsOverrideGlobalProxyStaticMeshSettings() == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniOverrideProxyChange", "Houdini Input: Override Global Proxy Mesh Settings"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					if (CurCookable->IsOverrideGlobalProxyStaticMeshSettings() == bNewState)
						continue;

					CurCookable->Modify();
					CurCookable->SetOverrideGlobalProxyStaticMeshSettings(bNewState);
					// Reset the timer
					CurCookable->ClearRefineMeshesTimer();
					// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
					CurCookable->SetRefineMeshesTimer();
				}

				// Refine if needed
				RefineCookablesIfNeeded();
			})
		]
	];


	// Enable Proxy Mesh
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Enable Proxy Mesh"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->IsProxyStaticMeshEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled_Lambda([MainCookable]() {return MainCookable->IsOverrideGlobalProxyStaticMeshSettings(); })
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, RefineCookablesIfNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const bool bNewState = (NewState == ECheckBoxState::Checked);
				if (MainCookable->IsProxyStaticMeshEnabled() == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniOverrideProxyEnable", "Houdini: Override Proxy Mesh Enabled"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					if (CurCookable->IsProxyStaticMeshEnabled() == bNewState)
						continue;

					CurCookable->Modify();
					CurCookable->SetEnableProxyStaticMeshOverride(bNewState);
					// Reset the timer
					CurCookable->ClearRefineMeshesTimer();
					// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
					CurCookable->SetRefineMeshesTimer();
				}

				// Refine if needed
				RefineCookablesIfNeeded();
			})
		]
	];

	// Refine Proxy Meshes after a timeout
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Refine Proxy Meshes after a timeout"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->GetProxyData()->bEnableProxyStaticMeshRefinementByTimerOverride ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled_Lambda([MainCookable]() {return MainCookable->IsOverrideGlobalProxyStaticMeshSettings(); })
			.OnCheckStateChanged_Lambda([MainCookable, InCookables](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const bool bNewState = (NewState == ECheckBoxState::Checked);
				if (MainCookable->GetProxyData()->bEnableProxyStaticMeshRefinementByTimerOverride == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniOverrideProxyByTimerEnable", "Houdini: Override Proxy Mesh Refine by Timer"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					if (CurCookable->GetProxyData()->bEnableProxyStaticMeshRefinementByTimerOverride == bNewState)
						continue;

					CurCookable->Modify();
					CurCookable->SetEnableProxyStaticMeshRefinementByTimerOverride(bNewState);
					// Reset the timer
					CurCookable->ClearRefineMeshesTimer();
					// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
					CurCookable->SetRefineMeshesTimer();
				}
			})
		]
	];

	//
	// Proxy Mesh Auto Refine Timeout Seconds
	//
		
	// Lambdas for slider begin
	auto SliderBegin = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniProxyMeshTimerChange", "Houdini: Changing Proxy Mesh refinement Timer value"),
			Cookables[0]->GetOuter());

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;
			
			Cookables[Idx]->GetProxyData()->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEnd = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			// TODO: Mark changed or equivalent?
		}
	};

	// Lambdas for changing the parameter value
	auto ChangeFloatValueAt = [](const float& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniProxyMeshTimerChange", "Houdini: Changing Proxy Mesh refinement Timer value"),
			Cookables[0]->GetOuter() );

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			Cookables[Idx]->SetProxyMeshAutoRefineTimeoutSecondsOverride(Value);

			if (DoChange)
			{
				Cookables[Idx]->GetProxyData()->Modify();

				// Reset the timer
				Cookables[Idx]->ClearRefineMeshesTimer();
				// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
				Cookables[Idx]->SetRefineMeshesTimer();
			}
		}
	};

	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Proxy Mesh Auto-refine Timeout Seconds"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(0)
				.MaxValue(3600)

				.MinSliderValue(0)
				.MaxSliderValue(60)

				.Value_Lambda([MainCookable]() { return MainCookable->GetProxyMeshAutoRefineTimeoutSeconds(); })
				.OnValueChanged_Lambda([InCookables, ChangeFloatValueAt](float Val)
					{ 
						ChangeFloatValueAt(Val, false, InCookables); 
					})

				.OnValueCommitted_Lambda([InCookables, ChangeFloatValueAt](float Val, ETextCommit::Type TextCommitType)
					{	
						ChangeFloatValueAt(Val, true, InCookables);
					})
				.OnBeginSliderMovement_Lambda([InCookables, SliderBegin]()
					{
						SliderBegin(InCookables);
					})
				.OnEndSliderMovement_Lambda([InCookables, SliderEnd](const float NewValue)
					{ 
						SliderEnd(InCookables);
					})
				.SliderExponent(1.0f)
			]
		]
	];

	// Refine Proxy Static Mesh when saving a Map
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Auto-refine Proxy Meshes when saving a Map"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->GetProxyData()->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled_Lambda([MainCookable]() {return MainCookable->IsOverrideGlobalProxyStaticMeshSettings(); })
			.OnCheckStateChanged_Lambda([MainCookable, InCookables](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const bool bNewState = (NewState == ECheckBoxState::Checked);
				if (MainCookable->GetProxyData()->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniOverrideProxyRefineOnMapSave", "Houdini: Override Proxy Mesh Refine on Map Save"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					if (CurCookable->GetProxyData()->bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride == bNewState)
						continue;

					CurCookable->Modify();
					CurCookable->SetEnableProxyStaticMeshRefinementOnPreSaveWorldOverride(bNewState);
					// Reset the timer
					CurCookable->ClearRefineMeshesTimer();
					// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
					CurCookable->SetRefineMeshesTimer();
				}
			})
		]
	];

	// Refine Proxy Meshes on PIE
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Auto-refine Proxy Meshes when Playing-In-Editor."))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->GetProxyData()->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled_Lambda([MainCookable]() {return MainCookable->IsOverrideGlobalProxyStaticMeshSettings(); })
			.OnCheckStateChanged_Lambda([MainCookable, InCookables](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const bool bNewState = (NewState == ECheckBoxState::Checked);
				if (MainCookable->GetProxyData()->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniOverrideProxyRefineOnPIE", "Houdini: Override Proxy Mesh Refine on PIE"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					if (CurCookable->GetProxyData()->bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride == bNewState)
						continue;

					CurCookable->Modify();
					CurCookable->SetEnableProxyStaticMeshRefinementOnPreBeginPIEOverride(bNewState);
					// Reset the timer
					CurCookable->ClearRefineMeshesTimer();
					// SetRefineMeshesTimer will check the relevant settings and only set the timer if enabled via settings
					CurCookable->SetRefineMeshesTimer();
				}
			})
		]
	];
}



void 
FHoudiniCookableDetails::CreateMeshBuildSettingsDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables)
{
	if (InCookables.Num() <= 0)
		return;

	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (!MainCookable->IsOutputSupported())
		return;

	// Create the SM Build Settings category
	FString BuildSettingsCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_MESHGEN);

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	// OutputCatName += MultiSelectionIdentifier;
	
	IDetailCategoryBuilder& HouMeshGenCategory =
		DetailBuilder.EditCategory(*BuildSettingsCatName, FText::GetEmpty(), ECategoryPriority::Important);

	FString Label = TEXT("Static Mesh Build Settings");
	IDetailGroup& ProxyGrp = HouMeshGenCategory.AddGroup(FName(*Label), FText::FromString(Label));


	// Lambda to mark the cookable as changed	
	auto MarkCookableOutputsNeedUpdate = [](TWeakObjectPtr<UHoudiniCookable>& InCookable)
	{
		if (!IsValidWeakPointer(InCookable))
			return;

		// TODO: actually trigger an output update
		// Mark all outputs as changed?
		//InCookable->NeedUpdateOutputs();
	};

	// COPIED FROM FMeshBuildSettingsLayout
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("MeshBuildSettings", "Build Settings"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("RecomputeNormals", "Recompute Normals") )
		ProxyGrp.AddWidgetRow()
		.RowTag("RecomputeNormals")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeNormals", "Recompute Normals"))
		
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bRecomputeNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bRecomputeNormals == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bRecomputeNormals = bNewState;
					
					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("RecomputeTangents", "Recompute Tangents") )
		ProxyGrp.AddWidgetRow()
		.RowTag("RecomputeTangents")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RecomputeTangents", "Recompute Tangents"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bRecomputeTangents ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bRecomputeTangents == bNewState)
						continue;
					CurCookable->Modify();
					SMBS.bRecomputeTangents = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("UseMikkTSpace", "Use MikkTSpace Tangent Space") )
		ProxyGrp.AddWidgetRow()
		.RowTag("UseMikkTSpace")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseMikkTSpace", "Use MikkTSpace Tangent Space"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bUseMikkTSpace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseMikkTSpace == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bUseMikkTSpace = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("ComputeWeightedNormals", "Compute Weighted Normals") )
		ProxyGrp.AddWidgetRow()
		.RowTag("ComputeWeightedNormals")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("ComputeWeightedNormals", "Compute Weighted Normals"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bComputeWeightedNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;
				
					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bComputeWeightedNormals == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bComputeWeightedNormals = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("RemoveDegenerates", "Remove Degenerates") )
		ProxyGrp.AddWidgetRow()
		.RowTag("RemoveDegenerates")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("RemoveDegenerates", "Remove Degenerates"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			//.IsEnabled(this, &FMeshBuildSettingsLayout::IsRemoveDegeneratesDisabled)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bRemoveDegenerates ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;
				
					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bRemoveDegenerates == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bRemoveDegenerates = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("BuildReversedIndexBuffer", "Build Reversed Index Buffer") )
		ProxyGrp.AddWidgetRow()
		.RowTag("BuildReversedIndexBuffer")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("BuildReversedIndexBuffer", "Build Reversed Index Buffer"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bBuildReversedIndexBuffer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bBuildReversedIndexBuffer == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bBuildReversedIndexBuffer = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow(LOCTEXT("UseHighPrecisionTangentBasis", "Use High Precision Tangent Basis"))
		ProxyGrp.AddWidgetRow()
		.RowTag("UseHighPrecisionTangentBasis")
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("UseHighPrecisionTangentBasis", "Use High Precision Tangent Basis"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bUseHighPrecisionTangentBasis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseHighPrecisionTangentBasis == bNewState)
						continue;
					CurCookable->Modify();
					SMBS.bUseHighPrecisionTangentBasis = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("UseFullPrecisionUVs", "Use Full Precision UVs") )
		ProxyGrp.AddWidgetRow()
		.RowTag("UseFullPrecisionUVs")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseFullPrecisionUVs", "Use Full Precision UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bUseFullPrecisionUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseFullPrecisionUVs == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bUseFullPrecisionUVs = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}
	
	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("UseBackwardsCompatibleF16TruncUVs", "UE4 Compatible UVs") )
		ProxyGrp.AddWidgetRow()
		.RowTag("UseBackwardsCompatibleF16TruncUVs")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("UseBackwardsCompatibleF16TruncUVs", "UE4 Compatible UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bUseBackwardsCompatibleF16TruncUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseBackwardsCompatibleF16TruncUVs == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bUseBackwardsCompatibleF16TruncUVs = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("GenerateLightmapUVs", "Generate Lightmap UVs") )
		ProxyGrp.AddWidgetRow()
		.RowTag("GenerateLightmapUVs")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("GenerateLightmapUVs", "Generate Lightmap UVs"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bGenerateLightmapUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;
			
					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bGenerateLightmapUVs == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bGenerateLightmapUVs = bNewState;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("MinLightmapResolution", "Min Lightmap Resolution") )
		ProxyGrp.AddWidgetRow()
		.RowTag("MinLightmapResolution")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("MinLightmapResolution", "Min Lightmap Resolution"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(1)
			.MaxValue(2048)
			.Value_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().MinLightmapResolution;
			})
			.OnValueChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](int32 NewValue)
			{
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.MinLightmapResolution == NewValue)
						continue;

					CurCookable->Modify();
					SMBS.MinLightmapResolution = NewValue;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("SourceLightmapIndex", "Source Lightmap Index") )
		ProxyGrp.AddWidgetRow()
		.RowTag("SourceLightmapIndex")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("SourceLightmapIndex", "Source Lightmap Index"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0)
			.MaxValue(7)
			.Value_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().SrcLightmapIndex;
			})
			.OnValueChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](int32 NewValue)
			{
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.SrcLightmapIndex == NewValue)
						continue;

					CurCookable->Modify();
					SMBS.SrcLightmapIndex = NewValue;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("DestinationLightmapIndex", "Destination Lightmap Index") )
		ProxyGrp.AddWidgetRow()
		.RowTag("DestinationLightmapIndex")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DestinationLightmapIndex", "Destination Lightmap Index"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0)
			.MaxValue(7)
			.Value_Lambda([MainCookable]()
			{	
				return MainCookable->GetStaticMeshBuildSettings().DstLightmapIndex;
			})
			.OnValueChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](int32 NewValue)
			{
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.DstLightmapIndex == NewValue)
						continue;

					CurCookable->Modify();
					SMBS.DstLightmapIndex = NewValue;

					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		// Lambda update BuildScale	
		auto OnBuildScaleChange = [InCookables, MarkCookableOutputsNeedUpdate](float NewValue, int8 XYZ)
		{
			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
				switch (XYZ)
				{
					case 0:
						SMBS.BuildScale3D.X = NewValue;
						break;
					case 1:
						SMBS.BuildScale3D.Y = NewValue;
						break;
					case 2:
						SMBS.BuildScale3D.Z = NewValue;
						break;
					default:
						break;
				}
				CurCookable->Modify();
				// Mark our outputs as needing an update
				MarkCookableOutputsNeedUpdate(CurCookable);
			}
		};

		//HouMeshGenCategory.AddCustomRow(LOCTEXT("BuildScale", "Build Scale"))
		ProxyGrp.AddWidgetRow()
		.RowTag("BuildScale")
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("BuildScale", "Build Scale"))
			.ToolTipText( LOCTEXT("BuildScale_ToolTip", "The local scale applied when building the mesh") )
		]
		.ValueContent()
		.MinDesiredWidth(125.0f * 3.0f)
		.MaxDesiredWidth(125.0f * 3.0f)
		[
			SNew(SVectorInputBox)
			.X_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().BuildScale3D.X; })
			.Y_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().BuildScale3D.Y; })
			.Z_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().BuildScale3D.Z; })
			.bColorAxisLabels(false)
			.AllowSpin(false)
			.OnXCommitted_Lambda([InCookables, OnBuildScaleChange, MarkCookableOutputsNeedUpdate](float NewValue, ETextCommit::Type TextCommitType)
				{ OnBuildScaleChange(NewValue, 0); })
			.OnYCommitted_Lambda([InCookables, OnBuildScaleChange, MarkCookableOutputsNeedUpdate](float NewValue, ETextCommit::Type TextCommitType)
				{ OnBuildScaleChange(NewValue, 1); })
			.OnZCommitted_Lambda([InCookables, OnBuildScaleChange, MarkCookableOutputsNeedUpdate](float NewValue, ETextCommit::Type TextCommitType)
				{ OnBuildScaleChange(NewValue, 2); })
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
	}

	{
		auto OnDistanceFieldResChanged = [InCookables, MarkCookableOutputsNeedUpdate](float NewValue, bool bIsCommit)
		{
			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
				if (SMBS.DistanceFieldResolutionScale == NewValue)
					continue;								
				SMBS.DistanceFieldResolutionScale = NewValue;
				if (bIsCommit)
				{
					CurCookable->Modify();
					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			}
		};

		//HouMeshGenCategory.AddCustomRow( LOCTEXT("DistanceFieldResolutionScale", "Distance Field Resolution Scale") )
		ProxyGrp.AddWidgetRow()
		.RowTag("DistanceFieldResolutionScale")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DistanceFieldResolutionScale", "Distance Field Resolution Scale"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.MinValue(0.0f)
			.MaxValue(100.0f)
			.Value_Lambda([MainCookable]()
			{	
				return MainCookable->GetStaticMeshBuildSettings().DistanceFieldResolutionScale;
			})
			.OnValueChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate, OnDistanceFieldResChanged](float NewValue)
				{ OnDistanceFieldResChanged(NewValue, false); })
			.OnValueCommitted_Lambda([InCookables, MarkCookableOutputsNeedUpdate, OnDistanceFieldResChanged](float NewValue, ETextCommit::Type TextCommitType)
				{ OnDistanceFieldResChanged(NewValue, true); })
		];
	}

	{
		//HouMeshGenCategory.AddCustomRow( LOCTEXT("GenerateDistanceFieldAsIfTwoSided", "Two-Sided Distance Field Generation") )
		ProxyGrp.AddWidgetRow()
		.RowTag("GenerateDistanceFieldAsIfTwoSided")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("GenerateDistanceFieldAsIfTwoSided", "Two-Sided Distance Field Generation"))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{		
				return MainCookable->GetStaticMeshBuildSettings().bGenerateDistanceFieldAsIfTwoSided ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](ECheckBoxState NewState)
			{
				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bGenerateDistanceFieldAsIfTwoSided == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bGenerateDistanceFieldAsIfTwoSided = bNewState;
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			})
		];
	}

	{
		TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
		.AllowedClass(UStaticMesh::StaticClass())
		.AllowClear(true)
		.ObjectPath_Lambda([MainCookable]()
		{
			if (MainCookable->GetStaticMeshBuildSettings().DistanceFieldReplacementMesh)
				return MainCookable->GetStaticMeshBuildSettings().DistanceFieldReplacementMesh->GetPathName();
			else
				return FString("");
		})
		.OnObjectChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate](const FAssetData& InAssetData)
		{
			UStaticMesh* NewObject = Cast<UStaticMesh>(InAssetData.GetAsset());
			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
				if (SMBS.DistanceFieldReplacementMesh == NewObject)
					continue;

				CurCookable->Modify();
				SMBS.DistanceFieldReplacementMesh = NewObject;
				MarkCookableOutputsNeedUpdate(CurCookable);
			}
		});

		//HouMeshGenCategory.AddCustomRow( LOCTEXT("DistanceFieldReplacementMesh", "Distance Field Replacement Mesh") )
		ProxyGrp.AddWidgetRow()
		.RowTag("DistanceFieldReplacementMesh")
		.NameContent()
		[
			SNew(STextBlock)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text(LOCTEXT("DistanceFieldReplacementMesh", "Distance Field Replacement Mesh"))
		]
		.ValueContent()
		[
			PropWidget
		];
	}

	{
		auto OnMaxLumenMeshCardsChanged = [InCookables, MarkCookableOutputsNeedUpdate](int32 NewValue, bool bIsCommit)
		{
			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
				if (SMBS.MaxLumenMeshCards == NewValue)
					continue;								
				SMBS.MaxLumenMeshCards = NewValue;
				if (bIsCommit)
				{
					CurCookable->Modify();
					// Mark our outputs as needing an update
					MarkCookableOutputsNeedUpdate(CurCookable);
				}
			}
		};

		//HouMeshGenCategory.AddCustomRow(LOCTEXT("MaxLumenMeshCards", "Max Lumen Mesh Cards"))
		ProxyGrp.AddWidgetRow()
		.RowTag("MaxLumenMeshCards")
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("MaxLumenMeshCards", "Max Lumen Mesh Cards"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<int32>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.MinValue(0)
			.MaxValue(32)
			.Value_Lambda([MainCookable]()
			{	
				return MainCookable->GetStaticMeshBuildSettings().MaxLumenMeshCards;
			})
			.OnValueChanged_Lambda([InCookables, MarkCookableOutputsNeedUpdate, OnMaxLumenMeshCardsChanged](int32 NewValue)
				{ OnMaxLumenMeshCardsChanged(NewValue, false); })
			.OnValueCommitted_Lambda([InCookables, MarkCookableOutputsNeedUpdate, OnMaxLumenMeshCardsChanged](int32 NewValue, ETextCommit::Type TextCommitType)
				{ OnMaxLumenMeshCardsChanged(NewValue, true); })
		];
	}
	
	/*
	// Lambda used to trigger a n output update if necessary
	auto MarkOutputUpdateNeeded = [InCookables]()
	{
		for (auto CurCookable : InCookables)
		{
			if (!IsValidWeakPointer(CurCookable))
				continue;

			// TODO
			//CurCookable->Need
		}
	};
	
	//
	// UseFullPrecisionUVs
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Use Full Precision UVs"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				
				return MainCookable->GetStaticMeshBuildSettings().bUseFullPrecisionUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bUseFullPrecisionUVs == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniSMBSFullPrecUVs", "Houdini Static Mesh Build Settings: Changed bUseFullPrecisionUVs"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseFullPrecisionUVs == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bUseFullPrecisionUVs = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];

	//
	// SrcLightmapIndex
	//
	// Lambdas for slider begin
	auto SliderBeginSrcLightmapIndex = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeSrcLightmapIndex", "Houdini Static Mesh Build Settings: Changed SrcLightmapIndex"),
			Cookables[0]->GetOuter());

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;
			
			Cookables[Idx]->GetProxyData()->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEndSrcLightmapIndex = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			// TODO: Mark changed or equivalent?
		}
	};

	// Lambdas for changing the value
	auto ChangeSrcLightmapIndex = [](const int32& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeSrcLightmapIndex", "Houdini Static Mesh Build Settings: Changed SrcLightmapIndex"),
			Cookables[0]->GetOuter() );

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			FMeshBuildSettings& SMBS = Cookables[Idx]->GetStaticMeshBuildSettings();
			if (SMBS.SrcLightmapIndex == Value)
				continue;

			Cookables[Idx]->Modify();
			SMBS.SrcLightmapIndex = Value;

			if (DoChange)
			{
				Cookables[Idx]->GetProxyData()->Modify();
			}
		}
	};

	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Source Lightmap Index"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(true)

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(0)
				.MaxValue(3600)

				.MinSliderValue(0)
				.MaxSliderValue(60)

				.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().SrcLightmapIndex; })
				.OnValueChanged_Lambda([InCookables, ChangeSrcLightmapIndex](int32 Val)
				{ 
					ChangeSrcLightmapIndex(Val, false, InCookables);
				})
				.OnValueCommitted_Lambda([InCookables, ChangeSrcLightmapIndex](int32 Val, ETextCommit::Type TextCommitType)
				{	
					ChangeSrcLightmapIndex(Val, true, InCookables);
				})
				.OnBeginSliderMovement_Lambda([InCookables, SliderBeginSrcLightmapIndex]()
				{
					SliderBeginSrcLightmapIndex(InCookables);
				})
				.OnEndSliderMovement_Lambda([InCookables, SliderEndSrcLightmapIndex](const int32 NewValue)
				{ 
					SliderEndSrcLightmapIndex(InCookables);
				})
				.SliderExponent(1.0f)
			]
		]
	];



	//
	// DstLightmapIndex
	//
	
	// Lambdas for slider begin
	auto SliderBeginDstLightmapIndex = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeDstLightmapIndex", "Houdini Static Mesh Build Settings: Changed DstLightmapIndex"),
			Cookables[0]->GetOuter());

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;
			
			Cookables[Idx]->GetProxyData()->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEndDstLightmapIndex = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			// TODO: Mark changed or equivalent?
		}
	};

	// Lambdas for changing the value
	auto ChangeDstLightmapIndex = [](const int32& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeDstLightmapIndex", "Houdini Static Mesh Build Settings: Changed DstLightmapIndex"),
			Cookables[0]->GetOuter() );

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			FMeshBuildSettings& SMBS = Cookables[Idx]->GetStaticMeshBuildSettings();
			if (SMBS.DstLightmapIndex == Value)
				continue;

			Cookables[Idx]->Modify();
			SMBS.DstLightmapIndex = Value;

			if (DoChange)
			{
				Cookables[Idx]->GetProxyData()->Modify();
			}
		}
	};

	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Destination Lightmap Index"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(true)

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(0)
				.MaxValue(3600)

				.MinSliderValue(0)
				.MaxSliderValue(60)

				.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().DstLightmapIndex; })
				.OnValueChanged_Lambda([InCookables, ChangeDstLightmapIndex](int32 Val)
				{ 
					ChangeDstLightmapIndex(Val, false, InCookables);
				})
				.OnValueCommitted_Lambda([InCookables, ChangeDstLightmapIndex](int32 Val, ETextCommit::Type TextCommitType)
				{	
					ChangeDstLightmapIndex(Val, true, InCookables);
				})
				.OnBeginSliderMovement_Lambda([InCookables, SliderBeginDstLightmapIndex]()
				{
						SliderBeginDstLightmapIndex(InCookables);
				})
				.OnEndSliderMovement_Lambda([InCookables, SliderEndDstLightmapIndex](const int32 NewValue)
				{ 
					SliderEndDstLightmapIndex(InCookables);
				})
				.SliderExponent(1.0f)
			]
		]
	];




	//
	// MinLightmapResolution
	//

	// Lambdas for slider begin
	auto SliderBeginMinLightmapResolution = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeMinLightmapResolution", "Houdini Static Mesh Build Settings: Changed MinLightmapResolution"),
			Cookables[0]->GetOuter());

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;
			
			Cookables[Idx]->GetProxyData()->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEndMinLightmapResolution = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			// TODO: Mark changed or equivalent?
		}
	};

	// Lambdas for changing the value
	auto ChangeMinLightmapResolution = [](const int32& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeMinLightmapResolution", "Houdini Static Mesh Build Settings: Changed MinLightmapResolution"),
			Cookables[0]->GetOuter() );

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			FMeshBuildSettings& SMBS = Cookables[Idx]->GetStaticMeshBuildSettings();
			if (SMBS.MinLightmapResolution == Value)
				continue;

			Cookables[Idx]->Modify();
			SMBS.MinLightmapResolution = Value;

			if (DoChange)
			{
				Cookables[Idx]->GetProxyData()->Modify();
			}
		}
	};

	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Minimum Lightmap Resolution"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(true)

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(0)
				.MaxValue(3600)

				.MinSliderValue(0)
				.MaxSliderValue(60)

				.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().MinLightmapResolution; })
				.OnValueChanged_Lambda([InCookables, ChangeMinLightmapResolution](int32 Val)
				{ 
					ChangeMinLightmapResolution(Val, false, InCookables);
				})
				.OnValueCommitted_Lambda([InCookables, ChangeMinLightmapResolution](int32 Val, ETextCommit::Type TextCommitType)
				{	
					ChangeMinLightmapResolution(Val, true, InCookables);
				})
				.OnBeginSliderMovement_Lambda([InCookables, SliderBeginMinLightmapResolution]()
				{
						SliderBeginMinLightmapResolution(InCookables);
				})
				.OnEndSliderMovement_Lambda([InCookables, SliderEndMinLightmapResolution](const int32 NewValue)
				{ 
						SliderEndMinLightmapResolution(InCookables);
				})
				.SliderExponent(1.0f)
			]
		]
	];



	//
	// RemoveDegenerates
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Remove Degenerates"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;
				
				return MainCookable->GetStaticMeshBuildSettings().bRemoveDegenerates ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bRemoveDegenerates == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSRemoveDegen", "Houdini Static Mesh Build Settings: Changed bRemoveDegenerates"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bRemoveDegenerates == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bRemoveDegenerates = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];



	//
	// bUseMikkTSpace
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Use MikkT Space"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;
				
				return MainCookable->GetStaticMeshBuildSettings().bUseMikkTSpace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bUseMikkTSpace == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSbUseMikkTSpace", "Houdini Static Mesh Build Settings: Changed bUseMikkTSpace"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseMikkTSpace == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bUseMikkTSpace = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];

	

	//
	// 	bComputeWeightedNormals;
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Compute Weighted Normals"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;
				
				return MainCookable->GetStaticMeshBuildSettings().bComputeWeightedNormals ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bComputeWeightedNormals == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSbComputeWeightedNormals", "Houdini Static Mesh Build Settings: Changed bComputeWeightedNormals"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bComputeWeightedNormals == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bComputeWeightedNormals = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];



	//
	// 	bBuildReversedIndexBuffer;
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Build Reversed Index Buffer"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;
				
				return MainCookable->GetStaticMeshBuildSettings().bBuildReversedIndexBuffer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bBuildReversedIndexBuffer == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSbBuildReversedIndexBuffer", "Houdini Static Mesh Build Settings: Changed bBuildReversedIndexBuffer"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bBuildReversedIndexBuffer == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bBuildReversedIndexBuffer = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];



	//
	// 	bUseHighPrecisionTangentBasis;
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Use High Precision Tangent Basis"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;
				
				return MainCookable->GetStaticMeshBuildSettings().bUseHighPrecisionTangentBasis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bUseHighPrecisionTangentBasis == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSbUseHighPrecisionTangentBasis", "Houdini Static Mesh Build Settings: Changed bUseHighPrecisionTangentBasis"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bUseHighPrecisionTangentBasis == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bUseHighPrecisionTangentBasis = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];




	//
	// 	DistanceFieldResolutionScale;
	//

	// Lambdas for slider begin
	auto SliderBeginDistanceFieldResolutionScale = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeDistanceFieldResolutionScale", "Houdini Static Mesh Build Settings: Changed DistanceFieldResolutionScale"),
			Cookables[0]->GetOuter());

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;
			
			Cookables[Idx]->GetProxyData()->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEndDistanceFieldResolutionScale = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			// TODO: Mark changed or equivalent?
		}
	};

	// Lambdas for changing the value
	auto ChangeDistanceFieldResolutionScale = [](const float& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
	{
		if (Cookables.Num() == 0)
			return;

		if (!IsValidWeakPointer(Cookables[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniChangeDistanceFieldResolutionScale", "Houdini Static Mesh Build Settings: Changed DistanceFieldResolutionScale"),
			Cookables[0]->GetOuter() );

		for (int Idx = 0; Idx < Cookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(Cookables[Idx]))
				continue;

			FMeshBuildSettings& SMBS = Cookables[Idx]->GetStaticMeshBuildSettings();
			if (SMBS.DistanceFieldResolutionScale == Value)
				continue;

			Cookables[Idx]->Modify();
			SMBS.DistanceFieldResolutionScale = Value;

			if (DoChange)
			{
				Cookables[Idx]->GetProxyData()->Modify();
			}
		}
	};

	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Distance Field Resolution Scale"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(0)
				.MaxValue(3600)

				.MinSliderValue(0)
				.MaxSliderValue(60)

				.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshBuildSettings().DistanceFieldResolutionScale; })
				.OnValueChanged_Lambda([InCookables, ChangeDistanceFieldResolutionScale](float Val)
				{ 
					ChangeDistanceFieldResolutionScale(Val, false, InCookables);
				})
				.OnValueCommitted_Lambda([InCookables, ChangeDistanceFieldResolutionScale](float Val, ETextCommit::Type TextCommitType)
				{	
					ChangeDistanceFieldResolutionScale(Val, true, InCookables);
				})
				.OnBeginSliderMovement_Lambda([InCookables, SliderBeginDistanceFieldResolutionScale]()
				{
					SliderBeginDistanceFieldResolutionScale(InCookables);
				})
				.OnEndSliderMovement_Lambda([InCookables, SliderEndDistanceFieldResolutionScale](const float NewValue)
				{ 
					SliderEndDistanceFieldResolutionScale(InCookables);
				})
				.SliderExponent(1.0f)
			]
		]
	];



	//
	// 	bGenerateDistanceFieldAsIfTwoSided;
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Generate Distance Field As If TwoSided"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->GetStaticMeshBuildSettings().bGenerateDistanceFieldAsIfTwoSided ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bGenerateDistanceFieldAsIfTwoSided == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSbGenerateDistanceFieldAsIfTwoSided", "Houdini Static Mesh Build Settings: Changed bGenerateDistanceFieldAsIfTwoSided"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bGenerateDistanceFieldAsIfTwoSided == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bGenerateDistanceFieldAsIfTwoSided = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];

	//
	// 	bSupportFaceRemap;
	//
	ProxyGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
			.Text(FText::FromString("Support Face Remap"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([MainCookable]()
			{
				if (!IsValidWeakPointer(MainCookable))
					return ECheckBoxState::Unchecked;

				return MainCookable->GetStaticMeshBuildSettings().bSupportFaceRemap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainCookable))
					return;

				const uint8 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
				if (MainCookable->GetStaticMeshBuildSettings().bSupportFaceRemap == bNewState)
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniChangeSMBSbSupportFaceRemap", "Houdini Static Mesh Build Settings: Changed bSupportFaceRemap"),
					MainCookable->GetOuter());

				for (auto CurCookable : InCookables)
				{
					if (!IsValidWeakPointer(CurCookable))
						continue;

					FMeshBuildSettings& SMBS = CurCookable->GetStaticMeshBuildSettings();
					if (SMBS.bSupportFaceRemap == bNewState)
						continue;

					CurCookable->Modify();
					SMBS.bSupportFaceRemap = bNewState;
				}

				// Mark our outputs as needing an update
				MarkOutputUpdateNeeded();
			})
		]
	];
	*/
}


void
FHoudiniCookableDetails::CreateMeshGenerationDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables)
{
	if (InCookables.Num() <= 0)
		return;

	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (!MainCookable->IsOutputSupported())
		return;

	// Create the Mesh Generation category
	FString BuildSettingsCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_MESHGEN);

	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	// OutputCatName += MultiSelectionIdentifier;
	
	IDetailCategoryBuilder& HouMeshGenCategory =
		DetailBuilder.EditCategory(*BuildSettingsCatName, FText::GetEmpty(), ECategoryPriority::Important);

	FString Label = TEXT("Static Mesh Generation Properties");
	IDetailGroup& ProxyGrp = HouMeshGenCategory.AddGroup(FName(*Label), FText::FromString(Label));

	// Lambda used to trigger a n output update if necessary
	auto MarkOutputUpdateNeeded = [InCookables]()
	{
		for (auto CurCookable : InCookables)
		{
			if (!IsValidWeakPointer(CurCookable))
				continue;
		}
	};

	//
	// bDoubleSidedGeometry
	//
	{
		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Double Sided Geometry"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([MainCookable]()
				{
					if (!IsValidWeakPointer(MainCookable))
						return ECheckBoxState::Unchecked;				
					return MainCookable->GetStaticMeshGenerationProperties().bGeneratedDoubleSidedGeometry ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
				{
					if (!IsValidWeakPointer(MainCookable))
						return;
	
					const uint32 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
					if (MainCookable->GetStaticMeshGenerationProperties().bGeneratedDoubleSidedGeometry == bNewState)
						return;
	
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniSMGPFullPrecUVs", "Houdini Static Mesh Generation Properties: Changed bUseFullPrecisionUVs"),
						MainCookable->GetOuter());
	
					for (auto CurCookable : InCookables)
					{
						if (!IsValidWeakPointer(CurCookable))
							continue;
	
						FHoudiniStaticMeshGenerationProperties& SMGP = CurCookable->GetStaticMeshGenerationProperties();
						if (SMGP.bGeneratedDoubleSidedGeometry == bNewState)
							continue;
	
						CurCookable->Modify();
						SMGP.bGeneratedDoubleSidedGeometry = bNewState;
					}
	
					// Mark our outputs as needing an update
					MarkOutputUpdateNeeded();
				})
			]
		];
	}


	//
	// PhysMaterial
	//

	// Create thumbnail for this HDA.
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool = HouMeshGenCategory.GetParentLayout().GetThumbnailPool();
	{
		// Lambda for updating the Physical Material
		auto UpdatePhysMat = [MainCookable](const TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables, UObject* InObject)
		{
			if (!IsValidWeakPointer(MainCookable))
				return;

			if (!InObject->IsA<UPhysicalMaterial>())
				return;

			UPhysicalMaterial* NewPhysMat = Cast<UPhysicalMaterial>(InObject);		
			if (!IsValid(NewPhysMat))
				return;

			// TODO: Transaction on all cookable?
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniSMGPPhysMaterial", "Houdini Static Mesh Generation Properties: Changed PhysMaterial"),
				MainCookable->GetOuter());

			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				if (!CurCookable->IsOutputSupported())
					continue;

				FHoudiniStaticMeshGenerationProperties& SMGP = CurCookable->GetStaticMeshGenerationProperties();
				if (SMGP.GeneratedPhysMaterial == NewPhysMat)
					continue;

				CurCookable->Modify();
				SMGP.GeneratedPhysMaterial = NewPhysMat;
			}
		};

		ProxyGrp.AddWidgetRow()
			.NameContent()
			[
				SNew(STextBlock)
					.Text(FText::FromString("Simple Collision Physical Material"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(2, 2, 5, 2)
					.AutoHeight()
					.Padding(0, 5, 0, 0)
					.AutoHeight()
					[
						SNew(SObjectPropertyEntryBox)
							.ObjectPath_Lambda([MainCookable]()
								{
									if (!IsValidWeakPointer(MainCookable))
										return FString();

									UPhysicalMaterial* PhysMat = MainCookable->GetStaticMeshGenerationProperties().GeneratedPhysMaterial;
									if (!IsValid(PhysMat))
										return FString();

									return PhysMat->GetPathName();
								})
							.AllowedClass(UPhysicalMaterial::StaticClass())
							.OnObjectChanged_Lambda([InCookables, UpdatePhysMat](const FAssetData& InAssetData)
								{
									UPhysicalMaterial* PhysMat = Cast<UPhysicalMaterial>(InAssetData.GetAsset());
									if (IsValid(PhysMat))
										UpdatePhysMat(InCookables, PhysMat);
								})
							.AllowCreate(false)
							.AllowClear(true)
							.DisplayUseSelected(true)
							.DisplayBrowse(true)
							.DisplayThumbnail(true)
							.ThumbnailPool(ThumbnailPool)
							.NewAssetFactories(TArray<UFactory*>())
					]
			];
	}

	//
	// DefaultBodyInstance
	// 

	/*
	// Default properties of the body instance
	//UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (FullyExpand = "true"))
	//struct FBodyInstance DefaultBodyInstance;

	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	{
		for (int Idx = 0; Idx < InCookables.Num(); Idx++)
		{
			if (!IsValidWeakPointer(InCookables[Idx]))
				continue;

			FHoudiniStaticMeshGenerationProperties& SMGP = InCookables[Idx]->GetStaticMeshGenerationProperties();
			SMGP.DefaultBodyInstance.
			InCookables[Idx]->GetOutputData()->Modify();
		}
	}
	TSharedPtr<class FComponentMaterialCategory> MaterialCategory;
	TSharedPtr<class FBodyInstanceCustomizationHelper> BodyInstanceCustomizationHelper;

	BodyInstanceCustomizationHelper = MakeShareable(new FBodyInstanceCustomizationHelper(ObjectsCustomized));
	BodyInstanceCustomizationHelper->CustomizeDetails(DetailBuilder, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, BodyInstance)));

	*/


	//
	// CollisionTraceFlag - TODO
	// 
	/// Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate for new Houdini Assets.
	//UPROPERTY(GlobalConfig, VisibleDefaultsOnly, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Collision Complexity"))
	//TEnumAsByte<enum ECollisionTraceFlag> CollisionTraceFlag;

	//
	// LightMapResolution
	//

	{
		// Resolution of lightmap for baked lighting.
		//UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Light Map Resolution", FixedIncrement = "4.0"))
		//int32 LightMapResolution;

		// Lambdas for slider begin
		auto SliderBeginLightMapResolution = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			if (Cookables.Num() == 0)
				return;

			if (!IsValidWeakPointer(Cookables[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniChangeLightMapResolution", "Houdini Static Mesh Generation Properties: Changed Light Map Resolution"),
				Cookables[0]->GetOuter());

			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;
			
				Cookables[Idx]->GetOutputData()->Modify();
			}
		};

		// Lambdas for slider end
		auto SliderEndLightMapResolution = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			// Mark the value as changed to trigger an update
			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;

				// TODO: Mark changed or equivalent?
			}
		};

		// Lambdas for changing the value
		auto ChangeLightMapResolution = [](const int32& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			if (Cookables.Num() == 0)
				return;

			if (!IsValidWeakPointer(Cookables[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniChangeLightMapResolution", "Houdini Static Mesh Generation Properties: Changed LightMapResolution"),
				Cookables[0]->GetOuter());

			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;

				FHoudiniStaticMeshGenerationProperties& SMGP = Cookables[Idx]->GetStaticMeshGenerationProperties();
				if (SMGP.GeneratedLightMapResolution == Value)
					continue;

				Cookables[Idx]->Modify();
				SMGP.GeneratedLightMapResolution = Value;

				if (DoChange)
				{
					Cookables[Idx]->GetProxyData()->Modify();
				}
			}
		};

		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
				.Text(FText::FromString("Light Map Resolution"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)

					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

					.MinValue(0)
					.MaxValue(3600)

					.MinSliderValue(0)
					.MaxSliderValue(60)
						
					.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshGenerationProperties().GeneratedLightMapResolution; })
					.OnValueChanged_Lambda([InCookables, ChangeLightMapResolution](int32 Val)
					{
						ChangeLightMapResolution(Val, false, InCookables);
					})
					.OnValueCommitted_Lambda([InCookables, ChangeLightMapResolution](int32 Val, ETextCommit::Type TextCommitType)
					{
						ChangeLightMapResolution(Val, true, InCookables);
					})
					.OnBeginSliderMovement_Lambda([InCookables, SliderBeginLightMapResolution]()
					{
						SliderBeginLightMapResolution(InCookables);
					})
					.OnEndSliderMovement_Lambda([InCookables, SliderEndLightMapResolution](const int32 NewValue)
					{
						SliderEndLightMapResolution(InCookables);
					})
					.SliderExponent(4.0f)
				]
			]
		];
	}


	/// Default Mesh distance field resolution, setting it to 0 will prevent the mesh distance field generation while editing the asset
	//UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Distance Field Resolution Scale", UIMin = "0.0", UIMax = "100.0"))
	//float GeneratedDistanceFieldResolutionScale;

	/// Custom walkable slope setting for bodies of new Houdini Assets.
	//UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Walkable Slope Override"))
	//FWalkableSlopeOverride WalkableSlopeOverride;

	//
	// LightMapCoordinateIndex
	//

	{
		/// The UV coordinate index of lightmap 
		//UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Light map coordinate index"))
		//int32 LightMapCoordinateIndex;

		// Lambdas for slider begin
		auto SliderBeginLightMapCoordinateIndex = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			if (Cookables.Num() == 0)
				return;

			if (!IsValidWeakPointer(Cookables[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniChangeLightMapCoordinateIndex", "Houdini Static Mesh Generation Properties: Changed LightMapCoordinateIndex"),
				Cookables[0]->GetOuter());

			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;
			
				Cookables[Idx]->GetProxyData()->Modify();
			}
		};

		// Lambdas for slider end
		auto SliderEndLightMapCoordinateIndex = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			// Mark the value as changed to trigger an update
			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;

				// TODO: Mark changed or equivalent?
			}
		};

		// Lambdas for changing the value
		auto ChangeLightMapCoordinateIndex = [](const int32& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			if (Cookables.Num() == 0)
				return;

			if (!IsValidWeakPointer(Cookables[0]))
				return;
		
			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniChangeLightMapCoordinateIndex", "Houdini Static Mesh Generation Properties: Changed LightMapCoordinateIndex"),
				Cookables[0]->GetOuter() );

			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;

				FHoudiniStaticMeshGenerationProperties& SMGP = Cookables[Idx]->GetStaticMeshGenerationProperties();
				if (SMGP.GeneratedLightMapCoordinateIndex == Value)
					continue;

				Cookables[Idx]->Modify();
				SMGP.GeneratedLightMapCoordinateIndex = Value;

				if (DoChange)
				{
					Cookables[Idx]->GetProxyData()->Modify();
				}
			}
		};

		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Light map coordinate index"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.AllowSpin(true)

					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

					.MinValue(0)
					.MaxValue(3600)

					.MinSliderValue(0)
					.MaxSliderValue(60)

					.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshGenerationProperties().GeneratedLightMapCoordinateIndex; })
					.OnValueChanged_Lambda([InCookables, ChangeLightMapCoordinateIndex](int32 Val)
					{ 
						ChangeLightMapCoordinateIndex(Val, false, InCookables);
					})
					.OnValueCommitted_Lambda([InCookables, ChangeLightMapCoordinateIndex](int32 Val, ETextCommit::Type TextCommitType)
					{	
						ChangeLightMapCoordinateIndex(Val, true, InCookables);
					})
					.OnBeginSliderMovement_Lambda([InCookables, SliderBeginLightMapCoordinateIndex]()
					{
						SliderBeginLightMapCoordinateIndex(InCookables);
					})
					.OnEndSliderMovement_Lambda([InCookables, SliderEndLightMapCoordinateIndex](const int32 NewValue)
					{ 
						SliderEndLightMapCoordinateIndex(InCookables);
					})
					.SliderExponent(4.0f)
				]
			]
		];
	}


	//
	// bUseMaximumStreamingTexelRatio
	//

	/*
	{	
		// True if mesh should use a less-conservative method of mip LOD texture factor computation for new Houdini Assets.
		//UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Use Maximum Streaming Texel Ratio"))
		//uint32 bUseMaximumStreamingTexelRatio : 1;

		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Use Maximum Streaming Texel Ratio"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([MainCookable]()
				{
					if (!IsValidWeakPointer(MainCookable))
						return ECheckBoxState::Unchecked;

				
					return MainCookable->GetStaticMeshGenerationProperties().bGeneratedUseMaximumStreamingTexelRatio ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([MainCookable, InCookables, MarkOutputUpdateNeeded](ECheckBoxState NewState)
				{
					if (!IsValidWeakPointer(MainCookable))
						return;

					const uint32 bNewState = (NewState == ECheckBoxState::Checked) ? 1 : 0;
					if (MainCookable->GetStaticMeshGenerationProperties().bGeneratedUseMaximumStreamingTexelRatio == bNewState)
						return;

					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniChangedUseMaximumStreamingTexelRatio", "Houdini Static Mesh Build Settings: Changed bGeneratedUseMaximumStreamingTexelRatio"),
						MainCookable->GetOuter());

					for (auto CurCookable : InCookables)
					{
						if (!IsValidWeakPointer(CurCookable))
							continue;

						FHoudiniStaticMeshGenerationProperties& SMGP = CurCookable->GetStaticMeshGenerationProperties();
						if (SMGP.bGeneratedUseMaximumStreamingTexelRatio == bNewState)
							continue;

						CurCookable->Modify();
						SMGP.bGeneratedUseMaximumStreamingTexelRatio = bNewState;
					}

					// Mark our outputs as needing an update
					MarkOutputUpdateNeeded();
				})
			]
		];
	}
	*/

	//
	// StreamingDistanceMultiplier
	//
	/*
	{
		
		/// Allows artists to adjust the distance where textures using UV 0 are streamed in/out for new Houdini Assets.
		//UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Streaming Distance Multiplier"))
		//float StreamingDistanceMultiplier;

		// Lambdas for slider begin
		auto SliderBeginStreamingDistanceMultiplier = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			if (Cookables.Num() == 0)
				return;

			if (!IsValidWeakPointer(Cookables[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniChangeStreamingDistanceMultiplier", "Houdini Static Mesh Generation Properties: Changed StreamingDistanceMultiplier"),
				Cookables[0]->GetOuter());

			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;
			
				Cookables[Idx]->GetProxyData()->Modify();
			}
		};

		// Lambdas for slider end
		auto SliderEndStreamingDistanceMultiplier = [](const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			// Mark the value as changed to trigger an update
			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;

				// TODO: Mark changed or equivalent?
			}
		};

		// Lambdas for changing the value
		auto ChangeStreamingDistanceMultiplier = [](const float& Value, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
		{
			if (Cookables.Num() == 0)
				return;

			if (!IsValidWeakPointer(Cookables[0]))
				return;
		
			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniChangeStreamingDistanceMultiplier", "Houdini Static Mesh Generation Properties: Changed StreamingDistanceMultiplier"),
				Cookables[0]->GetOuter() );

			for (int Idx = 0; Idx < Cookables.Num(); Idx++)
			{
				if (!IsValidWeakPointer(Cookables[Idx]))
					continue;

				FHoudiniStaticMeshGenerationProperties& SMGP = Cookables[Idx]->GetStaticMeshGenerationProperties();
				if (SMGP.GeneratedStreamingDistanceMultiplier == Value)
					continue;

				Cookables[Idx]->Modify();
				SMGP.GeneratedStreamingDistanceMultiplier = Value;

				if (DoChange)
				{
					Cookables[Idx]->GetProxyData()->Modify();
				}
			}
		};

		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Streaming Distance Multiplier"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)

					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

					.MinValue(0)
					.MaxValue(3600)

					.MinSliderValue(0)
					.MaxSliderValue(60)

					.Value_Lambda([MainCookable]() { return MainCookable->GetStaticMeshGenerationProperties().GeneratedStreamingDistanceMultiplier; })
					.OnValueChanged_Lambda([InCookables, ChangeStreamingDistanceMultiplier](int32 Val)
					{ 
						ChangeStreamingDistanceMultiplier(Val, false, InCookables);
					})
					.OnValueCommitted_Lambda([InCookables, ChangeStreamingDistanceMultiplier](int32 Val, ETextCommit::Type TextCommitType)
					{	
						ChangeStreamingDistanceMultiplier(Val, true, InCookables);
					})
					.OnBeginSliderMovement_Lambda([InCookables, SliderBeginStreamingDistanceMultiplier]()
					{
						SliderBeginStreamingDistanceMultiplier(InCookables);
					})
					.OnEndSliderMovement_Lambda([InCookables, SliderEndStreamingDistanceMultiplier](const int32 NewValue)
					{ 
						SliderEndStreamingDistanceMultiplier(InCookables);
					})
					.SliderExponent(4.0f)
				]
			]
		];
	}
	*/


	//
	// FoliageDefaultSettings
	//
	/*
	{
		/// Default settings when using new Houdini Asset mesh for instanced foliage.
		//UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Foliage Default Settings"))
		//TObjectPtr<UFoliageType_InstancedStaticMesh>  FoliageDefaultSettings;

		// Create thumbnail for this HDA.
		//TSharedPtr<FAssetThumbnailPool> ThumbnailPool = HouMeshGenCategory.GetParentLayout().GetThumbnailPool();

		// Lambda for updating the Houdini asset
		auto UpdateFoliageSetting = [MainCookable](const TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables, UObject* InObject)
		{
			if (!IsValidWeakPointer(MainCookable))
				return;

			if (!InObject->IsA<UFoliageType_InstancedStaticMesh>())
				return;

			UFoliageType_InstancedStaticMesh* NewFoliageSetting = Cast<UFoliageType_InstancedStaticMesh>(InObject);
			if (!IsValid(NewFoliageSetting))
				return;

			// TODO: Transaction on all cookable?
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniSMGPFoliageSetting", "Houdini Static Mesh Generation Properties: Changed FoliageSetting"),
				MainCookable->GetOuter());

			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				if (!CurCookable->IsOutputSupported())
					continue;

				FHoudiniStaticMeshGenerationProperties& SMGP = CurCookable->GetStaticMeshGenerationProperties();
				if (SMGP.GeneratedFoliageDefaultSettings == NewFoliageSetting)
					continue;

				CurCookable->Modify();
				SMGP.GeneratedFoliageDefaultSettings = NewFoliageSetting;
			}
		};

		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Foliage Default Settings"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			.Padding(0, 5, 0, 0)
			.AutoHeight()
			[
				SNew(SObjectPropertyEntryBox)
				.ObjectPath_Lambda([MainCookable]()
				{
					if (!IsValidWeakPointer(MainCookable))
						return FString();

					UFoliageType_InstancedStaticMesh* FoliageSet = MainCookable->GetStaticMeshGenerationProperties().GeneratedFoliageDefaultSettings;
					if(!IsValid(FoliageSet))
						return FString();

					return FoliageSet->GetPathName();
				})
				.AllowedClass(UPhysicalMaterial::StaticClass())
				.OnObjectChanged_Lambda([InCookables, UpdateFoliageSetting](const FAssetData& InAssetData)
				{
					UFoliageType_InstancedStaticMesh* FoliageSet = Cast<UFoliageType_InstancedStaticMesh>(InAssetData.GetAsset());
					if (IsValid(FoliageSet))
						UpdateFoliageSetting(InCookables, FoliageSet);
				})
				.AllowCreate(false)
				.AllowClear(true)
				.DisplayUseSelected(true)
				.DisplayBrowse(true)
				.DisplayThumbnail(true)
				.ThumbnailPool(ThumbnailPool)
				.NewAssetFactories(TArray<UFactory*>())
			]
		];
	}
	*/

	//
	// AssetUserData
	//
	/*
	{
		/// Array of user data stored with the new Houdini Asset.
		//UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Asset User Data"))
		//TArray<TObjectPtr<UAssetUserData> > AssetUserData;

		// Create thumbnail for this HDA.
		//TSharedPtr<FAssetThumbnailPool> ThumbnailPool = HouMeshGenCategory.GetParentLayout().GetThumbnailPool();
		
		// Lambda for updating the Houdini asset
		auto UpdateUserData = [MainCookable](const TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables, UObject* InObject)
		{
			if (!IsValidWeakPointer(MainCookable))
				return;

			if (!InObject->IsA<UAssetUserData>())
				return;

			UAssetUserData* NewUserData = Cast<UAssetUserData>(InObject);
			if (!IsValid(NewUserData))
				return;

			// TODO: Transaction on all cookable?
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniSMGPAssetUserData", "Houdini Static Mesh Generation Properties: Changed AssetUserData"),
				MainCookable->GetOuter());

			for (auto CurCookable : InCookables)
			{
				if (!IsValidWeakPointer(CurCookable))
					continue;

				if (!CurCookable->IsOutputSupported())
					continue;

				FHoudiniStaticMeshGenerationProperties& SMGP = CurCookable->GetStaticMeshGenerationProperties();
				//if (SMGP.GeneratedAssetUserData == NewUserData)
					continue;

				CurCookable->Modify();
				//SMGP.GeneratedAssetUserData = NewUserData;
			}
		};

		ProxyGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Asset User Data"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
			.Padding(0, 5, 0, 0)
			.AutoHeight()
			[
				SNew(SObjectPropertyEntryBox)
				.ObjectPath_Lambda([MainCookable]()
				{
					if (!IsValidWeakPointer(MainCookable))
						return FString();

					UAssetUserData* UserData = MainCookable->GetStaticMeshGenerationProperties().GeneratedAssetUserData;
					if(!IsValid(UserData))
						return FString();

					return PhysMat->GetPathName();
				})
				.AllowedClass(UAssetUserData::StaticClass())
				.OnObjectChanged_Lambda([InCookables, UpdateUserData](const FAssetData& InAssetData)
				{
					UAssetUserData* NewUserData = Cast<UAssetUserData>(InAssetData.GetAsset());
					if (IsValid(NewUserData))
						UpdateUserData(InCookables, NewUserData);
				})
				.AllowCreate(false)
				.AllowClear(true)
				.DisplayUseSelected(true)
				.DisplayBrowse(true)
				.DisplayThumbnail(true)
				.ThumbnailPool(ThumbnailPool)
				.NewAssetFactories(TArray<UFactory*>())
			]
		];
	}
	
	*/
}

#undef LOCTEXT_NAMESPACE
