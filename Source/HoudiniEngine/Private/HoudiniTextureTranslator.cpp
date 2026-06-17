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

#include "HoudiniTextureTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOutput.h"
#include "HoudiniMaterialTranslator.h"

#include "AssetRegistry/AssetRegistryModule.h"  // FAssetRegistryModule
#if WITH_EDITOR
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#endif
#include "ImageUtils.h"  // FCreateTexture2DParameters
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "MaterialShared.h"
#endif
#include "PackageTools.h"
bool
FHoudiniTextureTranslator::HapiGetImagePlanes(
	const HAPI_ParmId InNodeParmId, const HAPI_MaterialInfo& InMaterialInfo, TArray<FString>& OutImagePlanes)
{
	OutImagePlanes.Empty();
		
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::RenderTextureToImage(
		FHoudiniEngine::Get().GetSession(),
		InMaterialInfo.nodeId, InNodeParmId), false);

	int32 ImagePlaneCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlaneCount(
		FHoudiniEngine::Get().GetSession(),
		InMaterialInfo.nodeId, &ImagePlaneCount), false);

	if (ImagePlaneCount <= 0)
		return true;

	TArray<HAPI_StringHandle> ImagePlaneStringHandles;
	ImagePlaneStringHandles.SetNumZeroed(ImagePlaneCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlanes(
		FHoudiniEngine::Get().GetSession(),
		InMaterialInfo.nodeId, &ImagePlaneStringHandles[0], ImagePlaneCount), false);
	
	FHoudiniEngineString::SHArrayToFStringArray(ImagePlaneStringHandles, OutImagePlanes);

	return true;
}

bool
FHoudiniTextureTranslator::GetPlaneInfo(
	const HAPI_ParmId InParmTextureId,
	const HAPI_MaterialInfo& InMaterialInfo,
	HAPI_ImagePacking& OutImagePacking,
	const char*& OutPlaneType,
	bool& bOutUseAlpha)
{
	TArray<FString> ImagePlanes;
	bool bGetImagePlanesSuccess = FHoudiniTextureTranslator::HapiGetImagePlanes(
		InParmTextureId, InMaterialInfo, ImagePlanes);

	if (bGetImagePlanesSuccess && ImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_COLOR)))
	{
		// We use RGBA packing even if the image planes do not contain the alpha plane.
		// This is because all texture data from Houdini Engine contains the alpha plane by default.
		OutImagePacking = HAPI_IMAGE_PACKING_RGBA;
		OutPlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;

		bOutUseAlpha = ImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA));

		return true;
	}
	else
	{
		OutImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
		OutPlaneType = "";

		return false;
	}
}

bool
FHoudiniTextureTranslator::HapiRenderTexture(
	const HAPI_NodeId InMaterialNodeId,
	const HAPI_ParmId InTextureParmId)
{
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::RenderTextureToImage(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, InTextureParmId), false);
	return true;
}

bool
FHoudiniTextureTranslator::HapiRenderCOPTexture(
	const HAPI_NodeId InCopNodeId)
{
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::RenderCOPToImage(
		FHoudiniEngine::Get().GetSession(),
		InCopNodeId), false);
	return true;
}

bool
FHoudiniTextureTranslator::HapiExtractImage(
	const HAPI_NodeId InMaterialNodeId,
	const char* InPlaneType,
	const HAPI_ImagePacking InImagePacking,
	const float InGamma,
	TArray<char>& OutImageBuffer)
{
	// See if we have the images planes we want
	int NumImagePlanes = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlaneCount(
		FHoudiniEngine::Get().GetSession(), InMaterialNodeId, &NumImagePlanes), false);

	TArray<int32> ImagePlanesSHArray;
	ImagePlanesSHArray.SetNum(NumImagePlanes);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlanes(
		FHoudiniEngine::Get().GetSession(), InMaterialNodeId, ImagePlanesSHArray.GetData(), NumImagePlanes), false);

	TArray<FString> ImagePlanesStringArray;
	FHoudiniEngineString::SHArrayToFStringArray(ImagePlanesSHArray, ImagePlanesStringArray);

	bool bFound = false;
	bool bCFound = false;
	bool bAFound = false;
	FString InPlaneTypeString(InPlaneType);
	for (int32 n = 0; n < ImagePlanesStringArray.Num(); n++)
	{
		if (ImagePlanesStringArray[n].Equals(InPlaneTypeString, ESearchCase::IgnoreCase))
			bFound = true;
		else if (InPlaneTypeString.Equals("C A"))
		{
			if (ImagePlanesStringArray[n].Equals("C"))
			{
				bCFound = true;
				// If only color is found, still allow image extraction
				bFound = true;
			}
			else if (ImagePlanesStringArray[n].Equals("A"))
			{
				bAFound = true;
			}

			if (bCFound && bAFound)
				bFound = true;
		}
	}

	if (!bFound)
		return false;

	HAPI_ImageInfo ImageInfo;
	FHoudiniApi::ImageInfo_Init(&ImageInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImageInfo(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, &ImageInfo), false);

	// Only INT8/16 & FLOAT16/32 are supported
	if (ImageInfo.dataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT8
		&& ImageInfo.dataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT16
		&& ImageInfo.dataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT16
		&& ImageInfo.dataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT32)
	{
		// fallback to default INT8 for unsupported image format
		ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
	}

	// For HDR (Int16/Float16/32 images) - UE wants us to use linear colors - so no gamma 2.2
	bool bIsHDR = (ImageInfo.dataFormat != HAPI_IMAGE_DATA_INT8);

	ImageInfo.interleaved = true;
	ImageInfo.packing = InImagePacking;
	ImageInfo.gamma = bIsHDR ? 1.0f : InGamma;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetImageInfo(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, &ImageInfo), false);

	int32 ImageBufferSize = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ExtractImageToMemory(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, HAPI_RAW_FORMAT_NAME,
		InPlaneType, &ImageBufferSize), false);

	if (ImageBufferSize <= 0)
		return false;

	OutImageBuffer.SetNumUninitialized(ImageBufferSize);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImageMemoryBuffer(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, &OutImageBuffer[0],
		ImageBufferSize), false);

	return true;
}

UPackage*
FHoudiniTextureTranslator::CreatePackageForTexture(
	const HAPI_NodeId& InMaterialNodeId,
	const FString& InTextureType,
	const FHoudiniPackageParams& InPackageParams,
	FString& OutTextureName)
{
	FString TextureInfoDescriptor = TEXT("_texture_") + FString::FromInt(InMaterialNodeId);
	if (!InTextureType.Equals(""))
		TextureInfoDescriptor += TEXT("_") + InTextureType;

	FHoudiniPackageParams MyPackageParams = InPackageParams;
	if (!MyPackageParams.ObjectName.IsEmpty())
	{
		MyPackageParams.ObjectName += TextureInfoDescriptor;
	}
	else if (!MyPackageParams.HoudiniAssetName.IsEmpty())
	{
		MyPackageParams.ObjectName = MyPackageParams.HoudiniAssetName + TextureInfoDescriptor;
	}
	else
	{
		MyPackageParams.ObjectName = TextureInfoDescriptor;
	}

	// If we have a part name (output name) be sure to add it
	// ... but only if it's different from the texture type!
	if(!InPackageParams.SplitStr.IsEmpty() && !InPackageParams.SplitStr.Equals(InTextureType))
		MyPackageParams.ObjectName += TEXT("_") + InPackageParams.SplitStr;

	return MyPackageParams.CreatePackageForObject(OutTextureName);
}

bool
FHoudiniTextureTranslator::CreateTexture(
	const HAPI_NodeId InMaterialNodeId,
	const char* InPlaneType,
	HAPI_ImageDataFormat InImageDataFormat,
	HAPI_ImagePacking InImagePacking,
	float InGamma,
	UTexture2D*& OutTexture,
	const FString& InNodePath,
	const FString& InTextureType,
	const FHoudiniPackageParams& InPackageParams,
	const FCreateTexture2DParameters& InTextureParameters,
	const TextureGroup InLODGroup,
	TArray<UPackage*>& OutPackages)
{
	HAPI_ImageInfo ImageInfo;
	FHoudiniApi::ImageInfo_Init(&ImageInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImageInfo(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, &ImageInfo), false);

	// Only INT8/16 & FLOAT16/32 are supported
	if (InImageDataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT8
		&& InImageDataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT16
		&& InImageDataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT16
		&& InImageDataFormat != HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT32)
	{
		// Unsupported data format - default to RGBA8
		ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
	}
	else
	{
		ImageInfo.dataFormat = InImageDataFormat;
	}

	ImageInfo.interleaved = true;
	ImageInfo.packing = InImagePacking;
	ImageInfo.gamma = InGamma;

	// Update the image info before extracting the image
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetImageInfo(
		FHoudiniEngine::Get().GetSession(),
		InMaterialNodeId, &ImageInfo), false);

	bool bTextureCreated = false;
	TArray<char> ImageBuffer;
	if (FHoudiniTextureTranslator::HapiExtractImage(
		InMaterialNodeId, InPlaneType, InImagePacking, InGamma, ImageBuffer))
	{
		UPackage* TexturePackage = nullptr;
		if (IsValid(OutTexture))
			TexturePackage = Cast<UPackage>(OutTexture->GetOuter());

		FHoudiniApi::ImageInfo_Init(&ImageInfo);
		HAPI_Result Result = FHoudiniApi::GetImageInfo(
			FHoudiniEngine::Get().GetSession(),
			InMaterialNodeId, &ImageInfo);

		if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
		{
			// Create texture package, if this is a new texture.
			FString TextureName;
			if (!TexturePackage)
			{
				TexturePackage = FHoudiniTextureTranslator::CreatePackageForTexture(
					InMaterialNodeId, InTextureType, InPackageParams, TextureName);
			}
			else if (IsValid(OutTexture))
			{
				// Get the name of the texture if we are overwriting the exist asset
				TextureName = OutTexture->GetName();
			}
			else
			{
				TextureName = FPaths::GetBaseFilename(TexturePackage->GetName(), true);
			}

			bool bCreatedNewTexture = !IsValid(OutTexture);

			// Reuse existing texture, or create new one.
			OutTexture = FHoudiniTextureTranslator::CreateUnrealTexture(
				OutTexture, 
				ImageInfo,
				TexturePackage,
				TextureName,
				ImageBuffer,
				InTextureParameters,
				InLODGroup,
				InTextureType,
				InNodePath);

			//if (BakeMode == EBakeMode::CookToTemp)
			OutTexture->SetFlags(RF_Public | RF_Standalone);

			// Propagate and trigger texture updates.
			if (bCreatedNewTexture)
				FAssetRegistryModule::AssetCreated(OutTexture);

			OutTexture->PreEditChange(nullptr);
			OutTexture->PostEditChange();
			OutTexture->MarkPackageDirty();

			bTextureCreated = true;
		}

		// Cache the texture package
		OutPackages.AddUnique(TexturePackage);
	}

	return bTextureCreated;
}

