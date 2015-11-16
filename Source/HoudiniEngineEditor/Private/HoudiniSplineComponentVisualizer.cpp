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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineEditor.h"


IMPLEMENT_HIT_PROXY(HHoudiniSplineVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HHoudiniSplineControlPointVisProxy, HHoudiniSplineVisProxy);


HHoudiniSplineVisProxy::HHoudiniSplineVisProxy(const UActorComponent* InComponent) :
	HComponentVisProxy(InComponent, HPP_Wireframe)
{

}


HHoudiniSplineControlPointVisProxy::HHoudiniSplineControlPointVisProxy(const UActorComponent* InComponent,
	int32 InControlPointIndex) :
	HHoudiniSplineVisProxy(InComponent),
	ControlPointIndex(InControlPointIndex)
{

}


FHoudiniSplineComponentVisualizerCommands::FHoudiniSplineComponentVisualizerCommands() :
	TCommands<FHoudiniSplineComponentVisualizerCommands>(
		"HoudiniSplineComponentVisualizer",
		LOCTEXT("HoudiniSplineComponentVisualizer", "Houdini Spline Component Visualizer"),
		NAME_None,
		FEditorStyle::GetStyleSetName()
		)
	{

	}


void
FHoudiniSplineComponentVisualizerCommands::RegisterCommands()
{
	UI_COMMAND(CommandAddControlPoint, "Add Control Point", "Add Control Point.",
		EUserInterfaceActionType::Button, FInputGesture());

	UI_COMMAND(CommandDeleteControlPoint, "Delete Control Point", "Delete Control Point.",
		EUserInterfaceActionType::Button, FInputGesture());
}


FHoudiniSplineComponentVisualizer::FHoudiniSplineComponentVisualizer() :
	FComponentVisualizer(),
	EditedHoudiniSplineComponent(nullptr),
	bCurveEditing(false),
	EditedControlPointIndex(INDEX_NONE)
{
	FHoudiniSplineComponentVisualizerCommands::Register();
	VisualizerActions = MakeShareable(new FUICommandList);
}


FHoudiniSplineComponentVisualizer::~FHoudiniSplineComponentVisualizer()
{
	FHoudiniSplineComponentVisualizerCommands::Unregister();
}


void
FHoudiniSplineComponentVisualizer::OnRegister()
{
	const auto& Commands = FHoudiniSplineComponentVisualizerCommands::Get();

	VisualizerActions->MapAction(
		Commands.CommandAddControlPoint,
		FExecuteAction::CreateSP(this, &FHoudiniSplineComponentVisualizer::OnAddControlPoint),
		FCanExecuteAction::CreateSP(this, &FHoudiniSplineComponentVisualizer::IsAddControlPointValid));

	VisualizerActions->MapAction(
		Commands.CommandDeleteControlPoint,
		FExecuteAction::CreateSP(this, &FHoudiniSplineComponentVisualizer::OnDeleteControlPoint),
		FCanExecuteAction::CreateSP(this, &FHoudiniSplineComponentVisualizer::IsDeleteControlPointValid));
}


void
FHoudiniSplineComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
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
			const FVector& DisplayPoint =
				HoudiniSplineComponentTransform.TransformPosition(CurveDisplayPoints[DisplayPointIdx]);

			if(DisplayPointIdx > 0)
			{
				// Draw line from previous point to current one.
				PDI->DrawLine(DisplayPointPrevious, DisplayPoint, ColorNormal, SDPG_Foreground);
			}
			else
			{
				DisplayPointFirst = DisplayPoint;
			}

			// If this is last point and curve is closed, draw link from last to first.
			if(HoudiniSplineComponent->IsClosedCurve() && (NumDisplayPoints > 1) &&
				(DisplayPointIdx + 1 == NumDisplayPoints))
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
FHoudiniSplineComponentVisualizer::VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bCurveEditing = false;

	if(VisProxy && VisProxy->Component.IsValid())
	{
		const UHoudiniSplineComponent* HoudiniSplineComponent =
			CastChecked<const UHoudiniSplineComponent>(VisProxy->Component.Get());

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
FHoudiniSplineComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient,
	FVector& OutLocation) const
{
	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		// Get curve points.
		const TArray<FVector>& CurvePoints = EditedHoudiniSplineComponent->CurvePoints;
		check(EditedControlPointIndex >= 0 && EditedControlPointIndex < CurvePoints.Num());
		OutLocation =
			EditedHoudiniSplineComponent->ComponentToWorld.TransformPosition(CurvePoints[EditedControlPointIndex]);

		return true;
	}

	return false;
}


bool
FHoudiniSplineComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		// Get curve points.
		const TArray<FVector>& CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

		// Get component transformation.
		const FTransform& HoudiniSplineComponentTransform = EditedHoudiniSplineComponent->ComponentToWorld;

		check(EditedControlPointIndex >= 0 && EditedControlPointIndex < CurvePoints.Num());
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

		NotifyComponentModified(EditedControlPointIndex, Point);
		return true;
	}

	return false;
}


