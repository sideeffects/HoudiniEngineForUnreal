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
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineMaterialUtils.h"
#include "Components/SplineComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"

#include "CoreMinimal.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Engine/StaticMeshSocket.h"
#if WITH_EDITOR
    #include "Editor.h"
    #include "EditorFramework/AssetImportData.h"
    #include "Interfaces/ITargetPlatform.h"
    #include "Interfaces/ITargetPlatformManagerModule.h"
    #include "Editor/UnrealEd/Private/GeomFitUtils.h"
	#include "UnrealEd/Private/ConvexDecompTool.h"
    #include "PackedNormal.h"
    #include "Widgets/Notifications/SNotificationList.h"
    #include "Framework/Notifications/NotificationManager.h"
#endif

#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshTypes.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"

#if PLATFORM_WINDOWS
    #include "Windows/WindowsHWrapper.h"

    // Of course, Windows defines its own GetGeoInfo,
    // So we need to undefine that before including HoudiniApi.h to avoid collision...
    #ifdef GetGeoInfo
        #undef GetGeoInfo
    #endif
#endif

#include <string>

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Internationalization/Internationalization.h"

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
FHoudiniEngineUtils::IsHoudiniNodeValid( const HAPI_NodeId& NodeId )
{
    if ( NodeId < 0 )
        return false;

    HAPI_NodeInfo NodeInfo;
    bool ValidationAnswer = 0;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo ), false );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::IsNodeValid(
        FHoudiniEngine::Get().GetSession(), NodeId,
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
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId, HAPI_PartId PartId,
    HAPI_GroupType GroupType, TArray< FString > & GroupNames, const bool& isPackedPrim )
{
    int32 GroupCount = 0;
    if ( !isPackedPrim )
    {
        // Get group count on the geo
        HAPI_GeoInfo GeoInfo;
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo( FHoudiniEngine::Get().GetSession(), GeoId, &GeoInfo), false);
        GroupCount = FHoudiniEngineUtils::HapiGetGroupCountByType(GroupType, GeoInfo);
    }
    else
    {
        // We need the group count for this packed prim
        int32 PointGroupCount = 0, PrimGroupCount = 0;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupCountOnPackedInstancePart( FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PointGroupCount, &PrimGroupCount ), false );

        if ( GroupType == HAPI_GROUPTYPE_POINT )
            GroupCount = PointGroupCount;
        else
            GroupCount = PrimGroupCount;
    }

    if ( GroupCount <= 0 )
        return true;

    std::vector< int32 > GroupNameHandles( GroupCount, 0 );
    if ( !isPackedPrim )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupNames(
            FHoudiniEngine::Get().GetSession(),
            GeoId, GroupType, &GroupNameHandles[ 0 ], GroupCount ), false );
    }
    else
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupNamesOnPackedInstancePart(
            FHoudiniEngine::Get().GetSession(),
            GeoId, PartId, GroupType, &GroupNameHandles[ 0 ], GroupCount ), false );
    }

    for ( int32 NameIdx = 0; NameIdx < GroupCount; ++NameIdx )
    {
        FString GroupName = TEXT( "" );
        FHoudiniEngineString HoudiniEngineString( GroupNameHandles[ NameIdx ] );

        HoudiniEngineString.ToFString( GroupName );
        GroupNames.Add( GroupName );
    }

    return true;
}

bool
FHoudiniEngineUtils::HapiGetGroupMembership(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId, HAPI_PartId PartId,
    HAPI_GroupType GroupType, const FString & GroupName, TArray< int32 > & GroupMembership )
{
    HAPI_PartInfo PartInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PartInfo ), false );

    int32 ElementCount = FHoudiniEngineUtils::HapiGetElementCountByGroupType( GroupType, PartInfo );
    std::string ConvertedGroupName = TCHAR_TO_UTF8( *GroupName );
    if ( ElementCount < 1 )
        return false;

    GroupMembership.SetNumUninitialized( ElementCount );

    if ( !PartInfo.isInstanced )
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupMembership(
            FHoudiniEngine::Get().GetSession(), GeoId, PartId, GroupType,
            ConvertedGroupName.c_str(), NULL, &GroupMembership[ 0 ], 0, ElementCount ), false );
    }
    else
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGroupMembershipOnPackedInstancePart(
            FHoudiniEngine::Get().GetSession(), GeoId, PartId, GroupType,
            ConvertedGroupName.c_str(), NULL, &GroupMembership[ 0 ], 0, ElementCount ), false );
    }

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
    FMemory::Memzero< HAPI_AttributeInfo >( AttribInfo );

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
    TArray< float > & Data, int32 TupleSize, HAPI_AttributeOwner Owner )
{
    ResultAttributeInfo.exists = false;

    // Reset container size.
    Data.SetNumUninitialized( 0 );

    int32 OriginalTupleSize = TupleSize;
    HAPI_AttributeInfo AttributeInfo;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );

    if ( Owner == HAPI_ATTROWNER_INVALID )
    {
        for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
        {
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(), GeoId, PartId, Name,
                (HAPI_AttributeOwner) AttrIdx, &AttributeInfo ), false );

            if ( AttributeInfo.exists )
                break;
        }
    }
    else
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(), GeoId, PartId, Name,
            Owner, &AttributeInfo ), false );
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
    HAPI_AttributeInfo & ResultAttributeInfo, TArray< float > & Data, int32 TupleSize, HAPI_AttributeOwner Owner )
{
    return FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
        ResultAttributeInfo, Data, TupleSize, Owner );
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo,
    TArray< int32 > & Data, int32 TupleSize, HAPI_AttributeOwner Owner )
{
    ResultAttributeInfo.exists = false;

    // Reset container size.
    Data.SetNumUninitialized( 0 );

    int32 OriginalTupleSize = TupleSize;
    HAPI_AttributeInfo AttributeInfo;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );

    if ( Owner == HAPI_ATTROWNER_INVALID )
    {
        for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
        {
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(),
                GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo ), false );

            if ( AttributeInfo.exists )
                break;
        }
    }
    else
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(),
            GeoId, PartId, Name, Owner, &AttributeInfo ), false );
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
    TArray< int32 > & Data, int32 TupleSize, HAPI_AttributeOwner Owner )
{
    return FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
        ResultAttributeInfo, Data, TupleSize, Owner );
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
    HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo,
    TArray< FString > & Data, int32 TupleSize, HAPI_AttributeOwner Owner )
{
    ResultAttributeInfo.exists = false;

    // Reset container size.
    Data.Empty();

    int32 OriginalTupleSize = TupleSize;
    HAPI_AttributeInfo AttributeInfo;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );

    if ( Owner == HAPI_ATTROWNER_INVALID )
    {
        for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
        {
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(),
                GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo ), false );

            if ( AttributeInfo.exists )
                break;
        }
    }
    else
    {
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(),
            GeoId, PartId, Name, Owner, &AttributeInfo ), false );
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
    HAPI_AttributeInfo & ResultAttributeInfo, TArray< FString > & Data, int32 TupleSize, HAPI_AttributeOwner Owner )
{
    return FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        HoudiniGeoPartObject.AssetId,
        HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId,
        HoudiniGeoPartObject.PartId, Name,
        ResultAttributeInfo, Data, TupleSize, Owner );
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

HAPI_ParmId FHoudiniEngineUtils::HapiFindParameterByNameOrTag( const HAPI_NodeId& NodeId, const std::string ParmName, HAPI_ParmInfo& FoundParmInfo )
{
    FMemory::Memset< HAPI_ParmInfo >( FoundParmInfo, 0 );

    HAPI_NodeInfo NodeInfo;
    FHoudiniApi::GetNodeInfo( FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo );
    if ( NodeInfo.parmCount <= 0 )
        return -1;

    HAPI_ParmId ParmId = HapiFindParameterByNameOrTag( NodeInfo.id, ParmName );
    if ( ( ParmId < 0 ) || ( ParmId >= NodeInfo.parmCount ) )
        return -1;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
        FHoudiniEngine::Get().GetSession(),
        NodeId, ParmId, &FoundParmInfo ), -1 );

    return ParmId;
}

HAPI_ParmId FHoudiniEngineUtils::HapiFindParameterByNameOrTag( const HAPI_NodeId& NodeId, const std::string ParmName )
{
    // First, try to find the parameter by its name
    HAPI_ParmId ParmId = -1;
    HOUDINI_CHECK_ERROR_RETURN ( FHoudiniApi::GetParmIdFromName(
        FHoudiniEngine::Get().GetSession(),
        NodeId, ParmName.c_str(), &ParmId ), -1 );

    if ( ParmId >= 0 )
        return ParmId;

    // Second, try to find it by its tag
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmWithTag(
        FHoudiniEngine::Get().GetSession(),
        NodeId, ParmName.c_str(), &ParmId ), -1 );

    if ( ParmId >= 0 )
        return ParmId;

    return -1;
}


bool
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(
    HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue, float & OutValue )
{
    float Value = DefaultValue;
    bool bComputed = false;

    HAPI_ParmInfo FoundParamInfo;
    HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeId, ParmName, FoundParamInfo );
    if ( ParmId > -1 )
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
    HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeId, ParmName, FoundParamInfo );
    if ( ParmId > -1 )
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
    HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeId, ParmName, FoundParamInfo );
    if ( ParmId > -1 )
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
FHoudiniEngineUtils::HapiGetParameterNoSwapTag( const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, bool& NoSwapValue )
{
    // Default noswap to false
    NoSwapValue = false;

    // We're looking for the parameter noswap tag
    FString NoSwapTag = "hengine_noswap";

    // Does the parameter has the "hengine_noswap" tag?
    bool HasNoSwap = false;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ParmHasTag(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        TCHAR_TO_ANSI( *NoSwapTag ), &HasNoSwap ), false );

    if ( !HasNoSwap )
        return true;

    // Set NoSwap to true
    NoSwapValue = true;
    /*
    // Get the noswap tag string value
    HAPI_StringHandle StringHandle;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmTagValue(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        TCHAR_TO_ANSI( *NoSwapTag ), &StringHandle ), false );

    FString NoSwapString = TEXT("");
    FHoudiniEngineString HoudiniEngineString( StringHandle );
    if ( HoudiniEngineString.ToFString( NoSwapString ) )
    {
        // Make sure noswap is not set to 0 or false
        if ( !NoSwapString.Compare( TEXT("false"), ESearchCase::IgnoreCase )
            || !NoSwapString.Compare( TEXT("0"), ESearchCase::IgnoreCase ) )
            NoSwapValue = false;
    }
    */
    return true;
}

bool
FHoudiniEngineUtils::HapiGetParameterTag( const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, const FString& Tag, FString& TagValue )
{
    // Default
    TagValue = FString();

    // Does the parameter has the tag?
    bool HasTag = false;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ParmHasTag(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        TCHAR_TO_ANSI(*Tag), &HasTag ), false );

    HAPI_ParmId FoundId = -1;
    if (!HasTag)
    {
        FoundId = FHoudiniEngineUtils::HapiFindParameterByNameOrTag(NodeId, TCHAR_TO_ANSI(*Tag));
    }

    // Get the tag string value
    HAPI_StringHandle StringHandle;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmTagValue(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        TCHAR_TO_ANSI( *Tag ), &StringHandle ), false );

    FHoudiniEngineString HoudiniEngineString( StringHandle );
    if ( HoudiniEngineString.ToFString( TagValue ) )
    {
        return true;
    }

    return false;
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
    if ( ( NumberOfCVs < 2 ) || !FHoudiniEngineUtils::IsHoudiniNodeValid( HostAssetId ) )
        return false;

    // Check if connected asset id is valid, if it is not, we need to create an input asset.
    if (ConnectedAssetId < 0)
    {
        HAPI_NodeId NodeId = -1;
        // Create the curve SOP Node
        if (!HapiCreateCurveNode(NodeId))
            return false;

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( NodeId ) )
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
            FMemory::Memzero< HAPI_AttributeInfo >( attr_info );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(),
                ConnectedAssetId, 0,
                attr_name.c_str(), 
                ( HAPI_AttributeOwner )nOwner,
                &attr_info ), false );

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
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoRotation );
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

    if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( NodeId ) )
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
FHoudiniEngineUtils::HapiCreateInputNodeForLandscape(
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
    if ( !LandscapeProxy || !FHoudiniEngineUtils::IsHoudiniNodeValid( HostAssetId ) )
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

                FBoxSphereBounds WorldBounds = LandscapeComponent->CalcBounds( LandscapeComponent->GetComponentTransform());

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

        // Add the Heightfield's parent OBJ node to the created nodes
        OutCreatedNodeIds.AddUnique( FHoudiniEngineUtils::HapiGetParentNodeId( ConnectedAssetId ) );

        bool bSuccess = false;
        int32 NumComponents = LandscapeProxy->LandscapeComponents.Num();
        if ( !bExportOnlySelected || ( SelectedComponents.Num() == NumComponents ) )
        {
            // Export the whole landscape and its layer as a single heightfield
            bSuccess = FHoudiniLandscapeUtils::CreateHeightfieldFromLandscape( LandscapeProxy, MergeId );
        }
        else
        {
            // Each selected landscape component will be exported as a separate heightfield
            bSuccess = FHoudiniLandscapeUtils::CreateHeightfieldFromLandscapeComponentArray( LandscapeProxy, SelectedComponents, MergeId );
        }

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
        if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( InputNodeId ) )
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
FHoudiniEngineUtils::HapiCreateInputNodeForSpline(
    HAPI_NodeId HostAssetId, 
    USplineComponent * SplineComponent,
    HAPI_NodeId & ConnectedAssetId,
    FHoudiniAssetInputOutlinerMesh& OutlinerMesh,
    const float& SplineResolution )
{
#if WITH_EDITOR

    // If we don't have a spline component, or host asset is invalid, there's nothing to do.
    if ( !SplineComponent || !FHoudiniEngineUtils::IsHoudiniNodeValid( HostAssetId ) )
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
    OutlinerMesh.SplineControlPointsTransform.SetNum( nNumberOfControlPoints );
    for ( int32 n = 0; n < nNumberOfControlPoints; n++ )
    {
        // Getting the Local Transform for positions and scale
        OutlinerMesh.SplineControlPointsTransform[ n ] = SplineComponent->GetTransformAtSplinePoint( n, ESplineCoordinateSpace::Local, true );

        // ... but we used we used the world rotation
        OutlinerMesh.SplineControlPointsTransform[ n ].SetRotation( SplineComponent->GetQuaternionAtSplinePoint(n, ESplineCoordinateSpace::World ) );
    }

    // Cook the spline node.
    FHoudiniApi::CookNode( FHoudiniEngine::Get().GetSession(), ConnectedAssetId, nullptr );

#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateInputNodeForStaticMesh(
    UStaticMesh * StaticMesh,
    HAPI_NodeId & ConnectedAssetId,
    TArray< HAPI_NodeId >& OutCreatedNodeIds,
    UStaticMeshComponent* StaticMeshComponent /* = nullptr */,
    const bool& ExportAllLODs /* = false */,
    const bool& ExportSockets /* = false */)
{
#if WITH_EDITOR

    // If we don't have a static mesh there's nothing to do.
    if ( !StaticMesh )
        return false;

    // Export sockets if there are some
    bool DoExportSockets = ExportSockets && ( StaticMesh->Sockets.Num() > 0 );

    // Export LODs if there are some
    bool DoExportLODs = ExportAllLODs && ( StaticMesh->GetNumLODs() > 1 );

    // We need to use a merge node if we export lods OR sockets
    bool UseMergeNode = DoExportLODs || DoExportSockets;

    if ( UseMergeNode )
    {
        // Create a merge SOP asset. This will be our "ConnectedAssetId".
        // All the different LOD meshes will be plugged into it
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
            FHoudiniEngine::Get().GetSession(), -1,
            "SOP/merge", "input", true, &ConnectedAssetId ), false );
    }
    else if ( ConnectedAssetId < 0 )
    {
        // No LOD, we need a single input node
        // If connected asset id is invalid, we need to create an input node.
        HAPI_NodeId InputNodeId = -1;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputNode(
            FHoudiniEngine::Get().GetSession(), &InputNodeId, nullptr ), false );

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( InputNodeId ) )
            return false;

        // We now have a valid id.
        ConnectedAssetId = InputNodeId;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
            FHoudiniEngine::Get().GetSession(), InputNodeId, nullptr ), false );
    }

    // Get the input's parent OBJ node
    HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId( ConnectedAssetId );
    OutCreatedNodeIds.AddUnique( ParentId );

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

    int32 NumLODsToExport = DoExportLODs ? StaticMesh->GetNumLODs() : 1;
    for ( int32 LODIndex = 0; LODIndex < NumLODsToExport; LODIndex++ )
    {
        // Grab the LOD level.
        FStaticMeshSourceModel & SrcModel = StaticMesh->SourceModels[ LODIndex ];

        // If we're using a merge node, we need to create a new input null
        HAPI_NodeId CurrentLODNodeId = -1;
        if ( UseMergeNode )
        {
            // Create a new input node for the current LOD
            const char * LODName = "";
            {
                FString LOD = TEXT( "lod" ) + FString::FromInt( LODIndex );
                LODName = TCHAR_TO_UTF8( *LOD );
            }

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
                FHoudiniEngine::Get().GetSession(), ParentId, "null", LODName, false, &CurrentLODNodeId ), false );
        }
        else
        {
            // No merge node, just use the input node we created before
            CurrentLODNodeId = ConnectedAssetId;
        }

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

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPartInfo(
            FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0, &Part ), false );

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
            FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0,
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
            FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
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
                FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

                if ( MeshTexCoordIdx > 0 )
                    UVAttributeName += FString::Printf(TEXT("%d"), MeshTexCoordIdx + 1);

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
                    FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                    0, TCHAR_TO_ANSI(*UVAttributeName), &AttributeInfoVertex ), false );

                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                    FHoudiniEngine::Get().GetSession(),
                    CurrentLODNodeId, 0, TCHAR_TO_ANSI(*UVAttributeName), &AttributeInfoVertex,
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
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                CurrentLODNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex,
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
                    FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                    0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex ), false );

                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                    FHoudiniEngine::Get().GetSession(),
                    CurrentLODNodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex,
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
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                0, StaticMeshIndices.GetData(), 0, StaticMeshIndices.Num() ), false );

            // We need to generate array of face counts.
            TArray< int32 > StaticMeshFaceCounts;
            StaticMeshFaceCounts.Init( 3, Part.faceCount );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetFaceCounts(
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                0, StaticMeshFaceCounts.GetData(), 0, StaticMeshFaceCounts.Num() ), false );
        }

        // Marshall face material indices.
        if ( RawMesh.FaceMaterialIndices.Num() > 0 )
        {
            // Create an array of Material Interfaces
            TArray< UMaterialInterface * > MaterialInterfaces;
            MaterialInterfaces.SetNum(StaticMesh->StaticMaterials.Num());
            for( int32 MatIdx = 0; MatIdx < StaticMesh->StaticMaterials.Num(); MatIdx++ )
            {
                // The actual material index needs to be read from the section info map, as it can sometimes be different
                // Using MatIdx here could sometimes cause the materials to be incorrectly assigned on the faces.
                int32 SectionMatIdx = StaticMesh->SectionInfoMap.Get( LODIndex, MatIdx ).MaterialIndex;
                if ( !MaterialInterfaces.IsValidIndex(SectionMatIdx) )
                    SectionMatIdx = MatIdx;

                if ( StaticMeshComponent != nullptr )
                {
                    // Get the assigned material from the component instead of the Static Mesh
                    // As it could have been overriden
                    MaterialInterfaces[ SectionMatIdx ] = StaticMeshComponent->GetMaterial(MatIdx);
                }
                else
                {
                    // Get the material assigned to the SM
                    MaterialInterfaces[ SectionMatIdx ] = StaticMesh->StaticMaterials[ MatIdx ].MaterialInterface;
                }
            }

            // Create list of materials, one for each face.
            TArray< char * > StaticMeshFaceMaterials;
            FHoudiniEngineUtils::CreateFaceMaterialArray(
                MaterialInterfaces, RawMesh.FaceMaterialIndices, StaticMeshFaceMaterials );

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

            if ( FHoudiniApi::AddAttribute( FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0,
                MarshallingAttributeName.c_str(), &AttributeInfoMaterial ) != HAPI_RESULT_SUCCESS )
            {
                bAttributeError = true;
            }

            if ( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                CurrentLODNodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfoMaterial,
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
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                0, MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
                FHoudiniEngine::Get().GetSession(),
                CurrentLODNodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks,
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
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                0, MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
                FHoudiniEngine::Get().GetSession(),
                CurrentLODNodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution,
                (const int32 *) LightMapResolutions.GetData(), 0, LightMapResolutions.Num() ), false );
        }

        if ( !HoudiniRuntimeSettings->MarshallingAttributeInputMeshName.IsEmpty() )
        {
            // Create primitive attribute with mesh asset path
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

            HAPI_AttributeInfo AttributeInfo;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );
            AttributeInfo.count = Part.faceCount;
            AttributeInfo.tupleSize = 1;
            AttributeInfo.exists = true;
            AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                0, MarshallingAttributeName.c_str(), &AttributeInfo ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                CurrentLODNodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfo,
                PrimitiveAttrs.GetData(), 0, PrimitiveAttrs.Num() ), false );
        }

        if( !HoudiniRuntimeSettings->MarshallingAttributeInputSourceFile.IsEmpty() )
        {
            FString Filename;
            // Create primitive attribute with mesh asset path
            if( UAssetImportData* ImportData = StaticMesh->AssetImportData )
            {
                for( const auto& SourceFile : ImportData->SourceData.SourceFiles )
                {
                    Filename = UAssetImportData::ResolveImportFilename( SourceFile.RelativeFilename, ImportData->GetOutermost() );
                    break;
                }
            }

            if( !Filename.IsEmpty() )
            {
                std::string FilenameCStr = TCHAR_TO_ANSI( *Filename );
                const char* FilenameCStrRaw = FilenameCStr.c_str();
                TArray<const char*> PrimitiveAttrs;
                PrimitiveAttrs.AddUninitialized( Part.faceCount );
                for( int32 Ix = 0; Ix < Part.faceCount; ++Ix )
                {
                    PrimitiveAttrs[Ix] = FilenameCStrRaw;
                }

                std::string MarshallingAttributeName;
                FHoudiniEngineUtils::ConvertUnrealString(
                    HoudiniRuntimeSettings->MarshallingAttributeInputSourceFile,
                    MarshallingAttributeName );

                HAPI_AttributeInfo AttributeInfo;
                FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );
                AttributeInfo.count = Part.faceCount;
                AttributeInfo.tupleSize = 1;
                AttributeInfo.exists = true;
                AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
                AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
                AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                    FHoudiniEngine::Get().GetSession(), CurrentLODNodeId,
                    0, MarshallingAttributeName.c_str(), &AttributeInfo ), false );

                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                    FHoudiniEngine::Get().GetSession(),
                    CurrentLODNodeId, 0, MarshallingAttributeName.c_str(), &AttributeInfo,
                    PrimitiveAttrs.GetData(), 0, PrimitiveAttrs.Num() ), false );
            }
        }

        // Check if we have vertex attribute data to add
        if ( StaticMeshComponent && StaticMeshComponent->GetOwner() )
        {
            if ( UHoudiniAttributeDataComponent* DataComponent = StaticMeshComponent->GetOwner()->FindComponentByClass<UHoudiniAttributeDataComponent>() )
            {
                bool bSuccess = DataComponent->Upload( CurrentLODNodeId, StaticMeshComponent );
                if ( !bSuccess )
                {
                    HOUDINI_LOG_ERROR( TEXT( "Upload of attribute data for %s failed" ), *StaticMeshComponent->GetOwner()->GetName() );
                }
            }
        }

        if ( DoExportLODs )
        {
            // LOD Group
            const char * LODGroupStr = "";
            {
                FString LODGroup = TEXT("lod") + FString::FromInt(LODIndex);
                LODGroupStr = TCHAR_TO_UTF8(*LODGroup);
            }

            // Add a LOD group
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddGroup(
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0,
                HAPI_GROUPTYPE_PRIM, LODGroupStr), false);

            // Set GroupMembership
            TArray<int> GroupArray;
            GroupArray.Init(1, Part.faceCount);
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetGroupMembership(
                FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0,
                HAPI_GROUPTYPE_PRIM, LODGroupStr, GroupArray.GetData(), 0, Part.faceCount ), false );

            if ( !StaticMesh->bAutoComputeLODScreenSize )
            {
                // Add the lodX_screensize
                FString LODAttributeName = TEXT("lod") + FString::FromInt( LODIndex ) + TEXT("_screensize");

                // Create screensize detail attribute info.
                HAPI_AttributeInfo AttributeInfoLODScreenSize;
                FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoLODScreenSize );
                AttributeInfoLODScreenSize.count = 1;
                AttributeInfoLODScreenSize.tupleSize = 1;
                AttributeInfoLODScreenSize.exists = true;
                AttributeInfoLODScreenSize.owner = HAPI_ATTROWNER_DETAIL;
                AttributeInfoLODScreenSize.storage = HAPI_STORAGETYPE_FLOAT;
                AttributeInfoLODScreenSize.originalOwner = HAPI_ATTROWNER_INVALID;

                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                    FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0,
                    TCHAR_TO_UTF8( *LODAttributeName ), &AttributeInfoLODScreenSize), false);

                float lodscreensize = SrcModel.ScreenSize.Default;
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                    FHoudiniEngine::Get().GetSession(), CurrentLODNodeId, 0,
                    TCHAR_TO_UTF8( *LODAttributeName ), &AttributeInfoLODScreenSize,
                    &lodscreensize, 0, 1 ), false);
            }
        }

        // Commit the geo.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), CurrentLODNodeId), false );

        if ( UseMergeNode )
        {
            // Now we can connect the LOD node to the input node.
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, LODIndex,
                CurrentLODNodeId, 0), false);
        }
    }

    if ( DoExportSockets )
    {
        int32 NumSockets = StaticMesh->Sockets.Num();
        HAPI_NodeId SocketsNodeId = -1;
        // Create a new input node for the sockets
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
            FHoudiniEngine::Get().GetSession(), ParentId, "null", "sockets", false, &SocketsNodeId ), false );

        // Create part.
        HAPI_PartInfo Part;
        FMemory::Memzero< HAPI_PartInfo >( Part );
        Part.id = 0;
        Part.nameSH = 0;
        Part.attributeCounts[ HAPI_ATTROWNER_POINT ] = 0;
        Part.attributeCounts[ HAPI_ATTROWNER_PRIM ] = 0;
        Part.attributeCounts[ HAPI_ATTROWNER_VERTEX ] = 0;
        Part.attributeCounts[ HAPI_ATTROWNER_DETAIL ] = 0;
        Part.pointCount = NumSockets;
        Part.vertexCount = 0;
        Part.faceCount = 0;	
        Part.type = HAPI_PARTTYPE_MESH;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPartInfo(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0, &Part), false);

        // Create POS point attribute info.
        HAPI_AttributeInfo AttributeInfoPos;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPos );
        AttributeInfoPos.count = NumSockets;
        AttributeInfoPos.tupleSize = 3;
        AttributeInfoPos.exists = true;
        AttributeInfoPos.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoPos.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoPos.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPos ), false );

        // Create Rot point attribute Info
        HAPI_AttributeInfo AttributeInfoRot;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoRot );
        AttributeInfoRot.count = NumSockets;
        AttributeInfoRot.tupleSize = 4;
        AttributeInfoRot.exists = true;
        AttributeInfoRot.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoRot.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoRot.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_ROTATION, &AttributeInfoRot ), false );

        // Create scale point attribute Info
        HAPI_AttributeInfo AttributeInfoScale;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoScale );
        AttributeInfoScale.count = NumSockets;
        AttributeInfoScale.tupleSize = 3;
        AttributeInfoScale.exists = true;
        AttributeInfoScale.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_SCALE, &AttributeInfoScale ), false );

        //  Create the name attrib info
        HAPI_AttributeInfo AttributeInfoName;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoName );
        AttributeInfoName.count = NumSockets;
        AttributeInfoName.tupleSize = 1;
        AttributeInfoName.exists = true;
        AttributeInfoName.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoName.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfoName.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, &AttributeInfoName ), false );

        //  Create the tag attrib info
        HAPI_AttributeInfo AttributeInfoTag;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoTag );
        AttributeInfoTag.count = NumSockets;
        AttributeInfoTag.tupleSize = 1;
        AttributeInfoTag.exists = true;
        AttributeInfoTag.owner = HAPI_ATTROWNER_POINT;
        AttributeInfoTag.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfoTag.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG, &AttributeInfoTag ), false );

        // Extract the sockets transform values
        TArray< float > SocketPos;
        SocketPos.SetNumZeroed( NumSockets * 3 );
        TArray< float > SocketRot;
        SocketRot.SetNumZeroed( NumSockets * 4 );
        TArray< float > SocketScale;
        SocketScale.SetNumZeroed( NumSockets * 3 );
        TArray< const char * > SocketNames;
        TArray< const char * > SocketTags;

        for ( int32 Idx = 0; Idx < NumSockets; ++Idx )
        {
            UStaticMeshSocket* CurrentSocket = StaticMesh->Sockets[ Idx ];
            if ( !CurrentSocket )
                continue;

            // Get the socket's transform and convert it to HapiTransform
            FTransform SocketTransform( CurrentSocket->RelativeRotation, CurrentSocket->RelativeLocation, CurrentSocket->RelativeScale );
            HAPI_Transform HapiSocketTransform;
            TranslateUnrealTransform( SocketTransform, HapiSocketTransform );

            // Fill the attribute values
            SocketPos[ 3 * Idx + 0 ] = HapiSocketTransform.position[ 0 ];
            SocketPos[ 3 * Idx + 1 ] = HapiSocketTransform.position[ 1 ];
            SocketPos[ 3 * Idx + 2 ] = HapiSocketTransform.position[ 2 ];

            SocketRot[ 4 * Idx + 0 ] = HapiSocketTransform.rotationQuaternion[ 0 ];
            SocketRot[ 4 * Idx + 1 ] = HapiSocketTransform.rotationQuaternion[ 1 ];
            SocketRot[ 4 * Idx + 2 ] = HapiSocketTransform.rotationQuaternion[ 2 ];
            SocketRot[ 4 * Idx + 3 ] = HapiSocketTransform.rotationQuaternion[ 3 ];

            SocketScale[ 3 * Idx + 0 ] = HapiSocketTransform.scale[ 0 ];
            SocketScale[ 3 * Idx + 1 ] = HapiSocketTransform.scale[ 1 ];
            SocketScale[ 3 * Idx + 2 ] = HapiSocketTransform.scale[ 2 ];

            FString CurrentSocketName;
            if ( !CurrentSocket->SocketName.IsNone() )
                CurrentSocketName = CurrentSocket->SocketName.ToString();
            else
                CurrentSocketName = TEXT("Socket") + FString::FromInt( Idx );
            SocketNames.Add( FHoudiniEngineUtils::ExtractRawName( CurrentSocketName ) );

            if ( !CurrentSocket->Tag.IsEmpty() )
                SocketTags.Add( FHoudiniEngineUtils::ExtractRawName( CurrentSocket->Tag ) );
            else
                SocketTags.Add( "" );
        }

        //we can now upload them to our attribute.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_POSITION,
            &AttributeInfoPos,
            SocketPos.GetData(),
            0, AttributeInfoPos.count ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_ROTATION,
            &AttributeInfoRot,
            SocketRot.GetData(),
            0, AttributeInfoRot.count ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_SCALE,
            &AttributeInfoScale,
            SocketScale.GetData(),
            0, AttributeInfoScale.count ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME,
            &AttributeInfoName,
            SocketNames.GetData(),
            0, AttributeInfoName.count ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            SocketsNodeId, 0,
            HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG,
            &AttributeInfoTag,
            SocketTags.GetData(),
            0, AttributeInfoTag.count ), false );

        // We will also create the socket_details attributes
        for ( int32 Idx = 0; Idx < NumSockets; ++Idx )
        {
            // Build the current socket's prefix
            FString SocketAttrPrefix = TEXT( HAPI_UNREAL_ATTRIB_MESH_SOCKET_PREFIX ) + FString::FromInt( Idx );

            // Create mesh_socketX_pos attribute info.
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPos );
            AttributeInfoPos.count = 1;
            AttributeInfoPos.tupleSize = 3;
            AttributeInfoPos.exists = true;
            AttributeInfoPos.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoPos.storage = HAPI_STORAGETYPE_FLOAT;
            AttributeInfoPos.originalOwner = HAPI_ATTROWNER_INVALID;

            FString PosAttr = SocketAttrPrefix + TEXT("_pos");
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
                TCHAR_TO_ANSI( *PosAttr ), &AttributeInfoPos ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                SocketsNodeId, 0,
                TCHAR_TO_ANSI( *PosAttr ),
                &AttributeInfoPos,
                &(SocketPos[ 3 * Idx ]),
                0, AttributeInfoPos.count ), false );

            // Create mesh_socketX_rot point attribute Info
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoRot );
            AttributeInfoRot.count = 1;
            AttributeInfoRot.tupleSize = 4;
            AttributeInfoRot.exists = true;
            AttributeInfoRot.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoRot.storage = HAPI_STORAGETYPE_FLOAT;
            AttributeInfoRot.originalOwner = HAPI_ATTROWNER_INVALID;

            FString RotAttr = SocketAttrPrefix + TEXT("_rot");
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
                TCHAR_TO_ANSI( *RotAttr ), &AttributeInfoRot ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                SocketsNodeId, 0,
                TCHAR_TO_ANSI( *RotAttr ),
                &AttributeInfoRot,
                &(SocketRot[ 4 * Idx ]),
                0, AttributeInfoRot.count ), false );

            // Create mesh_socketX_scale point attribute Info
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoScale );
            AttributeInfoScale.count = 1;
            AttributeInfoScale.tupleSize = 3;
            AttributeInfoScale.exists = true;
            AttributeInfoScale.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
            AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

            FString ScaleAttr = SocketAttrPrefix + TEXT("_scale");
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
                TCHAR_TO_ANSI( *ScaleAttr ), &AttributeInfoScale ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                FHoudiniEngine::Get().GetSession(),
                SocketsNodeId, 0,
                TCHAR_TO_ANSI( *ScaleAttr ),
                &AttributeInfoScale,
                &(SocketScale[ 3 * Idx ]),
                0, AttributeInfoScale.count ), false );

            //  Create the mesh_socketX_name attrib info
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoName );
            AttributeInfoName.count = 1;
            AttributeInfoName.tupleSize = 1;
            AttributeInfoName.exists = true;
            AttributeInfoName.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoName.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoName.originalOwner = HAPI_ATTROWNER_INVALID;

            FString NameAttr = SocketAttrPrefix + TEXT("_name");
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
                TCHAR_TO_ANSI( *NameAttr ), &AttributeInfoName ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                SocketsNodeId, 0,
                TCHAR_TO_ANSI( *NameAttr ),
                &AttributeInfoName,
                &(SocketNames[ Idx ]),
                0, AttributeInfoName.count ), false );

            //  Create the mesh_socketX_tag attrib info
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoTag );
            AttributeInfoTag.count = 1;
            AttributeInfoTag.tupleSize = 1;
            AttributeInfoTag.exists = true;
            AttributeInfoTag.owner = HAPI_ATTROWNER_DETAIL;
            AttributeInfoTag.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoTag.originalOwner = HAPI_ATTROWNER_INVALID;

            FString TagAttr = SocketAttrPrefix + TEXT("_tag");
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
                TCHAR_TO_ANSI( *TagAttr ), &AttributeInfoTag ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                SocketsNodeId, 0,
                TCHAR_TO_ANSI( *TagAttr ),
                &AttributeInfoTag,
                & ( SocketTags[ Idx ] ),
                0, AttributeInfoTag.count ), false );
        }

        // Now add the sockets group
        const char * SocketGroupStr = "socket_imported";
        // Add a LOD group
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddGroup(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_GROUPTYPE_POINT, SocketGroupStr ), false );

        // Set GroupMembership
        TArray<int> GroupArray;
        GroupArray.Init( 1, NumSockets );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetGroupMembership(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId, 0,
            HAPI_GROUPTYPE_POINT, SocketGroupStr, GroupArray.GetData(), 0, NumSockets ), false );

        // Commit the geo.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), SocketsNodeId ), false);

        // Now we can connect the socket node to the merge node's last input.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, NumLODsToExport,
            SocketsNodeId, 0), false );
    }
