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
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"
#include "HoudiniAttributeDataComponent.h"
#include "HoudiniLandscapeUtils.h"
#include "Components/SplineComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "HoudiniInstancedActorComponent.h"

#include "CoreMinimal.h"
#include "AI/Navigation/NavCollision.h"
#include "Engine/StaticMeshSocket.h"
#if WITH_EDITOR
    #include "Editor.h"
    #include "Factories/MaterialFactoryNew.h"
    #include "Interfaces/ITargetPlatform.h"
    #include "Interfaces/ITargetPlatformManagerModule.h"
#endif
#include "EngineUtils.h"
#include "MetaData.h"
#include "PhysicsEngine/BodySetup.h"

#if PLATFORM_WINDOWS
    #include "WindowsHWrapper.h"

    // Of course, Windows defines its own GetGeoInfo,
    // So we need to undefine that before including HoudiniApi.h to avoid collision...
    #ifdef GetGeoInfo
	#undef GetGeoInfo
    #endif
#endif

#include "Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

DECLARE_CYCLE_STAT( TEXT( "Houdini: Build Static Mesh" ), STAT_BuildStaticMesh, STATGROUP_HoudiniEngine );

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
FHoudiniEngineUtils::ComputeAssetPresetBufferLength( HAPI_NodeId AssetId, int32 & OutBufferLength )
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
FHoudiniEngineUtils::SetAssetPreset( HAPI_NodeId AssetId, const TArray< char > & PresetBuffer )
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
FHoudiniEngineUtils::GetAssetPreset( HAPI_NodeId AssetId, TArray< char > & PresetBuffer )
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
FHoudiniEngineUtils::IsHoudiniAssetValid( HAPI_NodeId AssetId )
{
    if ( AssetId < 0 )
        return false;

    HAPI_NodeInfo NodeInfo;
    bool ValidationAnswer = 0;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &NodeInfo ), false );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::IsNodeValid(
        FHoudiniEngine::Get().GetSession(), AssetId,
        NodeInfo.uniqueHoudiniNodeId, &ValidationAnswer ), false );

    return ValidationAnswer;
}

bool
FHoudiniEngineUtils::DestroyHoudiniAsset( HAPI_NodeId AssetId )
{
    return FHoudiniApi::DeleteNode( FHoudiniEngine::Get().GetSession(), AssetId ) == HAPI_RESULT_SUCCESS;
}

void
FHoudiniEngineUtils::ConvertUnrealString( const FString & UnrealString, std::string & String )
{
    String = TCHAR_TO_UTF8( *UnrealString );
}

bool
FHoudiniEngineUtils::GetUniqueMaterialShopName( HAPI_NodeId AssetId, HAPI_NodeId MaterialId, FString & Name )
{
    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    HAPI_MaterialInfo MaterialInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetMaterialInfo(
        FHoudiniEngine::Get().GetSession(), MaterialId,
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
    FMemory::Memzero< HAPI_Transform >( HapiTransformQuat );
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
FHoudiniEngineUtils::GetHoudiniAssetName( HAPI_NodeId AssetId, FString & NameString )
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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_GroupType GroupType, TArray< FString > & GroupNames )
{
    HAPI_GeoInfo GeoInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGeoInfo( FHoudiniEngine::Get().GetSession(), GeoId, &GeoInfo ), false );

    int32 GroupCount = FHoudiniEngineUtils::HapiGetGroupCountByType( GroupType, GeoInfo );

    if ( GroupCount > 0 )
    {
        std::vector< int32 > GroupNameHandles( GroupCount, 0 );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupNames(
            FHoudiniEngine::Get().GetSession(),
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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_PartId PartId, HAPI_GroupType GroupType,
    const FString & GroupName, TArray< int32 > & GroupMembership )
{
    HAPI_PartInfo PartInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PartInfo ), false );

    int32 ElementCount = FHoudiniEngineUtils::HapiGetElementCountByGroupType( GroupType, PartInfo );
    std::string ConvertedGroupName = TCHAR_TO_UTF8( *GroupName );

    GroupMembership.SetNumUninitialized( ElementCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupMembership(
        FHoudiniEngine::Get().GetSession(), GeoId, PartId, GroupType,
        ConvertedGroupName.c_str(), NULL, &GroupMembership[ 0 ], 0, ElementCount ), false );

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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId, HAPI_PartId PartId,
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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_PartId PartId, const char * Name)
{
    for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
    {
        if (HapiCheckAttributeExists(AssetId, ObjectId, GeoId,
            PartId, Name, (HAPI_AttributeOwner)AttrIdx))
            return true;
    }

    return false;
}

bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeOwner Owner )
{
    HAPI_AttributeInfo AttribInfo;
    if ( FHoudiniApi::GetAttributeInfo(
        FHoudiniEngine::Get().GetSession(),
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


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
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
            FHoudiniEngine::Get().GetSession(), GeoId, PartId, Name,
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
        FHoudiniEngine::Get().GetSession(), GeoId, PartId, Name,
        &AttributeInfo, -1, &Data[ 0 ], 0, AttributeInfo.count ), false );

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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
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
            FHoudiniEngine::Get().GetSession(),
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
        FHoudiniEngine::Get().GetSession(),
        GeoId, PartId, Name, &AttributeInfo, -1, &Data[ 0 ], 0, AttributeInfo.count ), false );

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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
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
            FHoudiniEngine::Get().GetSession(),
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
        FHoudiniEngine::Get().GetSession(), GeoId, PartId, Name, &AttributeInfo,
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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_PartId PartId, TArray< FTransform > & Transforms )
{
    Transforms.Empty();

    // Number of instances is number of points.
    HAPI_PartInfo PartInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), GeoId,
        PartId, &PartInfo ), false );

    if ( PartInfo.pointCount == 0 )
        return false;

    TArray< HAPI_Transform > InstanceTransforms;
    InstanceTransforms.SetNumZeroed( PartInfo.pointCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstanceTransforms(
        FHoudiniEngine::Get().GetSession(),
        GeoId, HAPI_SRT, &InstanceTransforms[ 0 ],
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
        MaterialInfo.nodeId, NodeParmId ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( FHoudiniApi::GetImagePlaneCount(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImagePlaneCount ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( !ImagePlaneCount )
        return true;

    TArray< HAPI_StringHandle > ImagePlaneStringHandles;
    ImagePlaneStringHandles.SetNumUninitialized( ImagePlaneCount );

    if ( FHoudiniApi::GetImagePlanes(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImagePlaneStringHandles[ 0 ], ImagePlaneCount ) != HAPI_RESULT_SUCCESS )
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
            FHoudiniEngine::Get().GetSession(),
            MaterialInfo.nodeId, NodeParmId ) != HAPI_RESULT_SUCCESS )
        {
            return false;
        }
    }

    HAPI_ImageInfo ImageInfo;

    if ( FHoudiniApi::GetImageInfo(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImageInfo ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    ImageInfo.dataFormat = ImageDataFormat;
    ImageInfo.interleaved = true;
    ImageInfo.packing = ImagePacking;

    if ( FHoudiniApi::SetImageInfo(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImageInfo) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    int32 ImageBufferSize = 0;

    if ( FHoudiniApi::ExtractImageToMemory(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, HAPI_RAW_FORMAT_NAME,
        PlaneType, &ImageBufferSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( !ImageBufferSize )
        return false;

    ImageBuffer.SetNumUninitialized( ImageBufferSize );

    if ( FHoudiniApi::GetImageMemoryBuffer(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImageBuffer[ 0 ],
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

int32 FHoudiniEngineUtils::HapiFindParameterByNameOrTag( const HAPI_NodeId& NodeId, const std::string ParmName )
{
    HAPI_ParmInfo ParmInfo;
    return HapiFindParameterByNameOrTag( NodeId, ParmName, ParmInfo );
}

int32 FHoudiniEngineUtils::HapiFindParameterByNameOrTag( const HAPI_NodeId& NodeId, const std::string ParmName, HAPI_ParmInfo& FoundParmInfo )
{
    HAPI_NodeInfo NodeInfo;
    FHoudiniApi::GetNodeInfo( FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo );

    TArray< HAPI_ParmInfo > NodeParams;
    NodeParams.SetNumUninitialized( NodeInfo.parmCount );
    FHoudiniApi::GetParameters(
        FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount );

    int32 ParmId = HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, ParmName );
    if ( ( ParmId < 0 ) || ( ParmId >= NodeParams.Num() ) )
        return -1;

    FoundParmInfo = NodeParams[ParmId];
    return ParmId;
}

int32 FHoudiniEngineUtils::HapiFindParameterByNameOrTag( const TArray< HAPI_ParmInfo > NodeParams, const HAPI_NodeId& NodeId, const std::string ParmName )
{
   {
        // First, try to find the parameter by its name
        TArray< std::string > NodeParamNames;
        FHoudiniEngineUtils::HapiRetrieveParameterNames( NodeParams, NodeParamNames );

        int32 ParmNameIdx = -1;
        for ( ParmNameIdx = 0; ParmNameIdx < NodeParamNames.Num(); ++ParmNameIdx )
        {
            if ( ParmName.compare( 0, ParmName.length(), NodeParamNames[ParmNameIdx] ) == 0 )
                break;
        }

        if ( ( ParmNameIdx >= 0 ) && ( ParmNameIdx < NodeParams.Num() ) )
            return ParmNameIdx;
    }

    {
        // Second, try to find it by its tag
        HAPI_ParmId ParmId;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmWithTag(
            FHoudiniEngine::Get().GetSession(),
            NodeId, ParmName.c_str(), &ParmId ), false );

        if ( ( ParmId >= 0 ) && ( ParmId < NodeParams.Num() ) )
            return ParmId;
    }

    return -1;
}


bool
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(
    HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue, float & OutValue )
{
    float Value = DefaultValue;
    bool bComputed = false;

    HAPI_ParmInfo FoundParamInfo;
    int32 ParmIndex = FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeId, ParmName, FoundParamInfo );
    if ( ParmIndex > -1 )
    {
        if (FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeId, &Value,
            FoundParamInfo.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
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

    HAPI_ParmInfo FoundParamInfo;
    int32 ParmIndex = FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeId, ParmName, FoundParamInfo );
    if ( ParmIndex > -1 )
    {
        if ( FHoudiniApi::GetParmIntValues(
            FHoudiniEngine::Get().GetSession(), NodeId, &Value,
            FoundParamInfo.intValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
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

    HAPI_ParmInfo FoundParamInfo;
    int32 ParmIndex = FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeId, ParmName, FoundParamInfo );
    if ( ParmIndex > -1 )
    {
        HAPI_StringHandle StringHandle;
        if ( FHoudiniApi::GetParmStringValues(
            FHoudiniEngine::Get().GetSession(), NodeId, false,
            &StringHandle, FoundParamInfo.stringValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
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
FHoudiniEngineUtils::HapiGetParameterUnit( const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, FString& OutUnitString )
{
    //
    OutUnitString = TEXT("");

    // We're looking for the parameter unit tag
    FString UnitTag = "units";
    bool HasUnit = false;

    // Does the parameter has the "units" tag?
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ParmHasTag(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        TCHAR_TO_ANSI(*UnitTag), &HasUnit ), false );

    if ( !HasUnit )
        return false;
    
    // Get the unit string value
    HAPI_StringHandle StringHandle;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmTagValue(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        TCHAR_TO_ANSI(*UnitTag), &StringHandle ), false );

    // Get the actual string value.
    FString UnitString = TEXT("");
    FHoudiniEngineString HoudiniEngineString( StringHandle );
    if ( HoudiniEngineString.ToFString( UnitString ) )
    {
        // We need to do some replacement in the string here in order to be able to get the
        // proper unit type when calling FUnitConversion::UnitFromString(...) after.

        // Per second and per hour are the only "per" unit that unreal recognize
        UnitString.ReplaceInline( TEXT( "s-1" ), TEXT( "/s" ) );
        UnitString.ReplaceInline( TEXT( "h-1" ), TEXT( "/h" ) );

        // Houdini likes to add '1' on all the unit, so we'll remove all of them
        // except the '-1' that still needs to remain.
        UnitString.ReplaceInline( TEXT( "-1" ), TEXT( "--" ) );
        UnitString.ReplaceInline( TEXT( "1" ), TEXT( "" ) );
        UnitString.ReplaceInline( TEXT( "--" ), TEXT( "-1" ) );

        OutUnitString = UnitString;

        return true;
    }

    return false;
}

bool
FHoudiniEngineUtils::HapiIsMaterialTransparent( const HAPI_MaterialInfo & MaterialInfo )
{
    float Alpha;
    FHoudiniEngineUtils::HapiGetParameterDataAsFloat( MaterialInfo.nodeId, HAPI_UNREAL_PARAM_ALPHA, 1.0f, Alpha );

    return Alpha < HAPI_UNREAL_ALPHA_THRESHOLD;
}

bool
FHoudiniEngineUtils::IsValidAssetId( HAPI_NodeId AssetId )
{
    return AssetId != -1;
}

bool
FHoudiniEngineUtils::HapiCreateCurveNode( HAPI_NodeId & ConnectedAssetId )
{
#if WITH_EDITOR

    // Create the curve SOP Node
    HAPI_NodeId NodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), -1,
        "SOP/curve", nullptr, false, &NodeId), false);
    
    ConnectedAssetId = NodeId;

    // Submit default points to curve.
    HAPI_ParmId ParmId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
        FHoudiniEngine::Get().GetSession(), NodeId,
        HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
        FHoudiniEngine::Get().GetSession(), NodeId,
        HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT, ParmId, 0), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), NodeId, nullptr), false);
   
#endif // WITH_EDITOR

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateCurveInputNodeForData(
    HAPI_NodeId HostAssetId,
    HAPI_NodeId& ConnectedAssetId,
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
    if (ConnectedAssetId < 0)
    {
        HAPI_NodeId NodeId = -1;
        // Create the curve SOP Node
        if (!HapiCreateCurveNode(NodeId))
            return false;

        // Check if we have a valid id for this new input asset.
        if (!FHoudiniEngineUtils::IsHoudiniAssetValid(NodeId))
            return false;

        // We now have a valid id.
        ConnectedAssetId = NodeId;
    }
    else
    {
        // We have to revert the Geo to its original state so we can use the Curve SOP:
        // adding parameters to the Curve SOP locked it, preventing its parameters (type, method, isClosed) from working
        FHoudiniApi::RevertGeo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId);
    }

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
        ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_TYPE,
        0, CurveTypeValue);
    FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
        ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_METHOD,
        0, CurveMethodValue);
    FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
        ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_CLOSED,
        1, CurveClosed);

    // For closed NURBS (CVs and Breakpoints), we have to close the curve manually, by duplicating its last point
    // in order to be able to set the rotations and scales attributes properly.
    bool bCloseCurveManually = false;
    if ( CurveClosed && (CurveTypeValue == HAPI_CURVETYPE_NURBS) && (CurveMethodValue != 2) )
    {
        // The curve is not closed anymore
        if (HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
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
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
            HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) != HAPI_RESULT_SUCCESS)
    {
        return false;
    }

    std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
        ConvertedString.c_str(), ParmId, 0), false);
    
    // If we don't want to add rotations or scale attributes to the curve, 
    // we can just cook the node normally and stop here.
    bool bAddRotations = (Rotations != nullptr);
    bool bAddScales3d = (Scales3d != nullptr);
    bool bAddUniformScales = (UniformScales != nullptr);

    if (!bAddRotations && !bAddScales3d && !bAddUniformScales)
    {
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, nullptr), false);

        return true;
    }

    // Setting up the first cook, without the curve refinement
    HAPI_CookOptions CookOptions;
    FMemory::Memzero< HAPI_CookOptions >(CookOptions);
    CookOptions.curveRefineLOD = 8.0f;
    CookOptions.clearErrorsAndWarnings = false;
    CookOptions.maxVerticesPerPrimitive = -1;
    CookOptions.splitGeosByGroup = false;
    CookOptions.handleBoxPartTypes = false;
    CookOptions.handleSpherePartTypes = false;
    CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;
    CookOptions.refineCurveToLinear = false;

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &CookOptions), false);

    //  We can now read back the Part infos from the cooked curve.
    HAPI_PartInfo PartInfos;
    FMemory::Memzero< HAPI_PartInfo >(PartInfos);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, &PartInfos), false);

    //
    // Depending on the curve type and method, additionnal control points might have been created.
    // We now have to interpolate the rotations and scale attributes for these.
    //

    // Lambda function that interpolates rotation, scale and uniform scales values
    // between two points using fCoeff as a weight, and insert the interpolated value at nInsertIndex
    auto InterpolateRotScaleUScale = [&](const int32& nIndex1, const int32& nIndex2, const float& fCoeff, const int32& nInsertIndex)
    {
        if ( Rotations && Rotations->IsValidIndex(nIndex1) && Rotations->IsValidIndex(nIndex2) )
        {           
            FQuat interpolation = FQuat::Slerp((*Rotations)[nIndex1], (*Rotations)[nIndex2], fCoeff);
            if (Rotations->IsValidIndex(nInsertIndex))
                Rotations->Insert(interpolation, nInsertIndex);
            else
                Rotations->Add(interpolation);
        }

        if ( Scales3d && Scales3d->IsValidIndex(nIndex1) && Scales3d->IsValidIndex(nIndex2) )
        {
            FVector interpolation = fCoeff * (*Scales3d)[nIndex1] + (1.0f - fCoeff) * (*Scales3d)[nIndex2];
            if (Scales3d->IsValidIndex(nInsertIndex))
                Scales3d->Insert( interpolation, nInsertIndex);
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
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
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
            DuplicateRotScaleUScale(NumberOfCVs-1, NumberOfCVs);
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

    int OriginalPointParametersCount = PartInfos.attributeCounts[NewAttributesOwner];
    if (bAddRotations)
        PartInfos.attributeCounts[NewAttributesOwner] += 1;
    if (bAddScales3d)
        PartInfos.attributeCounts[NewAttributesOwner] += 1;
    if (bAddUniformScales)
        PartInfos.attributeCounts[NewAttributesOwner] += 1;

    // Sending the updated PartInfos
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(),
        ConnectedAssetId, 0, &PartInfos), false);
    
    // We need now to reproduce ALL the curves attributes for ALL the Owners..
    for (int nOwner = 0; nOwner < HAPI_ATTROWNER_MAX; nOwner++)
    {
        int nOwnerAttributeCount = nOwner == NewAttributesOwner ? OriginalPointParametersCount : PartInfos.attributeCounts[nOwner];
        if (nOwnerAttributeCount == 0)
            continue;

        std::vector< HAPI_StringHandle > sh_attributes(nOwnerAttributeCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, 
            (HAPI_AttributeOwner)nOwner,
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
                ConnectedAssetId, 0,
                attr_name.c_str(), 
                (HAPI_AttributeOwner)nOwner,
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
                        ConnectedAssetId, 0,
                        attr_name.c_str(), &attr_info, -1,
                        IntData.GetData(), 0, attr_info.count), false);

                    // ADD
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(),
                        ConnectedAssetId, 0,
                        attr_name.c_str(), &attr_info), false);

                    // SET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
                        FHoudiniEngine::Get().GetSession(),
                        ConnectedAssetId, 0,
                        attr_name.c_str(), 
                        &attr_info, IntData.GetData(),
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
                        ConnectedAssetId, 0,
                        attr_name.c_str(), &attr_info, -1,
                        FloatData.GetData(), 
                        0, attr_info.count), false);

                    // ADD
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(),
                        ConnectedAssetId, 0,
                        attr_name.c_str(), 
                        &attr_info), false);

                    // SET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
                        FHoudiniEngine::Get().GetSession(),
                        ConnectedAssetId, 0,
                        attr_name.c_str(), &attr_info, 
                        FloatData.GetData(),
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
                        ConnectedAssetId, 0,
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
                        ConnectedAssetId, 0,
                        attr_name.c_str(), 
                        &attr_info), false);

                    // SET
                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
                        FHoudiniEngine::Get().GetSession(),
                        ConnectedAssetId, 0,
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
            ConnectedAssetId, 0,
            &CurveInfo), false);

        // ... the curve counts
        TArray< int > CurveCounts;
        CurveCounts.SetNumUninitialized(CurveInfo.curveCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveCounts(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0,
            CurveCounts.GetData(), 
            0, CurveInfo.curveCount), false);

        // .. the curve orders
        TArray< int > CurveOrders;
        CurveOrders.SetNumUninitialized(CurveInfo.curveCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveOrders(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0,
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
                    ConnectedAssetId, 0,
                    KnotsArray.GetData(), 
                    0, CurveInfo.knotCount), false);
        }

        // To set them back in HAPI
        // CurveInfo
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveInfo(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0,
            &CurveInfo), false);

        // CurveCounts
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveCounts(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0,
            CurveCounts.GetData(), 
            0, CurveInfo.curveCount), false);

        // CurveOrders
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveOrders(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0,
            CurveOrders.GetData(), 
            0, CurveInfo.curveCount), false);

        // And Knots if they exist
        if (CurveInfo.hasKnots)
        {
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveKnots(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0,
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
                ConnectedAssetId, 0,
                FaceCounts.GetData(), 0, 
                PartInfos.faceCount) == HAPI_RESULT_SUCCESS)
        {
            // Set the face count
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0,
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
                ConnectedAssetId, 0,
                VertexList.GetData(),
                0, PartInfos.vertexCount) == HAPI_RESULT_SUCCESS)
        {
            // setting the vertex list
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0,
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
            ConnectedAssetId, 0,
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
            ConnectedAssetId, 0,
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
            ConnectedAssetId, 0,
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
            ConnectedAssetId, 0, 
            HAPI_UNREAL_ATTRIB_SCALE, 
            &AttributeInfoScale,
            CurveScales.GetData(),
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
            ConnectedAssetId, 0,
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
            ConnectedAssetId, 0,
            HAPI_UNREAL_ATTRIB_UNIFORM_SCALE, 
            &AttributeInfoPScale,
            CurvePScales.GetData(), 
            0, AttributeInfoPScale.count), false);
    }

    // Finally, commit the geo ...
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId), false);
#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiGetAssetTransform( HAPI_NodeId AssetId, FTransform & InTransform )
{
    HAPI_NodeInfo LocalAssetNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetId,
        &LocalAssetNodeInfo ), false );

    HAPI_Transform LocalHapiTransform;
    FMemory::Memzero< HAPI_Transform >( LocalHapiTransform );

    if ( LocalAssetNodeInfo.type == HAPI_NODETYPE_SOP )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetObjectTransform(
            FHoudiniEngine::Get().GetSession(), LocalAssetNodeInfo.parentId, -1,
            HAPI_SRT, &LocalHapiTransform ), false );
    }
    else if ( LocalAssetNodeInfo.type == HAPI_NODETYPE_OBJ )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetObjectTransform(
            FHoudiniEngine::Get().GetSession(), AssetId, -1,
            HAPI_SRT, &LocalHapiTransform ), false );
    }
    else
        return false;

    // Convert HAPI transform to Unreal one.
    FHoudiniEngineUtils::TranslateHapiTransform( LocalHapiTransform, InTransform );

    return true;
}

