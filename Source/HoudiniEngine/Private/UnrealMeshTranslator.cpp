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

#include "UnrealMeshTranslator.h"

#include "HoudiniDataLayerUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineTimers.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniMeshUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"

#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMeshBuilder.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "MeshDescriptionOperations.h"
#include "MeshUtilities.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "SkeletalMeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"


#include "HoudiniEngineAttributes.h"
#include "UnrealObjectInputManager.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "Engine/SkinnedAssetCommon.h"
	#include "MaterialDomain.h"
	#include "StaticMeshComponentLODInfo.h"	
#endif

#if WITH_EDITOR
	#include "EditorFramework/AssetImportData.h"
#endif

bool FUnrealMeshTranslator::bUseNewMeshPath = true;

const FString FUnrealMeshTranslator::LODPrefix = TEXT("lod");
const FString FUnrealMeshTranslator::HiResMeshName = TEXT("hires");
const FString FUnrealMeshTranslator::MTLParams = TEXT("mtl_params");
const FString FUnrealMeshTranslator::CombinePrefix = TEXT("combined_");
const FString FUnrealMeshTranslator::MaterialTableName = TEXT("material_table");

bool
FUnrealMeshTranslator::CreateInputNodeForStaticMesh(
	HAPI_NodeId& InputNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const UStaticMesh* StaticMesh,
	const UStaticMeshComponent* StaticMeshComponent,
	const FString& InputNodeName,
	const FUnrealMeshExportOptions& ExportOptions,
	const bool bInputNodesCanBeDeleted,
	const bool bForceReferenceInputNodeCreation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh);

	if(bUseNewMeshPath)
	{
		bool bSuccess = CreateInputNodeForStaticMeshNew(InputNodeId, OutHandle, StaticMesh, StaticMeshComponent, InputNodeName, ExportOptions, bInputNodesCanBeDeleted);
		return bSuccess;
	}

	// If we don't have a static mesh there's nothing to do.
	if (!IsValid(StaticMesh))
		return false;

	const USplineMeshComponent* SplineMeshComponent = nullptr;
	bool bIsSplineMesh = false;
	if (IsValid(StaticMeshComponent))
	{
		SplineMeshComponent = Cast<USplineMeshComponent>(StaticMeshComponent);
		bIsSplineMesh = IsValid(SplineMeshComponent);
	}

	// Only set bMainMeshIsNaniteFallback to true if this is a Nanite mesh and we are sending the fallback
	// For non-Nanite meshes bMainMeshIsNaniteFallback should always be false
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	const bool bNaniteBuildEnabled = StaticMesh->IsNaniteEnabled();
#else
	const bool bNaniteBuildEnabled = StaticMesh->NaniteSettings.bEnabled;