UTexture2D*
FHoudiniTextureTranslator::CreateUnrealTexture(
	UTexture2D* ExistingTexture,
	const HAPI_ImageInfo& ImageInfo,
	UPackage* Package,
	const FString& TextureName,
	const TArray<char>& ImageBuffer,
	const FCreateTexture2DParameters& TextureParameters,
	const TextureGroup LODGroup, 
	const FString& TextureType,
	const FString& NodePath)
{
	if (!IsValid(Package))
		return nullptr;

	UTexture2D* Texture = nullptr;
	if (ExistingTexture)
	{
		Texture = ExistingTexture;
	}
	else
	{
		// Create new texture object.
		Texture = NewObject<UTexture2D>(
			Package, UTexture2D::StaticClass(), *TextureName,
			RF_Transactional);

		// Assign texture group.
		Texture->LODGroup = LODGroup;
	}

	// Add/Update meta information to package.
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName);
	if (!TextureType.Equals(""))
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);
	if (!NodePath.Equals(""))
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			Package, Texture, HAPI_UNREAL_PACKAGE_META_NODE_PATH, *NodePath);

	// Initialize texture source and params from the data format
	bool bDeferComp = true;
	bool bSRGB = TextureParameters.bSRGB;
	ETextureSourceFormat SrcFormat = TSF_BGRA8;
	TextureCompressionSettings CompSetting = TextureParameters.CompressionSettings;
	switch (ImageInfo.dataFormat)
	{
		case HAPI_IMAGE_DATA_INT16:
		{
			SrcFormat = TSF_RGBA16;
			CompSetting = TextureParameters.CompressionSettings;
			bDeferComp = false;
			bSRGB = TextureParameters.bSRGB;
		}
		break;

		case HAPI_IMAGE_DATA_FLOAT16:
		{
			SrcFormat = TSF_RGBA16F;
			CompSetting = TC_HDR_Compressed;
			bDeferComp = false;
			bSRGB = false;
		}
		break;
			
		case HAPI_IMAGE_DATA_FLOAT32:
		{
			SrcFormat = TSF_RGBA32F;
			CompSetting = TC_HDR_F32;
			bDeferComp = false;
		}
		break;

		case HAPI_IMAGE_DATA_INT32:
			// unsupported by UE
		case HAPI_IMAGE_DATA_INT8:
		default:
		{
			bDeferComp = false;
			SrcFormat = TSF_BGRA8;
			CompSetting = TextureParameters.CompressionSettings;
			bSRGB = TextureParameters.bSRGB;
		}
		break;
	}

	Texture->Source.Init(ImageInfo.xRes, ImageInfo.yRes, 1, 1, SrcFormat);

	// Lock the texture.
	uint8* MipData = Texture->Source.LockMip(0);

	// Create base map.
	uint8* DestPtr = nullptr;
	uint32 SrcWidth = ImageInfo.xRes;
	uint32 SrcHeight = ImageInfo.yRes;
	const char* SrcData = &ImageBuffer[0];

	// Handle the different packing for the source Houdini texture
	uint32 PackOffset = 4;
	uint32 OffsetR = 0;
	uint32 OffsetG = 1;
	uint32 OffsetB = 2;
	uint32 OffsetA = 3;
	switch (ImageInfo.packing)
	{
		case HAPI_IMAGE_PACKING_SINGLE:
			PackOffset = 1;
			OffsetR = 0;
			OffsetG = 0;
			OffsetB = 0;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_DUAL:
			PackOffset = 2;
			OffsetR = 0;
			OffsetG = 1;
			OffsetB = 1;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_RGB:
			PackOffset = 3;
			OffsetR = 0;
			OffsetG = 1;
			OffsetB = 2;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_BGR:
			PackOffset = 3;
			OffsetR = 2;
			OffsetG = 1;
			OffsetB = 0;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_RGBA:
			PackOffset = 4;
			OffsetR = 0;
			OffsetG = 1;
			OffsetB = 2;
			OffsetA = 3;
			break;

		case HAPI_IMAGE_PACKING_ABGR:
			PackOffset = 4;
			OffsetR = 3;
			OffsetG = 2;
			OffsetB = 1;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_UNKNOWN:
		case HAPI_IMAGE_PACKING_MAX:
			// invalid packing
			HOUDINI_CHECK_RETURN(false, nullptr);
			break;
	}

	// RGB8
	bool bHasAlphaValue = false;
	if (SrcFormat == TSF_BGRA8)
	{
		for (uint32 y = 0; y < SrcHeight; y++)
		{
			DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];

			for (uint32 x = 0; x < SrcWidth; x++)
			{
				uint32 DataOffset = y * SrcWidth * PackOffset + x * PackOffset;

				*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetB); // B
				*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetG); // G
				*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetR); // R

				if (TextureParameters.bUseAlpha && PackOffset == 4)
				{
					*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetA); // A
					if (*(uint8*)(SrcData + DataOffset + OffsetA) != 0xFF)
						bHasAlphaValue = true;
				}
				else
					*DestPtr++ = 0xFF;
			}
		}
	}
	// Not supported by COPZ resolver?
	else if (SrcFormat == TSF_RGBA16)
	{
		uint16* DestPtr16 = nullptr;
		const uint16* SrcData16 = (const uint16*)SrcData;

		//const int32 BytesPerPixel = sizeof(uint16) * 4;
		for (uint32 y = 0; y < SrcHeight; y++)
		{
			DestPtr16 = (uint16*)&MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(uint16) * 4];

			for (uint32 x = 0; x < SrcWidth; x++)
			{
				uint32 DataOffset = y * SrcWidth * PackOffset + x * PackOffset;

				*DestPtr16++ = *(uint16*)(SrcData16 + DataOffset + OffsetR); // R
				*DestPtr16++ = *(uint16*)(SrcData16 + DataOffset + OffsetG); // G
				*DestPtr16++ = *(uint16*)(SrcData16 + DataOffset + OffsetB); // B

				if (TextureParameters.bUseAlpha && PackOffset == 4)
				{
					*DestPtr16++ = *(uint16*)(SrcData16 + DataOffset + OffsetA); // A
					if (*(uint16*)(SrcData16 + DataOffset + OffsetA) != 0xFFFF)
						bHasAlphaValue = true;
				}
				else
					*DestPtr16++ = 0xFFFF;
			}
		}
	}
	else if (SrcFormat == TSF_RGBA16F)
	{
		FFloat16* DestPtr16f = nullptr;
		const FFloat16* SrcData16f = (const FFloat16*)SrcData;

		//const FFloat16 BytesPerPixel = sizeof(FFloat16) * 4;
		for (uint32 y = 0; y < SrcHeight; y++)
		{
			DestPtr16f = (FFloat16*)&MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FFloat16) * 4];

			for (uint32 x = 0; x < SrcWidth; x++)
			{
				uint32 DataOffset = y * SrcWidth * PackOffset + x * PackOffset;

				*DestPtr16f++ = *(FFloat16*)(SrcData16f + DataOffset + OffsetR); // R
				*DestPtr16f++ = *(FFloat16*)(SrcData16f + DataOffset + OffsetG); // G
				*DestPtr16f++ = *(FFloat16*)(SrcData16f + DataOffset + OffsetB); // B

				if (TextureParameters.bUseAlpha && PackOffset == 4)
				{
					*DestPtr16f++ = *(FFloat16*)(SrcData16f + DataOffset + OffsetA); // A
					if (*(FFloat16*)(SrcData16f + DataOffset + OffsetA) != 1.0f)
						bHasAlphaValue = true;
				}
				else
					*DestPtr16f++ = 1.0f;
			}
		}
	}	
	else if (SrcFormat == TSF_RGBA32F)
	{
		float* DestPtr32f = nullptr;
		const float* SrcData32f = (const float*)SrcData;

		//const float BytesPerPixel = sizeof(float) * 4;
		for (uint32 y = 0; y < SrcHeight; y++)
		{
			DestPtr32f = (float*)&MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(float) * 4];

			for (uint32 x = 0; x < SrcWidth; x++)
			{
				uint32 DataOffset = y * SrcWidth * PackOffset + x * PackOffset;

				*DestPtr32f++ = *(float*)(SrcData32f + DataOffset + OffsetR); // R
				*DestPtr32f++ = *(float*)(SrcData32f + DataOffset + OffsetG); // G
				*DestPtr32f++ = *(float*)(SrcData32f + DataOffset + OffsetB); // B

				if (TextureParameters.bUseAlpha && PackOffset == 4)
				{
					*DestPtr32f++ = *(float*)(SrcData32f + DataOffset + OffsetA); // A
					if (*(float*)(SrcData32f + DataOffset + OffsetA) != 1.0f)
						bHasAlphaValue = true;
				}
				else
					*DestPtr32f++ = 1.0f;
			}
		}
	}

	// Unlock the texture.
	Texture->Source.UnlockMip(0);

	// Texture creation parameters.
	Texture->SRGB = bSRGB ? 1 : 0;
	Texture->CompressionSettings = CompSetting;
	Texture->CompressionNoAlpha = !bHasAlphaValue;
	Texture->DeferCompression = bDeferComp;

	// Set the Source Guid/Hash if specified.
	/*
	if (TextureParameters.SourceGuidHash.IsValid())
	{
		Texture->Source.SetId(TextureParameters.SourceGuidHash, true);
	}
	*/
	Texture->UpdateResource();

	Texture->PostEditChange();

	return Texture;
}

