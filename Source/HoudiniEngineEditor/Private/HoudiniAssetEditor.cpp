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
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineStyle.h"
#include "SHoudiniAssetEditorViewport.h"
#include "SHoudiniNodeSyncPanel.h"


#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Commands/UICommandList.h"
#include "Materials/MaterialInstanceConstant.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "MaterialShared.h"
#endif
#include "PropertyEditorDelegates.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HoudiniAssetEditor"

const FName HoudiniAssetEditorAppName = FName(TEXT("HoudiniAssetEditorApp"));

struct FHoudiniAssetEditorTabs
{
	// Tab identifiers
	static const FName DetailsID;
	static const FName ViewportID;
	static const FName NodeSyncID;
	static const FName PreviewSceneSettingsID;
};

const FName FHoudiniAssetEditorTabs::DetailsID(TEXT("Details"));
const FName FHoudiniAssetEditorTabs::ViewportID(TEXT("Viewport"));
const FName FHoudiniAssetEditorTabs::NodeSyncID(TEXT("NodeSync"));
const FName FHoudiniAssetEditorTabs::PreviewSceneSettingsID(TEXT("PreviewSceneSettings"));

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

	// For Cookable details customization
	FOnGetDetailCustomizationInstance CustomizeHoudiniAssetForEditor = FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniCookableDetails::MakeInstance);
	PropertyView->RegisterInstancedCustomPropertyLayout(UHoudiniAsset::StaticClass(), CustomizeHoudiniAssetForEditor);

	/*
	// TODO: Offer modes for HDA editor??
	TAttribute<EHoudiniAssetEditorMode::Type> HoudiniAssetEditorMode = TAttribute<EHoudiniAssetEditorMode::Type>::Create(
		TAttribute<EHoudiniAssetEditorMode::Type>::FGetter::CreateSP(InHoudiniAssetEditor.ToSharedRef(), &FHoudiniAssetEditor::GetCurrentMode));
	*/

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
	// In order to be able to specify the identifier for this editor manually,
	// we had to directly use the  FPropertyEditorModule's create function instead of
	// using the SSingleObjectDetailsPanel function.
	// This prevents us from setting bAutoObserveObject on the SSingleObjectDetailsPanel...
	// ... so reproduce its behavior here...
	// see SSingleObjectDetailsPanel::Tick()

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
			DetailsTabPtr.ToSharedRef()
		];
}

TSharedRef<SDockTab> 
FHoudiniAssetEditor::SpawnNodeSyncTab(const FSpawnTabArgs& Args)
{
	// Set the Node Sync panel to the AssetEditor mode
	//NodeSyncPanel->SetIsAssetEditorPanel(true);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("NodeSyncTabTitle", "Node Sync"))
		[
			SAssignNew(NodeSyncPanel, SHoudiniNodeSyncPanel)
			.IsAssetEditor(true)
		];
	
	SpawnedTab->SetTabIcon(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngineLogo"));

	return SpawnedTab;
}

TSharedRef<SDockTab> 
FHoudiniAssetEditor::SpawnPreviewSceneSettingsTab(const FSpawnTabArgs& Args)
{
	FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");

	TArray<FAdvancedPreviewSceneModule::FDetailDelegates> Delegates;
	Delegates.Add({ OnPreviewSceneChangedDelegate });
	AdvancedPreviewSettingsWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(
		ViewportPtr->GetPreviewScene(),
		nullptr,
		TArray<FAdvancedPreviewSceneModule::FDetailCustomizationInfo>(),
		TArray<FAdvancedPreviewSceneModule::FPropertyTypeCustomizationInfo>(),
		Delegates);

	//check(Args.GetTabId() == PreviewSceneSettingsTabId);
	return SAssignNew(PreviewSceneDockTab, SDockTab)
		.Label(LOCTEXT("PReviewSceneSettingsTabTitle", "Preview Scene Settings"))
		[
			AdvancedPreviewSettingsWidget.IsValid() ? AdvancedPreviewSettingsWidget.ToSharedRef() : SNullWidget::NullWidget
		];
}

void
FHoudiniAssetEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_HoudiniAssetEditor", "Houdini Asset Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	// VIEWPORT
	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	// DETAILS
	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	/*
	// NODE SYNC
	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::NodeSyncID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnNodeSyncTab))
		.SetDisplayName(LOCTEXT("NodeSyncTabLabel", "Node Sync"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ContentBrowser"));
	*/

	// PREVIEW SCENE SETTINGS
	InTabManager->RegisterTabSpawner(FHoudiniAssetEditorTabs::PreviewSceneSettingsID, FOnSpawnTab::CreateSP(this, &FHoudiniAssetEditor::SpawnPreviewSceneSettingsTab))
		.SetDisplayName(LOCTEXT("PreviewSceneTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 3)
		.SetReadOnlyBehavior(ETabReadOnlyBehavior::Custom);
#else
		;
#endif
}

void 
FHoudiniAssetEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::ViewportID);
	InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::DetailsID);
	//InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::NodeSyncID);
	InTabManager->UnregisterTabSpawner(FHoudiniAssetEditorTabs::PreviewSceneSettingsID);
}

void 
FHoudiniAssetEditor::InitHoudiniAssetEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<class IToolkitHost>& InitToolkitHost,
	class UHoudiniAsset* InitHDA)
{
	HoudiniAssetBeingEdited = InitHDA;

	bIsViewingCopHDA = false;

	bShowRedChannel = true;
	bShowGreenChannel = true;
	bShowBlueChannel = true;
	bShowAlphaChannel = true;

	SelectedTextureOutput = 0;
	NumTextureOutputs = 0;

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
			// Set supported features in the asset editor
			HoudiniCookableBeingEdited->SetHoudiniAssetSupported(true);
			HoudiniCookableBeingEdited->SetParameterSupported(true);
			HoudiniCookableBeingEdited->SetInputSupported(true);
			HoudiniCookableBeingEdited->SetOutputSupported(true);
			HoudiniCookableBeingEdited->SetComponentSupported(true);
			HoudiniCookableBeingEdited->SetBakingSupported(true);

			// NO PDG - NO PROXIES
			HoudiniCookableBeingEdited->SetPDGSupported(false);
			HoudiniCookableBeingEdited->SetProxySupported(false);

			// Change the default bake type
			HoudiniCookableBeingEdited->SetHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToAsset);

			// Let the cookable know its used in an Houdini Asset Editor
			HoudiniCookableBeingEdited->AssetEditorId = FName(*HoudiniAssetEditorIdentifier);


			HoudiniCookableBeingEdited->GetOnPostOutputProcessingDelegate().AddRaw(this, &FHoudiniAssetEditor::OnPostOutputProcess);

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
FHoudiniAssetEditor::OnPostOutputProcess(UHoudiniCookable* _HC, bool  bSuccess)
{
	// See if this Cookable is texture only
	bool bTextureOnly = true;
	NumTextureOutputs = 0;
	for (int32 OutputIdx = 0; OutputIdx < _HC->GetNumOutputs(); OutputIdx++)
	{
		UHoudiniOutput* CurOutput = _HC->GetOutputAt(OutputIdx);
		if (!IsValid(CurOutput))
			continue;

		if (CurOutput->GetType() != EHoudiniOutputType::Cop)
			bTextureOnly = false;

		NumTextureOutputs++;
	}

	if (bTextureOnly)
	{
		if (!bIsViewingCopHDA)
		{
			// Switch viewport to texture
			ViewportPtr->GetViewportClient()->SetViewportTo2D();
			bIsViewingCopHDA = true;
		}
	}
	else
	{
		if (bIsViewingCopHDA)
		{
			// Switch viewport to 3D
			ViewportPtr->GetViewportClient()->SetViewportTo3D();
			bIsViewingCopHDA = false;
		}
	}

	// Update available 2d outputs
	UpdateOutputList();

	UpdateTextureOutputOnPreviewMesh();

	ViewportPtr->Invalidate();

}

void
FHoudiniAssetEditor::OnClose()
{
	// Unregister our Cookable
	FHoudiniEngineRuntime::Get().UnRegisterHoudiniCookable(HoudiniCookableBeingEdited);

	// Unregister our Details Identifier
	FHoudiniEngine::Get().UnRegisterHoudiniAssetEditor(HoudiniAssetEditorIdentifier);

	// TODO: 
	// Check if we need to manually clean up the scene / delete HAA
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
	return TEXT("https://www.sidefx.com/docs/houdini/unreal/");
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

void 
FHoudiniAssetEditor::ExtendMenu()
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
}


void
FHoudiniAssetEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		ViewportPtr->GetCommandList(),
		FToolBarExtensionDelegate::CreateSP(this, &FHoudiniAssetEditor::FillToolbar)
	);

	AddToolbarExtender(ToolbarExtender);
}

void
FHoudiniAssetEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	TSharedRef<SWidget> ChannelControl = MakeChannelControlWidget();
	TSharedRef<SWidget> TextureOutput = MakeTextureOutputWidget();

	ToolbarBuilder.BeginSection("Channels");
	{
		ToolbarBuilder.AddWidget(ChannelControl);
		ToolbarBuilder.AddWidget(TextureOutput);
	}
	ToolbarBuilder.EndSection();
}


FSlateColor
FHoudiniAssetEditor::GetChannelButtonBackgroundColor(ETextureChannelButton Button) const
{
	FSlateColor Dropdown = FAppStyle::Get().GetSlateColor("Colors.Dropdown");

	switch (Button)
	{
	case ETextureChannelButton::Red:
		return bShowRedChannel ? FLinearColor::Red : FLinearColor::White;
	case ETextureChannelButton::Green:
		return bShowGreenChannel ? FLinearColor::Green : FLinearColor::White;
	case ETextureChannelButton::Blue:
		return bShowBlueChannel ? FLinearColor::Blue : FLinearColor::White;
	case ETextureChannelButton::Alpha:
		return FLinearColor::White;
	default:
		check(false);
		return FSlateColor();
	}
}

FSlateColor
FHoudiniAssetEditor::GetChannelButtonForegroundColor(ETextureChannelButton Button) const
{
	FSlateColor DefaultForeground = FAppStyle::Get().GetSlateColor("Colors.Foreground");

	switch (Button)
	{
	case ETextureChannelButton::Red:
		return bShowRedChannel ? FLinearColor::Black : DefaultForeground;
	case ETextureChannelButton::Green:
		return bShowGreenChannel ? FLinearColor::Black : DefaultForeground;
	case ETextureChannelButton::Blue:
		return bShowBlueChannel ? FLinearColor::Black : DefaultForeground;
	case ETextureChannelButton::Alpha:
		return bShowAlphaChannel ? FLinearColor::Black : DefaultForeground;
	default:
		check(false);
		return FSlateColor::UseForeground();
	}
}

ECheckBoxState
FHoudiniAssetEditor::OnGetChannelButtonCheckState(ETextureChannelButton Button) const
{
	switch (Button)
	{
	case ETextureChannelButton::Red:
		return bShowRedChannel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	case ETextureChannelButton::Green:
		return bShowGreenChannel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	case ETextureChannelButton::Blue:
		return bShowBlueChannel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	case ETextureChannelButton::Alpha:
		return bShowAlphaChannel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	default:
		check(false);
		return ECheckBoxState::Unchecked;
	}
}