bool
FHoudiniEngineUtils::HapiGetNodeId( HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId, HAPI_NodeId & NodeId )
{
    if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        HAPI_GeoInfo GeoInfo;
        if ( FHoudiniApi::GetGeoInfo(
            FHoudiniEngine::Get().GetSession(), GeoId, &GeoInfo ) == HAPI_RESULT_SUCCESS )
        {
            NodeId = GeoInfo.nodeId;
            return true;
        }
    }

    NodeId = -1;
    return false;
}

bool
FHoudiniEngineUtils::HapiGetNodePath( HAPI_NodeId NodeId, HAPI_NodeId RelativeToNodeId, FString & OutPath )
{
    if ( ( NodeId == -1 ) || ( RelativeToNodeId == -1 ) )
        return false;

    if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( NodeId ) )
        return false;

    HAPI_StringHandle StringHandle;
    if ( FHoudiniApi::GetNodePath(
        FHoudiniEngine::Get().GetSession(),
        NodeId, RelativeToNodeId, &StringHandle ) == HAPI_RESULT_SUCCESS )
    {
        FHoudiniEngineString HoudiniEngineString( StringHandle );
        if ( HoudiniEngineString.ToFString( OutPath ) )
        {
            return true;
        }
    }
    return false;
}

bool
FHoudiniEngineUtils::HapiGetObjectInfos( HAPI_NodeId AssetId, TArray< HAPI_ObjectInfo > & ObjectInfos )
{
    HAPI_NodeInfo LocalAssetNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
	FHoudiniEngine::Get().GetSession(), AssetId,
	&LocalAssetNodeInfo), false);

    int32 ObjectCount = 0;
    if (LocalAssetNodeInfo.type == HAPI_NODETYPE_SOP)
    {
	ObjectCount = 1;
	ObjectInfos.SetNumUninitialized(1);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
	    FHoudiniEngine::Get().GetSession(), LocalAssetNodeInfo.parentId,
	    &ObjectInfos[0]), false);
    }
    else if (LocalAssetNodeInfo.type == HAPI_NODETYPE_OBJ)
    {
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeObjectList(
	    FHoudiniEngine::Get().GetSession(), AssetId, nullptr, &ObjectCount), false);

	if (ObjectCount <= 0)
	{
	    ObjectCount = 1;
	    ObjectInfos.SetNumUninitialized(1);

	    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
		FHoudiniEngine::Get().GetSession(), AssetId,
		&ObjectInfos[0]), false);
	}
	else
	{
	    ObjectInfos.SetNumUninitialized(ObjectCount);
	    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedObjectList(
		FHoudiniEngine::Get().GetSession(), AssetId,
		&ObjectInfos[0], 0, ObjectCount), false);
	}
    }
    else
	return false;

    return true;
}

