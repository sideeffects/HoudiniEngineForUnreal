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
#include "PhysicsAssetGenerationSettings.h"
#include "PhysicsAssetUtils.h"
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ReferenceSkeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "PhysicsEngine/BodySetup.h"
	#include "PhysicsEngine/SkeletalBodySetup.h"
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
	}
}



//Builds Skeletal Mesh and Skeleton Assets from FSkeletalMeshImportData
void
FHoudiniSkeletalMeshTranslator::CreateUnrealData(FHoudiniSkeletalMeshBuildSettings& BuildSettings)
{
	FSkeletalMeshImportData& ImportData = BuildSettings.SkeletalMeshImportData;
	USkeleton* Skeleton = BuildSettings.Skeleton;
	FBox3f BoundingBox(ImportData.Points.GetData(), ImportData.Points.Num());

	// Setup NewMesh defaults
	FSkeletalMeshModel* ImportedResource = BuildSettings.SKMesh->GetImportedModel();
	check(ImportedResource->LODModels.Num() == 0);
	ImportedResource->LODModels.Empty();
	ImportedResource->LODModels.Add(new FSkeletalMeshLODModel());
	const int32 ImportLODModelIndex = 0;
	FSkeletalMeshLODModel& NewLODModel = ImportedResource->LODModels[ImportLODModelIndex];


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
	BuildSettings.SKMesh->SaveLODImportedData(0, ImportData);  //Import the ImportData
#endif

	int32 SkeletalDepth = 0;
	FReferenceSkeleton& RefSkeleton = BuildSettings.SKMesh->GetRefSkeleton();
	bool bSuccess = SkeletalMeshImportUtils::ProcessImportMeshSkeleton(Skeleton, RefSkeleton, SkeletalDepth, ImportData);
	if (!bSuccess)
	{
		HOUDINI_LOG_ERROR(TEXT("SkeletalMeshImportUtils::ProcessImportMeshSkeleton() failed."));
		return;
	}

	for (SkeletalMeshImportData::FMaterial SkeletalImportMaterial : ImportData.Materials)
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
	SkeletalMeshImportUtils::ProcessImportMeshInfluences(ImportData, BuildSettings.SKMesh->GetPathName());

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	BuildSettings.SKMesh->SetNumSourceModels(0);
#else
	BuildSettings.SKMesh->ResetLODInfo();
#endif
	FSkeletalMeshLODInfo& NewLODInfo = BuildSettings.SKMesh->AddLODInfo();
	NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
	NewLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
	NewLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
	NewLODInfo.LODHysteresis = 0.02f;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	BuildSettings.SKMesh->SaveLODImportedData(ImportLODModelIndex, ImportData);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	FBoxSphereBounds3f bsb3f = FBoxSphereBounds3f(BoundingBox);
	BuildSettings.SKMesh->SetImportedBounds(FBoxSphereBounds(bsb3f));
	// Store whether or not this mesh has vertex colors
	BuildSettings.SKMesh->SetHasVertexColors(ImportData.bHasVertexColors);
	//NewMesh->VertexColorGuid = Mesh->bHasVertexColors ? FGuid::NewGuid() : FGuid();

	// Pass the number of texture coordinate sets to the LODModel.	Ensure there is at least one UV coord
	NewLODModel.NumTexCoords = ImportData.NumTexCoords;

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	FSkeletalMeshRenderData* RenderData = BuildSettings.SKMesh->GetResourceForRendering();
	if (RenderData == nullptr)
	{
		BuildSettings.SKMesh->AllocateResourceForRendering();
		RenderData = BuildSettings.SKMesh->GetResourceForRendering();
	}
		
	bool bBuildSuccess = MeshBuilderModule.BuildSkeletalMesh(*RenderData, SkeletalMeshBuildParameters);
#else
	bool bBuildSuccess = MeshBuilderModule.BuildSkeletalMesh(SkeletalMeshBuildParameters);
#endif

	//We need to have a valid render data to create physic asset
	BuildSettings.SKMesh->CalculateInvRefMatrices();
	BuildSettings.SKMesh->Build();
	BuildSettings.SKMesh->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(BuildSettings.SKMesh);

	//CREATE A NEW SKELETON ASSET IF NEEDED
	if (Skeleton == nullptr)
	{
		FString ObjectName = FString::Printf(TEXT("%s_Skeleton"), *BuildSettings.SKMesh->GetName());
		Skeleton = NewObject<USkeleton>(BuildSettings.SKPackage, *ObjectName, RF_Public | RF_Standalone);
		Skeleton->MarkPackageDirty();
	}
	bSuccess = Skeleton->MergeAllBonesToBoneTree(BuildSettings.SKMesh);
	if (!bSuccess)
	{
		HOUDINI_LOG_ERROR(TEXT("MergeAllBonesToBoneTree() failed."));
		return;
	}

	BuildSettings.SKMesh->SetSkeleton(Skeleton);
	UE_LOG(LogTemp, Log, TEXT("SkeletalMeshImportData:	Materials %i Points %i Wedges %i Faces %i Influences %i"), 
		ImportData.Materials.Num(),
		ImportData.Points.Num(),
		ImportData.Wedges.Num(),
		ImportData.Faces.Num(),
		ImportData.Influences.Num());
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



float
FHoudiniSkeletalMeshTranslator::GetSkeletonImportScale(const FHoudiniGeoPartObject& ShapeMeshHGPO)
{
	HAPI_NodeId GeoId = ShapeMeshHGPO.GeoId;
	HAPI_NodeId PartId = ShapeMeshHGPO.PartId;

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
	return UnrealSKImportScale;
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

FHoudiniSkeletalMesh
FHoudiniSkeletalMeshTranslator::GetSkeletalMeshMeshData(HAPI_NodeId ShapeGeoId, HAPI_NodeId ShapePartId, bool bImportNormals)
{
	FHoudiniSkeletalMesh Mesh;

	//-----------------------------------------------------------------------------------
	// Shape Infos
	//-----------------------------------------------------------------------------------

	HAPI_PartInfo ShapeMeshPartInfo;
	FHoudiniApi::PartInfo_Init(&ShapeMeshPartInfo);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), ShapeGeoId, ShapePartId, &ShapeMeshPartInfo);

	//-----------------------------------------------------------------------------------
	// Rest Geometry Points
	//-----------------------------------------------------------------------------------

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_POSITION);
	bool bSuccess = Accessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, Mesh.Positions);

	//-----------------------------------------------------------------------------------
	// UVs
	//-----------------------------------------------------------------------------------

	FHoudiniEngineUtils::UpdateMeshPartUVSets(ShapeGeoId, ShapePartId, true, Mesh.UVSets, Mesh.AttribInfoUVSets);

	//-----------------------------------------------------------------------------------
	// Normals
	//-----------------------------------------------------------------------------------

	bool bUseComputedNormals = !bImportNormals;
	if (!bUseComputedNormals)
	{
		Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_NORMAL);
		bSuccess = Accessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, Mesh.Normals);
	}

	//-----------------------------------------------------------------------------------
	// Vertex Colors
	//-----------------------------------------------------------------------------------

	Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_COLOR);
	Accessor.GetInfo(Mesh.ColorInfo, HAPI_ATTROWNER_INVALID);
	bool bColorInfoExists = Accessor.GetAttributeData(Mesh.ColorInfo, Mesh.Colors);

	//-----------------------------------------------------------------------------------
	// Tangents
	//-----------------------------------------------------------------------------------

	Accessor.Init(ShapeGeoId, ShapePartId, HAPI_UNREAL_ATTRIB_TANGENTU);
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Mesh.Tangents);

	//-----------------------------------------------------------------------------------
	// Materials
	//-----------------------------------------------------------------------------------

	Mesh.Materials = GetMaterials(ShapeGeoId, ShapePartId, ShapeMeshPartInfo.faceCount);

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
		return {};
	}


	Mesh.Vertices.SetNum(VertexInfo.count);
	HAPI_Result VertexDataResult = FHoudiniApi::GetVertexList(
		FHoudiniEngine::Get().GetSession(),
		ShapeGeoId,
		ShapePartId,
		&Mesh.Vertices[0],
		0,
		VertexInfo.count);

	return Mesh;
}
bool
FHoudiniSkeletalMeshTranslator::SetSkeletalMeshImportDataInfluences(
	FSkeletalMeshImportData& SkeletalMeshImportData,
	const FHoudiniInfluences& Influences,
	const FHoudiniPackageParams& PackageParams)
{
	for (int32 PointIndex = 0; PointIndex < Influences.NumVertices; PointIndex++)
	{
		for (int Influence = 0; Influence < Influences.NumInfluences; Influence++)
		{
			const FHoudiniSkinInfluence& SkinInfluence = Influences.Influences[PointIndex * Influences.NumInfluences + Influence];

			SkeletalMeshImportData::FRawBoneInfluence UnrealInfluence;
			UnrealInfluence.VertexIndex = PointIndex;
			UnrealInfluence.BoneIndex = SkinInfluence.Bone ? SkinInfluence.Bone->UnrealBoneNumber : 0;
			UnrealInfluence.Weight = SkinInfluence.Weight;
			SkeletalMeshImportData.Influences.Add(UnrealInfluence);
		}
	}
	return true;
}