#endif
	const bool ShouldUseNaniteFallback = ExportOptions.bPreferNaniteFallbackMesh && StaticMesh->GetRenderData()->LODResources.Num();
	const bool bMainMeshIsNaniteFallback = bNaniteBuildEnabled && ShouldUseNaniteFallback && !bIsSplineMesh && (ExportOptions.bMainMesh || ExportOptions.bLODs);

	// Input node name, default to InputNodeName, but can be changed by the new input system
	FString FinalInputNodeName = InputNodeName;
	
	// Find the node in new input system
	// Identifier will be the identifier for the entry created in this call of the function. We may call this function
	// recursively to create the main mesh, LODs, sockets and colliders, each getting its own identifier.
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;
	const UObject* InputSystemObject = bIsSplineMesh ? static_cast<const UObject*>(SplineMeshComponent) : static_cast<const UObject*>(StaticMesh);
	{
		// Check if we already have an input node for this asset
		bool bSingleLeafNodeOnly = false;
		FUnrealObjectInputIdentifier ReferenceNodeIdentifier;
		TArray<FUnrealObjectInputIdentifier> IdentPerOption;

		if (!FUnrealObjectInputUtils::BuildMeshInputObjectIdentifiers(
			InputSystemObject,
			ExportOptions,
			bMainMeshIsNaniteFallback,
			ExportOptions.bMaterialParameters,
			bForceReferenceInputNodeCreation,
			bSingleLeafNodeOnly,
			ReferenceNodeIdentifier,
			IdentPerOption))
		{
			return false;
		}

		if (bSingleLeafNodeOnly)
		{
			// We'll create the static mesh input node entirely is this function call
			check(!IdentPerOption.IsEmpty());
			Identifier = IdentPerOption[0];
		}
		else
		{
			// Look for the reference node that references the per-option (LODs, sockets, colliders) nodes
			Identifier = ReferenceNodeIdentifier;
		}
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId) && (bSingleLeafNodeOnly || FUnrealObjectInputUtils::AreReferencedHAPINodesValid(Handle)))
			{
				if (!bInputNodesCanBeDeleted)
				{
					// Make sure to prevent deletion of the input node if needed
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);
				}

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// We now need to create the nodes (since we couldn't find existing ones in the manager)
		// For the single leaf node case we can simply continue this function
		// For the ref + multiple options, we call this function again for each option (as a single leaf node) and
		// then create the reference node.
		if (!bSingleLeafNodeOnly)
		{
			TSet<FUnrealObjectInputHandle> PerOptionNodeHandles;
			PerOptionNodeHandles.Reserve(IdentPerOption.Num());
			for (const FUnrealObjectInputIdentifier& OptionIdentifier : IdentPerOption)
			{
				const FUnrealObjectInputOptions& Options = OptionIdentifier.GetOptions();

				FString NodeLabel;
				FUnrealObjectInputUtils::GetDefaultInputNodeName(OptionIdentifier, NodeLabel);

				HAPI_NodeId NewNodeId = -1;
				FUnrealObjectInputHandle OptionHandle;
				if (FUnrealObjectInputUtils::FindNodeViaManager(OptionIdentifier, OptionHandle))
				{
					FUnrealObjectInputUtils::GetHAPINodeId(OptionHandle, NewNodeId);
				}

				FUnrealMeshExportOptions InputExportOptions;
				InputExportOptions.bLODs = Options.bExportLODs;
				InputExportOptions.bSockets = Options.bExportSockets;
				InputExportOptions.bColliders = Options.bExportColliders;
				InputExportOptions.bMainMesh = !Options.bExportLODs && !Options.bExportSockets && !Options.bExportColliders;
				InputExportOptions.bMaterialParameters = Options.bExportMaterialParameters;
				InputExportOptions.bPreferNaniteFallbackMesh = Options.bMainMeshIsNaniteFallbackMesh;

				static constexpr bool bForceInputRefNodeCreation = false;
				if (!CreateInputNodeForStaticMesh(
						NewNodeId,
						OptionHandle,
						StaticMesh,
						StaticMeshComponent,
						NodeLabel,
						InputExportOptions,
						bInputNodesCanBeDeleted,
						bForceInputRefNodeCreation))
				{
					return false;
				}

				PerOptionNodeHandles.Add(OptionHandle);
			}

			// Create or update the HAPI node for the reference node if it does not exist
			FUnrealObjectInputHandle RefNodeHandle;
			if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(ReferenceNodeIdentifier, PerOptionNodeHandles, RefNodeHandle, true, bInputNodesCanBeDeleted))
				return false;
			
			OutHandle = RefNodeHandle;
			FUnrealObjectInputUtils::GetHAPINodeId(ReferenceNodeIdentifier, InputNodeId);
			return true;
		}

		// Set InputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that InputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, InputNodeId))
				InputNodeId = -1;
		}
		else
		{
			InputNodeId = -1;
		}
	}

	// Delete previous node first
	// This will avoid naming collisions due to the previous node already existing?
	// 
	HAPI_NodeId PreviousInputNodeId = InputNodeId;
	if (PreviousInputNodeId >= 0)
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *FinalInputNodeName);
		}
	}


	// Node ID for the newly created node
	HAPI_NodeId NewNodeId = -1;

	// Export sockets if there are some
	bool DoExportSockets = ExportOptions.bSockets && (StaticMesh->Sockets.Num() > 0);

	// Export LODs if there are some
	bool DoExportLODs = ExportOptions.bLODs && (StaticMesh->GetNumLODs() > 1);

	// Export colliders if there are some
	bool DoExportColliders = ExportOptions.bColliders && StaticMesh->GetBodySetup() != nullptr;
	if (DoExportColliders)
	{
		if (StaticMesh->GetBodySetup()->AggGeom.GetElementCount() <= 0)
		{
			DoExportColliders = false;
		}
	}

	// We need to use a merge node if we export lods OR sockets
	bool UseMergeNode = DoExportLODs || DoExportSockets || DoExportColliders;
	if (UseMergeNode)
	{
		// TODO:
		// What if OutInputNodeId already exists? 
		// Delete previous merge?/input?

		// Create a merge SOP asset. This will be our "InputNodeId"
		// as all the different LOD meshes and sockets will be plugged into it
		if (ParentNodeId < 0)
		{
			HOUDINI_CHECK_ERROR_RETURN(	FHoudiniEngineUtils::CreateNode(
				-1, TEXT("SOP/merge"), FinalInputNodeName, true, &NewNodeId), false);
		}
		else
		{
			// When creating a node inside a parent node (in other words, ParentNodeId is not -1), then we cannot
			// specify the node type category prefix on the node name. We have to create the geo Object and merge
			// SOPs separately.
			HAPI_NodeId ObjectNodeId = -1; 
			HOUDINI_CHECK_ERROR_RETURN(	FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("geo"), FinalInputNodeName, true, &ObjectNodeId), false);

			HOUDINI_CHECK_ERROR_RETURN(	FHoudiniEngineUtils::CreateNode(ObjectNodeId, TEXT("merge"), FinalInputNodeName, true, &NewNodeId), false);
		}
	}
	else
	{
		// No LODs/Sockets, we just need a single input node
		// If InputNodeId is invalid, we need to create an input node.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(FinalInputNodeName, NewNodeId, ParentNodeId), false);

		if (!FHoudiniEngineUtils::HapiCookNode(NewNodeId, nullptr, true))
			return false;
	}

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;


	// Update our input NodeId
	InputNodeId = NewNodeId;
	// Get our parent OBJ NodeID
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);

	// TODO:
	// Setting for lightmap resolution?	

	// Next Index used to connect nodes to the merge
	int32 NextMergeIndex = 0;

	// Should we export the HiRes Nanite Mesh?
	const bool bHaveHiResSourceModel = StaticMesh->IsHiResMeshDescriptionValid();
	bool bHiResMeshSuccess = false;
	const bool bWantToExportHiResModel = bNaniteBuildEnabled && ExportOptions.bMainMesh && !ShouldUseNaniteFallback && !bIsSplineMesh;
	if (bWantToExportHiResModel && bHaveHiResSourceModel)
	{
		// Get the HiRes Mesh description and SourceModel
		FMeshDescription HiResMeshDescription = *StaticMesh->GetHiResMeshDescription();

		const FStaticMeshSourceModel& HiResSrcModel = StaticMesh->GetHiResSourceModel();
		const FMeshBuildSettings& HiResBuildSettings = HiResSrcModel.BuildSettings;		// cannot be const because FMeshDescriptionHelper modifies the LightmapIndex fields ?!?

		// If we're using a merge node, we need to create a new input null
		HAPI_NodeId CurrentNodeId = -1;
		if (UseMergeNode)
		{
			// Create a new input node for the HiRes Mesh in this input object's OBJ node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("null"), TEXT("HiRes"), false, &CurrentNodeId), false);
		}
		else
		{
			// No merge node, just use the input node we created before
			CurrentNodeId = NewNodeId;
		}

		// Convert the Mesh using FMeshDescription
		const double StartTime = FPlatformTime::Seconds();
		bHiResMeshSuccess = FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
			CurrentNodeId,
			HiResMeshDescription,
			-1,
			false,
			ExportOptions.bMaterialParameters,
			StaticMesh,
			StaticMeshComponent);

		HOUDINI_LOG_MESSAGE(TEXT("FUnrealMeshTranslator::CreateInputNodeForMeshDescription HiRes mesh completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);

		if (UseMergeNode)
		{
			// Connect the HiRes mesh node to the merge node if needed
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, CurrentNodeId, 0), false);
		}

		NextMergeIndex++;
	}

	// Determine which LODs to export based on the ExportLODs/bMainMesh, high res mesh availability and whether
	// the new input system is being used.
	const int32 NumLODs = StaticMesh->GetNumLODs();
	int32 FirstLODIndex = 0;
	int32 LastLODIndex = -1;
	{
		if (DoExportLODs)
		{
			// With the new system we export LODs and the main mesh in separate steps. We only want to export LOD0 with
			// the LODs if this is a Nanite mesh
			if (bHaveHiResSourceModel && bNaniteBuildEnabled)
			{
				LastLODIndex = NumLODs - 1;
				FirstLODIndex = 0;
			}
			else
			{
				// Don't export LOD0 with the LODs if this is not a nanite mesh, since we have a separate "main mesh"
				// input
				LastLODIndex = NumLODs - 1;
				FirstLODIndex = 1;
			}
		}
		else if (ExportOptions.bMainMesh)
		{
			if (bHiResMeshSuccess)
			{
				LastLODIndex = -1;
				FirstLODIndex = 0;
			}
			else
			{
				// Without nanite, the main mesh/high res mesh is LOD0
				LastLODIndex = 0;
				FirstLODIndex = 0;
			}
		}
		else
		{
			LastLODIndex = -1;
			FirstLODIndex = 0;
		}
	}

	if (LastLODIndex >= 0)
	{
		for (int32 LODIndex = FirstLODIndex; LODIndex <= LastLODIndex; LODIndex++)
		{
			// Grab the LOD level.
			const FStaticMeshSourceModel & SrcModel = StaticMesh->GetSourceModel(LODIndex);

			// If we're using a merge node, we need to create a new input null
			HAPI_NodeId CurrentLODNodeId = -1;
			if (UseMergeNode)
			{
				// Create a new input node for the current LOD

				FString LODName = TEXT("lod") + FString::FromInt(LODIndex);

				// Create the node in this input object's OBJ node
				HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("null"), LODName, false, &CurrentLODNodeId), false);
			}
			else
			{
				// No merge node, just use the input node we created before
				CurrentLODNodeId = NewNodeId;
			}

			// Export the current LOD Mesh by using MeshDescription 
			FMeshDescription* MeshDesc = nullptr;
			FMeshDescription SplineMeshDesc;

			// This will either fetch the mesh description that is cached on the SrcModel
			// or load it from bulk data / DDC once
			if (!bIsSplineMesh)
			{
				if (SrcModel.GetCachedMeshDescription() != nullptr)
				{
					MeshDesc = SrcModel.GetCachedMeshDescription();
				}
				else
				{
					const double StartTime = FPlatformTime::Seconds();
					MeshDesc = StaticMesh->GetMeshDescription(LODIndex);
					HOUDINI_LOG_MESSAGE(TEXT("StaticMesh->GetMeshDescription completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);
				}
			}
			else
			{
				// Deform mesh data according to the Spline Mesh Component's data
				static constexpr bool bPropagateVertexColours = false;
				static constexpr bool bApplyComponentTransform = false;
				FHoudiniMeshUtils::RetrieveMesh(SplineMeshDesc, SplineMeshComponent, LODIndex, bPropagateVertexColours, bApplyComponentTransform);
				MeshDesc = &SplineMeshDesc;
			}

			// Should we use Mesh Description? Depends on Nanite settings, if we have a valid Mesh Description and
			// if exporting LODs. (Recently discovered that Mesh Description can be null for LODs)

			bool bUseMeshDescription = (!bNaniteBuildEnabled || !ShouldUseNaniteFallback);

			if(DoExportLODs)
				bUseMeshDescription = false;
			else if(!MeshDesc)
				bUseMeshDescription = false;

			bool bMeshSuccess = false;
			if (bUseMeshDescription)
			{
				// Convert the Mesh using FMeshDescription
				const double StartTime = FPlatformTime::Seconds();
				bMeshSuccess = FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
					CurrentLODNodeId,
					*MeshDesc,
					LODIndex,
					DoExportLODs,
					ExportOptions.bMaterialParameters,
					StaticMesh,
					StaticMeshComponent);
				HOUDINI_LOG_MESSAGE(TEXT("FUnrealMeshTranslator::CreateInputNodeForMeshDescription completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);
			}
			else
			{
				// Convert the LOD Mesh using FStaticMeshLODResources
				const double StartTime = FPlatformTime::Seconds();
				bMeshSuccess = FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources(
					CurrentLODNodeId,
					StaticMesh->GetLODForExport(LODIndex),
					LODIndex,
					DoExportLODs,
					ExportOptions.bMaterialParameters,
					StaticMesh,
					StaticMeshComponent);
				HOUDINI_LOG_MESSAGE(TEXT("FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);
			}

			if (!bMeshSuccess)
				continue;

			if (UseMergeNode)
			{
				// Connect the LOD node to the merge node.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
					NewNodeId, NextMergeIndex, CurrentLODNodeId, 0), false);
			}

			NextMergeIndex++;
		}
	}

	if (DoExportColliders && StaticMesh->GetBodySetup() != nullptr)
	{
		ExportCollisions(NextMergeIndex, StaticMesh, NewNodeId, InputObjectNodeId, StaticMesh->GetBodySetup()->AggGeom);
	}

	if (DoExportSockets && StaticMesh->Sockets.Num() > 0)
    {
		// Create an input node for the mesh sockets
		HAPI_NodeId SocketsNodeId = -1;
		if (CreateInputNodeForMeshSockets(StaticMesh->Sockets, InputObjectNodeId, SocketsNodeId))
		{
			// We can connect the socket node to the merge node's last input.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(), NewNodeId, NextMergeIndex, SocketsNodeId, 0), false);

			NextMergeIndex++;
		}
		else if (SocketsNodeId != -1)
		{
			// If we failed to properly export the sockets, clean up the created node
			FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SocketsNodeId);
		}
	}

	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}
	
	//
	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForMeshSockets(
	const TArray<UStaticMeshSocket*>& InMeshSocket, 
	const HAPI_NodeId InParentNodeId, 
	HAPI_NodeId& OutSocketsNodeId)
{
	// Create a new input node for the sockets
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
		InParentNodeId, TEXT("null"), "sockets", false, &OutSocketsNodeId), false);

	int32 NumSockets = InMeshSocket.Num();
	if (NumSockets <= 0)
		return true;

	// Create part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.pointCount = NumSockets;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, &Part), false);

	// Create POS point attribute info.
	HAPI_AttributeInfo AttributeInfoPos;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPos);
	AttributeInfoPos.count = NumSockets;
	AttributeInfoPos.tupleSize = 3;
	AttributeInfoPos.exists = true;
	AttributeInfoPos.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPos.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPos.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPos), false);

	// Create Rot point attribute Info
	HAPI_AttributeInfo AttributeInfoRot;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoRot);
	AttributeInfoRot.count = NumSockets;
	AttributeInfoRot.tupleSize = 4;
	AttributeInfoRot.exists = true;
	AttributeInfoRot.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoRot.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoRot.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, &AttributeInfoRot), false);

	// Create scale point attribute Info
	HAPI_AttributeInfo AttributeInfoScale;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoScale);
	AttributeInfoScale.count = NumSockets;
	AttributeInfoScale.tupleSize = 3;
	AttributeInfoScale.exists = true;
	AttributeInfoScale.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE, &AttributeInfoScale), false);

	//  Create the name attrib info
	HAPI_AttributeInfo AttributeInfoName;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoName);
	AttributeInfoName.count = NumSockets;
	AttributeInfoName.tupleSize = 1;
	AttributeInfoName.exists = true;
	AttributeInfoName.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoName.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoName.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, &AttributeInfoName), false);

	//  Create the tag attrib info
	HAPI_AttributeInfo AttributeInfoTag;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoTag);
	AttributeInfoTag.count = NumSockets;
	AttributeInfoTag.tupleSize = 1;
	AttributeInfoTag.exists = true;
	AttributeInfoTag.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoTag.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoTag.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG, &AttributeInfoTag), false);

	// Extract the sockets transform values
	TArray<float> SocketPos;
	SocketPos.SetNumZeroed(NumSockets * 3);
	TArray<float> SocketRot;
	SocketRot.SetNumZeroed(NumSockets * 4);
	TArray<float > SocketScale;
	SocketScale.SetNumZeroed(NumSockets * 3);
	
	TArray<FString> SocketNames;
	TArray<FString> SocketTags;
	for (int32 Idx = 0; Idx < NumSockets; ++Idx)
	{
		UStaticMeshSocket* CurrentSocket = InMeshSocket[Idx];
		if (!IsValid(CurrentSocket))
			continue;

		// Get the socket's transform and convert it to HapiTransform
		FTransform SocketTransform(CurrentSocket->RelativeRotation, CurrentSocket->RelativeLocation, CurrentSocket->RelativeScale);
		HAPI_Transform HapiSocketTransform;
		FHoudiniApi::Transform_Init(&HapiSocketTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(SocketTransform, HapiSocketTransform);

		// Fill the attribute values
		SocketPos[3 * Idx + 0] = HapiSocketTransform.position[0];
		SocketPos[3 * Idx + 1] = HapiSocketTransform.position[1];
		SocketPos[3 * Idx + 2] = HapiSocketTransform.position[2];

		SocketRot[4 * Idx + 0] = HapiSocketTransform.rotationQuaternion[0];
		SocketRot[4 * Idx + 1] = HapiSocketTransform.rotationQuaternion[1];
		SocketRot[4 * Idx + 2] = HapiSocketTransform.rotationQuaternion[2];
		SocketRot[4 * Idx + 3] = HapiSocketTransform.rotationQuaternion[3];

		SocketScale[3 * Idx + 0] = HapiSocketTransform.scale[0];
		SocketScale[3 * Idx + 1] = HapiSocketTransform.scale[1];
		SocketScale[3 * Idx + 2] = HapiSocketTransform.scale[2];

		FString CurrentSocketName;
		if (!CurrentSocket->SocketName.IsNone())
			CurrentSocketName = CurrentSocket->SocketName.ToString();
		else
			CurrentSocketName = TEXT("Socket") + FString::FromInt(Idx);
		SocketNames.Add(CurrentSocketName);

		if (!CurrentSocket->Tag.IsEmpty())
			SocketTags.Add(CurrentSocket->Tag);
		else
			SocketTags.Add("");
	}

	//we can now upload them to our attribute.

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
	Accessor.SetAttributeData(AttributeInfoPos, SocketPos);

	Accessor.Init(OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION);
	Accessor.SetAttributeData(AttributeInfoRot, SocketRot);

	Accessor.Init(OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE);
	Accessor.SetAttributeData(AttributeInfoScale, SocketScale);

	Accessor.Init(OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME);
	Accessor.SetAttributeData(AttributeInfoName, SocketNames);

	Accessor.Init(OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG);
	Accessor.SetAttributeData(AttributeInfoTag, SocketTags);

	// We will also create the socket_details attributes
	for (int32 Idx = 0; Idx < NumSockets; ++Idx)
	{
		// Build the current socket's prefix
		FString SocketAttrPrefix = TEXT(HAPI_UNREAL_ATTRIB_MESH_SOCKET_PREFIX) + FString::FromInt(Idx);

		// Create mesh_socketX_pos attribute info.
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPos);
		AttributeInfoPos.count = 1;
		AttributeInfoPos.tupleSize = 3;
		AttributeInfoPos.exists = true;
		AttributeInfoPos.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoPos.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPos.originalOwner = HAPI_ATTROWNER_INVALID;

		FString PosAttr = SocketAttrPrefix + TEXT("_pos");
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*PosAttr), &AttributeInfoPos), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*PosAttr), &AttributeInfoPos,
			&(SocketPos[3 * Idx]), 0, AttributeInfoPos.count), false);

		// Create mesh_socketX_rot point attribute Info
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoRot);
		AttributeInfoRot.count = 1;
		AttributeInfoRot.tupleSize = 4;
		AttributeInfoRot.exists = true;
		AttributeInfoRot.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoRot.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoRot.originalOwner = HAPI_ATTROWNER_INVALID;

		FString RotAttr = SocketAttrPrefix + TEXT("_rot");
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*RotAttr), &AttributeInfoRot), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*RotAttr), &AttributeInfoRot,
			&(SocketRot[4 * Idx]), 0, AttributeInfoRot.count), false);

		// Create mesh_socketX_scale point attribute Info
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoScale);
		AttributeInfoScale.count = 1;
		AttributeInfoScale.tupleSize = 3;
		AttributeInfoScale.exists = true;
		AttributeInfoScale.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

		FString ScaleAttr = SocketAttrPrefix + TEXT("_scale");
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*ScaleAttr), &AttributeInfoScale), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*ScaleAttr), &AttributeInfoScale,
			&(SocketScale[3 * Idx]), 0, AttributeInfoScale.count), false);

		//  Create the mesh_socketX_name attrib info
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoName);
		AttributeInfoName.count = 1;
		AttributeInfoName.tupleSize = 1;
		AttributeInfoName.exists = true;
		AttributeInfoName.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoName.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoName.originalOwner = HAPI_ATTROWNER_INVALID;

		FString NameAttr = SocketAttrPrefix + TEXT("_name");
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*NameAttr), &AttributeInfoName), false);

		Accessor.Init(OutSocketsNodeId, 0, TCHAR_TO_ANSI(*NameAttr));
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoName, SocketNames[Idx]), false);

		//  Create the mesh_socketX_tag attrib info
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoTag);
		AttributeInfoTag.count = 1;
		AttributeInfoTag.tupleSize = 1;
		AttributeInfoTag.exists = true;
		AttributeInfoTag.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoTag.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoTag.originalOwner = HAPI_ATTROWNER_INVALID;

		FString TagAttr = SocketAttrPrefix + TEXT("_tag");
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*TagAttr), &AttributeInfoTag), false);

		Accessor.Init(OutSocketsNodeId, 0, TCHAR_TO_ANSI(*TagAttr));
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoTag, SocketTags[Idx]), false);
	}

	// Now add the sockets group
	const char * SocketGroupStr = "socket_imported";
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_GROUPTYPE_POINT, SocketGroupStr), false);

	// Set GroupMembership
	TArray<int> GroupArray;
	GroupArray.SetNumUninitialized(NumSockets);
	for (int32 n = 0; n < GroupArray.Num(); n++)
		GroupArray[n] = 1;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_GROUPTYPE_POINT, SocketGroupStr, GroupArray.GetData(), 0, NumSockets), false);

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(OutSocketsNodeId), false);

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources(
	const HAPI_NodeId NodeId,
	const FStaticMeshLODResources& LODResources,
	const int32 InLODIndex,
	const bool bAddLODGroups,
	const bool bInExportMaterialParametersAsAttributes,
	const UStaticMesh* StaticMesh,
	const UStaticMeshComponent* StaticMeshComponent)
{
	bool bDoTiming = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;

	// Convert the Mesh using FStaticMeshLODResources

	// Check that the mesh is not empty
	if (LODResources.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("No vertices in mesh!"));
		return false;
	}

	if (LODResources.Sections.Num() == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("No triangles in mesh!"));
		return false;
	}

	// Vertex instance and triangle counts
	const uint32 OrigNumVertexInstances = LODResources.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
	const uint32 NumTriangles = LODResources.GetNumTriangles();
	const uint32 NumVertexInstances = NumTriangles * 3;
	const uint32 NumSections = LODResources.Sections.Num();

	// Grab the build scale
	const FStaticMeshSourceModel &SourceModel = StaticMesh->GetSourceModel(InLODIndex);
	FVector3f BuildScaleVector = (FVector3f)SourceModel.BuildSettings.BuildScale3D;

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITION (P)
	//--------------------------------------------------------------------------------------------------------------------- 
	// In FStaticMeshLODResources each vertex instances stores its position, even if the positions are not unique (in other
	// words, in Houdini terminology, the number of points and vertices are the same. We'll do the same thing that Epic
	// does in FBX export: we'll run through all vertex instances and use a hash to determine which instances share a 
	// position, so that we can a smaller number of points than vertices, and vertices share point positions

	bool bDoTimings = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;
	FHoudiniPerfTimer PositionsTimer(TEXT("Positions"), bDoTimings);
	PositionsTimer.Start();

	TArray<int32> UEVertexInstanceIdxToPointIdx;
	UEVertexInstanceIdxToPointIdx.Reserve(OrigNumVertexInstances);

	TMap<FVector3f, int32> PositionToPointIndexMap;
	PositionToPointIndexMap.Reserve(OrigNumVertexInstances);

	TArray<float> StaticMeshVertices;
	StaticMeshVertices.Reserve(OrigNumVertexInstances * 3);
	for (uint32 VertexInstanceIndex = 0; VertexInstanceIndex < OrigNumVertexInstances; ++VertexInstanceIndex)
	{
		// Convert Unreal to Houdini
		const FVector3f &PositionVector = LODResources.VertexBuffers.PositionVertexBuffer.VertexPosition(VertexInstanceIndex);
		const int32 *FoundPointIndexPtr = PositionToPointIndexMap.Find(PositionVector);
		if (!FoundPointIndexPtr)
		{
			const int32 NewPointIndex = StaticMeshVertices.Add(PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.X) / 3;
			StaticMeshVertices.Add(PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Z);
			StaticMeshVertices.Add(PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Y);

			PositionToPointIndexMap.Add(PositionVector, NewPointIndex);
			UEVertexInstanceIdxToPointIdx.Add(NewPointIndex);
		}
		else
		{
			UEVertexInstanceIdxToPointIdx.Add(*FoundPointIndexPtr);
		}
	}

	StaticMeshVertices.Shrink();
	const uint32 NumVertices = StaticMeshVertices.Num() / 3;

	// Now that we know how many vertices (points), vertex instances (vertices) and triagnles we have,
	// we can create the part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);

	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = NumVertexInstances;
	Part.faceCount = NumTriangles;
	Part.pointCount = NumVertices;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part), false);
		 
	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPoint );
	AttributeInfoPoint.count = Part.pointCount;
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	// Now that we have raw positions, we can upload them for our attribute.
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, StaticMeshVertices), false);

	PositionsTimer.Stop();


	// Determine which attributes we have
	const bool bIsVertexInstanceNormalsValid = true;
	const bool bIsVertexInstanceTangentsValid = true;
	const bool bIsVertexInstanceBinormalsValid = true;
	const bool bIsVertexInstanceColorsValid = LODResources.bHasColorVertexData;
	const uint32 NumUVLayers = FMath::Min<uint32>(LODResources.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(), MAX_STATIC_TEXCOORDS);
	const bool bIsVertexInstanceUVsValid = NumUVLayers > 0;

	bool bUseComponentOverrideColors = false;
	// Determine if have override colors on the static mesh component, if so prefer to use those
	if (StaticMeshComponent &&
		StaticMeshComponent->LODData.IsValidIndex(InLODIndex) &&
		StaticMeshComponent->LODData[InLODIndex].OverrideVertexColors)
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
		FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

		if (ColorVertexBuffer.GetNumVertices() == LODResources.GetNumVertices())
		{
			bUseComponentOverrideColors = true;
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIAL INDEX -> MATERIAL INTERFACE
	//---------------------------------------------------------------------------------------------------------------------

	FHoudiniPerfTimer MaterialTimer(TEXT("Materials"), bDoTimings);
	MaterialTimer.Start();

	double MaterialsTickTime = FPlatformTime::Seconds();

	TArray<UMaterialInterface*> MaterialInterfaces;
	TArray<int32> TriangleMaterialIndices;

	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

	// If the static mesh component is valid, and we are not using the ref counted input system, get the materials via
	// the component to account for overrides. For the ref counted input system the component will override the
	// materials in its input node.
	const bool bIsStaticMeshComponentValid = (IsValid(StaticMeshComponent) && StaticMeshComponent->IsValidLowLevel());
	const int32 NumStaticMaterials = StaticMaterials.Num();
	// If we find any invalid Material (null or pending kill), or we find a section below with an out of range MaterialIndex,
	// then we will set UEDefaultMaterial at the invalid index
	int32 UEDefaultMaterialIndex = INDEX_NONE;
	UMaterialInterface *UEDefaultMaterial = nullptr;
	if (NumStaticMaterials > 0)
	{
		MaterialInterfaces.Reserve(NumStaticMaterials);
		for (int32 MaterialIndex = 0; MaterialIndex < NumStaticMaterials; ++MaterialIndex)
		{
			const FStaticMaterial &MaterialInfo = StaticMaterials[MaterialIndex];
			UMaterialInterface *Material = nullptr;
			{
				Material = MaterialInfo.MaterialInterface;
			}
			// If the Material is NULL or invalid, fallback to the default material
			if (!IsValid(Material))
			{
				if (!UEDefaultMaterial)
				{
					UEDefaultMaterial = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
					UEDefaultMaterialIndex = MaterialIndex;
				}
				Material = UEDefaultMaterial;
				HOUDINI_LOG_WARNING(TEXT("Material Index %d (slot %s) has an invalid material, falling back to default: %s"), MaterialIndex, *(MaterialInfo.MaterialSlotName.ToString()), *(UEDefaultMaterial->GetPathName()));
			}
			// MaterialSlotToInterface.Add(MaterialInfo.ImportedMaterialSlotName, MaterialIndex);
			MaterialInterfaces.Add(Material);
		}

		TriangleMaterialIndices.Reserve(NumTriangles);
	}

	MaterialTimer.Stop();

	// If we haven't created UEDefaultMaterial yet, check that all the sections' MaterialIndex
	// is valid, if not, create UEDefaultMaterial and add to MaterialInterfaces to get UEDefaultMaterialIndex
	if (!UEDefaultMaterial || UEDefaultMaterialIndex == INDEX_NONE)
	{
		for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			// If the MaterialIndex referenced by this Section is out of range, fill MaterialInterfaces with UEDefaultMaterial
			// up to and including MaterialIndex and log a warning
			const int32 MaterialIndex = LODResources.Sections[SectionIndex].MaterialIndex;
			if (!MaterialInterfaces.IsValidIndex(MaterialIndex))
			{
				if (!UEDefaultMaterial)
				{
					UEDefaultMaterial = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
					// Add the UEDefaultMaterial to MaterialInterfaces
					UEDefaultMaterialIndex = MaterialInterfaces.Add(UEDefaultMaterial);
				}
				HOUDINI_LOG_WARNING(TEXT("Section Index %d references an invalid Material Index %d, falling back to default material: %s"), SectionIndex, MaterialIndex, *(UEDefaultMaterial->GetPathName()));
			}
		}
	}

	// Determine the final number of materials we have, with default for missing/invalid indices
	const int32 NumMaterials = MaterialInterfaces.Num();

	if(bDoTiming)
	{
		HOUDINI_LOG_MESSAGE(TEXT("CreateInputNodeForStaticMeshLODResources() - materials %f secs"), FPlatformTime::Seconds() - MaterialsTickTime);
	}

	FHoudiniPerfTimer MakeUVTimer(TEXT("UV Make"), bDoTimings);
	FHoudiniPerfTimer MakeNormalTimer(TEXT("Normals Make"), bDoTimings);
	FHoudiniPerfTimer MakeTangentTimer(TEXT("Tangents Make"), bDoTimings);
	FHoudiniPerfTimer MakeBinormalTimer(TEXT("Binormals Make"), bDoTimings);
	FHoudiniPerfTimer MakeColorsTimer(TEXT("Colors Make"), bDoTimings);
	FHoudiniPerfTimer MakeAlphasTimer(TEXT("Alphas Make"), bDoTimings);

	FHoudiniPerfTimer TransferUVTimer(TEXT("UV Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferNormalTimer(TEXT("Normals Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferTangentTimer(TEXT("Tangents Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferBinormalTimer(TEXT("Binormals Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferColorsTimer(TEXT("Colors Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferAlphasTimer(TEXT("Alphas Transfer"), bDoTimings);

	// Now we deal with vertex instance attributes. 
	if (NumTriangles > 0)
	{
		// UV layer array. Each layer has an array of floats, 3 floats per vertex instance
		TArray<TArray<float>> UVs;
		// Normals: 3 floats per vertex instance
		TArray<float> Normals;
		// Tangents: 3 floats per vertex instance
		TArray<float> Tangents;
		// Binormals: 3 floats per vertex instance
		TArray<float> Binormals;
		// RGBColors: 3 floats per vertex instance
		TArray<float> RGBColors;
		// Alphas: 1 float per vertex instance
		TArray<float> Alphas;

		// Initialize the arrays for the attributes that are valid
		if (bIsVertexInstanceUVsValid)
		{
			UVs.SetNum(NumUVLayers);
			for (uint32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
			{
				UVs[UVLayerIndex].SetNumUninitialized(NumVertexInstances * 3);
			}
		}

		if (bIsVertexInstanceNormalsValid)
		{
			Normals.SetNumUninitialized(NumVertexInstances * 3);
		}

		if (bIsVertexInstanceTangentsValid)
		{
			Tangents.SetNumUninitialized(NumVertexInstances * 3);
		}

		if (bIsVertexInstanceBinormalsValid)
		{
			Binormals.SetNumUninitialized(NumVertexInstances * 3);
		}

		if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
		{
			RGBColors.SetNumUninitialized(NumVertexInstances * 3);
			Alphas.SetNumUninitialized(NumVertexInstances);
		}

		// Array of vertex (point position) indices per triangle
		TArray<int32> MeshTriangleVertexIndices;
		MeshTriangleVertexIndices.SetNumUninitialized(NumVertexInstances);
		// Array of vertex counts per triangle/face
		TArray<int32> MeshTriangleVertexCounts;
		MeshTriangleVertexCounts.SetNumUninitialized(NumTriangles);

		int32 TriangleIdx = 0;
		int32 HoudiniVertexIdx = 0;
		FIndexArrayView TriangleVertexIndices = LODResources.IndexBuffer.GetArrayView();
		for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			const FStaticMeshSection& Section = LODResources.Sections[SectionIndex];
			for (uint32 SectionTriangleIndex = 0; SectionTriangleIndex < Section.NumTriangles; ++SectionTriangleIndex)
			{
				MeshTriangleVertexCounts[TriangleIdx] = 3;
				for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
				{
					// Reverse the winding order for Houdini (but still start at 0)
					const int32 WindingIdx = (3 - TriangleVertexIndex) % 3;
					const uint32 UEVertexIndex = TriangleVertexIndices[Section.FirstIndex + SectionTriangleIndex * 3 + WindingIdx];

					// Calculate the index of the first component of a vertex instance's value in an inline float array 
					// representing vectors (3 float) per vertex instance
					const int32 Float3Index = HoudiniVertexIdx * 3;

					//--------------------------------------------------------------------------------------------------------------------- 
					// UVS (uvX)
					//--------------------------------------------------------------------------------------------------------------------- 
					if (bIsVertexInstanceUVsValid)
					{
						MakeUVTimer.Start();
						for (uint32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
						{
							const FVector2f &UV = LODResources.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(UEVertexIndex, UVLayerIndex);
							UVs[UVLayerIndex][Float3Index + 0] = UV.X;
							UVs[UVLayerIndex][Float3Index + 1] = 1.0f - UV.Y;
							UVs[UVLayerIndex][Float3Index + 2] = 0;
						}
						MakeUVTimer.Stop();
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// NORMALS (N)
					//---------------------------------------------------------------------------------------------------------------------
					if (bIsVertexInstanceNormalsValid)
					{
						MakeNormalTimer.Start();
						const FVector4f &Normal = LODResources.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(UEVertexIndex);
						Normals[Float3Index + 0] = Normal.X;
						Normals[Float3Index + 1] = Normal.Z;
						Normals[Float3Index + 2] = Normal.Y;
						MakeNormalTimer.Stop();
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// TANGENT (tangentu)
					//---------------------------------------------------------------------------------------------------------------------
					if (bIsVertexInstanceTangentsValid)
					{
						MakeTangentTimer.Start();
						const FVector4f &Tangent = LODResources.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(UEVertexIndex);
						Tangents[Float3Index + 0] = Tangent.X;
						Tangents[Float3Index + 1] = Tangent.Z;
						Tangents[Float3Index + 2] = Tangent.Y;
						MakeTangentTimer.Stop();
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// BINORMAL (tangentv)
					//---------------------------------------------------------------------------------------------------------------------
					// In order to calculate the binormal we also need the tangent and normal
					if (bIsVertexInstanceBinormalsValid)
					{
						MakeBinormalTimer.Start();
						FVector3f Binormal = LODResources.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(UEVertexIndex);
						Binormals[Float3Index + 0] = Binormal.X;
						Binormals[Float3Index + 1] = Binormal.Z;
						Binormals[Float3Index + 2] = Binormal.Y;
						MakeBinormalTimer.Stop();
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// COLORS (Cd)
					//---------------------------------------------------------------------------------------------------------------------
					if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
					{
						MakeColorsTimer.Start();
						FLinearColor Color = FLinearColor::White;
						if (bUseComponentOverrideColors)
						{
							const FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
							FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;
							Color = ColorVertexBuffer.VertexColor(UEVertexIndex).ReinterpretAsLinear();
						}
						else
						{
							Color = LODResources.VertexBuffers.ColorVertexBuffer.VertexColor(UEVertexIndex).ReinterpretAsLinear();
						}
						RGBColors[Float3Index + 0] = Color.R;
						RGBColors[Float3Index + 1] = Color.G;
						RGBColors[Float3Index + 2] = Color.B;
						Alphas[HoudiniVertexIdx] = Color.A;
						MakeColorsTimer.Stop();
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// TRIANGLE/FACE VERTEX INDICES
					//---------------------------------------------------------------------------------------------------------------------
					if (UEVertexInstanceIdxToPointIdx.IsValidIndex(UEVertexIndex))
					{
						MeshTriangleVertexIndices[HoudiniVertexIdx] = UEVertexInstanceIdxToPointIdx[UEVertexIndex];
					}

					HoudiniVertexIdx++;
				}

				//--------------------------------------------------------------------------------------------------------------------- 
				// TRIANGLE MATERIAL ASSIGNMENT
				//---------------------------------------------------------------------------------------------------------------------
				if (MaterialInterfaces.IsValidIndex(Section.MaterialIndex))
				{
					TriangleMaterialIndices.Add(Section.MaterialIndex);
				}
				else
				{
					TriangleMaterialIndices.Add(UEDefaultMaterialIndex);
					HOUDINI_LOG_WARNING(TEXT("Section Index %d references an invalid Material Index %d, falling back to default material: %s"), SectionIndex, Section.MaterialIndex, *(UEDefaultMaterial->GetPathName()));
				}

				TriangleIdx++;
			}
		}

		// Now transfer valid vertex instance attributes to Houdini vertex attributes

		//--------------------------------------------------------------------------------------------------------------------- 
		// UVS (uvX)
		//--------------------------------------------------------------------------------------------------------------------- 
		if (bIsVertexInstanceUVsValid)
		{
			TransferUVTimer.Start();
			for (uint32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; UVLayerIndex++)
			{
				// Construct the attribute name for this UV index.
				FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;
				if (UVLayerIndex > 0)
					UVAttributeName += FString::Printf(TEXT("%d"), UVLayerIndex + 1);

				// Create attribute for UVs
				HAPI_AttributeInfo AttributeInfoVertex;
				FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

				AttributeInfoVertex.count = NumVertexInstances;
				AttributeInfoVertex.tupleSize = 3;
				AttributeInfoVertex.exists = true;
				AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
				AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
				AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
					FHoudiniEngine::Get().GetSession(),
					NodeId, 0, TCHAR_TO_ANSI(*UVAttributeName), &AttributeInfoVertex), false);

				Accessor.Init(NodeId, 0, TCHAR_TO_ANSI(*UVAttributeName));
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, UVs[UVLayerIndex]), false);
			}
			TransferUVTimer.Stop();
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// NORMALS (N)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceNormalsValid)
		{
			TransferNormalTimer.Start();
			// Create attribute for normals.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = Normals.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex), false);

			Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Normals), false);
			TransferNormalTimer.Stop();
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// TANGENT (tangentu)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceTangentsValid)
		{
			TransferTangentTimer.Start();
			// Create attribute for tangentu.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = Tangents.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, &AttributeInfoVertex), false);

			Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Tangents), false);
			TransferTangentTimer.Stop();
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// BINORMAL (tangentv)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceBinormalsValid)
		{
			TransferBinormalTimer.Start();
			// Create attribute for normals.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = Binormals.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, &AttributeInfoVertex), false);

			Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Binormals), false);
			TransferBinormalTimer.Stop();
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// COLORS (Cd)
		//---------------------------------------------------------------------------------------------------------------------
		if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
		{
			TransferColorsTimer.Start();
			// Create attribute for colors.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = RGBColors.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex), false);

			Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, RGBColors), false);

			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);
			AttributeInfoVertex.tupleSize = 1;
			AttributeInfoVertex.count = Alphas.Num();
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, &AttributeInfoVertex), false);

			Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Alphas), false);
			TransferColorsTimer.Stop();
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// TRIANGLE/FACE VERTEX INDICES
		//---------------------------------------------------------------------------------------------------------------------
		// We can now set vertex list.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
			MeshTriangleVertexIndices, NodeId, 0), false);

		// Send the array of face vertex counts.
		TArray<int32> StaticMeshFaceCounts;
		StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < Part.faceCount; n++)
			StaticMeshFaceCounts[n] = 3;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
			StaticMeshFaceCounts, NodeId, 0), false);

		// Send material assignments to Houdini
		if (NumMaterials > 0)
		{
			// List of materials, one for each face.
			FHoudiniEngineIndexedStringMap TriangleMaterials;

			//Lists of material parameters
			TMap<FString, TArray<float>> ScalarMaterialParameters;
			TMap<FString, TArray<float>> VectorMaterialParameters;
            TMap<FString, FHoudiniEngineIndexedStringMap> TextureMaterialParameters;
			TMap<FString, TArray<int8>> BoolMaterialParameters;

			bool bAttributeSuccess = false;
			FString PhysicalMaterialPath = GetSimplePhysicalMaterialPath(StaticMesh->GetBodySetup());

			FHoudiniPerfTimer MaterialFaceArray(TEXT("MaterialFaceArray"), bDoTimings);
			MaterialFaceArray.Start();

			if (bInExportMaterialParametersAsAttributes)
			{
				// Create attributes for the material and all its parameters
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, 
					TriangleMaterialIndices,
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters,
					BoolMaterialParameters);
			}
			else
			{
				// Create attributes only for the materials. Only get the material attribute data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, 
					TriangleMaterialIndices, 
					TriangleMaterials);
			}
			MaterialFaceArray.Stop();

			FHoudiniPerfTimer MeshAttributes(TEXT("Mesh Attributes"), bDoTimings);
			MeshAttributes.Start();

			// Create all the needed attributes for materials
			bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
				NodeId,
				0,
				TriangleMaterials.GetIds().Num(),
				TriangleMaterials,
				TriangleMaterialIndices,
				ScalarMaterialParameters,
				VectorMaterialParameters,
				TextureMaterialParameters,
				BoolMaterialParameters,
				PhysicalMaterialPath,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
				StaticMesh->GetNaniteSettings());
#else
				StaticMesh->NaniteSettings);
