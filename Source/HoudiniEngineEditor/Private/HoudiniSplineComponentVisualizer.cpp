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

IMPLEMENT_HIT_PROXY( HHoudiniSplineVisProxy, HComponentVisProxy );
IMPLEMENT_HIT_PROXY( HHoudiniSplineControlPointVisProxy, HHoudiniSplineVisProxy );

HHoudiniSplineVisProxy::HHoudiniSplineVisProxy( const UActorComponent * InComponent )
    : HComponentVisProxy( InComponent, HPP_Wireframe )
{}

HHoudiniSplineControlPointVisProxy::HHoudiniSplineControlPointVisProxy(
    const UActorComponent * InComponent, int32 InControlPointIndex )
    : HHoudiniSplineVisProxy( InComponent )
    , ControlPointIndex( InControlPointIndex )
{}

FHoudiniSplineComponentVisualizerCommands::FHoudiniSplineComponentVisualizerCommands()
    : TCommands< FHoudiniSplineComponentVisualizerCommands >(
        "HoudiniSplineComponentVisualizer",
        LOCTEXT( "HoudiniSplineComponentVisualizer", "Houdini Spline Component Visualizer" ),
        NAME_None,
        FEditorStyle::GetStyleSetName() )
{}

void
FHoudiniSplineComponentVisualizerCommands::RegisterCommands()
{
    UI_COMMAND(
        CommandAddControlPoint, "Add Control Point", "Add Control Point.",
        EUserInterfaceActionType::Button, FInputGesture() );

    UI_COMMAND(
        CommandDuplicateControlPoint, "Duplicate Control Point", "Duplicate Control Point.",
        EUserInterfaceActionType::Button, FInputGesture());

    UI_COMMAND(
        CommandDeleteControlPoint, "Delete Control Point", "Delete Control Point.",
        EUserInterfaceActionType::Button, FInputGesture(EKeys::Delete) );
}

FHoudiniSplineComponentVisualizer::FHoudiniSplineComponentVisualizer()
    : FComponentVisualizer()
    , EditedHoudiniSplineComponent( nullptr )
    , bCurveEditing( false )
    , bAllowDuplication( true )
    , bIsMultiSelecting ( false )
{
    FHoudiniSplineComponentVisualizerCommands::Register();
    VisualizerActions = MakeShareable( new FUICommandList );
}

FHoudiniSplineComponentVisualizer::~FHoudiniSplineComponentVisualizer()
{
    FHoudiniSplineComponentVisualizerCommands::Unregister();
}

void
FHoudiniSplineComponentVisualizer::OnRegister()
{
    const auto & Commands = FHoudiniSplineComponentVisualizerCommands::Get();

    VisualizerActions->MapAction(
        Commands.CommandAddControlPoint,
        FExecuteAction::CreateSP( this, &FHoudiniSplineComponentVisualizer::OnAddControlPoint ),
        FCanExecuteAction::CreateSP( this, &FHoudiniSplineComponentVisualizer::IsAddControlPointValid ) );

    VisualizerActions->MapAction(
        Commands.CommandDuplicateControlPoint,
        FExecuteAction::CreateSP(this, &FHoudiniSplineComponentVisualizer::OnDuplicateControlPoint),
        FCanExecuteAction::CreateSP(this, &FHoudiniSplineComponentVisualizer::IsDuplicateControlPointValid));

    VisualizerActions->MapAction(
        Commands.CommandDeleteControlPoint,
        FExecuteAction::CreateSP( this, &FHoudiniSplineComponentVisualizer::OnDeleteControlPoint ),
        FCanExecuteAction::CreateSP( this, &FHoudiniSplineComponentVisualizer::IsDeleteControlPointValid ) );
}

void
FHoudiniSplineComponentVisualizer::DrawVisualization(
    const UActorComponent * Component, const FSceneView * View,
    FPrimitiveDrawInterface * PDI )
{
    const UHoudiniSplineComponent * HoudiniSplineComponent = Cast< const UHoudiniSplineComponent >( Component );
    if ( HoudiniSplineComponent && HoudiniSplineComponent->IsValidCurve() )
    {
        static const FColor ColorNormal( 255, 255, 255 );
        static const FColor ColorSelected( 255, 0, 0 );

        static const float GrabHandleSize = 12.0f;

        // Get component transformation.
        const FTransform & HoudiniSplineComponentTransform = HoudiniSplineComponent->ComponentToWorld;

        // Get curve points.
        const TArray< FVector > & CurvePoints = HoudiniSplineComponent->CurvePoints;
        const TArray< FVector > & CurveDisplayPoints = HoudiniSplineComponent->CurveDisplayPoints;

        // Draw the curve.
        FVector DisplayPointFirst;
        FVector DisplayPointPrevious;

        int32 NumDisplayPoints = CurveDisplayPoints.Num();
        for ( int32 DisplayPointIdx = 0; DisplayPointIdx < NumDisplayPoints; ++DisplayPointIdx )
        {
            // Get point for this index.
            const FVector & DisplayPoint =
                HoudiniSplineComponentTransform.TransformPosition( CurveDisplayPoints[ DisplayPointIdx ] );

            if ( DisplayPointIdx > 0 )
            {
                // Draw line from previous point to current one.
                PDI->DrawLine( DisplayPointPrevious, DisplayPoint, ColorNormal, SDPG_Foreground );
            }
            else
            {
                DisplayPointFirst = DisplayPoint;
            }

            // If this is last point and curve is closed, draw link from last to first.
            if ( HoudiniSplineComponent->IsClosedCurve() && NumDisplayPoints > 1 &&
                DisplayPointIdx + 1 == NumDisplayPoints )
            {
                PDI->DrawLine( DisplayPointFirst, DisplayPoint, ColorNormal, SDPG_Foreground );
            }

            DisplayPointPrevious = DisplayPoint;
        }

        // Draw control points.
        for ( int32 PointIdx = 0; PointIdx < CurvePoints.Num(); ++PointIdx )
        {
            // Get point at this index.
            const FVector & DisplayPoint = HoudiniSplineComponentTransform.TransformPosition( CurvePoints[ PointIdx ] );

            // If we are editing this control point, change color.
            FColor ColorCurrent = ColorNormal;
            if ( bCurveEditing &&  EditedControlPointsIndexes.Contains(PointIdx) )
                ColorCurrent = ColorSelected;

            // Draw point and set hit box for it.
            PDI->SetHitProxy( new HHoudiniSplineControlPointVisProxy( HoudiniSplineComponent, PointIdx ) );
            PDI->DrawPoint( DisplayPoint, ColorCurrent, GrabHandleSize, SDPG_Foreground );
            PDI->SetHitProxy( nullptr );
        }
    }
}

bool
FHoudiniSplineComponentVisualizer::VisProxyHandleClick(
    FLevelEditorViewportClient * InViewportClient,
    HComponentVisProxy * VisProxy, const FViewportClick & Click )
{
    bCurveEditing = false;
    if ( !VisProxy || !VisProxy->Component.IsValid() )
        return bCurveEditing;

    const UHoudiniSplineComponent * HoudiniSplineComponent =
        CastChecked< const UHoudiniSplineComponent >( VisProxy->Component.Get() );

    EditedHoudiniSplineComponent = const_cast<UHoudiniSplineComponent *>(HoudiniSplineComponent);

    if ( !HoudiniSplineComponent )
        return bCurveEditing;

    if ( !VisProxy->IsA(HHoudiniSplineControlPointVisProxy::StaticGetType()) )
        return bCurveEditing;

    HHoudiniSplineControlPointVisProxy * ControlPointProxy = (HHoudiniSplineControlPointVisProxy *) VisProxy;
    if ( !ControlPointProxy )
        return bCurveEditing;

    bCurveEditing = true;

    // If we are right-clicking, we dont want to select a new point unless the selection is empty/just one point...
    bool bRightClick = Click.GetKey() == EKeys::RightMouseButton;
    if (bRightClick && EditedControlPointsIndexes.Num() > 1)
        return bCurveEditing;

    if ( bIsMultiSelecting )
    {
        // Add to multi-selection
        if ( !EditedControlPointsIndexes.Contains(ControlPointProxy->ControlPointIndex) )
            EditedControlPointsIndexes.Add(ControlPointProxy->ControlPointIndex);
	else
	    EditedControlPointsIndexes.Remove(ControlPointProxy->ControlPointIndex);
    }
    else
    {
        // Single selection
        EditedControlPointsIndexes.Empty();
        EditedControlPointsIndexes.Add(ControlPointProxy->ControlPointIndex);
    }

    return bCurveEditing;
}

