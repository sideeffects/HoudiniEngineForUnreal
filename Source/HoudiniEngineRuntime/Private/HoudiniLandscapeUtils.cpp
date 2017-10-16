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

#include "HoudiniApi.h"
#include "HoudiniLandscapeUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"
#include "HoudiniCookHandler.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"

#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "LightMap.h"
#include "Engine/MapBuildDataRegistry.h"
#if WITH_EDITOR
    #include "FileHelpers.h"
    #include "EngineUtils.h"
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
    FVector LandscapeResizeFactor = FVector::OneVector;
    FVector LandscapePositionOffsetInPixels = FVector::ZeroVector;

    if ( !FHoudiniLandscapeUtils::ResizeHeightDataForLandscape(
        IntHeightData, XSize, YSize,
        NumSectionPerLandscapeComponent,
        NumQuadsPerLandscapeSection,
        LandscapeResizeFactor, LandscapePositionOffsetInPixels ) )
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
    {
        HAPI_Transform HapiTransform = HeightfieldVolumeInfo.transform;

        FQuat ObjectRotation(
            HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[1],
            HapiTransform.rotationQuaternion[2], -HapiTransform.rotationQuaternion[3] );
        Swap( ObjectRotation.Y, ObjectRotation.Z );

        // Unreal XYZ becomes Houdini ZXY (since heightfields are also rotated due the ZX transform) 
        FVector ObjectTranslation( HapiTransform.position[2], HapiTransform.position[0], HapiTransform.position[1] );
        ObjectTranslation *= 100.0f;

        FVector ObjectScale3D( HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2] );
        //Swap( ObjectScale3D.Y, ObjectScale3D.Z );
        //Swap( ObjectScale3D.X, ObjectScale3D.Y );

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
    TArray<uint8>& LayerData )
{
    int32 LayerXSize = HoudiniYSize;
    int32 LayerYSize = HoudiniXSize;

    // Convert the float data to uint8
    LayerData.SetNumUninitialized( LayerXSize * LayerYSize );

    // Calculating the factor used to convert from Houdini's ZRange to [0 255]
    double LayerZRange = ( LayerMax - LayerMin );
    double LayerZSpacing = ( LayerZRange != 0.0 ) ? ( 255.0 / (double)( LayerZRange ) ) : 0.0;

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

bool
FHoudiniLandscapeUtils::GetNonWeightBlendedLayerNames( const FHoudiniGeoPartObject& HeightfieldGeoPartObject, TArray<FString>& NonWeightBlendedLayerNames )
{
    // See if we can find the NonWeightBlendedLayer prim attribute on the heightfield
    HAPI_NodeId HeightfieldNodeId = HeightfieldGeoPartObject.HapiGeoGetNodeId();

    HAPI_PartInfo PartInfo;
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
//	Helpers function for FHoudiniEngineUtils::ResizeHeightDataForLandscape
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
FHoudiniLandscapeUtils::ResizeHeightDataForLandscape(
    TArray<uint16>& HeightData,
    int32& SizeX, int32& SizeY,
    int32& NumberOfSectionsPerComponent,
    int32& NumberOfQuadsPerSection,
    FVector& LandscapeResizeFactor,
    FVector& LandscapePositionOffset )
{
    LandscapeResizeFactor = FVector::OneVector;
    LandscapePositionOffset = FVector::ZeroVector;

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
        ComponentsCountX = FMath::Clamp( ComponentsCountX, 1, FMath::Min( 32, FMath::FloorToInt( 8191 / ( NumberOfSectionsPerComponent * NumberOfQuadsPerSection ) ) ) );
        ComponentsCountY = FMath::Clamp( ComponentsCountY, 1, FMath::Min( 32, FMath::FloorToInt( 8191 / ( NumberOfSectionsPerComponent * NumberOfQuadsPerSection ) ) ) );
    };

    // Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
    bool bFoundMatch = false;
    for ( int32 SectionSizesIdx = ARRAY_COUNT( SectionSizes ) - 1; SectionSizesIdx >= 0; SectionSizesIdx-- )
    {
        for ( int32 NumSectionsIdx = ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx-- )
        {
            int32 ss = SectionSizes[ SectionSizesIdx ];
            int32 ns = NumSections[ NumSectionsIdx ];

            if ( ( ( SizeX - 1 ) % ( ss * ns ) ) == 0 && ( ( SizeX - 1 ) / ( ss * ns ) ) <= 32 &&
                ( ( SizeY - 1 ) % ( ss * ns ) ) == 0 && ( ( SizeY - 1 ) / ( ss * ns ) ) <= 32 )
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

#if WITH_EDITOR
bool
FHoudiniLandscapeUtils::CreateHeightfieldFromLandscape(
    ALandscapeProxy* LandscapeProxy, const HAPI_NodeId& InputMergeNodeId, TArray< HAPI_NodeId >& OutCreatedNodeIds )
{
    if ( !LandscapeProxy )
        return false;

    // Export the whole landscape and its layer as a single heightfield
    ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
    if ( !Landscape )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 1. Extracting the height data
    //--------------------------------------------------------------------------------------------------
    TArray<uint16> HeightData;
    int32 XSize, YSize;
    FVector Min, Max;

    if ( !GetLandscapeData( Landscape, HeightData, XSize, YSize, Min, Max ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 2. Convert the height uint16 data to float
    //--------------------------------------------------------------------------------------------------
    TArray<float> HeightfieldFloatValues;
    HAPI_VolumeInfo HeightfieldVolumeInfo;
    FTransform LandscapeTransform = Landscape->LandscapeActorToWorld();

    if ( !ConvertLandscapeDataToHeightfieldData(
        HeightData, XSize, YSize, Min, Max, LandscapeTransform,
        HeightfieldFloatValues, HeightfieldVolumeInfo ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 3. Set the HeightfieldData in Houdini
    //--------------------------------------------------------------------------------------------------    
    HAPI_NodeId VolumeNodeId = -1;
    HAPI_PartId PartId = 0;
    FString Name = TEXT("height");
    if ( !CreateVolumeInputNode( VolumeNodeId, Name ) )
        return false;

    // Add the node to the array so we can destroy it later
    OutCreatedNodeIds.Add( VolumeNodeId );

    if ( !SetHeighfieldData( VolumeNodeId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, Name ) )
        return false;

    // Commit the landscape and connect it to the parent merge node
    int32 MergeInputIndex = 0;
    if ( !CommitVolumeInputNode(VolumeNodeId, InputMergeNodeId, MergeInputIndex++ ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 4. Extract and convert all the layers
    //--------------------------------------------------------------------------------------------------
    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    if ( !LandscapeInfo )
        return false;

    bool bMaskCreated = false;
    int32 NumLayers = LandscapeInfo->Layers.Num();    
    for ( int32 n = 0; n < NumLayers; n++ )
    {
        // 1. Extract the uint8 values from the layer
        TArray<uint8> CurrentLayerIntData;
        FLinearColor LayerUsageDebugColor;
        FString LayerName;
        if ( !GetLandscapeLayerData( LandscapeInfo, n, CurrentLayerIntData, LayerUsageDebugColor, LayerName ) )
            continue;

        // 2. Convert unreal uint8 to float
        // If the layer came from Houdini, additionnal info might have been stored in the DebugColor
        HAPI_VolumeInfo CurrentLayerVolumeInfo;
        TArray < float > CurrentLayerFloatData;
        if ( !ConvertLandscapeLayerDataToHeightfieldData(
            CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
            CurrentLayerFloatData, CurrentLayerVolumeInfo ) )
            continue;

        // We reuse the height's transform
        CurrentLayerVolumeInfo.transform = HeightfieldVolumeInfo.transform;

        HAPI_NodeId LayerVolumeNodeId = -1;
        if ( !CreateVolumeInputNode( LayerVolumeNodeId, LayerName ) )
            continue;

        // Add the node to the array so we can destroy it later
        OutCreatedNodeIds.Add( LayerVolumeNodeId );

        // 3. Set the heighfield data in Houdini
        HAPI_PartId CurrentPartId = 0;
        if ( !SetHeighfieldData( LayerVolumeNodeId, PartId, CurrentLayerFloatData, CurrentLayerVolumeInfo, LayerName ) )
            continue;

        if ( !CommitVolumeInputNode( LayerVolumeNodeId, InputMergeNodeId, MergeInputIndex ) )
            continue;

        MergeInputIndex++;

        // Was the mask added?
        if ( LayerName == TEXT("mask") )
            bMaskCreated = true;
    }

    // We need to have a mask layer as it is required for proper heightfield functionalities
    // If we didn't create one, we'll send a fully measured mask now
    if ( !bMaskCreated )
    {
        HAPI_NodeId MaskNodeId = -1;
        bMaskCreated = CreateDefaultHeightfieldMask( HeightfieldVolumeInfo, InputMergeNodeId, MergeInputIndex, MaskNodeId );

        MergeInputIndex++;

        OutCreatedNodeIds.Add( MaskNodeId );
    }

    if ( !bMaskCreated )
        return false;

    return true;
}

bool
FHoudiniLandscapeUtils::CreateHeightfieldFromLandscapeComponentArray(
    ALandscapeProxy* LandscapeProxy, TSet< ULandscapeComponent * >& LandscapeComponentArray, 
    const HAPI_NodeId& InputMergeNodeId, TArray< HAPI_NodeId >& OutCreatedNodeIds)
{
    if ( LandscapeComponentArray.Num() <= 0 )
        return false;

    //--------------------------------------------------------------------------------------------------
    //  Each selected component will be exported as a heightfield volume
    //--------------------------------------------------------------------------------------------------    
    int32 MergeInputIndex = 0;
    bool bAllComponentCreated = true;
    for (int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++)
    {
        ULandscapeComponent * CurrentComponent = LandscapeProxy->LandscapeComponents[ ComponentIdx ];
        if ( !CurrentComponent )
            continue;

        if ( !LandscapeComponentArray.Contains( CurrentComponent ) )
            continue;

        if ( !CreateHeightfieldFromLandscapeComponent( CurrentComponent, InputMergeNodeId, OutCreatedNodeIds, MergeInputIndex ) )
            bAllComponentCreated = false;
    }

    return bAllComponentCreated;
}


bool
FHoudiniLandscapeUtils::CreateHeightfieldFromLandscapeComponent(
    ULandscapeComponent * LandscapeComponent, const HAPI_NodeId& InputMergeNodeId,
    TArray< HAPI_NodeId >& OutCreatedNodeIds, int32& MergeInputIndex )
{
    if ( !LandscapeComponent )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 1. Extracting the height data
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
    // 2. Convert the height uint16 data to float
    //--------------------------------------------------------------------------------------------------
    TArray<float> HeightfieldFloatValues;
    HAPI_VolumeInfo HeightfieldVolumeInfo;

    FTransform LandscapeTransform = LandscapeComponent->GetComponentTransform();
    FVector Position = LandscapeTransform.GetLocation();
    Swap( Position.X, Position.Y );
    LandscapeTransform.SetLocation( Position );

    if ( !ConvertLandscapeDataToHeightfieldData(
        HeightData, XSize, YSize, Min, Max, LandscapeTransform,
        HeightfieldFloatValues, HeightfieldVolumeInfo ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 3. Set the HeightfieldData in Houdini
    //--------------------------------------------------------------------------------------------------
    HAPI_NodeId VolumeNodeId = -1;
    HAPI_PartId PartId = 0;
    FString Name = TEXT( "height" );
    if ( !CreateVolumeInputNode( VolumeNodeId, Name ) )
        return false;

    // Add the node to the array so we can destroy it later
    OutCreatedNodeIds.Add( VolumeNodeId );

    if ( !SetHeighfieldData( VolumeNodeId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, Name ) )
        return false;

    // Commit the landscape and connect it to the parent merge node
    if ( !CommitVolumeInputNode( VolumeNodeId, InputMergeNodeId, MergeInputIndex++ ) )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 4. Extract and convert all the layers
    //--------------------------------------------------------------------------------------------------
    bool bMaskCreated = false;
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
        TArray < float > CurrentLayerFloatData;
        if ( !ConvertLandscapeLayerDataToHeightfieldData(
            CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
            CurrentLayerFloatData, CurrentLayerVolumeInfo ) )
            continue;

        // We reuse the height's transform
        CurrentLayerVolumeInfo.transform = HeightfieldVolumeInfo.transform;

        HAPI_NodeId LayerVolumeNodeId = -1;
        if ( !CreateVolumeInputNode( LayerVolumeNodeId, LayerName ) )
            continue;

        // Add the node to the array so we can destroy it later
        OutCreatedNodeIds.Add( LayerVolumeNodeId );

        // 3. Set the heighfield data in Houdini
        HAPI_PartId CurrentPartId = 0;
        if ( !SetHeighfieldData( LayerVolumeNodeId, CurrentPartId, CurrentLayerFloatData, CurrentLayerVolumeInfo, LayerName ) )
            continue;

        if ( !CommitVolumeInputNode( LayerVolumeNodeId, InputMergeNodeId, MergeInputIndex ) )
            continue;

        MergeInputIndex++;

        // Was the mask added?
        if ( LayerName == TEXT( "mask" ) )
            bMaskCreated = true;
    }

    // We need to have a mask layer as it is required for proper heightfield functionnalities
    // If we didn't create one, we'll send a fully measured mask now
    if ( !bMaskCreated )
    {
        HAPI_NodeId MaskNodeId = -1;
        bMaskCreated = CreateDefaultHeightfieldMask( HeightfieldVolumeInfo, InputMergeNodeId, MergeInputIndex, MaskNodeId );

        MergeInputIndex++;

        OutCreatedNodeIds.Add( MaskNodeId );
    }

    if ( !bMaskCreated )
        return false;

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
    FVector Origin, Extent;
    Landscape->GetActorBounds(false, Origin, Extent);

    Min = Origin - Extent;
    Max = Origin + Extent;

    return true;
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
    HAPI_VolumeInfo& HeightfieldVolumeInfo)
{
    HeightfieldFloatValues.Empty();

    int32 HoudiniXSize = YSize;
    int32 HoudiniYSize = XSize;
    int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
    if ( (HoudiniXSize < 2) || (HoudiniYSize < 2) )
        return false;

    if ( IntHeightData.Num() != SizeInPoints )
        return false;

    //--------------------------------------------------------------------------------------------------
    // 1. Convert values to float
    //--------------------------------------------------------------------------------------------------

    // We need the ZMin / ZMax int16 value
    uint16 IntMin = IntHeightData[ 0 ];
    uint16 IntMax = IntMin;
    for ( int32 n = 0; n < IntHeightData.Num(); n++ )
    {
        if ( IntHeightData[ n ] < IntMin )
            IntMin = IntHeightData[ n ];
        if ( IntHeightData[ n ] > IntMax )
            IntMax = IntHeightData[ n ];
    }

    // The range in Digits
    double DigitRange = (double)IntMax - (double)IntMin;

    // Convert the min/max values from cm to meters
    Min /= 100.0;
    Max /= 100.0;

    // The desired range in meter
    double ZMin = (double)Min.Z; 
    double FloatRange = (double)Max.Z - ZMin;

    // The factor used to convert from unreal digit range to Houdini's float Range
    double ZSpacing = ( DigitRange != 0.0 ) ? ( FloatRange / DigitRange ) : 0.0;

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
            double DoubleValue = ( (double)IntHeightData[ nUnreal ] - (double)IntMin ) * ZSpacing + ZMin;
            HeightfieldFloatValues[ nHoudini ] = (float)DoubleValue;
        }
    }
    
    // Verifying the converted ZMin / ZMax
    float FloatMin = HeightfieldFloatValues[ 0 ];
    float FloatMax = FloatMin;

    for ( int32 n = 0; n < HeightfieldFloatValues.Num(); n++ )
    {
        if (HeightfieldFloatValues[n] < FloatMin)
            FloatMin = HeightfieldFloatValues[n];
        if (HeightfieldFloatValues[n] > FloatMax)
            FloatMax = HeightfieldFloatValues[n];
    }

    //--------------------------------------------------------------------------------------------------
    // 2. Convert the Unreal Transform to a HAPI_transform
    //--------------------------------------------------------------------------------------------------
    HAPI_Transform HapiTransform;
    FMemory::Memzero< HAPI_Transform >( HapiTransform );
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
        FVector CenterOffset = ( Max - Min ) * 0.5f;

        FVector Position = LandscapeTransform.GetLocation() / 100.0f;
        HapiTransform.position[ 0 ] = Position.X + CenterOffset.X;
        HapiTransform.position[ 1 ] = Position.Y + CenterOffset.Y;
        HapiTransform.position[ 2 ] = 0.0f;

        FVector Scale = LandscapeTransform.GetScale3D() / 100.0f;
        HapiTransform.scale[ 0 ] = Scale.X * 0.5f * HoudiniXSize;
        HapiTransform.scale[ 1 ] = Scale.Y * 0.5f * HoudiniYSize;
        HapiTransform.scale[ 2 ] = 0.5f;

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
    const HAPI_NodeId& AssetId, const HAPI_PartId& PartId,
    TArray<float>& FloatValues,
    const HAPI_VolumeInfo& VolumeInfo,
    const FString& HeightfieldName )
{
    // Creating the part and sending the data through HAPI
    HAPI_PartInfo Part;
    FMemory::Memzero< HAPI_PartInfo >(Part);
    Part.id = PartId;
    Part.nameSH = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_POINT ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_PRIM ] = 1;
    Part.attributeCounts[ HAPI_ATTROWNER_VERTEX ] = 0;
    Part.attributeCounts[ HAPI_ATTROWNER_DETAIL ] = 0;
    Part.pointCount = 0; 
    Part.vertexCount = 0;
    Part.faceCount = 1;
    Part.type = HAPI_PARTTYPE_VOLUME;

    HAPI_GeoInfo DisplayGeoInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &DisplayGeoInfo ), false );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetPartInfo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, Part.id, &Part ), false );

    // Set the VolumeInfo
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetVolumeInfo(
        FHoudiniEngine::Get().GetSession(),
        DisplayGeoInfo.nodeId, Part.id, &VolumeInfo ), false );

    // Volume name
    std::string NameStr;
    FHoudiniEngineUtils::ConvertUnrealString( HeightfieldName, NameStr );
    
    // Set Heighfield data
    float * HeightData = FloatValues.GetData();
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetHeightFieldData(
        FHoudiniEngine::Get().GetSession(),
        DisplayGeoInfo.nodeId, Part.id, NameStr.c_str() , HeightData, 0, FloatValues.Num() ), false );

    return true;
}

bool
FHoudiniLandscapeUtils::CreateHeightfieldInputNode( HAPI_NodeId& DisplayNodeId, HAPI_NodeId& MergeNodeId, const FString& NodeName )
{
    // Node already exists
    if ( DisplayNodeId != -1 )
        return false;

    // Converting the Node Name
    std::string NameStr;
    FHoudiniEngineUtils::ConvertUnrealString( NodeName, NameStr );

    // ---------------------------------------------------------
    // Start by creating a Transform node to set the heightfield orientation to ZX
    // this will be our display SOP
    HAPI_NodeId TransformId = -1;
    //FHoudiniEngineUtils::ConvertUnrealString(TEXT("ZX"), NameStr);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), -1,
        "SOP/xform", NameStr.c_str(), true, &TransformId), false);

    // And set the rx ry parameters
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(
        FHoudiniEngine::Get().GetSession(),
        TransformId, "rx", 0, -90.0f), false);

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(
        FHoudiniEngine::Get().GetSession(),
        TransformId, "rx", 1, -90.0f), false);

    // Cook it
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), TransformId, nullptr), false);

    // We need to get the xform NodeInfo to get its parent id
    HAPI_NodeInfo NodeInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), TransformId, &NodeInfo), false);

    // ---------------------------------------------------------
    // Now, create a volvis node
    HAPI_NodeId VolVisId = -1;
    FHoudiniEngineUtils::ConvertUnrealString(TEXT("volumevisualization"), NameStr);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), NodeInfo.parentId,
        "volumevisualization", NameStr.c_str(), false, &VolVisId), false);

    // Finally we need to setup the volvis node for heightfields
    // Change the vismode parameter to heightfield
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
        FHoudiniEngine::Get().GetSession(),
        VolVisId, "vismode", 0, 2), false);

    // And the density field parameter to "height"
    //const std::string sDensityField = "densityfield";
    HAPI_ParmId densId = FHoudiniEngineUtils::HapiFindParameterByNameOrTag(VolVisId, "densityfield");
    if (densId != -1)
    {
        HAPI_ParmInfo ParmInfo;
        FMemory::Memset< HAPI_ParmInfo >(ParmInfo, 0);
        if (FHoudiniApi::GetParmInfo(
            FHoudiniEngine::Get().GetSession(), VolVisId,
            densId, &ParmInfo) == HAPI_RESULT_SUCCESS)
        {
            const std::string sHeight = "height";
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
                FHoudiniEngine::Get().GetSession(),
                VolVisId, sHeight.c_str(), densId, 0), false);
        }
    }

    // And the diffuse field parameter to "mask"
    //const std::string sDiffuseField = "cdfield";
    HAPI_ParmId diffId = FHoudiniEngineUtils::HapiFindParameterByNameOrTag(VolVisId, "cdfield");
    if (diffId != -1)
    {
        HAPI_ParmInfo ParmInfo;
        FMemory::Memset< HAPI_ParmInfo >(ParmInfo, 0);
        if (FHoudiniApi::GetParmInfo(
            FHoudiniEngine::Get().GetSession(), VolVisId,
            diffId, &ParmInfo) == HAPI_RESULT_SUCCESS)
        {
            const std::string sMask = "mask";
            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
                FHoudiniEngine::Get().GetSession(),
                VolVisId, sMask.c_str(), diffId, 0), false);
        }
    }

    // Cook it
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), VolVisId, nullptr), false);

    // ---------------------------------------------------------
    // We can now create the merge node, this will our InAssetId since
    // we will be connecting inputs to it
    HAPI_NodeId MergeId = -1;
    FHoudiniEngineUtils::ConvertUnrealString(TEXT("Heightfield Merge"), NameStr);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), NodeInfo.parentId,
        "merge", NameStr.c_str(), false, &MergeId), false);

    // Cook it
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), MergeId, nullptr), false);

    // ---------------------------------------------------------
    // Then connect the volvis to the xform node input
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), TransformId, 0, VolVisId), false);

    // Cook it
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), TransformId, nullptr), false);

    // Then connect the merge to the volvis node input
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
        FHoudiniEngine::Get().GetSession(), VolVisId, 0, MergeId), false);

    // Cook it
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), VolVisId, nullptr), false);

    // The xform node ID will be our connected asset ID
    DisplayNodeId = TransformId;
    // We need to keep the merge node ID to connect inputs to it later on
    MergeNodeId = MergeId;

    return true;
}

