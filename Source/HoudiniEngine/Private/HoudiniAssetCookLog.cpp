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

#include "HoudiniEnginePrivatePCH.h"


SHoudiniAssetCookLog::SHoudiniAssetCookLog()
{

}


void
SHoudiniAssetCookLog::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;
	CookLogText = InArgs._CookLogText;

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
				.Text(this, &SHoudiniAssetCookLog::GetCookLogText)
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
					.Text(LOCTEXT("HoudiniAssetCookLog_CopyToClipboard", "Copy"))
					.OnClicked(this, &SHoudiniAssetCookLog::OnButtonCopyToClipboard)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("HoudiniAssetCookLog_OK", "Accept"))
					.OnClicked(this, &SHoudiniAssetCookLog::OnButtonOk)
				]
			]
		]
	];
}


FReply
SHoudiniAssetCookLog::OnButtonOk()
{
	WidgetWindow->HideWindow();
	WidgetWindow->RequestDestroyWindow();

	return FReply::Handled();
}


FText
SHoudiniAssetCookLog::GetCookLogText() const
{
	return FText::FromString(CookLogText);
}


FReply
SHoudiniAssetCookLog::OnButtonCopyToClipboard()
{
	FPlatformMisc::ClipboardCopy(*CookLogText);
	return FReply::Handled();
}
