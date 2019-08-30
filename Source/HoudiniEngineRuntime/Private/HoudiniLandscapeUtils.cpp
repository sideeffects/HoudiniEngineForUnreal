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
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniLandscapeUtils.h"

#include "HoudiniApi.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"
#include "HoudiniCookHandler.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"

#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeStreamingProxy.h"
#include "LightMap.h"
#include "Engine/MapBuildDataRegistry.h"

#if WITH_EDITOR
    #include "FileHelpers.h"
    #include "EngineUtils.h"
    #include "LandscapeEditorModule.h"
    #include "LandscapeFileFormatInterface.h"
#endif

void
FHoudiniLandscapeUtils::GetHeightfieldsInArray(
    const TArray< FHoudiniGeoPartObject >& InArray,
    TArray< const FHoudiniGeoPartObject* >& FoundHeightfields )
{
    FoundHeightfields.Empty();

    // First, we need to extract proper height data from FoundVolumes
    for ( TArray< FHoudiniGeoPartObject >::TConstIterator Iter( InArray ); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = *Iter;
        if ( !HoudiniGeoPartObject.IsVolume() )
            continue;

        // Retrieve node id from geo part.
        //HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId( AssetId );
        HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId();
        if ( NodeId == -1 )
            continue;

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo CurrentVolumeInfo;
        FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            NodeId, HoudiniGeoPartObject.PartId,
            &CurrentVolumeInfo ) )
            continue;

        // We're only interested in heightfields
        FString CurrentVolumeName;
        FHoudiniEngineString( CurrentVolumeInfo.nameSH ).ToFString( CurrentVolumeName );
        if ( !CurrentVolumeName.Contains( "height" ) )
            continue;

        // We're only handling single values for now
        if ( CurrentVolumeInfo.tupleSize != 1 )
            continue;

        // Terrains always have a ZSize of 1.
        if ( CurrentVolumeInfo.zLength != 1 )
            continue;

        // Values should be float
        if ( CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
            continue;

        FoundHeightfields.Add( &HoudiniGeoPartObject );
    }
}

void
FHoudiniLandscapeUtils::GetHeightfieldsLayersInArray(
    const TArray< FHoudiniGeoPartObject >& InArray,
    const FHoudiniGeoPartObject& Heightfield,
    TArray< const FHoudiniGeoPartObject* >& FoundLayers )
{
    FoundLayers.Empty();

    // We need the parent heightfield's node ID and tile attribute
    HAPI_NodeId HeightFieldNodeId = Heightfield.HapiGeoGetNodeId();

    // We need the tile attribute if the height has it
    bool bParentHeightfieldHasTile = false;
    int32 HeightFieldTile = -1;
    {
        HAPI_AttributeInfo AttribInfoTile;
        FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);
        TArray< int32 > TileValues;

        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
            Heightfield, "tile",
            AttribInfoTile, TileValues );

        if ( AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM && TileValues.Num() > 0 )
        {
            HeightFieldTile = TileValues[ 0 ];
            bParentHeightfieldHasTile = true;
        }
    }

    // Look for all the layers/masks corresponding to the current heightfield
    for ( TArray< FHoudiniGeoPartObject >::TConstIterator IterLayers( InArray ); IterLayers; ++IterLayers )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = *IterLayers;
        if ( !HoudiniGeoPartObject.IsVolume() )
            continue;

        // Retrieve node id from geo part.
        HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId();
        if ( NodeId == -1 )
            continue;

        // It needs to be from the same node as the parent heightfield...
        if ( NodeId != HeightFieldNodeId )
            continue;

        // If the parent had a tile info, retrieve the tile attribute for the current layer
        if ( bParentHeightfieldHasTile )
        {
            int32 CurrentTile = -1;

            HAPI_AttributeInfo AttribInfoTile;
            FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);
            TArray<int32> TileValues;
            FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                HoudiniGeoPartObject, "tile",
                AttribInfoTile, TileValues );

            if ( AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM && TileValues.Num() > 0 )
            {
                CurrentTile = TileValues[ 0 ];
            }

            // Does this layer come from the same tile as the height?
            if ( ( CurrentTile != HeightFieldTile ) || ( CurrentTile == -1 ) )
                continue;
        }

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo CurrentVolumeInfo;
        FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            NodeId, HoudiniGeoPartObject.PartId,
            &CurrentVolumeInfo ) )
            continue;

        // We're interesting in anything but height data
        FString CurrentVolumeName;
        FHoudiniEngineString( CurrentVolumeInfo.nameSH ).ToFString( CurrentVolumeName );
        if ( CurrentVolumeName.Contains( "height" ) )
            continue;

        // We're only handling single values for now
        if ( CurrentVolumeInfo.tupleSize != 1 )
            continue;

        // Terrains always have a ZSize of 1.
        if ( CurrentVolumeInfo.zLength != 1 )
            continue;

        // Values should be float
        if ( CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
            continue;

        FoundLayers.Add( &HoudiniGeoPartObject );
    }
}

void
FHoudiniLandscapeUtils::CalcHeightfieldsArrayGlobalZMinZMax(
    const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
    TMap<FString, float>& GlobalMinimums,
    TMap<FString, float>& GlobalMaximums )
{
    GlobalMinimums.Empty();
    GlobalMaximums.Empty();

    for (TArray< FHoudiniGeoPartObject >::TConstIterator CurrentHeightfield(InHeightfieldArray); CurrentHeightfield; ++CurrentHeightfield)
    {
        // Get the current Heightfield GeoPartObject
        if (!CurrentHeightfield)
            continue;

        if (!CurrentHeightfield->IsVolume())
            continue;

        // Retrieve node id from geo part.
        HAPI_NodeId NodeId = CurrentHeightfield->HapiGeoGetNodeId();
        if (NodeId == -1)
            continue;

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo CurrentVolumeInfo;
        FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
        if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            NodeId, CurrentHeightfield->PartId,
            &CurrentVolumeInfo))
            continue;

        // Unreal's Z values are Y in Houdini
        float ymin, ymax;
        if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds(FHoudiniEngine::Get().GetSession(),
            NodeId, CurrentHeightfield->PartId,
            nullptr, &ymin, nullptr,
            nullptr, &ymax, nullptr,
            nullptr, nullptr, nullptr))
            continue;

        // Retrieve the volume name.
        FString VolumeName;
        FHoudiniEngineString HoudiniEngineStringPartName(CurrentVolumeInfo.nameSH);
        HoudiniEngineStringPartName.ToFString(VolumeName);

        // Read the global min value for this volume
        if ( !GlobalMinimums.Contains(VolumeName) )
        {
            GlobalMinimums.Add(VolumeName, ymin);
        }
        else
        {
            // Update the min if necessary
            if ( ymin < GlobalMinimums[VolumeName] )
                GlobalMinimums[VolumeName] = ymin;
        }

        // Read the global max value for this volume
        float fCurrentVolumeGlobalMax = -MAX_FLT;
        if (!GlobalMaximums.Contains(VolumeName))
        {
            GlobalMaximums.Add(VolumeName, ymax);
        }
        else
        {
            // Update the max if necessary
            if (ymax > GlobalMaximums[VolumeName])
                GlobalMaximums[VolumeName] = ymax;
        }
    }
}

void
FHoudiniLandscapeUtils::CalcHeightfieldsArrayGlobalZMinZMax(
    const TArray< const FHoudiniGeoPartObject* > & InHeightfieldArray,
    float& fGlobalMin, float& fGlobalMax )
{
    // Initialize the global values
    fGlobalMin = MAX_FLT;
    fGlobalMax = -MAX_FLT;

    for ( TArray< const FHoudiniGeoPartObject* >::TConstIterator IterHeighfields( InHeightfieldArray ); IterHeighfields; ++IterHeighfields )
    {
        // Get the current Heightfield GeoPartObject
        const FHoudiniGeoPartObject* CurrentHeightfield = *IterHeighfields;
        if ( !CurrentHeightfield )
            continue;

        if ( !CurrentHeightfield->IsVolume() )
            continue;

        // Retrieve node id from geo part.
        HAPI_NodeId NodeId = CurrentHeightfield->HapiGeoGetNodeId();
        if ( NodeId == -1 )
            continue;

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo CurrentVolumeInfo;
        FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            NodeId, CurrentHeightfield->PartId,
            &CurrentVolumeInfo ) )
            continue;

        // Unreal's Z values are Y in Houdini
        float ymin, ymax;
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds( FHoudiniEngine::Get().GetSession(),
            NodeId, CurrentHeightfield->PartId,
            nullptr, &ymin, nullptr,
            nullptr, &ymax, nullptr,
            nullptr, nullptr, nullptr ) )
            continue;

        if ( ymin < fGlobalMin )
            fGlobalMin = ymin;

        if ( ymax > fGlobalMax )
            fGlobalMax = ymax;
    }

    // Set Min/Max to zero if we couldn't set the values properly
    if ( fGlobalMin > fGlobalMax )
    {
        fGlobalMin = 0.0f;
        fGlobalMax = 0.0f;
    }
}

bool FHoudiniLandscapeUtils::GetHeightfieldData(
    const FHoudiniGeoPartObject& Heightfield,
    TArray<float>& FloatValues,
    HAPI_VolumeInfo& VolumeInfo,
    float& FloatMin, float& FloatMax )
{
    FloatValues.Empty();
    FloatMin = 0.0f;
    FloatMax = 0.0f;

    if ( !Heightfield.IsVolume() )
        return false;

    // Retrieve node id from geo part.
    HAPI_NodeId NodeId = Heightfield.HapiGeoGetNodeId();
    if ( NodeId == -1 )
        return false;

    // Retrieve the VolumeInfo
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetVolumeInfo(
        FHoudiniEngine::Get().GetSession(),
        NodeId, Heightfield.PartId,
        &VolumeInfo ), false );

    // We're only handling single values for now
    if ( VolumeInfo.tupleSize != 1 )
        return false;

    // Terrains always have a ZSize of 1.
    if ( VolumeInfo.zLength != 1 )
        return false;

    // Values must be float
    if ( VolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
        return false;

    if ( ( VolumeInfo.xLength < 2 ) || ( VolumeInfo.yLength < 2 ) )
        return false;

    int32 SizeInPoints = VolumeInfo.xLength *  VolumeInfo.yLength;
    int32 TotalSize = SizeInPoints * VolumeInfo.tupleSize;

    //--------------------------------------------------------------------------------------------------
    // 1. Reading and converting the Height values from HAPI
    //--------------------------------------------------------------------------------------------------    
    FloatValues.SetNumUninitialized( TotalSize );

    // Get all the heightfield data
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetHeightFieldData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, Heightfield.PartId,
        FloatValues.GetData(),
        0, SizeInPoints ), false );

    // We will need the min and max value for the conversion to uint16
    FloatMin = FloatValues[0];
    FloatMax = FloatMin;
    for ( int32 n = 0; n < FloatValues.Num(); n++ )
    {
        if ( FloatValues[ n ] > FloatMax )
            FloatMax = FloatValues[ n ];
        else if ( FloatValues[ n ] < FloatMin )
            FloatMin = FloatValues[ n ];
    }

    return true;
}

bool
FHoudiniLandscapeUtils::ConvertHeightfieldDataToLandscapeData(
    const TArray< float >& HeightfieldFloatValues,
    const HAPI_VolumeInfo& HeightfieldVolumeInfo,
    const int32& FinalXSize, const int32& FinalYSize,
    float FloatMin, float FloatMax,
    TArray< uint16 >& IntHeightData,
    FTransform& LandscapeTransform,
    const bool& NoResize )
{
    IntHeightData.Empty();
    LandscapeTransform.SetIdentity();
    // HF sizes needs an X/Y swap
    int32 HoudiniXSize = HeightfieldVolumeInfo.yLength;
    int32 HoudiniYSize = HeightfieldVolumeInfo.xLength;
    int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
    if ( ( HoudiniXSize < 2 ) || ( HoudiniYSize < 2 ) )
        return false;

    // Test for potential special cases...
    // Just print a warning for now
    if ( HeightfieldVolumeInfo.minX != 0 )
        HOUDINI_LOG_WARNING( TEXT( "Converting Landscape: heightfield's min X is not zero." ) );

    if ( HeightfieldVolumeInfo.minY != 0 )
        HOUDINI_LOG_WARNING( TEXT( "Converting Landscape: heightfield's min Y is not zero." ) );

    //--------------------------------------------------------------------------------------------------
    // 1. Convert values to uint16 using doubles to get the maximum precision during the conversion
    //--------------------------------------------------------------------------------------------------

    // Extract the HF's current transform
    FTransform CurrentVolumeTransform;
    {
        HAPI_Transform HapiTransform = HeightfieldVolumeInfo.transform;
        FQuat ObjectRotation(
            HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[1],
            HapiTransform.rotationQuaternion[2], -HapiTransform.rotationQuaternion[3]);
        Swap(ObjectRotation.Y, ObjectRotation.Z);

        FVector ObjectTranslation(HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2]);
        ObjectTranslation *= 100.0f;
        Swap(ObjectTranslation[2], ObjectTranslation[1]);

        FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2]);

        CurrentVolumeTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
    }

    // The ZRange in Houdini (in m)
    double MeterZRange = (double) ( FloatMax - FloatMin );

    // The corresponding unreal digit range (as unreal uses uint16, max is 65535)
    // We may want to not use the full range in order to be able to sculpt the landscape past the min/max values after.
    const double dUINT16_MAX = (double)UINT16_MAX;
    double DigitZRange = 49152.0;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseFullResolution )
        DigitZRange = dUINT16_MAX - 1.0;

    // If we  are not using the full range, we need to center the digit values so the terrain can be edited up and down
    double DigitCenterOffset = FMath::FloorToDouble( ( dUINT16_MAX - DigitZRange ) / 2.0 );

    // The factor used to convert from Houdini's ZRange to the desired digit range
    double ZSpacing = ( MeterZRange != 0.0 ) ? ( DigitZRange / MeterZRange ) : 0.0;

    // Changes these values if the user wants to loose a lot of precision
    // just to keep the same transform as the landscape input
    bool bUseDefaultUE4Scaling = false;
    if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
        bUseDefaultUE4Scaling = HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling;

    if ( bUseDefaultUE4Scaling )
    {
        //Check that our values are compatible with UE4's default scale values
        if (FloatMin < -256.0f || FloatMin > 256.0f || FloatMax < -256.0f || FloatMax > 256.0f)
        {
            // Warn the user that the landscape conversion will have issues 
            // invite him to change that setting
            HOUDINI_LOG_WARNING(
                TEXT("The heightfield's min and max height values are too large for being used with the \"Use Default UE4 scaling\" option.\n \
                      The generated Heightfield will likely be incorrectly converted to landscape unless you disable that option in the project settings and recook the asset."));
        }

        DigitZRange = dUINT16_MAX - 1.0;
        DigitCenterOffset = 0;

        // Default unreal landscape scaling is -256m:256m at Scale = 100
        // We need to apply the scale back to
        FloatMin = -256.0f * CurrentVolumeTransform.GetScale3D().Z * 2.0f;
        FloatMax = 256.0f * CurrentVolumeTransform.GetScale3D().Z * 2.0f;
        MeterZRange = (double)(FloatMax - FloatMin);

        ZSpacing = ((double)DigitZRange) / MeterZRange;
    }

    // Converting the data from Houdini to Unreal
    // For correct orientation in unreal, the point matrix has to be transposed.
    IntHeightData.SetNumUninitialized( SizeInPoints );

    int32 nUnreal = 0;
    for (int32 nY = 0; nY < HoudiniYSize; nY++)
    {
        for (int32 nX = 0; nX < HoudiniXSize; nX++)
        {
            // Copying values X then Y in Unreal but reading them Y then X in Houdini due to swapped X/Y
            int32 nHoudini = nY + nX * HoudiniYSize;

            // Get the double values in [0 - ZRange]
            double DoubleValue = (double)HeightfieldFloatValues[nHoudini] - (double)FloatMin;

            // Then convert it to [0 - DesiredRange] and center it 
            DoubleValue = DoubleValue * ZSpacing + DigitCenterOffset;
            IntHeightData[nUnreal++] = FMath::RoundToInt(DoubleValue);
        }
    }

    //--------------------------------------------------------------------------------------------------
    // 2. Resample / Pad the int data so that if fits unreal size requirements
    //--------------------------------------------------------------------------------------------------

    // UE has specific size requirements for landscape,
    // so we might need to pad/resample the heightfield data
    FVector LandscapeResizeFactor = FVector::OneVector;
    FVector LandscapePositionOffsetInPixels = FVector::ZeroVector;
    if (!NoResize)
    {
        // Try to resize the data
        if ( !FHoudiniLandscapeUtils::ResizeHeightDataForLandscape(
            IntHeightData,
            HoudiniXSize, HoudiniYSize, FinalXSize, FinalYSize,
            LandscapeResizeFactor, LandscapePositionOffsetInPixels))
            return false;
    }

    //--------------------------------------------------------------------------------------------------
    // 3. Calculating the proper transform for the landscape to be sized and positionned properly
    //--------------------------------------------------------------------------------------------------

    // Scale:
    // Calculating the equivalent scale to match Houdini's Terrain Size in Unreal
    FVector LandscapeScale;

    // Unreal has a X/Y resolution of 1m per point while Houdini is dependant on the heighfield's grid spacing
    LandscapeScale.X = CurrentVolumeTransform.GetScale3D().X * 2.0f;
    LandscapeScale.Y = CurrentVolumeTransform.GetScale3D().Y * 2.0f;

    // Calculating the Z Scale so that the Z values in Unreal are the same as in Houdini
    // Unreal has a default Z range is 512m for a scale of a 100%
    LandscapeScale.Z = (float)( (double)( dUINT16_MAX / DigitZRange ) * MeterZRange / 512.0 );
    if ( bUseDefaultUE4Scaling )
        LandscapeScale.Z = CurrentVolumeTransform.GetScale3D().Z * 2.0f;
    LandscapeScale *= 100.f;

    // If the data was resized and not expanded, we need to modify the landscape's scale
    LandscapeScale *= LandscapeResizeFactor;

    // We'll use the position from Houdini, but we will need to offset the Z Position to center the 
    // values properly as the data has been offset by the conversion to uint16
    FVector LandscapePosition = CurrentVolumeTransform.GetLocation();
    //LandscapePosition.Z = 0.0f;

    // We need to calculate the position offset so that Houdini and Unreal have the same Zero position
    // In Unreal, zero has a height value of 32768.
    // These values are then divided by 128 internally, and then multiplied by the Landscape's Z scale
    // ( DIGIT - 32768 ) / 128 * ZScale = ZOffset

    // We need the Digit (Unreal) value of Houdini's zero for the scale calculation
    // ( float and int32 are used for this because 0 might be out of the landscape Z range!
    // when using the full range, this would cause an overflow for a uint16!! )
    float HoudiniZeroValueInDigit = (float)FMath::RoundToInt((0.0 - (double)FloatMin) * ZSpacing + DigitCenterOffset);
    float ZOffset = -( HoudiniZeroValueInDigit - 32768.0f ) / 128.0f * LandscapeScale.Z;

    LandscapePosition.Z += ZOffset;

    // If we have padded the data when resizing the landscape, we need to offset the position because of
    // the added values on the topLeft Corner of the Landscape
    if ( LandscapePositionOffsetInPixels != FVector::ZeroVector )
    {
        FVector LandscapeOffset = LandscapePositionOffsetInPixels * LandscapeScale;
        LandscapeOffset.Z = 0.0f;

        LandscapePosition += LandscapeOffset;
    }

    // Landscape rotation
    //FRotator LandscapeRotation( 0.0, -90.0, 0.0 );
    //Landscape->SetActorRelativeRotation( LandscapeRotation );

    // We can now set the Landscape position
    LandscapeTransform.SetLocation( LandscapePosition );
    LandscapeTransform.SetScale3D( LandscapeScale );

    return true;
}

