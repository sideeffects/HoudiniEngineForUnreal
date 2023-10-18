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

#include "UnrealSkeletalMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealMeshTranslator.h"

#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Rendering/SkeletalMeshModel.h"


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    #include "Engine/SkinnedAssetCommon.h"
#endif


FTransform
GetCompSpaceTransformForBone(const FReferenceSkeleton& InSkel, const int32& InBoneIdx)
{
    FTransform resultBoneTransform = InSkel.GetRefBonePose()[InBoneIdx];

    auto refBoneInfo = InSkel.GetRefBoneInfo();

    int32 Bone = InBoneIdx;
    while (Bone)
    {
        resultBoneTransform *= InSkel.GetRefBonePose()[refBoneInfo[Bone].ParentIndex];
        Bone = refBoneInfo[Bone].ParentIndex;
    }

    return resultBoneTransform;
}

void
GetComponentSpaceTransforms(TArray<FTransform>& OutResult, const FReferenceSkeleton& InRefSkeleton)
{
    const int32 PoseNum = InRefSkeleton.GetRefBonePose().Num();
    OutResult.SetNum(PoseNum);

    for (int32 i = 0; i < PoseNum; i++)
    {
        OutResult[i] = GetCompSpaceTransformForBone(InRefSkeleton, i);

    }
}


