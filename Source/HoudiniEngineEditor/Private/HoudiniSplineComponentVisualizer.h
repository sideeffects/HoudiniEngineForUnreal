/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once


#include "HoudiniSplineComponent.h"


/** Base class for clickable spline editing proxies. **/
struct HHoudiniSplineVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniSplineVisProxy(const UActorComponent* InComponent);
};


/** Proxy for a spline control point. **/
struct HHoudiniSplineControlPointVisProxy : public HHoudiniSplineVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniSplineControlPointVisProxy(const UActorComponent* InComponent, int32 InControlPointIndex);

	int32 ControlPointIndex;
};


/** Define commands for our component visualizer */
class FHoudiniSplineComponentVisualizerCommands : public TCommands<FHoudiniSplineComponentVisualizerCommands>
{
public:

	/** Constructor. **/
	FHoudiniSplineComponentVisualizerCommands();

	/** Register commands. **/
	virtual void RegisterCommands() override;

public:

	/** Command for adding a control point. **/
	TSharedPtr<FUICommandInfo> CommandAddControlPoint;

	/** Command for deleting a control point. **/
	TSharedPtr<FUICommandInfo> CommandDeleteControlPoint;
};


/** Our spline visualizer. **/
class FHoudiniSplineComponentVisualizer : public FComponentVisualizer
{
public:

	FHoudiniSplineComponentVisualizer();
	virtual ~FHoudiniSplineComponentVisualizer();

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
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;

	/** Handle input change. **/
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate,
		FRotator& DeltaRotate, FVector& DeltaScale) override;

	/** Create context menu for this visualizer. **/
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;

protected:

	/** Update owner spline component and Houdini component it is attached to. **/
	void UpdateHoudiniComponents();

	/** Perform internal component update. **/
	void NotifyComponentModified(int32 PointIndex, const FVector& Point);

	/** Callbacks for Add control point action. **/
	void OnAddControlPoint();
	bool IsAddControlPointValid() const;

	/** Callbacks for Delete control point action. **/
	void OnDeleteControlPoint();
	bool IsDeleteControlPointValid() const;

protected:

	/** Visualizer actions. **/
	TSharedPtr<FUICommandList> VisualizerActions;

	/** Houdini component which is being edited. **/
	UHoudiniSplineComponent* EditedHoudiniSplineComponent;

	/** Is set to true if we are editing corresponding curve. **/
	bool bCurveEditing;

	/** Keeps index of currently selected control point, if editing is being performed. **/
	int32 EditedControlPointIndex;
};