bool
FHoudiniTextureTranslator::ProcessCopOutput(
	UHoudiniOutput* InOutput,
	const FHoudiniPackageParams& InPackageParams,
	EHoudiniEngineImageDataFormat ImageDataFormat)
{
	if (!InOutput)
		return false;

	if (InOutput->GetType() != EHoudiniOutputType::Cop)
		return false;

	// TODO: Delete previous output?
	TArray<UPackage*> DummyPackages;

	FString Notification = TEXT("BGEO Importer: Creating Cop Textures...");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	const TArray<FHoudiniGeoPartObject>& GeoPartObjects = InOutput->GetHoudiniGeoPartObjects();
	if (GeoPartObjects.Num() <= 0)
		return false;

	// TODO: Handle multiple geo/parts here?
	for (auto& HGPO : GeoPartObjects)
	{
		HAPI_NodeId CopNodeId = HGPO.GeoId;

		// Render the COP Output to an image
		// This will reset Image infos
		HAPI_Result Result;
		HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::RenderCOPOutputToImage(
			FHoudiniEngine::Get().GetSession(),
			CopNodeId,
			TCHAR_TO_ANSI(*HGPO.PartName)));

		if (HAPI_RESULT_SUCCESS != Result)
			continue;

		// Infer what that texture is using its name:
		// ie, an output named "normal" implies a normal map etc..
		EHoudiniTextureType TextureType = FHoudiniTextureTranslator::GetTextureTypeFromName(HGPO.PartName);
		FCreateTexture2DParameters CreateTexture2DParameters
			= FHoudiniTextureTranslator::GetTextureParametersFromType(TextureType);

		HAPI_ImageInfo ImageInfo;
		FHoudiniApi::ImageInfo_Init(&ImageInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImageInfo(
			FHoudiniEngine::Get().GetSession(),
			CopNodeId, &ImageInfo), false);

		// see if the user wants hdr (float) texture from its name
		bool bIsHDR = FHoudiniTextureTranslator::IsHDRFromName(HGPO.PartName);

		// see if the COP network is set to use 16 bits
		bool bIs16bit = FHoudiniTextureTranslator::Is16BitCOP(CopNodeId);

		float Gamma = 1.0;
		if (TextureType == EHoudiniTextureType::Diffuse
			|| TextureType == EHoudiniTextureType::Emissive)
		{
			Gamma = 2.2f;
		}

		// Data format can be overriden via the details panels
		switch (ImageDataFormat)
		{
			case EHoudiniEngineImageDataFormat::Int8:
				bIsHDR = false;
				ImageInfo.dataFormat = HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT8;
				break;

			case EHoudiniEngineImageDataFormat::Int16:
				bIsHDR = true;
				Gamma = 1.0; // unreal can only accept 16bit color in linear space
				ImageInfo.dataFormat = HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT16;
				break;

			case EHoudiniEngineImageDataFormat::Float16:
				bIsHDR = true;
				ImageInfo.dataFormat = HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT16;
				break;

			case EHoudiniEngineImageDataFormat::Float32:
				bIsHDR = true;
				ImageInfo.dataFormat = HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT32;
				break;

			case EHoudiniEngineImageDataFormat::Auto:
				if (bIsHDR)
				{
					ImageInfo.dataFormat = bIs16bit ? HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT16 : HAPI_ImageDataFormat::HAPI_IMAGE_DATA_FLOAT32;
				}
				else
				{
					ImageInfo.dataFormat = HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT8;
				}
				break;

			default:
				ImageInfo.dataFormat = HAPI_ImageDataFormat::HAPI_IMAGE_DATA_INT8;
				break;
		}

		ImageInfo.interleaved = true;
		ImageInfo.packing = HAPI_IMAGE_PACKING_RGBA;
		ImageInfo.gamma = bIsHDR ? 1.0 : Gamma;

		// Create custom package param for this output
		FHoudiniPackageParams MyPackageParams = InPackageParams;
		MyPackageParams.ObjectId = HGPO.ObjectId;
		MyPackageParams.GeoId = HGPO.GeoId;
		MyPackageParams.PartId = HGPO.PartId;
		MyPackageParams.SplitStr = HGPO.PartName;

		UTexture2D* Texture = nullptr;
		FHoudiniTextureTranslator::CreateTexture(
			CopNodeId,
			HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA,
			ImageInfo.dataFormat,
			HAPI_IMAGE_PACKING_RGBA,
			Gamma,
			Texture,
			HGPO.NodePath, 
			FHoudiniTextureTranslator::GetTextureTypeString(TextureType),
			MyPackageParams,
			CreateTexture2DParameters,
			TEXTUREGROUP_World,
			DummyPackages);

		FHoudiniOutputObjectIdentifier OutputID(HGPO.ObjectId, CopNodeId, HGPO.PartId, HGPO.PartName);
		FHoudiniOutputObject& FoundOutputObject = InOutput->GetOutputObjects().FindOrAdd(OutputID);
		FoundOutputObject.OutputComponents.Empty();
		FoundOutputObject.OutputObject = Texture;
	}

	return true;
}

