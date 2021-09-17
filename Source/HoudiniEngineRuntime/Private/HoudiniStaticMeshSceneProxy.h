/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Based on: Plugins\Experimental\MeshModelingToolset\Source\ModelingComponents\Private\BaseDynamicMeshSceneProxy.h

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"

#include "HoudiniStaticMeshComponent.h"

class UHoudiniStaticMesh;

class FHoudiniStaticMeshRenderBufferSet
{
public:
	// Data members

	/** The number of triangles in the buffer set. */
	int NumTriangles;

	/** The static mesh data buffer. */
	FStaticMeshVertexBuffer StaticMeshVertexBuffer;

	/** The position buffer. */
	FPositionVertexBuffer PositionVertexBuffer;

	/** The triangle indices buffer. */
	FDynamicMeshIndexBuffer32 TriangleIndexBuffer;

	/** The color buffer */
	FColorVertexBuffer ColorVertexBuffer;

	FLocalVertexFactory LocalVertexFactory;

	/** Default material for this mesh. */
	UMaterialInterface* Material = nullptr;

	// Functions

	FHoudiniStaticMeshRenderBufferSet(ERHIFeatureLevel::Type FeatureLevelType);

	virtual ~FHoudiniStaticMeshRenderBufferSet();

	/**
	 * Copy buffers to GPU.
	 * @warning render thread only.
	 */
	virtual void CopyBuffers();

	/**
	 * Initialize (or update) a render resource.
	 * @warning Render thread only.
	 */
	void InitOrUpdateResource(FRenderResource* Resource);

protected:
	friend class FHoudiniStaticMeshSceneProxy;

	// Queue a command on the render thread to destroy the given buffer set
	static void DestroyRenderBufferSet(FHoudiniStaticMeshRenderBufferSet* BufferSet);
};


class FHoudiniStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FHoudiniStaticMeshSceneProxy(UHoudiniStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FHoudiniStaticMeshSceneProxy();

	uint32 AllocateNewRenderBufferSet(FHoudiniStaticMeshRenderBufferSet*& OutBufferSet);

	void ReleaseRenderBufferSet(uint32 InIndex);

	void UpdatedReferencedMaterials();

	// Build buffer sets to render the mesh.
	virtual void Build();

	// FPrimitiveSceneProxy
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool CanBeOccluded() const override;

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize()); }

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	// end - FPrimitiveSceneProxy

	// Color to use if vertex does not have an assigned color.
	FColor DefaultVertexColor;

	ERHIFeatureLevel::Type FeatureLevel;

protected:
	void PopulateBuffers(const UHoudiniStaticMesh *InMesh, FHoudiniStaticMeshRenderBufferSet *InBuffers, const TArray<uint32>* InTriangleIDs=nullptr, uint32 InTriangleGroupStartIdx=0u, uint32 InNumTrianglesInGroup=0u);

	// Virtual function for creating a new buffer set instances.
	// Subclasses can overwrite this is they use a different buffer set with 
	// different instantiation requirements.
	virtual FHoudiniStaticMeshRenderBufferSet* MakeNewBufferSet() { return new FHoudiniStaticMeshRenderBufferSet(FeatureLevel);	}

	// Build a single buffer set for the entire mesh (one material for the entire mesh).
	void BuildSingleBufferSet();

	void BuildBufferSetsByMaterial();

	// Get the number of materials from the parent mesh/component
	uint32 GetNumMaterials() const { return Component ? Component->GetNumMaterials() : 0; }

	virtual bool PopulateMeshElement(
		FMeshBatch &InMeshBatch,
		const FHoudiniStaticMeshRenderBufferSet& Buffers,
		FMaterialRenderProxy* Material,
		bool bRenderAsWireframe,
		ESceneDepthPriorityGroup DepthPriority,
		int ViewIndex,
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer) const;

	virtual UMaterialInterface* GetMaterial(uint32 InMaterialIdx) const;

	UHoudiniStaticMeshComponent *Component;

	TArray<FHoudiniStaticMeshRenderBufferSet*> BufferSets;

	FCriticalSection BufferSetsLock;

	FMaterialRelevance MaterialRelevance;

private:
#if STATICMESH_ENABLE_DEBUG_RENDERING
	AActor* Owner;
#endif
};