bool
FHoudiniEngineUtils::HapiGetObjectTransforms( HAPI_NodeId AssetId, TArray< HAPI_Transform > & ObjectTransforms )
{
    HAPI_NodeInfo LocalAssetNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetId,
        &LocalAssetNodeInfo ), false );

    int32 ObjectCount = 1;
    ObjectTransforms.SetNumUninitialized( 1 );

    FMemory::Memzero< HAPI_Transform >( ObjectTransforms[ 0 ] );

    ObjectTransforms[ 0 ].rotationQuaternion[ 3 ] = 1.0f;
    ObjectTransforms[ 0 ].scale[ 0 ] = 1.0f;
    ObjectTransforms[ 0 ].scale[ 1 ] = 1.0f;
    ObjectTransforms[ 0 ].scale[ 2 ] = 1.0f;
    ObjectTransforms[ 0 ].rstOrder = HAPI_SRT;

    if ( LocalAssetNodeInfo.type == HAPI_NODETYPE_SOP )
    {
        // Do nothing. Identity transform will be used for the main parent object.
    }
    else if ( LocalAssetNodeInfo.type == HAPI_NODETYPE_OBJ )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ComposeObjectList(
            FHoudiniEngine::Get().GetSession(), AssetId, nullptr, &ObjectCount ), false );

        if ( ObjectCount <= 0 )
        {
            // Do nothing. Identity transform will be used for the main asset object.
        }
        else
        {
            ObjectTransforms.SetNumUninitialized( ObjectCount );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetComposedObjectTransforms(
                FHoudiniEngine::Get().GetSession(), AssetId,
                HAPI_SRT, &ObjectTransforms[ 0 ], 0, ObjectCount ), false );
        }
    }
    else
        return false;

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateInputNodeForData(
    const HAPI_NodeId& HostAssetId, ALandscapeProxy * LandscapeProxy,
    HAPI_NodeId & ConnectedAssetId, TArray< HAPI_NodeId >& OutCreatedNodeIds,
    const bool& bExportOnlySelected, const bool& bExportCurves,
    const bool& bExportMaterials, const bool& bExportGeometryAsMesh,
    const bool& bExportLighting, const bool& bExportNormalizedUVs,
    const bool& bExportTileUVs, const FBox& AssetBounds,
    const bool& bExportAsHeighfield, const bool& bAutoSelectComponents )
{
#if WITH_EDITOR

    // If we don't have any landscapes or host asset is invalid then there's nothing to do.
    if ( !LandscapeProxy || !FHoudiniEngineUtils::IsHoudiniAssetValid( HostAssetId ) )
        return false;

    // Get runtime settings.
    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }
    
    TSet< ULandscapeComponent * > SelectedComponents;
    if ( bExportOnlySelected )
    {
        const ULandscapeInfo * LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
        if ( LandscapeInfo )
        {
            // Get the currently selected components
            SelectedComponents = LandscapeInfo->GetSelectedComponents();
        }

        if ( bAutoSelectComponents && SelectedComponents.Num() <= 0 && AssetBounds.IsValid )
        {
            // We'll try to use the asset bounds to automatically "select" the components
            for ( int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++ )
            {
                ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];
                if ( !LandscapeComponent )
                    continue;

                FBoxSphereBounds WorldBounds = LandscapeComponent->CalcBounds( LandscapeComponent->ComponentToWorld );

                if ( AssetBounds.IntersectXY( WorldBounds.GetBox() ) )
                    SelectedComponents.Add( LandscapeComponent );
            }

            int32 Num = SelectedComponents.Num();
            HOUDINI_LOG_MESSAGE( TEXT("Landscape input: automatically selected %d components within the asset's bounds."), Num );
        }
    }
    else
    {
        // Add all the components to the selected set
        for ( int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++ )
        {
            ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];
            if ( !LandscapeComponent )
                continue;

            SelectedComponents.Add( LandscapeComponent );
        }
    }

    //--------------------------------------------------------------------------------------------------
    // EXPORT TO HEIGHTFIELD
    //--------------------------------------------------------------------------------------------------
    if ( bExportAsHeighfield )
    {
        // 1. Create the heightfield input node.
        // We'll use its mergeId to connect all the landscape layers,
        // while it's displayId will be our connected asset ID
        FString LandscapeName = LandscapeProxy->GetName() + TEXT("_Merge");
        HAPI_NodeId MergeId = -1;
        if ( !FHoudiniLandscapeUtils::CreateHeightfieldInputNode( ConnectedAssetId, MergeId, LandscapeName ) )
            return false;

        bool bSuccess = false;
        int32 NumComponents = LandscapeProxy->LandscapeComponents.Num();
        if ( !bExportOnlySelected || ( SelectedComponents.Num() == NumComponents ) )
        {
            // Export the whole landscape and its layer as a single heightfield
            bSuccess = FHoudiniLandscapeUtils::CreateHeightfieldFromLandscape( LandscapeProxy, MergeId, OutCreatedNodeIds );
        }
        else
        {
            // Each selected landscape component will be exported as a separate heightfield
            bSuccess = FHoudiniLandscapeUtils::CreateHeightfieldFromLandscapeComponentArray( LandscapeProxy, SelectedComponents, MergeId, OutCreatedNodeIds );
        }
        
        // Reconnect the merge and volvis?
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, MergeId ), false);

        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, nullptr), false);

        return bSuccess;
    }

    //--------------------------------------------------------------------------------------------------
    // EXPORT TO MESH / POINTS
    //--------------------------------------------------------------------------------------------------

    //--------------------------------------------------------------------------------------------------
    // 1. Create an input node if needed
    //--------------------------------------------------------------------------------------------------

    // Check if connected asset id is invalid, if it is not, we need to create an input node.
    if ( ConnectedAssetId < 0 )
    {
        HAPI_NodeId InputNodeId = -1;
        // Create the curve SOP Node
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputNode(
            FHoudiniEngine::Get().GetSession(), &InputNodeId, nullptr ), false );

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( InputNodeId ) )
            return false;

        // We now have a valid id.
        ConnectedAssetId = InputNodeId;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
            FHoudiniEngine::Get().GetSession(), InputNodeId, nullptr ), false );
    }

    //--------------------------------------------------------------------------------------------------
    // 2. Set the part info
    //--------------------------------------------------------------------------------------------------
    int32 ComponentSizeQuads = ( ( LandscapeProxy->ComponentSizeQuads + 1 ) >> LandscapeProxy->ExportLOD ) - 1;
    float ScaleFactor = (float)LandscapeProxy->ComponentSizeQuads / (float)ComponentSizeQuads;

    int32 NumComponents = bExportOnlySelected ? SelectedComponents.Num() : LandscapeProxy->LandscapeComponents.Num();
    int32 VertexCountPerComponent = FMath::Square( ComponentSizeQuads + 1 );
    int32 VertexCount = NumComponents * VertexCountPerComponent;
    if ( !VertexCount )
        return false;
  
    int32 TriangleCount = NumComponents * FMath::Square( ComponentSizeQuads ) * 2;
    int32 QuadCount = NumComponents * FMath::Square( ComponentSizeQuads );
    int32 IndexCount = QuadCount * 4;

    // Create part info
    HAPI_PartInfo Part;
    FMemory::Memzero< HAPI_PartInfo >(Part);
    Part.id = 0;
    Part.nameSH = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_POINT ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_PRIM ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_VERTEX ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_DETAIL ] = 0;
    Part.vertexCount = 0;
    Part.faceCount = 0;
    Part.pointCount = VertexCount;
    Part.type = HAPI_PARTTYPE_MESH;

    // If we are exporting to a mesh, we need vertices and faces
    if ( bExportGeometryAsMesh )
    {
        Part.vertexCount = IndexCount;
        Part.faceCount = QuadCount;
    }

    // Set the part infos
    HAPI_GeoInfo DisplayGeoInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &DisplayGeoInfo ), false );

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0, &Part ), false );

    //--------------------------------------------------------------------------------------------------
    // 3. Extract the landscape data
    //--------------------------------------------------------------------------------------------------
    // Array for the position data
    TArray<FVector> LandscapePositionArray;
    // Array for the normals
    TArray<FVector> LandscapeNormalArray;
    // Array for the UVs
    TArray<FVector> LandscapeUVArray;
    // Array for the vertex index of each point in its component
    TArray<FIntPoint> LandscapeComponentVertexIndicesArray;
    // Array for the tile names per point
    TArray<const char *> LandscapeComponentNameArray;
    // Array for the lightmap values
    TArray<FLinearColor> LandscapeLightmapValues;

    // Extract all the data from the landscape to the arrays
    if ( !FHoudiniLandscapeUtils::ExtractLandscapeData(
        LandscapeProxy, SelectedComponents,
        bExportLighting, bExportTileUVs, bExportNormalizedUVs,
        LandscapePositionArray, LandscapeNormalArray,
        LandscapeUVArray, LandscapeComponentVertexIndicesArray,
        LandscapeComponentNameArray, LandscapeLightmapValues ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 3. Set the corresponding attributes in Houdini
    //--------------------------------------------------------------------------------------------------

    // Create point attribute info containing positions.
    if ( !FHoudiniLandscapeUtils::AddLandscapePositionAttribute( DisplayGeoInfo.nodeId, LandscapePositionArray ) )
        return false;

    // Create point attribute info containing normals.
    if ( !FHoudiniLandscapeUtils::AddLandscapeNormalAttribute( DisplayGeoInfo.nodeId, LandscapeNormalArray ) )
        return false;

    // Create point attribute info containing UVs.
    if ( !FHoudiniLandscapeUtils::AddLandscapeUVAttribute( DisplayGeoInfo.nodeId, LandscapeUVArray ) )
        return false;

    // Create point attribute containing landscape component vertex indices (indices of vertices within the grid - x,y).
    if ( !FHoudiniLandscapeUtils::AddLandscapeComponentVertexIndicesAttribute( DisplayGeoInfo.nodeId, LandscapeComponentVertexIndicesArray ) )
        return false;

    // Create point attribute containing landscape component name.
    if ( !FHoudiniLandscapeUtils::AddLandscapeComponentNameAttribute( DisplayGeoInfo.nodeId, LandscapeComponentNameArray ) )
        return false;

    // Create point attribute info containing lightmap information.
    if ( bExportLighting )
    {
        if ( !FHoudiniLandscapeUtils::AddLandscapeLightmapColorAttribute( DisplayGeoInfo.nodeId, LandscapeLightmapValues ) )
            return false;
    }

    // Set indices if we are exporting full geometry.
    if ( bExportGeometryAsMesh )
    {
        if (!FHoudiniLandscapeUtils::AddLandscapeMeshIndicesAndMaterialsAttribute(
            DisplayGeoInfo.nodeId, bExportMaterials,
            ComponentSizeQuads, QuadCount, 
            LandscapeProxy,  SelectedComponents ) )
            return false;
    }

    // If we are marshalling material information.
    if ( bExportMaterials )
    {
        if ( !FHoudiniLandscapeUtils::AddLandscapeGlobalMaterialAttribute( DisplayGeoInfo.nodeId, LandscapeProxy ) )
            return false;
    }

    // Commit the geo.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId ), false );

#endif

    return true;
}


bool
FHoudiniEngineUtils::HapiCreateInputNodeForData(
    HAPI_NodeId HostAssetId, 
    USplineComponent * SplineComponent,
    HAPI_NodeId & ConnectedAssetId,
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

    if ( (nNumberOfRefinedSplinePoints < nNumberOfControlPoints) || (fSplineResolution <= 0.0f) )
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

    if ( !HapiCreateCurveInputNodeForData(
            HostAssetId, 
            ConnectedAssetId,
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
    for ( int32 n = 0; n < nNumberOfControlPoints; n++ )
    {
        // Getting the Local Transform for positions and scale
        OutlinerMesh.SplineControlPointsTransform[n] = SplineComponent->GetTransformAtSplinePoint(n, ESplineCoordinateSpace::Local, true);

        // ... but we used we used the world rotation
        OutlinerMesh.SplineControlPointsTransform[n].SetRotation(SplineComponent->GetQuaternionAtSplinePoint(n, ESplineCoordinateSpace::World));
    }

    // Cook the spline node.
    FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, nullptr);

#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateInputNodeForData(
    HAPI_NodeId HostAssetId, 
    UStaticMesh * StaticMesh,
    const FTransform& InputTransform,
    HAPI_NodeId & ConnectedAssetId,
    UStaticMeshComponent* StaticMeshComponent /* = nullptr */)
{
#if WITH_EDITOR

    // If we don't have a static mesh, or host asset is invalid, there's nothing to do.
    if ( !StaticMesh || !FHoudiniEngineUtils::IsHoudiniAssetValid( HostAssetId ) )
        return false;

    // Check if connected asset id is valid, if it is not, we need to create an input asset.
    if ( ConnectedAssetId < 0 )
    {
        HAPI_NodeId AssetId = -1;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputNode(
            FHoudiniEngine::Get().GetSession(), &AssetId, nullptr ), false );

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) )
            return false;

        // We now have a valid id.
        ConnectedAssetId = AssetId;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
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
    static constexpr int32 LODIndex = 0;
    FStaticMeshSourceModel & SrcModel = StaticMesh->SourceModels[LODIndex];

    // Load the existing raw mesh.
    FRawMesh RawMesh;
    SrcModel.RawMeshBulkData->LoadRawMesh( RawMesh );

    // Create part.
    HAPI_PartInfo Part;
    FMemory::Memzero< HAPI_PartInfo >( Part );
    Part.id = 0;
    Part.nameSH = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_POINT ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_PRIM ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_VERTEX ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_DETAIL ] = 0;
    Part.vertexCount = RawMesh.WedgeIndices.Num();
    Part.faceCount = RawMesh.WedgeIndices.Num() / 3;
    Part.pointCount = RawMesh.VertexPositions.Num();
    Part.type = HAPI_PARTTYPE_MESH;
    
    HAPI_GeoInfo DisplayGeoInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &DisplayGeoInfo ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0, &Part ), false );

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
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0,
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
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
        0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint,
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
                FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
                0, UVAttributeNameString, &AttributeInfoVertex ), false );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                DisplayGeoInfo.nodeId, 0, UVAttributeNameString, &AttributeInfoVertex,
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
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
            0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            DisplayGeoInfo.nodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex,
            (const float *) ChangedNormals.GetData(),
            0, AttributeInfoVertex.count ), false );
    }

    {
        // If we have instance override vertex colors, first propagate them to our copy of 
        // the RawMesh Vert Colors
        TArray< FLinearColor > ChangedColors;
        if ( StaticMeshComponent &&
            StaticMeshComponent->LODData.IsValidIndex( LODIndex ) &&
            StaticMeshComponent->LODData[LODIndex].OverrideVertexColors &&
            StaticMesh->RenderData &&
            StaticMesh->RenderData->LODResources.IsValidIndex( LODIndex ) )
        {
            FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[LODIndex];
            FStaticMeshRenderData& RenderData = *StaticMesh->RenderData;
            FStaticMeshLODResources& RenderModel = RenderData.LODResources[LODIndex];
            FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;
            if ( RenderData.WedgeMap.Num() > 0 && ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices() )
            {
                // Use the wedge map if it is available as it is lossless.
                int32 NumWedges = RawMesh.WedgeIndices.Num();
                if ( RenderData.WedgeMap.Num() == NumWedges )
                {
                    int32 NumExistingColors = RawMesh.WedgeColors.Num();
                    if ( NumExistingColors < NumWedges )
                    {
                        RawMesh.WedgeColors.AddUninitialized( NumWedges - NumExistingColors );
                    }

                    // Replace mesh colors with override colors
                    for ( int32 i = 0; i < NumWedges; ++i )
                    {
                        FColor WedgeColor = FColor::White;
                        int32 Index = RenderData.WedgeMap[i];
                        if ( Index != INDEX_NONE )
                        {
                            WedgeColor = ColorVertexBuffer.VertexColor( Index );
                        }
                        RawMesh.WedgeColors[i] = WedgeColor;
                    }
                }
            }
        }

        // See if we have colors to upload.
        if ( RawMesh.WedgeColors.Num() > 0 )
        {
            ChangedColors.SetNumUninitialized( RawMesh.WedgeColors.Num() );

            if ( ImportAxis == HRSAI_Unreal )
            {
                // We need to re-index colors for wedges we swapped (due to winding differences).
                for ( int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3 )
                {
                    ChangedColors[WedgeIdx + 0] = RawMesh.WedgeColors[WedgeIdx + 0].ReinterpretAsLinear();
                    ChangedColors[WedgeIdx + 1] = RawMesh.WedgeColors[WedgeIdx + 2].ReinterpretAsLinear();
                    ChangedColors[WedgeIdx + 2] = RawMesh.WedgeColors[WedgeIdx + 1].ReinterpretAsLinear();
                }
            }
            else if ( ImportAxis == HRSAI_Houdini )
            {
                for ( int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); ++WedgeIdx )
                {
                    ChangedColors[WedgeIdx] = RawMesh.WedgeColors[WedgeIdx].ReinterpretAsLinear();
                }
            }
            else
            {
                // Not valid enum value.
                check( 0 );
            }
        }

        if ( ChangedColors.Num() > 0 )
        {
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
                FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
                0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex ), false );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                DisplayGeoInfo.nodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex,
                (const float *)ChangedColors.GetData(), 0, AttributeInfoVertex.count ), false );
        }
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
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
            0, StaticMeshIndices.GetData(), 0, StaticMeshIndices.Num() ), false );

        // We need to generate array of face counts.
        TArray< int32 > StaticMeshFaceCounts;
        StaticMeshFaceCounts.Init( 3, Part.faceCount );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetFaceCounts(
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
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

        if ( FHoudiniApi::AddAttribute( FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0,
            MarshallingAttributeName.c_str(), &AttributeInfoMaterial ) != HAPI_RESULT_SUCCESS )
        {
            bAttributeError = true;
        }

        if ( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            DisplayGeoInfo.nodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfoMaterial,
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
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
            0, MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
            FHoudiniEngine::Get().GetSession(),
            DisplayGeoInfo.nodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks,
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
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
            0, MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution ), false );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
            FHoudiniEngine::Get().GetSession(),
            DisplayGeoInfo.nodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution,
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
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
            0, MarshallingAttributeName.c_str(), &AttributeInfo ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            DisplayGeoInfo.nodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfo,
            PrimitiveAttrs.GetData(), 0, PrimitiveAttrs.Num() ), false );
    }

    // Check if we have vertex attribute data to add
    if ( StaticMeshComponent && StaticMeshComponent->GetOwner() )
    {
        if ( UHoudiniAttributeDataComponent* DataComponent = StaticMeshComponent->GetOwner()->FindComponentByClass<UHoudiniAttributeDataComponent>() )
        {
            bool bSuccess = DataComponent->Upload( DisplayGeoInfo.nodeId, StaticMeshComponent );
            if ( !bSuccess )
            {
                HOUDINI_LOG_ERROR( TEXT( "Upload of attribute data for %s failed" ), *StaticMeshComponent->GetOwner()->GetName() );
            }
        }
    } 

    // Commit the geo.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId ), false );
#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateInputNodeForData(
    HAPI_NodeId HostAssetId,
    TArray< FHoudiniAssetInputOutlinerMesh > & OutlinerMeshArray,
    HAPI_NodeId & ConnectedAssetId,
    const float& SplineResolution )
{
#if WITH_EDITOR
    if ( OutlinerMeshArray.Num() <= 0 )
        return false;

    // Create the merge SOP asset. This will be our "ConnectedAssetId".
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), -1,
        "SOP/merge", nullptr, true, &ConnectedAssetId ), false );

    for ( int32 InputIdx = 0; InputIdx < OutlinerMeshArray.Num(); ++InputIdx )
    {
        auto & OutlinerMesh = OutlinerMeshArray[ InputIdx ];

        bool bInputCreated = false;

        if ( OutlinerMesh.StaticMesh != nullptr )
        {
            // Creating an Input Node for Mesh Data
            bInputCreated = HapiCreateInputNodeForData(
                ConnectedAssetId,
                OutlinerMesh.StaticMesh,
                FTransform::Identity,
                OutlinerMesh.AssetId,
                OutlinerMesh.StaticMeshComponent );
        }
        else if ( OutlinerMesh.SplineComponent != nullptr )
        {
            // Creating an input node for spline data
            bInputCreated = HapiCreateInputNodeForData(
                ConnectedAssetId,
                OutlinerMesh.SplineComponent,
                OutlinerMesh.AssetId,
                OutlinerMesh,
                SplineResolution );
        }

        if ( !bInputCreated )
        {
            OutlinerMesh.AssetId = -1;
            continue;
        }

        // Now we can connect the input node to the asset node.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, InputIdx,
            OutlinerMesh.AssetId), false);
        
        // Updating the Transform
        HAPI_TransformEuler HapiTransform;
        FMemory::Memzero< HAPI_TransformEuler >( HapiTransform );
        FHoudiniEngineUtils::TranslateUnrealTransform( OutlinerMesh.ComponentTransform, HapiTransform );

        HAPI_NodeInfo LocalAssetNodeInfo;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), OutlinerMesh.AssetId,
            &LocalAssetNodeInfo ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetObjectTransform(
            FHoudiniEngine::Get().GetSession(),
            LocalAssetNodeInfo.parentId, &HapiTransform ), false );
    }
#endif
    return true;
}

bool 
FHoudiniEngineUtils::HapiCreateInputNodeForData( 
    HAPI_NodeId HostAssetId, TArray<UObject *>& InputObjects, const TArray< FTransform >& InputTransforms,
    HAPI_NodeId & ConnectedAssetId, TArray< HAPI_NodeId >& OutCreatedNodeIds )
{
#if WITH_EDITOR
    if ( ensure( InputObjects.Num() ) )
    {
        // TODO: No need to merge if there is only one input object if ( InputObjects.Num() == 1 )

        // Create the merge SOP asset. This will be our "ConnectedAssetId".
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
            FHoudiniEngine::Get().GetSession(), -1,
            "SOP/merge", nullptr, true, &ConnectedAssetId ), false );

        for ( int32 InputIdx = 0; InputIdx < InputObjects.Num(); ++InputIdx )
        {
            if ( UStaticMesh* InputStaticMesh = Cast< UStaticMesh >( InputObjects[ InputIdx ] ) )
            {
                FTransform InputTransform = FTransform::Identity;
                if ( InputTransforms.IsValidIndex( InputIdx ) )
                    InputTransform = InputTransforms[ InputIdx ];

                HAPI_NodeId MeshAssetNodeId = -1;
                // Creating an Input Node for Mesh Data
                bool bInputCreated = HapiCreateInputNodeForData( ConnectedAssetId, InputStaticMesh, InputTransform, MeshAssetNodeId, nullptr );
                if ( !bInputCreated )
                {
                    HOUDINI_LOG_WARNING( TEXT( "Error creating input index %d on %d" ), InputIdx, ConnectedAssetId );
                }

                if ( MeshAssetNodeId >= 0 )
                {
                    OutCreatedNodeIds.Add( MeshAssetNodeId );

                    // Now we can connect the input node to the asset node.
                    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
                        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, InputIdx,
                        MeshAssetNodeId ), false );

                    if ( !InputTransform.Equals( FTransform::Identity ) )
                    {
                        // Updating the Transform
                        HAPI_TransformEuler HapiTransform;
                        FMemory::Memzero< HAPI_TransformEuler >( HapiTransform );
                        FHoudiniEngineUtils::TranslateUnrealTransform( InputTransform, HapiTransform );

                        /*
                        FVector InputPosition = InputTransform.GetLocation() / 100.0f;
                        HapiTransform.position[ 0 ] = InputPosition.X;
                        HapiTransform.position[ 1 ] = InputPosition.Y;
                        HapiTransform.position[ 2 ] = InputPosition.Z;

                        FRotator InputRotator = InputTransform.Rotator();
                        HapiTransform.rotationEuler[ 0 ] = InputRotator.Pitch;
                        HapiTransform.rotationEuler[ 1 ] = InputRotator.Yaw;
                        HapiTransform.rotationEuler[ 2 ] = InputRotator.Roll;

                        FVector InputScale = InputTransform.GetScale3D();
                        HapiTransform.scale[ 0 ] = InputScale.X;
                        HapiTransform.scale[ 1 ] = InputScale.Y;
                        HapiTransform.scale[ 2 ] = InputScale.Z;
                        */

                        HAPI_NodeInfo LocalAssetNodeInfo;
                        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
                            FHoudiniEngine::Get().GetSession(), MeshAssetNodeId, &LocalAssetNodeInfo), false);

                        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetObjectTransform(
                            FHoudiniEngine::Get().GetSession(), LocalAssetNodeInfo.parentId, &HapiTransform ), false);
                    }
                }
            }
        }
    }
#endif
    return true;
}

bool
FHoudiniEngineUtils::HapiDisconnectAsset( HAPI_NodeId HostAssetId, int32 InputIndex )
{
#if WITH_EDITOR

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::DisconnectNodeInput(
        FHoudiniEngine::Get().GetSession(), HostAssetId, InputIndex ), false );

#endif // WITH_EDITOR

    return true;
}

