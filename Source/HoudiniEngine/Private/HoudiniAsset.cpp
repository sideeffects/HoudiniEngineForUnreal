/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
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

#include "HoudiniEnginePrivatePCH.h"


UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP) : 
	Super(PCIP),
	AssetBytes(nullptr),
	AssetBytesCount(0)
{

}


UHoudiniAsset::UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, 
							 const uint8*& BufferStart, 
							 const uint8* BufferEnd,
							 const FString& InFileName) : 
	Super(PCIP),
	OTLFileName(InFileName),
	AssetBytes(nullptr),
	AssetBytesCount(0)
{
	// Calculate buffer size.
	AssetBytesCount = BufferEnd - BufferStart;

	if(AssetBytesCount)
	{
		// Allocate buffer to store OTL raw data.
		AssetBytes = static_cast<uint8*>(FMemory::Malloc(AssetBytesCount));

		if(AssetBytes)
		{
			// Copy data into a newly allocated buffer.
			FMemory::Memcpy(AssetBytes, BufferStart, AssetBytesCount);
		}
	}
}


const uint8*
UHoudiniAsset::GetAssetBytes() const
{
	return AssetBytes;
}


uint32
UHoudiniAsset::GetAssetBytesCount() const
{
	return AssetBytesCount;
}


bool
UHoudiniAsset::ContainsPreviewTriangles() const
{
	return PreviewHoudiniMeshTriangles.Num() > 0;
}


const TArray<FHoudiniMeshTriangle>&
UHoudiniAsset::GetPreviewTriangles() const
{
	return PreviewHoudiniMeshTriangles;
}


void 
UHoudiniAsset::SetPreviewTriangles(const TArray<FHoudiniMeshTriangle>& triangles)
{
	PreviewHoudiniMeshTriangles = TArray<FHoudiniMeshTriangle>(triangles);
}


void
UHoudiniAsset::FinishDestroy()
{
	FThumbnailRenderingInfo* RenderInfo = UThumbnailManager::Get().GetRenderingInfo(this);
	if(RenderInfo)
	{
		UHoudiniAssetThumbnailRenderer* ThumbnailRenderer = CastChecked<UHoudiniAssetThumbnailRenderer>(RenderInfo->Renderer);
		if(ThumbnailRenderer)
		{
			ThumbnailRenderer->RemoveAssetThumbnail(this);
		}
	}

	// Release buffer which was used to store raw OTL data.
	if(AssetBytes)
	{
		FMemory::Free(AssetBytes);
		AssetBytes = nullptr;
		AssetBytesCount = 0;
	}

	Super::FinishDestroy();
}


void
UHoudiniAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Properties will get serialized.

	{
		Ar << AssetBytesCount;

		if(Ar.IsLoading())
		{
			// If buffer was previously used, release it.
			if(AssetBytes)
			{
				FMemory::Free(AssetBytes);
				AssetBytes = nullptr;
			}

			// Allocate sufficient space to read stored raw OTL data.
			if(AssetBytesCount)
			{
				AssetBytes = static_cast<uint8*>(FMemory::Malloc(AssetBytesCount));
			}
		}

		if(AssetBytes && AssetBytesCount)
		{
			Ar.Serialize(AssetBytes, AssetBytesCount);
		}
	}

	{
		if(Ar.IsSaving())
		{
			int PreviewTriangleCount = PreviewHoudiniMeshTriangles.Num();
			Ar << PreviewTriangleCount;

			for(int TriIndex = 0; TriIndex < PreviewHoudiniMeshTriangles.Num(); ++TriIndex)
			{
				FHoudiniMeshTriangle& Triangle = PreviewHoudiniMeshTriangles[TriIndex];
				Ar << Triangle;
			}
		}
		else if(Ar.IsLoading())
		{
			int PreviewTriangleCount = 0;
			Ar << PreviewTriangleCount;

			for(int TriIndex = 0; TriIndex < PreviewTriangleCount; ++TriIndex)
			{
				FHoudiniMeshTriangle Triangle;
				Ar << Triangle;

				PreviewHoudiniMeshTriangles.Add(Triangle);
			}
		}
	}
}
