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

#include "Landscape.h"
#include "HAPI/HAPI_Common.h"

class ALandscapeProxy;
class UHoudiniInputLandscape;
class FUnrealObjectInputHandle;
class UHoudiniInput;

struct FHoudiniLandscapeExportOptions
{
	bool bExportHeightDataPerEditLayer;
	bool bExportPaintLayersPerEditLayer;
	bool bExportMergedPaintLayers;
};

struct HOUDINIENGINE_API FUnrealLandscapeTranslator 
{
	public:

		// ------------------------------------------------------------------------------------------
		// Unreal Landscape to Houdini Heightfield
		// ------------------------------------------------------------------------------------------
		static bool CreateHeightfieldFromLandscape(
			ALandscapeProxy* LandcapeProxy,
			const FHoudiniLandscapeExportOptions & Options,
			HAPI_NodeId& CreatedHeightfieldNodeId,
			const FString &InputNodeNameStr,
			HAPI_NodeId ParentNodeId,
			bool bSetObjectTransformToWorldTransform);

		static bool CreateHeightfieldFromLandscapeComponentArray(
			ALandscapeProxy* LandscapeProxy,
			const TSet< ULandscapeComponent * > & SelectedComponents,
			const FHoudiniLandscapeExportOptions& Options,
			HAPI_NodeId& CreatedHeightfieldNodeId,
			const FString &InputNodeNameStr,
			const HAPI_NodeId& ParentNodeId,
			bool bSetObjectTransformToWorldTransform);
	
		static bool CreateHeightfieldFromLandscapeComponent(
			ALandscapeProxy* LandscapeProxy, 
			ULandscapeComponent * LandscapeComponent,
			const int32& ComponentIndex,
			HAPI_NodeId& HeightFieldId,
			HAPI_NodeId& MergeId,
			int32& MergeInputIndex,
			const FHoudiniLandscapeExportOptions& Options,
			const FString& InputNodeNameStr,
			const FTransform & ParentTransform,
			const HAPI_NodeId& ParentNodeId);
	
		static bool CreateInputNodeForLandscape(
			ALandscapeProxy* LandscapeProxy,
			const FString& InputNodeNameStr,
			const FString& HeightFieldName,
			const FTransform& LandscapeTransform,
			HAPI_NodeId& HeightId,
			HAPI_PartId& PartId,
			HAPI_NodeId& HeightFieldId,
			HAPI_NodeId& MaskId,
			HAPI_NodeId& MergeId,
			TArray<uint16>& HeightData,
			HAPI_VolumeInfo& HeightfieldVolumeInfo,
			int32& XSize,
			int32& YSize,
			const HAPI_NodeId& ParentNodeId);

		static bool CreateInputNodeForLandscapeObject(
			ALandscapeProxy* InLandscape,
			UHoudiniInput* InInput,
			HAPI_NodeId& InputNodeId,
			const FString& InputNodeName,
			FUnrealObjectInputHandle& OutHandle,
			const bool& bInputNodesCanBeDeleted);

		static void ApplyAttributesToHeightfieldNode(
			const HAPI_NodeId HeightId,
			const HAPI_PartId PartId,
			ALandscapeProxy* LandscapeProxy
			);

		// Extracts the uint16 values of a given landscape
		static bool GetLandscapeData(
			ALandscapeProxy* LandscapeProxy,
			TArray<uint16>& HeightData,
			int32& XSize, int32& YSize,
			FVector3d& Min, FVector3d& Max);

		static bool GetLandscapeData(
			ULandscapeInfo* LandscapeInfo,
			const int32& MinX,
			const int32& MinY,
			const int32& MaxX,
			const int32& MaxY,
			TArray<uint16>& HeightData,
			int32& XSize, int32& YSize);

		static void GetLandscapeProxyBounds(
			ALandscapeProxy* LandscapeProxy,
			FVector3d& Origin, FVector3d& Extents);

		// Converts Unreal uint16 values to Houdini Float
		static bool ConvertLandscapeDataToHeightFieldData(
			const TArray<uint16>& IntHeightData,
			int32 XSize,
			int32 YSize,
			FVector Min,
			FVector Max,
			const FVector3d& LandscapeActorScale,
			TArray<float>& HeightfieldFloatValues,
			HAPI_VolumeInfo& HeightfieldVolumeInfo);