#endif

			MeshAttributes.Stop();

			if (!bAttributeSuccess)
			{
				return false;
			}

		}

		// TODO: The render mesh (LODResources) does not have face smoothing information, and the raw mesh triangle order is
		// potentially different (see also line 4152 TODO_FBX in Engine\Source\Editor\UnrealEd\Private\Fbx\FbxMainExport.cpp
		////--------------------------------------------------------------------------------------------------------------------- 
		//// TRIANGLE SMOOTHING MASKS
		////---------------------------------------------------------------------------------------------------------------------	
		//TArray<uint32> TriangleSmoothingMasks;
		//TriangleSmoothingMasks.SetNumZeroed(NumTriangles);
		//FStaticMeshOperations::ConvertHardEdgesToSmoothGroup(MeshDescription, TriangleSmoothingMasks);
		//if (TriangleSmoothingMasks.Num() > 0)
		//{
		//	HAPI_AttributeInfo AttributeInfoSmoothingMasks;
		//	FHoudiniApi::AttributeInfo_Init(&AttributeInfoSmoothingMasks);

		//	AttributeInfoSmoothingMasks.tupleSize = 1;
		//	AttributeInfoSmoothingMasks.count = TriangleSmoothingMasks.Num();
		//	AttributeInfoSmoothingMasks.exists = true;
		//	AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
		//	AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
		//	AttributeInfoSmoothingMasks.originalOwner = HAPI_ATTROWNER_INVALID;

		//	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		//		FHoudiniEngine::Get().GetSession(),
		//		NodeId, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, &AttributeInfoSmoothingMasks), false);

		//	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
		//		FHoudiniEngine::Get().GetSession(),
		//		NodeId, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, &AttributeInfoSmoothingMasks,
		//		(const int32 *)TriangleSmoothingMasks.GetData(), 0, TriangleSmoothingMasks.Num()), false);
		//}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// LIGHTMAP RESOLUTION
	//---------------------------------------------------------------------------------------------------------------------
	{
		int32 LightMapResolution = StaticMesh->GetLightMapResolution();

		HAPI_AttributeInfo AttributeInfoLightMapResolution;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoLightMapResolution);
		AttributeInfoLightMapResolution.count = 1;
		AttributeInfoLightMapResolution.tupleSize = 1;
		AttributeInfoLightMapResolution.exists = true;
		AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, &AttributeInfoLightMapResolution), false);

		Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoLightMapResolution, LightMapResolution), false);

	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT MESH NAME
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = Part.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME, &AttributeInfo), false);

		Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfo, StaticMesh->GetPathName()), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT SOURCE FILE
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		FString Filename;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		if (UAssetImportData* ImportData = StaticMesh->GetAssetImportData())
#else
		if (UAssetImportData* ImportData = StaticMesh->AssetImportData)
#endif
		{
			for (const auto& SourceFile : ImportData->SourceData.SourceFiles)
			{
				Filename = UAssetImportData::ResolveImportFilename(SourceFile.RelativeFilename, ImportData->GetOutermost());
				break;
			}
		}

		if (!Filename.IsEmpty())
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = Part.faceCount;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE, &AttributeInfo), false);


			Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfo, Filename), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// LOD GROUP AND SCREENSIZE
	//---------------------------------------------------------------------------------------------------------------------
	if (bAddLODGroups)
	{
		// LOD Group

		FString LODGroup = TEXT("lod") + FString::FromInt(InLODIndex);

		// Add a LOD group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, H_TCHAR_TO_UTF8(*LODGroup)), false);

		// Set GroupMembership
		TArray<int> GroupArray;
		GroupArray.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < GroupArray.Num(); n++)
			GroupArray[n] = 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, H_TCHAR_TO_UTF8(*LODGroup),
			GroupArray.GetData(), 0, Part.faceCount), false);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		if (!StaticMesh->GetAutoComputeLODScreenSize())
#else
		if (!StaticMesh->bAutoComputeLODScreenSize)
#endif
		{
			// Add the lodX_screensize attribute
			FString LODAttributeName =
				TEXT(HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_PREFIX) + FString::FromInt(InLODIndex) + TEXT(HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_POSTFIX);

			// Create lodX_screensize detail attribute info.
			HAPI_AttributeInfo AttributeInfoLODScreenSize;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoLODScreenSize);
			AttributeInfoLODScreenSize.count = 1;
			AttributeInfoLODScreenSize.tupleSize = 1;
			AttributeInfoLODScreenSize.exists = true;
			AttributeInfoLODScreenSize.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfoLODScreenSize.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoLODScreenSize.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, H_TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize), false);

			float lodscreensize = SourceModel.ScreenSize.Default;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(), NodeId, 0,
				H_TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize,
				&lodscreensize, 0, 1), false);
		}
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), false);

	return true;
}


FString
FUnrealMeshTranslator::GetSimplePhysicalMaterialPath(UBodySetup const* const BodySetup)
{
	if (IsValid(BodySetup) && IsValid(BodySetup->PhysMaterial))
	{
		FString Path = BodySetup->PhysMaterial.GetPath();
		if (Path != "None")
			return Path;
	}

	return FString();
}

bool
FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
	const HAPI_NodeId& NodeId,
	const FMeshDescription& MeshDescription,
	const int32 InLODIndex,
	const bool bAddLODGroups,
	const bool bInExportMaterialParametersAsAttributes,
	const UStaticMesh * StaticMesh,
	const UStaticMeshComponent * StaticMeshComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUnrealMeshTranslator::CreateInputNodeForMeshDescription);

	if (!IsValid(StaticMesh))
		return false;

	// ----------------------------------------------------------------------------------------------------------------
	// Prepare the data we need for exporting the mesh via CreateAndPopulateMeshPartFromMeshDescription
	// ----------------------------------------------------------------------------------------------------------------
	
	// Get the physical material path
	FString PhysicalMaterialPath = GetSimplePhysicalMaterialPath(StaticMesh->GetBodySetup());
	
	// Grab the build scale
	const FStaticMeshSourceModel &SourceModel = InLODIndex > 0 ? StaticMesh->GetSourceModel(InLODIndex) : StaticMesh->GetHiResSourceModel();
	const FVector3f BuildScaleVector = static_cast<FVector3f>(SourceModel.BuildSettings.BuildScale3D);

	// Get the mesh attributes
	FStaticMeshConstAttributes MeshConstAttributes(MeshDescription);
	const int32 NumVertexInstances = MeshDescription.VertexInstances().Num();
	
	FStaticMeshRenderData const* const SMRenderData = StaticMesh->GetRenderData();

	// Determine if have override colors on the static mesh component, if so prefer to use those
	bool bUseComponentOverrideColors = false;
	if (StaticMeshComponent &&
		StaticMeshComponent->LODData.IsValidIndex(InLODIndex) &&
		StaticMeshComponent->LODData[InLODIndex].OverrideVertexColors &&
		SMRenderData &&
		SMRenderData->LODResources.IsValidIndex(InLODIndex))
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
		const FStaticMeshLODResources& RenderModel = SMRenderData->LODResources[InLODIndex];
		const FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

		if (RenderModel.WedgeMap.Num() > 0 && ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
		{
			// Use the wedge map if it is available as it is lossless.
			if (RenderModel.WedgeMap.Num() == NumVertexInstances)
			{
				bUseComponentOverrideColors = true;
			}
		}
	}

	// Build a material interface array (by material index)
	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
	TArray<UMaterialInterface*> Materials;
	Materials.Reserve(StaticMaterials.Num());
	for (const FStaticMaterial& StaticMaterial : StaticMaterials)
	{
		Materials.Add(StaticMaterial.MaterialInterface);
	}

	const int32 NumSections = StaticMesh->GetNumSections(InLODIndex);
	const FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();
	TArray<uint16> SectionMaterialIndices;
	SectionMaterialIndices.Reserve(NumSections);
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		SectionMaterialIndices.Add(SectionInfoMap.Get(InLODIndex, SectionIndex).MaterialIndex);
	}

	TOptional<float> LODScreenSize;
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	const bool bIsLODScreenSizeAutoComputed = StaticMesh->bAutoComputeLODScreenSize;
#else
	const bool bIsLODScreenSizeAutoComputed = StaticMesh->IsLODScreenSizeAutoComputed();
#endif
	if (!bIsLODScreenSizeAutoComputed)
		LODScreenSize = StaticMesh->GetSourceModel(InLODIndex).ScreenSize.Default;

	// ----------------------------------------------------------------------------------------------------------------
	// Export the mesh via CreateAndPopulateMeshPartFromMeshDescription
	// ----------------------------------------------------------------------------------------------------------------

	// If we are using override colors from the component, then don't export vertex colors in the
	// CreateAndPopulateMeshPartFromMeshDescription function call and don't commit the geo: we'll add the override
	// colors afterwards and then commit the geo.
	const bool bExportVertexColors = !bUseComponentOverrideColors;
	const bool bCommitGeo = !bUseComponentOverrideColors;
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	if (!CreateAndPopulateMeshPartFromMeshDescription(
		NodeId, 
		MeshDescription, 
		MeshConstAttributes, 
		InLODIndex, 
		bAddLODGroups, 
		bInExportMaterialParametersAsAttributes,
		StaticMesh, 
		StaticMeshComponent, 
		Materials, 
		SectionMaterialIndices, 
		BuildScaleVector, 
		PhysicalMaterialPath,
		bExportVertexColors, 
		StaticMesh->GetLightMapResolution(), 
		LODScreenSize,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		StaticMesh->GetNaniteSettings(),
#else
		StaticMesh->NaniteSettings,
#endif
		StaticMesh->GetAssetImportData(), 
		bCommitGeo, 
		PartInfo))
	{
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Handle any StaticMesh(Component) specific data that is not handled by CreateAndPopulateMeshPartFromMeshDescription
	// ----------------------------------------------------------------------------------------------------------------
	
	if (bUseComponentOverrideColors)
	{
		// RGBColors: 3 floats per vertex instance
		TArray<float> RGBColors;
		// Alphas: 1 float per vertex instance
		TArray<float> Alphas;

		const FPolygonArray& MDPolygons = MeshDescription.Polygons();
		
		RGBColors.SetNumUninitialized(NumVertexInstances * 3);
		Alphas.SetNumUninitialized(NumVertexInstances);

		{
			H_SCOPED_FUNCTION_STATIC_LABEL("Fetching Vertex Data - SM Specific");

			int32 TriangleIdx = 0;
			int32 VertexInstanceIdx = 0;

			for (const FPolygonID &PolygonID : MDPolygons.GetElementIDs())
			{
				for (const FTriangleID& TriangleID : MeshDescription.GetPolygonTriangles(PolygonID))
				{
					for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
					{
						// Reverse the winding order for Houdini (but still start at 0)
						const int32 WindingIdx = (3 - TriangleVertexIndex) % 3;
						const FVertexInstanceID &VertexInstanceID = MeshDescription.GetTriangleVertexInstance(TriangleID, WindingIdx);

						// Calculate the index of the first component of a vertex instance's value in an inline float array 
						// representing vectors (3 float) per vertex instance
						const int32 Float3Index = VertexInstanceIdx * 3;

						//--------------------------------------------------------------------------------------------------------------------- 
						// COLORS (Cd)
						//---------------------------------------------------------------------------------------------------------------------
						if (bUseComponentOverrideColors && SMRenderData)
						{
							FLinearColor Color = FLinearColor::White;
							const FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
							const FStaticMeshLODResources& RenderModel = SMRenderData->LODResources[InLODIndex];
							const FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

							const int32 Index = RenderModel.WedgeMap[VertexInstanceID];
							if (Index != INDEX_NONE)
								Color = ColorVertexBuffer.VertexColor(Index).ReinterpretAsLinear();

							RGBColors[Float3Index + 0] = Color.R;
							RGBColors[Float3Index + 1] = Color.G;
							RGBColors[Float3Index + 2] = Color.B;
							Alphas[VertexInstanceIdx] = Color.A;
						}

						VertexInstanceIdx++;
					}

					TriangleIdx++;
				}
			}
		}

		{
			H_SCOPED_FUNCTION_STATIC_LABEL("Transfering Data -- SM Specific");

			//--------------------------------------------------------------------------------------------------------------------- 
			// COLORS (Cd)
			//---------------------------------------------------------------------------------------------------------------------
			if (bUseComponentOverrideColors)
			{
				// Create attribute for colors.
				HAPI_AttributeInfo AttributeInfoVertex;
				FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

				AttributeInfoVertex.tupleSize = 3;
				AttributeInfoVertex.count = RGBColors.Num() / AttributeInfoVertex.tupleSize;
				AttributeInfoVertex.exists = true;
				AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
				AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
				AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
					FHoudiniEngine::Get().GetSession(),
					NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex), false);

				FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, RGBColors), false);

				FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);
				AttributeInfoVertex.tupleSize = 1;
				AttributeInfoVertex.count = Alphas.Num();
				AttributeInfoVertex.exists = true;
				AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
				AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
				AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
					FHoudiniEngine::Get().GetSession(),
					NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, &AttributeInfoVertex), false);

				Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Alphas), false);
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------
	// Commit the geo
	// ----------------------------------------------------------------------------------------------------------------
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), false);

	return true;
}