#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateInputNodeForWorldOutliner(
    HAPI_NodeId HostAssetId,
    TArray< FHoudiniAssetInputOutlinerMesh > & OutlinerMeshArray,
    HAPI_NodeId & ConnectedAssetId,
    TArray< HAPI_NodeId >& OutCreatedNodeIds,
    const float& SplineResolution,
    const bool& ExportAllLODs /* = false */,
    const bool& ExportSockets /* = false */)
{
#if WITH_EDITOR
    if ( OutlinerMeshArray.Num() <= 0 )
        return false;

    bool UseMerge = OutlinerMeshArray.Num() > 0;

    if ( UseMerge )
    {
        // If we have more than one input, create the merge SOP asset.
        // This will be our "ConnectedAssetId".
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
            FHoudiniEngine::Get().GetSession(), -1,
            "SOP/merge", nullptr, true, &ConnectedAssetId ), false );

        // Add the merge node's parent OBJ to the created nodes
        OutCreatedNodeIds.AddUnique( FHoudiniEngineUtils::HapiGetParentNodeId( ConnectedAssetId ) );
    }

    for ( int32 InputIdx = 0; InputIdx < OutlinerMeshArray.Num(); ++InputIdx )
    {
        auto & OutlinerMesh = OutlinerMeshArray[ InputIdx ];

        bool bInputCreated = false;
        if ( OutlinerMesh.StaticMesh != nullptr )
        {
            // Creating an Input Node for Mesh Data
            bInputCreated = HapiCreateInputNodeForStaticMesh(
                OutlinerMesh.StaticMesh,
                OutlinerMesh.AssetId,
                OutCreatedNodeIds,
                OutlinerMesh.StaticMeshComponent,
                ExportAllLODs, ExportSockets );
        }
        else if ( OutlinerMesh.SplineComponent != nullptr )
        {
            // Creating an input node for spline data
            bInputCreated = HapiCreateInputNodeForSpline(
                ConnectedAssetId,
                OutlinerMesh.SplineComponent,
                OutlinerMesh.AssetId,
                OutlinerMesh,
                SplineResolution );

            OutCreatedNodeIds.AddUnique( FHoudiniEngineUtils::HapiGetParentNodeId( OutlinerMesh.AssetId ) );
        }

        if ( !bInputCreated )
        {
            OutlinerMesh.AssetId = -1;
            continue;
        }

        if ( UseMerge )
        {
            // Now we can connect the input node to the merge node.
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, InputIdx,
                OutlinerMesh.AssetId, 0), false );
        }
        else
        {
            // We only have one input, use its Id as our "ConnectedAssetId"
            ConnectedAssetId = OutlinerMesh.AssetId;
        }
        
        // Updating the Transform
        HAPI_TransformEuler HapiTransform;
        FMemory::Memzero< HAPI_TransformEuler >( HapiTransform );
        FHoudiniEngineUtils::TranslateUnrealTransform( OutlinerMesh.ComponentTransform, HapiTransform );

        // Set it on the input mesh's parent OBJ node
        HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId( OutlinerMesh.AssetId );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetObjectTransform(
            FHoudiniEngine::Get().GetSession(), ParentId, &HapiTransform ), false );
    }
#endif
    return true;
}

bool 
FHoudiniEngineUtils::HapiCreateInputNodeForObjects( 
    HAPI_NodeId HostAssetId, TArray<UObject *>& InputObjects, const TArray< FTransform >& InputTransforms,
    HAPI_NodeId & ConnectedAssetId, TArray< HAPI_NodeId >& OutCreatedNodeIds, 
    const bool& bExportSkeleton, const bool& bExportAllLODs /* = false */, const bool& bExportSockets /* = false */)
{
#if WITH_EDITOR
    if ( ensure( InputObjects.Num() ) )
    {
        bool UseMergeNode = InputObjects.Num() > 1;

        if ( UseMergeNode )
        {
            // If we have more thant one input mesh, create a merge SOP asset. This will be our "ConnectedAssetId".
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
                FHoudiniEngine::Get().GetSession(), -1,
                "SOP/merge", nullptr, true, &ConnectedAssetId ), false );
        }

        for ( int32 InputIdx = 0; InputIdx < InputObjects.Num(); ++InputIdx )
        {
            HAPI_NodeId MeshAssetNodeId = -1;
            FTransform InputTransform = FTransform::Identity;
            if ( InputTransforms.IsValidIndex( InputIdx ) )
                InputTransform = InputTransforms[ InputIdx ];

            if ( UStaticMesh* InputStaticMesh = Cast< UStaticMesh >( InputObjects[ InputIdx ] ) )
            {
                // Creating an Input Node for Static Mesh Data
                if ( !HapiCreateInputNodeForStaticMesh( InputStaticMesh, MeshAssetNodeId, OutCreatedNodeIds, nullptr, bExportAllLODs, bExportSockets ) )
                {
                    HOUDINI_LOG_WARNING( TEXT( "Error creating input index %d on %d" ), InputIdx, ConnectedAssetId );
                }
            }
            else if ( USkeletalMesh* InputSkeletalMesh = Cast< USkeletalMesh >( InputObjects[ InputIdx ] ) )
            {
                // Creating an Input Node for Skeletal Mesh Data
                if ( !HapiCreateInputNodeForSkeletalMesh( ConnectedAssetId, InputSkeletalMesh, MeshAssetNodeId, OutCreatedNodeIds, bExportSkeleton ) )
                {
                    HOUDINI_LOG_WARNING( TEXT( "Error creating input index %d on %d" ), InputIdx, ConnectedAssetId );
                }
            }

            if ( MeshAssetNodeId < 0 )
                continue;

            if ( UseMergeNode )
            {
                // Now we can connect the input node to the merge node.
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
                    FHoudiniEngine::Get().GetSession(), ConnectedAssetId, InputIdx,
                    MeshAssetNodeId, 0), false );
            }
            else
            {
                // We only have one input, use the MeshNodeId as our "ConnectedAssetId".
                ConnectedAssetId = MeshAssetNodeId;
            }

            // If the Geometry Input has a Transform offset
            if ( !InputTransform.Equals( FTransform::Identity ) )
            {
                // Updating the Transform
                HAPI_TransformEuler HapiTransform;
                FMemory::Memzero< HAPI_TransformEuler >( HapiTransform );
                FHoudiniEngineUtils::TranslateUnrealTransform( InputTransform, HapiTransform );

                HAPI_NodeInfo LocalAssetNodeInfo;
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
                    FHoudiniEngine::Get().GetSession(), MeshAssetNodeId, &LocalAssetNodeInfo ), false );

                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetObjectTransform(
                    FHoudiniEngine::Get().GetSession(), LocalAssetNodeInfo.parentId, &HapiTransform ), false );
            }
        }
    }
#endif
    return true;
}