		// Converts Unreal uint8 values to Houdini Float
		static bool ConvertLandscapeLayerDataToHeightfieldData(
			const TArray<uint8>& IntHeightData,
			int32 XSize, int32 YSize,
			const FLinearColor& LayerUsageDebugColor,
			TArray<float>& LayerFloatValues);

		// Creates an unlocked heightfield input node
		static bool CreateHeightfieldInputNode(
			const FString& NodeName,
			const int32& XSize, 
			const int32& YSize,
			HAPI_NodeId& HeightfieldNodeId, 
			HAPI_NodeId& HeightNodeId,
			HAPI_NodeId& MaskNodeId,
			HAPI_NodeId& MergeNodeId,
			HAPI_NodeId ParentNodeId);

		// Set the volume float value for a heightfield
		static bool SetHeightfieldData(
			const HAPI_NodeId& AssetId,
			const HAPI_PartId& PartId,
			TArray< float >& FloatValues,
			const HAPI_VolumeInfo& VolumeInfo,
			const FString& HeightfieldName);

		static bool AddLandscapeMaterialAttributesToVolume(
			const HAPI_NodeId& VolumeNodeId,
			const HAPI_PartId& PartId,
			UMaterialInterface* LandscapeMaterial,
			UMaterialInterface* LandscapeHoleMaterial,
			UPhysicalMaterial* InPhysicalMaterial);

		/*
		static bool AddLevelPathAttributeToVolume(
			const HAPI_NodeId& VolumeNodeId,
			const HAPI_PartId& PartId,
			const FString& LevelPath);
		*/

		// Extracts the uint8 values of a given landscape
		static bool GetLandscapeTargetLayerData(
			ALandscapeProxy* LandscapeProxy,
			ULandscapeInfo* LandscapeInfo,
			int32 TargetLayerIndex,
			TArray<uint8>& TargetLayerData,
			FLinearColor& TargetLayerDebugColor,
			FString& TaregtLayerName);

		static bool GetLandscapeTargetLayerData(
			ULandscapeInfo* LandscapeInfo,
			int32 TargetLayerIndex,
			int32 MinX,
			int32 MinY,
			int32 MaxX,
			int32 MaxY,
			TArray<uint8>& TargetLayerData,
			FLinearColor& TargetLayerUsageDebugColor,
			FString& TargetLayerName);

		// Initialise the Heightfield Mask with default values
		static bool InitDefaultHeightfieldMask(
			const HAPI_VolumeInfo& HeightVolumeInfo,
			const HAPI_NodeId& MaskVolumeNodeId);

		// Landscape nodes clean up
		static bool DestroyLandscapeAssetNode(
			HAPI_NodeId& ConnectedAssetId,
			TArray<HAPI_NodeId>& CreatedInputAssetIds);


		//--------------------------------------------------------------------------------------------------
		// Unreal to Houdini - MESH / POINTS
		//--------------------------------------------------------------------------------------------------

		static bool CreateMeshOrPointsFromLandscape(
			ALandscapeProxy* InLandscape,
			HAPI_NodeId& InputNodeId,
			const FString& InInputNodeNameString,
			const bool bExportGeometryAsMesh,
			const bool bExportTileUVs,
			const bool bExportNormalizedUVs,
			const bool bExportLighting,
			const bool bExportMaterials,
			const bool bApplyWorldTransform,
			const HAPI_NodeId& ParentNodeId);

		// Extract data from the landscape
		static bool ExtractLandscapeData(
			ALandscapeProxy * LandscapeProxy,
			TSet<ULandscapeComponent *>& SelectedComponents,
			const bool& bExportLighting,
			const bool& bExportTileUVs,
			const bool& bExportNormalizedUVs,
			bool bApplyWorldTransform,
			TArray<FVector3f>& LandscapePositionArray,
			TArray<FVector3f>& LandscapeNormalArray,
			TArray<FVector3f>& LandscapeUVArray,
			TArray<FIntPoint>& LandscapeComponentVertexIndicesArray,
			TArray<const char *>& LandscapeComponentNameArray,
			TArray<FLinearColor>& LandscapeLightmapValues);

		// Helper functions to extract color from a texture
		static FColor PickVertexColorFromTextureMip(
			const uint8 * MipBytes,
			FVector2D & UVCoord,
			int32 MipWidth,
			int32 MipHeight);

		// Add the Position attribute extracted from a landscape
		static bool AddLandscapePositionAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<FVector3f>& LandscapePositionArray);

		// Add the Normal attribute extracted from a landscape
		static bool AddLandscapeNormalAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<FVector3f>& LandscapeNormalArray);

		// Add the UV attribute extracted from a landscape
		static bool AddLandscapeUVAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<FVector3f>& LandscapeUVArray);

		// Add the Component Vertex Index attribute extracted from a landscape
		static bool AddLandscapeComponentVertexIndicesAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<FIntPoint>& LandscapeComponentVertexIndicesArray);

		// Add the Component Name attribute extracted from a landscape
		static bool AddLandscapeComponentNameAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<const char *>& LandscapeComponentNameArray);

		static bool AddLandscapeTileAttribute(
			const HAPI_NodeId& NodeId, const HAPI_PartId& PartId, const int32& TileIdx );
	
		// Add the lightmap color attribute extracted from a landscape
		static bool AddLandscapeLightmapColorAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<FLinearColor>& LandscapeLightmapValues);

		// Creates and add the vertex indices and face materials attribute from a landscape
		static bool AddLandscapeMeshIndicesAndMaterialsAttribute(
			const HAPI_NodeId& NodeId, 
			const bool& bExportMaterials,
			const int32& ComponentSizeQuads, 
			const int32& QuadCount,
			ALandscapeProxy * LandscapeProxy,
			const TSet<ULandscapeComponent *>& SelectedComponents);

		// Add the global (detail) material and hole material attribute from a landscape
		static bool AddLandscapeGlobalMaterialAttribute(
			const HAPI_NodeId& NodeId,
			ALandscapeProxy * LandscapeProxy);

		// Add landscape layer values as point attributes
		static bool AddLandscapeLayerAttribute(
			const HAPI_NodeId& NodeId,
			const TArray<float>& LandscapeLayerArray,
			const FString& LayerName);

		static bool SendCombinedTargetLayersToHoudini(
			ALandscapeProxy* LandscapeProxy,
			HAPI_NodeId HeightFieldId,
			HAPI_PartId PartId,
			HAPI_NodeId MergeId,
			HAPI_NodeId MaskId,
			const HAPI_VolumeInfo& HeightfieldVolumeInfo,
			int32 XSize,
			int32 YSize,
			int32 & OutMergeInputIndex);


		static bool SendAllEditLayerTargetLayersToHoudini(
			ALandscapeProxy* LandscapeProxy,
			HAPI_NodeId HeightFieldId,
			HAPI_PartId PartId,
			HAPI_NodeId MergeId,
			HAPI_NodeId MaskId,
			const HAPI_VolumeInfo& HeightfieldVolumeInfo,
			int32 XSize,
			int32 YSize,
			int32& OutMergeInputIndex);

		static bool SendTargetLayersToHoudini(
			ALandscapeProxy* LandscapeProxy,
			HAPI_NodeId HeightFieldId,
			HAPI_PartId PartId,
			HAPI_NodeId MergeId,
			HAPI_NodeId MaskId,
			const FHoudiniLandscapeExportOptions& Options,
			const HAPI_VolumeInfo& HeightfieldVolumeInfo,
			int32 XSize,
			int32 YSize,
			int32& OutMergeInputIndex);

		static HAPI_NodeId CreateVolumeLayer(ALandscapeProxy* LandscapeProxy,
			const FString& VolumeNameLayer,
			const HAPI_Transform& NodeTransform,
			HAPI_NodeId HeightFieldId,
			HAPI_PartId PartId,
			HAPI_PartId MaskId,
			int XSize,
			int YSize,
			TArray<float>& Data);
};