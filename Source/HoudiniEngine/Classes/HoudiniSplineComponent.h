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
#include "HoudiniSplineComponent.generated.h"


namespace EHoudiniSplineComponentType
{
	enum Enum
	{
		Polygon,
		Nurbs,
		Bezier
	};
}


namespace EHoudiniSplineComponentMethod
{
	enum Enum
	{
		CVs,
		Breakpoints,
		Freehand
	};
}


UCLASS(config=Editor)
class HOUDINIENGINE_API UHoudiniSplineComponent : public USceneComponent
{
	friend class UHoudiniAssetComponent;
	friend class FHoudiniSplineComponentVisualizer;

	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniSplineComponent();

public:

	/** Construct spline from given information. Resets any existing state. **/
	bool Construct(const TArray<FVector>& InCurvePoints, const TArray<FVector>& InCurveDisplayPoints,
				   EHoudiniSplineComponentType::Enum InCurveType, EHoudiniSplineComponentMethod::Enum InCurveMethod, bool bInClosedCurve = false);

	/** Return the type of this curve. **/
	EHoudiniSplineComponentType::Enum GetCurveType() const;

	/** Return method used by this curve. **/
	EHoudiniSplineComponentMethod::Enum GetCurveMethod() const;

	/** Return true if this curve is closed. **/
	bool IsClosedCurve() const;

	/** Resets all points of this curve. **/
	void ResetCurvePoints();

	/** Reset display points of this curve. **/
	void ResetCurveDisplayPoints();

	/** Add a point to this curve. **/
	void AddPoint(const FVector& Point);

	/** Add points to this curve. **/
	void AddPoints(const TArray<FVector>& Points);

	/** Add display points to this curve. **/
	void AddDisplayPoints(const TArray<FVector>& Points);

	/** Return true if this spline is a valid spline. **/
	bool IsValidCurve() const;

protected:

	/** List of points composing this curve. **/
	TArray<FVector> CurvePoints;

	/** List of refined points used for drawing. **/
	TArray<FVector> CurveDisplayPoints;

	/** Type of this curve. **/
	EHoudiniSplineComponentType::Enum CurveType;

	/** Method used for this curve. **/
	EHoudiniSplineComponentMethod::Enum CurveMethod;

	/** Whether this spline is closed. **/
	bool bClosedCurve;
};

