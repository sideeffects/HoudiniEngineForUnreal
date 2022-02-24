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

#include "HoudiniStaticMeshSceneProxy.h"

#include "Async/ParallelFor.h"
#include "Materials/Material.h"
#include "PrimitiveViewRelevance.h"
#include "Engine/Engine.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "HoudiniStaticMeshComponent.h"
#include "HoudiniStaticMesh.h"

// Based on: Plugins\Experimental\MeshModelingToolset\Source\ModelingComponents\Private\BaseDynamicMeshSceneProxy.h

//
// FHoudiniStaticMeshRenderBufferSet
//

FHoudiniStaticMeshRenderBufferSet::FHoudiniStaticMeshRenderBufferSet(ERHIFeatureLevel::Type InFeatureLevel)
	: LocalVertexFactory(InFeatureLevel, "FHoudiniStaticMeshRenderBufferSet")
{
}


FHoudiniStaticMeshRenderBufferSet::~FHoudiniStaticMeshRenderBufferSet()
{
	check(IsInRenderingThread());

	if (NumTriangles > 0)
	{
		PositionVertexBuffer.ReleaseResource();
		ColorVertexBuffer.ReleaseResource();
		StaticMeshVertexBuffer.ReleaseResource();
		LocalVertexFactory.ReleaseResource();
		if (TriangleIndexBuffer.IsInitialized())
		{
			TriangleIndexBuffer.ReleaseResource();
		}
	}
}

void FHoudiniStaticMeshRenderBufferSet::CopyBuffers()
{
	check(IsInRenderingThread());

	if (NumTriangles == 0)
	{
		return;
	}

	InitOrUpdateResource(&PositionVertexBuffer);
	InitOrUpdateResource(&ColorVertexBuffer);
	InitOrUpdateResource(&StaticMeshVertexBuffer);

	FLocalVertexFactory::FDataType Data;
	PositionVertexBuffer.BindPositionVertexBuffer(&LocalVertexFactory, Data);
	StaticMeshVertexBuffer.BindTangentVertexBuffer(&LocalVertexFactory, Data);
	StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&LocalVertexFactory, Data);
	ColorVertexBuffer.BindColorVertexBuffer(&LocalVertexFactory, Data);

	LocalVertexFactory.SetData(Data);
	InitOrUpdateResource(&LocalVertexFactory);

	if (TriangleIndexBuffer.Indices.Num() > 0)
	{
		TriangleIndexBuffer.InitResource();
	}
}

void FHoudiniStaticMeshRenderBufferSet::InitOrUpdateResource(FRenderResource* Resource)
{
	check(IsInRenderingThread());

	if (Resource->IsInitialized())
		Resource->UpdateRHI();
	else
		Resource->InitResource();
}

void FHoudiniStaticMeshRenderBufferSet::DestroyRenderBufferSet(FHoudiniStaticMeshRenderBufferSet* BufferSet)
{
	if (BufferSet->NumTriangles == 0)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(FMeshRenderBufferSetDestroy)(
		[BufferSet](FRHICommandListImmediate& RHICmdList)
	{
		delete BufferSet;
	});
}

//
// End - FHoudiniStaticMeshRenderBufferSet
//

//
// FHoudiniStaticMeshSceneProxy
//