bool FHoudiniLandscapeUtils::ConvertHeightfieldLayerToLandscapeLayer(
    const TArray<float>& FloatLayerData,
    const int32& HoudiniXSize, const int32& HoudiniYSize,
    const float& LayerMin, const float& LayerMax,
    const int32& LandscapeXSize, const int32& LandscapeYSize,
    TArray<uint8>& LayerData, const bool& NoResize )
{
    // Convert the float data to uint8
    LayerData.SetNumUninitialized( HoudiniXSize * HoudiniYSize );

    // Calculating the factor used to convert from Houdini's ZRange to [0 255]
    double LayerZRange = ( LayerMax - LayerMin );
    double LayerZSpacing = ( LayerZRange != 0.0 ) ? ( 255.0 / (double)( LayerZRange ) ) : 0.0;

    int32 nUnrealIndex = 0;
    for ( int32 nY = 0; nY < HoudiniYSize; nY++ )
    {
        for ( int32 nX = 0; nX < HoudiniXSize; nX++ )
        {
            // Copying values X then Y in Unreal but reading them Y then X in Houdini due to swapped X/Y
            int32 nHoudini = nY + nX * HoudiniYSize;

            // Get the double values in [0 - ZRange]
            double DoubleValue = (double)FMath::Clamp(FloatLayerData[ nHoudini ], LayerMin, LayerMax) - (double)LayerMin;

            // Then convert it to [0 - 255]
            DoubleValue *= LayerZSpacing;

            LayerData[ nUnrealIndex++ ] = FMath::RoundToInt( DoubleValue );
        }
    }

    // Finally, resize the data to fit with the new landscape size if needed
    if ( NoResize )
        return true;

    return FHoudiniLandscapeUtils::ResizeLayerDataForLandscape(
        LayerData, HoudiniXSize, HoudiniYSize,
        LandscapeXSize, LandscapeYSize );
}

bool
FHoudiniLandscapeUtils::GetNonWeightBlendedLayerNames( const FHoudiniGeoPartObject& HeightfieldGeoPartObject, TArray<FString>& NonWeightBlendedLayerNames )
{
    // See if we can find the NonWeightBlendedLayer prim attribute on the heightfield
    HAPI_NodeId HeightfieldNodeId = HeightfieldGeoPartObject.HapiGeoGetNodeId();

    HAPI_PartInfo PartInfo;
    FHoudiniApi::PartInfo_Init(&PartInfo);
    if ( !HeightfieldGeoPartObject.HapiPartGetInfo( PartInfo ) )
        return false;

    HAPI_PartId PartId = HeightfieldGeoPartObject.GetPartId();

    // Get All attribute names for that part
    int32 nAttribCount = PartInfo.attributeCounts[ HAPI_ATTROWNER_PRIM ];

    TArray<HAPI_StringHandle> AttribNameSHArray;
    AttribNameSHArray.SetNum( nAttribCount );

    if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeNames(
        FHoudiniEngine::Get().GetSession(),
        HeightfieldNodeId, PartInfo.id, HAPI_ATTROWNER_PRIM,
        AttribNameSHArray.GetData(), nAttribCount ) )
        return false;

    // Looking for all the attributes that starts with unreal_landscape_layer_nonweightblended
    for ( int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx )
    {
        FString HapiString = TEXT("");
        FHoudiniEngineString HoudiniEngineString( AttribNameSHArray[ Idx ] );
        HoudiniEngineString.ToFString( HapiString );

        if ( !HapiString.StartsWith( "unreal_landscape_layer_nonweightblended", ESearchCase::IgnoreCase ) )
            continue;

        // Get the Attribute Info
        HAPI_AttributeInfo AttribInfo;
        FHoudiniApi::AttributeInfo_Init(&AttribInfo);
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeInfo(
            FHoudiniEngine::Get().GetSession(),
            HeightfieldNodeId, PartId, TCHAR_TO_UTF8( *HapiString ),
            HAPI_ATTROWNER_PRIM, &AttribInfo ), false );

        if ( AttribInfo.storage != HAPI_STORAGETYPE_STRING )
            break;

        // Initialize a string handle array
        TArray< HAPI_StringHandle > HapiSHArray;
        HapiSHArray.SetNumZeroed( AttribInfo.count * AttribInfo.tupleSize );

        // Get the string handle(s)
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeStringData(
            FHoudiniEngine::Get().GetSession(),
            HeightfieldNodeId, PartId, TCHAR_TO_UTF8( *HapiString ), &AttribInfo,
            HapiSHArray.GetData(), 0, AttribInfo.count ), false );

        // Convert them to FString
        for ( int32 IdxSH = 0; IdxSH < HapiSHArray.Num(); IdxSH++ )
        {
            FString CurrentString;
            FHoudiniEngineString HEngineString( HapiSHArray[ IdxSH ] );
            HEngineString.ToFString( CurrentString );

            TArray<FString> Tokens;
            CurrentString.ParseIntoArray( Tokens, TEXT(" "), true );

            for( int32 n = 0; n < Tokens.Num(); n++ )
                NonWeightBlendedLayerNames.Add( Tokens[ n ] );
        }

        // We found the attribute, exit
        break;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------------------------
// From LandscapeEditorUtils.h
//
//    Helpers function for FHoudiniEngineUtils::ResizeHeightDataForLandscape
//-------------------------------------------------------------------------------------------------------------------
template<typename T>
void ExpandData( T* OutData, const T* InData,
    int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
    int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY )
{
    const int32 OldWidth = OldMaxX - OldMinX + 1;
    const int32 OldHeight = OldMaxY - OldMinY + 1;
    const int32 NewWidth = NewMaxX - NewMinX + 1;
    const int32 NewHeight = NewMaxY - NewMinY + 1;
    const int32 OffsetX = NewMinX - OldMinX;
    const int32 OffsetY = NewMinY - OldMinY;

    for ( int32 Y = 0; Y < NewHeight; ++Y )
    {
        const int32 OldY = FMath::Clamp<int32>( Y + OffsetY, 0, OldHeight - 1 );

        // Pad anything to the left
        const T PadLeft = InData[ OldY * OldWidth + 0 ];
        for ( int32 X = 0; X < -OffsetX; ++X )
        {
            OutData[ Y * NewWidth + X ] = PadLeft;
        }

        // Copy one row of the old data
        {
            const int32 X = FMath::Max( 0, -OffsetX );
            const int32 OldX = FMath::Clamp<int32>( X + OffsetX, 0, OldWidth - 1 );
            FMemory::Memcpy( &OutData[ Y * NewWidth + X ], &InData[ OldY * OldWidth + OldX ], FMath::Min<int32>( OldWidth, NewWidth ) * sizeof( T ) );
        }

        const T PadRight = InData[ OldY * OldWidth + OldWidth - 1 ];
        for ( int32 X = -OffsetX + OldWidth; X < NewWidth; ++X )
        {
            OutData[ Y * NewWidth + X ] = PadRight;
        }
    }
}

template<typename T>
TArray<T> ExpandData(const TArray<T>& Data,
    int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
    int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY,
    int32* PadOffsetX = nullptr, int32* PadOffsetY = nullptr )
{
    const int32 NewWidth = NewMaxX - NewMinX + 1;
    const int32 NewHeight = NewMaxY - NewMinY + 1;

    TArray<T> Result;
    Result.Empty( NewWidth * NewHeight );
    Result.AddUninitialized( NewWidth * NewHeight );

    ExpandData( Result.GetData(), Data.GetData(),
        OldMinX, OldMinY, OldMaxX, OldMaxY,
        NewMinX, NewMinY, NewMaxX, NewMaxY );

    // Return the padding so we can offset the terrain position after
    if ( PadOffsetX )
        *PadOffsetX = NewMinX;

    if ( PadOffsetY )
        *PadOffsetY = NewMinY;

    return Result;
}

template<typename T>
TArray<T> ResampleData( const TArray<T>& Data, int32 OldWidth, int32 OldHeight, int32 NewWidth, int32 NewHeight )
{
    TArray<T> Result;
    Result.Empty( NewWidth * NewHeight );
    Result.AddUninitialized( NewWidth * NewHeight );

    const float XScale = (float)( OldWidth - 1 ) / ( NewWidth - 1 );
    const float YScale = (float)( OldHeight - 1 ) / ( NewHeight - 1 );
    for ( int32 Y = 0; Y < NewHeight; ++Y )
    {
        for ( int32 X = 0; X < NewWidth; ++X )
        {
            const float OldY = Y * YScale;
            const float OldX = X * XScale;
            const int32 X0 = FMath::FloorToInt( OldX );
            const int32 X1 = FMath::Min( FMath::FloorToInt( OldX ) + 1, OldWidth - 1 );
            const int32 Y0 = FMath::FloorToInt( OldY );
            const int32 Y1 = FMath::Min( FMath::FloorToInt( OldY ) + 1, OldHeight - 1 );
            const T& Original00 = Data[ Y0 * OldWidth + X0 ];
            const T& Original10 = Data[ Y0 * OldWidth + X1 ];
            const T& Original01 = Data[ Y1 * OldWidth + X0 ];
            const T& Original11 = Data[ Y1 * OldWidth + X1 ];
            Result[ Y * NewWidth + X ] = FMath::BiLerp( Original00, Original10, Original01, Original11, FMath::Fractional( OldX ), FMath::Fractional( OldY ) );
        }
    }

    return Result;
}

//-------------------------------------------------------------------------------------------------------------------
bool
FHoudiniLandscapeUtils::CalcLandscapeSizeFromHeightfieldSize(
    const int32& SizeX, const int32& SizeY,
    int32& NewSizeX, int32& NewSizeY,
    int32& NumberOfSectionsPerComponent,
    int32& NumberOfQuadsPerSection )
{
    if ( (SizeX < 2) || (SizeY < 2) )
        return false;

    NumberOfSectionsPerComponent = 1;
    NumberOfQuadsPerSection = 1;
    NewSizeX = -1;
    NewSizeY = -1;

    // Unreal's default sizes
    int32 SectionSizes[] = { 7, 15, 31, 63, 127, 255 };
    int32 NumSections[] = { 1, 2 };

    // Component count used to calculate the final size of the landscape
    int32 ComponentsCountX = 1;
    int32 ComponentsCountY = 1;

    // Lambda for clamping the number of component in X/Y
    auto ClampLandscapeSize = [&]()
    {
        // Max size is either whole components below 8192 verts, or 32 components
        ComponentsCountX = FMath::Clamp(ComponentsCountX, 1, FMath::Min(32, FMath::FloorToInt(8191 / (NumberOfSectionsPerComponent * NumberOfQuadsPerSection))));
        ComponentsCountY = FMath::Clamp(ComponentsCountY, 1, FMath::Min(32, FMath::FloorToInt(8191 / (NumberOfSectionsPerComponent * NumberOfQuadsPerSection))));
    };

    // Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
    bool bFoundMatch = false;
    for (int32 SectionSizesIdx = ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
    {
        for (int32 NumSectionsIdx = ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
        {
            int32 ss = SectionSizes[SectionSizesIdx];
            int32 ns = NumSections[NumSectionsIdx];

            if (((SizeX - 1) % (ss * ns)) == 0 && ((SizeX - 1) / (ss * ns)) <= 32 &&
                ((SizeY - 1) % (ss * ns)) == 0 && ((SizeY - 1) / (ss * ns)) <= 32)
            {
                bFoundMatch = true;
                NumberOfQuadsPerSection = ss;
                NumberOfSectionsPerComponent = ns;
                ComponentsCountX = (SizeX - 1) / (ss * ns);
                ComponentsCountY = (SizeY - 1) / (ss * ns);
                ClampLandscapeSize();
                break;
            }
        }
        if (bFoundMatch)
        {
            break;
        }
    }

    if (!bFoundMatch)
    {
        // if there was no exact match, try increasing the section size until we encompass the whole heightmap
        const int32 CurrentSectionSize = NumberOfQuadsPerSection;
        const int32 CurrentNumSections = NumberOfSectionsPerComponent;
        for (int32 SectionSizesIdx = 0; SectionSizesIdx < ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
        {
            if (SectionSizes[SectionSizesIdx] < CurrentSectionSize)
            {
                continue;
            }

            const int32 ComponentsX = FMath::DivideAndRoundUp((SizeX - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
            const int32 ComponentsY = FMath::DivideAndRoundUp((SizeY - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
            if (ComponentsX <= 32 && ComponentsY <= 32)
            {
                bFoundMatch = true;
                NumberOfQuadsPerSection = SectionSizes[SectionSizesIdx];
                ComponentsCountX = ComponentsX;
                ComponentsCountY = ComponentsY;
                ClampLandscapeSize();
                break;
            }
        }
    }

    if (!bFoundMatch)
    {
        // if the heightmap is very large, fall back to using the largest values we support
        const int32 MaxSectionSize = SectionSizes[ARRAY_COUNT(SectionSizes) - 1];
        const int32 MaxNumSubSections = NumSections[ARRAY_COUNT(NumSections) - 1];
        const int32 ComponentsX = FMath::DivideAndRoundUp((SizeX - 1), MaxSectionSize * MaxNumSubSections);
        const int32 ComponentsY = FMath::DivideAndRoundUp((SizeY - 1), MaxSectionSize * MaxNumSubSections);

        bFoundMatch = true;
        NumberOfQuadsPerSection = MaxSectionSize;
        NumberOfSectionsPerComponent = MaxNumSubSections;
        ComponentsCountX = ComponentsX;
        ComponentsCountY = ComponentsY;
        ClampLandscapeSize();
    }

    if (!bFoundMatch)
    {
        // Using default size just to not crash..
        NewSizeX = 512;
        NewSizeY = 512;
        NumberOfSectionsPerComponent = 1;
        NumberOfQuadsPerSection = 511;
        ComponentsCountX = 1;
        ComponentsCountY = 1;
    }
    else
    {
        // Calculating the desired size
        int32 QuadsPerComponent = NumberOfSectionsPerComponent * NumberOfQuadsPerSection;

        NewSizeX = ComponentsCountX * QuadsPerComponent + 1;
        NewSizeY = ComponentsCountY * QuadsPerComponent + 1;
    }

    return bFoundMatch;
}

//-------------------------------------------------------------------------------------------------------------------
bool
FHoudiniLandscapeUtils::ResizeHeightDataForLandscape(
    TArray<uint16>& HeightData,
    const int32& SizeX, const int32& SizeY,
    const int32& NewSizeX, const int32& NewSizeY,
    FVector& LandscapeResizeFactor,
    FVector& LandscapePositionOffset )
{
    LandscapeResizeFactor = FVector::OneVector;
    LandscapePositionOffset = FVector::ZeroVector;

    if ( HeightData.Num() <= 4 )
        return false;

    if ( ( SizeX < 2 ) || ( SizeY < 2 ) )
        return false;

    // No need to resize anything
    if ( SizeX == NewSizeX && SizeY == NewSizeY )
        return true;

    // Do we need to resize/expand the data to the new size?
    bool bForceResample = false;
    bool bResample = bForceResample ? true : ( ( NewSizeX <= SizeX ) && ( NewSizeY <= SizeY ) );

    TArray<uint16> NewData;
    if ( !bResample )
    {
        // Expanding the data by padding
        NewData.SetNumUninitialized( NewSizeX * NewSizeY );

        const int32 OffsetX = (int32)( NewSizeX - SizeX ) / 2;
        const int32 OffsetY = (int32)( NewSizeY - SizeY ) / 2;

        // Store the offset in pixel due to the padding
        int32 PadOffsetX = 0;
        int32 PadOffsetY = 0;

        // Expanding the Data
        NewData = ExpandData(
            HeightData, 0, 0, SizeX - 1, SizeY - 1,
            -OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1,
            &PadOffsetX, &PadOffsetY );

        // We will need to offset the landscape position due to the value added by the padding
        LandscapePositionOffset.X = (float)PadOffsetX;
        LandscapePositionOffset.Y = (float)PadOffsetY;

        // Notify the user that the data was padded
        HOUDINI_LOG_WARNING(
            TEXT("Landscape data was padded from ( %d x %d ) to ( %d x %d )."),
            SizeX, SizeY, NewSizeX, NewSizeY );
    }
    else
    {
        // Resampling the data
        NewData.SetNumUninitialized( NewSizeX * NewSizeY );
        NewData = ResampleData( HeightData, SizeX, SizeY, NewSizeX, NewSizeY );

        // The landscape has been resized, we'll need to take that into account when sizing it
        LandscapeResizeFactor.X = (float)SizeX / (float)NewSizeX;
        LandscapeResizeFactor.Y = (float)SizeY / (float)NewSizeY;
        LandscapeResizeFactor.Z = 1.0f;

        // Notify the user if the heightfield data was resized
        HOUDINI_LOG_WARNING(
            TEXT("Landscape data was resized from ( %d x %d ) to ( %d x %d )."),
            SizeX, SizeY, NewSizeX, NewSizeY );
    }

    // Replaces Old data with the new one
    HeightData = NewData;

    return true;
}

bool
FHoudiniLandscapeUtils::ResizeLayerDataForLandscape(
    TArray< uint8 >& LayerData,
    const int32& SizeX, const int32& SizeY,
    const int32& NewSizeX, const int32& NewSizeY )
{
    if ( ( NewSizeX == SizeX ) && ( NewSizeY == SizeY ) )
        return true; 

    bool bForceResample = false;
    bool bResample = bForceResample ? true : ( ( NewSizeX <= SizeX ) && ( NewSizeY <= SizeY ) );

    TArray<uint8> NewData;
    if (!bResample)
    {
        NewData.SetNumUninitialized( NewSizeX * NewSizeY );

        const int32 OffsetX = (int32)( NewSizeX - SizeX ) / 2;
        const int32 OffsetY = (int32)( NewSizeY - SizeY ) / 2;

        // Expanding the Data
        NewData = ExpandData(
            LayerData,
            0, 0, SizeX - 1, SizeY - 1,
            -OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1 );
    }
    else
    {
        // Resampling the data
        NewData.SetNumUninitialized( NewSizeX * NewSizeY );
        NewData = ResampleData( LayerData, SizeX, SizeY, NewSizeX, NewSizeY );
    }

    LayerData = NewData;

    return true;
}

#if WITH_EDITOR
bool
FHoudiniLandscapeUtils::CreateHeightfieldFromLandscape(
    ALandscapeProxy* LandscapeProxy, HAPI_NodeId& CreatedHeightfieldNodeId )
{
    if ( !LandscapeProxy )
        return false;

    // Export the whole landscape and its layer as a single heightfield

    //--------------------------------------------------------------------------------------------------
    // 1. Extracting the height data
    //--------------------------------------------------------------------------------------------------
    TArray<uint16> HeightData;
    int32 XSize, YSize;
    FVector Min, Max;
    if ( !GetLandscapeData( LandscapeProxy, HeightData, XSize, YSize, Min, Max ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 2. Convert the height uint16 data to float
    //--------------------------------------------------------------------------------------------------
    TArray<float> HeightfieldFloatValues;
    HAPI_VolumeInfo HeightfieldVolumeInfo;
    FHoudiniApi::VolumeInfo_Init(&HeightfieldVolumeInfo);
    FTransform LandscapeTransform = LandscapeProxy->ActorToWorld();
    FVector CenterOffset = FVector::ZeroVector;
    if ( !ConvertLandscapeDataToHeightfieldData(
        HeightData, XSize, YSize, Min, Max, LandscapeTransform,
        HeightfieldFloatValues, HeightfieldVolumeInfo, CenterOffset ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 3. Create the Heightfield Input Node
    //-------------------------------------------------------------------------------------------------- 
    HAPI_NodeId HeightFieldId = -1;
    HAPI_NodeId HeightId = -1;
    HAPI_NodeId MaskId = -1;
    HAPI_NodeId MergeId = -1;
    if ( !CreateHeightfieldInputNode( -1, LandscapeProxy->GetName(), XSize, YSize, HeightFieldId, HeightId, MaskId, MergeId ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 4. Set the HeightfieldData in Houdini
    //--------------------------------------------------------------------------------------------------    
    // Set the Height volume's data
    HAPI_PartId PartId = 0;
    if ( !SetHeighfieldData( HeightId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, TEXT("height") ) )
        return false;

    // Add the materials used
    UMaterialInterface* LandscapeMat = LandscapeProxy->GetLandscapeMaterial();
    UMaterialInterface* LandscapeHoleMat = LandscapeProxy->GetLandscapeHoleMaterial();
    AddLandscapeMaterialAttributesToVolume( HeightId, PartId, LandscapeMat, LandscapeHoleMat );

    // Add the landscape's actor tags as prim attributes if we have any    
    FHoudiniEngineUtils::CreateGroupOrAttributeFromTags(HeightId, PartId, LandscapeProxy->Tags, true);

    // Commit the height volume
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), HeightId ), false );

    //--------------------------------------------------------------------------------------------------
    // 5. Extract and convert all the layers
    //--------------------------------------------------------------------------------------------------
    ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
    if ( !LandscapeInfo )
        return false;

    bool MaskInitialized = false;
    int32 MergeInputIndex = 2;
    int32 NumLayers = LandscapeInfo->Layers.Num();
    for ( int32 n = 0; n < NumLayers; n++ )
    {
        // 1. Extract the uint8 values from the layer
        TArray<uint8> CurrentLayerIntData;
        FLinearColor LayerUsageDebugColor;
        FString LayerName;
        if ( !GetLandscapeLayerData( LandscapeInfo, n, CurrentLayerIntData, LayerUsageDebugColor, LayerName ) )
            continue;

        // 2. Convert unreal uint8 values to floats
        // If the layer came from Houdini, additional info might have been stored in the DebugColor to convert the data back to float
        HAPI_VolumeInfo CurrentLayerVolumeInfo;
        FHoudiniApi::VolumeInfo_Init(&CurrentLayerVolumeInfo);
        TArray < float > CurrentLayerFloatData;
        if ( !ConvertLandscapeLayerDataToHeightfieldData(
            CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
            CurrentLayerFloatData, CurrentLayerVolumeInfo ) )
            continue;

        // We reuse the height layer's transform
        CurrentLayerVolumeInfo.transform = HeightfieldVolumeInfo.transform;

        // 3. See if we need to create an input volume, or can reuse the HF's default mask volume
        bool IsMask = false;
        if ( LayerName.Equals( TEXT("mask"), ESearchCase::IgnoreCase ) )
            IsMask = true;

        HAPI_NodeId LayerVolumeNodeId = -1;
        if ( !IsMask )
        {
            // Current layer is not mask, so we need to create a new input volume
            std::string LayerNameStr;
            FHoudiniEngineUtils::ConvertUnrealString(LayerName, LayerNameStr);

            FHoudiniApi::CreateHeightfieldInputVolumeNode(
                FHoudiniEngine::Get().GetSession(),
                HeightFieldId, &LayerVolumeNodeId, LayerNameStr.c_str(), XSize, YSize, 1.0f );
        }
        else
        {
            // Current Layer is mask, so we simply reuse the mask volume node created by default by the heightfield node
            LayerVolumeNodeId = MaskId;
        }

        // Check if we have a valid id for the input volume.
        if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( LayerVolumeNodeId ) )
            continue;

        // 4. Set the layer/mask heighfield data in Houdini
        HAPI_PartId CurrentPartId = 0;
        if ( !SetHeighfieldData( LayerVolumeNodeId, PartId, CurrentLayerFloatData, CurrentLayerVolumeInfo, LayerName ) )
            continue;

        // Also add the material attributes to the layer volumes
        AddLandscapeMaterialAttributesToVolume(LayerVolumeNodeId, PartId, LandscapeMat, LandscapeHoleMat);

        // Add the landscape's actor tags as prim attributes if we have any    
        FHoudiniEngineUtils::CreateGroupOrAttributeFromTags(LayerVolumeNodeId, PartId, LandscapeProxy->Tags, true);

        // Commit the volume's geo
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), LayerVolumeNodeId ), false);

        if ( !IsMask )
        {
            // We had to create a new volume for this layer, so we need to connect it to the HF's merge node
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
                FHoudiniEngine::Get().GetSession(),
                MergeId, MergeInputIndex, LayerVolumeNodeId, 0 ), false);

            MergeInputIndex++;
        }
        else
        {
            MaskInitialized = true;
        }
    }

    // We need to have a mask layer as it is required for proper heightfield functionalities
    // Setting the volume info on the mask is needed for the HF to have proper transform in H!
    // If we didn't create a mask volume before, send a default one now
    if (!MaskInitialized)
    {
        MaskInitialized = InitDefaultHeightfieldMask( HeightfieldVolumeInfo, MaskId );

        // Add the materials used
        AddLandscapeMaterialAttributesToVolume( MaskId, PartId, LandscapeMat, LandscapeHoleMat );

        // Add the landscape's actor tags as prim attributes if we have any    
        FHoudiniEngineUtils::CreateGroupOrAttributeFromTags(MaskId, PartId, LandscapeProxy->Tags, true);

        // Commit the mask volume's geo
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), MaskId ), false );
    }

    HAPI_TransformEuler HAPIObjectTransform;
    FHoudiniApi::TransformEuler_Init(&HAPIObjectTransform);
    //FMemory::Memzero< HAPI_TransformEuler >( HAPIObjectTransform );
    LandscapeTransform.SetScale3D(FVector::OneVector);    
    FHoudiniEngineUtils::TranslateUnrealTransform( LandscapeTransform, HAPIObjectTransform );
    HAPIObjectTransform.position[1] = 0.0f;

    HAPI_NodeId ParentObjNodeId = FHoudiniEngineUtils::HapiGetParentNodeId( HeightFieldId );
    FHoudiniApi::SetObjectTransform(FHoudiniEngine::Get().GetSession(), ParentObjNodeId, &HAPIObjectTransform);

    // Since HF are centered but landscape aren't, we need to set the HF's center parameter
    FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 0, CenterOffset.X);
    FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 1, 0.0);
    FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 2, CenterOffset.Y);

    // Finally, cook the Heightfield node
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), HeightFieldId, nullptr), false );

    CreatedHeightfieldNodeId = HeightFieldId;

    return true;
}

