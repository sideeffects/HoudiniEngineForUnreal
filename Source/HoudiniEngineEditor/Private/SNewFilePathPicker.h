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

// This is the same as SFilePathPicker but it uses SaveFileDialog instead of OpenFileDialog 
// to allow browsing to a new path

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class SEditableTextBox;

/**
 * Declares a delegate that is executed when a file was picked in the SFilePathPicker widget.
 *
 * The first parameter will contain the path to the picked file.
 */
DECLARE_DELEGATE_OneParam(FOnPathPicked, const FString& /*PickedPath*/);


/**
 * Implements an editable text box with a browse button.
 */
class SNewFilePathPicker
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SNewFilePathPicker)
		: _BrowseButtonToolTip(NSLOCTEXT("SNewFilePathPicker", "BrowseButtonToolTip", "Choose a file from this computer"))
		, _FileTypeFilter(TEXT("All files (*.*)|*.*"))
		, _Font()
		, _IsReadOnly(false)
                , _IsNewFile(true)
	{ }

		/** Browse button image resource. */
		SLATE_ATTRIBUTE(const FSlateBrush*, BrowseButtonImage)

		/** Browse button visual style. */
		SLATE_STYLE_ARGUMENT(FButtonStyle, BrowseButtonStyle)

		/** Browse button tool tip text. */
		SLATE_ATTRIBUTE(FText, BrowseButtonToolTip)

		/** The directory to browse by default */
		SLATE_ATTRIBUTE(FString, BrowseDirectory)

		/** Title for the browse dialog window. */
		SLATE_ATTRIBUTE(FText, BrowseTitle)

		/** The currently selected file path. */
		SLATE_ATTRIBUTE(FString, FilePath)

		/** File type filter string. */
		SLATE_ATTRIBUTE(FString, FileTypeFilter)

		/** Font color and opacity of the path text box. */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)

		/** Whether the path text box can be modified by the user. */
		SLATE_ATTRIBUTE(bool, IsReadOnly)

                /** Whether to use the new-file dialog instead of open-file */
                SLATE_ATTRIBUTE(bool, IsNewFile)

		/** Called when a file path has been picked. */
		SLATE_EVENT(FOnPathPicked, OnPathPicked)

	SLATE_END_ARGS()

	/**
	 * Constructs a new widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs );

private:

	/** Callback for clicking the browse button. */
	FReply HandleBrowseButtonClicked( );

	/** Callback for getting the text in the path text box. */
	FText HandleTextBoxText( ) const;

	/** Callback for committing the text in the path text box. */
	void HandleTextBoxTextCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ );

private:

	/** Holds the directory path to browse by default. */
	TAttribute<FString> BrowseDirectory;

	/** Holds the title for the browse dialog window. */
	TAttribute<FText> BrowseTitle;

	/** Holds the currently selected file path. */
	TAttribute<FString> FilePath;

	/** Holds the file type filter string. */
	TAttribute<FString> FileTypeFilter;

	/** Holds the editable text box. */
	TSharedPtr<SEditableTextBox> TextBox;

        TAttribute<bool> IsNewFile;

private:

	/** Holds a delegate that is executed when a file was picked. */
	FOnPathPicked OnPathPicked;
};
