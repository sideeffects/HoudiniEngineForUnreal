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


FHoudiniAssetObjectGeo::FHoudiniAssetObjectGeo() :
	HoudiniMeshVertexFactory(nullptr),
	ObjectId(-1),
	GeoId(-1),
	PartId(-1),
	ComponentReferenceCount(1),
	bMultipleMaterials(false),
	bHoudiniLogo(false),
	bRenderingResourcesCreated(false)
{
	Transform = FMatrix::Identity;
	AggregateBoundingVolume = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));
}


FHoudiniAssetObjectGeo::FHoudiniAssetObjectGeo(const FMatrix& InTransform, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId) :
	Transform(InTransform),
	HoudiniMeshVertexFactory(nullptr),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	ComponentReferenceCount(1),
	bMultipleMaterials(false),
	bHoudiniLogo(false),
	bRenderingResourcesCreated(false)
{
	AggregateBoundingVolume = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));
}


FHoudiniAssetObjectGeo::~FHoudiniAssetObjectGeo()
{
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		delete HoudiniAssetObjectGeoPart;
	}

	HoudiniAssetObjectGeoParts.Empty();

	// Delete dangling rendering resources.
	for(TArray<FHoudiniMeshVertexBuffer*>::TIterator Iter = HoudiniMeshVertexBuffers.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = *Iter;
		if(HoudiniMeshVertexBuffer)
		{
			check(!HoudiniMeshVertexBuffer->IsInitialized());
			delete HoudiniMeshVertexBuffer;
		}
	}

	if(HoudiniMeshVertexFactory)
	{
		check(!HoudiniMeshVertexFactory->IsInitialized());
		delete HoudiniMeshVertexFactory;
	}
}


bool
FHoudiniAssetObjectGeo::CheckRenderingResourcesCreated() const
{
	return bRenderingResourcesCreated;
}


const FMatrix&
FHoudiniAssetObjectGeo::GetTransform() const
{
	return Transform;
}


void
FHoudiniAssetObjectGeo::Serialize(FArchive& Ar)
{
	/*
	// Serialize stored vertices.
	{
		// Serialize number of vertices.
		int32 NumVertices = Vertices.Num();
		Ar << NumVertices;

		if(NumVertices)
		{
			for(int32 Idx = 0; Idx < NumVertices; ++Idx)
			{
				if(Ar.IsLoading())
				{
					FHoudiniMeshVertex Vertex;
					Ar << Vertex;
					Vertices.Add(Vertex);
				}
				else if(Ar.IsSaving())
				{
					FHoudiniMeshVertex& Vertex = Vertices[Idx];
					Ar << Vertex;
				}
			}
		}
	}
	*/

	// Serialize transform.
	Ar << Transform;

	// Serialize aggregate bounding volume.
	Ar << AggregateBoundingVolume;

	// Serialize multiple materials flag.
	Ar << bMultipleMaterials;

	// Serialize parts.
	int32 NumParts = HoudiniAssetObjectGeoParts.Num();
	Ar << NumParts;

	for(int32 PartIdx = 0; PartIdx < NumParts; ++PartIdx)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = nullptr;

		if(Ar.IsSaving())
		{
			HoudiniAssetObjectGeoPart = HoudiniAssetObjectGeoParts[PartIdx];
		}
		else if(Ar.IsLoading())
		{
			HoudiniAssetObjectGeoPart = new FHoudiniAssetObjectGeoPart();
		}

		check(HoudiniAssetObjectGeoPart);
		HoudiniAssetObjectGeoPart->Serialize(Ar);

		if(Ar.IsLoading())
		{
			HoudiniAssetObjectGeoParts.Add(HoudiniAssetObjectGeoPart);
		}
	}
}


void
FHoudiniAssetObjectGeo::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Manually add references for each Geo part.
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		HoudiniAssetObjectGeoPart->AddReferencedObjects(Collector);
	}
}


void
FHoudiniAssetObjectGeo::AddGeoPart(FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart)
{
	HoudiniAssetObjectGeoParts.Add(HoudiniAssetObjectGeoPart);
}


