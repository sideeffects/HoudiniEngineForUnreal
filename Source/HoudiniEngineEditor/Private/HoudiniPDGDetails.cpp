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

#include "HoudiniPDGDetails.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniPDGAssetLink.h"
#include "HoudiniPDGManager.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "HoudiniCookable.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define HOUDINI_ENGINE_UI_SECTION_PDG_BAKE 2

#define HOUDINI_PDG_DETAILS_FONT TEXT("PropertyWindow.NormalFont")

namespace

{
bool 
IsTextAndTooltipLexicographicallyBefore(
	const TSharedPtr<FTextAndTooltip>& Left,
	const TSharedPtr<FTextAndTooltip>& Right)
{
	// If we don't trim the whitespace, then all strings with
	// padded spaces will end up at the start.
	return Left->Text.TrimStart() < Right->Text.TrimStart();
}

FString
FormatTOPNodeName(
	const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink,
	FString TOPNodeName)
{
	if (InPDGAssetLink->bUseTOPOutputFilter &&
		TOPNodeName.StartsWith(InPDGAssetLink->TOPOutputFilter))
	{
		TOPNodeName.Append(" (Output)");
	}

	return TOPNodeName;
}
}

void 
FHoudiniPDGDetails::CreateWidget(
	IDetailCategoryBuilder& HouPDGCategory,
	const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if (!IsValidWeakPointer(InHC))
		return;

	FHoudiniPDGDetails::AddPDGAssetWidget(HouPDGCategory, InHC);

	FHoudiniPDGDetails::AddTOPNetworkWidget(HouPDGCategory, InHC);

	if(!InHC->GetIsPCG())
	{
		FHoudiniPDGDetails::AddTOPNodeWidget(HouPDGCategory, InHC);
	}

	// Add bake widgets for PDG output
	CreatePDGBakeWidgets(HouPDGCategory, InHC);

	FHoudiniPDGDetails::AddAssetOptions(HouPDGCategory, InHC);
}


void
FHoudiniPDGDetails::AddPDGAssetWidget(
	IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	// PDG STATUS ROW
	AddPDGAssetStatus(InPDGCategory, InHC);

	// Commandlet Status row
	AddPDGCommandletStatus(InPDGCategory, FHoudiniEngine::Get().GetPDGCommandletStatus());

	// REFRESH / RESET Buttons
	{
		TSharedRef<SHorizontalBox> RefreshHBox = SNew(SHorizontalBox);
		TSharedPtr<SHorizontalBox> ResetHBox = SNew(SHorizontalBox);

		FDetailWidgetRow& PDGRefreshResetRow = InPDGCategory.AddCustomRow(FText::FromString("PDG Refresh"))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("LinkTooltip", "Links the PDG Asset."))
					.ContentPadding(FMargin(5.0f, 5.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InHC]()
					{
							if(!FHoudiniEngine::Get().GetSession())
								return false;

							if(!InHC.IsValid())
								return false;

							if(!IsValid(InHC->GetPDGAssetLink()))
								return false;

							if(InHC->GetPDGAssetLink()->LinkState == EPDGLinkState::Linking)
								return false;
							else
								return true;
					})
					.OnClicked_Lambda([InHC]()
					{
						if (InHC.IsValid())
							FHoudiniPDGDetails::RefreshPDGAssetLink(InHC->GetPDGAssetLink());
						return FReply::Handled();
					})
					.Content()
					[
						SAssignNew(RefreshHBox, SHorizontalBox)
					]
				]
			]
		];

		TSharedPtr<FSlateDynamicImageBrush> RefreshIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGRefreshIconBrush();
		if (RefreshIconBrush.IsValid())
		{
			TSharedPtr<SImage> RefreshImage;
			RefreshHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(RefreshImage, SImage)
					]
				];

			RefreshImage->SetImage(
				TAttribute<const FSlateBrush*>::Create(
					TAttribute<const FSlateBrush*>::FGetter::CreateLambda([RefreshIconBrush]() { return RefreshIconBrush.Get(); })));
		}

		RefreshHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Relink", "Relink"))
			];

		TSharedPtr<FSlateDynamicImageBrush> ResetIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGResetIconBrush();
		if (ResetIconBrush.IsValid())
		{
			TSharedPtr<SImage> ResetImage;
			ResetHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(ResetImage, SImage)
					]
				];

			ResetImage->SetImage(
				TAttribute<const FSlateBrush*>::Create(
					TAttribute<const FSlateBrush*>::FGetter::CreateLambda([ResetIconBrush]() { return ResetIconBrush.Get(); })));
		}

		ResetHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LinkToPDG", "Link to PDG"))
			];
	}

	// TODO: move this to a better place: the baking code is in HoudiniEngineEditor, the PDG manager (that knows about
	// when work object results are loaded is in HoudiniEngine and the PDGAssetLink is in HoudiniEngineRuntime). So
	// we bind an auto-bake helper function here. Maybe the baking code can move to HoudiniEngine?
	if (PDGAssetLink->AutoBakeDelegateHandle.IsValid())
		PDGAssetLink->OnWorkResultObjectLoaded.Remove(PDGAssetLink->AutoBakeDelegateHandle);
	PDGAssetLink->AutoBakeDelegateHandle = PDGAssetLink->OnWorkResultObjectLoaded.AddStatic(FHoudiniEngineBakeUtils::CheckPDGAutoBakeAfterResultObjectLoaded);

}

void FHoudiniPDGDetails::AddAssetOptions(IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	FString GroupLabel = TEXT("Asset Options");
	FString GroupName = FString::Printf(TEXT("%s %s"), *GroupLabel, *InHC.Get()->GetCookableGUID().ToString());
	IDetailGroup& TOPNodesGrp = InPDGCategory.AddGroup(FName(*GroupName), FText::FromString(GroupLabel), false);

	// Checkbox: Autocook
	if(!InHC->GetIsPCG())
	{
		FText Tooltip = FText::FromString(TEXT("When enabled, the selected TOP Network's output will automatically cook after succesfully cooking the PDG Asset Link HDA."));
		FDetailWidgetRow& PDGAutocookRow = TOPNodesGrp.AddWidgetRow();
		// Disable if PDG is not linked
		BindEnablePDGWiddgetsTest(PDGAutocookRow, InHC);
		PDGAutocookRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("Auto Cook")))
					.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
					.ToolTipText(Tooltip)
			];

		TSharedPtr<SCheckBox> AutoCookCheckBox;
		PDGAutocookRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SAssignNew(AutoCookCheckBox, SCheckBox)
					.IsChecked_Lambda([PDGAssetLink]()
						{
							return PDGAssetLink->bAutoCook ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
					.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
						{
							const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
							if(!IsValidWeakPointer(PDGAssetLink) || PDGAssetLink->bAutoCook == bNewState)
								return;

							// Record a transaction for undo/redo
							FScopedTransaction Transaction(
								TEXT(HOUDINI_MODULE_RUNTIME),
								LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
								PDGAssetLink.Get());

							PDGAssetLink->Modify();
							PDGAssetLink->bAutoCook = bNewState;
							FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
								GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bAutoCook), PDGAssetLink.Get());
						})
					.ToolTipText(Tooltip)
			];
	}


	// Output parent actor selector
	if(!InHC->GetIsPCG())
	{
		FText Tooltip = FText::FromString(
			TEXT("The PDG Output Actors will be created under this parent actor. If not set, then the PDG Output Actors will be created under a new folder."));

		FDetailWidgetRow& PDGParentActorRow = TOPNodesGrp.AddWidgetRow();
		// Disable if PDG is not linked
		BindEnablePDGWiddgetsTest(PDGParentActorRow, InHC);
		PDGParentActorRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("Output Parent Actor")))
					.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
					.ToolTipText(Tooltip)
			];

		TSharedPtr<SCheckBox> AutoCookCheckBox;
		PDGParentActorRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SNew(SObjectPropertyEntryBox)
					.AllowedClass(AActor::StaticClass())
					.ObjectPath_Lambda([PDGAssetLink]()
						{
							FString Result;
							if(PDGAssetLink.IsValid())
							{
								Result = PDGAssetLink.Get()->OutputParentActor->GetPathName();
							}
							return Result;
						})
					.OnObjectChanged_Lambda([PDGAssetLink](const FAssetData& InAssetData)
						{
							if(PDGAssetLink.IsValid())
							{
								UObject* Obj = InAssetData.GetAsset();
								if(AActor* Actor = Cast<AActor>(Obj))
								{
									PDGAssetLink.Get()->OutputParentActor = Actor;
								}
								else
								{
									PDGAssetLink.Get()->OutputParentActor = nullptr;
								}
							}
						})
					.ToolTipText(Tooltip)
			];
	}

	// Checkbox: Load Work Item Output Files
	if(!InHC->GetIsPCG())
	{
		auto ToolTipLambda = [PDGAssetLink]()
			{
				return FText::FromString(TEXT("When enabled, Output files produced by the node or network will automatically be loaded when cooked."));
			};
		FDetailWidgetRow& PDGNodeAutoLoadRow = TOPNodesGrp.AddWidgetRow();
		BindEnablePDGWiddgetsTest(PDGNodeAutoLoadRow, InHC);
		PDGNodeAutoLoadRow.IsEnabledAttr.Bind(TAttribute<bool>::FGetter::CreateLambda([PDGAssetLink]()
			{
				if(!IsPDGLinked(PDGAssetLink))
					return false;
				UTOPNode* const Node = PDGAssetLink->GetSelectedTOPNode();
				if(IsValid(Node) && !Node->bHidden && !Node->bHasChildNodes)
					return true;
				return false;
			}));

		PDGNodeAutoLoadRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("Auto-Import Work Item Output Files")))
					.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
					.ToolTipText_Lambda(ToolTipLambda)
			];

		TSharedPtr<SCheckBox> AutoLoadCheckBox;

		PDGNodeAutoLoadRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SAssignNew(AutoLoadCheckBox, SCheckBox)
					.IsChecked_Lambda([PDGAssetLink]()
						{
							if (!IsValidWeakPointer(PDGAssetLink))
								return ECheckBoxState::Unchecked;

							return PDGAssetLink->GetSelectedTOPNode()
								? (PDGAssetLink->GetSelectedTOPNode()->bAutoLoad ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								: ECheckBoxState::Unchecked;
						})
					.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
						{
							if (!IsValidWeakPointer(PDGAssetLink))
								return;

							const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
							UTOPNode* TOPNode = PDGAssetLink->GetSelectedTOPNode();
							if(!IsValid(TOPNode) || TOPNode->bAutoLoad == bNewState)
								return;

							// Record a transaction for undo/redo
							FScopedTransaction Transaction(
								TEXT(HOUDINI_MODULE_RUNTIME),
								LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
								TOPNode);

							TOPNode->Modify();
							TOPNode->bAutoLoad = bNewState;
							FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
								GET_MEMBER_NAME_STRING_CHECKED(UTOPNode, bAutoLoad), TOPNode);

							for (UTOPNetwork* TOPNetwork : PDGAssetLink->AllTOPNetworks)
							{
								TOPNetwork->bAutoLoadResults = bNewState;
								for(auto CurrentTOPNode : TOPNetwork->AllTOPNodes)
								{
									if(IsValid(CurrentTOPNode) && CurrentTOPNode->bAutoLoad != bNewState)
									{
										CurrentTOPNode->bAutoLoad = bNewState;
									}
								}
							}

						})
					.ToolTipText_Lambda(ToolTipLambda)
			];


	}

	// Checkbox: Work Item Output Files Visible
	{
		auto ToolTipLambda = [PDGAssetLink]()
			{
				bool bDisabled = false;
				if(IsValidWeakPointer(PDGAssetLink) && PDGAssetLink->GetSelectedTOPNode())
				{
					bDisabled = PDGAssetLink->GetSelectedTOPNode()->bHasChildNodes;
				}

				return bDisabled
					? FText::FromString(TEXT("This node has child nodes, visibility of work item outputs must be set on the child nodes individually."))
					: FText::FromString(TEXT("Toggles the visibility of the actors created from this TOP Node's Work Item File Outputs."));
			};

		FDetailWidgetRow& PDGNodeShowResultRow = TOPNodesGrp.AddWidgetRow();
		PDGNodeShowResultRow.IsEnabledAttr.Bind(TAttribute<bool>::FGetter::CreateLambda([PDGAssetLink]()
			{
				if(!IsValidWeakPointer(PDGAssetLink))
					return false;

				if(!IsPDGLinked(PDGAssetLink))
					return false;

				UTOPNode* const Node = PDGAssetLink->GetSelectedTOPNode();
				if(IsValid(Node) && !Node->bHidden && !Node->bHasChildNodes)
					return true;

				return false;
			}));
		PDGNodeShowResultRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("Work Item Output Files Visible")))
					.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
					.ToolTipText_Lambda(ToolTipLambda)
			];

		TSharedPtr<SCheckBox> ShowResCheckBox;
		PDGNodeShowResultRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SAssignNew(ShowResCheckBox, SCheckBox)
					.IsChecked_Lambda([PDGAssetLink]()
						{
							if (!IsValidWeakPointer(PDGAssetLink))
								return ECheckBoxState::Unchecked;

							return PDGAssetLink->GetSelectedTOPNode()
								? (PDGAssetLink->GetSelectedTOPNode()->IsVisibleInLevel() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								: ECheckBoxState::Unchecked;
						})
					.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
						{
							if (!IsValidWeakPointer(PDGAssetLink))
								return;

							const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
							UTOPNode* const TOPNode = PDGAssetLink->GetSelectedTOPNode();
							if(!IsValid(TOPNode) || TOPNode->IsVisibleInLevel() == bNewState)
								return;

							// Record a transaction for undo/redo
							FScopedTransaction Transaction(
								TEXT(HOUDINI_MODULE_RUNTIME),
								LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
								TOPNode);

							TOPNode->Modify();
							TOPNode->SetVisibleInLevel(bNewState);
							FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(TEXT("bShow"), TOPNode);
							// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						})
					.ToolTipText_Lambda(ToolTipLambda)
			];
	}

}

