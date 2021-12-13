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

#include "HoudiniStaticMesh.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "Async/ParallelFor.h"
#include "MeshUtilitiesCommon.h"

UHoudiniStaticMesh::UHoudiniStaticMesh(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	bHasNormals = false;
	bHasTangents = false;
	bHasColors = false;
	NumUVLayers = 0;
	bHasPerFaceMaterials = false;
}

void 
UHoudiniStaticMesh::Initialize(uint32 InNumVertices, uint32 InNumTriangles, uint32 InNumUVLayers, uint32 InInitialNumStaticMaterials, bool bInHasNormals, bool bInHasTangents, bool bInHasColors, bool bInHasPerFaceMaterials)
{
	// Initialize the vertex positions and triangle indices arrays
	VertexPositions.SetNumUninitialized(InNumVertices);
	for(int32 n = 0; n < VertexPositions.Num(); n++)
		VertexPositions[n] = FVector::ZeroVector;

	TriangleIndices.SetNumUninitialized(InNumTriangles);
	for (int32 n = 0; n < TriangleIndices.Num(); n++)
		TriangleIndices[n] = FIntVector(-1, -1, -1);

	if (InInitialNumStaticMaterials > 0)
	{
		StaticMaterials.SetNumUninitialized(InNumTriangles);
		for (int32 n = 0; n < StaticMaterials.Num(); n++)
			StaticMaterials[n] = FStaticMaterial();
	}
	else
		StaticMaterials.Empty();

	SetNumUVLayers(InNumUVLayers);
	SetHasNormals(bInHasNormals);
	SetHasTangents(bInHasTangents);
	SetHasColors(bInHasColors);
	SetHasPerFaceMaterials(bInHasPerFaceMaterials);
}

void 
UHoudiniStaticMesh::SetHasPerFaceMaterials(bool bInHasPerFaceMaterials)
{
	bHasPerFaceMaterials = bInHasPerFaceMaterials;
	if (bHasPerFaceMaterials)
	{
		MaterialIDsPerTriangle.SetNumUninitialized(GetNumTriangles());
		for (int32 n = 0; n < MaterialIDsPerTriangle.Num(); n++)
			MaterialIDsPerTriangle[n] = -1;
	}
	else
		MaterialIDsPerTriangle.Empty();
}

void 
UHoudiniStaticMesh::SetHasNormals(bool bInHasNormals)
{
	bHasNormals = bInHasNormals;
	if (bHasNormals)
	{
		VertexInstanceNormals.SetNumUninitialized(GetNumVertexInstances());
		for (int32 n = 0; n < VertexInstanceNormals.Num(); n++)
			VertexInstanceNormals[n] = FVector(0, 0, 1);
	}
	else
		VertexInstanceNormals.Empty();
}

void 
UHoudiniStaticMesh::SetHasTangents(bool bInHasTangents)
{
	bHasTangents = bInHasTangents;
	if (bHasTangents)
	{
		VertexInstanceUTangents.SetNumUninitialized(GetNumVertexInstances());
		for (int32 n = 0; n < VertexInstanceUTangents.Num(); n++)
			VertexInstanceUTangents[n] = FVector(1, 0, 0);

		VertexInstanceVTangents.SetNumUninitialized(GetNumVertexInstances());
		for (int32 n = 0; n < VertexInstanceVTangents.Num(); n++)
			VertexInstanceVTangents[n] = FVector(0, 1, 0);
	}
	else
	{
		VertexInstanceUTangents.Empty();
		VertexInstanceVTangents.Empty();
	}
}

void 
UHoudiniStaticMesh::SetHasColors(bool bInHasColors)
{
	bHasColors = bInHasColors;
	if (bHasColors)
	{
		VertexInstanceColors.SetNumUninitialized(GetNumVertexInstances());
		for (int32 n = 0; n < VertexInstanceColors.Num(); n++)
			VertexInstanceColors[n] = FColor(127, 127, 127);
	}
	else
		VertexInstanceColors.Empty();
}

void UHoudiniStaticMesh::SetNumUVLayers(uint32 InNumUVLayers)
{
	NumUVLayers = InNumUVLayers;
	if (NumUVLayers > 0)
	{
		VertexInstanceUVs.SetNumUninitialized(GetNumVertexInstances() * NumUVLayers);
		for (int32 n = 0; n < VertexInstanceUVs.Num(); n++)
			VertexInstanceUVs[n] = FVector2D::ZeroVector;
	}
	else
		VertexInstanceUVs.Empty();
}

