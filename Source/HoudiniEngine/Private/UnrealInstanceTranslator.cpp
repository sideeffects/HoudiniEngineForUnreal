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

#include "UnrealInstanceTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "UnrealMeshTranslator.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"

#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

bool
FUnrealInstanceTranslator::HapiCreateInputNodeForInstancer(
	UInstancedStaticMeshComponent* ISMC, 
	const FString& InNodeName, 
	HAPI_NodeId& OutCreatedNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const bool& bExportLODs,
	const bool& bExportSockets,
	const bool& bExportColliders,
	const bool& bExportAsAttributeInstancer,
	const bool bPreferNaniteFallbackMesh,
	bool bExportMaterialParameters,
	const bool& bInputNodesCanBeDeleted)
{
	int32 InstanceCount = ISMC->GetInstanceCount();
	if (InstanceCount < 1)
		return true;

	// Get the Static Mesh instanced by the component
	UStaticMesh* SM = ISMC->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	// Marshall the Static Mesh to Houdini
	int32 SMNodeId = -1;
	FString ISMCName = InNodeName + TEXT("_") + ISMC->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(ISMCName);

	FUnrealObjectInputHandle SMNodeHandle;
	bool bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
		SM,
		SMNodeId,
		InNodeName,
		SMNodeHandle,
		ISMC,
		bExportLODs,
		bExportSockets,
		bExportColliders,
		true,
		true,
		bPreferNaniteFallbackMesh,
		bExportMaterialParameters,
		false);

	if (!bSuccess)
		return false;

	// Modifier chain name for component overrides on the static mesh (used with the ref counted input system).
	const FName MeshChainName("sm_overrides");

	FString FinalInputNodeName = InNodeName;
	HAPI_NodeId ParentNodeId = -1;
	FUnrealObjectInputHandle ParentHandle;
	FUnrealObjectInputIdentifier Identifier;
	{
		// Build the identifier for the entry in the manager
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputOptions Options = SMNodeHandle.GetIdentifier().GetOptions();
		Identifier = FUnrealObjectInputIdentifier(ISMC, Options, bIsLeaf);

		// If the entry exists in the manager, the associated HAPI nodes are valid, and it is not marked as dirty, then
		// return the existing entry
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			// Check consistency: the ref node should have 0 or 1 entries. If it has more than one, then it is incompatible
			// and we need to proceed with recreation
			TSet<FUnrealObjectInputHandle> ReferencedNodes;
			if (FUnrealObjectInputUtils::GetReferencedNodes(Handle, ReferencedNodes))
			{
				if (ReferencedNodes.Num() == 1 && ReferencedNodes.Contains(SMNodeHandle))
				{
					HAPI_NodeId NodeId = -1;
					if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
					{
						// The obj merge should be along input 0 from NodeId until we get to a node that does not have
						// an input
						HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
						HAPI_NodeId CurrentNodeId = NodeId;
						HAPI_NodeId CurrentInputNodeId = -1;
						while (FHoudiniApi::QueryNodeInput(Session, CurrentNodeId, 0, &CurrentInputNodeId) == HAPI_RESULT_SUCCESS && CurrentInputNodeId >= 0)
						{
							CurrentNodeId = CurrentInputNodeId;
						}
						if (CurrentNodeId >= 0)
						{
							// Get the objpath1 node parm from the object_merge
							if (FHoudiniApi::GetParmNodeValue(Session, CurrentNodeId, TCHAR_TO_UTF8(TEXT("objpath1")), &CurrentNodeId) != HAPI_RESULT_SUCCESS)
								CurrentNodeId = -1;
						}

						// If SMNodeId is not currently the source of our connected objmerge (or there is no connected objmerge) we have to rebuild
						if (CurrentNodeId == SMNodeId)
						{
							// Make sure the node cant be deleted if needed
							if (!bInputNodesCanBeDeleted)
								FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

							OutHandle = Handle;
							OutCreatedNodeId = NodeId;
							return true;
						}
					}
				}
			}
		}
		// If the entry does not exist, or is invalid, then we need create it
		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// Set OutCreatedNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that OutCreatedNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, OutCreatedNodeId))
				OutCreatedNodeId = -1;
		}
		else
		{
			OutCreatedNodeId = -1;
		}
	}
	
	// To create the instance properly (via packed prim), we need to:
	// - create a copytopoints (with pack and instance enable
	// - an inputnode containing all of the instances transform as points
	// - plug the input node and the static mesh node in the copytopoints

	// Create the copytopoints SOP.
	HAPI_NodeId ObjectNodeId = -1;
	HAPI_NodeId CopyNodeId = -1;
	int32 MatNodeId = -1;
	{
		// If we have existing valid HAPI nodes (so we are rebuilding a dirty / old version) reuse those nodes. This
		// is quite important, to keep our obj merges / node references in _Houdini_ valid
		bool bCreateNodes = true;
		if (FUnrealObjectInputUtils::AreHAPINodesValid(Identifier))
		{
			if (FUnrealObjectInputUtils::GetHAPINodeId(Identifier, MatNodeId))
			{
				ObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(MatNodeId);
				if (ObjectNodeId >= 0)
				{
					bCreateNodes = false;
					if (FHoudiniApi::QueryNodeInput(FHoudiniEngine::Get().GetSession(), MatNodeId, 0, &CopyNodeId) == HAPI_RESULT_SUCCESS)
					{
						// Best effort: clean up: disconnect the object merge from input 0 (HAPI should delete it
						// automatically) and delete the instances null that is connected to input 1
						FHoudiniApi::DisconnectNodeInput(FHoudiniEngine::Get().GetSession(), CopyNodeId, 0);
						HAPI_NodeId OldInstancesId = -1;
						if (FHoudiniApi::QueryNodeInput(FHoudiniEngine::Get().GetSession(), CopyNodeId, 1, &OldInstancesId) == HAPI_RESULT_SUCCESS
								&& OldInstancesId >= 0)
						{
							FHoudiniEngineUtils::DeleteHoudiniNode(OldInstancesId);
						}
					}
				}
			}
		}
		
		if (bCreateNodes)
		{
			HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
				ParentNodeId, ParentNodeId < 0 ? TEXT("Object/geo") : TEXT("geo"), FinalInputNodeName, true, &ObjectNodeId), false);
			HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
				ObjectNodeId, ObjectNodeId < 0 ? TEXT("SOP/attribcreate") : TEXT("attribcreate"), FinalInputNodeName, true, &MatNodeId), false);
		}
	}

	if (CopyNodeId < 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
			ObjectNodeId, TEXT("copytopoints"), "copytopoints", false, &CopyNodeId), false);
	}

	// set "Pack And Instance" (pack) to true
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CopyNodeId, "pack", 0, 1), false);

	// Now create an input node for the instance transforms
	HAPI_NodeId InstancesNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		ObjectNodeId, TEXT("null"), "instances", false, &InstancesNodeId), false);

	// MARSHALL THE INSTANCE TRANSFORM
	{
		// Get the instance transform and convert them to Position/Rotation/Scale array
		TArray<float> Positions;
		Positions.SetNumZeroed(InstanceCount * 3);
		TArray<float> Rotations;
		Rotations.SetNumZeroed(InstanceCount * 4);
		TArray<float> Scales;
		Scales.SetNumZeroed(InstanceCount * 3);
		for (int32 InstanceIdx = 0; InstanceIdx < InstanceCount; InstanceIdx++)
		{
			FTransform CurTransform;
			ISMC->GetInstanceTransform(InstanceIdx, CurTransform);

			// Convert Unreal Position to Houdini
			FVector PositionVector = CurTransform.GetLocation();
			Positions[InstanceIdx * 3 + 0] = (float)PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Positions[InstanceIdx * 3 + 1] = (float)PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Positions[InstanceIdx * 3 + 2] = (float)PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;

			// Convert Unreal Rotation to Houdini
			FQuat RotationQuaternion = CurTransform.GetRotation();
			Rotations[InstanceIdx * 4 + 0] = (float)RotationQuaternion.X;
			Rotations[InstanceIdx * 4 + 1] = (float)RotationQuaternion.Z;
			Rotations[InstanceIdx * 4 + 2] = (float)RotationQuaternion.Y;
			Rotations[InstanceIdx * 4 + 3] = (float)-RotationQuaternion.W;

			// Convert Unreal Scale to Houdini
			FVector ScaleVector = CurTransform.GetScale3D();
			Scales[InstanceIdx * 3 + 0] = (float)ScaleVector.X;
			Scales[InstanceIdx * 3 + 1] = (float)ScaleVector.Z;
			Scales[InstanceIdx * 3 + 2] = (float)ScaleVector.Y;
		}

		// Create a part for the instance points.
		HAPI_PartInfo Part;
		FHoudiniApi::PartInfo_Init(&Part);
		Part.id = 0;
		Part.nameSH = 0;
		Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
		Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
		Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
		Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
		Part.vertexCount = 0;
		Part.faceCount = 0;
		Part.pointCount = InstanceCount;
		Part.type = HAPI_PARTTYPE_MESH;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
			FHoudiniEngine::Get().GetSession(), InstancesNodeId, 0, &Part), false);

		// Create position (P) attribute
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = InstanceCount;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

		// Now that we have raw positions, we can upload them for our attribute.

		FHoudiniHapiAccessor Accessor(InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, Positions), false);

		// Create Rotation (rot) attribute
		HAPI_AttributeInfo AttributeInfoRotation;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoRotation);
		AttributeInfoRotation.count = InstanceCount;
		AttributeInfoRotation.tupleSize = 4;
		AttributeInfoRotation.exists = true;
		AttributeInfoRotation.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoRotation.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoRotation.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, &AttributeInfoRotation), false);

		Accessor.Init(InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoRotation, Rotations), false);

		// Create scale attribute
		HAPI_AttributeInfo AttributeInfoScale;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoScale);
		AttributeInfoScale.count = InstanceCount;
		AttributeInfoScale.tupleSize = 3;
		AttributeInfoScale.exists = true;
		AttributeInfoScale.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE, &AttributeInfoScale), false);

		Accessor.Init(InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoScale, Scales), false);

		// Commit the instance point geo.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(InstancesNodeId), false);
	}

	// Connect the mesh to the copytopoints node's second input
	// Make sure to set the XFormType fopr the obejct merge created to None
	FHoudiniEngineUtils::HapiConnectNodeInput(CopyNodeId, 0, SMNodeId, 0, 0);

	// Connect the instances to the copytopoints node's second input
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), CopyNodeId, 1, InstancesNodeId, 0), false);

	FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), MatNodeId, "numattr", 0, SM->GetStaticMaterials().Num());
	HAPI_ParmInfo ParmInfo;
	HAPI_PartId ParmId;

	const TArray<FStaticMaterial>& MeshMaterials = SM->GetStaticMaterials();
	int32 MatIdx = 0;
	for (const FStaticMaterial& Mat : MeshMaterials)
	{
		FString MatName = MeshMaterials.Num() == 1 ? HAPI_UNREAL_ATTRIB_MATERIAL : FString(HAPI_UNREAL_ATTRIB_MATERIAL) + FString::FromInt(MatIdx);

		// parm name is one indexed
		ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MatNodeId, "name" + std::to_string(++MatIdx), ParmInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MatNodeId, TCHAR_TO_ANSI(*MatName), ParmId, 0), false);

		// set attribute type to string (index 3)
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), MatNodeId, TCHAR_TO_ANSI(*(FString("type") + FString::FromInt(MatIdx))), 0, 3), false);

		// set value to path of material
		ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MatNodeId, "string" + std::to_string(MatIdx), ParmInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MatNodeId, TCHAR_TO_ANSI(*Mat.MaterialInterface->GetPathName(nullptr)), ParmId, 0), false);
	}
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), MatNodeId, 0, CopyNodeId, 0), false);

	// Update/create the entry in the input manager
	{
		// Record the node in the manager
		FUnrealObjectInputHandle Handle;
		TSet<FUnrealObjectInputHandle> ReferencedNodes({SMNodeHandle});
		//FHoudiniEngineUtils::AddNodeOrUpdateNode(Identifier, CopyNodeId, Handle, ObjectNodeId, &ReferencedNodes, bInputNodesCanBeDeleted);
		FUnrealObjectInputHandle CopyPointsHandle;
		//ReferencedNodes = { CopyPointsHandle };
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, MatNodeId, Handle, ObjectNodeId, &ReferencedNodes, bInputNodesCanBeDeleted))
			OutHandle = Handle;

		// First create the overrides on the static mesh before the copy + pack
		
		// Create the smc_overrides modifier chain if missing
		HAPI_NodeId SMObjMergeNodeId = -1;
		if (FHoudiniApi::QueryNodeInput(FHoudiniEngine::Get().GetSession(), CopyNodeId, 0, &SMObjMergeNodeId) != HAPI_RESULT_SUCCESS && SMObjMergeNodeId < 0)
		{
			// Destroy chain and log warning
			HOUDINI_LOG_WARNING(TEXT("Could not find obj merge input node for instancer copytopoints: removing '%s' modifier chain."), *MeshChainName.ToString());
			FUnrealObjectInputUtils::RemoveModifierChain(Handle, MeshChainName);
		}
		else
		{
			if (!FUnrealObjectInputUtils::DoesModifierChainExist(Handle, MeshChainName))
				FUnrealObjectInputUtils::AddModifierChain(Handle, MeshChainName, SMObjMergeNodeId);
			else
				FUnrealObjectInputUtils::SetModifierChainNodeToConnectTo(Handle, MeshChainName, SMObjMergeNodeId);

			// Make sure that a static mesh component overrides modifier exists for this component's input node
			if (!FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, MeshChainName, EUnrealObjectInputModifierType::MaterialOverrides))
				FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputMaterialOverrides>(Handle, MeshChainName, ISMC);

			// Ensure that the physical material override modifier exists for this component's input node
			if (!FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, MeshChainName, EUnrealObjectInputModifierType::PhysicalMaterialOverride))
				FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputPhysicalMaterialOverride>(Handle, MeshChainName, ISMC, HAPI_ATTROWNER_PRIM);
		}

		// Then create the output override chain
		const FName InstancerChainName(FUnrealObjectInputNode::OutputChainName);
		if (!FUnrealObjectInputUtils::DoesModifierChainExist(Handle, InstancerChainName))
			FUnrealObjectInputUtils::AddModifierChain(Handle, InstancerChainName, MatNodeId);
		else
			FUnrealObjectInputUtils::SetModifierChainNodeToConnectTo(Handle, InstancerChainName, MatNodeId);

		// Make sure that a static mesh component overrides modifier exists for this component's input node
		if (!FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, InstancerChainName, EUnrealObjectInputModifierType::MaterialOverrides))
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputMaterialOverrides>(Handle, InstancerChainName, ISMC, false);

		// Ensure that the physical material override modifier exists for this component's input node
		if (!FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, InstancerChainName, EUnrealObjectInputModifierType::PhysicalMaterialOverride))
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputPhysicalMaterialOverride>(Handle, InstancerChainName, ISMC, HAPI_ATTROWNER_POINT);

		// Data layer Modifier
		FUnrealObjectInputModifier* DataLayerModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, InstancerChainName, EUnrealObjectInputModifierType::DataLayerGroups);
		if (!DataLayerModifier)
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputDataLayer>(Handle, InstancerChainName, ISMC->GetOwner());
		}

		// HLODs
		FUnrealObjectInputModifier* HLODModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, InstancerChainName, EUnrealObjectInputModifierType::HLODAttributes);
		if (!HLODModifier)
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputHLODAttributes>(Handle, InstancerChainName, ISMC->GetOwner());
		}

		// Update all modifiers
		FUnrealObjectInputUtils::UpdateAllModifierChains(Handle);

		// Record the node that the static mesh obj merge is now connected to in the reference node
		FUnrealObjectInputUtils::SetReferencesNodeConnectToNodeId(Handle, FUnrealObjectInputUtils::GetInputNodeOfModifierChain(Handle, MeshChainName));

		// Ensure that the output of the sm chain is connected to the copytopoints node
		HAPI_NodeId SMChainOutputNodeId = FUnrealObjectInputUtils::GetOutputNodeOfModifierChain(Handle, MeshChainName);
		if (SMChainOutputNodeId >= 0 && FHoudiniEngineUtils::IsHoudiniNodeValid(SMChainOutputNodeId))
		{
			if (FHoudiniApi::ConnectNodeInput(
					FHoudiniEngine::Get().GetSession(), CopyNodeId, 0, SMChainOutputNodeId, 0) != HAPI_RESULT_SUCCESS)
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to connect the '%d' chain to the copytopoints instancer node."), (int32)SMChainOutputNodeId);
			}
		}
	}

	// Update this input object's node IDs
	OutCreatedNodeId = MatNodeId;

	return true;
}
