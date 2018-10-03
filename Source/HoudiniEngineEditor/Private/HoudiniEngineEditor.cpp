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
#include "HoudiniAssetActor.h"
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
#include "HoudiniShelfEdMode.h"
#include "HoudiniEngineBakeUtils.h"

#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/ObjectLibrary.h"
#include "EditorDirectories.h"
#include "Styling/SlateStyleRegistry.h"
#include "SHoudiniToolPalette.h"
#include "IPlacementModeModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistryModule.h"
#include "Engine/Selection.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SlateOptMacros.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define TTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )
#define OTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FHoudiniEngineStyle::StyleSet = nullptr;

TSharedPtr<class ISlateStyle>
FHoudiniEngineStyle::Get()
{
    return StyleSet;
}

FName
FHoudiniEngineStyle::GetStyleSetName()
{
    static FName HoudiniStyleName(TEXT("HoudiniEngineStyle"));
    return HoudiniStyleName;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void
FHoudiniEngineStyle::Initialize()
{
    // Only register the StyleSet once
    if ( StyleSet.IsValid() )
        return;

    StyleSet = MakeShareable( new FSlateStyleSet( GetStyleSetName() ) );
    StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
    StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
    
    // Note, these sizes are in Slate Units.
    // Slate Units do NOT have to map to pixels.
    const FVector2D Icon5x16(5.0f, 16.0f);
    const FVector2D Icon8x4(8.0f, 4.0f);
    const FVector2D Icon8x8(8.0f, 8.0f);
    const FVector2D Icon10x10(10.0f, 10.0f);
    const FVector2D Icon12x12(12.0f, 12.0f);
    const FVector2D Icon12x16(12.0f, 16.0f);
    const FVector2D Icon14x14(14.0f, 14.0f);
    const FVector2D Icon16x16(16.0f, 16.0f);
    const FVector2D Icon20x20(20.0f, 20.0f);
    const FVector2D Icon22x22(22.0f, 22.0f);
    const FVector2D Icon24x24(24.0f, 24.0f);
    const FVector2D Icon25x25(25.0f, 25.0f);
    const FVector2D Icon32x32(32.0f, 32.0f);
    const FVector2D Icon40x40(40.0f, 40.0f);
    const FVector2D Icon64x64(64.0f, 64.0f);
    const FVector2D Icon36x24(36.0f, 24.0f);
    const FVector2D Icon128x128(128.0f, 128.0f);

    static FString IconsDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine/Content/Icons/");
    StyleSet->Set(
        "HoudiniEngine.HoudiniEngineLogo",
        new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set(
        "ClassIcon.HoudiniAssetActor",
        new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set(
        "HoudiniEngine.HoudiniEngineLogo40",
        new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_40.png"), Icon40x40));

    StyleSet->Set(
        "ClassIcon.HoudiniAsset",
        new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

    StyleSet->Set(
        "ClassThumbnail.HoudiniAsset",
        new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_128.png"), Icon64x64));

    //FSlateImageBrush* HoudiniLogo16 = new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16);
    StyleSet->Set("HoudiniEngine.SaveHIPFile", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set("HoudiniEngine.ReportBug", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set("HoudiniEngine.OpenInHoudini", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set("HoudiniEngine.CleanUpTempFolder", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set("HoudiniEngine.BakeAllAssets", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
    StyleSet->Set("HoudiniEngine.PauseAssetCooking", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

    // We need some colors from Editor Style & this is the only way to do this at the moment
    const FSlateColor DefaultForeground = FEditorStyle::GetSlateColor("DefaultForeground");
    const FSlateColor InvertedForeground = FEditorStyle::GetSlateColor("InvertedForeground");
    const FSlateColor SelectorColor = FEditorStyle::GetSlateColor("SelectorColor");
    const FSlateColor SelectionColor = FEditorStyle::GetSlateColor("SelectionColor");
    const FSlateColor SelectionColor_Inactive = FEditorStyle::GetSlateColor("SelectionColor_Inactive");

    const FTableRowStyle &NormalTableRowStyle = FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
    StyleSet->Set(
        "HoudiniEngine.TableRow", FTableRowStyle( NormalTableRowStyle)
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

    // Normal Text
    const FTextBlockStyle& NormalText = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
    StyleSet->Set(
        "HoudiniEngine.ThumbnailText", FTextBlockStyle(NormalText)
        .SetFont(TTF_CORE_FONT("Fonts/Roboto-Regular", 9))
        .SetColorAndOpacity(FSlateColor::UseForeground())
        .SetShadowOffset(FVector2D::ZeroVector)
        .SetShadowColorAndOpacity(FLinearColor::Black)
        .SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
        .SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f / 8.f)))
    );

    StyleSet->Set("HoudiniEngine.ThumbnailShadow", new BOX_BRUSH("ContentBrowser/ThumbnailShadow", FMargin(4.0f / 64.0f)));
    StyleSet->Set("HoudiniEngine.ThumbnailBackground", new IMAGE_BRUSH("Common/ClassBackground_64x", FVector2D(64.f, 64.f), FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));    

    // Register Slate style.
    FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef TTF_CORE_FONT
#undef OTF_FONT
#undef OTF_CORE_FONT

void
FHoudiniEngineStyle::Shutdown()
{
    if (StyleSet.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
        ensure(StyleSet.IsUnique());
        StyleSet.Reset();
    }
}


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
    , CurrentHoudiniToolDirIndex( -1 )
{}

void
FHoudiniEngineEditor::StartupModule()
{
    HOUDINI_LOG_MESSAGE( TEXT( "Starting the Houdini Engine Editor module." ) );
    
    // Create style set.
    FHoudiniEngineStyle::Initialize();

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

    // Register thumbnails.
    //RegisterThumbnails();

    // Extends the file menu.
    ExtendMenu();

    // Extend the World Outliner Menu
    AddLevelViewportMenuExtender();

    // Adds the custom console commands
    RegisterConsoleCommands();

    // Register global undo / redo callbacks.
    RegisterForUndo();

#ifdef HOUDINI_MODE
    // Register editor modes
    RegisterModes();
#endif 

    RegisterPlacementModeExtensions();

    // Store the instance.
    FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;

    HOUDINI_LOG_MESSAGE( TEXT("Houdini Engine Editor module startup complete." ) );
}

void
FHoudiniEngineEditor::ShutdownModule()
{
    HOUDINI_LOG_MESSAGE( TEXT( "Shutting down the Houdini Engine Editor module." ) );

    // Remove the level viewport Menu extender%
    RemoveLevelViewportMenuExtender();

    // Unregister asset type actions.
    UnregisterAssetTypeActions();

    // Unregister asset brokers.
    UnregisterAssetBrokers();

    // Unregister detail presenters.
    UnregisterDetails();

    // Unregister thumbnails.
    //UnregisterThumbnails();

    // Unregister our component visualizers.
    UnregisterComponentVisualizers();

    // Unregister global undo / redo callbacks.
    UnregisterForUndo();

#ifdef HOUDINI_MODE
    UnregisterModes();
#endif

    UnregisterPlacementModeExtensions();

    // Unregister the styleset
    FHoudiniEngineStyle::Shutdown();

    HOUDINI_LOG_MESSAGE( TEXT("Houdini Engine Editor module shutdown complete." ) );
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
        // We need to add/bind the UI Commands to their functions first
        BindMenuCommands();

        // Extend main menu, we will add Houdini section.
        MainMenuExtender = MakeShareable( new FExtender );
        MainMenuExtender->AddMenuExtension(
            "FileLoadAndSave", EExtensionHook::After, HEngineCommands,
            FMenuExtensionDelegate::CreateRaw( this, &FHoudiniEngineEditor::AddHoudiniMenuExtension ) );
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >( "LevelEditor" );
        LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( MainMenuExtender );
    }
}

void
FHoudiniEngineEditor::AddHoudiniMenuExtension( FMenuBuilder & MenuBuilder )
{
    MenuBuilder.BeginSection( "Houdini", LOCTEXT( "HoudiniLabel", "Houdini Engine" ) );

    MenuBuilder.AddMenuEntry( FHoudiniEngineCommands::Get().OpenInHoudini );
    MenuBuilder.AddMenuEntry( FHoudiniEngineCommands::Get().SaveHIPFile );
    MenuBuilder.AddMenuEntry( FHoudiniEngineCommands::Get().ReportBug );
    MenuBuilder.AddMenuEntry( FHoudiniEngineCommands::Get().CleanUpTempFolder );
    MenuBuilder.AddMenuEntry( FHoudiniEngineCommands::Get().BakeAllAssets );
    MenuBuilder.AddMenuEntry( FHoudiniEngineCommands::Get().PauseAssetCooking );

    MenuBuilder.EndSection();
}

void
FHoudiniEngineEditor::BindMenuCommands()
{
    HEngineCommands = MakeShareable( new FUICommandList );

    FHoudiniEngineCommands::Register();
    const FHoudiniEngineCommands& Commands = FHoudiniEngineCommands::Get();

    HEngineCommands->MapAction(
        Commands.OpenInHoudini,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::OpenInHoudini ),
        FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanOpenInHoudini ) );

    HEngineCommands->MapAction(
        Commands.SaveHIPFile,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::SaveHIPFile ),
        FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanSaveHIPFile ) );

    HEngineCommands->MapAction(
        Commands.ReportBug,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::ReportBug ),
        FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanReportBug ) );

    HEngineCommands->MapAction(
        Commands.CleanUpTempFolder,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CleanUpTempFolder ),
        FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanCleanUpTempFolder ) );

    HEngineCommands->MapAction(
        Commands.BakeAllAssets,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::BakeAllAssets ),
        FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanBakeAllAssets ) );

    HEngineCommands->MapAction(
        Commands.PauseAssetCooking,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::PauseAssetCooking ),
        FCanExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::CanPauseAssetCooking ),
        FIsActionChecked::CreateRaw( this, &FHoudiniEngineEditor::IsAssetCookingPaused ) );

    // Non menu command (used for shortcuts only)
    HEngineCommands->MapAction(
        Commands.CookSelec,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::RecookSelection ),
        FCanExecuteAction() );

    HEngineCommands->MapAction(
        Commands.RebuildSelec,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::RebuildSelection ),
        FCanExecuteAction() );

    HEngineCommands->MapAction(
        Commands.BakeSelec,
        FExecuteAction::CreateRaw( this, &FHoudiniEngineEditor::BakeSelection ),
        FCanExecuteAction() );

    // Append the command to the editor module
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor");
    LevelEditorModule.GetGlobalLevelEditorActions()->Append(HEngineCommands.ToSharedRef());
}

