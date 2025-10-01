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

#include "UnrealTextureTranslator.h"
#include "HoudiniEngineUtils.h"
#include "TextureResource.h"


bool
FUnrealTextureTranslator::HapiCreateCOPTexture(
	UTexture2D* Texture,
	const HAPI_NodeId ParentNode)
{
	// The texture needs certain settings, otherwise RawData->Lock() will fail.
	// So we save the old settings so we can restore them later.
	TextureCompressionSettings OldCompressionSettings = Texture->CompressionSettings;
	TextureMipGenSettings OldMipGenSettings = Texture->MipGenSettings;
	bool OldSRGB = Texture->SRGB;
	auto RestoreTexture = [&]()
	{
		Texture->CompressionSettings = OldCompressionSettings;
		Texture->MipGenSettings = OldMipGenSettings;
		Texture->SRGB = OldSRGB;
		Texture->UpdateResource();
	};
	Texture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	Texture->SRGB = false;
	Texture->UpdateResource();

	// Get the texture's first mipmap (i.e. the full resolution texture).
	if (Texture->GetNumMips() == 0)
	{
		RestoreTexture();
		return false;
	}
	FTexture2DMipMap* MipMap = &Texture->GetPlatformData()->Mips[0];
	if (!MipMap)
	{
		RestoreTexture();
		return false;
	}

	// Get a pointer to the mipmap's raw pixel data.
	FByteBulkData* RawData = &MipMap->BulkData;
	if (!RawData || RawData->IsLocked())
	{
		RestoreTexture();
		return false;
	}
	FColor* ImageColors = static_cast<FColor*>(RawData->Lock(LOCK_READ_ONLY));
	if (!ImageColors)
	{
		// Unlocks the raw data if the image data wasn't casted successfully.
		if (RawData->IsLocked())
			RawData->Unlock();
		RestoreTexture();
		return false;
	}

	// Convert the data to linear color space, and save it to a new array.
	float* ImageData = new float[MipMap->SizeX * MipMap->SizeY * 4];
	for (size_t i = 0; i < MipMap->SizeX * MipMap->SizeY; i++)
	{
		FLinearColor LinearImageColor = FLinearColor(ImageColors[i]);
		ImageData[i*4] = LinearImageColor.R;
		ImageData[i*4+1] = LinearImageColor.G;
		ImageData[i*4+2] = LinearImageColor.B;
		ImageData[i*4+3] = LinearImageColor.A;
	}

	// Create the COP in Houdini.
	bool bSuccess = HAPI_RESULT_SUCCESS == FHoudiniApi::CreateCOPImage(
		FHoudiniEngine::Get().GetSession(),
		ParentNode, 
		MipMap->SizeX, 
		MipMap->SizeY,
		HAPI_IMAGE_PACKING_RGBA,
		false,
		true, 
		ImageData,
		0, 
		MipMap->SizeX*MipMap->SizeY*4);

	// Clean up.
	RawData->Unlock();
	RestoreTexture();
	delete[] ImageData;

	return bSuccess;
}

bool
FUnrealTextureTranslator::CreateGeometryForTexture(
	HAPI_NodeId ParentNodeId,
	HAPI_NodeId& CreatedOutNodeId)
{
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();

	// Check ParentNodeId is a Geo ?
	CreatedOutNodeId = -1;

	// Output Node
	HAPI_NodeId OutNodeId = -1;
	HAPI_Result Result = FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("output"), TEXT("OUT"), true, &OutNodeId);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::CreateInputNode]: CreateNode failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	// Grid Node
	HAPI_NodeId GridNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("grid"), TEXT("Grid"), true, &GridNodeId), false);
	// Size 1 1 - sizex sizey
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(Session, GridNodeId, "size", 0, 1.0), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(Session, GridNodeId, "size", 1, 1.0), false);
	// XY Plane - orient 0
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, GridNodeId, "orient", 0, 0), false);
	// rows 2 cols 2
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, GridNodeId, "rows", 0, 2), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, GridNodeId, "cols", 0, 2), false);

	// uv project Node
	HAPI_NodeId UVNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("uvproject"), TEXT("UV"), true, &UVNodeId), false);

	// cop preview mat Node	
	HAPI_NodeId MatNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("coppreviewmaterial"), TEXT("COP Preview Material"), true, &MatNodeId), false);
	// basecolorsource 1 (COP)
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, MatNodeId, "basecolorsource", 0, 1), false);
	// basecolorpath ../copmemoryimport1
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(Session, MatNodeId, "basecolorpath", &ParmId), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(Session, MatNodeId, "../copmemoryimport1/texture1", ParmId, 0), false);

	// Connect the nodes together
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, OutNodeId, 0, MatNodeId, 0), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, MatNodeId, 0, UVNodeId, 0), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, UVNodeId, 0, GridNodeId, 0), false);

	// Return the output node as the created node ID
	CreatedOutNodeId = OutNodeId;

	return true;
}
