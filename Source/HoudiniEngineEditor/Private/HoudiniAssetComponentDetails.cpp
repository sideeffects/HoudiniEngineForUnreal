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

#include "HoudiniAssetComponentDetails.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniAsset.h"
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
FHoudiniAssetComponentDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniAssetComponentDetails);
}

FHoudiniAssetComponentDetails::FHoudiniAssetComponentDetails()
{
	OutputDetails = MakeShared<FHoudiniOutputDetails, ESPMode::NotThreadSafe>();
	ParameterDetails = MakeShared<FHoudiniParameterDetails, ESPMode::NotThreadSafe>();
	PDGDetails = MakeShared<FHoudiniPDGDetails, ESPMode::NotThreadSafe>();
	HoudiniEngineDetails = MakeShared<FHoudiniEngineDetails, ESPMode::NotThreadSafe>();
}

void 
FHoudiniAssetComponentDetails::AddIndieLicenseRow(IDetailCategoryBuilder& InCategory)
{
	FText IndieText =
		FText::FromString(TEXT("Houdini Engine Indie - For Limited Commercial Use Only"));

	FSlateFontInfo LargeDetailsFont = IDetailLayoutBuilder::GetDetailFontBold();
	LargeDetailsFont.Size += 2;

	FSlateColor LabelColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	InCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(STextBlock)
		.Text(IndieText)
		.ToolTipText(IndieText)
		.Font(LargeDetailsFont)
		.Justification(ETextJustify::Center)
		.ColorAndOpacity(LabelColor)
	];

	InCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0, 0, 5, 0)
		[
			SNew(SSeparator)
			.Thickness(2.0f)
		]
	];
}

void
FHoudiniAssetComponentDetails::AddEducationLicenseRow(IDetailCategoryBuilder& InCategory)
{
	FText EduText =
		FText::FromString(TEXT("Houdini Engine Education - For Educationnal Use Only"));

	FSlateFontInfo LargeDetailsFont = IDetailLayoutBuilder::GetDetailFontBold();
	LargeDetailsFont.Size += 2;

	FSlateColor LabelColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	InCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(STextBlock)
		.Text(EduText)
		.ToolTipText(EduText)
		.Font(LargeDetailsFont)
		.Justification(ETextJustify::Center)
		.ColorAndOpacity(LabelColor)
	];

	InCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0, 0, 5, 0)
		[
			SNew(SSeparator)
			.Thickness(2.0f)
		]
	];
}


void
FHoudiniAssetComponentDetails::AddSessionStatusRow(IDetailCategoryBuilder& InCategory)
{
	FDetailWidgetRow& PDGStatusRow = InCategory.AddCustomRow(FText::FromString("PDG Status"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([]()
			{
				FString StatusString;
				FLinearColor StatusColor;
				GetSessionStatusAndColor(StatusString, StatusColor);
				return FText::FromString(StatusString);
			})
			.ColorAndOpacity_Lambda([]()
			{
				FString StatusString;
				FLinearColor StatusColor;
				GetSessionStatusAndColor(StatusString, StatusColor);
				return FSlateColor(StatusColor);
			})
		]
	];
}

bool
FHoudiniAssetComponentDetails::GetSessionStatusAndColor(
	FString& OutStatusString, FLinearColor& OutStatusColor)
{
	OutStatusString = FString();
	OutStatusColor = FLinearColor::White;

	bool result = FHoudiniEngine::Get().GetSessionStatusAndColor(OutStatusString, OutStatusColor);
	return result;
}

void 
FHoudiniAssetComponentDetails::AddBakeMenu(IDetailCategoryBuilder& InCategory, UHoudiniAssetComponent* HAC) 
{
	FString CategoryName = "Bake";
	InCategory.AddGroup(FName(*CategoryName), FText::FromString(CategoryName), false, false);
	
}

// TSharedPtr<SWidget> FHoudiniAssetComponentDetails::ConstructActionMenu(TWeakObjectPtr<UHoudiniAssetComponent> HAC)
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
FHoudiniAssetComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get all components which are being customized.
	TArray< TWeakObjectPtr< UObject > > ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);
	
	// Extract the Houdini Asset Component to detail
	for (int32 i = 0; i < ObjectsCustomized.Num(); ++i)
	{
		if (!IsValidWeakPointer(ObjectsCustomized[i]))
			continue;

		UObject * Object = ObjectsCustomized[i].Get();
		if (Object)
		{
			UHoudiniAssetComponent * HAC = Cast< UHoudiniAssetComponent >(Object);
			if (IsValid(HAC))
				HoudiniAssetComponents.Add(HAC);
		}
	}

	// Check if we'll need to add indie license labels
	bool bIsIndieLicense = FHoudiniEngine::Get().IsLicenseIndie();
	bool bIsEduLicense = FHoudiniEngine::Get().IsLicenseEducation();

	// To handle multiselection parameter edit, we try to group the selected components by their houdini assets
	// TODO? ignore multiselection if all are not the same HDA?
	// TODO do the same for inputs
	TMap<TWeakObjectPtr<UHoudiniAsset>, TArray<TWeakObjectPtr<UHoudiniAssetComponent>>> HoudiniAssetToHACs;
	for (auto HAC : HoudiniAssetComponents)
	{
		// Add NodeSync component with a null Houdini Asset
		if (HAC->IsA<UHoudiniNodeSyncComponent>())
		{
			TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& ValueRef = HoudiniAssetToHACs.FindOrAdd(nullptr);
			ValueRef.Add(HAC);
			continue;
		}

		TWeakObjectPtr<UHoudiniAsset> HoudiniAsset = HAC->GetHoudiniAsset();
		if (!IsValidWeakPointer(HoudiniAsset))
			continue;

		TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& ValueRef = HoudiniAssetToHACs.FindOrAdd(HoudiniAsset);
		ValueRef.Add(HAC);
	}

	for (auto Iter : HoudiniAssetToHACs)
	{
		TArray<TWeakObjectPtr<UHoudiniAssetComponent>> HACs = Iter.Value;
		if (HACs.Num() < 1)
			continue;
			   
		TWeakObjectPtr<UHoudiniAssetComponent> MainComponent = HACs[0];
		if (!IsValidWeakPointer(MainComponent))
			continue;

		// If we have selected more than one component that have different HDAs, 
		// we'll want to separate the param/input/output category for each HDA
		FString MultiSelectionIdentifier = FString();
		if (HoudiniAssetToHACs.Num() > 1)
		{
			MultiSelectionIdentifier = TEXT("(");
			if (MainComponent->GetHoudiniAsset())
				MultiSelectionIdentifier += MainComponent->GetHoudiniAssetName();
			MultiSelectionIdentifier += TEXT(")");
		}


		bool bIsNodeSyncComponent = MainComponent->IsA<UHoudiniNodeSyncComponent>();
		//
		// 0. HOUDINI ASSET DETAILS
		//

		{
			FString HoudiniEngineCategoryName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_MAIN);
			HoudiniEngineCategoryName += MultiSelectionIdentifier;

			TSharedPtr<SLayeredImage> OptionsImage = SNew(SLayeredImage)
				 .Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
				 .ColorAndOpacity(FSlateColor::UseForeground());
			
			// Create Houdini Engine details category
			IDetailCategoryBuilder & HouEngineCategory =
				DetailBuilder.EditCategory(*HoudiniEngineCategoryName, FText::FromString("Houdini Engine"), ECategoryPriority::Important);

			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (bIsIndieLicense)
				AddIndieLicenseRow(HouEngineCategory);
			else if (bIsEduLicense)
				AddEducationLicenseRow(HouEngineCategory);

			TArray<TWeakObjectPtr<UHoudiniAssetComponent>> MultiSelectedHACs;
			for (auto& NextHACWeakPtr : HACs) 
			{
				if (!IsValidWeakPointer(NextHACWeakPtr))
					continue;

				MultiSelectedHACs.Add(NextHACWeakPtr);
			}

			HoudiniEngineDetails->CreateWidget(HouEngineCategory, MultiSelectedHACs);
		}

		if (bIsNodeSyncComponent)
		{
			// If we are working on a node sync component, display its specific options
			FString HoudiniNodeSyncCategoryName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_NODESYNC);
			HoudiniNodeSyncCategoryName += MultiSelectionIdentifier;

			TArray<TWeakObjectPtr<UHoudiniAssetComponent>> MultiSelectedHACs;
			for (auto& NextHACWeakPtr : HACs)
			{
				if (!IsValidWeakPointer(NextHACWeakPtr))
					continue;

				MultiSelectedHACs.Add(NextHACWeakPtr);
			}

			// Create Houdini Engine details category
			IDetailCategoryBuilder& HouNodeSyncCategory =
				DetailBuilder.EditCategory(*HoudiniNodeSyncCategoryName, FText::FromString("Houdini - Node Sync"), ECategoryPriority::Important);
			HoudiniEngineDetails->CreateNodeSyncWidgets(HouNodeSyncCategory, MultiSelectedHACs);
		}

		//
		//  1. PDG ASSET LINK (if available)
		//
		if (MainComponent->GetPDGAssetLink() && !bIsNodeSyncComponent)
		{
			FString PDGCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PDG);
			PDGCatName += MultiSelectionIdentifier;

			// Create the PDG Asset Link details category
			IDetailCategoryBuilder & HouPDGCategory =
				DetailBuilder.EditCategory(*PDGCatName, FText::FromString("Houdini - PDG Asset Link"), ECategoryPriority::Important);

			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (bIsIndieLicense)
				AddIndieLicenseRow(HouPDGCategory);
			else if (bIsEduLicense)
				AddEducationLicenseRow(HouPDGCategory);

			// TODO: Handle multi selection of outputs like params/inputs?


			PDGDetails->CreateWidget(HouPDGCategory, MainComponent->GetPDGAssetLink()/*, MainComponent*/);
		}
		

		//
		// 2. PARAMETER DETAILS
		//

		if (!bIsNodeSyncComponent)
		{
			// If we have selected more than one component that have different HDAs, 
			// we need to create multiple categories one for each different HDA
			FString ParamCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PARAMS);
			ParamCatName += MultiSelectionIdentifier;

			// Create the Parameters details category
			IDetailCategoryBuilder& HouParameterCategory =
				DetailBuilder.EditCategory(*ParamCatName, FText::GetEmpty(), ECategoryPriority::Important);

			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (MainComponent->GetNumParameters() > 0)
			{
				if (bIsIndieLicense)
					AddIndieLicenseRow(HouParameterCategory);
				else if (bIsEduLicense)
					AddEducationLicenseRow(HouParameterCategory);
			}

			TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>> JoinedParams;

			// Iterate through the component's parameters
			for (int32 ParamIdx = 0; ParamIdx < MainComponent->GetNumParameters(); ParamIdx++)
			{
				// We only want to create root parameters here, they will recursively create child parameters.
				UHoudiniParameter* CurrentParam = MainComponent->GetParameterAt(ParamIdx);
				if (!IsValid(CurrentParam))
					continue;

				// Build an array of edited parameter for multi edit
				JoinedParams.Emplace();
				auto& EditedParams = JoinedParams.Last();
				EditedParams.Add(CurrentParam);

				// Add the corresponding params in the other HAC
				for (int LinkedIdx = 1; LinkedIdx < HACs.Num(); LinkedIdx++)
				{
					UHoudiniParameter* LinkedParam = HACs[LinkedIdx]->GetParameterAt(ParamIdx);
					if (!IsValid(LinkedParam))
						continue;

					// Linked params should match the main param! If not try to find one that matches
					if (!LinkedParam->Matches(*CurrentParam))
					{
						LinkedParam = MainComponent->FindMatchingParameter(CurrentParam);
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

		//
		// 3. HANDLE DETAILS
		//
		if (!bIsNodeSyncComponent)
		{
			// If we have selected more than one component that have different HDAs, 
			// we need to create multiple categories one for each different HDA
			FString HandleCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_HANDLES);
			HandleCatName += MultiSelectionIdentifier;

			// Create the Parameters details category
			IDetailCategoryBuilder& HouHandleCategory =
				DetailBuilder.EditCategory(*HandleCatName, FText::GetEmpty(), ECategoryPriority::Important);

			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (MainComponent->GetNumHandles() > 0)
			{
				if (bIsIndieLicense)
					AddIndieLicenseRow(HouHandleCategory);
				else if (bIsEduLicense)
					AddEducationLicenseRow(HouHandleCategory);
			}

			// Iterate through the component's Houdini handles
			for (int32 HandleIdx = 0; HandleIdx < MainComponent->GetNumHandles(); ++HandleIdx)
			{
				UHoudiniHandleComponent* CurrentHandleComponent = MainComponent->GetHandleComponentAt(HandleIdx);

				if (!IsValid(CurrentHandleComponent))
					continue;

				TArray<TWeakObjectPtr<UHoudiniHandleComponent>> EditedHandles;
				EditedHandles.Add(CurrentHandleComponent);

				// Add the corresponding params in the other HAC
				for (int LinkedIdx = 1; LinkedIdx < HACs.Num(); ++LinkedIdx)
				{
					UHoudiniHandleComponent* LinkedHandle = HACs[LinkedIdx]->GetHandleComponentAt(HandleIdx);
					if (!IsValid(LinkedHandle))
						continue;

					// Linked handles should match the main param, if not try to find one that matches
					if (!LinkedHandle->Matches(*CurrentHandleComponent))
					{
						LinkedHandle = MainComponent->FindMatchingHandle(CurrentHandleComponent);
						if (!IsValid(LinkedHandle))
							continue;
					}

					EditedHandles.Add(LinkedHandle);
				}

				FHoudiniHandleDetails::CreateWidget(HouHandleCategory, EditedHandles);
			}
		}

		//
		// 5. INPUT DETAILS
		//
		if (!bIsNodeSyncComponent)
		{
			// If we have selected more than one component that have different HDAs, 
			// we need to create multiple categories one for each different HDA
			FString InputCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_INPUTS);
			InputCatName += MultiSelectionIdentifier;

			// Create the input details category
			IDetailCategoryBuilder& HouInputCategory =
				DetailBuilder.EditCategory(*InputCatName, FText::GetEmpty(), ECategoryPriority::Important);

			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (MainComponent->GetNumInputs() > 0)
			{
				if (bIsIndieLicense)
					AddIndieLicenseRow(HouInputCategory);
				else if (bIsEduLicense)
					AddEducationLicenseRow(HouInputCategory);
			}

			// Iterate through the component's inputs
			for (int32 InputIdx = 0; InputIdx < MainComponent->GetNumInputs(); InputIdx++)
			{
				UHoudiniInput* CurrentInput = MainComponent->GetInputAt(InputIdx);
				if (!IsValid(CurrentInput))
					continue;

				if (!MainComponent->IsInputTypeSupported(CurrentInput->GetInputType()))
					continue;

				// Object path parameter inputs are displayed by the ParameterDetails - skip them
				if (CurrentInput->IsObjectPathParameter())
					continue;

				// Build an array of edited inputs for multi edit
				TArray<TWeakObjectPtr<UHoudiniInput>> EditedInputs;
				EditedInputs.Add(CurrentInput);

				// Add the corresponding inputs in the other HAC
				for (int LinkedIdx = 1; LinkedIdx < HACs.Num(); LinkedIdx++)
				{
					UHoudiniInput* LinkedInput = HACs[LinkedIdx]->GetInputAt(InputIdx);
					if (!IsValid(LinkedInput))
						continue;

					// Linked params should match the main param! If not try to find one that matches
					if (!LinkedInput->Matches(*CurrentInput))
					{
						LinkedInput = MainComponent->FindMatchingInput(CurrentInput);
						if (!IsValid(LinkedInput))
							continue;
					}

					EditedInputs.Add(LinkedInput);
				}

				FHoudiniInputDetails::CreateWidget(HouInputCategory, EditedInputs);
			}
		}

		//
		// 6. OUTPUT DETAILS
		//

		// If we have selected more than one component that have different HDAs, 
		// we need to create multiple categories one for each different HDA
		FString OutputCatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_OUTPUTS);
		OutputCatName += MultiSelectionIdentifier;

		// Create the output details category
		IDetailCategoryBuilder & HouOutputCategory =
			DetailBuilder.EditCategory(*OutputCatName, FText::GetEmpty(), ECategoryPriority::Important);
	
		// Iterate through the component's outputs
		for (int32 OutputIdx = 0; OutputIdx < MainComponent->GetNumOutputs(); OutputIdx++)
		{
			UHoudiniOutput* CurrentOutput = MainComponent->GetOutputAt(OutputIdx);
			if (!IsValid(CurrentOutput))
				continue;

			// Build an array of edited outputs for multi edit
			TArray<TWeakObjectPtr<UHoudiniOutput>> EditedOutputs;
			EditedOutputs.Add(CurrentOutput);

			// Add the corresponding outputs in the other HAC
			for (int LinkedIdx = 1; LinkedIdx < HACs.Num(); LinkedIdx++)
			{
				UHoudiniOutput* LinkedOutput = HACs[LinkedIdx]->GetOutputAt(OutputIdx);
				if (!IsValid(LinkedOutput))
					continue;

				EditedOutputs.Add(LinkedOutput);
			}

			// TODO: Handle multi selection of outputs like params/inputs?	
			OutputDetails->CreateWidget(HouOutputCategory, EditedOutputs);
		}
	}
}


#undef LOCTEXT_NAMESPACE
