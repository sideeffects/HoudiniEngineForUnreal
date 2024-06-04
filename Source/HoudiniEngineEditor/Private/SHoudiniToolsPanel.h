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

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_HoudiniAsset.h"
#include "HoudiniEngineEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Misc/NotifyHook.h"
#include "HoudiniEngineToolTypes.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/STileView.h"

#include "SHoudiniToolsPanel.generated.h"

class SCheckBox;
class SDirectoryPicker;
class IDetailsView;
class ITableRow;
class STableViewBase;
struct FSlateBrush;
enum class ECheckBoxState : uint8;

// View mode for the HoudiniTools panel
UENUM()
enum class EHoudiniToolsViewMode
{
	TileView,
	ListView
};

// Toolbar of the tool panel
class SHoudiniToolsCategoryFilter : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_TwoParams(FOnCategoryFilterChanged, bool /*ShowAllSource*/, const TSet<FString>& /*HiddenCategories*/);
	DECLARE_DELEGATE_OneParam(FOnShowAllChanged, bool /*ShowAllSource*/);
	DECLARE_DELEGATE_TwoParams(FOnCategoryStateChanged, const FString& /*CategoryName*/, const bool /*bIsEnabled*/);

	SHoudiniToolsCategoryFilter();

	SLATE_BEGIN_ARGS( SHoudiniToolsCategoryFilter )
		: _ShowAll(true)
		, _HiddenCategoriesSource(nullptr)
		, _CategoriesSource(nullptr)
	{}
	
	SLATE_EVENT( FOnShowAllChanged, OnShowAllChanged )
	SLATE_EVENT( FOnCategoryStateChanged, OnCategoryStateChanged )

	SLATE_ATTRIBUTE(bool, ShowAll)
	SLATE_ARGUMENT(const TSet<FString>*, HiddenCategoriesSource)
	SLATE_ARGUMENT(const TArray<FString>*, CategoriesSource)
	
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs );
	
	void SetShowAll(TAttribute<bool> InShowAll);

	bool IsCategoryEnabled(FString CategoryName) const;

private:
	TSharedPtr<SWidget> RebuildWidget();

	TAttribute<bool> ShowAll;
	const TSet<FString>* HiddenCategories;
	const TArray<FString>* Categories;

	FOnShowAllChanged OnShowAllChanged;
	FOnCategoryStateChanged OnCategoryStateChanged;
};


// // Toolbar of the tool panel
// class SHoudiniToolsToolbar : public SCompoundWidget
// {
// public:
//
//     DECLARE_DELEGATE_OneParam(FOn)
//     
//     SLATE_BEGIN_ARGS( SHoudiniToolsToolbar ) {}
//     
//     SLATE_EVENT( FOnTextChanged, OnSearchTextChanged )
//     SLATE_NAMED_SLOT( FArguments, FilterMenu )
//     SLATE_NAMED_SLOT( FArguments, ActionMenu )
//     SLATE_ATTRIBUTE( )
//     SLATE_END_ARGS();
//
//     void Construct( const FArguments& InArgs );
//
// protected:
//     
// };

/** The list view mode of the asset view */
class SHoudiniToolListView : public SListView< TSharedPtr<FHoudiniTool> >
{
	public:
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override
		{
			return FReply::Unhandled();
		}
};

/** The grid/tile view mode of the asset view */
class SHoudiniToolTileView : public STileView< TSharedPtr<FHoudiniTool> >
{
	public:
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override
		{
			return FReply::Unhandled();
		}
};

enum class ECategoryContentType: uint8
{
	Tools = 0,
	NoToolsInCategory = 1,
	NoVisibleToolsInCategory = 2
};

// A collapsible category containing a Tile view of houdini tools.
class SHoudiniToolCategory : public SCompoundWidget
{
public:
	SHoudiniToolCategory();
	
	// Typename of this widget
	static FName TypeName;

	typedef SListView< TSharedPtr<FHoudiniTool> > FBaseViewType;
	typedef TSharedPtr<FHoudiniTool> FItemType;
	typedef TListTypeTraits< FItemType >::NullableType FNullableItemType;
	
	typedef TSlateDelegates< FItemType >::FOnGenerateRow FOnGenerateRow;
	typedef TSlateDelegates< FItemType >::FOnMouseButtonClick FOnMouseButtonClick;
	typedef TSlateDelegates< FItemType >::FOnMouseButtonDoubleClick FOnMouseButtonDoubleClick;

