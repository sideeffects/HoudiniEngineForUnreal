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

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "IMeshBuilderModule.h"
#include "ImportUtils/SkeletalMeshImportUtils.h"
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
	for (int32 i = 0; i < SkeletalMeshImportData.RefBonesBinary.Num(); i++)
	{
		SkeletalMeshImportData::FBone Bone = SkeletalMeshImportData.RefBonesBinary[i];
		UE_LOG(LogTemp, Log, TEXT("Bone %i %s parent %i children %i"), i, *Bone.Name, Bone.ParentIndex, Bone.NumChildren);
	}

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

	for (int32 i = 0; i < SortedBones.Num(); i++)
	{
		SkeletalMeshImportData::FBone Bone = SortedBones[i].Bone;
		UE_LOG(LogTemp, Log, TEXT("SORTED Bone %i %s parent %i children %i"), i, *Bone.Name, Bone.ParentIndex, Bone.NumChildren);
	}

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
		float weight = SkeletalMeshImportData.Influences[i].Weight;
		//UE_LOG(LogTemp, Log, TEXT("Old BoneIndex %i NewBoneIndex %i %s %f %i"), OldIndex, NewIndex, *SkeletalMeshImportData.RefBonesBinary[NewIndex].Name,weight, SkeletalMeshImportData.RefBonesBinary[NewIndex].ParentIndex);
	}
}

//Builds Skeletal Mesh and Skeleton Assets from FSkeletalMeshImportData
void
FHoudiniSkeletalMeshTranslator::BuildSKFromImportData(
	SKBuildSettings & BuildSettings, TArray<FSkeletalMaterial>&Materials)
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

	BuildSettings.SKMesh->SaveLODImportedData(0, SkeletalMeshImportData);  //Import the ImportData

	int32 SkeletalDepth = 0;
	FReferenceSkeleton& RefSkeleton = BuildSettings.SKMesh->GetRefSkeleton();
	SkeletalMeshImportUtils::ProcessImportMeshSkeleton(MySkeleton, RefSkeleton, SkeletalDepth, SkeletalMeshImportData);

	for (SkeletalMeshImportData::FMaterial SkeletalImportMaterial : SkeletalMeshImportData.Materials)
	{
		UMaterialInterface* MaterialInterface;
		MaterialInterface = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(),
				nullptr, *SkeletalImportMaterial.MaterialImportName, nullptr, LOAD_NoWarn, nullptr));
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
	BuildSettings.SKMesh->Build();
	BuildSettings.SKMesh->CalculateInvRefMatrices();
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

bool
FHoudiniSkeletalMeshTranslator::HasSkeletalMeshData(const HAPI_NodeId & GeoId, const HAPI_NodeId & PartId)
{
	HAPI_AttributeInfo CaptNamesInfo;
	FHoudiniApi::AttributeInfo_Init(&CaptNamesInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		"capt_names",
		HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
		&CaptNamesInfo), false);

	return CaptNamesInfo.exists;
}

/*
//Populates FSkeletalMeshImportData from HAPI
void
FHoudiniSkeletalMeshTranslator::LoadImportData(const HAPI_NodeId & GeoId, const HAPI_NodeId & PartId)
{
	HOUDINI_LOG_MESSAGE(TEXT("LoadImportData"));

	TArray<FString> OutputNames;
	FHoudiniEngineUtils::GetOutputNameAttribute(GeoId, PartId, OutputNames, 0, 1);

	TArray<FString> AllBakeFolders;
	FHoudiniEngineUtils::GetBakeFolderAttribute(GeoId, AllBakeFolders, PartId, 0, 1);

	//----------------------------------------------------------------------------------------
	// PackageName
	//----------------------------------------------------------------------------------------	
	// HAPI_AttributeInfo UnrealSKPackageInfo;
	// FHoudiniApi::AttributeInfo_Init(&UnrealSKPackageInfo);
	// HAPI_Result UnrealSKPackageInfoResult = FHoudiniApi::GetAttributeInfo(
	//		FHoudiniEngine::Get().GetSession(),
	//		GeoId, PartId,
	//		"unreal_sk_package_path", HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, &UnrealSKPackageInfo);

	// if (UnrealSKPackageInfo.exists == false)
	// {
	//		return;  //no package path set
	// }
	// TArray<FString> UnrealSkPackageData;
	// FHoudiniEngineUtils::HapiGetAttributeDataAsString(GeoId, PartId, "unreal_sk_package_path", UnrealSKPackageInfo, UnrealSkPackageData);

	if (OutputNames.Num() <= 0)
		return;

	if (AllBakeFolders.Num() <= 0)
		return;

	SKBuildSettings skBuildSettings;
	FHoudiniSkeletalMeshTranslator::CreateSKAssetAndPackage(skBuildSettings, GeoId, PartId, AllBakeFolders[0] + OutputNames[0]);

	// TODO: ?? Why twice?
	TArray<FSkeletalMaterial> Materials;
	FSkeletalMaterial Mat;
	Materials.Add(Mat);
	Materials.Add(Mat);
	FHoudiniSkeletalMeshTranslator::BuildSKFromImportData(skBuildSettings, Materials);
}
*/