bool
FUnrealMeshTranslator::CreateAndPopulateMeshPartFromMeshDescription(
	const HAPI_NodeId& NodeId,
	const FMeshDescription& MeshDescription,
	const FStaticMeshConstAttributes& MeshDescriptionAttributes,
	const int32 InLODIndex,
	const bool bAddLODGroups,
	const bool bInExportMaterialParametersAsAttributes,
	const UObject * Mesh,
	const UMeshComponent * MeshComponent,
	const TArray<UMaterialInterface*>& MeshMaterials,
	const TArray<uint16>& SectionMaterialIndices,
	const FVector3f& BuildScaleVector,
	const FString& PhysicalMaterialPath,
	const bool bExportVertexColors,
	const TOptional<int32> LightMapResolution,
	const TOptional<float> LODScreenSize,
	const TOptional<FMeshNaniteSettings> NaniteSettings,
	const UAssetImportData * ImportData,
	const bool bCommitGeo,
	HAPI_PartInfo& OutPartInfo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUnrealMeshTranslator::CreateAndPopulateMeshPartFromMeshDescription);

    H_SCOPED_FUNCTION_TIMER();

	AActor* ParentActor = MeshComponent ? MeshComponent->GetOwner() : nullptr;

	// Convert the Mesh using FMeshDescription
	// Get references to the attributes we are interested in
	// before sending to Houdini we'll check if each attribute is valid

	TVertexAttributesConstRef<FVector3f> VertexPositions = MeshDescriptionAttributes.GetVertexPositions();
	TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceNormals = MeshDescriptionAttributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceTangents = MeshDescriptionAttributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = MeshDescriptionAttributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesConstRef<FVector4f> VertexInstanceColors = MeshDescriptionAttributes.GetVertexInstanceColors();
	TVertexInstanceAttributesConstRef<FVector2f> VertexInstanceUVs = MeshDescriptionAttributes.GetVertexInstanceUVs();
	//TPolygonGroupAttributesConstRef<FName> PolygonGroupMaterialSlotNames = MeshDescriptionAttributes.GetPolygonGroupMaterialSlotNames();

	// Get the vertex and triangle arrays
	const FVertexArray &MDVertices = MeshDescription.Vertices();
	const FPolygonGroupArray &MDPolygonGroups = MeshDescription.PolygonGroups();
	const FPolygonArray &MDPolygons = MeshDescription.Polygons();
	const FTriangleArray &MDTriangles = MeshDescription.Triangles();

	// Determine point, vertex and polygon counts
	const uint32 NumVertices = MDVertices.Num();
	const uint32 NumTriangles = MDTriangles.Num();
	const uint32 NumVertexInstances = NumTriangles * 3;

	// Some checks: we expect triangulated meshes
	if (MeshDescription.VertexInstances().Num() != NumTriangles * 3)
	{
		HOUDINI_LOG_ERROR(TEXT("Expected a triangulated mesh, but # VertexInstances (%d) != # Triangles * 3 (%d)"), MeshDescription.VertexInstances().Num(), NumTriangles * 3);
		return false;
	}

	// Determine which attributes we have
	const bool bIsVertexPositionsValid = VertexPositions.IsValid();
	const bool bIsVertexInstanceNormalsValid = VertexInstanceNormals.IsValid();
	const bool bIsVertexInstanceTangentsValid = VertexInstanceTangents.IsValid();
	const bool bIsVertexInstanceBinormalSignsValid = VertexInstanceBinormalSigns.IsValid();
	const bool bIsVertexInstanceColorsValid = VertexInstanceColors.IsValid();
	const bool bIsVertexInstanceUVsValid = VertexInstanceUVs.IsValid();
	//const bool bIsPolygonGroupImportedMaterialSlotNamesValid = PolygonGroupMaterialSlotNames.IsValid();

	// Create part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);

	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = NumVertexInstances;
	Part.faceCount = NumTriangles;
	Part.pointCount = NumVertices;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part), false);

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPoint );
	AttributeInfoPoint.count = Part.pointCount;
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	bool bDoTimings = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;

	FHoudiniPerfTimer PositionsTimer(TEXT("Positions"), bDoTimings);
	PositionsTimer.Start();

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITION (P)
	//--------------------------------------------------------------------------------------------------------------------- 
	// The mesh element arrays are sparse: the max index/ID value can be larger than the number of elements - 1
	// so we have to maintain a lookup of VertexID (UE) to PointIndex (Houdini)
	TArray<int32> VertexIDToHIndex;
	if (bIsVertexPositionsValid && VertexPositions.GetNumElements() >= 3)
	{
		TArray<float> StaticMeshVertices;
		StaticMeshVertices.SetNumUninitialized(NumVertices * 3);
				
		VertexIDToHIndex.SetNumUninitialized(MDVertices.GetArraySize());
		for (int32 n = 0; n < VertexIDToHIndex.Num(); n++)
			VertexIDToHIndex[n] = INDEX_NONE;

		int32 VertexIdx = 0;
		for (const FVertexID& VertexID : MDVertices.GetElementIDs())
		{
			// Convert Unreal to Houdini
			const FVector3f& PositionVector = VertexPositions.Get(VertexID);
			StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.X;
			StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Z;
			StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Y;

			// Record the UE Vertex ID to Houdini Point Index lookup
			VertexIDToHIndex[VertexID.GetValue()] = VertexIdx;
			VertexIdx++;
		}

		// Now that we have raw positions, we can upload them for our attribute.

		FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, StaticMeshVertices), false);
	}

	PositionsTimer.Stop();

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIAL SLOT -> MATERIAL INTERFACE
	//---------------------------------------------------------------------------------------------------------------------
	// In theory the ImportedMaterialSlotName attribute on PolygonGroups should tell us which material slot is used by
	// that group, and thus which MaterialIndex we should assign to triangles in that group. Unfortunately we have 
	// encountered cases where the ImportedMaterialSlotName name attribute does not match any of the MaterialSlotName or
	// ImportedMaterialSlotNames in the StaticMesh->StaticMaterials array. Therefore we have no choice but to rely
	// on the PolygonGroup order vs Section order to determine the MaterialIndex for a group. We do what Epic does
	// when building a static mesh: Sections are created in the same order as iterating over PolygonGroups, but importantly,
	// empty PolygonGroups are skipped

	// // Get material slot name to material index
	// and the UMaterialInterface array
	// TMap<FName, int32> MaterialSlotToInterface;
	TArray<UMaterialInterface*> MaterialInterfaces;
	TArray<int32> TriangleMaterialIndices;

	// If the mesh component is valid, and we are not using the ref counted input system, get the materials via
	// the component to account for overrides. For the ref counted input system the component will override the
	// materials in its input node.
	const bool bIsMeshComponentValid = (IsValid(MeshComponent) && MeshComponent->IsValidLowLevel());
	const int32 NumStaticMaterials = MeshMaterials.Num();
	// If we find any invalid Material (null or pending kill), or we find a section below with an out of range MaterialIndex,
	// then we will set UEDefaultMaterial at the invalid index
	int32 UEDefaultMaterialIndex = INDEX_NONE;
	UMaterialInterface *UEDefaultMaterial = nullptr;
	if (NumStaticMaterials > 0)
	{
		MaterialInterfaces.Reserve(NumStaticMaterials);
		for (int32 MaterialIndex = 0; MaterialIndex < NumStaticMaterials; ++MaterialIndex)
		{
			const FStaticMaterial &MaterialInfo = MeshMaterials[MaterialIndex];
			UMaterialInterface *Material = nullptr;
			{
				Material = MaterialInfo.MaterialInterface;
			}
			// If the Material is NULL or invalid, fallback to the default material
			if (!IsValid(Material))
			{
				if (!UEDefaultMaterial)
				{
					UEDefaultMaterial = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
					UEDefaultMaterialIndex = MaterialIndex;
				}
				Material = UEDefaultMaterial;
				HOUDINI_LOG_WARNING(TEXT("Material Index %d (slot %s) has an invalid material, falling back to default: %s"), MaterialIndex, *(MaterialInfo.MaterialSlotName.ToString()), *(UEDefaultMaterial->GetPathName()));
			}
			// MaterialSlotToInterface.Add(MaterialInfo.ImportedMaterialSlotName, MaterialIndex);
			MaterialInterfaces.Add(Material);
		}

		TriangleMaterialIndices.Reserve(NumTriangles);
	}
	// SectionIndex: Looking at Epic's StaticMesh build code, Sections are created in the same
	// order as iterating over PolygonGroups, but skipping empty PolygonGroups
	TMap<FPolygonGroupID, int32> PolygonGroupToMaterialIndex;
	PolygonGroupToMaterialIndex.Reserve(MeshDescription.PolygonGroups().Num());
	int32 SectionIndex = 0;
	for (const FPolygonGroupID &PolygonGroupID : MDPolygonGroups.GetElementIDs())
	{
		// Skip empty polygon groups
		if (MeshDescription.GetNumPolygonGroupPolygons(PolygonGroupID) == 0)
		{
			continue;
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// TRIANGLE MATERIAL ASSIGNMENT
		//---------------------------------------------------------------------------------------------------------------------
		// // Get the material index for the material slot for this polygon group
		//int32 MaterialIndex = INDEX_NONE;
		//if (bIsPolygonGroupImportedMaterialSlotNamesValid)
		//{
		//	const FName &MaterialSlotName = PolygonGroupMaterialSlotNames.Get(PolygonGroupID);
		//	const int32 *MaterialIndexPtr = MaterialSlotToInterface.Find(MaterialSlotName);
		//	if (MaterialIndexPtr != nullptr)
		//	{
		//		MaterialIndex = *MaterialIndexPtr;
		//	}
		//}

		// Get the material for the LOD and section via the section info map
		if (!SectionMaterialIndices.IsValidIndex(SectionIndex))
		{
			HOUDINI_LOG_ERROR(TEXT("Found more non-empty polygon groups in the mesh description for LOD %d than sections in the mesh..."), InLODIndex);
			return false;
		}

		// If the MaterialIndex referenced by this Section is out of range, fill MaterialInterfaces with UEDefaultMaterial
		// up to and including MaterialIndex and log a warning
		int32 MaterialIndex = SectionMaterialIndices[SectionIndex];
		if (!MaterialInterfaces.IsValidIndex(MaterialIndex))
		{
			if (!UEDefaultMaterial)
			{
				UEDefaultMaterial = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
				// Add the UEDefaultMaterial to MaterialInterfaces
				UEDefaultMaterialIndex = MaterialInterfaces.Add(UEDefaultMaterial);
			}
			HOUDINI_LOG_WARNING(TEXT("Section Index %d references an invalid Material Index %d, falling back to default material: %s"), SectionIndex, MaterialIndex, *(UEDefaultMaterial->GetPathName()));
			MaterialIndex = UEDefaultMaterialIndex;
		}

		PolygonGroupToMaterialIndex.Add(PolygonGroupID, MaterialIndex);
		SectionIndex++;
	}

	// Determine the final number of materials we have, with defaults for missing/invalid indices
	const int32 NumMaterials = MaterialInterfaces.Num();

	// Now we deal with vertex instance attributes. 
	// // First we must also record a UE VertexInstanceID to Houdini Vertex Index lookup, 
	// // and then get and convert all valid and supported vertex instance attributes from UE
	// TArray<int32> VertexInstanceIDToHIndex;

	FHoudiniPerfTimer MakeUVTimer(TEXT("UV Make"), bDoTimings);
	FHoudiniPerfTimer MakeNormalTimer(TEXT("Normals Make"), bDoTimings);
	FHoudiniPerfTimer MakeTangentTimer(TEXT("Tangents Make"), bDoTimings);
	FHoudiniPerfTimer MakeBinormalTimer(TEXT("Binormals Make"), bDoTimings);
	FHoudiniPerfTimer MakeColorsTimer(TEXT("Colors Make"), bDoTimings);
	FHoudiniPerfTimer MakeAlphasTimer(TEXT("Alphas Make"), bDoTimings);

	FHoudiniPerfTimer TransferUVTimer(TEXT("UV Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferNormalTimer(TEXT("Normals Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferTangentTimer(TEXT("Tangents Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferBinormalTimer(TEXT("Binormals Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferColorsTimer(TEXT("Colors Transfer"), bDoTimings);
	FHoudiniPerfTimer TransferAlphasTimer(TEXT("Alphas Transfer"), bDoTimings);

	if (NumTriangles > 0)
	{
		// UV layer array. Each layer has an array of floats, 3 floats per vertex instance
		TArray<TArray<float>> UVs;
		const int32 NumUVLayers = bIsVertexInstanceUVsValid ? FMath::Min(VertexInstanceUVs.GetNumChannels(), (int32)MAX_STATIC_TEXCOORDS) : 0;
		// Normals: 3 floats per vertex instance
		TArray<float> Normals;
		// Tangents: 3 floats per vertex instance
		TArray<float> Tangents;
		// Binormals: 3 floats per vertex instance
		TArray<float> Binormals;
		// RGBColors: 3 floats per vertex instance
		TArray<float> RGBColors;
		// Alphas: 1 float per vertex instance
		TArray<float> Alphas;

		// Initialize the arrays for the attributes that are valid
		if (bIsVertexInstanceUVsValid)
		{
			UVs.SetNum(NumUVLayers);
			for (int32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
			{
				UVs[UVLayerIndex].SetNumUninitialized(NumVertexInstances * 3);
			}
		}

		if (bIsVertexInstanceNormalsValid)
		{
			Normals.SetNumUninitialized(NumVertexInstances * 3);
		}

		if (bIsVertexInstanceTangentsValid)
		{
			Tangents.SetNumUninitialized(NumVertexInstances * 3);
		}

		if (bIsVertexInstanceBinormalSignsValid)
		{
			Binormals.SetNumUninitialized(NumVertexInstances * 3);
		}

		if (bExportVertexColors && bIsVertexInstanceColorsValid)
		{
			RGBColors.SetNumUninitialized(NumVertexInstances * 3);
			Alphas.SetNumUninitialized(NumVertexInstances);
		}

		// Array of material index per triangle/face
		TArray<int32> MeshTriangleVertexIndices;
		MeshTriangleVertexIndices.SetNumUninitialized(NumVertexInstances);
		// Array of vertex counts per triangle/face
		TArray<int32> MeshTriangleVertexCounts;
		MeshTriangleVertexCounts.SetNumUninitialized(NumTriangles);

		int32 TriangleIdx = 0;
		int32 VertexInstanceIdx = 0;
		{
			H_SCOPED_FUNCTION_STATIC_LABEL("Fetching Vertex Data");
		    for (const FPolygonID &PolygonID : MDPolygons.GetElementIDs())
		    {
			    for (const FTriangleID& TriangleID : MeshDescription.GetPolygonTriangles(PolygonID))
			    {

				    MeshTriangleVertexCounts[TriangleIdx] = 3;
				    for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
				    {
					    // Reverse the winding order for Houdini (but still start at 0)
					    const int32 WindingIdx = (3 - TriangleVertexIndex) % 3;
					    const FVertexInstanceID &VertexInstanceID = MeshDescription.GetTriangleVertexInstance(TriangleID, WindingIdx);

					    // // UE Vertex Instance ID to Houdini Vertex Index look up
					    // VertexInstanceIDToHIndex[VertexInstanceID.GetValue()] = VertexInstanceIdx;

					    // Calculate the index of the first component of a vertex instance's value in an inline float array 
					    // representing vectors (3 float) per vertex instance
					    const int32 Float3Index = VertexInstanceIdx * 3;

					    //--------------------------------------------------------------------------------------------------------------------- 
					    // UVS (uvX)
					    //--------------------------------------------------------------------------------------------------------------------- 
					    if (bIsVertexInstanceUVsValid)
					    {
							MakeUVTimer.Start();
						    for (int32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
						    {
							    const FVector2f &UV = VertexInstanceUVs.Get(VertexInstanceID, UVLayerIndex);
							    UVs[UVLayerIndex][Float3Index + 0] = UV.X;
							    UVs[UVLayerIndex][Float3Index + 1] = 1.0f - UV.Y;
							    UVs[UVLayerIndex][Float3Index + 2] = 0;
						    }
							MakeUVTimer.Stop();
					    }

					    //--------------------------------------------------------------------------------------------------------------------- 
					    // NORMALS (N)
					    //---------------------------------------------------------------------------------------------------------------------
					    if (bIsVertexInstanceNormalsValid)
					    {
							MakeNormalTimer.Start();
						    const FVector3f &Normal = VertexInstanceNormals.Get(VertexInstanceID);
						    Normals[Float3Index + 0] = Normal.X;
						    Normals[Float3Index + 1] = Normal.Z;
						    Normals[Float3Index + 2] = Normal.Y;
							MakeNormalTimer.Stop();
					    }

					    //--------------------------------------------------------------------------------------------------------------------- 
					    // TANGENT (tangentu)
					    //---------------------------------------------------------------------------------------------------------------------
					    if (bIsVertexInstanceTangentsValid)
					    {
							MakeTangentTimer.Start();
						    const FVector3f &Tangent = VertexInstanceTangents.Get(VertexInstanceID);
						    Tangents[Float3Index + 0] = Tangent.X;
						    Tangents[Float3Index + 1] = Tangent.Z;
						    Tangents[Float3Index + 2] = Tangent.Y;
							MakeTangentTimer.Stop();
					    }

					    //--------------------------------------------------------------------------------------------------------------------- 
					    // BINORMAL (tangentv)
					    //---------------------------------------------------------------------------------------------------------------------
					    // In order to calculate the binormal we also need the tangent and normal
					    if (bIsVertexInstanceBinormalSignsValid && bIsVertexInstanceTangentsValid && bIsVertexInstanceNormalsValid)
					    {
							MakeBinormalTimer.Start();
						    const float &BinormalSign = VertexInstanceBinormalSigns.Get(VertexInstanceID);
						    FVector Binormal = FVector::CrossProduct(
							    FVector(Tangents[Float3Index + 0], Tangents[Float3Index + 1], Tangents[Float3Index + 2]),
							    FVector(Normals[Float3Index + 0], Normals[Float3Index + 1], Normals[Float3Index + 2])
						    ) * BinormalSign;
						    Binormals[Float3Index + 0] = (float)Binormal.X;
						    Binormals[Float3Index + 1] = (float)Binormal.Y;
						    Binormals[Float3Index + 2] = (float)Binormal.Z;
							MakeBinormalTimer.Stop();
					    }

					    //--------------------------------------------------------------------------------------------------------------------- 
					    // COLORS (Cd)
					    //---------------------------------------------------------------------------------------------------------------------
					    if (bExportVertexColors && bIsVertexInstanceColorsValid)
					    {
							MakeColorsTimer.Start();
						    FLinearColor Color = FLinearColor::White;
							// Convert from SRGB to Linear. Unfortunately UE only provides this via the FColor()
							// structure, so we loose precision as we have to convert to 8-bit and back to 32-bit.
							FLinearColor SRGBColor = VertexInstanceColors.Get(VertexInstanceID);
							Color = SRGBColor.ToFColor(true).ReinterpretAsLinear();
						    RGBColors[Float3Index + 0] = Color.R;
						    RGBColors[Float3Index + 1] = Color.G;
						    RGBColors[Float3Index + 2] = Color.B;
						    Alphas[VertexInstanceIdx] = Color.A;
							MakeColorsTimer.Stop();
					    }

					    //--------------------------------------------------------------------------------------------------------------------- 
					    // TRIANGLE/FACE VERTEX INDICES
					    //---------------------------------------------------------------------------------------------------------------------

					    const FVertexID& VertexID = MeshDescription.GetVertexInstanceVertex(VertexInstanceID);
					    const int32 UEVertexIdx = VertexID.GetValue();
					    if (VertexIDToHIndex.IsValidIndex(UEVertexIdx))
					    {
						    MeshTriangleVertexIndices[VertexInstanceIdx] = VertexIDToHIndex[UEVertexIdx];
					    }

					    VertexInstanceIdx++;
				    }

				    //--------------------------------------------------------------------------------------------------------------------- 
				    // TRIANGLE MATERIAL ASSIGNMENT
				    //---------------------------------------------------------------------------------------------------------------------
				    const FPolygonGroupID& PolygonGroupID = MeshDescription.GetPolygonPolygonGroup(PolygonID);
				    int32 MaterialIndex = PolygonGroupToMaterialIndex.FindChecked(PolygonGroupID);
				    TriangleMaterialIndices.Add(MaterialIndex);

				    TriangleIdx++;
			    }

		    }
		}
		// Now transfer valid vertex instance attributes to Houdini vertex attributes

		{
			H_SCOPED_FUNCTION_STATIC_LABEL("Transfering Data");
		    //--------------------------------------------------------------------------------------------------------------------- 
		    // UVS (uvX)
		    //--------------------------------------------------------------------------------------------------------------------- 
		    if (bIsVertexInstanceUVsValid)
		    {
				TransferUVTimer.Start();
			    for (int32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; UVLayerIndex++)
			    {
				    // Construct the attribute name for this UV index.
				    FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;
				    if (UVLayerIndex > 0)
					    UVAttributeName += FString::Printf(TEXT("%d"), UVLayerIndex + 1);

				    // Create attribute for UVs
				    HAPI_AttributeInfo AttributeInfoVertex;
				    FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

				    AttributeInfoVertex.count = NumVertexInstances;
				    AttributeInfoVertex.tupleSize = 3;
				    AttributeInfoVertex.exists = true;
				    AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
				    AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
				    AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

				    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
					    FHoudiniEngine::Get().GetSession(),
					    NodeId, 0, TCHAR_TO_ANSI(*UVAttributeName), &AttributeInfoVertex), false);

					FHoudiniHapiAccessor Accessor(NodeId, 0, TCHAR_TO_ANSI(*UVAttributeName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, UVs[UVLayerIndex]), false);
			    }
				TransferUVTimer.Stop();
		    }

		    //--------------------------------------------------------------------------------------------------------------------- 
		    // NORMALS (N)
		    //---------------------------------------------------------------------------------------------------------------------
		    if (bIsVertexInstanceNormalsValid)
		    {
				TransferNormalTimer.Start();
			    // Create attribute for normals.
			    HAPI_AttributeInfo AttributeInfoVertex;
			    FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			    AttributeInfoVertex.tupleSize = 3;
			    AttributeInfoVertex.count = Normals.Num() / AttributeInfoVertex.tupleSize;
			    AttributeInfoVertex.exists = true;
			    AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			    AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			    AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				    FHoudiniEngine::Get().GetSession(),
				    NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex), false);

				FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Normals), false);

				TransferNormalTimer.Stop();
		    }

		    //--------------------------------------------------------------------------------------------------------------------- 
		    // TANGENT (tangentu)
		    //---------------------------------------------------------------------------------------------------------------------
		    if (bIsVertexInstanceTangentsValid)
		    {
				TransferTangentTimer.Start();
			    // Create attribute for tangentu.
			    HAPI_AttributeInfo AttributeInfoVertex;
			    FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			    AttributeInfoVertex.tupleSize = 3;
			    AttributeInfoVertex.count = Tangents.Num() / AttributeInfoVertex.tupleSize;
			    AttributeInfoVertex.exists = true;
			    AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			    AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			    AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				    FHoudiniEngine::Get().GetSession(),
				    NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, &AttributeInfoVertex), false);

				FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Tangents), false);

				TransferTangentTimer.Stop();
		    }

		    //--------------------------------------------------------------------------------------------------------------------- 
		    // BINORMAL (tangentv)
		    //---------------------------------------------------------------------------------------------------------------------
		    if (bIsVertexInstanceBinormalSignsValid)
		    {
				TransferBinormalTimer.Start();
			    // Create attribute for normals.
			    HAPI_AttributeInfo AttributeInfoVertex;
			    FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			    AttributeInfoVertex.tupleSize = 3;
			    AttributeInfoVertex.count = Binormals.Num() / AttributeInfoVertex.tupleSize;
			    AttributeInfoVertex.exists = true;
			    AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			    AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			    AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				    FHoudiniEngine::Get().GetSession(),
				    NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, &AttributeInfoVertex), false);

				FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Binormals), false);

				TransferBinormalTimer.Stop();
		    }

		    //--------------------------------------------------------------------------------------------------------------------- 
		    // COLORS (Cd)
		    //---------------------------------------------------------------------------------------------------------------------
		    if (bExportVertexColors && bIsVertexInstanceColorsValid)
		    {
				TransferColorsTimer.Start();
			    // Create attribute for colors.
			    HAPI_AttributeInfo AttributeInfoVertex;
			    FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			    AttributeInfoVertex.tupleSize = 3;
			    AttributeInfoVertex.count = RGBColors.Num() / AttributeInfoVertex.tupleSize;
			    AttributeInfoVertex.exists = true;
			    AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			    AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			    AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				    FHoudiniEngine::Get().GetSession(),
				    NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex), false);

				FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, RGBColors), false);

			    FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);
			    AttributeInfoVertex.tupleSize = 1;
			    AttributeInfoVertex.count = Alphas.Num();
			    AttributeInfoVertex.exists = true;
			    AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			    AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			    AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				    FHoudiniEngine::Get().GetSession(),
				    NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, &AttributeInfoVertex), false);

				Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoVertex, Alphas), false);

				TransferColorsTimer.Stop();
		    }

		    //--------------------------------------------------------------------------------------------------------------------- 
		    // TRIANGLE/FACE VERTEX INDICES
		    //---------------------------------------------------------------------------------------------------------------------
		    // We can now set vertex list.
		    HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
			    MeshTriangleVertexIndices, NodeId, 0), false);

		    // Send the array of face vertex counts.
		    TArray<int32> StaticMeshFaceCounts;
		    StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);
		    for (int32 n = 0; n < Part.faceCount; n++)
			    StaticMeshFaceCounts[n] = 3;

		    HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
			    StaticMeshFaceCounts, NodeId, 0), false);

		    // Send material assignments to Houdini
		    if (NumMaterials > 0)
		    {
			    // List of materials, one for each face.
			    FHoudiniEngineIndexedStringMap TriangleMaterials;

			    //Lists of material parameters
			    TMap<FString, TArray<float>> ScalarMaterialParameters;
			    TMap<FString, TArray<float>> VectorMaterialParameters;
                TMap<FString, FHoudiniEngineIndexedStringMap> TextureMaterialParameters;
				TMap<FString, TArray<int8>>BoolMaterialParameters;

				FHoudiniPerfTimer TransferFaceArray(TEXT("Face Material Array"), bDoTimings);
				TransferFaceArray.Start();

				bool bAttributeSuccess = false;
				if (bInExportMaterialParametersAsAttributes)
				{
					// Create attributes for the material and all its parameters
					// Get material attribute data, and all material parameters data
					FUnrealMeshTranslator::CreateFaceMaterialArray(
						MaterialInterfaces,
						TriangleMaterialIndices, 
						TriangleMaterials,
						ScalarMaterialParameters, 
						VectorMaterialParameters, 
						TextureMaterialParameters,
						BoolMaterialParameters);
			    }
			    else
			    {
				    // Create attributes only for the materials
				    // Only get the material attribute data
				    FUnrealMeshTranslator::CreateFaceMaterialArray(
					    MaterialInterfaces, 
						TriangleMaterialIndices, 
						TriangleMaterials);
			    }
				TransferFaceArray.Stop();

				FHoudiniPerfTimer TransferMeshAttributes(TEXT("Mesh Attributes"), bDoTimings);
				TransferMeshAttributes.Start();

			    // Create all the needed attributes for materials
			    bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
				    NodeId,
				    0,
				    TriangleMaterials.GetIds().Num(),
				    TriangleMaterials,
					TriangleMaterialIndices,
				    ScalarMaterialParameters,
				    VectorMaterialParameters,
				    TextureMaterialParameters,
					BoolMaterialParameters,
				    PhysicalMaterialPath,
					NaniteSettings);

				TransferMeshAttributes.Stop();
			    if (!bAttributeSuccess)
			    {
					HOUDINI_LOG_ERROR(TEXT("Failed to Create Mesh Attributes."));
				    return false;
			    }
		    }

		    //--------------------------------------------------------------------------------------------------------------------- 
		    // TRIANGLE SMOOTHING MASKS
		    //---------------------------------------------------------------------------------------------------------------------
		    TArray<int32> TriangleSmoothingMasks;
		    TriangleSmoothingMasks.SetNumZeroed(NumTriangles);
		    {
			    // Convert uint32 smoothing mask to int
			    TArray<uint32> UnsignedSmoothingMasks;
			    UnsignedSmoothingMasks.SetNumZeroed(NumTriangles);
			    FStaticMeshOperations::ConvertHardEdgesToSmoothGroup(MeshDescription, UnsignedSmoothingMasks);
			    for (int32 n = 0; n < TriangleSmoothingMasks.Num(); n++)
				    TriangleSmoothingMasks[n] = (int32)UnsignedSmoothingMasks[n];
		    }

		    if (TriangleSmoothingMasks.Num() > 0)
		    {
			    HAPI_AttributeInfo AttributeInfoSmoothingMasks;
			    FHoudiniApi::AttributeInfo_Init(&AttributeInfoSmoothingMasks);

			    AttributeInfoSmoothingMasks.tupleSize = 1;
			    AttributeInfoSmoothingMasks.count = TriangleSmoothingMasks.Num();
			    AttributeInfoSmoothingMasks.exists = true;
			    AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
			    AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
			    AttributeInfoSmoothingMasks.originalOwner = HAPI_ATTROWNER_INVALID;

			    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				    FHoudiniEngine::Get().GetSession(),
				    NodeId, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, &AttributeInfoSmoothingMasks), false);

				FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK);
				HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoSmoothingMasks, TriangleSmoothingMasks), false);

		    }
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// LIGHTMAP RESOLUTION
	//---------------------------------------------------------------------------------------------------------------------
	if (LightMapResolution.IsSet())
	{
		HAPI_AttributeInfo AttributeInfoLightMapResolution;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoLightMapResolution);
		AttributeInfoLightMapResolution.count = 1;
		AttributeInfoLightMapResolution.tupleSize = 1;
		AttributeInfoLightMapResolution.exists = true;
		AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, &AttributeInfoLightMapResolution), false);

		FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoLightMapResolution, LightMapResolution.GetValue()), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT MESH NAME
	//---------------------------------------------------------------------------------------------------------------------
	{
		H_SCOPED_FUNCTION_STATIC_LABEL(HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME);

		// Create primitive attribute with mesh asset path
		const FString MeshAssetPath = Mesh->GetPathName();

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = Part.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME, &AttributeInfo), false);

		FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfo, MeshAssetPath), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT SOURCE FILE
	//---------------------------------------------------------------------------------------------------------------------
	{
		H_SCOPED_FUNCTION_STATIC_LABEL(HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE);

		// Create primitive attribute with mesh asset path
		FString Filename;
		if (IsValid(ImportData))
		{
			for (const auto& SourceFile : ImportData->SourceData.SourceFiles)
			{
				Filename = UAssetImportData::ResolveImportFilename(SourceFile.RelativeFilename, ImportData->GetOutermost());
				break;
			}
		}

		if (!Filename.IsEmpty())
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = Part.faceCount;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE, &AttributeInfo), false);

			FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfo, Filename), false);
		}
	}

	/*
	// Check if we have vertex attribute data to add
	if (StaticMeshComponent && StaticMeshComponent->GetOwner())
	{
		if (UHoudiniAttributeDataComponent* DataComponent = StaticMeshComponent->GetOwner()->FindComponentByClass<UHoudiniAttributeDataComponent>())
		{
			bool bSuccess = DataComponent->Upload(NodeId, StaticMeshComponent);
			if (!bSuccess)
			{
				HOUDINI_LOG_ERROR(TEXT("Upload of attribute data for %s failed"), *StaticMeshComponent->GetOwner()->GetName());
			}
		}
	}
	*/

	//--------------------------------------------------------------------------------------------------------------------- 
	// LOD GROUP AND SCREEN SIZE
	//---------------------------------------------------------------------------------------------------------------------
	if (bAddLODGroups)
	{
		H_SCOPED_FUNCTION_STATIC_LABEL("LOD GROUP AND SCREEN SIZE");

		// LOD Group

		FString LODGroup = TEXT("lod") + FString::FromInt(InLODIndex);

		// Add a LOD group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, H_TCHAR_TO_UTF8(*LODGroup)), false);

		// Set GroupMembership
		TArray<int> GroupArray;
		GroupArray.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < GroupArray.Num(); n++)
			GroupArray[n] = 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, H_TCHAR_TO_UTF8(*LODGroup),
			GroupArray.GetData(), 0, Part.faceCount), false);

		if (LODScreenSize.IsSet())
		{
			// Add the lodX_screensize attribute
			FString LODAttributeName =
				TEXT(HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_PREFIX) + FString::FromInt(InLODIndex) + TEXT(HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_POSTFIX);

			// Create lodX_screensize detail attribute info.
			HAPI_AttributeInfo AttributeInfoLODScreenSize;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoLODScreenSize);
			AttributeInfoLODScreenSize.count = 1;
			AttributeInfoLODScreenSize.tupleSize = 1;
			AttributeInfoLODScreenSize.exists = true;
			AttributeInfoLODScreenSize.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfoLODScreenSize.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoLODScreenSize.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, H_TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize), false);

			const float LODScreenSizeValue = LODScreenSize.GetValue();
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(), NodeId, 0,
				H_TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize,
				&LODScreenSizeValue, 0, 1), false);
		}
	}

	if (bCommitGeo)
	{
		// Commit the geo.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), false);
	}

	OutPartInfo = Part;
	
	return true;
}

