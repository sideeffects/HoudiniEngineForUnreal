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

#pragma once

#include "IDetailsView.h"
#include "SEditorViewport.h"
#include "SSingleObjectDetailsPanel.h"
#include "Toolkits/AssetEditorToolkit.h"

class FSpawnTabArgs;
class FToolBarBuilder;

class FHoudiniAssetEditor;
class SHoudiniAssetEditorViewport;
class SHoudiniNodeSyncPanel;
class UHoudiniAsset;
class UHoudiniCookable;

namespace EHoudiniAssetEditorMode
{
	enum Type
	{
		ViewMode,
		HDAMode,
		SessionSyncMode
	};
}

//-----------------------------------------------------------------------------
// SHoudiniAssetEditorDetailsPanel
//-----------------------------------------------------------------------------
class SHoudiniAssetEditorDetailsPanel : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SHoudiniAssetEditorDetailsPanel) {}
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, TSharedPtr<FHoudiniAssetEditor> InHoudiniAssetEditor);

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override;
	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override;
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	// Pointer back to owning Houdini Asset editor instance (the keeper of state)
	TWeakPtr<class FHoudiniAssetEditor> HoudiniAssetEditorPtr;

	// Cached object view
	TWeakObjectPtr<UObject> MyLastObservedObject;

	//double LastUpdateTime = 0.0;
};


//-----------------------------------------------------------------------------
// FHoudiniAssetEditor
//-----------------------------------------------------------------------------
class FHoudiniAssetEditor : public FAssetEditorToolkit, public FGCObject
{
public:
	FHoudiniAssetEditor();

	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;


	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FString GetDocumentationLink() const override;
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override;

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FHoudiniAssetEditor"); }

	virtual void OnClose() override;

	void InitHoudiniAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, class UHoudiniAsset* InitHDA);
	
	UHoudiniAsset* GetHoudiniAssetBeingEdited() const { return HoudiniAssetBeingEdited; }
	UHoudiniCookable* GetHoudiniCookableBeingEdited() { return HoudiniCookableBeingEdited; }

	EHoudiniAssetEditorMode::Type GetCurrentMode() const;

	FString GetHoudiniAssetEditorIdentifier() const { return HoudiniAssetEditorIdentifier; };

protected:

	void BindCommands();
	void ExtendMenu();
	void ExtendToolbar();

	virtual void CreateEditorModeManager() override;

	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnNodeSyncTab(const FSpawnTabArgs& Args);

	void CreateModeToolbarWidgets(FToolBarBuilder& ToolbarBuilder);

	FText GetViewportCornerText() const;

protected:

	TObjectPtr<UHoudiniAsset> HoudiniAssetBeingEdited;
	TObjectPtr<UHoudiniCookable> HoudiniCookableBeingEdited;

	TSharedPtr<SHoudiniAssetEditorViewport> ViewportPtr;
	TSharedPtr<SHoudiniAssetEditorDetailsPanel> DetailsTabPtr;
	TSharedPtr<SHoudiniNodeSyncPanel> NodeSyncPanel;

	FString HoudiniAssetEditorIdentifier;
};
