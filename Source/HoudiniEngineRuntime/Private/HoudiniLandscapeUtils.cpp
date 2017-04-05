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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniLandscapeUtils.h"
#include "HoudiniApi.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"

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
        HAPI_AttributeInfo AttribInfoTile{};
        TArray< int32 > TileValues;

        FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
            Heightfield, "tile",
            AttribInfoTile, TileValues );

        if ( AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM )
        {
            HeightFieldTile = TileValues[ 0 ];
            bParentHeightfieldHasTile = true;
        }
    }

    // Look for all the layers/masks corresponding to the current heightfield
    //TArray< const FHoudiniGeoPartObject* > FoundLayers;
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

            HAPI_AttributeInfo AttribInfoTile{};
            TArray<int32> TileValues;

            FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
                HoudiniGeoPartObject, "tile",
                AttribInfoTile, TileValues );

            if ( AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM )
            {
                CurrentTile = TileValues[ 0 ];
            }

            // Does this layer come from the same tile as the height?
            if ( ( CurrentTile != HeightFieldTile ) || ( CurrentTile == -1 ) )
                continue;
        }

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo CurrentVolumeInfo;
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
    const TArray< const FHoudiniGeoPartObject* > & InHeightfieldArray,
    float& fGlobalMin, float& fGlobalMax )
{
    fGlobalMin = 0.0f;
    fGlobalMax = 0.0f;

    // TODO: Replace this unoptimized version with the proper HAPI call when available!
    // We need to know the global min/max for all hieghtfield found in order to convert the landscapes accurately    
    bool bMinMaxSet = false;
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
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            NodeId, CurrentHeightfield->PartId,
            &CurrentVolumeInfo ) )
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

        int32 XSize = CurrentVolumeInfo.xLength;
        int32 YSize = CurrentVolumeInfo.yLength;
        int32 SizeInPoints = XSize * YSize;
        int32 TotalSize = SizeInPoints * CurrentVolumeInfo.tupleSize;

        if ( ( XSize < 2 ) || ( YSize < 2 ) )
            continue;

        int32 XOffset = CurrentVolumeInfo.minX;
        int32 YOffset = CurrentVolumeInfo.minY;

        TArray<float> FloatHeightData;
        FloatHeightData.SetNumUninitialized( TotalSize );

        // Get all the heightfield data
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetHeightFieldData(
            FHoudiniEngine::Get().GetSession(),
            NodeId, CurrentHeightfield->PartId,
            FloatHeightData.GetData(),
            0, SizeInPoints ) )
            continue;

        // We will need the min and max value for the conversion to uint16
        if ( !bMinMaxSet )
        {
            fGlobalMin = FloatHeightData[ 0 ];
            fGlobalMax = fGlobalMin;
            bMinMaxSet = true;
        }

        for ( int32 n = 0; n < SizeInPoints; n++ )
        {
            if ( FloatHeightData[ n ] > fGlobalMax )
                fGlobalMax = FloatHeightData[ n ];
            else if ( FloatHeightData[ n ] < fGlobalMin )
                fGlobalMin = FloatHeightData[ n ];
        }
    }
}

