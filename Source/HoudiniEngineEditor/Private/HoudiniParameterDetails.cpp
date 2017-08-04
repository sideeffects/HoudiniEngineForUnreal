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

#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetParameterMultiparm.h"

#include "EditorDirectories.h"
#include "SNewFilePathPicker.h"

#include "Internationalization.h"
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

    if ( UHoudiniAssetParameterFile* Param = Cast<UHoudiniAssetParameterFile>( InParam ) )
    {
        CreateWidgetFile( LocalDetailCategoryBuilder, *Param );
    }
    else
    {
        // FIXME: This case should go away
        InParam->CreateWidget( LocalDetailCategoryBuilder );
    }
}

void FHoudiniParameterDetails::CreateWidgetFile( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFile& InParam )
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