bool
FHoudiniLandscapeUtils::CreateVolumeInputNode( HAPI_NodeId& InAssetId, const FString& NodeName )
{
    // The node cannot already exist
    if ( InAssetId >= 0 )
        return false;

    // Volume name?
    std::string NameStr;
    FHoudiniEngineUtils::ConvertUnrealString( NodeName, NameStr );

    HAPI_NodeId AssetId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputNode(
        FHoudiniEngine::Get().GetSession(), &AssetId, NameStr.c_str() ), false);

    // Check if we have a valid id for this new input asset.
    if ( !FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) )
        return false;

    // We now have a valid id.
    InAssetId = AssetId;

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), AssetId, nullptr ), false );

    return true;
}

bool
FHoudiniLandscapeUtils::CommitVolumeInputNode(
    const HAPI_NodeId& NodeToCommit, const HAPI_NodeId& NodeToConnectTo, const int32& InputToConnect )
{
    HAPI_GeoInfo DisplayGeoInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
        FHoudiniEngine::Get().GetSession(), NodeToCommit, &DisplayGeoInfo ), false );

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
        FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId ), false );

    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), NodeToCommit, nullptr ), false );

    if ( NodeToConnectTo != -1 && InputToConnect >= 0 )
    {
        // Now we can connect the input node to the asset node.
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), NodeToConnectTo, InputToConnect,
            NodeToCommit ), false );
    }

    return true;
}

