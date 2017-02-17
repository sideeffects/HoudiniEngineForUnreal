/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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

#include "HoudiniApi.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineEditor.h"
#include "EditorViewportClient.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

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
    , CachedRotation( FQuat::Identity )
    , bComponentNeedUpdate( false )
    , bCookOnlyOnMouseRelease( false )
    , bRecordTransactionOnMove( true )
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
    if ( HoudiniSplineComponent && HoudiniSplineComponent->IsValidCurve() && HoudiniSplineComponent->IsActive() )
    {   
        static const FColor ColorNormal = FColor(255, 255, 255);
        static const FColor ColorFirst(0, 192, 0);
        static const FColor ColorSecond(255, 159, 0);
        static const FColor ColorSelected(255, 0, 0);

        static const FColor ColorNone = FColor(172, 172, 172);
        static const FColor ColorNoneFirst = FColor(172, 255, 172);
        static const FColor ColorNoneSecond = FColor(254, 216, 177);

        static const float GrabHandleSize = 12.0f;
        static const float GrabHandleSizeNone = 12.0f;// 8.0f;
        static const float GrabHandleSizeSelected = 13.0f;

        // Get component transformation.
        const FTransform & HoudiniSplineComponentTransform = HoudiniSplineComponent->ComponentToWorld;

        // Get curve points.
        const TArray< FTransform > & CurvePoints = HoudiniSplineComponent->CurvePoints;
        const TArray< FVector > & CurveDisplayPoints = HoudiniSplineComponent->CurveDisplayPoints;

        // Draw the curve.
        FVector DisplayPointFirst;
        FVector DisplayPointPrevious;

        // Dim the color if no points is selected
        bool bNoPointSelected = EditedControlPointsIndexes.Num() <= 0;
        float GrabHandleCurrentSize = bNoPointSelected ? GrabHandleSizeNone : GrabHandleSize;

        int32 NumDisplayPoints = CurveDisplayPoints.Num();
        for ( int32 DisplayPointIdx = 0; DisplayPointIdx < NumDisplayPoints; ++DisplayPointIdx )
        {
            // Get point for this index.
            const FVector & DisplayPoint =
                HoudiniSplineComponentTransform.TransformPosition( CurveDisplayPoints[ DisplayPointIdx ] );

            if ( DisplayPointIdx > 0 )
            {
                // Draw line from previous point to current one.
                PDI->DrawLine( DisplayPointPrevious, DisplayPoint, bNoPointSelected ? ColorNone : ColorNormal, SDPG_Foreground );
            }
            else
            {
                DisplayPointFirst = DisplayPoint;
            }

            // If this is last point and curve is closed, draw link from last to first.
            if ( HoudiniSplineComponent->IsClosedCurve() && NumDisplayPoints > 1 &&
                DisplayPointIdx + 1 == NumDisplayPoints )
            {
                PDI->DrawLine( DisplayPointFirst, DisplayPoint, bNoPointSelected ? ColorNone : ColorNormal, SDPG_Foreground );
            }

            DisplayPointPrevious = DisplayPoint;
        }

        // Draw control points.
        for ( int32 PointIdx = 0; PointIdx < CurvePoints.Num(); ++PointIdx )
        {
            // Get point at this index.
            const FVector & DisplayPoint = HoudiniSplineComponentTransform.TransformPosition( CurvePoints[ PointIdx ].GetLocation() );

            // Draw point and set hit box for it.
            PDI->SetHitProxy( new HHoudiniSplineControlPointVisProxy( HoudiniSplineComponent, PointIdx ) );
       
            if ( ( bCurveEditing ) && ( EditedControlPointsIndexes.Contains(PointIdx)))
            {
                // If we are editing this control point, change its color
                PDI->DrawPoint(DisplayPoint, ColorSelected, GrabHandleSizeSelected, SDPG_Foreground);
            }
            else
            {
                // Color the first two points differently to show the direction of the spline
                if( PointIdx == 0 )
                    PDI->DrawPoint(DisplayPoint, bNoPointSelected ? ColorNoneFirst : ColorFirst, GrabHandleCurrentSize, SDPG_Foreground);
                else if (PointIdx == 1)
                    PDI->DrawPoint(DisplayPoint, bNoPointSelected ? ColorNoneSecond : ColorSecond, GrabHandleCurrentSize, SDPG_Foreground);
                else
                    PDI->DrawPoint(DisplayPoint, bNoPointSelected ? ColorNone : ColorNormal, GrabHandleCurrentSize, SDPG_Foreground);
            }

            PDI->SetHitProxy( nullptr );
        }
    }
}

bool
FHoudiniSplineComponentVisualizer::VisProxyHandleClick(
    FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click )
{
    bCurveEditing = false;
    if ( !VisProxy || !VisProxy->Component.IsValid() )
        return bCurveEditing;

    const UHoudiniSplineComponent * HoudiniSplineComponent =
        CastChecked< const UHoudiniSplineComponent >(VisProxy->Component.Get());

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

    bool bIsMultiSelecting = false;
    FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
    if (ModifierKeys.IsControlDown())
        bIsMultiSelecting = true;

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

    CacheRotation();

    return bCurveEditing;
}

bool
FHoudiniSplineComponentVisualizer::HandleInputKey(
    FEditorViewportClient * ViewportClient, FViewport * Viewport, FKey Key, EInputEvent Event )
{
    bool bHandled = false;

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    bCookOnlyOnMouseRelease = HoudiniRuntimeSettings->bCookCurvesOnMouseRelease;

    if ( Key == EKeys::LeftMouseButton && Event == IE_Released )
    {
        // Updates the spline
        if ( bComponentNeedUpdate )
            UpdateHoudiniComponents();

        // Reset duplication flag on LMB release.
        bAllowDuplication = true;

        // Reset the transaction flag
        bRecordTransactionOnMove = true;

        // Re-cache the rotation
        CacheRotation();
    }

    if (Key == EKeys::Delete && Event == IE_Pressed) 
    {
        if (IsDeleteControlPointValid())
        {
            OnDeleteControlPoint();
            return true;
        }    
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
    const TArray< FTransform > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    int32 nCurrentCPIndex = -1;
    FVector LocationSum = FVector::ZeroVector;

    for (int n = 0; n < EditedControlPointsIndexes.Num(); n++)
    {
        nCurrentCPIndex = EditedControlPointsIndexes[n];
        if( nCurrentCPIndex < 0 || nCurrentCPIndex >= CurvePoints.Num() )
            continue;

        LocationSum += CurvePoints[nCurrentCPIndex].GetLocation();
    }

    LocationSum /= EditedControlPointsIndexes.Num();
    OutLocation = EditedHoudiniSplineComponent->ComponentToWorld.TransformPosition(LocationSum);

    return true;
}

bool
FHoudiniSplineComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
    // Change the system only for the rotation gizmo or if in LocalSpace
    if (ViewportClient->GetWidgetCoordSystemSpace() != COORD_Local && ViewportClient->GetWidgetMode() != FWidget::WM_Rotate)
        return false;

    // We only orient the widget if we select one point
    if (!EditedHoudiniSplineComponent || EditedControlPointsIndexes.Num() != 1)
        return false;

    // Get the selected curve point
    /*
    int32 nCurrentCPIndex = EditedControlPointsIndexes[0];
    FTransform transf = EditedHoudiniSplineComponent->CurvePoints[nCurrentCPIndex];
    transf.SetLocation(FVector::ZeroVector);
    OutMatrix = transf.ToMatrixNoScale();
    */

    OutMatrix = FRotationMatrix::Make(CachedRotation);

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
        DuplicateControlPoint();

        // Don't duplicate again until we release LMB
        bAllowDuplication = false;
    }

    // Get curve points.
    const TArray< FTransform > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    // Get component transformation.
    const FTransform & HoudiniSplineComponentTransform = EditedHoudiniSplineComponent->ComponentToWorld;

    FTransform CurrentPoint = FTransform::Identity;
    for ( int n = 0; n < EditedControlPointsIndexes.Num(); n++ )
    {
        // Get current point from the selected points
        int32 nCurrentCPIndex = EditedControlPointsIndexes[n];
        if (nCurrentCPIndex < 0 || nCurrentCPIndex >= CurvePoints.Num())
            continue;

        CurrentPoint = CurvePoints[nCurrentCPIndex];

        // Handle change in translation.
        if ( !DeltaTranslate.IsZero() )
        {

            FVector PointTransformed = HoudiniSplineComponentTransform.TransformPosition(CurrentPoint.GetLocation());   // Get Position in world space
            FVector PointTransformedDelta = PointTransformed + DeltaTranslate;                                          // apply delta in world space
            PointTransformed = HoudiniSplineComponentTransform.InverseTransformPosition(PointTransformedDelta);         // convert back to local
            CurrentPoint.SetLocation(PointTransformed);
        }

        // Handle change in rotation.
        if ( !DeltaRotate.IsZero() )
        {
            FQuat NewRot = HoudiniSplineComponentTransform.GetRotation() * CurrentPoint.GetRotation();  // convert local-space rotation to world-space
            NewRot = DeltaRotate.Quaternion() * NewRot;                                                 // apply world-space rotation
            NewRot = HoudiniSplineComponentTransform.GetRotation().Inverse() * NewRot;                  // convert world-space rotation to local-space
            CurrentPoint.SetRotation(NewRot);
        }

        // Handle change in scale
        if (!DeltaScale.IsZero())
        {
            FVector NewScale = CurrentPoint.GetScale3D() * (FVector(1,1,1) + DeltaScale);
            CurrentPoint.SetScale3D( NewScale );
        }

        NotifyComponentModified(nCurrentCPIndex, CurrentPoint);
    }

    if ( ( bComponentNeedUpdate ) &&  ( !bCookOnlyOnMouseRelease ) )
    {
        // Update and cook the asset
        UpdateHoudiniComponents();
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
    if ( EditedHoudiniSplineComponent )
        EditedHoudiniSplineComponent->UpdateHoudiniComponents();

    bComponentNeedUpdate = false;
}

void
FHoudiniSplineComponentVisualizer::NotifyComponentModified( int32 PointIndex, const FTransform & Point )
{
    if ( !EditedHoudiniSplineComponent )
        return;

    UHoudiniAssetComponent * HoudiniAssetComponent =
        Cast< UHoudiniAssetComponent >( EditedHoudiniSplineComponent->GetAttachParent() );

    if ( bRecordTransactionOnMove )
    {
        FScopedTransaction Transaction( TEXT(HOUDINI_MODULE_EDITOR),
            LOCTEXT("HoudiniSplineComponentChange", "Houdini Spline Component: Moving a point"),
            HoudiniAssetComponent );
        EditedHoudiniSplineComponent->Modify();

        // Do not record further transaction until the flag is reset
        bRecordTransactionOnMove = false;
    }

    // Update given control point.
    EditedHoudiniSplineComponent->UpdatePoint( PointIndex, Point );

    bComponentNeedUpdate = true;
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

    FTransform OtherPoint = FTransform::Identity;
    FTransform CurrentPoint = FTransform::Identity;
    int32 nCurrentCPIndex = -1;

    // We need to sort the selection to insert the new nodes properly
    EditedControlPointsIndexes.Sort();

    const TArray< FTransform > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

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
            if ( EditedHoudiniSplineComponent->bClosedCurve )
            {
                OtherPoint = CurvePoints[ 0 ];
            }
            else
            {
                    OtherPoint = CurvePoints[nCurrentCPIndex - 1];
                    nCurrentCPIndex--;
            }
        }

        FVector NewPointLocation = CurrentPoint.GetLocation() + (OtherPoint.GetLocation() - CurrentPoint.GetLocation()) / 2.0f;
        FVector NewPointScale = CurrentPoint.GetScale3D() + (OtherPoint.GetScale3D() - CurrentPoint.GetScale3D()) / 2.0f;
        FQuat NewPointRotation = FQuat::Slerp(CurrentPoint.GetRotation(), OtherPoint.GetRotation(), 0.5f);

        FTransform NewTransform = FTransform::Identity;
        NewTransform.SetLocation(NewPointLocation);
        NewTransform.SetScale3D(NewPointScale);
        NewTransform.SetRotation(NewPointRotation);

        int32 NewPointIndex = AddControlPointAfter(NewTransform, nCurrentCPIndex);

        tNewSelection.Add(NewPointIndex);
    }

    // Update the spline component
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
	HoudiniAssetComponent);
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

    // cache the rotation
    CacheRotation();

    // Update the spline object
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

int32 FHoudiniSplineComponentVisualizer::AddControlPointAfter( const FTransform & NewPoint, const int32& nIndex )
{
    if ( !EditedHoudiniSplineComponent )
        return nIndex;

    const TArray< FTransform > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;
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
    // Duplicate the selected points
    DuplicateControlPoint();

    // Update the spline component
    UpdateHoudiniComponents();
}

void
FHoudiniSplineComponentVisualizer::DuplicateControlPoint()
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

    const TArray< FTransform > & CurvePoints = EditedHoudiniSplineComponent->CurvePoints;

    //
    EditedControlPointsIndexes.Sort();

    int32 nCurrentCPIndex = -1;
    int32 nNewCPIndex = -1;
    TArray<int32> tNewSelection;

    FTransform NewPoint = FTransform::Identity;
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

        // Small hack when extending from the first point
        if (nCurrentCPIndex == 0)
            nNewCPIndex = 0;

        tNewSelection.Add(nNewCPIndex);
        nOffset++;
    }

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


void
FHoudiniSplineComponentVisualizer::CacheRotation()
{
    FQuat NewCachedQuat = FQuat::Identity;
    if (EditedHoudiniSplineComponent && EditedControlPointsIndexes.Num() == 1)
    {
	if (EditedHoudiniSplineComponent->CurvePoints.IsValidIndex(EditedControlPointsIndexes[0]))
	    NewCachedQuat = (EditedHoudiniSplineComponent->CurvePoints[EditedControlPointsIndexes[0]]).GetRotation();
    }

    CachedRotation = NewCachedQuat;
}

#undef LOCTEXT_NAMESPACE