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
#include "Engine/SkeletalMesh.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class USkeletalMeshSocket;
class FUnrealObjectInputHandle;

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

		// Create nodes for the mesh data only: mesh, LODs, colliders, sockets.
		static bool CreateInputNodesForSkeletalMesh(
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

		static bool CreateInputNodeForCapturePose(
			USkeletalMesh* InSkeletalMesh,
			const HAPI_NodeId InParentNodeId,
			const FString& InInputNodeName,
			HAPI_NodeId& InOutSkeletonNodeId,
			FUnrealObjectInputHandle& OutHandle,
			const bool bInputNodesCanBeDeleted=true);

};