bool
FHoudiniLandscapeUtils::CreateDefaultHeightfieldMask(
    const HAPI_VolumeInfo& HeightVolumeInfo,
    const HAPI_NodeId& AssetId,
    int32& MergeInputIndex,
    HAPI_NodeId& OutCreatedNodeId )
{
    // We need to have a mask layer as it is required for proper heightfield functionalities
    // If we didn't create one, we'll send a fully measured mask now

    // Creating an array filled with 1.0
    TArray< float > MaskFloatData;
    MaskFloatData.SetNumUninitialized( HeightVolumeInfo.xLength * HeightVolumeInfo.yLength );
    for ( int32 n = 0; n < ( HeightVolumeInfo.xLength * HeightVolumeInfo.yLength ); n++ )
        MaskFloatData[ n ] = 1.0;

    // Creating the volume infos
    HAPI_VolumeInfo MaskVolumeInfo = HeightVolumeInfo;

    // Creating the mask node
    FString MaskName = TEXT( "mask" );
    HAPI_NodeId MaskVolumeNodeId = -1;
    if ( !CreateVolumeInputNode( MaskVolumeNodeId, MaskName ) )
        return false;

    OutCreatedNodeId = MaskVolumeNodeId;

    // Set the heighfield data in Houdini
    HAPI_PartId PartId = 0;
    if ( !SetHeighfieldData( MaskVolumeNodeId, PartId, MaskFloatData, MaskVolumeInfo, MaskName ) )
        return false;

    if ( !CommitVolumeInputNode(MaskVolumeNodeId, AssetId, MergeInputIndex ) )
        return false;

    MergeInputIndex++;

    return true;
}

