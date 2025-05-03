/*
* Copyright (c) <2023> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Optional.h"

#include "HoudiniInputTypes.h"
#include "HoudiniInputObject.h"

#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"

class UHoudiniOutput;
class ULandscapeSplinesComponent;
class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
class ALandscape;

class UHoudiniLandscapeSplinesOutput;

struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeRuntimeUtils
{
    static void DeleteLandscapeCookedData(UHoudiniOutput* Output);

    static void DeleteEditLayer(ALandscape* Landscape, const FName& LayerName);

    static void DestroyLandscape(ALandscape* Landscape);

    static void DestroyLandscapeProxy(ALandscapeProxy* Proxy);

    // ------------------
    // Landscape splines
    // ------------------

	/**
	 * Destroy a landscape spline control point, optionally only if it 
	 * is unused. The interpolated spline points associated with the
	 * point is deleted and it is removed from segment connections. The
	 * connected segments' interpolated spline points are also removed.
	 *
	 * Removing the control point from its spline component can be deferred. This
	 * can be useful in bulk delete situations where we want to cleanup the
	 * interpolated data and segment connections first and then clear the 
	 * control point / segment arrays on the owning splines component in one go.
	 * 
	 * @param InControlPoint The control point to destroy.
	 * @param bInDestroyOnlyIfUnused Defaults to true. If this is true, then if
	 * the control point has one or more valid segment connections the point
	 * and its associated spline data is not destroyed, and the function
	 * returns false.
	 * @param bInRemoveDestroyedControlPointFromComponent Defaults to True.
	 * If false, then we do all the cleanup of segment connections and
	 * interpolated spline points, but InControlPoint is not removed
	 * from its splines component. It is expected to be removed by the caller
	 * during, for example, a bulk delete. The function returns true.

	 * @return true if the control point was destroyed (even if it was technically
	 * not removed from its owning splines component).
	 */
	static bool DestroyLandscapeSplinesControlPoint(
		ULandscapeSplineControlPoint* InControlPoint,
		bool bInDestroyOnlyIfUnused = true,
		bool bInRemoveDestroyedControlPointFromComponent = true);

	/**
	 * Destroy a landscape spline segment, and optionally all of its control
	 * points that are unused after removal from the segment. The interpolated
	 * spline points associated with the segment are deleted and it is removed
	 * from segment connections with control points. The
	 * connected control points' interpolated spline points are also removed.
	 *
	 * Removing the segment from its spline component can be deferred. This
	 * can be useful in bulk delete situations where we want to cleanup the
	 * interpolated data and segment connections first and then clear the
	 * control point / segment arrays on the owning splines component in one go.
	 *
	 * @param InSegment The segment to destroy.
	 * @param bInRemoveSegmentFromComponent Defaults to true. If false this
	 * function continues normally but does not remove the segment from its
	 * owning component. It is expected that the caller will remove the segment
	 * from the component, during a bulk delete for example.
	 * @param bInDestroyOnlyUnusedControlPoints Defaults to true. If this is
	 * true, then if a control point has no more valid segment connections
	 * left after removal from InSegment then it is also destroyed.
	 * @param bInRemoveDestroyedControlPointsFromComponent Defaults to True.
	 * If true, then any control points that are destroyed in this function
	 * as a result of bInDestroyUnusedControlPoints, will also be removed
	 * from their owning splines component. This could be set to false in
	 * bulk deletion scenarios where the caller will remove all segments/
	 * control points from the splines component.
	 * 
	 * @return true if the segment was destroyed (even if it was technically
	 * not removed from its owning splines component).
	 */
	static bool DestroyLandscapeSplinesSegment(
		ULandscapeSplineSegment* InSegment,
		bool bInRemoveSegmentFromComponent = true,
		bool bInDestroyUnusedControlPoints = true,
		bool bInRemoveDestroyedControlPointsFromComponent = true,
		TArray<ULandscapeSplineControlPoint*>* RemainingControlPoints = nullptr);

	static bool DestroyLandscapeSplinesSegmentsAndControlPoints(ULandscapeSplinesComponent* InSplinesComponent);

	static bool DestroyLandscapeSplinesSegmentsAndControlPoints(UHoudiniLandscapeSplinesOutput* InOutputObject);
	
	static void DeleteLandscapeSplineCookedData(UHoudiniOutput* InOutput);

    /**
     * Get or generate a control point id. The ids are stored in InControlPointIdMap and InNextControlPointId is the
     * next valid id we can use for a newly seen control point.
     * @param InControlPoint The control point to get the id for.
     * @param InControlPointIdMap The map to get the id from. If no valid id for control point in the map, generates one
     * via InNextControlPointId and add it to the map.
     * @param InNextControlPointId The next valid control point id. Updated if we needed to generate a new id for
     * InControlPoint.
     * @return The control point id. 
     */
     static int32 GetOrGenerateValidControlPointId(
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		TObjectPtr<ULandscapeSplineControlPoint>& InControlPoint,
#else
		ULandscapeSplineControlPoint const* const InControlPoint,
#endif
		TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap,
		int32& InNextControlPointId);
};
