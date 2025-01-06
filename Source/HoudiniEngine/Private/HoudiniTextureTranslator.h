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

#pragma once

#include "CoreMinimal.h"
#include "HAPI/HAPI_Common.h"
#include "Engine/TextureDefines.h"
#include "Engine/EngineTypes.h"

class UPackage;
class UTexture2D;

struct FHoudiniPackageParams;
struct FCreateTexture2DParameters;

struct HOUDINIENGINE_API FHoudiniTextureTranslator
{
public:
	// Get image planes from HAPI
	static bool HapiGetImagePlanes(
		const HAPI_ParmId InNodeParmId, const HAPI_MaterialInfo& InMaterialInfo, TArray<FString>& OutImagePlanes);

	// Retrieve information based on the planes of the HAPI material.
	static bool GetPlaneInfo(
		const HAPI_ParmId InParmTextureId,
		const HAPI_MaterialInfo& InMaterialInfo,
		HAPI_ImagePacking& OutImagePacking,
		const char*& OutPlaneType,
		bool& bOutUseAlpha);

	// Render a texture off a Houdini material node.
	// Note that GetPlaneInfo() renders the texture too. So if you have called GetPlaneInfo(), you don't need to call this.
	static bool HapiRenderTexture(
		const HAPI_NodeId InMaterialNodeId,
		const HAPI_ParmId InTextureParmId);

	static bool HapiRenderCOPTexture(
		const HAPI_NodeId InCopNodeId);

	// HAPI : Retrieve a list of image planes.
	static bool HapiExtractImage(
		const HAPI_NodeId InMaterialNodeId,
		const char* InPlaneType,
		const HAPI_ImageDataFormat InImageDataFormat,
		const HAPI_ImagePacking InImagePacking,
		TArray<char>& OutImageBuffer);

	static UPackage* CreatePackageForTexture(
		const HAPI_NodeId& InMaterialNodeId,
		const FString& InTextureType,
		const FHoudiniPackageParams& InPackageParams,
		FString& OutTextureName);

	// Create a texture from a HAPI material.
	// Returns true if the texture is successfully created, false otherwise.
	static bool CreateTexture(
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
		TArray<UPackage*>& OutPackages);

	static UTexture2D* CreateUnrealTexture(
		UTexture2D* ExistingTexture,
		const HAPI_ImageInfo& ImageInfo,
		UPackage* Package,
		const FString& TextureName,
		const TArray<char>& ImageBuffer,
		const FCreateTexture2DParameters& TextureParameters,
		const TextureGroup LODGroup, 
		const FString& TextureType,
		const FString& NodePath);
};