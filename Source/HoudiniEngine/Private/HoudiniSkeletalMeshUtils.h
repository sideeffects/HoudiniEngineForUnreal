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

#include "HAPI/HAPI_Common.h"

#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "CoreMinimal.h"
#include "Math/MathFwd.h"


struct FReferenceSkeletonModifier;

struct FHoudiniSkeletonBone
{
	FString Name;
	FTransform UnrealGlobalTransform;
	FTransform UnrealLocalMatrix;
	int UnrealBoneNumber = -1;
	int HoudiniBoneNumber = -1;;
	TArray<FHoudiniSkeletonBone *> Children;
	FHoudiniSkeletonBone * Parent = nullptr;
};


struct FHoudiniSkeleton
{
	TArray<FHoudiniSkeletonBone> Bones;
	FHoudiniSkeletonBone * Root = nullptr;
	TMap<FString, FHoudiniSkeletonBone*> BoneMap;
	TMap<int, FString> HoudiniBoneMap;
};

struct FHoudiniSkinInfluence
{
	// Don't be tempted to store a string here, there are tens of millions of these in an import.
	// Keep this class simple.
	FHoudiniSkeletonBone * Bone = nullptr;
	float Weight = 0.0f;
};
struct FHoudiniInfluences
{
	TArray<FHoudiniSkinInfluence> Influences;
	int NumInfluences = 0;
	int NumVertices = 0;
};

struct FHoudiniSkeletalMeshMaterial
{
	FString OverridePath; // valid if using unreal_material
	HAPI_NodeId NodeId; // valid if using a houdini material
	FString AssetPath;	// asset path of material to use.
	int Slot; // material slot number. may be derived from OverridePath.
};


struct FHoudiniSkeletalMeshMaterialSettings
{
	TArray< FHoudiniSkeletalMeshMaterial> Materials;
	TArray<int> MaterialIds;
	HAPI_NodeId GeoNodeId = -1;
	bool bHoudiniMaterials = false;
};

struct HOUDINIENGINE_API FHoudiniSkeletalMeshUtils
{
	static FHoudiniSkeleton UnrealToHoudiniSkeleton(USkeleton * Skeleton);

	static FHoudiniSkeleton FetchSkeleton(HAPI_NodeId PoseNodeId, HAPI_PartId PosePartId);

	static FHoudiniInfluences FetchInfluences(HAPI_NodeId NodeId, HAPI_PartId PartId, FHoudiniSkeleton& Skeleton);

	static FHoudiniSkeletalMeshMaterialSettings GetHoudiniMaterials(HAPI_NodeId NodeId, HAPI_PartId PartId, int NumFaces);

	static bool CreateHoudiniMaterial(
		FHoudiniSkeletalMeshMaterialSettings& SkeletalFaceMaterials,
			TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> InputAssignmentMaterials,
			TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> AllOutputMaterials,
			TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> OutputAssignmentMaterials,
			const FHoudiniPackageParams& InPackageParams);

	static FHoudiniSkeletalMeshMaterialSettings GetMaterialOverrides(HAPI_NodeId NodeId, HAPI_PartId PartId);

	static bool AddBonesToUnrealSkeleton(USkeleton * UnrealSkeleton, const FHoudiniSkeleton * HoudiniSkeleton);

	static void AddBonesToUnrealSkeleton(FReferenceSkeletonModifier& RefSkeletonModifier, const FHoudiniSkeletonBone* Bone);

	static bool RemapInfluences(FHoudiniInfluences & Influences, const FHoudiniSkeleton & NewSkeleton);

	static FMatrix MakeMatrixFromHoudiniData(const float Rotation[], const float Position[]);

	static FMatrix UnrealToHoudiniMatrix(const FTransform& Transform);

	static FMatrix UnrealToHoudiniMatrix(const FMatrix& Transform);

	static void UnrealToHoudiniMatrix(FMatrix& UnrealMatrix, float Rotation[], float Position[]);

	static void UnrealToHoudiniMatrix(FMatrix& UnrealMatrix, float Matrix[]);

	static void ConstructLocalMatricesFromGlobal(FHoudiniSkeletonBone* Node, const FHoudiniSkeletonBone* Parent);

	static TArray<FHoudiniSkeletonBone> CreateSortedBoneList(TArray<FHoudiniSkeletonBone> & UnsortedBones, int RootIndex);

};

