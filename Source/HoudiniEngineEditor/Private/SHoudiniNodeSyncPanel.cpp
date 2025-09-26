/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "SHoudiniNodeSyncPanel.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEditorNodeSyncSubsystem.h"
#include "HoudiniInputDetails.h"
#include "HoudiniInput.h"

#include "SNewFilePathPicker.h"
#include "SSelectFolderPathDialog.h"
#include "SSelectHoudiniPathDialog.h"
#include "SHoudiniNodeTreeView.h"

#include "ActorTreeItem.h"
#include "DetailsViewArgs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SceneOutlinerModule.h"
#include "SlateOptMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"


#define LOCTEXT_NAMESPACE "HoudiniNodeSync"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SHoudiniNodeSyncPanel::SHoudiniNodeSyncPanel()
	: SelectedActors(true, nullptr)
{	

}


SHoudiniNodeSyncPanel::~SHoudiniNodeSyncPanel()
{

}

void
SHoudiniNodeSyncPanel::Construct( const FArguments& InArgs )
{
	bIsAssetEditorPanel = InArgs._IsAssetEditor.Get();

	UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
	TSharedPtr<SHorizontalBox> HoudiniLogoBox;
	TSharedPtr<SExpandableArea> ImportOptionsArea;
	TSharedPtr<SExpandableArea> FetchToWorldOptionsArea;

	SelectionContainer = SNew(SVerticalBox);
	RebuildSelectionView();

	ExportOptionsVBox = SNew(SVerticalBox);
	LandscapeOptionsVBox = SNew(SVerticalBox);
	LandscapeSplineOptionsVBox = SNew(SVerticalBox);

	TSharedPtr<SButton> FetchButton;
	TSharedPtr<SButton> SendWorldButton;

	const FSlateFontInfo BoldFontStyle = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	TSharedPtr<SCheckBox> CheckBoxUseExistingSkeleton;
	TSharedPtr<SCheckBox> CheckBoxUseOutputNodes;
	TSharedPtr<SCheckBox> CheckBoxFetchToWorld;
	TSharedPtr<SCheckBox> CheckBoxReplaceExisting;
	TSharedPtr<SCheckBox> CheckBoxAutoBake;
	TSharedPtr<SCheckBox> CheckBoxSyncWorld;

	auto OnImportFolderBrowseButtonClickedLambda = []()
	{
		UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
		if (!HoudiniEditorNodeSyncSubsystem)
			return FReply::Handled();

		HoudiniEditorNodeSyncSubsystem->CreateSessionIfNeeded();

		TSharedRef<SSelectFolderPathDialog> Dialog =
			SNew(SSelectFolderPathDialog)
			.InitialPath(FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealAssetFolder))
			.TitleText(LOCTEXT("CookFolderDialogTitle", "Select Temporary Cook Folder"));

		if (Dialog->ShowModal() != EAppReturnType::Cancel)
		{
			HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealAssetFolder = Dialog->GetFolderPath().ToString();
		}
		return FReply::Handled();

	};

	auto OnFetchFolderBrowseButtonClickedLambda = []()
	{
		UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
		if (!HoudiniEditorNodeSyncSubsystem)
			return FReply::Handled();

		TSharedRef<SSelectHoudiniPathDialog> Dialog =
			SNew(SSelectHoudiniPathDialog)
			.InitialPath(FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath))
			.TitleText(LOCTEXT("FetchPathDialogTitle", "Select Houdini nodes to fetch"))
			.SingleSelection(false);

		if (Dialog->ShowModal() != EAppReturnType::Cancel)
		{
			HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath = Dialog->GetFolderPath().ToString();
		}

		return FReply::Handled();
	};

	// Get the session status
	auto GetSessionSyncStatusAndColor = [](FString& OutStatus, FLinearColor& OutStatusColor)
	{
		OutStatus = TEXT("Session Status");
		OutStatusColor = FLinearColor::Red;

		FHoudiniEngine::Get().GetSessionStatusAndColor(OutStatus, OutStatusColor);

		// For valid state, check if session sync is enabled
		if (OutStatusColor != FLinearColor::Red && OutStatusColor != FLinearColor::White)
		{
			bool bSessionSyncON = FHoudiniEngine::Get().IsSessionSyncEnabled();
			if (!bSessionSyncON)
			{
				// Append a warning and change the color to orange
				OutStatus += TEXT(" - Session Sync OFF");
				OutStatusColor = FLinearColor(1.0f, 0.647f, 0.0f);
			}
			else
			{
				// Append a warning and change the color to orange
				OutStatus += TEXT(" - Session Sync READY");
			}
		}
	};

	// Tool for the fetch path
	FString FetchPathTooltipString =
		"The path of the nodes in Houdini that you want to fetch.\ne.g /obj/MyNetwork/Mynode \nThe paths can easily be obtained by using the browse button and selecting them in the dialog.\
		\nAlternatively, you can copy/paste a node to this text box to get its path.\nMultiple paths can be separated by using ; delimiters.";
	
	TSharedPtr<SVerticalBox> NodeSyncVerticalBox;
	ChildSlot
	[
		SNew(SScrollBox)
		.Orientation(Orient_Vertical)
		+ SScrollBox::Slot()
		[
			SAssignNew(NodeSyncVerticalBox, SVerticalBox)
		]
	];
	
	//------------------------------------------------------------------------------------------
	// Session status
	//------------------------------------------------------------------------------------------
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.Padding(15.0, 0.0, 0.0, 0.0)
	.AutoHeight()
	[
		SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.MinDesiredWidth(500.0)
		[
			SAssignNew(HoudiniLogoBox, SHorizontalBox)
		]
	];
	
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(15.0, 0.0, 15.0, 15.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Top)
		[
			SNew(STextBlock)
			.Justification(ETextJustify::Left)
			.Text_Lambda([GetSessionSyncStatusAndColor]()
			{
				FString StatusString;
				FLinearColor StatusColor;
				GetSessionSyncStatusAndColor(StatusString, StatusColor);
				return FText::FromString(StatusString);
				})
				.ColorAndOpacity_Lambda([GetSessionSyncStatusAndColor]()
					{
						FString StatusString;
						FLinearColor StatusColor;
						GetSessionSyncStatusAndColor(StatusString, StatusColor);
						return FSlateColor(StatusColor);
					})
		]
	];

	//------------------------------------------------------------------------------------------
	// FETCH from Houdini
	//------------------------------------------------------------------------------------------
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(10.0, 20.0, 0.0, 15.0)
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[
			SNew(STextBlock)
			.Font(BoldFontStyle)
			.Text(LOCTEXT("FetchLabel", "FETCH from Houdini"))
		]
	];

	// HOUDINI NODE PATH
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(10.0, 0.0, 0.0, 5.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		//.MaxWidth(HAPI_UNREAL_DESIRED_SETTINGS_ROW_FULL_WIDGET_WIDTH)
		[
			SNew(SBox)
			.WidthOverride(335.0f)
			//.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(FText::FromString(FetchPathTooltipString))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FetchNodePathLabel", "Houdini Node Paths To Fetch"))
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		//.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText_Lambda([FetchPathTooltipString]()
			{
				FString TooltipString = FetchPathTooltipString;
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				if (!HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath.IsEmpty())
				{
					TooltipString += "\n\nCurrent value:\n";
					TooltipString += HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath.Replace(TEXT(";"), TEXT("\n"));
				}

				return FText::FromString(TooltipString);
			})
			.HintText(LOCTEXT("NodePathLabel", "Houdini Node Paths To Fetch"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath);
			})
			.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FString NewPathStr = Val.ToString();

				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.FetchNodePath = NewPathStr;
			})
		]

		+ SHorizontalBox::Slot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			//.ContentPadding(FMargin(6.0, 2.0))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.IsEnabled(true)
			.Text(LOCTEXT("BrowseButtonText", "..."))
			.ToolTipText(LOCTEXT("FetchBrowseButtonToolTip", "Browse to select the nodes to fetch..."))
			.OnClicked_Lambda(OnFetchFolderBrowseButtonClickedLambda)
		]
	];

	// USE OUTPUT NODE
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(10.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(SBox)
		.WidthOverride(160.f)
		[
			SAssignNew(CheckBoxUseOutputNodes, SCheckBox)
			.Content()
			[
				SNew(STextBlock).Text(LOCTEXT("UseOutputNodes", "Use Output Nodes"))
				.ToolTipText(LOCTEXT("UseOutputNodesToolTip", "If enabled, output nodes will be prefered over the display flag when fetching a node's data."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				return HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bUseOutputNodes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
			{
				const bool bNewState = (NewState == ECheckBoxState::Checked);
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bUseOutputNodes = bNewState;
			})
		]
	];

	// REPLACE EXISTING
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(10.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(SBox)
		.WidthOverride(160.f)
		[
			SAssignNew(CheckBoxReplaceExisting, SCheckBox)
			.Content()
			[
				SNew(STextBlock).Text(LOCTEXT("ReplaceExisting", "Replace Existing Assets/Actors"))
				.ToolTipText(LOCTEXT("ReplaceExisitngToolTip", "If enabled, existing Assets or Actors will be overwritten and replaced by the newly fetched data."))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				return HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bReplaceExisting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
			{
				const bool bNewState = (NewState == ECheckBoxState::Checked);
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bReplaceExisting = bNewState;
			})
		]
	];

	// UNREAL ASSET NAME
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(10.0, 0.0, 0.0, 5.0)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.WidthOverride(335.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UnrealAssetName", "Unreal Asset Name"))
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		//.AutoWidth()
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH + 45)
			.ToolTipText(LOCTEXT("UnrealAssetNameTooltip",
				"Name to be given to the fetched data in Unreal.\nLeaving this field empty will use the node name for the unreal names."))
			.HintText(LOCTEXT("UnrealAssetNameLabel", "Name the of Asset in Unreal"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealAssetName);
			})
			.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FString NewPathStr = Val.ToString();

				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealAssetName = NewPathStr;
			})
		]
	];

	// UNREAL ASSET FOLDER
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	.Padding(10.0, 0.0, 0.0, 5.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.WidthOverride(335.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UnrealAssetFolder", "Unreal Asset Import Folder"))
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT("UnrealAssetFolderTooltip","Path to the project folder that will contain the generated assets in unreal"))
			.HintText(LOCTEXT("UnrealAssetFolderLabel", "Unreal Asset Import Folder"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealAssetFolder);
			})
			.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FString NewPathStr = Val.ToString();

				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealAssetFolder = NewPathStr;
			})
		]
		+ SHorizontalBox::Slot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			//.ContentPadding(FMargin(6.0, 2.0))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.IsEnabled(true)
			.Text(LOCTEXT("BrowseButtonText", "..."))
			.ToolTipText(LOCTEXT("ImportFolderBrowseButtonToolTip", "Browse to select the Import Asset folder..."))
			.OnClicked_Lambda(OnImportFolderBrowseButtonClickedLambda)
		]
		/*
		+ SHorizontalBox::Slot()
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
			.OnClicked_Lambda(OnImportFolderBrowseButtonClickedLambda)
		];
		*/
	];

	if (!bIsAssetEditorPanel)
	{
		// FETCH TO WORLD?
		NodeSyncVerticalBox->AddSlot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0f, 0.0f, 0.0f, 5.0f)
		[
			SNew(SBox)
			.WidthOverride(160.f)
			//.IsEnabled(false)
			[
				SAssignNew(CheckBoxFetchToWorld, SCheckBox)
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("FetchToWorld", "Fetch to World Outliner"))
					.ToolTipText(LOCTEXT("FetchToWorldToolTip", "If enabled, the data fetched from Houdini will be instantiated as an Actor in the current level."))
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					return HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bFetchToWorld ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					const bool bNewState = (NewState == ECheckBoxState::Checked);
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bFetchToWorld = bNewState;
				})
			]
		];

		// FETCH TO WORLD OPTIONS
		NodeSyncVerticalBox->AddSlot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			SAssignNew(FetchToWorldOptionsArea, SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FetchToWorldOptions", "Fetch to World Options"))
				.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
			.BodyContent()
			[
				SNew(SVerticalBox)
				// AutoBake?
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(SBox)
					.WidthOverride(160.f)
					[
						SAssignNew(CheckBoxAutoBake, SCheckBox)
						.Content()
						[
							SNew(STextBlock).Text(LOCTEXT("AutoBake", "Auto Bake"))
							.ToolTipText(LOCTEXT("AutoBakeToolTip", "If enabled, output data fetched to world will automatically be baked. If disabled, they will be created as temporary cooked data, and attached to a Houdini Node Sync Component."))
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
						.IsChecked_Lambda([]()
						{
							UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
							return HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bAutoBake ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
						{
							const bool bNewState = (NewState == ECheckBoxState::Checked);
							UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
							HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bAutoBake = bNewState;
						})
					]
				]
				// UNREAL ACTOR NAME
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0, 0.0, 0.0, 5.0)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
						//.IsEnabled(false)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnrealActorName", "Unreal Actor Name"))
						]
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					[
						SNew(SEditableTextBox)
						//.IsEnabled(false)
						.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.ToolTipText(LOCTEXT("UnrealActorNameTooltip", "Name of the generated Actor in unreal"))
						.HintText(LOCTEXT("UnrealActorNameLabel", "Unreal Actor Name"))
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([]()
						{
							UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
							return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealActorName);
						})
						.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
						{
							FString NewPathStr = Val.ToString();

							UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
							HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealActorName = NewPathStr;
						})
					]
				]

				// UNREAL ACTOR FOLDER
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0, 0.0, 0.0, 5.0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
						//.IsEnabled(false)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnrealActorFolderLabel", "World Outliner Folder"))
						]
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					[
						SNew(SEditableTextBox)
						//.IsEnabled(false)
						.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.ToolTipText(LOCTEXT("UnrealActorFolderTooltip","Path to a world outliner folder that will contain the created Actor"))
						.HintText(LOCTEXT("UnrealActorFolderLabel", "Unreal Actor World Outliner Folder"))
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([]()
						{
							UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
							return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealActorFolder);
						})
						.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
						{
							FString NewPathStr = Val.ToString();

							UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
							HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.UnrealActorFolder = NewPathStr;
						})
					]
				]
			]
		];
	}

	/*
	// Build Settings
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Center)
	.AutoHeight()
	.Padding(5.0, 5.0, 5.0, 5.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			MakeMBSDetailsView()
		]
	];

	// MeshGenerationProeprties
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Center)
	.AutoHeight()
	.Padding(5.0, 5.0, 5.0, 5.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			MakeHSMGPDetailsView()
		]
	];
	*/

	// FETCH BUTTON
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Center)
	.AutoHeight()
	.Padding(5.0, 5.0, 5.0, 5.0)
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(135.0f)
			[
				SAssignNew(FetchButton, SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("FetchFromHoudiniLabel", "Fetch Asset From Houdini"))
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					HoudiniEditorNodeSyncSubsystem->FetchFromHoudini();
					return FReply::Handled();
				})
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString("Fetch"))
				]
			]
		]
	];

	// Last FETCH status
	NodeSyncVerticalBox->AddSlot()
	.HAlign(HAlign_Center)
	.AutoHeight()
	.Padding(15.0, 0.0, 15.0, 15.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Top)
		[
			SNew(STextBlock)
			.Justification(ETextJustify::Left)
			.Text_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();					
				return FText::FromString(HoudiniEditorNodeSyncSubsystem->FetchStatusMessage);
			})
			.ColorAndOpacity_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();					
				FLinearColor StatusColor = UHoudiniEditorNodeSyncSubsystem::GetStatusColor(HoudiniEditorNodeSyncSubsystem->LastFetchStatus);					
				return FSlateColor(StatusColor);
			})
			.ToolTipText_Lambda([]()
			{
				UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
				if(!HoudiniEditorNodeSyncSubsystem->FetchStatusDetails.IsEmpty())
					return FText::FromString(HoudiniEditorNodeSyncSubsystem->FetchStatusDetails);
				else
					return FText::FromString(HoudiniEditorNodeSyncSubsystem->FetchStatusMessage);
			})
		]
	];


	//------------------------------------------------------------------------------------------
	// SEND to Houdini
	//------------------------------------------------------------------------------------------
	if(!bIsAssetEditorPanel)
	{
		NodeSyncVerticalBox->AddSlot()			
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0, 20.0, 0.0, 15.0)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Font(BoldFontStyle)
				.Text(LOCTEXT("SendLabel", "SEND to Houdini"))
			]
		];

		// Houdini Node Path
		NodeSyncVerticalBox->AddSlot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(335.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SendNodePathLabel", "Houdini Node Path To Send To"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SEditableTextBox)
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.ToolTipText(LOCTEXT("HoudiniNodePathTooltip",
					"The path of the node in Houdini that will receive the sent data.  e.g /obj/UnrealContent "))
				.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Send To"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.SendNodePath);
				})
				.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
				{
					FString NewPathStr = Val.ToString();

					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.SendNodePath = NewPathStr;
				})
			]
		];
			
		// Export Options
		NodeSyncVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			ExportOptionsVBox.ToSharedRef()
		];

		// Landscape Options
		NodeSyncVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			LandscapeOptionsVBox.ToSharedRef()
		];

		// Landscape Spline Options
		NodeSyncVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			LandscapeSplineOptionsVBox.ToSharedRef()
		];

		NodeSyncVerticalBox->AddSlot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0f, 0.0f, 0.0f, 5.0f)
		[
			SNew(SBox)
			.WidthOverride(160.f)
			[
				SAssignNew(CheckBoxSyncWorld, SCheckBox)
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("SyncWorld", "Sync World Inputs"))
					.ToolTipText(LOCTEXT("SyncWorldToolTip", "If enabled, actors sent to Houdini will be automatically updated in Houdini if they are modified in the level."))
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					return HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bSyncWorldInput ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					const bool bNewState = (NewState == ECheckBoxState::Checked);
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bSyncWorldInput = bNewState;
					if (bNewState)
						HoudiniEditorNodeSyncSubsystem->StartTicking();
					else
						HoudiniEditorNodeSyncSubsystem->StopTicking();
				})
			]
		];

		
		// SEND Button
		NodeSyncVerticalBox->AddSlot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(5.0, 5.0, 5.0, 5.0)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.WidthOverride(135.0f)
				[
					SAssignNew(SendWorldButton, SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.ToolTipText(LOCTEXT("SendWorldToHoudiniLabel", "Send World Selection To Houdini"))
					.Visibility(EVisibility::Visible)
					.OnClicked_Lambda([]()
					{
						UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
						HoudiniEditorNodeSyncSubsystem->SendWorldSelection();
						return FReply::Handled();
					})
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromString("Send"))
					]
				]
			]
		];

		// Last SEND status
		NodeSyncVerticalBox->AddSlot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(15.0, 0.0, 15.0, 15.0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Left)
				.Text_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();					
					return FText::FromString(HoudiniEditorNodeSyncSubsystem->SendStatusMessage);
				})
				.ColorAndOpacity_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();					
					FLinearColor StatusColor = UHoudiniEditorNodeSyncSubsystem::GetStatusColor(HoudiniEditorNodeSyncSubsystem->LastSendStatus);					
					return FSlateColor(StatusColor);
				})
				.ToolTipText_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					if (!HoudiniEditorNodeSyncSubsystem->SendStatusDetails.IsEmpty())
						return FText::FromString(HoudiniEditorNodeSyncSubsystem->SendStatusDetails);
					else
						return FText::FromString(HoudiniEditorNodeSyncSubsystem->SendStatusMessage);
				})
			]
		];

		// World IN UI
		NodeSyncVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			SelectionContainer.ToSharedRef()
		];

		// World IN UI
		NodeSyncVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(5.0, 5.0, 5.0, 5.0)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("UpdateAll", "Update All Sent Data"))
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					HoudiniEditorNodeSyncSubsystem->UpdateAllSelection();
					return FReply::Handled();
				})
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString("Update All"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.Padding(5.0, 5.0, 5.0, 5.0)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("DeleteAll", "Delete All Sent Data"))
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([]()
				{
					UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
					HoudiniEditorNodeSyncSubsystem->DeleteAllSelection();
					return FReply::Handled();
				})
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString("Delete All"))
				]
			]
		];
	}
	
	// Get the NodeSync inputs from the editor subsystem
	UHoudiniInput* NodeSyncWorldInput = nullptr;
	if (!HoudiniEditorNodeSyncSubsystem || !HoudiniEditorNodeSyncSubsystem->GetNodeSyncWorldInput(NodeSyncWorldInput))
		return;

	UHoudiniInput* NodeSyncCBInput = nullptr;
	if (!HoudiniEditorNodeSyncSubsystem || !HoudiniEditorNodeSyncSubsystem->GetNodeSyncCBInput(NodeSyncCBInput))
		return;

	// ... and create an input array ...
	TArray<TWeakObjectPtr<UHoudiniInput>> NodeSyncInputs;
	NodeSyncInputs.Add(NodeSyncWorldInput);
	NodeSyncInputs.Add(NodeSyncCBInput);

	// ... so we can reuse the input UI code
	FHoudiniInputDetails::AddExportOptions(ExportOptionsVBox.ToSharedRef(), NodeSyncInputs);
	FHoudiniInputDetails::AddLandscapeOptions(LandscapeOptionsVBox.ToSharedRef(), NodeSyncInputs);
	FHoudiniInputDetails::AddLandscapeSplinesOptions(LandscapeSplineOptionsVBox.ToSharedRef(), NodeSyncInputs);

	// Handle the Houdini logo box
	TSharedPtr<SImage> Image;
	HoudiniLogoBox->AddSlot()
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

	// Skip drawing the icon if the icon image is not loaded correctly.
	TSharedPtr<FSlateDynamicImageBrush> IconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();
	if (IconBrush.IsValid())
	{
		Image->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda(
					[IconBrush](){return IconBrush.Get();}
		)));
	}
}


