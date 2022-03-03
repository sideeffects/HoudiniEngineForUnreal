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

#include "HoudiniEngineDetails.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"

#include "CoreMinimal.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "DetailWidgetRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Brushes/SlateImageBrush.h"
#include "Widgets/Input/SComboBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ActorPickerMode.h"
#include "SceneOutlinerModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "AssetThumbnail.h"
#include "DetailLayoutBuilder.h"
#include "SAssetDropTarget.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SEnumCombo.h"
#include "HAL/FileManager.h"
#include "ActorTreeItem.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define HOUDINI_ENGINE_UI_SECTION_GENERATE												1
#define HOUDINI_ENGINE_UI_SECTION_BAKE													2
#define HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS											3
#define HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG										4	

#define HOUDINI_ENGINE_UI_BUTTON_WIDTH											   135.0f

#define HOUDINI_ENGINE_UI_SECTION_GENERATE_HEADER_TEXT							   "Generate"
#define HOUDINI_ENGINE_UI_SECTION_BAKE_HEADER_TEXT							       "Bake"
#define HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS_HEADER_TEXT						   "Asset Options"
#define HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG_HEADER_TEXT					   "Help and Debug"


void
SHoudiniAssetLogWidget::Construct(const FArguments & InArgs)
{
	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
		.Content()
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SMultiLineEditableTextBox)
				.Text(FText::FromString(InArgs._LogText))
				.AutoWrapText(true)
				.IsReadOnly(true)
			]
		]
	];
}


void 
FHoudiniEngineDetails::CreateWidget(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];

	if (!IsValidWeakPointer(MainHAC))
		return;

	// 0. Houdini Engine Icon
	FHoudiniEngineDetails::CreateHoudiniEngineIconWidget(HoudiniEngineCategoryBuilder, InHACs);
	
	// 1. Houdini Engine Session Status
	FHoudiniAssetComponentDetails::AddSessionStatusRow(HoudiniEngineCategoryBuilder);
	
	// 2. Create Generate Category
	FHoudiniEngineDetails::CreateGenerateWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
	// 3. Create Bake Category
	FHoudiniEngineDetails::CreateBakeWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
	// 4. Create Asset Options Category
	FHoudiniEngineDetails::CreateAssetOptionsWidgets(HoudiniEngineCategoryBuilder, InHACs);

	// 5. Create Help and Debug Category
	FHoudiniEngineDetails::CreateHelpAndDebugWidgets(HoudiniEngineCategoryBuilder, InHACs);
}

void 
FHoudiniEngineDetails::CreateHoudiniEngineIconWidget(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];

	if (!IsValidWeakPointer(MainHAC))
		return;

	// Skip drawing the icon if the icon image is not loaded correctly.
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();
	if (!HoudiniEngineUIIconBrush.IsValid())
		return;

	FDetailWidgetRow & Row = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	TSharedPtr<SImage> Image;
	
	Box->AddSlot()
	.AutoWidth()
	.Padding(0.0f, 5.0f, 5.0f, 10.0f)
	.HAlign(HAlign_Left)
	[
		SNew(SBox)
		.HeightOverride(30)
		.WidthOverride(208)
		[
			SAssignNew(Image, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
	
	Image->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIIconBrush]() {
		return HoudiniEngineUIIconBrush.Get();
	})));

	Row.WholeRowWidget.Widget = Box;
	Row.IsEnabled(false);
}

void 
FHoudiniEngineDetails::CreateGenerateWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs)
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];

	if (!IsValidWeakPointer(MainHAC))
		return;

	auto OnReBuildClickedLambda = [InHACs]()
	{
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->MarkAsNeedRebuild();
		}

		return FReply::Handled();
	};

	auto OnRecookClickedLambda = [InHACs]()
	{
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->MarkAsNeedCook();
		}

		return FReply::Handled();
	};

	auto ShouldEnableResetParametersButtonLambda = [InHACs]() 
	{
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			// Reset parameters to default values?
			for (int32 n = 0; n < NextHAC->GetNumParameters(); ++n)
			{
				UHoudiniParameter* NextParm = NextHAC->GetParameterAt(n);

				if (IsValid(NextParm) && !NextParm->IsDefault())
					return true;
			}
		}

		return false;
	};

	auto OnResetParametersClickedLambda = [InHACs]()
	{
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			// Reset parameters to default values?
			for (int32 n = 0; n < NextHAC->GetNumParameters(); ++n)
			{
				UHoudiniParameter* NextParm = NextHAC->GetParameterAt(n);

				if (IsValid(NextParm) && !NextParm->IsDefault())
				{
					NextParm->RevertToDefault();
				}
			}
		}

		return FReply::Handled();
	};

	auto OnCookFolderTextCommittedLambda = [InHACs, MainHAC](const FText& Val, ETextCommit::Type TextCommitType)
	{
		if (!IsValidWeakPointer(MainHAC))
			return;

		FString NewPathStr = Val.ToString();
		
		if (NewPathStr.StartsWith("Game/")) 
		{
			NewPathStr = "/" + NewPathStr;
		}

		{
			FText InvalidPathReason;
			if (!FHoudiniEngineUtils::ValidatePath(NewPathStr, &InvalidPathReason)) 
			{
				HOUDINI_LOG_WARNING(TEXT("Invalid path: %s"), *InvalidPathReason.ToString());

				FHoudiniEngineUtils::UpdateEditorProperties(MainHAC.Get(), true);
				return;
			}
		}

		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			if (NextHAC->TemporaryCookFolder.Path.Equals(NewPathStr))
				continue;

			NextHAC->TemporaryCookFolder.Path = NewPathStr;
		}
	};

	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_GENERATE);
	
	// Button Row (draw only if expanded)
	if (!MainHAC->bGenerateMenuExpanded) 
		return;
	
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIRebuildIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIRebuildIconBrush();
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIRecookIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIRecookIconBrush();
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIResetParametersIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIResetParametersIconBrush();

	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> ButtonHorizontalBox = SNew(SHorizontalBox);

	// Recook button
	TSharedPtr<SButton> RecookButton;
	TSharedPtr<SHorizontalBox> RecookButtonHorizontalBox;
	ButtonHorizontalBox->AddSlot()
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	//.Padding(2.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(RecookButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsRecookAssetButton", "Recook the selected Houdini Asset: all parameters and inputs are re-upload to Houdini and the asset is then forced to recook."))
			//.Text(FText::FromString("Recook"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnRecookClickedLambda)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SAssignNew(RecookButtonHorizontalBox, SHorizontalBox)
				]
			]
		]
	];

	if (HoudiniEngineUIRecookIconBrush.IsValid())
	{
		TSharedPtr<SImage> RecookImage;
		RecookButtonHorizontalBox->AddSlot()
		.MaxWidth(16.0f)
		//.Padding(23.0f, 0.0f, 3.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			.HeightOverride(16.0f)
			[
				SAssignNew(RecookImage, SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		RecookImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIRecookIconBrush]() {
			return HoudiniEngineUIRecookIconBrush.Get();
		})));
	}

	RecookButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	//.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Recook"))
	];

	// Rebuild button
	TSharedPtr<SButton> RebuildButton;
	TSharedPtr<SHorizontalBox> RebuildButtonHorizontalBox;
	ButtonHorizontalBox->AddSlot()
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	//.Padding(15.0f, 0.0f, 0.0f, 2.0f)
	//.Padding(2.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(RebuildButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsRebuildAssetButton", "Rebuild the selected Houdini Asset: its source .HDA file is reimported and updated, the asset's nodes in Houdini are destroyed and recreated, and the asset is then forced to recook."))
			//.Text(FText::FromString("Rebuild"))
			.Visibility(EVisibility::Visible)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SAssignNew(RebuildButtonHorizontalBox, SHorizontalBox)
				]
			]
			.OnClicked_Lambda(OnReBuildClickedLambda)
		]
	];

	if (HoudiniEngineUIRebuildIconBrush.IsValid())
	{
		TSharedPtr<SImage> RebuildImage;
		RebuildButtonHorizontalBox->AddSlot()
		//.Padding(25.0f, 0.0f, 3.0f, 0.0f)
		//.Padding(2.0f, 0.0f, 0.0f, 2.0f)
		.MaxWidth(16.0f)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			.HeightOverride(16.0f)
			[
				SAssignNew(RebuildImage, SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		RebuildImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIRebuildIconBrush]() {
			return HoudiniEngineUIRebuildIconBrush.Get();
		})));
	}

	RebuildButtonHorizontalBox->AddSlot()
	.VAlign(VAlign_Center)
	.AutoWidth()
	.Padding(5.0, 0.0, 0.0, 0.0)
	[
		SNew(STextBlock)
		.Text(FText::FromString("Rebuild"))
	];
	
	ButtonRow.WholeRowWidget.Widget = ButtonHorizontalBox;
	ButtonRow.IsEnabled(true);

	// Reset Parameters button
	TSharedPtr<SButton> ResetParametersButton;
	TSharedPtr<SHorizontalBox> ResetParametersButtonHorizontalBox;
	ButtonHorizontalBox->AddSlot()
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	//.Padding(2.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(ResetParametersButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsResetParametersAssetButton", "Reset the selected Houdini Asset's parameters to their default values."))
			//.Text(FText::FromString("Reset Parameters"))
			.IsEnabled_Lambda(ShouldEnableResetParametersButtonLambda)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnResetParametersClickedLambda)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SAssignNew(ResetParametersButtonHorizontalBox, SHorizontalBox)
				]
			]
		]
	];

	if (HoudiniEngineUIResetParametersIconBrush.IsValid())
	{
		TSharedPtr<SImage> ResetParametersImage;
		ResetParametersButtonHorizontalBox->AddSlot()
		.MaxWidth(16.0f)
		//.Padding(0.0f, 0.0f, 3.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			.HeightOverride(16.0f)
			[
				SAssignNew(ResetParametersImage, SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		ResetParametersImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIResetParametersIconBrush]() {
			return HoudiniEngineUIResetParametersIconBrush.Get();
		})));
	}

	ResetParametersButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	//.FillWidth(4.2f)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(STextBlock)
		//.MinDesiredWidth(160.f)
		.Text(FText::FromString("Reset Parameters"))		
	];


	// Temp Cook Folder Row
	FDetailWidgetRow & TempCookFolderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());

	TSharedRef<SHorizontalBox> TempCookFolderRowHorizontalBox = SNew(SHorizontalBox);

	TempCookFolderRowHorizontalBox->AddSlot()
	//.Padding(30.0f, 0.0f, 6.0f, 0.0f)
	.MaxWidth(155.0f)
	[
		SNew(SBox)
		.WidthOverride(155.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineTemporaryCookFolderLabel", "Temporary Cook Folder"))
			.ToolTipText(LOCTEXT(
				"HoudiniEngineTemporaryCookFolderTooltip",
				"Default folder used to store the temporary files (Static Meshes, Materials, Textures..) that are "
				"generated by Houdini Assets when they cook. If this value is blank, the default from the plugin "
				"settings is used."))
		]
	];

	TempCookFolderRowHorizontalBox->AddSlot()
	.MaxWidth(235.0f)
	[
		SNew(SBox)
		.WidthOverride(235.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT(
				"HoudiniEngineTemporaryCookFolderTooltip",
				"Default folder used to store the temporary files (Static Meshes, Materials, Textures..) that are "
				"generated by Houdini Assets when they cook. If this value is blank, the default from the plugin "
				"settings is used."))
			.HintText(LOCTEXT("HoudiniEngineTempCookFolderHintText", "Input to set temporary cook folder"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(FText::FromString(MainHAC->TemporaryCookFolder.Path))
			.OnTextCommitted_Lambda(OnCookFolderTextCommittedLambda)
		]
	];
	
	TempCookFolderRow.WholeRowWidget.Widget = TempCookFolderRowHorizontalBox;
}

void
FHoudiniEngineDetails::OnBakeAfterCookChangedHelper(bool bInState, UHoudiniAssetComponent* InHAC)
{
	if (!IsValid(InHAC))
		return;
	
	if (!bInState)
	{
		if (InHAC->GetOnPostCookBakeDelegate().IsBound())
			InHAC->GetOnPostCookBakeDelegate().Unbind();
	}
	else
	{
		InHAC->GetOnPostCookBakeDelegate().BindLambda([](UHoudiniAssetComponent* HAC)
		{
			return FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
				HAC,
				HAC->bReplacePreviousBake,
				HAC->HoudiniEngineBakeOption,
				HAC->bRemoveOutputAfterBake,
				HAC->bRecenterBakedActors);
		});
	}	
}

void 
FHoudiniEngineDetails::CreateBakeWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs)
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];
	if (!IsValidWeakPointer(MainHAC))
		return;

	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_BAKE);

	if (!MainHAC->bBakeMenuExpanded)
		return;

	auto OnBakeButtonClickedLambda = [InHACs, MainHAC]() 
	{
		for (auto & NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
				NextHAC.Get(),
				MainHAC->bReplacePreviousBake,
				MainHAC->HoudiniEngineBakeOption,
				MainHAC->bRemoveOutputAfterBake,
				MainHAC->bRecenterBakedActors);
		}

		return FReply::Handled();	
	};

	auto OnBakeFolderTextCommittedLambda = [InHACs, MainHAC](const FText& Val, ETextCommit::Type TextCommitType)
	{
		if (!IsValidWeakPointer(MainHAC))
			return;

		FString NewPathStr = Val.ToString();

		if (NewPathStr.StartsWith("Game/"))
		{
			NewPathStr = "/" + NewPathStr;
		}

		{
			FText InvalidPathReason;
			if (!FHoudiniEngineUtils::ValidatePath(NewPathStr, &InvalidPathReason))
			{
				HOUDINI_LOG_WARNING(TEXT("Invalid path: %s"), *InvalidPathReason.ToString());

				FHoudiniEngineUtils::UpdateEditorProperties(MainHAC.Get(), true);
				return;
			}
		}

		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			if (NextHAC->BakeFolder.Path.Equals(NewPathStr))
				continue;

			NextHAC->BakeFolder.Path = NewPathStr;
		}
	};

	// Button Row
	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());

	TSharedRef<SHorizontalBox> ButtonRowHorizontalBox = SNew(SHorizontalBox);

	// Bake Button
	TSharedPtr<SButton> BakeButton;
	TSharedPtr<SHorizontalBox> BakeButtonHorizontalBox;

	ButtonRowHorizontalBox->AddSlot()
    .MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
    //.Padding(15.f, 0.0f, 0.0f, 0.0f)
	[
        SNew(SBox)
        .WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(BakeButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
            .ToolTipText(LOCTEXT("HoudiniAssetDetailsBakeButton", "Bake the Houdini Asset Component(s)."))
            //.Text(FText::FromString("Recook"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnBakeButtonClickedLambda)
            .Content()
            [
                SAssignNew(BakeButtonHorizontalBox, SHorizontalBox)
            ]
        ]
    ];

	TSharedPtr<FSlateDynamicImageBrush> BakeIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIBakeIconBrush();
	if (BakeIconBrush.IsValid())
	{
		TSharedPtr<SImage> BakeImage;
		BakeButtonHorizontalBox->AddSlot()
        .MaxWidth(16.0f)
        //.Padding(23.0f, 0.0f, 3.0f, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(16.0f)
            .HeightOverride(16.0f)
            [
                SAssignNew(BakeImage, SImage)
			]
		];

		BakeImage->SetImage(
            TAttribute<const FSlateBrush*>::Create(
                TAttribute<const FSlateBrush*>::FGetter::CreateLambda([BakeIconBrush]() {
            return BakeIconBrush.Get();
        })));
	}

	BakeButtonHorizontalBox->AddSlot()
    .Padding(5.0, 0.0, 0.0, 0.0)
    .VAlign(VAlign_Center)
    .AutoWidth()
    [
        SNew(STextBlock)
        .Text(FText::FromString("Bake"))
    ];
	
	// Bake Type ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeComboBox;

	TArray<TSharedPtr<FString>>* OptionSource = FHoudiniEngineEditor::Get().GetHoudiniEngineBakeTypeOptionsLabels();
	TSharedPtr<FString> IntialSelec;
	if (OptionSource) 
	{
		IntialSelec = (*OptionSource)[(int)MainHAC->HoudiniEngineBakeOption];
	}

	ButtonRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(3.0, 0.0, 4.0f, 0.0f)
    //.MaxWidth(103.f)
    .MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
        //.WidthOverride(103.f)
        .WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(OptionSource)
			.InitiallySelectedItem(IntialSelec)
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > InItem)
			{
				FText ChoiceEntryText = FText::FromString(*InItem);
				return SNew(STextBlock)
						.Text(ChoiceEntryText)
						.ToolTipText(ChoiceEntryText)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnSelectionChanged_Lambda(
				[MainHAC, InHACs](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				if (!NewChoice.IsValid())
					return;

				const EHoudiniEngineBakeOption NewOption = 
					FHoudiniEngineEditor::Get().StringToHoudiniEngineBakeOption(*NewChoice.Get());

				for (auto & NextHAC : InHACs) 
				{
					if (!IsValidWeakPointer(NextHAC))
						continue;

					MainHAC->HoudiniEngineBakeOption = NewOption;
				}

				if (MainHAC.IsValid())
					FHoudiniEngineUtils::UpdateEditorProperties(MainHAC.Get(), true);
			})
			[
				SNew(STextBlock)
				.Text_Lambda([MainHAC]() 
				{ 
                	if (!IsValidWeakPointer(MainHAC))
                		return FText();

					return FText::FromString(
						FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(MainHAC->HoudiniEngineBakeOption));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];
	
	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;

	// Clear Output After Baking Row
	FDetailWidgetRow & ClearOutputAfterBakingRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> AdditionalBakeSettingsRowHorizontalBox = SNew(SHorizontalBox);

	// Remove Output Checkbox
	TSharedPtr<SCheckBox> CheckBoxRemoveOutput;
	TSharedPtr<SCheckBox> CheckBoxAutoBake;
	TSharedPtr<SCheckBox> CheckBoxRecenterBakedActors;
	TSharedPtr<SCheckBox> CheckBoxReplacePreviousBake;

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
            SAssignNew(CheckBoxRemoveOutput, SCheckBox)
            .Content()
            [
                SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIRemoveOutputCheckBox", "Remove HDA Output After Bake"))
                .ToolTipText(LOCTEXT("HoudiniEngineUIRemoveOutputCheckBoxToolTip", "After baking the existing output of this Houdini Asset Actor will be removed."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            .IsChecked_Lambda([MainHAC]()
            {
            	if (!IsValidWeakPointer(MainHAC))
            		return ECheckBoxState::Unchecked;
            	
                return MainHAC->bRemoveOutputAfterBake ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([InHACs](ECheckBoxState NewState)
            {
                const bool bNewState = (NewState == ECheckBoxState::Checked);

                for (auto & NextHAC : InHACs) 
                {
                    if (!IsValidWeakPointer(NextHAC))
                        continue;

                    NextHAC->bRemoveOutputAfterBake = bNewState;
                }

                // FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
            })
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
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            .IsChecked_Lambda([MainHAC]()
            {
            	if (!IsValidWeakPointer(MainHAC))
            		return ECheckBoxState::Unchecked;
            	
                return MainHAC->bRecenterBakedActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([InHACs](ECheckBoxState NewState)
            {
                const bool bNewState = (NewState == ECheckBoxState::Checked);

                for (auto & NextHAC : InHACs) 
                {
                    if (!IsValidWeakPointer(NextHAC))
                        continue;

                    NextHAC->bRecenterBakedActors = bNewState;
                }

                // FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
            })
        ]
    ];

	// TODO: find a better way to manage the initial binding/unbinding of the post cook bake delegate
	// We do this here to ensure the delegate is bound/unbound correctly when the UI is initially drawn
	// Currently we have the problem that the HoudiniEngineRuntime and HoudiniEngine modules cannot access
	// the FHoudiniEngineBakeUtils code that is in HoudiniEngineEditor (this is the primary reason for the delegate and
	// managing the delegate in this way).
	for (auto & NextHAC : InHACs) 
	{
		if (!IsValidWeakPointer(NextHAC))
			continue;

		const bool bState = NextHAC->IsBakeAfterNextCookEnabled();
		NextHAC->SetBakeAfterNextCookEnabled(bState);
		OnBakeAfterCookChangedHelper(bState, NextHAC.Get());
	}	
	
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
                .ToolTipText(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxToolTip", "Automatically bake the next cook."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            .IsChecked_Lambda([MainHAC]()
            {
            	if (!IsValidWeakPointer(MainHAC))
            		return ECheckBoxState::Unchecked;

            	return MainHAC->IsBakeAfterNextCookEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([InHACs](ECheckBoxState NewState)
            {
                const bool bNewState = (NewState == ECheckBoxState::Checked);

                for (auto & NextHAC : InHACs) 
                {
                    if (!IsValidWeakPointer(NextHAC))
                        continue;

                    NextHAC->SetBakeAfterNextCookEnabled(bNewState);
                    OnBakeAfterCookChangedHelper(bNewState, NextHAC.Get());
                }

                // FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
            })
        ]
    ];

	// Replace Checkbox
	RightColumnVerticalBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 0.0f, 0.0f, 3.5f)
	[
		SNew(SBox)
        .WidthOverride(160.f)
		[
            SAssignNew(CheckBoxReplacePreviousBake, SCheckBox)
			.Content()
			[
                SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIBakeReplaceWithPreviousCheckBox", "Replace Previous Bake"))
                .ToolTipText(LOCTEXT("HoudiniEngineUIBakeReplaceWithPreviousCheckBoxToolTip", "When baking replace the previous bake's output instead of creating additional output actors/components/objects."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainHAC]()
			{
            	if (!IsValidWeakPointer(MainHAC))
            		return ECheckBoxState::Unchecked;

				return MainHAC->bReplacePreviousBake ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainHAC, InHACs](ECheckBoxState NewState)
			{
				const bool bNewState = (NewState == ECheckBoxState::Checked);

				for (auto & NextHAC : InHACs) 
				{
					if (!IsValidWeakPointer(NextHAC))
						continue;

					NextHAC->bReplacePreviousBake = bNewState;
				}

				if (MainHAC.IsValid())
					FHoudiniEngineUtils::UpdateEditorProperties(MainHAC.Get(), true);
			})
		]
	];

	ClearOutputAfterBakingRow.WholeRowWidget.Widget = AdditionalBakeSettingsRowHorizontalBox;

	// Bake Folder Row
	FDetailWidgetRow & BakeFolderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());

	TSharedRef<SHorizontalBox> BakeFolderRowHorizontalBox = SNew(SHorizontalBox);

	BakeFolderRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(30.0f, 0.0f, 6.0f, 0.0f)
	.MaxWidth(155.0f)
	[
		SNew(SBox)
		.WidthOverride(155.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineBakeFolderLabel", "Bake Folder"))
			.ToolTipText(LOCTEXT(
				"HoudiniEngineBakeFolderTooltip",
				"The folder used to store the objects that are generated by this Houdini Asset when baking, if the "
				"unreal_bake_folder attribute is not set on the geometry. If this value is blank, the default from the "
				"plugin settings is used."))
		]
	];

	BakeFolderRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.MaxWidth(235.0)
	[
		SNew(SBox)
		.WidthOverride(235.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT(
				"HoudiniEngineBakeFolderTooltip",
				"The folder used to store the objects that are generated by this Houdini Asset when baking, if the "
				"unreal_bake_folder attribute is not set on the geometry. If this value is blank, the default from the "
				"plugin settings is used."))
			.HintText(LOCTEXT("HoudiniEngineBakeFolderHintText", "Input to set bake folder"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([MainHAC]()
			{
				if (!IsValidWeakPointer(MainHAC))
					return FText();
				return FText::FromString(MainHAC->BakeFolder.Path);
			})
			.OnTextCommitted_Lambda(OnBakeFolderTextCommittedLambda)
		]
	];

	BakeFolderRow.WholeRowWidget.Widget = BakeFolderRowHorizontalBox;

	switch (MainHAC->HoudiniEngineBakeOption) 
	{
		case EHoudiniEngineBakeOption::ToActor:
		{
			if (MainHAC->bReplacePreviousBake) 
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeWithReplaceToActorToolTip", 
					"Bake this Houdini Asset Actor and its components to native unreal actors and components, replacing the previous baked result."));
			}
			else 
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToActorToolTip", 
					"Bake this Houdini Asset Actor and its components to native unreal actors and components."));
			}
		}
		break;

		case EHoudiniEngineBakeOption::ToBlueprint:
		{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToBlueprintToolTip",
					"Bake this Houdini Asset Actor to a blueprint."));
			}
		break;

		case EHoudiniEngineBakeOption::ToFoliage:
		{
			if (!FHoudiniEngineBakeUtils::CanHoudiniAssetComponentBakeToFoliage(MainHAC.Get()))
			{
				// If the HAC does not have instanced output, disable Bake to Foliage
				BakeButton->SetEnabled(false);
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonNoInstancedOutputToolTip",
					"The Houdini Asset must be outputing at least one instancer in order to be able to bake to Foliage."));
			}
			else 
			{
				if (MainHAC->bReplacePreviousBake)
				{
					BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeWithReplaceToFoliageToolTip",
						"Add this Houdini Asset Actor's instancers to the current level's Foliage, replacing the previously baked foliage instancers from this actor."));
				}
				else
				{
					BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToFoliageToolTip",
						"Add this Houdini Asset Actor's instancers to the current level's Foliage."));
				}
			}
		}
		break;

		case EHoudiniEngineBakeOption::ToWorldOutliner:
		{
			if (MainHAC->bReplacePreviousBake)
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeWithReplaceToWorldOutlinerToolTip",
					"Not implemented."));
			}
			else
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToWorldOutlinerToolTip",
					"Not implemented."));
			}
		}
		break;
	}

	// Todo: remove me!
	if (MainHAC->HoudiniEngineBakeOption == EHoudiniEngineBakeOption::ToWorldOutliner)
		BakeButton->SetEnabled(false);

}

void 
FHoudiniEngineDetails::CreateAssetOptionsWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];
	if (!IsValidWeakPointer(MainHAC))
		return;

	// Header Row
	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS);

	if (!MainHAC->bAssetOptionMenuExpanded)
		return;

	auto IsCheckedParameterChangedLambda = [MainHAC]()
	{
      	if (!IsValidWeakPointer(MainHAC))
      		return ECheckBoxState::Unchecked;

		return MainHAC->bCookOnParameterChange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateParameterChangedLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bCookOnParameterChange = bChecked;
		}
	};

	auto IsCheckedTransformChangeLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bCookOnTransformChange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedTransformChangeLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bCookOnTransformChange = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedAssetInputCookLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bCookOnAssetInputCook ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedAssetInputCookLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bCookOnAssetInputCook = bChecked;
		}
	};

	auto IsCheckedPushTransformToHoudiniLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bUploadTransformsToHoudiniEngine ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedPushTransformToHoudiniLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bUploadTransformsToHoudiniEngine = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedDoNotGenerateOutputsLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bOutputless ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedDoNotGenerateOutputsLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bOutputless = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedOutputTemplatedGeosLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bOutputTemplateGeos ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedOutputTemplatedGeosLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bOutputTemplateGeos = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedUseOutputNodesLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bUseOutputNodes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedUseOutputNodesLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			NextHAC->bUseOutputNodes = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	// Checkboxes row
	FDetailWidgetRow & CheckBoxesRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SVerticalBox> FirstLeftColumnVerticalBox;
	TSharedPtr<SVerticalBox> FirstRightColumnVerticalBox;
	TSharedPtr<SVerticalBox> SecondLeftColumnVerticalBox;
	TSharedPtr<SVerticalBox> SecondRightColumnVerticalBox;
	TSharedRef<SHorizontalBox> WidgetBox = SNew(SHorizontalBox);
	WidgetBox->AddSlot()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			//First Line
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(30.0f, 5.0f, 0.0f, 0.0f)
			.MaxWidth(200.f)
			[
				// First Left
				SNew(SBox)
				.WidthOverride(200.f)
				[
					SAssignNew(FirstLeftColumnVerticalBox, SVerticalBox)
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f, 5.0f, 0.0f, 0.0f)
			.MaxWidth(200.f)
			[
				// First Right
				SNew(SBox)
				[
					SAssignNew(FirstRightColumnVerticalBox, SVerticalBox)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			//Second Line
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(30.0f, 5.0f, 0.0f, 0.0f)
			.MaxWidth(200.f)
			[
				// Second Left
				SNew(SBox)
				.WidthOverride(200.f)
				[
					SAssignNew(SecondLeftColumnVerticalBox, SVerticalBox)
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f, 5.0f, 0.0f, 0.0f)
			.MaxWidth(200.f)
			[
				// Second Right
				SNew(SBox)
				[
					SAssignNew(SecondRightColumnVerticalBox, SVerticalBox)
				]
			]
		]
	];

	//
	// First line - left
	// 
	FirstLeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 3.5f)
	[
		SNew(SBox)
		.WidthOverride(160.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineCookTriggersLabel", "Cook Triggers"))
		]
	];

	// Parameter change check box
	FText TooltipText = LOCTEXT("HoudiniEngineParameterChangeTooltip", "If enabled, modifying a parameter or input on this Houdini Asset will automatically trigger a cook of the HDA in Houdini.");
	FirstLeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineParameterChangeCheckBoxLabel", "On Parameter/Input Change"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateParameterChangedLambda)
			.IsChecked_Lambda(IsCheckedParameterChangedLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Transform change check box
	TooltipText = LOCTEXT("HoudiniEngineTransformChangeTooltip", "If enabled, changing the Houdini Asset Actor's transform in Unreal will also update its HDA's node transform in Houdini, and trigger a recook of the HDA with the updated transform.");
	FirstLeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineTransformChangeCheckBoxLabel", "On Transform Change"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedTransformChangeLambda)
			.IsChecked_Lambda(IsCheckedTransformChangeLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Triggers Downstream cook checkbox
	TooltipText = LOCTEXT("HoudiniEngineAssetInputCookTooltip", "When enabled, this asset will automatically re-cook after one its asset input has finished cooking.");
	FirstLeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineAssetInputCheckBoxLabel", "On Asset Input Cook"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedAssetInputCookLambda)
			.IsChecked_Lambda(IsCheckedAssetInputCookLambda)
			.ToolTipText(TooltipText)
		]
	];

	//
	// First line - right
	// 
	FirstRightColumnVerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 3.5f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("HoudiniEngineOutputLabel", "Outputs"))
	];

	// Do not generate output check box
	TooltipText = LOCTEXT("HoudiniEnginOutputlessTooltip", "If enabled, this Houdini Asset will cook normally but will not generate any output in Unreal. This is especially usefull when chaining multiple assets together via Asset Inputs.");
	FirstRightColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineDoNotGenerateOutputsCheckBoxLabel", "Do Not Generate Outputs"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedDoNotGenerateOutputsLambda)
			.IsChecked_Lambda(IsCheckedDoNotGenerateOutputsLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Use Output Nodes geos check box
	TooltipText = LOCTEXT("HoudiniEnginUseOutputNodesTooltip", "If enabled, Output nodes found in this Houdini asset will be used alongside the Display node to create outputs.");
	FirstRightColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEnginUseOutputNodesCheckBoxLabel", "Use Output Nodes"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedUseOutputNodesLambda)
			.IsChecked_Lambda(IsCheckedUseOutputNodesLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Output templated geos check box
	TooltipText = LOCTEXT("HoudiniEnginOutputTemplatesTooltip", "If enabled, Geometry nodes in the asset that have the template flag will be outputed.");
	FirstRightColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEnginOutputTemplatesCheckBoxLabel", "Use Templated Geos"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedOutputTemplatedGeosLambda)
			.IsChecked_Lambda(IsCheckedOutputTemplatedGeosLambda)
			.ToolTipText(TooltipText)
		]
	];


	//
	// Second line
	// 
	SecondLeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 3.5f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("HoudiniEngineMiscLabel", "Miscellaneous"))
	];

	// Push Transform to Houdini check box
	TooltipText = LOCTEXT("HoudiniEnginePushTransformTooltip", "If enabled, modifying this Houdini Asset Actor's transform will automatically update the HDA's node transform in Houdini.");
	SecondLeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEnginePushTransformToHoudiniCheckBoxLabel", "Push Transform to Houdini"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedPushTransformToHoudiniLambda)
			.IsChecked_Lambda(IsCheckedPushTransformToHoudiniLambda)
			.ToolTipText(TooltipText)
		]
	];
	
	// Use whole widget
	CheckBoxesRow.WholeRowWidget.Widget = WidgetBox;
}

void 
FHoudiniEngineDetails::CreateHelpAndDebugWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];
	if (!IsValidWeakPointer(MainHAC))
		return;

	// Header Row
	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG);

	if (!MainHAC->bHelpAndDebugMenuExpanded)
		return;

	auto OnFetchCookLogButtonClickedLambda = [InHACs]()
	{
		return ShowCookLog(InHACs);
	};

	auto OnHelpButtonClickedLambda = [MainHAC]()
	{
		return ShowAssetHelp(MainHAC);
	};

	// Button Row
	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> ButtonRowHorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SHorizontalBox> CookLogButtonHorizontalBox = SNew(SHorizontalBox);

	// Fetch Cook Log button
	ButtonRowHorizontalBox->AddSlot()
	//.Padding(15.0f, 0.0f, 0.0f, 0.0f)
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(FText::FromString("Fetch and display all cook logs available for this Houdini Asset Actor."))
			//.Text(FText::FromString("Fetch Cook Log"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnFetchCookLogButtonClickedLambda)
			.Content()
			[
				SAssignNew(CookLogButtonHorizontalBox, SHorizontalBox)
			]
		]
	];

	TSharedPtr<FSlateDynamicImageBrush> CookLogIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUICookLogIconBrush();
	if (CookLogIconBrush.IsValid())
	{
		TSharedPtr<SImage> CookImage;
		CookLogButtonHorizontalBox->AddSlot()
		.MaxWidth(16.0f)
		//.Padding(23.0f, 0.0f, 3.0f, 0.0f)
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
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([CookLogIconBrush]() {
			return CookLogIconBrush.Get();
		})));
	}

	CookLogButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Show Cook Logs"))
	];

	// Asset Help Button
	TSharedPtr<SHorizontalBox> AssetHelpButtonHorizontalBox;
	ButtonRowHorizontalBox->AddSlot()
	//.Padding(4.0, 0.0f, 0.0f, 0.0f)
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(FText::FromString("Display this Houdini Asset Actor's HDA help."))
			//.Text(FText::FromString("Asset Help"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnHelpButtonClickedLambda)
			.Content()
			[
				SAssignNew(AssetHelpButtonHorizontalBox, SHorizontalBox)
		]
		]
	];

	TSharedPtr<FSlateDynamicImageBrush> AssetHelpIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIAssetHelpIconBrush();
	if (AssetHelpIconBrush.IsValid())
	{
		TSharedPtr<SImage> AssetHelpImage;
		AssetHelpButtonHorizontalBox->AddSlot()
		.MaxWidth(16.0f)
		//.Padding(23.0f, 0.0f, 3.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			.HeightOverride(16.0f)
			[
				SAssignNew(AssetHelpImage, SImage)
			]
		];

		AssetHelpImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([AssetHelpIconBrush]() {
			return AssetHelpIconBrush.Get();
		})));
	}

	AssetHelpButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(STextBlock)
		.Text(FText::FromString("Asset Help"))
	];

	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;
}

FMenuBuilder 
FHoudiniEngineDetails::Helper_CreateHoudiniAssetPicker() 
{
	auto OnShouldFilterHoudiniAssetLambda = [](const AActor* const Actor)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		return true;
	};

	auto OnActorSelected = [](AActor* Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Actor Selected"));

		return;
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FActorTreeItem::FFilterPredicate::CreateLambda(OnShouldFilterHoudiniAssetLambda);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("HoudiniEngineDetailsAssetPicker", "Asset"));
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
		TSharedRef<SWidget> MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateActorPicker(InitOptions, FOnActorPicked::CreateLambda(OnActorSelected))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

const FSlateBrush *
FHoudiniEngineDetails::GetHoudiniAssetThumbnailBorder(TSharedPtr< SBorder > HoudiniAssetThumbnailBorder) const
{
	if (HoudiniAssetThumbnailBorder.IsValid() && HoudiniAssetThumbnailBorder->IsHovered())
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}

/*
TSharedRef< SWidget >
FHoudiniEngineDetails::OnGetHoudiniAssetMenuContent(TArray<UHoudiniAssetComponent*> InHACs)
{
	TArray< const UClass * > AllowedClasses;
	AllowedClasses.Add(UHoudiniAsset::StaticClass());

	TArray< UFactory * > NewAssetFactories;

	UHoudiniAsset * HoudiniAsset = nullptr;
	if (InHACs.Num() > 0)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = InHACs[0];
		HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	}
	
	auto OnShouldFilterHoudiniAssetLambda = [](const AActor* const Actor)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		return true;
	};

	// Delegate for filtering Houdini assets.
	FOnShouldFilterAsset OnShouldFilterHoudiniAsset = FOnShouldFilterAsset::CreateLambda(OnShouldFilterHoudiniAssetLambda);
	
	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(HoudiniAsset), true,
		AllowedClasses, NewAssetFactories, OnShouldFilterHoudiniAsset,
		FOnAssetSelected::CreateLambda([](const FAssetData & AssetData) {}),
		FSimpleDelegate::CreateLambda([]() { })
		);
}
*/

