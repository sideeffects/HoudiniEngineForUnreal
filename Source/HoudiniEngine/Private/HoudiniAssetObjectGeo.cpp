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
	HoudiniMeshVertexBuffer(nullptr),
	HoudiniMeshVertexFactory(nullptr),
	ObjectId(-1),
	GeoId(-1),
	PartId(-1),
	bMultipleMaterials(false)
{
	Transform = FMatrix::Identity;
}


FHoudiniAssetObjectGeo::FHoudiniAssetObjectGeo(const FMatrix& InTransform, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId) :
	Transform(InTransform),
	HoudiniMeshVertexBuffer(nullptr),
	HoudiniMeshVertexFactory(nullptr),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	bMultipleMaterials(false)
{

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
	if(HoudiniMeshVertexBuffer)
	{
		check(!HoudiniMeshVertexBuffer->IsInitialized());
		delete HoudiniMeshVertexBuffer;
	}

	if(HoudiniMeshVertexFactory)
	{
		check(!HoudiniMeshVertexFactory->IsInitialized());
		delete HoudiniMeshVertexFactory;
	}
}


const FMatrix&
FHoudiniAssetObjectGeo::GetTransform() const
{
	return Transform;
}


void
FHoudiniAssetObjectGeo::Serialize(FArchive& Ar)
{
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
					FDynamicMeshVertex Vertex;

					Ar << Vertex.Position;
					Ar << Vertex.TextureCoordinate;
					Ar << Vertex.TangentX;
					Ar << Vertex.TangentZ;
					Ar << Vertex.Color;

					Vertices.Add(Vertex);
				}
				else if(Ar.IsSaving())
				{
					FDynamicMeshVertex& Vertex = Vertices[Idx];

					Ar << Vertex.Position;
					Ar << Vertex.TextureCoordinate;
					Ar << Vertex.TangentX;
					Ar << Vertex.TangentZ;
					Ar << Vertex.Color;
				}
			}
		}
	}

	// Serialize transform.
	Ar << Transform;

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


TArray<FDynamicMeshVertex>&
FHoudiniAssetObjectGeo::GetVertices()
{
	return Vertices;
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


UHoudiniAssetMaterial*
FHoudiniAssetObjectGeo::GetSingleMaterial() const
{
	UHoudiniAssetMaterial* Material = nullptr;

	if(HoudiniAssetObjectGeoParts.Num() > 0)
	{
		for(int32 Idx = 0; Idx < HoudiniAssetObjectGeoParts.Num(); ++Idx)
		{
			UHoudiniAssetMaterial* FirstMaterial = HoudiniAssetObjectGeoParts[Idx]->Material;

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
FHoudiniAssetObjectGeo::ReplaceMaterial(UHoudiniAssetMaterial* Material)
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


void
FHoudiniAssetObjectGeo::AddTriangleVertices(FHoudiniMeshTriangle& Tri)
{
	const FVector Edge01 = Tri.Vertex1 - Tri.Vertex0;
	const FVector Edge02 = Tri.Vertex2 - Tri.Vertex0;

	const FVector TangentX = Edge01.SafeNormal();
	const FVector TangentZ = (Edge02 ^ Edge01).SafeNormal();
	const FVector TangentY = (TangentX ^ TangentZ).SafeNormal();

	FDynamicMeshVertex Vert0;
	Vert0.Position = Tri.Vertex0;
	Vert0.Color = Tri.Color0;
	Vert0.TextureCoordinate = Tri.TextureCoordinate0;
	Vert0.SetTangents(TangentX, TangentY, TangentZ);

	Vertices.Add(Vert0);

	FDynamicMeshVertex Vert1;
	Vert1.Position = Tri.Vertex1;
	Vert1.Color = Tri.Color1;
	Vert1.TextureCoordinate = Tri.TextureCoordinate1;
	Vert1.SetTangents(TangentX, TangentY, TangentZ);

	Vertices.Add(Vert1);

	FDynamicMeshVertex Vert2;
	Vert2.Position = Tri.Vertex2;
	Vert2.Color = Tri.Color2;
	Vert2.TextureCoordinate = Tri.TextureCoordinate2;
	Vert2.SetTangents(TangentX, TangentY, TangentZ);

	Vertices.Add(Vert2);
}


void
FHoudiniAssetObjectGeo::CreateRenderingResources()
{
	// Check to make sure we are not running this twice.
	if(HoudiniMeshVertexBuffer && HoudiniMeshVertexFactory)
	{
		// Resources have been initialized, we can skip. This will happen when geos are reused between the cooks.
		return;
	}

	// Create new vertex buffer.
	HoudiniMeshVertexBuffer = new FHoudiniMeshVertexBuffer();
	HoudiniMeshVertexBuffer->Vertices = Vertices;

	// Create new vertex factory.
	HoudiniMeshVertexFactory = new FHoudiniMeshVertexFactory();
	HoudiniMeshVertexFactory->Init(HoudiniMeshVertexBuffer);

	// Enqueue initialization of vertex buffer and vertex factory on render thread.
	BeginInitResource(HoudiniMeshVertexBuffer);
	BeginInitResource(HoudiniMeshVertexFactory);

	// Create necessary rendering resources for each part.
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		HoudiniAssetObjectGeoPart->CreateRenderingResources();
	}
}


void
FHoudiniAssetObjectGeo::ReleaseRenderingResources()
{
	// Enqueue release of vertex buffer and vertex factory on render thread.
	if(HoudiniMeshVertexBuffer)
	{
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
