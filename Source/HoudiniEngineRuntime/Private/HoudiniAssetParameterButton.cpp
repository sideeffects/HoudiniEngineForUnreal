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
#include "HoudiniAssetParameterButton.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"

UHoudiniAssetParameterButton::UHoudiniAssetParameterButton( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{}

UHoudiniAssetParameterButton *
UHoudiniAssetParameterButton::Create(
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

    UHoudiniAssetParameterButton * HoudiniAssetParameterButton = NewObject< UHoudiniAssetParameterButton >(
        Outer, UHoudiniAssetParameterButton::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterButton->CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterButton;
}

bool
UHoudiniAssetParameterButton::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle button type.
    if ( ParmInfo.type != HAPI_PARMTYPE_BUTTON )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.intValuesIndex );

    return true;
}

bool
UHoudiniAssetParameterButton::UploadParameterValue()
{
    int32 PressValue = 1;
    if ( FHoudiniApi::SetParmIntValues(
        FHoudiniEngine::Get().GetSession(), NodeId, &PressValue, ValuesIndex, 1 ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterButton::SetParameterVariantValue(
    const FVariant & Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    // We don't care about variant values for button. Just trigger the click.
    MarkPreChanged( bTriggerModify );
    MarkChanged( bTriggerModify );

    return true;
}
