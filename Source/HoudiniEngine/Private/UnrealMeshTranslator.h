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
#include "HoudiniEngineRuntimeUtils.h"

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Misc/Optional.h"
#include "UObject/ObjectMacros.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

class UStaticMesh;
class UAnimSequence;
class UStaticMeshComponent;
class UStaticMeshSocket;

class UMaterialInterface;
class FUnrealObjectInputHandle;
class FHoudiniEngineIndexedStringMap;
class FStaticMeshConstAttributes;
struct FStaticMeshSourceModel;
struct FStaticMeshLODResources;
struct FMeshDescription;
struct FKConvexElem;

struct HOUDINIENGINE_API FUnrealMeshTranslator
{
	public:

		// HAPI : Marshaling, extract geometry and create input asset for it - return true on success
		static bool HapiCreateInputNodeForStaticMesh(
			UStaticMesh * Mesh,
			HAPI_NodeId& InputObjectNodeId,
			const FString& InputNodeName,
			FUnrealObjectInputHandle& OutHandle,
			class UStaticMeshComponent* StaticMeshComponent = nullptr,
			const bool& ExportAllLODs = false,
			const bool& ExportSockets = false,
			const bool& ExportColliders = false,
			const bool& ExportMainMesh = true,
			const bool& bInputNodesCanBeDeleted = true,
			const bool& bPreferNaniteFallbackMesh = false,
			const bool& bExportMaterialParameters = false,
			const bool& bForceReferenceInputNodeCreation = false);

		// Convert the Mesh using FStaticMeshLODResources
		static bool CreateInputNodeForStaticMeshLODResources(
			const HAPI_NodeId& NodeId,
			const FStaticMeshLODResources& LODResources,
			const int32& LODIndex,
			const bool&	DoExportLODs,
			bool bInExportMaterialParametersAsAttributes,
			UStaticMesh* StaticMesh,
			UStaticMeshComponent* StaticMeshComponent);

		// Helper for converting mesh assets using FMeshDescription
		static bool CreateAndPopulateMeshPartFromMeshDescription(
			const HAPI_NodeId& NodeId,
			const FMeshDescription& MeshDescription,
			const FStaticMeshConstAttributes& MeshDescriptionAttributes,
			int32 InLODIndex,
			bool bAddLODGroups,
			bool bInExportMaterialParametersAsAttributes,
			UObject const* Mesh,
			UMeshComponent const* MeshComponent,
			const TArray<UMaterialInterface*>& MeshMaterials,
			const TArray<uint16>& SectionMaterialIndices,
			const FVector3f& BuildScaleVector,
			const FString& PhysicalMaterialPath,
			bool bExportVertexColors,
			TOptional<int32> LightMapResolution,
			TOptional<float> LODScreenSize,
			TOptional<FMeshNaniteSettings> NaniteSettings,
			UAssetImportData const* ImportData,
			bool bCommitGeo,
			HAPI_PartInfo& OutPartInfo);

		// Convert the Mesh using FMeshDescription
		static bool CreateInputNodeForMeshDescription(
			const HAPI_NodeId& NodeId,
			const FMeshDescription& MeshDescription,
			int32 InLODIndex,
			bool bAddLODGroups,
			bool bInExportMaterialParametersAsAttributes,
			UStaticMesh const* StaticMesh,
			UStaticMeshComponent const* StaticMeshComponent);

		// Convert the Mesh using FRawMesh
		static bool CreateInputNodeForRawMesh(
			const HAPI_NodeId& NodeId,
			const FStaticMeshSourceModel& SourceModel,
			const int32& LODIndex,
			const bool&	DoExportLODs,
			bool bInExportMaterialParametersAsAttributes,
			UStaticMesh* StaticMesh,
			UStaticMeshComponent* StaticMeshComponent);

