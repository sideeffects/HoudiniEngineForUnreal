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
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetThumbnailRenderer.h"
#include "HoudiniAsset.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniHandleComponentVisualizer.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniRuntimeSettingsDetails.h"
#include "HoudiniAssetTypeActions.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAttributePaintEdMode.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniAssetActor.h"

#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/ObjectLibrary.h"
#include "EditorDirectories.h"
#include "Styling/SlateStyleRegistry.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "SHoudiniToolPalette.h"
#include "IPlacementModeModule.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

const FName
FHoudiniEngineEditor::HoudiniEngineEditorAppIdentifier = FName( TEXT( "HoudiniEngineEditorApp" ) );

IMPLEMENT_MODULE( FHoudiniEngineEditor, HoudiniEngineEditor );
DEFINE_LOG_CATEGORY( LogHoudiniEngineEditor );

FHoudiniEngineEditor *
FHoudiniEngineEditor::HoudiniEngineEditorInstance = nullptr;

FHoudiniEngineEditor &
FHoudiniEngineEditor::Get()
{
    return *HoudiniEngineEditorInstance;
}

bool
FHoudiniEngineEditor::IsInitialized()
{
    return FHoudiniEngineEditor::HoudiniEngineEditorInstance != nullptr;
}

FHoudiniEngineEditor::FHoudiniEngineEditor()
    : LastHoudiniAssetComponentUndoObject( nullptr )
{}

void
FHoudiniEngineEditor::StartupModule()
{
    HOUDINI_LOG_MESSAGE( TEXT( "Starting the Houdini Engine Editor module." ) );
    
    // Register asset type actions.
    RegisterAssetTypeActions();

    // Register asset brokers.
    RegisterAssetBrokers();

    // Register component visualizers.
    RegisterComponentVisualizers();

    // Register detail presenters.
    RegisterDetails();

    // Register actor factories.
    RegisterActorFactories();

    // Create style set.
    RegisterStyleSet();

    // Register thumbnails.
    RegisterThumbnails();

    // Extend menu.
    ExtendMenu();

    // Register global undo / redo callbacks.
    RegisterForUndo();

#ifdef HOUDINI_MODE
    // Register editor modes
    RegisterModes();
#endif 
#if 0
 	// WIP
    RegisterPlacementModeExtensions();
#endif
    // Store the instance.
    FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;
}

void
FHoudiniEngineEditor::ShutdownModule()
{
    HOUDINI_LOG_MESSAGE( TEXT( "Shutting down the Houdini Engine Editor module." ) );

    // Unregister asset type actions.
    UnregisterAssetTypeActions();

    // Unregister detail presenters.
    UnregisterDetails();

    // Unregister thumbnails.
    UnregisterThumbnails();

    // Unregister our component visualizers.
    UnregisterComponentVisualizers();

    // Unregister global undo / redo callbacks.
    UnregisterForUndo();

#ifdef HOUDINI_MODE
    UnregisterModes();
#endif

    UnregisterPlacementModeExtensions();
}

void
FHoudiniEngineEditor::RegisterComponentVisualizers()
{
    if ( GUnrealEd )
    {
        if ( !SplineComponentVisualizer.IsValid() )
        {
            SplineComponentVisualizer = MakeShareable( new FHoudiniSplineComponentVisualizer );

            GUnrealEd->RegisterComponentVisualizer(
                UHoudiniSplineComponent::StaticClass()->GetFName(),
                SplineComponentVisualizer
            );

            SplineComponentVisualizer->OnRegister();
        }

        if ( !HandleComponentVisualizer.IsValid() )
        {
            HandleComponentVisualizer = MakeShareable( new FHoudiniHandleComponentVisualizer );

            GUnrealEd->RegisterComponentVisualizer(
                UHoudiniHandleComponent::StaticClass()->GetFName(),
                HandleComponentVisualizer
            );

            HandleComponentVisualizer->OnRegister();
        }
    }
}

void
FHoudiniEngineEditor::UnregisterComponentVisualizers()
{
    if ( GUnrealEd )
    {
        if ( SplineComponentVisualizer.IsValid() )
            GUnrealEd->UnregisterComponentVisualizer( UHoudiniSplineComponent::StaticClass()->GetFName() );

        if ( HandleComponentVisualizer.IsValid() )
            GUnrealEd->UnregisterComponentVisualizer( UHoudiniHandleComponent::StaticClass()->GetFName() );
    }
}

void
FHoudiniEngineEditor::RegisterDetails()
{
    FPropertyEditorModule & PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

    // Register details presenter for our component type and runtime settings.
    PropertyModule.RegisterCustomClassLayout(
        TEXT( "HoudiniAssetComponent" ),
        FOnGetDetailCustomizationInstance::CreateStatic( &FHoudiniAssetComponentDetails::MakeInstance ) );
    PropertyModule.RegisterCustomClassLayout(
        TEXT( "HoudiniRuntimeSettings" ),
        FOnGetDetailCustomizationInstance::CreateStatic( &FHoudiniRuntimeSettingsDetails::MakeInstance ) );
}

void
FHoudiniEngineEditor::UnregisterDetails()
{
    if ( FModuleManager::Get().IsModuleLoaded( "PropertyEditor" ) )
    {
        FPropertyEditorModule & PropertyModule =
            FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

        PropertyModule.UnregisterCustomClassLayout( TEXT( "HoudiniAssetComponent" ) );
        PropertyModule.UnregisterCustomClassLayout( TEXT( "HoudiniRuntimeSettings" ) );
    }
}

void
FHoudiniEngineEditor::RegisterAssetTypeActions()
{
    // Create and register asset type actions for Houdini asset.
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked< FAssetToolsModule >( "AssetTools" ).Get();
    RegisterAssetTypeAction( AssetTools, MakeShareable( new FHoudiniAssetTypeActions() ) );
}

void
FHoudiniEngineEditor::UnregisterAssetTypeActions()
{
    // Unregister asset type actions we have previously registered.
    if ( FModuleManager::Get().IsModuleLoaded( "AssetTools" ) )
    {
        IAssetTools & AssetTools = FModuleManager::GetModuleChecked< FAssetToolsModule >( "AssetTools" ).Get();

        for ( int32 Index = 0; Index < AssetTypeActions.Num(); ++Index )
            AssetTools.UnregisterAssetTypeActions( AssetTypeActions[ Index ].ToSharedRef() );

        AssetTypeActions.Empty();
    }
}

void
FHoudiniEngineEditor::RegisterAssetTypeAction( IAssetTools & AssetTools, TSharedRef< IAssetTypeActions > Action )
{
    AssetTools.RegisterAssetTypeActions( Action );
    AssetTypeActions.Add( Action );
}

void
FHoudiniEngineEditor::RegisterAssetBrokers()
{
    // Create and register broker for Houdini asset.
    HoudiniAssetBroker = MakeShareable( new FHoudiniAssetBroker() );
    FComponentAssetBrokerage::RegisterBroker( HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true );
}

void
FHoudiniEngineEditor::UnregisterAssetBrokers()
{
    if ( UObjectInitialized() )
    {
        // Unregister broker.
        FComponentAssetBrokerage::UnregisterBroker( HoudiniAssetBroker );
    }
}

void
FHoudiniEngineEditor::RegisterActorFactories()
{
    if ( GEditor )
    {
        UHoudiniAssetActorFactory * HoudiniAssetActorFactory =
            NewObject< UHoudiniAssetActorFactory >( GetTransientPackage(), UHoudiniAssetActorFactory::StaticClass() );

        GEditor->ActorFactories.Add( HoudiniAssetActorFactory );
    }
}

void
FHoudiniEngineEditor::ExtendMenu()
{
    if ( !IsRunningCommandlet() )
    {
        // Extend main menu, we will add Houdini section.
        MainMenuExtender = MakeShareable( new FExtender );
        MainMenuExtender->AddMenuExtension(
            "FileLoadAndSave", EExtensionHook::After, NULL,
            FMenuExtensionDelegate::CreateRaw( this, &FHoudiniEngineEditor::AddHoudiniMenuExtension ) );
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >( "LevelEditor" );
        LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( MainMenuExtender );
    }
}

void
FHoudiniEngineEditor::AddHoudiniMenuExtension( FMenuBuilder & MenuBuilder )
{
    MenuBuilder.BeginSection( "Houdini", LOCTEXT( "HoudiniLabel", "Houdini Engine" ) );

    MenuBuilder.AddMenuEntry(
        LOCTEXT("HoudiniMenuEntryTitleOpenHoudini", "Open scene in Houdini"),
        LOCTEXT("HoudiniMenuEntryToolTipOpenInHoudini", "Opens the current Houdini scene in Houdini."),
        FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
        FUIAction(
            FExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::OpenInHoudini),
            FCanExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::CanOpenInHoudini)));

    MenuBuilder.AddMenuEntry(
        LOCTEXT( "HoudiniMenuEntryTitleSaveHip", "Save Houdini scene (HIP)" ),
        LOCTEXT( "HoudiniMenuEntryToolTipSaveHip", "Saves a .hip file of the current Houdini scene." ),
        FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::SaveHIPFile ),
            FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanSaveHIPFile ) ) );

    MenuBuilder.AddMenuEntry(
        LOCTEXT( "HoudiniMenuEntryTitleReportBug", "Report a plugin bug" ),
        LOCTEXT( "HoudiniMenuEntryToolTipReportBug", "Report a bug for Houdini Engine plugin." ),
        FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::ReportBug ),
            FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanReportBug ) ) );

    MenuBuilder.AddMenuEntry(
        LOCTEXT( "HoudiniMenuEntryTitleCleanTemp", "Clean Houdini Engine Temp Folder" ),
        LOCTEXT( "HoudiniMenuEntryToolTipCleanTemp", "Deletes the unused temporary files in the Temporary Cook Folder." ),
        FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CleanUpTempFolder ),
            FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanCleanUpTempFolder ) ) );

    MenuBuilder.AddMenuEntry(
        LOCTEXT( "HoudiniMenuEntryTitleBakeAll", "Bake And Replace All Houdini Assets" ),
        LOCTEXT( "HoudiniMenuEntryToolTipBakeAll", "Bakes and replaces with blueprints all Houdini Assets in the scene." ),
        FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::BakeAllAssets ),
            FCanExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::CanBakeAllAssets ) ) );

    MenuBuilder.EndSection();
}

bool
FHoudiniEngineEditor::CanSaveHIPFile() const
{
    return FHoudiniEngine::IsInitialized();
}

void
FHoudiniEngineEditor::SaveHIPFile()
{
    IDesktopPlatform * DesktopPlatform = FDesktopPlatformModule::Get();
    if ( DesktopPlatform && FHoudiniEngineUtils::IsInitialized() )
    {
        TArray< FString > SaveFilenames;
        bool bSaved = false;
        void * ParentWindowWindowHandle = NULL;

        IMainFrameModule & MainFrameModule = FModuleManager::LoadModuleChecked< IMainFrameModule >( TEXT( "MainFrame" ) );
        const TSharedPtr< SWindow > & MainFrameParentWindow = MainFrameModule.GetParentWindow();
        if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
            ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();

        bSaved = DesktopPlatform->SaveFileDialog(
            ParentWindowWindowHandle,
            NSLOCTEXT( "SaveHIPFile", "SaveHIPFile", "Saves a .hip file of the current Houdini scene." ).ToString(),
            *( FEditorDirectories::Get().GetLastDirectory( ELastDirectory::GENERIC_EXPORT ) ),
            TEXT( "" ),
            TEXT( "Houdini HIP file|*.hip" ),
            EFileDialogFlags::None,
            SaveFilenames );

        if ( bSaved && SaveFilenames.Num() )
        {
            // Get first path.
            std::wstring HIPPath( *SaveFilenames[ 0 ] );
            std::string HIPPathConverted( HIPPath.begin(), HIPPath.end() );

            // Save HIP file through Engine.
            FHoudiniApi::SaveHIPFile( FHoudiniEngine::Get().GetSession(), HIPPathConverted.c_str(), false );
        }
    }
}

bool
FHoudiniEngineEditor::CanOpenInHoudini() const
{
    return FHoudiniEngine::IsInitialized();
}

void
FHoudiniEngineEditor::OpenInHoudini()
{
    if ( !FHoudiniEngine::IsInitialized() )
        return;

    // First, saves the current scene as a hip file
    // Creates a proper temporary file name
    FString UserTempPath = FPaths::CreateTempFilename(
        FPlatformProcess::UserTempDir(), 
        TEXT( "HoudiniEngine" ), TEXT( ".hip" ) );

    // Save HIP file through Engine.
    std::string TempPathConverted( TCHAR_TO_UTF8( *UserTempPath ) );
    FHoudiniApi::SaveHIPFile(
        FHoudiniEngine::Get().GetSession(), 
        TempPathConverted.c_str(), false);

    if ( !FPaths::FileExists( UserTempPath ) )
        return;
    
    // Then open the hip file in Houdini
    FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
    FString houdiniLocation = LibHAPILocation + "//houdini";
    FPlatformProcess::CreateProc( 
        houdiniLocation.GetCharArray().GetData(), 
        UserTempPath.GetCharArray().GetData(), 
        true, false, false, 
        nullptr, 0,
        FPlatformProcess::UserTempDir(),
        nullptr, nullptr );

    // Unfortunately, LaunchFileInDefaultExternalApplication doesn't seem to be working properly
    //FPlatformProcess::LaunchFileInDefaultExternalApplication( UserTempPath.GetCharArray().GetData(), nullptr, ELaunchVerb::Open );
}

void
FHoudiniEngineEditor::ReportBug()
{
    FPlatformProcess::LaunchURL( HAPI_UNREAL_BUG_REPORT_URL, nullptr, nullptr );
}

bool
FHoudiniEngineEditor::CanReportBug() const
{
    return FHoudiniEngine::IsInitialized();
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define TTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )
#define OTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

void
FHoudiniEngineEditor::RegisterStyleSet()
{
    // Create Slate style set.
    if ( !StyleSet.IsValid() )
    {
        StyleSet = MakeShareable( new FSlateStyleSet( "HoudiniEngineStyle" ) );
        StyleSet->SetContentRoot( FPaths::EngineContentDir() / TEXT( "Editor/Slate" ) );
        StyleSet->SetCoreContentRoot( FPaths::EngineContentDir() / TEXT( "Slate" ) );

        // Note, these sizes are in Slate Units.
        // Slate Units do NOT have to map to pixels.
        const FVector2D Icon5x16( 5.0f, 16.0f );
        const FVector2D Icon8x4( 8.0f, 4.0f );
        const FVector2D Icon8x8( 8.0f, 8.0f );
        const FVector2D Icon10x10( 10.0f, 10.0f );
        const FVector2D Icon12x12( 12.0f, 12.0f );
        const FVector2D Icon12x16( 12.0f, 16.0f );
        const FVector2D Icon14x14( 14.0f, 14.0f );
        const FVector2D Icon16x16( 16.0f, 16.0f );
        const FVector2D Icon20x20( 20.0f, 20.0f );
        const FVector2D Icon22x22( 22.0f, 22.0f );
        const FVector2D Icon24x24( 24.0f, 24.0f );
        const FVector2D Icon25x25( 25.0f, 25.0f );
        const FVector2D Icon32x32( 32.0f, 32.0f );
        const FVector2D Icon40x40( 40.0f, 40.0f );
        const FVector2D Icon64x64( 64.0f, 64.0f );
        const FVector2D Icon36x24( 36.0f, 24.0f );
        const FVector2D Icon128x128( 128.0f, 128.0f );

        static FString IconsDir = FPaths::EnginePluginsDir() / TEXT( "Runtime/HoudiniEngine/Content/Icons/" );
        StyleSet->Set(
            "HoudiniEngine.HoudiniEngineLogo",
            new FSlateImageBrush( IconsDir + TEXT( "icon_houdini_logo_16.png" ), Icon16x16 ) );
        StyleSet->Set(
            "ClassIcon.HoudiniAssetActor",
            new FSlateImageBrush( IconsDir + TEXT( "icon_houdini_logo_16.png" ), Icon16x16 ) );
        StyleSet->Set(
            "HoudiniEngine.HoudiniEngineLogo40",
            new FSlateImageBrush( IconsDir + TEXT( "icon_houdini_logo_40.png" ), Icon40x40 ) );

        // We need some colors from Editor Style & this is the only way to do this at the moment
        const FSlateColor DefaultForeground = FEditorStyle::GetSlateColor( "DefaultForeground" );
        const FSlateColor InvertedForeground = FEditorStyle::GetSlateColor( "InvertedForeground" );
        const FSlateColor SelectorColor = FEditorStyle::GetSlateColor( "SelectorColor" );
        const FSlateColor SelectionColor = FEditorStyle::GetSlateColor( "SelectionColor" );
        const FSlateColor SelectionColor_Inactive = FEditorStyle::GetSlateColor( "SelectionColor_Inactive" );

        // Normal Text
        FTextBlockStyle NormalText = FTextBlockStyle()
            .SetFont( TTF_CORE_FONT( "Fonts/Roboto-Regular", 9 ) )
            .SetColorAndOpacity( FSlateColor::UseForeground() )
            .SetShadowOffset( FVector2D::ZeroVector )
            .SetShadowColorAndOpacity( FLinearColor::Black )
            .SetHighlightColor( FLinearColor( 0.02f, 0.3f, 0.0f ) )
            .SetHighlightShape( BOX_BRUSH( "Common/TextBlockHighlightShape", FMargin( 3.f / 8.f ) ) );

        StyleSet->Set( "HoudiniEngine.TableRow", FTableRowStyle()
                       .SetEvenRowBackgroundBrush( FSlateNoResource() )
                       .SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, FLinearColor( 1.0f, 1.0f, 1.0f, 0.1f ) ) )
                       .SetOddRowBackgroundBrush( FSlateNoResource() )
                       .SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, FLinearColor( 1.0f, 1.0f, 1.0f, 0.1f ) ) )
                       .SetSelectorFocusedBrush( BORDER_BRUSH( "Common/Selector", FMargin( 4.f / 16.f ), SelectorColor ) )
                       .SetActiveBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) )
                       .SetActiveHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) )
                       .SetInactiveBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
                       .SetInactiveHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
                       .SetTextColor( DefaultForeground )
                       .SetSelectedTextColor( InvertedForeground )
        );

        StyleSet->Set( "HoudiniEngine.ThumbnailShadow", new BOX_BRUSH( "ContentBrowser/ThumbnailShadow", FMargin( 4.0f / 64.0f ) ) );
        StyleSet->Set( "HoudiniEngine.ThumbnailBackground", new IMAGE_BRUSH( "Common/ClassBackground_64x", FVector2D( 64.f, 64.f ), FLinearColor( 0.75f, 0.75f, 0.75f, 1.0f ) ) );
        StyleSet->Set( "HoudiniEngine.ThumbnailText", NormalText );

        // Register Slate style.
        FSlateStyleRegistry::RegisterSlateStyle( *StyleSet.Get() );
    }
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef TTF_CORE_FONT
#undef OTF_FONT
#undef OTF_CORE_FONT

void
FHoudiniEngineEditor::UnregisterStyleSet()
{
    // Unregister Slate style set.
    if ( StyleSet.IsValid() )
    {
        // Unregister Slate style.
        FSlateStyleRegistry::UnRegisterSlateStyle( *StyleSet.Get() );

        ensure( StyleSet.IsUnique() );
        StyleSet.Reset();
    }
}

void
FHoudiniEngineEditor::RegisterForUndo()
{
    if ( GUnrealEd )
        GUnrealEd->RegisterForUndo( this );
}

void
FHoudiniEngineEditor::UnregisterForUndo()
{
    if ( GUnrealEd )
        GUnrealEd->UnregisterForUndo( this );
}

void 
FHoudiniEngineEditor::RegisterModes()
{
#ifdef WANT_PAINT_MODE
    FEditorModeRegistry::Get().RegisterMode<FHoudiniAttributePaintEdMode>( 
        FHoudiniAttributePaintEdMode::EM_HoudiniAttributePaintEdModeId, 
        LOCTEXT( "HoudiniVertexAttributePainting", "Houdini Vertex Attribute Painting" ), 
        FSlateIcon(), true );
#endif
}

void 
FHoudiniEngineEditor::UnregisterModes()
{
#ifdef WANT_PAINT_MODE
    FEditorModeRegistry::Get().UnregisterMode( FHoudiniAttributePaintEdMode::EM_HoudiniAttributePaintEdModeId );
#endif
}

/** Registers placement mode extensions. */
void 
FHoudiniEngineEditor::RegisterPlacementModeExtensions()
{

    // Load custom houdini tools 
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    check( HoudiniRuntimeSettings );

    if ( HoudiniRuntimeSettings->bHidePlacementModeHoudiniTools )
        return;

    // Set up Built-in Houdini Tools
    //
    FString ToolsDir = FPaths::EnginePluginsDir() / TEXT( "Runtime/HoudiniEngine/Content/Tools/" );

    auto ToolArray = HoudiniRuntimeSettings->CustomHoudiniTools;

    ToolArray.Add( FHoudiniTool{
        TEXT( "Rock Generator" ),
        TEXT( "Generates procedural rock meshes" ),
        FFilePath{ ToolsDir / TEXT( "rock_generator_40.png" ) },
        TAssetPtr<UHoudiniAsset>( FStringAssetReference( TEXT( "HoudiniAsset'/HoudiniEngine/Tools/rock_generator.rock_generator'" ) ) ),
        TEXT("http://www.sidefx.com/docs/unreal/")
    } );

    for ( const FHoudiniTool& HoudiniTool : ToolArray )
    {
        FText AssetName = FText::FromString(HoudiniTool.Name);
        FText AssetTip = FText::FromString(HoudiniTool.ToolTip);

        FString IconPath = FPaths::ConvertRelativePathToFull( HoudiniTool.IconPath.FilePath );
        const FSlateBrush* CustomIconBrush = nullptr;
        if ( FPaths::FileExists( IconPath ) )
        {
            FName BrushName = *IconPath;
            CustomIconBrush = new FSlateDynamicImageBrush( BrushName, FVector2D( 40.f, 40.f ) );
        }
        else
        {
            CustomIconBrush = StyleSet->GetBrush( TEXT( "HoudiniEngine.HoudiniEngineLogo40" ) );
        }
        ToolTypes.Add( MakeShareable( new FHoudiniToolType( HoudiniTool.HoudiniAsset, AssetName, AssetTip, CustomIconBrush, HoudiniTool.HelpURL ) ) );
    }

    FPlacementCategoryInfo Info(
        LOCTEXT( "HoudiniCategoryName", "Houdini Engine" ),
        "HoudiniEngine",
        TEXT( "PMHoudiniEngine" ),
        25
    );
    Info.CustomGenerator = []() -> TSharedRef<SWidget> { return SNew( SHoudiniToolPalette ); };

    IPlacementModeModule::Get().RegisterPlacementCategory( Info );
}

void 
FHoudiniEngineEditor::UnregisterPlacementModeExtensions()
{
    if ( IPlacementModeModule::IsAvailable() )
    {
        IPlacementModeModule::Get().UnregisterPlacementCategory( "HoudiniEngine" );
    }
}

TSharedPtr<ISlateStyle>
FHoudiniEngineEditor::GetSlateStyle() const
{
    return StyleSet;
}

void
FHoudiniEngineEditor::RegisterThumbnails()
{
    UThumbnailManager::Get().RegisterCustomRenderer( UHoudiniAsset::StaticClass(), UHoudiniAssetThumbnailRenderer::StaticClass() );
}

void
FHoudiniEngineEditor::UnregisterThumbnails()
{
    if ( UObjectInitialized() )
        UThumbnailManager::Get().UnregisterCustomRenderer( UHoudiniAsset::StaticClass() );
}

bool
FHoudiniEngineEditor::MatchesContext( const FString & InContext, UObject * PrimaryObject ) const
{
    if ( InContext == TEXT( HOUDINI_MODULE_EDITOR ) || InContext == TEXT( HOUDINI_MODULE_RUNTIME ) )
    {
        LastHoudiniAssetComponentUndoObject = Cast< UHoudiniAssetComponent >( PrimaryObject );
        return true;
    }

    LastHoudiniAssetComponentUndoObject = nullptr;
    return false;
}

void
FHoudiniEngineEditor::PostUndo( bool bSuccess )
{
    if ( LastHoudiniAssetComponentUndoObject && bSuccess )
    {
        LastHoudiniAssetComponentUndoObject->UpdateEditorProperties( false );
        LastHoudiniAssetComponentUndoObject = nullptr;
    }
}

void
FHoudiniEngineEditor::PostRedo( bool bSuccess )
{
    if ( LastHoudiniAssetComponentUndoObject && bSuccess )
    {
        LastHoudiniAssetComponentUndoObject->UpdateEditorProperties( false );
        LastHoudiniAssetComponentUndoObject = nullptr;
    }
}

void
FHoudiniEngineEditor::CleanUpTempFolder()
{
    // Get Runtime settings to get the Temp Cook Folder
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (!HoudiniRuntimeSettings)
        return;
    
    FString TempCookFolder = HoudiniRuntimeSettings->TemporaryCookFolder.ToString();

    // The Asset registry will help us finding if the content of the asset is referenced
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    bool bDidDeleteAsset = true;
    while ( bDidDeleteAsset )
    {
        // To correctly clean the temp folder, we need to iterate multiple times, because some of the temp assets
        // might be referenced by other temp assets.. (ie Textures are referenced by Materials)
        // We'll stop looking for assets to delete when no deletion occured.
        bDidDeleteAsset = false;

        // The Object library will list all UObjects found in the TempFolder
        auto ObjectLibrary = UObjectLibrary::CreateLibrary( UObject::StaticClass(), false, true );
        ObjectLibrary->LoadAssetDataFromPath( TempCookFolder );

        // Getting all the found asset in the TEMPO folder
        TArray<FAssetData> AssetDataList;
        ObjectLibrary->GetAssetDataList( AssetDataList );

        // All the assets we're going to delete
        TArray<FAssetData> AssetDataToDelete;

        for ( FAssetData Data : AssetDataList )
        {
            UPackage* CurrentPackage = Data.GetPackage();
            if ( !CurrentPackage )
                continue;

            TArray<FAssetData> AssetsInPackage;
            bool bAssetDataSafeToDelete = true;
            AssetRegistryModule.Get().GetAssetsByPackageName( CurrentPackage->GetFName(), AssetsInPackage );

            for ( const auto& AssetInfo : AssetsInPackage )
            {
                if ( !AssetInfo.GetAsset() )
                    continue;

                // Check and see whether we are referenced by any objects that won't be garbage collected (*including* the undo buffer)
                FReferencerInformationList ReferencesIncludingUndo;
                UObject* AssetInPackage = AssetInfo.GetAsset();
                bool bReferencedInMemoryOrUndoStack = IsReferenced( AssetInPackage, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &ReferencesIncludingUndo );
                if ( !bReferencedInMemoryOrUndoStack )
                    continue;

                // We do have external references, check if the external references are in our ObjectToDelete list
                // If they are, we can delete the asset cause its references are going to be deleted too.
                for ( auto ExtRef : ReferencesIncludingUndo.ExternalReferences )
                {
                    UObject* Outer = ExtRef.Referencer->GetOuter();

                    bool bOuterFound = false;
                    for ( auto DataToDelete : AssetDataToDelete )
                    {
                        if ( DataToDelete.GetPackage() == Outer )
                        {
                            bOuterFound = true;
                            break;
                        }
                        else if ( DataToDelete.GetAsset() == Outer )
                        {
                            bOuterFound = true;
                            break;
                        }
                    }

                    // We have at least one reference that's not going to be deleted, we have to keep the asset
                    if ( !bOuterFound )
                    {
                        bAssetDataSafeToDelete = false;
                        break;
                    }
                }
            }
            
            if ( bAssetDataSafeToDelete )
                AssetDataToDelete.Add( Data );
        }

        // Nothing to delete
        if ( AssetDataToDelete.Num() <= 0 )
            break;

        if ( ObjectTools::DeleteAssets( AssetDataToDelete, false ) > 0 )
            bDidDeleteAsset = true;   
    }
}

bool
FHoudiniEngineEditor::CanCleanUpTempFolder() const
{
    return FHoudiniEngine::IsInitialized();
}

void
FHoudiniEngineEditor::BakeAllAssets()
{
    // Bakes and replaces with blueprints all Houdini Assets in the current level
    for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
        if ( !HoudiniAssetComponent || !HoudiniAssetComponent->IsValidLowLevel() )
        {
            HOUDINI_LOG_ERROR( TEXT( "Failed to export a Houdini Asset in the scene!" ) );
            continue;
        }

        if ( HoudiniAssetComponent->IsTemplate() )
            continue;

        if ( HoudiniAssetComponent->IsPendingKillOrUnreachable() )
            continue;

        bool bInvalidComponent = false;
        if ( !HoudiniAssetComponent->GetOuter() )//|| !HoudiniAssetComponent->GetOuter()->GetLevel() )
            bInvalidComponent = true;

        if ( bInvalidComponent )
        {
            FString AssetName = HoudiniAssetComponent->GetOuter() ? HoudiniAssetComponent->GetOuter()->GetName() : HoudiniAssetComponent->GetName();
            HOUDINI_LOG_ERROR(TEXT("Failed to export Houdini Asset: %s in the scene!"), *AssetName );
            continue;
        }

        // If component is not cooking or instancing, we can bake blueprint.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
            FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithBlueprint( HoudiniAssetComponent );
    }
}

bool
FHoudiniEngineEditor::CanBakeAllAssets() const
{
    if ( !FHoudiniEngine::IsInitialized() )
        return false;
    
    return true;
}

#undef LOCTEXT_NAMESPACE