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

#include "HoudiniApi.h"
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
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "AssetRegistryModule.h"
#include "AssetDragDropOp.h"
#include "ActorFactories/ActorFactory.h"
#include "Engine/Selection.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "EditorViewportClient.h"

#define LOCTEXT_NAMESPACE "HoudiniToolPalette"

/** The list view mode of the asset view */
class SHoudiniToolListView : public SListView<TSharedPtr<FHoudiniTool>>
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

TSharedRef<ITableRow> SHoudiniToolPalette::MakeListViewWidget( TSharedPtr<FHoudiniTool> ToolType, const TSharedRef<STableViewBase>& OwnerTable )
{
    check( ToolType.IsValid() );
 
    auto Style = FHoudiniEngineEditor::Get().GetSlateStyle();

    auto HelpDefault = FEditorStyle::GetBrush( "HelpIcon" );
    auto HelpHovered = FEditorStyle::GetBrush( "HelpIcon.Hovered" );
    auto HelpPressed = FEditorStyle::GetBrush( "HelpIcon.Pressed" );
    TSharedPtr< SImage > HelpButtonImage;
    TSharedPtr< SButton > HelpButton;

    FString HelpURL = ToolType->HelpURL;
    FText ToolTip = ToolType->ToolTipText;
    FText HelpTip = HelpURL.Len() > 0 ? 
        FText::Format( LOCTEXT( "OpenHelp", "Click to view tool help: {0}" ), FText::FromString( HelpURL ) ) : 
        ToolType->Name;

    TSharedRef< STableRow<TSharedPtr<FHoudiniTool>> > TableRowWidget =
        SNew( STableRow<TSharedPtr<FHoudiniTool>>, OwnerTable )
        .Style( Style, "HoudiniEngine.TableRow" )
        .OnDragDetected( this, &SHoudiniToolPalette::OnDraggingListViewWidget );

    TSharedRef<SWidget> Content =
        SNew( SBorder )
        .BorderImage( FCoreStyle::Get().GetBrush( "NoBorder" ) )
        .Padding( 0 )
        .ToolTip( SNew( SToolTip ).Text( ToolTip ) )
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
                            .ToolTip( SNew( SToolTip ).Text( ToolTip ) )
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
                    .Text( ToolType->Name )
                    .ToolTip( SNew( SToolTip ).Text( ToolTip ) )
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
                     } ) )
                    .HAlign( HAlign_Center )
                    .VAlign( VAlign_Center )
                    .ToolTip( SNew( SToolTip ).Text( HelpTip ) )
                    [
                        SAssignNew( HelpButtonImage, SImage )
                        .ToolTip( SNew( SToolTip ).Text( HelpTip ) )
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

void SHoudiniToolPalette::OnSelectionChanged( TSharedPtr<FHoudiniTool> ToolType, ESelectInfo::Type SelectionType )
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
SHoudiniToolPalette::OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniTool> ToolType )
{
    if ( !ToolType.IsValid() )
        return;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );

    // Load the asset
    UObject* AssetObj = ToolType->HoudiniAsset.LoadSynchronous();
    if ( !AssetObj )
        return;

    // Get the asset Factory
    UActorFactory* Factory = GEditor->FindActorFactoryForActorClass( AHoudiniAssetActor::StaticClass() );
    if ( !Factory )
        return;

    // Create the asset actor
    AActor* CreatedActor = Factory->CreateActor( AssetObj, GEditor->GetEditorWorldContext().World()->GetCurrentLevel(), GetDefaulToolSpawnTransform() );
    if ( !CreatedActor )
        return;

    // Get the current Level Editor Selection
    TArray<UObject * > WorldSelection;
    int32 WorldSelectionCount = GetWorldSelection( WorldSelection );

    // Get the current Content browser selection
    TArray<UObject *> ContentBrowserSelection;
    int32 ContentBrowserSelectionCount = GetContentBrowserSelection( ContentBrowserSelection );

    // Modify the created actor's position from the current editor world selection
    if ( WorldSelectionCount > 0 )
    {
        // Get the "mean" transform of all the selected actors
        FTransform MeanWorldSelectionTransform = GetMeanWorldSelectionTransform();
        CreatedActor->SetActorTransform( MeanWorldSelectionTransform );
    }

    // Depending on the type of Houdini Tool, the current selection will be handled differently
    switch ( ToolType->Type )
    {
        case ( EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE ):
        {
            // All the selection will be applied to the asset's first input
            // World selection has priority over CB selection
            // World selection => World Input
            // CB selection => Geometry Input
            AHoudiniAssetActor* HoudiniAssetActor = (AHoudiniAssetActor*)CreatedActor;
            if ( !HoudiniAssetActor )
                break;

            UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
            if ( !HoudiniAssetComponent )
                break;

            TMap<UObject*, int32> InputPresets;
            if  ( ContentBrowserSelectionCount > 0 )
            {
                // Build the preset map using the CB selection
                // All selected objects will go to the first input
                for ( auto CurrentObject : ContentBrowserSelection )
                {
                    if ( CurrentObject )
                        InputPresets.Add( CurrentObject, 0 );
                }
            }
            else if ( WorldSelectionCount > 0 )
            {
                // Build the preset map using the world selection
                // All selected actors will go to the first input
                for ( auto CurrentObject : WorldSelection )
                {
                    if ( CurrentObject )
                        InputPresets.Add( CurrentObject, 0 );
                }
            }

            if ( InputPresets.Num() > 0 )
            {
                // set the input preset on the HoudiniAssetComponent
                HoudiniAssetComponent->SetHoudiniToolInputPresets( InputPresets );
            }
        }
        break;

        case ( EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI ):
        {
            // The selection will be applied individually to multiple inputs
            // (first object to first input, second object to second input etc...)
            // World selection has priority over CB selection
            // World selection => World Input
            // CB selection => Geometry Input

            AHoudiniAssetActor* HoudiniAssetActor = (AHoudiniAssetActor*)CreatedActor;
            if ( !HoudiniAssetActor )
                break;

            UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
            if ( !HoudiniAssetComponent )
                break;

            TMap<UObject*, int32> InputPresets;
            if  ( ContentBrowserSelectionCount > 0 )
            {
                // Build the preset map using the CB selection
                // Each selected object will go to a separated input if possible
                int nInput = 0;
                for ( auto CurrentObject : ContentBrowserSelection )
                {
                    if ( CurrentObject )
                        InputPresets.Add( CurrentObject, nInput++ );
                }
            }
            else if ( WorldSelectionCount > 0 )
            {
                // Build the preset map using the world selection
                // Each selected object will go to a separated input if possible
                int nInput = 0;
                for ( auto CurrentObject : WorldSelection )
                {
                    if ( CurrentObject )
                        InputPresets.Add( CurrentObject, nInput++ );
                }
            }

            if ( InputPresets.Num() > 0 )
            {
                // set the input preset on the HoudiniAssetComponent
                HoudiniAssetComponent->SetHoudiniToolInputPresets( InputPresets );		
            }
        }
        break;

        case ( EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH ):
        {
            // The asset will be duplicated and applied multiple times to the current selection
            // One asset => One Input => One selected object
            // World selection has priority over CB selection
            // World selection => World Input
            // CB selection => Geometry Input
        }
        break;

        case( EHoudiniToolType::HTOOLTYPE_GENERATOR ):
        default:
        {
            // The asset just generates geometry
            // CB / World selection are ignored
            // World selection will be used to set the asset transform?	    
        }
        break;
    }

    // Select the Actor we just created
    if ( GEditor->CanSelectActor( CreatedActor, true, true ) )
    {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor( CreatedActor, true, true, true );
    }
}

FTransform
SHoudiniToolPalette::GetDefaulToolSpawnTransform()
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
SHoudiniToolPalette::GetMeanWorldSelectionTransform()
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

            // Accumulate all the actor transforms...
            if ( NumAppliedTransform == 0 )
                SpawnTransform = Actor->GetActorTransform();
            else
                SpawnTransform.Accumulate( Actor->GetActorTransform() );

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

int32
SHoudiniToolPalette::GetContentBrowserSelection(TArray< UObject* >& ContentBrowserSelection )
{
    ContentBrowserSelection.Empty();

    // Get the current Content browser selection
    FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked< FContentBrowserModule >( "ContentBrowser" );
    TArray<FAssetData> SelectedAssets;
    ContentBrowserModule.Get().GetSelectedAssets( SelectedAssets );

    for( int32 n = 0; n < SelectedAssets.Num(); n++ )
    {
        // Get the current object
        UObject * Object = SelectedAssets[ n ].GetAsset();
        if ( !Object )
            continue;

        // Only static meshes are supported
        if ( Object->GetClass() != UStaticMesh::StaticClass() )
            continue;

        ContentBrowserSelection.Add( Object );
    }

    return ContentBrowserSelection.Num();
}

int32 
SHoudiniToolPalette::GetWorldSelection( TArray< UObject* >& WorldSelection )
{
    WorldSelection.Empty();

    // Get the current editor selection
    if ( GEditor )
    {
        USelection* SelectedActors = GEditor->GetSelectedActors();
        for ( FSelectionIterator It( *SelectedActors ); It; ++It )
        {
            AActor * Actor = Cast< AActor >( *It );
            if ( !Actor )
                continue;

            // Ignore the SkySphere?
            FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
            if ( ClassName == TEXT( "BP_Sky_Sphere_C" ) )
                continue;

            // We're normally only selecting actors with StaticMeshComponents and SplineComponents
            // Heightfields? Filter here or later? also allow HoudiniAssets?
            WorldSelection.Add( Actor );
        }
    }

    return WorldSelection.Num();
}

#undef LOCTEXT_NAMESPACE
