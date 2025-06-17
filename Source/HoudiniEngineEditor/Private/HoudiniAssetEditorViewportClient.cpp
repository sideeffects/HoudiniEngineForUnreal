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

#include "HoudiniAssetEditorViewportClient.h"

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"
#include "SHoudiniAssetEditorViewport.h"

#include "ActorFactories/ActorFactory.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewSceneModule.h"
#include "Editor/UnrealEd/Public/UnrealWidget.h"
#include "Runtime/Engine/Classes/Components/PostProcessComponent.h"
#include "Runtime/Engine/Classes/Engine/PostProcessVolume.h"
#include "Runtime/Engine/Public/SceneView.h"

FHoudiniAssetEditorViewportClient::FHoudiniAssetEditorViewportClient(
	const TSharedRef<SHoudiniAssetEditorViewport>& InHoudiniAssetEditorViewport,
	const TSharedRef<FAdvancedPreviewScene>& InPreviewScene) 
	: FEditorViewportClient(
		nullptr,
		&InPreviewScene.Get(),
		StaticCastSharedRef<SEditorViewport>(InHoudiniAssetEditorViewport))
	, ViewportPtr(InHoudiniAssetEditorViewport)
{
	AdvancedPreviewScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);

	// Enable RealTime
	SetRealtime(true);

	// Hide grid, we don't need this.
	DrawHelper.bDrawGrid = false;
	DrawHelper.bDrawPivot = false;
	DrawHelper.AxesLineThickness = 5;
	DrawHelper.PivotSize = 5;

	//Initiate view
	SetViewLocation(FVector(75, 75, 75));
	SetViewRotation(FVector(-75, -75, -75).Rotation());

	EngineShowFlags.SetScreenPercentage(true);

	// Set the Default type to Ortho and the XZ Plane
	ELevelViewportType NewViewportType = LVT_Perspective;
	SetViewportType(NewViewportType);
	
	// View Modes in Persp and Ortho
	SetViewModes(VMI_Lit, VMI_Lit);
	
	// Add a PostProcess Component to the scene that will be controlled
	// by the scene settings
	PostProcessComponent = NewObject<UPostProcessComponent>();
	//PostProcessComponent->Settings = Profile.PostProcessingSettings;
	PostProcessComponent->bUnbound = true;
	PreviewScene->AddComponent(PostProcessComponent, FTransform(), false);

	//Allow post process materials...
	EngineShowFlags.SetPostProcessMaterial(true);
	EngineShowFlags.SetPostProcessing(true);
}

void 
FHoudiniAssetEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		if(AdvancedPreviewScene)
		AdvancedPreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void
FHoudiniAssetEditorViewportClient::ResetScene()
{
	// TODO ?
	// Reset scene settings, view transform etc... ?
}

void
FHoudiniAssetEditorViewportClient::SetHoudiniAsset(UHoudiniAsset* InAsset)
{
	// Get the HAA asset Factory
	UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
	if (!Factory)
		return;

	// Load the asset
	UObject* AssetObj = Cast<UObject>(InAsset);
	if (!AssetObj)
		return;
	

	// Create the actor for the HDA
	AActor* CreatedActor = Factory->CreateActor(AssetObj, GetWorld()->GetCurrentLevel(), FTransform::Identity);
	if (!CreatedActor)
		return;

	HoudiniAssetActor = Cast<AHoudiniAssetActor>(CreatedActor);
	if (!HoudiniAssetActor)
		return;

}
