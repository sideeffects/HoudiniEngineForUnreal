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

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniInput.h"
#include "HoudiniOutput.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniRuntimeSettingsDetails.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniHandleComponentVisualizer.h"
#include "AssetTypeActions_HoudiniAsset.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniPackageParams.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Templates/SharedPointer.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/ConsoleManager.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Engine/Selection.h"
#include "Widgets/Input/SCheckBox.h"
#include "Logging/LogMacros.h"
//#include "UObject/ObjectSaveContext.h"
#include "Engine/SkeletalMesh.h"
#include "UnrealMeshTranslator.h"
#include "HoudiniEditorSubsystem.h"
#include "ContentBrowserModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Brushes/SlateImageBrush.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SExpandableArea.h"
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

	//RegisterPlacementModeExtensions();

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
		UHoudiniAssetActorFactory * HoudiniAssetActorFactory =
			NewObject< UHoudiniAssetActorFactory >(GetTransientPackage(), UHoudiniAssetActorFactory::StaticClass());

		GEditor->ActorFactories.Add(HoudiniAssetActorFactory);
	}
}

void
FHoudiniEngineEditor::RegisterEditorTabs()
{
	/*
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(NodeSyncTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnNodeSyncTab))
			.SetDisplayName(LOCTEXT("FNodeSyncTitleTitle", "NodeSync"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	*/

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FGlobalTabmanager::Get()->RegisterTabSpawner(NodeSyncTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnNodeSyncTab))
		.SetDisplayName(LOCTEXT("FNodeSyncTitleTitle", "NodeSync"))
		.SetTooltipText(LOCTEXT("FNodeSyncTitleTitleTooltip", "Houdini Node Sync"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetGroup(MenuStructure.GetLevelEditorCategory());

	/*
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	LevelEditorTabManager->RegisterTabSpawner(NodeSyncTabName, FOnSpawnTab::CreateRaw(this, &FHoudiniEngineEditor::OnSpawnNodeSyncTab))
		.SetDisplayName(LOCTEXT("FNodeSyncTitleTitle", "NodeSync"))
		.SetTooltipText(LOCTEXT("FNodeSyncTitleTitleTooltip", "Houdini Node Sync"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetGroup(MenuStructure.GetLevelEditorCategory());
	*/
}

void
FHoudiniEngineEditor::UnRegisterEditorTabs()
{
	FGlobalTabmanager::Get()->UnregisterTabSpawner(NodeSyncTabName);
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
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ReportBug);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeAll);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PauseAssetCooking);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RestartSession);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineAll);

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
	/*
	MenuBuilder.BeginSection("Houdini", LOCTEXT("HoudiniLabel", "Houdini Engine"));
	// Icons used by the commands are defined in the HoudiniEngineStyle
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenInHoudini);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._SaveHIPFile);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ReportBug);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PauseAssetCooking);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RestartSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineAll);
	MenuBuilder.EndSection();
	*/

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
	// Load custom houdini tools
	/*
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	check(HoudiniRuntimeSettings);

	if (HoudiniRuntimeSettings->bHidePlacementModeHoudiniTools)
		return;

	FPlacementCategoryInfo Info(
		LOCTEXT("HoudiniCategoryName", "Houdini Engine"),
		"HoudiniEngine",
		TEXT("PMHoudiniEngine"),
		25
	);
	Info.CustomGenerator = []() -> TSharedRef<SWidget> { return SNew(SHoudiniToolPalette); };

	IPlacementModeModule::Get().RegisterPlacementCategory(Info);
	*/
}

void
FHoudiniEngineEditor::UnregisterPlacementModeExtensions()
{
	/*
	if (IPlacementModeModule::IsAvailable())
	{
		IPlacementModeModule::Get().UnregisterPlacementCategory("HoudiniEngine");
	}

	HoudiniTools.Empty();
	*/
}

