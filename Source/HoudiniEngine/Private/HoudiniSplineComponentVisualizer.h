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

#pragma once

class FHoudiniSplineComponentVisualizer : public FComponentVisualizer
{
public:

	FHoudiniSplineComponentVisualizer();
	virtual ~FHoudiniSplineComponentVisualizer();

/** FComponentVisualizer methods. **/
public:

	/** Registration of this component visualizer. **/
	virtual void OnRegister() override;

	/** Draw visualization for the given component. **/
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
