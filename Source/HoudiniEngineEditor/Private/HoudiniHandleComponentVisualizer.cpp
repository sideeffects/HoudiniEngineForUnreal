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

IMPLEMENT_HIT_PROXY( HHoudiniHandleVisProxy, HComponentVisProxy );

HHoudiniHandleVisProxy::HHoudiniHandleVisProxy( const UActorComponent * InComponent )
    : HComponentVisProxy( InComponent, HPP_Wireframe )
{}

FHoudiniHandleComponentVisualizerCommands::FHoudiniHandleComponentVisualizerCommands()
    : TCommands< FHoudiniHandleComponentVisualizerCommands >(
        "HoudiniHandleComponentVisualizer",
        LOCTEXT( "HoudiniHandleComponentVisualizer", "Houdini Handle Component Visualizer" ),
        NAME_None,
        FEditorStyle::GetStyleSetName() )
{}

void
FHoudiniHandleComponentVisualizerCommands::RegisterCommands()
{}

FHoudiniHandleComponentVisualizer::FHoudiniHandleComponentVisualizer()
    : FComponentVisualizer()
    , EditedComponent( nullptr )
    , bEditing( false )
{
    FHoudiniHandleComponentVisualizerCommands::Register();
    VisualizerActions = MakeShareable( new FUICommandList );
}

FHoudiniHandleComponentVisualizer::~FHoudiniHandleComponentVisualizer()
{
    FHoudiniHandleComponentVisualizerCommands::Unregister();
}

bool 
FHoudiniHandleComponentVisualizer::HandleInputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event )
{
    if( EditedComponent )
    {
        if( Key == EKeys::LeftMouseButton && Event == IE_Released )
        {
            if( GEditor )
                GEditor->RedrawLevelEditingViewports( true );

            EditedComponent->UpdateTransformParameters();
        }
    }
    return false;
}

void
FHoudiniHandleComponentVisualizer::DrawVisualization(
    const UActorComponent * Component, const FSceneView * View,
    FPrimitiveDrawInterface * PDI )
{
    const UHoudiniHandleComponent * HandleComponent = Cast< const UHoudiniHandleComponent >( Component );
    if ( !HandleComponent )
        return;

    bool IsActive = EditedComponent != nullptr;

    static const FLinearColor ActiveColor( 1.f, 0, 1.f);
    static const FLinearColor InactiveColor( 0.2f, 0.2f, 0.2f, 0.2f );

    // Draw point and set hit box for it.
    PDI->SetHitProxy( new HHoudiniHandleVisProxy( HandleComponent ) );
    {
        static const float GrabHandleSize = 12.0f;
        PDI->DrawPoint( HandleComponent->ComponentToWorld.GetLocation(), IsActive ? ActiveColor : InactiveColor, GrabHandleSize, SDPG_Foreground );
    }

    if( HandleComponent->HandleType == EHoudiniHandleType::Bounder )
    {
        // draw the scale box
        FTransform BoxTransform = HandleComponent->ComponentToWorld;
        const float BoxRad = 50.f;
        const FBox Box( FVector( -BoxRad, -BoxRad, -BoxRad ), FVector( BoxRad, BoxRad, BoxRad ) );
        DrawWireBox( PDI, BoxTransform.ToMatrixWithScale(), Box, IsActive ? ActiveColor : InactiveColor, SDPG_Foreground );
    }
    PDI->SetHitProxy( nullptr );
}

bool
FHoudiniHandleComponentVisualizer::VisProxyHandleClick( 
    FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click )
{
    bEditing = false;
    
    bAllowTranslate = false;
    bAllowRotation = false;
    bAllowScale = false;

    if ( VisProxy && VisProxy->Component.IsValid() )
    {
        const UHoudiniHandleComponent * Component =
            CastChecked< const UHoudiniHandleComponent >( VisProxy->Component.Get() );

        EditedComponent = const_cast< UHoudiniHandleComponent * >( Component );

        if ( Component )
        {
            if ( VisProxy->IsA( HHoudiniHandleVisProxy::StaticGetType() ) )
                bEditing = true;

            bAllowTranslate =
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::TX ].AssetParameter ||
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::TY ].AssetParameter ||
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::TZ ].AssetParameter;

            bAllowRotation =
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::RX ].AssetParameter ||
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::RY ].AssetParameter ||
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::RZ ].AssetParameter;

            bAllowScale =
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::SX ].AssetParameter ||
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::SY ].AssetParameter ||
                Component->XformParms[ UHoudiniHandleComponent::EXformParameter::SZ ].AssetParameter;
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
    const FEditorViewportClient * ViewportClient,
    FVector & OutLocation ) const
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
    const FEditorViewportClient * ViewportClient,
    FMatrix & OutMatrix ) const
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
    FEditorViewportClient * ViewportClient, FViewport * Viewport,
    FVector& DeltaTranslate, FRotator & DeltaRotate, FVector & DeltaScale )
{
    if ( !EditedComponent )
        return false;

    bool bUpdated = false;
    if ( !DeltaTranslate.IsZero() && bAllowTranslate )
    {
        EditedComponent->SetWorldLocation( EditedComponent->ComponentToWorld.GetLocation() + DeltaTranslate );
        bUpdated = true;
    }

    if ( !DeltaRotate.IsZero() && bAllowRotation )
    {
        EditedComponent->SetWorldRotation( DeltaRotate.Quaternion() * EditedComponent->ComponentToWorld.GetRotation() );
        bUpdated = true;
    }

    if ( !DeltaScale.IsZero() && bAllowScale )
    {
        EditedComponent->SetWorldScale3D( EditedComponent->ComponentToWorld.GetScale3D() + DeltaScale );
        bUpdated = true;
    }

    // Don't continuously update bounder because it jitters badly
    if( bUpdated && (EditedComponent->HandleType != EHoudiniHandleType::Bounder) )
    {
        if( GEditor )
            GEditor->RedrawLevelEditingViewports( true );

        EditedComponent->UpdateTransformParameters();
    }

    return true;
}