bool
FHoudiniLandscapeUtils::CreateHeightfieldFromLandscapeComponentArray(
    ALandscapeProxy* LandscapeProxy,
    TSet< ULandscapeComponent * >& LandscapeComponentArray, 
    HAPI_NodeId& CreatedHeightfieldNodeId )
{
    if ( LandscapeComponentArray.Num() <= 0 )
        return false;

    //--------------------------------------------------------------------------------------------------
    //  Each selected component will be exported as tiled volumes in a single heightfield
    //--------------------------------------------------------------------------------------------------
    FTransform LandscapeTransform = LandscapeProxy->GetTransform();

    //
    HAPI_NodeId HeightfieldNodeId = -1;
    HAPI_NodeId HeightfieldeMergeId = -1;

    int32 MergeInputIndex = 0;
    bool bAllComponentCreated = true;
    for (int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++)
    {
        ULandscapeComponent * CurrentComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];
        if ( !CurrentComponent )
            continue;

        if ( !LandscapeComponentArray.Contains( CurrentComponent ) )
            continue;

        if ( !CreateHeightfieldFromLandscapeComponent( CurrentComponent, ComponentIdx, HeightfieldNodeId, HeightfieldeMergeId, MergeInputIndex ) )
            bAllComponentCreated = false;
    }

    // Check that we have a valid id for the input Heightfield.
    if ( FHoudiniEngineUtils::IsHoudiniNodeValid( HeightfieldNodeId ) )
        CreatedHeightfieldNodeId = HeightfieldNodeId;

    // Set the HF's parent OBJ's tranform to the Landscape's transform
    HAPI_TransformEuler HAPIObjectTransform;
    FHoudiniApi::TransformEuler_Init(&HAPIObjectTransform);
    //FMemory::Memzero< HAPI_TransformEuler >( HAPIObjectTransform );
    LandscapeTransform.SetScale3D( FVector::OneVector );
    FHoudiniEngineUtils::TranslateUnrealTransform( LandscapeTransform, HAPIObjectTransform );
    HAPIObjectTransform.position[ 1 ] = 0.0f;

    HAPI_NodeId ParentObjNodeId = FHoudiniEngineUtils::HapiGetParentNodeId( HeightfieldNodeId );
    FHoudiniApi::SetObjectTransform( FHoudiniEngine::Get().GetSession(), ParentObjNodeId, &HAPIObjectTransform );

    return bAllComponentCreated;
}


bool
FHoudiniLandscapeUtils::CreateHeightfieldFromLandscapeComponent(
    ULandscapeComponent * LandscapeComponent,
    const int32& ComponentIndex,
    HAPI_NodeId& HeightFieldId,
    HAPI_NodeId& MergeId,
    int32& MergeInputIndex )
{
    if ( !LandscapeComponent )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 1. Extract the height data
    //--------------------------------------------------------------------------------------------------
    int32 MinX = MAX_int32;
    int32 MinY = MAX_int32;
    int32 MaxX = -MAX_int32;
    int32 MaxY = -MAX_int32;
    LandscapeComponent->GetComponentExtent( MinX, MinY, MaxX, MaxY );
    
    ULandscapeInfo* LandscapeInfo = LandscapeComponent->GetLandscapeInfo();
    if ( !LandscapeInfo )
        return false;

    TArray<uint16> HeightData;
    int32 XSize, YSize;
    if ( !GetLandscapeData( LandscapeInfo, MinX, MinY, MaxX, MaxY, HeightData, XSize, YSize ) )
        return false;

    FVector Origin = LandscapeComponent->Bounds.Origin;
    FVector Extents = LandscapeComponent->Bounds.BoxExtent;
    FVector Min = Origin - Extents;
    FVector Max = Origin + Extents;

    //--------------------------------------------------------------------------------------------------
    // 2. Convert the landscape's height uint16 data to float
    //--------------------------------------------------------------------------------------------------
    TArray<float> HeightfieldFloatValues;
    HAPI_VolumeInfo HeightfieldVolumeInfo;
    FHoudiniApi::VolumeInfo_Init(&HeightfieldVolumeInfo);
    FTransform LandscapeComponentTransform = LandscapeComponent->GetComponentTransform();

    FVector CenterOffset = FVector::ZeroVector;
    if ( !ConvertLandscapeDataToHeightfieldData(
        HeightData, XSize, YSize, Min, Max, LandscapeComponentTransform,
        HeightfieldFloatValues, HeightfieldVolumeInfo,
        CenterOffset ) )
        return false;

    // We need to modify the Volume's position to the Component's position relative to the Landscape's position
    FVector RelativePosition = LandscapeComponent->GetRelativeTransform().GetLocation();
    HeightfieldVolumeInfo.transform.position[1] = RelativePosition.X;
    HeightfieldVolumeInfo.transform.position[0] = RelativePosition.Y;
    HeightfieldVolumeInfo.transform.position[2] = 0.0f;

    //--------------------------------------------------------------------------------------------------
    // 3. Create the Heightfield Input Node
    //--------------------------------------------------------------------------------------------------
    HAPI_NodeId HeightId = -1;
    HAPI_NodeId MaskId = -1;
    bool CreatedHeightfieldNode = false;
    if ( HeightFieldId < 0 || MergeId < 0 )
    {
        // We haven't created the HF input node yet, do it now
        if (!CreateHeightfieldInputNode(-1, TEXT("LandscapeComponents"), XSize, YSize, HeightFieldId, HeightId, MaskId, MergeId))
            return false;

        MergeInputIndex = 2;
        CreatedHeightfieldNode = true;
    }
    else
    {
        // Heightfield node was previously created, create additionnal height and a mask volumes for it
        FHoudiniApi::CreateHeightfieldInputVolumeNode(
            FHoudiniEngine::Get().GetSession(),
            HeightFieldId, &HeightId, "height", XSize, YSize, 1.0f );

        FHoudiniApi::CreateHeightfieldInputVolumeNode(
            FHoudiniEngine::Get().GetSession(),
            HeightFieldId, &MaskId, "mask", XSize, YSize, 1.0f );

        // Connect the two newly created volumes to the HF's merge node
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(),
            MergeId, MergeInputIndex++, HeightId, 0 ), false );

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(),
            MergeId, MergeInputIndex++, MaskId, 0 ), false );
    }


    //--------------------------------------------------------------------------------------------------
    // 4. Set the HeightfieldData in Houdini
    //--------------------------------------------------------------------------------------------------    
    // Set the Height volume's data
    HAPI_PartId PartId = 0;
    if (!SetHeighfieldData( HeightId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, TEXT("height") ) )
        return false;

    // Add the materials used
    UMaterialInterface* LandscapeMat = LandscapeComponent->GetLandscapeMaterial();
    UMaterialInterface* LandscapeHoleMat = LandscapeComponent->GetLandscapeHoleMaterial();
    AddLandscapeMaterialAttributesToVolume(HeightId, PartId, LandscapeMat, LandscapeHoleMat);

    // Add the tile attribute
    AddLandscapeTileAttribute(HeightId, PartId, ComponentIndex);

    // Add the landscape component extent attribute
    AddLandscapeComponentExtentAttributes( HeightId, PartId, MinX, MaxX, MinY, MaxY );

    // Add the component's tag as prim attributes if we have any
    FHoudiniEngineUtils::CreateGroupOrAttributeFromTags(HeightId, PartId, LandscapeComponent->ComponentTags, true);

    // Commit the height volume
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), HeightId), false);

    //--------------------------------------------------------------------------------------------------
    // 4. Extract and convert all the layers to HF masks
    //--------------------------------------------------------------------------------------------------
    bool MaskInitialized = false;
    int32 NumLayers = LandscapeInfo->Layers.Num();
    for ( int32 n = 0; n < NumLayers; n++ )
    {
        // 1. Extract the uint8 values from the layer
        TArray<uint8> CurrentLayerIntData;
        FLinearColor LayerUsageDebugColor;
        FString LayerName;
        if ( !GetLandscapeLayerData(LandscapeInfo, n, MinX, MinY, MaxX, MaxY, CurrentLayerIntData, LayerUsageDebugColor, LayerName) )
            continue;

        // 2. Convert unreal uint8 to float
        // If the layer came from Houdini, additional info might have been stored in the DebugColor
        HAPI_VolumeInfo CurrentLayerVolumeInfo;
        FHoudiniApi::VolumeInfo_Init(&CurrentLayerVolumeInfo);
        TArray < float > CurrentLayerFloatData;
        if ( !ConvertLandscapeLayerDataToHeightfieldData(
            CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
            CurrentLayerFloatData, CurrentLayerVolumeInfo ) )
            continue;

        // We reuse the transform used for the height volume
        CurrentLayerVolumeInfo.transform = HeightfieldVolumeInfo.transform;

        // 3. See if we need to create an input volume, or if we can reuse the HF's default mask volume
        bool IsMask = false;
        if ( LayerName.Equals( TEXT("mask"), ESearchCase::IgnoreCase ) )
            IsMask = true;

        HAPI_NodeId LayerVolumeNodeId = -1;
        if ( !IsMask )
        {
            // Current layer is not mask, so we need to create a new input volume
            std::string LayerNameStr;
            FHoudiniEngineUtils::ConvertUnrealString( LayerName, LayerNameStr );

            FHoudiniApi::CreateHeightfieldInputVolumeNode(
                FHoudiniEngine::Get().GetSession(),
                HeightFieldId, &LayerVolumeNodeId, LayerNameStr.c_str(), XSize, YSize, 1.0f );
        }
        else
        {
            // Current Layer is mask, so we simply reuse the mask volume node created by default by the heightfield node
            LayerVolumeNodeId = MaskId;
        }

        // Check if we have a valid id for the input volume.
        if ( !FHoudiniEngineUtils::IsHoudiniNodeValid( LayerVolumeNodeId ) )
            continue;

        // 4. Set the layer/mask heighfield data in Houdini
        HAPI_PartId CurrentPartId = 0;
        if ( !SetHeighfieldData( LayerVolumeNodeId, PartId, CurrentLayerFloatData, CurrentLayerVolumeInfo, LayerName ) )
            continue;

        // Add the materials used
        AddLandscapeMaterialAttributesToVolume( LayerVolumeNodeId, PartId, LandscapeMat, LandscapeHoleMat );

        // Add the tile attribute
        AddLandscapeTileAttribute( LayerVolumeNodeId, PartId, ComponentIndex );

        // Add the landscape component extent attribute
        AddLandscapeComponentExtentAttributes( LayerVolumeNodeId, PartId, MinX, MaxX, MinY, MaxY );

        // Add the component's tag as prim attributes if we have any
        FHoudiniEngineUtils::CreateGroupOrAttributeFromTags(LayerVolumeNodeId, PartId, LandscapeComponent->ComponentTags, true);

        // Commit the volume's geo
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), LayerVolumeNodeId ), false);

        if ( !IsMask )
        {
            // We had to create a new volume for this layer, so we need to connect it to the HF's merge node
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
                FHoudiniEngine::Get().GetSession(),
                MergeId, MergeInputIndex, LayerVolumeNodeId, 0 ), false);

            MergeInputIndex++;
        }
        else
        {
            MaskInitialized = true;
        }
    }

    // We need to have a mask layer as it is required for proper heightfield functionalities
    // Setting the volume info on the mask is needed for the HF to have proper transform in H!
    // If we didn't create a mask volume before, send a default one now
    if (!MaskInitialized)
    {
        MaskInitialized = InitDefaultHeightfieldMask( HeightfieldVolumeInfo, MaskId );

        // Add the materials used
        AddLandscapeMaterialAttributesToVolume( MaskId, PartId, LandscapeMat, LandscapeHoleMat );

        // Add the tile attribute
        AddLandscapeTileAttribute( MaskId, PartId, ComponentIndex );

        // Add the landscape component extent attribute
        AddLandscapeComponentExtentAttributes( MaskId, PartId, MinX, MaxX, MinY, MaxY );

        // Add the component's tag as prim attributes if we have any
        FHoudiniEngineUtils::CreateGroupOrAttributeFromTags(MaskId, PartId, LandscapeComponent->ComponentTags, true);

        // Commit the mask volume's geo
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
            FHoudiniEngine::Get().GetSession(), MaskId), false);
    }

    if ( CreatedHeightfieldNode )
    {
        // Since HF are centered but landscape arent, we need to set the HF's center parameter
        // Do it only once after creating the Heightfield node
        FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 0, CenterOffset.X);
        FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 1, 0.0);
        FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 2, CenterOffset.Y);
    }    

    // Finally, cook the Heightfield node
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), HeightFieldId, nullptr), false);

    return true;
}

bool
FHoudiniLandscapeUtils::GetLandscapeData(
    ALandscape* Landscape,
    TArray<uint16>& HeightData,
    int32& XSize, int32& YSize,
    FVector& Min, FVector& Max )
{
    if ( !Landscape )
        return false;

    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    if ( !LandscapeInfo )
        return false;

    // Get the landscape extents to get its size
    int32 MinX = MAX_int32;
    int32 MinY = MAX_int32;
    int32 MaxX = -MAX_int32;
    int32 MaxY = -MAX_int32;

    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
        return false;

    if ( !GetLandscapeData( LandscapeInfo, MinX, MinY, MaxX, MaxY, HeightData, XSize, YSize ) )
        return false;

    // Get the landscape Min/Max values
    // Do not use Landscape->GetActorBounds() here as instanced geo
    // (due to grass layers for example) can cause it to return incorrect bounds!
    FVector Origin, Extent;
    GetLandscapeActorBounds(Landscape, Origin, Extent);

    // Get the landscape Min/Max values
    Min = Origin - Extent;
    Max = Origin + Extent;

    return true;
}

bool
FHoudiniLandscapeUtils::GetLandscapeData(
    ALandscapeProxy* LandscapeProxy,
    TArray<uint16>& HeightData,
    int32& XSize, int32& YSize,
    FVector& Min, FVector& Max)
{
    if (!LandscapeProxy)
        return false;

    ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
    if (!LandscapeInfo)
        return false;

    // Get the landscape extents to get its size
    int32 MinX = MAX_int32;
    int32 MinY = MAX_int32;
    int32 MaxX = -MAX_int32;
    int32 MaxY = -MAX_int32;

    // To handle streaming proxies correctly, get the extents via all the components,
    // not by calling GetLandscapeExtent or we'll end up sending ALL the streaming proxies.
    for (const ULandscapeComponent* Comp : LandscapeProxy->LandscapeComponents)
    {
        Comp->GetComponentExtent(MinX, MinY, MaxX, MaxY);
    }

    if (!GetLandscapeData(LandscapeInfo, MinX, MinY, MaxX, MaxY, HeightData, XSize, YSize))
        return false;

    // Get the landscape Min/Max values
    // Do not use Landscape->GetActorBounds() here as instanced geo
    // (due to grass layers for example) can cause it to return incorrect bounds!
    FVector Origin, Extent;
    GetLandscapeProxyBounds(LandscapeProxy, Origin, Extent);

    // Get the landscape Min/Max values
    Min = Origin - Extent;
    Max = Origin + Extent;

    return true;
}


void
FHoudiniLandscapeUtils::GetLandscapeActorBounds(
    ALandscape* Landscape, FVector& Origin, FVector& Extents )
{
    // Iterate only on the landscape components
    FBox Bounds(ForceInit);
    for (const UActorComponent* ActorComponent : Landscape->GetComponents())
    {
        const ULandscapeComponent* LandscapeComp = Cast<const ULandscapeComponent>(ActorComponent);
        if ( LandscapeComp && LandscapeComp->IsRegistered() )
            Bounds += LandscapeComp->Bounds.GetBox();
    }

    // Convert the bounds to origin/offset vectors
    Bounds.GetCenterAndExtents(Origin, Extents);
}

