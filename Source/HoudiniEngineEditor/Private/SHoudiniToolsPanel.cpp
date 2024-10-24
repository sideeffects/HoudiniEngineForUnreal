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

#include "SHoudiniToolsPanel.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ActorFactories/ActorFactory.h"
#include "Engine/Selection.h"
#include "IContentBrowserSingleton.h"
#include "EditorViewportClient.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetToolsModule.h"
#include "EditorDirectories.h"
#include "EditorReimportHandler.h"
#include "FileHelpers.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditorSettings.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniToolsEditor.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolsPackageAssetFactory.h"
#include "LandscapeProxy.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "SWarningOrErrorBox.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "HoudiniToolsRuntimeUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniPresetActorFactory.h"
#include "HoudiniToolTypesEditor.h"

#define LOCTEXT_NAMESPACE "HoudiniTools"

const FString SHoudiniToolsPanel::SettingsIniSection = TEXT("HoudiniEngine");

struct FHoudiniToolsPanelUtils
{
	static FHoudiniToolsEditor& GetHoudiniTools() { return FHoudiniEngineEditor::Get().GetHoudiniTools(); }

	static FString GetAbsoluteGameContentDir();
	
	static FString GetLastDirectory(const bool bRelativeToGameContentDir, const ELastDirectory::Type LastDirectory = ELastDirectory::GENERIC_IMPORT);

	// Check whether the path adheres to the RelativeToGameContentDir restriction.
	// This does not check whether the path actually exists.
	static bool IsValidPath(const FString& AbsolutePath, bool bRelativeToGameContentDir, FText* OutReason=nullptr);
};


FString
FHoudiniToolsPanelUtils::GetAbsoluteGameContentDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
}


FString
FHoudiniToolsPanelUtils::GetLastDirectory(const bool bRelativeToGameContentDir, const ELastDirectory::Type LastDirectory)
{
	FString StartDirectory = FEditorDirectories::Get().GetLastDirectory(LastDirectory);
	const FString AbsoluteGameContentDir = GetAbsoluteGameContentDir();
	if (bRelativeToGameContentDir && !IsValidPath(StartDirectory, bRelativeToGameContentDir))
	{
		StartDirectory = AbsoluteGameContentDir;
	}
	return StartDirectory;
}


bool
FHoudiniToolsPanelUtils::IsValidPath(const FString& AbsolutePath, const bool bRelativeToGameContentDir, FText* const OutReason)
{
	if(bRelativeToGameContentDir)
	{
		const FString AbsoluteGameContentDir = GetAbsoluteGameContentDir();
		if(!AbsolutePath.StartsWith(AbsoluteGameContentDir))
		{
			if(OutReason)
			{
				*OutReason = FText::Format(LOCTEXT("Error_InvalidRootPath", "The chosen directory must be within {0}"), FText::FromString(AbsoluteGameContentDir));
			}
			return false;
		}
	}
	return true;
}

UHoudiniToolDirectoryProperties::UHoudiniToolDirectoryProperties(const FObjectInitializer & ObjectInitializer)
	:Super(ObjectInitializer)
	, CustomHoudiniToolsDirectories()
{
};

FName SHoudiniToolCategory::TypeName = TEXT("SHoudiniToolCategory");
	


SHoudiniToolsCategoryFilter::SHoudiniToolsCategoryFilter()
	: ShowAll(true)
	, HiddenCategories(nullptr)
	, Categories(nullptr)
{
	
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void
SHoudiniToolsCategoryFilter::Construct(const FArguments& InArgs)
{
	SetShowAll(InArgs._ShowAll);
	
	OnShowAllChanged = InArgs._OnShowAllChanged;
	OnCategoryStateChanged = InArgs._OnCategoryStateChanged;
	HiddenCategories = InArgs._HiddenCategoriesSource;
	Categories = InArgs._CategoriesSource;
	
	RebuildWidget();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void
SHoudiniToolsCategoryFilter::SetShowAll(TAttribute<bool> InShowAll)
{
	ShowAll = InShowAll;
}

bool
SHoudiniToolsCategoryFilter::IsCategoryEnabled(FString CategoryName) const
{
	if (!HiddenCategories)
	{
		return false;
	}

	return !HiddenCategories->Contains(CategoryName);
}


TSharedPtr<SWidget>
SHoudiniToolsCategoryFilter::RebuildWidget()
{
	FMenuBuilder MenuBuilder( true, NULL );
	
	MenuBuilder.AddMenuEntry(
		FText::FromString("Show All"),
		FText::FromString("Show all categories."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([&]() -> void
			{
				OnShowAllChanged.ExecuteIfBound( !ShowAll.Get() );
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([&]() -> ECheckBoxState { return ShowAll.Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.BeginSection("Categories", LOCTEXT("FilterMenu_Section_Categories", "Categories"));
	
	// Add all the categories to the menu 
	
	if (Categories)
	{
		for (const FString& CategoryName : *Categories)
		{
			MenuBuilder.AddMenuEntry(
			FText::FromString(CategoryName),
			FText::FromString( FString::Format(TEXT("Show/Hide {0}."), {CategoryName}) ),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([&, CategoryName]() -> void
				{
					OnCategoryStateChanged.ExecuteIfBound(CategoryName, !IsCategoryEnabled(CategoryName));
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([&, CategoryName]() -> ECheckBoxState
				{
					return HiddenCategories->Contains(CategoryName) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
				} )
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}

	TSharedPtr<SWidget> Widget = MenuBuilder.MakeWidget(); 
	ChildSlot
	[
		Widget.ToSharedRef()
	];

	return Widget;
}

SHoudiniToolCategory::SHoudiniToolCategory()
	: CategoryType(EHoudiniToolCategoryType::Package)
	, TextWrapWidth(0)
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void
SHoudiniToolCategory::Construct(const FArguments& InArgs)
{
	CategoryLabel = InArgs._CategoryLabel.Get().ToString();
	CategoryType = InArgs._CategoryType.Get();
	SourceEntries = InArgs._HoudiniToolsItemSource.Get();
	IsVisible = InArgs._IsVisible;
	ShowEmptyCategory = InArgs._ShowEmptyCategory;
	ShowHoudiniAssets = InArgs._ShowHoudiniAssets;
	ShowPresets = InArgs._ShowPresets;
	ShowHiddenTools = InArgs._ShowHiddenTools;
	ToolContextMenuOpening = InArgs._OnToolContextMenuOpening;
	CategoryContextMenuOpening = InArgs._OnCategoryContextMenuOpening;

	const FText UserCategoryTooltip = FText::Format(LOCTEXT("HoudiniToolsPanel_UserCategoryTooltip", "{0} (User Category)"), FText::FromString(CategoryLabel));
	const FText PackageCategoryTooltip = FText::Format( LOCTEXT("HoudiniToolsPanel_PackageCategoryTooltip", "{0} (Package Category)"), FText::FromString(CategoryLabel));
	
	if (InArgs._ViewMode.Get() == EHoudiniToolsViewMode::TileView)
	{
		// Configure Tile View
		SAssignNew(HoudiniToolsView, SHoudiniToolTileView)
		.ScrollbarVisibility(EVisibility::Collapsed)
		.SelectionMode( ESelectionMode::Single )
		.ListItemsSource( &VisibleEntries )
		.OnGenerateTile( InArgs._OnGenerateTile )
		.OnSelectionChanged_Lambda( [this, InArgs](const TSharedPtr<FHoudiniTool>& HoudiniTool, ESelectInfo::Type SelectInfo)
		{
			ActiveTool = HoudiniTool;
			InArgs._OnToolSelectionChanged.ExecuteIfBound(this, HoudiniTool, SelectInfo);
		} )
		.OnMouseButtonDoubleClick( InArgs._OnMouseButtonDoubleClick )
		.OnContextMenuOpening(this, &SHoudiniToolCategory::HandleContextMenuOpening )
		.ItemHeight( 64 )
		.ItemWidth( 120 )
		.ItemAlignment(EListItemAlignment::LeftAligned);
	}
	else
	{
		// Configure List View
		SAssignNew(HoudiniToolsView, SHoudiniToolListView)
			.ScrollbarVisibility(EVisibility::Collapsed)
			.SelectionMode(ESelectionMode::Single)
			.ListItemsSource(&VisibleEntries)
			.OnGenerateRow(InArgs._OnGenerateRow)
			.OnSelectionChanged_Lambda([this, InArgs](const TSharedPtr<FHoudiniTool>& HoudiniTool, ESelectInfo::Type SelectInfo)
				{
					ActiveTool = HoudiniTool;
					InArgs._OnToolSelectionChanged.ExecuteIfBound(this, HoudiniTool, SelectInfo);
				})
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
			.ItemHeight(64) //UE5.5 deprecated this - only for tile view
#endif
			.OnMouseButtonDoubleClick(InArgs._OnMouseButtonDoubleClick)
			.OnContextMenuOpening(this, &SHoudiniToolCategory::HandleContextMenuOpening);
	}
	
	ChildSlot
	[
		SNew(SExpandableArea)
		.HeaderContent()
		[
			SNew(SHorizontalBox)
			.ToolTipText(CategoryType == EHoudiniToolCategoryType::User ? UserCategoryTooltip : PackageCategoryTooltip)
			// Label
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock)
					.Font(_GetEditorStyle().GetFontStyle("HeadingExtraSmall"))
					.Text( InArgs._CategoryLabel )
					.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			
			// Category Type Icon ( displayed for user categories )
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.TextStyle(_GetEditorStyle(), "ContentBrowser.TopBar.Font")
					.Font(_GetEditorStyle().GetFontStyle("FontAwesome.12"))
					.Text(FText::FromString(FString(
						 CategoryType == EHoudiniToolCategoryType::User ?
							TEXT("\xf007") : /* fa-user */
							TEXT("\xf187") /* fa-archive */
							)) )
					.ColorAndOpacity( FSlateColor::UseForeground() )
					.Visibility( CategoryType == EHoudiniToolCategoryType::User ? EVisibility::Visible : EVisibility::Collapsed )
					.ToolTipText( CategoryType == EHoudiniToolCategoryType::User ? UserCategoryTooltip : PackageCategoryTooltip )
			]
		]
		.BodyContent()
		[
			SAssignNew(CategoryContentSwitcher, SWidgetSwitcher)
			// Houdini Tools View
			+SWidgetSwitcher::Slot()
			[
				HoudiniToolsView.ToSharedRef()
			]
			// Message if we don't have any tools in this category
			+SWidgetSwitcher::Slot()
			.HAlign(HAlign_Left)
			.Padding(FMargin(10.f, 15.f, 10.f, 10.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HoudiniToolsPanel_NoToolsInCategory", "Import Houdini Assets next to this HoudiniToolsPackage asset to add tools to this category."))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.25f))
				.WrapTextAt_Lambda([this]()->float { return TextWrapWidth; })
			]
			// Message if we don't have any _visible_ tools in this category
			+SWidgetSwitcher::Slot()
			.HAlign(HAlign_Left)
			.Padding(FMargin(10.f, 15.f, 10.f, 10.f))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HoudiniToolsPanel_NoVisibleToolsInCategory2", "Enable \"Show Hidden Tools\" or import Houdini Assets next to this HoudiniToolsPackage asset to add tools to this category."))
					.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.25f))
					.WrapTextAt_Lambda([this]()->float { return TextWrapWidth; })
				]
			]
		]
	];

	UpdateVisibleItems();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void
SHoudiniToolCategory::SetFilterString(const FString& NewFilterString)
{
	if (FilterString != NewFilterString)
	{
		FilterString = NewFilterString;
		UpdateVisibleItems();
	}
}

void
SHoudiniToolCategory::ClearSelection()
{
	if (!HoudiniToolsView.IsValid())
		return;
	
	HoudiniToolsView->ClearHighlightedItems();
	HoudiniToolsView->ClearSelection();
}

void
SHoudiniToolCategory::RequestRefresh()
{
	ClearSelection();
	if (HoudiniToolsView.IsValid())
	{
		HoudiniToolsView->RequestListRefresh();
	}
}

void
SHoudiniToolCategory::UpdateVisibleItems()
{
	VisibleEntries.Empty();
	HoudiniToolsView->RequestListRefresh();
	
	ECategoryContentType ContentType = ECategoryContentType::Tools;

	ON_SCOPE_EXIT
	{
		if (CategoryContentSwitcher.IsValid())
		{
			CategoryContentSwitcher->SetActiveWidgetIndex(static_cast<int32>(ContentType));
		}
	};

	if (!SourceEntries.IsValid())
	{
		ContentType = ECategoryContentType::NoToolsInCategory;
		return;
	}

	for (TSharedPtr<FHoudiniTool> HoudiniTool : SourceEntries->Tools )
	{
		if (!HoudiniTool.IsValid())
			continue;

		if (ShowHiddenTools.Get() == false)
		{
			if (HoudiniTool->IsHiddenInCategory(GetCategoryLabel()))
			{
				// This tool should not be visible in this category
				continue;
			}
		}
		
		if (HoudiniTool->PackageToolType == EHoudiniPackageToolType::HoudiniAsset && ShowHoudiniAssets.Get() == false)
		{
			// We don't want to show HoudiniAssets
			continue;
		}
		if (HoudiniTool->PackageToolType == EHoudiniPackageToolType::Preset && ShowPresets.Get() == false)
		{
			// We don't want to show Presets
			continue;
		}
		if (HoudiniTool->Name.ToString().Contains(FilterString) || FilterString.Len() == 0)
		{
			VisibleEntries.Add(HoudiniTool);
		}
	}

	if (IsVisible.Get() == false)
	{
		SetVisibility(EVisibility::Collapsed);
	}
	else if (VisibleEntries.Num() == 0 )
	{
		if (FilterString.Len() > 0)
		{
			// We are searching for something. Collapse this category.
			SetVisibility(EVisibility::Collapsed);
		}
		else if (ShowEmptyCategory.Get() == true)
		{
			// We want to show this (empty) category
			SetVisibility(EVisibility::Visible);
		}
		else
		{
			SetVisibility(EVisibility::Collapsed);
		}
	}
	else
	{
		SetVisibility(EVisibility::Visible);
	}

	if (SourceEntries->Tools.Num() == 0)
	{
		// We have no tools in this category.
		ContentType = ECategoryContentType::NoToolsInCategory;
	}
	else
	{
		if (VisibleEntries.Num() == 0)
		{
			// We have tools, they are just not visible.
			ContentType = ECategoryContentType::NoVisibleToolsInCategory;
		}
	}
}


void
SHoudiniToolCategory::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Set cached wrap text width based on new "LastGeometry" value. 
	// We set this only when changed because binding a delegate to text wrapping attributes is expensive
	constexpr float Padding = 25.f;
	if(!FMath::IsNearlyEqual(TextWrapWidth, AllottedGeometry.Size.X-Padding))
	{
		TextWrapWidth = AllottedGeometry.Size.X - Padding; // Add a tiny bit of padding to avoid words being cut off.
	}
}


FReply
SHoudiniToolCategory::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);

	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		if (!Reply.IsEventHandled())
		{
			if (CategoryContextMenuOpening.IsSet() && CategoryContextMenuOpening.Get().IsBound())
			{
				const TSharedPtr<SWidget> MenuContent = CategoryContextMenuOpening.Get().Execute();
				if (MenuContent.IsValid())
				{
					const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 1
					const FVector2f SummonLocation = MouseEvent.GetScreenSpacePosition();
#else
					const FVector2D SummonLocation = MouseEvent.GetScreenSpacePosition();
#endif
					FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent.ToSharedRef(), SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
					return FReply::Handled();
				}
			}
		}
	}
	
	return Reply;
}


TSharedPtr<SWidget>
SHoudiniToolCategory::HandleContextMenuOpening()
{
	// otherwise, invoke the categeory context menu
	if (HoudiniToolsView->GetNumItemsSelected() > 0)
	{
		if (ToolContextMenuOpening.IsSet() && ToolContextMenuOpening.Get().IsBound())
		{
			// If we have an active item, invoke the Tool context menu.
			return ToolContextMenuOpening.Get().Execute();
		}
	}
	else
	{
		if (CategoryContextMenuOpening.IsSet() && CategoryContextMenuOpening.Get().IsBound())
		{
			// otherwise, invoke the category
			return CategoryContextMenuOpening.Get().Execute();
		}
	}

	return nullptr;
}


SHoudiniToolNewPackage::SHoudiniToolNewPackage()
	: CalculatedPackageName(FText())
	, bValidPackageName(false)
	, bSyncCategory(true)
{
	
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void
SHoudiniToolNewPackage::Construct(const FArguments& InArgs)
{
	const FText InitialPackageName = LOCTEXT("CreatePackage_PackageNameValue", "PackageName");
	int row = 0;

	const int RowPackageName = row++;
	const int RowCategoryName = row++;
	const int RowPackageLocation = row++;
	
	// Assign our widget events to our delegates
	OnCanceled = InArgs._OnCanceled;
	OnCreated = InArgs._OnCreated;
	
	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(18.0f)
		.BorderImage( _GetBrush("Docking.Tab.ContentAreaBrush") )
		[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
				.Text( LOCTEXT( "HoudiniTool_CreatePackageTitle", "Create Houdini Tools Package" ) )
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			
			// -----------------------------
			// Description
			// -----------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("HoudiniTool_CreatePackageDescription", "Enter a name for your new Houdini Tools package. Package names may only contain alphanumeric characters, and may not contain a space."))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("HoudiniTool_CreatePackageDescription2", "When you click the Create button, an empty Houdini Tools package will created at the displayed location."))
				]

			]
			
			// -----------------------------
			// Name Error label
			// -----------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f)
			[
				SNew(SWarningOrErrorBox)
				.MessageStyle(EMessageStyle::Error)
				.Visibility(this, &SHoudiniToolNewPackage::GetNameErrorLabelVisibility)
				.Message(this, &SHoudiniToolNewPackage::GetNameErrorLabelText)
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(_GetBrush("DetailsView.CategoryTop"))
				.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
				.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
				[
					SNew(SGridPanel)
					.FillColumn(1, 1.0f)

					//--------------------------
					// Houdini Tools package name
					//--------------------------
					
					+ SGridPanel::Slot(0, RowPackageName)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreatePackage_NameLabel", "Name"))
					]
					+ SGridPanel::Slot(1, RowPackageName)
					.VAlign(VAlign_Center)
					.Padding(0.f, 3.0f)
					[
						SAssignNew(PackageNameEditBox, SEditableTextBox)
						.Text(InitialPackageName)
						.ToolTipText(LOCTEXT("CreatePackage_NameTooltip", "The name for the package folder as seen in the content browser."))
						.OnTextChanged( this, &SHoudiniToolNewPackage::OnPackageNameTextChanged )
						.OnTextCommitted( this, &SHoudiniToolNewPackage::OnPackageNameTextCommitted )
					]

					//--------------------------
					// Houdini Tools category name
					//--------------------------
					
					+ SGridPanel::Slot(0, RowCategoryName)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreatePackage_CategoryLabel", "Category"))
					]
					+ SGridPanel::Slot(1, RowCategoryName)
					.VAlign(VAlign_Center)
					.Padding(0.f, 3.0f)
					[
						SAssignNew(PackageCategoryEditBox, SEditableTextBox)
						.Text(InitialPackageName)
						.ToolTipText(LOCTEXT("CreatePackage_CategoryTooltip", "The category that will show up in the Houdini Tools Panel, containing all the tools in this package."))
						.OnTextChanged( this, &SHoudiniToolNewPackage::OnPackageCategoryTextChanged )
						.OnTextCommitted( this, &SHoudiniToolNewPackage::OnPackageCategoryTextCommitted )
					]

					//--------------------------
					// Houdini Tools package location
					//--------------------------
					
					+ SGridPanel::Slot(0, RowPackageLocation)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreatePackage_LocationLabel", "Location"))
					]
					+ SGridPanel::Slot(1, RowPackageLocation)
					.VAlign(VAlign_Center)
					.Padding(0.f, 5.0f)
					[
						SNew(STextBlock)
						.Text(this, &SHoudiniToolNewPackage::OnGetPackagePathText)
					]
				]
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			.FillHeight(1.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))

				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SHoudiniToolNewPackage::HandleCreateClicked)
					.IsEnabled(this, &SHoudiniToolNewPackage::IsCreateEnabled)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "CreatePackage_CreateButtonLabel", "Create" ))
					]
				]
				
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SHoudiniToolNewPackage::HandleCancelClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "CreatePackage_CancelButtonLabel", "Cancel" ))
					]
				]
			]
		]
	]];

	// Perform initial package name check to initialize the UI.  
	UpdatePackageName(InitialPackageName);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void
