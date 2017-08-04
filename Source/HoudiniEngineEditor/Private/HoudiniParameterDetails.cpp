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
#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAssetInput.h"
#include "HoudiniAssetParameterButton.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniRuntimeSettings.h"
#include "SNewFilePathPicker.h"

#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerPublicTypes.h"
#include "EditorDirectories.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization.h"
#include "NumericUnitTypeInterface.inl"
#include "UnitConversion.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniParameterDetails::CreateNameWidget( UHoudiniAssetParameter* InParam, FDetailWidgetRow & Row, bool WithLabel )
{
    FText ParameterLabelText = FText::FromString( InParam->GetParameterLabel() );
    const FText & FinalParameterLabelText = WithLabel ? ParameterLabelText : FText::GetEmpty();
    FText ParameterTooltip = WithLabel ? FText::FromString( InParam->GetParameterName() ) : 
        FText::FromString( InParam->GetParameterLabel() + TEXT( " (" ) + InParam->GetParameterName() + TEXT( ")" ) );

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
            .ToolTipText( ParameterLabelText )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
    }
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
    if ( auto ParamFile = Cast<UHoudiniAssetParameterFile>( InParam ) )
    {
        CreateWidgetFile( LocalDetailCategoryBuilder, *ParamFile );
    }
    else
    {
        // FIXME: This case should go away
        InParam->CreateWidget( LocalDetailCategoryBuilder );
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
        // FIXME: This case should go away
        InParam->CreateWidget( VerticalBox );
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
            .IsNewFile(true)
        ];
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
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

            HorizontalBox->AddSlot().Padding( 0, 2, 0, 2 )
            [
                SNew( SButton )
                .VAlign( VAlign_Center )
                .HAlign( HAlign_Center )
                .Text( ParameterLabelText )
                .ToolTipText( ParameterLabelText )
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
FHoudiniParameterDetails::CreateWidgetButton( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterButton& InParam )
{
    TWeakObjectPtr<UHoudiniAssetParameterButton> MyParam( &InParam );
    FDetailWidgetRow& Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );

    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );
    TSharedPtr< SButton > Button;

    HorizontalBox->AddSlot().Padding( 1, 2, 4, 2 )
        [
            SAssignNew( Button, SButton )
            .VAlign( VAlign_Center )
            .HAlign( HAlign_Center )
            .Text( ParameterLabelText )
            .ToolTipText( ParameterLabelText )
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
    
    TWeakObjectPtr<UHoudiniAssetParameterChoice> MyParam( &InParam );

    VerticalBox->AddSlot().Padding( 2, 2, 2, 2 )
    [
        SNew( SHorizontalBox )
        + SHorizontalBox::Slot().MaxWidth( 80 ).Padding( 7, 1, 0, 0 ).VAlign( VAlign_Center )
        [
            SNew( STextBlock )
            .Text( ParameterLabelText )
            .ToolTipText( ParameterLabelText )
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
                PickerArgs.DisplayGamma = TAttribute< float >::Create( TAttribute< float >::FGetter::CreateUObject(
                    GEngine, &UEngine::GetDisplayGamma ) );
                PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateUObject(
                    MyParam.Get(), &UHoudiniAssetParameterColor::OnPaintColorChanged, true, true );
                PickerArgs.InitialColorOverride = MyParam->GetColor();
                PickerArgs.bOnlyRefreshOnOk = true;
                PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateLambda(
                    [=]( const TSharedRef<SWindow>& ) { 
                        if ( MyParam.IsValid() ) 
                            MyParam->bIsColorPickerOpen = false;  
                    } );

                OpenColorPicker( PickerArgs );
                MyParam->bIsColorPickerOpen = true;

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
                .ToolTipText( ParameterLabelText )
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
                    .ToolTipText( ParameterLabelText )
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

    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( &InParam, Row, true );

    // Helper function to find a unit from a string (name or abbreviation) 
    TOptional<EUnit> ParmUnit = FUnitConversion::UnitFromString( *InParam.ValueUnit );

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

            if ( NumericEntryBox.IsValid() )
                NumericEntryBox->SetEnabled( !InParam.bIsDisabled );
        }

        Row.ValueWidget.Widget = VerticalBox;
    }
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
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
                []( const UObject* InObject ) {
                    return InObject && InObject->IsA< UStaticMesh >();
            } ) )
            .OnAssetDropped( SAssetDropTarget::FOnAssetDropped::CreateUObject(
                &InParam, &UHoudiniAssetInput::OnStaticMeshDropped, AtIndex ) )
            [
                SAssignNew( HorizontalBox, SHorizontalBox )
            ]
        ];

    // Thumbnail : Static Mesh
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );
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
        TAttribute< const FSlateBrush * >::FGetter::CreateLambda( [StaticMeshThumbnailBorder]() {
        if ( StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered() )
            return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
        else
            return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
    } ) ) );

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
        [MyParam, AtIndex, StaticMeshComboButton]() {
            TArray< const UClass * > AllowedClasses;
            AllowedClasses.Add( UStaticMesh::StaticClass() );

            TArray< UFactory * > NewAssetFactories;
            return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
                FAssetData( MyParam->GetInputObject( AtIndex ) ),
                true,
                AllowedClasses,
                NewAssetFactories,
                MyParam->OnShouldFilterStaticMesh,
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
        //TSharedPtr<SButton> ExpanderArrow;
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( InParam.ExpanderArrow, SButton )
                .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                .ClickMethod( EButtonClickMethod::MouseDown )
                    .Visibility( EVisibility::Visible )
                .OnClicked( FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnExpandInputTransform, AtIndex ) )
                [
                    SNew( SImage )
                    .Image( FEditorStyle::GetBrush( TEXT( "TreeArrow_Collapsed" ) ) )
                    .Image( TAttribute<const FSlateBrush*>::Create(
                        TAttribute<const FSlateBrush*>::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::GetExpanderImage, AtIndex ) ) )
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

void 
FHoudiniParameterDetails::CreateWidgetInput( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInput& InParam )
{
    TWeakObjectPtr<UHoudiniAssetInput> MyParam( &InParam );

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = LocalDetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    FText ParameterLabelText = FText::FromString( InParam.GetParameterLabel() );

    Row.NameWidget.Widget =
        SNew( STextBlock )
            .Text( ParameterLabelText )
            .ToolTipText( ParameterLabelText )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    if ( InParam.StringChoiceLabels.Num() > 0 )
    {
	// ComboBox :  Input Type
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
        [
            SNew( SComboBox<TSharedPtr< FString > > )
            .OptionsSource( &InParam.StringChoiceLabels )
            .InitiallySelectedItem( InParam.StringChoiceLabels[InParam.ChoiceIndex ] )
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

            // Locate the details panel.
            FName DetailsPanelName = "LevelEditorSelectionDetails";
            TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

            auto ButtonLabel = LOCTEXT( "WorldInputStartSelection", "Start Selection (Lock Details Panel)" );
            if ( DetailsView->IsLocked() )
                ButtonLabel = LOCTEXT( "WorldInputUseCurrentSelection", "Use Current Selection (Unlock Details Panel)" );

            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( HorizontalBox, SHorizontalBox )
                + SHorizontalBox::Slot()
                [
                    SNew( SButton )
                    .VAlign( VAlign_Center )
                    .HAlign( HAlign_Center )
                    .Text( ButtonLabel )
                    .OnClicked( FOnClicked::CreateUObject( &InParam, &UHoudiniAssetInput::OnButtonClickSelectActors ) )
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

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}