TSharedRef<SWidget> 
FHoudiniAssetEditor::MakeChannelControlWidget()
{
	auto OnChannelCheckStateChanged = [this](ECheckBoxState NewState, ETextureChannelButton Button)
	{
		switch (Button)
		{
			case ETextureChannelButton::Red:
				bShowRedChannel = !bShowRedChannel;
				break;
			case ETextureChannelButton::Green:
				bShowGreenChannel = !bShowGreenChannel;
				break;
			case ETextureChannelButton::Blue:
				bShowBlueChannel = !bShowBlueChannel;
				break;
			case ETextureChannelButton::Alpha:
				bShowAlphaChannel = !bShowAlphaChannel;
				break;
			default:
				check(false);
				break;
		}

		UpdateColorChannelsOnPreviewMesh();
	};

	auto GetChannelVisibilty = [this]()
	{
		return bIsViewingCopHDA ? EVisibility::Visible : EVisibility::Hidden;
	};

	TSharedRef<SWidget> ChannelControl = 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "TextureEditor.ChannelButtonStyle")
			.BorderBackgroundColor(this, &FHoudiniAssetEditor::GetChannelButtonBackgroundColor, ETextureChannelButton::Red)
			.ForegroundColor(this, &FHoudiniAssetEditor::GetChannelButtonForegroundColor, ETextureChannelButton::Red)
			.OnCheckStateChanged_Lambda(OnChannelCheckStateChanged, ETextureChannelButton::Red)
			.IsChecked(this, &FHoudiniAssetEditor::OnGetChannelButtonCheckState, ETextureChannelButton::Red)
			//.IsEnabled(this, &FTextureEditorToolkit::IsChannelButtonEnabled, ETextureChannelButton::Red)
			.Visibility_Lambda(GetChannelVisibilty)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("TextureEditor.ChannelButtonFont"))
				.Text(FText::FromString("R"))
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "TextureEditor.ChannelButtonStyle")
			.BorderBackgroundColor(this, &FHoudiniAssetEditor::GetChannelButtonBackgroundColor, ETextureChannelButton::Green)
			.ForegroundColor(this, &FHoudiniAssetEditor::GetChannelButtonForegroundColor, ETextureChannelButton::Green)
			.OnCheckStateChanged_Lambda(OnChannelCheckStateChanged, ETextureChannelButton::Green)
			.IsChecked(this, &FHoudiniAssetEditor::OnGetChannelButtonCheckState, ETextureChannelButton::Green)
			//.IsEnabled(this, &FTextureEditorToolkit::IsChannelButtonEnabled, ETextureChannelButton::Green)
			.Visibility_Lambda(GetChannelVisibilty)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("TextureEditor.ChannelButtonFont"))
				.Text(FText::FromString("G"))
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "TextureEditor.ChannelButtonStyle")
			.BorderBackgroundColor(this, &FHoudiniAssetEditor::GetChannelButtonBackgroundColor, ETextureChannelButton::Blue)
			.ForegroundColor(this, &FHoudiniAssetEditor::GetChannelButtonForegroundColor, ETextureChannelButton::Blue)
			.OnCheckStateChanged_Lambda(OnChannelCheckStateChanged, ETextureChannelButton::Blue)
			.IsChecked(this, &FHoudiniAssetEditor::OnGetChannelButtonCheckState, ETextureChannelButton::Blue)
			//.IsEnabled(this, &FTextureEditorToolkit::IsChannelButtonEnabled, ETextureChannelButton::Blue)
			.Visibility_Lambda(GetChannelVisibilty)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("TextureEditor.ChannelButtonFont"))
				.Text(FText::FromString("B"))
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "TextureEditor.ChannelButtonStyle")
			.BorderBackgroundColor(this, &FHoudiniAssetEditor::GetChannelButtonBackgroundColor, ETextureChannelButton::Alpha)
			.ForegroundColor(this, &FHoudiniAssetEditor::GetChannelButtonForegroundColor, ETextureChannelButton::Alpha)
			.OnCheckStateChanged_Lambda(OnChannelCheckStateChanged, ETextureChannelButton::Alpha)
			.IsChecked(this, &FHoudiniAssetEditor::OnGetChannelButtonCheckState, ETextureChannelButton::Alpha)
			//.IsEnabled(this, &FTextureEditorToolkit::IsChannelButtonEnabled, ETextureChannelButton::Alpha)
			.Visibility_Lambda(GetChannelVisibilty)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("TextureEditor.ChannelButtonFont"))
				.Text(FText::FromString("A"))
			]
		];

	return ChannelControl;
}