void
FUnrealMeshTranslator::CreateFaceMaterialArray(
	const TArray<UMaterialInterface* >& Materials,
	const TArray<int32>& FaceMaterialIndices,
	FHoudiniEngineIndexedStringMap& OutStaticMeshFaceMaterials)
{	
	// Get the default material
	UMaterialInterface * DefaultMaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
	FString DefaultMaterialName = DefaultMaterialInterface ? DefaultMaterialInterface->GetPathName() : TEXT("default");

    OutStaticMeshFaceMaterials.Reset(Materials.Num(), FaceMaterialIndices.Num());

	// We need to create list of unique materials.
	TArray<FString> PerSlotMaterialList;

	UMaterialInterface* MaterialInterface = nullptr;
	if (Materials.Num())
	{
		// We have materials.
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); MaterialIdx++)
		{
			FString ParamPrefix = Materials.Num() == 1 ? "" : FString::FromInt(MaterialIdx) + FString("_");
			MaterialInterface = Materials[MaterialIdx];
			if (!MaterialInterface)
			{
				// Null material interface found, add default instead.
				PerSlotMaterialList.Add(DefaultMaterialName);
			}
			else
			{
				// We found a material, get its name
				PerSlotMaterialList.Add(MaterialInterface->GetPathName());
			}
		}
	}
	else
	{
		// We do not have any materials, just add default.
		PerSlotMaterialList.Add(DefaultMaterialName);
	}

	// Add the Material slot index in brackets if we have more than one material
	if (PerSlotMaterialList.Num() > 1)
	{
		for (int32 Idx = 0; Idx < PerSlotMaterialList.Num(); ++Idx)
		{
			PerSlotMaterialList[Idx] = "[" + FString::FromInt(Idx) + "]" + PerSlotMaterialList[Idx];
		}
	}

	OutStaticMeshFaceMaterials.Reset(PerSlotMaterialList.Num(), FaceMaterialIndices.Num());
	for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
	{
		int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];

		const FString & MaterialName = PerSlotMaterialList.IsValidIndex(FaceMaterialIdx) ? PerSlotMaterialList[FaceMaterialIdx] : DefaultMaterialName;
		OutStaticMeshFaceMaterials.SetString(FaceIdx, MaterialName);
	}
}


void
FUnrealMeshTranslator::CreateFaceMaterialArray(
	const TArray<UMaterialInterface* >& Materials,
	const TArray<int32>& FaceMaterialIndices,
	FHoudiniEngineIndexedStringMap& OutStaticMeshFaceMaterials,
	TMap<FString, TArray<float>> & OutScalarMaterialParameters,
	TMap<FString, TArray<float>> & OutVectorMaterialParameters,
    TMap<FString, FHoudiniEngineIndexedStringMap>& OutTextureMaterialParameters,
	TMap<FString, TArray<int8>>& OutBoolMaterialParameters)
{
    H_SCOPED_FUNCTION_TIMER();

	// Get the default material
	UMaterialInterface* DefaultMaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
	FString DefaultMaterialName = DefaultMaterialInterface ? DefaultMaterialInterface->GetPathName() : TEXT("default");

	// We need to create list of unique materials.
	TArray<FString> PerSlotMaterialList;

	// Initialize material parameter arrays. The key on each array is a prefix + the name of the parameter. The prefix is the
	// material slot number, although omitted if there is only one material.

	TMap<FString, TArray<float>> ScalarParams;
	TMap<FString, TArray<FLinearColor>> VectorParams;
	TMap<FString, TArray<FString>> TextureParams;
	TMap<FString, TArray<int8>> BoolParams;

	UMaterialInterface* MaterialInterface = nullptr;
	if (Materials.Num() > 0)
	{
		H_SCOPED_FUNCTION_STATIC_LABEL("Gather Materials");

		// We have materials.
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); MaterialIdx++)
		{
			FString ParamPrefix = Materials.Num() == 1 ? "" : FString::FromInt(MaterialIdx) + FString("_");
			MaterialInterface = Materials[MaterialIdx];
			if (!MaterialInterface)
			{
				// Null material interface found, add default instead.
				PerSlotMaterialList.Add(DefaultMaterialName);

				// No need to collect material parameters on the default material
				continue;
			}

			// We found a material, get its name and material parameters
			PerSlotMaterialList.Add(MaterialInterface->GetPathName());

			// Collect all scalar parameters in this material
			{
				TArray<FMaterialParameterInfo> MaterialScalarParamInfos;
				TArray<FGuid> MaterialScalarParamGuids;
				MaterialInterface->GetAllScalarParameterInfo(MaterialScalarParamInfos, MaterialScalarParamGuids);

				for (auto& CurScalarParam : MaterialScalarParamInfos)
				{
					FString CurScalarParamName = ParamPrefix + CurScalarParam.Name.ToString();
					float CurScalarVal;
					MaterialInterface->GetScalarParameterValue(CurScalarParam, CurScalarVal);
					if (!ScalarParams.Contains(CurScalarParamName))
					{
						TArray<float> CurArray;
						CurArray.SetNumUninitialized(Materials.Num());
						// Initialize the array with the Min float value
						for (int32 ArrIdx = 0; ArrIdx < CurArray.Num(); ++ArrIdx)
							CurArray[ArrIdx] = FLT_MIN;

						ScalarParams.Add(CurScalarParamName, CurArray);
						OutScalarMaterialParameters.Add(CurScalarParamName);
					}

					ScalarParams[CurScalarParamName][MaterialIdx] = CurScalarVal;
				}
			}

			// Collect all vector parameters in this material
			{
				TArray<FMaterialParameterInfo> MaterialVectorParamInfos;
				TArray<FGuid> MaterialVectorParamGuids;
				MaterialInterface->GetAllVectorParameterInfo(MaterialVectorParamInfos, MaterialVectorParamGuids);

				for (auto& CurVectorParam : MaterialVectorParamInfos) 
				{
					FString CurVectorParamName = ParamPrefix + CurVectorParam.Name.ToString();
					FLinearColor CurVectorValue;
					MaterialInterface->GetVectorParameterValue(CurVectorParam, CurVectorValue);
					if (!VectorParams.Contains(CurVectorParamName)) 
					{
						TArray<FLinearColor> CurArray;
						CurArray.SetNumUninitialized(Materials.Num());
						FLinearColor MinColor(FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN);
						for (int32 ArrIdx = 0; ArrIdx < CurArray.Num(); ++ArrIdx)
							CurArray[ArrIdx] = MinColor;

						VectorParams.Add(CurVectorParamName, CurArray);
						OutVectorMaterialParameters.Add(CurVectorParamName);
					}

					VectorParams[CurVectorParamName][MaterialIdx] = CurVectorValue;
				}
			}

			// Collect all texture parameters in this material
			{
				TArray<FMaterialParameterInfo> MaterialTextureParamInfos;
				TArray<FGuid> MaterialTextureParamGuids;
				MaterialInterface->GetAllTextureParameterInfo(MaterialTextureParamInfos, MaterialTextureParamGuids);

				for (auto & CurTextureParam : MaterialTextureParamInfos) 
				{
					FString CurTextureParamName = ParamPrefix + CurTextureParam.Name.ToString();
					UTexture * CurTexture = nullptr;
					MaterialInterface->GetTextureParameterValue(CurTextureParam, CurTexture);

					if (!IsValid(CurTexture))
						continue;

					FString TexturePath = CurTexture->GetPathName();
					if (!TextureParams.Contains(CurTextureParamName)) 
					{
						TArray<FString> CurArray;
						CurArray.SetNumZeroed(Materials.Num());

						TextureParams.Add(CurTextureParamName, CurArray);
						OutTextureMaterialParameters.Add(CurTextureParamName);
					}

					TextureParams[CurTextureParamName][MaterialIdx] = TexturePath;
				}
			}

			// Collect all bool parameters in this material
			{
				TArray<FMaterialParameterInfo> MaterialBoolParamInfos;
				TArray<FGuid> MaterialBoolParamGuids;
				MaterialInterface->GetAllStaticSwitchParameterInfo(MaterialBoolParamInfos, MaterialBoolParamGuids);

				for (auto& CurBoolParam : MaterialBoolParamInfos)
				{
					FString CurBoolParamName = ParamPrefix + CurBoolParam.Name.ToString();
					bool CurBool = false;
					FGuid CurExprValue;
					MaterialInterface->GetStaticSwitchParameterValue(CurBoolParam, CurBool, CurExprValue);

					if (!BoolParams.Contains(CurBoolParamName))
					{
						TArray<int8> CurArray;
						CurArray.SetNumZeroed(Materials.Num());

						BoolParams.Add(CurBoolParamName, CurArray);
						OutBoolMaterialParameters.Add(CurBoolParamName);
					}

					BoolParams[CurBoolParamName][MaterialIdx] = CurBool ? 1 : 0;
				}
			}		
		}
	}
	else
	{
		// We do not have any materials, add default.
		PerSlotMaterialList.Add(DefaultMaterialName);
	}

	// Add the Material slot index in brackets if we have more than one material
	if (PerSlotMaterialList.Num() > 1)
	{
		for (int32 Idx = 0; Idx < PerSlotMaterialList.Num(); ++Idx)
		{
			PerSlotMaterialList[Idx] = "[" + FString::FromInt(Idx) + "]" + PerSlotMaterialList[Idx];
		}
	}

    // Set all materials per face
    {
		H_SCOPED_FUNCTION_STATIC_LABEL("Materials");
        OutStaticMeshFaceMaterials.Reset(PerSlotMaterialList.Num(), FaceMaterialIndices.Num());
        for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
        {
            int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
            if (PerSlotMaterialList.IsValidIndex(FaceMaterialIdx))
            {
                OutStaticMeshFaceMaterials.SetString(FaceIdx, PerSlotMaterialList[FaceMaterialIdx]);
            }
            else
            {
                OutStaticMeshFaceMaterials.SetString(FaceIdx, DefaultMaterialName);
            }
        }
    }

	// Add scalar parameters
	{
		H_SCOPED_FUNCTION_STATIC_LABEL("ScalarParams");
	    for (auto& Pair : ScalarParams)
        {
            auto & Entries = OutScalarMaterialParameters[Pair.Key];
            Entries.SetNum(FaceMaterialIndices.Num());
			int Index = 0;
            for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
            {
                int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
                if (PerSlotMaterialList.IsValidIndex(FaceMaterialIdx)) 
			    {
                    const auto& Value = Pair.Value[FaceMaterialIdx];
                    Entries[Index++] = Value;
                }
            }
            check(Index == Entries.Num());
        }
	}

	// Add vector parameters.
	{
		H_SCOPED_FUNCTION_STATIC_LABEL("VectorParams");
        for (auto& Pair : VectorParams)
        {
            auto& Entries = OutVectorMaterialParameters[Pair.Key];
            Entries.SetNum(FaceMaterialIndices.Num() * 4);
			int Index = 0;
            for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
            {
                int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
                if (PerSlotMaterialList.IsValidIndex(FaceMaterialIdx))
                {
					const auto & Value = Pair.Value[FaceMaterialIdx];
                    Entries[Index++] = Value.R;
                    Entries[Index++] = Value.G;
                    Entries[Index++] = Value.B;
                    Entries[Index++] = Value.A;
                }
            }
            check(Index == Entries.Num());
        }
	}

	// Add texture params.
	{
		H_SCOPED_FUNCTION_STATIC_LABEL("TextureParams");

	    for (auto& Pair : TextureParams)
        {
            auto& Entries = OutTextureMaterialParameters[Pair.Key];
            Entries.Reset(PerSlotMaterialList.Num(), FaceMaterialIndices.Num());
            for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
            {
                int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
                if (PerSlotMaterialList.IsValidIndex(FaceMaterialIdx))
                {
                    Entries.SetString(FaceIdx, Pair.Value[FaceMaterialIdx]);
                }
            }
        }
	}

	// Add bool params.
	{
		H_SCOPED_FUNCTION_STATIC_LABEL("BoolParams");
		for (auto& Pair : BoolParams)
		{
			auto& Entries = OutBoolMaterialParameters[Pair.Key];
			Entries.SetNum(FaceMaterialIndices.Num());
			int Index = 0;
			for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
			{
				int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
				if (PerSlotMaterialList.IsValidIndex(FaceMaterialIdx))
				{
					const auto& Value = Pair.Value[FaceMaterialIdx];
					Entries[Index++] = Value;
				}
			}
			check(Index == Entries.Num());
		}
	}
}


bool
FUnrealMeshTranslator::CreateInputNodeForBox(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId InParentNodeID,
	const int32 ColliderIndex,
	const FVector& BoxCenter,
	const FVector& BoxExtent,
	const FRotator& BoxRotation)
{
	// Create a new input node for the box collider
	FString BoxName = TEXT("box") + FString::FromInt(ColliderIndex);

	// Create the node in this input object's OBJ node
	HAPI_NodeId BoxNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, TEXT("box"), BoxName, false, &BoxNodeId), false);
		
	// Set the box parameters
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "size", 0, (float)(BoxExtent.X) / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "size", 1, (float)(BoxExtent.Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "size", 2, (float)(BoxExtent.Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "t", 0, (float)(BoxCenter.X) / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "t", 1, (float)(BoxCenter.Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "t", 2, (float)(BoxCenter.Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	// Do coordinate system conversion before sending to Houdini
	FQuat RotationQuat = BoxRotation.Quaternion();

	Swap(RotationQuat.Y, RotationQuat.Z);
	RotationQuat.W = -RotationQuat.W;
	const FRotator Rotator = RotationQuat.Rotator();

	// Negate roll and pitch since they are actually RHR
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "r", 0, (float)-Rotator.Roll);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "r", 1, (float)-Rotator.Pitch);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "r", 2, (float)Rotator.Yaw);
	
	if (!FHoudiniEngineUtils::HapiCookNode(BoxNodeId, nullptr, true))
		return false;

	// Create a group node
	FString GroupNodeName = TEXT("group") + FString::FromInt(ColliderIndex);

	HAPI_NodeId GroupNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, TEXT("groupcreate"), GroupNodeName, false, &GroupNodeId), false);

	// Set its group name param to collision_geo_simple_box
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId parmId = FHoudiniEngineUtils::HapiFindParameterByName(GroupNodeId, "groupname", ParmInfo);

	FString LODGroup = TEXT("collision_geo_simple_box") + FString::FromInt(ColliderIndex);

	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, H_TCHAR_TO_UTF8(*LODGroup), parmId, 0);

	// Connect the box to the group
	FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, BoxNodeId, 0);

	OutNodeId = GroupNodeId;

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForSphere(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId InParentNodeID,
	const int32 ColliderIndex,
	const FVector& SphereCenter,
	const float SphereRadius)
{
	// Create a new input node for the sphere collider
	FString SphereName = TEXT("Sphere") + FString::FromInt(ColliderIndex);

	// Create the node in this input object's OBJ node
	HAPI_NodeId SphereNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, "sphere", SphereName, false, &SphereNodeId), false);

	// Set the box parameters
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "rad", 0, SphereRadius / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "rad", 1, SphereRadius / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "rad", 2, SphereRadius / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "t", 0, (float)(SphereCenter.X) / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "t", 1, (float)(SphereCenter.Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "t", 2, (float)(SphereCenter.Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "type", 0, 1);
	/*
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "scale", 0, SphereRadius / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	*/

	if (!FHoudiniEngineUtils::HapiCookNode(SphereNodeId, nullptr, true))
		return false;

	// Create a group node
	FString GroupNodeName = TEXT("group") + FString::FromInt(ColliderIndex);
	HAPI_NodeId GroupNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
		InParentNodeID, TEXT("groupcreate"), GroupNodeName, false, &GroupNodeId), false);

	// Set its group name param to collision_geo_simple_box
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId parmId = FHoudiniEngineUtils::HapiFindParameterByName(GroupNodeId, "groupname", ParmInfo);

	FString LODGroup = TEXT("collision_geo_simple_sphere") + FString::FromInt(ColliderIndex);
	
	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, H_TCHAR_TO_UTF8(*LODGroup), parmId, 0);

	// Connect the box to the group
	FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, SphereNodeId, 0);

	OutNodeId = GroupNodeId;

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForSphyl(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId InParentNodeID,
	const int32 ColliderIndex,
	const FVector& SphylCenter,
	const FRotator& SphylRotation,
	const float SphylRadius,
	const float SphereLength)
{
	//
	// Get the Sphyl's vertices and indices
	// (code drived from FKSphylElem::GetElemSolid)
	//

	// TODO:
	// Rotation?
	const int32 NumSides = 6;
	const int32 NumRings = (NumSides / 2) + 1;

	// The first/last arc are on top of each other.
	const int32 NumVerts = (NumSides + 1) * (NumRings + 1);	

	// Calculate the vertices for one arc
	TArray<FVector> ArcVertices;
	ArcVertices.SetNum(NumRings + 1);
	for (int32 RingIdx = 0; RingIdx < NumRings + 1; RingIdx++)
	{
		float Angle;
		float ZOffset;
		if (RingIdx <= NumSides / 4)
		{
			Angle = ((float)RingIdx / (NumRings - 1)) * PI;
			ZOffset = 0.5 * SphereLength;
		}
		else
		{
			Angle = ((float)(RingIdx - 1) / (NumRings - 1)) * PI;
			ZOffset = -0.5 * SphereLength;
		}

		// Note- unit sphere, so position always has mag of one. We can just use it for normal!		
		FVector SpherePos;
		SpherePos.X = 0.0f;
		SpherePos.Y = SphylRadius * FMath::Sin(Angle);
		SpherePos.Z = SphylRadius * FMath::Cos(Angle);

		ArcVertices[RingIdx] = SpherePos + FVector(0, 0, ZOffset);
	}

	// Get the transform matrix for the rotation

	// Get the Sphyl's vertices by rotating the arc NumSides+1 times.
	TArray<float> Vertices;
	Vertices.SetNum(NumVerts * 3);
	for (int32 SideIdx = 0; SideIdx < NumSides + 1; SideIdx++)
	{
		const FRotator ArcRotator(0, 360.f * ((float)SideIdx / NumSides), 0);
		const FRotationMatrix ArcRot(ArcRotator);
		const float XTexCoord = ((float)SideIdx / NumSides);

		for (int32 VertIdx = 0; VertIdx < NumRings + 1; VertIdx++)
		{
			int32 VIx = (NumRings + 1)*SideIdx + VertIdx;

			FVector ArcVertex = ArcRot.TransformPosition(ArcVertices[VertIdx]);
			ArcVertex = SphylRotation.Quaternion() * ArcVertex;

			FVector CurPosition = SphylCenter + ArcVertex;

			// Convert the UE4 position to Houdini
			Vertices[VIx * 3 + 0] = (float)(CurPosition.X) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[VIx * 3 + 1] = (float)(CurPosition.Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[VIx * 3 + 2] = (float)(CurPosition.Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}
	}

	// Add all of the indices to the mesh.
	int32 NumIndices = NumSides * NumRings * 6;
	TArray<int32> Indices;
	Indices.SetNum(NumIndices);

	int32 CurIndex = 0;
	for (int32 SideIdx = 0; SideIdx < NumSides; SideIdx++)
	{
		const int32 a0start = (SideIdx + 0) * (NumRings + 1);
		const int32 a1start = (SideIdx + 1) * (NumRings + 1);
		for (int32 RingIdx = 0; RingIdx < NumRings; RingIdx++)
		{
			// First Tri (reverse winding)
			Indices[CurIndex+0] = a0start + RingIdx + 0;
			Indices[CurIndex+2] = a1start + RingIdx + 0;
			Indices[CurIndex+1] = a0start + RingIdx + 1;
			CurIndex += 3;
			// Second Tri (reverse winding)
			Indices[CurIndex+0] = a1start + RingIdx + 0;
			Indices[CurIndex+2] = a1start + RingIdx + 1;
			Indices[CurIndex+1] = a0start + RingIdx + 1;
			CurIndex += 3;
		}
	}

	//
	// Create the Sphyl Mesh in houdini
	//
	HAPI_NodeId SphylNodeId = -1;
	FString SphylName = TEXT("Sphyl") + FString::FromInt(ColliderIndex);
	if(!CreateInputNodeForCollider(SphylNodeId, InParentNodeID, ColliderIndex, SphylName, Vertices, Indices))
		return false;

	// Create a group node
	FString GroupNodeName = TEXT("group") + FString::FromInt(ColliderIndex);
	HAPI_NodeId GroupNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, TEXT("groupcreate"), GroupNodeName, false, &GroupNodeId), false);

	// Set its group name param to collision_geo_simple_box
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId parmId = FHoudiniEngineUtils::HapiFindParameterByName(GroupNodeId, "groupname", ParmInfo);

	FString LODGroup = TEXT("collision_geo_simple_capsule") + FString::FromInt(ColliderIndex);

	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, H_TCHAR_TO_UTF8(*LODGroup), parmId, 0);

	// Connect the box to the group
	FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, SphylNodeId, 0);

	OutNodeId = GroupNodeId;
	
	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForConvex(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId InParentNodeID,
	const int32 ColliderIndex,
	const FKConvexElem& ConvexCollider)
{
	TArray<float> Vertices;
	TArray<int32> Indices;

	FTransform ConvexTransform = ConvexCollider.GetTransform();

	FVector3d TransformOffset = ConvexTransform.GetLocation();
	FVector3d ScaleOffset = ConvexTransform.GetScale3D();
	FQuat4d RotationOffset = ConvexTransform.GetRotation();

#if ENGINE_MINOR_VERSION < 1
	//UE5.0 PhysX/Chaos
#if PHYSICS_INTERFACE_PHYSX
	if (ConvexCollider.GetConvexMesh() || ConvexCollider.GetMirroredConvexMesh())
#elif WITH_CHAOS
	if (ConvexCollider.IndexData.Num() > 0 && ConvexCollider.IndexData.Num() % 3 == 0)
#else
	if(false)
#endif
#else
	if (ConvexCollider.IndexData.Num() > 0 && ConvexCollider.IndexData.Num() % 3 == 0)
#endif
	{
		// Get the convex colliders vertices and indices from the mesh
		TArray<FDynamicMeshVertex> VertexBuffer;
		TArray<uint32> IndexBuffer;
		ConvexCollider.AddCachedSolidConvexGeom(VertexBuffer, IndexBuffer, FColor::White);

		for (int32 i = 0; i < VertexBuffer.Num(); i++)
		{
			VertexBuffer[i].Position =  (FVector3f)(TransformOffset + (RotationOffset * (ScaleOffset * (FVector3d)VertexBuffer[i].Position)));
		}

		Vertices.SetNum(VertexBuffer.Num() * 3);
		int32 CurIndex = 0;
		for (auto& CurVert : VertexBuffer)
		{
			Vertices[CurIndex * 3 + 0] = (float)(CurVert.Position.X) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 1] = (float)(CurVert.Position.Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 2] = (float)(CurVert.Position.Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			CurIndex++;
		}
		
		Indices.SetNum(IndexBuffer.Num());
		for (int Idx = 0; (Idx + 2) < IndexBuffer.Num(); Idx += 3)
		{
			// Reverse winding
			Indices[Idx + 0] = Indices[Idx + 0];
			Indices[Idx + 2] = Indices[Idx + 1];
			Indices[Idx + 1] = Indices[Idx + 2];
		}
	}
	else
	{
		// Need to copy vertices because we plan on modifying it by Quaternion/Vector multiplication
		TArray<FVector> VertexBuffer;
		VertexBuffer.SetNum(ConvexCollider.VertexData.Num());

		for (int32 Idx = 0; Idx < ConvexCollider.VertexData.Num(); Idx++)
		{
			VertexBuffer[Idx] = TransformOffset + (RotationOffset * (ScaleOffset * ConvexCollider.VertexData[Idx]));
		}
		
		int32 NumVert = ConvexCollider.VertexData.Num();
		Vertices.SetNum(NumVert * 3);
		//Indices.SetNum(NumVert);
		int32 CurIndex = 0;
		for (auto& CurVert : VertexBuffer)
		{
			Vertices[CurIndex * 3 + 0] = (float)(CurVert.X) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 1] = (float)(CurVert.Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 2] = (float)(CurVert.Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION;

			/*
			// TODO: Get proper polygons...
			Indices[CurIndex] = CurIndex;
			*/
			CurIndex++;
		}
		
		// TODO: Get Proper polygons
		for (int32 Idx = 0; Idx + 2 < NumVert; Idx++)
		{
			Indices.Add(Idx + 0);
			Indices.Add(Idx + 1);
			Indices.Add(Idx + 2);
		}
	}

	//
	// Create the Convex Mesh in houdini
	//
	HAPI_NodeId ConvexNodeId = -1;
	FString ConvexName = TEXT("Convex") + FString::FromInt(ColliderIndex);
	if (!CreateInputNodeForCollider(ConvexNodeId, InParentNodeID, ColliderIndex, ConvexName, Vertices, Indices))
		return false;

	//HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	//FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), ColliderNodeId, &CookOptions);

	// Create a group node
	FString GroupNodeName = TEXT("group") + FString::FromInt(ColliderIndex);
	HAPI_NodeId GroupNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, "groupcreate", GroupNodeName, false, &GroupNodeId), false);

	// Set its group name param to collision_geo_simple_ucx
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId parmId = FHoudiniEngineUtils::HapiFindParameterByName(GroupNodeId, "groupname", ParmInfo);

	FString LODGroup = TEXT("collision_geo_simple_ucx") + FString::FromInt(ColliderIndex);

	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, H_TCHAR_TO_UTF8(*LODGroup), parmId, 0);

	// Create a convex hull (shrinkwrap::2.0) node to fix the lack of proper indices
	HAPI_NodeId ConvexHullNodeId = -1;	
	FString ConvexHullName = TEXT("ConvexHull") + FString::FromInt(ColliderIndex);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, "shrinkwrap::2.0", ConvexHullName, false, &ConvexHullNodeId), false);

	if (ConvexHullNodeId > 0)
	{
		// Connect the collider to the convex hull
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(), ConvexHullNodeId, 0, ConvexNodeId, 0), false);

		// Connect the convex hull to the group
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, ConvexHullNodeId, 0), false);
	}
	else
	{	
		// Connect the collider to the group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(			
			FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, ConvexNodeId, 0), false);

	}

	OutNodeId = GroupNodeId;

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForCollider(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId InParentNodeID,
	const int32 ColliderIndex,
	const FString& ColliderName,
	const TArray<float>& ColliderVertices,
	const TArray<int32>& ColliderIndices)
{
	// Create a new input node for the collider in this input object's OBJ node
	HAPI_NodeId ColliderNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, "null", H_TCHAR_TO_UTF8(*ColliderName), false, &ColliderNodeId), false);

	// Create a part
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = ColliderIndices.Num();
	Part.faceCount = ColliderIndices.Num() / 3;
	Part.pointCount = ColliderVertices.Num() / 3;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), ColliderNodeId, 0, &Part), false);

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
	AttributeInfoPoint.count = ColliderVertices.Num() / 3;
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		ColliderNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	// Upload the positions
	FHoudiniHapiAccessor Accessor(ColliderNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, ColliderVertices), false);

	// Upload the indices
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
		ColliderIndices, ColliderNodeId, 0), false);

	// Generate the array of face counts.
	TArray<int32> ColliderFaceCounts;
	ColliderFaceCounts.SetNumUninitialized(Part.faceCount);
	for (int32 n = 0; n < Part.faceCount; n++)
		ColliderFaceCounts[n] = 3;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
		ColliderFaceCounts, ColliderNodeId, 0), false);

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(ColliderNodeId), false);

	OutNodeId = ColliderNodeId;

	return true;
}

