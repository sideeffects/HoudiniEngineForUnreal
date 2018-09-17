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
#include "HoudiniParameterDetails.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetInstanceInputField.h"
#include "HoudiniAssetParameterButton.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterLabel.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniAssetParameterRamp.h"
#include "HoudiniAssetParameterSeparator.h"
#include "HoudiniAssetParameterString.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniRuntimeSettings.h"
#include "SNewFilePathPicker.h"

#include "Editor/CurveEditor/Public/CurveEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerPublicTypes.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/UnrealEd/Public/Layers/ILayers.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "Editor/PropertyEditor/Private/PropertyNode.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "EditorDirectories.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Internationalization.h"
#include "IDetailGroup.h"
#include "Particles/ParticleSystemComponent.h"
#include "SCurveEditor.h"
#include "SAssetDropTarget.h"
#include "Sound/SoundBase.h"
#include "Math/UnitConversion.h"
#include "Math/NumericLimits.h"
#include "Misc/Optional.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniParameterDetails::CreateNameWidget( UHoudiniAssetParameter* InParam, FDetailWidgetRow & Row, bool WithLabel )
{
    if ( !InParam )
        return;

    FText ParameterLabelText = FText::FromString( InParam->GetParameterLabel() );
    const FText & FinalParameterLabelText = WithLabel ? ParameterLabelText : FText::GetEmpty();

    FText ParameterTooltip = GetParameterTooltip( InParam );
    if ( InParam->bIsChildOfMultiparm && InParam->ParentParameter )
    {
        TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );

        // We have to make sure the ParentParameter is a multiparm, as folders will cause issues here
        // ( we want to call RemoveMultiParmInstance or AddMultiParmInstance on the parent multiparm, not just the parent)
        UHoudiniAssetParameter * ParentMultiparm = InParam->ParentParameter;
        while ( ParentMultiparm && !ParentMultiparm->bIsMultiparm )
            ParentMultiparm = ParentMultiparm->ParentParameter;

        // Failed to find the multiparm parent, better have the original parent than nullptr
        if ( !ParentMultiparm )
            ParentMultiparm = InParam->ParentParameter;

        TSharedRef< SWidget > ClearButton = PropertyCustomizationHelpers::MakeClearButton(
            FSimpleDelegate::CreateUObject(
            (UHoudiniAssetParameterMultiparm *)ParentMultiparm,
                &UHoudiniAssetParameterMultiparm::RemoveMultiparmInstance,
                InParam->MultiparmInstanceIndex ),
            LOCTEXT( "RemoveMultiparmInstanceToolTip", "Remove" ) );
        TSharedRef< SWidget > AddButton = PropertyCustomizationHelpers::MakeAddButton(
            FSimpleDelegate::CreateUObject(
            (UHoudiniAssetParameterMultiparm *)ParentMultiparm,
                &UHoudiniAssetParameterMultiparm::AddMultiparmInstance,
                InParam->MultiparmInstanceIndex ),
            LOCTEXT( "InsertBeforeMultiparmInstanceToolTip", "Insert Before" ) );

        if ( InParam->ChildIndex != 0 )
        {
            AddButton.Get().SetVisibility( EVisibility::Hidden );
            ClearButton.Get().SetVisibility( EVisibility::Hidden );
        }

        // Adding eventual padding for nested multiparams
        UHoudiniAssetParameter* currentParentParameter = ParentMultiparm;
        while ( currentParentParameter && currentParentParameter->bIsChildOfMultiparm )
        {
            if ( currentParentParameter->bIsMultiparm )
                HorizontalBox->AddSlot().MaxWidth( 16.0f );

            currentParentParameter = currentParentParameter->ParentParameter;
        }

        HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
        [
            ClearButton
        ];

        HorizontalBox->AddSlot().AutoWidth().Padding( 0.0f, 0.0f )
        [
            AddButton
        ];

        HorizontalBox->AddSlot().Padding( 2, 5, 5, 2 )
            [
                SNew( STextBlock )
                .Text( FinalParameterLabelText )
                .ToolTipText( WithLabel ? ParameterTooltip : ParameterLabelText )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ];

        Row.NameWidget.Widget = HorizontalBox;
    }
    else
    {
        Row.NameWidget.Widget =
            SNew( STextBlock )
            .Text( FinalParameterLabelText )
            .ToolTipText( ParameterTooltip )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
    }
}

FText
FHoudiniParameterDetails::GetParameterTooltip( UHoudiniAssetParameter* InParam )
{
    if ( !InParam )
        return FText();

    /*
    FString Tooltip = InParam->GetParameterLabel() + TEXT(" (") + InParam->GetParameterName() + TEXT(")");
    if ( !InParam->GetParameterHelp().IsEmpty() )
        Tooltip += TEXT ( ":\n") + InParam->GetParameterHelp();

    return FText::FromString( Tooltip );
    */
    
    if ( !InParam->GetParameterHelp().IsEmpty() )
        return FText::FromString( InParam->GetParameterHelp() );
    else
        return FText::FromString( InParam->GetParameterLabel() + TEXT( " (" ) + InParam->GetParameterName() + TEXT( ")" ) );
}

void 
FHoudiniParameterDetails::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder, UHoudiniAssetParameter* InParam )
{
    check( InParam );

    if ( auto ParamFloat = Cast<UHoudiniAssetParameterFloat>( InParam ) )
    {
        CreateWidgetFloat( LocalDetailCategoryBuilder, *ParamFloat );
    }
    else if ( auto ParamFolder = Cast<UHoudiniAssetParameterFolder>( InParam ) )
    {
        CreateWidgetFolder( LocalDetailCategoryBuilder, *ParamFolder );
    }
    else if ( auto ParamFolderList = Cast<UHoudiniAssetParameterFolderList>( InParam ) )
    {
        CreateWidgetFolderList( LocalDetailCategoryBuilder, *ParamFolderList );
    }
    // Test Ramp before Multiparm!
    else if ( auto ParamRamp = Cast<UHoudiniAssetParameterRamp>( InParam ) )
    {
        CreateWidgetRamp( LocalDetailCategoryBuilder, *ParamRamp );
    }
    else if ( auto ParamMultiparm = Cast<UHoudiniAssetParameterMultiparm>( InParam ) )
    {
        CreateWidgetMultiparm( LocalDetailCategoryBuilder, *ParamMultiparm );
    }
    else if ( auto ParamButton = Cast<UHoudiniAssetParameterButton>( InParam ) )
    {
        CreateWidgetButton( LocalDetailCategoryBuilder, *ParamButton );
    }
    else if ( auto ParamChoice = Cast<UHoudiniAssetParameterChoice>( InParam ) )
    {
        CreateWidgetChoice( LocalDetailCategoryBuilder, *ParamChoice );
    }
    else if ( auto ParamColor = Cast<UHoudiniAssetParameterColor>( InParam ) )
    {
        CreateWidgetColor( LocalDetailCategoryBuilder, *ParamColor );
    }
    else if ( auto ParamToggle = Cast<UHoudiniAssetParameterToggle>( InParam ) )
    {
        CreateWidgetToggle( LocalDetailCategoryBuilder, *ParamToggle );
    }
    else if ( auto ParamInput = Cast<UHoudiniAssetInput>( InParam ) )
    {
        CreateWidgetInput( LocalDetailCategoryBuilder, *ParamInput );
    }
    else if ( auto ParamInt = Cast<UHoudiniAssetParameterInt>( InParam ) )
    {
        CreateWidgetInt( LocalDetailCategoryBuilder, *ParamInt );
    }
    else if ( auto ParamInstanceInput = Cast<UHoudiniAssetInstanceInput>( InParam ) )
    {
        CreateWidgetInstanceInput( LocalDetailCategoryBuilder, *ParamInstanceInput );
    }
    else if ( auto ParamLabel = Cast<UHoudiniAssetParameterLabel>( InParam ) )
    {
        CreateWidgetLabel( LocalDetailCategoryBuilder, *ParamLabel );
    }
    else if ( auto ParamString = Cast<UHoudiniAssetParameterString>( InParam ) )
    {
        CreateWidgetString( LocalDetailCategoryBuilder, *ParamString );
    }
    else if ( auto ParamSeparator = Cast<UHoudiniAssetParameterSeparator>( InParam ) )
    {
        CreateWidgetSeparator( LocalDetailCategoryBuilder, *ParamSeparator );
    }
    else if ( auto ParamFile = Cast<UHoudiniAssetParameterFile>( InParam ) )
    {
        CreateWidgetFile( LocalDetailCategoryBuilder, *ParamFile );
    }
    else
    {
        check( false );
    }
}

void 
FHoudiniParameterDetails::CreateWidget( TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameter* InParam )
{
    check( InParam );

    if ( auto ParamChoice = Cast<UHoudiniAssetParameterChoice>( InParam ) )
    {
        CreateWidgetChoice( VerticalBox, *ParamChoice );
    }
    else if ( auto ParamToggle = Cast<UHoudiniAssetParameterToggle>( InParam ) )
    {
        CreateWidgetToggle( VerticalBox, *ParamToggle );
    }
    else
    {
        check( false );
    }
}

void 
FHoudiniParameterDetails::CreateWidgetFile( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFile& InParam )
{
    FDetailWidgetRow& Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedRef<SVerticalBox> VerticalBox = SNew( SVerticalBox );

    FString FileTypeWidgetFilter = TEXT( "All files (*.*)|*.*" );
    if ( !InParam.Filters.IsEmpty() )
        FileTypeWidgetFilter = FString::Printf( TEXT( "%s files (*.%s)|*.%s" ), *InParam.Filters, *InParam.Filters, *InParam.Filters );

    FString BrowseWidgetDirectory = FEditorDirectories::Get().GetLastDirectory( ELastDirectory::GENERIC_OPEN );

    for ( int32 Idx = 0; Idx < InParam.GetTupleSize(); ++Idx )
    {
        FString FileWidgetPath = InParam.Values[Idx];
        FString FileWidgetBrowsePath = BrowseWidgetDirectory;

        if ( !FileWidgetPath.IsEmpty() )
        {
            FString FileWidgetDirPath = FPaths::GetPath( FileWidgetPath );
            if ( !FileWidgetDirPath.IsEmpty() )
                FileWidgetBrowsePath = FileWidgetDirPath;
        }

        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
        [
            SNew( SNewFilePathPicker )
            .BrowseButtonImage( FEditorStyle::GetBrush( "PropertyWindow.Button_Ellipsis" ) )
            .BrowseButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
            .BrowseButtonToolTip( LOCTEXT( "FileButtonToolTipText", "Choose a file" ) )
            .BrowseDirectory( FileWidgetBrowsePath )
            .BrowseTitle( LOCTEXT( "PropertyEditorTitle", "File picker..." ) )
            .FilePath( FileWidgetPath )
            .FileTypeFilter( FileTypeWidgetFilter )
            .OnPathPicked( FOnPathPicked::CreateUObject(
            &InParam, &UHoudiniAssetParameterFile::HandleFilePathPickerPathPicked, Idx ) )
            .IsNewFile( !InParam.IsReadOnly )
        ];

    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );

    Row.ValueWidget.Widget->SetEnabled( !InParam.bIsDisabled );
}

void
FHoudiniParameterDetails::CreateWidgetFolder( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFolder& InParam )
{
    if ( InParam.ParentParameter && InParam.ParentParameter->IsActiveChildParameter( &InParam ) )
    {
        // Recursively create all child parameters.
        for ( UHoudiniAssetParameter * ChildParam : InParam.ChildParameters )
            FHoudiniParameterDetails::CreateWidget( LocalDetailCategoryBuilder, ChildParam );
    }
}

void
FHoudiniParameterDetails::CreateWidgetFolderList( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFolderList& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterFolderList> MyParam( &InParam );
    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );

    LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
    [
        SAssignNew(HorizontalBox, SHorizontalBox)
    ];

    for ( int32 ParameterIdx = 0; ParameterIdx < InParam.ChildParameters.Num(); ++ParameterIdx )
    {
        UHoudiniAssetParameter * HoudiniAssetParameterChild = InParam.ChildParameters[ ParameterIdx ];
        if ( HoudiniAssetParameterChild->IsA( UHoudiniAssetParameterFolder::StaticClass() ) )
        {
            FText ParameterLabelText = FText::FromString( HoudiniAssetParameterChild->GetParameterLabel() );
            FText ParameterToolTip = GetParameterTooltip( HoudiniAssetParameterChild );

            HorizontalBox->AddSlot().Padding( 0, 2, 0, 2 )
            [
                SNew( SButton )
                .VAlign( VAlign_Center )
                .HAlign( HAlign_Center )
                .Text( ParameterLabelText )
                .ToolTipText( ParameterToolTip )
                .OnClicked( FOnClicked::CreateLambda( [=]() {
                    if ( MyParam.IsValid() )
                    {
                        MyParam->ActiveChildParameter = ParameterIdx;
                        MyParam->OnParamStateChanged();
                    }
                    return FReply::Handled();
                }))
            ];
        }
    }

    // Recursively create all child parameters.
    for ( UHoudiniAssetParameter * ChildParam : InParam.ChildParameters )
        FHoudiniParameterDetails::CreateWidget( LocalDetailCategoryBuilder, ChildParam );

    if ( InParam.ChildParameters.Num() > 1 )
    {
        TSharedPtr< STextBlock > TextBlock;

        LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
        [
            SAssignNew( TextBlock, STextBlock )
            .Text( FText::GetEmpty() )
            .ToolTipText( FText::GetEmpty() )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            .WrapTextAt( HAPI_UNREAL_DESIRED_ROW_FULL_WIDGET_WIDTH )
        ];

        if ( TextBlock.IsValid() )
            TextBlock->SetEnabled( !InParam.bIsDisabled );
    }
}

void 
FHoudiniParameterDetails::CreateWidgetMultiparm( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterMultiparm& InParam )
{
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    
    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );

    TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;

    HorizontalBox->AddSlot().Padding( 2, 2, 5, 2 )
    [
        SAssignNew( NumericEntryBox, SNumericEntryBox< int32 > )
        .AllowSpin( true )

        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )

        .Value( TAttribute< TOptional< int32 > >::Create( TAttribute< TOptional< int32 > >::FGetter::CreateUObject(
            &InParam, &UHoudiniAssetParameterMultiparm::GetValue ) ) )
        .OnValueChanged( SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(
            &InParam, &UHoudiniAssetParameterMultiparm::SetValue ) )
    ];

    HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
    [
        PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateUObject(
            &InParam, &UHoudiniAssetParameterMultiparm::AddElement, true, true ),
            LOCTEXT( "AddAnotherMultiparmInstanceToolTip", "Add Another Instance" ) )
    ];

    HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
    [
        PropertyCustomizationHelpers::MakeRemoveButton( FSimpleDelegate::CreateUObject(
            &InParam, &UHoudiniAssetParameterMultiparm::RemoveElement, true, true ),
            LOCTEXT( "RemoveLastMultiparmInstanceToolTip", "Remove Last Instance" ) )
    ];

    HorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 0.0f )
    [
        PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateUObject(
            &InParam, &UHoudiniAssetParameterMultiparm::SetValue, 0 ),
            LOCTEXT( "ClearAllMultiparmInstanesToolTip", "Clear All Instances" ) )
    ];

    if ( NumericEntryBox.IsValid() )
        NumericEntryBox->SetEnabled( !InParam.bIsDisabled );

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );

    // Recursively create all child parameters.
    for ( UHoudiniAssetParameter * ChildParam : InParam.ChildParameters )
        FHoudiniParameterDetails::CreateWidget( LocalDetailCategoryBuilder, ChildParam );
}

/** We need to inherit from curve editor in order to get subscription to mouse events. **/
class SHoudiniAssetParameterRampCurveEditor : public SCurveEditor
{
public:

    SLATE_BEGIN_ARGS( SHoudiniAssetParameterRampCurveEditor )
        : _ViewMinInput( 0.0f )
        , _ViewMaxInput( 10.0f )
        , _ViewMinOutput( 0.0f )
        , _ViewMaxOutput( 1.0f )
        , _InputSnap( 0.1f )
        , _OutputSnap( 0.05f )
        , _InputSnappingEnabled( false )
        , _OutputSnappingEnabled( false )
        , _ShowTimeInFrames( false )
        , _TimelineLength( 5.0f )
        , _DesiredSize( FVector2D::ZeroVector )
        , _DrawCurve( true )
        , _HideUI( true )
        , _AllowZoomOutput( true )
        , _AlwaysDisplayColorCurves( false )
        , _ZoomToFitVertical( true )
        , _ZoomToFitHorizontal( true )
        , _ShowZoomButtons( true )
        , _XAxisName()
        , _YAxisName()
        , _ShowInputGridNumbers( true )
        , _ShowOutputGridNumbers( true )
        , _ShowCurveSelector( true )
        , _GridColor( FLinearColor( 0.0f, 0.0f, 0.0f, 0.3f ) )
        {
            _Clipping = EWidgetClipping::ClipToBounds;
        }

    SLATE_ATTRIBUTE(float, ViewMinInput)
        SLATE_ATTRIBUTE(float, ViewMaxInput)
        SLATE_ATTRIBUTE(TOptional<float>, DataMinInput)
        SLATE_ATTRIBUTE(TOptional<float>, DataMaxInput)
        SLATE_ATTRIBUTE(float, ViewMinOutput)
        SLATE_ATTRIBUTE(float, ViewMaxOutput)
        SLATE_ATTRIBUTE(float, InputSnap)
        SLATE_ATTRIBUTE(float, OutputSnap)
        SLATE_ATTRIBUTE(bool, InputSnappingEnabled)
        SLATE_ATTRIBUTE(bool, OutputSnappingEnabled)
        SLATE_ATTRIBUTE(bool, ShowTimeInFrames)
        SLATE_ATTRIBUTE(float, TimelineLength)
        SLATE_ATTRIBUTE(FVector2D, DesiredSize)
        SLATE_ATTRIBUTE(bool, AreCurvesVisible)
        SLATE_ARGUMENT(bool, DrawCurve)
        SLATE_ARGUMENT(bool, HideUI)
        SLATE_ARGUMENT(bool, AllowZoomOutput)
        SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
        SLATE_ARGUMENT(bool, ZoomToFitVertical)
        SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
        SLATE_ARGUMENT(bool, ShowZoomButtons)
        SLATE_ARGUMENT(TOptional<FString>, XAxisName)
        SLATE_ARGUMENT(TOptional<FString>, YAxisName)
        SLATE_ARGUMENT(bool, ShowInputGridNumbers)
        SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
        SLATE_ARGUMENT(bool, ShowCurveSelector)
        SLATE_ARGUMENT(FLinearColor, GridColor)
        SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
        SLATE_EVENT( FOnSetOutputViewRange, OnSetOutputViewRange )
        SLATE_EVENT( FOnSetAreCurvesVisible, OnSetAreCurvesVisible )
        SLATE_EVENT( FSimpleDelegate, OnCreateAsset )
        SLATE_END_ARGS()

public:

    /** Widget construction. **/
    void Construct( const FArguments & InArgs );

protected:

    /** Handle mouse up events. **/
    virtual FReply OnMouseButtonUp( const FGeometry & MyGeometry, const FPointerEvent & MouseEvent ) override;

public:

    /** Set parent ramp parameter. **/
    void SetParentRampParameter( UHoudiniAssetParameterRamp * InHoudiniAssetParameterRamp )
    {
        HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
    }

protected:

    /** Parent ramp parameter. **/
    TWeakObjectPtr<UHoudiniAssetParameterRamp> HoudiniAssetParameterRamp;
};

void
SHoudiniAssetParameterRampCurveEditor::Construct( const FArguments & InArgs )
{
    SCurveEditor::Construct( SCurveEditor::FArguments()
                             .ViewMinInput( InArgs._ViewMinInput )
                             .ViewMaxInput( InArgs._ViewMaxInput )
                             .ViewMinOutput( InArgs._ViewMinOutput )
                             .ViewMaxOutput( InArgs._ViewMaxOutput )
                             .XAxisName( InArgs._XAxisName )
                             .YAxisName( InArgs._YAxisName )
                             .HideUI( InArgs._HideUI )
                             .DrawCurve( InArgs._DrawCurve )
                             .TimelineLength( InArgs._TimelineLength )
                             .AllowZoomOutput( InArgs._AllowZoomOutput )
                             .ShowInputGridNumbers( InArgs._ShowInputGridNumbers )
                             .ShowOutputGridNumbers( InArgs._ShowOutputGridNumbers )
                             .ShowZoomButtons( InArgs._ShowZoomButtons )
                             .ZoomToFitHorizontal( InArgs._ZoomToFitHorizontal )
                             .ZoomToFitVertical( InArgs._ZoomToFitVertical )
    );

    HoudiniAssetParameterRamp = nullptr;

    UCurveEditorSettings * CurveEditorSettings = GetSettings();
    if ( CurveEditorSettings )
    {
        //CurveEditorSettings->SetCurveVisibility( ECurveEditorCurveVisibility::AllCurves );
        CurveEditorSettings->SetTangentVisibility( ECurveEditorTangentVisibility::NoTangents );
    }
}

FReply
SHoudiniAssetParameterRampCurveEditor::OnMouseButtonUp(
    const FGeometry & MyGeometry,
    const FPointerEvent & MouseEvent )
{
    FReply Reply = SCurveEditor::OnMouseButtonUp( MyGeometry, MouseEvent );

    if ( HoudiniAssetParameterRamp.IsValid() )
        HoudiniAssetParameterRamp->OnCurveEditingFinished();

    return Reply;
}

void 
FHoudiniParameterDetails::CreateWidgetRamp( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterRamp& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterRamp> MyParam( &InParam );
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    
    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );

    FString CurveAxisTextX = TEXT( "" );
    FString CurveAxisTextY = TEXT( "" );
    UClass * CurveClass = nullptr;

    if ( InParam.bIsFloatRamp )
    {
        CurveAxisTextX = TEXT( HAPI_UNREAL_RAMP_FLOAT_AXIS_X );
        CurveAxisTextY = TEXT( HAPI_UNREAL_RAMP_FLOAT_AXIS_Y );
        CurveClass = UHoudiniAssetParameterRampCurveFloat::StaticClass();
    }
    else
    {
        CurveAxisTextX = TEXT( HAPI_UNREAL_RAMP_COLOR_AXIS_X );
        CurveAxisTextY = TEXT( HAPI_UNREAL_RAMP_COLOR_AXIS_Y );
        CurveClass = UHoudiniAssetParameterRampCurveColor::StaticClass();
    }

    HorizontalBox->AddSlot().Padding( 2, 2, 5, 2 )
    [
        SNew( SBorder )
        .VAlign( VAlign_Fill )
        [
            SAssignNew( InParam.CurveEditor, SHoudiniAssetParameterRampCurveEditor )
            .ViewMinInput( 0.0f )
            .ViewMaxInput( 1.0f )
            .HideUI( true )
            .DrawCurve( true )
            .ViewMinInput( 0.0f )
            .ViewMaxInput( 1.0f )
            .ViewMinOutput( 0.0f )
            .ViewMaxOutput( 1.0f )
            .TimelineLength( 1.0f )
            .AllowZoomOutput( false )
            .ShowInputGridNumbers( false )
            .ShowOutputGridNumbers( false )
            .ShowZoomButtons( false )
            .ZoomToFitHorizontal( false )
            .ZoomToFitVertical( false )
            .XAxisName( CurveAxisTextX )
            .YAxisName( CurveAxisTextY )
            .ShowCurveSelector( false )
        ]
    ];

    // Set callback for curve editor events.
    TSharedPtr< SHoudiniAssetParameterRampCurveEditor > HoudiniRampEditor = StaticCastSharedPtr<SHoudiniAssetParameterRampCurveEditor>(InParam.CurveEditor );
    if ( HoudiniRampEditor.IsValid() )
        HoudiniRampEditor->SetParentRampParameter( &InParam );

    if ( InParam.bIsFloatRamp )
    {
        if ( !InParam.HoudiniAssetParameterRampCurveFloat )
        {
            InParam.HoudiniAssetParameterRampCurveFloat = Cast< UHoudiniAssetParameterRampCurveFloat >(
                NewObject< UHoudiniAssetParameterRampCurveFloat >(
                    InParam.PrimaryObject, UHoudiniAssetParameterRampCurveFloat::StaticClass(),
                    NAME_None, RF_Transactional | RF_Public ) );

            InParam.HoudiniAssetParameterRampCurveFloat->SetParentRampParameter( &InParam );
        }

        // Set curve values.
        InParam.GenerateCurvePoints();

        // Set the curve that is being edited.
        InParam.CurveEditor->SetCurveOwner( InParam.HoudiniAssetParameterRampCurveFloat, true );
    }
    else
    {
        if ( !InParam.HoudiniAssetParameterRampCurveColor )
        {
            InParam.HoudiniAssetParameterRampCurveColor = Cast< UHoudiniAssetParameterRampCurveColor >(
                NewObject< UHoudiniAssetParameterRampCurveColor >(
                    InParam.PrimaryObject, UHoudiniAssetParameterRampCurveColor::StaticClass(),
                    NAME_None, RF_Transactional | RF_Public ) );

            InParam.HoudiniAssetParameterRampCurveColor->SetParentRampParameter( &InParam );
        }

        // Set curve values.
        InParam.GenerateCurvePoints();

        // Set the curve that is being edited.
        InParam.CurveEditor->SetCurveOwner( InParam.HoudiniAssetParameterRampCurveColor, true );
    }

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );

    // Recursively create all child parameters.
    for ( UHoudiniAssetParameter * ChildParam : InParam.ChildParameters )
        FHoudiniParameterDetails::CreateWidget( LocalDetailCategoryBuilder, ChildParam );
}

