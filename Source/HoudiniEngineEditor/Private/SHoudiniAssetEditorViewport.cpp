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

#include "SHoudiniAssetEditorViewport.h"

#include "HoudiniAssetEditorViewportClient.h"
#include "HoudiniAssetEditor.h"

#include "AssetEditorModeManager.h"
#include "Components/PostProcessComponent.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewScene.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewSceneModule.h"
#include "PreviewProfileController.h"
#include "ToolMenus.h"
#include "Widgets/SViewport.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	#include "AdvancedPreviewSceneMenus.h"
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

//-----------------------------------------------------------------------------
// SHoudiniAssetEditorViewport
//-----------------------------------------------------------------------------
void
SHoudiniAssetEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(HoudiniCookable);
}

TSharedRef<class SEditorViewport>
SHoudiniAssetEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender>
SHoudiniAssetEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void 
SHoudiniAssetEditorViewport::OnFloatingButtonClicked()
{
	// Nothing to do
}

// Create the advanced preview scene and initiate our component?
SHoudiniAssetEditorViewport::SHoudiniAssetEditorViewport()
	: PreviewScene(MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues())))
{
	// Nothing to do
}

SHoudiniAssetEditorViewport::~SHoudiniAssetEditorViewport() 
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->Viewport = NULL;
	}
}

void
SHoudiniAssetEditorViewport::Construct(const FArguments& InArgs, TSharedPtr<FHoudiniAssetEditor> InHoudiniAssetEditor)
{
	HoudiniAssetEditorPtr = InHoudiniAssetEditor;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void 
SHoudiniAssetEditorViewport::Construct(const FArguments& InArgs)
{
	SEditorViewport::Construct(SEditorViewport::FArguments());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	UE::AdvancedPreviewScene::BindDefaultOnSettingsChangedHandler(PreviewScene, TypedViewportClient);
#endif
}


TSharedRef<FEditorViewportClient>
SHoudiniAssetEditorViewport::MakeEditorViewportClient()
{
	TypedViewportClient = MakeShareable(new FHoudiniAssetEditorViewportClient(SharedThis(this), PreviewScene.ToSharedRef()));

	TypedViewportClient->ToggleOrbitCamera(true);

	return TypedViewportClient.ToSharedRef(); 
}

void 
SHoudiniAssetEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();
}

EVisibility 
SHoudiniAssetEditorViewport::GetTransformToolbarVisibility() const
{
	return EVisibility::Visible;
}

void 
SHoudiniAssetEditorViewport::OnFocusViewportToSelection()
{

}

void
SHoudiniAssetEditorViewport::SetHoudiniAsset(UHoudiniAsset* InAsset)
{
	if (!InAsset)
		return;

	TypedViewportClient->SetHoudiniAsset(InAsset);
}