bool
FHoudiniSkeletalMeshTranslator::SetSkeletalMeshImportDataMesh(
	FSkeletalMeshImportData& SkeletalMeshImportData,
	const FHoudiniSkeletalMesh& Mesh,
	const FHoudiniPackageParams& PackageParams)
{

	SkeletalMeshImportData.Points.SetNum(Mesh.Positions.Num());
	int32 Index = 0;
	for (FVector3f Point : Mesh.Positions)
	{
		//flip x and z
		SkeletalMeshImportData.Points[Index] = FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector3f(Point);
		SkeletalMeshImportData.PointToRawMap.Add(Index);
		Index++;
	}

	bool bUseComputedNormals = Mesh.Normals.IsEmpty();

	bool bColorInfoExists = !Mesh.Colors.IsEmpty();

	//-----------------------------------------------------------------------------------
	// Materials
	//-----------------------------------------------------------------------------------

	TArray<int32> PerFaceUEMaterialIds;
	TArray<UMaterialInterface*> UniqueMaterials;
	if (!FHoudiniSkeletalMeshTranslator::LoadOrCreateMaterials(
		Mesh.Materials,
		PackageParams,
		PerFaceUEMaterialIds,
		SkeletalMeshImportData))
	{
		// Unable to retrieve materials, should we use default Houdini one?
		HOUDINI_LOG_ERROR(TEXT("Creating Skeletal Mesh : unable to load/create materials"));
	}

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
			if (!Mesh.AttribInfoUVSets.IsValidIndex(TexCoordIndex))
				continue;
			if (!Mesh.AttribInfoUVSets[TexCoordIndex].exists)
				continue;
			++NumTexCoords;
		}
		SkeletalMeshImportData.NumTexCoords = NumTexCoords;
	}

	SkeletalMeshImportData::FTriangle Triangle;
	for (int VertexInstanceIndex = 0; VertexInstanceIndex < Mesh.Vertices.Num(); ++VertexInstanceIndex)
	{
		int VertexIndex = Mesh.Vertices[VertexInstanceIndex];
		SkeletalMeshImportData::FVertex Wedge;
		Wedge.VertexIndex = VertexIndex;

		for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
		{
			if (!Mesh.AttribInfoUVSets.IsValidIndex(TexCoordIndex))
				continue;
			if (!Mesh.AttribInfoUVSets[TexCoordIndex].exists)
				continue;

			int UVIndex = 0;
			switch (Mesh.AttribInfoUVSets[TexCoordIndex].owner)
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

			int UVTupleSize = Mesh.AttribInfoUVSets[TexCoordIndex].tupleSize;
			const TArray<float>& UVData = Mesh.UVSets[TexCoordIndex];
			// ERROR: This keeps going out of bounds. Why are we getting point UVs ? Should be vertex?!
			Wedge.UVs[TexCoordIndex] = FVector2f(UVData[UVIndex * UVTupleSize], 1.0f - UVData[UVIndex * UVTupleSize + 1]);

			if (bColorInfoExists)
			{

				int ColorIndex = (Mesh.ColorInfo.owner == HAPI_ATTROWNER_VERTEX ? VertexInstanceIndex : VertexIndex) * Mesh.ColorInfo.tupleSize;
				Wedge.Color = FLinearColor(Mesh.Colors[ColorIndex], Mesh.Colors[ColorIndex + 1], Mesh.Colors[ColorIndex + 2]).ToFColor(false);
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
			FVector3f n = Mesh.Normals[count];
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

	SkeletalMeshImportData.bHasVertexColors = Mesh.ColorInfo.exists;

	SkeletalMeshImportData.bHasNormals = true;
	SkeletalMeshImportData.bHasTangents = false;

	return true;
}


bool
FHoudiniSkeletalMeshTranslator::SetSkeletalMeshImportDataSkeleton(
	FSkeletalMeshImportData& SkeletalMeshImportData,
	const FHoudiniSkeleton& Skeleton,
	const FHoudiniPackageParams& InPackageParams)
{
	if (Skeleton.Bones.IsEmpty())
		return false;

	SkeletalMeshImportData.RefBonesBinary.SetNum(Skeleton.Bones.Num());
	int32 BoneIndex = 0;
	for (int JointIndex = 0; JointIndex < Skeleton.Bones.Num(); JointIndex++)
	{
		const FHoudiniSkeletonBone& Bone = Skeleton.Bones[JointIndex];
		SkeletalMeshImportData::FBone NewBone;
		NewBone.Name = Bone.Name;
		NewBone.Flags = 0;
		NewBone.ParentIndex = Bone.Parent ? Bone.Parent->UnrealBoneNumber : -1;
		NewBone.NumChildren = Bone.Children.Num();

		SkeletalMeshImportData::FJointPos JointPos;
		JointPos.Transform = FTransform3f(FTransform(Bone.UnrealLocalMatrix));

		NewBone.BonePos = JointPos;

		if (SkeletalMeshImportData.RefBonesBinary.IsValidIndex(BoneIndex))
			SkeletalMeshImportData.RefBonesBinary[BoneIndex] = NewBone;

		BoneIndex++;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
	SkeletalMeshImportData.bDiffPose = false;
	SkeletalMeshImportData.bUseT0AsRefPose = false;
#endif

	return true;
}

bool
FHoudiniSkeletalMeshTranslator::CreateSkeletalMeshImportData(
	FSkeletalMeshImportData & SkeletalMeshImportData, 
	const FHoudiniSkeletalMesh & Mesh,
	const FHoudiniSkeleton & Skeleton,
	const FHoudiniInfluences & SkinWeights,
	const FHoudiniPackageParams& PackageParams)
{
	bool bSuccess = true;
	bSuccess &= SetSkeletalMeshImportDataMesh(SkeletalMeshImportData, Mesh, PackageParams);
	bSuccess &= SetSkeletalMeshImportDataSkeleton(SkeletalMeshImportData, Skeleton, PackageParams);
	bSuccess &= SetSkeletalMeshImportDataInfluences(SkeletalMeshImportData, SkinWeights, PackageParams);
	return bSuccess;
}

//Creates SkelatalMesh and Skeleton Assets and Packages, and adds them to OutputObjects
bool FHoudiniSkeletalMeshTranslator::ProcessSkeletalMeshParts()
{
	// If we have a mesh but no skeleton, bail. Its always required.
	if (!SKParts.HasSkeleton())
	{
		HOUDINI_LOG_ERROR(TEXT("No skeleton / pose found for skeletal mesh"));
		return false;
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	// if unreal_skeleton attribute is present, load the skeletal mesh asset.
	//----------------------------------------------------------------------------------------------------------------------------------------

	USkeleton* SkeletonAsset = nullptr;
	FHoudiniSkeleton UnrealSkeleton;
	bool bUseExistingSkeleton = false;

	// Look for unreal_skeleton attribute on the Shape packed prim (instancer) level, then on the mesh HGPO level.
	bool bFoundUnrealSkeletonPath = false;
	int SkeletonPathGeoId = INDEX_NONE;
	int SkeletonPathPartId = INDEX_NONE;
	bFoundUnrealSkeletonPath = FindAttributeOnSkeletalMeshShapeParts(SKParts, HAPI_UNREAL_ATTRIB_SKELETON, SkeletonPathGeoId, SkeletonPathPartId);

	if (bFoundUnrealSkeletonPath)
	{
		FString UnrealSkeletonPath;

		FHoudiniHapiAccessor Accessor(SkeletonPathGeoId, SkeletonPathPartId, HAPI_UNREAL_ATTRIB_SKELETON);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, UnrealSkeletonPath);
		if (!UnrealSkeletonPath.IsEmpty())
		{
			SkeletonAsset = LoadObject<USkeleton>(nullptr, *UnrealSkeletonPath);
			// If the unreal_skeleton path was valid, UnrealSkeleton would now point to our desired skeleton asset.
			if (SkeletonAsset)
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(SkeletonAsset);
				bUseExistingSkeleton = true;
				UnrealSkeleton = FHoudiniSkeletalMeshUtils::UnrealToHoudiniSkeleton(SkeletonAsset);
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Could not find Skeleton asset at path '%s'. A new temp skeleton will be created."), *UnrealSkeletonPath);
				return false;
			}
		}
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Create the skeleton, if it exists.
	//----------------------------------------------------------------------------------------------------------------------------------------

	if (SKParts.HGPOPoseMesh == nullptr)
	{
		HOUDINI_LOG_ERROR(TEXT("No pose mesh found, cannot import skeletal mesh"));
		return false;
	}

	FHoudiniSkeleton SkeletonFromHoudini = FHoudiniSkeletalMeshUtils::FetchSkeleton(SKParts.HGPOPoseMesh->GeoId, SKParts.HGPOPoseMesh->PartId);
	if (SkeletonFromHoudini.Bones.Num() == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("No skeleton found on skeletal mesh export."));
		return false;
	}
	// If we don't have a skeleton asset yet, create one now.
	if (!SkeletonAsset)
	{
		const FHoudiniGeoPartObject& PoseInstancerHGPO = *SKParts.HGPOPoseInstancer;
		FHoudiniOutputObjectIdentifier SkeletonIdentifier(PoseInstancerHGPO.ObjectId, PoseInstancerHGPO.GeoId, PoseInstancerHGPO.PartId, "");

		SkeletonAsset = CreateNewSkeleton(SkeletonIdentifier.SplitIdentifier);
		if (!SkeletonAsset)
			return false;

		// Create the output object
		SkeletonIdentifier.PartName = PoseInstancerHGPO.PartName;
		SkeletonIdentifier.PointIndex = 0;
		SkeletonIdentifier.PrimitiveIndex = 0;

		FHoudiniOutputObject& SkeletonOutputObject = OutputObjects.FindOrAdd(SkeletonIdentifier);
		SkeletonOutputObject.OutputObject = SkeletonAsset;
		SkeletonOutputObject.bProxyIsCurrent = false;

		FHoudiniSkeletalMeshUtils::AddBonesToUnrealSkeleton(SkeletonAsset, &SkeletonFromHoudini);
	}

	// At this point, if we do not have a skinned mesh, bail. We're only being asked to create a skeleton.
	if (!SKParts.HasRestShape())
		return true;

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Create a USkeletalMesh
	//----------------------------------------------------------------------------------------------------------------------------------------

	const FHoudiniGeoPartObject * ShapeInstanceGPO = SKParts.GetShapeInstancer();
	const FHoudiniGeoPartObject& ShapeMeshHGPO = *SKParts.HGPOShapeMesh;

	FHoudiniOutputObjectIdentifier ShapeIdentifier(ShapeInstanceGPO->ObjectId, ShapeInstanceGPO->GeoId, ShapeInstanceGPO->PartId, "");
	ShapeIdentifier.PartName = ShapeInstanceGPO->PartName;
	ShapeIdentifier.PointIndex = 0;
	ShapeIdentifier.PrimitiveIndex = 0;

	FHoudiniOutputObject& OutputObject = OutputObjects.FindOrAdd(ShapeIdentifier);

	// Get non-generic supported attributes from OutputObjectIdentifier
	OutputObject.CachedAttributes.Empty();
	OutputObject.CachedTokens.Empty();
	FHoudiniMeshTranslator::CopyAttributesFromHGPOForSplit(ShapeMeshHGPO, ShapeIdentifier.PointIndex, ShapeIdentifier.PrimitiveIndex, OutputObject.CachedAttributes, OutputObject.CachedTokens);
	

	USkeletalMesh* SkeletalMeshAsset = CreateNewSkeletalMesh(ShapeIdentifier.SplitIdentifier);
	OutputObject.OutputObject = SkeletalMeshAsset;
	OutputObject.bProxyIsCurrent = false;

	// This ensures that the render data gets built before we return, by calling PostEditChange when we fall out of scope.
	FScopedSkeletalMeshPostEditChange ScopedPostEditChange( SkeletalMeshAsset );
	if (SkeletalMeshAsset)
	{
		SkeletalMeshAsset->PreEditChange( nullptr );
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Get the data from Houdini
	//----------------------------------------------------------------------------------------------------------------------------------------

	const bool bImportNormals = true;
	HAPI_NodeId ShapeGeoId = SKParts.HGPOShapeMesh->GeoId;
	HAPI_NodeId ShapePartId = SKParts.HGPOShapeMesh->PartId;
	FHoudiniSkeletalMesh Mesh = GetSkeletalMeshMeshData(ShapeGeoId, ShapePartId, bImportNormals);
	FHoudiniInfluences Influences = FHoudiniSkeletalMeshUtils::FetchInfluences(ShapeGeoId, ShapePartId, SkeletonFromHoudini);
	if (bUseExistingSkeleton)
	{
		FHoudiniSkeletalMeshUtils::RemapInfluences(Influences, UnrealSkeleton);
	}

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Fill out the build settings, then build
	//----------------------------------------------------------------------------------------------------------------------------------------

	FHoudiniSkeletalMeshBuildSettings SkeletalMeshBuildSettings;
	SkeletalMeshBuildSettings.ImportNormals = bImportNormals;
	SkeletalMeshBuildSettings.SKMesh = SkeletalMeshAsset;
	SkeletalMeshBuildSettings.Skeleton = SkeletonAsset;
	SkeletalMeshBuildSettings.ImportScale = GetSkeletonImportScale(*SKParts.HGPOShapeMesh);

	bool bSuccess = CreateSkeletalMeshImportData(SkeletalMeshBuildSettings.SkeletalMeshImportData, Mesh, SkeletonFromHoudini, Influences, SkinnedMeshPackageParams);
	if (!bSuccess)
		return false;

	FHoudiniSkeletalMeshTranslator::CreateUnrealData(SkeletalMeshBuildSettings);

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Physics Asset
	//----------------------------------------------------------------------------------------------------------------------------------------

	if (UnrealSkeleton.BoneMap.IsEmpty())
	{
		USkeleton * Skeleton = SkeletalMeshAsset->GetSkeleton();
		UnrealSkeleton = FHoudiniSkeletalMeshUtils::UnrealToHoudiniSkeleton(Skeleton);
	}

	UPhysicsAsset* PhysicsAsset = GetExistingPhysicsAssetFromParts();
	bool bCreateDefaultPhysicsAsset = GetCreateDefaultPhysicsAssetAttributeSet();

	if (PhysicsAsset)
	{
		PhysicsAsset->PreviewSkeletalMesh = SkeletalMeshAsset;
		SkeletalMeshAsset->SetPhysicsAsset(PhysicsAsset);
	}
	else if (SKParts.HGPOPhysAssetInstancer && SKParts.HGPOPhysAssetMesh)
	{
		const FHoudiniGeoPartObject* PhysAssetInstancerHGPO = SKParts.HGPOPhysAssetInstancer;

		FHoudiniOutputObjectIdentifier PhysAssetIdentifier = FHoudiniOutputObjectIdentifier(PhysAssetInstancerHGPO->ObjectId, PhysAssetInstancerHGPO->GeoId, PhysAssetInstancerHGPO->PartId, "");

		PhysicsAsset = CreateNewPhysAsset(PhysAssetIdentifier.SplitIdentifier);
		PhysicsAsset->PreviewSkeletalMesh = SkeletalMeshAsset;

		SetPhysicsAssetFromHGPO(PhysicsAsset, UnrealSkeleton, *SKParts.HGPOPhysAssetMesh);

		SkeletalMeshAsset->SetPhysicsAsset(PhysicsAsset);

		FHoudiniOutputObject& SkeletonOutputObject = OutputObjects.FindOrAdd(PhysAssetIdentifier);
		SkeletonOutputObject.OutputObject = PhysicsAsset;
		SkeletonOutputObject.bProxyIsCurrent = false;

	}
	else if (bCreateDefaultPhysicsAsset)
	{
		FHoudiniOutputObjectIdentifier PhysAssetIdentifier;

		const FHoudiniGeoPartObject* PhysAssetInstancerHGPO = SKParts.HGPOPhysAssetInstancer;
		if (!PhysAssetInstancerHGPO)
		{
			PhysAssetInstancerHGPO = SKParts.HGPOShapeMesh;
		}

		if (PhysAssetInstancerHGPO)
			PhysAssetIdentifier = FHoudiniOutputObjectIdentifier(PhysAssetInstancerHGPO->ObjectId, PhysAssetInstancerHGPO->GeoId, PhysAssetInstancerHGPO->PartId, "");

		PhysicsAsset = CreateNewPhysAsset(PhysAssetIdentifier.SplitIdentifier);

		FHoudiniOutputObject& PhysicsAssetOutputObject = OutputObjects.FindOrAdd(PhysAssetIdentifier);
		PhysicsAssetOutputObject.OutputObject = PhysicsAsset;
		PhysicsAssetOutputObject.bProxyIsCurrent = false;

		// Do automatic asset generation.
		FText ErrorMessage;
		const FPhysAssetCreateParams& NewBodyData = GetDefault<UPhysicsAssetGenerationSettings>()->CreateParams;
		bool bSetToMesh = true;
		FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, SkeletalMeshAsset, NewBodyData, ErrorMessage, bSetToMesh);


	}

	return true;
}

void
FHoudiniSkeletalMeshTranslator::SetPhysicsAssetFromHGPO(UPhysicsAsset* PhysicsAsset, const FHoudiniSkeleton& Skeleton, const FHoudiniGeoPartObject & HGPO)
{
	TArray<FString> BoneNames;
	FHoudiniHapiAccessor Accessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_PHYSICS_BONE);

	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, BoneNames);
	if (BoneNames.IsEmpty())
		return;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), HGPO.GeoId, HGPO.PartId, &PartInfo));
	TArray<FString> GroupNames;
	if (!FHoudiniEngineUtils::HapiGetGroupNames(HGPO.GeoId, HGPO.PartId, HAPI_GROUPTYPE_POINT, PartInfo.isInstanced, GroupNames))
	{
		return;
	}

	TArray<float> Points;
	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_ATTRIB_POSITION);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Points);

	for(FString & GroupName : GroupNames)
	{
		TMap<FString, TArray<int>> BoneSplitGroups = ExtractBoneGroup(BoneNames, HGPO, PartInfo, GroupName);
		for (auto& Entry : BoneSplitGroups)
		{
			const FString& BoneName = Entry.Key;
			const TArray<int>& PointIndices = Entry.Value;

			if (BoneName.IsEmpty() || PointIndices.IsEmpty())
				continue;

			// Create or get BodySetup for this joint. Assign a Physical Material, if specified.
			UBodySetup* BodySetup = GetBodySetup(PhysicsAsset, BoneName);

			FHoudiniHapiAccessor PhysicalMaterialAccessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_PHYSICAL_MATERIAL);
			TArray<FString> PhysicMaterials;
			PhysicalMaterialAccessor.GetAttributeData(HAPI_ATTROWNER_POINT, PhysicMaterials, PointIndices[0], 1);
			if (!PhysicMaterials.IsEmpty())
			{
				UPhysicalMaterial* Material = Cast<UPhysicalMaterial>(StaticLoadObject(UPhysicalMaterial::StaticClass(),
					nullptr, *(PhysicMaterials[0]), nullptr, LOAD_NoWarn, nullptr));
				BodySetup->PhysMaterial = Material;
			}

			// Get Points in Unreal space.  These will then be used to construct the appropriate simple primitive.
			TArray<FVector> UnrealPoints = GetPointForPhysicsBone(Skeleton, BoneName, PointIndices, Points);

			if (GroupName.StartsWith(TEXT("collision_geo_simple_box")))
			{
				FHoudiniMeshTranslator::GenerateOrientedBoxAsSimpleCollision(UnrealPoints, BodySetup->AggGeom);
			}
			else if (GroupName.StartsWith(TEXT("collision_geo_simple_sphere")))
			{
				FHoudiniMeshTranslator::GenerateSphereAsSimpleCollision(UnrealPoints, BodySetup->AggGeom);
			}
			else if (GroupName.StartsWith(TEXT("collision_geo_simple_capsule")))
			{
				FHoudiniMeshTranslator::GenerateOrientedSphylAsSimpleCollision(UnrealPoints, BodySetup->AggGeom);
			}
			else if (GroupName.StartsWith(TEXT("collision_geo_simple_kdop")))
			{
				TArray<FVector> Directions = FHoudiniMeshTranslator::GetKdopDirections(GroupName);
				FHoudiniMeshTranslator::GenerateKDopAsSimpleCollision(UnrealPoints, Directions, BodySetup->AggGeom);
			}
		}
	}
}

TMap<FString, TArray<int>>
FHoudiniSkeletalMeshTranslator::ExtractBoneGroup(const TArray<FString>& BoneNames, const FHoudiniGeoPartObject& HGPO, const HAPI_PartInfo& PartInfo, const FString& GroupName)
{
	// This will return an array of point indices for each bone where the point is in the group.

	TMap<FString, TArray<int>> Result;

	TArray<int32> PointGroupMembership;
	bool AllEqual;
	if (!FHoudiniEngineUtils::HapiGetGroupMembership(HGPO.GeoId, PartInfo, HAPI_GROUPTYPE_POINT, GroupName, PointGroupMembership, AllEqual))
		return Result;

	for (int Index = 0; Index < PointGroupMembership.Num(); Index++)
	{
		if (PointGroupMembership[Index] == 0)
			continue;

		const FString& BoneName = BoneNames[Index];
		if (!Result.Contains(BoneName))
		{
			Result.Add(BoneName, {});
		}
		Result[BoneName].Add(Index);
	}


	return Result;
}

UBodySetup* FHoudiniSkeletalMeshTranslator::GetBodySetup(UPhysicsAsset* PhysicsAsset, const FString& BoneName)
{
	// Get or create a new UBodySetup for this bone. Note: FPhysicsAssetUtils::CreateNewBody() will not create
	// a new body setup if it already exists

	const FPhysAssetCreateParams& NewBodyData = GetDefault<UPhysicsAssetGenerationSettings>()->CreateParams;
	int32 BodyId = FPhysicsAssetUtils::CreateNewBody(PhysicsAsset, FName(BoneName), NewBodyData);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	UBodySetup* BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyId].Get());