bool 
FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
	const int32 NodeId,
	const int32 PartId,
	const int32 Count,
	const FHoudiniEngineIndexedStringMap& TriangleMaterials,
	const TArray<int> & MaterialSlotIndices,
	const TMap<FString, TArray<float>>& ScalarMaterialParameters,
	const TMap<FString, TArray<float>>& VectorMaterialParameters,
    const TMap<FString, FHoudiniEngineIndexedStringMap>& TextureMaterialParameters,
	const TMap<FString, TArray<int8>>& BoolMaterialParameters,
    const TOptional<FString> PhysicalMaterial,
	const TOptional<FMeshNaniteSettings> InNaniteSettings)
{
    H_SCOPED_FUNCTION_TIMER();

	if (NodeId < 0)
		return false;

	bool bSuccess = true;

	// Create attribute for material slot
	HAPI_AttributeInfo AttributeInfoMaterialSlot;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterialSlot);
	AttributeInfoMaterialSlot.tupleSize = 1;
	AttributeInfoMaterialSlot.count = Count;
	AttributeInfoMaterialSlot.exists = true;
	AttributeInfoMaterialSlot.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoMaterialSlot.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoMaterialSlot.originalOwner = HAPI_ATTROWNER_INVALID;

	// Create the new attribute
	if(HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL_SLOT, &AttributeInfoMaterialSlot))
	{
		// The New attribute has been successfully created, set its value
		FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL_SLOT);
		bSuccess &= Accessor.SetAttributeData(AttributeInfoMaterialSlot, MaterialSlotIndices);
	}


	// Create attribute for materials.
	HAPI_AttributeInfo AttributeInfoMaterial;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
	AttributeInfoMaterial.tupleSize = 1;
	AttributeInfoMaterial.count = Count;
	AttributeInfoMaterial.exists = true;
	AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

	// Create the new attribute
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoMaterial))
	{
		// The New attribute has been successfully created, set its value
		FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL);
		bSuccess &= Accessor.SetAttributeStringMap(AttributeInfoMaterial, TriangleMaterials);
	}

	// Add scalar material parameter attributes
	for (auto& Pair : ScalarMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;
		FHoudiniEngineUtils::SanitizeHAPIVariableName(CurMaterialParamAttribName);

		// Create attribute for material parameter.
		HAPI_AttributeInfo AttributeInfoMaterialParameter;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterialParameter);
		AttributeInfoMaterialParameter.tupleSize = 1;
		AttributeInfoMaterialParameter.count = Count;
		AttributeInfoMaterialParameter.exists = true;
		AttributeInfoMaterialParameter.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterialParameter.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoMaterialParameter.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName), &AttributeInfoMaterialParameter))
		{
			// The New attribute has been successfully created, set its value

			FHoudiniHapiAccessor Accessor(NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName));

			bSuccess &= Accessor.SetAttributeData(AttributeInfoMaterialParameter, Pair.Value);
		}
	}

	// Add vector material parameters
	for (auto& Pair : VectorMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;
		FHoudiniEngineUtils::SanitizeHAPIVariableName(CurMaterialParamAttribName);

		// Create attribute for material parameter.
		HAPI_AttributeInfo AttributeInfoMaterialParameter;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterialParameter);
		AttributeInfoMaterialParameter.tupleSize = 4;
		AttributeInfoMaterialParameter.count = Count;
		AttributeInfoMaterialParameter.exists = true;
		AttributeInfoMaterialParameter.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterialParameter.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoMaterialParameter.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName), &AttributeInfoMaterialParameter))
		{
			// The New attribute has been successfully created, set its value
			FHoudiniHapiAccessor Accessor(NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName));
			bSuccess &= Accessor.SetAttributeData(AttributeInfoMaterialParameter, Pair.Value);
		}
	}

	// Add texture material parameter attributes
	for (auto& Pair : TextureMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;
		FHoudiniEngineUtils::SanitizeHAPIVariableName(CurMaterialParamAttribName);

		// Create attribute for material parameter.
		HAPI_AttributeInfo AttributeInfoMaterialParameter;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterialParameter);
		AttributeInfoMaterialParameter.tupleSize = 1;
		AttributeInfoMaterialParameter.count = Count;
		AttributeInfoMaterialParameter.exists = true;
		AttributeInfoMaterialParameter.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterialParameter.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterialParameter.originalOwner = HAPI_ATTROWNER_INVALID;

		const FHoudiniEngineIndexedStringMap & StringMap = Pair.Value;
		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName), &AttributeInfoMaterialParameter))
		{
			// The New attribute has been successfully created, set its value
			FHoudiniHapiAccessor Accessor(NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName));
			bSuccess = Accessor.SetAttributeStringMap(AttributeInfoMaterialParameter, StringMap);
		}
	}

	// Add bool material parameter attributes
	for (auto& Pair : BoolMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;
		FHoudiniEngineUtils::SanitizeHAPIVariableName(CurMaterialParamAttribName);

		// Create attribute for material parameter.
		HAPI_AttributeInfo AttributeInfoMaterialParameter;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterialParameter);
		AttributeInfoMaterialParameter.tupleSize = 1;
		AttributeInfoMaterialParameter.count = Count;
		AttributeInfoMaterialParameter.exists = true;
		AttributeInfoMaterialParameter.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterialParameter.storage = HAPI_STORAGETYPE_INT8;
		AttributeInfoMaterialParameter.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName), &AttributeInfoMaterialParameter))
		{
			FHoudiniHapiAccessor Accessor(NodeId, 0, TCHAR_TO_ANSI(*CurMaterialParamAttribName));
			bSuccess &= Accessor.SetAttributeData(AttributeInfoMaterialParameter, Pair.Value);
		}
	}

    if (PhysicalMaterial.IsSet() && !PhysicalMaterial->IsEmpty())
    {
		// Create attribute for physical materials.
		HAPI_AttributeInfo AttributeInfoPhysicalMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPhysicalMaterial);
		AttributeInfoPhysicalMaterial.tupleSize = 1;
		AttributeInfoPhysicalMaterial.count = Count;
		AttributeInfoPhysicalMaterial.exists = true;
		AttributeInfoPhysicalMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoPhysicalMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPhysicalMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
		    FHoudiniEngine::Get().GetSession(),
		    NodeId, PartId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL, &AttributeInfoPhysicalMaterial))
		{
		    // The New attribute has been successfully created, set its value
			FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL);
			bSuccess &= Accessor.SetAttributeUniqueData(AttributeInfoPhysicalMaterial, PhysicalMaterial.GetValue());
		}
    }

	// Add the nanite attributes if needed
	bool bNaniteEnabled = false;
	if (InNaniteSettings.IsSet() && InNaniteSettings->bEnabled)
	{
		// Create an attribute for nanite enabled
		HAPI_AttributeInfo AttributeInfoNanite;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoNanite);
		AttributeInfoNanite.tupleSize = 1;
		AttributeInfoNanite.count = Count;
		AttributeInfoNanite.exists = true;
		AttributeInfoNanite.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoNanite.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoNanite.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_ENABLED, &AttributeInfoNanite))
		{
			FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_ENABLED);
			bSuccess &= Accessor.SetAttributeUniqueData(AttributeInfoNanite, 1);
		}

		// Create an attribute for nanite position precision
		//HAPI_AttributeInfo AttributeInfoNanite;
		
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoNanite);
		AttributeInfoNanite.tupleSize = 1;
		AttributeInfoNanite.count = Count;
		AttributeInfoNanite.exists = true;
		AttributeInfoNanite.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoNanite.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoNanite.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_POSITION_PRECISION, &AttributeInfoNanite))
		{
			// The New attribute has been successfully created, set its value
			int32 PositionPrecision = InNaniteSettings->PositionPrecision;

			FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_POSITION_PRECISION);
			bSuccess &= Accessor.SetAttributeUniqueData(AttributeInfoNanite, InNaniteSettings->PositionPrecision);
		}

		// Create an attribute for nanite percent triangle
		//HAPI_AttributeInfo AttributeInfoNanite;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoNanite);
		AttributeInfoNanite.tupleSize = 1;
		AttributeInfoNanite.count = Count;
		AttributeInfoNanite.exists = true;
		AttributeInfoNanite.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoNanite.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoNanite.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_PERCENT_TRIANGLES, &AttributeInfoNanite))
		{
			// The New attribute has been successfully created, set its value
			float KeepPercentTriangles = InNaniteSettings->KeepPercentTriangles;

			FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_PERCENT_TRIANGLES);
			bSuccess &= Accessor.SetAttributeUniqueData(AttributeInfoNanite, KeepPercentTriangles);
		}

		// Create an attribute for nanite fb relative error
		//HAPI_AttributeInfo AttributeInfoNanite;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoNanite);
		AttributeInfoNanite.tupleSize = 1;
		AttributeInfoNanite.count = Count;
		AttributeInfoNanite.exists = true;
		AttributeInfoNanite.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoNanite.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoNanite.originalOwner = HAPI_ATTROWNER_INVALID;
		
		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_FB_RELATIVE_ERROR, &AttributeInfoNanite))
		{
			// The New attribute has been successfully created, set its value
			float FallbackRelativeError = InNaniteSettings->FallbackRelativeError;

			FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_FB_RELATIVE_ERROR);
			bSuccess &= Accessor.SetAttributeUniqueData(AttributeInfoNanite, FallbackRelativeError);
		}

		// Create an attribute for nanite trim relative error
		//HAPI_AttributeInfo AttributeInfoNanite;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoNanite);
		AttributeInfoNanite.tupleSize = 1;
		AttributeInfoNanite.count = Count;
		AttributeInfoNanite.exists = true;
		AttributeInfoNanite.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoNanite.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoNanite.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_TRIM_RELATIVE_ERROR, &AttributeInfoNanite))
		{
			// The New attribute has been successfully created, set its value
			float TrimRelativeError = InNaniteSettings->TrimRelativeError;

			FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_NANITE_TRIM_RELATIVE_ERROR);
			bSuccess &= Accessor.SetAttributeUniqueData(AttributeInfoNanite, TrimRelativeError);
		}
	}

	return bSuccess;
}

bool FUnrealMeshTranslator::ExportCollisions(
	int32& NextMergeIndex,
	const UStaticMesh* StaticMesh,
	const HAPI_NodeId MergeNodeId,
	const HAPI_NodeId InputObjectNodeId,
	const FKAggregateGeom& SimpleColliders)
{
	// If there are no simple colliders to create then skip this bodysetup
	if(SimpleColliders.BoxElems.Num() + SimpleColliders.SphereElems.Num() + SimpleColliders.SphylElems.Num()
		+ SimpleColliders.ConvexElems.Num() > 0)
	{
		HAPI_NodeId CollisionMergeNodeId = -1;
		int32 NextCollisionMergeIndex = 0;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
			InputObjectNodeId, TEXT("merge"), TEXT("simple_colliders_merge") + FString::FromInt(NextMergeIndex), false, &CollisionMergeNodeId), false);

		// Export BOX colliders
		for(auto& CurBox : SimpleColliders.BoxElems)
		{
			FVector BoxCenter = CurBox.Center;
			FVector BoxExtent = FVector(CurBox.X, CurBox.Y, CurBox.Z);
			FRotator BoxRotation = CurBox.Rotation;

			HAPI_NodeId BoxNodeId = -1;
			if(!CreateInputNodeForBox(
				BoxNodeId, InputObjectNodeId, NextCollisionMergeIndex,
				BoxCenter, BoxExtent, BoxRotation))
				continue;

			if(BoxNodeId < 0)
				continue;

			// Connect the Box node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				CollisionMergeNodeId, NextCollisionMergeIndex, BoxNodeId, 0), false);

			NextCollisionMergeIndex++;
		}

		// Export SPHERE colliders
		for(auto& CurSphere : SimpleColliders.SphereElems)
		{
			HAPI_NodeId SphereNodeId = -1;
			if(!CreateInputNodeForSphere(
				SphereNodeId, InputObjectNodeId, NextCollisionMergeIndex,
				CurSphere.Center, CurSphere.Radius))
				continue;

			if(SphereNodeId < 0)
				continue;

			// Connect the Sphere node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				CollisionMergeNodeId, NextCollisionMergeIndex, SphereNodeId, 0), false);

			NextCollisionMergeIndex++;
		}

		// Export CAPSULE colliders
		for(auto& CurSphyl : SimpleColliders.SphylElems)
		{
			HAPI_NodeId SphylNodeId = -1;
			if(!CreateInputNodeForSphyl(
				SphylNodeId, InputObjectNodeId, NextCollisionMergeIndex,
				CurSphyl.Center, CurSphyl.Rotation, CurSphyl.Radius, CurSphyl.Length))
				continue;

			if(SphylNodeId < 0)
				continue;

			// Connect the capsule node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				CollisionMergeNodeId, NextCollisionMergeIndex, SphylNodeId, 0), false);

			NextCollisionMergeIndex++;
		}

		// Export CONVEX colliders
		for(auto& CurConvex : SimpleColliders.ConvexElems)
		{
			HAPI_NodeId ConvexNodeId = -1;
			if(!CreateInputNodeForConvex(
				ConvexNodeId, InputObjectNodeId, NextCollisionMergeIndex, CurConvex))
				continue;

			if(ConvexNodeId < 0)
				continue;

			// Connect the capsule node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				CollisionMergeNodeId, NextCollisionMergeIndex, ConvexNodeId, 0), false);

			NextCollisionMergeIndex++;
		}

		// Create a new Attribute Wrangler node which will be used to create the new attributes.
		HAPI_NodeId AttribWrangleNodeId;
		if(FHoudiniEngineUtils::CreateNode(
			InputObjectNodeId, TEXT("attribwrangle"),
			TEXT("physical_material"),
			true, &AttribWrangleNodeId) != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the node.
			HOUDINI_LOG_WARNING(
				TEXT("Failed to create Physical Material attribute for mesh: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
			return true;
		}

		// Connect the new node to the previous node. Set CollisionMergeNodeId to the attrib node
		// as is this the final output of the chain.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			AttribWrangleNodeId, 0, CollisionMergeNodeId, 0), false);
		CollisionMergeNodeId = AttribWrangleNodeId;

		// Set the wrangle's class to primitives
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), AttribWrangleNodeId, "class", 0, 1), false);

		// Create a Vex expression, add the mesh input name.

		const FString FormatString = TEXT("s@{0} = '{1}';\n");
		FString PathName = StaticMesh->GetPathName();
		FString AttrName = TEXT(HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME);
		std::string VEXpression = H_TCHAR_TO_UTF8(*FString::Format(*FormatString, { AttrName, PathName }));

		// Create a new primitive attribute where each value contains the Physical Material
		// mae in Unreal.
		UPhysicalMaterial* PhysicalMaterial = StaticMesh->GetBodySetup()->PhysMaterial;
		if(PhysicalMaterial)
		{

			// Construct a VEXpression to set create and set a Physical Material Attribute.
			// eg. s@unreal_physical_material = 'MyPath/PhysicalMaterial';
			PathName = PhysicalMaterial->GetPathName();
			AttrName = TEXT(HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL);
			VEXpression += H_TCHAR_TO_UTF8(*FString::Format(*FormatString, { AttrName, PathName }));
		}

		// Set the snippet parameter to the VEXpression.
		HAPI_ParmInfo ParmInfo;
		HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(AttribWrangleNodeId, "snippet", ParmInfo);
		if(ParmId != -1)
		{
			FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), AttribWrangleNodeId,
				VEXpression.c_str(), ParmId, 0);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}

		// Connect our collision merge node (or the phys mat attrib wrangle) to the main merge node
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			MergeNodeId, NextMergeIndex, CollisionMergeNodeId, 0), false);
		NextMergeIndex++;
	}
	return true;
}


bool FUnrealMeshTranslator::GetMaterialInfo(
	const TArray<UMaterialInterface*>& Materials,
	TArray<FUnrealMaterialInfo>& OutMaterialInfos)
{
	H_SCOPED_FUNCTION_TIMER();

	OutMaterialInfos.SetNum(Materials.Num());

	// We have materials.
	for(int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); MaterialIdx++)
	{
		FString ParamPrefix = HAPI_UNREAL_ATTRIB_GENERIC_MAT_PARAM_PREFIX;
		ParamPrefix += Materials.Num() == 1 ? "" : FString::FromInt(MaterialIdx) + FString("_");

		UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
		if(!MaterialInterface)
			continue;

		FUnrealMaterialInfo& MaterialInfo = OutMaterialInfos[MaterialIdx];

		MaterialInfo.MaterialPath = MaterialInterface->GetPathName();

		// Collect all scalar parameters in this material
		{
			TArray<FMaterialParameterInfo> MaterialScalarParamInfos;
			TArray<FGuid> MaterialScalarParamGuids;
			MaterialInterface->GetAllScalarParameterInfo(MaterialScalarParamInfos, MaterialScalarParamGuids);

			for(auto& CurScalarParam : MaterialScalarParamInfos)
			{
				FString CurScalarParamName = ParamPrefix + CurScalarParam.Name.ToString();
				float CurScalarVal;
				MaterialInterface->GetScalarParameterValue(CurScalarParam, CurScalarVal);
				MaterialInfo.ScalarParameters.Add(CurScalarParamName, CurScalarVal);
			}
		}

		// Collect all vector parameters in this material
		{
			TArray<FMaterialParameterInfo> MaterialVectorParamInfos;
			TArray<FGuid> MaterialVectorParamGuids;
			MaterialInterface->GetAllVectorParameterInfo(MaterialVectorParamInfos, MaterialVectorParamGuids);

			for(auto& CurVectorParam : MaterialVectorParamInfos)
			{
				FString CurVectorParamName = ParamPrefix + CurVectorParam.Name.ToString();
				FLinearColor CurVectorValue;
				MaterialInterface->GetVectorParameterValue(CurVectorParam, CurVectorValue);
				MaterialInfo.VectorParameters.Add(CurVectorParamName, CurVectorValue);
			}
		}

		// Collect all texture parameters in this material
		{
			TArray<FMaterialParameterInfo> MaterialTextureParamInfos;
			TArray<FGuid> MaterialTextureParamGuids;
			MaterialInterface->GetAllTextureParameterInfo(MaterialTextureParamInfos, MaterialTextureParamGuids);

			for(auto& CurTextureParam : MaterialTextureParamInfos)
			{
				FString CurTextureParamName = ParamPrefix + CurTextureParam.Name.ToString();
				UTexture* CurTexture = nullptr;
				MaterialInterface->GetTextureParameterValue(CurTextureParam, CurTexture);

				FString TexturePath = IsValid(CurTexture) ? CurTexture->GetPathName() : TEXT("");

				MaterialInfo.TextureParameters.Add(CurTextureParamName, TexturePath);
			}
		}

		// Collect all bool parameters in this material
		{
			TArray<FMaterialParameterInfo> MaterialBoolParamInfos;
			TArray<FGuid> MaterialBoolParamGuids;
			MaterialInterface->GetAllStaticSwitchParameterInfo(MaterialBoolParamInfos, MaterialBoolParamGuids);

			for(auto& CurBoolParam : MaterialBoolParamInfos)
			{
				FString CurBoolParamName = ParamPrefix + CurBoolParam.Name.ToString();
				bool CurBool = false;
				FGuid CurExprValue;
				MaterialInterface->GetStaticSwitchParameterValue(CurBoolParam, CurBool, CurExprValue);

				MaterialInfo.BoolParameters.Add(CurBoolParamName, CurBool);
			}
		}
	}

	return true;
}