void
FHoudiniLandscapeUtils::GetLandscapeProxyBounds(
    ALandscapeProxy* LandscapeProxy, FVector& Origin, FVector& Extents)
{
    // Iterate only on the landscape components
    FBox Bounds(ForceInit);
    for (const UActorComponent* ActorComponent : LandscapeProxy->GetComponents())
    {
        const ULandscapeComponent* LandscapeComp = Cast<const ULandscapeComponent>(ActorComponent);
        if (LandscapeComp && LandscapeComp->IsRegistered())
            Bounds += LandscapeComp->Bounds.GetBox();
    }

    // Convert the bounds to origin/offset vectors
    Bounds.GetCenterAndExtents(Origin, Extents);
}

bool
FHoudiniLandscapeUtils::GetLandscapeData(
    ULandscapeInfo* LandscapeInfo,
    const int32& MinX, const int32& MinY,
    const int32& MaxX, const int32& MaxY,
    TArray<uint16>& HeightData,
    int32& XSize, int32& YSize )
{
    if ( !LandscapeInfo )
        return false;

    // Get the X/Y size in points
    XSize = ( MaxX - MinX + 1 );
    YSize = ( MaxY - MinY + 1 );

    if ( ( XSize < 2 ) || ( YSize < 2 ) )
        return false;

    // Extracting the uint16 values from the landscape
    FLandscapeEditDataInterface LandscapeEdit( LandscapeInfo );
    HeightData.AddZeroed( XSize * YSize );
    LandscapeEdit.GetHeightDataFast( MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0 );

    return true;
}

bool
FHoudiniLandscapeUtils::GetLandscapeLayerData(
    ULandscapeInfo* LandscapeInfo, const int32& LayerIndex,
    TArray<uint8>& LayerData, FLinearColor& LayerUsageDebugColor,
    FString& LayerName )
{
    if ( !LandscapeInfo )
        return false;

    // Get the landscape X/Y Size
    int32 MinX = MAX_int32;
    int32 MinY = MAX_int32;
    int32 MaxX = -MAX_int32;
    int32 MaxY = -MAX_int32;
    if ( !LandscapeInfo->GetLandscapeExtent( MinX, MinY, MaxX, MaxY ) )
        return false;

    if ( !GetLandscapeLayerData(
        LandscapeInfo, LayerIndex,
        MinX, MinY, MaxX, MaxY,
        LayerData, LayerUsageDebugColor, LayerName ) )
        return false;

    return true;
}

bool
FHoudiniLandscapeUtils::GetLandscapeLayerData(
    ULandscapeInfo* LandscapeInfo,
    const int32& LayerIndex,
    const int32& MinX, const int32& MinY,
    const int32& MaxX, const int32& MaxY,
    TArray<uint8>& LayerData,
    FLinearColor& LayerUsageDebugColor,
    FString& LayerName )
{
    if ( !LandscapeInfo )
        return false;

    if ( !LandscapeInfo->Layers.IsValidIndex( LayerIndex ) )
        return false;

    FLandscapeInfoLayerSettings LayersSetting = LandscapeInfo->Layers[ LayerIndex ];
    ULandscapeLayerInfoObject* LayerInfo = LayersSetting.LayerInfoObj;
    if (!LayerInfo)
        return false;

    // Calc the X/Y size in points
    int32 XSize = (MaxX - MinX + 1);
    int32 YSize = (MaxY - MinY + 1);
    if ( (XSize < 2) || (YSize < 2) )
        return false;

    // extracting the uint8 values from the layer
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    LayerData.AddZeroed(XSize * YSize);
    LandscapeEdit.GetWeightDataFast(LayerInfo, MinX, MinY, MaxX, MaxY, LayerData.GetData(), 0);

    LayerUsageDebugColor = LayerInfo->LayerUsageDebugColor;

    LayerName = LayersSetting.GetLayerName().ToString();

    return true;
}
#endif

bool
FHoudiniLandscapeUtils::ConvertLandscapeDataToHeightfieldData(
    const TArray<uint16>& IntHeightData,
    const int32& XSize, const int32& YSize,
    FVector Min, FVector Max, 
    const FTransform& LandscapeTransform,
    TArray<float>& HeightfieldFloatValues,
    HAPI_VolumeInfo& HeightfieldVolumeInfo,
    FVector& CenterOffset)
{
    HeightfieldFloatValues.Empty();

    int32 HoudiniXSize = YSize;
    int32 HoudiniYSize = XSize;
    int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
    if ( (HoudiniXSize < 2) || (HoudiniYSize < 2) )
        return false;

    if ( IntHeightData.Num() != SizeInPoints )
        return false;

    // Use default unreal scaling for marshalling landscapes
    // A lot of precision will be lost in order to keep the same transform as the landscape input
    bool bUseDefaultUE4Scaling = false;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
        bUseDefaultUE4Scaling = HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling;

    //--------------------------------------------------------------------------------------------------
    // 1. Convert values to float
    //--------------------------------------------------------------------------------------------------


    // Convert the min/max values from cm to meters
    Min /= 100.0;
    Max /= 100.0;

    // Unreal's landscape uses 16bits precision and range from -256m to 256m with the default scale of 100.0
    // To convert the uint16 values to float "metric" values, offset the int by 32768 to center it,
    // then scale it

    // Spacing used to convert from uint16 to meters
    double ZSpacing = 512.0 / ((double)UINT16_MAX);
    ZSpacing *= ( (double)LandscapeTransform.GetScale3D().Z / 100.0 );

    // Center value in meters (Landscape ranges from [-255:257] meters at default scale
    double ZCenterOffset = 32767;
    double ZPositionOffset = LandscapeTransform.GetLocation().Z / 100.0f;
    // Convert the Int data to Float
    HeightfieldFloatValues.SetNumUninitialized( SizeInPoints );

    for ( int32 nY = 0; nY < HoudiniYSize; nY++ )
    {
        for ( int32 nX = 0; nX < HoudiniXSize; nX++ )
        {
            // We need to invert X/Y when reading the value from Unreal
            int32 nHoudini = nX + nY * HoudiniXSize;
            int32 nUnreal = nY + nX * XSize;

            // Convert the int values to meter
            // Unreal's digit value have a zero value of 32768
            double DoubleValue = ((double)IntHeightData[nUnreal] - ZCenterOffset) * ZSpacing + ZPositionOffset;
            HeightfieldFloatValues[nHoudini] = (float)DoubleValue;
        }
    }

    //--------------------------------------------------------------------------------------------------
    // 2. Convert the Unreal Transform to a HAPI_transform
    //--------------------------------------------------------------------------------------------------
    HAPI_Transform HapiTransform;
    FHoudiniApi::Transform_Init(&HapiTransform);
    //FMemory::Memzero< HAPI_Transform >( HapiTransform );
    {
        FQuat Rotation = LandscapeTransform.GetRotation();
        if ( Rotation != FQuat::Identity )
        {
            //Swap(ObjectRotation.Y, ObjectRotation.Z);
            HapiTransform.rotationQuaternion[0] = Rotation.X;
            HapiTransform.rotationQuaternion[1] = Rotation.Z;
            HapiTransform.rotationQuaternion[2] = Rotation.Y;
            HapiTransform.rotationQuaternion[3] = -Rotation.W;
        }
        else
        {
            HapiTransform.rotationQuaternion[0] = 0;
            HapiTransform.rotationQuaternion[1] = 0;
            HapiTransform.rotationQuaternion[2] = 0;
            HapiTransform.rotationQuaternion[3] = 1;
        }

        // Heightfield are centered, landscapes are not
        CenterOffset = (Max - Min) * 0.5f;

        // Unreal XYZ becomes Houdini YXZ (since heightfields are also rotated due the ZX transform) 
        //FVector Position = LandscapeTransform.GetLocation() / 100.0f;
        HapiTransform.position[ 1 ] = 0.0f;//Position.X + CenterOffset.X;
        HapiTransform.position[ 0 ] = 0.0f;//Position.Y + CenterOffset.Y;
        HapiTransform.position[ 2 ] = 0.0f;

        FVector Scale = LandscapeTransform.GetScale3D() / 100.0f;
        HapiTransform.scale[ 0 ] = Scale.X * 0.5f * HoudiniXSize;
        HapiTransform.scale[ 1 ] = Scale.Y * 0.5f * HoudiniYSize;
        HapiTransform.scale[ 2 ] = 0.5f;
        if ( bUseDefaultUE4Scaling )
            HapiTransform.scale[2] *= Scale.Z;

        HapiTransform.shear[ 0 ] = 0.0f;
        HapiTransform.shear[ 1 ] = 0.0f;
        HapiTransform.shear[ 2 ] = 0.0f;
    }

    //--------------------------------------------------------------------------------------------------
    // 3. Fill the volume info
    //--------------------------------------------------------------------------------------------------
    HeightfieldVolumeInfo.xLength = HoudiniXSize;
    HeightfieldVolumeInfo.yLength = HoudiniYSize;
    HeightfieldVolumeInfo.zLength = 1;

    HeightfieldVolumeInfo.minX = 0;
    HeightfieldVolumeInfo.minY = 0;
    HeightfieldVolumeInfo.minZ = 0;
    
    HeightfieldVolumeInfo.transform = HapiTransform;
    
    HeightfieldVolumeInfo.type = HAPI_VOLUMETYPE_HOUDINI;
    HeightfieldVolumeInfo.storage = HAPI_STORAGETYPE_FLOAT;
    HeightfieldVolumeInfo.tupleSize = 1;
    HeightfieldVolumeInfo.tileSize = 1;
    
    HeightfieldVolumeInfo.hasTaper = false;
    HeightfieldVolumeInfo.xTaper = 0.0;
    HeightfieldVolumeInfo.yTaper = 0.0;

    return true;
}

// Converts Unreal uint16 values to Houdini Float
bool
FHoudiniLandscapeUtils::ConvertLandscapeLayerDataToHeightfieldData(
    const TArray<uint8>& IntHeightData,
    const int32& XSize, const int32& YSize,
    const FLinearColor& LayerUsageDebugColor,
    TArray<float>& LayerFloatValues,
    HAPI_VolumeInfo& LayerVolumeInfo)
{
    LayerFloatValues.Empty();

    int32 HoudiniXSize = YSize;
    int32 HoudiniYSize = XSize;
    int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
    if ( ( HoudiniXSize < 2 ) || ( HoudiniYSize < 2 ) )
        return false;

    if ( IntHeightData.Num() != SizeInPoints )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 1. Convert values to float
    //--------------------------------------------------------------------------------------------------

    // We need the ZMin / ZMax unt8 values
    uint8 IntMin = IntHeightData[ 0 ];
    uint8 IntMax = IntMin;

    for ( int n = 0; n < IntHeightData.Num(); n++ )
    {
        if ( IntHeightData[ n ] < IntMin )
            IntMin = IntHeightData[ n ];
        if ( IntHeightData[ n ] > IntMax )
            IntMax = IntHeightData[ n ];
    }

    // The range in Digits
    double DigitRange = (double)IntMax - (double)IntMin;

    // By default, the values will be converted to [0, 1]
    float LayerMin = 0.0f;
    float LayerMax = 1.0f;
    float LayerSpacing = 1.0f / DigitRange;

    // If this layer came from Houdini, its alpha value should be PI
    // So we can extract the additionnal infos stored its debug usage color
    if ( LayerUsageDebugColor.A == PI )
    {
        LayerMin = LayerUsageDebugColor.R;
        LayerMax = LayerUsageDebugColor.G;
        LayerSpacing = LayerUsageDebugColor.B;
    }

    LayerSpacing = ( LayerMax - LayerMin ) / DigitRange;

    // Convert the Int data to Float
    LayerFloatValues.SetNumUninitialized( SizeInPoints );

    for ( int32 nY = 0; nY < HoudiniYSize; nY++ )
    {
        for ( int32 nX = 0; nX < HoudiniXSize; nX++ )
        {
            // We need to invert X/Y when reading the value from Unreal
            int32 nHoudini = nX + nY * HoudiniXSize;
            int32 nUnreal = nY + nX * XSize;

            // Convert the int values to meter
            // Unreal's digit value have a zero value of 32768
            double DoubleValue = ( (double)IntHeightData[ nUnreal ] - (double)IntMin ) * LayerSpacing + LayerMin;
            LayerFloatValues[ nHoudini ] = (float)DoubleValue;
        }
    }

    // Verifying the converted ZMin / ZMax
    float FloatMin = LayerFloatValues[ 0 ];
    float FloatMax = FloatMin;
    for ( int32 n = 0; n < LayerFloatValues.Num(); n++ )
    {
        if ( LayerFloatValues[ n ] < FloatMin )
            FloatMin = LayerFloatValues[ n ];
        if ( LayerFloatValues[ n ] > FloatMax )
            FloatMax = LayerFloatValues[ n ];
    }

    //--------------------------------------------------------------------------------------------------
    // 2. Fill the volume info
    //--------------------------------------------------------------------------------------------------
    LayerVolumeInfo.xLength = HoudiniXSize;
    LayerVolumeInfo.yLength = HoudiniYSize;
    LayerVolumeInfo.zLength = 1;

    LayerVolumeInfo.minX = 0;
    LayerVolumeInfo.minY = 0;
    LayerVolumeInfo.minZ = 0;

    LayerVolumeInfo.type = HAPI_VOLUMETYPE_HOUDINI;
    LayerVolumeInfo.storage = HAPI_STORAGETYPE_FLOAT;
    LayerVolumeInfo.tupleSize = 1;
    LayerVolumeInfo.tileSize = 1;

    LayerVolumeInfo.hasTaper = false;
    LayerVolumeInfo.xTaper = 0.0;
    LayerVolumeInfo.yTaper = 0.0;

    // The layer transform will have to be copied from the main heightfield's transform
    return true;
}

bool
FHoudiniLandscapeUtils::SetHeighfieldData(
    const HAPI_NodeId& VolumeNodeId,
    const HAPI_PartId& PartId,
    TArray<float>& FloatValues,
    const HAPI_VolumeInfo& VolumeInfo,
    const FString& HeightfieldName )
{
    // Cook the node to get proper infos on it
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), VolumeNodeId, nullptr ), false );

    // Read the geo/part/volume info from the volume node
    HAPI_GeoInfo GeoInfo;
    FHoudiniApi::GeoInfo_Init(&GeoInfo);
    //FMemory::Memset< HAPI_GeoInfo >(GeoInfo, 0);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
        FHoudiniEngine::Get().GetSession(),
        VolumeNodeId, &GeoInfo), false);

    HAPI_PartInfo PartInfo;
    FHoudiniApi::PartInfo_Init(&PartInfo);
    //FMemory::Memset< HAPI_PartInfo >(PartInfo, 0);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(),
        GeoInfo.nodeId, PartId, &PartInfo), false);

    // Update the volume infos
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetVolumeInfo(
        FHoudiniEngine::Get().GetSession(),
        VolumeNodeId, PartInfo.id, &VolumeInfo), false );

    // Volume name
    std::string NameStr;
    FHoudiniEngineUtils::ConvertUnrealString( HeightfieldName, NameStr );

    // Set the Heighfield data on the volume
    float * HeightData = FloatValues.GetData();
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetHeightFieldData(
        FHoudiniEngine::Get().GetSession(),
        GeoInfo.nodeId, PartInfo.id, NameStr.c_str(), HeightData, 0, FloatValues.Num() ), false );

    return true;
}

bool
FHoudiniLandscapeUtils::CreateHeightfieldInputNode(
    const HAPI_NodeId& ParentNodeId, const FString& NodeName, const int32& XSize, const int32& YSize,
    HAPI_NodeId& HeightfieldNodeId, HAPI_NodeId& HeightNodeId, HAPI_NodeId& MaskNodeId, HAPI_NodeId& MergeNodeId )
{
    // Make sure the Heightfield node doesnt already exists
    if (HeightfieldNodeId != -1)
        return false;

    // Convert the node's name
    std::string NameStr;
    FHoudiniEngineUtils::ConvertUnrealString(NodeName, NameStr);

    // Create the heigthfield node via HAPI
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateHeightfieldInputNode(
        FHoudiniEngine::Get().GetSession(),
        ParentNodeId, NameStr.c_str(), XSize, YSize, 1.0f,
        &HeightfieldNodeId, &HeightNodeId, &MaskNodeId, &MergeNodeId ), false);

    // Cook it
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), HeightfieldNodeId, nullptr), false);

    return true;
}

bool
FHoudiniLandscapeUtils::DestroyLandscapeAssetNode( HAPI_NodeId& ConnectedAssetId, TArray<HAPI_NodeId>& CreatedInputAssetIds )
{
    HAPI_AssetInfo NodeAssetInfo;
    FHoudiniApi::AssetInfo_Init(&NodeAssetInfo);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &NodeAssetInfo ), false );

    FHoudiniEngineString AssetOpName( NodeAssetInfo.fullOpNameSH );
    FString OpName;
    if ( !AssetOpName.ToFString( OpName ) )
        return false;

    if ( !OpName.Contains(TEXT("xform")))
    {
        // Not a transform node, so not a Heightfield
        // We just need to destroy the landscape asset node
        return FHoudiniEngineUtils::DestroyHoudiniAsset( ConnectedAssetId );
    }

    // The landscape was marshalled as a heightfield, so we need to destroy and disconnect
    // the volvis nodes, all the merge node's input (each merge input is a volume for one 
    // of the layer/mask of the landscape )

    // Query the volvis node id
    // The volvis node is the fist input of the xform node
    HAPI_NodeId VolvisNodeId = -1;
    FHoudiniApi::QueryNodeInput(
        FHoudiniEngine::Get().GetSession(),
        ConnectedAssetId, 0, &VolvisNodeId );

    // First, destroy the merge node and its inputs
    // The merge node is in the first input of the volvis node
    HAPI_NodeId MergeNodeId = -1;
    FHoudiniApi::QueryNodeInput(
            FHoudiniEngine::Get().GetSession(),
            VolvisNodeId, 0, &MergeNodeId );

    if ( MergeNodeId != -1 )
    {
        // Get the merge node info
        HAPI_NodeInfo NodeInfo;
        FHoudiniApi::NodeInfo_Init(&NodeInfo);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), MergeNodeId, &NodeInfo ), false );

        for ( int32 n = 0; n < NodeInfo.inputCount; n++ )
        {
            // Get the Input node ID from the host ID
            HAPI_NodeId InputNodeId = -1;
            if ( HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
                FHoudiniEngine::Get().GetSession(),
                MergeNodeId, n, &InputNodeId ) )
                break;

            if ( InputNodeId == -1 )
                break;

            // Disconnect and Destroy that input
            FHoudiniEngineUtils::HapiDisconnectAsset( MergeNodeId, n );
            FHoudiniEngineUtils::DestroyHoudiniAsset( InputNodeId );
        }
    }

    // Second step, destroy all the volumes GEO assets
    for ( HAPI_NodeId AssetNodeId : CreatedInputAssetIds )
    {
        FHoudiniEngineUtils::DestroyHoudiniAsset( AssetNodeId );
    }
    CreatedInputAssetIds.Empty();

    // Finally disconnect and destroy the xform, volvis and merge nodes, then destroy them
    FHoudiniEngineUtils::HapiDisconnectAsset( ConnectedAssetId, 0 );
    FHoudiniEngineUtils::HapiDisconnectAsset( VolvisNodeId, 0 );
    FHoudiniEngineUtils::DestroyHoudiniAsset( MergeNodeId );
    FHoudiniEngineUtils::DestroyHoudiniAsset( VolvisNodeId );

    return FHoudiniEngineUtils::DestroyHoudiniAsset( ConnectedAssetId );
}

FColor
FHoudiniLandscapeUtils::PickVertexColorFromTextureMip(
    const uint8 * MipBytes, FVector2D & UVCoord, int32 MipWidth, int32 MipHeight )
{
    check( MipBytes );

    FColor ResultColor( 0, 0, 0, 255 );

    if ( UVCoord.X >= 0.0f && UVCoord.X < 1.0f && UVCoord.Y >= 0.0f && UVCoord.Y < 1.0f )
    {
        const int32 X = MipWidth * UVCoord.X;
        const int32 Y = MipHeight * UVCoord.Y;

        const int32 Index = ( ( Y * MipWidth ) + X ) * 4;

        ResultColor.B = MipBytes[ Index + 0 ];
        ResultColor.G = MipBytes[ Index + 1 ];
        ResultColor.R = MipBytes[ Index + 2 ];
        ResultColor.A = MipBytes[ Index + 3 ];
    }

    return ResultColor;
}

