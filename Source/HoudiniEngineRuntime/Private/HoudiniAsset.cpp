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
#include "HoudiniAsset.h"
#include "HoudiniAssetInstance.h"

const uint32
UHoudiniAsset::PersistenceFormatVersion = 2u;

UHoudiniAsset::UHoudiniAsset( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , AssetFileName( TEXT( "" ) )
    , AssetBytes( nullptr )
    , AssetBytesCount( 0 )
    , FileFormatVersion( UHoudiniAsset::PersistenceFormatVersion )
    , HoudiniAssetFlagsPacked ( 0u )
{}

void
UHoudiniAsset::CreateAsset( const uint8 * BufferStart, const uint8 * BufferEnd, const FString & InFileName )
{
    AssetFileName = InFileName;

    // Calculate buffer size.
    AssetBytesCount = BufferEnd - BufferStart;

    if ( AssetBytesCount )
    {
        // Allocate buffer to store OTL raw data.
        AssetBytes = static_cast< uint8 * >( FMemory::Malloc( AssetBytesCount ) );

        if ( AssetBytes )
        {
            // Copy data into a newly allocated buffer.
            FMemory::Memcpy( AssetBytes, BufferStart, AssetBytesCount );
        }
    }

    FString FileExtension = FPaths::GetExtension( InFileName );

    if ( FileExtension.Equals( TEXT( "hdalc" ), ESearchCase::IgnoreCase ) ||
        FileExtension.Equals( TEXT( "otlc" ), ESearchCase::IgnoreCase ) )
    {
        bAssetLimitedCommercial = true;
    }
    else if ( FileExtension.Equals( TEXT( "hdanc" ), ESearchCase::IgnoreCase ) ||
        FileExtension.Equals( TEXT( "otlnc" ), ESearchCase::IgnoreCase ) )
    {
        bAssetNonCommercial = true;
    }
}

const uint8 *
UHoudiniAsset::GetAssetBytes() const
{
    return AssetBytes;
}

const FString &
UHoudiniAsset::GetAssetFileName() const
{
    return AssetFileName;
}

uint32
UHoudiniAsset::GetAssetBytesCount() const
{
    return AssetBytesCount;
}

bool
UHoudiniAsset::IsPreviewHoudiniLogo() const
{
    return bPreviewHoudiniLogo;
}

void
UHoudiniAsset::FinishDestroy()
{
    // Release buffer which was used to store raw OTL data.
    if ( AssetBytes )
    {
        FMemory::Free( AssetBytes );
        AssetBytes = nullptr;
        AssetBytesCount = 0;
    }

    Super::FinishDestroy();
}

void
UHoudiniAsset::Serialize( FArchive & Ar )
{
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    // Properties will get serialized.

    // Serialize persistence format version.
    Ar << FileFormatVersion;

    Ar << AssetBytesCount;

    if ( Ar.IsLoading() )
    {
        // If buffer was previously used, release it.
        if ( AssetBytes )
        {
            FMemory::Free( AssetBytes );
            AssetBytes = nullptr;
        }

        // Allocate sufficient space to read stored raw OTL data.
        if ( AssetBytesCount )
            AssetBytes = static_cast< uint8 * >( FMemory::Malloc( AssetBytesCount ) );
    }

    if ( AssetBytes && AssetBytesCount )
        Ar.Serialize( AssetBytes, AssetBytesCount );

    // Serialize flags.
    Ar << HoudiniAssetFlagsPacked;

    // Serialize asset file path.
    Ar << AssetFileName;
}

void
UHoudiniAsset::GetAssetRegistryTags( TArray< FAssetRegistryTag > & OutTags ) const
{
    OutTags.Add( FAssetRegistryTag( "FileName", AssetFileName, FAssetRegistryTag::TT_Alphabetical ) );
    OutTags.Add(
        FAssetRegistryTag( "FileFormatVersion", FString::FromInt( FileFormatVersion ),
        FAssetRegistryTag::TT_Numerical ) );
    OutTags.Add( FAssetRegistryTag( "Bytes", FString::FromInt( AssetBytesCount ), FAssetRegistryTag::TT_Numerical ) );

    FString AssetType = TEXT( "Full" );

    if ( bAssetLimitedCommercial )
        AssetType = TEXT( "Limited Commercial (LC)" );
    else if( bAssetNonCommercial )
        AssetType = TEXT( "Non Commercial (NC)" );

    OutTags.Add( FAssetRegistryTag( "Asset Type", AssetType, FAssetRegistryTag::TT_Alphabetical ) );

    Super::GetAssetRegistryTags( OutTags );
}

bool
UHoudiniAsset::IsAssetLimitedCommercial() const
{
    return bAssetLimitedCommercial;
}

bool
UHoudiniAsset::IsAssetNonCommercial() const
{
    return bAssetNonCommercial;
}

UHoudiniAssetInstance *
UHoudiniAsset::CreateHoudiniAssetInstance( UObject * Outer )
{
    return UHoudiniAssetInstance::CreateAssetInstance( Outer, this );
}
