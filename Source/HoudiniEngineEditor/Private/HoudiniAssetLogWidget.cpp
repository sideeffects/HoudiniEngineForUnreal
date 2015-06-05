/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetLogWidget.h"


SHoudiniAssetLogWidget::SHoudiniAssetLogWidget()
{

}


void
SHoudiniAssetLogWidget::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;
	LogText = InArgs._LogText;

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
		.Content()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			[
				SNew(SRichTextBlock)
				.Text(this, &SHoudiniAssetLogWidget::GetLogText)
				.AutoWrapText(true)
			]

			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("HoudiniAssetLogWidget_CopyToClipboard", "Copy"))
					.OnClicked(this, &SHoudiniAssetLogWidget::OnButtonCopyToClipboard)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("HoudiniAssetLogWidget_OK", "Accept"))
					.OnClicked(this, &SHoudiniAssetLogWidget::OnButtonOk)
				]
			]
		]
	];
}


FReply
SHoudiniAssetLogWidget::OnButtonOk()
{
	WidgetWindow->HideWindow();
	WidgetWindow->RequestDestroyWindow();

	return FReply::Handled();
}


FText
SHoudiniAssetLogWidget::GetLogText() const
{
	return FText::FromString(LogText);
}


FReply
SHoudiniAssetLogWidget::OnButtonCopyToClipboard()
{
	FPlatformMisc::ClipboardCopy(*LogText);
	return FReply::Handled();
}
