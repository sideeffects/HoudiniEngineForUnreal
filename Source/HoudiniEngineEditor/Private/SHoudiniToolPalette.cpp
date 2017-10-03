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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniApi.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "Editor.h"
#include "SHoudiniToolPalette.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "HoudiniAssetActor.h"
#include "AssetRegistryModule.h"
#include "AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE HOUDINI_MODULE_EDITOR

/** The list view mode of the asset view */
class SHoudiniToolListView : public SListView<TSharedPtr<FHoudiniToolType>>
{
public:
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent ) override
    {
        return FReply::Unhandled();
    }
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SHoudiniToolPalette::Construct( const FArguments& InArgs )
{
    FHoudiniEngineEditor& HoudiniEngineEditor = FModuleManager::GetModuleChecked<FHoudiniEngineEditor>( "HoudiniEngineEditor" );

    TSharedRef<SHoudiniToolListView> ListViewWidget =
        SNew( SHoudiniToolListView )
        .SelectionMode( ESelectionMode::Single )
        .ListItemsSource( &HoudiniEngineEditor.GetToolTypes() )
        .OnGenerateRow( this, &SHoudiniToolPalette::MakeListViewWidget )
        .OnSelectionChanged( this, &SHoudiniToolPalette::OnSelectionChanged )
        .OnMouseButtonDoubleClick( this, &SHoudiniToolPalette::OnDoubleClickedListViewWidget)
        .ItemHeight( 35 );

    ChildSlot
        [
            SNew( SVerticalBox )

            + SVerticalBox::Slot()
            .FillHeight( 1.0f )
            [
                SNew( SScrollBorder, ListViewWidget )
                [
                    ListViewWidget
                ]
            ]
        ];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<ITableRow> SHoudiniToolPalette::MakeListViewWidget( TSharedPtr<FHoudiniToolType> ToolType, const TSharedRef<STableViewBase>& OwnerTable )
{
    check( ToolType.IsValid() );
 
    auto Style = FHoudiniEngineEditor::Get().GetSlateStyle();

    auto HelpDefault = FEditorStyle::GetBrush( "HelpIcon" );
    auto HelpHovered = FEditorStyle::GetBrush( "HelpIcon.Hovered" );
    auto HelpPressed = FEditorStyle::GetBrush( "HelpIcon.Pressed" );
    TSharedPtr< SImage > HelpButtonImage;
    TSharedPtr< SButton > HelpButton;

    FString HelpURL = ToolType->HelpURL;
    FText Tip = HelpURL.Len() > 0 ? 
        FText::Format( LOCTEXT( "OpenHelp", "Click to view tool help: {0}" ), FText::FromString( HelpURL ) ) : 
        ToolType->Text;

    TSharedRef< STableRow<TSharedPtr<FHoudiniToolType>> > TableRowWidget =
        SNew( STableRow<TSharedPtr<FHoudiniToolType>>, OwnerTable )
        .Style( Style, "HoudiniEngine.TableRow" )
        .OnDragDetected( this, &SHoudiniToolPalette::OnDraggingListViewWidget );

    TSharedRef<SWidget> Content =
        SNew( SBorder )
        .BorderImage( FCoreStyle::Get().GetBrush( "NoBorder" ) )
        .Padding( 0 )
        .ToolTip( SNew( SToolTip ).Text( Tip ) )
        .Cursor( EMouseCursor::GrabHand )
        [
            SNew( SHorizontalBox )

            // Icon
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew( SBorder )
                .Padding( 4.0f )
                .BorderImage( Style->GetBrush( "HoudiniEngine.ThumbnailShadow" ) )
                [
                    SNew( SBox )
                    .WidthOverride( 35.0f )
                    .HeightOverride( 35.0f )
                    [
                        SNew( SBorder )
                        .BorderImage( Style->GetBrush( "HoudiniEngine.ThumbnailBackground" ) )
                        .HAlign( HAlign_Center )
                        .VAlign( VAlign_Center )
                        [
                            SNew( SImage )
                            .Image( ToolType->Icon )
                        ]
                    ]
                ]
            ]

            + SHorizontalBox::Slot()
                .HAlign( HAlign_Left )
                .VAlign( VAlign_Center )
                .FillWidth( 1.f )
                [
                    SNew( STextBlock )
                    .TextStyle( *Style, "HoudiniEngine.ThumbnailText" )
                    .Text( ToolType->Text )
                ]
            + SHorizontalBox::Slot()
                .VAlign( VAlign_Center )
                .AutoWidth()
                [
                    SAssignNew( HelpButton, SButton )
                    .ContentPadding( 0 )
                    .ButtonStyle( FEditorStyle::Get(), "HelpButton" )
                    .OnClicked( FOnClicked::CreateLambda( [HelpURL]() {
                        if ( HelpURL.Len() )
                            FPlatformProcess::LaunchURL( *HelpURL, nullptr, nullptr );
                        return FReply::Handled();
                     } ))
                    .HAlign( HAlign_Center )
                    .VAlign( VAlign_Center )
                    .ToolTip( SNew( SToolTip ).Text( Tip ))
                    [
                        SAssignNew( HelpButtonImage, SImage )
                    ]
                ]
        ];

    HelpButtonImage->SetImage( TAttribute<const FSlateBrush *>::Create( TAttribute<const FSlateBrush *>::FGetter::CreateLambda( [=]() {
        if ( HelpButton->IsPressed() )
        {
            return HelpPressed;
        }

        if ( HelpButtonImage->IsHovered() )
        {
            return HelpHovered;
        }

        return HelpDefault;
    } ) ) );

    TableRowWidget->SetContent( Content );

    return TableRowWidget;
}

void SHoudiniToolPalette::OnSelectionChanged( TSharedPtr<FHoudiniToolType> ToolType, ESelectInfo::Type SelectionType )
{
    if ( ToolType.IsValid() )
    {
        ActiveTool = ToolType;
    }
}

FReply SHoudiniToolPalette::OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
    if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
    {
        if ( ActiveTool.IsValid() )
        {
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
            UObject* AssetObj = ActiveTool->HoudiniAsset.LoadSynchronous();
            if ( AssetObj )
            {
                FAssetData BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetObj->GetPathName());

                return FReply::Handled().BeginDragDrop( FAssetDragDropOp::New( BackingAssetData, GEditor->FindActorFactoryForActorClass( AHoudiniAssetActor::StaticClass() ) ) );
            }
        }
    }

    return FReply::Unhandled();
}

void 
SHoudiniToolPalette::OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniToolType> ToolType )
{
    if ( ToolType.IsValid() )
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
        UObject* AssetObj = ToolType->HoudiniAsset.LoadSynchronous();
        if ( AssetObj )
        {
            UActorFactory* Factory = GEditor->FindActorFactoryForActorClass( AHoudiniAssetActor::StaticClass() );
            Factory->CreateActor( AssetObj, GEditor->GetEditorWorldContext().World()->GetCurrentLevel(), FTransform::Identity );
        }
    }
}
