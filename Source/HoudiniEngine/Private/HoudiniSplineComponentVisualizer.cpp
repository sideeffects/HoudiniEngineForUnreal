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


FHoudiniSplineComponentVisualizer::FHoudiniSplineComponentVisualizer() :
	FComponentVisualizer()
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
		FVector DisplayPointPrevious;
		for(int32 DisplayPointIdx = 0; DisplayPointIdx < CurveDisplayPoints.Num(); ++DisplayPointIdx)
		{
			// Get point for this index.
			const FVector& DisplayPoint = HoudiniSplineComponentTransform.TransformPosition(CurveDisplayPoints[DisplayPointIdx]);

			if(DisplayPointIdx > 0)
			{
				// Draw line from previous point to current one.
				PDI->DrawLine(DisplayPointPrevious, DisplayPoint, ColorNormal, SDPG_Foreground);
			}

			DisplayPointPrevious = DisplayPoint;
		}

		// Draw control points.
		for(int32 PointIdx = 0; PointIdx < CurvePoints.Num(); ++PointIdx)
		{
			// Get point at this index.
			const FVector& DisplayPoint = HoudiniSplineComponentTransform.TransformPosition(CurvePoints[PointIdx]);

			// Draw point.
			PDI->DrawPoint(DisplayPoint, ColorNormal, GrabHandleSize, SDPG_Foreground);
		}
	}
}