SHoudiniToolNewPackage::OnPackageNameTextChanged(const FText& Text)
{
	UpdatePackageName(Text);
}


void
SHoudiniToolNewPackage::OnPackageNameTextCommitted(const FText& Text, ETextCommit::Type Arg)
{
	UpdatePackageName(Text); 
}


FText
SHoudiniToolNewPackage::OnGetPackagePathText() const
{
	return FText::FromString(FHoudiniToolsEditor::GetDefaultPackagePath(CalculatedPackageName.ToString()));
}


void
SHoudiniToolNewPackage::OnPackageCategoryTextChanged(const FText& Text)
{
	bSyncCategory = false;
}


void
SHoudiniToolNewPackage::OnPackageCategoryTextCommitted(const FText& Text, ETextCommit::Type Arg)
{
	bSyncCategory = false;
}


EVisibility
SHoudiniToolNewPackage::GetNameErrorLabelVisibility() const
{
	return bValidPackageName ? EVisibility::Hidden : EVisibility::SelfHitTestInvisible;
}


FText
SHoudiniToolNewPackage::GetNameErrorLabelText() const
{
	return PackageNameError;
}


bool
SHoudiniToolNewPackage::IsCreateEnabled() const
{
	return bValidPackageName;
}


FReply
SHoudiniToolNewPackage::HandleCreateClicked()
{
	// Create HoudiniToolsAssetPackageInfo
	UHoudiniToolsPackageAssetFactory* Factory = NewObject<UHoudiniToolsPackageAssetFactory>();

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	const FString PackageAssetName = FHoudiniToolsRuntimeUtils::GetPackageUAssetName();
	const FString PackageBasePath = FHoudiniToolsEditor::GetDefaultPackagePath(CalculatedPackageName.ToString());
	const FString FullPath = FPaths::Combine(PackageBasePath, PackageAssetName);

	UHoudiniToolsPackageAsset* Asset = Cast<UHoudiniToolsPackageAsset>(AssetToolsModule.Get().CreateAsset(
		PackageAssetName, PackageBasePath,
		UHoudiniToolsPackageAsset::StaticClass(), Factory, FName("ContentBrowserNewAsset")));
	
	if (!Asset)
	{
		return FReply::Handled();
	}

	// By default add a catch-all category.
	FCategoryRules DefaultRule;
	DefaultRule.Include.Add("*");
	Asset->Categories.Add(GetCategory().ToString(), DefaultRule);
	
	OnCreated.ExecuteIfBound(FullPath);

	// Open the content browser at the newly created asset package.
	TArray<UObject*> Objs = {Asset};
	GEditor->SyncBrowserToObjects(Objs, true);
	
	CloseContainingWindow();
	
	return FReply::Handled();
}


FReply
SHoudiniToolNewPackage::HandleCancelClicked()
{
	OnCanceled.ExecuteIfBound();
	CloseContainingWindow();
	
	return FReply::Handled();
}


FText
SHoudiniToolNewPackage::GetCategory() const
{
	return PackageCategoryEditBox->GetText();
}

void
SHoudiniToolNewPackage::UpdatePackageName(FText NewName)
{
	bValidPackageName = FHoudiniToolsEditor::IsValidPackageName(NewName.ToString(), &PackageNameError);
	CalculatedPackageName = NewName;

	if (bSyncCategory)
	{
		PackageCategoryEditBox->SetText(NewName);
		// Ensure bSyncCategory remains true until the Sync Category is
		// changed by the user.
		bSyncCategory = true;
	}
	
	if (!bValidPackageName)
	{
		// Invalid package name
		return;
	}

	// Check whether a HoudiniTools package already exists at this location.
	const FString PkgBasePath = FHoudiniToolsEditor::GetDefaultPackagePath(CalculatedPackageName.ToString());
	const FString PkgPath = FPaths::Combine(PkgBasePath, FHoudiniToolsRuntimeUtils::GetPackageUAssetName());
	
	if ( _DoesAssetExist(PkgPath) ) 
	{
		// Package already exists.
		PackageNameError = LOCTEXT("PackageAlreadyExists", "Package already exists: '{0}'");
		PackageNameError = FText::Format(PackageNameError, {FText::FromString(PkgPath)});
		bValidPackageName = false;
		return;
	}

	PackageNameError = FText();
}

void
SHoudiniToolNewPackage::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if ( ContainingWindow.IsValid() )
	{
		ContainingWindow->RequestDestroyWindow();
	}
}


SHoudiniToolImportPackage::SHoudiniToolImportPackage()
	: bIsValidPackageDirectory(false)
	, bCreateMissingPackageJSON(true)
{

}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void
SHoudiniToolImportPackage::Construct(const FArguments& InArgs)
{
	const FText ImportPackageTitle = LOCTEXT( "HoudiniTool_ImportPackage_Title", "Import Houdini Tools Package" );
	const FText InitialPackageName = LOCTEXT("CreatePackage_PackageNameValue", "PackageName");
	// Row counter for widgets in a uniform grid.
	// Allows us to easily insert rows without having to worry about renumbering everything.
	int row = 0;

	// First screen
	const int RowPackageDirSelector = row++;
	const int RowPackageName = row++;
	const int RowPackageLocation = row++;

	// Second screen
	row = 0;
	const int Row2PackageDir = row++;
	const int Row2PackageName = row++;
	const int Row2PackageLocation = row++;
	const int Row2CreateExternalJSONFile = row++;
	
	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(18.0f)
		.BorderImage( _GetBrush("Docking.Tab.ContentAreaBrush") )
		[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(10.0f)
		[
			// Vertical container for the widget switcher and row of action buttons
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoHeight()

			// --------------------------------
			// Content Switcher Row
			// --------------------------------
			
			+ SVerticalBox::Slot()
			[
				SAssignNew(WidgetSwitcher, SWidgetSwitcher)

				// --------------------------------
				// Import Tool - First Panel
				// --------------------------------
				
				+ SWidgetSwitcher::Slot()
				[
					// -----------------------------
					// Title
					// -----------------------------
				
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
						.Text( ImportPackageTitle )
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]

					// -----------------------------
					// Description
					// -----------------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 10.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT("HoudiniTool_ImportPackageDescription1", "Select the external package directory."))
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT("HoudiniTool_ImportPackageDescription2", "Enter a name for your new Houdini Tools package. Package names may only contain alphanumeric characters, and may not contain a space."))
						]
					]
					
					// -----------------------------
					// Name Error label
					// -----------------------------
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f)
					[
						SNew(SWarningOrErrorBox)
						.MessageStyle(EMessageStyle::Error)
						.Visibility(this, &SHoudiniToolImportPackage::GetNameErrorLabelVisibility)
						.Message(this, &SHoudiniToolImportPackage::GetNameErrorLabelText)
					]
					
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(_GetBrush("DetailsView.CategoryTop"))
						.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
						.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
						[
							SNew(SGridPanel)
							.FillColumn(1, 1.0f)

							//--------------------------
							// Package Directory Selector
							//--------------------------

							+ SGridPanel::Slot(0, RowPackageDirSelector)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ImportPackage_DirectoryLabel", "Package Directory"))
							]
							+ SGridPanel::Slot(1, RowPackageDirSelector)
							.VAlign(VAlign_Center)
							.Padding(0.f, 3.0f)
							[
								SAssignNew(DirectoryPicker, SDirectoryPicker)
								.OnDirectoryChanged(this, &SHoudiniToolImportPackage::OnDirectoryChanged)
							]

							//--------------------------
							// Houdini Tools package name
							//--------------------------
							
							+ SGridPanel::Slot(0, RowPackageName)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ImportPackage_NameLabel", "Name"))
							]
							+ SGridPanel::Slot(1, RowPackageName)
							.VAlign(VAlign_Center)
							.Padding(0.f, 3.0f)
							[
								SAssignNew(PackageNameEditBox, SEditableTextBox)
								.Text(InitialPackageName)
								.ToolTipText(LOCTEXT("ImportPackage_NameTooltip", "The name for the package folder as seen in the content browser."))
								.OnTextChanged( this, &SHoudiniToolImportPackage::OnPackageNameTextChanged )
								.OnTextCommitted( this, &SHoudiniToolImportPackage::OnPackageNameTextCommitted )
							]

							//--------------------------
							// Houdini Tools package location
							//--------------------------
							
							+ SGridPanel::Slot(0, RowPackageLocation)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ImportPackage_LocationLabel", "Location"))
							]
							+ SGridPanel::Slot(1, RowPackageLocation)
							.VAlign(VAlign_Center)
							.Padding(0.f, 3.0f)
							[
								SNew(STextBlock)
								.Text(this, &SHoudiniToolImportPackage::OnGetPackagePathText)
							]
						]
					] // VerticalBox Slot
				] // Switcher - First Panel

				// --------------------------------
				// Import Tool - Second Panel
				// --------------------------------
				
				+ SWidgetSwitcher::Slot()
				[
					// -----------------------------
					// Title
					// -----------------------------
				
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
						.Text( ImportPackageTitle )
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]

					// -----------------------------
					// Description
					// -----------------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 10.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT("HoudiniTool_ImportPackageDescription", "External package descriptor does not exist."))
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT("HoudiniTool_ImportPackageDescription", "Create a new package instead and import HDAs from the package directory."))
						]
					]

					// -----------------------------
					// Grid panel for fields
					// -----------------------------

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(_GetBrush("DetailsView.CategoryTop"))
						.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
						.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
						[
							SNew(SGridPanel)
							.FillColumn(1, 1.0f)

							//--------------------------
							// External Package Directory
							//--------------------------
							
							+ SGridPanel::Slot(0, Row2PackageDir)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ImportPackage_DirectoryLabel", "Package Directory"))
							]
							+ SGridPanel::Slot(1, Row2PackageDir)
							.VAlign(VAlign_Center)
							.Padding(0.f, 3.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([&]() -> FText
								{
									if (DirectoryPicker.IsValid())
									{
										return FText::FromString(DirectoryPicker->GetDirectory());
									}
									return FText();
								})
							]

							//--------------------------
							// External Package Directory
							//--------------------------
							
							+ SGridPanel::Slot(0, Row2PackageName)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ImportPackage_PackageNameLabel", "Package Name"))
							]
							+ SGridPanel::Slot(1, Row2PackageName)
							.VAlign(VAlign_Center)
							.Padding(0.f, 3.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([&]() -> FText
								{
									return FText::FromString(NewPackageName);
								})
							]

							//--------------------------
							// Package Location
							//--------------------------
							
							+ SGridPanel::Slot(0, Row2PackageLocation)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ImportPackage_LocationLabel", "Location"))
							]
							+ SGridPanel::Slot(1, Row2PackageLocation)
							.VAlign(VAlign_Center)
							.Padding(0.f, 3.0f)
							[
								SNew(STextBlock)
								.Text(this, &SHoudiniToolImportPackage::OnGetPackagePathText)
							]

							//--------------------------
							// Create external JSON file
							//--------------------------
							
							+ SGridPanel::Slot(0, Row2CreateExternalJSONFile)
							.VAlign(VAlign_Center)
							.ColumnSpan(2)
							.Padding(0.0f, 3.0f, 12.0f, 0.0f)
							[
								SAssignNew(CreateExternalJSON, SCheckBox)
								.Content()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ImportPackage_CreateJSONLabel", "Create external package description (JSON file)"))
								]
							]
						]
					]
				]
			] // Content Switcher Row

			// --------------------------------
			// Action Buttons row
			// --------------------------------
			
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			.FillHeight(1.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))

				// Back Button
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.OnClicked(this, &SHoudiniToolImportPackage::HandleBackClicked)
					.Visibility(this, &SHoudiniToolImportPackage::GetBackVisibility)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "ImportPackage_BackButtonLabel", "Back" ))
					]
				]

				// Next Button
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
					.OnClicked(this, &SHoudiniToolImportPackage::HandleNextClicked)
					.IsEnabled(this, &SHoudiniToolImportPackage::IsNextEnabled)
					.Visibility(this, &SHoudiniToolImportPackage::GetNextVisibility)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "ImportPackage_NextButtonLabel", "Next" ))
					]
				]
				
				// Import Button
				+ SUniformGridPanel::Slot(2, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
					.OnClicked(this, &SHoudiniToolImportPackage::HandleImportClicked)
					.IsEnabled(this, &SHoudiniToolImportPackage::IsImportEnabled)
					.Visibility(this, &SHoudiniToolImportPackage::GetImportVisibility)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "ImportPackage_ImportButtonLabel", "Import" ))
					]
				]

				// Create Button
				+ SUniformGridPanel::Slot(2, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
					.OnClicked(this, &SHoudiniToolImportPackage::HandleCreateClicked)
					.IsEnabled(this, &SHoudiniToolImportPackage::IsCreateEnabled)
					.Visibility(this, &SHoudiniToolImportPackage::GetCreateVisibility)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "ImportPackage_CreateButtonLabel", "Create" ))
					]
				]


				// Cancel Button
				
				+ SUniformGridPanel::Slot(3, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SHoudiniToolImportPackage::HandleCancelClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "ImportPackage_CancelButtonLabel", "Cancel" ))
					]
				]
			] // Action Buttons Row
		]
	]];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EVisibility
SHoudiniToolImportPackage::GetNameErrorLabelVisibility() const
{
	// TODO: Implement error conditions here
	if (!bIsValidPackageDirectory)
	{
		return EVisibility::Visible;
	}

	if (!bValidPackageName)
	{
		return EVisibility::Visible;
	}

	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		return EVisibility::Hidden;
	}
	else if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		return EVisibility::Visible;
	}

	// Hidden (not collapsed) by default.
	return EVisibility::Hidden;
}

auto
SHoudiniToolImportPackage::GetNameErrorLabelText() const -> FText
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		if (DirectoryPicker->GetDirectory().IsEmpty())
		{
			return LOCTEXT("ImportPackage_EmptyDirectory", "Select a package directory.");
		}
		
		if (!bIsValidPackageDirectory)
		{
			return LOCTEXT("ImportPackage_DirectoryNotExist", "Directory does not exist.");
		}

		if (!bValidPackageName)
		{
			return PackageNameError;
		}
	}
	else if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		return LOCTEXT("ImportPackage_ExternalPackageNotExist", "External package descriptor does not exist. Create new package.");
	}

	return FText();
}

