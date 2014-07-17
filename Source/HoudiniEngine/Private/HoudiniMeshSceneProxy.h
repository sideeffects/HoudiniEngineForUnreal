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

	/** Constructor. **/
	FHoudiniMeshSceneProxy(UHoudiniAssetComponent* Component, FName ResourceName = NAME_None);

	/** Destructor. **/
	virtual ~FHoudiniMeshSceneProxy();

public: /** FPrimitiveSceneProxy methods. **/

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View) OVERRIDE;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) OVERRIDE;
	virtual bool CanBeOccluded() const OVERRIDE;
	virtual uint32 GetMemoryFootprint() const OVERRIDE;

private:

	/** Owner component. Unsafe to use as this object will live in a render thread. Used here for debugging. **/
	UHoudiniAssetComponent* HoudiniAssetComponent;

	/** View relevance for all involved materials. **/
	FMaterialRelevance MaterialRelevance;
};
