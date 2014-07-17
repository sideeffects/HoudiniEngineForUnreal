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
	FMaterialRenderProxy* MaterialProxy = NULL;

	if(bWireframe)
	{
		MaterialProxy = &WireframeMaterialInstance;
	}
	else
	{
		UMaterial* Material = UMaterial::GetDefaultMaterial(MD_Surface);
		MaterialProxy = Material->GetRenderProxy(IsSelected(), IsHovered());
	}

	for(TArray<FHoudiniAssetObjectGeo*>::TIterator Iter = HoudiniAssetComponent->HoudiniAssetObjectGeos.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = *Iter;

		// Get number of parts in this geo.
		int32 GeoPartCount = HoudiniAssetObjectGeo->HoudiniAssetObjectGeoParts.Num();

		// Create mesh for drawing submission. By default it contains one part already.
		FMeshBatch Mesh;
		Mesh.Elements.RemoveAtSwap(0, 1, false);
		Mesh.Elements.Reserve(GeoPartCount);

		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = HoudiniAssetObjectGeo->HoudiniMeshVertexFactory;
		Mesh.MaterialRenderProxy = MaterialProxy;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;

		for(int32 PartIdx = 0; PartIdx < GeoPartCount; ++PartIdx)
		{
			FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = HoudiniAssetObjectGeo->HoudiniAssetObjectGeoParts[PartIdx];

			FMeshBatchElement BatchElement;

			BatchElement.IndexBuffer = HoudiniAssetObjectGeoPart->HoudiniMeshIndexBuffer;
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = HoudiniAssetObjectGeoPart->HoudiniMeshIndexBuffer->Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = HoudiniAssetObjectGeo->HoudiniMeshVertexBuffer->Vertices.Num() - 1;

			// Compute proper transformation for this element.
			//FMatrix TransformMatrix = GetLocalToWorld() * HoudiniAssetObjectGeo->GetTransform();
			FMatrix TransformMatrix = HoudiniAssetObjectGeo->GetTransform() * GetLocalToWorld();

			// Store necessary matrices and bounds in uniform buffer.
			BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(TransformMatrix, GetBounds(), GetLocalBounds(), true);

			// Add this sub-mesh to our mesh.
			Mesh.Elements.Add(BatchElement);
		}

		PDI->DrawMesh(Mesh);
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
