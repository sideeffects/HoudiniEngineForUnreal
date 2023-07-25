/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "HoudiniToolData.h"

#include "ImageCore.h"
#include "ImageUtils.h"

#ifndef ENGINE_MAJOR_VERSION
#include "Runtime/Launch/Resources/Version.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define DEFAULT_GAMMA_SPACE EGammaSpace::Invalid
#define DEFAULT_RAW_IMAGE_FORMAT_TYPE ERawImageFormat::Invalid
#else
#define DEFAULT_GAMMA_SPACE EGammaSpace::sRGB
#define DEFAULT_RAW_IMAGE_FORMAT_TYPE ERawImageFormat::Type::BGRA8
#endif

// Wrapper interface to manage code compatibility across multiple UE versions

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
// Unreal 5.0 implementation
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

struct FToolsWrapper
{
	// static IImageWrapperModule * GetOrLoadImageWrapperModule()
	// {
	// 	static FName ImageWrapperName("ImageWrapper");
	//
	// 	if ( IsInGameThread() )
	// 	{
	// 		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(ImageWrapperName);
	// 		return &ImageWrapperModule;
	// 	}
	// 	else
	// 	{
	// 		IImageWrapperModule * ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>(ImageWrapperName);
	//
	// 		if ( ImageWrapperModule == nullptr )
	// 		{
	// 			HOUDINI_LOG_WARNING(TEXT("Not on GameThread, cannot load ImageWrapper.  Do on main thread in startup."))
	//
	// 			// LoadModule needs to be done on the Game thread as part of your initialization
	// 			//   before any thread tries to run this code
	// 			// Engine does this.  If you are making a stand-alone app and hit this error,
	// 			//   add this to your initialization on the Game thread :
	// 			// FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	// 		}
	//
	// 		return ImageWrapperModule;
	// 	}
	// }
	//
	
	static bool LoadImage(const TCHAR * Filename, FImage & OutImage)
	{
		// implementation adapted from: FSlateRHIResourceManager::LoadTexture
		bool bSucceeded = false;
		uint32 BytesPerPixel = 4;

		TArray<uint8> RawFileData;
		if( FFileHelper::LoadFileToArray( RawFileData, Filename ) )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );

			//Try and determine format, if that fails assume PNG
			EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
			if (ImageFormat == EImageFormat::Invalid)
			{
				ImageFormat = EImageFormat::PNG;
			}

			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num()) )
			{
				OutImage.SizeX = ImageWrapper->GetWidth();
				OutImage.SizeY = ImageWrapper->GetHeight();
				OutImage.Format = DEFAULT_RAW_IMAGE_FORMAT_TYPE;
				OutImage.NumSlices = 1;
				
				if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, OutImage.RawData))
				{
					bSucceeded = true;
				}
				else
				{
					HOUDINI_LOG_WARNING(TEXT("Invalid texture format for Slate resource only RGBA and RGB pngs are supported: %s"), Filename );
				}
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Only pngs are supported in Slate."));
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find file: %s"), Filename);
		}

		return bSucceeded;
	}
	

};

#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
// Unreal 5.1+ implementation

struct FToolsWrapper
{
    static bool LoadImage(const TCHAR * Filename, FImage & OutImage)
    {
	    return FImageUtils::LoadImage(Filename, OutImage);
    }
};

#endif


FHImageData::FHImageData()
	: SizeX(0)
	, SizeY(0)
	, NumSlices(0)
	, Format(DEFAULT_RAW_IMAGE_FORMAT_TYPE)
	, GammaSpace(static_cast<uint8>(DEFAULT_GAMMA_SPACE))
	, RawDataMD5(FString())
{ }

void FHImageData::FromImage(const FImage& InImage)
{
	SizeX = InImage.SizeX;
	SizeY = InImage.SizeY;
	NumSlices = InImage.NumSlices;
	Format = static_cast<uint8>(InImage.Format);
	GammaSpace = static_cast<uint8>(InImage.GammaSpace);
	RawData = InImage.RawData;
	RawDataMD5 = FMD5::HashBytes(RawData.GetData(), RawData.Num());
}

void FHImageData::ToImage(FImage& OutImage) const
{
	OutImage.SizeX = SizeX;
	OutImage.SizeY = SizeY;
	OutImage.NumSlices = NumSlices;
	OutImage.Format = static_cast<ERawImageFormat::Type>(Format);
	OutImage.GammaSpace = static_cast<EGammaSpace>(GammaSpace);
	OutImage.RawData = RawData;
}
 

UHoudiniToolData::UHoudiniToolData()
{
}

bool
UHoudiniToolData::PopulateFromJSONData(const FString& JSONData)
{
	return false;
}

bool
UHoudiniToolData::PopulateFromJSONFile(const FString& JsonFilePath)
{
	return false;
}

bool
UHoudiniToolData::ConvertToJSONData(FString& JSONData) const
{
	return false;
}

bool
UHoudiniToolData::SaveToJSONFile(const FString& JsonFilePath)
{
	return false;
}

void UHoudiniToolData::LoadIconFromPath(const FString& IconPath)
{
	const FString FullIconPath = FPaths::ConvertRelativePathToFull(IconPath);
	bool bSuccess = false;
	
    if (FPaths::FileExists(FullIconPath))
    {
        FName BrushName = *IconPath;
        FImage TmpImage;
        UE_LOG(LogTemp, Log, TEXT("[UHoudiniToolData::LoadIconFromPath] Image Path: %s"), *FullIconPath);
		if (FToolsWrapper::LoadImage(*FullIconPath, TmpImage))
		{
	        IconImageData.FromImage(TmpImage);
	        IconSourcePath.FilePath = FullIconPath;
			bSuccess = true;
			UE_LOG(LogTemp, Log, TEXT("[UHoudiniToolData::LoadIconFromPath] Loaded image with MD5 hash: %s"), *IconImageData.RawDataMD5);
			UE_LOG(LogTemp, Log, TEXT("[UHoudiniToolData::LoadIconFromPath] Num bytes: %d"), IconImageData.RawData.Num());
		}
    }
	
    if (!bSuccess)
    {
        ClearCachedIcon();
    }
}

void UHoudiniToolData::ClearCachedIcon()
{
	IconImageData = FHImageData();
    IconSourcePath.FilePath = FString();
}

void UHoudiniToolData::CopyFrom(const UHoudiniToolData& Other)
{
	if (this == &Other)
	{
		return;
	}
	
	Name = Other.Name;
	ToolTip = Other.ToolTip;
	IconImageData = Other.IconImageData;
	IconSourcePath = Other.IconSourcePath;
	HelpURL = Other.HelpURL;
	Type = Other.Type;
	DefaultTool = Other.DefaultTool;
	SelectionType = Other.SelectionType;
	SourceAssetPath = Other.SourceAssetPath;
	UE_LOG(LogTemp, Log, TEXT("[UHoudiniToolData::CopyFrom] Source Icon Hash: %s, New Icon Hash: %s"),  *Other.IconImageData.RawDataMD5, *IconImageData.RawDataMD5);
}
