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


FHoudiniAssetObjectGeoPart::FHoudiniAssetObjectGeoPart(const TArray<int32>& InIndices, UHoudiniAssetMaterial* InMaterial) :
	Material(InMaterial),
	HoudiniMeshIndexBuffer(nullptr)
{
	Indices = InIndices;
}


FHoudiniAssetObjectGeoPart::~FHoudiniAssetObjectGeoPart()
{
	if(HoudiniMeshIndexBuffer)
	{
		check(!HoudiniMeshIndexBuffer->IsInitialized());
		delete(HoudiniMeshIndexBuffer);
	}

	if(Material)
	{
		Material->RemoveFromRoot();
	}
}


void
FHoudiniAssetObjectGeoPart::Serialize(FArchive& Ar)
{
	// Serialize number of indices.
	int32 NumIndices = Indices.Num();
	Ar << NumIndices;

	if(NumIndices)
	{
		for(int32 Idx = 0; Idx < NumIndices; ++Idx)
		{
			int32 Index = -1;

			if(Ar.IsSaving())
			{
				Index = Indices[Idx];
			}

			Ar << Index;

			if(Ar.IsLoading())
			{
				Indices.Add(Index);
			}
		}
	}

	// Serialize bounding volume.
	Ar << BoundingVolume;

	// Serialize Material information.
	//bool MaterialPresent = (Material != nullptr);
	bool MaterialPresent = false;
	Ar << MaterialPresent;
}


void
FHoudiniAssetObjectGeoPart::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Add reference to material.
	if(Material)
	{
		Collector.AddReferencedObject(Material);
	}
}


void
FHoudiniAssetObjectGeoPart::CreateRenderingResources()
{
	// Check to make sure we are not running this twice.
	check(!HoudiniMeshIndexBuffer);

	// Create new index buffer.
	HoudiniMeshIndexBuffer = new FHoudiniMeshIndexBuffer();
	HoudiniMeshIndexBuffer->Indices = Indices;

	// Enqueue initialization of index buffer on render thread.
	BeginInitResource(HoudiniMeshIndexBuffer);
}


void
FHoudiniAssetObjectGeoPart::ReleaseRenderingResources()
{
	// Enqueue release of index buffer and vertex factory on render thread.
	if(HoudiniMeshIndexBuffer)
	{
		BeginReleaseResource(HoudiniMeshIndexBuffer);
	}
}


void
FHoudiniAssetObjectGeoPart::CollectTextures(TArray<UTexture2D*>& InTextures)
{
	if(Material)
	{
		InTextures.Append(Material->GeneratedTextures);
	}
}
