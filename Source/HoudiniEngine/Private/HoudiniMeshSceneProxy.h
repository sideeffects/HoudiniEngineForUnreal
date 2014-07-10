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

class FSceneView;
class UMaterialInterface;
class UHoudiniAssetComponent;
class FPrimitiveDrawInterface;
class FStaticPrimitiveDrawInterface;

class FHoudiniMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FHoudiniMeshSceneProxy(UHoudiniAssetComponent* Component, FName ResourceName = NAME_None);
	virtual ~FHoudiniMeshSceneProxy();

public: /** FPrimitiveSceneProxy methods. **/

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View) OVERRIDE;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) OVERRIDE;
	virtual bool CanBeOccluded() const OVERRIDE;

	virtual uint32 GetMemoryFootprint() const OVERRIDE;
	uint32 GetAllocatedSize() const;

private:

	/** Vertex factory simply transforms explicit vertex attributes from local to world space. **/
	FHoudiniMeshVertexFactory VertexFactory;

	/** Vertex buffer. **/
	FHoudiniMeshVertexBuffer VertexBuffer;

	/** Index buffer. **/
	FHoudiniMeshIndexBuffer IndexBuffer;

	/** View relevance for all Houdini mesh materials. **/
	FMaterialRelevance MaterialRelevance;


	UMaterialInterface* Material;
};
