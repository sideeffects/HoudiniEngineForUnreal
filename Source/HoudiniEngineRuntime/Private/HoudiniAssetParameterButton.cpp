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
#include "HoudiniAssetParameterButton.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "Widgets/Input/SButton.h"

UHoudiniAssetParameterButton::UHoudiniAssetParameterButton( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{}

UHoudiniAssetParameterButton::~UHoudiniAssetParameterButton()
{}

UHoudiniAssetParameterButton *
UHoudiniAssetParameterButton::Create(
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

    UHoudiniAssetParameterButton * HoudiniAssetParameterButton = NewObject< UHoudiniAssetParameterButton >(
        Outer, UHoudiniAssetParameterButton::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterButton->CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterButton;
}

bool
UHoudiniAssetParameterButton::CreateParameter(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo ) )
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

#if WITH_EDITOR

void
UHoudiniAssetParameterButton::CreateWidget( IDetailCategoryBuilder & localDetailCategoryBuilder )
{
    Super::CreateWidget( localDetailCategoryBuilder );

    FDetailWidgetRow & Row = localDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    FText ParameterLabelText = FText::FromString( GetParameterLabel() );

    // Create the standard parameter name widget.
    CreateNameWidget( Row, false );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );
    TSharedPtr< SButton > Button;

    HorizontalBox->AddSlot().Padding( 1, 2, 4, 2 )
    [
        SAssignNew( Button, SButton )
        .VAlign( VAlign_Center )
        .HAlign( HAlign_Center )
        .Text( ParameterLabelText )
        .ToolTipText( ParameterLabelText )
        .OnClicked( FOnClicked::CreateUObject( this, &UHoudiniAssetParameterButton::OnButtonClick ) )
    ];

    if ( Button.IsValid() )
        Button->SetEnabled( !bIsDisabled );

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

FReply
UHoudiniAssetParameterButton::OnButtonClick()
{
    // There's no undo operation for button.

    MarkPreChanged();
    MarkChanged();

    return FReply::Handled();
}

#endif // WITH_EDITOR
