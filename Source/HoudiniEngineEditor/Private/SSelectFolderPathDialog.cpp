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

#include "SSelectFolderPathDialog.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h" 
#include "HoudiniEngineEditorPrivatePCH.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

SSelectFolderPathDialog::SSelectFolderPathDialog()
	: UserResponse(EAppReturnType::Cancel)
{

}

void
SSelectFolderPathDialog::Construct(const FArguments& InArgs)
{
	FolderPath = InArgs._InitialPath;

	if (FolderPath.IsEmpty())
	{
		FolderPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = FolderPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SSelectFolderPathDialog::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;
	
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SWindow::Construct(SWindow::FArguments()
	.Title(InArgs._TitleText)
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	.IsTopmostWindow(true)
	.ClientSize(FVector2D(450, 450))
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(_GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SelectPath", "Select Path"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				]

				+ SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(3)
				[
					ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(5)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(_GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(_GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(_GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(_GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("OK", "OK"))
				.OnClicked(this, &SSelectFolderPathDialog::OnButtonClick, EAppReturnType::Ok)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(_GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SSelectFolderPathDialog::OnButtonClick, EAppReturnType::Cancel)
			]
		]
	]);
}

EAppReturnType::Type
SSelectFolderPathDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

const FText&
SSelectFolderPathDialog::GetFolderPath() const
{
	return FolderPath;
}

void
SSelectFolderPathDialog::OnPathChange(const FString& NewPath)
{
	FolderPath = FText::FromString(NewPath);
}

FReply
SSelectFolderPathDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE