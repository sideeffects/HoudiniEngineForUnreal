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

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngineDetails.h"

#include "HAPI/HAPI_Version.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEditorNodeSyncSubsystem.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniPackageParams.h"
#include "HoudiniParameter.h"
#include "HoudiniPresetFactory.h"
#include "HoudiniToolsEditor.h"
#include "SHoudiniPresets.h"
#include "SSelectFolderPathDialog.h"
#include "SSelectHoudiniPathDialog.h"

#include "ActorPickerMode.h"
#include "ActorTreeItem.h"
#include "AssetSelection.h"
#include "AssetThumbnail.h"
#include "Brushes/SlateImageBrush.h"
#include "CoreMinimal.h"
//#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
//#include "IContentBrowserSingleton.h"
#include "IDetailGroup.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "SceneOutlinerModule.h"
#include "ScopedTransaction.h"
#include "SEnumCombo.h"
#include "ToolMenuEntry.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define HOUDINI_ENGINE_UI_SECTION_GENERATE												1
#define HOUDINI_ENGINE_UI_SECTION_BAKE													2
#define HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS											3
#define HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG										4


#define HOUDINI_ENGINE_UI_BUTTON_WIDTH											   150.0f

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
		.BorderImage(_GetEditorStyle().GetBrush(TEXT("Menu.Background")))
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

	// Houdini Engine Icon
	FHoudiniEngineDetails::CreateHoudiniEngineIconWidget(HoudiniEngineCategoryBuilder, InHACs);

	// Widget for HoudiniAsset related actions. Currently only contains things for Presets.
	FHoudiniEngineDetails::CreateHoudiniEngineActionWidget(HoudiniEngineCategoryBuilder, InHACs);
	
	// Houdini Engine Session Status
	FHoudiniAssetComponentDetails::AddSessionStatusRow(HoudiniEngineCategoryBuilder);
	
	// Create Generate Category
	FHoudiniEngineDetails::CreateGenerateWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
	// Create Bake Category
	FHoudiniEngineDetails::CreateBakeWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
	// Create Asset Options Category
	FHoudiniEngineDetails::CreateAssetOptionsWidgets(HoudiniEngineCategoryBuilder, InHACs);

	// Create Help and Debug Category
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
	IDetailLayoutBuilder* SavedLayoutBuilder = &HoudiniEngineCategoryBuilder.GetParentLayout();

	if (!IsValidWeakPointer(MainHAC))
		return;

	// Skip drawing the icon if the icon image is not loaded correctly.
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();
	if (!HoudiniEngineUIIconBrush.IsValid())
		return;

	FDetailWidgetRow & Row = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	TSharedPtr<SImage> Image;

	TSharedPtr<SLayeredImage> OptionsImage = SNew(SLayeredImage)
		 .Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
		 .ColorAndOpacity(FSlateColor::UseForeground());
	
	Box->AddSlot()
	.AutoWidth()
	.Padding(0.0f, 5.0f, 5.0f, 10.0f)
	.HAlign(HAlign_Left)
	[
		SNew(SBox)
		.IsEnabled(false)
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
FHoudiniEngineDetails::CreateHoudiniEngineActionWidget(IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs)
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];
	IDetailLayoutBuilder* SavedLayoutBuilder = &HoudiniEngineCategoryBuilder.GetParentLayout();

	if (!IsValidWeakPointer(MainHAC))
		return;

	// Skip drawing the icon if the icon image is not loaded correctly.
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();
	if (!HoudiniEngineUIIconBrush.IsValid())
		return;

	FDetailWidgetRow & Row = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	TSharedPtr<SImage> Image;

	TSharedPtr<SLayeredImage> OptionsImage = SNew(SLayeredImage)
		 .Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
		 .ColorAndOpacity(FSlateColor::UseForeground());
	
	Box->AddSlot()
	.FillWidth(1.0f)
	.HAlign(HAlign_Right)
	[
		SNew(SComboButton)
		.HasDownArrow(false)
		.ContentPadding(0)
		.ForegroundColor( FSlateColor::UseForeground() )
		.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
		.OnGetMenuContent_Lambda([InHACs, SavedLayoutBuilder]() -> TSharedRef<SWidget>
		{
			return ConstructActionMenu(InHACs, SavedLayoutBuilder).ToSharedRef();
		})
		.ButtonContent()
		[
			OptionsImage.ToSharedRef()
		]
	];

	Row.WholeRowWidget.Widget = Box;
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

	bool bIsNodeSyncComponent = MainHAC->IsA<UHoudiniNodeSyncComponent>();

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
		SetCookFolderPath(Val, MainHAC, InHACs);
	};

	auto OnCookFolderBrowseButtonClickedLambda = [InHACs, MainHAC]()
	{
		TSharedRef<SSelectFolderPathDialog> Dialog =
			SNew(SSelectFolderPathDialog)
			.InitialPath(FText::FromString(MainHAC->GetTemporaryCookFolderOrDefault()))
			.TitleText(LOCTEXT("CookFolderDialogTitle", "Select Temporary Cook Folder"));

		if (Dialog->ShowModal() != EAppReturnType::Cancel)
		{
			SetCookFolderPath(Dialog->GetFolderPath(), MainHAC, InHACs);
		}

		return FReply::Handled();
	};

	auto OnCookFolderResetButtonClickedLambda = [InHACs, MainHAC]()
	{
		FText EmptyText;
		SetCookFolderPath(EmptyText, MainHAC, InHACs);

		return FReply::Handled();
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
	if(!bIsNodeSyncComponent)
	{
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
		.HAlign(HAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			//.MinDesiredWidth(160.f)
			.Text(FText::FromString("Reset Parameters"))
		];
	}

	// Temp Cook Folder Row
	FDetailWidgetRow & TempCookFolderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Temporary Cook Folder"));

	TSharedRef<SHorizontalBox> TempCookFolderRowHorizontalBox = SNew(SHorizontalBox);

	TempCookFolderRowHorizontalBox->AddSlot()
	.MaxWidth(155.0f)
	.VAlign(VAlign_Center)
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
	//.MaxWidth(235.0f)
	[
		SNew(SBox)
		//.WidthOverride(235.0f)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT(
				"HoudiniEngineTemporaryCookFolderTooltip",
				"Default folder used to store the temporary files (Static Meshes, Materials, Textures..) that are "
				"generated by Houdini Assets when they cook. If this value is blank, the default from the plugin "
				"settings is used."))
			.HintText(LOCTEXT("HoudiniEngineTempCookFolderHintText", "Input to set temporary cook folder"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([MainHAC]()
			{
				if (!IsValidWeakPointer(MainHAC))
					return FText();
				return FText::FromString(MainHAC->TemporaryCookFolder.Path);
			})
			.OnTextCommitted_Lambda(OnCookFolderTextCommittedLambda)
		]
	];

	TempCookFolderRowHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SButton)
		//.ContentPadding(FMargin(6.0, 2.0))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.IsEnabled(true)
		.Text(LOCTEXT("BrowseButtonText", "Browse"))
		.ToolTipText(LOCTEXT("CookFolderBrowseButtonToolTip", "Browse to select temporary cook folder"))
		.OnClicked_Lambda(OnCookFolderBrowseButtonClickedLambda)
	];

	TempCookFolderRowHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		//.ContentPadding(FMargin(6.0, 2.0))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.IsEnabled(true)
		.Text(LOCTEXT("ResetButtonText", "Reset"))
		.ToolTipText(LOCTEXT("CookFolderResetButtonToolTip", "Reset the cook folder to default setting"))
		.OnClicked_Lambda(OnCookFolderResetButtonClickedLambda)
	];
	
	TempCookFolderRow.WholeRowWidget.Widget = TempCookFolderRowHorizontalBox;
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

			FHoudiniBakeSettings BakeSettings;
			BakeSettings.SetFromHAC(MainHAC.Get());

			FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
				NextHAC.Get(),
				BakeSettings,
				MainHAC->HoudiniEngineBakeOption,
				MainHAC->bRemoveOutputAfterBake);
		}

		return FReply::Handled();	
	};

	auto OnBakeFolderTextCommittedLambda = [InHACs, MainHAC](const FText& Val, ETextCommit::Type TextCommitType)
	{
		SetBakeFolderPath(Val, MainHAC, InHACs);
	};

	// Button Row
	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Bake"));

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

	TArray<TSharedPtr<FString>>* BakeOptionSources = FHoudiniEngineEditor::Get().GetHoudiniEngineBakeTypeOptionsLabels();
	TSharedPtr<FString> IntialSelec = MakeShareable(new FString(FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(MainHAC->HoudiniEngineBakeOption)));

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
			.OptionsSource(BakeOptionSources)
			.InitiallySelectedItem(IntialSelec)
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

					if (MainHAC->HoudiniEngineBakeOption == NewOption)
						continue;

					MainHAC->HoudiniEngineBakeOption = NewOption;
					NextHAC->MarkPackageDirty();
				}

				if (MainHAC.IsValid())
					FHoudiniEngineUtils::UpdateEditorProperties(true);
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
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];
	
	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;

	// Clear Output After Baking Row
	FDetailWidgetRow & ClearOutputAfterBakingRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Bake Options"));
	TSharedRef<SHorizontalBox> AdditionalBakeSettingsRowHorizontalBox = SNew(SHorizontalBox);

	// Remove Output Checkbox
	TSharedPtr<SCheckBox> CheckBoxRemoveOutput;
	TSharedPtr<SCheckBox> CheckBoxAutoBake;
	TSharedPtr<SCheckBox> CheckBoxRecenterBakedActors;
	TSharedPtr<SCheckBox> CheckBoxReplacePreviousBake;
	TSharedPtr<SCheckBox> CheckBoxGroupBakedComponents;

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
                .Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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

					if (NextHAC->bRemoveOutputAfterBake == bNewState) 
						continue;

                    NextHAC->bRemoveOutputAfterBake = bNewState;
					NextHAC->MarkPackageDirty();
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
                .Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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

					if (NextHAC->bRecenterBakedActors == bNewState)
						continue;

                    NextHAC->bRecenterBakedActors = bNewState;
					NextHAC->MarkPackageDirty();
                }

                // FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
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
            SAssignNew(CheckBoxAutoBake, SCheckBox)
            .Content()
            [
                SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIAutoBakeCheckBox", "Auto Bake"))
                .ToolTipText(LOCTEXT("HoudiniEngineUIAutoBakeCheckBoxToolTip", "Automatically bake the next cook."))
                .Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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

					if (NextHAC->IsBakeAfterNextCookEnabled() == bNewState)
						continue;

                    NextHAC->SetBakeAfterNextCook(bNewState ? EHoudiniBakeAfterNextCook::Always : EHoudiniBakeAfterNextCook::Disabled);
					NextHAC->MarkPackageDirty();
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
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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

					if (NextHAC->bReplacePreviousBake == bNewState)
						continue;

					NextHAC->bReplacePreviousBake = bNewState;
					NextHAC->MarkPackageDirty();
				}

				if (MainHAC.IsValid())
					FHoudiniEngineUtils::UpdateEditorProperties(true);
			})
		]
	];

	ClearOutputAfterBakingRow.WholeRowWidget.Widget = AdditionalBakeSettingsRowHorizontalBox;

	// Bake Folder Row
	FDetailWidgetRow & BakeFolderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Bake Folder"));

	TSharedRef<SHorizontalBox> BakeFolderRowHorizontalBox = SNew(SHorizontalBox);

	BakeFolderRowHorizontalBox->AddSlot()
	.MaxWidth(155.0f)
	.VAlign(VAlign_Center)
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
	.MaxWidth(235.0)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT(
				"HoudiniEngineBakeFolderTooltip",
				"The folder used to store the objects that are generated by this Houdini Asset when baking, if the "
				"unreal_bake_folder attribute is not set on the geometry. If this value is blank, the default from the "
				"plugin settings is used."))
			.HintText(LOCTEXT("HoudiniEngineBakeFolderHintText", "Input to set bake folder"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([MainHAC]()
			{
				if (!IsValidWeakPointer(MainHAC))
					return FText();
				return FText::FromString(MainHAC->BakeFolder.Path);
			})
			.OnTextCommitted_Lambda(OnBakeFolderTextCommittedLambda)
		]
	];

	TArray<TSharedPtr<FString>>* ActorBakeOptionSources = FHoudiniEngineEditor::Get().GetHoudiniEngineBakeActorOptionsLabels();

	ButtonRowHorizontalBox->AddSlot()
		/*.AutoWidth()*/
		.Padding(3.0, 0.0, 4.0f, 0.0f)
		//.MaxWidth(103.f)
		.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH * 1.5f)
		[
			SNew(SBox)
			//.WidthOverride(103.f)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(ActorBakeOptionSources)
			.InitiallySelectedItem(IntialSelec)
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
				[MainHAC, InHACs](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
				{
					if (!NewChoice.IsValid())
						return;

					const EHoudiniEngineActorBakeOption NewOption =
						FHoudiniEngineEditor::Get().StringToHoudiniEngineActorBakeOption(*NewChoice.Get());

					for (auto& NextHAC : InHACs)
					{
						if (!IsValidWeakPointer(NextHAC))
							continue;

						if (MainHAC->ActorBakeOption == NewOption)
							continue;

						MainHAC->ActorBakeOption = NewOption;
						NextHAC->MarkPackageDirty();
					}

					if (MainHAC.IsValid())
						FHoudiniEngineUtils::UpdateEditorProperties(true);
				})
			[
				SNew(STextBlock)
				.Text_Lambda([MainHAC]()
				{
					if (!IsValidWeakPointer(MainHAC))
						return FText();

					return FText::FromString(
						FHoudiniEngineEditor::GetStringfromActorBakeOption(MainHAC->ActorBakeOption));
				})
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	auto OnBakeFolderBrowseButtonClickedLambda = [BakeFolderRowHorizontalBox, MainHAC, InHACs]()
	{
		TSharedRef<SSelectFolderPathDialog> Dialog =
			SNew(SSelectFolderPathDialog)
			.InitialPath(FText::FromString(MainHAC->GetBakeFolderOrDefault()))
			.TitleText(LOCTEXT("BakeFolderDialogTitle", "Select Bake Folder"));

		if (Dialog->ShowModal() != EAppReturnType::Cancel)
		{
			SetBakeFolderPath(Dialog->GetFolderPath(), MainHAC, InHACs);
		}

		return FReply::Handled();
	};

	auto OnBakeFolderResetButtonClickedLambda = [MainHAC, InHACs]()
	{
		FText EmptyText;
		SetBakeFolderPath(EmptyText, MainHAC, InHACs);

		return FReply::Handled();
	};

	BakeFolderRowHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SButton)
		//.ContentPadding(FMargin(6.0, 2.0))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.IsEnabled(true)
		.Text(LOCTEXT("BrowseButtonText", "Browse"))
		.ToolTipText(LOCTEXT("BakeFolderBrowseButtonToolTip", "Browse to select bake folder"))
		.OnClicked_Lambda(OnBakeFolderBrowseButtonClickedLambda)
	];

	BakeFolderRowHorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		//.ContentPadding(FMargin(6.0, 2.0))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.IsEnabled(true)
		.Text(LOCTEXT("ResetButtonText", "Reset"))
		.ToolTipText(LOCTEXT("BrowseButtonToolTip", "Reset the bake folder to default setting"))
		.OnClicked_Lambda(OnBakeFolderResetButtonClickedLambda)
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
	}
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

	bool bIsNodeSyncComponent = MainHAC->IsA<UHoudiniNodeSyncComponent>();

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

			if (NextHAC->bCookOnParameterChange == bChecked)
				continue;

			NextHAC->bCookOnParameterChange = bChecked;
			NextHAC->MarkPackageDirty();
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

			if (NextHAC->bCookOnTransformChange == bChecked)
				continue;

			NextHAC->bCookOnTransformChange = bChecked;
			NextHAC->MarkPackageDirty();
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

			if (NextHAC->bCookOnAssetInputCook == bChecked)
				continue;

			NextHAC->bCookOnAssetInputCook = bChecked;
			NextHAC->MarkPackageDirty();
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

			if (NextHAC->bUploadTransformsToHoudiniEngine == bChecked)
				continue;

			NextHAC->bUploadTransformsToHoudiniEngine = bChecked;
			NextHAC->MarkPackageDirty();
			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedUseTempLandscapesLayersToHoudiniLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bLandscapeUseTempLayers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedUseTempLandscapeLayersLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			if (NextHAC->bLandscapeUseTempLayers == bChecked)
				continue;

			NextHAC->bLandscapeUseTempLayers = bChecked;
			NextHAC->MarkPackageDirty();
		}
	};

	auto IsCheckedEnableCurveEditingLambda = [MainHAC]()
	{
		if (!IsValidWeakPointer(MainHAC))
			return ECheckBoxState::Unchecked;

		return MainHAC->bEnableCurveEditing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedEnableCurveEditingLambda = [InHACs](ECheckBoxState NewState)
	{
		const bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			if (NextHAC->bEnableCurveEditing == bChecked)
				continue;

			NextHAC->bEnableCurveEditing = bChecked;
			NextHAC->MarkPackageDirty();
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

			if (NextHAC->bOutputless == bChecked)
				continue;

			NextHAC->bOutputless = bChecked;
			NextHAC->MarkPackageDirty();
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

			if (NextHAC->bOutputTemplateGeos == bChecked)
				continue;

			NextHAC->bOutputTemplateGeos = bChecked;
			NextHAC->MarkPackageDirty();
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

			if (NextHAC->bUseOutputNodes == bChecked)
				continue;

			NextHAC->bUseOutputNodes = bChecked;
			NextHAC->MarkPackageDirty();
			NextHAC->MarkAsNeedCook();
		}
	};

	// Checkboxes row
	FDetailWidgetRow & CheckBoxesRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Asset Cook Options"));
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

	FText TooltipText;
	if (!bIsNodeSyncComponent)
	{
		// Parameter change check box
		TooltipText = LOCTEXT("HoudiniEngineParameterChangeTooltip", "If enabled, modifying a parameter or input on this Houdini Asset will automatically trigger a cook of the HDA in Houdini.");
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
	}

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

	if (!bIsNodeSyncComponent)
	{
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
	}

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

	// Landscape Temp Layers
	TooltipText = LOCTEXT("HoudiniEngineTempLandscapeLayersTooltip", "Cooking use temporary landscape layers.");
	SecondLeftColumnVerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
		.Text(LOCTEXT("HoudiniEngineTempLandscapeCheckBoxLabel", "Temp Landscape Layers"))
		.ToolTipText(TooltipText)
		]
	+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedUseTempLandscapeLayersLambda)
		.IsChecked_Lambda(IsCheckedUseTempLandscapesLayersToHoudiniLambda)
		.ToolTipText(TooltipText)
		]
		];


	// Curve Editing
	TooltipText = LOCTEXT("HoudiniEngineEnableCurveEditingTooltip", "Enable curve editing.");
	SecondLeftColumnVerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
		.Text(LOCTEXT("HoudiniEngineEnableCurveEditingToolLabel", "Enable Curve Editing"))
		.ToolTipText(TooltipText)
		]
	+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedEnableCurveEditingLambda)
		.IsChecked_Lambda(IsCheckedEnableCurveEditingLambda)
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

	bool bIsNodeSyncComponent = MainHAC->IsA<UHoudiniNodeSyncComponent>();

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
	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Help Cook Logs"));
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

	if (!bIsNodeSyncComponent)
	{
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
	}

	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;
}

FString
FormatHoudiniVersionString(int32 VersionMajor, int32 VersionMinor, int32 VersionBuild, int32 VersionPatch)
{
	return FString::Printf(TEXT("%d.%d.%d.%d"), VersionMajor, VersionMinor, VersionBuild, VersionPatch);
}

FString
FormatEngineVersionString(int32 VersionMajor, int32 VersionMinor, int32 VersionApi)
{
	return FString::Printf(TEXT("%d.%d.%d"), VersionMajor, VersionMinor, VersionApi);
}

void
CreateInstallInfoStrings(FString &InstallInfo, FString &InstallInfoStyled)
{
	FString VersionHoudiniBuilt = FormatHoudiniVersionString(
		HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD, HAPI_VERSION_HOUDINI_PATCH);
	FString VersionEngineBuilt = FormatEngineVersionString(
		HAPI_VERSION_HOUDINI_ENGINE_MAJOR, HAPI_VERSION_HOUDINI_ENGINE_MINOR, HAPI_VERSION_HOUDINI_ENGINE_API);
	FString VersionHoudiniRunning;
	FString VersionEngineRunning;

	// Add running against Houdini version.
	{
		int32 RunningMajor = 0;
		int32 RunningMinor = 0;
		int32 RunningBuild = 0;
		int32 RunningPatch = 0;

		if (FHoudiniApi::IsHAPIInitialized())
		{
			const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();
			// Retrieve version numbers for running Houdini.
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_MAJOR, &RunningMajor);
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_MINOR, &RunningMinor);
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_BUILD, &RunningBuild);
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_PATCH, &RunningPatch);

			VersionHoudiniRunning = FormatHoudiniVersionString(
				RunningMajor, RunningMinor, RunningBuild, RunningPatch);
		}
		else
			VersionHoudiniRunning = TEXT("Unknown");
	}

	// Add running against Houdini Engine version.
	{
		int32 RunningEngineMajor = 0;
		int32 RunningEngineMinor = 0;
		int32 RunningEngineApi = 0;

		if (FHoudiniApi::IsHAPIInitialized())
		{
			const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();
			// Retrieve version numbers for running Houdini Engine.
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
			FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

			VersionEngineRunning = FormatEngineVersionString(
				RunningEngineMajor, RunningEngineMinor, RunningEngineApi);
		}
		else
			VersionHoudiniRunning = "Unknown";
	}

	// Add path of libHAPI.
	FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
	if (LibHAPILocation.IsEmpty())
		LibHAPILocation = TEXT("Not Found");

	FString HoudiniExecutable = FHoudiniEngine::Get().GetHoudiniExecutable();

	// Add licensing info.
	FString HAPILicenseType = TEXT("");
	if (!FHoudiniEngineUtils::GetLicenseType(HAPILicenseType))
		HAPILicenseType = TEXT("Unknown");

	FString HoudiniSessionStatus;
	switch (FHoudiniEngine::Get().GetSessionStatus()) {
	case EHoudiniSessionStatus::Invalid:
		HoudiniSessionStatus = TEXT("Invalid");
		break;
	case EHoudiniSessionStatus::NotStarted:
		HoudiniSessionStatus = TEXT("NotStarted");
		break;
	case EHoudiniSessionStatus::Connected:
		HoudiniSessionStatus = TEXT("Connected");
		break;
	case EHoudiniSessionStatus::None:
		HoudiniSessionStatus = TEXT("None");
		break;
	case EHoudiniSessionStatus::Stopped:
		HoudiniSessionStatus = TEXT("Stopped");
		break;
	case EHoudiniSessionStatus::Failed:
		HoudiniSessionStatus = TEXT("Failed");
		break;
	case EHoudiniSessionStatus::Lost:
		HoudiniSessionStatus = TEXT("Lost");
		break;
	case EHoudiniSessionStatus::NoLicense:
		HoudiniSessionStatus = TEXT("NoLicense");
		break;
	}

	const FString InstallInfoFormat = TEXT(
R"""(Plugin was built with:
  Houdini: {0}
  HoudiniEngine: {1}

Plugin is running with:
  Houdini: {2}
  HoudiniEngine: {3}

Houdini Executable Type: {4}
HoudiniEngine Library Location: {5}

License Type Acquired: {6}
Current Session Status: {7})""");

	const FString InstallInfoFormatStyled = TEXT(
R"""(<InstallInfo.Bold>Plugin was built with</>:
  <InstallInfo.Italic>Houdini</>: {0}
  <InstallInfo.Italic>HoudiniEngine</>: {1}

<InstallInfo.Bold>Plugin is running with</>:
  <InstallInfo.Italic>Houdini</>: {2}
  <InstallInfo.Italic>HoudiniEngine</>: {3}

<InstallInfo.Italic>Houdini Executable Type</>: {4}
<InstallInfo.Italic>HoudiniEngine Library Location</>: {5}

<InstallInfo.Italic>License Type Acquired</>: {6}
<InstallInfo.Italic>Current Session Status</>: {7})""");

	TArray<FStringFormatArg> Args
	{
		VersionHoudiniBuilt,
		VersionEngineBuilt,
		VersionHoudiniRunning,
		VersionEngineRunning,
		HoudiniExecutable,
		LibHAPILocation,
		HAPILicenseType,
		HoudiniSessionStatus
	};

	InstallInfo = FString::Format(*InstallInfoFormat, Args);
	InstallInfoStyled = FString::Format(*InstallInfoFormatStyled, Args);
}

void
FHoudiniEngineDetails::CreateInstallInfoWindow()
{
	FString InstallInfo;
	FString InstallInfoStyled;
	CreateInstallInfoStrings(InstallInfo, InstallInfoStyled);

	auto CopyInstallInfo = [InstallInfo]()
	{
		FPlatformApplicationMisc::ClipboardCopy(*InstallInfo);
		return FReply::Handled();
	};

	TSharedPtr<SImage> Image;
	TSharedPtr<SButton> CloseButton;
	float InstallInfoButtonWidth = 70.0f;

	TSharedRef<SWindow> InstallInfoWindow =
		SNew(SWindow)
		.Title(LOCTEXT("InstallInfoTitle", "Houdini Engine Installation Info"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			// Houdini Engine Logo
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			.Padding(20.0f, 20.0f, 20.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
					.HeightOverride(30)
					.WidthOverride(208)
					[
						SAssignNew(Image, SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
			// Install Info
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			.Padding(20.0f, 20.0f, 20.0f, 0.0f)
			[
				SNew(SRichTextBlock)
				.Text(FText::FromString(InstallInfoStyled))
				.DecoratorStyleSet(FHoudiniEngineStyle::Get().Get())
				.Justification(ETextJustify::Left)
				.LineHeightPercentage(1.25f)
			]
			+SVerticalBox::Slot()
			.Padding(20.0f, 20.0f, 20.0f, 0.0f)
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SHorizontalBox)
				// Copy Button
				+SHorizontalBox::Slot()
				.MaxWidth(InstallInfoButtonWidth)
				[
					SNew(SBox)
					.WidthOverride(InstallInfoButtonWidth)
					[
						SNew(SButton)
						.Content()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Copy", "Copy"))
							]
						]
						.OnClicked_Lambda(CopyInstallInfo)
					]
				]
				// Close Button
				+SHorizontalBox::Slot()
				.MaxWidth(InstallInfoButtonWidth)
				[
					SNew(SBox)
					.WidthOverride(InstallInfoButtonWidth)
					[
						SAssignNew(CloseButton, SButton)
						.Content()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Close", "Close"))
							]
						]
					]
				]
			]
		];

	CloseButton->SetOnClicked(
		FOnClicked::CreateLambda(
			[InstallInfoWindow]()
			{
				TSharedRef<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(InstallInfoWindow).ToSharedRef();
				FSlateApplication::Get().RequestDestroyWindow(InstallInfoWindow);
				return FReply::Handled();
			}));

	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();

	// Skip drawing the icon if the icon image is not loaded correctly.
	if (HoudiniEngineUIIconBrush.IsValid())
	{
		Image->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda(
					[HoudiniEngineUIIconBrush]() { return HoudiniEngineUIIconBrush.Get(); }
		)));
	}

	IMainFrameModule &MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));

	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(InstallInfoWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(InstallInfoWindow);
	}
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
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Gutter_Localized()));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef<SWidget> MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(_GetEditorStyle().GetBrush("Menu.Background"))
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
		return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailShadow");
}

TSharedPtr<SWidget>
FHoudiniEngineDetails::ConstructActionMenu(const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs, class IDetailLayoutBuilder* LayoutBuilder)
{
	FMenuBuilder MenuBuilder( true, NULL );

	const int32 NumHACs = InHACs.Num(); 

	if (NumHACs == 0)
	{
		return MenuBuilder.MakeWidget();
	}

	TWeakObjectPtr<UHoudiniAssetComponent> HAC = InHACs[0];

	if (!HAC.IsValid())
	{
		return MenuBuilder.MakeWidget();
	}

	MenuBuilder.BeginSection("AssetCreate", LOCTEXT("HDAActionMenu_SectionCreate", "Create"));

	// Create Preset
	MenuBuilder.AddMenuEntry(
		FText::FromString("Create Preset"),
		FText::FromString("Create a new preset from the current HoudiniAssetComponent parameters."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([HAC]() -> void
			{
				SHoudiniCreatePresetFromHDA::CreateDialog(HAC);
			}),
			FCanExecuteAction()
		)
	);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Modify", LOCTEXT("HDAActionMenu_SectionModify", "Modify"));

	// Update Selected Preset (if a preset asset is selected)
	MenuBuilder.AddMenuEntry(
		FText::FromString("Update Selected Preset"),
		FText::FromString("Update the Houdini Preset that is currently selected in the content browser."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([HAC]() -> void
			{
				SHoudiniUpdatePresetFromHDA::CreateDialog(HAC);
			}),
			FCanExecuteAction::CreateLambda([NumHACs]() -> bool
			{
				if (NumHACs != 1)
				{
					return false;
				}
				
				TArray<FAssetData> SelectedAssets; 
				AssetSelectionUtils::GetSelectedAssets(SelectedAssets);
				if (SelectedAssets.Num() != 1)
				{
					return false;
				}

				const FAssetData& AssetData = SelectedAssets[0];
				const UHoudiniPreset* SelectedPreset = Cast<UHoudiniPreset>(AssetData.GetAsset());
				return IsValid(SelectedPreset);
			})
		)
	);
	
	MenuBuilder.EndSection();

	TArray<UHoudiniPreset*> Presets;
	FHoudiniToolsEditor::FindPresetsForHoudiniAsset(HAC->GetHoudiniAsset(), Presets);

	Algo::Sort(Presets, [](const UHoudiniPreset* LHS, const UHoudiniPreset* RHS) { return LHS->Name < RHS->Name; });

	TSharedPtr<SImage> SearchImage = SNew(SImage)
		 .Image(FAppStyle::Get().GetBrush("Symbols.SearchGlass"))
		 .ColorAndOpacity(FSlateColor::UseForeground());

	// Presets
	// TODO: store presets in a searchable submenu
	MenuBuilder.BeginSection("Presets", LOCTEXT("HDAActionMenu_SectionPresets", "Presets"));
	for (UHoudiniPreset* Preset : Presets)
	{
		if (!IsValid(Preset))
		{
			continue;
		}

		if (Preset->bHidePreset)
		{
			continue;
		}

		TSharedRef<SHorizontalBox> PresetItem =
			SNew(SHorizontalBox)

			// Preset Name 
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( FText::FromString(Preset->Name) )
			]

			// Browse to HoudiniPreset button
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ContentPadding(0)
				.ForegroundColor( FSlateColor::UseForeground() )
				.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
				.ToolTipText( LOCTEXT("HDAActionMenu_SectionPresets_FindInCB", "Find in Content Browser") )
				.OnClicked_Lambda([Preset]() -> FReply
				{
					FHoudiniToolsEditor::BrowseToObjectInContentBrowser(Preset);
					return FReply::Handled();
				})
				.Content()
				[
					SearchImage.ToSharedRef()
				]
			];
		
		// Menu entry for preset
		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateLambda([Preset, InHACs, LayoutBuilder]() -> void
					{
						bool bPresetApplied = false;
						for (TWeakObjectPtr<UHoudiniAssetComponent> HAC : InHACs)
						{
							// Apply preset on Houdini Asset Component
							if (!HAC.IsValid())
							{
								HOUDINI_LOG_WARNING(TEXT("Could not apply preset. HoudiniAssetComponent reference is no longer valid."));
								continue;
							}

							FHoudiniToolsEditor::ApplyPresetToHoudiniAssetComponent(Preset, HAC.Get(), true);
							bPresetApplied = true;
						}
					
						if (bPresetApplied && LayoutBuilder)
						{
							LayoutBuilder->ForceRefreshDetails();
						}
					}),
					FCanExecuteAction()
				),
			PresetItem,
			NAME_None,
			FText::FromString(Preset->Description)
			);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
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

	// If we have a help URL, then open it
	const FString AssetHelpURL = FHoudiniEngineUtils::GetAssetHelpURL(InHAC.Get());
	if (AssetHelpURL.StartsWith(TEXT("http://")) || AssetHelpURL.StartsWith(TEXT("https://")) || AssetHelpURL.StartsWith(TEXT("file://")))
	{
		FPlatformProcess::LaunchURL(*AssetHelpURL, nullptr, nullptr);
		return FReply::Handled();
	}
	
	// If not, get the help string
	const FString AssetHelp = FHoudiniEngineUtils::GetAssetHelp(InHAC.Get());
	
	// Check if the main frame is loaded. When using the old main frame it may not be.
	TSharedPtr<SWindow> ParentWindow;
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

	FOnClicked OnExpanderClick = FOnClicked::CreateLambda([HoudiniAssetComponent, MenuSection,&HoudiniEngineCategoryBuilder]()
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

		FHoudiniEngineUtils::UpdateEditorProperties(true);

		// TODO: This is a quick fix for 130742. However, its not a complete solution since clicking the expansion does not
		// always update all details panels correctly. The correct solution here is to move all the above expansion bools, like
		// bAssetOptionMenuExpanded, out of the HAC into per- IDetailLayoutBuilder values. ie. store them per Details panel,
		// not per-component. https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/Slate/DetailsCustomization/

		HoudiniEngineCategoryBuilder.GetParentLayout().ForceRefreshDetails();

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
		
		return _GetEditorStyle().GetBrush(ResourceName);
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
		
		return _GetEditorStyle().GetBrush(ResourceName);
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
		.ButtonStyle(_GetEditorStyle(), "NoBorder")
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
		.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	ExpanderImage->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			[ExpanderArrow, InGetExpanderBrush]()
			{
				return InGetExpanderBrush(ExpanderArrow.Get());
			}));
}


void
FHoudiniEngineDetails::CreateNodeSyncWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs)
{
	if (InHACs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& MainHAC = InHACs[0];
	if (!IsValidWeakPointer(MainHAC))
		return;

	bool bIsNodeSyncComponent = MainHAC->IsA<UHoudiniNodeSyncComponent>();
	if (!bIsNodeSyncComponent)
		return;

	const TWeakObjectPtr<UHoudiniNodeSyncComponent>& MainHNSC = Cast<UHoudiniNodeSyncComponent>(MainHAC);

	//FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_GENERATE);

	auto IsCheckedLiveSyncLambda = [MainHNSC]()
	{
		if (!IsValidWeakPointer(MainHNSC))
			return ECheckBoxState::Unchecked;

		return MainHNSC->GetLiveSyncEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateLiveSyncLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			const TWeakObjectPtr<UHoudiniNodeSyncComponent>& NextHNSC = Cast<UHoudiniNodeSyncComponent>(NextHAC);
			if (!IsValidWeakPointer(NextHNSC))
				continue;

			if (NextHNSC->GetLiveSyncEnabled() == bChecked)
				continue;

			NextHNSC->SetLiveSyncEnabled(bChecked);
			NextHNSC->MarkPackageDirty();
		}
	};

	auto UpdateNodePath = [InHACs](const FString& NewPath)
	{
		UHoudiniEditorNodeSyncSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
		if (!IsValid(HoudiniSubsystem))
			return;

		HAPI_NodeId FetchedNodeId = -1;
		if (!HoudiniSubsystem->ValidateFetchedNodePath(NewPath, FetchedNodeId))
		{
			// Node path invalid!
			HOUDINI_LOG_WARNING(TEXT("Houdini Node Sync - Fetch Failed - The Fetch node path is invalid."));
			FHoudiniEngineUtils::UpdateEditorProperties(true);
			return;
		}

		// Change the node path
		for (auto& NextHAC : InHACs)
		{
			if (!IsValidWeakPointer(NextHAC))
				continue;

			const TWeakObjectPtr<UHoudiniNodeSyncComponent>& NextHNSC = Cast<UHoudiniNodeSyncComponent>(NextHAC);
			if (!IsValidWeakPointer(NextHNSC))
				continue;

			if (NextHNSC->GetFetchNodePath().Equals(NewPath))
				continue;

			NextHNSC->SetFetchNodePath(NewPath);
			NextHNSC->MarkPackageDirty();
			NextHNSC->SetHoudiniAssetState(EHoudiniAssetState::NewHDA);
		}
	};

	auto OnFetchPathTextCommittedLambda = [InHACs, MainHNSC, UpdateNodePath](const FText& Val, ETextCommit::Type TextCommitType)
	{
		if (!IsValidWeakPointer(MainHNSC))
			return;

		FString NewPathStr = Val.ToString();
		UpdateNodePath(NewPathStr);
	};

	auto OnFetchFolderBrowseButtonClickedLambda = [InHACs, MainHNSC, UpdateNodePath]()
	{
		UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
		if (!HoudiniEditorNodeSyncSubsystem)
			return FReply::Handled();

		TSharedRef<SSelectHoudiniPathDialog> Dialog =
			SNew(SSelectHoudiniPathDialog)
			.InitialPath(FText::FromString(MainHNSC->GetFetchNodePath()))
			.TitleText(LOCTEXT("FetchPathDialogTitle", "Select a Houdini node to fetch"))
			.SingleSelection(true);

		if (Dialog->ShowModal() != EAppReturnType::Ok)
			return FReply::Handled();

		// Get the new path and update it
		FString NewPath = Dialog->GetFolderPath().ToString();
		UpdateNodePath(NewPath);

		return FReply::Handled();
	};


	//
	// Fetch node Path Row
	//
	FDetailWidgetRow & FetchNodeRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Fetch Node Path"));
	
	FString FetchPathTooltipString =
		"The path of a node in Houdini that you want to fetch.\ne.g /obj/MyNetwork/Mynode \nThe paths can easily be obtained by using the browse button and selecting them in the dialog.\
				\nAlternatively, you can copy/paste a node to this text box to get its path.\nOnly a single path can be used with per NodeSyncComponent.";

	TSharedRef<SHorizontalBox> FetchNodeRowHorizontalBox = SNew(SHorizontalBox);
	FetchNodeRowHorizontalBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(335.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FetchNodePathLabel", "Houdini Node Path To Fetch (single)"))	
			.ToolTipText(FText::FromString(FetchPathTooltipString))
		]
	];

	FetchNodeRowHorizontalBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(SEditableTextBox)
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		.ToolTipText_Lambda([MainHNSC, FetchPathTooltipString]()
		{
			FString TooltipString = FetchPathTooltipString;

			UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
			if (!HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath.IsEmpty())
			{
				TooltipString += "\n\nCurrent value:\n";
				TooltipString += MainHNSC->GetFetchNodePath().Replace(TEXT(";"), TEXT("\n"));
			}

			return FText::FromString(TooltipString);
		})
		.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Fetch"))
		.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.Text(FText::FromString(MainHNSC->GetFetchNodePath()))
		.OnTextCommitted_Lambda(OnFetchPathTextCommittedLambda)	
	];

	FetchNodeRowHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.IsEnabled(true)
		.Text(LOCTEXT("BrowseButtonText", "..."))
		.ToolTipText(LOCTEXT("FetchBrowseButtonToolTip", "Browse to select a node to fetch..."))
		.OnClicked_Lambda(OnFetchFolderBrowseButtonClickedLambda)
	];
	
	FetchNodeRow.WholeRowWidget.Widget = FetchNodeRowHorizontalBox;

	//
	// Enable LiveSync Row
	//
	FDetailWidgetRow& LiveSyncRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Live Sync"));
	
	TSharedRef<SHorizontalBox> LiveSyncRowHorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SCheckBox> CheckBoxLiveSync;

	LiveSyncRowHorizontalBox->AddSlot()
	[
		SNew(SBox)
		.WidthOverride(160.f)
		[
			SAssignNew(CheckBoxLiveSync, SCheckBox)
			.Content()
			[
				SNew(STextBlock).Text(LOCTEXT("LiveSyncCheckBox", "Enable Live Sync"))
				.ToolTipText(LOCTEXT("LiveSyncCheckBoxToolTip", "When enabled, changes made to the feched node in Houdini will automatically updates this component's outputs."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda(IsCheckedLiveSyncLambda)
			.OnCheckStateChanged_Lambda(OnCheckStateLiveSyncLambda)
		]
	];

	LiveSyncRow.WholeRowWidget.Widget = LiveSyncRowHorizontalBox;
	
	//
	// FETCH Button
	// 
	FDetailWidgetRow& NodeSyncFetchRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Node Sync Fetch"));	
	TSharedRef<SHorizontalBox> NodeSyncFetchRowHorizontalBox = SNew(SHorizontalBox);

	NodeSyncFetchRowHorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(135.0f)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("FetchFromHoudiniLabel", "Fetch the data from Houdini"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InHACs]()
			{
				// Change the node path
				for (auto& NextHAC : InHACs)
				{
					if (!IsValidWeakPointer(NextHAC))
						continue;

					const TWeakObjectPtr<UHoudiniNodeSyncComponent>& NextHNSC = Cast<UHoudiniNodeSyncComponent>(NextHAC);
					if (!IsValidWeakPointer(NextHNSC))
						continue;

					NextHNSC->MarkPackageDirty();
					NextHNSC->SetHoudiniAssetState(EHoudiniAssetState::NewHDA);
				}

				return FReply::Handled();
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(FText::FromString("Fetch"))
			]
		]
	];
	NodeSyncFetchRow.WholeRowWidget.Widget = NodeSyncFetchRowHorizontalBox;

	//
	// FETCH status
	//
	FDetailWidgetRow& NodeSyncStatusRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::FromString("Node Sync Status"));	
	TSharedRef<SHorizontalBox> NodeSyncStatusRowHorizontalBox = SNew(SHorizontalBox);

	NodeSyncStatusRowHorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Top)
	[
		SNew(STextBlock)
		.Justification(ETextJustify::Left)
		.Text_Lambda([MainHNSC]()
		{
			return FText::FromString(MainHNSC->FetchMessage);
		})
		.ColorAndOpacity_Lambda([MainHNSC]()
		{
			FLinearColor StatusColor = UHoudiniEditorNodeSyncSubsystem::GetStatusColor(MainHNSC->FetchStatus);
			return FSlateColor(StatusColor);
		})
		.ToolTipText_Lambda([MainHNSC]()
		{
			return FText::FromString(MainHNSC->FetchMessage);
		})
	];
	NodeSyncStatusRow.WholeRowWidget.Widget = NodeSyncStatusRowHorizontalBox;
}

void
FHoudiniEngineDetails::SetCookFolderPath(
	const FText& InPathText,
	const TWeakObjectPtr<UHoudiniAssetComponent>& InMainHAC, 
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs)
{
	if (!IsValidWeakPointer(InMainHAC))
		return;

	FString NewPathStr = InPathText.ToString();
	if (NewPathStr.StartsWith("Game/"))
	{
		NewPathStr = "/" + NewPathStr;
	}

	FText InvalidPathReason;
	if (!FHoudiniEngineUtils::ValidatePath(NewPathStr, &InvalidPathReason))
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid path: %s"), *InvalidPathReason.ToString());

		FHoudiniEngineUtils::UpdateEditorProperties(true);
		return;
	}

	for (auto& NextHAC : InHACs)
	{
		if (!IsValidWeakPointer(NextHAC))
			continue;

		if (NextHAC->TemporaryCookFolder.Path.Equals(NewPathStr))
			continue;

		if (NextHAC->TemporaryCookFolder.Path == NewPathStr)
			continue;

		NextHAC->TemporaryCookFolder.Path = NewPathStr;
		NextHAC->MarkPackageDirty();
	}
}

void
FHoudiniEngineDetails::SetBakeFolderPath(
	const FText& InPathText,
	const TWeakObjectPtr<UHoudiniAssetComponent>& InMainHAC,
	const TArray<TWeakObjectPtr<UHoudiniAssetComponent>>& InHACs)
{
	if (!IsValidWeakPointer(InMainHAC))
		return;

	FString NewPathStr = InPathText.ToString();
	if (NewPathStr.StartsWith("Game/"))
	{
		NewPathStr = "/" + NewPathStr;
	}

	FText InvalidPathReason;
	if (!FHoudiniEngineUtils::ValidatePath(NewPathStr, &InvalidPathReason))
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid path: %s"), *InvalidPathReason.ToString());

		FHoudiniEngineUtils::UpdateEditorProperties(true);
		return;
	}

	for (auto& NextHAC : InHACs)
	{
		if (!IsValidWeakPointer(NextHAC))
			continue;

		if (NextHAC->BakeFolder.Path.Equals(NewPathStr))
			continue;

		if (NextHAC->BakeFolder.Path == NewPathStr)
			continue;

		NextHAC->BakeFolder.Path = NewPathStr;
		NextHAC->MarkPackageDirty();
	}
}

#undef LOCTEXT_NAMESPACE