FReply
FHoudiniEngineDetails::ShowCookLog(const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACS)
{
	// Convert to an array of valid HACs for the GetCookLog call
	TArray<UHoudiniAssetComponent*> HACs;
	if (InHACS.Num() > 0)
	{
		HACs.Reserve(InHACS.Num());
		for (const auto& HAC : InHACS)
		{
			if (!IsValidWeakPointer(HAC))
				continue;
			HACs.Add(HAC.Get());
		}
	}
	TSharedPtr< SWindow > ParentWindow;
	const FString CookLog = FHoudiniEngineUtils::GetCookLog(HACs);

	// Check if the main frame is loaded. When using the old main frame it may not be.
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule & MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		TSharedPtr<SHoudiniAssetLogWidget> HoudiniAssetCookLog;

		TSharedRef<SWindow> Window =
			SNew(SWindow)
			.Title(LOCTEXT("WindowTitle", "Houdini Cook Log"))
			.ClientSize(FVector2D(640, 480));

		Window->SetContent(
			SAssignNew(HoudiniAssetCookLog, SHoudiniAssetLogWidget)
			.LogText(CookLog));

		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}

	return FReply::Handled();
}

FReply
FHoudiniEngineDetails::ShowAssetHelp(const TWeakObjectPtr<UHoudiniAssetComponent>& InHAC)
{
	if (!IsValidWeakPointer(InHAC))
		return FReply::Handled();

	const FString AssetHelp = FHoudiniEngineUtils::GetAssetHelp(InHAC.Get());

	TSharedPtr< SWindow > ParentWindow;

	// Check if the main frame is loaded. When using the old main frame it may not be.
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked< IMainFrameModule >("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		TSharedPtr<SHoudiniAssetLogWidget> HoudiniAssetHelpLog;

		TSharedRef<SWindow> Window =
			SNew(SWindow)
			.Title(LOCTEXT("WindowTitle", "Houdini Asset Help"))
			.ClientSize(FVector2D(640, 480));

		Window->SetContent(
			SAssignNew(HoudiniAssetHelpLog, SHoudiniAssetLogWidget)
			.LogText(AssetHelp));

		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}
	return FReply::Handled();
}

void 
FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(IDetailCategoryBuilder& HoudiniEngineCategoryBuilder, const TWeakObjectPtr<UHoudiniAssetComponent>& HoudiniAssetComponent, int32 MenuSection)
{
	if (!IsValidWeakPointer(HoudiniAssetComponent))
		return;

	FOnClicked OnExpanderClick = FOnClicked::CreateLambda([HoudiniAssetComponent, MenuSection]()
	{
		if (!IsValidWeakPointer(HoudiniAssetComponent))
			return FReply::Handled();

		switch (MenuSection) 
		{
			case HOUDINI_ENGINE_UI_SECTION_GENERATE:
				HoudiniAssetComponent->bGenerateMenuExpanded = !HoudiniAssetComponent->bGenerateMenuExpanded;
			break;

			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				HoudiniAssetComponent->bBakeMenuExpanded = !HoudiniAssetComponent->bBakeMenuExpanded;
			break;

			case HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS:
				HoudiniAssetComponent->bAssetOptionMenuExpanded = !HoudiniAssetComponent->bAssetOptionMenuExpanded;
			break;

			case HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG:
				HoudiniAssetComponent->bHelpAndDebugMenuExpanded = !HoudiniAssetComponent->bHelpAndDebugMenuExpanded;
		}

		FHoudiniEngineUtils::UpdateEditorProperties(HoudiniAssetComponent.Get(), true);

		return FReply::Handled();
	});

	TFunction<FText(void)> GetText = [MenuSection]() 
	{
		switch (MenuSection)
		{
			case HOUDINI_ENGINE_UI_SECTION_GENERATE:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_GENERATE_HEADER_TEXT);
			break;

			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_BAKE_HEADER_TEXT);
			break;

			case HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS_HEADER_TEXT);
			break;

			case HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG_HEADER_TEXT);
			break;
		}
		return FText::FromString("");
	};

	TFunction<const FSlateBrush*(SButton* InExpanderArrow)> GetExpanderBrush = [HoudiniAssetComponent, MenuSection](SButton* InExpanderArrow)
	{
		FName ResourceName;
		bool bMenuExpanded = false;

		if (IsValidWeakPointer(HoudiniAssetComponent))
		{
			switch (MenuSection)
			{
			case HOUDINI_ENGINE_UI_SECTION_GENERATE:
				bMenuExpanded = HoudiniAssetComponent->bGenerateMenuExpanded;
				break;

			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				bMenuExpanded = HoudiniAssetComponent->bBakeMenuExpanded;
				break;

			case HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS:
				bMenuExpanded = HoudiniAssetComponent->bAssetOptionMenuExpanded;
				break;

			case HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG:
				bMenuExpanded = HoudiniAssetComponent->bHelpAndDebugMenuExpanded;
			}
		}
		
		if (bMenuExpanded)
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
		}
		else
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
		}
		
		return FEditorStyle::GetBrush(ResourceName);
	};

	return AddHeaderRow(HoudiniEngineCategoryBuilder, OnExpanderClick, GetText, GetExpanderBrush);
}

void
FHoudiniEngineDetails::AddHeaderRowForHoudiniPDGAssetLink(IDetailCategoryBuilder& PDGCategoryBuilder, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, int32 MenuSection)
{
	if (!IsValidWeakPointer(InPDGAssetLink))
		return;

	FOnClicked OnExpanderClick = FOnClicked::CreateLambda([InPDGAssetLink, MenuSection]()
	{
		if (!IsValidWeakPointer(InPDGAssetLink))
			return FReply::Handled();
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
			InPDGAssetLink.Get());

		switch (MenuSection) 
		{
			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				InPDGAssetLink->Modify();
				InPDGAssetLink->bBakeMenuExpanded = !InPDGAssetLink->bBakeMenuExpanded;
				FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(
					GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeMenuExpanded), InPDGAssetLink.Get());
			break;
		}

		//FHoudiniEngineUtils::UpdateEditorProperties(InPDGAssetLink, true);

		return FReply::Handled();
	});

	TFunction<FText(void)> GetText = [MenuSection]() 
	{
		switch (MenuSection)
		{
			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_BAKE_HEADER_TEXT);
			break;
		}
		return FText::FromString("");
	};

	TFunction<const FSlateBrush*(SButton* InExpanderArrow)> GetExpanderBrush = [InPDGAssetLink, MenuSection](SButton* InExpanderArrow)
	{
		FName ResourceName;
		bool bMenuExpanded = false;

		if (IsValidWeakPointer(InPDGAssetLink))
		{
			switch (MenuSection)
			{
			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				bMenuExpanded = InPDGAssetLink->bBakeMenuExpanded;
				break;
			}
		}

		if (bMenuExpanded)
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
		}
		else
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
		}
		
		return FEditorStyle::GetBrush(ResourceName);
	};

	return AddHeaderRow(PDGCategoryBuilder, OnExpanderClick, GetText, GetExpanderBrush);	
}

void 
FHoudiniEngineDetails::AddHeaderRow(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	FOnClicked& InOnExpanderClick,
	TFunction<FText(void)>& InGetText,
	TFunction<const FSlateBrush*(SButton* InExpanderArrow)>& InGetExpanderBrush) 
{
	// Header Row
	FDetailWidgetRow & HeaderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SHorizontalBox> HeaderHorizontalBox;
	HeaderRow.WholeRowWidget.Widget = SAssignNew(HeaderHorizontalBox, SHorizontalBox);

	TSharedPtr<SImage> ExpanderImage;
	TSharedPtr<SButton> ExpanderArrow;
	HeaderHorizontalBox->AddSlot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoWidth()
	[
		SAssignNew(ExpanderArrow, SButton)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.ClickMethod(EButtonClickMethod::MouseDown)
		.Visibility(EVisibility::Visible)
		.OnClicked(InOnExpanderClick)
		[
			SAssignNew(ExpanderImage, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];

	HeaderHorizontalBox->AddSlot().Padding(1.0f).VAlign(VAlign_Center).AutoWidth()
	[
		SNew(STextBlock)
		.Text_Lambda([InGetText](){return InGetText(); })
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	ExpanderImage->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			[ExpanderArrow, InGetExpanderBrush]()
			{
				return InGetExpanderBrush(ExpanderArrow.Get());
			}));
}

#undef LOCTEXT_NAMESPACE