bool
FUnrealSkeletalMeshTranslator::HapiCreateInputNodeForSkeletalMesh(
	USkeletalMesh* SkeletalMesh,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	USkeletalMeshComponent* SkeletalMeshComponent /*=nullptr*/,
	const bool& ExportAllLODs /*=false*/,
	const bool& ExportSockets /*=false*/,
	const bool& ExportColliders /*=false*/,
	const bool& ExportMainMesh /* = true */,
	const bool& bInputNodesCanBeDeleted /*=true*/,
	const bool& bExportMaterialParameters /*= false*/)
{
	// If we don't have a skeletal mesh there's nothing to do.
	if (!IsValid(SkeletalMesh))
		return false;

	// Input node name, defaults to InputNodeName, but can be changed by the new input system
	FString FinalInputNodeName = InputNodeName;

	// Find the node in new input system
	// Identifier will be the identifier for the entry created in this call of the function.
	// We may call this function recursively to create the main mesh, LODs, sockets and colliders, each getting its own identifier.
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;
	const bool bUseRefCountedInputSystem = FUnrealObjectInputRuntimeUtils::IsRefCountedInputSystemEnabled();
	if (bUseRefCountedInputSystem)
	{
		// Check if we already have an input node for this asset
		static constexpr bool bForceCreateInputRefNode = false;
		bool bSingleLeafNodeOnly = false;
		FUnrealObjectInputIdentifier IdentReferenceNode;
		TArray<FUnrealObjectInputIdentifier> IdentPerOption;
		static constexpr bool bMainMeshIsNaniteFallbackMesh = false;
		if (!FUnrealObjectInputUtils::BuildMeshInputObjectIdentifiers(
			SkeletalMesh,
			ExportMainMesh,
			ExportAllLODs,
			ExportSockets,
			ExportColliders,
			bMainMeshIsNaniteFallbackMesh,
			bForceCreateInputRefNode,
			bSingleLeafNodeOnly,
			IdentReferenceNode,
			IdentPerOption))
		{
			return false;
		}

		if (bSingleLeafNodeOnly)
		{
			// We'll create the skeletal mesh input node entirely is this function call
			check(!IdentPerOption.IsEmpty());
			Identifier = IdentPerOption[0];
		}
		else
		{
			// Look for the reference node that references the per-option (LODs, colliders) nodes
			Identifier = IdentReferenceNode;
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
				if (FUnrealObjectInputUtils::FindNodeViaManager(OptionIdentifier, OptionHandle) || !FUnrealObjectInputUtils::AreHAPINodesValid(OptionHandle))
				{
					FUnrealObjectInputUtils::GetHAPINodeId(OptionHandle, NewNodeId);
				}

				// Recursive call
				if (!FUnrealSkeletalMeshTranslator::HapiCreateInputNodeForSkeletalMesh(
					SkeletalMesh,
					NewNodeId,
					NodeLabel,
					OptionHandle,
					SkeletalMeshComponent,
					Options.bExportLODs,
					Options.bExportSockets,
					Options.bExportColliders,
					!Options.bExportLODs && !Options.bExportSockets && !Options.bExportColliders,
					bInputNodesCanBeDeleted,
					bExportMaterialParameters))
				{
					return false;
				}

				PerOptionNodeHandles.Add(OptionHandle);
			}

			// Create or update the HAPI node for the reference node if it does not exist
			FUnrealObjectInputHandle RefNodeHandle;
			if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(IdentReferenceNode, PerOptionNodeHandles, RefNodeHandle, true, bInputNodesCanBeDeleted))
				return false;

			OutHandle = RefNodeHandle;
			FUnrealObjectInputUtils::GetHAPINodeId(IdentReferenceNode, InputNodeId);
			return true;
		}
	}

	// Node ID for the newly created node
	HAPI_NodeId NewNodeId = -1;

	// Export sockets if there are some
	bool DoExportSockets = ExportSockets && (SkeletalMesh->NumSockets() > 0);

	// Export LODs if there are some	
	bool DoExportLODs = ExportAllLODs && (SkeletalMesh->GetLODNum() > 1);

	/*
	// Const cast due to deprecation warning to access the body setup
	const USkeletalMesh* SK_const = SkeletalMesh;
	const UBodySetup* BodySetup = SK_const->GetBodySetup();
	bool DoExportColliders = ExportColliders && BodySetup != nullptr;
	if (DoExportColliders)
	{
		if (BodySetup->AggGeom.GetElementCount() <= 0)
		{
			DoExportColliders = false;
		}
	}*/

	// Export colliders if there are some
	// For Skeletal mesh, we need to look at all the SKBodySetups
	bool DoExportColliders = false;
	TArray<TObjectPtr<USkeletalBodySetup>> BodySetups;
	if (ExportColliders && SkeletalMesh->GetPhysicsAsset())
	{
		BodySetups = SkeletalMesh->GetPhysicsAsset()->SkeletalBodySetups;
		for (auto& CurBS : BodySetups)
		{
			if (CurBS->AggGeom.GetElementCount() <= 0)
				continue;

			// We found at least one collider, we'll need to export them!
			DoExportColliders = true;
			break;
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
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				-1, TEXT("SOP/merge"), FinalInputNodeName, true, &NewNodeId), false);
		}
		else
		{
			// When creating a node inside a parent node (in other words, ParentNodeId is not -1), then we cannot
			// specify the node type category prefix on the node name. We have to create the geo Object and merge
			// SOPs separately.
			HAPI_NodeId ObjectNodeId = -1;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				ParentNodeId, TEXT("geo"), FinalInputNodeName, true, &ObjectNodeId), false);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				ObjectNodeId, TEXT("merge"), FinalInputNodeName, true, &NewNodeId), false);
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

	HAPI_NodeId PreviousInputNodeId = InputNodeId;

	// Update our input NodeId
	InputNodeId = NewNodeId;
	// Get our parent OBJ NodeID
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);

	// We have now created a valid new input node, delete the previous one
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

	// Next Index used to connect nodes to the merge
	int32 NextMergeIndex = 0;

	// Determine which LODs to export based on the ExportLODs/ExportMainMesh, high res mesh availability and whether
	// the new input system is being used.
	const int32 NumLODs = SkeletalMesh->GetLODNum();
	int32 FirstLODIndex = 0;
	int32 LastLODIndex = -1;

	if (DoExportLODs)
	{
		LastLODIndex = NumLODs - 1;

		// When using the new input system
		// Don't export LOD0 with the LODs  since we have a separate "main mesh" input
		FirstLODIndex = bUseRefCountedInputSystem ? 1 : 0;
	}
	else if (ExportMainMesh)
	{
		// Just export the main mesh (LOD0)
		LastLODIndex = 0;
		FirstLODIndex = 0;
	}
	else
	{
		// Dont export any LOD
		LastLODIndex = -1;
		FirstLODIndex = 0;
	}

	if (LastLODIndex >= 0)
	{
		for (int32 LODIndex = FirstLODIndex; LODIndex <= LastLODIndex; LODIndex++)
		{
			// If we're using a merge node, we need to create a new input null
			HAPI_NodeId CurrentLODNodeId = -1;
			if (UseMergeNode)
			{
				// Create a new input node for the current LOD
				const char* LODName = "";
				{
					FString LOD = TEXT("lod") + FString::FromInt(LODIndex);
					LODName = TCHAR_TO_UTF8(*LOD);
				}

				// Create the node in this input object's OBJ node
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
					InputObjectNodeId, TEXT("null"), LODName, false, &CurrentLODNodeId), false);
			}
			else
			{
				// No merge node, just use the input node we created before
				CurrentLODNodeId = NewNodeId;
			}

			// Set the skeletal mesh data for this lod on the input node
			if (!FUnrealSkeletalMeshTranslator::SetSkeletalMeshDataOnNode(
				SkeletalMesh, SkeletalMeshComponent, CurrentLODNodeId, LODIndex, DoExportLODs, bExportMaterialParameters))
			{
				HOUDINI_LOG_ERROR(TEXT("Failed to set the skeletal mesh data on the input node for %s LOD %d."), *InputNodeName, LODIndex);
				continue;
			}

			if (UseMergeNode)
			{
				// Connect the LOD node to the merge node.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					NewNodeId, NextMergeIndex, CurrentLODNodeId, 0), false);
			}

			NextMergeIndex++;
		}
	}

	// TODO: duplicate with Static Mesh
	// Refactor this and the static mesh to a exportcolliders function taking a body setup?
	if (DoExportColliders && BodySetups.Num() > 0)
	{
		FReferenceSkeleton RefSK = SkeletalMesh->GetRefSkeleton();
		TArray<FTransform> AllBonePos = RefSK.GetRawRefBonePose();

		for (auto& BodySetup : BodySetups)
		{
			FKAggregateGeom SimpleColliders = BodySetup->AggGeom;

			// If there are no simple colliders to create then skip this bodysetup
			if (SimpleColliders.BoxElems.Num() + SimpleColliders.SphereElems.Num() + SimpleColliders.SphylElems.Num()
				+ SimpleColliders.ConvexElems.Num() <= 0)
				continue;

			HAPI_NodeId CollisionMergeNodeId = -1;
			int32 NextCollisionMergeIndex = 0;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				InputObjectNodeId, TEXT("merge"), TEXT("simple_colliders_merge") + FString::FromInt(NextMergeIndex), false, &CollisionMergeNodeId), false);

			// Calculate the colliders transform
			// They are stored relative to a bone, so we first need to get the corresponding bone's transform
			// by going up the chains of bones until we reach the root bone
			int32 BoneIndex = RefSK.FindBoneIndex(BodySetup->BoneName);
			FTransform BoneTransform = AllBonePos.IsValidIndex(BoneIndex) ? AllBonePos[BoneIndex] : FTransform::Identity;
			do
			{
				int32 ParentIndex = RefSK.GetParentIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					BoneTransform = BoneTransform * AllBonePos[ParentIndex];
				}

				BoneIndex = ParentIndex;
			} while (RefSK.IsValidIndex(BoneIndex));


			// Export BOX colliders
			for (auto& CurBox : SimpleColliders.BoxElems)
			{
				FTransform BoxTransform = CurBox.GetTransform() * BoneTransform;

				FVector BoxCenter = BoxTransform.GetLocation();//CurBox.Center;
				FVector BoxExtent = FVector(CurBox.X, CurBox.Y, CurBox.Z);
				FRotator BoxRotation = BoxTransform.GetRotation().Rotator();//CurBox.Rotation;

				HAPI_NodeId BoxNodeId = -1;
				if (!FUnrealMeshTranslator::CreateInputNodeForBox(
					BoxNodeId, InputObjectNodeId, NextCollisionMergeIndex,
					BoxCenter, BoxExtent, BoxRotation))
					continue;

				if (BoxNodeId < 0)
					continue;

				// Connect the Box node to the merge node.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					CollisionMergeNodeId, NextCollisionMergeIndex, BoxNodeId, 0), false);

				NextCollisionMergeIndex++;
			}

			// Export SPHERE colliders
			for (auto& CurSphere : SimpleColliders.SphereElems)
			{
				FTransform SphereTransform = CurSphere.GetTransform() * BoneTransform;
				FVector SphereCenter = SphereTransform.GetLocation();// CurSphere.Center;				

				HAPI_NodeId SphereNodeId = -1;
				if (!FUnrealMeshTranslator::CreateInputNodeForSphere(
					SphereNodeId, InputObjectNodeId, NextCollisionMergeIndex,
					SphereCenter, CurSphere.Radius))
					continue;

				if (SphereNodeId < 0)
					continue;

				// Connect the Sphere node to the merge node.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					CollisionMergeNodeId, NextCollisionMergeIndex, SphereNodeId, 0), false);

				NextCollisionMergeIndex++;
			}

			// Export CAPSULE colliders
			for (auto& CurSphyl : SimpleColliders.SphylElems)
			{
				FTransform SphylTransform = CurSphyl.GetTransform() * BoneTransform;

				FVector SphylCenter = SphylTransform.GetLocation(); // CurSphyl.Center
				FRotator SphylRotation = SphylTransform.GetRotation().Rotator();//CurSphyl.Rotation;

				HAPI_NodeId SphylNodeId = -1;
				if (!FUnrealMeshTranslator::CreateInputNodeForSphyl(
					SphylNodeId, InputObjectNodeId, NextCollisionMergeIndex,
					SphylCenter, SphylRotation, CurSphyl.Radius, CurSphyl.Length))
					continue;

				if (SphylNodeId < 0)
					continue;

				// Connect the capsule node to the merge node.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					CollisionMergeNodeId, NextCollisionMergeIndex, SphylNodeId, 0), false);

				NextCollisionMergeIndex++;
			}

			// TODO!! Insert bone transform here!!
			// Export CONVEX colliders
			for (auto& CurConvex : SimpleColliders.ConvexElems)
			{
				HAPI_NodeId ConvexNodeId = -1;
				if (!FUnrealMeshTranslator::CreateInputNodeForConvex(
					ConvexNodeId, InputObjectNodeId, NextCollisionMergeIndex, CurConvex))
					continue;

				if (ConvexNodeId < 0)
					continue;

				// Connect the capsule node to the merge node.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					CollisionMergeNodeId, NextCollisionMergeIndex, ConvexNodeId, 0), false);

				NextCollisionMergeIndex++;
			}

			// Create a new primitive attribute where each value contains the Physical Material name in Unreal.
			UPhysicalMaterial* PhysicalMaterial = BodySetup->PhysMaterial;
			if (PhysicalMaterial)
			{
				// Create a new Attribute Wrangler node which will be used to create the new attributes.
				HAPI_NodeId AttribWrangleNodeId;
				if (FHoudiniEngineUtils::CreateNode(
					InputObjectNodeId, TEXT("attribwrangle"),
					TEXT("physical_material"),
					true, &AttribWrangleNodeId) != HAPI_RESULT_SUCCESS)
				{
					// Failed to create the node.
					HOUDINI_LOG_WARNING(
						TEXT("Failed to create Physical Material attribute for mesh: %s"),
						*FHoudiniEngineUtils::GetErrorDescription());
					return false;
				}

				// Connect the new node to the previous node. Set NewNodeId to the attrib node
				// as is this the final output of the chain.
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(),
					AttribWrangleNodeId, 0, CollisionMergeNodeId, 0), false);
				CollisionMergeNodeId = AttribWrangleNodeId;

				// Set the wrangle's class to primitives
				HOUDINI_CHECK_ERROR_RETURN(
					FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), AttribWrangleNodeId, "class", 0, 1), false);

				// Construct a VEXpression to set create and set a Physical Material Attribute.
				// eg. s@unreal_physical_material = 'MyPath/PhysicalMaterial';
				const FString FormatString = TEXT("s@{0} = '{1}';");
				FString PathName = PhysicalMaterial->GetPathName();
				FString AttrName = TEXT(HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL);
				std::string VEXpression = TCHAR_TO_UTF8(*FString::Format(*FormatString,
					{ AttrName, PathName }));

				// Set the snippet parameter to the VEXpression.
				HAPI_ParmInfo ParmInfo;
				HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(AttribWrangleNodeId, "snippet", ParmInfo);
				if (ParmId != -1)
				{
					FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), AttribWrangleNodeId,
						VEXpression.c_str(), ParmId, 0);
				}
				else
				{
					HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"),
						*FHoudiniEngineUtils::GetErrorDescription());
				}
			}

			// Connect our collision merge node (or the phys mat attrib wrangle) to the main merge node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, CollisionMergeNodeId, 0), false);
			NextMergeIndex++;
		}
	}

	if (DoExportSockets && SkeletalMesh->NumSockets() > 0)
	{
		// Create an input node for the skeletal mesh sockets
		HAPI_NodeId SocketsNodeId = -1;

		if (CreateInputNodeForSkeletalMeshSockets(SkeletalMesh, InputObjectNodeId, SocketsNodeId))
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

	if (bUseRefCountedInputSystem)
	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	//
	return true;
}

bool
FUnrealSkeletalMeshTranslator::SetSkeletalMeshDataOnNode(
	USkeletalMesh* SkeletalMesh,
	USkeletalMeshComponent* SkeletalMeshComponent,
	HAPI_NodeId& NewNodeId,
	int32 LODIndex,
	const bool& bAddLODGroups,
	const bool bInExportMaterialParametersAsAttributes)
{
	if (!IsValid(SkeletalMesh))
		return false;

	const FSkeletalMeshModel* SkelMeshResource = SkeletalMesh->GetImportedModel();
	if (!SkelMeshResource)
		return false;

	if (!SkelMeshResource->LODModels.IsValidIndex(LODIndex))
		return false;

	const FSkeletalMeshLODModel& SourceModel = SkelMeshResource->LODModels[LODIndex];

	// Copy all the vertex data from the various chunks to a single buffer.
	// Makes the rest of the code in this function cleaner and easier to maintain.  
	TArray<FSoftSkinVertex> Vertices;
	SourceModel.GetVertices(Vertices);

	// Verify the integrity of the mesh.
	const int32 VertexCount = Vertices.Num();
	if (VertexCount == 0)
		return false;

	if (Vertices.Num() != VertexCount)
		return false;

	TArray<FVector3f> Points;
	Points.SetNum(VertexCount);

	TArray<FVector3f> Normals;
	Normals.SetNum(VertexCount);

	TArray<FVector3f> UV0;
	UV0.SetNum(VertexCount);

	for (int32 VertIndex = 0; VertIndex < VertexCount; VertIndex++)
	{
		Points[VertIndex] = Vertices[VertIndex].Position;
		Normals[VertIndex] = FVector3f(Vertices[VertIndex].TangentZ.X, Vertices[VertIndex].TangentZ.Y, Vertices[VertIndex].TangentZ.Z);
		Swap(Normals[VertIndex].Y, Normals[VertIndex].Z);
		UV0[VertIndex] = FVector3f(Vertices[VertIndex].UVs[0].X, 1.0f - Vertices[VertIndex].UVs[0].Y, 0.0f);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITION (P)
	//--------------------------------------------------------------------------------------------------------------------- 
	// In FStaticMeshLODResources each vertex instances stores its position, even if the positions are not unique (in other
	// words, in Houdini terminology, the number of points and vertices are the same. We'll do the same thing that Epic
	// does in FBX export: we'll run through all vertex instances and use a hash to determine which instances share a 
	// position, so that we can a smaller number of points than vertices, and vertices share point positions
	TArray<int32> UEVertexInstanceIdxToPointIdx;
	UEVertexInstanceIdxToPointIdx.Reserve(VertexCount);

	TMap<FVector3f, int32> PositionToPointIndexMap;
	PositionToPointIndexMap.Reserve(VertexCount);
	FVector3f BuildScaleVector = FVector3f::OneVector;

	TArray<float> SkeletalMeshPoints;
	SkeletalMeshPoints.Reserve(VertexCount * 3);

	TArray<float> SkeletalMeshNormals;
	SkeletalMeshNormals.Reserve(VertexCount * 3);

	TArray<float> PointUVs;
	PointUVs.Reserve(VertexCount * 3);

	TArray<float> BoneCaptureData;

	int32 InfluenceCount = 4;
	TArray<int32> BoneCaptureIndexArray;
	BoneCaptureIndexArray.Reserve(InfluenceCount * VertexCount);

	TArray<float> BoneCaptureDataArray;
	BoneCaptureDataArray.Reserve(InfluenceCount * VertexCount);

	TArray<int32> SizesBoneCaptureIndexArray;
	SizesBoneCaptureIndexArray.Reserve(InfluenceCount * VertexCount);

	// - Switching to iterate over sections
	for (FSkelMeshSection section : SourceModel.Sections)
	{
		for (int32 VertexInstanceIndex = 0; VertexInstanceIndex < section.SoftVertices.Num(); ++VertexInstanceIndex)
		{
			// Convert Unreal to Houdini
			const FVector3f& PositionVector = section.SoftVertices[VertexInstanceIndex].Position;
			const FVector3f& NormalVector = FVector3f(section.SoftVertices[VertexInstanceIndex].TangentZ.X, section.SoftVertices[VertexInstanceIndex].TangentZ.Y, section.SoftVertices[VertexInstanceIndex].TangentZ.Z);
			const FVector2D& UV0Vector2d = FVector2D(section.SoftVertices[VertexInstanceIndex].UVs[0].X, section.SoftVertices[VertexInstanceIndex].UVs[0].Y);
			const int32* FoundPointIndexPtr = PositionToPointIndexMap.Find(PositionVector);

			if (!FoundPointIndexPtr)
			{
				const int32 NewPointIndex = SkeletalMeshPoints.Add(PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.X) / 3;
				SkeletalMeshPoints.Add(PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Z);
				SkeletalMeshPoints.Add(PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Y);
				SkeletalMeshNormals.Add(NormalVector.X);
				//SkeletalMeshNormals.Add(-NormalVector.Z);
				SkeletalMeshNormals.Add(NormalVector.Z);
				SkeletalMeshNormals.Add(NormalVector.Y);
				PointUVs.Add((float)UV0Vector2d.X);
				PointUVs.Add(1.0f - (float)UV0Vector2d.Y);
				PointUVs.Add(0);

				PositionToPointIndexMap.Add(PositionVector, NewPointIndex);
				UEVertexInstanceIdxToPointIdx.Add(NewPointIndex);

				int weightcounts = 0;
				for (int idx = 0; idx < InfluenceCount; idx++)
				{
					float weight = (float)section.SoftVertices[VertexInstanceIndex].InfluenceWeights[idx] / 255.0f;
					if (weight > 0.0f)
					{
						BoneCaptureData.Add(weight);
						BoneCaptureDataArray.Add(weight);
						int BoneIndex = section.SoftVertices[VertexInstanceIndex].InfluenceBones[idx];
						int AltIndex = section.BoneMap[BoneIndex];
						BoneCaptureData.Add(AltIndex);
						BoneCaptureIndexArray.Add(AltIndex);
						weightcounts++;
					}
				}
				SizesBoneCaptureIndexArray.Add(weightcounts);
			}
			else
			{
				UEVertexInstanceIdxToPointIdx.Add(*FoundPointIndexPtr);
			}
		}
	}

	SkeletalMeshPoints.Shrink();
	SkeletalMeshNormals.Shrink();
	SizesBoneCaptureIndexArray.Shrink();
	BoneCaptureIndexArray.Shrink();
	BoneCaptureDataArray.Shrink();


	//--------------------------------------------------------------------------------------------------------------------- 
	// VERTICES (Vertex Indices)
	//---------------------------------------------------------------------------------------------------------------------
	TArray<int32> StaticMeshIndices;

	// Create the per-material polygons sets.
	int32 SectionCount = SourceModel.Sections.Num();
	TArray<TPair<uint32, uint32>> VertexIndexOffsetPairArray{ TPair<uint32, uint32>(0,0) };

	int TotalTriangleCount = 0;
	TArray<FVector3f> FaceNormals;
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FSkelMeshSection& Section = SourceModel.Sections[SectionIndex];

		int32 MatIndex = Section.MaterialIndex;

		// Static meshes contain one triangle list per element.
		int32 TriangleCount = Section.NumTriangles;
		TotalTriangleCount += TriangleCount;

		// Copy over the index buffer into the FBX polygons set.
		for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			for (int32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				int32 VertexPositionIndex = SourceModel.IndexBuffer[Section.BaseIndex + ((TriangleIndex * 3) + PointIndex)];
				StaticMeshIndices.Add(UEVertexInstanceIdxToPointIdx[VertexPositionIndex]);

				FVector3f fixed = Normals[VertexPositionIndex];
				FaceNormals.Add(fixed);
			}

			//fix winding
			int32 last = StaticMeshIndices.Num();
			int32 temp = StaticMeshIndices[last - 1];

			StaticMeshIndices[last - 1] = StaticMeshIndices[last - 2];
			StaticMeshIndices[last - 2] = temp;
		}
	}

	// Create part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = StaticMeshIndices.Num();
	Part.faceCount = TotalTriangleCount;
	Part.pointCount = SkeletalMeshPoints.Num() / 3;
	Part.type = HAPI_PARTTYPE_MESH;

	HAPI_Result ResultPartInfo = FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0, &Part);

	//--------------------------------------------------------------------------------------------------------------------- 
	// POINTS (P)
	//---------------------------------------------------------------------------------------------------------------------
	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
	AttributeInfoPoint.count = Part.pointCount;
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	// Now that we have raw positions, we can upload them for our attribute.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint,
		(float*)SkeletalMeshPoints.GetData(), 0, AttributeInfoPoint.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// INDICES (VertexList)
	//---------------------------------------------------------------------------------------------------------------------

	// We can now set vertex list.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, StaticMeshIndices.GetData(), 0, StaticMeshIndices.Num()), false);

	//   // We need to generate array of face counts.
	//   TArray< int32 > StaticMeshFaceCounts;
	   //BoneCaptureIndexArray.Reserve(Part.faceCount);
	//   StaticMeshFaceCounts.Init(3, Part.faceCount);
	   //HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(
	   //	FHoudiniEngine::Get().GetSession(),
	   //	NewNodeId, 0, StaticMeshFaceCounts.GetData(), 0, StaticMeshFaceCounts.Num()), false);


	   // We need to generate array of face counts.
	TArray<int32> StaticMeshFaceCounts;
	StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);
	for (int32 n = 0; n < Part.faceCount; n++)
		StaticMeshFaceCounts[n] = 3;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
		StaticMeshFaceCounts, NewNodeId, 0), false);



	//--------------------------------------------------------------------------------------------------------------------- 
	// NORMALS (N)
	//---------------------------------------------------------------------------------------------------------------------
	// Create attribute for normals.
	HAPI_AttributeInfo AttributeInfoNormal;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoNormal);
	AttributeInfoNormal.tupleSize = 3;
	AttributeInfoNormal.count = Part.pointCount;  //Normals is array of FVector3f
	AttributeInfoNormal.exists = true;
	AttributeInfoNormal.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoNormal.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoNormal.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoNormal), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL,
		&AttributeInfoNormal, (float*)SkeletalMeshNormals.GetData(),
		0, AttributeInfoNormal.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// POINT UVS (UV)
	//---------------------------------------------------------------------------------------------------------------------

	HAPI_AttributeInfo AttributeInfoUV;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoUV);
	AttributeInfoUV.tupleSize = 3;
	AttributeInfoUV.count = PointUVs.Num() / AttributeInfoUV.tupleSize;
	AttributeInfoUV.exists = true;
	AttributeInfoUV.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoUV.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoUV.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoUV), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_UV,
		&AttributeInfoUV, (float*)PointUVs.GetData(),
		0, AttributeInfoUV.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// Materials
	//---------------------------------------------------------------------------------------------------------------------

	// Create attribute for materials.
	TArray<UMaterialInterface*> MaterialInterfaces;
	for (FSkeletalMaterial SkeletalMaterial : SkeletalMesh->GetMaterials())
	{
		MaterialInterfaces.Add(SkeletalMaterial.MaterialInterface);
	}

	// 
	// Build a triangle material indices array: material index per triangle
	//
	TArray<int32> TriangleMaterialIndices;
	TriangleMaterialIndices.Reserve(TotalTriangleCount);
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const int32 MaterialIndex = SourceModel.Sections[SectionIndex].MaterialIndex;
		const int32 NumSectionTriangles = SourceModel.Sections[SectionIndex].NumTriangles;
		for (int32 SectionTriangleIndex = 0; SectionTriangleIndex < NumSectionTriangles; ++SectionTriangleIndex)
		{
			TriangleMaterialIndices.Add(MaterialIndex);
		}
	}

	// List of materials, one for each face.
	FHoudiniEngineIndexedStringMap StaticMeshFaceMaterials;

	//Lists of material parameters
	TMap<FString, TArray<float>> ScalarMaterialParameters;
	TMap<FString, TArray<float>> VectorMaterialParameters;
	TMap<FString, FHoudiniEngineIndexedStringMap> TextureMaterialParameters;

	bool bAttributeSuccess = false;
	FString PhysicalMaterialPath = FUnrealMeshTranslator::GetSimplePhysicalMaterialPath(
		SkeletalMeshComponent, const_cast<USkeletalMesh const*>(SkeletalMesh)->GetBodySetup());
	if (bInExportMaterialParametersAsAttributes)
	{
		// Create attributes for the material and all its parameters
		// Get material attribute data, and all material parameters data
		FUnrealMeshTranslator::CreateFaceMaterialArray(
			MaterialInterfaces, TriangleMaterialIndices, StaticMeshFaceMaterials,
			ScalarMaterialParameters, VectorMaterialParameters, TextureMaterialParameters);
	}
	else
	{
		// Create attributes only for the materials
		// Only get the material attribute data
		FUnrealMeshTranslator::CreateFaceMaterialArray(
			MaterialInterfaces, TriangleMaterialIndices, StaticMeshFaceMaterials);
	}

	// Create all the needed attributes for materials
	bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
		NewNodeId,
		0,
		TriangleMaterialIndices.Num(),
		StaticMeshFaceMaterials,
		ScalarMaterialParameters,
		VectorMaterialParameters,
		TextureMaterialParameters,
		PhysicalMaterialPath);

	if (!bAttributeSuccess)
		return false;

	//--------------------------------------------------------------------------------------------------------------------- 
	// Capt_Names
	// Bone Names
	//---------------------------------------------------------------------------------------------------------------------
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();

	TArray<FTransform> ComponentSpaceTransforms;
	GetComponentSpaceTransforms(ComponentSpaceTransforms, RefSkeleton);

	TArray<FString> CaptNamesData;
	TArray<int32> CaptParentsData;
	TArray<float> XFormsData;

	int32 TotalBones = RefSkeleton.GetRawBoneNum();
	TArray<float> CaptData;  //for pCaptData property
	CaptData.SetNumZeroed(TotalBones * 20);
	//CaptData.Init(0.0f, TotalBones * 20);

	XFormsData.AddZeroed(16 * RefSkeleton.GetRawBoneNum());
	CaptParentsData.AddUninitialized(RefSkeleton.GetRawBoneNum());
	CaptNamesData.SetNum(RefSkeleton.GetRawBoneNum());

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex)
	{
		const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[BoneIndex];
		const FTransform& LocalBoneTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
		FTransform& BoneTransform = ComponentSpaceTransforms[BoneIndex];

		FTransform ScaleConversion = FTransform(FRotator(0.0, 0.0, 00), FVector(0.0, 0.0, 0.0), FVector(1, 1, -1));
		FTransform FirstRotationConversion = FTransform(FRotator(0.0, 0, -90.0), FVector(0.0, 0.0, 0.0), FVector(1, 1, 1));
		FTransform BoneTransformConverted = BoneTransform * ScaleConversion * FirstRotationConversion;

		FTransform FinalTransform;
		FinalTransform.SetTranslation(0.01f * BoneTransformConverted.GetTranslation());

		FRotator StockRot = BoneTransformConverted.GetRotation().Rotator();
		StockRot.Roll += 180;
		FinalTransform.SetRotation(StockRot.Quaternion());

		FMatrix M44 = FinalTransform.ToMatrixWithScale();
		FMatrix M44Inverse = M44.Inverse();  //see pCaptData property

		int32 row = 0;
		int32 col = 0;
		for (int32 i = 0; i < 16; i++)
		{
			XFormsData[16 * BoneIndex + i] = M44.M[row][col];
			CaptData[20 * BoneIndex + i] = M44Inverse.M[row][col];
			col++;
			if (col > 3)
			{
				row++;
				col = 0;
			}
		}
		CaptData[20 * BoneIndex + 16] = 1.0f;//Top height
		CaptData[20 * BoneIndex + 17] = 1.0f;//Bottom Height
		CaptData[20 * BoneIndex + 18] = 1.0f;//Ratio of (top x radius of tube)/(bottom x radius of tube) adjusted for orientation
		CaptData[20 * BoneIndex + 19] = 1.0f;//Ratio of (top z radius of tube)/(bottom z radius of tube) adjusted for orientation


		CaptNamesData[BoneIndex] = CurrentBone.ExportName;
		CaptParentsData[BoneIndex] = CurrentBone.ParentIndex;
	}

	HAPI_AttributeInfo CaptNamesInfo;
	FHoudiniApi::AttributeInfo_Init(&CaptNamesInfo);
	CaptNamesInfo.count = 1;
	CaptNamesInfo.tupleSize = 1;
	CaptNamesInfo.exists = true;
	CaptNamesInfo.owner = HAPI_ATTROWNER_DETAIL;
	CaptNamesInfo.storage = HAPI_STORAGETYPE_STRING_ARRAY;
	CaptNamesInfo.originalOwner = HAPI_ATTROWNER_DETAIL;
	CaptNamesInfo.totalArrayElements = CaptNamesData.Num();
	CaptNamesInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"capt_names", &CaptNamesInfo), false);

	TArray<int32> SizesFixedArray;
	SizesFixedArray.Add(CaptNamesData.Num());
	FHoudiniEngineUtils::HapiSetAttributeStringArrayData(CaptNamesData, NewNodeId, 0, "capt_names", CaptNamesInfo, SizesFixedArray);

	//boneCapture_pCaptPath-------------------------------------------------------------------
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"boneCapture_pCaptPath", &CaptNamesInfo), false);

	FHoudiniEngineUtils::HapiSetAttributeStringArrayData(CaptNamesData, NewNodeId, 0, "boneCapture_pCaptPath", CaptNamesInfo, SizesFixedArray);

	//--------------------------------------------------------------------------------------------------------------------- 
	// Capt_Parents
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo CaptParentsInfo;
	FHoudiniApi::AttributeInfo_Init(&CaptParentsInfo);
	CaptParentsInfo.count = 1;
	CaptParentsInfo.tupleSize = 1;
	CaptParentsInfo.exists = true;
	CaptParentsInfo.owner = HAPI_ATTROWNER_DETAIL;
	CaptParentsInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
	CaptParentsInfo.originalOwner = HAPI_ATTROWNER_DETAIL;
	CaptParentsInfo.totalArrayElements = CaptParentsData.Num();
	CaptParentsInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"capt_parents", &CaptParentsInfo), false);

	TArray<int32> SizesParentsArray;
	SizesParentsArray.Add(CaptParentsData.Num());
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "capt_parents", &CaptParentsInfo, CaptParentsData.GetData(),
		CaptParentsData.Num(), SizesParentsArray.GetData(), 0, SizesParentsArray.Num()), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// Capt_Xforms
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo CaptXFormsInfo;
	FHoudiniApi::AttributeInfo_Init(&CaptXFormsInfo);
	CaptXFormsInfo.count = 1;
	CaptXFormsInfo.tupleSize = 16;
	CaptXFormsInfo.exists = true;
	CaptXFormsInfo.owner = HAPI_ATTROWNER_DETAIL;
	CaptXFormsInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
	CaptXFormsInfo.originalOwner = HAPI_ATTROWNER_DETAIL;
	CaptXFormsInfo.totalArrayElements = XFormsData.Num();
	CaptXFormsInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"capt_xforms", &CaptXFormsInfo), false);

	TArray<int32> SizesXFormsArray;
	SizesXFormsArray.Add(TotalBones);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "capt_xforms", &CaptXFormsInfo, XFormsData.GetData(),
		CaptXFormsInfo.totalArrayElements, SizesXFormsArray.GetData(), 0, CaptXFormsInfo.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// boneCapture_pCaptData
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo CaptDataInfo;
	FHoudiniApi::AttributeInfo_Init(&CaptDataInfo);
	CaptDataInfo.count = 1;
	CaptDataInfo.tupleSize = 20;  //The pCaptData property property contains exactly 20 floats
	CaptDataInfo.exists = true;
	CaptDataInfo.owner = HAPI_ATTROWNER_DETAIL;
	CaptDataInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
	CaptDataInfo.originalOwner = HAPI_ATTROWNER_DETAIL;
	CaptDataInfo.totalArrayElements = CaptData.Num(); //(bones * 20)
	CaptDataInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"boneCapture_pCaptData", &CaptDataInfo), false);

	TArray<int32> SizesCaptDataArray;
	SizesCaptDataArray.Add(TotalBones);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "boneCapture_pCaptData", &CaptDataInfo, CaptData.GetData(),
		CaptDataInfo.totalArrayElements, SizesCaptDataArray.GetData(), 0, CaptDataInfo.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// boneCapture_data
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo BoneCaptureDataInfo;
	FHoudiniApi::AttributeInfo_Init(&BoneCaptureDataInfo);
	BoneCaptureDataInfo.count = Part.pointCount;
	BoneCaptureDataInfo.tupleSize = 1;
	BoneCaptureDataInfo.exists = true;
	BoneCaptureDataInfo.owner = HAPI_ATTROWNER_POINT;
	BoneCaptureDataInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
	BoneCaptureDataInfo.originalOwner = HAPI_ATTROWNER_POINT;
	BoneCaptureDataInfo.totalArrayElements = BoneCaptureDataArray.Num();// Part.pointCount* InfluenceCount;
	BoneCaptureDataInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"boneCapture_data", &BoneCaptureDataInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "boneCapture_data", &BoneCaptureDataInfo, BoneCaptureDataArray.GetData(),
		BoneCaptureDataInfo.totalArrayElements, SizesBoneCaptureIndexArray.GetData(), 0, SizesBoneCaptureIndexArray.Num()), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// bonecapture_index
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo BoneCaptureIndexInfo;
	FHoudiniApi::AttributeInfo_Init(&BoneCaptureIndexInfo);
	BoneCaptureIndexInfo.count = Part.pointCount;
	BoneCaptureIndexInfo.tupleSize = 1;
	BoneCaptureIndexInfo.exists = true;
	BoneCaptureIndexInfo.owner = HAPI_ATTROWNER_POINT;
	BoneCaptureIndexInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
	BoneCaptureIndexInfo.originalOwner = HAPI_ATTROWNER_POINT;
	BoneCaptureIndexInfo.totalArrayElements = BoneCaptureIndexArray.Num();// Part.pointCount* InfluenceCount;
	BoneCaptureIndexInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"boneCapture_index", &BoneCaptureIndexInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "boneCapture_index", &BoneCaptureDataInfo, BoneCaptureIndexArray.GetData(),
		BoneCaptureIndexInfo.totalArrayElements, SizesBoneCaptureIndexArray.GetData(), 0, SizesBoneCaptureIndexArray.Num()), false);


	//--------------------------------------------------------------------------------------------------------------------- 
	// LOD GROUP AND SCREENSIZE
	//---------------------------------------------------------------------------------------------------------------------
	if (bAddLODGroups)
	{
		// LOD Group
		const char* LODGroupStr = "";
		{
			FString LODGroup = TEXT("lod") + FString::FromInt(LODIndex);
			LODGroupStr = TCHAR_TO_UTF8(*LODGroup);
		}

		// Add a LOD group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NewNodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr), false);

		// Set GroupMembership
		TArray<int> GroupArray;
		GroupArray.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < GroupArray.Num(); n++)
			GroupArray[n] = 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			NewNodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr,
			GroupArray.GetData(), 0, Part.faceCount), false);

		FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
		if (LODInfo)
		{
			// Add the lodX_screensize attribute
			FString LODAttributeName =
				TEXT(HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_PREFIX) + FString::FromInt(LODIndex) + TEXT(HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_POSTFIX);

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
				NewNodeId, 0, TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize), false);

			float lodscreensize = LODInfo->ScreenSize.Default;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
				TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize,
				&lodscreensize, 0, 1), false);
		}
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), NewNodeId), false);

	return true;
}