#if WITH_EDITOR
bool 
FHoudiniLandscapeUtils::ExtractLandscapeData(
    ALandscapeProxy * LandscapeProxy, TSet<ULandscapeComponent *>& SelectedComponents,
    const bool& bExportLighting, const bool& bExportTileUVs, const bool& bExportNormalizedUVs,
    TArray<FVector>& LandscapePositionArray,
    TArray<FVector>& LandscapeNormalArray,
    TArray<FVector>& LandscapeUVArray, 
    TArray<FIntPoint>& LandscapeComponentVertexIndicesArray, 
    TArray<const char *>& LandscapeComponentNameArray,
    TArray<FLinearColor>& LandscapeLightmapValues )
{
    if ( !LandscapeProxy )
        return false;

    if ( SelectedComponents.Num() < 1 )
        return false;

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    // Calc all the needed sizes
    int32 ComponentSizeQuads = ( ( LandscapeProxy->ComponentSizeQuads + 1 ) >> LandscapeProxy->ExportLOD ) - 1;
    float ScaleFactor = (float)LandscapeProxy->ComponentSizeQuads / (float)ComponentSizeQuads;

    int32 NumComponents = SelectedComponents.Num();
    bool bExportOnlySelected = NumComponents != LandscapeProxy->LandscapeComponents.Num();

    int32 VertexCountPerComponent = FMath::Square( ComponentSizeQuads + 1 );
    int32 VertexCount = NumComponents * VertexCountPerComponent;
    if ( !VertexCount )
        return false;

    // Initialize the data arrays    
    LandscapePositionArray.SetNumUninitialized( VertexCount );
    LandscapeNormalArray.SetNumUninitialized( VertexCount );
    LandscapeUVArray.SetNumUninitialized( VertexCount );
    LandscapeComponentNameArray.SetNumUninitialized( VertexCount );
    LandscapeComponentVertexIndicesArray.SetNumUninitialized( VertexCount );    
    if ( bExportLighting )
        LandscapeLightmapValues.SetNumUninitialized( VertexCount );

    //-----------------------------------------------------------------------------------------------------------------
    // EXTRACT THE LANDSCAPE DATA
    //-----------------------------------------------------------------------------------------------------------------
    FIntPoint IntPointMax = FIntPoint::ZeroValue;

    int32 AllPositionsIdx = 0;
    for ( int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++ )
    {
        ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];
        if ( bExportOnlySelected && !SelectedComponents.Contains( LandscapeComponent ) )
            continue;

        TArray64< uint8 > LightmapMipData;
        int32 LightmapMipSizeX = 0;
        int32 LightmapMipSizeY = 0;

        // See if we need to export lighting information.
        if ( bExportLighting )
        {
            const FMeshMapBuildData* MapBuildData = LandscapeComponent->GetMeshMapBuildData();
            FLightMap2D* LightMap2D = MapBuildData && MapBuildData->LightMap ? MapBuildData->LightMap->GetLightMap2D() : nullptr;
            if ( LightMap2D && LightMap2D->IsValid( 0 ) )
            {
                UTexture2D * TextureLightmap = LightMap2D->GetTexture( 0 );
                if ( TextureLightmap )
                {
                    if ( TextureLightmap->Source.GetMipData( LightmapMipData, 0 ) )
                    {
                        LightmapMipSizeX = TextureLightmap->Source.GetSizeX();
                        LightmapMipSizeY = TextureLightmap->Source.GetSizeY();
                    }
                    else
                    {
                        LightmapMipData.Empty();
                    }
                }
            }
        }

        // Construct landscape component data interface to access raw data.
        FLandscapeComponentDataInterface CDI( LandscapeComponent, LandscapeProxy->ExportLOD );

        // Get name of this landscape component.
        char * LandscapeComponentNameStr = FHoudiniEngineUtils::ExtractRawName( LandscapeComponent->GetName() );

        for ( int32 VertexIdx = 0; VertexIdx < VertexCountPerComponent; VertexIdx++ )
        {
            int32 VertX = 0;
            int32 VertY = 0;
            CDI.VertexIndexToXY( VertexIdx, VertX, VertY );

            // Get position.
            FVector PositionVector = CDI.GetWorldVertex( VertX, VertY );

            // Get normal / tangent / binormal.
            FVector Normal = FVector::ZeroVector;
            FVector TangentX = FVector::ZeroVector;
            FVector TangentY = FVector::ZeroVector;
            CDI.GetLocalTangentVectors( VertX, VertY, TangentX, TangentY, Normal );

            // Export UVs.
            FVector TextureUV = FVector::ZeroVector;
            if ( bExportTileUVs )
            {
                // We want to export uvs per tile.
                TextureUV = FVector( VertX, VertY, 0.0f );

                // If we need to normalize UV space.
                if ( bExportNormalizedUVs )
                    TextureUV /= ComponentSizeQuads;
            }
            else
            {
                // We want to export global uvs (default).
                FIntPoint IntPoint = LandscapeComponent->GetSectionBase();
                TextureUV = FVector( VertX * ScaleFactor + IntPoint.X, VertY * ScaleFactor + IntPoint.Y, 0.0f );

                // Keep track of max offset.
                IntPointMax = IntPointMax.ComponentMax( IntPoint );
            }

            if ( bExportLighting )
            {
                FLinearColor VertexLightmapColor( 0.0f, 0.0f, 0.0f, 1.0f );
                if ( LightmapMipData.Num() > 0 )
                {
                    FVector2D UVCoord( VertX, VertY );
                    UVCoord /= ( ComponentSizeQuads + 1 );

                    FColor LightmapColorRaw = PickVertexColorFromTextureMip(
                        LightmapMipData.GetData(), UVCoord, LightmapMipSizeX, LightmapMipSizeY );

                    VertexLightmapColor = LightmapColorRaw.ReinterpretAsLinear();
                }

                LandscapeLightmapValues[ AllPositionsIdx ] = VertexLightmapColor;
            }

            // Retrieve component transform.
            const FTransform & ComponentTransform = LandscapeComponent->GetComponentTransform();

            // Retrieve component scale.
            const FVector & ScaleVector = ComponentTransform.GetScale3D();

            // Perform normalization.
            Normal /= ScaleVector;
            Normal.Normalize();

            TangentX /= ScaleVector;
            TangentX.Normalize();

            TangentY /= ScaleVector;
            TangentY.Normalize();

            // Perform position scaling.
            FVector PositionTransformed = PositionVector / GeneratedGeometryScaleFactor;
            if ( ImportAxis == HRSAI_Unreal )
            {
                LandscapePositionArray[ AllPositionsIdx ].X = PositionTransformed.X;
                LandscapePositionArray[ AllPositionsIdx ].Y = PositionTransformed.Z;
                LandscapePositionArray[ AllPositionsIdx ].Z = PositionTransformed.Y;

                Swap( Normal.Y, Normal.Z );
            }
            else if ( ImportAxis == HRSAI_Houdini )
            {
                LandscapePositionArray[ AllPositionsIdx ].X = PositionTransformed.X;
                LandscapePositionArray[ AllPositionsIdx ].Y = PositionTransformed.Y;
                LandscapePositionArray[ AllPositionsIdx ].Z = PositionTransformed.Z;
            }
            else
            {
                // Not a valid enum value.
                check( 0 );
            }

            // Store landscape component name for this point.
            LandscapeComponentNameArray[ AllPositionsIdx ] = LandscapeComponentNameStr;

            // Store vertex index (x,y) for this point.
            LandscapeComponentVertexIndicesArray[ AllPositionsIdx ].X = VertX;
            LandscapeComponentVertexIndicesArray[ AllPositionsIdx ].Y = VertY;

            // Store point normal.
            LandscapeNormalArray[ AllPositionsIdx ] = Normal;

            // Store uv.
            LandscapeUVArray[ AllPositionsIdx ] = TextureUV;

            AllPositionsIdx++;
        }
    }

    // If we need to normalize UV space and we are doing global UVs.
    if ( !bExportTileUVs && bExportNormalizedUVs )
    {
        IntPointMax += FIntPoint( ComponentSizeQuads, ComponentSizeQuads );
        IntPointMax = IntPointMax.ComponentMax( FIntPoint( 1, 1 ) );

        for ( int32 UVIdx = 0; UVIdx < VertexCount; ++UVIdx )
        {
            FVector & PositionUV = LandscapeUVArray[ UVIdx ];
            PositionUV.X /= IntPointMax.X;
            PositionUV.Y /= IntPointMax.Y;
        }
    }

    return true;
}
#endif

bool FHoudiniLandscapeUtils::AddLandscapePositionAttribute( const HAPI_NodeId& NodeId, const TArray< FVector >& LandscapePositionArray )
{
    int32 VertexCount = LandscapePositionArray.Num();
    if ( VertexCount < 3 )
        return false;

    // Create point attribute info containing positions.    
    HAPI_AttributeInfo AttributeInfoPointPosition;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointPosition);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointPosition );
    AttributeInfoPointPosition.count = VertexCount;
    AttributeInfoPointPosition.tupleSize = 3;
    AttributeInfoPointPosition.exists = true;
    AttributeInfoPointPosition.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPointPosition.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoPointPosition.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId, 0,
        HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointPosition ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(), NodeId, 0,
        HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointPosition,
        (const float *)LandscapePositionArray.GetData(),
        0, AttributeInfoPointPosition.count ), false );

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeNormalAttribute( const HAPI_NodeId& NodeId, const TArray<FVector>& LandscapeNormalArray )
{
    int32 VertexCount = LandscapeNormalArray.Num();
    if ( VertexCount < 3 )
        return false;

    HAPI_AttributeInfo AttributeInfoPointNormal;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointNormal);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointNormal );
    AttributeInfoPointNormal.count = VertexCount;
    AttributeInfoPointNormal.tupleSize = 3;
    AttributeInfoPointNormal.exists = true;
    AttributeInfoPointNormal.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPointNormal.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoPointNormal.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId,
        0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal,
        (const float *)LandscapeNormalArray.GetData(), 0, AttributeInfoPointNormal.count ), false );

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeUVAttribute( const HAPI_NodeId& NodeId, const TArray<FVector>& LandscapeUVArray )
{
    int32 VertexCount = LandscapeUVArray.Num();
    if ( VertexCount < 3 )
        return false;

    HAPI_AttributeInfo AttributeInfoPointUV;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointUV);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointUV );
    AttributeInfoPointUV.count = VertexCount;
    AttributeInfoPointUV.tupleSize = 3;
    AttributeInfoPointUV.exists = true;
    AttributeInfoPointUV.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPointUV.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoPointUV.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId,
        0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, 0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV,
        (const float *)LandscapeUVArray.GetData(), 0, AttributeInfoPointUV.count ), false );

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeComponentVertexIndicesAttribute( const HAPI_NodeId& NodeId, const TArray<FIntPoint>& LandscapeComponentVertexIndicesArray )
{
    int32 VertexCount = LandscapeComponentVertexIndicesArray.Num();
    if ( VertexCount < 3 )
        return false;

    HAPI_AttributeInfo AttributeInfoPointLandscapeComponentVertexIndices;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLandscapeComponentVertexIndices);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentVertexIndices );
    AttributeInfoPointLandscapeComponentVertexIndices.count = VertexCount;
    AttributeInfoPointLandscapeComponentVertexIndices.tupleSize = 2;
    AttributeInfoPointLandscapeComponentVertexIndices.exists = true;
    AttributeInfoPointLandscapeComponentVertexIndices.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPointLandscapeComponentVertexIndices.storage = HAPI_STORAGETYPE_INT;
    AttributeInfoPointLandscapeComponentVertexIndices.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId,
        0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
        &AttributeInfoPointLandscapeComponentVertexIndices ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
        &AttributeInfoPointLandscapeComponentVertexIndices,
        (const int * )LandscapeComponentVertexIndicesArray.GetData(), 0,
        AttributeInfoPointLandscapeComponentVertexIndices.count ), false );

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeComponentNameAttribute( const HAPI_NodeId& NodeId, const TArray<const char *>& LandscapeComponentNameArray )
{
    int32 VertexCount = LandscapeComponentNameArray.Num();
    if ( VertexCount < 3 )
        return false;

    // Create point attribute containing landscape component name.
    HAPI_AttributeInfo AttributeInfoPointLandscapeComponentNames;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLandscapeComponentNames);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentNames );
    AttributeInfoPointLandscapeComponentNames.count = VertexCount;
    AttributeInfoPointLandscapeComponentNames.tupleSize = 1;
    AttributeInfoPointLandscapeComponentNames.exists = true;
    AttributeInfoPointLandscapeComponentNames.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPointLandscapeComponentNames.storage = HAPI_STORAGETYPE_STRING;
    AttributeInfoPointLandscapeComponentNames.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId, 0,
        HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
        &AttributeInfoPointLandscapeComponentNames), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
        FHoudiniEngine::Get().GetSession(), NodeId, 0,
        HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
        &AttributeInfoPointLandscapeComponentNames,
        (const char **)LandscapeComponentNameArray.GetData(),
        0, AttributeInfoPointLandscapeComponentNames.count ), false );

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeLightmapColorAttribute( const HAPI_NodeId& NodeId, const TArray<FLinearColor>& LandscapeLightmapValues )
{
    int32 VertexCount = LandscapeLightmapValues.Num();

    HAPI_AttributeInfo AttributeInfoPointLightmapColor;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLightmapColor);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLightmapColor );
    AttributeInfoPointLightmapColor.count = VertexCount;
    AttributeInfoPointLightmapColor.tupleSize = 4;
    AttributeInfoPointLightmapColor.exists = true;
    AttributeInfoPointLightmapColor.owner = HAPI_ATTROWNER_POINT;
    AttributeInfoPointLightmapColor.storage = HAPI_STORAGETYPE_FLOAT;
    AttributeInfoPointLightmapColor.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId,
        0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor,
        (const float *)LandscapeLightmapValues.GetData(), 0,
        AttributeInfoPointLightmapColor.count ), false );

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeMeshIndicesAndMaterialsAttribute( 
    const HAPI_NodeId& NodeId, const bool& bExportMaterials,
    const int32& ComponentSizeQuads, const int32& QuadCount,
    ALandscapeProxy * LandscapeProxy,
    const TSet< ULandscapeComponent * >& SelectedComponents )
{
    if ( !LandscapeProxy )
        return false;

    // Compute number of necessary indices.
    int32 IndexCount = QuadCount * 4;
    if ( IndexCount < 0 )
        return false;

    int32 VertexCountPerComponent = FMath::Square( ComponentSizeQuads + 1 );

    // Get runtime settings.
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings )
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;

    // Array holding indices data.
    TArray< int32 > LandscapeIndices;
    LandscapeIndices.SetNumUninitialized( IndexCount );

    // Allocate space for face names.
    // The LandscapeMaterial and HoleMaterial per point
    TArray< const char * > FaceMaterials;
    TArray< const char * > FaceHoleMaterials;
    FaceMaterials.SetNumUninitialized( QuadCount );
    FaceHoleMaterials.SetNumUninitialized( QuadCount );

    int32 VertIdx = 0;
    int32 QuadIdx = 0;

    char * MaterialRawStr = nullptr;
    char * MaterialHoleRawStr = nullptr;

    const int32 QuadComponentCount = ComponentSizeQuads + 1;
    for ( int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++ )
    {
        ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];
        if ( !SelectedComponents.Contains( LandscapeComponent ) )
            continue;

        if ( bExportMaterials )
        {
            // If component has an override material, we need to get the raw name (if exporting materials).
            if ( LandscapeComponent->OverrideMaterial )
            {
                MaterialRawStr = FHoudiniEngineUtils::ExtractRawName(LandscapeComponent->OverrideMaterial->GetName());
            }

            // If component has an override hole material, we need to get the raw name (if exporting materials).
            if ( LandscapeComponent->OverrideHoleMaterial )
            {
                MaterialHoleRawStr = FHoudiniEngineUtils::ExtractRawName(LandscapeComponent->OverrideHoleMaterial->GetName());
            }
        }

        int32 BaseVertIndex = ComponentIdx * VertexCountPerComponent;
        for ( int32 YIdx = 0; YIdx < ComponentSizeQuads; YIdx++ )
        {
            for ( int32 XIdx = 0; XIdx < ComponentSizeQuads; XIdx++ )
            {
                if ( ImportAxis == HRSAI_Unreal )
                {
                    LandscapeIndices[ VertIdx + 0 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 0 ) * QuadComponentCount;
                    LandscapeIndices[ VertIdx + 1 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 0 ) * QuadComponentCount;
                    LandscapeIndices[ VertIdx + 2 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 1 ) * QuadComponentCount;
                    LandscapeIndices[ VertIdx + 3 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 1 ) * QuadComponentCount;
                }
                else if (ImportAxis == HRSAI_Houdini)
                {
                    LandscapeIndices[ VertIdx + 0 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 0 ) * QuadComponentCount;
                    LandscapeIndices[ VertIdx + 1 ] = BaseVertIndex + ( XIdx + 0 ) + ( YIdx + 1 ) * QuadComponentCount;
                    LandscapeIndices[ VertIdx + 2 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 1 ) * QuadComponentCount;
                    LandscapeIndices[ VertIdx + 3 ] = BaseVertIndex + ( XIdx + 1 ) + ( YIdx + 0 ) * QuadComponentCount;
                }
                else
                {
                   // Not a valid enum value.
                    check( 0 );
                }

                // Store override materials (if exporting materials).
                if ( bExportMaterials )
                {
                    FaceMaterials[ QuadIdx ] = MaterialRawStr;
                    FaceHoleMaterials[ QuadIdx ] = MaterialHoleRawStr;
                }

                VertIdx += 4;
                QuadIdx++;
            }
        }
    }

    // We can now set vertex list.
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetVertexList(
        FHoudiniEngine::Get().GetSession(), NodeId,
        0, LandscapeIndices.GetData(), 0, LandscapeIndices.Num() ), false );

    // We need to generate array of face counts.
    TArray< int32 > LandscapeFaces;
    LandscapeFaces.Init( 4, QuadCount );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetFaceCounts(
        FHoudiniEngine::Get().GetSession(), NodeId,
        0, LandscapeFaces.GetData(), 0, LandscapeFaces.Num() ), false );

    if ( bExportMaterials )
    {
        if ( !FaceMaterials.Contains( nullptr ) )
        {
            // Get name of attribute used for marshalling materials.
            std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL;
            if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
            {
                FHoudiniEngineUtils::ConvertUnrealString(
                    HoudiniRuntimeSettings->MarshallingAttributeMaterial,
                    MarshallingAttributeMaterialName );
            }

            // Marshall in override primitive material names.
            HAPI_AttributeInfo AttributeInfoPrimitiveMaterial;
            FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrimitiveMaterial);
            //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterial );
            AttributeInfoPrimitiveMaterial.count = FaceMaterials.Num();
            AttributeInfoPrimitiveMaterial.tupleSize = 1;
            AttributeInfoPrimitiveMaterial.exists = true;
            AttributeInfoPrimitiveMaterial.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfoPrimitiveMaterial.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoPrimitiveMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(), NodeId, 0,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoPrimitiveMaterial ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(), NodeId, 0,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoPrimitiveMaterial,
                (const char **)FaceMaterials.GetData(), 0, AttributeInfoPrimitiveMaterial.count ), false );
        }

        if ( !FaceHoleMaterials.Contains( nullptr ) )
        {
            // Get name of attribute used for marshalling hole materials.
            std::string MarshallingAttributeMaterialHoleName = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;
            if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterialHole.IsEmpty() )
            {
                FHoudiniEngineUtils::ConvertUnrealString(
                    HoudiniRuntimeSettings->MarshallingAttributeMaterialHole,
                    MarshallingAttributeMaterialHoleName );
            }

            // Marshall in override primitive material hole names.
            HAPI_AttributeInfo AttributeInfoPrimitiveMaterialHole;
            FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrimitiveMaterialHole);
            //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterialHole );
            AttributeInfoPrimitiveMaterialHole.count = FaceHoleMaterials.Num();
            AttributeInfoPrimitiveMaterialHole.tupleSize = 1;
            AttributeInfoPrimitiveMaterialHole.exists = true;
            AttributeInfoPrimitiveMaterialHole.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfoPrimitiveMaterialHole.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoPrimitiveMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                FHoudiniEngine::Get().GetSession(),
                NodeId, 0, MarshallingAttributeMaterialHoleName.c_str(),
                &AttributeInfoPrimitiveMaterialHole ), false );

            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(),
                NodeId, 0, MarshallingAttributeMaterialHoleName.c_str(),
                &AttributeInfoPrimitiveMaterialHole, (const char **) FaceHoleMaterials.GetData(), 0,
                AttributeInfoPrimitiveMaterialHole.count ), false );
        }
    }

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeTileAttribute(
    const HAPI_NodeId& NodeId, const HAPI_PartId& PartId, const int32& TileIdx )
{
    HAPI_AttributeInfo AttributeInfoTileIndex;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoTileIndex);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoTileIndex );
    AttributeInfoTileIndex.count = 1;
    AttributeInfoTileIndex.tupleSize = 1;
    AttributeInfoTileIndex.exists = true;
    AttributeInfoTileIndex.owner = HAPI_ATTROWNER_PRIM;
    AttributeInfoTileIndex.storage = HAPI_STORAGETYPE_INT;
    AttributeInfoTileIndex.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId,
        PartId, "tile", &AttributeInfoTileIndex ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "tile", &AttributeInfoTileIndex,
        (const int *)&TileIdx, 0, AttributeInfoTileIndex.count ), false );

    return true;
}

