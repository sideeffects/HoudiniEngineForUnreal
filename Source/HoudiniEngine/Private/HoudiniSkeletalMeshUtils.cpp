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

#include "HoudiniSkeletalMeshUtils.h"

#include "HoudiniSkeletalMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniMeshTranslator.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "IMeshBuilderModule.h"
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ReferenceSkeleton.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "Engine/SkinnedAssetCommon.h"
#endif
#include <functional>

#include "Chaos/ChaosPerfTest.h"
#include "Internationalization/Regex.h"

FMatrix
FHoudiniSkeletalMeshUtils::MakeMatrixFromHoudiniData(const float RotationData[], const float PositionData[])
{
	// Convert Houdini matrix to data to Unreal data. Y/Z axis are swapped so we each column 1 and 2 are switched.
	// In addition the Y/Z vectors need to be switched.

	FMatrix M44Pose;  
	M44Pose.M[0][0] = RotationData[0];
	M44Pose.M[0][1] = RotationData[2];
	M44Pose.M[0][2] = RotationData[1];
	M44Pose.M[0][3] = 0;
	M44Pose.M[1][0] = RotationData[6];
	M44Pose.M[1][1] = RotationData[8];
	M44Pose.M[1][2] = RotationData[7];
	M44Pose.M[1][3] = 0;
	M44Pose.M[2][0] = RotationData[3];
	M44Pose.M[2][1] = RotationData[5];
	M44Pose.M[2][2] = RotationData[4];
	M44Pose.M[2][3] = 0;
	M44Pose.M[3][0] = PositionData[0] * 100.0;
	M44Pose.M[3][1] = PositionData[2] * 100.0;
	M44Pose.M[3][2] = PositionData[1] * 100.0;
	M44Pose.M[3][3] = 1;

	return M44Pose;
}

FMatrix FHoudiniSkeletalMeshUtils::UnrealToHoudiniMatrix(const FTransform& Transform)
{
	FMatrix UnrealMatrix = Transform.ToMatrixWithScale();
	return UnrealToHoudiniMatrix(UnrealMatrix);
}

void FHoudiniSkeletalMeshUtils::UnrealToHoudiniMatrix(FMatrix& UnrealMatrix, float Matrix[])
{
	FMatrix HoudiniMatrix = UnrealToHoudiniMatrix(UnrealMatrix);

	for (int Row = 0; Row < 4; Row++)
	{
		for (int Col = 0; Col < 4; Col++)
		{
			Matrix[Row * 4 + Col] = HoudiniMatrix.M[Row][Col];
		}
	}
}


FMatrix FHoudiniSkeletalMeshUtils::UnrealToHoudiniMatrix(const FMatrix& UnrealMatrix)
{
	FMatrix Result;
	Result.M[0][0] = UnrealMatrix.M[0][0];
	Result.M[0][1] = UnrealMatrix.M[0][2];
	Result.M[0][2] = UnrealMatrix.M[0][1];
	Result.M[0][3] = UnrealMatrix.M[0][3];

	Result.M[1][0] = UnrealMatrix.M[2][0];
	Result.M[1][1] = UnrealMatrix.M[2][2];
	Result.M[1][2] = UnrealMatrix.M[2][1];
	Result.M[1][3] = UnrealMatrix.M[2][3];

	Result.M[2][0] = UnrealMatrix.M[1][0];
	Result.M[2][1] = UnrealMatrix.M[1][2];
	Result.M[2][2] = UnrealMatrix.M[1][1];
	Result.M[2][3] = UnrealMatrix.M[1][3];

	Result.M[3][0] = UnrealMatrix.M[3][0] * 0.01;
	Result.M[3][1] = UnrealMatrix.M[3][2] * 0.01;
	Result.M[3][2] = UnrealMatrix.M[3][1] * 0.01;
	Result.M[3][3] = UnrealMatrix.M[3][3];

	return Result;
}

void FHoudiniSkeletalMeshUtils::UnrealToHoudiniMatrix(FMatrix& UnrealMatrix, float Rotation[], float Position[])
{
	FMatrix HoudiniMatrix = UnrealToHoudiniMatrix(UnrealMatrix);

	for (int Col = 0; Col < 3; Col++)
	{
		for (int Row = 0; Row < 3; Row++)
		{
			Rotation[Row * 3 + Col] = HoudiniMatrix.M[Row][Col];
		}
		Position[Col] = HoudiniMatrix.M[3][Col];
	}
}