FHoudiniStaticMeshSceneProxy::FHoudiniStaticMeshSceneProxy(UHoudiniStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
	: FPrimitiveSceneProxy(InComponent)
	, DefaultVertexColor(255, 255, 255)
	, FeatureLevel(InFeatureLevel)
	, Component(InComponent)
	, MaterialRelevance(InComponent ? InComponent->GetMaterialRelevance(InFeatureLevel) : FMaterialRelevance())
#if STATICMESH_ENABLE_DEBUG_RENDERING
	, Owner(InComponent ? InComponent->GetOwner() : nullptr)
#endif
{
}

FHoudiniStaticMeshSceneProxy::~FHoudiniStaticMeshSceneProxy()
{
	check(IsInRenderingThread());

	for (FHoudiniStaticMeshRenderBufferSet* BufferSet : BufferSets)
	{
		FHoudiniStaticMeshRenderBufferSet::DestroyRenderBufferSet(BufferSet);
	}
}

uint32 FHoudiniStaticMeshSceneProxy::AllocateNewRenderBufferSet(FHoudiniStaticMeshRenderBufferSet*& OutBufferSet)
{
	OutBufferSet = MakeNewBufferSet();
	OutBufferSet->Material = UMaterial::GetDefaultMaterial(MD_Surface);

	BufferSetsLock.Lock();
	const uint32 NewIndex = BufferSets.Add(OutBufferSet);
	BufferSetsLock.Unlock();
	return NewIndex;
}

void FHoudiniStaticMeshSceneProxy::ReleaseRenderBufferSet(uint32 InIndex)
{
	FHoudiniStaticMeshRenderBufferSet *BufferSet = nullptr;

	BufferSetsLock.Lock();
	check(BufferSets.IsValidIndex(InIndex));
	BufferSet = BufferSets[InIndex];
	BufferSets.RemoveAt(InIndex);
	BufferSetsLock.Unlock();

	FHoudiniStaticMeshRenderBufferSet::DestroyRenderBufferSet(BufferSet);
}

void FHoudiniStaticMeshSceneProxy::UpdatedReferencedMaterials()
{
	// copied from FPrimitiveSceneProxy::FPrimitiveSceneProxy()
#if WITH_EDITOR
	TArray<UMaterialInterface*> Materials;
	Component->GetUsedMaterials(Materials, true);
	ENQUEUE_RENDER_COMMAND(FHoudiniStaticMeshRenderBufferSetDestroy)(
		[this, Materials](FRHICommandListImmediate& RHICmdList)
	{
		this->SetUsedMaterialForVerification(Materials);
	});
#endif
}

void FHoudiniStaticMeshSceneProxy::Build()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FHoudiniStaticMeshSceneProxy::Build"));

	// Allocate a buffer set per material
	const uint32 NumMaterials = GetNumMaterials();
	if (NumMaterials == 0)
	{
		// No materials, allocate a singel buffer set using the default material
		FHoudiniStaticMeshRenderBufferSet *BufferSet = nullptr;
		AllocateNewRenderBufferSet(BufferSet);
		BufferSet->Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	else
	{
		for (uint32 MaterialIdx = 0; MaterialIdx < NumMaterials; ++MaterialIdx)
		{
			FHoudiniStaticMeshRenderBufferSet *BufferSet = nullptr;
			AllocateNewRenderBufferSet(BufferSet);
			BufferSet->Material = GetMaterial(MaterialIdx);
		}
	}

	if (Component)
	{
		UHoudiniStaticMesh *Mesh = Component->GetMesh();
		if (Mesh)
		{
			if (NumMaterials > 1 && Mesh->HasPerFaceMaterials())
			{
				BuildBufferSetsByMaterial();
			}
			else
			{
				BuildSingleBufferSet();
			}
		}
	}
}

void FHoudiniStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const FEngineShowFlags EngineShowFlags = ViewFamily.EngineShowFlags;
	const bool bRenderAsWireframe = (AllowDebugViewmodes() && EngineShowFlags.Wireframe);

	// Set up the wireframe material
	FMaterialRenderProxy *WireframeMaterialProxy = nullptr;
	if (bRenderAsWireframe)
	{
		FColoredMaterialRenderProxy *WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
			FLinearColor(0.6f, 0.6f, 0.6f)
		);
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		WireframeMaterialProxy = WireframeMaterialInstance;
	}

	const ESceneDepthPriorityGroup DepthPriority = SDPG_World;

	const int32 NumViews = Views.Num();
	for (int32 ViewIdx = 0; ViewIdx < NumViews; ++ViewIdx)
	{
		if (!(VisibilityMap & (1 << ViewIdx)))
			continue;

		const FSceneView *View = Views[ViewIdx];

		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity;
		GetScene().GetPrimitiveUniformShaderParameters_RenderThread(
			GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

		const uint32 NumBufferSets = BufferSets.Num();
		for (uint32 BufferSetIdx = 0; BufferSetIdx < NumBufferSets; ++BufferSetIdx)
		{
			FHoudiniStaticMeshRenderBufferSet *BufferSet = BufferSets[BufferSetIdx];

			UMaterialInterface *Material = BufferSet->Material;
			FMaterialRenderProxy *MaterialProxy = Material->GetRenderProxy();

			if (BufferSet->NumTriangles == 0)
				continue;

			FDynamicPrimitiveUniformBuffer &DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			DynamicPrimitiveUniformBuffer.Set(
				GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);

			if (BufferSet->TriangleIndexBuffer.Indices.Num() > 0)
			{
				FMeshBatch& Mesh = Collector.AllocateMesh();
				if (PopulateMeshElement(Mesh, *BufferSet, MaterialProxy, false, DepthPriority, ViewIdx, DynamicPrimitiveUniformBuffer))
				{
					Collector.AddMesh(ViewIdx, Mesh);
				}
				if (bRenderAsWireframe)
				{
					FMeshBatch& WireframeMesh = Collector.AllocateMesh();
					if (PopulateMeshElement(WireframeMesh, *BufferSet, WireframeMaterialProxy, true, DepthPriority, ViewIdx, DynamicPrimitiveUniformBuffer))
					{
						Collector.AddMesh(ViewIdx, WireframeMesh);
					}
				}
			}
		}

#if STATICMESH_ENABLE_DEBUG_RENDERING
		if (EngineShowFlags.StaticMeshes)
		{
			RenderBounds(Collector.GetPDI(ViewIdx), EngineShowFlags, GetBounds(), !Owner || IsSelected());
		}
#endif // STATICMESH_ENABLE_DEBUG_RENDERING
	}
}