bool
FHoudiniSplineComponentVisualizer::HandleInputKey(
    FEditorViewportClient * ViewportClient, FViewport * Viewport, FKey Key, EInputEvent Event )
{
    bool bHandled = false;

    if ( Key == EKeys::LeftMouseButton && Event == IE_Released )
    {
        // Reset duplication flag on LMB release.
        bAllowDuplication = true;
    }

    if ( Key == EKeys::Delete && Event == IE_Pressed ) 
    {
        if (IsDeleteControlPointValid())
        {
            OnDeleteControlPoint();
            return true;
        }    
    }

    if ( Key == EKeys::LeftControl || Key == EKeys::RightControl )
    {
        if ( Event == IE_Pressed )
            bIsMultiSelecting = true;
        else if ( Event == IE_Released )
            bIsMultiSelecting = false;
    }


    if ( Event == IE_Pressed )
        bHandled = VisualizerActions->ProcessCommandBindings( Key, FSlateApplication::Get().GetModifierKeys(), false );

    return bHandled;
}

void
FHoudiniSplineComponentVisualizer::EndEditing()
{
    EditedHoudiniSplineComponent = nullptr;
    EditedControlPointsIndexes.Empty();
}

bool
FHoudiniSplineComponentVisualizer::GetWidgetLocation(
    const FEditorViewportClient * ViewportClient,
    FVector & OutLocation ) const
{
    if ( !EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() <= 0 )
        return false;

    // Get curve points.
    const TArray< FVector > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    int32 nCurrentCPIndex = -1;
    FVector LocationSum = FVector::ZeroVector;

    for (int n = 0; n < EditedControlPointsIndexes.Num(); n++)
    {
        nCurrentCPIndex = EditedControlPointsIndexes[n];
        if( nCurrentCPIndex < 0 || nCurrentCPIndex >= CurvePoints.Num() )
            continue;

        LocationSum += CurvePoints[nCurrentCPIndex];
    }

    LocationSum /= EditedControlPointsIndexes.Num();
    OutLocation = EditedHoudiniSplineComponent->ComponentToWorld.TransformPosition(LocationSum);

    return true;
}

bool
FHoudiniSplineComponentVisualizer::HandleInputDelta(
    FEditorViewportClient * ViewportClient, FViewport * Viewport,
    FVector & DeltaTranslate, FRotator & DeltaRotate, FVector & DeltaScale)
{
    if (!EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() <= 0)
        return false;

    if ( ViewportClient->IsAltPressed() && bAllowDuplication )
    {
        OnDuplicateControlPoint();
        // Don't duplicate again until we release LMB
        bAllowDuplication = false;
    }

    // Get curve points.
    const TArray< FVector > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    // Get component transformation.
    const FTransform & HoudiniSplineComponentTransform = EditedHoudiniSplineComponent->ComponentToWorld;
    

    FVector CurrentPoint = FVector::ZeroVector;
    for ( int n = 0; n < EditedControlPointsIndexes.Num(); n++ )
    {
        // Get current point from the selected points
        int32 nCurrentCPIndex = EditedControlPointsIndexes[n];
        if (nCurrentCPIndex < 0 || nCurrentCPIndex >= CurvePoints.Num())
            continue;

        CurrentPoint = CurvePoints[nCurrentCPIndex];

	// Handle change in translation.
	if (!DeltaTranslate.IsZero())
	{
            FVector PointTransformed = HoudiniSplineComponentTransform.TransformPosition(CurrentPoint);
	    FVector PointTransformedDelta = PointTransformed + DeltaTranslate;

            CurrentPoint = HoudiniSplineComponentTransform.InverseTransformPosition(PointTransformedDelta);
	}

	// Handle change in rotation.
	if (!DeltaRotate.IsZero())
            CurrentPoint = DeltaRotate.RotateVector(CurrentPoint);

	if (!DeltaScale.IsZero())
	{
	    // For now in case of rescaling, do nothing.
        continue;
	}

        NotifyComponentModified(nCurrentCPIndex, CurrentPoint);
    }
    
    return true;
}

TSharedPtr< SWidget >
FHoudiniSplineComponentVisualizer::GenerateContextMenu() const
{
    FHoudiniEngineEditor& HoudiniEngineEditor = FHoudiniEngineEditor::Get();
    TSharedPtr< ISlateStyle > StyleSet = HoudiniEngineEditor.GetSlateStyle();

    FMenuBuilder MenuBuilder( true, VisualizerActions );
    {
        MenuBuilder.BeginSection( "CurveKeyEdit" );

        {
            if ( EditedControlPointsIndexes.Num() > 0 )
            {
                MenuBuilder.AddMenuEntry(
                    FHoudiniSplineComponentVisualizerCommands::Get().CommandAddControlPoint,
                    NAME_None, TAttribute< FText >(), TAttribute< FText >(),
                    FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ) );

                MenuBuilder.AddMenuEntry(
                    FHoudiniSplineComponentVisualizerCommands::Get().CommandDuplicateControlPoint,
                    NAME_None, TAttribute< FText >(), TAttribute< FText >(),
                    FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"));

                MenuBuilder.AddMenuEntry(
                    FHoudiniSplineComponentVisualizerCommands::Get().CommandDeleteControlPoint,
                    NAME_None, TAttribute< FText >(), TAttribute< FText >(),
                    FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ) );
            }
        }

        MenuBuilder.EndSection();
    }

    TSharedPtr< SWidget > MenuWidget = MenuBuilder.MakeWidget();
    return MenuWidget;
}

void
FHoudiniSplineComponentVisualizer::UpdateHoudiniComponents()
{
    if ( !EditedHoudiniSplineComponent )
        return;

    if (EditedHoudiniSplineComponent->IsInputCurve())
    {
	EditedHoudiniSplineComponent->NotifyHoudiniInputCurveChanged();
    }
    else
    {
	UHoudiniAssetComponent * HoudiniAssetComponent =
	    Cast< UHoudiniAssetComponent >(EditedHoudiniSplineComponent->GetAttachParent());

	if (HoudiniAssetComponent)
	    HoudiniAssetComponent->NotifyHoudiniSplineChanged(EditedHoudiniSplineComponent);
    }

    if (GEditor)
	GEditor->RedrawLevelEditingViewports(true);
}

void
FHoudiniSplineComponentVisualizer::NotifyComponentModified( int32 PointIndex, const FVector & Point )
{
    if ( !EditedHoudiniSplineComponent )
        return;

    UHoudiniAssetComponent * HoudiniAssetComponent =
        Cast< UHoudiniAssetComponent >( EditedHoudiniSplineComponent->GetAttachParent() );

    FScopedTransaction Transaction( TEXT( HOUDINI_MODULE_EDITOR ),
        LOCTEXT( "HoudiniSplineComponentChange", "Houdini Spline Component: Moving a point" ),
        HoudiniAssetComponent );
    EditedHoudiniSplineComponent->Modify();

    // Update given control point.
    EditedHoudiniSplineComponent->UpdatePoint( PointIndex, Point );
    EditedHoudiniSplineComponent->UploadControlPoints();

    UpdateHoudiniComponents();
}

void
FHoudiniSplineComponentVisualizer::OnAddControlPoint()
{
    if ( !EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() < 0 )
        return;    

    // Transaction for Undo/Redo
    UHoudiniAssetComponent * HoudiniAssetComponent =
        Cast< UHoudiniAssetComponent >(EditedHoudiniSplineComponent->GetAttachParent());

    if (!HoudiniAssetComponent)
        return;

    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_EDITOR),
        LOCTEXT("HoudiniSplineComponentChange", "Houdini Spline Component: Adding a control point"),
        HoudiniAssetComponent);

    EditedHoudiniSplineComponent->Modify();

    FVector OtherPoint = FVector::ZeroVector;
    FVector CurrentPoint = FVector::ZeroVector;
    int32 nCurrentCPIndex = -1;

    // We need to sort the selection to insert the new nodes properly
    EditedControlPointsIndexes.Sort();

    const TArray< FVector > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    TArray<int32> tNewSelection;
    for (int32 n = EditedControlPointsIndexes.Num() - 1; n >= 0 ; n--)
    {
        // Get current point from the selected points
        nCurrentCPIndex = EditedControlPointsIndexes[n];
        if (nCurrentCPIndex < 0 || nCurrentCPIndex >= CurvePoints.Num())
            continue;

        CurrentPoint = CurvePoints[nCurrentCPIndex];

        // Select the other point
        if (nCurrentCPIndex + 1 != CurvePoints.Num())
	{
            OtherPoint = CurvePoints[nCurrentCPIndex + 1];
	}
	else
	{
	    if (EditedHoudiniSplineComponent->bClosedCurve)
	    {
		OtherPoint = CurvePoints[0];
	    }
	    else
	    {
                OtherPoint = CurvePoints[nCurrentCPIndex - 1];
                nCurrentCPIndex--;
	    }
	}

        FVector NewPoint = CurrentPoint + (OtherPoint - CurrentPoint) / 2.0f;
        int32 NewPointIndex = AddControlPointAfter(NewPoint, nCurrentCPIndex);

        tNewSelection.Add(NewPointIndex);
    }

    // Update the spline component
    EditedHoudiniSplineComponent->UploadControlPoints();
    UpdateHoudiniComponents();

    // Select the new points.
    EditedControlPointsIndexes.Empty();
    EditedControlPointsIndexes = tNewSelection;
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
    if (!EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() <= 0)
        return;

    UHoudiniAssetComponent * HoudiniAssetComponent =
        Cast< UHoudiniAssetComponent >(EditedHoudiniSplineComponent->GetAttachParent() );

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_EDITOR ),
        LOCTEXT( "HoudiniSplineComponentChange", "Houdini Spline Component: Removing a control point" ),
        HoudiniAssetComponent );
    EditedHoudiniSplineComponent->Modify();

    // We need to sort the selection to delete the nodes properly
    EditedControlPointsIndexes.Sort();
    
    int32 nOffset = 0;
    TArray<int32> tNewSelection;
    for (int32 n = 0; n < EditedControlPointsIndexes.Num(); n++)
    {
        int32 nIndex = EditedControlPointsIndexes[n] - nOffset;
        EditedHoudiniSplineComponent->RemovePoint(nIndex);

        if (!tNewSelection.Contains(--nIndex))
            tNewSelection.Add(nIndex);	
        nOffset++;
    }

    // Select previous points if possible
    for (int32 n = tNewSelection.Num() - 1; n >= 0; n--)
    {
        // get the previous point
        if (tNewSelection[n] < 0)
            tNewSelection[n] = 0;

        // if the new index is invalid, or has been removed, unselect it
        if (tNewSelection[n] >= EditedHoudiniSplineComponent->CurvePoints.Num() || EditedControlPointsIndexes.Contains(tNewSelection[n]))
        {
            tNewSelection.RemoveAt(n);
	}
    }

    if(tNewSelection.Num() > 0)
        EditedControlPointsIndexes = tNewSelection;
    else
    {
        EditedControlPointsIndexes.Empty();
        EditedControlPointsIndexes.Add(0);
    }

    /*
    for (int32 n = EditedControlPointsIndexes.Num() - 1; n >= 0; n--)
    {
        EditedHoudiniSplineComponent->RemovePoint(EditedControlPointsIndexes[n]);	
    }

    // Select previous points if possible
    TArray<int32> tDeletedPoints = EditedControlPointsIndexes;
    for (int32 n = EditedControlPointsIndexes.Num() - 1; n >= 0; n--)
    {
        // get the previous point
        int32 nNewIndex = EditedControlPointsIndexes[n] - 1;
        if (nNewIndex < 0)
            nNewIndex = 0;

        // if the new index is invalid, or has been removed, unselect it
        if ( nNewIndex >= EditedHoudiniSplineComponent->CurvePoints.Num() || tDeletedPoints.Contains(nNewIndex) )
        {
            EditedControlPointsIndexes.RemoveAt(n);
        }
        else
        {
            EditedControlPointsIndexes[n] = nNewIndex;
        }
    }

    if (EditedControlPointsIndexes.Num() == 0)
        EditedControlPointsIndexes.Add(0);
    */

    // Update the spline object
    EditedHoudiniSplineComponent->UploadControlPoints();
    UpdateHoudiniComponents();
}

bool
FHoudiniSplineComponentVisualizer::IsDeleteControlPointValid() const
{
    if ( !EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() <= 0 )
        return false;

    // We can only delete points if we have more than two points.
    if ( EditedHoudiniSplineComponent->GetCurvePointCount() < 2 )
        return false;

    // We need to leave 2 points after deleting
    if ( EditedHoudiniSplineComponent->GetCurvePointCount() - EditedControlPointsIndexes.Num() < 2 )
	return false;

    return true;
}

int32 FHoudiniSplineComponentVisualizer::AddControlPointAfter( const FVector & NewPoint, const int32& nIndex )
{
    if ( !EditedHoudiniSplineComponent )
        return nIndex;

    const TArray< FVector > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;
    check( nIndex >= 0 && nIndex < CurvePoints.Num() );

    int32 ControlPointIndex = nIndex + 1;
    if (ControlPointIndex == CurvePoints.Num())
        EditedHoudiniSplineComponent->AddPoint( NewPoint );
    else
        EditedHoudiniSplineComponent->AddPoint( ControlPointIndex, NewPoint);

    // Return the newly created point index.
    return ControlPointIndex;
}

void
FHoudiniSplineComponentVisualizer::OnDuplicateControlPoint()
{
    if ( !EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() <= 0 )
        return;

    UHoudiniAssetComponent * HoudiniAssetComponent =
        Cast< UHoudiniAssetComponent >(EditedHoudiniSplineComponent->GetAttachParent());

    if (!HoudiniAssetComponent)
        return;

    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_EDITOR),
        LOCTEXT("HoudiniSplineComponentChange", "Houdini Spline Component: Adding a control point"),
        HoudiniAssetComponent);

    EditedHoudiniSplineComponent->Modify();

    const TArray< FVector > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    //
    EditedControlPointsIndexes.Sort();

    int32 nCurrentCPIndex = -1;
    int32 nNewCPIndex = -1;
    TArray<int32> tNewSelection;

    FVector NewPoint = FVector::ZeroVector;
    int nOffset = 0;
    for ( int32 n = 0; n < EditedControlPointsIndexes.Num(); n++ )
    {
        // Get current point from the selected points
        nCurrentCPIndex = EditedControlPointsIndexes[n] + nOffset;
        if (nCurrentCPIndex < 0 || nCurrentCPIndex >= CurvePoints.Num())
            continue;

	// We just add the new point on top of the existing point.
        NewPoint = CurvePoints[nCurrentCPIndex];

	// Add the new point and select it.
        nNewCPIndex = AddControlPointAfter(NewPoint, nCurrentCPIndex);

        tNewSelection.Add(nNewCPIndex);
        nOffset++;
    }

    // Update the spline component
    EditedHoudiniSplineComponent->UploadControlPoints();
    UpdateHoudiniComponents();

    // Select the new points.
    EditedControlPointsIndexes.Empty();
    EditedControlPointsIndexes = tNewSelection;
}

bool
FHoudiniSplineComponentVisualizer::IsDuplicateControlPointValid() const
{
    // We can only duplicate points if we have selected a point.
    if (!EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() <= 0 )
        return false;

    return true;
}