void 
FHoudiniParameterDetails::CreateWidgetButton( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterButton& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterButton> MyParam( &InParam );
    FDetailWidgetRow& Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    FText ParameterTooltip = GetParameterTooltip( &InParam );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );
    TSharedPtr< SButton > Button;

    HorizontalBox->AddSlot().Padding( 1, 2, 4, 2 )
    [
        SAssignNew( Button, SButton )
        .VAlign( VAlign_Center )
        .HAlign( HAlign_Center )
        .Text( ParameterLabelText )
        .ToolTipText( ParameterTooltip )
        .OnClicked( FOnClicked::CreateLambda( [=]() {
            if ( MyParam.IsValid() )
            {
                // There's no undo operation for button.
                MyParam->MarkPreChanged();
                MyParam->MarkChanged();
            }
            return FReply::Handled();
            }))
    ];

    if ( Button.IsValid() )
        Button->SetEnabled( !InParam.bIsDisabled );

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );

}

void 
FHoudiniParameterDetails::CreateWidgetChoice( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterChoice& InParam )
{
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );
    TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBox;

    TWeakObjectPtr<UHoudiniAssetParameterChoice> MyParam( &InParam );

    HorizontalBox->AddSlot().Padding( 2, 2, 5, 2 )
    [
        SAssignNew( ComboBox, SComboBox< TSharedPtr< FString > > )
        .OptionsSource( &InParam.StringChoiceLabels )
        .InitiallySelectedItem( InParam.StringChoiceLabels[InParam.CurrentValue] )
        .OnGenerateWidget( SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateLambda(
            []( TSharedPtr< FString > ChoiceEntry ) {
                FText ChoiceEntryText = FText::FromString( *ChoiceEntry );
                return SNew( STextBlock )
                    .Text( ChoiceEntryText )
                    .ToolTipText( ChoiceEntryText )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
            }))
        .OnSelectionChanged( SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateLambda(
            [=]( TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType ) {
                if ( !NewChoice.IsValid() || !MyParam.IsValid() )
                        return;
                MyParam->OnChoiceChange( NewChoice );
            }))
        [
            SNew( STextBlock )
            .Text( TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetParameterChoice::HandleChoiceContentText ) ) )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
        ]
    ];

    if ( ComboBox.IsValid() )
        ComboBox->SetEnabled( !InParam.bIsDisabled );

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
FHoudiniParameterDetails::CreateWidgetChoice( TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameterChoice& InParam )
{
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    FText ParameterTooltip = GetParameterTooltip( &InParam );

    TWeakObjectPtr<UHoudiniAssetParameterChoice> MyParam( &InParam );
    
    VerticalBox->AddSlot().Padding( 2, 2, 2, 2 )
    [
        SNew( SHorizontalBox )
        + SHorizontalBox::Slot().MaxWidth( 80 ).Padding( 7, 1, 0, 0 ).VAlign( VAlign_Center )
        [
            SNew( STextBlock )
            .Text( ParameterLabelText )
            .ToolTipText( ParameterTooltip )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
        ]
        + SHorizontalBox::Slot()
        [
            SNew( SComboBox< TSharedPtr< FString > > )
            .OptionsSource( &InParam.StringChoiceLabels )
            .InitiallySelectedItem( InParam.StringChoiceLabels[InParam.CurrentValue] )
            .OnGenerateWidget( SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateLambda(
                []( TSharedPtr< FString > ChoiceEntry ) {
                    FText ChoiceEntryText = FText::FromString( *ChoiceEntry );
                    return SNew( STextBlock )
                        .Text( ChoiceEntryText )
                        .ToolTipText( ChoiceEntryText )
                        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
                }))
            .OnSelectionChanged( SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateLambda(
                [=]( TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType ) {
                    if ( !NewChoice.IsValid() || !MyParam.IsValid() )
                            return;
                    MyParam->OnChoiceChange( NewChoice );
                }))
            [
                SNew( STextBlock )
                .Text( TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetParameterChoice::HandleChoiceContentText ) ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ]
    ];
}

void 
FHoudiniParameterDetails::CreateWidgetColor( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterColor& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterColor> MyParam( &InParam );

    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedPtr< SColorBlock > ColorBlock;
    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );
    VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
    [
        SAssignNew( ColorBlock, SColorBlock )
        .Color( TAttribute< FLinearColor >::Create( TAttribute< FLinearColor >::FGetter::CreateUObject(
            &InParam, &UHoudiniAssetParameterColor::GetColor ) ) )
        .OnMouseButtonDown( FPointerEventHandler::CreateLambda(
            [=]( const FGeometry & MyGeometry, const FPointerEvent & MouseEvent ) 
            {
                if ( MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !MyParam.IsValid() )
                    return FReply::Unhandled();

                FColorPickerArgs PickerArgs;
                PickerArgs.ParentWidget = ColorBlock;
                PickerArgs.bUseAlpha = true;
                PickerArgs.DisplayGamma = TAttribute< float >::Create(
                    TAttribute< float >::FGetter::CreateUObject( GEngine, &UEngine::GetDisplayGamma ) );
                PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateUObject(
                    MyParam.Get(), &UHoudiniAssetParameterColor::OnPaintColorChanged, true, true );
                PickerArgs.InitialColorOverride = MyParam->GetColor();
                PickerArgs.bOnlyRefreshOnOk = true;
                OpenColorPicker( PickerArgs );
                return FReply::Handled();
            } ))
    ];

    if ( ColorBlock.IsValid() )
        ColorBlock->SetEnabled( !InParam.bIsDisabled );

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
FHoudiniParameterDetails::CreateWidgetToggle( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterToggle& InParam )
{
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, false );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    for ( int32 Idx = 0; Idx < InParam.GetTupleSize(); ++Idx )
    {
        TSharedPtr< SCheckBox > CheckBox;

        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
        [
            SAssignNew( CheckBox, SCheckBox )
            .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetParameterToggle::CheckStateChanged, Idx ) )
            .IsChecked( TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetParameterToggle::IsChecked, Idx ) ) )
            .Content()
            [
                SNew( STextBlock )
                .Text( ParameterLabelText )
                .ToolTipText( GetParameterTooltip( &InParam ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];

        if ( CheckBox.IsValid() )
            CheckBox->SetEnabled( !InParam.bIsDisabled );
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
FHoudiniParameterDetails::CreateWidgetToggle( TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameterToggle& InParam )
{
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    FText ParameterTooltip = GetParameterTooltip( &InParam );

    for ( int32 Idx = 0; Idx < InParam.GetTupleSize(); ++Idx )
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
                    &InParam, &UHoudiniAssetParameterToggle::CheckStateChanged, Idx ) )
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetParameterToggle::IsChecked, Idx ) ) )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( ParameterLabelText )
                    .ToolTipText( ParameterTooltip )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
            ]
        ];
    }
}


void
FHoudiniParameterDetails::CreateWidgetFloat( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFloat& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterFloat> MyParam( &InParam );

    /** Should we swap Y and Z fields (only relevant for Vector3) */
    bool SwappedAxis3Vector = false;
    if ( auto Settings = UHoudiniRuntimeSettings::StaticClass()->GetDefaultObject<UHoudiniRuntimeSettings>() )
    {
        SwappedAxis3Vector = InParam.GetTupleSize() == 3 && Settings->ImportAxis == HRSAI_Unreal;
    }

    if ( SwappedAxis3Vector )
    {
        // Ignore the swapping if that parameter has the noswap tag
        if ( InParam.NoSwap )
            SwappedAxis3Vector = false;
    }

    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    // Helper function to find a unit from a string (name or abbreviation) 
    auto ParmUnit = FUnitConversion::UnitFromString( *InParam.ValueUnit );

    TSharedPtr<INumericTypeInterface<float>> TypeInterface;
    if ( FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet() )
    {
        TypeInterface = MakeShareable( new TNumericUnitTypeInterface<float>( ParmUnit.GetValue() ) );
    }

    if ( InParam.GetTupleSize() == 3 )
    {
        Row.ValueWidget.Widget = SNew( SVectorInputBox )
            .bColorAxisLabels( true )
            .X( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject( &InParam, &UHoudiniAssetParameterFloat::GetValue, 0 ) ) )
            .Y( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject( &InParam, &UHoudiniAssetParameterFloat::GetValue, SwappedAxis3Vector ? 2 : 1 ) ) )
            .Z( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject( &InParam, &UHoudiniAssetParameterFloat::GetValue, SwappedAxis3Vector ? 1 : 2 ) ) )
            .OnXCommitted( FOnFloatValueCommitted::CreateLambda(
                [=]( float Val, ETextCommit::Type TextCommitType ) {
                MyParam->SetValue( Val, 0, true, true );
            } ) )
            .OnYCommitted( FOnFloatValueCommitted::CreateLambda(
                [=]( float Val, ETextCommit::Type TextCommitType ) {
                MyParam->SetValue( Val, SwappedAxis3Vector ? 2 : 1, true, true );
            } ) )
            .OnZCommitted( FOnFloatValueCommitted::CreateLambda(
                [=]( float Val, ETextCommit::Type TextCommitType ) {
                MyParam->SetValue( Val, SwappedAxis3Vector ? 1 : 2, true, true );
            } ) )
            .TypeInterface( TypeInterface );
    }
    else
    {
        TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

        for ( int32 Idx = 0; Idx < InParam.GetTupleSize(); ++Idx )
        {
            TSharedPtr< SNumericEntryBox< float > > NumericEntryBox;

            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
            [
                SAssignNew( NumericEntryBox, SNumericEntryBox< float > )
                .AllowSpin( true )

                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )

                .MinValue( InParam.ValueMin )
                .MaxValue( InParam.ValueMax )

                .MinSliderValue( InParam.ValueUIMin )
                .MaxSliderValue( InParam.ValueUIMax )

                .Value( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetParameterFloat::GetValue, Idx ) ) )
                .OnValueChanged( SNumericEntryBox< float >::FOnValueChanged::CreateLambda(
                    [=]( float Val ) {
                    MyParam->SetValue( Val, Idx, false, false );
                } ) )
                .OnValueCommitted( SNumericEntryBox< float >::FOnValueCommitted::CreateLambda(
                    [=]( float Val, ETextCommit::Type TextCommitType ) {
                    MyParam->SetValue( Val, Idx, true, true );
                } ) )
                .OnBeginSliderMovement( FSimpleDelegate::CreateUObject(
                    &InParam, &UHoudiniAssetParameterFloat::OnSliderMovingBegin, Idx ) )
                .OnEndSliderMovement( SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetParameterFloat::OnSliderMovingFinish, Idx ) )

                .SliderExponent( 1.0f )
                .TypeInterface( TypeInterface )
            ];
        }

        Row.ValueWidget.Widget = VerticalBox;
    }
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
    Row.ValueWidget.Widget->SetEnabled( !InParam.bIsDisabled );
}

