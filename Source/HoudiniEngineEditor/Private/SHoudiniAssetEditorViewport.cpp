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

#include "AssetEditorModeManager.h"
#include "Components/PostProcessComponent.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewScene.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewSceneModule.h"
#include "Editor/LevelEditor/Private/SLevelViewportToolBar.h"
#include "Widgets/SViewport.h"


//////////////////////////////////////////////////////////////////////////
// SHoudiniAssetEditorViewport
/*
class SHoudiniAssetEditorViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SHoudiniAssetEditorViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FHoudiniAssetEditor> InSpriteEditor);

	// SEditorViewport interface
	virtual void BindCommands() override;
	//virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	//virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility GetTransformToolbarVisibility() const override;
	virtual void OnFocusViewportToSelection() override;
	// End of SEditorViewport interface

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	//virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface
	/*
	// Invalidate any references to the HDA being edited; it has changed
	void NotifyHoudiniAssetBeingEditedHasChanged()
	{
		EditorViewportClient->NotifyHoudiniAssetBeingEditedHasChanged();
	}

	EHoudiniAssetEditorMode::Type GetCurrentMode() const
	{
		return EditorViewportClient->GetCurrentMode();
	}

	void ActivateEditMode()
	{
		EditorViewportClient->ActivateEditMode();
	}

private:
	// Pointer back to owning asset editor instance (the keeper of state)
	TWeakPtr<class FHoudiniAssetEditor> HoudiniAssetEditorPtr;

	// Viewport client
	//TSharedPtr<FHoudiniAssetEditorViewportClient> EditorViewportClient;
};

void 
SHoudiniAssetEditorViewport::Construct(const FArguments& InArgs, TSharedPtr<FHoudiniAssetEditor> InHoudiniAssetEditor)
{
	HoudiniAssetEditorPtr = InHoudiniAssetEditor;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void 
SHoudiniAssetEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FHoudiniEngineCommands& Commands = FHoudiniEngineCommands::Get();

	//TSharedRef<FHoudiniAssetEditorViewportClient> EditorViewportClientRef = EditorViewportClient.ToSharedRef();

	/*
	// Show toggles
	CommandList->MapAction(
		Commands.SetShowGrid,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::SetShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowGridChecked));

	CommandList->MapAction(
		Commands.SetShowSourceTexture,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::ToggleShowSourceTexture),
		FCanExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::CanShowSourceTexture),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsShowSourceTextureChecked));

	CommandList->MapAction(
		Commands.ExtractSprites,
		FExecuteAction::CreateSP(this, &SSpriteEditorViewport::ShowExtractSpritesDialog),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInSourceRegionEditMode));

	CommandList->MapAction(
		Commands.ToggleShowRelatedSprites,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::ToggleShowRelatedSprites),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsShowRelatedSpritesChecked),
		FIsActionButtonVisible::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInSourceRegionEditMode));

	CommandList->MapAction(
		Commands.ToggleShowSpriteNames,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::ToggleShowSpriteNames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsShowSpriteNamesChecked),
		FIsActionButtonVisible::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInSourceRegionEditMode));

	CommandList->MapAction(
		Commands.SetShowBounds,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::ToggleShowBounds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowBoundsChecked));

	CommandList->MapAction(
		Commands.SetShowCollision,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::SetShowCollision),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowCollisionChecked));

	CommandList->MapAction(
		Commands.SetShowMeshEdges,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::ToggleShowMeshEdges),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsShowMeshEdgesChecked));

	CommandList->MapAction(
		Commands.SetShowSockets,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::ToggleShowSockets),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsShowSocketsChecked));

	CommandList->MapAction(
		Commands.SetShowPivot,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::ToggleShowPivot),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsShowPivotChecked));

	// Editing modes
	CommandList->MapAction(
		Commands.EnterViewMode,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::EnterViewMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInViewMode));
	CommandList->MapAction(
		Commands.EnterSourceRegionEditMode,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::EnterSourceRegionEditMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInSourceRegionEditMode));
	CommandList->MapAction(
		Commands.EnterCollisionEditMode,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::EnterCollisionEditMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInCollisionEditMode));
	CommandList->MapAction(
		Commands.EnterRenderingEditMode,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::EnterRenderingEditMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FSpriteEditorViewportClient::IsInRenderingEditMode));

	*//*
}
/*
TSharedRef<FEditorViewportClient>
SHoudiniAssetEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FSpriteEditorViewportClient(SpriteEditorPtr, SharedThis(this)));

	return EditorViewportClient.ToSharedRef();
}
*/
/*
TSharedPtr<SWidget> 
SHoudiniAssetEditorViewport::MakeViewportToolbar()
{
	return SNew(SSpriteEditorViewportToolbar, SharedThis(this));
}
*/
/*

EVisibility 
SHoudiniAssetEditorViewport::GetTransformToolbarVisibility() const
{
	return EVisibility::Visible;
}

void 
SHoudiniAssetEditorViewport::OnFocusViewportToSelection()
{
	//EditorViewportClient->RequestFocusOnSelection(/*bInstant=*//* false);
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
/*
void
SHoudiniAssetEditorViewport::OnFloatingButtonClicked()
{
}
*/
/*
void 
SHoudiniAssetEditorViewport::ShowExtractSpritesDialog()
{
	if (UPaperSprite* Sprite = SpriteEditorPtr.Pin()->GetSpriteBeingEdited())
	{
		if (UTexture2D* SourceTexture = Sprite->GetSourceTexture())
		{
			SPaperExtractSpritesDialog::ShowWindow(SourceTexture);
		}
	}
}*/

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
	// Nothing
}

// Create the advanced preview scene and initiate our component?
SHoudiniAssetEditorViewport::SHoudiniAssetEditorViewport()
	: PreviewScene(MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues())))
{
	// TODO: Nothing!
	//HoudiniAssetComponent = NewObject<UHoudiniAssetComponent>();
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
}


TSharedRef<FEditorViewportClient>
SHoudiniAssetEditorViewport::MakeEditorViewportClient()
{
	TypedViewportClient = MakeShareable(new FHoudiniAssetEditorViewportClient(SharedThis(this), PreviewScene.ToSharedRef()));
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

	/*
	HoudiniCookable = InCookable;
	
	// Set the the Cookable as the HAC's outer
	HoudiniAssetComponent = NewObject<UHoudiniAssetComponent>(InCookable);

	// Set the HAC as the Cookable's component
	InCookable->SetComponentSupported(true);
	InCookable->SetComponent(HoudiniAssetComponent);

	TypedViewportClient->SetHoudiniAssetComponent(HoudiniAssetComponent);
	*/
}

FText 
SHoudiniAssetEditorViewport::GetTitleText() const
{	
	return FText::FromString("Houdini Asset Editor");
}