void
FHoudiniEngineEditor::RegisterConsoleCommands()
{
    // Register corresponding console commands
    static FAutoConsoleCommand CCmdOpen = FAutoConsoleCommand(
        TEXT("Houdini.Open"),
        TEXT("Open the scene in Houdini."),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::OpenInHoudini ) );

    static FAutoConsoleCommand CCmdSave = FAutoConsoleCommand(
        TEXT("Houdini.Save"),
        TEXT("Save the current Houdini scene to a hip file."),
        FConsoleCommandDelegate::CreateRaw(this, &FHoudiniEngineEditor::SaveHIPFile ) );

    static FAutoConsoleCommand CCmdBake = FAutoConsoleCommand(
        TEXT("Houdini.BakeAll"),
        TEXT("Bakes and replaces with blueprints all Houdini Asset Actors in the current level."),
        FConsoleCommandDelegate::CreateRaw(this, &FHoudiniEngineEditor::BakeAllAssets ) );

    static FAutoConsoleCommand CCmdClean = FAutoConsoleCommand(
        TEXT("Houdini.Clean"),
        TEXT("Cleans up unused/unreferenced Houdini Engine temporary files."),
        FConsoleCommandDelegate::CreateRaw(this, &FHoudiniEngineEditor::CleanUpTempFolder ) );

    static FAutoConsoleCommand CCmdPause = FAutoConsoleCommand(
        TEXT( "Houdini.Pause"),
        TEXT( "Pauses Houdini Engine Asset cooking." ),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::PauseAssetCooking ) );

    // Additional console only commands
    static FAutoConsoleCommand CCmdCookAll = FAutoConsoleCommand(
        TEXT("Houdini.CookAll"),
        TEXT("Re-cooks all Houdini Engine Asset Actors in the current level."),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::RecookAllAssets ) );

    static FAutoConsoleCommand CCmdRebuildAll = FAutoConsoleCommand(
        TEXT("Houdini.RebuildAll"),
        TEXT("Rebuilds all Houdini Engine Asset Actors in the current level."),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::RebuildAllAssets ) );

    static FAutoConsoleCommand CCmdCookSelec = FAutoConsoleCommand(
        TEXT("Houdini.Cook"),
        TEXT("Re-cooks selected Houdini Asset Actors in the current level."),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::RecookSelection ) );

    static FAutoConsoleCommand CCmdRebuildSelec = FAutoConsoleCommand(
        TEXT("Houdini.Rebuild"),
        TEXT("Rebuilds selected Houdini Asset Actors in the current level."),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::RebuildSelection ) );

    static FAutoConsoleCommand CCmdBakeSelec = FAutoConsoleCommand(
        TEXT("Houdini.Bake"),
        TEXT("Bakes and replaces with blueprints selected Houdini Asset Actors in the current level."),
        FConsoleCommandDelegate::CreateRaw( this, &FHoudiniEngineEditor::BakeSelection ) );

    static FAutoConsoleCommand CCmdRestartSession = FAutoConsoleCommand(
        TEXT("Houdini.RestartSession"),
        TEXT("Restart the current Houdini Session."),
        FConsoleCommandDelegate::CreateRaw(this, &FHoudiniEngineEditor::RestartSession ) );
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
            // Add a slate notification
            FString Notification = TEXT("Saving internal Houdini scene...");
            FHoudiniEngineUtils::CreateSlateNotification(Notification);

            // ... and a log message
            HOUDINI_LOG_MESSAGE(TEXT("Saved Houdini scene to %s"), *SaveFilenames[ 0 ] );

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
    
    // Add a slate notification
    FString Notification = TEXT( "Opening scene in Houdini..." );
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE( TEXT("Opened scene in Houdini.") );

        // Add quotes to the path to avoid issues with spaces
        UserTempPath = TEXT("\"") + UserTempPath + TEXT("\"");
    // Then open the hip file in Houdini
    FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
    FString HoudiniLocation = LibHAPILocation + TEXT("//houdini");
    FPlatformProcess::CreateProc( 
        *HoudiniLocation, 
        *UserTempPath, 
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
    FEditorModeRegistry::Get().RegisterMode<FHoudiniShelfEdMode>( 
        FHoudiniShelfEdMode::EM_HoudiniShelfEdModeId, 
        LOCTEXT( "HoudiniMode", "Houdini Tools" ), 
        FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo40" ),
        true );
}

