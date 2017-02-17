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
#include "HoudiniAssetParameterSeparator.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "Widgets/Layout/SSeparator.h"

UHoudiniAssetParameterSeparator::UHoudiniAssetParameterSeparator( const FObjectInitializer & ObjectInitializer ) :
    Super(ObjectInitializer)
{}

UHoudiniAssetParameterSeparator::~UHoudiniAssetParameterSeparator()
{}

UHoudiniAssetParameterSeparator *
UHoudiniAssetParameterSeparator::Create(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    UObject * Outer = InHoudiniAssetComponent;
    if ( !Outer )
    {
        Outer = InParentParameter;
        if ( !Outer )
        {
            // Must have either component or parent not null.
            check( false );
        }
    }

    UHoudiniAssetParameterSeparator * HoudiniAssetParameterSeparator = NewObject< UHoudiniAssetParameterSeparator >(
        Outer, UHoudiniAssetParameterSeparator::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterSeparator->CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterSeparator;
}

bool
UHoudiniAssetParameterSeparator::CreateParameter(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle separator type.
    if ( ParmInfo.type != HAPI_PARMTYPE_SEPARATOR )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.stringValuesIndex );

    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterSeparator::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    Super::CreateWidget( LocalDetailCategoryBuilder );

    TSharedPtr< SSeparator > Separator;

    LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
    [
        SNew( SVerticalBox )
        +SVerticalBox::Slot()
        .Padding( 0, 0, 5, 0 )
        [
            SAssignNew( Separator, SSeparator )
            .Thickness( 2.0f )
        ]
    ];

    if ( Separator.IsValid() )
        Separator->SetEnabled( !bIsDisabled );
}

#endif
