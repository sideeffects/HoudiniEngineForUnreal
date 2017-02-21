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
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

#include "HoudiniHandleComponent.h"
#include "ComponentVisualizer.h"
#include "Commands.h"

/** Base class for clickable editing proxies. **/
struct HHoudiniHandleVisProxy : public HComponentVisProxy
{
    DECLARE_HIT_PROXY();
    HHoudiniHandleVisProxy( const UActorComponent * InComponent );
};

/** Define commands for our component visualizer */
class FHoudiniHandleComponentVisualizerCommands : public TCommands< FHoudiniHandleComponentVisualizerCommands >
{
    public:

        /** Constructor. **/
        FHoudiniHandleComponentVisualizerCommands();

        /** Register commands. **/
        virtual void RegisterCommands() override;

    public:

        /** Command for adding a control point. **/
        TSharedPtr< FUICommandInfo > CommandAddControlPoint;

        /** Command for deleting a control point. **/
        TSharedPtr< FUICommandInfo > CommandDeleteControlPoint;
};


/** Our handle visualizer. **/
class FHoudiniHandleComponentVisualizer : public FComponentVisualizer
{
    public:

        FHoudiniHandleComponentVisualizer();
        virtual ~FHoudiniHandleComponentVisualizer();

        /** FComponentVisualizer methods. **/
    public:

        /** Draw visualization for the given component. **/
        virtual void DrawVisualization(
            const UActorComponent * Component, const FSceneView * View,
            FPrimitiveDrawInterface * PDI) override;

        /** Handle a click on a registered hit box. **/
        virtual bool VisProxyHandleClick(
            FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click ) override;

        /** Called when editing is no longer being performed. **/
        virtual void EndEditing() override;

        /** Returns location of a gizmo widget. **/
        virtual bool GetWidgetLocation(
            const FEditorViewportClient *, FVector & OutLocation) const override;

        virtual bool GetCustomInputCoordinateSystem(
            const FEditorViewportClient * ViewportClient, FMatrix & OutMatrix) const override;

        /** Handle input change. **/
        virtual bool HandleInputDelta(
            FEditorViewportClient *, FViewport *, FVector & DeltaTranslate,
            FRotator & DeltaRotate, FVector & DeltaScale) override;

        virtual bool HandleInputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;

    protected:
        /** Visualizer actions. **/
        TSharedPtr< FUICommandList > VisualizerActions;

        /** Houdini component which is being edited. **/
        UHoudiniHandleComponent * EditedComponent;

        /** Is set to true if we are editing. **/
        uint32 bEditing : 1;
        uint32 bAllowTranslate : 1;
        uint32 bAllowRotation : 1;
        uint32 bAllowScale : 1;
};
