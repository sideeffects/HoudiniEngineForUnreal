/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once

#include "CoreMinimal.h"
#include "HoudiniEngineEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/NotifyHook.h"
#include "SHoudiniToolPalette.generated.h"

class IDetailsView;
class ITableRow;
class STableViewBase;
struct FSlateBrush;
enum class ECheckBoxState : uint8;

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

// Class describing the various properties for a Houdini Tool
// adding a UClass was necessary to use the PropertyEditor window
UCLASS( EditInlineNew )
class UHoudiniToolProperties : public UObject
{
    GENERATED_UCLASS_BODY()

    public:

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

        /** Path to a custom icon */
        UPROPERTY( Category = Tool, EditAnywhere, meta = (FilePathFilter = "png") )
        FFilePath IconPath;

        /** Houdini uasset */
        UPROPERTY( Category = Tool, EditAnywhere )
        TSoftObjectPtr < class UHoudiniAsset > HoudiniAsset;

        /** Houdini Asset path **/
        UPROPERTY(Category = Tool, EditAnywhere)
        FFilePath AssetPath;

        /** Clicking on help icon will bring up this URL */
        UPROPERTY( Category = Tool, EditAnywhere )
        FString HelpURL;
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

class SHoudiniToolPalette : public SCompoundWidget, public FNotifyHook
{
public:
    SLATE_BEGIN_ARGS( SHoudiniToolPalette ) {}
    SLATE_END_ARGS();

    void Construct( const FArguments& InArgs );

    static FTransform GetDefaulToolSpawnTransform();
    static FTransform GetMeanWorldSelectionTransform();

    /** Instantiate the selected HoudiniTool and assigns input depending on the current selection and tool type **/
    static void InstantiateHoudiniTool( FHoudiniTool* HoudiniTool );

    /** Handler for Key presses**/
    virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

    bool IsActiveHoudiniToolEditable();

    FText OnGetSelectedDirText() const;

protected:

    /** Remove the current tool from the tool list **/
    void RemoveActiveTool();

    FReply OnEditToolDirectories();

    /** Handler for closing the EditToolDirectory window**/
    void OnEditToolDirectoriesWindowClosed(const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects);

private:

    /** Make a widget for the list view display */
    TSharedRef<ITableRow> MakeListViewWidget( TSharedPtr<struct FHoudiniTool> BspBuilder, const TSharedRef<STableViewBase>& OwnerTable );

    /** Delegate for when the list view selection changes */
    void OnSelectionChanged( TSharedPtr<FHoudiniTool> BspBuilder, ESelectInfo::Type SelectionType );

    void OnDirectoryChange(const FString& NewChoice);

    /** Begin dragging a list widget */
    FReply OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

    virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

    /** Handler for double clicking on a Houdini tool **/
    void OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniTool> ListItem );

    /** Handler for the right click context menu on a Houdini tool **/
    TSharedPtr< SWidget > ConstructHoudiniToolContextMenu();

    /** Shows a Property Window for editing the properties of new HoudiniTools**/
    void ShowAddHoudiniToolWindow( const TArray< UHoudiniAsset *>& HoudiniAssets );

    /** Handler for closing the AddHoudiniTools window**/
    void OnAddHoudiniToolWindowClosed(const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects);

    /** Shows a Property Window for editing the properties of new HoudiniTools**/
    void EditActiveHoudiniTool();

    /** Handler for closing the AddHoudiniTools window**/
    void OnEditHoudiniToolWindowClosed(const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects);

    TSharedRef<SWindow> CreateFloatingDetailsView( const TArray< UObject* >& InObjects );

    void UpdateHoudiniToolDirectories();

private:
    TSharedPtr<FHoudiniTool> ActiveTool;

    TArray< TSharedPtr < FString > > HoudiniToolDirArray;

    FString CurrentHoudiniToolDir;

    /** Holds the tools list view. */
    TSharedPtr<SHoudiniToolListView> HoudiniToolListView;
};
