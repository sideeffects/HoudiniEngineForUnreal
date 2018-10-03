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
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "AssetRegistryModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ActorFactories/ActorFactory.h"
#include "Engine/Selection.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "EditorViewportClient.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Framework/Application/SlateApplication.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "HoudiniToolPalette"


UHoudiniToolProperties::UHoudiniToolProperties(const FObjectInitializer & ObjectInitializer)
    :Super( ObjectInitializer )
    , Name()
    , Type(EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE)
    , SelectionType(EHoudiniToolSelectionType::HTOOL_SELECTION_ALL)
    , ToolTip()
    , IconPath(FFilePath())
    , HoudiniAsset(nullptr)
    , HelpURL()
{
};

UHoudiniToolDirectoryProperties::UHoudiniToolDirectoryProperties(const FObjectInitializer & ObjectInitializer)
    :Super(ObjectInitializer)
    , CustomHoudiniToolsDirectories()
{
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void
SHoudiniToolPalette::Construct( const FArguments& InArgs )
{
    FHoudiniEngineEditor& HoudiniEngineEditor = FModuleManager::GetModuleChecked<FHoudiniEngineEditor>("HoudiniEngineEditor");

    UpdateHoudiniToolDirectories();

    SAssignNew( HoudiniToolListView, SHoudiniToolListView )
        .SelectionMode( ESelectionMode::Single )
        .ListItemsSource( &HoudiniEngineEditor.GetHoudiniTools() )
        .OnGenerateRow( this, &SHoudiniToolPalette::MakeListViewWidget )
        .OnSelectionChanged( this, &SHoudiniToolPalette::OnSelectionChanged )
        .OnMouseButtonDoubleClick( this, &SHoudiniToolPalette::OnDoubleClickedListViewWidget )
        .OnContextMenuOpening( this, &SHoudiniToolPalette::ConstructHoudiniToolContextMenu )
        .ItemHeight( 35 );

    int32 ToolDirComboSelectedIdx = HoudiniEngineEditor.CurrentHoudiniToolDirIndex + 1;
    if (!HoudiniToolDirArray.IsValidIndex(ToolDirComboSelectedIdx))
        ToolDirComboSelectedIdx = 0;

    TSharedRef<SComboBox< TSharedPtr< FString > > > HoudiniToolDirComboBox =
        SNew(SComboBox< TSharedPtr< FString > >)
        .OptionsSource( &HoudiniToolDirArray)
        .InitiallySelectedItem(HoudiniToolDirArray[ ToolDirComboSelectedIdx ])
        .OnGenerateWidget( SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateLambda(
            []( TSharedPtr< FString > ChoiceEntry ) 
            {
                FText ChoiceEntryText = FText::FromString( *ChoiceEntry );
                return SNew( STextBlock )
                    .Text( ChoiceEntryText )
                    .ToolTipText( ChoiceEntryText )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
            } ) )
        .OnSelectionChanged( SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateLambda(
            [=]( TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType ) 
            {
                if ( !NewChoice.IsValid() )
                        return;
                OnDirectoryChange(*(NewChoice.Get()) );
            } ) )
            [
                SNew(STextBlock)
                .Text(this, &SHoudiniToolPalette::OnGetSelectedDirText )
                .ToolTipText( this,  &SHoudiniToolPalette::OnGetSelectedDirText )
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ];
    TSharedPtr< SButton > EditButton;
    FText EditButtonText = FText::FromString(TEXT("Edit"));
    FText EditButtonTooltip = FText::FromString(TEXT("Add, Remove or Edit custom Houdini tool directories"));

    FText ActiveShelfText = FText::FromString(TEXT("Active Shelf:"));
    FText ActiveShelfTooltip = FText::FromString(TEXT("Name of the currently selected Houdini Tool Shelf"));

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SScrollBorder, HoudiniToolListView.ToSharedRef())
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(ActiveShelfText)
                    .ToolTipText(ActiveShelfTooltip)
                    .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
                ]
                + SHorizontalBox::Slot()
                [
                    HoudiniToolDirComboBox
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SAssignNew(EditButton, SButton)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Text(EditButtonText)
                    .ToolTipText(EditButtonTooltip)
                    .OnClicked(FOnClicked::CreateSP(this, &SHoudiniToolPalette::OnEditToolDirectories))
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBorder, HoudiniToolListView.ToSharedRef())
            [
                HoudiniToolListView.ToSharedRef()
            ]
        ]
    ];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef< ITableRow >
SHoudiniToolPalette::MakeListViewWidget( TSharedPtr< FHoudiniTool > HoudiniTool, const TSharedRef< STableViewBase >& OwnerTable )
{
    check( HoudiniTool.IsValid() );
 
    auto HelpDefault = FEditorStyle::GetBrush( "HelpIcon" );
    auto HelpHovered = FEditorStyle::GetBrush( "HelpIcon.Hovered" );
    auto HelpPressed = FEditorStyle::GetBrush( "HelpIcon.Pressed" );
    auto DefaultTool = FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.HoudiniEngineLogo");
    TSharedPtr< SImage > HelpButtonImage;
    TSharedPtr< SButton > HelpButton;
    TSharedPtr< SImage > DefaultToolImage;

    // Building the tool's tooltip
    FString HoudiniToolTip = HoudiniTool->Name.ToString() + TEXT( "\n" );
    if ( HoudiniTool->HoudiniAsset.IsValid() )
        HoudiniToolTip += HoudiniTool->HoudiniAsset.ToSoftObjectPath().ToString() /*->AssetFileName */+ TEXT( "\n\n" );
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

    // Build the "DefaultTool" tool tip
    bool IsDefault = HoudiniTool->DefaultTool;
    FText DefaultToolText = FText::FromString( TEXT("Default Houdini Engine for Unreal plug-in tool") );

    TSharedRef< STableRow<TSharedPtr<FHoudiniTool>> > TableRowWidget =
        SNew( STableRow<TSharedPtr<FHoudiniTool>>, OwnerTable )
        .Style( FHoudiniEngineStyle::Get(), "HoudiniEngine.TableRow" )
        .OnDragDetected( this, &SHoudiniToolPalette::OnDraggingListViewWidget );

    TSharedPtr< SHorizontalBox > ContentBox;
    TSharedRef<SWidget> Content =
        SNew( SBorder )
        .BorderImage( FCoreStyle::Get().GetBrush( "NoBorder" ) )
        .Padding( 0 )
        .ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
        .Cursor( EMouseCursor::GrabHand )
        [ SAssignNew( ContentBox, SHorizontalBox ) ];

    // Tool Icon
    ContentBox->AddSlot()
        .AutoWidth()
        [
            SNew( SBorder )
            .Padding( 4.0f )
            .BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailShadow" ) )
            [
                SNew( SBox )
                .WidthOverride( 35.0f )
                .HeightOverride( 35.0f )
                [
                    SNew( SBorder )
                    .BorderImage( FHoudiniEngineStyle::Get()->GetBrush( "HoudiniEngine.ThumbnailBackground" ) )
                    .HAlign( HAlign_Center )
                    .VAlign( VAlign_Center )
                    [
                        SNew( SImage )
                        .Image( HoudiniTool->Icon )
                        .ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
                    ]
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
            .Text( HoudiniTool->Name )
            .ToolTip( SNew( SToolTip ).Text( ToolTipText ) )
        ];

    // Default Tool Icon
    if ( IsDefault )
    {
        ContentBox->AddSlot()
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( DefaultToolImage, SImage )
                .ToolTip( SNew( SToolTip ).Text( DefaultToolText ) )
            ];

        // Houdini logo for the Default Tool
        DefaultToolImage->SetImage( DefaultTool );
    }

    if ( HasHelp )
    {
        ContentBox->AddSlot()
        .VAlign( VAlign_Center )
        .AutoWidth()
        [
            SAssignNew( HelpButton, SButton )
            .ContentPadding( 0 )
            .ButtonStyle( FEditorStyle::Get(), "HelpButton" )
            .OnClicked( FOnClicked::CreateLambda( [ HelpURL ]() {
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
        ];

        HelpButtonImage->SetImage( TAttribute<const FSlateBrush *>::Create( TAttribute<const FSlateBrush *>::FGetter::CreateLambda( [=]()
        {
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
    }

    TableRowWidget->SetContent( Content );

    return TableRowWidget;
}

void
SHoudiniToolPalette::OnSelectionChanged( TSharedPtr<FHoudiniTool> ToolType, ESelectInfo::Type SelectionType )
{
    if ( ToolType.IsValid() )
    {
        ActiveTool = ToolType;
    }
    else
    {
        ActiveTool = NULL;
    }
}

FReply
SHoudiniToolPalette::OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
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
SHoudiniToolPalette::OnDoubleClickedListViewWidget( TSharedPtr<FHoudiniTool> HoudiniTool )
{
    if ( !HoudiniTool.IsValid() )
        return;

    return InstantiateHoudiniTool( HoudiniTool.Get() );
}

void 
SHoudiniToolPalette::InstantiateHoudiniTool( FHoudiniTool* HoudiniTool )
{
    if ( !HoudiniTool )
        return;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );

    // Load the asset
    UObject* AssetObj = HoudiniTool->HoudiniAsset.LoadSynchronous();
    if ( !AssetObj )
        return;

    // Get the asset Factory
    UActorFactory* Factory = GEditor->FindActorFactoryForActorClass( AHoudiniAssetActor::StaticClass() );
    if ( !Factory )
        return;

    // Get the current Level Editor Selection
    TArray<UObject * > WorldSelection;
    int32 WorldSelectionCount = FHoudiniEngineEditor::GetWorldSelection( WorldSelection );

    // Get the current Content browser selection
    TArray<UObject *> ContentBrowserSelection;
    int32 ContentBrowserSelectionCount = FHoudiniEngineEditor::GetContentBrowserSelection( ContentBrowserSelection );
    
    // By default, Content browser selection has a priority over the world selection
    bool UseCBSelection = ContentBrowserSelectionCount > 0;
    if ( HoudiniTool->SelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY )
        UseCBSelection = true;
    else if ( HoudiniTool->SelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY )
        UseCBSelection = false;

    // Modify the created actor's position from the current editor world selection
    FTransform SpawnTransform = GetDefaulToolSpawnTransform();
    if ( WorldSelectionCount > 0 )
    {
        // Get the "mean" transform of all the selected actors
        SpawnTransform = GetMeanWorldSelectionTransform();
    }

    // If the current tool is a batch one, we'll need to create multiple instances of the HDA
    if ( HoudiniTool->Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH )
    {
        // Unselect the current selection to select the created actor after
        if ( GEditor )
            GEditor->SelectNone( true, true, false );

        // An instance of the asset will be created for each selected object
        for( int32 SelecIndex = 0; SelecIndex < ( UseCBSelection ? ContentBrowserSelectionCount : WorldSelectionCount ); SelecIndex++ )
        {
            // Get the current object
            UObject* CurrentSelectedObject = nullptr;
            if ( UseCBSelection && ContentBrowserSelection.IsValidIndex( SelecIndex ) )
                CurrentSelectedObject = ContentBrowserSelection[ SelecIndex ];

            if ( !UseCBSelection && WorldSelection.IsValidIndex( SelecIndex ) )
                CurrentSelectedObject = WorldSelection[ SelecIndex ];

            if ( !CurrentSelectedObject )
                continue;

            // If it's an actor, use its Transform to spawn the HDA
            AActor* CurrentSelectedActor = Cast<AActor>( CurrentSelectedObject );
            if ( CurrentSelectedActor )
                SpawnTransform = CurrentSelectedActor->GetTransform();
            else
                SpawnTransform = GetDefaulToolSpawnTransform();

            // Create the actor for the HDA
            AActor* CreatedActor = Factory->CreateActor( AssetObj, GEditor->GetEditorWorldContext().World()->GetCurrentLevel(), SpawnTransform );
            if ( !CreatedActor )
                continue;

            // Get the HoudiniAssetActor / HoudiniAssetComponent we just created
            AHoudiniAssetActor* HoudiniAssetActor = ( AHoudiniAssetActor* )CreatedActor;
            if ( !HoudiniAssetActor )
                continue;

            UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
            if ( !HoudiniAssetComponent )
                continue;

            // Create and set the input preset for this HDA and selected Object
            TMap< UObject*, int32 > InputPreset;
            InputPreset.Add( CurrentSelectedObject, 0 );
            HoudiniAssetComponent->SetHoudiniToolInputPresets( InputPreset );

            // Select the Actor we just created
            if ( GEditor && GEditor->CanSelectActor( CreatedActor, true, false ) )
                GEditor->SelectActor( CreatedActor, true, true, true );
        }
    }
    else
    {
        // We only need to create a single instance of the asset, regarding of the selection
        AActor* CreatedActor = Factory->CreateActor( AssetObj, GEditor->GetEditorWorldContext().World()->GetCurrentLevel(), SpawnTransform );
        if ( !CreatedActor )
            return;

        // Generator tools don't need to preset their input
        if ( HoudiniTool->Type != EHoudiniToolType::HTOOLTYPE_GENERATOR )
        {
            TMap<UObject*, int32> InputPresets;
            AHoudiniAssetActor* HoudiniAssetActor = (AHoudiniAssetActor*)CreatedActor;            
            UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor ? HoudiniAssetActor->GetHoudiniAssetComponent() : nullptr;
            if ( HoudiniAssetComponent )
            {
                // Build the preset map
                int InputIndex = 0;
                for ( auto CurrentObject : ( UseCBSelection ? ContentBrowserSelection : WorldSelection ) )
                {
                    if ( !CurrentObject )
                        continue;

                    if ( HoudiniTool->Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI )
                    {
                        // The selection will be applied individually to multiple inputs
                        // (first object to first input, second object to second input etc...)
                        InputPresets.Add( CurrentObject, InputIndex++ );
                    }
                    else
                    {
                        // All the selection will be applied to the asset's first input
                        InputPresets.Add( CurrentObject, 0 );
                    }
                }

                // Set the input preset on the HoudiniAssetComponent
                if ( InputPresets.Num() > 0 )
                    HoudiniAssetComponent->SetHoudiniToolInputPresets( InputPresets );
            }
        }

        // Select the Actor we just created
        if ( GEditor->CanSelectActor( CreatedActor, true, true ) )
        {
            GEditor->SelectNone( true, true, false );
            GEditor->SelectActor( CreatedActor, true, true, true );
        }
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

            FTransform CurrentTransform = Actor->GetTransform();

            ALandscape* Landscape = Cast< ALandscape >( Actor );
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

// Builds the context menu for the selected tool
TSharedPtr<SWidget>
SHoudiniToolPalette::ConstructHoudiniToolContextMenu()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked< FAssetRegistryModule >( "AssetRegistry" );

    FMenuBuilder MenuBuilder( true, NULL );
    
    TArray< UObject* > AssetArray;
    if ( ActiveTool.IsValid() && !ActiveTool->HoudiniAsset.IsNull() )
    {
        // Load the asset
        UObject* AssetObj = ActiveTool->HoudiniAsset.LoadSynchronous();
        if( AssetObj )
            AssetArray.Add( AssetObj );
    }

    //MenuBuilder.AddMenuEntry(FGlobalEditorCommonCommands::Get().FindInContentBrowser );

    FAssetToolsModule & AssetToolsModule = FModuleManager::GetModuleChecked< FAssetToolsModule >( "AssetTools" );
    TSharedPtr< IAssetTypeActions > EngineActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( UHoudiniAsset::StaticClass() ).Pin();
    if ( EngineActions.IsValid() )
        EngineActions->GetActions( AssetArray, MenuBuilder );

    /*
    // Add HoudiniTools actions
    MenuBuilder.AddMenuSeparator();

    // Remove Tool
    MenuBuilder.AddMenuEntry(
        NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_Remove", "Remove Tool" ),
        NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_RemoveTooltip", "Remove the selected tool from the list of Houdini Tools." ),
        FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateSP(this, &SHoudiniToolPalette::RemoveActiveTool ),
            FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
        )
    );

    // Edit Tool
    MenuBuilder.AddMenuEntry(
        NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_Edit", "Edit Tool Properties" ),
        NSLOCTEXT( "HoudiniToolsTypeActions", "HoudiniTool_EditTooltip", "Edit the selected tool properties." ),
        FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateSP(this, &SHoudiniToolPalette::EditActiveHoudiniTool ),
            FCanExecuteAction::CreateLambda([&] { return IsActiveHoudiniToolEditable(); } )
        )
    );
    */

    return MenuBuilder.MakeWidget();
}

FReply
SHoudiniToolPalette::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
    if ( !Operation.IsValid() )
        return FReply::Unhandled();
    /*
    if ( Operation->IsOfType<FAssetDragDropOp>() )
    {
        // Handle Assets Drag and Drop operations
        TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( DragDropEvent.GetOperation() );

        // Only keep HoudiniAssets
        TArray< FAssetData > DroppedAssets = DragDropOp->GetAssets();
        TArray< UHoudiniAsset *> DroppedHoudiniAssets;
        for ( auto CurrentAsset : DroppedAssets )
        {
            // We only care about Houdini Assets
            if (CurrentAsset.GetClass() != UHoudiniAsset::StaticClass())
                continue;

            UHoudiniAsset * HoudiniAsset = Cast<UHoudiniAsset>(CurrentAsset.GetAsset());
            if ( !HoudiniAsset )
                continue;

            DroppedHoudiniAssets.Add( HoudiniAsset );
        }

        // Open a property editor window to create the new tools
        if ( DroppedHoudiniAssets.Num() > 0 )
            ShowAddHoudiniToolWindow( DroppedHoudiniAssets );

        return FReply::Handled();
    }
    */
    /*
    if ( Operation->IsOfType<FExternalDragOperation>() )
    {
    // Handle external Drag and Drop operations
    TSharedPtr<FExternalDragOperation> DragDropOp = StaticCastSharedPtr<FExternalDragOperation>( DragDropEvent.GetOperation() );
    OnFilesDragDropped.ExecuteIfBound( DragDropOp->GetFiles(), TreeItem.Pin() );
    return FReply::Handled();
    }
    */

    return FReply::Unhandled();
}

void
SHoudiniToolPalette::ShowAddHoudiniToolWindow(const TArray< UHoudiniAsset *>& HoudiniAssets )
{
    TArray<UObject *> NewCustomHoudiniTools;
    for ( auto CurrentHoudiniAsset : HoudiniAssets )
    {
        if ( !CurrentHoudiniAsset )
            continue;

        // Create a new Tool property object for the property dialog
        FString ToolName = CurrentHoudiniAsset->GetName() + TEXT(" (") + CurrentHoudiniAsset->AssetFileName + TEXT(")");
        UHoudiniToolProperties* NewToolProperty = NewObject< UHoudiniToolProperties >( GetTransientPackage(), FName( *ToolName ) );
        NewToolProperty->AddToRoot();

        // Set the default values for this asset
        NewToolProperty->HoudiniAsset = CurrentHoudiniAsset;
        NewToolProperty->Name = CurrentHoudiniAsset->GetName();
        NewToolProperty->Type = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
        NewToolProperty->ToolTip = TEXT("");
        NewToolProperty->IconPath = FFilePath();
        NewToolProperty->HelpURL = TEXT("");//TEXT("http://www.sidefx.com/docs/unreal/");

        NewCustomHoudiniTools.Add( NewToolProperty );
    }

    TSharedRef< SWindow > Window = CreateFloatingDetailsView( NewCustomHoudiniTools );    
    Window->SetOnWindowClosed( FOnWindowClosed::CreateSP( this, &SHoudiniToolPalette::OnAddHoudiniToolWindowClosed, NewCustomHoudiniTools ) );
}

void
SHoudiniToolPalette::OnAddHoudiniToolWindowClosed( const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects )
{
    TArray< FHoudiniTool > NewToolArray;
    TArray< FHoudiniToolDescription > NewToolDescriptionArray;
    for ( int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ObjIdx++ )
    {
        UHoudiniToolProperties* ToolProperties = Cast< UHoudiniToolProperties >( InObjects[ ObjIdx ] );
        if ( !ToolProperties )
            continue;

        // Remove the tool from Root
        ToolProperties->RemoveFromRoot();

        // Check the Tool is valid before adding it
        // TODO : FIX ME
        if ( !ToolProperties->HoudiniAsset )
            continue;

        if (!ToolProperties->HoudiniAsset->IsValidLowLevel())
            continue;

        FString IconPath = FPaths::ConvertRelativePathToFull( ToolProperties->IconPath.FilePath );
        const FSlateBrush* CustomIconBrush = nullptr;
        if ( FPaths::FileExists( IconPath ) )
        {
            FName BrushName = *IconPath;
            CustomIconBrush = new FSlateDynamicImageBrush( BrushName, FVector2D( 40.f, 40.f ) );
        }
        else
        {
            CustomIconBrush = FHoudiniEngineStyle::Get()->GetBrush( TEXT( "HoudiniEngine.HoudiniEngineLogo40" ) );
        }

        // Create a new Houdini Tool from the modified properties
        FHoudiniTool NewHoudiniTool(
            ToolProperties->HoudiniAsset,
            FText::FromString( ToolProperties->Name ),
            ToolProperties->Type,
            ToolProperties->SelectionType,
            FText::FromString (ToolProperties->ToolTip ),
            CustomIconBrush,
            ToolProperties->HelpURL,
            false );

        // Make sure we're not adding a duplicate (same asset, same type)
        int32 FoundIndex = -1;
        bool IsDefault = false;
        if ( FHoudiniEngineEditor::Get().FindHoudiniTool( NewHoudiniTool, FoundIndex, IsDefault ) )
            continue;

        // Make sure (again) that we're not adding a duplicate
        if ( FHoudiniEngineEditor::Get().FindHoudiniToolInHoudiniSettings( NewHoudiniTool, FoundIndex ) )
            continue;

        // Create a new Houdini Tool Description from the modified properties
        FHoudiniToolDescription NewToolDescription;
        NewToolDescription.HoudiniAsset = ToolProperties->HoudiniAsset;
        NewToolDescription.AssetPath = ToolProperties->AssetPath;
        NewToolDescription.Name = ToolProperties->Name;
        NewToolDescription.Type = ToolProperties->Type;
        NewToolDescription.ToolTip = ToolProperties->ToolTip;
        NewToolDescription.IconPath = ToolProperties->IconPath;
        NewToolDescription.HelpURL = ToolProperties->HelpURL;

        // Add the tool and its description to the list
        NewToolArray.Add( NewHoudiniTool );
        NewToolDescriptionArray.Add( NewToolDescription );
    }

    if ( NewToolArray.Num() <= 0 || NewToolDescriptionArray.Num() <= 0 )
        return;

    // Add the new tools to the editor
    TArray< TSharedPtr<FHoudiniTool> >* EditorTools = FHoudiniEngineEditor::Get().GetHoudiniToolsForWrite();
    for ( auto NewTool : NewToolArray )
    {
        if ( EditorTools )
            EditorTools->Add( MakeShareable( new FHoudiniTool(
                NewTool.HoudiniAsset, NewTool.Name, NewTool.Type, NewTool.SelectionType, NewTool.ToolTipText, NewTool.Icon, NewTool.HelpURL, false ) ) );
    }

    // Get the runtime settings to add the new custom tools there
    UHoudiniRuntimeSettings * HoudiniRuntimeSettings = const_cast<UHoudiniRuntimeSettings*>( GetDefault< UHoudiniRuntimeSettings >() );
    if ( !HoudiniRuntimeSettings )
        return;

    // TODO: FIX ME!
    /*
    for ( auto NewToolDesc : NewToolDescriptionArray )
    {
        HoudiniRuntimeSettings->CustomHoudiniTools.Add( NewToolDesc );
    }

    // Save the Settings file to keep the new tools upon restart
    HoudiniRuntimeSettings->SaveConfig();
    */

    // Call construct to refresh the shelf
    SHoudiniToolPalette::FArguments Args;
    Construct( Args );
}

void
SHoudiniToolPalette::EditActiveHoudiniTool()
{
    if ( !ActiveTool.IsValid() )
        return;

    if ( !IsActiveHoudiniToolEditable() )
        return;

    // Create a new Tool property object for the property dialog
    FString ToolName = ActiveTool->Name.ToString();
    if ( ActiveTool->HoudiniAsset )
        ToolName += TEXT(" (") + ActiveTool->HoudiniAsset->AssetFileName + TEXT(")");

    UHoudiniToolProperties* NewToolProperty = NewObject< UHoudiniToolProperties >( GetTransientPackage(), FName( *ToolName ) );
    NewToolProperty->AddToRoot();

    // Set the default values for this asset
    NewToolProperty->HoudiniAsset = ActiveTool->HoudiniAsset;
    NewToolProperty->Name = ActiveTool->Name.ToString();
    NewToolProperty->Type = ActiveTool->Type;
    NewToolProperty->ToolTip = ActiveTool->ToolTipText.ToString();
    FName IconResourceName = ActiveTool->Icon->GetResourceName();
    NewToolProperty->IconPath.FilePath = IconResourceName.ToString();
    NewToolProperty->HelpURL = ActiveTool->HelpURL;

    TArray<UObject *> ActiveHoudiniTools;
    ActiveHoudiniTools.Add( NewToolProperty );

    // Create a new property editor window
    TSharedRef< SWindow > Window = CreateFloatingDetailsView( ActiveHoudiniTools );
    Window->SetOnWindowClosed( FOnWindowClosed::CreateSP( this, &SHoudiniToolPalette::OnEditHoudiniToolWindowClosed, ActiveHoudiniTools ) );
}

void
SHoudiniToolPalette::OnEditHoudiniToolWindowClosed( const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects )
{
    TArray< FHoudiniTool > EditedToolArray;
    TArray< FHoudiniToolDescription > EditedToolDescriptionArray;
    for ( int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ObjIdx++ )
    {
        UHoudiniToolProperties* ToolProperties = Cast< UHoudiniToolProperties >( InObjects[ ObjIdx ] );
        if ( !ToolProperties )
            continue;

        // Check the Tool is valid before adding it
        if ( !ToolProperties->HoudiniAsset || !ToolProperties->HoudiniAsset->IsValidLowLevel() )
        {
            // Remove the tool from Root before skipping it
            ToolProperties->RemoveFromRoot();
            continue;
        }

        FString IconPath = FPaths::ConvertRelativePathToFull( ToolProperties->IconPath.FilePath );
        const FSlateBrush* CustomIconBrush = nullptr;
        if ( FPaths::FileExists( IconPath ) )
        {
            FName BrushName = *IconPath;
            CustomIconBrush = new FSlateDynamicImageBrush( BrushName, FVector2D( 40.f, 40.f ) );
        }
        else
        {
            CustomIconBrush = FHoudiniEngineStyle::Get()->GetBrush( TEXT( "HoudiniEngine.HoudiniEngineLogo40" ) );
        }

        // Create a new Houdini Tool from the modified properties
        FHoudiniTool EditedHoudiniTool(
            ToolProperties->HoudiniAsset,
            FText::FromString( ToolProperties->Name ),
            ToolProperties->Type,
            ToolProperties->SelectionType,
            FText::FromString (ToolProperties->ToolTip ),
            CustomIconBrush,
            ToolProperties->HelpURL,
            false );

        EditedToolArray.Add( EditedHoudiniTool );

        // Create a new Houdini Tool Description from the modified properties
        FHoudiniToolDescription EditedToolDescription;
        EditedToolDescription.HoudiniAsset = ToolProperties->HoudiniAsset;
        EditedToolDescription.AssetPath = ToolProperties->AssetPath;
        EditedToolDescription.Name = ToolProperties->Name;
        EditedToolDescription.Type = ToolProperties->Type;
        EditedToolDescription.ToolTip = ToolProperties->ToolTip;
        EditedToolDescription.IconPath = ToolProperties->IconPath;
        EditedToolDescription.HelpURL = ToolProperties->HelpURL;

        EditedToolDescriptionArray.Add( EditedToolDescription );

        // Remove the tool from Root
        ToolProperties->RemoveFromRoot();
    }

    if ( EditedToolArray.Num() <= 0 )
        return;

    // Add the new tools to the editor
    TArray< TSharedPtr<FHoudiniTool> >* EditorTools = FHoudiniEngineEditor::Get().GetHoudiniToolsForWrite();
    if ( !EditorTools )
        return;

    // Get the runtime settings to add the new custom tools there
    UHoudiniRuntimeSettings * HoudiniRuntimeSettings = const_cast<UHoudiniRuntimeSettings*>( GetDefault< UHoudiniRuntimeSettings >() );
    if ( !HoudiniRuntimeSettings )
        return;

    for ( int32 Idx = 0; Idx < EditedToolArray.Num(); Idx++ )
    {
        FHoudiniTool EditedTool = EditedToolArray[ Idx ];
        // Update the editor tool list
        int32 FoundIndex = -1;
        bool IsDefault = false;
        if ( !FHoudiniEngineEditor::Get().FindHoudiniTool( EditedTool, FoundIndex, IsDefault ) )
            continue;

        if ( IsDefault )
            continue;

        (*EditorTools)[ FoundIndex ] = MakeShareable( new FHoudiniTool(
            EditedTool.HoudiniAsset, EditedTool.Name, EditedTool.Type, EditedTool.SelectionType, EditedTool.ToolTipText, EditedTool.Icon, EditedTool.HelpURL, false ) );

        // Update in the settings custom tools
        if ( !FHoudiniEngineEditor::Get().FindHoudiniToolInHoudiniSettings( EditedTool, FoundIndex ) )
            continue;
        // TODO: FIX ME!
        //HoudiniRuntimeSettings->CustomHoudiniTools[ FoundIndex ] = EditedToolDescriptionArray[ Idx ];
    }

    // Save the Settings file to keep the new tools upon restart
    HoudiniRuntimeSettings->SaveConfig();

    // Call construct to refresh the shelf
    SHoudiniToolPalette::FArguments Args;
    Construct( Args );
}

TSharedRef<SWindow>
SHoudiniToolPalette::CreateFloatingDetailsView( const TArray< UObject* >& InObjects )
{
    TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
        .Title(NSLOCTEXT("PropertyEditor", "WindowTitle", "Houdini Tools Property Editor"))
        .ClientSize(FVector2D(400, 550));

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
    Args.bShowActorLabel = false;

    FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
    TSharedRef<IDetailsView> DetailView = PropertyEditorModule.CreateDetailView( Args );
    DetailView->SetObjects( InObjects );

    NewSlateWindow->SetContent(
        SNew( SBorder )
        .BorderImage( FEditorStyle::GetBrush( TEXT("PropertyWindow.WindowBorder") ) )
        [
            DetailView
        ]
    );

    return NewSlateWindow;
}

FReply
SHoudiniToolPalette::OnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
    /*
    if ( InKeyEvent.GetKey() == EKeys::Delete )
    {
        RemoveActiveTool();
        return FReply::Handled();
    }
    else if ( InKeyEvent.GetKey() == EKeys::Enter )
    {
        EditActiveHoudiniTool();
        return FReply::Handled();
    }
    else
    */
    if (InKeyEvent.GetKey() == EKeys::F5 )
    {
        OnDirectoryChange( CurrentHoudiniToolDir );
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

void
SHoudiniToolPalette::RemoveActiveTool()
{
    if ( !ActiveTool.IsValid() )
        return;

    // Remove the tool from the editor list
    TArray< TSharedPtr<FHoudiniTool> >* EditorTools = FHoudiniEngineEditor::Get().GetHoudiniToolsForWrite();
    if ( !EditorTools )
        return;

    int32 EditorIndex = -1;
    bool IsDefaultTool = false;
    if ( !FHoudiniEngineEditor::Get().FindHoudiniTool( *ActiveTool, EditorIndex, IsDefaultTool ) )
        return;

    if ( IsDefaultTool )
        return;

    EditorTools->RemoveAt( EditorIndex );

    // Remove the tool from the runtime settings
    UHoudiniRuntimeSettings * HoudiniRuntimeSettings = const_cast<UHoudiniRuntimeSettings*>( GetDefault< UHoudiniRuntimeSettings >() );
    if ( !HoudiniRuntimeSettings )
        return;

    int32 SettingsIndex = -1;
    if ( !FHoudiniEngineEditor::Get().FindHoudiniToolInHoudiniSettings( *ActiveTool, SettingsIndex ) )
        return;

    // TODO: FIX ME!
    //HoudiniRuntimeSettings->CustomHoudiniTools.RemoveAt( SettingsIndex );

    // Save the Settings file to keep the new tools upon restart
    HoudiniRuntimeSettings->SaveConfig();

    // Call construct to refresh the shelf
    SHoudiniToolPalette::FArguments Args;
    Construct( Args );
}

bool
SHoudiniToolPalette::IsActiveHoudiniToolEditable()
{
    return false;
    /*
    int32 FoundIndex = -1;
    bool IsDefault = false;

    if ( !ActiveTool.IsValid() )
        return false;

    if ( !FHoudiniEngineEditor::Get().FindHoudiniTool( *ActiveTool, FoundIndex, IsDefault ) )
        return false;

    return !IsDefault;
    */
}

FReply
SHoudiniToolPalette::OnEditToolDirectories()
{
    // Append all the custom tools directory from the runtime settings
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (!HoudiniRuntimeSettings)
        return FReply::Handled();

    TArray<UObject *> NewCustomHoudiniToolDirs;

    FString CustomToolDirs = TEXT("CustomToolDir");
    UHoudiniToolDirectoryProperties* NewToolDirProperty = NewObject< UHoudiniToolDirectoryProperties >(GetTransientPackage(), FName(*CustomToolDirs));
    NewToolDirProperty->CustomHoudiniToolsDirectories = HoudiniRuntimeSettings->CustomHoudiniToolsLocation;
    NewCustomHoudiniToolDirs.Add(NewToolDirProperty);

    TSharedRef< SWindow > Window = CreateFloatingDetailsView(NewCustomHoudiniToolDirs);
    Window->SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SHoudiniToolPalette::OnEditToolDirectoriesWindowClosed, NewCustomHoudiniToolDirs));

    return FReply::Handled();
}


void
SHoudiniToolPalette::OnEditToolDirectoriesWindowClosed(const TSharedRef<SWindow>& InWindow, TArray<UObject *> InObjects)
{
    TArray<FHoudiniToolDirectory> EditedToolDirectories;
    for (int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ObjIdx++)
    {
        UHoudiniToolDirectoryProperties* ToolDirProperties = Cast< UHoudiniToolDirectoryProperties >(InObjects[ObjIdx]);
        if (!ToolDirProperties)
            continue;

        for ( auto CurrentToolDir : ToolDirProperties->CustomHoudiniToolsDirectories )
        {
            EditedToolDirectories.Add( CurrentToolDir );
        }

        // Remove the tool directory from Root
        ToolDirProperties->RemoveFromRoot();
    }

    // Get the runtime settings to add the new custom tools there
    UHoudiniRuntimeSettings * HoudiniRuntimeSettings = const_cast<UHoudiniRuntimeSettings*>(GetDefault< UHoudiniRuntimeSettings >());
    if (!HoudiniRuntimeSettings)
        return;

    // Replace the Tool dir in the runtime settings with the edited list
    HoudiniRuntimeSettings->CustomHoudiniToolsLocation.Empty();
    HoudiniRuntimeSettings->CustomHoudiniToolsLocation.Append( EditedToolDirectories );

    // Save the Settings file to keep the new tools upon restart
    HoudiniRuntimeSettings->SaveConfig();

    // Call construct to refresh the shelf
    SHoudiniToolPalette::FArguments Args;
    Construct(Args);
}

void
SHoudiniToolPalette::OnDirectoryChange(const FString& NewDir)
{
    if ( NewDir.IsEmpty() )
        return;

    if ( CurrentHoudiniToolDir.Equals(NewDir) )
    {
        HoudiniToolListView->RequestListRefresh();
        return;
    }

    CurrentHoudiniToolDir = NewDir;
    
    int32 newIndex = 0;
    for (int32 n = 0; n < HoudiniToolDirArray.Num(); n++)
    {
        if (!HoudiniToolDirArray[n].IsValid())
            continue;

        if (!HoudiniToolDirArray[n]->Equals(NewDir))
            continue;

        FHoudiniEngineEditor& HoudiniEngineEditor = FModuleManager::GetModuleChecked<FHoudiniEngineEditor>("HoudiniEngineEditor");
        HoudiniEngineEditor.UpdateHoudiniToolList(n - 1);
        HoudiniToolListView->RequestListRefresh();
    }
}

void
SHoudiniToolPalette::UpdateHoudiniToolDirectories()
{
    FHoudiniEngineEditor& HoudiniEngineEditor = FModuleManager::GetModuleChecked<FHoudiniEngineEditor>("HoudiniEngineEditor");

    int32 ChoiceIndex = HoudiniEngineEditor.CurrentHoudiniToolDirIndex;

    // Refresh the Editor's Houdini Tool list
    HoudiniEngineEditor.UpdateHoudiniToolList( ChoiceIndex );

    // Add the directory choice lists
    HoudiniToolDirArray.Empty();

    TArray<FHoudiniToolDirectory> ToolDirArray;
    HoudiniEngineEditor.GetHoudiniToolDirectories( ToolDirArray );

    {
        FString * ChoiceLabel = new FString(TEXT("All"));
        HoudiniToolDirArray.Add(TSharedPtr< FString >(ChoiceLabel));

        if (ChoiceIndex < 0 )
            CurrentHoudiniToolDir = *ChoiceLabel;
    }

    {
        FString * ChoiceLabel = new FString(TEXT("Default"));
        HoudiniToolDirArray.Add(TSharedPtr< FString >(ChoiceLabel));

        if ( ChoiceIndex == 0 )
            CurrentHoudiniToolDir = *ChoiceLabel;
    }

    for (int32 n = 1; n < ToolDirArray.Num(); n++)
    {
        FString * ChoiceLabel = new FString( ToolDirArray[n].Name );
        HoudiniToolDirArray.Add(TSharedPtr< FString >(ChoiceLabel));

        if ( ChoiceIndex == n )
            CurrentHoudiniToolDir = *ChoiceLabel;
    }
}

FText
SHoudiniToolPalette::OnGetSelectedDirText() const
{
    return FText::FromString(CurrentHoudiniToolDir);
}

#undef LOCTEXT_NAMESPACE