USkeleton*
FHoudiniSkeletalMeshTranslator::CreateOrUpdateSkeleton(SKBuildSettings & BuildSettings)
{
	const HAPI_NodeId& GeoId = BuildSettings.GeoId;
	const HAPI_NodeId& PartId = BuildSettings.PartId;
	FSkeletalMeshImportData& SkeletalMeshImportData = BuildSettings.SkeletalMeshImportData;

	//ImportScale----------------------------------------------------------------------------------------
	HAPI_AttributeInfo UnrealSKImportScaleInfo;
	FHoudiniApi::AttributeInfo_Init(&UnrealSKImportScaleInfo);
	HAPI_Result UnrealSKImportScaleInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		"unreal_sk_import_scale",
		HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
		&UnrealSKImportScaleInfo);

	//check result
	float UnrealSKImportScale = 100.0f;
	if (UnrealSKImportScaleInfo.exists)
	{
		// TODO: Only get the first value since that's the one we're using?
		TArray<float> UnrealSKImportScaleArray;
		FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId,
			PartId,
			"unreal_sk_import_scale",
			UnrealSKImportScaleInfo,
			UnrealSKImportScaleArray,
			UnrealSKImportScaleInfo.tupleSize);

		if (UnrealSKImportScaleArray.Num() > 0)
		{
			UnrealSKImportScale = UnrealSKImportScaleArray[0];
		}
	}
	BuildSettings.ImportScale = UnrealSKImportScale;

	////Unreal Skeleton------------------------------------------------------------------------------------
	//HAPI_AttributeInfo UnrealSkeletonInfo;
	//FHoudiniApi::AttributeInfo_Init(&UnrealSkeletonInfo);

	//HAPI_Result UnrealSkeletonInfoResult = FHoudiniApi::GetAttributeInfo(
	//	FHoudiniEngine::Get().GetSession(),
	//	GeoId, PartId,
	//	"unreal_skeleton", HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, &UnrealSkeletonInfo);

	USkeleton* MySkeleton = nullptr;

	TArray<FString> CaptNamesData;
	TArray<FString> CaptNamesAltData;
	TArray<FString> WeightNamesData;


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

		// WeightNames are limited to the bones used by boneCapture point attribute
		// capt_names are the bones of the full skeleton
		//  attributecapt_names are the bones of the full skeleton

		// 
		// Load Skeleton from capt_data
		// 

		// ---------------------------------------------------------------------------
		// capt_names
		// 
		// capt_names are the bones of the full skeleton
		// ---------------------------------------------------------------------------
		HAPI_AttributeInfo CaptNamesInfo;
		FHoudiniApi::AttributeInfo_Init(&CaptNamesInfo);
		HAPI_Result CaptNamesInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			"capt_names",
			HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
			&CaptNamesInfo);

		HAPI_Int64 CaptNamesCount = CaptNamesInfo.exists ? CaptNamesInfo.totalArrayElements : 0;
		if (CaptNamesCount > 0)
		{
			// Extract the StringHandles
			TArray<HAPI_StringHandle> StringHandles;
			StringHandles.Init(-1, CaptNamesCount);

			TArray<int> SizesFixedArray;
			SizesFixedArray.SetNum(CaptNamesCount);

			HAPI_Result CaptNamesDataResult2 = FHoudiniApi::GetAttributeStringArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId,
				PartId,
				"capt_names",
				&CaptNamesInfo,
				&StringHandles[0],
				CaptNamesCount,
				&SizesFixedArray[0],
				0,
				CaptNamesInfo.count);

			// Set the output data size
			CaptNamesData.SetNum(StringHandles.Num());

			// Convert the StringHandles to FString.
			// using a map to minimize the number of HAPI calls
			FHoudiniEngineString::SHArrayToFStringArray(StringHandles, CaptNamesData);
		}
		else
		{
			// No capt names, should we return here?
			CaptNamesData.SetNum(0);
		}


		// ---------------------------------------------------------------------------
		// weight_names
		// 
		// WeightNames are limited to the bones used by boneCapture point		
		// ---------------------------------------------------------------------------		
		HAPI_AttributeInfo WeightNamesInfo;
		FHoudiniApi::AttributeInfo_Init(&WeightNamesInfo);
		HAPI_Result WeightNamesInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			"WeightNames",
			HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
			&WeightNamesInfo);

		HAPI_Int64 WeightNameCount = WeightNamesInfo.exists ? WeightNamesInfo.totalArrayElements : 0;
		if (WeightNameCount > 0)
		{
			// Extract the StringHandles
			TArray<HAPI_StringHandle> WeightNamesStringHandles;
			WeightNamesStringHandles.Init(-1, WeightNameCount);

			TArray<int> WeightNamesSizesFixedArray;
			WeightNamesSizesFixedArray.SetNum(WeightNameCount);

			HAPI_Result WeightNamesInfoResult2 = FHoudiniApi::GetAttributeStringArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId,
				PartId,
				"WeightNames",
				&WeightNamesInfo,
				&WeightNamesStringHandles[0],
				WeightNameCount,
				&WeightNamesSizesFixedArray[0],
				0,
				WeightNamesInfo.count);

			// Set the output data size
			WeightNamesData.SetNum(WeightNamesStringHandles.Num());

			// Convert the StringHandles to FString.
			// using a map to minimize the number of HAPI calls
			FHoudiniEngineString::SHArrayToFStringArray(WeightNamesStringHandles, WeightNamesData);
		}
		else
		{
			// No weight names, should we return here?
			WeightNamesData.SetNum(0);
		}

		//----------------------------------------------------------------------------
		// __DEPRECATED
		// capt_names_alt
		// the capture data bone names dont match the parent and transfer data bone names
		//----------------------------------------------------------------------------
		HAPI_AttributeInfo CaptNamesAltInfo;
		FHoudiniApi::AttributeInfo_Init(&CaptNamesAltInfo);
		HAPI_Result CaptNamesAltInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			"capt_names_alt",
			HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
			&CaptNamesAltInfo);

		HAPI_Int64 CaptNameAltCount = CaptNamesAltInfo.exists ? CaptNamesAltInfo.totalArrayElements : 0;
		if (CaptNameAltCount > 0)
		{
			// Extract the StringHandles
			TArray<HAPI_StringHandle> StringAltHandles;
			StringAltHandles.Init(-1, CaptNameAltCount);

			TArray<int> SizesAltFixedArray;
			SizesAltFixedArray.SetNum(CaptNameAltCount);
			HAPI_Result CaptNamesAltDataResult2 = FHoudiniApi::GetAttributeStringArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId,
				PartId,
				"capt_names_alt",
				&CaptNamesAltInfo,
				&StringAltHandles[0],
				CaptNameAltCount,
				&SizesAltFixedArray[0],
				0,
				CaptNamesAltInfo.count);

			// Set the output data size
			CaptNamesAltData.SetNum(StringAltHandles.Num());

			// Convert the StringHandles to FString.
			// using a map to minimize the number of HAPI calls
			FHoudiniEngineString::SHArrayToFStringArray(StringAltHandles, CaptNamesAltData);
		}

		//----------------------------------------------------------------------------
		// capt_xforms
		//----------------------------------------------------------------------------
		HAPI_AttributeInfo CaptXFormsInfo;
		FHoudiniApi::AttributeInfo_Init(&CaptXFormsInfo);
		HAPI_Result CaptXFormsInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			"capt_xforms",
			HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
			&CaptXFormsInfo);

		// TODO: Check? mix of totalArrayElements and count ?? why?
		TArray<float> XFormsData;
		HAPI_Int64 CaptXFormsCount = CaptXFormsInfo.exists ? CaptXFormsInfo.totalArrayElements : 0;
		XFormsData.SetNum(CaptXFormsCount);
		if (CaptXFormsCount > 0)
		{
			TArray<int> XFormSizesFixedArray;
			XFormSizesFixedArray.SetNum(CaptXFormsInfo.count);

			HAPI_Result CaptXFormsDataResult = FHoudiniApi::GetAttributeFloatArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId,
				PartId,
				"capt_xforms",
				&CaptXFormsInfo,
				&XFormsData[0],
				CaptXFormsCount,
				&XFormSizesFixedArray[0],
				0,
				CaptXFormsInfo.count);
		}
		else
		{
			// No XForms, return?
		}

		//----------------------------------------------------------------------------
		// capt_parents
		//----------------------------------------------------------------------------
		HAPI_AttributeInfo CaptParentsInfo;
		FHoudiniApi::AttributeInfo_Init(&CaptParentsInfo);
		HAPI_Result CaptParentsInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			"capt_parents",
			HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
			&CaptParentsInfo);

		HAPI_Int64 ParentDataCount = CaptParentsInfo.exists ? CaptParentsInfo.totalArrayElements : 0;
		TArray<int> ParentsData;
		ParentsData.SetNum(ParentDataCount);
		if (ParentDataCount > 0)
		{
			TArray<int> ParentSizesFixedArray;
			ParentSizesFixedArray.SetNum(CaptParentsInfo.count);

			HAPI_Result ParentsDataResult = FHoudiniApi::GetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId,
				PartId,
				"capt_parents",
				&CaptParentsInfo,
				&ParentsData[0],
				CaptParentsInfo.totalArrayElements,
				&ParentSizesFixedArray[0],
				0,
				CaptParentsInfo.count);
		}

		//----------------------------------------------------------------------------
		// Build RefBonesBinary 
		// 
		// two passes required since skeleton hierarchy might not be properly ordered		
		//----------------------------------------------------------------------------

		// first pass - load matrix, intitalize bone info
		TArray<FMatrix> MatrixData;
		MatrixData.SetNum(CaptNamesData.Num());
		SkeletalMeshImportData.RefBonesBinary.SetNum(CaptNamesData.Num());

		int32 BoneIdx = 0;
		for (FString BoneName : CaptNamesData)
		{
			SkeletalMeshImportData::FBone NewBone;
			NewBone.Name = BoneName;
			NewBone.Flags = 0;
			NewBone.ParentIndex = ParentsData.IsValidIndex(BoneIdx) ? ParentsData[BoneIdx] : -1;
			NewBone.NumChildren = 0;

			FMatrix M44;
			int32 row = 0;
			int32 col = 0;
			for (int32 i = 0; i < 16; i++)
			{
				M44.M[row][col] = XFormsData[16 * BoneIdx + i];
				col++;
				if (col > 3)
				{
					row++;
					col = 0;
				}
			}

			if (MatrixData.IsValidIndex(BoneIdx))
				MatrixData[BoneIdx] = M44;

			if (SkeletalMeshImportData.RefBonesBinary.IsValidIndex(BoneIdx))
				SkeletalMeshImportData.RefBonesBinary[BoneIdx] = NewBone;

			BoneIdx++;
		}

		// second pass to count children, calculate joint transform
		BoneIdx = 0;
		for (SkeletalMeshImportData::FBone& RefBone : SkeletalMeshImportData.RefBonesBinary)
		{
			int32 RefParentIndex = RefBone.ParentIndex;
			if (RefParentIndex != -1)
			{
				if (SkeletalMeshImportData.RefBonesBinary.IsValidIndex(RefParentIndex))
					SkeletalMeshImportData.RefBonesBinary[RefParentIndex].NumChildren++;
			}

			if (!MatrixData.IsValidIndex(BoneIdx))
				continue;

			FMatrix& M44 = MatrixData[BoneIdx];
			FTransform Transform = FTransform(M44);
			//UE_LOG(LogTemp, Log, TEXT("M44 Translation %s Rotation %s Scale %s"), *Transform.GetTranslation().ToString(), *Transform.GetRotation().ToString(), *Transform.GetScale3D().ToString());

			FMatrix Final;
			if (RefParentIndex == -1)
			{
				//no Parent for root
				Final = M44;
			}
			else
			{
				if (MatrixData.IsValidIndex(RefParentIndex))
				{
					//Final = MatrixData[Bone.ParentIndex].Inverse() * M44;
					Final = M44 * MatrixData[RefParentIndex].Inverse();
				}
				else
				{
					// Error? no parent transform
					Final = M44;
				}
			}

			FTransform FinalTransform = FTransform(Final);

			// account for unit difference
			FinalTransform.ScaleTranslation(UnrealSKImportScale);

			//CONVERSION ADJUSTMENTS
			FVector Translation = FinalTransform.GetTranslation();
			FRotator Rotator = FinalTransform.GetRotation().Rotator();
			FRotator FixedRotator = FRotator(Rotator.Pitch, -Rotator.Yaw, -Rotator.Roll);

			// coord conversion for Root Bone
			if (RefBone.ParentIndex == -1)
			{
				//TODO Fix dependency on zero rotation/translation on root bone
				FRotator Converter = FRotator(0.0, 0.0f, 90.0f);
				FixedRotator += Converter;
			}

			FinalTransform.SetTranslation(FVector(Translation.X, -Translation.Y, Translation.Z));
			FinalTransform.SetRotation(FixedRotator.Quaternion());
			FinalTransform.SetScale3D(FVector(1.0f, 1.0f, 1.0f));

			SkeletalMeshImportData::FJointPos JointPos;
			JointPos.Transform = FTransform3f(FinalTransform);
			RefBone.BonePos = JointPos;
			BoneIdx++;
		}
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
			FHoudiniEngineUtils::HapiGetAttributeDataAsString(GeoId, PartId, "unreal_skeleton", UnrealSkeletonInfo, UnrealSkeletonData);
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
		//TArray<FTransform3f>& RawRefBonePose = MySkeleton->GetReferenceSkeleton().GetRawRefBonePose();
		//Populate RefBonesBinary from Existing Skeleton Asset

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

	//Bonecapture-----------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo BoneCaptureInfo;
	FHoudiniApi::AttributeInfo_Init(&BoneCaptureInfo);
	HAPI_Result AttributeInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		"boneCapture",
		HAPI_AttributeOwner::HAPI_ATTROWNER_POINT,
		&BoneCaptureInfo);

	//if not fbx imported, these indexes match CaptNamesAltData sorting
	TArray<float> BoneCaptureData;
	FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId, "boneCapture", BoneCaptureInfo, BoneCaptureData);

	SkeletalMeshImportData::FRawBoneInfluence RawBoneInfluence;
	RawBoneInfluence.BoneIndex = 0;
	RawBoneInfluence.VertexIndex = 0;
	RawBoneInfluence.Weight = 0;

	int32 InfluenceVertIndex = 0;
	int32 BoneInfluence_idx = 0;
	int32 CaptureCount = 0;
	float sum = 0;
	int32 bonecount = 0;

	//TODO If possible, allow importing of direct roundtrip without use of UnrealSK

	//Process the incoming weight data 
	//Data is in [idx,weight] pair, with tuple representing stride for that vertex
	//NOTE the indexs refer to WeightNamesData not the actual Skeleton CaptNamesData

	FString BoneName;
	int firstinfluence = 0;
	int32 TotalPairs = BoneCaptureData.Num() / 2;
	for (int32 i = 0; i < TotalPairs; i++)
	{
		// count the pairs
		CaptureCount++;

		// set current weigth/index pair
		float idx = BoneCaptureData[i * 2];
		float weight = BoneCaptureData[(i * 2) + 1];

		if (WeightNamesData.IsValidIndex(idx))
		{
			BoneName = WeightNamesData[idx];
			float RemappedIndex = CaptNamesData.Find(BoneName);
			if (RemappedIndex < 0)
			{
				//BROKEN
				// TODO? We should probably do something about this
			}
			else
			{
				idx = RemappedIndex;
			}
		}

		// remap if not fbx imported
		if (CaptNamesAltData.IsValidIndex(idx))
		{
			BoneName = CaptNamesAltData[idx];
			idx = CaptNamesData.Find(BoneName);
		}

		// fix up index due to alt names
		RawBoneInfluence.BoneIndex = idx;

		if (RawBoneInfluence.BoneIndex >= 0)
		{
			RawBoneInfluence.VertexIndex = InfluenceVertIndex;
			if (weight < 0.0)
			{
				RawBoneInfluence.Weight = 0.0f;
			}
			else
			{
				RawBoneInfluence.Weight = weight;
			}

			// UE_LOG(LogTemp, Log, TEXT("RawBoneInfluence: vertindex %i bonecount %i %s %i %i  %f %f"), InfluenceVertIndex, bonecount, *BoneName, RawBoneInfluence.BoneIndex, RawBoneInfluence.VertexIndex, RawBoneInfluence.Weight, sum);
			if ((sum + RawBoneInfluence.Weight) > 1.0f)
			{

				// UE_LOG(LogTemp, Log, TEXT("ERROR:  SUM would be %f  clamping wieght to %f"), sum, 1.0 - sum);
				RawBoneInfluence.Weight = 1.0 - sum;
			}
			sum += RawBoneInfluence.Weight;

			if (RawBoneInfluence.BoneIndex == 0)
			{
				//UE_LOG(LogTemp, Log, TEXT("WARNING:  Root Bone weight %f"), RawBoneInfluence.Weight);
			}

			int32 newinfluence = SkeletalMeshImportData.Influences.Add(RawBoneInfluence);
			if (bonecount == 0)
			{
				firstinfluence = newinfluence;
			}
			bonecount++;
		}

		// fixup so sum of weights is 1
		int32 Stride = CaptureCount * 2;
		//Should work with any tuple size  || NOT CONSTANT TUPLE SIZE??
		if ((Stride % BoneCaptureInfo.tupleSize) == 0)
		{
			//	if (!FMath::IsNearlyEqual(sum, 1.0f, 0.0001f))
			//	{
			//		SkeletalMeshImportData.Influences[firstinfluence].Weight += (1.0f - sum);
			//		//UE_LOG(LogTemp, Log, TEXT("ERRROR InfluenceVertIndex %i Sum %f bone %i weight fixed to %f "), InfluenceVertIndex, sum, firstinfluence, SkeletalMeshImportData.Influences[firstinfluence].Weight);
			//	}
			//	else
			//	{
			//		//UE_LOG(LogTemp, Log, TEXT("InfluenceVertIndex %i Sum %f"), InfluenceVertIndex, sum);
			//	}
			InfluenceVertIndex++;
			sum = 0;
			bonecount = 0;
		}
	}

	//for (float BoneCapture : BoneCaptureData)
	//{
	//	CaptureCount++;
	//	if ((CaptureCount % 2) == 0)  //have last dat afor this BoneInfluence so store
	//	{

	//		//RawBoneInfluence.Weight = 1.0f;
	//		//if (BoneCapture > 0)
	//		//{
	//		//if ((bonecount < 1)  && (RawBoneInfluence.BoneIndex >= 0))
	//		if (RawBoneInfluence.BoneIndex >= 0)
	//		{
	//			RawBoneInfluence.VertexIndex = InfluenceVertIndex;
	//			if (BoneCapture < 0.0)
	//			{
	//				RawBoneInfluence.Weight = 0.0f;
	//			}
	//			else
	//			{
	//				RawBoneInfluence.Weight = BoneCapture;
	//			}
	//			int32 newinfluence = SkeletalMeshImportData.Influences.Add(RawBoneInfluence);
	//			if (bonecount == 0)
	//			{
	//				firstinfluence = newinfluence;
	//			}
	//			//UE_LOG(LogTemp, Log, TEXT("RawBoneInfluence: vertindex %i bonecount %i %s %i %i  %f"), InfluenceVertIndex, bonecount, *BoneName, RawBoneInfluence.BoneIndex, RawBoneInfluence.VertexIndex, RawBoneInfluence.Weight);
	//			sum += RawBoneInfluence.Weight;
	//			bonecount++;
	//		}
	//		//}
	//		BoneInfluence_idx = 0;
	//	}
	//	else
	//	{
	//		//RawBoneInfluence.BoneIndex = BoneCapture;
	//		int32 idx = BoneCapture;
	//		if (CaptNamesAltData.Num() > 0)	 //remap if not fbx imported
	//		{
	//			if (idx > 0)
	//			{
	//				BoneName = CaptNamesAltData[idx];
	//				idx = CaptNamesData.Find(BoneName);
	//			}
	//		}
	//		RawBoneInfluence.BoneIndex = idx;//fix up index due to alt names  
	//	}
	//	if ((CaptureCount % BoneCaptureInfo.tupleSize) == 0)	//Should work with any tuple size
	//	{
	//		if (!FMath::IsNearlyEqual(sum, 1.0f, 0.0001f))
	//		{
	//			SkeletalMeshImportData.Influences[firstinfluence].Weight += (1.0f - sum);
	//			//UE_LOG(LogTemp, Log, TEXT("ERRROR InfluenceVertIndex %i Sum %f bone %i weight fixed to %f "), InfluenceVertIndex, sum, firstinfluence, SkeletalMeshImportData.Influences[firstinfluence].Weight);
	//		}
	//		else
	//		{
	//			//UE_LOG(LogTemp, Log, TEXT("InfluenceVertIndex %i Sum %f"), InfluenceVertIndex, sum);
	//		}
	//		InfluenceVertIndex++;
	//		sum = 0;
	//		bonecount = 0;
	//	}
	//}

	return MySkeleton;
}


