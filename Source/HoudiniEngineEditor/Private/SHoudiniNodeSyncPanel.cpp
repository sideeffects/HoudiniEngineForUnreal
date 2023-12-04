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

#include "PropertyCustomizationHelpers.h"
#include "SlateOptMacros.h"

#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"

//#include "Widgets/Docking/SDockTab.h"
//#include "Widgets/Layout/SBox.h"
//#include "Widgets/Text/STextBlock.h"
//#include "Widgets/SBoxPanel.h"
//#include "Widgets/Images/SImage.h"
//#include "Widgets/Input/SMultiLineEditableTextBox.h"
//#include "Widgets/Layout/SUniformGridPanel.h"
//#include "Brushes/SlateImageBrush.h"
//#include "Widgets/Input/SComboBox.h"


#define LOCTEXT_NAMESPACE "HoudiniNodeSync"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SHoudiniNodeSyncPanel::SHoudiniNodeSyncPanel()
{
}


SHoudiniNodeSyncPanel::~SHoudiniNodeSyncPanel()
{

}

void
SHoudiniNodeSyncPanel::Construct( const FArguments& InArgs )
{
	TSharedPtr<SHorizontalBox> HoudiniLogoBox;
	TSharedPtr<SExpandableArea> ImportOptionsArea;
	TSharedPtr<SExpandableArea> FetchToWorldOptionsArea;
	
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
		}
	};	
	
	ChildSlot
	[
		SNew(SScrollBox)
		.Orientation(Orient_Vertical)
		+ SScrollBox::Slot()
		[
			//------------------------------------------------------------------------------------------
			// Session status
			//------------------------------------------------------------------------------------------
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
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
			]
			+ SVerticalBox::Slot()
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
			]

			//------------------------------------------------------------------------------------------
			// FETCH from Houdini
			//------------------------------------------------------------------------------------------
			+ SVerticalBox::Slot()
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
			]

			// HOUDINI NODE PATH
			+ SVerticalBox::Slot()
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
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FetchNodePathLabel", "Houdini Node Path To Fetch"))
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				//.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					.ToolTipText(LOCTEXT("HoudiniNodePathTooltip",
						"The path of the node in Houdini.\ne.g /obj/MyNetwork/Mynode \nThe path can easily be obtain by copy/pasting the node to this text box."))
					.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Fetch"))
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
			]

			// USE OUTPUT NODE
			+ SVerticalBox::Slot()
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
			]

			// REPLACE EXISTING
			+ SVerticalBox::Slot()
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
			]

			// UNREAL ASSET NAME
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
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UnrealAssetName", "Unreal Asset Name"))
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					.ToolTipText(LOCTEXT("UnrealAssetNameTooltip",
						"What to name the asset in unreal"))
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
			]

			// UNREAL ASSET FOLDER
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
			]

			// FETCH TO WORLD?
			+ SVerticalBox::Slot()
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
			]

			// FETCH TO WORLD OPTIONS
			+ SVerticalBox::Slot()
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
			]
			/*
			// IMPORT OPTIONS
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoHeight()
			.Padding(10.0, 0.0, 0.0, 5.0)
			[
				SAssignNew(ImportOptionsArea, SExpandableArea)
				.InitiallyCollapsed(true)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OptionDetails", "Import Options"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				.BodyContent()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Vertical)
					+ SScrollBox::Slot()
					.Padding(10.0, 0.0, 0.0, 5.0)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SKOptionsLabel", "Skeletal Mesh Options"))
						]
					]
					+ SScrollBox::Slot()
					.Padding(10.0, 0.0, 0.0, 5.0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						[
							SNew(SBox)
							.WidthOverride(160.f)
							[
								SAssignNew(CheckBoxUseExistingSkeleton, SCheckBox)
								.Content()
								[
									SNew(STextBlock).Text(LOCTEXT("OverwriteSkeletonLabel", "Use Existing Skeleton"))
									.ToolTipText(LOCTEXT("OverwriteSkeletonToolTip", "Use Existing Skeleton"))
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								]
								.IsChecked_Lambda([]()
								{
									UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
									return HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bOverwriteSkeleton ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
								})
								.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
								{
									const bool bNewState = (NewState == ECheckBoxState::Checked);
									UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
									HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.bOverwriteSkeleton = bNewState;
								})
							]
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						[
							SNew(SEditableTextBox)
							.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
							.ToolTipText(LOCTEXT("ExistingSkeletonTooltip", "Path to the skelton asset in unreal"))
							.HintText(LOCTEXT("ExistingSkeletonLabel", "Path to skeleton asset In Unreal"))
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.Text_Lambda([]()
							{
								UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
								return FText::FromString(HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.SkeletonAssetPath);
							})
							.OnTextCommitted_Lambda([](const FText& Val, ETextCommit::Type TextCommitType)
							{
								FString NewPathStr = Val.ToString();

								UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
								HoudiniEditorNodeSyncSubsystem->NodeSyncOptions.SkeletonAssetPath = NewPathStr;
							})
						]
					]
				]
			]
			*/

			// FETCH BUTTON
			+ SVerticalBox::Slot()
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
							HoudiniEditorNodeSyncSubsystem->Fetch();
							return FReply::Handled();
						})
						.Content()
						[
							SNew(STextBlock)
							.Text(FText::FromString("Fetch"))
						]
					]
				]
			]

			// Last FETCH status
			+ SVerticalBox::Slot()
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
			]


			//------------------------------------------------------------------------------------------
			// SEND to Houdini
			//------------------------------------------------------------------------------------------
			+ SVerticalBox::Slot()
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
			]

			// Houdini Node Path
			+ SVerticalBox::Slot()
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
			]

			// Export Options
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.0, 0.0, 0.0, 5.0)
			[
				ExportOptionsVBox.ToSharedRef()
			]

			// Landscape Options
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.0, 0.0, 0.0, 5.0)
			[
				LandscapeOptionsVBox.ToSharedRef()
			]

			// Landscape Spline Options
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.0, 0.0, 0.0, 5.0)
			[
				LandscapeSplineOptionsVBox.ToSharedRef()
			]

		
			// SEND Button
			+ SVerticalBox::Slot()
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
			]

			// Last SEND status
			+ SVerticalBox::Slot()
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
			]
		]
	];
	
	// Get the NodeSync input from the editor subsystem
	UHoudiniEditorNodeSyncSubsystem* HoudiniEditorNodeSyncSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
	UHoudiniInput* NodeSyncInput = nullptr;
	if (!HoudiniEditorNodeSyncSubsystem || !HoudiniEditorNodeSyncSubsystem->GetNodeSyncInput(NodeSyncInput))
		return;

	bool bUselessBool;
	NodeSyncInput->SetInputType(EHoudiniInputType::World, bUselessBool);

	// ... and a fake input array ...
	TArray<TWeakObjectPtr<UHoudiniInput>> FakeInputs;
	FakeInputs.Add(NodeSyncInput);

	// ... so we can reuse the input UI code
	FHoudiniInputDetails::AddExportOptions(ExportOptionsVBox.ToSharedRef(), FakeInputs);
	FHoudiniInputDetails::AddLandscapeOptions(LandscapeOptionsVBox.ToSharedRef(), FakeInputs);
	FHoudiniInputDetails::AddLandscapeSplinesOptions(LandscapeSplineOptionsVBox.ToSharedRef(), FakeInputs);

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
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