void
SHoudiniToolImportPackage::OnDirectoryChanged(const FString& Directory)
{
	bHasPackageJSON = false;
	bIsValidPackageDirectory = false;
	
	if (Directory.IsEmpty())
	{
		return;
	}

	bIsValidPackageDirectory = FPaths::DirectoryExists(Directory);
	if (bIsValidPackageDirectory)
	{
		// Check whether the directory contains a JSON file.
		const FString JSONPath = FPaths::Combine(Directory, FHoudiniToolsRuntimeUtils::GetPackageJSONName());
		if (FPaths::FileExists(JSONPath))
		{
			bHasPackageJSON = true;
		}
	
		// Populate the PackageName with the selected directory name.
		FString PackageName = Directory.TrimStartAndEnd();
		if (PackageName.EndsWith("/"))
		{
			// Remove trailing slash
			PackageName = PackageName.LeftChop(1);
		}
		PackageName = FPaths::GetBaseFilename(PackageName);
		PackageNameEditBox->SetText(FText::FromString(PackageName));
	}
}

void
SHoudiniToolImportPackage::OnPackageNameTextChanged(const FText& Text)
{
	UpdatePackageName(Text.ToString());
}

void
SHoudiniToolImportPackage::OnPackageNameTextCommitted(const FText& Text, ETextCommit::Type Arg)
{
	UpdatePackageName(Text.ToString());
}

FText
SHoudiniToolImportPackage::OnGetPackagePathText() const
{
	return FText::FromString( FHoudiniToolsEditor::GetDefaultPackagePath(NewPackageName) );
}

bool
SHoudiniToolImportPackage::IsImportEnabled() const
{
	if (!bIsValidPackageDirectory)
		return false;
	if (!bValidPackageName)
		return false;

	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		// On the first page, if we have a valid package JSON, enable import button.
		return bHasPackageJSON;
	}

	if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		// On the second page, TODO
		return false;
	}
	
	return false;
}

bool
SHoudiniToolImportPackage::IsNextEnabled() const
{
	if (!bIsValidPackageDirectory)
		return false;
	if (!bValidPackageName)
		return false;

	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		// On the first page, if don't have a valid package JSON, enable next button.
		return !bHasPackageJSON;
	}

	return false;
}

FReply
SHoudiniToolImportPackage::HandleImportClicked()
{
	const FString PackageJSONPath = FPaths::Combine(DirectoryPicker->GetDirectory(), FHoudiniToolsRuntimeUtils::GetPackageJSONName());

	const FString PackageName = PackageNameEditBox->GetText().ToString();
	const FString PackageDestPath = FHoudiniToolsEditor::GetDefaultPackagePath(PackageName);
	UHoudiniToolsPackageAsset* ImportedPackage = FHoudiniToolsEditor::ImportExternalToolsPackage(PackageDestPath, PackageJSONPath, true);
	
	if (!ImportedPackage)
	{
		const FText Title = LOCTEXT("ImportPackage_ImportError_Title", "Import Error");
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ImportPackage_ImportError", "An error occured during import."),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			Title
#else
			&Title
#endif
			);
	}
	else
	{
		// Performing a reimport on this package will import all hdas found in the external package directory.
		int NumHDAs = 0;
		FHoudiniToolsEditor::ReimportPackageHDAs(ImportedPackage, false, &NumHDAs);

		if (NumHDAs > 0)
		{
			const FText Message = LOCTEXT("ImportPackage_ImportedHDAs", "Imported {0} HDAs.");
			const FText Title = LOCTEXT("ImportPackage_ImportSuccess_Title", "Package Import Successful");
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(Message, {NumHDAs}),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
				Title
#else
				&Title
#endif
				);
		}
		else
		{
			const FText Message = LOCTEXT("ImportPackage_NoHDAs", "No HDAs to import.");
			const FText Title = LOCTEXT("ImportPackage_ImportSuccess_Title", "Package Import Successful");
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(Message, {NumHDAs}),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
				Title
#else
				&Title
#endif
				);
		}
		
		FHoudiniToolsEditor::BrowseToObjectInContentBrowser(ImportedPackage);
	}

	CloseContainingWindow();

	return FReply::Handled();
}

EVisibility
SHoudiniToolImportPackage::GetImportVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		// We don't want to see the import button on this screen.
		return EVisibility::Collapsed;
	}
	
	return EVisibility::Visible;
}

FReply
SHoudiniToolImportPackage::HandleNextClicked() const
{
	WidgetSwitcher->SetActiveWidgetIndex( WidgetSwitcher->GetActiveWidgetIndex() + 1 );

	return FReply::Handled();
}

EVisibility
SHoudiniToolImportPackage::GetNextVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() < 1)
	{
		// We only want to see the create button on this screen.
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

FReply
SHoudiniToolImportPackage::HandleCreateClicked()
{
	const FString PackageDir = FHoudiniToolsEditor::GetDefaultPackagePath(NewPackageName);
	UHoudiniToolsPackageAsset* Asset = FHoudiniToolsEditor::CreateToolsPackageAsset(
		PackageDir,
		NewPackageName,
		DirectoryPicker->GetDirectory()
		);
	if (!Asset)
	{
		return FReply::Handled();
	}

	if (CreateExternalJSON.IsValid() && CreateExternalJSON->IsChecked())
	{
		// Create a default external JSON
		const FString JSONFilePath = FHoudiniToolsRuntimeUtils::GetHoudiniToolsPackageJSONPath(Asset);
		FHoudiniToolsEditor::WriteDefaultJSONFile(Asset, JSONFilePath);
	}

	// Import HDAs
	int NumHDAs = 0;
	FHoudiniToolsEditor::ReimportPackageHDAs(Asset, false, &NumHDAs);

	if (NumHDAs > 0)
	{
		const FText Message = LOCTEXT("ImportPackage_ImportedHDAs", "Imported {0} HDAs.");
		const FText Title = LOCTEXT("ImportPackage_ImportSuccess_Title", "Package Creation Successful");
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(Message, {NumHDAs}),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			Title
#else
			&Title
#endif
			);
	}
	else
	{
		const FText Message = LOCTEXT("ImportPackage_NoHDAs", "No HDAs to import.");
		const FText Title = LOCTEXT("ImportPackage_ImportSuccess_Title", "Package Creation Successful");
		FMessageDialog::Open(EAppMsgType::Ok, 
			Message,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			Title
#else
			&Title
#endif
		);
	}

	// Open the content browser at the newly created asset package.
	FHoudiniToolsEditor::BrowseToObjectInContentBrowser(Asset);

	CloseContainingWindow();
 
	 return FReply::Handled();
}

bool
SHoudiniToolImportPackage::IsCreateEnabled() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		return true;
	}

	return false;
}

EVisibility
SHoudiniToolImportPackage::GetCreateVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FReply
SHoudiniToolImportPackage::HandleBackClicked() const
{
	WidgetSwitcher->SetActiveWidgetIndex( WidgetSwitcher->GetActiveWidgetIndex() - 1 );
	return FReply::Handled();
}

EVisibility
SHoudiniToolImportPackage::GetBackVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() >= 1)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

FReply
SHoudiniToolImportPackage::HandleCancelClicked()
{
	OnCanceled.ExecuteIfBound();
	CloseContainingWindow();
	
	return FReply::Handled();
}

FText
SHoudiniToolImportPackage::GetCategory() const
{
	return FText::FromString(TEXT("PlaceholderCategory"));
}

void
SHoudiniToolImportPackage::UpdatePackageName(const FString& InName)
{
	NewPackageName = InName;
	
	bValidPackageName = FHoudiniToolsEditor::CanCreateCreateToolsPackage(InName, &PackageNameError);
	if (!bValidPackageName)
	{
		return;
	}

	PackageNameError = FText();
}

void
SHoudiniToolImportPackage::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if ( ContainingWindow.IsValid() )
	{
		ContainingWindow->RequestDestroyWindow();
	}
}


SHoudiniToolAddToUserCategory::SHoudiniToolAddToUserCategory()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void
SHoudiniToolAddToUserCategory::Construct(const FArguments& InArgs)
{
	const FText DialogTitle = LOCTEXT( "HoudiniTool_AddToUserCategory_Title", "Add Tool To User Category" );

	ActiveTool = InArgs._ActiveTool.Get();
	checkf(ActiveTool.IsValid(), TEXT("Required a valid FHoudiniTool."));
	
	Package = ActiveTool->ToolsPackage.LoadSynchronous();
	checkf(IsValid(Package), TEXT("FHoudiniTools must have a valid owning package."));

	// Capture user categories
	TArray<FString> Categories;
	FHoudiniToolsEditor::GetUserCategoriesList(Categories);
	for(const FString& CategoryName : Categories)
	{
		UserCategories.Add( MakeShareable(new FString(CategoryName)) );
	}

	int row = 0;

	// First view
	const int RowToolName = row++;
	const int RowPackagePath = row++;
	const int RowUserCategorySelection = row++;

	// Second screen
	row = 0;
	
	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(18.0f)
		.BorderImage( _GetBrush("Docking.Tab.ContentAreaBrush") )
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(10.0f)
			[
				// Vertical container for the widget switcher and row of action buttons
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.AutoHeight()

				// --------------------------------
				// Content Switcher Row
				// --------------------------------
				
				+ SVerticalBox::Slot()
				[
					SAssignNew(WidgetSwitcher, SWidgetSwitcher)

					// --------------------------------
					// Add To Category View
					// --------------------------------
					
					+ SWidgetSwitcher::Slot()
					[
						SAssignNew(AddToCategoryView, SVerticalBox)

						// -----------------------------
						// Title
						// -----------------------------
						
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Fill)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
							.Text( DialogTitle )
							.TransformPolicy(ETextTransformPolicy::ToUpper)
						]

						// -----------------------------
						// Description
						// -----------------------------
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 10.0f)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.AutoWrapText(true)
								.Text(LOCTEXT("HoudiniTool_AddToUserCategory_AddToCategoryDescription1", "Select the category to which the active tool should be added."))
							]
							// +SVerticalBox::Slot()
							// .AutoHeight()
							// [
							//     SNew(STextBlock)
							//     .AutoWrapText(true)
							//     .Text(LOCTEXT("HoudiniTool_AddToUserCategory_AddToCategoryDescription2", "Enter a name for your new Houdini Tools package. Package names may only contain alphanumeric characters, and may not contain a space."))
							// ]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(_GetBrush("DetailsView.CategoryTop"))
							.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
							.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
							[
								SNew(SGridPanel)
								.FillColumn(1, 1.0f)

								//--------------------------
								// Tool Name
								//--------------------------
								
								+ SGridPanel::Slot(0, RowToolName)
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HoudiniTool_AddToUserCategory_ToolName", "Tool Name"))
								]
								+ SGridPanel::Slot(1, RowToolName)
								.VAlign(VAlign_Center)
								.Padding(0.f, 3.0f)
								[
									SNew(STextBlock)
									.Text( ActiveTool->Name )
									.ToolTipText(LOCTEXT("HoudiniTool_AddToUserCategory_NameTooltip", "Location of the package containing the active tool."))
								]

								//--------------------------
								// Houdini Tools package location
								//--------------------------
								
								+ SGridPanel::Slot(0, RowPackagePath)
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HoudiniTool_AddToUserCategory_PackagePath", "Package Path"))
								]
								+ SGridPanel::Slot(1, RowPackagePath)
								.VAlign(VAlign_Center)
								.Padding(0.f, 3.0f)
								[
									SNew(STextBlock)
									.Text(SHoudiniToolAddToUserCategory::ToolPackagePathText())
								]

								//--------------------------
								// Category Selection
								//--------------------------
								
								+ SGridPanel::Slot(0, RowUserCategorySelection)
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HoudiniTool_AddToUserCategory_UserCategory", "User Category"))
								]
								+ SGridPanel::Slot(1, RowUserCategorySelection)
								.VAlign(VAlign_Center)
								.Padding(0.f, 3.0f)
								[
									SNew(SComboBox<TSharedPtr<FString>>)
									.OptionsSource(&UserCategories)
									.OnGenerateWidget_Lambda([](const TSharedPtr<FString>& InItem)
									{
										return SNew(STextBlock).Text(FText::FromString(*InItem));
									})
									.OnSelectionChanged(this, &SHoudiniToolAddToUserCategory::OnUserCategorySelectionChanged)
									[
										SNew(STextBlock)
										.Text_Lambda([&]()
										{
											if (SelectedCategory.IsValid())
											{
												return FText::FromString(*SelectedCategory);
											}
											return FText(LOCTEXT("HoudiniTool_AddToUserCategory_SelectCategory", "-- Select User Category --"));
										})
									]
								]
							]
						] // VerticalBox Slot
					] // Add To Category View

					// --------------------------------
					// Create User Category View
					// --------------------------------
					
					+ SWidgetSwitcher::Slot()
					[
						SAssignNew(CreateCategoryView, SVerticalBox)

						// -----------------------------
						// Title
						// -----------------------------
						
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Fill)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
							.Text( DialogTitle )
							.TransformPolicy(ETextTransformPolicy::ToUpper)
						]

						// -----------------------------
						// Description
						// -----------------------------
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 10.0f)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.AutoWrapText(true)
								.Text(LOCTEXT("HoudiniTool_AddToUserCategory_CreateCategoryDescription1", "Enter the new user category name."))
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.AutoWrapText(true)
								.Text(LOCTEXT("HoudiniTool_AddToUserCategory_CreateCategoryDescription2", "If the category exists, the tool will be added to the existing category. If the category doesn't exist, it will be created before adding the tool to the new category."))
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(_GetBrush("DetailsView.CategoryTop"))
							.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f ))
							.Padding(FMargin(6.0f, 4.0f, 7.0f, 4.0f))
							[
								SNew(SGridPanel)
								.FillColumn(1, 1.0f)

								//--------------------------
								// Tool Name
								//--------------------------
								
								+ SGridPanel::Slot(0, RowToolName)
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HoudiniTool_AddToUserCategory_ToolName", "Tool Name"))
								]
								+ SGridPanel::Slot(1, RowToolName)
								.VAlign(VAlign_Center)
								.Padding(0.f, 3.0f)
								[
									SNew(STextBlock)
									.Text( ActiveTool->Name )
									.ToolTipText(LOCTEXT("HoudiniTool_AddToUserCategory_NameTooltip", "Location of the package containing the active tool."))
								]

								//--------------------------
								// Houdini Tools package location
								//--------------------------
								
								+ SGridPanel::Slot(0, RowPackagePath)
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HoudiniTool_AddToUserCategory_PackagePath", "Package Path"))
								]
								+ SGridPanel::Slot(1, RowPackagePath)
								.VAlign(VAlign_Center)
								.Padding(0.f, 3.0f)
								[
									SNew(STextBlock)
									.Text(SHoudiniToolAddToUserCategory::ToolPackagePathText())
								]

								//--------------------------
								// Category Creation
								//--------------------------
								
								+ SGridPanel::Slot(0, RowUserCategorySelection)
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("HoudiniTool_AddToUserCategory_UserCategory", "User Category"))
								]
								+ SGridPanel::Slot(1, RowUserCategorySelection)
								.VAlign(VAlign_Center)
								.Padding(0.f, 3.0f)
								[
									SNew(SEditableTextBox)
									.OnTextChanged(this, &SHoudiniToolAddToUserCategory::OnUserCategoryCreateTextChanged)
									.OnTextCommitted(this, &SHoudiniToolAddToUserCategory::OnUserCategoryCreateTextCommitted)
								]
							]
						] // VerticalBox Slot
						
					] // Create User Category View
				] // Content Switcher Row

				// --------------------------------
				// Action Buttons row
				// --------------------------------
				
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				.FillHeight(1.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))

					// Back Button
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.OnClicked(this, &SHoudiniToolAddToUserCategory::HandleBackClicked)
						.Visibility(this, &SHoudiniToolAddToUserCategory::GetBackButtonVisibility)
						[
							SNew(STextBlock)
							.Text(LOCTEXT( "HoudiniTool_AddToUserCategory_BackButtonLabel", "Back" ))
						]
					]

					// Add to Selected Category
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.OnClicked(this, &SHoudiniToolAddToUserCategory::HandleAddToCategoryClicked)
						.IsEnabled(this, &SHoudiniToolAddToUserCategory::GetAddToCategoryEnabled)
						.Visibility(this, &SHoudiniToolAddToUserCategory::GetAddToCategoryVisibility)
						[
							SNew(STextBlock)
							.Text(LOCTEXT( "HoudiniTool_AddToUserCategory_AddToCategoryLabel", "Add" ))
						]
					]
					
					// Goto New Category
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.OnClicked(this, &SHoudiniToolAddToUserCategory::HandleGotoCreateCategoryClicked)
						.IsEnabled(this, &SHoudiniToolAddToUserCategory::GetGotoCreateCategoryEnabled)
						.Visibility(this, &SHoudiniToolAddToUserCategory::GetGotoCreateCategoryVisibility)
						[
							SNew(STextBlock)
							.Text(LOCTEXT( "HoudiniTool_AddToUserCategory_GotoNewCategoryLabel", "New Category" ))
						]
					]

					// Create and Add Category Button
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.OnClicked(this, &SHoudiniToolAddToUserCategory::HandleAddNewCategoryClicked)
						.IsEnabled(this, &SHoudiniToolAddToUserCategory::GetCreateCategoryEnabled)
						.Visibility(this, &SHoudiniToolAddToUserCategory::GetCreateCategoryVisibility)
						[
							SNew(STextBlock)
							.Text(LOCTEXT( "HoudiniTool_AddToUserCategory_CreateButtonLabel", "Create And Add" ))
							// .ToolTip(LOCTEXT( "ImportPackage_CreateButtonLabelTooltip", "Create the user category and add the tool to the category." ))
						]
					]


					// Cancel Button
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SHoudiniToolAddToUserCategory::HandleCancelClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT( "HoudiniTool_AddToUserCategory_CancelButtonLabel", "Cancel" ))
						]
					]
				] // Action Buttons Row
				
			]
		]
	]; // this->ChildSlot
}