//Creates and populates an FSkeletalMeshImportData by reading Houdini Attribute Data
//then calls BuildSKFromImportData to create sk mesh from it
void
FHoudiniSkeletalMeshTranslator::CreateSKAssetAndPackage(
	SKBuildSettings & BuildSettings,
	const HAPI_NodeId & GeoId,
	const HAPI_NodeId & PartId,
	FString PackageName,
	int MaxInfluences,
	bool ImportNormals)
{
	FString SKMeshName = FPackageName::GetShortName(PackageName);

	//UPackage* Package = CreatePackage(nullptr, *PackageName);
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	USkeletalMesh* NewMesh = nullptr;
	NewMesh = NewObject<USkeletalMesh>(Package, FName(*SKMeshName), RF_Public | RF_Standalone | RF_MarkAsRootSet);

	//Unreal Skeleton------------------------------------------------------------------------------------
	HAPI_AttributeInfo UnrealSkeletonInfo;
	FHoudiniApi::AttributeInfo_Init(&UnrealSkeletonInfo);
	HAPI_Result UnrealSkeletonInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		"unreal_skeleton",
		HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL,
		&UnrealSkeletonInfo);

	USkeleton* MySkeleton = nullptr;

	TArray<FString> CaptNamesData;
	TArray<FString> CaptNamesAltData;

	BuildSettings.bIsNewSkeleton = !UnrealSkeletonInfo.exists;
	if ((BuildSettings.OverwriteSkeleton) && (!BuildSettings.SkeletonAssetPath.IsEmpty()))
	{
		//NodeSync Settings overrides the unreal_skeleton attribute
		BuildSettings.bIsNewSkeleton = false;
	}

	if (BuildSettings.bIsNewSkeleton)
	{
		FString SkeltonPackageName = PackageName + "Skeleton";
		FString SkeletonName = FPackageName::GetShortName(SkeltonPackageName);

		UPackage* SkeletonPackage = CreatePackage(*SkeltonPackageName);
		SkeletonPackage->FullyLoad();

		USkeleton* NewSkeleton = nullptr;
		NewSkeleton = NewObject<USkeleton>(SkeletonPackage, FName(*SkeletonName), RF_Public | RF_Standalone | RF_MarkAsRootSet);
		BuildSettings.Skeleton = NewSkeleton;
	}

	//Skeleton
	BuildSettings.ImportNormals = ImportNormals;
	BuildSettings.GeoId = GeoId;
	BuildSettings.PartId = PartId;
	BuildSettings.SKMesh = NewMesh;
	BuildSettings.SKPackage = Package;
	FHoudiniSkeletalMeshTranslator::CreateOrUpdateSkeleton(BuildSettings);
	SKImportData(BuildSettings);

	//Materials
	//TArray<FSkeletalMaterial> Materials;
	//FSkeletalMaterial Mat;
	//Materials.Add(Mat);
	//Materials.Add(Mat);	

	//BuildSKFromImportData(BuildSettings, Materials);
}

