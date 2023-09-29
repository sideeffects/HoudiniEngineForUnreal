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

#include "HoudiniToolsRuntimeUtils.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"

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

#if WITH_EDITOR
void UHoudiniToolData::LoadIconFromPath(const FString& IconPath)
{
	const FString FullIconPath = FPaths::ConvertRelativePathToFull(IconPath);
	bool bSuccess = false;
	
    if (FPaths::FileExists(FullIconPath))
    {
        FName BrushName = *IconPath;
        FHImageData ImageData;
		if (FHoudiniToolsRuntimeUtils::LoadFHImageFromFile(*FullIconPath, ImageData))
		{
			Modify();
			MarkPackageDirty();
	        IconImageData = MoveTemp(ImageData);
	        IconSourcePath.FilePath = FullIconPath;
			bSuccess = true;
		}
    }

	if (bSuccess)
	{
		UpdateOwningAssetThumbnail();
	}
    else
    {
        ClearCachedIcon();
    }
}
#endif

#if WITH_EDITOR
void UHoudiniToolData::ClearCachedIcon()
{
	IconImageData = FHImageData();
    IconSourcePath.FilePath = FString();
	UpdateOwningAssetThumbnail();
}

void UHoudiniToolData::UpdateOwningAssetThumbnail()
{
	UHoudiniAsset* HoudiniAsset = this->GetTypedOuter<UHoudiniAsset>();
    FHoudiniToolsRuntimeUtils::UpdateHoudiniAssetThumbnail(HoudiniAsset);
}
#endif

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
}

#undef DEFAULT_GAMMA_SPACE
#undef DEFAULT_RAW_IMAGE_FORMAT_TYPE