	DECLARE_DELEGATE_ThreeParams( FOnToolSelectionChanged,
		SHoudiniToolCategory* /*CategoryWidget*/,
		const TSharedPtr<FHoudiniTool>& /*HoudiniTool*/,
		ESelectInfo::Type /*SelectInfo*/
		);
	
	
	SLATE_BEGIN_ARGS( SHoudiniToolCategory )
		: _ShowEmptyCategory(true)
		, _ShowHoudiniAssets(true)
		, _ShowPresets(true)
		, _ShowHiddenTools(true)
		, _ViewMode(EHoudiniToolsViewMode::TileView)
		, _IsVisible(true)
	{}
	SLATE_EVENT( FOnGenerateRow, OnGenerateTile )
	SLATE_EVENT( FOnGenerateRow, OnGenerateRow )
	SLATE_EVENT( FOnMouseButtonClick, OnMouseButtonClick )
	SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )
	SLATE_EVENT( FOnContextMenuOpening, OnToolContextMenuOpening )
	SLATE_EVENT( FOnContextMenuOpening, OnCategoryContextMenuOpening )
	SLATE_EVENT( FOnToolSelectionChanged, OnToolSelectionChanged )
	
	SLATE_ATTRIBUTE( FText, CategoryLabel )
	SLATE_ATTRIBUTE( bool, ShowEmptyCategory )
	SLATE_ATTRIBUTE( bool, ShowHoudiniAssets )
	SLATE_ATTRIBUTE( bool, ShowPresets )
	SLATE_ATTRIBUTE( bool, ShowHiddenTools )
	SLATE_ATTRIBUTE( EHoudiniToolCategoryType, CategoryType )
	SLATE_ATTRIBUTE( TSharedPtr<FHoudiniToolList>, HoudiniToolsItemSource )
	SLATE_ATTRIBUTE( EHoudiniToolsViewMode, ViewMode )
	SLATE_ATTRIBUTE( bool, IsVisible )
	
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs );

	// Get the active tool for this category, if any.
	TSharedPtr<FHoudiniTool> GetActiveTool() const { return ActiveTool; }
	FString GetCategoryLabel() const { return CategoryLabel; }
	EHoudiniToolCategoryType GetCategoryType() const { return CategoryType; }

	FHoudiniToolCategory GetHoudiniToolCategory() const { return FHoudiniToolCategory(CategoryLabel, CategoryType); }

	void SetFilterString(const FString& NewFilterString);
	
	void ClearSelection();
	void RequestRefresh();


protected:
	
	void UpdateVisibleItems();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	TSharedPtr<SWidget> HandleContextMenuOpening();

	TSharedPtr<FBaseViewType> HoudiniToolsView;
	
	FString CategoryLabel;
	EHoudiniToolCategoryType CategoryType;
	FString FilterString;

	TAttribute<bool> ShowEmptyCategory;
	TAttribute<bool> ShowHoudiniAssets;
	TAttribute<bool> ShowPresets;
	TAttribute<bool> ShowHiddenTools;
	TAttribute<bool> IsVisible;
	TAttribute<FOnContextMenuOpening> ToolContextMenuOpening;
	TAttribute<FOnContextMenuOpening> CategoryContextMenuOpening;
	
	TSharedPtr<FHoudiniTool> ActiveTool;

	// TODO: Maybe this should become a shared ref?
	// The source entries are intentionally stored as a SharedPtr since the category / tool list
	// may be removed in HoudiniTools but we don't want this widget to crash if that happens.
	TSharedPtr<FHoudiniToolList> SourceEntries;
	TArray< TSharedPtr<FHoudiniTool> > VisibleEntries;

	// Use ECategoryContentType to set this switcher's index.
	TSharedPtr<SWidgetSwitcher> CategoryContentSwitcher;
	float TextWrapWidth;
};


// Class describing a Houdini Tool directory
// adding a UClass was necessary to use the PropertyEditor window
UCLASS(EditInlineNew)
class UHoudiniToolDirectoryProperties : public UObject
{
	GENERATED_UCLASS_BODY()

	public:

		/** Custom Houdini Tool Directories **/
		UPROPERTY(EditAnywhere, Category = CustomHoudiniTools)
		TArray<FHoudiniToolDirectory> CustomHoudiniToolsDirectories;
};


