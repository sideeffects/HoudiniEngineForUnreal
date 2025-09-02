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

#include "HoudiniAssetActor.h"
#include "HoudiniAssetBlueprintComponent.h"
#include "HoudiniCookable.h"
#include "HoudiniInput.h"
#include "HoudiniInputWidgets.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInputTranslator.h"
#include "HoudiniPackageParams.h"
#if defined(HOUDINI_USE_PCG)
	#include "HoudiniPCGCookable.h"
#endif
#include "HoudiniSplineComponentVisualizer.h"
#include "UnrealObjectInputRuntimeUtils.h"

#include "ActorTreeItem.h"
#include "AssetRegistry/AssetData.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/SlateDelegates.h"
#include "IDetailCustomization.h"
#include "Modules/ModuleManager.h"
#include "SAssetDropTarget.h"
#include "SceneOutlinerModule.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWidget.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionObject.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#endif


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

// Customized SVerticalBox used by for creating the spline point selection
// and control UI. When a spline is changed, or the selected points for a spline
// are changed, then this refreshes the UI so that the transform controls for
// the points are updated accordingly.
class SCurveListenerVerticalBox : public SVerticalBox
{
public:
	TSharedPtr<FHoudiniSplineComponentVisualizer> HoudiniSplineComponentVisualizer;
	IDetailCategoryBuilder* CategoryBuilder;
	mutable UHoudiniSplineComponent* PrevSplineComponent;
	mutable FTransform PrevSplineTransform;
	mutable TArray<int32> PrevEditedControlPointsIndexes;

public:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		if (!HoudiniSplineComponentVisualizer.IsValid())
			return LayerId;

		bool bRefreshUI = false;

		if (HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent() != PrevSplineComponent)
		{
			bRefreshUI = true;
			PrevSplineComponent = HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent();
			if (PrevSplineComponent)
			{
				PrevEditedControlPointsIndexes = PrevSplineComponent->EditedControlPointsIndexes;
				PrevSplineTransform = PrevSplineComponent->GetComponentTransform();
			}
		}
		else if (PrevSplineComponent)
		{
			if (PrevSplineComponent->HasChanged())
			{
				bRefreshUI = true;
			}
			else if (PrevSplineTransform.GetLocation() != PrevSplineComponent->GetComponentTransform().GetLocation() ||
				 PrevSplineTransform.GetRotation() != PrevSplineComponent->GetComponentTransform().GetRotation())
			{
				bRefreshUI = true;
				PrevSplineTransform = PrevSplineComponent->GetComponentTransform();
			}
			else {
				TArray<int32>& CurEditedControlPointIndexes = PrevSplineComponent->EditedControlPointsIndexes;

				if (CurEditedControlPointIndexes.Num() == PrevEditedControlPointsIndexes.Num())
				{
					for (int32 Idx = 0; Idx < CurEditedControlPointIndexes.Num(); ++Idx)
					{
						if (CurEditedControlPointIndexes[Idx] != PrevEditedControlPointsIndexes[Idx]) {
							bRefreshUI = true;
							break;
						}
					}
				}
				else
					bRefreshUI = true;

				if (bRefreshUI)
					PrevEditedControlPointsIndexes = CurEditedControlPointIndexes;
			}
		}

		if (bRefreshUI)
		{
			if (GEditor)
				GEditor->RedrawAllViewports();

			if (CategoryBuilder->IsParentLayoutValid())
				CategoryBuilder->GetParentLayout().ForceRefreshDetails();
		}

		return LayerId;
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
	FString InputRowString = MainInput->GetInputLabel() + " " + MainInput->GetInputName();
	Row = InputRow == nullptr ? &(HouInputCategory.AddCustomRow(FText::FromString(InputRowString))) : InputRow;
	if (!Row)
		return;

	// Create the standard input name widget if this is not a operator path parameter.
	// Operator path parameter's name widget is handled by HoudiniParameterDetails.
	if (!InputRow)
		CreateNameWidget(MainInput, *Row, true, InInputs.Num());

	// Create a vertical Box for storing the UI
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	// ComboBox :  Input Type
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	TSharedPtr<IDetailsView> DetailsViewShared = HouInputCategory.GetParentLayout().GetDetailsViewSharedPtr();
	const IDetailsView* DetailsView = DetailsViewShared.Get();
#else
	const IDetailsView* DetailsView = HouInputCategory.GetParentLayout().GetDetailsView();
#endif

	AddInputTypeComboBox(HouInputCategory, VerticalBox, InInputs, DetailsView);

	switch (MainInput->GetInputType())
	{
		case EHoudiniInputType::Geometry:
		{
			AddGeometryInputUI(HouInputCategory, VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::Curve:
		{
			AddCurveInputUI(HouInputCategory, VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::World:
		{
			AddWorldInputUI(HouInputCategory, VerticalBox, InInputs, DetailsView);
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

	FString InputLabelStr = InInput->GetInputLabel();
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
			.Font(_GetEditorStyle().GetFontStyle(!InInput->HasChanged() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")));
	}
}

FText
FHoudiniInputDetails::GetInputTooltip(const TWeakObjectPtr<UHoudiniInput>& InParam)
{
	// TODO: Add input tooltip
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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Input Type"),
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
#if defined(HOUDINI_USE_PCG)
	else if (UHoudiniPCGCookable* HPCGC = MainInput->GetTypedOuter<UHoudiniPCGCookable>())
	{
		SupportedChoices = FHoudiniEngineEditor::Get().GetPCGInputTypeChoiceLabels();
	}
#endif
	else if (UHoudiniCookable* Cookable = MainInput->GetTypedOuter<UHoudiniCookable>())
	{
		if(!Cookable->AssetEditorId.IsNone())
			SupportedChoices = FHoudiniEngineEditor::Get().GetAssetEditorInputTypeChoiceLabels();
		else
			SupportedChoices = FHoudiniEngineEditor::Get().GetInputTypeChoiceLabels();
	}
	else
	{
		SupportedChoices = FHoudiniEngineEditor::Get().GetInputTypeChoiceLabels();
	}

	TSharedPtr<FString> InitiallySelectedType = (*SupportedChoices)[0];
	for (int Idx = 0; Idx < SupportedChoices->Num(); ++Idx)
	{
		EHoudiniInputType CurType = UHoudiniInput::StringToInputType(*(*SupportedChoices)[Idx]);
		if (CurType == MainInput->GetInputType())
		{
			InitiallySelectedType = (*SupportedChoices)[Idx];
			break;
		}
	}

	// ComboBox :  Input Type
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ComboBoxInputType;
	TSharedPtr<SImage> RebuildImage;
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1.0f)
		//.FillWidth(0.75f)
		//.AutoWidth()
		[
			SAssignNew(ComboBoxInputType, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(SupportedChoices)
			.InitiallySelectedItem(InitiallySelectedType)
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
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
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.MaxWidth(16.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RecookInput", "Recook this input only."))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InInputs]()
			{
				for (auto CurInput : InInputs)
				{
					if (!IsValidWeakPointer(CurInput))
						continue;

					// Force the input to reupload its data
					CurInput->MarkChanged(true);
					//CurInput->MarkDataUploadNeeded(true);
					CurInput->MarkAllInputObjectsChanged(true);
				}

				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine._RebuildAll"))
			]
		]
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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Keep World Transform"),
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
	TSharedPtr<SCheckBox> CheckBoxTranformType;
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SAssignNew(CheckBoxTranformType, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("KeepWorldTransformCheckbox", "Keep World Transform"))
			.ToolTipText(LOCTEXT("KeepWorldTransformCheckboxTip", "Set this Input's object_merge Transform Type to INTO_THIS_OBJECT. If unchecked, it will be set to NONE."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Pack before merge"),
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

	TSharedPtr<SCheckBox> CheckBoxPackBeforeMerge;
	VerticalBox->AddSlot()
	.Padding( 2, 2, 5, 2 )
	.AutoHeight()
	[
		SAssignNew(CheckBoxPackBeforeMerge, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PackBeforeMergeCheckbox", "Pack Geometry Before Merging"))
			.ToolTipText(LOCTEXT( "PackBeforeMergeCheckboxTip", "Pack each separate piece of geometry before merging them into the input."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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
FHoudiniInputDetails::AddExportCheckboxes(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current ExportMainGeoemtry state
	auto IsCheckedExportMainGeo = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetExportMainGeometry() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

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

	auto IsCheckedMergeSplineMeshComponents = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->IsMergeSplineMeshComponentsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto IsCheckedPreferNanite = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetPreferNaniteFallbackMesh() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto IsCheckedExportMaterialParameters = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetExportMaterialParameters() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing ExportMainGeometry state
	auto CheckStateChangedExportMainGeo = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetExportMainGeometry() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Main Geometry"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetExportMainGeometry() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetExportMainGeometry(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export LODs"),
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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Sockets"),
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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Colliders"),
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

	auto CheckStateChangedMergeSplineMeshComponents = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->IsMergeSplineMeshComponentsEnabled() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Merge Spline Mesh Components"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsMergeSplineMeshComponentsEnabled() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetMergeSplineMeshComponents(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	auto CheckStateChangedPreferNanite = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetPreferNaniteFallbackMesh() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Use Nanite Fallback Preference"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetPreferNaniteFallbackMesh() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetPreferNaniteFallbackMesh(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda for changing ExportMaterialParameters state
	auto CheckStateChangedExportMaterialParameters = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetExportMaterialParameters() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Material Parameters"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetExportMaterialParameters() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetExportMaterialParameters(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};



	TSharedPtr<SCheckBox> CheckBoxExportMainGeo;
	VerticalBox->AddSlot()
	.Padding( 2, 2, 5, 2 )
	.AutoHeight()
	[
		SAssignNew(CheckBoxExportMainGeo, SCheckBox )
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "ExportMainGeo", "Export Main Geometry" ) )
			.ToolTipText( LOCTEXT( "ExportMainGeoCheckboxTip", "If enabled, the Main Geometry will be sent to Houdini. Turn this off if you only want to send Collider data, or LOD data to Houdini." ) )
			.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedExportMainGeo(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedExportMainGeo(InInputs, NewState);
		})
	];

	TSharedPtr<SCheckBox> CheckBoxExportLODs;
	VerticalBox->AddSlot()
	.Padding( 2, 2, 5, 2 )
	.AutoHeight()
	[
		SAssignNew(CheckBoxExportLODs, SCheckBox )
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "ExportAllLOD", "Export LODs" ) )
			.ToolTipText( LOCTEXT( "ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini." ) )
			.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedExportLODs(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedExportLODs(InInputs, NewState);
		})
	];

	TSharedPtr<SCheckBox> CheckBoxExportSockets;
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SAssignNew( CheckBoxExportSockets, SCheckBox )
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "ExportSockets", "Export Sockets" ) )
			.ToolTipText( LOCTEXT( "ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini." ) )
			.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedExportSockets(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedExportSockets(InInputs, NewState);
		})
	];


	TSharedPtr<SCheckBox> CheckBoxExportColliders;
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SAssignNew( CheckBoxExportColliders, SCheckBox )
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "ExportColliders", "Export Colliders" ) )
			.ToolTipText( LOCTEXT( "ExportCollidersTip", "If enabled, collision geometry for this static mesh will be sent to Houdini." ) )
			.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedExportColliders(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedExportColliders(InInputs, NewState);
		})
	];


	TSharedPtr<SCheckBox> CheckBoxExportMaterialParameters;
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
			SAssignNew(CheckBoxExportMaterialParameters, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExportMaterialParameters", "Export Material Parameters"))
				.ToolTipText(LOCTEXT("ExportMaterialParametersTip", "If enabled, materials parameters on the input objects will be sent to Houdini as primitive attributes."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportMaterialParameters(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportMaterialParameters(InInputs, NewState);
			})
		]
	];


	TSharedPtr<SCheckBox> CheckBoxPreferNaniteFallback;
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
			SAssignNew(CheckBoxPreferNaniteFallback, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PreferNaniteFallbackMesh", "Prefer Nanite Fallback Mesh"))
				.ToolTipText(LOCTEXT("PreferNaniteFallbackMeshTip", "If enabled, when a Nanite asset is used as input, Houdini will use the fallback mesh if available."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedPreferNanite(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedPreferNanite(InInputs, NewState);
			})
		]
	];


	TSharedPtr<SCheckBox> CheckBoxMergeSplineMeshComponents;
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
			SAssignNew(CheckBoxMergeSplineMeshComponents, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MergeSplineMeshComponents", "Merge Spline Mesh Components"))
				.ToolTipText(LOCTEXT("MergeSplineMeshComponentsTip", "If enabled, spline mesh components from world input Actors are merged into a single static mesh per actor."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.Visibility(EVisibility::Visible)
			.IsChecked_Lambda([=]()
			{
				return IsCheckedMergeSplineMeshComponents(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedMergeSplineMeshComponents(InInputs, NewState);
			})
		]
	];
}

void
FHoudiniInputDetails::AddLevelInstanceExportCheckboxes(TSharedRef< SVerticalBox > VerticalBox, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current bExportLevelInstanceContent state
	auto IsExportLevelInstanceContentEnabled = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->IsExportLevelInstanceContentEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing bIsExportLevelInstanceContent state
	auto CheckStateChangedExportLevelInstanceContent = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		const bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->IsExportLevelInstanceContentEnabled() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Level Instance Content"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsExportLevelInstanceContentEnabled() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetExportLevelInstanceContent(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};


	TSharedPtr<SCheckBox> CheckBoxExportLevelInstanceContent;
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
			SAssignNew(CheckBoxExportLevelInstanceContent, SCheckBox)
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportLevelInstanceContent", "Export Level Instance Content" ) )
				.ToolTipText( LOCTEXT( "ExportLevelInstanceContentCheckboxTip", "If enabled, level instance content will be sent to Houdini." ) )
				.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.IsChecked_Lambda([=]()
			{
				return IsExportLevelInstanceContentEnabled(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportLevelInstanceContent(InInputs, NewState);
			})
		]
	];
}

void
FHoudiniInputDetails::AddCurveRotScaleAttributesCheckBox(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput) || MainInput->GetInputType() != EHoudiniInputType::Curve)
		return;

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CurveAddRotScaleAttributes", "Add Rotation/Scale Attributes"))
			.ToolTipText(LOCTEXT("CurveAddRotScaleAttributesTip", "If enabled, rotation and scale attributes will be added per to the input curve for each control point."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([MainInput]()
		{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->IsAddRotAndScaleAttributesEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([MainInput, InInputs](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			const bool bNewState = (NewState == ECheckBoxState::Checked);

			if (MainInput->IsAddRotAndScaleAttributesEnabled() == bNewState)
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Add Rotation/Scale Attributes to Curve Checkbox"),
				MainInput->GetOuter());
			
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->IsAddRotAndScaleAttributesEnabled() == bNewState)
					continue;

				CurInput->Modify();
				CurInput->SetAddRotAndScaleAttributes(bNewState);
				CurInput->MarkChanged(true);
			}
		})
	];
}

