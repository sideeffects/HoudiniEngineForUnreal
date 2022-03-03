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

#include "HoudiniInputDetails.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniInput.h"
#include "HoudiniInputWidgets.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetBlueprintComponent.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEngineDetails.h"

#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "ScopedTransaction.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "AssetData.h"
#include "Framework/SlateDelegates.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerModule.h"

#include "Editor/UnrealEdEngine.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "UnrealEdGlobals.h"
#include "Widgets/SWidget.h"

#include "HoudiniEngineRuntimeUtils.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"

#include "ActorTreeItem.h"
#include "Widgets/Layout/SExpandableArea.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

// Customized TextBlock to show 'editing...' text if this Houdini Spline Component is being edited
class SCurveEditingTextBlock : public STextBlock
{
public:
	UHoudiniSplineComponent* HoudiniSplineComponent;
	TSharedPtr<FHoudiniSplineComponentVisualizer> HoudiniSplineComponentVisualizer;
public:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override 
	{
		if (!HoudiniSplineComponentVisualizer.IsValid() || !HoudiniSplineComponent)
			return LayerId;

		if (HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent() != HoudiniSplineComponent)
			return LayerId;

		return STextBlock::OnPaint(Args, AllottedGeometry, MyClippingRect,
			OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
};

void
FHoudiniInputDetails::CreateWidget(
	IDetailCategoryBuilder& HouInputCategory,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	FDetailWidgetRow* InputRow)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Get thumbnail pool for this builder.
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouInputCategory.GetParentLayout().GetThumbnailPool();
	
	EHoudiniInputType MainInputType = MainInput->GetInputType();
	UHoudiniAssetComponent* HAC = MainInput->GetTypedOuter<UHoudiniAssetComponent>();

	// Create a widget row, or get the given row.
	FDetailWidgetRow* Row = InputRow;
	Row = InputRow == nullptr ? &(HouInputCategory.AddCustomRow(FText::GetEmpty())) : InputRow;
	if (!Row)
		return;

	// Create the standard input name widget if this is not a operator path parameter.
	// Operator path parameter's name widget is handled by HoudiniParameterDetails.
	if (!InputRow)
		CreateNameWidget(MainInput, *Row, true, InInputs.Num());

	// Create a vertical Box for storing the UI
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	// ComboBox :  Input Type
	const IDetailsView* DetailsView = HouInputCategory.GetParentLayout().GetDetailsView();
	AddInputTypeComboBox(HouInputCategory, VerticalBox, InInputs, DetailsView);

	// Checkbox : Keep World Transform
	AddKeepWorldTransformCheckBox(VerticalBox, InInputs);


	// Checkbox : CurveInput trigger cook on curve changed
	AddCurveInputCookOnChangeCheckBox(VerticalBox, InInputs);


	
	if (MainInputType == EHoudiniInputType::Geometry || MainInputType == EHoudiniInputType::World)
	{
		// Checkbox : Pack before merging
		AddPackBeforeMergeCheckbox(VerticalBox, InInputs);
	}

	if (MainInputType == EHoudiniInputType::Geometry || MainInputType == EHoudiniInputType::World || MainInputType == EHoudiniInputType::Asset) 
	{
		AddImportAsReferenceCheckbox(VerticalBox, InInputs);
	}

	if (MainInputType == EHoudiniInputType::Geometry || MainInputType == EHoudiniInputType::World)
	{
		// Checkboxes : Export LODs / Sockets / Collisions
		AddExportCheckboxes(VerticalBox, InInputs);
	}

	switch (MainInput->GetInputType())
	{
		case EHoudiniInputType::Geometry:
		{
			AddGeometryInputUI(HouInputCategory, VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::Asset:
		{
			AddAssetInputUI(VerticalBox, InInputs);
		}
		break;

		case EHoudiniInputType::Curve:
		{
			AddCurveInputUI(HouInputCategory, VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::Landscape:
		{
			AddLandscapeInputUI(VerticalBox, InInputs);
		}
		break;

		case EHoudiniInputType::World:
		{
			AddWorldInputUI(HouInputCategory, VerticalBox, InInputs, DetailsView);
		}
		break;

		case EHoudiniInputType::Skeletal:
		{
			AddSkeletalInputUI(VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::GeometryCollection:
		{
			AddGeometryCollectionInputUI(HouInputCategory, VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;
		
	}


	Row->ValueWidget.Widget = VerticalBox;

	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	//Row.ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniInputDetails::CreateNameWidget(
	const TWeakObjectPtr<UHoudiniInput>& InInput, FDetailWidgetRow & Row, bool bLabel, int32 InInputCount)
{
	if (!IsValidWeakPointer(InInput))
		return;

	FString InputLabelStr = InInput->GetLabel();
	if (InInputCount > 1)
	{
		InputLabelStr += TEXT(" (") + FString::FromInt(InInputCount) + TEXT(")");
	}

	const FText & FinalInputLabelText = bLabel ? FText::FromString(InputLabelStr) : FText::GetEmpty();
	FText InputTooltip = GetInputTooltip(InInput);
	{
		Row.NameWidget.Widget =
			SNew(STextBlock)
			.Text(FinalInputLabelText)
			.ToolTipText(InputTooltip)
			.Font(FEditorStyle::GetFontStyle(!InInput->HasChanged() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")));
	}
}

FText
FHoudiniInputDetails::GetInputTooltip(const TWeakObjectPtr<UHoudiniInput>& InParam)
{
	// TODO
	return FText();
}

void
FHoudiniInputDetails::AddInputTypeComboBox(IDetailCategoryBuilder& CategoryBuilder, TSharedRef<SVerticalBox> VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const IDetailsView* DetailsView)
{
	// Get the details view name and locked status
	bool bDetailsLocked = false;
	FName DetailsPanelName = "LevelEditorSelectionDetails";
	if (DetailsView)
	{
		DetailsPanelName = DetailsView->GetIdentifier();
		if (DetailsView->IsLocked())
			bDetailsLocked = true;
	}

	// Lambda return a FText correpsonding to an input's current type
	auto GetInputText = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return FText();
		return FText::FromString(InInput->GetInputTypeAsString());
	};

	// Lambda for changing inputs type
	auto OnSelChanged = [DetailsPanelName](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, TSharedPtr<FString> InNewChoice)
	{
		if (!InNewChoice.IsValid())
			return;
		
		EHoudiniInputType NewInputType = UHoudiniInput::StringToInputType(*InNewChoice.Get());
		if (NewInputType != EHoudiniInputType::World)
		{
			Helper_CancelWorldSelection(InInputsToUpdate, DetailsPanelName);
		}

		if (InInputsToUpdate.Num() <= 0)
			return;

		const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputsToUpdate[0];
		if (!IsValidWeakPointer(MainInput))
			return;

		UHoudiniAssetBlueprintComponent* HAB = MainInput->GetTypedOuter<UHoudiniAssetBlueprintComponent>();

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Input Type"),
			MainInput->GetOuter());

		bool bBlueprintStructureModified = false;

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetInputType() == NewInputType)
				continue;

			/*  This causes multiple issues. It does not set reset the previous type variable to Invalid sometimes
				and it causes re-cook infinitely after few undo changing type.
			{
				CurInput->SetInputType(NewInputType);
				CurInput->Modify();
			}
			*/

			{
				// Cache the current input type for undo type changing (since new type becomes previous type after undo)
				EHoudiniInputType PrevType = CurInput->GetPreviousInputType();
				CurInput->SetPreviousInputType(NewInputType);
				
				CurInput->Modify();
				CurInput->SetPreviousInputType(PrevType);
				CurInput->SetInputType(NewInputType, bBlueprintStructureModified);   // pass in false for 2nd parameter in order to avoid creating default curve if empty
			}
			CurInput->MarkChanged(true);

			FHoudiniEngineEditorUtils::ReselectSelectedActors();

		}

		if (HAB)
		{
			if (bBlueprintStructureModified)
				HAB->MarkAsBlueprintStructureModified();
		}

	};

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	TArray<TSharedPtr<FString>>* SupportedChoices = nullptr;
	UHoudiniAssetBlueprintComponent* HAC = MainInput->GetTypedOuter<UHoudiniAssetBlueprintComponent>();
	if (HAC)
	{
		SupportedChoices = FHoudiniEngineEditor::Get().GetBlueprintInputTypeChoiceLabels();
	}
	else
	{
		SupportedChoices = FHoudiniEngineEditor::Get().GetInputTypeChoiceLabels();
	}

	// ComboBox :  Input Type
	TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBoxInputType;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ComboBoxInputType, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(SupportedChoices)
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetInputTypeChoiceLabels())[((int32)MainInput->GetInputType() - 1)])
		.OnGenerateWidget_Lambda(
			[](TSharedPtr< FString > ChoiceEntry)
		{
			FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
			return SNew(STextBlock)
				.Text(ChoiceEntryText)
				.ToolTipText(ChoiceEntryText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
		})
		.OnSelectionChanged_Lambda([=](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
		{
			return OnSelChanged(InInputs, NewChoice);
		})
		[
			SNew( STextBlock )
			.Text_Lambda([=]()
			{
				return GetInputText(MainInput); 
			})            
			.Font( FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];
}

void
FHoudiniInputDetails:: AddCurveInputCookOnChangeCheckBox(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!MainInput.IsValid() || MainInput->GetInputType() != EHoudiniInputType::Curve)
		return;

	auto IsCheckedCookOnChange = [MainInput]()
	{
		if (!IsValidWeakPointer(MainInput))
			return ECheckBoxState::Checked;

		return MainInput->GetCookOnCurveChange() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto CheckStateChangedCookOnChange = [InInputs](ECheckBoxState NewState)
	{
		const bool bChecked = NewState == ECheckBoxState::Checked;
		for (auto & NextInput : InInputs) 
		{
			if (!IsValidWeakPointer(NextInput))
				continue;

			NextInput->SetCookOnCurveChange(bChecked);
		}
	};

	// Checkbox : Trigger cook on input curve changed
	TSharedPtr< SCheckBox > CheckBoxCookOnCurveChanged;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew(CheckBoxCookOnCurveChanged, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CookOnCurveChangedCheckbox", "Auto-update"))
			.ToolTipText(LOCTEXT("CookOnCurveChangeCheckboxTip", "When checked, cook is triggered automatically when the curve is modified."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([IsCheckedCookOnChange, MainInput]()
		{
			return IsCheckedCookOnChange();
		})
		.OnCheckStateChanged_Lambda([CheckStateChangedCookOnChange](ECheckBoxState NewState)
		{
			return CheckStateChangedCookOnChange( NewState);
		})
	];
	
}

void
FHoudiniInputDetails::AddKeepWorldTransformCheckBox(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current KeepWorldTransform state
	auto IsCheckedKeepWorldTransform = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		return InInput.IsValid() && InInput->GetKeepWorldTransform() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing KeepWorldTransform state
	auto CheckStateChangedKeepWorldTransform = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);
		if (MainInput->GetKeepWorldTransform() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Keep World Transform"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetKeepWorldTransform() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetKeepWorldTransform(bNewState);
			CurInput->MarkChanged(true);
		}
	};


	// Checkbox : Keep World Transform
	TSharedPtr< SCheckBox > CheckBoxTranformType;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew(CheckBoxTranformType, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("KeepWorldTransformCheckbox", "Keep World Transform"))
			.ToolTipText(LOCTEXT("KeepWorldTransformCheckboxTip", "Set this Input's object_merge Transform Type to INTO_THIS_OBJECT. If unchecked, it will be set to NONE."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]		
		.IsChecked_Lambda([=]()
		{
			return IsCheckedKeepWorldTransform(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedKeepWorldTransform(InInputs, NewState);
		})
	];

	// the checkbox is read only for geo inputs
	if (MainInput->GetInputType() == EHoudiniInputType::Geometry)
		CheckBoxTranformType->SetEnabled(false);

	// Checkbox is read only if the input is an object-path parameter
	//if (MainInput->IsObjectPathParameter() )
	//    CheckBoxTranformType->SetEnabled(false);
}

void
FHoudiniInputDetails::AddPackBeforeMergeCheckbox(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{

	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current PackBeforeMerge state
	auto IsCheckedPackBeforeMerge = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetPackBeforeMerge() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing PackBeforeMerge state
	auto CheckStateChangedPackBeforeMerge = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetPackBeforeMerge() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Pack before merge"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetPackBeforeMerge() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetPackBeforeMerge(bNewState);
			CurInput->MarkChanged(true);
		}
	};

	TSharedPtr< SCheckBox > CheckBoxPackBeforeMerge;
	VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
	[
		SAssignNew( CheckBoxPackBeforeMerge, SCheckBox )
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "PackBeforeMergeCheckbox", "Pack Geometry before merging" ) )
			.ToolTipText( LOCTEXT( "PackBeforeMergeCheckboxTip", "Pack each separate piece of geometry before merging them into the input." ) )
			.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedPackBeforeMerge(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedPackBeforeMerge(InInputs, NewState);
		})
	];
}

void
FHoudiniInputDetails::AddImportAsReferenceCheckbox(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current PackBeforeMerge state
	auto IsCheckedImportAsReference= [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetImportAsReference() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing PackBeforeMerge state
	auto CheckStateChangedImportAsReference= [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		const bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetImportAsReference() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Pack before merge"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetImportAsReference() == bNewState)
				continue;

			TArray<UHoudiniInputObject*> * InputObjs = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
			if (InputObjs) 
			{
				// Mark all its input objects as changed to trigger recook.
				for (auto CurInputObj : *InputObjs) 
				{
					if (!IsValid(CurInputObj))
						continue;

					if (CurInputObj->GetImportAsReference() != bNewState)
					{
						CurInputObj->SetImportAsReference(bNewState);
						CurInputObj->MarkChanged(true);
					}
				}
			}

			CurInput->Modify();
			CurInput->SetImportAsReference(bNewState);
		}
	};

	TSharedPtr< SCheckBox > CheckBoxImportAsReference;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew(CheckBoxImportAsReference, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ImportInputAsRefCheckbox", "Import input as references"))
			.ToolTipText(LOCTEXT("ImportInputAsRefCheckboxTip", "Import input objects as references. (Geometry, World and Asset input types only)"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedImportAsReference(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedImportAsReference(InInputs, NewState);
		})
	];

	// Add Rot/Scale to input as reference
	auto IsCheckedImportAsReferenceRotScale= [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetImportAsReferenceRotScaleEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing PackBeforeMerge state
	auto CheckStateChangedImportAsReferenceRotScale= [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetImportAsReferenceRotScaleEnabled() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputAsReferenceRotScale", "Houdini Input: Changing InputAsReference Rot/Scale"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetImportAsReferenceRotScaleEnabled() == bNewState)
				continue;

			TArray<UHoudiniInputObject*> * InputObjs = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
			if (InputObjs) 
			{
				// Mark all its input objects as changed to trigger recook.
				for (auto CurInputObj : *InputObjs) 
				{
					if (!IsValid(CurInputObj))
						continue;

					if (CurInputObj->GetImportAsReferenceRotScaleEnabled() != bNewState)
					{
						CurInputObj->SetImportAsReferenceRotScaleEnabled(bNewState);
						CurInputObj->MarkChanged(true);
					}
				}
			}

			CurInput->Modify();
			CurInput->SetImportAsReferenceRotScaleEnabled(bNewState);
		}
	};

	TSharedPtr< SCheckBox > CheckBoxImportAsReferenceRotScale;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew(CheckBoxImportAsReferenceRotScale, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ImportInputAsRefRotScaleCheckbox", "Add rot/scale to input references"))
			.ToolTipText(LOCTEXT("ImportInputAsRefRotScaleCheckboxTip", "Add rot/scale attributes to the input references when Import input as references is enabled"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedImportAsReferenceRotScale(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedImportAsReferenceRotScale(InInputs, NewState);
		})
		.IsEnabled(IsCheckedImportAsReference(MainInput))
			
	];
}
void
FHoudiniInputDetails::AddExportCheckboxes(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current ExportLODs state
	auto IsCheckedExportLODs = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetExportLODs() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda returning a CheckState from the input's current ExportSockets state
	auto IsCheckedExportSockets = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetExportSockets() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda returning a CheckState from the input's current ExportColliders state
	auto IsCheckedExportColliders = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetExportColliders() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing ExportLODs state
	auto CheckStateChangedExportLODs = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetExportLODs() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Export LODs"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetExportLODs() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetExportLODs(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda for changing ExportSockets state
	auto CheckStateChangedExportSockets = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetExportSockets() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Export Sockets"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetExportSockets() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetExportSockets(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda for changing ExportColliders state
	auto CheckStateChangedExportColliders = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetExportColliders() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Export Colliders"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetExportColliders() == bNewState)
				continue;

			CurInput->Modify();
			
			CurInput->SetExportColliders(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	TSharedPtr< SCheckBox > CheckBoxExportLODs;
	TSharedPtr< SCheckBox > CheckBoxExportSockets;
	TSharedPtr< SCheckBox > CheckBoxExportColliders;
	VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
	[
		SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.Padding( 1.0f )
		.VAlign( VAlign_Center )
		.AutoWidth()
		[
			SAssignNew(CheckBoxExportLODs, SCheckBox )
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportAllLOD", "Export LODs" ) )
				.ToolTipText( LOCTEXT( "ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini." ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportLODs(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportLODs(InInputs, NewState);
			})
		]
		+ SHorizontalBox::Slot()
		.Padding( 1.0f )
		.VAlign( VAlign_Center )
		.AutoWidth()
		[
			SAssignNew( CheckBoxExportSockets, SCheckBox )
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportSockets", "Export Sockets" ) )
				.ToolTipText( LOCTEXT( "ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini." ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportSockets(MainInput);
			})
				.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportSockets(InInputs, NewState);
			})
		]
		+ SHorizontalBox::Slot()
		.Padding( 1.0f )
		.VAlign( VAlign_Center )
		.AutoWidth()
		[
			SAssignNew( CheckBoxExportColliders, SCheckBox )
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportColliders", "Export Colliders" ) )
				.ToolTipText( LOCTEXT( "ExportCollidersTip", "If enabled, collision geometry for this static mesh will be sent to Houdini." ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportColliders(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportColliders(InInputs, NewState);
			})
		]
	];
}

void
FHoudiniInputDetails::AddGeometryInputUI(
	IDetailCategoryBuilder& CategoryBuilder,
	TSharedRef<SVerticalBox> InVerticalBox, 
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool )
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);

	// Lambda for changing ExportColliders state
	auto SetGeometryInputObjectsCount = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const int32& NewInputCount)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing the number of Geometry Input Objects"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry) == NewInputCount)
				continue;

			CurInput->Modify();

			CurInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, NewInputCount);
			CurInput->MarkChanged(true);

			// 
			if (GEditor)
				GEditor->RedrawAllViewports();

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	InVerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("NumArrayItemsFmt", "{0} elements"), FText::AsNumber(NumInputObjects)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([SetGeometryInputObjectsCount, InInputs, NumInputObjects]()
			{
				return SetGeometryInputObjectsCount(InInputs, NumInputObjects + 1);
			}),
			LOCTEXT("AddInput", "Adds a Geometry Input"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([SetGeometryInputObjectsCount, InInputs]()
			{
				return SetGeometryInputObjectsCount(InInputs, 0);
			}),
			LOCTEXT("EmptyInputs", "Removes All Inputs"), true)
		]
	];

	for (int32 GeometryObjectIdx = 0; GeometryObjectIdx < NumInputObjects; GeometryObjectIdx++)
	{
		//UObject* InputObject = InParam.GetInputObject(Idx);
		Helper_CreateGeometryWidget(CategoryBuilder, InInputs, GeometryObjectIdx, AssetThumbnailPool, InVerticalBox);
	}
}



// Create a single geometry widget for the given input object
void 
FHoudiniInputDetails::Helper_CreateGeometryWidget(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, 
	const int32& InGeometryObjectIdx,
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool,
	TSharedRef< SVerticalBox > VerticalBox )
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	// Access the object used in the corresponding geometry input
	UHoudiniInputObject* HoudiniInputObject = MainInput->GetHoudiniInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
	UObject* InputObject = HoudiniInputObject ? HoudiniInputObject->GetObject() : nullptr;

	// Create thumbnail for this static mesh.
	TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(
		new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

	// Lambda for adding new geometry input objects
	auto UpdateGeometryObjectAt = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const int32& AtIndex, UObject* InObject, const bool& bAutoInserMissingObjects)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		if (!IsValid(InObject))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing a Geometry Input Object"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			UObject* InputObject = nullptr;
			int32 NumInputObjects = CurInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);
			if (AtIndex < NumInputObjects)
			{
				InputObject = CurInput->GetInputObjectAt(EHoudiniInputType::Geometry, AtIndex);
				if (InObject == InputObject)
					continue;
			}
			else if (bAutoInserMissingObjects)
			{
				CurInput->InsertInputObjectAt(EHoudiniInputType::Geometry, AtIndex);
			}
			else
			{
				continue;
			}
		