bool FUnrealMeshTranslator::GetOrCreateMaterialTableNode(
	FUnrealMeshExportData& ExportData,
	const TArray<FUnrealMaterialInfo>& MaterialInfos)
{
	// Get or create the GeoNode.
	bool bCreated = false;
	HAPI_NodeId GeoNodeId = ExportData.GetOrCreateConstructionGeoNode(bCreated, MaterialTableName, EUnrealObjectInputNodeType::Leaf);
	if(GeoNodeId == INDEX_NONE)
		return false;

	// If we already created the geo node, we don't have to recreate the internal nodes.
	if(!bCreated)
		return true;

	HAPI_NodeId MaterialNodeId;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(GeoNodeId, TEXT("null"), TEXT("material_node"), false, &MaterialNodeId), false);

	ExportData.RegisterConstructionNode(MaterialTableName, MaterialNodeId);

	// Create part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.pointCount = MaterialInfos.Num();
	Part.vertexCount = 3 * MaterialInfos.Num();
	Part.faceCount = MaterialInfos.Num();
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, &Part), false);

	{
		// Create POS point attribute info. We won't use it.
		HAPI_AttributeInfo AttributeInfoPos;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPos);
		AttributeInfoPos.count = 3;
		AttributeInfoPos.tupleSize = 3;
		AttributeInfoPos.exists = true;
		AttributeInfoPos.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPos.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPos.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPos), false);

		TArray<float> Positions;
		Positions.SetNumZeroed(AttributeInfoPos.count * AttributeInfoPos.tupleSize);

		FHoudiniHapiAccessor Accessor(MaterialNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		Accessor.SetAttributeData(AttributeInfoPos, Positions);
	}


	TArray<int> VertexListData;
	VertexListData.SetNumUninitialized(Part.vertexCount);
	for (int Index = 0; Index < VertexListData.Num(); Index++)
	{
		VertexListData[Index] = Index % 3;
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(VertexListData, MaterialNodeId, 0), false);

	TArray<int32> StaticMeshFaceCounts;
	StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);

	for(int32 n = 0; n < Part.faceCount; n++)
		StaticMeshFaceCounts[n] = 3;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(StaticMeshFaceCounts, MaterialNodeId, 0), false);

	{
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = MaterialInfos.Num();
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_INT;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		TArray<int> MaterialSlots;
		MaterialSlots.SetNum(MaterialInfos.Num());
		for(int Index = 0; Index < MaterialSlots.Num(); Index++)
			MaterialSlots[Index] = Index;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_SLOT, &AttributeInfo), false);

		FHoudiniHapiAccessor Accessor(MaterialNodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_SLOT);
		Accessor.SetAttributeData(AttributeInfo, MaterialSlots);
	}

	for (int MaterialIndex = 0; MaterialIndex < MaterialInfos.Num(); MaterialIndex++)
	{
		const FUnrealMaterialInfo& MaterialInfo = MaterialInfos[MaterialIndex];

		for (auto It : MaterialInfo.ScalarParameters)
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = MaterialInfos.Num();
			AttributeInfo.tupleSize = 1;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			FString AttributeName = *It.Key;

			FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, H_TCHAR_TO_UTF8(*AttributeName), &AttributeInfo), false);

			FHoudiniHapiAccessor Accessor(MaterialNodeId, 0, *AttributeName);
			Accessor.SetAttributeData(AttributeInfo,&It.Value, MaterialIndex, 1);
		}

		for(auto It : MaterialInfo.VectorParameters)
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = MaterialInfos.Num();
			AttributeInfo.tupleSize = 4;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			FString AttributeName = *It.Key;
			FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, H_TCHAR_TO_UTF8(*AttributeName), &AttributeInfo), false);

			float Values[4];
			Values[0] = It.Value.R;
			Values[1] = It.Value.G;
			Values[2] = It.Value.B;
			Values[3] = It.Value.A;

			FHoudiniHapiAccessor Accessor(MaterialNodeId, 0, *AttributeName);
			Accessor.SetAttributeData(AttributeInfo, Values, MaterialIndex, 1);
		}

		for(auto It : MaterialInfo.BoolParameters)
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = MaterialInfos.Num();
			AttributeInfo.tupleSize = 1;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT8;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			FString AttributeName = *It.Key;
			FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, H_TCHAR_TO_UTF8(*AttributeName), &AttributeInfo), false);

			int8 Value = It.Value ? 1 : 0;
			FHoudiniHapiAccessor Accessor(MaterialNodeId, 0, *AttributeName);
			Accessor.SetAttributeData(AttributeInfo, &Value, MaterialIndex, 1);
		}

		for(auto It : MaterialInfo.TextureParameters)
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = MaterialInfos.Num();
			AttributeInfo.tupleSize = 1;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			FString AttributeName = *It.Key;
			FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), MaterialNodeId, 0, H_TCHAR_TO_UTF8(*AttributeName), &AttributeInfo), false);

			FHoudiniHapiAccessor Accessor(MaterialNodeId, 0, *AttributeName);
			Accessor.SetAttributeData(AttributeInfo, &It.Value, MaterialIndex, 1);
		}
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(MaterialNodeId), {});

	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	if(!FHoudiniEngineUtils::HapiCookNode(MaterialNodeId, &CookOptions, true))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to cook node!"));
	}

	return true;
}

bool FUnrealMeshTranslator::GetOrCreateMaterialZipNode(
	HAPI_NodeId& AttribCopyNodeId,
	const HAPI_NodeId ParentNodeId,
	const HAPI_NodeId MeshNode,
	const HAPI_NodeId MaterialTableNode,
	const TArray<FUnrealMaterialInfo>& MaterialInfos)
{
	FStringBuilderBase AttributesToCopy;

	for (const FUnrealMaterialInfo & Info : MaterialInfos)
	{
		for(auto ScalarParam : Info.ScalarParameters)
		{
			AttributesToCopy.Append(ScalarParam.Key);
			AttributesToCopy.Append(TEXT(" "));
		}

		for(auto VectorParam : Info.VectorParameters)
		{
			AttributesToCopy.Append(VectorParam.Key);
			AttributesToCopy.Append(TEXT(" "));
		}

		for(auto TextureParam : Info.TextureParameters)
		{
			AttributesToCopy.Append(TextureParam.Key);
			AttributesToCopy.Append(TEXT(" "));
		}

		for(auto BoolParam : Info.BoolParameters)
		{
			AttributesToCopy.Append(BoolParam.Key);
			AttributesToCopy.Append(TEXT(" "));
		}
	}
	TArray<char> Attribs = HoudiniTCHARToUTF(AttributesToCopy.ToString());

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("attribcopy"), TEXT("attrib_copy"), false, &AttribCopyNodeId), false);

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HAPI_ParmId ParmId = -1;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, AttribCopyNodeId, "srcgrouptype", 0, 2), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, AttribCopyNodeId, "destgrouptype", 0, 2), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, AttribCopyNodeId, "matchbyattribute", 0, 1), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, AttribCopyNodeId, "matchbyattributemethod", 0, 1), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, AttribCopyNodeId, "attrib", 0, 2), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(Session, AttribCopyNodeId, "attributetomatch", &ParmId), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(Session, AttribCopyNodeId, HAPI_UNREAL_ATTRIB_MATERIAL_SLOT, ParmId, 0), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(Session, AttribCopyNodeId, "attribname", &ParmId), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(Session, AttribCopyNodeId, Attribs.GetData(), ParmId, 0), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, AttribCopyNodeId, 0, MeshNode, 0), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, AttribCopyNodeId, 1, MaterialTableNode, 0), false);

	return true;
}

bool FUnrealMeshExportData::ScanForExistingNodesInHoudini()
{
	// This function looks in the top level Geo node in Houdini to see which nodes already exist.
	// We currently do this on each Unreal->Houdini export, possibly we could keep track of this
	// per Mesh, but this also allows us to cache data from existing sessions?
	
	int ChildCount = 0;

	HAPI_NodeId ParentNodeId = GetConstructionSubnetNodeId();
	if(ParentNodeId == INDEX_NONE)
		return true;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(),
		ParentNodeId,
		HAPI_NODETYPE_ANY,
		HAPI_NODEFLAGS_ANY,
		false,
		&ChildCount), false);

	if(ChildCount == 0)
		return true;

	// Retrieve all the display node ids
	TArray<HAPI_NodeId> ChildNodeIds;
	ChildNodeIds.SetNum(ChildCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
		FHoudiniEngine::Get().GetSession(),
		ParentNodeId,
		ChildNodeIds.GetData(),
		ChildCount), false);

	// See what we have

	for(int ChildNodeId : ChildNodeIds)
	{
		FString NodeLabel;
		FHoudiniEngineUtils::GetHoudiniAssetName(ChildNodeId, NodeLabel);
		ExistingUnassignedHAPINodes.Add(NodeLabel, ChildNodeId);
	}
	return true;
}

bool FUnrealMeshTranslator::GetOrCreateStaticMeshLODGeometries(
	FUnrealMeshExportData& ExportData,
	const UStaticMesh* StaticMesh,
	const FUnrealMeshExportOptions& ExportOptions,
	EHoudiniMeshSource MeshSource)
{
	if(ExportOptions.bMainMesh)
	{
		FString Label = MakeLODName(0, MeshSource);

		if(!ExportData.Contains(Label))
		{
			const bool bAddLODGroups = ExportOptions.bLODs;
			GetOrCreateExportStaticMeshLOD(ExportData, 0, bAddLODGroups, StaticMesh, MeshSource);
		}
	}

	if(ExportOptions.bLODs)
	{
		int NumLODs = StaticMesh->GetNumLODs();

		for(int LODIndex = 0; LODIndex < NumLODs; LODIndex++)
		{
			FString NodeLabel = MakeLODName(LODIndex, MeshSource);
			if(!ExportData.Contains(NodeLabel))
			{
				const bool bAddLODGroups = true;
				GetOrCreateExportStaticMeshLOD(ExportData, LODIndex, bAddLODGroups, StaticMesh, MeshSource);
			}
		}
	}

	return true;
}


bool FUnrealMeshTranslator::CreateInputNodeForStaticMeshNew(
	HAPI_NodeId& InputObjectNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const UStaticMesh* StaticMesh,
	const UStaticMeshComponent* StaticMeshComponent,
	const FString& InputNodeName,
	const FUnrealMeshExportOptions& ExportOptions,
	const bool bInputNodesCanBeDeleted)
{
	FUnrealObjectInputHandle StaticMeshHandle;

	FHoudiniPerfTimer PerfTimer(TEXT("Create Static Mesh Input Nodes"), true);
	PerfTimer.Start();

	if (IsValid(StaticMeshComponent) && StaticMeshComponent->IsA<USplineMeshComponent>())
	{
		// Spline Mesh requires special handling, since its geometry is per-component.

		FUnrealObjectInputHandle ComponentHandle;

		bool bSuccess = CreateInputNodeForSplineMeshComponentNew(
			InputObjectNodeId,
			ComponentHandle,
			Cast<USplineMeshComponent>(StaticMeshComponent),
			ExportOptions,
			bInputNodesCanBeDeleted);

		if(bSuccess)
		{
			OutHandle = std::move(ComponentHandle);
		}
	}
	else
	{
		// Static Mesh with optional component.

		bool bSuccess = CreateInputNodeForStaticMeshNew(
			InputObjectNodeId,
			StaticMeshHandle,
			StaticMesh,
			InputNodeName,
			ExportOptions,
			bInputNodesCanBeDeleted);

		if(!bSuccess)
			return false;

		if(StaticMeshComponent)
		{
			FUnrealObjectInputHandle ComponentHandle;

			bSuccess = CreateInputNodeForStaticMeshComponentNew(
				InputObjectNodeId,
				ComponentHandle,
				StaticMeshHandle,
				StaticMeshComponent,
				InputNodeName,
				ExportOptions,
				bInputNodesCanBeDeleted);

			if(bSuccess)
			{
				OutHandle = ComponentHandle;
			}
		}
		else
		{
			OutHandle = StaticMeshHandle;
		}

	}

	return true;
}

bool FUnrealMeshTranslator::CreateInputNodeForStaticMeshNew(
	HAPI_NodeId& InputObjectNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const UStaticMesh* StaticMesh,
	const FString& InputNodeName,
	const FUnrealMeshExportOptions& ExportOptions,
	const bool bInputNodesCanBeDeleted)
{
	// ExportData contains information about the mesh being constructed.
	FUnrealMeshExportData ExportData(StaticMesh, bInputNodesCanBeDeleted);

	FString MeshLabel;
	bool bSuccess = GetOrConstructStaticMesh(MeshLabel, ExportData, ExportOptions, StaticMesh);
	if(!bSuccess || !ExportData.Contains(MeshLabel))
		return false;

	// Fetch the construction results.
	InputObjectNodeId = ExportData.GetHapiNodeId(MeshLabel);
	OutHandle = ExportData.GetNodeHandle(MeshLabel);

	return true;
}

FString FUnrealMeshExportData::CleanInputPath(const FString & ObjectPath)
{
	FString Path = ObjectPath.Replace(TEXT(":"), TEXT("/")).Replace(TEXT("."), TEXT("/"));
	return Path;
}

void FUnrealMeshExportData::EnsureConstructionSubnetExists()
{
	// Just add a dummy node to make sure parent exists.
	FString Path = ConstructionSubnetPath + TEXT("/Dummy");

	FUnrealObjectInputIdentifier TopLevelIdentifier = FUnrealObjectInputIdentifier(Path);
	FUnrealObjectInputUtils::EnsureParentsExist(TopLevelIdentifier, ConstructionSubnetHandle, bCanDelete);
	ConstructionSubnetNodeId = FUnrealObjectInputUtils::GetHAPINodeId(ConstructionSubnetHandle);
}

bool FUnrealMeshTranslator::GetOrCreateExportStaticMeshLOD(
	FUnrealMeshExportData& ExportData,
	const int LODIndex,
	const bool bAddLODGroups,
	const UStaticMesh* StaticMesh,
	const EHoudiniMeshSource RequestedMeshSource)

{
	FString LODName = MakeLODName(LODIndex, RequestedMeshSource);

	bool bCreated = false;
	HAPI_NodeId GeoNodeId = ExportData.GetOrCreateConstructionGeoNode(bCreated, LODName, EUnrealObjectInputNodeType::Leaf);
	if(GeoNodeId == INDEX_NONE)
		return false;

	// If the geo node already existed, don't recreate it.
	if(!bCreated)
		return true;

	HAPI_NodeId NodeId;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), GeoNodeId, "null", H_TCHAR_TO_UTF8(*LODName), true, &NodeId), false);

	ExportData.RegisterConstructionNode(LODName, NodeId);

	// Try to use the prefered mesh source. Not all options are available on every mesh, so provide fallbacks.
	EHoudiniMeshSource MeshSource = RequestedMeshSource;

	if (MeshSource == EHoudiniMeshSource::HiResMeshDescription)
	{
		if 	(StaticMesh->GetHiResMeshDescription() == nullptr)
		{
			// Something has gone wrong!
			HOUDINI_LOG_ERROR(TEXT("Bad Mesh Descriptor"));
			MeshSource = EHoudiniMeshSource::MeshDescription;
		}
	}

	if(MeshSource == EHoudiniMeshSource::MeshDescription)
	{
		if(StaticMesh->GetMeshDescription(LODIndex) == nullptr)
		{
			HOUDINI_LOG_MESSAGE(TEXT("No MeshDescription, falling back to LOD Resource. %s "), *StaticMesh->GetPathName());
			MeshSource = EHoudiniMeshSource::LODResource;
		}
	}

	bool bSuccess = false;

	switch(MeshSource)
	{
	case EHoudiniMeshSource::LODResource:
		bSuccess = FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources(
			NodeId,
			StaticMesh->GetLODForExport(LODIndex),
			LODIndex,
			bAddLODGroups,
			false,
			StaticMesh,
			nullptr);
		break;
	case EHoudiniMeshSource::MeshDescription:
		bSuccess = FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
			NodeId,
			*StaticMesh->GetMeshDescription(LODIndex),
			LODIndex,
			bAddLODGroups,
			false,
			StaticMesh,
			nullptr);
		break;

		case EHoudiniMeshSource::HiResMeshDescription:
		bSuccess = FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
			NodeId,
			*StaticMesh->GetHiResMeshDescription(),
			LODIndex,
			bAddLODGroups,
			false,
			StaticMesh,
			nullptr);
		break;

	default:
		break;
	}

	return bSuccess;
}

//TODO: Fix spline meshes


TArray<UMaterialInterface*> FUnrealMeshTranslator::GetMaterials(const UStaticMesh* StaticMesh)
{
	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

	TArray<UMaterialInterface*> Results;

	UMaterialInterface* UEDefaultMaterial = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);

	for(int32 MaterialIndex = 0; MaterialIndex < StaticMaterials.Num(); ++MaterialIndex)
	{
		const FStaticMaterial& MaterialInfo = StaticMaterials[MaterialIndex];
		UMaterialInterface* Material = MaterialInfo.MaterialInterface;

		// If the Material is NULL or invalid, fallback to the default material
		if(!IsValid(Material))
		{
			Material = UEDefaultMaterial;
			HOUDINI_LOG_WARNING(TEXT("Material Index %d (slot %s) has an invalid material, falling back to default: %s"), MaterialIndex, *(MaterialInfo.MaterialSlotName.ToString()), *(UEDefaultMaterial->GetPathName()));
		}
		// MaterialSlotToInterface.Add(MaterialInfo.ImportedMaterialSlotName, MaterialIndex);
		Results.Add(Material);
	}

	return Results;
}
bool FUnrealMeshTranslator::GetOrConstructStaticMeshGeometryNode(
	FString& GeometryLabel, 
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const UStaticMesh* StaticMesh)
{
	EHoudiniMeshSource MeshSource = DetermineMeshSource(ExportOptions, StaticMesh);

	// Create all low-level geometry nodes required by these export options. For example, lod0, lod1
	bool bSuccess = GetOrCreateStaticMeshLODGeometries(ExportData, StaticMesh, ExportOptions, MeshSource);
	if(!bSuccess)
		return false;

	// Constructs the geometry nodes for the current mesh, if needed. Usuall the main mesh and/or lods and/or
	// hires mesh.
	if (ExportOptions.bMainMesh && !ExportOptions.bLODs)
	{
		// If we are just constructing the main mesh and no LODs, we don't need to create an extra
		// merge node, just return the current node.

		GeometryLabel = MakeLODName(0, MeshSource);

		return true;
	}
	else if(ExportOptions.bLODs)
	{
		// Combine all LODs and return that node.

		GeometryLabel = TEXT("all_lods_") + MakeMeshSourceStr(MeshSource);

		if(ExportData.Contains(GeometryLabel))
			return true;

		TSet<FUnrealObjectInputHandle> NodeIds;

		// Add each LOD... ignore LOD0, if its needed it will already have been added.
		auto & Handles = ExportData.GetConstructionHandles();
		for (int LODIndex = 0; LODIndex < StaticMesh->GetNumLODs(); LODIndex++)
		{
			FString LODName = MakeLODName(LODIndex, MeshSource);
			if (Handles.Contains(LODName))
			{
				NodeIds.Add(Handles[LODName]);
			}
		}

		// Create the geo node, but if it already exists just re-use it.

		bool bCreated = false;
		HAPI_NodeId GeoNode = ExportData.GetOrCreateConstructionGeoNode(bCreated, GeometryLabel, EUnrealObjectInputNodeType::Reference);
		if(GeoNode == INDEX_NONE)
			return false;

		if(!bCreated)
			return true;

		HAPI_NodeId NodeId;
		bSuccess = CreateMergeNode(NodeId, GeometryLabel, GeoNode, GetHapiNodeIds(NodeIds.Array()));

		ExportData.RegisterConstructionNode(GeometryLabel, NodeId, &NodeIds);
		return bSuccess;
	}
	return false;
}

bool FUnrealMeshTranslator::GetOrConstructStaticMeshRenderNode(
	FString& RenderMeshLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const UStaticMesh* StaticMesh)
{

	// Get or create the geometry node for this set of export options.
	FString GeometryLabel;
	bool bSuccess = GetOrConstructStaticMeshGeometryNode(
		GeometryLabel,
		ExportData,
		ExportOptions,
		StaticMesh);

	if(!bSuccess)
		return false;

	if (ExportOptions.bMaterialParameters)
	{
		// Fetch Materials
		TArray<UMaterialInterface*> MaterialInterfaces = FUnrealMeshTranslator::GetMaterials(StaticMesh);
		TArray<FUnrealMaterialInfo> MaterialInfos;
		GetMaterialInfo(MaterialInterfaces, MaterialInfos);


		// Material Table.
		if(!ExportData.Contains(MaterialTableName))
		{
			GetOrCreateMaterialTableNode(ExportData, MaterialInfos);
		}

		// if we need material parameters create a new node and zip the geometry and materials
		FStringBuilderBase StringBuilder;
		StringBuilder.Append(GeometryLabel);
		StringBuilder.Append(TEXT("_mparams"));

		RenderMeshLabel = StringBuilder.ToString();

		TSet<FUnrealObjectInputHandle> References;
		References.Add(ExportData.GetNodeHandle(GeometryLabel));
		References.Add(ExportData.GetNodeHandle(MaterialTableName));

		// Get or create the geo node. If it already exists, don't recreate it.
		bool bCreated = false;
		HAPI_NodeId GeoNodeId = ExportData.GetOrCreateConstructionGeoNode(bCreated, RenderMeshLabel, EUnrealObjectInputNodeType::Reference);
		if(GeoNodeId == INDEX_NONE)
			return false;

		if(!bCreated)
			return true;

		HAPI_NodeId ZipNodeId;

		bSuccess = GetOrCreateMaterialZipNode(
			ZipNodeId,
			GeoNodeId,
			ExportData.GetHapiNodeId(GeometryLabel),
			ExportData.GetHapiNodeId(MaterialTableName),
			MaterialInfos);

		ExportData.RegisterConstructionNode(RenderMeshLabel, ZipNodeId, &References);

		return bSuccess;
	}
	else
	{
		RenderMeshLabel = GeometryLabel;
	}

	return true;
}

FString FUnrealMeshTranslator::MakeUniqueExportName(const FUnrealMeshExportOptions& ExportOptions)
{
	FStringBuilderBase LabelBuilder;
	LabelBuilder.Append("final");

	if(ExportOptions.bMainMesh)
		LabelBuilder.Append(TEXT("_main"));

	if(ExportOptions.bLODs)
		LabelBuilder.Append(TEXT("_lods"));

	if(ExportOptions.bColliders)
		LabelBuilder.Append(TEXT("_colliders"));

	if(ExportOptions.bSockets)
		LabelBuilder.Append(TEXT("_sockets"));

	if(ExportOptions.bPreferNaniteFallbackMesh)
		LabelBuilder.Append(TEXT("_nanite"));

	if(ExportOptions.bMaterialParameters)
		LabelBuilder.Append(TEXT("_materialparams"));

	return LabelBuilder.ToString();
}

bool FUnrealMeshTranslator::GetOrConstructStaticMesh(
	FString& MeshLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const UStaticMesh* StaticMesh)
{
	MeshLabel = MakeUniqueExportName(ExportOptions);

	// Get or create the geo node. Don't construct internal nodes if it already exists.
	bool bCreated = false;
	HAPI_NodeId GeoNode = ExportData.GetOrCreateConstructionGeoNode(bCreated, MeshLabel, EUnrealObjectInputNodeType::Reference);
	if(GeoNode == INDEX_NONE)
		return false;

	if(!bCreated)
		return true;

	TSet<FUnrealObjectInputHandle> ReferencedNodes;
	if(ExportOptions.bLODs || ExportOptions.bMainMesh)
	{
		FString RenderMesh;
		bool bSuccess = GetOrConstructStaticMeshRenderNode(RenderMesh, ExportData, ExportOptions, StaticMesh);
		if(!bSuccess)
			return false;

		ReferencedNodes.Add(ExportData.GetNodeHandle(RenderMesh));
	}

	if (ExportOptions.bColliders)
	{
		FString CollisionLabel;
		bool bSuccess = GetOrConstructCollisions(CollisionLabel, ExportData, ExportOptions, StaticMesh);
		if(!bSuccess)
			return false;

		ReferencedNodes.Add(ExportData.GetNodeHandle(CollisionLabel));
	}

	if(ExportOptions.bSockets)
	{
		FString SocketsLabel;
		bool bSuccess = GetOrConstructSockets(SocketsLabel, ExportData, ExportOptions, StaticMesh);
		if(!bSuccess)
			return false;

		ReferencedNodes.Add(ExportData.GetNodeHandle(SocketsLabel));
	}

	HAPI_NodeId NodeId;
	bool bSuccess = CreateMergeNode(NodeId, MeshLabel, GeoNode, GetHapiNodeIds(ReferencedNodes.Array()));

	ExportData.RegisterConstructionNode(MeshLabel, NodeId, &ReferencedNodes);

	if(!bSuccess || GeoNode == INDEX_NONE)
		return false;

	return bSuccess;
}