// Add the landscape component extent attribute
bool FHoudiniLandscapeUtils::AddLandscapeComponentExtentAttributes(
    const HAPI_NodeId& NodeId, const HAPI_PartId& PartId,
    const int32& MinX, const int32& MaxX,
    const int32& MinY, const int32& MaxY )
{
    // Create an AttributeInfo
    HAPI_AttributeInfo AttributeInfo;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
    //FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfo);
    AttributeInfo.count = 1;
    AttributeInfo.tupleSize = 1;
    AttributeInfo.exists = true;
    AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
    AttributeInfo.storage = HAPI_STORAGETYPE_INT;
    AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

    // Add the landscape_component_min_X primitive attribute
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(),
        NodeId,    PartId,    "landscape_component_min_X", &AttributeInfo ), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_min_X", &AttributeInfo,
        (const int *)&MinX, 0, AttributeInfo.count), false);

    // Add the landscape_component_max_X primitive attribute
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_max_X", &AttributeInfo), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_max_X", &AttributeInfo,
        (const int *)&MaxX, 0, AttributeInfo.count), false);

    // Add the landscape_component_min_Y primitive attribute
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_min_Y", &AttributeInfo), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_min_Y", &AttributeInfo,
        (const int *)&MinY, 0, AttributeInfo.count), false);

    // Add the landscape_component_max_Y primitive attribute
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_max_Y", &AttributeInfo), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, PartId, "landscape_component_max_Y", &AttributeInfo,
        (const int *)&MaxY, 0, AttributeInfo.count), false);

    return true;
}

// Read the landscape component extent attribute from a heightfield
bool FHoudiniLandscapeUtils::GetLandscapeComponentExtentAttributes(
    const FHoudiniGeoPartObject& HoudiniGeoPartObject,
    int32& MinX, int32& MaxX,
    int32& MinY, int32& MaxY )
{
    // If we dont have minX, we likely dont have the others too
    if ( !FHoudiniEngineUtils::HapiCheckAttributeExists(
        HoudiniGeoPartObject, "landscape_component_min_X", HAPI_ATTROWNER_PRIM) )
        return false;

    // Create an AttributeInfo
    HAPI_AttributeInfo AttributeInfo;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
    //FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfo);

    // Get MinX
    TArray<int32> IntData;
    if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        HoudiniGeoPartObject, "landscape_component_min_X", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM ) )
        return false;

    if ( IntData.Num() > 0 )
        MinX = IntData[0];

    // Get MaxX
    if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        HoudiniGeoPartObject, "landscape_component_max_X", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
        return false;

    if (IntData.Num() > 0)
        MaxX = IntData[0];

    // Get MinY
    if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        HoudiniGeoPartObject, "landscape_component_min_Y", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
        return false;

    if (IntData.Num() > 0)
        MinY = IntData[0];

    // Get MaxX
    if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        HoudiniGeoPartObject, "landscape_component_max_Y", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
        return false;

    if (IntData.Num() > 0)
        MaxY = IntData[0];

    return true;
}

#if WITH_EDITOR
bool
FHoudiniLandscapeUtils::CreateAllLandscapes( 
    FHoudiniCookParams& HoudiniCookParams,
    const TArray< FHoudiniGeoPartObject > & FoundVolumes, 
    TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >& Landscapes,
    TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >& NewLandscapes,
    TArray<ALandscapeProxy *>& InputLandscapeToUpdate,
    float ForcedZMin , float ForcedZMax )
{
    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesForceMinMaxValues )
    {
        ForcedZMin = HoudiniRuntimeSettings->MarshallingLandscapesForcedMinValue;
        ForcedZMax = HoudiniRuntimeSettings->MarshallingLandscapesForcedMaxValue;
    }

    // First, we need to extract proper height data from FoundVolumes
    TArray< const FHoudiniGeoPartObject* > FoundHeightfields;
    FHoudiniLandscapeUtils::GetHeightfieldsInArray( FoundVolumes, FoundHeightfields );

    // If we have multiple heightfields, we want to convert them using the same Z range
    // Either that range has been specified/forced by the user, or we'll have to calculate it from all the height volumes.
    float fGlobalMin = ForcedZMin, fGlobalMax = ForcedZMax;
    if ( FoundHeightfields.Num() > 1 && ( fGlobalMin == 0.0f && fGlobalMax == 0.0f ) )
        FHoudiniLandscapeUtils::CalcHeightfieldsArrayGlobalZMinZMax( FoundHeightfields, fGlobalMin, fGlobalMax );

    // We want to be doing a similar things for the mask/layer volumes
    TMap<FString, float> GlobalMinimums;
    TMap<FString, float> GlobalMaximums;
    if ( FoundVolumes.Num() > 1 )
        FHoudiniLandscapeUtils::CalcHeightfieldsArrayGlobalZMinZMax( FoundVolumes, GlobalMinimums, GlobalMaximums );

    // Try to create a Landscape for each HeightData found
    NewLandscapes.Empty();
    for (TArray< const FHoudiniGeoPartObject* >::TConstIterator IterHeighfields(FoundHeightfields); IterHeighfields; ++IterHeighfields)
    {
        // Get the current Heightfield GeoPartObject
        const FHoudiniGeoPartObject* CurrentHeightfield = *IterHeighfields;
        if (!CurrentHeightfield)
            continue;

        // Look for the unreal_landscape_streaming_proxy attribute on the height volume
        bool bCreateLandscapeStreamingProxy = false;
        TArray<int32> IntData;
        HAPI_AttributeInfo AttributeInfo;
        FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
        //FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfo);
        if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
            CurrentHeightfield->AssetId, CurrentHeightfield->ObjectId,
            CurrentHeightfield->GeoId, CurrentHeightfield->PartId,
            "unreal_landscape_streaming_proxy", AttributeInfo, IntData, 1))
        {
            if (IntData.Num() > 0 && IntData[0] != 0)
                bCreateLandscapeStreamingProxy = true;
        }

        // Try to find the landscape previously created from that HGPO
        ALandscapeProxy * FoundLandscape = nullptr;
        if (TWeakObjectPtr<ALandscapeProxy>* FoundLandscapePtr = Landscapes.Find(*CurrentHeightfield))
        {
            FoundLandscape = FoundLandscapePtr->Get();
            if (!FoundLandscape || !FoundLandscape->IsValidLowLevel())
                FoundLandscape = nullptr;
        }

        bool bLandscapeNeedsToBeUpdated = true;
        if (!CurrentHeightfield->bHasGeoChanged)
        {
            // The Geo has not changed, do we need to recreate the landscape?
            if (FoundLandscape)
            {
                // Check that all layers/mask have not changed too
                TArray< const FHoudiniGeoPartObject* > FoundLayers;
                FHoudiniLandscapeUtils::GetHeightfieldsLayersInArray(FoundVolumes, *CurrentHeightfield, FoundLayers);

                bool bLayersHaveChanged = false;
                for (int32 n = 0; n < FoundLayers.Num(); n++)
                {
                    if (FoundLayers[n] && FoundLayers[n]->bHasGeoChanged)
                    {
                        bLayersHaveChanged = true;
                        break;
                    }
                }

                if (!bLayersHaveChanged)
                {
                    // Height and layers/masks have not changed, there is no need to reimport the landscape
                    bLandscapeNeedsToBeUpdated = false;
                }

                // Force update/recreation if the actor is not of the desired type
                if ( (!FoundLandscape->IsA(ALandscapeStreamingProxy::StaticClass()) && bCreateLandscapeStreamingProxy)
                    || (FoundLandscape->IsA(ALandscapeStreamingProxy::StaticClass()) && !bCreateLandscapeStreamingProxy) )
                    bLandscapeNeedsToBeUpdated = true;
            }
        }

        // Height and mask volumes have not changed
        // No need to update the Landscape
        if (!bLandscapeNeedsToBeUpdated)
        {
            // We can add the landscape to the map and remove it from the old one to avoid its destruction
            NewLandscapes.Add(*CurrentHeightfield, FoundLandscape);
            Landscapes.Remove(*CurrentHeightfield);
            continue;
        }
            
        HAPI_NodeId HeightFieldNodeId = CurrentHeightfield->HapiGeoGetNodeId();

        // We need to see if the current heightfield has an unreal_material or unreal_hole_material assigned to it
        UMaterialInterface* LandscapeMaterial = nullptr;
        UMaterialInterface* LandscapeHoleMaterial = nullptr;
        FHoudiniLandscapeUtils::GetHeightFieldLandscapeMaterials(*CurrentHeightfield, LandscapeMaterial, LandscapeHoleMaterial);

        // Extract the Float Data from the Heightfield
        TArray< float > FloatValues;
        HAPI_VolumeInfo VolumeInfo;
        float FloatMin, FloatMax;
        if (!FHoudiniLandscapeUtils::GetHeightfieldData(*CurrentHeightfield, FloatValues, VolumeInfo, FloatMin, FloatMax))
            continue;

        // Do we need to convert the heightfields using the same global Min/Max
        if (fGlobalMin != fGlobalMax)
        {
            FloatMin = fGlobalMin;
            FloatMax = fGlobalMax;
        }

        // See if we need to create a new Landscape Actor for this heightfield:
        // Either we haven't created one yet, or it's size has changed
        int32 HoudiniXSize = VolumeInfo.yLength;
        int32 HoudiniYSize = VolumeInfo.xLength;
        int32 UnrealXSize = -1;
        int32 UnrealYSize = -1;
        int32 NumSectionPerLandscapeComponent = -1;
        int32 NumQuadsPerLandscapeSection = -1;
        if (!FHoudiniLandscapeUtils::CalcLandscapeSizeFromHeightfieldSize(
            HoudiniXSize, HoudiniYSize,
            UnrealXSize, UnrealYSize,
            NumSectionPerLandscapeComponent,
            NumQuadsPerLandscapeSection))
            continue;

        // See if the Heightfield has component extent attributes
        // This would mean that we'd need to update parts of the landscape only 
        bool UpdateLandscapeComponent = false;
        int32 MinX, MaxX, MinY, MaxY;
        bool HasComponentExtent = GetLandscapeComponentExtentAttributes(
            *CurrentHeightfield, MinX, MaxX, MinY, MaxY);

        // Try to see if we have an input landscape that matches the size of the current HGPO
        bool UpdatingInputLandscape = false;
        for (int nIdx = 0; nIdx < InputLandscapeToUpdate.Num(); nIdx++)
        {
            ALandscapeProxy* CurrentInputLandscape = InputLandscapeToUpdate[nIdx];
            if (!CurrentInputLandscape)
                continue;

            ULandscapeInfo* CurrentInfo = CurrentInputLandscape->GetLandscapeInfo();
            if (!CurrentInfo)
                continue;

            int32 InputMinX = 0;
            int32 InputMinY = 0;
            int32 InputMaxX = 0;
            int32 InputMaxY = 0;
            CurrentInfo->GetLandscapeExtent(InputMinX, InputMinY, InputMaxX, InputMaxY);

            // If the full size matches, we'll update that input landscape
            bool SizeMatch = false;
            if ((InputMaxX - InputMinX + 1) == UnrealXSize && (InputMaxY - InputMinY + 1) == UnrealYSize)
                SizeMatch = true;

            /*
            // If not, see if we could update that landscape's components instead of the full landscape
            if ( !SizeMatch && HasComponentExtent )
            {
                if ( (MinX >= InputMinX) && (MinX <= InputMaxX)
                    && (MinY >= InputMinY) && (MinY <= InputMaxY)
                    && (MaxX >= InputMinX) && (MaxX <= InputMaxX)
                    && (MaxY >= InputMinY) && (MaxY <= InputMaxY) )
                {
                    // Try to update this landscape component data
                    SizeMatch = true;
                    UpdateLandscapeComponent = true;
                }
            }
            */

            // HF and landscape don't match, try another one
            if ( !SizeMatch )
                continue;

            // Replace FoundLandscape by that input landscape
            FoundLandscape = CurrentInputLandscape;

            // If we're not updating part of that landscape via components, 
            // Remove that landscape from the input array so we dont try to update it twice
            if ( !UpdateLandscapeComponent )
                InputLandscapeToUpdate.RemoveAt(nIdx);

            UpdatingInputLandscape = true;
        }

        // See if we need to create a new Landscape Actor for this heightfield
        // Either we haven't created one yet, or it's size has changed
        bool bLandscapeNeedsRecreate = true;
        if ( UpdatingInputLandscape )
        {
            // No need to create a new landscape as we're modifying the input one
            bLandscapeNeedsRecreate = false;
        }
        else if ( FoundLandscape && !FoundLandscape->IsPendingKill() )
        {
            // See if we can reuse the previous landscape
            ULandscapeInfo* PreviousInfo = FoundLandscape->GetLandscapeInfo();
            if ( PreviousInfo )
            {
                int32 PrevMinX = 0;
                int32 PrevMinY = 0;
                int32 PrevMaxX = 0;
                int32 PrevMaxY = 0;
                PreviousInfo->GetLandscapeExtent(PrevMinX, PrevMinY, PrevMaxX, PrevMaxY);

                bool SizeMatch = false;
                if ( (PrevMaxX - PrevMinX + 1) == UnrealXSize
                    && (PrevMaxY - PrevMinY + 1) == UnrealYSize )
                    SizeMatch = true;

                /*
                // If not, see if we could update that landscape's component
                if (!SizeMatch && HasComponentExtent)
                {
                    if ((MinX >= PrevMinX) && (MinX <= PrevMaxX)
                        && (MinY >= PrevMinY) && (MinY <= PrevMaxY)
                        && (MaxX >= PrevMinX) && (MaxX <= PrevMaxX)
                        && (MaxY >= PrevMinY) && (MaxY <= PrevMaxY))
                    {
                        // Try to update this landscape component data
                        SizeMatch = true;
                        UpdateLandscapeComponent = true;
                    }
                }
                */
                if ( SizeMatch )
                {
                    // We can reuse the existing actor
                    bLandscapeNeedsRecreate = false;
                }
            }
        }

        if (!bLandscapeNeedsRecreate)
        {
            // Force update/recreation if the actor is not of the desired type
            if ((!FoundLandscape->IsA(ALandscapeStreamingProxy::StaticClass()) && bCreateLandscapeStreamingProxy)
                || (FoundLandscape->IsA(ALandscapeStreamingProxy::StaticClass()) && !bCreateLandscapeStreamingProxy))
                bLandscapeNeedsRecreate = true;
        }

        if ( bLandscapeNeedsRecreate )
        {
            // We need to create a new Landscape Actor
            // Either we didnt create one before, or we cannot reuse the old one (size has changed)

            // Convert the height data from Houdini's heightfield to Unreal's Landscape
            TArray< uint16 > IntHeightData;
            FTransform LandscapeTransform;
            if (!FHoudiniLandscapeUtils::ConvertHeightfieldDataToLandscapeData(
                FloatValues, VolumeInfo,
                UnrealXSize, UnrealYSize,
                FloatMin, FloatMax,
                IntHeightData, LandscapeTransform))
                continue;

            // Look for all the layers/masks corresponding to the current heightfield
            TArray< const FHoudiniGeoPartObject* > FoundLayers;
            FHoudiniLandscapeUtils::GetHeightfieldsLayersInArray(FoundVolumes, *CurrentHeightfield, FoundLayers);

            // Extract and convert the Landscape layers
            TArray< FLandscapeImportLayerInfo > ImportLayerInfos;
            if (!FHoudiniLandscapeUtils::CreateLandscapeLayers(HoudiniCookParams, FoundLayers, *CurrentHeightfield,
                UnrealXSize, UnrealYSize, GlobalMinimums, GlobalMaximums, ImportLayerInfos))
                continue;

            // Create the actual Landscape
            ALandscapeProxy * CurrentLandscape = CreateLandscape(
                IntHeightData, ImportLayerInfos,
                LandscapeTransform, UnrealXSize, UnrealYSize,
                NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
                LandscapeMaterial, LandscapeHoleMaterial, bCreateLandscapeStreamingProxy );

            if (!CurrentLandscape)
                continue;

            // Update the visibility mask / layer if we have any
            for (auto CurrLayerInfo : ImportLayerInfos)
            {
                if (CurrLayerInfo.LayerInfo && CurrLayerInfo.LayerName.ToString().Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
                {
                    CurrentLandscape->VisibilityLayer = CurrLayerInfo.LayerInfo;
                    CurrentLandscape->VisibilityLayer->bNoWeightBlend = true;
                    CurrentLandscape->VisibilityLayer->AddToRoot();
                }
            }

            // Add the new landscape to the map
            NewLandscapes.Add(*CurrentHeightfield, CurrentLandscape);
        }
        else
        {
            // We're reusing an existing landscape Actor
            // Only update the height / layer data that has changed

            // Update the materials if they have changed
            if ( FoundLandscape->GetLandscapeMaterial() != LandscapeMaterial )
                FoundLandscape->LandscapeMaterial = LandscapeMaterial;

            if ( FoundLandscape->GetLandscapeHoleMaterial() != LandscapeHoleMaterial )
                FoundLandscape->LandscapeHoleMaterial = LandscapeHoleMaterial;

            // Update the previous landscape's height data
            // Decide if we need to create a new Landscape or if we can reuse the previous one
            ULandscapeInfo* PreviousInfo = FoundLandscape->GetLandscapeInfo();
            if ( !PreviousInfo )
                continue;

            FLandscapeEditDataInterface LandscapeEdit(PreviousInfo);

            // Update the height data only if it's marked as changed
            if ( CurrentHeightfield->bHasGeoChanged )
            {
                // Convert the height data from Houdini's heightfield to Unreal's Landscape
                TArray< uint16 > IntHeightData;
                FTransform LandscapeTransform;
                if ( !FHoudiniLandscapeUtils::ConvertHeightfieldDataToLandscapeData(
                    FloatValues, VolumeInfo,
                    UnrealXSize, UnrealYSize,
                    FloatMin, FloatMax,
                    IntHeightData, LandscapeTransform,
                    UpdateLandscapeComponent ) )
                    continue;

                if ( !UpdateLandscapeComponent )
                    LandscapeEdit.SetHeightData(0, 0, UnrealXSize - 1, UnrealYSize - 1, IntHeightData.GetData(), 0, true);
                else
                    LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, IntHeightData.GetData(), 0, true);

                // Set the landscape Transform
                if ( !UpdateLandscapeComponent )
                    FoundLandscape->SetActorRelativeTransform(LandscapeTransform);
            }

            // Look for all the layers/masks corresponding to the current heightfield
            TArray< const FHoudiniGeoPartObject* > FoundLayers;
            FHoudiniLandscapeUtils::GetHeightfieldsLayersInArray(FoundVolumes, *CurrentHeightfield, FoundLayers);

            // Get the names of all the non weight blended layers
            TArray< FString > NonWeightBlendedLayerNames;
            FHoudiniLandscapeUtils::GetNonWeightBlendedLayerNames(*CurrentHeightfield, NonWeightBlendedLayerNames);

            for ( auto LayerGeoPartObject : FoundLayers )
            {
                if (!LayerGeoPartObject)
                    continue;

                if (!LayerGeoPartObject->IsValid())
                    continue;

                if (LayerGeoPartObject->AssetId == -1)
                    continue;

                if ( !LayerGeoPartObject->bHasGeoChanged )
                    continue;

                // Extract the layer's float data from the HF
                TArray< float > FloatLayerData;
                HAPI_VolumeInfo LayerVolumeInfo;
                FHoudiniApi::VolumeInfo_Init(&LayerVolumeInfo);

                float LayerMin = 0;
                float LayerMax = 0;
                if (!FHoudiniLandscapeUtils::GetHeightfieldData(*LayerGeoPartObject, FloatLayerData, LayerVolumeInfo, LayerMin, LayerMax))
                    continue;

                // No need to create flat layers as Unreal will remove them afterwards..
                if (LayerMin == LayerMax)
                    continue;

                // Get the layer's name
                FString LayerString;
                FHoudiniEngineString(LayerVolumeInfo.nameSH).ToFString(LayerString);

                // Check if that landscape layer has been marked as unit (range in [0-1]
                if ( IsUnitLandscapeLayer( *LayerGeoPartObject ) )
                {
                    LayerMin = 0.0f;
                    LayerMax = 1.0f;
                }
                else
                {
                    // Do we want to convert the layer's value using the global Min/Max
                    if (GlobalMaximums.Contains(LayerString))
                        LayerMax = GlobalMaximums[LayerString];

                    if (GlobalMinimums.Contains(LayerString))
                        LayerMin = GlobalMinimums[LayerString];
                }

                // Find the ImportLayerInfo and LayerInfo objects
                ObjectTools::SanitizeObjectName(LayerString);
                FName LayerName(*LayerString);
                FLandscapeImportLayerInfo currentLayerInfo(LayerName);

                UPackage * Package = nullptr;
                currentLayerInfo.LayerInfo = FHoudiniLandscapeUtils::CreateLandscapeLayerInfoObject(HoudiniCookParams, LayerString.GetCharArray().GetData(), Package, LayerGeoPartObject->GetPartId());
                if (!currentLayerInfo.LayerInfo || !Package)
                    continue;

                // Convert the float data to uint8
                if ( !FHoudiniLandscapeUtils::ConvertHeightfieldLayerToLandscapeLayer(
                    FloatLayerData, LayerVolumeInfo.yLength, LayerVolumeInfo.xLength,
                    LayerMin, LayerMax,
                    UnrealXSize, UnrealYSize,
                    currentLayerInfo.LayerData,
                    UpdateLandscapeComponent ) )
                    continue;

                // Store the data used to convert the Houdini float values to int in the DebugColor
                // This is the only way we'll be able to reconvert those values back to their houdini equivalent afterwards...
                // R = Min, G = Max, B = Spacing, A = ?
                currentLayerInfo.LayerInfo->LayerUsageDebugColor.R = LayerMin;
                currentLayerInfo.LayerInfo->LayerUsageDebugColor.G = LayerMax;
                currentLayerInfo.LayerInfo->LayerUsageDebugColor.B = (LayerMax - LayerMin) / 255.0f;
                currentLayerInfo.LayerInfo->LayerUsageDebugColor.A = PI;

                // Updated non weight blended layers (visibility is by default non weight blended)
                if (NonWeightBlendedLayerNames.Contains(LayerString) || LayerString.Equals(TEXT("visibility"), ESearchCase::IgnoreCase))
                    currentLayerInfo.LayerInfo->bNoWeightBlend = true;
                else
                    currentLayerInfo.LayerInfo->bNoWeightBlend = false;

                // Update the layer on the heightfield
                if ( !UpdateLandscapeComponent )
                    LandscapeEdit.SetAlphaData( currentLayerInfo.LayerInfo, 0, 0, UnrealXSize - 1, UnrealYSize - 1, currentLayerInfo.LayerData.GetData(), 0 );
                else
                    LandscapeEdit.SetAlphaData( currentLayerInfo.LayerInfo, MinX, MinY, MaxX, MaxY, currentLayerInfo.LayerData.GetData(), 0 );

                if ( currentLayerInfo.LayerInfo && currentLayerInfo.LayerName.ToString().Equals( TEXT("Visibility"), ESearchCase::IgnoreCase ) )
                {
                    FoundLandscape->VisibilityLayer = currentLayerInfo.LayerInfo;
                    FoundLandscape->VisibilityLayer->bNoWeightBlend = true;
                    FoundLandscape->VisibilityLayer->AddToRoot();
                }
            }

            // Update the materials if they have changed
            if (FoundLandscape->GetLandscapeMaterial() != LandscapeMaterial)
                FoundLandscape->LandscapeMaterial = LandscapeMaterial;

            if (FoundLandscape->GetLandscapeHoleMaterial() != LandscapeHoleMaterial)
                FoundLandscape->LandscapeHoleMaterial = LandscapeHoleMaterial;

            //FoundLandscape->RecreateCollisionComponents();

            //PreviousInfo->UpdateLayerInfoMap();
            //PreviousInfo->RecreateCollisionComponents();
            //PreviousInfo->UpdateAllAddCollisions();

            // We can add the landscape to the new map
            NewLandscapes.Add(*CurrentHeightfield, FoundLandscape);
        }
    }

    // Handle the HF's tags
    for (auto Iter : NewLandscapes)
    {
        FHoudiniGeoPartObject HGPO = Iter.Key;

        // See if we have unreal_tag_ attribute
        TArray<FName> Tags;
        if (!FHoudiniEngineUtils::GetUnrealTagAttributes(HGPO, Tags))
            continue;

        TWeakObjectPtr<ALandscapeProxy> Landscape = Iter.Value;
        if (!Landscape.IsValid())
            continue;

        Landscape->Tags = Tags;
    }

    return true;
}

