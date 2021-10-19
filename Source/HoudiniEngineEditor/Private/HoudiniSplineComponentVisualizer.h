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

#include "HoudiniSplineComponent.h"

#include "HitProxies.h"
#include "ComponentVisualizer.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Commands/Commands.h"

class FEditorViewportClient;

/** Base class for clickable spline editing proxies. **/
struct HHoudiniSplineVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniSplineVisProxy(const UActorComponent * InComponent) 
		: HComponentVisProxy(InComponent, HPP_Wireframe)
	{}
};

/** Proxy for a spline control point. **/
struct HHoudiniSplineControlPointVisProxy : public HHoudiniSplineVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniSplineControlPointVisProxy(const UActorComponent * InComponent, int32 InControlPointIndex)
		: HHoudiniSplineVisProxy(InComponent)
		, ControlPointIndex(InControlPointIndex)
	{}

	int32 ControlPointIndex;
};

/** Proxy for a spline display point. **/
struct HHoudiniSplineCurveSegmentVisProxy : public HHoudiniSplineVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniSplineCurveSegmentVisProxy(const UActorComponent * InComponent, int32 InDisplayPointIndex)
		: HHoudiniSplineVisProxy(InComponent)
		, DisplayPointIndex(InDisplayPointIndex)
	{}

	int32 DisplayPointIndex;
};

class FHoudiniSplineComponentVisualizerCommands : public TCommands< FHoudiniSplineComponentVisualizerCommands > 
{
	public:
		FHoudiniSplineComponentVisualizerCommands();

		/** Register commands. **/
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> CommandAddControlPoint;

		TSharedPtr<FUICommandInfo> CommandDuplicateControlPoint;

		TSharedPtr<FUICommandInfo> CommandDeleteControlPoint;

		TSharedPtr<FUICommandInfo> CommandDeselectAllControlPoints;

		TSharedPtr<FUICommandInfo> CommandInsertControlPoint;
};


/** **/
class FHoudiniSplineComponentVisualizer : public FComponentVisualizer 
{
	public:
		FHoudiniSplineComponentVisualizer();

	private:
		void RefreshViewport(bool bInvalidateHitProxies=true);

	public:
		virtual void OnRegister() override;

		virtual void DrawVisualization(
			const UActorComponent * Component, const FSceneView * View,
			FPrimitiveDrawInterface * PDI) override;

		virtual bool VisProxyHandleClick(
			FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy,
			const FViewportClick& Click) override;

		virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
		virtual bool IsVisualizingArchetype() const override;

		virtual void EndEditing() override;

		virtual bool HandleInputDelta(
			FEditorViewportClient* ViewportClient, FViewport* Viewport,
			FVector& DeltaTranslate, FRotator& DeltaRotate,
			FVector& DeltaScale) override;

		virtual bool HandleInputKey(FEditorViewportClient * ViewportClient, FViewport * Viewport, FKey Key, EInputEvent Event) override;

		virtual TSharedPtr<SWidget> GenerateContextMenu() const override;

	protected:

		/** Callbacks for add control point action**/
		void OnAddControlPoint();
		bool IsAddControlPointValid() const;

		/** Callbacks for delete control point action. **/
		void OnDeleteControlPoint();
		bool IsDeleteControlPointValid() const;

		/** Callbacks for duplicate control point action. **/
		void OnDuplicateControlPoint();
		bool IsDuplicateControlPointValid() const;

		/** Callbacks for deselect all control points action. **/
		void OnDeselectAllControlPoints();
		bool IsDeselectAllControlPointsValid() const;

		/** Callbacks for inserting a control point action.**/
		void OnInsertControlPoint();
		bool IsInsertControlPointValid() const;
		// For alt-pressed inserting control point on curve.
		int32 OnInsertControlPointWithoutUpdate();

		int32 AddControlPointAfter(const FTransform & NewPoint, const int32 & nIndex);

	public:
		/** Property path from the parent actor to the component */
		// NOTE: We need to use SplinePropertyPath on the visualizer as opposed to a direct pointer since the
		// direct pointer breaks during Blueprint reconstructions properly
		// (see SplineComponent / SplineMeshComponent visualizers).
		FComponentPropertyPath SplinePropertyPath;
		UHoudiniSplineComponent* GetEditedHoudiniSplineComponent() const { return Cast<UHoudiniSplineComponent>(SplinePropertyPath.GetComponent()); }

	protected:

		bool bAllowDuplication;

		int32 EditedCurveSegmentIndex;

		TSharedPtr<FUICommandList> VisualizerActions;

		/** Rotation used for the gizmo widgets **/
		FQuat CachedRotation;

		FVector CachedScale3D;

		/** Indicates wether or not a transaction should be recorded when moving a point **/
		bool bMovingPoints;

		bool bInsertingOnCurveControlPoints;

		bool bRecordingMovingPoints;

	private:
		FEditorViewportClient * FindViewportClient(const UHoudiniSplineComponent * InHoudiniSplineComponent, const FSceneView * View);

		bool IsCookOnCurveChanged(UHoudiniSplineComponent* InHoudiniSplineComponent);

};