FHoudiniSkeleton FHoudiniSkeletalMeshUtils::FetchSkeleton(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	FHoudiniSkeleton Result;

	// Houdini stores the bone names in the "name" attribute on the point. However, when this data is fetched HAPI
	// doesn't return just the name, it expands the points so there is one per vertex. So for now, we have to assign
	// an extra attribute to determine which preserves the bone number.

	TArray<FString> ParentChild;
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(NodeId, PartId, "name");
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, ParentChild);

	if (ParentChild.IsEmpty())
	{
		HOUDINI_LOG_ERROR(TEXT("No name found on skeleton"));
		return {};
	}

	TArray<int> ParentChildBoneNumbers;
	Accessor.Init(NodeId, PartId, "__bone_id");
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, ParentChildBoneNumbers);

	if (ParentChildBoneNumbers.Num() != ParentChild.Num())
	{
		HOUDINI_LOG_ERROR(TEXT("No __bone_id found on skeleton"));
		return {};
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Get all bone names and uses this to seed the skeleton
	//--------------------------------------------------------------------------------------------------------------------

	TArray<FString> BoneNames;
	TMap<FString, int> BoneNamesToPointIndex;
	for(int Index = 0; Index < ParentChild.Num(); Index++)
	{
		BoneNamesToPointIndex.Add(ParentChild[Index], Index);
	}

	BoneNamesToPointIndex.GetKeys(BoneNames);

	if (BoneNames.IsEmpty())
	{
		HOUDINI_LOG_ERROR(TEXT("No Bone names found on skeleton"));
		return {};
	}

	// Fill in bone names.
	TArray<FHoudiniSkeletonBone> Bones;
	Bones.SetNum(BoneNames.Num());
	TMap<FString, FHoudiniSkeletonBone*> BoneMap;

	for(int Index = 0; Index < BoneNames.Num(); Index++)
	{
		FHoudiniSkeletonBone& Bone = Bones[Index];
		Bone.Name = BoneNames[Index];
		BoneMap.Add(Bone.Name, &Bone);
	}

	for (int Index = 0; Index < ParentChild.Num(); Index++)
	{
		FHoudiniSkeletonBone* Node = BoneMap[ParentChild[Index]];
		if (!Result.HoudiniBoneMap.Contains(ParentChildBoneNumbers[Index]))
		{
			Node->HoudiniBoneNumber = ParentChildBoneNumbers[Index];
			Result.HoudiniBoneMap.Add(Node->HoudiniBoneNumber, Node->Name);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Read matrices
	//--------------------------------------------------------------------------------------------------------------------

	TArray<float> RotationData; // 9 floats per bone
	TArray<float> PositionData; // 3 floats per bone

	Accessor.Init(NodeId, PartId, "transform");
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, RotationData);
	Accessor.Init(NodeId, PartId, "P");
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, PositionData);

	for(int Index = 0; Index < Bones.Num(); Index++)
	{
		FHoudiniSkeletonBone* Bone = &Bones[Index];
		int PointIndex = BoneNamesToPointIndex[Bone->Name];

		Bone->UnrealGlobalTransform = FTransform(MakeMatrixFromHoudiniData(&RotationData[PointIndex * 9], &PositionData[PointIndex * 3]));
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Fill in parent - child relationships
	//--------------------------------------------------------------------------------------------------------------------

	for(int Index = 0; Index < ParentChild.Num(); Index+= 2)
	{
		const FString & ParentName = ParentChild[Index];
		const FString& ChildName = ParentChild[Index + 1];

		FHoudiniSkeletonBone* Parent = BoneMap[ParentName];
		FHoudiniSkeletonBone* Child = BoneMap[ChildName];

		if (!Parent || !Child)
		{
			HOUDINI_LOG_ERROR(TEXT("Missing bone names: %s or %s "), *ParentName, *ChildName);
			return {};
		}
		Child->Parent = Parent;
		Parent->Children.Add(Child);
	}

	// Root is first bone with no parent
	Result.Root = nullptr;
	int RootIndex = INDEX_NONE;

	for (int BoneIndex = 0;BoneIndex < Bones.Num(); BoneIndex++)
	{
		if(!Bones[BoneIndex].Parent)
		{
			RootIndex = BoneIndex;
		}
	}

	if (RootIndex == INDEX_NONE)
	{
		HOUDINI_LOG_ERROR(TEXT("No root found on skeleton"));
		return {};
	}

    Result.Bones = CreateSortedBoneList(Bones, RootIndex);

    Result.Root = &Result.Bones[0];

	for(int Index = 0; Index < Result.Bones.Num(); Index++)
	{
		FHoudiniSkeletonBone* Bone = &Result.Bones[Index];
		Result.BoneMap.Add(Bone->Name, Bone);
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Convert Houdini matrices to Unreal matrix, and calculate the local matrices(which is what Unreal wants).
	//--------------------------------------------------------------------------------------------------------------------

	ConstructLocalMatricesFromGlobal(Result.Root, nullptr);

	return Result;
}

TArray<FHoudiniSkeletonBone>
FHoudiniSkeletalMeshUtils::CreateSortedBoneList(TArray<FHoudiniSkeletonBone>& UnsortedBones, int RootIndex)
{
	TArray<FHoudiniSkeletonBone> SortedBones;
	SortedBones.SetNum(UnsortedBones.Num());
	int NextFreeSlot = 0;
	TMap<FHoudiniSkeletonBone*, FHoudiniSkeletonBone*> Remap;
	Remap.Add(nullptr, nullptr);

	std::function<void(FHoudiniSkeletonBone*)> AddChildren = [&](FHoudiniSkeletonBone* Parent)
	{
		Remap.Add(Parent, &SortedBones[NextFreeSlot]);
		SortedBones[NextFreeSlot++] = *Parent;
		for (auto& Child : Parent->Children)
		{
			AddChildren(Child);
		}
	};

	AddChildren(&UnsortedBones[RootIndex]);

	for (int Index = 0; Index < SortedBones.Num(); Index++)
	{
		SortedBones[Index].UnrealBoneNumber = Index;
		SortedBones[Index].Parent = Remap[SortedBones[Index].Parent];
		for (int ChildIndex = 0; ChildIndex < SortedBones[Index].Children.Num(); ChildIndex++)
		{
			SortedBones[Index].Children[ChildIndex] = Remap[SortedBones[Index].Children[ChildIndex]];
		}
	}

	return SortedBones;
}

FHoudiniInfluences FHoudiniSkeletalMeshUtils::FetchInfluences(HAPI_NodeId NodeId, HAPI_PartId PartId, FHoudiniSkeleton& Skeleton)
{
	HAPI_AttributeInfo BoneCaptureInfo;
	TArray<float> BoneCaptureData;
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(NodeId, PartId, "boneCapture");
	Accessor.GetInfo(BoneCaptureInfo, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT);
	Accessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, BoneCaptureData);

	TArray<FString> BoneNames;
	TArray<int> BoneNameIndices;
	Accessor.Init(NodeId, PartId, "boneCapture_pCaptPath");
	Accessor.GetAttributeArrayData(HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, BoneNames, BoneNameIndices);

	int HoudiniInfluencesPerVertex = BoneCaptureInfo.tupleSize / 2; // Divide by two compensate for the 'two floats' per capture data entry.

	const int MaxInfluences = 4; // TODO: Support more than 4.

	FHoudiniInfluences SkinWeights;
	SkinWeights.NumInfluences = FMath::Min(MaxInfluences, HoudiniInfluencesPerVertex);
	SkinWeights.NumVertices = BoneCaptureData.Num() / (2 * HoudiniInfluencesPerVertex);
	SkinWeights.Influences.SetNum(SkinWeights.NumInfluences * SkinWeights.NumVertices);

	TArray< FHoudiniSkinInfluence> InputInfluences;
	InputInfluences.SetNum(HoudiniInfluencesPerVertex);

	for(int VertexIndex = 0; VertexIndex < SkinWeights.NumVertices; VertexIndex++)
	{
		// Read input influences
		for(int InputInfluence = 0; InputInfluence < HoudiniInfluencesPerVertex; InputInfluence++)
		{
			int Index = VertexIndex * HoudiniInfluencesPerVertex + InputInfluence;
			int BoneIndex = static_cast<int>(BoneCaptureData[Index * 2]);
			float BoneWeight = BoneCaptureData[Index * 2 + 1];

			if (BoneWeight > 0.0f && BoneIndex != -1)
			{
				if(BoneIndex >= BoneNames.Num())
				{
					HOUDINI_LOG_ERROR(TEXT("Invalid bone index in bone capture."));
					return {};
				}

				const FString& BoneName = BoneNames[BoneIndex];
				InputInfluences[InputInfluence].Bone = Skeleton.BoneMap[BoneName];
				InputInfluences[InputInfluence].Weight = BoneWeight;
			}
			else
			{
				InputInfluences[InputInfluence].Bone = nullptr;
				InputInfluences[InputInfluence].Weight = 0.0f;
			}
		}

		// Sort the bone influences by weight (higher weight first) so we can get the most influencial.
		InputInfluences.Sort([](const FHoudiniSkinInfluence & Bone1, const FHoudiniSkinInfluence& Bone2) { return Bone1.Weight > Bone2.Weight; });
		float TotalWeight = 0.0f;
		for(int AddIndex = 0; AddIndex < SkinWeights.NumInfluences; AddIndex++)
			TotalWeight += InputInfluences[AddIndex].Weight;

		float WeightScale = 1.0f / TotalWeight;
		for (int AddIndex = 0; AddIndex < SkinWeights.NumInfluences; AddIndex++)
			InputInfluences[AddIndex].Weight *= WeightScale;

		// Add input influence to result
		for (int AddIndex = 0; AddIndex < SkinWeights.NumInfluences; AddIndex++)
			SkinWeights.Influences[VertexIndex * SkinWeights.NumInfluences + AddIndex] = InputInfluences[AddIndex];

		// If there are less influences in Houdini than required for Unreal, add dummy wights
		FHoudiniSkinInfluence EmptyInfluence;
		for(int Index = InputInfluences.Num(); Index < SkinWeights.NumInfluences; Index++)
		{
			SkinWeights.Influences[VertexIndex * SkinWeights.NumInfluences + Index] = EmptyInfluence;
		}
	}
	return SkinWeights;
}


FHoudiniSkeletalMeshMaterialSettings FHoudiniSkeletalMeshUtils::GetMaterialOverrides(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	// Fetch attributes and check its valid
	TArray<FString> AttributeData;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, AttributeData);
	if (!bSuccess || AttributeData.IsEmpty())
		return {};

	// Find all unique material overrides and create a FHoudiniSkeletalMeshMaterial per material.
	TMap<FString, FHoudiniSkeletalMeshMaterial> UniqueMaterials;
	int NextFreeSlot = 0;
	for(FString & Attribute : AttributeData)
	{
		if (!UniqueMaterials.Contains(Attribute))
		{
			FHoudiniSkeletalMeshMaterial Material;
			Material.OverridePath = Attribute;
			Material.AssetPath = Attribute;
			Material.Slot = NextFreeSlot++;

			// see if  there is a slot override. The name will begin the [0], [1], etc.
			if (Material.OverridePath.StartsWith("["))
			{
				FRegexPattern NumberPattern(TEXT("\\[(\\d+)\\]"));
				FRegexMatcher Matcher(NumberPattern, Material.OverridePath);
				if (Matcher.FindNext())
				{
					int32 BeginIndex = Matcher.GetCaptureGroupBeginning(1);
					int32 EndIndex = Matcher.GetCaptureGroupEnding(1);
					FString MatchedNumber = Material.OverridePath.Mid(BeginIndex, EndIndex - BeginIndex);
					if (MatchedNumber.IsNumeric())
					{
						Material.Slot = FCString::Atoi(*MatchedNumber);
						Material.AssetPath = Material.OverridePath.Mid(EndIndex + 1);
						Attribute = Material.AssetPath;
					}
				}
			}

			UniqueMaterials.Add(Attribute, Material);
		}
	}

	// Store all unique materials in the results, sorting by slot.
	FHoudiniSkeletalMeshMaterialSettings Result;
	for(auto & It : UniqueMaterials)
	{
		Result.Materials.Add(It.Value);
	}
	Result.Materials.Sort([](const FHoudiniSkeletalMeshMaterial & A, const FHoudiniSkeletalMeshMaterial & B) { return A.Slot < B.Slot; } );

	// Create per-face indexes into the material array
	TMap<FString, int> AttributeToMaterialIndex;
	for(int Index = 0; Index < Result.Materials.Num(); Index++)
	{
		AttributeToMaterialIndex.Add(Result.Materials[Index].AssetPath, Index);
	}

	Result.MaterialIds.SetNum(AttributeData.Num());
	for (int Index = 0; Index < AttributeData.Num(); Index++)
	{
		Result.MaterialIds[Index] = AttributeToMaterialIndex[AttributeData[Index]];
	}
	return Result;
}