bool FHoudiniLandscapeUtils::ExtractHeightfieldData(
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
    for ( int32 n = 0; n < SizeInPoints; n++ )
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
    const TArray<float>& HeightfieldFloatValues,
    const HAPI_VolumeInfo& HeightfieldVolumeInfo,
    const float& FloatMin, const float& FloatMax,
    TArray<uint16>& IntHeightData,
    FTransform& LandscapeTransform,
    int32& FinalXSize, int32& FinalYSize,
    int32& NumSectionPerLandscapeComponent,
    int32& NumQuadsPerLandscapeSection )
{
    IntHeightData.Empty();
    LandscapeTransform.SetIdentity();
    FinalXSize = -1;
    FinalYSize = -1;
    NumSectionPerLandscapeComponent = -1;
    NumQuadsPerLandscapeSection = -1;

    int32 HoudiniXSize = HeightfieldVolumeInfo.xLength;
    int32 HoudiniYSize = HeightfieldVolumeInfo.yLength;
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

    // The ZRange in Houdini (in m)
    double MeterZRange = (double) ( FloatMax - FloatMin );

    // The corresponding unreal digit range (as unreal uses uint16, max is 65535)
    // We may want to not use the full range in order to be able to sculpt the landscape past the min/max values after.
    double DigitZRange = 49152.0;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseFullResolution )
        DigitZRange = 65535.0;

    // If we  are not using the full range, we need to center the digit values so the terrain can be edited up and down
    double DigitCenterOffset = FMath::FloorToDouble( ( 65535.0 - DigitZRange ) / 2.0 );

    // The factor used to convert from Houdini's ZRange to the desired digit range
    double ZSpacing = ( MeterZRange != 0.0 ) ? ( DigitZRange / MeterZRange ) : 0.0;

    // Converting the data from Houdini to Unreal
    // For correct orientation in unreal, the point matrix has to be transposed.
    int32 XSize = HoudiniYSize;
    int32 YSize = HoudiniXSize;
    IntHeightData.SetNumUninitialized( SizeInPoints );

    int32 nUnreal = 0;
    for ( int32 nY = 0; nY < YSize; nY++ )
    {
        for ( int32 nX = 0; nX < XSize; nX++ )
        {
            // We need to invert X/Y when reading the value from Houdini
            // int32 nUnreal = nX + nY * XSize;
            int32 nHoudini = nY + nX * HoudiniXSize;

            // Get the double values in [0 - ZRange]
            double DoubleValue = (double)HeightfieldFloatValues[ nHoudini ] - (double)FloatMin;

            // Then convert it to [0 - DesiredRange] and center it 
            DoubleValue = DoubleValue * ZSpacing + DigitCenterOffset;

            //dValue = FMath::Clamp(dValue, 0.0, 65535.0);
            IntHeightData[ nUnreal++ ] = FMath::RoundToInt( DoubleValue );
        }
    }

    //--------------------------------------------------------------------------------------------------
    // 2. Resample / Pad the int data so that if fits unreal size requirements
    //--------------------------------------------------------------------------------------------------

    // Calculating the final size, number of component, sections...
    // UE expects special values for these, so we might need to pad/resample the height data
    int32 OldXSize = XSize;
    int32 OldYSize = YSize;
    NumSectionPerLandscapeComponent = 1;
    NumQuadsPerLandscapeSection = XSize - 1;
    FVector LandscapeResizeFactor = FVector( 1.0f, 1.0f, 1.0f );
    if ( !FHoudiniLandscapeUtils::ResizeHeightDataForLandscape(
        IntHeightData, XSize, YSize,
        NumSectionPerLandscapeComponent,
        NumQuadsPerLandscapeSection,
        LandscapeResizeFactor ) )
        return false;

    // If the landscape has been resized
    if ( ( OldXSize != XSize ) && ( OldYSize != YSize ) )
    {
        // Notify the user if the heightfield data was resized
        HOUDINI_LOG_WARNING(
            TEXT("Landscape was resized from ( %d x %d ) to ( %d x %d )."),
            OldXSize, OldYSize, XSize, YSize );
    }

    FinalXSize = XSize;
    FinalYSize = YSize;

    //--------------------------------------------------------------------------------------------------
    // 3. Calculating the proper transform for the landscape to be sized and positionned properly
    //--------------------------------------------------------------------------------------------------
    FTransform CurrentVolumeTransform;
    //FHoudiniEngineUtils::TranslateHapiTransform(HeightfieldVolumeInfo.transform, CurrentVolumeTransform);
    {
        HAPI_Transform HapiTransform = HeightfieldVolumeInfo.transform;

        FQuat ObjectRotation(
            HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[1],
            HapiTransform.rotationQuaternion[2], -HapiTransform.rotationQuaternion[3] );
        Swap( ObjectRotation.Y, ObjectRotation.Z );

        FVector ObjectTranslation( HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2] );
        ObjectTranslation *= 100.0f;
        Swap( ObjectTranslation[2], ObjectTranslation[1] );

        FVector ObjectScale3D( HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2] );
        Swap( ObjectScale3D.Y, ObjectScale3D.Z );
        Swap( ObjectScale3D.X, ObjectScale3D.Y );

        CurrentVolumeTransform.SetComponents( ObjectRotation, ObjectTranslation, ObjectScale3D );
    }

    // Scale:
    // Calculating the equivalent scale to match Houdini's Terrain Size in Unreal
    FVector LandscapeScale;

    // Unreal has a X/Y resolution of 1m per point while Houdini is dependant on the heighfield's grid spacing
    LandscapeScale.X = CurrentVolumeTransform.GetScale3D().X * 2.0f;
    LandscapeScale.Y = CurrentVolumeTransform.GetScale3D().Y * 2.0f;

    // Calculating the Z Scale so that the Z values in Unreal are the same as in Houdini
    // Unreal has a default Z range is 512m for a scale of a 100%
    LandscapeScale.Z = (float)( (double)( 65535.0 / DigitZRange ) * MeterZRange / 512.0 );
    LandscapeScale *= 100.f;

    // If the data was resized and not expanded, we need to modify the landscape's scale
    LandscapeScale *= LandscapeResizeFactor;

    // We'll use the position from Houdini, but we will need to offset the Z Position to center the 
    // values properly as the data has been offset by the conversion to uint16
    FVector LandscapePosition = CurrentVolumeTransform.GetLocation();
    LandscapePosition.Z = 0.0f;

    /*
    // Trying to compensate the landscape grid orientation (XZ / YZ / XY)
    if ( FMath::Abs(LandscapePosition.Z) < SMALL_NUMBER )
    {
        // XZ orientation, we just need to invert Y
        /LandscapePosition.X = LandscapePosition.X;
        //LandscapePosition.Y = LandscapePosition.Y;
        LandscapePosition.Z = 0.0f;
    }
    else if ( FMath::Abs( LandscapePosition.X ) < SMALL_NUMBER )
    {
        // YZ orientation
        LandscapePosition.X = LandscapePosition.Y;
        LandscapePosition.Y = LandscapePosition.Z;
        LandscapePosition.Z = 0.0f;
    }
    else if ( FMath::Abs( LandscapePosition.Y ) < SMALL_NUMBER )
    {
        // XY orientation
        LandscapePosition.X = LandscapePosition.Z;
        LandscapePosition.Y = LandscapePosition.X;
        LandscapePosition.Z = 0.0f;
    }
    */

    // We need to calculate the position offset so that Houdini and Unreal have the same Zero position
    // In Unreal, zero has a height value of 32768.
    // These values are then divided by 128 internally, and then multiplied by the Landscape's Z scale
    // ( DIGIT - 32768 ) / 128 * ZScale = ZOffset

    // We need the Digit (Unreal) value of Houdini's zero for the scale calculation
    uint16 HoudiniZeroValueInDigit = FMath::RoundToInt( ( 0.0 - (double)FloatMin ) * ZSpacing + DigitCenterOffset );

    float ZOffset = -( (float)HoudiniZeroValueInDigit - 32768.0f ) / 128.0f * LandscapeScale.Z;
    if ( false )
    {
        ZOffset = ( (double)( 32768 - HoudiniZeroValueInDigit ) - DigitCenterOffset ) / ZSpacing + (double)FloatMin;
        ZOffset *= LandscapeScale.Z;
    }

    LandscapePosition.Z += ZOffset;

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
    TArray<uint8>& LayerData )
{
    int32 LayerXSize = HoudiniYSize;
    int32 LayerYSize = HoudiniXSize;
    
    // Convert the float data to uint8
    LayerData.SetNumUninitialized( LayerXSize * LayerYSize );

    // Calculating the factor used to convert from Houdini's ZRange to [0 255]
    double LayerZRange = ( LayerMax - LayerMin );
    double LayerZSpacing = ( LayerZRange != 0.0 ) ? ( 255.0 / (double)( LayerMax - LayerMin ) ) : 0.0;

    int32 nUnrealIndex = 0;
    for ( int32 nY = 0; nY < LayerYSize; nY++ )
    {
        for ( int32 nX = 0; nX < LayerXSize; nX++ )
        {
            // We need to invert X/Y when reading the value from Houdini
            int32 nHoudini = nY + nX * HoudiniXSize;

            // Get the double values in [0 - ZRange]
            double DoubleValue = (double)FloatLayerData[ nHoudini ] - (double)LayerMin;

            // Then convert it to [0 - 255]
            DoubleValue *= LayerZSpacing;

            //dValue = FMath::Clamp(dValue, 0.0, 65535.0);
            LayerData[ nUnrealIndex++ ] = FMath::RoundToInt( DoubleValue );
        }
    }

    // Finally, we need to resize the data to fit with the new landscape size
    return FHoudiniLandscapeUtils::ResizeLayerDataForLandscape(
        LayerData, LayerXSize, LayerYSize,
        LandscapeXSize, LandscapeYSize );
}