ALandscapeProxy *
FHoudiniLandscapeUtils::CreateLandscape(
    const TArray< uint16 >& IntHeightData,
    const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
    const FTransform& LandscapeTransform,
    const int32& XSize, const int32& YSize, 
    const int32& NumSectionPerLandscapeComponent, const int32& NumQuadsPerLandscapeSection,
    UMaterialInterface* LandscapeMaterial, UMaterialInterface* LandscapeHoleMaterial,
    const bool& CreateLandscapeStreamingProxy )
{
    if ( ( XSize < 2 ) || ( YSize < 2 ) )
        return nullptr;

    if ( IntHeightData.Num() != ( XSize * YSize ) )
        return nullptr;

    if ( !GEditor )
        return nullptr;
    
    // Get the world we'll spawn the landscape in
    UWorld* MyWorld = nullptr;
    {
        // We want to create the landscape in the landscape editor mode's world
        FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
        MyWorld = EditorWorldContext.World();
    }

    if ( !MyWorld )
        return nullptr;

    // We need to create the landscape now and assign it a new GUID so we can create the LayerInfos
    ALandscapeProxy* LandscapeProxy = nullptr;
    if ( CreateLandscapeStreamingProxy )
        LandscapeProxy = MyWorld->SpawnActor<ALandscapeStreamingProxy>();
    else
        LandscapeProxy = MyWorld->SpawnActor<ALandscape>();

    if (!LandscapeProxy)
        return nullptr;

    // Create a new GUID
    FGuid currentGUID = FGuid::NewGuid();
    LandscapeProxy->SetLandscapeGuid( currentGUID );

    // Set the landscape Transform
    LandscapeProxy->SetActorTransform( LandscapeTransform );

    // Autosaving the layers prevents them for being deleted with the Asset
    // Save the packages created for the LayerInfos
    //if ( CreatedLayerInfoPackage.Num() > 0 )
    //    FEditorFileUtils::PromptForCheckoutAndSave( CreatedLayerInfoPackage, true, false );

    // Import the landscape data

    // Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
    LandscapeProxy->bCastStaticShadow = false;

    if ( LandscapeMaterial )
        LandscapeProxy->LandscapeMaterial = LandscapeMaterial;

    if ( LandscapeHoleMaterial )
        LandscapeProxy->LandscapeHoleMaterial = LandscapeHoleMaterial;

    // Setting the layer type here.
    ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

	TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	HeightmapDataPerLayers.Add(FGuid(), IntHeightData);
	MaterialLayerDataPerLayer.Add(FGuid(), ImportLayerInfos);

    // Import the data
    LandscapeProxy->Import(
        currentGUID,
        0, 0, XSize - 1, YSize - 1,
        NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
	HeightmapDataPerLayers, NULL,
	MaterialLayerDataPerLayer, ImportLayerType );

    // Copied straight from UE source code to avoid crash after importing the landscape:
    // automatically calculate a lighting LOD that won't crash lightmass (hopefully)
    // < 2048x2048 -> LOD0,  >=2048x2048 -> LOD1,  >= 4096x4096 -> LOD2,  >= 8192x8192 -> LOD3
    LandscapeProxy->StaticLightingLOD = FMath::DivideAndRoundUp( FMath::CeilLogTwo( ( XSize * YSize ) / ( 2048 * 2048 ) + 1 ), ( uint32 )2 );

    // Register all the landscape components
    LandscapeProxy->RegisterAllComponents();

    /*ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
    if (!Landscape)
        return nullptr;*/

    return LandscapeProxy;
}

void FHoudiniLandscapeUtils::GetHeightFieldLandscapeMaterials(
    const FHoudiniGeoPartObject& Heightfield,
    UMaterialInterface*& LandscapeMaterial,
    UMaterialInterface*& LandscapeHoleMaterial )
{
    LandscapeMaterial = nullptr;
    LandscapeHoleMaterial = nullptr;

    if ( !Heightfield.IsVolume() )
        return;

    std::string MarshallingAttributeNameMaterial = HAPI_UNREAL_ATTRIB_MATERIAL;
    std::string MarshallingAttributeNameMaterialInstance = HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE; 
    std::string MarshallingAttributeNameMaterialHole = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;
    std::string MarshallingAttributeNameMaterialHoleInstance = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE_INSTANCE;

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings )
    {
        if ( !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterial,
                MarshallingAttributeNameMaterial);

        if ( !HoudiniRuntimeSettings->MarshallingAttributeMaterialHole.IsEmpty() )
            FHoudiniEngineUtils::ConvertUnrealString(
                HoudiniRuntimeSettings->MarshallingAttributeMaterialHole,
                MarshallingAttributeNameMaterialHole );
    }

    TArray< FString > Materials;
    HAPI_AttributeInfo AttribMaterials;
    FHoudiniApi::AttributeInfo_Init(&AttribMaterials);
    //FMemory::Memset< HAPI_AttributeInfo >( AttribMaterials, 0 );

    // First, look for landscape material
    {
        FHoudiniEngineUtils::HapiGetAttributeDataAsString(
            Heightfield, MarshallingAttributeNameMaterial.c_str(),
            AttribMaterials, Materials );

        // If the material attribute was not found, check the material instance attribute.
        if ( !AttribMaterials.exists )
        {
            Materials.Empty();
            FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                Heightfield, MarshallingAttributeNameMaterialInstance.c_str(),
                AttribMaterials, Materials);
        }

        if ( AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL )
        {
            HOUDINI_LOG_WARNING( TEXT( "Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute." ) );
            AttribMaterials.exists = false;
            Materials.Empty();
        }

        if ( AttribMaterials.exists && Materials.Num() > 0 )
        {
            // Load the material
            LandscapeMaterial = Cast< UMaterialInterface >( StaticLoadObject(
                UMaterialInterface::StaticClass(),
                nullptr, *( Materials[ 0 ] ), nullptr, LOAD_NoWarn, nullptr ) );
        }
    }

    Materials.Empty();
    FHoudiniApi::AttributeInfo_Init(&AttribMaterials);
    //FMemory::Memset< HAPI_AttributeInfo >( AttribMaterials, 0 );
    
    // Then, for the hole_material
    {
        FHoudiniEngineUtils::HapiGetAttributeDataAsString(
            Heightfield, MarshallingAttributeNameMaterialHole.c_str(),
            AttribMaterials, Materials );

        // If the material attribute was not found, check the material instance attribute.
        if (!AttribMaterials.exists)
        {
            Materials.Empty();
            FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                Heightfield, MarshallingAttributeNameMaterialHoleInstance.c_str(),
                AttribMaterials, Materials);
        }

        if ( AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL )
        {
            HOUDINI_LOG_WARNING( TEXT( "Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute." ) );
            AttribMaterials.exists = false;
            Materials.Empty();
        }

        if ( AttribMaterials.exists && Materials.Num() > 0 )
        {
            // Load the material
            LandscapeHoleMaterial = Cast< UMaterialInterface >( StaticLoadObject(
                UMaterialInterface::StaticClass(),
                nullptr, *( Materials[ 0 ] ), nullptr, LOAD_NoWarn, nullptr ) );
        }
    }
}

bool FHoudiniLandscapeUtils::CreateLandscapeLayers(
    FHoudiniCookParams& HoudiniCookParams,
    const TArray< const FHoudiniGeoPartObject* >& FoundLayers,
    const FHoudiniGeoPartObject& Heightfield,
    const int32& LandscapeXSize, const int32& LandscapeYSize,
    const TMap<FString, float>& GlobalMinimums,
    const TMap<FString, float>& GlobalMaximums,
    TArray<FLandscapeImportLayerInfo>& ImportLayerInfos )
{    
    // Verifying HoudiniCookParams validity
    if ( !HoudiniCookParams.HoudiniAsset || !HoudiniCookParams.CookedTemporaryLandscapeLayers )
        return false;

    ImportLayerInfos.Empty();

    // Get the names of all the non weight blended layers
    TArray< FString > NonWeightBlendedLayerNames;
    FHoudiniLandscapeUtils::GetNonWeightBlendedLayerNames( Heightfield, NonWeightBlendedLayerNames );

    TArray<UPackage*> CreatedLandscapeLayerPackage;

    // Try to create all the layers
    ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;
    for ( TArray<const FHoudiniGeoPartObject *>::TConstIterator IterLayers( FoundLayers ); IterLayers; ++IterLayers )
    {
        const FHoudiniGeoPartObject * LayerGeoPartObject = *IterLayers;
        if ( !LayerGeoPartObject )
            continue;

        if ( !LayerGeoPartObject->IsValid() )
            continue;

        if ( LayerGeoPartObject->AssetId == -1 )
            continue;

        TArray< float > FloatLayerData;
        HAPI_VolumeInfo LayerVolumeInfo;
        float LayerMin = 0;
        float LayerMax = 0;
        if ( !FHoudiniLandscapeUtils::GetHeightfieldData( *LayerGeoPartObject, FloatLayerData, LayerVolumeInfo, LayerMin, LayerMax ) )
            continue;

        // No need to create flat layers as Unreal will remove them afterwards..
        if ( LayerMin == LayerMax )
            continue;

        // Get the layer's name
        FString LayerString;
        FHoudiniEngineString(LayerVolumeInfo.nameSH).ToFString(LayerString);

        // Check if that landscape layer has been marked as unit (range in [0-1]
        if ( IsUnitLandscapeLayer( *LayerGeoPartObject ) )
        {
            LayerMin = 0.0f;
            LayerMax = 1.0f;
        }
        else
        {
            // We want to convert the layer using the global Min/Max
            if ( GlobalMaximums.Contains( LayerString ) )
                LayerMax = GlobalMaximums[ LayerString ];

            if ( GlobalMinimums.Contains( LayerString ) )
                LayerMin = GlobalMinimums[ LayerString ];
        }        

        // Creating the ImportLayerInfo and LayerInfo objects
        ObjectTools::SanitizeObjectName( LayerString );
        FName LayerName( *LayerString );
        FLandscapeImportLayerInfo currentLayerInfo( LayerName );

        UPackage * Package = nullptr;
        currentLayerInfo.LayerInfo = FHoudiniLandscapeUtils::CreateLandscapeLayerInfoObject( HoudiniCookParams, LayerString.GetCharArray().GetData(), Package, LayerGeoPartObject->PartId);
        if ( !currentLayerInfo.LayerInfo || !Package )
            continue;

        // Convert the float data to uint8
        // HF masks need their X/Y sizes swapped
        if ( !FHoudiniLandscapeUtils::ConvertHeightfieldLayerToLandscapeLayer(
            FloatLayerData, LayerVolumeInfo.yLength, LayerVolumeInfo.xLength,
            LayerMin, LayerMax,
            LandscapeXSize, LandscapeYSize,
            currentLayerInfo.LayerData ) )
            continue;

        // We will store the data used to convert from Houdini values to int in the DebugColor
        // This is the only way we'll be able to reconvert those values back to their houdini equivalent afterwards...
        // R = Min, G = Max, B = Spacing, A = ?
        currentLayerInfo.LayerInfo->LayerUsageDebugColor.R = LayerMin;
        currentLayerInfo.LayerInfo->LayerUsageDebugColor.G = LayerMax;
        currentLayerInfo.LayerInfo->LayerUsageDebugColor.B = ( LayerMax - LayerMin) / 255.0f;
        currentLayerInfo.LayerInfo->LayerUsageDebugColor.A = PI;

        HoudiniCookParams.CookedTemporaryLandscapeLayers->Add( Package, Heightfield );

        // Visibility are by default non weight blended
        if ( NonWeightBlendedLayerNames.Contains( LayerString )
            || LayerString.Equals(TEXT("visibility"), ESearchCase::IgnoreCase) )
            currentLayerInfo.LayerInfo->bNoWeightBlend = true;
        else
            currentLayerInfo.LayerInfo->bNoWeightBlend = false;

        // Mark the package dirty...
        Package->MarkPackageDirty();

        CreatedLandscapeLayerPackage.Add( Package );

        ImportLayerInfos.Add( currentLayerInfo );
    }

    // Autosaving the layers prevents them for being deleted with the Asset
    // Save the packages created for the LayerInfos
    FEditorFileUtils::PromptForCheckoutAndSave( CreatedLandscapeLayerPackage, true, false );

    return true;
}

ULandscapeLayerInfoObject *
FHoudiniLandscapeUtils::CreateLandscapeLayerInfoObject( FHoudiniCookParams& HoudiniCookParams, const TCHAR* LayerName, UPackage*& Package , HAPI_PartId PartId)
{
    // Verifying HoudiniCookParams validity
    if ( !HoudiniCookParams.HoudiniAsset || HoudiniCookParams.HoudiniAsset->IsPendingKill() )
        return nullptr;

    FString ComponentGUIDString = HoudiniCookParams.PackageGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    FString LayerNameString = FString::Printf( TEXT( "%s_%d" ), LayerName, (int32)PartId );
    LayerNameString = UPackageTools::SanitizePackageName( LayerNameString );

    // Create the LandscapeInfoObjectName from the Asset name and the mask name
    FName LayerObjectName = FName( * (HoudiniCookParams.HoudiniAsset->GetName() + ComponentGUIDString + TEXT( "_LayerInfoObject_" ) + LayerNameString ) );

    // Save the package in the temp folder
    FString Path = HoudiniCookParams.TempCookFolder.ToString() + TEXT( "/" );
    FString PackageName = Path + LayerObjectName.ToString();
    PackageName = UPackageTools::SanitizePackageName( PackageName );

    // See if package exists, if it does, reuse it
    bool bCreatedPackage = false;
    Package = FindPackage( nullptr, *PackageName );
    if ( !Package || Package->IsPendingKill() )
    {
        // We need to create a new package
        Package = CreatePackage( nullptr, *PackageName );
        bCreatedPackage = true;
    }

    if ( !Package || Package->IsPendingKill() )
        return nullptr;

    if ( !Package->IsFullyLoaded() )
        Package->FullyLoad();

    ULandscapeLayerInfoObject* LayerInfo = nullptr;
    if ( !bCreatedPackage )
    {
        // See if we can load the layer info instead of creating a new one
        LayerInfo = (ULandscapeLayerInfoObject*)StaticFindObjectFast(ULandscapeLayerInfoObject::StaticClass(), Package, LayerObjectName);
    }

    if ( !LayerInfo || LayerInfo->IsPendingKill() )
    {
        // Create a new LandscapeLayerInfoObject in the package
        LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, LayerObjectName, RF_Public | RF_Standalone /*| RF_Transactional*/);

        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(LayerInfo);
    }

    if ( LayerInfo && !LayerInfo->IsPendingKill() )
    {
        LayerInfo->LayerName = LayerName;

        // Trigger update of the Layer Info
        LayerInfo->PreEditChange(nullptr);
        LayerInfo->PostEditChange();
        LayerInfo->MarkPackageDirty();

        // Mark the package dirty...
        Package->MarkPackageDirty();
    }

    return LayerInfo;
}