FHoudiniSkeletalMeshMaterialSettings FHoudiniSkeletalMeshUtils::GetHoudiniMaterials(HAPI_NodeId NodeId, HAPI_PartId PartId, int NumFaces)
{
	TArray<HAPI_NodeId> MaterialNodes;
	MaterialNodes.SetNum(NumFaces);

	bool bSingleMaterial = false;

	HAPI_Result HapiResult = FHoudiniApi::GetMaterialNodeIdsOnFaces(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, &bSingleMaterial, MaterialNodes.GetData(), 0, NumFaces);

	if (HapiResult != HAPI_RESULT_SUCCESS)
		return {};

	FHoudiniSkeletalMeshMaterialSettings Result;

	TMap<HAPI_NodeId, int > UniqueMaterials;

	if (bSingleMaterial)
	{
		FHoudiniSkeletalMeshMaterial Material;
		Material.NodeId = MaterialNodes[0];
		Material.Slot = 0;
		UniqueMaterials.Add(MaterialNodes[0], 0);
		Result.Materials.Add(Material);
	}
	else
	{
		for(int Index = 0; Index < MaterialNodes.Num(); Index++)
		{
			if (!UniqueMaterials.Contains(NodeId))
			{
				FHoudiniSkeletalMeshMaterial Material;
				Material.NodeId = MaterialNodes[Index];
				Material.Slot = Result.Materials.Num();
				UniqueMaterials.Add(NodeId, Index);
				Result.Materials.Add(Material);
			}
		}
	}

	Result.MaterialIds.SetNum(NumFaces);
	for(int FaceId = 0; FaceId < NumFaces; FaceId++)
	{
		Result.MaterialIds[FaceId] = UniqueMaterials[MaterialNodes[FaceId]];
	}

	Result.GeoNodeId = NodeId;
	return Result;

}

