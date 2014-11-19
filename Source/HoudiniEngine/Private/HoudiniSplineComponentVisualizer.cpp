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

#include "HoudiniEnginePrivatePCH.h"


IMPLEMENT_HIT_PROXY(HHoudiniSplineVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HHoudiniSplineControlPointVisProxy, HHoudiniSplineVisProxy);


HHoudiniSplineVisProxy::HHoudiniSplineVisProxy(const UActorComponent* InComponent) :
	HComponentVisProxy(InComponent, HPP_Wireframe)
{

}


HHoudiniSplineControlPointVisProxy::HHoudiniSplineControlPointVisProxy(const UActorComponent* InComponent, int32 InControlPointIndex) :
	HHoudiniSplineVisProxy(InComponent),
	ControlPointIndex(InControlPointIndex)
{

}


FHoudiniSplineComponentVisualizer::FHoudiniSplineComponentVisualizer() :
	FComponentVisualizer(),
	EditedHoudiniSplineComponent(nullptr),
	bCurveEditing(false),
	EditedControlPointIndex(INDEX_NONE)
{

}


FHoudiniSplineComponentVisualizer::~FHoudiniSplineComponentVisualizer()
{

}


void
FHoudiniSplineComponentVisualizer::OnRegister()
{

}


void
FHoudiniSplineComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UHoudiniSplineComponent* HoudiniSplineComponent = Cast<const UHoudiniSplineComponent>(Component);
	if(HoudiniSplineComponent && HoudiniSplineComponent->IsValidCurve())
	{
		static const FColor ColorNormal(255, 255, 255);
		static const FColor ColorSelected(255, 0, 0);

		static const float GrabHandleSize = 12.0f;

		// Get component transformation.
		const FTransform& HoudiniSplineComponentTransform = HoudiniSplineComponent->ComponentToWorld;

		// Get curve points.
		const TArray<FVector>& CurvePoints = HoudiniSplineComponent->CurvePoints;
		const TArray<FVector>& CurveDisplayPoints = HoudiniSplineComponent->CurveDisplayPoints;

		// Draw the curve.
		FVector DisplayPointFirst;
		FVector DisplayPointPrevious;

		int32 NumDisplayPoints = CurveDisplayPoints.Num();
		for(int32 DisplayPointIdx = 0; DisplayPointIdx < NumDisplayPoints; ++DisplayPointIdx)
		{
			// Get point for this index.
			const FVector& DisplayPoint = HoudiniSplineComponentTransform.TransformPosition(CurveDisplayPoints[DisplayPointIdx]);

			if(DisplayPointIdx > 0)
			{
				// Draw line from previous point to current one.
				PDI->DrawLine(DisplayPointPrevious, DisplayPoint, ColorNormal, SDPG_Foreground);
			}
			else
			{
				DisplayPointFirst = DisplayPoint;
			}

			// If this is last point, draw link from last to first.
			if(NumDisplayPoints > 1 && DisplayPointIdx + 1 == NumDisplayPoints)
			{
				PDI->DrawLine(DisplayPointFirst, DisplayPoint, ColorNormal, SDPG_Foreground);
			}

			DisplayPointPrevious = DisplayPoint;
		}

		// Draw control points.
		for(int32 PointIdx = 0; PointIdx < CurvePoints.Num(); ++PointIdx)
		{
			// Get point at this index.
			const FVector& DisplayPoint = HoudiniSplineComponentTransform.TransformPosition(CurvePoints[PointIdx]);

			// If we are editing this control point, change color.
			FColor ColorCurrent = ColorNormal;
			if(bCurveEditing && PointIdx == EditedControlPointIndex)
			{
				ColorCurrent = ColorSelected;
			}

			// Draw point and set hit box for it.
			PDI->SetHitProxy(new HHoudiniSplineControlPointVisProxy(HoudiniSplineComponent, PointIdx));
			PDI->DrawPoint(DisplayPoint, ColorCurrent, GrabHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}
	}
}


bool
FHoudiniSplineComponentVisualizer::VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bCurveEditing = false;

	if(VisProxy && VisProxy->Component.IsValid())
	{
		const UHoudiniSplineComponent* HoudiniSplineComponent = CastChecked<const UHoudiniSplineComponent>(VisProxy->Component.Get());
		EditedHoudiniSplineComponent = const_cast<UHoudiniSplineComponent*>(HoudiniSplineComponent);

		if(HoudiniSplineComponent)
		{
			if(VisProxy->IsA(HHoudiniSplineControlPointVisProxy::StaticGetType()))
			{
				HHoudiniSplineControlPointVisProxy* ControlPointProxy = (HHoudiniSplineControlPointVisProxy*) VisProxy;
				bCurveEditing = true;
				EditedControlPointIndex = ControlPointProxy->ControlPointIndex;
			}
		}
	}

	return bCurveEditing;
}


void
FHoudiniSplineComponentVisualizer::EndEditing()
{
	EditedHoudiniSplineComponent = nullptr;
	EditedControlPointIndex = INDEX_NONE;
}


bool
FHoudiniSplineComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		// Get curve points.
		const TArray<FVector>& CurvePoints = EditedHoudiniSplineComponent->CurvePoints;
		OutLocation = EditedHoudiniSplineComponent->ComponentToWorld.TransformPosition(CurvePoints[EditedControlPointIndex]);
		
		return true;
	}

	return false;
}


bool
FHoudiniSplineComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		// Get curve points.
		const TArray<FVector>& CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

		// Get component transformation.
		const FTransform& HoudiniSplineComponentTransform = EditedHoudiniSplineComponent->ComponentToWorld;

		FVector Point = CurvePoints[EditedControlPointIndex];

		// Handle change in translation.
		if(!DeltaTranslate.IsZero())
		{
			FVector PointTransformed = HoudiniSplineComponentTransform.TransformPosition(Point);
			FVector PointTransformedDelta = PointTransformed + DeltaTranslate;

			Point = HoudiniSplineComponentTransform.InverseTransformPosition(PointTransformedDelta);
		}

		// Handle change in rotation.
		if(!DeltaRotate.IsZero())
		{
			Point = DeltaRotate.RotateVector(Point);
		}

		if(!DeltaScale.IsZero())
		{
			// For now in case of rescaling, do nothing.
			return true;
		}

		// Handle change in scale.
		/*
		if(!DeltaScale.IsZero())
		{
			if(DeltaScale.X != 0.0f)
			{
				Point.X *= DeltaScale.X;
			}

			if(DeltaScale.Y != 0.0f)
			{
				Point.Y *= DeltaScale.Y;
			}

			if(DeltaScale.Z != 0.0f)
			{
				Point.Z *= DeltaScale.Z;
			}
		}
		*/

		NotifyComponentModified(EditedControlPointIndex, Point);
		return true;
	}

	return false;
}


void
FHoudiniSplineComponentVisualizer::NotifyComponentModified(int32 PointIndex, const FVector& Point)
{
	if(EditedHoudiniSplineComponent)
	{
		// Update given control point.
		EditedHoudiniSplineComponent->UpdatePoint(PointIndex, Point);
		EditedHoudiniSplineComponent->UploadControlPoints();

		// Retrieve Houdini asset component we are attached to and notify it about control point change. This will trigger recook.
		UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(EditedHoudiniSplineComponent->AttachParent);
		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->NotifyHoudiniSplineChanged(EditedHoudiniSplineComponent);
		}

		/*
		if (SplineOwningActor.IsValid())
		{
			SplineOwningActor.Get()->PostEditMove(true);
		}
		*/

		GEditor->RedrawLevelEditingViewports(true);
	}
}

