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