bool FHoudiniLandscapeUtils::AddLandscapeMaterialAttributesToVolume(
    const HAPI_NodeId& VolumeNodeId, const HAPI_PartId& PartId,
    UMaterialInterface* LandscapeMaterial, UMaterialInterface* LandscapeHoleMaterial )
{
    if ( !FHoudiniEngineUtils::IsValidNodeId(VolumeNodeId) )
        return false;

    // LANDSCAPE MATERIAL
    if ( LandscapeMaterial && !LandscapeMaterial->IsPendingKill() )
    {
        // Extract the path name from the material interface
        FString LandscapeMaterialString = LandscapeMaterial->GetPathName();

        // Get name of attribute used for marshalling materials.
        std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL;

        // Marshall in material names.
        HAPI_AttributeInfo AttributeInfoMaterial;
        FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
        //FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoMaterial);
        AttributeInfoMaterial.count = 1;
        AttributeInfoMaterial.tupleSize = 1;
        AttributeInfoMaterial.exists = true;
        AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
        AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

        HAPI_Result Result = FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
            MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial);

        if ( HAPI_RESULT_SUCCESS == Result )
        {
            // Convert the FString to cont char *
            std::string LandscapeMatCStr = TCHAR_TO_ANSI(*LandscapeMaterialString);
            const char* LandscapeMatCStrRaw = LandscapeMatCStr.c_str();
            TArray<const char *> LandscapeMatArr;
            LandscapeMatArr.Add( LandscapeMatCStrRaw );

            // Set the attribute's string data
            Result = FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial,
                LandscapeMatArr.GetData(), 0, AttributeInfoMaterial.count);
        }

        if( Result != HAPI_RESULT_SUCCESS )
        {
            // Failed to create the attribute
            HOUDINI_LOG_WARNING(
                TEXT("Failed to upload unreal_material attribute for landscape: %s"),
                *FHoudiniEngineUtils::GetErrorDescription());
        }
    }

    // HOLE MATERIAL
    if ( LandscapeHoleMaterial && !LandscapeHoleMaterial->IsPendingKill() )
    {
        // Extract the path name from the material interface
        FString LandscapeMaterialString = LandscapeHoleMaterial->GetPathName();

        // Get name of attribute used for marshalling materials.
        std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;

        // Marshall in material names.
        HAPI_AttributeInfo AttributeInfoMaterial;
        FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
        //FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoMaterial);
        AttributeInfoMaterial.count = 1;
        AttributeInfoMaterial.tupleSize = 1;
        AttributeInfoMaterial.exists = true;
        AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
        AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
        AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

        HAPI_Result Result = FHoudiniApi::AddAttribute(
            FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
            MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial);

        if ( Result == HAPI_RESULT_SUCCESS )
        {
            // Convert the FString to cont char *
            std::string LandscapeMatCStr = TCHAR_TO_ANSI(*LandscapeMaterialString);
            const char* LandscapeMatCStrRaw = LandscapeMatCStr.c_str();
            TArray<const char *> LandscapeMatArr;
            LandscapeMatArr.Add(LandscapeMatCStrRaw);

            // Set the attribute's string data
            Result = FHoudiniApi::SetAttributeStringData(
                FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
                MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial,
                LandscapeMatArr.GetData(), 0, AttributeInfoMaterial.count);
        }

        if ( Result != HAPI_RESULT_SUCCESS )
        {
            // Failed to create the attribute
            HOUDINI_LOG_WARNING(
                TEXT("Failed to upload unreal_material attribute for landscape: %s"),
                *FHoudiniEngineUtils::GetErrorDescription());
        }
    }

    return true;
}

bool FHoudiniLandscapeUtils::AddLandscapeGlobalMaterialAttribute( const HAPI_NodeId& NodeId, ALandscapeProxy * LandscapeProxy )
{
    if ( !LandscapeProxy )
        return false;

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    // Get name of attribute used for marshalling materials.
    std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL;
    if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
    {
        FHoudiniEngineUtils::ConvertUnrealString(
            HoudiniRuntimeSettings->MarshallingAttributeMaterial,
            MarshallingAttributeMaterialName );
    }

    // If there's a global landscape material, we marshall it as detail.
    UMaterialInterface * MaterialInterface = LandscapeProxy->GetLandscapeMaterial();
    const char * MaterialNameStr = "";
    if ( MaterialInterface )
    {
        FString FullMaterialName = MaterialInterface->GetPathName();
        MaterialNameStr = TCHAR_TO_UTF8(*FullMaterialName);
    }

    HAPI_AttributeInfo AttributeInfoDetailMaterial;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoDetailMaterial);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterial );
    AttributeInfoDetailMaterial.count = 1;
    AttributeInfoDetailMaterial.tupleSize = 1;
    AttributeInfoDetailMaterial.exists = true;
    AttributeInfoDetailMaterial.owner = HAPI_ATTROWNER_DETAIL;
    AttributeInfoDetailMaterial.storage = HAPI_STORAGETYPE_STRING;
    AttributeInfoDetailMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(), NodeId, 0,
        MarshallingAttributeMaterialName.c_str(), &AttributeInfoDetailMaterial ), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
        FHoudiniEngine::Get().GetSession(), NodeId, 0,
        MarshallingAttributeMaterialName.c_str(), &AttributeInfoDetailMaterial,
        (const char**)&MaterialNameStr, 0, AttributeInfoDetailMaterial.count ), false );

    // Get name of attribute used for marshalling hole materials.
    std::string MarshallingAttributeMaterialHoleName = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;
    if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterialHole.IsEmpty() )
    {
        FHoudiniEngineUtils::ConvertUnrealString(
            HoudiniRuntimeSettings->MarshallingAttributeMaterialHole,
            MarshallingAttributeMaterialHoleName );
    }

    // If there's a global landscape hole material, we marshall it as detail.
    UMaterialInterface * HoleMaterialInterface = LandscapeProxy->GetLandscapeHoleMaterial();
    const char * HoleMaterialNameStr = "";
    if ( HoleMaterialInterface )
    {
        FString FullMaterialName = HoleMaterialInterface->GetPathName();
        MaterialNameStr = TCHAR_TO_UTF8( *FullMaterialName );
    }

    HAPI_AttributeInfo AttributeInfoDetailMaterialHole;
    FHoudiniApi::AttributeInfo_Init(&AttributeInfoDetailMaterialHole);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterialHole );
    AttributeInfoDetailMaterialHole.count = 1;
    AttributeInfoDetailMaterialHole.tupleSize = 1;
    AttributeInfoDetailMaterialHole.exists = true;
    AttributeInfoDetailMaterialHole.owner = HAPI_ATTROWNER_DETAIL;
    AttributeInfoDetailMaterialHole.storage = HAPI_STORAGETYPE_STRING;
    AttributeInfoDetailMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
        FHoudiniEngine::Get().GetSession(),
        NodeId, 0, MarshallingAttributeMaterialHoleName.c_str(),
        &AttributeInfoDetailMaterialHole), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeStringData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, 0, MarshallingAttributeMaterialHoleName.c_str(),
        &AttributeInfoDetailMaterialHole, (const char **)&HoleMaterialNameStr, 0,
        AttributeInfoDetailMaterialHole.count ), false );
    
    return true;
}

bool
FHoudiniLandscapeUtils::UpdateOldLandscapeReference(ALandscapeProxy* OldLandscape, ALandscapeProxy*  NewLandscape)
{
    if ( !OldLandscape || !NewLandscape )
        return false;

    bool bReturn = false;

    // Iterates through all the Houdini Assets in the scene
    UWorld* editorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if ( !editorWorld )
        return false;

    for ( TActorIterator<AHoudiniAssetActor> ActorItr( editorWorld ); ActorItr; ++ActorItr )
    {
        AHoudiniAssetActor* Actor = *ActorItr;
        UHoudiniAssetComponent * HoudiniAssetComponent = Actor->GetHoudiniAssetComponent();
        if ( !HoudiniAssetComponent || !HoudiniAssetComponent->IsValidLowLevel() )
        {
            HOUDINI_LOG_ERROR(TEXT("Failed to export a Houdini Asset in the scene!"));
            continue;
        }

        if ( HoudiniAssetComponent->IsTemplate() )
            continue;

        if ( HoudiniAssetComponent->IsPendingKillOrUnreachable() )
            continue;

        if ( !HoudiniAssetComponent->GetOuter() )
            continue;

        bReturn = HoudiniAssetComponent->ReplaceLandscapeInInputs( OldLandscape, NewLandscape );
    }

    return bReturn;
}

bool
FHoudiniLandscapeUtils::InitDefaultHeightfieldMask(
    const HAPI_VolumeInfo& HeightVolumeInfo,
    const HAPI_NodeId& MaskVolumeNodeId )
{
    // We need to have a mask layer as it is required for proper heightfield functionalities

    // Creating an array filled with 0.0
    TArray< float > MaskFloatData;
    MaskFloatData.Init( 0.0f, HeightVolumeInfo.xLength * HeightVolumeInfo.yLength );

    // Creating the volume infos
    HAPI_VolumeInfo MaskVolumeInfo = HeightVolumeInfo;

    // Set the heighfield data in Houdini
    FString MaskName = TEXT("mask");
    HAPI_PartId PartId = 0;
    if ( !SetHeighfieldData( MaskVolumeNodeId, PartId, MaskFloatData, MaskVolumeInfo, MaskName ) )
        return false;

    return true;
}


bool
FHoudiniLandscapeUtils::BackupLandscapeToFile(const FString& BaseName, ALandscapeProxy* Landscape)
{
    // We need to cache the input landscape to a file    
    if ( !Landscape )
        return false;

    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
        return false;

    // Save Height data to file
    //FString HeightSave = TEXT("/Game/HoudiniEngine/Temp/HeightCache.png");    
    FString HeightSave = BaseName + TEXT("_height.png");
    LandscapeInfo->ExportHeightmap(HeightSave);
    Landscape->ReimportHeightmapFilePath = HeightSave;

    // Save each layer to a file
    for ( int LayerIndex = 0; LayerIndex < LandscapeInfo->Layers.Num(); LayerIndex++ )
    {
        FName CurrentLayerName = LandscapeInfo->Layers[LayerIndex].GetLayerName();
        //ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->GetLayerInfoByName(CurrentLayerName, Landscape);
        ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->Layers[LayerIndex].LayerInfoObj;
        if ( !CurrentLayerInfo || CurrentLayerInfo->IsPendingKill() )
            continue;

        FString LayerSave = BaseName + CurrentLayerName.ToString() + TEXT(".png");
        LandscapeInfo->ExportLayer(CurrentLayerInfo, LayerSave);

        // Update the file reimport path on the input landscape for this layer
        LandscapeInfo->GetLayerEditorSettings(CurrentLayerInfo).ReimportLayerFilePath = LayerSave;
    }

    return true;
}

bool
FHoudiniLandscapeUtils::RestoreLandscapeFromFile( ALandscapeProxy* LandscapeProxy )
{
    if ( !LandscapeProxy )
        return false;

    ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
    if (!LandscapeInfo)
        return false;

    // Restore Height data from the backup file
    FString ReimportFile = LandscapeProxy->ReimportHeightmapFilePath;
    if ( !ImportLandscapeData(LandscapeInfo, ReimportFile, TEXT("height") ) )
        HOUDINI_LOG_ERROR(TEXT("Could not restore the landscape actor's source height data."));

    
    // Restore each layer from the backup file
    TArray< ULandscapeLayerInfoObject* > SourceLayers;
    for ( int LayerIndex = 0; LayerIndex < LandscapeProxy->EditorLayerSettings.Num(); LayerIndex++ )
    {
        ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeProxy->EditorLayerSettings[LayerIndex].LayerInfoObj;
        if (!CurrentLayerInfo || CurrentLayerInfo->IsPendingKill())
            continue;

        FString CurrentLayerName = CurrentLayerInfo->LayerName.ToString();
        ReimportFile = LandscapeProxy->EditorLayerSettings[LayerIndex].ReimportLayerFilePath;

        if (!ImportLandscapeData(LandscapeInfo, ReimportFile, CurrentLayerName, CurrentLayerInfo))
            HOUDINI_LOG_ERROR( TEXT("Could not restore the landscape actor's source height data.") );

        SourceLayers.Add( CurrentLayerInfo );
    }

    // Iterate on the landscape info's layer to remove any layer that could have been added by Houdini
    for (int LayerIndex = 0; LayerIndex < LandscapeInfo->Layers.Num(); LayerIndex++)
    {
        ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->Layers[LayerIndex].LayerInfoObj;
        if ( SourceLayers.Contains( CurrentLayerInfo ) )
            continue;

        // Delete the added layer
        FName LayerName = LandscapeInfo->Layers[LayerIndex].LayerName;
        LandscapeInfo->DeleteLayer(CurrentLayerInfo, LayerName);
    }

    return true;
}

bool
FHoudiniLandscapeUtils::ImportLandscapeData(
    ULandscapeInfo* LandscapeInfo, const FString& Filename, const FString& LayerName, ULandscapeLayerInfoObject* LayerInfoObject )
{
    //
    // Code copied/edited from FEdModeLandscape::ImportData as we cannot access that function
    //
    if ( !LandscapeInfo )
        return false;

    bool IsHeight = LayerName.Equals(TEXT("height"), ESearchCase::IgnoreCase);

    int32 MinX, MinY, MaxX, MaxY;
    if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        const FLandscapeFileResolution LandscapeResolution = { (uint32)(1 + MaxX - MinX), (uint32)(1 + MaxY - MinY) };

        ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");

        if ( IsHeight )
        {
            const ILandscapeHeightmapFileFormat* HeightmapFormat = LandscapeEditorModule.GetHeightmapFormatByExtension(*FPaths::GetExtension(Filename, true));

            if (!HeightmapFormat)
            {
                HOUDINI_LOG_ERROR( TEXT("Could not reimport the input heightfield's source data for %s, File type not recognised"), *LayerName );
                return false;
            }

            FLandscapeFileResolution ImportResolution = { 0, 0 };

            const FLandscapeHeightmapInfo HeightmapInfo = HeightmapFormat->Validate(*Filename);

            // display error message if there is one, and abort the import
            if (HeightmapInfo.ResultCode == ELandscapeImportResult::Error)
            {
                HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, %s"), *LayerName, *(HeightmapInfo.ErrorMessage.ToString()) );
                return false;
            }

            // if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
            if (HeightmapInfo.PossibleResolutions.Num() > 1)
            {
                if (!HeightmapInfo.PossibleResolutions.Contains(LandscapeResolution))
                {
                    HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. The heightmap file does not match the Landscape extent and its exact resolution could not be determined"));
                    return false;
                }
                else
                {
                    ImportResolution = LandscapeResolution;
                }
            }

            // display warning message if there is one and allow user to cancel
            if (HeightmapInfo.ResultCode == ELandscapeImportResult::Warning)
                HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. %s"), *LayerName, *(HeightmapInfo.ErrorMessage.ToString()));

            // if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
            // unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
            if (HeightmapInfo.PossibleResolutions.Num() == 1)
            {
                ImportResolution = HeightmapInfo.PossibleResolutions[0];
                if (ImportResolution != LandscapeResolution)
                    HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. The heightmap file's size does not match the current Landscape extent, data will be padded/clipped"), *LayerName);
            }

            FLandscapeHeightmapImportData ImportData = HeightmapFormat->Import(*Filename, ImportResolution);
            if (ImportData.ResultCode == ELandscapeImportResult::Error)
            {
                HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. %s"), *LayerName, *(ImportData.ErrorMessage.ToString()));
                return false;
            }

            TArray<uint16> Data;
            if (ImportResolution != LandscapeResolution)
            {
                // Cloned from FLandscapeEditorDetailCustomization_NewLandscape.OnCreateButtonClicked
                // so that reimports behave the same as the initial import :)

                const int32 OffsetX = (int32)(LandscapeResolution.Width - ImportResolution.Width) / 2;
                const int32 OffsetY = (int32)(LandscapeResolution.Height - ImportResolution.Height) / 2;

                Data.SetNumUninitialized(LandscapeResolution.Width * LandscapeResolution.Height * sizeof(uint16));

                ExpandData<uint16>(Data.GetData(), ImportData.Data.GetData(),
                    0, 0, ImportResolution.Width - 1, ImportResolution.Height - 1,
                    -OffsetX, -OffsetY, LandscapeResolution.Width - OffsetX - 1, LandscapeResolution.Height - OffsetY - 1);
            }
            else
            {
                Data = MoveTemp(ImportData.Data);
            }

            //FScopedTransaction Transaction(TEXT("Undo_ImportHeightmap", "Importing Landscape Heightmap"));

            FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
            HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData());
        }
        else
        {
            // We're importing a Landscape layer
            if ( !LayerInfoObject || LayerInfoObject->IsPendingKill() )
                return false;

            const ILandscapeWeightmapFileFormat* WeightmapFormat = LandscapeEditorModule.GetWeightmapFormatByExtension(*FPaths::GetExtension(Filename, true));

            if (!WeightmapFormat)
            {
                HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, File type not recognised"), *LayerName);
                return false;
            }

            FLandscapeFileResolution ImportResolution = { 0, 0 };

            const FLandscapeWeightmapInfo WeightmapInfo = WeightmapFormat->Validate(*Filename, FName(*LayerName));

            // display error message if there is one, and abort the import
            if (WeightmapInfo.ResultCode == ELandscapeImportResult::Error)
            {
                HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, %s"), *LayerName, *(WeightmapInfo.ErrorMessage.ToString()));
                return false;
            }

            // if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
            if (WeightmapInfo.PossibleResolutions.Num() > 1)
            {
                if (!WeightmapInfo.PossibleResolutions.Contains(LandscapeResolution))
                {
                    HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. The weightmap file does not match the Landscape extent and its exact resolution could not be determined"));
                    return false;
                }
                else
                {
                    ImportResolution = LandscapeResolution;
                }
            }

            // display warning message if there is one and allow user to cancel
            if (WeightmapInfo.ResultCode == ELandscapeImportResult::Warning)
                HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. %s"), *LayerName, *(WeightmapInfo.ErrorMessage.ToString()));

            // if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
            // unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
            if (WeightmapInfo.PossibleResolutions.Num() == 1)
            {
                ImportResolution = WeightmapInfo.PossibleResolutions[0];
                if (ImportResolution != LandscapeResolution)
                    HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. The heightmap file's size does not match the current Landscape extent, data will be padded/clipped"), *LayerName);
            }

            FLandscapeWeightmapImportData ImportData = WeightmapFormat->Import(*Filename, FName(*LayerName), ImportResolution);

            if (ImportData.ResultCode == ELandscapeImportResult::Error)
            {
                HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. %s"), *LayerName, *(ImportData.ErrorMessage.ToString()));
                return false;
            }

            TArray<uint8> Data;
            if (ImportResolution != LandscapeResolution)
            {
                // Cloned from FLandscapeEditorDetailCustomization_NewLandscape.OnCreateButtonClicked
                // so that reimports behave the same as the initial import :)
                const int32 OffsetX = (int32)(LandscapeResolution.Width - ImportResolution.Width) / 2;
                const int32 OffsetY = (int32)(LandscapeResolution.Height - ImportResolution.Height) / 2;

                Data.SetNumUninitialized(LandscapeResolution.Width * LandscapeResolution.Height * sizeof(uint8));

                ExpandData<uint8>(Data.GetData(), ImportData.Data.GetData(),
                    0, 0, ImportResolution.Width - 1, ImportResolution.Height - 1,
                    -OffsetX, -OffsetY, LandscapeResolution.Width - OffsetX - 1, LandscapeResolution.Height - OffsetY - 1);
            }
            else
            {
                Data = MoveTemp(ImportData.Data);
            }

            //FScopedTransaction Transaction(LOCTEXT("Undo_ImportWeightmap", "Importing Landscape Layer"));

            FAlphamapAccessor<false, false> AlphamapAccessor(LandscapeInfo, LayerInfoObject);
            AlphamapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData(), ELandscapeLayerPaintingRestriction::None);
        }
    }

    return true;
}

#endif

bool
FHoudiniLandscapeUtils::IsUnitLandscapeLayer(const FHoudiniGeoPartObject& LayerGeoPartObject)
{
    // Check the attribute exists on primitive or detail
    HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
    if (FHoudiniEngineUtils::HapiCheckAttributeExists(LayerGeoPartObject, "unreal_unit_landscape_layer", HAPI_ATTROWNER_PRIM))
        Owner = HAPI_ATTROWNER_PRIM;
    else if (FHoudiniEngineUtils::HapiCheckAttributeExists(LayerGeoPartObject, "unreal_unit_landscape_layer", HAPI_ATTROWNER_DETAIL))
        Owner = HAPI_ATTROWNER_DETAIL;
    else
        return false;

    // Check the value
    HAPI_AttributeInfo AttribInfoUnitLayer;
    FHoudiniApi::AttributeInfo_Init(&AttribInfoUnitLayer);
    TArray< int32 > AttribValues;
    FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
        LayerGeoPartObject, "unreal_unit_landscape_layer", AttribInfoUnitLayer, AttribValues, 1, Owner);

    if (AttribValues.Num() > 0 && AttribValues[0] == 1 )
        return true;

    return false;
}