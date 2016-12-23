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
#include "HoudiniEngineUtils.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniApi.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"
#include "Components/SplineComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "HoudiniInstancedActorComponent.h"

#include "AI/Navigation/NavCollision.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Engine/StaticMeshSocket.h"

const FString kResultStringSuccess( TEXT( "Success" ) );
const FString kResultStringFailure( TEXT( "Generic Failure" ) );
const FString kResultStringAlreadyInitialized( TEXT( "Already Initialized" ) );
const FString kResultStringNotInitialized( TEXT( "Not Initialized" ) );
const FString kResultStringCannotLoadFile( TEXT( "Unable to Load File" ) );
const FString kResultStringParmSetFailed( TEXT( "Failed Setting Parameter" ) );
const FString kResultStringInvalidArgument( TEXT( "Invalid Argument" ) );
const FString kResultStringCannotLoadGeo( TEXT( "Uneable to Load Geometry" ) );
const FString kResultStringCannotGeneratePreset( TEXT( "Uneable to Generate Preset" ) );
const FString kResultStringCannotLoadPreset( TEXT( "Uneable to Load Preset" ) );

const int32
FHoudiniEngineUtils::PackageGUIDComponentNameLength = 12;

const int32
FHoudiniEngineUtils::PackageGUIDItemNameLength = 8;

const int32
FHoudiniEngineUtils::MaterialExpressionNodeX = -400;

const int32
FHoudiniEngineUtils::MaterialExpressionNodeY = -150;

const int32
FHoudiniEngineUtils::MaterialExpressionNodeStepX = 220;

const int32
FHoudiniEngineUtils::MaterialExpressionNodeStepY = 220;

const FString
FHoudiniEngineUtils::GetErrorDescription( HAPI_Result Result )
{
    if ( Result == HAPI_RESULT_SUCCESS )
    {
        return kResultStringSuccess;
    }
    else
    {
        switch ( Result )
        {
            case HAPI_RESULT_FAILURE:
            {
                return kResultStringAlreadyInitialized;
            }

            case HAPI_RESULT_ALREADY_INITIALIZED:
            {
                return kResultStringAlreadyInitialized;
            }

            case HAPI_RESULT_NOT_INITIALIZED:
            {
                return kResultStringNotInitialized;
            }

            case HAPI_RESULT_CANT_LOADFILE:
            {
                return kResultStringCannotLoadFile;
            }

            case HAPI_RESULT_PARM_SET_FAILED:
            {
                return kResultStringParmSetFailed;
            }

            case HAPI_RESULT_INVALID_ARGUMENT:
            {
                return kResultStringInvalidArgument;
            }

            case HAPI_RESULT_CANT_LOAD_GEO:
            {
                return kResultStringCannotLoadGeo;
            }

            case HAPI_RESULT_CANT_GENERATE_PRESET:
            {
                return kResultStringCannotGeneratePreset;
            }

            case HAPI_RESULT_CANT_LOAD_PRESET:
            {
                return kResultStringCannotLoadPreset;
            }

            default:
            {
                return kResultStringFailure;
            }
        };
    }
}

const FString
FHoudiniEngineUtils::GetErrorDescription()
{
    return FHoudiniEngineUtils::GetStatusString( HAPI_STATUS_CALL_RESULT, HAPI_STATUSVERBOSITY_ERRORS );
}

const FString
FHoudiniEngineUtils::GetCookState()
{
    return FHoudiniEngineUtils::GetStatusString( HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS );
}

const FString
FHoudiniEngineUtils::GetCookResult()
{
    return FHoudiniEngineUtils::GetStatusString( HAPI_STATUS_COOK_RESULT, HAPI_STATUSVERBOSITY_MESSAGES );
}

bool
FHoudiniEngineUtils::IsInitialized()
{
    return ( FHoudiniApi::IsHAPIInitialized() &&
        FHoudiniApi::IsInitialized( FHoudiniEngine::Get().GetSession() ) == HAPI_RESULT_SUCCESS );
}

bool
FHoudiniEngineUtils::GetLicenseType( FString & LicenseType )
{
    LicenseType = TEXT( "" );
    HAPI_License LicenseTypeValue = HAPI_LICENSE_NONE;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetSessionEnvInt(
        FHoudiniEngine::Get().GetSession(), HAPI_SESSIONENVINT_LICENSE,
        (int32 *) &LicenseTypeValue ), false );

    switch ( LicenseTypeValue )
    {
        case HAPI_LICENSE_NONE:
        {
            LicenseType = TEXT( "No License Acquired" );
            break;
        }

        case HAPI_LICENSE_HOUDINI_ENGINE:
        {
            LicenseType = TEXT( "Houdini Engine" );
            break;
        }

        case HAPI_LICENSE_HOUDINI:
        {
            LicenseType = TEXT( "Houdini" );
            break;
        }

        case HAPI_LICENSE_HOUDINI_FX:
        {
            LicenseType = TEXT( "Houdini FX" );
            break;
        }

        case HAPI_LICENSE_HOUDINI_ENGINE_INDIE:
        {
            LicenseType = TEXT( "Houdini Engine Indie" );
            break;
        }

        case HAPI_LICENSE_HOUDINI_INDIE:
        {
            LicenseType = TEXT( "Houdini Indie" );
            break;
        }

        case HAPI_LICENSE_MAX:
        default:
        {
            return false;
        }
    }

    return true;
}

bool
FHoudiniEngineUtils::IsLicenseHoudiniEngineIndie()
{
    HAPI_License LicenseTypeValue = HAPI_LICENSE_NONE;

    if ( FHoudiniApi::GetSessionEnvInt( FHoudiniEngine::Get().GetSession(),
        HAPI_SESSIONENVINT_LICENSE, (int32 *) &LicenseTypeValue ) == HAPI_RESULT_SUCCESS )
    {
        return HAPI_LICENSE_HOUDINI_ENGINE_INDIE == LicenseTypeValue;
    }

    return false;
}

bool
FHoudiniEngineUtils::ComputeAssetPresetBufferLength( HAPI_AssetId AssetId, int32 & OutBufferLength )
{
    HAPI_AssetInfo AssetInfo;
    OutBufferLength = 0;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    int32 BufferLength = 0;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPresetBufLength(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
        HAPI_PRESETTYPE_BINARY, NULL, &BufferLength ), false );

    OutBufferLength = BufferLength;
    return true;
}

bool
FHoudiniEngineUtils::SetAssetPreset( HAPI_AssetId AssetId, const TArray< char > & PresetBuffer )
{
    if ( PresetBuffer.Num() > 0 )
    {
        HAPI_AssetInfo AssetInfo;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
            FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPreset(
            FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
            HAPI_PRESETTYPE_BINARY, NULL, &PresetBuffer[ 0 ],
            PresetBuffer.Num() ), false );

        return true;
    }

    return false;
}

bool
FHoudiniEngineUtils::GetAssetPreset( HAPI_AssetId AssetId, TArray< char > & PresetBuffer )
{
    PresetBuffer.Empty();

    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    int32 BufferLength = 0;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPresetBufLength(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
        HAPI_PRESETTYPE_BINARY, NULL, &BufferLength ), false );

    PresetBuffer.SetNumZeroed( BufferLength );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPreset(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
        &PresetBuffer[ 0 ], PresetBuffer.Num() ), false );

    return true;
}

bool
FHoudiniEngineUtils::IsHoudiniAssetValid( HAPI_AssetId AssetId )
{
    if ( AssetId < 0 )
        return false;

    HAPI_AssetInfo AssetInfo;
    int32 ValidationAnswer = 0;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::IsAssetValid(
        FHoudiniEngine::Get().GetSession(), AssetId,
        AssetInfo.validationId, &ValidationAnswer ), false );

    return ValidationAnswer != 0;
}

bool
FHoudiniEngineUtils::DestroyHoudiniAsset( HAPI_AssetId AssetId )
{
    return FHoudiniApi::DestroyAsset( FHoudiniEngine::Get().GetSession(), AssetId ) == HAPI_RESULT_SUCCESS;
}

void
FHoudiniEngineUtils::ConvertUnrealString( const FString & UnrealString, std::string & String )
{
    String = TCHAR_TO_UTF8( *UnrealString );
}

bool
FHoudiniEngineUtils::GetUniqueMaterialShopName( HAPI_AssetId AssetId, HAPI_MaterialId MaterialId, FString & Name )
{
    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    HAPI_MaterialInfo MaterialInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetMaterialInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, MaterialId,
        &MaterialInfo ), false );

    HAPI_NodeInfo AssetNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
        &AssetNodeInfo ), false );

    HAPI_NodeInfo MaterialNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId,
        &MaterialNodeInfo ), false );

    FString AssetNodeName = TEXT( "" );
    FString MaterialNodeName = TEXT( "" );

    {
        FHoudiniEngineString HoudiniEngineString( AssetNodeInfo.internalNodePathSH );
        if ( !HoudiniEngineString.ToFString( AssetNodeName ) )
            return false;
    }

    {
        FHoudiniEngineString HoudiniEngineString( MaterialNodeInfo.internalNodePathSH );
        if ( !HoudiniEngineString.ToFString( MaterialNodeName ) )
            return false;
    }

    if ( AssetNodeName.Len() > 0 && MaterialNodeName.Len() > 0 )
    {
        // Remove AssetNodeName part from MaterialNodeName. Extra position is for separator.
        Name = MaterialNodeName.Mid( AssetNodeName.Len() + 1 );
        return true;
    }

    return false;
}

void
FHoudiniEngineUtils::TranslateHapiTransform( const HAPI_Transform & HapiTransform, FTransform & UnrealTransform )
{
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float TransformScaleFactor = HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    if ( ImportAxis == HRSAI_Unreal )
    {
        FQuat ObjectRotation(
            HapiTransform.rotationQuaternion[ 0 ], HapiTransform.rotationQuaternion[ 1 ],
            HapiTransform.rotationQuaternion[ 2 ],  -HapiTransform.rotationQuaternion[ 3 ]);
        Swap( ObjectRotation.Y, ObjectRotation.Z );

        FVector ObjectTranslation( HapiTransform.position[ 0 ], HapiTransform.position[ 1 ], HapiTransform.position[ 2 ] );
        ObjectTranslation *= TransformScaleFactor;
        Swap( ObjectTranslation[ 2 ], ObjectTranslation[ 1 ] );

        FVector ObjectScale3D( HapiTransform.scale[ 0 ], HapiTransform.scale[ 1 ], HapiTransform.scale[ 2 ] );
        Swap( ObjectScale3D.Y, ObjectScale3D.Z );

        UnrealTransform.SetComponents( ObjectRotation, ObjectTranslation, ObjectScale3D );
    }
    else if ( ImportAxis == HRSAI_Houdini )
    {
        FQuat ObjectRotation(
            HapiTransform.rotationQuaternion[ 0 ], HapiTransform.rotationQuaternion[ 1 ],
            HapiTransform.rotationQuaternion[ 2 ], HapiTransform.rotationQuaternion[ 3 ] );

        FVector ObjectTranslation(
            HapiTransform.position[ 0 ], HapiTransform.position[ 1 ], HapiTransform.position[ 2 ] );
        ObjectTranslation *= TransformScaleFactor;

        FVector ObjectScale3D( HapiTransform.scale[ 0 ], HapiTransform.scale[ 1 ], HapiTransform.scale[ 2 ] );

        UnrealTransform.SetComponents( ObjectRotation, ObjectTranslation, ObjectScale3D );
    }
    else
    {
        // Not valid enum value.
        check( 0 );
    }
}

void
FHoudiniEngineUtils::TranslateHapiTransform( const HAPI_TransformEuler & HapiTransformEuler, FTransform & UnrealTransform )
{
    float HapiMatrix[ 16 ];
    FHoudiniApi::ConvertTransformEulerToMatrix( FHoudiniEngine::Get().GetSession(), &HapiTransformEuler, HapiMatrix );

    HAPI_Transform HapiTransformQuat;
    FHoudiniApi::ConvertMatrixToQuat( FHoudiniEngine::Get().GetSession(), HapiMatrix, HAPI_SRT, &HapiTransformQuat );

    FHoudiniEngineUtils::TranslateHapiTransform( HapiTransformQuat, UnrealTransform );
}

void
FHoudiniEngineUtils::TranslateUnrealTransform( const FTransform & UnrealTransform, HAPI_Transform & HapiTransform )
{
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float TransformScaleFactor = HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    FMemory::Memzero< HAPI_Transform >( HapiTransform );

    HapiTransform.rstOrder = HAPI_SRT;

    FQuat UnrealRotation = UnrealTransform.GetRotation();
    FVector UnrealTranslation = UnrealTransform.GetTranslation();
    FVector UnrealScale = UnrealTransform.GetScale3D();

    if ( ImportAxis == HRSAI_Unreal )
    {
        Swap( UnrealRotation.Y, UnrealRotation.Z );
        HapiTransform.rotationQuaternion[ 0 ] = -UnrealRotation.X;
        HapiTransform.rotationQuaternion[ 1 ] = -UnrealRotation.Y;
        HapiTransform.rotationQuaternion[ 2 ] = -UnrealRotation.Z;
        HapiTransform.rotationQuaternion[ 3 ] = UnrealRotation.W;

        UnrealTranslation /= TransformScaleFactor;
        Swap( UnrealTranslation.Y, UnrealTranslation.Z );
        HapiTransform.position[ 0 ] = UnrealTranslation.X;
        HapiTransform.position[ 1 ] = UnrealTranslation.Y;
        HapiTransform.position[ 2 ] = UnrealTranslation.Z;

        Swap( UnrealScale.Y, UnrealScale.Z );
        HapiTransform.scale[ 0 ] = UnrealScale.X;
        HapiTransform.scale[ 1 ] = UnrealScale.Y;
        HapiTransform.scale[ 2 ] = UnrealScale.Z;
    }
    else if ( ImportAxis == HRSAI_Houdini )
    {
        HapiTransform.rotationQuaternion[ 0 ] = UnrealRotation.X;
        HapiTransform.rotationQuaternion[ 1 ] = UnrealRotation.Y;
        HapiTransform.rotationQuaternion[ 2 ] = UnrealRotation.Z;
        HapiTransform.rotationQuaternion[ 3 ] = UnrealRotation.W;

        HapiTransform.position[ 0 ] = UnrealTranslation.X;
        HapiTransform.position[ 1 ] = UnrealTranslation.Y;
        HapiTransform.position[ 2 ] = UnrealTranslation.Z;

        HapiTransform.scale[ 0 ] = UnrealScale.X;
        HapiTransform.scale[ 1 ] = UnrealScale.Y;
        HapiTransform.scale[ 2 ] = UnrealScale.Z;
    }
    else
    {
        // Not valid enum value.
        check( 0 );
    }
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(
    const FTransform & UnrealTransform,
    HAPI_TransformEuler & HapiTransformEuler )
{
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float TransformScaleFactor = HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    FMemory::Memzero< HAPI_TransformEuler >( HapiTransformEuler );

    HapiTransformEuler.rstOrder = HAPI_SRT;
    HapiTransformEuler.rotationOrder = HAPI_XYZ;

    FQuat UnrealRotation = UnrealTransform.GetRotation();
    FVector UnrealTranslation = UnrealTransform.GetTranslation();
    UnrealTranslation /= TransformScaleFactor;

    FVector UnrealScale = UnrealTransform.GetScale3D();

    if ( ImportAxis == HRSAI_Unreal )
    {
        // switch quat to Y-up, LHR
        Swap( UnrealRotation.Y, UnrealRotation.Z );
        UnrealRotation.W = -UnrealRotation.W;
        const FRotator Rotator = UnrealRotation.Rotator();
        // negate roll and pitch since they are actually RHR
        HapiTransformEuler.rotationEuler[ 0 ] = -Rotator.Roll;
        HapiTransformEuler.rotationEuler[ 1 ] = -Rotator.Pitch;
        HapiTransformEuler.rotationEuler[ 2 ] = Rotator.Yaw;

        Swap( UnrealTranslation.Y, UnrealTranslation.Z );
        HapiTransformEuler.position[ 0 ] = UnrealTranslation.X;
        HapiTransformEuler.position[ 1 ] = UnrealTranslation.Y;
        HapiTransformEuler.position[ 2 ] = UnrealTranslation.Z;

        Swap( UnrealScale.Y, UnrealScale.Z );
        HapiTransformEuler.scale[ 0 ] = UnrealScale.X;
        HapiTransformEuler.scale[ 1 ] = UnrealScale.Y;
        HapiTransformEuler.scale[ 2 ] = UnrealScale.Z;
    }
    else if ( ImportAxis == HRSAI_Houdini )
    {
        const FRotator Rotator = UnrealRotation.Rotator();
        HapiTransformEuler.rotationEuler[ 0 ] = Rotator.Roll;
        HapiTransformEuler.rotationEuler[ 1 ] = Rotator.Yaw;
        HapiTransformEuler.rotationEuler[ 2 ] = Rotator.Pitch;

        HapiTransformEuler.position[ 0 ] = UnrealTranslation.X;
        HapiTransformEuler.position[ 1 ] = UnrealTranslation.Y;
        HapiTransformEuler.position[ 2 ] = UnrealTranslation.Z;

        HapiTransformEuler.scale[ 0 ] = UnrealScale.X;
        HapiTransformEuler.scale[ 1 ] = UnrealScale.Y;
        HapiTransformEuler.scale[ 2 ] = UnrealScale.Z;
    }
    else
    {
        // Not valid enum value.
        check( 0 );
    }
}

bool
FHoudiniEngineUtils::SetCurrentTime( float CurrentTime )
{
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetTime(
        FHoudiniEngine::Get().GetSession(), CurrentTime ), false );
    return true;
}

bool
FHoudiniEngineUtils::GetHoudiniAssetName( HAPI_AssetId AssetId, FString & NameString )
{
    HAPI_AssetInfo AssetInfo;

    if ( FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ) == HAPI_RESULT_SUCCESS )
    {
        FHoudiniEngineString HoudiniEngineString( AssetInfo.nameSH );
        return HoudiniEngineString.ToFString( NameString );
    }

    return false;
}

int32
FHoudiniEngineUtils::HapiGetGroupCountByType( HAPI_GroupType GroupType, HAPI_GeoInfo & GeoInfo )
{
    switch ( GroupType )
    {
        case HAPI_GROUPTYPE_POINT: return GeoInfo.pointGroupCount;
        case HAPI_GROUPTYPE_PRIM: return GeoInfo.primitiveGroupCount;
        default: break;
    }

    return 0;
}

int32
FHoudiniEngineUtils::HapiGetElementCountByGroupType( HAPI_GroupType GroupType, HAPI_PartInfo & PartInfo )
{
    switch ( GroupType )
    {
        case HAPI_GROUPTYPE_POINT: return PartInfo.pointCount;
        case HAPI_GROUPTYPE_PRIM: return PartInfo.faceCount;
        default: break;
    }

    return 0;
}

bool
FHoudiniEngineUtils::HapiGetGroupNames(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_GroupType GroupType, TArray< FString > & GroupNames )
{
    HAPI_GeoInfo GeoInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGeoInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, &GeoInfo ), false );

    int32 GroupCount = FHoudiniEngineUtils::HapiGetGroupCountByType( GroupType, GeoInfo );

    if ( GroupCount > 0 )
    {
        std::vector< int32 > GroupNameHandles( GroupCount, 0 );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupNames(
            FHoudiniEngine::Get().GetSession(), AssetId, ObjectId,
            GeoId, GroupType, &GroupNameHandles[ 0 ], GroupCount ), false );

        for ( int32 NameIdx = 0; NameIdx < GroupCount; ++NameIdx )
        {
            FString GroupName = TEXT( "" );
            FHoudiniEngineString HoudiniEngineString( GroupNameHandles[ NameIdx ] );
            HoudiniEngineString.ToFString( GroupName );
            GroupNames.Add( GroupName );
        }
    }

    return true;
}

bool
FHoudiniEngineUtils::HapiGetGroupMembership(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, HAPI_GroupType GroupType,
    const FString & GroupName, TArray< int32 > & GroupMembership )
{
    HAPI_PartInfo PartInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, &PartInfo ), false );

    int32 ElementCount = FHoudiniEngineUtils::HapiGetElementCountByGroupType( GroupType, PartInfo );
    std::string ConvertedGroupName = TCHAR_TO_UTF8( *GroupName );

    GroupMembership.SetNumUninitialized( ElementCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupMembership(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, GroupType,
        ConvertedGroupName.c_str(), &GroupMembership[ 0 ], 0, ElementCount ), false );

    return true;
}

bool
FHoudiniEngineUtils::HapiCheckGroupMembership(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, HAPI_GroupType GroupType, const FString & GroupName )
{
    return FHoudiniEngineUtils::HapiCheckGroupMembership(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId,
        HoudiniGeoPartObject.PartId, GroupType, GroupName );
}

bool
FHoudiniEngineUtils::HapiCheckGroupMembership(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
    HAPI_GroupType GroupType, const FString & GroupName )
{
    TArray< int32 > GroupMembership;
    if ( FHoudiniEngineUtils::HapiGetGroupMembership( AssetId, ObjectId, GeoId, PartId, GroupType, GroupName, GroupMembership ) )
    {
        int32 GroupSum = 0;
        for ( int32 Idx = 0; Idx < GroupMembership.Num(); ++Idx )
            GroupSum += GroupMembership[ Idx ];

        return GroupSum > 0;
    }

    return false;
}

void
FHoudiniEngineUtils::HapiRetrieveParameterNames(
    const TArray< HAPI_ParmInfo > & ParmInfos,
    TArray< std::string > & Names)
{
    static const std::string InvalidParameterName( "Invalid Parameter Name" );

    Names.Empty();

    for ( int32 ParmIdx = 0; ParmIdx < ParmInfos.Num(); ++ParmIdx )
    {
        const HAPI_ParmInfo& NodeParmInfo = ParmInfos[ ParmIdx ];
        HAPI_StringHandle NodeParmHandle = NodeParmInfo.nameSH;

        int32 NodeParmNameLength = 0;
        FHoudiniApi::GetStringBufLength( FHoudiniEngine::Get().GetSession(), NodeParmHandle, &NodeParmNameLength );

        if ( NodeParmNameLength )
        {
            std::vector< char > NodeParmName( NodeParmNameLength, '\0' );

            HAPI_Result Result = FHoudiniApi::GetString(
                FHoudiniEngine::Get().GetSession(), NodeParmHandle,
                &NodeParmName[ 0 ], NodeParmNameLength );
            if ( Result == HAPI_RESULT_SUCCESS )
                Names.Add( std::string( NodeParmName.begin(), NodeParmName.end() - 1 ) );
            else
                Names.Add( InvalidParameterName );
        }
        else
        {
            Names.Add( InvalidParameterName );
        }
    }
}

void
FHoudiniEngineUtils::HapiRetrieveParameterNames( const TArray< HAPI_ParmInfo > & ParmInfos, TArray< FString > & Names )
{
    TArray< std::string > IntermediateNames;
    FHoudiniEngineUtils::HapiRetrieveParameterNames( ParmInfos, IntermediateNames );

    for ( int32 Idx = 0, Num = IntermediateNames.Num(); Idx < Num; ++Idx )
        Names.Add( UTF8_TO_TCHAR( IntermediateNames[ Idx ].c_str() ) );
}

bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeOwner Owner )
{
    HAPI_AttributeInfo AttribInfo;
    if ( FHoudiniApi::GetAttributeInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId,
        GeoId, PartId, Name, Owner, &AttribInfo ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return AttribInfo.exists;
}

bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name,
    HAPI_AttributeOwner Owner )
{
    return FHoudiniEngineUtils::HapiCheckAttributeExists(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name, Owner );
}

int32
FHoudiniEngineUtils::HapiFindParameterByName( const std::string & ParmName, const TArray< std::string > & Names )
{
    for ( int32 Idx = 0; Idx < Names.Num(); ++Idx )
        if ( !ParmName.compare( 0, ParmName.length(), Names[ Idx ] ) )
            return Idx;

    return -1;
}

int32
FHoudiniEngineUtils::HapiFindParameterByName( const FString & ParmName, const TArray< FString > & Names )
{
    for ( int32 Idx = 0, Num = Names.Num(); Idx < Num; ++Idx )
        if ( Names[ Idx ].Equals( ParmName ) )
            return Idx;

    return -1;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo,
    TArray< float > & Data, int32 TupleSize )
{
    ResultAttributeInfo.exists = false;

    // Reset container size.
    Data.SetNumUninitialized( 0 );

    int32 OriginalTupleSize = TupleSize;
    HAPI_AttributeInfo AttributeInfo;
    for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name,
            (HAPI_AttributeOwner) AttrIdx, &AttributeInfo ), false );

        if ( AttributeInfo.exists )
            break;
    }

    if ( !AttributeInfo.exists )
        return false;

    if ( OriginalTupleSize > 0 )
        AttributeInfo.tupleSize = OriginalTupleSize;

    // Allocate sufficient buffer for data.
    Data.SetNumUninitialized( AttributeInfo.count * AttributeInfo.tupleSize );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name,
        &AttributeInfo, &Data[ 0 ], 0, AttributeInfo.count ), false );

    // Store the retrieved attribute information.
    ResultAttributeInfo = AttributeInfo;
    return true;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name,
    HAPI_AttributeInfo & ResultAttributeInfo, TArray< float > & Data, int32 TupleSize )
{
    return FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
        ResultAttributeInfo, Data, TupleSize );
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo,
    TArray< int32 > & Data, int32 TupleSize )
{
    ResultAttributeInfo.exists = false;

    // Reset container size.
    Data.SetNumUninitialized( 0 );

    int32 OriginalTupleSize = TupleSize;
    HAPI_AttributeInfo AttributeInfo;
    for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(), AssetId, ObjectId,
            GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo ), false );

        if ( AttributeInfo.exists )
            break;
    }

    if ( !AttributeInfo.exists )
        return false;

    if ( OriginalTupleSize > 0 )
        AttributeInfo.tupleSize = OriginalTupleSize;

    // Allocate sufficient buffer for data.
    Data.SetNumUninitialized( AttributeInfo.count * AttributeInfo.tupleSize );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeIntData(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId,
        GeoId, PartId, Name, &AttributeInfo, &Data[ 0 ], 0, AttributeInfo.count ), false );

    // Store the retrieved attribute information.
    ResultAttributeInfo = AttributeInfo;
    return true;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    const char * Name, HAPI_AttributeInfo & ResultAttributeInfo,
    TArray< int32 > & Data, int32 TupleSize )
{
    return FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
        ResultAttributeInfo, Data, TupleSize );
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo,
    TArray< FString > & Data, int32 TupleSize )
{
    ResultAttributeInfo.exists = false;

    // Reset container size.
    Data.Empty();

    int32 OriginalTupleSize = TupleSize;
    HAPI_AttributeInfo AttributeInfo;
    for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(), AssetId, ObjectId,
            GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo ), false );

        if ( AttributeInfo.exists )
            break;
    }

    if ( !AttributeInfo.exists )
        return false;

    if ( OriginalTupleSize > 0 )
        AttributeInfo.tupleSize = OriginalTupleSize;

    TArray< HAPI_StringHandle > StringHandles;
    StringHandles.Init( -1, AttributeInfo.count * AttributeInfo.tupleSize );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeStringData(
        FHoudiniEngine::Get().GetSession(), AssetId,
        ObjectId, GeoId, PartId, Name, &AttributeInfo,
        &StringHandles[ 0 ], 0, AttributeInfo.count ), false );

    for ( int32 Idx = 0; Idx < StringHandles.Num(); ++Idx )
    {
        FString HapiString = TEXT( "" );
        FHoudiniEngineString HoudiniEngineString( StringHandles[ Idx ] );
        HoudiniEngineString.ToFString( HapiString );
        Data.Add( HapiString );
    }

    // Store the retrieved attribute information.
    ResultAttributeInfo = AttributeInfo;
    return true;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name,
    HAPI_AttributeInfo & ResultAttributeInfo, TArray< FString > & Data, int32 TupleSize)
{
    return FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        HoudiniGeoPartObject.AssetId,
        HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId,
        HoudiniGeoPartObject.PartId, Name,
        ResultAttributeInfo, Data, TupleSize );
}

bool
FHoudiniEngineUtils::HapiGetInstanceTransforms(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, TArray< FTransform > & Transforms )
{
    Transforms.Empty();

    // Number of instances is number of points.
    HAPI_PartInfo PartInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId,
        PartId, &PartInfo ), false );

    if ( PartInfo.pointCount == 0 )
        return false;

    TArray< HAPI_Transform > InstanceTransforms;
    InstanceTransforms.SetNumZeroed( PartInfo.pointCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstanceTransforms(
        FHoudiniEngine::Get().GetSession(), AssetId,
        ObjectId, GeoId, HAPI_SRT, &InstanceTransforms[ 0 ],
        0, PartInfo.pointCount ), false );

    for ( int32 Idx = 0; Idx < PartInfo.pointCount; ++Idx )
    {
        const HAPI_Transform& HapiInstanceTransform = InstanceTransforms[ Idx ];
        FTransform TransformMatrix;
        FHoudiniEngineUtils::TranslateHapiTransform( HapiInstanceTransform, TransformMatrix );
        Transforms.Add( TransformMatrix );
    }

    return true;
}

bool
FHoudiniEngineUtils::HapiGetInstanceTransforms(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    TArray< FTransform > & Transforms )
{
    return FHoudiniEngineUtils::HapiGetInstanceTransforms(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Transforms );
}

bool
FHoudiniEngineUtils::HapiGetImagePlanes(
    HAPI_ParmId NodeParmId, const HAPI_MaterialInfo & MaterialInfo,
    TArray< FString > & ImagePlanes )
{
    ImagePlanes.Empty();
    int32 ImagePlaneCount = 0;

    if ( FHoudiniApi::RenderTextureToImage(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.assetId, MaterialInfo.id, NodeParmId ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( FHoudiniApi::GetImagePlaneCount(
        FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
        MaterialInfo.id, &ImagePlaneCount ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( !ImagePlaneCount )
        return true;

    TArray< HAPI_StringHandle > ImagePlaneStringHandles;
    ImagePlaneStringHandles.SetNumUninitialized( ImagePlaneCount );

    if ( FHoudiniApi::GetImagePlanes(
        FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
        MaterialInfo.id, &ImagePlaneStringHandles[ 0 ], ImagePlaneCount ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    for ( int32 IdxPlane = 0, IdxPlaneMax = ImagePlaneStringHandles.Num(); IdxPlane < IdxPlaneMax; ++IdxPlane )
    {
        FString ValueString = TEXT( "" );
        FHoudiniEngineString FHoudiniEngineString( ImagePlaneStringHandles[ IdxPlane ] );
        FHoudiniEngineString.ToFString( ValueString );
        ImagePlanes.Add( ValueString );
    }

    return true;
}

bool
FHoudiniEngineUtils::HapiExtractImage(
    HAPI_ParmId NodeParmId, const HAPI_MaterialInfo & MaterialInfo,
    TArray< char > & ImageBuffer, const char * PlaneType, HAPI_ImageDataFormat ImageDataFormat,
    HAPI_ImagePacking ImagePacking, bool bRenderToImage )
{
    if ( bRenderToImage )
    {
        if ( FHoudiniApi::RenderTextureToImage(
            FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
            MaterialInfo.id, NodeParmId ) != HAPI_RESULT_SUCCESS )
        {
            return false;
        }
    }

    HAPI_ImageInfo ImageInfo;

    if ( FHoudiniApi::GetImageInfo(
        FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
        MaterialInfo.id, &ImageInfo ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    ImageInfo.dataFormat = ImageDataFormat;
    ImageInfo.interleaved = true;
    ImageInfo.packing = ImagePacking;

    if ( FHoudiniApi::SetImageInfo(
        FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
        MaterialInfo.id, &ImageInfo) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    int32 ImageBufferSize = 0;

    if ( FHoudiniApi::ExtractImageToMemory(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.assetId, MaterialInfo.id, HAPI_RAW_FORMAT_NAME,
        PlaneType, &ImageBufferSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( !ImageBufferSize )
        return false;

    ImageBuffer.SetNumUninitialized( ImageBufferSize );

    if ( FHoudiniApi::GetImageMemoryBuffer(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.assetId, MaterialInfo.id, &ImageBuffer[ 0 ],
        ImageBufferSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return true;
}

FColor
FHoudiniEngineUtils::PickVertexColorFromTextureMip(
    const uint8 * MipBytes, FVector2D & UVCoord, int32 MipWidth, int32 MipHeight )
{
    check( MipBytes );

    FColor ResultColor( 0, 0, 0, 255 );

    if ( UVCoord.X >= 0.0f && UVCoord.X < 1.0f && UVCoord.Y >= 0.0f && UVCoord.Y < 1.0f )
    {
        const int32 X = MipWidth * UVCoord.X;
        const int32 Y = MipHeight * UVCoord.Y;

        const int32 Index = ( ( Y * MipWidth ) + X ) * 4;

        ResultColor.B = MipBytes[ Index + 0 ];
        ResultColor.G = MipBytes[ Index + 1 ];
        ResultColor.R = MipBytes[ Index + 2 ];
        ResultColor.A = MipBytes[ Index + 3 ];
    }

    return ResultColor;
}

#if WITH_EDITOR

UTexture2D *
FHoudiniEngineUtils::CreateUnrealTexture(
    UTexture2D * ExistingTexture, const HAPI_ImageInfo & ImageInfo,
    UPackage * Package, const FString & TextureName,
    const TArray< char > & ImageBuffer, const FString & TextureType,
    const FCreateTexture2DParameters & TextureParameters, TextureGroup LODGroup )
{
    UTexture2D * Texture = nullptr;
    if ( ExistingTexture )
    {
        Texture = ExistingTexture;
    }
    else
    {
        // Create new texture object.
        Texture = NewObject< UTexture2D >(
            Package, UTexture2D::StaticClass(), *TextureName,
            RF_Transactional );

        // Add meta information to package.
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
            Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
            Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName );
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
            Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType );

        // Assign texture group.
        Texture->LODGroup = LODGroup;
    }

    // Initialize texture source.
    Texture->Source.Init( ImageInfo.xRes, ImageInfo.yRes, 1, 1, TSF_BGRA8 );

    // Lock the texture.
    uint8 * MipData = Texture->Source.LockMip( 0 );

    // Create base map.
    uint8* DestPtr = nullptr;
    uint32 SrcWidth = ImageInfo.xRes;
    uint32 SrcHeight = ImageInfo.yRes;
    const char * SrcData = &ImageBuffer[ 0 ];

    for ( uint32 y = 0; y < SrcHeight; y++ )
    {
        DestPtr = &MipData[ ( SrcHeight - 1 - y ) * SrcWidth * sizeof( FColor ) ];

        for ( uint32 x = 0; x < SrcWidth; x++ )
        {
            uint32 DataOffset = y * SrcWidth * 4 + x * 4;

            *DestPtr++ = *(uint8*)( SrcData + DataOffset + 2 ); // B
            *DestPtr++ = *(uint8*)( SrcData + DataOffset + 1 ); //  G
            *DestPtr++ = *(uint8*)( SrcData + DataOffset + 0 ); //R

            if ( TextureParameters.bUseAlpha )
                *DestPtr++ = *(uint8*)( SrcData + DataOffset + 3 ); // A
            else
                *DestPtr++ = 0xFF;
        }
    }

    // Unlock the texture.
    Texture->Source.UnlockMip( 0 );

    // Texture creation parameters.
    Texture->SRGB = TextureParameters.bSRGB;
    Texture->CompressionSettings = TextureParameters.CompressionSettings;
    Texture->CompressionNoAlpha = !TextureParameters.bUseAlpha;
    Texture->DeferCompression = TextureParameters.bDeferCompression;

    // Set the Source Guid/Hash if specified.
    /*
    if ( TextureParameters.SourceGuidHash.IsValid() )
    {
        Texture->Source.SetId( TextureParameters.SourceGuidHash, true );
    }
    */

    Texture->PostEditChange();

    return Texture;
}

void
FHoudiniEngineUtils::ResetRawMesh( FRawMesh & RawMesh )
{
    // Unlike Empty this will not change memory allocations.

    RawMesh.FaceMaterialIndices.Reset();
    RawMesh.FaceSmoothingMasks.Reset();
    RawMesh.VertexPositions.Reset();
    RawMesh.WedgeIndices.Reset();
    RawMesh.WedgeTangentX.Reset();
    RawMesh.WedgeTangentY.Reset();
    RawMesh.WedgeTangentZ.Reset();
    RawMesh.WedgeColors.Reset();

    for ( int32 Idx = 0; Idx < MAX_MESH_TEXTURE_COORDS; ++Idx )
        RawMesh.WedgeTexCoords[ Idx ].Reset();
}

#endif

bool
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(
    HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue, float & OutValue )
{
    float Value = DefaultValue;
    bool bComputed = false;

    HAPI_NodeInfo NodeInfo;
    FHoudiniApi::GetNodeInfo( FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo );

    TArray< HAPI_ParmInfo > NodeParams;
    NodeParams.SetNumUninitialized( NodeInfo.parmCount );
    FHoudiniApi::GetParameters(
        FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[ 0 ], 0, NodeInfo.parmCount );

    // Get names of parameters.
    TArray< std::string > NodeParamNames;
    FHoudiniEngineUtils::HapiRetrieveParameterNames( NodeParams, NodeParamNames );

    // See if parameter is present.
    int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName( ParmName, NodeParamNames );

    if ( ParmNameIdx != -1 )
    {
        HAPI_ParmInfo& ParmInfo = NodeParams[ ParmNameIdx ];
        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeId, &Value,
            ParmInfo.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            bComputed = true;
        }
    }

    OutValue = Value;
    return bComputed;
}

bool
FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
    HAPI_NodeId NodeId, const std::string ParmName, int32 DefaultValue, int32 & OutValue)
{
    int32 Value = DefaultValue;
    bool bComputed = false;

    HAPI_NodeInfo NodeInfo;
    FHoudiniApi::GetNodeInfo( FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo );
    
    TArray<HAPI_ParmInfo> NodeParams;
    NodeParams.SetNumUninitialized( NodeInfo.parmCount );
    FHoudiniApi::GetParameters(
        FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[ 0 ], 0, NodeInfo.parmCount );

    // Get names of parameters.
    TArray< std::string > NodeParamNames;
    FHoudiniEngineUtils::HapiRetrieveParameterNames( NodeParams, NodeParamNames );

    // See if parameter is present.
    int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName( ParmName, NodeParamNames );

    if ( ParmNameIdx != -1 )
    {
        HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNameIdx ];
        if ( FHoudiniApi::GetParmIntValues(
            FHoudiniEngine::Get().GetSession(), NodeId, &Value,
            ParmInfo.intValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            bComputed = true;
        }
    }

    OutValue = Value;
    return bComputed;
}

bool
FHoudiniEngineUtils::HapiGetParameterDataAsString(
    HAPI_NodeId NodeId, const std::string ParmName,
    const FString & DefaultValue, FString & OutValue )
{
    FString Value;
    bool bComputed = false;

    HAPI_NodeInfo NodeInfo;
    FHoudiniApi::GetNodeInfo( FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo );

    TArray< HAPI_ParmInfo > NodeParams;
    NodeParams.SetNumUninitialized( NodeInfo.parmCount );
    FHoudiniApi::GetParameters(
        FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[ 0 ], 0, NodeInfo.parmCount );

    // Get names of parameters.
    TArray< std::string > NodeParamNames;
    FHoudiniEngineUtils::HapiRetrieveParameterNames( NodeParams, NodeParamNames );

    // See if parameter is present.
    int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName( ParmName, NodeParamNames );

    if ( ParmNameIdx != -1 )
    {
        HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNameIdx ];
        HAPI_StringHandle StringHandle;

        if ( FHoudiniApi::GetParmStringValues(
            FHoudiniEngine::Get().GetSession(), NodeId, false,
            &StringHandle, ParmInfo.stringValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            FHoudiniEngineString HoudiniEngineString( StringHandle );
            if ( HoudiniEngineString.ToFString( Value ) )
                bComputed = true;
        }
    }

    if ( bComputed )
        OutValue = Value;
    else
        OutValue = DefaultValue;

    return bComputed;
}

bool
FHoudiniEngineUtils::HapiIsMaterialTransparent( const HAPI_MaterialInfo & MaterialInfo )
{
    float Alpha;
    FHoudiniEngineUtils::HapiGetParameterDataAsFloat( MaterialInfo.nodeId, HAPI_UNREAL_PARAM_ALPHA, 1.0f, Alpha );

    return Alpha < HAPI_UNREAL_ALPHA_THRESHOLD;
}

bool
FHoudiniEngineUtils::IsValidAssetId( HAPI_AssetId AssetId )
{
    return AssetId != -1;
}

bool
FHoudiniEngineUtils::HapiCreateCurve( HAPI_AssetId & CurveAssetId )
{
#if WITH_EDITOR

    HAPI_AssetId AssetId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateCurve(
        FHoudiniEngine::Get().GetSession(), &AssetId ), false );

    CurveAssetId = AssetId;

    HAPI_NodeId NodeId = -1;
    if ( !FHoudiniEngineUtils::HapiGetNodeId( AssetId, 0, 0, NodeId ) )
        return false;

    // Submit default points to curve.
    HAPI_ParmId ParmId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
        FHoudiniEngine::Get().GetSession(), NodeId,
        HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId ), false );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmStringValue(
        FHoudiniEngine::Get().GetSession(), NodeId,
        HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT, ParmId, 0 ), false );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookAsset(
        FHoudiniEngine::Get().GetSession(), AssetId, nullptr ), false );

#endif // WITH_EDITOR

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateCurveAsset(
    HAPI_AssetId HostAssetId,
    HAPI_AssetId& CurveAssetId,
    TArray<FVector>* Positions,
    TArray<FQuat>* Rotations /*= nullptr*/,
    TArray<FVector>* Scales3d /*= nullptr*/,
    TArray<float>* UniformScales /*= nullptr*/)
{
#if WITH_EDITOR

    // Positions are required
    if (!Positions)
        return false;

    // We also need a valid host asset and 2 points to make a curve
    int32 NumberOfCVs = Positions->Num();
    if ((NumberOfCVs < 2) || !FHoudiniEngineUtils::IsHoudiniAssetValid(HostAssetId))
        return false;

    // Check if connected asset id is valid, if it is not, we need to create an input asset.
    if (CurveAssetId < 0)
    {
        if (!HapiCreateCurve(CurveAssetId))
            CurveAssetId = -1;
    }
    else
    {
        // We have to revert the Geo to its original state so we can use the Curve SOP:
        // adding parameters to the Curve SOP locked it, preventing its parameters (type, method, isClosed) from working
        FHoudiniApi::RevertGeo(FHoudiniEngine::Get().GetSession(), CurveAssetId, 0, 0);
    }

    HAPI_NodeId NodeId = -1;
    if (!FHoudiniEngineUtils::HapiGetNodeId(CurveAssetId, 0, 0, NodeId))
        return false;

    //
    // In order to be able to add rotations and scale attributes to the curve SOP, we need to cook it twice:
    // 
    // - First, we send the positions string to it, and cook it without refinement.
    //   this will allow us to get the proper curve CVs, part attributes and curve info to create the desired curve.
    //
    // - We then need to send back all the info extracted from the curve SOP to it, and add the rotation 
    //   and scale attributes to it. This will lock the curve SOP, and prevent the curve type and method 
    //   parameters from functioning properly (hence why we needed the first cook to set that up)
    //

    // Reading the curve parameters
    int32 CurveTypeValue, CurveMethodValue, CurveClosed;
    FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
        NodeId, HAPI_UNREAL_PARAM_CURVE_TYPE, 0, CurveTypeValue);
    FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
        NodeId, HAPI_UNREAL_PARAM_CURVE_METHOD, 0, CurveMethodValue);
    FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
        NodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed);

    // For closed NURBS (CVs and Breakpoints), we have to close the curve manually, by duplicating its last point
    // in order to be able to set the rotations and scales attributes properly.
    bool bCloseCurveManually = false;
    if ( CurveClosed && (CurveTypeValue == HAPI_CURVETYPE_NURBS) && (CurveMethodValue != 2) )
    {
        // The curve is not closed anymore
        if (HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
            FHoudiniEngine::Get().GetSession(), NodeId,
            HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, 0))
        {
            bCloseCurveManually = true;

            // Duplicating the first point to the end of the curve
            // This needs to be done before sending the position string
            FVector pos = (*Positions)[0];
            Positions->Add(pos);

            CurveClosed = false;
        }
    }

    // Creating the position string
    FString PositionString = TEXT("");
    FHoudiniEngineUtils::CreatePositionsString(*Positions, PositionString);

    // Get param id for the PositionString and modify it
    HAPI_ParmId ParmId = -1;
    if (FHoudiniApi::GetParmIdFromName(
        FHoudiniEngine::Get().GetSession(), NodeId,
        HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) != HAPI_RESULT_SUCCESS)
    {
        return false;
    }

    std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
        FHoudiniEngine::Get().GetSession(), NodeId,
        ConvertedString.c_str(), ParmId, 0), false);
    
    // If we don't want to add rotations or scale attributes to the curve, 
    // we can just cook the node normally and stop here.
    bool bAddRotations = (Rotations != nullptr);
    bool bAddScales3d = (Scales3d != nullptr);
    bool bAddUniformScales = (UniformScales != nullptr);

    if (!bAddRotations && !bAddScales3d && !bAddUniformScales)
    {
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(
            FHoudiniEngine::Get().GetSession(), CurveAssetId, nullptr), false);

        return true;
    }

    // Setting up the first cook, without the curve refinement
    HAPI_CookOptions CookOptions;
    FMemory::Memzero< HAPI_CookOptions >(CookOptions);
    CookOptions.curveRefineLOD = 8.0f;
    CookOptions.clearErrorsAndWarnings = false;
    CookOptions.maxVerticesPerPrimitive = -1;
    CookOptions.splitGeosByGroup = false;
    CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;
    CookOptions.refineCurveToLinear = false;

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(
        FHoudiniEngine::Get().GetSession(), CurveAssetId, &CookOptions), false);

    //  We can now read back the Part infos from the cooked curve.
    HAPI_PartInfo PartInfos;
    FMemory::Memzero< HAPI_PartInfo >(PartInfos);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), 
        CurveAssetId, 0, 0, 0,
        &PartInfos), false);

    //
    // Depending on the curve type and method, additionnal control points might have been created.
    // We now have to interpolate the rotations and scale attributes for these.
    //

    // Lambda function that interpolates rotation, scale and uniform scales values
    // between two points using fCoeff as a weight, and inserts the interpolated value at nInsertIndex
    auto InterpolateRotScaleUScale = [&](const int32& nIndex1, const int32& nIndex2, const float& fCoeff, const int32& nInsertIndex)
    {
        if ( Rotations && Rotations->IsValidIndex(nIndex1) && Rotations->IsValidIndex(nIndex2) )
        {           
            FQuat interpolation = FQuat::Slerp((*Rotations)[nIndex1], (*Rotations)[nIndex2], fCoeff);
            if (Rotations->IsValidIndex( nInsertIndex ))
                Rotations->Insert(interpolation, nInsertIndex);
            else
                Rotations->Add(interpolation);
        }

        if ( Scales3d && Scales3d->IsValidIndex(nIndex1) && Scales3d->IsValidIndex(nIndex2) )
        {
            FVector interpolation = fCoeff * (*Scales3d)[nIndex1] + (1.0f - fCoeff) * (*Scales3d)[nIndex2];
            if (Scales3d->IsValidIndex(nInsertIndex))
                Scales3d->Insert(interpolation, nInsertIndex);
            else
                Scales3d->Add(interpolation);
        }


        if ( UniformScales  && UniformScales->IsValidIndex(nIndex1) && UniformScales->IsValidIndex(nIndex2) )
        { 
            float interpolation = fCoeff * (*UniformScales)[nIndex1] + (1.0f - fCoeff) * (*UniformScales)[nIndex2];
            if (UniformScales->IsValidIndex(nInsertIndex))
                UniformScales->Insert(interpolation, nInsertIndex);
            else
                UniformScales->Add(interpolation);
        }
    };

    // Lambda function that duplicates rotation, scale and uniform scales values
    // at nIndex and insert/adds it at nInsertIndex
    auto DuplicateRotScaleUScale = [&](const int32& nIndex, const int32& nInsertIndex)
    {
        if ( Rotations && Rotations->IsValidIndex(nIndex) )
        {
            FQuat value = (*Rotations)[nIndex];
            if ( Rotations->IsValidIndex(nInsertIndex) )
                Rotations->Insert(value, nInsertIndex);
            else
                Rotations->Add(value);
        }

        if ( Scales3d && Scales3d->IsValidIndex(nIndex) )
        {
            FVector value = (*Scales3d)[nIndex];
            if ( Scales3d->IsValidIndex(nInsertIndex) )
                Scales3d->Insert(value, nInsertIndex);
            else
                Scales3d->Add(value);
        }

        if ( UniformScales  && UniformScales->IsValidIndex(nIndex) )
        {
            float value = (*UniformScales)[nIndex];
            if ( UniformScales->IsValidIndex(nInsertIndex) )
                UniformScales->Insert(value, nInsertIndex);
            else
                UniformScales->Add(value);
        }
    };

    // Do we want to close the curve by ourselves?
    if (bCloseCurveManually)
    {
        // We need to duplicate the info of the first point to the last
        DuplicateRotScaleUScale(0, NumberOfCVs++);

        // We need to upddate the closed parameter
        FHoudiniApi::SetParmIntValue(
            FHoudiniEngine::Get().GetSession(), NodeId,
            HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, 1);
    }

    // INTERPOLATION
    if (CurveTypeValue == HAPI_CURVETYPE_NURBS)
    {
        // Closed NURBS have additional points reproducing the first ones
        if (CurveClosed)
        {
            // Only the first one if the method is freehand ... 
            DuplicateRotScaleUScale(0, NumberOfCVs++);

            if (CurveMethodValue != 2)
            {
                // ... but also the 2nd and 3rd if the method is CVs or Breakpoints.
                DuplicateRotScaleUScale(1, NumberOfCVs++);
                DuplicateRotScaleUScale(2, NumberOfCVs++);
            }
        }
        else if (CurveMethodValue == 1)
        {
            // Open NURBS have 2 new points if the method is breakpoint:
            // One between the 1st and 2nd ...
            InterpolateRotScaleUScale(0, 1, 0.5f, 1);

            // ... and one before the last one.
            InterpolateRotScaleUScale(NumberOfCVs, NumberOfCVs - 1, 0.5f, NumberOfCVs);
            NumberOfCVs += 2;
        }
    }
    else if (CurveTypeValue == HAPI_CURVETYPE_BEZIER)
    {
        // Bezier curves requires additional point if the method is Breakpoints
        if (CurveMethodValue == 1)
        {
            // 2 interpolated control points are added per points (except the last one)
            int32 nOffset = 0;
            for (int32 n = 0; n < NumberOfCVs - 1; n++)
            {
                int nIndex1 = n + nOffset;
                int nIndex2 = n + nOffset + 1;

                InterpolateRotScaleUScale(nIndex1, nIndex2, 0.33f, nIndex2);
                nIndex2++;
                InterpolateRotScaleUScale(nIndex1, nIndex2, 0.66f, nIndex2);

                nOffset += 2;
            }
            NumberOfCVs += nOffset;

            if (CurveClosed)
            {
                // If the curve is closed, we need to add 2 points after the last,
                // interpolated between the last and the first one
                int nIndex = NumberOfCVs - 1;
                InterpolateRotScaleUScale(nIndex, 0, 0.33f, NumberOfCVs++);
                InterpolateRotScaleUScale(nIndex, 0, 0.66f, NumberOfCVs++);

                // and finally, the last point is the first..
                DuplicateRotScaleUScale(0, NumberOfCVs++);
            }
        }
        else if (CurveClosed)
        {
            // For the other methods, if the bezier curve is closed, the last point is the 1st
            DuplicateRotScaleUScale(0, NumberOfCVs++);
        }
    }

    // Even after interpolation, additional points might still be missing:
    // Bezier curves require a certain number of points regarding their order,
    // if points are lacking then HAPI duplicates the last one.
    if (NumberOfCVs < PartInfos.pointCount)
    {
        int nToAdd = PartInfos.pointCount - NumberOfCVs;
        for (int n = 0; n < nToAdd; n++)
        {
            DuplicateRotScaleUScale(NumberOfCVs - 1, NumberOfCVs);
            NumberOfCVs++;
        }
    }

    // To avoid crashes, attributes will only be added if we now have the correct number of them
    bAddRotations = bAddRotations && (Rotations->Num() == PartInfos.pointCount);
    bAddScales3d = bAddScales3d && (Scales3d->Num() == PartInfos.pointCount);
    bAddUniformScales = bAddUniformScales && (UniformScales->Num() == PartInfos.pointCount);

    // We need to increase the point attributes count for points in the Part Infos
    HAPI_AttributeOwner NewAttributesOwner = HAPI_ATTROWNER_POINT;
    HAPI_AttributeOwner OriginalAttributesOwner = HAPI_ATTROWNER_POINT;

    int OriginalPointParametersCount = PartInfos.pointAttributeCount;
    if (bAddRotations)
        PartInfos.pointAttributeCount += 1;
    if (bAddScales3d)
        PartInfos.pointAttributeCount += 1;
    if (bAddUniformScales)
        PartInfos.pointAttributeCount += 1;

    // Sending the updated PartInfos
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), CurveAssetId, 0, 0, &PartInfos), false);
    
    // We need now to reproduce ALL the curves attributes for ALL the Owners..
    for (int nOwner = 0; nOwner < HAPI_ATTROWNER_MAX; nOwner++)
    {
        int nOwnerAttributeCount = 0;
        if (nOwner == HAPI_ATTROWNER_POINT)
            nOwnerAttributeCount = OriginalPointParametersCount;
        else if (nOwner == HAPI_ATTROWNER_VERTEX)
            nOwnerAttributeCount = PartInfos.vertexAttributeCount;
        else if (nOwner == HAPI_ATTROWNER_PRIM)
            nOwnerAttributeCount = PartInfos.faceAttributeCount;
        else if (nOwner == HAPI_ATTROWNER_DETAIL)
            nOwnerAttributeCount = PartInfos.detailAttributeCount;

        if (nOwnerAttributeCount == 0)
            continue;

        std::vector< HAPI_StringHandle > sh_attributes(nOwnerAttributeCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0, (HAPI_AttributeOwner)nOwner,
            &sh_attributes.front(), sh_attributes.size()), false);

        for (int nAttribute = 0; nAttribute < sh_attributes.size(); nAttribute++)
        {
            const HAPI_StringHandle sh = sh_attributes[nAttribute];
            if (sh == 0)
                continue;

            // Get the attribute name
            FHoudiniEngineString sName(sh);
            std::string attr_name;
            sName.ToStdString(attr_name);

            if (strcmp(attr_name.c_str(), "__topology") == 0)
                continue;

            // and the attribute infos
            HAPI_AttributeInfo attr_info;
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(),
                CurveAssetId, 0, 0, 0,
                attr_name.c_str(), (HAPI_AttributeOwner)nOwner,
                &attr_info), false);

            switch (attr_info.storage)
            {
                case HAPI_STORAGETYPE_INT:
                {
                    // Storing IntData
                    TArray< int > IntData;
                    IntData.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

                    // GET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0, 0,
                        attr_name.c_str(), &attr_info,
                        IntData.GetData(),
                        0, attr_info.count), false);

                    // ADD
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0,
                        attr_name.c_str(), &attr_info), false);

                    // SET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0,
                        attr_name.c_str(), &attr_info,
                        IntData.GetData(),
                        0, attr_info.count), false);
                }
                break;

                case HAPI_STORAGETYPE_FLOAT:
                {
                    // Storing Float Data
                    TArray< float > FloatData;
                    FloatData.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

                    // GET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatData(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0, 0,
                        attr_name.c_str(), &attr_info,
                        FloatData.GetData(),
                        0, attr_info.count), false);

                    // ADD
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0,
                        attr_name.c_str(), &attr_info), false);

                    // SET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0,
                        attr_name.c_str(), &attr_info, FloatData.GetData(),
                        0, attr_info.count), false);
                }
                break;

                case HAPI_STORAGETYPE_STRING:
                {
                    // Storing String Data
                    TArray<HAPI_StringHandle> StringHandleData;
                    StringHandleData.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

                    // GET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0, 0,
                        attr_name.c_str(), &attr_info,
                        StringHandleData.GetData(),
                        0, attr_info.count), false);

                    // Convert the SH to const char *
                    TArray<const char *> StringData;
                    StringData.SetNumUninitialized(attr_info.count);
                    for (int n = 0; n < StringHandleData.Num(); n++)
                    {
                        // Converting the string
                        FHoudiniEngineString strHE(sh);
                        std::string strSTD;
                        strHE.ToStdString(strSTD);

                        StringData[n] = strSTD.c_str();
                    }

                    // ADD
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0,
                        attr_name.c_str(), &attr_info), false);

                    // SET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
                        FHoudiniEngine::Get().GetSession(),
                        CurveAssetId, 0, 0,
                        attr_name.c_str(), &attr_info,
                        StringData.GetData(),
                        0, attr_info.count), false);
                }
                break;

                default:
                    continue;
            }
        }
    }

    // Only GET/SET curve infos if the part is a curve...
    // (Closed linear curves are actually not considered as curves...)
    if (PartInfos.type == HAPI_PARTTYPE_CURVE)
    {
        // We need to read the curve infos ...
        HAPI_CurveInfo CurveInfo;
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveInfo(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0,
            &CurveInfo), false);

        // ... the curve counts
        TArray< int > CurveCounts;
        CurveCounts.SetNumUninitialized(CurveInfo.curveCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveCounts(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0,
            CurveCounts.GetData(),
            0, CurveInfo.curveCount), false);

        // .. the curve orders
        TArray< int > CurveOrders;
        CurveOrders.SetNumUninitialized(CurveInfo.curveCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveOrders(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0,
            CurveOrders.GetData(),
            0, CurveInfo.curveCount), false);

        // .. And the Knots if they exist.
        TArray< float > KnotsArray;
        if (CurveInfo.hasKnots)
        {
            KnotsArray.SetNumUninitialized(CurveInfo.knotCount);
            HOUDINI_CHECK_ERROR_RETURN(
                FHoudiniApi::GetCurveKnots(
                    FHoudiniEngine::Get().GetSession(),
                    CurveAssetId, 0, 0, 0,
                    KnotsArray.GetData(),
                    0, CurveInfo.knotCount), false);
        }

        // To set them back in HAPI
        // CurveInfo
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveInfo(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0,
            &CurveInfo), false);

        // CurveCounts
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveCounts(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0,
            CurveCounts.GetData(), 
            0, CurveInfo.curveCount), false);

        // CurveOrders
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveOrders(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0, 0,
            CurveOrders.GetData(),
            0, CurveInfo.curveCount), false);

        // And Knots if they exist
        if (CurveInfo.hasKnots)
        {
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveKnots(
                FHoudiniEngine::Get().GetSession(),
                CurveAssetId, 0, 0, 0,
                KnotsArray.GetData(),
                0, CurveInfo.knotCount), false);
        }
    }

    if (PartInfos.faceCount > 0)
    {
        // getting the face counts
        TArray< int > FaceCounts;
        FaceCounts.SetNumUninitialized(PartInfos.faceCount);

        if (FHoudiniApi::GetFaceCounts(
                FHoudiniEngine::Get().GetSession(),
                CurveAssetId, 0, 0, 0,
                FaceCounts.GetData(),
                0, PartInfos.faceCount) == HAPI_RESULT_SUCCESS)
            {
            // Set the face count
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(
                FHoudiniEngine::Get().GetSession(),
                CurveAssetId, 0, 0,
                FaceCounts.GetData(), 
                0, PartInfos.faceCount), false);
        }
    }

    if (PartInfos.vertexCount > 0)
    {
        // the vertex list
        TArray< int > VertexList;
        VertexList.SetNumUninitialized(PartInfos.vertexCount);

        if (FHoudiniApi::GetVertexList(
                FHoudiniEngine::Get().GetSession(),
                CurveAssetId, 0, 0, 0,
                VertexList.GetData(), 
                0, PartInfos.vertexCount) == HAPI_RESULT_SUCCESS)
        {
            // setting the vertex list
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(
                FHoudiniEngine::Get().GetSession(),
                CurveAssetId, 0, 0,
                VertexList.GetData(), 
                0, PartInfos.vertexCount), false);
        }
    }

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;
    if (HoudiniRuntimeSettings)
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;

    // We can add attributes to the curve now that all the curves attributes
    // and properties have been reset.
    if (bAddRotations)
    {
        // Create ROTATION attribute info
        HAPI_AttributeInfo AttributeInfoRotation;
        FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoRotation);
        AttributeInfoRotation.count = NumberOfCVs;
        AttributeInfoRotation.tupleSize = 4;
        AttributeInfoRotation.exists = true;
        AttributeInfoRotation.owner = NewAttributesOwner;
        AttributeInfoRotation.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoRotation.originalOwner = OriginalAttributesOwner;

        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), 
            CurveAssetId, 0, 0,
            HAPI_UNREAL_ATTRIB_ROTATION,
            &AttributeInfoRotation), false);

        // Convert the rotation infos
        TArray< float > CurveRotations;
        CurveRotations.SetNumZeroed(NumberOfCVs * 4);
        for (int32 Idx = 0; Idx < NumberOfCVs; ++Idx)
        {
            // Get current quaternion
            const FQuat& RotationQuaternion = (*Rotations)[Idx];

            if (ImportAxis == HRSAI_Unreal)
            {
                CurveRotations[Idx * 4 + 0] = RotationQuaternion.X;
                CurveRotations[Idx * 4 + 1] = RotationQuaternion.Z;
                CurveRotations[Idx * 4 + 2] = RotationQuaternion.Y;
                CurveRotations[Idx * 4 + 3] = -RotationQuaternion.W;
            }
            else if (ImportAxis == HRSAI_Houdini)
            {
                CurveRotations[Idx * 4 + 0] = RotationQuaternion.X;
                CurveRotations[Idx * 4 + 1] = RotationQuaternion.Y;
                CurveRotations[Idx * 4 + 2] = RotationQuaternion.Z;
                CurveRotations[Idx * 4 + 3] = RotationQuaternion.W;
            }
            else
            {
                // Not valid enum value.
                check(0);
            }
        }

        //we can now upload them to our attribute.
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            CurveAssetId, 0, 0,
            HAPI_UNREAL_ATTRIB_ROTATION,
            &AttributeInfoRotation,
            CurveRotations.GetData(),
            0, AttributeInfoRotation.count), false);
    }

    // Create SCALE attribute info.
    if (bAddScales3d)
    {
        HAPI_AttributeInfo AttributeInfoScale;
        FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoScale);
        AttributeInfoScale.count = NumberOfCVs;
        AttributeInfoScale.tupleSize = 3;
        AttributeInfoScale.exists = true;
        AttributeInfoScale.owner = NewAttributesOwner;
        AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoScale.originalOwner = OriginalAttributesOwner;

        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), 
            CurveAssetId, 0, 0,
            HAPI_UNREAL_ATTRIB_SCALE,
            &AttributeInfoScale), false);

        // Convert the scale
        TArray< float > CurveScales;
        CurveScales.SetNumZeroed(NumberOfCVs * 3);
        for (int32 Idx = 0; Idx < NumberOfCVs; ++Idx)
        {
            // Get current scale
            FVector ScaleVector = (*Scales3d)[Idx];
            if (ImportAxis == HRSAI_Unreal)
            {
                CurveScales[Idx * 3 + 0] = ScaleVector.X;
                CurveScales[Idx * 3 + 1] = ScaleVector.Z;
                CurveScales[Idx * 3 + 2] = ScaleVector.Y;
            }
            else if (ImportAxis == HRSAI_Houdini)
            {
                CurveScales[Idx * 3 + 0] = ScaleVector.X;
                CurveScales[Idx * 3 + 1] = ScaleVector.Y;
                CurveScales[Idx * 3 + 2] = ScaleVector.Z;
            }
            else
            {
                // Not valid enum value.
                check(0);
            }
        }

        // We can now upload them to our attribute.
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(), 
            CurveAssetId, 0, 0,
            HAPI_UNREAL_ATTRIB_SCALE,
            &AttributeInfoScale, CurveScales.GetData(),
            0, AttributeInfoScale.count), false);
    }

    // Create PSCALE attribute info.
    if (bAddUniformScales)
    {
        HAPI_AttributeInfo AttributeInfoPScale;
        FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoPScale);
        AttributeInfoPScale.count = NumberOfCVs;
        AttributeInfoPScale.tupleSize = 1;
        AttributeInfoPScale.exists = true;
        AttributeInfoPScale.owner = NewAttributesOwner;
        AttributeInfoPScale.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPScale.originalOwner = OriginalAttributesOwner;

        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), 
            CurveAssetId, 0, 0,
            HAPI_UNREAL_ATTRIB_UNIFORM_SCALE,
            &AttributeInfoPScale), false);

        // Get the current uniform scale.
        TArray<float> CurvePScales;
        CurvePScales.SetNumZeroed(NumberOfCVs);
        for (int32 Idx = 0; Idx < NumberOfCVs; ++Idx)
        {
            CurvePScales[Idx] = (*UniformScales)[Idx];
        }

        // We can now upload them to our attribute.
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(), 
            CurveAssetId, 0, 0,
            HAPI_UNREAL_ATTRIB_UNIFORM_SCALE, 
            &AttributeInfoPScale, CurvePScales.GetData(),
            0, AttributeInfoPScale.count), false);
    }

    // Finally, commit the geo ...
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(),
        CurveAssetId, 0, 0), false);

    // ... and cook the node with curve refinement
    CookOptions.refineCurveToLinear = true;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(
        FHoudiniEngine::Get().GetSession(), 
        CurveAssetId, &CookOptions), false);
#endif

    return true;
}


bool
FHoudiniEngineUtils::HapiGetNodeId( HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_NodeId & NodeId )
{
    if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        HAPI_GeoInfo GeoInfo;
        if ( FHoudiniApi::GetGeoInfo(
            FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, &GeoInfo ) == HAPI_RESULT_SUCCESS )
        {
            NodeId = GeoInfo.nodeId;
            return true;
        }
    }

    NodeId = -1;
    return false;
}

bool
FHoudiniEngineUtils::GetNodePathToAsset( const FHoudiniGeoPartObject& GeoPart, FString & OutPath )
{
    HAPI_GeoInfo GeoInfo;
    if ( FHoudiniApi::GetGeoInfo( 
        FHoudiniEngine::Get().GetSession(), GeoPart.AssetId, GeoPart.ObjectId, GeoPart.GeoId, &GeoInfo ) == HAPI_RESULT_SUCCESS )
    {
        HAPI_NodeInfo GeoNodeInfo;
        if ( FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, &GeoNodeInfo ) == HAPI_RESULT_SUCCESS )
        {
            HAPI_AssetInfo AssetInfo;
            if ( FHoudiniApi::GetAssetInfo(
                FHoudiniEngine::Get().GetSession(), GeoPart.AssetId, &AssetInfo ) == HAPI_RESULT_SUCCESS )
            {
                HAPI_NodeInfo AssetNodeInfo;
                if ( FHoudiniApi::GetNodeInfo(
                    FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &AssetNodeInfo ) == HAPI_RESULT_SUCCESS )
                {
                    FString AssetPath;
                    FHoudiniEngineString AssetPathHEString( AssetNodeInfo.internalNodePathSH );
                    if ( AssetPathHEString.ToFString( AssetPath ) )
                    {
                        AssetPathHEString = FHoudiniEngineString( GeoNodeInfo.internalNodePathSH );
                        if ( AssetPathHEString.ToFString( OutPath ) )
                        {
                            if ( OutPath.RemoveFromStart( AssetPath ) )
                            {
                                OutPath.RemoveFromStart( TEXT( "/" ) );
                                if ( OutPath.IsEmpty() )
                                    OutPath = TEXT( "." );
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(
    HAPI_AssetId HostAssetId, int32 InputIndex,
    ALandscapeProxy * LandscapeProxy, HAPI_AssetId & ConnectedAssetId,
    bool bExportOnlySelected, bool bExportCurves,
    bool bExportMaterials, bool bExportFullGeometry,
    bool bExportLighting, bool bExportNormalizedUVs,
    bool bExportTileUVs )
{
#if WITH_EDITOR

    // If we don't have any landscapes or host asset is invalid then there's nothing to do.
    if ( !LandscapeProxy || !FHoudiniEngineUtils::IsHoudiniAssetValid( HostAssetId ) )
        return false;

    // Check if connected asset id is invalid, if it is not, we need to create an input asset.
    if ( ConnectedAssetId < 0 )
    {
        HAPI_AssetId AssetId = -1;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputAsset(
            FHoudiniEngine::Get().GetSession(), &AssetId, nullptr ), false );

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) )
            return false;

        // We now have a valid id.
        ConnectedAssetId = AssetId;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookAsset(
            FHoudiniEngine::Get().GetSession(), AssetId, nullptr ), false );
    }

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    const ULandscapeInfo * LandscapeInfo = LandscapeProxy->GetLandscapeInfo();

    TSet< ULandscapeComponent * > SelectedComponents;
    if ( bExportOnlySelected && LandscapeInfo )
        SelectedComponents = LandscapeInfo->GetSelectedComponents();

    bExportOnlySelected = bExportOnlySelected && SelectedComponents.Num() > 0;

    int32 MinX = TNumericLimits< int32 >::Max();
    int32 MinY = TNumericLimits< int32 >::Max();
    int32 MaxX = TNumericLimits< int32 >::Lowest();
    int32 MaxY = TNumericLimits< int32 >::Lowest();

    for ( int32 ComponentIdx = 0, ComponentNum = LandscapeProxy->LandscapeComponents.Num();
        ComponentIdx < ComponentNum; ComponentIdx++ )
    {
        ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];

        if ( bExportOnlySelected && !SelectedComponents.Contains( LandscapeComponent ) )
            continue;

        LandscapeComponent->GetComponentExtent( MinX, MinY, MaxX, MaxY );
    }

    // Landscape actor transform.
    const FTransform & LandscapeTransform = LandscapeProxy->LandscapeActorToWorld();

    // Add Weightmap UVs to match with an exported weightmap, not the original weightmap UVs, which are per-component.
    //const FVector2D UVScale = FVector2D(1.0f, 1.0f) / FVector2D((MaxX - MinX) + 1, (MaxY - MinY) + 1);

    int32 ComponentSizeQuads = ( ( LandscapeProxy->ComponentSizeQuads + 1 ) >> LandscapeProxy->ExportLOD ) - 1;
    float ScaleFactor = (float) LandscapeProxy->ComponentSizeQuads / (float) ComponentSizeQuads;

    int32 NumComponents = LandscapeProxy->LandscapeComponents.Num();
    if(bExportOnlySelected)
    {
        NumComponents = SelectedComponents.Num();
    }

    int32 VertexCountPerComponent = FMath::Square( ComponentSizeQuads + 1 );
    int32 VertexCount = NumComponents * VertexCountPerComponent;
    int32 TriangleCount = NumComponents * FMath::Square( ComponentSizeQuads ) * 2;
    int32 QuadCount = NumComponents * FMath::Square( ComponentSizeQuads );

    // Compute number of necessary indices.
    int32 IndexCount = QuadCount * 4;

    if ( !VertexCount )
        return false;

    // Create part.
    HAPI_PartInfo Part;
    FMemory::Memzero< HAPI_PartInfo >( Part );
    Part.id = 0;
    Part.nameSH = 0;
    Part.pointAttributeCount = 0;
    Part.faceAttributeCount = 0;
    Part.vertexAttributeCount = 0;
    Part.detailAttributeCount = 0;
    Part.vertexCount = 0;
    Part.faceCount = 0;
    Part.pointCount = VertexCount;
    Part.type = HAPI_PARTTYPE_MESH;

    // If we are exporting full geometry.
    if ( bExportFullGeometry )
    {
        Part.vertexCount = IndexCount;
        Part.faceCount = QuadCount;
    }

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, &Part ), false );

    // Extract point data.
    TArray< float > AllPositions;
    AllPositions.SetNumUninitialized( VertexCount * 3 );

    // Array which stores indices of landscape components, for each point.
    TArray< const char * > PositionTileNames;
    PositionTileNames.SetNumUninitialized( VertexCount );

    // Temporary array to hold unique raw names.
    TArray< TSharedPtr< char > > UniqueNames;

    // Array which stores indices of vertices (x,y) within each landscape component.
    TArray< int > PositionComponentVertexIndices;
    PositionComponentVertexIndices.SetNumUninitialized( VertexCount * 2 );

    TArray< FVector > PositionNormals;
    PositionNormals.SetNumUninitialized( VertexCount );

    // Array which stores uvs.
    TArray< FVector > PositionUVs;
    PositionUVs.SetNumUninitialized( VertexCount );

    // Array which holds lightmap color value.
    TArray< FLinearColor > LightmapVertexValues;
    LightmapVertexValues.SetNumUninitialized( VertexCount );

    // Array which stores weightmap uvs.
    //TArray< FVector2D > PositionWeightmapUVs;
    //PositionWeightmapUVs.SetNumUninitialized( VertexCount );

    // Array which holds face materials and face hole materials.
    TArray< const char * > FaceMaterials;
    TArray< const char * > FaceHoleMaterials;

    FIntPoint IntPointMax = FIntPoint::ZeroValue;

    int32 AllPositionsIdx = 0;
    for ( int32 ComponentIdx = 0, ComponentNum = LandscapeProxy->LandscapeComponents.Num();
        ComponentIdx < ComponentNum; ComponentIdx++ )
    {
        ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];

        if ( bExportOnlySelected && !SelectedComponents.Contains( LandscapeComponent ) )
            continue;

        TArray< uint8 > LightmapMipData;
        int32 LightmapMipSizeX = 0;
        int32 LightmapMipSizeY = 0;

        // See if we need to export lighting information.
        if ( bExportLighting )
        {
            const FMeshMapBuildData* MapBuildData = LandscapeComponent->GetMeshMapBuildData();
            FLightMap2D* LightMap2D = MapBuildData && MapBuildData->LightMap ? MapBuildData->LightMap->GetLightMap2D() : nullptr;
            if ( LightMap2D )
            {
                if ( LightMap2D->IsValid( 0 ) )
                {
                    UTexture2D * TextureLightmap = LightMap2D->GetTexture( 0 );
                    if ( TextureLightmap )
                    {
                        if ( TextureLightmap->Source.GetMipData( LightmapMipData, 0 ) )
                        {
                            LightmapMipSizeX = TextureLightmap->Source.GetSizeX();
                            LightmapMipSizeY = TextureLightmap->Source.GetSizeY();
                        }
                        else
                        {
                            LightmapMipData.Empty();
                        }
                    }
                }
            }
        }

        // Construct landscape component data interface to access raw data.
        FLandscapeComponentDataInterface CDI( LandscapeComponent, LandscapeProxy->ExportLOD );

        // Get name of this landscape component.
        char * LandscapeComponentNameStr = FHoudiniEngineUtils::ExtractRawName( LandscapeComponent->GetName() );
        UniqueNames.Add( TSharedPtr< char >( LandscapeComponentNameStr ) );

        for ( int32 VertexIdx = 0; VertexIdx < VertexCountPerComponent; VertexIdx++ )
        {
            int32 VertX = 0;
            int32 VertY = 0;
            CDI.VertexIndexToXY( VertexIdx, VertX, VertY );

            // Get position.
            FVector PositionVector = CDI.GetWorldVertex( VertX, VertY );

            // Get normal / tangent / binormal.
            FVector Normal = FVector::ZeroVector;
            FVector TangentX = FVector::ZeroVector;
            FVector TangentY = FVector::ZeroVector;
            CDI.GetLocalTangentVectors( VertX, VertY, TangentX, TangentY, Normal );

            // Export UVs.
            FVector TextureUV = FVector::ZeroVector;

            if ( bExportTileUVs )
            {
                // We want to export uvs per tile.
                TextureUV = FVector( VertX, VertY, 0.0f );

                // If we need to normalize UV space.
                if ( bExportNormalizedUVs )
                    TextureUV /= ComponentSizeQuads;
            }
            else
            {
                // We want to export global uvs (default).
                FIntPoint IntPoint = LandscapeComponent->GetSectionBase();
                TextureUV = FVector( VertX * ScaleFactor + IntPoint.X, VertY * ScaleFactor + IntPoint.Y, 0.0f );

                // Keep track of max offset.
                IntPointMax = IntPointMax.ComponentMax( IntPoint );
            }

            if ( bExportLighting )
            {
                FLinearColor VertexLightmapColor( 0.0f, 0.0f, 0.0f, 1.0f );

                if ( LightmapMipData.Num() > 0 )
                {
                    FVector2D UVCoord( VertX, VertY );
                    UVCoord /= ( ComponentSizeQuads + 1 );

                    FColor LightmapColorRaw = PickVertexColorFromTextureMip(
                        LightmapMipData.GetData(), UVCoord, LightmapMipSizeX, LightmapMipSizeY );

                    VertexLightmapColor = LightmapColorRaw.ReinterpretAsLinear();
                }

                LightmapVertexValues[ AllPositionsIdx ] = VertexLightmapColor;
            }

            // Get Weightmap UV data.
            //FVector2D WeightmapUV = ( TextureUV - FVector2D( MinX, MinY ) ) * UVScale;
            //WeightmapUV.Y = 1.0f - WeightmapUV.Y;

            // Retrieve component transform.
            const FTransform & ComponentTransform = LandscapeComponent->ComponentToWorld;

            // Retrieve component scale.
            const FVector & ScaleVector = ComponentTransform.GetScale3D();

            // Perform normalization.
            Normal /= ScaleVector;
            Normal.Normalize();

            TangentX /= ScaleVector;
            TangentX.Normalize();

            TangentY /= ScaleVector;
            TangentY.Normalize();

            // Peform position scaling.
            FVector PositionTransformed = PositionVector / GeneratedGeometryScaleFactor;

            if ( ImportAxis == HRSAI_Unreal )
            {
                AllPositions[ AllPositionsIdx * 3 + 0 ] = PositionTransformed.X;
                AllPositions[ AllPositionsIdx * 3 + 1 ] = PositionTransformed.Z;
                AllPositions[ AllPositionsIdx * 3 + 2 ] = PositionTransformed.Y;

                Swap( Normal.Y, Normal.Z );
            }
            else if ( ImportAxis == HRSAI_Houdini )
            {
                AllPositions[ AllPositionsIdx * 3 + 0 ] = PositionTransformed.X;
                AllPositions[ AllPositionsIdx * 3 + 1 ] = PositionTransformed.Y;
                AllPositions[ AllPositionsIdx * 3 + 2 ] = PositionTransformed.Z;
            }
            else
            {
                // Not a valid enum value.
                check( 0 );
            }

            // Store landscape component name for this point.
            PositionTileNames[ AllPositionsIdx ] = LandscapeComponentNameStr;

            // Store vertex index (x,y) for this point.
            PositionComponentVertexIndices[ AllPositionsIdx * 2 + 0 ] = VertX;
            PositionComponentVertexIndices[ AllPositionsIdx * 2 + 1 ] = VertY;

            // Store point normal.
            PositionNormals[ AllPositionsIdx ] = Normal;

            // Store uv.
            PositionUVs[ AllPositionsIdx ] = TextureUV;

            // Store weightmap uv.
            //PositionWeightmapUVs[ AllPositionsIdx ] = WeightmapUV;

            AllPositionsIdx++;
        }
    }

    // If we need to normalize UV space and we are doing global UVs.
    if ( !bExportTileUVs && bExportNormalizedUVs )
    {
        IntPointMax += FIntPoint( ComponentSizeQuads, ComponentSizeQuads );
        IntPointMax = IntPointMax.ComponentMax( FIntPoint( 1, 1 ) );

        for ( int32 UVIdx = 0; UVIdx < VertexCount; ++UVIdx )
        {
            FVector & PositionUV = PositionUVs[ UVIdx ];
            PositionUV.X /= IntPointMax.X;
            PositionUV.Y /= IntPointMax.Y;
        }
    }

    // Create point attribute containing landscape component name.
    {
        HAPI_AttributeInfo AttributeInfoPointLandscapeComponentNames;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentNames );
        AttributeInfoPointLandscapeComponentNames.count = VertexCount;
        AttributeInfoPointLandscapeComponentNames.tupleSize = 1;
        AttributeInfoPointLandscapeComponentNames.exists = true;
        AttributeInfoPointLandscapeComponentNames.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointLandscapeComponentNames.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfoPointLandscapeComponentNames.originalOwner = HAPI_ATTROWNER_INVALID;

        bool bFailedAttribute = true;

        if ( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
            &AttributeInfoPointLandscapeComponentNames ) == HAPI_RESULT_SUCCESS )
        {
            if ( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
                &AttributeInfoPointLandscapeComponentNames,
                (const char **) PositionTileNames.GetData(), 0,
                AttributeInfoPointLandscapeComponentNames.count ) == HAPI_RESULT_SUCCESS )
            {
                bFailedAttribute = false;
            }
        }

        // Delete allocated raw names.
        UniqueNames.Empty();

        if ( bFailedAttribute )
            return false;
    }

    // Create point attribute info containing positions.
    {
        HAPI_AttributeInfo AttributeInfoPointPosition;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointPosition );
        AttributeInfoPointPosition.count = VertexCount;
        AttributeInfoPointPosition.tupleSize = 3;
        AttributeInfoPointPosition.exists = true;
        AttributeInfoPointPosition.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointPosition.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPointPosition.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointPosition ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_POSITION,
            &AttributeInfoPointPosition, AllPositions.GetData(),
            0, AttributeInfoPointPosition.count ), false );
    }

    // Create point attribute info containing normals.
    {
        HAPI_AttributeInfo AttributeInfoPointNormal;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointNormal );
        AttributeInfoPointNormal.count = VertexCount;
        AttributeInfoPointNormal.tupleSize = 3;
        AttributeInfoPointNormal.exists = true;
        AttributeInfoPointNormal.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointNormal.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPointNormal.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal,
            (const float *) PositionNormals.GetData(), 0, AttributeInfoPointNormal.count ), false );
    }

    // Create point attribute containing landscape component vertex indices (indices of vertices within the grid - x,y).
    {
        HAPI_AttributeInfo AttributeInfoPointLandscapeComponentVertexIndices;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentVertexIndices );
        AttributeInfoPointLandscapeComponentVertexIndices.count = VertexCount;
        AttributeInfoPointLandscapeComponentVertexIndices.tupleSize = 2;
        AttributeInfoPointLandscapeComponentVertexIndices.exists = true;
        AttributeInfoPointLandscapeComponentVertexIndices.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointLandscapeComponentVertexIndices.storage = HAPI_STORAGETYPE_INT;
        AttributeInfoPointLandscapeComponentVertexIndices.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
            &AttributeInfoPointLandscapeComponentVertexIndices ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
            &AttributeInfoPointLandscapeComponentVertexIndices,
            PositionComponentVertexIndices.GetData(), 0,
            AttributeInfoPointLandscapeComponentVertexIndices.count ), false );
    }

    // Create point attribute info containing UVs.
    {
        HAPI_AttributeInfo AttributeInfoPointUV;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointUV );
        AttributeInfoPointUV.count = VertexCount;
        AttributeInfoPointUV.tupleSize = 3;
        AttributeInfoPointUV.exists = true;
        AttributeInfoPointUV.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointUV.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPointUV.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV,
            (const float *) PositionUVs.GetData(), 0, AttributeInfoPointUV.count ), false );
    }

    // Create point attribute info containing lightmap information.
    if ( bExportLighting )
    {
        HAPI_AttributeInfo AttributeInfoPointLightmapColor;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLightmapColor );
        AttributeInfoPointLightmapColor.count = VertexCount;
        AttributeInfoPointLightmapColor.tupleSize = 4;
        AttributeInfoPointLightmapColor.exists = true;
        AttributeInfoPointLightmapColor.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointLightmapColor.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPointLightmapColor.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor,
            (const float *) LightmapVertexValues.GetData(), 0,
            AttributeInfoPointLightmapColor.count ), false );
    }

    // Create point attribute info containing weightmap UVs.
    /*
    {
        HAPI_AttributeInfo AttributeInfoPointWeightmapUV;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointWeightmapUV );
        AttributeInfoPointWeightmapUV.count = VertexCount;
        AttributeInfoPointWeightmapUV.tupleSize = 2;
        AttributeInfoPointWeightmapUV.exists = true;
        AttributeInfoPointWeightmapUV.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPointWeightmapUV.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPointWeightmapUV.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_UV_WEIGHTMAP, &AttributeInfoPointWeightmapUV ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_UV_WEIGHTMAP, &AttributeInfoPointWeightmapUV,
            (const float*) PositionWeightmapUVs.GetData(), 0,
            AttributeInfoPointWeightmapUV.count ), false );
    }
    */

    // Set indices if we are exporting full geometry.
    if ( bExportFullGeometry && IndexCount > 0 )
    {
        // Array holding indices data.
        TArray< int32 > LandscapeIndices;
        LandscapeIndices.SetNumUninitialized( IndexCount );

        // Allocate space for face names.
        FaceMaterials.SetNumUninitialized( QuadCount );
        FaceHoleMaterials.SetNumUninitialized( QuadCount );

        int32 VertIdx = 0;
        int32 QuadIdx = 0;

        char * MaterialRawStr = nullptr;
        char * MaterialHoleRawStr = nullptr;

        const int32 QuadComponentCount = ComponentSizeQuads + 1;
        for ( int32 ComponentIdx = 0; ComponentIdx < NumComponents; ComponentIdx++ )
        {
            ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];

            // If component has an override material, we need to get the raw name (if exporting materials).
            if ( bExportMaterials && LandscapeComponent->OverrideMaterial )
            {
                MaterialRawStr = FHoudiniEngineUtils::ExtractRawName( LandscapeComponent->OverrideMaterial->GetName() );
                UniqueNames.Add( TSharedPtr< char >( MaterialRawStr ) );
            }

            // If component has an override hole material, we need to get the raw name (if exporting materials).
            if ( bExportMaterials && LandscapeComponent->OverrideHoleMaterial )
            {
                MaterialHoleRawStr = FHoudiniEngineUtils::ExtractRawName(
                    LandscapeComponent->OverrideHoleMaterial->GetName() );
                UniqueNames.Add( TSharedPtr< char >( MaterialHoleRawStr ) );
            }

            int32 BaseVertIndex = ComponentIdx * VertexCountPerComponent;
            for ( int32 YIdx = 0; YIdx < ComponentSizeQuads; YIdx++ )
            {
                for ( int32 XIdx = 0; XIdx < ComponentSizeQuads; XIdx++ )
                {
                    if ( ImportAxis == HRSAI_Unreal )
                    {
                        LandscapeIndices[ VertIdx + 0 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 0 ) * QuadComponentCount;
                        LandscapeIndices[ VertIdx + 1 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 0 ) * QuadComponentCount;
                        LandscapeIndices[ VertIdx + 2 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 1 ) * QuadComponentCount;
                        LandscapeIndices[ VertIdx + 3 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 1 ) * QuadComponentCount;
                    }
                    else if ( ImportAxis == HRSAI_Houdini )
                    {
                        LandscapeIndices[ VertIdx + 0 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 0 ) * QuadComponentCount;
                        LandscapeIndices[ VertIdx + 1 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 1 ) * QuadComponentCount;
                        LandscapeIndices[ VertIdx + 2 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 1 ) * QuadComponentCount;
                        LandscapeIndices[ VertIdx + 3 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 0 ) * QuadComponentCount;
                    }
                    else
                    {
                        // Not a valid enum value.
                        check( 0 );
                    }

                    // Store override materials (if exporting materials).
                    if ( bExportMaterials )
                    {
                        FaceMaterials[ QuadIdx ] = MaterialRawStr;
                        FaceHoleMaterials[ QuadIdx ] = MaterialHoleRawStr;
                    }

                    VertIdx += 4;
                    QuadIdx++;
                }
            }
        }

        // We can now set vertex list.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetVertexList(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, LandscapeIndices.GetData(), 0, LandscapeIndices.Num() ), false );

        // We need to generate array of face counts.
        TArray< int32 > LandscapeFaces;
        LandscapeFaces.Init( 4, QuadCount );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetFaceCounts(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
            0, 0, LandscapeFaces.GetData(), 0, LandscapeFaces.Num() ), false );
    }

    // If we are marshalling material information.
    if ( bExportMaterials )
    {
        // Get name of attribute used for marshalling materials.
        std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL;
        if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
        {
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterial,
                MarshallingAttributeMaterialName );
        }

        // Get name of attribute used for marshalling hole materials.
        std::string MarshallingAttributeMaterialHoleName = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;
        if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterialHole.IsEmpty() )
        {
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterialHole,
                MarshallingAttributeMaterialHoleName );
        }

        // Marshall in override primitive material names.
        if ( bExportFullGeometry )
        {
            HAPI_AttributeInfo AttributeInfoPrimitiveMaterial;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterial );
            AttributeInfoPrimitiveMaterial.count = FaceMaterials.Num();
            AttributeInfoPrimitiveMaterial.tupleSize = 1;
            AttributeInfoPrimitiveMaterial.exists = true;
            AttributeInfoPrimitiveMaterial.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfoPrimitiveMaterial.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoPrimitiveMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoPrimitiveMaterial ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoPrimitiveMaterial,
                (const char **) FaceMaterials.GetData(), 0, AttributeInfoPrimitiveMaterial.count ), false );
        }

        // Marshall in override primitive material hole names.
        if ( bExportFullGeometry )
        {
            HAPI_AttributeInfo AttributeInfoPrimitiveMaterialHole;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterialHole );
            AttributeInfoPrimitiveMaterialHole.count = FaceHoleMaterials.Num();
            AttributeInfoPrimitiveMaterialHole.tupleSize = 1;
            AttributeInfoPrimitiveMaterialHole.exists = true;
            AttributeInfoPrimitiveMaterialHole.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfoPrimitiveMaterialHole.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoPrimitiveMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0, 0, MarshallingAttributeMaterialHoleName.c_str(),
                &AttributeInfoPrimitiveMaterialHole ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0, 0, MarshallingAttributeMaterialHoleName.c_str(),
                &AttributeInfoPrimitiveMaterialHole, (const char **) FaceHoleMaterials.GetData(), 0,
                AttributeInfoPrimitiveMaterialHole.count ), false );
        }

        // If there's a global landscape material, we marshall it as detail.
        {
            UMaterialInterface * MaterialInterface = LandscapeProxy->GetLandscapeMaterial();
            const char * MaterialNameStr = "";
            if ( MaterialInterface )
            {
                FString FullMaterialName = MaterialInterface->GetPathName();
                MaterialNameStr = TCHAR_TO_UTF8( *FullMaterialName );
            }

            HAPI_AttributeInfo AttributeInfoDetailMaterial;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterial );
            AttributeInfoDetailMaterial.count = 1;
            AttributeInfoDetailMaterial.tupleSize = 1;
            AttributeInfoDetailMaterial.exists = true;
            AttributeInfoDetailMaterial.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoDetailMaterial.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoDetailMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoDetailMaterial ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoDetailMaterial,
                (const char**) &MaterialNameStr, 0, AttributeInfoDetailMaterial.count ), false );
        }

        // If there's a global landscape hole material, we marshall it as detail.
        {
            UMaterialInterface * MaterialInterface = LandscapeProxy->GetLandscapeHoleMaterial();
            const char * MaterialNameStr = "";
            if ( MaterialInterface )
            {
                FString FullMaterialName = MaterialInterface->GetPathName();
                MaterialNameStr = TCHAR_TO_UTF8( *FullMaterialName );
            }

            HAPI_AttributeInfo AttributeInfoDetailMaterialHole;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterialHole );
            AttributeInfoDetailMaterialHole.count = 1;
            AttributeInfoDetailMaterialHole.tupleSize = 1;
            AttributeInfoDetailMaterialHole.exists = true;
            AttributeInfoDetailMaterialHole.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoDetailMaterialHole.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoDetailMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0, 0, MarshallingAttributeMaterialHoleName.c_str(),
                &AttributeInfoDetailMaterialHole ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0, 0, MarshallingAttributeMaterialHoleName.c_str(),
                &AttributeInfoDetailMaterialHole, (const char **) &MaterialNameStr, 0,
                AttributeInfoDetailMaterialHole.count ), false );
        }
    }

    // Commit the geo.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0 ), false );

    // Now we can connect assets together.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectAssetGeometry(
        FHoudiniEngine::Get().GetSession(),
        ConnectedAssetId, 0, HostAssetId, InputIndex ), false );

#endif

    return true;
}


bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(
    HAPI_AssetId HostAssetId,
    int32 InputIndex,
    USplineComponent * SplineComponent,
    HAPI_AssetId & ConnectedAssetId,
    FHoudiniAssetInputOutlinerMesh& OutlinerMesh,
    const float& SplineResolution)
{
#if WITH_EDITOR

    // If we don't have a spline component, or host asset is invalid, there's nothing to do.
    if (!SplineComponent || !FHoudiniEngineUtils::IsHoudiniAssetValid(HostAssetId))
        return false;

    float fSplineResolution = SplineResolution;
    if (fSplineResolution == -1.0f)
    {
        // Get runtime settings and extract the spline resolution from it
        const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
        if (HoudiniRuntimeSettings)
            fSplineResolution = HoudiniRuntimeSettings->MarshallingSplineResolution;
        else
            fSplineResolution = HAPI_UNREAL_PARAM_SPLINE_RESOLUTION_DEFAULT;
    }

    int32 nNumberOfControlPoints = SplineComponent->GetNumberOfSplinePoints();
    float fSplineLength = SplineComponent->GetSplineLength();

    // Calculate the number of refined point we want
    int32 nNumberOfRefinedSplinePoints = fSplineResolution > 0.0f ? ceil(fSplineLength / fSplineResolution) + 1 : nNumberOfControlPoints;

    // Array that will store the attributes we want to add to the curves
    TArray<FVector> tRefinedSplinePositions;
    TArray<FQuat> tRefinedSplineRotations;
    // Scale on Unreal's spline will require some tweaking, as the XScale is always 1
    TArray<FVector> tRefinedSplineScales;
    //TArray<float> tRefinedSplinePScales;

    if ((nNumberOfRefinedSplinePoints < nNumberOfControlPoints) || (fSplineResolution <= 0.0f))
    {
        // There's not enough refined points, so we'll use the Spline CVs instead
        tRefinedSplinePositions.SetNumZeroed(nNumberOfControlPoints);
        tRefinedSplineRotations.SetNumZeroed(nNumberOfControlPoints);
        tRefinedSplineScales.SetNumZeroed(nNumberOfControlPoints);
        //tRefinedSplinePScales.SetNumZeroed(nNumberOfControlPoints);

        FVector Scale;
        for (int32 n = 0; n < nNumberOfControlPoints; n++)
        {
            tRefinedSplinePositions[n] = SplineComponent->GetLocationAtSplinePoint(n, ESplineCoordinateSpace::Local);
            tRefinedSplineRotations[n] = SplineComponent->GetQuaternionAtSplinePoint(n, ESplineCoordinateSpace::World);
            Scale = SplineComponent->GetScaleAtSplinePoint(n);
            tRefinedSplineScales[n] = Scale;
            // tRefinedSplinePScales[n] = (Scale.Y + Scale.Z) / 2.0f; //FMath::Max(Scale.Y, Scale.Z);
        }
    }
    else
    {
        // Calculating the refined spline points
        tRefinedSplinePositions.SetNumZeroed(nNumberOfRefinedSplinePoints);
        tRefinedSplineRotations.SetNumZeroed(nNumberOfRefinedSplinePoints);
        tRefinedSplineScales.SetNumZeroed(nNumberOfRefinedSplinePoints);
        // tRefinedSplinePScales.SetNumZeroed(nNumberOfRefinedSplinePoints);

        FVector Scale;
        float fCurrentDistance = 0.0f;
        for (int32 n = 0; n < nNumberOfRefinedSplinePoints; n++)
        {
            tRefinedSplinePositions[n] = SplineComponent->GetLocationAtDistanceAlongSpline(fCurrentDistance, ESplineCoordinateSpace::Local);
            tRefinedSplineRotations[n] = SplineComponent->GetQuaternionAtDistanceAlongSpline(fCurrentDistance, ESplineCoordinateSpace::World);

            Scale = SplineComponent->GetScaleAtDistanceAlongSpline(fCurrentDistance);

            tRefinedSplineScales[n] = Scale;
            // tRefinedSplinePScales[n] = (Scale.Y + Scale.Z) / 2.0f; //FMath::Max(Scale.Y, Scale.Z);

            fCurrentDistance += fSplineResolution;
        }
    }

    if (!HapiCreateCurveAsset(
            HostAssetId, ConnectedAssetId,
            &tRefinedSplinePositions,
            &tRefinedSplineRotations,
            &tRefinedSplineScales,
            nullptr) )//&tRefinedSplinePScales) )
        return false;

    // Updating the OutlinerMesh's struct infos
    OutlinerMesh.SplineResolution = fSplineResolution;
    OutlinerMesh.SplineLength = fSplineLength;
    OutlinerMesh.NumberOfSplineControlPoints = nNumberOfControlPoints;

    // We also need to extract all the CV's Transform to check for modifications
    OutlinerMesh.SplineControlPointsTransform.SetNum(nNumberOfControlPoints);
    for (int32 n = 0; n < nNumberOfControlPoints; n++)
    {
        // Getting the Local Transform for positions and scale
        OutlinerMesh.SplineControlPointsTransform[n] = SplineComponent->GetTransformAtSplinePoint(n, ESplineCoordinateSpace::Local, true);

        // ... but we used we used the world rotation
        OutlinerMesh.SplineControlPointsTransform[n].SetRotation(SplineComponent->GetQuaternionAtSplinePoint(n, ESplineCoordinateSpace::World));
    }

    // Cook the spline node.
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(
        FHoudiniEngine::Get().GetSession(),
        ConnectedAssetId,
        nullptr), false);

    // Connect asset.
    if (!FHoudiniEngineUtils::HapiConnectAsset(ConnectedAssetId, 0, HostAssetId, InputIndex))
    {
        ConnectedAssetId = -1;
        return false;
    }

#endif

    return true;
}




bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(
    HAPI_AssetId HostAssetId, 
    int32 InputIndex, 
    UStaticMesh * StaticMesh,
    HAPI_AssetId & ConnectedAssetId)
{
#if WITH_EDITOR

    // If we don't have a static mesh, or host asset is invalid, there's nothing to do.
    if ( !StaticMesh || !FHoudiniEngineUtils::IsHoudiniAssetValid( HostAssetId ) )
        return false;

    // Check if connected asset id is valid, if it is not, we need to create an input asset.
    if ( ConnectedAssetId < 0 )
    {
        HAPI_AssetId AssetId = -1;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputAsset(
            FHoudiniEngine::Get().GetSession(), &AssetId, nullptr ), false );

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) )
            return false;

        // We now have a valid id.
        ConnectedAssetId = AssetId;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookAsset(
            FHoudiniEngine::Get().GetSession(), AssetId, nullptr ), false );
    }

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    check( HoudiniRuntimeSettings );

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;
    int32 GeneratedLightMapResolution = 32;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
        GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
    }

    // Grab base LOD level.
    FStaticMeshSourceModel & SrcModel = StaticMesh->SourceModels[ 0 ];

    // Load the existing raw mesh.
    FRawMesh RawMesh;
    SrcModel.RawMeshBulkData->LoadRawMesh( RawMesh );

    // Create part.
    HAPI_PartInfo Part;
    FMemory::Memzero< HAPI_PartInfo >( Part );
    Part.id = 0;
    Part.nameSH = 0;
    Part.pointAttributeCount = 0;
    Part.faceAttributeCount = 0;
    Part.vertexAttributeCount = 0;
    Part.detailAttributeCount = 0;
    Part.vertexCount = RawMesh.WedgeIndices.Num();
    Part.faceCount = RawMesh.WedgeIndices.Num() / 3;
    Part.pointCount = RawMesh.VertexPositions.Num();
    Part.type = HAPI_PARTTYPE_MESH;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, &Part ), false );

    // Create point attribute info.
    HAPI_AttributeInfo AttributeInfoPoint;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPoint );
    AttributeInfoPoint.count = RawMesh.VertexPositions.Num();
    AttributeInfoPoint.tupleSize = 3;
    AttributeInfoPoint.exists = true;
    AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
        HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint ), false );

    // Extract vertices from static mesh.
    TArray< float > StaticMeshVertices;
    StaticMeshVertices.SetNumZeroed( RawMesh.VertexPositions.Num() * 3 );
    for ( int32 VertexIdx = 0; VertexIdx < RawMesh.VertexPositions.Num(); ++VertexIdx )
    {
        // Grab vertex at this index.
        const FVector & PositionVector = RawMesh.VertexPositions[ VertexIdx ];

        if ( ImportAxis == HRSAI_Unreal )
        {
            StaticMeshVertices[ VertexIdx * 3 + 0 ] = PositionVector.X / GeneratedGeometryScaleFactor;
            StaticMeshVertices[ VertexIdx * 3 + 1 ] = PositionVector.Z / GeneratedGeometryScaleFactor;
            StaticMeshVertices[ VertexIdx * 3 + 2 ] = PositionVector.Y / GeneratedGeometryScaleFactor;
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            StaticMeshVertices[ VertexIdx * 3 + 0 ] = PositionVector.X / GeneratedGeometryScaleFactor;
            StaticMeshVertices[ VertexIdx * 3 + 1 ] = PositionVector.Y / GeneratedGeometryScaleFactor;
            StaticMeshVertices[ VertexIdx * 3 + 2 ] = PositionVector.Z / GeneratedGeometryScaleFactor;
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }
    }

    // Now that we have raw positions, we can upload them for our attribute.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
        0, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint,
        StaticMeshVertices.GetData(), 0,
        AttributeInfoPoint.count ), false );

    // See if we have texture coordinates to upload.
    for ( int32 MeshTexCoordIdx = 0; MeshTexCoordIdx < MAX_STATIC_TEXCOORDS; ++MeshTexCoordIdx )
    {
        int32 StaticMeshUVCount = RawMesh.WedgeTexCoords[ MeshTexCoordIdx ].Num();

        if ( StaticMeshUVCount > 0 )
        {
            const TArray< FVector2D > & RawMeshUVs = RawMesh.WedgeTexCoords[ MeshTexCoordIdx ];
            TArray< FVector > StaticMeshUVs;
            StaticMeshUVs.Reserve( StaticMeshUVCount );

            // Transfer UV data.
            for ( int32 UVIdx = 0; UVIdx < StaticMeshUVCount; ++UVIdx )
                StaticMeshUVs.Emplace( RawMeshUVs[ UVIdx ].X, 1.0 - RawMeshUVs[ UVIdx ].Y, 0 );

            if ( ImportAxis == HRSAI_Unreal )
            {
                // We need to re-index UVs for wedges we swapped (due to winding differences).
                for ( int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3 )
                {
                    // We do not touch wedge 0 of this triangle.
                    StaticMeshUVs.SwapMemory( WedgeIdx + 1, WedgeIdx + 2 );
                }
            }
            else if ( ImportAxis == HRSAI_Houdini )
            {
                // Do nothing, data is in proper format.
            }
            else
            {
                // Not valid enum value.
                check( 0 );
            }

            // Construct attribute name for this index.
            std::string UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

            if ( MeshTexCoordIdx > 0 )
                UVAttributeName += std::to_string( MeshTexCoordIdx + 1 );

            const char * UVAttributeNameString = UVAttributeName.c_str();

            // Create attribute for UVs
            HAPI_AttributeInfo AttributeInfoVertex;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoVertex );
            AttributeInfoVertex.count = StaticMeshUVCount;
            AttributeInfoVertex.tupleSize = 3;
            AttributeInfoVertex.exists = true;
            AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
            AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
            AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
                0, 0, UVAttributeNameString, &AttributeInfoVertex ), false );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0, 0, UVAttributeNameString, &AttributeInfoVertex,
                (const float *) StaticMeshUVs.GetData(), 0, AttributeInfoVertex.count ), false );
        }
    }

    // See if we have normals to upload.
    if ( RawMesh.WedgeTangentZ.Num() > 0 )
    {
        TArray< FVector > ChangedNormals( RawMesh.WedgeTangentZ );

        if ( ImportAxis == HRSAI_Unreal )
        {
            // We need to re-index normals for wedges we swapped (due to winding differences).
            for ( int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3 )
            {
                FVector TangentZ1 = ChangedNormals[ WedgeIdx + 1 ];
                FVector TangentZ2 = ChangedNormals[ WedgeIdx + 2 ];

                ChangedNormals[ WedgeIdx + 1 ] = TangentZ2;
                ChangedNormals[ WedgeIdx + 2 ] = TangentZ1;
            }

            for ( int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); ++WedgeIdx )
                Swap( ChangedNormals[ WedgeIdx ].Y, ChangedNormals[ WedgeIdx ].Z );
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            // Do nothing, data is in proper format.
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }

        // Create attribute for normals.
        HAPI_AttributeInfo AttributeInfoVertex;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoVertex );
        AttributeInfoVertex.count = ChangedNormals.Num();
        AttributeInfoVertex.tupleSize = 3;
        AttributeInfoVertex.exists = true;
        AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
        AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex,
            (const float *) ChangedNormals.GetData(),
            0, AttributeInfoVertex.count ), false );
    }

    // See if we have colors to upload.
    if ( RawMesh.WedgeColors.Num() > 0 )
    {
        TArray< FLinearColor > ChangedColors;
        ChangedColors.SetNumUninitialized( RawMesh.WedgeColors.Num() );

        if ( ImportAxis == HRSAI_Unreal )
        {
            // We need to re-index colors for wedges we swapped (due to winding differences).
            for ( int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3 )
            {
                ChangedColors[ WedgeIdx + 0 ] = FLinearColor( RawMesh.WedgeColors[ WedgeIdx + 0 ] );
                ChangedColors[ WedgeIdx + 1 ] = FLinearColor( RawMesh.WedgeColors[ WedgeIdx + 2 ] );
                ChangedColors[ WedgeIdx + 2 ] = FLinearColor( RawMesh.WedgeColors[ WedgeIdx + 1 ] );
            }
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            ChangedColors = TArray< FLinearColor >( RawMesh.WedgeColors );
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }

        // Create attribute for colors.
        HAPI_AttributeInfo AttributeInfoVertex;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoVertex );
        AttributeInfoVertex.count = ChangedColors.Num();
        AttributeInfoVertex.tupleSize = 4;
        AttributeInfoVertex.exists = true;
        AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
        AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex,
            (const float *) ChangedColors.GetData(), 0, AttributeInfoVertex.count ), false );
    }

    // Extract indices from static mesh.
    if ( RawMesh.WedgeIndices.Num() > 0 )
    {
        TArray< int32 > StaticMeshIndices;
        StaticMeshIndices.SetNumUninitialized( RawMesh.WedgeIndices.Num() );

        if ( ImportAxis == HRSAI_Unreal )
        {
            for ( int32 IndexIdx = 0; IndexIdx < RawMesh.WedgeIndices.Num(); IndexIdx += 3 )
            {
                // Swap indices to fix winding order.
                StaticMeshIndices[ IndexIdx + 0 ] = RawMesh.WedgeIndices[ IndexIdx + 0 ];
                StaticMeshIndices[ IndexIdx + 1 ] = RawMesh.WedgeIndices[ IndexIdx + 2 ];
                StaticMeshIndices[ IndexIdx + 2 ] = RawMesh.WedgeIndices[ IndexIdx + 1 ];
            }
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            StaticMeshIndices = TArray< int32 >( RawMesh.WedgeIndices );
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }

        // We can now set vertex list.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetVertexList(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, StaticMeshIndices.GetData(), 0, StaticMeshIndices.Num() ), false );

        // We need to generate array of face counts.
        TArray< int32 > StaticMeshFaceCounts;
        StaticMeshFaceCounts.Init( 3, Part.faceCount );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetFaceCounts(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, StaticMeshFaceCounts.GetData(), 0, StaticMeshFaceCounts.Num() ), false );
    }

    // Marshall face material indices.
    if ( RawMesh.FaceMaterialIndices.Num() > 0 )
    {
        // Create list of materials, one for each face.
        TArray< char * > StaticMeshFaceMaterials;
        FHoudiniEngineUtils::CreateFaceMaterialArray(
            StaticMesh->StaticMaterials, RawMesh.FaceMaterialIndices,
            StaticMeshFaceMaterials );

        // Get name of attribute used for marshalling materials.
        std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_MATERIAL;
        if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterial, MarshallingAttributeName );

        // Create attribute for materials.
        HAPI_AttributeInfo AttributeInfoMaterial;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoMaterial );
        AttributeInfoMaterial.count = RawMesh.FaceMaterialIndices.Num();
        AttributeInfoMaterial.tupleSize = 1;
        AttributeInfoMaterial.exists = true;
        AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
        AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

        bool bAttributeError = false;

        if ( FHoudiniApi::AddAttribute( FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
            MarshallingAttributeName.c_str(), &AttributeInfoMaterial ) != HAPI_RESULT_SUCCESS )
        {
            bAttributeError = true;
        }

        if ( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, MarshallingAttributeName.c_str(), &AttributeInfoMaterial,
            (const char **) StaticMeshFaceMaterials.GetData(), 0,
            StaticMeshFaceMaterials.Num() ) != HAPI_RESULT_SUCCESS )
        {
            bAttributeError = true;
        }

        // Delete material names.
        FHoudiniEngineUtils::DeleteFaceMaterialArray( StaticMeshFaceMaterials );

        if ( bAttributeError )
        {
            check( 0 );
            return false;
        }
    }

    // Marshall face smoothing masks.
    if ( RawMesh.FaceSmoothingMasks.Num() > 0 )
    {
        // Get name of attribute used for marshalling face smoothing masks.
        std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK;
        if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
        {
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask,
                MarshallingAttributeName );
        }

        HAPI_AttributeInfo AttributeInfoSmoothingMasks;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoSmoothingMasks );
        AttributeInfoSmoothingMasks.count = RawMesh.FaceSmoothingMasks.Num();
        AttributeInfoSmoothingMasks.tupleSize = 1;
        AttributeInfoSmoothingMasks.exists = true;
        AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
        AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
        AttributeInfoSmoothingMasks.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks,
            (const int32 *) RawMesh.FaceSmoothingMasks.GetData(), 0, RawMesh.FaceSmoothingMasks.Num() ), false );
    }

    // Marshall lightmap resolution.
    if ( StaticMesh->LightMapResolution != GeneratedLightMapResolution )
    {
        std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION;
        if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution.IsEmpty() )
        {
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution,
                MarshallingAttributeName );
        }

        TArray< int32 > LightMapResolutions;
        LightMapResolutions.Add( StaticMesh->LightMapResolution );

        HAPI_AttributeInfo AttributeInfoLightMapResolution;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoLightMapResolution );
        AttributeInfoLightMapResolution.count = LightMapResolutions.Num();
        AttributeInfoLightMapResolution.tupleSize = 1;
        AttributeInfoLightMapResolution.exists = true;
        AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
        AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
        AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0,
            0, MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution,
            (const int32 *) LightMapResolutions.GetData(), 0, LightMapResolutions.Num() ), false );
    }

    if ( !HoudiniRuntimeSettings->MarshallingAttributeInputMeshName.IsEmpty() )
    {
        // Create primitive attribute with mesh asset path

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
        const FString MeshAssetPath = StaticMesh->GetPathName();
        std::string MeshAssetPathCStr = TCHAR_TO_ANSI( *MeshAssetPath );
        const char* MeshAssetPathRaw = MeshAssetPathCStr.c_str();
        TArray<const char*> PrimitiveAttrs;
        PrimitiveAttrs.AddUninitialized( Part.faceCount );
        for ( int32 Ix = 0; Ix < Part.faceCount; ++Ix )
        {
            PrimitiveAttrs[ Ix ] = MeshAssetPathRaw;
        }

        std::string MarshallingAttributeName;
        FHoudiniEngineUtils::ConvertUnrealString(
            HoudiniRuntimeSettings->MarshallingAttributeInputMeshName,
            MarshallingAttributeName );
        
        HAPI_AttributeInfo AttributeInfo {};
        AttributeInfo.count = Part.faceCount;
        AttributeInfo.tupleSize = 1;
        AttributeInfo.exists = true;
        AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
        AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
            0, 0, MarshallingAttributeName.c_str(), &AttributeInfo ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 0, MarshallingAttributeName.c_str(), &AttributeInfo,
            PrimitiveAttrs.GetData(), 0, PrimitiveAttrs.Num() ), false );
    }

    // Commit the geo.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0 ), false );

    // Now we can connect assets together.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectAssetGeometry(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
        0, HostAssetId, InputIndex ), false );

#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(
    HAPI_AssetId HostAssetId,
    int32 InputIndex,
    TArray< FHoudiniAssetInputOutlinerMesh > & OutlinerMeshArray,
    HAPI_AssetId & ConnectedAssetId,
    const float& SplineResolution)
{
    if ( OutlinerMeshArray.Num() <= 0 )
        return false;

    // Create the merge SOP asset. This will be our "ConnectedAssetId".
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::InstantiateAsset(
        FHoudiniEngine::Get().GetSession(),
        "SOP/merge", true, &ConnectedAssetId ), false );

    for ( int32 InputIdx = 0; InputIdx < OutlinerMeshArray.Num(); ++InputIdx )
    {
        auto & OutlinerMesh = OutlinerMeshArray[ InputIdx ];
        
        bool bInputCreated = false;
        if (OutlinerMesh.StaticMesh != nullptr)
        {
            // Creating an Input Node for Mesh Data
            bInputCreated = HapiCreateAndConnectAsset(
                ConnectedAssetId,
                InputIdx,
                OutlinerMesh.StaticMesh,
                OutlinerMesh.AssetId );
        }
        else if (OutlinerMesh.SplineComponent != nullptr)
        {
            // Creating an input node for spline data
            bInputCreated = HapiCreateAndConnectAsset(
                ConnectedAssetId,
                InputIdx,
                OutlinerMesh.SplineComponent,
                OutlinerMesh.AssetId,
                OutlinerMesh,
                SplineResolution);
        }

        if ( !bInputCreated )
        {
            OutlinerMesh.AssetId = -1;
            continue;
        }

        
        // Updating the Transform
        HAPI_TransformEuler HapiTransform;
        FHoudiniEngineUtils::TranslateUnrealTransform( OutlinerMesh.ComponentTransform, HapiTransform );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAssetTransform(
            FHoudiniEngine::Get().GetSession(),
            OutlinerMesh.AssetId, &HapiTransform ), false );
    }

    // Now we can connect assets together.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectAssetGeometry(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, HostAssetId, InputIndex ), false );

    return true;
}

bool 
FHoudiniEngineUtils::HapiCreateAndConnectAsset( HAPI_AssetId HostAssetId, int32 InputIndex, TArray<UObject *>& InputObjects, HAPI_AssetId & ConnectedAssetId, TArray< HAPI_NodeId >& OutCreatedNodeIds )
{
    if ( ensure( InputObjects.Num() ) )
    {
        // TODO: No need to merge if there is only one input object if ( InputObjects.Num() == 1 )
        // Create the merge SOP asset. This will be our "ConnectedAssetId".
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::InstantiateAsset(
            FHoudiniEngine::Get().GetSession(),
            "SOP/merge", true, &ConnectedAssetId ), false );

        for ( int32 InputIdx = 0; InputIdx < InputObjects.Num(); ++InputIdx )
        {
            if ( UStaticMesh* InputStaticMesh = Cast< UStaticMesh >( InputObjects[ InputIdx ] ) )
            {
                HAPI_AssetId MeshAssetNodeId = -1;
                // Creating an Input Node for Mesh Data
                bool bInputCreated = HapiCreateAndConnectAsset( ConnectedAssetId, InputIdx, InputStaticMesh, MeshAssetNodeId );
                if ( !bInputCreated )
                {
                    HOUDINI_LOG_WARNING( TEXT( "Error creating input index %d on %d" ), InputIdx, ConnectedAssetId );
                }
                if ( MeshAssetNodeId >= 0 )
                {
                    OutCreatedNodeIds.Add( MeshAssetNodeId );
                }
            }
        }
        // Now we can connect assets together.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectAssetGeometry(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, HostAssetId, InputIndex ), false );
    }
    return true;
}

bool
FHoudiniEngineUtils::HapiDisconnectAsset( HAPI_AssetId HostAssetId, int32 InputIndex )
{
#if WITH_EDITOR

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::DisconnectAssetGeometry(
        FHoudiniEngine::Get().GetSession(), HostAssetId,
        InputIndex ), false );

#endif // WITH_EDITOR

    return true;
}

bool
FHoudiniEngineUtils::HapiConnectAsset(
    HAPI_AssetId AssetIdFrom, HAPI_ObjectId ObjectIdFrom, HAPI_AssetId AssetIdTo, int32 InputIndex )
{
#if WITH_EDITOR

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectAssetGeometry(
        FHoudiniEngine::Get().GetSession(), AssetIdFrom,
        ObjectIdFrom, AssetIdTo, InputIndex ), false );

#endif // WITH_EDITOR

    return true;
}

bool
FHoudiniEngineUtils::HapiSetAssetTransform( HAPI_AssetId AssetId, const FTransform & Transform )
{
    if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        // Translate Unreal transform to HAPI Euler one.
        HAPI_TransformEuler TransformEuler;
        FHoudiniEngineUtils::TranslateUnrealTransform( Transform, TransformEuler );

        if ( FHoudiniApi::SetAssetTransform(
            FHoudiniEngine::Get().GetSession(), AssetId, &TransformEuler ) == HAPI_RESULT_SUCCESS )
        {
            return true;
        }
    }

    return false;
}

bool FHoudiniEngineUtils::CheckPackageSafeForBake( UPackage* Package, FString& FoundAssetName )
{
    if( Package )
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
        TArray<FAssetData> AssetsInPackage;
        AssetRegistryModule.Get().GetAssetsByPackageName( Package->GetFName(), AssetsInPackage );
        for( const auto& AssetInfo : AssetsInPackage )
        {
            if( AssetInfo.GetAsset() )
            {
                // Check and see whether we are referenced by any objects that won't be garbage collected (*including* the undo buffer)
                FReferencerInformationList ReferencesIncludingUndo;
                UObject* AssetInPackage = AssetInfo.GetAsset();
                bool bReferencedInMemoryOrUndoStack = IsReferenced( AssetInPackage, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &ReferencesIncludingUndo );
                if( bReferencedInMemoryOrUndoStack )
                {
                    // warn
                    HOUDINI_LOG_ERROR( TEXT( "Could not bake to %s because it is being referenced" ), *Package->GetPathName() );
                    return false;
                }
                FoundAssetName = AssetInfo.AssetName.ToString();
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}
UPackage *
FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    FString & MeshName, FGuid & BakeGUID, EBakeMode BakeMode )
{
    UPackage * PackageNew = nullptr;

#if WITH_EDITOR

    FString PackageName;
    int32 BakeCount = 0;
    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

    const FGuid & ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
    FString ComponentGUIDString = ComponentGUID.ToString().Left(
        FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    while ( true )
    {
        if( BakeMode == EBakeMode::ReplaceExisitingAssets )
        {
            // Find a previously baked asset
            if( auto FoundPackage = HoudiniAssetComponent->BakedStaticMeshPackagesForParts.Find( HoudiniGeoPartObject ) )
            {
                if( (*FoundPackage).IsValid() )
                {
                    if( CheckPackageSafeForBake( (*FoundPackage).Get(), MeshName ) )
                    {
                        return (*FoundPackage).Get();
                    }
                    else
                    {
                        // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                        //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );
                        return nullptr;
                    }
                }
                else
                {
                    HoudiniAssetComponent->BakedStaticMeshPackagesForParts.Remove( HoudiniGeoPartObject );
                }
            }
        }

        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        // We only want half of generated guid string.
        FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );
        FString PartName = TEXT( "" );

        if ( HoudiniGeoPartObject.HasCustomName() )
            PartName = TEXT( "_" ) + HoudiniGeoPartObject.PartName;

        if ( BakeMode != EBakeMode::Intermediate )
        {
            MeshName = HoudiniAsset->GetName() + PartName + FString::Printf( TEXT( "_bake%d_" ), BakeCount ) +
                FString::FromInt( HoudiniGeoPartObject.ObjectId ) + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.GeoId ) + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.PartId ) + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.SplitId ) + TEXT( "_" ) +
                HoudiniGeoPartObject.SplitName;
                
            PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOutermost()->GetName() ) +
                TEXT( "/" ) +
                MeshName;
        }
        else
        {
            MeshName = HoudiniAsset->GetName() + PartName + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.ObjectId ) + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.GeoId ) + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.PartId ) + TEXT( "_" ) +
                FString::FromInt( HoudiniGeoPartObject.SplitId ) + TEXT( "_" ) +
                HoudiniGeoPartObject.SplitName + TEXT( "_" ) +
                BakeGUIDString;
            
            PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOuter()->GetName()) +
                TEXT( "/" ) +
                HoudiniAsset->GetName() +
                PartName +
                TEXT( "_" ) +
                ComponentGUIDString +
                TEXT( "/" ) +
                MeshName;
        }

        // Santize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;

        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniAssetComponent->GetComponentLevel();
        }

        // See if package exists, if it does, we need to regenerate the name.
        UPackage * Package = FindPackage( OuterPackage, *PackageName );

        if ( Package )
        {
            if ( BakeMode != EBakeMode::Intermediate )
            {
                // Increment bake counter
                BakeCount++;
            }
            else
            {
                // Package does exist, there's a collision, we need to generate a new name.
                BakeGUID.Invalidate();
            }
        }
        else
        {
            // Create actual package.
            PackageNew = CreatePackage( OuterPackage, *PackageName );
            break;
        }
    }

#endif

    if( PackageNew && BakeMode == EBakeMode::ReplaceExisitingAssets )
    {
        HoudiniAssetComponent->BakedStaticMeshPackagesForParts.Add( HoudiniGeoPartObject, PackageNew );
    }

    return PackageNew;
}

UPackage *
FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    FString & BlueprintName )
{
    UPackage * Package = nullptr;

#if WITH_EDITOR

    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    FGuid BakeGUID = FGuid::NewGuid();

    while ( true )
    {
        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        // We only want half of generated guid string.
        FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

        // Generate Blueprint name.
        BlueprintName = HoudiniAsset->GetName() + TEXT( "_" ) + BakeGUIDString;

        // Generate unique package name.=
        FString PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) +
            TEXT( "/" ) +
            BlueprintName;

        PackageName = PackageTools::SanitizePackageName( PackageName );

        // See if package exists, if it does, we need to regenerate the name.
        Package = FindPackage( nullptr, *PackageName );

        if ( Package )
        {
            // Package does exist, there's a collision, we need to generate a new name.
            BakeGUID.Invalidate();
        }
        else
        {
            // Create actual package.
            Package = CreatePackage( nullptr, *PackageName );
            break;
        }
    }

#endif

    return Package;
}

UPackage *
FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const HAPI_MaterialInfo & MaterialInfo, FString & MaterialName, EBakeMode BakeMode )
{
    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    FString MaterialDescriptor;

    if( BakeMode != EBakeMode::Intermediate )
        MaterialDescriptor = HoudiniAsset->GetName() + TEXT( "_material_" ) + FString::FromInt( MaterialInfo.id ) + TEXT( "_" );
    else
        MaterialDescriptor = HoudiniAsset->GetName() + TEXT( "_" ) + FString::FromInt( MaterialInfo.id ) + TEXT( "_" );

    return FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
        HoudiniAssetComponent, MaterialDescriptor,
        MaterialName, BakeMode );
}

UPackage *
FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const FString & MaterialInfoDescriptor,
    FString & MaterialName, EBakeMode BakeMode )
{
    UPackage * PackageNew = nullptr;

#if WITH_EDITOR

    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    FGuid BakeGUID;
    FString PackageName;

    const FGuid & ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
    FString ComponentGUIDString = ComponentGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    if( BakeMode == EBakeMode::ReplaceExisitingAssets )
    {
        // Find a previously baked asset
        if( auto FoundPackage = HoudiniAssetComponent->BakedMaterialPackagesForIds.Find( MaterialInfoDescriptor ) )
        {
            if( ( *FoundPackage ).IsValid() )
            {
                if( CheckPackageSafeForBake( ( *FoundPackage ).Get(), MaterialName ) )
                {
                    return ( *FoundPackage ).Get();
                }
                else
                {
                    // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                    //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );
                    return nullptr;
                }
            }
            else
            {
                HoudiniAssetComponent->BakedMaterialPackagesForIds.Remove( MaterialInfoDescriptor );
            }
        }
    }

    while ( true )
    {
        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        // We only want half of generated guid string.
        FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

        // Generate material name.
        MaterialName = MaterialInfoDescriptor;
        MaterialName += BakeGUIDString;

        if ( BakeMode != EBakeMode::Intermediate )
        {
            // Generate unique package name.
            PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOutermost()->GetName() ) +
                TEXT( "/" ) +
                MaterialName;
        }
        else
        {
            // Generate unique package name.
            PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOuter()->GetName() ) +
                TEXT( "/" ) +
                HoudiniAsset->GetName() +
                TEXT( "_" ) +
                ComponentGUIDString +
                TEXT( "/" ) +
                MaterialName;
        }

        // Sanitize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;

        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniAssetComponent->GetComponentLevel();
        }

        // See if package exists, if it does, we need to regenerate the name.
        UPackage * Package = FindPackage( OuterPackage, *PackageName );

        if ( Package )
        {
            // Package does exist, there's a collision, we need to generate a new name.
            BakeGUID.Invalidate();
        }
        else
        {
            // Create actual package.
            PackageNew = CreatePackage( OuterPackage, *PackageName );
            break;
        }
    }

#endif

    if( PackageNew && BakeMode == EBakeMode::ReplaceExisitingAssets )
    {
        HoudiniAssetComponent->BakedMaterialPackagesForIds.Add( MaterialInfoDescriptor, PackageNew );
    }

    return PackageNew;
}

UPackage *
FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const HAPI_MaterialInfo & MaterialInfo, const FString & TextureType,
    FString & TextureName, EBakeMode BakeMode )
{
    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    FString TextureInfoDescriptor;

    if ( BakeMode != EBakeMode::Intermediate )
    {
        TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT( "_texture_" ) + FString::FromInt( MaterialInfo.id ) +
            TEXT( "_" ) + TextureType + TEXT( "_" );
    }
    else
    {
        TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT( "_" ) + FString::FromInt( MaterialInfo.id ) + TEXT( "_" ) +
            TextureType + TEXT( "_" );
    }

    return FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
        HoudiniAssetComponent, TextureInfoDescriptor,
        TextureType, TextureName, BakeMode );
}

UPackage *
FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const FString & TextureInfoDescriptor, const FString & TextureType,
    FString & TextureName, EBakeMode BakeMode )
{
    UPackage* PackageNew = nullptr;

#if WITH_EDITOR

    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    FGuid BakeGUID;
    FString PackageName;

    const FGuid & ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
    FString ComponentGUIDString = ComponentGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    if( BakeMode == EBakeMode::ReplaceExisitingAssets )
    {
        // Find a previously baked asset
        if( auto FoundPackage = HoudiniAssetComponent->BakedMaterialPackagesForIds.Find( TextureInfoDescriptor ) )
        {
            if( ( *FoundPackage ).IsValid() )
            {
                if( CheckPackageSafeForBake( ( *FoundPackage ).Get(), TextureName ) )
                {
                    return ( *FoundPackage ).Get();
                }
                else
                {
                    // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                    //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );
                    return nullptr;
                }
            }
            else
            {
                HoudiniAssetComponent->BakedMaterialPackagesForIds.Remove( TextureInfoDescriptor );
            }
        }
    }

    while ( true )
    {
        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        // We only want half of generated guid string.
        FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

        // Generate texture name.
        TextureName = TextureInfoDescriptor;
        TextureName += BakeGUIDString;

        if ( BakeMode != EBakeMode::Intermediate )
        {
            // Generate unique package name.=
            PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOutermost()->GetName() ) +
                TEXT( "/" ) +
                TextureName;
        }
        else
        {
            // Generate unique package name.
            PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOuter()->GetName() ) +
                TEXT( "/" ) +
                HoudiniAsset->GetName() +
                TEXT( "_" ) +
                ComponentGUIDString +
                TEXT( "/" ) +
                TextureName;
        }

        // Sanitize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;

        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniAssetComponent->GetComponentLevel();
        }

        // See if package exists, if it does, we need to regenerate the name.
        UPackage * Package = FindPackage( OuterPackage, *PackageName );

        if ( Package )
        {
            // Package does exist, there's a collision, we need to generate a new name.
            BakeGUID.Invalidate();
        }
        else
        {
            // Create actual package.
            PackageNew = CreatePackage( OuterPackage, *PackageName );
            break;
        }
    }

    if( PackageNew && BakeMode == EBakeMode::ReplaceExisitingAssets )
    {
        HoudiniAssetComponent->BakedMaterialPackagesForIds.Add( TextureInfoDescriptor, PackageNew );
    }

#endif

    return PackageNew;
}

bool FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
    UHoudiniAssetComponent * HoudiniAssetComponent, const TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesIn,
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesOut, FTransform & ComponentTransform )
{
#if WITH_EDITOR

    HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();
    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

    if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) || !HoudiniAsset )
        return false;

    // Make sure rendering is done - so we are not changing data being used by collision drawing.
    FlushRenderingCommands();

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    check( HoudiniRuntimeSettings );

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    // Attribute marshalling names.
    std::string MarshallingAttributeNameLightmapResolution = HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION;
    std::string MarshallingAttributeNameMaterial = HAPI_UNREAL_ATTRIB_MATERIAL;
    std::string MarshallingAttributeNameMaterialFallback = HAPI_UNREAL_ATTRIB_MATERIAL_FALLBACK;
    std::string MarshallingAttributeNameFaceSmoothingMask = HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;

        if ( !HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution,
                MarshallingAttributeNameLightmapResolution );

        if ( !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterial,
                MarshallingAttributeNameMaterial );

        if ( !HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask,
                MarshallingAttributeNameFaceSmoothingMask );
    }

    // Get platform manager LOD specific information.
    ITargetPlatform * CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    check( CurrentPlatform );
    const FStaticMeshLODGroup & LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup( NAME_None );
    int32 NumLODs = LODGroup.GetDefaultNumLODs();

    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    // Retrieve asset transform.
    HAPI_TransformEuler AssetEulerTransform;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetTransform(
        FHoudiniEngine::Get().GetSession(), AssetId, HAPI_SRT, HAPI_XYZ, &AssetEulerTransform ), false );

    // Convert HAPI Euler transform to Unreal one.
    FTransform AssetUnrealTransform;
    TranslateHapiTransform( AssetEulerTransform, AssetUnrealTransform );
    ComponentTransform = AssetUnrealTransform;

    // Retrieve information about each object contained within our asset.
    TArray< HAPI_ObjectInfo > ObjectInfos;
    ObjectInfos.SetNumUninitialized( AssetInfo.objectCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetObjects(
        FHoudiniEngine::Get().GetSession(), AssetId, &ObjectInfos[ 0 ], 0, AssetInfo.objectCount ), false );

    // Retrieve transforms for each object in this asset.
    TArray< HAPI_Transform > ObjectTransforms;
    ObjectTransforms.SetNumUninitialized( AssetInfo.objectCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetObjectTransforms(
        FHoudiniEngine::Get().GetSession(), AssetId, HAPI_SRT, &ObjectTransforms[ 0 ], 0,
        AssetInfo.objectCount ), false );

    // Containers used for raw data extraction.
    TArray< int32 > VertexList;
    TArray< float > Positions;
    TArray< float > TextureCoordinates[ MAX_STATIC_TEXCOORDS ];
    TArray< float > Normals;
    TArray< float > Colors;
    TArray< FString > FaceMaterials;
    TArray< int32 > FaceSmoothingMasks;
    TArray< int32 > LightMapResolutions;

    // Retrieve all used unique material ids.
    TSet< HAPI_MaterialId > UniqueMaterialIds;
    TSet< HAPI_MaterialId > UniqueInstancerMaterialIds;
    TMap< FHoudiniGeoPartObject, HAPI_MaterialId > InstancerMaterialMap;
    FHoudiniEngineUtils::ExtractUniqueMaterialIds(
        AssetInfo, UniqueMaterialIds, UniqueInstancerMaterialIds,
        InstancerMaterialMap );

    // Map to hold materials.
    TMap< FString, UMaterialInterface * > Materials;

    // Create materials.
    FHoudiniEngineUtils::HapiCreateMaterials(
        HoudiniAssetComponent, AssetInfo, UniqueMaterialIds,
        UniqueInstancerMaterialIds, Materials );

    // Cache all materials inside the component.
    if ( HoudiniAssetComponent->HoudiniAssetComponentMaterials )
        HoudiniAssetComponent->HoudiniAssetComponentMaterials->Assignments = Materials;

    UStaticMesh * StaticMesh = nullptr;
    FString MeshName;
    FGuid MeshGuid;

    // Iterate through all objects.
    for ( int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx )
    {
        // Retrieve object at this index.
        const HAPI_ObjectInfo & ObjectInfo = ObjectInfos[ ObjectIdx ];

        // Retrieve object name.
        FString ObjectName = TEXT( "" );
        FHoudiniEngineString HoudiniEngineString( ObjectInfo.nameSH );
        HoudiniEngineString.ToFString( ObjectName );

        // Get transformation for this object.
        const HAPI_Transform & ObjectTransform = ObjectTransforms[ ObjectIdx ];
        FTransform TransformMatrix;
        FHoudiniEngineUtils::TranslateHapiTransform( ObjectTransform, TransformMatrix );

        // Iterate through all Geo informations within this object.
        for ( int32 GeoIdx = 0; GeoIdx < ObjectInfo.geoCount; ++GeoIdx )
        {
            // Get Geo information.
            HAPI_GeoInfo GeoInfo;
            if ( FHoudiniApi::GetGeoInfo(
                FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id, GeoIdx, &GeoInfo ) != HAPI_RESULT_SUCCESS )
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d] unable to retrieve GeoInfo, " )
                    TEXT( "- skipping." ),
                    ObjectIdx, *ObjectName, GeoIdx );
                continue;
            }

            if ( GeoInfo.type == HAPI_GEOTYPE_CURVE )
            {
                // If this geo is a curve, we skip part processing.
                FHoudiniGeoPartObject HoudiniGeoPartObject(
                    TransformMatrix, ObjectName, ObjectName, AssetId,
                    ObjectInfo.id, GeoInfo.id, 0 );
                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsCurve = true;
                HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
                HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;

                StaticMesh = nullptr;
                StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );
                continue;
            }

            // Right now only care about display SOPs.
            if ( !GeoInfo.isDisplayGeo )
                continue;

            // Get object / geo group memberships for primitives.
            TArray< FString > ObjectGeoGroupNames;
            if( ! FHoudiniEngineUtils::HapiGetGroupNames(
                AssetId, ObjectInfo.id, GeoIdx, HAPI_GROUPTYPE_PRIM,
                ObjectGeoGroupNames ) )
            {
                HOUDINI_LOG_MESSAGE( TEXT( "Creating Static Meshes: Object [%d %s] non-fatal error reading group names" ),
                    ObjectInfo.nodeId, *ObjectName );
            }

            bool bIsRenderCollidable = false;
            bool bIsCollidable = false;
            bool bIsUCXCollidable = false;
            bool bIsSimpleCollidable = false;
            
            if ( HoudiniRuntimeSettings )
            {
                // Detect if this object has collision geo, rendered collision geo, UCX collisions
                for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < ObjectGeoGroupNames.Num(); ++GeoGroupNameIdx )
                {
                    const FString & GroupName = ObjectGeoGroupNames[ GeoGroupNameIdx ];

                    // UCX and simple collisions need to be checked first as they both start in the same way
                    // as their non UCX/non simple equivalent!
                    if ( !HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix.IsEmpty() &&
                            GroupName.StartsWith(
                            HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        bIsUCXCollidable = true;
                    }
                    else if ( !HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith(
                                HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase ) )
                    {
                        bIsUCXCollidable = true;
                    }
                    else if ( !HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix.IsEmpty() &&
                        GroupName.StartsWith(
                            HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        bIsCollidable = true;
                    }
                    else if ( !HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix.IsEmpty() &&
                        GroupName.StartsWith(
                            HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        bIsRenderCollidable = true;
                    }
                    else if ( !HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith(
                                HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase ) )
                    {
                        bIsRenderCollidable = true;
                    }
                    else if ( !HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith(
                                HoudiniRuntimeSettings->CollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase ) )
                    {
                        bIsCollidable = true;
                    }
                }
            }

            // Prepare the object that will store UCX/UBX/USP Collision geo
            FKAggregateGeom AggregateCollisionGeo;
            bool bHasAggregateGeometryCollision = false;

            // Prepare the object that will store the mesh sockets and their names
            TArray< FTransform > AllSockets;
            TArray< FString > AllSocketsNames;
            TArray< FString > AllSocketsActors;

            for ( int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx )
            {
                // Get part information.
                HAPI_PartInfo PartInfo;
                FString PartName = TEXT( "" );

                if ( FHoudiniApi::GetPartInfo(
                    FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id, GeoInfo.id, PartIdx,
                    &PartInfo ) != HAPI_RESULT_SUCCESS )
                {
                    // Error retrieving part info.
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve PartInfo, " )
                        TEXT( "- skipping." ),
                        ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                    continue;
                }

                // Retrieve part name.
                FHoudiniEngineString HoudiniEngineStringPartName( PartInfo.nameSH );
                HoudiniEngineStringPartName.ToFString( PartName );

                if (PartInfo.type == HAPI_PARTTYPE_INSTANCER)
                {
                    // This is a Packed Primitive instancer
                    FHoudiniGeoPartObject HoudiniGeoPartObject(
                        TransformMatrix, ObjectName, PartName, AssetId,
                        ObjectInfo.id, GeoInfo.id, PartInfo.id );
                    HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
                    HoudiniGeoPartObject.bIsInstancer = false;
                    HoudiniGeoPartObject.bIsCurve = false;
                    HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
                    HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;
                    HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = true;
                    StaticMeshesOut.Add(HoudiniGeoPartObject, nullptr);
                    continue;
                }
                else if( PartInfo.type == HAPI_PARTTYPE_INVALID )
                {
                    continue;
                }

                // Get name of attribute used for marshalling generated mesh name.
                HAPI_AttributeInfo AttribGeneratedMeshName;
                TArray< FString > GeneratedMeshNames;

                {
                    std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_GENERATED_MESH_NAME;
                    if ( HoudiniRuntimeSettings )
                    {
                        FHoudiniEngineUtils::ConvertUnrealString(
                            HoudiniRuntimeSettings->MarshallingAttributeGeneratedMeshName,
                            MarshallingAttributeName );
                    }

                    FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                        AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
                        MarshallingAttributeName.c_str(), AttribGeneratedMeshName, GeneratedMeshNames );
                }

                // There are no vertices and no points.
                if ( PartInfo.vertexCount <= 0 && PartInfo.pointCount <= 0 )
                {
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] no points or vertices found, " )
                        TEXT( "- skipping." ),
                        ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                    continue;
                }

                // Retrieve material information for this geo part.
                TArray< HAPI_MaterialId > FaceMaterialIds;
                HAPI_Bool bSingleFaceMaterial = false;
                bool bMaterialsFound = false;
                bool bMaterialsChanged = false;

                if ( PartInfo.faceCount > 0 )
                {
                    FaceMaterialIds.SetNumUninitialized( PartInfo.faceCount );

                    if ( FHoudiniApi::GetMaterialIdsOnFaces(
                        FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id,
                        GeoInfo.id, PartInfo.id, &bSingleFaceMaterial,
                        &FaceMaterialIds[ 0 ], 0, PartInfo.faceCount ) != HAPI_RESULT_SUCCESS )
                    {
                        // Error retrieving material face assignments.
                        HOUDINI_LOG_MESSAGE(
                            TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments, " )
                            TEXT( "- skipping." ),
                            ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                        continue;
                    }

                    // Set flag if we have materials.
                    for ( int32 MaterialIdx = 0; MaterialIdx < FaceMaterialIds.Num(); ++MaterialIdx )
                    {
                        if ( FaceMaterialIds[ MaterialIdx ] != -1 )
                        {
                            bMaterialsFound = true;
                            break;
                        }
                    }

                    // Set flag if any of the materials have changed.
                    if ( bMaterialsFound )
                    {
                        for ( int32 MaterialFaceIdx = 0; MaterialFaceIdx < FaceMaterialIds.Num(); ++MaterialFaceIdx )
                        {
                            HAPI_MaterialInfo MaterialInfo;

                            if ( FHoudiniApi::GetMaterialInfo(
                                FHoudiniEngine::Get().GetSession(), AssetInfo.id, FaceMaterialIds[MaterialFaceIdx],
                                &MaterialInfo ) != HAPI_RESULT_SUCCESS )
                            {
                                continue;
                            }

                            if ( MaterialInfo.hasChanged )
                                bMaterialsChanged = true;
                        }
                    }
                }

                // Extracting Sockets points
                GetMeshSocketList( AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, AllSockets, AllSocketsNames, AllSocketsActors );

                // Create geo part object identifier.
                FHoudiniGeoPartObject HoudiniGeoPartObject(
                    TransformMatrix, ObjectName, PartName, AssetId,
                    ObjectInfo.id, GeoInfo.id, PartInfo.id );

                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !PartInfo.isInstanced;
                HoudiniGeoPartObject.bIsInstancer = ObjectInfo.isInstancer;
                HoudiniGeoPartObject.bIsCurve = ( PartInfo.type == HAPI_PARTTYPE_CURVE );
                HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
                HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;
                //HoudiniGeoPartObject.bIsBox = ( PartInfo.type == HAPI_PARTTYPE_BOX );
                HoudiniGeoPartObject.bIsVolume = ( PartInfo.type == HAPI_PARTTYPE_VOLUME );

                if ( AttribGeneratedMeshName.exists && GeneratedMeshNames.Num() > 0 )
                {
                    const FString & CustomPartName = GeneratedMeshNames[ 0 ];
                    if ( !CustomPartName.IsEmpty() )
                        HoudiniGeoPartObject.SetCustomName( CustomPartName );
                }

                // We do not create mesh for instancers.
                if ( ObjectInfo.isInstancer )
                {
                    if ( PartInfo.pointCount > 0 )
                    {
                        // We need to check whether this instancer has a material.
                        HAPI_MaterialId const * FoundInstancerMaterialId =
                            InstancerMaterialMap.Find( HoudiniGeoPartObject );
                        if ( FoundInstancerMaterialId )
                        {
                            HAPI_MaterialId InstancerMaterialId = *FoundInstancerMaterialId;

                            FString InstancerMaterialShopName = TEXT( "" );
                            if ( InstancerMaterialId > -1 &&
                                FHoudiniEngineUtils::GetUniqueMaterialShopName( AssetId, InstancerMaterialId, InstancerMaterialShopName ) )
                            {
                                HoudiniGeoPartObject.bInstancerMaterialAvailable = true;
                                HoudiniGeoPartObject.InstancerMaterialName = InstancerMaterialShopName;
                            }
                        }

                        // See if we have instancer attribute material present.
                        {
                            HAPI_AttributeInfo AttribInstancerAttribMaterials;
                            FMemory::Memset< HAPI_AttributeInfo >( AttribInstancerAttribMaterials, 0 );

                            TArray< FString > InstancerAttribMaterials;

                            FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                                AssetId, ObjectInfo.id, GeoInfo.id,
                                PartInfo.id, MarshallingAttributeNameMaterial.c_str(), AttribInstancerAttribMaterials,
                                InstancerAttribMaterials );

                            if ( AttribInstancerAttribMaterials.exists && InstancerAttribMaterials.Num() > 0 )
                            {
                                const FString & InstancerAttribMaterialName = InstancerAttribMaterials[ 0 ];
                                if ( !InstancerAttribMaterialName.IsEmpty() )
                                {
                                    HoudiniGeoPartObject.bInstancerAttributeMaterialAvailable = true;
                                    HoudiniGeoPartObject.InstancerAttributeMaterialName = InstancerAttribMaterialName;
                                }
                            }
                        }

                        // Instancer objects have no mesh assigned.
                        StaticMesh = nullptr;
                        StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );
                        continue;
                    }
                    else
                    {
                        // This is an instancer with no points.
                        HOUDINI_LOG_MESSAGE(
                            TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] is instancer but has 0 points " )
                            TEXT( "skipping." ),
                            ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                        continue;
                    }
                }
                else if ( PartInfo.type == HAPI_PARTTYPE_CURVE )
                {
                    // This is a curve part.
                    StaticMesh = nullptr;
                    StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );
                    continue;
                }
                else if ( PartInfo.vertexCount <= 0 )
                {
                    // This is not an instancer, but we do not have vertices, skip.
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] has 0 vertices and non-zero points, " )
                        TEXT( "but is not an intstancer - skipping." ),
                        ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                    continue;
                }

                // Retrieve all vertex indices.
                VertexList.SetNumUninitialized( PartInfo.vertexCount );

                if ( FHoudiniApi::GetVertexList(
                    FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
                    &VertexList[ 0 ], 0, PartInfo.vertexCount ) != HAPI_RESULT_SUCCESS )
                {
                    // Error getting the vertex list.
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list " )
                        TEXT( "- skipping." ),
                        ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );

                    continue;
                }

                // See if we require splitting.
                TMap< FString, TArray< int32 > > GroupSplitFaces;
                TMap< FString, int32 > GroupSplitFaceCounts;
                TMap< FString, TArray< int32 > > GroupSplitFaceIndices;

                int32 GroupVertexListCount = 0;
                static const FString RemainingGroupName = TEXT( HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION );

                if ( bIsRenderCollidable || bIsCollidable || bIsUCXCollidable )
                {
                    // Buffer for all vertex indices used for collision. We need this to figure out all vertex
                    // indices that are not part of collision geos.
                    TArray< int32 > AllCollisionVertexList;
                    AllCollisionVertexList.SetNumZeroed( VertexList.Num() );

                    // Buffer for all face indices used for collision. We need this to figure out all face indices
                    // that are not part of collision geos.
                    TArray< int32 > AllCollisionFaceIndices;
                    AllCollisionFaceIndices.SetNumZeroed( FaceMaterialIds.Num() );

                    for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < ObjectGeoGroupNames.Num(); ++GeoGroupNameIdx )
                    {
                        const FString & GroupName = ObjectGeoGroupNames[ GeoGroupNameIdx ];

                        if ( ( !HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith( HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase ) ) ||
                            ( !HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith( HoudiniRuntimeSettings->CollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase ) ) ||
                            ( !HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith( HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase ) ) ||
                            ( !HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith( HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase) ) || 
                            ( !HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith( HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase) ) || 
                            ( !HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix.IsEmpty() &&
                                GroupName.StartsWith( HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix,
                                ESearchCase::IgnoreCase) ) )
                        {
                            // New vertex list just for this group.
                            TArray< int32 > GroupVertexList;
                            TArray< int32 > AllFaceList;

                            // Extract vertex indices for this split.
                            GroupVertexListCount = FHoudiniEngineUtils::HapiGetVertexListForGroup(
                                AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, GroupName, VertexList, GroupVertexList,
                                AllCollisionVertexList, AllFaceList, AllCollisionFaceIndices );

                            if ( GroupVertexListCount > 0 )
                            {
                                // If list is not empty, we store it for this group - this will define new mesh.
                                GroupSplitFaces.Add( GroupName, GroupVertexList );
                                GroupSplitFaceCounts.Add( GroupName, GroupVertexListCount );
                                GroupSplitFaceIndices.Add( GroupName, AllFaceList );
                            }
                        }
                    }

                    // We also need to figure out / construct vertex list for everything that's not collision geometry
                    // or rendered collision geometry.
                    TArray< int32 > GroupSplitFacesRemaining;
                    GroupSplitFacesRemaining.Init( -1, VertexList.Num() );
                    bool bMainSplitGroup = false;
                    GroupVertexListCount = 0;

                    TArray< int32 > GroupSplitFaceIndicesRemaining;

                    for ( int32 CollisionVertexIdx = 0; CollisionVertexIdx < AllCollisionVertexList.Num();
                        ++CollisionVertexIdx )
                    {
                        int32 VertexIndex = AllCollisionVertexList[CollisionVertexIdx];
                        if ( VertexIndex == 0 )
                        {
                            // This is unused index, we need to add it to unused vertex list.
                            //GroupSplitFacesRemaining.Add(VertexList[CollisionVertexIdx]);
                            GroupSplitFacesRemaining[ CollisionVertexIdx ] = VertexList[ CollisionVertexIdx ];
                            bMainSplitGroup = true;
                            GroupVertexListCount++;
                        }
                    }

                    for ( int32 CollisionFaceIdx = 0; CollisionFaceIdx < AllCollisionFaceIndices.Num(); ++CollisionFaceIdx )
                    {
                        int32 FaceIndex = AllCollisionFaceIndices[ CollisionFaceIdx ];
                        if ( FaceIndex == 0 )
                        {
                            // This is unused face, we need to add it to unused faces list.
                            GroupSplitFaceIndicesRemaining.Add( CollisionFaceIdx );
                        }
                    }

                    // We store remaining geo vertex list as a special name.
                    if ( bMainSplitGroup )
                    {
                        GroupSplitFaces.Add( RemainingGroupName, GroupSplitFacesRemaining );
                        GroupSplitFaceCounts.Add( RemainingGroupName, GroupVertexListCount );
                        GroupSplitFaceIndices.Add( RemainingGroupName, GroupSplitFaceIndicesRemaining );
                    }
                }
                else
                {
                    GroupSplitFaces.Add( RemainingGroupName, VertexList );
                    GroupSplitFaceCounts.Add( RemainingGroupName, VertexList.Num() );

                    TArray<int32> AllFaces;
                    for ( int32 FaceIdx = 0; FaceIdx < PartInfo.faceCount; ++FaceIdx )
                        AllFaces.Add( FaceIdx );

                    GroupSplitFaceIndices.Add( RemainingGroupName, AllFaces );
                }

                // Keep track of split id.
                int32 SplitId = 0;

                // Iterate through all detected split groups we care about and split geometry.
                for ( TMap< FString, TArray< int32 > >::TIterator IterGroups( GroupSplitFaces ); IterGroups; ++IterGroups )
                {
                    // Get split group name and vertex indices.
                    const FString & SplitGroupName = IterGroups.Key();
                    TArray< int32 > & SplitGroupVertexList = IterGroups.Value();

                    // Get valid count of vertex indices for this split.
                    int32 SplitGroupVertexListCount = GroupSplitFaceCounts[ SplitGroupName ];

                    // Get face indices for this split.
                    TArray< int32 > & SplitGroupFaceIndices = GroupSplitFaceIndices[ SplitGroupName ];

                    // Record split id in geo part.
                    HoudiniGeoPartObject.SplitId = SplitId;

                    // Reset collision flags.
                    HoudiniGeoPartObject.bIsRenderCollidable = false;
                    HoudiniGeoPartObject.bIsCollidable = false;
                    HoudiniGeoPartObject.bIsSimpleCollisionGeo = false;
                    HoudiniGeoPartObject.bIsUCXCollisionGeo = false;
                    HoudiniGeoPartObject.bHasCollisionBeenAdded = false;
                    HoudiniGeoPartObject.bHasSocketBeenAdded = false;

                    // Increment split id.
                    SplitId++;

                    // Determining the type of collision:
                    // UCX and simple collisions need to be checked first as they both start in the same way
                    // as their non UCX/non simple equivalent!}
                    if ( !HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        HoudiniGeoPartObject.bIsUCXCollisionGeo = true;
                    }
                    else if ( !HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        HoudiniGeoPartObject.bIsRenderCollidable = true;
                        HoudiniGeoPartObject.bIsUCXCollisionGeo = true;
                    }
                    else if ( !HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        HoudiniGeoPartObject.bIsRenderCollidable = true;
                        HoudiniGeoPartObject.bIsSimpleCollisionGeo = true;
                    }
                    else if ( !HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        HoudiniGeoPartObject.bIsCollidable = true;
                        HoudiniGeoPartObject.bIsSimpleCollisionGeo = true;
                    }
                    else if ( !HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        HoudiniGeoPartObject.bIsRenderCollidable = true;
                    }
                    else if ( !HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->CollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        HoudiniGeoPartObject.bIsCollidable = true;
                    }

                    // Boolean used to avoid asking the Positions to HAPI twice if we have a rendered collision UCX
                    bool bAlreadyCalledGetPositions = false;

                    // Handling UCX colliders
                    if ( HoudiniGeoPartObject.bIsUCXCollisionGeo )
                    {
                        bool bCollisionCreated = false;

                        // CONVEX HULL
                        // We need to retrieve the vertices positions
                        HAPI_AttributeInfo AttribInfoPositions;
                        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoPositions, 0 );

                        // Retrieve position data.
                        if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.id, GeoInfo.id,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions ) )
                        {
                            // Error retrieving positions.
                            HOUDINI_LOG_MESSAGE(
                                TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data ")
                                TEXT("- skipping."),
                                ObjectInfo.nodeId, *ObjectName, GeoIdx, PartIdx, *PartName);

                            break;
                        }

                        bAlreadyCalledGetPositions = true;

                        // We're only interested in the unique vertices
                        TArray<int32> UniqueVertexIndexes;
                        for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++ )
                        {
                            int32 Index = SplitGroupVertexList[VertexIdx];
                            if ( Index < 0 || (Index >= Positions.Num() ) )
                                continue;

                            UniqueVertexIndexes.AddUnique( Index );
                        }
                            
                        // Extract the collision geo's vertices
                        TArray< FVector > VertexArray;
                        VertexArray.SetNum(UniqueVertexIndexes.Num());
                        for ( int32 Idx = 0; Idx < UniqueVertexIndexes.Num(); Idx++ )
                        {
                            int32 VertexIndex = UniqueVertexIndexes[Idx];

                            VertexArray[Idx].X = Positions[VertexIndex * 3 + 0] * GeneratedGeometryScaleFactor;
                            if ( ImportAxis == HRSAI_Unreal )
                            {
                                VertexArray[Idx].Y = Positions[VertexIndex * 3 + 2] * GeneratedGeometryScaleFactor;
                                VertexArray[Idx].Z = Positions[VertexIndex * 3 + 1] * GeneratedGeometryScaleFactor;
                            }
                            else
                            {
                                VertexArray[Idx].Y = Positions[VertexIndex * 3 + 1] * GeneratedGeometryScaleFactor;
                                VertexArray[Idx].Z = Positions[VertexIndex * 3 + 2] * GeneratedGeometryScaleFactor;
                            }
                        }

                        // Creating Convex collision
                        FKConvexElem ConvexCollision;
                        ConvexCollision.VertexData = VertexArray;
                        ConvexCollision.UpdateElemBox();

                        AggregateCollisionGeo.ConvexElems.Add( ConvexCollision );

                        bCollisionCreated = true;

                        // We'll add the collision after all the meshes are generated
                        // unless this a rendered_collision_geo_ucx
                        if( bCollisionCreated )
                            bHasAggregateGeometryCollision = true;

                        if ( !HoudiniGeoPartObject.bIsRenderCollidable )
                            continue;
                    }

                    // Record split group name.
                    HoudiniGeoPartObject.SplitName = SplitGroupName;

                    // Attempt to locate static mesh from previous instantiation.
                    UStaticMesh * const * FoundStaticMesh = StaticMeshesIn.Find( HoudiniGeoPartObject );

                    // Flag whether we need to rebuild the mesh.
                    bool bRebuildStaticMesh = false;

                    // See if the geometry and scaling factor have changed. 
                    // If not, then we can reuse the corresponding static mesh.
                    if ( GeoInfo.hasGeoChanged || !HoudiniAssetComponent->CheckGlobalSettingScaleFactors() )
                        bRebuildStaticMesh = true;

                    // If the user asked for a cook manually, we will need to rebuild the static mesh
                    if ( HoudiniAssetComponent->bManualRecook )
                        bRebuildStaticMesh = true;

                    if ( !bRebuildStaticMesh )
                    {
                        // If geometry has not changed.
                        if ( FoundStaticMesh )
                        {
                            StaticMesh = *FoundStaticMesh;

                            // If any of the materials on corresponding geo part object have not changed.
                            if ( !bMaterialsChanged )
                            {
                                // We can reuse previously created geometry.
                                StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );
                                continue;
                            }
                        }
                        else
                        {
                            // No mesh located, this is an error.
                            HOUDINI_LOG_MESSAGE(
                                TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] geometry has changed " )
                                TEXT( "but static mesh does not exist - skipping." ),
                                ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                            continue;
                        }
                    }

                    // If static mesh was not located, we need to create one.
                    bool bStaticMeshCreated = false;
                    if ( !FoundStaticMesh || *FoundStaticMesh == nullptr )
                    {
                        MeshGuid.Invalidate();

                        UPackage * MeshPackage = FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(
                            HoudiniAssetComponent, HoudiniGeoPartObject, MeshName, MeshGuid, FHoudiniEngineUtils::EBakeMode::Intermediate );
                        if( !MeshPackage )
                            continue;

                        StaticMesh = NewObject< UStaticMesh >(
                            MeshPackage, FName( *MeshName ),
                            RF_Transactional );

                        // Add meta information to this package.
                        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                            MeshPackage, MeshPackage,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
                        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                            MeshPackage, MeshPackage,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName );

                        // Notify system that new asset has been created.
                        FAssetRegistryModule::AssetCreated( StaticMesh );

                        bStaticMeshCreated = true;
                    }                    
                    else
                    {
                        // If it was located, we will just reuse it.
                        StaticMesh = *FoundStaticMesh;
                    }

                    // Create new source model for current static mesh.
                    if ( !StaticMesh->SourceModels.Num() )
                        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

                    // Grab current source model.
                    FStaticMeshSourceModel * SrcModel = &StaticMesh->SourceModels[ 0 ];

                    // Load existing raw model. This will be empty as we are constructing a new mesh.
                    FRawMesh RawMesh;

                    // Compute number of faces.
                    int32 FaceCount = SplitGroupFaceIndices.Num();

                    // Reset Face materials.
                    FaceMaterials.Empty();

                    // Attributes we are interested in.
                    HAPI_AttributeInfo AttribInfoPositions;
                    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoPositions, 0 );
                    HAPI_AttributeInfo AttribLightmapResolution;
                    FMemory::Memset< HAPI_AttributeInfo >( AttribLightmapResolution, 0 );
                    HAPI_AttributeInfo AttribFaceMaterials;
                    FMemory::Memset< HAPI_AttributeInfo >( AttribFaceMaterials, 0 );
                    HAPI_AttributeInfo AttribInfoColors;
                    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoColors, 0 );
                    HAPI_AttributeInfo AttribInfoNormals;
                    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNormals, 0 );
                    HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;
                    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoFaceSmoothingMasks, 0 );
                    HAPI_AttributeInfo AttribInfoUVs[ MAX_STATIC_TEXCOORDS ];
                    FMemory::Memset( &AttribInfoUVs[ 0 ], 0, sizeof( HAPI_AttributeInfo ) * MAX_STATIC_TEXCOORDS );

                    if ( bRebuildStaticMesh )
                    {
                        if ( !bAlreadyCalledGetPositions )

                        {
                            // Retrieve position data.
                            if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.id, GeoInfo.id,
                                PartInfo.id, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions ) )
                            {
                                // Error retrieving positions.
                                HOUDINI_LOG_MESSAGE(
                                    TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data " )
                                    TEXT( "- skipping." ),
                                    ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );

                                if ( bStaticMeshCreated )
                                    StaticMesh->MarkPendingKill();

                                break;
                            }
                        }

                        // Get lightmap resolution (if present).
                        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                            AssetId, ObjectInfo.id, GeoInfo.id,
                            PartInfo.id, MarshallingAttributeNameLightmapResolution.c_str(),
                            AttribLightmapResolution, LightMapResolutions );

                        // Get name of attribute used for marshalling materials.
                        {
                            FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                                AssetId, ObjectInfo.id, GeoInfo.id,
                                PartInfo.id, MarshallingAttributeNameMaterial.c_str(),
                                AttribFaceMaterials, FaceMaterials );

                            // If material attribute was not found, check fallback compatibility attribute.
                            if ( !AttribFaceMaterials.exists )
                            {
                                FaceMaterials.Empty();
                                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                                    AssetId, ObjectInfo.id, GeoInfo.id,
                                    PartInfo.id, MarshallingAttributeNameMaterialFallback.c_str(),
                                    AttribFaceMaterials, FaceMaterials );
                            }

                            if ( AttribFaceMaterials.exists && AttribFaceMaterials.owner != HAPI_ATTROWNER_PRIM && AttribFaceMaterials.owner != HAPI_ATTROWNER_DETAIL )
                            {
                                HOUDINI_LOG_WARNING( TEXT( "Static Mesh [%d %s], Geo [%d], Part [%d %s]: unreal_material must be a primitive or detail attribute, ignoring attribute." ),
                                    ObjectInfo.nodeId, *ObjectName, GeoIdx, PartIdx, *PartName);
                                AttribFaceMaterials.exists = false;
                                FaceMaterials.Empty();
                            }
                        }

                        // Retrieve color data.
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.id, GeoInfo.id,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_COLOR, AttribInfoColors, Colors );

                        // See if we need to transfer color point attributes to vertex attributes.
                        FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                            SplitGroupVertexList,
                            AttribInfoColors, Colors );

                        // Retrieve normal data.
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.id, GeoInfo.id,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals );

                        // Retrieve face smoothing data.
                        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                            AssetId, ObjectInfo.id, GeoInfo.id,
                            PartInfo.id, MarshallingAttributeNameFaceSmoothingMask.c_str(),
                            AttribInfoFaceSmoothingMasks, FaceSmoothingMasks );

                        // See if we need to transfer normal point attributes to vertex attributes.
                        FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                            SplitGroupVertexList, AttribInfoNormals, Normals );

                        // Retrieve UVs.
                        for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                        {
                            std::string UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

                            if ( TexCoordIdx > 0 )
                                UVAttributeName += std::to_string( TexCoordIdx + 1 );

                            const char * UVAttributeNameString = UVAttributeName.c_str();
                            FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, UVAttributeNameString,
                                AttribInfoUVs[ TexCoordIdx ], TextureCoordinates[ TexCoordIdx ], 2 );

                            // See if we need to transfer uv point attributes to vertex attributes.
                            FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                                SplitGroupVertexList, AttribInfoUVs[ TexCoordIdx ], TextureCoordinates[ TexCoordIdx ] );
                        }

                        // We can transfer attributes to raw mesh.

                        // Set face smoothing masks.
                        {
                            RawMesh.FaceSmoothingMasks.SetNumZeroed( FaceCount );

                            if ( FaceSmoothingMasks.Num() )
                            {
                                int32 ValidFaceIdx = 0;
                                for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3 )
                                {
                                    int32 WedgeCheck = SplitGroupVertexList[ VertexIdx + 0 ];
                                    if ( WedgeCheck == -1 )
                                        continue;

                                    RawMesh.FaceSmoothingMasks[ ValidFaceIdx ] = FaceSmoothingMasks[ VertexIdx / 3 ];
                                    ValidFaceIdx++;
                                }
                            }
                        }

                        // Transfer UVs.
                        int32 UVChannelCount = 0;
                        int32 FirstUVChannelIndex = -1;
                        for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                        {
                            TArray< float > & TextureCoordinate = TextureCoordinates[ TexCoordIdx ];
                            if ( TextureCoordinate.Num() > 0 )
                            {
                                int32 WedgeUVCount = TextureCoordinate.Num() / 2;
                                RawMesh.WedgeTexCoords[ TexCoordIdx ].SetNumZeroed( WedgeUVCount );
                                for ( int32 WedgeUVIdx = 0; WedgeUVIdx < WedgeUVCount; ++WedgeUVIdx )
                                {
                                    // We need to flip V coordinate when it's coming from HAPI.
                                    FVector2D WedgeUV;
                                    WedgeUV.X = TextureCoordinate[ WedgeUVIdx * 2 + 0 ];
                                    WedgeUV.Y = 1.0f - TextureCoordinate[ WedgeUVIdx * 2 + 1 ];

                                    RawMesh.WedgeTexCoords[ TexCoordIdx ][ WedgeUVIdx ] = WedgeUV;
                                }

                                UVChannelCount++;
                                if ( FirstUVChannelIndex == -1 )
                                    FirstUVChannelIndex = TexCoordIdx;
                            }
                            else
                            {
                                RawMesh.WedgeTexCoords[ TexCoordIdx ].Empty();
                            }
                        }

                        switch ( UVChannelCount )
                        {
                            case 0:
                            {
                                // We have to have at least one UV channel. If there's none, create one with zero data.
                                RawMesh.WedgeTexCoords[ 0 ].SetNumZeroed( SplitGroupVertexListCount );
                                StaticMesh->LightMapCoordinateIndex = 0;

                                break;
                            }

                            case 1:
                            {
                                // We have only one UV channel.
                                StaticMesh->LightMapCoordinateIndex = FirstUVChannelIndex;

                                break;
                            }

                            default:
                            {
                                // We have more than one channel, by convention use 2nd set for lightmaps.
                                StaticMesh->LightMapCoordinateIndex = 1;

                                break;
                            }
                        }

                        // See if we need to generate tangents, we do this only if normals are present.
                        bool bGenerateTangents = ( Normals.Num() > 0 );

                        // Transfer normals.
                        int32 WedgeNormalCount = Normals.Num() / 3;
                        RawMesh.WedgeTangentZ.SetNumZeroed( WedgeNormalCount );
                        for ( int32 WedgeTangentZIdx = 0; WedgeTangentZIdx < WedgeNormalCount; ++WedgeTangentZIdx )
                        {
                            FVector WedgeTangentZ;

                            WedgeTangentZ.X = Normals[ WedgeTangentZIdx * 3 + 0 ];
                            WedgeTangentZ.Y = Normals[ WedgeTangentZIdx * 3 + 1 ];
                            WedgeTangentZ.Z = Normals[ WedgeTangentZIdx * 3 + 2 ];

                            if ( ImportAxis == HRSAI_Unreal )
                            {
                                // We need to flip Z and Y coordinate here.
                                Swap( WedgeTangentZ.Y, WedgeTangentZ.Z );
                                RawMesh.WedgeTangentZ[ WedgeTangentZIdx ] = WedgeTangentZ;
                            }
                            else if( ImportAxis == HRSAI_Houdini )
                            {
                                // Do nothing in this case.
                            }
                            else
                            {
                                // Not valid enum value.
                                check( 0 );
                            }

                            // If we need to generate tangents.
                            if ( bGenerateTangents )
                            {
                                FVector TangentX, TangentY;
                                WedgeTangentZ.FindBestAxisVectors( TangentX, TangentY );

                                RawMesh.WedgeTangentX.Add( TangentX );
                                RawMesh.WedgeTangentY.Add( TangentY );
                            }
                        }

                        // Transfer colors.
                        if ( AttribInfoColors.exists && AttribInfoColors.tupleSize )
                        {
                            int32 WedgeColorsCount = Colors.Num() / AttribInfoColors.tupleSize;
                            RawMesh.WedgeColors.SetNumZeroed( WedgeColorsCount );
                            for ( int32 WedgeColorIdx = 0; WedgeColorIdx < WedgeColorsCount; ++WedgeColorIdx )
                            {
                                FLinearColor WedgeColor;

                                WedgeColor.R = FMath::Clamp(
                                    Colors[ WedgeColorIdx * AttribInfoColors.tupleSize + 0 ], 0.0f, 1.0f );
                                WedgeColor.G = FMath::Clamp(
                                    Colors[ WedgeColorIdx * AttribInfoColors.tupleSize + 1 ], 0.0f, 1.0f );
                                WedgeColor.B = FMath::Clamp(
                                    Colors[ WedgeColorIdx * AttribInfoColors.tupleSize + 2 ], 0.0f, 1.0f );

                                if ( AttribInfoColors.tupleSize == 4 )
                                {
                                    // We have alpha.
                                    WedgeColor.A = FMath::Clamp(
                                        Colors [WedgeColorIdx * AttribInfoColors.tupleSize + 3 ], 0.0f, 1.0f );
                                }
                                else
                                {
                                    WedgeColor.A = 1.0f;
                                }

                                // Convert linear color to fixed color.
                                RawMesh.WedgeColors[ WedgeColorIdx ] = WedgeColor.ToFColor( false );
                            }
                        }
                        else
                        {
                            FColor DefaultWedgeColor = FLinearColor::White.ToFColor( false );

                            int32 WedgeColorsCount = RawMesh.WedgeIndices.Num();
                            if ( WedgeColorsCount > 0 )
                            {
                                RawMesh.WedgeColors.SetNumZeroed( WedgeColorsCount );

                                for ( int32 WedgeColorIdx = 0; WedgeColorIdx < WedgeColorsCount; ++WedgeColorIdx )
                                    RawMesh.WedgeColors[ WedgeColorIdx ] = DefaultWedgeColor;
                            }
                        }

                        // Transfer indices.
                        RawMesh.WedgeIndices.SetNumZeroed( SplitGroupVertexListCount );
                        int32 ValidVertexId = 0;
                        for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3 )
                        {
                            int32 WedgeCheck = SplitGroupVertexList[ VertexIdx + 0 ];
                            if ( WedgeCheck == -1 )
                                continue;

                            int32 WedgeIndices[ 3 ] = {
                                SplitGroupVertexList[ VertexIdx + 0 ],
                                SplitGroupVertexList[ VertexIdx + 1 ],
                                SplitGroupVertexList[ VertexIdx + 2 ]
                            };

                            if ( ImportAxis == HRSAI_Unreal )
                            {
                                // Flip wedge indices to fix winding order.
                                RawMesh.WedgeIndices[ ValidVertexId + 0 ] = WedgeIndices[ 0 ];
                                RawMesh.WedgeIndices[ ValidVertexId + 1 ] = WedgeIndices[ 2 ];
                                RawMesh.WedgeIndices[ ValidVertexId + 2 ] = WedgeIndices[ 1 ];

                                // Check if we need to patch UVs.
                                for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                                {
                                    if ( RawMesh.WedgeTexCoords[ TexCoordIdx ].Num() > 0
                                        && ( (ValidVertexId + 2) < RawMesh.WedgeTexCoords[TexCoordIdx].Num() ) )
                                    {
                                        Swap( RawMesh.WedgeTexCoords[ TexCoordIdx ][ ValidVertexId + 1 ],
                                            RawMesh.WedgeTexCoords[ TexCoordIdx ][ ValidVertexId + 2 ] );
                                    }
                                }

                                // Check if we need to patch colors.
                                if ( RawMesh.WedgeColors.Num() > 0 )
                                    Swap( RawMesh.WedgeColors[ ValidVertexId + 1 ], RawMesh.WedgeColors[ ValidVertexId + 2 ] );

                                // Check if we need to patch tangents.
                                if ( RawMesh.WedgeTangentZ.Num() > 0 )
                                    Swap( RawMesh.WedgeTangentZ[ ValidVertexId + 1 ], RawMesh.WedgeTangentZ[ ValidVertexId + 2 ] );

                                /*
                                if ( RawMesh.WedgeTangentX.Num() > 0 )
                                    Swap( RawMesh.WedgeTangentX[ ValidVertexId + 1 ], RawMesh.WedgeTangentX[ ValidVertexId + 2 ] );

                                if ( RawMesh.WedgeTangentY.Num() > 0 )
                                    Swap ( RawMesh.WedgeTangentY[ ValidVertexId + 1 ], RawMesh.WedgeTangentY[ ValidVertexId + 2 ] );
                                */
                            }
                            else if ( ImportAxis == HRSAI_Houdini )
                            {
                                // Flip wedge indices to fix winding order.
                                RawMesh.WedgeIndices[ ValidVertexId + 0 ] = WedgeIndices[ 0 ];
                                RawMesh.WedgeIndices[ ValidVertexId + 1 ] = WedgeIndices[ 1 ];
                                RawMesh.WedgeIndices[ ValidVertexId + 2 ] = WedgeIndices[ 2 ];
                            }
                            else
                            {
                                // Not valid enum value.
                                check( 0 );
                            }

                            ValidVertexId += 3;
                        }

                        // Transfer vertex positions.
                        int32 VertexPositionsCount = Positions.Num() / 3;
                        RawMesh.VertexPositions.SetNumZeroed( VertexPositionsCount );
                        for ( int32 VertexPositionIdx = 0; VertexPositionIdx < VertexPositionsCount; ++VertexPositionIdx )
                        {
                            FVector VertexPosition;
                            if ( ImportAxis == HRSAI_Unreal )
                            {
                                // We need to swap Z and Y coordinate here.
                                VertexPosition.X = Positions[ VertexPositionIdx * 3 + 0 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Y = Positions[ VertexPositionIdx * 3 + 2 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Z = Positions[ VertexPositionIdx * 3 + 1 ] * GeneratedGeometryScaleFactor;
                            }
                            else if ( ImportAxis == HRSAI_Houdini )
                            {
                                // No swap required.
                                VertexPosition.X = Positions[ VertexPositionIdx * 3 + 0 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Y = Positions[ VertexPositionIdx * 3 + 1 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Z = Positions[ VertexPositionIdx * 3 + 2 ] * GeneratedGeometryScaleFactor;
                            }
                            else
                            {
                                // Not valid enum value.
                                check( 0 );
                            }

                            RawMesh.VertexPositions[ VertexPositionIdx ] = VertexPosition;
                        }

                        // We need to check if this mesh contains only degenerate triangles.
                        if ( FHoudiniEngineUtils::CountDegenerateTriangles( RawMesh ) == FaceCount )
                        {
                            // This mesh contains only degenerate triangles, there's nothing we can do.
                            if ( bStaticMeshCreated )
                                StaticMesh->MarkPendingKill();

                            continue;
                        }
                    }
                    else
                    {
                        // Otherwise we'll just load old data into Raw mesh and reuse it.
                        FRawMeshBulkData * InRawMeshBulkData = SrcModel->RawMeshBulkData;
                        InRawMeshBulkData->LoadRawMesh( RawMesh );
                    }

                    // Process material replacements first.
                    bool bMissingReplacement = false;
                    bool bMaterialsReplaced = false;

                    if ( FaceMaterials.Num() > 0 )
                    {
                        // If material name was assigned per detail we replicate it for each primitive.
                        if ( AttribFaceMaterials.owner == HAPI_ATTROWNER_DETAIL )
                        {
                            FString SingleFaceMaterial = FaceMaterials[ 0 ];
                            FaceMaterials.Init( SingleFaceMaterial, SplitGroupVertexList.Num() / 3 );
                        }

                        StaticMesh->StaticMaterials.Empty();
                        RawMesh.FaceMaterialIndices.SetNumZeroed( FaceCount );

                        TMap< FString, int32 > FaceMaterialMap;
                        int32 UniqueMaterialIdx = 0;
                        int32 FaceIdx = 0;

                        for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3 )
                        {
                            int32 WedgeCheck = SplitGroupVertexList[ VertexIdx + 0 ];
                            if ( WedgeCheck == -1 )
                                continue;

                            const FString & MaterialName = FaceMaterials[ VertexIdx / 3 ];
                            int32 const * FoundFaceMaterialIdx = FaceMaterialMap.Find( MaterialName );
                            int32 CurrentFaceMaterialIdx = 0;
                            if ( FoundFaceMaterialIdx )
                            {
                                CurrentFaceMaterialIdx = *FoundFaceMaterialIdx;
                            }
                            else
                            {
                                UMaterialInterface * MaterialInterface = Cast< UMaterialInterface >(
                                    StaticLoadObject(
                                        UMaterialInterface::StaticClass(),
                                        nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr ) );

                                if ( MaterialInterface )
                                {
                                    // Make sure this material is in the assignemets before replacing it.
                                    if ( !HoudiniAssetComponent->GetAssignmentMaterial(  MaterialInterface->GetName() ) )
                                        HoudiniAssetComponent->HoudiniAssetComponentMaterials->Assignments.Add( MaterialInterface->GetName(), MaterialInterface );

                                    // See if we have a replacement material for this.
                                    UMaterialInterface * ReplacementMaterialInterface = HoudiniAssetComponent->GetReplacementMaterial( HoudiniGeoPartObject, MaterialInterface->GetName() );
                                    if ( ReplacementMaterialInterface )
                                        MaterialInterface = ReplacementMaterialInterface;
                                }

                                if ( !MaterialInterface )
                                {
                                    // Material/Replacement does not exist, use default material.
                                    MaterialInterface = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
                                    bMissingReplacement = true;
                                }

                                StaticMesh->StaticMaterials.Add( FStaticMaterial(MaterialInterface) );
                                FaceMaterialMap.Add( MaterialName, UniqueMaterialIdx );
                                CurrentFaceMaterialIdx = UniqueMaterialIdx;
                                UniqueMaterialIdx++;
                            }

                            RawMesh.FaceMaterialIndices[ FaceIdx ] = CurrentFaceMaterialIdx;
                            FaceIdx++;
                        }

                        bMaterialsReplaced = true;
                    }

                    if ( bMaterialsReplaced )
                    {
                        // We want to fallback to supplied material only if replacement occurred and there was an issue in the process.
                        if ( bMaterialsFound && bMissingReplacement )
                        {
                            // Get default Houdini material.
                            UMaterial * MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

                            // Extract native Houdini materials.
                            TMap< HAPI_MaterialId, UMaterialInterface * > NativeMaterials;

                            for(int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
                            {
                                // Get material id for this face.
                                HAPI_MaterialId MaterialId = FaceMaterialIds[ FaceIdx ];
                                UMaterialInterface * Material = MaterialDefault;

                                FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                                FHoudiniEngineUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName );
                                UMaterialInterface * const * FoundMaterial = Materials.Find( MaterialShopName );

                                if ( FoundMaterial )
                                    Material = *FoundMaterial;

                                // If we have replacement material for this geo part object and this shop material name.
                                UMaterialInterface * ReplacementMaterial =
                                    HoudiniAssetComponent->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                                if ( ReplacementMaterial )
                                    Material = ReplacementMaterial;

                                // See if this material has been added.
                                UMaterialInterface * const * FoundNativeMaterial = NativeMaterials.Find( MaterialId );
                                if ( !FoundNativeMaterial )
                                    NativeMaterials.Add( MaterialId, Material );
                            }

                            for ( int32 FaceIdx = 0; FaceIdx < RawMesh.FaceMaterialIndices.Num(); ++FaceIdx )
                            {
                                int32 MaterialIdx = RawMesh.FaceMaterialIndices[ FaceIdx ];
                                auto FaceMaterial = StaticMesh->StaticMaterials[ MaterialIdx ];

                                if ( FaceMaterial.MaterialInterface == MaterialDefault )
                                {
                                    HAPI_MaterialId MaterialId = FaceMaterialIds[ FaceIdx ];
                                    if ( MaterialId >= 0 )
                                    {
                                        UMaterialInterface * const * FoundNativeMaterial = NativeMaterials.Find( MaterialId );
                                        if ( FoundNativeMaterial )
                                            StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface = *FoundNativeMaterial;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if ( bMaterialsFound )
                        {
                            if ( bSingleFaceMaterial )
                            {
                                // Use default Houdini material if no valid material is assigned to any of the faces.
                                UMaterialInterface * Material = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

                                // We have only one material.
                                RawMesh.FaceMaterialIndices.SetNumZeroed( FaceCount );

                                // Get id of this single material.
                                FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                                FHoudiniEngineUtils::GetUniqueMaterialShopName( AssetId, FaceMaterialIds[ 0 ], MaterialShopName );
                                UMaterialInterface * const * FoundMaterial = Materials.Find( MaterialShopName );

                                if ( FoundMaterial )
                                    Material = *FoundMaterial;

                                // If we have replacement material for this geo part object and this shop material name.
                                UMaterialInterface * ReplacementMaterial =
                                    HoudiniAssetComponent->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                                if ( ReplacementMaterial )
                                    Material = ReplacementMaterial;

                                StaticMesh->StaticMaterials.Empty();
                                StaticMesh->StaticMaterials.Add( FStaticMaterial(Material) );
                            }
                            else
                            {
                                // Get default Houdini material.
                                UMaterial * MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

                                // We have multiple materials.
                                int32 MaterialIndex = 0;
                                TMap< UMaterialInterface *, int32 > MappedMaterials;
                                TArray< UMaterialInterface * > MappedMaterialsList;

                                // Reset Rawmesh material face assignments.
                                RawMesh.FaceMaterialIndices.SetNumZeroed( SplitGroupFaceIndices.Num() );

                                for ( int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx )
                                {
                                    int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];

                                    // Get material id for this face.
                                    HAPI_MaterialId MaterialId = -1; 
                                    if( SplitFaceIndex >= 0 && SplitFaceIndex < FaceMaterialIds.Num() )
                                        MaterialId = FaceMaterialIds[ SplitFaceIndex ];

                                    UMaterialInterface * Material = MaterialDefault;

                                    FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                                    FHoudiniEngineUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName );
                                    UMaterialInterface * const * FoundMaterial = Materials.Find( MaterialShopName );

                                    if ( FoundMaterial )
                                        Material = *FoundMaterial;

                                    // If we have replacement material for this geo part object and this shop material name.
                                    UMaterialInterface * ReplacementMaterial =
                                        HoudiniAssetComponent->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                                    if ( ReplacementMaterial )
                                        Material = ReplacementMaterial;

                                    // See if this material has been added.
                                    int32 const * FoundMaterialIdx = MappedMaterials.Find( Material );
                                    if ( FoundMaterialIdx )
                                    {
                                        // This material has been mapped already.
                                        RawMesh.FaceMaterialIndices[ FaceIdx ] = *FoundMaterialIdx;
                                    }
                                    else
                                    {
                                        // This is the first time we've seen this material.
                                        MappedMaterials.Add( Material, MaterialIndex );
                                        MappedMaterialsList.Add( Material );

                                        RawMesh.FaceMaterialIndices[ FaceIdx ] = MaterialIndex;

                                        MaterialIndex++;
                                    }
                                }

                                StaticMesh->StaticMaterials.Empty();

                                for ( int32 MaterialIdx = 0; MaterialIdx < MappedMaterialsList.Num(); ++MaterialIdx )
                                    StaticMesh->StaticMaterials.Add( FStaticMaterial(MappedMaterialsList[ MaterialIdx ]) );
                            }
                        }
                        else
                        {
                            // No materials were found, we need to use default Houdini material.
                            RawMesh.FaceMaterialIndices.SetNumZeroed( FaceCount );

                            UMaterialInterface * Material = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
                            FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;

                            // If we have replacement material for this geo part object and this shop material name.
                            UMaterialInterface * ReplacementMaterial =
                                HoudiniAssetComponent->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                            if ( ReplacementMaterial )
                                Material = ReplacementMaterial;

                            StaticMesh->StaticMaterials.Empty();
                            StaticMesh->StaticMaterials.Add( FStaticMaterial(Material) );
                        }
                    }

                    // Some mesh generation settings.
                    HoudiniRuntimeSettings->SetMeshBuildSettings( SrcModel->BuildSettings, RawMesh );

                    // By default the distance field resolution should be set to 2.0
                    SrcModel->BuildSettings.DistanceFieldResolutionScale = HoudiniAssetComponent->GeneratedDistanceFieldResolutionScale;

                    // We need to check light map uv set for correctness. Unreal seems to have occasional issues with
                    // zero UV sets when building lightmaps.
                    if ( SrcModel->BuildSettings.bGenerateLightmapUVs )
                    {
                        // See if we need to disable lightmap generation because of bad UVs.
                        if ( FHoudiniEngineUtils::ContainsInvalidLightmapFaces( RawMesh, StaticMesh->LightMapCoordinateIndex ) )
                        {
                            SrcModel->BuildSettings.bGenerateLightmapUVs = false;

                            HOUDINI_LOG_MESSAGE(
                                TEXT( "Skipping Lightmap Generation: Object [%d %s], Geo [%d], Part [%d %s] invalid face detected " )
                                TEXT( "- skipping." ),
                                ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName );
                        }
                    }

                    // Store the new raw mesh.
                    SrcModel->RawMeshBulkData->SaveRawMesh( RawMesh );

                    while( StaticMesh->SourceModels.Num() < NumLODs )
                        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

                    for ( int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex )
                    {
                        StaticMesh->SourceModels[ ModelLODIndex ].ReductionSettings =
                            LODGroup.GetDefaultSettings( ModelLODIndex );

                        for ( int32 MaterialIndex = 0; MaterialIndex < StaticMesh->StaticMaterials.Num(); ++MaterialIndex )
                        {
                            FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get( ModelLODIndex, MaterialIndex );
                            Info.MaterialIndex = MaterialIndex;
                            Info.bEnableCollision = true;
                            Info.bCastShadow = true;
                            StaticMesh->SectionInfoMap.Set( ModelLODIndex, MaterialIndex, Info );
                        }
                    }

                    // Assign generation parameters for this static mesh.
                    HoudiniAssetComponent->SetStaticMeshGenerationParameters( StaticMesh );

                    // If we have an override for lightmap resolution.
                    if ( LightMapResolutions.Num() > 0 )
                    {
                        int32 LightMapResolutionOverride = LightMapResolutions[ 0 ];
                        if ( LightMapResolutionOverride > 0 )
                            StaticMesh->LightMapResolution = LightMapResolutionOverride;
                    }

                    // Make sure we remove the old simple colliders if needed
                    UBodySetup * BodySetup = StaticMesh->BodySetup;
                    check(BodySetup);

                    if ( !HoudiniGeoPartObject.bHasCollisionBeenAdded )
                    {
                        BodySetup->RemoveSimpleCollision();
                    }

                    // See if we need to enable collisions on the whole mesh.
                    if ( ( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
                        && ( !HoudiniGeoPartObject.bIsSimpleCollisionGeo && !bIsUCXCollidable ) )
                    {
                        // Enable collisions for this static mesh.
                        BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
                    }

                    // Free any RHI resources.
                    StaticMesh->PreEditChange( nullptr );

                    FHoudiniScopedGlobalSilence ScopedGlobalSilence;

                    TArray<FText> BuildErrors;
                    StaticMesh->Build( true, &BuildErrors );

                    for ( int32 BuildErrorIdx = 0; BuildErrorIdx < BuildErrors.Num(); ++BuildErrorIdx )
                    {
                        const FText& TextError = BuildErrors[BuildErrorIdx];
                        HOUDINI_LOG_MESSAGE(
                            TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d] build error " )
                            TEXT( "- %s." ),
                            ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName, SplitId, *( TextError.ToString() ) );
                    }

                    // Do we want to add simple collisions ?
                    bool bAddSimpleCollisions = false;
                    if (!HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        bAddSimpleCollisions = true;
                        // This geo part will have to be considered as a collision geo
                        HoudiniGeoPartObject.bIsCollidable = true;
                    }
                    else if(!HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix.IsEmpty() &&
                        SplitGroupName.StartsWith(
                            HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix,
                            ESearchCase::IgnoreCase ) )
                    {
                        bAddSimpleCollisions = true;
                        // This geo part will have to be considered as a rendered collision
                        HoudiniGeoPartObject.bIsRenderCollidable = true;
                    }

                    if ( bAddSimpleCollisions )
                    {
                        int32 PrimIndex = INDEX_NONE;
                        if ( SplitGroupName.Contains( "Box" ) )
                        {
                            PrimIndex = GenerateBoxAsSimpleCollision( StaticMesh );
                        }
                        else if ( SplitGroupName.Contains( "Sphere" ) )
                        {
                            PrimIndex = GenerateSphereAsSimpleCollision( StaticMesh );
                        }
                        else if ( SplitGroupName.Contains( "Capsule" ) )
                        {
                            PrimIndex = GenerateSphylAsSimpleCollision( StaticMesh );
                        }
                        else
                        {
                            // We need to see what type of collision the user wants
                            // by default, a kdop26 will be created
                            uint32 NumDirections = 26;
                            const FVector* Directions = KDopDir26;

                            if ( SplitGroupName.Contains( "kdop10X" ) )
                            {
                                NumDirections = 10;
                                Directions = KDopDir10X;
                            }
                            else if (SplitGroupName.Contains( "kdop10Y" ) )
                            {
                                NumDirections = 10;
                                Directions = KDopDir10Y;
                            }
                            else if (SplitGroupName.Contains( "kdop10Z" ) )
                            {
                                NumDirections = 10;
                                Directions = KDopDir10Z;
                            }
                            else if (SplitGroupName.Contains( "kdop18" ) )
                            {
                                NumDirections = 18;
                                Directions = KDopDir18;
                            }

                            // Converting the directions to a TArray
                            TArray<FVector> DirArray;
                            for ( uint32 DirectionIndex = 0; DirectionIndex < NumDirections; DirectionIndex++ )
                            {
                                DirArray.Add( Directions[DirectionIndex] );
                            }

                            PrimIndex = GenerateKDopAsSimpleCollision( StaticMesh, DirArray );
                        }

                        if ( PrimIndex == INDEX_NONE )
                        {
                            HoudiniGeoPartObject.bIsSimpleCollisionGeo = false;
                            HoudiniGeoPartObject.bIsCollidable = false;
                            HoudiniGeoPartObject.bIsRenderCollidable = false;
                        }
                        else
                        {
                            // We don't want these collisions to be removed
                            HoudiniGeoPartObject.bHasCollisionBeenAdded = true;
                        }
                    }

                    // We need to handle rendered_ucx collisions now
                    if ( HoudiniGeoPartObject.bIsUCXCollisionGeo && HoudiniGeoPartObject.bIsRenderCollidable && bHasAggregateGeometryCollision )
                    {
                        // Add the aggregate collision geo to the static mesh
                        if ( AddAggregateCollisionGeometryToStaticMesh(
                            StaticMesh, HoudiniGeoPartObject, AggregateCollisionGeo ) )
                        {
                            bHasAggregateGeometryCollision = false;
                        }
                    }

                    // Add sockets to the static mesh if neeeded
                    AddMeshSocketsToStaticMesh( StaticMesh, HoudiniGeoPartObject, AllSockets, AllSocketsNames, AllSocketsActors );

                    StaticMesh->MarkPackageDirty();

                    StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );

                } // end for SplitId

            } // end for PartId

            // We need to add the remaining UCX/UBX/Collisions here
            if ( bHasAggregateGeometryCollision )
            {
                // We want to find a StaticMesh for these collisions...
                // As there's no way of telling where we should add these, 
                // We need to find a static mesh for this geo that doesn't have any collision
                // and add the aggregate UCX/UBX/USP geo to its body setup
                UStaticMesh * CollisionStaticMesh = nullptr;
                FHoudiniGeoPartObject * CollisionHoudiniGeoPartObject = nullptr;
                for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter(StaticMeshesOut); Iter; ++Iter )
                {
                    FHoudiniGeoPartObject * HoudiniGeoPartObject = &(Iter.Key());

                    if ( ( HoudiniGeoPartObject->ObjectId != ObjectInfo.nodeId ) && ( HoudiniGeoPartObject->GeoId != GeoInfo.nodeId ) )
                    {
                        // If we haven't find a mesh for the collision, we might as well use this one but
                        // we will keep searching for a better one
                        if ( !CollisionStaticMesh )
                        {
                            CollisionStaticMesh = Iter.Value();
                            CollisionHoudiniGeoPartObject = HoudiniGeoPartObject;
                        }

                        continue;
                    }

                    if ( HoudiniGeoPartObject->IsCollidable() || HoudiniGeoPartObject->IsRenderCollidable() )
                    {
                        // We can add collision to this StaticMesh, but as it already has some.
                        // we'd prefer to find one that has no collision already, so we'll keep searching...
                        CollisionStaticMesh = Iter.Value();
                        CollisionHoudiniGeoPartObject = HoudiniGeoPartObject;
                        continue;
                    }

                    // This mesh is from the same geo, and is not a collision mesh so 
                    // we will add the simple collision to this mesh's body setup
                    CollisionStaticMesh = Iter.Value();
                    CollisionHoudiniGeoPartObject = HoudiniGeoPartObject;
                    break;
                }

                // Add the aggregate collision geo to the static mesh
                if ( AddAggregateCollisionGeometryToStaticMesh(
                StaticMesh, *CollisionHoudiniGeoPartObject, AggregateCollisionGeo ) )
                {
                    bHasAggregateGeometryCollision = false;
                }
            }

            // We still have socket that need to be attached to a StaticMesh...
            if (AllSockets.Num() > 0)
            {
                // We want to find a StaticMesh for these socket...
                // As there's no way of telling where we should add these, 
                // We need to find a static mesh for this geo that is (preferably) visible
                UStaticMesh * SocketStaticMesh = nullptr;
                FHoudiniGeoPartObject * SocketHoudiniGeoPartObject = nullptr;
                for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshesOut ); Iter; ++Iter )
                {
                    FHoudiniGeoPartObject * HoudiniGeoPartObject = &(Iter.Key());

                    if ( ( HoudiniGeoPartObject->ObjectId != ObjectInfo.nodeId ) && ( HoudiniGeoPartObject->GeoId != GeoInfo.nodeId ) )
                    {
                        // If we haven't find a mesh for the socket yet, we might as well use this one but
                        // we will keep searching for a "better" candidate
                        if ( !SocketStaticMesh )
                        {
                            SocketStaticMesh = Iter.Value();
                            SocketHoudiniGeoPartObject = HoudiniGeoPartObject;
                        }

                        continue;
                    }

                    if ( HoudiniGeoPartObject->IsCollidable() )
                    {
                        // This object has the same geo/node id, but won't be visible,
                        // so we'll keep looking for a "better" one
                        SocketStaticMesh = Iter.Value();
                        SocketHoudiniGeoPartObject = HoudiniGeoPartObject;
                        continue;
                    }

                    // This mesh is from the same geo and is visible, we'll add the socket to it..
                    SocketStaticMesh = Iter.Value();
                    SocketHoudiniGeoPartObject = HoudiniGeoPartObject;
                    break;
                }

                // Add socket to the mesh if we found a suitable one
                if ( SocketStaticMesh )
                    AddMeshSocketsToStaticMesh( SocketStaticMesh, *SocketHoudiniGeoPartObject, AllSockets, AllSocketsNames, AllSocketsActors );
            }

        } // end for GeoId

    } // end for ObjectId


    // Now that all the meshes are built and their collisions meshes and primitives updated,
    // we need to update their pre-built navigation collision used by the navmesh
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshesOut ); Iter; ++Iter )
    {
        FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();

        // Only update for collidable objects
        if ( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
        {
            StaticMesh = Iter.Value();
            if ( !StaticMesh )
                continue;

            UBodySetup * BodySetup = StaticMesh->BodySetup;
            check(BodySetup);

            // Unreal caches the Navigation Collision and never updates it for StaticMeshes,
            // so we need to manually flush and recreate the data to have proper navigation collision
            if ( StaticMesh->NavCollision )
            {
                StaticMesh->NavCollision->CookedFormatData.FlushData();
                StaticMesh->NavCollision->GatherCollision();
                StaticMesh->NavCollision->Setup( BodySetup );
            }
        }
    }

#endif

    return true;
}

#if WITH_EDITOR

bool
FHoudiniEngineUtils::ContainsDegenerateTriangles( const FRawMesh & RawMesh )
{
    int32 WedgeCount = RawMesh.WedgeIndices.Num();
    for ( int32 WedgeIdx = 0; WedgeIdx < WedgeCount; WedgeIdx += 3 )
    {
        const FVector & Vertex0 = RawMesh.VertexPositions[ RawMesh.WedgeIndices[ WedgeIdx + 0 ] ];
        const FVector & Vertex1 = RawMesh.VertexPositions[ RawMesh.WedgeIndices[ WedgeIdx + 1 ] ];
        const FVector & Vertex2 = RawMesh.VertexPositions[ RawMesh.WedgeIndices[ WedgeIdx + 2 ] ];

        // Strict equality will not detect properly all the degenerated triangles, we need to use Equals here
        if ( Vertex0.Equals(Vertex1, THRESH_POINTS_ARE_SAME) || Vertex0.Equals(Vertex2, THRESH_POINTS_ARE_SAME) || Vertex1.Equals(Vertex2, THRESH_POINTS_ARE_SAME) )
            return true;
    }

    return false;
}

int32
FHoudiniEngineUtils::CountDegenerateTriangles( const FRawMesh & RawMesh )
{
    int32 DegenerateTriangleCount = 0;
    int32 WedgeCount = RawMesh.WedgeIndices.Num();
    for ( int32 WedgeIdx = 0; WedgeIdx < WedgeCount; WedgeIdx += 3 )
    {
        const FVector & Vertex0 = RawMesh.VertexPositions[ RawMesh.WedgeIndices[ WedgeIdx + 0 ] ];
        const FVector & Vertex1 = RawMesh.VertexPositions[ RawMesh.WedgeIndices[ WedgeIdx + 1 ] ];
        const FVector & Vertex2 = RawMesh.VertexPositions[ RawMesh.WedgeIndices[ WedgeIdx + 2 ] ];

        // Strict equality will not detect properly all the degenerated triangles, we need to use Equals here
        if( Vertex0.Equals(Vertex1, THRESH_POINTS_ARE_SAME) || Vertex0.Equals(Vertex2, THRESH_POINTS_ARE_SAME) || Vertex1.Equals(Vertex2, THRESH_POINTS_ARE_SAME) )
            DegenerateTriangleCount++;
    }

    return DegenerateTriangleCount;
}

#endif

int32
FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
    const TArray< int32 > & VertexList, const HAPI_AttributeInfo & AttribInfo, TArray< float > & Data )
{
    int32 ValidWedgeCount = 0;

    if ( AttribInfo.exists && AttribInfo.tupleSize )
    {
        // Future optimization - see if we can do direct vertex transfer.

        int32 WedgeCount = VertexList.Num();
        TArray< float > VertexData;
        VertexData.SetNumZeroed( WedgeCount * AttribInfo.tupleSize );

        int32 LastValidWedgeIdx = 0;
        for ( int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx )
        {
            int32 VertexId = VertexList[ WedgeIdx ];

            if ( VertexId == -1 )
            {
                // This is an index/wedge we are skipping due to split.
                continue;
            }

            // Increment wedge count, since this is a valid wedge.
            ValidWedgeCount++;

            int32 PrimIdx = WedgeIdx / 3;
            int32 SaveIdx = 0;
            float Value = 0.0f;

            for ( int32 AttributeIndexIdx = 0; AttributeIndexIdx < AttribInfo.tupleSize; ++AttributeIndexIdx )
            {
                switch ( AttribInfo.owner )
                {
                    case HAPI_ATTROWNER_POINT:
                    {
                        Value = Data[ VertexId * AttribInfo.tupleSize + AttributeIndexIdx ];
                        break;
                    }

                    case HAPI_ATTROWNER_PRIM:
                    {
                        Value = Data[ PrimIdx * AttribInfo.tupleSize + AttributeIndexIdx ];
                        break;
                    }

                    case HAPI_ATTROWNER_DETAIL:
                    {
                        Value = Data[ AttributeIndexIdx ];
                        break;
                    }

                    case HAPI_ATTROWNER_VERTEX:
                    {
                        Value = Data[ WedgeIdx * AttribInfo.tupleSize + AttributeIndexIdx ];
                        break;
                    }

                    default:
                    {
                        check( false );
                        continue;
                    }
                }

                SaveIdx = LastValidWedgeIdx * AttribInfo.tupleSize + AttributeIndexIdx;
                VertexData[ SaveIdx ] = Value;
            }

            // We are re-indexing wedges.
            LastValidWedgeIdx++;
        }

        VertexData.SetNumZeroed( ValidWedgeCount * AttribInfo.tupleSize );
        Data = VertexData;
    }

    return ValidWedgeCount;
}

UStaticMesh *
FHoudiniEngineUtils::BakeStaticMesh(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    UStaticMesh * InStaticMesh )
{
    UStaticMesh * StaticMesh = nullptr;

#if WITH_EDITOR

    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    check( HoudiniAsset );

    // We cannot bake curves.
    if ( HoudiniGeoPartObject.IsCurve() )
        return nullptr;

    if ( HoudiniGeoPartObject.IsInstancer() )
    {
        HOUDINI_LOG_MESSAGE( TEXT( "Baking of instanced static meshes is not supported at the moment." ) );
        return nullptr;
    }

    // Get platform manager LOD specific information.
    ITargetPlatform * CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    check( CurrentPlatform );
    const FStaticMeshLODGroup & LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup( NAME_None );
    int32 NumLODs = LODGroup.GetDefaultNumLODs();

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    check( HoudiniRuntimeSettings );

    FString MeshName;
    FGuid BakeGUID;
    UPackage * Package = BakeCreateStaticMeshPackageForComponent(
        HoudiniAssetComponent, HoudiniGeoPartObject, MeshName, BakeGUID, FHoudiniEngineUtils::EBakeMode::CreateNewAssets );

    if( !Package )
    {
        return nullptr;
    }

    // Create static mesh.
    StaticMesh = NewObject< UStaticMesh >( Package, FName( *MeshName ), RF_Public | RF_Transactional );

    // Add meta information to this package.
    FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
        Package, StaticMesh, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
    FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
        Package, StaticMesh, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName );

    // Notify registry that we created a new asset.
    FAssetRegistryModule::AssetCreated( StaticMesh );

    // Copy materials.
    StaticMesh->StaticMaterials = InStaticMesh->StaticMaterials;

    // Create new source model for current static mesh.
    if ( !StaticMesh->SourceModels.Num() )
        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

    FStaticMeshSourceModel * SrcModel = &StaticMesh->SourceModels[ 0 ];
    FRawMeshBulkData * RawMeshBulkData = SrcModel->RawMeshBulkData;

    // Load raw data bytes.
    FRawMesh RawMesh;
    FStaticMeshSourceModel * InSrcModel = &InStaticMesh->SourceModels[ 0 ];
    FRawMeshBulkData * InRawMeshBulkData = InSrcModel->RawMeshBulkData;
    InRawMeshBulkData->LoadRawMesh( RawMesh );

    // Some mesh generation settings.
    HoudiniRuntimeSettings->SetMeshBuildSettings( SrcModel->BuildSettings, RawMesh );

    // Setting the DistanceField resolution
    SrcModel->BuildSettings.DistanceFieldResolutionScale = HoudiniAssetComponent->GeneratedDistanceFieldResolutionScale;

    // We need to check light map uv set for correctness. Unreal seems to have occasional issues with
    // zero UV sets when building lightmaps.
    if ( SrcModel->BuildSettings.bGenerateLightmapUVs )
    {
        // See if we need to disable lightmap generation because of bad UVs.
        if ( FHoudiniEngineUtils::ContainsInvalidLightmapFaces( RawMesh, StaticMesh->LightMapCoordinateIndex ) )
        {
            SrcModel->BuildSettings.bGenerateLightmapUVs = false;

            HOUDINI_LOG_MESSAGE(
                TEXT( "Skipping Lightmap Generation: Object %s " )
                TEXT( "- skipping." ),
                *MeshName );
        }
    }

    // Store the new raw mesh.
    RawMeshBulkData->SaveRawMesh( RawMesh );

    while ( StaticMesh->SourceModels.Num() < NumLODs )
        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

    for ( int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex )
    {
        StaticMesh->SourceModels[ ModelLODIndex ].ReductionSettings = LODGroup.GetDefaultSettings( ModelLODIndex );

        for ( int32 MaterialIndex = 0; MaterialIndex < StaticMesh->StaticMaterials.Num(); ++MaterialIndex )
        {
            FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get( ModelLODIndex, MaterialIndex );
            Info.MaterialIndex = MaterialIndex;
            Info.bEnableCollision = true;
            Info.bCastShadow = true;
            StaticMesh->SectionInfoMap.Set( ModelLODIndex, MaterialIndex, Info );
        }
    }

    // Assign generation parameters for this static mesh.
    HoudiniAssetComponent->SetStaticMeshGenerationParameters( StaticMesh );

    // Copy custom lightmap resolution if it is set.
    if ( InStaticMesh->LightMapResolution != StaticMesh->LightMapResolution )
        StaticMesh->LightMapResolution = InStaticMesh->LightMapResolution;

    if( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
    {
        UBodySetup * BodySetup = StaticMesh->BodySetup;
        check( BodySetup );

        // Enable collisions for this static mesh.
        BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
    }

    FHoudiniScopedGlobalSilence ScopedGlobalSilence;
    StaticMesh->Build( true );
    StaticMesh->MarkPackageDirty();

#endif

    return StaticMesh;
}

UBlueprint *
FHoudiniEngineUtils::BakeBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    UBlueprint * Blueprint = nullptr;

#if WITH_EDITOR

    // Create package for our Blueprint.
    FString BlueprintName = TEXT( "" );
    UPackage * Package = FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent(
        HoudiniAssetComponent, BlueprintName );

    if ( Package )
    {
        AActor * Actor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
        if ( Actor )
        {
            Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor( *BlueprintName, Package, Actor, false );

            // If actor is rooted, unroot it. We can also delete intermediate actor.
            Actor->RemoveFromRoot();
            Actor->ConditionalBeginDestroy();

            if ( Blueprint )
                FAssetRegistryModule::AssetCreated( Blueprint );
        }
    }

#endif

    return Blueprint;
}

AActor *
FHoudiniEngineUtils::ReplaceHoudiniActorWithBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    AActor * Actor = nullptr;

#if WITH_EDITOR

    // Create package for our Blueprint.
    FString BlueprintName = TEXT( "" );
    UPackage * Package = FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent( HoudiniAssetComponent, BlueprintName );

    if ( Package )
    {
        AActor * ClonedActor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
        if ( ClonedActor )
        {
            UBlueprint * Blueprint =  FKismetEditorUtilities::CreateBlueprint(
                ClonedActor->GetClass(), Package, *BlueprintName,
                EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(),
                UBlueprintGeneratedClass::StaticClass(), FName( "CreateFromActor" ) );

            if ( Blueprint )
            {
                Package->MarkPackageDirty();

                if ( ClonedActor->GetInstanceComponents().Num() > 0 )
                    FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, ClonedActor->GetInstanceComponents());

                if ( Blueprint->GeneratedClass )
                {
                    AActor * CDO = CastChecked< AActor >( Blueprint->GeneratedClass->GetDefaultObject() );

                    const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)
                        ( EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
                          EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances );

                    EditorUtilities::CopyActorProperties( ClonedActor, CDO, CopyOptions );

                    USceneComponent * Scene = CDO->GetRootComponent();
                    if ( Scene )
                    {
                        Scene->RelativeLocation = FVector::ZeroVector;
                        Scene->RelativeRotation = FRotator::ZeroRotator;

                        // Clear out the attachment info after having copied the properties from the source actor
                        Scene->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
                        while ( true )
                        {
                            const int32 ChildCount = Scene->GetAttachChildren().Num();
                            if ( ChildCount <= 0 )
                                break;

                            USceneComponent * Component = Scene->GetAttachChildren()[ ChildCount - 1 ];
                            Component->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
                        }
                        check( Scene->GetAttachChildren().Num() == 0 );

                        // Ensure the light mass information is cleaned up
                        Scene->InvalidateLightingCache();
                    }
                }

                // Compile our blueprint and notify asset system about blueprint.
                FKismetEditorUtilities::CompileBlueprint( Blueprint );
                FAssetRegistryModule::AssetCreated( Blueprint );

                // Retrieve actor transform.
                FVector Location = ClonedActor->GetActorLocation();
                FRotator Rotator = ClonedActor->GetActorRotation();

                // Replace cloned actor with Blueprint instance.
                {
                    TArray< AActor * > Actors;
                    Actors.Add( ClonedActor );

                    ClonedActor->RemoveFromRoot();
                    Actor = FKismetEditorUtilities::CreateBlueprintInstanceFromSelection( Blueprint, Actors, Location, Rotator );
                }

                // We can initiate Houdini actor deletion.
                AHoudiniAssetActor * HoudiniAssetActor = HoudiniAssetComponent->GetHoudiniAssetActorOwner();

                // Remove Houdini actor from active selection in editor and delete it.
                if( GEditor )
                {
                    GEditor->SelectActor( HoudiniAssetActor, false, false );
                    GEditor->Layers->DisassociateActorFromLayers( HoudiniAssetActor );
                }

                UWorld * World = HoudiniAssetActor->GetWorld();
                World->EditorDestroyActor( HoudiniAssetActor, false );
            }
            else
            {
                ClonedActor->RemoveFromRoot();
                ClonedActor->ConditionalBeginDestroy();
            }
        }
    }

#endif

    return Actor;
}

void
FHoudiniEngineUtils::HapiCreateMaterials(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const HAPI_AssetInfo & AssetInfo,
    const TSet< HAPI_MaterialId > & UniqueMaterialIds,
    const TSet< HAPI_MaterialId > & UniqueInstancerMaterialIds,
    TMap< FString, UMaterialInterface * > & Materials )
{
#if WITH_EDITOR

    // Empty returned materials.
    Materials.Empty();

    if ( UniqueMaterialIds.Num() == 0 )
        return;

    HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();

    const TMap< FString, UMaterialInterface * > & CachedMaterials =
        HoudiniAssetComponent->HoudiniAssetComponentMaterials->Assignments;

    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

    // Update context for generated materials (will trigger when object goes out of scope).
    FMaterialUpdateContext MaterialUpdateContext;

    // Default Houdini material.
    UMaterial * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
    Materials.Add( HAPI_UNREAL_DEFAULT_MATERIAL_NAME, DefaultMaterial );

    // Factory to create materials.
    UMaterialFactoryNew * MaterialFactory = NewObject< UMaterialFactoryNew >();
    MaterialFactory->AddToRoot();

    for ( TSet< HAPI_MaterialId >::TConstIterator IterMaterialId( UniqueMaterialIds ); IterMaterialId; ++IterMaterialId )
    {
        HAPI_MaterialId MaterialId = *IterMaterialId;

        // Get material information.
        HAPI_MaterialInfo MaterialInfo;
        if ( FHoudiniApi::GetMaterialInfo(
            FHoudiniEngine::Get().GetSession(), AssetInfo.id,
            MaterialId, &MaterialInfo ) != HAPI_RESULT_SUCCESS )
        {
            continue;
        }

        // Get node information.
        HAPI_NodeInfo NodeInfo;
        if ( FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, &NodeInfo ) != HAPI_RESULT_SUCCESS )
        {
            continue;
        }

        if ( MaterialInfo.exists )
        {
            FString MaterialShopName = TEXT( "" );
            if ( !FHoudiniEngineUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName ) )
                continue;

            UMaterialInterface * const * FoundMaterialInterface = CachedMaterials.Find( MaterialShopName );
            UMaterial * FoundMaterial = nullptr;
            if(FoundMaterialInterface)
                FoundMaterial = Cast< UMaterial >(*FoundMaterialInterface);

	    UMaterial * Material = nullptr;
            bool bCreatedNewMaterial = false;

            if ( FoundMaterial )
            {
                Material = FoundMaterial;

                // If cached material exists and has not changed, we can reuse it.
                if ( !MaterialInfo.hasChanged )
                {
                    // We found cached material, we can reuse it.
                    Materials.Add( MaterialShopName, Material );
                    continue;
                }
            }
            else
            {
                // Material was not found, we need to create it.

                FString MaterialName = TEXT( "" );

                // Create material package and get material name.
                UPackage * MaterialPackage = FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
                    HoudiniAssetComponent, MaterialInfo, MaterialName, FHoudiniEngineUtils::EBakeMode::Intermediate );

                // Create new material.
                Material = (UMaterial *) MaterialFactory->FactoryCreateNew(
                    UMaterial::StaticClass(), MaterialPackage,
                    *MaterialName, RF_Transactional, NULL, GWarn );

                // Add meta information to this package.
                FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                    MaterialPackage, Material, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
                FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                    MaterialPackage, Material, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName );

                bCreatedNewMaterial = true;
            }

            // If this is an instancer material, enable the instancing flag.
            if ( UniqueInstancerMaterialIds.Contains( MaterialId ) )
                Material->bUsedWithInstancedStaticMeshes = true;

            // Get node parameters.
            TArray< HAPI_ParmInfo > NodeParams;
            NodeParams.SetNumUninitialized( NodeInfo.parmCount );
            FHoudiniApi::GetParameters(
                FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[ 0 ], 0, NodeInfo.parmCount);

            // Get names of parameters.
            TArray< std::string > NodeParamNames;
            FHoudiniEngineUtils::HapiRetrieveParameterNames( NodeParams, NodeParamNames );

            // Reset material expressions.
            Material->Expressions.Empty();

            // Generate various components for this material.
            bool bMaterialComponentCreated = false;
            int32 MaterialNodeY = FHoudiniEngineUtils::MaterialExpressionNodeY;

            // By default we mark material as opaque. Some of component creators can change this.
            Material->BlendMode = BLEND_Opaque;

            // Extract diffuse plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentDiffuse(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract opacity plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentOpacity(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract opacity mask plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentOpacityMask(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract normal plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentNormal(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract specular plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentSpecular(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract roughness plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentRoughness(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract metallic plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentMetallic(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Extract emissive plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentEmissive(
                HoudiniAssetComponent, Material, MaterialInfo, NodeInfo, NodeParams, NodeParamNames, MaterialNodeY );

            // Set other material properties.
            Material->TwoSided = true;
            Material->SetShadingModel( MSM_DefaultLit );

            // Schedule this material for update.
            MaterialUpdateContext.AddMaterial( Material );

            // Cache material.
            Materials.Add( MaterialShopName, Material );

            // Propagate and trigger material updates.
            if ( bCreatedNewMaterial )
                FAssetRegistryModule::AssetCreated( Material );


            Material->PreEditChange( nullptr );
            Material->PostEditChange();
            Material->MarkPackageDirty();
        }
        else
        {
            // Material does not exist, we will use default Houdini material in this case.
        }
    }

    MaterialFactory->RemoveFromRoot();

#endif
}

#if WITH_EDITOR

bool
FHoudiniEngineUtils::CreateMaterialComponentDiffuse(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY)
{
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Names of generating Houdini parameters.
    FString GeneratingParameterNameDiffuseTexture = TEXT( "" );
    FString GeneratingParameterNameUniformColor = TEXT( "" );
    FString GeneratingParameterNameVertexColor = TEXT( HAPI_UNREAL_ATTRIB_COLOR );

    // Diffuse texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Default;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = true;

    // Attempt to look up previously created expressions.
    UMaterialExpression * MaterialExpression = Material->BaseColor.Expression;

    // Locate sampling expression.
    UMaterialExpressionTextureSampleParameter2D * ExpressionTextureSample =
        Cast< UMaterialExpressionTextureSampleParameter2D >( FHoudiniEngineUtils::MaterialLocateExpression(
            MaterialExpression, UMaterialExpressionTextureSampleParameter2D::StaticClass() ) );

    // If texture sampling expression does exist, attempt to look up corresponding texture.
    UTexture2D * TextureDiffuse = nullptr;
    if ( ExpressionTextureSample )
        TextureDiffuse = Cast< UTexture2D >( ExpressionTextureSample->Texture );

    // Locate uniform color expression.
    UMaterialExpressionVectorParameter * ExpressionConstant4Vector =
        Cast< UMaterialExpressionVectorParameter >( FHoudiniEngineUtils::MaterialLocateExpression(
            MaterialExpression, UMaterialExpressionVectorParameter::StaticClass() ) );

    // If uniform color expression does not exist, create it.
    if ( !ExpressionConstant4Vector )
    {
        ExpressionConstant4Vector = NewObject< UMaterialExpressionVectorParameter >(
            Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, RF_Transactional );
        ExpressionConstant4Vector->DefaultValue = FLinearColor::White;
    }

    // Add expression.
    Material->Expressions.Add( ExpressionConstant4Vector );

    // Locate vertex color expression.
    UMaterialExpressionVertexColor * ExpressionVertexColor =
        Cast< UMaterialExpressionVertexColor >( FHoudiniEngineUtils::MaterialLocateExpression(
            MaterialExpression, UMaterialExpressionVertexColor::StaticClass() ) );

    // If vertex color expression does not exist, create it.
    if ( !ExpressionVertexColor )
    {
        ExpressionVertexColor = NewObject< UMaterialExpressionVertexColor >(
            Material, UMaterialExpressionVertexColor::StaticClass(), NAME_None, RF_Transactional );
        ExpressionVertexColor->Desc = GeneratingParameterNameVertexColor;
    }

    // Add expression.
    Material->Expressions.Add( ExpressionVertexColor );

    // Material should have at least one multiply expression.
    UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast< UMaterialExpressionMultiply >( MaterialExpression );
    if ( !MaterialExpressionMultiply )
        MaterialExpressionMultiply = NewObject< UMaterialExpressionMultiply >(
            Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, RF_Transactional );

    // Add expression.
    Material->Expressions.Add( MaterialExpressionMultiply );

    // See if primary multiplication has secondary multiplication as A input.
    UMaterialExpressionMultiply * MaterialExpressionMultiplySecondary = nullptr;
    if ( MaterialExpressionMultiply->A.Expression )
        MaterialExpressionMultiplySecondary =
            Cast< UMaterialExpressionMultiply >( MaterialExpressionMultiply->A.Expression );

    // Get number of diffuse textures in use.
    /*
    int32 DiffuseTexCount = 0;

    int32 ParmNumDiffuseTex =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_TEXTURE_LAYERS_NUM, NodeParamNames );

    if ( ParmNumDiffuseTex != -1 )
    {
        const HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNumDiffuseTex ];
        int DiffuseTexCountValue = 0;

        if ( FHoudiniApi::GetParmIntValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (int *) &DiffuseTexCountValue,
            ParmInfo.intValuesIndex, ParmInfo.size ) == HAPI_RESULT_SUCCESS )
        {
            DiffuseTexCount = DiffuseTexCountValue;
        }
    }
    */

    // See if diffuse texture is available.
    int32 ParmDiffuseTextureIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0, NodeParamNames );

    if ( ParmDiffuseTextureIdx >= 0 )
    {
        GeneratingParameterNameDiffuseTexture = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );
    }
    else
    {
        ParmDiffuseTextureIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1, NodeParamNames );

        if ( ParmDiffuseTextureIdx >= 0 )
            GeneratingParameterNameDiffuseTexture = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );
    }

    // See if uniform color is available.
    int32 ParmDiffuseColorIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_0, NodeParamNames );

    if ( ParmDiffuseColorIdx >= 0 )
    {
        GeneratingParameterNameUniformColor = TEXT( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_0 );
    }
    else
    {
        ParmDiffuseColorIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_1, NodeParamNames );

        if ( ParmDiffuseColorIdx >= 0 )
            GeneratingParameterNameUniformColor = TEXT( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_1 );
    }

    // If we have diffuse texture parameter.
    if ( ParmDiffuseTextureIdx >= 0 )
    {
        TArray< char > ImageBuffer;

        // Get image planes of diffuse map.
        TArray< FString > DiffuseImagePlanes;
        bool bFoundImagePlanes = FHoudiniEngineUtils::HapiGetImagePlanes(
            NodeParams[ ParmDiffuseTextureIdx ].id, MaterialInfo, DiffuseImagePlanes );

        HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
        const char * PlaneType = "";

        if ( bFoundImagePlanes && DiffuseImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_COLOR ) ) )
        {
            if ( DiffuseImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA ) ) )
            {
                ImagePacking = HAPI_IMAGE_PACKING_RGBA;
                PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;

                // Material does use alpha.
                CreateTexture2DParameters.bUseAlpha = true;
            }
            else
            {
                // We still need to have the Alpha plane, just not the CreateTexture2DParameters
                // alpha option. This is because all texture data from Houdini Engine contains
                // the alpha plane by default.
                ImagePacking = HAPI_IMAGE_PACKING_RGBA;
                PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
            }
        }
        else
        {
            bFoundImagePlanes = false;
        }

        // Retrieve color plane.
        if ( bFoundImagePlanes && FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ParmDiffuseTextureIdx].id,
            MaterialInfo, ImageBuffer, PlaneType,
            HAPI_IMAGE_DATA_INT8, ImagePacking, false ) )
        {
            UPackage * TextureDiffusePackage = nullptr;
            if ( TextureDiffuse )
                TextureDiffusePackage = Cast< UPackage >( TextureDiffuse->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                MaterialInfo.id, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureDiffuseName;
                bool bCreatedNewTextureDiffuse = false;

                // Create diffuse texture package, if this is a new diffuse texture.
                if ( !TextureDiffusePackage )
                {
                    TextureDiffusePackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniAssetComponent,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
                        TextureDiffuseName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                }

                // Create diffuse texture, if we need to create one.
                if ( !TextureDiffuse )
                    bCreatedNewTextureDiffuse = true;

                // Reuse existing diffuse texture, or create new one.
                TextureDiffuse = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureDiffuse, ImageInfo,
                    TextureDiffusePackage, TextureDiffuseName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World );

                // Create diffuse sampling expression, if needed.
                if ( !ExpressionTextureSample )
                {
                    ExpressionTextureSample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional );
                }

                // Record generating parameter.
                ExpressionTextureSample->Desc = GeneratingParameterNameDiffuseTexture;
                ExpressionTextureSample->ParameterName = *GeneratingParameterNameDiffuseTexture;
                ExpressionTextureSample->Texture = TextureDiffuse;
                ExpressionTextureSample->SamplerType = SAMPLERTYPE_Color;

                // Add expression.
                Material->Expressions.Add( ExpressionTextureSample );

                // Propagate and trigger diffuse texture updates.
                if ( bCreatedNewTextureDiffuse )
                    FAssetRegistryModule::AssetCreated( TextureDiffuse );

                TextureDiffuse->PreEditChange( nullptr );
                TextureDiffuse->PostEditChange();
                TextureDiffuse->MarkPackageDirty();
            }
        }
    }

    // If we have uniform color parameter.
    if ( ParmDiffuseColorIdx >= 0 )
    {
        FLinearColor Color = FLinearColor::White;
        const HAPI_ParmInfo& ParmInfo = NodeParams[ ParmDiffuseColorIdx ];

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float *) &Color.R,
            ParmInfo.floatValuesIndex, ParmInfo.size ) == HAPI_RESULT_SUCCESS )
        {
            if ( ParmInfo.size == 3 )
                Color.A = 1.0f;

            // Record generating parameter.
            ExpressionConstant4Vector->Desc = GeneratingParameterNameUniformColor;
            ExpressionConstant4Vector->ParameterName = *GeneratingParameterNameUniformColor;
            ExpressionConstant4Vector->DefaultValue = Color;
        }
    }

    // If we have have texture sample expression present, we need a secondary multiplication expression.
    if ( ExpressionTextureSample )
    {
        if ( !MaterialExpressionMultiplySecondary )
        {
            MaterialExpressionMultiplySecondary = NewObject< UMaterialExpressionMultiply >(
                Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, RF_Transactional );

            // Add expression.
            Material->Expressions.Add( MaterialExpressionMultiplySecondary );
        }
    }
    else
    {
        // If secondary multiplication exists, but we have no sampling, we can free it.
        if ( MaterialExpressionMultiplySecondary )
        {
            MaterialExpressionMultiplySecondary->A.Expression = nullptr;
            MaterialExpressionMultiplySecondary->B.Expression = nullptr;
            MaterialExpressionMultiplySecondary->ConditionalBeginDestroy();
        }
    }

    float SecondaryExpressionScale = 1.0f;
    if ( MaterialExpressionMultiplySecondary )
        SecondaryExpressionScale = 1.5f;

    // Create multiplication expression which has uniform color and vertex color.
    MaterialExpressionMultiply->A.Expression = ExpressionConstant4Vector;
    MaterialExpressionMultiply->B.Expression = ExpressionVertexColor;

    ExpressionConstant4Vector->MaterialExpressionEditorX =
        FHoudiniEngineUtils::MaterialExpressionNodeX -
        FHoudiniEngineUtils::MaterialExpressionNodeStepX * SecondaryExpressionScale;
    ExpressionConstant4Vector->MaterialExpressionEditorY = MaterialNodeY;
    MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

    ExpressionVertexColor->MaterialExpressionEditorX =
        FHoudiniEngineUtils::MaterialExpressionNodeX -
        FHoudiniEngineUtils::MaterialExpressionNodeStepX * SecondaryExpressionScale;
    ExpressionVertexColor->MaterialExpressionEditorY = MaterialNodeY;
    MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

    MaterialExpressionMultiply->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
    MaterialExpressionMultiply->MaterialExpressionEditorY =
        ( ExpressionVertexColor->MaterialExpressionEditorY - ExpressionConstant4Vector->MaterialExpressionEditorY ) / 2;

    // Hook up secondary multiplication expression to first one.
    if ( MaterialExpressionMultiplySecondary )
    {
        MaterialExpressionMultiplySecondary->A.Expression = MaterialExpressionMultiply;
        MaterialExpressionMultiplySecondary->B.Expression = ExpressionTextureSample;

        ExpressionTextureSample->MaterialExpressionEditorX =
            FHoudiniEngineUtils::MaterialExpressionNodeX -
            FHoudiniEngineUtils::MaterialExpressionNodeStepX * SecondaryExpressionScale;
        ExpressionTextureSample->MaterialExpressionEditorY = MaterialNodeY;
        MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

        MaterialExpressionMultiplySecondary->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
        MaterialExpressionMultiplySecondary->MaterialExpressionEditorY =
            MaterialExpressionMultiply->MaterialExpressionEditorY + FHoudiniEngineUtils::MaterialExpressionNodeStepY;

        // Assign expression.
        Material->BaseColor.Expression = MaterialExpressionMultiplySecondary;
    }
    else
    {
        // Assign expression.
        Material->BaseColor.Expression = MaterialExpressionMultiply;

        MaterialExpressionMultiply->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
        MaterialExpressionMultiply->MaterialExpressionEditorY =
            ( ExpressionVertexColor->MaterialExpressionEditorY -
                ExpressionConstant4Vector->MaterialExpressionEditorY ) / 2;
    }

    return true;
}

bool
FHoudiniEngineUtils::CreateMaterialComponentOpacityMask(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams,
    const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameters.
    FString GeneratingParameterNameTexture = TEXT( "" );

    UMaterialExpression * MaterialExpression = Material->OpacityMask.Expression;

    // Opacity expressions.
    UMaterialExpressionTextureSampleParameter2D * ExpressionTextureOpacitySample = nullptr;
    UTexture2D * TextureOpacity = nullptr;

    // Opacity texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = true;

    // See if opacity texture is available.
    int32 ParmOpacityTextureIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_OPACITY_1, NodeParamNames );

    if ( ParmOpacityTextureIdx >= 0 )
        GeneratingParameterNameTexture = TEXT( HAPI_UNREAL_PARAM_MAP_OPACITY_1 );

    // If we have opacity texture parameter.
    if ( ParmOpacityTextureIdx >= 0 )
    {
        TArray< char > ImageBuffer;

        // Get image planes of opacity map.
        TArray< FString > OpacityImagePlanes;
        bool bFoundImagePlanes = FHoudiniEngineUtils::HapiGetImagePlanes(
            NodeParams[ ParmOpacityTextureIdx ].id,
            MaterialInfo, OpacityImagePlanes );

        HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
        const char * PlaneType = "";

        if ( bFoundImagePlanes && OpacityImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA ) ) )
        {
            ImagePacking = HAPI_IMAGE_PACKING_RGBA;
            PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
            CreateTexture2DParameters.bUseAlpha = true;
        }
        else
        {
            bFoundImagePlanes = false;
        }

        if ( bFoundImagePlanes && FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ ParmOpacityTextureIdx ].id,
            MaterialInfo, ImageBuffer, PlaneType,
            HAPI_IMAGE_DATA_INT8, ImagePacking, false ) )
        {
            // Locate sampling expression.
            ExpressionTextureOpacitySample = Cast< UMaterialExpressionTextureSampleParameter2D >(
                FHoudiniEngineUtils::MaterialLocateExpression(
                    MaterialExpression, UMaterialExpressionTextureSampleParameter2D::StaticClass() ) );

            // Locate opacity texture, if valid.
            if ( ExpressionTextureOpacitySample )
                TextureOpacity = Cast< UTexture2D >( ExpressionTextureOpacitySample->Texture );

            UPackage * TextureOpacityPackage = nullptr;
            if ( TextureOpacity )
                TextureOpacityPackage = Cast< UPackage >( TextureOpacity->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                MaterialInfo.id, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureOpacityName;
                bool bCreatedNewTextureOpacity = false;

                // Create opacity texture package, if this is a new opacity texture.
                if ( !TextureOpacityPackage )
                {
                    TextureOpacityPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniAssetComponent,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
                        TextureOpacityName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                }

                // Create opacity texture, if we need to create one.
                if ( !TextureOpacity )
                    bCreatedNewTextureOpacity = true;

                // Reuse existing opacity texture, or create new one.
                TextureOpacity = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureOpacity, ImageInfo,
                    TextureOpacityPackage, TextureOpacityName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World );

                // Create opacity sampling expression, if needed.
                if ( !ExpressionTextureOpacitySample )
                {
                    ExpressionTextureOpacitySample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional );
                }

                // Record generating parameter.
                ExpressionTextureOpacitySample->Desc = GeneratingParameterNameTexture;
                ExpressionTextureOpacitySample->ParameterName = *GeneratingParameterNameTexture;
                ExpressionTextureOpacitySample->Texture = TextureOpacity;
                ExpressionTextureOpacitySample->SamplerType = SAMPLERTYPE_Grayscale;

                // Offset node placement.
                ExpressionTextureOpacitySample->MaterialExpressionEditorX =
                    FHoudiniEngineUtils::MaterialExpressionNodeX;
                ExpressionTextureOpacitySample->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                // Add expression.
                Material->Expressions.Add( ExpressionTextureOpacitySample );

                // We need to set material type to masked.
                TArray< FExpressionOutput > ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
                FExpressionOutput* ExpressionOutput = ExpressionOutputs.GetData();

                Material->OpacityMask.Expression = ExpressionTextureOpacitySample;
                Material->BlendMode = BLEND_Masked;

                Material->OpacityMask.Mask = ExpressionOutput->Mask;
                Material->OpacityMask.MaskR = 1;
                Material->OpacityMask.MaskG = 0;
                Material->OpacityMask.MaskB = 0;
                Material->OpacityMask.MaskA = 0;

                // Propagate and trigger opacity texture updates.
                if ( bCreatedNewTextureOpacity )
                    FAssetRegistryModule::AssetCreated( TextureOpacity );

                TextureOpacity->PreEditChange( nullptr );
                TextureOpacity->PostEditChange();
                TextureOpacity->MarkPackageDirty();

                bExpressionCreated = true;
            }
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineUtils::CreateMaterialComponentOpacity(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams,
    const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;
    float OpacityValue = 1.0f;
    bool bNeedsTranslucency = false;

    // Name of generating Houdini parameters.
    FString GeneratingParameterNameScalar = TEXT( "" );
    FString GeneratingParameterNameTexture = TEXT( "" );

    UMaterialExpression * MaterialExpression = Material->Opacity.Expression;

    // Opacity expressions.
    UMaterialExpressionTextureSampleParameter2D * ExpressionTextureOpacitySample = nullptr;
    UMaterialExpressionScalarParameter * ExpressionScalarOpacity = nullptr;
    UTexture2D * TextureOpacity = nullptr;

    // Opacity texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = true;

    // If opacity sampling expression was not created, check if diffuse contains an alpha plane.
    if ( !ExpressionTextureOpacitySample )
    {
        UMaterialExpression * MaterialExpressionDiffuse = Material->BaseColor.Expression;
        if ( MaterialExpressionDiffuse )
        {
            // Locate diffuse sampling expression.
            UMaterialExpressionTextureSampleParameter2D * ExpressionTextureDiffuseSample =
                Cast< UMaterialExpressionTextureSampleParameter2D >(
                    FHoudiniEngineUtils::MaterialLocateExpression(
                        MaterialExpressionDiffuse,
                        UMaterialExpressionTextureSampleParameter2D::StaticClass() ) );

            // See if there's an alpha plane in this expression's texture.
            if ( ExpressionTextureDiffuseSample )
            {
                UTexture2D * DiffuseTexture = Cast< UTexture2D >( ExpressionTextureDiffuseSample->Texture );
                if ( DiffuseTexture && !DiffuseTexture->CompressionNoAlpha )
                {
                    ExpressionTextureOpacitySample = ExpressionTextureDiffuseSample;

                    // Check if material is transparent. If it is, we need to hook up alpha.
                    bNeedsTranslucency = FHoudiniEngineUtils::HapiIsMaterialTransparent( MaterialInfo );
                }
            }
        }
    }

    // Retrieve opacity uniform parameter.
    int32 ParmOpacityValueIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_ALPHA, NodeParamNames );

    if ( ParmOpacityValueIdx >= 0 )
    {
        GeneratingParameterNameScalar = TEXT( HAPI_UNREAL_PARAM_ALPHA );

        const HAPI_ParmInfo & ParmInfo = NodeParams[ ParmOpacityValueIdx ];
        if ( ParmInfo.size > 0 && ParmInfo.floatValuesIndex >= 0 )
        {
            float OpacityValueRetrieved = 1.0f;
            if ( FHoudiniApi::GetParmFloatValues(
                FHoudiniEngine::Get().GetSession(), NodeInfo.id,
                (float *) &OpacityValue, ParmInfo.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
            {
                if ( !ExpressionScalarOpacity )
                {
                    ExpressionScalarOpacity = NewObject< UMaterialExpressionScalarParameter >(
                        Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, RF_Transactional );
                }

                // Clamp retrieved value.
                OpacityValueRetrieved = FMath::Clamp< float >( OpacityValueRetrieved, 0.0f, 1.0f );
                OpacityValue = OpacityValueRetrieved;

                // Set expression fields.
                ExpressionScalarOpacity->DefaultValue = OpacityValue;
                ExpressionScalarOpacity->SliderMin = 0.0f;
                ExpressionScalarOpacity->SliderMax = 1.0f;
                ExpressionScalarOpacity->Desc = GeneratingParameterNameScalar;
                ExpressionScalarOpacity->ParameterName = *GeneratingParameterNameScalar;

                // Add expression.
                Material->Expressions.Add( ExpressionScalarOpacity );

                // If alpha is less than 1, we need translucency.
                bNeedsTranslucency |= ( OpacityValue != 1.0f );
            }
        }
    }

    if ( bNeedsTranslucency )
        Material->BlendMode = BLEND_Translucent;

    if ( ExpressionScalarOpacity && ExpressionTextureOpacitySample )
    {
        // We have both alpha and alpha uniform, attempt to locate multiply expression.
        UMaterialExpressionMultiply * ExpressionMultiply =
            Cast< UMaterialExpressionMultiply >(
                FHoudiniEngineUtils::MaterialLocateExpression(
                    MaterialExpression,
                    UMaterialExpressionMultiply::StaticClass() ) );

        if ( !ExpressionMultiply )
            ExpressionMultiply = NewObject< UMaterialExpressionMultiply >(
                Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, RF_Transactional );

        Material->Expressions.Add( ExpressionMultiply );

        TArray< FExpressionOutput > ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
        FExpressionOutput * ExpressionOutput = ExpressionOutputs.GetData();

        ExpressionMultiply->A.Expression = ExpressionTextureOpacitySample;
        ExpressionMultiply->B.Expression = ExpressionScalarOpacity;

        Material->Opacity.Expression = ExpressionMultiply;
        Material->Opacity.Mask = ExpressionOutput->Mask;
        Material->Opacity.MaskR = 0;
        Material->Opacity.MaskG = 0;
        Material->Opacity.MaskB = 0;
        Material->Opacity.MaskA = 1;

        ExpressionMultiply->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
        ExpressionMultiply->MaterialExpressionEditorY = MaterialNodeY;

        ExpressionScalarOpacity->MaterialExpressionEditorX =
            FHoudiniEngineUtils::MaterialExpressionNodeX - FHoudiniEngineUtils::MaterialExpressionNodeStepX;
        ExpressionScalarOpacity->MaterialExpressionEditorY = MaterialNodeY;
        MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

        bExpressionCreated = true;
    }
    else if ( ExpressionScalarOpacity )
    {
        Material->Opacity.Expression = ExpressionScalarOpacity;

        ExpressionScalarOpacity->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
        ExpressionScalarOpacity->MaterialExpressionEditorY = MaterialNodeY;
        MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

        bExpressionCreated = true;
    }
    else if ( ExpressionTextureOpacitySample )
    {
        TArray<FExpressionOutput> ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
        FExpressionOutput * ExpressionOutput = ExpressionOutputs.GetData();

        Material->Opacity.Expression = ExpressionTextureOpacitySample;
        Material->Opacity.Mask = ExpressionOutput->Mask;
        Material->Opacity.MaskR = 0;
        Material->Opacity.MaskG = 0;
        Material->Opacity.MaskB = 0;
        Material->Opacity.MaskA = 1;

        bExpressionCreated = true;
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineUtils::CreateMaterialComponentNormal(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams,
    const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY)
{
    bool bExpressionCreated = false;
    bool bTangentSpaceNormal = true;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Normal texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Normalmap;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if separate normal texture is available.
    int32 ParmNameNormalIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_NORMAL_0, NodeParamNames );

    if ( ParmNameNormalIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_0 );
    }
    else
    {
        ParmNameNormalIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_NORMAL_1, NodeParamNames );

        if ( ParmNameNormalIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_1 );
    }

    if ( ParmNameNormalIdx >= 0 )
    {
        // Retrieve space for this normal texture.
        int32 ParmNormalTypeIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE, NodeParamNames );

        // Retrieve value for normal type choice list (if exists).

        if ( ParmNormalTypeIdx >= 0 )
        {
            FString NormalType = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_TANGENT );

            const HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNormalTypeIdx ];
            if ( ParmInfo.size > 0 && ParmInfo.stringValuesIndex >= 0 )
            {
                HAPI_StringHandle StringHandle;
                if ( FHoudiniApi::GetParmStringValues(
                    FHoudiniEngine::Get().GetSession(),
                    NodeInfo.id, false, &StringHandle, ParmInfo.stringValuesIndex, ParmInfo.size ) == HAPI_RESULT_SUCCESS )
                {
                    // Get the actual string value.
                    FString NormalTypeString = TEXT( "" );
                    FHoudiniEngineString HoudiniEngineString( StringHandle );
                    if ( HoudiniEngineString.ToFString( NormalTypeString ) )
                        NormalType = NormalTypeString;
                }
            }

            // Check if we require world space normals.
            if ( NormalType.Equals( TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_WORLD ), ESearchCase::IgnoreCase ) )
                bTangentSpaceNormal = false;
        }

        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ ParmNameNormalIdx ].id, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionNormal =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Normal.Expression );

            UTexture2D * TextureNormal = nullptr;
            if ( ExpressionNormal )
            {
                TextureNormal = Cast< UTexture2D >( ExpressionNormal->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Normal.Expression )
                {
                    Material->Normal.Expression->ConditionalBeginDestroy();
                    Material->Normal.Expression = nullptr;
                }
            }

            UPackage * TextureNormalPackage = nullptr;
            if ( TextureNormal )
                TextureNormalPackage = Cast< UPackage >( TextureNormal->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                MaterialInfo.id, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureNormalName;
                bool bCreatedNewTextureNormal = false;

                // Create normal texture package, if this is a new normal texture.
                if ( !TextureNormalPackage )
                {
                    TextureNormalPackage =
                        FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                            HoudiniAssetComponent,
                            MaterialInfo,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                            TextureNormalName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                }

                // Create normal texture, if we need to create one.
                if ( !TextureNormal )
                    bCreatedNewTextureNormal = true;

                // Reuse existing normal texture, or create new one.
                TextureNormal = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureNormal, ImageInfo,
                    TextureNormalPackage, TextureNormalName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_WorldNormalMap );

                // Create normal sampling expression, if needed.
                if ( !ExpressionNormal )
                    ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional );

                // Record generating parameter.
                ExpressionNormal->Desc = GeneratingParameterName;
                ExpressionNormal->ParameterName = *GeneratingParameterName;

                ExpressionNormal->Texture = TextureNormal;
                ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

                // Offset node placement.
                ExpressionNormal->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
                ExpressionNormal->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                // Set normal space.
                Material->bTangentSpaceNormal = bTangentSpaceNormal;

                // Assign expression to material.
                Material->Expressions.Add(ExpressionNormal);
                Material->Normal.Expression = ExpressionNormal;

                bExpressionCreated = true;
            }
        }
    }

    // If separate normal map was not found, see if normal plane exists in diffuse map.
    if ( !bExpressionCreated )
    {
        // See if diffuse texture is available.
        int32 ParmNameBaseIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0, NodeParamNames );

        if ( ParmNameBaseIdx >= 0 )
        {
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );
        }
        else
        {
            ParmNameBaseIdx =
                FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1, NodeParamNames );

            if ( ParmNameBaseIdx >= 0 )
                GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );
        }

        if ( ParmNameBaseIdx >= 0 )
        {
            // Normal plane is available in diffuse map.

            TArray< char > ImageBuffer;

            // Retrieve color plane - this will contain normal data.
            if ( FHoudiniEngineUtils::HapiExtractImage(
                NodeParams[ ParmNameBaseIdx ].id, MaterialInfo, ImageBuffer,
                HAPI_UNREAL_MATERIAL_TEXTURE_NORMAL, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGB, true ) )
            {
                UMaterialExpressionTextureSampleParameter2D * ExpressionNormal =
                    Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Normal.Expression );

                UTexture2D * TextureNormal = nullptr;
                if ( ExpressionNormal )
                {
                    TextureNormal = Cast< UTexture2D >( ExpressionNormal->Texture );
                }
                else
                {
                    // Otherwise new expression is of a different type.
                    if ( Material->Normal.Expression )
                    {
                        Material->Normal.Expression->ConditionalBeginDestroy();
                        Material->Normal.Expression = nullptr;
                    }
                }

                UPackage * TextureNormalPackage = nullptr;
                if ( TextureNormal )
                    TextureNormalPackage = Cast< UPackage >( TextureNormal->GetOuter() );

                HAPI_ImageInfo ImageInfo;
                Result = FHoudiniApi::GetImageInfo(
                    FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                    MaterialInfo.id, &ImageInfo );

                if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
                {
                    // Create texture.
                    FString TextureNormalName;
                    bool bCreatedNewTextureNormal = false;

                    // Create normal texture package, if this is a new normal texture.
                    if ( !TextureNormalPackage )
                    {
                        TextureNormalPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                            HoudiniAssetComponent,
                            MaterialInfo,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                            TextureNormalName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                    }

                    // Create normal texture, if we need to create one.
                    if ( !TextureNormal )
                        bCreatedNewTextureNormal = true;

                    // Reuse existing normal texture, or create new one.
                    TextureNormal = FHoudiniEngineUtils::CreateUnrealTexture(
                        TextureNormal, ImageInfo,
                        TextureNormalPackage, TextureNormalName, ImageBuffer,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL, CreateTexture2DParameters,
                        TEXTUREGROUP_WorldNormalMap );

                    // Create normal sampling expression, if needed.
                    if ( !ExpressionNormal )
                        ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                            Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional);

                    // Record generating parameter.
                    ExpressionNormal->Desc = GeneratingParameterName;
                    ExpressionNormal->ParameterName = *GeneratingParameterName;

                    ExpressionNormal->Texture = TextureNormal;
                    ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

                    // Offset node placement.
                    ExpressionNormal->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
                    ExpressionNormal->MaterialExpressionEditorY = MaterialNodeY;
                    MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                    // Set normal space.
                    Material->bTangentSpaceNormal = bTangentSpaceNormal;

                    // Assign expression to material.
                    Material->Expressions.Add( ExpressionNormal );
                    Material->Normal.Expression = ExpressionNormal;

                    // Propagate and trigger diffuse texture updates.
                    if ( bCreatedNewTextureNormal )
                        FAssetRegistryModule::AssetCreated( TextureNormal );

                    TextureNormal->PreEditChange( nullptr );
                    TextureNormal->PostEditChange();
                    TextureNormal->MarkPackageDirty();

                    bExpressionCreated = true;
                }
            }
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineUtils::CreateMaterialComponentSpecular(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Specular texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if specular texture is available.
    int32 ParmNameSpecularIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_SPECULAR_0, NodeParamNames );

    if ( ParmNameSpecularIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_SPECULAR_0 );
    }
    else
    {
        ParmNameSpecularIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_SPECULAR_1, NodeParamNames );

        if ( ParmNameSpecularIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_SPECULAR_1 );
    }

    if ( ParmNameSpecularIdx >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ ParmNameSpecularIdx ].id, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionSpecular =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Specular.Expression );

            UTexture2D * TextureSpecular = nullptr;
            if ( ExpressionSpecular )
            {
                TextureSpecular = Cast< UTexture2D >( ExpressionSpecular->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Specular.Expression )
                {
                    Material->Specular.Expression->ConditionalBeginDestroy();
                    Material->Specular.Expression = nullptr;
                }
            }

            UPackage * TextureSpecularPackage = nullptr;
            if ( TextureSpecular )
                TextureSpecularPackage = Cast< UPackage >( TextureSpecular->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                MaterialInfo.id, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureSpecularName;
                bool bCreatedNewTextureSpecular = false;

                // Create specular texture package, if this is a new specular texture.
                if ( !TextureSpecularPackage )
                {
                    TextureSpecularPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniAssetComponent,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
                        TextureSpecularName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                }

                // Create specular texture, if we need to create one.
                if ( !TextureSpecular )
                    bCreatedNewTextureSpecular = true;

                // Reuse existing specular texture, or create new one.
                TextureSpecular = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureSpecular, ImageInfo,
                    TextureSpecularPackage, TextureSpecularName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World );

                // Create specular sampling expression, if needed.
                if ( !ExpressionSpecular )
                {
                    ExpressionSpecular = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional );
                }

                // Record generating parameter.
                ExpressionSpecular->Desc = GeneratingParameterName;
                ExpressionSpecular->ParameterName = *GeneratingParameterName;

                ExpressionSpecular->Texture = TextureSpecular;
                ExpressionSpecular->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionSpecular->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
                ExpressionSpecular->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionSpecular );
                Material->Specular.Expression = ExpressionSpecular;

                bExpressionCreated = true;
            }
        }
    }

    int32 ParmNameSpecularColorIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_COLOR_SPECULAR_0, NodeParamNames );

    if( ParmNameSpecularColorIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_COLOR_SPECULAR_0 );
    }
    else
    {
        ParmNameSpecularColorIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_COLOR_SPECULAR_1, NodeParamNames );

        if ( ParmNameSpecularColorIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_COLOR_SPECULAR_1 );
    }

    if ( !bExpressionCreated && ParmNameSpecularColorIdx >= 0 )
    {
        // Specular color is available.

        FLinearColor Color = FLinearColor::White;
        const HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNameSpecularColorIdx ];

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float*) &Color.R,
            ParmInfo.floatValuesIndex, ParmInfo.size ) == HAPI_RESULT_SUCCESS )
        {
            if ( ParmInfo.size == 3 )
                Color.A = 1.0f;

            UMaterialExpressionVectorParameter * ExpressionSpecularColor =
                Cast< UMaterialExpressionVectorParameter >( Material->Specular.Expression );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionSpecularColor )
            {
                // Otherwise new expression is of a different type.
                if ( Material->Specular.Expression )
                {
                    Material->Specular.Expression->ConditionalBeginDestroy();
                    Material->Specular.Expression = nullptr;
                }

                ExpressionSpecularColor = NewObject< UMaterialExpressionVectorParameter >(
                    Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, RF_Transactional );
            }

            // Record generating parameter.
            ExpressionSpecularColor->Desc = GeneratingParameterName;
            ExpressionSpecularColor->ParameterName = *GeneratingParameterName;

            ExpressionSpecularColor->DefaultValue = Color;

            // Offset node placement.
            ExpressionSpecularColor->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
            ExpressionSpecularColor->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionSpecularColor );
            Material->Specular.Expression = ExpressionSpecularColor;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineUtils::CreateMaterialComponentRoughness(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY)
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Roughness texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if roughness texture is available.
    int32 ParmNameRoughnessIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_0, NodeParamNames );

    if ( ParmNameRoughnessIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_0 );
    }
    else
    {
        ParmNameRoughnessIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_1, NodeParamNames );

        if ( ParmNameRoughnessIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_1 );
    }

    if ( ParmNameRoughnessIdx >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ ParmNameRoughnessIdx ].id, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D* ExpressionRoughness =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Roughness.Expression );

            UTexture2D* TextureRoughness = nullptr;
            if ( ExpressionRoughness )
            {
                TextureRoughness = Cast< UTexture2D >( ExpressionRoughness->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Roughness.Expression )
                {
                    Material->Roughness.Expression->ConditionalBeginDestroy();
                    Material->Roughness.Expression = nullptr;
                }
            }

            UPackage * TextureRoughnessPackage = nullptr;
            if ( TextureRoughness )
                TextureRoughnessPackage = Cast< UPackage >( TextureRoughness->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                MaterialInfo.id, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureRoughnessName;
                bool bCreatedNewTextureRoughness = false;

                // Create roughness texture package, if this is a new roughness texture.
                if ( !TextureRoughnessPackage )
                {
                    TextureRoughnessPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniAssetComponent,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
                        TextureRoughnessName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                }

                // Create roughness texture, if we need to create one.
                if ( !TextureRoughness )
                    bCreatedNewTextureRoughness = true;

                // Reuse existing roughness texture, or create new one.
                TextureRoughness = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureRoughness, ImageInfo,
                    TextureRoughnessPackage, TextureRoughnessName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World );

                // Create roughness sampling expression, if needed.
                if ( !ExpressionRoughness )
                    ExpressionRoughness = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional );

                // Record generating parameter.
                ExpressionRoughness->Desc = GeneratingParameterName;
                ExpressionRoughness->ParameterName = *GeneratingParameterName;

                ExpressionRoughness->Texture = TextureRoughness;
                ExpressionRoughness->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionRoughness->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
                ExpressionRoughness->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionRoughness );
                Material->Roughness.Expression = ExpressionRoughness;

                bExpressionCreated = true;
            }
        }
    }

    int32 ParmNameRoughnessValueIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_0, NodeParamNames );

    if ( ParmNameRoughnessValueIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_0 );
    }
    else
    {
        ParmNameRoughnessValueIdx =
            FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_1, NodeParamNames );

        if ( ParmNameRoughnessValueIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_1 );
    }

    if ( !bExpressionCreated && ParmNameRoughnessValueIdx >= 0 )
    {
        // Roughness value is available.

        float RoughnessValue = 0.0f;
        const HAPI_ParmInfo& ParmInfo = NodeParams[ ParmNameRoughnessValueIdx ];

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float *) &RoughnessValue,
            ParmInfo.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            UMaterialExpressionScalarParameter * ExpressionRoughnessValue =
                Cast< UMaterialExpressionScalarParameter >( Material->Roughness.Expression );

            // Clamp retrieved value.
            RoughnessValue = FMath::Clamp< float >( RoughnessValue, 0.0f, 1.0f );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionRoughnessValue )
            {
                // Otherwise new expression is of a different type.
                if ( Material->Roughness.Expression )
                {
                    Material->Roughness.Expression->ConditionalBeginDestroy();
                    Material->Roughness.Expression = nullptr;
                }

                ExpressionRoughnessValue = NewObject< UMaterialExpressionScalarParameter >(
                    Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, RF_Transactional );
            }

            // Record generating parameter.
            ExpressionRoughnessValue->Desc = GeneratingParameterName;
            ExpressionRoughnessValue->ParameterName = *GeneratingParameterName;

            ExpressionRoughnessValue->DefaultValue = RoughnessValue;
            ExpressionRoughnessValue->SliderMin = 0.0f;
            ExpressionRoughnessValue->SliderMax = 1.0f;

            // Offset node placement.
            ExpressionRoughnessValue->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
            ExpressionRoughnessValue->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionRoughnessValue );
            Material->Roughness.Expression = ExpressionRoughnessValue;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineUtils::CreateMaterialComponentMetallic(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams,
    const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY)
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Metallic texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if metallic texture is available.
    int32 ParmNameMetallicIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_MAP_METALLIC, NodeParamNames );

    if ( ParmNameMetallicIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_METALLIC );
    }

    if ( ParmNameMetallicIdx >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ ParmNameMetallicIdx ].id, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionMetallic =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Metallic.Expression );

            UTexture2D * TextureMetallic = nullptr;
            if ( ExpressionMetallic )
            {
                TextureMetallic = Cast< UTexture2D >( ExpressionMetallic->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Metallic.Expression )
                {
                    Material->Metallic.Expression->ConditionalBeginDestroy();
                    Material->Metallic.Expression = nullptr;
                }
            }

            UPackage * TextureMetallicPackage = nullptr;
            if ( TextureMetallic )
                TextureMetallicPackage = Cast< UPackage >( TextureMetallic->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId,
                MaterialInfo.id, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureMetallicName;
                bool bCreatedNewTextureMetallic = false;

                // Create metallic texture package, if this is a new metallic texture.
                if ( !TextureMetallicPackage )
                {
                    TextureMetallicPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniAssetComponent,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
                        TextureMetallicName, FHoudiniEngineUtils::EBakeMode::Intermediate );
                }

                // Create metallic texture, if we need to create one.
                if ( !TextureMetallic )
                    bCreatedNewTextureMetallic = true;

                // Reuse existing metallic texture, or create new one.
                TextureMetallic = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureMetallic, ImageInfo,
                    TextureMetallicPackage, TextureMetallicName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World );

                // Create metallic sampling expression, if needed.
                if ( !ExpressionMetallic )
                    ExpressionMetallic = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, RF_Transactional );

                // Record generating parameter.
                ExpressionMetallic->Desc = GeneratingParameterName;
                ExpressionMetallic->ParameterName = *GeneratingParameterName;

                ExpressionMetallic->Texture = TextureMetallic;
                ExpressionMetallic->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionMetallic->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
                ExpressionMetallic->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionMetallic );
                Material->Metallic.Expression = ExpressionMetallic;

                bExpressionCreated = true;
            }
        }
    }

    int32 ParmNameMetallicValueIdx =
        FHoudiniEngineUtils::HapiFindParameterByName( HAPI_UNREAL_PARAM_VALUE_METALLIC, NodeParamNames );

    if ( ParmNameMetallicValueIdx >= 0 )
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_METALLIC );

    if ( !bExpressionCreated && ParmNameMetallicValueIdx >= 0 )
    {
        // Metallic value is available.

        float MetallicValue = 0.0f;
        const HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNameMetallicValueIdx ];

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float *) &MetallicValue,
            ParmInfo.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            UMaterialExpressionScalarParameter * ExpressionMetallicValue =
                Cast< UMaterialExpressionScalarParameter >( Material->Metallic.Expression );

            // Clamp retrieved value.
            MetallicValue = FMath::Clamp< float >( MetallicValue, 0.0f, 1.0f );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionMetallicValue )
            {
                // Otherwise new expression is of a different type.
                if ( Material->Metallic.Expression )
                {
                    Material->Metallic.Expression->ConditionalBeginDestroy();
                    Material->Metallic.Expression = nullptr;
                }

                ExpressionMetallicValue = NewObject< UMaterialExpressionScalarParameter >(
                    Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, RF_Transactional );
            }

            // Record generating parameter.
            ExpressionMetallicValue->Desc = GeneratingParameterName;
            ExpressionMetallicValue->ParameterName = *GeneratingParameterName;

            ExpressionMetallicValue->DefaultValue = MetallicValue;
            ExpressionMetallicValue->SliderMin = 0.0f;
            ExpressionMetallicValue->SliderMax = 1.0f;

            // Offset node placement.
            ExpressionMetallicValue->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
            ExpressionMetallicValue->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionMetallicValue );
            Material->Metallic.Expression = ExpressionMetallicValue;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}


bool
FHoudiniEngineUtils::CreateMaterialComponentEmissive(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams,
    const TArray< std::string > & NodeParamNames, int32 & MaterialNodeY )
{
    return true;
}

#endif

char *
FHoudiniEngineUtils::ExtractRawName( const FString & Name )
{
    if ( !Name.IsEmpty() )
    {
        std::string ConvertedString = TCHAR_TO_UTF8( *Name );

        // Allocate space for unique string.
        int32 UniqueNameBytes = ConvertedString.size() + 1;
        char * UniqueName = static_cast< char * >( FMemory::Malloc( UniqueNameBytes ) );

        FMemory::Memzero( UniqueName, UniqueNameBytes );
        FMemory::Memcpy( UniqueName, ConvertedString.c_str(), ConvertedString.size() );

        return UniqueName;
    }

    return nullptr;
}

void
FHoudiniEngineUtils::CreateFaceMaterialArray(
    const TArray< FStaticMaterial > & Materials, const TArray< int32 > & FaceMaterialIndices,
    TArray< char * > & OutStaticMeshFaceMaterials )
{
    // We need to create list of unique materials.
    TArray< char * > UniqueMaterialList;
    UMaterialInterface * MaterialInterface;
    char * UniqueName = nullptr;

    if ( Materials.Num() )
    {
        // We have materials.
        for ( int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx )
        {
            UniqueName = nullptr;
            MaterialInterface = Materials[ MaterialIdx ].MaterialInterface;

            if ( !MaterialInterface )
            {
                // Null material interface found, add default instead.
                MaterialInterface = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
            }

            FString FullMaterialName = MaterialInterface->GetPathName();
            UniqueName = FHoudiniEngineUtils::ExtractRawName( FullMaterialName );
            UniqueMaterialList.Add( UniqueName );
        }
    }
    else
    {
        // We do not have any materials, add default.
        MaterialInterface = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
        FString FullMaterialName = MaterialInterface->GetPathName();
        UniqueName = FHoudiniEngineUtils::ExtractRawName( FullMaterialName );
        UniqueMaterialList.Add( UniqueName );
    }

    for ( int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx )
    {
        int32 FaceMaterialIdx = FaceMaterialIndices[ FaceIdx ];
        check( FaceMaterialIdx < UniqueMaterialList.Num() );

        OutStaticMeshFaceMaterials.Add( UniqueMaterialList[ FaceMaterialIdx ] );
    }
}

void
FHoudiniEngineUtils::DeleteFaceMaterialArray( TArray< char * > & OutStaticMeshFaceMaterials )
{
    TSet< char * > UniqueMaterials( OutStaticMeshFaceMaterials );
    for ( TSet< char * >::TIterator Iter = UniqueMaterials.CreateIterator(); Iter; ++Iter )
    {
        char* MaterialName = *Iter;
        FMemory::Free( MaterialName );
    }

    OutStaticMeshFaceMaterials.Empty();
}

void
FHoudiniEngineUtils::ExtractStringPositions( const FString & Positions, TArray< FVector > & OutPositions )
{
    TArray< FString > PointStrings;

    static const TCHAR * PositionSeparators[] =
    {
        TEXT( " " ),
        TEXT( "," ),
    };

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    int32 NumCoords = Positions.ParseIntoArray( PointStrings, PositionSeparators, 2 );
    for ( int32 CoordIdx = 0; CoordIdx < NumCoords; CoordIdx += 3 )
    {
        FVector Position;

        Position.X = FCString::Atof( *PointStrings[ CoordIdx + 0 ] );
        Position.Y = FCString::Atof( *PointStrings[ CoordIdx + 1 ] );
        Position.Z = FCString::Atof( *PointStrings[ CoordIdx + 2 ] );

        Position *= GeneratedGeometryScaleFactor;

        if ( ImportAxis == HRSAI_Unreal )
        {
            Swap( Position.Y, Position.Z );
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            // No action required.
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }

        OutPositions.Add( Position );
    }
}

void
FHoudiniEngineUtils::CreatePositionsString( const TArray< FVector > & Positions, FString & PositionString )
{
    PositionString = TEXT( "" );

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    for ( int32 Idx = 0; Idx < Positions.Num(); ++Idx )
    {
        FVector Position = Positions[ Idx ];

        if ( ImportAxis == HRSAI_Unreal )
        {
            Swap( Position.Z, Position.Y );
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            // No action required.
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }

        if ( GeneratedGeometryScaleFactor != 0.0f )
            Position /= GeneratedGeometryScaleFactor;

        PositionString += FString::Printf( TEXT( "%f, %f, %f " ), Position.X, Position.Y, Position.Z );
    }
}

void
FHoudiniEngineUtils::ConvertScaleAndFlipVectorData( const TArray< float > & DataRaw, TArray< FVector > & DataOut )
{
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    for ( int32 Idx = 0; Idx < DataRaw.Num(); Idx += 3 )
    {
        FVector Point( DataRaw[ Idx + 0 ], DataRaw[ Idx + 1 ], DataRaw[ Idx + 2 ] );

        Point *= GeneratedGeometryScaleFactor;

        if ( ImportAxis == HRSAI_Unreal )
        {
            Swap(Point.Z, Point.Y);
        }
        else if ( ImportAxis == HRSAI_Houdini )
        {
            // No action required.
        }
        else
        {
            // Not valid enum value.
            check( 0 );
        }

        DataOut.Add( Point );
    }
}

FString
FHoudiniEngineUtils::HoudiniGetLibHAPIName()
{
    static const FString LibHAPIName =

#if PLATFORM_WINDOWS

        HAPI_LIB_OBJECT_WINDOWS;

#elif PLATFORM_MAC

        HAPI_LIB_OBJECT_MAC;

#elif PLATFORM_LINUX

        HAPI_LIB_OBJECT_LINUX;

#else

        TEXT( "" );

#endif

    return LibHAPIName;
}

#if PLATFORM_WINDOWS

void *
FHoudiniEngineUtils::LocateLibHAPIInRegistry(
    const FString & HoudiniInstallationType,
    const FString & HoudiniVersionString,
    FString & StoredLibHAPILocation )
{
    FString HoudiniRegistryLocation = FString::Printf(
        TEXT( "Software\\Side Effects Software\\%s %s" ), *HoudiniInstallationType, *HoudiniVersionString );

    FString HoudiniInstallationPath;

    if ( FWindowsPlatformMisc::QueryRegKey(
        HKEY_LOCAL_MACHINE, *HoudiniRegistryLocation, TEXT( "InstallPath" ), HoudiniInstallationPath ) )
    {
        HoudiniInstallationPath += FString::Printf( TEXT( "/%s" ), HAPI_HFS_SUBFOLDER_WINDOWS );

        // Create full path to libHAPI binary.
        FString LibHAPIPath = FString::Printf( TEXT( "%s/%s" ), *HoudiniInstallationPath, HAPI_LIB_OBJECT_WINDOWS );

        if ( FPaths::FileExists( LibHAPIPath ) )
        {
            FPlatformProcess::PushDllDirectory( *HoudiniInstallationPath );
            void* HAPILibraryHandle = FPlatformProcess::GetDllHandle( HAPI_LIB_OBJECT_WINDOWS );
            FPlatformProcess::PopDllDirectory( *HoudiniInstallationPath );

            if ( HAPILibraryHandle )
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Loaded %s from Registry path %s" ), HAPI_LIB_OBJECT_WINDOWS,
                    *HoudiniInstallationPath );

                StoredLibHAPILocation = HoudiniInstallationPath;
                return HAPILibraryHandle;
            }
        }
    }

    return nullptr;
}

#endif

void *
FHoudiniEngineUtils::LoadLibHAPI( FString & StoredLibHAPILocation )
{
    FString HFSPath = TEXT( "" );
    void * HAPILibraryHandle = nullptr;

    // Before doing anything platform specific, check if HFS environment variable is defined.
    TCHAR HFS_ENV_VARIABLE[ MAX_PATH ];
    FMemory::Memzero( &HFS_ENV_VARIABLE[ 0 ], sizeof( TCHAR ) * MAX_PATH );

    // Look up HAPI_PATH environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
    FPlatformMisc::GetEnvironmentVariable( TEXT( "HAPI_PATH" ), HFS_ENV_VARIABLE, MAX_PATH );
    if ( *HFS_ENV_VARIABLE )
        HFSPath = &HFS_ENV_VARIABLE[ 0 ];

    // Look up environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
    FPlatformMisc::GetEnvironmentVariable( TEXT( "HFS" ), HFS_ENV_VARIABLE, MAX_PATH );
    if ( *HFS_ENV_VARIABLE )
        HFSPath = &HFS_ENV_VARIABLE[ 0 ];

    // Get platform specific name of libHAPI.
    FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

    // If we have a custom location specified through settings, attempt to use that.
    bool bCustomPathFound = false;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings && HoudiniRuntimeSettings->bUseCustomHoudiniLocation )
    {
        // Create full path to libHAPI binary.
        FString CustomHoudiniLocationPath = HoudiniRuntimeSettings->CustomHoudiniLocation.Path;
        if ( !CustomHoudiniLocationPath.IsEmpty() )
        {
            // Convert path to absolute if it is relative.
            if ( FPaths::IsRelative( CustomHoudiniLocationPath ) )
                CustomHoudiniLocationPath = FPaths::ConvertRelativePathToFull( CustomHoudiniLocationPath );

            FString LibHAPICustomPath = FString::Printf( TEXT( "%s/%s" ), *CustomHoudiniLocationPath, *LibHAPIName );

            if ( FPaths::FileExists( LibHAPICustomPath ) )
            {
                HFSPath = CustomHoudiniLocationPath;
                bCustomPathFound = true;
            }
        }
    }

    // We have HFS environment variable defined (or custom location), attempt to load libHAPI from it.
    if ( !HFSPath.IsEmpty() )
    {
        if ( !bCustomPathFound )
        {

#if PLATFORM_WINDOWS

            HFSPath += FString::Printf( TEXT( "/%s" ), HAPI_HFS_SUBFOLDER_WINDOWS );

#elif PLATFORM_MAC

            HFSPath += FString::Printf( TEXT( "/%s" ), HAPI_HFS_SUBFOLDER_MAC );

#elif PLATFORM_LINUX

            HFSPath += FString::Printf( TEXT( "/%s" ), HAPI_HFS_SUBFOLDER_LINUX );

#endif
        }

        // Create full path to libHAPI binary.
        FString LibHAPIPath = FString::Printf( TEXT( "%s/%s" ), *HFSPath, *LibHAPIName );

        if ( FPaths::FileExists( LibHAPIPath ) )
        {
            // libHAPI binary exists at specified location, attempt to load it.
            FPlatformProcess::PushDllDirectory( *HFSPath );

#if PLATFORM_WINDOWS

            HAPILibraryHandle = FPlatformProcess::GetDllHandle( *LibHAPIName );

#elif PLATFORM_MAC || PLATFORM_LINUX

            HAPILibraryHandle = FPlatformProcess::GetDllHandle( *LibHAPIPath );

#endif

            FPlatformProcess::PopDllDirectory( *HFSPath );

            // If library has been loaded successfully we can stop.
            if ( HAPILibraryHandle )
            {
                if ( bCustomPathFound )
                    HOUDINI_LOG_MESSAGE( TEXT( "Loaded %s from custom path %s" ), *LibHAPIName, *HFSPath );
                else
                    HOUDINI_LOG_MESSAGE( TEXT( "Loaded %s from HFS environment path %s" ), *LibHAPIName, *HFSPath );

                StoredLibHAPILocation = HFSPath;
                return HAPILibraryHandle;
            }
        }
    }

    // Compute Houdini version string.
    FString HoudiniVersionString = FString::Printf(
        TEXT( "%d.%d.%d" ), HAPI_VERSION_HOUDINI_MAJOR,
        HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD );

    // If we have a patch version, we need to append it.
    if ( HAPI_VERSION_HOUDINI_PATCH > 0 )
        HoudiniVersionString = FString::Printf( TEXT( "%s.%d" ), *HoudiniVersionString, HAPI_VERSION_HOUDINI_PATCH );

    // Otherwise, we will attempt to detect Houdini installation.

#if PLATFORM_WINDOWS

    // On Windows, we have also hardcoded HFS path in plugin configuration file; attempt to load from it.
    HFSPath = HOUDINI_ENGINE_HFS_PATH;

    if ( !HFSPath.IsEmpty() )
    {
        HFSPath += FString::Printf( TEXT( "/%s" ), HAPI_HFS_SUBFOLDER_WINDOWS );

        // Create full path to libHAPI binary.
        FString LibHAPIPath = FString::Printf( TEXT( "%s/%s" ), *HFSPath, *LibHAPIName );

        if ( FPaths::FileExists( LibHAPIPath ) )
        {
            FPlatformProcess::PushDllDirectory( *HFSPath );
            HAPILibraryHandle = FPlatformProcess::GetDllHandle( *LibHAPIName );
            FPlatformProcess::PopDllDirectory( *HFSPath );

            if ( HAPILibraryHandle )
            {
                HOUDINI_LOG_MESSAGE( TEXT( "Loaded %s from Plugin defined HFS path %s" ), *LibHAPIName, *HFSPath );
                StoredLibHAPILocation = HFSPath;
                return HAPILibraryHandle;
            }
        }
    }

    // As a second attempt, on Windows, we try to look up location of Houdini Engine in the registry.
    HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
        TEXT( "Houdini Engine" ), HoudiniVersionString, StoredLibHAPILocation );
    if ( HAPILibraryHandle )
        return HAPILibraryHandle;

    // As a third attempt, we try to look up location of Houdini installation (not Houdini Engine) in the registry.
    HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
        TEXT( "Houdini" ), HoudiniVersionString, StoredLibHAPILocation );
    if ( HAPILibraryHandle )
        return HAPILibraryHandle;

    // As a fourth attempt, we will try to load from hardcoded program files path.
    FString HoudiniLocation = FString::Printf(
        TEXT( "C:\\Program Files\\Side Effects Software\\Houdini %s\\%s" ), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_WINDOWS );

#else

#   if PLATFORM_MAC

    // Attempt to load from standard Mac OS X installation.
    FString HoudiniLocation = FString::Printf(
        TEXT( "/Library/Frameworks/Houdini.framework/Versions/%s/Libraries" ), *HoudiniVersionString );

#   elif PLATFORM_LINUX

    // Attempt to load from standard Linux installation.
    // TODO: Support this.
    //FString HoudiniLocation = FString::Printf(
        //TEXT( "/opt/dev%s/" ) + HAPI_HFS_SUBFOLDER_LINUX, *HoudiniVersionString );
    FString HoudiniLocation = HOUDINI_ENGINE_HFS_PATH;

#   endif

#endif

    // Create full path to libHAPI binary.
    FString LibHAPIPath = FString::Printf( TEXT( "%s/%s" ), *HoudiniLocation, *LibHAPIName );

    if ( FPaths::FileExists( LibHAPIPath ) )
    {
        FPlatformProcess::PushDllDirectory( *HoudiniLocation );
        HAPILibraryHandle = FPlatformProcess::GetDllHandle( *LibHAPIPath );
        FPlatformProcess::PopDllDirectory( *HoudiniLocation );

        if ( HAPILibraryHandle )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Loaded %s from expected installation %s" ), *LibHAPIName, *HoudiniLocation );
            StoredLibHAPILocation = HoudiniLocation;
            return HAPILibraryHandle;
        }
    }

    StoredLibHAPILocation = TEXT( "" );
    return HAPILibraryHandle;
}

int32
FHoudiniEngineUtils::HapiGetVertexListForGroup(
    HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
    HAPI_PartId PartId, const FString & GroupName,
    const TArray< int32 > & FullVertexList, TArray< int32 > & NewVertexList,
    TArray< int32 > & AllVertexList, TArray< int32 > & AllFaceList,
    TArray< int32 > & AllCollisionFaceIndices )
{
    NewVertexList.Init( -1, FullVertexList.Num() );
    int32 ProcessedWedges = 0;

    AllFaceList.Empty();

    TArray< int32 > PartGroupMembership;
    FHoudiniEngineUtils::HapiGetGroupMembership(
        AssetId, ObjectId, GeoId, PartId, HAPI_GROUPTYPE_PRIM, GroupName, PartGroupMembership );

    // Go through all primitives.
    for ( int32 FaceIdx = 0; FaceIdx < PartGroupMembership.Num(); ++FaceIdx )
    {
        if ( PartGroupMembership[ FaceIdx ] > 0 )
        {
            // Add face.
            AllFaceList.Add( FaceIdx );

            // This face is a member of specified group.
            NewVertexList[ FaceIdx * 3 + 0 ] = FullVertexList[ FaceIdx * 3 + 0 ];
            NewVertexList[ FaceIdx * 3 + 1 ] = FullVertexList[ FaceIdx * 3 + 1 ];
            NewVertexList[ FaceIdx * 3 + 2 ] = FullVertexList[ FaceIdx * 3 + 2 ];

            // Mark these vertex indices as used.
            AllVertexList[ FaceIdx * 3 + 0 ] = 1;
            AllVertexList[ FaceIdx * 3 + 1 ] = 1;
            AllVertexList[ FaceIdx * 3 + 2 ] = 1;

            // Mark this face as used.
            AllCollisionFaceIndices[ FaceIdx ] = 1;

            ProcessedWedges += 3;
        }
    }

    return ProcessedWedges;
}


#if WITH_EDITOR

bool
FHoudiniEngineUtils::ContainsInvalidLightmapFaces( const FRawMesh & RawMesh, int32 LightmapSourceIdx )
{
    const TArray< FVector2D > & LightmapUVs = RawMesh.WedgeTexCoords[ LightmapSourceIdx ];
    const TArray< uint32 > & Indices = RawMesh.WedgeIndices;

    if ( LightmapUVs.Num() != Indices.Num() )
    {
        // This is invalid raw mesh; by design we consider that it contains invalid lightmap faces.
        return true;
    }

    for ( int32 Idx = 0; Idx < Indices.Num(); Idx += 3 )
    {
        const FVector2D & uv0 = LightmapUVs[ Idx + 0 ];
        const FVector2D & uv1 = LightmapUVs[ Idx + 1 ];
        const FVector2D & uv2 = LightmapUVs[ Idx + 2 ];

        if ( uv0 == uv1 && uv1 == uv2 )
        {
            // Detect invalid lightmap face, can stop.
            return true;
        }
    }

    // Otherwise there are no invalid lightmap faces.
    return false;
}

#endif

int32
FHoudiniEngineUtils::CountUVSets( const FRawMesh & RawMesh )
{
    int32 UVSetCount = 0;

#if WITH_EDITOR

    for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_MESH_TEXTURE_COORDS; ++TexCoordIdx )
    {
        const TArray< FVector2D > & WedgeTexCoords = RawMesh.WedgeTexCoords[ TexCoordIdx ];
        if ( WedgeTexCoords.Num() > 0 )
            UVSetCount++;
    }

#endif // WITH_EDITOR

    return UVSetCount;
}

const FString
FHoudiniEngineUtils::GetStatusString( HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity )
{
    int32 StatusBufferLength = 0;
    FHoudiniApi::GetStatusStringBufLength( FHoudiniEngine::Get().GetSession(), status_type, verbosity, &StatusBufferLength );

    if ( StatusBufferLength > 0 )
    {
        TArray< char > StatusStringBuffer;
        StatusStringBuffer.SetNumZeroed( StatusBufferLength );
        FHoudiniApi::GetStatusString( FHoudiniEngine::Get().GetSession(), status_type, &StatusStringBuffer[ 0 ], StatusBufferLength );

        return FString( UTF8_TO_TCHAR( &StatusStringBuffer[ 0 ] ) );
    }

    return FString( TEXT( ""  ));
}

bool
FHoudiniEngineUtils::ExtractUniqueMaterialIds(
    const HAPI_AssetInfo & AssetInfo,
    TSet< HAPI_MaterialId > & MaterialIds,
    TSet< HAPI_MaterialId > & InstancerMaterialIds,
    TMap< FHoudiniGeoPartObject, HAPI_MaterialId > & InstancerMaterialMap )
{
    // Empty passed incontainers.
    MaterialIds.Empty();
    InstancerMaterialIds.Empty();
    InstancerMaterialMap.Empty();

    TArray< HAPI_ObjectInfo > ObjectInfos;
    ObjectInfos.SetNumUninitialized( AssetInfo.objectCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetObjects(
        FHoudiniEngine::Get().GetSession(), AssetInfo.id,
        &ObjectInfos[0], 0, AssetInfo.objectCount ), false );

    // Iterate through all objects.
    for ( int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx )
    {
        // Retrieve object at this index.
        const HAPI_ObjectInfo & ObjectInfo = ObjectInfos[ ObjectIdx ];

        // Iterate through all geos.
        for ( int32 GeoIdx = 0; GeoIdx < ObjectInfo.geoCount; ++GeoIdx )
        {
            // Get Geo information.
            HAPI_GeoInfo GeoInfo;
            if ( FHoudiniApi::GetGeoInfo(
                FHoudiniEngine::Get().GetSession(), AssetInfo.id,
                ObjectInfo.id, GeoIdx, &GeoInfo ) != HAPI_RESULT_SUCCESS )
            {
                continue;
            }

            // Iterate through all parts.
            for ( int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx )
            {
                // Get part information.
                HAPI_PartInfo PartInfo;
                FString PartName = TEXT( "" );

                if ( FHoudiniApi::GetPartInfo(
                    FHoudiniEngine::Get().GetSession(), AssetInfo.id,
                    ObjectInfo.id, GeoInfo.id, PartIdx, &PartInfo ) != HAPI_RESULT_SUCCESS )
                {
                    continue;
                }

                // Retrieve material information for this geo part.
                HAPI_Bool bSingleFaceMaterial = false;
                bool bMaterialsFound = false;

                if ( PartInfo.faceCount > 0 )
                {
                    TArray< HAPI_MaterialId > FaceMaterialIds;
                    FaceMaterialIds.SetNumUninitialized( PartInfo.faceCount );

                    if ( FHoudiniApi::GetMaterialIdsOnFaces(
                        FHoudiniEngine::Get().GetSession(),
                        AssetInfo.id, ObjectInfo.id, GeoInfo.id, PartInfo.id, &bSingleFaceMaterial,
                        &FaceMaterialIds[0], 0, PartInfo.faceCount ) != HAPI_RESULT_SUCCESS )
                    {
                        continue;
                    }

                    MaterialIds.Append( FaceMaterialIds );
                }
                else
                {
                    // If this is an instancer, attempt to look up instancer material.
                    if ( ObjectInfo.isInstancer )
                    {
                        HAPI_MaterialId InstanceMaterialId = -1;

                        if ( FHoudiniApi::GetMaterialIdsOnFaces(
                            FHoudiniEngine::Get().GetSession(),
                            AssetInfo.id, ObjectInfo.id, GeoInfo.id, PartInfo.id, &bSingleFaceMaterial,
                            &InstanceMaterialId, 0, 1 ) != HAPI_RESULT_SUCCESS )
                        {
                            continue;
                        }

                        MaterialIds.Add( InstanceMaterialId );

                        if ( InstanceMaterialId != -1 )
                        {
                            FHoudiniGeoPartObject GeoPartObject( AssetInfo.id, ObjectInfo.id, GeoInfo.id, PartInfo.id );
                            InstancerMaterialMap.Add( GeoPartObject, InstanceMaterialId );

                            InstancerMaterialIds.Add( InstanceMaterialId );
                        }
                    }
                }
            }
        }
    }

    MaterialIds.Remove( -1 );

    return true;
}

UMaterialExpression *
FHoudiniEngineUtils::MaterialLocateExpression( UMaterialExpression * Expression, UClass * ExpressionClass )
{
    if ( !Expression )
        return nullptr;

    if ( ExpressionClass == Expression->GetClass() )
        return Expression;

#if WITH_EDITOR
    // If this is a channel multiply expression, we can recurse.
    UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast< UMaterialExpressionMultiply >( Expression );
    if ( MaterialExpressionMultiply )
    {
        {
            UMaterialExpression * MaterialExpression = MaterialExpressionMultiply->A.Expression;
            if ( MaterialExpression )
            {
                if ( MaterialExpression->GetClass() == ExpressionClass )
                    return MaterialExpression;

                MaterialExpression = FHoudiniEngineUtils::MaterialLocateExpression(
                    Cast< UMaterialExpressionMultiply >( MaterialExpression ), ExpressionClass );

                if ( MaterialExpression )
                    return MaterialExpression;
            }
        }

        {
            UMaterialExpression * MaterialExpression = MaterialExpressionMultiply->B.Expression;
            if ( MaterialExpression )
            {
                if ( MaterialExpression->GetClass() == ExpressionClass )
                    return MaterialExpression;

                MaterialExpression = FHoudiniEngineUtils::MaterialLocateExpression(
                    Cast< UMaterialExpressionMultiply >( MaterialExpression ), ExpressionClass );

                if ( MaterialExpression )
                    return MaterialExpression;
            }
        }
    }
#endif

    return nullptr;
}

AHoudiniAssetActor * 
FHoudiniEngineUtils::LocateClipboardActor( const AActor* IgnoreActor, const FString & ClipboardText )
{
    const TCHAR * Paste = nullptr;

    if ( ClipboardText.IsEmpty() )
    {
        FString PasteString;
        FPlatformMisc::ClipboardPaste( PasteString );
        Paste = *PasteString;
    }
    else
    {
        Paste = *ClipboardText;
    }

    AHoudiniAssetActor * HoudiniAssetActor = nullptr;
    
    // Extract the Name/Label of the from the clipboard string ...
    FString ActorName = TEXT( "" );
    FString StrLine;
    while ( FParse::Line( &Paste, StrLine ) )
    {
        StrLine = StrLine.Trim();

        const TCHAR * Str = *StrLine;
        FString ClassName;
	if (StrLine.StartsWith(TEXT("Begin Actor")) && FParse::Value(Str, TEXT("Class="), ClassName))
	{
	    if (ClassName == *AHoudiniAssetActor::StaticClass()->GetFName().ToString())
	    {
		if (FParse::Value(Str, TEXT("Name="), ActorName))
		    break;
	    }
	}
	else if (StrLine.StartsWith(TEXT("ActorLabel")))
	{
	    if (FParse::Value(Str, TEXT("ActorLabel="), ActorName))
		break;
	}
    }

#if WITH_EDITOR
    // And try to find the corresponding HoudiniAssetActor in the editor world
    // to avoid finding "deleted" assets with the same name
    UWorld* editorWorld = GEditor->GetEditorWorldContext().World();
    for (TActorIterator<AHoudiniAssetActor> ActorItr(editorWorld); ActorItr; ++ActorItr)
    {
	if ( *ActorItr != IgnoreActor && (ActorItr->GetActorLabel() == ActorName || ActorItr->GetName() == ActorName))
	    HoudiniAssetActor = *ActorItr;
    }
#endif

    return HoudiniAssetActor;
}

void
FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(
    USceneComponent * Component,
    const TArray< FTransform > & InstancedTransforms,
    const FRotator & RotationOffset, const FVector & ScaleOffset )
{
    UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( Component );
    UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>( Component );
    
    check( ISMC || IAC );

    auto ProcessOffsets = [&]() {
        TArray<FTransform> ProcessedTransforms;
        ProcessedTransforms.Reserve( InstancedTransforms.Num() );

        for ( int32 InstanceIdx = 0; InstanceIdx < InstancedTransforms.Num(); ++InstanceIdx )
        {
            FTransform Transform = InstancedTransforms[ InstanceIdx ];

            // Compute new rotation and scale.
            FQuat TransformRotation = Transform.GetRotation() * RotationOffset.Quaternion();
            FVector TransformScale3D = Transform.GetScale3D() * ScaleOffset;

            // Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances.
            // Happens in blueprint as well.
            if ( TransformScale3D.X < HAPI_UNREAL_SCALE_SMALL_VALUE )
                TransformScale3D.X = HAPI_UNREAL_SCALE_SMALL_VALUE;

            if ( TransformScale3D.Y < HAPI_UNREAL_SCALE_SMALL_VALUE )
                TransformScale3D.Y = HAPI_UNREAL_SCALE_SMALL_VALUE;

            if ( TransformScale3D.Z < HAPI_UNREAL_SCALE_SMALL_VALUE )
                TransformScale3D.Z = HAPI_UNREAL_SCALE_SMALL_VALUE;

            Transform.SetRotation( TransformRotation );
            Transform.SetScale3D( TransformScale3D );

            ProcessedTransforms.Add( Transform );
        }
        return ProcessedTransforms;
    };

    if ( ISMC )
    {
        ISMC->ClearInstances();
        for ( const auto& Transform : ProcessOffsets() )
        {
            ISMC->AddInstance( Transform );
        }
    }
    else if ( IAC )
    {
        IAC->SetInstances( ProcessOffsets() );
    }
}

bool
FHoudiniEngineUtils::GetAssetNames(
    UHoudiniAsset * HoudiniAsset, HAPI_AssetLibraryId & OutAssetLibraryId,
    TArray< HAPI_StringHandle > & OutAssetNames )
{
    OutAssetLibraryId = -1;
    OutAssetNames.Empty();

    if ( FHoudiniEngineUtils::IsInitialized() && HoudiniAsset )
    {
        const FString & AssetFileName = HoudiniAsset->GetAssetFileName();
        HAPI_Result Result = HAPI_RESULT_SUCCESS;
        HAPI_AssetLibraryId AssetLibraryId = -1;
        int32 AssetCount = 0;
        TArray< HAPI_StringHandle > AssetNames;

        if ( !AssetFileName.IsEmpty() && FPaths::FileExists( AssetFileName ) )
        {
            // File does exist, we can load asset from file.
            std::string AssetFileNamePlain;
            FHoudiniEngineUtils::ConvertUnrealString( AssetFileName, AssetFileNamePlain );

            Result = FHoudiniApi::LoadAssetLibraryFromFile(
                FHoudiniEngine::Get().GetSession(), AssetFileNamePlain.c_str(), true, &AssetLibraryId );
        }
        else
        {
            // Otherwise we will try to load from buffer we've cached.
            Result = FHoudiniApi::LoadAssetLibraryFromMemory(
                FHoudiniEngine::Get().GetSession(),
                reinterpret_cast< const char * >( HoudiniAsset->GetAssetBytes() ),
                HoudiniAsset->GetAssetBytesCount(), true, &AssetLibraryId );
        }

        if ( Result != HAPI_RESULT_SUCCESS )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Error loading asset library for %s" ), *AssetFileName );
            return false;
        }

        Result = FHoudiniApi::GetAvailableAssetCount( FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetCount );
        if ( Result != HAPI_RESULT_SUCCESS )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Error getting asset count for %s" ), *AssetFileName );
            return false;
        }

        AssetNames.SetNumUninitialized( AssetCount );

        Result = FHoudiniApi::GetAvailableAssets( FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetNames[ 0 ], AssetCount );
        if ( Result != HAPI_RESULT_SUCCESS )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Unable to retrieve asset names for %s" ), *AssetFileName );
            return false;
        }

        if ( !AssetCount )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "No assets found within %s" ), *AssetFileName );
            return false;
        }

        OutAssetLibraryId = AssetLibraryId;
        OutAssetNames = AssetNames;
    
        return true;
    }

    return false;
}

void
FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
    UPackage * Package, UObject * Object, const TCHAR * Key,
    const TCHAR * Value)
{
    UMetaData * MetaData = Package->GetMetaData();
    MetaData->SetValue( Object, Key, Value );
}

bool
FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(
    UPackage * Package, UObject * Object, FString & HoudiniName )
{
    UMetaData * MetaData = Package->GetMetaData();
    if ( MetaData && MetaData->HasValue( Object, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT ) )
    {
        // Retrieve name used for package generation.
        const FString NameFull = MetaData->GetValue( Object, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME );
        HoudiniName = NameFull.Left( NameFull.Len() - FHoudiniEngineUtils::PackageGUIDItemNameLength );

        return true;
    }

    return false;
}

#if WITH_EDITOR

bool
FHoudiniEngineUtils::StaticMeshRequiresBake( const UStaticMesh * StaticMesh )
{
    check( StaticMesh );
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
    
    FAssetData BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( *StaticMesh->GetPathName() );
    if ( ! BackingAssetData.IsUAsset() )
        return true;

    for ( const auto& StaticMaterial : StaticMesh->StaticMaterials )
    {
        if ( StaticMaterial.MaterialInterface )
        {
            BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( *StaticMaterial.MaterialInterface->GetPathName() );
            if ( ! BackingAssetData.IsUAsset() )
                return true;
        }
    }
    return false;
}

UStaticMesh *
FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(
    const UStaticMesh * StaticMesh, UHoudiniAssetComponent * Component,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode )
{
    UStaticMesh * DuplicatedStaticMesh = nullptr;

    if ( !HoudiniGeoPartObject.IsCurve() && !HoudiniGeoPartObject.IsInstancer() && !HoudiniGeoPartObject.IsPackedPrimitiveInstancer() )
    {
        // Create package for this duplicated mesh.
        FString MeshName;
        FGuid MeshGuid;

        UPackage * MeshPackage = FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(
            Component, HoudiniGeoPartObject, MeshName, MeshGuid, BakeMode );
        if( !MeshPackage )
            return nullptr;

        // Duplicate mesh for this new copied component.
        DuplicatedStaticMesh = DuplicateObject< UStaticMesh >( StaticMesh, MeshPackage, *MeshName );

        if ( BakeMode != EBakeMode::Intermediate )
            DuplicatedStaticMesh->SetFlags( RF_Public | RF_Standalone );

        // Add meta information.
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
            MeshPackage, DuplicatedStaticMesh,
            HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
            MeshPackage, DuplicatedStaticMesh,
            HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName );

        // See if we need to duplicate materials and textures.
        TArray< FStaticMaterial > DuplicatedMaterials;
        TArray< FStaticMaterial > & Materials = DuplicatedStaticMesh->StaticMaterials;

        for ( int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx )
        {
            UMaterialInterface* MaterialInterface = Materials[ MaterialIdx ].MaterialInterface;
            if ( MaterialInterface )
            {
                UPackage * MaterialPackage = Cast< UPackage >( MaterialInterface->GetOuter() );
                if ( MaterialPackage )
                {
                    FString MaterialName;
                    if ( FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(
                        MaterialPackage, MaterialInterface, MaterialName ) )
                    {
                        // We only deal with materials.
                        UMaterial * Material = Cast< UMaterial >( MaterialInterface );
                        if ( Material )
                        {
                            // Duplicate material resource.
                            UMaterial * DuplicatedMaterial = FHoudiniEngineUtils::DuplicateMaterialAndCreatePackage(
                                Material, Component, MaterialName, BakeMode );

                            if( !DuplicatedMaterial )
                                continue;

                            // Store duplicated material.
                            FStaticMaterial DupeStaticMaterial = Materials[ MaterialIdx ];
                            DupeStaticMaterial.MaterialInterface = DuplicatedMaterial;
                            DuplicatedMaterials.Add( DupeStaticMaterial );
                            continue;
                        }
                    }
                }
            }

            DuplicatedMaterials.Add( Materials[ MaterialIdx ] );
        }

        // Assign duplicated materials.
        DuplicatedStaticMesh->StaticMaterials = DuplicatedMaterials;

        // Notify registry that we have created a new duplicate mesh.
        FAssetRegistryModule::AssetCreated( DuplicatedStaticMesh );

        // Dirty the static mesh package.
        DuplicatedStaticMesh->MarkPackageDirty();
    }

    return DuplicatedStaticMesh;
}

UMaterial *
FHoudiniEngineUtils::DuplicateMaterialAndCreatePackage(
    UMaterial * Material, UHoudiniAssetComponent * Component,
    const FString & SubMaterialName, EBakeMode BakeMode )
{
    UMaterial * DuplicatedMaterial = nullptr;

    // Create material package.
    FString MaterialName;
    UPackage * MaterialPackage = FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
        Component, SubMaterialName, MaterialName, BakeMode );

    if( !MaterialPackage )
        return nullptr;

    // Clone material.
    DuplicatedMaterial = DuplicateObject< UMaterial >( Material, MaterialPackage, *MaterialName );

    if ( BakeMode != EBakeMode::Intermediate )
        DuplicatedMaterial->SetFlags( RF_Public | RF_Standalone );

    // Add meta information.
    FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
        MaterialPackage, DuplicatedMaterial,
        HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
    FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
        MaterialPackage, DuplicatedMaterial,
        HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName );

    // Retrieve and check various sampling expressions. If they contain textures, duplicate (and bake) them.
    
    for ( auto& Expression : DuplicatedMaterial->Expressions )
    {
        FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(
            Expression, Component, BakeMode );
    }

    // Notify registry that we have created a new duplicate material.
    FAssetRegistryModule::AssetCreated( DuplicatedMaterial );

    // Dirty the material package.
    DuplicatedMaterial->MarkPackageDirty();

    // Reset any derived state
    DuplicatedMaterial->ForceRecompileForRendering();

    return DuplicatedMaterial;
}

void
FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(
    UMaterialExpression * MaterialExpression,
    UHoudiniAssetComponent * Component, EBakeMode BakeMode )
{
    UMaterialExpressionTextureSample * TextureSample = Cast< UMaterialExpressionTextureSample >( MaterialExpression );
    if ( TextureSample )
    {
        UTexture2D * Texture = Cast< UTexture2D >( TextureSample->Texture );
        if ( Texture )
        {
            UPackage * TexturePackage = Cast< UPackage >( Texture->GetOuter() );
            if ( TexturePackage )
            {
                FString GeneratedTextureName;
                if ( FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(
                    TexturePackage, Texture, GeneratedTextureName ) )
                {
                    // Duplicate texture.
                    UTexture2D * DuplicatedTexture = FHoudiniEngineUtils::DuplicateTextureAndCreatePackage(
                        Texture, Component, GeneratedTextureName, BakeMode );

                    // Re-assign generated texture.
                    TextureSample->Texture = DuplicatedTexture;
                }
            }
        }
    }
}

UTexture2D *
FHoudiniEngineUtils::DuplicateTextureAndCreatePackage(
    UTexture2D * Texture, UHoudiniAssetComponent * Component,
    const FString & SubTextureName, EBakeMode BakeMode )
{
    UTexture2D* DuplicatedTexture = nullptr;

    // Retrieve original package of this texture.
    UPackage * TexturePackage = Cast< UPackage >( Texture->GetOuter() );
    if ( TexturePackage )
    {
        FString GeneratedTextureName;
        if ( FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation( TexturePackage, Texture, GeneratedTextureName ) )
        {
            UMetaData * MetaData = TexturePackage->GetMetaData();
            if ( MetaData )
            {
                // Retrieve texture type.
                const FString & TextureType =
                    MetaData->GetValue( Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE );

                // Create texture package.
                FString TextureName;
                UPackage * NewTexturePackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                    Component, SubTextureName, TextureType, TextureName, BakeMode );

                // Clone texture.
                DuplicatedTexture = DuplicateObject< UTexture2D >( Texture, NewTexturePackage, *TextureName );

                if ( BakeMode != EBakeMode::Intermediate )
                    DuplicatedTexture->SetFlags( RF_Public | RF_Standalone );

                // Add meta information.
                FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                    NewTexturePackage, DuplicatedTexture,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
                FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                    NewTexturePackage, DuplicatedTexture,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName );
                FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
                    NewTexturePackage, DuplicatedTexture,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType );

                // Notify registry that we have created a new duplicate texture.
                FAssetRegistryModule::AssetCreated( DuplicatedTexture );

                // Dirty the texture package.
                DuplicatedTexture->MarkPackageDirty();
            }
        }
    }

    return DuplicatedTexture;
}

void 
FHoudiniEngineUtils::BakeHoudiniActorToActors( UHoudiniAssetComponent * HoudiniAssetComponent, bool SelectNewActors )
{
    const FScopedTransaction Transaction( LOCTEXT( "BakeToActors", "Bake To Actors" ) );

    auto SMComponentToPart = HoudiniAssetComponent->CollectAllStaticMeshComponents();    
    TArray< AActor* > NewActors = BakeHoudiniActorToActors_StaticMeshes( HoudiniAssetComponent, SMComponentToPart );

    auto IAComponentToPart = HoudiniAssetComponent->CollectAllInstancedActorComponents();
    NewActors.Append( BakeHoudiniActorToActors_InstancedActors( HoudiniAssetComponent, IAComponentToPart ) );
    
    if ( SelectNewActors && NewActors.Num() )
    {
        GEditor->SelectNone( false, true );
        for ( AActor* NewActor : NewActors )
        {
            GEditor->SelectActor( NewActor, true, false );
        }
        GEditor->NoteSelectionChange();
    }
}

TArray< AActor* >
FHoudiniEngineUtils::BakeHoudiniActorToActors_InstancedActors(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    TMap< const UHoudiniInstancedActorComponent*, FHoudiniGeoPartObject >& ComponentToPart )
{
    TArray< AActor* > NewActors;

    ULevel* DesiredLevel = GWorld->GetCurrentLevel();
    FName BaseName( *( HoudiniAssetComponent->GetOwner()->GetName() + TEXT( "_Baked" ) ) );

    for ( const auto& Iter : ComponentToPart )
    {
        const UHoudiniInstancedActorComponent * OtherSMC = Iter.Key;
        for ( AActor* InstActor : OtherSMC->Instances )
        {
            FName NewName = MakeUniqueObjectName( DesiredLevel, OtherSMC->InstancedAsset->StaticClass(), BaseName );
            FString NewNameStr = NewName.ToString();

            if ( AActor* NewActor = OtherSMC->SpawnInstancedActor( InstActor->GetTransform() ) )
            {
                NewActor->SetActorLabel( NewNameStr );
                NewActor->SetFolderPath( BaseName );
            }
        }
    }
    return NewActors;
}

TArray< AActor* > 
FHoudiniEngineUtils::BakeHoudiniActorToActors_StaticMeshes(
    UHoudiniAssetComponent * HoudiniAssetComponent, 
    TMap< const UStaticMeshComponent*, FHoudiniGeoPartObject >& SMComponentToPart )
{
    TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;

    // Loop over all comps, bake static mesh if not already baked, and create an actor for every one of them
    TArray< AActor* > NewActors;

    for ( const auto& Iter : SMComponentToPart )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Value;
        const UStaticMeshComponent * OtherSMC = Iter.Key;

        if ( ! ensure( OtherSMC->GetStaticMesh() ) )
            continue;

        UStaticMesh* BakedSM = nullptr;
        if ( UStaticMesh ** FoundMeshPtr = OriginalToBakedMesh.Find( OtherSMC->GetStaticMesh() ) )
        {
            // We've already baked this mesh, use it
            BakedSM = *FoundMeshPtr;
        }
        else
        {
            if ( FHoudiniEngineUtils::StaticMeshRequiresBake( OtherSMC->GetStaticMesh() ) )
            {
                // Bake the found mesh into the project
                BakedSM = FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(
                    OtherSMC->GetStaticMesh(), HoudiniAssetComponent, HoudiniGeoPartObject, FHoudiniEngineUtils::EBakeMode::CreateNewAssets );

                if ( ensure( BakedSM ) )
                {
                    FAssetRegistryModule::AssetCreated( BakedSM );
                }
            }
            else
            {
                // We didn't bake this mesh, but it's already baked so we will just use it as is
                BakedSM = OtherSMC->GetStaticMesh();
            }
            OriginalToBakedMesh.Add( OtherSMC->GetStaticMesh(), BakedSM );
        }
    }

    // Finished baking, now spawn the actors
    for ( const auto& Iter : SMComponentToPart )
    {
        const UStaticMeshComponent * OtherSMC = Iter.Key;
        UStaticMesh* BakedSM = OriginalToBakedMesh[ OtherSMC->GetStaticMesh() ];

        if ( ensure( BakedSM ) )
        {
            ULevel* DesiredLevel = GWorld->GetCurrentLevel();
            FName BaseName( *( HoudiniAssetComponent->GetOwner()->GetName() + TEXT( "_Baked" ) ) );
            UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryStaticMesh::StaticClass() );

            auto PrepNewStaticMeshActor = [&]( AActor* NewActor ) {
                // The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
                FName NewName = MakeUniqueObjectName( DesiredLevel, Factory->NewActorClass, BaseName );
                FString NewNameStr = NewName.ToString();
                NewActor->Rename( *NewNameStr );
                NewActor->SetActorLabel( NewNameStr );
                NewActor->SetFolderPath( BaseName );

                // Copy properties to new actor
                if ( AStaticMeshActor* SMActor = Cast< AStaticMeshActor>( NewActor ) )
                {
                    if ( UStaticMeshComponent* SMC = SMActor->GetStaticMeshComponent() )
                    {
                        UStaticMeshComponent* OtherSMC_NonConst = const_cast<UStaticMeshComponent*>( OtherSMC );
                        SMC->SetCollisionProfileName( OtherSMC_NonConst->GetCollisionProfileName() );
                        SMC->SetCollisionEnabled( OtherSMC->GetCollisionEnabled() );
                        SMC->LightmassSettings = OtherSMC->LightmassSettings;
                        SMC->CastShadow = OtherSMC->CastShadow;
                        SMC->SetMobility( OtherSMC->Mobility );
                        if ( OtherSMC_NonConst->GetBodySetup() )
                            SMC->SetPhysMaterialOverride( OtherSMC_NonConst->GetBodySetup()->GetPhysMaterial() );
                        SMActor->SetActorHiddenInGame( OtherSMC->bHiddenInGame );
                        SMC->SetVisibility( OtherSMC->IsVisible() );
                    }
                }
            };

            if ( const UInstancedStaticMeshComponent* OtherISMC = Cast< const UInstancedStaticMeshComponent>( OtherSMC ) )
            {
#ifdef BAKE_TO_INSTANCEDSTATICMESHCOMPONENT_ACTORS
                // This is an instanced static mesh component - we will create a generic AActor with a UInstancedStaticMeshComponent root
                FActorSpawnParameters SpawnInfo;
                SpawnInfo.OverrideLevel = DesiredLevel;
                SpawnInfo.ObjectFlags = RF_Transactional;
                SpawnInfo.Name = MakeUniqueObjectName( DesiredLevel, AActor::StaticClass(), BaseName );
                SpawnInfo.bDeferConstruction = true;

                if ( AActor* NewActor = DesiredLevel->OwningWorld->SpawnActor<AActor>( SpawnInfo ) )
                {
                    NewActor->SetActorLabel( NewActor->GetName() );
                    NewActor->SetActorHiddenInGame( OtherISMC->bHiddenInGame );

                    if ( UInstancedStaticMeshComponent* NewISMC = DuplicateObject< UInstancedStaticMeshComponent >( OtherISMC, NewActor, *OtherISMC->GetName() ) )
                    {
                        NewISMC->SetupAttachment( nullptr );
                        NewISMC->SetStaticMesh( BakedSM );
                        NewActor->AddInstanceComponent( NewISMC );
                        NewActor->SetRootComponent( NewISMC );
                        NewISMC->SetWorldTransform( OtherISMC->GetComponentTransform() );
                        NewISMC->RegisterComponent();

                        NewActor->SetFolderPath( BaseName );
                        NewActor->FinishSpawning( OtherISMC->GetComponentTransform() );

                        NewActors.Add( NewActor );
                        NewActor->InvalidateLightingCache();
                        NewActor->PostEditMove( true );
                        NewActor->MarkPackageDirty();
                    }
                }
#else
                // This is an instanced static mesh component - we will split it up into StaticMeshActors
                for ( int32 InstanceIx = 0; InstanceIx < OtherISMC->GetInstanceCount(); ++InstanceIx )
                {
                    FTransform InstanceTransform;
                    OtherISMC->GetInstanceTransform( InstanceIx, InstanceTransform, true );
                    if ( AActor* NewActor = Factory->CreateActor( BakedSM, DesiredLevel, InstanceTransform, RF_Transactional ) )
                    {
                        PrepNewStaticMeshActor( NewActor );
                    }
                }
#endif
            }
            else
            {
                if ( AActor* NewActor = Factory->CreateActor( BakedSM, DesiredLevel, OtherSMC->GetComponentTransform(), RF_Transactional ) )
                {
                    PrepNewStaticMeshActor( NewActor );
                }
            }
        }
    }
    return NewActors;
}

UHoudiniAssetInput* 
FHoudiniEngineUtils::GetInputForBakeHoudiniActorToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent )
{
    UHoudiniAssetInput *FirstInput = nullptr, *Result = nullptr;
    if ( HoudiniAssetComponent->GetInputs().Num() && HoudiniAssetComponent->GetInputs()[ 0 ] )
    {
        FirstInput = HoudiniAssetComponent->GetInputs()[ 0 ];
    }
    else
    {
        TMap< FString, UHoudiniAssetParameter* > InputParams;
        HoudiniAssetComponent->CollectAllParametersOfType( UHoudiniAssetInput::StaticClass(), InputParams );
        TArray< UHoudiniAssetParameter* > InputParamsArray;
        InputParams.GenerateValueArray( InputParamsArray );
        if ( InputParamsArray.Num() > 0 )
        {
            FirstInput = Cast<UHoudiniAssetInput>(InputParamsArray[ 0 ]);
        }
    }

    if ( FirstInput )
    {
        if ( FirstInput->GetChoiceIndex() == EHoudiniAssetInputType::WorldInput && FirstInput->GetWorldOutlinerInputs().Num() == 1 )
        {
            const FHoudiniAssetInputOutlinerMesh& InputOutlinerMesh = FirstInput->GetWorldOutlinerInputs()[ 0 ];
            if ( InputOutlinerMesh.Actor && InputOutlinerMesh.StaticMeshComponent )
            {
                Result = FirstInput;
            }
        }
    }
    return Result;
}

bool 
FHoudiniEngineUtils::GetCanComponentBakeToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent )
{
    // TODO: Cache this
    auto SMComponentToPart = HoudiniAssetComponent->CollectAllStaticMeshComponents();
    if ( SMComponentToPart.Num() == 1 )
    {
        return nullptr != GetInputForBakeHoudiniActorToOutlinerInput( HoudiniAssetComponent );
    }
    return false;
}

void 
FHoudiniEngineUtils::BakeHoudiniActorToOutlinerInput( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;
    TMap< const UStaticMeshComponent*, FHoudiniGeoPartObject > SMComponentToPart = HoudiniAssetComponent->CollectAllStaticMeshComponents();

    for ( const auto& Iter : SMComponentToPart )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Value;
        const UStaticMeshComponent * OtherSMC = Iter.Key;

        if ( ! ensure( OtherSMC->GetStaticMesh() ) )
            continue;

        UStaticMesh* BakedSM = nullptr;
        if ( UStaticMesh ** FoundMeshPtr = OriginalToBakedMesh.Find( OtherSMC->GetStaticMesh() ) )
        {
            // We've already baked this mesh, use it
            BakedSM = *FoundMeshPtr;
        }
        else
        {
            // Bake the found mesh into the project
            BakedSM = FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(
                OtherSMC->GetStaticMesh(), HoudiniAssetComponent, HoudiniGeoPartObject, FHoudiniEngineUtils::EBakeMode::CreateNewAssets );

            if ( BakedSM )
            {
                OriginalToBakedMesh.Add( OtherSMC->GetStaticMesh(), BakedSM );
                FAssetRegistryModule::AssetCreated( BakedSM );
            }
        }
    }

    {
        const FScopedTransaction Transaction( LOCTEXT( "BakeToInput", "Bake To Input" ) );

        for ( auto Iter : OriginalToBakedMesh )
        {
            // Get the first outliner input
            if ( UHoudiniAssetInput* FirstInput = GetInputForBakeHoudiniActorToOutlinerInput( HoudiniAssetComponent ) )
            {
                const FHoudiniAssetInputOutlinerMesh& InputOutlinerMesh = FirstInput->GetWorldOutlinerInputs()[ 0 ];
                if ( InputOutlinerMesh.Actor && InputOutlinerMesh.StaticMeshComponent )
                {
                    UStaticMeshComponent* InOutSMC = InputOutlinerMesh.StaticMeshComponent;
                    InputOutlinerMesh.Actor->Modify();
                    InOutSMC->SetStaticMesh( Iter.Value );
                    InOutSMC->InvalidateLightingCache();
                    InOutSMC->MarkPackageDirty();

                    // Disconnect the input from the asset - InputOutlinerMesh now garbage
                    FirstInput->RemoveWorldOutlinerInput( 0 );
                }
            }
            // Only handle the first Baked Mesh
            break;
        }
    }
}

#endif


int32
FHoudiniEngineUtils::GetMeshSocketList(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId,
    HAPI_NodeId GeoId, HAPI_PartId PartId,
    TArray< FTransform >& AllSockets,
    TArray< FString >& AllSocketsNames,
    TArray< FString >& AllSocketsActors )
{
    // Get object / geo group memberships for primitives.
    TArray< FString > ObjectGeoGroupNames;
    if ( !FHoudiniEngineUtils::HapiGetGroupNames(
        AssetId, ObjectId, GeoId, HAPI_GROUPTYPE_POINT, ObjectGeoGroupNames ) )
    {
        HOUDINI_LOG_MESSAGE( TEXT( "GetMeshSocketList: Object [%d] non-fatal error reading group names" ), ObjectId );
    }

    // First, we want to make sure we have at least one socket group before continuing
    bool bHasSocketGroup = false;
    for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < ObjectGeoGroupNames.Num(); ++GeoGroupNameIdx )
    {
        const FString & GroupName = ObjectGeoGroupNames[ GeoGroupNameIdx ];
        if ( GroupName.StartsWith( TEXT( HAPI_UNREAL_GROUP_MESH_SOCKETS ), ESearchCase::IgnoreCase ) )
        {
            bHasSocketGroup = true;
            break;
        }
    }

    if ( !bHasSocketGroup )
        return 0;

    //
    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    check( HoudiniRuntimeSettings );

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    // Attributes we are interested in.
    TArray< float > Positions;
    HAPI_AttributeInfo AttribInfoPositions;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoPositions, 0 );

    TArray< float > Rotations;
    HAPI_AttributeInfo AttribInfoRotations;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoRotations, 0 );

    TArray< float > Normals;
    HAPI_AttributeInfo AttribInfoNormals;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNormals, 0 );

    TArray< float > Scales;
    HAPI_AttributeInfo AttribInfoScales;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoScales, 0 );

    TArray< FString > Names;
    HAPI_AttributeInfo AttribInfoNames;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNames, 0 );

    TArray< FString > Actors;
    HAPI_AttributeInfo AttribInfoActors;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoActors, 0 );

    // Retrieve position data.
    if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions ) )
        return false;

    // Retrieve rotation data.
    bool bHasRotation = false;
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_ROTATION, AttribInfoRotations, Rotations ) )
        bHasRotation = true;

    // Retrieve normal data.
    bool bHasNormals = false;
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals ) )
        bHasNormals = true;

    // Retrieve scale data.
    bool bHasScale = false;
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_SCALE, AttribInfoScales, Scales ) )
        bHasScale = true;

    // Retrieve mesh socket names.
    bool bHasNames = false;
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, AttribInfoNames, Names ) )
        bHasNames = true;

    // Retrieve mesh socket actor.
    bool bHasActors = false;
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR, AttribInfoActors, Actors ) )
        bHasActors = true;

    // Extracting Sockets vertices
    for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < ObjectGeoGroupNames.Num(); ++GeoGroupNameIdx )
    {
        const FString & GroupName = ObjectGeoGroupNames[ GeoGroupNameIdx ];
        if ( !GroupName.StartsWith ( TEXT ( HAPI_UNREAL_GROUP_MESH_SOCKETS ) , ESearchCase::IgnoreCase ) )
            continue;

        TArray< int32 > PointGroupMembership;
        FHoudiniEngineUtils::HapiGetGroupMembership(
            AssetId, ObjectId, GeoId, PartId, 
            HAPI_GROUPTYPE_POINT, GroupName, PointGroupMembership );

        // Go through all primitives.
        for ( int32 PointIdx = 0; PointIdx < PointGroupMembership.Num(); ++PointIdx )
        {
            if ( PointGroupMembership[PointIdx] != 1 )
                continue;

            FTransform currentSocketTransform;
            FVector currentPosition = FVector::ZeroVector;
            FVector currentScale = FVector( 1.0f, 1.0f, 1.0f );
            FQuat currentRotation = FQuat::Identity;

            currentPosition.X = Positions[ PointIdx * 3 ] * GeneratedGeometryScaleFactor;

            if( bHasScale )
             currentScale.X = Scales[PointIdx * 3];

            if ( ImportAxis == HRSAI_Unreal )
            {
                currentPosition.Y = Positions[ PointIdx * 3 + 2 ] * GeneratedGeometryScaleFactor;
                currentPosition.Z = Positions[ PointIdx * 3 + 1 ] * GeneratedGeometryScaleFactor;

                if ( bHasScale )
                {
                    currentScale.Y = Scales[ PointIdx * 3 + 2 ];
                    currentScale.Z = Scales[ PointIdx * 3 + 1 ];
                }

                if ( bHasRotation )
                {
                    currentRotation.X = Rotations[ PointIdx * 4 ];
                    currentRotation.Y = Rotations[ PointIdx * 4 + 2 ];
                    currentRotation.Z = Rotations[ PointIdx * 4 + 1 ];
                    currentRotation.W = -Rotations[ PointIdx * 4 + 3 ];
                }
                else if ( bHasNormals )
                {
                    FVector vNormal;
                    vNormal.X = Normals[ PointIdx * 3 ];
                    vNormal.Y = Normals[ PointIdx * 3 + 2 ];
                    vNormal.Z = Normals[ PointIdx * 3 + 1 ];

                    if ( vNormal != FVector::ZeroVector )
                        currentRotation = FQuat::FindBetween( FVector::UpVector, vNormal );
                }
            }
            else
            {
                currentPosition.Y = Positions[ PointIdx * 3 + 1 ] * GeneratedGeometryScaleFactor;
                currentPosition.Z = Positions[ PointIdx * 3 + 2 ] * GeneratedGeometryScaleFactor;

                if ( bHasScale )
                {
                    currentScale.Y = Scales[ PointIdx * 3 + 1 ];
                    currentScale.Z = Scales[ PointIdx * 3 + 2 ];
                }

                if ( bHasRotation )
                {
                    currentRotation.X = Rotations[ PointIdx * 4 ];
                    currentRotation.Y = Rotations[ PointIdx * 4 + 1 ];
                    currentRotation.Z = Rotations[ PointIdx * 4 + 2 ];
                    currentRotation.W = Rotations[ PointIdx * 4 + 3 ];
                }
                else if ( bHasNormals )
                {
                    FVector vNormal;
                    vNormal.X = Normals[ PointIdx * 3 ];
                    vNormal.Y = Normals[ PointIdx * 3 + 1 ];
                    vNormal.Z = Normals[ PointIdx * 3 + 2 ];

                    if ( vNormal != FVector::ZeroVector )
                        currentRotation = FQuat::FindBetween( FVector::UpVector, vNormal );
                }
            }

            FString currentName;
            if ( bHasNames )
                currentName = Names[ PointIdx ];

            FString currentActors;
            if ( bHasActors )
                currentActors = Actors[ PointIdx ];

            // If the scale attribute wasn't set on all socket, we might end up
            // with a zero scale socket, avoid that.
            if ( currentScale == FVector::ZeroVector)
                currentScale = FVector( 1.0f, 1.0f, 1.0f );

            currentSocketTransform.SetLocation( currentPosition );
            currentSocketTransform.SetRotation( currentRotation );
            currentSocketTransform.SetScale3D( currentScale );

            AllSockets.Add( currentSocketTransform );
            AllSocketsNames.Add( currentName );
            AllSocketsActors.Add( currentActors );
        }
    }

    return AllSockets.Num();
}


bool 
FHoudiniEngineUtils::AddMeshSocketsToStaticMesh(
    UStaticMesh* StaticMesh,
    FHoudiniGeoPartObject& HoudiniGeoPartObject,
    TArray< FTransform >& AllSockets,
    TArray< FString >& AllSocketsNames,
    TArray< FString >& AllSocketsActors )
{
    if ( !StaticMesh )
        return false;

    if ( AllSockets.Num() <= 0 )
        return false;

    // Do we need to clear the previously generated sockets?
    if ( !HoudiniGeoPartObject.bHasSocketBeenAdded )
        StaticMesh->Sockets.Empty();

    for ( int32 nSocket = 0; nSocket < AllSockets.Num(); nSocket++ )
    {
        // Create a new Socket
        UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>( StaticMesh );
        check( Socket );

        Socket->RelativeLocation = AllSockets[ nSocket ].GetLocation();
        Socket->RelativeRotation = FRotator(AllSockets[ nSocket ].GetRotation());
        Socket->RelativeScale = AllSockets[ nSocket ].GetScale3D();

        if ( AllSocketsNames.IsValidIndex( nSocket ) && !AllSocketsNames[ nSocket ].IsEmpty() )
        {
            Socket->SocketName = FName( *AllSocketsNames[ nSocket ] );
        }
        else
        {
            // Having sockets with empty names can lead to various issues, so we'll create one now
            Socket->SocketName = FName( TEXT("Socket"), StaticMesh->Sockets.Num() );
        }


        // The actor will be store temporarily in the socket's Tag as we need a StaticMeshComponent to add an actor to the socket
        if ( AllSocketsActors.IsValidIndex( nSocket ) && !AllSocketsActors[ nSocket ].IsEmpty() )
            Socket->Tag = AllSocketsActors[ nSocket ];

        StaticMesh->Sockets.Add( Socket );
    }

    // We don't want these new socket to be removed until next cook
    HoudiniGeoPartObject.bHasSocketBeenAdded = true;

    // Clean up
    AllSockets.Empty();
    AllSocketsNames.Empty();
    AllSocketsActors.Empty();

    return true;
}


bool
FHoudiniEngineUtils::AddAggregateCollisionGeometryToStaticMesh(
    UStaticMesh* StaticMesh,
    FHoudiniGeoPartObject& HoudiniGeoPartObject,
    FKAggregateGeom& AggregateCollisionGeo )
{
    if ( !StaticMesh )
        return false;
    
    UBodySetup * BodySetup = StaticMesh->BodySetup;
    if ( !BodySetup )
        return false;

    // Do we need to remove the old collisions from the previous cook
    if ( !HoudiniGeoPartObject.bHasCollisionBeenAdded )
        BodySetup->RemoveSimpleCollision();

    BodySetup->AddCollisionFrom( AggregateCollisionGeo );
    BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;

    BodySetup->ClearPhysicsMeshes();
    BodySetup->InvalidatePhysicsData();

#if WITH_EDITOR
    RefreshCollisionChange( StaticMesh );
#endif

    // This geo part will have to be considered as rendered collision
    if ( !HoudiniGeoPartObject.bIsCollidable )
        HoudiniGeoPartObject.bIsRenderCollidable = true;

    // We don't want these collisions to be removed before the next cook
    HoudiniGeoPartObject.bHasCollisionBeenAdded = true;

    // Clean the added collisions
    AggregateCollisionGeo.EmptyElements();

    return true;
}


bool
FHoudiniEngineUtils::AddActorsToMeshSocket( UStaticMeshSocket* Socket, UStaticMeshComponent* StaticMeshComponent )
{
    if ( !Socket || !StaticMeshComponent )
        return false;

    // The actor to assign is stored is the socket's tag
    FString ActorString = Socket->Tag;
    if ( ActorString.IsEmpty() )
        return false;

    // Converting the string to a string array using delimiters
    const TCHAR* Delims[] = { TEXT(","), TEXT(";") };
    TArray<FString> ActorStringArray;
    ActorString.ParseIntoArray( ActorStringArray, Delims, 2 );

    if ( ActorStringArray.Num() <= 0 )
        return false;

#if WITH_EDITOR
    // And try to find the corresponding HoudiniAssetActor in the editor world
    // to avoid finding "deleted" assets with the same name
    //UWorld* editorWorld = GEditor->GetEditorWorldContext().World();
    UWorld* editorWorld = StaticMeshComponent->GetOwner()->GetWorld();
    for (TActorIterator<AActor> ActorItr(editorWorld); ActorItr; ++ActorItr)
    {
        // Same as with the Object Iterator, access the subclass instance with the * or -> operators.
        AActor *Actor = *ActorItr;
        if ( !Actor )
            continue;

        for ( int32 StringIdx = 0; StringIdx < ActorStringArray.Num(); StringIdx++ )
        {
            if ( Actor->GetName() != ActorStringArray[ StringIdx ] )
                continue;

            if ( Actor->IsPendingKillOrUnreachable() )
                continue;

            Socket->AttachActor( Actor, StaticMeshComponent );
        }
    }
#endif

    Socket->Tag = TEXT("");

    return true;
}

