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
FHoudiniHandleComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	const UHoudiniHandleComponent* HandleComponent = Cast<const UHoudiniHandleComponent>(Component);
	if (!HandleComponent)
	{
		return;
	}

	static const FColor ColorNormal(255, 255, 255);
	static const FColor ColorSelected(255, 0, 255);

	static const float GrabHandleSize = 12.0f;

	// Draw point and set hit box for it.
	PDI->SetHitProxy(new HHoudiniHandleVisProxy(HandleComponent));
	PDI->DrawPoint( HandleComponent->ComponentToWorld.GetLocation(), ColorSelected, GrabHandleSize, SDPG_Foreground );
	PDI->SetHitProxy(nullptr);
}

bool
FHoudiniHandleComponentVisualizer::VisProxyHandleClick(FLevelEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bEditing = false;

	if ( VisProxy && VisProxy->Component.IsValid() )
	{
		const UHoudiniHandleComponent* Component =
			CastChecked<const UHoudiniHandleComponent>(VisProxy->Component.Get());

		EditedComponent = const_cast<UHoudiniHandleComponent*>(Component);

		if(Component)
		{
			if ( VisProxy->IsA(HHoudiniHandleVisProxy::StaticGetType()) )
			{
				bEditing = true;
			}
		}
	}

	return bEditing;
}

void
FHoudiniHandleComponentVisualizer::EndEditing()
{
	EditedComponent = nullptr;
}

bool
FHoudiniHandleComponentVisualizer::GetWidgetLocation(
	const FEditorViewportClient* ViewportClient,
	FVector& OutLocation
) const
{
	if ( EditedComponent )
	{
		OutLocation = EditedComponent->ComponentToWorld.GetLocation();
		return true;
	}

	return false;
}

bool
FHoudiniHandleComponentVisualizer::GetCustomInputCoordinateSystem(
	const FEditorViewportClient* ViewportClient,
	FMatrix& OutMatrix
) const
{	
	/*if ( EditedComponent )
	{
		OutMatrix = EditedComponent->ComponentToWorld;
		return true;
	}
	else*/
	{
		return false;
	}
}

bool
FHoudiniHandleComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if ( EditedComponent )
	{
		// Handle change in translation.
		if ( !DeltaTranslate.IsZero() )
		{
			FTransform UnrealXform;
			UnrealXform.SetTranslation(DeltaTranslate);

			HAPI_Transform HapiXform;
			FHoudiniEngineUtils::TranslateUnrealTransform(UnrealXform, HapiXform);

			EditedComponent->XformParms[UHoudiniHandleComponent::EXformParameter::TX] += HapiXform.position[0];
			EditedComponent->XformParms[UHoudiniHandleComponent::EXformParameter::TY] += HapiXform.position[1];
			EditedComponent->XformParms[UHoudiniHandleComponent::EXformParameter::TZ] += HapiXform.position[2];
		}

		if(GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
		return true;
	}

	return false;
}

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

