/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*	 this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*	 promote products derived from this software without specific prior
*	 written permission.
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

#include "HoudiniSkeletalMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniSkeletalMeshUtils.h"

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

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE



//
// Process and fill in the mesh ref skeleton bone hierarchy using the raw binary import data
// (difference from epic - Remove any FBX Importer depenedencies)
//
// @param RefSkeleton - [out] reference skeleton hierarchy to update
// @param SkeletalDepth - [out] depth of the reference skeleton hierarchy
// @param ImportData - raw binary import data to process
// @return true if the operation completed successfully
//
bool
ProcessImportMeshSkeleton(
	const USkeleton * SkeletonAsset,
	FReferenceSkeleton & OutRefSkeleton,
	int32 & OutSkeletalDepth,
	FSkeletalMeshImportData & ImportData)
{
	// Setup skeletal hierarchy + names structure.
	OutRefSkeleton.Empty();

	FReferenceSkeletonModifier RefSkelModifier(OutRefSkeleton, SkeletonAsset);

	// Digest bones to the serializable format.
	TArray<SkeletalMeshImportData::FBone>& RefBonesBinary = ImportData.RefBonesBinary;
	for (int32 b = 0; b < RefBonesBinary.Num(); b++)
	{
		const SkeletalMeshImportData::FBone& BinaryBone = RefBonesBinary[b];
		const FString BoneName = FSkeletalMeshImportData::FixupBoneName(BinaryBone.Name);
		const FMeshBoneInfo BoneInfo(FName(*BoneName, FNAME_Add), BinaryBone.Name, BinaryBone.ParentIndex);
		const FTransform BoneTransform(BinaryBone.BonePos.Transform);

		if (OutRefSkeleton.FindRawBoneIndex(BoneInfo.Name) != INDEX_NONE)
		{
			HOUDINI_LOG_MESSAGE(TEXT("SkeletonHasDuplicateBones: Skeleton has non-unique bone names.\nBone named %s encountered more than once."), *BoneName);
		}

		RefSkelModifier.Add(BoneInfo, BoneTransform);
	}

	if (RefBonesBinary.Num() != OutRefSkeleton.GetRawBoneNum())
	{
		HOUDINI_LOG_MESSAGE(TEXT("ProcessImportMeshSkeleton : RefBonesBinary Not Equal to RefSkeleton"));
	}

	// Add hierarchy index to each bone and detect max depth.
	OutSkeletalDepth = 0;
	TArray<int32> SkeletalDepths;
	SkeletalDepths.AddZeroed(OutRefSkeleton.GetRawBoneNum());
	for (int32 b = 0; b < OutRefSkeleton.GetRawBoneNum(); b++)
	{
		int32 ParentIndex = OutRefSkeleton.GetRawParentIndex(b);

		int32 Depth = 1.0f;
		SkeletalDepths[b] = 1.0f;
		if (SkeletalDepths.IsValidIndex(ParentIndex))
		{
			Depth += SkeletalDepths[ParentIndex];
		}

		if (OutSkeletalDepth < Depth)
		{
			OutSkeletalDepth = Depth;
		}
		SkeletalDepths[b] = Depth;
	}

	return true;
}



// Raw data bone.
struct FBoneTracker
{
	SkeletalMeshImportData::FBone Bone;
	int32 OrigIndex = 0;
	int32 NewIndex = 0;
};



void
AddChildren(
	TArray<FBoneTracker>&OutSortedBones,
	int Parent,
	const TArray<SkeletalMeshImportData::FBone>&RefBonesBinary)
{
	//Bone.NumChildren
	for (int32 i = 0; i < RefBonesBinary.Num(); i++)
	{
		if (RefBonesBinary[i].ParentIndex != Parent)
		{
			continue;
		}
		FBoneTracker* BoneTracker = OutSortedBones.FindByPredicate([i](FBoneTracker& BoneTracker) {
			return BoneTracker.OrigIndex == i;
			});

		if (BoneTracker != nullptr)
		{
			continue;
		}
		FBoneTracker NewBone;
		NewBone.Bone = RefBonesBinary[i];
		NewBone.OrigIndex = i;
		OutSortedBones.Add(NewBone);
		AddChildren(OutSortedBones, i, RefBonesBinary);
	}
}



//Resorts Bones By Their ParentIndex
void
SortBonesByParent(FSkeletalMeshImportData & SkeletalMeshImportData)
{
	/*
	// DEBUG ONLY - Print orginal bones
	for (int32 i = 0; i < SkeletalMeshImportData.RefBonesBinary.Num(); i++)
	{
		SkeletalMeshImportData::FBone Bone = SkeletalMeshImportData.RefBonesBinary[i];		
		UE_LOG(LogTemp, Log, TEXT("Bone %i %s parent %i number of children %i"), i, *Bone.Name, Bone.ParentIndex, Bone.NumChildren);
	}
	*/

	TArray <SkeletalMeshImportData::FBone>& RefBonesBinary = SkeletalMeshImportData.RefBonesBinary;
	TArray<FBoneTracker> SortedBones;

	//Add all with no parent
	//AddChildren(SortedBones, -1, RefBonesBinary);
	for (int32 b = 0; b < RefBonesBinary.Num(); b++)
	{
		SkeletalMeshImportData::FBone Bone = RefBonesBinary[b];
		//add all with parent self and their children
		if ((Bone.ParentIndex == b) || (Bone.ParentIndex == -1))
		{
			FBoneTracker NewBone;
			NewBone.Bone = RefBonesBinary[b];
			NewBone.Bone.ParentIndex = -1;
			NewBone.OrigIndex = b;
			SortedBones.Add(NewBone);
			AddChildren(SortedBones, b, RefBonesBinary);
		}
	}

	/*
	// DEBUG ONLY - Print sorted bones
	for (int32 i = 0; i < SortedBones.Num(); i++)
	{
		SkeletalMeshImportData::FBone Bone = SortedBones[i].Bone;
		UE_LOG(LogTemp, Log, TEXT("SORTED Bone %i %s parent %i children %i"), i, *Bone.Name, Bone.ParentIndex, Bone.NumChildren);
	}
	*/

	//store back in proper order 
	for (int32 b = 0; b < SortedBones.Num(); b++)
	{
		SortedBones[b].NewIndex = b;
		RefBonesBinary[b] = SortedBones[b].Bone;
	}

	//update Parent to new index
	for (int32 i = 0; i < SkeletalMeshImportData.RefBonesBinary.Num(); i++)
	{
		int32 OldParentIndex = SkeletalMeshImportData.RefBonesBinary[i].ParentIndex;
		//skip reparenting root
		if (OldParentIndex == -1)
			continue;
		//Lookup incorrect oldparent 
		FBoneTracker* BoneTracker = SortedBones.FindByPredicate([OldParentIndex](FBoneTracker& BoneTracker) {
			return BoneTracker.OrigIndex == OldParentIndex;
			});
		int32 NewParentIndex = BoneTracker->NewIndex;
		SkeletalMeshImportData.RefBonesBinary[i].ParentIndex = NewParentIndex;
	}

	//update influence indexes
	for (int32 i = 0; i < SkeletalMeshImportData.Influences.Num(); i++)
	{
		int32 OldIndex = SkeletalMeshImportData.Influences[i].BoneIndex;
		FBoneTracker* BoneTracker = SortedBones.FindByPredicate([OldIndex](FBoneTracker& BoneTracker) {
			return BoneTracker.OrigIndex == OldIndex;
			});
		if (BoneTracker == nullptr)
		{
			continue;
		}
		int32 NewIndex = BoneTracker->NewIndex;
		SkeletalMeshImportData.Influences[i].BoneIndex = NewIndex;

		/*
		// DEBUG ONLY - Print sorted bones
		float weight = SkeletalMeshImportData.Influences[i].Weight;
		UE_LOG(LogTemp, Log, TEXT("Old BoneIndex %i NewBoneIndex %i - %s - weight %f - parent %i"), OldIndex, NewIndex, *SkeletalMeshImportData.RefBonesBinary[NewIndex].Name,weight, SkeletalMeshImportData.RefBonesBinary[NewIndex].ParentIndex);
		*/
	}
}



//Builds Skeletal Mesh and Skeleton Assets from FSkeletalMeshImportData
void
FHoudiniSkeletalMeshTranslator::BuildSKFromImportData(SKBuildSettings& BuildSettings)
{
	FSkeletalMeshImportData& SkeletalMeshImportData = BuildSettings.SkeletalMeshImportData;
	SkeletalMeshImportData.NumTexCoords = BuildSettings.NumTexCoords;
	USkeleton* MySkeleton = BuildSettings.Skeleton;
	FBox3f BoundingBox(SkeletalMeshImportData.Points.GetData(), SkeletalMeshImportData.Points.Num());
	const FVector3f BoundingBoxSize = BoundingBox.GetSize();

	//Setup NewMesh defaults
	FSkeletalMeshModel* ImportedResource = BuildSettings.SKMesh->GetImportedModel();
	check(ImportedResource->LODModels.Num() == 0);
	ImportedResource->LODModels.Empty();
	ImportedResource->LODModels.Add(new FSkeletalMeshLODModel());
	const int32 ImportLODModelIndex = 0;
	FSkeletalMeshLODModel& NewLODModel = ImportedResource->LODModels[ImportLODModelIndex];
	if (BuildSettings.bIsNewSkeleton)
	{
		SortBonesByParent(SkeletalMeshImportData);//only sort if new skeleton
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
	BuildSettings.SKMesh->SaveLODImportedData(0, SkeletalMeshImportData);  //Import the ImportData
#endif

	int32 SkeletalDepth = 0;
	FReferenceSkeleton& RefSkeleton = BuildSettings.SKMesh->GetRefSkeleton();
	SkeletalMeshImportUtils::ProcessImportMeshSkeleton(MySkeleton, RefSkeleton, SkeletalDepth, SkeletalMeshImportData);

	for (SkeletalMeshImportData::FMaterial SkeletalImportMaterial : SkeletalMeshImportData.Materials)
	{
		UMaterialInterface* MaterialInterface;
		MaterialInterface = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(),
				nullptr, *SkeletalImportMaterial.MaterialImportName, nullptr, LOAD_NoWarn, nullptr));

		if (!IsValid(MaterialInterface))
		{
			MaterialInterface = Cast<UMaterialInterface>(SkeletalImportMaterial.Material);
		}

		FSkeletalMaterial SkeletalMaterial;
		SkeletalMaterial.MaterialInterface = MaterialInterface;
		BuildSettings.SKMesh->GetMaterials().Add(SkeletalMaterial);
	}

	// process bone influences from import data
	SkeletalMeshImportUtils::ProcessImportMeshInfluences(SkeletalMeshImportData, BuildSettings.SKMesh->GetPathName());

	BuildSettings.SKMesh->ResetLODInfo();
	FSkeletalMeshLODInfo& NewLODInfo = BuildSettings.SKMesh->AddLODInfo();
	NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
	NewLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
	NewLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
	NewLODInfo.LODHysteresis = 0.02f;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	BuildSettings.SKMesh->SaveLODImportedData(ImportLODModelIndex, SkeletalMeshImportData);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	FBoxSphereBounds3f bsb3f = FBoxSphereBounds3f(BoundingBox);
	BuildSettings.SKMesh->SetImportedBounds(FBoxSphereBounds(bsb3f));
	// Store whether or not this mesh has vertex colors
	BuildSettings.SKMesh->SetHasVertexColors(SkeletalMeshImportData.bHasVertexColors);
	//NewMesh->VertexColorGuid = Mesh->bHasVertexColors ? FGuid::NewGuid() : FGuid();

	// Pass the number of texture coordinate sets to the LODModel.	Ensure there is at least one UV coord
	NewLODModel.NumTexCoords = FMath::Max<uint32>(BuildSettings.NumTexCoords, SkeletalMeshImportData.NumTexCoords);

	//int ImportLODModelIndex = 0;
	//The imported LOD is always 0 here, the LOD custom import will import the LOD alone(in a temporary skeletalmesh) and add it to the base skeletal mesh later
	check(BuildSettings.SKMesh->GetLODInfo(ImportLODModelIndex) != nullptr);
	//Set the build options
	FSkeletalMeshBuildSettings BuildOptions;
	//Make sure the build option change in the re-import ui is reconduct
	//BuildOptions.bBuildAdjacencyBuffer = true;
	BuildOptions.bUseFullPrecisionUVs = false;
	BuildOptions.bUseBackwardsCompatibleF16TruncUVs = false;
	BuildOptions.bUseHighPrecisionTangentBasis = false;
	//BuildOptions.bRecomputeNormals = !SkeletalMeshImportData.bHasNormals;
	//BuildOptions.bRecomputeTangents = !SkeletalMeshImportData.bHasTangents;
	BuildOptions.bRecomputeNormals = true;
	BuildOptions.bRecomputeTangents = true;
	//BuildOptions.bComputeWeightedNormals = true;
	BuildOptions.bUseMikkTSpace = true;
	//BuildOptions.bRecomputeNormals = !ImportOptions->ShouldImportNormals() || !SkelMeshImportDataPtr->bHasNormals;
	//BuildOptions.bRecomputeTangents = !ImportOptions->ShouldImportTangents() || !SkelMeshImportDataPtr->bHasTangents;
	//BuildOptions.bUseMikkTSpace = (ImportOptions->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace) && (!ImportOptions->ShouldImportNormals() || !ImportOptions->ShouldImportTangents());
	//BuildOptions.bComputeWeightedNormals = ImportOptions->bComputeWeightedNormals;
	//BuildOptions.bRemoveDegenerates = ImportOptions->bRemoveDegenerates;
	//BuildOptions.ThresholdPosition = ImportOptions->OverlappingThresholds.ThresholdPosition;
	//BuildOptions.ThresholdTangentNormal = ImportOptions->OverlappingThresholds.ThresholdTangentNormal;
	//BuildOptions.ThresholdUV = ImportOptions->OverlappingThresholds.ThresholdUV;
	//BuildOptions.MorphThresholdPosition = ImportOptions->OverlappingThresholds.MorphThresholdPosition;
	BuildSettings.SKMesh->GetLODInfo(ImportLODModelIndex)->BuildSettings = BuildOptions;
	//New MeshDescription build process
	IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForRunningPlatform();
	//We must build the LODModel so we can restore properly the mesh, but we do not have to regenerate LODs

	FSkeletalMeshBuildParameters SkeletalMeshBuildParameters = FSkeletalMeshBuildParameters(BuildSettings.SKMesh, GetTargetPlatformManagerRef().GetRunningTargetPlatform(), ImportLODModelIndex, false);
	bool bBuildSuccess = MeshBuilderModule.BuildSkeletalMesh(SkeletalMeshBuildParameters);

	//We need to have a valid render data to create physic asset
	BuildSettings.SKMesh->CalculateInvRefMatrices();
	BuildSettings.SKMesh->Build();
	BuildSettings.SKMesh->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(BuildSettings.SKMesh);

	//CREATE A NEW SKELETON ASSET IF NEEDED
	if (MySkeleton == nullptr)
	{
		FString ObjectName = FString::Printf(TEXT("%s_Skeleton"), *BuildSettings.SKMesh->GetName());
		MySkeleton = NewObject<USkeleton>(BuildSettings.SKPackage, *ObjectName, RF_Public | RF_Standalone);
		MySkeleton->MarkPackageDirty();
	}
	MySkeleton->MergeAllBonesToBoneTree(BuildSettings.SKMesh);

	BuildSettings.SKMesh->SetSkeleton(MySkeleton);
	UE_LOG(LogTemp, Log, TEXT("SkeletalMeshImportData:	Materials %i Points %i Wedges %i Faces %i Influences %i"), SkeletalMeshImportData.Materials.Num(),
		SkeletalMeshImportData.Points.Num(),
		SkeletalMeshImportData.Wedges.Num(),
		SkeletalMeshImportData.Faces.Num(),
		SkeletalMeshImportData.Influences.Num());
}



//swap y and z
FVector3f
ConvertDir(FVector3f Vector)
{
	FVector3f Out;
	Out[0] = Vector[0];
	Out[1] = Vector[2];
	Out[2] = Vector[1];
	return Out;
}



void
FHoudiniSkeletalMeshTranslator::UpdateBuildSettings(SKBuildSettings & BuildSettings)
{
	const FHoudiniGeoPartObject& ShapeMeshHGPO = *BuildSettings.SKParts.HGPOShapeMesh;
	
	HAPI_NodeId GeoId = INDEX_NONE;
	HAPI_NodeId PartId = INDEX_NONE;

	bool bFoundImportScaleAttribute = false;
	
	//ImportScale----------------------------------------------------------------------------------------
	HAPI_AttributeInfo UnrealSKImportScaleInfo;
	FHoudiniApi::AttributeInfo_Init(&UnrealSKImportScaleInfo);
	HAPI_Result UnrealSKImportScaleInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_SKELETON_IMPORT_SCALE,
		HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
		&UnrealSKImportScaleInfo);

	//check result
	float UnrealSKImportScale = 100.0f;
	if (UnrealSKImportScaleInfo.exists)
	{
		TArray<float> UnrealSKImportScaleArray;
		FHoudiniHapiAccessor Accessor(GeoId, PartId, HAPI_UNREAL_ATTRIB_SKELETON_IMPORT_SCALE);
		bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, UnrealSKImportScaleArray);

		if (bSuccess && UnrealSKImportScaleArray.Num() > 0)
		{
			UnrealSKImportScale = UnrealSKImportScaleArray[0];
		}
	}
	BuildSettings.ImportScale = UnrealSKImportScale;
	
/*
	USkeleton* MySkeleton = nullptr;

	//BuildSettings.bIsNewSkeleton = !UnrealSkeletonInfo.exists;
	//
	//if ((BuildSettings.OverwriteSkeleton) && (!BuildSettings.SkeletonAssetPath.IsEmpty()))  //Panel NodeSync Settings Overrides unreal_skeleton  Attribute
	//{
	//	BuildSettings.bIsNewSkeleton = false;
	//}


	//if ((UnrealSkeletonInfo.exists == false) && (!IsValid(BuildSettings.Skeleton)))
	if (BuildSettings.bIsNewSkeleton)
	{
		//use the pre-created new asset 
		if (IsValid(BuildSettings.Skeleton))
		{
			MySkeleton = BuildSettings.Skeleton;
		}
		else
		{
			FHoudiniPackageParams SkeltonPackageParams;
			SkeltonPackageParams.GeoId = GeoId;
			SkeltonPackageParams.PartId = PartId;
			//SkeltonPackageParams.ComponentGUID = PackageParams.ComponentGUID;
			//PackageParams.ObjectName = BuildSettings.CurrentObjectName + "Skeleton";
			SkeltonPackageParams.ObjectName = BuildSettings.SKMesh->GetName() + "Skeleton";
			MySkeleton = SkeltonPackageParams.CreateObjectAndPackage<USkeleton>();

			if (!IsValid(MySkeleton))
				return nullptr;
		}

		// Free any RHI resources for existing mesh before we re-create in place.
		MySkeleton->PreEditChange(nullptr);
		
	}
	else
	{
		// Use existing skeleton asset
		MySkeleton = BuildSettings.Skeleton;
		if (!IsValid(MySkeleton))
			return nullptr;

		// Free any RHI resources for existing mesh before we re-create in place.
		MySkeleton->PreEditChange(nullptr);

		FString SkeletonAssetPathString;
		if ((BuildSettings.OverwriteSkeleton) && (!BuildSettings.SkeletonAssetPath.IsEmpty()))
		{
			// NodeSync Settings can possibly override the unreal_skeleton attribute
			SkeletonAssetPathString = BuildSettings.SkeletonAssetPath;
		}
		else
		{
			// Get the unreal_skeleton string attribute
			HAPI_AttributeInfo UnrealSkeletonInfo;
			FHoudiniApi::AttributeInfo_Init(&UnrealSkeletonInfo);

			TArray<FString> UnrealSkeletonData;
			FHoudiniEngineUtils::HapiGetAttributeDataAsString(GeoId, PartId, HAPI_UNREAL_ATTRIB_SKELETON, UnrealSkeletonInfo, UnrealSkeletonData);
			if (UnrealSkeletonData.Num() <= 0)
			{
				return nullptr;
			}

			SkeletonAssetPathString = UnrealSkeletonData[0];
		}

		const FSoftObjectPath SkeletonAssetPath(SkeletonAssetPathString);
		MySkeleton = Cast<USkeleton>(SkeletonAssetPath.TryLoad());
		if (!IsValid(MySkeleton))
		{
			return nullptr;
		}

		BuildSettings.Skeleton = MySkeleton;

		const TArray<FTransform>& RawRefBonePose = MySkeleton->GetReferenceSkeleton().GetRawRefBonePose();

		// TODO: Add additional bones from the Capture Pose.

		int32 BoneIdx = 0;
		SkeletalMeshImportData.RefBonesBinary.SetNum(MySkeleton->GetReferenceSkeleton().GetRefBoneInfo().Num());
		for (FMeshBoneInfo BoneInfo : MySkeleton->GetReferenceSkeleton().GetRefBoneInfo())
		{
			SkeletalMeshImportData::FBone Bone;
			Bone.Name = BoneInfo.Name.ToString();
			Bone.ParentIndex = BoneInfo.ParentIndex;

			SkeletalMeshImportData::FJointPos JointPos;
			JointPos.Transform = RawRefBonePose.IsValidIndex(BoneIdx) ? FTransform3f(RawRefBonePose[BoneIdx]) : FTransform3f();
			Bone.BonePos = JointPos;

			if (SkeletalMeshImportData.RefBonesBinary.IsValidIndex(BoneIdx))
				SkeletalMeshImportData.RefBonesBinary[BoneIdx] = Bone;

			BoneIdx++;
		}
	}

	

	return MySkeleton;
*/
}


bool
FHoudiniSkeletalMeshTranslator::FindAttributeOnSkeletalMeshShapeParts(const FHoudiniSkeletalMeshParts& InSKParts,
	const char* AttribName, HAPI_NodeId& OutGeoId, HAPI_PartId& OutPartId)
{
	if (InSKParts.HGPOShapeInstancer && FHoudiniEngineUtils::HapiCheckAttributeExists(InSKParts.HGPOShapeInstancer->GeoId, InSKParts.HGPOShapeInstancer->PartId, AttribName))
	{
		// Found unreal_skeleton on the Shape packed prim
		OutGeoId = InSKParts.HGPOShapeInstancer->GeoId;
		OutPartId = InSKParts.HGPOShapeInstancer->PartId;
		return true;
	}

	if (InSKParts.HGPOShapeMesh && FHoudiniEngineUtils::HapiCheckAttributeExists(InSKParts.HGPOShapeMesh->GeoId, InSKParts.HGPOShapeMesh->PartId, AttribName))
	{
		// Found unreal_skeleton inside the Shape packed prim
		OutGeoId = InSKParts.HGPOShapeMesh->GeoId;
		OutPartId = InSKParts.HGPOShapeMesh->PartId;
		return true;
	}

	return false;
}



bool
FHoudiniSkeletalMeshTranslator::FillSkeletalMeshImportData(SKBuildSettings& BuildSettings, const FHoudiniPackageParams& InPackageParams)
{
	const FHoudiniGeoPartObject& ShapeMeshHGPO = *BuildSettings.SKParts.HGPOShapeMesh;
	const FHoudiniGeoPartObject& PoseMeshHGPO = *BuildSettings.SKParts.HGPOPoseMesh;
	
	HAPI_NodeId ShapeGeoId = ShapeMeshHGPO.GeoId;
	HAPI_NodeId ShapePartId = ShapeMeshHGPO.PartId;

	FSkeletalMeshImportData& SkeletalMeshImportData = BuildSettings.SkeletalMeshImportData;

	//-----------------------------------------------------------------------------------
	// Shape Infos
	//-----------------------------------------------------------------------------------

	HAPI_PartInfo ShapeMeshPartInfo;
	FHoudiniApi::PartInfo_Init(&ShapeMeshPartInfo);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), ShapeGeoId, ShapePartId, &ShapeMeshPartInfo);

	//-----------------------------------------------------------------------------------
	// Rest Geometry Points
	//-----------------------------------------------------------------------------------

	TArray<FVector3f> PositionData;
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_POSITION);
	bool bSuccess = Accessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, PositionData);

	// we don't need to multiply by tupleSize, it's already a vector container
	SkeletalMeshImportData.Points.SetNum(PositionData.Num());
	int32 Index = 0;
	for (FVector3f Point : PositionData)
	{
		//flip x and z
		SkeletalMeshImportData.Points[Index] = FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector3f(Point);
		SkeletalMeshImportData.PointToRawMap.Add(Index);
		Index++;
	}

	//-----------------------------------------------------------------------------------
	// UVs
	//-----------------------------------------------------------------------------------
	
	TArray<TArray<float>> PartUVSets;
	TArray<HAPI_AttributeInfo> AttribInfoUVSets;
	FHoudiniEngineUtils::UpdateMeshPartUVSets(ShapeGeoId, ShapePartId, true, PartUVSets, AttribInfoUVSets);

	//-----------------------------------------------------------------------------------
	// Normals
	//-----------------------------------------------------------------------------------

	TArray<FVector3f> NormalData;
	bool bUseComputedNormals = !BuildSettings.ImportNormals;

	if (!bUseComputedNormals)
	{
		Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_NORMAL);
		bSuccess = Accessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, NormalData);
		if (!bSuccess || NormalData.IsEmpty())
			bUseComputedNormals = true;
	}

	//-----------------------------------------------------------------------------------
	// Vertex Colors
	//-----------------------------------------------------------------------------------

	HAPI_AttributeInfo ColorInfo;
	TArray<float> ColorData;

	Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_COLOR);
	Accessor.GetInfo(ColorInfo, HAPI_ATTROWNER_INVALID);
	bool bColorInfoExists = Accessor.GetAttributeData(ColorInfo, ColorData);

	//-----------------------------------------------------------------------------------
	// Tangents
	//-----------------------------------------------------------------------------------

	TArray<float> TangentData;
	Accessor.Init(ShapeGeoId,ShapePartId, HAPI_UNREAL_ATTRIB_TANGENTU);
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, TangentData);

	//-----------------------------------------------------------------------------------
	// Materials
	//-----------------------------------------------------------------------------------
	TArray<int32> PerFaceUEMaterialIds;
	TArray<UMaterialInterface*> UniqueMaterials;
	if (!FHoudiniSkeletalMeshTranslator::CreateSkeletalMeshMaterials(
		ShapeMeshHGPO,
		ShapeMeshPartInfo,
		InPackageParams,
		PerFaceUEMaterialIds,
		SkeletalMeshImportData))
	{
		// Unable to retrieve materials, should we use default Houdini one?
		HOUDINI_LOG_ERROR(TEXT("Creating Skeletal Mesh : unable to load/create materials"));
	}

	//-----------------------------------------------------------------------------------
	// Indices
	//-----------------------------------------------------------------------------------
	HAPI_AttributeInfo VertexInfo;
	FHoudiniApi::AttributeInfo_Init(&VertexInfo);

	HAPI_Result VertexInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		ShapeGeoId,
		ShapePartId,
		"__vertex_id",
		HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX,
		&VertexInfo);

	if (!VertexInfo.exists || VertexInfo.count <= 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error Creating Skeletal Mesh :  No Vertex Info"));
		return false;
	}

	TArray<int> VertexData;
	VertexData.SetNum(VertexInfo.count);
	HAPI_Result VertexDataResult = FHoudiniApi::GetVertexList(
		FHoudiniEngine::Get().GetSession(),
		ShapeGeoId,
		ShapePartId,
		&VertexData[0],
		0,
		VertexInfo.count);

	//-----------------------------------------------------------------------------------
	// LoadInWedgeData
	// FACES AND WEDGES
	//-----------------------------------------------------------------------------------
	int32 face_id = 0;
	int32 face_idx = 0;
	int32 count = 0;

	{
		int NumTexCoords = 0;
		for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
		{
			if (!AttribInfoUVSets.IsValidIndex(TexCoordIndex))
				continue;
			if (!AttribInfoUVSets[TexCoordIndex].exists)
				continue;
			++NumTexCoords;
		}
		BuildSettings.NumTexCoords = NumTexCoords;
	}

	SkeletalMeshImportData::FTriangle Triangle;
	for (int VertexInstanceIndex = 0; VertexInstanceIndex < VertexData.Num(); ++VertexInstanceIndex)
	{
		int VertexIndex = VertexData[VertexInstanceIndex];
		SkeletalMeshImportData::FVertex Wedge;
		Wedge.VertexIndex = VertexIndex;
		
		for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
		{
			if (!AttribInfoUVSets.IsValidIndex(TexCoordIndex))
				continue;
			if (!AttribInfoUVSets[TexCoordIndex].exists)
				continue;

			int UVIndex = 0;
			switch (AttribInfoUVSets[TexCoordIndex].owner)
			{
				case HAPI_ATTROWNER_VERTEX:
					UVIndex = VertexInstanceIndex;
					break;
				case HAPI_ATTROWNER_POINT:
					UVIndex = VertexIndex;
					break;
				default:
					// We don't support UV attributes on anything other than (houdini) points or verts.
					break;
			}
			
			const int UVTupleSize = AttribInfoUVSets[TexCoordIndex].tupleSize;
			TArray<float>& UVData = PartUVSets[TexCoordIndex];
			// ERROR: This keeps going out of bounds. Why are we getting point UVs ? Should be vertex?!
			Wedge.UVs[TexCoordIndex] = FVector2f(UVData[UVIndex * UVTupleSize], 1.0f - UVData[UVIndex * UVTupleSize + 1]);

			if (bColorInfoExists)
			{
				
				int ColorIndex = (ColorInfo.owner == HAPI_ATTROWNER_VERTEX ? VertexInstanceIndex : VertexIndex) * ColorInfo.tupleSize;
				Wedge.Color = FLinearColor( ColorData[ColorIndex], ColorData[ColorIndex+1], ColorData[ColorIndex+2] ).ToFColor(false);
			}
		}
		
		//Wedge.MatIndex = 
		SkeletalMeshImportData.Wedges.Add(Wedge);
		Triangle.WedgeIndex[face_idx] = count;
		Triangle.SmoothingGroups = 255;
		Triangle.MatIndex = PerFaceUEMaterialIds.IsEmpty() ? 0 : PerFaceUEMaterialIds[face_id];

		// Store normal for each vertex of face
		FVector3f ConvertedNormal;
		if (bUseComputedNormals)
		{
			ConvertedNormal = FVector3f::ZeroVector;
		}
		else
		{
			FVector3f n = NormalData[count];
			ConvertedNormal = ConvertDir(n);
			ConvertedNormal.Normalize();
		}
		Triangle.TangentZ[face_idx] = ConvertedNormal;

		// Compute tangent/binormal from the normal?
		FVector3f TangentX, TangentY;
		Triangle.TangentZ[face_idx].FindBestAxisVectors(TangentX, TangentY);

		count++;
		face_idx++;

		// We're starting the next triangle so store the old one
		if ((count % 3) == 0)
		{
			SkeletalMeshImportData::FVertex Wedge1 = SkeletalMeshImportData.Wedges[count - 3];
			SkeletalMeshImportData::FVertex Wedge2 = SkeletalMeshImportData.Wedges[count - 2];
			SkeletalMeshImportData::FVertex Wedge3 = SkeletalMeshImportData.Wedges[count - 1];

			SkeletalMeshImportData.Wedges[count - 3] = Wedge3;
			SkeletalMeshImportData.Wedges[count - 1] = Wedge1;

			//tangent winding
			FVector3f Tangent0 = Triangle.TangentZ[0];
			FVector3f Tangent1 = Triangle.TangentZ[1];
			FVector3f Tangent2 = Triangle.TangentZ[2];

			Triangle.TangentZ[0] = Tangent2;
			Triangle.TangentZ[2] = Tangent0;

			SkeletalMeshImportData.Faces.Add(Triangle);

			face_id++;
			face_idx = 0;
		}
	}

	//------------------------------------------------------------------------------------------------------------------------------
	// Fetch Skeleton from Houdini and convert to Unreal.
	//------------------------------------------------------------------------------------------------------------------------------

	FHoudiniSkeleton Skeleton = FHoudiniSkeletalMeshUtils::ConstructSkeleton(PoseMeshHGPO.GeoId, PoseMeshHGPO.PartId);
	if (Skeleton.Bones.IsEmpty())
		return false;

	SkeletalMeshImportData.RefBonesBinary.SetNum(Skeleton.Bones.Num());
	int32 BoneIndex = 0;
	for (int JointIndex = 0; JointIndex <  Skeleton.Bones.Num(); JointIndex++)
	{
		FHoudiniSkeletonBone & Joint = Skeleton.Bones[JointIndex];
		SkeletalMeshImportData::FBone NewBone;
		NewBone.Name = Joint.Name;
		NewBone.Flags = 0;
		NewBone.ParentIndex = Joint.Parent ? Joint.Parent->Id : - 1; 
		NewBone.NumChildren = Joint.Children.Num();

		SkeletalMeshImportData::FJointPos JointPos;
		JointPos.Transform = FTransform3f(FTransform(Joint.UnrealLocalMatrix));
		
		NewBone.BonePos = JointPos;

		if (SkeletalMeshImportData.RefBonesBinary.IsValidIndex(BoneIndex))
			SkeletalMeshImportData.RefBonesBinary[BoneIndex] = NewBone;

		BoneIndex++;
	}

	//------------------------------------------------------------------------------------------------------------------------------
	// Fetch skinning data from Houdini (aka "Bone Capture") and convert to Unreal
	//------------------------------------------------------------------------------------------------------------------------------

	FHoudiniSkinWeights SkinWeights = FHoudiniSkeletalMeshUtils::ConstructSkinWeights(ShapeGeoId, ShapePartId, Skeleton);

	for (int32 PointIndex = 0; PointIndex < ShapeMeshPartInfo.pointCount; PointIndex++)
	{
		for(int Influence = 0; Influence < SkinWeights.NumInfluences; Influence++)
		{
			FHoudiniSkinInfluence& SkinInfluence = SkinWeights.Influences[PointIndex * SkinWeights.NumInfluences + Influence];

			SkeletalMeshImportData::FRawBoneInfluence UnrealInfluence;
			UnrealInfluence.VertexIndex = PointIndex;
			UnrealInfluence.BoneIndex = SkinInfluence.Bone ? SkinInfluence.Bone->Id : 0;
			UnrealInfluence.Weight = SkinInfluence.Weight;
			SkeletalMeshImportData.Influences.Add(UnrealInfluence);
		}
	}

	SkeletalMeshImportData.bHasVertexColors = ColorInfo.exists;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
	SkeletalMeshImportData.bDiffPose = false;
	SkeletalMeshImportData.bUseT0AsRefPose = false;
