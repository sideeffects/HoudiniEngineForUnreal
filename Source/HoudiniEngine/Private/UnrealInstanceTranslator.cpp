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
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "UnrealMeshTranslator.h"

#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"

bool
FUnrealInstanceTranslator::HapiCreateInputNodeForInstancer(
	UInstancedStaticMeshComponent* ISMC, 
	const FString& InNodeName, 
	HAPI_NodeId& OutCreatedNodeId,
	const bool& bExportLODs,
	const bool& bExportSockets,
	const bool& bExportColliders,
	const bool& bExportAsAttributeInstancer)
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
	bool bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
		SM, SMNodeId, InNodeName, ISMC, bExportLODs, bExportSockets, bExportColliders);

	if (!bSuccess)
		return false;

	// To create the instance properly (via packed prim), we need to:
	// - create a copytopoints (with pack and instance enable
	// - an inputnode containing all of the instances transform as points
	// - plug the input node and the static mesh node in the copytopoints

	// Create the copytopoints SOP.
	int32 CopyNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		-1, TEXT("SOP/copytopoints"), InNodeName, true, &CopyNodeId), false);

	// set "Pack And Instance" (pack) to true
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CopyNodeId, "pack", 0, 1), false);

	// Get the copytopoints parent OBJ NodeID
	HAPI_NodeId ParentNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CopyNodeId);

	// Now create an input node for the instance transforms
	int32 InstancesNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		ParentNodeId, TEXT("null"), "instances", false, &InstancesNodeId), false);

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
			Positions[InstanceIdx * 3 + 0] = PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Positions[InstanceIdx * 3 + 1] = PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Positions[InstanceIdx * 3 + 2] = PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;

			// Convert Unreal Rotation to Houdini
			FQuat RotationQuaternion = CurTransform.GetRotation();
			Rotations[InstanceIdx * 4 + 0] = RotationQuaternion.X;
			Rotations[InstanceIdx * 4 + 1] = RotationQuaternion.Z;
			Rotations[InstanceIdx * 4 + 2] = RotationQuaternion.Y;
			Rotations[InstanceIdx * 4 + 3] = -RotationQuaternion.W;

			// Convert Unreal Scale to Houdini
			FVector ScaleVector = CurTransform.GetScale3D();
			Scales[InstanceIdx * 3 + 0] = ScaleVector.X;
			Scales[InstanceIdx * 3 + 1] = ScaleVector.Z;
			Scales[InstanceIdx * 3 + 2] = ScaleVector.Y;
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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Positions, InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Rotations, InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, AttributeInfoRotation), false);

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Scales, InstancesNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE, AttributeInfoScale), false);

		// Commit the instance point geo.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), InstancesNodeId), false);
	}
		
	// Connect the mesh to the copytopoints node's second input
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), CopyNodeId, 0, SMNodeId, 0), false);

	// Connect the instances to the copytopoints node's second input
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), CopyNodeId, 1, InstancesNodeId, 0), false);

	// Update this input object's node IDs
	OutCreatedNodeId = CopyNodeId;

	return true;
}