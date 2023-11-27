#pragma once

#include "HAPI/HAPI_Common.h"
//#include "HoudiniEngineRuntimeUtils.h"

//#include "CoreMinimal.h"
//#include "Components/MeshComponent.h"

//#include "Engine/EngineTypes.h"
//#include "Misc/Optional.h"
//#include "UObject/ObjectMacros.h"
//#include "PhysicalMaterials/PhysicalMaterial.h"

//class UStaticMesh;
//class UAnimSequence;
//class UStaticMeshComponent;
//class UStaticMeshSocket;

class USkeletalMesh;
class USkeletalMeshComponent;
class USkeletalMeshSocket;

//class UMaterialInterface;
class FUnrealObjectInputHandle;
//class FHoudiniEngineIndexedStringMap;
//struct FStaticMeshSourceModel;
//struct FStaticMeshLODResources;
//struct FMeshDescription;
//struct FKConvexElem;

struct HOUDINIENGINE_API FUnrealSkeletalMeshTranslator
{
	public:

		// HAPI : Marshaling, extract geometry and skeletons and create input nodes for it - return true on success
		static bool HapiCreateInputNodeForSkeletalMesh(
			USkeletalMesh* Mesh,
			HAPI_NodeId& InputObjectNodeId,
			const FString& InputNodeName,
			FUnrealObjectInputHandle& OutHandle,
			class USkeletalMeshComponent* SkeletalMeshComponent = nullptr,
			const bool& ExportAllLODs = false,
			const bool& ExportSockets = false,
			const bool& ExportColliders = false,
			const bool& ExportMainMesh = true,
			const bool& bInputNodesCanBeDeleted = true,
			const bool& bExportMaterialParameters = false);

		// Actually exports the skeletal mesh data (mesh, skeleton ... ) using LOD's SourceModel to the newly created input node - returns true on success
		static bool SetSkeletalMeshDataOnNodeFromSourceModel(
			USkeletalMesh* SkeletalMesh,
			USkeletalMeshComponent* SkeletalMeshComponent,
			HAPI_NodeId& NewNodeId,
			int32 LODIndex,
			const bool& bAddLODGroup,
			const bool bInExportMaterialParametersAsAttributes);

		static bool CreateSkeletalMeshBoneCaptureAttributes(
			HAPI_NodeId InNodeId,
			USkeletalMesh const* InSkeletalMesh,
			const HAPI_PartInfo& PartInfo,
			const TArray<int32>& BoneCaptureIndexArray,
			const TArray<float>& BoneCaptureDataArray,
			const TArray<int32>& SizesBoneCaptureIndexArray);

		// Actually exports the skeletal mesh data (mesh, skeleton ... ) using the LOD's import data MeshDescription to the newly created input node - returns true on success
		static bool SetSkeletalMeshDataOnNodeFromMeshDescription(
			USkeletalMesh const* SkeletalMesh,
			USkeletalMeshComponent const* SkeletalMeshComponent,
			const HAPI_NodeId& NewNodeId,
			int32 LODIndex,
			bool bAddLODGroup,
			bool bInExportMaterialParametersAsAttributes);

		static bool CreateInputNodeForSkeletalMeshSockets(
			USkeletalMesh* InSkeletalMesh,
			const HAPI_NodeId& InParentNodeId,
			HAPI_NodeId& OutSocketsNodeId);
};