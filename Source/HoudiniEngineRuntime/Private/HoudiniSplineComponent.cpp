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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetInput.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"


UHoudiniSplineComponent::UHoudiniSplineComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetInput(nullptr),
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


void
UHoudiniSplineComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	int32 Version = 0; // Placeholder until we need to use it.
	Ar << Version;

	Ar << HoudiniGeoPartObject;

	Ar << CurvePoints;
	Ar << CurveDisplayPoints;

	SerializeEnumeration<EHoudiniSplineComponentType::Enum>(Ar, CurveType);
	SerializeEnumeration<EHoudiniSplineComponentMethod::Enum>(Ar, CurveMethod);
	Ar << bClosedCurve;
}


#if WITH_EDITOR

void
UHoudiniSplineComponent::PostEditUndo()
{
	Super::PostEditUndo();

	UHoudiniAssetComponent* AttachComponent = Cast<UHoudiniAssetComponent>(AttachParent);
	if(AttachComponent)
	{
		UploadControlPoints();
		AttachComponent->StartTaskAssetCooking(true);
	}
}

#endif


bool
UHoudiniSplineComponent::Construct(const FHoudiniGeoPartObject& InHoudiniGeoPartObject,
	const TArray<FVector>& InCurvePoints, const TArray<FVector>& InCurveDisplayPoints,
	EHoudiniSplineComponentType::Enum InCurveType, EHoudiniSplineComponentMethod::Enum InCurveMethod,
	bool bInClosedCurve)
{
	HoudiniGeoPartObject = InHoudiniGeoPartObject;

	ResetCurvePoints();
	AddPoints(InCurvePoints);

	ResetCurveDisplayPoints();
	AddDisplayPoints(InCurveDisplayPoints);

	CurveType = InCurveType;
	CurveMethod = InCurveMethod;
	bClosedCurve = bInClosedCurve;

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


int32
UHoudiniSplineComponent::GetCurvePointCount() const
{
	return CurvePoints.Num();
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


void
UHoudiniSplineComponent::UpdatePoint(int32 PointIndex, const FVector& Point)
{
	check(PointIndex >= 0 && PointIndex < CurvePoints.Num());
	CurvePoints[PointIndex] = Point;
}


void
UHoudiniSplineComponent::UploadControlPoints()
{
	HAPI_NodeId NodeId = -1;

	if(IsInputCurve())
	{
		if(HoudiniGeoPartObject.IsValid())
		{
			NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(HoudiniAssetInput->GetConnectedAssetId());
		}
	}
	else
	{
		// Grab component we are attached to.
		UHoudiniAssetComponent* AttachComponent = Cast<UHoudiniAssetComponent>(AttachParent);
		if(HoudiniGeoPartObject.IsValid() && AttachComponent)
		{
			NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(AttachComponent->GetAssetId());
		}
	}

	if(NodeId >= 0)
	{
		FString PositionString = TEXT("");
		FHoudiniEngineUtils::CreatePositionsString(CurvePoints, PositionString);

		// Get param id.
		HAPI_ParmId ParmId = -1;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIdFromName(FHoudiniEngine::Get().GetSession(), NodeId,
			HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId))
		{
			return;
		}

		std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), NodeId,
			ConvertedString.c_str(), ParmId, 0))
		{
			return;
		}
	}
}


void
UHoudiniSplineComponent::RemovePoint(int32 PointIndex)
{
	check(PointIndex >= 0 && PointIndex < CurvePoints.Num());
	CurvePoints.RemoveAt(PointIndex);
}


void
UHoudiniSplineComponent::AddPoint(int32 PointIndex, const FVector& Point)
{
	check(PointIndex >= 0 && PointIndex < CurvePoints.Num());
	CurvePoints.Insert(Point, PointIndex);
}


bool
UHoudiniSplineComponent::IsInputCurve() const
{
	return HoudiniAssetInput != nullptr;
}


void
UHoudiniSplineComponent::SetHoudiniAssetInput(UHoudiniAssetInput* InHoudiniAssetInput)
{
	HoudiniAssetInput = InHoudiniAssetInput;
}


void
UHoudiniSplineComponent::NotifyHoudiniInputCurveChanged()
{
	if(HoudiniAssetInput)
	{
		HoudiniAssetInput->OnInputCurveChanged();
	}
}


const TArray<FVector>&
UHoudiniSplineComponent::GetCurvePoints() const
{
	return CurvePoints;
}