void
FHoudiniInputDetails::AddCurveAutoUpdateCheckBox(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput) || MainInput->GetInputType() != EHoudiniInputType::Curve)
		return;

	auto IsAutoUpdate = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetCookOnCurveChange() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto CheckStateChangedAutoUpdate = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetCookOnCurveChange() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Curve Auto Update CheckBox"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetCookOnCurveChange() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetCookOnCurveChange(bNewState);
			CurInput->MarkChanged(true);
		}
	};

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CurveAutoUpdateCheckBox", "Auto Update"))
			.ToolTipText(LOCTEXT("CurveAutoUpdateCheckBoxTip", "When checked, cook is triggered automatically when the curve is modified."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([=]()
		{
			return IsAutoUpdate(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedAutoUpdate(InInputs, NewState);
		})
	];
}

void 
FHoudiniInputDetails::AddExportAsReferenceCheckBoxes(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	// NOTE: The "Export*" checkboxes are represented in
	// UHoudiniInput as "Import*". This function is used for the
	// New Geometry and New World input types. It differs from
	// AddImportAsReferenceCheckboxes in the appearance of the
	// checkboxes.

	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	// Lambda returning a CheckState from the input's current
	// ImportAsReference state
	auto IsCheckedImportAsReference = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetImportAsReference() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Add export input as references checkbox
	{
		auto CheckStateChangeImportAsReference = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			const bool bNewState = (NewState == ECheckBoxState::Checked);

			if (MainInput->GetImportAsReference() == bNewState)
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Input as References CheckBox"),
				MainInput->GetOuter());

			for (auto CurInput : InInputsToUpdate)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->GetImportAsReference() == bNewState)
					continue;

				TArray<TObjectPtr<UHoudiniInputObject>>* InputObjs = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
				
				if (InputObjs)
				{
					for (auto CurInputObj : *InputObjs)
					{
						if (!IsValid(CurInputObj))
							continue;

						if (CurInputObj->GetImportAsReference() != bNewState)
						{
							CurInputObj->MarkChanged(true);
						}
					}
				}

				CurInput->Modify();
				CurInput->SetImportAsReference(bNewState);
				CurInput->MarkChanged(true);
			}
		};

		TSharedPtr<SCheckBox> CheckBoxImportAsReference;
		InVerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(CheckBoxImportAsReference, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExportInputAsRefCheckbox", "Export Input as References"))
				.ToolTipText(LOCTEXT("ExportInputAsRefCheckboxTip", "Export input objects as references to Houdini."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedImportAsReference(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangeImportAsReference(InInputs, NewState);
			})
		];
	}

	// 1 - Add rot/scale checkbox
	{
		auto IsCheckedImportAsReferenceRotScale = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
		{
			if (!IsValidWeakPointer(InInput))
				return ECheckBoxState::Unchecked;
			
			return InInput->GetImportAsReferenceRotScaleEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto CheckStateChangedImportAsReferenceRotScale = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			bool bNewState = (NewState == ECheckBoxState::Checked);

			if (MainInput->GetImportAsReferenceRotScaleEnabled() == bNewState)
				return;

			FScopedTransaction Transcation(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniExportAsRefRotScale", "Houdini Input: Changed Add Rotation/Scale to References CheckBox"),
				MainInput->GetOuter());

			for (auto CurInput : InInputsToUpdate)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->GetImportAsReferenceRotScaleEnabled() == bNewState)
					continue;

				TArray<TObjectPtr<UHoudiniInputObject>>* InputObjs = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
				if (InputObjs)
				{
					for (auto CurInputObj : *InputObjs)
					{
						if (!IsValid(CurInputObj))
							continue;

						if (CurInputObj->GetImportAsReferenceRotScaleEnabled() != bNewState)
						{
							CurInputObj->MarkChanged(true);
						}
					}
				}

				CurInput->Modify();
				CurInput->SetImportAsReferenceRotScaleEnabled(bNewState);
				CurInput->MarkChanged(true);
			}
		};
		
		if (IsCheckedImportAsReference(MainInput) == ECheckBoxState::Checked) {
			TSharedPtr<SCheckBox> CheckBoxImportAsReferenceRotScale;
			
			InVerticalBox->AddSlot()
			.Padding(10, 2, 5, 2)
			.AutoHeight()
			[
				SAssignNew(CheckBoxImportAsReferenceRotScale, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ExportInputAsRefRotScaleCheckbox", "Add Rotation/Scale to References"))
					.ToolTipText(LOCTEXT("ExportInputAsRefRotScaleTip", "Add rotation/scale attributes to the input references when Export Input as References is enabled."))
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([=]()
				{
					return IsCheckedImportAsReferenceRotScale(MainInput);
				})
				.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
				{
					return CheckStateChangedImportAsReferenceRotScale(InInputs, NewState);
				})
			];
		}
	}

	// 2 - Add bounding box min/max checkbox
	{
		auto IsCheckedImportAsReferenceBbox = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
		{
			if (!IsValidWeakPointer(InInput))
				return ECheckBoxState::Unchecked;
			
			return InInput->GetImportAsReferenceBboxEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto CheckStateChangedImportAsReferenceBbox = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			bool bNewState = (NewState == ECheckBoxState::Checked);

			if (MainInput->GetImportAsReferenceBboxEnabled() == bNewState)
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputAsReferenceBbox", "Houdini Input: Changed Add Bbox Min/Max to References CheckBox"),
				MainInput->GetOuter());

			for (auto CurInput : InInputsToUpdate)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->GetImportAsReferenceBboxEnabled() == bNewState)
					continue;

				TArray<TObjectPtr<UHoudiniInputObject>>* InputObjs = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
				if (InputObjs)
				{
					for (auto CurInputObj : *InputObjs)
					{
						if (!IsValid(CurInputObj))
							continue;

						if (CurInputObj->GetImportAsReferenceBboxEnabled() != bNewState)
						{
							CurInputObj->MarkChanged(true);
						}
					}
				}

				CurInput->Modify();
				CurInput->SetImportAsReferenceBboxEnabled(bNewState);
				CurInput->MarkChanged(true);
			}
		};

		if (IsCheckedImportAsReference(MainInput) == ECheckBoxState::Checked)
		{
			TSharedPtr<SCheckBox> CheckBoxImportAsReferenceBbox;
			
			InVerticalBox->AddSlot()
			.Padding(10, 2, 5, 2)
			.AutoHeight()
			[
				SAssignNew(CheckBoxImportAsReferenceBbox, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ExportInputAsRefBboxCheckBox", "Add Bounding Box Min/Max to References"))
					.ToolTipText(LOCTEXT("ExportInputAsRefBboxTip", "Add bounding box min/max attributes to the input references when Export Input as References is enabled."))
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([=]()
				{
					return IsCheckedImportAsReferenceBbox(MainInput);
				})
				.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
				{
					return CheckStateChangedImportAsReferenceBbox(InInputs, NewState);
				})
			];
		}
	}

	// 3 - Add material checkbox
	{
		auto IsCheckedImportAsReferenceMaterial = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
		{
			if (!IsValidWeakPointer(InInput))
				return ECheckBoxState::Unchecked;
			
			return InInput->GetImportAsReferenceMaterialEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto CheckStateChangedImportAsReferenceMaterial = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			bool bNewState = (NewState == ECheckBoxState::Checked);

			if (MainInput->GetImportAsReferenceMaterialEnabled() == bNewState)
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputAsReferenceMaterial", "Houdini Input: Changed Add Material to References CheckBox"),
				MainInput->GetOuter());

			for (auto CurInput : InInputsToUpdate)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->GetImportAsReferenceMaterialEnabled() == bNewState)
					continue;

				TArray<TObjectPtr<UHoudiniInputObject>>* InputObjs = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
				if (InputObjs)
				{
					for (auto CurInputObj : *InputObjs)
					{
						if (!IsValid(CurInputObj))
							continue;

						if (CurInputObj->GetImportAsReferenceMaterialEnabled() != bNewState)
						{
							CurInputObj->MarkChanged(true);
						}
					}
				}

				CurInput->Modify();
				CurInput->SetImportAsReferenceMaterialEnabled(bNewState);
				CurInput->MarkChanged(true);
			}
		};

		if (IsCheckedImportAsReference(MainInput) == ECheckBoxState::Checked)
		{
			TSharedPtr<SCheckBox> CheckBoxImportAsReferenceMaterial;
			
			InVerticalBox->AddSlot()
			.Padding(10, 2, 5, 2)
			.AutoHeight()
			[
				SAssignNew(CheckBoxImportAsReferenceMaterial, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ExportInputAsRefMaterialCheckBox", "Add Material to References"))
					.ToolTipText(LOCTEXT("ExportInputAsRefMaterialTip", "Add material attributes to the input references when Export Input as References is enabled."))
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([=]()
				{
					return IsCheckedImportAsReferenceMaterial(MainInput);
				})
				.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
				{
					return CheckStateChangedImportAsReferenceMaterial(InInputs, NewState);
				})
			];
		}
	}
}

void
FHoudiniInputDetails::AddDirectlyConnectHdasCheckBox(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	auto DirectlyConnectHdas = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->GetDirectlyConnectHdas() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto CheckStateChangedDirectlyConnectHdas = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->GetDirectlyConnectHdas() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Directly Connect HDAs CheckBox"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetDirectlyConnectHdas() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetDirectlyConnectHdas(bNewState);
			CurInput->MarkChanged(true);
		}
	};

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DirectlyConnectHdasCheckBox", "Directly Connect HDAs in Houdini"))
			.ToolTipText(LOCTEXT("DirectlyConnectHdasCheckBoxTip", "Directly connect HDAs in Houdini instead of processing input HDAs separately."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([=]()
		{
			return DirectlyConnectHdas(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedDirectlyConnectHdas(InInputs, NewState);
		})
	];
}

void
FHoudiniInputDetails::AddUnrealSplineResolutionInput(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	auto UnrealSplineResolutionChanged = [MainInput, InInputs](float NewVal)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Unreal Spline Resolution"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetUnrealSplineResolution() == NewVal)
				continue;

			CurInput->Modify();
			CurInput->SetUnrealSplineResolution(NewVal);
			CurInput->MarkChanged(true);
		}
	};

	auto ResetUnrealSplineResolution = [MainInput, InInputs]()
	{
		if (!IsValidWeakPointer(MainInput))
			return FReply::Handled();

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Reset Unreal Spline Resolution"),
			MainInput->GetOuter());

		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		float DefaultSplineResolution = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->MarshallingSplineResolution : 50;

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetUnrealSplineResolution() == DefaultSplineResolution)
				continue;

			CurInput->Modify();
			CurInput->SetUnrealSplineResolution(DefaultSplineResolution);
			CurInput->MarkChanged(true);
		}
		
		return FReply::Handled();
	};

	auto GetUnrealSplineResolutionValue = [MainInput]()
	{
		if (!IsValidWeakPointer(MainInput))
			return TOptional<float>();

		return TOptional<float>(MainInput->GetUnrealSplineResolution());
	};

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UnrealSplineResolution", "Unreal Spline Resolution"))
			.ToolTipText(LOCTEXT("UnrealSplineResolutionTip", "Resolution used when marshalling the Unreal Splines to HoudiniEngine.\n(step in cm between control points)\nSet this to 0 to only export the control points."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		+ SHorizontalBox::Slot()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(true)
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.MinValue(0)
			.MaxValue(1000)
			.MinSliderValue(0)
			.MaxSliderValue(1000)
			.Value_Lambda([GetUnrealSplineResolutionValue]()
			{
				return GetUnrealSplineResolutionValue();
			})
			.SliderExponent(1)
			.OnValueChanged_Lambda([UnrealSplineResolutionChanged](float NewVal)
			{
				UnrealSplineResolutionChanged(NewVal);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("UnrealSpineResolutionDefault", "Result to default value."))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([ResetUnrealSplineResolution]()
			{
				return ResetUnrealSplineResolution();
			})
			[
				SNew(SImage)
				.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];
}

void
FHoudiniInputDetails::AddUseLegacyInputCurvesCheckBox(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	auto UseLegacyInputCurves = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->IsUseLegacyInputCurvesEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto CheckStateChangedUseLegacyInputCurves = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->IsUseLegacyInputCurvesEnabled() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Use Legacy Input Curves CheckBox"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsUseLegacyInputCurvesEnabled() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetUseLegacyInputCurve(bNewState);
			CurInput->MarkChanged(true);
		}
	};

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UseLegacyInputCurvesCheckBox", "Use Legacy Input Curves"))
			.ToolTipText(LOCTEXT("UseLegacyInputCurvesCheckBoxTip", "If enabled, the deprecated curve::1.0 node will be used for spline inputs."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([=]()
		{
			return UseLegacyInputCurves(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedUseLegacyInputCurves(InInputs, NewState);
		})
	];
}

void
FHoudiniInputDetails::AddExportSelectedLandscapesOnlyCheckBox(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	InVerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ExportEditLayersDataCheckBox", "Export Height Data Per Edit Layer"))
			.ToolTipText(LOCTEXT("ExportEditLayersDataCheckBoxTip", "Exports Edit Layer data. If Edit Layers are not needed disabling this will improve performance."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->IsEditLayerHeightExportEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Selected Landscape Components Only CheckBox"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				bool bNewState = (NewState == ECheckBoxState::Checked);
				if (bNewState == CurrentInput->IsEditLayerHeightExportEnabled())
					continue;

				CurrentInput->Modify();
				CurrentInput->SetExportHeightDataPerEditLayer(bNewState);
				CurrentInput->UpdateLandscapeInputSelection();
				CurrentInput->MarkChanged(true);
				CurrentInput->MarkAllInputObjectsChanged(true);
			}
		})
	];

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ExportPaintLayersPerEditLauyerCheckBox", "Export Paint Layers Per Edit Layer"))
			.ToolTipText(LOCTEXT("ExportPaintLayersPerEditLauyerCheckBoxTip", "Exports Paint Layer data per Edit Layer. If Edit Layers are not needed disabling this will improve performance."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->IsPaintLayerPerEditLayerExportEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Selected Landscape Components Only CheckBox"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				bool bNewState = (NewState == ECheckBoxState::Checked);
				if (bNewState == CurrentInput->IsPaintLayerPerEditLayerExportEnabled())
					continue;

				CurrentInput->Modify();
				CurrentInput->SetExportPaintLayerPerEditLayer(bNewState);
				CurrentInput->UpdateLandscapeInputSelection();
				CurrentInput->MarkChanged(true);
				CurrentInput->MarkAllInputObjectsChanged(true);
			}
		})
	];

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ExportMergedPaintLayersCheckBox", "Export Merged Paint Layers Per Landscape"))
			.ToolTipText(LOCTEXT("ExportMergedPaintLayersCheckBoxTip", "Exports merged Paint Layers for the landscape."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->IsMergedPaintLayerExportEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Selected Landscape Components Only CheckBox"),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->IsMergedPaintLayerExportEnabled())
						continue;

					CurrentInput->Modify();
					CurrentInput->SetExportMergedPaintLayers(bNewState);
					CurrentInput->UpdateLandscapeInputSelection();
					CurrentInput->MarkChanged(true);
					CurrentInput->MarkAllInputObjectsChanged(true);
				}
			})
		];

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ExportSelectedLandscapeCheckBox", "Export Selected Landscape Components Only"))
			.ToolTipText(LOCTEXT("ExportSelectedLandscapeCheckBoxTip", "If enabled, only the selected landscape components will be exported."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->IsLandscapeExportSelectionOnlyEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Selected Landscape Components Only CheckBox"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				bool bNewState = (NewState == ECheckBoxState::Checked);
				if (bNewState == CurrentInput->IsLandscapeExportSelectionOnlyEnabled())
					continue;

				CurrentInput->Modify();
				CurrentInput->SetLandscapeExportSelectionOnlyEnabled(bNewState);
				CurrentInput->UpdateLandscapeInputSelection();
				CurrentInput->MarkChanged(true);
				CurrentInput->MarkAllInputObjectsChanged(true);
			}
		})
	];

	if (MainInput->IsLandscapeExportSelectionOnlyEnabled())
	{
		InVerticalBox->AddSlot()
		.Padding(10, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AutoSelectComponentCheckbox", "Auto Select Components in Asset Bounds"))
				.ToolTipText(LOCTEXT("AutoSelectComponentCheckboxTooltip", "If enabled, when no Landscape components are currently selected, the one within the asset's bounding box will be exported."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->IsLandscapeAutoSelectComponentEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape Auto Select Components in Asset Bounds CheckBox"),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->IsLandscapeAutoSelectComponentEnabled())
						continue;

					CurrentInput->Modify();
					CurrentInput->SetLandscapeAutoSelectComponentEnabled(bNewState);
					CurrentInput->UpdateLandscapeInputSelection();
					CurrentInput->MarkChanged(true);
				}
			})
		];
	}
}

