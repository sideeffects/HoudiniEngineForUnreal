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
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterButton.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterLabel.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniAssetParameterRamp.h"
#include "HoudiniAssetParameterSeparator.h"
#include "HoudiniAssetParameterString.h"
#include "HoudiniAssetParameterToggle.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniParamUtils.h"
#include "HoudiniRuntimeSettings.h"


bool 
FHoudiniParamUtils::Build( HAPI_NodeId AssetId, class UObject* PrimaryObject,
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * >& CurrentParameters,
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * >& NewParameters )
{
    if( !FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        // There's no Houdini asset, we can return.
        return false;
    }

    bool bTreatRampParametersAsMultiparms = false;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if( HoudiniRuntimeSettings )
        bTreatRampParametersAsMultiparms = HoudiniRuntimeSettings->bTreatRampParametersAsMultiparms;

    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    HAPI_NodeInfo NodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &NodeInfo ), false );

    if( NodeInfo.parmCount > 0 )
    {
        // Retrieve parameters.
        TArray< HAPI_ParmInfo > ParmInfos;
        ParmInfos.SetNumUninitialized( NodeInfo.parmCount );
        HOUDINI_CHECK_ERROR_RETURN(
            FHoudiniApi::GetParameters(
                FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &ParmInfos[ 0 ], 0,
                NodeInfo.parmCount ), false );

        // Create name lookup cache
        TMap<FString, UHoudiniAssetParameter*> CurrentParametersByName;
        CurrentParametersByName.Reserve( CurrentParameters.Num() );
        for( auto& ParmPair : CurrentParameters )
        {
            CurrentParametersByName.Add( ParmPair.Value->GetParameterName(), ParmPair.Value );
        }

        // Create properties for parameters.
        for( int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx )
        {
            // Retrieve param info at this index.
            const HAPI_ParmInfo & ParmInfo = ParmInfos[ ParamIdx ];

            // If the parameter is corrupt, skip it
            if( ParmInfo.id < 0 || ParmInfo.childIndex < 0 )
            {
                HOUDINI_LOG_WARNING( TEXT( "Corrupt parameter %d detected, skipping.  Note: Plug-in does not support nested Multiparm parameters" ), ParamIdx );
                continue;
            }

            // If parameter is invisible, skip it.
            if( ParmInfo.invisible )
                continue;

            // Check if any parent folder of this parameter is invisible 
            bool SkipParm = false;
            HAPI_ParmId ParentId = ParmInfo.parentId;
            while( ParentId > 0 && !SkipParm )
            {
                if( const HAPI_ParmInfo* ParentInfoPtr = ParmInfos.FindByPredicate( [=]( const HAPI_ParmInfo& Info ) {
                    return Info.id == ParentId;
                } ) )
                {
                    if( ParentInfoPtr->invisible && ParentInfoPtr->type == HAPI_PARMTYPE_FOLDER )
                        SkipParm = true;
                    ParentId = ParentInfoPtr->parentId;
                }
                else
                {
                    HOUDINI_LOG_ERROR( TEXT( "Could not find parent of parameter %d" ), ParmInfo.id );
                    SkipParm = true;
                }
            }

            if( SkipParm )
                continue;

            UHoudiniAssetParameter * HoudiniAssetParameter = nullptr;

            // See if this parameter has already been created.
            // We can't use HAPI_ParmId because that is not unique to parameter instances, so instead
            // we find the existing parameter by name
            FString NewParmName;
            FHoudiniEngineString( ParmInfo.nameSH ).ToFString( NewParmName );
            UHoudiniAssetParameter ** FoundHoudiniAssetParameter = CurrentParametersByName.Find( NewParmName );

            // If parameter exists, we can reuse it.
            if( FoundHoudiniAssetParameter && *FoundHoudiniAssetParameter )
            {
                // sanity check that type and tuple size hasn't changed
                if( (*FoundHoudiniAssetParameter)->GetTupleSize() == ParmInfo.size )
                {
                    UClass* FoundClass = (*FoundHoudiniAssetParameter)->GetClass();
                    bool FailedTypeCheck = true;
                    switch( ParmInfo.type )
                    {
                        case HAPI_PARMTYPE_STRING:
                            if( !ParmInfo.choiceCount )
                                FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniAssetParameterString >();
                            else
                                FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniAssetParameterChoice >();
                            break;
                        case HAPI_PARMTYPE_INT:
                            if( !ParmInfo.choiceCount )
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterInt>();
                            else
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterChoice>();
                            break;
                        case HAPI_PARMTYPE_FLOAT:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFloat>();
                            break;
                        case HAPI_PARMTYPE_TOGGLE:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterToggle>();
                            break;
                        case HAPI_PARMTYPE_COLOR:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterColor>();
                            break;
                        case HAPI_PARMTYPE_LABEL:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterLabel>();
                            break;
                        case HAPI_PARMTYPE_BUTTON:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterButton>();
                            break;
                        case HAPI_PARMTYPE_SEPARATOR:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterSeparator>();
                            break;
                        case HAPI_PARMTYPE_FOLDERLIST:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFolderList>();
                            break;
                        case HAPI_PARMTYPE_FOLDER:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFolder>();
                            break;
                        case HAPI_PARMTYPE_MULTIPARMLIST:
                            if( !bTreatRampParametersAsMultiparms && ( HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType ||
                                HAPI_RAMPTYPE_COLOR == ParmInfo.rampType ) )
                            {
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterRamp>();
                            }
                            else
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterMultiparm>();
                            break;
                        case HAPI_PARMTYPE_PATH_FILE:
                        case HAPI_PARMTYPE_PATH_FILE_DIR:
                        case HAPI_PARMTYPE_PATH_FILE_GEO:
                        case HAPI_PARMTYPE_PATH_FILE_IMAGE:
                            FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFile>();
                            break;
                        case HAPI_PARMTYPE_NODE:
                            if( ParmInfo.inputNodeType == HAPI_NODETYPE_ANY ||
                                ParmInfo.inputNodeType == HAPI_NODETYPE_SOP ||
                                ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ )
                            {
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetInput>();
                            }
                            else
                            {
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterString>();
                            }
                            break;
                    }

                    if( !FailedTypeCheck )
                    {
                        HoudiniAssetParameter = *FoundHoudiniAssetParameter;

                        // Transfer param object from current map to new map
                        CurrentParameters.Remove( HoudiniAssetParameter->GetParmId() );
                        CurrentParametersByName.Remove( NewParmName );

                        // Reinitialize parameter and add it to map.
                        HoudiniAssetParameter->CreateParameter( PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                        NewParameters.Add( ParmInfo.id, HoudiniAssetParameter );
                        continue;
                    }
                }
            }

            switch( ParmInfo.type )
            {
                case HAPI_PARMTYPE_STRING:
                {
                    if( !ParmInfo.choiceCount )
                        HoudiniAssetParameter = UHoudiniAssetParameterString::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    else
                        HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );

                    break;
                }

                case HAPI_PARMTYPE_INT:
                {
                    if( !ParmInfo.choiceCount )
                        HoudiniAssetParameter = UHoudiniAssetParameterInt::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    else
                        HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );

                    break;
                }

                case HAPI_PARMTYPE_FLOAT:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFloat::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_TOGGLE:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_COLOR:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterColor::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_LABEL:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterLabel::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_BUTTON:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterButton::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_SEPARATOR:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterSeparator::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_FOLDERLIST:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFolderList::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_FOLDER:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFolder::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_MULTIPARMLIST:
                {
                    if( !bTreatRampParametersAsMultiparms && ( HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType ||
                        HAPI_RAMPTYPE_COLOR == ParmInfo.rampType ) )
                    {
                        HoudiniAssetParameter = UHoudiniAssetParameterRamp::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    }
                    else
                    {
                        HoudiniAssetParameter = UHoudiniAssetParameterMultiparm::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    }

                    break;
                }

                case HAPI_PARMTYPE_PATH_FILE:
                case HAPI_PARMTYPE_PATH_FILE_DIR:
                case HAPI_PARMTYPE_PATH_FILE_GEO:
                case HAPI_PARMTYPE_PATH_FILE_IMAGE:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFile::Create(
                        PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_NODE:
                {
                    if( ParmInfo.inputNodeType == HAPI_NODETYPE_ANY ||
                        ParmInfo.inputNodeType == HAPI_NODETYPE_SOP ||
                        ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ )
                    {
                        HoudiniAssetParameter = UHoudiniAssetInput::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    }
                    else
                    {
                        HoudiniAssetParameter = UHoudiniAssetParameterString::Create(
                            PrimaryObject, nullptr, AssetInfo.nodeId, ParmInfo );
                    }
                    break;
                }

                default:
                {
                    // Just ignore unsupported types for now.
                    HOUDINI_LOG_WARNING( TEXT( "Parameter Type (%d) is unsupported" ), static_cast<int32>( ParmInfo.type ) );
                    continue;
                }
            }
            check ( HoudiniAssetParameter );
            
            // Add this parameter to the map.
            NewParameters.Add( ParmInfo.id, HoudiniAssetParameter );
        }

        // We a pass over the new params to patch parent links.
        for( int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx )
        {
            // Retrieve param info at this index.
            const HAPI_ParmInfo & ParmInfo = ParmInfos[ ParamIdx ];

            // Locate corresponding parameter.
            UHoudiniAssetParameter * const * FoundHoudiniAssetParameter = NewParameters.Find( ParmInfo.id );
            UHoudiniAssetParameter * HoudiniAssetParentParameter = nullptr;

            if( FoundHoudiniAssetParameter )
            {
                UHoudiniAssetParameter * HoudiniAssetParameter = *FoundHoudiniAssetParameter;

                // Get parent parm id.
                HAPI_ParmId ParentParmId = HoudiniAssetParameter->GetParmParentId();
                if( ParentParmId != -1 )
                {
                    // Locate corresponding parent parameter.
                    UHoudiniAssetParameter * const * FoundHoudiniAssetParentParameter = NewParameters.Find( ParentParmId );
                    if( FoundHoudiniAssetParentParameter )
                        HoudiniAssetParentParameter = *FoundHoudiniAssetParentParameter;
                }

                // Set parent for this parameter.
                HoudiniAssetParameter->SetParentParameter( HoudiniAssetParentParameter );

                if( ParmInfo.type == HAPI_PARMTYPE_FOLDERLIST )
                {
                    // For folder lists we need to add children manually.
                    HoudiniAssetParameter->ResetChildParameters();

                    for( int32 ChildIdx = 0; ChildIdx < ParmInfo.size; ++ChildIdx )
                    {
                        // Children folder parm infos come after folder list parm info.
                        const HAPI_ParmInfo & ChildParmInfo = ParmInfos[ ParamIdx + ChildIdx + 1 ];

                        UHoudiniAssetParameter * const * FoundHoudiniAssetParameterChild =
                            NewParameters.Find( ChildParmInfo.id );

                        if( FoundHoudiniAssetParameterChild )
                        {
                            UHoudiniAssetParameter * HoudiniAssetParameterChild = *FoundHoudiniAssetParameterChild;
                            HoudiniAssetParameterChild->SetParentParameter( HoudiniAssetParameter );
                        }
                    }
                }
            }
        }

        // Another pass to notify parameters that all children parameters have been assigned
        for( auto& NewParamPair : NewParameters )
        {
            if( NewParamPair.Value->HasChildParameters() )
                NewParamPair.Value->NotifyChildParametersCreated();
        }
    }
    return true;
}