			UHoudiniInputObject* CurrentInputObjectWrapper = CurInput->GetHoudiniInputObjectAt(AtIndex);
			if (CurrentInputObjectWrapper)
				CurrentInputObjectWrapper->Modify();

			CurInput->Modify();

			CurInput->SetInputObjectAt(EHoudiniInputType::Geometry, AtIndex, InObject);
			CurInput->MarkChanged(true);
			
			// TODO: Not needed?
			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	// Drop Target: Static/Skeletal Mesh
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
	[
		SNew( SAssetDropTarget )
		.bSupportsMultiDrop(true)
		.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
		{
			for (auto& CurAssetData : InAssets)
			{
				if (UHoudiniInput::IsObjectAcceptable(EHoudiniInputType::Geometry, CurAssetData.GetAsset()))
					return true;
			}
			
			return false;
		})
		.OnAssetsDropped_Lambda([InInputs, InGeometryObjectIdx, UpdateGeometryObjectAt](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
		{
			int32 CurrentObjectIdx = InGeometryObjectIdx;
			for (auto& CurAssetData : InAssets)
			{
				UObject* Object = CurAssetData.GetAsset();
				if (!IsValid(Object))
					continue;

				if (!UHoudiniInput::IsObjectAcceptable(EHoudiniInputType::Geometry, Object))
					continue;

				// Update the object, inserting new one if necessary
				UpdateGeometryObjectAt(InInputs, CurrentObjectIdx++, Object, true);
			}
		})
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
		]
	];

	// Thumbnail : Static Mesh
	FText ParameterLabelText = FText::FromString(MainInput->GetLabel());
	
	TSharedPtr< SBorder > StaticMeshThumbnailBorder;
	HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
	[
		SAssignNew(StaticMeshThumbnailBorder, SBorder)
		.Padding(5.0f)
		.OnMouseDoubleClick_Lambda([MainInput, InGeometryObjectIdx](const FGeometry&, const FPointerEvent&)
		{
			UObject* InputObject = MainInput->GetInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
			if (GEditor && InputObject)
				GEditor->EditObject(InputObject);

			return FReply::Handled(); 
		})		
		[
			SNew(SBox)
			.WidthOverride(64)
			.HeightOverride(64)
			.ToolTipText(ParameterLabelText)
			[
			   StaticMeshThumbnail->MakeThumbnailWidget()
			]
		]
	];
	
	TWeakPtr<SBorder> WeakStaticMeshThumbnailBorder(StaticMeshThumbnailBorder);
	StaticMeshThumbnailBorder->SetBorderImage(TAttribute<const FSlateBrush *>::Create(
		TAttribute<const FSlateBrush *>::FGetter::CreateLambda([WeakStaticMeshThumbnailBorder]()
		{
			TSharedPtr<SBorder> ThumbnailBorder = WeakStaticMeshThumbnailBorder.Pin();
			if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
				return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
			else
				return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
		}
	)));
	
	FText MeshNameText = FText::GetEmpty();
	if (InputObject)
		MeshNameText = FText::FromString(InputObject->GetName());
	

	TSharedPtr<SVerticalBox> ComboAndButtonBox;
	HorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	.VAlign(VAlign_Center)
	[
		SAssignNew(ComboAndButtonBox, SVerticalBox)
	];

	// Add Combo box : Static Mesh
	TSharedPtr<SComboButton> StaticMeshComboButton;
	ComboAndButtonBox->AddSlot().FillHeight(1.0f).VAlign(VAlign_Center)
	[
		SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
		[
			SAssignNew(StaticMeshComboButton, SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
			.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
			.ContentPadding(2.0f)
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
				.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
				.Text(MeshNameText)
			]
		]
	];


	TWeakPtr<SComboButton> WeakStaticMeshComboButton(StaticMeshComboButton);
	StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
		[MainInput, InInputs, InGeometryObjectIdx, WeakStaticMeshComboButton, UpdateGeometryObjectAt]()
		{
			TArray< const UClass * > AllowedClasses = UHoudiniInput::GetAllowedClasses(EHoudiniInputType::Geometry);
			UObject* DefaultObj = MainInput->GetInputObjectAt(InGeometryObjectIdx);

			TArray< UFactory * > NewAssetFactories;
			return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
				FAssetData(DefaultObj),
				true,
				AllowedClasses,
				NewAssetFactories,
				FOnShouldFilterAsset(),
				FOnAssetSelected::CreateLambda(
					[InInputs, InGeometryObjectIdx, WeakStaticMeshComboButton, UpdateGeometryObjectAt](const FAssetData & AssetData)
					{
						TSharedPtr<SComboButton> ComboButton = WeakStaticMeshComboButton.Pin();
						if (ComboButton.IsValid())
						{
							ComboButton->SetIsOpen(false);
							UObject * Object = AssetData.GetAsset();
							UpdateGeometryObjectAt(InInputs, InGeometryObjectIdx, Object, false);
						}
					}
				),
				FSimpleDelegate::CreateLambda([]() {})
			);
		}
	));


	// Add buttons
	TSharedPtr<SHorizontalBox> ButtonHorizontalBox;
	ComboAndButtonBox->AddSlot()
	.FillHeight(1.0f)
	.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	.VAlign(VAlign_Center)
	[
		SAssignNew(ButtonHorizontalBox, SHorizontalBox)
	];

	// Create tooltip.
	FFormatNamedArguments Args;
	Args.Add( TEXT( "Asset" ), MeshNameText );
	FText StaticMeshTooltip = FText::Format(
		LOCTEXT("BrowseToSpecificAssetInContentBrowser",
			"Browse to '{Asset}' in Content Browser" ), Args );

	// Button : Use selected in content browser
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateLambda([InInputs, InGeometryObjectIdx, UpdateGeometryObjectAt]()
		{
		if (GEditor) 
		{
			TArray<FAssetData> CBSelections;
			GEditor->GetContentBrowserSelections(CBSelections);

			TArray<const UClass*> AllowedClasses = UHoudiniInput::GetAllowedClasses(EHoudiniInputType::Geometry);
			int32 CurrentObjectIdx = InGeometryObjectIdx;
			for (auto& CurAssetData : CBSelections)
			{
				UObject* Object = CurAssetData.GetAsset();
				if (!IsValid(Object))
					continue;

				if (!UHoudiniInput::IsObjectAcceptable(EHoudiniInputType::Geometry, Object))
					continue;

				// Update the object, inserting new one if necessary
				UpdateGeometryObjectAt(InInputs, CurrentObjectIdx++, Object, true);
			}
		}
		}), TAttribute< FText >(LOCTEXT("GeometryInputUseSelectedAssetFromCB", "Use Selected Asset from Content Browser")))
	];

	// Button : Browse Static Mesh
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding( 2.0f, 0.0f )
	.VAlign( VAlign_Center )
	[
		PropertyCustomizationHelpers::MakeBrowseButton(
			FSimpleDelegate::CreateLambda([MainInput, InGeometryObjectIdx]()
			{
				UObject* InputObject = MainInput->GetInputObjectAt(InGeometryObjectIdx);
				if (GEditor && InputObject)
				{
					TArray<UObject*> Objects;
					Objects.Add(InputObject);
					GEditor->SyncBrowserToObjects(Objects);
				}
			}),
			TAttribute< FText >( StaticMeshTooltip )
		)
	];

	// ButtonBox : Reset
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding( 2.0f, 0.0f )
	.VAlign( VAlign_Center )
	[
		SNew( SButton )
		.ToolTipText( LOCTEXT( "ResetToBase", "Reset to default static mesh" ) )
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ContentPadding( 0 )
		.Visibility( EVisibility::Visible )
		.OnClicked_Lambda( [UpdateGeometryObjectAt, InInputs, InGeometryObjectIdx]()
		{
			UpdateGeometryObjectAt(InInputs, InGeometryObjectIdx, nullptr, false);
			return FReply::Handled();
		})
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
		]
	];

	// Insert/Delete/Duplicate
	ButtonHorizontalBox->AddSlot()
	.Padding( 1.0f )
	.VAlign( VAlign_Center )
	.AutoWidth()
	[
		PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
		FExecuteAction::CreateLambda( [ InInputs, InGeometryObjectIdx, MainInput ]() 
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: insert a Geometry Input Object"),
			MainInput->GetOuter());
			// Insert
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->InsertInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
			}
		} ),
		FExecuteAction::CreateLambda([MainInput, InInputs, InGeometryObjectIdx]()
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: delete a Geometry Input Object"),
				MainInput->GetOuter());

			// Delete
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->DeleteInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);

				if (GEditor)
					GEditor->RedrawAllViewports();
			}
		} ),
		FExecuteAction::CreateLambda([InInputs, InGeometryObjectIdx, MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: duplicate a Geometry Input Object"),
				MainInput->GetOuter());
				
			// Duplicate
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->DuplicateInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
			}
		} ) )
	];
	
	// TRANSFORM OFFSET EXPANDER
	{
		TSharedPtr<SButton> ExpanderArrow;
		TSharedPtr<SImage> ExpanderImage;
		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SAssignNew( ExpanderArrow, SButton )
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ClickMethod( EButtonClickMethod::MouseDown )
				.Visibility( EVisibility::Visible )
				.OnClicked(FOnClicked::CreateLambda([InInputs, InGeometryObjectIdx, MainInput, &CategoryBuilder]()
				{
					if (!IsValidWeakPointer(MainInput))
						return FReply::Handled();;

					FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: duplicate a Geometry Input Object"),
					MainInput->GetOuter());

					// Expand transform
					for (auto CurInput : InInputs)
					{
						if (!IsValidWeakPointer(CurInput))
							continue;

						CurInput->Modify();	
						CurInput->OnTransformUIExpand(InGeometryObjectIdx);
					}

					// TODO: Not needed?
					if (CategoryBuilder.IsParentLayoutValid())
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();

					return FReply::Handled();
				}))
				[
					SAssignNew(ExpanderImage, SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("GeoInputTransform", "Transform Offset") )
				.ToolTipText( LOCTEXT( "GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
		];

		TWeakPtr<SButton> WeakExpanderArrow(ExpanderArrow);
		// Set delegate for image
		ExpanderImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([InGeometryObjectIdx, MainInput, WeakExpanderArrow]() 
			{
				FName ResourceName;
				TSharedPtr<SButton> ExpanderArrowPtr = WeakExpanderArrow.Pin();
				if (MainInput->IsTransformUIExpanded(InGeometryObjectIdx))
				{
					ResourceName = ExpanderArrowPtr.IsValid() && ExpanderArrowPtr->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
				}
				else
				{
					ResourceName = ExpanderArrowPtr.IsValid() && ExpanderArrowPtr->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
				}
				return FEditorStyle::GetBrush(ResourceName);
			}
		)));
	}

	// Lambda for changing the transform values
	auto ChangeTransformOffsetAt = [&](const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputTransformChange", "Houdini Input: Changing Transform offset"),
			InInputs[0]->GetOuter());

		bool bChanged = true;
		for (int Idx = 0; Idx < InInputs.Num(); Idx++)
		{
			if (!InInputs[Idx].IsValid())
				continue;

			UHoudiniInputObject* InputObject = InInputs[Idx]->GetHoudiniInputObjectAt(AtIndex);
			if (InputObject)
				InputObject->Modify();

			bChanged &= InInputs[Idx]->SetTransformOffsetAt(Value, AtIndex, PosRotScaleIndex, XYZIndex);
		}

		if (bChanged && DoChange)
		{
			// Mark the values as changed to trigger an update
			for (int Idx = 0; Idx < InInputs.Num(); Idx++)
			{
				InInputs[Idx]->MarkChanged(true);
			}
		}
		else
		{
			// Cancel the transaction
			Transaction.Cancel();
		}
	};

	// Get Visibility of reset buttons
	bool bResetButtonVisiblePosition = false;
	bool bResetButtonVisibleRotation = false;
	bool bResetButtonVisibleScale = false;
	for (auto & CurInput : InInputs)
	{
		if (!IsValidWeakPointer(CurInput))
			continue;

		FTransform* CurTransform = CurInput->GetTransformOffset(InGeometryObjectIdx);
		if (!CurTransform)
			continue;

		if (CurTransform->GetLocation() != FVector::ZeroVector)
			bResetButtonVisiblePosition = true;

		FRotator Rotator = CurTransform->Rotator();
		if (Rotator.Roll != 0 || Rotator.Pitch != 0 || Rotator.Yaw != 0)
			bResetButtonVisibleRotation = true;

		if (CurTransform->GetScale3D() != FVector::OneVector)
			bResetButtonVisibleScale = true;
	}

	auto ChangeTransformOffsetUniformlyAt = [InGeometryObjectIdx, InInputs, ChangeTransformOffsetAt](const float & Val, const int32& PosRotScaleIndex)
	{
		ChangeTransformOffsetAt(Val, InGeometryObjectIdx, PosRotScaleIndex, 0, true, InInputs);
		ChangeTransformOffsetAt(Val, InGeometryObjectIdx, PosRotScaleIndex, 1, true, InInputs);
		ChangeTransformOffsetAt(Val, InGeometryObjectIdx, PosRotScaleIndex, 2, true, InInputs);
	};

	// TRANSFORM OFFSET
	if (MainInput->IsTransformUIExpanded(InGeometryObjectIdx))
	{
		// Position
		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("GeoInputTranslate", "T") )
				.ToolTipText( LOCTEXT( "GeoInputTranslateTooltip", "Translate" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew( SVectorInputBox )
				.bColorAxisLabels( true )
				.AllowSpin(true)
				.X(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetPositionOffsetX, InGeometryObjectIdx)))
				.Y(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetPositionOffsetY, InGeometryObjectIdx)))
				.Z(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetPositionOffsetZ, InGeometryObjectIdx)))
				.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 0, true, InInputs); })
				.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 1, true, InInputs); })
				.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 2, true, InInputs); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// Lock Button (not visible)
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.Visibility(EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("GenericLock"))
					]
				]
				// Reset Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
					.Visibility(bResetButtonVisiblePosition ? EVisibility::Visible : EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
					.OnClicked_Lambda([MainInput, ChangeTransformOffsetUniformlyAt, &CategoryBuilder]()
					{
						ChangeTransformOffsetUniformlyAt(0.0f, 0);
						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();

						return FReply::Handled();
					})
				]
			]
		];
		
		// Rotation
		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding(1.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("GeoInputRotate", "R") )
				.ToolTipText( LOCTEXT( "GeoInputRotateTooltip", "Rotate" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew( SRotatorInputBox )
				.AllowSpin( true )
				.bColorAxisLabels( true )
				.Roll(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetRotationOffsetRoll, InGeometryObjectIdx)))
				.Pitch(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetRotationOffsetPitch, InGeometryObjectIdx)))
				.Yaw(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetRotationOffsetYaw, InGeometryObjectIdx)))
				.OnRollCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 1, 0, true, InInputs); })
				.OnPitchCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 1, 1, true, InInputs); })
				.OnYawCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 1, 2, true, InInputs); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// Lock Button (Not visible)
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.Visibility(EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("GenericLock"))
					]
				]
				// Reset Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
					.Visibility(bResetButtonVisibleRotation ? EVisibility::Visible : EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
					.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, MainInput, &CategoryBuilder]()
					{
						ChangeTransformOffsetUniformlyAt(0.0f, 1);
						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();

						return FReply::Handled();
					})
				]
			]
		];
		
		// Scale
		bool bLocked = false;
		if (HoudiniInputObject)
			bLocked = HoudiniInputObject->IsUniformScaleLocked();

		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "GeoInputScale", "S" ) )
				.ToolTipText( LOCTEXT( "GeoInputScaleTooltip", "Scale" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew( SVectorInputBox )
				.bColorAxisLabels( true )
				.X(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetScaleOffsetX, InGeometryObjectIdx)))
				.Y(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetScaleOffsetY, InGeometryObjectIdx)))
				.Z(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetScaleOffsetZ, InGeometryObjectIdx)))
				.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{
					if (bLocked) 
						ChangeTransformOffsetUniformlyAt(Val, 2);
					else
						ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 2, 0, true, InInputs);
				})
				.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ 
					if (bLocked)
						ChangeTransformOffsetUniformlyAt(Val, 2);
					else
						ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 2, 1, true, InInputs);
				})
				.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{
					if (bLocked)
						ChangeTransformOffsetUniformlyAt(Val, 2);
					else
						ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 2, 2, true, InInputs);
				})
			]
		
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox) 
				// Lock Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
				[	
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ToolTipText(HoudiniInputObject ? 
						LOCTEXT("GeoInputLockButtonToolTip", "When locked, scales uniformly based on the current xyz scale values so the input object maintains its shape in each direction when scaled") : 
						LOCTEXT("GeoInputLockButtonToolTipNoObject", "No input object selected"))
					.ClickMethod(EButtonClickMethod::MouseDown)
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(bLocked ? FEditorStyle::GetBrush("GenericLock") : FEditorStyle::GetBrush("GenericUnlock"))
					]
					.OnClicked_Lambda([InInputs, MainInput, InGeometryObjectIdx, HoudiniInputObject, &CategoryBuilder]() 
					{
						for (auto & CurInput : InInputs)
						{
							if (!IsValidWeakPointer(CurInput))
								continue;

							UHoudiniInputObject*CurInputObject = CurInput->GetHoudiniInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);

							if (!IsValid(CurInputObject))
								continue;

							CurInputObject->SwitchUniformScaleLock();
						}

						if (HoudiniInputObject)
						{
							if (CategoryBuilder.IsParentLayoutValid())
								CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						}

						return FReply::Handled();
					})
				]
				// Reset Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")				
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
					.Visibility(bResetButtonVisibleScale ? EVisibility::Visible : EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))		
					]
					.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, MainInput, &CategoryBuilder]()
					{
						ChangeTransformOffsetUniformlyAt(1.0f, 2);
						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();

						return FReply::Handled();
					})
				]
			]
		];
	}
}