#endif

	SkeletalMeshImportData.bHasNormals = true;
	SkeletalMeshImportData.bHasTangents = false;

	return true;
}



//Creates SkelatalMesh and Skeleton Assets and Packages, and adds them to OutputObjects
bool FHoudiniSkeletalMeshTranslator::CreateSkeletalMesh_SkeletalMeshImportData()
{
	const FHoudiniGeoPartObject& MainHGPO = *SKParts.GetMainHGPO();
	const FHoudiniGeoPartObject& ShapeMeshHGPO = *SKParts.HGPOShapeMesh;
	
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier(
		MainHGPO.ObjectId, MainHGPO.GeoId, MainHGPO.PartId, "");
	OutputObjectIdentifier.PartName = MainHGPO.PartName;
	// Hard-coded point and prim indices to 0 and 0
	OutputObjectIdentifier.PointIndex = 0;
	OutputObjectIdentifier.PrimitiveIndex = 0;

	// If we don't already have an object for OutputObjectIdentifier in OutputObjects, then check in InputObjects and
	// copy it from there. Otherwise create a new empty OutputObject in OutputObjects.
	if (!OutputObjects.Contains(OutputObjectIdentifier))
	{
		FHoudiniOutputObject const* const InputObject = InputObjects.Find(OutputObjectIdentifier);
		if (InputObject)
			OutputObjects.Emplace(OutputObjectIdentifier, *InputObject);
	}
	FHoudiniOutputObject& OutputObject = OutputObjects.FindOrAdd(OutputObjectIdentifier);

	// Get non-generic supported attributes from OutputObjectIdentifier
	OutputObject.CachedAttributes.Empty();
	OutputObject.CachedTokens.Empty();
	FHoudiniMeshTranslator::CopyAttributesFromHGPOForSplit(
		ShapeMeshHGPO, OutputObjectIdentifier.PointIndex, OutputObjectIdentifier.PrimitiveIndex, OutputObject.CachedAttributes, OutputObject.CachedTokens);

	// Resolve our temp package params
	const FHoudiniPackageParams InitialPackageParams = PackageParams;
	FHoudiniAttributeResolver Resolver;
	FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
		InitialPackageParams,
		IsValid(OuterComponent) ? OuterComponent->GetWorld() : nullptr,
		OuterComponent,
		OutputObject.CachedAttributes,
		OutputObject.CachedTokens,
		PackageParams,
		Resolver);

	//-----------------------------------------------------------------------------------
	// unreal_skeleton
	//-----------------------------------------------------------------------------------
	
	USkeleton* SkeletonAsset = nullptr;
	{
		// Look for unreal_skeleton attribute on the Shape packed prim (instancer) level, then
		// on the mesh HGPO level.
		
		bool bFoundUnrealSkeletonPath = false;
		int SkeletonPathGeoId = INDEX_NONE;
		int SkeletonPathPartId = INDEX_NONE;

		bFoundUnrealSkeletonPath = FindAttributeOnSkeletalMeshShapeParts(SKParts, HAPI_UNREAL_ATTRIB_SKELETON, SkeletonPathGeoId, SkeletonPathPartId);

		if (bFoundUnrealSkeletonPath)
		{
			TArray<FString> StringData;

			FHoudiniHapiAccessor Accessor(SkeletonPathGeoId, SkeletonPathPartId, HAPI_UNREAL_ATTRIB_SKELETON);
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, StringData);

			if (StringData.Num() == 1)
			{
				const FString UnrealSkeletonPath = StringData[0];
				SkeletonAsset = LoadObject<USkeleton>(nullptr, *UnrealSkeletonPath);
				// If the unreal_skeleton path was valid, UnrealSkeleton would now point to our desired skeleton asset.
				if (SkeletonAsset)
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(SkeletonAsset);
				}
				else
				{
					HOUDINI_LOG_WARNING(TEXT("Could not find Skeleton asset at path '%s'. A new temp skeleton will be created."), *UnrealSkeletonPath);
				}
			}
		}
	}
	
	// Create packages for the skeleton and skeletal mesh.
	
	bool bIsNewSkeleton = SkeletonAsset == nullptr;

	// If we don't have a skeleton asset yet, create one now.
	if (!SkeletonAsset)
	{
		SkeletonAsset = CreateNewSkeleton(OutputObjectIdentifier.SplitIdentifier);
		if (!SkeletonAsset)
		{
			return false;
		}
		// Notify the asset registry of new asset
		FAssetRegistryModule::AssetCreated(SkeletonAsset);

		const FHoudiniGeoPartObject& PoseInstancerHGPO = *SKParts.HGPOPoseInstancer;

		// Create the output object
		FHoudiniOutputObjectIdentifier SkeletonOutputObjectIdentifier(
			PoseInstancerHGPO.ObjectId, PoseInstancerHGPO.GeoId, PoseInstancerHGPO.PartId, "");
		SkeletonOutputObjectIdentifier.PartName = MainHGPO.PartName;
		// Hard-coded point and prim indices to 0 and 0
		SkeletonOutputObjectIdentifier.PointIndex = 0;
		SkeletonOutputObjectIdentifier.PrimitiveIndex = 0;

		// If we don't already have an object for SkeletonOutputObjectIdentifier in OutputObjects, then check in InputObjects and
		// copy it from there. Otherwise create a new empty OutputObject in OutputObjects.
		if (!OutputObjects.Contains(SkeletonOutputObjectIdentifier))
		{
			FHoudiniOutputObject const* const InputObject = InputObjects.Find(SkeletonOutputObjectIdentifier);
			if (InputObject)
				OutputObjects.Emplace(SkeletonOutputObjectIdentifier, *InputObject);
		}
		FHoudiniOutputObject& SkeletonOutputObject = OutputObjects.FindOrAdd(SkeletonOutputObjectIdentifier);

		SkeletonOutputObject.OutputObject = SkeletonAsset;
		SkeletonOutputObject.bProxyIsCurrent = false;
	}
	
	USkeletalMesh* SkeletalMeshAsset = CreateNewSkeletalMesh(OutputObjectIdentifier.SplitIdentifier);
	OutputObject.OutputObject = SkeletalMeshAsset;
	OutputObject.bProxyIsCurrent = false;

	// This ensures that the render data gets built before we return, by calling PostEditChange when we fall out of scope.
	FScopedSkeletalMeshPostEditChange ScopedPostEditChange( SkeletalMeshAsset );
	
	if (SkeletalMeshAsset)
	{
		SkeletalMeshAsset->PreEditChange( nullptr );
		// Notify the asset registry of new asset
		FAssetRegistryModule::AssetCreated(SkeletalMeshAsset);
	}

	//FHoudiniOutputObject* FoundOutputObject;
	//FHoudiniOutputObject NewOutputObject;
	//FoundOutputObject = &OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
	//USkeletalMesh* NewSkeletalMesh = CreateNewSkeletalMesh(OutputObjectIdentifier.SplitIdentifier);

	//if (FoundOutputObject)
	//{
	//	FoundOutputObject->OutputObject = NewSkeletalMesh;
	//	FoundOutputObject->bProxyIsCurrent = false;
	//	OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
	//}

	SKBuildSettings skBuildSettings;
	skBuildSettings.SKParts = SKParts;
	skBuildSettings.ImportNormals = true;
	skBuildSettings.SKMesh = SkeletalMeshAsset;
	skBuildSettings.bIsNewSkeleton = bIsNewSkeleton;
	skBuildSettings.Skeleton = SkeletonAsset;
	
	FHoudiniSkeletalMeshTranslator::UpdateBuildSettings(skBuildSettings);

	const bool bResult = FillSkeletalMeshImportData(skBuildSettings, PackageParams);
	if (!bResult)
	{
		return false;
	}

	FHoudiniSkeletalMeshTranslator::BuildSKFromImportData(skBuildSettings);

	return true;
}