TSharedPtr<SWidget>
FHoudiniSplineComponentVisualizer::GenerateContextMenu() const
{
	FHoudiniEngineEditor& HoudiniEngineEditor = FHoudiniEngineEditor::Get();
	TSharedPtr<ISlateStyle> StyleSet = HoudiniEngineEditor.GetSlateStyle();

	FMenuBuilder MenuBuilder(true, VisualizerActions);
	{
		MenuBuilder.BeginSection("CurveKeyEdit");

		{
			if(INDEX_NONE != EditedControlPointIndex)
			{
				MenuBuilder.AddMenuEntry(FHoudiniSplineComponentVisualizerCommands::Get().CommandAddControlPoint,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"));

				MenuBuilder.AddMenuEntry(FHoudiniSplineComponentVisualizerCommands::Get().CommandDeleteControlPoint,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"));
			}
		}

		MenuBuilder.EndSection();
	}

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


void
FHoudiniSplineComponentVisualizer::UpdateHoudiniComponents()
{
	if(EditedHoudiniSplineComponent)
	{
		if(EditedHoudiniSplineComponent->IsInputCurve())
		{
			EditedHoudiniSplineComponent->NotifyHoudiniInputCurveChanged();
		}
		else
		{
			UHoudiniAssetComponent* HoudiniAssetComponent =
				Cast<UHoudiniAssetComponent>(EditedHoudiniSplineComponent->AttachParent);

			if(HoudiniAssetComponent)
			{
				HoudiniAssetComponent->NotifyHoudiniSplineChanged(EditedHoudiniSplineComponent);
			}
		}

		if(GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
}


void
FHoudiniSplineComponentVisualizer::NotifyComponentModified(int32 PointIndex, const FVector& Point)
{
	if(EditedHoudiniSplineComponent)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent =
				Cast<UHoudiniAssetComponent>(EditedHoudiniSplineComponent->AttachParent);

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniSplineComponentChange", "Houdini Spline Component: Moving a point"),
			HoudiniAssetComponent);
		EditedHoudiniSplineComponent->Modify();

		// Update given control point.
		EditedHoudiniSplineComponent->UpdatePoint(PointIndex, Point);
		EditedHoudiniSplineComponent->UploadControlPoints();

		UpdateHoudiniComponents();
	}
}


void
FHoudiniSplineComponentVisualizer::OnAddControlPoint()
{
	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		const TArray<FVector>& CurvePoints = EditedHoudiniSplineComponent->CurvePoints;
		check(EditedControlPointIndex >= 0 && EditedControlPointIndex < CurvePoints.Num());

		FVector OtherPoint;
		FVector Point = CurvePoints[EditedControlPointIndex];

		if(EditedControlPointIndex + 1 != CurvePoints.Num())
		{
			OtherPoint = CurvePoints[EditedControlPointIndex + 1];
		}
		else
		{
			if(EditedHoudiniSplineComponent->bClosedCurve)
			{
				OtherPoint = CurvePoints[0];
			}
			else
			{
				OtherPoint = CurvePoints[EditedControlPointIndex - 1];
			}
		}

		FVector Dir = (OtherPoint - Point) / 2.0f;
		FVector NewPoint = Point + Dir;

		int32 ControlPointIndex = EditedControlPointIndex + 1;
		if(ControlPointIndex == CurvePoints.Num())
		{
			if(EditedHoudiniSplineComponent->bClosedCurve)
			{
				ControlPointIndex = 0;
			}
			else
			{
				ControlPointIndex = EditedControlPointIndex;
			}
		}

		UHoudiniAssetComponent* HoudiniAssetComponent =
				Cast<UHoudiniAssetComponent>(EditedHoudiniSplineComponent->AttachParent);

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniSplineComponentChange", "Houdini Spline Component: Adding a control point"),
			HoudiniAssetComponent);
		EditedHoudiniSplineComponent->Modify();

		EditedHoudiniSplineComponent->AddPoint(ControlPointIndex, NewPoint);
		EditedHoudiniSplineComponent->UploadControlPoints();

		// Select newly created point.
		EditedControlPointIndex = ControlPointIndex;

		UpdateHoudiniComponents();
	}
}


bool
FHoudiniSplineComponentVisualizer::IsAddControlPointValid() const
{
	// We can always add points.

	return true;
}


void
FHoudiniSplineComponentVisualizer::OnDeleteControlPoint()
{
	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent =
				Cast<UHoudiniAssetComponent>(EditedHoudiniSplineComponent->AttachParent);

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniSplineComponentChange", "Houdini Spline Component: Removing a control point"),
			HoudiniAssetComponent);
		EditedHoudiniSplineComponent->Modify();

		EditedHoudiniSplineComponent->RemovePoint(EditedControlPointIndex);
		EditedHoudiniSplineComponent->UploadControlPoints();

		UpdateHoudiniComponents();
		EditedControlPointIndex = INDEX_NONE;
	}
}


bool
FHoudiniSplineComponentVisualizer::IsDeleteControlPointValid() const
{
	// We can only delete points if we have more than two points.

	if(EditedHoudiniSplineComponent && EditedControlPointIndex != INDEX_NONE)
	{
		if(EditedHoudiniSplineComponent->GetCurvePointCount() > 2)
		{
			return true;
		}
	}

	return false;
}