// Houdini Tool Helper operations.
// These operations should be callable from the UI as well as no-UI code. We don't want to
// end up in a situation where we are only able to perform these operations from the frontend.
UCLASS()
class UHoudiniToolHelpers : public UObject
{
	GENERATED_BODY()
	
public:
	/** Create a new empty package */
	static bool CreateEmptyPackage(const FName& PackageName);

	/** Import a package from disk */
	static bool ImportPackage(const FName& PackageName, const FString& PackagePath);
};  


/**
 * SHoudiniToolNewPackage 
 */

class SHoudiniToolNewPackage : public SCompoundWidget, public FNotifyHook
{
public:
	DECLARE_DELEGATE_OneParam(FOnCreated, const FString&);
	DECLARE_DELEGATE(FOnCanceled);
	
	SLATE_BEGIN_ARGS( SHoudiniToolNewPackage ) {}
	SLATE_EVENT( FOnCreated, OnCreated )
	SLATE_EVENT( FOnCanceled, OnCanceled )
	SLATE_END_ARGS();

	/**
	 * Default constructor.
	 */
	SHoudiniToolNewPackage();

	void Construct( const FArguments& InArgs );
	
	void OnPackageNameTextChanged(const FText& Text);
	void OnPackageNameTextCommitted(const FText& Text, ETextCommit::Type Arg);
	FText OnGetPackagePathText() const;
	
	void OnPackageCategoryTextChanged(const FText& Text);
	void OnPackageCategoryTextCommitted(const FText& Text, ETextCommit::Type Arg);

	EVisibility GetNameErrorLabelVisibility() const;
	auto GetNameErrorLabelText() const -> FText;    

	bool IsCreateEnabled() const;

	// Create the empty tools package
	FReply HandleCreateClicked();
	// Cancel this dialog
	FReply HandleCancelClicked();

protected:
	
	TSharedPtr<SEditableTextBox> PackageNameEditBox;
	TSharedPtr<SEditableTextBox> PackageCategoryEditBox;
	
	FText GetCategory() const;
	
	void UpdatePackageName(FText NewName);
	void CloseContainingWindow();
	
	/** The calculated name for the new Houdini Tools package */
	FText CalculatedPackageName;
	FText PackageNameError;
	bool bValidPackageName;
	bool bSyncCategory;

	FOnCreated OnCreated;
	FOnCanceled OnCanceled;
};

/**
 * SHoudiniToolImportPackage 
 */

class SHoudiniToolImportPackage : public SCompoundWidget, public FNotifyHook
{
public:
	DECLARE_DELEGATE_OneParam(FOnImported, const FString&);
	DECLARE_DELEGATE(FOnCanceled);
	
	SLATE_BEGIN_ARGS( SHoudiniToolImportPackage ) {}
	SLATE_EVENT( FOnImported, OnImported )
	SLATE_EVENT( FOnCanceled, OnCanceled )
	SLATE_END_ARGS();

	/**
	 * Default constructor.
	 */
	SHoudiniToolImportPackage();

	void Construct( const FArguments& InArgs );

	// Error label

	EVisibility GetNameErrorLabelVisibility() const;
	auto GetNameErrorLabelText() const -> FText;

	// Package directory

	void OnDirectoryChanged(const FString& Directory);
	
	void OnPackageNameTextChanged(const FText& Text);
	void OnPackageNameTextCommitted(const FText& Text, ETextCommit::Type Arg);
	FText OnGetPackagePathText() const;

	bool IsImportEnabled() const;
	bool IsNextEnabled() const;

	// Create the empty tools package
	FReply HandleImportClicked();
	EVisibility GetImportVisibility() const;

	FReply HandleNextClicked() const;
	EVisibility GetNextVisibility() const;

	FReply HandleCreateClicked();
	bool IsCreateEnabled() const;
	EVisibility GetCreateVisibility() const;
	
	FReply HandleBackClicked() const;
	EVisibility GetBackVisibility() const;

	// Cancel this dialog
	FReply HandleCancelClicked();

protected:

	static FHoudiniToolsEditor& GetHoudiniTools() { return FHoudiniEngineEditor::Get().GetHoudiniTools(); }

	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
	
	TSharedPtr<SDirectoryPicker> DirectoryPicker;
	TSharedPtr<SEditableTextBox> PackageNameEditBox;
	TSharedPtr<SEditableTextBox> PackageCategoryEditBox;

	TSharedPtr<SCheckBox> CreateExternalJSON;


	FText GetCategory() const;
	
	void UpdatePackageName(const FString& InName);
	void CloseContainingWindow();