void FHoudiniPDGDetails::AddTOPNodeFilter(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	FText Tooltip = FText::FromString(TEXT("When enabled, the TOP Node Filter will only display the TOP Nodes found in the current network that start with the filter prefix. Disabling the Filter will display all of the TOP Network's TOP Nodes."));
	// Lambda for changing the filter value
	auto ChangeTOPNodeFilter = [PDGAssetLink](const FString& NewValue)
		{
			if(!IsValidWeakPointer(PDGAssetLink))
				return;

			if(PDGAssetLink->TOPNodeFilter.Equals(NewValue))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				PDGAssetLink.Get());

			PDGAssetLink->Modify();
			PDGAssetLink->TOPNodeFilter = NewValue;
			// Notify that we have changed the property
			FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
				GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPNodeFilter), PDGAssetLink.Get());
		};

	FDetailWidgetRow& PDGFilterRow = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGFilterRow, InHC);

	PDGFilterRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			// Checkbox enable filter
			SNew(SCheckBox)
				.IsChecked_Lambda([PDGAssetLink]()
					{
						return PDGAssetLink->bUseTOPNodeFilter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;;
					})
				.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
					{
						if(!IsValidWeakPointer(PDGAssetLink))
							return;

						const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
						if(PDGAssetLink->bUseTOPNodeFilter == bNewState)
							return;

						// Record a transaction for undo/redo
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_RUNTIME),
							LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
							PDGAssetLink.Get());

						PDGAssetLink->Modify();
						PDGAssetLink->bUseTOPNodeFilter = bNewState;
						// Notify that we have changed the property
						FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
							GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPNodeFilter), PDGAssetLink.Get());
					})
				.ToolTipText(Tooltip)
		]
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 3.0f)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node Filter")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
				.ToolTipText(Tooltip)
				.IsEnabled_Lambda([PDGAssetLink]()
					{
						if(IsValidWeakPointer(PDGAssetLink))
							return PDGAssetLink->bUseTOPNodeFilter;
						else
							return false;
					})
		];

	PDGFilterRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		.IsEnabled_Lambda([PDGAssetLink]()
			{
				if(IsValidWeakPointer(PDGAssetLink))
					return PDGAssetLink->bUseTOPNodeFilter;
				else
					return false;
			})
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ToolTipText(Tooltip)
				.Text_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink))
							return FText();
						return FText::FromString(PDGAssetLink->TOPNodeFilter);
					})
				.OnTextCommitted_Lambda([ChangeTOPNodeFilter](const FText& Val, ETextCommit::Type TextCommitType)
					{
						ChangeTOPNodeFilter(Val.ToString());
					})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ContentPadding(0)
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([=]()
					{
						FString DefaultFilter = TEXT(HAPI_UNREAL_PDG_DEFAULT_TOP_FILTER);
						ChangeTOPNodeFilter(DefaultFilter);
						return FReply::Handled();
					})
				[
					SNew(SImage)
						.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
				]
		];
}


void FHoudiniPDGDetails::AddTOPOutputFilter(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	// Lambda for changing the filter value
	FText Tooltip = FText::FromString(TEXT("When enabled, the Work Item Output Files created for the TOP Nodes found in the current network that start with the filter prefix will be automatically loaded int the world after being cooked."));
	auto ChangeTOPOutputFilter = [PDGAssetLink](const FString& NewValue)
		{
			if(!IsValidWeakPointer(PDGAssetLink))
				return;

			if(PDGAssetLink->TOPOutputFilter.Equals(NewValue))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				PDGAssetLink.Get());

			PDGAssetLink->Modify();
			PDGAssetLink->TOPOutputFilter = NewValue;
			// Notify that we have changed the property
			FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
				GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPOutputFilter), PDGAssetLink.Get());
		};


	FDetailWidgetRow& PDGOutputFilterEnablecRow = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGOutputFilterEnablecRow, InHC);

	PDGOutputFilterEnablecRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			// Checkbox enable filter
			SNew(SCheckBox)
				.IsChecked_Lambda([PDGAssetLink]()
					{
						return PDGAssetLink->bUseTOPOutputFilter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
				.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
					{
						if(!IsValidWeakPointer(PDGAssetLink))
							return;

						const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
						if(PDGAssetLink->bUseTOPOutputFilter == bNewState)
							return;

						// Record a transaction for undo/redo
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_RUNTIME),
							LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
							PDGAssetLink.Get());

						PDGAssetLink->Modify();
						PDGAssetLink->bUseTOPOutputFilter = bNewState;
						// Notify that we have changed the property
						FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
							GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPOutputFilter), PDGAssetLink.Get());
					})
				.ToolTipText(Tooltip)
		]
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 3.0f)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Output Filter")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
				.IsEnabled_Lambda([PDGAssetLink]()
					{
						if(IsValidWeakPointer(PDGAssetLink))
							return PDGAssetLink->bUseTOPOutputFilter;
						else
							return false;
					})
				.ToolTipText(Tooltip)
		];

	PDGOutputFilterEnablecRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
			.IsEnabled_Lambda([PDGAssetLink]()
				{
					if(IsValidWeakPointer(PDGAssetLink))
						return PDGAssetLink->bUseTOPOutputFilter;
					else
						return false;
				})
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink))
							return FText();
						return FText::FromString(PDGAssetLink->TOPOutputFilter);
					})
				.OnTextCommitted_Lambda([ChangeTOPOutputFilter](const FText& Val, ETextCommit::Type TextCommitType)
					{
						ChangeTOPOutputFilter(Val.ToString());
					})
				.ToolTipText(Tooltip)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
				.ContentPadding(0)
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([ChangeTOPOutputFilter]()
					{
						FString DefaultFilter = TEXT(HAPI_UNREAL_PDG_DEFAULT_TOP_OUTPUT_FILTER);
						ChangeTOPOutputFilter(DefaultFilter);
						return FReply::Handled();
					})
				[
					SNew(SImage)
						.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
				]
		];
}

bool
FHoudiniPDGDetails::GetPDGStatusAndColor(
	const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, FString& OutPDGStatusString, FLinearColor& OutPDGStatusColor)
{
	OutPDGStatusString = FString();
	OutPDGStatusColor = FLinearColor::White;

	if (FHoudiniEngine::Get().GetSession() == nullptr)
	{
		OutPDGStatusString = TEXT("Start a Houdini Session before linking PDG");
		OutPDGStatusColor = FLinearColor::White;
		return true;
	}
	if (!IsValidWeakPointer(InPDGAssetLink))
		return false;
	
	switch (InPDGAssetLink->LinkState)
	{
	case EPDGLinkState::Linked:
		OutPDGStatusString = TEXT("PDG is READY");
		OutPDGStatusColor = FLinearColor::Green;
		break;
	case EPDGLinkState::Linking:
		OutPDGStatusString = TEXT("PDG is Linking");
		OutPDGStatusColor = FLinearColor::Yellow;
		break;
	case EPDGLinkState::Error_Not_Linked:
		OutPDGStatusString = TEXT("PDG is ERRORED");
		OutPDGStatusColor = FLinearColor::Red;
		break;
	case EPDGLinkState::Inactive:
		OutPDGStatusString = TEXT("PDG is INACTIVE");
		OutPDGStatusColor = FLinearColor::White;
		break;
	default:
		return false;
	}

	return true;
}

void
FHoudiniPDGDetails::AddPDGAssetStatus(
	IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	FDetailWidgetRow& PDGStatusRow = InPDGCategory.AddCustomRow(FText::FromString("PDG Asset Status"))
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
			.Text_Lambda([InHC]()
			{
				if (!IsValidWeakPointer(InHC))
					return FText();

				FString PDGStatusString;
				FLinearColor PDGStatusColor;
				GetPDGStatusAndColor(InHC->GetPDGAssetLink(), PDGStatusString, PDGStatusColor);
				return FText::FromString(PDGStatusString);
			})
			.ColorAndOpacity_Lambda([InHC]()
			{
				if (!IsValidWeakPointer(InHC))
					return FSlateColor(FLinearColor::Transparent);

				FString PDGStatusString;
				FLinearColor PDGStatusColor;
				GetPDGStatusAndColor(InHC->GetPDGAssetLink(), PDGStatusString, PDGStatusColor);
				return FSlateColor(PDGStatusColor);
			})
		]
	];
}

void
FHoudiniPDGDetails::GetPDGCommandletStatus(FString& OutStatusString, FLinearColor& OutStatusColor)
{
	OutStatusString = FString();
	OutStatusColor = FLinearColor::White;

	if (!FHoudiniEngineCommands::IsPDGCommandletEnabled())
	{
		OutStatusString = TEXT("Async Importer Disabled");
		OutStatusColor = FLinearColor(0.5, 0.5, 0.5f, 1.0f);
		return;

	}
	switch (FHoudiniEngine::Get().GetPDGCommandletStatus())
	{
	case EHoudiniBGEOCommandletStatus::Connected:
		OutStatusString = TEXT("Async Importer is CONNECTED");
		OutStatusColor = FLinearColor::Green;
		break;
	case EHoudiniBGEOCommandletStatus::Running:
		OutStatusString = TEXT("Async Importer is Running, Connecting...");
		OutStatusColor = FLinearColor::Yellow;
		break;
	case EHoudiniBGEOCommandletStatus::Crashed:
		OutStatusString = TEXT("Async Importer has CRASHED");
		OutStatusColor = FLinearColor::Red;
		break;
	case EHoudiniBGEOCommandletStatus::NotStarted:
		OutStatusString = TEXT("Async Importer is NOT STARTED");
		OutStatusColor = FLinearColor::White;
		break;
	}
}

void
FHoudiniPDGDetails::AddPDGCommandletStatus(
	IDetailCategoryBuilder& InPDGCategory, const EHoudiniBGEOCommandletStatus& InCommandletStatus)
{
	FDetailWidgetRow& PDGStatusRow = InPDGCategory.AddCustomRow(FText::FromString("PDG Commandlet Status"))
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
            .Visibility_Lambda([]()
            {
            	const UHoudiniRuntimeSettings* Settings = GetDefault<UHoudiniRuntimeSettings>();
            	if (IsValid(Settings))
            	{
            	//	return FHoudiniEngineCommands::IsPDGCommandletEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
            	}
            	
	            return EVisibility::Visible;
            })
            .Text_Lambda([]()
            {
            	FString StatusString;
            	FLinearColor StatusColor;
            	GetPDGCommandletStatus(StatusString, StatusColor);
	            return FText::FromString(StatusString);
            })
            .ColorAndOpacity_Lambda([]()
            {
            	FString StatusString;
            	FLinearColor StatusColor;
            	GetPDGCommandletStatus(StatusString, StatusColor);
            	return FSlateColor(StatusColor);
            })
        ]
    ];
}

bool
FHoudiniPDGDetails::GetWorkItemTallyValueAndColor(
	const TWeakObjectPtr<UHoudiniPDGAssetLink>& InAssetLink,
	bool bInForSelectedNode,
	EWorkItemTallyType InTallyType,
	int32& OutValue,
	FLinearColor& OutColor)
{
	OutValue = 0;
	OutColor = FLinearColor::White;
	
	if (!IsValidWeakPointer(InAssetLink))
		return false;

	bool bFound = false;
	EPDGNodeState State = EPDGNodeState::None;

	UTOPNetwork* TOPNetwork = InAssetLink->GetSelectedTOPNetwork();

	const FWorkItemTallyBase* TallyPtr = nullptr;
	if (bInForSelectedNode)
	{
		UTOPNode* const TOPNode = InAssetLink->GetSelectedTOPNode();
		if(TOPNode && !TOPNode->bHidden)
		{
			TallyPtr = &(TOPNode->GetWorkItemTally());
			State = TOPNode->NodeState;
		}
	}
	else
	{
		TallyPtr = &(InAssetLink->WorkItemTally);

		if (TOPNetwork)
		{
			State = TOPNetwork->NetworkState;
		}
	}

	if(!TallyPtr)
		return false;

	auto OverrideColor = [State](const FLinearColor Color)
		{
			FLinearColor Cyan = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

			// Override color based off certain states.
			switch(State)
			{
			case EPDGNodeState::Cook_Failed:
				return FLinearColor::Red;
			case EPDGNodeState::Cooking:
				return Cyan;
			default:
				return Color;
			}
		};

	switch (InTallyType)
	{
	case EWorkItemTallyType::Waiting:
		if(bInForSelectedNode)
		{
			OutValue = TallyPtr->NumWaitingWorkItems() + TallyPtr->NumScheduledWorkItems();
		}
		else
		{
			OutValue = 0;
			OutValue = 0;
			for(UTOPNode* Node : TOPNetwork->AllTOPNodes)
			{
				if(Node->bAutoLoad)
				{
					OutValue += Node->GetWorkItemTally().NumWaitingWorkItems() + Node->GetWorkItemTally().NumScheduledWorkItems();
				}
			}
		}
		OutColor = OutValue > 0 ? FLinearColor(0.0f, 1.0f, 1.0f) : FLinearColor::White;
		bFound = true;
		break;
	case EWorkItemTallyType::Cooking:
		if(bInForSelectedNode)
		{
			OutValue = TallyPtr->NumCookingWorkItems();
		}
		else
		{
			OutValue = 0;
			for(UTOPNode* Node : TOPNetwork->AllTOPNodes)
			{
				if(Node->bAutoLoad && Node->NodeState == EPDGNodeState::Cooking)
				{
					OutValue += Node->GetWorkItemTally().NumCookingWorkItems();
				}
			}
		}
		OutColor = OutValue > 0 ? FLinearColor::Yellow : FLinearColor::White;
		bFound = true;
		break;
	case EWorkItemTallyType::Cooked:
		if(bInForSelectedNode)
		{
			OutValue = TallyPtr->NumCookedWorkItems();
		}
		else
		{
			OutValue = 0;
			for(UTOPNode* Node : TOPNetwork->AllTOPNodes)
			{
				if(Node->bAutoLoad && Node->NodeState == EPDGNodeState::Cook_Complete)
				{
					OutValue += Node->GetWorkItemTally().NumCookedWorkItems();
				}
			}
		}
		OutColor = OutValue > 0 ? FLinearColor::Green : FLinearColor::White;
		bFound = true;
		break;
	case EWorkItemTallyType::Imported:
		if(bInForSelectedNode)
		{
			OutValue = TallyPtr->NumLoadedWorkItems() + TallyPtr->NumEmptyWorkItems() + TallyPtr->NumIgnoredWorkItems();
		}
		else
		{
			OutValue = 0;
			for(UTOPNode* Node : TOPNetwork->AllTOPNodes)
			{
				if(Node->bAutoLoad)
				{
					OutValue += Node->GetWorkItemTally().NumLoadedWorkItems();
				}
			}
		}
		OutColor = OutValue > 0 ? FLinearColor::Green : FLinearColor::White;
		bFound = true;
		break;
	case EWorkItemTallyType::Failed:
		OutValue = TallyPtr->NumErroredWorkItems();
		OutColor = OutValue > 0 ? FLinearColor::Red : FLinearColor::White;
		bFound = true;
		break;
	}

	OutColor = OverrideColor(OutColor);

	return bFound;
}

