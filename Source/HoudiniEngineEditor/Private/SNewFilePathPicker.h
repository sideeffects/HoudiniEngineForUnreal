/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
		, _IsDirectoryPicker(false)
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

	/** Whether to use the a directory picker dialog */
	SLATE_ATTRIBUTE(bool, IsDirectoryPicker)

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

	TAttribute<bool> IsDirectoryPicker;

private:

	/** Holds a delegate that is executed when a file was picked. */
	FOnPathPicked OnPathPicked;
};