bool
FHoudiniEngineUtils::HapiCreateInputNodeForSkeletalMesh(
    HAPI_NodeId HostAssetId, USkeletalMesh * SkeletalMesh,
    HAPI_NodeId & ConnectedAssetId, TArray< HAPI_NodeId >& OutCreatedNodeIds,
    const bool& bExportSkeleton )
{
#if WITH_EDITOR
    // If we don't have a skeletal mesh there's nothing to do.
    if ( !SkeletalMesh )
        return false;

    // Check if connected asset id is valid, if it is not, we need to create an input asset.
    if ( ConnectedAssetId < 0 )
    {
        HAPI_NodeId AssetId = -1;
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
            FHoudiniEngine::Get().GetSession(), &AssetId, nullptr ), false );

        // Check if we have a valid id for this new input asset.
        if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( AssetId ) )
            return false;

        // We now have a valid id.
        ConnectedAssetId = AssetId;

	// Get the input's parent OBJ node
	HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId( ConnectedAssetId );
	OutCreatedNodeIds.AddUnique( ParentId );

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
    const FSkeletalMeshModel* SkelMeshResource = SkeletalMesh->GetImportedModel();
    const FSkeletalMeshLODModel& SourceModel = SkelMeshResource->LODModels[0];
    const int32 VertexCount = SourceModel.GetNumNonClothingVertices();

    // Verify the integrity of the mesh.
    if ( VertexCount <= 0 ) 
        return false;

    // Extract the vertices buffer (this also contains normals, uvs, colors...)
    TArray<FSoftSkinVertex> SoftSkinVertices;
    SourceModel.GetNonClothVertices( SoftSkinVertices );
    if ( SoftSkinVertices.Num() != VertexCount )
        return false;

    // Extract the indices
    TArray<uint32> Indices = SourceModel.IndexBuffer;
    int32 FaceCount = Indices.Num() / 3;

    // Create part.
    HAPI_PartInfo Part;
    FMemory::Memzero< HAPI_PartInfo >(Part);
    Part.id = 0;
    Part.nameSH = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_POINT ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_PRIM ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_VERTEX ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_DETAIL ] = 0;
    Part.vertexCount = Indices.Num();
    Part.faceCount = FaceCount;
    Part.pointCount = SoftSkinVertices.Num();
    Part.type = HAPI_PARTTYPE_MESH;

    HAPI_GeoInfo DisplayGeoInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &DisplayGeoInfo ), false );

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0, &Part ), false );


    //-------------------------------------------------------------------------
    // POSITIONS
    //-------------------------------------------------------------------------
    
    // Create point attribute info.
    HAPI_AttributeInfo AttributeInfoPoint;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPoint );
    AttributeInfoPoint.count = SoftSkinVertices.Num();
    AttributeInfoPoint.tupleSize = 3;
    AttributeInfoPoint.exists = true;
    AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0,
        HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint ), false );

    // Convert vertices from the skeletal mesh to a float array
    TArray< float > SkelMeshVertices;
    SkelMeshVertices.SetNumZeroed( SoftSkinVertices.Num() * 3 );
    for ( int32 VertexIdx = 0; VertexIdx < SoftSkinVertices.Num(); ++VertexIdx )
    {
        // Grab vertex at this index.
        const FVector & PositionVector = SoftSkinVertices[ VertexIdx ].Position;

        if ( ImportAxis == HRSAI_Unreal )
        {
            SkelMeshVertices[ VertexIdx * 3 + 0 ] = PositionVector.X / GeneratedGeometryScaleFactor;
            SkelMeshVertices[ VertexIdx * 3 + 1 ] = PositionVector.Z / GeneratedGeometryScaleFactor;
            SkelMeshVertices[ VertexIdx * 3 + 2 ] = PositionVector.Y / GeneratedGeometryScaleFactor;
        }
        else
        {
            SkelMeshVertices[ VertexIdx * 3 + 0 ] = PositionVector.X / GeneratedGeometryScaleFactor;
            SkelMeshVertices[ VertexIdx * 3 + 1 ] = PositionVector.Y / GeneratedGeometryScaleFactor;
            SkelMeshVertices[ VertexIdx * 3 + 2 ] = PositionVector.Z / GeneratedGeometryScaleFactor;
        }
    }

    // Now that we have raw positions, we can upload them for our attribute.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
        0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint,
        SkelMeshVertices.GetData(), 0,
        AttributeInfoPoint.count ), false );


    //-------------------------------------------------------------------------
    // UVS
    //-------------------------------------------------------------------------
    // See if we have texture coordinates to upload.
    for ( uint32 MeshTexCoordIdx = 0; MeshTexCoordIdx < SourceModel.NumTexCoords; ++MeshTexCoordIdx )
    {
        TArray< FVector > MeshUVs;
        MeshUVs.SetNumUninitialized( Indices.Num() );

        // Transfer UV data.
        for ( int32 UVIdx = 0; UVIdx < Indices.Num(); ++UVIdx )
        {
            // Grab uv for this coordSet at this index.
            const FVector2D & UV = SoftSkinVertices[ Indices[UVIdx] ].UVs[ MeshTexCoordIdx ];
            MeshUVs[ UVIdx ] = FVector(UV.X, 1.0 - UV.Y, 0);
        }

        if ( ImportAxis == HRSAI_Unreal )
        {
            // We need to re-index UVs due to swapped indices in the faces (due to winding order differences) 
            for ( int32 WedgeIdx = 0; WedgeIdx < Indices.Num(); WedgeIdx += 3 )
            {
                // Swap second and third values for the vertices of the face
                MeshUVs.SwapMemory( WedgeIdx + 1, WedgeIdx + 2 );
            }
        }

        // Construct attribute name for this index.
        FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;
        if ( MeshTexCoordIdx > 0 )
            UVAttributeName += FString::Printf(TEXT("%d"), MeshTexCoordIdx + 1);

        // Create attribute for UVs
        HAPI_AttributeInfo AttributeInfoUVS;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoUVS );
        AttributeInfoUVS.count = MeshUVs.Num();
        AttributeInfoUVS.tupleSize = 3;
        AttributeInfoUVS.exists = true;
        AttributeInfoUVS.owner = HAPI_ATTROWNER_VERTEX;
        AttributeInfoUVS.storage = HAPI_STORAGETYPE_FLOAT;
        AttributeInfoUVS.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
            0, TCHAR_TO_ANSI( *UVAttributeName ), &AttributeInfoUVS), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(),
            DisplayGeoInfo.nodeId, 0, TCHAR_TO_ANSI( *UVAttributeName ), &AttributeInfoUVS,
            (const float *) MeshUVs.GetData(), 0, AttributeInfoUVS.count ), false );
    }
    

    //-------------------------------------------------------------------------
    // NORMALS
    //-------------------------------------------------------------------------
    TArray< FVector > MeshNormals;
    MeshNormals.SetNumUninitialized( Indices.Num() );

    // Transfer Normal data.
    for (int32 NormalIdx = 0; NormalIdx < Indices.Num(); ++NormalIdx)
    {
        FPackedNormal PackedNormal = SoftSkinVertices[Indices[NormalIdx]].TangentZ;
        MeshNormals[NormalIdx] = PackedNormal.ToFVector();

        // Doesnt work on MacOS ...
        //MeshNormals[ NormalIdx ] = FVector( SoftSkinVertices[ Indices[NormalIdx]  ].TangentZ.Vector. );
    }

    if ( ImportAxis == HRSAI_Unreal )
    {
        // We need to re-index normals due to swapped indices on the faces (due to winding differences).
        for ( int32 WedgeIdx = 0; WedgeIdx < Indices.Num(); WedgeIdx += 3 )
        {
            // Swap second and third values for the vertices of the face
            MeshNormals.SwapMemory( WedgeIdx + 1, WedgeIdx + 2 );
        }

        // Also swap the normal's Y and Z
        for ( int32 WedgeIdx = 0; WedgeIdx < Indices.Num(); ++WedgeIdx )
            Swap( MeshNormals[ WedgeIdx ].Y, MeshNormals[ WedgeIdx ].Z );
    }

    // Create attribute for normals.
    HAPI_AttributeInfo AttributeInfoNormals;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoNormals );
    AttributeInfoNormals.count = MeshNormals.Num();
    AttributeInfoNormals.tupleSize = 3;
    AttributeInfoNormals.exists = true;
    AttributeInfoNormals.owner = HAPI_ATTROWNER_VERTEX;
    AttributeInfoNormals.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoNormals.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
        0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoNormals ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(),
        DisplayGeoInfo.nodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoNormals,
        (const float *)MeshNormals.GetData(),
        0, AttributeInfoNormals.count ), false );


    //-------------------------------------------------------------------------
    // Vertex Colors
    //-------------------------------------------------------------------------
    TArray< FLinearColor > MeshColors;
    MeshColors.SetNumUninitialized( Indices.Num() );

    // Transfer Color data.
    for (int32 ColorIdx = 0; ColorIdx < Indices.Num(); ++ColorIdx)
    {
        MeshColors[ ColorIdx ] = SoftSkinVertices[ Indices[ColorIdx] ].Color.ReinterpretAsLinear();
    }
    
    if ( ImportAxis == HRSAI_Unreal )
    {
        // We need to re-index colors due to swapped indices on the faces (due to winding differences).
        for ( int32 WedgeIdx = 0; WedgeIdx < Indices.Num(); WedgeIdx += 3 )
        {
            // Swap second and third values for the vertices of the face
            MeshColors.SwapMemory(WedgeIdx + 1, WedgeIdx + 2);
        }
    }

    // Create attribute for colors.
    HAPI_AttributeInfo AttributeInfoColors;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoColors );
    AttributeInfoColors.count = MeshColors.Num();
    AttributeInfoColors.tupleSize = 4;
    AttributeInfoColors.exists = true;
    AttributeInfoColors.owner = HAPI_ATTROWNER_VERTEX;
    AttributeInfoColors.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoColors.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
        0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoColors), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(),
        DisplayGeoInfo.nodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoColors,
        (const float *)MeshColors.GetData(), 0, AttributeInfoColors.count ), false );

    //-------------------------------------------------------------------------
    // Indices
    //-------------------------------------------------------------------------

    // Extract indices from static mesh.
    TArray< int32 > MeshIndices;
    MeshIndices.SetNumUninitialized( Indices.Num() );

    if ( ImportAxis == HRSAI_Unreal )
    {
        // We have to swap indices to fix the winding order.
        for ( int32 IndexIdx = 0; IndexIdx < Indices.Num(); IndexIdx += 3 )
        {            
            MeshIndices[ IndexIdx + 0 ] = Indices[ IndexIdx + 0 ];
            MeshIndices[ IndexIdx + 1 ] = Indices[ IndexIdx + 2 ];
            MeshIndices[ IndexIdx + 2 ] = Indices[ IndexIdx + 1 ];
        }
    }
    else
    {
        // Direct copy, no need to change the winding order
        MeshIndices = TArray< int32 >( Indices );
    }

    // We can now set the vertex list.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetVertexList(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
        0, MeshIndices.GetData(), 0, MeshIndices.Num() ), false );

    // We need to generate the array of face counts.
    TArray< int32 > MeshFaceCounts;
    MeshFaceCounts.Init( 3, Part.faceCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetFaceCounts(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId,
        0, MeshFaceCounts.GetData(), 0, MeshFaceCounts.Num() ), false );

    //-------------------------------------------------------------------------
    // Face Materials
    //-------------------------------------------------------------------------
    TArray<int32> FaceMaterialIds;
    FaceMaterialIds.SetNumUninitialized( FaceCount );
    int32 FaceIdx = 0;
    int32 SectionCount = SourceModel.NumNonClothingSections();
    for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        const FSkelMeshSection& Section = SourceModel.Sections[SectionIndex];

        // Copy the material sections for all the faces of the current section
        int32 CurrentMatIndex = Section.MaterialIndex;
        for (int32 TriangleIndex = 0; TriangleIndex < (int32)Section.NumTriangles; ++TriangleIndex)
        {
            FaceMaterialIds[FaceIdx++] = CurrentMatIndex;
        }
    }

    // Create an array of Material Interfaces
    TArray< UMaterialInterface * > MaterialInterfaces;
    MaterialInterfaces.SetNum( SkeletalMesh->Materials.Num() );
    for( int32 MatIdx = 0; MatIdx < SkeletalMesh->Materials.Num(); MatIdx++ )
        MaterialInterfaces[MatIdx] = SkeletalMesh->Materials[ MatIdx ].MaterialInterface;

    // Create list of materials, one for each face.
    TArray< char * > MeshFaceMaterials;
    FHoudiniEngineUtils::CreateFaceMaterialArray(
        MaterialInterfaces, FaceMaterialIds, MeshFaceMaterials );

    // Get name of attribute used for marshalling materials.
    std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_MATERIAL;
    if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
        FHoudiniEngineUtils::ConvertUnrealString( HoudiniRuntimeSettings->MarshallingAttributeMaterial, MarshallingAttributeName );

    // Create attribute for materials.
    HAPI_AttributeInfo AttributeInfoMaterial;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoMaterial );
    AttributeInfoMaterial.count = FaceMaterialIds.Num();
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
        (const char **) MeshFaceMaterials.GetData(), 0,
        MeshFaceMaterials.Num() ) != HAPI_RESULT_SUCCESS )
    {
        bAttributeError = true;
    }

    // Delete material names.
    FHoudiniEngineUtils::DeleteFaceMaterialArray( MeshFaceMaterials );

    if ( bAttributeError )
    {
        check( 0 );
        return false;
    }

    //-------------------------------------------------------------------------
    // Mesh name
    //-------------------------------------------------------------------------
    if ( !HoudiniRuntimeSettings->MarshallingAttributeInputMeshName.IsEmpty() )
    {
        // Create primitive attribute with mesh asset path
        const FString MeshAssetPath = SkeletalMesh->GetPathName();
        std::string MeshAssetPathCStr = TCHAR_TO_ANSI( *MeshAssetPath );
        const char* MeshAssetPathRaw = MeshAssetPathCStr.c_str();

        TArray<const char*> PrimitiveAttrs;
        PrimitiveAttrs.AddUninitialized( Part.faceCount );
        for ( int32 Ix = 0; Ix < Part.faceCount; ++Ix )
        {
            PrimitiveAttrs[ Ix ] = MeshAssetPathRaw;
        }

        FHoudiniEngineUtils::ConvertUnrealString(
            HoudiniRuntimeSettings->MarshallingAttributeInputMeshName, MarshallingAttributeName );
        
        HAPI_AttributeInfo AttributeInfo;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );
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
   
    //-------------------------------------------------------------------------
    // Mesh asset path
    //-------------------------------------------------------------------------
    if( !HoudiniRuntimeSettings->MarshallingAttributeInputSourceFile.IsEmpty() )
    {
        // Create primitive attribute with mesh asset path
        FString Filename;
        if( UAssetImportData* ImportData = SkeletalMesh->AssetImportData )
        {
            for( const auto& SourceFile : ImportData->SourceData.SourceFiles )
            {
                Filename = UAssetImportData::ResolveImportFilename( SourceFile.RelativeFilename, ImportData->GetOutermost() );
                break;
            }
        }

        if( !Filename.IsEmpty() )
        {
            std::string FilenameCStr = TCHAR_TO_ANSI( *Filename );
            const char* FilenameCStrRaw = FilenameCStr.c_str();

            TArray<const char*> PrimitiveAttrs;
            PrimitiveAttrs.AddUninitialized( Part.faceCount );
            for( int32 Ix = 0; Ix < Part.faceCount; ++Ix )
            {
                PrimitiveAttrs[ Ix ] = FilenameCStrRaw;
            }

            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeInputSourceFile, MarshallingAttributeName );

            HAPI_AttributeInfo AttributeInfo;
            FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );
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
    }

    // Commit the geo before doing the skeleton.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId), false);

    if ( bExportSkeleton )
    {
        // Export the Skeleton!
        HAPI_NodeInfo NodeInfo;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, &NodeInfo), false );

        FHoudiniEngineUtils::HapiCreateSkeletonFromData( HostAssetId, SkeletalMesh, NodeInfo, OutCreatedNodeIds );

        // Commit the geo.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId ), false );
    }
#endif

    return true;
}

bool
FHoudiniEngineUtils::HapiCreateSkeletonFromData(
    HAPI_NodeId HostAssetId,
    USkeletalMesh * SkeletalMesh,
    const HAPI_NodeInfo& SkelMeshNodeInfo,
    TArray< HAPI_NodeId >& OutCreatedNodeIds )
{
#if WITH_EDITOR
    if ( !SkeletalMesh )
        return false;

    // We need to have an input asset already!
    if ( SkelMeshNodeInfo.parentId < 0 || SkelMeshNodeInfo.id < 0 )
        return false;

    // Get the skeleton from the skeletal mesh
    const FReferenceSkeleton& RefSkeleton = SkeletalMesh->RefSkeleton;
    int32 BoneCount = RefSkeleton.GetRawBoneNum();
    if ( BoneCount <= 0 )
        return false;

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

    // First, we need to create an objnet node in the input that will contains the nulls & bones
    HAPI_NodeId ObjnetNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.parentId, "objnet", "skeleton", true, &ObjnetNodeId ), false );

    // We also have to create an object merge node inside the objnet, to attach the skeletal mesh's geometry to the skeleton
    HAPI_NodeId GeoNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), ObjnetNodeId, "geo", "mesh", true, &GeoNodeId ), false );

    // For now, we dont export skinning info!!
    // TODO: new HAPI functions are needed to properly set the skinning weights on the vertices
    
    /*
    ///////// DPT: Deactivated skinning export for now

    // We're going to need a capture, capture override and deform node after the object merge,
    // So we'll create them now.

    // Create the bone deform
    HAPI_NodeId DeformNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, "bonedeform", "mesh_deform", true, &DeformNodeId), false);

    // We can delete the default file node created by the geometry node, 
    // This wll set the display flag top the deform node, which is exactly what we want
    HAPI_GeoInfo GeoInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, &GeoInfo), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::DeleteNode(
        FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId), false);

    // Create the capture attribute pack and unpack nodes
    HAPI_NodeId CaptureAttrPackNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, "captureattribpack", "mesh_capture_pack", true, &CaptureAttrPackNodeId ), false);

    HAPI_NodeId CaptureAttrUnpackNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, "captureattribunpack", "mesh_capture_unpack", true, &CaptureAttrUnpackNodeId ), false);

    // Create the capture node
    HAPI_NodeId CaptureNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, "capture", "mesh_capture", true, &CaptureNodeId), false);

    // Then create an object merge in the geo node...
    HAPI_NodeId ObjMergeNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, "object_merge", "mesh", true, &ObjMergeNodeId), false);

    // ... and set it's object path parameter to the skeletal mesh's geometry
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmNodeValue(
        FHoudiniEngine::Get().GetSession(), ObjMergeNodeId, "objpath1", SkelMeshNodeInfo.id), false);

    // We can now wire all those nodes together
    // Oject Merge > Capture > Capture Override > Capture Unpack > Capture Pack > Deform
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), CaptureNodeId, 0, ObjMergeNodeId, 0), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), CaptureAttrUnpackNodeId, 0, CaptureNodeId, 0 ), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), CaptureAttrPackNodeId, 0, CaptureAttrUnpackNodeId, 0 ), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), DeformNodeId, 0, CaptureAttrPackNodeId, 0 ), false);
        
        */

    // Create an object merge in the geo node...
    HAPI_NodeId ObjMergeNodeId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, "object_merge", "mesh", true, &ObjMergeNodeId ), false );

    // ... and set it's object path parameter to the skeletal mesh's geometry
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmNodeValue(
        FHoudiniEngine::Get().GetSession(), ObjMergeNodeId, "objpath1", SkelMeshNodeInfo.id ), false );

    // We can delete the default file node created by the geometry node, 
    // This will set the display flag to the object merge node
    HAPI_GeoInfo GeoInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, &GeoInfo ), false );

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::DeleteNode(
        FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId ), false );

    // Arrays keeping track of the created nulls / bone node IDs
    TArray<HAPI_NodeId> NullNodeIds;
    NullNodeIds.Init(-1,  BoneCount );
    TArray<HAPI_NodeId> BoneNodeIds;
    BoneNodeIds.Init(-1,  BoneCount );

    // String containing all the relative path to the joints cregion for the capture SOP
    int32 NumCapture = 0;
    FString Capture_Region_Paths;

    HAPI_NodeId RootNullNodeId = -1;
    for ( int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex )
    {
        // Get the current bone's info
        const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[ BoneIndex ];
        const FTransform& BoneTransform = RefSkeleton.GetRefBonePose()[ BoneIndex ];
        const FString & BoneName = CurrentBone.ExportName;

        // Create a joint (null node) for this bone
        HAPI_NodeId NullNodeId = -1;
        float BoneLength = 0.0f;
        {
            std::string NameStr;
            FHoudiniEngineUtils::ConvertUnrealString( BoneName, NameStr );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
                FHoudiniEngine::Get().GetSession(), ObjnetNodeId, "null", NameStr.c_str(), true, &NullNodeId ), false );

            // Check if we have a valid id for this new input asset.
            if ( NullNodeId == -1 )
                return false;

            // We have a valid node id.
            NullNodeIds[ BoneIndex ] = NullNodeId;

            // Convert the joint's transform
            HAPI_TransformEuler HapiTransform;
            FHoudiniEngineUtils::TranslateUnrealTransform( BoneTransform, HapiTransform );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetObjectTransform(
                FHoudiniEngine::Get().GetSession(), NullNodeId, &HapiTransform), false);

            // Calc the bone's length
            BoneLength = FVector( HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2] ).Size();

            // We also need to create a cregion node inside the null node
            HAPI_NodeId CRegionId = -1;
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
                FHoudiniEngine::Get().GetSession(), NullNodeId, "cregion", "cregion", true, &CRegionId ), false );

            /*
            ///////// DPT: Deactivated skinning export for now

            // Set the CRegion to XAxis??
            // Set the squatch and csquatch param to (0, 0, 0) for faster capture cook
            // (Since we'll be discarding the capture values)

            // Add a path to the cregion of the joint to the paths to be set for capture sop
            FString NodePathTemp;
            if ( !FHoudiniEngineUtils::HapiGetNodePath( CRegionId, CaptureNodeId, NodePathTemp ) )
                continue;

            Capture_Region_Paths += NodePathTemp + TEXT(" ");
            NumCapture++;
            */
        }

        // If we're the root node, we don't need to creating bones
        if ( CurrentBone.ParentIndex < 0 )
        {
            RootNullNodeId = NullNodeId;
            continue;
        }

        // Get our parent's id
        HAPI_NodeId ParentNullNodeId = -1;
        ParentNullNodeId = NullNodeIds[ CurrentBone.ParentIndex ];

        // Connect the joints
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), NullNodeId, 0, ParentNullNodeId, 0 ), false );

        // Now we need to create the bone
        // It has to be named by our parents, and looking at the current null node
        HAPI_NodeId BoneNodeId = -1;
        {
            const FString & ParentName = RefSkeleton.GetRefBoneInfo()[ CurrentBone.ParentIndex ].ExportName + TEXT("_bone");
            std::string NameStr;
            FHoudiniEngineUtils::ConvertUnrealString( ParentName, NameStr );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
                FHoudiniEngine::Get().GetSession(), ObjnetNodeId, "bone", NameStr.c_str(), true, &BoneNodeId), false );

            // Check if we have a valid id for this new input asset.
            if ( BoneNodeId == -1 )
                return false;

            // We have a valid node id.
            //NullNodeIds[ BoneIndex ] = BoneNodeId;

            // We need to change the length, lookatpath attributes
            // The bone looks at the current null
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmNodeValue(
                FHoudiniEngine::Get().GetSession(), BoneNodeId, "lookatpath", NullNodeId ), false );

            // Set the length parameter
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmFloatValue(
                FHoudiniEngine::Get().GetSession(), BoneNodeId, "length", 0, BoneLength), false);
        }

        // Connect the bone to its parent
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), BoneNodeId, 0, ParentNullNodeId, 0 ), false);
    }

    // Now that the skeleton has been created, we can connect the Geometry node containing the geometry to the skeleton's root
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), GeoNodeId, 0, RootNullNodeId, 0), false);

    /*
    ///////// DPT: Deactivated skinning export for now

    // We can also set the all the cregion path to the capture sop extraregions parameter
    // Get param id.
    HAPI_ParmId ParmId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
        FHoudiniEngine::Get().GetSession(), CaptureNodeId, "extraregions", &ParmId ), false );

    std::string ConvertedString = TCHAR_TO_UTF8(*Capture_Region_Paths);
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmStringValue( 
        FHoudiniEngine::Get().GetSession(), CaptureNodeId, ConvertedString.c_str(), ParmId, 0 ) , false );


    //-----------------------------------------------------------------------------------------------------------------
    // Extract Skinning info on the skeletal mesh
    // Grab base LOD level.
    const FSkeletalMeshResource* SkelMeshResource = SkeletalMesh->GetImportedResource();
    const FStaticLODModel& SourceModel = SkelMeshResource->LODModels[0];
    const int32 VertexCount = SourceModel.GetNumNonClothingVertices();

    // Extract the vertices buffer (this also contains normals, uvs, colors...)
    TArray<FSoftSkinVertex> SoftSkinVertices;
    SourceModel.GetNonClothVertices(SoftSkinVertices);
    if ( SoftSkinVertices.Num() != VertexCount )
        return false;

    // Array containing the weight values for each vertex, this will be converted to boneCapture_data attribute
    TArray<float> SkinningWeights;
    SkinningWeights.SetNum( VertexCount * MAX_TOTAL_INFLUENCES );

    // Array containing the index of the bones used for the weight values, this will be converted to boneCapture_index attribute 
    TArray<int32> SkinningIndexes;
    SkinningIndexes.SetNum( VertexCount * MAX_TOTAL_INFLUENCES );

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        // Add all the vertices that are weighted to the current skeletal bone to the cluster
        // NOTE: the bone influence indices contained in the vertex data are based on a per-chunk
        // list of verts.  The convert the chunk bone index to the mesh bone index, the chunk's boneMap is needed
        int32 VertIndex = 0;
        const int32 SectionCount = SourceModel.Sections.Num();
        for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            const FSkelMeshSection& Section = SourceModel.Sections[SectionIndex];

            for (int32 SoftIndex = 0; SoftIndex < Section.SoftVertices.Num(); ++SoftIndex)
            {
                const FSoftSkinVertex& Vert = Section.SoftVertices[SoftIndex];

                //SkinningIndexes[VertIndex].SetNum( MAX_TOTAL_INFLUENCES )

                for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
                {
                    SkinningIndexes[ VertIndex * MAX_TOTAL_INFLUENCES + InfluenceIndex ] = Section.BoneMap[ Vert.InfluenceBones[ InfluenceIndex ] ];
                    SkinningWeights[ VertIndex * MAX_TOTAL_INFLUENCES + InfluenceIndex ] = Vert.InfluenceWeights[ InfluenceIndex ] / 255.f;
                }

                ++VertIndex;
            }
        }
    }

    // We need to make sure the capture/capture unpack SOP are cooked before doing this
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode( 
        FHoudiniEngine::Get().GetSession(), CaptureNodeId, nullptr ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), CaptureAttrUnpackNodeId, nullptr), false);

    // Convert weight to the boneCapture_data attribute
    HAPI_AttributeInfo DataAttrInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
        FHoudiniEngine::Get().GetSession(), CaptureAttrUnpackNodeId, 0, "boneCapture_data", HAPI_ATTROWNER_POINT, &DataAttrInfo ), false );

    DataAttrInfo.tupleSize = MAX_TOTAL_INFLUENCES;

    // Get the old values
    TArray<float> Array;
    Array.SetNum(DataAttrInfo.count * DataAttrInfo.tupleSize);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(), CaptureAttrUnpackNodeId, 0, "boneCapture_data", &DataAttrInfo, 0, Array.GetData(), 0, DataAttrInfo.count), false);

    // Add the updated attribute
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.id,//CaptureAttrUnpackNodeId,
        0, "boneCapture_data", &DataAttrInfo ), false );

    // Set the new values
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.id,//CaptureAttrUnpackNodeId,
        0, "boneCapture_data", &DataAttrInfo, SkinningWeights.GetData(), 0, VertexCount ), false );

    // Convert indexes to the boneCapture_index attribute
    HAPI_AttributeInfo IndexAttrInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
        FHoudiniEngine::Get().GetSession(), CaptureAttrUnpackNodeId, 0, "boneCapture_index", HAPI_ATTROWNER_POINT, &IndexAttrInfo ), false);

    IndexAttrInfo.tupleSize = MAX_TOTAL_INFLUENCES;

    // Add the updated attribute to the mesh node
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.id,//CaptureAttrUnpackNodeId,
        0, "boneCapture_index", &IndexAttrInfo), false );

    // Set the new values
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.id,//CaptureAttrUnpackNodeId,
        0, "boneCapture_index", &IndexAttrInfo, SkinningIndexes.GetData(), 0, VertexCount ), false);
        */

    // Finally, we'll add a detail attribute with the path to the skeleton root node
    // This is an OBJ asset, return the path to this geo relative to the asset
    FString RootNodePath;
    if ( FHoudiniEngineUtils::HapiGetNodePath( RootNullNodeId, HostAssetId, RootNodePath ) )
    {
        const char * NodePathStr = TCHAR_TO_UTF8(*RootNodePath);

        HAPI_AttributeInfo AttributeInfo;
        FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfo );
        AttributeInfo.count = 1;
        AttributeInfo.tupleSize = 1;
        AttributeInfo.exists = true;
        AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
        AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.id,
            0, "unreal_skeleton_root_node", &AttributeInfo), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
            FHoudiniEngine::Get().GetSession(), SkelMeshNodeInfo.id,
            0, "unreal_skeleton_root_node", &AttributeInfo, (const char**)&NodePathStr, 0, 1 ), false );
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

bool FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
    HAPI_NodeId AssetId,
    FHoudiniCookParams& HoudiniCookParams,
    bool ForceRebuildStaticMesh, bool ForceRecookAll,
    const TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesIn,
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesOut,
    FTransform & ComponentTransform )
{
#if WITH_EDITOR
    /*
    // When called via commandlet, this might be perfectly valid
    if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) || !HoudiniCookParams.HoudiniAsset )
        return false;
    */

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
    std::string MarshallingAttributeNameMaterialInstance = HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE;
    std::string MarshallingAttributeNameFaceSmoothingMask = HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK;

    // Group name prefix used for collision geometry generation.
    FString CollisionGroupNamePrefix = TEXT("collision_geo");
    FString RenderedCollisionGroupNamePrefix = TEXT("rendered_collision_geo");
    FString UCXCollisionGroupNamePrefix = TEXT("collision_geo_ucx");
    FString UCXRenderedCollisionGroupNamePrefix = TEXT("rendered_collision_geo_ucx");
    FString SimpleCollisionGroupNamePrefix = TEXT("collision_geo_simple");
    FString SimpleRenderedCollisionGroupNamePrefix = TEXT("rendered_collision_geo_simple");

    // Add runtime setting for those?
    FString LodGroupNamePrefix = TEXT("lod");
    FString SocketGroupNamePrefix = TEXT("socket");

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;

        if ( !HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution, MarshallingAttributeNameLightmapResolution );

        if ( !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterial, MarshallingAttributeNameMaterial );

        if ( !HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask, MarshallingAttributeNameFaceSmoothingMask );

        if ( !HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() )
            CollisionGroupNamePrefix = HoudiniRuntimeSettings->CollisionGroupNamePrefix;

        if ( !HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() )
            RenderedCollisionGroupNamePrefix = HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix;

        if ( !HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix.IsEmpty() )
            UCXCollisionGroupNamePrefix = HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix;

        if ( !HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix.IsEmpty() )
            UCXRenderedCollisionGroupNamePrefix = HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix;

        if ( !HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix.IsEmpty() )
            SimpleCollisionGroupNamePrefix = HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix;

        if ( !HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix.IsEmpty() )
            SimpleRenderedCollisionGroupNamePrefix = HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix;
    }

    // Get platform manager LOD specific information.
    ITargetPlatform * CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    check( CurrentPlatform );
    const FStaticMeshLODGroup & LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup( NAME_None );
    int32 DefaultNumLODs = LODGroup.GetDefaultNumLODs();

    // Get the AssetInfo
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
    // Retrieve all used unique material ids.
    TSet< HAPI_NodeId > UniqueMaterialIds;
    TSet< HAPI_NodeId > UniqueInstancerMaterialIds;
    TMap< FHoudiniGeoPartObject, HAPI_NodeId > InstancerMaterialMap;
    FHoudiniEngineUtils::ExtractUniqueMaterialIds(
        AssetInfo, UniqueMaterialIds, UniqueInstancerMaterialIds, InstancerMaterialMap );

    // Create All the materials found on the asset
    TMap< FString, UMaterialInterface * > Materials;
    FHoudiniEngineMaterialUtils::HapiCreateMaterials(
        AssetId, HoudiniCookParams, AssetInfo, UniqueMaterialIds,
        UniqueInstancerMaterialIds, Materials, ForceRecookAll );

    // Update all material assignments
    HoudiniCookParams.HoudiniCookManager->ClearAssignmentMaterials();
    for( const auto& AssPair : Materials )
    {
        HoudiniCookParams.HoudiniCookManager->AddAssignmentMaterial( AssPair.Key, AssPair.Value );
    }

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

        // Handle Editable nodes
        // First, get the number of editable nodes
        int32 EditableNodeCount = 0;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ComposeChildNodeList(
            FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId,
            HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE, true, &EditableNodeCount ), false );

        if ( EditableNodeCount > 0 )
        {
            TArray< HAPI_NodeId > EditableNodeIds;
            EditableNodeIds.SetNumUninitialized( EditableNodeCount );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetComposedChildNodeList(
                FHoudiniEngine::Get().GetSession(), AssetId,
                EditableNodeIds.GetData(), EditableNodeCount ), false );

            for ( int nEditable = 0; nEditable < EditableNodeCount; nEditable++ )
            {
                HAPI_GeoInfo CurrentEditableGeoInfo;
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGeoInfo(
                    FHoudiniEngine::Get().GetSession(), EditableNodeIds[ nEditable ], &CurrentEditableGeoInfo ), false );

                // do not process the main display geo
                if ( CurrentEditableGeoInfo.isDisplayGeo )
                    continue;

                // We only handle editable curves
                if (CurrentEditableGeoInfo.type != HAPI_GEOTYPE_CURVE )
                    continue;

                // Add the editable curve to the output array
                FHoudiniGeoPartObject HoudiniGeoPartObject(
                    TransformMatrix, ObjectName, ObjectName, AssetId,
                    ObjectInfo.nodeId, CurrentEditableGeoInfo.nodeId, 0 );

                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsCurve = true;
                HoudiniGeoPartObject.bIsEditable = CurrentEditableGeoInfo.isEditable;
                HoudiniGeoPartObject.bHasGeoChanged = CurrentEditableGeoInfo.hasGeoChanged;

                StaticMeshesOut.Add( HoudiniGeoPartObject, nullptr );
            }
        }

        // Get the Display Geo's info
        HAPI_GeoInfo GeoInfo;
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetDisplayGeoInfo(
            FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId, &GeoInfo ) )
        {
            HOUDINI_LOG_MESSAGE( 
                TEXT("Creating Static Meshes: Object [%d %s] unable to retrieve GeoInfo, - skipping."),
                ObjectInfo.nodeId, *ObjectName );
            continue;
        }

        // Get object / geo group memberships for primitives.
        TArray< FString > ObjectGeoGroupNames;
        if( ! FHoudiniEngineUtils::HapiGetGroupNames(
            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, 0, HAPI_GROUPTYPE_PRIM, ObjectGeoGroupNames, false ) )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Creating Static Meshes: Object [%d %s] non-fatal error reading group names" ), 
                ObjectInfo.nodeId, *ObjectName );
        }

        // Prepare the object that will store UCX/UBX/USP Collision geo
        FKAggregateGeom AggregateCollisionGeo;
        bool bHasAggregateGeometryCollision = false;

        // Prepare the object that will store the mesh sockets and their names
        TArray< FTransform > AllSockets;
        TArray< FString > AllSocketsNames;
        TArray< FString > AllSocketsActors;
        TArray< FString > AllSocketsTags;

        for ( int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx )
        {
            // Get part information.
            HAPI_PartInfo PartInfo;
            if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(
                FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartIdx, &PartInfo ) )
            {
                // Error retrieving part info.
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d] unable to retrieve PartInfo - skipping." ),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx );
                continue;
            }

            // Retrieve part name.
            FString PartName = TEXT("");
            FHoudiniEngineString HoudiniEngineStringPartName( PartInfo.nameSH );
            HoudiniEngineStringPartName.ToFString( PartName );

            // Unsupported/Invalid part
            if( PartInfo.type == HAPI_PARTTYPE_INVALID )
                continue;

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

            // See if a custom name for the mesh was assigned via the GeneratedMeshName attribute        
            TArray< FString > GeneratedMeshNames;
            {
                HAPI_AttributeInfo AttribGeneratedMeshName;
                FMemory::Memzero< HAPI_AttributeInfo >( AttribGeneratedMeshName );
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

                if ( GeneratedMeshNames.Num() > 0 )
                {
                    const FString & CustomPartName = GeneratedMeshNames[ 0 ];
                    if ( !CustomPartName.IsEmpty() )
                        HoudiniGeoPartObject.SetCustomName( CustomPartName );
                }
            }

            // See if a custom bake folder override for the mesh was assigned via the "unreal_bake_folder" attribute
            TArray< FString > BakeFolderOverrides;
            {
                HAPI_AttributeInfo AttribBakeFolderOverride;
                FMemory::Memzero< HAPI_AttributeInfo >( AttribBakeFolderOverride );

                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                    AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                    HAPI_UNREAL_ATTRIB_BAKE_FOLDER, AttribBakeFolderOverride, BakeFolderOverrides );

                if ( BakeFolderOverrides.Num() > 0 )
                {
                    const FString & BakeFolderOverride = BakeFolderOverrides[ 0 ];
                    if ( !BakeFolderOverride.IsEmpty() )
                        HoudiniCookParams.BakeFolder = FText::FromString( BakeFolderOverride );
                }
            }

            // Extracting Sockets points on the current part and add them to the list
            AddMeshSocketToList( AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, AllSockets, AllSocketsNames, AllSocketsActors, AllSocketsTags, PartInfo.isInstanced );

            if ( PartInfo.type == HAPI_PARTTYPE_INSTANCER )
            {
                // This is a Packed Primitive instancer
                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !ObjectInfo.isInstanced;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = true;

                StaticMeshesOut.Add( HoudiniGeoPartObject, nullptr );
                continue;
            }
            else if ( PartInfo.type == HAPI_PARTTYPE_VOLUME )
            {
                // Volume Data, this is probably a Heightfield
                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !ObjectInfo.isInstanced;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = false;
                HoudiniGeoPartObject.bIsVolume = true;

                // We need to set the GeoChanged flag to true if we want to force the landscape reimport
                HoudiniGeoPartObject.bHasGeoChanged = ( GeoInfo.hasGeoChanged || ForceRebuildStaticMesh || ForceRecookAll );

                StaticMeshesOut.Add(HoudiniGeoPartObject, nullptr);

                continue;
            }
            else if ( PartInfo.type == HAPI_PARTTYPE_CURVE )
            {
                // This is a curve part.
                StaticMeshesOut.Add( HoudiniGeoPartObject, nullptr );
                continue;
            }
            else if ( !ObjectInfo.isInstancer && PartInfo.vertexCount <= 0 )
            {
                // This is not an instancer, but we do not have vertices, so maybe this is a point cloud with attribute override instancing
                // If it is, add it to the out list, if not skip it
                if ( HoudiniGeoPartObject.IsAttributeInstancer() || HoudiniGeoPartObject.IsAttributeOverrideInstancer() )
                {
                    HoudiniGeoPartObject.bIsInstancer = true;
                    StaticMeshesOut.Add( HoudiniGeoPartObject, nullptr );
                }
                continue;
            }

            // There are no vertices AND no points.
            if ( PartInfo.vertexCount <= 0 && PartInfo.pointCount <= 0 )
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] no points or vertices found - skipping." ),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName );
                continue;
            }

            // This is an instancer with no points.
            if ( ObjectInfo.isInstancer && PartInfo.pointCount <= 0 )
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] is instancer but has 0 points - skipping." ),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName );
                continue;
            }

            // Retrieve material information for this geo part.
            TArray< HAPI_NodeId > PartFaceMaterialIds;
            HAPI_Bool bSingleFaceMaterial = false;
            bool bPartHasMaterials = false;
            bool bMaterialsChanged = false;

            if ( PartInfo.faceCount > 0 )
            {
                PartFaceMaterialIds.SetNumUninitialized( PartInfo.faceCount );

                if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialNodeIdsOnFaces(
                    FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id,
                    &bSingleFaceMaterial, &PartFaceMaterialIds[ 0 ], 0, PartInfo.faceCount ) )
                {
                    // Error retrieving material face assignments.
                    HOUDINI_LOG_MESSAGE(
                        TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments, " )
                        TEXT( "- skipping." ),
                        ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName );
                    continue;
                }

                // Set flag if we have materials.
                TArray< HAPI_NodeId > PartUniqueMaterialIds;
                for ( int32 MaterialIdx = 0; MaterialIdx < PartFaceMaterialIds.Num(); ++MaterialIdx )
                    PartUniqueMaterialIds.AddUnique( PartFaceMaterialIds[ MaterialIdx ] );

                PartUniqueMaterialIds.RemoveSingle( -1 );
                bPartHasMaterials = PartUniqueMaterialIds.Num() > 0;

                // Set flag if any of the materials have changed.
                if ( bPartHasMaterials )
                {
                    for ( int32 MaterialIdx = 0; MaterialIdx < PartUniqueMaterialIds.Num(); ++MaterialIdx )
                    {
                        HAPI_MaterialInfo MaterialInfo;
                        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialInfo(
                            FHoudiniEngine::Get().GetSession(), PartUniqueMaterialIds[ MaterialIdx ], &MaterialInfo ) )
                            continue;

                        if ( MaterialInfo.hasChanged )
                        {
                            bMaterialsChanged = true;
                            break;
                        }
                    }
                }
            }

            // We do not create mesh for instancers.
            if ( ObjectInfo.isInstancer && PartInfo.pointCount > 0 )
            {
                // We need to check whether this instancer has a material.
                HAPI_NodeId const * FoundInstancerMaterialId = InstancerMaterialMap.Find( HoudiniGeoPartObject );
                if ( FoundInstancerMaterialId )
                {
                    HAPI_NodeId InstancerMaterialId = *FoundInstancerMaterialId;

                    FString InstancerMaterialShopName = TEXT( "" );
                    if ( InstancerMaterialId > -1 && FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName( AssetId, InstancerMaterialId, InstancerMaterialShopName ) )
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
                StaticMeshesOut.Add( HoudiniGeoPartObject, nullptr );
                continue;
            }

            // Containers used for raw data extraction.

            // Vertex Positions
            TArray< float > PartPositions;
            HAPI_AttributeInfo AttribInfoPositions;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfoPositions );

            // Vertex Normals
            TArray< float > PartNormals;
            HAPI_AttributeInfo AttribInfoNormals;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfoNormals );

            // Vertex Colors
            TArray< float > PartColors;
            HAPI_AttributeInfo AttribInfoColors;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfoColors );

            // Vertex Alpha values
            TArray< float > PartAlphas;
            HAPI_AttributeInfo AttribInfoAlpha;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfoAlpha );

            // UVs
            TArray< TArray< float > > PartUVs;
            PartUVs.SetNumZeroed( MAX_STATIC_TEXCOORDS );
            TArray< HAPI_AttributeInfo > AttribInfoUVs;
            AttribInfoUVs.SetNumZeroed( MAX_STATIC_TEXCOORDS );

            // Material Overrides per face
            TArray< FString > PartFaceMaterialAttributeOverrides;
            HAPI_AttributeInfo AttribFaceMaterials;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribFaceMaterials );

            // Face Smoothing masks
            TArray< int32 > PartFaceSmoothingMasks;
            HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfoFaceSmoothingMasks );

            // Lightmap resolution
            TArray< int32 > PartLightMapResolutions;
            HAPI_AttributeInfo AttribLightmapResolution;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribLightmapResolution );

            // Vertex Indices
            TArray< int32 > PartVertexList;
            PartVertexList.SetNumUninitialized( PartInfo.vertexCount );

            if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetVertexList(
                FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id,
                &PartVertexList[ 0 ], 0, PartInfo.vertexCount ) )
            {
                // Error getting the vertex list.
                HOUDINI_LOG_MESSAGE(
                    TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list - skipping." ),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName );

                continue;
            }

            // Array Storing the GroupNames for the current part
            TArray< FString > GroupNames;
            if ( !PartInfo.isInstanced )
            {
                GroupNames = ObjectGeoGroupNames;
            }
            else
            {
                // We're a packed primitive, so we want to get the group on the actual
                // packed geo, not on the main display geo
                if (!FHoudiniEngineUtils::HapiGetGroupNames(
                    AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, HAPI_GROUPTYPE_PRIM, GroupNames, true))
                    GroupNames = ObjectGeoGroupNames;
            }

            // Sort the Group name array so the LODs are ordered
            GroupNames.Sort();

            // See if we require splitting.
            TArray<FString> SplitGroupNames;
            int32 nLODInsertPos = 0;
            int32 NumberOfLODs = 0;
            for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx )
            {
                const FString & GroupName = GroupNames[ GeoGroupNameIdx ];

                // We're going to order the groups:
                // Simple and convex invisible colliders should be created first as they will have to be attached to the visible meshes
                // LODs should be created then, ordered by their names
                // Visible colliders should be created then, and finally, invisible complex colliders
                if ( GroupName.StartsWith( SimpleCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    // Invisible simple colliders should be treated first
                    SplitGroupNames.Insert( GroupName, 0 );
                    nLODInsertPos++;
                }
                else if ( GroupName.StartsWith( UCXCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    // Invisible complex colliders should be treated first
                    SplitGroupNames.Insert( GroupName, 0 );
                    nLODInsertPos++;
                }
                else if ( GroupName.StartsWith( LodGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    // LOD meshes should be next
                    SplitGroupNames.Insert( GroupName, nLODInsertPos++ );
                    NumberOfLODs++;
                }
                else if ( GroupName.StartsWith( RenderedCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    // Visible colliders (simple, convex or complex) should be treated after LODs
                    SplitGroupNames.Insert( GroupName, nLODInsertPos );
                }
                else if ( GroupName.StartsWith(CollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    // Invisible complex colliders should be treated last
                    SplitGroupNames.Add( GroupName );
                }
            }

            bool bRequireSplit = SplitGroupNames.Num() > 0;

            TMap< FString, TArray< int32 > > GroupSplitFaces;
            TMap< FString, int32 > GroupSplitFaceCounts;
            TMap< FString, TArray< int32 > > GroupSplitFaceIndices;

            int32 GroupVertexListCount = 0;
            static const FString RemainingGroupName = TEXT( HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION );

            if ( bRequireSplit )
            {
                // Buffer for all vertex indices used for split groups.
                // We need this to figure out all vertex indices that are not part of them. 
                TArray< int32 > AllSplitVertexList;
                AllSplitVertexList.SetNumZeroed( PartVertexList.Num() );

                // Buffer for all face indices used for split groups.
                // We need this to figure out all face indices that are not part of them.
                TArray< int32 > AllSplitFaceIndices;
                AllSplitFaceIndices.SetNumZeroed( PartFaceMaterialIds.Num() );

                // Some of the groups may contain invalid geometry 
                // Store them here so we can remove them afterwards
                TArray< int32 > InvalidGroupNameIndices;

                // Extract the vertices/faces for each of the split groups
                for ( int32 SplitIdx = 0; SplitIdx < SplitGroupNames.Num(); SplitIdx++ )
                {
                    FString GroupName = SplitGroupNames[ SplitIdx ];

                    // New vertex list just for this group.
                    TArray< int32 > GroupVertexList;
                    TArray< int32 > AllFaceList;

                    // Extract vertex indices for this split.
                    GroupVertexListCount = FHoudiniEngineUtils::HapiGetVertexListForGroup(
                        AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, GroupName, PartVertexList, GroupVertexList,
                        AllSplitVertexList, AllFaceList, AllSplitFaceIndices, PartInfo.isInstanced );

                    if ( GroupVertexListCount <= 0 )
                    {
                        // This group doesn't have vertices/faces, mark it as invalid
                        InvalidGroupNameIndices.Add( SplitIdx );

                        // Error getting the vertex list.
                        HOUDINI_LOG_MESSAGE(
                            TEXT( "Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list for group %s - skipping." ),
                            ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName, *GroupName );

                        continue;
                    }

                    // If list is not empty, we store it for this group - this will define new mesh.
                    GroupSplitFaces.Add( GroupName, GroupVertexList );
                    GroupSplitFaceCounts.Add( GroupName, GroupVertexListCount );
                    GroupSplitFaceIndices.Add( GroupName, AllFaceList );
                }

                if ( InvalidGroupNameIndices.Num() > 0 )
                {
                    // Remove all invalid split groups
                    for (int32 InvalIdx = InvalidGroupNameIndices.Num() - 1; InvalIdx >= 0; InvalIdx--)
                    {
                        int32 Index = InvalidGroupNameIndices[ InvalIdx ];

                        if ( SplitGroupNames[Index].StartsWith(LodGroupNamePrefix, ESearchCase::IgnoreCase) )
                            NumberOfLODs--;

                        SplitGroupNames.RemoveAt( Index );

                        if (Index <= nLODInsertPos)
                            nLODInsertPos--;
                    }
                }

                // We also need to figure out / construct vertex list for everything that's not in a split group
                TArray< int32 > GroupSplitFacesRemaining;
                GroupSplitFacesRemaining.Init( -1, PartVertexList.Num() );
                bool bMainSplitGroup = false;
                GroupVertexListCount = 0;

                TArray< int32 > GroupSplitFaceIndicesRemaining;
                for ( int32 SplitVertexIdx = 0; SplitVertexIdx < AllSplitVertexList.Num(); SplitVertexIdx++ )
                {
                    if ( AllSplitVertexList[ SplitVertexIdx ] == 0 )
                    {
                        // This is unused index, we need to add it to unused vertex list.
                        GroupSplitFacesRemaining[ SplitVertexIdx ] = PartVertexList[ SplitVertexIdx ];
                        bMainSplitGroup = true;
                        GroupVertexListCount++;
                    }
                }

                for ( int32 SplitFaceIdx = 0; SplitFaceIdx < AllSplitFaceIndices.Num(); SplitFaceIdx++ )
                {
                    if ( AllSplitFaceIndices[ SplitFaceIdx ] == 0 )
                    {
                        // This is unused face, we need to add it to unused faces list.
                        GroupSplitFaceIndicesRemaining.Add( SplitFaceIdx );
                    }
                }

                // We store the remaining geo vertex list as a special name (main geo)
                // and make sure its treated before the collider meshes
                if ( bMainSplitGroup )
                {
                    SplitGroupNames.Insert( RemainingGroupName, nLODInsertPos );
                    GroupSplitFaces.Add( RemainingGroupName, GroupSplitFacesRemaining );
                    GroupSplitFaceCounts.Add( RemainingGroupName, GroupVertexListCount );
                    GroupSplitFaceIndices.Add( RemainingGroupName, GroupSplitFaceIndicesRemaining );
                }
            }
            else
            {
                // No splitting required
                SplitGroupNames.Add( RemainingGroupName );
                GroupSplitFaces.Add( RemainingGroupName, PartVertexList );
                GroupSplitFaceCounts.Add( RemainingGroupName, PartVertexList.Num() );

                TArray<int32> AllFaces;
                for ( int32 FaceIdx = 0; FaceIdx < PartInfo.faceCount; ++FaceIdx )
                    AllFaces.Add( FaceIdx );

                GroupSplitFaceIndices.Add( RemainingGroupName, AllFaces );
            }

            // Look for LOD Specific attributes, "lod_screensize" by default
            TArray< float > LODScreenSizes;
            HAPI_AttributeInfo AttribInfoLODScreenSize;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfoLODScreenSize );
            FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                "lod_screensize", AttribInfoLODScreenSize, LODScreenSizes );

            // Keep track of the LOD Index
            int32 LodIndex = 0;
            int32 LodSplitId = -1;

            // Map of Houdini Material IDs to Unreal Material Indices
            TMap< HAPI_NodeId, int32 > MapHoudiniMatIdToUnrealIndex;
            // Map of Houdini Material Attributes to Unreal Material Indices
            TMap< FString, int32 > MapHoudiniMatAttributesToUnrealIndex;

            // Iterate through all detected split groups we care about and split geometry.
            // The split are ordered in the following way:
            // Invisible Simple/Convex Colliders > LODs > MainGeo > Visible Colliders > Invisible Colliders
            for ( int32 SplitId = 0; SplitId < SplitGroupNames.Num(); SplitId++ )
            {
                // Get split group name
                const FString & SplitGroupName = SplitGroupNames[ SplitId ];

                // Get the vertex indices for this group
                TArray< int32 > & SplitGroupVertexList = GroupSplitFaces[ SplitGroupName ];

                // Get valid count of vertex indices for this split.
                int32 SplitGroupVertexListCount = GroupSplitFaceCounts[ SplitGroupName ];

                // Get face indices for this split.
                TArray< int32 > & SplitGroupFaceIndices = GroupSplitFaceIndices[ SplitGroupName ];

                // LOD meshes need to use the same SplitID (as they will be on the same static mesh)
                bool IsLOD = SplitGroupName.StartsWith( LodGroupNamePrefix, ESearchCase::IgnoreCase );
                if ( IsLOD && LodSplitId == -1 )
                    LodSplitId = SplitId;

                // Materials maps (Houdini to Unreal) needs to be reset for each static mesh generated
                // Only the first LOD resets those maps
                if ( !IsLOD || ( IsLOD && LodIndex == 0 ) )
                {
                    MapHoudiniMatIdToUnrealIndex.Empty();
                    MapHoudiniMatAttributesToUnrealIndex.Empty();
                }

                // Record split id in geo part.
                // LODs must use the same SplitID since they belong to the same static mesh
                HoudiniGeoPartObject.SplitId = !IsLOD ? SplitId : LodSplitId;

                // Reset collision flags for the current GeoPartObj
                HoudiniGeoPartObject.bIsRenderCollidable = false;
                HoudiniGeoPartObject.bIsCollidable = false;
                HoudiniGeoPartObject.bIsSimpleCollisionGeo = false;
                HoudiniGeoPartObject.bIsUCXCollisionGeo = false;

                // Reset the collision/socket added flags if needed
                // For LODs, only reset the collision flag for the first LOD level
                if ( !IsLOD || ( IsLOD && LodIndex == 0 ) )
                {
                    HoudiniGeoPartObject.bHasCollisionBeenAdded = false;
                    HoudiniGeoPartObject.bHasSocketBeenAdded = false;
                }

                // Determining the type of collision:
                // UCX and simple collisions need to be checked first as they both start in the same way
                // as their non UCX/non simple equivalent!}
                if ( SplitGroupName.StartsWith( UCXCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    HoudiniGeoPartObject.bIsUCXCollisionGeo = true;
                }
                else if ( SplitGroupName.StartsWith( UCXRenderedCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    HoudiniGeoPartObject.bIsRenderCollidable = true;
                    HoudiniGeoPartObject.bIsUCXCollisionGeo = true;
                }
                else if ( SplitGroupName.StartsWith( SimpleRenderedCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    HoudiniGeoPartObject.bIsRenderCollidable = true;
                    HoudiniGeoPartObject.bIsSimpleCollisionGeo = true;
                }
                else if ( SplitGroupName.StartsWith( SimpleCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    HoudiniGeoPartObject.bIsCollidable = true;
                    HoudiniGeoPartObject.bIsSimpleCollisionGeo = true;
                }
                else if ( SplitGroupName.StartsWith( RenderedCollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    HoudiniGeoPartObject.bIsRenderCollidable = true;
                }
                else if ( SplitGroupName.StartsWith( HoudiniRuntimeSettings->CollisionGroupNamePrefix, ESearchCase::IgnoreCase ) )
                {
                    HoudiniGeoPartObject.bIsCollidable = true;
                }

                // Handling UCX/Convex Hull colliders
                if ( HoudiniGeoPartObject.bIsUCXCollisionGeo )
                {
                    // Retrieve the vertices positions if necessary
                    if ( PartPositions.Num() <= 0 )
                    {
                        if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, PartPositions ) )
                        {
                            // Error retrieving positions.
                            HOUDINI_LOG_MESSAGE(
                                TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data ")
                                TEXT("- skipping."),
                                ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName );

                            break;
                        }
                    }

                    // Use multiple convex hulls?
                    bool MultiHullDecomp = false;
                    if ( SplitGroupName.Contains( TEXT("ucx_multi"), ESearchCase::IgnoreCase ) )
                        MultiHullDecomp = true;

                    // Create the convex hull colliders and add them to the Aggregate
                    if ( AddConvexCollisionToAggregate( PartPositions, SplitGroupVertexList, MultiHullDecomp, AggregateCollisionGeo ) )
                    {
                        // We'll add the collision after all the meshes are generated unless this a rendered_collision_geo_ucx
                        bHasAggregateGeometryCollision = true;
                    }

                    // No need to create a mesh if the colliders is not visible
                    if ( !HoudiniGeoPartObject.bIsRenderCollidable )
                        continue;
                }

                // Record split group name.
                HoudiniGeoPartObject.SplitName = SplitGroupName;

                // Attempt to locate static mesh from previous instantiation.
                UStaticMesh * const * FoundStaticMesh = StaticMeshesIn.Find( HoudiniGeoPartObject );

                // LODs levels other than the first one need to reuse StaticMesh from the output!
                if ( IsLOD && LodIndex > 0 )
                    FoundStaticMesh = StaticMeshesOut.Find( HoudiniGeoPartObject );

                // Flag whether we need to rebuild the mesh.
                bool bRebuildStaticMesh = false;

                // If the geometry and scaling factor have changed or if the user asked for a cook manually,
                // we will need to rebuild the static mesh. If not, then we can reuse the corresponding static mesh.
                if ( GeoInfo.hasGeoChanged || ForceRebuildStaticMesh || ForceRecookAll )
                    bRebuildStaticMesh = true;

                // The geometry has not changed,
                if ( !bRebuildStaticMesh )
                {
                    // No mesh located, unless this split is a simple/convex collider, this is an error.
                    if ( !FoundStaticMesh && ( !HoudiniGeoPartObject.bIsSimpleCollisionGeo && !HoudiniGeoPartObject.bIsUCXCollisionGeo ) )
                    {
                        HOUDINI_LOG_ERROR(
                            TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] geometry has not changed ")
                            TEXT("but static mesh does not exist - skipping."),
                            ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName);
                        continue;
                    }

                    // If any of the materials on corresponding geo part object have not changed.
                    if ( !bMaterialsChanged && FoundStaticMesh && *FoundStaticMesh )
                    {
                        // We can reuse previously created geometry.
                        StaticMeshesOut.Add( HoudiniGeoPartObject, *FoundStaticMesh );
                        continue;
                    }
                }

                // TODO: FIX THIS PROPERLY!!
                // Ignore the found Static Mesh to force the creation of a new one
                // UE4.20 seems to be ignoring the fact that the SM get updated, and only displays the original one
                // This prevents meshes from updating after a cook, only updating them after a rebuild....
                if ( !IsLOD || LodIndex <= 0 )
                    FoundStaticMesh = nullptr;

                // If the static mesh was not located, we need to create a new one.
                bool bStaticMeshCreated = false;
                UStaticMesh * StaticMesh = nullptr;
                if ( !FoundStaticMesh || *FoundStaticMesh == nullptr )
                {
                    FGuid MeshGuid;
                    MeshGuid.Invalidate();

                    FString MeshName;
                    UPackage * MeshPackage = FHoudiniEngineBakeUtils::BakeCreateStaticMeshPackageForComponent(
                        HoudiniCookParams, HoudiniGeoPartObject, MeshName, MeshGuid );

                    if( !MeshPackage )
                        continue;

                    StaticMesh = NewObject< UStaticMesh >(
                        MeshPackage, FName( *MeshName ),
                        ( HoudiniCookParams.StaticMeshBakeMode == EBakeMode::Intermediate ) ? RF_NoFlags : RF_Public |  RF_Standalone );

                    // Add meta information to this package.
                    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                        MeshPackage, MeshPackage, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
                    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                        MeshPackage, MeshPackage, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName );

                    // Notify system that new asset has been created.
                    FAssetRegistryModule::AssetCreated( StaticMesh );

                    bStaticMeshCreated = true;
                }
                else
                {
                    // Found the corresponding Static Mesh, just reuse it.
                    StaticMesh = *FoundStaticMesh;
                }

                if ( !IsLOD || LodIndex == 0 )
                {
                    // We need to initialize the LODs used by this mesh
                    int32 NeededLODs = IsLOD ? NumberOfLODs : 1;
                    while ( StaticMesh->SourceModels.Num() < NeededLODs )
                        new (StaticMesh->SourceModels) FStaticMeshSourceModel();

                    // We may have to remove excessive LOD levels
                    if ( StaticMesh->SourceModels.Num() > NeededLODs )
                        StaticMesh->SourceModels.SetNum( NeededLODs );
                }

                // Grab the corresponding SourceModel
                FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[ IsLOD ? LodIndex : 0 ];
                if ( !SrcModel )
                {
                    HOUDINI_LOG_ERROR(
                        TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] Could not access SourceModel for the LOD %d - skipping."),
                        ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName, IsLOD ? LodIndex : 0 );
                    continue;
                }

                // Load existing raw model. This will be empty as we are constructing a new mesh.
                FRawMesh RawMesh;

                // Compute number of faces.
                int32 SplitGroupFaceCount = SplitGroupFaceIndices.Num();

                if ( bRebuildStaticMesh )
                {
                    //--------------------------------------------------------------------------------------------------------------------- 
                    // NORMALS
                    //--------------------------------------------------------------------------------------------------------------------- 
                    TArray< float > SplitGroupNormals;

                    // No need to read the normals if we'll recompute them after
                    bool bReadNormals = HoudiniRuntimeSettings->RecomputeNormalsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always;                
                    if ( bReadNormals )
                    {
                        if ( PartNormals.Num() <= 0 )
                        {
                            // Retrieve normal data for this part
                            FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                                AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                                PartInfo.id, HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, PartNormals );
                        }

                        // See if we need to transfer normal point attributes to vertex attributes.
                        FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                            SplitGroupVertexList, AttribInfoNormals, PartNormals, SplitGroupNormals );
                    }

                    // See if we need to generate tangents, we do this only if normals are present, and if we do not recompute them after
                    bool bGenerateTangents = ( SplitGroupNormals.Num() > 0 );
                    if ( bGenerateTangents && ( HoudiniRuntimeSettings->RecomputeTangentsFlag == EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always ) )
                    {
                        // No need to generate tangents if we'll recompute them after
                        bGenerateTangents = false;
                    }

                    // Transfer normals.
                    int32 WedgeNormalCount = SplitGroupNormals.Num() / 3;
                    RawMesh.WedgeTangentZ.SetNumZeroed( WedgeNormalCount );
                    for ( int32 WedgeTangentZIdx = 0; WedgeTangentZIdx < WedgeNormalCount; ++WedgeTangentZIdx )
                    {
                        FVector WedgeTangentZ;
                        WedgeTangentZ.X = SplitGroupNormals[ WedgeTangentZIdx * 3 + 0 ];
                        if ( ImportAxis == HRSAI_Unreal )
                        {
                            // We need to flip Z and Y coordinate
                            WedgeTangentZ.Y = SplitGroupNormals[ WedgeTangentZIdx * 3 + 2 ];
                            WedgeTangentZ.Z = SplitGroupNormals[ WedgeTangentZIdx * 3 + 1 ];
                        }
                        else
                        {
                            WedgeTangentZ.Y = SplitGroupNormals[ WedgeTangentZIdx * 3 + 1 ];
                            WedgeTangentZ.Z = SplitGroupNormals[ WedgeTangentZIdx * 3 + 2 ];
                        }

                        RawMesh.WedgeTangentZ[ WedgeTangentZIdx ] = WedgeTangentZ;

                        // If we need to generate tangents.
                        if ( bGenerateTangents )
                        {
                            FVector TangentX, TangentY;
                            WedgeTangentZ.FindBestAxisVectors( TangentX, TangentY );

                            RawMesh.WedgeTangentX.Add( TangentX );
                            RawMesh.WedgeTangentY.Add( TangentY );
                        }
                    }

                    //--------------------------------------------------------------------------------------------------------------------- 
                    //  VERTEX COLORS AND ALPHAS
                    //---------------------------------------------------------------------------------------------------------------------                                     
                    TArray< float > SplitGroupColors;
                    if ( PartColors.Num() <= 0 )
                    {
                        // Retrieve color data
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_COLOR, AttribInfoColors, PartColors );
                    }

                    // See if we need to transfer color point attributes to vertex attributes.
                    FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                        SplitGroupVertexList, AttribInfoColors, PartColors, SplitGroupColors );

                    TArray< float > SplitGroupAlphas;
                    if ( PartAlphas.Num() <= 0 )
                    {
                        // Retrieve alpha data
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_ALPHA, AttribInfoAlpha, PartAlphas );
                    }

                    // See if we need to transfer alpha point attributes to vertex attributes.
                    FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                        SplitGroupVertexList, AttribInfoAlpha, PartAlphas, SplitGroupAlphas );

                    // Transfer colors and alphas to the raw mesh
                    if ( AttribInfoColors.exists && ( AttribInfoColors.tupleSize > 0 ) )
                    {
                        int32 WedgeColorsCount = SplitGroupColors.Num() / AttribInfoColors.tupleSize;
                        RawMesh.WedgeColors.SetNumZeroed( WedgeColorsCount );
                        for ( int32 WedgeColorIdx = 0; WedgeColorIdx < WedgeColorsCount; ++WedgeColorIdx )
                        {
                            FLinearColor WedgeColor;
                            WedgeColor.R = FMath::Clamp(
                                SplitGroupColors[ WedgeColorIdx * AttribInfoColors.tupleSize + 0 ], 0.0f, 1.0f );
                            WedgeColor.G = FMath::Clamp(
                                SplitGroupColors[ WedgeColorIdx * AttribInfoColors.tupleSize + 1 ], 0.0f, 1.0f );
                            WedgeColor.B = FMath::Clamp(
                                SplitGroupColors[ WedgeColorIdx * AttribInfoColors.tupleSize + 2 ], 0.0f, 1.0f );

                            if( AttribInfoAlpha.exists )
                            {
                                WedgeColor.A = FMath::Clamp( SplitGroupAlphas[ WedgeColorIdx ], 0.0f, 1.0f );
                            }
                            else if ( AttribInfoColors.tupleSize == 4 )
                            {
                                // We have alpha.
                                WedgeColor.A = FMath::Clamp(
                                    SplitGroupColors[ WedgeColorIdx * AttribInfoColors.tupleSize + 3 ], 0.0f, 1.0f );
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
                        // No Colors or Alphas, init colors to White
                        FColor DefaultWedgeColor = FLinearColor::White.ToFColor( false );
                        int32 WedgeColorsCount = RawMesh.WedgeIndices.Num();
                        if ( WedgeColorsCount > 0 )
                            RawMesh.WedgeColors.Init( DefaultWedgeColor, WedgeColorsCount );
                    }

                    //--------------------------------------------------------------------------------------------------------------------- 
                    //  FACE SMOOTHING
                    //--------------------------------------------------------------------------------------------------------------------- 

                    // Retrieve face smoothing data.
                    if ( PartFaceSmoothingMasks.Num() <= 0 )
                    {
                        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, MarshallingAttributeNameFaceSmoothingMask.c_str(),
                            AttribInfoFaceSmoothingMasks, PartFaceSmoothingMasks );
                    }

                    // Set face smoothing masks.
                    RawMesh.FaceSmoothingMasks.SetNumZeroed( SplitGroupFaceCount );
                    if ( PartFaceSmoothingMasks.Num() )
                    {
                        int32 ValidFaceIdx = 0;
                        for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3 )
                        {
                            int32 WedgeCheck = SplitGroupVertexList[ VertexIdx + 0 ];
                            if ( WedgeCheck == -1 )
                                continue;

                            RawMesh.FaceSmoothingMasks[ ValidFaceIdx ] = PartFaceSmoothingMasks[ VertexIdx / 3 ];
                            ValidFaceIdx++;
                        }
                    }

                    //--------------------------------------------------------------------------------------------------------------------- 
                    //  UVS
                    //--------------------------------------------------------------------------------------------------------------------- 

                    // Extract all UV sets
                    TArray< TArray< float > > SplitGroupUVs;
                    SplitGroupUVs.SetNumZeroed( MAX_STATIC_TEXCOORDS );

                    if ( PartUVs.Num() && PartUVs[0].Num() <= 0 )
                    {
                        // Retrieve all the UVs sets for this part
                        FHoudiniEngineUtils::GetAllUVAttributesInfoAndTexCoords(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                            AttribInfoUVs, PartUVs );
                    }

                    // See if we need to transfer uv point attributes to vertex attributes.
                    for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                    {
                        FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
                            SplitGroupVertexList, AttribInfoUVs[ TexCoordIdx ], PartUVs[ TexCoordIdx ], SplitGroupUVs[ TexCoordIdx ] );
                    }

                    // Transfer UVs to the Raw Mesh
                    int32 UVChannelCount = 0;
                    int32 LightMapUVChannel = 0;
                    for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                    {
                        TArray< float > & TextureCoordinate = SplitGroupUVs[ TexCoordIdx ];
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

                            if ( UVChannelCount <= 2 )
                                LightMapUVChannel = TexCoordIdx;
                        }
                        else
                        {
                            RawMesh.WedgeTexCoords[ TexCoordIdx ].Empty();
                        }
                    }

                    // We have to have at least one UV channel. If there's none, create one with zero data.
                    if ( UVChannelCount == 0 )
                        RawMesh.WedgeTexCoords[ 0 ].SetNumZeroed( SplitGroupVertexListCount );

                    // Set the lightmap Coordinate Index
                    // If we have more than one UV set, the 2nd set will be used for lightmaps by convention
                    // If not, the first UV set will be used
                    StaticMesh->LightMapCoordinateIndex = LightMapUVChannel;

                    //--------------------------------------------------------------------------------------------------------------------- 
                    // LIGHTMAP RESOLUTION
                    //--------------------------------------------------------------------------------------------------------------------- 

                    // Get lightmap resolution (if present).
                    if ( PartLightMapResolutions.Num() <= 0 )
                    {
                        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, MarshallingAttributeNameLightmapResolution.c_str(),
                            AttribLightmapResolution, PartLightMapResolutions );
                    }

                    //--------------------------------------------------------------------------------------------------------------------- 
                    //  INDICES
                    //--------------------------------------------------------------------------------------------------------------------- 
                    
                    //
                    // Because of the splits, we don't need to declare all the vertices in the Part, 
                    // but only the one that are currently used by the split's faces.
                    // The indicesMapper array is used to map those indices from Part Vertices to Split Vertices.
                    // We also keep track of the needed vertices index to declare them easily afterwards.
                    //

                    // IndicesMapper:
                    // Maps index values for all vertices in the Part:
                    // - Vertices unused by the split will be set to -1
                    // - Used vertices will have their value set to the "NewIndex"
                    // So that IndicesMapper[ oldIndex ] => newIndex
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

                        int32 WedgeIndices[ 3 ] = 
                        {
                            SplitGroupVertexList[ VertexIdx + 0 ],
                            SplitGroupVertexList[ VertexIdx + 1 ],
                            SplitGroupVertexList[ VertexIdx + 2 ]
                        };

                        // Converting Old (Part) Indices to New (Split) Indices:
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
                            // Flip wedge indices to fix the winding order.
                            RawMesh.WedgeIndices[ ValidVertexId + 0 ] = WedgeIndices[ 0 ];
                            RawMesh.WedgeIndices[ ValidVertexId + 1 ] = WedgeIndices[ 2 ];
                            RawMesh.WedgeIndices[ ValidVertexId + 2 ] = WedgeIndices[ 1 ];

                            // Check if we need to patch UVs.
                            for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
                            {
                                if ( RawMesh.WedgeTexCoords[ TexCoordIdx ].Num() > 0
                                    && ( (ValidVertexId + 2) < RawMesh.WedgeTexCoords[ TexCoordIdx ].Num() ) )
                                {
                                    Swap( RawMesh.WedgeTexCoords[ TexCoordIdx ][ ValidVertexId + 1 ],
                                        RawMesh.WedgeTexCoords[ TexCoordIdx ][ ValidVertexId + 2 ] );
                                }
                            }

                            // Check if we need to patch colors.
                            if ( RawMesh.WedgeColors.Num() > 0 )
                                Swap( RawMesh.WedgeColors[ ValidVertexId + 1 ], RawMesh.WedgeColors[ ValidVertexId + 2 ] );

                            // Check if we need to patch Normals and tangents.
                            if ( RawMesh.WedgeTangentZ.Num() > 0 )
                                Swap( RawMesh.WedgeTangentZ[ ValidVertexId + 1 ], RawMesh.WedgeTangentZ[ ValidVertexId + 2 ] );

                            if ( RawMesh.WedgeTangentX.Num() > 0 )
                                Swap( RawMesh.WedgeTangentX[ ValidVertexId + 1 ], RawMesh.WedgeTangentX[ ValidVertexId + 2 ] );

                            if ( RawMesh.WedgeTangentY.Num() > 0 )
                                Swap ( RawMesh.WedgeTangentY[ ValidVertexId + 1 ], RawMesh.WedgeTangentY[ ValidVertexId + 2 ] );
                        }
                        else if ( ImportAxis == HRSAI_Houdini )
                        {
                            // Dont flip the wedge indices
                            RawMesh.WedgeIndices[ ValidVertexId + 0 ] = WedgeIndices[ 0 ];
                            RawMesh.WedgeIndices[ ValidVertexId + 1 ] = WedgeIndices[ 1 ];
                            RawMesh.WedgeIndices[ ValidVertexId + 2 ] = WedgeIndices[ 2 ];
                        }

                        ValidVertexId += 3;
                    }

                    //--------------------------------------------------------------------------------------------------------------------- 
                    // POSITIONS
                    //--------------------------------------------------------------------------------------------------------------------- 

                    // We may already have gotten the positions when creating the ucx collisions
                    if ( PartPositions.Num() <= 0 )
                    {
                        // Retrieve position data.
                        if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                            PartInfo.id, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, PartPositions ) )
                        {
                            // Error retrieving positions.
                            HOUDINI_LOG_MESSAGE(
                                TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data ")
                                TEXT("- skipping."),
                                ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName);

                            if ( bStaticMeshCreated )
                                StaticMesh->MarkPendingKill();

                            break;
                        }
                    }

                    //
                    // Transfer vertex positions:
                    //
                    // Because of the split, we're only interested in the needed vertices.
                    // Instead of declaring all the Positions, we'll only declare the vertices
                    // needed by the current split.
                    //
                    int32 VertexPositionsCount = NeededVertices.Num();
                    RawMesh.VertexPositions.SetNumZeroed( VertexPositionsCount );
                    for ( int32 VertexPositionIdx = 0; VertexPositionIdx < VertexPositionsCount; ++VertexPositionIdx )
                    {
                        int32 NeededVertexIndex = NeededVertices[ VertexPositionIdx ];

                        FVector VertexPosition;
                        VertexPosition.X = PartPositions[ NeededVertexIndex * 3 + 0 ] * GeneratedGeometryScaleFactor;
                        if ( ImportAxis == HRSAI_Unreal )
                        {
                            // We need to swap Z and Y coordinate here.                            
                            VertexPosition.Y = PartPositions[ NeededVertexIndex * 3 + 2 ] * GeneratedGeometryScaleFactor;
                            VertexPosition.Z = PartPositions[ NeededVertexIndex * 3 + 1 ] * GeneratedGeometryScaleFactor;
                        }
                        else if ( ImportAxis == HRSAI_Houdini )
                        {
                            // No swap required.
                            VertexPosition.Y = PartPositions[ NeededVertexIndex * 3 + 1 ] * GeneratedGeometryScaleFactor;
                            VertexPosition.Z = PartPositions[ NeededVertexIndex * 3 + 2 ] * GeneratedGeometryScaleFactor;
                        }

                        RawMesh.VertexPositions[ VertexPositionIdx ] = VertexPosition;
                    }

                    // We need to check if this mesh contains only degenerate triangles.
                    if ( FHoudiniEngineUtils::CountDegenerateTriangles( RawMesh ) == SplitGroupFaceCount )
                    {
                        // This mesh contains only degenerate triangles, there's nothing we can do.
                        if ( bStaticMeshCreated )
                            StaticMesh->MarkPendingKill();

                        continue;
                    }
                }
                else
                {
                    // We dont need to rebuild the mesh (because the geometry hasn't changed, but the materials have)
                    // So we can just load the old data into the Raw mesh and reuse it.
                    FRawMeshBulkData * InRawMeshBulkData = SrcModel->RawMeshBulkData;
                    InRawMeshBulkData->LoadRawMesh( RawMesh );
                }

                //--------------------------------------------------------------------------------------------------------------------- 
                // MATERIAL ATTRIBUTE OVERRIDES
                //---------------------------------------------------------------------------------------------------------------------

                // See if we have material override attributes
                if ( PartFaceMaterialAttributeOverrides.Num() <= 0 )
                {
                    FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                        AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                        MarshallingAttributeNameMaterial.c_str(),
                        AttribFaceMaterials, PartFaceMaterialAttributeOverrides );

                    // If material attribute was not found, check fallback compatibility attribute.
                    if ( !AttribFaceMaterials.exists )
                    {
                        PartFaceMaterialAttributeOverrides.Empty();
                        FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                            MarshallingAttributeNameMaterialFallback.c_str(),
                            AttribFaceMaterials, PartFaceMaterialAttributeOverrides );
                    }

                    // If material attribute and fallbacks were not found, check the material instance attribute.
                    if ( !AttribFaceMaterials.exists )
                    {
                        PartFaceMaterialAttributeOverrides.Empty();
                        FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                            MarshallingAttributeNameMaterialInstance.c_str(),
                            AttribFaceMaterials, PartFaceMaterialAttributeOverrides);
                    }

                    if ( AttribFaceMaterials.exists && AttribFaceMaterials.owner != HAPI_ATTROWNER_PRIM && AttribFaceMaterials.owner != HAPI_ATTROWNER_DETAIL )
                    {
                        HOUDINI_LOG_WARNING( TEXT( "Static Mesh [%d %s], Geo [%d], Part [%d %s]: unreal_material must be a primitive or detail attribute, ignoring attribute." ),
                            ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName);
                        AttribFaceMaterials.exists = false;
                        PartFaceMaterialAttributeOverrides.Empty();
                    }

                    // If the material name was assigned per detail we replicate it for each primitive.
                    if ( PartFaceMaterialAttributeOverrides.Num() > 0 && AttribFaceMaterials.owner == HAPI_ATTROWNER_DETAIL )
                    {
                        FString SingleFaceMaterial = PartFaceMaterialAttributeOverrides[ 0 ];
                        PartFaceMaterialAttributeOverrides.Init( SingleFaceMaterial, SplitGroupVertexList.Num() / 3 );
                    }
                }

                //--------------------------------------------------------------------------------------------------------------------- 
                // FACE MATERIALS
                //---------------------------------------------------------------------------------------------------------------------

                // Process material overrides first.
                if ( PartFaceMaterialAttributeOverrides.Num() > 0 )
                {
                    // Clear the previously generated materials ( unless we're not the first lod level )
                    if ( !IsLOD || ( IsLOD && LodIndex == 0 ) )
                        StaticMesh->StaticMaterials.Empty();

                    RawMesh.FaceMaterialIndices.SetNumZeroed( SplitGroupFaceCount );
                    for ( int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx )
                    {
                        int32 SplitFaceIndex = SplitGroupFaceIndices[ FaceIdx ];
                        if ( !PartFaceMaterialAttributeOverrides.IsValidIndex( SplitFaceIndex ) )
                            continue;

                        const FString & MaterialName = PartFaceMaterialAttributeOverrides[ SplitFaceIndex ];
                        int32 const * FoundFaceMaterialIdx = MapHoudiniMatAttributesToUnrealIndex.Find( MaterialName );
                        int32 CurrentFaceMaterialIdx = 0;
                        if ( FoundFaceMaterialIdx )
                        {
                            CurrentFaceMaterialIdx = *FoundFaceMaterialIdx;
                        }
                        else
                        {
                            UMaterialInterface * MaterialInterface = Cast< UMaterialInterface >(
                                StaticLoadObject( UMaterialInterface::StaticClass(),
                                    nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr ) );

                            if ( MaterialInterface )
                            {
                                // Make sure this material is in the assignments before replacing it.
                                if( !HoudiniCookParams.HoudiniCookManager->GetAssignmentMaterial( MaterialInterface->GetName() ) )
                                    HoudiniCookParams.HoudiniCookManager->AddAssignmentMaterial( MaterialInterface->GetName(), MaterialInterface );

                                // See if we have a replacement material for this.
                                UMaterialInterface * ReplacementMaterialInterface = HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialInterface->GetName() );
                                if( ReplacementMaterialInterface )
                                    MaterialInterface = ReplacementMaterialInterface;

                                // Add this material to the map
                                CurrentFaceMaterialIdx = StaticMesh->StaticMaterials.Add( FStaticMaterial( MaterialInterface ) );
                                MapHoudiniMatAttributesToUnrealIndex.Add( MaterialName, CurrentFaceMaterialIdx );
                            }
                            else
                            {
                                // The Attribute Material and its replacement do not exist
                                // See if we can fallback to the Houdini material assigned on the face

                                // Get the unreal material corresponding to this houdini one
                                HAPI_NodeId MaterialId = PartFaceMaterialIds[ SplitFaceIndex ];

                                // See if we have already treated that material
                                int32 const * FoundUnrealMatIndex = MapHoudiniMatIdToUnrealIndex.Find( MaterialId );
                                if ( FoundUnrealMatIndex )
                                {
                                    // This material has been mapped already, just assign the mat index
                                    CurrentFaceMaterialIdx = *FoundUnrealMatIndex;
                                }
                                else
                                {
                                    // If everything fails, we'll use the default material
                                    MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

                                    // We need to add this material to the map
                                    FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                                    FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName );
                                    UMaterialInterface * const * FoundMaterial = Materials.Find( MaterialShopName );
                                    if ( FoundMaterial )
                                        MaterialInterface = *FoundMaterial;

                                    // If we have a replacement material for this geo part object and this shop material name.
                                    UMaterialInterface * ReplacementMaterial =
                                        HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                                    if ( ReplacementMaterial )
                                        MaterialInterface = ReplacementMaterial;

                                    // Add the material to the Static mesh
                                    CurrentFaceMaterialIdx = StaticMesh->StaticMaterials.Add( FStaticMaterial( MaterialInterface ) );

                                    // Map the Houdini ID to the unreal one
                                    MapHoudiniMatIdToUnrealIndex.Add( MaterialId, CurrentFaceMaterialIdx );
                                }
                            }
                        }

                        // Update the Face Material on the mesh
                        RawMesh.FaceMaterialIndices[ FaceIdx ] = CurrentFaceMaterialIdx;
                    }
                }
                else
                {
                    if ( bPartHasMaterials )
                    {
                        if ( bSingleFaceMaterial )
                        {
                            // Use default Houdini material if no valid material is assigned to any of the faces.
                            UMaterialInterface * Material = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

                            // We have only one material.
                            RawMesh.FaceMaterialIndices.SetNumZeroed( SplitGroupFaceCount );

                            // Get id of this single material.
                            FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                            FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName( AssetId, PartFaceMaterialIds[ 0 ], MaterialShopName );
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
                            // We have multiple materials
                            // Clear the previously generated materials ( unless we're not the first lod level )
                            if ( !IsLOD || ( IsLOD && LodIndex == 0 ) )
                                StaticMesh->StaticMaterials.Empty();

                            // Get default Houdini material.
                            UMaterial * MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();

                            // Reset Rawmesh material face assignments.
                            RawMesh.FaceMaterialIndices.SetNumZeroed( SplitGroupFaceCount );
                            for ( int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx )
                            {
                                int32 SplitFaceIndex = SplitGroupFaceIndices[ FaceIdx ];				                                
                                if ( !PartFaceMaterialIds.IsValidIndex( SplitFaceIndex ) )
                                    continue;

                                // Get material id for this face.
                                HAPI_NodeId MaterialId = PartFaceMaterialIds[ SplitFaceIndex ];

                                // See if we have already treated that material
                                int32 const * FoundUnrealMatIndex = MapHoudiniMatIdToUnrealIndex.Find( MaterialId );
                                if ( FoundUnrealMatIndex )
                                {
                                    // This material has been mapped already, just assign the mat index
                                    RawMesh.FaceMaterialIndices[ FaceIdx ] = *FoundUnrealMatIndex;
                                    continue;
                                }

                                UMaterialInterface * Material = Cast<UMaterialInterface>(MaterialDefault);

                                FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                                FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName );
                                UMaterialInterface * const * FoundMaterial = Materials.Find( MaterialShopName );
                                if ( FoundMaterial )
                                    Material = *FoundMaterial;

                                // See if we have replacement material for this geo part object and this shop material name.
                                UMaterialInterface * ReplacementMaterial = 
                                    HoudiniCookParams.HoudiniCookManager->GetReplacementMaterial( HoudiniGeoPartObject, MaterialShopName );

                                if ( ReplacementMaterial )
                                    Material = ReplacementMaterial;

                                // Add the material to the Static mesh
                                int32 UnrealMatIndex = StaticMesh->StaticMaterials.Add( FStaticMaterial( Material ) );

                                // Map the houdini ID to the unreal one
                                MapHoudiniMatIdToUnrealIndex.Add( MaterialId, UnrealMatIndex );

                                // Update the face index
                                RawMesh.FaceMaterialIndices[ FaceIdx ] = UnrealMatIndex;
                            }
                        }
                    }
                    else
                    {
                        // No materials were found, we need to use default Houdini material.
                        RawMesh.FaceMaterialIndices.SetNumZeroed( SplitGroupFaceCount );

                        UMaterialInterface * Material = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
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
                            ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName );
                    }

                    if( PartLightMapResolutions.Num() > 0 )
                        LightMapResolutionOverride = PartLightMapResolutions[ 0 ];

                    // Apply lightmap resolution override if it has been specified
                    if ( LightMapResolutionOverride > 0 )
                        StaticMesh->LightMapResolution = LightMapResolutionOverride;
                }

                // Store the new raw mesh.
                SrcModel->RawMeshBulkData->SaveRawMesh( RawMesh );

                // Lambda for initializing a LOD level
                auto InitLODLevel = [ & ]( const int32& LODLevelIndex )
                {
                    // Ensure that this LOD level exisits
                    while ( StaticMesh->SourceModels.Num() < ( LODLevelIndex + 1 ) )
                        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

                    // Set its reduction settings to the default
                    StaticMesh->SourceModels[ LODLevelIndex ].ReductionSettings = LODGroup.GetDefaultSettings( LODLevelIndex );

                    for ( int32 MaterialIndex = 0; MaterialIndex < StaticMesh->StaticMaterials.Num(); ++MaterialIndex )
                    {
                        FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get( LODLevelIndex, MaterialIndex );
                        Info.MaterialIndex = MaterialIndex;
                        Info.bEnableCollision = true;
                        Info.bCastShadow = true;
                        StaticMesh->SectionInfoMap.Set( LODLevelIndex, MaterialIndex, Info );
                    }
                };

                if ( !IsLOD )
                {
                    // For non LODed mesh, init the default number of LODs
                    for ( int32 ModelLODIndex = 0; ModelLODIndex < DefaultNumLODs; ++ModelLODIndex )
                        InitLODLevel( ModelLODIndex );
                }
                else
                {
                    // Init the current LOD level
                    InitLODLevel( LodIndex );

                    bool InvalidateLODAttr = false;

                    // If the "lod_screensize" attribute was not found, fallback to the "lodX_screensize" attribute		    
                    if ( !AttribInfoLODScreenSize.exists )
                    {
                        LODScreenSizes.Empty();
                        FString LODAttributeName = SplitGroupName + TEXT("_screensize");
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                            TCHAR_TO_ANSI(*LODAttributeName), AttribInfoLODScreenSize, LODScreenSizes);

                        InvalidateLODAttr = AttribInfoLODScreenSize.exists;
                    }

                    // finally, look for a potential uproperty style attribute
                    if ( !AttribInfoLODScreenSize.exists )
                    {
                        LODScreenSizes.Empty();
                        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
                            AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                            "unreal_uproperty_screensize", AttribInfoLODScreenSize, LODScreenSizes);

                        InvalidateLODAttr = AttribInfoLODScreenSize.exists;
                    }

                    if ( AttribInfoLODScreenSize.exists )
                    {
                        float screensize = -1.0;
                        if ( AttribInfoLODScreenSize.owner == HAPI_ATTROWNER_PRIM )
                        {
                            int32 n = 0;
                            for( ; n <  SplitGroupVertexList.Num(); n++ )
                            {
                                if ( SplitGroupVertexList[ n ] > 0 )
                                    break;
                            }

                            screensize = LODScreenSizes[ n / 3 ];
                        }
                        else
                        {
                            // Handle single screensize attributes
                            if ( LODScreenSizes.Num() == 1 )
                                screensize = LODScreenSizes[ 0 ];
                            else
                            {
                                // Handle tuple screensize attributes
                                if ( LODScreenSizes.IsValidIndex( LodIndex ) )
                                    screensize = LODScreenSizes[ LodIndex ];
                                else
                                    screensize = 0.0f;
                            }
                        }

                        // Make sure the LOD Screensize is a percent, so if its above 1, divide by 100
                        if ( screensize > 1.0f )
                            screensize /= 100.0f;

                        // Only apply the LOD screensize if it's valid
                        if ( screensize >= 0.0f )
                        {
                            StaticMesh->SourceModels[ LodIndex ].ScreenSize = screensize;
                            StaticMesh->bAutoComputeLODScreenSize = false;
                        }

                        if ( InvalidateLODAttr )
                            AttribInfoLODScreenSize.exists = false;
                    }

                    // Increment the LODIndex
                    LodIndex++;
                }

                // The following actions needs to be done only once per Static Mesh,
                // So if we are a LOD level other than the last one, skip this!
                if ( IsLOD && ( LodIndex != NumberOfLODs ) )
                {
                    // The First LOD still needs to add the mesh to the out list so we can reuse it for the next LOD levels
                    if ( LodIndex == 1 )
                        StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );

                    continue;
                }

                // Assign generation parameters for this static mesh.
                HoudiniCookParams.HoudiniCookManager->SetStaticMeshGenerationParameters( StaticMesh );

                // Make sure we remove the old simple colliders if needed
                if( UBodySetup * BodySetup = StaticMesh->BodySetup )
                {
                    // Simple colliders are from a previous cook, remove them!
                    if( !HoudiniGeoPartObject.bHasCollisionBeenAdded )
                        BodySetup->RemoveSimpleCollision();

                    // See if we need to enable collisions on the whole mesh.
                    if( ( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
                        && ( !HoudiniGeoPartObject.bIsSimpleCollisionGeo && !HoudiniGeoPartObject.bIsUCXCollisionGeo ) )
                    {
                        // Enable collisions for this static mesh.
                        BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
                    }
                    else if ( !HoudiniGeoPartObject.bIsSimpleCollisionGeo && !HoudiniGeoPartObject.bIsUCXCollisionGeo )
                    {
                        // We dont have collider meshes, or simple colliders, if the LODForCollision uproperty attribute is set
                        // we need to activate complex collision for that lod to be picked up as collider
                        if ( HapiCheckAttributeExists( HoudiniGeoPartObject, "unreal_uproperty_LODForCollision", HAPI_ATTROWNER_DETAIL ) )
                            BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
                    }
                }

                // Try to update the uproperties of the StaticMesh
                UpdateUPropertyAttributesOnObject( StaticMesh, HoudiniGeoPartObject);

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
                        ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, PartIdx, *PartName, SplitId, *( TextError.ToString() ) );
                }

                StaticMesh->GetOnMeshChanged().Broadcast();

                // Do we need to add simple collisions ?
                bool bSimpleCollisionAddedToAggregate = false;
                if ( HoudiniGeoPartObject.bIsSimpleCollisionGeo )
                {
                    if ( AddSimpleCollision( SplitGroupName, StaticMesh, HoudiniGeoPartObject, AggregateCollisionGeo, bSimpleCollisionAddedToAggregate ) )
                    {
                        if ( bSimpleCollisionAddedToAggregate )
                        {
                            // The colliders are invisible and have been added to the aggregate,
                            // we can skip to the next object and this collider will be added after
                            bHasAggregateGeometryCollision = true;
                            continue;
                        }
                        else
                        {
                            // We don't want these collisions to be removed afterwards
                            HoudiniGeoPartObject.bHasCollisionBeenAdded = true;
                        }
                    }
                }

                // If any simple collider was added to the aggregate, and this mesh is visible, add the colliders now
                if ( bHasAggregateGeometryCollision )
                {
                    // Add the aggregate collision geo to the static mesh
                    if ( AddAggregateCollisionGeometryToStaticMesh( StaticMesh, HoudiniGeoPartObject, AggregateCollisionGeo ) )
                        bHasAggregateGeometryCollision = false;
                }

                // Sockets are attached to the mesh after, for now, clear the existing one ( as they are from a previous cook )
                if ( !HoudiniGeoPartObject.bHasSocketBeenAdded )
                    StaticMesh->Sockets.Empty();

                StaticMesh->MarkPackageDirty();

                StaticMeshesOut.Add( HoudiniGeoPartObject, StaticMesh );

            } // end for SplitId

            // Add the sockets we found to that part's meshes	    
            if ( AllSockets.Num() > 0 )
            {
                bool SocketsAdded = false;
                for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshesOut ); Iter; ++Iter )
                {
                    FHoudiniGeoPartObject * CurrentHoudiniGeoPartObject = &(Iter.Key());
                    if ( (CurrentHoudiniGeoPartObject->ObjectId != ObjectInfo.nodeId )
                        || (CurrentHoudiniGeoPartObject->GeoId != GeoInfo.nodeId )
                        || (CurrentHoudiniGeoPartObject->PartId != PartIdx  ) )
                        continue;

                    // This GeoPartObject is from the same object/geo, so we can add the sockets to it
                    if ( AddMeshSocketsToStaticMesh( Iter.Value(), *CurrentHoudiniGeoPartObject, AllSockets, AllSocketsNames, AllSocketsActors, AllSocketsTags ) )
                        SocketsAdded = true;
                }

                if ( SocketsAdded )
                {
                    // Clean up the sockets for this part
                    AllSockets.Empty();
                    AllSocketsNames.Empty();
                    AllSocketsActors.Empty();
                    AllSocketsTags.Empty();
                }
            }
        } // end for PartId

        // There should be no UCX/Simple colliders left now
        if ( bHasAggregateGeometryCollision )
            HOUDINI_LOG_ERROR( TEXT("All Simple Colliders found in the HDA were not attached to a static mesh!!") );

        // Add the sockets we found in the parts of this geo to the meshes it has generated
        if ( AllSockets.Num() > 0 )
        {
            for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshesOut ); Iter; ++Iter )
            {
                FHoudiniGeoPartObject * HoudiniGeoPartObject = &( Iter.Key() );
                if ( ( HoudiniGeoPartObject->ObjectId != ObjectInfo.nodeId ) || ( HoudiniGeoPartObject->GeoId != GeoInfo.nodeId ) )
                    continue;

                // This GeoPartObject is from the same object/geo, so we can add the sockets to it
                AddMeshSocketsToStaticMesh( Iter.Value(), *HoudiniGeoPartObject, AllSockets, AllSocketsNames, AllSocketsActors, AllSocketsTags );
            }
        }

        // Clean up the sockets for this geo
        AllSockets.Empty();
        AllSocketsNames.Empty();
        AllSocketsActors.Empty();
        AllSocketsTags.Empty();

    } // end for ObjectId

    // Now that all the meshes are built and their collisions meshes and primitives updated,
    // we need to update their pre-built navigation collision used by the navmesh
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshesOut ); Iter; ++Iter )
    {
        FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();

        // Only update for collidable objects
        if ( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
        {
            UStaticMesh* StaticMesh = Iter.Value();
            if ( !StaticMesh )
                continue;
			if (UBodySetup * BodySetup = StaticMesh->BodySetup)
			{
				// Unreal caches the Navigation Collision and never updates it for StaticMeshes,
				// so we need to manually flush and recreate the data to have proper navigation collision
				if (StaticMesh->NavCollision)
				{
					BodySetup->InvalidatePhysicsData();
					BodySetup->CreatePhysicsMeshes();

					//StaticMesh->NavCollision->InvalidatePhysicsData();
					//StaticMesh->NavCollision->InvalidateCollision();
					//StaticMesh->NavCollision->CookedFormatData.FlushData();
					//StaticMesh->NavCollision->GatherCollision();
					StaticMesh->NavCollision->Setup(BodySetup);
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
    const TArray< int32 > & VertexList, const HAPI_AttributeInfo & AttribInfo, TArray< float > & Data)
{
    TArray< float > VertexData;
    int32 ValidWedgeCount = TransferRegularPointAttributesToVertices( VertexList, AttribInfo, Data, VertexData );

    if ( ValidWedgeCount > 0 )
        Data = VertexData;

    return ValidWedgeCount;
}

int32
FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
    const TArray< int32 > & VertexList, const HAPI_AttributeInfo & AttribInfo, const TArray< float > & Data, TArray< float >& VertexData )
{
    if ( !AttribInfo.exists || AttribInfo.tupleSize <= 0 )
        return 0;

    int32 ValidWedgeCount = 0;

    // Future optimization - see if we can do direct vertex transfer.
    int32 WedgeCount = VertexList.Num();
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

    return ValidWedgeCount;
}



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

#if WITH_EDITOR
void
FHoudiniEngineUtils::CreateFaceMaterialArray(
    const TArray< UMaterialInterface * >& Materials, const TArray< int32 > & FaceMaterialIndices, TArray< char * > & OutStaticMeshFaceMaterials )
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
            MaterialInterface = Materials[ MaterialIdx ];
            if ( !MaterialInterface )
            {
                // Null material interface found, add default instead.
                MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
            }

            FString FullMaterialName = MaterialInterface->GetPathName();
            UniqueName = FHoudiniEngineUtils::ExtractRawName( FullMaterialName );
            UniqueMaterialList.Add( UniqueName );
        }
    }
    else
    {
        // We do not have any materials, add default.
        MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
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

#endif // WITH_EDITOR

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
	FString & StoredLibHAPILocation,
	bool LookIn32bitRegistry)
{
	auto FindDll = [&](const FString& InHoudiniInstallationPath)
	{
		FString HFSPath = FString::Printf(TEXT("%s/%s"), *InHoudiniInstallationPath, HAPI_HFS_SUBFOLDER_WINDOWS);

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, HAPI_LIB_OBJECT_WINDOWS);

		if (FPaths::FileExists(LibHAPIPath))
		{
			FPlatformProcess::PushDllDirectory(*HFSPath);
			void* HAPILibraryHandle = FPlatformProcess::GetDllHandle(HAPI_LIB_OBJECT_WINDOWS);
			FPlatformProcess::PopDllDirectory(*HFSPath);

			if (HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Loaded %s from Registry path %s"), HAPI_LIB_OBJECT_WINDOWS,
					*HFSPath);

				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
		return (void*)0;
	};
	FString HoudiniInstallationPath;
	FString HoudiniVersionString = ComputeVersionString(true);
	FString RegistryKey = FString::Printf(
		TEXT("Software\\%sSide Effects Software\\%s"),
		(LookIn32bitRegistry ? TEXT("WOW6432Node\\") : TEXT("")), *HoudiniInstallationType);

	if (FWindowsPlatformMisc::QueryRegKey(
		HKEY_LOCAL_MACHINE, *RegistryKey, *HoudiniVersionString, HoudiniInstallationPath))
	{
		FPaths::NormalizeDirectoryName(HoudiniInstallationPath);
		return FindDll(HoudiniInstallationPath);
	}

	return nullptr;
}

#endif

FString
FHoudiniEngineUtils::ComputeVersionString(bool ExtraDigit)
{
	// Compute Houdini version string.
	FString HoudiniVersionString = FString::Printf(
		TEXT("%d.%d.%s%d"), HAPI_VERSION_HOUDINI_MAJOR,
		HAPI_VERSION_HOUDINI_MINOR,
		(ExtraDigit ? (TEXT("0.")) : TEXT("")),
		HAPI_VERSION_HOUDINI_BUILD);

	// If we have a patch version, we need to append it.
	if (HAPI_VERSION_HOUDINI_PATCH > 0)
		HoudiniVersionString = FString::Printf(TEXT("%s.%d"), *HoudiniVersionString, HAPI_VERSION_HOUDINI_PATCH);
	return HoudiniVersionString;
}

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

    // Otherwise, we will attempt to detect Houdini installation.
    FString HoudiniLocation = TEXT( HOUDINI_ENGINE_HFS_PATH );
    FString LibHAPIPath;

	// Compute Houdini version string.
	FString HoudiniVersionString = ComputeVersionString(false);

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
        TEXT("Houdini Engine"), StoredLibHAPILocation, false);
    if ( HAPILibraryHandle )
        return HAPILibraryHandle;

    // As a third attempt, we try to look up location of Houdini installation (not Houdini Engine) in the registry.
    HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
        TEXT("Houdini"), StoredLibHAPILocation, false);
    if ( HAPILibraryHandle )
        return HAPILibraryHandle;

    // Do similar registry lookups for the 32 bits registry
    // Look for the Houdini Engine registry install path
    HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
        TEXT("Houdini Engine"), StoredLibHAPILocation, true);
    if ( HAPILibraryHandle )
        return HAPILibraryHandle;

    // ... and for the Houdini registry install path
    HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
        TEXT("Houdini"), StoredLibHAPILocation, true);
    if ( HAPILibraryHandle )
        return HAPILibraryHandle;

    // Finally, try to load from a hardcoded program files path.
    HoudiniLocation = FString::Printf(
        TEXT( "C:\\Program Files\\Side Effects Software\\Houdini %s\\%s" ), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_WINDOWS );

#else

#   if PLATFORM_MAC

    // Attempt to load from standard Mac OS X installation.
    HoudiniLocation = FString::Printf(
        TEXT("/Applications/Houdini/Houdini%s/Frameworks/Houdini.framework/Versions/%s/Libraries"), *HoudiniVersionString, *HoudiniVersionString );

#   elif PLATFORM_LINUX

    // Attempt to load from standard Linux installation.
    HoudiniLocation = FString::Printf(
        TEXT( "/opt/hfs%s/%s" ), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_LINUX );

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
    TArray< int32 > & AllCollisionFaceIndices, const bool& isPackedPrim )
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

    // Iterate through all objects.
    for ( int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx )
    {
        // Retrieve object at this index.
        const HAPI_ObjectInfo & ObjectInfo = ObjectInfos[ ObjectIdx ];

        // Get Geo information.
        HAPI_GeoInfo GeoInfo;
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetDisplayGeoInfo( FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId, &GeoInfo ) )
            continue;

        // Iterate through all parts.
        for ( int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx )
        {
            // Get part information.
            HAPI_PartInfo PartInfo;
            FString PartName = TEXT( "" );

            if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo( FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartIdx, &PartInfo ) )
                continue;

            // Retrieve material information for this geo part.
            HAPI_Bool bSingleFaceMaterial = false;
            bool bMaterialsFound = false;

            if ( PartInfo.faceCount > 0 )
            {
                TArray< HAPI_NodeId > FaceMaterialIds;
                FaceMaterialIds.SetNumUninitialized( PartInfo.faceCount );

                if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialNodeIdsOnFaces(
                    FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id, 
                    &bSingleFaceMaterial, &FaceMaterialIds[ 0 ], 0, PartInfo.faceCount ) )
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

                    if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialNodeIdsOnFaces(
                        FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id, 
                        &bSingleFaceMaterial, &InstanceMaterialId, 0, 1 ) )
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

    MaterialIds.Remove( -1 );

    return true;
}

AHoudiniAssetActor * 
FHoudiniEngineUtils::LocateClipboardActor( const AActor* IgnoreActor, const FString & ClipboardText )
{
    const TCHAR * Paste = nullptr;

    if ( ClipboardText.IsEmpty() )
    {
        FString PasteString;
#if WITH_EDITOR
        FPlatformApplicationMisc::ClipboardPaste( PasteString );
#endif
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
        StrLine = StrLine.TrimStart();

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


int32
FHoudiniEngineUtils::AddMeshSocketToList(
    HAPI_NodeId AssetId, HAPI_NodeId ObjectId,
    HAPI_NodeId GeoId, HAPI_PartId PartId,
    TArray< FTransform >& AllSockets,
    TArray< FString >& AllSocketsNames,
    TArray< FString >& AllSocketsActors,
    TArray< FString >& AllSocketsTags,
    const bool& isPackedPrim )
{    
    // Attributes we are interested in.
    // Position
    TArray< float > Positions;
    HAPI_AttributeInfo AttribInfoPositions;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoPositions, 0 );

    // Rotation
    bool bHasRotation = false;
    TArray< float > Rotations;
    HAPI_AttributeInfo AttribInfoRotations;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoRotations, 0 );

    // Scale
    bool bHasScale = false;
    TArray< float > Scales;
    HAPI_AttributeInfo AttribInfoScales;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoScales, 0 );
        
    // When using socket groups, we can also get the sockets rotation from the normal
    bool bHasNormals = false;
    TArray< float > Normals;
    HAPI_AttributeInfo AttribInfoNormals;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNormals, 0 );

    // Socket Name
    bool bHasNames = false;
    TArray< FString > Names;
    HAPI_AttributeInfo AttribInfoNames;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNames, 0 );

    // Socket Actor
    bool bHasActors = false;
    TArray< FString > Actors;
    HAPI_AttributeInfo AttribInfoActors;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoActors, 0 );

    // Socket Tags
    bool bHasTags = false;
    TArray< FString > Tags;
    HAPI_AttributeInfo AttribInfoTags;
    FMemory::Memset< HAPI_AttributeInfo >( AttribInfoTags, 0 );

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

    // Lambda function for creating the socket and adding it to the array
    // Shared between the by Attribute / by Group methods
    int32 FoundSocketCount = 0;
    auto AddSocketToArray = [&]( const int32& PointIdx )
    {
        FTransform currentSocketTransform;
        FVector currentPosition = FVector::ZeroVector;
        FVector currentScale = FVector( 1.0f, 1.0f, 1.0f );
        FQuat currentRotation = FQuat::Identity;

        if ( ImportAxis == HRSAI_Unreal )
        {
            if ( Positions.IsValidIndex( PointIdx * 3 + 2 ) )
            {
                currentPosition.X = Positions[ PointIdx * 3 ] * GeneratedGeometryScaleFactor;
                currentPosition.Y = Positions[ PointIdx * 3 + 2 ] * GeneratedGeometryScaleFactor;
                currentPosition.Z = Positions[ PointIdx * 3 + 1 ] * GeneratedGeometryScaleFactor;
            }

            if ( bHasScale && Scales.IsValidIndex( PointIdx * 3 + 2 ) )
            {
                currentScale.X = Scales[ PointIdx * 3 ];
                currentScale.Y = Scales[ PointIdx * 3 + 2 ];
                currentScale.Z = Scales[ PointIdx * 3 + 1 ];
            }

            if ( bHasRotation && Rotations.IsValidIndex( PointIdx * 4 + 3 ) )
            {
                currentRotation.X = Rotations[ PointIdx * 4 ];
                currentRotation.Y = Rotations[ PointIdx * 4 + 2 ];
                currentRotation.Z = Rotations[ PointIdx * 4 + 1 ];
                currentRotation.W = -Rotations[ PointIdx * 4 + 3 ];
            }
            else if ( bHasNormals && Normals.IsValidIndex( PointIdx * 3 + 2 ) )
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
            if ( Positions.IsValidIndex( PointIdx * 3 + 2 ) )
            {
                currentPosition.X = Positions[ PointIdx * 3 ] * GeneratedGeometryScaleFactor;
                currentPosition.Y = Positions[ PointIdx * 3 + 1 ] * GeneratedGeometryScaleFactor;
                currentPosition.Z = Positions[ PointIdx * 3 + 2 ] * GeneratedGeometryScaleFactor;
            }

            if ( bHasScale && Scales.IsValidIndex( PointIdx * 3 + 2 ) )
            {
                currentScale.X = Scales[ PointIdx * 3 ];
                currentScale.Y = Scales[ PointIdx * 3 + 1 ];
                currentScale.Z = Scales[ PointIdx * 3 + 2 ];
            }

            if ( bHasRotation && Rotations.IsValidIndex( PointIdx * 4 + 3 ) )
            {
                currentRotation.X = Rotations[ PointIdx * 4 ];
                currentRotation.Y = Rotations[ PointIdx * 4 + 1 ];
                currentRotation.Z = Rotations[ PointIdx * 4 + 2 ];
                currentRotation.W = Rotations[ PointIdx * 4 + 3 ];
            }
            else if ( bHasNormals && Normals.IsValidIndex( PointIdx * 3 + 2 ) )
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
        if ( bHasNames && Names.IsValidIndex( PointIdx ) )
            currentName = Names[ PointIdx ];

        FString currentActors;
        if ( bHasActors && Actors.IsValidIndex( PointIdx ) )
            currentActors = Actors[ PointIdx ];

        FString currentTag;
        if ( bHasTags && Tags.IsValidIndex( PointIdx ) )
            currentTag = Tags[ PointIdx ];

        // If the scale attribute wasn't set on all socket, we might end up
        // with a zero scale socket, avoid that.
        if ( currentScale == FVector::ZeroVector )
            currentScale = FVector( 1.0f, 1.0f, 1.0f );

        currentSocketTransform.SetLocation( currentPosition );
        currentSocketTransform.SetRotation( currentRotation );
        currentSocketTransform.SetScale3D( currentScale );

        // We want to make sure we're not adding the same socket multiple times
        int32 FoundIx = AllSockets.IndexOfByPredicate(
            [ currentSocketTransform ]( const FTransform InTransform ){ return InTransform.Equals( currentSocketTransform); } );

        if ( FoundIx >= 0 )
        {
            // If the transform, names and actors are identical, skip this duplicate
            if ( ( AllSocketsNames[ FoundIx ] == currentName ) && ( AllSocketsActors[ FoundIx ] == currentActors ) )
                return false;
        }

        AllSockets.Add( currentSocketTransform );
        AllSocketsNames.Add( currentName );
        AllSocketsActors.Add( currentActors );
        AllSocketsTags.Add( currentTag );

        FoundSocketCount++;

        return true;
    };


    // Lambda function for reseting the arrays/attributes
    auto ResetArraysAndAttr = [&]()
    {
        // Position
        Positions.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoPositions, 0 );

        // Rotation
        bHasRotation = false;
        Rotations.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoRotations, 0 );

        // Scale
        bHasScale = false;
        Scales.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoScales, 0 );

        // When using socket groups, we can also get the sockets rotation from the normal
        bHasNormals = false;
        Normals.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNormals, 0 );

        // Socket Name
        bHasNames = false;
        Names.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoNames, 0 );

        // Socket Actor
        bHasActors = false;
        Actors.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoActors, 0 );

        // Socket Tags
        bHasTags = false;
        Tags.Empty();
        FMemory::Memset< HAPI_AttributeInfo >( AttribInfoTags, 0 );
    };

    //-------------------------------------------------------------------------
    // FIND SOCKETS BY DETAIL ATTRIBUTES
    //-------------------------------------------------------------------------

    bool HasSocketAttributes = true;
    int32 SocketIdx = 0;    
    while ( HasSocketAttributes )
    {
        // Build the current socket's prefix
        FString SocketAttrPrefix = TEXT( HAPI_UNREAL_ATTRIB_MESH_SOCKET_PREFIX ) + FString::FromInt( SocketIdx );

        // Reset the arrays and attributes
        ResetArraysAndAttr();

        // Retrieve position data.
        FString SocketPosAttr = SocketAttrPrefix + TEXT("_pos");
        if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
            AssetId, ObjectId, GeoId, PartId,
            TCHAR_TO_ANSI(*SocketPosAttr), AttribInfoPositions, Positions, 0, HAPI_ATTROWNER_DETAIL ) )
            break;

        if ( !AttribInfoPositions.exists )
        {
            // No need to keep looking for socket attributes
            HasSocketAttributes = false;
            break;
        }

        // Retrieve rotation data.
        FString SocketRotAttr = SocketAttrPrefix + TEXT("_rot");
        if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
            AssetId, ObjectId, GeoId, PartId,
            TCHAR_TO_ANSI(*SocketRotAttr), AttribInfoRotations, Rotations, 0, HAPI_ATTROWNER_DETAIL ) )
            bHasRotation = true;

        // Retrieve scale data.
        FString SocketScaleAttr = SocketAttrPrefix + TEXT("_scale");
        if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
            AssetId, ObjectId, GeoId, PartId,
            TCHAR_TO_ANSI(*SocketScaleAttr), AttribInfoScales, Scales, 0, HAPI_ATTROWNER_DETAIL ) )
            bHasScale = true;

        // Retrieve mesh socket names.
        FString SocketNameAttr = SocketAttrPrefix + TEXT("_name");
        if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
            AssetId, ObjectId, GeoId, PartId,
            TCHAR_TO_ANSI(*SocketNameAttr), AttribInfoNames, Names ) )
            bHasNames = true;

        // Retrieve mesh socket actor.
        FString SocketActorAttr = SocketAttrPrefix + TEXT("_actor");
        if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
            AssetId, ObjectId, GeoId, PartId,
            TCHAR_TO_ANSI(*SocketActorAttr), AttribInfoActors, Actors ) )
            bHasActors = true;

        // Retrieve mesh socket tags.
        FString SocketTagAttr = SocketAttrPrefix + TEXT("_tag");
        if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
            AssetId, ObjectId, GeoId, PartId,
            TCHAR_TO_ANSI(*SocketTagAttr), AttribInfoTags, Tags ) )
            bHasTags = true;

        // Add the socket to the array
        AddSocketToArray( 0 );

        // Try to find the next socket
        SocketIdx++;
    }

    //-------------------------------------------------------------------------
    // FIND SOCKETS BY POINT GROUPS
    //-------------------------------------------------------------------------

    // Get object / geo group memberships for primitives.
    TArray< FString > GroupNames;
    if ( !FHoudiniEngineUtils::HapiGetGroupNames(
        AssetId, ObjectId, GeoId, PartId, HAPI_GROUPTYPE_POINT, GroupNames, isPackedPrim ) )
    {
        HOUDINI_LOG_MESSAGE( TEXT( "GetMeshSocketList: Object [%d] non-fatal error reading group names" ), ObjectId );
    }

    // First, we want to make sure we have at least one socket group before continuing
    bool bHasSocketGroup = false;
    for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx )
    {
        const FString & GroupName = GroupNames[ GeoGroupNameIdx ];
        if ( GroupName.StartsWith( TEXT( HAPI_UNREAL_GROUP_MESH_SOCKETS ), ESearchCase::IgnoreCase )
            || GroupName.StartsWith( TEXT( HAPI_UNREAL_GROUP_MESH_SOCKETS_OLD ), ESearchCase::IgnoreCase ))
        {
            bHasSocketGroup = true;
            break;
        }
    }

    if ( !bHasSocketGroup )
        return FoundSocketCount;

    // Reset the data arrays and attributes
    ResetArraysAndAttr();

    // Retrieve position data.
    if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions ) )
        return false;

    // Retrieve rotation data.
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_ROTATION, AttribInfoRotations, Rotations ) )
        bHasRotation = true;

    // Retrieve normal data.
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals ) )
        bHasNormals = true;

    // Retrieve scale data.
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_SCALE, AttribInfoScales, Scales ) )
        bHasScale = true;

    // Retrieve mesh socket names.
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, AttribInfoNames, Names ) )
        bHasNames = true;
    else if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME_OLD, AttribInfoNames, Names ) )
        bHasNames = true;

    // Retrieve mesh socket actor.
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR, AttribInfoActors, Actors ) )
        bHasActors = true;

    // Retrieve mesh socket tags.
    if ( FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        AssetId, ObjectId, GeoId, PartId,
        HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG, AttribInfoTags, Tags ) )
        bHasTags = true;

    // Extracting Sockets vertices
    for ( int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx )
    {
        const FString & GroupName = GroupNames[ GeoGroupNameIdx ];
        if ( !GroupName.StartsWith ( TEXT ( HAPI_UNREAL_GROUP_MESH_SOCKETS ) , ESearchCase::IgnoreCase )
            && !GroupName.StartsWith ( TEXT ( HAPI_UNREAL_GROUP_MESH_SOCKETS_OLD ) , ESearchCase::IgnoreCase ) )
            continue;

        TArray< int32 > PointGroupMembership;
        FHoudiniEngineUtils::HapiGetGroupMembership(
            AssetId, ObjectId, GeoId, PartId, 
            HAPI_GROUPTYPE_POINT, GroupName, PointGroupMembership );

        // Go through all primitives.
        for ( int32 PointIdx = 0; PointIdx < PointGroupMembership.Num(); ++PointIdx )
        {
            if ( PointGroupMembership[ PointIdx ] == 0 )
                continue;

            // Add the corresponding socket to the array
            AddSocketToArray( PointIdx );
        }
    }

    return FoundSocketCount;
}


bool 
FHoudiniEngineUtils::AddMeshSocketsToStaticMesh(
    UStaticMesh* StaticMesh,
    FHoudiniGeoPartObject& HoudiniGeoPartObject,
    TArray< FTransform >& AllSockets,
    TArray< FString >& AllSocketsNames,
    TArray< FString >& AllSocketsActors,
    TArray< FString >& AllSocketsTags )
{
    if ( !StaticMesh )
        return false;

    if ( AllSockets.Num() <= 0 )
        return false;

    // Remove the sockets from the previous cook!
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
            FString SocketName = TEXT("Socket ") + FString::FromInt( nSocket );
            Socket->SocketName = FName( *SocketName );
        }

        // Socket Tag
        FString Tag;
        if ( AllSocketsTags.IsValidIndex( nSocket ) && !AllSocketsTags[ nSocket ].IsEmpty() )
            Tag = AllSocketsTags[ nSocket ];

        // The actor will be store temporarily in the socket's Tag as we need a StaticMeshComponent to add an actor to the socket
        if ( AllSocketsActors.IsValidIndex( nSocket ) && !AllSocketsActors[ nSocket ].IsEmpty() )
            Tag += TEXT("|") + AllSocketsActors[ nSocket ];

        Socket->Tag = Tag;

        StaticMesh->Sockets.Add( Socket );
    }

    // We don't want these new socket to be removed until next cook
    HoudiniGeoPartObject.bHasSocketBeenAdded = true;

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

    // The actor to assign are listed after a |
    TArray<FString> ActorStringArray;
    ActorString.ParseIntoArray( ActorStringArray, TEXT("|"), false );

    // The "real" Tag is the first
    if ( ActorStringArray.Num() > 0 )
        Socket->Tag = ActorStringArray[ 0 ];

    // We just add a Tag, no Actor
    if ( ActorStringArray.Num() == 1 )
        return false;

    // Extract the parsed actor string to split it further
    ActorString = ActorStringArray[ 1 ];

    // Converting the string to a string array using delimiters
    const TCHAR* Delims[] = { TEXT(","), TEXT(";") };
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

    return true;
}

int32
FHoudiniEngineUtils::GetUPropertyAttributeList( const FHoudiniGeoPartObject& GeoPartObject, TArray< UGenericAttribute >& AllUProps )
{
    // Get the detail uprop attributes
    int nUPropsCount = FHoudiniEngineUtils::GetGenericAttributeList( GeoPartObject, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, AllUProps, HAPI_ATTROWNER_DETAIL );

    // Then the primitive uprop attributes
    nUPropsCount += FHoudiniEngineUtils::GetGenericAttributeList( GeoPartObject, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, AllUProps, HAPI_ATTROWNER_PRIM );

    return nUPropsCount;
}

int32
FHoudiniEngineUtils::GetGenericAttributeList(
    const FHoudiniGeoPartObject& GeoPartObject,
    const FString& GenericAttributePrefix,
    TArray< UGenericAttribute >& AllUProps,
    const HAPI_AttributeOwner& AttributeOwner,
    int32 PrimitiveIndex )
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

    // Since generic attributes can be on primitives, we may have to identify if a split occured during the mesh creation.
    // If the primitive index was not specified, we need to a suitable primitive corresponding to that split
    bool HandleSplit = false;
    int PrimIndexForSplit = -1;
    if ( AttributeOwner != HAPI_ATTROWNER_DETAIL )
    {
        if ( PrimitiveIndex != -1 )
        {
            // The index has already been specified so we'll use it
            HandleSplit = true;
            PrimIndexForSplit = PrimitiveIndex;
        }
        else if ( !GeoPartObject.SplitName.IsEmpty() && ( GeoPartObject.SplitName != TEXT("main_geo") ) )
        {
            HandleSplit = true;

            // Since the meshes have been split, we need to find a primitive that belongs to the proper group
            // so we can read the proper value for its generic attribute
            TArray< int32 > PartGroupMembership;
            FHoudiniEngineUtils::HapiGetGroupMembership(
                GeoPartObject.AssetId, GeoPartObject.GetObjectId(), GeoPartObject.GetGeoId(), GeoPartObject.GetPartId(), 
                HAPI_GROUPTYPE_PRIM, GeoPartObject.SplitName, PartGroupMembership );

            for ( int32 n = 0; n < PartGroupMembership.Num(); n++ )
            {
                if ( PartGroupMembership[ n ] > 0 )
                {
                    PrimIndexForSplit = n;
                    break;
                }
            }
        }

        if ( PrimIndexForSplit < 0 )
            PrimIndexForSplit = 0;
    }

    for ( int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx )
    {
        FString HapiString = TEXT("");
        FHoudiniEngineString HoudiniEngineString( AttribNameSHArray[ Idx ] );
        HoudiniEngineString.ToFString( HapiString );

        if ( HapiString.StartsWith( GenericAttributePrefix,  ESearchCase::IgnoreCase ) )
        {
            UGenericAttribute CurrentUProperty;

            // Extract the name of the UProperty from the attribute name
            CurrentUProperty.AttributeName = HapiString.Right( HapiString.Len() - GenericAttributePrefix.Len() );

            // Get the Attribute Info
            HAPI_AttributeInfo AttribInfo;
            FMemory::Memzero< HAPI_AttributeInfo >( AttribInfo );
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
                FHoudiniEngine::Get().GetSession(),
                NodeId, PartId, TCHAR_TO_UTF8( *HapiString ),
                AttributeOwner, &AttribInfo ), false );

            // Get the attribute type and tuple size
            CurrentUProperty.AttributeType = AttribInfo.storage;
            CurrentUProperty.AttributeCount = AttribInfo.count;
            CurrentUProperty.AttributeTupleSize = AttribInfo.tupleSize;     

            if ( CurrentUProperty.AttributeType == HAPI_STORAGETYPE_FLOAT64 )
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
                    CurrentUProperty.GetDoubleTuple( SplitValues, PrimIndexForSplit );

                    CurrentUProperty.DoubleValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.DoubleValues.Add( SplitValues[ n ] );

                    CurrentUProperty.AttributeCount = 1;
                }
            }
            else if ( CurrentUProperty.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_FLOAT )
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
            else if ( CurrentUProperty.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_INT64 )
            {
                // Initialize the value array
                CurrentUProperty.IntValues.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

                // Get the value(s)
                HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInt64Data(
                    FHoudiniEngine::Get().GetSession(),
                    NodeId, PartId, TCHAR_TO_UTF8( *HapiString) ,&AttribInfo,
                    0, CurrentUProperty.IntValues.GetData(), 0, AttribInfo.count ), false );
            }
            else if ( CurrentUProperty.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_INT )
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
            else if ( CurrentUProperty.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
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
                if ( ( CurrentUProperty.AttributeType == HAPI_STORAGETYPE_FLOAT64 )
                    || ( CurrentUProperty.AttributeType == HAPI_STORAGETYPE_FLOAT ) )
                {
                    TArray< double > SplitValues;
                    CurrentUProperty.GetDoubleTuple( SplitValues, PrimIndexForSplit );

                    CurrentUProperty.DoubleValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.DoubleValues.Add( SplitValues[ n ] );
                }
                else if ( ( CurrentUProperty.AttributeType == HAPI_STORAGETYPE_INT64 )
                    || ( CurrentUProperty.AttributeType == HAPI_STORAGETYPE_INT ) )
                {
                    TArray< int64 > SplitValues;
                    CurrentUProperty.GetIntTuple( SplitValues, PrimIndexForSplit );

                    CurrentUProperty.IntValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.IntValues.Add( SplitValues[ n ] );
                }
                else if ( CurrentUProperty.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
                {
                    TArray< FString > SplitValues;
                    CurrentUProperty.GetStringTuple( SplitValues, PrimIndexForSplit );

                    CurrentUProperty.StringValues.Empty();
                    for ( int32 n = 0; n < SplitValues.Num(); n++ )
                        CurrentUProperty.StringValues.Add( SplitValues[ n ] );
                }

                CurrentUProperty.AttributeCount = 1;
            }

            // We can add the UPropertyAttribute to the array
            AllUProps.Add( CurrentUProperty );
            nUPropCount++;
        }
    }

    return nUPropCount;
}

void
FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(
    UObject* MeshComponent, const FHoudiniGeoPartObject& HoudiniGeoPartObject )
{
    if ( !MeshComponent )
        return;

    // Get the list of uproperties to modify from the geopartobject's attributes
    TArray< UGenericAttribute > UPropertiesAttributesToModify;
    if ( !FHoudiniEngineUtils::GetUPropertyAttributeList( HoudiniGeoPartObject, UPropertiesAttributesToModify ) )
        return;
    
    // Iterate over the Found UProperty attributes
    for ( int32 nAttributeIdx = 0; nAttributeIdx < UPropertiesAttributesToModify.Num(); nAttributeIdx++ )
    {
        // Get the current Uproperty Attribute
        UGenericAttribute CurrentPropAttribute = UPropertiesAttributesToModify[ nAttributeIdx ];
        FString CurrentUPropertyName = CurrentPropAttribute.AttributeName;
        if ( CurrentUPropertyName.IsEmpty() )
            continue;

        // Some UProperties need to be modified manually...
        if ( CurrentUPropertyName == "CollisionProfileName" )
        {
            UPrimitiveComponent* PC = Cast< UPrimitiveComponent >( MeshComponent );
            if ( PC )
            {
                FString StringValue = CurrentPropAttribute.GetStringValue();
                FName Value = FName( *StringValue );
                PC->SetCollisionProfileName( Value );

                continue;
            }
        }

        // Handle Component Tags manually here
        if ( CurrentUPropertyName.Contains("Tags") )
        {
            UActorComponent* AC = Cast< UActorComponent >( MeshComponent );
            if ( AC )
            {
                for ( int nIdx = 0; nIdx < CurrentPropAttribute.AttributeCount; nIdx++ )
                {
                    FName NameAttr = FName( *CurrentPropAttribute.GetStringValue( nIdx ) );

                    if ( !AC->ComponentTags.Contains( NameAttr ) )
                        AC->ComponentTags.Add( NameAttr );
                }

                continue;
            }
        }

        // Try to find the corresponding UProperty
        UProperty* FoundProperty = nullptr;
        void* StructContainer = nullptr;
        UObject* FoundPropertyObject = nullptr;

        if ( !FindUPropertyAttributesOnObject( MeshComponent, CurrentPropAttribute, FoundProperty, FoundPropertyObject, StructContainer ) )
            continue;

        if ( !ModifyUPropertyValueOnObject( FoundPropertyObject, CurrentPropAttribute, FoundProperty, StructContainer ) )
            continue;

        // Sucess!
        FString ClassName = MeshComponent->GetClass() ? MeshComponent->GetClass()->GetName() : TEXT("Object");
        FString ObjectName = MeshComponent->GetName();

        // Couldn't find or modify the Uproperty!
        HOUDINI_LOG_MESSAGE( TEXT( "Modified UProperty %s on %s named %s" ), *CurrentUPropertyName, *ClassName, * ObjectName );	
    }
}
/*
bool
FHoudiniEngineUtils::TryToFindInStructProperty(UObject* Object, FString UPropertyNameToFind, UStructProperty* StructProperty, UProperty*& FoundProperty, void*& StructContainer )
{
    if ( !StructProperty || !Object )
        return false;

    // Walk the structs' properties and try to find the one we're looking for
    UScriptStruct* Struct = StructProperty->Struct;
    for (TFieldIterator< UProperty > It(Struct); It; ++It)
    {
        UProperty* Property = *It;
        if ( !Property )
            continue;

        FString DisplayName = It->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
        FString Name = It->GetName();

        // If the property name contains the uprop attribute name, we have a candidate
        if ( Name.Contains( UPropertyNameToFind ) || DisplayName.Contains( UPropertyNameToFind ) )
        {
            // We found the property in the struct property, we need to keep the ValuePtr in the object
            // of the structProp in order to be able to access the property value afterwards...
            FoundProperty = Property;
            StructContainer = StructProperty->ContainerPtrToValuePtr< void >( Object, 0);

            // If it's an equality, we dont need to keep searching
            if ( ( Name == UPropertyNameToFind ) || ( DisplayName == UPropertyNameToFind ) )
                return true;
        }

        if ( FoundProperty )
            continue;

        UStructProperty* NestedStruct = Cast<UStructProperty>( Property );
        if ( NestedStruct && TryToFindInStructProperty( Object, UPropertyNameToFind, NestedStruct, FoundProperty, StructContainer ) )
            return true;

        UArrayProperty* ArrayProp = Cast<UArrayProperty>( Property );
        if ( ArrayProp && TryToFindInArrayProperty( Object, UPropertyNameToFind, ArrayProp, FoundProperty, StructContainer ) )
            return true;
    }

    return false;
}

bool
FHoudiniEngineUtils::TryToFindInArrayProperty(UObject* Object, FString UPropertyNameToFind, UArrayProperty* ArrayProperty, UProperty*& FoundProperty, void*& StructContainer )
{
    if ( !ArrayProperty || !Object )
        return false;

    UProperty* Property = ArrayProperty->Inner;
    if ( !Property )
        return false;

    FString DisplayName = Property->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
    FString Name = Property->GetName();

    // If the property name contains the uprop attribute name, we have a candidate
    if ( Name.Contains( UPropertyNameToFind ) || DisplayName.Contains( UPropertyNameToFind ) )
    {
        // We found the property in the struct property, we need to keep the ValuePtr in the object
        // of the structProp in order to be able to access the property value afterwards...
        FoundProperty = Property;
        StructContainer = ArrayProperty->ContainerPtrToValuePtr< void >( Object, 0);

        // If it's an equality, we dont need to keep searching
        if ( ( Name == UPropertyNameToFind ) || ( DisplayName == UPropertyNameToFind ) )
            return true;
    }

    if ( !FoundProperty )
    {
        UStructProperty* NestedStruct = Cast<UStructProperty>( Property );
        if ( NestedStruct && TryToFindInStructProperty( ArrayProperty, UPropertyNameToFind, NestedStruct, FoundProperty, StructContainer ) )
            return true;

        UArrayProperty* ArrayProp = Cast<UArrayProperty>( Property );
        if ( ArrayProp && TryToFindInArrayProperty( ArrayProperty, UPropertyNameToFind, ArrayProp, FoundProperty, StructContainer ) )
            return true;
    }

    return false;
}
*/


bool FHoudiniEngineUtils::FindUPropertyAttributesOnObject(
    UObject* ParentObject, const UGenericAttribute& UPropertiesToFind,
    UProperty*& FoundProperty, UObject*& FoundPropertyObject, void*& StructContainer )
{
#if WITH_EDITOR
    if ( !ParentObject)
        return false;

    // Get the name of the uprop we're looking for
    FString CurrentUPropertyName = UPropertiesToFind.AttributeName;
    if ( CurrentUPropertyName.IsEmpty() )
        return false;

    UClass* MeshClass = ParentObject->GetClass();

    // Set the result pointer to null
    StructContainer = nullptr;
    FoundProperty = nullptr;

    FoundPropertyObject = ParentObject;

    bool bPropertyHasBeenFound = false;

    /*
    // Try to find the uprop using a TPropValueIterator??
    for ( TPropertyValueIterator<UProperty> It( MeshClass, ParentObject, EPropertyValueIteratorFlags::FullRecursion ); It; ++It )
    {
        UProperty* CurrentProperty = It.Key();
        void* CurrentPropertyValue = (void*)It.Value();
        UObject* ObjectValue = *((UObject**)CurrentPropertyValue);

        FString DisplayName = CurrentProperty->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
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
    }

    if ( bPropertyHasBeenFound )
        return true;
    */

    // Iterate manually on the properties, in order to handle structProperties correctly
    for ( TFieldIterator< UProperty > PropIt( MeshClass, EFieldIteratorFlags::IncludeSuper ); PropIt; ++PropIt )
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

        /*
        // StructProperty need to be a nested struct
        if (UStructProperty* StructProperty = Cast< UStructProperty >(CurrentProperty))
            bPropertyHasBeenFound = TryToFindInStructProperty(MeshComponent, CurrentUPropertyName, StructProperty, FoundProperty, StructContainer);
        else if (UArrayProperty* ArrayProperty = Cast< UArrayProperty >(CurrentProperty))
            bPropertyHasBeenFound = TryToFindInArrayProperty(MeshComponent, CurrentUPropertyName, ArrayProperty, FoundProperty, StructContainer);
        */

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
                    StructContainer = StructProperty->ContainerPtrToValuePtr< void >( ParentObject, 0 );

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

    if ( bPropertyHasBeenFound )
        return true;

    // Try with FindField??
    if ( !FoundProperty )
        FoundProperty = FindField<UProperty>( MeshClass, *CurrentUPropertyName );

    // Try with FindPropertyByName ??
    if ( !FoundProperty )
        FoundProperty = MeshClass->FindPropertyByName( *CurrentUPropertyName );

    // We found the UProperty we were looking for
    if ( FoundProperty )
        return true;

    // Handle common properties nested in classes
    // Static Meshes
    UStaticMesh* SM = Cast< UStaticMesh >( ParentObject );
    if ( SM )
    {
        if ( SM->BodySetup && FindUPropertyAttributesOnObject(
            SM->BodySetup, UPropertiesToFind, FoundProperty, FoundPropertyObject, StructContainer ) )
        {
            return true;
        }

        if ( SM->AssetImportData && FindUPropertyAttributesOnObject(
            SM->AssetImportData, UPropertiesToFind, FoundProperty, FoundPropertyObject, StructContainer ) )
        {
            return true;
        }

        if ( SM->NavCollision && FindUPropertyAttributesOnObject(
            SM->NavCollision, UPropertiesToFind, FoundProperty, FoundPropertyObject, StructContainer ) )
        {
            return true;
        }
    }

    // We found the UProperty we were looking for
    if ( FoundProperty )
        return true;

#endif
    return false;
}

bool FHoudiniEngineUtils::ModifyUPropertyValueOnObject(
    UObject* MeshComponent, UGenericAttribute CurrentPropAttribute,
    UProperty* FoundProperty, void * StructContainer )
{
    if ( !MeshComponent || !FoundProperty )
        return false;

    UProperty* InnerProperty = FoundProperty;
    int32 NumberOfProperties = 1;
    if ( UArrayProperty* ArrayProperty = Cast< UArrayProperty >( FoundProperty ) )
    {
        InnerProperty = ArrayProperty->Inner;
        NumberOfProperties = ArrayProperty->ArrayDim;

        // Do we need to add values to the array?
        FScriptArrayHelper_InContainer ArrayHelper( ArrayProperty, CurrentPropAttribute.GetData() );

        //ArrayHelper.ExpandForIndex( CurrentPropAttribute.AttributeTupleSize - 1 );
        if ( CurrentPropAttribute.AttributeTupleSize > NumberOfProperties )
        {
            ArrayHelper.Resize( CurrentPropAttribute.AttributeTupleSize );
            NumberOfProperties = CurrentPropAttribute.AttributeTupleSize;
        }
    }

    for ( int32 nPropIdx = 0; nPropIdx < NumberOfProperties; nPropIdx++ )
    {
        if ( UFloatProperty* FloatProperty = Cast< UFloatProperty >( InnerProperty ) )
        {
            // FLOAT PROPERTY
            if ( CurrentPropAttribute.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
            {
                FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FString >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    FloatProperty->SetNumericPropertyValueFromString( ValuePtr, *Value );
            }
            else
            {
                double Value = CurrentPropAttribute.GetDoubleValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< float >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< float >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    FloatProperty->SetFloatingPointPropertyValue( ValuePtr, Value );
            }
        }
        else if ( UIntProperty* IntProperty = Cast< UIntProperty >( InnerProperty ) )
        {
            // INT PROPERTY
            if ( CurrentPropAttribute.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
            {
                FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FString >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    IntProperty->SetNumericPropertyValueFromString( ValuePtr, *Value );
            }
            else
            {
                int64 Value = CurrentPropAttribute.GetIntValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< int64 >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< int64 >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    IntProperty->SetIntPropertyValue( ValuePtr, Value );
            }
        }
        else if ( UBoolProperty* BoolProperty = Cast< UBoolProperty >( InnerProperty ) )
        {
            // BOOL PROPERTY
            bool Value = CurrentPropAttribute.GetBoolValue( nPropIdx );
            void * ValuePtr = StructContainer ?
                InnerProperty->ContainerPtrToValuePtr< bool >( (uint8*)StructContainer, nPropIdx )
                : InnerProperty->ContainerPtrToValuePtr< bool >( MeshComponent, nPropIdx );

            if ( ValuePtr )
                BoolProperty->SetPropertyValue( ValuePtr, Value );
        }
        else if ( UStrProperty* StringProperty = Cast< UStrProperty >( InnerProperty ) )
        {
            // STRING PROPERTY
            FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
            void * ValuePtr = StructContainer ?
                InnerProperty->ContainerPtrToValuePtr< FString >( (uint8*)StructContainer, nPropIdx )
                : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx );

            if ( ValuePtr )
                StringProperty->SetPropertyValue( ValuePtr, Value );
        }
        else if ( UNumericProperty *NumericProperty = Cast< UNumericProperty >( InnerProperty ) )
        {
            // NUMERIC PROPERTY
            if ( CurrentPropAttribute.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
            {
                FString Value = CurrentPropAttribute.GetStringValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FString >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FString >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    NumericProperty->SetNumericPropertyValueFromString( ValuePtr, *Value );
            }
            else if ( NumericProperty->IsInteger() )
            {
                int64 Value = CurrentPropAttribute.GetIntValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< int64 >( (uint8*)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< int64 >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    NumericProperty->SetIntPropertyValue(ValuePtr, (int64)Value );
            }
            else if ( NumericProperty->IsFloatingPoint() )
            {
                double Value = CurrentPropAttribute.GetDoubleValue( nPropIdx );
                void * ValuePtr = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< float >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< float >( MeshComponent, nPropIdx );

                if ( ValuePtr )
                    NumericProperty->SetFloatingPointPropertyValue( ValuePtr, Value );
            }
            else
            {
                // Numeric Property was found, but is of an unsupported type
                HOUDINI_LOG_MESSAGE( TEXT("Unsupported Numeric UProperty") );
            }
        }
        else if ( UNameProperty* NameProperty = Cast< UNameProperty >( InnerProperty ) )
        {
            // NAME PROPERTY
            FString StringValue = CurrentPropAttribute.GetStringValue( nPropIdx );
            FName Value = FName( *StringValue );

            void * ValuePtr = StructContainer ?
                InnerProperty->ContainerPtrToValuePtr< FName >( (uint8*)StructContainer, nPropIdx )
                : InnerProperty->ContainerPtrToValuePtr< FName >( MeshComponent, nPropIdx );

            if ( ValuePtr )
                NameProperty->SetPropertyValue( ValuePtr, Value );
        }
        else if ( UStructProperty* StructProperty = Cast< UStructProperty >( InnerProperty ) )
        {
            // STRUCT PROPERTY
            const FName PropertyName = StructProperty->Struct->GetFName();
            if ( PropertyName == NAME_Vector )
            {
                FVector* PropertyValue = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FVector >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FVector >( MeshComponent, nPropIdx );

                if ( PropertyValue )
                {
                    // Found a vector property, fill it with the 3 tuple values
                    PropertyValue->X = CurrentPropAttribute.GetDoubleValue( nPropIdx + 0 );
                    PropertyValue->Y = CurrentPropAttribute.GetDoubleValue( nPropIdx + 1 );
                    PropertyValue->Z = CurrentPropAttribute.GetDoubleValue( nPropIdx + 2 );
                }
            }
            else if ( PropertyName == NAME_Transform )
            {
                FTransform* PropertyValue = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FTransform >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FTransform >( MeshComponent, nPropIdx );

                if ( PropertyValue )
                {
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

                    PropertyValue->SetTranslation( Translation );
                    PropertyValue->SetRotation( Rotation );
                    PropertyValue->SetScale3D( Scale );
                }
            }
            else if ( PropertyName == NAME_Color )
            {
                FColor* PropertyValue = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FColor >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FColor >( MeshComponent, nPropIdx );

                if ( PropertyValue )
                {
                    PropertyValue->R = (int8)CurrentPropAttribute.GetIntValue( nPropIdx );
                    PropertyValue->G = (int8)CurrentPropAttribute.GetIntValue( nPropIdx + 1 );
                    PropertyValue->B = (int8)CurrentPropAttribute.GetIntValue( nPropIdx + 2 );
                    if ( CurrentPropAttribute.AttributeTupleSize == 4 )
                        PropertyValue->A = (int8)CurrentPropAttribute.GetIntValue( nPropIdx + 3 );
                }
            }
            else if ( PropertyName == NAME_LinearColor )
            {
                FLinearColor* PropertyValue = StructContainer ?
                    InnerProperty->ContainerPtrToValuePtr< FLinearColor >( (uint8 *)StructContainer, nPropIdx )
                    : InnerProperty->ContainerPtrToValuePtr< FLinearColor >( MeshComponent, nPropIdx );

                if ( PropertyValue )
                {
                    PropertyValue->R = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx );
                    PropertyValue->G = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 1 );
                    PropertyValue->B = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 2 );
                    if ( CurrentPropAttribute.AttributeTupleSize == 4 )
                        PropertyValue->A = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 3 );
                }
            }
        }
        else
        {
            // Property was found, but is of an unsupported type	    
            FString PropertyClass = FoundProperty->GetClass() ? FoundProperty->GetClass()->GetName() : TEXT("Unknown");
            HOUDINI_LOG_MESSAGE( TEXT("Unsupported UProperty Class: %s found for uproperty %s"), *PropertyClass, *CurrentPropAttribute.AttributeName );
            return false;
        }
    }

    return true;
}

/*
void 
FHoudiniEngineUtils::ApplyUPropertyAttributesOnObject(
    UObject* MeshComponent, const TArray< UGenericAttribute >& UPropertiesToModify )
{
#if WITH_EDITOR
    if ( !MeshComponent )
        return;

    int nUPropsCount = UPropertiesToModify.Num();
    if ( nUPropsCount <= 0 )
        return;

    // MeshComponent should be either a StaticMeshComponent, an InstancedStaticMeshComponent or an InstancedActorComponent
    UStaticMeshComponent* SMC = Cast< UStaticMeshComponent >( MeshComponent );
    UInstancedStaticMeshComponent* ISMC = Cast< UInstancedStaticMeshComponent >( MeshComponent );
    UHierarchicalInstancedStaticMeshComponent* HISMC = Cast< UHierarchicalInstancedStaticMeshComponent >( MeshComponent );
    UHoudiniInstancedActorComponent* IAC = Cast< UHoudiniInstancedActorComponent >( MeshComponent );
    UHoudiniMeshSplitInstancerComponent* MSPIC = Cast<UHoudiniMeshSplitInstancerComponent>(MeshComponent);
    UStaticMesh* SM = Cast< UStaticMesh >( MeshComponent );
    UBodySetup* BS = Cast< UBodySetup >( MeshComponent );
    if ( !SMC && !HISMC && !ISMC && !IAC && !MSPIC && !SM )
        return;

    UClass* MeshClass = IAC ? IAC->StaticClass()
        : HISMC ? HISMC->StaticClass()
        : ISMC ? ISMC->StaticClass()
        : MSPIC ? MSPIC->StaticClass()
        : SMC ? SMC->StaticClass()
        : SM ? SM->StaticClass()
        : BS->StaticClass();

    // Trying to find the UProps in the object 
    for ( int32 nAttributeIdx = 0; nAttributeIdx < nUPropsCount; nAttributeIdx++ )
    {
        UGenericAttribute CurrentPropAttribute = UPropertiesToModify[ nAttributeIdx ];
        FString CurrentUPropertyName = CurrentPropAttribute.AttributeName;
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
            HOUDINI_LOG_MESSAGE( TEXT( "Could not find UProperty: %s"), *( CurrentPropAttribute.AttributeName ) );
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

            //ArrayHelper.ExpandForIndex( CurrentPropAttribute.AttributeTupleSize - 1 );

             if ( CurrentPropAttribute.AttributeTupleSize > NumberOfProperties )
             {
                 ArrayHelper.Resize( CurrentPropAttribute.AttributeTupleSize );
                 NumberOfProperties = CurrentPropAttribute.AttributeTupleSize;
             }
        }

        for( int32 nPropIdx = 0; nPropIdx < NumberOfProperties; nPropIdx++ )
        {
            if ( UFloatProperty* FloatProperty = Cast< UFloatProperty >( InnerProperty ) )
            {
                // FLOAT PROPERTY
                if ( CurrentPropAttribute.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
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
                if ( CurrentPropAttribute.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
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
                if ( CurrentPropAttribute.AttributeType == HAPI_StorageType::HAPI_STORAGETYPE_STRING )
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
                    if ( CurrentPropAttribute.AttributeTupleSize == 4 )
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
                    if ( CurrentPropAttribute.AttributeTupleSize == 4 )
                        PropertyValue.A = (float)CurrentPropAttribute.GetDoubleValue( nPropIdx + 3 );
                }
            }
            else
            {
                // Property was found, but is of an unsupported type
                HOUDINI_LOG_MESSAGE( TEXT("Unsupported UProperty Class: %s found for uproperty %s" ), *PropertyClass->GetName(), *CurrentPropAttribute.AttributeName );
                continue;
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

*/

int32
FHoudiniEngineUtils::HapiGetAttributeOfType(
    const HAPI_NodeId& AssetId,
    const HAPI_NodeId& ObjectId,
    const HAPI_NodeId& GeoId,
    const HAPI_NodeId& PartId,
    const HAPI_AttributeOwner& AttributeOwner,
    const HAPI_AttributeTypeInfo& AttributeType,
    TArray< HAPI_AttributeInfo >& MatchingAttributesInfo,
    TArray< FString >& MatchingAttributesName )
{
    int32 NumberOfAttributeFound = 0;

    // Get the part infos
    HAPI_PartInfo PartInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), 
        GeoId, PartId, &PartInfo ), NumberOfAttributeFound );

    // Get All attribute names for that part
    int32 nAttribCount = PartInfo.attributeCounts[AttributeOwner];

    TArray<HAPI_StringHandle> AttribNameSHArray;
    AttribNameSHArray.SetNum( nAttribCount );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeNames(
        FHoudiniEngine::Get().GetSession(),
        GeoId, PartInfo.id, AttributeOwner,
        AttribNameSHArray.GetData(), nAttribCount ), NumberOfAttributeFound );

    // Iterate on all the attributes, and get their part infos to get their type    
    for ( int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx )
    {
        // Get the name ...
        FString HapiString = TEXT("");
        FHoudiniEngineString HoudiniEngineString( AttribNameSHArray[ Idx ] );
        HoudiniEngineString.ToFString( HapiString );

        // ... then the attribute info
        HAPI_AttributeInfo AttrInfo;
        FMemory::Memzero< HAPI_AttributeInfo >( AttrInfo );

        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo( 
            FHoudiniEngine::Get().GetSession(),
            GeoId, PartInfo.id, TCHAR_TO_UTF8( *HapiString ),
            AttributeOwner, &AttrInfo ) )
            continue;

        if ( !AttrInfo.exists )
            continue;

        // ... check the type
        if ( AttrInfo.typeInfo != AttributeType )
            continue;

        MatchingAttributesInfo.Add( AttrInfo );
        MatchingAttributesName.Add( HapiString );

        NumberOfAttributeFound++;
    }

    return NumberOfAttributeFound;
}

void
FHoudiniEngineUtils::GetAllUVAttributesInfoAndTexCoords( 
    const HAPI_NodeId& AssetId, const HAPI_NodeId& ObjectId,
    const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, 
    TArray< HAPI_AttributeInfo >& AttribInfoUVs,
    TArray< TArray< float > >& TextureCoordinates )
{
    // The second UV set should be called uv2, but we will still check if need to look for a uv1 set.
    // If uv1 exists, we'll look for uv, uv1, uv2 etc.. if not we'll look for uv, uv2, uv3 etc..
    bool bUV1Exists = FHoudiniEngineUtils::HapiCheckAttributeExists(AssetId, ObjectId, GeoId, PartId, "uv1");

    // Retrieve UVs.
    for ( int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx )
    {
        FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

        if ( TexCoordIdx > 0 )
            UVAttributeName += FString::Printf( TEXT("%d"), bUV1Exists ? TexCoordIdx : TexCoordIdx + 1 );

        FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
            AssetId, ObjectId, GeoId, PartId, TCHAR_TO_ANSI(*UVAttributeName),
            AttribInfoUVs[ TexCoordIdx ], TextureCoordinates[ TexCoordIdx ], 2 );
    }

    // Also look for 16.5 uvs (attributes with a Texture type) 
    // For that, we'll have to iterate through ALL the attributes and check their types
    TArray< HAPI_AttributeInfo > FoundAttributeInfos;
    TArray< FString > FoundAttributeNames;
    for ( int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx )
    {
        HapiGetAttributeOfType( AssetId, ObjectId, GeoId, PartId,
            ( HAPI_AttributeOwner ) AttrIdx, HAPI_ATTRIBUTE_TYPE_TEXTURE, 
            FoundAttributeInfos, FoundAttributeNames );
    }

    if ( FoundAttributeInfos.Num() <= 0 )
        return;

    // We found some additionnal uv attributes
    int32 AvailableIdx = 0;
    for( int32 attrIdx = 0; attrIdx < FoundAttributeInfos.Num(); attrIdx++ )
    {
        // Ignore the old uvs
        if ( FoundAttributeNames[ attrIdx ] == TEXT("uv") 
            || FoundAttributeNames[ attrIdx ] == TEXT("uv1")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv2")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv3")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv4")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv5")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv6")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv7")
            || FoundAttributeNames[ attrIdx ] == TEXT("uv8") )
            continue;

        HAPI_AttributeInfo CurrentAttrInfo = FoundAttributeInfos[ attrIdx ];
        if ( !CurrentAttrInfo.exists )
            continue;

        // Look for the next available index in the return arrays
        for ( ; AvailableIdx < AttribInfoUVs.Num(); AvailableIdx++ )
        {
            if ( !AttribInfoUVs[ AvailableIdx ].exists )
                break;
        }

        // We are limited to MAX_STATIC_TEXCOORDS uv sets!
        // If we already have too many uv sets, skip the rest
        if ( ( AvailableIdx >= MAX_STATIC_TEXCOORDS ) || ( AvailableIdx >= AttribInfoUVs.Num() ) )
        {
            HOUDINI_LOG_WARNING( TEXT( "Too many UV sets found. Unreal only supports %d , skipping the remaining uv sets." ), (int32)MAX_STATIC_TEXCOORDS );
            break;
        }
        
        /*
        // We need to add a new attribute info and a new float array for the texcoords
        if ( AvailableIdx > AttribInfoUVs.Num() )
        {
            HAPI_AttributeInfo NewAttrInfo;
            FMemory::MemZero<HAPI_AttributeInfo>( NewAttrInfo );
            AttribInfoUVs.Add( NewAttrInfo );

            TArray< float > NewTexCoords;
            TextureCoordinates.Add( NewTexCoords );
        }
        */

        // Force the tuple size to 2 ?
        CurrentAttrInfo.tupleSize = 2;

        // Add the attribute infos we found
        AttribInfoUVs[ AvailableIdx ] = CurrentAttrInfo;

        // Allocate sufficient buffer for the attribute's data.
        TextureCoordinates[ AvailableIdx ].SetNumUninitialized( CurrentAttrInfo.count * CurrentAttrInfo.tupleSize );

        // Get the texture coordinates
        if ( HAPI_RESULT_SUCCESS !=  FHoudiniApi::GetAttributeFloatData(
            FHoudiniEngine::Get().GetSession(), GeoId, PartId, 
            TCHAR_TO_UTF8( *( FoundAttributeNames[ attrIdx ] ) ),
            &AttribInfoUVs[ AvailableIdx ], -1,
            &TextureCoordinates[ AvailableIdx ][0], 0, CurrentAttrInfo.count ) )
        {
            // Something went wrong when trying to access the uv values, invalidate this set
            AttribInfoUVs[ AvailableIdx ].exists = false;
        }
    }
}

bool
FHoudiniEngineUtils::AddConvexCollisionToAggregate(
    const TArray<float>& Positions, const TArray<int32>& SplitGroupVertexList,
    const bool& MultiHullDecomp, FKAggregateGeom& AggregateCollisionGeo )
{
#if WITH_EDITOR
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

    // We're only interested in the unique vertices
    TArray<int32> UniqueVertexIndexes;
    for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++ )
    {
        int32 Index = SplitGroupVertexList[ VertexIdx ];
        if ( Index < 0 || ( Index >= Positions.Num() ) )
            continue;

        UniqueVertexIndexes.AddUnique( Index );
    }

    // Extract the collision geo's vertices
    TArray< FVector > VertexArray;
    VertexArray.SetNum( UniqueVertexIndexes.Num() );
    for ( int32 Idx = 0; Idx < UniqueVertexIndexes.Num(); Idx++ )
    {
        int32 VertexIndex = UniqueVertexIndexes[ Idx ];

        VertexArray[ Idx ].X = Positions[ VertexIndex * 3 + 0 ] * GeneratedGeometryScaleFactor;
        if ( ImportAxis == HRSAI_Unreal )
        {
            VertexArray[ Idx ].Y = Positions[ VertexIndex * 3 + 2 ] * GeneratedGeometryScaleFactor;
            VertexArray[ Idx ].Z = Positions[ VertexIndex * 3 + 1 ] * GeneratedGeometryScaleFactor;
        }
        else
        {
            VertexArray[ Idx ].Y = Positions[ VertexIndex * 3 + 1 ] * GeneratedGeometryScaleFactor;
            VertexArray[ Idx ].Z = Positions[ VertexIndex * 3 + 2 ] * GeneratedGeometryScaleFactor;
        }
    }

    if ( MultiHullDecomp && ( VertexArray.Num() >= 3 || UniqueVertexIndexes.Num() >= 3 ) )
    {
        // creating multiple convex hull collision
        // ... this might take a while
        // We'll be using Unreal's DecomposeMeshToHulls() so we have to create a fake BodySetup
        UBodySetup* bs = NewObject<UBodySetup>();

        // We're only interested in the valid indices!
        TArray<uint32> Indices;
        for ( int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++ )
        {
            int32 Index = SplitGroupVertexList[ VertexIdx ];
            if ( Index < 0 || ( Index >= Positions.Num() ) )
                continue;

            Indices.Add( Index );
        }

        // But we need all the positions as vertex
        TArray< FVector > Vertices;
        Vertices.SetNum(Positions.Num() / 3);
        for ( int32 Idx = 0; Idx < Vertices.Num(); Idx++ )
        {
            Vertices[ Idx ].X = Positions[ Idx * 3 + 0 ] * GeneratedGeometryScaleFactor;
            if ( ImportAxis == HRSAI_Unreal )
            {
                Vertices[ Idx ].Y = Positions[ Idx * 3 + 2 ] * GeneratedGeometryScaleFactor;
                Vertices[ Idx ].Z = Positions[ Idx * 3 + 1 ] * GeneratedGeometryScaleFactor;
            }
            else
            {
                Vertices[ Idx ].Y = Positions[ Idx * 3 + 1 ] * GeneratedGeometryScaleFactor;
                Vertices[ Idx ].Z = Positions[ Idx * 3 + 2 ] * GeneratedGeometryScaleFactor;
            }
        }

        // Run actual util to do the work (if we have some valid input)
        DecomposeMeshToHulls( bs, Vertices, Indices, 8, 16 );

        // If we succeed, return here
        // If not, keep going and we'll try to do a single hull decomposition
        if ( bs->AggGeom.ConvexElems.Num() > 0 )
        {
            // Copy the convex elem to our aggregate
            for ( int32 n = 0; n < bs->AggGeom.ConvexElems.Num(); n++ )
                AggregateCollisionGeo.ConvexElems.Add( bs->AggGeom.ConvexElems[ n ] );

            return true;
        }
    }

    // Creating a single Convex collision
    FKConvexElem ConvexCollision;
    ConvexCollision.VertexData = VertexArray;
    ConvexCollision.UpdateElemBox();

    AggregateCollisionGeo.ConvexElems.Add( ConvexCollision );
#endif
    return true;
}

bool
FHoudiniEngineUtils::AddSimpleCollision(
    const FString& SplitGroupName, UStaticMesh* StaticMesh,
    FHoudiniGeoPartObject& HoudiniGeoPartObject, FKAggregateGeom& AggregateCollisionGeo, bool& bSimpleCollisionAddedToAggregate )

{
#if WITH_EDITOR
    int32 PrimIndex = INDEX_NONE;
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

    // We somehow couldnt create the simple collider
    if ( PrimIndex == INDEX_NONE )
    {
        HoudiniGeoPartObject.bIsSimpleCollisionGeo = false;
        HoudiniGeoPartObject.bIsCollidable = false;
        HoudiniGeoPartObject.bIsRenderCollidable = false;

        return false;
    }
#endif
    return true;
}


void
FHoudiniEngineUtils::CreateSlateNotification( const FString& NotificationString )
{
#if WITH_EDITOR
    // Check whether we want to display Slate notifications.
    bool bDisplaySlateCookingNotifications = true;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (HoudiniRuntimeSettings)
        bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

    if (!bDisplaySlateCookingNotifications)
        return;

    static float NotificationFadeOutDuration = 2.0f;
    static float NotificationExpireDuration = 2.0f;

    FText NotificationText = FText::FromString(NotificationString);
    FNotificationInfo Info(NotificationText);

    Info.bFireAndForget = true;
    Info.FadeOutDuration = NotificationFadeOutDuration;
    Info.ExpireDuration = NotificationExpireDuration;

    TSharedPtr< FSlateDynamicImageBrush > HoudiniBrush = FHoudiniEngine::Get().GetHoudiniLogoBrush();
    if (HoudiniBrush.IsValid())
        Info.Image = HoudiniBrush.Get();

    FSlateNotificationManager::Get().AddNotification(Info);
#endif

    return;
}

HAPI_NodeId
FHoudiniEngineUtils::HapiGetParentNodeId( const HAPI_NodeId& NodeId )
{
    HAPI_NodeId ParentId = -1;
    if ( NodeId >= 0 )
    {
        HAPI_NodeInfo NodeInfo;
        if ( HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInfo( FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo ) )
            ParentId = NodeInfo.parentId;
    }

    return ParentId;
}

FHoudiniCookParams::FHoudiniCookParams( class UHoudiniAsset* InHoudiniAsset )
: HoudiniAsset( InHoudiniAsset )
{
    PackageGUID = FGuid::NewGuid();
    TempCookFolder = LOCTEXT( "Temp", "/Game/HoudiniEngine/Temp" );
}

#undef LOCTEXT_NAMESPACE
