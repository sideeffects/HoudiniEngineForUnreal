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
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"

#include "Internationalization/Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetParameterFloat::UHoudiniAssetParameterFloat( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , ValueMin( TNumericLimits< float >::Lowest() )
    , ValueMax( TNumericLimits< float >::Max() )
    , ValueUIMin( TNumericLimits< float >::Lowest() )
    , ValueUIMax( TNumericLimits< float >::Max() )
    , ValueUnit ( TEXT("") )
    , NoSwap ( false )
{
    // Parameter will have at least one value.
    Values.AddZeroed( 1 );
}

void
UHoudiniAssetParameterFloat::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << Values;

    Ar << ValueMin;
    Ar << ValueMax;

    Ar << ValueUIMin;
    Ar << ValueUIMax;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_UNIT )
        Ar << ValueUnit;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_NOSWAP )
        Ar << NoSwap;
}

UHoudiniAssetParameterFloat *
UHoudiniAssetParameterFloat::Create(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    UObject * Outer = InPrimaryObject;
    if ( !Outer )
    {
        Outer = InParentParameter;
        if ( !Outer )
        {
            // Must have either component or parent not null.
            check( false );
        }
    }

    UHoudiniAssetParameterFloat * HoudiniAssetParameterFloat = NewObject< UHoudiniAssetParameterFloat >(
        Outer, UHoudiniAssetParameterFloat::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterFloat->CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterFloat;
}

bool
UHoudiniAssetParameterFloat::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle float type.
    if ( ParmInfo.type != HAPI_PARMTYPE_FLOAT )
    {
        return false;
    }

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.floatValuesIndex );

    if ( TupleSize <= 0 )
        return false;
    Values.SetNumZeroed(TupleSize);

    // Get the actual value for this property.
    if ( FHoudiniApi::GetParmFloatValues(
        FHoudiniEngine::Get().GetSession(), InNodeId, &Values[ 0 ],
        ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    // Set min and max for this property.
    if ( ParmInfo.hasMin )
        ValueMin = ParmInfo.min;

    if ( ParmInfo.hasMax )
        ValueMax = ParmInfo.max;

    bool bUsesDefaultMinMax = false;

    // Set min and max for UI for this property.
    if ( ParmInfo.hasUIMin )
    {
        ValueUIMin = ParmInfo.UIMin;
    }
    else
    {
        // If it is not set, use supplied min.
        if ( ParmInfo.hasMin )
        {
            ValueUIMin = ValueMin;
        }
        else
        {
            // Min value Houdini uses by default.
            ValueUIMin = HAPI_UNREAL_PARAM_FLOAT_UI_MIN;
            bUsesDefaultMinMax = true;
        }
    }

    if ( ParmInfo.hasUIMax )
    {
        ValueUIMax = ParmInfo.UIMax;
    }
    else
    {
        // If it is not set, use supplied max.
        if ( ParmInfo.hasMax )
        {
            ValueUIMax = ValueMax;
        }
        else
        {
            // Max value Houdini uses by default.
            ValueUIMax = HAPI_UNREAL_PARAM_FLOAT_UI_MAX;
            bUsesDefaultMinMax = true;
        }
    }

    if ( bUsesDefaultMinMax )
    {
        // If we are using defaults, we can detect some most common parameter names and alter defaults.

        FString LocalParameterName = TEXT( "" );
        FHoudiniEngineString HoudiniEngineString( ParmInfo.nameSH );
        HoudiniEngineString.ToFString( LocalParameterName );

        static const FString ParameterNameTranslate( TEXT( HAPI_UNREAL_PARAM_TRANSLATE ) );
        static const FString ParameterNameRotate( TEXT( HAPI_UNREAL_PARAM_ROTATE ) );
        static const FString ParameterNameScale( TEXT( HAPI_UNREAL_PARAM_SCALE ) );
        static const FString ParameterNamePivot( TEXT( HAPI_UNREAL_PARAM_PIVOT ) );

        if ( !LocalParameterName.IsEmpty() )
        {
            if ( LocalParameterName.Equals( ParameterNameTranslate )
                || LocalParameterName.Equals( ParameterNameScale )
                || LocalParameterName.Equals( ParameterNamePivot ) )
            {
                ValueUIMin = -1.0f;
                ValueUIMax = 1.0f;
            }
            else if ( LocalParameterName.Equals( ParameterNameRotate ) )
            {
                ValueUIMin = 0.0f;
                ValueUIMax = 360.0f;
            }
        }
    }

    // Get this parameter's unit if it has one
    FHoudiniEngineUtils::HapiGetParameterUnit( InNodeId, ParmId, ValueUnit );

    // Get this parameter's no swap tag if it has one
    FHoudiniEngineUtils::HapiGetParameterNoSwapTag( InNodeId, ParmId, NoSwap );

    return true;
}

bool
UHoudiniAssetParameterFloat::UploadParameterValue()
{
    if ( FHoudiniApi::SetParmFloatValues( FHoudiniEngine::Get().GetSession(), NodeId, &Values[ 0 ],
        ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterFloat::SetParameterVariantValue( const FVariant & Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    EVariantTypes VariantType = Variant.GetType();
    float VariantValue = 0.0f;

    if ( Idx >= 0 && Idx < Values.Num() )
        return false;

    if ( VariantType == EVariantTypes::Float )
        VariantValue = Variant.GetValue<float>();
    else if ( VariantType == EVariantTypes::Double )
        VariantValue = (float) Variant.GetValue< double >();
    else
        return false;

#if WITH_EDITOR

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterFloatChange", "Houdini Parameter Float: Changing a value"),
        PrimaryObject );

    Modify();

    if ( !bRecordUndo )
        Transaction.Cancel();

#endif // WITH_EDITOR

    MarkPreChanged( bTriggerModify );
    Values[ Idx ] = VariantValue;
    MarkChanged( bTriggerModify );

    return true;
}

TOptional< float >
UHoudiniAssetParameterFloat::GetValue( int32 Idx ) const
{
    return TOptional< float >( Values[ Idx ] );
}

void
UHoudiniAssetParameterFloat::SetValue( float InValue, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    if ( InValue != Values[ Idx ] )
    {

#if WITH_EDITOR

        // If this is not a slider change (user typed in values manually), record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterFloatChange", "Houdini Parameter Float: Changing a value" ),
            PrimaryObject );
        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif // WITH_EDITOR

        MarkPreChanged( bTriggerModify );

        Values[ Idx ] = FMath::Clamp< float >( InValue, ValueMin, ValueMax );

        // Mark this parameter as changed.
        MarkChanged( bTriggerModify );
    }
}

float
UHoudiniAssetParameterFloat::GetParameterValue( int32 Idx, float DefaultValue ) const
{
    if ( Values.Num() <= Idx )
        return DefaultValue;

    return Values[ Idx ];
}

#if WITH_EDITOR

void
UHoudiniAssetParameterFloat::OnSliderMovingBegin( int32 Idx )
{
    bSliderDragged = true;
    
    // We want to record undo increments only when user lets go of the slider.
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterIntChange", "Houdini Parameter Float: Changing a value" ),
        PrimaryObject );

    Modify();
}

void
UHoudiniAssetParameterFloat::OnSliderMovingFinish( float InValue, int32 Idx )
{
    bSliderDragged = false;

    // Mark this parameter as changed.
    MarkChanged( true );
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE