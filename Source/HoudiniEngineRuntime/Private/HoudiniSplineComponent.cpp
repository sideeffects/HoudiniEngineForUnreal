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
#include "HoudiniPluginSerializationVersion.h"

UHoudiniSplineComponent::UHoudiniSplineComponent( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , HoudiniAssetInput( nullptr )
    , CurveType( EHoudiniSplineComponentType::Polygon )
    , CurveMethod( EHoudiniSplineComponentMethod::Breakpoints )
    , bClosedCurve( false )
{
    // By default we will create two points.
    FTransform transf = FTransform::Identity;
    CurvePoints.Add( transf );
    transf.SetTranslation( FVector(200.0f, 0.0f, 0.0f) );
    CurvePoints.Add( transf );
}

UHoudiniSplineComponent::~UHoudiniSplineComponent()
{}

void
UHoudiniSplineComponent::Serialize( FArchive & Ar )
{
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    int32 Version = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
    Ar << Version;

    Ar << HoudiniGeoPartObject;

    if (Version < VER_HOUDINI_PLUGIN_SERIALIZATION_HOUDINI_SPLINE_TO_TRANSFORM)
    {
        // Before, curve points where stored as Vectors, not Transforms
        TArray<FVector> OldCurvePoints;
        Ar << OldCurvePoints;

        CurvePoints.SetNumUninitialized(OldCurvePoints.Num());

        FTransform trans = FTransform::Identity;
        for (int32 n = 0; n < CurvePoints.Num(); n++)
        {
            trans.SetLocation(OldCurvePoints[n]);
            CurvePoints[n] = trans;
        }
    }
    else
    {
        Ar << CurvePoints;
    }

    Ar << CurveDisplayPoints;

    SerializeEnumeration< EHoudiniSplineComponentType::Enum >( Ar, CurveType );
    SerializeEnumeration< EHoudiniSplineComponentMethod::Enum >( Ar, CurveMethod );
    Ar << bClosedCurve;
}

#if WITH_EDITOR

void
UHoudiniSplineComponent::PostEditUndo()
{
    Super::PostEditUndo();

    UHoudiniAssetComponent * AttachComponent = Cast< UHoudiniAssetComponent >( GetAttachParent() );
    if ( AttachComponent )
    {
        UploadControlPoints();
        AttachComponent->StartTaskAssetCooking( true );
    }
}

#endif // WITH_EDITOR

bool
UHoudiniSplineComponent::Construct(
    const FHoudiniGeoPartObject & InHoudiniGeoPartObject,
    const TArray< FTransform > & InCurvePoints,
    const TArray< FVector > & InCurveDisplayPoints,
    EHoudiniSplineComponentType::Enum InCurveType,
    EHoudiniSplineComponentMethod::Enum InCurveMethod,
    bool bInClosedCurve )
{
    HoudiniGeoPartObject = InHoudiniGeoPartObject;

    ResetCurvePoints();
    AddPoints( InCurvePoints );

    ResetCurveDisplayPoints();
    AddDisplayPoints( InCurveDisplayPoints );

    CurveType = InCurveType;
    CurveMethod = InCurveMethod;
    bClosedCurve = bInClosedCurve;

    return true;
}


bool
UHoudiniSplineComponent::Construct(
    const FHoudiniGeoPartObject & InHoudiniGeoPartObject,
    const TArray< FVector > & InCurveDisplayPoints,
    EHoudiniSplineComponentType::Enum InCurveType,
    EHoudiniSplineComponentMethod::Enum InCurveMethod,
    bool bInClosedCurve)
{
    HoudiniGeoPartObject = InHoudiniGeoPartObject;

    ResetCurveDisplayPoints();
    AddDisplayPoints(InCurveDisplayPoints);

    CurveType = InCurveType;
    CurveMethod = InCurveMethod;
    bClosedCurve = bInClosedCurve;

    return true;
}


bool
UHoudiniSplineComponent::CopyFrom( UHoudiniSplineComponent* InSplineComponent )
{
    if (!InSplineComponent)
        return false;

    HoudiniGeoPartObject = FHoudiniGeoPartObject(InSplineComponent->HoudiniGeoPartObject);

    ResetCurvePoints();   
    AddPoints(InSplineComponent->GetCurvePoints());

    ResetCurveDisplayPoints();
    AddDisplayPoints(InSplineComponent->CurveDisplayPoints);

    CurveType = InSplineComponent->GetCurveType();
    CurveMethod = InSplineComponent->GetCurveMethod();
    bClosedCurve = InSplineComponent->IsClosedCurve();

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
UHoudiniSplineComponent::AddPoint( const FTransform & Point )
{
    CurvePoints.Add( Point );
}

void
UHoudiniSplineComponent::AddPoints( const TArray< FTransform > & Points )
{
    CurvePoints.Append( Points );
}

void
UHoudiniSplineComponent::AddDisplayPoints( const TArray< FVector > & Points )
{
    CurveDisplayPoints.Append( Points );
}

bool
UHoudiniSplineComponent::IsValidCurve() const
{
    if ( CurvePoints.Num() < 2 )
        return false;

    return true;
}

void
UHoudiniSplineComponent::UpdatePoint( int32 PointIndex, const FTransform & Point )
{
    check( PointIndex >= 0 && PointIndex < CurvePoints.Num() );
    CurvePoints[ PointIndex ] = Point;
}

void
UHoudiniSplineComponent::UploadControlPoints()
{    
    // Grab component we are attached to.
    HAPI_AssetId HostAssetId = -1;
    UHoudiniAssetComponent * AttachedComponent = Cast< UHoudiniAssetComponent >(GetAttachParent());
    if (AttachedComponent)
        HostAssetId = AttachedComponent->GetAssetId();

    HAPI_AssetId CurveAssetId = -1;
    HAPI_NodeId NodeId = -1;    
    if (IsInputCurve())
    {
        CurveAssetId = HoudiniAssetInput->GetConnectedAssetId();
        if (HoudiniGeoPartObject.IsValid())
            NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(CurveAssetId);
    }
    else
    {
        if (HoudiniGeoPartObject.IsValid() && AttachedComponent)
        {
            CurveAssetId = AttachedComponent->GetAssetId();
            NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(CurveAssetId);
        }
    }
   
    if ( ( NodeId < 0 ) || ( HostAssetId < 0 ) )
        return;

    // Extract positions rotations and scales and upload them to the curve node
    TArray<FVector> Positions;
    GetCurvePositions(Positions);

    if (IsInputCurve())
    {
        // Only input curves suppot rotation and scale
        TArray<FQuat> Rotations;
        GetCurveRotations(Rotations);

        TArray<FVector> Scales;
        GetCurveScales(Scales);

        FHoudiniEngineUtils::HapiCreateCurveAsset(
            HostAssetId,
            CurveAssetId,
            &Positions,
            &Rotations,
            &Scales,
            nullptr);
    }
    else
    {
        FString PositionString = TEXT("");
        FHoudiniEngineUtils::CreatePositionsString(Positions, PositionString);

        // Get param id.
        HAPI_ParmId ParmId = -1;
        if (FHoudiniApi::GetParmIdFromName(
            FHoudiniEngine::Get().GetSession(), NodeId,
            HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) != HAPI_RESULT_SUCCESS)
        {
            return;
        }

        std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
        if (FHoudiniApi::SetParmStringValue(
            FHoudiniEngine::Get().GetSession(), NodeId,
            ConvertedString.c_str(), ParmId, 0) != HAPI_RESULT_SUCCESS)
        {
            return;
        }
    }
}

void
UHoudiniSplineComponent::RemovePoint( int32 PointIndex )
{
    check( PointIndex >= 0 && PointIndex < CurvePoints.Num() );
    CurvePoints.RemoveAt( PointIndex );
}

void
UHoudiniSplineComponent::AddPoint( int32 PointIndex, const FTransform & Point )
{
    check( PointIndex >= 0 && PointIndex < CurvePoints.Num() );
    CurvePoints.Insert( Point, PointIndex );
}

bool
UHoudiniSplineComponent::IsInputCurve() const
{
    return HoudiniAssetInput != nullptr;
}

void
UHoudiniSplineComponent::SetHoudiniAssetInput( UHoudiniAssetInput * InHoudiniAssetInput )
{
    HoudiniAssetInput = InHoudiniAssetInput;
}

void
UHoudiniSplineComponent::NotifyHoudiniInputCurveChanged()
{
    if ( HoudiniAssetInput )
        HoudiniAssetInput->OnInputCurveChanged();
}

const TArray< FTransform > &
UHoudiniSplineComponent::GetCurvePoints() const
{
    return CurvePoints;
}


void
UHoudiniSplineComponent::GetCurvePositions(TArray<FVector>& Positions) const
{
    Positions.SetNumUninitialized(CurvePoints.Num());
    for (int32 n = 0; n < CurvePoints.Num(); n++)
    {
        Positions[n] = CurvePoints[n].GetLocation();
    }
}


void 
UHoudiniSplineComponent::GetCurveRotations(TArray<FQuat>& Rotations) const
{
    Rotations.SetNumUninitialized(CurvePoints.Num());
    for (int32 n = 0; n < CurvePoints.Num(); n++)
    {
        Rotations[n] = CurvePoints[n].GetRotation();
    }
}

void 
UHoudiniSplineComponent::GetCurveScales(TArray<FVector>& Scales) const
{
    Scales.SetNumUninitialized(CurvePoints.Num());
    for (int32 n = 0; n < CurvePoints.Num(); n++)
    {
        Scales[n] = CurvePoints[n].GetScale3D();
    }
}