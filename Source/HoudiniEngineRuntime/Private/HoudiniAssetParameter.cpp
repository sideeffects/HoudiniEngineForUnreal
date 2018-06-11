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
#include "HoudiniAssetParameter.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniAssetParameterRamp.h"
#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniEngineString.h"

#include "Internationalization/Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

uint32
GetTypeHash( const UHoudiniAssetParameter * HoudiniAssetParameter )
{
    if ( HoudiniAssetParameter )
        return HoudiniAssetParameter->GetTypeHash();

    return 0;
}

UHoudiniAssetParameter::UHoudiniAssetParameter( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , PrimaryObject( nullptr )
    , ParentParameter( nullptr )
    , NodeId( -1 )
    , ParmId( -1 )
    , ParmParentId( -1 )
    , ChildIndex( 0 )
    , TupleSize( 1 )
    , ValuesIndex( -1 )
    , MultiparmInstanceIndex( -1 )
    , ActiveChildParameter( 0 )
    , HoudiniAssetParameterFlagsPacked( 0u )
    , HoudiniAssetParameterVersion( VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE )
{
    ParameterName = TEXT( "" );
    ParameterLabel = TEXT( "" );
    ParameterHelp = TEXT( "" );
}

bool
UHoudiniAssetParameter::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    // We need to reset child parameters.
    ResetChildParameters();

    // If parameter has changed, we do not need to recreate it.
    if ( bChanged )
        return false;

    // If parameter is invisible, we cannot create it.
    if ( !IsVisible( ParmInfo ) )
        return false;

    // Set name and label.
    if ( !SetNameAndLabel( ParmInfo ) )
        return false;

    // Set the help
    if ( !SetHelp( ParmInfo ) )
        return false;

    // If it is a Substance parameter, mark it as such.
    bIsSubstanceParameter = ParameterName.StartsWith( HAPI_UNREAL_PARAM_SUBSTANCE_PREFIX );

    // Set ids.
    SetNodeParmIds( InNodeId, ParmInfo.id );

    // Set parent id.
    ParmParentId = ParmInfo.parentId;

    // Set the index within parent.
    ChildIndex = ParmInfo.childIndex;

    // Set tuple count.
    TupleSize = ParmInfo.size;

    // Set the multiparm instance index.
    MultiparmInstanceIndex = ParmInfo.instanceNum;

    // Set spare flag.
    bIsSpare = ParmInfo.spare;

    // Set disabled flag.
    bIsDisabled = ParmInfo.disabled;

    // Set child of multiparm flag.
    bIsChildOfMultiparm = ParmInfo.isChildOfMultiParm;

    // Set ismultiparmflag
    bIsMultiparm = ( ParmInfo.type == HAPI_PARMTYPE_MULTIPARMLIST );

    // Set component.
    PrimaryObject = InPrimaryObject;

    // Store parameter parent.
    ParentParameter = InParentParameter;

    return true;
}

UHoudiniAssetParameter * 
UHoudiniAssetParameter::Duplicate( UObject* InOuter )
{
    return DuplicateObject<UHoudiniAssetParameter>(this, InOuter );
}

void
UHoudiniAssetParameter::NotifyChildParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter )
{
    // Default implementation does nothing.
}

void
UHoudiniAssetParameter::NotifyChildParametersCreated()
{
    // Default implementation does nothing.
}

bool
UHoudiniAssetParameter::UploadParameterValue()
{
    // Mark this parameter as no longer changed.
    bChanged = false;
    return true;
}

bool
UHoudiniAssetParameter::SetParameterVariantValue( const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    // Default implementation does nothing.
    return false;
}

bool
UHoudiniAssetParameter::HasChanged() const
{
    return bChanged;
}

void
UHoudiniAssetParameter::SetHoudiniAssetComponent( UHoudiniAssetComponent * InComponent )
{
    PrimaryObject = InComponent;
}

