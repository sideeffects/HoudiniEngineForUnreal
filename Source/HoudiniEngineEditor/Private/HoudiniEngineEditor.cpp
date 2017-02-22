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
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
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

#include "Textures/SlateIcon.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/ObjectLibrary.h"
#include "EditorDirectories.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Styling/SlateStyleRegistry.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "Internationalization.h"
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

    // Load Houdini Engine Runtime module.
    //IHoudiniEngine & HoudiniEngine = FModuleManager::LoadModuleChecked< FHoudiniEngine >( "HoudiniEngine" ).Get();

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

    // Register editor modes
    RegisterModes();

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

void
FHoudiniEngineEditor::RegisterStyleSet()
{
    // Create Slate style set.
    if ( !StyleSet.IsValid() )
    {
        StyleSet = MakeShareable( new FSlateStyleSet( "HoudiniEngineStyle" ) );
        StyleSet->SetContentRoot( FPaths::EngineContentDir() / TEXT( "Editor/Slate" ) );
        StyleSet->SetCoreContentRoot( FPaths::EngineContentDir() / TEXT( "Slate" ) );

        const FVector2D Icon16x16( 16.0f, 16.0f );
        static FString ContentDir = FPaths::EnginePluginsDir() / TEXT( "Runtime/HoudiniEngine/Content/Icons/" );
        StyleSet->Set(
            "HoudiniEngine.HoudiniEngineLogo",
            new FSlateImageBrush( ContentDir + TEXT( "icon_houdini_logo_16.png" ), Icon16x16 ) );
        StyleSet->Set(
            "ClassIcon.HoudiniAssetActor",
            new FSlateImageBrush( ContentDir + TEXT( "icon_houdini_logo_16.png" ), Icon16x16 ) );

        // Register Slate style.
        FSlateStyleRegistry::RegisterSlateStyle( *StyleSet.Get() );
    }
}

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

#undef LOCTEXT_NAMESPACE