void
FHoudiniInputDetails::AddExportLandscapeAsOptions(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("LandscapeExportAs", "Export Landscape as"))
		.ToolTipText(LOCTEXT("LandscapeExportAsTip", "Choose the type of data you want the ladscape to be exported to:\n * Heightfield\n * Mesh\n * Points"))
		.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	TSharedPtr <SUniformGridPanel> ButtonOptionsPanel;
	InVerticalBox->AddSlot()
	.Padding(2, 4, 5, 4)
	.AutoHeight()
	[
		SAssignNew(ButtonOptionsPanel, SUniformGridPanel)
	];

	auto IsCheckedExportAs = [](const TWeakObjectPtr<UHoudiniInput>& Input, const EHoudiniLandscapeExportType& LandscapeExportType)
	{
		if (!IsValidWeakPointer(Input))
			return ECheckBoxState::Unchecked;

		return Input->GetLandscapeExportType() == LandscapeExportType ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto CheckStateChangedExportAs = [](const TWeakObjectPtr<UHoudiniInput>& Input, const EHoudiniLandscapeExportType& LandscapeExportType)
	{
		if (!IsValidWeakPointer(Input))
			return false;

		if (Input->GetLandscapeExportType() == LandscapeExportType)
			return false;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape Export Type"),
			Input->GetOuter());
		Input->Modify();

		Input->SetLandscapeExportType(LandscapeExportType);
		Input->SetHasLandscapeExportTypeChanged(true);
		Input->MarkChanged(true);

		TArray<TObjectPtr<UHoudiniInputObject>>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!LandscapeInputObjectsArray)
			return true;

		for (UHoudiniInputObject* NextInputObj : *LandscapeInputObjectsArray)
		{
			if (!NextInputObj)
				continue;
			NextInputObj->MarkChanged(true);
		}

		return true;
	};

	// Heightfield
	FText HeightfieldTooltip = LOCTEXT("LandscapeExportAsHeightfieldTip", "If enabled, the landscape will be exported to Houdini as a heightfield.");
	ButtonOptionsPanel->AddSlot(0, 0)
	[
		SNew(SCheckBox)
		.Style(_GetEditorStyle(), "Property.ToggleButton.Start")
		.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
		{
			return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Heightfield);
		})
		.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
		{
			for (auto CurrentInput : InInputs)
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
				.Image(_GetEditorStyle().GetBrush("ClassIcon.LandscapeComponent"))
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
	FText MeshTooltip = LOCTEXT("LandscapeExportAsHeightfieldTip", "If enabled, the landscape will be exported to Houdini as a mesh.");
	ButtonOptionsPanel->AddSlot(1, 0)
	[
		SNew(SCheckBox)
		.Style(_GetEditorStyle(), "Property.ToggleButton.Middle")
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
				.Image(_GetEditorStyle().GetBrush("ClassIcon.StaticMeshComponent"))
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
	FText PointsTooltip = LOCTEXT("LandscapeExportAsPointsTip", "If enabled, the landscape will be exported to Houdini as points.");
	ButtonOptionsPanel->AddSlot(2, 0)
	[
		SNew(SCheckBox)
		.Style(_GetEditorStyle(), "Property.ToggleButton.End")
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
				.Image(_GetEditorStyle().GetBrush("Mobility.Static"))
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

	// The following checkbox are only added when not in heightfield mode
	if (MainInput->GetLandscapeExportType() != EHoudiniLandscapeExportType::Heightfield)
	{
		TSharedPtr<SCheckBox> CheckBoxExportMaterials;
		InVerticalBox->AddSlot()
		.Padding(10, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(CheckBoxExportMaterials, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeMaterialsCheckbox", "Export Landscape Materials"))
				.ToolTipText(LOCTEXT("LandscapeMaterialsTooltip", "If enabled, the landscape materials will be exported with it."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->IsLandscapeExportMaterialsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Landscape Materials CheckBox"),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->IsLandscapeExportMaterialsEnabled())
						continue;

					CurrentInput->Modify();

					CurrentInput->SetLandscapeExportMaterialsEnabled(bNewState);
					CurrentInput->MarkChanged(true);
				}
			})
		];

		TSharedPtr<SCheckBox> CheckBoxExportTileUVs;
		InVerticalBox->AddSlot()
		.Padding(10, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(CheckBoxExportTileUVs, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeTileUVsCheckbox", "Export Landscape Tile UVs"))
				.ToolTipText(LOCTEXT("LandscapeTileUVsTooltip", "If enabled, UVs will be exported separately for each Landscape tile."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->IsLandscapeExportTileUVsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Landscape Tile UVs CheckBox"),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->IsLandscapeExportTileUVsEnabled())
						continue;

					CurrentInput->Modify();

					CurrentInput->SetLandscapeExportTileUVsEnabled(bNewState);
					CurrentInput->MarkChanged(true);
				}
			})
		];

		TSharedPtr<SCheckBox> CheckBoxExportNormalizedUVs;
		InVerticalBox->AddSlot()
		.Padding(10, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(CheckBoxExportNormalizedUVs, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs"))
				.ToolTipText(LOCTEXT("LandscapeNormalizedUVsTooltip", "If enabled, landscape UVs will be exported in [0, 1]."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->IsLandscapeExportNormalizedUVsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Landscape Normalized UVs"),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->IsLandscapeExportNormalizedUVsEnabled())
						continue;

					CurrentInput->Modify();

					CurrentInput->SetLandscapeExportNormalizedUVsEnabled(bNewState);
					CurrentInput->MarkChanged(true);
				}
			})
		];

		TSharedPtr<SCheckBox> CheckBoxExportLighting;
		InVerticalBox->AddSlot()
		.Padding(10, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(CheckBoxExportLighting, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeLightingCheckbox", "Export Landscape Lighting"))
				.ToolTipText(LOCTEXT("LandscapeLightingTooltip", "If enabled, lightmap information will be exported with the landscape."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				if (!IsValidWeakPointer(MainInput))
					return ECheckBoxState::Unchecked;

				return MainInput->IsLandscapeExportLightingEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Export Landscape Lighting"),
					MainInput->GetOuter());

				for (auto CurrentInput : InInputs)
				{
					if (!IsValidWeakPointer(CurrentInput))
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->IsLandscapeExportLightingEnabled())
						continue;

					CurrentInput->Modify();

					CurrentInput->SetLandscapeExportLightingEnabled(bNewState);
					CurrentInput->MarkChanged(true);
				}
			})
		];

	}
}

void
FHoudiniInputDetails::AddLandscapeAutoSelectSplinesCheckBox(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeAutoSelectSplinesCheckBox", "Auto Select Landscape Splines For Export"))
			.ToolTipText(LOCTEXT("LandscapeAutoSelectSplinesCheckBoxTip", "If enabled, then for any landscape that is selected to be sent to Houdini, its splines are also selected for export."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->IsLandscapeAutoSelectSplinesEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([InInputs, MainInput](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Auto Select Landscape Splines For Export CheckBox"),
				MainInput->GetOuter());

			for (auto CurrentInput : InInputs)
			{
				if (!IsValidWeakPointer(CurrentInput))
					continue;

				const bool bNewState = (NewState == ECheckBoxState::Checked);
				if (bNewState == CurrentInput->IsLandscapeAutoSelectSplinesEnabled())
					continue;

				CurrentInput->Modify();
				CurrentInput->SetLandscapeAutoSelectSplines(bNewState);
				if (CurrentInput->GetInputType() == EHoudiniInputType::World)
				{
					if (bNewState)
					{
						CurrentInput->AddAllLandscapeSplineActorsForInputLandscapes();	
					}
					else
					{
						CurrentInput->RemoveAllLandscapeSplineActorsForInputLandscapes();
					}
				}				
				CurrentInput->MarkChanged(true);
			}
		})
	];

	if (MainInput->IsLandscapeAutoSelectSplinesEnabled())
	{
		// TODO: additional options?
	}
}

void
FHoudiniInputDetails::AddExportOptions(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	auto ExportOptionsMenuStateChanged = [MainInput, InInputs](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;
		if (MainInput->GetExportOptionsMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniExportOptionsMenu", "Houdini Input: Changed Export Options Menu State"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetExportOptionsMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetExportOptionsMenuExpanded(bNewState);
		}
	};

	TSharedRef<SVerticalBox> ExportOptions_VerticalBox = SNew(SVerticalBox);

	TSharedRef<SExpandableArea> ExportOptions_Expandable = SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("NewInputsExportOptionsMenu", "Export Options"))
		.InitiallyCollapsed(!MainInput->GetExportOptionsMenuExpanded())
		.MinWidth(350.0f)
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(ExportOptionsMenuStateChanged))
		.BodyContent()
		[
			ExportOptions_VerticalBox
		];

	switch (MainInput->GetInputType())
	{
		case EHoudiniInputType::Geometry:
		{
			AddKeepWorldTransformCheckBox(ExportOptions_VerticalBox, InInputs);
			AddPackBeforeMergeCheckbox(ExportOptions_VerticalBox, InInputs);
			AddExportAsReferenceCheckBoxes(ExportOptions_VerticalBox, InInputs);
			AddExportCheckboxes(ExportOptions_VerticalBox, InInputs);
		}
		break;

		case EHoudiniInputType::World:
		{
			AddKeepWorldTransformCheckBox(ExportOptions_VerticalBox, InInputs);
			AddPackBeforeMergeCheckbox(ExportOptions_VerticalBox, InInputs);
			AddExportAsReferenceCheckBoxes(ExportOptions_VerticalBox, InInputs);
			AddExportCheckboxes(ExportOptions_VerticalBox, InInputs);
			AddLevelInstanceExportCheckboxes(ExportOptions_VerticalBox, InInputs);
			AddDirectlyConnectHdasCheckBox(ExportOptions_VerticalBox, InInputs);
			AddUseLegacyInputCurvesCheckBox(ExportOptions_VerticalBox, InInputs);
			AddUnrealSplineResolutionInput(ExportOptions_VerticalBox, InInputs);
		}
		break;

		case EHoudiniInputType::Curve:
		{
			AddKeepWorldTransformCheckBox(ExportOptions_VerticalBox, InInputs);
			AddCurveAutoUpdateCheckBox(ExportOptions_VerticalBox, InInputs);
			AddCurveRotScaleAttributesCheckBox(ExportOptions_VerticalBox, InInputs);
			AddUseLegacyInputCurvesCheckBox(ExportOptions_VerticalBox, InInputs);
		}
		break;

		default:
			break;
	}

	EHoudiniInputType InputType = MainInput->GetInputType();

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.FillWidth(350.0f)
		[
			ExportOptions_Expandable
		]
	];
}

void
FHoudiniInputDetails::AddLandscapeOptions(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	auto LandscapeOptionsMenuStateChanged = [MainInput, InInputs](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;

		if (MainInput->GetLandscapeOptionsMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniLandscapeOptionsMenu", "Houdini Input: Changed Landscape Options Menu State"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetLandscapeOptionsMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetLandscapeOptionsMenuExpanded(bNewState);
		}
	};

	TSharedRef<SVerticalBox> LandscapeOptions_VerticalBox = SNew(SVerticalBox);

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("LandscapeOptionsMenu", "Landscape Options"))
		.InitiallyCollapsed(!MainInput->GetLandscapeOptionsMenuExpanded())
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(LandscapeOptionsMenuStateChanged))
		.BodyContent()
		[
			LandscapeOptions_VerticalBox
		]
	];

	AddExportSelectedLandscapesOnlyCheckBox(LandscapeOptions_VerticalBox, InInputs);
	AddExportLandscapeAsOptions(LandscapeOptions_VerticalBox, InInputs);
}

void
FHoudiniInputDetails::AddLandscapeSplinesOptions(
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	auto LandscapeOptionsMenuStateChanged = [MainInput, InInputs](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;

		if (MainInput->IsLandscapeSplinesExportOptionsMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniLandscapeSplinesOptionsMenu", "Houdini Input: Changed Landscape Splines Options Menu State"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsLandscapeSplinesExportOptionsMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetLandscapeSplinesExportOptionsMenuExpanded(bNewState);
		}
	};

	const TSharedRef<SVerticalBox> LandscapeSplinesOptions_VerticalBox = SNew(SVerticalBox);

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("LandscapeSplinesOptionsMenu", "Landscape Splines Options"))
		.InitiallyCollapsed(!MainInput->IsLandscapeSplinesExportOptionsMenuExpanded())
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(LandscapeOptionsMenuStateChanged))
		.BodyContent()
		[
			LandscapeSplinesOptions_VerticalBox
		]
	];

	// Lambda returning a CheckState from the input's current bLandscapeSplinesExportControlPoints state
	auto IsLandscapeSplinesExportControlPointsEnabled = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->IsLandscapeSplinesExportControlPointsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing bLandscapeSplinesExportControlPoints state
	auto CheckStateChangedLandscapeSplinesExportControlPoints = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->IsLandscapeSplinesExportControlPointsEnabled() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Export Landscape Spline Control Points"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsLandscapeSplinesExportControlPointsEnabled() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetLandscapeSplinesExportControlPoints(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda returning a CheckState from the input's current bLandscapeSplinesExportLeftRightCurves state
	auto IsLandscapeSplinesExportLeftRightCurvesEnabled = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->IsLandscapeSplinesExportLeftRightCurvesEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing bLandscapeSplinesExportLeftRightCurves state
	auto CheckStateChangedLandscapeSplinesExportLeftRightCurves = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->IsLandscapeSplinesExportLeftRightCurvesEnabled() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChangeLandscapeSplinesExportLeftRightCurves", "Houdini Input: Changed Export Landscape Spline Left/Right Curves"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsLandscapeSplinesExportLeftRightCurvesEnabled() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetLandscapeSplinesExportLeftRightCurves(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda returning a CheckState from the input's current bLandscapeSplinesExportSplineMeshComponents state
	auto IsLandscapeSplinesExportSplineMeshComponentsEnabled = [](const TWeakObjectPtr<UHoudiniInput>& InInput)
	{
		if (!IsValidWeakPointer(InInput))
			return ECheckBoxState::Unchecked;

		return InInput->IsLandscapeSplinesExportSplineMeshComponentsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing bLandscapeSplinesExportSplineMeshComponents state
	auto CheckStateChangedLandscapeSplinesExportSplineMeshComponents = [MainInput](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, ECheckBoxState NewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		if (MainInput->IsLandscapeSplinesExportSplineMeshComponentsEnabled() == bNewState)
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChangeLandscapeSplinesExportSplineMeshComponents", "Houdini Input: Changed Export Landscape Spline Mesh Components"),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->IsLandscapeSplinesExportSplineMeshComponentsEnabled() == bNewState)
				continue;

			CurInput->Modify();

			CurInput->SetLandscapeSplinesExportSplineMeshComponents(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	TSharedPtr<SCheckBox> CheckBoxLandscapeSplinesExportControlPoints;
	LandscapeSplinesOptions_VerticalBox->AddSlot()
	.Padding( 2, 2, 5, 2 )
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SAssignNew(CheckBoxLandscapeSplinesExportControlPoints, SCheckBox )
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportLandscapeSplineControlPoints", "Export Landscape Spline Control Points" ) )
				.ToolTipText( LOCTEXT( "ExportLandscapeSplineControlPointsCheckboxTip", "If enabled, the control points of landscape splines are exported as a point cloud." ) )
				.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.IsChecked_Lambda([=]()
			{
				return IsLandscapeSplinesExportControlPointsEnabled(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedLandscapeSplinesExportControlPoints(InInputs, NewState);
			})
		]
	];

	TSharedPtr<SCheckBox> CheckBoxLandscapeSplinesExportLeftRightCurves;
	LandscapeSplinesOptions_VerticalBox->AddSlot()
	.Padding( 2, 2, 5, 2 )
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SAssignNew(CheckBoxLandscapeSplinesExportLeftRightCurves, SCheckBox )
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportLandscapeSplineLeftRightCurves", "Export Landscape Spline Left/Right Curves" ) )
				.ToolTipText( LOCTEXT( "ExportLandscapeSplineLeftRightCurvesCheckboxTip", "If enabled, the left and right curves of landscape splines are also exported." ) )
				.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.IsChecked_Lambda([=]()
			{
				return IsLandscapeSplinesExportLeftRightCurvesEnabled(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedLandscapeSplinesExportLeftRightCurves(InInputs, NewState);
			})
		]
	];

	TSharedPtr<SCheckBox> CheckBoxLandscapeSplinesExportSplineMeshComponents;
	LandscapeSplinesOptions_VerticalBox->AddSlot()
	.Padding( 2, 2, 5, 2 )
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SAssignNew(CheckBoxLandscapeSplinesExportSplineMeshComponents, SCheckBox )
			.Content()
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "ExportLandscapeSplineMeshComponents", "Export Spline Mesh Components" ) )
				.ToolTipText( LOCTEXT( "ExportLandscapeSplineMeshComponentsCheckboxTip", "If enabled, the spline mesh components of landscape splines are also exported." ) )
				.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
			]
			.Visibility(EVisibility::Visible)
			.IsChecked_Lambda([=]()
			{
				return IsLandscapeSplinesExportSplineMeshComponentsEnabled(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedLandscapeSplinesExportSplineMeshComponents(InInputs, NewState);
			})
		]
	];

	AddLandscapeAutoSelectSplinesCheckBox(LandscapeSplinesOptions_VerticalBox, InInputs);
}

void
FHoudiniInputDetails::AddCurveInputUI(
	IDetailCategoryBuilder& CategoryBuilder,
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::Curve);

	auto SetInputObjectsCount = [MainInput, &CategoryBuilder](const int32& NewInputCount)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		UHoudiniCookable* OuterHC = Cast<UHoudiniCookable>(MainInput->GetOuter());
		if (!IsValid(OuterHC))
			return;

		// Do not insert input object when the Cookable does not finish cooking
		EHoudiniAssetState CurrentHCState = OuterHC->GetCurrentState();
		if (CurrentHCState >= EHoudiniAssetState::PreCook && CurrentHCState <= EHoudiniAssetState::Processing)
			return;

		// Clear the to be inserted object array, which records the pointers of the input objects to be inserted.
		MainInput->LastInsertedInputs.Empty();
		
		// Record the pointer of the object to be inserted before transaction for undo the insert action.
		bool bBlueprintStructureModified = false;
		UHoudiniInputHoudiniSplineComponent* NewInput =
			MainInput->CreateHoudiniSplineInput(nullptr, true, false, bBlueprintStructureModified);
		MainInput->LastInsertedInputs.Add(NewInput);

		FHoudiniEngineEditorUtils::ReselectSelectedActors();

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Added Curve Input"),
			MainInput->GetOuter());

		MainInput->Modify();
		MainInput->GetHoudiniInputObjectArray(MainInput->GetInputType())->Add(NewInput);
		MainInput->SetInputObjectsNumber(EHoudiniInputType::Curve, NewInputCount);

		if (bBlueprintStructureModified)
			FHoudiniEngineRuntimeUtils::MarkBlueprintAsStructurallyModified(OuterHC->GetComponent());

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	auto CurveInputsStateChanged = [MainInput, InInputs, &CategoryBuilder](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;

		if (MainInput->GetCurveInputsMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Curve Inputs Menu State"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetCurveInputsMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetCurveInputsMenuExpanded(bNewState);
			
			if (GEditor)
				GEditor->RedrawAllViewports();

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	AddExportOptions(InVerticalBox, InInputs);

	FText InputsMenuTitle = FText::Format(
		NumInputObjects == 1 ?
			LOCTEXT("NumArrayItemsFmt", "{0} Curve") :
			LOCTEXT("NumArrayItemsFmt", "{0} Curves"),
		FText::AsNumber(NumInputObjects));

	TSharedRef<SExpandableArea> Inputs_Expandable = SNew(SExpandableArea)
		.AreaTitle(InputsMenuTitle)
		.InitiallyCollapsed(!MainInput->GetCurveInputsMenuExpanded())
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(CurveInputsStateChanged));

	TSharedRef<SVerticalBox> InputsCollapsed_VerticalBox = SNew(SVerticalBox).Visibility_Lambda([MainInput]()
	{
		return MainInput->GetCurveInputsMenuExpanded() ? EVisibility::Collapsed : EVisibility::Visible;
	});

	TSharedRef<SVerticalBox> InputsExpanded_VerticalBox = SNew(SVerticalBox).Visibility_Lambda([MainInput]()
	{
		return MainInput->GetCurveInputsMenuExpanded() ? EVisibility::Visible : EVisibility::Collapsed;
	});

	InVerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()[Inputs_Expandable];
	InVerticalBox->AddSlot().Padding(5, 5, 5, 5).AutoHeight()[InputsCollapsed_VerticalBox];
	InVerticalBox->AddSlot().Padding(3, 1, 5, 2).AutoHeight()[InputsExpanded_VerticalBox];

	InputsExpanded_VerticalBox->AddSlot()
	.Padding(2, 0, 5, 15)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked_Lambda([SetInputObjectsCount, InInputs, NumInputObjects]()
			{
				SetInputObjectsCount(NumInputObjects + 1);
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Plus"))
					.ColorAndOpacity(FStyleColors::AccentGreen)
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddButtonText", "Add"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("AddButtonTip", "Adds a new Curve input."))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(3, 0, 0, 0))
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked_Lambda([InInputs, MainInput, &CategoryBuilder]()
			{
				TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputComponentArray = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);

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
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Delete"))
					.ColorAndOpacity(FStyleColors::AccentRed)
				]
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ClearAllButtonText", "Clear All"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("ClearAllButtonTip", "Clears all inputs."))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(3, 0, 0, 0))
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked_Lambda([MainInput]()
			{
				MainInput->ResetDefaultCurveOffset();
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
					.ColorAndOpacity(FStyleColors::AccentYellow)
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ResetButtonText", "Reset Offset"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("ResetButtonTip", "Reset offset for all inputs."))
				]
			]
		]
	];

	TSharedPtr<FHoudiniSplineComponentVisualizer> SplineComponentVisualizer;
	if (GUnrealEd)
	{
		TSharedPtr<FComponentVisualizer> Visualizer =
			GUnrealEd->FindComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());
		SplineComponentVisualizer = StaticCastSharedPtr<FHoudiniSplineComponentVisualizer>(Visualizer);
	}

	for (int32 Idx = 0; Idx < NumInputObjects; Idx++)
	{
		Helper_CreateCurveWidgetCollapsed(CategoryBuilder, InInputs, Idx, AssetThumbnailPool, InputsCollapsed_VerticalBox, SplineComponentVisualizer);
		Helper_CreateCurveWidgetExpanded(CategoryBuilder, InInputs, Idx, AssetThumbnailPool, InputsExpanded_VerticalBox, SplineComponentVisualizer);
	}

	auto PointSelectionMenuChanged = [MainInput, InInputs](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;

		if (MainInput->GetCurvePointSelectionMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurvePointSelectionMenu", "Houdini Input: Changed Curve Point Selection Menu State"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetCurvePointSelectionMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetCurvePointSelectionMenuExpanded(bNewState);
		}
	};

	TSharedRef<SVerticalBox> PointSelection_VerticalBox = SNew(SVerticalBox);

	TSharedRef<SExpandableArea> PointSelection_Expandable = SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("HoudiniCurvePointSelectionMenuTitle", "Spline Point Editor"))
		.InitiallyCollapsed(!MainInput->GetCurvePointSelectionMenuExpanded())
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(PointSelectionMenuChanged))
		.BodyContent()
		[
			PointSelection_VerticalBox
		];

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		[
			PointSelection_Expandable
		]
	];

	Helper_AddCurvePointSelectionUI(CategoryBuilder, InInputs, AssetThumbnailPool, PointSelection_VerticalBox, SplineComponentVisualizer);
}