//-------------------------------------------------------------------------------------------------------------------
// From LandscapeEditorUtils.h
//
//	Helpers function for FHoudiniEngineUtils::ResizeHeightDataForLandscape
//-------------------------------------------------------------------------------------------------------------------
template<typename T>
void ExpandData(T* OutData, const T* InData,
    int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
    int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
{
    const int32 OldWidth = OldMaxX - OldMinX + 1;
    const int32 OldHeight = OldMaxY - OldMinY + 1;
    const int32 NewWidth = NewMaxX - NewMinX + 1;
    const int32 NewHeight = NewMaxY - NewMinY + 1;
    const int32 OffsetX = NewMinX - OldMinX;
    const int32 OffsetY = NewMinY - OldMinY;

    for (int32 Y = 0; Y < NewHeight; ++Y)
    {
        const int32 OldY = FMath::Clamp<int32>(Y + OffsetY, 0, OldHeight - 1);

        // Pad anything to the left
        const T PadLeft = InData[OldY * OldWidth + 0];
        for (int32 X = 0; X < -OffsetX; ++X)
        {
            OutData[Y * NewWidth + X] = PadLeft;
        }

        // Copy one row of the old data
        {
            const int32 X = FMath::Max(0, -OffsetX);
            const int32 OldX = FMath::Clamp<int32>(X + OffsetX, 0, OldWidth - 1);
            FMemory::Memcpy(&OutData[Y * NewWidth + X], &InData[OldY * OldWidth + OldX], FMath::Min<int32>(OldWidth, NewWidth) * sizeof(T));
        }

        const T PadRight = InData[OldY * OldWidth + OldWidth - 1];
        for (int32 X = -OffsetX + OldWidth; X < NewWidth; ++X)
        {
            OutData[Y * NewWidth + X] = PadRight;
        }
    }
}

template<typename T>
TArray<T> ExpandData(const TArray<T>& Data,
    int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
    int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
{
    const int32 NewWidth = NewMaxX - NewMinX + 1;
    const int32 NewHeight = NewMaxY - NewMinY + 1;

    TArray<T> Result;
    Result.Empty(NewWidth * NewHeight);
    Result.AddUninitialized(NewWidth * NewHeight);

    ExpandData(Result.GetData(), Data.GetData(),
        OldMinX, OldMinY, OldMaxX, OldMaxY,
        NewMinX, NewMinY, NewMaxX, NewMaxY);

    return Result;
}

template<typename T>
TArray<T> ResampleData(const TArray<T>& Data, int32 OldWidth, int32 OldHeight, int32 NewWidth, int32 NewHeight)
{
    TArray<T> Result;
    Result.Empty(NewWidth * NewHeight);
    Result.AddUninitialized(NewWidth * NewHeight);

    const float XScale = (float)(OldWidth - 1) / (NewWidth - 1);
    const float YScale = (float)(OldHeight - 1) / (NewHeight - 1);
    for (int32 Y = 0; Y < NewHeight; ++Y)
    {
        for (int32 X = 0; X < NewWidth; ++X)
        {
            const float OldY = Y * YScale;
            const float OldX = X * XScale;
            const int32 X0 = FMath::FloorToInt(OldX);
            const int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, OldWidth - 1);
            const int32 Y0 = FMath::FloorToInt(OldY);
            const int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, OldHeight - 1);
            const T& Original00 = Data[Y0 * OldWidth + X0];
            const T& Original10 = Data[Y0 * OldWidth + X1];
            const T& Original01 = Data[Y1 * OldWidth + X0];
            const T& Original11 = Data[Y1 * OldWidth + X1];
            Result[Y * NewWidth + X] = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
        }
    }

    return Result;
}
//-------------------------------------------------------------------------------------------------------------------

bool
FHoudiniLandscapeUtils::ResizeHeightDataForLandscape(
    TArray<uint16>& HeightData,
    int32& SizeX, int32& SizeY,
    int32& NumberOfSectionsPerComponent,
    int32& NumberOfQuadsPerSection,
    FVector& LandscapeResizeFactor )
{
    if ( HeightData.Num() <= 4 )
        return false;

    if ( ( SizeX < 2 ) || ( SizeY < 2 ) )
        return false;

    NumberOfSectionsPerComponent = 1;
    NumberOfQuadsPerSection = 1;

    // Unreal's default sizes
    int32 SectionSizes[] = { 7, 15, 31, 63, 127, 255 };
    int32 NumSections[] = { 1, 2 };

    // Component count used to calculate the final size of the landscape
    int32 ComponentsCountX = 1;
    int32 ComponentsCountY = 1;
    int32 NewSizeX = -1;
    int32 NewSizeY = -1;

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
                ComponentsCountX = ( SizeX - 1 ) / ( ss * ns );
                ComponentsCountY = ( SizeY - 1 ) / ( ss * ns );
                ClampLandscapeSize();
                break;
            }
        }
        if ( bFoundMatch )
        {
            break;
        }
    }

    if ( !bFoundMatch )
    {
        // if there was no exact match, try increasing the section size until we encompass the whole heightmap
        const int32 CurrentSectionSize = NumberOfQuadsPerSection;
        const int32 CurrentNumSections = NumberOfSectionsPerComponent;
        for ( int32 SectionSizesIdx = 0; SectionSizesIdx < ARRAY_COUNT( SectionSizes ); SectionSizesIdx++ )
        {
            if ( SectionSizes[ SectionSizesIdx ] < CurrentSectionSize )
            {
                continue;
            }

            const int32 ComponentsX = FMath::DivideAndRoundUp( ( SizeX - 1 ), SectionSizes[ SectionSizesIdx ] * CurrentNumSections );
            const int32 ComponentsY = FMath::DivideAndRoundUp( ( SizeY - 1 ), SectionSizes[ SectionSizesIdx ] * CurrentNumSections );
            if ( ComponentsX <= 32 && ComponentsY <= 32 )
            {
                bFoundMatch = true;
                NumberOfQuadsPerSection = SectionSizes[ SectionSizesIdx ];
                // NumberOfSectionsPerComponent = ;
                ComponentsCountX = ComponentsX;
                ComponentsCountY = ComponentsY;
                ClampLandscapeSize();
                break;
            }
        }
    }

    if ( !bFoundMatch )
    {
        // if the heightmap is very large, fall back to using the largest values we support
        const int32 MaxSectionSize = SectionSizes[ ARRAY_COUNT( SectionSizes ) - 1 ];
        const int32 MaxNumSubSections = NumSections[ ARRAY_COUNT( NumSections ) - 1 ];
        const int32 ComponentsX = FMath::DivideAndRoundUp( ( SizeX - 1 ), MaxSectionSize * MaxNumSubSections );
        const int32 ComponentsY = FMath::DivideAndRoundUp( ( SizeY - 1 ), MaxSectionSize * MaxNumSubSections );

        bFoundMatch = true;
        NumberOfQuadsPerSection = MaxSectionSize;
        NumberOfSectionsPerComponent = MaxNumSubSections;
        ComponentsCountX = ComponentsX;
        ComponentsCountY = ComponentsY;
        ClampLandscapeSize();
    }

    if ( !bFoundMatch )
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

    // Do we need to resize/expand the data to the new size?
    bool bForceResample = false;
    if ( ( NewSizeX != SizeX ) || ( NewSizeY != SizeY ) )
    {
        bool bResample = bForceResample ? true : ( ( NewSizeX <= SizeX ) && ( NewSizeY <= SizeY ) );

        TArray<uint16> NewData;
        if ( !bResample )
        {
            // Expanding the data by padding
            NewData.SetNumUninitialized( NewSizeX * NewSizeY );

            const int32 OffsetX = (int32)( NewSizeX - SizeX ) / 2;
            const int32 OffsetY = (int32)( NewSizeY - SizeY ) / 2;

            // Expanding the Data
            NewData = ExpandData(
                HeightData, 0, 0, SizeX - 1, SizeY - 1,
                -OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1);

            // The landscape has been resized, we'll need to take that into account when sizing it
            //LandscapeResizeFactor.X = (float)NewSizeX / (float)SizeX;
            //LandscapeResizeFactor.Y = (float)NewSizeY / (float)SizeY;
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
        }

        // Replaces Old data with the new one
        HeightData = NewData;

        SizeX = NewSizeX;
        SizeY = NewSizeY;
    }

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