FText SHoudiniToolAddToUserCategory::ToolPackagePathText()
{
	const FString ToolsPackagePath = FPaths::GetPath(Package->GetPackage()->GetPathName());
	return FText::FromString(ToolsPackagePath);
}

FReply SHoudiniToolAddToUserCategory::HandleBackClicked()
{
	WidgetSwitcher->SetActiveWidgetIndex( WidgetSwitcher->GetActiveWidgetIndex() - 1 );
	return FReply::Handled();
}

FReply SHoudiniToolAddToUserCategory::HandleAddToCategoryClicked()
{
	UHoudiniAsset* HoudiniAsset = ActiveTool->HoudiniAsset.LoadSynchronous();
	if (!IsValid(HoudiniAsset))
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot add tool to category. HoudiniAsset is invalid."));
		return FReply::Handled();
	}

	if (!SelectedCategory.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot add tool to category. Selected Category is invalid."));
		return FReply::Handled();
	}

	if (ActiveTool->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		UHoudiniPreset* Preset = ActiveTool->HoudiniPreset.LoadSynchronous();
		if (IsValid(Preset))
		{
			FHoudiniToolsEditor::AddToolToUserCategory(Preset, *SelectedCategory);
		}
	}
	else
	{
		FHoudiniToolsEditor::AddToolToUserCategory(HoudiniAsset, *SelectedCategory);
	}

	CloseContainingWindow();
	
	return FReply::Handled();
}

bool SHoudiniToolAddToUserCategory::GetAddToCategoryEnabled() const
{
	return UserCategories.Contains(SelectedCategory);
}

EVisibility
SHoudiniToolAddToUserCategory::GetAddToCategoryVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		// First Screen
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply
SHoudiniToolAddToUserCategory::HandleGotoCreateCategoryClicked()
{
	// Goto Create New Category page
	WidgetSwitcher->SetActiveWidgetIndex(1);
	return FReply::Handled();
}

bool
SHoudiniToolAddToUserCategory::GetGotoCreateCategoryEnabled() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		// First Screen
		return true;
	}

	return false;
}

EVisibility
SHoudiniToolAddToUserCategory::GetGotoCreateCategoryVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 0)
	{
		// First Screen
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply
SHoudiniToolAddToUserCategory::HandleAddNewCategoryClicked()
{
	UHoudiniAsset* HoudiniAsset = ActiveTool->HoudiniAsset.LoadSynchronous();
	if (!IsValid(HoudiniAsset))
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot add tool to category. HoudiniAsset is invalid."));
		return FReply::Handled();
	}
	
	FHoudiniToolsEditor::AddToolToUserCategory(HoudiniAsset, UserCategoryToCreate);

	// const FText Title = LOCTEXT("HoudiniTools_AddToUserCategory_Title", "Created User Category.");
	// const FText Message = LOCTEXT("HoudiniTools_AddToUserCategory_Message", "Added '{0}' to new '{1}' category.");
 //
	//
	// FMessageDialog::Open(EAppMsgType::Ok,
 //        FText::Format( Message, ActiveTool->Name, FText::FromString(UserCategoryToCreate)),
 //        &Title
 //        );
	
	CloseContainingWindow();
	return FReply::Handled();
}

bool
SHoudiniToolAddToUserCategory::GetCreateCategoryEnabled() const
{
	return !UserCategoryToCreate.IsEmpty();
}

EVisibility
SHoudiniToolAddToUserCategory::GetCreateCategoryVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		// Second Screen
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility
SHoudiniToolAddToUserCategory::GetBackButtonVisibility() const
{
	if (WidgetSwitcher->GetActiveWidgetIndex() == 1)
	{
		// Second Screen
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}

FReply
SHoudiniToolAddToUserCategory::HandleCancelClicked()
{
	CloseContainingWindow();
	
	return FReply::Handled();
}

void SHoudiniToolAddToUserCategory::OnUserCategorySelectionChanged(TSharedPtr<FString> NewSelection,
	ESelectInfo::Type SelectType)
{
	SelectedCategory = NewSelection;
}

void SHoudiniToolAddToUserCategory::OnUserCategoryCreateTextChanged(const FText& NewText)
{
	UserCategoryToCreate = NewText.ToString();
}

void SHoudiniToolAddToUserCategory::OnUserCategoryCreateTextCommitted(const FText& NewText,
	ETextCommit::Type CommitType)
{
	UserCategoryToCreate = NewText.ToString();
}

void SHoudiniToolAddToUserCategory::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if ( ContainingWindow.IsValid() )
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SHoudiniToolsPanel::SHoudiniToolsPanel()
	: bRefreshPanelRequested(false)
	, bFilterShowAll(false)
	, bShowEmptyCategories(true)
	, bShowHiddenTools(false)
	, bAutoRefresh(true)
	, bShowHoudiniAssets(true)
	, bShowPresets(true)
	, ViewMode(EHoudiniToolsViewMode::TileView)
{
}

#define UNSUBSCRIBE(Delegate, Handle) \
	if ((Handle).IsValid()) { (Delegate).Remove(Handle); }

SHoudiniToolsPanel::~SHoudiniToolsPanel()
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	// Unsubscribe from delegates
	UNSUBSCRIBE(AssetRegistryModule.Get().OnInMemoryAssetCreated(), AssetMemCreatedHandle);
	UNSUBSCRIBE(AssetRegistryModule.Get().OnInMemoryAssetDeleted(), AssetMemDeletedHandle);
	UNSUBSCRIBE(AssetRegistryModule.Get().OnAssetAdded(), AssetAddedHandle);
	UNSUBSCRIBE(AssetRegistryModule.Get().OnAssetRenamed(), AssetRenamedHandle);
	UNSUBSCRIBE(AssetRegistryModule.Get().OnAssetUpdated(), AssetUpdatedHandle);
	UNSUBSCRIBE(AssetRegistryModule.Get().OnAssetUpdatedOnDisk(), AssetUpdatedOnDiskHandle);

	UHoudiniEngineEditorSettings* HoudiniEngineEditorSettings = const_cast<UHoudiniEngineEditorSettings*>(GetDefault<UHoudiniEngineEditorSettings>());
	if (HoudiniEngineEditorSettings)
	{
		UNSUBSCRIBE(HoudiniEngineEditorSettings->OnUserToolCategoriesChanged, UserToolCategoriesChangedHandle);
	}
	 
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void
SHoudiniToolsPanel::Construct( const FArguments& InArgs )
{
	LoadConfig();
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FHoudiniEngineEditor& HoudiniEngineEditor = FModuleManager::GetModuleChecked<FHoudiniEngineEditor>("HoudiniEngineEditor");
	FHoudiniEngineRuntime& HoudiniEngineRuntime = FModuleManager::GetModuleChecked<FHoudiniEngineRuntime>("HoudiniEngineRuntime");
	// FHoudiniToolsEditor& HoudiniTools = FHoudiniToolsPanelUtils::GetHoudiniTools();

	// Handler to trigger UI updates on asset changes.
	auto AssetChangedHandlerFn = [this](UObject* InObject)
	{
		if (!bAutoRefresh)
			return;
		if (!InObject)
			return;
		if ( InObject->IsA<UHoudiniAsset>() || InObject->IsA<UHoudiniToolsPackageAsset>() || InObject->IsA<UHoudiniPreset>() )
		{
			HandleToolChanged();
		}
	};
	
	AssetMemCreatedHandle = AssetRegistryModule.Get().OnInMemoryAssetCreated().AddLambda(AssetChangedHandlerFn);
	AssetMemDeletedHandle = AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddLambda(AssetChangedHandlerFn);

	// Handler for asset imports. PostImport event is broadcast on both imports and reimports.
	AssetPostImportHandle = GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetReimport.AddLambda(AssetChangedHandlerFn);

	// Handler for asset renames
	AssetRenamedHandle = AssetRegistryModule.Get().OnAssetRenamed().AddLambda([this](const FAssetData& AssetData, const FString& AssetName)
	{
		if (!bAutoRefresh)
			return;
		const UClass* AssetClass = AssetData.GetClass();
		if (!AssetClass)
			return;
		if (AssetClass->IsChildOf(UHoudiniAsset::StaticClass()) ||
			AssetClass->IsChildOf(UHoudiniToolsPackageAsset::StaticClass()) ||
			AssetClass->IsChildOf(UHoudiniPreset::StaticClass())
			)
		{
			HandleToolChanged();
		}
	});

	auto AssetUpdatedHandlerFn = [this](const FAssetData& AssetData)
	{
		if (!bAutoRefresh)
			return;
		const UClass* AssetClass = AssetData.GetClass();
		if (!AssetClass)
			return;
		if (AssetClass->IsChildOf(UHoudiniAsset::StaticClass()) ||
			AssetClass->IsChildOf(UHoudiniToolsPackageAsset::StaticClass()) ||
			AssetClass->IsChildOf(UHoudiniPreset::StaticClass())
			)
		{
			HandleToolChanged();
		}
	};

	// Handlers for asset creation and updates.
	AssetAddedHandle = AssetRegistryModule.Get().OnAssetAdded().AddLambda(AssetUpdatedHandlerFn);
	AssetUpdatedHandle = AssetRegistryModule.Get().OnAssetUpdated().AddLambda(AssetUpdatedHandlerFn);
	AssetUpdatedOnDiskHandle = AssetRegistryModule.Get().OnAssetUpdatedOnDisk().AddLambda(AssetUpdatedHandlerFn);

	// Handler for HoudiniEngineEditorSettings changes
	UHoudiniEngineEditorSettings* HoudiniEngineEditorSettings = GetMutableDefault<UHoudiniEngineEditorSettings>();
	if (HoudiniEngineEditorSettings)
	{
		UserToolCategoriesChangedHandle = HoudiniEngineEditorSettings->OnUserToolCategoriesChanged.AddLambda([&]()
		{
			RequestPanelRefresh();
		});
	}

	// Handler for tool or package change broadcasts
	ToolOrPackageChangedHandle = HoudiniEngineRuntime.GetOnToolOrPackageChangedEvent().AddLambda([&]()
	{
		RequestPanelRefresh();
	});

	UpdateHoudiniToolDirectories();

	CategoriesContainer = SNew(SVerticalBox);

	RebuildCategories();

	TSharedPtr<SLayeredImage> OptionsImage = SNew(SLayeredImage)
		 .Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
		 .ColorAndOpacity(FSlateColor::UseForeground());

	TSharedPtr<SLayeredImage> FilterImage = SNew(SLayeredImage)
		 .Image(FAppStyle::Get().GetBrush("Icons.Filter"))
		 .ColorAndOpacity(FSlateColor::UseForeground());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNew(SVerticalBox)

			// Toolbar
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5,5)
			[
				SNew(SHorizontalBox)

				// Search Box
				
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSearchBox)
					.OnTextChanged_Lambda([this](const FText& NewFilterString) -> void
					{
						// Handle Filter Text Changes.
						if (NewFilterString.ToString() != FilterString)
						{
							// Search filter changed. Update categories.
							FilterString = NewFilterString.ToString();
							ForEachCategory([this](SHoudiniToolCategory* Category) -> bool
							{
								Category->SetFilterString(FilterString);
								return true;
							});
						}
					})
				] // Search Box

				// Filter Menu Button
			
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.HasDownArrow(false)
					.ContentPadding(0)     
					.ForegroundColor( FSlateColor::UseForeground() )
					.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
					.OnGetMenuContent_Lambda([&]()
					{
						// This menu widget will be rebuilt dynamically when the menu is needed
						return SAssignNew(CategoryFilterWidget, SHoudiniToolsCategoryFilter)
						.ShowAll_Lambda([this]() { return bFilterShowAll; })
						.CategoriesSource(&FilterCategoryList)
						.HiddenCategoriesSource(&FilterHiddenCategories)
						.OnShowAllChanged_Lambda([this](bool NewShowAll)
						{
							bFilterShowAll = NewShowAll;
							if (bFilterShowAll)
							{
								FilterHiddenCategories.Empty();
							}
							else
							{
								// Set all categories as hidden
								ForEachCategory([&](SHoudiniToolCategory* Category) -> bool
								{
									if (Category)
									{
										FilterHiddenCategories.Add(Category->GetCategoryLabel());
									}
									return true;
								});
							}
							SaveConfig();
							RequestPanelRefresh();
						})
						.OnCategoryStateChanged_Lambda([this] (const FString& CategoryName, const bool bIsEnabled)
						{
							if (bIsEnabled)
							{
								FilterHiddenCategories.Remove(CategoryName);
							}
							else
							{
								FilterHiddenCategories.Add(CategoryName);
							}
							SaveConfig();
							RequestPanelRefresh();
						});
					})
					.ButtonContent()
					[
						FilterImage.ToSharedRef()
					]
				] // Filter menu button

				// Action Menu Button
			
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.HasDownArrow(false)
					.ContentPadding(0)
					.ForegroundColor( FSlateColor::UseForeground() )
					.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
					.MenuContent()
					[
						ConstructHoudiniToolsActionMenu().ToSharedRef()
					]
					.ButtonContent()
					[
						OptionsImage.ToSharedRef()
					]
				]
			]

			// Categories Container
			
			+ SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Visibility_Lambda([this]() -> EVisibility
				{
					// Only show the categories panel if we have categories
					if (CategoriesContainer.IsValid() && CategoriesContainer->NumSlots() > 0)
					{
						return EVisibility::Visible;
					}
					return EVisibility::Collapsed;
				})
				[
					SNew(SScrollBox)
					.ScrollBarAlwaysVisible(true)
					+ SScrollBox::Slot()
					[
						CategoriesContainer.ToSharedRef() // vertical box
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				.Visibility_Lambda([this]() -> EVisibility
				{
					// Show the "Import SideFX Tools" panel only if we don't have any existing categories.
					if (CategoriesContainer.IsValid() && CategoriesContainer->NumSlots() == 0)
					{
						return EVisibility::Visible;
					}
					return EVisibility::Collapsed;
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("HoudiniTools_ImportSideFXToolsPanel_Description", "No tools here yet! Import default SideFX Tools?"))
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.f, 10.f))
						.AutoHeight()
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
							.ButtonStyle(&_GetEditorStyle().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
							.OnClicked_Lambda([&]() -> FReply
							{
								HandleImportSideFXTools();
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Text(LOCTEXT( "ImportPackage_CreateButtonLabel", "Import SideFX Tools" ))
							]
						]
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef< ITableRow >
SHoudiniToolsPanel::MakeListViewWidget( TSharedPtr< FHoudiniTool > HoudiniTool, const FString& CategoryName, const TSharedRef< STableViewBase >& OwnerTable )
{
	check( HoudiniTool.IsValid() );
 
	auto DefaultTool = FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.HoudiniEngineLogo");
	// TSharedPtr< SButton > HelpButton;
	TSharedPtr< SImage > DefaultToolImage;

	// Building the tool's tooltip
	FString HoudiniToolTip;
	if (HoudiniTool->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		// HoudiniToolTip += HoudiniTool->Name.ToString() + TEXT(" (Preset) ") + TEXT( "\n\n" );
		HoudiniToolTip += HoudiniTool->Name.ToString() + TEXT(" - Preset") + TEXT( "\n\n" );
		if ( HoudiniTool->HoudiniPreset.IsValid() )
			HoudiniToolTip += HoudiniTool->HoudiniPreset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
	}
	else
	{
		HoudiniToolTip += HoudiniTool->Name.ToString() + TEXT(" - Houdini Asset") + TEXT( "\n\n" );
		if ( HoudiniTool->HoudiniAsset.IsValid() )
			HoudiniToolTip += HoudiniTool->HoudiniAsset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
	}
		
	// if ( HoudiniTool->HoudiniAsset.IsValid() )
	// 	HoudiniToolTip += HoudiniTool->HoudiniAsset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
	if ( !HoudiniTool->ToolTipText.IsEmpty() )
		HoudiniToolTip += HoudiniTool->ToolTipText.ToString() + TEXT("\n\n");

	// Adding a description of the tools behavior deriving from its type
	switch ( HoudiniTool->Type )
	{
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE:
			HoudiniToolTip += TEXT("Operator (Single):\nDouble clicking on this tool will instantiate the asset in the world.\nThe current selection will be automatically assigned to the asset's first input.\n" );
			break;
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI:
			HoudiniToolTip += TEXT("Operator (Multiple):\nDouble clicking on this tool will instantiate the asset in the world.\nEach of the selected object will be assigned to one of the asset's input (object1 to input1, object2 to input2 etc.)\n" );
			break;
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH:
			HoudiniToolTip += TEXT("Operator (Batch):\nDouble clicking on this tool will instantiate the asset in the world.\nAn instance of the asset will be created for each of the selected object, and the asset's first input will be set to that object.\n");
			break;
		case EHoudiniToolType::HTOOLTYPE_GENERATOR:
		default:
			HoudiniToolTip += TEXT("Generator:\nDouble clicking on this tool will instantiate the asset in the world.\n");
			break;
	}

	// Add a description from the tools selection type
	if ( HoudiniTool->Type != EHoudiniToolType::HTOOLTYPE_GENERATOR )
	{
		switch ( HoudiniTool->SelectionType )
		{
			case EHoudiniToolSelectionType::HTOOL_SELECTION_ALL:
				HoudiniToolTip += TEXT("\nBoth Content Browser and World Outliner selection will be considered.\nIf objects are selected in the content browser, the world selection will be ignored.\n");
				break;

			case EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY:
				HoudiniToolTip += TEXT("\nOnly World Outliner selection will be considered.\n");
				break;

			case EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY:
				HoudiniToolTip += TEXT("\nOnly Content Browser selection will be considered.\n");
				break;
		}
	}

	if ( HoudiniTool->Type != EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH )
	{
		if ( HoudiniTool->SelectionType != EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY )
			HoudiniToolTip += TEXT( "If objects are selected in the world outliner, the asset's transform will default to the mean Transform of the select objects.\n" );
	}
	else
	{
		if ( HoudiniTool->SelectionType != EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY )
			HoudiniToolTip += TEXT( "If objects are selected in the world outliner, each created asset will use its object transform.\n" );
	}

	FText ToolTipText = FText::FromString( HoudiniToolTip );

	// Build the Help Tooltip
	FString HelpURL = HoudiniTool->HelpURL;
	bool HasHelp = HelpURL.Len() > 0;
	FText HelpTip = HasHelp ? FText::Format( LOCTEXT( "OpenHelp", "Click to view tool help: {0}" ), FText::FromString( HelpURL ) ) : HoudiniTool->Name;

	// // Build the "DefaultTool" tool tip
	// bool IsDefault = HoudiniTool->DefaultTool;
	// FText DefaultToolText = FText::FromString( TEXT("Default Houdini Engine for Unreal plug-in tool") );

	TSharedRef< STableRow<TSharedPtr<FHoudiniTool>> > TableRowWidget =
		SNew( STableRow<TSharedPtr<FHoudiniTool>>, OwnerTable )
		.Style( FHoudiniEngineStyle::Get(), "HoudiniEngine.TableRow" )
		.OnDragDetected( this, &SHoudiniToolsPanel::OnDraggingListViewWidget );

	float IconOpacity = 1.f;

	if (HoudiniTool->IsHiddenInCategory(CategoryName))
	{
		IconOpacity = 0.5f;
	}
	
	TSharedPtr< SHorizontalBox > ContentBox;
	TSharedRef<SWidget> Content =
		SNew( SBorder )
		.BorderImage( FCoreStyle::Get().GetBrush( "NoBorder" ) )
		.Padding( 0 )
		.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
		.Cursor( EMouseCursor::GrabHand )
		.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, IconOpacity))
		[ SAssignNew( ContentBox, SHorizontalBox ) ];

	constexpr float IconWidth = 36.f;

	// Tool Icon
	const FSlateBrush* ToolIcon = nullptr;
	if (HoudiniTool->Icon)
	{
		ToolIcon = HoudiniTool->Icon.Get();
	}
	else
	{
		ToolIcon = FHoudiniEngineStyle::Get()->GetBrush( FName(FHoudiniToolsEditor::GetDefaultHoudiniToolIconBrushName()) );
	}

	TSharedPtr<SOverlay> IconOverlay = SNew(SOverlay);

	IconOverlay->AddSlot().AttachWidget(
		SNew( SBorder )
			.BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailBackground" ) )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			[
				SNew( SImage )
				.Image( ToolIcon )
				.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
			]
		);

	UnderlineBrush.DrawAs = ESlateBrushDrawType::Box;
	UnderlineBrush.ImageSize = FVector2D(2.f, 2.f);

	FSlateColor UnderlineColor;

	if (HoudiniTool->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		UnderlineColor = FSlateColor(FLinearColor(0.0f, 0.3f, 1.0f, 1.f));
	}
	else
	{
		// Default (HoudiniAsset) color
		UnderlineColor = FSlateColor(FLinearColor(1.0f, 0.25f, 0.0f, 1.f));
	}
	

	// Add icon underline using UnderlineColor
	IconOverlay->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
	.AttachWidget(
		SNew(SImage)
		.Image(&UnderlineBrush)
		.ColorAndOpacity( UnderlineColor )
		.DesiredSizeOverride(FVector2D(10.0f, 2.f))
		);

	ContentBox->AddSlot()
		.AutoWidth()
		[
			SNew( SBorder )
			.Padding( 4.0f )
			.BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailShadow" ) )
			[
				SNew( SBox )
				.WidthOverride( IconWidth )
				.HeightOverride( IconWidth )
				[
					IconOverlay.ToSharedRef()
					// SNew( SBorder )
					// .BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailBackground" ) )
					// .HAlign( HAlign_Center )
					// .VAlign( VAlign_Center )
					// [
					// 	SNew( SImage )
					// 	.Image_Lambda([ToolIcon]()->const FSlateBrush*
					// 	{
					// 		if (ToolIcon.IsValid())
					// 		{
					// 			return ToolIcon.Get();
					// 		}
					// 		else
					// 		{
					// 			return FHoudiniEngineStyle::Get()->GetBrush( TEXT( "HoudiniEngine.HoudiniEngineLogo40" ) );
					// 		}
					// 	})
					// 	// .Image( ToolIcon )
					// 	.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
					// ]
				]
			]
		];

	// Tool name + tooltip
	ContentBox->AddSlot()
		.HAlign( HAlign_Left )
		.VAlign( VAlign_Center )
		.FillWidth( 1.f )
		[
			SNew( STextBlock )
			.TextStyle( FHoudiniEngineStyle::Get(), "HoudiniEngine.ThumbnailText" )
			.HighlightText_Lambda([&]()
			{
				if (FilterString.Len() > 0)
				{
					return FText::FromString(FilterString);
				}
				return FText();
			})
			.Text( HoudiniTool->Name )
			.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
		];

	if ( HasHelp )
	{
		// const FButtonStyle* BtnStyle = &FHoudiniEngineStyle::Get()->GetWidgetStyle<FButtonStyle>("HoudiniEngine.HelpButton");

		ContentBox->AddSlot()
		.VAlign( VAlign_Center )
		.AutoWidth()
		[
			// Help Button
		
			SNew( SButton )
			.ContentPadding( 0 )
			.ButtonStyle( FHoudiniEngineStyle::Get(), "HoudiniEngine.HelpButton" )
			// .ButtonStyle( &DGB_HelpButtonStyle )
			.OnClicked_Lambda( [ &, HelpURL ]() {
				if ( HelpURL.Len() )
				{
					FPlatformProcess::LaunchURL( *HelpURL, nullptr, nullptr );
				}
				return FReply::Handled();
				} )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.ToolTip( SNew( SToolTip ).Text( HelpTip ) )
			[
				// Help Button Image
			
				// At the time of writing, the UE help icon looks better than the Houdini Engine help icon.
				SNew( SImage )
				.Image( _GetBrush("Icons.Help") )
				// .Image( FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.AssetHelp") )
			]
		];
	}

	TableRowWidget->SetContent( Content );

	return TableRowWidget;
}