void
FHoudiniInputDetails::Helper_CreateCurveWidgetExpanded(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const int32& InObjIdx,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool,
	TSharedRef<SVerticalBox> InVerticalBox,
	TSharedPtr<class FHoudiniSplineComponentVisualizer> HouSplineComponentVisualizer)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	UHoudiniCookable* OuterHC = Cast<UHoudiniCookable>(MainInput->GetOuter());
	if (!IsValid(OuterHC))
		return;

	TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!CurveInputs)
		return;

	if (!CurveInputs->IsValidIndex(InObjIdx))
		return;

	UHoudiniInputObject* HoudiniInputObject = (*CurveInputs)[InObjIdx];
	UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject = Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);
	UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();
	if (!HoudiniSplineComponent)
		return;

	bool bUseLegacyCurve = HoudiniSplineComponent->IsLegacyInputCurve();
	FString CurveName = HoudiniSplineComponent->GetHoudiniSplineName();
	int32 NumCurvePoints = HoudiniSplineComponent->GetCurvePointCount();
	bool bIsClosed = HoudiniSplineComponent->IsClosedCurve();
	bool bIsReversed = HoudiniSplineComponent->IsReversed();
	bool bIsVisible = HoudiniSplineComponent->IsHoudiniSplineVisible();

	auto GetHoudiniSplineComponentAtIdx = [](const TWeakObjectPtr<UHoudiniInput>& InInput, int32 Idx)
	{
		UHoudiniSplineComponent* FoundHoudiniSplineComponent = nullptr;
		if (!IsValidWeakPointer(InInput))
			return FoundHoudiniSplineComponent;

		// Get the TArray ptr to the curve objects in this input
		TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputComponentArray = InInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputComponentArray)
			return FoundHoudiniSplineComponent;

		if (!CurveInputComponentArray->IsValidIndex(Idx))
			return FoundHoudiniSplineComponent;

		UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[Idx];
		UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
			Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

		FoundHoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();

		return FoundHoudiniSplineComponent;
	};

	auto DeleteCurveAtIdx = [InInputs, InObjIdx, OuterHC, CurveInputs, &CategoryBuilder]()
	{
		if (!IsValid(OuterHC))
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Deleted a curve input."),
			OuterHC);
		
		int CurveInputsNum = CurveInputs->Num();
		for (auto& CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			CurInput->Modify();

			TArray<TObjectPtr<UHoudiniInputObject>>* CurCurveInputs = CurInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurCurveInputs ||
				CurveInputsNum != CurCurveInputs->Num() ||
				!CurCurveInputs->IsValidIndex(InObjIdx))
				continue;
			
			UHoudiniInputHoudiniSplineComponent* HoudiniInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurCurveInputs)[InObjIdx]);
			if (!IsValid(HoudiniInput))
				return;

			UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
			if (!HoudiniSplineComponent)
				return;

			FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
			HoudiniSplineComponent->DetachFromComponent(DetachTransRules);

			CurInput->DeleteInputObjectAt(EHoudiniInputType::Curve, InObjIdx);
		}
		
		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	auto ChangedClosedCurve = [GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC, &CategoryBuilder](ECheckBoxState NewState)
	{
		if (!IsValid(OuterHC))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeClosed", "Houdini Input: Changed Curve Closed"),
			OuterHC);

		for (auto& Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);

			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->IsClosedCurve() == bNewState)
				continue;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetClosedCurve(bNewState);
			HoudiniSplineComponent->MarkChanged(true);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	auto ChangedReversedCurve = [GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC, &CategoryBuilder](ECheckBoxState NewState)
	{
		if (!IsValid(OuterHC))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeReversed", "Houdini Input: Changed Curve Reversed"),
			OuterHC);

		for (auto& Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->IsReversed() == bNewState)
				continue;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetReversed(bNewState);
			HoudiniSplineComponent->MarkChanged(true);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	auto ChangedVisibleCurve = [GetHoudiniSplineComponentAtIdx, InInputs, OuterHC, InObjIdx, &CategoryBuilder](ECheckBoxState NewState)
	{
		if (!IsValid(OuterHC))
			return;

		bool bNewState = (NewState == ECheckBoxState::Checked);

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeVisible", "Houdini Input: Changed Curve Visible"),
			OuterHC);

		for (auto& Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->IsHoudiniSplineVisible() == bNewState)
				continue;

			HoudiniSplineComponent->SetHoudiniSplineVisible(bNewState);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();

		if (GEditor)
			GEditor->RedrawAllViewports();
	};

	TSharedPtr<SCurveEditingTextBlock> EditingTextBlock;

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SEditableText)
					.Text(FText::FromString(CurveName))
					.OnTextCommitted_Lambda([HoudiniSplineComponent](FText NewText, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::Type::OnEnter)
							HoudiniSplineComponent->SetHoudiniSplineName(NewText.ToString());
					})
				]
				+ SHorizontalBox::Slot()
				.Padding(0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(EditingTextBlock, SCurveEditingTextBlock)
					.Text(LOCTEXT("CurveEdited", "*"))
					.ToolTipText(LOCTEXT("CurveEditedTip", "This curve is being edited."))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(8, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(
					NumCurvePoints == 1 ?
						LOCTEXT("NumCurvePointsFmt", "({0} point)") :
						LOCTEXT("NumCurvePointsFmt", "({0} points)"),
					FText::AsNumber(NumCurvePoints)))
				.ToolTipText(LOCTEXT("NumCurvePointsTip", "Number of points on this curve."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		+ SHorizontalBox::Slot()
		.Padding(20, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeDeleteButton(
					FSimpleDelegate::CreateLambda([DeleteCurveAtIdx]()
					{
						return DeleteCurveAtIdx();
					}),
					TAttribute<FText>(LOCTEXT("CurveInputDeleteTip", "Delete this curve input object.")))
			]
		]
	];

	EditingTextBlock->HoudiniSplineComponent = HoudiniSplineComponent;
	EditingTextBlock->HoudiniSplineComponentVisualizer = HouSplineComponentVisualizer;

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveClosedCheckBox", "Closed"))
				.ToolTipText(LOCTEXT("CurveClosedCheckBoxTip", "Toggle whether this curve is closed."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([bIsClosed]() {
				return bIsClosed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return ChangedClosedCurve(NewState);
			})
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveReversedCheckBox", "Reversed"))
				.ToolTipText(LOCTEXT("CurveReversedCheckBoxTip", "Toggle whether this curve is reversed."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([bIsReversed]() {
				return bIsReversed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return ChangedReversedCurve(NewState);
			})
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveVisibleCheckBox", "Visible"))
				.ToolTipText(LOCTEXT("CurveVisibleCheckBoxTip", "Toggle whether this curve is visible."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([bIsVisible]() {
				return bIsVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return ChangedVisibleCurve(NewState);
			})
		]
	];

	auto GetCurveTypeText = [HoudiniSplineComponent]()
	{
		return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(HoudiniSplineComponent->GetCurveType()));
	};

	auto OnCurveTypeChanged = [GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC](TSharedPtr<FString> InNewChoice)
	{
		if (!IsValid(OuterHC))
			return;

		if (!InNewChoice.IsValid())
			return;

		EHoudiniCurveType NewInputType = UHoudiniInput::StringToHoudiniCurveType(*InNewChoice.Get());

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeType", "Houdini Input: Changed Curve Type"),
			OuterHC);

		for (auto& Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->GetCurveType() == NewInputType)
				continue;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetCurveType(NewInputType);

			int32 CurveOrder = HoudiniSplineComponent->GetCurveOrder();
			if (CurveOrder < 4 && (NewInputType == EHoudiniCurveType::Nurbs || NewInputType == EHoudiniCurveType::Bezier))
				HoudiniSplineComponent->SetCurveOrder(4);
			else if (NewInputType == EHoudiniCurveType::Polygon)
				HoudiniSplineComponent->SetCurveOrder(2);

			HoudiniSplineComponent->MarkChanged(true);
		}
	};

	auto GetCurveMethodText = [HoudiniSplineComponent]()
	{
		return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(HoudiniSplineComponent->GetCurveMethod()));
	};

	auto OnCurveMethodChanged = [GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC](TSharedPtr<FString> InNewChoice)
	{
		if (!IsValid(OuterHC))
			return;

		if (!InNewChoice.IsValid())
			return;

		EHoudiniCurveMethod NewInputMethod = UHoudiniInput::StringToHoudiniCurveMethod(*InNewChoice.Get());

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniCurveInputChangeMethod", "Houdini Input: Changed Curve Method"),
			OuterHC);

		for (auto& Input : InInputs)
		{
			if (!IsValidWeakPointer(Input))
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
			if (!IsValid(HoudiniSplineComponent))
				continue;

			if (HoudiniSplineComponent->GetCurveMethod() == NewInputMethod)
				return;

			HoudiniSplineComponent->Modify();
			HoudiniSplineComponent->SetCurveMethod(NewInputMethod);
			HoudiniSplineComponent->MarkChanged(true);
		}
	};

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.FillWidth(75)
		.MaxWidth(75)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CurveTypeLabel", "Curve Type"))
			.ToolTipText(LOCTEXT("CurveTypeLabelTip", "Change the curve type for this curve."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.FillWidth(100)
		.MaxWidth(100)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveTypeChoiceLabels())
			.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveTypeChoiceLabels())[(int)HoudiniSplineComponent->GetCurveType()])
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
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
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.FillWidth(75)
		.MaxWidth(75)

		[
			SNew(STextBlock)
			.Text(LOCTEXT("CurveMethodLabel", "Curve Method"))
			.ToolTipText(LOCTEXT("CurveTypeLabelTip", "Change the curve method for this curve."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.FillWidth(100)
		.MaxWidth(100)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveMethodChoiceLabels())
			.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveMethodChoiceLabels())[(int)HoudiniSplineComponent->GetCurveMethod()])
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
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
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	if (!bUseLegacyCurve)
	{
		TSharedPtr<SNumericEntryBox<int>> NumericEntryBox;
		int32 Idx = 0;

		auto OnBreakpointParameterizationChanged = [GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC](TSharedPtr<FString> InNewChoice)
		{
			if (!IsValid(OuterHC))
				return;

			if (!InNewChoice.IsValid())
				return;

			EHoudiniCurveBreakpointParameterization NewInputMethod = UHoudiniInput::StringToHoudiniCurveBreakpointParameterization(*InNewChoice.Get());

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniCurveInputChangeBreakpointParameterization", "Houdini Input: Changed Curve Breakpoint Parameterization"),
				OuterHC);

			for (auto& Input : InInputs)
			{
				if (!IsValidWeakPointer(Input))
					continue;

				UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
				if (!IsValid(HoudiniSplineComponent))
					continue;

				if (HoudiniSplineComponent->GetCurveBreakpointParameterization() == NewInputMethod)
					return;

				HoudiniSplineComponent->Modify();
				HoudiniSplineComponent->SetCurveBreakpointParameterization(NewInputMethod);
				HoudiniSplineComponent->MarkChanged(true);
			}
		};

		auto GetCurveBreakpointParameterizationText = [HoudiniSplineComponent]()
		{
			return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveBreakpointParameterizationToString(HoudiniSplineComponent->GetCurveBreakpointParameterization()));
		};

		InVerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.FillWidth(75)
			.MaxWidth(75)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveOrder", "Curve Order"))
				.ToolTipText(LOCTEXT("CurveOrderTooltip", "Curve order of the curve. Only used for Bezier or NURBs curves. Must be a value between 2 and 11."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(5, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.FillWidth(100)
			.MaxWidth(100)
			[
				SNew(SNumericEntryBox<int>)
				.AllowSpin(true)
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.MinValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN)
				.MaxValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MAX)
				.MinSliderValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN)
				.MaxSliderValue(HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MAX)
				.Value(HoudiniSplineComponent->GetCurveOrder())
				.OnValueChanged_Lambda([GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC](int Val)
				{
					if (!IsValid(OuterHC))
						return;

					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniChangeCurveOrder", "Houdini Input: Changed curve order"),
						OuterHC);

					for (auto& Input : InInputs)
					{
						if (!IsValidWeakPointer(Input))
							continue;

						UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
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
			.Padding(2, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("OrderToDefault", "Reset to default value."))
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ContentPadding(0)
				.Visibility(HoudiniSplineComponent->GetCurveOrder() != HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_ORDER_MIN ? EVisibility::Visible : EVisibility::Hidden)
				.OnClicked_Lambda([GetHoudiniSplineComponentAtIdx, InInputs, InObjIdx, OuterHC]()
				{
					if (!IsValid(OuterHC))
						return FReply::Handled();

					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniCurveInputChangeMethod", "Houdini Input: Changed Curve Method"),
						OuterHC);

					for (auto& Input : InInputs)
					{
						if (!IsValidWeakPointer(Input))
							continue;

						UHoudiniSplineComponent* HoudiniSplineComponent = GetHoudiniSplineComponentAtIdx(Input, InObjIdx);
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
					.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];

		InVerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveBreakpointParametrizationLabel", "Breakpoint Parametrization"))
				.ToolTipText(LOCTEXT("CurveBreakpointParametrizationLabelTip", "Change the breakpoint parametrization for this curve if the curve's method is 'Breakpoints'"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(5, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.FillWidth(100)
			.MaxWidth(100)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.IsEnabled(HoudiniSplineComponent->GetCurveMethod() == EHoudiniCurveMethod::Breakpoints)
				.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveBreakpointParameterizationChoiceLabels())
				.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveBreakpointParameterizationChoiceLabels())[(int32)HoudiniSplineComponent->GetCurveBreakpointParameterization()])
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
				{
					FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
					return SNew(STextBlock)
						.Text(ChoiceEntryText)
						.ToolTipText(ChoiceEntryText)
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
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
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
		];
	}

	auto BakeInputCurveLambda = [](const TArray<TWeakObjectPtr<UHoudiniInput>>& Inputs, int32 Index, bool bBakeToBlueprint)
	{
		for (auto& NextInput : Inputs)
		{
			if (!IsValidWeakPointer(NextInput))
				continue;

			UHoudiniCookable* OuterHC = Cast<UHoudiniCookable>(NextInput->GetOuter());
			if (!IsValid(OuterHC))
				continue;

			AActor* OwnerActor = OuterHC->GetOwner();
			if (!IsValid(OwnerActor))
				continue;

			TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputComponentArray = NextInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
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
			PackageParams.BakeFolder = OuterHC->GetBakeFolderOrDefault();
			PackageParams.HoudiniAssetName = OuterHC->GetName();
			PackageParams.GeoId = NextInput->GetAssetNodeId();
			PackageParams.PackageMode = EPackageMode::Bake;
			PackageParams.ObjectId = Index;
			PackageParams.ObjectName = OwnerActor->GetActorNameOrLabel() + "InputHoudiniSpline" + FString::FromInt(Index);

			FHoudiniBakeSettings BakeSettings;

			if (bBakeToBlueprint)
			{
				FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToBlueprint(
					OuterHC,
					HoudiniSplineComponent,
					PackageParams,
					BakeSettings,
					OwnerActor->GetWorld(), OwnerActor->GetActorTransform());
			}
			else
			{
				FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
					OuterHC,
					HoudiniSplineComponent,
					PackageParams,
					BakeSettings,
					OwnerActor->GetWorld(), OwnerActor->GetActorTransform());
			}
		}

		return FReply::Handled();
	};
	
	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 15)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("CurveBakeToActorButton", "Bake to Actor"))
			.ToolTipText(LOCTEXT("CurveBakeToActorButtonTip", "Bake this input curve to Actor."))
			.OnClicked_Lambda([InInputs, InObjIdx, BakeInputCurveLambda]()
			{
				return BakeInputCurveLambda(InInputs, InObjIdx, false);
			})
		]
		+ SHorizontalBox::Slot()
		.Padding(10, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("CurveBakeToBlueprintButton", "Bake to Blueprint"))
			.ToolTipText(LOCTEXT("CurveBakeToBlueprintButtonTip", "Back this input curve to Blueprint."))
			.OnClicked_Lambda([InInputs, InObjIdx, BakeInputCurveLambda]()
			{
				return BakeInputCurveLambda(InInputs, InObjIdx, true);
			})
		]
	];
}

