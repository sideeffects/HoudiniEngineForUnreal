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

#pragma once

class FHoudiniMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FHoudiniMeshSceneProxy(UHoudiniAssetComponent* Component, FName ResourceName = NAME_None);
	virtual ~FHoudiniMeshSceneProxy();

public: /** FPrimitiveSceneProxy methods. **/

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View) OVERRIDE;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) OVERRIDE;
	virtual bool CanBeOccluded() const OVERRIDE;

	virtual uint32 GetMemoryFootprint() const OVERRIDE;
	uint32 GetAllocatedSize() const;

private:

	FHoudiniMeshVertexFactory VertexFactory;
	FHoudiniMeshVertexBuffer VertexBuffer;
	FHoudiniMeshIndexBuffer IndexBuffer;

	FMaterialRelevance MaterialRelevance;

	UMaterialInterface* Material;
};