void FHoudiniInputDetails::Helper_CreateGeometryCollectionWidget(IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FPlatformTypes::int32& InGeometryCollectionObjectIdx,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool, TSharedRef<SVerticalBox> VerticalBox)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	// Access the object used in the corresponding geometry input
	UHoudiniInputObject* HoudiniInputObject = MainInput->GetHoudiniInputObjectAt(EHoudiniInputType::GeometryCollection, InGeometryCollectionObjectIdx);
	UObject* InputObject = HoudiniInputObject ? HoudiniInputObject->GetObject() : nullptr;

	// Create thumbnail for this static mesh.
	TSharedPtr<FAssetThumbnail> GCThumbnail = MakeShareable(
		new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

	// Lambda for adding new geometry input objects
	auto UpdateGeometryCollectionObjectAt = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const int32& AtIndex, UObject* InObject)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		if (!IsValid(InObject))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing a Geometry Input Object"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			UObject* InputObject = CurInput->GetInputObjectAt(EHoudiniInputType::GeometryCollection, AtIndex);
			if (InObject == InputObject)
				continue;
		
			UHoudiniInputObject* CurrentInputObjectWrapper = CurInput->GetHoudiniInputObjectAt(AtIndex);
			if (CurrentInputObjectWrapper)
				CurrentInputObjectWrapper->Modify();

			CurInput->Modify();

			CurInput->SetInputObjectAt(EHoudiniInputType::GeometryCollection, AtIndex, InObject);
			CurInput->MarkChanged(true);
			
			// TODO: Not needed?
			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	// Drop Target: Geometry collection
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
	[
		SNew( SAssetDropTarget )
		.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
		{
			return UHoudiniInput::IsObjectAcceptable(EHoudiniInputType::GeometryCollection, InAssets[0].GetAsset());
		})
		.OnAssetsDropped_Lambda([InInputs, InGeometryCollectionObjectIdx, UpdateGeometryCollectionObjectAt](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
		{
			UObject* InObject = InAssets[0].GetAsset();
			return UpdateGeometryCollectionObjectAt(InInputs, InGeometryCollectionObjectIdx, InObject);
		})
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
		]
	];

	// Thumbnail : Geometry Collectionh
	FText ParameterLabelText = FText::FromString(MainInput->GetLabel());
	
	TSharedPtr< SBorder > GeometryCollectionThumbnailBorder;
	HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
	[
		SAssignNew(GeometryCollectionThumbnailBorder, SBorder)
		.Padding(5.0f)
		.OnMouseDoubleClick_Lambda([MainInput, InGeometryCollectionObjectIdx](const FGeometry&, const FPointerEvent&)
		{
			UObject* InputObject = MainInput->GetInputObjectAt(EHoudiniInputType::GeometryCollection, InGeometryCollectionObjectIdx);
			if (GEditor && InputObject)
				GEditor->EditObject(InputObject);

			return FReply::Handled(); 
		})		
		[
			SNew(SBox)
			.WidthOverride(64)
			.HeightOverride(64)
			.ToolTipText(ParameterLabelText)
			[
			   GCThumbnail->MakeThumbnailWidget()
			]
		]
	];
	
	TWeakPtr<SBorder> WeakGeometryCollectionThumbnailBorder(GeometryCollectionThumbnailBorder);
	GeometryCollectionThumbnailBorder->SetBorderImage(TAttribute<const FSlateBrush *>::Create(
		TAttribute<const FSlateBrush *>::FGetter::CreateLambda([WeakGeometryCollectionThumbnailBorder]()
		{
			TSharedPtr<SBorder> ThumbnailBorder = WeakGeometryCollectionThumbnailBorder.Pin();
			if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
				return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
			else
				return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
		}
	)));
	
	FText InputObjectNameText = FText::GetEmpty();
	if (InputObject)
		InputObjectNameText = FText::FromString(InputObject->GetName());
	
	TSharedPtr<SVerticalBox> ComboAndButtonBox;
	HorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	.VAlign(VAlign_Center)
	[
		SAssignNew(ComboAndButtonBox, SVerticalBox)
	];

	// Add Combo box : Geometry collection
	// Combo button text
	TSharedPtr<SComboButton> GeometryCollectionComboButton;
	ComboAndButtonBox->AddSlot().FillHeight(1.0f).VAlign(VAlign_Center)
	[
		SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
		[
			SAssignNew(GeometryCollectionComboButton, SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
			.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
			.ContentPadding(2.0f)
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
				.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
				.Text(InputObjectNameText)
			]
		]
	];

	// Combo button Asset picker
	TWeakPtr<SComboButton> WeakGeometryCollectionComboButton(GeometryCollectionComboButton);
	GeometryCollectionComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
		[MainInput, InInputs, InGeometryCollectionObjectIdx, WeakGeometryCollectionComboButton, UpdateGeometryCollectionObjectAt]()
		{
			TArray< const UClass * > AllowedClasses = UHoudiniInput::GetAllowedClasses(EHoudiniInputType::GeometryCollection);
			UObject* DefaultObj = MainInput->GetInputObjectAt(InGeometryCollectionObjectIdx);

			TArray< UFactory * > NewAssetFactories;
			return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
				FAssetData(DefaultObj),
				true,
				AllowedClasses,
				NewAssetFactories,
				FOnShouldFilterAsset(),
				FOnAssetSelected::CreateLambda(
					[InInputs, InGeometryCollectionObjectIdx, WeakGeometryCollectionComboButton, UpdateGeometryCollectionObjectAt](const FAssetData & AssetData)
					{
						TSharedPtr<SComboButton> ComboButton = WeakGeometryCollectionComboButton.Pin();
						if (ComboButton.IsValid())
						{
							ComboButton->SetIsOpen(false);
							UObject * Object = AssetData.GetAsset();
							UpdateGeometryCollectionObjectAt(InInputs, InGeometryCollectionObjectIdx, Object);
						}
					}
				),
				FSimpleDelegate::CreateLambda([]() {})
			);
		}
	));

	// Add buttons
	TSharedPtr<SHorizontalBox> ButtonHorizontalBox;
	ComboAndButtonBox->AddSlot()
	.FillHeight(1.0f)
	.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	.VAlign(VAlign_Center)
	[
		SAssignNew(ButtonHorizontalBox, SHorizontalBox)
	];

	// Create tooltip.
	FFormatNamedArguments Args;
	Args.Add( TEXT( "Asset" ), InputObjectNameText );
	FText GeometryCollectionTooltip = FText::Format(
		LOCTEXT("BrowseToSpecificAssetInContentBrowser",
			"Browse to '{Asset}' in Content Browser" ), Args );

	// Button : Use selected in content browser
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateLambda([InInputs, InGeometryCollectionObjectIdx, UpdateGeometryCollectionObjectAt]()
		{
		if (GEditor) 
		{
			TArray<FAssetData> CBSelections;
			GEditor->GetContentBrowserSelections(CBSelections);

			// Get the first selected geometry collection object
			UObject* Object = nullptr;
			for (auto & CurAssetData : CBSelections) 
			{
				if (CurAssetData.AssetClass != UGeometryCollection::StaticClass()->GetFName())
					continue;

				Object = CurAssetData.GetAsset();
				break;
			}

			if (IsValid(Object)) 
			{
				UpdateGeometryCollectionObjectAt(InInputs, InGeometryCollectionObjectIdx, Object);
			}
		}
		}), TAttribute< FText >(LOCTEXT("GeometryInputUseSelectedAssetFromCB", "Use Selected Asset from Content Browser")))
	];

	// Button : Browse Geometry collection
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding( 2.0f, 0.0f )
	.VAlign( VAlign_Center )
	[
		PropertyCustomizationHelpers::MakeBrowseButton(
			FSimpleDelegate::CreateLambda([MainInput, InGeometryCollectionObjectIdx]()
			{
				UObject* InputObject = MainInput->GetInputObjectAt(InGeometryCollectionObjectIdx);
				if (GEditor && InputObject)
				{
					TArray<UObject*> Objects;
					Objects.Add(InputObject);
					GEditor->SyncBrowserToObjects(Objects);
				}
			}),
			TAttribute< FText >( GeometryCollectionTooltip )
		)
	];

	// ButtonBox : Reset
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding( 2.0f, 0.0f )
	.VAlign( VAlign_Center )
	[
		SNew( SButton )
		.ToolTipText( LOCTEXT( "ResetToBase", "Reset to default geometry collection" ) )
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ContentPadding( 0 )
		.Visibility( EVisibility::Visible )
		.OnClicked_Lambda( [UpdateGeometryCollectionObjectAt, InInputs, InGeometryCollectionObjectIdx]()
		{
			UpdateGeometryCollectionObjectAt(InInputs, InGeometryCollectionObjectIdx, nullptr);
			return FReply::Handled();
		})
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
		]
	];

	// Insert/Delete/Duplicate
	ButtonHorizontalBox->AddSlot()
	.Padding( 1.0f )
	.VAlign( VAlign_Center )
	.AutoWidth()
	[
		PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
		FExecuteAction::CreateLambda( [ InInputs, InGeometryCollectionObjectIdx, MainInput ]() 
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: insert a Geometry Input Object"),
			MainInput->GetOuter());
			// Insert
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->InsertInputObjectAt(EHoudiniInputType::GeometryCollection, InGeometryCollectionObjectIdx);
			}
		} ),
		FExecuteAction::CreateLambda([MainInput, InInputs, InGeometryCollectionObjectIdx]()
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: delete a GeometryCollection Input Object"),
				MainInput->GetOuter());

			// Delete
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->DeleteInputObjectAt(EHoudiniInputType::GeometryCollection, InGeometryCollectionObjectIdx);

				if (GEditor)
					GEditor->RedrawAllViewports();
			}
		} ),
		FExecuteAction::CreateLambda([InInputs, InGeometryCollectionObjectIdx, MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: duplicate a GeometryCollection Input Object"),
				MainInput->GetOuter());
				
			// Duplicate
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->DuplicateInputObjectAt(EHoudiniInputType::GeometryCollection, InGeometryCollectionObjectIdx);
			}
		} ) )
	];
	
	// TRANSFORM OFFSET EXPANDER
	{
		TSharedPtr<SButton> ExpanderArrow;
		TSharedPtr<SImage> ExpanderImage;
		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SAssignNew( ExpanderArrow, SButton )
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ClickMethod( EButtonClickMethod::MouseDown )
				.Visibility( EVisibility::Visible )
				.OnClicked(FOnClicked::CreateLambda([InInputs, InGeometryCollectionObjectIdx, MainInput, &CategoryBuilder]()
				{
					if (!IsValidWeakPointer(MainInput))
						return FReply::Handled();;

					FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: duplicate a Geometry Input Object"),
					MainInput->GetOuter());

					// Expand transform
					for (auto CurInput : InInputs)
					{
						if (!IsValidWeakPointer(CurInput))
							continue;

						CurInput->Modify();	
						CurInput->OnTransformUIExpand(InGeometryCollectionObjectIdx);
					}

					// TODO: Not needed?
					if (CategoryBuilder.IsParentLayoutValid())
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();

					return FReply::Handled();
				}))
				[
					SAssignNew(ExpanderImage, SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("GeoInputTransform", "Transform Offset") )
				.ToolTipText( LOCTEXT( "GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
		];

		TWeakPtr<SButton> WeakExpanderArrow(ExpanderArrow);
		// Set delegate for image
		ExpanderImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([InGeometryCollectionObjectIdx, MainInput, WeakExpanderArrow]() 
			{
				FName ResourceName;
				TSharedPtr<SButton> ExpanderArrowPtr = WeakExpanderArrow.Pin();
				if (MainInput->IsTransformUIExpanded(InGeometryCollectionObjectIdx))
				{
					ResourceName = ExpanderArrowPtr.IsValid() && ExpanderArrowPtr->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
				}
				else
				{
					ResourceName = ExpanderArrowPtr.IsValid() && ExpanderArrowPtr->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
				}
				return FEditorStyle::GetBrush(ResourceName);
			}
		)));
	}

	// Lambda for changing the transform values
	auto ChangeTransformOffsetAt = [&](const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputTransformChange", "Houdini Input: Changing Transform offset"),
			InInputs[0]->GetOuter());

		bool bChanged = true;
		for (int Idx = 0; Idx < InInputs.Num(); Idx++)
		{
			if (!InInputs[Idx].IsValid())
				continue;

			UHoudiniInputObject* InputObject = InInputs[Idx]->GetHoudiniInputObjectAt(AtIndex);
			if (InputObject)
				InputObject->Modify();

			bChanged &= InInputs[Idx]->SetTransformOffsetAt(Value, AtIndex, PosRotScaleIndex, XYZIndex);
		}

		if (bChanged && DoChange)
		{
			// Mark the values as changed to trigger an update
			for (int Idx = 0; Idx < InInputs.Num(); Idx++)
			{
				InInputs[Idx]->MarkChanged(true);
			}
		}
		else
		{
			// Cancel the transaction
			Transaction.Cancel();
		}
	};

	// Get Visibility of reset buttons
	bool bResetButtonVisiblePosition = false;
	bool bResetButtonVisibleRotation = false;
	bool bResetButtonVisibleScale = false;
	for (auto & CurInput : InInputs)
	{
		if (!IsValidWeakPointer(CurInput))
			continue;

		FTransform* CurTransform = CurInput->GetTransformOffset(InGeometryCollectionObjectIdx);
		if (!CurTransform)
			continue;

		if (CurTransform->GetLocation() != FVector::ZeroVector)
			bResetButtonVisiblePosition = true;

		FRotator Rotator = CurTransform->Rotator();
		if (Rotator.Roll != 0 || Rotator.Pitch != 0 || Rotator.Yaw != 0)
			bResetButtonVisibleRotation = true;

		if (CurTransform->GetScale3D() != FVector::OneVector)
			bResetButtonVisibleScale = true;
	}

	auto ChangeTransformOffsetUniformlyAt = [InGeometryCollectionObjectIdx, InInputs, ChangeTransformOffsetAt](const float & Val, const int32& PosRotScaleIndex)
	{
		ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, PosRotScaleIndex, 0, true, InInputs);
		ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, PosRotScaleIndex, 1, true, InInputs);
		ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, PosRotScaleIndex, 2, true, InInputs);
	};

	// TRANSFORM OFFSET
	if (MainInput->IsTransformUIExpanded(InGeometryCollectionObjectIdx))
	{
		// Position
		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("GeoInputTranslate", "T") )
				.ToolTipText( LOCTEXT( "GeoInputTranslateTooltip", "Translate" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew( SVectorInputBox )
				.bColorAxisLabels( true )
				.AllowSpin(true)
				.X(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetPositionOffsetX, InGeometryCollectionObjectIdx)))
				.Y(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetPositionOffsetY, InGeometryCollectionObjectIdx)))
				.Z(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetPositionOffsetZ, InGeometryCollectionObjectIdx)))
				.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 0, 0, true, InInputs); })
				.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 0, 1, true, InInputs); })
				.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 0, 2, true, InInputs); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// Lock Button (not visible)
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.Visibility(EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("GenericLock"))
					]
				]
				// Reset Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
					.Visibility(bResetButtonVisiblePosition ? EVisibility::Visible : EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
					.OnClicked_Lambda([MainInput, ChangeTransformOffsetUniformlyAt, &CategoryBuilder]()
					{
						ChangeTransformOffsetUniformlyAt(0.0f, 0);
						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();

						return FReply::Handled();
					})
				]
			]
		];
		
		// Rotation
		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding(1.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("GeoInputRotate", "R") )
				.ToolTipText( LOCTEXT( "GeoInputRotateTooltip", "Rotate" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew( SRotatorInputBox )
				.AllowSpin( true )
				.bColorAxisLabels( true )
				.Roll(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetRotationOffsetRoll, InGeometryCollectionObjectIdx)))
				.Pitch(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetRotationOffsetPitch, InGeometryCollectionObjectIdx)))
				.Yaw(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetRotationOffsetYaw, InGeometryCollectionObjectIdx)))
				.OnRollCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 1, 0, true, InInputs); })
				.OnPitchCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 1, 1, true, InInputs); })
				.OnYawCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 1, 2, true, InInputs); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// Lock Button (Not visible)
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.Visibility(EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("GenericLock"))
					]
				]
				// Reset Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
					.Visibility(bResetButtonVisibleRotation ? EVisibility::Visible : EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
					.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, MainInput, &CategoryBuilder]()
					{
						ChangeTransformOffsetUniformlyAt(0.0f, 1);
						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();

						return FReply::Handled();
					})
				]
			]
		];
		
		// Scale
		bool bLocked = false;
		if (HoudiniInputObject)
			bLocked = HoudiniInputObject->IsUniformScaleLocked();

		VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 1.0f )
			.VAlign( VAlign_Center )
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "GeoInputScale", "S" ) )
				.ToolTipText( LOCTEXT( "GeoInputScaleTooltip", "Scale" ) )
				.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew( SVectorInputBox )
				.bColorAxisLabels( true )
				.X(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetScaleOffsetX, InGeometryCollectionObjectIdx)))
				.Y(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetScaleOffsetY, InGeometryCollectionObjectIdx)))
				.Z(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput.Get(), &UHoudiniInput::GetScaleOffsetZ, InGeometryCollectionObjectIdx)))
				.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{
					if (bLocked) 
						ChangeTransformOffsetUniformlyAt(Val, 2);
					else
						ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 2, 0, true, InInputs);
				})
				.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ 
					if (bLocked)
						ChangeTransformOffsetUniformlyAt(Val, 2);
					else
						ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 2, 1, true, InInputs);
				})
				.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{
					if (bLocked)
						ChangeTransformOffsetUniformlyAt(Val, 2);
					else
						ChangeTransformOffsetAt(Val, InGeometryCollectionObjectIdx, 2, 2, true, InInputs);
				})
			]
		
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox) 
				// Lock Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
				[	
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ToolTipText(HoudiniInputObject ? 
						LOCTEXT("GeoInputLockButtonToolTip", "When locked, scales uniformly based on the current xyz scale values so the input object maintains its shape in each direction when scaled") : 
						LOCTEXT("GeoInputLockButtonToolTipNoObject", "No input object selected"))
					.ClickMethod(EButtonClickMethod::MouseDown)
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(bLocked ? FEditorStyle::GetBrush("GenericLock") : FEditorStyle::GetBrush("GenericUnlock"))
					]
					.OnClicked_Lambda([InInputs, MainInput, InGeometryCollectionObjectIdx, HoudiniInputObject, &CategoryBuilder]() 
					{
						for (auto & CurInput : InInputs)
						{
							if (!IsValidWeakPointer(CurInput))
								continue;

							UHoudiniInputObject*CurInputObject = CurInput->GetHoudiniInputObjectAt(EHoudiniInputType::GeometryCollection, InGeometryCollectionObjectIdx);

							if (!IsValid(CurInputObject))
								continue;

							CurInputObject->SwitchUniformScaleLock();
						}

						if (HoudiniInputObject)
						{
							if (CategoryBuilder.IsParentLayoutValid())
								CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						}

						return FReply::Handled();
					})
				]
				// Reset Button
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")				
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
					.Visibility(bResetButtonVisibleScale ? EVisibility::Visible : EVisibility::Hidden)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))		
					]
					.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, MainInput, &CategoryBuilder]()
					{
						ChangeTransformOffsetUniformlyAt(1.0f, 2);
						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();

						return FReply::Handled();
					})
				]
			]
		];
	}
}