FMenuBuilder
SHoudiniNodeSyncPanel::Helper_CreateSelectionWidget()
{
	auto OnShouldFilter = [](const AActor* const Actor)
	{
		if (!IsValid(Actor))
			return false;

		UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
		if (!HoudiniEditorNodeSyncSubsystem)
			return false;

		// Get the NodeSync input from the editor subsystem
		UHoudiniInput* NodeSyncInput = nullptr;
		if (!HoudiniEditorNodeSyncSubsystem || !HoudiniEditorNodeSyncSubsystem->GetNodeSyncWorldInput(NodeSyncInput))
			return false;

		const TArray<TObjectPtr<UHoudiniInputObject>>* InputObjects = NodeSyncInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
		if (!InputObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurInputObject : *InputObjects)
		{
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


	auto OnSelected = [](AActor* Actor)
	{
		// Do Nothing
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldInputSentData", "Sent Data"));
	{
		FSceneOutlinerModule& SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>(TEXT("SceneOutliner"));
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
SHoudiniNodeSyncPanel::RebuildSelectionView()
{
	// Clear the Box that contains the current sent data
	SelectionContainer->ClearChildren();

	// Recreate the actor picker with updated contents
	FMenuBuilder MenuBuilder = Helper_CreateSelectionWidget();
	SelectionContainer->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		MenuBuilder.MakeWidget()
	];
}


/*
// Create the widget that displays the Mesh Build Settings struct
TSharedRef<SWidget>
SHoudiniNodeSyncPanel::MakeMBSDetailsView()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	DetailsViewArgs.bShowCustomFilterOption = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowSearch = false;

	FStructureDetailsViewArgs StructDetailsArgs;	
	MBS_DetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructDetailsArgs, MBS_Ptr);

	return MBS_DetailsView->GetWidget().ToSharedRef();
}


// Create the widget that displays the Houdini Static Mesh Generation Settings struct
TSharedRef<SWidget>
SHoudiniNodeSyncPanel::MakeHSMGPDetailsView()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	DetailsViewArgs.bShowCustomFilterOption = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowSearch = false;

	FStructureDetailsViewArgs StructDetailsArgs;
	HSMGP_DetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructDetailsArgs, HSMGP_Ptr);

	return HSMGP_DetailsView->GetWidget().ToSharedRef();
}
*/



END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