USkeleton*
FHoudiniSkeletalMeshTranslator::CreateNewSkeleton(const FString& InSplitIdentifier) const
{
	FHoudiniPackageParams SkeletonPackageParams = PackageParams;
	
	SkeletonPackageParams.SplitStr = InSplitIdentifier;
	if (SkeletonPackageParams.ObjectName.IsEmpty())
		SkeletonPackageParams.ObjectName = FString::Printf(TEXT("%s_%d_%d_%d_%sSkeleton"), *PackageParams.HoudiniAssetName, PackageParams.ObjectId, PackageParams.GeoId, PackageParams.PartId, *PackageParams.SplitStr);
	else
		SkeletonPackageParams.ObjectName += TEXT("Skeleton");

	const FString AssetPath = SkeletonPackageParams.GetPackagePath();
	const FString PackageName = SkeletonPackageParams.GetPackageName();

	const FString PackagePath = FPaths::Combine(AssetPath, PackageName);
	const FSoftObjectPath SkeletonAssetPath(PackagePath);
	
	if (USkeleton* ExistingSkeleton = LoadObject<USkeleton>(nullptr, *PackagePath, nullptr, LOAD_NoWarn) )
	{
		ExistingSkeleton->PreEditChange( nullptr );
	}

	USkeleton* NewSkeleton = SkeletonPackageParams.CreateObjectAndPackage<USkeleton>();
	if (!IsValid(NewSkeleton))
		return nullptr;
	
	return NewSkeleton;
}



