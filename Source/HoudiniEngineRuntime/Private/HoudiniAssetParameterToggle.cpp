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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"

UHoudiniAssetParameterToggle::UHoudiniAssetParameterToggle( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{
    // Parameter will have at least one value.
    Values.AddZeroed( 1 );
}

UHoudiniAssetParameterToggle::~UHoudiniAssetParameterToggle()
{}

UHoudiniAssetParameterToggle *
UHoudiniAssetParameterToggle::Create(
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

    UHoudiniAssetParameterToggle * HoudiniAssetParameterToggle = NewObject< UHoudiniAssetParameterToggle >(
        Outer, UHoudiniAssetParameterToggle::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterToggle->CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterToggle;
}

bool
UHoudiniAssetParameterToggle::CreateParameter(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle toggle type.
    if ( ParmInfo.type != HAPI_PARMTYPE_TOGGLE )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.intValuesIndex );

    // Get the actual value for this property.
    Values.SetNumZeroed( TupleSize );
    if ( FHoudiniApi::GetParmIntValues(
        FHoudiniEngine::Get().GetSession(), InNodeId, &Values[ 0 ],
        ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    // Min and max make no sense for this type of parameter.
    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterToggle::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    Super::CreateWidget( LocalDetailCategoryBuilder );

    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    FText ParameterLabelText = FText::FromString( GetParameterLabel() );

    // Create the standard parameter name widget.
    CreateNameWidget( Row, false );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    for ( int32 Idx = 0; Idx < TupleSize; ++Idx )
    {
        TSharedPtr< SCheckBox > CheckBox;

        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
        [
            SAssignNew( CheckBox, SCheckBox )
            .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                this, &UHoudiniAssetParameterToggle::CheckStateChanged, Idx ) )
            .IsChecked( TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetParameterToggle::IsChecked, Idx ) ) )
            .Content()
            [
                SNew( STextBlock )
                .Text( ParameterLabelText )
                .ToolTipText( ParameterLabelText )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];

        if ( CheckBox.IsValid() )
            CheckBox->SetEnabled( !bIsDisabled );
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void
UHoudiniAssetParameterToggle::CreateWidget( TSharedPtr< SVerticalBox > VerticalBox )
{
    Super::CreateWidget( VerticalBox );
    FText ParameterLabelText = FText::FromString( GetParameterLabel() );

    for ( int32 Idx = 0; Idx < TupleSize; ++Idx )
    {
        VerticalBox->AddSlot().Padding( 0, 2, 0, 2 )
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot().MaxWidth( 8 )
            [
                SNew( STextBlock )
                .Text( FText::GetEmpty() )
                .ToolTipText( ParameterLabelText )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            +SHorizontalBox::Slot()
            [
                SNew( SCheckBox )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetParameterToggle::CheckStateChanged, Idx ) )
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        this, &UHoudiniAssetParameterToggle::IsChecked, Idx ) ) )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( ParameterLabelText )
                    .ToolTipText( ParameterLabelText )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
            ]
        ];
    }
}

#endif

bool
UHoudiniAssetParameterToggle::UploadParameterValue()
{
    if ( FHoudiniApi::SetParmIntValues(
        FHoudiniEngine::Get().GetSession(), NodeId, &Values[ 0 ],
        ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterToggle::SetParameterVariantValue(
    const FVariant & Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    int32 VariantType = Variant.GetType();
    int32 VariantValue = 0;

    if ( Idx >= 0 && Idx < Values.Num() )
        return false;

    switch ( VariantType )
    {
        case EVariantTypes::Int8:
        case EVariantTypes::Int16:
        case EVariantTypes::Int32:
        case EVariantTypes::Int64:
        case EVariantTypes::UInt8:
        case EVariantTypes::UInt16:
        case EVariantTypes::UInt32:
        case EVariantTypes::UInt64:
        {
            VariantValue = Variant.GetValue< int32 >();
            break;
        }

        case EVariantTypes::Bool:
        {
            VariantValue = (int32) Variant.GetValue< bool >();
            break;
        }

        default:
        {
            return false;
        }
    }

#if WITH_EDITOR

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterToggleChange", "Houdini Parameter Toggle: Changing a value" ),
        HoudiniAssetComponent );

    Modify();

    if ( !bRecordUndo )
        Transaction.Cancel();

#endif

    MarkPreChanged();
    Values[ Idx ] = VariantValue;
    MarkChanged();

    return true;
}

void
UHoudiniAssetParameterToggle::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << Values;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterToggle::CheckStateChanged( ECheckBoxState NewState, int32 Idx )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( Values[ Idx ] != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterToggleChange", "Houdini Parameter Toggle: Changing a value" ),
            HoudiniAssetComponent );
        Modify();

        MarkPreChanged();

        Values[ Idx ] = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetParameterToggle::IsChecked( int32 Idx ) const
{
    if ( Values[ Idx ] )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

#endif