UMaterialInterface*
FHoudiniTextureTranslator::CreateDefaultCopMaterialForTexture(
	UTexture2D* InTexture,
	const FHoudiniPackageParams & InPackageParams)
{
	// Try to find the material we want to create an instance of
	UMaterialInterface* SourceMaterial = Cast<UMaterialInterface>(
		FHoudiniEngine::Get().GetHoudiniDefaultCOPMaterial());
	if (!SourceMaterial)
		return nullptr;

	// Create/Retrieve the package for the MI
	FString MaterialInstanceName;
	FString MaterialInstanceNamePrefix = UPackageTools::SanitizePackageName(
		SourceMaterial->GetName()
		+ TEXT("_instance_")
		+ InPackageParams.ComponentGUID.ToString());

	// See if we can find an existing package for that instance
	UPackage* MaterialInstancePackage = FHoudiniMaterialTranslator::CreatePackageForMaterial(
		-1, MaterialInstanceNamePrefix, InPackageParams, MaterialInstanceName);

		// Couldn't create a package for that Material Instance
	if (!MaterialInstancePackage)
		return nullptr;

	bool bNewMaterialCreated = false;
	UMaterialInstanceConstant* NewMaterialInstance = LoadObject<UMaterialInstanceConstant>(
		MaterialInstancePackage, *MaterialInstanceName, nullptr, LOAD_None, nullptr);

	if (!NewMaterialInstance)
	{
		// Factory to create materials.
		UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
		if (!MaterialInstanceFactory)
			return nullptr;

		// Create the new material instance
		MaterialInstanceFactory->AddToRoot();
		MaterialInstanceFactory->InitialParent = SourceMaterial;
		NewMaterialInstance = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(), 
			MaterialInstancePackage,
			FName(*MaterialInstanceName),
			RF_Public | RF_Standalone,
			NULL,
			GWarn);

		if (NewMaterialInstance)
			bNewMaterialCreated = true;

		MaterialInstanceFactory->RemoveFromRoot();
	}

	if (!NewMaterialInstance)
		return nullptr;

	// Update context for generated materials (will trigger when the object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	// Apply material instance parameters
	FName MatParamName = FName("cop");
	NewMaterialInstance->SetTextureParameterValueEditorOnly(MatParamName, InTexture);
	bool bModifiedMaterialParameters = true;

	// Schedule this material for update if needed.
	if (bNewMaterialCreated || bModifiedMaterialParameters)
		MaterialUpdateContext.AddMaterialInstance(NewMaterialInstance);

	if (bNewMaterialCreated)
	{
		// Add meta information to this package.
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialInstanceName);
		// Notify registry that we have created a new material.
		FAssetRegistryModule::AssetCreated(NewMaterialInstance);
	}

	if (bNewMaterialCreated || bModifiedMaterialParameters)
	{
		// Dirty the material
		NewMaterialInstance->MarkPackageDirty();

		// Update the material instance
		NewMaterialInstance->InitStaticPermutation();
		NewMaterialInstance->PreEditChange(nullptr);
		NewMaterialInstance->PostEditChange();
	}

	return NewMaterialInstance;
}

