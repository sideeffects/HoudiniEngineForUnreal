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
#include "HoudiniEditorSubsystem.h"
#include "HoudiniInputDetails.h"
#include "HoudiniInput.h"

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
	TSharedPtr<SHorizontalBox> Box;
	TSharedPtr<SExpandableArea> OptionsArea;

	ExportOptionsVBox = SNew(SVerticalBox);
	LandscapeOptionsVBox = SNew(SVerticalBox);
	LandscapeSplineOptionsVBox = SNew(SVerticalBox);

	TSharedPtr<SButton> FetchButton;
	auto OnFetchButtonClickedLambda = []()
	{
		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->Fetch();
		return FReply::Handled();
	};

	TSharedPtr<SButton> SendWorldButton;
	auto OnSendWorldButtonClickedLambda = []()
	{
		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->SendWorldSelection();
		return FReply::Handled();
	};

	auto OnFetchNodePathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSyncOptions.FetchNodePath = NewPathStr;
	};

	auto OnSendNodePathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSyncOptions.SendNodePath = NewPathStr;
	};
	

	auto OnUnrealAssetTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSyncOptions.UnrealAssetName = NewPathStr;
	};

	/*
	auto OnUnrealDirectoryChangedLambda = [](const FString& Directory)
	{
		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSyncOptions.DirectoryPath.Path = Directory;
	};
	*/

	auto OnUnrealPathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSyncOptions.UnrealPathName = NewPathStr;
	};

	auto OnSkeletonPathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSyncOptions.SkeletonAssetPath = NewPathStr;
	};

	const FSlateFontInfo BoldFontStyle = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	TSharedPtr<SCheckBox> CheckBoxUseExistingSkeleton;
	TSharedPtr<SCheckBox> CheckBoxUseOutputNodes;

	/*
	UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	FDirectoryPath DirectoryPath = HoudiniEditorSubsystem->NodeSyncOptions.DirectoryPath;
	*/
	ChildSlot
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
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			[
				SAssignNew(Box, SHorizontalBox)
			]			
		]
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
				.Text_Lambda([]()
				{
					FString StatusString = TEXT("Session Status");
					FLinearColor StatusColor;
					//GetSessionStatusAndColor(StatusString, StatusColor);
					bool result = FHoudiniEngine::Get().GetSessionStatusAndColor(StatusString, StatusColor);
					return FText::FromString(StatusString);
				})
				.ColorAndOpacity_Lambda([]()
				{
					FString StatusString;
					FLinearColor StatusColor = FLinearColor::Red;
					bool result = FHoudiniEngine::Get().GetSessionStatusAndColor(StatusString, StatusColor);
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
				//.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
				.Font(BoldFontStyle)
				.Text(LOCTEXT("FetchLabel", "FETCH from Houdini"))
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0f, 0.0, 0.0, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(335.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NodePathLabel", "Houdini Node Path To Fetch"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SEditableTextBox)
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.ToolTipText(LOCTEXT("HoudiniNodePathTooltip",
					"The path of the node in Houdini.\ne.g /obj/MyNetwork/Mynode \nThe path can easily be obtain by copy/pasting the node to this text box."))
				.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Fetch"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([]()
				{
					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
					return FText::FromString(HoudiniEditorSubsystem->NodeSyncOptions.FetchNodePath);
				})
				.OnTextCommitted_Lambda(OnFetchNodePathTextCommittedLambda)
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0f, 0.0f, 0.0f, 10.0f)
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
					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
					return HoudiniEditorSubsystem->NodeSyncOptions.UseOutputNodes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					const bool bNewState = (NewState == ECheckBoxState::Checked);
					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
					HoudiniEditorSubsystem->NodeSyncOptions.UseOutputNodes = bNewState;
				})
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 0.0)
		[
			SNew(SHorizontalBox)
			//.AutoWidth()
			//.MaxWidth(300.0f)
			//.Padding(0, 60, 0, 10)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(335.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UnrealAssetLabel", "Name of Asset In Unreal"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			//.FillWidth(0.60f)
			//.FillWidth(1)
			//.MaxWidth(10)
			//.FillWidth(0.9f)
			[
				SNew(SEditableTextBox)
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.ToolTipText(LOCTEXT("UnrealAssetTooltip",
					"What to name the asset in unreal"))
				.HintText(LOCTEXT("UnrealAssetLabel", "Name of Asset In Unreal"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([]()
				{
					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
					return FText::FromString(HoudiniEditorSubsystem->NodeSyncOptions.UnrealAssetName);
				})
				.OnTextCommitted_Lambda(OnUnrealAssetTextCommittedLambda)
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 0.0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(335.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UnrealPathLabel", "Path of Asset In Unreal"))
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				/*
				// Directory picker cant be limited to just project folders...
				SNew(SDirectoryPicker)
				.Directory(DirectoryPath.Path)
				.OnDirectoryChanged_Lambda(OnUnrealDirectoryChangedLambda)
				*/
				SNew(SEditableTextBox)
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.ToolTipText(LOCTEXT("UnrealPathTooltip","Path to the asset in unreal"))
				.HintText(LOCTEXT("UnrealPathLabel", "Path of Asset In Unreal"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([]()
				{
					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
					return FText::FromString(HoudiniEditorSubsystem->NodeSyncOptions.UnrealPathName);
				})
				.OnTextCommitted_Lambda(OnUnrealPathTextCommittedLambda)
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SAssignNew(OptionsArea, SExpandableArea)
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
			//.BorderImage_Lambda([this]() { return ConcertFrontendUtils::GetExpandableAreaBorderImage(*DetailsArea); })
			.BodyBorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BodyBorderBackgroundColor(FLinearColor::White)
			//.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(OnDetailsAreaExpansionChanged))
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
				[
					SNew(SBox)
					.WidthOverride(335.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SKOptionsLabel", "Skeletal Mesh Options"))
					]
					/*.Padding(FMargin(0, 2, 0, 2))
					[
						SAssignNew(Grid, SGridPanel)
					]*/
				]
				+ SScrollBox::Slot()
				.Padding(0.0f, 0.0f, 0.0f, 3.5f)
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
								UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
								return HoudiniEditorSubsystem->NodeSyncOptions.OverwriteSkeleton ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
							{
								const bool bNewState = (NewState == ECheckBoxState::Checked);
								UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
								HoudiniEditorSubsystem->NodeSyncOptions.OverwriteSkeleton = bNewState;
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
							UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
							return FText::FromString(HoudiniEditorSubsystem->NodeSyncOptions.SkeletonAssetPath);
						})
						.OnTextCommitted_Lambda(OnSkeletonPathTextCommittedLambda)
					]
				]
			]
		]
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
					.OnClicked_Lambda(OnFetchButtonClickedLambda)
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromString("Fetch"))
					]
				]
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
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(335.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NodePathLabel", "Houdini Node Path To Send To"))
				]
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Right)
			[
				SNew(SEditableTextBox)
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.ToolTipText(LOCTEXT("HoudiniNodePathTooltip",
				"The path of the node in Houdini to Send too.  e.g /obj/UnrealContent "))
				.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Send To"))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([]()
				{
					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
					return FText::FromString(HoudiniEditorSubsystem->NodeSyncOptions.SendNodePath);
				})
				.OnTextCommitted_Lambda(OnSendNodePathTextCommittedLambda)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			ExportOptionsVBox.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			LandscapeOptionsVBox.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0, 0.0, 0.0, 5.0)
		[
			LandscapeSplineOptionsVBox.ToSharedRef()
		]
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
					.OnClicked_Lambda(OnSendWorldButtonClickedLambda)
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromString("Send"))
					]
				]
			]
		]
	];
	
	// Get the NodeSync input from the editor subsystem
	UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	UHoudiniInput* NodeSyncInput = nullptr;
	if (!HoudiniEditorSubsystem || !HoudiniEditorSubsystem->GetNodeSyncInput(NodeSyncInput))
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