bool FHoudiniStaticMeshSceneProxy::PopulateMeshElement(
	FMeshBatch &InMeshBatch,
	const FHoudiniStaticMeshRenderBufferSet& Buffers,
	FMaterialRenderProxy* Material,
	bool bRenderAsWireframe,
	ESceneDepthPriorityGroup DepthPriority,
	int ViewIndex,
	FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer) const
{
	FMeshBatchElement& BatchElement = InMeshBatch.Elements[0];
	BatchElement.IndexBuffer = &Buffers.TriangleIndexBuffer;
	InMeshBatch.bWireframe = bRenderAsWireframe;
	InMeshBatch.VertexFactory = &Buffers.LocalVertexFactory;
	InMeshBatch.MaterialRenderProxy = Material;

	BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = Buffers.NumTriangles;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = Buffers.PositionVertexBuffer.GetNumVertices() - 1;
	InMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	InMeshBatch.Type = PT_TriangleList;
	InMeshBatch.DepthPriorityGroup = DepthPriority;
	InMeshBatch.bCanApplyViewModeOverrides = false;
	
	return true;
}


FPrimitiveViewRelevance FHoudiniStaticMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;

	Result.bDrawRelevance = IsShown(View);
	Result.bDynamicRelevance = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = IsMovable() && Result.bOpaque && Result.bRenderInMainPass;

	return Result;
}

bool FHoudiniStaticMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