void UHoudiniStaticMesh::SetNumStaticMaterials(uint32 InNumMaterials)
{
	if (InNumMaterials > 0)
		StaticMaterials.SetNum(InNumMaterials);
	else
		StaticMaterials.Empty();
}

void UHoudiniStaticMesh::SetVertexPosition(uint32 InVertexIndex, const FVector& InPosition)
{
	check(VertexPositions.IsValidIndex(InVertexIndex));

	VertexPositions[InVertexIndex] = InPosition;
}

void UHoudiniStaticMesh::SetTriangleVertexIndices(uint32 InTriangleIndex, const FIntVector& InTriangleVertexIndices)
{
	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	check(VertexPositions.IsValidIndex(InTriangleVertexIndices[0]));
	check(VertexPositions.IsValidIndex(InTriangleVertexIndices[1]));
	check(VertexPositions.IsValidIndex(InTriangleVertexIndices[2]));

	TriangleIndices[InTriangleIndex] = InTriangleVertexIndices;
}

void UHoudiniStaticMesh::SetTriangleVertexNormal(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector& InNormal)
{
	if (!bHasNormals)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceNormals.IsValidIndex(VertexInstanceIndex));

	VertexInstanceNormals[VertexInstanceIndex] = InNormal;
}

void UHoudiniStaticMesh::SetTriangleVertexUTangent(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector& InUTangent)
{
	if (!bHasTangents)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceUTangents.IsValidIndex(VertexInstanceIndex));

	VertexInstanceUTangents[VertexInstanceIndex] = InUTangent;
}

void UHoudiniStaticMesh::SetTriangleVertexVTangent(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector& InVTangent)
{
	if (!bHasTangents)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceVTangents.IsValidIndex(VertexInstanceIndex));

	VertexInstanceVTangents[VertexInstanceIndex] = InVTangent;
}

void UHoudiniStaticMesh::SetTriangleVertexColor(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FColor& InColor)
{
	if (!bHasColors)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceColors.IsValidIndex(VertexInstanceIndex));

	VertexInstanceColors[VertexInstanceIndex] = InColor;
}

void UHoudiniStaticMesh::SetTriangleVertexUV(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, uint8 InUVLayer, const FVector2D& InUV)
{
	if (NumUVLayers <= 0)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceUVIndex = InUVLayer * GetNumVertexInstances() + InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceUVs.IsValidIndex(VertexInstanceUVIndex));

	VertexInstanceUVs[VertexInstanceUVIndex] = InUV;
}

void UHoudiniStaticMesh::SetTriangleMaterialID(uint32 InTriangleIndex, int32 InMaterialID)
{
	if (!bHasPerFaceMaterials)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	check(MaterialIDsPerTriangle.IsValidIndex(InTriangleIndex));

	MaterialIDsPerTriangle[InTriangleIndex] = InMaterialID;
}

void UHoudiniStaticMesh::SetStaticMaterial(uint32 InMaterialIndex, const FStaticMaterial& InStaticMaterial)
{
	check(StaticMaterials.IsValidIndex(InMaterialIndex));
	StaticMaterials[InMaterialIndex] = InStaticMaterial;
}

void UHoudiniStaticMesh::CalculateNormals(bool bInComputeWeightedNormals)
{
	const int32 NumVertexInstances = GetNumVertexInstances();

	// Pre-allocate space in the vertex instance normals array
	VertexInstanceNormals.SetNum(NumVertexInstances);

	const int32 NumTriangles = GetNumTriangles();
	const int32 NumVertices = GetNumVertices();
	
	// Setup a vertex normal array
	TArray<FVector> VertexNormals;
	VertexNormals.SetNum(NumVertices);

	// Zero all entries in VertexNormals
	// for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	ParallelFor(NumVertices, [&VertexNormals](int32 VertexIndex) 
	{
		VertexNormals[VertexIndex] = FVector::ZeroVector;
	});

	// Calculate face normals and sum them for each vertex that shares the triangle
	// for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	ParallelFor(NumTriangles, [this, &VertexNormals, bInComputeWeightedNormals](int32 TriangleIndex) 
	{
		const FIntVector& TriangleVertexIndices = TriangleIndices[TriangleIndex];

		if (!VertexPositions.IsValidIndex(TriangleVertexIndices[0]) || 
				!VertexPositions.IsValidIndex(TriangleVertexIndices[1]) ||
				!VertexPositions.IsValidIndex(TriangleVertexIndices[2]))
		{
			HOUDINI_LOG_WARNING(
				TEXT("[UHoudiniStaticMesh::CalculateNormals]: VertexPositions index out of range %d, %d, %d, Num %d"),
				TriangleVertexIndices[0], TriangleVertexIndices[1], TriangleVertexIndices[2], VertexPositions.Num());
			return;
		}

		const FVector& V0 = VertexPositions[TriangleVertexIndices[0]];
		const FVector& V1 = VertexPositions[TriangleVertexIndices[1]];
		const FVector& V2 = VertexPositions[TriangleVertexIndices[2]];
		
		FVector TriangleNormal = FVector::CrossProduct(V2 - V0, V1 - V0);
		float Area = TriangleNormal.Size();
		TriangleNormal /= Area;
		Area /= 2.0f;

		const float Weight[3] = {
			bInComputeWeightedNormals ? Area * TriangleUtilities::ComputeTriangleCornerAngle(V0, V1, V2) : 1.0f,
			bInComputeWeightedNormals ? Area * TriangleUtilities::ComputeTriangleCornerAngle(V1, V2, V0) : 1.0f,
			bInComputeWeightedNormals ? Area * TriangleUtilities::ComputeTriangleCornerAngle(V2, V0, V1) : 1.0f,
		};

		for (int CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
		{
			const FVector WeightedNormal = TriangleNormal * Weight[CornerIndex];
			if (!WeightedNormal.IsNearlyZero(SMALL_NUMBER) && !WeightedNormal.ContainsNaN())
			{
				if (!VertexNormals.IsValidIndex(TriangleVertexIndices[CornerIndex]))
				{
					HOUDINI_LOG_WARNING(
						TEXT("[UHoudiniStaticMesh::CalculateNormals]: VertexNormal index out of range %d, Num %d"),
						TriangleVertexIndices[CornerIndex], VertexNormals.Num());
					continue;
				}
				VertexNormals[TriangleVertexIndices[CornerIndex]] += WeightedNormal;
			}
		}
	});

	// Normalize the vertex normals
	// for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	ParallelFor(NumVertices, [&VertexNormals](int32 VertexIndex) 
	{
		VertexNormals[VertexIndex].Normalize();
	});

	// Copy vertex normals to vertex instance normals
	// for (int32 VertexInstanceIndex = 0; VertexInstanceIndex < NumVertexInstances; ++VertexInstanceIndex)
	ParallelFor(NumVertexInstances, [this, &VertexNormals](int32 VertexInstanceIndex) 
	{
		const int32 TriangleIndex = VertexInstanceIndex / 3;
		const int32 CornerIndex = VertexInstanceIndex % 3;
		const FIntVector& TriangleVertexIndices = TriangleIndices[TriangleIndex];
		if (!VertexNormals.IsValidIndex(TriangleVertexIndices[CornerIndex]))
		{
			HOUDINI_LOG_WARNING(
				TEXT("[UHoudiniStaticMesh::CalculateNormals]: VertexNormals index out of range %d, Num %d"),
				TriangleVertexIndices[CornerIndex], VertexNormals.Num());
			return;
		}
		VertexInstanceNormals[VertexInstanceIndex] = VertexNormals[TriangleVertexIndices[CornerIndex]];
	});

	bHasNormals = true;
}

void UHoudiniStaticMesh::CalculateTangents(bool bInComputeWeightedNormals)
{
	const int32 NumVertexInstances = GetNumVertexInstances();

	VertexInstanceUTangents.SetNum(NumVertexInstances);
	VertexInstanceVTangents.SetNum(NumVertexInstances);

	// Calculate normals first if we don't have any
	if (!HasNormals() || VertexInstanceNormals.Num() != NumVertexInstances)
		CalculateNormals(bInComputeWeightedNormals);

	// for (int32 VertexInstanceIndex = 0; VertexInstanceIndex < NumVertexInstances; ++VertexInstanceIndex)
	ParallelFor(NumVertexInstances, [this](int32 VertexInstanceIndex) 
	{
		const FVector& Normal = VertexInstanceNormals[VertexInstanceIndex];
		Normal.FindBestAxisVectors(
			VertexInstanceUTangents[VertexInstanceIndex], VertexInstanceVTangents[VertexInstanceIndex]);
	});

	bHasTangents = true;
}

void UHoudiniStaticMesh::Optimize()
{
	VertexPositions.Shrink();
	TriangleIndices.Shrink();
	VertexInstanceColors.Shrink();
	VertexInstanceNormals.Shrink();
	VertexInstanceUTangents.Shrink();
	VertexInstanceVTangents.Shrink();
	VertexInstanceUVs.Shrink();
	MaterialIDsPerTriangle.Shrink();
	StaticMaterials.Shrink();
}

FBox UHoudiniStaticMesh::CalcBounds() const
{
	const uint32 NumVertices = VertexPositions.Num();

	if (NumVertices == 0)
		return FBox();

	const FVector InitPosition = VertexPositions[0];
	double MinX = InitPosition.X, MaxX = InitPosition.X, MinY = InitPosition.Y, MaxY = InitPosition.Y, MinZ = InitPosition.Z, MaxZ = InitPosition.Z;
	for (uint32 VertIdx = 0; VertIdx < NumVertices; ++VertIdx)
	{
		const FVector Position = VertexPositions[VertIdx];
		if (Position.X < MinX) MinX = Position.X; else if (Position.X > MaxX) MaxX = Position.X;
		if (Position.Y < MinY) MinY = Position.Y; else if (Position.Y > MaxY) MaxY = Position.Y;
		if (Position.Z < MinZ) MinZ = Position.Z; else if (Position.Z > MaxZ) MaxZ = Position.Z;
	}

	return FBox(FVector(MinX, MinY, MinZ), FVector(MaxX, MaxY, MaxZ));
}

UMaterialInterface* UHoudiniStaticMesh::GetMaterial(int32 InMaterialIndex)
{
	check(StaticMaterials.IsValidIndex(InMaterialIndex));

	return StaticMaterials[InMaterialIndex].MaterialInterface;
}

int32 UHoudiniStaticMesh::GetMaterialIndex(FName InMaterialSlotName) const
{
	if (InMaterialSlotName == NAME_None)
		return -1;

	const uint32 NumMaterials = StaticMaterials.Num();
	for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		if (StaticMaterials[MaterialIndex].MaterialSlotName == InMaterialSlotName)
			return (int32)MaterialIndex;
	}

	return -1;
}

bool UHoudiniStaticMesh::IsValid(bool bInSkipVertexIndicesCheck) const
{
	// Validate the number of vertices, indices and triangles. This is basically the same function as FRawMesh::IsValid()
	const int32 NumVertices = GetNumVertices();
	const int32 NumVertexInstances = GetNumVertexInstances();
	const int32 NumTriangles = GetNumTriangles();

	auto ValidateAttributeArraySize = [](int32 InArrayNum, int32 InExpectedSize)
	{
		return InArrayNum == 0 || InArrayNum == InExpectedSize;
	};
	
	bool bValid = NumVertices > 0
		&& NumVertexInstances > 0
		&& NumTriangles > 0
		&& (NumVertexInstances / 3) == NumTriangles
		&& ValidateAttributeArraySize(MaterialIDsPerTriangle.Num(), NumTriangles)
		&& ValidateAttributeArraySize(VertexInstanceNormals.Num(), NumVertexInstances)
		&& ValidateAttributeArraySize(VertexInstanceUTangents.Num(), NumVertexInstances)
		&& ValidateAttributeArraySize(VertexInstanceVTangents.Num(), NumVertexInstances)
		&& ValidateAttributeArraySize(VertexInstanceColors.Num(), NumVertexInstances)
		&& NumUVLayers >= 0
		&& VertexInstanceUVs.Num() == NumUVLayers * NumVertexInstances; 

	if (!bInSkipVertexIndicesCheck)
	{
		int32 TriangleIndex = 0;
		while (bValid && TriangleIndex < NumTriangles)
		{
			bValid = bValid && (TriangleIndices[TriangleIndex].X < NumVertices);
			bValid = bValid && (TriangleIndices[TriangleIndex].Y < NumVertices);
			bValid = bValid && (TriangleIndices[TriangleIndex].Z < NumVertices);
			TriangleIndex++;
		}
	}

	return bValid;
}

void UHoudiniStaticMesh::Serialize(FArchive &InArchive)
{
	Super::Serialize(InArchive);

	VertexPositions.Shrink();
	VertexPositions.BulkSerialize(InArchive);

	TriangleIndices.Shrink();
	TriangleIndices.BulkSerialize(InArchive);

	VertexInstanceColors.Shrink();
	VertexInstanceColors.BulkSerialize(InArchive);

	VertexInstanceNormals.Shrink();
	VertexInstanceNormals.BulkSerialize(InArchive);

	VertexInstanceUTangents.Shrink();
	VertexInstanceUTangents.BulkSerialize(InArchive);

	VertexInstanceVTangents.Shrink();
	VertexInstanceVTangents.BulkSerialize(InArchive);

	VertexInstanceUVs.Shrink();
	VertexInstanceUVs.BulkSerialize(InArchive);

	MaterialIDsPerTriangle.Shrink();
	MaterialIDsPerTriangle.BulkSerialize(InArchive);
}