bool
FHoudiniEngineUtils::HapiSetAssetTransform( HAPI_NodeId AssetId, const FTransform & Transform )
{
    if (!FHoudiniEngineUtils::IsValidAssetId(AssetId))
        return false;

    // Translate Unreal transform to HAPI Euler one.
    HAPI_TransformEuler TransformEuler;
    FMemory::Memzero< HAPI_TransformEuler >( TransformEuler );
    FHoudiniEngineUtils::TranslateUnrealTransform( Transform, TransformEuler );
    
    // Get the NodeInfo
    HAPI_NodeInfo LocalAssetNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetId,
        &LocalAssetNodeInfo), false);

    if (LocalAssetNodeInfo.type == HAPI_NODETYPE_SOP)
    {
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
            FHoudiniEngine::Get().GetSession(), 
            LocalAssetNodeInfo.parentId, 
            &TransformEuler), false);
    }
    else if (LocalAssetNodeInfo.type == HAPI_NODETYPE_OBJ)
    {
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
            FHoudiniEngine::Get().GetSession(),
            AssetId, &TransformEuler), false);
    }
    else
        return false;

    return true;
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
    FHoudiniCookParams& HoudiniCookParams,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    FString & MeshName, FGuid & BakeGUID )
{
    UPackage * PackageNew = nullptr;

#if WITH_EDITOR
    EBakeMode BakeMode = HoudiniCookParams.StaticMeshBakeMode;
    FString PackageName;
    int32 BakeCount = 0;
    const FGuid & ComponentGUID = HoudiniCookParams.PackageGUID;
    FString ComponentGUIDString = ComponentGUID.ToString().Left(
        FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    while ( true )
    {
        if( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) )
        {
            bool bRemovePackageFromCache = false;

            UPackage* FoundPackage = nullptr;
            if (BakeMode == EBakeMode::ReplaceExisitingAssets)
            {
                TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.BakedStaticMeshPackagesForParts->Find( HoudiniGeoPartObject );
                if ( FoundPointer )
                {
                    if ( ( *FoundPointer ).IsValid() )
                        FoundPackage = ( *FoundPointer ).Get();
                }
                else
                {
                    bRemovePackageFromCache = true;
                }
            }
            else
            {
                TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.CookedTemporaryStaticMeshPackages->Find( HoudiniGeoPartObject );
                if ( FoundPointer )
                {
                    if ( ( *FoundPointer ).IsValid() )
                        FoundPackage = ( *FoundPointer ).Get();
                }
                else
                {
                    bRemovePackageFromCache = true;
                }
            }

            // Find a previously baked / cooked asset
            if ( FoundPackage )
            {
                if ( UPackage::IsEmptyPackage( FoundPackage ) )
                {
                    // This happens when the prior baked output gets renamed, we can delete this 
                    // orphaned package so that we can re-use the name
                    FoundPackage->ClearFlags( RF_Standalone );
                    FoundPackage->ConditionalBeginDestroy();

                    bRemovePackageFromCache = true;
                }
                else
                {
                    if ( CheckPackageSafeForBake( FoundPackage, MeshName ) && !MeshName.IsEmpty() )
                    {
                        return FoundPackage;
                    }
                    else
                    {
                        // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                        //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );

                        // If we're cooking, we'll create a new package, if baking, fail
                        if ( BakeMode != EBakeMode::CookToTemp )
                            return nullptr;
                    }
                }

                bRemovePackageFromCache = true;
            }

            if ( bRemovePackageFromCache )
            {
                // Package is either invalid / not found so we need to remove it from the cache
                if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
                    HoudiniCookParams.BakedStaticMeshPackagesForParts->Remove( HoudiniGeoPartObject );
                else
                    HoudiniCookParams.CookedTemporaryStaticMeshPackages->Remove( HoudiniGeoPartObject );
            }
        }

        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        MeshName = HoudiniCookParams.HoudiniCookManager->GetBakingBaseName( HoudiniGeoPartObject );

        if( BakeCount > 0 )
        {
            MeshName += FString::Printf( TEXT( "_%02d" ), BakeCount );
        }

        switch ( BakeMode )
        {
            case EBakeMode::Intermediate:
            {
                // We only want half of generated guid string.
                FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

                MeshName += TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.ObjectId) + TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.GeoId) + TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.PartId) + TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.SplitId) + TEXT("_") +
                    HoudiniGeoPartObject.SplitName + TEXT("_") +
                    BakeGUIDString;

                PackageName = FPackageName::GetLongPackagePath( HoudiniCookParams.HoudiniAsset->GetOuter()->GetName() ) +
                    TEXT("/") +
                    HoudiniCookParams.HoudiniAsset->GetName() +
                    TEXT("_") +
                    ComponentGUIDString +
                    TEXT("/") +
                    MeshName;
            }
            break;

            case EBakeMode::CookToTemp:
            {
                PackageName = HoudiniCookParams.TempCookFolder.ToString() + TEXT("/") + MeshName;
            }
            break;

            default:
            {
                PackageName = HoudiniCookParams.BakeFolder.ToString() + TEXT("/") + MeshName;
            }
            break;
        }

        // Santize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;

        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniCookParams.IntermediateOuter;
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

    if ( PackageNew && ( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) ) )
    {
        // Add the new package to the cache
        if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
            HoudiniCookParams.BakedStaticMeshPackagesForParts->Add( HoudiniGeoPartObject, PackageNew );
        else
            HoudiniCookParams.CookedTemporaryStaticMeshPackages->Add( HoudiniGeoPartObject, PackageNew );
    }
#endif
    return PackageNew;
}

UPackage *
FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_MaterialInfo & MaterialInfo, FString & MaterialName )
{
    UHoudiniAsset * HoudiniAsset = HoudiniCookParams.HoudiniAsset;
    FString MaterialDescriptor;

    if( HoudiniCookParams.MaterialAndTextureBakeMode != EBakeMode::Intermediate )
        MaterialDescriptor = HoudiniAsset->GetName() + TEXT( "_material_" ) + FString::FromInt( MaterialInfo.nodeId ) + TEXT( "_" );
    else
        MaterialDescriptor = HoudiniAsset->GetName() + TEXT( "_" ) + FString::FromInt( MaterialInfo.nodeId ) + TEXT( "_" );

    return FHoudiniEngineUtils::BakeCreateTextureOrMaterialPackageForComponent(
        HoudiniCookParams, MaterialDescriptor,
        MaterialName );
}

UPackage *
FHoudiniEngineUtils::BakeCreateTextureOrMaterialPackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const FString & MaterialInfoDescriptor,
    FString & MaterialName )
{
    UPackage * PackageNew = nullptr;

#if WITH_EDITOR
    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    UHoudiniAsset * HoudiniAsset = HoudiniCookParams.HoudiniAsset;
    FGuid BakeGUID;
    FString PackageName;

    const FGuid & ComponentGUID = HoudiniCookParams.PackageGUID;
    FString ComponentGUIDString = ComponentGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    if ( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) )
    {
        bool bRemovePackageFromCache = false;

        UPackage* FoundPackage = nullptr;
        if (BakeMode == EBakeMode::ReplaceExisitingAssets)
        {
            TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.BakedMaterialPackagesForIds->Find(MaterialInfoDescriptor);
            if ( FoundPointer )
            {
                if ( (*FoundPointer).IsValid() )
                    FoundPackage = (*FoundPointer).Get();
            }
            else
            {
                bRemovePackageFromCache = true;
            }
        }
        else
        {
            TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.CookedTemporaryPackages->Find(MaterialInfoDescriptor);
            if (FoundPointer)
            {
                if ( (*FoundPointer).IsValid() )
                    FoundPackage = (*FoundPointer).Get();
            }
            else
            {
                bRemovePackageFromCache = true;
            }
        }

        // Find a previously baked / cooked asset
        if ( FoundPackage )
        {
            if ( UPackage::IsEmptyPackage( FoundPackage ) )
            {
                // This happens when the prior baked output gets renamed, we can delete this 
                // orphaned package so that we can re-use the name
                FoundPackage->ClearFlags( RF_Standalone );
                FoundPackage->ConditionalBeginDestroy();

                bRemovePackageFromCache = true;
            }
            else
            {
                if ( CheckPackageSafeForBake( FoundPackage, MaterialName ) && !MaterialName.IsEmpty() )
                {
                    return FoundPackage;
                }
                else
                {
                    // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                    //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );

                    // If we're cooking, we'll create a new package, if baking, fail
                    if ( BakeMode != EBakeMode::CookToTemp )
                        return nullptr;
                }
            }

            bRemovePackageFromCache = true;
        }

        if ( bRemovePackageFromCache )
        {
            // Package is either invalid / not found so we need to remove it from the cache
            if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
                HoudiniCookParams.BakedMaterialPackagesForIds->Remove( MaterialInfoDescriptor );
            else
                HoudiniCookParams.CookedTemporaryPackages->Remove( MaterialInfoDescriptor );
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

        switch (BakeMode)
        {
            case EBakeMode::Intermediate:
            {
                // Generate unique package name.
                PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOuter()->GetName() ) +
                    TEXT("/") +
                    HoudiniAsset->GetName() +
                    TEXT("_") +
                    ComponentGUIDString +
                    TEXT("/") +
                    MaterialName;
            }
            break;

            case EBakeMode::CookToTemp:
            {
                PackageName = HoudiniCookParams.TempCookFolder.ToString() + TEXT("/") + MaterialName;
            }
            break;

            default:
            {
                // Generate unique package name.
                PackageName = HoudiniCookParams.BakeFolder.ToString() + TEXT("/") + MaterialName;
            }
            break;
        }

        // Sanitize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;
        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniCookParams.IntermediateOuter;
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

    if( PackageNew && ( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) ) )
    {
        // Add the new package to the cache
        if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
            HoudiniCookParams.BakedMaterialPackagesForIds->Add( MaterialInfoDescriptor, PackageNew );
        else
            HoudiniCookParams.CookedTemporaryPackages->Add( MaterialInfoDescriptor, PackageNew );
    }
#endif
    return PackageNew;
}

UPackage *
FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_MaterialInfo & MaterialInfo, const FString & TextureType,
    FString & TextureName )
{
    UHoudiniAsset * HoudiniAsset = HoudiniCookParams.HoudiniAsset;
    FString TextureInfoDescriptor;

    if ( HoudiniCookParams.MaterialAndTextureBakeMode != EBakeMode::Intermediate )
    {
        TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT( "_texture_" ) + FString::FromInt( MaterialInfo.nodeId ) +
            TEXT( "_" ) + TextureType + TEXT( "_" );
    }
    else
    {
        TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT( "_" ) + FString::FromInt( MaterialInfo.nodeId ) + TEXT( "_" ) +
            TextureType + TEXT( "_" );
    }

    return FHoudiniEngineUtils::BakeCreateTextureOrMaterialPackageForComponent(
        HoudiniCookParams, TextureInfoDescriptor, TextureName );
}