void
FHoudiniInputDetails::AddAssetInputUI(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;
	
	// Houdini Asset Picker Widget
	{
		FMenuBuilder MenuBuilder = Helper_CreateHoudiniAssetPickerWidget(InInputs);

		VerticalBox->AddSlot()
		.Padding(2.0f, 2.0f, 5.0f, 2.0f)
		.AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];	
	}	

	// Button : Clear Selection
	{
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
		auto IsClearButtonEnabled = [MainInput]() 
		{
			if (!IsValidWeakPointer(MainInput))
				return false;

			TArray<UHoudiniInputObject*>* AssetInputObjectsArray = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Asset);

			if (!AssetInputObjectsArray)
				return false;

			if (AssetInputObjectsArray->Num() <= 0)
				return false;

			return true;
		};

		FOnClicked OnClearSelect = FOnClicked::CreateLambda([InInputs, MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChangeClear", "Houdini Input: Clearing asset input selection"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				TArray<UHoudiniInputObject*>* AssetInputObjectsArray = CurrentInput->GetHoudiniInputObjectArray(EHoudiniInputType::Asset);
				if (!AssetInputObjectsArray)
					continue;

				CurrentInput->Modify();

				AssetInputObjectsArray->Empty();
				CurrentInput->MarkChanged(true);
			}

			return FReply::Handled();
		});

		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				// Button :  Clear Selection
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("ClearSelection", "Clear Selection"))
				.ToolTipText(LOCTEXT("ClearSelectionTooltip", "Clear input selection"))
				.IsEnabled_Lambda(IsClearButtonEnabled)
				.OnClicked(OnClearSelect)
			]
		];

		// Do not enable select all/clear select when selection has been started and details are locked
		//HorizontalBox->SetEnabled(!bDetailsLocked);
	}


}

void
FHoudiniInputDetails::AddCurveInputUI(IDetailCategoryBuilder& CategoryBuilder, TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::Curve);
	
	// lambda for inserting an input Houdini curve.
	auto InsertAnInputCurve = [MainInput, &CategoryBuilder](const int32& NewInputCount) 
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(MainInput->GetOuter());
		if (!IsValid(OuterHAC))
			return;

		// Do not insert input object when the HAC does not finish cooking
		EHoudiniAssetState CurrentHACState = OuterHAC->GetAssetState();
		if (CurrentHACState >= EHoudiniAssetState::PreCook && CurrentHACState<= EHoudiniAssetState::Processing)
			return;

		// Clear the to be inserted object array, which records the pointers of the input objects to be inserted.
		MainInput->LastInsertedInputs.Empty();
		// Record the pointer of the object to be inserted before transaction for undo the insert action.
		bool bBlueprintStructureModified = false;
		UHoudiniInputHoudiniSplineComponent* NewInput = MainInput->CreateHoudiniSplineInput(nullptr, true, false, bBlueprintStructureModified);
		MainInput->LastInsertedInputs.Add(NewInput);

		FHoudiniEngineEditorUtils::ReselectSelectedActors();

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini input: Adding curve input."));
		MainInput->Modify();

		// Modify the MainInput.
		MainInput->GetHoudiniInputObjectArray(MainInput->GetInputType())->Add(NewInput);

		MainInput->SetInputObjectsNumber(EHoudiniInputType::Curve, NewInputCount);

		if (bBlueprintStructureModified)
		{
			FHoudiniEngineRuntimeUtils::MarkBlueprintAsStructurallyModified(OuterHAC);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	// Add Rot/Scale attribute checkbox
	FText TooltipText = LOCTEXT("HoudiniEngineCurveAddRotScaleAttributesTooltip", "If enabled, rot and scale attributes will be added per to the input curve on each control points.");
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineCurveAddRotScaleAttributesLabel", "Add rot & scale Attributes"))
			.ToolTipText(TooltipText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			//.MinDesiredWidth(160.f)
		]
		.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
		{
			const bool bChecked = (NewState == ECheckBoxState::Checked);
			for (auto& CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				CurrentInput->SetAddRotAndScaleAttributes(bChecked);
			}
		})
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;
			
			return MainInput->IsAddRotAndScaleAttributesEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.ToolTipText(TooltipText)
	];

	// Use Legacy Input curves
	FText LegacyInputCurveTooltipText = LOCTEXT("HoudiniEngineLegacyInputCurvesTooltip", "If enabled, will use the deprecated curve::1.0 node as the curve input.");
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
	SNew(SCheckBox)
	.Content()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("HoudiniEngineLegacyInputCurvesLabel", "Use Legacy Input Curves"))
		.ToolTipText(LegacyInputCurveTooltipText)
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	]
	.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& CurrentInput : InInputs)
		{
			if (!IsValidWeakPointer(CurrentInput))
				continue;

			CurrentInput->SetUseLegacyInputCurve(bChecked);
		}
	})
	.IsChecked_Lambda([MainInput]()
	{
		if (!IsValidWeakPointer(MainInput))
			return ECheckBoxState::Unchecked;
			
		return MainInput->IsUseLegacyInputCurvesEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.ToolTipText(TooltipText)
	];
	
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("NumArrayItemsFmt", "{0} elements"), FText::AsNumber(NumInputObjects)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([InsertAnInputCurve, NumInputObjects]()
			{
				return InsertAnInputCurve(NumInputObjects+1);
				//return SetCurveInputObjectsCount(NumInputObjects+1);
			}),

			LOCTEXT("AddInputCurve", "Adds a Curve Input"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateLambda([InInputs, MainInput, &CategoryBuilder]()
			{
				TArray<UHoudiniInputObject*> * CurveInputComponentArray = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);

				// Detach all curves before deleting.
				for (int n = CurveInputComponentArray->Num() - 1; n >= 0; n--)
				{
					UHoudiniInputHoudiniSplineComponent* HoudiniInput = 
						Cast <UHoudiniInputHoudiniSplineComponent>((*CurveInputComponentArray)[n]);
					if (!IsValid(HoudiniInput))
						continue;

					UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
					if (!IsValid(HoudiniSplineComponent))
						continue;

					FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					HoudiniSplineComponent->DetachFromComponent(DetachTransRules);
				}

				// Clear the insert objects buffer before transaction.
				MainInput->LastInsertedInputs.Empty();

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Delete curve inputs."));
				MainInput->Modify();

				bool bBlueprintStructureModified = false;

				// actual delete.
				for (int n = CurveInputComponentArray->Num() - 1; n >= 0; n--)
				{
					UHoudiniInputHoudiniSplineComponent* HoudiniInput =
						Cast <UHoudiniInputHoudiniSplineComponent>((*CurveInputComponentArray)[n]);
					if (!IsValid(HoudiniInput))
						continue;

					UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
					if (!IsValid(HoudiniSplineComponent))
						continue;

					MainInput->RemoveSplineFromInputObject(HoudiniInput, bBlueprintStructureModified);

					MainInput->DeleteInputObjectAt(EHoudiniInputType::Curve, n);
				}

				MainInput->SetInputObjectsNumber(EHoudiniInputType::Curve, 0);

				if (bBlueprintStructureModified)
				{
					UActorComponent* OuterComponent = Cast<UActorComponent>(MainInput->GetOuter());
					FHoudiniEngineRuntimeUtils::MarkBlueprintAsStructurallyModified(OuterComponent);
				}

				if (CategoryBuilder.IsParentLayoutValid())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		
			}),
			LOCTEXT("EmptyInputsCurve", "Removes All Curve Inputs"), true)
		]
		+ SHorizontalBox::Slot().FillWidth(80.f).MaxWidth(80.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ResetCurveOffsetStr", "Reset Offset"))
			.OnClicked_Lambda([MainInput]()->FReply 
			{
				MainInput->ResetDefaultCurveOffset();
				return FReply::Handled(); 
			})
		]
	];

	//UHoudiniSplineComponent* SplineCompBeingEdited = nullptr;
	TSharedPtr<FHoudiniSplineComponentVisualizer> HouSplineComponentVisualizer;
	if (GUnrealEd)
	{
		TSharedPtr<FComponentVisualizer> Visualizer =
			GUnrealEd->FindComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());

		HouSplineComponentVisualizer = StaticCastSharedPtr<FHoudiniSplineComponentVisualizer>(Visualizer);
	}


	for (int n = 0; n < NumInputObjects; n++) 
	{
		Helper_CreateCurveWidget(CategoryBuilder, InInputs, n, AssetThumbnailPool ,VerticalBox, HouSplineComponentVisualizer);
	}
}