bool
FUnrealSkeletalMeshTranslator::CreateInputNodeForSkeletalMeshSockets(
	USkeletalMesh* InSkeletalMesh, 
	const HAPI_NodeId& InParentNodeId,
	HAPI_NodeId& OutSocketsNodeId)
{
	if (!InSkeletalMesh)
		return false;

	TArray<USkeletalMeshSocket*> InMeshSocket = InSkeletalMesh->GetActiveSocketList();
	int32 NumSockets = InMeshSocket.Num();
	if (NumSockets <= 0)
		return false;

	FReferenceSkeleton RefSK = InSkeletalMesh->GetRefSkeleton();
	TArray<FTransform> AllBonePos = RefSK.GetRawRefBonePose();

	// Create a new input node for the sockets
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
		InParentNodeId, TEXT("null"), "sockets", false, &OutSocketsNodeId), false);

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

	//  Create the bone name attrib info
	HAPI_AttributeInfo AttributeInfoBoneName;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoBoneName);
	AttributeInfoBoneName.count = NumSockets;
	AttributeInfoBoneName.tupleSize = 1;
	AttributeInfoBoneName.exists = true;
	AttributeInfoBoneName.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoBoneName.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoBoneName.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_BONE_NAME, &AttributeInfoBoneName), false);

	// Extract the sockets transform values
	TArray<float> SocketPos;
	SocketPos.SetNumZeroed(NumSockets * 3);
	TArray<float> SocketRot;
	SocketRot.SetNumZeroed(NumSockets * 4);
	TArray<float > SocketScale;
	SocketScale.SetNumZeroed(NumSockets * 3);

	TArray<FString> SocketNames;
	TArray<FString> SocketBoneNames;
	for (int32 Idx = 0; Idx < NumSockets; ++Idx)
	{
		USkeletalMeshSocket* CurrentSocket = InMeshSocket[Idx];
		if (!IsValid(CurrentSocket))
			continue;

		// Calculate the sockets transform
		// They are stored relative to a bone, so we first need to get the corresponding bone's transform
		int32 BoneIndex = RefSK.FindBoneIndex(CurrentSocket->BoneName);
		FTransform BoneTransform = AllBonePos.IsValidIndex(BoneIndex) ? AllBonePos[BoneIndex] : FTransform::Identity;
		do
		{
			int32 ParentIndex = RefSK.GetParentIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				BoneTransform = BoneTransform * AllBonePos[ParentIndex];
			}

			BoneIndex = ParentIndex;
		} while (RefSK.IsValidIndex(BoneIndex));


		//FTransform BoneTransform = AllBonePos.IsValidIndex(BoneIndex) ? AllBonePos[BoneIndex] : FTransform::Identity;
		FTransform RelSocketTransform(CurrentSocket->RelativeRotation, CurrentSocket->RelativeLocation, CurrentSocket->RelativeScale);
		FTransform SocketTransform = RelSocketTransform * BoneTransform;

		// Convert the socket transform to a HapiTransform		
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

		FString BoneName = CurrentSocket->BoneName.ToString();
		if (!BoneName.IsEmpty())
			SocketBoneNames.Add(BoneName);
		else
			SocketBoneNames.Add("");
	}

	//we can now upload them to our attribute.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		SocketPos, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPos), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		SocketRot, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, AttributeInfoRot), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		SocketScale, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE, AttributeInfoScale), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		SocketNames, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, AttributeInfoName), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		SocketBoneNames, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_BONE_NAME, AttributeInfoBoneName), false);

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			SocketNames[Idx], OutSocketsNodeId, 0, NameAttr, AttributeInfoName), false);

		//  Create the mesh_socketX_bone attrib info
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoBoneName);
		AttributeInfoBoneName.count = 1;
		AttributeInfoBoneName.tupleSize = 1;
		AttributeInfoBoneName.exists = true;
		AttributeInfoBoneName.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoBoneName.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoBoneName.originalOwner = HAPI_ATTROWNER_INVALID;

		FString BoneNameAttr = SocketAttrPrefix + TEXT("_bone_name");
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			OutSocketsNodeId, 0, TCHAR_TO_ANSI(*BoneNameAttr), &AttributeInfoBoneName), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			SocketBoneNames[Idx], OutSocketsNodeId, 0, BoneNameAttr, AttributeInfoBoneName), false);
	}

	// Now add the sockets group
	const char* SocketGroupStr = "socket_imported";
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), OutSocketsNodeId), false);

	return true;
}