void
FHoudiniAssetObjectGeo::ComputeMultipleMaterialUsage()
{
	if(HoudiniAssetObjectGeoParts.Num() > 1)
	{
		UMaterial* FirstMaterial = HoudiniAssetObjectGeoParts[0]->Material;

		for(int32 Idx = 1; Idx < HoudiniAssetObjectGeoParts.Num(); ++Idx)
		{
			if(HoudiniAssetObjectGeoParts[Idx]->Material != FirstMaterial)
			{
				bMultipleMaterials = true;
				break;
			}
		}
	}
}


UMaterial*
FHoudiniAssetObjectGeo::GetSingleMaterial() const
{
	UMaterial* Material = nullptr;

	if(HoudiniAssetObjectGeoParts.Num() > 0)
	{
		for(int32 Idx = 0; Idx < HoudiniAssetObjectGeoParts.Num(); ++Idx)
		{
			UMaterial* FirstMaterial = HoudiniAssetObjectGeoParts[Idx]->Material;

			if(FirstMaterial)
			{
				Material = FirstMaterial;
				break;
			}
		}
	}

	return Material;
}


void
FHoudiniAssetObjectGeo::ReplaceMaterial(UMaterial* Material)
{
	for(int32 Idx = 0; Idx < HoudiniAssetObjectGeoParts.Num(); ++Idx)
	{
		HoudiniAssetObjectGeoParts[Idx]->Material = Material;
	}
}


bool
FHoudiniAssetObjectGeo::UsesMultipleMaterials() const
{
	return bMultipleMaterials;
}


uint32
FHoudiniAssetObjectGeo::GetPositionCount() const
{
	return VertexPositions.Num();
}


void
FHoudiniAssetObjectGeo::CreateRenderingResources()
{
	// Check to make sure we are not running this twice.
	if(bRenderingResourcesCreated)
	{
		// Resources have been initialized, we can skip. This will happen when geos are reused between the cooks.
		return;
	}

	// Create vertex stream for position data.
	if(VertexPositions.Num() > 0)
	{
		uint32 DataBytesCount = sizeof(FVector) * VertexPositions.Num();
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer((const uint8*) VertexPositions.GetTypedData(),
			DataBytesCount, VET_Float3, 3, EHoudiniMeshVertexBufferSemantic::Position);
		HoudiniMeshVertexBuffers.Add(HoudiniMeshVertexBuffer);
	}

	// Create vertex stream for normals.
	if(VertexNormals.Num() > 0)
	{
		uint32 DataBytesCount = sizeof(FVector) * VertexNormals.Num();
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer((const uint8*) VertexNormals.GetTypedData(),
			DataBytesCount, VET_Float3, 3, EHoudiniMeshVertexBufferSemantic::Normal);
		HoudiniMeshVertexBuffers.Add(HoudiniMeshVertexBuffer);
	}

	// Create vertex stream for colors.
	if(VertexColors.Num() > 0)
	{
		uint32 DataBytesCount = sizeof(FVector) * VertexColors.Num();
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer((const uint8*) VertexColors.GetTypedData(),
			DataBytesCount, VET_Float3, 3, EHoudiniMeshVertexBufferSemantic::Color);
		HoudiniMeshVertexBuffers.Add(HoudiniMeshVertexBuffer);
	}

	// Create vertex streams for UVs.
	for(int32 TexCoordChannelIdx = 0; TexCoordChannelIdx < MAX_STATIC_TEXCOORDS; ++TexCoordChannelIdx)
	{
		// Get texture coordinate array for this channel.
		const TArray<FVector2D>& UVChannel = VertexTextureCoordinates[TexCoordChannelIdx];

		if(UVChannel.Num() > 0)
		{
			uint32 DataBytesCount = sizeof(FVector2D) * UVChannel.Num();
			FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer((const uint8*) UVChannel.GetTypedData(),
					DataBytesCount, VET_Float2, 2, 
					static_cast<EHoudiniMeshVertexBufferSemantic::Type>(EHoudiniMeshVertexBufferSemantic::TextureCoordinate0 + TexCoordChannelIdx));
			HoudiniMeshVertexBuffers.Add(HoudiniMeshVertexBuffer);
		}
	}

	// Create vertex stream for packed tangent data - tangent x.
	if(VertexPackedTangentXs.Num() > 0)
	{
		uint32 DataBytesCount = sizeof(FPackedNormal) * VertexPackedTangentXs.Num();
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer((const uint8*) VertexPackedTangentXs.GetTypedData(),
			DataBytesCount, VET_PackedNormal, 4, EHoudiniMeshVertexBufferSemantic::PackedTangentX);
		HoudiniMeshVertexBuffers.Add(HoudiniMeshVertexBuffer);
	}

	// Create vertex stream for packed tangent data - tangent z.
	if(VertexPackedTangentZs.Num() > 0)
	{
		uint32 DataBytesCount = sizeof(FPackedNormal) * VertexPackedTangentZs.Num();
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer((const uint8*) VertexPackedTangentZs.GetTypedData(),
			DataBytesCount, VET_PackedNormal, 4, EHoudiniMeshVertexBufferSemantic::PackedTangentZ);
		HoudiniMeshVertexBuffers.Add(HoudiniMeshVertexBuffer);
	}

	// Create new vertex factory.
	HoudiniMeshVertexFactory = new FHoudiniMeshVertexFactory();
	HoudiniMeshVertexFactory->Init(HoudiniMeshVertexBuffers);

	// Enqueue initialization of vertex buffers and vertex factory on render thread.
	for(TArray<FHoudiniMeshVertexBuffer*>::TIterator Iter = HoudiniMeshVertexBuffers.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = *Iter;
		BeginInitResource(HoudiniMeshVertexBuffer);
	}

	BeginInitResource(HoudiniMeshVertexFactory);

	// Create necessary rendering resources for each part.
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		HoudiniAssetObjectGeoPart->CreateRenderingResources();
	}

	bRenderingResourcesCreated = true;
}


void
FHoudiniAssetObjectGeo::ReleaseRenderingResources()
{
	// Enqueue release of vertex buffer and vertex factory on render thread.

	for(TArray<FHoudiniMeshVertexBuffer*>::TIterator Iter = HoudiniMeshVertexBuffers.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = *Iter;
		BeginReleaseResource(HoudiniMeshVertexBuffer);
	}

	if(HoudiniMeshVertexFactory)
	{
		BeginReleaseResource(HoudiniMeshVertexFactory);
	}

	// Release rendering resources taken by each part.
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		HoudiniAssetObjectGeoPart->ReleaseRenderingResources();
	}

	bRenderingResourcesCreated = false;
}


void
FHoudiniAssetObjectGeo::CollectTextures(TArray<UTexture2D*>& Textures)
{
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		HoudiniAssetObjectGeoPart->CollectTextures(Textures);
	}
}


void
FHoudiniAssetObjectGeo::SetHoudiniLogo()
{
	bHoudiniLogo = true;
}


bool
FHoudiniAssetObjectGeo::IsHoudiniLogo() const
{
	return bHoudiniLogo;
}


const FBoxSphereBounds&
FHoudiniAssetObjectGeo::GetAggregateBoundingVolume() const
{
	return AggregateBoundingVolume;
}


void
FHoudiniAssetObjectGeo::ComputeAggregateBoundingVolume()
{
	if(HoudiniAssetObjectGeoParts.Num() == 0)
	{
		AggregateBoundingVolume = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));
	}
	else
	{
		AggregateBoundingVolume = HoudiniAssetObjectGeoParts[0]->GetBoundingVolume();
	}

	for(int Idx = 1; Idx < HoudiniAssetObjectGeoParts.Num(); ++Idx)
	{
		AggregateBoundingVolume = AggregateBoundingVolume + HoudiniAssetObjectGeoParts[Idx]->GetBoundingVolume();
	}
}
