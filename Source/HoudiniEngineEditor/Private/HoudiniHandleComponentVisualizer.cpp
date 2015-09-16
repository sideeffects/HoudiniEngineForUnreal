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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniHandleComponentVisualizer.h"
#include "HoudiniEngineEditor.h"


IMPLEMENT_HIT_PROXY(HHoudiniHandleVisProxy, HComponentVisProxy);

HHoudiniHandleVisProxy::HHoudiniHandleVisProxy(const UActorComponent* InComponent) :
	HComponentVisProxy(InComponent, HPP_Wireframe)
{
}

FHoudiniHandleComponentVisualizerCommands::FHoudiniHandleComponentVisualizerCommands() :
	TCommands<FHoudiniHandleComponentVisualizerCommands>(
		"HoudiniHandleComponentVisualizer",
		LOCTEXT("HoudiniHandleComponentVisualizer", "Houdini Handle Component Visualizer"),
		NAME_None,
		FEditorStyle::GetStyleSetName()
	)
{
}

void
FHoudiniHandleComponentVisualizerCommands::RegisterCommands()
{	
}

FHoudiniHandleComponentVisualizer::FHoudiniHandleComponentVisualizer() :
	FComponentVisualizer(),
	EditedComponent(nullptr),
	bEditing(false)
{
	FHoudiniHandleComponentVisualizerCommands::Register();
	VisualizerActions = MakeShareable(new FUICommandList);
}

FHoudiniHandleComponentVisualizer::~FHoudiniHandleComponentVisualizer()
{
	FHoudiniHandleComponentVisualizerCommands::Unregister();
}

void
FHoudiniHandleComponentVisualizer::OnRegister()
{
	/*const auto& Commands = FHoudiniHandleComponentVisualizerCommands::Get();

	VisualizerActions->MapAction(
		Commands.CommandAddControlPoint,
		FExecuteAction::CreateSP(this, &FHoudiniHandleComponentVisualizer::OnAddControlPoint),
		FCanExecuteAction::CreateSP(this, &FHoudiniHandleComponentVisualizer::IsAddControlPointValid));

	VisualizerActions->MapAction(
		Commands.CommandDeleteControlPoint,
		FExecuteAction::CreateSP(this, &FHoudiniHandleComponentVisualizer::OnDeleteControlPoint),
		FCanExecuteAction::CreateSP(this, &FHoudiniHandleComponentVisualizer::IsDeleteControlPointValid));*/
}


void
FHoudiniHandleComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	const UHoudiniHandleComponent* HoudiniHandleComponent = Cast<const UHoudiniHandleComponent>(Component);
	if (!HoudiniHandleComponent)
	{
		return;
	}

	//static const FColor ColorNormal(255, 255, 255);
	//static const FColor ColorSelected(255, 0, 0);

	//static const float GrabHandleSize = 12.0f;

	//// Get component transformation.
	//const FTransform& HoudiniHandleComponentTransform = HoudiniHandleComponent->ComponentToWorld;

	//// Get curve points.
	//const TArray<FVector>& CurvePoints = HoudiniHandleComponent->CurvePoints;
	//const TArray<FVector>& CurveDisplayPoints = HoudiniHandleComponent->CurveDisplayPoints;

	//// Draw the curve.
	//FVector DisplayPointFirst;
	//FVector DisplayPointPrevious;

	//int32 NumDisplayPoints = CurveDisplayPoints.Num();
	//for(int32 DisplayPointIdx = 0; DisplayPointIdx < NumDisplayPoints; ++DisplayPointIdx)
	//{
	//	// Get point for this index.
	//	const FVector& DisplayPoint =
	//		HoudiniHandleComponentTransform.TransformPosition(CurveDisplayPoints[DisplayPointIdx]);

	//	if(DisplayPointIdx > 0)
	//	{
	//		// Draw line from previous point to current one.
	//		PDI->DrawLine(DisplayPointPrevious, DisplayPoint, ColorNormal, SDPG_Foreground);
	//	}
	//	else
	//	{
	//		DisplayPointFirst = DisplayPoint;
	//	}

	//	// If this is last point and curve is closed, draw link from last to first.
	//	if(HoudiniHandleComponent->IsClosedCurve() && (NumDisplayPoints > 1) &&
	//		(DisplayPointIdx + 1 == NumDisplayPoints))
	//	{
	//		PDI->DrawLine(DisplayPointFirst, DisplayPoint, ColorNormal, SDPG_Foreground);
	//	}

	//	DisplayPointPrevious = DisplayPoint;
	//}

	//// Draw control points.
	//for(int32 PointIdx = 0; PointIdx < CurvePoints.Num(); ++PointIdx)
	//{
	//	// Get point at this index.
	//	const FVector& DisplayPoint = HoudiniHandleComponentTransform.TransformPosition(CurvePoints[PointIdx]);

	//	// If we are editing this control point, change color.
	//	FColor ColorCurrent = ColorNormal;
	//	if(bEditing && PointIdx == EditedControlPointIndex)
	//	{
	//		ColorCurrent = ColorSelected;
	//	}

	//	// Draw point and set hit box for it.
	//	PDI->SetHitProxy(new HHoudiniSplineControlPointVisProxy(HoudiniHandleComponent, PointIdx));
	//	PDI->DrawPoint(DisplayPoint, ColorCurrent, GrabHandleSize, SDPG_Foreground);
	//	PDI->SetHitProxy(nullptr);
	//}
}

