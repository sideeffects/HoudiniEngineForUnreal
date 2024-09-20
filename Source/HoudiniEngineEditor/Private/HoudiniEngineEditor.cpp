/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HoudiniEngineEditor.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniEditorNodeSyncSubsystem.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniHandleComponentVisualizer.h"
#include "HoudiniInput.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "HoudiniParameter.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniRuntimeSettingsDetails.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniToolsEditor.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolsRuntimeUtils.h"
#include "SHoudiniToolsPanel.h"
#include "SHoudiniNodeSyncPanel.h"
#include "UnrealMeshTranslator.h"

#include "AssetTypeActions_HoudiniAsset.h"
#include "AssetTypeActions_HoudiniPreset.h"
#include "AssetTypeActions_HoudiniToolsPackageAsset.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorScriptingHelpers.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniPresetActorFactory.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/ConsoleManager.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "Subsystems/PlacementSubsystem.h"
#endif
#include "Templates/SharedPointer.h"
#include "UnrealEdGlobals.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_EDITOR
	#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
	#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

IMPLEMENT_MODULE(FHoudiniEngineEditor, HoudiniEngineEditor);
DEFINE_LOG_CATEGORY(LogHoudiniEngineEditor);


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
{
}

void FHoudiniEngineEditor::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine Editor module."));

	// Create style set.
	FHoudiniEngineStyle::Initialize();

	// Create HoudiniTools
	
	HoudiniToolsPtr = MakeShareable<FHoudiniToolsEditor>(new FHoudiniToolsEditor);

	// Initilizes various resources used by our editor UI widgets
	InitializeWidgetResource();

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

	// Extends the file menu.
	ExtendMenu();

	// Extend the World Outliner Menu
	AddLevelViewportMenuExtender();

	//Extend the right click context menu
	ExtendContextMenu();

	// Adds the custom console commands
	RegisterConsoleCommands();

	// Register global undo / redo callbacks.
	//RegisterForUndo();

	RegisterEditorTabs();

	// Register sections (filters) for the details category
	RegisterSectionMappings();

	/*
	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		RegisterEditorTabs();
	}
	else
	{
		ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FHoudiniEngineEditor::ModulesChangedCallback);
	}
	*/

	// RegisterPlacementModeExtensions();

	// Register for any FEditorDelegates that we are interested in, such as
	// PreSaveWorld and PreBeginPIE, for HoudiniStaticMesh -> UStaticMesh builds
	RegisterEditorDelegates();

	// Store the instance.
	FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Editor module startup complete."));
}

/*
//------------------------------------------------------------------------------
void FHoudiniEngineEditor::ModulesChangedCallback(FName ModuleName, EModuleChangeReason ReasonForChange)
{
	if (ReasonForChange == EModuleChangeReason::ModuleLoaded && ModuleName == TEXT("LevelEditor"))
	{
		RegisterEditorTabs();
	}
}
*/

void FHoudiniEngineEditor::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine Editor module."));

	if (HoudiniToolsPtr.IsValid())
	{
		HoudiniToolsPtr->Shutdown();
	}

	// Unregister the sections (filters) for the details category
	UnregisterSectionMappings();

	// Deregister editor delegates
	UnregisterEditorDelegates();

	// Deregister console commands
	UnregisterConsoleCommands();

	// Remove the level viewport Menu extender
	RemoveLevelViewportMenuExtender();

	// Unregister asset type actions.
	UnregisterAssetTypeActions();

	// Unregister asset brokers.
	//UnregisterAssetBrokers();

	// Unregister detail presenters.
	UnregisterDetails();

	UnRegisterEditorTabs();

	// Unregister our component visualizers.
	//UnregisterComponentVisualizers();

	// Unregister global undo / redo callbacks.
	//UnregisterForUndo();

	//UnregisterPlacementModeExtensions();

	// Unregister settings.
	ISettingsModule * SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
		SettingsModule->UnregisterSettings("Editor", "Plugins", "HoudiniEngine");

	HoudiniToolsPtr.Reset();

	// Unregister the styleset
	FHoudiniEngineStyle::Shutdown();

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Editor module shutdown complete."));
}

FString
FHoudiniEngineEditor::GetHoudiniEnginePluginDir()
{
	FString EnginePluginDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(EnginePluginDir))
		return EnginePluginDir;

	FString ProjectPluginDir = FPaths::ProjectPluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(ProjectPluginDir))
		return ProjectPluginDir;

	TSharedPtr<IPlugin> HoudiniPlugin = IPluginManager::Get().FindPlugin(TEXT("HoudiniEngine"));
	FString PluginBaseDir = HoudiniPlugin.IsValid() ? HoudiniPlugin->GetBaseDir() : EnginePluginDir;
	if (FPaths::DirectoryExists(PluginBaseDir))
		return PluginBaseDir;

	HOUDINI_LOG_WARNING(TEXT("Could not find the Houdini Engine plugin's directory"));

	return EnginePluginDir;
}

void
FHoudiniEngineEditor::RegisterDetails()
{
	FPropertyEditorModule & PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >("PropertyEditor");

	// Register details presenter for our component type and runtime settings.
	PropertyModule.RegisterCustomClassLayout(
		TEXT("HoudiniAssetComponent"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniAssetComponentDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(
		TEXT("HoudiniRuntimeSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniRuntimeSettingsDetails::MakeInstance));
}

void
FHoudiniEngineEditor::UnregisterDetails()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule & PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniAssetComponent"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniRuntimeSettings"));
	}
}

void
FHoudiniEngineEditor::RegisterComponentVisualizers()
{
	if (GUnrealEd) 
	{
		// Register Houdini spline visualizer
		SplineComponentVisualizer = MakeShareable<FComponentVisualizer>(new FHoudiniSplineComponentVisualizer);
		if (SplineComponentVisualizer.IsValid()) 
		{
			GUnrealEd->RegisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName(), SplineComponentVisualizer);
			SplineComponentVisualizer->OnRegister();
		}

		// Register Houdini handle visualizer
		HandleComponentVisualizer = MakeShareable<FComponentVisualizer>(new FHoudiniHandleComponentVisualizer);
		if (HandleComponentVisualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(UHoudiniHandleComponent::StaticClass()->GetFName(), HandleComponentVisualizer);
			HandleComponentVisualizer->OnRegister();
		}
	}
}

void
FHoudiniEngineEditor::UnregisterComponentVisualizers()
{
	if (GUnrealEd) 
	{
		// Unregister Houdini spline visualizer
		if(SplineComponentVisualizer.IsValid())
			GUnrealEd->UnregisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());

		// Unregister Houdini handle visualizer
		if (HandleComponentVisualizer.IsValid())
			GUnrealEd->UnregisterComponentVisualizer(UHoudiniHandleComponent::StaticClass()->GetFName());
	}
}

void
FHoudiniEngineEditor::RegisterAssetTypeAction(IAssetTools & AssetTools, TSharedRef< IAssetTypeActions > Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}

void
FHoudiniEngineEditor::RegisterAssetTypeActions()
{
	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked< FAssetToolsModule >("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_HoudiniAsset()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_HoudiniToolsPackageAsset()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_HoudiniPreset()));
}

void
FHoudiniEngineEditor::UnregisterAssetTypeActions()
{
	// Unregister asset type actions we have previously registered.
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools & AssetTools = FModuleManager::GetModuleChecked< FAssetToolsModule >("AssetTools").Get();

		for (int32 Index = 0; Index < AssetTypeActions.Num(); ++Index)
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions[Index].ToSharedRef());

		AssetTypeActions.Empty();
	}
}

void
FHoudiniEngineEditor::RegisterAssetBrokers()
{
	// Create and register broker for Houdini asset.
	HoudiniAssetBroker = MakeShareable(new FHoudiniAssetBroker());
	FComponentAssetBrokerage::RegisterBroker( HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true );
}

void
FHoudiniEngineEditor::UnregisterAssetBrokers()
{
	if (UObjectInitialized())
	{
		// Unregister broker.
		FComponentAssetBrokerage::UnregisterBroker( HoudiniAssetBroker );
	}
}

void
FHoudiniEngineEditor::RegisterActorFactories()
{
	if (GEditor)
	{
		UHoudiniAssetActorFactory* HoudiniAssetActorFactory =
			NewObject< UHoudiniAssetActorFactory >(GetTransientPackage(), UHoudiniAssetActorFactory::StaticClass());
		UHoudiniPresetActorFactory* HoudiniPresetActorFactory =
			NewObject< UHoudiniPresetActorFactory >(GetTransientPackage(), UHoudiniPresetActorFactory::StaticClass());

		GEditor->ActorFactories.Add(HoudiniAssetActorFactory);
		GEditor->ActorFactories.Add(HoudiniPresetActorFactory);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
		{
			PlacementSubsystem->RegisterAssetFactory(HoudiniAssetActorFactory);
			PlacementSubsystem->RegisterAssetFactory(HoudiniPresetActorFactory);
		}
#endif
	}
}


void
FHoudiniEngineEditor::RegisterEditorTabs()
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );

	// If we have a valid LevelEditor tab manager, register now, just in case the tab manager is already active.
	// Not sure whether this case ever occurs, but if it does it may cause issues with RegisterLayoutExtension events.
	const TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
	if (LevelEditorTabManager.IsValid())
	{
		RegisterLevelEditorTabs(LevelEditorTabManager);
	}
	
	// Be sure to also register during OnRegisterTabs() events, since it will be called whenever the LevelEditor tab manager changes.
	OnLevelEditorRegisterTabsHandle = LevelEditorModule.OnRegisterTabs().AddRaw(this, &FHoudiniEngineEditor::RegisterLevelEditorTabs);
	
	//FGlobalTabmanager::Get()->RegisterTabSpawner(NodeSyncTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnNodeSyncTab))
	//	.SetDisplayName(LOCTEXT("FNodeSyncTitleTitle", "Houdini Node Sync"))
	//	.SetTooltipText(LOCTEXT("FNodeSyncTitleTitleTooltip", "Houdini Node Sync"))
	//	.SetMenuType(ETabSpawnerMenuType::Hidden)
	//	.SetGroup(MenuStructure.GetLevelEditorCategory());

	// FGlobalTabmanager::Get()->RegisterTabSpawner(HoudiniToolsTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnHoudiniToolsTab))
	// 	.SetDisplayName(LOCTEXT("FHoudiniToolsTitle", "Houdini Tools"))
	// 	.SetTooltipText(LOCTEXT("FHoudiniToolsTitleTooltip", "A shelf containing Houdini Digital Assets"))
	// 	.SetMenuType(ETabSpawnerMenuType::Hidden)
	// 	.SetGroup(MenuStructure.GetLevelEditorCategory());
}

void
FHoudiniEngineEditor::UnRegisterEditorTabs()
{
	//FGlobalTabmanager::Get()->UnregisterTabSpawner(NodeSyncTabName);

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	const TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
	if (LevelEditorTabManager.IsValid())
	{
		LevelEditorTabManager->UnregisterTabSpawner(HoudiniToolsTabName);
		LevelEditorTabManager->UnregisterTabSpawner(NodeSyncTabName);
	}
	LevelEditorModule.OnRegisterTabs().Remove(OnLevelEditorRegisterTabsHandle);
}

void
FHoudiniEngineEditor::RegisterLevelEditorTabs(TSharedPtr<FTabManager> LevelTabManager)
{
	if (!LevelTabManager.IsValid())
	{
		return;
	}
	
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	LevelTabManager->RegisterTabSpawner(HoudiniToolsTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnHoudiniToolsTab))
		.SetDisplayName(LOCTEXT("FHoudiniToolsTitle", "Houdini Tools"))
		.SetTooltipText(LOCTEXT("FHoudiniToolsTitleTooltip", "A shelf containing Houdini Digital Assets"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetGroup(MenuStructure.GetLevelEditorCategory());

	LevelTabManager->RegisterTabSpawner(NodeSyncTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnNodeSyncTab))
		.SetDisplayName(LOCTEXT("FNodeSyncTitleTitle", "Houdini Node Sync"))
		.SetTooltipText(LOCTEXT("FNodeSyncTitleTitleTooltip", "Houdini Node Sync"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetGroup(MenuStructure.GetLevelEditorCategory());
}

void
FHoudiniEngineEditor::BindMenuCommands()
{
	HEngineCommands = MakeShareable(new FUICommandList);

	FHoudiniEngineCommands::Register();
	const FHoudiniEngineCommands& Commands = FHoudiniEngineCommands::Get();
	
	// Session 

	HEngineCommands->MapAction(
		Commands._CreateSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::CreateSession(); }),
		FCanExecuteAction::CreateLambda([]() { return !FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._ConnectSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ConnectSession(); }),
		FCanExecuteAction::CreateLambda([]() { return !FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._StopSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::StopSession(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._RestartSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RestartSession(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));
	
	HEngineCommands->MapAction(
		Commands._OpenSessionSync,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OpenSessionSync(); }),
		FCanExecuteAction::CreateLambda([]() { return !FHoudiniEngineCommands::IsSessionSyncProcessValid(); }));

	HEngineCommands->MapAction(
		Commands._CloseSessionSync,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::CloseSessionSync(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionSyncProcessValid(); }));

	HEngineCommands->MapAction(
		Commands._ViewportSyncNone,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::SetViewportSync(0); }),
		FCanExecuteAction::CreateLambda([]() { return true; }),
		FIsActionChecked::CreateLambda([]() { return (FHoudiniEngineCommands::GetViewportSync() == 0); })
	);

	HEngineCommands->MapAction(
		Commands._ViewportSyncHoudini,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::SetViewportSync(1); }),
		FCanExecuteAction::CreateLambda([]() { return true; }),
		FIsActionChecked::CreateLambda([]() { return (FHoudiniEngineCommands::GetViewportSync() == 1); })
	);

	HEngineCommands->MapAction(
		Commands._ViewportSyncUnreal,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::SetViewportSync(2); }),
		FCanExecuteAction::CreateLambda([]() { return true; }),
		FIsActionChecked::CreateLambda([]() { return (FHoudiniEngineCommands::GetViewportSync() == 2); })
	);

	HEngineCommands->MapAction(
		Commands._ViewportSyncBoth,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::SetViewportSync(3); }),
		FCanExecuteAction::CreateLambda([]() { return true; }),
		FIsActionChecked::CreateLambda([]() { return (FHoudiniEngineCommands::GetViewportSync() == 3); })
	);

	HEngineCommands->MapAction(
		Commands._OpenNodeSync,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OpenNodeSync(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._OpenHoudiniTools,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OpenHoudiniToolsTab(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// PDG commandlet
	HEngineCommands->MapAction(
		Commands._IsPDGCommandletEnabled,
		FExecuteAction::CreateLambda([]() { FHoudiniEngineCommands::SetPDGCommandletEnabled(!FHoudiniEngineCommands::IsPDGCommandletEnabled()); }),
		FCanExecuteAction::CreateLambda([]() { return true; }),
		FIsActionChecked::CreateLambda([]() { return FHoudiniEngineCommands::IsPDGCommandletEnabled(); })
	);

	HEngineCommands->MapAction(
		Commands._StartPDGCommandlet,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::StartPDGCommandlet(); }),
		FCanExecuteAction::CreateLambda([]()
		{
			return FHoudiniEngineCommands::IsPDGCommandletEnabled() && !FHoudiniEngineCommands::IsPDGCommandletRunningOrConnected();
		})
	);

	HEngineCommands->MapAction(
		Commands._StopPDGCommandlet,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::StopPDGCommandlet(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsPDGCommandletRunningOrConnected(); }));

	// Plugin

	HEngineCommands->MapAction(
		Commands._InstallInfo,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ShowInstallInfo(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._PluginSettings,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ShowPluginSettings(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._PluginEditorSettings,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ShowPluginEditorSettings(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Files

	HEngineCommands->MapAction(
		Commands._OpenInHoudini,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::OpenInHoudini(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._SaveHIPFile,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::SaveHIPFile(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._CleanUpTempFolder,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::CleanUpTempFolder(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Help and support
	HEngineCommands->MapAction(
		Commands._ContentExampleGit,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OpenContentExampleGit(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._ContentExampleBrowseTo,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::BrowseToContentExamples(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::HasContentExamples(); }));

	HEngineCommands->MapAction(
		Commands._ReportBug,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::ReportBug(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._OnlineDoc,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OnlineDocumentation(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._OnlineForum,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OnlineForum(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Actions

	HEngineCommands->MapAction(
		Commands._CookAll,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecookAllAssets(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._CookSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecookSelection(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._RebuildAll,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RebuildAllAssets(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._RebuildSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RebuildSelection(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._BakeAll,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::BakeAllAssets(); }),
		FCanExecuteAction::CreateLambda([](){ return true; }));

	HEngineCommands->MapAction(
		Commands._BakeSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::BakeSelection(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._RefineAll,
		FExecuteAction::CreateLambda([]() { FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(false); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._RefineSelected,
		FExecuteAction::CreateLambda([]() { FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(true); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._PauseAssetCooking,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::PauseAssetCooking(); }),
		FCanExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::IsSessionValid(); }),
		FIsActionChecked::CreateLambda([](){ return FHoudiniEngineCommands::IsAssetCookingPaused(); }));

	// Non menu command (used for shortcuts only)

	// Append the command to the editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor");
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(HEngineCommands.ToSharedRef());
}

void
FHoudiniEngineEditor::ExtendMenu()
{
	if (IsRunningCommandlet())
		return;

	// We need to add/bind the UI Commands to their functions first
	BindMenuCommands();

	MainMenuExtender = MakeShareable(new FExtender);

	// Extend File menu, we will add Houdini section.
	MainMenuExtender->AddMenuExtension(
		"FileLoadAndSave", 
		EExtensionHook::After,
		HEngineCommands,
		FMenuExtensionDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniFileMenuExtension));
		
	MainMenuExtender->AddMenuBarExtension(
		"Edit",
		EExtensionHook::After,
		HEngineCommands,
		FMenuBarExtensionDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniEditorMenu));

	// Add our menu extender
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
}

void
FHoudiniEngineEditor::AddHoudiniFileMenuExtension(FMenuBuilder & MenuBuilder)
{
	MenuBuilder.BeginSection("Houdini", LOCTEXT("HoudiniLabel", "Houdini Engine"));

	// Icons used by the commands are defined in the HoudiniEngineStyle
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenInHoudini);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._SaveHIPFile);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	
	MenuBuilder.EndSection();
}

void
FHoudiniEngineEditor::AddHoudiniEditorMenu(FMenuBarBuilder& MenuBarBuilder)
{
	// View
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("HoudiniLabel", "Houdini Engine"),
		LOCTEXT("HoudiniMenu_ToolTip", "Open the Houdini Engine menu"),
		FNewMenuDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniMainMenuExtension),
		"View");
}

void
FHoudiniEngineEditor::AddHoudiniMainMenuExtension(FMenuBuilder & MenuBuilder)
{
	MenuBuilder.BeginSection("Session", LOCTEXT("SessionLabel", "Session"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CreateSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ConnectSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._StopSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RestartSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenSessionSync);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CloseSessionSync);

	// Viewport sync menu
	struct FLocalMenuBuilder
	{
		static void FillViewportSyncMenu(FMenuBuilder& InSubMenuBuilder)
		{
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ViewportSyncNone);
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ViewportSyncHoudini); 
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ViewportSyncUnreal);
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ViewportSyncBoth);	
		}
	};

	MenuBuilder.AddSubMenu(
		LOCTEXT("SyncViewport", "Sync Viewport"),
		LOCTEXT("SyncViewport_ToolTip", "Sync Viewport"),
		FNewMenuDelegate::CreateStatic(&FLocalMenuBuilder::FillViewportSyncMenu),
		false,
		FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine._SyncViewport"));
	
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenNodeSync);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenHoudiniTools);

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("PDG", LOCTEXT("PDGLabel", "PDG"));
	struct FLocalPDGMenuBuilder
	{
		static void FillPDGMenu(FMenuBuilder& InSubMenuBuilder)
		{
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._IsPDGCommandletEnabled);
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._StartPDGCommandlet);
			InSubMenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._StopPDGCommandlet);
		}
	};	
	MenuBuilder.AddSubMenu(
		LOCTEXT("PDGSubMenu", "Work Item Import Settings"),
		LOCTEXT("PDGSubmenu_ToolTip", "PDG Work Item Import Settings"),
		FNewMenuDelegate::CreateStatic(&FLocalPDGMenuBuilder::FillPDGMenu),
		false,
		FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.PDGLink"));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Plugin", LOCTEXT("PluginLabel", "Plugin"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._InstallInfo);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PluginSettings);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PluginEditorSettings);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("File", LOCTEXT("FileLabel", "File"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenInHoudini);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._SaveHIPFile);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Help", LOCTEXT("HelpLabel", "Help And Support"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OnlineDoc);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OnlineForum);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ReportBug);

	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ContentExampleGit);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ContentExampleBrowseTo);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Actions", LOCTEXT("ActionsLabel", "Actions"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CookAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CookSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RebuildAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RebuildSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PauseAssetCooking);
	
	MenuBuilder.EndSection();
}

void
FHoudiniEngineEditor::RegisterForUndo()
{
	/*
	if (GUnrealEd)
		GUnrealEd->RegisterForUndo(this);
	*/
}

void
FHoudiniEngineEditor::UnregisterForUndo()
{
	/*
	if (GUnrealEd)
		GUnrealEd->UnregisterForUndo(this);
	*/
}

void
FHoudiniEngineEditor::RegisterPlacementModeExtensions()
{

}

void
FHoudiniEngineEditor::UnregisterPlacementModeExtensions()
{

}

void
FHoudiniEngineEditor::RegisterSectionMappings()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Houdini Category Section
	FName ClassName = UHoudiniAssetComponent::StaticClass()->GetFName();
	TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ClassName, "Houdini", LOCTEXT("Houdini", "Houdini"));

	// The Section (more or less details filters) will contain the following categories
	// Houdini Engine
	FString CatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_MAIN);
	Section->AddCategory(*CatName);

	// HoudiniPDGAssetLink
	CatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PDG);
	Section->AddCategory(*CatName);

	// HoudiniParameters
	CatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_PARAMS);
	Section->AddCategory(*CatName);

	// HoudiniHandles
	CatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_HANDLES);
	Section->AddCategory(*CatName);

	// HoudiniInputs
	CatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_INPUTS);
	Section->AddCategory(*CatName);

	// HoudiniOutputs
	CatName = TEXT(HOUDINI_ENGINE_EDITOR_CATEGORY_OUTPUTS);
	Section->AddCategory(*CatName);

	// Categories manually defined in HoudiniAssetComponent.h
	CatName = TEXT("HoudiniMeshGeneration");
	Section->AddCategory(*CatName);

	CatName = TEXT("HoudiniProxyMeshGeneration");
	Section->AddCategory(*CatName);

	CatName = TEXT("HoudiniAsset");
	Section->AddCategory(*CatName);	
}

