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
* Produced by:
*      Damien Pernuit
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

#include "HoudiniGeoPartObject.h"
#include "Landscape.h"

struct FHoudiniCookParams;

struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeUtils
{
    public:

        //--------------------------------------------------------------------------------------------------
        // Houdini to Unreal
        //--------------------------------------------------------------------------------------------------
#if WITH_EDITOR
        // Creates all the landscapes/layers from the volume array
        static bool CreateAllLandscapes(
            FHoudiniCookParams& HoudiniCookParams,
            const TArray< FHoudiniGeoPartObject > & FoundVolumes,
            TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >& Landscapes,
            TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >& NewLandscapes,
            TArray<ALandscapeProxy *>& InputLandscapeToUpdate,
            float ForcedZMin = 0.0f, float ForcedZMax = 0.0f );

        // Creates a single landscape object from the converted data
        static ALandscapeProxy * CreateLandscape(
            const TArray< uint16 >& IntHeightData,
            const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
            const FTransform& LandscapeTransform,
            const int32& XSize, const int32& YSize,
            const int32& NumSectionPerLandscapeComponent,
            const int32& NumQuadsPerLandscapeSection,
            UMaterialInterface* LandscapeMaterial,
            UMaterialInterface* LandscapeHoleMaterial,
			const bool& CreateLandscapeStreamingProxy );

        // Returns the materials assigned to the heightfield
        static void GetHeightFieldLandscapeMaterials(
            const FHoudiniGeoPartObject& Heightfield,
            UMaterialInterface*& LandscapeMaterial,
            UMaterialInterface*& LandscapeHoleMaterial );

        // Creates the package needed to store landscape layer infos
        static ULandscapeLayerInfoObject* CreateLandscapeLayerInfoObject( 
            FHoudiniCookParams& HoudiniCookParams, const TCHAR* LayerName, UPackage*& Package, HAPI_PartId PartId );

        // Creates all the landscape layers for a given heightfield
        static bool CreateLandscapeLayers(
            FHoudiniCookParams& HoudiniCookParams,
            const TArray< const FHoudiniGeoPartObject* >& FoundLayers,
            const FHoudiniGeoPartObject& Heightfield,
            const int32& LandscapeXSize, const int32& LandscapeYSize,
            const TMap<FString, float>& GlobalMinimums,
            const TMap<FString, float>& GlobalMaximums,
            TArray<FLandscapeImportLayerInfo>& ImportLayerInfos );

        /** Updates a reference to a generated landscape by the newly created one **/
        static bool UpdateOldLandscapeReference(
			ALandscapeProxy* OldLandscape, ALandscapeProxy*  NewLandscape );
#endif
        // Returns Heightfield contained in the GeoPartObject array
        static void GetHeightfieldsInArray(
            const TArray< FHoudiniGeoPartObject >& InArray,
            TArray< const FHoudiniGeoPartObject* >& OutHeightfields );

        // Returns the layers corresponding to a height field contained in the GeoPartObject array
        static void GetHeightfieldsLayersInArray(
            const TArray< FHoudiniGeoPartObject >& InArray,
            const FHoudiniGeoPartObject& Heightfield,
            TArray< const FHoudiniGeoPartObject* >& FoundLayers );

        // Returns the global ZMin/ZMax value of all heightfields contained in the array
        static void CalcHeightfieldsArrayGlobalZMinZMax(
            const TArray< const FHoudiniGeoPartObject* >& InHeightfieldArray,
            float& fGlobalMin, float& fGlobalMax );

        // Returns the global ZMin/ZMax value per volume for all the heightfields contained in the array
        static void CalcHeightfieldsArrayGlobalZMinZMax(
            const TArray< FHoudiniGeoPartObject >& InHeightfieldArray,
            TMap<FString, float>& GlobalMinimums,
            TMap<FString, float>& GlobalMaximums);

        // Extract the float values of a given heightfield
        static bool GetHeightfieldData(
            const FHoudiniGeoPartObject& Heightfield,
            TArray< float >& FloatValues,
            HAPI_VolumeInfo& VolumeInfo,
            float& FloatMin, float& FloatMax );

        // Converts the Houdini Float height values to Unreal uint16
        static bool ConvertHeightfieldDataToLandscapeData(
            const TArray< float >& HeightfieldFloatValues,
            const HAPI_VolumeInfo& HeightfieldVolumeInfo,
            const int32& FinalXSize, const int32& FinalYSize,
            float FloatMin, float FloatMax,
            TArray< uint16 >& IntHeightData,
            FTransform& LandscapeTransform,
            const bool& NoResize = false);

        // Converts the Houdini float layer values to Unreal uint8
        static bool ConvertHeightfieldLayerToLandscapeLayer(
            const TArray< float >& FloatLayerData,
            const int32& LayerXSize, const int32& LayerYSize,
            const float& LayerMin, const float& LayerMax,
            const int32& LandscapeXSize, const int32& LandscapeYSize,
            TArray< uint8 >& LayerData, const bool& NoResize = false );

        // Retrieves size, number of sections, and quads per section from houdin attributes
        static bool GetLandscapeSizeAttributes(
            const FHoudiniGeoPartObject* CurrentHeightfield,
            const int32& SizeX, const int32& SizeY,
            int32& NewSizeX,
            int32& NewSizeY,
            int32& NumberOfSectionsPerComponent,
            int32& NumberOfQuadsPerSection);

        // Calculates the closest "unreal friendly" size given a heighfield volume's size
        static bool CalcLandscapeSizeFromHeightfieldSize(
            const int32& SizeX, const int32& SizeY,
            int32& NewSizeX,
            int32& NewSizeY,
            int32& NumberOfSectionsPerComponent,
            int32& NumberOfQuadsPerSection );

        // Resizes the HeightData so that it fits to UE4's size requirements.
        static bool ResizeHeightDataForLandscape(
            TArray<uint16>& HeightData,
            const int32& SizeX, const int32& SizeY,
            const int32& NewSizeX, const int32& NewSizeY,
            FVector& LandscapeResizeFactor,
            FVector& LandscapePositionOffset );

        // Resizes LayerData so that it fits the Landscape size
        static bool ResizeLayerDataForLandscape(
            TArray< uint8 >& LayerData,
            const int32& SizeX, const int32& SizeY,
            const int32& NewSizeX, const int32& NewSizeY );

        // Checks if a layer's value should be converted in the [0 1] range
        static bool IsUnitLandscapeLayer(
            const FHoudiniGeoPartObject& LayerGeoPartObject );

        //--------------------------------------------------------------------------------------------------
        // Unreal to Houdini - HEIGHTFIELDS
        //--------------------------------------------------------------------------------------------------

#if WITH_EDITOR
        // Creates a heightfield from a Landscape
        static bool CreateHeightfieldFromLandscape(
			ALandscapeProxy* LandscapeProxy, HAPI_NodeId& CreatedHeightfieldNodeId );

        // Creates multiple heightfield from an array of Landscape Components
        static bool CreateHeightfieldFromLandscapeComponentArray(
            ALandscapeProxy* LandscapeProxy,
            TSet< ULandscapeComponent * >& LandscapeComponentArray,
            HAPI_NodeId& CreatedHeightfieldNodeId );

        // Creates a Heightfield from a Landscape Component
        static bool CreateHeightfieldFromLandscapeComponent(
            ULandscapeComponent * LandscapeComponent,
            const int32& ComponentIndex,
            HAPI_NodeId& HeightFieldId,
            HAPI_NodeId& MergeId,
            int32& MergeInputIndex );

        // Initialise the Heightfield Mask with default values
        static bool InitDefaultHeightfieldMask(
            const HAPI_VolumeInfo& HeightVolumeInfo,
            const HAPI_NodeId& MaskVolumeNodeId );

        // Extracts the uint16 values of a given landscape
        static bool GetLandscapeData(
			ALandscape* Landscape,
            TArray<uint16>& HeightData,
            int32& XSize, int32& YSize,
            FVector& Min, FVector& Max );

        static bool GetLandscapeData(
            ALandscapeProxy* LandscapeProxy,
            TArray<uint16>& HeightData,
            int32& XSize, int32& YSize,
            FVector& Min, FVector& Max );

        static bool GetLandscapeData(
            ULandscapeInfo* LandscapeInfo,
            const int32& MinX, const int32& MinY,
            const int32& MaxX, const int32& MaxY,
            TArray<uint16>& HeightData,
            int32& XSize, int32& YSize );

        // Extracts the uint8 values of a given landscape
        static bool GetLandscapeLayerData(
            ULandscapeInfo* LandscapeInfo, const int32& LayerIndex,
            TArray<uint8>& LayerData, FLinearColor& LayerUsageDebugColor,
            FString& LayerName );

        static bool GetLandscapeLayerData(
            ULandscapeInfo* LandscapeInfo,
            const int32& LayerIndex,
            const int32& MinX, const int32& MinY,
            const int32& MaxX, const int32& MaxY,
            TArray<uint8>& LayerData,
            FLinearColor& LayerUsageDebugColor,
            FString& LayerName );
#endif
        // Converts Unreal uint16 values to Houdini Float
        static bool ConvertLandscapeDataToHeightfieldData(
            const TArray<uint16>& IntHeightData,
            const int32& XSize, const int32& YSize,
            FVector Min, FVector Max,
            const FTransform& LandscapeTransform,
            TArray<float>& HeightfieldFloatValues,
            HAPI_VolumeInfo& HeightfieldVolumeInfo,
            FVector& CenterOffset);

        // Converts Unreal uint8 values to Houdini Float
        static bool ConvertLandscapeLayerDataToHeightfieldData(
            const TArray<uint8>& IntHeightData,
            const int32& XSize, const int32& YSize,
            const FLinearColor& LayerUsageDebugColor,
            TArray<float>& LayerFloatValues,
            HAPI_VolumeInfo& LayerVolumeInfo);

        // Set the volume float value for a heightfield
        static bool SetHeighfieldData(
            const HAPI_NodeId& AssetId,
            const HAPI_PartId& PartId,
            TArray< float >& FloatValues,
            const HAPI_VolumeInfo& VolumeInfo,
            const FString& HeightfieldName );

        // Creates an unlocked heightfield input node
        static bool CreateHeightfieldInputNode(
            const HAPI_NodeId& ParentNodeId, const FString& NodeName,
            const int32& XSize, const int32& YSize,
            HAPI_NodeId& HeightfieldNodeId, HAPI_NodeId& HeightNodeId,
            HAPI_NodeId& MaskNodeId, HAPI_NodeId& MergeNodeId );

        // Landscape nodes clean up
        static bool DestroyLandscapeAssetNode(
            HAPI_NodeId& ConnectedAssetId, TArray<HAPI_NodeId>& CreatedInputAssetIds );

        // Returns an array containing the names of the non weightblended layers
        static bool GetNonWeightBlendedLayerNames( 
            const FHoudiniGeoPartObject& HeightfieldGeoPartObject, TArray<FString>& NonWeightBlendedLayerNames );

        static void GetLandscapeActorBounds(
            ALandscape* Landscape, FVector& Origin, FVector& Extents );

        static void GetLandscapeProxyBounds(
            ALandscapeProxy* LandscapeProxy, FVector& Origin, FVector& Extents );

        static bool AddLandscapeMaterialAttributesToVolume(
            const HAPI_NodeId& VolumeNodeId, const HAPI_PartId& PartId,
            UMaterialInterface* LandscapeMaterial, UMaterialInterface* LandscapeHoleMaterial );

        //--------------------------------------------------------------------------------------------------
        // Input Landscape caching
        //--------------------------------------------------------------------------------------------------
        static bool BackupLandscapeToFile(
            const FString& BaseName, ALandscapeProxy* Landscape );

        static bool RestoreLandscapeFromFile(
            ALandscapeProxy* LandscapeProxy );

        static bool ImportLandscapeData(
            ULandscapeInfo* LandscapeInfo, const FString& Filename, const FString& LayerName, ULandscapeLayerInfoObject* LayerInfoObject = nullptr);

        //--------------------------------------------------------------------------------------------------
        // Unreal to Houdini - MESH / POINTS
        //--------------------------------------------------------------------------------------------------
#if WITH_EDITOR
        // Extract data from the landscape
        static bool ExtractLandscapeData(
            ALandscapeProxy * LandscapeProxy, TSet< ULandscapeComponent * >& SelectedComponents,
            const bool& bExportLighting, const bool& bExportTileUVs, const bool& bExportNormalizedUVs,
            TArray<FVector>& LandscapePositionArray,
            TArray<FVector>& LandscapeNormalArray,
            TArray<FVector>& LandscapeUVArray, 
            TArray<FIntPoint>& LandscapeComponentVertexIndicesArray, 
            TArray<const char *>& LandscapeComponentNameArray,
            TArray<FLinearColor>& LandscapeLightmapValues );
#endif

        // Add the Position attribute extracted from a landscape
        static bool AddLandscapePositionAttribute(
            const HAPI_NodeId& NodeId, const TArray< FVector >& LandscapePositionArray );

        // Add the Normal attribute extracted from a landscape
        static bool AddLandscapeNormalAttribute(
            const HAPI_NodeId& NodeId, const TArray< FVector >& LandscapeNormalArray );

        // Add the UV attribute extracted from a landscape
        static bool AddLandscapeUVAttribute(
            const HAPI_NodeId& NodeId, const TArray< FVector >& LandscapeUVArray );

        // Add the Component Vertex Index attribute extracted from a landscape
        static bool AddLandscapeComponentVertexIndicesAttribute(
            const HAPI_NodeId& NodeId, const TArray<FIntPoint>& LandscapeComponentVertexIndicesArray );

        // Add the Component Name attribute extracted from a landscape
        static bool AddLandscapeComponentNameAttribute(
            const HAPI_NodeId& NodeId, const TArray<const char *>& LandscapeComponentNameArray );

        // Add the lightmap color attribute extracted from a landscape
        static bool AddLandscapeLightmapColorAttribute(
            const HAPI_NodeId& NodeId, const TArray<FLinearColor>& LandscapeLightmapValues );

        // Creates and add the vertex indices and face materials attribute from a landscape
        static bool AddLandscapeMeshIndicesAndMaterialsAttribute(
            const HAPI_NodeId& NodeId, const bool& bExportMaterials,
            const int32& ComponentSizeQuads, const int32& QuadCount,
            ALandscapeProxy * LandscapeProxy,
            const TSet< ULandscapeComponent * >& SelectedComponents );

        // Add the tile index primitive attribute
        static bool AddLandscapeTileAttribute(
            const HAPI_NodeId& NodeId, const HAPI_PartId& PartId, const int32& TileIdx );

        // Add the landscape component extent attribute
        static bool AddLandscapeComponentExtentAttributes(
            const HAPI_NodeId& NodeId, const HAPI_PartId& PartId,
            const int32& MinX, const int32& MaxX,
            const int32& MinY, const int32& MaxY );

        // Add the landscape component extent attribute
        static bool GetLandscapeComponentExtentAttributes(
            const FHoudiniGeoPartObject& HoudiniGeoPartObject,
            int32& MinX, int32& MaxX,
            int32& MinY, int32& MaxY );

#if WITH_EDITOR
        // Add the global (detail) material and hole material attribute from a landscape
        static bool AddLandscapeGlobalMaterialAttribute(
            const HAPI_NodeId& NodeId, ALandscapeProxy * LandscapeProxy );
#endif

        // Helper functions to extract color from a texture
        static FColor PickVertexColorFromTextureMip(
            const uint8 * MipBytes, FVector2D & UVCoord, int32 MipWidth, int32 MipHeight);

        /*
        // Duplicate a given Landscape. This will create a new package for it. This will also create necessary
        // materials, textures, landscape layers and their corresponding packages.
        static ALandscape * DuplicateLandscapeAndCreatePackage(
        const ALandscape * Landscape, UHoudiniAssetComponent * Component,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode );
        */
};
