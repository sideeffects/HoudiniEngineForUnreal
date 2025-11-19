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
	const HAPI_ImageDataFormat InImageDataFormat,
	const HAPI_ImagePacking InImagePacking,
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

	ImageInfo.dataFormat = InImageDataFormat;
	ImageInfo.interleaved = true;
	ImageInfo.packing = InImagePacking;

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

	// If we have a part name (output name) be sure to it
	if(!InPackageParams.SplitStr.IsEmpty())
		MyPackageParams.ObjectName += InPackageParams.SplitStr;

	return MyPackageParams.CreatePackageForObject(OutTextureName);
}

bool
FHoudiniTextureTranslator::CreateTexture(
	// HAPI extraction parameters
	const HAPI_NodeId InMaterialNodeId,
	const char* InPlaneType,
	HAPI_ImageDataFormat InImageDataFormat,
	HAPI_ImagePacking InImagePacking,
	// Texture creation parameters
	UTexture2D*& OutTexture,
	const FString& InNodePath,
	const FString& InTextureType,
	const FHoudiniPackageParams& InPackageParams,
	const FCreateTexture2DParameters& InTextureParameters,
	const TextureGroup InLODGroup,
	TArray<UPackage*>& OutPackages)
{
	bool bTextureCreated = false;
	TArray<char> ImageBuffer;
	if (FHoudiniTextureTranslator::HapiExtractImage(
		InMaterialNodeId, InPlaneType, InImageDataFormat, InImagePacking, ImageBuffer))
	{
		UPackage* TexturePackage = nullptr;
		if (IsValid(OutTexture))
			TexturePackage = Cast<UPackage>(OutTexture->GetOuter());

		HAPI_ImageInfo ImageInfo;
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

	// Initialize texture source.
	Texture->Source.Init(ImageInfo.xRes, ImageInfo.yRes, 1, 1, TSF_BGRA8);

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

	bool bHasAlphaValue = false;
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

	// Unlock the texture.
	Texture->Source.UnlockMip(0);

	// Texture creation parameters.
	Texture->SRGB = TextureParameters.bSRGB;
	Texture->CompressionSettings = TextureParameters.CompressionSettings;
	Texture->CompressionNoAlpha = !bHasAlphaValue;
	Texture->DeferCompression = TextureParameters.bDeferCompression;

	// Set the Source Guid/Hash if specified.
	/*
	if (TextureParameters.SourceGuidHash.IsValid())
	{
		Texture->Source.SetId(TextureParameters.SourceGuidHash, true);
	}
	*/

	Texture->PostEditChange();

	return Texture;
}

bool
FHoudiniTextureTranslator::ProcessCopOutput(
	UHoudiniOutput* InOutput,
	const FHoudiniPackageParams& InPackageParams)
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

		//bool bRenderSuccessful = FHoudiniTextureTranslator::HapiRenderCOPTexture(CopNodeId);

		// TEST
		bool bRenderSuccessful = false;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::RenderCOPOutputToImage(
			FHoudiniEngine::Get().GetSession(),
			CopNodeId,
			TCHAR_TO_ANSI(*HGPO.PartName)), false);

		bRenderSuccessful = true;

		if (!bRenderSuccessful)
			continue;

		FCreateTexture2DParameters CreateTexture2DParameters;
		CreateTexture2DParameters.SourceGuidHash = FGuid();
		CreateTexture2DParameters.bUseAlpha = true;
		CreateTexture2DParameters.CompressionSettings = TC_Default;
		CreateTexture2DParameters.bDeferCompression = true;
		CreateTexture2DParameters.bSRGB = true;

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
			HAPI_IMAGE_DATA_INT8,
			HAPI_IMAGE_PACKING_RGBA,
			Texture,
			"",
			"",
			MyPackageParams,
			CreateTexture2DParameters,
			TEXTUREGROUP_World,
			DummyPackages);

		FHoudiniOutputObjectIdentifier OutputID(HGPO.ObjectId, CopNodeId, HGPO.PartId, HGPO.PartName);
		FHoudiniOutputObject& FoundOutputObject = InOutput->GetOutputObjects().FindOrAdd(OutputID);
		FoundOutputObject.OutputComponents.Empty();
		FoundOutputObject.OutputObject = Texture;

		/*if (Texture)
		{
			UMaterialInterface* NewMatInterface = CreateDefaultCopMaterialForTexture(Texture, InPackageParams);
			FoundOutputObject.OutputComponents.Add(NewMatInterface);
		}*/
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