void
FHoudiniInputDetails::Helper_CreateCurveWidget(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const int32& InCurveObjectIdx,
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool,
	TSharedRef< SVerticalBox > VerticalBox,
	TSharedPtr<FHoudiniSplineComponentVisualizer> HouSplineComponentVisualizer)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	UHoudiniAssetComponent * OuterHAC = Cast<UHoudiniAssetComponent>(MainInput->GetOuter());
	if (!IsValid(OuterHAC))
		return;

	auto GetHoudiniSplineComponentAtIndex = [](const TWeakObjectPtr<UHoudiniInput>& Input, int32 Index)
	{
		UHoudiniSplineComponent* FoundHoudiniSplineComponent = nullptr;
		if (!IsValidWeakPointer(Input))
			return FoundHoudiniSplineComponent;

		// Get the TArray ptr to the curve objects in this input
		TArray<UHoudiniInputObject*> * CurveInputComponentArray = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputComponentArray)
			return FoundHoudiniSplineComponent;

		if (!CurveInputComponentArray->IsValidIndex(Index))
			return FoundHoudiniSplineComponent;

		// Access the object used in the corresponding Houdini curve input
		UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[Index];
		UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
			Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

		FoundHoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();

		return FoundHoudiniSplineComponent;
	};


	// Get the TArray ptr to the curve objects in this input
	TArray<UHoudiniInputObject*> * CurveInputComponentArray = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!CurveInputComponentArray)
		return;

	if (!CurveInputComponentArray->IsValidIndex(InCurveObjectIdx))
		return;

	// Access the object used in the corresponding Houdini curve input
	UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[InCurveObjectIdx];
	UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
		Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

	UHoudiniSplineComponent * HoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();
	if (!HoudiniSplineComponent)
		return;

	bool bIsLegacyCurve = HoudiniSplineComponent->IsLegacyInputCurve();

	FString HoudiniSplineName = HoudiniSplineComponent->GetHoudiniSplineName();

	// Editable label for the current Houdini curve
	TSharedPtr <SHorizontalBox> LabelHorizontalBox;
	VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SAssignNew(LabelHorizontalBox, SHorizontalBox)
	];


	TSharedPtr <SEditableText> LabelBlock;
	LabelHorizontalBox->AddSlot()
		.Padding(0, 15, 0, 2)
		.MaxWidth(150.f)
		.FillWidth(150.f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		[	
			SNew(SBox).HeightOverride(20.f).WidthOverride(200.f).VAlign(VAlign_Center)
			[
				SAssignNew(LabelBlock, SEditableText).Text(FText::FromString(HoudiniSplineName))
				.OnTextCommitted_Lambda([HoudiniSplineComponent](FText NewText, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::Type::OnEnter)
					{
						HoudiniSplineComponent->SetHoudiniSplineName(NewText.ToString());
					}
				})
			]		
		];

	// 'Editing...' TextBlock showing if this component is being edited
	TSharedPtr<SCurveEditingTextBlock> EditingTextBlock;
	LabelHorizontalBox->AddSlot()
		.Padding(0, 15, 0, 2)
		.MaxWidth(55.f)
		.FillWidth(55.f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		[
			SNew(SBox).HeightOverride(20.f).WidthOverride(75.f).VAlign(VAlign_Center)
			[
				SAssignNew(EditingTextBlock, SCurveEditingTextBlock).Text(LOCTEXT("HoudiniCurveInputEditingLabel", "(editing...)"))
			]
		];

	EditingTextBlock->HoudiniSplineComponent = HoudiniSplineComponent;
	EditingTextBlock->HoudiniSplineComponentVisualizer = HouSplineComponentVisualizer;

	// Lambda for deleting the current curve input
	auto DeleteHoudiniCurveAtIndex = [InInputs, InCurveObjectIdx, OuterHAC, CurveInputComponentArray, &CategoryBuilder]()
	{
		if (!IsValid(OuterHAC))
			return;

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeDeleteACurve", "Houdini Input: Deleting a curve input"),
			OuterHAC);

		int MainInputCurveArraySize = CurveInputComponentArray->Num();
		for (auto & Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			Input->Modify();

			TArray<UHoudiniInputObject*>* InputObjectArr = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!InputObjectArr)
				continue;

			if (!InputObjectArr->IsValidIndex(InCurveObjectIdx))
				continue;

			if (MainInputCurveArraySize != InputObjectArr->Num())
				continue;

			UHoudiniInputHoudiniSplineComponent* HoudiniInput =
				Cast<UHoudiniInputHoudiniSplineComponent>((*InputObjectArr)[InCurveObjectIdx]);
			if (!IsValid(HoudiniInput))
				return;

			UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
			if (!HoudiniSplineComponent)
				return;

			// Detach the spline component before delete.
			FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
			HoudiniSplineComponent->DetachFromComponent(DetachTransRules);

			// This input is marked changed when an input component is deleted.
			Input->DeleteInputObjectAt(EHoudiniInputType::Curve, InCurveObjectIdx);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	// Add delete button UI
	LabelHorizontalBox->AddSlot().Padding(0, 2, 0, 2).HAlign(HAlign_Right).VAlign(VAlign_Bottom).AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([DeleteHoudiniCurveAtIndex]()
	{
		return DeleteHoudiniCurveAtIndex();
	}))
		];


	TSharedPtr <SHorizontalBox> HorizontalBox = NULL;
	VerticalBox->AddSlot().Padding(0, 2).AutoHeight()[SAssignNew(HorizontalBox, SHorizontalBox)];

	// Closed check box
	// Lambda returning a closed state
	auto IsCheckedClosedCurve = [HoudiniSplineComponent]()
	{
		if (!IsValid(HoudiniSplineComponent))
			return ECheckBoxState::Unchecked;

		return HoudiniSplineComponent->IsClosedCurve() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing Closed state
	auto CheckStateChangedClosedCurve = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](ECheckBoxState NewState)
	{
		if (!IsValid(OuterHAC))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);
		
		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeClosed", "Houdini Input: Changing Curve Closed"),
			OuterHAC);

		for (auto & Input : InInputs) 
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);

			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->IsClosedCurve() == bNewState)
				continue;

			HoudiniSplineComponent->Modify();

			HoudiniSplineComponent->SetClosedCurve(bNewState);
			HoudiniSplineComponent->MarkChanged(true);
		}
	};

	// Add Closed check box UI
	TSharedPtr <SCheckBox> CheckBoxClosed = NULL;
	HorizontalBox->AddSlot().Padding(0, 2).AutoWidth()
		[
			SAssignNew(CheckBoxClosed, SCheckBox).Content()
			[
				SNew(STextBlock).Text(LOCTEXT("ClosedCurveCheckBox", "Closed"))
				.ToolTipText(LOCTEXT("ClosedCurveCheckboxTip", "Close this input curve."))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
	.IsChecked_Lambda([IsCheckedClosedCurve]()
	{
		return IsCheckedClosedCurve();
	})
		.OnCheckStateChanged_Lambda([CheckStateChangedClosedCurve](ECheckBoxState NewState)
	{
		return CheckStateChangedClosedCurve(NewState);
	})
		];

	// Reversed check box
	// Lambda returning a reversed state
	auto IsCheckedReversedCurve = [HoudiniSplineComponent]()
	{
		if (!IsValid(HoudiniSplineComponent))
			return ECheckBoxState::Unchecked;

		return HoudiniSplineComponent->IsReversed() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing reversed state
	auto CheckStateChangedReversedCurve = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](ECheckBoxState NewState)
	{
		if (!IsValid(OuterHAC))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeReversed", "Houdini Input: Changing Curve Reversed"),
			OuterHAC);

		for (auto & Input : InInputs) 
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->IsReversed() == bNewState)
				continue;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetReversed(bNewState);
			HoudiniSplineComponent->MarkChanged(true);
		}
	};

	// Add reversed check box UI
	TSharedPtr <SCheckBox> CheckBoxReversed = NULL;
	HorizontalBox->AddSlot()
		.Padding(2, 2)
		.AutoWidth()
		[
			SAssignNew(CheckBoxReversed, SCheckBox).Content()
			[
				SNew(STextBlock).Text(LOCTEXT("ReversedCurveCheckBox", "Reversed"))
				.ToolTipText(LOCTEXT("ReversedCurveCheckboxTip", "Reverse this input curve."))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
	.IsChecked_Lambda([IsCheckedReversedCurve]()
	{
		return IsCheckedReversedCurve();
	})
		.OnCheckStateChanged_Lambda([CheckStateChangedReversedCurve](ECheckBoxState NewState)
	{
		return CheckStateChangedReversedCurve(NewState);
	})
		];

	// Visible check box
	// Lambda returning a visible state
	auto IsCheckedVisibleCurve = [HoudiniSplineComponent]()
	{
		if (!IsValid(HoudiniSplineComponent))
			return ECheckBoxState::Unchecked;

		return HoudiniSplineComponent->IsHoudiniSplineVisible() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing visible state
	auto CheckStateChangedVisibleCurve = [GetHoudiniSplineComponentAtIndex, InInputs, OuterHAC, InCurveObjectIdx](ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);

		for (auto & Input : InInputs) 
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->IsHoudiniSplineVisible() == bNewState)
				return;

			HoudiniSplineComponent->SetHoudiniSplineVisible(bNewState);
		}

		if (GEditor)
			GEditor->RedrawAllViewports();
		
	};

	// Add visible check box UI
	TSharedPtr <SCheckBox> CheckBoxVisible = NULL;
	HorizontalBox->AddSlot().Padding(2, 2).AutoWidth()
		[
			SAssignNew(CheckBoxVisible, SCheckBox).Content()
			[
				SNew(STextBlock).Text(LOCTEXT("VisibleCurveCheckBox", "Visible"))
				.ToolTipText(LOCTEXT("VisibleCurveCheckboxTip", "Set the visibility of this curve."))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
	.IsChecked_Lambda([IsCheckedVisibleCurve]()
	{
		return IsCheckedVisibleCurve();
	})
		.OnCheckStateChanged_Lambda([CheckStateChangedVisibleCurve](ECheckBoxState NewState)
	{
		return CheckStateChangedVisibleCurve(NewState);
	})
		];

	// Curve type comboBox
	// Lambda for changing Houdini curve type
	auto OnCurveTypeChanged = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](TSharedPtr<FString> InNewChoice)
	{
		if (!IsValid(OuterHAC))
			return;

		if (!InNewChoice.IsValid())
			return;

		EHoudiniCurveType NewInputType = UHoudiniInput::StringToHoudiniCurveType(*InNewChoice.Get());

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeType", "Houdini Input: Changing Curve Type"),
			OuterHAC);

		for (auto & Input : InInputs) 
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->GetCurveType() == NewInputType)
				continue;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetCurveType(NewInputType);

			// Set the curve order to at least 4 if it is nurbs or bezier
			int32 CurveOrder = HoudiniSplineComponent->GetCurveOrder();
			if (CurveOrder < 4 && (NewInputType == EHoudiniCurveType::Nurbs || NewInputType == EHoudiniCurveType::Bezier))
				HoudiniSplineComponent->SetCurveOrder(4);
			else if (NewInputType == EHoudiniCurveType::Polygon)
				HoudiniSplineComponent->SetCurveOrder(2);
			
			HoudiniSplineComponent->MarkChanged(true);
		}
	};

	// Lambda for getting Houdini curve type 
	auto GetCurveTypeText = [HoudiniSplineComponent]()
	{
		return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(HoudiniSplineComponent->GetCurveType()));
	};

	// Add curve type combo box UI
	TSharedPtr<SHorizontalBox> CurveTypeHorizontalBox;
	VerticalBox->AddSlot()
		.Padding(0, 2, 2, 0)
		.AutoHeight()
		[
			SAssignNew(CurveTypeHorizontalBox, SHorizontalBox)
		];

	// Add curve type label UI
	CurveTypeHorizontalBox->AddSlot().Padding(0, 10, 0, 2).AutoWidth()
		[
			SNew(STextBlock).Text(LOCTEXT("CurveTypeText", "Curve Type     "))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

	TSharedPtr < SComboBox < TSharedPtr < FString > > > ComboBoxCurveType;
	CurveTypeHorizontalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.FillWidth(150.f)
		.MaxWidth(150.f)
		[
			SAssignNew(ComboBoxCurveType, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveTypeChoiceLabels())
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveTypeChoiceLabels())[(int)HoudiniSplineComponent->GetCurveType()])
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
	{
		FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
		return SNew(STextBlock)
			.Text(ChoiceEntryText)
			.ToolTipText(ChoiceEntryText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	})
		.OnSelectionChanged_Lambda([OnCurveTypeChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
	{
		return OnCurveTypeChanged(NewChoice);
	})
		[
			SNew(STextBlock)
			.Text_Lambda([GetCurveTypeText]()
	{
		return GetCurveTypeText();
	})
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		];

	// Houdini curve method combo box
	// Lambda for changing Houdini curve method
	auto OnCurveMethodChanged = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](TSharedPtr<FString> InNewChoice)
	{
		if (!IsValid(OuterHAC))
			return;

		if (!InNewChoice.IsValid())
			return;

		EHoudiniCurveMethod NewInputMethod = UHoudiniInput::StringToHoudiniCurveMethod(*InNewChoice.Get());

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeMethod", "Houdini Input: Changing Curve Method"),
			OuterHAC);

		for (auto & Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->GetCurveMethod() == NewInputMethod)
				return;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetCurveMethod(NewInputMethod);

			HoudiniSplineComponent->MarkChanged(true);
		}
	};

	// Lambda for getting Houdini curve method 
	auto GetCurveMethodText = [HoudiniSplineComponent]()
	{
		return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(HoudiniSplineComponent->GetCurveMethod()));
	};
	
	// Add curve method combo box UI
	TSharedPtr< SHorizontalBox > CurveMethodHorizontalBox;
	VerticalBox->AddSlot().Padding(0, 2, 2, 0).AutoHeight()[SAssignNew(CurveMethodHorizontalBox, SHorizontalBox)];

	// Add curve method label UI
	CurveMethodHorizontalBox->AddSlot().Padding(0, 10, 0, 2).AutoWidth()
		[
			SNew(STextBlock).Text(LOCTEXT("CurveMethodText", "Curve Method "))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

	TSharedPtr < SComboBox < TSharedPtr < FString > > > ComboBoxCurveMethod;
	CurveMethodHorizontalBox->AddSlot().Padding(2, 2, 5, 2).FillWidth(150.f).MaxWidth(150.f)
		[
			SAssignNew(ComboBoxCurveMethod, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveMethodChoiceLabels())
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveMethodChoiceLabels())[(int)HoudiniSplineComponent->GetCurveMethod()])
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
	{
		FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
		return SNew(STextBlock)
			.Text(ChoiceEntryText)
			.ToolTipText(ChoiceEntryText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	})
		.OnSelectionChanged_Lambda([OnCurveMethodChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
	{
		return OnCurveMethodChanged(NewChoice);
	})
		[
			SNew(STextBlock)
			.Text_Lambda([GetCurveMethodText]()
	{
		return GetCurveMethodText();
	})
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		];

	if (!bIsLegacyCurve)
	{
		// Order
		TSharedPtr<SNumericEntryBox<int>> NumericEntryBox;
		int32 Idx = 0;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(0, 10, 0, 2)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveOrder", "Curve Order"))
				.ToolTipText(LOCTEXT("CurveOrderTooltip", "Curve order of the curve. Only used for Bezier or NURBs curves. Must be a value between 2 and 11."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<int>)
				.AllowSpin(true)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.MinValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN)
				.MaxValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MAX)
				.MinSliderValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN)
				.MaxSliderValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MAX)
				.Value(HoudiniSplineComponent->GetCurveOrder())
				.OnValueChanged_Lambda([GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](int Val) 
				{
					if (!IsValid(OuterHAC))
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniChangeCurveOrder", "Houdini Input: Changing curve order"),
						OuterHAC);

					for (auto & Input : InInputs)
					{
						if (!IsValidWeakPointer(Input))
							continue;

						UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
						if (!IsValid(HoudiniSplineComponent))
							continue;

						HoudiniSplineComponent->Modify();
						HoudiniSplineComponent->SetCurveOrder(Val);
						HoudiniSplineComponent->MarkChanged(true);
					}
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("OrderToDefault", "Reset to default value."))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(HoudiniSplineComponent->GetCurveOrder() != HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN ? EVisibility::Visible : EVisibility::Hidden)
				.OnClicked_Lambda([GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC]()
				{
					if (!IsValid(OuterHAC))
						return FReply::Handled();

					// Record a transaction for undo/redo.
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniCurveInputChangeMethod", "Houdini Input: Changing Curve Method"),
						OuterHAC);

					for (auto & Input : InInputs)
					{
						if (!IsValidWeakPointer(Input))
							continue;

						UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
						if (!IsValid(HoudiniSplineComponent))
							continue;

						if (HoudiniSplineComponent->GetCurveOrder() == HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN)
							continue;

						HoudiniSplineComponent->Modify();
						HoudiniSplineComponent->SetCurveOrder(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN);
						HoudiniSplineComponent->MarkChanged(true);
					}

					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
	}



	// Curve Parameterization
	if (!bIsLegacyCurve)
	{
		// Houdini curve breakpoint parameterization combo box
		// Lambda for changing Houdini curve breakpoint parameterization
		auto OnBreakpointParameterizationChanged = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](TSharedPtr<FString> InNewChoice)
		{
			if (!IsValid(OuterHAC))
				return;

			if (!InNewChoice.IsValid())
				return;

			EHoudiniCurveBreakpointParameterization NewInputMethod = UHoudiniInput::StringToHoudiniCurveBreakpointParameterization(*InNewChoice.Get());

			// Record a transaction for undo/redo.
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniCurveInputChangeBreakpointParameterization", "Houdini Input: Changing Curve Breakpoint Parameterization"),
				OuterHAC);

			for (auto & Input : InInputs)
			{
				if (!IsValidWeakPointer(Input))
					continue;

				UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
				if (!IsValid(HoudiniSplineComponent))
					continue;

				if (HoudiniSplineComponent->GetCurveBreakpointParameterization() == NewInputMethod)
					return;

				HoudiniSplineComponent->Modify();
				HoudiniSplineComponent->SetCurveBreakpointParameterization(NewInputMethod);
				HoudiniSplineComponent->MarkChanged(true);
			}
		};

		// Lambda for getting Houdini curve method 
		auto GetCurveBreakpointParameterizationText = [HoudiniSplineComponent]()
		{
			return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveBreakpointParameterizationToString(HoudiniSplineComponent->GetCurveBreakpointParameterization()));
		};

		const bool CurveBreakpointsVisible = HoudiniSplineComponent->GetCurveMethod() == EHoudiniCurveMethod::Breakpoints;

		// Add curve method combo box UI
		TSharedPtr< SHorizontalBox > CurveBreakpointParamHorizontalBox;
		VerticalBox->AddSlot().Padding(0, 2, 2, 0).AutoHeight()[SAssignNew(CurveBreakpointParamHorizontalBox, SHorizontalBox)];

		CurveBreakpointParamHorizontalBox->SetEnabled(CurveBreakpointsVisible);

		// Add curve method label UI
		CurveBreakpointParamHorizontalBox->AddSlot().Padding(0, 10, 0, 2).AutoWidth()
		[
			SNew(STextBlock).Text(LOCTEXT("CurveBreakpointParameterizationText", "Breakpoint Parameterization"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

		TSharedPtr < SComboBox < TSharedPtr < FString > > > ComboBoxCurveBreakpointParam;
		CurveBreakpointParamHorizontalBox->AddSlot().Padding(2, 2, 5, 2).FillWidth(150.f).MaxWidth(150.f)
		[
		SAssignNew(ComboBoxCurveBreakpointParam, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveBreakpointParameterizationChoiceLabels())
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveBreakpointParameterizationChoiceLabels())[(int32)HoudiniSplineComponent->GetCurveBreakpointParameterization()])
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
		{
			FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
			return SNew(STextBlock)
			.Text(ChoiceEntryText)
			.ToolTipText(ChoiceEntryText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
		})
		.OnSelectionChanged_Lambda([OnBreakpointParameterizationChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
		{	
			return OnBreakpointParameterizationChanged(NewChoice);
		})
		[
		SNew(STextBlock)
		.Text_Lambda([GetCurveBreakpointParameterizationText]()
		{
			return GetCurveBreakpointParameterizationText();
		})
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		];
	}
	
	auto BakeInputCurveLambda = [](const TArray<TWeakObjectPtr<UHoudiniInput>>& Inputs, int32 Index, bool bBakeToBlueprint)
	{
		for (auto & NextInput : Inputs)
		{
			if (!IsValidWeakPointer(NextInput))
				continue;

			UHoudiniAssetComponent * OuterHAC = Cast<UHoudiniAssetComponent>(NextInput->GetOuter());
			if (!IsValid(OuterHAC))
				continue;

			AActor * OwnerActor = OuterHAC->GetOwner();
			if (!IsValid(OwnerActor))
				continue;

			TArray<UHoudiniInputObject*> * CurveInputComponentArray = NextInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurveInputComponentArray)
				continue;

			if (!CurveInputComponentArray->IsValidIndex(Index))
				continue;

			UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[Index];
			UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
				Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

			if (!IsValid(HoudiniSplineInputObject))
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();
			if (!IsValid(HoudiniSplineComponent))
				continue;

			FHoudiniPackageParams PackageParams;
			PackageParams.BakeFolder = OuterHAC->BakeFolder.Path;
			PackageParams.HoudiniAssetName = OuterHAC->GetName();
			PackageParams.GeoId = NextInput->GetAssetNodeId();
			PackageParams.PackageMode = EPackageMode::Bake;
			PackageParams.ObjectId = Index;
			PackageParams.ObjectName = OwnerActor->GetName() + "InputHoudiniSpline" + FString::FromInt(Index);

			if (bBakeToBlueprint) 
			{
				FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToBlueprint(
					OuterHAC,
					HoudiniSplineComponent,
					PackageParams,
					OwnerActor->GetWorld(), OwnerActor->GetActorTransform());
			}
			else
			{
				FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
					OuterHAC,
					HoudiniSplineComponent,
					PackageParams,
					OwnerActor->GetWorld(), OwnerActor->GetActorTransform());
			}
		}

		return FReply::Handled();
	};

	// Add input curve bake button
	TSharedPtr< SHorizontalBox > InputCurveBakeHorizontalBox;
	VerticalBox->AddSlot().Padding(0, 2, 2, 0).AutoHeight()[SAssignNew(InputCurveBakeHorizontalBox, SHorizontalBox)];
	VerticalBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().MaxWidth(110.f)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("HoudiniInputCurveBakeToActorButton", "Bake to Actor"))
		.IsEnabled(true)
		.OnClicked_Lambda([InInputs, InCurveObjectIdx, BakeInputCurveLambda]()
		{
			return BakeInputCurveLambda(InInputs, InCurveObjectIdx, false);
		})
		.ToolTipText(LOCTEXT("HoudiniInputCurveBakeToActorButtonToolTip", "Bake this input curve to Actor"))
		]

	+ SHorizontalBox::Slot().MaxWidth(110.f)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("HoudiniInputCurveBakeToBPButton", "Bake to Blueprint"))
		.IsEnabled(true)
		.OnClicked_Lambda([InInputs, InCurveObjectIdx, BakeInputCurveLambda]()
		{
			return BakeInputCurveLambda(InInputs, InCurveObjectIdx, true);
		})
		.ToolTipText(LOCTEXT("HoudiniInputCurveBakeToBPButtonToolTip", "Bake this input curve to Blueprint"))
		]
	];

	// Do we actually need to set enable the UI components?
	if (MainInput->GetInputType() == EHoudiniInputType::Curve) 
	{
		LabelBlock->SetEnabled(true);
		CheckBoxClosed->SetEnabled(true);
		CheckBoxReversed->SetEnabled(true);
		CheckBoxVisible->SetEnabled(true);
		ComboBoxCurveType->SetEnabled(true);
		ComboBoxCurveMethod->SetEnabled(true);
	}
	else 
	{
		LabelBlock->SetEnabled(false);
		CheckBoxClosed->SetEnabled(false);
		CheckBoxReversed->SetEnabled(false);
		CheckBoxVisible->SetEnabled(false);
		ComboBoxCurveType->SetEnabled(false);
		ComboBoxCurveMethod->SetEnabled(false);
	}
}