void 
FHoudiniEngineEditor::InitializeWidgetResource()
{
	// Choice labels for all the input types
	//TArray<TSharedPtr<FString>> InputTypeChoiceLabels;
	InputTypeChoiceLabels.Reset();
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Geometry))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Curve))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Asset))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Landscape))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::World))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Skeletal))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::GeometryCollection))));

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
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToFoliage))));
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToWorldOutliner))));

	// Option labels for Houdini Engine PDG bake options
	HoudiniEnginePDGBakeSelectionOptionLabels.Reset();
	HoudiniEnginePDGBakeSelectionOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption::All))));
	HoudiniEnginePDGBakeSelectionOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption::SelectedNetwork))));
	HoudiniEnginePDGBakeSelectionOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(EPDGBakeSelectionOption::SelectedNode))));

	HoudiniEnginePDGBakePackageReplaceModeOptionLabels.Reset();
	HoudiniEnginePDGBakePackageReplaceModeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakePackageReplaceModeOption(EPDGBakePackageReplaceModeOption::ReplaceExistingAssets))));
	HoudiniEnginePDGBakePackageReplaceModeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromPDGBakePackageReplaceModeOption(EPDGBakePackageReplaceModeOption::CreateNewAssets))));
	

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
	UHoudiniEditorSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
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

	HoudiniSubsystem->SendToHoudini(SelectedObjects);
}

void
FHoudiniEngineEditor::SendToHoudini_World()
{
	UHoudiniEditorSubsystem* HoudiniSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
	if (!IsValid(HoudiniSubsystem))
		return;
	// Get current world selection
	TArray<UObject*> WorldSelection;
	int32 SelectedHoudiniAssets = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection, false);
	if (SelectedHoudiniAssets <= 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("No selection in the world outliner"));
		return;
	}

	HoudiniSubsystem->SendToHoudini(WorldSelection);
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
#if ENGINE_MINOR_VERSION < 1
					if ((Asset.AssetClass != USkeletalMesh::StaticClass()->GetFName()) && (Asset.AssetClass != UStaticMesh::StaticClass()->GetFName()))
#else
					if ((Asset.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName()) && (Asset.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName()))
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
				[=](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
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

		HoudiniAssets.AddUnique(HoudiniAssetComponent->GetHoudiniAsset());
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
					FCanExecuteAction::CreateLambda([=] { return ((Actors.Num() > 0) && FHoudiniEngine::Get().IsSessionSyncEnabled()); })
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
		TEXT("Houdini.Clean"),
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
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::OpenSessionSync));

#if !UE_BUILD_SHIPPING
	static FAutoConsoleCommand CCmdClearInputManager = FAutoConsoleCommand(
		TEXT("Houdini.Debug.ClearInputManager"),
		TEXT("Clears all entries from the input manager."),
		FConsoleCommandDelegate::CreateStatic(&FHoudiniEngineCommands::ClearInputManager));
#endif
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
}

FString 
FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(const EHoudiniEngineBakeOption & BakeOption) 
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

	case EHoudiniEngineBakeOption::ToFoliage:
		Str = "Foliage";
		break;

	case EHoudiniEngineBakeOption::ToWorldOutliner:
		Str = "World Outliner";
		break;
	}

	return Str;
}

FString 
FHoudiniEngineEditor::GetStringFromPDGBakeTargetOption(const EPDGBakeSelectionOption& BakeOption) 
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
FHoudiniEngineEditor::GetStringFromPDGBakePackageReplaceModeOption(const EPDGBakePackageReplaceModeOption & InOption)
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

const EHoudiniEngineBakeOption 
FHoudiniEngineEditor::StringToHoudiniEngineBakeOption(const FString & InString) 
{
	if (InString == "Actor")
		return EHoudiniEngineBakeOption::ToActor;

	if (InString == "Blueprint")
		return EHoudiniEngineBakeOption::ToBlueprint;

	if (InString == "Foliage")
		return EHoudiniEngineBakeOption::ToFoliage;

	if (InString == "World Outliner")
		return EHoudiniEngineBakeOption::ToWorldOutliner;

	return EHoudiniEngineBakeOption::ToActor;
}

const EPDGBakeSelectionOption 
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

const EPDGBakePackageReplaceModeOption
FHoudiniEngineEditor::StringToPDGBakePackageReplaceModeOption(const FString & InString)
{
	if (InString == "Create New Assets")
		return EPDGBakePackageReplaceModeOption::CreateNewAssets;

	if (InString == "Replace Existing Assets")
		return EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;

	return EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
}

const EPackageReplaceMode
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
			&DialogTitle);

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


TSharedRef<SDockTab> FHoudiniEngineEditor::OnSpawnNodeSyncTab(const FSpawnTabArgs& SpawnTabArgs)
{

	TSharedPtr<SHorizontalBox> Box;

	TSharedPtr<SExpandableArea> OptionsArea;

	TSharedPtr<SButton> FetchButton;
	auto OnFetchButtonClickedLambda = []()
	{
		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->Fetch();
		return FReply::Handled();
	};


	auto OnFetchNodePathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSync.FetchNodePath = NewPathStr;
	};

	auto OnSendNodePathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSync.SendNodePath = NewPathStr;
	};
	

	auto OnUnrealAssetTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSync.UnrealAssetName = NewPathStr;
	};

	auto OnUnrealPathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSync.UnrealPathName = NewPathStr;
	};

	auto OnSkeletonPathTextCommittedLambda = [](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();

		UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
		HoudiniEditorSubsystem->NodeSync.SkeletonAssetPath = NewPathStr;
	};


	const FSlateFontInfo BoldFontStyle = FCoreStyle::GetDefaultFontStyle("Bold", 14);

	//TSharedPtr<SCheckBox> CheckBoxUseDisplayFlag;
	TSharedPtr<SCheckBox> CheckBoxUseExistingSkeleton;

	TSharedRef<SDockTab> NodeSyncDock = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().HAlign(HAlign_Left)
			.Padding(15.0, 0.0, 0.0, 0.0)
			.AutoHeight()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[
					SAssignNew(Box, SHorizontalBox)
				]			
			]
			+ SVerticalBox::Slot().HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(15.0, 0.0, 15.0, 15.0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text_Lambda([]()
						{
							FString StatusString = TEXT("Session Status");
							FLinearColor StatusColor;
							//GetSessionStatusAndColor(StatusString, StatusColor);
							bool result = FHoudiniEngine::Get().GetSessionStatusAndColor(StatusString, StatusColor);
							return FText::FromString(StatusString);
						})
					.ColorAndOpacity_Lambda([]()
						{
							FString StatusString;
							FLinearColor StatusColor = FLinearColor::Red;
							bool result = FHoudiniEngine::Get().GetSessionStatusAndColor(StatusString, StatusColor);
							return FSlateColor(StatusColor);
						})
				]			
			]
			+ SVerticalBox::Slot().HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					//.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
					.Font(BoldFontStyle)
					.Text(LOCTEXT("FetchLabel", "FETCH from Houdini"))
				]
			]
			+ SVerticalBox::Slot().HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0, 0.0, 0.0, 15.0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NodePathLabel", "Houdini Node Path To Fetch"))
						]
					]
					+ SHorizontalBox::Slot().HAlign(HAlign_Right)
					[
						SNew(SEditableTextBox)
						.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.ToolTipText(LOCTEXT("HoudiniNodePathTooltip",
							"The path of the node in Houdini.  e.g /obj/MyNetwork/Mynode "))
						.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Fetch"))
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([]()
							{
								UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
								return FText::FromString(HoudiniEditorSubsystem->NodeSync.FetchNodePath);
							})
						.OnTextCommitted_Lambda(OnFetchNodePathTextCommittedLambda)
					]
				]

			//+ SVerticalBox::Slot().HAlign(HAlign_Left)
			//	.AutoHeight()
			//	.Padding(0.0f, 0.0f, 0.0f, 3.5f)
			//	[
			//		SNew(SBox)
			//		.WidthOverride(160.f)
			//		[
			//			SAssignNew(CheckBoxUseDisplayFlag, SCheckBox)
			//			.Content()
			//			[
			//				SNew(STextBlock).Text(LOCTEXT("UseDisplayFlagLabel", "Use DisplayFlag Instead"))
			//				.ToolTipText(LOCTEXT("UseDisplayFlagToolTip", "Use DisplayFlag Instead"))
			//				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			//			]
			//			.IsChecked_Lambda([]()
			//				{
			//					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
			//					return HoudiniEditorSubsystem->NodeSync.UseDisplayFlag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			//				})
			//			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
			//				{
			//					const bool bNewState = (NewState == ECheckBoxState::Checked);
			//					UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
			//					HoudiniEditorSubsystem->NodeSync.UseDisplayFlag = bNewState;

			//				})
			//		]
			//	]
			+ SVerticalBox::Slot().HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(SHorizontalBox)
					//.AutoWidth()
					//.MaxWidth(300.0f)
					//.Padding(0, 60, 0, 10)
					+SHorizontalBox::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnrealAssetLabel", "Name of Asset In Unreal"))
						]
					]
					+ SHorizontalBox::Slot().HAlign(HAlign_Right)
					//.FillWidth(0.60f)
					//.FillWidth(1)
					//.MaxWidth(10)
					//.FillWidth(0.9f)
					[
							SNew(SEditableTextBox)
							.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
							.ToolTipText(LOCTEXT("UnrealAssetTooltip",
							"What to name the asset in unreal"))
						.HintText(LOCTEXT("UnrealAssetLabel", "Name of Asset In Unreal"))
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([]()
							{
								UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
								return FText::FromString(HoudiniEditorSubsystem->NodeSync.UnrealAssetName);
							})
						.OnTextCommitted_Lambda(OnUnrealAssetTextCommittedLambda)
					]
				]
			+ SVerticalBox::Slot().HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UnrealPathLabel", "Path of Asset In Unreal"))
						]
					]
				+ SHorizontalBox::Slot().HAlign(HAlign_Right)
					[
							SNew(SEditableTextBox)
							.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
							.ToolTipText(LOCTEXT("UnrealPathTooltip","Path to the asset in unreal"))
							.HintText(LOCTEXT("UnrealPathLabel", "Path of Asset In Unreal"))
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.Text_Lambda([]()
								{
									UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
									return FText::FromString(HoudiniEditorSubsystem->NodeSync.UnrealPathName);
								})
							.OnTextCommitted_Lambda(OnUnrealPathTextCommittedLambda)
					]
				]

			+ SVerticalBox::Slot().HAlign(HAlign_Left)
				.AutoHeight()
				[
					SAssignNew(OptionsArea, SExpandableArea)
					.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
					//.BorderImage_Lambda([this]() { return ConcertFrontendUtils::GetExpandableAreaBorderImage(*DetailsArea); })
					.BodyBorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					//.OnAreaExpansionChanged(FOnBooleanValueChanged::CreateLambda(OnDetailsAreaExpansionChanged))
					.InitiallyCollapsed(true)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OptionDetails", "Import Options"))
						.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
					]
					.BodyContent()
					[
						SNew(SScrollBox)
						.Orientation(Orient_Vertical)
						+ SScrollBox::Slot()
						[
							SNew(SBox)
							.WidthOverride(335.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SKOptionsLabel", "Skeletal Mesh Options"))
							]
							/*.Padding(FMargin(0, 2, 0, 2))
							[
								SAssignNew(Grid, SGridPanel)
							]*/
						]
						+ SScrollBox::Slot()
						.Padding(0.0f, 0.0f, 0.0f, 3.5f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left)
							[
								SNew(SBox)
								.WidthOverride(160.f)
								[
									SAssignNew(CheckBoxUseExistingSkeleton, SCheckBox)
									.Content()
									[
										SNew(STextBlock).Text(LOCTEXT("OverwriteSkeletonLabel", "Use Existing Skeleton"))
										.ToolTipText(LOCTEXT("OverwriteSkeletonToolTip", "Use Existing Skeleton"))
										.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									]
									.IsChecked_Lambda([]()
										{
											UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
											return HoudiniEditorSubsystem->NodeSync.OverwriteSkeleton ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
										})
									.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
										{
											const bool bNewState = (NewState == ECheckBoxState::Checked);
											UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
											HoudiniEditorSubsystem->NodeSync.OverwriteSkeleton = bNewState;

										})
								]
							]
							+ SHorizontalBox::Slot().HAlign(HAlign_Left)
							[
								SNew(SEditableTextBox)
								.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
								.ToolTipText(LOCTEXT("ExistingSkeletonTooltip", "Path to the skelton asset in unreal"))
								.HintText(LOCTEXT("ExistingSkeletonLabel", "Path to skeleton asset In Unreal"))
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.Text_Lambda([]()
									{
										UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
										return FText::FromString(HoudiniEditorSubsystem->NodeSync.SkeletonAssetPath);
									})
								.OnTextCommitted_Lambda(OnSkeletonPathTextCommittedLambda)
							]

						]
					]
				]

			+ SVerticalBox::Slot().HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(5.0, 5.0, 5.0, 5.0)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					[
						SNew(SBox)
						.WidthOverride(135.0f)
						[
							SAssignNew(FetchButton, SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText(LOCTEXT("FetchFromHoudiniLabel", "Fetch Asset From Houdini"))
							.Visibility(EVisibility::Visible)
							.OnClicked_Lambda(OnFetchButtonClickedLambda)
							.Content()
							[
								SNew(STextBlock)
								.Text(FText::FromString("Fetch"))
							]
						]
					]
				]



			+ SVerticalBox::Slot().HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Font(BoldFontStyle)
					.Text(LOCTEXT("SendLabel", "SEND to Houdini"))
				]
			]
			+ SVerticalBox::Slot().HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(10.0, 0.0, 0.0, 15.0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().HAlign(HAlign_Left)
					[
						SNew(SBox)
						.WidthOverride(335.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NodePathLabel", "Houdini Node Path To Send To"))
					]
					]
					+ SHorizontalBox::Slot().HAlign(HAlign_Right)
					[
						SNew(SEditableTextBox)
						.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.ToolTipText(LOCTEXT("HoudiniNodePathTooltip",
						"The path of the node in Houdini to Send too.  e.g /obj/UnrealContent "))
						.HintText(LOCTEXT("NodePathLabel", "Houdini Node Path To Send To"))
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([]()
							{
								UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
								return FText::FromString(HoudiniEditorSubsystem->NodeSync.SendNodePath);
							})
						.OnTextCommitted_Lambda(OnSendNodePathTextCommittedLambda)
					]
				]
			//SNew(SBox)
			//.HAlign(HAlign_Center)
			//.VAlign(VAlign_Top)
			//[
			//	SAssignNew(Box, SHorizontalBox)
			//]
		];

	//FDetailWidgetRow& Row = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SImage> Image;

	Box->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 5.0f, 5.0f, 10.0f)
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.HeightOverride(30)
		.WidthOverride(208)
		[
			SAssignNew(Image, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		];


	// Skip drawing the icon if the icon image is not loaded correctly.
	TSharedPtr<FSlateDynamicImageBrush> IconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();
	if (IconBrush.IsValid())
	{

		Image->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([IconBrush]() {
					return IconBrush.Get();
					})));
	}
	//Row.WholeRowWidget.Widget = Box;
	//Row.IsEnabled(false);

	return NodeSyncDock;
}


#undef LOCTEXT_NAMESPACE