bool FHoudiniSkeletalMeshUtils::CreateHoudiniMaterial(
	FHoudiniSkeletalMeshMaterialSettings& SkeletalFaceMaterials,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> InputAssignmentMaterials,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> AllOutputMaterials,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> OutputAssignmentMaterials,
	const FHoudiniPackageParams& InPackageParams)
{
	TArray<int> UniqueHoudiniMaterialIds;

	TArray<HAPI_MaterialInfo> UniqueHoudiniMaterialInfos;
	UniqueHoudiniMaterialInfos.SetNum(SkeletalFaceMaterials.Materials.Num());
	UniqueHoudiniMaterialIds.SetNum(SkeletalFaceMaterials.Materials.Num());

	// Fetch all material infos.
	for(int Index = 0; Index < SkeletalFaceMaterials.Materials.Num(); Index++)
	{
		UniqueHoudiniMaterialIds[Index] = SkeletalFaceMaterials.Materials[Index].NodeId;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), 
			SkeletalFaceMaterials.Materials[Index].NodeId, &UniqueHoudiniMaterialInfos[Index]), false);
	}

	// Create materials.

	TArray<UPackage*> MaterialAndTexturePackages;
	FHoudiniPackageParams PackageParams = InPackageParams;
	TArray<UMaterialInterface*> OutMaterialArray;

	if (!FHoudiniMaterialTranslator::CreateHoudiniMaterials(
		SkeletalFaceMaterials.GeoNodeId,
		InPackageParams,
		UniqueHoudiniMaterialIds,
		UniqueHoudiniMaterialInfos,
		InputAssignmentMaterials,
		AllOutputMaterials,
		OutputAssignmentMaterials,
		OutMaterialArray,
		MaterialAndTexturePackages,
		false,
		true,
		false))
	{
		return false;
	}

	// Set output materials.

	if (OutMaterialArray.Num() != SkeletalFaceMaterials.Materials.Num())
		return false;

	for(int Index = 0; Index < SkeletalFaceMaterials.Materials.Num(); Index++)
	{
		SkeletalFaceMaterials.Materials[Index].AssetPath = OutMaterialArray[Index]->GetPathName();
	}
	return true;
}


