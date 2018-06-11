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
#include "SNewFilePathPicker.h"
#include "DesktopPlatformModule.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "SNewFilePathPicker"


/* SNewFilePathPicker interface
 *****************************************************************************/

void SNewFilePathPicker::Construct( const FArguments& InArgs )
{
	BrowseDirectory = InArgs._BrowseDirectory;
	BrowseTitle = InArgs._BrowseTitle;
	FilePath = InArgs._FilePath;
	FileTypeFilter = InArgs._FileTypeFilter;
	OnPathPicked = InArgs._OnPathPicked;
        IsNewFile = InArgs._IsNewFile;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				// file path text box
				SAssignNew(TextBox, SEditableTextBox)
					.Text(this, &SNewFilePathPicker::HandleTextBoxText)
					.Font(InArgs._Font)
					.SelectAllTextWhenFocused(true)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextCommitted(this, &SNewFilePathPicker::HandleTextBoxTextCommitted)
					.SelectAllTextOnCommit(false)
					.IsReadOnly(InArgs._IsReadOnly)
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				// browse button
				SNew(SButton)
					.ButtonStyle(InArgs._BrowseButtonStyle)
					.ToolTipText(InArgs._BrowseButtonToolTip)
					.OnClicked(this, &SNewFilePathPicker::HandleBrowseButtonClicked)
					.ContentPadding(2.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					[
						SNew(SImage)
							.Image(InArgs._BrowseButtonImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
					]
			]
	];
}


/* SNewFilePathPicker callbacks
 *****************************************************************************/
#if PLATFORM_WINDOWS

#include "Windows/WindowsHWrapper.h"
#include "Windows/COMPointer.h"
//#include "Misc/Paths.h"
//#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
//#include <shellapi.h>
#include <shlobj.h>
//#include <Winver.h>
//#include <LM.h>
//#include <tlhelp32.h>
//#include <Psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
//#pragma comment( lib, "version.lib" )