TSharedRef< ITableRow >
SHoudiniToolsPanel::MakeTileViewWidget( TSharedPtr< FHoudiniTool > HoudiniTool, const FString& CategoryName, const TSharedRef< STableViewBase >& OwnerTable )
{
	check( HoudiniTool.IsValid() );
	
	auto DefaultTool = FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.HoudiniEngineLogo");
	TSharedPtr< SImage > HelpButtonImage;
	TSharedPtr< SButton > HelpButton;
	TSharedPtr< SImage > DefaultToolImage;

	// Building the tool's tooltip
	FString HoudiniToolTip;
	if (HoudiniTool->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		// HoudiniToolTip += HoudiniTool->Name.ToString() + TEXT(" (Preset) ") + TEXT( "\n\n" );
		HoudiniToolTip += HoudiniTool->Name.ToString() + TEXT(" - Preset") + TEXT( "\n\n" );
		if ( HoudiniTool->HoudiniPreset.IsValid() )
			HoudiniToolTip += HoudiniTool->HoudiniPreset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
	}
	else
	{
		HoudiniToolTip += HoudiniTool->Name.ToString() + TEXT(" - Houdini Asset") + TEXT( "\n\n" );
		if ( HoudiniTool->HoudiniAsset.IsValid() )
			HoudiniToolTip += HoudiniTool->HoudiniAsset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
	}
		
	// if ( HoudiniTool->HoudiniAsset.IsValid() )
	// 	HoudiniToolTip += HoudiniTool->HoudiniAsset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
	if ( !HoudiniTool->ToolTipText.IsEmpty() )
		HoudiniToolTip += HoudiniTool->ToolTipText.ToString() + TEXT("\n\n");

	// Adding a description of the tools behavior deriving from its type
	switch ( HoudiniTool->Type )
	{
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE:
			HoudiniToolTip += TEXT("Operator (Single):\nDouble clicking on this tool will instantiate the asset in the world.\nThe current selection will be automatically assigned to the asset's first input.\n" );
			break;
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI:
			HoudiniToolTip += TEXT("Operator (Multiple):\nDouble clicking on this tool will instantiate the asset in the world.\nEach of the selected object will be assigned to one of the asset's input (object1 to input1, object2 to input2 etc.)\n" );
			break;
		case EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH:
			HoudiniToolTip += TEXT("Operator (Batch):\nDouble clicking on this tool will instantiate the asset in the world.\nAn instance of the asset will be created for each of the selected object, and the asset's first input will be set to that object.\n");
			break;
		case EHoudiniToolType::HTOOLTYPE_GENERATOR:
		default:
			HoudiniToolTip += TEXT("Generator:\nDouble clicking on this tool will instantiate the asset in the world.\n");
			break;
	}

	// Add a description from the tools selection type
	if ( HoudiniTool->Type != EHoudiniToolType::HTOOLTYPE_GENERATOR )
	{
		switch ( HoudiniTool->SelectionType )
		{
			case EHoudiniToolSelectionType::HTOOL_SELECTION_ALL:
				HoudiniToolTip += TEXT("\nBoth Content Browser and World Outliner selection will be considered.\nIf objects are selected in the content browser, the world selection will be ignored.\n");
				break;

			case EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY:
				HoudiniToolTip += TEXT("\nOnly World Outliner selection will be considered.\n");
				break;

			case EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY:
				HoudiniToolTip += TEXT("\nOnly Content Browser selection will be considered.\n");
				break;
		}
	}

	if ( HoudiniTool->Type != EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH )
	{
		if ( HoudiniTool->SelectionType != EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY )
			HoudiniToolTip += TEXT( "If objects are selected in the world outliner, the asset's transform will default to the mean Transform of the select objects.\n" );
	}
	else
	{
		if ( HoudiniTool->SelectionType != EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY )
			HoudiniToolTip += TEXT( "If objects are selected in the world outliner, each created asset will use its object transform.\n" );
	}

	FText ToolTipText = FText::FromString( HoudiniToolTip );

	// Build the Help Tooltip
	FString HelpURL = HoudiniTool->HelpURL;
	bool HasHelp = HelpURL.Len() > 0;
	FText HelpTip = HasHelp ? FText::Format( LOCTEXT( "OpenHelp", "Click to view tool help: {0}" ), FText::FromString( HelpURL ) ) : HoudiniTool->Name;

	// // Build the "DefaultTool" tool tip
	// bool IsDefault = HoudiniTool->DefaultTool;
	// FText DefaultToolText = FText::FromString( TEXT("Default Houdini Engine for Unreal plug-in tool") );

	TSharedRef< STableRow<TSharedPtr<FHoudiniTool>> > TableRowWidget =
		SNew( STableRow<TSharedPtr<FHoudiniTool>>, OwnerTable )
		.Style( FHoudiniEngineStyle::Get(), "HoudiniEngine.TableRow" )
		.OnDragDetected( this, &SHoudiniToolsPanel::OnDraggingListViewWidget )
		;

	float IconOpacity = 1.f;

	if (HoudiniTool->IsHiddenInCategory(CategoryName))
	{
		IconOpacity = 0.5f;
	}

	TSharedPtr< SVerticalBox > ContentBox;
	TSharedRef<SOverlay> Content = SNew(SOverlay);

	Content->AddSlot().AttachWidget(
		SNew( SBorder )
		.BorderImage( FCoreStyle::Get().GetBrush( "NoBorder" ) )
		.Padding( 0 )
		.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
		.Cursor( EMouseCursor::GrabHand )
		.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, IconOpacity))
		[
			SAssignNew( ContentBox, SVerticalBox )
		]);

	constexpr float IconWidth = 36.f;

	// Tool Icon
	const FSlateBrush* ToolIcon = nullptr;
	if (HoudiniTool->Icon)
	{
		ToolIcon = HoudiniTool->Icon.Get();
	}
	else
	{
		ToolIcon = FHoudiniEngineStyle::Get()->GetBrush( FName(FHoudiniToolsEditor::GetDefaultHoudiniToolIconBrushName()) );
	}

	TSharedPtr<SOverlay> IconOverlay = SNew(SOverlay);
	
	IconOverlay->AddSlot().AttachWidget(
		SNew( SBorder )
			.BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailBackground" ) )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			[
				SNew( SImage )
				.Image( ToolIcon )
				.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
			]
		);
	
	UnderlineBrush.DrawAs = ESlateBrushDrawType::Box;
	UnderlineBrush.ImageSize = FVector2D(2.f, 2.f);

	FSlateColor UnderlineColor;

	if (HoudiniTool->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		UnderlineColor = FSlateColor(FLinearColor(0.0f, 0.3f, 1.0f, 1.f));
	}
	else
	{
		// Default (HoudiniAsset) color
		UnderlineColor = FSlateColor(FLinearColor(1.0f, 0.25f, 0.0f, 1.f));
	}
	

	// Add icon underline using UnderlineColor
	IconOverlay->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
	.AttachWidget(
		SNew(SImage)
		.Image(&UnderlineBrush)
		.ColorAndOpacity( UnderlineColor )
		.DesiredSizeOverride(FVector2D(10.0f, 2.f))
		);

	ContentBox->AddSlot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.AutoHeight()
		[
			SNew( SBorder )
			.Padding( 4.0f )
			.BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailShadow" ) )
			[
				SNew( SBox )
				.WidthOverride( IconWidth )
				.HeightOverride( IconWidth )
				[
					IconOverlay.ToSharedRef()
				]
			]
		];

	// Tool name + tooltip
	ContentBox->AddSlot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		.FillHeight( 1.f )
		[
			SNew( STextBlock )
			.TextStyle( FHoudiniEngineStyle::Get(), "HoudiniEngine.ThumbnailText" )
			.Text( HoudiniTool->Name )
			.HighlightText_Lambda([&]()
			{
				if (FilterString.Len() > 0)
				{
					return FText::FromString(FilterString);
				}
				return FText();
			})
			.ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
			.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
		];

	TableRowWidget->SetContent( Content );

	return TableRowWidget;
}

void
SHoudiniToolsPanel::OnToolSelectionChanged( SHoudiniToolCategory* Category, const TSharedPtr<FHoudiniTool>& HoudiniTool, ESelectInfo::Type SelectionType )
{
	if (!Category)
		return;

	if ( HoudiniTool.IsValid() )
	{
		// Clear selections on any other categories.
		ForEachCategory([&Category](SHoudiniToolCategory* InCategory)
		{
			if (!InCategory)
				return true;
			if (InCategory != Category)
			{
				// The child category differs from the category with the last selection. Clear its selection.
				InCategory->ClearSelection();
			}
			return true;
		});
		// After we have cleared selections on the all the other categories, set the current Houdini Tool as active.
		ActiveTool = HoudiniTool;
		ActiveCategoryName = Category->GetCategoryLabel();
	}
	else
	{
		ActiveTool = nullptr;
		ActiveCategoryName.Empty();
	}
}

FReply
SHoudiniToolsPanel::OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		if ( ActiveTool.IsValid() )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
			UObject* AssetObj = ActiveTool->HoudiniAsset.LoadSynchronous();
			if ( AssetObj )
			{
				FString AssetPathName;

				UHoudiniPreset* Preset = ActiveTool->HoudiniPreset.LoadSynchronous();
				UActorFactory* Factory = nullptr;
				
				if (IsValid(Preset))
				{
					Factory = GEditor->FindActorFactoryByClass( UHoudiniPresetActorFactory::StaticClass() );
					AssetPathName = Preset->GetPathName();
				}

				if (!IsValid(Factory))
				{
					Factory = GEditor->FindActorFactoryForActorClass( AHoudiniAssetActor::StaticClass() );
					AssetPathName = AssetObj->GetPathName();
				}

				
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
				FAssetData BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPathName);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				FAssetData BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(*AssetPathName));
#endif

				return FReply::Handled().BeginDragDrop( FAssetDragDropOp::New( BackingAssetData, Factory ) );
			}
		}
	}

	return FReply::Unhandled();
}

void
SHoudiniToolsPanel::OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniTool> HoudiniTool )
{
	if ( !HoudiniTool.IsValid() )
		return;

	return InstantiateHoudiniTool( HoudiniTool.Get() );
}