void 
FHoudiniParameterDetails::CreateWidgetInt( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterInt& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterInt> MyParam( &InParam );
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    // Helper function to find a unit from a string (name or abbreviation) 
    auto ParmUnit = FUnitConversion::UnitFromString( *InParam.ValueUnit );

    TSharedPtr<INumericTypeInterface<int32>> TypeInterface;
    if ( FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet() )
    {
        TypeInterface = MakeShareable( new TNumericUnitTypeInterface<int32>( ParmUnit.GetValue() ) );
    }

    for ( int32 Idx = 0; Idx < InParam.GetTupleSize(); ++Idx )
    {
        TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;

        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
            [
                SAssignNew( NumericEntryBox, SNumericEntryBox< int32 > )
                .AllowSpin( true )

            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )

            .MinValue( InParam.ValueMin )
            .MaxValue( InParam.ValueMax )

            .MinSliderValue( InParam.ValueUIMin )
            .MaxSliderValue( InParam.ValueUIMax )

            .Value( TAttribute< TOptional< int32 > >::Create(
                TAttribute< TOptional< int32 > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetParameterInt::GetValue, Idx ) ) )
            .OnValueChanged( SNumericEntryBox< int32 >::FOnValueChanged::CreateLambda(
                [=]( int32 Val ) {
                MyParam->SetValue( Val, Idx, false, false );
            } ) )
            .OnValueCommitted( SNumericEntryBox< int32 >::FOnValueCommitted::CreateLambda(
                [=]( float Val, ETextCommit::Type TextCommitType ) {
                MyParam->SetValue( Val, Idx, true, true );
            } ) )
            .OnBeginSliderMovement( FSimpleDelegate::CreateUObject(
                &InParam, &UHoudiniAssetParameterInt::OnSliderMovingBegin, Idx ) )
            .OnEndSliderMovement( SNumericEntryBox< int32 >::FOnValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetParameterInt::OnSliderMovingFinish, Idx ) )

            .SliderExponent( 1.0f )
            .TypeInterface( TypeInterface )
            ];

        if ( NumericEntryBox.IsValid() )
            NumericEntryBox->SetEnabled( !InParam.bIsDisabled );
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
FHoudiniParameterDetails::CreateWidgetInstanceInput( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInstanceInput& InParam )
{
    TWeakObjectPtr<UHoudiniAssetInstanceInput> MyParam(&InParam);

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = LocalDetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

    // Classes allowed by instanced inputs.
    const TArray< const UClass * > AllowedClasses = { 
        UStaticMesh::StaticClass(), AActor::StaticClass(), UBlueprint::StaticClass(), 
        USoundBase::StaticClass(), UParticleSystem::StaticClass(),  USkeletalMesh::StaticClass() };

    const int32 FieldCount = InParam.InstanceInputFields.Num();
    for ( int32 FieldIdx = 0; FieldIdx < FieldCount; ++FieldIdx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InParam.InstanceInputFields[ FieldIdx ];
        const int32 VariationCount = HoudiniAssetInstanceInputField->InstanceVariationCount();

        for( int32 VariationIdx = 0; VariationIdx < VariationCount; VariationIdx++ )
        {
            UObject * InstancedObject = HoudiniAssetInstanceInputField->GetInstanceVariation( VariationIdx );
            if ( !InstancedObject )
            {
                HOUDINI_LOG_WARNING( TEXT("Null Object found for instance variation %d"), VariationIdx );
                continue;
            }

            // Create thumbnail for this object.
            TSharedPtr< FAssetThumbnail > StaticMeshThumbnail =
                MakeShareable( new FAssetThumbnail( InstancedObject, 64, 64, AssetThumbnailPool ) );
            TSharedRef< SVerticalBox > PickerVerticalBox = SNew( SVerticalBox );
            TSharedPtr< SHorizontalBox > PickerHorizontalBox = nullptr;
            TSharedPtr< SBorder > StaticMeshThumbnailBorder;

            FString FieldLabel = InParam.GetFieldLabel(FieldIdx, VariationIdx);
            
            IDetailGroup& DetailGroup = LocalDetailCategoryBuilder.AddGroup(FName(*FieldLabel), FText::FromString(FieldLabel));
            DetailGroup.AddWidgetRow()
            .NameContent()
            [
                SNew(SSpacer)
                .Size(FVector2D(250, 64))
            ]
            .ValueContent()
            .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                PickerVerticalBox
            ];
            TWeakObjectPtr<UHoudiniAssetInstanceInputField> InputFieldPtr( HoudiniAssetInstanceInputField );
            PickerVerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
            [
                SNew( SAssetDropTarget )
                .OnIsAssetAcceptableForDrop( SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda( 
                    [=]( const UObject* Obj ) {
                        for ( auto Klass : AllowedClasses )
                        {
                            if ( Obj && Obj->IsA( Klass ) )
                                return true;
                        }
                        return false;
                    })
                )
                .OnAssetDropped( SAssetDropTarget::FOnAssetDropped::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::OnStaticMeshDropped, HoudiniAssetInstanceInputField, FieldIdx, VariationIdx ) )
                [
                    SAssignNew( PickerHorizontalBox, SHorizontalBox )
                ]
            ];

            PickerHorizontalBox->AddSlot().Padding( 0.0f, 0.0f, 2.0f, 0.0f ).AutoWidth()
            [
                SAssignNew( StaticMeshThumbnailBorder, SBorder )
                .Padding( 5.0f )
                .OnMouseDoubleClick( FPointerEventHandler::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::OnThumbnailDoubleClick, InstancedObject ) )
                [
                    SNew( SBox )
                    .WidthOverride( 64 )
                    .HeightOverride( 64 )
                    .ToolTipText( FText::FromString( InstancedObject->GetPathName() ) )
                    [
                        StaticMeshThumbnail->MakeThumbnailWidget()
                    ]
                ]
            ];

            StaticMeshThumbnailBorder->SetBorderImage( TAttribute< const FSlateBrush * >::Create(
                TAttribute< const FSlateBrush * >::FGetter::CreateLambda([=]() {
                    
                    if ( StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered() )
                        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
                    else
                        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
            } ) ) );

            PickerHorizontalBox->AddSlot().AutoWidth().Padding( 0.0f, 28.0f, 0.0f, 28.0f )
            [
                PropertyCustomizationHelpers::MakeAddButton(
                    FSimpleDelegate::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::OnAddInstanceVariation,
                        HoudiniAssetInstanceInputField, VariationIdx ),
                    LOCTEXT("AddAnotherInstanceToolTip", "Add Another Instance" ) )
            ];

            PickerHorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 28.0f, 4.0f, 28.0f )
            [
                PropertyCustomizationHelpers::MakeRemoveButton(
                    FSimpleDelegate::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::OnRemoveInstanceVariation,
                        HoudiniAssetInstanceInputField, VariationIdx ),
                    LOCTEXT("RemoveLastInstanceToolTip", "Remove Last Instance"))
            ];

            TSharedPtr< SComboButton > AssetComboButton;
            TSharedPtr< SHorizontalBox > ButtonBox;

            PickerHorizontalBox->AddSlot()
                .FillWidth( 10.0f )
                .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
                .VAlign( VAlign_Center )
                [
                    SNew( SVerticalBox )
                    +SVerticalBox::Slot()
                    .HAlign( HAlign_Fill )
                    [
                        SAssignNew( ButtonBox, SHorizontalBox )
                        +SHorizontalBox::Slot()
                        [
                            SAssignNew( AssetComboButton, SComboButton )
                            //.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
                            .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                            .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
                            .OnMenuOpenChanged( FOnIsOpenChanged::CreateUObject(
                                &InParam, &UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton,
                                HoudiniAssetInstanceInputField, FieldIdx, VariationIdx ) )
                            .ContentPadding( 2.0f )
                            .ButtonContent()
                            [
                                SNew( STextBlock )
                                .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                                .Font( FEditorStyle::GetFontStyle( FName( TEXT( "PropertyWindow.NormalFont" ) ) ) )
                                .Text( FText::FromString(InstancedObject->GetName() ) )
                            ]
                        ]
                    ]
                ];

            // Create asset picker for this combo button.
            {
                TArray< UFactory * > NewAssetFactories;
                TSharedRef< SWidget > PropertyMenuAssetPicker =
                    PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
                        FAssetData( InstancedObject ), true,
                        AllowedClasses, NewAssetFactories, FOnShouldFilterAsset(),
                        FOnAssetSelected::CreateLambda( [=]( const FAssetData& AssetData ) {
                            if ( AssetComboButton.IsValid() && MyParam.IsValid() && InputFieldPtr.IsValid() )
                            {
                                AssetComboButton->SetIsOpen( false );
                                UObject * Object = AssetData.GetAsset();
                                MyParam->OnStaticMeshDropped( Object, InputFieldPtr.Get(), FieldIdx, VariationIdx );
                            }
                        }),
                        FSimpleDelegate::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::CloseStaticMeshComboButton,
                            HoudiniAssetInstanceInputField, FieldIdx, VariationIdx ) );

                AssetComboButton->SetMenuContent( PropertyMenuAssetPicker );
            }

            // Create tooltip.
            FFormatNamedArguments Args;
            Args.Add( TEXT( "Asset" ), FText::FromString( InstancedObject->GetName() ) );
            FText StaticMeshTooltip =
                FText::Format( LOCTEXT( "BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser" ), Args );

            ButtonBox->AddSlot()
            .AutoWidth()
            .Padding( 2.0f, 0.0f )
            .VAlign( VAlign_Center )
            [
                PropertyCustomizationHelpers::MakeBrowseButton(
                    FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInstanceInput::OnInstancedObjectBrowse, InstancedObject ),
                    TAttribute< FText >( StaticMeshTooltip ) )
            ];

            ButtonBox->AddSlot()
            .AutoWidth()
            .Padding( 2.0f, 0.0f )
            .VAlign( VAlign_Center )
            [
                SNew( SButton )
                .ToolTipText( LOCTEXT( "ResetToBase", "Reset to default static mesh" ) )
                .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                .ContentPadding( 0 )
                .Visibility( EVisibility::Visible )
                .OnClicked( FOnClicked::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::OnResetStaticMeshClicked,
                    HoudiniAssetInstanceInputField, FieldIdx, VariationIdx ) )
                [
                    SNew( SImage )
                    .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
                ]
            ];

            TSharedRef< SVerticalBox > OffsetVerticalBox = SNew( SVerticalBox );
            FText LabelRotationText = LOCTEXT( "HoudiniRotationOffset", "Rotation Offset" );
            DetailGroup.AddWidgetRow()
            .NameContent()
            [
                SNew( STextBlock )
                .Text( LabelRotationText )
                .ToolTipText( LabelRotationText )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            .ValueContent()
            .MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH - 17 )
            [
                SNew( SHorizontalBox )
                + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH - 17 )
                [
                    SNew( SRotatorInputBox )
                    .AllowSpin( true )
                    .bColorAxisLabels( true )
                    .Roll( TAttribute< TOptional< float > >::Create(
                        TAttribute< TOptional< float > >::FGetter::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::GetRotationRoll, HoudiniAssetInstanceInputField, VariationIdx ) ) )
                    .Pitch( TAttribute< TOptional< float> >::Create(
                        TAttribute< TOptional< float > >::FGetter::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::GetRotationPitch, HoudiniAssetInstanceInputField, VariationIdx ) ) )
                    .Yaw( TAttribute<TOptional< float > >::Create(
                        TAttribute< TOptional< float > >::FGetter::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::GetRotationYaw, HoudiniAssetInstanceInputField, VariationIdx ) ) )
                    .OnRollChanged( FOnFloatValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::SetRotationRoll, HoudiniAssetInstanceInputField, VariationIdx ) )
                    .OnPitchChanged( FOnFloatValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::SetRotationPitch, HoudiniAssetInstanceInputField, VariationIdx ) )
                    .OnYawChanged( FOnFloatValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::SetRotationYaw, HoudiniAssetInstanceInputField, VariationIdx ) )
                ]
            ];

            FText LabelScaleText = LOCTEXT( "HoudiniScaleOffset", "Scale Offset" );
            
            DetailGroup.AddWidgetRow()
            .NameContent()
            [
                SNew( STextBlock )
                .Text( LabelScaleText )
                .ToolTipText( LabelScaleText )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            .ValueContent()
            .MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SHorizontalBox )
                + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
                [
                    SNew( SVectorInputBox )
                    .bColorAxisLabels( true )
                    .X( TAttribute< TOptional< float > >::Create(
                        TAttribute< TOptional< float > >::FGetter::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::GetScaleX, HoudiniAssetInstanceInputField, VariationIdx ) ) )
                    .Y( TAttribute< TOptional< float > >::Create(
                        TAttribute< TOptional< float> >::FGetter::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::GetScaleY, HoudiniAssetInstanceInputField, VariationIdx ) ) )
                    .Z( TAttribute< TOptional< float> >::Create(
                        TAttribute< TOptional< float > >::FGetter::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::GetScaleZ, HoudiniAssetInstanceInputField, VariationIdx ) ) )
                    .OnXChanged( FOnFloatValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::SetScaleX, HoudiniAssetInstanceInputField, VariationIdx ) )
                    .OnYChanged( FOnFloatValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::SetScaleY, HoudiniAssetInstanceInputField, VariationIdx ) )
                    .OnZChanged( FOnFloatValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::SetScaleZ, HoudiniAssetInstanceInputField, VariationIdx ) )
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    // Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
                    SNew( SCheckBox )
                    .Style( FEditorStyle::Get(), "TransparentCheckBox" )
                    .ToolTipText( LOCTEXT( "PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
                    .OnCheckStateChanged( FOnCheckStateChanged::CreateLambda( [=]( ECheckBoxState NewState ) {
                        if ( MyParam.IsValid() && InputFieldPtr.IsValid() )
                            MyParam->CheckStateChanged( NewState == ECheckBoxState::Checked, InputFieldPtr.Get(), VariationIdx );
                    }))
                    .IsChecked( TAttribute< ECheckBoxState >::Create(
                        TAttribute<ECheckBoxState>::FGetter::CreateLambda( [=]() {
                            if ( InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly( VariationIdx ) )
                                return ECheckBoxState::Checked;
                            return ECheckBoxState::Unchecked;
                    })))
                    [
                        SNew( SImage )
                        .Image( TAttribute<const FSlateBrush*>::Create(
                            TAttribute<const FSlateBrush*>::FGetter::CreateLambda( [=]() {
                            if ( InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly( VariationIdx ) )
                            {
                                return FEditorStyle::GetBrush( TEXT( "GenericLock" ) );
                            }
                            return FEditorStyle::GetBrush( TEXT( "GenericUnlock" ) );
                        })))
                        .ColorAndOpacity( FSlateColor::UseForeground() )
                    ]
                ]
            ];
        }
    }
}

