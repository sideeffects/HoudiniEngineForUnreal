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

#include "HAPI/HAPI_Common.h"
#include "HoudiniGeoPartObject.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/TextureDefines.h"

#include <string>

class UMaterial;
class UMaterialInterface;
class UMaterialExpression;
class UMaterialInstanceConstant;
class UTexture2D;
class UTexture;
class UPackage;

struct FHoudiniPackageParams;
struct FCreateTexture2DParameters;
struct FHoudiniGenericAttribute;

// Forward declared enums do not work with 4.24 builds on Linux with the Clang 8.0.1 toolchain: ISO C++ forbids forward references to 'enum' types
// enum TextureGroup;

struct HOUDINIENGINE_API FHoudiniMaterialTranslator
{
public:

	// Helper function to handle difference with material expressions collection in UE5.1
	static void _AddMaterialExpression(
		UMaterial* InMaterial, UMaterialExpression* InMatExp);

	//
	static bool CreateHoudiniMaterials(
		const HAPI_NodeId& InNodeId,
		const FHoudiniPackageParams& InPackageParams,
		const TArray<int32>& InUniqueMaterialIds,
		const TArray<HAPI_MaterialInfo>& InUniqueMaterialInfos,
		const TMap<FString, UMaterialInterface *>& InMaterials,
		const TMap<FString, UMaterialInterface *>& InAllOutputMaterials,
		TMap<FString, UMaterialInterface *>& OutMaterials,
		TArray<UPackage*>& OutPackages,
		const bool& bForceRecookAll,
		bool bInTreatExistingMaterialsAsUpToDate=false);

	//
	static bool CreateMaterialInstances(
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams,
		const TMap<FString, int32>& UniqueMaterialInstanceOverrides,
		const TArray<UPackage*>& InPackages,
		const TMap<FString, UMaterialInterface *>& InMaterials,
		TMap<FString, UMaterialInterface *>& OutMaterials,
		const bool& bForceRecookAll);


	// Helper for CreateMaterialInstances so you don't have to sort unique face materials overrides
	static bool SortUniqueFaceMaterialOverridesAndCreateMaterialInstances(
		const TArray<FString>& Materials,
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams,
		const TArray<UPackage*>& InPackages,
		const TMap<FString, UMaterialInterface *>& InMaterials,
		TMap<FString, UMaterialInterface *>& OutMaterials,
		const bool& bForceRecookAll);
	
	//
	static bool UpdateMaterialInstanceParameter(
		FHoudiniGenericAttribute MaterialParameter,
		UMaterialInstanceConstant* MaterialInstance,
		const TArray<UPackage*>& InPackages);

	static UTexture* FindGeneratedTexture(
		const FString& TextureString,
		const TArray<UPackage*>& InPackages);

	//
	static UPackage* CreatePackageForTexture(
		const HAPI_NodeId& InMaterialNodeId,
		const FString& InTextureType,
		const FHoudiniPackageParams& InPackageParams,
		FString& OutTextureName);

	//
	static UPackage* CreatePackageForMaterial(
		const HAPI_NodeId& InMaterialNodeId,
		const FString& InMaterialName,
		const FHoudiniPackageParams& InPackageParams,
		FString& OutMaterialName);


	// Create a texture from given information.
	static UTexture2D* CreateUnrealTexture(
		UTexture2D* ExistingTexture,
		const HAPI_ImageInfo& ImageInfo,
		UPackage* Package,
		const FString& TextureName,
		const TArray<char>& ImageBuffer,
		const FCreateTexture2DParameters& TextureParameters,
		const TextureGroup& LODGroup,
		const FString& TextureType,
		const FString& NodePath);

	// HAPI : Retrieve a list of image planes.
	static bool HapiExtractImage(
		const HAPI_ParmId& NodeParmId,
		const HAPI_MaterialInfo& MaterialInfo,
		const char * PlaneType,
		const HAPI_ImageDataFormat& ImageDataFormat,
		HAPI_ImagePacking ImagePacking,
		bool bRenderToImage,
		TArray<char>& OutImageBuffer);

	// HAPI : Extract image data.
	static bool HapiGetImagePlanes(
		const HAPI_ParmId& NodeParmId, const HAPI_MaterialInfo& MaterialInfo, TArray<FString>& OutImagePlanes);
	
	// Returns a unique name for a given material, its relative path (to the asset)
	static bool GetMaterialRelativePath(
		const HAPI_NodeId& InAssetId, const HAPI_MaterialInfo& InMaterialNodeInfo, FString& OutRelativePath);
	static bool GetMaterialRelativePath(
		const HAPI_NodeId& InAssetId, const HAPI_NodeId& InMaterialNodeId, FString& OutRelativePath);

	// Returns true if a texture parameter was found
	// Ensures that the texture is not disabled via the "UseTexture" Parm name/tag
	static bool FindTextureParamByNameOrTag(
		const HAPI_NodeId& InNodeId,
		const std::string& InTextureParmName,
		const std::string& InUseTextureParmName,
		const bool& bFindByTag,
		HAPI_ParmId& OutParmId,
		HAPI_ParmInfo& OutParmInfo);
		
protected:

	// Helper function to locate first Material expression of given class within given expression subgraph.
	static UMaterialExpression * MaterialLocateExpression(UMaterialExpression* Expression, UClass* ExpressionClass);

	// Create various material components.
	static bool CreateMaterialComponentDiffuse(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName, 
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentNormal(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentSpecular(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentRoughness(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentMetallic(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentEmissive(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentOpacity(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentOpacityMask(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

public:

	// Material node construction offsets.
	static const int32 MaterialExpressionNodeX;
	static const int32 MaterialExpressionNodeY;
	static const int32 MaterialExpressionNodeStepX;
	static const int32 MaterialExpressionNodeStepY;
};