bool
FHoudiniTextureTranslator::Is16BitCOP(const HAPI_NodeId& InNodeId)
{
	// Start by seeing if the HDA is a COP HDA
	HAPI_NodeInfo NodeInfo;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId, &NodeInfo))
		return false;

	if(HAPI_NODETYPE_COP != NodeInfo.type)
		return false;

	HAPI_NodeInfo ParentNodeInfo;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), NodeInfo.parentId, &ParentNodeInfo))
		return false;

	// TODO: Handle subnet - keep climbing hierarchy until we reach a COP net?

	// see if the parent COP network is set to override the precision
	int precisionOverride = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValue(
		FHoudiniEngine::Get().GetSession(),
		NodeInfo.parentId, "setprecision", 0, &precisionOverride))
		return false;

	if (precisionOverride == 0)
		return false;

	// see if the COP network is set to use 16 bits
	int precisionValue = 1;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValue(
		FHoudiniEngine::Get().GetSession(),
		NodeInfo.parentId, "precision", 0, &precisionValue))
		return false;

	return (precisionValue == 0);
}

bool
FHoudiniTextureTranslator::IsHDRFromName(const FString& Name)
{
	if (Name.Contains("hdr"))
		return true;
	
	return false;
}


EHoudiniTextureType
FHoudiniTextureTranslator::GetTextureTypeFromName(const FString& Name)
{
	// Default to a color/diffuse texture
	EHoudiniTextureType Type = EHoudiniTextureType::Diffuse;
	if (Name.Contains("normal"))
		Type = EHoudiniTextureType::Normal;
	else if (Name.Contains("specular"))
		Type = EHoudiniTextureType::Specular;
	else if (Name.Contains("roughness"))
		Type = EHoudiniTextureType::Roughness;
	else if (Name.Contains("emissive"))
		Type = EHoudiniTextureType::Emissive;
	else if (Name.Contains("opacity") 
		|| Name.Contains("alpha"))
		Type = EHoudiniTextureType::Opacity;
	else if (Name.Contains("occlusion")
		|| Name.Contains("ao"))
		Type = EHoudiniTextureType::Occlusion;
	else if (Name.Contains("displacement")
		|| Name.Contains("height"))
		Type = EHoudiniTextureType::Displacement;
	else if (Name.Contains("metal"))
		Type = EHoudiniTextureType::Metallic;
	else if (Name.Contains("rma") || Name.Contains("compact"))
		Type = EHoudiniTextureType::RMA;

	return Type;
}