bool FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
    HAPI_NodeId AssetId,
    FHoudiniCookParams& HoudiniCookParams,
    bool ForceRebuildStaticMesh, bool ForceRecookAll,
    const TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesIn,
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesOut,
    FTransform & ComponentTransform )
{
#if WITH_EDITOR

    if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) || !HoudiniCookParams.HoudiniAsset )
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
    FTransform AssetUnrealTransform;
    if ( !FHoudiniEngineUtils::HapiGetAssetTransform( AssetId, AssetUnrealTransform ) )
        return false;
    ComponentTransform = AssetUnrealTransform;

    // Retrieve information about each object contained within our asset.
    TArray< HAPI_ObjectInfo > ObjectInfos;
    if ( !FHoudiniEngineUtils::HapiGetObjectInfos( AssetId, ObjectInfos ) )
        return false;
    const int32 ObjectCount = ObjectInfos.Num();

    // Retrieve transforms for each object in this asset.
    TArray< HAPI_Transform > ObjectTransforms;
    if ( !FHoudiniEngineUtils::HapiGetObjectTransforms( AssetId, ObjectTransforms ) )
        return false;

    // Containers used for raw data extraction.
    TArray< int32 > VertexList;
    TArray< float > Positions;
    TArray< float > TextureCoordinates[ MAX_STATIC_TEXCOORDS ];
    TArray< float > Normals;
    TArray< float > Colors;
    TArray< float > Alphas;
    TArray< FString > FaceMaterials;
    TArray< int32 > FaceSmoothingMasks;
    TArray< int32 > LightMapResolutions;

    // Retrieve all used unique material ids.
    TSet< HAPI_NodeId > UniqueMaterialIds;
    TSet< HAPI_NodeId > UniqueInstancerMaterialIds;
    TMap< FHoudiniGeoPartObject, HAPI_NodeId > InstancerMaterialMap;
    FHoudiniEngineUtils::ExtractUniqueMaterialIds(
        AssetInfo, UniqueMaterialIds, UniqueInstancerMaterialIds,
        InstancerMaterialMap );

    // Map to hold materials.
    TMap< FString, UMaterialInterface * > Materials;

    // Create materials.
    FHoudiniEngineUtils::HapiCreateMaterials(
        AssetId, HoudiniCookParams, AssetInfo, UniqueMaterialIds,
        UniqueInstancerMaterialIds, Materials, ForceRecookAll );

    // Replace all material assignments
    HoudiniCookParams.HoudiniCookManager->ClearAssignmentMaterials();
    for( const auto& AssPair : Materials )
    {
        HoudiniCookParams.HoudiniCookManager->AddAssignmentMaterial( AssPair.Key, AssPair.Value );
    }

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

        // We need both the display Geos and the editables Geos
        TArray<HAPI_GeoInfo> GeoInfos;

        // First, get the Display Geo Infos
        {
            HAPI_GeoInfo DisplayGeoInfo;
            if (FHoudiniApi::GetDisplayGeoInfo(
                FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId, &DisplayGeoInfo) != HAPI_RESULT_SUCCESS)
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT("Creating Static Meshes: Object [%d %s] unable to retrieve GeoInfo, ")
                    TEXT("- skipping."),
                    ObjectInfo.nodeId, *ObjectName);
            }
            else
            {
                GeoInfos.Add(DisplayGeoInfo);
            }

            // Then get all the GeoInfos for all the editable nodes
            int32 EditableNodeCount = 0;
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
                FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId,
                HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE,
                true, &EditableNodeCount), false);

            if (EditableNodeCount > 0)
            {
                TArray< HAPI_NodeId > EditableNodeIds;
                EditableNodeIds.SetNumUninitialized(EditableNodeCount);
                HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
                    FHoudiniEngine::Get().GetSession(), AssetId,
                    EditableNodeIds.GetData(), EditableNodeCount), false);

                for (int nEditable = 0; nEditable < EditableNodeCount; nEditable++)
                {
                    // don't add editable node if we already have it
                    if ( DisplayGeoInfo.nodeId == EditableNodeIds[nEditable] )
                        continue;

                    HAPI_GeoInfo CurrentEditableGeoInfo;

                    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
                        FHoudiniEngine::Get().GetSession(), 
                        EditableNodeIds[nEditable], 
                        &CurrentEditableGeoInfo), false);

                    GeoInfos.Add(CurrentEditableGeoInfo);
                }
            }
        }

        // Going through the geo infos of the display and editable nodes to create their GeoPartObject/StaticMeshes
        for ( int32 n = 0; n < GeoInfos.Num(); n++ )
        {
            HAPI_GeoInfo GeoInfo = GeoInfos[n];
            HAPI_NodeId GeoId = GeoInfo.nodeId;

            if ( GeoInfo.type == HAPI_GEOTYPE_CURVE )
            {
                // If this geo is a curve, we skip part processing.
                FHoudiniGeoPartObject HoudiniGeoPartObject(
                    TransformMatrix, ObjectName, ObjectName, AssetId,
                    ObjectInfo.nodeId, GeoInfo.nodeId, 0 );
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
                AssetId, ObjectInfo.nodeId, GeoId, HAPI_GROUPTYPE_PRIM,
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
                    FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartIdx,
                    &PartInfo ) != HAPI_RESULT_SUCCESS )
                {
                    // Error retrieving part info.
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve PartInfo, " )
                        TEXT( "- skipping." ),
                        ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );
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
                        ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id );

                    HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !ObjectInfo.isInstanced;
                    HoudiniGeoPartObject.bIsInstancer = false;
                    HoudiniGeoPartObject.bIsCurve = false;
                    HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
                    HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;
                    HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = true;
                    StaticMeshesOut.Add(HoudiniGeoPartObject, nullptr);
                    continue;
                }
                else if ( PartInfo.type == HAPI_PARTTYPE_VOLUME )
                {
                    // Volume Data, this is a Terrain?
                    FHoudiniGeoPartObject HoudiniGeoPartObject(
                        TransformMatrix, ObjectName, PartName, AssetId,
                        ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id );

                    HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !ObjectInfo.isInstanced;
                    HoudiniGeoPartObject.bIsInstancer = false;
                    HoudiniGeoPartObject.bIsCurve = false;
                    HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
                    HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = false;
                    HoudiniGeoPartObject.bIsVolume = true;

                    // We'll set the GeoChanged flag to true if we want to force the landscape reimport
                    HoudiniGeoPartObject.bHasGeoChanged = ( GeoInfo.hasGeoChanged || ForceRebuildStaticMesh || ForceRecookAll );

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
                        AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                        MarshallingAttributeName.c_str(), AttribGeneratedMeshName, GeneratedMeshNames );
                }

                // There are no vertices and no points.
                if ( PartInfo.vertexCount <= 0 && PartInfo.pointCount <= 0 )
                {
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] no points or vertices found, " )
                        TEXT( "- skipping." ),
                        ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );
                    continue;
                }

                // Retrieve material information for this geo part.
                TArray< HAPI_NodeId > FaceMaterialIds;
                HAPI_Bool bSingleFaceMaterial = false;
                bool bMaterialsFound = false;
                bool bMaterialsChanged = false;

                if ( PartInfo.faceCount > 0 )
                {
                    FaceMaterialIds.SetNumUninitialized( PartInfo.faceCount );

                    if ( FHoudiniApi::GetMaterialNodeIdsOnFaces(
                        FHoudiniEngine::Get().GetSession(),
                        GeoInfo.nodeId, PartInfo.id, &bSingleFaceMaterial,
                        &FaceMaterialIds[ 0 ], 0, PartInfo.faceCount ) != HAPI_RESULT_SUCCESS )
                    {
                        // Error retrieving material face assignments.
                        HOUDINI_LOG_MESSAGE(
                            TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments, " )
                            TEXT( "- skipping." ),
                            ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );
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
                                FHoudiniEngine::Get().GetSession(), FaceMaterialIds[ MaterialFaceIdx ],
                                &MaterialInfo ) != HAPI_RESULT_SUCCESS )
                            {
                                continue;
                            }

                            if ( MaterialInfo.hasChanged )
                            {
                                bMaterialsChanged = true;
                                break;
                            }
                        }
                    }
                }

                // Extracting Sockets points
                GetMeshSocketList( AssetId, ObjectInfo.nodeId, GeoId, PartInfo.id, AllSockets, AllSocketsNames, AllSocketsActors );

                // Create geo part object identifier.
                FHoudiniGeoPartObject HoudiniGeoPartObject(
                    TransformMatrix, ObjectName, PartName, AssetId,
                    ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id );

                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !PartInfo.isInstanced;
                HoudiniGeoPartObject.bIsInstancer = ObjectInfo.isInstancer;
                HoudiniGeoPartObject.bIsCurve = ( PartInfo.type == HAPI_PARTTYPE_CURVE );
                HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
                HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;
                HoudiniGeoPartObject.bIsBox = ( PartInfo.type == HAPI_PARTTYPE_BOX );
                HoudiniGeoPartObject.bIsSphere = ( PartInfo.type == HAPI_PARTTYPE_SPHERE );
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
                        HAPI_NodeId const * FoundInstancerMaterialId =
                            InstancerMaterialMap.Find( HoudiniGeoPartObject );
                        if ( FoundInstancerMaterialId )
                        {
                            HAPI_NodeId InstancerMaterialId = *FoundInstancerMaterialId;

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
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
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
                            ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );
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
                    // This is not an instancer, but we do not have vertices, but maybe this
                    // is a point cloud with attribute override instancing, we will assume it is and
                    // let the PostCook figure it out.
                    HoudiniGeoPartObject.bIsInstancer = true;
                    StaticMesh = nullptr;
                    StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );
                    continue;
                }

                // Retrieve all vertex indices.
                VertexList.SetNumUninitialized( PartInfo.vertexCount );

                if ( FHoudiniApi::GetVertexList(
                    FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id,
                    &VertexList[ 0 ], 0, PartInfo.vertexCount ) != HAPI_RESULT_SUCCESS )
                {
                    // Error getting the vertex list.
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list " )
                        TEXT( "- skipping." ),
                        ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );

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
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, GroupName, VertexList, GroupVertexList,
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
                        if ( HoudiniGeoPartObject.bIsBox )
                        {
                            // BOX ELEMENT
                            // For now, boxes are not handled in the cook options, so box will be treated as UCX anyway
                            HAPI_BoxInfo BoxInfo;
                            if ( HAPI_RESULT_SUCCESS == FHoudiniApi::GetBoxInfo(
                                FHoudiniEngine::Get().GetSession(), GeoId, PartInfo.id, &BoxInfo ) )
                            {
                                FKBoxElem BoxCollision;
                                FVector Rotation;

                                BoxCollision.Center.X = BoxInfo.center[0] * GeneratedGeometryScaleFactor;
                                BoxCollision.X = BoxInfo.size[0] * 2.0f * GeneratedGeometryScaleFactor;
                                Rotation.X = BoxInfo.rotation[0];

                                if ( ImportAxis == HRSAI_Unreal )
                                {
                                    BoxCollision.Center.Y = BoxInfo.center[2] * GeneratedGeometryScaleFactor;
                                    BoxCollision.Center.Z = BoxInfo.center[1] * GeneratedGeometryScaleFactor;

                                    BoxCollision.Y = BoxInfo.size[2] * 2.0f * GeneratedGeometryScaleFactor;
                                    BoxCollision.Z = BoxInfo.size[1] * 2.0f * GeneratedGeometryScaleFactor;

                                    Rotation.Y = BoxInfo.rotation[2];
                                    Rotation.Z = BoxInfo.rotation[1];
                                }
                                else
                                {
                                    BoxCollision.Center.Y = BoxInfo.center[1] * GeneratedGeometryScaleFactor;
                                    BoxCollision.Center.Z = BoxInfo.center[2] * GeneratedGeometryScaleFactor;

                                    BoxCollision.Y = BoxInfo.size[1] * 2.0f * GeneratedGeometryScaleFactor;
                                    BoxCollision.Z = BoxInfo.size[2] * 2.0f * GeneratedGeometryScaleFactor;

                                    Rotation.Y = BoxInfo.rotation[1];
                                    Rotation.Z = BoxInfo.rotation[2];
                                }

                                BoxCollision.Orientation = FQuat::MakeFromEuler( Rotation );

                                AggregateCollisionGeo.BoxElems.Add( BoxCollision );

                                bCollisionCreated = true;
                            }
                        }
                        else if ( HoudiniGeoPartObject.bIsSphere )
                        {
                            // SPHERE ELEMENT
                            // For now, Spheres are not handled in the cook options, so spheres will be treated as UCX anyway
                            HAPI_SphereInfo SphereInfo;
                            if ( HAPI_RESULT_SUCCESS == FHoudiniApi::GetSphereInfo(
                                FHoudiniEngine::Get().GetSession(), GeoId, PartInfo.id, &SphereInfo))
                            {
                                FKSphereElem SphereCollision;
                                SphereCollision.Center.X = SphereInfo.center[0] * GeneratedGeometryScaleFactor;

                                if ( ImportAxis == HRSAI_Unreal )
                                {
                                    SphereCollision.Center.Y = SphereInfo.center[2] * GeneratedGeometryScaleFactor;
                                    SphereCollision.Center.Z = SphereInfo.center[1] * GeneratedGeometryScaleFactor;
                                }
                                else
                                {
                                    SphereCollision.Center.Y = SphereInfo.center[1] * GeneratedGeometryScaleFactor;
                                    SphereCollision.Center.Z = SphereInfo.center[2] * GeneratedGeometryScaleFactor;
                                }

                                SphereCollision.Radius = SphereInfo.radius * GeneratedGeometryScaleFactor;

                                AggregateCollisionGeo.SphereElems.Add( SphereCollision );

                                bCollisionCreated = true;
                            }
                        }
                        else
                        {
                            // CONVEX HULL
                            // We need to retrieve the vertices positions
                            HAPI_AttributeInfo AttribInfoPositions;
                            FMemory::Memset< HAPI_AttributeInfo >( AttribInfoPositions, 0 );

                            if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                                PartInfo.id, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions ) )
                            {
                                // Error retrieving positions.
                                HOUDINI_LOG_MESSAGE(
                                    TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data ")
                                    TEXT("- skipping."),
                                    ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName);

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
                        }

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

                    // See if the geometry and scaling factor have changed Or 
                    // If the user asked for a cook manually, we will need to rebuild the static mesh
                    // If not, then we can reuse the corresponding static mesh.
                    if ( GeoInfo.hasGeoChanged || ForceRebuildStaticMesh || ForceRecookAll )
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
                            HOUDINI_LOG_ERROR(
                                TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] geometry has not changed " )
                                TEXT( "but static mesh does not exist - skipping." ),
                                ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );
                            continue;
                        }
                    }

                    // If static mesh was not located, we need to create one.
                    bool bStaticMeshCreated = false;
                    if ( !FoundStaticMesh || *FoundStaticMesh == nullptr )
                    {
                        MeshGuid.Invalidate();
                        UPackage * MeshPackage = FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(
                            HoudiniCookParams, HoudiniGeoPartObject, MeshName, MeshGuid );

                        if( !MeshPackage )
                            continue;

                        StaticMesh = NewObject< UStaticMesh >(
                            MeshPackage, FName( *MeshName ),
                            ( HoudiniCookParams.StaticMeshBakeMode == EBakeMode::Intermediate ) ? RF_NoFlags : RF_Public | RF_Standalone );

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
                    HAPI_AttributeInfo AttribInfoPositions{};
                    HAPI_AttributeInfo AttribLightmapResolution{};
                    HAPI_AttributeInfo AttribFaceMaterials{};
                    HAPI_AttributeInfo AttribInfoColors{};
                    HAPI_AttributeInfo AttribInfoAlpha{};
                    HAPI_AttributeInfo AttribInfoNormals{};
                    HAPI_AttributeInfo AttribInfoFaceSmoothingMasks{};
                    HAPI_AttributeInfo AttribInfoUVs[ MAX_STATIC_TEXCOORDS ]{};

                    if ( bRebuildStaticMesh )
                    {
                        // We may already have gooten the positions when creating the ucx collisions
                        if ( !bAlreadyCalledGetPositions )
                        {
                            // Retrieve position data.
                            if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                                PartInfo.id, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions))
                            {
                                // Error retrieving positions.
                                HOUDINI_LOG_MESSAGE(
                                    TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data ")
                                    TEXT("- skipping."),
                                    ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName);

                                if ( bStaticMeshCreated )
                                    StaticMesh->MarkPendingKill();

                                break;
                            }
                        }

                        // Get lightmap resolution (if present).
                        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, MarshallingAttributeNameLightmapResolution.c_str(),
                            AttribLightmapResolution, LightMapResolutions );

                        // Get name of attribute used for marshalling materials.
                        {
                            FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                                PartInfo.id, MarshallingAttributeNameMaterial.c_str(),
                                AttribFaceMaterials, FaceMaterials );

                            // If material attribute was not found, check fallback compatibility attribute.
                            if ( !AttribFaceMaterials.exists )
                            {
                                FaceMaterials.Empty();
                                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                                    AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                                    PartInfo.id, MarshallingAttributeNameMaterialFallback.c_str(),
                                    AttribFaceMaterials, FaceMaterials );
                            }

                            if ( AttribFaceMaterials.exists && AttribFaceMaterials.owner != HAPI_ATTROWNER_PRIM && AttribFaceMaterials.owner != HAPI_ATTROWNER_DETAIL )
                            {
                                HOUDINI_LOG_WARNING( TEXT( "Static Mesh [%d %s], Geo [%d], Part [%d %s]: unreal_material must be a primitive or detail attribute, ignoring attribute." ),
                                    ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName);
                                AttribFaceMaterials.exists = false;
                                FaceMaterials.Empty();
                            }
                        }

                        // Retrieve color data.
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_COLOR, AttribInfoColors, Colors );

                        // Retrieve alpha data.
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_ALPHA, AttribInfoAlpha, Alphas );

                        // See if we need to transfer color point attributes to vertex attributes.
                        FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                            SplitGroupVertexList,
                            AttribInfoColors, Colors );

                        // See if we need to transfer alpha point attributes to vertex attributes.
                        FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                            SplitGroupVertexList,
                            AttribInfoAlpha, Alphas );

                        // No need to read the normals if we'll recompute them after
                        bool bReadNormals = HoudiniRuntimeSettings->RecomputeNormalsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always;

                        // Retrieve normal data.
                        if ( bReadNormals )
                        {
                            FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                                PartInfo.id, HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals );

                            // See if we need to transfer normal point attributes to vertex attributes.
                            FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                                SplitGroupVertexList, AttribInfoNormals, Normals );
                        }

                        // Retrieve face smoothing data.
                        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, MarshallingAttributeNameFaceSmoothingMask.c_str(),
                            AttribInfoFaceSmoothingMasks, FaceSmoothingMasks );

                        // The second UV set should be called uv2, but we will still check if need to look for a uv1 set.
                        // If uv1 exists, we'll look for uv, uv1, uv2 etc.. if not we'll look for uv, uv2, uv3 etc..
                        bool bUV1Exists = FHoudiniEngineUtils::HapiCheckAttributeExists( AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, "uv1" );

                        // Retrieve UVs.
                        for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                        {
                            std::string UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

                            if ( TexCoordIdx > 0 )
                                UVAttributeName += std::to_string( bUV1Exists ? TexCoordIdx : TexCoordIdx + 1 );

                            const char * UVAttributeNameString = UVAttributeName.c_str();
                            FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, UVAttributeNameString,
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

                        // See if we need to generate tangents, we do this only if normals are present, and if we do not recompute them after
                        bool bGenerateTangents = ( Normals.Num() > 0 );
                        if ( bGenerateTangents && ( HoudiniRuntimeSettings->RecomputeTangentsFlag == EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always ) )
                        {
                            // No need to generate tangents if we'll recompute them after
                            bGenerateTangents = false;
                        }

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

                                if( AttribInfoAlpha.exists )
                                {
                                    WedgeColor.A = FMath::Clamp( Alphas[ WedgeColorIdx ], 0.0f, 1.0f );
                                }
                                else if ( AttribInfoColors.tupleSize == 4 )
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

                        //
                        // Transfer indices:
                        //
                        // Because of the split, we don't need to declare all the vertices in the mesh,
                        // but only the one that are currently used by the split's faces.
                        // We need the indicesMapper array to map those indices from all the vertices
                        // in the part to only the one we need
                        // We also keep track of the needed vertices index to declare them properly afterwards.
                        //

                        // IndicesMapper:
                        // Contains "AllVertices" values
                        // -1 for unused vertices or the "NewIndex" for used vertices
                        // IndicesMapper[ oldIndex ] => newIndex
                        TArray< int32 > IndicesMapper;
                        IndicesMapper.Init( -1, SplitGroupVertexList.Num() );
                        int32 CurrentMapperIndex = 0;

                        // Neededvertices:
                        // Contains the old index of the needed vertices for the current split
                        // NeededVertices[ newIndex ] => oldIndex
                        TArray< int32 > NeededVertices;

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

                            // Converting Old Indices to New Indices:
                            for ( int32 i = 0; i < 3; i++ )
                            {
                                if ( IndicesMapper[ WedgeIndices[ i ] ] < 0 )
                                {
                                    // This old index was not yet "converted" to a new index
                                    NeededVertices.Add( WedgeIndices[ i ] );

                                    IndicesMapper[ WedgeIndices[ i ] ] = CurrentMapperIndex;
                                    CurrentMapperIndex++;
                                }
                               
                                // Replace the old index with the new one
                                WedgeIndices[ i ] = IndicesMapper[ WedgeIndices[ i ] ];
                            }

                            if ( ValidVertexId >= SplitGroupVertexListCount )
                                continue;

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

                        //
                        // Transfer vertex positions:
                        //
                        // Because of the split, we're only interested in the needed vertices.
                        // Instead of declaring all the Positions, we'll only declare the vertices
                        // needed by the current split.
                        //
                        int32 VertexPositionsCount = NeededVertices.Num();// Positions.Num() / 3;
                        RawMesh.VertexPositions.SetNumZeroed( VertexPositionsCount );
                        for ( int32 VertexPositionIdx = 0; VertexPositionIdx < VertexPositionsCount; ++VertexPositionIdx )
                        {
                            int32 NeededVertexIndex = NeededVertices[ VertexPositionIdx ];

                            FVector VertexPosition;
                            if ( ImportAxis == HRSAI_Unreal )
                            {
                                // We need to swap Z and Y coordinate here.
                                VertexPosition.X = Positions[ NeededVertexIndex * 3 + 0 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Y = Positions[ NeededVertexIndex * 3 + 2 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Z = Positions[ NeededVertexIndex * 3 + 1 ] * GeneratedGeometryScaleFactor;
                            }
                            else if ( ImportAxis == HRSAI_Houdini )
                            {
                                // No swap required.
                                VertexPosition.X = Positions[ NeededVertexIndex * 3 + 0 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Y = Positions[ NeededVertexIndex * 3 + 1 ] * GeneratedGeometryScaleFactor;
                                VertexPosition.Z = Positions[ NeededVertexIndex * 3 + 2 ] * GeneratedGeometryScaleFactor;
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
                                    // Make sure this material is in the assignments before replacing it.
                                    if( HoudiniCookParams.HoudiniCookManager->GetAssignmentMaterial( MaterialInterface->GetName() ) )
                                    {
                                        HoudiniCookParams.HoudiniCookManager->AddAssignmentMaterial( MaterialInterface->GetName(), MaterialInterface );
                                    }

                                    // See if we have a replacement material for this.
                                    UMaterialInterface * ReplacementMaterialInterface = HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialInterface->GetName() );
                                    if( ReplacementMaterialInterface )
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
                            TMap< HAPI_NodeId, UMaterialInterface * > NativeMaterials;

                            for(int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
                            {
                                // Get material id for this face.
                                HAPI_NodeId MaterialId = FaceMaterialIds[ FaceIdx ];
                                UMaterialInterface * Material = MaterialDefault;

                                FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                                FHoudiniEngineUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName );
                                UMaterialInterface * const * FoundMaterial = Materials.Find( MaterialShopName );

                                if ( FoundMaterial )
                                    Material = *FoundMaterial;

                                // If we have replacement material for this geo part object and this shop material name.
                                UMaterialInterface * ReplacementMaterial = 
                                    HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

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
                                    HAPI_NodeId MaterialId = FaceMaterialIds[ FaceIdx ];
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
                                    HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

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
                                    HAPI_NodeId MaterialId = -1; 
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
                                        HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

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
                                HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                            if ( ReplacementMaterial )
                                Material = ReplacementMaterial;

                            StaticMesh->StaticMaterials.Empty();
                            StaticMesh->StaticMaterials.Add( FStaticMaterial(Material) );
                        }
                    }

                    // Some mesh generation settings.
                    HoudiniRuntimeSettings->SetMeshBuildSettings( SrcModel->BuildSettings, RawMesh );

                    // By default the distance field resolution should be set to 2.0
                    SrcModel->BuildSettings.DistanceFieldResolutionScale = HoudiniCookParams.GeneratedDistanceFieldResolutionScale;

                    // We need to check light map uv set for correctness. Unreal seems to have occasional issues with
                    // zero UV sets when building lightmaps.
                    int32 LightMapResolutionOverride = -1;
                    if ( SrcModel->BuildSettings.bGenerateLightmapUVs )
                    {
                        // See if we need to disable lightmap generation because of bad UVs.
                        if ( FHoudiniEngineUtils::ContainsInvalidLightmapFaces( RawMesh, StaticMesh->LightMapCoordinateIndex ) )
                        {
                            SrcModel->BuildSettings.bGenerateLightmapUVs = false;

                            HOUDINI_LOG_MESSAGE(
                                TEXT( "Skipping Lightmap Generation: Object [%d %s], Geo [%d], Part [%d %s] invalid face detected " )
                                TEXT( "- skipping." ),
                                ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName );
                        }

                        if( LightMapResolutions.Num() > 0 )
                        {
                            LightMapResolutionOverride = LightMapResolutions[0];
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
                    HoudiniCookParams.HoudiniCookManager->SetStaticMeshGenerationParameters( StaticMesh );

                    // Apply lightmap resolution override if it has been specified
                    if( LightMapResolutionOverride > 0 )
                    {
                        StaticMesh->LightMapResolution = LightMapResolutionOverride;
                    }

                    // Make sure we remove the old simple colliders if needed
                    if( UBodySetup * BodySetup = StaticMesh->BodySetup )
                    {
                        if( !HoudiniGeoPartObject.bHasCollisionBeenAdded )
                        {
#if WITH_PHYSX && (WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR)
                            BodySetup->RemoveSimpleCollision();
#endif
                        }

                        // See if we need to enable collisions on the whole mesh.
                        if( ( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
                            && ( !HoudiniGeoPartObject.bIsSimpleCollisionGeo && !bIsUCXCollidable ) )
                        {
                            // Enable collisions for this static mesh.
                            BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
                        }
                    }

                    // Free any RHI resources.
                    StaticMesh->PreEditChange( nullptr );

                    FHoudiniScopedGlobalSilence ScopedGlobalSilence;

                    TArray< FText > BuildErrors;
                    {
                        SCOPE_CYCLE_COUNTER( STAT_BuildStaticMesh );
                        StaticMesh->Build( true, &BuildErrors );
                    }
                    for ( int32 BuildErrorIdx = 0; BuildErrorIdx < BuildErrors.Num(); ++BuildErrorIdx )
                    {
                        const FText & TextError = BuildErrors[ BuildErrorIdx ];
                        HOUDINI_LOG_MESSAGE(
                            TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d] build error " )
                            TEXT( "- %s." ),
                            ObjectInfo.nodeId, *ObjectName, GeoId, PartIdx, *PartName, SplitId, *( TextError.ToString() ) );
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
                        bool bSimpleCollisionAddedToAggregate = false;
                        if ( SplitGroupName.Contains( "Box" ) )
                        {
                            PrimIndex = GenerateBoxAsSimpleCollision( StaticMesh );
                            if ( !HoudiniGeoPartObject.bIsRenderCollidable )
                            {
                                // If this part is not renderCollidable, we want to extract the Box collider
                                // and add it to the AggregateCollisionGeo to avoid creating extra meshes
                                UBodySetup* bs = StaticMesh->BodySetup;
                                if (bs && bs->AggGeom.BoxElems.IsValidIndex( PrimIndex ) )
                                {
                                    AggregateCollisionGeo.BoxElems.Add( bs->AggGeom.BoxElems[PrimIndex] );
                                    bSimpleCollisionAddedToAggregate = true;
                                }
                            }
                        }
                        else if ( SplitGroupName.Contains( "Sphere" ) )
                        {
                            PrimIndex = GenerateSphereAsSimpleCollision( StaticMesh );
                            if (!HoudiniGeoPartObject.bIsRenderCollidable)
                            {
                                // If this part is not renderCollidable, we want to extract the Sphere collider
                                // and add it to the AggregateCollisionGeo to avoid creating extra meshes
                                UBodySetup* bs = StaticMesh->BodySetup;
                                if ( bs && bs->AggGeom.SphereElems.IsValidIndex( PrimIndex ) )
                                {
                                    AggregateCollisionGeo.SphereElems.Add(bs->AggGeom.SphereElems[PrimIndex]);
                                    bSimpleCollisionAddedToAggregate = true;
                                }
                            }
                        }
                        else if ( SplitGroupName.Contains( "Capsule" ) )
                        {
                            PrimIndex = GenerateSphylAsSimpleCollision( StaticMesh );
                            if (!HoudiniGeoPartObject.bIsRenderCollidable)
                            {
                                // If this part is not renderCollidable, we want to extract the Capsule collider
                                // and add it to the AggregateCollisionGeo to avoid creating extra meshes
                                UBodySetup* bs = StaticMesh->BodySetup;
                                if ( bs && bs->AggGeom.SphylElems.IsValidIndex( PrimIndex ) )
                                {
                                    AggregateCollisionGeo.SphylElems.Add(bs->AggGeom.SphylElems[PrimIndex]);
                                    bSimpleCollisionAddedToAggregate = true;
                                }
                            }
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

                            if (!HoudiniGeoPartObject.bIsRenderCollidable)
                            {
                                // If this part is not renderCollidable, we want to extract the KDOP collider
                                // and add it to the AggregateCollisionGeo to avoid creating extra meshes
                                UBodySetup* bs = StaticMesh->BodySetup;
                                if (bs && bs->AggGeom.ConvexElems.IsValidIndex( PrimIndex ) )
                                {
                                    AggregateCollisionGeo.ConvexElems.Add(bs->AggGeom.ConvexElems[PrimIndex]);
                                    bSimpleCollisionAddedToAggregate = true;
                                }
                            }
                        }

                        if ( PrimIndex == INDEX_NONE )
                        {
                            HoudiniGeoPartObject.bIsSimpleCollisionGeo = false;
                            HoudiniGeoPartObject.bIsCollidable = false;
                            HoudiniGeoPartObject.bIsRenderCollidable = false;
                        }
                        else
                        {
                            if (bSimpleCollisionAddedToAggregate)
                            {
                                // We will add the colliders after
                                bHasAggregateGeometryCollision = true;
                                continue;
                            }
                            else
                            {
                                // We don't want these collisions to be removed
                                HoudiniGeoPartObject.bHasCollisionBeenAdded = true;
                            }
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

            if( UBodySetup * BodySetup = StaticMesh->BodySetup )
            {
                // Unreal caches the Navigation Collision and never updates it for StaticMeshes,
                // so we need to manually flush and recreate the data to have proper navigation collision
                if( StaticMesh->NavCollision )
                {
                    StaticMesh->NavCollision->CookedFormatData.FlushData();
                    StaticMesh->NavCollision->GatherCollision();
                    StaticMesh->NavCollision->Setup( BodySetup );
                }
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

void
FHoudiniEngineUtils::HapiCreateMaterials(
    HAPI_NodeId AssetId,
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_AssetInfo & AssetInfo,
    const TSet< HAPI_NodeId > & UniqueMaterialIds,
    const TSet< HAPI_NodeId > & UniqueInstancerMaterialIds,
    TMap< FString, UMaterialInterface * > & Materials,
    const bool& bForceRecookAll )
{
#if WITH_EDITOR

    // Empty returned materials.
    Materials.Empty();

    if ( UniqueMaterialIds.Num() == 0 )
        return;

    // Update context for generated materials (will trigger when object goes out of scope).
    FMaterialUpdateContext MaterialUpdateContext;

    // Default Houdini material.
    UMaterial * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
    Materials.Add( HAPI_UNREAL_DEFAULT_MATERIAL_NAME, DefaultMaterial );

    // Factory to create materials.
    UMaterialFactoryNew * MaterialFactory = NewObject< UMaterialFactoryNew >();
    MaterialFactory->AddToRoot();

    for ( TSet< HAPI_NodeId >::TConstIterator IterMaterialId( UniqueMaterialIds ); IterMaterialId; ++IterMaterialId )
    {
        HAPI_NodeId MaterialId = *IterMaterialId;

        // Get material information.
        HAPI_MaterialInfo MaterialInfo;
        if ( FHoudiniApi::GetMaterialInfo(
            FHoudiniEngine::Get().GetSession(),
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
            
            UMaterial * Material = Cast< UMaterial >( HoudiniCookParams.HoudiniCookManager->GetAssignmentMaterial( MaterialShopName ) );
            bool bCreatedNewMaterial = false;

            if ( Material )
            {
                // If cached material exists and has not changed, we can reuse it.
                if ( !MaterialInfo.hasChanged && !bForceRecookAll )
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
                EObjectFlags ObjFlags = ( HoudiniCookParams.MaterialAndTextureBakeMode == EBakeMode::Intermediate ) ? RF_Transactional : RF_Public | RF_Standalone;

                // Create material package and get material name.
                UPackage * MaterialPackage = FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(
                    HoudiniCookParams, MaterialInfo, MaterialName );

                // Create new material.
                Material = (UMaterial *) MaterialFactory->FactoryCreateNew(
                    UMaterial::StaticClass(), MaterialPackage,
                    *MaterialName, ObjFlags, NULL, GWarn );

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

            // Reset material expressions.
            Material->Expressions.Empty();

            // Generate various components for this material.
            bool bMaterialComponentCreated = false;
            int32 MaterialNodeY = FHoudiniEngineUtils::MaterialExpressionNodeY;

            // By default we mark material as opaque. Some of component creators can change this.
            Material->BlendMode = BLEND_Opaque;

            // Extract diffuse plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentDiffuse(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract opacity plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentOpacity(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract opacity mask plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentOpacityMask(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract normal plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentNormal(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract specular plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentSpecular(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract roughness plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentRoughness(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract metallic plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentMetallic(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

            // Extract emissive plane.
            bMaterialComponentCreated |= FHoudiniEngineUtils::CreateMaterialComponentEmissive(
                HoudiniCookParams, Material, MaterialInfo, NodeInfo, NodeParams, MaterialNodeY );

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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY)
{
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
            Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag );
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
            Material, UMaterialExpressionVertexColor::StaticClass(), NAME_None, ObjectFlag );
        ExpressionVertexColor->Desc = GeneratingParameterNameVertexColor;
    }

    // Add expression.
    Material->Expressions.Add( ExpressionVertexColor );

    // Material should have at least one multiply expression.
    UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast< UMaterialExpressionMultiply >( MaterialExpression );
    if ( !MaterialExpressionMultiply )
        MaterialExpressionMultiply = NewObject< UMaterialExpressionMultiply >(
            Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag );

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );

    if ( ParmDiffuseTextureIdx >= 0 )
    {
        GeneratingParameterNameDiffuseTexture = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );
    }
    else
    {
        ParmDiffuseTextureIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );

        if ( ParmDiffuseTextureIdx >= 0 )
            GeneratingParameterNameDiffuseTexture = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );
    }

    // See if uniform color is available.
    int32 ParmDiffuseColorIdx =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_DIFFUSE_0 );

    if ( ParmDiffuseColorIdx >= 0 )
    {
        GeneratingParameterNameUniformColor = TEXT( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_0 );
    }
    else
    {
        ParmDiffuseColorIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_DIFFUSE_1 );

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
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureDiffuseName;
                bool bCreatedNewTextureDiffuse = false;

                // Create diffuse texture package, if this is a new diffuse texture.
                if ( !TextureDiffusePackage )
                {
                    TextureDiffusePackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
                        TextureDiffuseName );
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

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureDiffuse->SetFlags( RF_Public | RF_Standalone );

                // Create diffuse sampling expression, if needed.
                if ( !ExpressionTextureSample )
                {
                    ExpressionTextureSample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );
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
                Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag );

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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameters.
    FString GeneratingParameterNameTexture = TEXT( "" );

    UMaterialExpression * MaterialExpression = Material->OpacityMask.Expression;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_OPACITY_1 );

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

        bool bColorAlphaFound = ( OpacityImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA ) ) && OpacityImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_COLOR ) ) );

        if ( bFoundImagePlanes && bColorAlphaFound )
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
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureOpacityName;
                bool bCreatedNewTextureOpacity = false;

                // Create opacity texture package, if this is a new opacity texture.
                if ( !TextureOpacityPackage )
                {
                    TextureOpacityPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
                        TextureOpacityName );
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

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureOpacity->SetFlags(RF_Public | RF_Standalone);

                // Create opacity sampling expression, if needed.
                if ( !ExpressionTextureOpacitySample )
                {
                    ExpressionTextureOpacitySample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );
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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;
    float OpacityValue = 1.0f;
    bool bNeedsTranslucency = false;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_ALPHA );

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
                        Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag );
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
                Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag );

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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY)
{
    bool bExpressionCreated = false;
    bool bTangentSpaceNormal = true;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_NORMAL_0 );

    if ( ParmNameNormalIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_0 );
    }
    else
    {
        ParmNameNormalIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_NORMAL_1 );

        if ( ParmNameNormalIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_1 );
    }

    if ( ParmNameNormalIdx >= 0 )
    {
        // Retrieve space for this normal texture.
        int32 ParmNormalTypeIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE );

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
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

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
                            HoudiniCookParams,
                            MaterialInfo,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                            TextureNormalName );
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

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureNormal->SetFlags(RF_Public | RF_Standalone);

                // Create normal sampling expression, if needed.
                if ( !ExpressionNormal )
                    ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

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
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );

        if ( ParmNameBaseIdx >= 0 )
        {
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );
        }
        else
        {
            ParmNameBaseIdx =
                FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );

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
                    FHoudiniEngine::Get().GetSession(),
                    MaterialInfo.nodeId, &ImageInfo );

                if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
                {
                    // Create texture.
                    FString TextureNormalName;
                    bool bCreatedNewTextureNormal = false;

                    // Create normal texture package, if this is a new normal texture.
                    if ( !TextureNormalPackage )
                    {
                        TextureNormalPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                            HoudiniCookParams,
                            MaterialInfo,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                            TextureNormalName );
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

                    if ( BakeMode == EBakeMode::CookToTemp )
                        TextureNormal->SetFlags( RF_Public | RF_Standalone );

                    // Create normal sampling expression, if needed.
                    if ( !ExpressionNormal )
                        ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                            Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_SPECULAR_0 );

    if ( ParmNameSpecularIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_SPECULAR_0 );
    }
    else
    {
        ParmNameSpecularIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_SPECULAR_1 );

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
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureSpecularName;
                bool bCreatedNewTextureSpecular = false;

                // Create specular texture package, if this is a new specular texture.
                if ( !TextureSpecularPackage )
                {
                    TextureSpecularPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
                        TextureSpecularName );
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

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureSpecular->SetFlags( RF_Public | RF_Standalone );

                // Create specular sampling expression, if needed.
                if ( !ExpressionSpecular )
                {
                    ExpressionSpecular = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );
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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_SPECULAR_0 );

    if( ParmNameSpecularColorIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_COLOR_SPECULAR_0 );
    }
    else
    {
        ParmNameSpecularColorIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_SPECULAR_1 );

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
                    Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag );
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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY)
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_ROUGHNESS_0 );

    if ( ParmNameRoughnessIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_0 );
    }
    else
    {
        ParmNameRoughnessIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_ROUGHNESS_1 );

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
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureRoughnessName;
                bool bCreatedNewTextureRoughness = false;

                // Create roughness texture package, if this is a new roughness texture.
                if ( !TextureRoughnessPackage )
                {
                    TextureRoughnessPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
                        TextureRoughnessName );
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

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureRoughness->SetFlags( RF_Public | RF_Standalone );

                // Create roughness sampling expression, if needed.
                if ( !ExpressionRoughness )
                    ExpressionRoughness = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_0 );

    if ( ParmNameRoughnessValueIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_0 );
    }
    else
    {
        ParmNameRoughnessValueIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_1 );

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
                    Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag );
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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY)
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_METALLIC );

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
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureMetallicName;
                bool bCreatedNewTextureMetallic = false;

                // Create metallic texture package, if this is a new metallic texture.
                if ( !TextureMetallicPackage )
                {
                    TextureMetallicPackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
                        TextureMetallicName );
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

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureMetallic->SetFlags( RF_Public | RF_Standalone );

                // Create metallic sampling expression, if needed.
                if ( !ExpressionMetallic )
                    ExpressionMetallic = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

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
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_METALLIC );

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
                    Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag );
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
    FHoudiniCookParams& HoudiniCookParams,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
    const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT("");

    // Emissive texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if emissive texture is available.
    int32 ParmNameEmissiveIdx =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_MAP_EMISSIVE );

    if ( ParmNameEmissiveIdx >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_EMISSIVE );
    }

    if ( ParmNameEmissiveIdx >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineUtils::HapiExtractImage(
            NodeParams[ ParmNameEmissiveIdx ].id, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionEmissive =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->EmissiveColor.Expression );

            UTexture2D * TextureEmissive = nullptr;
            if ( ExpressionEmissive )
            {
                TextureEmissive = Cast< UTexture2D >( ExpressionEmissive->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->EmissiveColor.Expression )
                {
                    Material->EmissiveColor.Expression->ConditionalBeginDestroy();
                    Material->EmissiveColor.Expression = nullptr;
                }
            }

            UPackage * TextureEmissivePackage = nullptr;
            if ( TextureEmissive )
                TextureEmissivePackage = Cast< UPackage >( TextureEmissive->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureEmissiveName;
                bool bCreatedNewTextureEmissive = false;

                // Create emissive texture package, if this is a new emissive texture.
                if ( !TextureEmissivePackage )
                {
                    TextureEmissivePackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
                        TextureEmissiveName );
                }

                // Create emissive texture, if we need to create one.
                if ( !TextureEmissive )
                    bCreatedNewTextureEmissive = true;

                // Reuse existing emissive texture, or create new one.
                TextureEmissive = FHoudiniEngineUtils::CreateUnrealTexture(
                    TextureEmissive, ImageInfo,
                    TextureEmissivePackage, TextureEmissiveName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureEmissive->SetFlags( RF_Public | RF_Standalone );

                // Create emissive sampling expression, if needed.
                if ( !ExpressionEmissive )
                    ExpressionEmissive = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

                // Record generating parameter.
                ExpressionEmissive->Desc = GeneratingParameterName;
                ExpressionEmissive->ParameterName = *GeneratingParameterName;

                ExpressionEmissive->Texture = TextureEmissive;
                ExpressionEmissive->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionEmissive->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
                ExpressionEmissive->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionEmissive );
                Material->EmissiveColor.Expression = ExpressionEmissive;

                bExpressionCreated = true;
            }
        }
    }

    int32 ParmNameEmissiveValueIdx =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_0 );

    if ( ParmNameEmissiveValueIdx >= 0 )
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_EMISSIVE_0 );
    else
    {
        ParmNameEmissiveValueIdx =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeParams, NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_1 );

        if ( ParmNameEmissiveValueIdx >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_EMISSIVE_1 );
    }

    if ( !bExpressionCreated && ParmNameEmissiveValueIdx >= 0 )
    {
        // Emissive color is available.

        FLinearColor Color = FLinearColor::White;
        const HAPI_ParmInfo & ParmInfo = NodeParams[ ParmNameEmissiveValueIdx ];

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float*)&Color.R,
            ParmInfo.floatValuesIndex, ParmInfo.size ) == HAPI_RESULT_SUCCESS )
        {
            if ( ParmInfo.size == 3 )
                Color.A = 1.0f;

            UMaterialExpressionConstant4Vector * ExpressionEmissiveColor =
                Cast< UMaterialExpressionConstant4Vector >( Material->EmissiveColor.Expression );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionEmissiveColor )
            {
                // Otherwise new expression is of a different type.
                if ( Material->EmissiveColor.Expression )
                {
                    Material->EmissiveColor.Expression->ConditionalBeginDestroy();
                    Material->EmissiveColor.Expression = nullptr;
                }

                ExpressionEmissiveColor = NewObject< UMaterialExpressionConstant4Vector >(
                    Material, UMaterialExpressionConstant4Vector::StaticClass(), NAME_None, ObjectFlag );
            }

            // Record generating parameter.
            ExpressionEmissiveColor->Desc = GeneratingParameterName;
            if ( ExpressionEmissiveColor->CanRenameNode() )
                ExpressionEmissiveColor->SetEditableName( *GeneratingParameterName );

            ExpressionEmissiveColor->Constant = Color;

            // Offset node placement.
            ExpressionEmissiveColor->MaterialExpressionEditorX = FHoudiniEngineUtils::MaterialExpressionNodeX;
            ExpressionEmissiveColor->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionEmissiveColor );
            Material->EmissiveColor.Expression = ExpressionEmissiveColor;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
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
    TCHAR HFS_ENV_VARIABLE[PLATFORM_MAX_FILEPATH_LENGTH] = { 0 };

    // Look up HAPI_PATH environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
    FPlatformMisc::GetEnvironmentVariable( TEXT( "HAPI_PATH" ), HFS_ENV_VARIABLE, PLATFORM_MAX_FILEPATH_LENGTH );
    if ( *HFS_ENV_VARIABLE )
        HFSPath = &HFS_ENV_VARIABLE[ 0 ];

    // Look up environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
    FPlatformMisc::GetEnvironmentVariable( TEXT( "HFS" ), HFS_ENV_VARIABLE, PLATFORM_MAX_FILEPATH_LENGTH );
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
    FString HoudiniLocation = HOUDINI_ENGINE_HFS_PATH;
    FString LibHAPIPath;