	// Package Directory state
	bool bIsValidPackageDirectory;
	FText DirectoryInvalidReason;

	// Package JSON state
	bool bHasPackageJSON;

	// Package Name state
	FString NewPackageName;
	FText PackageNameError;
	bool bValidPackageName;

	// If the external directory being imported doesn't
	// have a package description, create a default one.
	bool bCreateMissingPackageJSON;

	FOnImported OnImported;
	FOnCanceled OnCanceled;
};


/**
 * Add Tool to User Category 
 */

class SHoudiniToolAddToUserCategory : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SHoudiniToolAddToUserCategory ) {}

	SLATE_ATTRIBUTE(TSharedPtr<FHoudiniTool>, ActiveTool)
	
	SLATE_END_ARGS();

	SHoudiniToolAddToUserCategory();

	void Construct( const FArguments& InArgs );


protected:

	FText ToolPackagePathText();
	
	// Add To Category Button
	FReply HandleAddToCategoryClicked();
	bool GetAddToCategoryEnabled() const;
	EVisibility GetAddToCategoryVisibility() const;

	// Goto Create New Category button
	FReply HandleGotoCreateCategoryClicked();
	bool GetGotoCreateCategoryEnabled() const;
	EVisibility GetGotoCreateCategoryVisibility() const;

	// Add New Category Button
	FReply HandleAddNewCategoryClicked();
	bool GetCreateCategoryEnabled() const;
	EVisibility GetCreateCategoryVisibility() const;

	// Back Button
	FReply HandleBackClicked();
	EVisibility GetBackButtonVisibility() const;

	// Cancel Button
	FReply HandleCancelClicked();

	// User Category Selection
	void OnUserCategorySelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectType);

	// User Category Creation
	void OnUserCategoryCreateTextChanged(const FText& NewText);
	void OnUserCategoryCreateTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	void CloseContainingWindow();
	
	TSharedPtr<FHoudiniTool> ActiveTool;
	UHoudiniToolsPackageAsset* Package;
	
	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;

	TSharedPtr<SWidget> AddToCategoryView;
	TSharedPtr<SWidget> CreateCategoryView;

	TArray<TSharedPtr<FString>> UserCategories;
	TSharedPtr<FString> SelectedCategory;

	FString UserCategoryToCreate;
};

/**
 * Houdini Tools panel 
 */