void 
FHoudiniEngineEditor::UnregisterModes()
{
    FEditorModeRegistry::Get().UnregisterMode( FHoudiniShelfEdMode::EM_HoudiniShelfEdModeId );
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

    //
    // Set up Built-in Houdini Tools
    //
    UpdateHoudiniToolList();

    FPlacementCategoryInfo Info(
        LOCTEXT( "HoudiniCategoryName", "Houdini Engine" ),
        "HoudiniEngine",
        TEXT( "PMHoudiniEngine" ),
        25
    );
    Info.CustomGenerator = []() -> TSharedRef<SWidget> { return SNew( SHoudiniToolPalette ); };

    IPlacementModeModule::Get().RegisterPlacementCategory( Info );
}

FString
FHoudiniEngineEditor::GetDefaultHoudiniToolIcon() const
{
        return FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine/Content/Icons/icon_houdini_logo_40.png");
}

void 
FHoudiniEngineEditor::UnregisterPlacementModeExtensions()
{
    if ( IPlacementModeModule::IsAvailable() )
    {
        IPlacementModeModule::Get().UnregisterPlacementCategory( "HoudiniEngine" );
    }

    HoudiniTools.Empty();
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
    // Add a slate notification
    FString Notification = TEXT("Cleaning up Houdini Engine temporary folder");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // Get Runtime settings to get the Temp Cook Folder
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (!HoudiniRuntimeSettings)
        return;
    
    FString TempCookFolder = HoudiniRuntimeSettings->TemporaryCookFolder.ToString();

    // The Asset registry will help us finding if the content of the asset is referenced
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    int32 DeletedCount = 0;
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

        int32 CurrentDeleted = ObjectTools::DeleteAssets( AssetDataToDelete, false );
        if ( CurrentDeleted <= 0 )
        {
            // Normal deletion failed...  Try to force delete the objects?
            TArray<UObject*> ObjectsToDelete;
            for (int i = 0; i < AssetDataToDelete.Num(); i++)
            {
                const FAssetData& AssetData = AssetDataToDelete[i];
                UObject *ObjectToDelete = AssetData.GetAsset();
                // Assets can be loaded even when their underlying type/class no longer exists...
                if (ObjectToDelete != nullptr)
                {
                    ObjectsToDelete.Add(ObjectToDelete);
                }
            }

            CurrentDeleted = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
        }

        if ( CurrentDeleted > 0 )
        {
            DeletedCount += CurrentDeleted;
            bDidDeleteAsset = true;
        }
    }

    // Add a slate notification
    Notification = TEXT("Deleted ") + FString::FromInt( DeletedCount ) + TEXT(" temporary files.");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE( TEXT("Deleted %d temporary files."), DeletedCount );
}

bool
FHoudiniEngineEditor::CanCleanUpTempFolder() const
{
    return FHoudiniEngine::IsInitialized();
}

void
FHoudiniEngineEditor::BakeAllAssets()
{
    // Add a slate notification
    FString Notification = TEXT("Baking all assets in the current level...");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // Bakes and replaces with blueprints all Houdini Assets in the current level
    int32 BakedCount = 0;
    for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
        if ( !HoudiniAssetComponent )
        {
            HOUDINI_LOG_ERROR( TEXT( "Failed to export a Houdini Asset in the scene!" ) );
            continue;
        }

        if ( !HoudiniAssetComponent->IsComponentValid() )
        {
            FString AssetName = HoudiniAssetComponent->GetOuter() ? HoudiniAssetComponent->GetOuter()->GetName() : HoudiniAssetComponent->GetName();
            if ( AssetName != "Default__HoudiniAssetActor" )
                HOUDINI_LOG_ERROR( TEXT( "Failed to export Houdini Asset: %s in the scene!" ), *AssetName );
            continue;
        }

        // If component is not cooking or instancing, we can bake blueprint.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
        {
            if ( FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithBlueprint( HoudiniAssetComponent ) )
                BakedCount++;
        }
    }

    // Add a slate notification
    Notification = TEXT("Baked ") + FString::FromInt( BakedCount ) + TEXT(" Houdini assets.");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE( TEXT("Baked all %d Houdini assets in the current level."), BakedCount );
}

bool
FHoudiniEngineEditor::CanBakeAllAssets() const
{
    if ( !FHoudiniEngine::IsInitialized() )
        return false;
    
    return true;
}

void
FHoudiniEngineEditor::PauseAssetCooking()
{
    // Revert the global flag
    bool CurrentEnableCookingGlobal = !FHoudiniEngine::Get().GetEnableCookingGlobal();
    FHoudiniEngine::Get().SetEnableCookingGlobal( CurrentEnableCookingGlobal );

    // Add a slate notification
    FString Notification = TEXT("Houdini Engine cooking paused");
    if ( CurrentEnableCookingGlobal )
        Notification = TEXT("Houdini Engine cooking resumed");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    if ( CurrentEnableCookingGlobal )
        HOUDINI_LOG_MESSAGE( TEXT("Houdini Engine cooking resumed.") );
    else
        HOUDINI_LOG_MESSAGE( TEXT("Houdini Engine cooking paused.") );

    if ( !CurrentEnableCookingGlobal )
        return;

    // If we are unpausing, tick each asset component to "update" them
    for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
        if (!HoudiniAssetComponent || !HoudiniAssetComponent->IsValidLowLevel())
        {
            HOUDINI_LOG_ERROR( TEXT("Failed to cook a Houdini Asset in the scene!") );
            continue;
        }

        HoudiniAssetComponent->StartHoudiniTicking();
    }
}

bool
FHoudiniEngineEditor::CanPauseAssetCooking()
{
    if ( !FHoudiniEngine::IsInitialized() )
        return false;

    return true;
}

bool
FHoudiniEngineEditor::IsAssetCookingPaused()
{
    return !FHoudiniEngine::Get().GetEnableCookingGlobal();
}

void
FHoudiniEngineEditor::RecookSelection()
{
    // Get current world selection
    TArray<UObject*> WorldSelection;
    int32 SelectedHoudiniAssets = FHoudiniEngineEditor::GetWorldSelection( WorldSelection, true );
    if ( SelectedHoudiniAssets <= 0 )
    {
        HOUDINI_LOG_MESSAGE( TEXT( "No Houdini Assets selected in the world outliner" ) );
        return;
    }

    // Add a slate notification
    FString Notification = TEXT("Cooking selected Houdini Assets...");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // Iterates over the selection and cook the assets if they're in a valid state
    int32 CookedCount = 0;
    for ( int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++ )
    {
        AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>( WorldSelection[ Idx ] );
        if ( !HoudiniAssetActor )
            continue;

        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
        if ( !HoudiniAssetComponent || !HoudiniAssetComponent->IsComponentValid() )
            continue;

        HoudiniAssetComponent->StartTaskAssetCookingManual();
        CookedCount++;
    }

    // Add a slate notification
    Notification = TEXT("Re-cooked ") + FString::FromInt( CookedCount ) + TEXT(" Houdini assets.");
    FHoudiniEngineUtils::CreateSlateNotification(Notification);

    // ... and a log message
    HOUDINI_LOG_MESSAGE( TEXT("Re-cooked %d selected Houdini assets."), CookedCount );
}

void
FHoudiniEngineEditor::RecookAllAssets()
{
    // Add a slate notification
    FString Notification = TEXT("Cooking all assets in the current level...");
    FHoudiniEngineUtils::CreateSlateNotification(Notification);

    // Bakes and replaces with blueprints all Houdini Assets in the current level
    int32 CookedCount = 0;
    for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
        if (!HoudiniAssetComponent || !HoudiniAssetComponent->IsComponentValid())
            continue;

        HoudiniAssetComponent->StartTaskAssetCookingManual();
        CookedCount++;
    }

    // Add a slate notification
    Notification = TEXT("Re-cooked ") + FString::FromInt( CookedCount ) + TEXT(" Houdini assets.");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE(TEXT("Re-cooked %d Houdini assets in the current level."), CookedCount );
}

void
FHoudiniEngineEditor::RebuildAllAssets()
{
    // Add a slate notification
    FString Notification = TEXT("Re-building all assets in the current level...");
    FHoudiniEngineUtils::CreateSlateNotification(Notification);

    // Bakes and replaces with blueprints all Houdini Assets in the current level
    int32 RebuiltCount = 0;
    for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
        if ( !HoudiniAssetComponent || !HoudiniAssetComponent->IsComponentValid() )
            continue;

        HoudiniAssetComponent->StartTaskAssetRebuildManual();
        RebuiltCount++;
    }

    // Add a slate notification
    Notification = TEXT("Rebuilt ") + FString::FromInt( RebuiltCount ) + TEXT(" Houdini assets.");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE(TEXT("Rebuilt %d Houdini assets in the current level."), RebuiltCount );
}

void
FHoudiniEngineEditor::RebuildSelection()
{
    // Get current world selection
    TArray<UObject*> WorldSelection;
    int32 SelectedHoudiniAssets = FHoudiniEngineEditor::GetWorldSelection( WorldSelection, true );
    if ( SelectedHoudiniAssets <= 0 )
    {
        HOUDINI_LOG_MESSAGE(TEXT("No Houdini Assets selected in the world outliner"));
        return;
    }

    // Add a slate notification
    FString Notification = TEXT("Rebuilding selected Houdini Assets...");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // Iterates over the selection and rebuilds the assets if they're in a valid state
    int32 RebuiltCount = 0;
    for ( int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++ )
    {
        AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>( WorldSelection[ Idx ] );
        if ( !HoudiniAssetActor )
            continue;

        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
        if ( !HoudiniAssetComponent || !HoudiniAssetComponent->IsComponentValid() )
            continue;

        HoudiniAssetComponent->StartTaskAssetRebuildManual();
        RebuiltCount++;
    }

    // Add a slate notification
    Notification = TEXT("Rebuilt ") + FString::FromInt( RebuiltCount ) + TEXT(" Houdini assets.");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE( TEXT("Rebuilt %d selected Houdini assets."), RebuiltCount );
}

void
FHoudiniEngineEditor::BakeSelection()
{
    // Get current world selection
    TArray<UObject*> WorldSelection;
    int32 SelectedHoudiniAssets = FHoudiniEngineEditor::GetWorldSelection( WorldSelection, true );
    if ( SelectedHoudiniAssets <= 0 )
    {
        HOUDINI_LOG_MESSAGE( TEXT("No Houdini Assets selected in the world outliner") );
        return;
    }

    // Add a slate notification
    FString Notification = TEXT("Baking selected Houdini Asset Actors in the current level...");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // Iterates over the selection and rebuilds the assets if they're in a valid state
    int32 BakedCount = 0;
    for ( int32 Idx = 0; Idx < SelectedHoudiniAssets; Idx++ )
    {
        AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>( WorldSelection[ Idx ] );
        if ( !HoudiniAssetActor )
            continue;

        UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
        if ( !HoudiniAssetComponent )
        {
            HOUDINI_LOG_ERROR( TEXT("Failed to export a Houdini Asset in the scene!") );
            continue;
        }

        if ( !HoudiniAssetComponent->IsComponentValid() )
        {
            FString AssetName = HoudiniAssetComponent->GetOuter() ? HoudiniAssetComponent->GetOuter()->GetName() : HoudiniAssetComponent->GetName();
            HOUDINI_LOG_ERROR(TEXT("Failed to export Houdini Asset: %s in the scene!"), *AssetName);
            continue;
        }

        // If component is not cooking or instancing, we can bake blueprint.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
        {
            if ( FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithBlueprint( HoudiniAssetComponent ) )
                BakedCount++;
        }
    }

    // Add a slate notification
    Notification = TEXT("Baked ") + FString::FromInt(BakedCount) + TEXT(" Houdini assets.");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // ... and a log message
    HOUDINI_LOG_MESSAGE(TEXT("Baked all %d Houdini assets in the current level."), BakedCount);
}

int32
FHoudiniEngineEditor::GetContentBrowserSelection( TArray< UObject* >& ContentBrowserSelection )
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
FHoudiniEngineEditor::GetWorldSelection( TArray< UObject* >& WorldSelection, bool bHoudiniAssetActorsOnly )
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

    // If we only want Houdini Actors...
    if ( bHoudiniAssetActorsOnly )
    {
        // ... remove all but them
        for ( int32 Idx = WorldSelection.Num() - 1; Idx >= 0; Idx-- )
        {
            AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>( WorldSelection[ Idx ] );
            if ( !HoudiniAssetActor)
                WorldSelection.RemoveAt( Idx );
        }
    }

    return WorldSelection.Num();
}

void
FHoudiniEngineEditor::RestartSession()
{
    // Add a slate notification
    FString Notification = TEXT("Restarting Current Houdini Session");
    FHoudiniEngineUtils::CreateSlateNotification( Notification );

    // Restart the current Houdini Engine Session
    bool bSuccess = FHoudiniEngine::Get().RestartSession();

    // Notify all the HoudiniAssetComponent that they need to reinstantiate themselves in the new session.
    for ( TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = *Itr;
        if ( !HoudiniAssetComponent )
            continue;

        HoudiniAssetComponent->NotifyAssetNeedsToBeReinstantiated();
    }

    // Add a slate notification
    if ( bSuccess )
        Notification = TEXT("Houdini Session successfully restarted.");
    else
        Notification = TEXT("Failed to restart the current Houdini Session.");
    FHoudiniEngineUtils::CreateSlateNotification(Notification);

    // ... and a log message
    //HOUDINI_LOG_MESSAGE( *Notification );
}

void
FHoudiniEngineEditor::AddHoudiniTool( const FHoudiniTool& NewTool )
{
    HoudiniTools.Add( MakeShareable( new FHoudiniTool( NewTool.HoudiniAsset, NewTool.Name, NewTool.Type, NewTool.SelectionType, NewTool.ToolTipText, NewTool.Icon, NewTool.HelpURL, false ) ) );
}

void
FHoudiniEngineEditor::AddLevelViewportMenuExtender()
{
    FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
    auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

    MenuExtenders.Add( FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw( this, &FHoudiniEngineEditor::GetLevelViewportContextMenuExtender ) );
    LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void
FHoudiniEngineEditor::RemoveLevelViewportMenuExtender()
{
    if ( LevelViewportExtenderHandle.IsValid() )
    {
        FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
        if ( LevelEditorModule )
        {
            typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
            LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll(
                [=]( const DelegateType& In ) { return In.GetHandle() == LevelViewportExtenderHandle; } );
        }
    }
}

TSharedRef<FExtender>
FHoudiniEngineEditor::GetLevelViewportContextMenuExtender( const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors )
{
    TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

    // Build an array of the HoudiniAssets corresponding to the selected actors
    TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets;
    TArray< TWeakObjectPtr< AHoudiniAssetActor > > HoudiniAssetActors;
    for ( auto CurrentActor : InActors )
    {
        AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>( CurrentActor );
        if ( !HoudiniAssetActor )
            continue;

        HoudiniAssetActors.Add( HoudiniAssetActor );

        if ( !HoudiniAssetActor->GetHoudiniAssetComponent() )
            continue;

        HoudiniAssets.AddUnique( HoudiniAssetActor->GetHoudiniAssetComponent()->GetHoudiniAsset() );
    }

    if ( HoudiniAssets.Num() > 0 )
    {
        // Add the Asset menu extension
        if ( AssetTypeActions.Num() > 0 )
        {
            // Add the menu extensions via our HoudiniAssetTypeActions
            FHoudiniAssetTypeActions * HATA = static_cast<FHoudiniAssetTypeActions*>( AssetTypeActions[0].Get() );
            if ( HATA )
                Extender = HATA->AddLevelEditorMenuExtenders( HoudiniAssets );
        }
    }

    if ( HoudiniAssetActors.Num() > 0 )
    {
        // Add some actor menu extensions
        FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
        TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();
        Extender->AddMenuExtension(
            "ActorControl",
            EExtensionHook::After,
            LevelEditorCommandBindings,
            FMenuExtensionDelegate::CreateLambda( [ this, HoudiniAssetActors ]( FMenuBuilder& MenuBuilder )
            {
                MenuBuilder.AddMenuEntry(
                    NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Recook", "Recook selection"),
                    NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RecookTooltip", "Forces a recook on the selected Houdini Asset Actors."),
                    FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
                    FUIAction(
                        FExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::RecookSelection ),
                        FCanExecuteAction::CreateLambda([=] { return ( HoudiniAssetActors.Num() > 0 ); })
                    )
                );

                MenuBuilder.AddMenuEntry(
                    NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Rebuild", "Rebuild selection"),
                    NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RebuildTooltip", "Rebuilds selected Houdini Asset Actors in the current level."),
                    FSlateIcon( FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
                    FUIAction(
                        FExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::RebuildSelection ),
                        FCanExecuteAction::CreateLambda([=] { return ( HoudiniAssetActors.Num() > 0 ); })
                    )
                );
            } )
        );
    }

    return Extender;
}

void
FHoudiniEngineEditor::GetHoudiniToolDirectories( TArray<FHoudiniToolDirectory>& HoudiniToolsDirectoryArray ) const
{
    HoudiniToolsDirectoryArray.Empty();

    // Read the default tools from the $HFS/engine/tool folder
    FDirectoryPath DefaultToolPath;
    FString HFSPath = FPaths::GetPath(FHoudiniEngine::Get().GetLibHAPILocation());
    FString DefaultPath = FPaths::Combine(HFSPath, TEXT("engine"));
    DefaultPath = FPaths::Combine(DefaultPath, TEXT("tools"));

    FHoudiniToolDirectory ToolDir;
    ToolDir.Name = TEXT("Default");
    ToolDir.Path.Path = DefaultPath;

    HoudiniToolsDirectoryArray.Add( ToolDir );

    // Append all the custom tools directory from the runtime settings
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( !HoudiniRuntimeSettings )
        return;

    // Add all the custom tool paths
    HoudiniToolsDirectoryArray.Append( HoudiniRuntimeSettings->CustomHoudiniToolsLocation );
}

void
FHoudiniEngineEditor::UpdateHoudiniToolList(const int32 SelectedDir/*=-1*/)
{
    CurrentHoudiniToolDirIndex = SelectedDir;

    // Clean up the existing tool list
    HoudiniTools.Empty();

    // Get All the houdini tool directories
    TArray<FHoudiniToolDirectory> HoudiniToolsDirectoryArray;
    GetHoudiniToolDirectories( HoudiniToolsDirectoryArray );
    if ( CurrentHoudiniToolDirIndex >= 0 )
    {
        // Only keep the selected one
        for ( int32 n = HoudiniToolsDirectoryArray.Num() - 1; n >= 0; n-- )
        {
            if ( n != CurrentHoudiniToolDirIndex )
                HoudiniToolsDirectoryArray.RemoveAt( n );
        }
    }

    // Add the tools for all the directories
    for ( int32 n = 0; n < HoudiniToolsDirectoryArray.Num(); n++ )
    {
        const FHoudiniToolDirectory& CurrentDir = HoudiniToolsDirectoryArray[n];
        bool isDefault = ( (n == 0) && (CurrentHoudiniToolDirIndex <= 0) );
        UpdateHoudiniToolList( CurrentDir, isDefault );
    }
}

void
FHoudiniEngineEditor::UpdateHoudiniToolList(const FHoudiniToolDirectory& HoudiniToolsDirectory, const bool& isDefault)
{
    FString ToolDirPath = HoudiniToolsDirectory.Path.Path;
    if ( ToolDirPath.IsEmpty() )
        return;

    //
    TArray<FHoudiniToolDescription> HoudiniToolDescArray;        

    // List all the json files in the current directory
    TArray<FString> JSONFiles;
    IFileManager::Get().FindFiles(JSONFiles, *ToolDirPath, TEXT(".json"));
    for ( const FString& CurrentJsonFile : JSONFiles )
    {
        FString CurrentJsonFilePath = ToolDirPath / CurrentJsonFile;
        // Parse the JSON to a HoudiniTool
        FHoudiniToolDescription CurrentHoudiniToolDesc;
        if (!GetHoudiniToolDescriptionFromJSON(CurrentJsonFilePath, CurrentHoudiniToolDesc))
            continue;

        HoudiniToolDescArray.Add( CurrentHoudiniToolDesc );
    }

    // We need to either load or create a new HoudiniAsset from the tool's HDA
    FString ToolPath = TEXT("/Game/HoudiniEngine/Tools/");        
    ToolPath = FPaths::Combine(ToolPath, FPaths::MakeValidFileName( HoudiniToolsDirectory.Name ) );
    ToolPath = ObjectTools::SanitizeObjectPath( ToolPath );

    for (const FHoudiniToolDescription& HoudiniTool : HoudiniToolDescArray)
    {
        FText ToolName = FText::FromString(HoudiniTool.Name);
        FText ToolTip = FText::FromString(HoudiniTool.ToolTip);

        FString IconPath = FPaths::ConvertRelativePathToFull(HoudiniTool.IconPath.FilePath);
        const FSlateBrush* CustomIconBrush = nullptr;
        if (FPaths::FileExists(IconPath))
        {
            FName BrushName = *IconPath;
            CustomIconBrush = new FSlateDynamicImageBrush(BrushName, FVector2D(40.f, 40.f));
        }
        else
        {
            CustomIconBrush = FHoudiniEngineStyle::Get()->GetBrush(TEXT("HoudiniEngine.HoudiniEngineLogo40"));
        }

        // Construct the asset's ref
        FString BaseToolName = FPaths::GetBaseFilename(HoudiniTool.AssetPath.FilePath);
        FString ToolAssetRef = TEXT("HoudiniAsset'") + FPaths::Combine(ToolPath, BaseToolName) + TEXT(".") + BaseToolName + TEXT("'");
        TSoftObjectPtr<UHoudiniAsset> HoudiniAsset(ToolAssetRef);

        // See if the HDA needs to be imported in Unreal
        bool NeedsImport = false;
        if ( !HoudiniAsset.IsValid() )
        {
            NeedsImport = true;

            // Try to load the asset
            UHoudiniAsset* LoadedAsset = HoudiniAsset.LoadSynchronous();
            if ( LoadedAsset )
                NeedsImport = !HoudiniAsset.IsValid();
        }

        if ( NeedsImport )
        {
            // Automatically import the HDA and create a uasset for it
            FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
            TArray<FString> FileNames;
            FileNames.Add(HoudiniTool.AssetPath.FilePath);

            UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
            ImportData->bReplaceExisting = true;
            ImportData->bSkipReadOnly = true;
            ImportData->Filenames = FileNames;
            ImportData->DestinationPath = ToolPath;
            TArray<UObject*> AssetArray = AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
            if ( AssetArray.Num() <=  0 )
                continue;

            HoudiniAsset = Cast< UHoudiniAsset >(AssetArray[0]);
            if (!HoudiniAsset)
                continue;

            // Try to save the newly created package
            UPackage* Pckg = Cast<UPackage>( HoudiniAsset->GetOuter() );
            if (Pckg && Pckg->IsDirty() )
            {
                Pckg->FullyLoad();
                UPackage::SavePackage(
                    Pckg, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
                    *FPackageName::LongPackageNameToFilename( Pckg->GetName(), FPackageName::GetAssetPackageExtension() ) );
            }
        }

        HoudiniTools.Add( MakeShareable( new FHoudiniTool(
            HoudiniAsset, ToolName, HoudiniTool.Type, HoudiniTool.SelectionType, ToolTip, CustomIconBrush, HoudiniTool.HelpURL, isDefault ) ) );
    }
}