void 
SHoudiniToolsPanel::InstantiateHoudiniTool( FHoudiniTool* HoudiniTool )
{
	if ( !HoudiniTool )
		return;
	
	// Load the asset
	UHoudiniAsset* AssetObj = Cast<UHoudiniAsset>(HoudiniTool->HoudiniAsset.LoadSynchronous());
	if ( !AssetObj )
		return;

	FHoudiniEngineEditorUtils::InstantiateHoudiniAsset(AssetObj, HoudiniTool->Type, HoudiniTool->SelectionType, HoudiniTool->HoudiniPreset.LoadSynchronous());
}

FTransform
SHoudiniToolsPanel::GetDefaulToolSpawnTransform()
{
	FTransform SpawnTransform = FTransform::Identity;

	// Get the editor viewport LookAt position to spawn the new objects
	if ( GEditor && GEditor->GetActiveViewport() )
	{
		FEditorViewportClient* ViewportClient = (FEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
		if ( ViewportClient )
		{
			// We need to toggle the orbit camera on to get the proper LookAtLocation to spawn our asset
			ViewportClient->ToggleOrbitCamera( true );
			SpawnTransform.SetLocation( ViewportClient->GetLookAtLocation() );
			ViewportClient->ToggleOrbitCamera( false );
		}
	}

	return SpawnTransform;
}

FTransform
SHoudiniToolsPanel::GetMeanWorldSelectionTransform()
{
	FTransform SpawnTransform = GetDefaulToolSpawnTransform();

	if ( GEditor && ( GEditor->GetSelectedActorCount() > 0 ) )
	{
		// Get the current Level Editor Selection
		USelection* SelectedActors = GEditor->GetSelectedActors();

		int NumAppliedTransform = 0;
		for ( FSelectionIterator It( *SelectedActors ); It; ++It )
		{
			AActor * Actor = Cast< AActor >( *It );
			if ( !Actor )
				continue;

			// Just Ignore the SkySphere...
			FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
			if ( ClassName == TEXT( "BP_Sky_Sphere_C" ) )
				continue;

			FTransform CurrentTransform = Actor->GetTransform();

			ALandscapeProxy* Landscape = Cast< ALandscapeProxy >( Actor );
			if ( Landscape )
			{
				// We need to offset Landscape's transform in X/Y to center them properly
				FVector Origin, Extent;
				Actor->GetActorBounds(false, Origin, Extent);

				// Use the origin's XY Position
				FVector Location = CurrentTransform.GetLocation();
				Location.X = Origin.X;
				Location.Y = Origin.Y;
				CurrentTransform.SetLocation( Location );
			}

			// Accumulate all the actor transforms...
			if ( NumAppliedTransform == 0 )
				SpawnTransform = CurrentTransform;
			else
				SpawnTransform.Accumulate( CurrentTransform );

			NumAppliedTransform++;
		}

		if ( NumAppliedTransform > 0 )
		{
			// "Mean" all the accumulated Transform
			SpawnTransform.SetScale3D( FVector::OneVector );
			SpawnTransform.NormalizeRotation();

			if ( NumAppliedTransform > 1 )
				SpawnTransform.SetLocation( SpawnTransform.GetLocation() / (float)NumAppliedTransform );
		}
	}

	return SpawnTransform;
}


// TSharedPtr<SWidget>
// SHoudiniToolsPanel::ConstructContextMenu(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType)
// {
// 	FHoudiniToolCategory Category(CategoryName, CategoryType);
// 	if (ActiveTool.IsValid() && GetHoudiniTools().IsToolInCategory(Category, ActiveTool))
// 	{
// 		// if there is an active tool, and the context menu was summoned from the same category, only then do we
// 		// open the Tool context menu.
// 		return ConstructHoudiniToolContextMenu();
// 	}
// 	
// 	return ConstructCategoryContextMenu(CategoryName, CategoryType);
// }


// Builds the context menu for the selected tool
TSharedPtr<SWidget>
SHoudiniToolsPanel::ConstructHoudiniToolContextMenu()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked< FAssetRegistryModule >( "AssetRegistry" );

	FMenuBuilder MenuBuilder( true, NULL );
	
	// Prevent crashes caused by null active tool
	if (!ActiveTool.IsValid())
	{
		return MenuBuilder.MakeWidget();
	}

	bool bHasHelp = false;
	bool bIsFavoritesCategory = ActiveCategoryName == FHoudiniToolsEditor::GetFavoritesCategoryName().ToString();
	FString HelpURL;
	TArray< UObject* > AssetArray;
	EHoudiniToolCategoryType CategoryType = EHoudiniToolCategoryType::Package; 
	const UClass* AssetClass = nullptr;
	UObject* AssetObj = ActiveTool->GetAssetObject();
	if ( ActiveTool.IsValid() && !ActiveTool->HoudiniAsset.IsNull() )
	{
		// Load the underlying asset (HoudiniAsset or HoudiniPreset) for the active tool 
		if( AssetObj )
		{
			AssetArray.Add( AssetObj );
		}

		HelpURL = ActiveTool->HelpURL;
		bHasHelp = !HelpURL.IsEmpty();
		CategoryType = ActiveTool->CategoryType;
		AssetClass = ActiveTool->GetAssetObjectClass();
	}

	if (!IsValid(AssetClass))
	{
		return MenuBuilder.MakeWidget();
	}

	FAssetToolsModule & AssetToolsModule = FModuleManager::GetModuleChecked< FAssetToolsModule >( "AssetTools" );
	TSharedPtr< IAssetTypeActions > EngineActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( AssetClass ).Pin();
	if ( EngineActions.IsValid() )
		EngineActions->GetActions( AssetArray, MenuBuilder );

	// Add HoudiniTools actions
	MenuBuilder.AddMenuSeparator();

	if (FHoudiniToolsEditor::UserCategoryContainsTool("Favorites", AssetObj, ActiveTool->ToolsPackage.LoadSynchronous()))
	{
		// Remove From Favorites
		const FText HideLabel = LOCTEXT( "HoudiniTool_ContextMenu_ShowInPackageCategory", "Unfavorite" );
		const FText HideTooltip = LOCTEXT( "HoudiniTool_ContextMenu_ShowInPackageCategoryTooltip", "Remove this tool from the 'Favorites' user category." );
		MenuBuilder.AddMenuEntry(
			HideLabel,
			HideTooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::HandleRemoveFromFavorites )
			)
		);
	}
	else
	{
		// Add To Favorites
		const FText HideLabel = LOCTEXT( "HoudiniTool_ContextMenu_ShowInPackageCategory", "Favorite" );
		const FText HideTooltip = LOCTEXT( "HoudiniTool_ContextMenu_ShowInPackageCategoryTooltip", "Add this tool the 'Favorites' user category" );
		MenuBuilder.AddMenuEntry(
			HideLabel,
			HideTooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::HandleAddToFavorites )
			)
		);
	}

	if (!bIsFavoritesCategory)
	{
		if (ActiveTool->IsHiddenInCategory(ActiveCategoryName))
		{
			// The tool is already hidden in this category. Allow the user to 'show' this tool in this category.
			// TODO: First check whether it's even possible to remove this tool from the exclusion list. If not, disable these options.
			const FText HideLabel = LOCTEXT( "HoudiniTool_ContextMenu_ShowInPackageCategory", "Make Visible In Category" );
			const FText HideTooltip = LOCTEXT( "HoudiniTool_ContextMenu_ShowInPackageCategoryTooltip", "Make this tool visible in the Package Category by removing it from the category exclusion list, if possible." );
			MenuBuilder.AddMenuEntry(
				HideLabel,
				HideTooltip,
				FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
				FUIAction(
					FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::ShowActiveToolInCategory ),
					FCanExecuteAction::CreateLambda([this] { return IsActiveHoudiniToolEditable() && CanShowActiveToolInCategory(); } )
				)
			);
		}
		else
		{
			// Hide Tool from category
			const FText HideLabel = CategoryType == EHoudiniToolCategoryType::Package ?
				LOCTEXT( "HoudiniTool_ContextMenu_HideFromPackageCategory", "Hide From Category" ) :
				LOCTEXT( "HoudiniTool_ContextMenu_RemoveFromPackageCategory", "Remove From Category" );
			const FText HideTooltip = CategoryType == EHoudiniToolCategoryType::Package ?
				LOCTEXT( "HoudiniTool_ContextMenu_HideFromPackageCategoryTooltip", "Hide the selected tool from this package category." ) :
				LOCTEXT( "HoudiniTool_ContextMenu_RemoveFromPackageCategoryTooltip", "Remove the selected tool from this user category." );
			MenuBuilder.AddMenuEntry(
				HideLabel,
				HideTooltip,
				FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
				FUIAction(
					FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::HideActiveToolFromCategory ),
					FCanExecuteAction::CreateLambda([this] { return IsActiveHoudiniToolEditable(); } )
				)
			);
		}
	}

	// Add To User Category
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "HoudiniTool_ContextMenu_AddToUserCategory", "Add To User Category" ),
		LOCTEXT("HoudiniTool_ContextMenu_AddToUserCategoryTooltip", "Add the selected tool a user category." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::HandleAddToolToUserCategory ),
			// FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
			FCanExecuteAction()
		)
	);

	// Add HoudiniTools actions
	MenuBuilder.AddMenuSeparator();

	if (ActiveTool->PackageToolType == EHoudiniPackageToolType::Preset)
	{
		// Browse to tool package in the content browser 
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_BrowseToPreset", "Browse to Preset" ),
			NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_BrowseToPresetTooltip", "Browse to the preset in the content browser." ),
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::BrowseToActiveToolAsset ),
				FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
			)
		);
		
		// Browse to tool package in the content browser 
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_BrowseToSourceAsset", "Browse to Source Asset" ),
			NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_BrowseToSourceAssetTooltip", "Browse to the preset's source asset in the content browser." ),
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::BrowseToSourceAsset ),
				FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
			)
		);
	}

	if (ActiveTool->PackageToolType == EHoudiniPackageToolType::HoudiniAsset)
	{
		// Browse to tool package in the content browser 
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_BrowseToAsset", "Browse to Asset" ),
			NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_BrowseToAssetTooltip", "Browse to the tool asset in the content browser." ),
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::BrowseToActiveToolAsset ),
				FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
			)
		);
	}
	


	// Browse to tool package in the content browser 
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_FindPackage", "Browse to Package" ),
		NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_FindPackageTooltip", "Browse to the owning package in the content browser." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::BrowseToActiveToolPackage ),
			FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
		)
	);

	// Launch Tool Help
	const FText ToolHelpTooltip = NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_HelpTooltip", "Launch the browser to open the tools help." );
	const FText ToolNoHelpTooltip = NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_NoHelpTooltip", "This tool doesn't have a Help URL." );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_Edit", "Open Tool Help" ),
		bHasHelp ? ToolHelpTooltip : ToolNoHelpTooltip,
		// FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FSlateIcon( FAppStyle::GetAppStyleSetName(), "Icons.Help" ),
		FUIAction(
			FExecuteAction::CreateLambda([HelpURL]()
			{
				if (!HelpURL.IsEmpty())
				{
					FPlatformProcess::LaunchURL( *HelpURL, nullptr, nullptr );
				}
			} ),
			FCanExecuteAction::CreateLambda([bHasHelp] { return bHasHelp; } )
		)
	);

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SHoudiniToolsPanel::ConstructCategoryContextMenu(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType)
{
	FMenuBuilder MenuBuilder( true, NULL );

	TArray<FString> UserCategories;
	FHoudiniToolsEditor::GetUserCategoriesList(UserCategories);

	const bool bIsUserCategory = UserCategories.Contains(CategoryName);
	const bool bIsFavoriteCategory = CategoryName == FHoudiniToolsEditor::GetFavoritesCategoryName().ToString();

	MenuBuilder.BeginSection("HoudiniTool_CategoryContextMenu_CategorySection", FText::FromString(CategoryName) );

	if (bIsUserCategory)
	{
		// Edit User Category
		const FText Label = LOCTEXT( "HoudiniTool_CategoryContextMenu_EditCategory", "Edit User Category" );
		const FText Tooltip = LOCTEXT( "HoudiniTool_CategoryContextMenu_EditCategoryTooltip", "Edit user settings containing this category." );
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateLambda([this, CategoryName](){ HandleEditUserCategory(CategoryName); }),
				FCanExecuteAction::CreateLambda([bIsFavoriteCategory]() { return !bIsFavoriteCategory; })
			)
		);
	}
	else
	{
		// Edit Package Category
		const FText Label = LOCTEXT( "HoudiniTool_CategoryContextMenu_EditCategory", "Edit Category" );
		const FText Tooltip = LOCTEXT( "HoudiniTool_CategoryContextMenu_EditCategoryTooltip", "Edit the package containing this category." );
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateLambda([this, CategoryName](){ HandleEditPackageCategory(CategoryName); })
			)
		);
	}

	// Browse To
	{
		const FText Label = LOCTEXT( "HoudiniTool_CategoryContextMenu_BrowseTo", "Browse To" );
		const FText Tooltip = LOCTEXT( "HoudiniTool_CategoryContextMenu_BrowseToTooltip", "Browse to the package containing this category." );
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateLambda([this, CategoryName](){ HandleBrowseToCategoryPackage(CategoryName); }),
				FCanExecuteAction::CreateLambda([bIsUserCategory]() { return !bIsUserCategory; } )
			)
		);
	}

	// Save Category Tools
	{
		const FText Label = LOCTEXT( "HoudiniTool_CategoryContextMenu_SaveCategory", "Save Category Tools" );
		const FText Tooltip = LOCTEXT( "HoudiniTool_CategoryContextMenu_SaveCategoryTooltip", "Save all the unsaved tools in this category." );
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateLambda([this, CategoryName, CategoryType](){ HandleSaveToolsInCategory(CategoryName, CategoryType); })
			)
		);
	}

	// Import HoudiniTool into category
	{
		const FText Label = LOCTEXT( "HoudiniTool_CategoryContextMenu_ImportTool", "Import HoudiniTool" );
		const FText Tooltip = LOCTEXT( "HoudiniTool_CategoryContextMenu_ImportToolTooltip", "Import HoudiniTool into this category." );
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
			FUIAction(
				FExecuteAction::CreateLambda([this, CategoryName, CategoryType](){ HandleImportToolsIntoCategory(CategoryName, CategoryType); }),
				FCanExecuteAction::CreateLambda([bIsUserCategory] { return !bIsUserCategory; } )
			)
		);
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget>
SHoudiniToolsPanel::ConstructHoudiniToolsActionMenu()
{
	FMenuBuilder MenuBuilder( true, NULL );

	// Section - Options
	
	MenuBuilder.BeginSection("PackageCreate", LOCTEXT("ActionMenu_Section_Options", "Options"));
	
	// Options - Auto Refresh
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_AutoRefresh", "Auto Refresh"),
		LOCTEXT("ActionMenu_AutoRefreshTooltip", "Automatically refresh this panel when asset changes have been detected."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				bAutoRefresh = !bAutoRefresh;
				SaveConfig();
				if (bAutoRefresh)
				{
					RefreshPanel();
				}
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return bAutoRefresh ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
	
	// Options - Show Hidden Tools
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_ShowHiddenTools", "Show Hidden Tools"),
		LOCTEXT("ActionMenu_ShowHiddenToolsTooltip", "Show hidden tools (ignoring any exclusion patterns)."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				bShowHiddenTools = !bShowHiddenTools;
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return bShowHiddenTools ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	// Options - Show Empty Categories
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_ShowEmptyCategories", "Show Empty Categories"),
		LOCTEXT("ActionMenu_ShowEmptyCategoriesTooltip", "Show empty categories."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				bShowEmptyCategories = !bShowEmptyCategories;
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return bShowEmptyCategories ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	// Options - Show Houdini Assets
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_ShowHoudiniAssets", "Show Houdini Assets"),
		LOCTEXT("ActionMenu_ShowHoudiniAssetsTooltip", "Show Houdini Assets."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				bShowHoudiniAssets = !bShowHoudiniAssets;
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return bShowHoudiniAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	// Options - Show Houdini Assets
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_ShowPresets", "Show Presets"),
		LOCTEXT("ActionMenu_ShowPresetsTooltip", "Show Presets."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				bShowPresets = !bShowPresets;
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return bShowPresets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	// Options - Tile View
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_TileView", "Tile View"),
		LOCTEXT("ActionMenu_TileViewTooltip", "Show Houdini Tools in a compact tile view."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				ViewMode = EHoudiniToolsViewMode::TileView;
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return ViewMode == EHoudiniToolsViewMode::TileView ? ECheckBoxState::Checked : ECheckBoxState::Unchecked ; } )
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	// Options - List View

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ActionMenu_ListView", "List View"),
		LOCTEXT("ActionMenu_ListViewTooltip", "Show Houdini Tools in a list view."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() -> void
			{
				ViewMode = EHoudiniToolsViewMode::ListView;
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() -> ECheckBoxState { return ViewMode == EHoudiniToolsViewMode::ListView ? ECheckBoxState::Checked : ECheckBoxState::Unchecked ; } )
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.EndSection();

	// Section - Create
	
	MenuBuilder.BeginSection("PackageCreate", LOCTEXT("ActionMenu_Section_Create", "Create"));
	// New HTools Package
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ActionMenu_NewPackage", "Create HoudiniTools package" ),
		LOCTEXT( "ActionMenu_NewPackageTooltip", "Create an empty HoudiniTools package in the Unreal project." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::CreateEmptyToolsPackage )
		)
	);

	// Import HTools Package
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ActionMenu_ImportPackage", "Import HoudiniTools package" ),
		LOCTEXT( "ActionMenu_ImportPackageTooltip", "Import an external HTools package into Unreal." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::ImportToolsPackage )
		)
	);

	// Import HTools Package
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ActionMenu_ImportSideFXTools", "Import SideFX Tools" ),
		LOCTEXT( "ActionMenu_ImportSideFXToolsTooltip", "Import default SideFX tools from Houdini." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::HandleImportSideFXTools )
		)
	);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("PackageUpdate", LOCTEXT("ActionMenu_Section_Update", "Update"));

	// Rescan project and refresh tool panels.
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ActionMenu_RefreshPanel", "Refresh Panel" ),
		LOCTEXT( "ActionMenu_RefreshPanel_Description", "Rescan the Unreal project for Houdini Tools packages." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &SHoudiniToolsPanel::RefreshPanel )
		)
	);

	// Add HoudiniTools actions
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("PackageUpdate", LOCTEXT("ActionMenu_Section_Modify", "Modify"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ActionMenu_EditUserCategories", "Edit User Categories" ),
		LOCTEXT( "ActionMenu_EditUserCategories_Description", "Edit User Categories in the Houdini Engine editor settings." ),
		FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
		FUIAction(
			FExecuteAction::CreateLambda([](){ FHoudiniEngineCommands::ShowPluginEditorSettings(); })
		)
	);
	
	// Add HoudiniTools actions
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget>
SHoudiniToolsPanel::ConstructCategoryFilterMenu()
{
	FMenuBuilder MenuBuilder( true, NULL );
	
	MenuBuilder.AddMenuEntry(
		FText::FromString("Show All"),
		FText::FromString("Show all categories."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([&]() -> void
			{
				bFilterShowAll = !bFilterShowAll;
				if (bFilterShowAll)
				{
					FilterHiddenCategories.Empty();
				}
				else
				{
					// Set all categories as hidden.
					ForEachCategory([&](SHoudiniToolCategory* Category) -> bool
					{
						if (Category)
						{
							FilterHiddenCategories.Add(Category->GetCategoryLabel());
						}
						return true;
					});
				}
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([&]() -> ECheckBoxState { return bFilterShowAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } )
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.BeginSection("Categories", LOCTEXT("FilterMenu_Section_Categories", "Categories"));


	// Add all the categories to the menu 
	
	TArray<FString> CategoryList;

	ForEachCategory([&CategoryList](SHoudiniToolCategory* Category) -> bool
	{
		if (Category)
		{
			CategoryList.Add(Category->GetCategoryLabel());
		}
		return true;
	});
	CategoryList.Sort();

	for (const FString& CategoryName : CategoryList)
	{
		MenuBuilder.AddMenuEntry(
		FText::FromString(CategoryName),
		FText::FromString( FString::Format(TEXT("Show/Hide {0}."), {CategoryName}) ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([&, CategoryName]() -> void
			{
				// Toggle the current category name
				if (FilterHiddenCategories.Contains(CategoryName))
				{
					FilterHiddenCategories.Remove(CategoryName);
				}
				else
				{
					FilterHiddenCategories.Add(CategoryName);
				}
				SaveConfig();
				RequestPanelRefresh();
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([&, CategoryName]() -> ECheckBoxState
			{
				return FilterHiddenCategories.Contains(CategoryName) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
			} )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}

	return MenuBuilder.MakeWidget();
}

void
SHoudiniToolsPanel::EditActiveHoudiniTool() const
{
	if ( !ActiveTool.IsValid() )
		return;

	if ( !IsActiveHoudiniToolEditable() )
		return;

	FHoudiniToolsEditor::LaunchHoudiniToolPropertyEditor(ActiveTool);

	// const UHoudiniAsset* HoudiniAsset = ActiveTool->HoudiniAsset.LoadSynchronous();
	// if (!IsValid(HoudiniAsset))
	// 	return;
	//
	// const UHoudiniToolData* CachedToolData = HoudiniAsset->HoudiniToolData;
	// const bool bValidToolData = IsValid(CachedToolData); 
	//
	// // Create a new Tool property object for the property dialog
	// FString ToolName = ActiveTool->Name.ToString();
	// if ( ActiveTool->HoudiniAsset )
	// 	ToolName += TEXT(" (") + ActiveTool->HoudiniAsset->AssetFileName + TEXT(")");
	//
	// UHoudiniToolEditorProperties* NewToolProperty = NewObject< UHoudiniToolEditorProperties >( GetTransientPackage(), FName( *ToolName ) );
	// NewToolProperty->AddToRoot();
	//
	// // Set the default values for this asset
	// NewToolProperty->Name = ActiveTool->Name.ToString();
	// NewToolProperty->Type = ActiveTool->Type;
	// NewToolProperty->ToolTip = ActiveTool->ToolTipText.ToString();
	// NewToolProperty->HelpURL = ActiveTool->HelpURL;
	// NewToolProperty->SelectionType = ActiveTool->SelectionType;
	// // Always leave this field blank. The user can use this to import a new icon from an arbitrary location. 
	// NewToolProperty->IconPath.FilePath = FString();
	// NewToolProperty->ToolType = ActiveTool->PackageToolType;
	//
	// TArray<UObject *> ActiveHoudiniTools;
	// ActiveHoudiniTools.Add( NewToolProperty );
	//
	//
	// TSharedPtr<FHoudiniTool> EditingTool = ActiveTool;
	//
	// // Create a new property editor window
	// TSharedRef< SWindow > Window = CreateFloatingDetailsView(
	// 	ActiveHoudiniTools,
	// 	FVector2D(450,350),
	// 	[&,EditingTool](TArray<UObject*> InObjects)
	// 	{
	// 		HandleEditHoudiniToolSavedClicked(EditingTool, InObjects);
	// 	}
	// );
}

void
SHoudiniToolsPanel::BrowseToActiveToolAsset() const
{
	if (!ActiveTool.IsValid())
		return;

	if (GEditor)
	{
		FHoudiniToolsEditor::BrowseToObjectInContentBrowser(ActiveTool->GetAssetObject());
	}
}

void
SHoudiniToolsPanel::BrowseToSourceAsset() const
{
	if (!ActiveTool.IsValid())
		return;

	if (GEditor)
	{
		UHoudiniAsset* SourceAsset = ActiveTool->HoudiniAsset.LoadSynchronous();
		FHoudiniToolsEditor::BrowseToObjectInContentBrowser(SourceAsset);
	}
}

void
SHoudiniToolsPanel::BrowseToActiveToolPackage() const
{
	if (!ActiveTool.IsValid())
		return;

	if (GEditor)
	{
		const TSoftObjectPtr<UHoudiniAsset> HoudiniAsset = ActiveTool->HoudiniAsset;
		FHoudiniToolsEditor::BrowseToOwningPackageInContentBrowser(HoudiniAsset.LoadSynchronous());
	}
}

// void
// SHoudiniToolsPanel::HandleEditHoudiniToolSavedClicked(TSharedPtr<FHoudiniTool> HoudiniTool, TArray<UObject *>& InObjects )
// {
// 	// Sanity check, we can only edit one tool at a time!
// 	if ( InObjects.Num() != 1 )
// 		return;
//
// 	if (!HoudiniTool.IsValid())
// 		return;
// 	
// 	UHoudiniAsset* HoudiniAsset = HoudiniTool->HoudiniAsset.LoadSynchronous();
// 	if (!HoudiniAsset)
// 	{
// 		HOUDINI_LOG_WARNING(TEXT("Could not locate active tool. Unable to save changes."));
// 		return;
// 	}
//
// 	// Reimport assets from their new sources.
// 	TArray<UHoudiniAsset*> ReimportAssets;
//
// 	TArray< FHoudiniTool > EditedToolArray;
// 	for ( int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ObjIdx++ )
// 	{
// 		UHoudiniToolProperties* ToolProperties = Cast< UHoudiniToolProperties >( InObjects[ ObjIdx ] );
// 		if ( !ToolProperties )
// 			continue;
//
// 		// FString IconPath = FPaths::ConvertRelativePathToFull( ToolProperties->IconPath.FilePath );
// 		// const FSlateBrush* CustomIconBrush = nullptr;
// 		// if ( FPaths::FileExists( IconPath ) )
// 		// {
// 		//
// 		//     // If we have a valid icon path, load the file. 
// 		//     FName BrushName = *IconPath;
// 		//     CustomIconBrush = new FSlateDynamicImageBrush( BrushName, FVector2D( 40.f, 40.f ) );
// 		// }
// 		
// 		//  - Store all the edited properties on the HoudiniToolData.
// 		//  - Populate the relevant FHoudiniTool descriptor from the HoudiniAsset.
// 		//  - Save out the JSON description, if required.
// 		
// 		bool bModified = false;
//
// 		// Helper macro for Property assignments and modify flag management
// 		#define ASSIGNFN(Src, Dst) \
// 		{\
// 			if (Src != Dst)\
// 			{\
// 				bModified = true;\
// 				Dst = Src;\
// 			}\
// 		}
//
// 		UHoudiniToolData* ToolData = FHoudiniToolsEditor::GetOrCreateHoudiniToolData(HoudiniAsset);
// 		ToolData->Modify();
// 		ASSIGNFN(ToolProperties->Name, ToolData->Name);
// 		ASSIGNFN(ToolProperties->Type, ToolData->Type);
// 		ASSIGNFN(ToolProperties->SelectionType, ToolData->SelectionType);
// 		ASSIGNFN(ToolProperties->ToolTip, ToolData->ToolTip);
// 		ASSIGNFN(ToolProperties->HelpURL, ToolData->HelpURL);
//
// 		if (FPaths::FileExists(ToolProperties->AssetPath.FilePath))
// 		{
// 			// If the AssetPath has changed, reimport the HDA with the new source
// 			if (ToolProperties->AssetPath.FilePath != ToolData->SourceAssetPath.FilePath)
// 			{
// 				TArray<FString> Filenames;
// 				Filenames.Add(ToolProperties->AssetPath.FilePath);
// 				FReimportManager::Instance()->UpdateReimportPaths(HoudiniAsset, Filenames);
// 				
// 				ReimportAssets.Add(HoudiniAsset);
// 			}
// 			ASSIGNFN(ToolProperties->AssetPath.FilePath, ToolData->SourceAssetPath.FilePath);
// 		}
// 		else
// 		{
// 			HOUDINI_LOG_WARNING(TEXT("The specified AssetPath does not exist. Source Asset Path will remain unchanged"));
// 		}
//
// 		
//
// 		bool bModifiedIcon = false;
//
// 		if (ToolProperties->IconPath.FilePath.Len() > 0)
// 		{
// 			bModified = true;
// 			bModifiedIcon = true;
// 			ToolData->LoadIconFromPath(ToolProperties->IconPath.FilePath);
// 		}
// 		else if (ToolProperties->bClearCachedIcon)
// 		{
// 			bModified = true;
// 			bModifiedIcon = true;
// 			ToolData->ClearCachedIcon();
// 		}
//
// 		if (bModifiedIcon)
// 		{
// 			// Ensure the content browser reflects icon changes.
// 			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
// 			AssetRegistryModule.Get().OnAssetUpdated().Broadcast(FAssetData(HoudiniAsset));
// 		}
// 		
// 		ToolData->DefaultTool = false;
// 		
// 		if (bModified)
// 		{
// 			ToolData->MarkPackageDirty();
// 			
// 			// We modified an existing tool. Request a panel refresh.
// 			RequestPanelRefresh();
// 		}
//
// 		// Remove the tool from Root
// 		ToolProperties->RemoveFromRoot();
// 	}
//
// 	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
// 	
//
// 	for (UHoudiniAsset* Asset : ReimportAssets)
// 	{
// 		FReimportManager::Instance()->Reimport(Asset, false /* Ask for new file */, true /* Show notification */);
// 	}
// }

// TSharedRef<SWindow>
// SHoudiniToolsPanel::CreateFloatingDetailsView(
// 	TArray<UObject*>& InObjects,
// 	const FVector2D InClientSize,
// 	const TFunction<void(TArray<UObject*>)> OnSaveClickedFn
// )
// {
// 	TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
// 		.Title(NSLOCTEXT("PropertyEditor", "WindowTitle", "Houdini Tools Property Editor"))
// 		.ClientSize(InClientSize);
//
// 	// If the main frame exists parent the window to it
// 	TSharedPtr< SWindow > ParentWindow;
// 	if ( FModuleManager::Get().IsModuleLoaded("MainFrame") )
// 	{
// 		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
// 		ParentWindow = MainFrame.GetParentWindow();
// 	}
//
// 	if ( ParentWindow.IsValid() )
// 	{
// 		// Parent the window to the main frame 
// 		FSlateApplication::Get().AddWindowAsNativeChild( NewSlateWindow, ParentWindow.ToSharedRef() );
// 	}
// 	else
// 	{
// 		FSlateApplication::Get().AddWindow( NewSlateWindow );
// 	}
//
// 	FDetailsViewArgs Args;
// 	Args.bHideSelectionTip = true;
// 	Args.bLockable = false;
// 	Args.bAllowMultipleTopLevelObjects = true;
// 	Args.ViewIdentifier = TEXT("Houdini Tools Properties");
// 	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
// 	Args.bShowPropertyMatrixButton = false;
// 	Args.bShowOptions = false;
// 	Args.bShowModifiedPropertiesOption = false;
// 	Args.bShowObjectLabel = false;
//
// 	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
// 	TSharedRef<IDetailsView> DetailView = PropertyEditorModule.CreateDetailView( Args );
// 	DetailView->SetObjects( InObjects );
//
// 	NewSlateWindow->SetContent(
// 		SNew( SBorder )
// 		.BorderImage(_GetBrush(TEXT("PropertyWindow.WindowBorder")))
// 		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
// 		[
// 			SNew(SVerticalBox)
//
// 			// Detail View
// 			+ SVerticalBox::Slot()
// 			.AutoHeight()
// 			[
// 				DetailView
// 			]
//
// 			// Button Row
// 			+ SVerticalBox::Slot()
// 			.VAlign(VAlign_Bottom)
// 			.HAlign(HAlign_Right)
// 			.FillHeight(1.f)
// 			[
// 				SNew(SUniformGridPanel)
// 				.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
// 				.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
// 				.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))
//
// 				// Save Button
// 				+ SUniformGridPanel::Slot(0, 0)
// 				[
// 					SNew(SButton)
// 					.HAlign(HAlign_Center)
// 					.VAlign(VAlign_Center)
// 					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
// 					.ButtonStyle(&_GetEditorStyle().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
// 					.Content()
// 					[
// 						SNew(STextBlock)
// 						.TextStyle( &_GetEditorStyle().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText") )
// 						.Text( LOCTEXT("HoudiniTools_Details_Save","Save") )
// 					]
// 					.OnClicked_Lambda([NewSlateWindow, OnSaveClickedFn, InObjects]() -> FReply
// 					{
// 						OnSaveClickedFn(InObjects);
// 						NewSlateWindow->RequestDestroyWindow();
// 						return FReply::Handled();
// 					})
// 				]
//
// 				// Cancel Button
// 				+ SUniformGridPanel::Slot(1, 0)
// 				[
// 					SNew(SButton)
// 					.HAlign(HAlign_Center)
// 					.VAlign(VAlign_Center)
// 					.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
// 					.ButtonStyle(&_GetEditorStyle().GetWidgetStyle<FButtonStyle>("Button"))
// 					.Content()
// 					[
// 						SNew(STextBlock)
// 						.TextStyle( &_GetEditorStyle().GetWidgetStyle<FTextBlockStyle>("ButtonText") )
// 						.Text( LOCTEXT("HoudiniTools_Details_Cancel","Cancel") )
// 					]
// 					.OnClicked_Lambda([NewSlateWindow]() -> FReply
// 					{
// 						NewSlateWindow->RequestDestroyWindow();
// 						return FReply::Handled();
// 					})
// 				]
// 			] // Button Row
// 		] // SBorder
// 	);
//
// 	return NewSlateWindow;
// }

TSharedRef<SWindow>
SHoudiniToolsPanel::CreateFloatingWindow(
	const FText& WindowTitle,
	const FVector2D InitialSize
	) const
{
	 TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(InitialSize)
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMaximize(false).SupportsMinimize(false);

	// If the main frame exists parent the window to it
	TSharedPtr< SWindow > ParentWindow;
	if ( FModuleManager::Get().IsModuleLoaded("MainFrame") )
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if ( ParentWindow.IsValid() )
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild( NewSlateWindow, ParentWindow.ToSharedRef() );
	}
	else
	{
		FSlateApplication::Get().AddWindow( NewSlateWindow );
	}

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bLockable = false;
	Args.bAllowMultipleTopLevelObjects = true;
	Args.ViewIdentifier = TEXT("Houdini Tools Properties");
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bShowPropertyMatrixButton = false;
	Args.bShowOptions = false;
	Args.bShowModifiedPropertiesOption = false;
	Args.bShowObjectLabel = false;

	return NewSlateWindow;
}

FReply
SHoudiniToolsPanel::OnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	if ( InKeyEvent.GetKey() == EKeys::Delete )
	{
		HideActiveToolFromCategory();
		return FReply::Handled();
	}
	else if ( InKeyEvent.GetKey() == EKeys::Enter )
	{
		EditActiveHoudiniTool();
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::F5 )
	{
		RefreshPanel();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool SHoudiniToolsPanel::CanShowActiveToolInCategory() const
{
	if ( !ActiveTool.IsValid() )
		return false;

	if (ActiveTool->CategoryType == EHoudiniToolCategoryType::Package)
	{
		return GetHoudiniTools().CanRemoveToolExcludeFromPackageCategory(ActiveTool->GetAssetObject(), ActiveCategoryName);
	}
	else if (ActiveTool->CategoryType == EHoudiniToolCategoryType::User)
	{
		// User categories does not typically contain 'hidden' tools. We simple 'remove' tools from user categories
		// instead of hiding them.
		return false;
		//return GetHoudiniTools().CanRemoveToolExcludeFromPackageCategory(ActiveTool->GetAssetObject(), ActiveCategoryName);
	}

	return false;
}

void
SHoudiniToolsPanel::ShowActiveToolInCategory()
{
	if ( !ActiveTool.IsValid() )
		return;
	
	if (ActiveTool->CategoryType == EHoudiniToolCategoryType::Package)
	{
		GetHoudiniTools().RemoveToolExcludeFromPackageCategory(ActiveTool->GetAssetObject(), ActiveCategoryName);
	}
	else if (ActiveTool->CategoryType == EHoudiniToolCategoryType::User)
	{
		FHoudiniToolsEditor::RemoveToolFromUserCategory(ActiveTool->GetAssetObject(), ActiveCategoryName);
	}
	
	// Manually trigger a rebuild. The ExcludeToolFromCategory will modify the package, but it doesn't trigger an update because
	// the package is not automatically saved.
	RequestPanelRefresh();
}

bool SHoudiniToolsPanel::CanHideActiveToolFromCategory()
{
	if ( !ActiveTool.IsValid() )
		return false;

	// If this tool is not yet excluded from the active category, we can hide it.
	return !ActiveTool->ExcludedFromCategories.Contains(ActiveCategoryName);
}

void
SHoudiniToolsPanel::HideActiveToolFromCategory()
{
	if ( !ActiveTool.IsValid() )
		return;

	if (ActiveTool->CategoryType == EHoudiniToolCategoryType::Package)
	{
		GetHoudiniTools().ExcludeToolFromPackageCategory(ActiveTool->GetAssetObject(), ActiveCategoryName, false);
	}
	else if (ActiveTool->CategoryType == EHoudiniToolCategoryType::User)
	{
		FHoudiniToolsEditor::RemoveToolFromUserCategory(ActiveTool->GetAssetObject(), ActiveCategoryName);
	}
	
	// Manually trigger a rebuild. The ExcludeToolFromCategory will modify the package, but it doesn't trigger an update because
	// the package is not automatically saved.
	RequestPanelRefresh();
}

void
SHoudiniToolsPanel::RefreshPanel()
{
	UpdateHoudiniToolDirectories();
	RebuildCategories();
}

bool
SHoudiniToolsPanel::IsActiveHoudiniToolEditable() const
{
	if ( !ActiveTool.IsValid() )
		return false;

	return true;
}


void
SHoudiniToolsPanel::CreateEmptyToolsPackage()
{
	if (!CreatePackageWindow.IsValid())
	{
		// Create package window is not valid. Create a new one.
		TSharedPtr<SWindow> Window = CreateFloatingWindow(
				NSLOCTEXT("HoudiniToolsTypeActions", "HoudiniTools_Action_CreateEmptyToolsPackage", "Create Package"),
				FVector2D(700,350)
			);

		Window->SetContent(SNew(SHoudiniToolNewPackage));
	}
	else
	{
		TSharedPtr<SWindow> WindowPtr = CreatePackageWindow.Pin();
		WindowPtr->BringToFront();
	}
}

void
SHoudiniToolsPanel::ImportToolsPackage()
{
	TSharedRef< SWindow > Window = CreateFloatingWindow(
		NSLOCTEXT("HoudiniToolsTypeActions", "HoudiniTools_Action_ImportToolsPackage", "Import Package"),
		FVector2D(700,350)
		);

	// TODO
	Window->SetContent(SNew(SHoudiniToolImportPackage));

	// TArray<FString> HoudiniToolsDirs;
	// GetHoudiniTools().FindAllProjectHoudiniToolsPackages(HoudiniToolsDirs);
}

void
SHoudiniToolsPanel::HandleImportSideFXTools()
{
	int NumImportedHDAs = 0;
	const bool bResult = FHoudiniToolsEditor::ImportSideFXTools(&NumImportedHDAs);
	
	if (bResult)
	{
		const FText Title = LOCTEXT("ImportSideFXTools_ImportSuccess_Title", "Import Success");
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("ImportSideFXTools_ImportSuccess_Title", "Imported {0} HDAs."),
			FText::AsNumber(NumImportedHDAs)),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			Title
#else
			&Title
#endif
			);
	}
	else
	{
		const FText Title = LOCTEXT("ImportSideFXTools_ImportFailed_Title", "Import Failed");
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format( LOCTEXT("ImportSideFXTools_ImportFailed", "Could not import SideFX Tools package."),
			FText::AsNumber(NumImportedHDAs)),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			Title
#else
			&Title
#endif
			);
	}
}

void
SHoudiniToolsPanel::HandleAddToolToUserCategory() const
{
	TSharedRef< SWindow > Window = CreateFloatingWindow(
		NSLOCTEXT("HoudiniTools", "HoudiniTools_AddToUserCategory_Title", "Add to user category"),
		FVector2D(700,350)
		);
	
	Window->SetContent(
		SNew(SHoudiniToolAddToUserCategory)
		.ActiveTool(ActiveTool)
		);
}


void
SHoudiniToolsPanel::HandleAddToFavorites() const
{
	FHoudiniToolsEditor::AddToolToUserCategory(ActiveTool->GetAssetObject(), FHoudiniToolsEditor::GetFavoritesCategoryName().ToString());
}


void
SHoudiniToolsPanel::HandleRemoveFromFavorites() const
{
	FHoudiniToolsEditor::RemoveToolFromUserCategory(ActiveTool->GetAssetObject(), FHoudiniToolsEditor::GetFavoritesCategoryName().ToString());
}


void
SHoudiniToolsPanel::HandleEditUserCategory(const FString& CategoryName) const
{
	FHoudiniEngineCommands::ShowPluginEditorSettings();
}


void
SHoudiniToolsPanel::HandleEditPackageCategory(const FString& CategoryName) const
{
	TSoftObjectPtr<UHoudiniToolsPackageAsset> PackagePtr = GetHoudiniTools().FindFirstPackageContainingCategory(CategoryName);
	if (!PackagePtr.IsValid())
	{
		return;
	}

	if (!GEditor)
	{
		return;
	}
	GEditor->EditObject(PackagePtr.LoadSynchronous());
}


void
SHoudiniToolsPanel::HandleBrowseToCategoryPackage(const FString& CategoryName) const
{
	TSoftObjectPtr<UHoudiniToolsPackageAsset> PackagePtr = GetHoudiniTools().FindFirstPackageContainingCategory(CategoryName);
	FHoudiniToolsEditor::BrowseToObjectInContentBrowser(PackagePtr.LoadSynchronous());
}


void
SHoudiniToolsPanel::HandleSaveToolsInCategory(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType) const
{
	const TMap<FHoudiniToolCategory, TSharedPtr<FHoudiniToolList>>& ToolsMap = GetHoudiniTools().GetCategorizedTools();
	FHoudiniToolCategory Category(CategoryName, CategoryType);
	if (!ToolsMap.Contains(Category))
	{
		return;
	}
	TSharedPtr<FHoudiniToolList> ToolsList = ToolsMap.FindChecked(Category);
	if (!ToolsList.IsValid())
	{
		return;
	}

	// Collect all the package to save
	TArray<UPackage*> PackagesToSave;
	for (TSharedPtr<FHoudiniTool> Tool : ToolsList->Tools)
	{
		if (!Tool.IsValid())
		{
			continue;
		}
		UHoudiniAsset* Asset = Tool->HoudiniAsset.LoadSynchronous();
		UHoudiniPreset* Preset = Tool->HoudiniPreset.LoadSynchronous();
		if (IsValid(Asset) && IsValid(Asset->GetPackage()))
		{
			PackagesToSave.Add( Asset->GetPackage() );
		}
		if (IsValid(Preset) && IsValid(Preset->GetPackage()))
		{
			PackagesToSave.Add( Preset->GetPackage() );
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
}


void
SHoudiniToolsPanel::HandleImportToolsIntoCategory(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType) const
{
	
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TSoftObjectPtr<UHoudiniToolsPackageAsset> PackagePtr = GetHoudiniTools().FindFirstPackageContainingCategory(CategoryName);
	UHoudiniToolsPackageAsset* PackageAsset = PackagePtr.LoadSynchronous();
	if (!IsValid(PackageAsset))
	{
		HOUDINI_LOG_ERROR(TEXT("Category '%s' does not have a valid package"), *CategoryName);
		return;
	}
	const FString Path = FPaths::GetPath( PackageAsset->GetPathName() );
	const TArray<UObject*> ImportedObjects = AssetToolsModule.Get().ImportAssetsWithDialog(Path);
	if (ImportedObjects.Num() > 0)
	{
		FHoudiniToolsEditor::BrowseToObjectInContentBrowser(ImportedObjects[0]);
	}
}


void
SHoudiniToolsPanel::UpdateHoudiniToolDirectories()
{
	FHoudiniToolsEditor& HoudiniTools = FHoudiniToolsPanelUtils::GetHoudiniTools();
	// Refresh the Editor's Houdini Tool list
	// Retrieve all tools, including hidden ones. The categories will filter out hidden tools if needed.
	HoudiniTools.UpdateHoudiniToolListFromProject(true);
}


void
SHoudiniToolsPanel::RequestPanelRefresh()
{
	bRefreshPanelRequested = true;
}

void
SHoudiniToolsPanel::RebuildCategories()
{
	// Rebuild categories container
	CategoriesContainer->ClearChildren();
	
	const TMap<FHoudiniToolCategory, TSharedPtr<FHoudiniToolList>>& CategoriesToolsMap = GetHoudiniTools().GetCategorizedTools();
	TArray<FHoudiniToolCategory> SortedKeys;
	CategoriesToolsMap.GetKeys(SortedKeys);
	SortedKeys.Sort();

	// Store all the category names for filtering.
	FilterCategoryList.Empty();
	
	for (const FHoudiniToolCategory& Category : SortedKeys)
	{
		FilterCategoryList.Add(Category.Name);
		FString CategoryName = Category.Name;
		EHoudiniToolCategoryType CategoryType = Category.CategoryType;
		
		// Add Category view widget
		TSharedPtr<FHoudiniToolList> CategorizedTools = CategoriesToolsMap.FindChecked(Category);
		CategoriesContainer->AddSlot()
		.Padding(0,0,0,5)
		.AutoHeight()
		[
			SNew(SHoudiniToolCategory)
			.CategoryLabel(FText::FromString(Category.Name))
			.CategoryType(Category.CategoryType)
			.ShowEmptyCategory_Lambda( [this]() -> bool { return bShowEmptyCategories; } )
			.ShowHoudiniAssets_Lambda( [this]() -> bool { return bShowHoudiniAssets; } )
			.ShowPresets_Lambda( [this]() -> bool { return bShowPresets; } )
			.HoudiniToolsItemSource(CategorizedTools)
			.ViewMode(ViewMode)
			.OnGenerateTile_Lambda( [this, CategoryName](TSharedPtr< FHoudiniTool > HoudiniTool, const TSharedRef< STableViewBase >& OwnerTable)
				{
					return MakeTileViewWidget(HoudiniTool, CategoryName, OwnerTable);
				} )
			.OnGenerateRow_Lambda([this, CategoryName](TSharedPtr< FHoudiniTool > HoudiniTool, const TSharedRef< STableViewBase >& OwnerTable)
				{
					return MakeListViewWidget(HoudiniTool, CategoryName, OwnerTable);
				} )
			.OnToolSelectionChanged( this, &SHoudiniToolsPanel::OnToolSelectionChanged )
			.OnMouseButtonDoubleClick( this, &SHoudiniToolsPanel::OnDoubleClickedListViewWidget )
			.OnToolContextMenuOpening_Lambda([this, CategoryName, CategoryType]()
			{
				return ConstructHoudiniToolContextMenu();
			})
			.OnCategoryContextMenuOpening_Lambda([this, CategoryName, CategoryType]()
			{
				return ConstructCategoryContextMenu(CategoryName, CategoryType);
			})
			.IsVisible_Lambda([&, Category](){ return !FilterHiddenCategories.Contains(Category.Name); })
			.ShowHiddenTools_Lambda([this]() -> bool { return bShowHiddenTools; })
		];
	}
}

void
SHoudiniToolsPanel::HandleToolChanged()
{
	// An external tool change was detected.
	// Rebuild the categories.
	RequestPanelRefresh();
}


void
SHoudiniToolsPanel::ForEachCategory(const TFunctionRef<bool(SHoudiniToolCategory*)>& ForEachCategoryFunc) const
{
	// Clear selections on any other categories.
	FChildren* ChildWidgets = CategoriesContainer->GetChildren();
	const int32 ChildCount = ChildWidgets->Num();
	for (int32 i = 0; i < ChildCount; i++)
	{
		TSharedPtr<SWidget> Widget = ChildWidgets->GetChildAt(i);
		if (Widget.IsValid() && Widget->GetType().Compare(SHoudiniToolCategory::TypeName)==0)
		{
			SHoudiniToolCategory* ChildCategory = static_cast<SHoudiniToolCategory*>( Widget.Get() );
			if (!ForEachCategoryFunc(ChildCategory))
				break; // Stop iterating if the Lambda returned false
		}
	}
}

void
SHoudiniToolsPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bRefreshPanelRequested)
	{
		// Update tools list and refresh categories view.
		RefreshPanel();
		bRefreshPanelRequested = false;
	}
	
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void
SHoudiniToolsPanel::LoadConfig()
{
	check(GConfig);
	const FString SettingsString = TEXT("HoudiniTools");

	// Show Empty Categories
	if (!GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowEmptyCategories")), bShowEmptyCategories, GEditorPerProjectIni))
	{
		bShowEmptyCategories = true;
	}
	
	// Show Hidden tools
	if (!GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowHiddenTools")), bShowHiddenTools, GEditorPerProjectIni))
	{
		bShowHiddenTools = false;
	}

	// Show Houdini Assets
	if (!GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowHoudiniAssets")), bShowHoudiniAssets, GEditorPerProjectIni))
	{
		bShowHoudiniAssets = true;
	}

	// Show Presets
	if (!GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowPresets")), bShowPresets, GEditorPerProjectIni))
	{
		bShowPresets = true;
	}

	// Filter - Show All    
	if (!GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".Filter.ShowAll")), bFilterShowAll, GEditorPerProjectIni))
	{
		bFilterShowAll = true;
	}

	// Filter - Hidden categories
	TArray<FString> HiddenCategories;
	FilterHiddenCategories.Empty();
	if (GConfig->GetArray(*SettingsIniSection, *(SettingsString + TEXT(".Filter.HiddenCategories")), HiddenCategories, GEditorPerProjectIni))
	{
		FilterHiddenCategories.Append(HiddenCategories);
	}
	
	// View Mode
	FString ViewModeName;
	if (!GConfig->GetString(*SettingsIniSection, *(SettingsString + TEXT(".ViewMode")), ViewModeName, GEditorPerProjectIni))
	{
		ViewModeName = TEXT("Tile");
	}
	
	if (ViewModeName == TEXT("List"))
	{
		ViewMode = EHoudiniToolsViewMode::ListView; 
	}
	else // default: (ViewModeName == TEXT("Tile"))
	{
		ViewMode = EHoudiniToolsViewMode::TileView;
	}
	
	// Auto Refresh
	if (!GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".AutoRefresh")), bAutoRefresh, GEditorPerProjectIni))
	{
		bAutoRefresh = true;
	}
}

void
SHoudiniToolsPanel::SaveConfig() const
{
	check(GConfig);
	const FString SettingsString = TEXT("HoudiniTools");

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowEmptyCategories")), bShowEmptyCategories, GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowHiddenTools")), bShowHiddenTools, GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowHoudiniAssets")), bShowHoudiniAssets, GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".ShowPresets")), bShowPresets, GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".AutoRefresh")), bAutoRefresh, GEditorPerProjectIni);

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".Filter.ShowAll")), bFilterShowAll, GEditorPerProjectIni);
	GConfig->SetArray(*SettingsIniSection, *(SettingsString + TEXT(".Filter.HiddenCategories")), FilterHiddenCategories.Array(), GEditorPerProjectIni);

	FString ViewModeName;
	switch (ViewMode)
	{
		case EHoudiniToolsViewMode::ListView:
			ViewModeName = TEXT("List");
			break;
		case EHoudiniToolsViewMode::TileView:
			ViewModeName = TEXT("Tile");
			break;
		default:
			ViewModeName = TEXT("Tile");
			break;
	}
	GConfig->SetString(*SettingsIniSection, *(SettingsString + TEXT(".ViewMode")), *ViewModeName, GEditorPerProjectIni);

	GConfig->Flush(false, GEditorPerProjectIni);
}


#undef LOCTEXT_NAMESPACE