class SHoudiniToolsPanel : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SHoudiniToolsPanel ) {}
	SLATE_END_ARGS();

	SHoudiniToolsPanel();
	virtual ~SHoudiniToolsPanel() override;

	void Construct( const FArguments& InArgs );

	static FTransform GetDefaulToolSpawnTransform();
	static FTransform GetMeanWorldSelectionTransform();

	/** Instantiate the selected HoudiniTool and assigns input depending on the current selection and tool type **/
	static void InstantiateHoudiniTool( FHoudiniTool* HoudiniTool );

	/** Handler for Key presses**/
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	bool IsActiveHoudiniToolEditable() const;

	// Iterate over all the category widgets until the lambda returns false.
	void ForEachCategory(const TFunctionRef<bool(SHoudiniToolCategory*)>& ForEachCategoryFunc) const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	static FHoudiniToolsEditor& GetHoudiniTools() { return FHoudiniEngineEditor::Get().GetHoudiniTools(); }

	void LoadConfig();
	void SaveConfig() const;

	bool CanShowActiveToolInCategory() const;
	void ShowActiveToolInCategory();
	
	/** Remove the current tool from the tool list **/
	bool CanHideActiveToolFromCategory();
	void HideActiveToolFromCategory();

	void RefreshPanel();

	// Actions Menu
	
	/** Create an empty tools package in the project **/
	void CreateEmptyToolsPackage();

	/** Import a tools package **/
	void ImportToolsPackage();

	void HandleImportSideFXTools();

	void HandleAddToolToUserCategory() const;

	void HandleAddToFavorites() const;

	void HandleRemoveFromFavorites() const;

	void HandleEditUserCategory(const FString& CategoryName) const;

	void HandleEditPackageCategory(const FString& CategoryName) const;

	void HandleBrowseToCategoryPackage(const FString& CategoryName) const;

	void HandleSaveToolsInCategory(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType) const;

	void HandleImportToolsIntoCategory(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType) const;


	static const FString SettingsIniSection;

	/** Make a widget for the list view display */
	TSharedRef<ITableRow> MakeListViewWidget( TSharedPtr<struct FHoudiniTool> HoudiniTool, const FString& CategoryName, const TSharedRef<STableViewBase>& OwnerTable );

	TSharedRef<ITableRow> MakeTileViewWidget( TSharedPtr<struct FHoudiniTool> HoudiniTool, const FString& CategoryName, const TSharedRef<STableViewBase>& OwnerTable );

	/** Delegate for when the list view selection changes */
	void OnToolSelectionChanged( SHoudiniToolCategory* Category, const TSharedPtr<FHoudiniTool>& HoudiniTool, ESelectInfo::Type SelectionType );

	/** Begin dragging a list widget */
	FReply OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	/** Handler for double clicking on a Houdini tool **/
	void OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniTool> ListItem );

	// /** Return the appropriate context menu, depending on current Tools panel context **/
	// TSharedPtr<SWidget> ConstructContextMenu(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType);

	/** Handler for the right click context menu on a Houdini tool **/
	TSharedPtr<SWidget> ConstructHoudiniToolContextMenu();

	/** Context menu for right clicking on any empty area in a category */
	TSharedPtr<SWidget> ConstructCategoryContextMenu(const FString& CategoryName, const EHoudiniToolCategoryType CategoryType);

	/** Construct menu widget for the Actions popup button **/
	TSharedPtr<SWidget> ConstructHoudiniToolsActionMenu();

	TSharedPtr<SWidget> ConstructCategoryFilterMenu();

	/** Shows a Property Window for editing the properties of new HoudiniTools**/
	void EditActiveHoudiniTool() const;

	/** Focus the content browser on the asset object that relates to the active tool **/
	void BrowseToActiveToolAsset() const;

	/** Focus the content browser on the HoudiniAsset that relates to the active tool **/
	void BrowseToSourceAsset() const;

	/** Focus the content browser on the package that owns the active tool **/
	void BrowseToActiveToolPackage() const;

	/** Handler for Save clicked in the Edit Active Tool dialog. **/
	// void HandleEditHoudiniToolSavedClicked(TSharedPtr<FHoudiniTool> HoudiniTool, TArray<UObject *>& InObjects );
	
	TSharedRef<SWindow> CreateFloatingWindow(
		const ::FText& WindowTitle,
		const FVector2D IntialSize=FVector2D(400, 550)) const;
	
	void UpdateHoudiniToolDirectories();

	void RequestPanelRefresh();
	void RebuildCategories();

	void HandleToolChanged();


	// The active create package window is tracked here so that we can reuse the
	// active window if summoned again.
	TWeakPtr<SWindow> CreatePackageWindow;

	TSharedPtr<FHoudiniTool> ActiveTool;
	FString ActiveCategoryName;

	// Temporary button style for testing. Remove when done.
	FButtonStyle DGB_HelpButtonStyle;

	TSharedPtr<SVerticalBox> CategoriesContainer;
	bool bRefreshPanelRequested;

	// Tools filter string
	FString FilterString;

	// Categories filter menu
	TSharedPtr<SHoudiniToolsCategoryFilter> CategoryFilterWidget;
	// When the user clicks on the ShowAll action, it immediately shows all categories (i.e., removes them
	// from the FilterHiddenCategories array). If the user disables the Show All setting, all categories will immediate
	// be hidden. Individual categories can still be toggled in both cases. 
	bool bFilterShowAll;
	// Any categories that are NOT in this set is considered to be visible.
	TSet<FString> FilterHiddenCategories;
	// List of categories to be displayed by the in the filter.
	TArray<FString> FilterCategoryList;
	
	// Tools Settings
	bool bShowEmptyCategories;
	bool bShowHiddenTools;
	bool bAutoRefresh;
	bool bShowHoudiniAssets;
	bool bShowPresets;

	EHoudiniToolsViewMode ViewMode;

	// Styling

	// Brush used to draw the underline for icons in the HoudiniTools panel.
	FSlateBrush UnderlineBrush;


	// Handles to unsubscribe during destruction
	FDelegateHandle AssetMemCreatedHandle;
	FDelegateHandle AssetMemDeletedHandle;
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle AssetUpdatedHandle;
	FDelegateHandle AssetUpdatedOnDiskHandle;
	FDelegateHandle AssetPostImportHandle;
	FDelegateHandle UserToolCategoriesChangedHandle;
	FDelegateHandle ToolOrPackageChangedHandle;
};