void FHoudiniStaticMeshSceneProxy::PopulateBuffers(const UHoudiniStaticMesh *InMesh, FHoudiniStaticMeshRenderBufferSet *InBuffers, const TArray<uint32>* InTriangleIDs, uint32 InTriangleGroupStartIdx, uint32 InNumTrianglesInGroup)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FHoudiniStaticMeshSceneProxy::PopulateBuffers"));

	check(InMesh);
	check(InBuffers);

	const uint32 NumTriangles = InTriangleIDs ? InNumTrianglesInGroup : InMesh->GetNumTriangles();
	InBuffers->NumTriangles = NumTriangles;

	if (NumTriangles == 0)
		return;

	const uint32 NumVertices = NumTriangles * 3;
	const uint32 NumUVLayers = InMesh->GetNumUVLayers();

	InBuffers->PositionVertexBuffer.Init(NumVertices);
	// There must be at least one UV layer
	// TODO: Would it be possible to have no UV layers and bind to a dummy 0/black SRV?
	InBuffers->StaticMeshVertexBuffer.Init(NumVertices, NumUVLayers > 0 ? NumUVLayers : 1);
	InBuffers->ColorVertexBuffer.Init(NumVertices);
	InBuffers->TriangleIndexBuffer.Indices.AddUninitialized(NumTriangles * 3);

	const TArray<FVector>& VertexPositions = InMesh->GetVertexPositions();
	const TArray<FIntVector>& TriangleIndices = InMesh->GetTriangleIndices();
	const TArray<FColor>& VertexInstanceColors = InMesh->GetVertexInstanceColors();
	const TArray<FVector>& VertexInstanceNormals = InMesh->GetVertexInstanceNormals();
	const TArray<FVector>& VertexInstanceUTangents = InMesh->GetVertexInstanceUTangents();
	const TArray<FVector>& VertexInstanceVTangents = InMesh->GetVertexInstanceVTangents();
	const TArray<FVector2d>& VertexInstanceUVs = InMesh->GetVertexInstanceUVs();

	const bool bHasColors = InMesh->HasColors();
	const bool bHasNormals = InMesh->HasNormals();
	const bool bHasTangents = InMesh->HasTangents();

	FThreadSafeCounter VertCounter(0);
	//for (uint32 TriangleIDIdx = 0; TriangleIDIdx < NumTriangles; ++TriangleIDIdx)
	ParallelFor(NumTriangles, [&](uint32 TriangleIDIdx)
	{
		const uint32 TriangleID = InTriangleIDs ? (*InTriangleIDs)[InTriangleGroupStartIdx + TriangleIDIdx] : TriangleIDIdx;
		const FIntVector &TriIndices = TriangleIndices[TriangleID];

		FVector TangentU;
		FVector TangentV;
		uint32 VertIdx = VertCounter.Add(3);
		for (uint8 TriVertIdx = 0; TriVertIdx < 3; ++TriVertIdx)
		{
			const uint32 MeshVtxIdx = TriIndices[TriVertIdx];
			const uint32 MeshVtxInstanceIdx = TriangleID * 3 + TriVertIdx;

			InBuffers->PositionVertexBuffer.VertexPosition(VertIdx) = VertexPositions[TriIndices[TriVertIdx]];

			FVector Normal = bHasNormals ? VertexInstanceNormals[MeshVtxInstanceIdx] : FVector(0, 0, 1);
			if (bHasTangents)
			{
				TangentU = VertexInstanceUTangents[MeshVtxInstanceIdx];
				TangentV = VertexInstanceVTangents[MeshVtxInstanceIdx];
			}
			else
			{
				Normal.FindBestAxisVectors(TangentU, TangentV);
			}
			InBuffers->StaticMeshVertexBuffer.SetVertexTangents(VertIdx, TangentU, TangentV, Normal);

			if (NumUVLayers > 0)
			{
				for (uint8 UVLayerIdx = 0; UVLayerIdx < NumUVLayers; ++UVLayerIdx)
				{
					InBuffers->StaticMeshVertexBuffer.SetVertexUV(VertIdx, UVLayerIdx, FVector2f(VertexInstanceUVs[MeshVtxInstanceIdx]));
				}
			}
			else
			{
				InBuffers->StaticMeshVertexBuffer.SetVertexUV(VertIdx, 0, FVector2f::ZeroVector);
			}

			InBuffers->ColorVertexBuffer.VertexColor(VertIdx) = bHasColors ? VertexInstanceColors[MeshVtxInstanceIdx] : DefaultVertexColor;

			InBuffers->TriangleIndexBuffer.Indices[VertIdx] = VertIdx;
			VertIdx++;
		}
	});
}