void FHoudiniSkeletalMeshUtils::AddBonesToUnrealSkeleton(FReferenceSkeletonModifier & RefSkeletonModifier, const FHoudiniSkeletonBone * Bone)
{

	int ParentId = INDEX_NONE;
	if (Bone->Parent)
		ParentId = RefSkeletonModifier.FindBoneIndex(FName(Bone->Parent->Name));

	FMeshBoneInfo RootBoneInfo(FName(Bone->Name), Bone->Name, ParentId);
	RefSkeletonModifier.Add(RootBoneInfo, Bone->UnrealLocalMatrix);

	for(auto & Child : Bone->Children)
		AddBonesToUnrealSkeleton(RefSkeletonModifier, Child);
}


bool FHoudiniSkeletalMeshUtils::AddBonesToUnrealSkeleton(USkeleton* UnrealSkeleton, const FHoudiniSkeleton* HoudiniSkeleton)
{
	FReferenceSkeletonModifier RefSkeletonModifier(UnrealSkeleton);

	AddBonesToUnrealSkeleton(RefSkeletonModifier, HoudiniSkeleton->Root);
	return true;
}

FHoudiniSkeleton FHoudiniSkeletalMeshUtils::UnrealToHoudiniSkeleton(USkeleton * UnrealSkeleton)
{
	FHoudiniSkeleton HoudiniSkeleton;

	const FReferenceSkeleton& RefSkeleton = UnrealSkeleton->GetReferenceSkeleton();

	TArray<FTransform> RefPose = RefSkeleton.GetRefBonePose();

	int32 BoneCount = RefSkeleton.GetNum();

	HoudiniSkeleton.Bones.SetNum(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();
		int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		FTransform & BoneTransform = RefPose[BoneIndex];

		HoudiniSkeleton.BoneMap.Add(BoneName, &HoudiniSkeleton.Bones[BoneIndex]);
		auto * ThisBone = &HoudiniSkeleton.Bones[BoneIndex];
		ThisBone->Name = BoneName;
		ThisBone->UnrealGlobalTransform = BoneTransform;
		ThisBone->UnrealLocalMatrix = BoneTransform;
		ThisBone->UnrealBoneNumber = BoneIndex;
		if (ParentIndex != INDEX_NONE)
		{
			auto* ParentBone = &HoudiniSkeleton.Bones[ParentIndex];
			ThisBone->Parent = &HoudiniSkeleton.Bones[ParentIndex];
			ParentBone->Children.Add(ThisBone);
		}
		else
		{
			ThisBone->Parent = nullptr;
			HoudiniSkeleton.Root = ThisBone;
		}
	}

	return HoudiniSkeleton;
}