FMenuBuilder
FHoudiniParameterDetails::Helper_CreateCustomActorPickerWidget( UHoudiniAssetInput& InParam, const TAttribute<FText>& HeadingText, const bool& bShowCurrentSelectionSection )
{
    // Custom Actor Picker showing only the desired Actor types.
    // Note: Code stolen from SPropertyMenuActorPicker.cpp
    FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateUObject( &InParam, &UHoudiniAssetInput::OnShouldFilterActor );

    //FHoudiniEngineEditor & HoudiniEngineEditor = FHoudiniEngineEditor::Get();
    //TSharedPtr< ISlateStyle > StyleSet = GEditor->GetSlateStyle();

    FMenuBuilder MenuBuilder( true, NULL );

    if ( bShowCurrentSelectionSection )
    {
        MenuBuilder.BeginSection( NAME_None, LOCTEXT( "CurrentActorOperationsHeader", "Current Selection" ) );
        {
            MenuBuilder.AddMenuEntry(
                TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateUObject( &InParam, &UHoudiniAssetInput::GetCurrentSelectionText ) ),
                TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateUObject( &InParam, &UHoudiniAssetInput::GetCurrentSelectionText ) ),
                FSlateIcon(),//StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo")),
                FUIAction(),
                NAME_None,
                EUserInterfaceActionType::Button,
                NAME_None );
        }
        MenuBuilder.EndSection();
    }

    MenuBuilder.BeginSection( NAME_None, HeadingText );
    {
        TSharedPtr< SWidget > MenuContent;

        FSceneOutlinerModule & SceneOutlinerModule =
            FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >( TEXT( "SceneOutliner" ) );

        SceneOutliner::FInitializationOptions InitOptions;
        InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
        InitOptions.Filters->AddFilterPredicate( ActorFilter );
        InitOptions.bFocusSearchBoxWhenOpened = true;

        static const FVector2D SceneOutlinerWindowSize( 350.0f, 200.0f );
        MenuContent =
            SNew( SBox )
            .WidthOverride( SceneOutlinerWindowSize.X )
            .HeightOverride( SceneOutlinerWindowSize.Y )
            [
                SNew( SBorder )
                .BorderImage( FEditorStyle::GetBrush( "Menu.Background" ) )
            [
                SceneOutlinerModule.CreateSceneOutliner(
                    InitOptions, FOnActorPicked::CreateUObject(
                        &InParam, &UHoudiniAssetInput::OnActorSelected ) )
            ]
            ];

        MenuBuilder.AddWidget( MenuContent.ToSharedRef(), FText::GetEmpty(), true );
    }
    MenuBuilder.EndSection();

    return MenuBuilder;
}

/** Create a single geometry widget for the given input object */
void FHoudiniParameterDetails::Helper_CreateGeometryWidget(
    UHoudiniAssetInput& InParam, int32 AtIndex, UObject* InputObject,
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool, TSharedRef< SVerticalBox > VerticalBox )
{
    TWeakObjectPtr<UHoudiniAssetInput> MyParam( &InParam );

    // Create thumbnail for this static mesh.
    TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
        new FAssetThumbnail( InputObject, 64, 64, AssetThumbnailPool ) );

    TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
    // Drop Target: Static Mesh
    VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
    [
        SNew( SAssetDropTarget )
        .OnIsAssetAcceptableForDrop( SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda(
            []( const UObject* InObject )
                { return InObject && ( InObject->IsA< UStaticMesh >() || InObject->IsA< USkeletalMesh >() ); } 
        ) )
        .OnAssetDropped( SAssetDropTarget::FOnAssetDropped::CreateUObject(
            &InParam, &UHoudiniAssetInput::OnStaticMeshDropped, AtIndex ) )
        [
            SAssignNew( HorizontalBox, SHorizontalBox )
        ]
    ];

    // Thumbnail : Static Mesh
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    //FText ParameterTooltip = GetParameterTooltip( &InParam, true );
    TSharedPtr< SBorder > StaticMeshThumbnailBorder;

    HorizontalBox->AddSlot().Padding( 0.0f, 0.0f, 2.0f, 0.0f ).AutoWidth()
    [
        SAssignNew( StaticMeshThumbnailBorder, SBorder )
        .Padding( 5.0f )
        .OnMouseDoubleClick( FPointerEventHandler::CreateUObject( &InParam, &UHoudiniAssetInput::OnThumbnailDoubleClick, AtIndex ) )
        [
            SNew( SBox )
            .WidthOverride( 64 )
            .HeightOverride( 64 )
            .ToolTipText( ParameterLabelText )
            [
                StaticMeshThumbnail->MakeThumbnailWidget()
            ]
        ]
    ];

    StaticMeshThumbnailBorder->SetBorderImage( TAttribute< const FSlateBrush * >::Create(
        TAttribute< const FSlateBrush * >::FGetter::CreateLambda( [StaticMeshThumbnailBorder]()
        {
            if ( StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered() )
                return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
            else
                return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
        }
    ) ) );

    FText MeshNameText = FText::GetEmpty();
    if ( InputObject )
        MeshNameText = FText::FromString( InputObject->GetName() );

    // ComboBox : Static Mesh
    TSharedPtr< SComboButton > StaticMeshComboButton;

    TSharedPtr< SHorizontalBox > ButtonBox;
    HorizontalBox->AddSlot()
        .FillWidth( 1.0f )
        .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
        .VAlign( VAlign_Center )
        [
            SNew( SVerticalBox )
            + SVerticalBox::Slot()
            .HAlign( HAlign_Fill )
            [
                SAssignNew( ButtonBox, SHorizontalBox )
                + SHorizontalBox::Slot()
                [
                    SAssignNew( StaticMeshComboButton, SComboButton )
                    .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                    .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
                    .ContentPadding( 2.0f )
                    .ButtonContent()
                    [
                        SNew( STextBlock )
                        .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                        .Font( FEditorStyle::GetFontStyle( FName( TEXT( "PropertyWindow.NormalFont" ) ) ) )
                        .Text( MeshNameText )
                    ]
                ]
            ]
        ];

    StaticMeshComboButton->SetOnGetMenuContent( FOnGetContent::CreateLambda(
        [ MyParam, AtIndex, StaticMeshComboButton ]()
        {
            TArray< const UClass * > AllowedClasses;
            AllowedClasses.Add( UStaticMesh::StaticClass() );
            AllowedClasses.Add( USkeletalMesh::StaticClass() );

            TArray< UFactory * > NewAssetFactories;
            return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
                FAssetData( MyParam->GetInputObject( AtIndex ) ),
                true,
                AllowedClasses,
                NewAssetFactories,
                FOnShouldFilterAsset(),
                FOnAssetSelected::CreateLambda( [MyParam, AtIndex, StaticMeshComboButton]( const FAssetData & AssetData ) {
                    if ( StaticMeshComboButton.IsValid() )
                    {
                        StaticMeshComboButton->SetIsOpen( false );

                        UObject * Object = AssetData.GetAsset();
                        MyParam->OnStaticMeshDropped( Object, AtIndex );
                    }
                } ),
                FSimpleDelegate::CreateLambda( []() {} ) );
        } ) );

    // Create tooltip.
    FFormatNamedArguments Args;
    Args.Add( TEXT( "Asset" ), MeshNameText );
    FText StaticMeshTooltip = FText::Format(
        LOCTEXT( "BrowseToSpecificAssetInContentBrowser",
            "Browse to '{Asset}' in Content Browser" ), Args );

    // Button : Browse Static Mesh
    ButtonBox->AddSlot()
        .AutoWidth()
        .Padding( 2.0f, 0.0f )
        .VAlign( VAlign_Center )
        [
            PropertyCustomizationHelpers::MakeBrowseButton(
                FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInput::OnStaticMeshBrowse, AtIndex ),
                TAttribute< FText >( StaticMeshTooltip ) )
        ];

    // ButtonBox : Reset
    ButtonBox->AddSlot()
        .AutoWidth()
        .Padding( 2.0f, 0.0f )
        .VAlign( VAlign_Center )
        [
            SNew( SButton )
            .ToolTipText( LOCTEXT( "ResetToBase", "Reset to default static mesh" ) )
            .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
            .ContentPadding( 0 )
            .Visibility( EVisibility::Visible )
            .OnClicked( FOnClicked::CreateUObject( &InParam, &UHoudiniAssetInput::OnResetStaticMeshClicked, AtIndex ) )
            [
                SNew( SImage )
                .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
            ]
        ];

    ButtonBox->AddSlot()
        .Padding( 1.0f )
        .VAlign( VAlign_Center )
        .AutoWidth()
        [
            PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
            FExecuteAction::CreateLambda( [ MyParam, AtIndex ]() 
                {
                    MyParam->OnInsertGeo( AtIndex );
                } 
            ),
            FExecuteAction::CreateLambda( [MyParam, AtIndex ]()
                {
                    MyParam->OnDeleteGeo( AtIndex );
                } 
            ),
            FExecuteAction::CreateLambda( [ MyParam, AtIndex ]()
                {
                    MyParam->OnDuplicateGeo( AtIndex );
                } 
            ) )
        ];
    
    {
        TSharedPtr<SButton> ExpanderArrow;
        TSharedPtr<SImage> ExpanderImage;
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( ExpanderArrow, SButton )
                .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                .ClickMethod( EButtonClickMethod::MouseDown )
                .Visibility( EVisibility::Visible )
                .OnClicked( FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnExpandInputTransform, AtIndex ) )
                [
                    SAssignNew( ExpanderImage, SImage )
                    .ColorAndOpacity( FSlateColor::UseForeground() )
                ]
            ]
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT("GeoInputTransform", "Transform Offset") )
                .ToolTipText( LOCTEXT( "GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];
        // Set delegate for image
        ExpanderImage->SetImage(
            TAttribute<const FSlateBrush*>::Create(
                TAttribute<const FSlateBrush*>::FGetter::CreateLambda( [=]() {
            FName ResourceName;
            if ( MyParam->TransformUIExpanded[ AtIndex ] )
            {
                if ( ExpanderArrow->IsHovered() )
                    ResourceName = "TreeArrow_Expanded_Hovered";
                else
                    ResourceName = "TreeArrow_Expanded";
            }
            else
            {
                if ( ExpanderArrow->IsHovered() )
                    ResourceName = "TreeArrow_Collapsed_Hovered";
                else
                    ResourceName = "TreeArrow_Collapsed";
            }
            return FEditorStyle::GetBrush( ResourceName );
        } ) ) );
    }

    // TRANSFORM 
    if ( InParam.TransformUIExpanded[ AtIndex ] )
    {
        // Position
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text( LOCTEXT("GeoInputTranslate", "T") )
                .ToolTipText( LOCTEXT( "GeoInputTranslateTooltip", "Translate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
                .X( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetPositionX, AtIndex ) ) )
                .Y( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float> >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetPositionY, AtIndex ) ) )
                .Z( TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetPositionZ, AtIndex ) ) )
                .OnXChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetPositionX, AtIndex ) )
                .OnYChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetPositionY, AtIndex ) )
                .OnZChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetPositionZ, AtIndex ) )
            ]
        ];

        // Rotation
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text( LOCTEXT("GeoInputRotate", "R") )
                .ToolTipText( LOCTEXT( "GeoInputRotateTooltip", "Rotate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SRotatorInputBox )
                .AllowSpin( true )
                .bColorAxisLabels( true )
                .Roll( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetRotationRoll, AtIndex ) ) )
                .Pitch( TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetRotationPitch, AtIndex) ) )
                .Yaw( TAttribute<TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetRotationYaw, AtIndex) ) )
                .OnRollChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetRotationRoll, AtIndex) )
                .OnPitchChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetRotationPitch, AtIndex) )
                .OnYawChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetRotationYaw, AtIndex) )
            ]
        ];

        // Scale
	VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "GeoInputScale", "S" ) )
                .ToolTipText( LOCTEXT( "GeoInputScaleTooltip", "Scale" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
                .X( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetScaleX, AtIndex ) ) )
                .Y( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float> >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetScaleY, AtIndex ) ) )
                .Z( TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetScaleZ, AtIndex ) ) )
                .OnXChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetScaleX, AtIndex ) )
                .OnYChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetScaleY, AtIndex ) )
                .OnZChanged( FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetScaleZ, AtIndex ) )
            ]
            /*
            + SHorizontalBox::Slot().AutoWidth()
            [
                // Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
                SNew( SCheckBox )
                .Style( FEditorStyle::Get(), "TransparentCheckBox" )
                .ToolTipText( LOCTEXT( "PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChanged, AtIndex ) )
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute<ECheckBoxState>::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::IsChecked, AtIndex ) ) )
                [
                    SNew( SImage )
                    .Image( TAttribute<const FSlateBrush*>::Create(
                        TAttribute<const FSlateBrush*>::FGetter::CreateUObject(
                            this, &UHoudiniAssetInput::GetPreserveScaleRatioImage, AtIndex ) ) )
                    .ColorAndOpacity( FSlateColor::UseForeground() )
                ]
            ]
            */
        ];
    }
}

