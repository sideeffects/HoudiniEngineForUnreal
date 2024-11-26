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

#include "SAssetSelectionWidget.h"

#include "HoudiniEngineString.h"

#if WITH_EDITOR

#include "../../Launch/Resources/Version.h"

#include "EditorStyleSet.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

SAssetSelectionWidget::SAssetSelectionWidget()
	: SelectedAssetName(-1)
	, bIsValidWidget(false)
	, bIsCancelled(false)
	, bHideNameSpace(true)
	, bHideVersion(false)
	, bHideNodeManager(true)
{}

bool
SAssetSelectionWidget::IsCancelled() const
{
	return bIsCancelled;
}

bool
SAssetSelectionWidget::IsValidWidget() const
{
	return bIsValidWidget;
}

int32
SAssetSelectionWidget::GetSelectedAssetName() const
{
	return SelectedAssetName;
}

void
SAssetSelectionWidget::Construct(const FArguments & InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;
	AvailableAssetNames = InArgs._AvailableAssetNames;

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	this->ChildSlot
	[
		SNew(SBorder)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Background")))
#else
		.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))			
#endif
		.Content()
		[
			SNew(SScrollBox)
			.Orientation(Orient_Vertical)
			+ SScrollBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0)
				.VAlign(VAlign_Center)
				[
					SAssignNew(VerticalBox, SVerticalBox)
				]
			]
		]
	];



	// Add Checkboxes for the display options:
	// Hide Name Space
	VerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.IsChecked_Lambda([this]() { return bHideNameSpace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) {bHideNameSpace = NewState == ECheckBoxState::Checked; })
		.ToolTipText(LOCTEXT("HideNamespaceTooltipText", "Hide the Asset's namespace: NAMESPACE::NODE_TYPE/ASSET_NAME::VERSION"))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HideNamespaceText", "Hide namespace"))
		]
	];

	// Hide NodeManager
	VerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.IsChecked_Lambda([this]() { return bHideNodeManager ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) {bHideNodeManager = NewState == ECheckBoxState::Checked; })
		.ToolTipText(LOCTEXT("bHideNodeManagerTooltipText", "Hide the Asset's node type: NAMESPACE::NODE_TYPE/ASSET_NAME::VERSION"))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("bHideNodeManagerText", "Hide node type"))
		]
	];

	// Hide Version
	VerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	[
		SNew(SCheckBox)
		.IsChecked_Lambda([this]() { return bHideVersion ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) {bHideVersion = NewState == ECheckBoxState::Checked; })
		.ToolTipText(LOCTEXT("HideVersionTooltipText", "Hide the Asset's version: NAMESPACE::NODE_TYPE/ASSET_NAME::VERSION"))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HideVersionText", "Hide version"))
		]
	];

	// Lambda for getting the asset's display name that matches the chosen options
	auto GetShortNameToDisplay = [this](const FString& AssetNameString)
	{
		bool bDisplayShortNames = bHideNameSpace || bHideVersion || bHideNodeManager;
		if (!bDisplayShortNames)
			return FText::FromString(AssetNameString);

		// Extract the asset name from XX::YY/ASSET_NAME::ZZ
		FString ShortAssetNameString = AssetNameString;

		int32 FirstSepIdx = ShortAssetNameString.Find("::");
		int32 LastSepIdx = ShortAssetNameString.Find("::", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (FirstSepIdx >= 0 && LastSepIdx >= 0 && FirstSepIdx != LastSepIdx)
		{
			// We have two sets of :: separators - procede with the chopping
			if (bHideNameSpace)
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
				ShortAssetNameString.RightChopInline(FirstSepIdx + 2, EAllowShrinking::Yes);
#else
				ShortAssetNameString.RightChopInline(FirstSepIdx + 2, true);
#endif
				LastSepIdx -= FirstSepIdx + 2;
			}

			//
			if (bHideVersion && LastSepIdx >= 0)
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
				ShortAssetNameString.LeftInline(LastSepIdx, EAllowShrinking::Yes);
#else
				ShortAssetNameString.LeftInline(LastSepIdx, true);
#endif
			}
		}
		else if(FirstSepIdx >= 0 && (bHideNameSpace || bHideVersion))
		{
			// If we only have one separator
			// Try to see if we can find a node type in one of the two parts
			// as this will let us know where the node name is
			// If unsure - don't touch the short string			
			FString Left = ShortAssetNameString.Left(LastSepIdx);
			FString Right = ShortAssetNameString.RightChop(FirstSepIdx + 2);

			if (bHideVersion && Left.Find("/") >= 0)
			{
				ShortAssetNameString = Left;
			}
			else if (bHideNameSpace && Right.Find("/") >= 0)
			{
				ShortAssetNameString = Right;
			}
		}

		// Now remove the node type identifier YY if any
		if (bHideNodeManager)
		{
			FirstSepIdx = ShortAssetNameString.Find("::");
			int32 ManagerSepIdx = ShortAssetNameString.Find("/");

			if (ManagerSepIdx >= 0)
			{
				if (FirstSepIdx >= 0 && FirstSepIdx < ManagerSepIdx && !bHideNameSpace)
				{
					ShortAssetNameString =
						ShortAssetNameString.Left(FirstSepIdx + 2)
						+ ShortAssetNameString.RightChop(ManagerSepIdx + 1);
				}
				else
				{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
					ShortAssetNameString.RightChopInline(ManagerSepIdx + 1, EAllowShrinking::Yes);
#else
					ShortAssetNameString.RightChopInline(ManagerSepIdx + 1, true);
#endif
				}
			}
		}

		// Make sure we still have somnething to display
		if (ShortAssetNameString.Len() <= 0)
			ShortAssetNameString = AssetNameString;

		return FText::FromString(ShortAssetNameString);
	};

	for (int32 AssetNameIdx = 0, AssetNameNum = AvailableAssetNames.Num(); AssetNameIdx < AssetNameNum; ++AssetNameIdx)
	{
		FString AssetNameString = TEXT("");
		HAPI_StringHandle AssetName = AvailableAssetNames[AssetNameIdx];

		FHoudiniEngineString HoudiniEngineString(AssetName);
		if (!HoudiniEngineString.ToFString(AssetNameString))
			continue;

		bIsValidWidget = true;
		FText AssetNameText = FText::FromString(AssetNameString);	

		VerticalBox->AddSlot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 4.0f)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &SAssetSelectionWidget::OnButtonAssetPick, AssetName)
				.Text_Lambda([this, GetShortNameToDisplay, AssetNameString]()
				{
					return GetShortNameToDisplay(AssetNameString);
				})
				.ToolTipText(AssetNameText)
			]
		];
	}
}

FReply
SAssetSelectionWidget::OnButtonAssetPick(int32 AssetName)
{
	SelectedAssetName = AssetName;

	if (TSharedPtr<SWindow> WindowPtr = WidgetWindow.Pin())
	{
		WindowPtr->HideWindow();
		WindowPtr->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply
SAssetSelectionWidget::OnButtonOk()
{
	if (TSharedPtr<SWindow> WindowPtr = WidgetWindow.Pin())
	{
		WindowPtr->HideWindow();
		WindowPtr->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply
SAssetSelectionWidget::OnButtonCancel()
{
	bIsCancelled = true;

	if (TSharedPtr<SWindow> WindowPtr = WidgetWindow.Pin())
	{
		WindowPtr->HideWindow();
		WindowPtr->RequestDestroyWindow();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

#endif