bool
FHoudiniEngineEditor::GetHoudiniToolDescriptionFromJSON(const FString& JsonFilePath, FHoudiniToolDescription& HoudiniToolDesc)
{
    if ( JsonFilePath.IsEmpty() )
        return false;

    // Read the file as a string
    FString FileContents;
    if ( !FFileHelper::LoadFileToString( FileContents, *JsonFilePath ) )
    {
        HOUDINI_LOG_ERROR( TEXT("Failed to import Houdini Tool .json file: %s"), *JsonFilePath);
        return false;
    }

    // Parse it as JSON
    TSharedPtr<FJsonObject> JSONObject;
    TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FileContents );
    if (!FJsonSerializer::Deserialize(Reader, JSONObject) || !JSONObject.IsValid())
    {
        HOUDINI_LOG_ERROR( TEXT("Invalid json in Houdini Tool .json file: %s"), *JsonFilePath);
        return false;
    }

    // First, check that the tool is compatible with UE4
    bool IsCompatible = true;
    if (JSONObject->HasField(TEXT("target")))
    {
        IsCompatible = false;
        TArray<TSharedPtr<FJsonValue> >TargetArray = JSONObject->GetArrayField("target");
        for (TSharedPtr<FJsonValue> TargetValue : TargetArray)
        {
            if ( !TargetValue.IsValid() )
                continue;

            // Check the target array field contains either "all" or "unreal"
            FString TargetString = TargetValue->AsString();                        
            if ( TargetString.Equals( TEXT("all"), ESearchCase::IgnoreCase )
                    || TargetString.Equals(TEXT("unreal"), ESearchCase::IgnoreCase) )
            {
                IsCompatible = true;
                break;
            }
        }
    }

    if ( !IsCompatible )
    {
        // The tool is not compatible with unreal, skip it
        //HOUDINI_LOG_MESSAGE(TEXT("Skipped Houdini Tool: ") + JsonFilePath);
        return false;
    }

    // Then, we need to make sure the json file has a correponding hda
    // Start by looking at the assetPath field
    FString HDAFilePath = FString();
    if ( JSONObject->HasField( TEXT("assetPath") ) )
    {
        // If the json has the assetPath field, read it from there
        HDAFilePath = JSONObject->GetStringField(TEXT("assetPath"));
        if (!FPaths::FileExists(HDAFilePath))
            HDAFilePath = FString();
    }

    if (HDAFilePath.IsEmpty())
    {
        // Look for an hda file with the same name as the json file
        HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hda"));
        if (!FPaths::FileExists(HDAFilePath))
        {
            // Try .hdalc
            HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdalc"));
            if (!FPaths::FileExists(HDAFilePath))
            {
                // Try .hdanc
                HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdanc"));
                if (!FPaths::FileExists(HDAFilePath))
                    HDAFilePath = FString();
            }
        }
    }

    if ( HDAFilePath.IsEmpty() )
    {
        HOUDINI_LOG_ERROR(TEXT("Could not find the hda for the Houdini Tool .json file: %s"), *JsonFilePath);
        return false;
    }

    // TODO: Handle Houdini Tool with just the HDA path, not an Asset!
    // Create a new asset.
    HoudiniToolDesc.AssetPath = FFilePath{ HDAFilePath };
    //HoudiniToolDesc.HoudiniAsset = NewObject< UHoudiniAsset >(nullptr, FPaths::GetBaseFilename(HDAFilePath), RF_Public | RF_Standalone | RF_Transactional);
    //HoudiniToolDesc.HoudiniAsset->CreateAsset(nullptr, nullptr, HDAFilePath);

    // Read the tool name
    HoudiniToolDesc.Name = FPaths::GetBaseFilename(JsonFilePath);
    if ( JSONObject->HasField(TEXT("name") ) )
        HoudiniToolDesc.Name = JSONObject->GetStringField(TEXT("name"));

    // Read the tool type
    HoudiniToolDesc.Type = EHoudiniToolType::HTOOLTYPE_GENERATOR;
    if ( JSONObject->HasField( TEXT("toolType") ) )
    {
        FString ToolType = JSONObject->GetStringField(TEXT("toolType"));
        if ( ToolType.Equals( TEXT("GENERATOR"), ESearchCase::IgnoreCase ) )
            HoudiniToolDesc.Type = EHoudiniToolType::HTOOLTYPE_GENERATOR;
        else if (ToolType.Equals( TEXT("OPERATOR_SINGLE"), ESearchCase::IgnoreCase ) )
            HoudiniToolDesc.Type = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
        else if (ToolType.Equals(TEXT("OPERATOR_MULTI"), ESearchCase::IgnoreCase))
            HoudiniToolDesc.Type = EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI;
        else if (ToolType.Equals(TEXT("BATCH"), ESearchCase::IgnoreCase))
            HoudiniToolDesc.Type = EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH;
    }

    // Read the tooltip
    HoudiniToolDesc.ToolTip = FString();
    if ( JSONObject->HasField( TEXT("toolTip") ) )
    {
        HoudiniToolDesc.ToolTip = JSONObject->GetStringField(TEXT("toolTip"));
    }

    // Read the help url
    HoudiniToolDesc.HelpURL = FString();
    if (JSONObject->HasField(TEXT("helpURL")))
    {
        HoudiniToolDesc.HelpURL = JSONObject->GetStringField(TEXT("helpURL"));
    }

    // Read the icon path
    FString IconPath = FString();
    if (JSONObject->HasField(TEXT("iconPath")))
    {
        // If the json has the iconPath field, read it from there
        IconPath = JSONObject->GetStringField(TEXT("iconPath"));
        if (!FPaths::FileExists(IconPath))
            IconPath = FString();
    }

    if (IconPath.IsEmpty())
    {
        // Look for a png file with the same name as the json file
        IconPath = JsonFilePath.Replace(TEXT(".json"), TEXT(".png"));
        if (!FPaths::FileExists(IconPath))
        {
            IconPath = GetDefaultHoudiniToolIcon();
        }
    }

    HoudiniToolDesc.IconPath = FFilePath{ IconPath };

    // TODO: Handle Tags in the JSON?

    return true;
}