void
FHoudiniSkeletalMeshTranslator::SKImportData(SKBuildSettings & BuildSettings)
{
	HAPI_NodeId GeoId = BuildSettings.GeoId;
	HAPI_NodeId PartId = BuildSettings.PartId;
	FSkeletalMeshImportData& SkeletalMeshImportData = BuildSettings.SkeletalMeshImportData;

	//---------------------------------------------------------------------------------- -
	// Points
	//-----------------------------------------------------------------------------------
	HAPI_AttributeInfo PositionInfo;
	FHoudiniApi::AttributeInfo_Init(&PositionInfo);

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_POSITION,
		HAPI_AttributeOwner::HAPI_ATTROWNER_POINT,
		&PositionInfo))
	{
		HOUDINI_LOG_ERROR(TEXT("Error Creating Skeletal Mesh : No Points Info"));
		return;
	}

	if (PositionInfo.count <= 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error Creating Skeletal Mesh : No Points Info"));
		return;
	}

	TArray<FVector3f> PositionData;
	// we dont need to multiply by tupleSize, its already a vector container
	PositionData.SetNum(PositionInfo.count);

	//FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId, HAPI_UNREAL_ATTRIB_POSITION, PositionInfo, PositionData);
	FHoudiniApi::GetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_POSITION,
		&PositionInfo,
		-1,
		(float*)&PositionData[0],
		0,
		PositionInfo.count);

	// we dont need to multiply by tupleSize, its already a vector container
	SkeletalMeshImportData.Points.SetNum(PositionInfo.count);
	int32 c = 0;
	for (FVector3f Point : PositionData)
	{
		//flip x and z
		SkeletalMeshImportData.Points[c] = FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector3f(Point);
		SkeletalMeshImportData.PointToRawMap.Add(c);
		c++;
	}

	//-----------------------------------------------------------------------------------
	// Point UVs
	//-----------------------------------------------------------------------------------

	// TODO? UVs are manually fetched here, we should use a dynamic version like we do for meshes..

	HAPI_AttributeInfo PointUVInfo;
	TArray<float> PointUVData;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_UV,
		PointUVInfo,
		PointUVData))
	{
		BuildSettings.NumTexCoords = 1;
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error Creating Skeletal Mesh :  Invalid UV Data"));
	}

	// Point UVs second set -----------------------------------------------------------------------------------
	HAPI_AttributeInfo PointUV1Info;
	TArray<float> PointUV1Data;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId,
		PartId,
		"uv1",
		PointUV1Info,
		PointUV1Data))
	{
		BuildSettings.NumTexCoords = 2;
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error Creating Skeletal Mesh :  Invalid UV1 Data"));
	}

	//Point UVs third set -----------------------------------------------------------------------------------
	HAPI_AttributeInfo PointUV2Info;
	TArray<float> PointUV2Data;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId,
		PartId,
		"uv2",
		PointUV2Info,
		PointUV2Data))
	{
		BuildSettings.NumTexCoords = 3;
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error Creating Skeletal Mesh :  Invalid UV2 Data"));
	}

	//-----------------------------------------------------------------------------------
	// Normals
	//-----------------------------------------------------------------------------------
	HAPI_AttributeInfo NormalInfo;
	FHoudiniApi::AttributeInfo_Init(&NormalInfo);

	HAPI_Result NormalInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_NORMAL,
		HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX,
		&NormalInfo);

	bool bUseComputedNormals = !BuildSettings.ImportNormals;
	TArray<FVector3f> NormalData;
	if (NormalInfo.exists && NormalInfo.count > 0)
	{
		//we dont need to multiply by tupleSize, its already a vector container
		NormalData.SetNum(NormalInfo.count);

		//FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId, HAPI_UNREAL_ATTRIB_POSITION, PositionInfo, PositionData);
		FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			HAPI_UNREAL_ATTRIB_NORMAL,
			&NormalInfo,
			-1,
			(float*)&NormalData[0],
			0,
			NormalInfo.count);
	}
	else
	{
		//Use Computed Normals
		bUseComputedNormals = true;
	}

	//-----------------------------------------------------------------------------------
	// Tangents
	//-----------------------------------------------------------------------------------
	HAPI_AttributeInfo TangentInfo;
	TArray<float> TangentData;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_TANGENTU,
		TangentInfo,
		TangentData))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error Creating Skeletal Mesh :  Invalid Tangent Data"));
	}

	//-----------------------------------------------------------------------------------
	// Materials
	//-----------------------------------------------------------------------------------
	HAPI_AttributeInfo MaterialInfo;
	FHoudiniApi::AttributeInfo_Init(&MaterialInfo);
	HAPI_Result MaterialInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		HAPI_UNREAL_ATTRIB_MATERIAL,
		HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM,
		&MaterialInfo);

	TArray<FString> MaterialNamesData;
	if (MaterialInfo.exists && MaterialInfo.count > 0)
	{
		// Extract the StringHandles
		TArray<HAPI_StringHandle> MaterialStringHandles;
		MaterialStringHandles.SetNumUninitialized(MaterialInfo.count * MaterialInfo.tupleSize);
		HAPI_Result MaterialDataResult = FHoudiniApi::GetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			GeoId,
			PartId,
			HAPI_UNREAL_ATTRIB_MATERIAL,
			&MaterialInfo,
			&MaterialStringHandles[0],
			0,
			MaterialInfo.count);

		// Set the output data size
		MaterialNamesData.SetNum(MaterialStringHandles.Num());

		// Convert the StringHandles to FString.
		// using a map to minimize the number of HAPI calls
		FHoudiniEngineString::SHArrayToFStringArray(MaterialStringHandles, MaterialNamesData);

		//Set unique material names onto FSkeletalMeshImportData
		TSet<FString> UniqueMaterialNames;
		for (FString MaterialName : MaterialNamesData)
		{
			UniqueMaterialNames.Add(MaterialName);
		}

		for (FString MaterialName : UniqueMaterialNames)
		{
			SkeletalMeshImportData::FMaterial SKMIDMaterial;
			SKMIDMaterial.MaterialImportName = MaterialName;
			SkeletalMeshImportData.Materials.Add(SKMIDMaterial);
		}

		int32 NumFaces = MaterialInfo.count;
		TArray<int32> PartFaceMaterialIds;
		PartFaceMaterialIds.SetNum(NumFaces);

		if (NumFaces > 0)
		{
			HAPI_Bool bSingleFaceMaterial = false;
			HAPI_Result GetMaterialNodeIdsOnFacesResult = FHoudiniApi::GetMaterialNodeIdsOnFaces(
				FHoudiniEngine::Get().GetSession(),
				GeoId,
				PartId,
				&bSingleFaceMaterial,
				&PartFaceMaterialIds[0],
				0,
				NumFaces);
		}
	}

	//-----------------------------------------------------------------------------------
	// Indices
	//-----------------------------------------------------------------------------------
	HAPI_AttributeInfo VertexInfo;
	FHoudiniApi::AttributeInfo_Init(&VertexInfo);

	HAPI_Result VertexInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		"__vertex_id",
		HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX,
		&VertexInfo);

	if (!VertexInfo.exists || VertexInfo.count <= 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error Creating Skeletal Mesh :  No Vertex Info"));
		return;
	}

	TArray<int> VertexData;
	VertexData.SetNum(VertexInfo.count);
	//FHoudiniEngineUtils::HapiGetAttributeDataAsInt(GeoId, PartId, "__vertex_id", VertexInfo, VertexData);
	//FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), GeoId, PartId, "__vertex_id", &VertexInfo, -1, &VertexData[0], 0, VertexInfo.count);
	HAPI_Result VertexDataResult = FHoudiniApi::GetVertexList(
		FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
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

	SkeletalMeshImportData::FTriangle Triangle;
	for (int32 VertexIndex : VertexData)
	{
		SkeletalMeshImportData::FVertex Wedge;
		Wedge.VertexIndex = VertexIndex;

		// TODO? Handle multiple UVs properly?
		//Wedge.VertexIndex = count;  //HACK TO FIX WINDING ORDER
		//Wedge.Color =
		if (PointUVData.Num() > VertexIndex * 3)
		{
			FVector3f uv0 = FVector3f(PointUVData[VertexIndex * 3], PointUVData[VertexIndex * 3 + 1], PointUVData[VertexIndex * 3 + 2]);
			Wedge.UVs[0] = FVector2f(uv0.X, 1.0f - uv0.Y);
		}
		if (PointUV1Data.Num() > VertexIndex * 3)
		{
			FVector3f uv1 = FVector3f(PointUV1Data[VertexIndex * 3], PointUV1Data[VertexIndex * 3 + 1], PointUV1Data[VertexIndex * 3 + 2]);
			Wedge.UVs[1] = FVector2f(uv1.X, 1.0f - uv1.Y);
		}
		if (PointUV2Data.Num() > VertexIndex * 3)
		{
			FVector3f uv2 = FVector3f(PointUV2Data[VertexIndex * 3], PointUV2Data[VertexIndex * 3 + 1], PointUV2Data[VertexIndex * 3 + 2]);
			Wedge.UVs[2] = FVector2f(uv2.X, 1.0f - uv2.Y);
		}

		//Wedge.MatIndex = 
		SkeletalMeshImportData.Wedges.Add(Wedge);
		Triangle.WedgeIndex[face_idx] = count;
		Triangle.SmoothingGroups = 255;
		Triangle.MatIndex = 0;


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
			//Hack To Fix Winding
			//uint32 Temp = Triangle.WedgeIndex[2];
			//Triangle.WedgeIndex[2] = Triangle.WedgeIndex[1];
			//Triangle.WedgeIndex[1] = Temp;
			SkeletalMeshImportData::FVertex Wedge1 = SkeletalMeshImportData.Wedges[count - 3];
			SkeletalMeshImportData::FVertex Wedge2 = SkeletalMeshImportData.Wedges[count - 2];
			SkeletalMeshImportData::FVertex Wedge3 = SkeletalMeshImportData.Wedges[count - 1];

			SkeletalMeshImportData.Wedges[count - 3] = Wedge3;//1
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

	/*
	UE_LOG(LogTemp, Log, TEXT("SkeletalMeshImportData:	Materials %i Points %i Normals %i Wedges %i Faces %i Influences %i"), SkeletalMeshImportData.Materials.Num(),
		SkeletalMeshImportData.Points.Num(),
		NormalData.Num(),
		SkeletalMeshImportData.Wedges.Num(),
		SkeletalMeshImportData.Faces.Num(),
		SkeletalMeshImportData.Influences.Num()
	);
	*/

	SkeletalMeshImportData.bDiffPose = false;
	SkeletalMeshImportData.bUseT0AsRefPose = false;
	//SkeletalMeshImportData.bHasTangents = false;
	//SkeletalMeshImportData.bHasNormals = false;
	SkeletalMeshImportData.bHasVertexColors = false;

	//FSkeletalMeshImportData TestSkeletalMeshImportData;
	//BuildSK(SkeletalMeshImportData, Materials);
	SkeletalMeshImportData.bHasNormals = true;
	SkeletalMeshImportData.bHasTangents = false;
}

/*
void
FHoudiniSkeletalMeshTranslator::ExportSkeletalMeshAssets(UHoudiniOutput * InOutput)
{
	// Iterate on all of the output's HGPO, creating meshes as we go
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->HoudiniGeoPartObjects)
	{
		if (FHoudiniSkeletalMeshTranslator::HasSkeletalMeshData(CurHGPO.GeoId, CurHGPO.PartId))
		{
			FHoudiniSkeletalMeshTranslator::LoadImportData(CurHGPO.GeoId, CurHGPO.PartId);
		}
	}
}
*/


//Creates SkelatalMesh and Skeleton Assets and Packages, and adds them to OutputObjects
bool FHoudiniSkeletalMeshTranslator::CreateSkeletalMesh_SkeletalMeshImportData()
{
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier(
		HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, "");
	OutputObjectIdentifier.PartName = HGPO.PartName;

	FHoudiniOutputObject& OutputObject = OutputObjects.FindOrAdd(OutputObjectIdentifier);
	USkeletalMesh* NewSkeletalMesh = CreateNewSkeletalMesh(OutputObjectIdentifier.SplitIdentifier);
	USkeleton* NewSkeleton = CreateNewSkeleton(OutputObjectIdentifier.SplitIdentifier);
	OutputObject.OutputObject = NewSkeletalMesh;
	OutputObject.bProxyIsCurrent = false;

	//TODO - Check to see whether new skeleton or existing skeleton

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
	skBuildSettings.GeoId = HGPO.GeoId;
	skBuildSettings.PartId = HGPO.PartId;
	skBuildSettings.ImportNormals = true;
	skBuildSettings.SKMesh = NewSkeletalMesh;
	skBuildSettings.bIsNewSkeleton = true;
	skBuildSettings.Skeleton = NewSkeleton;
	TArray<FSkeletalMaterial> Materials;
	FSkeletalMaterial Mat;
	Materials.Add(Mat);
	Materials.Add(Mat);
	skBuildSettings.Skeleton = FHoudiniSkeletalMeshTranslator::CreateOrUpdateSkeleton(skBuildSettings);
	SKImportData(skBuildSettings);
	FHoudiniSkeletalMeshTranslator::BuildSKFromImportData(skBuildSettings, Materials);

	return true;
}


USkeleton*
FHoudiniSkeletalMeshTranslator::CreateNewSkeleton(const FString& InSplitIdentifier)
{
	FHoudiniPackageParams SkeltonPackageParams;
	SkeltonPackageParams.GeoId = HGPO.GeoId;
	SkeltonPackageParams.PartId = HGPO.PartId;
	SkeltonPackageParams.ComponentGUID = PackageParams.ComponentGUID;
	SkeltonPackageParams.HoudiniAssetName = PackageParams.HoudiniAssetName;
	SkeltonPackageParams.SplitStr = InSplitIdentifier;
	SkeltonPackageParams.ObjectName = FString::Printf(TEXT("%s_%d_%d_%d_%sSkeleton"), *PackageParams.HoudiniAssetName, PackageParams.ObjectId, PackageParams.GeoId, PackageParams.PartId, *PackageParams.SplitStr);

	USkeleton* NewSkeleton = SkeltonPackageParams.CreateObjectAndPackage<USkeleton>();
	if (!IsValid(NewSkeleton))
		return nullptr;

	return NewSkeleton;
}

USkeletalMesh*
FHoudiniSkeletalMeshTranslator::CreateNewSkeletalMesh(const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	PackageParams.ObjectId = HGPO.ObjectId;
	PackageParams.GeoId = HGPO.GeoId;
	PackageParams.PartId = HGPO.PartId;
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
		PackageParams.ObjectId = HGPO.ObjectId;
		PackageParams.GeoId = HGPO.GeoId;
		PackageParams.PartId = HGPO.PartId;
	}
}


