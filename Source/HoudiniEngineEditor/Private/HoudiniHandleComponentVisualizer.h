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


/** Base class for clickable spline editing proxies. **/
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


/** Our spline visualizer. **/
class FHoudiniHandleComponentVisualizer : public FComponentVisualizer
{
public:

	FHoudiniHandleComponentVisualizer();
	virtual ~FHoudiniHandleComponentVisualizer();

/** FComponentVisualizer methods. **/
public:

	/** Registration of this component visualizer. **/
	virtual void OnRegister() override;

	/** Draw visualization for the given component. **/
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;

	/** Handle a click on a registered hit box. **/
	virtual bool VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy,
		const FViewportClick& Click) override;

	/** Called when editing is no longer being performed. **/
	virtual void EndEditing() override;

	/** Returns location of a gizmo widget. **/
	//virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;

	/** Handle input change. **/
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate,
		FRotator& DeltaRotate, FVector& DeltaScale) override;

	/** Create context menu for this visualizer. **/
	//virtual TSharedPtr<SWidget> GenerateContextMenu() const override;

protected:

	/** Update owner spline component and Houdini component it is attached to. **/
	void UpdateHoudiniComponents();

	/** Perform internal component update. **/
	void NotifyComponentModified(int32 PointIndex, const FVector& Point);

protected:

	/** Visualizer actions. **/
	TSharedPtr<FUICommandList> VisualizerActions;

	/** Houdini component which is being edited. **/
	UHoudiniHandleComponent* EditedComponent;

	/** Is set to true if we are editing corresponding curve. **/
	bool bEditing;
};