void
FHoudiniInputDetails::Helper_CreateCurveWidgetCollapsed(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const int32& InObjIdx,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool,
	TSharedRef<SVerticalBox> InVerticalBox,
	TSharedPtr<class FHoudiniSplineComponentVisualizer> HouSplineComponentVisualizer)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;
	/*
	UHoudiniCookable* OuterHC = Cast<UHoudiniCookable>(MainInput->GetOuter());
	if (!IsValid(OuterHC))
		return;
	*/

	TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!CurveInputs)
		return;

	if (!CurveInputs->IsValidIndex(InObjIdx))
		return;

	UHoudiniInputObject* HoudiniInputObject = (*CurveInputs)[InObjIdx];
	UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject = Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);
	UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();
	if (!HoudiniSplineComponent)
		return;

	bool bIsLegacyCurve = HoudiniSplineComponent->IsLegacyInputCurve();
	FString CurveName = HoudiniSplineComponent->GetHoudiniSplineName();
	int32 NumCurvePoints = HoudiniSplineComponent->GetCurvePointCount();
	FText CurveType = FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(HoudiniSplineComponent->GetCurveType()));
	FText CurveMethod = FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(HoudiniSplineComponent->GetCurveMethod()));

	bool bIsClosed = HoudiniSplineComponent->IsClosedCurve();
	bool bIsReversed = HoudiniSplineComponent->IsReversed();
	bool bIsVisible = HoudiniSplineComponent->IsHoudiniSplineVisible();

	TSharedPtr<SCurveEditingTextBlock> EditingTextBlock;

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(CurveName))
				]
				+ SHorizontalBox::Slot()
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(EditingTextBlock, SCurveEditingTextBlock)
					.Text(LOCTEXT("CurveEdited", "*"))
					.ToolTipText(LOCTEXT("CurveEditedTip", "This curve is being edited."))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(8, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(
					NumCurvePoints == 1 ?
						LOCTEXT("NumCurvePointsFmt", "({0} point)") :
						LOCTEXT("NumCurvePointsFmt", "({0} points)"),
					FText::AsNumber(NumCurvePoints)))
				.ToolTipText(LOCTEXT("SplineNumCurvePointsTip", "Number of points on this curve."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		+ SHorizontalBox::Slot()
		.Padding(20, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(CurveType)
				.ToolTipText(LOCTEXT("CurveTypeTip", "Curve Type"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(8, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(CurveMethod)
				.ToolTipText(LOCTEXT("CurveMethodTip", "Curve Method"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(20, 0, 0, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(bIsClosed ? 
							LOCTEXT("CurveClosedTip", "Closed") :
							LOCTEXT("CurveClosedTip", "Not closed"))
					.ButtonStyle(_GetEditorStyle(), TEXT("NoBorder"))
					.ContentPadding(0)
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(FSlateIcon(
							FHoudiniEngineStyle::GetStyleSetName(),
							bIsClosed ?
								TEXT("HoudiniEngine._CurveClosed") :
								TEXT("HoudiniEngine._CurveNotClosed"))
							.GetSmallIcon())
						.ColorAndOpacity(FSlateColor(FColor(255, 255, 255, 168)))
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(bIsReversed ?
							LOCTEXT("CurveReversedTip", "Reversed") :
							LOCTEXT("CurveReversedTip", "Not reversed"))
					.ButtonStyle(_GetEditorStyle(), TEXT("NoBorder"))
					.ContentPadding(0)
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(FSlateIcon(
							FHoudiniEngineStyle::GetStyleSetName(),
							bIsClosed ?
								TEXT("HoudiniEngine._CurveReversed") :
								TEXT("HoudiniEngine._CurveNotReversed"))
							.GetSmallIcon())
						.ColorAndOpacity(FSlateColor(FColor(255, 255, 255, 168)))
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(bIsVisible ?
							LOCTEXT("CurveVisibleTip", "Visible") :
							LOCTEXT("CurveVisibleTip", "Hidden"))
					.ButtonStyle(_GetEditorStyle(), TEXT("NoBorder"))
					.ContentPadding(0)
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(_GetEditorStyle().GetBrush(bIsVisible ? "Icons.Visible" : "Icons.Hidden"))
						.ColorAndOpacity(FSlateColor(FColor(255, 255, 255, 168)))
					]
				]
			]
		]
	];

	EditingTextBlock->HoudiniSplineComponent = HoudiniSplineComponent;
	EditingTextBlock->HoudiniSplineComponentVisualizer = HouSplineComponentVisualizer;
}

void
FHoudiniInputDetails::Helper_AddCurvePointSelectionUI(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool,
	TSharedRef<SVerticalBox> InVerticalBox,
	TSharedPtr<class FHoudiniSplineComponentVisualizer> HoudiniSplineComponentVisualizer)
{
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	if (!IsValidWeakPointer(MainInput))
		return;

	UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent();

	TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!CurveInputs || CurveInputs->Num() <= 0)
		return;

	UHoudiniInputObject* HoudiniInputObject = nullptr;
	int32 HoudiniInputObjectIdx = -1;
	if (!HoudiniSplineComponent)
	{
		// We're not currently editing spline, get the first one by default
		HoudiniInputObject = (*CurveInputs)[0];
		HoudiniInputObjectIdx = 0;
	}
	else
	{
		for (int32 Idx = 0; Idx < CurveInputs->Num(); ++Idx)
		{
			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurveInputs)[Idx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (CurSpline == HoudiniSplineComponent)
			{
				HoudiniInputObject = (*CurveInputs)[Idx];
				HoudiniInputObjectIdx = Idx;
				break;
			}
		}
	}

	if (!HoudiniInputObject || HoudiniInputObjectIdx < 0)
		return;	

	TSharedPtr<SCurveListenerVerticalBox> CurveListener;

	// The curve listener vertical box is just an empty box that is added so that the
	// corresponding SCurveListenerVerticalBox can be used to trigger updates to the
	// UI and to redraw the viewports when:
	//   - The currently selected spline is changed
	//   - The spline is moved
	//   - The selected control points are changed
	//   - The spline's control points have moved or changed
	InVerticalBox->AddSlot()
	.Padding(0)
	.MaxHeight(1)
	[
		SAssignNew(CurveListener, SCurveListenerVerticalBox)
	];
	
	CurveListener->CategoryBuilder = &CategoryBuilder;
	CurveListener->HoudiniSplineComponentVisualizer = HoudiniSplineComponentVisualizer;
	CurveListener->PrevSplineComponent = HoudiniSplineComponent;
	if (HoudiniSplineComponent)
	{
		CurveListener->PrevEditedControlPointsIndexes = HoudiniSplineComponent->EditedControlPointsIndexes;
		CurveListener->PrevSplineTransform = HoudiniSplineComponent->GetComponentTransform();
	}

	if (!HoudiniSplineComponent)
	{
		InVerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PointSelectionEmpty", "No spline is being currently edited."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		];
		return;
	}

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PointSelectionSelected", "Currently Editing "))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(HoudiniSplineComponent->GetHoudiniSplineName()))
		]
	];

	FMargin ButtonPadding(2, 0);
	InVerticalBox->AddSlot()
	.Padding(2, 6, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2, 0, 10, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PointSelectionControls", "Point Selection"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2, 0, 2, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.SelectFirst")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectFirstSplinePointToolTip", "Select first spline point."))
				.OnClicked_Lambda([=]()
				{
					HoudiniSplineComponentVisualizer->SelectSplinePoint(0, false);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.AddPrev")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectAddPrevSplinePointToolTip", "Add previous spline point to current selection."))
				.OnClicked_Lambda([=]()
				{
					HoudiniSplineComponentVisualizer->ExtendSplinePointSelection(1, true, true);
					return FReply::Handled();
				})
				.IsEnabled(HoudiniSplineComponentVisualizer->AreSplinePointsSelected())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.SelectPrev")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectPrevSplinePointToolTip", "Select previous spline point."))
				.OnClicked_Lambda([=]()
				{
					HoudiniSplineComponentVisualizer->ExtendSplinePointSelection(1, true, false);
					return FReply::Handled();
				})
				.IsEnabled(HoudiniSplineComponentVisualizer->AreSplinePointsSelected())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.SelectAll")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectAllSplinePointToolTip", "Select all spline points."))
				.OnClicked_Lambda([=]()
				{
					if (!HoudiniSplineComponent)
						return FReply::Handled();

					int32 NumTotalPoints = HoudiniSplineComponent->GetCurvePointCount();
					for (int32 Idx = 0; Idx < NumTotalPoints; ++Idx)
						HoudiniSplineComponentVisualizer->SelectSplinePoint(Idx, true);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.SelectNext")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectNextSplinePointToolTip", "Select next spline point."))
				.OnClicked_Lambda([=]()
				{
					HoudiniSplineComponentVisualizer->ExtendSplinePointSelection(1, false, false);
					return FReply::Handled();
				})
				.IsEnabled(HoudiniSplineComponentVisualizer->AreSplinePointsSelected())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.AddNext")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectAddNextSplinePointToolTip", "Add next spline point to current selection."))
				.OnClicked_Lambda([=]()
				{
					HoudiniSplineComponentVisualizer->ExtendSplinePointSelection(1, false, true);
					return FReply::Handled();
				})
				.IsEnabled(HoudiniSplineComponentVisualizer->AreSplinePointsSelected())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ButtonPadding)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "SplineComponentDetails.SelectLast")
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("SelectLastSplinePointToolTip", "Select last spline point."))
				.OnClicked_Lambda([=]()
				{
					if (!HoudiniSplineComponent)
						return FReply::Handled();
					int32 NumTotalPoints = HoudiniSplineComponent->GetCurvePointCount();
					HoudiniSplineComponentVisualizer->SelectSplinePoint(NumTotalPoints - 1, false);
					return FReply::Handled();
				})
			]
		]
	];

	TArray<int32>& EditedControlPointsIndexes = HoudiniSplineComponent->EditedControlPointsIndexes;

	auto GetSelectedPointsAsString = [](const TArray<int32>& SelectedPointIndexes)
	{
		FString FormattedString;
		for (int32 i = 0; i < SelectedPointIndexes.Num(); ++i)
		{
			FormattedString += FText::AsNumber(SelectedPointIndexes[i]).ToString();
			if (i + 1 < SelectedPointIndexes.Num())
				FormattedString += TEXT(", ");
		}
		return FormattedString;
	};

	auto UpdateSelectedPointsFromString = [MainInput, InInputs, HoudiniSplineComponentVisualizer, &CategoryBuilder](FString InputString)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputs)
			return;

		int32 SplineIdx = -1;
		UHoudiniSplineComponent* TargetSpline = HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent();
		for (int32 Idx = 0; Idx < CurveInputs->Num(); ++Idx)
		{
			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurveInputs)[Idx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (CurSpline == TargetSpline)
			{
				SplineIdx = Idx;
				break;
			}
		}

		if (SplineIdx == -1)
			return;

		// Parse the selected point indexes
		TArray<int32> SelectedPointIndexes;
		int32 MaxSelectedPointIdx = -1;
		{
			const int32 MaxPointIdx = TargetSpline->GetCurvePointCount();
			int32 Idx = 0;
			int32 InputLen = InputString.Len();
			bool bIsPrevComma = false;
			while (Idx < InputLen)
			{
				// Only allow whitespace and commas. If something else is
				// present, then the current input string is invalid
				// and the selection update is cancelled.
				if (InputString[Idx] == ',')
				{
					if (bIsPrevComma)
						return;
					bIsPrevComma = true;
					++Idx;
					continue;
				}

				// Skip whitespaces
				if (InputString[Idx] == ' ')
				{
					++Idx;
					continue;
				}

				// Require the current character to be a digit.
				if ('0' > InputString[Idx] || '9' < InputString[Idx])
					return;

				int32 CurPointIdx = 0;
				while (Idx < InputLen && '0' <= InputString[Idx] && InputString[Idx] <= '9')
				{
					CurPointIdx = (CurPointIdx * 10) + (InputString[Idx] - '0');
					++Idx;
					if (CurPointIdx >= MaxPointIdx)
						return;
				}
				if (CurPointIdx > MaxSelectedPointIdx)
					MaxSelectedPointIdx = CurPointIdx;
				if (!SelectedPointIndexes.Contains(CurPointIdx))
					SelectedPointIndexes.Add(CurPointIdx);
				bIsPrevComma = false;
			}
		}

		// If we have got this far, then we can perform the
		// update transaction.
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Updating the selected spline points from key selection."),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			TArray<TObjectPtr<UHoudiniInputObject>>* CurCurveInputs = CurInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurCurveInputs || !CurCurveInputs->IsValidIndex(SplineIdx))
				continue;

			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurCurveInputs)[SplineIdx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (!CurSpline)
				continue;

			if (MaxSelectedPointIdx >= CurSpline->GetCurvePointCount())
				continue;

			CurSpline->EditedControlPointsIndexes = SelectedPointIndexes;
		}

		if (GEditor)
			GEditor->RedrawAllViewports();

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.FillWidth(85)
		.MaxWidth(85)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectedKeys", "Selected Points"))
			.ToolTipText(LOCTEXT("SelectedKeysTip", "0-indexed collection of comma-separated points currently selected."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.FillWidth(160)
		.MaxWidth(160)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(160)
			.Text_Lambda([&EditedControlPointsIndexes, GetSelectedPointsAsString]() {
				return FText::FromString(GetSelectedPointsAsString(EditedControlPointsIndexes));
			})
			.ToolTipText(LOCTEXT("SelectedKeysInputTip", "Enter the list of points you want to selected as a 0-indexed comma-separated list."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
			.OnTextCommitted_Lambda([=](const FText& InputText, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateSelectedPointsFromString(InputText.ToString());
			})
		]
	];

	if (!HoudiniSplineComponentVisualizer->AreSplinePointsSelected())
	{
		InVerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoPointsSelected", "No control points are selected."))
			.ToolTipText(LOCTEXT("NoPointsSelectedTip", "Select at least one control point to open the transform menu."))
			.Font(_GetEditorStyle().GetFontStyle("PropertyWindow.NormalFont"))
		];
		return;
	}
	
	// From here on forward, we are guaranteed to have at least one point selected.

	// Find the input fields which have multiple values.
	TArray<FTransform>& CurvePoints = HoudiniSplineComponent->CurvePoints;
	
	bool IsSingleValue[3][3];
	FTransform Transform = CurvePoints[EditedControlPointsIndexes[0]];
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
			IsSingleValue[i][j] = 1;
	}

	for (int32 i = 1; i < EditedControlPointsIndexes.Num(); ++i)
	{
		const FTransform& CurTransform = CurvePoints[EditedControlPointsIndexes[i]];

		// Equal to UE_KINDA_SMALL_NUMBER used by UE
		const float Threshold = 1.e-4f;

		// Translate
		IsSingleValue[0][0] &= FMath::Abs(CurTransform.GetLocation().X - Transform.GetLocation().X) < Threshold;
		IsSingleValue[0][1] &= FMath::Abs(CurTransform.GetLocation().Y - Transform.GetLocation().Y) < Threshold;
		IsSingleValue[0][2] &= FMath::Abs(CurTransform.GetLocation().Z - Transform.GetLocation().Z) < Threshold;

		// Rotate
		IsSingleValue[1][0] &= FMath::Abs(CurTransform.GetRotation().Rotator().Roll - Transform.GetRotation().Rotator().Roll) < Threshold;
		IsSingleValue[1][1] &= FMath::Abs(CurTransform.GetRotation().Rotator().Pitch - Transform.GetRotation().Rotator().Pitch) < Threshold;
		IsSingleValue[1][2] &= FMath::Abs(CurTransform.GetRotation().Rotator().Yaw - Transform.GetRotation().Rotator().Yaw) < Threshold;

		// Scale
		IsSingleValue[2][0] &= FMath::Abs(CurTransform.GetScale3D().X - Transform.GetScale3D().X) < Threshold;
		IsSingleValue[2][1] &= FMath::Abs(CurTransform.GetScale3D().Y - Transform.GetScale3D().Y) < Threshold;
		IsSingleValue[2][2] &= FMath::Abs(CurTransform.GetScale3D().Z - Transform.GetScale3D().Z) < Threshold;
	}

	const FTransform& HoudiniSplineComponentTransform = HoudiniSplineComponent->GetComponentTransform();
	
	const bool bUseAbsLocation = MainInput->GetCurvePointSelectionUseAbsLocation();
	if (bUseAbsLocation)
	{
		FVector AbsoluteLocation = HoudiniSplineComponentTransform.TransformPosition(Transform.GetLocation());
		Transform.SetLocation(AbsoluteLocation);
	}

	const bool bUseAbsRotation = MainInput->GetCurvePointSelectionUseAbsRotation();
	if (bUseAbsRotation)
	{
		FQuat AbsoluteRotation = HoudiniSplineComponentTransform.GetRotation() * Transform.GetRotation();
		Transform.SetRotation(AbsoluteRotation);
	}

	auto UpdateTransformLocation = [MainInput, InInputs, HoudiniSplineComponentVisualizer](FVector InNewLocation, bool bUpdateX, bool bUpdateY, bool bUpdateZ, bool bUseAbsLocation)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputs)
			return;

		int32 SplineIdx = -1;
		UHoudiniSplineComponent* TargetSpline = HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent();
		for (int32 Idx = 0; Idx < CurveInputs->Num(); ++Idx)
		{
			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurveInputs)[Idx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (CurSpline == TargetSpline)
			{
				SplineIdx = Idx;
				break;
			}
		}

		if (SplineIdx == -1)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Modifying transform for selected spline points."),
			MainInput->GetOuter());

		const TArray<int32>& SplinePointsToUpdate = TargetSpline->EditedControlPointsIndexes;

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			TArray<TObjectPtr<UHoudiniInputObject>>* CurCurveInputs = CurInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurCurveInputs || !CurCurveInputs->IsValidIndex(SplineIdx))
				continue;

			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurCurveInputs)[SplineIdx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (!CurSpline)
				continue;

			CurInput->Modify();

			// If we are passing an absolute location, then convert it back to relative
			// before storing it back into the spline.
			FVector CurInNewLocation = InNewLocation;
			if (bUseAbsLocation)
			{
				const FTransform& CurSplineTransform = CurSpline->GetComponentTransform();
				CurInNewLocation = CurSplineTransform.InverseTransformPosition(CurInNewLocation);
			}

			const TArray<FTransform>& CurCurvePoints = CurSpline->CurvePoints;
			for (const int32& Idx : SplinePointsToUpdate)
			{
				if (!CurCurvePoints.IsValidIndex(Idx))
					continue;
				FTransform CurPoint = CurCurvePoints[Idx];
				FVector NewLocation = CurPoint.GetLocation();
				if (bUpdateX)
					NewLocation.X = CurInNewLocation.X;
				if (bUpdateY)
					NewLocation.Y = CurInNewLocation.Y;
				if (bUpdateZ)
					NewLocation.Z = CurInNewLocation.Z;
				CurPoint.SetLocation(NewLocation);
				CurSpline->EditPointAtindex(CurPoint, Idx);
			}

			CurInput->MarkChanged(true);
		}
	};

	auto UpdateTransformRotation = [MainInput, InInputs, &CategoryBuilder, HoudiniSplineComponentVisualizer](FRotator InNewRotator, bool bUpdateRoll, bool bUpdatePitch, bool bUpdateYaw, bool bUseAbsRotation)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputs)
			return;

		int32 SplineIdx = -1;
		UHoudiniSplineComponent* TargetSpline = HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent();
		for (int32 Idx = 0; Idx < CurveInputs->Num(); ++Idx)
		{
			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurveInputs)[Idx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (CurSpline == TargetSpline)
			{
				SplineIdx = Idx;
				break;
			}
		}

		if (SplineIdx == -1)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Modifying rotation for selected spline points."),
			MainInput->GetOuter());

		const TArray<int32>& SplinePointsToUpdate = TargetSpline->EditedControlPointsIndexes;

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			TArray<TObjectPtr<UHoudiniInputObject>>* CurCurveInputs = CurInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurCurveInputs || !CurCurveInputs->IsValidIndex(SplineIdx))
				continue;

			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurCurveInputs)[SplineIdx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (!CurSpline)
				continue;

			CurInput->Modify();

			FRotator CurInNewRotator = InNewRotator;
			if (bUseAbsRotation)
			{
				const FTransform& CurSplineTransform = CurSpline->GetComponentTransform();
				CurInNewRotator = (CurSplineTransform.GetRotation().Inverse() * FQuat::MakeFromRotator(InNewRotator)).Rotator();
			}

			const TArray<FTransform>& CurCurvePoints = CurSpline->CurvePoints;
			for (const int32& Idx : SplinePointsToUpdate)
			{
				if (!CurCurvePoints.IsValidIndex(Idx))
					continue;
				FTransform CurPoint = CurCurvePoints[Idx];
				FQuat NewRotation = CurPoint.GetRotation();
				FRotator NewRotator = NewRotation.Rotator();
				if (bUpdateRoll)
					NewRotator.Roll = CurInNewRotator.Roll;
				if (bUpdatePitch)
					NewRotator.Pitch = CurInNewRotator.Pitch;
				if (bUpdateYaw)
					NewRotator.Yaw = CurInNewRotator.Yaw;
				CurPoint.SetRotation(FQuat::MakeFromRotator(NewRotator));
				CurSpline->EditPointAtindex(CurPoint, Idx);
			}

			CurInput->MarkChanged(true);
		}
	};

	auto UpdateTransformScale = [MainInput, InInputs, HoudiniSplineComponentVisualizer](FVector InNewScale, bool bUpdateX, bool bUpdateY, bool bUpdateZ)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		TArray<TObjectPtr<UHoudiniInputObject>>* CurveInputs = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputs)
			return;

		int32 SplineIdx = -1;
		UHoudiniSplineComponent* TargetSpline = HoudiniSplineComponentVisualizer->GetEditedHoudiniSplineComponent();
		for (int32 Idx = 0; Idx < CurveInputs->Num(); ++Idx)
		{
			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurveInputs)[Idx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (CurSpline == TargetSpline)
			{
				SplineIdx = Idx;
				break;
			}
		}

		if (SplineIdx == -1)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Modifying scale for selected spline points."),
			MainInput->GetOuter());

		const TArray<int32>& SplinePointsToUpdate = TargetSpline->EditedControlPointsIndexes;

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			TArray<TObjectPtr<UHoudiniInputObject>>* CurCurveInputs = CurInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurCurveInputs || !CurCurveInputs->IsValidIndex(SplineIdx))
				continue;

			UHoudiniInputHoudiniSplineComponent* CurSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>((*CurCurveInputs)[SplineIdx]);
			UHoudiniSplineComponent* CurSpline = CurSplineInput->GetCurveComponent();
			if (!CurSpline)
				continue;

			CurInput->Modify();

			const TArray<FTransform>& CurCurvePoints = CurSpline->CurvePoints;
			for (const int32& Idx : SplinePointsToUpdate)
			{
				if (!CurCurvePoints.IsValidIndex(Idx))
					continue;
				FTransform CurPoint = CurCurvePoints[Idx];
				FVector NewScale = CurPoint.GetScale3D();
				if (bUpdateX)
					NewScale.X = InNewScale.X;
				if (bUpdateY)
					NewScale.Y = InNewScale.Y;
				if (bUpdateZ)
					NewScale.Z = InNewScale.Z;
				CurPoint.SetScale3D(NewScale);
				CurSpline->EditPointAtindex(CurPoint, Idx);
			}

			CurInput->MarkChanged(true);
		}
	};

	// Add Transform UI
	TSharedPtr<SVerticalBox> TransformOffset_VerticalBox;

	InVerticalBox->AddSlot()
	.Padding(0)
	.AutoHeight()
	[
		SAssignNew(TransformOffset_VerticalBox, SVerticalBox)
	];

	auto GetUseAbsoluteLocation = [MainInput](const bool& bExpectedState)
	{
		if (!IsValidWeakPointer(MainInput))
			return false;
		return MainInput->GetCurvePointSelectionUseAbsLocation() == bExpectedState;
	};

	auto SetUseAbsoluteLocation = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const bool &bNewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		if (MainInput->GetCurvePointSelectionUseAbsLocation() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Change curve point use absolute location."),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetCurvePointSelectionUseAbsLocation() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetCurvePointSelectionUseAbsLocation(bNewState);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	FMenuBuilder LocationMenu(true, NULL, NULL);

	FUIAction SetRelativeLocation
	(
		FExecuteAction::CreateLambda([=]()
		{
			SetUseAbsoluteLocation(InInputs, false);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]()
		{
			return GetUseAbsoluteLocation(false);
		})
	);

	FUIAction SetAbsoluteLocation
	(
		FExecuteAction::CreateLambda([=]()
		{
			SetUseAbsoluteLocation(InInputs, true);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]()
		{
			return GetUseAbsoluteLocation(true);
		})
	);

	LocationMenu.BeginSection(TEXT("TransformType"), FText(LOCTEXT("TransformType", "Location Type")));

	LocationMenu.AddMenuEntry
	(
		FText(LOCTEXT("LocationRelativeLabel", "Relative")),
		FText(LOCTEXT("LocationRelativeLabelTip", "Location is relative to its parents.")),
		FSlateIcon(),
		SetRelativeLocation,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);


	LocationMenu.AddMenuEntry
	(
		FText(LOCTEXT("LocationRelativeLabel", "Absolute")),
		FText(LOCTEXT("LocationRelativeLabelTip", "Location is absolute to the world.")),
		FSlateIcon(),
		SetAbsoluteLocation,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	LocationMenu.EndSection();

	// Translate
	TransformOffset_VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.FillWidth(80)
		.MaxWidth(80)
		[
			SNew(SComboButton)
			.ContentPadding(0)
			.MenuContent()
			[
				LocationMenu.MakeWidget()
			]
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(bUseAbsLocation ? LOCTEXT("LocationType", "Absolute Location") : LOCTEXT("LocationType", "Location"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(240)
		.MaxWidth(240)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.AllowSpin(true)
			.X(IsSingleValue[0][0] ? TOptional<float>(Transform.GetLocation().X) : TOptional<float>())
			.Y(IsSingleValue[0][1] ? TOptional<float>(Transform.GetLocation().Y) : TOptional<float>())
			.Z(IsSingleValue[0][2] ? TOptional<float>(Transform.GetLocation().Z) : TOptional<float>())
			.OnXCommitted_Lambda([=](float InNewX, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateTransformLocation(FVector(InNewX, 0, 0), true, false, false, bUseAbsLocation);
			})
			.OnYCommitted_Lambda([=](float InNewY, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateTransformLocation(FVector(0, InNewY, 0), false, true, false, bUseAbsLocation);
			})
			.OnZCommitted_Lambda([=](float InNewZ, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateTransformLocation(FVector(0, 0, InNewZ), false, false, true, bUseAbsLocation);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.Visibility(EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Lock"))
				]
			]
		]
	];
	
	// Rotation
	auto GetUseAbsoluteRotation = [MainInput](const bool& bExpectedState)
	{
		if (!IsValidWeakPointer(MainInput))
			return false;
		return MainInput->GetCurvePointSelectionUseAbsRotation() == bExpectedState;
	};

	auto SetUseAbsoluteRotation = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const bool &bNewState)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		if (MainInput->GetCurvePointSelectionUseAbsRotation() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Change curve point use absolute Rotation."),
			MainInput->GetOuter());

		for (auto CurInput : InInputsToUpdate)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetCurvePointSelectionUseAbsRotation() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetCurvePointSelectionUseAbsRotation(bNewState);
		}

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	FMenuBuilder RotationMenu(true, NULL, NULL);

	FUIAction SetRelativeRotation
	(
		FExecuteAction::CreateLambda([=]()
		{
			SetUseAbsoluteRotation(InInputs, false);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]()
		{
			return GetUseAbsoluteRotation(false);
		})
	);

	FUIAction SetAbsoluteRotation
	(
		FExecuteAction::CreateLambda([=]()
		{
			SetUseAbsoluteRotation(InInputs, true);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([=]()
		{
			return GetUseAbsoluteRotation(true);
		})
	);

	RotationMenu.BeginSection(TEXT("TransformType"), FText(LOCTEXT("TransformType", "Rotation Type")));

	RotationMenu.AddMenuEntry
	(
		FText(LOCTEXT("RotationRelativeLabel", "Relative")),
		FText(LOCTEXT("RotationRelativeLabelTip", "Rotation is relative to its parents.")),
		FSlateIcon(),
		SetRelativeRotation,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);


	RotationMenu.AddMenuEntry
	(
		FText(LOCTEXT("RotationRelativeLabel", "Absolute")),
		FText(LOCTEXT("RotationRelativeLabelTip", "Rotation is absolute to the world.")),
		FSlateIcon(),
		SetAbsoluteRotation,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	RotationMenu.EndSection();

	TransformOffset_VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.FillWidth(80)
		.MaxWidth(80)
		[
			SNew(SComboButton)
			.ContentPadding(0)
			.MenuContent()
			[
				RotationMenu.MakeWidget()
			]
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(bUseAbsRotation ? LOCTEXT("RotationType", "Absolute Rotation") : LOCTEXT("RotationType", "Rotation"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(240)
		.MaxWidth(240)
		[
			SNew(SRotatorInputBox)
			.AllowSpin(true)
			.bColorAxisLabels(true)
			.Roll(IsSingleValue[1][0] ? TOptional<float>(Transform.GetRotation().Rotator().Roll) : TOptional<float>())
			.Pitch(IsSingleValue[1][1] ? TOptional<float>(Transform.GetRotation().Rotator().Pitch) : TOptional<float>())
			.Yaw(IsSingleValue[1][2] ? TOptional<float>(Transform.GetRotation().Rotator().Yaw) : TOptional<float>())
			.OnRollCommitted_Lambda([=](float InNewRoll, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateTransformRotation(FRotator(InNewRoll, 0, 0), true, false, false, bUseAbsRotation);
			})
			.OnPitchCommitted_Lambda([=](float InNewPitch, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateTransformRotation(FRotator(0, InNewPitch, 0), false, true, false, bUseAbsRotation);
			})
			.OnYawCommitted_Lambda([=](float InNewYaw, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				UpdateTransformRotation(FRotator(0, 0, InNewYaw), false, false, true, bUseAbsRotation);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0.0f)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.Visibility(EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Lock"))
				]
			]
		]
	];

	const bool &bScaleLocked = HoudiniInputObject->IsUniformScaleLocked();

	TransformOffset_VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		.FillWidth(80)
		.MaxWidth(80)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CurvePointInputScale", "Scale"))
			.ToolTipText(LOCTEXT("CurvePointInputScaleTooltip", "Scale"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(240)
		.MaxWidth(240)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.X(IsSingleValue[2][0] ? TOptional<float>(Transform.GetScale3D().X) : TOptional<float>())
			.Y(IsSingleValue[2][1] ? TOptional<float>(Transform.GetScale3D().Y) : TOptional<float>())
			.Z(IsSingleValue[2][2] ? TOptional<float>(Transform.GetScale3D().Z) : TOptional<float>())
			.OnXCommitted_Lambda([=](float InNewX, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				if (bScaleLocked)
					UpdateTransformScale(FVector(InNewX, InNewX, InNewX), true, true, true);
				else
					UpdateTransformScale(FVector(InNewX, 0, 0), true, false, false);
			})
			.OnYCommitted_Lambda([=](float InNewY, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				if (bScaleLocked)
					UpdateTransformScale(FVector(InNewY, InNewY, InNewY), true, true, true);
				else
					UpdateTransformScale(FVector(0, InNewY, 0), false, true, false);
			})
			.OnZCommitted_Lambda([=](float InNewZ, ETextCommit::Type CommitType)
			{
				if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
					return;
				if (bScaleLocked)
					UpdateTransformScale(FVector(InNewZ, InNewZ, InNewZ), true, true, true);
				else
					UpdateTransformScale(FVector(0, 0, InNewZ), false, false, true);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([=, &CategoryBuilder]()
				{
					for (auto& CurInput : InInputs)
					{
						if (!IsValidWeakPointer(CurInput))
							continue;

						UHoudiniInputObject* CurInputObject = CurInput->GetHoudiniInputObjectAt(EHoudiniInputType::Curve, HoudiniInputObjectIdx);

						if (!IsValid(CurInputObject))
							continue;

						CurInputObject->SwitchUniformScaleLock();
					}

					if (CategoryBuilder.IsParentLayoutValid())
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();
			
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush(bScaleLocked ? TEXT("Icons.Lock") : TEXT("Icons.Unlock")))
				]
		
			]
		]
	];
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateWorldActorPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
{	
	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	
	auto OnShouldFilterWorld = [MainInput](const AActor* const Actor)
	{
		if (!IsValidWeakPointer(MainInput))
			return true;

		const TArray<TObjectPtr<UHoudiniInputObject>>* InputObjects = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
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
			InitOptions.bFocusSearchBoxWhenOpened = false;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Gutter_Localized()));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(_GetEditorStyle().GetBrush("Menu.Background"))
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

		const TArray<TObjectPtr<AActor>>* BoundObjects = MainInput->GetBoundSelectorObjectArray();
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
			InitOptions.bFocusSearchBoxWhenOpened = false;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Gutter_Localized()));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(_GetEditorStyle().GetBrush("Menu.Background"))
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
FHoudiniInputDetails::AddGeometryInputUI(
	IDetailCategoryBuilder& CategoryBuilder,
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);

	auto SetInputObjectsCount = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const int32& NewInputCount)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Number of New Geometry Input Objects"),
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

			if (GEditor)
				GEditor->RedrawAllViewports();

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	auto GeometryInputsStateChanged = [MainInput, InInputs, &CategoryBuilder](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;

		if (MainInput->GetGeometryInputsMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed New Geometry Inputs Menu State"),
			MainInput->GetOuter());

		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetGeometryInputsMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetGeometryInputsMenuExpanded(bNewState);
			
			if (GEditor)
				GEditor->RedrawAllViewports();

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	AddExportOptions(InVerticalBox, InInputs);

	FText InputsMenuTitle = FText::Format(
		NumInputObjects == 1 ?
			LOCTEXT("NumArrayItemsFmt", "{0} Input Selected") :
			LOCTEXT("NumArrayItemsFmt", "{0} Inputs Selected"),
		FText::AsNumber(NumInputObjects));

	TSharedRef<SExpandableArea> Inputs_Expandable = SNew(SExpandableArea)
		.AreaTitle(InputsMenuTitle)
		.InitiallyCollapsed(!MainInput->GetGeometryInputsMenuExpanded())
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(GeometryInputsStateChanged));
		
	TSharedRef<SVerticalBox> InputsCollapsed_VerticalBox = SNew(SVerticalBox).Visibility_Lambda([MainInput]()
	{
		return MainInput->GetGeometryInputsMenuExpanded() ? EVisibility::Collapsed : EVisibility::Visible;
	});
	
	TSharedRef<SVerticalBox> InputsExpanded_VerticalBox = SNew(SVerticalBox).Visibility_Lambda([MainInput]()
	{
		return MainInput->GetGeometryInputsMenuExpanded() ? EVisibility::Visible : EVisibility::Collapsed;
	});

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		Inputs_Expandable
	];

	InVerticalBox->AddSlot()
	.Padding(5, 5, 5, 5)
	.AutoHeight()
	[
		InputsCollapsed_VerticalBox
	];

	InVerticalBox->AddSlot()
	.Padding(3, 1, 5, 2)
	.AutoHeight()
	[
		InputsExpanded_VerticalBox
	];


	InputsExpanded_VerticalBox->AddSlot()
	.Padding(2, 0, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked_Lambda([SetInputObjectsCount, InInputs, NumInputObjects]()
			{
				SetInputObjectsCount(InInputs, NumInputObjects + 1);
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Plus"))
					.ColorAndOpacity(FStyleColors::AccentGreen)
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddButtonText", "Add"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("AddButtonTip", "Adds a new Geometry input."))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(3, 0, 0, 0))
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked_Lambda([SetInputObjectsCount, InInputs, NumInputObjects]()
			{
				// Delete all inputs
				SetInputObjectsCount(InInputs, 0);
				// Add a default, empty one
				SetInputObjectsCount(InInputs, 1);
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Delete"))
					.ColorAndOpacity(FStyleColors::AccentRed)
				]
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ClearAllButtonText", "Clear All"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("ClearAllButtonTip", "Clears all inputs."))
				]
			]
		]
	];

	if (NumInputObjects <= 0)
	{
		InputsExpanded_VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(1.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoInputsSelectedText", "No inputs are selected."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	}
	
	for (int32 CurObjectIdx = 0; CurObjectIdx < NumInputObjects; CurObjectIdx++)
	{
		Helper_CreateGeometryInputObjectCollapsed(CategoryBuilder, InInputs, CurObjectIdx, InputsCollapsed_VerticalBox, AssetThumbnailPool);
		Helper_CreateGeometryInputObjectExpanded(CategoryBuilder, InInputs, CurObjectIdx, InputsExpanded_VerticalBox, AssetThumbnailPool);
	}
}

void
FHoudiniInputDetails::AddWorldInputUI(
	IDetailCategoryBuilder& CategoryBuilder,
	TSharedRef<SVerticalBox> InVerticalBox,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const IDetailsView* DetailsView)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	AddExportOptions(InVerticalBox, InInputs);
	AddLandscapeOptions(InVerticalBox, InInputs);
	AddLandscapeSplinesOptions(InVerticalBox, InInputs);

	bool bIsBoundSelector = MainInput->IsWorldInputBoundSelector();

	const int32 NumInputObjects =
		bIsBoundSelector ?
			MainInput->GetNumberOfBoundSelectorObjects() :
			MainInput->GetNumberOfInputObjects(EHoudiniInputType::World);

	bool bDetailsLocked = false;
	FName DetailsPanelName = "LevelEditorSelectionDetails";
	if (DetailsView)
	{
		DetailsPanelName = DetailsView->GetIdentifier();
		if (DetailsView->IsLocked())
			bDetailsLocked = true;
	}

	auto WorldInputsStateChanged = [MainInput, InInputs, &CategoryBuilder](bool bIsExpanded)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		bool bNewState = bIsExpanded;
		if (MainInput->GetWorldInputsMenuExpanded() == bNewState)
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed New World Inputs Menu State"),
			MainInput->GetOuter());
		
		for (auto CurInput : InInputs)
		{
			if (!IsValidWeakPointer(CurInput))
				continue;

			if (CurInput->GetWorldInputsMenuExpanded() == bNewState)
				continue;

			CurInput->Modify();
			CurInput->SetWorldInputsMenuExpanded(bNewState);
		}

		if (GEditor)
			GEditor->RedrawAllViewports();

		if (CategoryBuilder.IsParentLayoutValid())
			CategoryBuilder.GetParentLayout().ForceRefreshDetails();
	};

	TSharedRef<SVerticalBox> Inputs_VerticalBox = SNew(SVerticalBox);

	FText InputSelectionType =
		bIsBoundSelector ?
			LOCTEXT("BoundSelection", "Bound") :
			LOCTEXT("InputSelection", "Input");

	FText InputsMenuTitle = FText::Format(
		NumInputObjects == 1 ?
			LOCTEXT("NumBoundsSelected", "{0} {1} Selected") :
			LOCTEXT("NumInputsSelected", "{0} {1}s Selected"),
		FText::AsNumber(NumInputObjects), InputSelectionType);

	InVerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SExpandableArea)
		.AreaTitle(InputsMenuTitle)
		.InitiallyCollapsed(!MainInput->GetWorldInputsMenuExpanded())
		.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(WorldInputsStateChanged))
		.BodyContent()
		[
			Inputs_VerticalBox
		]
	];

	// Use Bound Selector CheckBox
	Inputs_VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UseBoundSelector", "Use Bound Selector"))
			.ToolTipText(LOCTEXT("UseBoundSelectorTip", "When enabled, this world input works as a bound selector, sending all the objects contained in the bound selector bounding boxes."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->IsWorldInputBoundSelector() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([MainInput, &CategoryBuilder, InInputs](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed World Input Use Bound Selector"),
				MainInput->GetOuter());

			bool bNewState = (NewState == ECheckBoxState::Checked);

			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->IsWorldInputBoundSelector() == bNewState)
					continue;

				CurInput->Modify();
				CurInput->SetWorldInputBoundSelector(bNewState);
				CurInput->MarkChanged(true);
			}

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		})
	];
	
	// Update Selection Automatically CheckBox
	Inputs_VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AutoUpdate", "Update Automatically"))
			.ToolTipText(LOCTEXT("AutoUpdateTip", "If enabled, this input will automatically update if its selected objects/actors are changed."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([MainInput]()
		{
			if (!IsValidWeakPointer(MainInput))
				return ECheckBoxState::Unchecked;

			return MainInput->GetWorldInputAutoUpdates() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([MainInput, InInputs](ECheckBoxState NewState)
		{
			if (!IsValidWeakPointer(MainInput))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniAutoUpdateChange", "Houdini Input: Changed Update Automatically"),
				MainInput->GetOuter());

			bool bNewState = (NewState == ECheckBoxState::Checked);
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				if (CurInput->GetWorldInputAutoUpdates() == bNewState)
					continue;

				CurInput->Modify();
				CurInput->SetWorldInputAutoUpdates(bNewState);
				CurInput->MarkChanged(true);
			}
		})
	];

	// Start Selection / Use Current Selection Button
	FPropertyEditorModule& PropertyModule =
		FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FText SelectButtonText = bDetailsLocked ?
		LOCTEXT("WorldInputUseCurrentSelection", "Use Current") :
		LOCTEXT("WorldInputStartSelection", "Start Selection");

	FText SelectButtonTip = bDetailsLocked ?
		LOCTEXT("WorldInputUseCurrentSelectionTip", "Unlock details panel and use currently selected objects.") :
		LOCTEXT("WorldInputStartSelectionTip", "Lock details panel and select world objects to use as input.");

	FName SelectButtonImage = bDetailsLocked ?
		FName("Icons.ArrowDown") :
		FName("Icons.Plus");

	Inputs_VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(SelectButtonTip)
			.OnClicked_Lambda([MainInput, InInputs, DetailsPanelName, &CategoryBuilder]()
			{
				return MainInput->IsWorldInputBoundSelector() ?
					Helper_OnButtonClickUseSelectionAsBoundSelector(CategoryBuilder, InInputs, DetailsPanelName) :
					Helper_OnButtonClickSelectActors(CategoryBuilder, InInputs, DetailsPanelName);
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush(SelectButtonImage))
					.ColorAndOpacity(FStyleColors::AccentGreen)
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(SelectButtonText)
					.TextStyle(_GetEditorStyle(), "SmallButtonText")					
				]
			]
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(3, 0, 0, 0))
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("ClearSelectionButtonTip", "Clear Selection - All inputs will be removed from the current selection."))
			.OnClicked_Lambda([InInputs, MainInput, &CategoryBuilder]()			
			{
				if (!IsValidWeakPointer(MainInput))
					return FReply::Handled();

				const bool bMainInputBoundSelection = MainInput->IsWorldInputBoundSelector();

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Cleared World Input Selection"),
					MainInput->GetOuter());

				for (auto CurInput : InInputs)
				{
					if (!IsValidWeakPointer(CurInput))
						continue;

					if (CurInput->IsWorldInputBoundSelector() != bMainInputBoundSelection)
						continue;

					CurInput->Modify();

					if (bMainInputBoundSelection)
					{
						CurInput->SetBoundSelectorObjectsNumber(0);
						CurInput->UpdateWorldSelectionFromBoundSelectors();

						if (CategoryBuilder.IsParentLayoutValid())
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();
					}
					else
					{
						TArray<AActor*> EmptySelection;
						CurInput->UpdateWorldSelection(EmptySelection);
					}
				}

				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Delete"))
					.ColorAndOpacity(FStyleColors::AccentRed)
				]
				/* + SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ClearSelectionButtonText", "Clear Selection"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("ClearSelectionButtonTip", "Clears all inputs."))
				]*/
			]
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(3, 0, 0, 0))
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RefreshSelectionButtonTextTip", "Refresh Selection - Refresh the current selection and updates them in Houdini if needed."))
			.OnClicked_Lambda([MainInput, InInputs, DetailsPanelName, &CategoryBuilder]()
			{
				return Helper_OnButtonClickSelectActors(
					CategoryBuilder,
					InInputs,
					DetailsPanelName,
					MainInput->IsWorldInputBoundSelector(),
					true);
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Refresh"))
					.ColorAndOpacity(FStyleColors::AccentBlue)
				]/*
				+ SHorizontalBox::Slot()
				.Padding(FMargin(3, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RefreshSelectionButtonText", "Refresh Selection"))
					.TextStyle(_GetEditorStyle(), "SmallButtonText")
					.ToolTipText(LOCTEXT("RefreshSelectionButtonTextTip", "Refresh the current selection and updates them in Houdini if needed."))
				]*/
			]
		]
	];

	if (bIsBoundSelector)
	{
		FMenuBuilder MenuBuilder = Helper_CreateBoundSelectorPickerWidget(InInputs);
		Inputs_VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];
	}
	{
		FMenuBuilder MenuBuilder = Helper_CreateWorldActorPickerWidget(InInputs);
		Inputs_VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];
	}
}