bool
FHoudiniHandleComponentVisualizer::VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bEditing = false;

	/*if(VisProxy && VisProxy->Component.IsValid())
	{
		const UHoudiniHandleComponent* HoudiniHandleComponent =
			CastChecked<const UHoudiniHandleComponent>(VisProxy->Component.Get());

		EditedHoudiniHandleComponent = const_cast<UHoudiniHandleComponent*>(HoudiniHandleComponent);

		if(HoudiniHandleComponent)
		{
			if(VisProxy->IsA(HHoudiniSplineControlPointVisProxy::StaticGetType()))
			{
				HHoudiniSplineControlPointVisProxy* ControlPointProxy = (HHoudiniSplineControlPointVisProxy*) VisProxy;
				bEditing = true;
				EditedControlPointIndex = ControlPointProxy->ControlPointIndex;
			}
		}
	}*/

	return bEditing;
}

void
FHoudiniHandleComponentVisualizer::EndEditing()
{
	EditedComponent = nullptr;
}

//bool
//FHoudiniHandleComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient,
//	FVector& OutLocation) const
//{
//	if(EditedComponent && EditedControlPointIndex != INDEX_NONE)
//	{
//		// Get curve points.
//		const TArray<FVector>& CurvePoints = EditedComponent->CurvePoints;
//		check(EditedControlPointIndex >= 0 && EditedControlPointIndex < CurvePoints.Num());
//		OutLocation =
//			EditedComponent->ComponentToWorld.TransformPosition(CurvePoints[EditedControlPointIndex]);
//
//		return true;
//	}
//
//	return false;
//}

bool
FHoudiniHandleComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	//if(EditedComponent && EditedControlPointIndex != INDEX_NONE)
	//{
	//	// Get curve points.
	//	const TArray<FVector>& CurvePoints = EditedComponent->CurvePoints;

	//	// Get component transformation.
	//	const FTransform& HoudiniHandleComponentTransform = EditedComponent->ComponentToWorld;

	//	check(EditedControlPointIndex >= 0 && EditedControlPointIndex < CurvePoints.Num());
	//	FVector Point = CurvePoints[EditedControlPointIndex];

	//	// Handle change in translation.
	//	if(!DeltaTranslate.IsZero())
	//	{
	//		FVector PointTransformed = HoudiniHandleComponentTransform.TransformPosition(Point);
	//		FVector PointTransformedDelta = PointTransformed + DeltaTranslate;

	//		Point = HoudiniHandleComponentTransform.InverseTransformPosition(PointTransformedDelta);
	//	}

	//	// Handle change in rotation.
	//	if(!DeltaRotate.IsZero())
	//	{
	//		Point = DeltaRotate.RotateVector(Point);
	//	}

	//	if(!DeltaScale.IsZero())
	//	{
	//		// For now in case of rescaling, do nothing.
	//		return true;
	//	}

	//	NotifyComponentModified(EditedControlPointIndex, Point);
	//	return true;
	//}

	return false;
}

//TSharedPtr<SWidget>
//FHoudiniHandleComponentVisualizer::GenerateContextMenu() const
//{
//	FHoudiniEngineEditor& HoudiniEngineEditor = FHoudiniEngineEditor::Get();
//	TSharedPtr<ISlateStyle> StyleSet = HoudiniEngineEditor.GetSlateStyle();
//
//	FMenuBuilder MenuBuilder(true, VisualizerActions);
//	{
//		MenuBuilder.BeginSection("CurveKeyEdit");
//
//		{
//			if(INDEX_NONE != EditedControlPointIndex)
//			{
//				MenuBuilder.AddMenuEntry(FHoudiniHandleComponentVisualizerCommands::Get().CommandAddControlPoint,
//					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
//					FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"));
//
//				MenuBuilder.AddMenuEntry(FHoudiniHandleComponentVisualizerCommands::Get().CommandDeleteControlPoint,
//					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
//					FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"));
//			}
//		}
//
//		MenuBuilder.EndSection();
//	}
//
//	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
//	return MenuWidget;
//}

void
FHoudiniHandleComponentVisualizer::UpdateHoudiniComponents()
{
	/*if(EditedComponent)
	{
		if(EditedComponent->IsInputCurve())
		{
			EditedComponent->NotifyHoudiniInputCurveChanged();
		}
		else
		{
			UHoudiniAssetComponent* HoudiniAssetComponent =
				Cast<UHoudiniAssetComponent>(EditedComponent->AttachParent);

			if(HoudiniAssetComponent)
			{
				HoudiniAssetComponent->NotifyHoudiniSplineChanged(EditedComponent);
			}
		}

		if(GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}*/
}


void
FHoudiniHandleComponentVisualizer::NotifyComponentModified(int32 PointIndex, const FVector& Point)
{
	if(EditedComponent)
	{
		//UHoudiniAssetComponent* HoudiniAssetComponent =
		//		Cast<UHoudiniAssetComponent>(EditedComponent->AttachParent);

		//FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_EDITOR),
		//	LOCTEXT("HoudiniHandleComponentChange", "Houdini Spline Component: Moving a point"),
		//	HoudiniAssetComponent);
		//EditedComponent->Modify();

		//// Update given control point.
		//EditedComponent->UpdatePoint(PointIndex, Point);
		//EditedComponent->UploadControlPoints();

		//UpdateHoudiniComponents();
	}
}