#else
	UBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyId];
#endif
	return BodySetup;

}

TArray<FVector> FHoudiniSkeletalMeshTranslator::GetPointForPhysicsBone(const FHoudiniSkeleton& Skeleton, const FString& BoneName, const TArray<int> PointIndices, const TArray<float>& Points)
{
	auto Bone = Skeleton.BoneMap.Find(BoneName);

	auto BoneTransform = (*Bone)->UnrealGlobalTransform.Inverse();

	// Convert Houdini Points to Unreal Points.
	TArray<FVector> UnrealPoints;
	UnrealPoints.SetNum(PointIndices.Num());
	for (int Index = 0; Index < PointIndices.Num(); Index++)
	{
		int PointIndex = PointIndices[Index];
		UnrealPoints[Index].X = Points[PointIndex * 3 + 0];
		UnrealPoints[Index].Y = Points[PointIndex * 3 + 2];
		UnrealPoints[Index].Z = Points[PointIndex * 3 + 1];
		UnrealPoints[Index] *= HAPI_UNREAL_SCALE_FACTOR_POSITION;

		UnrealPoints[Index] = BoneTransform.TransformPosition(UnrealPoints[Index]);
	}

	return UnrealPoints;
}

UPhysicsAsset*
FHoudiniSkeletalMeshTranslator::CreateNewPhysAsset(const FString& InSplitIdentifier)
{

	PhysAssetPackageParams.SplitStr = InSplitIdentifier;
	if (PhysAssetPackageParams.ObjectName.IsEmpty())
	{
		PhysAssetPackageParams.ObjectName = FString::Printf(TEXT("%s_%d_%d_%d_%sPhysicsAsset"), 
			*PhysAssetPackageParams.HoudiniAssetName,
			PhysAssetPackageParams.ObjectId, PhysAssetPackageParams.GeoId, PhysAssetPackageParams.PartId, *PhysAssetPackageParams.SplitStr);
	}
	else
	{
		PhysAssetPackageParams.ObjectName += TEXT("Skeleton");
	}

	const FString AssetPath = PhysAssetPackageParams.GetPackagePath();
	const FString PackageName = PhysAssetPackageParams.GetPackageName();

	const FString PackagePath = FPaths::Combine(AssetPath, PackageName);
	const FSoftObjectPath PhysAssetPath(PackagePath);

	UPhysicsAsset* PhysAsset = LoadObject<UPhysicsAsset>(nullptr, *PackagePath, nullptr, LOAD_NoWarn);

	if (IsValid(PhysAsset))
	{
		PhysAsset->PreEditChange(nullptr);
	}
	else
	{
		PhysAsset = PhysAssetPackageParams.CreateObjectAndPackage<UPhysicsAsset>();
		if (PhysAsset)
			FAssetRegistryModule::AssetCreated(PhysAsset);
	}

	return PhysAsset;
}