bool
FHoudiniLandscapeUtils::DestroyLandscapeAssetNode( HAPI_NodeId& ConnectedAssetId, TArray<HAPI_NodeId>& CreatedInputAssetIds )
{
    HAPI_AssetInfo NodeAssetInfo;
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &NodeAssetInfo ), false );

    FHoudiniEngineString AssetOpName( NodeAssetInfo.fullOpNameSH );
    FString OpName;
    if ( !AssetOpName.ToFString( OpName ) )
        return false;

    if ( !OpName.Contains( TEXT( "volumevisualization") ) )
    {
        // Not a volvis node, so not a Heightfield
        // We just need to destroy the landscape asset node
        return FHoudiniEngineUtils::DestroyHoudiniAsset( ConnectedAssetId );
    }

    // The landscape was marshalled as a heightfield, so we need to destroy and disconnect
    // the volvis nodes, all the merge node's input (each merge input is a volume for one 
    // of the layer/mask of the landscape )

    // First, destroy the merge node and its inputs
    // The merge node is in the first input of the volvis node
    HAPI_NodeId MergeNodeId = -1;
    FHoudiniApi::QueryNodeInput(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, 0, &MergeNodeId );

    if ( MergeNodeId != -1 )
    {
        // Get the merge node info
        HAPI_NodeInfo NodeInfo;
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

    // Finally disconnect and destroy the volvis and merge node
    FHoudiniEngineUtils::HapiDisconnectAsset( ConnectedAssetId, 0 );
    FHoudiniEngineUtils::DestroyHoudiniAsset( MergeNodeId );

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

        TArray< uint8 > LightmapMipData;
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointPosition );
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointNormal );
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointUV );
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentVertexIndices );
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentNames );
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLightmapColor );
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

        // If component has an override material, we need to get the raw name (if exporting materials).
        if ( bExportMaterials && LandscapeComponent->OverrideMaterial )
        {
            MaterialRawStr = FHoudiniEngineUtils::ExtractRawName( LandscapeComponent->OverrideMaterial->GetName() );	    
        }

        // If component has an override hole material, we need to get the raw name (if exporting materials).
        if ( bExportMaterials && LandscapeComponent->OverrideHoleMaterial )
        {
            MaterialHoleRawStr = FHoudiniEngineUtils::ExtractRawName( LandscapeComponent->OverrideHoleMaterial->GetName() );
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

    // Get name of attribute used for marshalling materials.
    std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL;
    if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
    {
        FHoudiniEngineUtils::ConvertUnrealString(
            HoudiniRuntimeSettings->MarshallingAttributeMaterial,
            MarshallingAttributeMaterialName );
    }

    // Get name of attribute used for marshalling hole materials.
    std::string MarshallingAttributeMaterialHoleName = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;
    if ( HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterialHole.IsEmpty() )
    {
        FHoudiniEngineUtils::ConvertUnrealString(
            HoudiniRuntimeSettings->MarshallingAttributeMaterialHole,
            MarshallingAttributeMaterialHoleName );
    }

    // Marshall in override primitive material names.
    HAPI_AttributeInfo AttributeInfoPrimitiveMaterial;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterial );
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

    // Marshall in override primitive material hole names.
    HAPI_AttributeInfo AttributeInfoPrimitiveMaterialHole;
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterialHole );
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

    return true;
}

