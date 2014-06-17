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


FHoudiniMeshSceneProxy::FHoudiniMeshSceneProxy(UHoudiniAssetComponent* Component, FName ResourceName) :
	FPrimitiveSceneProxy(Component, ResourceName),
	MaterialRelevance(Component->GetMaterialRelevance())
{
	int32 VIndex;
	const FColor VertexColor(255, 255, 255);

	const TArray<FHoudiniMeshTriangle>& HoudiniMeshTris = Component->GetMeshTriangles();

	// Add each triangle to the vertex / index buffer.
	for(int TriIdx = 0; TriIdx < HoudiniMeshTris.Num(); TriIdx++)
	{
		const FHoudiniMeshTriangle& Tri = HoudiniMeshTris[TriIdx];

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

		VIndex = VertexBuffer.Vertices.Add(Vert0);
		IndexBuffer.Indices.Add(VIndex);

		FDynamicMeshVertex Vert1;
		Vert1.Position = Tri.Vertex1;
		Vert1.Color = Tri.Color1;
		Vert1.TextureCoordinate = Tri.TextureCoordinate1;
		Vert1.SetTangents(TangentX, TangentY, TangentZ);

		VIndex = VertexBuffer.Vertices.Add(Vert1);
		IndexBuffer.Indices.Add(VIndex);

		FDynamicMeshVertex Vert2;
		Vert2.Position = Tri.Vertex2;
		Vert2.Color = Tri.Color2;
		Vert2.TextureCoordinate = Tri.TextureCoordinate2;
		Vert2.SetTangents(TangentX, TangentY, TangentZ);

		VIndex = VertexBuffer.Vertices.Add(Vert2);
		IndexBuffer.Indices.Add(VIndex);
	}

	// Init vertex factory.
	VertexFactory.Init(&VertexBuffer);

	// Enqueue initialization of render resource.
	BeginInitResource(&VertexBuffer);
	BeginInitResource(&IndexBuffer);
	BeginInitResource(&VertexFactory);

	// Grab material.
	Material = Component->GetMaterial(0);
	if(!Material)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
}


FHoudiniMeshSceneProxy::~FHoudiniMeshSceneProxy()
{
	VertexFactory.ReleaseResource();

	VertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
}


void 
FHoudiniMeshSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HoudiniMeshSceneProxy_DrawDynamicElements);

	const bool bWireframe = View->Family->EngineShowFlags.Wireframe;

	FColoredMaterialRenderProxy WireframeMaterialInstance(GEngine->WireframeMaterial->GetRenderProxy(IsSelected()), FLinearColor(0, 0.5f, 1.f));
	FMaterialRenderProxy* MaterialProxy = NULL;

	if(bWireframe)
	{
		MaterialProxy = &WireframeMaterialInstance;
	}
	else
	{
		MaterialProxy = Material->GetRenderProxy(IsSelected());
	}

	// Draw the mesh.
	FMeshBatch Mesh;
	FMeshBatchElement& BatchElement = Mesh.Elements[0];

	BatchElement.IndexBuffer = &IndexBuffer;
	Mesh.bWireframe = bWireframe;
	Mesh.VertexFactory = &VertexFactory;
	Mesh.MaterialRenderProxy = MaterialProxy;
	BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true);
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;

	PDI->DrawMesh(Mesh);
}


FPrimitiveViewRelevance 
FHoudiniMeshSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;

	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

	return Result;
}


bool 
FHoudiniMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}


uint32 
FHoudiniMeshSceneProxy::GetMemoryFootprint() const
{
	return(sizeof(*this) + GetAllocatedSize());
}


uint32 
FHoudiniMeshSceneProxy::GetAllocatedSize() const
{
	return(FPrimitiveSceneProxy::GetAllocatedSize());
}