/** Create a single skeleton widget for the given input object */
void FHoudiniParameterDetails::Helper_CreateSkeletonWidget(
    UHoudiniAssetInput& InParam, int32 AtIndex, UObject* InputObject,
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool, TSharedRef< SVerticalBox > VerticalBox )
{
    TWeakObjectPtr<UHoudiniAssetInput> MyParam( &InParam );

    // Create thumbnail for this skeleton mesh.
    TSharedPtr< FAssetThumbnail > SkeletalMeshThumbnail = MakeShareable(
        new FAssetThumbnail( InputObject, 64, 64, AssetThumbnailPool ) );

    TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
    // Drop Target: Skeletal Mesh
    VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
    [
        SNew( SAssetDropTarget )
        .OnIsAssetAcceptableForDrop( SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda(
            [] ( const UObject* InObject )
            {
                return InObject && InObject->IsA< USkeletalMesh >();
            } 
        ) )
        .OnAssetDropped( SAssetDropTarget::FOnAssetDropped::CreateUObject(
            &InParam, &UHoudiniAssetInput::OnSkeletalMeshDropped, AtIndex ) )
        [
            SAssignNew(HorizontalBox, SHorizontalBox)
        ]
    ];

    // Thumbnail : Skeletal Mesh
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    //FText ParameterTooltip = GetParameterTooltip( &InParam, true );
    TSharedPtr< SBorder > SkeletalMeshThumbnailBorder;

    HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
    [
        SAssignNew( SkeletalMeshThumbnailBorder, SBorder )
        .Padding(5.0f)
        .OnMouseDoubleClick(FPointerEventHandler::CreateUObject(&InParam, &UHoudiniAssetInput::OnThumbnailDoubleClick, AtIndex))
        [
            SNew(SBox)
            .WidthOverride(64)
            .HeightOverride(64)
            .ToolTipText(ParameterLabelText)
            [
                SkeletalMeshThumbnail->MakeThumbnailWidget()
            ]
        ]
    ];

    SkeletalMeshThumbnailBorder->SetBorderImage( TAttribute< const FSlateBrush * >::Create(
    TAttribute< const FSlateBrush * >::FGetter::CreateLambda( [ SkeletalMeshThumbnailBorder ]()
    {
        if ( SkeletalMeshThumbnailBorder.IsValid() && SkeletalMeshThumbnailBorder->IsHovered() )
            return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
        else
            return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
    } ) ) );

    FText MeshNameText = FText::GetEmpty();
    if ( InputObject )
        MeshNameText = FText::FromString( InputObject->GetName() );

    // ComboBox : Skeletal Mesh
    TSharedPtr< SComboButton > SkeletalMeshComboButton;

    TSharedPtr< SHorizontalBox > ButtonBox;
    HorizontalBox->AddSlot()
    .FillWidth( 1.0f )
    .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
    .VAlign( VAlign_Center )
    [
        SNew( SVerticalBox )
        + SVerticalBox::Slot()
        .HAlign( HAlign_Fill )
        [
            SAssignNew( ButtonBox, SHorizontalBox )
            + SHorizontalBox::Slot()
            [
                SAssignNew(SkeletalMeshComboButton, SComboButton )
                .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
                .ContentPadding( 2.0f )
                .ButtonContent()
                [
                    SNew( STextBlock )
                    .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                    .Font( FEditorStyle::GetFontStyle( FName( TEXT( "PropertyWindow.NormalFont" ) ) ) )
                    .Text( MeshNameText )
                ]
            ]
        ]
    ];

    SkeletalMeshComboButton->SetOnGetMenuContent( FOnGetContent::CreateLambda( 
        [ MyParam, AtIndex, SkeletalMeshComboButton ]()
    {
        TArray< const UClass * > AllowedClasses;
        AllowedClasses.Add(USkeletalMesh::StaticClass());

        TArray< UFactory * > NewAssetFactories;
        return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
            FAssetData( MyParam->GetInputObject( AtIndex ) ),
            true,
            AllowedClasses,
            NewAssetFactories,
            FOnShouldFilterAsset(),
            FOnAssetSelected::CreateLambda(
                [ MyParam, AtIndex, SkeletalMeshComboButton]( const FAssetData & AssetData )
            {
                if (SkeletalMeshComboButton.IsValid())
                {
                    SkeletalMeshComboButton->SetIsOpen(false);

                    UObject * Object = AssetData.GetAsset();
                    MyParam->OnSkeletalMeshDropped(Object, AtIndex);
                }
            } ),
            FSimpleDelegate::CreateLambda( [](){}) 
        );
    } ) );

    // Create tooltip.
    FFormatNamedArguments Args;
    Args.Add( TEXT("Asset"), MeshNameText );
    FText SkeletalMeshTooltip = FText::Format(
        LOCTEXT("BrowseToSpecificAssetInContentBrowser",
            "Browse to '{Asset}' in Content Browser"), Args);

    // Button : Browse Skeletal Mesh
    ButtonBox->AddSlot()
    .AutoWidth()
    .Padding( 2.0f, 0.0f )
    .VAlign( VAlign_Center )
    [
        PropertyCustomizationHelpers::MakeBrowseButton(
            FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInput::OnStaticMeshBrowse, AtIndex ),
            TAttribute< FText >( SkeletalMeshTooltip ) )
    ];

    // ButtonBox : Reset
    ButtonBox->AddSlot()
    .AutoWidth()
    .Padding( 2.0f, 0.0f )
    .VAlign( VAlign_Center )
    [
        SNew( SButton )
        .ToolTipText( LOCTEXT( "ResetToBase", "Reset to default skeletal mesh" ) )
        .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
        .ContentPadding( 0 )
        .Visibility( EVisibility::Visible )
        .OnClicked( FOnClicked::CreateUObject( &InParam, &UHoudiniAssetInput::OnResetSkeletalMeshClicked, AtIndex ) )
        [
            SNew( SImage )
            .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
        ]
    ];

    ButtonBox->AddSlot()
    .Padding( 1.0f )
    .VAlign( VAlign_Center )
    .AutoWidth()
    [
        PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
            FExecuteAction::CreateLambda( [ MyParam, AtIndex ](){ MyParam->OnInsertGeo( AtIndex ); } ),
            FExecuteAction::CreateLambda( [ MyParam, AtIndex ](){ MyParam->OnDeleteGeo( AtIndex ); } ),
            FExecuteAction::CreateLambda( [ MyParam, AtIndex ](){ MyParam->OnDuplicateGeo( AtIndex ); } ) )
    ];

    /*
    // TRANSFORM
    {
        TSharedPtr<SButton> ExpanderArrow;
        TSharedPtr<SImage> ExpanderImage;
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( ExpanderArrow, SButton )
                .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                .ClickMethod( EButtonClickMethod::MouseDown )
                .Visibility( EVisibility::Visible )
                .OnClicked( FOnClicked::CreateUObject( &InParam, &UHoudiniAssetInput::OnExpandInputTransform, AtIndex ) )
                [
                    SAssignNew( ExpanderImage, SImage )
                    .ColorAndOpacity( FSlateColor::UseForeground() )
                ]
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT("GeoInputTransform", "Transform Offset" ) )
                .ToolTipText( LOCTEXT( "GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];

        // Set delegate for image
        ExpanderImage->SetImage( TAttribute<const FSlateBrush*>::Create(
            TAttribute<const FSlateBrush*>::FGetter::CreateLambda( [=]()
        {
            FName ResourceName;
            if ( MyParam->TransformUIExpanded[ AtIndex ] )
            {
                if ( ExpanderArrow->IsHovered() )
                    ResourceName = "TreeArrow_Expanded_Hovered";
                else
                    ResourceName = "TreeArrow_Expanded";
            }
            else
            {
                if ( ExpanderArrow->IsHovered() )
                    ResourceName = "TreeArrow_Collapsed_Hovered";
                else
                    ResourceName = "TreeArrow_Collapsed";
            }
            return FEditorStyle::GetBrush( ResourceName );
        } ) ) );
    }

    if ( InParam.TransformUIExpanded[ AtIndex ] )
    {
        // Position
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "GeoInputTranslate", "T" ) )
                .ToolTipText( LOCTEXT( "GeoInputTranslateTooltip", "Translate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
                .X( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject( &InParam, &UHoudiniAssetInput::GetPositionX, AtIndex ) ) )
                .Y(TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject( &InParam, &UHoudiniAssetInput::GetPositionY, AtIndex ) ) )
                .Z(TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject( &InParam, &UHoudiniAssetInput::GetPositionZ, AtIndex ) ) )
                .OnXChanged( FOnFloatValueChanged::CreateUObject( &InParam, &UHoudiniAssetInput::SetPositionX, AtIndex ) )
                .OnYChanged(FOnFloatValueChanged::CreateUObject( &InParam, &UHoudiniAssetInput::SetPositionY, AtIndex ) )
                .OnZChanged(FOnFloatValueChanged::CreateUObject( &InParam, &UHoudiniAssetInput::SetPositionZ, AtIndex ) )
            ]
        ];

        // Rotation
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "GeoInputRotate", "R" ) )
                .ToolTipText(LOCTEXT("GeoInputRotateTooltip", "Rotate") )
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                SNew(SRotatorInputBox)
                .AllowSpin(true)
            .bColorAxisLabels(true)
            .Roll(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetRotationRoll, AtIndex)))
            .Pitch(TAttribute< TOptional< float> >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetRotationPitch, AtIndex)))
            .Yaw(TAttribute<TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetRotationYaw, AtIndex)))
            .OnRollChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetRotationRoll, AtIndex))
            .OnPitchChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetRotationPitch, AtIndex))
            .OnYawChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetRotationYaw, AtIndex))
            ]
        ];

        // Scale
        VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("GeoInputScale", "S"))
            .ToolTipText(LOCTEXT("GeoInputScaleTooltip", "Scale"))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                SNew(SVectorInputBox)
                .bColorAxisLabels(true)
            .X(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetScaleX, AtIndex)))
            .Y(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float> >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetScaleY, AtIndex)))
            .Z(TAttribute< TOptional< float> >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetScaleZ, AtIndex)))
            .OnXChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetScaleX, AtIndex))
            .OnYChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetScaleY, AtIndex))
            .OnZChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetScaleZ, AtIndex))
            ]
        ];
    }
    */
}