void FHoudiniStaticMeshSceneProxy::BuildSingleBufferSet()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FHoudiniStaticMeshSceneProxy::BuildSingleBufferSet"));

	if (!Component)
		return;

	UHoudiniStaticMesh *Mesh = Component->GetMesh();
	if (!Mesh)
		return;

	if (BufferSets.Num() == 0)
		return;

	FHoudiniStaticMeshRenderBufferSet *Buffers = BufferSets.Last();

	PopulateBuffers(Mesh, Buffers);

	ENQUEUE_RENDER_COMMAND(FHoudiniStaticMeshSceneProxy_BuildSingleBufferSet)(
		[Buffers](FRHICommandListImmediate& RHICMdList)
	{
		Buffers->CopyBuffers();
	});
}

void FHoudiniStaticMeshSceneProxy::BuildBufferSetsByMaterial()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FHoudiniStaticMeshSceneProxy::BuildBufferSetsByMaterial"));

	// We need to group tris by which material they use, and populate a buffer set for each group
	if (!Component)
		return;

	UHoudiniStaticMesh *Mesh = Component->GetMesh();
	if (!Mesh)
		return;

	if (BufferSets.Num() == 0)
		return;

	const TArray<int32>& MaterialIDsPerTriangle = Mesh->GetMaterialIDsPerTriangle();

	const uint32 NumTriangles = MaterialIDsPerTriangle.Num();
	const uint32 NumMaterials = GetNumMaterials();
	TArray<FThreadSafeCounter> TriCountPerMaterialSafe;
	TriCountPerMaterialSafe.Init(FThreadSafeCounter(0), NumMaterials);

	ParallelFor(NumTriangles, [&](uint32 TriangleID)
	{
		const int32 MatID = MaterialIDsPerTriangle[TriangleID];
		if (MatID >= 0 && (uint32) MatID < NumMaterials)
		{
			TriCountPerMaterialSafe[MatID].Increment();
		}
	});

	TArray<uint32> TriCountPerMaterial;
	TriCountPerMaterial.SetNumZeroed(NumMaterials);
	TArray<uint32> OffsetPerMaterial;
	OffsetPerMaterial.SetNumZeroed(NumMaterials);
	TArray<FThreadSafeCounter> WrittenPerMaterial;
	WrittenPerMaterial.Init(FThreadSafeCounter(0), NumMaterials);
	for (int32 MatID = 0; (uint32) MatID < NumMaterials; ++MatID)
	{
		const uint32 Count = TriCountPerMaterialSafe[MatID].GetValue();
		TriCountPerMaterial[MatID] = Count;
		if (MatID > 0)
		{
			OffsetPerMaterial[MatID] = OffsetPerMaterial[MatID - 1] + TriCountPerMaterial[MatID - 1];
		}
	}

	TArray<uint32> GroupTriangleIDs;
	GroupTriangleIDs.SetNumZeroed(NumTriangles);
	ParallelFor(NumTriangles, [&](uint32 TriangleID) 
	{
		const int32 MatID = MaterialIDsPerTriangle[TriangleID];
		if (MatID >= 0 && (uint32) MatID < NumMaterials)
		{
			GroupTriangleIDs[OffsetPerMaterial[MatID] + WrittenPerMaterial[MatID].Add(1)] = TriangleID;
		}
	});

	for (int32 MatID = 0; (uint32) MatID < NumMaterials; ++MatID)
	{
		if (TriCountPerMaterial[MatID] == 0)
			continue;

		FHoudiniStaticMeshRenderBufferSet *Buffers = BufferSets[MatID];

		PopulateBuffers(
			Mesh, Buffers,
			&GroupTriangleIDs, OffsetPerMaterial[MatID], TriCountPerMaterial[MatID]
		);

		ENQUEUE_RENDER_COMMAND(FHoudiniStaticMeshSceneProxy_BuildSingleBufferSet)(
			[Buffers](FRHICommandListImmediate& RHICMdList)
		{
			Buffers->CopyBuffers();
		});
	}
}

UMaterialInterface* FHoudiniStaticMeshSceneProxy::GetMaterial(uint32 InMaterialIdx) const
{
	if (!Component)
		return UMaterial::GetDefaultMaterial(MD_Surface);
	UMaterialInterface *Material = Component->GetMaterial(InMaterialIdx);
	return Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
}

//
// End - FHoudiniStaticMeshSceneProxy
//