USkeletalMesh*
FHoudiniSkeletalMeshTranslator::CreateNewSkeletalMesh(const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	FHoudiniGeoPartObject MainHGPO = *SKParts.GetMainHGPO();
	PackageParams.ObjectId = MainHGPO.ObjectId;
	PackageParams.GeoId = MainHGPO.GeoId;
	PackageParams.PartId = MainHGPO.PartId;
	PackageParams.SplitStr = InSplitIdentifier;

	USkeletalMesh* NewSkeletalMesh = PackageParams.CreateObjectAndPackage<USkeletalMesh>();
	if (!IsValid(NewSkeletalMesh))
		return nullptr;

	return NewSkeletalMesh;
}



void
FHoudiniSkeletalMeshTranslator::SetPackageParams(const FHoudiniPackageParams& InPackageParams, const bool& bUpdateHGPO)
{
	PackageParams = InPackageParams;

	if (bUpdateHGPO)
	{
		FHoudiniGeoPartObject MainHGPO = *SKParts.GetMainHGPO();
		PackageParams.ObjectId = MainHGPO.ObjectId;
		PackageParams.GeoId = MainHGPO.GeoId;
		PackageParams.PartId = MainHGPO.PartId;
	}
}



bool
FHoudiniSkeletalMeshTranslator::IsRestGeometryInstancer(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, FString& OutBaseName)
{
	// Rest Geometry packed prim name must end with '.shp'
	TArray<FString> NameData;

	FHoudiniHapiAccessor Accessor(GeoId, PartId, "name");
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, NameData);

	if (!bSuccess || NameData.Num() == 0)
	{
		return false;
	}
	if (!NameData[0].EndsWith(".shp"))
	{
		return false;
	}

	// Extract the base name that we can use to identify this capture pose and pair it with its respective rest geometry.
	FString Path, Filename, Extension;
	FPaths::Split(NameData[0], Path, OutBaseName, Extension );
	
	// Check for attributes inside this packed prim:
	// point attributes: boneCapture
	
	// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
	const int NumInstancedParts = 1;
	TArray<HAPI_PartId> InstancedPartIds;
	InstancedPartIds.SetNumZeroed(NumInstancedParts);
	if ( FHoudiniApi::GetInstancedPartIds(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			InstancedPartIds.GetData(),
			0, NumInstancedParts ) != HAPI_RESULT_SUCCESS )
	{
		return false;
	}

	const HAPI_PartId InstancedPartId = InstancedPartIds[0];

	if (!IsRestGeometryMesh(GeoId, InstancedPartId))
	{
		return false;
	}

	return true;
}



bool
FHoudiniSkeletalMeshTranslator::IsRestGeometryMesh(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId)
{
	if (!GetAttrInfo(GeoId, PartId, "boneCapture", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}
	
	return true;
}



bool
FHoudiniSkeletalMeshTranslator::IsCapturePoseInstancer(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, FString& OutBaseName)
{
	auto GetAttrInfo = [](const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, const char* AttrName, HAPI_AttributeOwner AttrOwner) -> HAPI_AttributeInfo
	{
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);
		HAPI_Result AttrInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			AttrName, AttrOwner, &AttrInfo);
		return AttrInfo;  
	};


	// Capture Pose packed prim name must end with '.skel'
	TArray<FString> NameData;

	FHoudiniHapiAccessor Accessor(GeoId, PartId, "name");
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, NameData);

	if (!bSuccess || NameData.Num() == 0)
	{
		return false;
	}
	if (!NameData[0].EndsWith(".skel"))
	{
		return false;
	}

	// Extract the base name that we can use to identify this capture pose and pair it with its respective rest geometry.
	FString Path, Filename, Extension;
	FPaths::Split(NameData[0], Path, OutBaseName, Extension );
	
	// Check for attributes inside this packed prim:
	// point attributes: transform, name
	
	// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
	const int NumInstancedParts = 1;
	TArray<HAPI_PartId> InstancedPartIds;
	InstancedPartIds.SetNumZeroed(NumInstancedParts);
	if ( FHoudiniApi::GetInstancedPartIds(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			InstancedPartIds.GetData(),
			0, NumInstancedParts ) != HAPI_RESULT_SUCCESS )
	{
		return false;
	}

	const HAPI_PartId InstancedPartId = InstancedPartIds[0];

	if (!IsCapturePoseMesh(GeoId, InstancedPartId))
	{
		return false;
	}

	return true;
}



bool
FHoudiniSkeletalMeshTranslator::IsCapturePoseMesh(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId)
{
	if (!GetAttrInfo(GeoId, PartId, "transform", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}

	if (!GetAttrInfo(GeoId, PartId, "name", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}
	
	return true;
}



HAPI_AttributeInfo
FHoudiniSkeletalMeshTranslator::GetAttrInfo(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId,
	const char* AttrName, HAPI_AttributeOwner AttrOwner)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	HAPI_Result AttrInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId,
		AttrName, AttrOwner, &AttrInfo);

	return AttrInfo;
}