#if WITH_EDITOR
bool
FHoudiniLandscapeUtils::CreateAllLandscapes( 
    FHoudiniCookParams& HoudiniCookParams,
    const TArray< FHoudiniGeoPartObject > & FoundVolumes, 
    TMap< FHoudiniGeoPartObject, ALandscape * >& Landscapes,
    TMap< FHoudiniGeoPartObject, ALandscape * >& NewLandscapes,
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

    // Try to create a Landscape for each HeightData found
    //TMap< FHoudiniGeoPartObject, ALandscape * > NewLandscapes;
    NewLandscapes.Empty();
    for ( TArray< const FHoudiniGeoPartObject* >::TConstIterator IterHeighfields( FoundHeightfields ); IterHeighfields; ++IterHeighfields )
    {
        // Get the current Heightfield GeoPartObject
        const FHoudiniGeoPartObject* CurrentHeightfield = *IterHeighfields;
        if ( !CurrentHeightfield )
            continue;

        bool bLandscapeNeedsRecreate = true;
        if ( !CurrentHeightfield->bHasGeoChanged )
        {
            // The Geo has not changed, do we need to recreate the landscape?
            ALandscape * FoundLandscape = Landscapes.FindChecked( *CurrentHeightfield );
            if ( FoundLandscape )
            {
                // Check that all layers/mask have not changed too
                TArray< const FHoudiniGeoPartObject* > FoundLayers;
                FHoudiniLandscapeUtils::GetHeightfieldsLayersInArray( FoundVolumes, *CurrentHeightfield, FoundLayers );

                bool bLayersHaveChanged = false;
                for ( int32 n = 0; n < FoundLayers.Num(); n++ )
                {
                    if ( FoundLayers[ n ] && FoundLayers[ n ]->bHasGeoChanged )
                    {
                        bLayersHaveChanged = true;
                        break;
                    }
                }

                if ( !bLayersHaveChanged )
                {
                    // Height and layers/masks have not changed, there is no need to reimport the landscape
                    bLandscapeNeedsRecreate = false;

                    // We can add the landscape to the map and remove it from the old one to avoid its destruction
                    NewLandscapes.Add( *CurrentHeightfield, FoundLandscape );
                    Landscapes.Remove( *CurrentHeightfield );
                }
            }
        }

        if ( !bLandscapeNeedsRecreate )
            continue;

        HAPI_NodeId HeightFieldNodeId = CurrentHeightfield->HapiGeoGetNodeId();

        // We need to see if the current heightfield has an unreal_material or unreal_hole_material assigned to it
        UMaterialInterface* LandscapeMaterial = nullptr;
        UMaterialInterface* LandscapeHoleMaterial = nullptr;
        FHoudiniLandscapeUtils::GetHeightFieldLandscapeMaterials( *CurrentHeightfield, LandscapeMaterial, LandscapeHoleMaterial );

        // Extract the Float Data from the Heightfield
        TArray< float > FloatValues;
        HAPI_VolumeInfo VolumeInfo;
        float FloatMin, FloatMax;
        if ( !FHoudiniLandscapeUtils::GetHeightfieldData( *CurrentHeightfield, FloatValues, VolumeInfo, FloatMin, FloatMax ) )
            continue;

        // Do we need to convert the heightfields using the same global Min/Max
        if ( fGlobalMin != fGlobalMax )
        {
            FloatMin = fGlobalMin;
            FloatMax = fGlobalMax;
        }

        // Convert the height data from Houdini's heightfield to Unreal's Landscape
        TArray< uint16 > IntHeightData;
        FTransform LandscapeTransform;
        int32 XSize, YSize, NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection;
        if ( !FHoudiniLandscapeUtils::ConvertHeightfieldDataToLandscapeData(
            FloatValues, VolumeInfo, FloatMin, FloatMax,
            IntHeightData, LandscapeTransform,
            XSize, YSize,
            NumSectionPerLandscapeComponent,
            NumQuadsPerLandscapeSection ) )
            continue;

        // Look for all the layers/masks corresponding to the current heightfield
        TArray< const FHoudiniGeoPartObject* > FoundLayers;
        FHoudiniLandscapeUtils::GetHeightfieldsLayersInArray( FoundVolumes, *CurrentHeightfield, FoundLayers );

        // Extract and convert the Landscape layers
        TArray< FLandscapeImportLayerInfo > ImportLayerInfos;
        if ( !FHoudiniLandscapeUtils::CreateLandscapeLayers( HoudiniCookParams, FoundLayers, *CurrentHeightfield,
            XSize, YSize, ImportLayerInfos ) )
            continue;

        // Create the actual Landscape
        ALandscape * CurrentLandscape = CreateLandscape( 
            IntHeightData, ImportLayerInfos,
            LandscapeTransform, XSize, YSize,
            NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
            LandscapeMaterial, LandscapeHoleMaterial );

        if ( !CurrentLandscape )
            continue;

        // Add the new landscape to the map
        NewLandscapes.Add( *CurrentHeightfield, CurrentLandscape );
    }

    return true;
}