bool FUnrealMeshTranslator::GetOrConstructSplineMeshComponent(
	FString& MeshLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const USplineMeshComponent* SplineMeshComponent)
{
	UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
	if(!IsValid(StaticMesh))
		return true;

	MeshLabel = MakeUniqueExportName(ExportOptions);

	// Get or create the geo node. Don't construct internal nodes if it already exists.
	bool bCreated = false;
	HAPI_NodeId GeoNode = ExportData.GetOrCreateConstructionGeoNode(bCreated, MeshLabel, EUnrealObjectInputNodeType::Reference);
	if(GeoNode == INDEX_NONE)
		return false;

	if(!bCreated)
		return true;

	if(ExportOptions.bMaterialParameters)
	{
		// Fetch Materials
		TArray<UMaterialInterface*> MaterialInterfaces = FUnrealMeshTranslator::GetMaterials(StaticMesh);
		TArray<FUnrealMaterialInfo> MaterialInfos;
		GetMaterialInfo(MaterialInterfaces, MaterialInfos);

		// Material Table.
		if(!ExportData.Contains(MaterialTableName))
		{
			GetOrCreateMaterialTableNode(ExportData, MaterialInfos);
		}
	}

	TSet<FUnrealObjectInputHandle> ReferencedNodes;
	if(ExportOptions.bLODs || ExportOptions.bMainMesh)
	{
		FString RenderMesh;
		bool bSuccess = GetOrConstructSplineMeshRenderNode(RenderMesh, ExportData, ExportOptions, SplineMeshComponent);
		if(!bSuccess)
			return false;

		ReferencedNodes.Add(ExportData.GetNodeHandle(RenderMesh));
	}

	if(ExportOptions.bColliders)
	{
		FString CollisionLabel;
		bool bSuccess = GetOrConstructCollisions(CollisionLabel, ExportData, ExportOptions, StaticMesh);
		if(!bSuccess)
			return false;

		ReferencedNodes.Add(ExportData.GetNodeHandle(CollisionLabel));
	}

	if(ExportOptions.bSockets)
	{
		FString SocketsLabel;
		bool bSuccess = GetOrConstructSockets(SocketsLabel, ExportData, ExportOptions, StaticMesh);
		if(!bSuccess)
			return false;

		ReferencedNodes.Add(ExportData.GetNodeHandle(SocketsLabel));
	}

	HAPI_NodeId NodeId;
	bool bSuccess = CreateMergeNode(NodeId, MeshLabel, GeoNode, GetHapiNodeIds(ReferencedNodes.Array()));

	ExportData.RegisterConstructionNode(MeshLabel, NodeId, &ReferencedNodes);

	if(!bSuccess || GeoNode == INDEX_NONE)
		return false;

	return bSuccess;
	return true;
}

bool FUnrealMeshTranslator::CreateMergeNode(
	HAPI_NodeId& NodeId,
	const FString& NodeLabel,
	const HAPI_NodeId ParentNodeId,
	const TArray<HAPI_NodeId>& Inputs)
{

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("merge"), NodeLabel, true, &NodeId), false);

	for(int Index = 0; Index < Inputs.Num(); Index++)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(), NodeId, Index, Inputs[Index], 0), false);
	}

	return true;

}

FString FUnrealMeshTranslator::MakeMeshSourceStr(EHoudiniMeshSource Source)
{
	FString SourceString = TEXT("");
	switch(Source)
	{
	case EHoudiniMeshSource::LODResource:
		SourceString = TEXT("_lodresource");
		break;
	case EHoudiniMeshSource::MeshDescription:
		SourceString = TEXT("_meshdesc");
		break;
	case EHoudiniMeshSource::HiResMeshDescription:
		SourceString = TEXT("_hiresmeshdesc");
		break;
	default:
		SourceString = TEXT("");
		break;
	}
	return SourceString;
}

FString FUnrealMeshTranslator::MakeLODName(int LODIndex, EHoudiniMeshSource Source)
{
	FString SourceString = MakeMeshSourceStr(Source);
	FString Result = FString::Printf(TEXT("%s%d_%s"), *LODPrefix, LODIndex, *SourceString);
	return Result;
}


bool FUnrealMeshTranslator::GetOrConstructCollisions(
	FString& CollisionsLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const UStaticMesh* Mesh)
{
	CollisionsLabel = TEXT("collisions");

	bool bCreated = false;
	HAPI_NodeId GeoNode = ExportData.GetOrCreateConstructionGeoNode(bCreated, CollisionsLabel, EUnrealObjectInputNodeType::Leaf);
	if(GeoNode == INDEX_NONE)
		return false;

	if(!bCreated)
		return false;

	HAPI_NodeId MergeNodeId = INDEX_NONE;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(GeoNode, TEXT("merge"), *CollisionsLabel, true, &MergeNodeId), false);

	ExportData.RegisterConstructionNode(CollisionsLabel, MergeNodeId);

	int NextMergeIndex = 0;
	bool bSuccess = ExportCollisions(NextMergeIndex, Mesh, MergeNodeId, GeoNode, Mesh->GetBodySetup()->AggGeom);
	if(!bSuccess)
		return bSuccess;

	return bSuccess;
}

bool FUnrealMeshTranslator::GetOrConstructSockets(
	FString& SocketsLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const UStaticMesh* Mesh)
{
	SocketsLabel = TEXT("sockets");

	bool bCreated = false;
	HAPI_NodeId GeoNode = ExportData.GetOrCreateConstructionGeoNode(bCreated, SocketsLabel, EUnrealObjectInputNodeType::Leaf);
	if(GeoNode == INDEX_NONE)
		return false;

	if(!bCreated)
		return true;

	// Create an input node for the mesh sockets
	HAPI_NodeId SocketsNodeId = -1;
	bool bSuccess = CreateInputNodeForMeshSockets(Mesh->Sockets, GeoNode, SocketsNodeId);
	if(!bSuccess)
		return bSuccess;

	ExportData.RegisterConstructionNode(SocketsLabel, SocketsNodeId);

	return bSuccess;
}

bool FUnrealMeshExportData::Contains(const FString& Label)
{
	return RegisteredHandles.Contains(Label);
}


HAPI_NodeId FUnrealMeshExportData::GetHapiNodeId(const FString& Label)
{
	HAPI_NodeId NodeId = INDEX_NONE;

	FUnrealObjectInputUtils::GetHAPINodeId(RegisteredHandles[Label], NodeId);
	return NodeId;
}

const TMap<FString, FUnrealObjectInputHandle>& FUnrealMeshExportData::GetConstructionHandles()
{
	return RegisteredHandles;
}

FUnrealMeshExportData::FUnrealMeshExportData(const UObject* Object, bool bInCanDoDelete)
{
	FString ObjectPath = Object->GetPathName();
	bCanDelete = bInCanDoDelete;

	ConstructionSubnetPath = CleanInputPath(ObjectPath);

	EnsureConstructionSubnetExists();

	ScanForExistingNodesInHoudini();
}


HAPI_NodeId FUnrealMeshExportData::GetOrCreateConstructionGeoNode(
	bool& bCreated,
	const FString& Label,
	EUnrealObjectInputNodeType NodeType)
{
	bCreated = false;

	// Have we already seen this identifier and registered it? If so, just return it.
	if (RegisteredIdentifiers.Contains(Label))
	{
		FUnrealObjectInputIdentifier Identifier = RegisteredIdentifiers[Label];
		ensure(RegisteredHandles.Contains(Label));
		ensure(RegisteredGeoNodes.Contains(Label));

		HAPI_NodeId NodeId = INDEX_NONE;
		FUnrealObjectInputUtils::GetHAPINodeId(Identifier, NodeId);
		return NodeId;

	}

	FUnrealObjectInputIdentifier Identifier = MakeNodeIdentifier(Label, NodeType);
	RegisteredIdentifiers.Add(Label, Identifier);

	// Is there a HAPI node for this label which isn't registered? Is so, fetch the Handle and register it.
	if (ExistingUnassignedHAPINodes.Contains(Label))
	{
		HAPI_NodeId GeoNodeId = ExistingUnassignedHAPINodes[Label];
		ExistingUnassignedHAPINodes.Remove(Label);

		FUnrealObjectInputHandle Handle;
		bool bSuccess = FUnrealObjectInputUtils::FindNodeViaManager(Identifier, Handle);
		if(bSuccess)
		{
			RegisteredHandles.Add(Label, Handle);
			RegisteredGeoNodes.Add(Label, GeoNodeId);
			return GeoNodeId;
		}
		else
		{
			// This means we found a node that the reference input system knows nothing about. We need to overwrite it,
			// so delete it.
			FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), GeoNodeId);
		}
	}

	// IF we get her, we'll create a new node.

	bCreated = true;
	HAPI_NodeId GeoNodeId = INDEX_NONE;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(GetConstructionSubnetNodeId(), TEXT("geo"), Label, true, &GeoNodeId), INDEX_NONE);

	RegisteredGeoNodes.Add(Label, GeoNodeId);

	return GeoNodeId;
}

HAPI_NodeId FUnrealMeshExportData::RegisterConstructionNode(
	const FString& Label,
	const HAPI_NodeId NodeId,
	const TSet<FUnrealObjectInputHandle>* ReferencedNodes)
{
	// This function must  be called if GetOrCreateConstructionGeoNode() returned with bCreated == true.
	// For reasons which are not entirely clear to me FUnrealObjectInputUtils::AddNodeOrUpdateNode()
	// requires both an Geo (Object) node and an internal SOP. Since we cannot create the internal SOP
	// until after we create the GEO we must call GetOrCreateConstructionGeoNode() first, then call RegisterConstructionNode()
	// later. 

	// If this call crashes, you didn't call GetOrCreateConstructionGeoNode() :^)

	FUnrealObjectInputIdentifier* FoundId = RegisteredIdentifiers.Find(Label);
	ensure(FoundId);
	HAPI_NodeId GeoNodeId = RegisteredGeoNodes[Label]; 

	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputUtils::AddNodeOrUpdateNode(*FoundId, NodeId, Handle, GeoNodeId, ReferencedNodes, bCanDelete);

	RegisteredHandles.Add(Label, Handle);

	return GeoNodeId;
}

FUnrealObjectInputIdentifier FUnrealMeshExportData::MakeNodeIdentifier(const FString& Label, EUnrealObjectInputNodeType NodeType)
{
	FString FullPath = FString::Printf(TEXT("%s/%s"), *ConstructionSubnetPath, *Label);
	FullPath = FullPath.Replace(TEXT("."), TEXT("/"));

	// Why does the Id need a type? Sure its a property of the Handle or internal node? I am perplexed.
	FUnrealObjectInputIdentifier Id = FUnrealObjectInputIdentifier(FullPath, NodeType);
	RegisteredIdentifiers.Add(Label, Id);
	return Id;
}

FUnrealObjectInputHandle FUnrealMeshExportData::GetNodeHandle(const FString& Label)
{
	FUnrealObjectInputHandle* Found = RegisteredHandles.Find(Label);
	if(Found)
		return *Found;
	else
		return FUnrealObjectInputHandle();
}

TArray<HAPI_NodeId> GetHapiNodeIds(const TArray<FUnrealObjectInputIdentifier>& Identifiers)
{
	TArray<HAPI_NodeId>  Results;
	for (auto Id : Identifiers)
	{
		Results.Add(GetHapiNodeId(Id));
	}
	return Results;
}

HAPI_NodeId GetHapiNodeId(FUnrealObjectInputIdentifier Identifier)
{
	HAPI_NodeId NodeId = INDEX_NONE;
	FUnrealObjectInputUtils::GetHAPINodeId(Identifier, NodeId);
	return NodeId;
}

TArray<HAPI_NodeId> GetHapiNodeIds(const TArray<FUnrealObjectInputHandle>& Handles)
{
	TArray<HAPI_NodeId>  Results;
	for(auto Handle : Handles)
	{
		Results.Add(GetHapiNodeId(Handle));
	}
	return Results;
}

HAPI_NodeId GetHapiNodeId(FUnrealObjectInputHandle Handle)
{
	HAPI_NodeId NodeId = INDEX_NONE;
	FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId);
	return NodeId;
}


bool FUnrealMeshTranslator::CreateInputNodeForStaticMeshComponentNew(
	HAPI_NodeId& InputObjectNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const FUnrealObjectInputHandle& StaticMeshHandle,
	const UStaticMeshComponent* StaticMeshComponent,
	const FString& InputNodeName,
	const FUnrealMeshExportOptions& ExportOptions,
	const bool bInputNodesCanBeDeleted)
{
	FString TopLevelNodePath = *StaticMeshComponent->GetPathName();
	FUnrealObjectInputHandle ParentHandle;

	FUnrealObjectInputIdentifier TopLevelIdentifier = FUnrealObjectInputIdentifier(TopLevelNodePath);
	FUnrealObjectInputUtils::EnsureParentsExist(TopLevelIdentifier, ParentHandle, bInputNodesCanBeDeleted);
	HAPI_NodeId ParentNodeId = FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle);

	FString GeoNodeLabel = TEXT("component");

	HAPI_NodeId GeoNode;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("geo"), GeoNodeLabel, true, &GeoNode), false);

	TSet<FUnrealObjectInputHandle> References;
	References.Add(StaticMeshHandle);

	HAPI_NodeId NodeId;
	bool bSuccess = CreateMergeNode(NodeId, TEXT("static_mesh"), GeoNode, GetHapiNodeIds(References.Array()));

	FString FullPath = FString::Printf(TEXT("%s/%s"), *TopLevelNodePath, *GeoNodeLabel);
	FUnrealObjectInputIdentifier Id = FUnrealObjectInputIdentifier(FullPath, EUnrealObjectInputNodeType::Reference);

	FUnrealObjectInputUtils::AddNodeOrUpdateNode(Id, NodeId, OutHandle, GeoNode, &References, true);

	return true;
}

EHoudiniMeshSource FUnrealMeshTranslator::DetermineMeshSource(const FUnrealMeshExportOptions& ExportOptions, const UStaticMesh* StaticMesh)
{
	bool bAllMeshDescriptionValid = true;
	for (int LODIndex = 0; LODIndex < StaticMesh->GetNumLODs(); LODIndex++)
	{
		if (StaticMesh->GetMeshDescription(LODIndex) == nullptr)
		{
			bAllMeshDescriptionValid = false;
			break;
		}
	}

	// If any LOD is missing a mesh description, use the LOD Resources instead.
	// Missing Mesh Descriptions can happen for automatically generated LODs.
	// But we should make the LODResource and Mesh Description export data the same, then we can mix and match.

	if(!bAllMeshDescriptionValid)
		return EHoudiniMeshSource::LODResource;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	if (StaticMesh->IsNaniteEnabled())
#else
	if (StaticMesh->NaniteSettings.bEnabled)
#endif
	{
		if (ExportOptions.bPreferNaniteFallbackMesh)
		{
			if (StaticMesh->GetRenderData()->LODResources.Num())
			{
				return EHoudiniMeshSource::LODResource;
			}
			else 
			{
				return EHoudiniMeshSource::MeshDescription;
			}
		}
		else
		{
			if (StaticMesh->GetHiResMeshDescription() != nullptr)
			{
				return EHoudiniMeshSource::HiResMeshDescription;
			}
			else
			{
				return EHoudiniMeshSource::MeshDescription;
			}
		}
	}
	else
	{
		return EHoudiniMeshSource::MeshDescription;
	}
}

bool FUnrealMeshTranslator::CreateInputNodeForSplineMeshComponentNew(
	HAPI_NodeId& InputObjectNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const USplineMeshComponent* StaticMeshComponent,
	const FUnrealMeshExportOptions& ExportOptions,
	const bool bInputNodesCanBeDeleted)
{
	// ExportData contains information about the mesh being constructed.
	FUnrealMeshExportData ExportData(StaticMeshComponent, bInputNodesCanBeDeleted);

	FString ComponentLabel;
	bool bSuccess = GetOrConstructSplineMeshComponent(ComponentLabel, ExportData, ExportOptions, StaticMeshComponent);
	if(!bSuccess || !ExportData.Contains(ComponentLabel))
		return false;

	// Fetch the construction results.
	InputObjectNodeId = ExportData.GetHapiNodeId(ComponentLabel);
	OutHandle = ExportData.GetNodeHandle(ComponentLabel);

	return true;
}

bool FUnrealMeshTranslator::GetOrConstructSplineMeshRenderNode(
	FString& RenderMeshLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const USplineMeshComponent* SplineMeshComponent)
{

	// Get or create the geometry node for this set of export options.
	FString GeometryLabel;
	bool bSuccess = GetOrConstructSplineMeshGeometryNode(
		GeometryLabel,
		ExportData,
		ExportOptions,
		SplineMeshComponent);

	if(!bSuccess)
		return false;

	if(ExportOptions.bMaterialParameters)
	{
		UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
		if(!StaticMesh)
			return true;

		// Fetch Materials
		TArray<UMaterialInterface*> MaterialInterfaces = FUnrealMeshTranslator::GetMaterials(StaticMesh);
		TArray<FUnrealMaterialInfo> MaterialInfos;
		GetMaterialInfo(MaterialInterfaces, MaterialInfos);


		// Material Table.
		if(!ExportData.Contains(MaterialTableName))
		{
			GetOrCreateMaterialTableNode(ExportData, MaterialInfos);
		}

		// if we need material parameters create a new node and zip the geometry and materials
		FStringBuilderBase StringBuilder;
		StringBuilder.Append(GeometryLabel);
		StringBuilder.Append(TEXT("_mparams"));

		RenderMeshLabel = StringBuilder.ToString();

		TSet<FUnrealObjectInputHandle> References;
		References.Add(ExportData.GetNodeHandle(GeometryLabel));
		References.Add(ExportData.GetNodeHandle(MaterialTableName));

		// Get or create the geo node. If it already exists, don't recreate it.
		bool bCreated = false;
		HAPI_NodeId GeoNodeId = ExportData.GetOrCreateConstructionGeoNode(bCreated, RenderMeshLabel, EUnrealObjectInputNodeType::Reference);
		if(GeoNodeId == INDEX_NONE)
			return false;

		if(!bCreated)
			return true;

		HAPI_NodeId ZipNodeId;

		bSuccess = GetOrCreateMaterialZipNode(
			ZipNodeId,
			GeoNodeId,
			ExportData.GetHapiNodeId(GeometryLabel),
			ExportData.GetHapiNodeId(MaterialTableName),
			MaterialInfos);

		ExportData.RegisterConstructionNode(RenderMeshLabel, ZipNodeId, &References);

		return bSuccess;
	}
	else
	{
		RenderMeshLabel = GeometryLabel;
	}

	return true;
}

bool FUnrealMeshTranslator::GetOrConstructSplineMeshGeometryNode(
	FString& GeometryLabel,
	FUnrealMeshExportData& ExportData,
	const FUnrealMeshExportOptions& ExportOptions,
	const USplineMeshComponent* MeshComponent)

{
	// Create all low-level geometry nodes required by these export options. For example, lod0, lod1
	bool bSuccess = GetOrCreateSplineMeshLODGeometries(ExportData, MeshComponent, ExportOptions);
	if(!bSuccess)
		return false;

	// Constructs the geometry nodes for the current mesh, if needed. Usuall the main mesh and/or lods and/or
	// hires mesh.

	FString LOD0Name = MakeLODName(0, EHoudiniMeshSource::MeshDescription);

	if(ExportOptions.bMainMesh && !ExportOptions.bLODs)
	{
		GeometryLabel = LOD0Name;

		return true;
	}
	else if(ExportOptions.bLODs)
	{
		// Combine all LODs and return that node.

		GeometryLabel = TEXT("all_lods");
		if(ExportData.Contains(GeometryLabel))
			return true;

		TSet<FUnrealObjectInputHandle> NodeIds;

		// Add either the hires mesh or LOD0

		FString MainMeshName = LOD0Name;

		NodeIds.Add(ExportData.GetNodeHandle(MainMeshName));

		// Add each LOD... ignore LOD0, if its needed it will already have been added.

		for(auto It : ExportData.GetConstructionHandles())
		{
			if(It.Key == LOD0Name)
				continue;

			if(It.Key.StartsWith(LODPrefix))
				NodeIds.Add(It.Value);
		}

		// Create the geo node, but if it already exists just re-use it.

		bool bCreated = false;
		HAPI_NodeId GeoNode = ExportData.GetOrCreateConstructionGeoNode(bCreated, GeometryLabel, EUnrealObjectInputNodeType::Reference);
		if(GeoNode == INDEX_NONE)
			return false;

		if(!bCreated)
			return true;

		HAPI_NodeId NodeId;
		bSuccess = CreateMergeNode(NodeId, GeometryLabel, GeoNode, GetHapiNodeIds(NodeIds.Array()));

		ExportData.RegisterConstructionNode(GeometryLabel, NodeId, &NodeIds);
		return bSuccess;
	}
	return false;
}

bool FUnrealMeshTranslator::GetOrCreateSplineMeshLODGeometries(
	FUnrealMeshExportData& ExportData,
	const USplineMeshComponent* SplineMeshComponent,
	const FUnrealMeshExportOptions& ExportOptions)
{
	UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
	if(!IsValid(StaticMesh))
		return true;

	if(ExportOptions.bMainMesh)
	{
		FString Label = MakeLODName(0, EHoudiniMeshSource::MeshDescription);

		if(!ExportData.Contains(Label))
		{
			GetOrCreateExportSplineMeshLOD(ExportData, 0, SplineMeshComponent);
		}
	}

	if(ExportOptions.bLODs)
	{
		int NumLODs = StaticMesh->GetNumLODs();

		for(int LODIndex = 0; LODIndex < NumLODs; LODIndex++)
		{
			FString NodeLabel = MakeLODName(LODIndex, EHoudiniMeshSource::MeshDescription);
			if(!ExportData.Contains(NodeLabel))
			{
				GetOrCreateExportSplineMeshLOD(ExportData, LODIndex, SplineMeshComponent);
			}
		}
	}

	return true;
}

bool FUnrealMeshTranslator::GetOrCreateExportSplineMeshLOD(
	FUnrealMeshExportData& ExportData,
	const int LODIndex,
	const USplineMeshComponent* SplineMeshComponent)

{
	FString LODName = MakeLODName(LODIndex, EHoudiniMeshSource::MeshDescription);

	bool bCreated = false;
	HAPI_NodeId GeoNodeId = ExportData.GetOrCreateConstructionGeoNode(bCreated, LODName, EUnrealObjectInputNodeType::Leaf);
	if(GeoNodeId == INDEX_NONE)
		return false;

	// If the geo node already existed, don't recreate it.
	if(!bCreated)
		return true;

	HAPI_NodeId NodeId = INDEX_NONE;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), GeoNodeId, "null", H_TCHAR_TO_UTF8(*LODName), true, &NodeId), false);

	ExportData.RegisterConstructionNode(LODName, NodeId);

	FMeshDescription MeshDesc;
	static constexpr bool bPropagateVertexColours = false;
	static constexpr bool bApplyComponentTransform = false;
	FHoudiniMeshUtils::RetrieveMesh(MeshDesc, SplineMeshComponent, LODIndex, bPropagateVertexColours, bApplyComponentTransform);

	bool bSuccess = FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
			NodeId,
			MeshDesc,
			LODIndex,
			true,
			false,
			SplineMeshComponent->GetStaticMesh(),
			nullptr);

	return bSuccess;
}