void
FHoudiniInputDetails::AddLandscapeInputUI(TSharedRef<SVerticalBox> VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current KeepWorldTransform state
	auto IsCheckedUpdateInputLandscape = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetUpdateInputLandscape() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing KeepWorldTransform state
	auto CheckStateChangedUpdateInputLandscape = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniLandscapeInputChangedUpdate", "Houdini Input: Changing Keep World Transform"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (bNewState == CurInput->GetUpdateInputLandscape())
				continue;

			CurInput->Modify();

			UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(CurInput->GetOuter());
			if (!HAC)
				continue;

			TArray<UHoudiniInputObject*>* LandscapeInputObjects = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
			if (!LandscapeInputObjects)
				continue;

			for (UHoudiniInputObject* NextInputObj : *LandscapeInputObjects)
			{
				UHoudiniInputLandscape* CurrentInputLandscape = Cast<UHoudiniInputLandscape>(NextInputObj);
				if (!CurrentInputLandscape)
					continue;

				ALandscapeProxy* CurrentInputLandscapeProxy = CurrentInputLandscape->GetLandscapeProxy();
				if (!CurrentInputLandscapeProxy)
					continue;

				if (bNewState)
				{
					// We want to update this landscape data directly, start by backing it up to image files in the temp folder
					FString BackupBaseName = HAC->TemporaryCookFolder.Path
						+ TEXT("/")
						+ CurrentInputLandscapeProxy->GetName()
						+ TEXT("_")
						+ HAC->GetComponentGUID().ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);

					// We need to cache the input landscape to a file
					FHoudiniLandscapeTranslator::BackupLandscapeToImageFiles(BackupBaseName, CurrentInputLandscapeProxy);
					
					// Cache its transform on the input
					CurrentInputLandscape->CachedInputLandscapeTraqnsform = CurrentInputLandscapeProxy->ActorToWorld();

					HAC->SetMobility(EComponentMobility::Static);
					CurrentInputLandscapeProxy->AttachToComponent(HAC, FAttachmentTransformRules::KeepWorldTransform);
				}
				else
				{
					// We are not updating this input landscape anymore, detach it and restore its backed-up values
					CurrentInputLandscapeProxy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

					// Restore the input landscape's backup data
					FHoudiniLandscapeTranslator::RestoreLandscapeFromImageFiles(CurrentInputLandscapeProxy);

					// Reapply the source Landscape's transform
					CurrentInputLandscapeProxy->SetActorTransform(CurrentInputLandscape->CachedInputLandscapeTraqnsform);

					// TODO: 
					// Clear the input obj map?
				}
			}

			CurInput->bUpdateInputLandscape = (NewState == ECheckBoxState::Checked);
			CurInput->MarkChanged(true);
		}
	};

	// CheckBox : Update Input Landscape Data
	TSharedPtr< SCheckBox > CheckBoxUpdateInput;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew( CheckBoxUpdateInput, SCheckBox).Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeUpdateInputCheckbox", "Update Input Landscape Data"))
			.ToolTipText(LOCTEXT("LandscapeUpdateInputTooltip", "If enabled, the input landscape's data will be updated instead of creating a new landscape Actor"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([IsCheckedUpdateInputLandscape, MainInput]()
		{
			return IsCheckedUpdateInputLandscape(MainInput);
		})
		.OnCheckStateChanged_Lambda([CheckStateChangedUpdateInputLandscape, InInputs](ECheckBoxState NewState)
		{
			return CheckStateChangedUpdateInputLandscape(InInputs, NewState);
		})
	];
	
	// ------------------------
	// Landscape: Actor picker
	// ------------------------
	FMenuBuilder MenuBuilder = Helper_CreateLandscapePickerWidget(InInputs);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		MenuBuilder.MakeWidget()
	];

	{
		// -------------------------------------
		// Button: Recommit
		// -------------------------------------
		auto OnButtonRecommitClicked = [InInputs]()
		{
			for (auto CurrentInput : InInputs)
			{
				TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = CurrentInput->GetHoudiniInputObjectArray(CurrentInput->GetInputType());
				if (!LandscapeInputObjectsArray)
					continue;

				for (UHoudiniInputObject* NextLandscapeInput : *LandscapeInputObjectsArray)
				{
					if (!NextLandscapeInput)
						continue;

					NextLandscapeInput->MarkChanged(true);
					NextLandscapeInput->MarkTransformChanged(true);
				}

				CurrentInput->MarkChanged(true);
			}
		
			return FReply::Handled();
		};
		
		// -------------------------------------
		// Button : Clear Selection
		// -------------------------------------
		auto IsClearButtonEnabled = [MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return false;

			if (MainInput->GetInputType() != EHoudiniInputType::Landscape)
				return false;

			TArray<UHoudiniInputObject*>* MainInputObjectsArray = MainInput->GetHoudiniInputObjectArray(MainInput->GetInputType());
			if (!MainInputObjectsArray)
				return false;

			if (MainInputObjectsArray->Num() <= 0)
				return false;

			return true;
		};

		auto OnButtonClearClicked = [InInputs]()
		{
			if (InInputs.Num() <= 0)
				return FReply::Handled();

			const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			if (MainInput->GetInputType() != EHoudiniInputType::Landscape)
				return FReply::Handled();

			TArray<UHoudiniInputObject*>* MainInputObjectsArray = MainInput->GetHoudiniInputObjectArray(MainInput->GetInputType());
			if (!MainInputObjectsArray)
				return FReply::Handled();

			if (MainInputObjectsArray->Num() <= 0)
				return FReply::Handled();

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniLandscapeInputChangeExportNormalizedUVs", "Houdini Input: Clearing landscape input."),
				MainInput->GetOuter());

			for (auto & CurInput : InInputs) 
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
				if (!LandscapeInputObjectsArray)
					continue;

				if (LandscapeInputObjectsArray->Num() <= 0)
					continue;

				CurInput->MarkChanged(true);
				CurInput->Modify();

				LandscapeInputObjectsArray->Empty();
			}

			return FReply::Handled();
		};

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1, 2, 4, 2)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("LandscapeInputRecommit", "Recommit"))
				.ToolTipText(LOCTEXT("LandscapeInputRecommitTooltip", "Recommits the Landscape to Houdini."))
				.OnClicked_Lambda(OnButtonRecommitClicked)
			]
			+ SHorizontalBox::Slot()
			.Padding(1, 2, 4, 2)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("ClearSelection", "Clear Selection"))
				.ToolTipText(LOCTEXT("ClearSelectionTooltip", "Clear input selection"))
				.IsEnabled_Lambda(IsClearButtonEnabled)
				.OnClicked_Lambda(OnButtonClearClicked)
			]
		];
	}

	// -----------------------------
	// Landscape: Advanced Settings
	// -----------------------------

	// Vertical box for 'advanced' options
	TSharedRef<SVerticalBox> Landscape_VerticalBox = SNew(SVerticalBox);

	TSharedRef<SExpandableArea> Landscape_Expandable = SNew(SExpandableArea)
		.AreaTitle( LOCTEXT("LandscapeAdvancedSettings", "Advanced") )
		.InitiallyCollapsed(!MainInput->bLandscapeUIAdvancedIsExpanded)
		.OnAreaExpansionChanged_Lambda( [MainInput]( const bool& bIsExpanded)
		{
			MainInput->bLandscapeUIAdvancedIsExpanded = bIsExpanded;
		})
		.BodyContent()
		[
			Landscape_VerticalBox
		];

	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()[Landscape_Expandable];
	
	const FMargin ItemPadding(0.0f, 2.0f, 0.0f, 2.f);
	// Checkboxes : Export landscape as Heightfield/Mesh/Points
	{
		Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeExportAs", "Export Landscape As"))
			.ToolTipText(LOCTEXT("LandscapeExportAsToolTip", "Choose the type of data you want the ladscape to be exported to:\n * Heightfield\n * Mesh\n * Points"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];
	
		TSharedPtr <SUniformGridPanel> ButtonOptionsPanel;
		Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
		[
			SAssignNew(ButtonOptionsPanel, SUniformGridPanel)
		];

		auto IsCheckedExportAs = [](const TWeakObjectPtr<UHoudiniInput>& Input, const EHoudiniLandscapeExportType& LandscapeExportType)
		{
			if (!IsValidWeakPointer(Input))
				return ECheckBoxState::Unchecked;

			if (Input->GetLandscapeExportType() == LandscapeExportType)
				return ECheckBoxState::Checked;
			else
				return ECheckBoxState::Unchecked;
		};

		auto CheckStateChangedExportAs = [](const TWeakObjectPtr<UHoudiniInput>& Input, const EHoudiniLandscapeExportType& LandscapeExportType)
		{
			if (!IsValidWeakPointer(Input))
				return false;

			if (Input->GetLandscapeExportType() == LandscapeExportType)
				return false;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape export type."),
				Input->GetOuter());
			Input->Modify();

			Input->SetLandscapeExportType(LandscapeExportType);
			Input->SetHasLandscapeExportTypeChanged(true);
			Input->MarkChanged(true);

			TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
			if (!LandscapeInputObjectsArray)
				return true;

			for (UHoudiniInputObject *NextInputObj : *LandscapeInputObjectsArray)
			{
				if (!NextInputObj)
					continue;
				NextInputObj->MarkChanged(true);
			}

			return true;
		};
		
		// Heightfield
		FText HeightfieldTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heightfield.");
		ButtonOptionsPanel->AddSlot(0, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
			.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
			{
				return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Heightfield);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
			{
				for(auto CurrentInput : InInputs)
					CheckStateChangedExportAs(CurrentInput, EHoudiniLandscapeExportType::Heightfield);
			})
			.ToolTipText(HeightfieldTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("ClassIcon.LandscapeComponent"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(2, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeExportAsHeightfieldCheckbox", "Heightfield"))
					.ToolTipText(HeightfieldTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];

		// Mesh
		FText MeshTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a mesh.");
		ButtonOptionsPanel->AddSlot(1, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
			.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
			{
				return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Mesh);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
					CheckStateChangedExportAs(CurrentInput, EHoudiniLandscapeExportType::Mesh);
			})
			.ToolTipText(MeshTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("ClassIcon.StaticMeshComponent"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(2, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeExportAsMeshCheckbox", "Mesh"))
					.ToolTipText(MeshTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];

		// Points
		FText PointsTooltip = LOCTEXT("LandscapeExportAsPointsTooltip", "If enabled, the landscape will be exported to Houdini as points.");
		ButtonOptionsPanel->AddSlot(2, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.End")
			.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
			{
				return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Points);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
					CheckStateChangedExportAs(CurrentInput, EHoudiniLandscapeExportType::Points);
			})
			.ToolTipText(PointsTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Static"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(2, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeExportAsPointsCheckbox", "Points"))
					.ToolTipText(PointsTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
	}

	// CheckBox : Export selected components only
	{
		TSharedPtr< SCheckBox > CheckBoxExportSelected;
		Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
		[
			SAssignNew(CheckBoxExportSelected, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeSelectedCheckbox", "Export Selected Landscape Components Only"))
				.ToolTipText(LOCTEXT("LandscapeSelectedTooltip", "If enabled, only the selected Landscape Components will be exported."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->bLandscapeExportSelectionOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniLandscapeInputChangeExportSelectionOnly", "Houdini Input: Changing Landscape export only selected component."),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->bLandscapeExportSelectionOnly)
						continue;

					CurrentInput->Modify();
					CurrentInput->bLandscapeExportSelectionOnly = bNewState;
					CurrentInput->UpdateLandscapeInputSelection();

					CurrentInput->MarkChanged(true);

				}
			})
		];
	}

	// Checkbox:  auto select components
	{		
		TSharedPtr< SCheckBox > CheckBoxAutoSelectComponents;
		Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
		[
			SAssignNew(CheckBoxAutoSelectComponents, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AutoSelectComponentCheckbox", "Auto-select component in asset bounds"))
				.ToolTipText(LOCTEXT("AutoSelectComponentCheckboxTooltip", "If enabled, when no Landscape components are curremtly selected, the one within the asset's bounding box will be exported."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->bLandscapeAutoSelectComponent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniLandscapeInputChangeAutoSelectComponent", "Houdini Input: Changing Landscape input auto-selects components."),
				MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->bLandscapeAutoSelectComponent)
						continue;

					CurrentInput->Modify();

					CurrentInput->bLandscapeAutoSelectComponent = bNewState;
					CurrentInput->UpdateLandscapeInputSelection();
					CurrentInput->MarkChanged(true);
				}
			})
		];

		// Enable only when exporting selection	or when exporting heighfield (for now)
		bool bEnable = false;
		for (auto CurrentInput : InInputs)
		{
			if (!MainInput->bLandscapeExportSelectionOnly)
				continue;

			bEnable = true;
			break;
		}
		CheckBoxAutoSelectComponents->SetEnabled(bEnable);
	}

	// -------------------------------------
	// Landscape: Update component selection
	// -------------------------------------
	{
		auto OnButtonUpdateComponentSelection = [InInputs, MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniRecommitSelectedComponents", "Houdini Input: Recommit selected landscapes."),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				CurrentInput->UpdateLandscapeInputSelection();

				CurrentInput->Modify();
				CurrentInput->MarkChanged(true);
			}

			return FReply::Handled();
		};

		auto IsLandscapeExportEnabled = [MainInput, InInputs]()
		{
			bool bEnable = false;
			for (auto CurrentInput : InInputs)
			{
				if (!MainInput->bLandscapeExportSelectionOnly)
					continue;

				bEnable = true;
				break;
			}
			
			return bEnable;
		};
		
		Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(1, 2, 4, 2)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("LandscapeInputUpdateSelectedComponents", "Update Selected Components"))
				.ToolTipText(LOCTEXT("LandscapeInputUpdateSelectedComponentsTooltip", "Updates the selected components. Only valid if export selected components is true."))
				.OnClicked_Lambda(OnButtonUpdateComponentSelection)
				.IsEnabled_Lambda(IsLandscapeExportEnabled)
			]
		];

	}
	
	// The following checkbox are only added when not in heightfield mode
	if (MainInput->LandscapeExportType != EHoudiniLandscapeExportType::Heightfield)
	{
		// Checkbox : Export materials
		{
			TSharedPtr< SCheckBox > CheckBoxExportMaterials;
			Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
			[
				SAssignNew(CheckBoxExportMaterials, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeMaterialsCheckbox", "Export Landscape Materials"))
					.ToolTipText(LOCTEXT("LandscapeMaterialsTooltip", "If enabled, the landscape materials will be exported with it."))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([MainInput]()
				{
					if (!IsValidWeakPointer(MainInput))
						return ECheckBoxState::Unchecked;

					return MainInput->bLandscapeExportMaterials ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
				{
					if (!IsValidWeakPointer(MainInput))
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniLandscapeInputChangeExportMaterials", "Houdini Input: Changing Landscape input export materials."),
						MainInput->GetOuter());

					for (auto CurrentInput : InInputs)
					{
						if (!IsValidWeakPointer(CurrentInput))
							continue;

						bool bNewState = (NewState == ECheckBoxState::Checked);
						if (bNewState == CurrentInput->bLandscapeExportMaterials)
							continue;

						CurrentInput->Modify();

						CurrentInput->bLandscapeExportMaterials = bNewState;
						CurrentInput->MarkChanged(true);
					}
				})
			];

			/*
			// Disable when exporting heightfields
			if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
				CheckBoxExportMaterials->SetEnabled(false);
			*/
		}

		// Checkbox : Export Tile UVs
		{
			TSharedPtr< SCheckBox > CheckBoxExportTileUVs;
			Landscape_VerticalBox->AddSlot().Padding(ItemPadding).AutoHeight()
			[
				SAssignNew(CheckBoxExportTileUVs, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeTileUVsCheckbox", "Export Landscape Tile UVs"))
					.ToolTipText(LOCTEXT("LandscapeTileUVsTooltip", "If enabled, UVs will be exported separately for each Landscape tile."))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([MainInput]()
				{
					if (!IsValidWeakPointer(MainInput))
						return ECheckBoxState::Unchecked;

					return MainInput->bLandscapeExportTileUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
				{
					if (!IsValidWeakPointer(MainInput))
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniLandscapeInputChangeExportTileUVs", "Houdini Input: Changing Landscape export tile UVs."),
						MainInput->GetOuter());

					for (auto CurrentInput : InInputs)
					{
						if (!IsValidWeakPointer(CurrentInput))
							continue;

						bool bNewState = (NewState == ECheckBoxState::Checked);
						if (bNewState == CurrentInput->bLandscapeExportTileUVs)
							continue;

						CurrentInput->Modify();

						CurrentInput->bLandscapeExportTileUVs = bNewState;
						CurrentInput->MarkChanged(true);
					}
				})
			];

			/*
			// Disable when exporting heightfields
			if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
				CheckBoxExportTileUVs->SetEnabled(false);
			*/
		}

		// Checkbox : Export normalized UVs
		{
		TSharedPtr< SCheckBox > CheckBoxExportNormalizedUVs;
		Landscape_VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportNormalizedUVs, SCheckBox)
				.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs"))
			.ToolTipText(LOCTEXT("LandscapeNormalizedUVsTooltip", "If enabled, landscape UVs will be exported in [0, 1]."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->bLandscapeExportNormalizedUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniLandscapeInputChangeExportNormalizedUVs", "Houdini Input: Changing Landscape export normalized UVs."),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				bool bNewState = (NewState == ECheckBoxState::Checked);
				if (bNewState == CurrentInput->bLandscapeExportNormalizedUVs)
					continue;

				CurrentInput->Modify();

				CurrentInput->bLandscapeExportNormalizedUVs = bNewState;
				CurrentInput->MarkChanged(true);
			}
		})
			];

		/*
		// Disable when exporting heightfields
		if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
			CheckBoxExportNormalizedUVs->SetEnabled(false);
		*/
		}

		// Checkbox : Export lighting
		{
			TSharedPtr< SCheckBox > CheckBoxExportLighting;
			Landscape_VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
				[
					SAssignNew(CheckBoxExportLighting, SCheckBox)
					.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeLightingCheckbox", "Export Landscape Lighting"))
				.ToolTipText(LOCTEXT("LandscapeLightingTooltip", "If enabled, lightmap information will be exported with the landscape."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->bLandscapeExportLighting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
				.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniLandscapeInputChangeExportLighting", "Houdini Input: Changing Landscape export lighting."),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->bLandscapeExportLighting)
						continue;

					CurrentInput->Modify();

					CurrentInput->bLandscapeExportLighting = bNewState;
					CurrentInput->MarkChanged(true);
				}
			})
				];

			/*
			// Disable when exporting heightfields
			if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
				CheckBoxExportLighting->SetEnabled(false);
				*/
		}

	}
	
}

/*
FMenuBuilder
FHoudiniInputDetails::Helper_CreateCustomActorPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const TAttribute<FText>& HeadingText, const bool& bShowCurrentSelectionSection)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;

	// Filters are only based on the MainInput
	auto OnShouldFilterLandscape = [](const AActor* const Actor, UHoudiniInput* InInput)
	{
		if (!IsValid(Actor))
			return false;

		if (!Actor->IsA<ALandscapeProxy>())
			return false;

		ALandscapeProxy* LandscapeProxy = const_cast<ALandscapeProxy *>(Cast<const ALandscapeProxy>(Actor));
		if (!LandscapeProxy)
			return false;

		// Get the landscape's actor
		AActor* OwnerActor = LandscapeProxy->GetOwner();
		
		// Get our Actor
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(InInput->GetOuter());
		AActor* MyOwner = MyHAC ? MyHAC->GetOwner() : nullptr;

		// TODO: FIX ME!
		// IF the landscape is owned by ourself, skip it!
		if (OwnerActor == MyOwner)
			return false;

		return true;
	};

	auto OnShouldFilterWorld = [](const AActor* const Actor, UHoudiniInput* InInput)
	{		
		if (!IsValid(Actor))
			return false;

		const TArray<UHoudiniInputObject*>* InputObjects = InInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
		if (!InputObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurInputObject : *InputObjects)
		{
			if (!IsValid(CurInputObject))
				continue;

			AActor* CurActor = Cast<AActor>(CurInputObject->GetObject());
			if (!IsValid(CurActor))
				continue;

			if (CurActor == Actor)
				return true;
		}

		return false;
	};

	auto OnShouldFilterHoudiniAsset = [](const AActor* const Actor, UHoudiniInput* InInput)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		// But not our own Asset Actor
		if (const USceneComponent* RootComp = Cast<const USceneComponent>(InInput->GetOuter()))
		{
			if (RootComp && Cast<AHoudiniAssetActor>(RootComp->GetOwner()) != Actor)
				return true;
		}

		return false;
	};

	auto OnShouldFilterActor = [MainInput, OnShouldFilterLandscape, OnShouldFilterWorld, OnShouldFilterHoudiniAsset](const AActor* const Actor)
	{
		if (!IsValid(MainInput))
			return true;

		switch (MainInput->GetInputType())
		{
		case EHoudiniInputType::Landscape:
			return OnShouldFilterLandscape(Actor, MainInput);
		case EHoudiniInputType::World:
			return OnShouldFilterWorld(Actor, MainInput);
		case EHoudiniInputType::Asset:
			return OnShouldFilterHoudiniAsset(Actor, MainInput);
		default:
			return true;
		}

		return false;
	};


	// Selection uses the input arrays
	auto OnLandscapeSelected = [](AActor* Actor, UHoudiniInput* Input)
	{
		if (!Actor || !Input)
			return;

		ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
		if (!LandscapeProxy)
			return;

		TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!LandscapeInputObjectsArray)
			return;

		LandscapeInputObjectsArray->Empty();

		FName LandscapeName = MakeUniqueObjectName(Input->GetOuter(), ALandscapeProxy::StaticClass(), TEXT("Landscape"));

		// Create a Houdini Input Object.
		UHoudiniInputObject* NewInputObject = UHoudiniInputLandscape::Create(
			LandscapeProxy, Input, LandscapeName.ToString());

		UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(NewInputObject);
		LandscapeInput->MarkChanged(true);

		LandscapeInputObjectsArray->Add(LandscapeInput);
		Input->MarkChanged(true);
	};

	auto OnHoudiniAssetActorSelected = [](AActor* Actor, UHoudiniInput* Input) 
	{
		if (!Actor || !Input)
			return;

		AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(Actor);
		if (!HoudiniAssetActor)
			return;

		TArray<UHoudiniInputObject*>* AssetInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!AssetInputObjectsArray)
			return;

		AssetInputObjectsArray->Empty();

		FName HoudiniAssetActorName = MakeUniqueObjectName(Input->GetOuter(), AHoudiniAssetActor::StaticClass(), TEXT("HoudiniAsset"));

		// Create a Houdini Asset Input Object
		UHoudiniInputObject* NewInputObject = UHoudiniInputHoudiniAsset::Create(HoudiniAssetActor->GetHoudiniAssetComponent(), Input, HoudiniAssetActorName.ToString());

		UHoudiniInputHoudiniAsset* AssetInput = Cast<UHoudiniInputHoudiniAsset>(NewInputObject);
		AssetInput->MarkChanged(true);

		AssetInputObjectsArray->Add(AssetInput);
		Input->MarkChanged(true);
	};

	auto OnWorldSelected = [](AActor* Actor, UHoudiniInput* Input)
	{
		// Do Nothing
	};

	auto OnActorSelected = [OnLandscapeSelected, OnWorldSelected, OnHoudiniAssetActorSelected](AActor* Actor, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
	{
		for (auto& CurInput : InInputs)
		{
			if (!IsValid(CurInput))
				return;

			switch (CurInput->GetInputType())
			{
			case EHoudiniInputType::Landscape:
				return OnLandscapeSelected(Actor, CurInput);
			case EHoudiniInputType::World:
				return OnWorldSelected(Actor, CurInput);
			case EHoudiniInputType::Asset:
				return OnHoudiniAssetActorSelected(Actor, CurInput);
			default:
				return;
			}
		}

		return;
	};
	
	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateLambda(OnShouldFilterActor);
	
	if (bShowCurrentSelectionSection) 
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationHeader", "Current Selection"));
		{
			MenuBuilder.AddMenuEntry(
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
				FSlateIcon(),
				FUIAction(),
				NAME_None,
				EUserInterfaceActionType::Button,
				NAME_None);
		}
		MenuBuilder.EndSection();
	}


	MenuBuilder.BeginSection(NAME_None, HeadingText);
	{
		FSceneOutlinerModule & SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));
		SceneOutliner::FInitializationOptions InitOptions;
		{
			InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
			InitOptions.Filters->AddFilterPredicate(ActorFilter);
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Gutter(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 0));
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Label(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 10));
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::ActorInfo(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 20));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnActorSelected, InInputs))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}
*/


