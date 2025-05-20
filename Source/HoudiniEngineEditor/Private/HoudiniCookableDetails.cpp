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
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"

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
	TWeakObjectPtr<UHoudiniCookable> MainCookable = InCookables[0];
	if (!IsValidWeakPointer(MainCookable))
		return;

	if (!MainCookable->IsProxySupported())
		return;

	// Create the Proxy details category
	FString ProxyCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PROXY);
	// If we have selected more than one component that have different HDAs, 
	// we need to create multiple categories one for each different HDA
	// OutputCatName += MultiSelectionIdentifier;
	
	IDetailCategoryBuilder& HouProxyCategory =
		DetailBuilder.EditCategory(*ProxyCatName, FText::GetEmpty(), ECategoryPriority::Important);

	FString Label = TEXT("Proxy Mesh Settings");
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
						// Refine if needed
						RefineCookablesIfNeeded();
					}
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
						// Refine if needed
						RefineCookablesIfNeeded();
					}
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


#undef LOCTEXT_NAMESPACE