void
FHoudiniPDGDetails::AddWorkItemStatusWidget(
	FDetailWidgetRow& InRow, const FString& InTitleString, const TWeakObjectPtr<UHoudiniCookable>& InHC, bool bInForSelectedNode)
{
	TWeakObjectPtr<UHoudiniPDGAssetLink> AssetLink = InHC->GetPDGAssetLink();

	auto AddGridBox = [AssetLink, bInForSelectedNode](EWorkItemTallyType TallyType) -> SHorizontalBox::FSlot::FSlotArguments
	{
		SHorizontalBox::FSlot::FSlotArguments Slot = SHorizontalBox::Slot();
		
		Slot
		.MaxWidth(500.0f)
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(FMargin(1.0f, 2.0f))
			[
				SNew(SBorder)
				.IsEnabled_Lambda([AssetLink]() { return IsPDGLinked(AssetLink); })
				.BorderImage(_GetEditorStyle().GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				.Padding(FMargin(1.0f, 5.0f))
				[
					SNew(SBox)
					.WidthOverride(95.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
							.Text_Lambda([TallyType, AssetLink, bInForSelectedNode]()
								{
									FString Title;
									switch(TallyType)
									{
									case EWorkItemTallyType::Waiting:
										Title = TEXT("WAITING");
										break;
									case EWorkItemTallyType::Cooking:
										Title = TEXT("COOKING");
										break;
									case EWorkItemTallyType::Cooked:
										{
											Title = TEXT("COOKED");
										}
										break;
									case EWorkItemTallyType::Imported:
										Title = TEXT("IMPORTED");
										break;
									case EWorkItemTallyType::Failed:
										Title = TEXT("FAILED");
										break;
									}
									return FText::FromString(Title);
								})
						.ColorAndOpacity_Lambda([AssetLink, bInForSelectedNode, TallyType]()
						{
							int32 Value;
							FLinearColor Color;
							GetWorkItemTallyValueAndColor(AssetLink, bInForSelectedNode, TallyType, Value, Color);
							return FSlateColor(Color);
						})
					]
				]
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(FMargin(1.0f, 2.0f))
			[
				SNew(SBorder)
				.IsEnabled_Lambda([AssetLink]() { return IsPDGLinked(AssetLink); })
				.BorderImage(_GetEditorStyle().GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)))
				.Padding(FMargin(1.0f, 5.0f))
				[
					SNew(SBox)
					.WidthOverride(95.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([AssetLink, bInForSelectedNode, TallyType]()
						{
							int32 Value;
							FLinearColor Color;
							GetWorkItemTallyValueAndColor(AssetLink, bInForSelectedNode, TallyType, Value, Color);
							return FText::AsNumber(Value);
						})
						.ColorAndOpacity_Lambda([AssetLink, bInForSelectedNode, TallyType]()
						{
							int32 Value;
							FLinearColor Color;
							GetWorkItemTallyValueAndColor(AssetLink, bInForSelectedNode, TallyType, Value, Color);
							return FSlateColor(Color);
						})
					]
				]
			]
		];

		return Slot;
	};
	
	InRow.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 0.0f)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(STextBlock)
				.IsEnabled_Lambda([AssetLink]() { return IsPDGLinked(AssetLink); })
				.Text(FText::FromString(InTitleString))
				.ToolTipText_Lambda([AssetLink, bInForSelectedNode]()
				{
					if(!AssetLink.IsValid())
						return FText::FromString(FString("Asset Link not active."));

					if (bInForSelectedNode)
					{
						if (!AssetLink->GetSelectedTOPNode())
							return FText::FromString(FString("No active TOP Node."));
						
						FString String = FString::Printf(TEXT("Output Node: %s"), *AssetLink->GetSelectedTOPNode()->NodeName);
						return FText::FromString(String);
					}
					else
					{
						UTOPNetwork* Network = AssetLink->GetSelectedTOPNetwork();

						if(!AssetLink->GetSelectedTOPNetwork())
							return FText::FromString(FString("No active TOP Network."));

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
						TStringBuilder<256> Builder;
#else
						FStringBuilderBase Builder;
#endif
						Builder.Append(TEXT("Output Nodes: "));
						int OutputCount = 0;
						int CookedCount = 0;
						for(UTOPNode* Node : Network->AllTOPNodes)
						{
							if(Node->bAutoLoad)
							{
								++OutputCount;
								if(OutputCount > 1)
									Builder.Append(", ");
								Builder.Append(Node->NodeName);
							}

							if(Node->NodeState == EPDGNodeState::Cook_Complete)
								++CookedCount;
						}
						if(OutputCount == 0)
							Builder.Append(TEXT("<none>"));
						Builder.Append(TEXT("\n"));

						Builder.Append(FString::Printf(TEXT("Cooked %d of %d Total TOP Nodes"), CookedCount, Network->AllTOPNodes.Num()));
						return  FText::FromString(Builder.ToString());
					}

				})
				
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ AddGridBox(EWorkItemTallyType::Waiting) 
				+ AddGridBox(EWorkItemTallyType::Cooking) 
				+ AddGridBox(EWorkItemTallyType::Cooked)
				+ AddGridBox(EWorkItemTallyType::Imported)
				+ AddGridBox(EWorkItemTallyType::Failed) 
			]
			+ SVerticalBox::Slot()
			[
				SNew(SSpacer)
			]
		]
	];
}


void
FHoudiniPDGDetails::AddTOPNetworkWidget(
	IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	if (!PDGAssetLink->GetSelectedTOPNetwork())
		return;

	if (PDGAssetLink->AllTOPNetworks.Num() <= 0)
		return;

	TOPNetworksPtr.Reset();

	FString GroupLabel = TEXT("TOP Networks");
	IDetailGroup& TOPNetWorkGrp = InPDGCategory.AddGroup(FName(*GroupLabel), FText::FromString(GroupLabel), false, false);

	AddTOPOutputFilter(TOPNetWorkGrp, InHC);

	AddTOPNetworkSelectWidgets(TOPNetWorkGrp, InHC);

	if (!InHC->GetIsPCG())
	{
		AddTOPNetworkDirtyAllAndCookOutputWidgets(TOPNetWorkGrp, InHC);
	}

	if (!InHC->GetIsPCG())
	{
		AddTOPNetworkPauseOrCancelWidgets(TOPNetWorkGrp, InHC);
	}

	if(!InHC->GetIsPCG())
	{
		AddTOPNetworkUnloadWorkItemsObjectsWidgets(TOPNetWorkGrp, InHC);
	}

	if(!InHC->GetIsPCG())
	{
		AddTOPNetworkCombinedState(TOPNetWorkGrp, InHC);

		// Enable this if you want to see un-combined PDG and loading state, useful for debugging.
		//AddTOPNetworkStates(TOPNetWorkGrp, InHC);
	}

	if(!InHC->GetIsPCG())
	{
		FDetailWidgetRow& PDGStatusRow = TOPNetWorkGrp.AddWidgetRow();
		// Disable if PDG is not linked
		BindEnablePDGWiddgetsTest(PDGStatusRow, InHC);
		FHoudiniPDGDetails::AddWorkItemStatusWidget(PDGStatusRow, TEXT("TOP Network Output Work Item Status"), InHC, false);
	}
}

void FHoudiniPDGDetails::AddTOPNetworkSelectWidgets(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	FDetailWidgetRow& PDGTOPNetRow = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGTOPNetRow, InHC);
	PDGTOPNetRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Network")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	// Fill the TOP Networks SharedString array
	TOPNetworksPtr.SetNum(PDGAssetLink->AllTOPNetworks.Num());
	for(int32 Idx = 0; Idx < PDGAssetLink->AllTOPNetworks.Num(); Idx++)
	{
		const UTOPNetwork* Network = PDGAssetLink->AllTOPNetworks[Idx];
		if(!IsValid(Network))
		{
			TOPNetworksPtr[Idx] = MakeShareable(new FTextAndTooltip(
				Idx,
				TEXT("Invalid"),
				TEXT("Invalid")
			));
		}
		else
		{
			TOPNetworksPtr[Idx] = MakeShareable(new FTextAndTooltip(
				Idx,
				FHoudiniEngineEditorUtils::GetNodeNamePaddedByPathDepth(Network->NodeName, Network->NodePath),
				Network->NodePath
			));
		}
	}

	Algo::Sort(TOPNetworksPtr, &IsTextAndTooltipLexicographicallyBefore);

	if(TOPNetworksPtr.Num() <= 0)
		TOPNetworksPtr.Add(MakeShareable(new FTextAndTooltip(INDEX_NONE, "----")));

	// Lambda for selecting another TOPNet
	auto OnTOPNetChanged = [PDGAssetLink](TSharedPtr<FTextAndTooltip> InNewChoice)
		{
			if(!InNewChoice.IsValid() || !IsValidWeakPointer(PDGAssetLink))
				return;

			const int32 NewChoice = InNewChoice->Value;
			int32 NewSelectedIndex = -1;
			if(PDGAssetLink->AllTOPNetworks.IsValidIndex(NewChoice))
				NewSelectedIndex = NewChoice;

			if(PDGAssetLink->SelectedTOPNetworkIndex == NewSelectedIndex)
				return;

			if(NewSelectedIndex < 0)
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				PDGAssetLink.Get());

			PDGAssetLink->Modify();
			PDGAssetLink->SelectedTOPNetworkIndex = NewSelectedIndex;
			FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
				GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, SelectedTOPNetworkIndex), PDGAssetLink.Get());
		};

	TSharedPtr<SHorizontalBox, ESPMode::NotThreadSafe> HorizontalBoxTOPNet;
	TSharedPtr<SComboBox<TSharedPtr<FTextAndTooltip>>> ComboBoxTOPNet;
	int32 SelectedIndex = TOPNetworksPtr.IndexOfByPredicate([PDGAssetLink](const TSharedPtr<FTextAndTooltip>& InEntry)
		{
			if (!IsValidWeakPointer(PDGAssetLink))
				return false;

			return InEntry.IsValid() && InEntry->Value == PDGAssetLink->SelectedTOPNetworkIndex;
		});
	if(SelectedIndex < 0)
		SelectedIndex = 0;

	PDGTOPNetRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(300.f)
		.MaxWidth(300.f)
		[
			SAssignNew(ComboBoxTOPNet, SComboBox<TSharedPtr<FTextAndTooltip>>)
				.OptionsSource(&TOPNetworksPtr)
				.InitiallySelectedItem(TOPNetworksPtr[SelectedIndex])
				.OnGenerateWidget_Lambda([](TSharedPtr<FTextAndTooltip> ChoiceEntry)
					{
						const FText ChoiceEntryText = FText::FromString(ChoiceEntry->Text);
						const FText ChoiceEntryToolTip = FText::FromString(ChoiceEntry->ToolTip);
						return SNew(STextBlock)
							.Text(ChoiceEntryText)
							.ToolTipText(ChoiceEntryToolTip)
							.Margin(2.0f)
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
					})
				.OnSelectionChanged_Lambda([OnTOPNetChanged](TSharedPtr<FTextAndTooltip> NewChoice, ESelectInfo::Type SelectType)
					{
						return OnTOPNetChanged(NewChoice);
					})
				[
					SNew(STextBlock)
						.Text_Lambda([PDGAssetLink]()
							{
								if (!IsValidWeakPointer(PDGAssetLink))
									return FText();

								return FText::FromString(PDGAssetLink->GetSelectedTOPNetworkName());
							})
						.ToolTipText_Lambda([PDGAssetLink]()
							{
								if (!IsValidWeakPointer(PDGAssetLink))
									return FText();

								UTOPNetwork const* const Network = PDGAssetLink->GetSelectedTOPNetwork();
								if(IsValid(Network))
								{
									if(!Network->NodePath.IsEmpty())
										return FText::FromString(Network->NodePath);
									else
										return FText::FromString(Network->NodeName);
								}
								else
								{
									return FText();
								}
							})
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
		];
}


bool FHoudiniPDGDetails::IsSelectedNetworkCookingOrLoading(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
{
	if(!IsValidWeakPointer(InPDGAssetLink))
		return false;

	if(!IsPDGLinked(InPDGAssetLink))
		return false;

	if(!InPDGAssetLink->GetSelectedTOPNetwork())
		return false;

	EPDGNodeState State = InPDGAssetLink->GetSelectedTOPNetwork()->NetworkState;
	EPDGLoadState LoadState = InPDGAssetLink->GetSelectedTOPNetwork()->LoadState;

	return (State == EPDGNodeState::Cooking || LoadState == EPDGLoadState::Loading);
}

bool FHoudiniPDGDetails::IsTOPCooking(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
{
	if(!IsValidWeakPointer(InPDGAssetLink))
		return false;

	if(!IsPDGLinked(InPDGAssetLink))
		return false;

	UTOPNode* SelectedNode = InPDGAssetLink->GetSelectedTOPNode();
	if(!IsValid(SelectedNode))
		return false;

	return !SelectedNode->bHidden && SelectedNode->NodeState != EPDGNodeState::Cooking && !SelectedNode->AnyWorkItemsPending();
}

bool FHoudiniPDGDetails::IsSOPCooking(const TWeakObjectPtr<UHoudiniCookable>& InCookable)
{
	if(!InCookable.IsValid())
		return false;

	EHoudiniAssetState State = InCookable.Get()->GetCurrentState();

	switch (State)
	{
	case EHoudiniAssetState::PreCook:
	case EHoudiniAssetState::Cooking:
	case EHoudiniAssetState::PostCook:
	case EHoudiniAssetState::PreProcess:
	case EHoudiniAssetState::Processing:
		return true;

	case EHoudiniAssetState::None:
	case EHoudiniAssetState::NeedInstantiation:
	case EHoudiniAssetState::NewHDA:
	case EHoudiniAssetState::PreInstantiation:
	case EHoudiniAssetState::Instantiating:
	case EHoudiniAssetState::NeedDelete:
	case EHoudiniAssetState::Deleting:

	case EHoudiniAssetState::ProcessTemplate:
	case EHoudiniAssetState::Dormant:
	default:
		return false;

	}

}

void FHoudiniPDGDetails::AddTOPNetworkDirtyAllAndCookOutputWidgets(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	TSharedRef<SHorizontalBox> DirtyAllHBox = SNew(SHorizontalBox);
	TSharedPtr<SHorizontalBox> CookOutHBox = SNew(SHorizontalBox);

	auto DirtyAll = [this](const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
		{
			if(IsValidWeakPointer(InPDGAssetLink))
			{
				UTOPNetwork* const TOPNetwork = InPDGAssetLink->GetSelectedTOPNetwork();
				if(IsValid(TOPNetwork))
				{
					if(IsPDGLinked(InPDGAssetLink))
					{
						FHoudiniPDGManager::DirtyAll(TOPNetwork);
					}
					else
					{
						UHoudiniPDGAssetLink::ClearTOPNetworkWorkItemResults(TOPNetwork);
					}
				}
			}
		};


	FDetailWidgetRow& PDGDirtyCookRow = TOPNetWorkGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
						.WidthOverride(200.0f)
						[
							SNew(SButton)
								//.Text(LOCTEXT("DirtyAll", "Dirty All"))
								.ToolTipText(LOCTEXT("DirtyAllTooltip", "Dirty all TOP nodes in the selected TOP network and clears all of its work item results."))
								.ContentPadding(FMargin(5.0f, 5.0f))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled_Lambda([PDGAssetLink]()
								{
										return !IsSelectedNetworkCookingOrLoading(PDGAssetLink);
								})
								.OnClicked_Lambda([PDGAssetLink, DirtyAll]()
									{
										DirtyAll(PDGAssetLink);
										return FReply::Handled();
									})
								.Content()
								[
									SAssignNew(DirtyAllHBox, SHorizontalBox)
								]
						]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
						.WidthOverride(200.0f)
						[
							SNew(SButton)
								//.Text(LOCTEXT("CookOut", "Cook Output"))
								.ToolTipText(LOCTEXT("CookOutTooltip", "Cooks the output nodes of the selected TOP network"))
								.ContentPadding(FMargin(5.0f, 5.0f))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled_Lambda([PDGAssetLink]()
									{
										return !IsSelectedNetworkCookingOrLoading(PDGAssetLink);
									})
								.OnClicked_Lambda([PDGAssetLink, InHC, DirtyAll]()
									{
										if (!IsValidWeakPointer(PDGAssetLink) || !IsValidWeakPointer(InHC))
											return FReply::Handled();

										UTOPNetwork* const TOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();
										if(IsValid(TOPNetwork))
										{
											FHoudiniPDGManager::CookOutput(InHC.Get(), TOPNetwork);
										}
										return FReply::Handled();
									})
								.Content()
								[
									SAssignNew(CookOutHBox, SHorizontalBox)
								]
						]
				]
		];

	TSharedPtr<FSlateDynamicImageBrush> DirtyAllIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGDirtyAllIconBrush();
	if(DirtyAllIconBrush.IsValid())
	{
		TSharedPtr<SImage> DirtyAllImage;
		DirtyAllHBox->AddSlot()
			.MaxWidth(16.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(DirtyAllImage, SImage)
					]
			];

		DirtyAllImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([DirtyAllIconBrush]() { return DirtyAllIconBrush.Get(); })));
	}

	DirtyAllHBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("DirtyAll", "Dirty All"))
		];

	TSharedPtr<FSlateDynamicImageBrush> CookOutIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIRecookIconBrush();
	if(CookOutIconBrush.IsValid())
	{
		TSharedPtr<SImage> CookOutImage;
		CookOutHBox->AddSlot()
			.MaxWidth(16.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(CookOutImage, SImage)
					]
			];

		CookOutImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([CookOutIconBrush]() { return CookOutIconBrush.Get(); })));
	}

	CookOutHBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text_Lambda([PDGAssetLink]()
				{
					if (!IsValidWeakPointer(PDGAssetLink))
						return LOCTEXT("CookOut", "Cook Output");

					return PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded ? LOCTEXT("CookOutAndBake", "Cook Output & Bake") : LOCTEXT("CookOut", "Cook Output");
				})
		];

	BindEnablePDGWiddgetsTest(PDGDirtyCookRow, InHC);
}

