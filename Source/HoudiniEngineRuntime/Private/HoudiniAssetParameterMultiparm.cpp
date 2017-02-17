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
*      Damian Campeanu
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniApi.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"

#include "Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetParameterMultiparm::UHoudiniAssetParameterMultiparm( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , MultiparmValue( 0 )
    , LastModificationType( RegularValueChange )
    , LastRemoveAddInstanceIndex( -1 )
{}

UHoudiniAssetParameterMultiparm::~UHoudiniAssetParameterMultiparm()
{}

UHoudiniAssetParameterMultiparm *
UHoudiniAssetParameterMultiparm::Create(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo)
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

    UHoudiniAssetParameterMultiparm * HoudiniAssetParameterMultiparm = NewObject< UHoudiniAssetParameterMultiparm >(
        Outer, UHoudiniAssetParameterMultiparm::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterMultiparm->CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterMultiparm;
}

bool
UHoudiniAssetParameterMultiparm::CreateParameter(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    if ( ParmInfo.type != HAPI_PARMTYPE_MULTIPARMLIST )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.intValuesIndex );

    // Get the actual value for this property.
    MultiparmValue = 0;
    if ( FHoudiniApi::GetParmIntValues( nullptr, InNodeId, &MultiparmValue, ValuesIndex, 1 ) != HAPI_RESULT_SUCCESS )
        return false;

    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterMultiparm::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    
    // Create the standard parameter name widget.
    CreateNameWidget( Row, true );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );

    TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;

    HorizontalBox->AddSlot().Padding( 2, 2, 5, 2 )
    [
        SAssignNew( NumericEntryBox, SNumericEntryBox< int32 > )
        .AllowSpin( true )

        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )

        .Value( TAttribute< TOptional< int32 > >::Create( TAttribute< TOptional< int32 > >::FGetter::CreateUObject(
            this, &UHoudiniAssetParameterMultiparm::GetValue ) ) )
        .OnValueChanged( SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(
            this, &UHoudiniAssetParameterMultiparm::SetValue ) )
        .OnValueCommitted( SNumericEntryBox<int32>::FOnValueCommitted::CreateUObject(
            this, &UHoudiniAssetParameterMultiparm::SetValueCommitted ) )
    ];

    HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
    [
        PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateUObject(
            this, &UHoudiniAssetParameterMultiparm::AddElement, true, true ),
            LOCTEXT( "AddAnotherMultiparmInstanceToolTip", "Add Another Instance" ) )
    ];

    HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
    [
        PropertyCustomizationHelpers::MakeRemoveButton( FSimpleDelegate::CreateUObject(
            this, &UHoudiniAssetParameterMultiparm::RemoveElement, true, true ),
            LOCTEXT( "RemoveLastMultiparmInstanceToolTip", "Remove Last Instance" ) )
    ];

    HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
    [
        PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateUObject(
            this, &UHoudiniAssetParameterMultiparm::SetValue, 0 ),
            LOCTEXT( "ClearAllMultiparmInstanesToolTip", "Clear All Instances" ) )
    ];

    if ( NumericEntryBox.IsValid() )
        NumericEntryBox->SetEnabled( !bIsDisabled );

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );

    Super::CreateWidget( LocalDetailCategoryBuilder );
}

void
UHoudiniAssetParameterMultiparm::AddMultiparmInstance( int32 ChildMultiparmInstanceIndex )
{
    // Set the last modification type and instance index before the current state
    // is saved by Modify().
    LastModificationType = InstanceAdded;
    LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex - 1; // Added above the current one.

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Adding instance" ),
        HoudiniAssetComponent );
    Modify();

    MarkPreChanged();

    FHoudiniApi::InsertMultiparmInstance(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        ChildMultiparmInstanceIndex );

    MultiparmValue++;

    // Save the Redo modification type (should be the opposite operation to this one).
    LastModificationType = InstanceRemoved;
    LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex;

    // Mark this parameter as changed.
    MarkChanged();
}