bool
FHoudiniEngineEditor::WriteJSONFromHoudiniTool(const FHoudiniTool& Tool)
{
    // Start by building a JSON object from the tool
    TSharedPtr<FJsonObject> JSONObject;
    JSONObject->SetStringField(TEXT("target"), TEXT("unreal"));

    if ( Tool.AssetPath.IsEmpty() )
        JSONObject->SetStringField(TEXT("assetPath"), Tool.AssetPath);

    if ( Tool.Name.IsEmpty() )
        JSONObject->SetStringField(TEXT("name"), Tool.Name.ToString());

    // Tooltype
    FString ToolType = TEXT("GENERATOR");
    if ( Tool.Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE)
        ToolType = TEXT("OPERATOR_SINGLE");
    else if (Tool.Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI)
        ToolType = TEXT("OPERATOR_MULTI");
    else if ( Tool.Type == EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH)
        ToolType = TEXT("BATCH");

    JSONObject->SetStringField(TEXT("toolType"), ToolType);

    // Tooltip
    if ( !Tool.ToolTipText.IsEmpty() )
        JSONObject->SetStringField(TEXT("toolTip"), Tool.ToolTipText.ToString());

    // Help URL
    if ( !Tool.HelpURL.IsEmpty() )
        JSONObject->SetStringField(TEXT("helpURL"), Tool.HelpURL);

    // IconPath
    if ( Tool.Icon )
    {
        FString IconPath = Tool.Icon->GetResourceName().ToString();
        JSONObject->SetStringField(TEXT("iconPath"), IconPath);
    }

    return true;
}

bool
FHoudiniEngineEditor::FindHoudiniTool( const FHoudiniTool& Tool, int32& FoundIndex, bool& IsDefault )
{
    // Return -1 if we cant find the Tool
    FoundIndex = -1;

    for ( int32 Idx = 0; Idx < HoudiniTools.Num(); Idx++ )
    {
        TSharedPtr<FHoudiniTool> Current = HoudiniTools[ Idx ];
        if ( !Current.IsValid() )
            continue;

        if ( Current->HoudiniAsset != Tool.HoudiniAsset )
            continue;

        if ( Current->Type != Tool.Type )
            continue;

        // We found the Houdini Tool
        IsDefault = Current->DefaultTool;
        FoundIndex = Idx;

        return true;
    }

    return false;
}

bool
FHoudiniEngineEditor::FindHoudiniToolInHoudiniSettings( const FHoudiniTool& Tool, int32& FoundIndex )
{
    // Return -1 if we cant find the Tool
    FoundIndex = -1;

    // Remove the tool from the runtime settings
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( !HoudiniRuntimeSettings )
        return false;

    // TODO: FIX ME!
    /*
    for ( int32 Idx = 0; Idx < HoudiniRuntimeSettings->CustomHoudiniTools.Num(); Idx++ )
    {
        FHoudiniToolDescription CurrentDesc = HoudiniRuntimeSettings->CustomHoudiniTools[ Idx ];

        if ( CurrentDesc.HoudiniAsset != Tool.HoudiniAsset )
            continue;

        if ( CurrentDesc.Type != Tool.Type )
            continue;

        FoundIndex = Idx;
        return true;
    }
    */

    return false;
}

void 
FHoudiniEngineCommands::RegisterCommands()
{  
    UI_COMMAND( OpenInHoudini, "Open scene in Houdini", "Opens the current Houdini scene in Houdini.", EUserInterfaceActionType::Button, FInputChord( EKeys::O, EModifierKey::Control | EModifierKey::Alt) );

    UI_COMMAND( SaveHIPFile, "Save Houdini scene (HIP)", "Saves a .hip file of the current Houdini scene.", EUserInterfaceActionType::Button, FInputChord() );
    UI_COMMAND( ReportBug, "Report a plugin bug", "Report a bug for Houdini Engine plugin.", EUserInterfaceActionType::Button, FInputChord() );
   
    UI_COMMAND( CleanUpTempFolder, "Clean Houdini Engine Temp Folder", "Deletes the unused temporary files in the Temporary Cook Folder.", EUserInterfaceActionType::Button, FInputChord() );
    UI_COMMAND( BakeAllAssets, "Bake And Replace All Houdini Assets", "Bakes and replaces with blueprints all Houdini Assets in the scene.", EUserInterfaceActionType::Button, FInputChord() );
    UI_COMMAND( PauseAssetCooking, "Pause Houdini Engine Cooking", "When activated, prevents Houdini Engine from cooking assets until unpaused.", EUserInterfaceActionType::Check, FInputChord( EKeys::P, EModifierKey::Control | EModifierKey::Alt ) );

    UI_COMMAND( CookSelec, "Recook Selection", "Recooks selected Houdini Asset Actors in the current level.", EUserInterfaceActionType::Button, FInputChord( EKeys::C, EModifierKey::Control | EModifierKey::Alt ) );
    UI_COMMAND( RebuildSelec, "Rebuild Selection", "Rebuilds selected Houdini Asset Actors in the current level.", EUserInterfaceActionType::Button, FInputChord( EKeys::R, EModifierKey::Control | EModifierKey::Alt ) );
    UI_COMMAND( BakeSelec, "Bake Selection", "Bakes and replaces with blueprints selected Houdini Asset Actors in the current level.", EUserInterfaceActionType::Button, FInputChord( EKeys::B, EModifierKey::Control | EModifierKey::Alt ) );
}

#undef LOCTEXT_NAMESPACE