FMenuBuilder
FHoudiniInputDetails::Helper_CreateHoudiniAssetPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	auto OnShouldFilterHoudiniAsset = [InInputs](const AActor* const Actor)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		// But not our selected Asset Actor
		for (auto & NextSelectedInput : InInputs) 
		{
			if (!IsValidWeakPointer(NextSelectedInput))
				continue;

			const USceneComponent* RootComp = Cast<const USceneComponent>(NextSelectedInput->GetOuter());
			if (RootComp && Cast<AHoudiniAssetActor>(RootComp->GetOwner()) == Actor)
				return false;

		}

		return true;
	};

	// Filters are only based on the MainInput
	auto OnShouldFilterActor = [MainInput, OnShouldFilterHoudiniAsset](const AActor* const Actor)
	{
		if (!IsValidWeakPointer(MainInput))
			return true;

		return OnShouldFilterHoudiniAsset(Actor);
	};

	auto OnHoudiniAssetActorSelected = [OnShouldFilterHoudiniAsset](AActor* Actor, const TWeakObjectPtr<UHoudiniInput>& Input)
	{
		if (!IsValid(Actor) || !Input.IsValid())
			return;
		
		AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(Actor);
		if (!HoudiniAssetActor)
			return;

		// Make sure that the actor is valid for this input
		if (!OnShouldFilterHoudiniAsset(Actor))
			return;

		TArray<UHoudiniInputObject*>* AssetInputObjectsArray = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Asset);
		if (!AssetInputObjectsArray)
			return;

		FName HoudiniAssetActorName = MakeUniqueObjectName(Input->GetOuter(), AHoudiniAssetActor::StaticClass(), TEXT("HoudiniAsset"));

		// Create a Houdini Asset Input Object
		UHoudiniInputObject* NewInputObject = UHoudiniInputHoudiniAsset::Create(HoudiniAssetActor->GetHoudiniAssetComponent(), Input.Get(), HoudiniAssetActorName.ToString());

		UHoudiniInputHoudiniAsset* AssetInput = Cast<UHoudiniInputHoudiniAsset>(NewInputObject);
		AssetInput->MarkChanged(true);

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniAssetInputChange", "Houdini Input: Selecting an asset input"),
			Input->GetOuter());

		Input->Modify();

		AssetInputObjectsArray->Empty();
		AssetInputObjectsArray->Add(AssetInput);
		Input->MarkChanged(true);
	};

	auto OnActorSelected = [OnHoudiniAssetActorSelected](AActor* Actor, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
	{
		for (auto& CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				return;

			OnHoudiniAssetActorSelected(Actor, CurInput);
		}
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FActorTreeItem::FFilterPredicate::CreateLambda(OnShouldFilterActor);

	// Show current selection
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationHeader", "Current Selection"));
	{
		MenuBuilder.AddMenuEntry(
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput.Get(), &UHoudiniInput::GetCurrentSelectionText)),
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput.Get(), &UHoudiniInput::GetCurrentSelectionText)),
			FSlateIcon(),
			FUIAction(),
			NAME_None,
			EUserInterfaceActionType::Button,
			NAME_None);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AssetInputSelectableActors", "Houdini Assets"));
	{
		FSceneOutlinerModule & SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));
		FSceneOutlinerInitializationOptions InitOptions;
		{
			InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(ActorFilter);
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateActorPicker(
						InitOptions,
						FOnActorPicked::CreateLambda(OnActorSelected, InInputs))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateLandscapePickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;	
	auto OnShouldFilterLandscape = [](const AActor* const Actor, const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValid(Actor))
			return false;

		if (!Actor->IsA<ALandscapeProxy>())
			return false;

		ALandscapeProxy* LandscapeProxy = const_cast<ALandscapeProxy *>(Cast<const ALandscapeProxy>(Actor));
		if (!LandscapeProxy)
			return false;

		// Get the landscape's parent actor
		// We need to get the AttachParent on the root componet, GteOwner will not return the parent actor!
		AActor* OwnerActor = nullptr;
		USceneComponent* RootComponent = LandscapeProxy->GetRootComponent();
		if (IsValid(RootComponent))
			OwnerActor = RootComponent->GetAttachParent() ? RootComponent->GetAttachParent()->GetOwner() : LandscapeProxy->GetOwner();

		// Get our Actor
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(InInput->GetOuter());
		AActor* MyOwner = MyHAC ? MyHAC->GetOwner() : nullptr;

		// IF the landscape is owned by ourself, skip it!
		if (OwnerActor && OwnerActor == MyOwner)
		{
			// ... buuuut we dont want to filter input landscapes that have the "Update Input Landscape Data" option enabled
			// (and are, therefore, outputs as well)
			for (int32 Idx = 0; Idx < MyHAC->GetNumInputs(); Idx++)
			{
				UHoudiniInput* CurrentInput = MyHAC->GetInputAt(Idx);
				if (!IsValid(CurrentInput))
					continue;

				if (CurrentInput->GetInputType() != EHoudiniInputType::Landscape)
					continue;

				if (!CurrentInput->GetUpdateInputLandscape())
					continue;

				// Don't filter our input landscapes
				ALandscapeProxy* UpdatedInputLandscape = Cast<ALandscapeProxy>(CurrentInput->GetInputObjectAt(0));
				if (LandscapeProxy == UpdatedInputLandscape)
					return true;
			}

			return false;
		}

		return true;
	};

	// Filters are only based on the MainInput
	auto OnShouldFilterActor = [MainInput, OnShouldFilterLandscape](const AActor* const Actor)
	{
		if (!IsValidWeakPointer(MainInput))
			return true;

		return OnShouldFilterLandscape(Actor, MainInput);
	};

	// Selection uses the input arrays
	auto OnLandscapeSelected = [OnShouldFilterLandscape](AActor* Actor, const TWeakObjectPtr<UHoudiniInput>& Input)
	{
		if (!Actor || !Input.IsValid())
			return;

		// Make sure that the actor is valid for this input
		if (!OnShouldFilterLandscape(Actor, Input))
			return;

		ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
		if (!LandscapeProxy)
			return;

		TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!LandscapeInputObjectsArray)
			return;

		LandscapeInputObjectsArray->Empty();

		FName LandscapeName = MakeUniqueObjectName(Input->GetOuter(), ALandscapeProxy::StaticClass(), TEXT("Landscape"));

		// Create a Houdini Input Object.
		UHoudiniInputObject* NewInputObject = UHoudiniInputLandscape::Create(
			LandscapeProxy, Input.Get(), LandscapeName.ToString());

		UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(NewInputObject);
		LandscapeInput->MarkChanged(true);

		LandscapeInputObjectsArray->Add(LandscapeInput);
		Input->MarkChanged(true);
	};
	
	auto OnActorSelected = [OnLandscapeSelected](AActor* Actor, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
	{
		if (InInputs.Num() <= 0)
			return;

		const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
		if (!IsValidWeakPointer(MainInput))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniLandscapeInputChangeSelections", "Houdini Input: Selecting input landscape."),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			CurInput->Modify();
			OnLandscapeSelected(Actor, CurInput);
		}
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationHeader", "Landscape Selection"));
	{
		//--------------------------
		// Landscape input selector
		//--------------------------
		// Populate the Landscape options from landscape actors in the world (subject to filtering).
		UWorld* LandscapeWorld = MainInput->GetWorld();
		TMap<FString, AActor*> LandscapeOptions;
		for (TActorIterator<ALandscapeProxy> It(LandscapeWorld); It; ++It)
		{ 
			ALandscapeProxy* Actor = *It;
			if (!OnShouldFilterActor(*It))
			{
				continue;
			}

			LandscapeOptions.Add(It->GetActorLabel(), Actor);
		}
		
		FString CurrentSelection;

		if (MainInput.IsValid())
		{
			CurrentSelection = MainInput->GetCurrentSelectionText().ToString();
		}

		TSharedRef< SLandscapeComboBox > LandscapeInputWidget =
			SNew(SLandscapeComboBox)
			.LandscapeOptions(LandscapeOptions)
			.CurrentSelection( CurrentSelection )
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnLandscapeSelectionChanged_Lambda([OnActorSelected, InInputs](const FString& Name, AActor* LandscapeProxy)
			{
				OnActorSelected(LandscapeProxy, InInputs);
			})
		;
		
		MenuBuilder.AddWidget(LandscapeInputWidget, FText::GetEmpty(), true);
		
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateWorldActorPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	auto OnShouldFilterWorld = [MainInput](const AActor* const Actor)
	{
		if (!IsValidWeakPointer(MainInput))
			return true;

		const TArray<UHoudiniInputObject*>* InputObjects = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
		if (!InputObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurInputObject : *InputObjects)
		{
			if (!IsValid(CurInputObject))
				continue;

			AActor* CurActor = Cast<AActor>(CurInputObject->GetObject());
			if (!IsValid(CurActor))
			{
				// See if the input object is a HAC, if it is, get its parent actor
				UHoudiniAssetComponent* CurHAC = Cast<UHoudiniAssetComponent>(CurInputObject->GetObject());
				if (IsValid(CurHAC))
					CurActor = CurHAC->GetOwner();
			}

			if (!IsValid(CurActor))
				continue;

			if (CurActor == Actor)
				return true;
		}

		return false;
	};

	auto OnWorldSelected = [](AActor* Actor)
	{
		// Do Nothing
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FActorTreeItem::FFilterPredicate::CreateLambda(OnShouldFilterWorld);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldInputSelectedActors", "Currently Selected Actors"));
	{
		FSceneOutlinerModule & SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));
		FSceneOutlinerInitializationOptions InitOptions;
		{
			InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(ActorFilter);
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateActorPicker(
 						InitOptions,
 						FOnActorPicked::CreateLambda(OnWorldSelected))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateBoundSelectorPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	auto OnShouldFilter = [MainInput](const AActor* const Actor)
	{
		if (!IsValid(Actor))
			return false;

		const TArray<AActor*>* BoundObjects = MainInput->GetBoundSelectorObjectArray();
		if (!BoundObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurActor : *BoundObjects)
		{
			if (!IsValid(CurActor))
				continue;

			if (CurActor == Actor)
				return true;
		}

		return false;
	};

	
	auto OnSelected = [](AActor* Actor)
	{
		// Do Nothing
	};
	
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldInputBoundSelectors", "Bound Selectors"));
	{
		FSceneOutlinerModule & SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));
		FSceneOutlinerInitializationOptions InitOptions;
		{
			InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(FActorTreeItem::FFilterPredicate::CreateLambda(OnShouldFilter));
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateActorPicker(
 						InitOptions,
 						FOnActorPicked::CreateLambda(OnSelected))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

void
FHoudiniInputDetails::AddWorldInputUI(
	IDetailCategoryBuilder& CategoryBuilder,
	TSharedRef<SVerticalBox> VerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const IDetailsView* DetailsView)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::World);
	
	// Get the details view name and locked status
	bool bDetailsLocked = false;
	FName DetailsPanelName = "LevelEditorSelectionDetails";
	if (DetailsView)
	{
		DetailsPanelName = DetailsView->GetIdentifier();
		if (DetailsView->IsLocked())
			bDetailsLocked = true;
	}

	// Check of we're in bound selector mode
	bool bIsBoundSelector = MainInput->IsWorldInputBoundSelector();

	// Button : Start Selection / Use current selection + refresh        
	{
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
		FPropertyEditorModule & PropertyModule =
			FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

		//auto ButtonLabel = LOCTEXT("WorldInputStartSelection", "Start Selection (Locks Details Panel)");
		//auto ButtonTooltip = LOCTEXT("WorldInputStartSelectionTooltip", "Locks the Details Panel, and allow you to select object in the world that you can commit to the input afterwards.");
		FText ButtonLabel;
		FText ButtonTooltip;
		if (!bIsBoundSelector)
		{
			// Button :  Start Selection / Use current selection
			if (bDetailsLocked)
			{
				ButtonLabel = LOCTEXT("WorldInputUseCurrentSelection", "Use Current Selection (Unlocks Details Panel)");
				ButtonTooltip = LOCTEXT("WorldInputUseCurrentSelectionTooltip", "Fill the asset's input with the current selection  and unlocks the Details Panel.");
			}
			else 
			{
				ButtonLabel = LOCTEXT("WorldInputStartSelection", "Start Selection (Locks Details Panel)");
				ButtonTooltip = LOCTEXT("WorldInputStartSelectionTooltip", "Locks the Details Panel, and allow you to select object in the world that you can commit to the input afterwards.");
			}
			/*
			FOnClicked OnSelectActors = FOnClicked::CreateStatic(
				&FHoudiniInputDetails::Helper_OnButtonClickSelectActors, InInputs, DetailsPanelName);
			*/
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(ButtonLabel)
					.ToolTipText(ButtonTooltip)
					//.OnClicked(OnSelectActors)
					.OnClicked_Lambda([InInputs, DetailsPanelName, &CategoryBuilder]()
					{
						return Helper_OnButtonClickSelectActors(CategoryBuilder, InInputs, DetailsPanelName);
					})
					
				]
			];
		}
		else
		{
			// Button :  Start Selection / Use current selection as Bound selector
			if (bDetailsLocked)
			{
				ButtonLabel = LOCTEXT("WorldInputUseSelectionAsBoundSelector", "Use Selection as Bound Selector (Unlocks Details Panel)");
				ButtonTooltip = LOCTEXT("WorldInputUseSelectionAsBoundSelectorTooltip", "Fill the asset's input with all the actors contains in the current's selection bound, and unlocks the Details Panel.");
			}
			else 
			{
				ButtonLabel = LOCTEXT("WorldInputStartBoundSelection", "Start Bound Selection (Locks Details Panel)");
				ButtonTooltip = LOCTEXT("WorldInputStartBoundSelectionTooltip", "Locks the Details Panel, and allow you to select object in the world that will be used as bounds.");
			}
			
			/*
			FOnClicked OnSelectBounds = FOnClicked::CreateStatic(
				&FHoudiniInputDetails::Helper_OnButtonClickUseSelectionAsBoundSelector, InInputs, DetailsPanelName);
			*/
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(ButtonLabel)
					.ToolTipText(ButtonTooltip)
					//.OnClicked(OnSelectBounds)
					.OnClicked_Lambda([InInputs, DetailsPanelName, &CategoryBuilder]()
					{
						return Helper_OnButtonClickUseSelectionAsBoundSelector(CategoryBuilder, InInputs, DetailsPanelName);
					})
				]
			];
		}
	}

	// Button : Select All + Clear Selection
	{
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FOnClicked OnSelectAll = FOnClicked::CreateLambda([InInputs, MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniWorldInputSelectedAll", "Houdini Input: Selecting all actor in the current world"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				// Get the parent component/actor/world of the current input
				USceneComponent* ParentComponent = Cast<USceneComponent>(CurrentInput->GetOuter());
				AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;
				UWorld* MyWorld = CurrentInput->GetWorld();

				TArray<AActor*> NewSelectedActors;
				for (TActorIterator<AActor> ActorItr(MyWorld); ActorItr; ++ActorItr)
				{
					AActor *CurrentActor = *ActorItr;
					if (!IsValid(CurrentActor))
						continue;

					// Ignore the SkySpheres?
					FString ClassName = CurrentActor->GetClass() ? CurrentActor->GetClass()->GetName() : FString();
					if (ClassName.Contains("BP_Sky_Sphere"))
						continue;

					// Don't allow selection of ourselves. Bad things happen if we do.
					if (ParentActor && (CurrentActor == ParentActor))
						continue;

					NewSelectedActors.Add(CurrentActor);
				}

				CurrentInput->Modify();

				bool bHasChanged = CurrentInput->UpdateWorldSelection(NewSelectedActors);
			}

			return FReply::Handled();
		});

		FOnClicked OnClearSelect = FOnClicked::CreateLambda([InInputs, MainInput, &CategoryBuilder]()
		{
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			const bool bMainInputBoundSelection = MainInput->IsWorldInputBoundSelector();

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniWorldInputClear", "Houdini Input: Clearing world input selection"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				// Do nothing if the current input has different selector settings from the main input
				if (CurrentInput->IsWorldInputBoundSelector() != bMainInputBoundSelection)
					continue;

				CurrentInput->Modify();

				if (CurrentInput->IsWorldInputBoundSelector())
				{
					CurrentInput->SetBoundSelectorObjectsNumber(0);
					if (CategoryBuilder.IsParentLayoutValid())
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();
				}
				else
				{
					TArray<AActor*> EmptySelection;
					bool bHasChanged = CurrentInput->UpdateWorldSelection(EmptySelection);
				}
			}

			return FReply::Handled();
		});

		FText ClearSelectionLabel;
		FText ClearSelectionTooltip;
		if (bIsBoundSelector)
		{
			ClearSelectionLabel = LOCTEXT("ClearBoundSelection", "Clear Bound Selection");
			ClearSelectionTooltip = LOCTEXT("ClearBoundSelectionTooltip", "Clear the input's current bound selection.");
		}
		else
		{
			ClearSelectionLabel = LOCTEXT("ClearSelection", "Clear Selection");
			ClearSelectionTooltip = LOCTEXT("ClearSelectionTooltip", "Clear the input's current selection.");
		}

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				// Button :  SelectAll
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("WorldInputSelectAll", "Select All"))
				.ToolTipText(LOCTEXT("WorldInputSelectAll", "Fill the asset's input with all actors."))
				.OnClicked(OnSelectAll)
				.IsEnabled(!bIsBoundSelector)
			]
			+ SHorizontalBox::Slot()
			[
				// Button :  Clear Selection
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(ClearSelectionLabel)
				.ToolTipText(ClearSelectionTooltip)
				.OnClicked(OnClearSelect)
			]
		];

		// Do not enable select all/clear select when selection has been started and details are locked
		HorizontalBox->SetEnabled(!bDetailsLocked);
	}

	// Checkbox: Bound Selector
	{
		// Lambda returning a CheckState from the input's current bound selector state
		auto IsCheckedBoundSelector = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
		{
			if (!IsValidWeakPointer(InInput))
				return ECheckBoxState::Unchecked;

			return InInput->IsWorldInputBoundSelector() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		// Lambda for changing bound selector state
		auto CheckStateChangedIsBoundSelector = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniWorldInputChangeBoungSelector", "Houdini Input: Changing world input to bound selector"),
				MainInput->GetOuter());

			bool bNewState = (NewState == ECheckBoxState::Checked);
			for (auto CurInput : InInputsToUpdate)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->IsWorldInputBoundSelector() == bNewState)
					continue;

				CurInput->Modify();

				CurInput->SetWorldInputBoundSelector(bNewState);
			}

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		};

		// Checkbox : Is Bound Selector
		TSharedPtr< SCheckBox > CheckBoxBoundSelector;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(CheckBoxBoundSelector, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoundSelector", "Bound Selector"))
				.ToolTipText(LOCTEXT("BoundSelectorTip", "When enabled, this world input works as a bound selector, sending all the objects contained in the bound selector bounding boxes."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]		
			.IsChecked_Lambda([IsCheckedBoundSelector, MainInput]()
			{
				return IsCheckedBoundSelector(MainInput);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedIsBoundSelector, InInputs](ECheckBoxState NewState)
			{
				return CheckStateChangedIsBoundSelector(InInputs, NewState);
			})
		];
	}

	// Checkbox: Bound Selector Auto update
	{
		// Lambda returning a CheckState from the input's current auto update state
		auto IsCheckedAutoUpdate = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
		{
			if (!IsValidWeakPointer(InInput))
				return ECheckBoxState::Unchecked;

			return InInput->GetWorldInputBoundSelectorAutoUpdates() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		// Lambda for changing the auto update state
		auto CheckStateChangedBoundAutoUpdates = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniWorldInputChangeAutoUpdate", "Houdini Input: Changing bound selector auto-update state."),
				MainInput->GetOuter());

			bool bNewState = (NewState == ECheckBoxState::Checked);
			for (auto CurInput : InInputsToUpdate)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->GetWorldInputBoundSelectorAutoUpdates() == bNewState)
					continue;

				CurInput->Modify();

				CurInput->SetWorldInputBoundSelectorAutoUpdates(bNewState);
				CurInput->MarkChanged(true);
			}
		};

		// Checkbox : Is Bound Selector
		TSharedPtr< SCheckBox > CheckBoxBoundAutoUpdate;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(CheckBoxBoundAutoUpdate, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoundAutoUpdate", "Update bound selection automatically"))
				.ToolTipText(LOCTEXT("BoundAutoUpdateTip", "If enabled and if this world input is set as a bound selector, the objects selected by the bounds will update automatically."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([IsCheckedAutoUpdate, MainInput]()
			{
				return IsCheckedAutoUpdate(MainInput);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedBoundAutoUpdates, InInputs](ECheckBoxState NewState)
			{
				return CheckStateChangedBoundAutoUpdates(InInputs, NewState);
			})
		];

		CheckBoxBoundAutoUpdate->SetEnabled(MainInput->IsWorldInputBoundSelector());
	}

	// ActorPicker : Bound Selector
	if(bIsBoundSelector)
	{
		FMenuBuilder MenuBuilder = Helper_CreateBoundSelectorPickerWidget(InInputs);
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];
	}

	// ActorPicker : World Outliner
	{
		FMenuBuilder MenuBuilder = Helper_CreateWorldActorPickerWidget(InInputs);
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];
	}

	{
		// Spline Resolution
		TSharedPtr<SNumericEntryBox<float>> NumericEntryBox;
		int32 Idx = 0;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SplineRes", "Unreal Spline Resolution"))
				.ToolTipText(LOCTEXT("SplineResTooltip", "Resolution used when marshalling the Unreal Splines to HoudiniEngine.\n(step in cm between control points)\nSet this to 0 to only export the control points."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.MinValue(-1.0f)
				.MaxValue(1000.0f)
				.MinSliderValue(0.0f)
				.MaxSliderValue(1000.0f)
				.Value(MainInput->GetUnrealSplineResolution())
				.OnValueChanged_Lambda([MainInput, InInputs](float Val) 
				{
					if (!IsValidWeakPointer(MainInput))
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniWorldInputChangeSplineResolution", "Houdini Input: Changing world input spline resolution"),
						MainInput->GetOuter());

					for (auto CurrentInput : InInputs)
					{
						if (!IsValidWeakPointer(CurrentInput))
							continue;

						if (CurrentInput->GetUnrealSplineResolution() == Val)
							continue;

						CurrentInput->Modify();

						CurrentInput->SetUnrealSplineResolution(Val);
						CurrentInput->MarkChanged(true);
					}					
				})
				/*
				.Value(TAttribute< TOptional< float > >::Create(TAttribute< TOptional< float > >::FGetter::CreateUObject(
					&InParam, &UHoudiniAssetInput::GetSplineResolutionValue)))
				.OnValueChanged(SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
					&InParam, &UHoudiniAssetInput::SetSplineResolutionValue))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(
					&InParam, &UHoudiniAssetInput::IsSplineResolutionEnabled)))
				*/
				.SliderExponent(1.0f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("SplineResToDefault", "Reset to default value."))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(EVisibility::Visible)
				// TODO: FINISH ME!
				//.OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnResetSplineResolutionClicked))
				.OnClicked_Lambda([MainInput, InInputs]()
				{
					if (!IsValidWeakPointer(MainInput))
						return FReply::Handled();

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniWorldInputRevertSplineResolution", "Houdini Input: Reverting world input spline resolution to default"),
						MainInput->GetOuter());

					const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
					float DefaultSplineResolution = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->MarshallingSplineResolution : 50.0f;

					for (auto CurrentInput : InInputs)
					{
						if (!IsValidWeakPointer(CurrentInput))
							continue;

						if (CurrentInput->GetUnrealSplineResolution() == DefaultSplineResolution)
							continue;

						CurrentInput->Modify();

						CurrentInput->SetUnrealSplineResolution(DefaultSplineResolution);
						CurrentInput->MarkChanged(true);
					}

					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
	}
}

void
FHoudiniInputDetails::AddSkeletalInputUI(
	TSharedRef<SVerticalBox> VerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, 
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool )
{
}

void FHoudiniInputDetails::AddGeometryCollectionInputUI(IDetailCategoryBuilder& CategoryBuilder, TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::GeometryCollection);

	// Lambda for changing ExportColliders state
	auto SetGeometryCollectionInputObjectsCount = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const int32& NewInputCount)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changing the number of Geometry Input Objects"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetNumberOfInputObjects(EHoudiniInputType::GeometryCollection) == NewInputCount)
				continue;

			CurInput->Modify();

			CurInput->SetInputObjectsNumber(EHoudiniInputType::GeometryCollection, NewInputCount);
			CurInput->MarkChanged(true);

			// 
			if (GEditor)
				GEditor->RedrawAllViewports();

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	InVerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("NumArrayItemsFmt", "{0} elements"), FText::AsNumber(NumInputObjects)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([SetGeometryCollectionInputObjectsCount, InInputs, NumInputObjects]()
			{
				return SetGeometryCollectionInputObjectsCount(InInputs, NumInputObjects + 1);
			}),
			LOCTEXT("AddInput", "Adds a Geometry Input"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([SetGeometryCollectionInputObjectsCount, InInputs]()
			{
				return SetGeometryCollectionInputObjectsCount(InInputs, 0);
			}),
			LOCTEXT("EmptyInputs", "Removes All Inputs"), true)
		]
	];

	for (int32 GeometryObjectIdx = 0; GeometryObjectIdx < NumInputObjects; GeometryObjectIdx++)
	{
		//UObject* InputObject = InParam.GetInputObject(Idx);
		Helper_CreateGeometryCollectionWidget(CategoryBuilder, InInputs, GeometryObjectIdx, AssetThumbnailPool, InVerticalBox);
	}
	
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickSelectActors(IDetailCategoryBuilder& CategoryBuilder, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName)
{
	return Helper_OnButtonClickSelectActors(CategoryBuilder, InInputs, DetailsPanelName, false);
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickUseSelectionAsBoundSelector(IDetailCategoryBuilder& CategoryBuilder, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName)
{
	return Helper_OnButtonClickSelectActors(CategoryBuilder, InInputs, DetailsPanelName, true);
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickSelectActors(IDetailCategoryBuilder& CategoryBuilder, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName, const bool& bUseWorldInAsWorldSelector)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	if (!IsValidWeakPointer(MainInput))
		return FReply::Handled();

	// There's no undo operation for button.
	FPropertyEditorModule & PropertyModule =
		FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

	// Locate the details panel.
	TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);
	if (!DetailsView.IsValid())
		return FReply::Handled();

	class SLocalDetailsView : public SDetailsViewBase
	{
		public:
			void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
			void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
	};
	auto * LocalDetailsView = static_cast<SLocalDetailsView *>(DetailsView.Get());

	if (!DetailsView->IsLocked())
	{
		//
		// START SELECTION
		//  Locks the details view and select our currently selected actors
		//
		LocalDetailsView->LockDetailsView();
		check(DetailsView->IsLocked());

		// Force refresh of details view.
		TArray<UObject*> InputOuters;
		for (auto CurIn : InInputs)
			InputOuters.Add(CurIn->GetOuter());
		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		//ReselectSelectedActors();

		if (bUseWorldInAsWorldSelector)
		{
			// Bound Selection
			// Select back the previously chosen bound selectors
			GEditor->SelectNone(false, true);
			int32 NumBoundSelectors = MainInput->GetNumberOfBoundSelectorObjects();
			for (int32 Idx = 0; Idx < NumBoundSelectors; Idx++)
			{
				AActor* Actor = MainInput->GetBoundSelectorObjectAt(Idx);
				if (!IsValid(Actor))
					continue;

				GEditor->SelectActor(Actor, true, true);
			}
		}
		else
		{
			// Regular selection
			// Select the already chosen input Actors from the World Outliner.
			GEditor->SelectNone(false, true);
			int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::World);
			for (int32 Idx = 0; Idx < NumInputObjects; Idx++)
			{
				UHoudiniInputObject* CurInputObject = MainInput->GetHoudiniInputObjectAt(Idx);
				if (!CurInputObject)
					continue;

				AActor* Actor = nullptr;
				UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(CurInputObject);
				if (IsValid(InputActor))
				{
					// Get the input actor
					Actor = InputActor->GetActor();
				}
				else
				{
					// See if the input object is a HAC
					UHoudiniInputHoudiniAsset* InputHAC = Cast<UHoudiniInputHoudiniAsset>(CurInputObject);
					if (IsValid(InputHAC))
					{
						Actor = InputHAC->GetHoudiniAssetComponent() ? InputHAC->GetHoudiniAssetComponent()->GetOwner() : nullptr;
					}
				}

				if (!IsValid(Actor))
					continue;

				GEditor->SelectActor(Actor, true, true);
			}
		}

		return FReply::Handled();
	}
	else
	{
		//
		// UPDATE SELECTION
		//  Unlocks the input's selection and select the HDA back.
		//

		if (!GEditor || !GEditor->GetSelectedObjects())
			return FReply::Handled();

		USelection * SelectedActors = GEditor->GetSelectedActors();
		if (!SelectedActors)
			return FReply::Handled();

		// Create a transaction
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniWorldInputSelectionChanged", "Changing Houdini world outliner input objects"),
			MainInput->GetOuter());


		TArray<UObject*> AllActors;
		for (auto CurrentInput : InInputs)
		{
			CurrentInput->Modify();
			// Get our parent component/actor
			USceneComponent* ParentComponent = Cast<USceneComponent>(CurrentInput->GetOuter());
			AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;
			AllActors.Add(ParentActor);

			bool bHasChanged = true;
			if (bUseWorldInAsWorldSelector)
			{
				//
				// Update bound selectors
				
				// Clean up the selected actors
				TArray<AActor*> ValidBoundSelectedActors;
				for (FSelectionIterator It(*SelectedActors); It; ++It)
				{
					AActor* CurrentBoundActor = Cast<AActor>(*It);
					if (!CurrentBoundActor)
						continue;

					// Don't allow selection of ourselves. Bad things happen if we do.
					if (ParentActor && (CurrentBoundActor == ParentActor))
						continue;

					ValidBoundSelectedActors.Add(CurrentBoundActor);
				}

				// See if the bound selector have changed
				int32 PreviousBoundSelectorCount = CurrentInput->GetNumberOfBoundSelectorObjects();
				if (PreviousBoundSelectorCount == ValidBoundSelectedActors.Num())
				{
					// Same number of BoundSelectors, see if they have changed
					bHasChanged = false;
					for (int32 BoundIdx = 0; BoundIdx < PreviousBoundSelectorCount; BoundIdx++)
					{
						AActor* PreviousBound = CurrentInput->GetBoundSelectorObjectAt(BoundIdx);
						if (!PreviousBound)
							continue;

						if (!ValidBoundSelectedActors.Contains(PreviousBound))
						{
							bHasChanged = true;
							break;
						}
					}
				}

				if (bHasChanged)
				{
					// Only update the bound selector objects on the input if they have changed
					CurrentInput->SetBoundSelectorObjectsNumber(ValidBoundSelectedActors.Num());
					int32 InputObjectIdx = 0;
					for (auto CurActor : ValidBoundSelectedActors)
					{
						CurrentInput->SetBoundSelectorObjectAt(InputObjectIdx++, CurActor);
					}

					// Update the current selection from the BoundSelectors
					CurrentInput->UpdateWorldSelectionFromBoundSelectors();
				}
			}
			else
			{
				//
				// Update our selection directly with the currently selected actors
				//

				TArray<AActor*> ValidSelectedActors;
				for (FSelectionIterator It(*SelectedActors); It; ++It)
				{
					AActor* CurrentActor = Cast<AActor>(*It);
					if (!CurrentActor)
						continue;

					// Don't allow selection of ourselves. Bad things happen if we do.
					if (ParentActor && (CurrentActor == ParentActor))
						continue;

					ValidSelectedActors.Add(CurrentActor);
				}

				// Update the input objects from the valid selected actors array
				// Only new/remove input objects will be marked as changed
				bHasChanged = CurrentInput->UpdateWorldSelection(ValidSelectedActors);
			}

			// If we didnt change the selection, cancel the transaction
			if (!bHasChanged)
				Transaction.Cancel();
		}

		// We can now unlock the details view...
		LocalDetailsView->UnlockDetailsView();
		check(!DetailsView->IsLocked());

		// .. reset the selected actors, force refresh and override the lock.
		DetailsView->SetObjects(AllActors, true, true);

		// We now need to reselect all our Asset Actors.
		// If we don't do this, our Asset parameters will stop refreshing and the user will be very confused.
		// It is also resetting the state of the selection before the input actor selection process was started.
		GEditor->SelectNone(false, true);
		for (auto CurrentActor : AllActors)
		{
			AActor* ParentActor = Cast<AActor>(CurrentActor);
			if (!ParentActor)
				continue;

			GEditor->SelectActor(ParentActor, true, true);
		}

		// Update the input details layout.
		// if (CategoryBuilder.IsParentLayoutValid())
		//   CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	}

	return FReply::Handled();
}


