/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Damian Campeanu, Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetLogWidget.h"

void
SHoudiniAssetLogWidget::Construct( const FArguments & InArgs )
{
    this->ChildSlot
    [
        SNew( SBorder )
        .BorderImage( FEditorStyle::GetBrush( TEXT( "Menu.Background" ) ) )
        .Content()
        [
            SNew( SScrollBox )
            + SScrollBox::Slot()
            [
                SNew( SMultiLineEditableTextBox )
                .Text( FText::FromString( InArgs._LogText ) )
                .AutoWrapText( true )
                .IsReadOnly( true )
            ]
        ]
    ];
}
