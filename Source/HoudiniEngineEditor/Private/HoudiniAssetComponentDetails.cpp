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
#include "HoudiniOutput.h"
#include "HoudiniOutputDetails.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"

#include "PropertyCustomizationHelpers.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

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


FHoudiniAssetComponentDetails::~FHoudiniAssetComponentDetails()
{
	// The ramp param's curves are added to root to avoid garbage collection
	// We need to remove those curves from the root when the details classes are destroyed.
	if (ParameterDetails.IsValid()) 
	{
		FHoudiniParameterDetails* ParamDetailsPtr = ParameterDetails.Get();

		for (auto& CurFloatRampCurveEditor : ParamDetailsPtr->CreatedFloatCurveEditors)
		{
			if (!CurFloatRampCurveEditor.IsValid())
				continue;

			CurFloatRampCurveEditor->HoudiniFloatRampCurve = nullptr;
			CurFloatRampCurveEditor->SetCurveOwner(nullptr);
		}
		for (auto& CurFloatRampCurve : ParamDetailsPtr->CreatedFloatRampCurves)
		{
			if (!IsValid(CurFloatRampCurve))
				continue;

			CurFloatRampCurve->RemoveFromRoot();
		}

		for (auto& CurColorRampCurveEditor : ParamDetailsPtr->CreatedColorGradientEditors)
		{
			if (!CurColorRampCurveEditor.IsValid())
				continue;

			CurColorRampCurveEditor->HoudiniColorRampCurve = nullptr;
			CurColorRampCurveEditor->SetCurveOwner(nullptr);
		}
		for (auto& CurColorRampCurve : ParamDetailsPtr->CreatedColorRampCurves)
		{
			if (!IsValid(CurColorRampCurve))
				continue;

			CurColorRampCurve->RemoveFromRoot();
		}
		
		ParamDetailsPtr->CreatedFloatCurveEditors.Empty();
		ParamDetailsPtr->CreatedColorGradientEditors.Empty();
		ParamDetailsPtr->CreatedFloatRampCurves.Empty();
		ParamDetailsPtr->CreatedColorRampCurves.Empty();
	}
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
FHoudiniAssetComponentDetails::AddSessionStatusRow(IDetailCategoryBuilder& InCategory)
{
	FDetailWidgetRow& PDGStatusRow = InCategory.AddCustomRow(FText::GetEmpty())
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

	const EHoudiniSessionStatus& SessionStatus = FHoudiniEngine::Get().GetSessionStatus();

	switch (SessionStatus)
	{
		case EHoudiniSessionStatus::NotStarted:
			// Session not initialized yet
			OutStatusString = TEXT("Houdini Engine Session - Not Started");
			OutStatusColor = FLinearColor::White;
			break;

		case EHoudiniSessionStatus::Connected:
			// Session successfully started
			OutStatusString = TEXT("Houdini Engine Session READY");
			OutStatusColor = FLinearColor::Green;
			break;
		case EHoudiniSessionStatus::Stopped:
			// Session stopped
			OutStatusString = TEXT("Houdini Engine Session STOPPED");
			OutStatusColor = FLinearColor(1.0f, 0.5f, 0.0f);
			break;
		case EHoudiniSessionStatus::Failed:
			// Session failed to be created/connected
			OutStatusString = TEXT("Houdini Engine Session FAILED");
			OutStatusColor = FLinearColor::Red;
			break;
		case EHoudiniSessionStatus::Lost:
			// Session Lost (HARS/Houdini Crash?)
			OutStatusString = TEXT("Houdini Engine Session LOST");
			OutStatusColor = FLinearColor::Red;
			break;
		case EHoudiniSessionStatus::NoLicense:
			// Failed to acquire a license
			OutStatusString = TEXT("Houdini Engine Session FAILED - No License");
			OutStatusColor = FLinearColor::Red;
			break;
		case EHoudiniSessionStatus::None:
			// Session type set to None
			OutStatusString = TEXT("Houdini Engine Session DISABLED");
			OutStatusColor = FLinearColor::White;
			break;
		default:
		case EHoudiniSessionStatus::Invalid:
			OutStatusString = TEXT("Houdini Engine Session INVALID");
			OutStatusColor = FLinearColor::Red;
			break;
	}

	// Handle a few specific case for active session
	if (SessionStatus == EHoudiniSessionStatus::Connected)
	{
		bool bPaused = !FHoudiniEngine::Get().IsCookingEnabled();
		bool bSSync = FHoudiniEngine::Get().IsSessionSyncEnabled();
		if (bPaused)
		{
			OutStatusString = TEXT("Houdini Engine Session PAUSED");
			OutStatusColor = FLinearColor::Yellow;
		}			
		/*
		else if (bSSync)
		{
			OutStatusString = TEXT("Houdini Engine Session Sync READY");
			OutStatusColor = FLinearColor::Blue;
		}
		*/
	}

	return true;
}

void 
FHoudiniAssetComponentDetails::AddBakeMenu(IDetailCategoryBuilder& InCategory, UHoudiniAssetComponent* HAC) 
{
	FString CategoryName = "Bake";
	InCategory.AddGroup(FName(*CategoryName), FText::FromString(CategoryName), false, false);
	
}

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

	// To handle multiselection parameter edit, we try to group the selected components by their houdini assets
	// TODO? ignore multiselection if all are not the same HDA?
	// TODO do the same for inputs
	TMap<TWeakObjectPtr<UHoudiniAsset>, TArray<TWeakObjectPtr<UHoudiniAssetComponent>>> HoudiniAssetToHACs;
	for (auto HAC : HoudiniAssetComponents)
	{ 
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
				MultiSelectionIdentifier += MainComponent->GetHoudiniAsset()->GetName();
			MultiSelectionIdentifier += TEXT(")");
		}

		/*
		// Handled by the UPROPERTIES on the component in v2!
		// Edit the Houdini details category
		IDetailCategoryBuilder & HoudiniAssetCategory =
			DetailBuilder.EditCategory("HoudiniAsset", FText::GetEmpty(), ECategoryPriority::Important);
		*/

		//
		// 0. HOUDINI ASSET DETAILS
		//

		{
			FString HoudiniEngineCategoryName = "Houdini Engine";
			HoudiniEngineCategoryName += MultiSelectionIdentifier;
			
			// Create Houdini Engine details category
			IDetailCategoryBuilder & HouEngineCategory =
				DetailBuilder.EditCategory(*HoudiniEngineCategoryName, FText::FromString("Houdini Engine"), ECategoryPriority::Important);
		
			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (bIsIndieLicense)
				AddIndieLicenseRow(HouEngineCategory);

			TArray<TWeakObjectPtr<UHoudiniAssetComponent>> MultiSelectedHACs;
			for (auto& NextHACWeakPtr : HACs) 
			{
				if (!IsValidWeakPointer(NextHACWeakPtr))
					continue;

				MultiSelectedHACs.Add(NextHACWeakPtr);
			}

			HoudiniEngineDetails->CreateWidget(HouEngineCategory, MultiSelectedHACs);
		}

		//
		//  1. PDG ASSET LINK (if available)
		//
		if (MainComponent->GetPDGAssetLink())
		{
			FString PDGCatName = "HoudiniPDGAssetLink";
			PDGCatName += MultiSelectionIdentifier;

			// Create the PDG Asset Link details category
			IDetailCategoryBuilder & HouPDGCategory =
				DetailBuilder.EditCategory(*PDGCatName, FText::FromString("Houdini - PDG Asset Link"), ECategoryPriority::Important);

			// If we are running Houdini Engine Indie license, we need to display a special label.
			if (bIsIndieLicense)
				AddIndieLicenseRow(HouPDGCategory);

			// TODO: Handle multi selection of outputs like params/inputs?


			PDGDetails->CreateWidget(HouPDGCategory, MainComponent->GetPDGAssetLink()/*, MainComponent*/);
		}
		

		//
		// 2. PARAMETER DETAILS
		//

		// If we have selected more than one component that have different HDAs, 
		// we need to create multiple categories one for each different HDA
		FString ParamCatName = "HoudiniParameters";
		ParamCatName += MultiSelectionIdentifier;

		// Create the Parameters details category
		IDetailCategoryBuilder & HouParameterCategory =
			DetailBuilder.EditCategory(*ParamCatName, FText::GetEmpty(), ECategoryPriority::Important);

		// If we are running Houdini Engine Indie license, we need to display a special label.
		if(bIsIndieLicense)
			AddIndieLicenseRow(HouParameterCategory);

		// Iterate through the component's parameters
		for (int32 ParamIdx = 0; ParamIdx < MainComponent->GetNumParameters(); ParamIdx++)
		{	
			// We only want to create root parameters here, they will recursively create child parameters.
			UHoudiniParameter* CurrentParam = MainComponent->GetParameterAt(ParamIdx);
			if (!IsValid(CurrentParam))
				continue;
			
			// TODO: remove ? unneeded?
			// ensure the parameter is actually owned by a HAC
			/*const TWeakObjectPtr<UHoudiniAssetComponent> Owner = Cast<UHoudiniAssetComponent>(CurrentParam->GetOuter());
			if (!Owner.IsValid())
				continue;*/

			// Build an array of edited parameter for multi edit
			TArray<TWeakObjectPtr<UHoudiniParameter>> EditedParams;
			EditedParams.Add(CurrentParam);

			// Add the corresponding params in the other HAC
			for (int LinkedIdx = 1; LinkedIdx < HACs.Num(); LinkedIdx++)
			{
				UHoudiniParameter* LinkedParam = HACs[LinkedIdx]->GetParameterAt(ParamIdx);
				if (!IsValid(LinkedParam))
					continue;

				// Linked params should match the main param! If not try to find one that matches
				if ( !LinkedParam->Matches(*CurrentParam) )
				{
					LinkedParam = MainComponent->FindMatchingParameter(CurrentParam);
					if (!IsValid(LinkedParam) || LinkedParam->IsChildParameter())
						continue;
				}

				EditedParams.Add(LinkedParam);
			}

			ParameterDetails->CreateWidget(HouParameterCategory, EditedParams);
		}

		/***   HOUDINI HANDLE DETAILS   ***/

		// If we have selected more than one component that have different HDAs, 
		// we need to create multiple categories one for each different HDA
		FString HandleCatName = "HoudiniHandles";
		HandleCatName += MultiSelectionIdentifier;

		// Create the Parameters details category
		IDetailCategoryBuilder & HouHandleCategory =
			DetailBuilder.EditCategory(*HandleCatName, FText::GetEmpty(), ECategoryPriority::Important);

		// If we are running Houdini Engine Indie license, we need to display a special label.
		if (bIsIndieLicense)
			AddIndieLicenseRow(HouHandleCategory);

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


		//
		// 3. INPUT DETAILS
		//

		// If we have selected more than one component that have different HDAs, 
		// we need to create multiple categories one for each different HDA
		FString InputCatName = "HoudiniInputs";
		InputCatName += MultiSelectionIdentifier;

		// Create the input details category
		IDetailCategoryBuilder & HouInputCategory =
			DetailBuilder.EditCategory(*InputCatName, FText::GetEmpty(), ECategoryPriority::Important);

		// If we are running Houdini Engine Indie license, we need to display a special label.
		if (bIsIndieLicense)
			AddIndieLicenseRow(HouInputCategory);
		
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

		//
		// 4. OUTPUT DETAILS
		//

		// If we have selected more than one component that have different HDAs, 
		// we need to create multiple categories one for each different HDA
		FString OutputCatName = "HoudiniOutputs";
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

			// Build an array of edited inpoutputs for multi edit
			TArray<TWeakObjectPtr<UHoudiniOutput>> EditedOutputs;
			EditedOutputs.Add(CurrentOutput);

			// Add the corresponding outputs in the other HAC
			for (int LinkedIdx = 1; LinkedIdx < HACs.Num(); LinkedIdx++)
			{
				UHoudiniOutput* LinkedOutput = HACs[LinkedIdx]->GetOutputAt(OutputIdx);
				if (!IsValid(LinkedOutput))
					continue;

				/*
				// Linked output should match the main output! If not try to find one that matches
				if (!LinkedOutput->Matches(*CurrentOutput))
				{
					LinkedOutput = MainComponent->FindMatchingInput(CurrentOutput);
					if (!IsValid(LinkedOutput))
						continue;
				}
				*/

				EditedOutputs.Add(LinkedOutput);
			}

			// TODO: Handle multi selection of outputs like params/inputs?	
			OutputDetails->CreateWidget(HouOutputCategory, EditedOutputs);
		}
	}
}


#undef LOCTEXT_NAMESPACE