bool
FHoudiniInputDetails::Helper_CancelWorldSelection(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName)
{
	if (InInputs.Num() <= 0)
		return false;

	// Get the property module to access the details view
	FPropertyEditorModule & PropertyModule =
		FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

	// Locate the details panel.
	TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);
	if (!DetailsView.IsValid())
		return false;

	if (!DetailsView->IsLocked())
		return false;

	class SLocalDetailsView : public SDetailsViewBase
	{
	public:
		void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
		void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
	};
	auto * LocalDetailsView = static_cast<SLocalDetailsView *>(DetailsView.Get());

	// Get all our parent components / actors
	TArray<UObject*> AllComponents;
	TArray<UObject*> AllActors;
	for (auto CurrentInput : InInputs)
	{
		// Get our parent component/actor
		USceneComponent* ParentComponent = Cast<USceneComponent>(CurrentInput->GetOuter());
		if (!ParentComponent)
			continue;

		AllComponents.Add(ParentComponent);

		AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;
		if (!ParentActor)
			continue;

		AllActors.Add(ParentActor);
	}

	// Unlock the detail view and re-select our parent actors
	{
		LocalDetailsView->UnlockDetailsView();
		check(!DetailsView->IsLocked());

		// Reset selected actor to itself, force refresh and override the lock.
		DetailsView->SetObjects(AllActors, true, true);
	}

	// Reselect the Asset Actor. If we don't do this, our Asset parameters will stop
	// refreshing and the user will be very confused. It is also resetting the state
	// of the selection before the input actor selection process was started.
	GEditor->SelectNone(false, true);
	for (auto ParentActorObj : AllActors)
	{
		AActor* ParentActor = Cast<AActor>(ParentActorObj);
		if (!ParentActor)
			continue;

		GEditor->SelectActor(ParentActor, true, true);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE