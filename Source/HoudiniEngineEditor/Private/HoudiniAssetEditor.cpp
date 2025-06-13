/*
* Copyright (c) <2025> Side Effects Software Inc.
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

#include "HoudiniAssetEditor.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetEditorViewportClient.h"
#include "HoudiniCookable.h"
#include "HoudiniCookableDetails.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineStyle.h"
#include "SHoudiniAssetEditorViewport.h"
#include "SHoudiniNodeSyncPanel.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "PropertyEditorDelegates.h"
#include "SCommonEditorViewportToolbarBase.h"
//#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "HoudiniAssetEditor"

const FName HoudiniAssetEditorAppName = FName(TEXT("HoudiniAssetEditorApp"));

struct FHoudiniAssetEditorTabs
{
	// Tab identifiers
	static const FName DetailsID;
	static const FName ViewportID;
	static const FName NodeSyncID;
};

const FName FHoudiniAssetEditorTabs::DetailsID(TEXT("Details"));
const FName FHoudiniAssetEditorTabs::ViewportID(TEXT("Viewport"));
const FName FHoudiniAssetEditorTabs::NodeSyncID(TEXT("NodeSync"));

//-----------------------------------------------------------------------------
// SHoudiniAssetEditorDetailsPanel
//-----------------------------------------------------------------------------
void 
SHoudiniAssetEditorDetailsPanel::Construct(
	const FArguments& InArgs, TSharedPtr<FHoudiniAssetEditor> InHoudiniAssetEditor)
{
	HoudiniAssetEditorPtr = InHoudiniAssetEditor;
		
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Get the ViewIdentifier for this details view from the AssetEditor
	FString DetailsIdentifier = InHoudiniAssetEditor->GetHoudiniAssetEditorIdentifier();

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.HostCommandList = InHoudiniAssetEditor->GetToolkitCommands();
	DetailsViewArgs.HostTabManager = InHoudiniAssetEditor->GetTabManager();
	DetailsViewArgs.ViewIdentifier = FName(*DetailsIdentifier);

	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);
	
	// Create the box that will contain all our content
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding( 3.0f, 2.0f )
		[
			PopulateSlot(PropertyView.ToSharedRef())
		]
	];

	// TODO: Offer modes for HDA editor??
	TAttribute<EHoudiniAssetEditorMode::Type> HoudiniAssetEditorMode = TAttribute<EHoudiniAssetEditorMode::Type>::Create(
		TAttribute<EHoudiniAssetEditorMode::Type>::FGetter::CreateSP(InHoudiniAssetEditor.ToSharedRef(), &FHoudiniAssetEditor::GetCurrentMode));

	// For Cookable details customization
	FOnGetDetailCustomizationInstance CustomizeHoudiniAssetForEditor = FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniCookableDetails::MakeInstance);
	PropertyView->RegisterInstancedCustomPropertyLayout(UHoudiniAsset::StaticClass(), CustomizeHoudiniAssetForEditor);
}

UObject* 
SHoudiniAssetEditorDetailsPanel::GetObjectToObserve() const
{
	return HoudiniAssetEditorPtr.Pin()->GetHoudiniCookableBeingEdited();
}

TSharedRef<SWidget>
SHoudiniAssetEditorDetailsPanel::PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			PropertyEditorWidget
		];
}

void 
SHoudiniAssetEditorDetailsPanel::Tick(
	const FGeometry& AllottedGeometry, 
	const double InCurrentTime, 
	const float InDeltaTime)
{
	UObject* CurrentObject = GetObjectToObserve();
	if (MyLastObservedObject.Get() != CurrentObject)
	{
		MyLastObservedObject = CurrentObject;

		TArray<UObject*> SelectedObjects;
		if (CurrentObject != NULL)
		{
			SelectedObjects.Add(CurrentObject);
		}

		SetPropertyWindowContents(SelectedObjects);
	}
	/*
	if (InCurrentTime > (LastUpdateTime + 2.0))
	{
		PropertyView->ForceRefresh();
		LastUpdateTime = InCurrentTime;
	}
	*/		
}


//-----------------------------------------------------------------------------
// FHoudiniAssetEditor
//-----------------------------------------------------------------------------
FHoudiniAssetEditor::FHoudiniAssetEditor()
	: HoudiniAssetBeingEdited(nullptr),
	HoudiniCookableBeingEdited(nullptr),
	HoudiniAssetEditorIdentifier()
{
}

TSharedRef<SDockTab>
FHoudiniAssetEditor::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabTitle", "Viewport"))
		[
			SNew(SOverlay)

			// The editor viewport
			+ SOverlay::Slot()
			[
				ViewportPtr.ToSharedRef()
			]
			
			// Bottom-right corner text indicating the mode of the editor
			+ SOverlay::Slot()
			.Padding(10)
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.TextStyle(FAppStyle::Get(), "Graph.CornerText")
					.Text(this, &FHoudiniAssetEditor::GetViewportCornerText)
			]
		];
}

TSharedRef<SDockTab>
FHoudiniAssetEditor::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	TSharedPtr<FHoudiniAssetEditor> HoudiniAssetEditorPtr = SharedThis(this);

	// Spawn the tab
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabTitle", "Details"))
		[
			//SNew(SHoudiniAssetEditorTabBody, HoudiniAssetEditorPtr)
			DetailsTabPtr.ToSharedRef()
		];
}

TSharedRef<SDockTab> 
FHoudiniAssetEditor::SpawnNodeSyncTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("NodeSyncTabTitle", "Node Sync"))
		[
			SAssignNew(NodeSyncPanel, SHoudiniNodeSyncPanel)
		];

	SpawnedTab->SetTabIcon(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngineLogo"));

	return SpawnedTab;
}

void
FHoudiniAssetEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_HoudiniAssetEditor", "Houdini Asset Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::NodeSyncID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnNodeSyncTab))
		.SetDisplayName(LOCTEXT("NodeSyncTabLabel", "Node Sync"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));
}

void 
FHoudiniAssetEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::ViewportID);
	InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::DetailsID);
	InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::NodeSyncID);
}

void 
FHoudiniAssetEditor::InitHoudiniAssetEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<class IToolkitHost>& InitToolkitHost,
	class UHoudiniAsset* InitHDA)
{
	HoudiniAssetBeingEdited = InitHDA;

	// Get the next available Identifier for our details
	if(HoudiniAssetEditorIdentifier.IsEmpty())
		HoudiniAssetEditorIdentifier = 	FHoudiniEngine::Get().RegisterNewHoudiniAssetEditor();

	BindCommands();

	TSharedPtr<FHoudiniAssetEditor> HoudiniAssetEditorPtr = SharedThis(this);
	ViewportPtr = SNew(SHoudiniAssetEditorViewport, HoudiniAssetEditorPtr);
	DetailsTabPtr = SNew(SHoudiniAssetEditorDetailsPanel, HoudiniAssetEditorPtr);

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_HoudiniAssetEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab(FHoudiniAssetEditorTabs::ViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.75f)
						->SetHideTabWell(true)
						->AddTab(FHoudiniAssetEditorTabs::DetailsID, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.25f)
						->AddTab(FHoudiniAssetEditorTabs::NodeSyncID, ETabState::OpenedTab)
					)
				)
			)
		);

	// Initialize the asset editor
	InitAssetEditor(
		Mode, 
		InitToolkitHost,
		HoudiniAssetEditorAppName,
		StandaloneDefaultLayout,
		/*bCreateDefaultStandaloneMenu=*/ true,
		/*bCreateDefaultToolbar=*/ true,
		InitHDA);

	// Set the Houdini Asset on the viewport
	// This will instantiate the HDA in the editor world, creating the Actor, Component and Cookable
	ViewportPtr->SetHoudiniAsset(HoudiniAssetBeingEdited);

	// Get the Houdini Asset Actor created in the viewport
	AHoudiniAssetActor* HAA = ViewportPtr->GetViewportClient()->GetHoudiniAssetActor();
	if (HAA)
	{
		// Initialize the Cookable
		HoudiniCookableBeingEdited = HAA->GetHoudiniCookable();
				
		if (HoudiniCookableBeingEdited)
		{
			// Set supproted features in the asset editor
			HoudiniCookableBeingEdited->SetHoudiniAssetSupported(true);
			HoudiniCookableBeingEdited->SetParameterSupported(true);
			HoudiniCookableBeingEdited->SetInputSupported(true);
			HoudiniCookableBeingEdited->SetOutputSupported(true);
			HoudiniCookableBeingEdited->SetComponentSupported(true);
			HoudiniCookableBeingEdited->SetBakingSupported(true);

			// NO PDG - NO PROXIES
			HoudiniCookableBeingEdited->SetPDGSupported(false);
			HoudiniCookableBeingEdited->SetProxySupported(false);

			// Let the cookable know its used in an Houdini Asset Editor
			HoudiniCookableBeingEdited->AssetEditorId = FName(*HoudiniAssetEditorIdentifier);
			
			// Register the Cookable with the Manager
			FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(HoudiniCookableBeingEdited);		
		}
	}

	// Extend things
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void
FHoudiniAssetEditor::OnClose()
{
	// Unregister our Cookable
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(HoudiniCookableBeingEdited);

	// Unregister our Details Identifier
	FHoudiniEngine::Get().UnRegisterHoudiniAssetEditor(HoudiniAssetEditorIdentifier);
}

void 
FHoudiniAssetEditor::BindCommands()
{
}

FName 
FHoudiniAssetEditor::GetToolkitFName() const
{
	return FName("HoudiniAssetEditor");
}

FText
FHoudiniAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("HoudiniAssetEditorAppLabel", "Houdini Asset Editor");
}

FText 
FHoudiniAssetEditor::GetToolkitName() const
{
	return FText::FromString(HoudiniAssetBeingEdited->GetName());
}

FText 
FHoudiniAssetEditor::GetToolkitToolTipText() const
{
	return FAssetEditorToolkit::GetToolTipTextForObject(HoudiniAssetBeingEdited);
}

FString 
FHoudiniAssetEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("HoudiniAssetEditor");
}

FString FHoudiniAssetEditor::GetDocumentationLink() const
{
	// TODO
	return TEXT("HoudiniEngine/Awesome");
}

void 
FHoudiniAssetEditor::OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit)
{
	// @TODO: MODETOOLS: Need to be able to register the widget in the toolbox panel with ToolkitHost,
	// so it can instance the ed mode widgets into it
	// TSharedPtr<SWidget> InlineContent = Toolkit->GetInlineContent();
	// 	if (InlineContent.IsValid())
	// 	{
	// 		ToolboxPtr->SetContent(InlineContent.ToSharedRef());
	// 	}
}

void 
FHoudiniAssetEditor::OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit)
{
	//ToolboxPtr->SetContent(SNullWidget::NullWidget);
	//@TODO: MODETOOLS: How to handle multiple ed modes at once in a standalone asset editor?
}

FLinearColor 
FHoudiniAssetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(255.0f, 165.0f/255.0f, 0.0f);
}

void 
FHoudiniAssetEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(HoudiniAssetBeingEdited);
	Collector.AddReferencedObject(HoudiniCookableBeingEdited);
}

void FHoudiniAssetEditor::ExtendMenu()
{
	MainMenuExtender = MakeShareable(new FExtender);

	// Extend File menu, we will add Houdini section.
	MainMenuExtender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::After,
		FHoudiniEngineEditor::Get().GetHoudiniEngineCommands(),
		FMenuExtensionDelegate::CreateStatic(&FHoudiniEngineEditor::AddHoudiniFileMenuExtension));

	MainMenuExtender->AddMenuBarExtension(
		"Edit",
		EExtensionHook::After,
		FHoudiniEngineEditor::Get().GetHoudiniEngineCommands(),
		FMenuBarExtensionDelegate::CreateStatic(&FHoudiniEngineEditor::AddHoudiniEditorMenu));

	AddMenuExtender(MainMenuExtender);
	//GetSharedMenuExtensibilityManager()->AddExtender(MainMenuExtender);
}

