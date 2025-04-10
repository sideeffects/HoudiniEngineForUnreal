/*
* Copyright (c) <2021> Side Effects Software Inc.
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
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "HoudiniEngineEditorPrivatePCH.h"

#include "PropertyCustomizationHelpers.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "SHoudiniPresets.h"
#include "Chaos/AABB.h"
#include "Chaos/AABB.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Images/SLayeredImage.h"

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

// TSharedPtr<SWidget> FHoudiniCookableDetails::ConstructActionMenu(TWeakObjectPtr<UHoudiniAssetComponent> HAC)
// {
// 	FMenuBuilder MenuBuilder( true, NULL );
//
// 	if (!HAC.IsValid())
// 	{
// 		return MenuBuilder.MakeWidget();
// 	}
//
// 	MenuBuilder.BeginSection("AssetCreate", LOCTEXT("HDAActionMenu_SectionCreate", "Create"));
//
// 	// Options - Create Preset
// 	MenuBuilder.AddMenuEntry(
// 		FText::FromString("Create Preset"),
// 		FText::FromString("Create a new preset from the current HoudiniAssetComponent parameters."),
// 		FSlateIcon(),
// 		FUIAction(
// 			FExecuteAction::CreateLambda([HAC]() -> void
// 			{
// 				SHoudiniCreatePresetFromHDA::CreateDialog(HAC);
// 			}),
// 			FCanExecuteAction()
// 		)
// 	);
// 	
// 	// SHoudiniCreatePresetFromHDA::Create(HAC);
//
// 	MenuBuilder.EndSection();
//
// 	MenuBuilder.BeginSection("Modify", LOCTEXT("HDAActionMenu_SectionModify", "Modify"));
//
// 	// Presets submenu
// 	// MenuBuilder.AddSubMenu( LOCTEXT("HDAActionMenu_SubmenuPresets", "Presets")
// 	// 	,
// 	// 	)
// 	MenuBuilder.EndSection();
//
// 	return MenuBuilder.MakeWidget();
// }


void
FHoudiniCookableDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get all components which are being customized.
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);
	
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
				if (!IsValid(HC))
					continue;

				HoudiniCookable.Add(HC);
			}	
		}
	}

	// Check if we'll need to add indie license labels
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();

	// To handle multiselection parameter edit, we try to group the selected components by their houdini assets
	// TODO? ignore multiselection if all are not the same HDA?
	// TODO? do the same for inputs
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
		// 0. HOUDINI ASSET DETAILS
		//
		CreateHoudiniEngineDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		
		//
		// 1. NODE SYNC DETAILS
		//		
		// TODO: Handle NodeSync better?
		//bool bIsNodeSyncComponent = MainComponent->IsA<UHoudiniNodeSyncComponent>();
		bool bIsNodeSyncComponent = MainCookable->GetComponent() ? MainCookable->GetComponent()->IsA<UHoudiniNodeSyncComponent>() : false;		
		if (bIsNodeSyncComponent)
		{
			// TODO: COOKABLE - node sync!
			CreateNodeSyncDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		//  2. PDG ASSET LINK (if available)
		//
		if (MainCookable->IsPDGSupported())
		{
			CreatePDGDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// 3. PARAMETER DETAILS
		//
		if (MainCookable->IsParameterSupported())
		{
			CreateParameterDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// 4. HANDLE DETAILS
		//
		if (MainCookable->IsComponentSupported())
		{
			CreateHandleDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// 5. INPUT DETAILS
		//
		if (MainCookable->IsInputSupported())
		{
			CreateInputDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}

		//
		// 6. OUTPUT DETAILS
		//
		if (MainCookable->IsOutputSupported())
		{
			CreateOutputDetails(DetailBuilder, HCs, MultiSelectionIdentifier);
		}
	}
}

void
FHoudiniCookableDetails::CreateHoudiniEngineDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
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

	// TODO COOKABLE: Handle presets!
	// Widget for HoudiniAsset related actions. Currently only contains things for Presets.
	//HoudiniEngineDetails->CreateHoudiniEngineActionWidget(HouEngineCategory, MultiSelectedHCs);

	// Houdini Engine Session Status
	HoudiniEngineDetails->AddSessionStatusRow(HouEngineCategory);

	// Create Generate Category
	HoudiniEngineDetails->CreateGenerateWidgets(HouEngineCategory, InCookables);

	// Create Bake Category
	HoudiniEngineDetails->CreateBakeWidgets(HouEngineCategory, InCookables);

	// Create Asset Options Category
	HoudiniEngineDetails->CreateAssetOptionsWidgets(HouEngineCategory, InCookables);

	// Create Help and Debug Category
	HoudiniEngineDetails->CreateHelpAndDebugWidgets(HouEngineCategory, InCookables);
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

	// TODO: COOKABLE - Handle Node sync
	// Create Houdini Engine details category
	//IDetailCategoryBuilder& HouNodeSyncCategory =
	//	DetailBuilder.EditCategory(*HoudiniNodeSyncCategoryName, FText::FromString("Houdini - Node Sync"), ECategoryPriority::Important);
	//HoudiniEngineDetails->CreateNodeSyncWidgets(HouNodeSyncCategory, InCookables);
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

	// TODO: Handle multi selection of outputs like params/inputs?
	PDGDetails->CreateWidget(HouPDGCategory, HPDGAL);
}

void
FHoudiniCookableDetails::CreateParameterDetails(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
	const FString& MultiSelectionIdentifier)
{
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

#undef LOCTEXT_NAMESPACE
