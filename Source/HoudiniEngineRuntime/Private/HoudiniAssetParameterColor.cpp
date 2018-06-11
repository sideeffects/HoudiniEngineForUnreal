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
#include "HoudiniAssetParameterColor.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"

#include "Internationalization/Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetParameterColor::UHoudiniAssetParameterColor( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , Color( FColor::White )
{}

void
UHoudiniAssetParameterColor::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    if ( Ar.IsLoading() )
        Color = FColor::White;

    Ar << Color;
}

UHoudiniAssetParameterColor *
UHoudiniAssetParameterColor::Create(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
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

    UHoudiniAssetParameterColor * HoudiniAssetParameterColor = NewObject< UHoudiniAssetParameterColor >(
        Outer, UHoudiniAssetParameterColor::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterColor->CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterColor;
}

bool
UHoudiniAssetParameterColor::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle float type.
    if ( ParmInfo.type != HAPI_PARMTYPE_COLOR )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.floatValuesIndex );

    // Get the actual value for this property.
    Color = FLinearColor::White;
    if ( FHoudiniApi::GetParmFloatValues(
        FHoudiniEngine::Get().GetSession(), InNodeId,
        (float *) &Color.R, ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( TupleSize == 3 )
        Color.A = 1.0f;

    return true;
}

bool
UHoudiniAssetParameterColor::UploadParameterValue()
{
    if ( FHoudiniApi::SetParmFloatValues(
        FHoudiniEngine::Get().GetSession(), NodeId, (const float*)&Color.R, ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterColor::SetParameterVariantValue( const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    EVariantTypes VariantType = Variant.GetType();
    FLinearColor VariantLinearColor = FLinearColor::White;

    if ( EVariantTypes::Color == VariantType )
    {
        FColor VariantColor = Variant.GetValue<FColor>();
        VariantLinearColor = VariantColor.ReinterpretAsLinear();
    }
    else if ( EVariantTypes::LinearColor == VariantType )
    {
        VariantLinearColor = Variant.GetValue< FLinearColor >();
    }
    else
    {
        return false;
    }

#if WITH_EDITOR

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterColorChange", "Houdini Parameter Color: Changing a value" ),
        PrimaryObject );

    Modify();

    if ( !bRecordUndo )
        Transaction.Cancel();

#endif // WITH_EDITOR

    MarkPreChanged( bTriggerModify );
    Color = VariantLinearColor;
    MarkChanged( bTriggerModify );

    return true;
}

FLinearColor
UHoudiniAssetParameterColor::GetColor() const
{
    return Color;
}

void
UHoudiniAssetParameterColor::OnPaintColorChanged( FLinearColor InNewColor, bool bTriggerModify, bool bRecordUndo )
{
    if ( InNewColor != Color )
    {

#if WITH_EDITOR

        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterColorChange", "Houdini Parameter Color: Changing a value" ),
            PrimaryObject );
        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif // WITH_EDITOR

        MarkPreChanged( bTriggerModify );

        Color = InNewColor;

        // Mark this parameter as changed.
        MarkChanged( bTriggerModify );
    }
}

#undef LOCTEXT_NAMESPACE