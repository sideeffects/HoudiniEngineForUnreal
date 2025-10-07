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
#include "HoudiniHandleComponent.h"
#include "HoudiniSplineComponent.h"

#include "ActorFactories/ActorFactory.h"
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 3)
	#include "Editor/ActorPositioning.h"
#endif
#include "ComponentVisualizer.h"
#include "Editor/AdvancedPreviewScene/Public/AdvancedPreviewSceneModule.h"
#include "Editor/UnrealEd/Public/UnrealWidget.h"
#include "Editor/UnrealEdEngine.h"
#include "Runtime/Engine/Classes/Components/PostProcessComponent.h"
#include "Runtime/Engine/Classes/Engine/PostProcessVolume.h"
#include "Runtime/Engine/Public/SceneView.h"
#include "SnappingUtils.h"
#include "UnrealEdGlobals.h"
#include "UObject/UObjectIterator.h"



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
	
	// Set the Actor transform so assets properly face forward in UE
	FTransform FaceForward = FTransform::Identity;
	FaceForward.SetRotation(FQuat(FVector(0.0f, 0.0f, 1.0f), -UE_HALF_PI));

	// Create the actor for the HDA
	AActor* CreatedActor = Factory->CreateActor(AssetObj, GetWorld()->GetCurrentLevel(), FaceForward);
	if (!CreatedActor)
		return;

	HoudiniAssetActor = Cast<AHoudiniAssetActor>(CreatedActor);
	if (!HoudiniAssetActor)
		return;

}


void 
FHoudiniAssetEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
	if (GUnrealEd == nullptr)
		return;
	/*
	if (GUnrealEd != NULL && !IsInGameView())
	{
		GUnrealEd->DrawComponentVisualizers(View, PDI);
	}
	*/
	
	// Visualize Houdini splines
	TSharedPtr<FComponentVisualizer> SplineVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniSplineComponent::StaticClass());
	if (SplineVisualizer.IsValid())
	{
		for (TObjectIterator<UHoudiniSplineComponent> Itr; Itr; ++Itr)
		{
			if (Itr->GetOwner() && Itr->GetOwner() == HoudiniAssetActor)
			{
				const UActorComponent* Comp = Cast<UActorComponent>(*Itr);
				if (Comp != nullptr && Comp->IsRegistered())
				{
					SplineVisualizer->DrawVisualization(Comp, View, PDI);
				}
			}
		}
	}

	// Visualize Houdini Handles
	TSharedPtr<FComponentVisualizer> HandleVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniHandleComponent::StaticClass());
	if (HandleVisualizer.IsValid())
	{
		for (TObjectIterator<UHoudiniHandleComponent> Itr; Itr; ++Itr)
		{
			if (Itr->GetOwner() && Itr->GetOwner() == HoudiniAssetActor)
			{
				const UActorComponent* Comp = Cast<UActorComponent>(*Itr);
				if (Comp != nullptr && Comp->IsRegistered())
				{
					HandleVisualizer->DrawVisualization(Comp, View, PDI);
				}
			}
		}
	}
}


void 
FHoudiniAssetEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	bool bHandled = false;

	const FViewportClick Click(&View, this, Key, Event, HitX, HitY);
	bHandled = GUnrealEd->ComponentVisManager.HandleClick(this, HitProxy, Click);

	/*
	HComponentVisProxy* ComponentVisProxy = HitProxyCast<HComponentVisProxy>(HitProxy);

	TSharedPtr<FComponentVisualizer> SplineVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniSplineComponent::StaticClass());
	TSharedPtr<FComponentVisualizer> HandleVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniHandleComponent::StaticClass());	
	if (HitProxy == nullptr)
	{
		// If our HitProxy is null - make sure we end editing of current splines/handles
		SplineVisualizer->EndEditing();
		HandleVisualizer->EndEditing();
	}
	else
	{
		
		
		// Forward to our visualizers
		bHandled = SplineVisualizer->VisProxyHandleClick(this, ComponentVisProxy, Click);

		if(!bHandled)
			bHandled = SplineVisualizer->VisProxyHandleClick(this, ComponentVisProxy, Click);
	}
	*/

	if (!bHandled)
	{
		FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
	}
}

bool 
FHoudiniAssetEditorViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	bool bHandled = false;
	if (GUnrealEd->ComponentVisManager.HandleInputDelta(this, InViewport, Drag, Rot, Scale))
	{
		return true;
	}

	// Give the current editor mode a chance to use the input first.  If it does, don't apply it to anything else.
	if (FEditorViewportClient::InputWidgetDelta(InViewport, CurrentAxis, Drag, Rot, Scale))
	{
		bHandled = true;
	}

	/*
	// Forward to our visualizers
	TSharedPtr<FComponentVisualizer> SplineVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniSplineComponent::StaticClass());
	bHandled = SplineVisualizer->HandleInputDelta(this, Viewport, Drag, Rot, Scale);

	TSharedPtr<FComponentVisualizer> HandleVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniHandleComponent::StaticClass());
	if (!bHandled)
		bHandled = HandleVisualizer->HandleInputDelta(this, Viewport, Drag, Rot, Scale);

	if (!bHandled)
	{
		return FEditorViewportClient::InputWidgetDelta(InViewport, CurrentAxis, Drag, Rot, Scale);
	}
	*/
	return bHandled;
}


bool
FHoudiniAssetEditorViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (bDisableInput)
		return true;

	const int32	HitX = InEventArgs.Viewport->GetMouseX();
	const int32	HitY = InEventArgs.Viewport->GetMouseY();

	FInputEventState InputState(InEventArgs.Viewport, InEventArgs.Key, InEventArgs.Event);

	// Compute a view.
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InEventArgs.Viewport,
		GetScene(),
		EngineShowFlags)
		.SetRealtimeUpdate(IsRealtime()));

	FSceneView* View = CalcSceneView(&ViewFamily);

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 3)
	// Compute the click location.
	if (InputState.IsMouseButtonEvent() && InputState.IsAnyMouseButtonDown())
	{
		const FViewportCursorLocation Cursor(View, this, HitX, HitY);
		const FActorPositionTraceResult TraceResult = FActorPositioning::TraceWorldForPositionWithDefault(Cursor, *View);
		GEditor->UnsnappedClickLocation = TraceResult.Location;
		GEditor->ClickLocation = TraceResult.Location;
		GEditor->ClickPlane = FPlane(TraceResult.Location, TraceResult.SurfaceNormal);

		// Snap the new location if snapping is enabled
		FSnappingUtils::SnapPointToGrid(GEditor->ClickLocation, FVector::ZeroVector);
	}
#endif

	if (GUnrealEd->ComponentVisManager.HandleInputKey(this, InEventArgs.Viewport, InEventArgs.Key, InEventArgs.Event))
	{
		return true;
	}
	/*
	bool bHandled = false;
	const FKey& Key = InEventArgs.Key;
	const EInputEvent& Event = InEventArgs.Event;
	const FViewport* InViewport = InEventArgs.Viewport;

	// Forward to our visualizers
	TSharedPtr<FComponentVisualizer> SplineVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniSplineComponent::StaticClass());
	bHandled = SplineVisualizer->HandleInputKey(this, Viewport, Key, Event);

	TSharedPtr<FComponentVisualizer> HandleVisualizer = GUnrealEd->FindComponentVisualizer(UHoudiniHandleComponent::StaticClass());
	if (!bHandled)
		bHandled = HandleVisualizer->HandleInputKey(this, Viewport, Key, Event);
	*/
	//if (!bHandled)
		return FEditorViewportClient::InputKey(InEventArgs);

	//return bHandled;
}