bool
FHoudiniSkeletalMeshTranslator::CreateAllSkeletalMeshesAndComponentsFromHoudiniOutput(
	UHoudiniOutput* InOutput,
	const FHoudiniPackageParams& InPackageParams,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
	UObject* InOuterComponent)
{
	if (!IsValid(InOutput))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OldOutputObjects = InOutput->GetOutputObjects();
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignementMaterials = InOutput->GetAssignementMaterials();
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterials = InOutput->GetReplacementMaterials();

	bool InForceRebuild = false;

	FHoudiniSkeletalMeshParts SKParts;

	// Find all the correct parts that we need
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->HoudiniGeoPartObjects)
	{
		if (CurHGPO.Type == EHoudiniPartType::SkeletalMeshShape)
		{
			if (CurHGPO.bIsInstanced)
				SKParts.HGPOShapeMesh = &CurHGPO;
			else
				SKParts.HGPOShapeInstancer = &CurHGPO;
		}
		else if (CurHGPO.Type == EHoudiniPartType::SkeletalMeshPose)
		{
			if (CurHGPO.bIsInstanced)
				SKParts.HGPOPoseMesh = &CurHGPO;
			else
				SKParts.HGPOPoseInstancer = &CurHGPO;
		}
	}


	if (!(SKParts.IsValid()))
	{
		HOUDINI_LOG_ERROR(TEXT("Missing parts of skeletal mesh. Could not process output"));
		return false;
	}

	// Iterate on all of the output's HGPO, creating meshes as we go
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->HoudiniGeoPartObjects)
	{
		// Not a skeletal mesh geo, skip
		if (!(CurHGPO.Type == EHoudiniPartType::SkeletalMeshShape && CurHGPO.bIsInstanced == false))
			continue;

		// See if we have some uproperty attributes to update on 
		// the outer component (in most case, the HAC)
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			CurHGPO.GeoId, CurHGPO.PartId,
			true, 0, 0, 0,
			PropertyAttributes))
		{
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
				InOuterComponent, PropertyAttributes);
		}
	}

	if (!CreateSkeletalMeshFromHoudiniGeoPartObject(
		SKParts,
		InPackageParams,
		InOuterComponent,
		OldOutputObjects,
		NewOutputObjects,
		AssignementMaterials,
		ReplacementMaterials,
		InAllOutputMaterials))
	{
		return false;
	}

	for (auto& CurMat : AssignementMaterials)
	{
		// Adds the newly generated materials to the output materials array
		// This is to avoid recreating those same materials again
		if (!InAllOutputMaterials.Contains(CurMat.Key))
			InAllOutputMaterials.Add(CurMat);
	}

	return FHoudiniMeshTranslator::CreateOrUpdateAllComponents(
		InOutput,
		InOuterComponent,
		NewOutputObjects);
}



bool
FHoudiniSkeletalMeshTranslator::CreateSkeletalMeshFromHoudiniGeoPartObject(
	const FHoudiniSkeletalMeshParts& SKParts,
	const FHoudiniPackageParams& InPackageParams,
	UObject* InOuterComponent,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignmentMaterialMap,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterialMap,
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials)
{
	// Make sure this is indeed a Skeletal Mesh
	if (!FHoudiniSkeletalMeshTranslator::IsRestGeometryMesh(SKParts.HGPOShapeMesh->GeoId, SKParts.HGPOShapeMesh->PartId))
		return false;

	FHoudiniSkeletalMeshTranslator SKMeshTranslator;
	SKMeshTranslator.SetHoudiniSkeletalMeshParts(SKParts);
	SKMeshTranslator.SetInputObjects(InOutputObjects);
	SKMeshTranslator.SetOutputObjects(OutOutputObjects);
	SKMeshTranslator.SetPackageParams(InPackageParams, true);
	SKMeshTranslator.SetOuterComponent(InOuterComponent);
	SKMeshTranslator.SetInputAssignmentMaterials(AssignmentMaterialMap);
	SKMeshTranslator.SetReplacementMaterials(AssignmentMaterialMap);
	SKMeshTranslator.SetAllOutputMaterials(InAllOutputMaterials);
	if (SKMeshTranslator.CreateSkeletalMesh_SkeletalMeshImportData())
	{
		// Copy the output objects/materials
		OutOutputObjects = SKMeshTranslator.OutputObjects;
		AssignmentMaterialMap = SKMeshTranslator.OutputAssignmentMaterials;

		return true;
	}
	
	return false;
}



bool
FHoudiniSkeletalMeshTranslator::CreateSkeletalMeshMaterials(
	const FHoudiniGeoPartObject& InShapeMeshHGPO,
	const HAPI_PartInfo& InShapeMeshPartInfo,
	const FHoudiniPackageParams& InPackageParams,
	TArray<int32>& OutPerFaceUEMaterialIds,
	FSkeletalMeshImportData& OutImportData)
{
	// Get material information from unreal_material.
	FHoudiniSkeletalMeshMaterialSettings MeshSettings = FHoudiniSkeletalMeshUtils::GetMaterialOverrides(InShapeMeshHGPO.GeoId, InShapeMeshHGPO.PartId);

	// If no unreal material, try to use houdini materials.
	if (MeshSettings.Materials.IsEmpty())
	{
		MeshSettings = FHoudiniSkeletalMeshUtils::GetHoudiniMaterials(InShapeMeshHGPO.GeoId, InShapeMeshHGPO.PartId, InShapeMeshPartInfo.faceCount);

		if (!MeshSettings.Materials.IsEmpty())
		{
			bool bCreatedMaterials = FHoudiniSkeletalMeshUtils::CreateHoudiniMaterial(MeshSettings,
				InputAssignmentMaterials, OutputAssignmentMaterials, AllOutputMaterials, InPackageParams);
			if (!bCreatedMaterials)
				return false;
		}
	}

	// If there are no materials, create one empty one, which will be assigned the default material.
	if (MeshSettings.Materials.IsEmpty())
	{
		MeshSettings.MaterialIds.SetNumZeroed(InShapeMeshPartInfo.faceCount);
		MeshSettings.Materials.SetNum(1);

	}

	for (auto& Material : MeshSettings.Materials)
	{
		SkeletalMeshImportData::FMaterial SKMIDMaterial;

		FString MaterialAssetPath = Material.AssetPath;
		SKMIDMaterial.Material = nullptr;
		if (!MaterialAssetPath.IsEmpty())
			SKMIDMaterial.Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialAssetPath, nullptr, LOAD_NoWarn, nullptr));

		if (!IsValid(SKMIDMaterial.Material.Get()))
			SKMIDMaterial.Material = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

		SKMIDMaterial.MaterialImportName = MaterialAssetPath;
		OutImportData.Materials.Add(SKMIDMaterial);
	}
	OutPerFaceUEMaterialIds = MoveTemp(MeshSettings.MaterialIds);

	return true;
}

#undef LOCTEXT_NAMESPACE