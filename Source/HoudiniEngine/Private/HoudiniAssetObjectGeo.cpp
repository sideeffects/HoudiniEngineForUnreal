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


FHoudiniAssetObjectGeo::FHoudiniAssetObjectGeo(const FMatrix& InTransform) :
	Transform(InTransform),
	HoudiniMeshVertexBuffer(nullptr),
	HoudiniMeshVertexFactory(nullptr)
{
	//Transform.SetIdentity();
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

	// Serialize parts.
	for(TArray<FHoudiniAssetObjectGeoPart*>::TIterator Iter = HoudiniAssetObjectGeoParts.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = *Iter;
		HoudiniAssetObjectGeoPart->Serialize(Ar);
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
	check(!HoudiniMeshVertexBuffer);
	check(!HoudiniMeshVertexFactory);

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
