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

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMeshSocket;

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
			class UStaticMeshComponent* StaticMeshComponent = nullptr,
			const bool& ExportAllLODs = false,
			const bool& ExportSockets = false,
			const bool& ExportColliders = false);

		// Convert the Mesh using FStaticMeshLODResources
		static bool CreateInputNodeForStaticMeshLODResources(
			const HAPI_NodeId& NodeId,
			const FStaticMeshLODResources& LODResources,
			const int32& LODIndex,
			const bool&	DoExportLODs,
			UStaticMesh* StaticMesh,
			UStaticMeshComponent* StaticMeshComponent);

		// Convert the Mesh using FMeshDescription
		static bool CreateInputNodeForMeshDescription(
			const HAPI_NodeId& NodeId,
			const FMeshDescription& MeshDescription,
			const int32& LODIndex,
			const bool&	DoExportLODs,
			UStaticMesh* StaticMesh,
			UStaticMeshComponent* StaticMeshComponent);

		// Convert the Mesh using FRawMesh

		static bool CreateInputNodeForRawMesh(
			const HAPI_NodeId& NodeId,
			const FStaticMeshSourceModel& SourceModel,
			const int32& LODIndex,
			const bool&	DoExportLODs,
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
			TArray<FString>& OutStaticMeshFaceMaterials);

		// Helper function to extract the array of material names used by a given mesh
		// Also extracts all scalar/vector/texture parameter in the materials 
		// This is used for marshalling static mesh's materials.
		// Memory allocated by this function needs to be cleared by DeleteFaceMaterialArray()
		// The texture parameter array also needs to be cleared.
		static void CreateFaceMaterialArray(
			const TArray<UMaterialInterface *>& Materials,
			const TArray<int32>& FaceMaterialIndices,
			TArray<FString> & OutStaticMeshFaceMaterials,
			TMap<FString, TArray<float>>& OutScalarMaterialParameters,
			TMap<FString, TArray<float>>& OutVectorMaterialParameters,
			TMap<FString, TArray<FString>>& OutTextureMaterialParameters);

		// Delete helper array of material names.
		// Clean up the memory allocated by CreateFaceMaterialArray()
		//static void DeleteFaceMaterialArray(TArray<char *> & OutStaticMeshFaceMaterials);

		// Create and set mesh material attribute and material (scalar, vector and texture) parameters attributes
		static bool CreateHoudiniMeshAttributes(
			const int32& NodeId,
			const int32& PartId,
			const int32& Count,
			const TArray<FString> & TriangleMaterials,
			const TMap<FString, TArray<float>>& ScalarMaterialParameters,
			const TMap<FString, TArray<float>>& VectorMaterialParameters,
			const TMap<FString, TArray<FString>>& TextureMaterialParameters);

		/*
		// Creates the unreal_level_path attribute on the input mesh
		static bool AddLevelPathAttributeToMesh(
			const HAPI_NodeId& NodeId,
			const HAPI_PartId& PartId,
			const FString& LevelPath,
			const int32& Count);
			*/

	private:

};