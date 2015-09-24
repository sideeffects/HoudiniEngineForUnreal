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

	static const FColor Color(255, 0, 255);
	static const float GrabHandleSize = 12.0f;

	// Draw point and set hit box for it.
	PDI->SetHitProxy(new HHoudiniHandleVisProxy(HandleComponent));
	PDI->DrawPoint( HandleComponent->ComponentToWorld.GetLocation(), Color, GrabHandleSize, SDPG_Foreground );
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
	if ( EditedComponent && ViewportClient->GetWidgetMode() == FWidget::WM_Scale )
	{
		OutMatrix = FRotationMatrix::Make( EditedComponent->ComponentToWorld.GetRotation() );
		return true;
	}
	else
	{
		return false;
	}
}

bool
FHoudiniHandleComponentVisualizer::HandleInputDelta(
	FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale
)
{
	if ( !EditedComponent )
	{
		return false;
	}

	bool bUpdated = false;
	if ( !DeltaTranslate.IsZero() )
	{
		EditedComponent->SetWorldLocation(EditedComponent->ComponentToWorld.GetLocation() + DeltaTranslate);
		bUpdated = true;
	}

	if ( !DeltaRotate.IsZero() )
	{
		EditedComponent->SetWorldRotation(DeltaRotate.Quaternion() * EditedComponent->ComponentToWorld.GetRotation());
		bUpdated = true;
	}

	if ( !DeltaScale.IsZero() )
	{
		EditedComponent->SetWorldScale3D( EditedComponent->ComponentToWorld.GetScale3D() + DeltaScale );
		bUpdated = true;
	}

	if ( bUpdated )
	{
		if ( GEditor )
		{
			GEditor->RedrawLevelEditingViewports(true);
		}

		EditedComponent->UpdateTransformParameters();
	}	

	return true;
}