#if PLATFORM_WINDOWS

    // On Windows, we have also hardcoded HFS path in plugin configuration file; attempt to load from it.
    HFSPath = FString::Printf( TEXT( "%s/%s" ), *HoudiniLocation, HAPI_HFS_SUBFOLDER_WINDOWS );

    // Create full path to libHAPI binary.
    LibHAPIPath = FString::Printf( TEXT( "%s/%s" ), *HFSPath, *LibHAPIName );

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
    HoudiniLocation = FString::Printf(
        TEXT( "C:\\Program Files\\Side Effects Software\\Houdini %s\\%s" ), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_WINDOWS );

#else

#   if PLATFORM_MAC

    // Attempt to load from standard Mac OS X installation.
    HoudiniLocation = FString::Printf(
        TEXT("/Applications/Houdini/Houdini%s/Frameworks/Houdini.framework/Versions/%s/Libraries"), *HoudiniVersionString, *HoudiniVersionString );

#   elif PLATFORM_LINUX

    // Attempt to load from standard Linux installation.
    // TODO: Support this.
    //HoudiniLocation = FString::Printf(
        //TEXT( "/opt/dev%s/" ) + HAPI_HFS_SUBFOLDER_LINUX, *HoudiniVersionString );

#   endif

#endif

    // Create full path to libHAPI binary.
    LibHAPIPath = FString::Printf( TEXT( "%s/%s" ), *HoudiniLocation, *LibHAPIName );

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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
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
    TSet< HAPI_NodeId > & MaterialIds,
    TSet< HAPI_NodeId > & InstancerMaterialIds,
    TMap< FHoudiniGeoPartObject, HAPI_NodeId > & InstancerMaterialMap )
{
    // Empty passed incontainers.
    MaterialIds.Empty();
    InstancerMaterialIds.Empty();
    InstancerMaterialMap.Empty();

    TArray< HAPI_ObjectInfo > ObjectInfos;
    if ( !HapiGetObjectInfos( AssetInfo.nodeId, ObjectInfos ) )
        return false;
    const int32 ObjectCount = ObjectInfos.Num();

    // Iterate through all objects.
    for ( int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx )
    {
        // Retrieve object at this index.
        const HAPI_ObjectInfo & ObjectInfo = ObjectInfos[ ObjectIdx ];

        // This is a loop that goes over once and stops. We use this so we can then
        // exit out of the scope using break or continue.
        for ( int32 Idx = 0; Idx < 1; ++Idx )
        {
            // Get Geo information.
            HAPI_GeoInfo GeoInfo;
            if ( FHoudiniApi::GetDisplayGeoInfo(
                FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId, &GeoInfo ) != HAPI_RESULT_SUCCESS )
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
                    FHoudiniEngine::Get().GetSession(),
                    GeoInfo.nodeId, PartIdx, &PartInfo ) != HAPI_RESULT_SUCCESS )
                {
                    continue;
                }

                // Retrieve material information for this geo part.
                HAPI_Bool bSingleFaceMaterial = false;
                bool bMaterialsFound = false;

                if ( PartInfo.faceCount > 0 )
                {
                    TArray< HAPI_NodeId > FaceMaterialIds;
                    FaceMaterialIds.SetNumUninitialized( PartInfo.faceCount );

                    if ( FHoudiniApi::GetMaterialNodeIdsOnFaces(
                        FHoudiniEngine::Get().GetSession(),
                        GeoInfo.nodeId, PartInfo.id, &bSingleFaceMaterial,
                        &FaceMaterialIds[ 0 ], 0, PartInfo.faceCount ) != HAPI_RESULT_SUCCESS )
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
                        HAPI_NodeId InstanceMaterialId = -1;

                        if ( FHoudiniApi::GetMaterialNodeIdsOnFaces(
                            FHoudiniEngine::Get().GetSession(),
                            GeoInfo.nodeId, PartInfo.id, &bSingleFaceMaterial,
                            &InstanceMaterialId, 0, 1 ) != HAPI_RESULT_SUCCESS )
                        {
                            continue;
                        }

                        MaterialIds.Add( InstanceMaterialId );

                        if ( InstanceMaterialId != -1 )
                        {
                            FHoudiniGeoPartObject GeoPartObject( AssetInfo.nodeId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id );
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

bool
FHoudiniEngineUtils::GetAssetNames(
    UHoudiniAsset * HoudiniAsset, HAPI_AssetLibraryId & OutAssetLibraryId,
    TArray< HAPI_StringHandle > & OutAssetNames )
{
    OutAssetLibraryId = -1;
    OutAssetNames.Empty();

    if ( FHoudiniEngineUtils::IsInitialized() && HoudiniAsset )
    {
        FString AssetFileName = HoudiniAsset->GetAssetFileName();
        HAPI_Result Result = HAPI_RESULT_FAILURE;
        HAPI_AssetLibraryId AssetLibraryId = -1;
        int32 AssetCount = 0;
        TArray< HAPI_StringHandle > AssetNames;

        if ( FPaths::IsRelative( AssetFileName ) && ( FHoudiniEngine::Get().GetSession()->type != HAPI_SESSION_INPROCESS ) )
            AssetFileName = FPaths::ConvertRelativePathToFull( AssetFileName );

        if ( !AssetFileName.IsEmpty() && FPaths::FileExists( AssetFileName ) )
        {
            // We'll need to modify the file name for expanded .hda
            FString FileExtension = FPaths::GetExtension( AssetFileName );
            if ( FileExtension.Compare( TEXT( "hdalibrary" ), ESearchCase::IgnoreCase ) == 0 )
            {
                // the .hda directory is what we're interested in loading
                AssetFileName = FPaths::GetPath( AssetFileName );
            }

            // File does exist, we can load asset from file.
            std::string AssetFileNamePlain;
            FHoudiniEngineUtils::ConvertUnrealString( AssetFileName, AssetFileNamePlain );

            Result = FHoudiniApi::LoadAssetLibraryFromFile(
                FHoudiniEngine::Get().GetSession(), AssetFileNamePlain.c_str(), true, &AssetLibraryId );
        }

        // Try to load the asset from memory if loading from file failed
        if ( Result != HAPI_RESULT_SUCCESS )
        {
            // Expanded hdas cannot be loaded from  Memory
            FString FileExtension = FPaths::GetExtension( AssetFileName );
            if ( FileExtension.Compare( TEXT( "hdalibrary" ), ESearchCase::IgnoreCase ) == 0 )
            {
                HOUDINI_LOG_ERROR( TEXT( "Error loading expanded Asset %s: source asset file not found." ), *AssetFileName );
                return false;
            }
            else
            {
                // Warn the user that we are loading from memory
                HOUDINI_LOG_WARNING( TEXT( "Asset %s, loading from Memory: source asset file not found."), *AssetFileName );

                // Otherwise we will try to load from buffer we've cached.
                Result = FHoudiniApi::LoadAssetLibraryFromMemory(
                    FHoudiniEngine::Get().GetSession(),
                    reinterpret_cast<const char *>( HoudiniAsset->GetAssetBytes() ),
                    HoudiniAsset->GetAssetBytesCount(), true, &AssetLibraryId );
            }
        }

        if ( Result != HAPI_RESULT_SUCCESS )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Error loading asset library for %s: %s" ), *AssetFileName, *FHoudiniEngineUtils::GetErrorDescription() );
            return false;
        }

        Result = FHoudiniApi::GetAvailableAssetCount( FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetCount );
        if ( Result != HAPI_RESULT_SUCCESS )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Error getting asset count for %s: %s" ), *AssetFileName, *FHoudiniEngineUtils::GetErrorDescription() );
            return false;
        }

        if( AssetCount <= 0 )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Could not find an asset in library %s" ), *AssetFileName );
            return false;
        }

        AssetNames.SetNumUninitialized( AssetCount );

        Result = FHoudiniApi::GetAvailableAssets( FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetNames[ 0 ], AssetCount );
        if ( Result != HAPI_RESULT_SUCCESS )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Unable to retrieve asset names for %s: %s" ), *AssetFileName, *FHoudiniEngineUtils::GetErrorDescription() );
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
#if WITH_PHYSX && (WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR)
    if ( !HoudiniGeoPartObject.bHasCollisionBeenAdded )
        BodySetup->RemoveSimpleCollision();
#endif

    BodySetup->AddCollisionFrom( AggregateCollisionGeo );
    BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;

    BodySetup->ClearPhysicsMeshes();
#if WITH_PHYSX && (WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR)
    BodySetup->InvalidatePhysicsData();
#endif

#if WITH_EDITOR
    RefreshCollisionChange( *StaticMesh );
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
    for ( TActorIterator<AActor> ActorItr( editorWorld ); ActorItr; ++ActorItr )
    {
        // Same as with the Object Iterator, access the subclass instance with the * or -> operators.
        AActor *Actor = *ActorItr;
        if ( !Actor )
            continue;

        for ( int32 StringIdx = 0; StringIdx < ActorStringArray.Num(); StringIdx++ )
        {
            if ( Actor->GetName() != ActorStringArray[ StringIdx ]
                 && Actor->GetActorLabel() != ActorStringArray[ StringIdx ] )
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

int32
FHoudiniEngineUtils::GetUPropertyAttributesList(
    const FHoudiniGeoPartObject& GeoPartObject,
    TArray< UPropertyAttribute >& AllUProps,
    const HAPI_AttributeOwner& AttributeOwner )
{
    HAPI_NodeId NodeId = GeoPartObject.HapiGeoGetNodeId();
    HAPI_PartInfo PartInfo;
    if ( !GeoPartObject.HapiPartGetInfo( PartInfo ) )
        return 0;

    HAPI_PartId PartId = GeoPartObject.GetPartId();

    int32 nUPropCount = 0;

    // Get All attribute names for that part
    int32 nAttribCount = PartInfo.attributeCounts[ AttributeOwner ];

    TArray<HAPI_StringHandle> AttribNameSHArray;
    AttribNameSHArray.SetNum( nAttribCount );

    if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeNames(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartInfo.id, AttributeOwner,
        AttribNameSHArray.GetData(), nAttribCount ) )
        return 0;

    // As the uprop can be on primitives, if so, we'll have to identify
    // if a split occured during the mesh creation, and find a suitable primitive
    // that correspond to that split
    bool HandleSplit = false;
    int PrimNumberForSplit = -1;
    if ( AttributeOwner != HAPI_ATTROWNER_DETAIL )
    {
        if ( !GeoPartObject.SplitName.IsEmpty() )
        {
            HandleSplit = true;

            // Since the meshes have been split, we need to find a primitive that belongs to the proper group
            // so we can read the proper value for its uprop attribute
            TArray< int32 > PartGroupMembership;
            FHoudiniEngineUtils::HapiGetGroupMembership(
                GeoPartObject.AssetId, GeoPartObject.GetObjectId(), GeoPartObject.GetGeoId(), GeoPartObject.GetPartId(), 
                HAPI_GROUPTYPE_PRIM, GeoPartObject.SplitName, PartGroupMembership );

            for ( int32 n = 0; n < PartGroupMembership.Num(); n++ )
            {
                if ( PartGroupMembership[ n ] > 0 )
                {
                    PrimNumberForSplit = n;
                    break;
                }
            }
        }
    }

    for ( int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx )
    {
        FString HapiString = TEXT("");
        FHoudiniEngineString HoudiniEngineString( AttribNameSHArray[ Idx ] );
        HoudiniEngineString.ToFString( HapiString );

        if ( HapiString.StartsWith( "unreal_uproperty_",  ESearchCase::IgnoreCase ) )
        {
            UPropertyAttribute CurrentUProperty;

            // Extract the name of the UProperty from the attribute name
            CurrentUProperty.PropertyName = HapiString.Right( HapiString.Len() - 17 );

            // Get the Attribute Info
            HAPI_AttributeInfo AttribInfo;
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(),
                NodeId, PartId, TCHAR_TO_UTF8( *HapiString ),
                AttributeOwner, &AttribInfo ), false );

            // Get the attribute type and tuple size
            CurrentUProperty.PropertyType = AttribInfo.storage;
            CurrentUProperty.PropertyCount = AttribInfo.count;
            CurrentUProperty.PropertyTupleSize = AttribInfo.tupleSize;	    

            if ( CurrentUProperty.PropertyType == HAPI_STORAGETYPE_FLOAT64 )
            {
                // Initialize the value array
                CurrentUProperty.DoubleValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                // Get the value(s)
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeFloat64Data(
                    FHoudiniEngine::Get().GetSession(),
                    NodeId, PartId, TCHAR_TO_UTF8( *HapiString ) ,&AttribInfo,
                    0, CurrentUProperty.DoubleValues.GetData(), 0, AttribInfo.count ), false );

                if ( HandleSplit )
                {
                    // For split primitives, we'll keep only one value from the proper split prim
                    TArray<double> SplitValues;
                    CurrentUProperty.GetDoubleTuple( SplitValues, PrimNumberForSplit );

                    CurrentUProperty.DoubleValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.DoubleValues.Add( SplitValues[ n ] );

                    CurrentUProperty.PropertyCount = 1;
                }
            }
            else if ( CurrentUProperty.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_FLOAT )
            {
                // Initialize the value array
                TArray< float > FloatValues;
                FloatValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                // Get the value(s)
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeFloatData(
                    FHoudiniEngine::Get().GetSession(),
                    NodeId, PartId, TCHAR_TO_UTF8( *HapiString ) ,&AttribInfo,
                    0, FloatValues.GetData(), 0, AttribInfo.count ), false );

                // Convert them to double
                CurrentUProperty.DoubleValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );
                for ( int32 n = 0; n < FloatValues.Num(); n++ )
                    CurrentUProperty.DoubleValues[ n ] = (double)FloatValues[ n ];

            }
            else if ( CurrentUProperty.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_INT64 )
            {
                // Initialize the value array
                CurrentUProperty.IntValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                // Get the value(s)
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInt64Data(
                    FHoudiniEngine::Get().GetSession(),
                    NodeId, PartId, TCHAR_TO_UTF8( *HapiString) ,&AttribInfo,
                    0, CurrentUProperty.IntValues.GetData(), 0, AttribInfo.count ), false );
            }
            else if ( CurrentUProperty.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_INT )
            {
                // Initialize the value array
                TArray< int32 > IntValues;
                IntValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                // Get the value(s)
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeIntData(
                    FHoudiniEngine::Get().GetSession(),
                    NodeId, PartId, TCHAR_TO_UTF8( *HapiString) ,&AttribInfo,
                    0, IntValues.GetData(), 0, AttribInfo.count ), false );

                // Convert them to int64
                CurrentUProperty.IntValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );
                for( int32 n = 0; n < IntValues.Num(); n++ )
                    CurrentUProperty.IntValues[ n ] = (int64)IntValues[ n ];

            }
            else if ( CurrentUProperty.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
            {
                // Initialize a string handle array
                TArray< HAPI_StringHandle > HapiSHArray;
                HapiSHArray.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                // Get the string handle(s)
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeStringData(
                    FHoudiniEngine::Get().GetSession(),
                    NodeId, PartId, TCHAR_TO_UTF8( *HapiString) ,&AttribInfo,
                    HapiSHArray.GetData(), 0, AttribInfo.count ), false );

                // Convert them to FString
                CurrentUProperty.StringValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                for( int32 IdxSH = 0; IdxSH < HapiSHArray.Num(); IdxSH++ )
                {
                    FString CurrentString;
                    FHoudiniEngineString HEngineString( HapiSHArray[ IdxSH ] );
                    HEngineString.ToFString( CurrentString );

                    CurrentUProperty.StringValues[ IdxSH ] = CurrentString;
                }
            }
            else
            {
                // Unsupported type, skipping!
                continue;
            }

            // Handling Split
            // For split primitives, we'll keep only one value from the proper split prim
            if ( HandleSplit )
            {
                if ( ( CurrentUProperty.PropertyType == HAPI_STORAGETYPE_FLOAT64 )
                    || ( CurrentUProperty.PropertyType == HAPI_STORAGETYPE_FLOAT ) )
                {
                    TArray< double > SplitValues;
                    CurrentUProperty.GetDoubleTuple( SplitValues, PrimNumberForSplit );

                    CurrentUProperty.DoubleValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.DoubleValues.Add( SplitValues[ n ] );
                }
                else if ( ( CurrentUProperty.PropertyType == HAPI_STORAGETYPE_INT64 )
                    || ( CurrentUProperty.PropertyType == HAPI_STORAGETYPE_INT ) )
                {
                    TArray< int64 > SplitValues;
                    CurrentUProperty.GetIntTuple( SplitValues, PrimNumberForSplit );

                    CurrentUProperty.IntValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.IntValues.Add( SplitValues[ n ] );
                }
                else if ( CurrentUProperty.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
                {
                    TArray< FString > SplitValues;
                    CurrentUProperty.GetStringTuple( SplitValues, PrimNumberForSplit );

                    CurrentUProperty.StringValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.StringValues.Add( SplitValues[ n ] );
                }

                CurrentUProperty.PropertyCount = 1;
            }

            // We can add the UPropertyAttribute to the array
            AllUProps.Add( CurrentUProperty );
            nUPropCount++;
        }
    }

    return nUPropCount;
}

void 
FHoudiniEngineUtils::UpdateUPropertyAttributes( UObject* MeshComponent, FHoudiniGeoPartObject GeoPartObject )
{
#if WITH_EDITOR
    if ( !MeshComponent )
        return;

    // MeshComponent should be either a StaticMeshComponent, an InstancedStaticMeshComponent or an InstancedActorComponent
    UStaticMeshComponent* SMC = Cast< UStaticMeshComponent >( MeshComponent );
    UInstancedStaticMeshComponent* ISMC = Cast< UInstancedStaticMeshComponent >( MeshComponent );
    UHoudiniInstancedActorComponent* IAC = Cast< UHoudiniInstancedActorComponent >( MeshComponent );

    if ( !SMC && !ISMC && !IAC )
        return;

    UClass* MeshClass = IAC ? IAC->StaticClass() : ISMC ? ISMC->StaticClass() : SMC->StaticClass();

    TArray< UPropertyAttribute > AllUProps;
    // Get the detail uprop attributes
    int nUPropsCount = FHoudiniEngineUtils::GetUPropertyAttributesList( GeoPartObject, AllUProps );
    // Then the primitive uprop attributes
    nUPropsCount += FHoudiniEngineUtils::GetUPropertyAttributesList( GeoPartObject, AllUProps, HAPI_ATTROWNER_PRIM );

    if ( nUPropsCount <= 0 )
        return;

    // Trying to find the UProps in the object 
    for ( int32 nAttributeIdx = 0; nAttributeIdx < nUPropsCount; nAttributeIdx++ )
    {
        UPropertyAttribute CurrentPropAttribute = AllUProps[ nAttributeIdx ];
        FString CurrentUPropertyName = CurrentPropAttribute.PropertyName;
        if ( CurrentUPropertyName.IsEmpty() )
            continue;

        // We have to iterate manually on the properties, in order to handle structProperties correctly
        void* StructContainer = nullptr;
        UProperty* FoundProperty = nullptr;
        bool bPropertyHasBeenFound = false;
        for ( TFieldIterator< UProperty > PropIt( MeshClass ); PropIt; ++PropIt )
        {
            UProperty* CurrentProperty = *PropIt;

            FString DisplayName = CurrentProperty->GetDisplayNameText().ToString().Replace( TEXT(" "), TEXT("") );
            FString Name = CurrentProperty->GetName();

            // If the property name contains the uprop attribute name, we have a candidate
            if ( Name.Contains( CurrentUPropertyName ) || DisplayName.Contains( CurrentUPropertyName ) )
            {
                FoundProperty = CurrentProperty;

                // If it's an equality, we dont need to keep searching
                if ( ( Name == CurrentUPropertyName ) || ( DisplayName == CurrentUPropertyName ) )
                {
                    bPropertyHasBeenFound = true;
                    break;
                }
            }

            // StructProperty need to be a nested struct
            if ( UStructProperty* StructProperty = Cast< UStructProperty >( CurrentProperty ) )
            {
                // Walk the structs' properties and try to find the one we're looking for
                UScriptStruct* Struct = StructProperty->Struct;
                for ( TFieldIterator< UProperty > It( Struct ); It; ++It )
                {
                    UProperty* Property = *It;

                    DisplayName = It->GetDisplayNameText().ToString().Replace( TEXT(" "), TEXT("") );		    
                    Name = It->GetName();

                    // If the property name contains the uprop attribute name, we have a candidate
                    if ( Name.Contains( CurrentUPropertyName ) || DisplayName.Contains( CurrentUPropertyName ) )
                    {
                        // We found the property in the struct property, we need to keep the ValuePtr in the object
                        // of the structProp in order to be able to access the property value afterwards...
                        FoundProperty = Property;
                        StructContainer = StructProperty->ContainerPtrToValuePtr< void >( MeshComponent, 0 );

                        // If it's an equality, we dont need to keep searching
                        if ( ( Name == CurrentUPropertyName ) || ( DisplayName == CurrentUPropertyName ) )
                        {
                            bPropertyHasBeenFound = true;
                            break;
                        }
                    }
                }
            }

            if ( bPropertyHasBeenFound )
                break;
        }

        if ( !FoundProperty )
        {
            // Property not found
            HOUDINI_LOG_MESSAGE( TEXT( "Could not find UProperty: %s"), *( CurrentPropAttribute.PropertyName ) );
            continue;
        }

        // The found property's class (TODO Remove me! for debug only)
        UClass* PropertyClass = FoundProperty->GetClass();

        UProperty* InnerProperty = FoundProperty;
        int32 NumberOfProperties = 1;

        if ( UArrayProperty* ArrayProperty = Cast< UArrayProperty >( FoundProperty ) )
        {
            InnerProperty = ArrayProperty->Inner;
            NumberOfProperties = ArrayProperty->ArrayDim;

            // Do we need to add values to the array?
            FScriptArrayHelper_InContainer ArrayHelper( ArrayProperty, CurrentPropAttribute.GetData() );
            if ( CurrentPropAttribute.PropertyTupleSize > NumberOfProperties )
            {
                ArrayHelper.Resize( CurrentPropAttribute.PropertyTupleSize );
                NumberOfProperties = CurrentPropAttribute.PropertyTupleSize;
            }
        }

        for( int32 nPropIdx = 0; nPropIdx < NumberOfProperties; nPropIdx++ )
        {
            if ( UFloatProperty* FloatProperty = Cast< UFloatProperty >( InnerProperty ) )
            {
                // FLOAT PROPERTY
                if ( CurrentPropAttribute.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
                {
                    FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
                    FloatProperty->SetNumericPropertyValueFromString(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< FString >( ( uint8 * )StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx ),
                        *Value );
                }
                else
                {
                    double Value = CurrentPropAttribute.GetDoubleValue( nPropIdx );
                    FloatProperty->SetFloatingPointPropertyValue(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< float >( ( uint8 * )StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< float >( MeshComponent, nPropIdx ),
                        Value );
                }
            }
            else if ( UIntProperty* IntProperty = Cast< UIntProperty >( InnerProperty ) )
            {
                // INT PROPERTY
                if ( CurrentPropAttribute.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
                {
                    FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
                    IntProperty->SetNumericPropertyValueFromString(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< FString >( ( uint8 * )StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx ),
                        *Value );
                }
                else
                {
                    int64 Value = CurrentPropAttribute.GetIntValue( nPropIdx );
                    IntProperty->SetIntPropertyValue(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< int64 >( ( uint8 * )StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< int64 >( MeshComponent, nPropIdx ),
                        Value );
                }
            }
            else if ( UBoolProperty* BoolProperty = Cast< UBoolProperty >( InnerProperty ) )
            {
                // BOOL PROPERTY
                bool Value = CurrentPropAttribute.GetBoolValue( nPropIdx );

                BoolProperty->SetPropertyValue(
                    StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< bool >( ( uint8* )StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< bool >( MeshComponent, nPropIdx ),
                    Value );
            }
            else if ( UStrProperty* StringProperty = Cast< UStrProperty >( InnerProperty ) )
            {
                // STRING PROPERTY
                FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );

                StringProperty->SetPropertyValue(
                    StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FString >( ( uint8* )StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx ),
                    Value );
            }
            else if ( UNumericProperty *NumericProperty = Cast< UNumericProperty >( InnerProperty ) )
            {
                // NUMERIC PROPERTY
                if ( CurrentPropAttribute.PropertyType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
                {
                    FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
                    NumericProperty->SetNumericPropertyValueFromString(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< FString >( ( uint8 * )StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx ),
                        *Value );
                }
                else if ( NumericProperty->IsInteger() )
                {
                    int64 Value = CurrentPropAttribute.GetIntValue( nPropIdx );

                    NumericProperty->SetIntPropertyValue(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< int64 >( (uint8*)StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< int64 >( MeshComponent, nPropIdx ),
                        (int64)Value );
                }
                else if ( NumericProperty->IsFloatingPoint() )
                {
                    double Value = CurrentPropAttribute.GetDoubleValue( nPropIdx );

                    NumericProperty->SetFloatingPointPropertyValue(
                        StructContainer ?
                        InnerProperty->ContainerPtrToValuePtr< float >( ( uint8 * )StructContainer, nPropIdx )
                        : InnerProperty->ContainerPtrToValuePtr< float >( MeshComponent, nPropIdx ),
                        Value );
                }
                else
                {
                    // Numeric Property was found, but is of an unsupported type
                    HOUDINI_LOG_MESSAGE( TEXT( "Unsupported Numeric UProperty" ) );
                }
            }
            else if ( UNameProperty* NameProperty = Cast< UNameProperty >( InnerProperty ) )
            {
                // NAME PROPERTY
                FString StringValue = CurrentPropAttribute.GetStringValue( nPropIdx );

                FName Value = FName( *StringValue );

                NameProperty->SetPropertyValue(
                    StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FName >( ( uint8* )StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FName >( MeshComponent, nPropIdx ),
                    Value );
            }
            else if ( UStructProperty* StructProperty = Cast< UStructProperty >( InnerProperty ) )
            {
                // STRUCT PROPERTY
                const FName PropertyName = StructProperty->Struct->GetFName();
                if ( PropertyName == NAME_Vector )
                {
                    FVector& PropertyValue = StructContainer ?
                        *( InnerProperty->ContainerPtrToValuePtr< FVector >( (uint8 *) StructContainer, nPropIdx ) )
                        : *( InnerProperty->ContainerPtrToValuePtr< FVector >( MeshComponent, nPropIdx ) );

                    // Found a vector property, fill it with the 3 tuple values
                    PropertyValue.X = CurrentPropAttribute.GetDoubleValue( nPropIdx + 0 );
                    PropertyValue.Y = CurrentPropAttribute.GetDoubleValue( nPropIdx + 1 );
                    PropertyValue.Z = CurrentPropAttribute.GetDoubleValue( nPropIdx + 2 );
                }
                else if ( PropertyName == NAME_Transform )
                {
                    FTransform& PropertyValue = StructContainer ?
                        *( InnerProperty->ContainerPtrToValuePtr< FTransform >( (uint8 *) StructContainer, nPropIdx ) )
                        : *( InnerProperty->ContainerPtrToValuePtr< FTransform >( MeshComponent, nPropIdx ) );

                    // Found a transform property fill it with the attribute tuple values
                    FVector Translation;
                    Translation.X = CurrentPropAttribute.GetDoubleValue( nPropIdx + 0 );
                    Translation.Y = CurrentPropAttribute.GetDoubleValue( nPropIdx + 1 );
                    Translation.Z = CurrentPropAttribute.GetDoubleValue( nPropIdx + 2 );

                    FQuat Rotation;
                    Rotation.W = CurrentPropAttribute.GetDoubleValue( nPropIdx + 3 );
                    Rotation.X = CurrentPropAttribute.GetDoubleValue( nPropIdx + 4 );
                    Rotation.Y = CurrentPropAttribute.GetDoubleValue( nPropIdx + 5 );
                    Rotation.Z = CurrentPropAttribute.GetDoubleValue( nPropIdx + 6 );

                    FVector Scale;
                    Scale.X = CurrentPropAttribute.GetDoubleValue( nPropIdx + 7 );
                    Scale.Y = CurrentPropAttribute.GetDoubleValue( nPropIdx + 8 );
                    Scale.Z = CurrentPropAttribute.GetDoubleValue( nPropIdx + 9 );

                    PropertyValue.SetTranslation( Translation );
                    PropertyValue.SetRotation( Rotation );
                    PropertyValue.SetScale3D( Scale );
                }
                else if ( PropertyName == NAME_Color )
                {
                    FColor& PropertyValue = StructContainer ?
                        *(InnerProperty->ContainerPtrToValuePtr< FColor >( (uint8 *)StructContainer, nPropIdx ) )
                        : *(InnerProperty->ContainerPtrToValuePtr< FColor >( MeshComponent, nPropIdx ) );

                    PropertyValue.R = (int8)CurrentPropAttribute.GetIntValue( nPropIdx );
                    PropertyValue.G = (int8)CurrentPropAttribute.GetIntValue( nPropIdx + 1 );
                    PropertyValue.B = (int8)CurrentPropAttribute.GetIntValue( nPropIdx + 2 );
                    if ( CurrentPropAttribute.PropertyTupleSize == 4 )
                        PropertyValue.A = (int8)CurrentPropAttribute.GetIntValue( nPropIdx + 3 );
                }
                else if ( PropertyName == NAME_LinearColor )
                {
                    FLinearColor& PropertyValue = StructContainer ?
                        *(InnerProperty->ContainerPtrToValuePtr< FLinearColor >( (uint8 *)StructContainer, nPropIdx ) )
                        : *(InnerProperty->ContainerPtrToValuePtr< FLinearColor >( MeshComponent, nPropIdx ) );

                    PropertyValue.R = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx );
                    PropertyValue.G = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 1 );
                    PropertyValue.B = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 2 );
                    if ( CurrentPropAttribute.PropertyTupleSize == 4 )
                        PropertyValue.A = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 3 );
                }
            }
            else
            {
                // Property was found, but is of an unsupported type
                HOUDINI_LOG_MESSAGE( TEXT("Unsupported UProperty Class: %s found for uproperty %s" ), *PropertyClass->GetName(), *CurrentPropAttribute.PropertyName );
            }

            // Some UProperties might need some additional tweaks...
            if ( CurrentUPropertyName == "CollisionProfileName" )
            {
                FString StringValue = CurrentPropAttribute.GetStringValue( nPropIdx );
                FName Value = FName( *StringValue );

                if ( SMC )
                    SMC->SetCollisionProfileName( Value );
                else if ( ISMC )
                    ISMC->SetCollisionProfileName( Value );
            }
        }
    }
#endif
}

FHoudiniCookParams::FHoudiniCookParams( class UHoudiniAsset* InHoudiniAsset )
: HoudiniAsset( InHoudiniAsset )
{
    PackageGUID = FGuid::NewGuid();
    TempCookFolder = LOCTEXT( "Temp", "/Game/HoudiniEngine/Temp" );
}

#undef LOCTEXT_NAMESPACE