void 
FHoudiniAssetEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			const FHoudiniEngineCommands& HoudiniCommands = FHoudiniEngineCommands::Get();

			ToolbarBuilder.BeginSection("Command");
			{
				ToolbarBuilder.AddToolBarButton(HoudiniCommands._RestartSession);
				ToolbarBuilder.AddToolBarButton(HoudiniCommands._StopSession);				
				ToolbarBuilder.AddToolBarButton(HoudiniCommands._OpenInHoudini);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Tools");
			{
				ToolbarBuilder.AddToolBarButton(HoudiniCommands._CookSelected);
				ToolbarBuilder.AddToolBarButton(HoudiniCommands._RebuildSelected);				
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	/*
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ViewportPtr->GetCommandList(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar)
	);
	*/
		/*
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ViewportPtr->GetCommandList(),
		FToolBarExtensionDelegate::CreateSP(this, &FHoudiniAssetEditor::CreateModeToolbarWidgets));
		*/
	AddToolbarExtender(ToolbarExtender);

	//IPaper2DEditorModule* Paper2DEditorModule = &FModuleManager::LoadModuleChecked<IPaper2DEditorModule>("Paper2DEditor");
	//AddToolbarExtender(Paper2DEditorModule->GetSpriteEditorToolBarExtensibilityManager()->GetAllExtenders());
}

void
FHoudiniAssetEditor::CreateEditorModeManager()
{
	/*check(ViewportPtr.IsValid());
	TSharedPtr<FEditorViewportClient> ViewportClient = ViewportPtr->GetViewportClient();
	check(ViewportClient.IsValid());
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ViewportClient->TakeOwnershipOfModeManager(EditorModeManager);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS*/
}

/*
void 
FHoudiniAssetEditor::SetHoudiniAssetBeingEdited(UHoudiniAsset* NewHDA)
{
	if ((NewHDA != HoudiniAssetBeingEdited) && (NewHDA != nullptr))
	{
		UHoudiniAsset* OldHDA = HoudiniAssetBeingEdited;
		HoudiniAssetBeingEdited = NewHDA;

		// Let the viewport know that we are editing something different
		//ViewportPtr->NotifySpriteBeingEditedHasChanged();

		// Let the editor know that are editing something different
		RemoveEditingObject(OldHDA);
		AddEditingObject(NewHDA);

		// Update the asset picker to select the new active sprite
		//SpriteListPtr->SelectAsset(NewHDA);

		InitCookableFromHoudiniAsset();
	}
}
*/

EHoudiniAssetEditorMode::Type
FHoudiniAssetEditor::GetCurrentMode() const
{
	return EHoudiniAssetEditorMode::ViewMode; //ViewportPtr->GetCurrentMode();
}

void 
FHoudiniAssetEditor::CreateModeToolbarWidgets(FToolBarBuilder& IgnoredBuilder)
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(ViewportPtr->GetCommandList(), FMultiBoxCustomization::None);
	/*ToolbarBuilder.AddToolBarButton(FSpriteEditorCommands::Get().EnterViewMode);
	ToolbarBuilder.AddToolBarButton(FSpriteEditorCommands::Get().EnterSourceRegionEditMode);
	ToolbarBuilder.AddToolBarButton(FSpriteEditorCommands::Get().EnterCollisionEditMode);
	ToolbarBuilder.AddToolBarButton(FSpriteEditorCommands::Get().EnterRenderingEditMode);*/
	AddToolbarWidget(ToolbarBuilder.MakeWidget());
}

FText 
FHoudiniAssetEditor::GetViewportCornerText() const
{
	switch (GetCurrentMode())
	{
	case EHoudiniAssetEditorMode::ViewMode:
		return LOCTEXT("ViewMode_CornerText", "View");
	case EHoudiniAssetEditorMode::HDAMode:
		return LOCTEXT("HDAMode_CornerText", "Edit HDA");
	case EHoudiniAssetEditorMode::SessionSyncMode:
		return LOCTEXT("SessionSyncMode_CornerText", "Session Sync");
	default:
		return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