TSharedPtr<SBox> FHoudiniPDGDetails::AddTOPNetworkCancelWidgets(const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	TSharedPtr<SHorizontalBox> CancelHBox = SNew(SHorizontalBox);

	TSharedPtr<SBox> CancelBox;
	if(!IsValidWeakPointer(InHC))
		return CancelBox;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	CancelBox = SNew(SBox)
		.WidthOverride(200.0f)
		[
			SNew(SButton)
				.ToolTipText(LOCTEXT("CancelTooltip", "Cancels cooking the selected TOP network"))
				.ContentPadding(FMargin(5.0f, 2.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.IsEnabled_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink) || !IsPDGLinked(PDGAssetLink))
							return false;

						UTOPNetwork* TopNetwork = PDGAssetLink->GetSelectedTOPNetwork();
						if(!TopNetwork)
							return false;

						EPDGNodeState State = TopNetwork->NetworkState;
						bool bEnabled = TopNetwork->NetworkState == EPDGNodeState::Cooking || TopNetwork->NetworkState == EPDGNodeState::Paused ||
							TopNetwork->LoadState == EPDGLoadState::Loading;

						return bEnabled;
					})
				.OnReleased_Lambda([PDGAssetLink]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return;

						UTOPNetwork* const TOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();
						if(IsValid(TOPNetwork))
						{
							FHoudiniPDGManager::CancelCook(TOPNetwork);
						}
					})
				.Content()
				[
					SAssignNew(CancelHBox, SHorizontalBox)
				]
		];

	TSharedPtr<FSlateDynamicImageBrush> CancelIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGCancelIconBrush();
	if(CancelIconBrush.IsValid())
	{
		TSharedPtr<SImage> CancelImage;
		CancelHBox->AddSlot()
			.MaxWidth(16.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(CancelImage, SImage)
					]
			];

		CancelImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([CancelIconBrush]() { return CancelIconBrush.Get(); })));
	}

	CancelHBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("Cancel", "Cancel Cook"))
		];

	return CancelBox;
}

TSharedPtr<SBox> FHoudiniPDGDetails::AddTOPNetworkPauseWidgets(const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	TSharedPtr<SBox> PauseBox;
	if(!IsValidWeakPointer(InHC))
		return PauseBox;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();


	TSharedRef<SHorizontalBox> PauseHBox = SNew(SHorizontalBox);

	PauseBox = SNew(SBox)
		.WidthOverride(200.0f)
		[
			SNew(SButton)
				.ToolTipText(LOCTEXT("PauseTooltip", "Pauses cooking for the selected TOP Network"))
				.ContentPadding(FMargin(5.0f, 2.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.IsEnabled_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink) || !IsPDGLinked(PDGAssetLink))
							return false;

						UTOPNetwork* TopNetwork = PDGAssetLink->GetSelectedTOPNetwork();
						if(!TopNetwork)
							return false;

						if (TopNetwork->NetworkState == EPDGNodeState::Cooking || 
							TopNetwork->LoadState == EPDGLoadState::Loading || 
							TopNetwork->IsPaused())
							return true;

						return false;
					})
				.OnReleased_Lambda([PDGAssetLink, PauseHBox]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return;

						UTOPNetwork* const TOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();
						if(IsValid(TOPNetwork))
						{
							if (TOPNetwork->IsPaused())
							{
								FHoudiniPDGManager::ResumeCook(TOPNetwork);
							}
							else
							{
								FHoudiniPDGManager::PauseCook(TOPNetwork);
							}
						}
					//	return FReply::Handled();
					})
				.Content()
				[
					SAssignNew(PauseHBox, SHorizontalBox)
				]
		];

	TSharedPtr<FSlateDynamicImageBrush> PauseIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGPauseIconBrush();
	if(PauseIconBrush.IsValid())
	{
		TSharedPtr<SImage> PauseImage;
		PauseHBox->AddSlot()
			.MaxWidth(16.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(PauseImage, SImage)
					]
			];

		PauseImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([PauseIconBrush]() { return PauseIconBrush.Get(); })));
	}


	PauseHBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text_Lambda([PDGAssetLink]()
					{
						auto PauseText = LOCTEXT("Pause", "Pause Cook");
						auto ResumeText = LOCTEXT("Resume", "Resume Cook");

						if(!IsValidWeakPointer(PDGAssetLink) || !PDGAssetLink->GetSelectedTOPNetwork())
							return PauseText;

						if(PDGAssetLink->GetSelectedTOPNetwork()->IsPaused())
							return ResumeText;
						else
							return PauseText;
					})
		];
	return PauseBox;
}

TSharedPtr<SBox> FHoudiniPDGDetails::AddTOPNodeCancelWidgets(const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	TSharedPtr<SHorizontalBox> CancelHBox = SNew(SHorizontalBox);

	TSharedPtr<SBox> CancelBox;
	if(!IsValidWeakPointer(InHC))
		return CancelBox;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	CancelBox = SNew(SBox)
		.WidthOverride(200.0f)
		[
			SNew(SButton)
				.ToolTipText(LOCTEXT("CancelTooltip", "Cancels cooking the selected TOP network"))
				.ContentPadding(FMargin(5.0f, 2.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.IsEnabled_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink) || !IsPDGLinked(PDGAssetLink))
							return false;

						UTOPNode * TOPNode = PDGAssetLink->GetSelectedTOPNode();
						if(!TOPNode)
							return false;

						if(TOPNode->NodeState == EPDGNodeState::Cooking ||
							TOPNode->NodeState == EPDGNodeState::Paused ||
							TOPNode->LoadState == EPDGLoadState::Loading_Paused ||
							TOPNode->LoadState == EPDGLoadState::Loading)
							return true;

						return false;
					})
				.OnReleased_Lambda([PDGAssetLink]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return;

						UTOPNode* const TOPNode = PDGAssetLink->GetSelectedTOPNode();
						if(IsValid(TOPNode))
						{
							FHoudiniPDGManager::CancelCook(TOPNode);
						}
					})
				.Content()
				[
					SAssignNew(CancelHBox, SHorizontalBox)
				]
		];

	TSharedPtr<FSlateDynamicImageBrush> CancelIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGCancelIconBrush();
	if(CancelIconBrush.IsValid())
	{
		TSharedPtr<SImage> CancelImage;
		CancelHBox->AddSlot()
			.MaxWidth(16.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(CancelImage, SImage)
					]
			];

		CancelImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([CancelIconBrush]() { return CancelIconBrush.Get(); })));
	}

	CancelHBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("Cancel", "Cancel Cook"))
		];

	return CancelBox;
}


TSharedPtr<SBox> FHoudiniPDGDetails::AddTOPNodePauseWidgets(const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	TSharedPtr<SBox> PauseBox;
	if(!IsValidWeakPointer(InHC))
		return PauseBox;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();


	TSharedRef<SHorizontalBox> PauseHBox = SNew(SHorizontalBox);

	PauseBox = SNew(SBox)
		.WidthOverride(200.0f)
		[
			SNew(SButton)
				.ToolTipText(LOCTEXT("PauseTooltip", "Pauses cooking for the selected TOP Network"))
				.ContentPadding(FMargin(5.0f, 2.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.IsEnabled_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink) || !IsPDGLinked(PDGAssetLink))
							return false;

						UTOPNode* TOPNode = PDGAssetLink->GetSelectedTOPNode();
						if(!TOPNode)
							return false;


						if(TOPNode->NodeState == EPDGNodeState::Cooking || 
							TOPNode->NodeState == EPDGNodeState::Paused ||
							TOPNode->LoadState == EPDGLoadState::Loading_Paused ||
							TOPNode->LoadState == EPDGLoadState::Loading)
							return true;

						return false;
					})
				.OnReleased_Lambda([PDGAssetLink, PauseHBox]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return;

						UTOPNode* const TOPNode = PDGAssetLink->GetSelectedTOPNode();
						if(IsValid(TOPNode))
						{
							if(TOPNode->IsPaused())
							{
								FHoudiniPDGManager::ResumeCook(TOPNode);
							}
							else
							{
								FHoudiniPDGManager::PauseCook(TOPNode);
							}
						}
						//	return FReply::Handled();
					})
				.Content()
				[
					SAssignNew(PauseHBox, SHorizontalBox)
				]
		];

	TSharedPtr<FSlateDynamicImageBrush> PauseIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGPauseIconBrush();
	if(PauseIconBrush.IsValid())
	{
		TSharedPtr<SImage> PauseImage;
		PauseHBox->AddSlot()
			.MaxWidth(16.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(PauseImage, SImage)
					]
			];

		PauseImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([PauseIconBrush]() { return PauseIconBrush.Get(); })));
	}


	PauseHBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text_Lambda([PDGAssetLink]()
					{
						auto PauseText = LOCTEXT("Pause", "Pause Cook");
						auto ResumeText = LOCTEXT("Resume", "Resume Cook");

						if(!IsValidWeakPointer(PDGAssetLink) || !PDGAssetLink->GetSelectedTOPNetwork())
							return PauseText;

						if(PDGAssetLink->GetSelectedTOPNode()->IsPaused())
							return ResumeText;
						else
							return PauseText;
					})
		];
	return PauseBox;
}

void FHoudiniPDGDetails::AddTOPNetworkPauseOrCancelWidgets(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TSharedPtr<SBox> PauseBox = AddTOPNetworkPauseWidgets(InHC);
	TSharedPtr<SBox> CancelBox = AddTOPNetworkCancelWidgets(InHC);

	FDetailWidgetRow& PDGPauseOrCancelWidgets = TOPNetWorkGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PauseBox.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CancelBox.ToSharedRef()
				]
		];

	BindEnablePDGWiddgetsTest(PDGPauseOrCancelWidgets, InHC);
}

void FHoudiniPDGDetails::AddTOPNodePauseOrCancelWidgets(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TSharedPtr<SBox> PauseBox = AddTOPNodePauseWidgets(InHC);
	TSharedPtr<SBox> CancelBox = AddTOPNodeCancelWidgets(InHC);

	FDetailWidgetRow& PDGPauseOrCancelWidgets = TOPNetWorkGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PauseBox.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CancelBox.ToSharedRef()
				]
		];

	BindEnablePDGWiddgetsTest(PDGPauseOrCancelWidgets, InHC);
}


void FHoudiniPDGDetails::AddTOPNetworkUnloadWorkItemsObjectsWidgets(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	FDetailWidgetRow& PDGUnloadLoadWorkItemsRow = TOPNetWorkGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
						.IsEnabled_Lambda([PDGAssetLink]() { return IsValidWeakPointer(PDGAssetLink) && PDGAssetLink->GetSelectedTOPNetwork(); })
						.WidthOverride(200.0f)
						[
							SNew(SButton)
								.Text(LOCTEXT("RemoveImportedOutputsForNetwork", "Remove Imported Outputs"))
								.ToolTipText(LOCTEXT("UnloadWorkItemsForNetworkTooltip", "Removes imported actors created from this TOP Netowork's output files."))
								.ContentPadding(FMargin(5.0f, 2.0f))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled_Lambda([PDGAssetLink]()
									{
										if(!IsValidWeakPointer(PDGAssetLink))
											return false;

										UTOPNetwork* const SelectedNet = PDGAssetLink->GetSelectedTOPNetwork();

										if(!IsValid(SelectedNet))
											return false;

										if(SelectedNet->LoadState != EPDGLoadState::Loading_Complete)
											return false;

										if(!IsValid(SelectedNet) ||
											INDEX_NONE == SelectedNet->AllTOPNodes.IndexOfByPredicate([](const UTOPNode* InNode) { return IsValid(InNode) && InNode->bCachedHaveLoadedWorkResults; }))
											return false;

										return true;
									})
								.OnReleased_Lambda([PDGAssetLink]()
									{
										if(IsValidWeakPointer(PDGAssetLink))
										{
											UTOPNetwork* const TOPNet = PDGAssetLink->GetSelectedTOPNetwork();
											if(IsValid(TOPNet))
											{
												if(IsPDGLinked(PDGAssetLink))
												{
													// Set the state to ToDelete, PDGManager will delete it when processing work items
													TOPNet->SetLoadedWorkResultsToDelete();
												}
												else
												{
													// Delete and unload the result objects and actors now
													TOPNet->DeleteAllWorkResultObjectOutputs();
												}
											}
										}
									})
						]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
						.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
						.WidthOverride(200.0f)
						[
							SNew(SButton)
								.Text(LOCTEXT("ImportOutputFiles", "Import Output Files"))
								.ToolTipText(LOCTEXT("ImportOutputFilesTooltip", "Loads any unimported work items files and created Unreal assets & actors."))
								.ContentPadding(FMargin(5.0f, 2.0f))
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.IsEnabled_Lambda([PDGAssetLink]()
									{
										if(!IsValidWeakPointer(PDGAssetLink))
											return false;

										UTOPNetwork* const Network = PDGAssetLink->GetSelectedTOPNetwork();
										if(!IsValid(Network))
											return false;

										if(Network->LoadState == EPDGLoadState::LoadDisabled || Network->LoadState == EPDGLoadState::Unloaded)
											return true;
										else
											return false;
									})
								.OnReleased_Lambda([PDGAssetLink]()
									{
										if(IsValidWeakPointer(PDGAssetLink))
										{
											UTOPNetwork * const Network = PDGAssetLink->GetSelectedTOPNetwork();
											if(IsValid(Network))
											{
												Network->SetNotLoadedWorkResultsToLoad(true, PDGAssetLink->TOPOutputFilter);
											}
										}
									})
						]
				]
		];

	BindEnablePDGWiddgetsTest(PDGUnloadLoadWorkItemsRow, InHC);
}

bool
FHoudiniPDGDetails::GetSelectedTOPNetworkStatusAndColor(const TWeakObjectPtr<UHoudiniCookable>& InHC, FString& OutTOPNodeStatus, FLinearColor& OutTOPNodeStatusColor)
{
	OutTOPNodeStatus = FString();
	OutTOPNodeStatusColor = FLinearColor::White;

	if(!IsValidWeakPointer(InHC))
		return false;


	UHoudiniPDGAssetLink* AssetLink = InHC->GetPDGAssetLink();
	if(!AssetLink)
		return false;

	if (IsSOPCooking(InHC))
	{
		OutTOPNodeStatus = TEXT("SOP Nodes Are Cooking");
		OutTOPNodeStatusColor = FLinearColor::White;
		return true;
	}

	UTOPNetwork* TopNetwork = AssetLink->GetSelectedTOPNetwork();
	if(IsValid(TopNetwork))
	{
		OutTOPNodeStatus = UHoudiniPDGAssetLink::GetTOPNodeStatus(TopNetwork->NetworkState);
		OutTOPNodeStatusColor = UHoudiniPDGAssetLink::GetTOPNodeStatusColor(TopNetwork->NetworkState);
		return true;
	}

	return false;
}

bool
FHoudiniPDGDetails::GetSelectedTOPNetworkCombinedStatusAndColor(const TWeakObjectPtr<UHoudiniCookable>& InHC, FString& OutTOPNodeStatus, FLinearColor& OutTOPNodeStatusColor)
{
	OutTOPNodeStatus = FString();
	OutTOPNodeStatusColor = FLinearColor::White;

	if(!IsValidWeakPointer(InHC))
		return false;

	UHoudiniPDGAssetLink* AssetLink = InHC->GetPDGAssetLink();
	if(!AssetLink)
		return false;

	if(IsSOPCooking(InHC))
	{
		OutTOPNodeStatus = TEXT("SOP Nodes Are Cooking");
		OutTOPNodeStatusColor = FLinearColor::White;
		return true;
	}

	UTOPNetwork* TopNetwork = AssetLink->GetSelectedTOPNetwork();

	if(!IsValid(TopNetwork))
		return false;

	OutTOPNodeStatus = UHoudiniPDGAssetLink::GetTOPNodeStatus(TopNetwork->NetworkState);
	OutTOPNodeStatusColor = UHoudiniPDGAssetLink::GetTOPNodeStatusColor(TopNetwork->NetworkState);

	if (TopNetwork->NetworkState == EPDGNodeState::Cook_Complete)
	{
		switch(TopNetwork->LoadState)
		{
		case EPDGLoadState::Loading:
			OutTOPNodeStatus = TEXT("PDG Cooked, Importing Work Items");
			OutTOPNodeStatusColor = FLinearColor(0.0, 1.0f, 1.0f);
			break;
		case EPDGLoadState::Loading_Complete:
			OutTOPNodeStatus = TEXT("PDG Cooked, Work Items Imported");
			OutTOPNodeStatusColor = FLinearColor::Green;
			break;
		case EPDGLoadState::Loading_Failed:
			OutTOPNodeStatus = TEXT("PDG Cooked, Work Items Failed To Load");
			OutTOPNodeStatusColor = FLinearColor::Red;
			break;
		default:
			break;
		}
	}

	if (TopNetwork->LoadState == EPDGLoadState::Unloaded)
	{
		OutTOPNodeStatus = TEXT("Work Items Unloaded");
	}
	return false;
}

bool
FHoudiniPDGDetails::GetSelectedTOPNodeCombinedStatusAndColor(const TWeakObjectPtr<UHoudiniCookable>& InHC, FString& OutTOPNodeStatus, FLinearColor& OutTOPNodeStatusColor)
{
	OutTOPNodeStatus = FString();
	OutTOPNodeStatusColor = FLinearColor::White;

	if(!IsValidWeakPointer(InHC))
		return false;

	UHoudiniPDGAssetLink* AssetLink = InHC->GetPDGAssetLink();
	if(!AssetLink)
		return false;

	if(IsSOPCooking(InHC))
	{
		OutTOPNodeStatus = TEXT("SOP Nodes Are Cooking");
		OutTOPNodeStatusColor = FLinearColor::White;
		return true;
	}

	UTOPNode * TOPNode = AssetLink->GetSelectedTOPNode();

	if(!IsValid(TOPNode))
		return false;

	OutTOPNodeStatus = UHoudiniPDGAssetLink::GetTOPNodeStatus(TOPNode->NodeState);
	OutTOPNodeStatusColor = UHoudiniPDGAssetLink::GetTOPNodeStatusColor(TOPNode->NodeState);

	if(TOPNode->NodeState == EPDGNodeState::Cook_Complete)
	{
		switch(TOPNode->LoadState)
		{
		case EPDGLoadState::Loading:
			OutTOPNodeStatus = TEXT("PDG Cooked, Importing Work Items");
			OutTOPNodeStatusColor = FLinearColor(0.0, 1.0f, 1.0f);
			break;
		case EPDGLoadState::Loading_Complete:
			OutTOPNodeStatus = TEXT("PDG Cooked, Work Items Imported");
			OutTOPNodeStatusColor = FLinearColor::Green;
			break;
		case EPDGLoadState::Loading_Failed:
			OutTOPNodeStatus = TEXT("PDG Cooked, Work Items Failed To Load");
			OutTOPNodeStatusColor = FLinearColor::Red;
		default:
			break;
		}
	}

	if(TOPNode->LoadState == EPDGLoadState::Unloaded)
	{
		OutTOPNodeStatus = TEXT("Work Items Unloaded");
	}
	return false;
}

bool
FHoudiniPDGDetails::GetSelectedTOPNodeStatusAndColor(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, FString& OutTOPNodeStatus, FLinearColor &OutTOPNodeStatusColor)
{
	OutTOPNodeStatus = FString();
	OutTOPNodeStatusColor = FLinearColor::White;
	if (IsValidWeakPointer(InPDGAssetLink))
	{
		UTOPNode* const TOPNode = InPDGAssetLink->GetSelectedTOPNode();
		if (IsValid(TOPNode) && !TOPNode->bHidden)
		{
			OutTOPNodeStatus = UHoudiniPDGAssetLink::GetTOPNodeStatus(TOPNode);
			OutTOPNodeStatusColor = UHoudiniPDGAssetLink::GetTOPNodeStatusColor(TOPNode);
			
			return true;
		}
	}

	return false;
}

