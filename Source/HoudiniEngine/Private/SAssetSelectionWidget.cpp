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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"


SAssetSelectionWidget::SAssetSelectionWidget()
	: SelectedAssetName(-1)
	, bIsValidWidget(false)
	, bIsCancelled(false)
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

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	this->ChildSlot
		[
			SNew(SBorder)
#if ENGINE_MINOR_VERSION < 1
			.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
#else
			.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Background")))
#endif
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SAssignNew(VerticalBox, SVerticalBox)
				]
			]
		];

	for (int32 AssetNameIdx = 0, AssetNameNum = AvailableAssetNames.Num(); AssetNameIdx < AssetNameNum; ++AssetNameIdx)
	{
		FString AssetNameString = TEXT("");
		HAPI_StringHandle AssetName = AvailableAssetNames[AssetNameIdx];

		FHoudiniEngineString HoudiniEngineString(AssetName);
		if (HoudiniEngineString.ToFString(AssetNameString))
		{
			bIsValidWidget = true;
			FText AssetNameStringText = FText::FromString(AssetNameString);

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
						.Text(AssetNameStringText)
						.ToolTipText(AssetNameStringText)
					]
				];
		}
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

#endif