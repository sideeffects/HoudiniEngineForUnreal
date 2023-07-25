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

class SDirectoryPicker;
class IDetailsView;
class ITableRow;
class STableViewBase;
struct FSlateBrush;
enum class ECheckBoxState : uint8;

// Toolbar of the tool panel
class SHoudiniToolsToolbar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS( SHoudiniToolsToolbar ) {}
    //TODO: SLATE_NAMED_SLOT( FArguments, CategoryFilter )
    SLATE_NAMED_SLOT( FArguments, ActionMenu )
    SLATE_EVENT( FOnTextChanged, OnSearchTextChanged )
    SLATE_END_ARGS();

    void Construct( const FArguments& InArgs );

protected:
    
};


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

// A collapsible category containing a Tile view of houdini tools. 
class SHoudiniToolCategory : public SCompoundWidget
{
public:
    SHoudiniToolCategory();
    
    // Typename of this widget
    static FName TypeName;
    
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
    
    
    SLATE_BEGIN_ARGS( SHoudiniToolCategory ) {}
    SLATE_EVENT( FOnGenerateRow, OnGenerateTile )
    SLATE_EVENT( FOnMouseButtonClick, OnMouseButtonClick )
    SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )
    SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )
    SLATE_EVENT( FOnToolSelectionChanged, OnToolSelectionChanged )
    
    SLATE_ATTRIBUTE( FText, CategoryLabel )
    SLATE_ATTRIBUTE( TSharedPtr<FHoudiniToolList>, HoudiniToolsItemSource )
    
    SLATE_END_ARGS();

    void Construct( const FArguments& InArgs );

    // Get the active tool for this category, if any.
    TSharedPtr<FHoudiniTool> GetActiveTool() const { return ActiveTool; }
    FString GetCategoryLabel() const { return CategoryLabel; }

    void SetFilterString(const FString& NewFilterString);
    
    void ClearSelection();
    void RequestRefresh();


protected:

    void UpdateVisibleItems();

    TSharedPtr<class SHoudiniToolTileView> HoudiniToolsTileView;
    FString CategoryLabel;
    FString FilterString;
    
    TSharedPtr<FHoudiniTool> ActiveTool;

    // TODO: Maybe this should become a shared ref?
    // The source entries are intentionally stored as a SharedPtr since the category / tool list
    // may be removed in HoudiniTools but we don't want this widget to crash if that happens.
    TSharedPtr<FHoudiniToolList> SourceEntries;
    TArray< TSharedPtr<FHoudiniTool> > VisibleEntries;
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


// Class describing the various properties for a Houdini Tool
// adding a UClass was necessary to use the PropertyEditor window
UCLASS( EditInlineNew )
class UHoudiniToolProperties : public UObject
{
    GENERATED_BODY()

    public:

        UHoudiniToolProperties();

        /** Name of the tool */
        UPROPERTY( Category = Tool, EditAnywhere )
        FString Name;

        /** Type of the tool */
        UPROPERTY( Category = Tool, EditAnywhere )
        EHoudiniToolType Type;

        /** Selection Type of the tool */
        UPROPERTY( Category = Tool, EditAnywhere )
        EHoudiniToolSelectionType SelectionType;

        /** Tooltip shown on mouse hover */
        UPROPERTY( Category = Tool, EditAnywhere )
        FString ToolTip;

        /** Houdini Asset path **/
        UPROPERTY(Category = Tool, EditAnywhere, meta = (FilePathFilter = "hda"))
        FFilePath AssetPath;

        /** Clicking on help icon will bring up this URL */
        UPROPERTY( Category = Tool, EditAnywhere )
        FString HelpURL;
    
        /** This will ensure that the cached icon for this HDA is removed. Note that if an IconPath has been
         * specified, this option will have no effect.
         */
        UPROPERTY( Category = Tool, EditAnywhere )
        bool bClearCachedIcon;
    
        /** Import a new icon for this HDA. If this field is left blank, we will look for an
         * icon next to the HDA with matching name.
         */
        UPROPERTY( Category = Tool, EditAnywhere, meta = (FilePathFilter = "png") )
        FFilePath IconPath;

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

    // Create the empty tools package
    FReply HandleImportClicked();

    // Cancel this dialog
    FReply HandleCancelClicked();

protected:

    static FHoudiniToolsEditor& GetHoudiniTools() { return FHoudiniEngineEditor::Get().GetHoudiniTools(); }

    TSharedPtr<SDirectoryPicker> DirectoryPicker;
    
    TSharedPtr<SEditableTextBox> PackageNameEditBox;
    TSharedPtr<SEditableTextBox> PackageCategoryEditBox;

    FText GetCategory() const;
    
    void UpdatePackageName(const FString& InName);
    void CloseContainingWindow();

    // Package Directory state
    
    bool bIsValidPackageDirectory;
    FText DirectoryInvalidReason;

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
 * SHoudiniToolImportPackage 
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

    bool IsActiveHoudiniToolEditable();

    FText OnGetSelectedDirText() const;

    // Iterate over all the category widgets until the lambda returns false.
    void ForEachCategory(const TFunctionRef<bool(SHoudiniToolCategory*)>& ForEachCategoryFunc) const;

    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
    static FHoudiniToolsEditor& GetHoudiniTools() { return FHoudiniEngineEditor::Get().GetHoudiniTools(); }

    /** Remove the current tool from the tool list **/
    void HideActiveToolFromCategory();

    void RefreshPanel();

    /** Shows a property window for editing the current Tool directory **/
    FReply OnEditToolDirectories();

    /** Handler for closing the EditToolDirectory window **/
    void OnEditToolDirectoriesWindowClosed(const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects);

    // DEPRECATED
    // /** Generates default JSON files for HDA in the current Tool directory that doesnt have one **/
    // void GenerateMissingJSONFiles();

    // Actions Menu
    
    /** Create an empty tools package in the project **/
    void CreateEmptyToolsPackage();

    /** Import a tools package **/
    void ImportToolsPackage();
    

private:

    /** Make a widget for the list view display */
    TSharedRef<ITableRow> MakeListViewWidget( TSharedPtr<struct FHoudiniTool> BspBuilder, const TSharedRef<STableViewBase>& OwnerTable );

    TSharedRef<ITableRow> MakeTileViewWidget( TSharedPtr<struct FHoudiniTool> BspBuilder, const TSharedRef<STableViewBase>& OwnerTable );

    /** Returns the index of the currently selected directory in the Editor ToolDirectoryArray **/
    bool GetCurrentDirectoryIndex(int32& SelectedIndex) const;

    /** Delegate for when the list view selection changes */
    void OnToolSelectionChanged( SHoudiniToolCategory* Category, const TSharedPtr<FHoudiniTool>& HoudiniTool, ESelectInfo::Type SelectionType );

    /** Begin dragging a list widget */
    FReply OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

    /** Handler for double clicking on a Houdini tool **/
    void OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniTool> ListItem );

    /** Handler for the right click context menu on a Houdini tool **/
    TSharedPtr< SWidget > ConstructHoudiniToolContextMenu();

    /** Construct menu widget for the Actions popup button **/
    TSharedPtr< SWidget > ConstructHoudiniToolsActionMenu();

    /** Shows a Property Window for editing the properties of new HoudiniTools**/
    void EditActiveHoudiniTool();

    /** Shows a Property Window for editing the properties of new HoudiniTools**/
    void BrowseToActiveToolAsset() const;

    /** Focus the content browser on the  **/
    void BrowseToActiveToolPackage() const;

    /** Handler for closing the AddHoudiniTools window**/
    void OnEditActiveHoudiniToolWindowClosed(const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects);

    TSharedRef<SWindow> CreateFloatingDetailsView( const TArray< UObject* >& InObjects );
    TSharedRef<SWindow> CreateFloatingWindow(
        const ::FText& WindowTitle,
        const FVector2D IntialSize=FVector2D(400, 550)) const;
    
    void UpdateHoudiniToolDirectories();

    void RequestPanelRefresh();
    void RebuildCategories();

    void HandleToolChanged();

private:
    // The active create package window is tracked here so that we can reuse the
    // active window if summoned again.
    TWeakPtr<SWindow> CreatePackageWindow;

    TSharedPtr<FHoudiniTool> ActiveTool;
    FString ActiveCategoryName; 

    // DEPRECATED
    TArray< TSharedPtr < FString > > HoudiniToolDirArray;
    // DEPRECATED
    FString CurrentHoudiniToolDir;

    TSharedPtr<SVerticalBox> CategoriesContainer;
    bool bRefreshPanelRequested;

    bool bShowHiddenTools;
    bool bAutoRefresh;

    FString FilterString;

    // Handles to unsubscribe during destruction
    FDelegateHandle AssetMemCreatedHandle;
    FDelegateHandle AssetMemDeletedHandle;
    FDelegateHandle AssetAddedHandle;
    FDelegateHandle AssetRenamedHandle;
    FDelegateHandle AssetUpdatedHandle;
    FDelegateHandle AssetUpdatedOnDiskHandle;
    FDelegateHandle AssetPostImportHandle;
};