USkeleton*
FHoudiniSkeletalMeshTranslator::CreateNewSkeleton(const FString& InSplitIdentifier) 
{

	SkeletonPackageParams.SplitStr = InSplitIdentifier;
	if (SkeletonPackageParams.ObjectName.IsEmpty())
		SkeletonPackageParams.ObjectName = FString::Printf(TEXT("%s_%d_%d_%d_%sSkeleton"), *SkeletonPackageParams.HoudiniAssetName, SkeletonPackageParams.ObjectId, SkeletonPackageParams.GeoId, SkeletonPackageParams.PartId, *SkeletonPackageParams.SplitStr);
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

	FAssetRegistryModule::AssetCreated(NewSkeleton);

	return NewSkeleton;
}
USkeletalMesh*
FHoudiniSkeletalMeshTranslator::CreateNewSkeletalMesh(const FString& InSplitIdentifier)
{
	USkeletalMesh* NewSkeletalMesh = SkinnedMeshPackageParams.CreateObjectAndPackage<USkeletalMesh>();
	if (!IsValid(NewSkeletalMesh))
		return nullptr;

	// Notify the asset registry of new asset
	FAssetRegistryModule::AssetCreated(NewSkeletalMesh);

	return NewSkeletalMesh;
}

bool
FHoudiniSkeletalMeshTranslator::IsRestShapeInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, FString& OutBaseName, HAPI_PartId & MeshPartId)
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

	MeshPartId = InstancedPartIds[0];

	if (!IsRestShapeMesh(GeoId, MeshPartId))
	{
		return false;
	}

	return true;
}