bool FileDialogShared( bool bSave, const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex )
{
    FScopedSystemModalMode SystemModalScope;

    bool bSuccess = false;

    TComPtr<IFileDialog> FileDialog;
    if ( SUCCEEDED( ::CoCreateInstance( bSave ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, bSave ? IID_IFileSaveDialog : IID_IFileOpenDialog, IID_PPV_ARGS_Helper( &FileDialog ) ) ) )
    {
        if ( bSave )
        {
            // Set the default "filename"
            if ( !DefaultFile.IsEmpty() )
            {
                FileDialog->SetFileName( *FPaths::GetCleanFilename( DefaultFile ) );
            }
            DWORD dwFlags = 0;
            FileDialog->GetOptions( &dwFlags );
            FileDialog->SetOptions( dwFlags & ~FOS_OVERWRITEPROMPT );
        }
        else
        {
            // Set this up as a multi-select picker
            if ( Flags & EFileDialogFlags::Multiple )
            {
                DWORD dwFlags = 0;
                FileDialog->GetOptions( &dwFlags );
                FileDialog->SetOptions( dwFlags | FOS_ALLOWMULTISELECT );
            }
        }

        // Set up common settings
        FileDialog->SetTitle( *DialogTitle );
        if ( !DefaultPath.IsEmpty() )
        {
            // SHCreateItemFromParsingName requires the given path be absolute and use \ rather than / as our normalized paths do
            FString DefaultWindowsPath = FPaths::ConvertRelativePathToFull( DefaultPath );
            DefaultWindowsPath.ReplaceInline( TEXT( "/" ), TEXT( "\\" ), ESearchCase::CaseSensitive );

            TComPtr<IShellItem> DefaultPathItem;
            if ( SUCCEEDED( ::SHCreateItemFromParsingName( *DefaultWindowsPath, nullptr, IID_PPV_ARGS( &DefaultPathItem ) ) ) )
            {
                FileDialog->SetFolder( DefaultPathItem );
            }
        }

        // Set-up the file type filters
        TArray<FString> UnformattedExtensions;
        TArray<COMDLG_FILTERSPEC> FileDialogFilters;
        {
            // Split the given filter string (formatted as "Pair1String1|Pair1String2|Pair2String1|Pair2String2") into the Windows specific filter struct
            FileTypes.ParseIntoArray( UnformattedExtensions, TEXT( "|" ), true );

            if ( UnformattedExtensions.Num() % 2 == 0 )
            {
                FileDialogFilters.Reserve( UnformattedExtensions.Num() / 2 );
                for ( int32 ExtensionIndex = 0; ExtensionIndex < UnformattedExtensions.Num();)
                {
                    COMDLG_FILTERSPEC& NewFilterSpec = FileDialogFilters[FileDialogFilters.AddDefaulted()];
                    NewFilterSpec.pszName = *UnformattedExtensions[ExtensionIndex++];
                    NewFilterSpec.pszSpec = *UnformattedExtensions[ExtensionIndex++];
                }
            }
        }
        FileDialog->SetFileTypes( FileDialogFilters.Num(), FileDialogFilters.GetData() );

        // Show the picker
        if ( SUCCEEDED( FileDialog->Show( (HWND)ParentWindowHandle ) ) )
        {
            OutFilterIndex = 0;
            if ( SUCCEEDED( FileDialog->GetFileTypeIndex( (UINT*)&OutFilterIndex ) ) )
            {
                OutFilterIndex -= 1; // GetFileTypeIndex returns a 1-based index
            }

            auto AddOutFilename = [&OutFilenames]( const FString& InFilename )
            {
                FString& OutFilename = OutFilenames[OutFilenames.Add( InFilename )];
                OutFilename = IFileManager::Get().ConvertToRelativePath( *OutFilename );
                FPaths::NormalizeFilename( OutFilename );
            };

            if ( bSave )
            {
                TComPtr<IShellItem> Result;
                if ( SUCCEEDED( FileDialog->GetResult( &Result ) ) )
                {
                    PWSTR pFilePath = nullptr;
                    if ( SUCCEEDED( Result->GetDisplayName( SIGDN_FILESYSPATH, &pFilePath ) ) )
                    {
                        bSuccess = true;

                        // Apply the selected extension if the given filename doesn't already have one
                        FString SaveFilePath = pFilePath;
                        if ( FileDialogFilters.IsValidIndex( OutFilterIndex ) )
                        {
                            // Build a "clean" version of the selected extension (without the wildcard)
                            FString CleanExtension = FileDialogFilters[OutFilterIndex].pszSpec;
                            if ( CleanExtension == TEXT( "*.*" ) )
                            {
                                CleanExtension.Reset();
                            }
                            else
                            {
                                const int32 WildCardIndex = CleanExtension.Find( TEXT( "*" ) );
                                if ( WildCardIndex != INDEX_NONE )
                                {
                                    CleanExtension = CleanExtension.RightChop( WildCardIndex + 1 );
                                }
                            }

                            // We need to split these before testing the extension to avoid anything within the path being treated as a file extension
                            FString SaveFileName = FPaths::GetCleanFilename( SaveFilePath );
                            SaveFilePath = FPaths::GetPath( SaveFilePath );

                            // Apply the extension if the file name doesn't already have one
                            if ( FPaths::GetExtension( SaveFileName ).IsEmpty() && !CleanExtension.IsEmpty() )
                            {
                                SaveFileName = FPaths::SetExtension( SaveFileName, CleanExtension );
                            }

                            SaveFilePath /= SaveFileName;
                        }
                        AddOutFilename( SaveFilePath );

                        ::CoTaskMemFree( pFilePath );
                    }
                }
            }
            else
            {
                IFileOpenDialog* FileOpenDialog = static_cast<IFileOpenDialog*>( FileDialog.Get() );

                TComPtr<IShellItemArray> Results;
                if ( SUCCEEDED( FileOpenDialog->GetResults( &Results ) ) )
                {
                    DWORD NumResults = 0;
                    Results->GetCount( &NumResults );
                    for ( DWORD ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex )
                    {
                        TComPtr<IShellItem> Result;
                        if ( SUCCEEDED( Results->GetItemAt( ResultIndex, &Result ) ) )
                        {
                            PWSTR pFilePath = nullptr;
                            if ( SUCCEEDED( Result->GetDisplayName( SIGDN_FILESYSPATH, &pFilePath ) ) )
                            {
                                bSuccess = true;
                                AddOutFilename( pFilePath );
                                ::CoTaskMemFree( pFilePath );
                            }
                        }
                    }
                }
            }
        }
    }

    return bSuccess;
}

bool SaveFileDialog( const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames )
{
    int32 DummyFilterIndex = 0;
    return FileDialogShared( true, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, DummyFilterIndex );
}

#else

bool SaveFileDialog( const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames )
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    return DesktopPlatform->SaveFileDialog( ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames );
}
#endif

FReply SNewFilePathPicker::HandleBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform == nullptr)
	{
		return FReply::Handled();
	}

	const FString DefaultPath = BrowseDirectory.IsSet()
		? BrowseDirectory.Get()
		: FPaths::GetPath(FilePath.Get());

	// show the file browse dialog
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;

        // CG: Use SaveFileDialog instead of OpenFileDialog
        if ( IsNewFile.Get() )
        {
            if ( SaveFileDialog( ParentWindowHandle, BrowseTitle.Get().ToString(), DefaultPath, TEXT( "" ), FileTypeFilter.Get(), EFileDialogFlags::None, OutFiles ) )
            {
                OnPathPicked.ExecuteIfBound( OutFiles[0] );
            }
        }
        else
        {
            if ( DesktopPlatform->OpenFileDialog( ParentWindowHandle, BrowseTitle.Get().ToString(), DefaultPath, TEXT( "" ), FileTypeFilter.Get(), EFileDialogFlags::None, OutFiles ) )
            {
                OnPathPicked.ExecuteIfBound( OutFiles[0] );
            }
        }

	return FReply::Handled();
}


FText SNewFilePathPicker::HandleTextBoxText() const
{
	return FText::FromString(FilePath.Get());
}


void SNewFilePathPicker::HandleTextBoxTextCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	OnPathPicked.ExecuteIfBound(NewText.ToString());
}




#undef LOCTEXT_NAMESPACE