/*
ALandscape *
FHoudiniEngineUtils::DuplicateLandscapeAndCreatePackage(
const ALandscape * Landscape, UHoudiniAssetComponent * Component,
const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode)
{
    ALandscape * DuplicatedLandscape = nullptr;

    if ( HoudiniGeoPartObject.IsVolume() )
    {
        // Create package for this duplicated mesh.
        FString LandscapeName;
        FGuid LandscapeGuid;

        UPackage * LandscapePackage = FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(
        Component, HoudiniGeoPartObject, LandscapeName, LandscapeGuid, BakeMode);
        if (!LandscapePackage)
            return nullptr;

        // Duplicate Landscape for this new copied component.
        DuplicatedLandscape = cDuplicateObject< ALandscape >( Landscape, LandscapePackage, *LandscapeName );

        if ( BakeMode != EBakeMode::Intermediate )
            DuplicatedLandscape->SetFlags( RF_Public | RF_Standalone );

        // Add meta information.
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
        LandscapePackage, DuplicatedLandscape,
        HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
        FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
        LandscapePackage, DuplicatedLandscape,
        HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *LandscapeName);

        // Do we need to duplicate the landscape material and hole material as well?
        UMaterialInterface* MaterialInterface = nullptr;
        for ( int32 Idx = 0; Idx < 2; Idx++ )
        {
            if (Idx == 0)
                MaterialInterface = Landscape->LandscapeMaterial;
            else
                MaterialInterface = Landscape->LandscapeHoleMaterial;

            if ( !MaterialInterface )
                continue;

            UPackage * MaterialPackage = Cast< UPackage >( MaterialInterface->GetOuter() );
            if ( !MaterialPackage )
                continue;

            FString MaterialName;
            if (FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(
                MaterialPackage, MaterialInterface, MaterialName))
            {
                // We only deal with materials.
                UMaterial * Material = Cast< UMaterial >(MaterialInterface);
                if ( Material )
                {
                    // Duplicate material resource.
                    UMaterial * DuplicatedMaterial = FHoudiniEngineUtils::DuplicateMaterialAndCreatePackage(
                        Material, Component, MaterialName, FHoudiniEngineUtils::GetMaterialAndTextureCookMode() );

                    if ( !DuplicatedMaterial )
                        continue;

                    if ( Idx == 0 )
                        DuplicatedLandscape->LandscapeMaterial = DuplicatedMaterial;
                    else
                        DuplicatedLandscape->LandscapeHoleMaterial = DuplicatedMaterial;
                }
            }
        }

        // We also need to bake the landscape layers

        // Notify registry that we have created a new duplicate mesh.
        FAssetRegistryModule::AssetCreated( DuplicatedLandscape );

        // Dirty the static mesh package.
        DuplicatedLandscape->MarkPackageDirty();
    }

    return DuplicatedLandscape;
}
*/

