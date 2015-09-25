/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once


#include "HoudiniHandleComponent.h"


/** Base class for clickable editing proxies. **/
struct HHoudiniHandleVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniHandleVisProxy(const UActorComponent* InComponent);
};

/** Define commands for our component visualizer */
class FHoudiniHandleComponentVisualizerCommands : public TCommands<FHoudiniHandleComponentVisualizerCommands>
{
public:

	/** Constructor. **/
	FHoudiniHandleComponentVisualizerCommands();

	/** Register commands. **/
	virtual void RegisterCommands() override;

public:

	/** Command for adding a control point. **/
	TSharedPtr<FUICommandInfo> CommandAddControlPoint;

	/** Command for deleting a control point. **/
	TSharedPtr<FUICommandInfo> CommandDeleteControlPoint;
};


/** Our handle visualizer. **/
class FHoudiniHandleComponentVisualizer : public FComponentVisualizer
{
public:

	FHoudiniHandleComponentVisualizer();
	virtual ~FHoudiniHandleComponentVisualizer();

/** FComponentVisualizer methods. **/
public:

	/** Draw visualization for the given component. **/
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;

	/** Handle a click on a registered hit box. **/
	virtual bool VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy,
		const FViewportClick& Click) override;

	/** Called when editing is no longer being performed. **/
	virtual void EndEditing() override;

	/** Returns location of a gizmo widget. **/
	virtual bool GetWidgetLocation(const FEditorViewportClient*, FVector& OutLocation) const override;

	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;

	/** Handle input change. **/
	virtual bool HandleInputDelta(FEditorViewportClient*, FViewport*, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;

protected:
	/** Visualizer actions. **/
	TSharedPtr<FUICommandList> VisualizerActions;

	/** Houdini component which is being edited. **/
	UHoudiniHandleComponent* EditedComponent;

	/** Is set to true if we are editing. **/
	bool bEditing;
};