void
FHoudiniPDGDetails::AddTOPNodeWidget(
	IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	if (!PDGAssetLink->GetSelectedTOPNetwork())
		return;

	if(InHC->GetIsPCG())
		return;

	FString GroupLabel = TEXT("TOP Nodes");
	FString GroupName = FString::Printf(TEXT("%s %s"), *GroupLabel, *InHC.Get()->GetCookableGUID().ToString());
	IDetailGroup& TOPNodesGrp = InPDGCategory.AddGroup(FName(*GroupName), FText::FromString(GroupLabel), false);

	AddTOPNodeFilter(TOPNodesGrp, InHC);


	// Combobox: TOP Node
	{
		FDetailWidgetRow& PDGTOPNodeRow = TOPNodesGrp.AddWidgetRow();
		BindEnablePDGWiddgetsTest(PDGTOPNodeRow, InHC);
		PDGTOPNodeRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
			];

		// Update the TOP Node SharedString
		TOPNodesPtr.Reset();
		TOPNodesPtr.Add(MakeShareable(new FTextAndTooltip(INDEX_NONE, LOCTEXT("ComboBoxEntryNoSelectedTOPNode", "- Select -").ToString())));
		const UTOPNetwork* const SelectedTOPNet = PDGAssetLink->GetSelectedTOPNetwork();
		if (IsValid(SelectedTOPNet))
		{
			const int32 NumTOPNodes = SelectedTOPNet->AllTOPNodes.Num();
			for (int32 Idx = 0; Idx < NumTOPNodes; Idx++)
			{
				const UTOPNode* const Node = SelectedTOPNet->AllTOPNodes[Idx]; 
				if (!IsValid(Node) || Node->bHidden)
					continue;
				
				TOPNodesPtr.Add(MakeShareable(new FTextAndTooltip(
					Idx,
					FHoudiniEngineEditorUtils::GetNodeNamePaddedByPathDepth(
						FormatTOPNodeName(PDGAssetLink, Node->NodeName),
						Node->NodePath),
					Node->NodePath
				)));
			}

			// We only want to sort the range after the empty selection.
			// That is, we always want "- Select -" at the top of the combobox.
			{
				const auto TOPNodesView = 
					TArrayView<TSharedPtr<FTextAndTooltip>>(TOPNodesPtr).RightChop(1);
				Algo::Sort(TOPNodesView, &IsTextAndTooltipLexicographicallyBefore);
			}
		}

		FString NodeErrorText = FString();
		FString NodeErrorTooltip = FString();
		FLinearColor NodeErrorColor = FLinearColor::White;
		if (!IsValid(SelectedTOPNet) || SelectedTOPNet->AllTOPNodes.Num() <= 0)
		{
			NodeErrorText = TEXT("No valid TOP Node found!");
			NodeErrorTooltip = TEXT("There is no valid TOP Node found in the selected TOP Network!");
			NodeErrorColor = FLinearColor::Red;
		}
		else if(TOPNodesPtr.Num() <= 0)
		{
			NodeErrorText = TEXT("No visible TOP Node found!");
			NodeErrorTooltip = TEXT("No visible TOP Node found, all nodes in this network are hidden. Please update your TOP Node Filter.");
			NodeErrorColor = FLinearColor::Yellow;
		}

		// Lambda for selecting a TOPNode
		auto OnTOPNodeChanged = [PDGAssetLink](TSharedPtr<FTextAndTooltip> InNewChoice)
		{
			if (!IsValidWeakPointer(PDGAssetLink))
				return;

			UTOPNetwork* const TOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();
			if (!InNewChoice.IsValid() || !IsValid(TOPNetwork))
				return;

			const int32 NewChoice = InNewChoice->Value;
			int32 NewSelectedIndex = INDEX_NONE;
			if (TOPNetwork->AllTOPNodes.IsValidIndex(NewChoice))
				NewSelectedIndex = NewChoice;

			if (TOPNetwork->SelectedTOPIndex != NewSelectedIndex)
			{
				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_RUNTIME),
					LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
					TOPNetwork);
				
				TOPNetwork->Modify();
				TOPNetwork->SelectedTOPIndex = NewSelectedIndex;
				FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
					GET_MEMBER_NAME_STRING_CHECKED(UTOPNetwork, SelectedTOPIndex), TOPNetwork);
			}
		};
		
		TSharedPtr<SHorizontalBox, ESPMode::NotThreadSafe> HorizontalBoxTOPNode;
		TSharedPtr<SComboBox<TSharedPtr<FTextAndTooltip>>> ComboBoxTOPNode;
		int32 SelectedIndex = 0;
		UTOPNetwork* const SelectedTOPNetwork = PDGAssetLink->GetSelectedTOPNetwork();
		if (IsValid(SelectedTOPNetwork) && SelectedTOPNetwork->SelectedTOPIndex >= 0)
		{
			//SelectedIndex = InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex;

			// We need to match the selection by the index in the AllTopNodes array
			// Because of the nodefilter, it is possible that the selected index does not match the index in TOPNodesPtr
			const int32 SelectedTOPNodeIndex = SelectedTOPNetwork->SelectedTOPIndex;
			// Find the matching UI index
			for (int32 UIIndex = 0; UIIndex < TOPNodesPtr.Num(); UIIndex++)
			{
				if (TOPNodesPtr[UIIndex] && TOPNodesPtr[UIIndex]->Value == SelectedTOPNodeIndex)
				{
					// We found the UI Index that matches the current TOP Node!
					SelectedIndex = UIIndex;
					break;
				}
			}
		}

		TSharedPtr<STextBlock> ErrorText;

		PDGTOPNodeRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(300.f)
			.MaxWidth(300.f)
			[
				SAssignNew(ComboBoxTOPNode, SComboBox<TSharedPtr<FTextAndTooltip>>)
				.OptionsSource(&TOPNodesPtr)
				.InitiallySelectedItem(TOPNodesPtr[SelectedIndex])
				.OnGenerateWidget_Lambda([](TSharedPtr<FTextAndTooltip> ChoiceEntry)
				{
					const FText ChoiceEntryText = FText::FromString(ChoiceEntry->Text);
					const FText ChoiceEntryToolTip = FText::FromString(ChoiceEntry->ToolTip);
					return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryToolTip)
					.Margin(2.0f)
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
				})
				.OnSelectionChanged_Lambda([OnTOPNodeChanged](TSharedPtr<FTextAndTooltip> NewChoice, ESelectInfo::Type SelectType)
				{
					return OnTOPNodeChanged(NewChoice);
				})
				[
					SNew(STextBlock)
					.Text_Lambda([PDGAssetLink, ComboBoxTOPNode, Options = TOPNodesPtr]()
					{
						if (IsValidWeakPointer(PDGAssetLink))
							return FText::FromString(FormatTOPNodeName(
								PDGAssetLink, PDGAssetLink->GetSelectedTOPNodeName()));
						else
							return FText();
					})
					.ToolTipText_Lambda([PDGAssetLink]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return FText();

						UTOPNode const * const TOPNode = PDGAssetLink->GetSelectedTOPNode();
						if (IsValid(TOPNode))
						{
							if (!TOPNode->NodePath.IsEmpty())
								return FText::FromString(TOPNode->NodePath);
							else
								return FText::FromString(TOPNode->NodeName);
						}
						else
						{
							return FText();
						}
					})
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoWidth()
			[
				SAssignNew(ErrorText, STextBlock)
				.Text(FText::FromString(NodeErrorText))
				.ToolTipText(FText::FromString(NodeErrorText))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ColorAndOpacity(FLinearColor::Red)
				//.ShadowColorAndOpacity(FLinearColor::Black)
			];

		// Update the error text if needed
		ErrorText->SetText(FText::FromString(NodeErrorText));
		ErrorText->SetToolTipText(FText::FromString(NodeErrorTooltip));
		ErrorText->SetColorAndOpacity(NodeErrorColor);

		// Hide the combobox if we have an error
		ComboBoxTOPNode->SetVisibility(NodeErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden);
	}


	// Buttons: DIRTY NODE / COOK NODE
	if (!InHC->GetIsPCG())
	{
		TSharedRef<SHorizontalBox> DirtyHBox = SNew(SHorizontalBox);
		TSharedPtr<SHorizontalBox> CookHBox = SNew(SHorizontalBox);

		TSharedPtr<SButton> DirtyButton;
		TSharedPtr<SButton> CookButton;

		FDetailWidgetRow& PDGDirtyCookRow = TOPNodesGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]()
				{
					return IsPDGLinked(PDGAssetLink) || (IsValidWeakPointer(PDGAssetLink) && IsValid(PDGAssetLink->GetSelectedTOPNode()));
				})
				.WidthOverride(200.0f)
				[
					SAssignNew(DirtyButton, SButton)
					//.Text(LOCTEXT("DirtyNode", "Dirty Node"))
					.ToolTipText(LOCTEXT("DirtyNodeTooltip", "Dirties the selected TOP node and clears its work item results."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([PDGAssetLink]()
					{
						return IsPDGLinked(PDGAssetLink) || (IsValidWeakPointer(PDGAssetLink) && IsValid(PDGAssetLink->GetSelectedTOPNode()));
					})
					.OnClicked_Lambda([PDGAssetLink]()
					{
						if (IsValidWeakPointer(PDGAssetLink))
						{
							UTOPNode* const TOPNode = PDGAssetLink->GetSelectedTOPNode();
							if (IsValid(TOPNode))
							{
								if (IsPDGLinked(PDGAssetLink))
								{
                                    FHoudiniPDGManager::DirtyTOPNode(TOPNode);
                                    // FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
                                }
                                else
                                {
                                    UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(TOPNode);
                                }
							}
						}
						
						return FReply::Handled();
					})
					.IsEnabled_Lambda([PDGAssetLink]()
					{
						return IsTOPCooking(PDGAssetLink);
					})
					.Content()
					[
						SAssignNew(DirtyHBox, SHorizontalBox)
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
				.WidthOverride(200.0f)
				[
					SAssignNew(CookButton, SButton)
					.ToolTipText(LOCTEXT("CookNodeTooltip", "Cooks the selected TOP Node."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([PDGAssetLink]()
					{
						return IsTOPCooking(PDGAssetLink);
					})
					.OnClicked_Lambda([PDGAssetLink]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return FReply::Handled();

						UTOPNode* const Node = PDGAssetLink->GetSelectedTOPNode();
						if (IsValid(Node))
						{
							FHoudiniPDGManager::CookTOPNode(Node);
							// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
					.Content()
					[
						SAssignNew(CookHBox, SHorizontalBox)
					]
				]
			]
		];

		TSharedPtr<FSlateDynamicImageBrush> DirtyIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIPDGDirtyNodeIconBrush();
		if (DirtyIconBrush.IsValid())
		{
			TSharedPtr<SImage> DirtyImage;
			DirtyHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(DirtyImage, SImage)
					]
				];

			DirtyImage->SetImage(
				TAttribute<const FSlateBrush*>::Create(
					TAttribute<const FSlateBrush*>::FGetter::CreateLambda([DirtyIconBrush]() { return DirtyIconBrush.Get(); })));
		}

		DirtyHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DirtyNode", "Dirty Node"))
			];

		TSharedPtr<FSlateDynamicImageBrush> CookIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIRecookIconBrush();
		if (CookIconBrush.IsValid())
		{
			TSharedPtr<SImage> CookImage;
			CookHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(CookImage, SImage)
					]
				];

			CookImage->SetImage(
				TAttribute<const FSlateBrush*>::Create(
					TAttribute<const FSlateBrush*>::FGetter::CreateLambda([CookIconBrush]() { return CookIconBrush.Get(); })));
		}

		CookHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CookNode", "Cook Node"))
			];

		BindEnablePDGWiddgetsTest(PDGDirtyCookRow, InHC);
	}

	AddTOPNodePauseOrCancelWidgets(TOPNodesGrp, InHC);

	// Buttons: Load Work Item Objects / Unload Work Item Objects
	if (!InHC->GetIsPCG())
	{
		TSharedPtr<SButton> UnloadWorkItemsButton;
		TSharedPtr<SButton> LoadWorkItemsButton;

		FDetailWidgetRow& PDGUnloadLoadWorkItemsRow = TOPNodesGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]()
				{
					return IsValidWeakPointer(PDGAssetLink) && IsValid(PDGAssetLink->GetSelectedTOPNode());
				})
				.WidthOverride(200.0f)
				[
					SAssignNew(UnloadWorkItemsButton, SButton)
					.Text(LOCTEXT("RemoveImportedOutputsForNode", "Remove Imported Outputs"))
					.ToolTipText(LOCTEXT("RemoveImportedOutputsForNodeTooltip", "Removes imported actors created from this TOP Node's output files."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([PDGAssetLink]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return false;

						if(!IsPDGLinked(PDGAssetLink))
							return false;

						UTOPNode* const SelectedNode = PDGAssetLink->GetSelectedTOPNode();
						if (!IsValid(SelectedNode) || SelectedNode->bHidden || !SelectedNode->bCachedHaveLoadedWorkResults)
							return false;

						return true;
					})
					.OnClicked_Lambda([PDGAssetLink]()
					{
						if (IsValidWeakPointer(PDGAssetLink))
						{
							UTOPNode* const TOPNode = PDGAssetLink->GetSelectedTOPNode();
							if (IsValid(TOPNode))
							{
								if (IsPDGLinked(PDGAssetLink))
								{
									// Set the state to ToDelete, PDGManager will delete it when processing work items
									TOPNode->SetLoadedWorkResultsToDelete();
								}
								else
								{
									// Delete and unload the result objects and actors now
									TOPNode->DeleteAllWorkResultObjectOutputs();
								}
							}
						}
						
						return FReply::Handled();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
				.WidthOverride(200.0f)
				[
					SAssignNew(LoadWorkItemsButton, SButton)
					.Text(LOCTEXT("ImportOutputFiles", "Import Output Files"))
					.ToolTipText(LOCTEXT("ImportOutputFilesTooltip", "Loads any unimported work items files and created Unreal assets & actors."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([PDGAssetLink]()
					{
						if (!IsValidWeakPointer(PDGAssetLink))
							return false;

						UTOPNode* const SelectedNode = PDGAssetLink->GetSelectedTOPNode();
						if (!IsValid(SelectedNode) || SelectedNode->bHidden || !SelectedNode->bCachedHaveNotLoadedWorkResults)
							return false;

						return true;
					})
					.OnClicked_Lambda([PDGAssetLink]()
					{
						if (IsValidWeakPointer(PDGAssetLink))
						{
							UTOPNode* const SelectedNode = PDGAssetLink->GetSelectedTOPNode();						
                            if (IsValid(SelectedNode))
                            {
                            	SelectedNode->SetNotLoadedWorkResultsToLoad(true);
                            }
						}
						return FReply::Handled();
					})
				]
			]
		];
	}

	// TOP Node State
	if(!InHC->GetIsPCG())
	{
		AddTOPNodeState(TOPNodesGrp, InHC);
	}

	// TOP Node WorkItem Status
	if(!InHC->GetIsPCG())
	{
		if (PDGAssetLink->GetSelectedTOPNode())
		{
			FDetailWidgetRow& PDGNodeWorkItemStatsRow = TOPNodesGrp.AddWidgetRow();
			BindEnablePDGWiddgetsTest(PDGNodeWorkItemStatsRow, InHC);
			FHoudiniPDGDetails::AddWorkItemStatusWidget(
				PDGNodeWorkItemStatsRow, TEXT("TOP Node Work Item Status"), InHC, true);
		}
	}
}

void FHoudiniPDGDetails::AddTOPNetworkStates(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	if(!IsValidWeakPointer(PDGAssetLink))
		return;

	FDetailWidgetRow& PDGNodeStateResultRow = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGNodeStateResultRow, InHC);
	PDGNodeStateResultRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Network State")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	PDGNodeStateResultRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text_Lambda([InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNetworkStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FText::FromString(TOPNodeStatus);
					})
				.ColorAndOpacity_Lambda([InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNetworkStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FSlateColor(TOPNodeStatusColor);
					})
		];

	FDetailWidgetRow& PDGNodeStateResultRow2 = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGNodeStateResultRow2, InHC);
	PDGNodeStateResultRow2.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("Importing State")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	PDGNodeStateResultRow2.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text_Lambda([InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;

						TOPNodeStatus = UHoudiniPDGAssetLink::GetLoadStatus(InHC->GetPDGAssetLink()->GetSelectedTOPNetwork()->LoadState);

					return FText::FromString(TOPNodeStatus);
					})
				.ColorAndOpacity_Lambda([InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNetworkStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FSlateColor(TOPNodeStatusColor);
					})
		];

}

void FHoudiniPDGDetails::AddTOPNetworkCombinedState(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	if(!IsValidWeakPointer(PDGAssetLink))
		return;

	FDetailWidgetRow& PDGNodeStateResultRow = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGNodeStateResultRow, InHC);
	PDGNodeStateResultRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("State")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	PDGNodeStateResultRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text_Lambda([InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNetworkCombinedStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FText::FromString(TOPNodeStatus);
					})
				.ColorAndOpacity_Lambda([InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNetworkCombinedStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FSlateColor(TOPNodeStatusColor);
					})
		];
}

void FHoudiniPDGDetails::AddTOPNodeState(IDetailGroup& TOPNetWorkGrp, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	if(!IsValidWeakPointer(PDGAssetLink))
		return;

	FDetailWidgetRow& PDGNodeStateResultRow = TOPNetWorkGrp.AddWidgetRow();
	BindEnablePDGWiddgetsTest(PDGNodeStateResultRow, InHC);
	PDGNodeStateResultRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("State")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	PDGNodeStateResultRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text_Lambda([PDGAssetLink, InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNodeCombinedStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FText::FromString(TOPNodeStatus);
					})
				.ColorAndOpacity_Lambda([PDGAssetLink, InHC]()
					{
						FString TOPNodeStatus = FString();
						FLinearColor TOPNodeStatusColor = FLinearColor::White;
						GetSelectedTOPNodeCombinedStatusAndColor(InHC, TOPNodeStatus, TOPNodeStatusColor);
						return FSlateColor(TOPNodeStatusColor);
					})
		];
}

void
FHoudiniPDGDetails::RefreshPDGAssetLink(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
{
	// Repopulate the network and nodes for the assetlink
	if (!IsValidWeakPointer(InPDGAssetLink) || !FHoudiniPDGManager::UpdatePDGAssetLink(InPDGAssetLink.Get()))
		return;
	
	FHoudiniPDGDetails::RefreshUI(InPDGAssetLink, true);
}

void
FHoudiniPDGDetails::RefreshUI(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, const bool& InFullUpdate)
{
	if (!IsValidWeakPointer(InPDGAssetLink))
		return;

	// Update the workitem stats
	InPDGAssetLink->UpdateWorkItemTally();

	// Update the editor properties
	FHoudiniEngineUtils::UpdateEditorProperties(InFullUpdate);
}

void 
FHoudiniPDGDetails::CreatePDGBakeWidgets(IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniCookable>& InHC) 
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	if (!IsValidWeakPointer(PDGAssetLink))
		return;

	FString GroupLabel = TEXT("Bake");
	IDetailGroup& BakeGroup = InPDGCategory.AddGroup(FName(*GroupLabel), FText::FromString(GroupLabel), false, false);

	AddBakeSelectionWidgets(BakeGroup, InHC);

	AddBakeReplaceModeWidgets(BakeGroup, InHC);

	AddBakeFolderWidgets(BakeGroup, InHC);

	AddBakeAdditionalSettingsWidgets(BakeGroup, InHC);

}

void FHoudiniPDGDetails::AddBakeSelectionWidgets(IDetailGroup& InBakeGroup, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{

	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();


	auto OnBakeButtonClickedLambda = [PDGAssetLink]()
		{
			if (!IsValidWeakPointer(PDGAssetLink))
				return FReply::Handled();

			switch(PDGAssetLink->HoudiniEngineBakeOption)
			{
			case EHoudiniEngineBakeOption::ToActor:
			{
				// if (InPDGAssetLink->bIsReplace)
				// 	FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithActors(InPDGAssetLink);
				// else
				FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(PDGAssetLink.Get(), PDGAssetLink->PDGBakeSelectionOption, PDGAssetLink->PDGBakePackageReplaceMode, PDGAssetLink->bRecenterBakedActors);
			}
			break;

			case EHoudiniEngineBakeOption::ToBlueprint:
			{
				// if (InPDGAssetLink->bIsReplace)
				// 	FHoudiniEngineBakeUtils::ReplaceWithBlueprint(InPDGAssetLink);
				// else
				FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(PDGAssetLink.Get(), PDGAssetLink->PDGBakeSelectionOption, PDGAssetLink->PDGBakePackageReplaceMode, PDGAssetLink->bRecenterBakedActors);
			}
			break;

			case EHoudiniEngineBakeOption::ToAsset:
			{
				// TODO
				// This should not happen!
			}
			break;
			}

			return FReply::Handled();
		};

	auto OnDeleteBakeButtonClickedLambda = [InHC, PDGAssetLink]()
		{
			if (PDGAssetLink.IsValid())
				FHoudiniEngineBakeUtils::DeleteBakedPDGOutputs(PDGAssetLink.Get());
			return FReply::Handled();
		};

	auto OnUnlinkBakeButtonClickedLambda = [PDGAssetLink]()
		{
			if (PDGAssetLink.IsValid())
				FHoudiniEngineBakeUtils::UnlinkBakedPDGOutputs(PDGAssetLink.Get());

			return FReply::Handled();
		};

	// Button Row
	FDetailWidgetRow& ButtonRow = InBakeGroup.AddWidgetRow();
	BindEnablePDGWiddgetsTest(ButtonRow, InHC);

	TSharedRef<SHorizontalBox> ButtonRowHorizontalBox = SNew(SHorizontalBox);

	//----------------------------------------
	// Bake Button
	//----------------------------------------

	{
		// BakeButtonHBox contains the image and the text.

		TSharedRef<SHorizontalBox> BakeButtonHBox = SNew(SHorizontalBox);

		TSharedPtr<FSlateDynamicImageBrush> BakeIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIBakeIconBrush();
		if(BakeIconBrush.IsValid())
		{
			TSharedPtr<SImage> BakeImage;
			BakeButtonHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
						.WidthOverride(16.0f)
						.HeightOverride(16.0f)
						[
							SAssignNew(BakeImage, SImage)
						]
				];

			BakeImage->SetImage(TAttribute<const FSlateBrush*>::Create(TAttribute<const FSlateBrush*>::FGetter::CreateLambda([BakeIconBrush]() { return BakeIconBrush.Get(); })));
		}

		BakeButtonHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(FText::FromString("Bake"))
			];

		ButtonRowHorizontalBox->AddSlot()
			.MaxWidth(150.0f)
			[
				SNew(SBox)
					.WidthOverride(75.0f)
					[
						SNew(SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText_Lambda([PDGAssetLink]()
								{
									switch(PDGAssetLink->HoudiniEngineBakeOption)
									{
									case EHoudiniEngineBakeOption::ToActor:
									{
										return LOCTEXT(
											"HoudiniEnginePDGBakeButtonBakeToActorToolTip",
											"Bake this Houdini PD G Asset's output assets and seperate the output actors from the PDG asset link.");
									}
									break;

									case EHoudiniEngineBakeOption::ToBlueprint:
									{
										return LOCTEXT(
											"HoudiniEnginePDGBakeButtonBakeToBlueprintToolTip",
											"Bake this Houdini PDG Asset's output assets to blueprints and remove temporary output actors that no "
											"longer has output components from the PDG asset link.");
									}
									break;

									case EHoudiniEngineBakeOption::ToAsset:
									default:
									{
										return FText();
									}
									}
								})
							.Visibility(EVisibility::Visible)
							.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
							.OnClicked_Lambda(OnBakeButtonClickedLambda)
							[
								BakeButtonHBox
							]
					]
			];
	}
	//----------------------------------------
	// Delete Bake Button
	//----------------------------------------

	{
		// BakeButtonHBox contains the image and the text.

		TSharedRef<SHorizontalBox> DeleteBakeButtonHBox = SNew(SHorizontalBox);

		TSharedPtr<FSlateDynamicImageBrush> IconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIDeleteBakeIconBrush();

		if(IconBrush.IsValid())
		{
			TSharedPtr<SImage> BakeImage;
			DeleteBakeButtonHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
						.WidthOverride(16.0f)
						.HeightOverride(16.0f)
						[
							SAssignNew(BakeImage, SImage)
						]
				];

			BakeImage->SetImage(TAttribute<const FSlateBrush*>::Create(TAttribute<const FSlateBrush*>::FGetter::CreateLambda([IconBrush]() { return IconBrush.Get(); })));
		}

		DeleteBakeButtonHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(FText::FromString("Delete Bake"))
			];

		ButtonRowHorizontalBox->AddSlot()
			.MaxWidth(150.0f)
			[
				SNew(SBox)
					.WidthOverride(75.0f)
					[
						SNew(SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText(LOCTEXT("HoudiniAssetPFGDetailsDeleteBakeButton", "Delete assets and actors from the previous Bake."))
							.Visibility(EVisibility::Visible)
							.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
							.OnClicked_Lambda(OnDeleteBakeButtonClickedLambda)
							[
								DeleteBakeButtonHBox
							]
					]
			];
	}

	//----------------------------------------
	// Unlink Bake Button
	//----------------------------------------

	{
		// BakeButtonHBox contains the image and the text.

		TSharedRef<SHorizontalBox> UnlinkBakeButtonHBox = SNew(SHorizontalBox);

		TSharedPtr<FSlateDynamicImageBrush> IconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIUnlinkBakeIconBrush();

		if(IconBrush.IsValid())
		{
			TSharedPtr<SImage> BakeImage;
			UnlinkBakeButtonHBox->AddSlot()
				.MaxWidth(16.0f)
				[
					SNew(SBox)
						.WidthOverride(16.0f)
						.HeightOverride(16.0f)
						[
							SAssignNew(BakeImage, SImage)
						]
				];

			BakeImage->SetImage(TAttribute<const FSlateBrush*>::Create(TAttribute<const FSlateBrush*>::FGetter::CreateLambda([IconBrush]() { return IconBrush.Get(); })));
		}

		UnlinkBakeButtonHBox->AddSlot()
			.Padding(5.0, 0.0, 0.0, 0.0)
			.Padding(5.0, 0.0, 0.0, 0.0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(FText::FromString("Unlink Bake"))
			];

		ButtonRowHorizontalBox->AddSlot()
			.MaxWidth(150.0f)
			[
				SNew(SBox)
					.WidthOverride(75.0f)
					[
						SNew(SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText(LOCTEXT("HoudiniAssetPFGDetailsDeleteBakeButton", "Unlinks assets and actors from the previous Bake."))
							.Visibility(EVisibility::Visible)
							.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
							.OnClicked_Lambda(OnUnlinkBakeButtonClickedLambda)
							[
								UnlinkBakeButtonHBox
							]
					]
			];
	}

	//---------------------------------------------------------------------------------------------------------------------------------------------------
	// Bake Target (Actor or Blueprint) aka BakeTypeOptions
	//---------------------------------------------------------------------------------------------------------------------------------------------------

	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeComboBox;

	TArray<TSharedPtr<FString>>* BakeTargetEnumLabels = FHoudiniEngineEditor::Get().GetHoudiniEnginePDGBakeTypeOptionsLabels();
	TSharedPtr<FString> BakeTargetEnumInitialValue;
	if(BakeTargetEnumLabels)
	{
		const FString DefaultStr = FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(PDGAssetLink->HoudiniEngineBakeOption);
		const TSharedPtr<FString>* DefaultOption = BakeTargetEnumLabels->FindByPredicate(
			[DefaultStr](const TSharedPtr<FString> & InString)
			{
				return InString.IsValid() && *InString == DefaultStr;
			}
		);
		if(DefaultOption)
			BakeTargetEnumInitialValue = *DefaultOption;
	}

	TSharedRef<SHorizontalBox> BakeOptionRowHorizontalBox = SNew(SHorizontalBox);

	FDetailWidgetRow& BakeTargetDetailWidgetRow = InBakeGroup.AddWidgetRow();

	FHoudiniPDGDetails::BindEnablePDGWiddgetsTest(BakeTargetDetailWidgetRow, InHC);

	BakeTargetDetailWidgetRow.NameWidget.Widget = SNew(STextBlock)
		.Text(FText::FromString("Bake Output"))
		.ToolTipText(FText::FromString(TEXT("The type of Unreal object the baked outputs should use.")))
		.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT));

	BakeTargetDetailWidgetRow.ValueWidget.Widget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.MaxWidth(93.f)
		[
			SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
				.WidthOverride(93.f)
				[
					SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(BakeTargetEnumLabels)
						.InitiallySelectedItem(BakeTargetEnumInitialValue)
						.OnGenerateWidget_Lambda(
							[](TSharedPtr< FString > InItem)
							{
								FText ChoiceEntryText = FText::FromString(*InItem);
								return SNew(STextBlock)
									.Text(ChoiceEntryText)
									.ToolTipText(ChoiceEntryText)
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
							})
						.OnSelectionChanged_Lambda(
							[PDGAssetLink](const TSharedPtr<FString> & NewChoice, ESelectInfo::Type SelectType)
							{
								if(!IsValidWeakPointer(PDGAssetLink))
									return;

								if(!NewChoice.IsValid() || SelectType == ESelectInfo::Type::Direct)
									return;

								const EHoudiniEngineBakeOption NewOption =
									FHoudiniEngineEditor::Get().StringToHoudiniEngineBakeOption(*NewChoice.Get());

								if(NewOption != PDGAssetLink->HoudiniEngineBakeOption)
								{
									// Record a transaction for undo/redo
									FScopedTransaction Transaction(
										TEXT(HOUDINI_MODULE_RUNTIME),
										LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
										PDGAssetLink.Get());

									PDGAssetLink->Modify();
									PDGAssetLink->HoudiniEngineBakeOption = NewOption;
									FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
										GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, HoudiniEngineBakeOption), PDGAssetLink.Get());
								}
							})
						[
							SNew(STextBlock)
								.Text_Lambda([PDGAssetLink, TypeComboBox, BakeTargetEnumLabels]()
									{
										return FText::FromString(FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(PDGAssetLink->HoudiniEngineBakeOption));
									})
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
				]
		];

	//---------------------------------------------------------------------------------------------------------------------------------------------------
	// bake selection ComboBox (Network, Node or all)
	//---------------------------------------------------------------------------------------------------------------------------------------------------

	TSharedPtr<SComboBox<TSharedPtr<FString>>> BakeSelectionComboBox;

	TArray<TSharedPtr<FString>>* PDGBakeSelectionOptionSource = FHoudiniEngineEditor::Get().GetHoudiniEnginePDGBakeSelectionOptionsLabels();
	TSharedPtr<FString> PDGBakeSelectionIntialSelection;
	if(PDGBakeSelectionOptionSource)
	{
		PDGBakeSelectionIntialSelection = (*PDGBakeSelectionOptionSource)[(int)PDGAssetLink->PDGBakeSelectionOption];
	}

	FDetailWidgetRow& BakOutputsRow = InBakeGroup.AddWidgetRow();

	FHoudiniPDGDetails::BindEnablePDGWiddgetsTest(BakOutputsRow, InHC);

	BakOutputsRow.NameWidget.Widget = SNew(STextBlock)
		.Text(FText::FromString("Outputs To Bake"))
		.ToolTipText(FText::FromString(TEXT("PDG Outputs to bake.")))
		.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT));

	BakOutputsRow.ValueWidget.Widget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.MaxWidth(163.f)
		[
			SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
				.WidthOverride(163.f)
				[
					SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(PDGBakeSelectionOptionSource)
						.InitiallySelectedItem(PDGBakeSelectionIntialSelection)
						.OnGenerateWidget_Lambda(
							[](TSharedPtr< FString > InItem)
							{
								FText ChoiceEntryText = FText::FromString(*InItem);
								return SNew(STextBlock)
									.Text(ChoiceEntryText)
									.ToolTipText(ChoiceEntryText)
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
							})
						.OnSelectionChanged_Lambda(
							[PDGAssetLink](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
							{
								if(!IsValidWeakPointer(PDGAssetLink) || !NewChoice.IsValid())
									return;

								const EPDGBakeSelectionOption NewOption =
									FHoudiniEngineEditor::Get().StringToPDGBakeSelectionOption(*NewChoice.Get());

								if(NewOption != PDGAssetLink->PDGBakeSelectionOption)
								{
									// Record a transaction for undo/redo
									FScopedTransaction Transaction(
										TEXT(HOUDINI_MODULE_RUNTIME),
										LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
										PDGAssetLink.Get());

									PDGAssetLink->Modify();
									PDGAssetLink->PDGBakeSelectionOption = NewOption;
									FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
										GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakeSelectionOption), PDGAssetLink.Get());
								}
							})
						[
							SNew(STextBlock)
								.Text_Lambda([PDGAssetLink]()
									{
										return FText::FromString(
											FHoudiniEngineEditor::Get().GetStringFromPDGBakeTargetOption(PDGAssetLink->PDGBakeSelectionOption));
									})
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
				]
		];

	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;
}

void FHoudiniPDGDetails::AddBakeReplaceModeWidgets(IDetailGroup& InBakeGroup, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	// Bake package replacement mode row
	FDetailWidgetRow& BakePackageReplaceRow = InBakeGroup.AddWidgetRow();
	BindEnablePDGWiddgetsTest(BakePackageReplaceRow, InHC);

	BakePackageReplaceRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Replace Mode")))
			.ToolTipText(FText::FromString(TEXT("How previous bakes are replaced or kept.")))
			.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	// bake package replace mode ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> BakePackageReplaceModeComboBox;

	TArray<TSharedPtr<FString>>* PDGBakePackageReplaceModeOptionSource = FHoudiniEngineEditor::Get().GetHoudiniEnginePDGBakePackageReplaceModeOptionsLabels();
	TSharedPtr<FString> PDGBakePackageReplaceModeInitialSelec;
	if(PDGBakePackageReplaceModeOptionSource)
	{
		const FString DefaultStr = FHoudiniEngineEditor::Get().GetStringFromPDGBakePackageReplaceModeOption(PDGAssetLink->PDGBakePackageReplaceMode);
		const TSharedPtr<FString>* DefaultOption = PDGBakePackageReplaceModeOptionSource->FindByPredicate(
			[DefaultStr](TSharedPtr<FString> InStringPtr)
			{
				return InStringPtr.IsValid() && *InStringPtr == DefaultStr;
			}
		);
		if(DefaultOption)
			PDGBakePackageReplaceModeInitialSelec = *DefaultOption;
	}

	// bake Type ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeComboBox;

	BakePackageReplaceRow.ValueWidget.Widget = 
		SNew(SBox)
			.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
			.WidthOverride(163.f)
			[
				SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(PDGBakePackageReplaceModeOptionSource)
					.InitiallySelectedItem(PDGBakePackageReplaceModeInitialSelec)
					.OnGenerateWidget_Lambda(
						[](TSharedPtr< FString > InItem)
						{
							const FText ChoiceEntryText = FText::FromString(*InItem);
							return SNew(STextBlock)
								.Text(ChoiceEntryText)
								.ToolTipText(ChoiceEntryText)
								.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT));
						})
					.OnSelectionChanged_Lambda(
						[PDGAssetLink](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
						{
							if(!IsValidWeakPointer(PDGAssetLink) || !NewChoice.IsValid())
								return;

							const EPDGBakePackageReplaceModeOption NewOption =
								FHoudiniEngineEditor::Get().StringToPDGBakePackageReplaceModeOption(*NewChoice.Get());

							if(NewOption != PDGAssetLink->PDGBakePackageReplaceMode)
							{
								// Record a transaction for undo/redo
								FScopedTransaction Transaction(
									TEXT(HOUDINI_MODULE_RUNTIME),
									LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
									PDGAssetLink.Get());

								PDGAssetLink->Modify();
								PDGAssetLink->PDGBakePackageReplaceMode = NewOption;
								FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
									GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakePackageReplaceMode), PDGAssetLink.Get());
							}
						})
					[
						SNew(STextBlock)
							.Text_Lambda([PDGAssetLink]()
								{
									return FText::FromString(
										FHoudiniEngineEditor::Get().GetStringFromPDGBakePackageReplaceModeOption(PDGAssetLink->PDGBakePackageReplaceMode));
								})
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
			];

}

void FHoudiniPDGDetails::AddBakeFolderWidgets(IDetailGroup& InBakeGroup, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	auto OnBakeFolderTextCommittedLambda = [PDGAssetLink](const FText& Val, ETextCommit::Type TextCommitType)
		{
			if(!IsValidWeakPointer(PDGAssetLink))
				return;

			FString NewPathStr = Val.ToString();
			if(NewPathStr.IsEmpty())
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				PDGAssetLink.Get());

			//Todo? Check if the new Bake folder path is valid
			PDGAssetLink->Modify();
			PDGAssetLink->BakeFolder.Path = NewPathStr;
			FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
				GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, BakeFolder), PDGAssetLink.Get());
		};

	// Bake Folder Row
	FDetailWidgetRow& BakeFolderRow = InBakeGroup.AddWidgetRow();
	BindEnablePDGWiddgetsTest(BakeFolderRow, InHC);

	BakeFolderRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("Bake Folder")))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
				.ToolTipText(LOCTEXT(
					"HoudiniEnginePDGBakeFolderTooltip",
					"The folder used to store the objects that are generated by this Houdini PDG Asset when baking, if the "
					"unreal_bake_folder attribute is not set on the geometry. If this value is blank, the default from the "
					"plugin settings is used."))
		];
		
	BakeFolderRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SBox)
				.IsEnabled_Lambda([PDGAssetLink]() { return IsPDGLinked(PDGAssetLink); })
				.WidthOverride(235.0f)
				[
					SNew(SEditableTextBox)
						.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.ToolTipText(LOCTEXT(
							"HoudiniEnginePDGBakeFolderTooltip",
							"The folder used to store the objects that are generated by this Houdini PDG Asset when baking, if the "
							"unreal_bake_folder attribute is not set on the geometry. If this value is blank, the default from the "
							"plugin settings is used."))
						.HintText(LOCTEXT("HoudiniEngineBakeFolderHintText", "Input to set bake folder"))
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([PDGAssetLink]() { return FText::FromString(PDGAssetLink->BakeFolder.Path); })
						.OnTextCommitted_Lambda(OnBakeFolderTextCommittedLambda)
				]
		];
}

// Helper to check if the asset link state is Linked
bool FHoudiniPDGDetails::IsPDGLinked(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
{
	if(FHoudiniEngine::Get().GetSession() == nullptr)
		return false;

	if(!IsValidWeakPointer(InPDGAssetLink))
		return false;

	if (FHoudiniEngineCommands::IsPDGCommandletEnabled())
	{
		if(FHoudiniEngine::Get().GetPDGCommandletStatus() == EHoudiniBGEOCommandletStatus::Running)
			return false;
	}

	if(InPDGAssetLink->LinkState == EPDGLinkState::Linked)
		return true;

	return false;
}

void FHoudiniPDGDetails::BindEnablePDGWiddgetsTest(FDetailWidgetRow& InRow, const TWeakObjectPtr<UHoudiniCookable>& InCookable)
{
	InRow.IsEnabledAttr.Bind(
		TAttribute<bool>::FGetter::CreateLambda([InCookable]()
			{
				if(!IsValidWeakPointer(InCookable))
					return false;

				if(!IsValid(InCookable->GetPDGAssetLink()))
					return false;

				if(!IsPDGLinked(InCookable->GetPDGAssetLink()))
					return false;

				if(IsSOPCooking(InCookable))
					return false;

				return true;
			})
	);
}

void FHoudiniPDGDetails::AddBakeAdditionalSettingsWidgets(IDetailGroup& InBakeGroup, const TWeakObjectPtr<UHoudiniCookable>& InHC)
{
	if(!IsValidWeakPointer(InHC))
		return;

	TWeakObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink = InHC->GetPDGAssetLink();

	// Add additional bake options
	FDetailWidgetRow& RenderCenterBakedActorsRow = InBakeGroup.AddWidgetRow();

	RenderCenterBakedActorsRow.NameWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("HoudiniEngineUIRecenterBakedActorsCheckBox", "Recenter Baked Actors"))
				.ToolTipText(LOCTEXT("HoudiniEngineUIRecenterBakedActorsCheckBoxToolTip", "After baking recenter the baked actors to their bounding box center."))
				.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
		];

	RenderCenterBakedActorsRow.ValueWidget.Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
				.IsChecked_Lambda([PDGAssetLink]()
					{
						if(!IsValidWeakPointer(PDGAssetLink))
							return ECheckBoxState::Unchecked;

						return PDGAssetLink->bRecenterBakedActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
				.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
					{
						if(!IsValidWeakPointer(PDGAssetLink))
							return;

						const bool bNewState = (NewState == ECheckBoxState::Checked);

						// Record a transaction for undo/redo
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_RUNTIME),
							LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
							PDGAssetLink.Get());

						PDGAssetLink->Modify();
						PDGAssetLink->bRecenterBakedActors = bNewState;

						// Notify that we have changed the property
						FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
							GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bRecenterBakedActors), PDGAssetLink.Get());
					})
		];

	BindEnablePDGWiddgetsTest(RenderCenterBakedActorsRow, InHC);

	if(!InHC->GetIsPCG())
	{
		FDetailWidgetRow& AutoBakeRow = InBakeGroup.AddWidgetRow();

		AutoBakeRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("HoudiniEngineUIAutoBakeCheckBox", "Auto Bake"))
					.ToolTipText(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxToolTip", "Automatically bake work result objects as they are loaded."))
					.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
			];

		AutoBakeRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SCheckBox)
					.IsChecked_Lambda([PDGAssetLink]()
						{
							if(!IsValidWeakPointer(PDGAssetLink))
								return ECheckBoxState::Unchecked;

							return PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
					.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
						{
							const bool bNewState = (NewState == ECheckBoxState::Checked);

							if(!IsValidWeakPointer(PDGAssetLink))
								return;

							// Record a transaction for undo/redo
							FScopedTransaction Transaction(
								TEXT(HOUDINI_MODULE_RUNTIME),
								LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
								PDGAssetLink.Get());

							PDGAssetLink->Modify();
							PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded = bNewState;

							// Notify that we have changed the property
							FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
								GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeAfterAllWorkResultObjectsLoaded), PDGAssetLink.Get());
						})
			];


		BindEnablePDGWiddgetsTest(AutoBakeRow, InHC);

		FDetailWidgetRow& AutoBakeIfFailedRow = InBakeGroup.AddWidgetRow();

		AutoBakeIfFailedRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxWithFailedWorkItems", "Auto Bake With Failed Work Items"))
					.ToolTipText(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxWithFailedWorkItemsToolTip", "Automatically bake work result objects as they are loaded even for nodes with failed work items."))
					.Font(_GetEditorStyle().GetFontStyle(HOUDINI_PDG_DETAILS_FONT))
			];

		AutoBakeIfFailedRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SCheckBox)
					.IsEnabled_Lambda([PDGAssetLink]()
						{
							if(!IsValidWeakPointer(PDGAssetLink))
								return false;
							return PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded;
						})
					.IsChecked_Lambda([PDGAssetLink]()
						{
							if(!IsValidWeakPointer(PDGAssetLink))
								return ECheckBoxState::Unchecked;

							return PDGAssetLink->IsAutoBakeNodesWithFailedWorkItemsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
					.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
						{
							const bool bNewState = (NewState == ECheckBoxState::Checked);

							if(!IsValidWeakPointer(PDGAssetLink))
								return;

							// Record a transaction for undo/redo
							FScopedTransaction Transaction(
								TEXT(HOUDINI_MODULE_RUNTIME),
								LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
								PDGAssetLink.Get());

							PDGAssetLink->Modify();
							PDGAssetLink->SetAutoBakeNodesWithFailedWorkItemsEnabled(bNewState);

							// Notify that we have changed the property
							FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
								UHoudiniPDGAssetLink::GetbAutoBakeNodesWithFailedWorkItemsPropertyName(), PDGAssetLink.Get());
						})
			];

		BindEnablePDGWiddgetsTest(AutoBakeIfFailedRow, InHC);
	}

#if 0
	// OLD LAYOUT

	// Add additional bake options
	FDetailWidgetRow& AdditionalBakeSettingsRow = InBakeGroup.AddWidgetRow();
	TSharedRef<SHorizontalBox> AdditionalBakeSettingsRowHorizontalBox = SNew(SHorizontalBox);

	TSharedPtr<SCheckBox> CheckBoxAutoBake;
	TSharedPtr<SCheckBox> CheckBoxAutoBakeWithFailedWorkItems;
	TSharedPtr<SCheckBox> CheckBoxRecenterBakedActors;

	TSharedPtr<SVerticalBox> LeftColumnVerticalBox;
	TSharedPtr<SVerticalBox> RightColumnVerticalBox;

	AdditionalBakeSettingsRowHorizontalBox->AddSlot()
		.Padding(30.0f, 5.0f, 0.0f, 0.0f)
		.MaxWidth(200.f)
		[
			SNew(SBox)
				.WidthOverride(200.f)
				[
					SAssignNew(LeftColumnVerticalBox, SVerticalBox)
				]
		];

	AdditionalBakeSettingsRowHorizontalBox->AddSlot()
		.Padding(20.0f, 5.0f, 0.0f, 0.0f)
		.MaxWidth(200.f)
		[
			SNew(SBox)
				[
					SAssignNew(RightColumnVerticalBox, SVerticalBox)
				]
		];

	LeftColumnVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 3.5f)
		[
			SNew(SBox)
				.WidthOverride(160.f)
				[
					SAssignNew(CheckBoxRecenterBakedActors, SCheckBox)
						.Content()
						[
							SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIRecenterBakedActorsCheckBox", "Recenter Baked Actors"))
								.ToolTipText(LOCTEXT("HoudiniEngineUIRecenterBakedActorsCheckBoxToolTip", "After baking recenter the baked actors to their bounding box center."))
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
							.IsChecked_Lambda([PDGAssetLink]()
								{
									if(!IsValidWeakPointer(PDGAssetLink))
										return ECheckBoxState::Unchecked;

									return PDGAssetLink->bRecenterBakedActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
								})
						.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
							{
								if(!IsValidWeakPointer(PDGAssetLink))
									return;

								const bool bNewState = (NewState == ECheckBoxState::Checked);

								// Record a transaction for undo/redo
								FScopedTransaction Transaction(
									TEXT(HOUDINI_MODULE_RUNTIME),
									LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
									PDGAssetLink.Get());

								PDGAssetLink->Modify();
								PDGAssetLink->bRecenterBakedActors = bNewState;

								// Notify that we have changed the property
								FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
									GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bRecenterBakedActors), PDGAssetLink.Get());
							})
				]
		];

	if(!InHC->GetIsPCG())
	{
		RightColumnVerticalBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.5f)
			[
				SNew(SBox)
					.WidthOverride(160.f)
					[
						SAssignNew(CheckBoxAutoBake, SCheckBox)
							.Content()
							[
								SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIAutoBakeCheckBox", "Auto Bake"))
									.ToolTipText(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxToolTip", "Automatically bake work result objects as they are loaded."))
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]
							.IsChecked_Lambda([PDGAssetLink]()
								{
									if(!IsValidWeakPointer(PDGAssetLink))
										return ECheckBoxState::Unchecked;

									return PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
								})
							.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
								{
									const bool bNewState = (NewState == ECheckBoxState::Checked);

									if(!IsValidWeakPointer(PDGAssetLink))
										return;

									// Record a transaction for undo/redo
									FScopedTransaction Transaction(
										TEXT(HOUDINI_MODULE_RUNTIME),
										LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
										PDGAssetLink.Get());

									PDGAssetLink->Modify();
									PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded = bNewState;

									// Notify that we have changed the property
									FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
										GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeAfterAllWorkResultObjectsLoaded), PDGAssetLink.Get());
								})
					]
			];

		RightColumnVerticalBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.5f)
			[
				SNew(SBox)
					.WidthOverride(160.f)
					[
						SAssignNew(CheckBoxAutoBakeWithFailedWorkItems, SCheckBox)
							.Content()
							[
								SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxWithFailedWorkItems", "Auto Bake With Failed Work Items"))
									.ToolTipText(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxWithFailedWorkItemsToolTip", "Automatically bake work result objects as they are loaded even for nodes with failed work items."))
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]
							.IsEnabled_Lambda([PDGAssetLink]()
								{
									if(!IsValidWeakPointer(PDGAssetLink))
										return false;
									return PDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded;
								})
							.IsChecked_Lambda([PDGAssetLink]()
								{
									if(!IsValidWeakPointer(PDGAssetLink))
										return ECheckBoxState::Unchecked;

									return PDGAssetLink->IsAutoBakeNodesWithFailedWorkItemsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
								})
							.OnCheckStateChanged_Lambda([PDGAssetLink](ECheckBoxState NewState)
								{
									const bool bNewState = (NewState == ECheckBoxState::Checked);

									if(!IsValidWeakPointer(PDGAssetLink))
										return;

									// Record a transaction for undo/redo
									FScopedTransaction Transaction(
										TEXT(HOUDINI_MODULE_RUNTIME),
										LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
										PDGAssetLink.Get());

									PDGAssetLink->Modify();
									PDGAssetLink->SetAutoBakeNodesWithFailedWorkItemsEnabled(bNewState);

									// Notify that we have changed the property
									FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
										UHoudiniPDGAssetLink::GetbAutoBakeNodesWithFailedWorkItemsPropertyName(), PDGAssetLink.Get());
								})
					]
			];
	}

	AdditionalBakeSettingsRow.WholeRowWidget.Widget = AdditionalBakeSettingsRowHorizontalBox;
#endif
}

FTextAndTooltip::FTextAndTooltip(int32 InValue, const FString& InText)
	: Text(InText)
	, Value(InValue)
{
}

FTextAndTooltip::FTextAndTooltip(int32 InValue, const FString& InText, const FString &InToolTip)
	: Text(InText)
	, ToolTip(InToolTip)
	, Value(InValue)
{
}

FTextAndTooltip::FTextAndTooltip(int32 InValue, FString&& InText)
	: Text(InText)
	, Value(InValue)
{
}

FTextAndTooltip::FTextAndTooltip(int32 InValue, FString&& InText, FString&& InToolTip)
	: Text(InText)
	, ToolTip(InToolTip)
	, Value(InValue)
{
}

#undef LOCTEXT_NAMESPACE