FString
FHoudiniTextureTranslator::GetTextureTypeString(const EHoudiniTextureType& InType)
{
	FString TypeString = FString();
	switch (InType)
	{		
		case EHoudiniTextureType::Diffuse:
			TypeString = TEXT("diffuse");
			break;
		case EHoudiniTextureType::Metallic:
			TypeString = TEXT("metallic"); 
			break;
		case EHoudiniTextureType::Specular:
			TypeString = TEXT("specular"); 
			break;
		case EHoudiniTextureType::Roughness:
			TypeString = TEXT("roughness"); 
			break;
		case EHoudiniTextureType::Emissive:
			TypeString = TEXT("emissive"); 
			break;
		case EHoudiniTextureType::Opacity:
			TypeString = TEXT("opacity"); 
			break;
		case EHoudiniTextureType::Normal:
			TypeString = TEXT("normal"); 
			break;
		case EHoudiniTextureType::Occlusion:
			TypeString = TEXT("occlusion"); 
			break;
		case EHoudiniTextureType::Displacement:
			TypeString = TEXT("displacement"); 
			break;
		case EHoudiniTextureType::RMA:
			TypeString = TEXT("RMA");
			break;

		case EHoudiniTextureType::Invalid:
			break;
	}

	return TypeString;
}


FCreateTexture2DParameters
FHoudiniTextureTranslator::GetTextureParametersFromType(const EHoudiniTextureType& InType)
{
	FCreateTexture2DParameters TextureParams;
	TextureParams.SourceGuidHash = FGuid();
	TextureParams.bUseAlpha = false;
	TextureParams.bDeferCompression = true;
	TextureParams.bVirtualTexture = false;
	//TextureParams.MipGenSettings = TMGS_FromTextureGroup;
	//TextureParams.TextureGroup = TEXTUREGROUP_MAX;

	// Compression
	switch (InType)
	{
		// NORMAL
		case EHoudiniTextureType::Normal:
			TextureParams.CompressionSettings = TC_Normalmap;
			break;

		// GREYSCALE
		case EHoudiniTextureType::Metallic:
		case EHoudiniTextureType::Specular:
		case EHoudiniTextureType::Roughness:
		case EHoudiniTextureType::Opacity:
		case EHoudiniTextureType::Occlusion:
		case EHoudiniTextureType::Displacement:
			TextureParams.CompressionSettings = TC_Grayscale;
			break;

		// COMPACT
		case EHoudiniTextureType::RMA:
			TextureParams.CompressionSettings = TC_Masks;
			break;

		// DEFAULT
		case EHoudiniTextureType::Diffuse:
		case EHoudiniTextureType::Emissive:
		default:
			TextureParams.CompressionSettings = TC_Default;
			break;
	}
	
	// SRGB
	// Only for color channels: diffuse, emissive
	switch (InType)
	{
		// SRGB OFF
		case EHoudiniTextureType::Normal:
		case EHoudiniTextureType::Metallic:
		case EHoudiniTextureType::Specular:
		case EHoudiniTextureType::Roughness:
		case EHoudiniTextureType::Opacity:
		case EHoudiniTextureType::Occlusion:
		case EHoudiniTextureType::Displacement:
		case EHoudiniTextureType::RMA:
			TextureParams.bSRGB = false;
			break;

		// SRGB ON
		case EHoudiniTextureType::Diffuse:
		case EHoudiniTextureType::Emissive:
		default:
			TextureParams.bSRGB = true;
			break;
	}

	return TextureParams;
}

UMaterialInstanceConstant*
FHoudiniTextureTranslator::CreateCopOutputMaterialInstance(
	UMaterialInterface* InSourceMaterial,
	const FHoudiniPackageParams& InPackageParams)
{
	// Try to find the material we want to create an instance of, or use the default one if non is provided
	UMaterialInterface* SourceMaterial = InSourceMaterial ? InSourceMaterial
	: Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultCOPOutputMaterial());

	if (!SourceMaterial)
		return nullptr;

	// Create/Retrieve the package for the MI
	FString MaterialInstanceName;
	FString MaterialInstanceNamePrefix = UPackageTools::SanitizePackageName(
		SourceMaterial->GetName()
		+ TEXT("_instance_")
		+ InPackageParams.ComponentGUID.ToString());

	// See if we can find an existing package for that instance
	UPackage* MaterialInstancePackage = FHoudiniMaterialTranslator::CreatePackageForMaterial(
		-1, MaterialInstanceNamePrefix, InPackageParams, MaterialInstanceName);

	// Couldn't create a package for that Material Instance
	if (!MaterialInstancePackage)
		return nullptr;

	bool bNewMaterialCreated = false;
	UMaterialInstanceConstant* NewMaterialInstance = LoadObject<UMaterialInstanceConstant>(
		MaterialInstancePackage, *MaterialInstanceName, nullptr, LOAD_None, nullptr);

	if (!NewMaterialInstance)
	{
		// Factory to create materials.
		UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
		if (!MaterialInstanceFactory)
			return nullptr;

		// Create the new material instance
		MaterialInstanceFactory->AddToRoot();
		MaterialInstanceFactory->InitialParent = SourceMaterial;
		NewMaterialInstance = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			MaterialInstancePackage,
			FName(*MaterialInstanceName),
			RF_Public | RF_Standalone,
			NULL,
			GWarn);

		if (NewMaterialInstance)
			bNewMaterialCreated = true;

		MaterialInstanceFactory->RemoveFromRoot();
	}

	if (!NewMaterialInstance)
		return nullptr;

	// Update context for generated materials (will trigger when the object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	if (bNewMaterialCreated)
	{
		// Add meta information to this package.
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialInstanceName);
		// Notify registry that we have created a new material.
		FAssetRegistryModule::AssetCreated(NewMaterialInstance);

		// Dirty the material
		NewMaterialInstance->MarkPackageDirty();
	}

	return NewMaterialInstance;
}

bool
FHoudiniTextureTranslator::UpdateTexureParamOnCopOutputMaterialInstance(
	UMaterialInstanceConstant* InSourceMaterial,
	UTexture2D* InTexture,
	const FString& InParamName)
{
	if(!InSourceMaterial || !InTexture)
		return false;

	// Update context for generated materials (will trigger when the object goes out of scope).
	//FMaterialUpdateContext MaterialUpdateContext;

	// Apply material instance parameters
	FName MatParamName = FName(InParamName);
	//MaterialInstance->GetParameterInfo(GlobalParameter, MatParamName, nullptr);

	InSourceMaterial->SetTextureParameterValueEditorOnly(MatParamName, InTexture);

	return true;
}

bool
FHoudiniTextureTranslator::UpdateBooleanParamOnCopOutputMaterialInstance(
	UMaterialInstanceConstant* InSourceMaterial,
	const bool InValue,
	const FString& InParamName)
{
	if (!InSourceMaterial)
		return false;

	// Update context for generated materials (will trigger when the object goes out of scope).
	//FMaterialUpdateContext MaterialUpdateContext;

	// Apply material instance parameters
	FName MatParamName = FName(InParamName);
	InSourceMaterial->SetStaticSwitchParameterValueEditorOnly(MatParamName, InValue);

	return true;
}