		static bool CreateInputNodeForBox(
			HAPI_NodeId& OutBoxNodeId,
			const HAPI_NodeId& InParentNodeID,
			const int32& ColliderIndex,
			const FVector& BoxCenter,
			const FVector& BoxExtent,
			const FRotator& BoxRotation);

		static bool CreateInputNodeForSphere(
			HAPI_NodeId& OutSphereNodeId,
			const HAPI_NodeId& InParentNodeID,
			const int32& ColliderIndex,
			const FVector& SphereCenter,
			const float& SphereRadius);

		static bool CreateInputNodeForSphyl(
			HAPI_NodeId& OutNodeId,
			const HAPI_NodeId& InParentNodeID,
			const int32& ColliderIndex,
			const FVector& SphylCenter,
			const FRotator& SphylRotation,
			const float& SphylRadius,
			const float& SphereLength);

		static bool CreateInputNodeForConvex(
			HAPI_NodeId& OutNodeId,
			const HAPI_NodeId& InParentNodeID,
			const int32& ColliderIndex,
			const FKConvexElem& ConvexCollider);

		static bool CreateInputNodeForCollider(
			HAPI_NodeId& OutNodeId,
			const HAPI_NodeId& InParentNodeID,
			const int32& ColliderIndex,
			const FString& ColliderName,
			const TArray<float>& ColliderVertices,
			const TArray<int32>& ColliderIndices);

		static bool CreateInputNodeForMeshSockets(
			const TArray<UStaticMeshSocket*>& InMeshSocket,
			const HAPI_NodeId& InParentNodeId,
			HAPI_NodeId& OutSocketsNodeId);

		// Helper function to extract the array of material names used by a given mesh
		// This is used for marshalling static mesh's materials.
		// Memory allocated by this function needs to be cleared by DeleteFaceMaterialArray()
		static void CreateFaceMaterialArray(
			const TArray<UMaterialInterface* >& Materials,
			const TArray<int32>& FaceMaterialIndices,
			FHoudiniEngineIndexedStringMap& OutStaticMeshFaceMaterials);

		// Helper function to extract the array of material names used by a given mesh
		// Also extracts all scalar/vector/texture parameter in the materials 
		// This is used for marshalling static mesh's materials.
		// Memory allocated by this function needs to be cleared by DeleteFaceMaterialArray()
		// The texture parameter array also needs to be cleared.
		static void CreateFaceMaterialArray(
			const TArray<UMaterialInterface *>& Materials,
			const TArray<int32>& FaceMaterialIndices,
			FHoudiniEngineIndexedStringMap& OutStaticMeshFaceMaterials,
			TMap<FString, TArray<float>>& OutScalarMaterialParameters,
			TMap<FString, TArray<float>>& OutVectorMaterialParameters,
			TMap<FString, FHoudiniEngineIndexedStringMap>& OutTextureMaterialParameters,
			TMap<FString, TArray<int8>>& OutBoolMaterialParameters);

		// Create and set mesh material attribute and material (scalar, vector and texture) parameters attributes
		static bool CreateHoudiniMeshAttributes(
		    const int32& NodeId,
		    const int32& PartId,
		    const int32& Count,
		    const FHoudiniEngineIndexedStringMap& TriangleMaterials,
		    const TMap<FString, TArray<float>>& ScalarMaterialParameters,
		    const TMap<FString, TArray<float>>& VectorMaterialParameters,
			const TMap<FString, FHoudiniEngineIndexedStringMap>& TextureMaterialParameters,
			const TMap<FString, TArray<int8>>& BoolMaterialParameters,
		    const TOptional<FString> PhysicalMaterial = TOptional<FString>(),
			const TOptional<FMeshNaniteSettings> InNaniteSettings = TOptional<FMeshNaniteSettings>());

		// Gets the simple physical Material path for the mesh component overrides or,
		// if not set, from the body setup
		static FString GetSimplePhysicalMaterialPath(UMeshComponent const* MeshComponent, UBodySetup const* BodySetup);
};