void
FHoudiniAssetEditor::UpdateColorChannelsOnPreviewMesh()
{
	// No need to do anything if we aren't viewing a COP
	if (!bIsViewingCopHDA)
		return;

	// Get our cookable's scene component
	USceneComponent* CookableComponent = 
		HoudiniCookableBeingEdited ? HoudiniCookableBeingEdited->GetComponent() : nullptr;
	if (!CookableComponent)
		return;

	// Get the COP SM
	UStaticMesh* HoudiniCOPMesh = FHoudiniEngine::Get().GetHoudiniCOPStaticMesh().Get();
	if (!HoudiniCOPMesh)
		return;

	// Update context for generated materials (will trigger when the object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : CookableComponent->GetAttachChildren())
	{
		if (!IsValid(CurrentSceneComp) || !CurrentSceneComp->IsA<UStaticMeshComponent>())
			continue;

		// Get the static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CurrentSceneComp);
		if (!IsValid(SMC))
			continue;

		// Check if the SMC is the Houdini Logo
		if (SMC->GetStaticMesh() != HoudiniCOPMesh)
			continue;

		UMaterialInstanceConstant* MaterialInstance = 
			Cast<UMaterialInstanceConstant>(SMC->GetMaterial(0));
		if (!MaterialInstance)
			continue;

		// Apply material instance parameters
		MaterialInstance->SetStaticSwitchParameterValueEditorOnly(FName("R"), bShowRedChannel);
		MaterialInstance->SetStaticSwitchParameterValueEditorOnly(FName("G"), bShowGreenChannel);
		MaterialInstance->SetStaticSwitchParameterValueEditorOnly(FName("B"), bShowBlueChannel);
		MaterialInstance->SetStaticSwitchParameterValueEditorOnly(FName("A"), bShowAlphaChannel);

		MaterialUpdateContext.AddMaterialInstance(MaterialInstance);
	}
}

void
FHoudiniAssetEditor::UpdateTextureOutputOnPreviewMesh()
{
	// No need to do anything if we aren't viewing a COP
	if (!bIsViewingCopHDA)
		return;

	// Get our cookable's scene component
	USceneComponent* CookableComponent =
		HoudiniCookableBeingEdited ? HoudiniCookableBeingEdited->GetComponent() : nullptr;
	if (!CookableComponent)
		return;

	// Get the COP SM
	UStaticMesh* HoudiniCOPMesh = FHoudiniEngine::Get().GetHoudiniCOPStaticMesh().Get();
	if (!HoudiniCOPMesh)
		return;

	// Update context for generated materials (will trigger when the object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	// Get the texture for the selected output
	UTexture2D* SelectedTexture = nullptr;
	UHoudiniOutput* CurOutput = HoudiniCookableBeingEdited->GetOutputAt(SelectedTextureOutput);
	if (IsValid(CurOutput) && CurOutput->GetType() == EHoudiniOutputType::Cop)
	{
		for (auto& It : CurOutput->GetOutputObjects())
		{
			// ... Get the first valid texture for display purpose
			SelectedTexture = Cast<UTexture2D>(It.Value.OutputObject);
			if (IsValid(CurOutput))
				break;
		}
	}

	// Fully stream in the texture before drawing it.
	// Not doing this would cause the texture to appear blurry in the ortho viewport
	if (SelectedTexture)
	{
		SelectedTexture->SetForceMipLevelsToBeResident(30.0f);
		SelectedTexture->WaitForStreaming();
	}

	// Iterate on the HAC's component	
	for (USceneComponent* CurrentSceneComp : CookableComponent->GetAttachChildren())
	{
		if (!IsValid(CurrentSceneComp) || !CurrentSceneComp->IsA<UStaticMeshComponent>())
			continue;

		// Get the static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CurrentSceneComp);
		if (!IsValid(SMC))
			continue;

		// Check if the SMC is the Houdini Logo
		if (SMC->GetStaticMesh() != HoudiniCOPMesh)
			continue;

		UMaterialInstanceConstant* MaterialInstance =
			Cast<UMaterialInstanceConstant>(SMC->GetMaterial(0));
		if (!MaterialInstance)
			continue;

		// Apply material instance parameters
		FName MatParamName = FName("cop");
		MaterialInstance->SetTextureParameterValueEditorOnly(MatParamName, SelectedTexture);

		MaterialUpdateContext.AddMaterialInstance(MaterialInstance);
	}
}

TSharedRef<SWidget> 
FHoudiniAssetEditor::MakeTextureOutputWidget()
{
	auto GetOutputVisibility = [this]()
	{
		if(!bIsViewingCopHDA)
			return EVisibility::Hidden;

		// Only show output selector if we have more than one texture output
		if(NumTextureOutputs <= 1)
			return EVisibility::Hidden;

		return EVisibility::Visible;
	};

	// Lambda for changing output
	auto OnSelChanged = [this](TSharedPtr<FString> InNewChoice)
	{
		if (!InNewChoice.IsValid())
			return;
		
		FString NewChoiceStr = *InNewChoice.Get();
		for (int32 OutputIdx = 0; OutputIdx < HoudiniCookableBeingEdited->GetNumOutputs(); OutputIdx++)
		{
			UHoudiniOutput* CurOutput = HoudiniCookableBeingEdited->GetOutputAt(OutputIdx);
			if (!IsValid(CurOutput))
				continue;

			if (CurOutput->GetType() != EHoudiniOutputType::Cop)
				continue;

			for (auto& HGPO : CurOutput->GetHoudiniGeoPartObjects())
			{
				if (HGPO.PartName != NewChoiceStr)
					continue;

				SelectedTextureOutput = OutputIdx;
			}

			// Update the selected texture output
			UpdateTextureOutputOnPreviewMesh();
		}
	};

	TSharedPtr<FString> InitiallySelectedOutput = OutputList.Num() > 0 ? OutputList[0] : nullptr;
	TSharedRef<SWidget> OutputControl = 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		.AutoWidth()
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&OutputList)
			.InitiallySelectedItem(InitiallySelectedOutput)
			.Visibility_Lambda(GetOutputVisibility)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnSelectionChanged_Lambda([=](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
			{
				return OnSelChanged(NewChoice);
			})
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (OutputList.IsValidIndex(SelectedTextureOutput))
						return FText::FromString(*OutputList[SelectedTextureOutput].Get());
					else
						return FText::FromString(FString::FromInt(SelectedTextureOutput));
				})
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
		

	return OutputControl;
}

void
FHoudiniAssetEditor::UpdateOutputList()
{
	OutputList.Reset();
	for (int32 OutputIdx = 0; OutputIdx < HoudiniCookableBeingEdited->GetNumOutputs(); OutputIdx++)
	{
		UHoudiniOutput* CurOutput = HoudiniCookableBeingEdited->GetOutputAt(OutputIdx);
		if (!IsValid(CurOutput))
			continue;

		if (CurOutput->GetType() != EHoudiniOutputType::Cop)
			continue;

		for (auto& HGPO : CurOutput->GetHoudiniGeoPartObjects())
			OutputList.Add(MakeShareable(new FString(HGPO.PartName)));
	}
}
/*
void
FHoudiniAssetEditor::CreateEditorModeManager()
{
	check(ViewportPtr.IsValid());
	TSharedPtr<FEditorViewportClient> ViewportClient = ViewportPtr->GetViewportClient();
	check(ViewportClient.IsValid());
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ViewportClient->TakeOwnershipOfModeManager(EditorModeManager);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
*/

/*
EHoudiniAssetEditorMode::Type
FHoudiniAssetEditor::GetCurrentMode() const
{
	return EHoudiniAssetEditorMode::ViewMode;
}
*/

void 
FHoudiniAssetEditor::CreateModeToolbarWidgets(FToolBarBuilder& IgnoredBuilder)
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(ViewportPtr->GetCommandList(), FMultiBoxCustomization::None);
	AddToolbarWidget(ToolbarBuilder.MakeWidget());
}

FText 
FHoudiniAssetEditor::GetViewportCornerText() const
{
	return LOCTEXT("HDA_CornerText", "HDA");

	/*
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
	*/
}


#undef LOCTEXT_NAMESPACE