bool
FHoudiniSkeletalMeshTranslator::IsRestShapeMesh(HAPI_NodeId GeoId, HAPI_PartId PartId)
{
	if (!GetAttrInfo(GeoId, PartId, "boneCapture", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}
	
	return true;
}

bool
FHoudiniSkeletalMeshTranslator::IsPhysAssetMesh(HAPI_NodeId GeoId, HAPI_PartId PartId)
{
	if (!GetAttrInfo(GeoId, PartId, HAPI_UNREAL_ATTRIB_PHYSICS_BONE, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}

	return true;
}

bool
FHoudiniSkeletalMeshTranslator::IsCapturePoseInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, FString& OutBaseName, HAPI_PartId & PosePartId)
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

	PosePartId = InstancedPartIds[0];

	if (!IsCapturePoseMesh(GeoId, PosePartId))
	{
		return false;
	}

	return true;
}

bool
FHoudiniSkeletalMeshTranslator::IsPhysAssetInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, FString& OutBaseName, HAPI_PartId& PhysAssetPartId)
{

	// Capture Pose packed prim name must end with '.skel'
	TArray<FString> NameData;

	FHoudiniHapiAccessor Accessor(GeoId, PartId, "name");
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, NameData);

	if (!bSuccess || NameData.Num() == 0)
	{
		return false;
	}
	if (!NameData[0].EndsWith(".phys"))
	{
		return false;
	}

	// Extract the base name that we can use to identify this capture pose and pair it with its respective rest geometry.
	FString Path, Filename, Extension;
	FPaths::Split(NameData[0], Path, OutBaseName, Extension);

	// Check for attributes inside this packed prim:
	// point attributes: transform, name

	// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
	const int NumInstancedParts = 1;
	TArray<HAPI_PartId> InstancedPartIds;
	InstancedPartIds.SetNumZeroed(NumInstancedParts);
	if (FHoudiniApi::GetInstancedPartIds(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId,
		InstancedPartIds.GetData(),
		0, NumInstancedParts) != HAPI_RESULT_SUCCESS)
	{
		return false;
	}

	PhysAssetPartId = InstancedPartIds[0];

	if (!IsPhysAssetMesh(GeoId, PhysAssetPartId))
	{
		return false;
	}

	return true;
}



bool
FHoudiniSkeletalMeshTranslator::IsCapturePoseMesh(HAPI_NodeId GeoId, HAPI_PartId PartId)
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
FHoudiniSkeletalMeshTranslator::GetAttrInfo(HAPI_NodeId GeoId, HAPI_NodeId PartId,
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
FHoudiniSkeletalMeshTranslator::ProcessSkeletalMeshOutputs(
	UHoudiniOutput* InOutput,
	const FHoudiniPackageParams& InPackageParams,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
	UObject* InOuterComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniSkeletalMeshTranslator::ProcessSkeletalMeshOutputs);

	if (!IsValid(InOutput))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OldOutputObjects = InOutput->GetOutputObjects();
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& AssignementMaterials = InOutput->GetAssignementMaterials();
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& ReplacementMaterials = InOutput->GetReplacementMaterials();

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
		else if (CurHGPO.Type == EHoudiniPartType::SkeletalMeshPhysAsset)
		{
			if (CurHGPO.bIsInstanced)
				SKParts.HGPOPhysAssetMesh = &CurHGPO;
			else
				SKParts.HGPOPhysAssetInstancer = &CurHGPO;
		}
	}

	// Iterate on all the output's HGPO, creating meshes as we go
	for (const FHoudiniGeoPartObject& HGPO : InOutput->HoudiniGeoPartObjects)
	{
		// Not a skeletal mesh geo, skip
		if (!(HGPO.Type == EHoudiniPartType::SkeletalMeshShape && HGPO.bIsInstanced == false))
			continue;

		// See if we have some uproperty attributes to update on 
		// the outer component (in most case, the HAC)
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(HGPO.GeoId, HGPO.PartId, true, 0, 0, 0, PropertyAttributes))
		{
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(InOuterComponent, PropertyAttributes);
		}
	}

	if (!ProcessSkeletalMeshParts(
		SKParts,
		InPackageParams,
		InOuterComponent,
		NewOutputObjects,
		AssignementMaterials,
		ReplacementMaterials,
		InAllOutputMaterials))
	{
		return false;
	}

	for (auto& Material : AssignementMaterials)
	{
		// Adds the newly generated materials to the output materials array
		// This is to avoid recreating those same materials again
		if (!InAllOutputMaterials.Contains(Material.Key))
			InAllOutputMaterials.Add(Material);
	}

	return FHoudiniMeshTranslator::CreateOrUpdateAllComponents(
		InOutput,
		InOuterComponent,
		NewOutputObjects);
}



bool
FHoudiniSkeletalMeshTranslator::ProcessSkeletalMeshParts(
	const FHoudiniSkeletalMeshParts& SKParts,
	const FHoudiniPackageParams& InPackageParams,
	UObject* InOuterComponent,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& AssignmentMaterialMap,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& ReplacementMaterialMap,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials)
{
	FHoudiniSkeletalMeshTranslator SKMeshTranslator;
	SKMeshTranslator.SKParts = SKParts;
	SKMeshTranslator.OutputObjects = OutOutputObjects;

	if (SKParts.HGPOShapeInstancer)
	{
		SKMeshTranslator.SkinnedMeshPackageParams = InPackageParams;
		SKMeshTranslator.SkinnedMeshPackageParams.ObjectId = SKParts.HGPOShapeInstancer->ObjectId;
		SKMeshTranslator.SkinnedMeshPackageParams.GeoId = SKParts.HGPOShapeInstancer->GeoId;
		SKMeshTranslator.SkinnedMeshPackageParams.PartId = SKParts.HGPOShapeInstancer->PartId;
	}

	if (SKParts.HGPOPoseInstancer)
	{
		SKMeshTranslator.SkeletonPackageParams = InPackageParams;
		SKMeshTranslator.SkeletonPackageParams.ObjectId = SKParts.HGPOPoseInstancer->ObjectId;
		SKMeshTranslator.SkeletonPackageParams.GeoId = SKParts.HGPOPoseInstancer->GeoId;
		SKMeshTranslator.SkeletonPackageParams.PartId = SKParts.HGPOPoseInstancer->PartId;
	}


	if (SKParts.HGPOPhysAssetInstancer)
	{
		SKMeshTranslator.PhysAssetPackageParams = InPackageParams;
		SKMeshTranslator.PhysAssetPackageParams.ObjectId = SKParts.HGPOPhysAssetInstancer->ObjectId;
		SKMeshTranslator.PhysAssetPackageParams.GeoId = SKParts.HGPOPhysAssetInstancer->GeoId;
		SKMeshTranslator.PhysAssetPackageParams.PartId = SKParts.HGPOPhysAssetInstancer->PartId;
	}
	else if (SKParts.HGPOShapeInstancer)
	{
		SKMeshTranslator.PhysAssetPackageParams = SKMeshTranslator.SkinnedMeshPackageParams;
	}

	SKMeshTranslator.OuterComponent = InOuterComponent;

	SKMeshTranslator.InputAssignmentMaterials = AssignmentMaterialMap;
	SKMeshTranslator.ReplacementMaterials = AssignmentMaterialMap;
	SKMeshTranslator.AllOutputMaterials = InAllOutputMaterials;
	if (SKMeshTranslator.ProcessSkeletalMeshParts())
	{
		// Copy the output objects/materials
		OutOutputObjects = SKMeshTranslator.OutputObjects;
		AssignmentMaterialMap = SKMeshTranslator.OutputAssignmentMaterials;

		return true;
	}
	
	return false;
}



FHoudiniSkeletalMeshMaterialSettings
FHoudiniSkeletalMeshTranslator::GetMaterials(HAPI_NodeId GeoId, HAPI_PartId PartId, int NumFaces)
{
	// Get material information from unreal_material.
	FHoudiniSkeletalMeshMaterialSettings MaterialSettings = FHoudiniSkeletalMeshUtils::GetMaterialOverrides(GeoId, PartId);

	// If no unreal material, try to use houdini materials.
	if (MaterialSettings.Materials.IsEmpty())
	{
		MaterialSettings = FHoudiniSkeletalMeshUtils::GetHoudiniMaterials(GeoId, PartId, NumFaces);

		if (!MaterialSettings.Materials.IsEmpty())
		{
			MaterialSettings.bHoudiniMaterials = true;
		}
	}

	// If there are no materials, create one empty one, which will be assigned the default material.
	if (MaterialSettings.Materials.IsEmpty())
	{
		MaterialSettings.MaterialIds.SetNumZeroed(NumFaces);
		MaterialSettings.Materials.SetNum(1);

	}

	return MaterialSettings;
}

bool
FHoudiniSkeletalMeshTranslator::LoadOrCreateMaterials(
	FHoudiniSkeletalMeshMaterialSettings MaterialSettings,
	const FHoudiniPackageParams& InPackageParams,
	TArray<int32>& OutPerFaceUEMaterialIds,
	FSkeletalMeshImportData& OutImportData)
{
	if (MaterialSettings.bHoudiniMaterials)
	{
		MaterialSettings.bHoudiniMaterials = true;
		bool bCreatedMaterials = FHoudiniSkeletalMeshUtils::CreateHoudiniMaterial(MaterialSettings,
			InputAssignmentMaterials, OutputAssignmentMaterials, AllOutputMaterials, InPackageParams);
		if (!bCreatedMaterials)
			return false;
	}

	for (auto& Material : MaterialSettings.Materials)
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
	OutPerFaceUEMaterialIds = MoveTemp(MaterialSettings.MaterialIds);

	return true;
}

TOptional<bool>  FHoudiniSkeletalMeshTranslator::GetCreateDefaultPhysicsAssetAttributeSet(const FHoudiniGeoPartObject * GeoPart)
{
	TOptional<bool> Result;
	Result.Reset();

	if (!GeoPart)
	{
		return Result;
	}

	int Value = 0;
	FHoudiniHapiAccessor Accessor(GeoPart->GeoId, GeoPart->PartId, HAPI_UNREAL_ATTRIB_CREATE_DEFAULT_PHYSICS_ASSET);
	if (Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, Value))
		Result = Value != 0;
	return Result;
}

bool FHoudiniSkeletalMeshTranslator::GetCreateDefaultPhysicsAssetAttributeSet()
{
	TArray<const FHoudiniGeoPartObject*> PartsToCheck;
	PartsToCheck.Add(SKParts.HGPOPhysAssetInstancer);
	PartsToCheck.Add(SKParts.HGPOShapeMesh);
	PartsToCheck.Add(SKParts.HGPOPoseInstancer);
	PartsToCheck.Add(SKParts.HGPOPoseMesh);
	PartsToCheck.Add(SKParts.HGPOPhysAssetInstancer);
	PartsToCheck.Add(SKParts.HGPOPhysAssetMesh);

	for(auto & PartToCheck : PartsToCheck)
	{
		TOptional<bool> bShouldCreate = GetCreateDefaultPhysicsAssetAttributeSet(PartToCheck);
		if (bShouldCreate.IsSet())
			return bShouldCreate.Get(true);
	}
	return true;
}

FString FHoudiniSkeletalMeshTranslator::GetPhysicAssetRef(const FHoudiniGeoPartObject* GeoPart)
{
	FString Result;

	if (!GeoPart)
		return Result;

	FHoudiniHapiAccessor Accessor(GeoPart->GeoId, GeoPart->PartId, HAPI_UNREAL_ATTRIB_PHYSICS_ASSET);
	Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, Result);
	return Result;
}

UPhysicsAsset * FHoudiniSkeletalMeshTranslator::GetExistingPhysicsAssetFromParts()
{
	FString RefPath;
	RefPath = GetPhysicAssetRef(SKParts.HGPOPhysAssetInstancer);
	if (RefPath.IsEmpty())
		RefPath = GetPhysicAssetRef(SKParts.HGPOShapeMesh);
	if (RefPath.IsEmpty())
		RefPath = GetPhysicAssetRef(SKParts.HGPOPoseInstancer);
	if (RefPath.IsEmpty())
		RefPath = GetPhysicAssetRef(SKParts.HGPOPoseMesh);
	if (RefPath.IsEmpty())
		RefPath = GetPhysicAssetRef(SKParts.HGPOPhysAssetInstancer);
	if (RefPath.IsEmpty())
		RefPath = GetPhysicAssetRef(SKParts.HGPOPhysAssetMesh);

	UPhysicsAsset * PhysicsAsset = Cast<UPhysicsAsset>(StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *RefPath, nullptr, LOAD_NoWarn, nullptr));

	return PhysicsAsset;
}



#undef LOCTEXT_NAMESPACE