void FHoudiniSkeletalMeshUtils::ConstructLocalMatricesFromGlobal(FHoudiniSkeletonBone* Node, const FHoudiniSkeletonBone* Parent) 
{
	FTransform ParentUnrealMatrix = FTransform::Identity;
	if (Parent != nullptr)
	{
		ParentUnrealMatrix = Parent->UnrealGlobalTransform;
	}
	Node->UnrealLocalMatrix = Node->UnrealGlobalTransform * ParentUnrealMatrix.Inverse();
	for (auto Child : Node->Children)
	{
		ConstructLocalMatricesFromGlobal(Child, Node);
	}
}

bool FHoudiniSkeletalMeshUtils::RemapInfluences(FHoudiniInfluences& Influences, const FHoudiniSkeleton& NewSkeleton)
{
	bool bErrors = false;

	TSet<FString> MissingBones;

	for(FHoudiniSkinInfluence& Influence : Influences.Influences)
	{
		if (Influence.Bone == nullptr)
			continue;

		const FString & BoneName = Influence.Bone->Name;
		auto Bone = NewSkeleton.BoneMap.Find(BoneName);
		if (Bone)
		{
			Influence.Bone = *Bone;
		}
		else
		{
			// we can't find the bone, try using an ancestor.
			FHoudiniSkeletonBone* Ancestor  = NewSkeleton.Root;
			FHoudiniSkeletonBone* Search = Influence.Bone->Parent;
			while(Search)
			{
				auto Candidate = NewSkeleton.BoneMap.Find(Search->Name);
				if (Candidate != nullptr)
				{
					Ancestor = *Candidate;
					break;
				}
				else
				{
					Search = Search->Parent;					
				}

			}

			if (!MissingBones.Contains(*BoneName))
			{
				MissingBones.Add(*BoneName);
				HOUDINI_LOG_WARNING(TEXT("Could not find bone in unreal skeleton %s. Using %s."), *BoneName, *Ancestor->Name);
			}
			bErrors = true;
		}
	}
	return bErrors;
}