void 
FHoudiniEngineEditor::UnregisterSectionMappings()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor") && FSlateApplication::IsInitialized())
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FName ClassName = UHoudiniAssetComponent::StaticClass()->GetFName();

		PropertyModule.RemoveSection(ClassName, "Houdini");
	}
}


void 
FHoudiniEngineEditor::InitializeWidgetResource()
{
	// Choice labels for all the input types
	//TArray<TSharedPtr<FString>> InputTypeChoiceLabels;
	InputTypeChoiceLabels.Reset();
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Geometry))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::World))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Curve))));


	BlueprintInputTypeChoiceLabels.Reset();
	BlueprintInputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Geometry))));
	BlueprintInputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Curve))));

	// Choice labels for all Houdini curve types
	HoudiniCurveTypeChoiceLabels.Reset();
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Polygon))));
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Nurbs))));
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Bezier))));
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Points))));

	// Choice labels for all Houdini curve methods
	HoudiniCurveMethodChoiceLabels.Reset();
	HoudiniCurveMethodChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(EHoudiniCurveMethod::CVs))));
	HoudiniCurveMethodChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(EHoudiniCurveMethod::Breakpoints))));
	HoudiniCurveMethodChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(EHoudiniCurveMethod::Freehand))));

	// Choice labels for all Houdini breakpoint parameterization
	HoudiniCurveBreakpointParameterizationChoiceLabels.Reset();
	HoudiniCurveBreakpointParameterizationChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveBreakpointParameterizationToString(EHoudiniCurveBreakpointParameterization::Uniform))));
	HoudiniCurveBreakpointParameterizationChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveBreakpointParameterizationToString(EHoudiniCurveBreakpointParameterization::Chord))));
	HoudiniCurveBreakpointParameterizationChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveBreakpointParameterizationToString(EHoudiniCurveBreakpointParameterization::Centripetal))));

	// Choice labels for all Houdini ramp parameter interpolation methods
	HoudiniParameterRampInterpolationLabels.Reset();
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::CONSTANT))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::LINEAR))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::CATMULL_ROM))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::MONOTONE_CUBIC))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::BEZIER))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::BSPLINE))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::HERMITE))));

	// Choice labels for all Houdini curve output export types
	HoudiniCurveOutputExportTypeLabels.Reset();
	HoudiniCurveOutputExportTypeLabels.Add(MakeShareable(new FString(TEXT("Unreal Spline"))));
	HoudiniCurveOutputExportTypeLabels.Add(MakeShareable(new FString(TEXT("Houdini Spline"))));

	// Choice labels for all Unreal curve output curve types 
	//(for temporary, we need to figure out a way to access the output curve's info later)
	UnrealCurveOutputCurveTypeLabels.Reset();
	UnrealCurveOutputCurveTypeLabels.Add(MakeShareable(new FString(TEXT("Linear"))));
	UnrealCurveOutputCurveTypeLabels.Add(MakeShareable(new FString(TEXT("Curve"))));

	// Option labels for all landscape outputs bake options
	HoudiniLandscapeOutputBakeOptionLabels.Reset();
	HoudiniLandscapeOutputBakeOptionLabels.Add(MakeShareable(new FString(TEXT("To Current Level"))));
	HoudiniLandscapeOutputBakeOptionLabels.Add(MakeShareable(new FString(TEXT("To Image"))));
	HoudiniLandscapeOutputBakeOptionLabels.Add(MakeShareable(new FString(TEXT("To New World"))));

	// Option labels for Houdini Engine PDG bake options
	HoudiniEnginePDGBakeTypeOptionLabels.Reset();
	HoudiniEnginePDGBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToActor))));
	HoudiniEnginePDGBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToBlueprint))));

	// Option labels for Houdini Engine bake options
	HoudiniEngineBakeTypeOptionLabels.Reset();
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToActor))));
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToBlueprint))));

	// Option labels for Houdini Engine PDG bake options
	HoudiniEnginePDGBakeSelectionOptionLabels.Reset();
	HoudiniEnginePDGBakeSelectionOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption::All))));
	HoudiniEnginePDGBakeSelectionOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption::SelectedNetwork))));
	HoudiniEnginePDGBakeSelectionOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption::SelectedNode))));

	HoudiniEnginePDGBakePackageReplaceModeOptionLabels.Reset();
	HoudiniEnginePDGBakePackageReplaceModeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakePackageReplaceModeOption(EPDGBakePackageReplaceModeOption::ReplaceExistingAssets))));
	HoudiniEnginePDGBakePackageReplaceModeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakePackageReplaceModeOption(EPDGBakePackageReplaceModeOption::CreateNewAssets))));
	
	HoudiniEngineBakeActorOptionsLabels.Reset();
	HoudiniEngineBakeActorOptionsLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringfromActorBakeOption(EHoudiniEngineActorBakeOption::OneActorPerComponent))));
	HoudiniEngineBakeActorOptionsLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringfromActorBakeOption(EHoudiniEngineActorBakeOption::OneActorPerHDA))));

	static FString IconsDir = FHoudiniEngineUtils::GetHoudiniEnginePluginDir() / TEXT("Resources/Icons/");

	// Houdini Logo Brush
	FString Icon128FilePath = IconsDir + TEXT("icon_houdini_logo_128");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*Icon128FilePath))
	{
		const FName BrushName(*Icon128FilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine Logo Brush
	FString HEIcon128FilePath = IconsDir + TEXT("icon_hengine_logo_128");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*HEIcon128FilePath))
	{
		const FName BrushName(*HEIcon128FilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineLogoBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine Banner
	FString HoudiniEngineUIIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_banner_d.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Rebuild Icon Brush
	FString HoudiniEngineUIRebuildIconFilePath = IconsDir + TEXT("rebuild_all16x16.png");
	//FString HoudiniEngineUIRebuildIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_reload_icon.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIRebuildIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIRebuildIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIRebuildIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Recook Icon Brush
	//FString HoudiniEngineUIRecookIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_recook_icon.png");
	FString HoudiniEngineUIRecookIconFilePath = IconsDir + TEXT("cook_all16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIRecookIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIRecookIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIRecookIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Reset Parameters Icon Brush
	//FString HoudiniEngineUIResetParametersIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_resetparameters_icon.png");
	FString HoudiniEngineUIResetParametersIconFilePath = IconsDir + TEXT("reset_parameters16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIResetParametersIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIResetParametersIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIResetParametersIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Bake
	FString BakeIconFilePath = IconsDir + TEXT("bake_all16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*BakeIconFilePath))
	{
		const FName BrushName(*BakeIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIBakeIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// CookLog
	FString CookLogIconFilePath = IconsDir + TEXT("cook_log16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*CookLogIconFilePath))
	{
		const FName BrushName(*CookLogIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUICookLogIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// AssetHelp
	FString AssetHelpIconFilePath = IconsDir + TEXT("asset_help16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetHelpIconFilePath))
	{
		const FName BrushName(*AssetHelpIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIAssetHelpIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}


	// PDG Asset Link
	// PDG
	FString PDGIconFilePath = IconsDir + TEXT("pdg_link16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGIconFilePath))
	{
		const FName BrushName(*PDGIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// PDG Cancel
	// PDGCancel
	FString PDGCancelIconFilePath = IconsDir + TEXT("pdg_cancel16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGCancelIconFilePath))
	{
		const FName BrushName(*PDGCancelIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGCancelIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// PDG Dirty All
	// PDGDirtyAll
	FString PDGDirtyAllIconFilePath = IconsDir + TEXT("pdg_dirty_all16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGDirtyAllIconFilePath))
	{
		const FName BrushName(*PDGDirtyAllIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGDirtyAllIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// PDG Dirty Node
	// PDGDirtyNode
	FString PDGDirtyNodeIconFilePath = IconsDir + TEXT("pdg_dirty_node16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGDirtyNodeIconFilePath))
	{
		const FName BrushName(*PDGDirtyNodeIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGDirtyNodeIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// PDG Pause
	// PDGReset
	FString PDGPauseIconFilePath = IconsDir + TEXT("pdg_pause16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGPauseIconFilePath))
	{
		const FName BrushName(*PDGPauseIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGPauseIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// PDG Reset
	// PDGReset
	FString PDGResetIconFilePath = IconsDir + TEXT("pdg_reset16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGResetIconFilePath))
	{
		const FName BrushName(*PDGResetIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGResetIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// PDG Refresh
	// PDGRefresh
	FString PDGRefreshIconFilePath = IconsDir + TEXT("pdg_refresh16x16.png");
	if (FSlateApplication::IsInitialized() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*PDGRefreshIconFilePath))
	{
		const FName BrushName(*PDGRefreshIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIPDGRefreshIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}
}

void 
FHoudiniEngineEditor::SendToHoudini_CB(TArray<FAssetData> SelectedAssets)
{
	UHoudiniEditorNodeSyncSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
	if (!IsValid(HoudiniSubsystem))
		return;

	TArray<UObject*> SelectedObjects;
	for (auto& CurrentAsset : SelectedAssets)
	{
		UObject* CurrentObject = CurrentAsset.GetAsset();
		if (!IsValid(CurrentObject))
			continue;

		SelectedObjects.Add(CurrentObject);
	}

	HoudiniSubsystem->SendContentBrowserSelection(SelectedObjects);
}

void
FHoudiniEngineEditor::SendToHoudini_World()
{
	UHoudiniEditorNodeSyncSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorNodeSyncSubsystem>();
	if (!IsValid(HoudiniSubsystem))
		return;

	HoudiniSubsystem->SendWorldSelection();
}

void
FHoudiniEngineEditor::ExtendContextMenu()
{
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray< FContentBrowserMenuExtender_SelectedAssets >& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda([this](const TArray<FAssetData>& SelectedAssets)
			{
				TSharedRef<FExtender> Extender(new FExtender());

				bool bShouldExtendAssetActions = true;
				for (const FAssetData& Asset : SelectedAssets)
				{
					// TODO: Foliage Types? BP ?
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
						if ((Asset.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName()) && (Asset.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName()) && (Asset.AssetClassPath != UAnimSequence::StaticClass()->GetClassPathName()))
#else
						if ((Asset.AssetClass != USkeletalMesh::StaticClass()->GetFName()) && (Asset.AssetClass != UStaticMesh::StaticClass()->GetFName()) && (Asset.AssetClass != UAnimSequence::StaticClass()->GetFName()))					
#endif
					{
						bShouldExtendAssetActions = false;
						break;
					}
				}

				if (bShouldExtendAssetActions)
				{
					Extender->AddMenuExtension(
						"GetAssetActions",
						EExtensionHook::After,
						nullptr,
						FMenuExtensionDelegate::CreateLambda(
							[SelectedAssets, this](FMenuBuilder& MenuBuilder)
							{
								MenuBuilder.AddMenuEntry(
									LOCTEXT("CB_Extension_SendToHoudini", "Send To Houdini"),
									LOCTEXT("CB_Extension_SendToHoudini_Tooltip", "Send this asset to houdini"),
									FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
									FUIAction(
										FExecuteAction::CreateLambda([SelectedAssets, this]() { SendToHoudini_CB(SelectedAssets); }),
										FCanExecuteAction::CreateLambda([=] { return (SelectedAssets.Num() > 0); })
									)
								);
							})
					);
				}

				return Extender;
			}
		));
		ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();

	}

}


void
FHoudiniEngineEditor::AddLevelViewportMenuExtender()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FHoudiniEngineEditor::GetLevelViewportContextMenuExtender));
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void
FHoudiniEngineEditor::RemoveLevelViewportMenuExtender()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll(
				[this](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
		}
	}
}

TSharedRef<FExtender>
FHoudiniEngineEditor::GetLevelViewportContextMenuExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	// Build an array of the HoudiniAssets corresponding to the selected actors
	TArray<TWeakObjectPtr<AActor>> Actors;
	TArray<TWeakObjectPtr<UHoudiniAsset>> HoudiniAssets;
	TArray<TWeakObjectPtr<AHoudiniAssetActor>> HoudiniAssetActors;
	for (auto CurrentActor : InActors)
	{
		if (!IsValid(CurrentActor))
			continue;

		Actors.Add(CurrentActor);

		AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(CurrentActor);
		if (!IsValid(HoudiniAssetActor))
			continue;

		HoudiniAssetActors.Add(HoudiniAssetActor);

		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!IsValid(HoudiniAssetComponent))
			continue;

		UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
		if (!IsValid(HoudiniAsset))
			continue;

		HoudiniAssets.AddUnique(HoudiniAsset);
	}

	if (HoudiniAssets.Num() > 0)
	{
		// Add the Asset menu extension
		if (AssetTypeActions.Num() > 0)
		{
			// Add the menu extensions via our HoudiniAssetTypeActions
			FAssetTypeActions_HoudiniAsset * HATA = static_cast<FAssetTypeActions_HoudiniAsset*>(AssetTypeActions[0].Get());
			if (HATA)
				Extender = HATA->AddLevelEditorMenuExtenders(HoudiniAssets);
		}
	}

	if (HoudiniAssetActors.Num() > 0)
	{
		// Add some actor menu extensions
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();
		Extender->AddMenuExtension(
			"ActorControl",
			EExtensionHook::After,
			LevelEditorCommandBindings,
			FMenuExtensionDelegate::CreateLambda([this, HoudiniAssetActors](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Recentre", "Recentre selected"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RecentreTooltip", "Recentres the selected Houdini Asset Actors pivots to their input/cooked static mesh average centre."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
				FUIAction(
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecentreSelection(); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Recook", "Recook selected"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RecookTooltip", "Forces a recook on the selected Houdini Asset Actors."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine._CookSelected"),
				FUIAction(					
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecookSelection(); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Rebuild", "Rebuild selected"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RebuildTooltip", "Rebuilds selected Houdini Asset Actors in the current level."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine._RebuildSelected"),
				FUIAction(
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RebuildSelection(); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Refine_ProxyMeshes", "Refine Houdini Proxy Meshes"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Refine_ProxyMeshesTooltip", "Build and replace Houdini Proxy Meshes with Static Meshes."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine._RefineSelected"),
				FUIAction(
					FExecuteAction::CreateLambda([]() { FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(true); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);
		})
		);
	}

	// Now add the node sync extender if we have any actor
	if (Actors.Num() > 0)
	{
		// Add some actor menu extensions
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();
		Extender->AddMenuExtension(
			"ActorControl",
			EExtensionHook::After,
			LevelEditorCommandBindings,
			FMenuExtensionDelegate::CreateLambda([this, Actors](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "Houdini_NodeSync_SendToHoudini", "Send to Houdini"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "Houdini_NodeSync_SendToHoudiniTooltip", "Sends the current selection to Houdini via Node Sync."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
				FUIAction(
					FExecuteAction::CreateLambda([this]() { return SendToHoudini_World(); }),
					FCanExecuteAction::CreateLambda([=] { return ((Actors.Num() > 0)); })
				)
			);
		})
		);
	}

	return Extender;
}

void
FHoudiniEngineEditor::RegisterConsoleCommands()
{
	// Register corresponding console commands
	static FAutoConsoleCommand CCmdOpen = FAutoConsoleCommand(
		TEXT("Houdini.Open"),
		TEXT("Open the scene in Houdini."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::OpenInHoudini));

	static FAutoConsoleCommand CCmdSave = FAutoConsoleCommand(
		TEXT("Houdini.Save"),
		TEXT("Save the current Houdini scene to a hip file."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::SaveHIPFile));

	static FAutoConsoleCommand CCmdBake = FAutoConsoleCommand(
		TEXT("Houdini.BakeAll"),
		TEXT("Bakes and replaces with blueprints all Houdini Asset Actors in the current level."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::BakeAllAssets));

	static FAutoConsoleCommand CCmdClean = FAutoConsoleCommand(
		TEXT("Houdini.CleanTemp"),
		TEXT("Cleans up unused/unreferenced Houdini Engine temporary files."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::CleanUpTempFolder));

	static FAutoConsoleCommand CCmdPause = FAutoConsoleCommand(
		TEXT("Houdini.Pause"),
		TEXT("Pauses Houdini Engine Asset cooking."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::PauseAssetCooking));

	// Additional console only commands
	static FAutoConsoleCommand CCmdCookAll = FAutoConsoleCommand(
		TEXT("Houdini.CookAll"),
		TEXT("Re-cooks all Houdini Engine Asset Actors in the current level."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::RecookAllAssets));

	static FAutoConsoleCommand CCmdRebuildAll = FAutoConsoleCommand(
		TEXT("Houdini.RebuildAll"),
		TEXT("Rebuilds all Houdini Engine Asset Actors in the current level."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::RebuildAllAssets));

	static FAutoConsoleCommand CCmdCookSelec = FAutoConsoleCommand(
		TEXT("Houdini.Cook"),
		TEXT("Re-cooks selected Houdini Asset Actors in the current level."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::RecookSelection));

	static FAutoConsoleCommand CCmdRebuildSelec = FAutoConsoleCommand(
		TEXT("Houdini.Rebuild"),
		TEXT("Rebuilds selected Houdini Asset Actors in the current level."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::RebuildSelection));

	static FAutoConsoleCommand CCmdBakeSelec = FAutoConsoleCommand(
		TEXT("Houdini.Bake"),
		TEXT("Bakes and replaces with blueprints selected Houdini Asset Actors in the current level."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::BakeSelection));

	static FAutoConsoleCommand CCmdRestartSession = FAutoConsoleCommand(
		TEXT("Houdini.RestartSession"),
		TEXT("Restart the current Houdini Session."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::RestartSession));

	/*
	IConsoleManager &ConsoleManager = IConsoleManager::Get();
	const TCHAR *CommandName = TEXT("HoudiniEngine.RefineHoudiniProxyMeshesToStaticMeshes");
	IConsoleCommand *Command = ConsoleManager.RegisterConsoleCommand(
		CommandName,
		TEXT("Builds and replaces all Houdini proxy meshes with UStaticMeshes."),
		FConsoleCommandDelegate::CreateLambda([]() { FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(false); }));
	if (Command)
	{
		ConsoleCommands.Add(Command);
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to register the '%s' console command."), CommandName);
	}
	*/

	static FAutoConsoleCommand CCmdRefine = FAutoConsoleCommand(
		TEXT("Houdini.RefineAll"),
		TEXT("Builds and replaces all Houdini proxy meshes with UStaticMeshes."),
		FConsoleCommandDelegate::CreateLambda([]() { FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(false); }));

	static FAutoConsoleCommand CCmdOpenSessionSync = FAutoConsoleCommand(
		TEXT("Houdini.OpenSessionSync"),
		TEXT("Stops the current session, opens Houdini and automatically start and connect a Session Sync."),
		FConsoleCommandDelegate::CreateLambda([]() { FHoudiniEngineCommands::OpenSessionSync(false); }));

#if !UE_BUILD_SHIPPING
	static FAutoConsoleCommand CCmdClearInputManager = FAutoConsoleCommand(
		TEXT("Houdini.Debug.ClearInputManager"),
		TEXT("Clears all entries from the input manager."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::ClearInputManager));
#endif

	static FAutoConsoleCommand CCmdDumpGenericAttribute = FAutoConsoleCommand(
		TEXT("Houdini.DumpGenericAttribute"),
		TEXT("Outputs a list of all the generic property attribute for a given class."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FHoudiniEngineCommands::DumpGenericAttribute));

	static FAutoConsoleCommand CCmdCleanSession = FAutoConsoleCommand(
		TEXT("Houdini.CleanSession"),
		TEXT("Cleans the current Houdini Engine Session - this will delete every node in the current Houdini Session."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::CleanHoudiniEngineSession));

	static FAutoConsoleCommand CCmdStartPerfMon = FAutoConsoleCommand(
		TEXT("Houdini.StartHAPIPerformanceMonitor"),
		TEXT("Starts a HAPI Performance Monitoring Session."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::StartPerformanceMonitoring));

	static FAutoConsoleCommand CCmdStopPerfMon = FAutoConsoleCommand(
		TEXT("Houdini.StopHAPIPerformanceMonitor"),
		TEXT("Stops and save to file the current HAPI Performance Monitoring Session."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::StopPerformanceMonitoring));
}

void
FHoudiniEngineEditor::UnregisterConsoleCommands()
{
	IConsoleManager &ConsoleManager = IConsoleManager::Get();
	for (IConsoleCommand *Command : ConsoleCommands)
	{
		if (Command)
		{
			ConsoleManager.UnregisterConsoleObject(Command);
		}
	}
	ConsoleCommands.Empty();
}

void
FHoudiniEngineEditor::RegisterEditorDelegates()
{
	// This runs when the world has been modified and saved
	// (in non-WP world, this is called when saving the level)
	PreSaveWorldEditorDelegateHandle = FEditorDelegates::PreSaveWorldWithContext.AddLambda([this](UWorld* World, FObjectPreSaveContext InContext)
	{
		// Skip if this is a game world or an autosave, only refine meshes when the user manually saves
		if (World->IsGameWorld() || InContext.GetSaveFlags() & ESaveFlags::SAVE_FromAutosave || InContext.IsProceduralSave())
			return;

		// Do the refinement
		HandleOnPreSave(World);

		// Set a PostSaveWorld delegate for saving all dirty temp packages
		FDelegateHandle& OnPostSaveWorldHandle = FHoudiniEngineEditor::Get().GetOnPostSaveWorldOnceHandle();
		if (OnPostSaveWorldHandle.IsValid())
		{
			if (FEditorDelegates::PostSaveWorldWithContext.Remove(OnPostSaveWorldHandle))
				OnPostSaveWorldHandle.Reset();
		}

		// Save all dirty temporary cook package OnPostSaveWorld
		OnPostSaveWorldHandle = FEditorDelegates::PostSaveWorldWithContext.AddLambda(
			[World](UWorld* PreSaveWorld, FObjectPostSaveContext InContext)
		{
			if (World && World != PreSaveWorld)
				return;

			FHoudiniEngineEditorUtils::SaveAllHoudiniTemporaryCookData(PreSaveWorld);

			FDelegateHandle& OnPostSaveWorldHandle = FHoudiniEngineEditor::Get().GetOnPostSaveWorldOnceHandle();
			if (OnPostSaveWorldHandle.IsValid())
			{
				if (FEditorDelegates::PostSaveWorldWithContext.Remove(OnPostSaveWorldHandle))
					OnPostSaveWorldHandle.Reset();
			}
		});
	});
	
	// WP worlds do not call the PreSaveWorld callback when saving the current level
	// This prevented the refinement when saving from being executed properly
	// We can instead rely on PreSavePackage, when called on HoudiniAssetActors.
	// This means that the refinement is called multiple times when multiple HDAs are in the level,
	// but the actual refinement process happens only once.
	PreSavePackageEditorDelegateHandle = UPackage::PreSavePackageWithContextEvent.AddLambda([this](UPackage* Package, FObjectPreSaveContext InContext)
	{
		// Detect if we should actually do anything (check for autosaves, cooking, etc.)
		if (InContext.GetSaveFlags() & ESaveFlags::SAVE_FromAutosave || InContext.IsProceduralSave())
			return;

		// Only runs the refinement when Houdini Asset Actors are being saved
		UObject* Asset = Package->FindAssetInPackage();
		if (!IsValid(Asset) || !Asset->IsA<AHoudiniAssetActor>())
			return;
		
		AHoudiniAssetActor* HAA = Cast<AHoudiniAssetActor>(Asset);
		if (!IsValid(HAA))
			return;
		
		UWorld* World = HAA->GetWorld();
		if (World->IsGameWorld())
			return;

		bool bPostSaveNeeded = HandleOnPreSave(World);
		
		// Only add a PostSave delegate call if refinement happened
		if(!bPostSaveNeeded)
			return;

		// Set a PostSavePackage delegate for saving all dirty temp packages
		FDelegateHandle& OnPostSavePackageOnceHandle = FHoudiniEngineEditor::Get().GetOnPostSavePackageOnceHandle();
		if (OnPostSavePackageOnceHandle.IsValid())
		{
			if (UPackage::PackageSavedWithContextEvent.Remove(OnPostSavePackageOnceHandle))
				OnPostSavePackageOnceHandle.Reset();
		}

		// Save all dirty temporary cook package on PostSavePackage
		OnPostSavePackageOnceHandle = UPackage::PackageSavedWithContextEvent.AddLambda(
			[World](const FString & PackageFilename, UPackage * Package, FObjectPostSaveContext ObjectSaveContext)
		{
			FHoudiniEngineEditorUtils::SaveAllHoudiniTemporaryCookData(World);

			FDelegateHandle& OnPostSavePackageOnceHandle = FHoudiniEngineEditor::Get().GetOnPostSavePackageOnceHandle();
			if (OnPostSavePackageOnceHandle.IsValid())
			{
				if (UPackage::PackageSavedWithContextEvent.Remove(OnPostSavePackageOnceHandle))
					OnPostSavePackageOnceHandle.Reset();
			}
		});
	});

	PreBeginPIEEditorDelegateHandle = FEditorDelegates::PreBeginPIE.AddLambda([this](const bool bIsSimulating){	this->HandleOnBeginPIE(); });

	OnDeleteActorsBegin = FEditorDelegates::OnDeleteActorsBegin.AddLambda([this](){ this->HandleOnDeleteActorsBegin(); });
	OnDeleteActorsEnd = FEditorDelegates::OnDeleteActorsEnd.AddLambda([this](){ this-> HandleOnDeleteActorsEnd(); });

	/*
	//
	// COMMENTED OUT! Unreal actually calls the delegate for all class, not just the specified owner
	// so this actually prevents renaming for everything... 
	//
	// Add a rename prevention filter to HodiniToolsPackage
	OnIsNameAllowed.BindLambda([](const FString& Name, FText* OutErrorMessage) -> bool
		{
			if (Name != FHoudiniToolsRuntimeUtils::GetPackageUAssetName())
			{
				*OutErrorMessage = FText::FromString("Renaming a HoudiniToolsPackage to anything but \"HoudiniToolsPackage\" is not supported.");
				return false;
			}

			return true;
		});
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().RegisterIsNameAllowedDelegate("HoudiniToolsPackage", OnIsNameAllowed);
	*/
}

void
FHoudiniEngineEditor::UnregisterEditorDelegates()
{
	if (PreSaveWorldEditorDelegateHandle.IsValid())
		FEditorDelegates::PreSaveWorldWithContext.Remove(PreSaveWorldEditorDelegateHandle);

	if (PreSavePackageEditorDelegateHandle.IsValid())
		UPackage::PreSavePackageWithContextEvent.Remove(PreSavePackageEditorDelegateHandle);

	if (PreBeginPIEEditorDelegateHandle.IsValid())
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEEditorDelegateHandle);

	if (EndPIEEditorDelegateHandle.IsValid())
		FEditorDelegates::EndPIE.Remove(EndPIEEditorDelegateHandle);

	if (OnDeleteActorsBegin.IsValid())
		FEditorDelegates::OnDeleteActorsBegin.Remove(OnDeleteActorsBegin);

	if (OnDeleteActorsEnd.IsValid())
		FEditorDelegates::OnDeleteActorsEnd.Remove(OnDeleteActorsEnd);
	
	/*
	// Unregister the ToolsPackage rename delegate
	if (OnIsNameAllowed.IsBound())
	{
		FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
		if (AssetToolsModule)
			AssetToolsModule->Get().UnregisterIsNameAllowedDelegate("HoudiniToolsPackage");

		OnIsNameAllowed.Unbind();
	}	
	*/
}

FString 
FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption BakeOption) 
{
	FString Str;
	switch (BakeOption) 
	{
	case EHoudiniEngineBakeOption::ToActor:
		Str = "Actor";
		break;

	case EHoudiniEngineBakeOption::ToBlueprint:
		Str = "Blueprint";
		break;
	}

	return Str;
}

FString 
FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption BakeOption) 
{
	FString Str;
	switch (BakeOption) 
	{
	case EPDGBakeSelectionOption::All:
		Str = "All Outputs";
		break;

	case EPDGBakeSelectionOption::SelectedNetwork:
		Str = "Selected Network (All Outputs)";
		break;

	case EPDGBakeSelectionOption::SelectedNode:
		Str = "Selected Node (All Outputs)";
		break;
	}

	return Str;
}

FString
FHoudiniEngineEditor::GetStringfromActorBakeOption(EHoudiniEngineActorBakeOption ActorBakeOption)
{
	FString Str;
	switch(ActorBakeOption)
	{
	case EHoudiniEngineActorBakeOption::OneActorPerHDA:
		Str = "One Actor Per HDA";
		break;

	case EHoudiniEngineActorBakeOption::OneActorPerComponent:
		Str = "One Actor Per Component";
		break;
	}
	return Str;
}


EHoudiniEngineActorBakeOption
FHoudiniEngineEditor::StringToHoudiniEngineActorBakeOption(const FString& InString)
{
	if (InString == "One Actor Per HDA")
		return  EHoudiniEngineActorBakeOption::OneActorPerHDA;
	if (InString == "One Actor Per Component")
		return  EHoudiniEngineActorBakeOption::OneActorPerComponent;
	return EHoudiniEngineActorBakeOption::OneActorPerComponent;;
}

FString
FHoudiniEngineEditor::GetStringFromPDGBakePackageReplaceModeOption(EPDGBakePackageReplaceModeOption InOption)
{
	FString Str;
	switch (InOption)
	{
		case EPDGBakePackageReplaceModeOption::CreateNewAssets:
			Str = "Create New Assets";
			break;
		case EPDGBakePackageReplaceModeOption::ReplaceExistingAssets:
			Str = "Replace Existing Assets";
			break;
	}
	
	return Str;
}

EHoudiniEngineBakeOption 
FHoudiniEngineEditor::StringToHoudiniEngineBakeOption(const FString & InString) 
{
	if (InString == "Actor")
		return EHoudiniEngineBakeOption::ToActor;

	if (InString == "Blueprint")
		return EHoudiniEngineBakeOption::ToBlueprint;

	return EHoudiniEngineBakeOption::ToActor;
}

EPDGBakeSelectionOption 
FHoudiniEngineEditor::StringToPDGBakeSelectionOption(const FString& InString) 
{
	if (InString == "All Outputs")
		return EPDGBakeSelectionOption::All;

	if (InString == "Selected Network (All Outputs)")
		return EPDGBakeSelectionOption::SelectedNetwork;

	if (InString == "Selected Node (All Outputs)")
		return EPDGBakeSelectionOption::SelectedNode;

	return EPDGBakeSelectionOption::All;
}

EPDGBakePackageReplaceModeOption
FHoudiniEngineEditor::StringToPDGBakePackageReplaceModeOption(const FString & InString)
{
	if (InString == "Create New Assets")
		return EPDGBakePackageReplaceModeOption::CreateNewAssets;

	if (InString == "Replace Existing Assets")
		return EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;

	return EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
}

EPackageReplaceMode
FHoudiniEngineEditor::PDGBakePackageReplaceModeToPackageReplaceMode(const EPDGBakePackageReplaceModeOption& InReplaceMode)
{
	EPackageReplaceMode Mode;
	switch (InReplaceMode)
	{
		case EPDGBakePackageReplaceModeOption::CreateNewAssets:
			Mode = EPackageReplaceMode::CreateNewAssets;
			break;
		case EPDGBakePackageReplaceModeOption::ReplaceExistingAssets:
			Mode = EPackageReplaceMode::ReplaceExistingAssets;
			break;
		default:
		{
			Mode = FHoudiniPackageParams::GetDefaultReplaceMode();
			HOUDINI_LOG_WARNING(TEXT("Unsupported value for EPDGBakePackageReplaceModeOption %d, using "
				"FHoudiniPackageParams::GetDefaultReplaceMode() for resulting EPackageReplaceMode %d"),
				InReplaceMode, Mode);
		}
	}

	return Mode;
}

bool
FHoudiniEngineEditor::HandleOnPreSave(UWorld* InWorld)
{
	// Refine current ProxyMeshes to Static Meshes
	const bool bSelectedOnly = false;
	const bool bSilent = false;
	const bool bRefineAll = false;
	const bool bOnPreSaveWorld = true;
	//UWorld* const OnPreSaveWorld = InWorld;
	const bool bOnPreBeginPIE = false;
	
	// Do the refinement
	EHoudiniProxyRefineRequestResult RefineResult = FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(bSelectedOnly, bSilent, bRefineAll, bOnPreSaveWorld, InWorld, bOnPreBeginPIE);

	//Return true only if a refinement happened 
	if (RefineResult == EHoudiniProxyRefineRequestResult::Invalid || RefineResult == EHoudiniProxyRefineRequestResult::None)
		return false;

	return true;
}

void
FHoudiniEngineEditor::HandleOnBeginPIE()
{
	// If the Houdini Engine Session was connected and valid before PIE, 
	// we'll need to reconnect the Houdini session after PIE.
	// Setup a delegate for that
	if (FHoudiniEngine::Get().IsTicking())
	{
		const bool bWasConnected = FHoudiniEngine::Get().GetSessionStatus() == EHoudiniSessionStatus::Connected;
		if (bWasConnected)
		{
			EndPIEEditorDelegateHandle = FEditorDelegates::EndPIE.AddLambda([&, bWasConnected](const bool bEndPIEIsSimulating)
			{
				// If the Houdini session was previously connected, we need to reestablish the connection after PIE.
				// We need to restart the current Houdini Engine Session
				// This will reuse the previous session if it didnt shutdown, or start a new one if needed.
				// (HARS shuts down when stopping the session, so we cant just reconnect when not using Session Sync)
				FHoudiniEngineCommands::RestartSession();

				FEditorDelegates::EndPIE.Remove(EndPIEEditorDelegateHandle);
			});
		}
	}

	// Refine ProxyMeshes to StaticMeshes for PIE
	const bool bSelectedOnly = false;
	const bool bSilent = false;
	const bool bRefineAll = false;
	const bool bOnPreSaveWorld = false;
	UWorld* const OnPreSaveWorld = nullptr;
	const bool bOnPreBeginPIE = true;
	FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(bSelectedOnly, bSilent, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE);
}

void
FHoudiniEngineEditor::HandleOnDeleteActorsBegin()
{
	if (!GEditor)
		return;
	
	TArray<AHoudiniAssetActor*> AssetActorsWithTempPDGOutput;
	// Iterate over all selected actors
	for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* SelectedActor = Cast<AActor>(*It);
		if (IsValid(SelectedActor))
		{
			// If the class is a AHoudiniAssetActor check if it has temporary PDG outputs
			AHoudiniAssetActor* AssetActor = Cast<AHoudiniAssetActor>(SelectedActor);
			if (IsValid(AssetActor))
			{
				UHoudiniPDGAssetLink* AssetLink = AssetActor->GetPDGAssetLink();
				if (IsValid(AssetLink) && AssetLink->HasTemporaryOutputs())
				{
					AssetActorsWithTempPDGOutput.Add(AssetActor);						
				}
			}
		}
	}

	if (AssetActorsWithTempPDGOutput.Num() > 0)
	{
		const FText DialogTitle = LOCTEXT(
			"PDGAssetLink_DeleteWithTemporaryOutputs_Title",
			"Warning: PDG Asset Link(s) With Temporary Outputs");
		const EAppReturnType::Type Choice = FMessageDialog::Open(
			EAppMsgType::YesNo,
			EAppReturnType::No,
			LOCTEXT(
				"PDGAssetLink_DeleteWithTemporaryOutputs",
				"One or more PDG Asset Links in the selection still have temporary outputs. Are you sure you want to "
				"delete these PDG Asset Links and their actors?"),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			DialogTitle);
#else
			&DialogTitle);
#endif

		const bool bKeepAssetLinkActors = (Choice == EAppReturnType::No);
		for (AHoudiniAssetActor* AssetActor : AssetActorsWithTempPDGOutput)
		{
			if (bKeepAssetLinkActors)
			{
				GEditor->SelectActor(AssetActor, false, false);
				ActorsToReselectOnDeleteActorsEnd.Add(AssetActor);
			}
		}
	}
}

void
FHoudiniEngineEditor::HandleOnDeleteActorsEnd()
{
	if (!GEditor)
		return;

	for (AActor* Actor : ActorsToReselectOnDeleteActorsEnd)
	{
		if (IsValid(Actor))
			GEditor->SelectActor(Actor, true, false);
	}
	GEditor->NoteSelectionChange();
	ActorsToReselectOnDeleteActorsEnd.Empty();
}


TSharedRef<SDockTab> 
FHoudiniEngineEditor::OnSpawnNodeSyncTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
	.TabRole(ETabRole::NomadTab)
	//.Icon(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngineLogo"))
	[
		SAssignNew(NodeSyncPanel, SHoudiniNodeSyncPanel)
	];

	SpawnedTab->SetTabIcon(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngineLogo"));

	return SpawnedTab;
}

TSharedRef<SDockTab> FHoudiniEngineEditor::OnSpawnHoudiniToolsTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
	.TabRole(ETabRole::NomadTab)
	//.Icon(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngineLogo"))
	[
		SNew(SHoudiniToolsPanel)
	];

	SpawnedTab->SetTabIcon(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngineLogo"));

	return SpawnedTab;
}


#undef LOCTEXT_NAMESPACE
