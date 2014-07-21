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
	HoudiniAssetComponent(Component),
	MaterialRelevance(Component->GetMaterialRelevance())
{
	// This constructor will be executing on a game thread.

}


FHoudiniMeshSceneProxy::~FHoudiniMeshSceneProxy()
{

}


void
FHoudiniMeshSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View)
{
	bool bWireframe = View->Family->EngineShowFlags.Wireframe;
	FColoredMaterialRenderProxy WireframeMaterialInstance(GEngine->WireframeMaterial->GetRenderProxy(IsSelected()), FLinearColor(0, 0.5f, 1.f));

	for(TArray<FHoudiniAssetObjectGeo*>::TIterator Iter = HoudiniAssetComponent->HoudiniAssetObjectGeos.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = *Iter;

		// Get number of parts in this geo.
		int32 GeoPartCount = HoudiniAssetObjectGeo->HoudiniAssetObjectGeoParts.Num();

		// Create mesh for drawing submission.
		FMeshBatch Mesh;
		Mesh.UseDynamicData = false;
		Mesh.bDisableBackfaceCulling = true;
		Mesh.bWireframe = bWireframe;
		Mesh.DynamicVertexData = nullptr;
		Mesh.DynamicVertexStride = 0;
		Mesh.VertexFactory = HoudiniAssetObjectGeo->HoudiniMeshVertexFactory;
		Mesh.MaterialRenderProxy = nullptr;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.CastShadow = false;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bUseAsOccluder = true;

		if(HoudiniAssetObjectGeo->UsesMultipleMaterials() && !bWireframe)
		{
			// Different parts of this geo use different materials. In this scenario one mesh will contain one submesh.
			for(int32 PartIdx = 0; PartIdx < GeoPartCount; ++PartIdx)
			{
				// Get part at this index.
				FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = HoudiniAssetObjectGeo->HoudiniAssetObjectGeoParts[PartIdx];

				// Set material for this part.
				Mesh.MaterialRenderProxy = ComputeMaterialProxy(bWireframe, &WireframeMaterialInstance, HoudiniAssetObjectGeoPart);

				// Get batch element for this only submesh.
				FMeshBatchElement& BatchElement = Mesh.Elements[0];

				BatchElement.IndexBuffer = HoudiniAssetObjectGeoPart->HoudiniMeshIndexBuffer;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = HoudiniAssetObjectGeoPart->HoudiniMeshIndexBuffer->Indices.Num() / 3;
				BatchElement.NumInstances = 1;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = HoudiniAssetObjectGeo->HoudiniMeshVertexBuffer->Vertices.Num() - 1;
				BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), false);
			
				// Draw mesh with only one submesh.
				PDI->DrawMesh(Mesh);
			}
		}
		else
		{
			// All parts of this geo use same material or no material at all.

			Mesh.Elements.Reserve(GeoPartCount);

			// Create sub-mesh for each geo part. By default it contains one part already.
			for(int32 PartIdx = 1; PartIdx < GeoPartCount; ++PartIdx)
			{
				FMeshBatchElement* NextElement = new(Mesh.Elements) FMeshBatchElement();
			}

			for(int32 PartIdx = 0; PartIdx < GeoPartCount; ++PartIdx)
			{
				// Get part at this index.
				FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = HoudiniAssetObjectGeo->HoudiniAssetObjectGeoParts[PartIdx];

				// Set material for the whole mesh if it hasn't been set.
				if(!Mesh.MaterialRenderProxy)
				{
					Mesh.MaterialRenderProxy = ComputeMaterialProxy(bWireframe, &WireframeMaterialInstance, HoudiniAssetObjectGeoPart);
				}

				// Get batch element for this index.
				FMeshBatchElement& BatchElement = Mesh.Elements[PartIdx];

				BatchElement.IndexBuffer = HoudiniAssetObjectGeoPart->HoudiniMeshIndexBuffer;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = HoudiniAssetObjectGeoPart->HoudiniMeshIndexBuffer->Indices.Num() / 3;
				BatchElement.NumInstances = 1;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = HoudiniAssetObjectGeo->HoudiniMeshVertexBuffer->Vertices.Num() - 1;
				BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), false);
			}

			// Draw mesh with all submeshes.
			PDI->DrawMesh(Mesh);
		}
	}
}


void
FHoudiniMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	FPrimitiveSceneProxy::DrawStaticElements(PDI);
}


FPrimitiveViewRelevance
FHoudiniMeshSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;

	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;

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


const FMaterialRenderProxy*
FHoudiniMeshSceneProxy::ComputeMaterialProxy(bool bWireframe, const FColoredMaterialRenderProxy* WireframeProxy, FHoudiniAssetObjectGeoPart* GeoPart)
{
	// Get material for this mesh.
	FMaterialRenderProxy* MaterialProxy = nullptr;

	if(bWireframe)
	{
		return WireframeProxy;
	}
	else if(GeoPart->Material)
	{
		return GeoPart->Material->GetRenderProxy(IsSelected(), IsHovered());
	}

	UMaterial* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	return Material->GetRenderProxy(IsSelected(), IsHovered());
}
