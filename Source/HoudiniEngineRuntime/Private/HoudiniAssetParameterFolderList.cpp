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
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetComponent.h"
#include "Widgets/Input/SButton.h"

UHoudiniAssetParameterFolderList::UHoudiniAssetParameterFolderList( const FObjectInitializer & ObjectInitializer)
    : Super( ObjectInitializer )
{}

UHoudiniAssetParameterFolderList::~UHoudiniAssetParameterFolderList()
{}

UHoudiniAssetParameterFolderList *
UHoudiniAssetParameterFolderList::Create(
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

    UHoudiniAssetParameterFolderList * HoudiniAssetParameterFolderList =
        NewObject< UHoudiniAssetParameterFolderList >(
            Outer, UHoudiniAssetParameterFolderList::StaticClass(), NAME_None,
            RF_Public | RF_Transactional );

    HoudiniAssetParameterFolderList->CreateParameter(
        InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo );

    return HoudiniAssetParameterFolderList;
}

bool
UHoudiniAssetParameterFolderList::CreateParameter(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo)
{
    if ( !Super::CreateParameter( InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle folder and folder list types.
    if ( ParmInfo.type != HAPI_PARMTYPE_FOLDERLIST )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.intValuesIndex );

    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterFolderList::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    TSharedRef< SHorizontalBox > HorizontalBox = SNew( SHorizontalBox );

    LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
    [
        SAssignNew(HorizontalBox, SHorizontalBox)
    ];

    for ( int32 ParameterIdx = 0; ParameterIdx < ChildParameters.Num(); ++ParameterIdx )
    {
        UHoudiniAssetParameter * HoudiniAssetParameterChild = ChildParameters[ ParameterIdx ];
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
                .OnClicked( FOnClicked::CreateUObject( this, &UHoudiniAssetParameterFolderList::OnButtonClick, ParameterIdx ) )
            ];
        }
    }

    Super::CreateWidget( LocalDetailCategoryBuilder );

    if ( ChildParameters.Num() > 1 )
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
            TextBlock->SetEnabled( !bIsDisabled );
    }
}

FReply
UHoudiniAssetParameterFolderList::OnButtonClick( int32 ParameterIdx )
{
    ActiveChildParameter = ParameterIdx;

    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->UpdateEditorProperties( false );

    return FReply::Handled();
}

#endif
