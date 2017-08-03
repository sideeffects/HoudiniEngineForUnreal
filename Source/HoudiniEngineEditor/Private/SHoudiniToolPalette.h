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

#pragma once

//#include "CoreMinimal.h"
#include "HoudiniEngineEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/NotifyHook.h"

class IDetailsView;
class ITableRow;
class STableViewBase;
struct FSlateBrush;
enum class ECheckBoxState : uint8;

class SHoudiniToolPalette : public SCompoundWidget, public FNotifyHook
{
public:
    SLATE_BEGIN_ARGS( SHoudiniToolPalette ) {}
    SLATE_END_ARGS();

    void Construct( const FArguments& InArgs );

private:

    /** Make a widget for the list view display */
    TSharedRef<ITableRow> MakeListViewWidget( TSharedPtr<struct FHoudiniToolType> BspBuilder, const TSharedRef<STableViewBase>& OwnerTable );

    /** Delegate for when the list view selection changes */
    void OnSelectionChanged( TSharedPtr<FHoudiniToolType> BspBuilder, ESelectInfo::Type SelectionType );

    /** Begin dragging a list widget */
   FReply OnDraggingListViewWidget( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

private:
    TSharedPtr<FHoudiniToolType> ActiveTool;
};