ALandscape *
FHoudiniLandscapeUtils::CreateLandscape(
    const TArray< uint16 >& IntHeightData,
    const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
    const FTransform& LandscapeTransform,
    const int32& XSize, const int32& YSize, 
    const int32& NumSectionPerLandscapeComponent, const int32& NumQuadsPerLandscapeSection,
    UMaterialInterface* LandscapeMaterial, UMaterialInterface* LandscapeHoleMaterial )
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
    ALandscape* Landscape = MyWorld->SpawnActor< ALandscape >();
    if ( !Landscape )
        return nullptr;

    // Create a new GUID
    FGuid currentGUID = FGuid::NewGuid();
    Landscape->SetLandscapeGuid( currentGUID );

    // Set the landscape Transform
    Landscape->SetActorTransform( LandscapeTransform );

    // Autosaving the layers prevents them for being deleted with the Asset
    // Save the packages created for the LayerInfos
    //if ( CreatedLayerInfoPackage.Num() > 0 )
    //    FEditorFileUtils::PromptForCheckoutAndSave( CreatedLayerInfoPackage, true, false );

    // Import the landscape data

    // Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
    Landscape->bCastStaticShadow = false;

    if ( LandscapeMaterial )
        Landscape->LandscapeMaterial = LandscapeMaterial;

    if ( LandscapeHoleMaterial )
        Landscape->LandscapeHoleMaterial = LandscapeHoleMaterial;

    // Setting the layer type here.
    ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

    // Import the data
    Landscape->Import(
        currentGUID,
        0, 0, XSize - 1, YSize - 1,
        NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
        &( IntHeightData[ 0 ] ), NULL,
        ImportLayerInfos, ImportLayerType );

    // Copied straight from UE source code to avoid crash after importing the landscape:
    // automatically calculate a lighting LOD that won't crash lightmass (hopefully)
    // < 2048x2048 -> LOD0,  >=2048x2048 -> LOD1,  >= 4096x4096 -> LOD2,  >= 8192x8192 -> LOD3
    Landscape->StaticLightingLOD = FMath::DivideAndRoundUp( FMath::CeilLogTwo( ( XSize * YSize ) / ( 2048 * 2048 ) + 1 ), ( uint32 )2 );

    // Register all the landscape components
    Landscape->RegisterAllComponents();

    return Landscape;
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
    FMemory::Memset< HAPI_AttributeInfo >( AttribMaterials, 0 );
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
    FMemory::Memset< HAPI_AttributeInfo >( AttribMaterials, 0 );
    
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

        // Creating the ImportLayerInfo and LayerInfo objects
        FString LayerString;
        FHoudiniEngineString( LayerVolumeInfo.nameSH ).ToFString( LayerString );
        ObjectTools::SanitizeObjectName( LayerString );

        FName LayerName( *LayerString );
        FLandscapeImportLayerInfo currentLayerInfo( LayerName );

        UPackage * Package;
        currentLayerInfo.LayerInfo = FHoudiniLandscapeUtils::CreateLandscapeLayerInfoObject( HoudiniCookParams, LayerString.GetCharArray().GetData(), Package );
        if ( !currentLayerInfo.LayerInfo || !Package )
            continue;

        // Convert the float data to uint8
        if ( !FHoudiniLandscapeUtils::ConvertHeightfieldLayerToLandscapeLayer(
            FloatLayerData, LayerVolumeInfo.xLength, LayerVolumeInfo.yLength,
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

        if ( NonWeightBlendedLayerNames.Contains( LayerString ) )
            currentLayerInfo.LayerInfo->bNoWeightBlend = true;
        else
            currentLayerInfo.LayerInfo->bNoWeightBlend = false;

        // Mark the package dirty...
        //Package->MarkPackageDirty();

        CreatedLandscapeLayerPackage.Add( Package );

        ImportLayerInfos.Add( currentLayerInfo );
    }

    // Autosaving the layers prevents them for being deleted with the Asset
    // Save the packages created for the LayerInfos
    FEditorFileUtils::PromptForCheckoutAndSave( CreatedLandscapeLayerPackage, true, false );

    return true;
}

ULandscapeLayerInfoObject *
FHoudiniLandscapeUtils::CreateLandscapeLayerInfoObject( FHoudiniCookParams& HoudiniCookParams, const TCHAR* LayerName, UPackage*& Package )
{
    // Verifying HoudiniCookParams validity
    if ( !HoudiniCookParams.HoudiniAsset )
        return nullptr;

    FString ComponentGUIDString = HoudiniCookParams.PackageGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    FString LayerNameString = FString::Printf( TEXT( "%s" ), LayerName );
    LayerNameString = PackageTools::SanitizePackageName( LayerNameString );

    // Create the LandscapeInfoObjectName from the Asset name and the mask name
    FName LayerObjectName = FName( * (HoudiniCookParams.HoudiniAsset->GetName() + ComponentGUIDString + TEXT( "_LayerInfoObject_" ) + LayerNameString ) );

    // Save the package in the temp folder
    FString Path = HoudiniCookParams.TempCookFolder.ToString() + TEXT( "/" );
    FString PackageName = Path + LayerObjectName.ToString();
    PackageName = PackageTools::SanitizePackageName( PackageName );

    // See if package exists, if it does, reuse it
    //UPackage * Package = FindPackage( nullptr , *PackageName );
    Package = FindPackage( nullptr, *PackageName );
    if ( !Package )
    {
        // Package does not exists, create it
        Package = CreatePackage( nullptr, *PackageName );
    }

    if ( !Package )
        return nullptr;

    ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>( Package, LayerObjectName, RF_Public | RF_Standalone /*| RF_Transactional*/ );
    LayerInfo->LayerName = LayerName;

    // Notify the asset registry
    FAssetRegistryModule::AssetCreated( LayerInfo );

    // Mark the package dirty...
    Package->MarkPackageDirty();

    return LayerInfo;
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterial );
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
    FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterialHole );
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
FHoudiniLandscapeUtils::UpdateOldLandscapeReference(ALandscape* OldLandscape, ALandscape*  NewLandscape)
{
    if ( !OldLandscape || !NewLandscape )
        return false;

    bool bReturn = false;

    // Iterates through all the Houdini Assets in the scene
    UWorld* editorWorld = GEditor->GetEditorWorldContext().World();
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

#endif