void
UHoudiniAssetParameter::SetParentParameter( UHoudiniAssetParameter * InParentParameter )
{
    if ( ParentParameter != InParentParameter )
    {
        ParentParameter = InParentParameter;

        if ( ParentParameter )
        {
            // Retrieve parent parameter id. We ignore folder lists, they are artificial parents created by us.
            ParmParentId = ParentParameter->GetParmId();

            // Add this parameter to parent collection of child parameters.
            ParentParameter->AddChildParameter( this );
        }
        else
        {
            // Reset parent parm id.
            ParmParentId = -1;
        }
    }
}

bool
UHoudiniAssetParameter::IsChildParameter() const
{
    return ParentParameter != nullptr;
}

void
UHoudiniAssetParameter::AddChildParameter( UHoudiniAssetParameter * HoudiniAssetParameter )
{
    if ( HoudiniAssetParameter )
        ChildParameters.Add( HoudiniAssetParameter );
}

uint32
UHoudiniAssetParameter::GetTypeHash() const
{
    // We do hashing based on parameter name.
    return ::GetTypeHash( ParameterName );
}

HAPI_ParmId
UHoudiniAssetParameter::GetParmId() const
{
    return ParmId;
}

HAPI_ParmId
UHoudiniAssetParameter::GetParmParentId() const
{
    return ParmParentId;
}

void
UHoudiniAssetParameter::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetParameter * HoudiniAssetParameter = Cast< UHoudiniAssetParameter >( InThis );
    if ( HoudiniAssetParameter )
    {
        if ( HoudiniAssetParameter->PrimaryObject )
            Collector.AddReferencedObject( HoudiniAssetParameter->PrimaryObject, InThis );
    }

    // Call base implementation.
    Super::AddReferencedObjects( InThis, Collector );
}

void
UHoudiniAssetParameter::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    HoudiniAssetParameterVersion = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
    Ar << HoudiniAssetParameterVersion;

    Ar << HoudiniAssetParameterFlagsPacked;

    if ( Ar.IsLoading() )
        bChanged = false;

    Ar << ParameterName;
    Ar << ParameterLabel;

    Ar << NodeId;
    if( !Ar.IsTransacting() && Ar.IsLoading() )
    {
        // NodeId is invalid after load
        NodeId = -1;
    }
    Ar << ParmId;

    Ar << ParmParentId;
    Ar << ChildIndex;

    Ar << TupleSize;
    Ar << ValuesIndex;
    Ar << MultiparmInstanceIndex;

    if( HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_ASSET_INSTANCE_MEMBER )
    {
        UObject* Dummy = nullptr;
        Ar << Dummy;
    }

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_PARAM_HELP )
    {
        Ar << ParameterHelp;
    }
    else
    {
        ParameterHelp = TEXT( "" );
    }

    if ( Ar.IsTransacting() )
    {
        Ar << PrimaryObject;
        Ar << ParentParameter;
    }
}

#if WITH_EDITOR

void
UHoudiniAssetParameter::PostEditUndo()
{
    Super::PostEditUndo();

    MarkPreChanged();
    MarkChanged();
}

#endif // WITH_EDITOR

void
UHoudiniAssetParameter::SetNodeParmIds( HAPI_NodeId InNodeId, HAPI_ParmId InParmId )
{
    NodeId = InNodeId;
    ParmId = InParmId;
}

bool
UHoudiniAssetParameter::HasValidNodeParmIds() const
{
    return ( NodeId != -1 ) && ( ParmId != -1 );
}

bool
UHoudiniAssetParameter::IsVisible( const HAPI_ParmInfo & ParmInfo ) const
{
    return ParmInfo.invisible == false;
}

bool
UHoudiniAssetParameter::SetNameAndLabel( const HAPI_ParmInfo & ParmInfo )
{
    FHoudiniEngineString HoudiniEngineStringName( ParmInfo.nameSH );
    FHoudiniEngineString HoudiniEngineStringLabel( ParmInfo.labelSH );

    bool bresult = true;

    bresult |= HoudiniEngineStringName.ToFString( ParameterName );
    bresult |= HoudiniEngineStringLabel.ToFString( ParameterLabel );

    return bresult;
}

