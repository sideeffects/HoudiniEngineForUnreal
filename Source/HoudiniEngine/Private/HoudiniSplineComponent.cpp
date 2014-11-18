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

#include "HoudiniEnginePrivatePCH.h"


UHoudiniSplineComponent::UHoudiniSplineComponent(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	CurveType(EHoudiniSplineComponentType::Polygon),
	CurveMethod(EHoudiniSplineComponentMethod::Breakpoints),
	bClosedCurve(false)
{
	// By default we will create two points.
	CurvePoints.Add(FVector(0.0f, 0.0f, 0.0f));
	CurvePoints.Add(FVector(100.0f, 0.0f, 0.0f));
}


UHoudiniSplineComponent::~UHoudiniSplineComponent()
{

}


bool
UHoudiniSplineComponent::Construct(const TArray<FVector>& InCurvePoints, const TArray<FVector>& InCurveDisplayPoints,
								   EHoudiniSplineComponentType::Enum InCurveType, EHoudiniSplineComponentMethod::Enum InCurveMethod, bool bInClosedCurve)
{
	ResetCurvePoints();
	AddPoints(InCurvePoints);

	ResetCurveDisplayPoints();
	AddDisplayPoints(InCurveDisplayPoints);

	CurveType = InCurveType;
	CurveMethod = InCurveMethod;
	bClosedCurve = bInClosedCurve;

	// Perform other construction here.
	return true;
}


EHoudiniSplineComponentType::Enum
UHoudiniSplineComponent::GetCurveType() const
{
	return CurveType;
}


EHoudiniSplineComponentMethod::Enum
UHoudiniSplineComponent::GetCurveMethod() const
{
	return CurveMethod;
}


bool
UHoudiniSplineComponent::IsClosedCurve() const
{
	return bClosedCurve;
}


void
UHoudiniSplineComponent::ResetCurvePoints()
{
	CurvePoints.Empty();
}


void
UHoudiniSplineComponent::ResetCurveDisplayPoints()
{
	CurveDisplayPoints.Empty();
}


void
UHoudiniSplineComponent::AddPoint(const FVector& Point)
{
	CurvePoints.Add(Point);
}


void
UHoudiniSplineComponent::AddPoints(const TArray<FVector>& Points)
{
	CurvePoints.Append(Points);
}


void
UHoudiniSplineComponent::AddDisplayPoints(const TArray<FVector>& Points)
{
	CurveDisplayPoints.Append(Points);
}


bool
UHoudiniSplineComponent::IsValidCurve() const
{
	if(CurvePoints.Num() < 2)
	{
		return false;
	}

	return true;
}

