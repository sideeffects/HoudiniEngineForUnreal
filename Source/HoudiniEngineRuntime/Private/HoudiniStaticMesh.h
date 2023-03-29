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

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"

#include "HoudiniStaticMesh.generated.h"

/**
 * This is a simple static mesh that is meant to be built in one go, without modifications afterwards.
 * The number of vertices and triangles must be known before hand.
 */
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniStaticMesh : public UObject
{
    GENERATED_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	UHoudiniStaticMesh(const FObjectInitializer &ObjectInitializer);

	// Clears all existing data and initializes internal arrays to the relevant sizes to accommodate the
	// mesh based InNumVertices, InNumTriangles, UVs etc.
	UFUNCTION()
	void Initialize(uint32 InNumVertices, uint32 InNumTriangles, uint32 InNumUVLayers, uint32 InInitialNumStaticMaterials, bool bInHasNormals, bool bInHasTangents, bool bInHasColors, bool bInHasPerFaceMaterials);

	UFUNCTION()
	bool HasPerFaceMaterials() const { return bHasPerFaceMaterials;  }

	UFUNCTION()
	void SetHasPerFaceMaterials(bool bInHasPerFaceMaterials);

	UFUNCTION()
	bool HasNormals() const { return bHasNormals; }

	UFUNCTION()
	void SetHasNormals(bool bInHasNormals);

	UFUNCTION()
	bool HasTangents() const { return bHasTangents; }

	UFUNCTION()
	void SetHasTangents(bool bInHasTangents);

	UFUNCTION()
	bool HasColors() const { return bHasColors; }

	UFUNCTION()
	void SetHasColors(bool bInHasColors);

	UFUNCTION()
	uint32 GetNumUVLayers() const { return NumUVLayers; }

	UFUNCTION()
	void SetNumUVLayers(uint32 InNumUVLayers);

	UFUNCTION()
	uint32 GetNumStaticMaterials() const { return StaticMaterials.Num(); }

	UFUNCTION()
	void SetNumStaticMaterials(uint32 InNumStaticMaterials);

	UFUNCTION()
	uint32 GetNumVertices() const { return VertexPositions.Num(); }

	UFUNCTION()
	uint32 GetNumTriangles() const { return TriangleIndices.Num(); }

	UFUNCTION()
	uint32 GetNumVertexInstances() const { return TriangleIndices.Num() * 3; }

	UFUNCTION()
	void SetVertexPosition(uint32 InVertexIndex, const FVector3f& InPosition);

	UFUNCTION()
	void SetTriangleVertexIndices(uint32 InTriangleIndex, const FIntVector& InTriangleVertexIndices);

	UFUNCTION()
	void SetTriangleVertexNormal(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector3f& InNormal);

	UFUNCTION()
	void SetTriangleVertexUTangent(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector3f& InUTangent);

	UFUNCTION()
	void SetTriangleVertexVTangent(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector3f& InVTangent);

	UFUNCTION()
	void SetTriangleVertexColor(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FColor& InColor);

	UFUNCTION()
	void SetTriangleVertexUV(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, uint8 InUVLayer, const FVector2f& InUV);

	UFUNCTION()
	void SetTriangleMaterialID(uint32 InTriangleIndex, int32 InMaterialID);

	UFUNCTION()
	void SetStaticMaterial(uint32 InMaterialIndex, const FStaticMaterial& InStaticMaterial);

	UFUNCTION()
	uint32 AddStaticMaterial(const FStaticMaterial& InStaticMaterial) { return StaticMaterials.Add(InStaticMaterial); }

	/** Calculate the normals of the mesh by calculating the face normal of each triangle (if a triangle has vertices
	 * V0, V1, V2, get the vector perpendicular to the face Pf = (V2 - V0) x (V1 - V0). To calculate the
	 * vertex normal for V0 sum and then normalize all its shared face normals. If bInComputeWeightedNormals is true
	 * then the weight of each face normal that contributes to V0's normal is the area of the face multiplied by the V0
	 * corner angle of that face. If bInComputeWeightedNormals is false then the weight is 1.
	 *
	 * @param bInComputeWeightedNormals Whether or not to use weighted normal calculation. Defaults to false.
	 */
	UFUNCTION()
	void CalculateNormals(bool bInComputeWeightedNormals=false);

	/**
	 * Calculate tangents from the normals. Calculates normals first via CalculateNormals() if the mesh does not yet
	 * have normals.
	 *
	 * @param bInComputeWeightedNormals Whether or not to use weighted normal calculation if CalculateNormals() is
	 * called. Defaults to false.
	 */
	UFUNCTION()
	void CalculateTangents(bool bInComputeWeightedNormals=false);
	
	/**
	 * Meant to be called after the mesh data arrays are populated.
	 * Currently only calls Shrink on the arrays
	 */
	UFUNCTION()
	void Optimize();

	UFUNCTION()
	FBox CalcBounds() const;

	UFUNCTION()
	const TArray<FVector3f>& GetVertexPositions() const { return VertexPositions; }

	UFUNCTION()
	const TArray<FIntVector>& GetTriangleIndices() const { return TriangleIndices; }

	UFUNCTION()
	const TArray<FColor>& GetVertexInstanceColors() const { return VertexInstanceColors; }

	UFUNCTION()
	const TArray<FVector3f>& GetVertexInstanceNormals() const { return VertexInstanceNormals; }

	UFUNCTION()
	const TArray<FVector3f>& GetVertexInstanceUTangents() const { return VertexInstanceUTangents; }

	UFUNCTION()
	const TArray<FVector3f>& GetVertexInstanceVTangents() const { return VertexInstanceVTangents; }

	UFUNCTION()
	const TArray<FVector2f>& GetVertexInstanceUVs() const { return VertexInstanceUVs; }

	UFUNCTION()
	const TArray<int32>& GetMaterialIDsPerTriangle() const { return MaterialIDsPerTriangle; }

	UFUNCTION()
	const TArray<FStaticMaterial>& GetStaticMaterials() const { return StaticMaterials; }

	TArray<FStaticMaterial>& GetStaticMaterials() { return StaticMaterials; }

	UFUNCTION()
	UMaterialInterface* GetMaterial(int32 InMaterialIndex);

	UFUNCTION()
	int32 GetMaterialIndex(FName InMaterialSlotName) const;

	// Checks if the mesh is valid by checking face, vertex and attribute (normals etc) counts.
	// If bSkipVertexIndicesCheck is true, then we don't loop over all triangle vertex indices to
	// check if each index is valid (< NumVertices)
	UFUNCTION()
	bool IsValid(bool bInSkipVertexIndicesCheck=false) const;

	// Custom serialization: we use TArray::BulkSerialize to speed up array serialization
	virtual void Serialize(FArchive &InArchive) override;

protected:

	UPROPERTY()
	bool bHasNormals;

	UPROPERTY()
	bool bHasTangents;

	UPROPERTY()
	bool bHasColors;

	/** The number of UV layers that the mesh has */
	UPROPERTY()
	uint32 NumUVLayers;

	UPROPERTY()
	bool bHasPerFaceMaterials;

	/** Vertex positions. The vertex id == vertex index => indexes into this array. */
	UPROPERTY(SkipSerialization)
	TArray<FVector3f> VertexPositions;

	/** Triangle vertices. Triangle id == triangle index => indexes into this array, which returns a FIntVector of
	 * vertex ids/indices for VertexPositions.
	 */
	UPROPERTY(SkipSerialization)
	TArray<FIntVector> TriangleIndices;

	/** Array of colors per vertex instance, in other words, a color per triangle-vertex. Index 3 * TriangleID + LocalTriangleVertexIndex. */
	UPROPERTY(SkipSerialization)
	TArray<FColor> VertexInstanceColors;

	/** Array of normals per vertex instance, in other words, a normal per triangle-vertex. Index 3 * TriangleID + LocalTriangleVertexIndex. */
	UPROPERTY(SkipSerialization)
	TArray<FVector3f> VertexInstanceNormals;

	/** Array of U tangents per vertex instance, in other words, a tangent per triangle-vertex. Index 3 * TriangleID + LocalTriangleVertexIndex. */
	UPROPERTY(SkipSerialization)
	TArray<FVector3f> VertexInstanceUTangents;

	/** Array of V tangents per vertex instance, in other words, a tangent per triangle-vertex. Index 3 * TriangleID + LocalTriangleVertexIndex. */
	UPROPERTY(SkipSerialization)
	TArray<FVector3f> VertexInstanceVTangents;

	/** Array of UV layers to array of per triangle-vertex UVs. Index: UVLayerIndex * (NumVertexInstances) + 3 * TriangleID + LocalTriangleVertexIndex. */
	UPROPERTY(SkipSerialization)
	TArray<FVector2f> VertexInstanceUVs;

	/** Array of material ID per triangle. Indexed by Triangle ID/Index. */
	UPROPERTY(SkipSerialization)
	TArray<int32> MaterialIDsPerTriangle;

	/** The materials of the mesh. Index by MaterialID (MaterialIndex). */
	UPROPERTY()
	TArray<FStaticMaterial> StaticMaterials;
};