void
FHoudiniInputDetails::Helper_CreateGeometryInputObjectCollapsed(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const FPlatformTypes::int32& InObjectIdx,
	TSharedRef<SVerticalBox> InVerticalBox,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];
	
	if (!IsValidWeakPointer(MainInput))
		return;

	UHoudiniInputObject* HoudiniInputObject = MainInput->GetHoudiniInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);
	UObject* InputObject = HoudiniInputObject ? HoudiniInputObject->GetObject() : nullptr;

	TSharedPtr<FAssetThumbnail> ObjThumbnail = MakeShareable(new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

	// Create thumbnail for this static mesh.
	constexpr int32 ThumbnailSize = 30;

	TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(
		new FAssetThumbnail(InputObject, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

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
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed a Geometry Input Object"),
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

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	// Drop Target: Static/Skeletal Mesh
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	InVerticalBox->AddSlot()
	.Padding(0.0f)
	.AutoHeight()
	[
		SNew(SAssetDropTarget)
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
		.OnAssetsDropped_Lambda([InInputs, InObjectIdx, UpdateGeometryObjectAt](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
		{
			int32 CurrentObjectIdx = InObjectIdx;
				
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
	HorizontalBox->AddSlot()
	.Padding(0.0f)
	.AutoWidth()
	[
		SNew(SBox)
		.WidthOverride(ThumbnailSize)
		.HeightOverride(ThumbnailSize)
		[
			StaticMeshThumbnail->MakeThumbnailWidget()
		]
	];

	FText MeshNameText = FText::GetEmpty();
	if (InputObject)
	{
		MeshNameText = FText::FromString(InputObject->GetName());
	}

	HorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding(3, 0, 5, 0)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(MeshNameText)
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];
}

void
FHoudiniInputDetails::Helper_CreateGeometryInputObjectExpanded(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const FPlatformTypes::int32& InObjectIdx,
	TSharedRef<SVerticalBox> InVerticalBox,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return;

	UHoudiniInputObject* HoudiniInputObject = MainInput->GetHoudiniInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);
	UObject* InputObject = HoudiniInputObject ? HoudiniInputObject->GetObject() : nullptr;


	// Create thumbnail for this static mesh.
	constexpr int32 ThumbnailSize = 46;

	TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(
		new FAssetThumbnail(InputObject, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

	// Lambda for adding new geometry input objects
	auto UpdateGeometryObjectAt = [MainInput, &CategoryBuilder](const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputsToUpdate, const int32& AtIndex, UObject* InObject, const bool& bAutoInserMissingObjects)
	{
		if (!IsValidWeakPointer(MainInput))
			return;

		// TODO: Commented out to make reset button functional. Check
		// that removing this doesn't create undesired results.
		//if (!IsValid(InObject))
		//	return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed a Geometry Input Object"),
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

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
		}
	};

	TSharedPtr<SHorizontalBox> HorizontalBox;
	InVerticalBox->AddSlot()
	.Padding(0, 5, 0, 0)
	.AutoHeight()
	[
		SNew(SAssetDropTarget)
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
		.OnAssetsDropped_Lambda([InInputs, InObjectIdx, UpdateGeometryObjectAt](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
		{
			int32 CurrentObjectIdx = InObjectIdx;
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

		
	FText ThumbnailTip = FText::GetEmpty();
	if (InputObject)
	{
	    ThumbnailTip = FText::FromString(InputObject->GetDesc());
	}

	HorizontalBox->AddSlot()
	.Padding(0)
	.AutoWidth()
	[
		SNew(SBorder)
		.BorderImage(_GetEditorStyle().GetBrush(TEXT("AssetThumbnail.AssetBackground")))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnMouseDoubleClick_Lambda([MainInput, InObjectIdx](const FGeometry&, const FPointerEvent&)
		{
			UObject* InputObject = MainInput->GetInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);
			if (GEditor && InputObject)
				GEditor->EditObject(InputObject);

			return FReply::Handled();
		})
		[
			SNew(SBox)
			.WidthOverride(ThumbnailSize)
			.HeightOverride(ThumbnailSize)
			.ToolTipText(ThumbnailTip)
			[
				StaticMeshThumbnail->MakeThumbnailWidget()
			]
		]
	];

	FText MeshNameText = FText::GetEmpty();
	if (InputObject)
		MeshNameText = FText::FromString(InputObject->GetName());

	TSharedPtr<SVerticalBox> ComboAndButtonBox;
	HorizontalBox->AddSlot()
	.FillWidth(1)
	.Padding(4, 0, 5, 0)
	.VAlign(VAlign_Center)
	[
		SAssignNew(ComboAndButtonBox, SVerticalBox)
	];

	// Add Combo box : Static Mesh
	TSharedPtr<SComboButton> StaticMeshComboButton;
	ComboAndButtonBox->AddSlot()
	.FillHeight(1)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(StaticMeshComboButton, SComboButton)
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(_GetEditorStyle(), TEXT("PropertyEditor.AssetClass"))
				.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
				.ColorAndOpacity(_GetEditorStyle().GetColor(TEXT("AssetThumbnail"), ".ColorAndOpacity"))
				.Text(MeshNameText)
			]
		]
	];


	TWeakPtr<SComboButton> WeakStaticMeshComboButton(StaticMeshComboButton);
	StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
		[MainInput, InInputs, InObjectIdx, WeakStaticMeshComboButton, UpdateGeometryObjectAt]()
		{
			TArray<const UClass*> AllowedClasses = UHoudiniInput::GetAllowedClasses(EHoudiniInputType::Geometry);
			UObject* DefaultObj = MainInput->GetInputObjectAt(InObjectIdx);

			TArray<UFactory*> NewAssetFactories;
			return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
				FAssetData(DefaultObj),
				true,
				AllowedClasses,
				NewAssetFactories,
				FOnShouldFilterAsset(),
				FOnAssetSelected::CreateLambda(
					[InInputs, InObjectIdx, WeakStaticMeshComboButton, UpdateGeometryObjectAt](const FAssetData& AssetData)
					{
						TSharedPtr<SComboButton> ComboButton = WeakStaticMeshComboButton.Pin();
						if (ComboButton.IsValid())
						{
							ComboButton->SetIsOpen(false);
							UObject* Object = AssetData.GetAsset();
							UpdateGeometryObjectAt(InInputs, InObjectIdx, Object, false);
						}
					}
				),
				FSimpleDelegate::CreateLambda([]() {}));
		}));

	// Add buttons
	TSharedPtr<SHorizontalBox> ButtonHorizontalBox;
	ComboAndButtonBox->AddSlot()
	.FillHeight(1)
	.Padding(0)
	.VAlign(VAlign_Center)
	[
		SAssignNew(ButtonHorizontalBox, SHorizontalBox)
	];

	// Create tooltip.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Asset"), MeshNameText);
	FText StaticMeshTooltip = FText::Format(
		LOCTEXT("BrowseToSpecificAssetInContentBrowser",
			"Browse to '{Asset}' in the content browser."), Args);

	// Button : Use selected in content browser
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeUseSelectedButton(
			FSimpleDelegate::CreateLambda([InInputs, InObjectIdx, UpdateGeometryObjectAt]()
			{
				if (GEditor)
				{
					TArray<FAssetData> CBSelections;
					GEditor->GetContentBrowserSelections(CBSelections);

					TArray<const UClass*> AllowedClasses = UHoudiniInput::GetAllowedClasses(EHoudiniInputType::Geometry);
					int32 CurrentObjectIdx = InObjectIdx;
					for (auto& CurAssetData : CBSelections)
					{
						UObject* Object = CurAssetData.GetAsset();
						if (!IsValid(Object))
							continue;

						if (!UHoudiniInput::IsObjectAcceptable(EHoudiniInputType::Geometry, Object))
							continue;

						UpdateGeometryObjectAt(InInputs, CurrentObjectIdx++, Object, true);
					}
				}
			}),
			TAttribute<FText>(LOCTEXT("GeometryInputUseSelectedAssetFromCB", "Use the currently selected asset from the content browser.")))
	];

	// Button : Browse Static Mesh
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeBrowseButton(
			FSimpleDelegate::CreateLambda([MainInput, InObjectIdx]()
			{
				UObject* InputObject = MainInput->GetInputObjectAt(InObjectIdx);
				if (GEditor && InputObject)
				{
					TArray<UObject*> Objects;
					Objects.Add(InputObject);
					GEditor->SyncBrowserToObjects(Objects);
				}
			}),
			TAttribute<FText>(StaticMeshTooltip))
	];

	// ButtonBox: Reset
	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeResetButton(
			FSimpleDelegate::CreateLambda([UpdateGeometryObjectAt, InInputs, InObjectIdx]()
			{
				UpdateGeometryObjectAt(InInputs, InObjectIdx, nullptr, false);
			}),
			TAttribute<FText>(LOCTEXT("GeometryInputReset", "Reset this geometry input object.")))
	];

	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("TransformOffset", "Open transform offset menu below."))
		.ButtonStyle(_GetEditorStyle(), "NoBorder")
		.ContentPadding(0)
		.Visibility(EVisibility::Visible)
		.OnClicked_Lambda([&CategoryBuilder, MainInput, InInputs, InObjectIdx]()
		{
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Toggling Transform Offset Menu for Geometry Input Object"),
				MainInput->GetOuter());

			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->OnTransformUIExpand(InObjectIdx);
				CurInput->Modify();
			}

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();

			return FReply::Handled();
		})
		[
			SNew(SImage)
			.Image(_GetEditorStyle().GetBrush("Icons.Transform"))
			.ColorAndOpacity(FSlateColor(FColor(255, 255, 255, 168)))
		]
	];

	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeAddButton(
			FSimpleDelegate::CreateLambda([&CategoryBuilder, MainInput, InInputs, InObjectIdx]()
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Insert a Geometry Input Object"),
					MainInput->GetOuter());
				// Insert
				for (auto CurInput : InInputs)
				{
					if (!IsValidWeakPointer(CurInput))
						continue;

					CurInput->Modify();
					CurInput->InsertInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);
				}

				if (CategoryBuilder.IsParentLayoutValid())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
			}),
			TAttribute<FText>(LOCTEXT("GeometryInputAdd", "Add a new geometry input object above.")))
	];

	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("DuplicateGeometryInputObject", "Duplicate this geometry input object."))
		.ButtonStyle(_GetEditorStyle(), "NoBorder")
		.ContentPadding(0)
		.Visibility(EVisibility::Visible)
		.OnClicked_Lambda([&CategoryBuilder, MainInput, InInputs, InObjectIdx]()
		{
			if (!IsValidWeakPointer(MainInput))
				return FReply::Handled();

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Duplicate a Geometry Input Object"),
				MainInput->GetOuter());

			// Duplicate
			for (auto CurInput : InInputs)
			{
				if (!IsValidWeakPointer(CurInput))
					continue;

				CurInput->Modify();
				CurInput->DuplicateInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);
			}

			if (CategoryBuilder.IsParentLayoutValid())
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();

			return FReply::Handled();
		})
		[
			SNew(SImage)
			.Image(_GetEditorStyle().GetBrush("Icons.Duplicate"))
			.ColorAndOpacity(FSlateColor(FColor(255, 255, 255, 168)))
		]
	];

	ButtonHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(1, 0, 3, 0)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeDeleteButton(
			FSimpleDelegate::CreateLambda([&CategoryBuilder, MainInput, InInputs, InObjectIdx]()
			{
				if (!IsValidWeakPointer(MainInput))
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Delete a Geometry Input Object"),
					MainInput->GetOuter());

				// Delete
				for (auto CurInput : InInputs)
				{
					if (!IsValidWeakPointer(CurInput))
						continue;

					CurInput->Modify();
					CurInput->DeleteInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);

					if (GEditor)
						GEditor->RedrawAllViewports();
				}

				if (CategoryBuilder.IsParentLayoutValid())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
			}),
			TAttribute<FText>(LOCTEXT("GeometryInputDelete", "Delete this geometry input object.")))
	];

	
	TSharedPtr<SVerticalBox> TransformOffset_VerticalBox;
	
	InVerticalBox->AddSlot()
	.Padding(5, 0, 0, 0)
	.AutoHeight()
	[
		SAssignNew(TransformOffset_VerticalBox, SVerticalBox)
		.Visibility(MainInput->IsTransformUIExpanded(InObjectIdx) ? EVisibility::Visible : EVisibility::Collapsed)
	];

	TransformOffset_VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GeoInputTransform", "Transform Offset"))
			.ToolTipText(LOCTEXT("GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini."))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	// Lambda for changing the transform values
	auto ChangeTransformOffsetAt = [&](float Value, int32 AtIndex, int32 PosRotScaleIndex, int32 XYZIndex, bool DoChange, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Transform Offset"),
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

	for (auto& CurInput : InInputs)
	{
		if (!IsValidWeakPointer(CurInput))
			continue;

		FTransform* CurTransform = CurInput->GetTransformOffset(InObjectIdx);
		if (!CurTransform)
			continue;

		if (CurTransform->GetLocation() != FVector3d::ZeroVector)
			bResetButtonVisiblePosition = true;

		FRotator Rotator = CurTransform->Rotator();
		if (Rotator.Roll != 0 || Rotator.Pitch != 0 || Rotator.Yaw != 0)
			bResetButtonVisibleRotation = true;

		if (CurTransform->GetScale3D() != FVector3d::OneVector)
			bResetButtonVisibleScale = true;
	}

	auto ChangeTransformOffsetUniformlyAt = [InObjectIdx, InInputs, ChangeTransformOffsetAt](const float& Val, const int32& PosRotScaleIndex)
	{
		ChangeTransformOffsetAt(Val, InObjectIdx, PosRotScaleIndex, 0, true, InInputs);
		ChangeTransformOffsetAt(Val, InObjectIdx, PosRotScaleIndex, 1, true, InInputs);
		ChangeTransformOffsetAt(Val, InObjectIdx, PosRotScaleIndex, 2, true, InInputs);
	};

	TransformOffset_VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GeoInputTranslate", "T"))
			.ToolTipText(LOCTEXT("GeoInputTranslateTooltip", "Translate"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.AllowSpin(true)
			.X(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetPositionOffsetX, InObjectIdx)))
			.Y(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetPositionOffsetY, InObjectIdx)))
			.Z(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetPositionOffsetZ, InObjectIdx)))
			.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ ChangeTransformOffsetAt(Val, InObjectIdx, 0, 0, true, InInputs); })
			.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ ChangeTransformOffsetAt(Val, InObjectIdx, 0, 1, true, InInputs); })
			.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ ChangeTransformOffsetAt(Val, InObjectIdx, 0, 2, true, InInputs); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.Visibility(EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Lock"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
				.Visibility(bResetButtonVisiblePosition ? EVisibility::Visible : EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
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
	TransformOffset_VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GeoInputRotate", "R"))
			.ToolTipText(LOCTEXT("GeoInputRotateTooltip", "Rotate"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SRotatorInputBox)
			.AllowSpin(true)
			.bColorAxisLabels(true)
			.Roll(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetUserInputRoll, InObjectIdx)))
			.Pitch(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetUserInputPitch, InObjectIdx)))
			.Yaw(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetUserInputYaw, InObjectIdx)))
			.OnRollCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ ChangeTransformOffsetAt(Val, InObjectIdx, 1, 0, true, InInputs); })
			.OnPitchCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ ChangeTransformOffsetAt(Val, InObjectIdx, 1, 1, true, InInputs); })
			.OnYawCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
				{ ChangeTransformOffsetAt(Val, InObjectIdx, 1, 2, true, InInputs); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.Visibility(EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("Icons.Lock"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
				.Visibility(bResetButtonVisibleRotation ? EVisibility::Visible : EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
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

	bool bLocked = false;
	if (HoudiniInputObject)
		bLocked = HoudiniInputObject->IsUniformScaleLocked();

	TransformOffset_VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GeoInputScale", "S"))
			.ToolTipText(LOCTEXT("GeoInputScaleTooltip", "Scale"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.X(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetScaleOffsetX, InObjectIdx)))
			.Y(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetScaleOffsetY, InObjectIdx)))
			.Z(TAttribute<TOptional<float>>::Create(
				TAttribute<TOptional<float>>::FGetter::CreateUObject(
					MainInput.Get(), &UHoudiniInput::GetScaleOffsetZ, InObjectIdx)))
			.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
			{
				if (bLocked)
					ChangeTransformOffsetUniformlyAt(Val, 2);
				else
					ChangeTransformOffsetAt(Val, InObjectIdx, 2, 0, true, InInputs);
			})
			.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
			{
				if (bLocked)
					ChangeTransformOffsetUniformlyAt(Val, 2);
				else
					ChangeTransformOffsetAt(Val, InObjectIdx, 2, 1, true, InInputs);
			})
			.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
			{
				if (bLocked)
					ChangeTransformOffsetUniformlyAt(Val, 2);
				else
					ChangeTransformOffsetAt(Val, InObjectIdx, 2, 2, true, InInputs);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ToolTipText(HoudiniInputObject ?
					LOCTEXT("GeoInputLockButtonToolTip", "When locked, scales uniformly based on the current xyz scale values so the input object maintains its shape in each direction when scaled") :
					LOCTEXT("GeoInputLockButtonToolTipNoObject", "No input object selected"))
				.ClickMethod(EButtonClickMethod::MouseDown)
				.Visibility(EVisibility::Visible)
				[
					SNew(SImage)
					.Image(bLocked ? _GetEditorStyle().GetBrush("Icons.Lock") : _GetEditorStyle().GetBrush("Icons.Unlock"))
				]
				.OnClicked_Lambda([InInputs, MainInput, InObjectIdx, HoudiniInputObject, &CategoryBuilder]()
				{
					for (auto& CurInput : InInputs)
					{
						if (!IsValidWeakPointer(CurInput))
							continue;

						UHoudiniInputObject* CurInputObject = CurInput->GetHoudiniInputObjectAt(EHoudiniInputType::Geometry, InObjectIdx);
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
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
				.Visibility(bResetButtonVisibleScale ? EVisibility::Visible : EVisibility::Hidden)
				[
					SNew(SImage)
					.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
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

FReply
FHoudiniInputDetails::Helper_OnButtonClickSelectActors(
	IDetailCategoryBuilder& CategoryBuilder, 
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const FName& DetailsPanelName)
{
	return Helper_OnButtonClickSelectActors(CategoryBuilder, InInputs, DetailsPanelName, false, false);
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickUseSelectionAsBoundSelector(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const FName& DetailsPanelName)
{
	return Helper_OnButtonClickSelectActors(CategoryBuilder, InInputs, DetailsPanelName, true, false);
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickSelectActors(
	IDetailCategoryBuilder& CategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
	const FName& DetailsPanelName, 
	bool bUseWorldInAsWorldSelector,
	bool bForceUpdate)
{
	if (InInputs.Num() <= 0)
		return FReply::Handled();

	const TWeakObjectPtr<UHoudiniInput>& MainInput = InInputs[0];

	if (!IsValidWeakPointer(MainInput))
		return FReply::Handled();

	// There's no undo operation for button.
	FPropertyEditorModule& PropertyModule =
		FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

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
	auto* LocalDetailsView = static_cast<SLocalDetailsView*>(DetailsView.Get());

	// When forcing the update (refresh)
	// Restart the selection AND update it immediately.
	// This ensures that everything is properly taken care of
	bool bDoStartSelection = !DetailsView->IsLocked();
	bool bDoUpdateSelection = DetailsView->IsLocked();
	if (bForceUpdate)
	{
		bDoStartSelection = true;
		bDoUpdateSelection = true;
	}
		
	if (bDoStartSelection)
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
						Actor = InputHAC->GetHoudiniCookable() ? InputHAC->GetHoudiniCookable()->GetOwner() : nullptr;
					}
				}

				if (!IsValid(Actor))
					continue;

				GEditor->SelectActor(Actor, true, true);
			}
		}
	}
	
	if(bDoUpdateSelection)
	{
		//
		// UPDATE SELECTION
		//  Unlocks the input's selection and select the HDA back.
		//

		if (!GEditor || !GEditor->GetSelectedObjects())
			return FReply::Handled();

		USelection* SelectedActors = GEditor->GetSelectedActors();
		USelection* SelectedComponents = GEditor->GetSelectedComponents();
		if (!SelectedActors && !SelectedComponents)
			return FReply::Handled();

		TArray<AActor*> AllSelectedActors;

		// Add all actors from the actor selection
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			AActor* CurrentActor = Cast<AActor>(*It);
			if (!CurrentActor)
				continue;

			AllSelectedActors.Add(CurrentActor);
		}

		// Create a transaction
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Changed Houdini World Outliner Input Objects"),
			MainInput->GetOuter());

		TArray<UObject*> AllActors;
		for (auto CurrentInput : InInputs)
		{
			CurrentInput->Modify();

			// Get our parent component/actor
			UHoudiniCookable* OuterCookable = Cast<UHoudiniCookable>(CurrentInput->GetOuter());
			USceneComponent* ParentComponent = OuterCookable ? OuterCookable->GetComponent() : Cast<USceneComponent>(CurrentInput->GetOuter());
			AActor* ParentActor = OuterCookable ? OuterCookable->GetOwner() : nullptr;

			AllActors.Add(ParentActor);

			bool bHasChanged = true;
			if (bUseWorldInAsWorldSelector)
			{
				//
				// Update bound selectors
				
				// Clean up the selected actors
				TArray<AActor*> ValidBoundSelectedActors;
				for (auto& CurrentBoundActor : AllSelectedActors)
				{
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
				for (auto& CurrentActor : AllSelectedActors)
				{
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

			// This will allow a one time update of the inputs if auto-update is disabled
			if (!CurrentInput->GetWorldInputAutoUpdates())
				bForceUpdate = true;
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

	if (bForceUpdate)
	{
		for (auto CurrentInput : InInputs)
		{
			// Manually update the world input if auto update is disabled
			bool bPrevious = CurrentInput->GetWorldInputAutoUpdates();
			CurrentInput->SetWorldInputAutoUpdates(true);
			FHoudiniInputTranslator::UpdateWorldInput(CurrentInput.Get());
			CurrentInput->SetWorldInputAutoUpdates(bPrevious);
		}
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
		FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

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
		UHoudiniCookable* OuterCookable = Cast<UHoudiniCookable>(CurrentInput->GetOuter());
		USceneComponent* ParentComponent = OuterCookable ? OuterCookable->GetComponent() : Cast<USceneComponent>(CurrentInput->GetOuter());
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