bool
UHoudiniAssetParameter::SetNameAndLabel( HAPI_StringHandle StringHandle )
{
    FHoudiniEngineString HoudiniEngineString( StringHandle );

    bool bresult = true;

    bresult |= HoudiniEngineString.ToFString( ParameterName );
    bresult |= HoudiniEngineString.ToFString( ParameterLabel );

    return bresult;
}

bool
UHoudiniAssetParameter::SetNameAndLabel( const FString & Name )
{
    ParameterName = Name;
    ParameterLabel = Name;

    return true;
}

bool
UHoudiniAssetParameter::SetHelp( const HAPI_ParmInfo & ParmInfo )
{
    FHoudiniEngineString HoudiniEngineStringHelp( ParmInfo.helpSH );

    bool bresult = true;
    bresult = HoudiniEngineStringHelp.ToFString( ParameterHelp );

    return bresult;
}

void
UHoudiniAssetParameter::MarkPreChanged( bool bMarkAndTriggerUpdate )
{
}

void
UHoudiniAssetParameter::MarkChanged( bool bMarkAndTriggerUpdate )
{
    // Set changed flag.
    bChanged = true;

#if WITH_EDITOR

    // Notify component about change.
    if( bMarkAndTriggerUpdate )
    {
        if( UHoudiniAssetComponent* Component = Cast<UHoudiniAssetComponent>(PrimaryObject) )
            Component->NotifyParameterChanged( this );

        // Notify parent parameter about change.
        if( ParentParameter )
            ParentParameter->NotifyChildParameterChanged( this );
    }

#endif // WITH_EDITOR
}

void
UHoudiniAssetParameter::UnmarkChanged()
{
    bChanged = false;
}

void
UHoudiniAssetParameter::ResetChildParameters()
{
    ChildParameters.Empty();
}

int32
UHoudiniAssetParameter::GetTupleSize() const
{
    return TupleSize;
}

bool
UHoudiniAssetParameter::IsArray() const
{
    return TupleSize > 1;
}

void
UHoudiniAssetParameter::SetValuesIndex( int32 InValuesIndex )
{
    ValuesIndex = InValuesIndex;
}

int32
UHoudiniAssetParameter::GetActiveChildParameter() const
{
    return ActiveChildParameter;
}

void UHoudiniAssetParameter::OnParamStateChanged()
{
#if WITH_EDITOR
    if( UHoudiniAssetComponent* Comp = Cast<UHoudiniAssetComponent>( PrimaryObject ) )
    {
        Comp->UpdateEditorProperties( false );
    }
#endif
}

bool
UHoudiniAssetParameter::HasChildParameters() const
{
    return ChildParameters.Num() > 0;
}

bool
UHoudiniAssetParameter::IsSubstanceParameter() const
{
    return bIsSubstanceParameter;
}

bool
UHoudiniAssetParameter::IsActiveChildParameter( UHoudiniAssetParameter * ChildParam ) const
{
    if ( ChildParam && ( ActiveChildParameter < ChildParameters.Num() ) )
        return ChildParameters[ ActiveChildParameter ] == ChildParam;

    return false;
}

UHoudiniAssetParameter *
UHoudiniAssetParameter::GetParentParameter() const
{
    return ParentParameter;
}

const FString &
UHoudiniAssetParameter::GetParameterName() const
{
    return ParameterName;
}

const FString &
UHoudiniAssetParameter::GetParameterLabel() const
{
    return ParameterLabel;
}

const FString &
UHoudiniAssetParameter::GetParameterHelp() const
{
    return ParameterHelp;
}


void
UHoudiniAssetParameter::SetNodeId( HAPI_NodeId InNodeId )
{
    NodeId = InNodeId;
}

bool
UHoudiniAssetParameter::IsSpare() const
{
    return bIsSpare;
}

bool
UHoudiniAssetParameter::IsDisabled() const
{
    return bIsDisabled;
}

#undef LOCTEXT_NAMESPACE