void
UHoudiniAssetParameterMultiparm::RemoveMultiparmInstance( int32 ChildMultiparmInstanceIndex )
{
    // Set the last modification type and instance index before the current state
    // is saved by Modify().
    LastModificationType = InstanceRemoved;
    LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex;

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Removing instance" ),
        HoudiniAssetComponent );
    Modify();

    MarkPreChanged();

    FHoudiniApi::RemoveMultiparmInstance(
        FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
        ChildMultiparmInstanceIndex );

    MultiparmValue--;

    // Save the Redo modification type (should be the opposite operation to this one).
    LastModificationType = InstanceAdded;
    LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex;

    // Mark this parameter as changed.
    MarkChanged();
}

#endif

bool
UHoudiniAssetParameterMultiparm::UploadParameterValue()
{
    if ( FHoudiniApi::SetParmIntValues(
        FHoudiniEngine::Get().GetSession(), NodeId,
        &MultiparmValue, ValuesIndex, 1 ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

#if WITH_EDITOR

void
UHoudiniAssetParameterMultiparm::SetValueCommitted( int32 InValue, ETextCommit::Type CommitType )
{}

#endif

TOptional< int32 >
UHoudiniAssetParameterMultiparm::GetValue() const
{
    return TOptional< int32 >( MultiparmValue );
}

void
UHoudiniAssetParameterMultiparm::SetValue( int32 InValue )
{
    if ( MultiparmValue != InValue )
    {
        LastModificationType = RegularValueChange;
        LastRemoveAddInstanceIndex = -1;

#if WITH_EDITOR

        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Changing a value" ),
            HoudiniAssetComponent );
        Modify();

#endif

        MarkPreChanged();

        MultiparmValue = InValue;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

void
UHoudiniAssetParameterMultiparm::AddElement( bool bTriggerModify, bool bRecordUndo )
{
    AddElements( 1, bTriggerModify, bRecordUndo );
}

void
UHoudiniAssetParameterMultiparm::AddElements( int32 NumElements, bool bTriggerModify, bool bRecordUndo )
{
    if ( NumElements > 0 )
    {
        LastModificationType = RegularValueChange;
        LastRemoveAddInstanceIndex = -1;

#if WITH_EDITOR

        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Changing a value" ),
            HoudiniAssetComponent );
        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif

        MarkPreChanged( bTriggerModify );

        MultiparmValue += NumElements;

        MarkChanged( bTriggerModify );
    }
}

void
UHoudiniAssetParameterMultiparm::RemoveElement( bool bTriggerModify, bool bRecordUndo )
{
    RemoveElements( 1, bTriggerModify, bRecordUndo );
}

void
UHoudiniAssetParameterMultiparm::RemoveElements( int32 NumElements, bool bTriggerModify, bool bRecordUndo )
{
    if ( NumElements > 0 )
    {
        if ( MultiparmValue - NumElements < 0 )
            NumElements = MultiparmValue;

        LastModificationType = RegularValueChange;
        LastRemoveAddInstanceIndex = -1;

#if WITH_EDITOR

        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Changing a value" ),
            HoudiniAssetComponent );
        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif

        MarkPreChanged( bTriggerModify );

        MultiparmValue -= NumElements;

        MarkChanged( bTriggerModify );
    }
}

void
UHoudiniAssetParameterMultiparm::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    if ( Ar.IsTransacting() )
    {
        SerializeEnumeration( Ar, LastModificationType );
        Ar << LastRemoveAddInstanceIndex;
    }

    if ( Ar.IsLoading() )
        MultiparmValue = 0;

    Ar << MultiparmValue;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterMultiparm::PostEditUndo()
{
    if ( LastModificationType == InstanceAdded )
    {
        FHoudiniApi::RemoveMultiparmInstance(
            FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
            LastRemoveAddInstanceIndex );
    }
    else if( LastModificationType == InstanceRemoved )
    {
        FHoudiniApi::InsertMultiparmInstance(
            FHoudiniEngine::Get().GetSession(), NodeId, ParmId,
            LastRemoveAddInstanceIndex );
    }

    Super::PostEditUndo();
}

#endif
