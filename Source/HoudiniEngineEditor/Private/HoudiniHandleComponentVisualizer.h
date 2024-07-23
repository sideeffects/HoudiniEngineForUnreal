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

#pragma once

#include "ComponentVisualizer.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"

#include "Components/ActorComponent.h"
#include "HoudiniHandleComponent.h"

// Base class for clickable editing proxies.
struct HHoudiniHandleVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniHandleVisProxy(const UActorComponent * InComponent);

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

// Define commands for our component visualizer
class FHoudiniHandleComponentVisualizerCommands : public TCommands< FHoudiniHandleComponentVisualizerCommands >
{
public:

	// Constructor.
	FHoudiniHandleComponentVisualizerCommands();

	// Register commands.
	virtual void RegisterCommands() override;

public:

	// Command for adding a control point.
	TSharedPtr< FUICommandInfo > CommandAddControlPoint;

	// Command for deleting a control point.
	TSharedPtr< FUICommandInfo > CommandDeleteControlPoint;
};


// Our handle visualizer.
class FHoudiniHandleComponentVisualizer : public FComponentVisualizer
{
public:
	FHoudiniHandleComponentVisualizer();

	virtual ~FHoudiniHandleComponentVisualizer();

	// FComponentVisualizer methods.

	// Draw visualization for the given component.
	virtual void DrawVisualization(
		const UActorComponent * Component,
		const FSceneView * View,
		FPrimitiveDrawInterface * PDI) override;

	// Draw visualization HUD for the given component (on screen text).
	virtual void DrawVisualizationHUD(
		const UActorComponent* Component,
		const FViewport* Viewport, 
		const FSceneView* View,
		FCanvas* Canvas) override;

	// Handle a click on a registered hit box.
	virtual bool VisProxyHandleClick(
		FEditorViewportClient* InViewportClient,
		HComponentVisProxy* VisProxy,
		const FViewportClick& Click) override;

	virtual void EndEditing() override;

	// Returns location of a gizmo widget.
	virtual bool GetWidgetLocation(
		const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;

	//
	virtual bool GetCustomInputCoordinateSystem(
		const FEditorViewportClient * ViewportClient, FMatrix & OutMatrix) const override;

	// Handle input change.
	virtual bool HandleInputDelta(
		FEditorViewportClient* ViewportClient,
		FViewport* Viewport,
		FVector& DeltaTranslate,
		FRotator& DeltaRotate,
		FVector& DeltaScale) override;

	//
	void SetEditedComponent(UHoudiniHandleComponent* InComponent) { EditedComponent = InComponent; };

	//
	void ClearEditedComponent() { EditedComponent = nullptr; };


protected:

	// Visualizer actions.
	TSharedPtr<FUICommandList> VisualizerActions;

	// Houdini component which is being edited.
	UHoudiniHandleComponent* EditedComponent;

	// Is set to true if we are editing.
	uint32 bEditing : 1;
	uint32 bAllowTranslate : 1;
	uint32 bAllowRotation : 1;
	uint32 bAllowScale : 1;
};