FText 
SHoudiniAssetEditorViewport::GetTitleText() const
{
	return FText::FromString("Houdini Asset Editor");
}


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
TSharedPtr<SWidget> 
SHoudiniAssetEditorViewport::BuildViewportToolbar()
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	// Register the viewport toolbar if another viewport hasn't already (it's shared).
	const FName ViewportToolbarName = "HoudiniAssetEditor.ViewportToolbar";
	if (!UToolMenus::Get()->IsMenuRegistered(ViewportToolbarName))
	{
		UToolMenu* const ViewportToolbarMenu = UToolMenus::Get()->RegisterMenu(
			ViewportToolbarName, NAME_None /* parent */, EMultiBoxType::SlimHorizontalToolBar
		);

		ViewportToolbarMenu->StyleName = "ViewportToolbar";

		
		// Add the left-aligned part of the viewport toolbar.
		{
			FToolMenuSection& LeftSection = ViewportToolbarMenu->AddSection("Left");

			// We don't need transform/snapping settings for now
			//LeftSection.AddEntry(UE::UnrealEd::CreateTransformsSubmenu());
			//LeftSection.AddEntry(UE::UnrealEd::CreateSnappingSubmenu());
		}

		// Add the right-aligned part of the viewport toolbar.
		{
			FToolMenuSection& RightSection = ViewportToolbarMenu->AddSection("Right");
			RightSection.Alignment = EToolMenuSectionAlign::Last;

			// Add the "Camera" submenu.
			RightSection.AddEntry(UE::UnrealEd::CreateCameraSubmenu(UE::UnrealEd::FViewportCameraMenuOptions().ShowAll()));

			// Add the "View Modes" sub menu.
			{
				// Stay backward-compatible with the old viewport toolbar.
				{
					const FName ParentSubmenuName = "UnrealEd.ViewportToolbar.View";
					// Create our parent menu.
					if (!UToolMenus::Get()->IsMenuRegistered(ParentSubmenuName))
					{
						UToolMenus::Get()->RegisterMenu(ParentSubmenuName);
					}

					// Register our ToolMenu here first, before we create the submenu, so we can set our parent.
					UToolMenus::Get()->RegisterMenu("HoudiniAssetEditor.ViewportToolbar.ViewModes", ParentSubmenuName);
				}
				RightSection.AddEntry(UE::UnrealEd::CreateViewModesSubmenu());
			}

			// Add the performance and scalability settings
			RightSection.AddEntry(UE::UnrealEd::CreatePerformanceAndScalabilitySubmenu());

			// Add the Preview Scene setting submenu
			{
				const FName PreviewSceneMenuName = "HoudiniAssetEditor.ViewportToolbar.AssetViewerProfile";
				RightSection.AddEntry(UE::UnrealEd::CreateAssetViewerProfileSubmenu());
				UE::AdvancedPreviewScene::Menus::ExtendAdvancedPreviewSceneSettings(PreviewSceneMenuName);
				UE::UnrealEd::ExtendPreviewSceneSettingsWithTabEntry(PreviewSceneMenuName);
			}
		}
	}

	FToolMenuContext ViewportToolbarContext;
	{
		ViewportToolbarContext.AppendCommandList(PreviewScene->GetCommandList());
		ViewportToolbarContext.AppendCommandList(GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject =
				UE::UnrealEd::CreateViewportToolbarDefaultContext(SharedThis(this));

			ContextObject->bShowCoordinateSystemControls = false;

			ContextObject->AssetEditorToolkit = HoudiniAssetEditorPtr;
			ContextObject->PreviewSettingsTabId = FName(TEXT("PreviewSceneSettings"));
			ViewportToolbarContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(ViewportToolbarName, ViewportToolbarContext);
#else
	// Register the viewport toolbar if another viewport hasn't already (it's shared).
	const FName ViewportToolbarName = "HoudiniAssetEditor.ViewportToolbar";

	if (!UToolMenus::Get()->IsMenuRegistered(ViewportToolbarName))
	{
		UToolMenu* const ViewportToolbarMenu = UToolMenus::Get()->RegisterMenu(
			ViewportToolbarName, NAME_None /* parent */, EMultiBoxType::SlimHorizontalToolBar
		);

		ViewportToolbarMenu->StyleName = "ViewportToolbar";

		// Add the right-aligned part of the viewport toolbar.
		{
			// Add the submenus of this section as EToolMenuInsertType::Last to sort them after any
			// default-positioned submenus external code might add.
			FToolMenuSection& RightSection = ViewportToolbarMenu->FindOrAddSection("Right");
			RightSection.Alignment = EToolMenuSectionAlign::Last;

			// Add the "Camera" submenu.
			{
				const FName GrandParentSubmenuName = "UnrealEd.ViewportToolbar.Camera";
				const FName ParentSubmenuName = "HoudiniAssetEditor.ViewportToolbar.Camera";
				const FName SubmenuName = "HoudiniAssetEditor.ViewportToolbar.CameraOptions";

				// Create our grandparent menu.
				if (!UToolMenus::Get()->IsMenuRegistered(GrandParentSubmenuName))
				{
					UToolMenus::Get()->RegisterMenu(GrandParentSubmenuName);
				}

				// Create our parent menu.
				if (!UToolMenus::Get()->IsMenuRegistered(ParentSubmenuName))
				{
					UToolMenus::Get()->RegisterMenu(ParentSubmenuName, GrandParentSubmenuName);
				}

				// Create our menu.
				UToolMenus::Get()->RegisterMenu(SubmenuName, ParentSubmenuName);

				UE::UnrealEd::ExtendCameraSubmenu(SubmenuName);

				FToolMenuEntry CameraSubmenu = UE::UnrealEd::CreateViewportToolbarCameraSubmenu();
				CameraSubmenu.InsertPosition.Position = EToolMenuInsertType::First;
				RightSection.AddEntry(CameraSubmenu);
			}

			// Add the "View Modes" sub menu.
			{
				// Stay backward-compatible with the old viewport toolbar.
				{
					const FName ParentSubmenuName = "UnrealEd.ViewportToolbar.View";
					// Create our parent menu.
					if (!UToolMenus::Get()->IsMenuRegistered(ParentSubmenuName))
					{
						UToolMenus::Get()->RegisterMenu(ParentSubmenuName);
					}

					// Register our ToolMenu here first, before we create the submenu, so we can set our parent.
					UToolMenus::Get()->RegisterMenu("HoudiniAssetEditor.ViewportToolbar.ViewModes", ParentSubmenuName);
				}

				FToolMenuEntry ViewModesSubmenu = UE::UnrealEd::CreateViewportToolbarViewModesSubmenu();
				ViewModesSubmenu.InsertPosition.Position = EToolMenuInsertType::Last;
				RightSection.AddEntry(ViewModesSubmenu);
			}

			// Add the "Performance and Scalability" sub menu.
			{
				FToolMenuEntry PerformanceAndScalabilitySubmenu = UE::UnrealEd::CreatePerformanceAndScalabilitySubmenu();
				PerformanceAndScalabilitySubmenu.InsertPosition.Position = EToolMenuInsertType::Last;
				RightSection.AddEntry(PerformanceAndScalabilitySubmenu);
			}

			/*
			// Add the "Preview Profile" sub menu.
			{
				//PreviewProfileController = MakeShared<FPreviewProfileController>();
				FToolMenuEntry PreviewProfileSubmenu =
					UE::UnrealEd::CreateViewportToolbarAssetViewerProfileSubmenu(MakeShared<FPreviewProfileController>());
				PreviewProfileSubmenu.InsertPosition.Position = EToolMenuInsertType::Last;
				RightSection.AddEntry(PreviewProfileSubmenu);
			}
			*/
		}
	}

	FToolMenuContext ViewportToolbarContext;
	{
		ViewportToolbarContext.AppendCommandList(GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject =
				UE::UnrealEd::CreateViewportToolbarDefaultContext(SharedThis(this));

			ViewportToolbarContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(ViewportToolbarName, ViewportToolbarContext);
#endif
}
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
TSharedPtr<IPreviewProfileController> 
SHoudiniAssetEditorViewport::CreatePreviewProfileController()
{
	return MakeShared<FPreviewProfileController>();
}
#endif
#undef LOCTEXT_NAMESPACE