FReply
FHoudiniParameterDetails::Helper_OnButtonClickSelectActors( TWeakObjectPtr<class UHoudiniAssetInput> InParam, FName DetailsPanelName )
{
    if ( !InParam.IsValid() )
        return FReply::Handled();

    // There's no undo operation for button.

    FPropertyEditorModule & PropertyModule =
        FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

    // Locate the details panel.
    TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

    if ( !DetailsView.IsValid() )
        return FReply::Handled();

    class SLocalDetailsView : public SDetailsViewBase
    {
    public:
        void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
        void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
    };
    auto * LocalDetailsView = static_cast< SLocalDetailsView * >( DetailsView.Get() );

    if ( !DetailsView->IsLocked() )
    {
        LocalDetailsView->LockDetailsView();
        check( DetailsView->IsLocked() );

        // Force refresh of details view.
        InParam->OnParamStateChanged();

        // Select the previously chosen input Actors from the World Outliner.
        GEditor->SelectNone( false, true );
        for ( auto & OutlinerMesh : InParam->InputOutlinerMeshArray )
        {
            if ( OutlinerMesh.ActorPtr.IsValid() )
                GEditor->SelectActor( OutlinerMesh.ActorPtr.Get(), true, true );
        }

        return FReply::Handled();
    }

    if ( !GEditor || !GEditor->GetSelectedObjects() )
        return FReply::Handled();

    // If details panel is locked, locate selected actors and check if this component belongs to one of them.

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini World Outliner Input Change" ),
        InParam->PrimaryObject );
    InParam->Modify();

    InParam->MarkPreChanged();
    InParam->bStaticMeshChanged = true;

    // Delete all assets and reset the array.
    // TODO: Make this process a little more efficient.
    InParam->DisconnectAndDestroyInputAsset();
    InParam->InputOutlinerMeshArray.Empty();

    USelection * SelectedActors = GEditor->GetSelectedActors();

    // If the builder brush is selected, first deselect it.
    for ( FSelectionIterator It( *SelectedActors ); It; ++It )
    {
        AActor * Actor = Cast< AActor >( *It );
        if ( !Actor )
            continue;

        // Don't allow selection of ourselves. Bad things happen if we do.
        if ( InParam->GetHoudiniAssetComponent() && ( Actor == InParam->GetHoudiniAssetComponent()->GetOwner() ) )
            continue;

        InParam->UpdateInputOulinerArrayFromActor( Actor, false );
    }

    InParam->MarkChanged();

    AActor* HoudiniAssetActor = InParam->GetHoudiniAssetComponent()->GetOwner();

    if ( DetailsView->IsLocked() )
    {
        LocalDetailsView->UnlockDetailsView();
        check( !DetailsView->IsLocked() );

        TArray< UObject * > DummySelectedActors;
        DummySelectedActors.Add( HoudiniAssetActor );

        // Reset selected actor to itself, force refresh and override the lock.
        DetailsView->SetObjects( DummySelectedActors, true, true );
    }

    // Reselect the Asset Actor. If we don't do this, our Asset parameters will stop
    // refreshing and the user will be very confused. It is also resetting the state
    // of the selection before the input actor selection process was started.
    GEditor->SelectNone( false, true );
    GEditor->SelectActor( HoudiniAssetActor, true, true );

    // Update parameter layout.
    InParam->OnParamStateChanged();

    // Start or stop the tick timer to check if the selected Actors have been transformed.
    if ( InParam->InputOutlinerMeshArray.Num() > 0 )
        InParam->StartWorldOutlinerTicking();
    else if ( InParam->InputOutlinerMeshArray.Num() <= 0 )
        InParam->StopWorldOutlinerTicking();

    return FReply::Handled();
}

void 
FHoudiniParameterDetails::CreateWidgetInput( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInput& InParam )
{
    TWeakObjectPtr<UHoudiniAssetInput> MyParam( &InParam );

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = LocalDetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    FText ParameterTooltip = GetParameterTooltip( &InParam );
    Row.NameWidget.Widget =
        SNew( STextBlock )
            .Text( ParameterLabelText )
            .ToolTipText( ParameterTooltip )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    if ( InParam.StringChoiceLabels.Num() > 0 )
    {
        // ComboBox :  Input Type
        TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBoxInputType;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2)
        [
            //SNew(SComboBox<TSharedPtr< FString > >)
            SAssignNew( ComboBoxInputType, SComboBox< TSharedPtr< FString > > )
            .OptionsSource( &InParam.StringChoiceLabels )
            .InitiallySelectedItem( InParam.StringChoiceLabels[ InParam.ChoiceIndex ] )
            .OnGenerateWidget( SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateLambda(
                []( TSharedPtr< FString > ChoiceEntry ) {
                    FText ChoiceEntryText = FText::FromString( *ChoiceEntry );
                    return SNew( STextBlock )
                        .Text( ChoiceEntryText )
                        .ToolTipText( ChoiceEntryText )
                        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
                }))
             .OnSelectionChanged( SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateLambda(
                [=]( TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType ) {
                    if ( !NewChoice.IsValid() || !MyParam.IsValid() )
                            return;
                    MyParam->OnChoiceChange( NewChoice );
                }))
            [
                SNew( STextBlock )
                .Text( TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::HandleChoiceContentText ) ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];

        //ComboBoxInputType->SetSelectedItem( InParam.StringChoiceLabels[ InParam.ChoiceIndex ] );
    }

    // Checkbox : Keep World Transform
    {
        TSharedPtr< SCheckBox > CheckBoxTranformType;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
        [
            SAssignNew(CheckBoxTranformType, SCheckBox)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("KeepWorldTransformCheckbox", "Keep World Transform"))
                .ToolTipText(LOCTEXT("KeepWorldTransformCheckboxTip", "Set this Input's object_merge Transform Type to INTO_THIS_OBJECT. If unchecked, it will be set to NONE."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedKeepWorldTransform)))
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedKeepWorldTransform))
        ];

        // the checkbox is read only for geo inputs
        if ( InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
            CheckBoxTranformType->SetEnabled( false );

        // Checkbox is read only if the input is an object-path parameter
        //if ( bIsObjectPathParameter )
        //    CheckBoxTranformType->SetEnabled(false);
    }

    // Checkbox Pack before merging
    if ( InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput
        || InParam.ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        TSharedPtr< SCheckBox > CheckBoxPackBeforeMerge;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SAssignNew( CheckBoxPackBeforeMerge, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "PackBeforeMergeCheckbox", "Pack Geometry before merging" ) )
                .ToolTipText( LOCTEXT( "PackBeforeMergeCheckboxTip", "Pack each separate piece of geometry before merging them into the input." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            .IsChecked( TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedPackBeforeMerge ) ) )
            .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedPackBeforeMerge ) )
        ];
    }

    // Checkboxes Export LODs and Export Sockets
    if ( InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput
        || InParam.ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        /*
        // Add a checkbox to export all lods
        TSharedPtr< SCheckBox > CheckBoxExportAllLODs;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SAssignNew( CheckBoxExportAllLODs, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "ExportAllLOD", "Export all LODs" ) )
                .ToolTipText( LOCTEXT( "ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            .IsChecked( TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedExportAllLODs ) ) )
            .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedExportAllLODs ) )
        ];

        // the checkbox is read only if the input doesnt have LODs
        CheckBoxExportAllLODs->SetEnabled( InParam.HasLODs() );

        // Add a checkbox to export sockets
        TSharedPtr< SCheckBox > CheckBoxExportSockets;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SAssignNew( CheckBoxExportSockets, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "ExportSockets", "Export Sockets" ) )
                .ToolTipText( LOCTEXT( "ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            .IsChecked( TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedExportSockets ) ) )
            .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedExportSockets ) )
        ];

        // the checkbox is read only if the input doesnt have LODs
        CheckBoxExportSockets->SetEnabled( InParam.HasSockets() );
        */

        TSharedPtr< SCheckBox > CheckBoxExportAllLODs;
        TSharedPtr< SCheckBox > CheckBoxExportSockets;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( CheckBoxExportAllLODs, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "ExportAllLOD", "Export LODs" ) )
                    .ToolTipText( LOCTEXT( "ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportAllLODs ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAllLODs ) )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( CheckBoxExportSockets, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "ExportSockets", "Export Sockets" ) )
                    .ToolTipText( LOCTEXT( "ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportSockets ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportSockets ) )
            ]
        ];
    }

    if ( InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
    {
        const int32 NumInputs = InParam.InputObjects.Num();
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( FText::Format( LOCTEXT( "NumArrayItemsFmt", "{0} elements" ), FText::AsNumber( NumInputs ) ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInput::OnAddToInputObjects ), LOCTEXT( "AddInput", "Adds a Geometry Input" ), true )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInput::OnEmptyInputObjects ), LOCTEXT( "EmptyInputs", "Removes All Inputs" ), true )
            ]
        ];

        for ( int32 Ix = 0; Ix < NumInputs; Ix++ )
        {
            UObject* InputObject = InParam.GetInputObject( Ix );
            Helper_CreateGeometryWidget( InParam, Ix, InputObject, AssetThumbnailPool, VerticalBox );
        }
    }
    else if ( InParam.ChoiceIndex == EHoudiniAssetInputType::AssetInput )
    {
        // ActorPicker : Houdini Asset
        FMenuBuilder MenuBuilder = Helper_CreateCustomActorPickerWidget( InParam, LOCTEXT( "AssetInputSelectableActors", "Houdini Asset Actors" ), true );

        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
        [
            MenuBuilder.MakeWidget()
        ];
    }
    else if ( InParam.ChoiceIndex == EHoudiniAssetInputType::CurveInput )
    {
        // Go through all input curve parameters and build their widgets recursively.
        for ( TMap< FString, UHoudiniAssetParameter * >::TIterator
            IterParams( InParam.InputCurveParameters ); IterParams; ++IterParams )
        {
            // Note: Only choice and toggle supported atm
            if ( UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value() )
                FHoudiniParameterDetails::CreateWidget( VerticalBox, HoudiniAssetParameter );
        }
    }
    else if ( InParam.ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
    {
        // ActorPicker : Landscape
        FMenuBuilder MenuBuilder = Helper_CreateCustomActorPickerWidget( InParam, LOCTEXT( "LandscapeInputSelectableActors", "Landscapes" ), true );
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
        [
            MenuBuilder.MakeWidget()
        ];

        // Checkboxes : Export Landscape As
        //		Heightfield Mesh Points
        {
            // Label
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("LandscapeExportAs", "Export Landscape As"))
                .ToolTipText(LOCTEXT("LandscapeExportAsTooltip", "Choose the type of data you want the landscape to be exported to:\n * Heightfield\n * Mesh\n * Points"))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ];

            //
            TSharedPtr<SUniformGridPanel> ButtonOptionsPanel;
            VerticalBox->AddSlot().Padding(5, 2, 5, 2).AutoHeight()
            [
                SAssignNew( ButtonOptionsPanel, SUniformGridPanel )
            ];

            // Heightfield
            FText HeightfieldTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heighfield.");
            ButtonOptionsPanel->AddSlot(0, 0)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportAsHeightfield ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAsHeightfield ) )
                .ToolTipText( HeightfieldTooltip )
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush("ClassIcon.LandscapeComponent"))
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(STextBlock)
                        .Text( LOCTEXT( "LandscapeExportAsHeightfieldCheckbox", "Heightfield" ) )
                        .ToolTipText( HeightfieldTooltip )
                        .Font( IDetailLayoutBuilder::GetDetailFont() )
                    ]
                ]
            ];

            // Mesh
            FText MeshTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heighfield.");
            ButtonOptionsPanel->AddSlot(1, 0)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportAsMesh ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAsMesh ) )
                .ToolTipText( MeshTooltip )
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush( "ClassIcon.StaticMeshComponent" ) )
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew( STextBlock )
                        .Text( LOCTEXT( "LandscapeExportAsMeshCheckbox", "Mesh" ) )
                        .ToolTipText( MeshTooltip )
                        .Font( IDetailLayoutBuilder::GetDetailFont() )
                    ]
                ]
            ];

            // Points
            FText PointsTooltip = LOCTEXT( "LandscapeExportAsPointsTooltip", "If enabled, the landscape will be exported to Houdini as points." );
            ButtonOptionsPanel->AddSlot(2, 0)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "Property.ToggleButton.End")
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportAsPoints ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAsPoints ) )
                .ToolTipText( PointsTooltip )
                [
                     SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush("Mobility.Static"))
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(STextBlock)
                        .Text( LOCTEXT( "LandscapeExportAsPointsCheckbox", "Points" ) )
                        .ToolTipText( PointsTooltip )
                        .Font( IDetailLayoutBuilder::GetDetailFont() )
                    ]
                ]
            ];
        }

        // CheckBox : Export selected components only
        {
            TSharedPtr< SCheckBox > CheckBoxExportSelected;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportSelected, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeSelectedCheckbox", "Export Selected Landscape Components Only" ) )
                    .ToolTipText( LOCTEXT( "LandscapeSelectedTooltip", "If enabled, only the selected Landscape Components will be exported." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont") ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::IsCheckedExportOnlySelected) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportOnlySelected ) )
            ];
        }

        // Checkboxes auto select components
        {
            TSharedPtr< SCheckBox > CheckBoxAutoSelectComponents;
            VerticalBox->AddSlot().Padding(10, 2, 5, 2).AutoHeight()
            [
                SAssignNew( CheckBoxAutoSelectComponents, SCheckBox )
                .Content()
                [
                    SNew(STextBlock)
                    .Text( LOCTEXT( "AutoSelectComponentCheckbox", "Auto-select component in asset bounds" ) )
                    .ToolTipText( LOCTEXT( "AutoSelectComponentCheckboxTooltip", "If enabled, when no Landscape components are curremtly selected, the one within the asset's bounding box will be exported." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont") ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedAutoSelectLandscape ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedAutoSelectLandscape) )
            ];

            // Enable only when exporting selection
            // or when exporting heighfield (for now)
            if ( !InParam.bLandscapeExportSelectionOnly )
                CheckBoxAutoSelectComponents->SetEnabled( false );
        }

        // Checkbox : Export materials
        {
            TSharedPtr< SCheckBox > CheckBoxExportMaterials;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportMaterials, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeMaterialsCheckbox", "Export Landscape Materials" ) )
                    .ToolTipText( LOCTEXT( "LandscapeMaterialsTooltip", "If enabled, the landscape materials will be exported with it." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportMaterials ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportMaterials ) )
            ];

            // Disable when exporting heightfields
            if ( InParam.bLandscapeExportAsHeightfield )
                CheckBoxExportMaterials->SetEnabled( false );
        }

        // Checkbox : Export Tile UVs
        {
            TSharedPtr< SCheckBox > CheckBoxExportTileUVs;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportTileUVs, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeTileUVsCheckbox", "Export Landscape Tile UVs" ) )
                    .ToolTipText( LOCTEXT( "LandscapeTileUVsTooltip", "If enabled, UVs will be exported separately for each Landscape tile." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportTileUVs ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportTileUVs ) )
            ];

            // Disable when exporting heightfields
            if ( InParam.bLandscapeExportAsHeightfield )
                CheckBoxExportTileUVs->SetEnabled( false );
        }

        // Checkbox : Export normalized UVs
        {
            TSharedPtr< SCheckBox > CheckBoxExportNormalizedUVs;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportNormalizedUVs, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs" ) )
                    .ToolTipText( LOCTEXT( "LandscapeNormalizedUVsTooltip", "If enabled, landscape UVs will be exported in [0, 1]." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::IsCheckedExportNormalizedUVs ) ) )
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportNormalizedUVs ) )
            ];

            // Disable when exporting heightfields
            if ( InParam.bLandscapeExportAsHeightfield )
                CheckBoxExportNormalizedUVs->SetEnabled( false );
        }

        // Checkbox : Export lighting
        {
            TSharedPtr< SCheckBox > CheckBoxExportLighting;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportLighting, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeLightingCheckbox", "Export Landscape Lighting" ) )
                    .ToolTipText( LOCTEXT( "LandscapeLightingTooltip", "If enabled, lightmap information will be exported with the landscape." ) )
                    .Font(FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportLighting ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportLighting ) )
            ];

            // Disable when exporting heightfields
            if ( InParam.bLandscapeExportAsHeightfield )
                CheckBoxExportLighting->SetEnabled( false );
        }

        // Checkbox : Export landscape curves
        {
            TSharedPtr< SCheckBox > CheckBoxExportCurves;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportCurves, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeCurvesCheckbox", "Export Landscape Curves" ) )
                    .ToolTipText( LOCTEXT( "LandscapeCurvesTooltip", "If enabled, Landscape curves will be exported." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportCurves ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportCurves ) )
            ];

            // Disable curves until we have them implemented.
            if ( CheckBoxExportCurves.IsValid() )
                CheckBoxExportCurves->SetEnabled( false );
        }

        // Button : Recommit
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1, 2, 4, 2 )
            [
                SNew( SButton )
                .VAlign( VAlign_Center )
                .HAlign( HAlign_Center )
                .Text( LOCTEXT( "LandscapeInputRecommit", "Recommit Landscape" ) )
                .ToolTipText( LOCTEXT( "LandscapeInputRecommitTooltip", "Recommits the Landscape to Houdini." ) )
                .OnClicked( FOnClicked::CreateUObject( &InParam, &UHoudiniAssetInput::OnButtonClickRecommit ) )
            ]
        ];
    }
    else if ( InParam.ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        // Button : Start Selection / Use current selection + refresh
        {
            TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
            FPropertyEditorModule & PropertyModule =
                FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

            // Get the details view
            bool bDetailsLocked = false;
            FName DetailsPanelName = "LevelEditorSelectionDetails";

            const IDetailsView* DetailsView = DetailLayoutBuilder.GetDetailsView();
            if ( DetailsView )
            {
                DetailsPanelName = DetailsView->GetIdentifier();
                if ( DetailsView->IsLocked() )
                    bDetailsLocked = true;
            }

            auto ButtonLabel = LOCTEXT( "WorldInputStartSelection", "Start Selection (Lock Details Panel)" );
            if ( bDetailsLocked )
                ButtonLabel = LOCTEXT( "WorldInputUseCurrentSelection", "Use Current Selection (Unlock Details Panel)" );

            FOnClicked OnSelectActors = FOnClicked::CreateStatic( &FHoudiniParameterDetails::Helper_OnButtonClickSelectActors, MyParam, DetailsPanelName );

            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( HorizontalBox, SHorizontalBox )
                + SHorizontalBox::Slot()
                [
                    SNew( SButton )
                    .VAlign( VAlign_Center )
                    .HAlign( HAlign_Center )
                    .Text( ButtonLabel )
                    .OnClicked( OnSelectActors )
                ]
            ];
        }

        // ActorPicker : World Outliner
        {
            FMenuBuilder MenuBuilder = Helper_CreateCustomActorPickerWidget( InParam, LOCTEXT( "WorldInputSelectedActors", "Currently Selected Actors" ), false );
            
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                MenuBuilder.MakeWidget()
            ];
        }

        {
            // Spline Resolution
            TSharedPtr< SNumericEntryBox< float > > NumericEntryBox;
            int32 Idx = 0;

            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("SplineRes", "Unreal Spline Resolution"))
                    .ToolTipText(LOCTEXT("SplineResTooltip", "Resolution used when marshalling the Unreal Splines to HoudiniEngine.\n(step in cm betweem control points)\nSet this to 0 to only export the control points."))
                    .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
                + SHorizontalBox::Slot()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SNumericEntryBox< float >)
                    .AllowSpin(true)

                    .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

                    .MinValue(-1.0f)
                    .MaxValue(1000.0f)
                    .MinSliderValue(0.0f)
                    .MaxSliderValue(1000.0f)

                    .Value(TAttribute< TOptional< float > >::Create(TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetSplineResolutionValue) ) )
                    .OnValueChanged(SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
                        &InParam, &UHoudiniAssetInput::SetSplineResolutionValue) )
                    .IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::IsSplineResolutionEnabled)))

                    .SliderExponent(1.0f)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .ToolTipText(LOCTEXT("SplineResToDefault", "Reset to default value."))
                    .ButtonStyle(FEditorStyle::Get(), "NoBorder")
                    .ContentPadding(0)
                    .Visibility(EVisibility::Visible)
                    .OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnResetSplineResolutionClicked))
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
                    ]
                ]
            ];
        }
    }

    if ( InParam.ChoiceIndex == EHoudiniAssetInputType::SkeletonInput )
    {
        const int32 NumInputs = InParam.SkeletonInputObjects.Num();
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( FText::Format( LOCTEXT( "NumArrayItemsFmt", "{0} elements" ), FText::AsNumber( NumInputs ) ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInput::OnAddToSkeletonInputObjects ), LOCTEXT( "AddSkeletonInput", "Adds a Skeleton Input" ), true )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateUObject( &InParam, &UHoudiniAssetInput::OnEmptySkeletonInputObjects ), LOCTEXT( "EmptySkeletonInputs", "Removes All Inputs" ), true )
            ]
        ];

        for ( int32 Ix = 0; Ix < NumInputs; Ix++ )
        {
            UObject* InputObject = InParam.GetSkeletonInputObject( Ix );
            Helper_CreateSkeletonWidget( InParam, Ix, InputObject, AssetThumbnailPool, VerticalBox );
        }
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
FHoudiniParameterDetails::CreateWidgetLabel( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterLabel& InParam )
{
    TSharedPtr< STextBlock > TextBlock;
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
    FText ParameterTooltip = GetParameterTooltip( &InParam );

    LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
    [
        SAssignNew( TextBlock, STextBlock )
        .Text( ParameterLabelText )
        .ToolTipText( ParameterTooltip )
        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
        .WrapTextAt( HAPI_UNREAL_DESIRED_ROW_FULL_WIDGET_WIDTH )
        .Justification( ETextJustify::Center )
    ];

    if ( TextBlock.IsValid() )
        TextBlock->SetEnabled( !InParam.bIsDisabled );
}

void 
FHoudiniParameterDetails::CreateWidgetString( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterString& InParam )
{
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    for ( int32 Idx = 0; Idx < InParam.GetTupleSize(); ++Idx )
    {
        TSharedPtr< SEditableTextBox > EditableTextBox;

        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
        [
            SAssignNew( EditableTextBox, SEditableTextBox )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            .Text( FText::FromString( InParam.Values[Idx] ) )
            .OnTextCommitted( FOnTextCommitted::CreateUObject(
                &InParam, &UHoudiniAssetParameterString::SetValueCommitted, Idx ) )
        ];

        if ( EditableTextBox.IsValid() )
            EditableTextBox->SetEnabled( !InParam.bIsDisabled );
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
FHoudiniParameterDetails::CreateWidgetSeparator( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterSeparator& InParam )
{
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
        Separator->SetEnabled( !InParam.bIsDisabled );
}
