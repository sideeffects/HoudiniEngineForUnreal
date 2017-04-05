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

struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeUtils
{
    public:

        // Resizes the HeightData so that it fits to UE4's size requirements.
        static bool ResizeHeightDataForLandscape(
            TArray< uint16 >& HeightData,
            int32& SizeX, int32& SizeY,
            int32& NumberOfSectionsPerComponent,
            int32& NumberOfQuadsPerSection,
            FVector& LandscapeResizeFactor );

        // Resizes LayerData so that it fits the Landscape size
        static bool ResizeLayerDataForLandscape(
            TArray< uint8 >& LayerData,
            const int32& SizeX, const int32& SizeY,
            const int32& NewSizeX, const int32& NewSizeY );

        // Returns Heightfield contained in the GeoPartObject array
        static void GetHeightfieldsInArray(
            const TArray< FHoudiniGeoPartObject >& InArray,
            TArray< const FHoudiniGeoPartObject* >& OutHeightfields );

        // Returns the layers corresponding to a height field contained in the GeoPartObject array
        static void GetHeightfieldsLayersInArray(
            const TArray< FHoudiniGeoPartObject >& InArray,
            const FHoudiniGeoPartObject& Heightfield,
            TArray< const FHoudiniGeoPartObject* >& FoundLayers );

        // Returns the global ZMin/ZMax value of all heightfield contained in the array
        static void CalcHeightfieldsArrayGlobalZMinZMax(
            const TArray< const FHoudiniGeoPartObject* >& InHeightfieldArray,
            float& fGlobalMin, float& fGlobalMax );

        // Extract the float values of a given heightfield
        static bool ExtractHeightfieldData(
            const FHoudiniGeoPartObject& Heightfield,
            TArray< float >& FloatValues,
            HAPI_VolumeInfo& VolumeInfo,
            float& FloatMin, float& FloatMax );

        // Converts the Houdini Float height values to Unreal uint16
        static bool ConvertHeightfieldDataToLandscapeData(
            const TArray< float >& HeightfieldFloatValues,
            const HAPI_VolumeInfo& HeightfieldVolumeInfo,
            const float& FloatMin, const float& FloatMax,
            TArray< uint16 >& IntHeightData,
            FTransform& LandscapeTransform,
            int32& FinalXSize, int32& FinalYSize,
            int32& NumSectionPerLandscapeComponent,
            int32& NumQuadsPerLandscapeSection );

        static bool ConvertHeightfieldLayerToLandscapeLayer(
            const TArray< float >& FloatLayerData,
            const int32& LayerXSize, const int32& LayerYSize,
            const float& LayerMin, const float& LayerMax,
            const int32& LandscapeXSize, const int32& LandscapeYSize,
            TArray< uint8 >& LayerData );

        /*
        // Duplicate a given Landscape. This will create a new package for it. This will also create necessary
        // materials, textures, landscape layers and their corresponding packages.
        static ALandscape * DuplicateLandscapeAndCreatePackage(
        const ALandscape * Landscape, UHoudiniAssetComponent * Component,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode );
        */

};