bool
FHoudiniSkeletalMeshTranslator::CreateAllSkeletalMeshesAndComponentsFromHoudiniOutput(
	UHoudiniOutput* InOutput,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
	UObject* InOuterComponent)
{
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InPackageParams.OuterPackage))
		return false;

	if (!IsValid(InOuterComponent))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OldOutputObjects = InOutput->GetOutputObjects();
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignementMaterials = InOutput->GetAssignementMaterials();
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterials = InOutput->GetReplacementMaterials();

	bool InForceRebuild = false;

	// Iterate on all of the output's HGPO, creating meshes as we go
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->HoudiniGeoPartObjects)
	{
		// Not a skeletal mesh, skip
		if (CurHGPO.Type != EHoudiniPartType::SkeletalMesh)
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

		CreateSkeletalMeshFromHoudiniGeoPartObject(
			CurHGPO,
			InPackageParams,
			OldOutputObjects,
			NewOutputObjects);
	}

	return FHoudiniMeshTranslator::CreateOrUpdateAllComponents(
		InOutput,
		InOuterComponent,
		NewOutputObjects);
}


bool
FHoudiniSkeletalMeshTranslator::CreateSkeletalMeshFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects)
{
	// Make sure this is indeed a Skeletal Mesh
	if (!FHoudiniSkeletalMeshTranslator::HasSkeletalMeshData(InHGPO.GeoId, InHGPO.PartId))
		return false;

	FHoudiniSkeletalMeshTranslator SKMeshTranslator;
	SKMeshTranslator.SetHoudiniGeoPartObject(InHGPO);
	SKMeshTranslator.SetOutputObjects(OutOutputObjects);
	SKMeshTranslator.SetPackageParams(InPackageParams, true);
	if (SKMeshTranslator.CreateSkeletalMesh_SkeletalMeshImportData())
	{
		// Copy the output objects/materials
		OutOutputObjects = SKMeshTranslator.OutputObjects;
		//AssignmentMaterialMap = SKMT.OutputAssignmentMaterials;

		return true;
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE