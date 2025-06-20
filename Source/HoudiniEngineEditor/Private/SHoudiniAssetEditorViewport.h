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

#include "HoudiniAssetComponent.h"
#include "HoudiniCookable.h"

#include "CoreMinimal.h"
#include "Components/Viewport.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewScene.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewSceneModule.h"
#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"
#include "SAssetEditorViewport.h"
#include "SEditorViewport.h"
#include "SlateFwd.h"
#include "UObject/GCObject.h"
#include "Widgets/SViewport.h"
#include "LevelEditorViewport.h"

class FHoudiniAssetEditor;

class SHoudiniAssetEditorViewport
	: public SAssetEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:

	SLATE_BEGIN_ARGS(SHoudiniAssetEditorViewport) {}
	SLATE_END_ARGS()

	SHoudiniAssetEditorViewport();
	~SHoudiniAssetEditorViewport();

	// Construct this widget
	void Construct(const FArguments& InArgs);
	void Construct(const FArguments& InArgs, TSharedPtr<FHoudiniAssetEditor> InHoudiniAssetEditor);

	// Viewport widget accessor
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;

	// Creates the ViewPortClient for this viewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

	// Get the viewport client
	TSharedPtr<class FHoudiniAssetEditorViewportClient> GetViewportClient() { return TypedViewportClient; };

	// FGCObject 
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	// Toolbar interface	
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	void BindCommands() override;
	EVisibility GetTransformToolbarVisibility() const;
	void OnFocusViewportToSelection() override;
	
	virtual FString GetReferencerName() const override
	{
		return TEXT("SHoudiniAssetEditorViewport");
	}

	// Houdini Asset setter
	void SetHoudiniAsset(UHoudiniAsset* InAsset);

	// Returns the preview scene being rendered in the viewport
	TSharedRef<FAdvancedPreviewScene> GetPreviewScene() { return PreviewScene.ToSharedRef(); }

protected:

	FText GetTitleText() const;
		
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	virtual TSharedPtr<IPreviewProfileController> CreatePreviewProfileController() override;
#endif


private:
	
	// Pointer to our Houdini Asset Editor owner
	TWeakPtr<class FHoudiniAssetEditor> HoudiniAssetEditorPtr;
	// The Preview Scene for this viewport
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	//Shared ptr to the client
	TSharedPtr<class FHoudiniAssetEditorViewportClient> TypedViewportClient;
	//All components to use in the client
	TObjectPtr<UHoudiniCookable> HoudiniCookable;
};
