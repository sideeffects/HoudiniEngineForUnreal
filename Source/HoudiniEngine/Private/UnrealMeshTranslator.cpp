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

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"

#include "RawMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionOperations.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMeshSocket.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshAttributes.h"
#include "StaticMeshAttributes.h"

#if WITH_EDITOR
	#include "EditorFramework/AssetImportData.h"
#endif


static TAutoConsoleVariable<int32> CVarHoudiniEngineStaticMeshExportMethod(
	TEXT("HoudiniEngine.StaticMeshExportMethod"),
	1,
	TEXT("Controls the method used for exporting Static Meshes from Unreal to Houdini.\n")
	TEXT("0: Raw Mesh (legacy)\n")
	TEXT("1: Mesh description (default)\n")
	TEXT("2: Render Mesh / LODResources\n")
);

bool
FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
	UStaticMesh* StaticMesh,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	UStaticMeshComponent* StaticMeshComponent /* = nullptr */,
	const bool& ExportAllLODs /* = false */,
	const bool& ExportSockets /* = false */,
	const bool& ExportColliders /* = false */)
{
	// If we don't have a static mesh there's nothing to do.
	if (!IsValid(StaticMesh))
		return false;

	// Node ID for the newly created node
	HAPI_NodeId NewNodeId = -1;

	// Export sockets if there are some
	bool DoExportSockets = ExportSockets && (StaticMesh->Sockets.Num() > 0);

	// Export LODs if there are some
	bool DoExportLODs = ExportAllLODs && (StaticMesh->GetNumLODs() > 1);

	// Export colliders if there are some
	bool DoExportColliders = ExportColliders && StaticMesh->GetBodySetup() != nullptr;
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
		HOUDINI_CHECK_ERROR_RETURN(	FHoudiniEngineUtils::CreateNode(
			-1, TEXT("SOP/merge"), InputNodeName, true, &NewNodeId), false);
	}
	else
	{
		// No LODs/Sockets, we just need a single input node
		// If InputNodeId is invalid, we need to create an input node.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
			FHoudiniEngine::Get().GetSession(), &NewNodeId, TCHAR_TO_ANSI(*InputNodeName)), false);

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
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *InputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *InputNodeName);
		}		
	}

	// TODO:
	// Setting for lightmap resolution?	
	
	// Select the export method we want to use for Static Meshes:
	// 	   0 - Raw Mesh - Legacy UE4 method, deprecated
	// 	   1 - Mesh Description
	// 	   2 - Render Mesh / LODResources - As issue in UE5
	uint8 ExportMethod = 1; // Mesh description
	ExportMethod = (uint8)CVarHoudiniEngineStaticMeshExportMethod.GetValueOnAnyThread();

	// Next Index used to connect nodes to the merge
	int32 NextMergeIndex = 0;

	// Should we export the HiRes Nanite Mesh?
	const bool bNaniteBuildEnabled = StaticMesh->NaniteSettings.bEnabled;
	const bool bHaveHiResSourceModel = StaticMesh->IsHiResMeshDescriptionValid();
	bool bHiResMeshSuccess = false;
	if (bHaveHiResSourceModel && bNaniteBuildEnabled)
	{
		// Get the HiRes Mesh description and SourceModel
		FMeshDescription HiResMeshDescription = *StaticMesh->GetHiResMeshDescription();
		FStaticMeshSourceModel& HiResSrcModel = StaticMesh->GetHiResSourceModel();
		FMeshBuildSettings& HiResBuildSettings = HiResSrcModel.BuildSettings;		// cannot be const because FMeshDescriptionHelper modifies the LightmapIndex fields ?!?

		// If we're using a merge node, we need to create a new input null
		HAPI_NodeId CurrentNodeId = -1;
		if (UseMergeNode)
		{
			// Create a new input node for the HiRes Mesh in this input object's OBJ node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				InputObjectNodeId, TEXT("null"), TEXT("HiRes"), false, &CurrentNodeId), false);
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

	int32 NumLODsToExport = DoExportLODs ? StaticMesh->GetNumLODs() : 1;

	// Do not export LOD0 if we have exported the HiRes mesh and don't need additional LODs
	if (!DoExportLODs && bHiResMeshSuccess)
		NumLODsToExport = 0;

	for (int32 LODIndex = 0; LODIndex < NumLODsToExport; LODIndex++)
	{
		// Grab the LOD level.
		FStaticMeshSourceModel & SrcModel = StaticMesh->GetSourceModel(LODIndex);

		// If we're using a merge node, we need to create a new input null
		HAPI_NodeId CurrentLODNodeId = -1;
		if (UseMergeNode)
		{
			// Create a new input node for the current LOD
			const char * LODName = "";
			{
				FString LOD = TEXT("lod") + FString::FromInt(LODIndex);
				LODName = TCHAR_TO_UTF8(*LOD);
			}

			// Create the node in this input object's OBJ node
			HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
				InputObjectNodeId, TEXT("null"), LODName, false, &CurrentLODNodeId), false);
		}
		else
		{
			// No merge node, just use the input node we created before
			CurrentLODNodeId = NewNodeId;
		}

		// Either export the current LOD Mesh by using RawMEsh or MeshDescription (legacy)
		FMeshDescription* MeshDesc = nullptr;
		// if (!bExportViaRawMesh)
		if (ExportMethod == 1)
		{
			// This will either fetch the mesh description that is cached on the SrcModel
			// or load it from bulk data / DDC once
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

		bool bMeshSuccess = false;
		if (ExportMethod == 1 && MeshDesc)
		{
			// Convert the Mesh using FMeshDescription
			const double StartTime = FPlatformTime::Seconds();
			bMeshSuccess = FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
				CurrentLODNodeId,
				*MeshDesc,
				LODIndex,
				DoExportLODs,
				StaticMesh,
				StaticMeshComponent);
			HOUDINI_LOG_MESSAGE(TEXT("FUnrealMeshTranslator::CreateInputNodeForMeshDescription completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);
		}
		else if (ExportMethod == 2)
		{
			// Convert the LOD Mesh using FStaticMeshLODResources
			const double StartTime = FPlatformTime::Seconds();
			bMeshSuccess = FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources(
				CurrentLODNodeId,
				StaticMesh->GetLODForExport(LODIndex),
				LODIndex,
				DoExportLODs,
				StaticMesh,
				StaticMeshComponent);
			HOUDINI_LOG_MESSAGE(TEXT("FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);
		}
		else
		{
			// Convert the LOD Mesh using FRawMesh
			const double StartTime = FPlatformTime::Seconds();
			bMeshSuccess = FUnrealMeshTranslator::CreateInputNodeForRawMesh(
				CurrentLODNodeId,
				SrcModel,
				LODIndex,
				DoExportLODs,
				StaticMesh,
				StaticMeshComponent);
			HOUDINI_LOG_MESSAGE(TEXT("FUnrealMeshTranslator::CreateInputNodeForRawMesh completed in %.4f seconds"), FPlatformTime::Seconds() - StartTime);
		}

		if (!bMeshSuccess)
			continue;

		if (UseMergeNode)
		{
			// Connect the LOD node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, CurrentLODNodeId, 0), false);
		}

		NextMergeIndex++;
	}

	if (DoExportColliders)
	{
		FKAggregateGeom SimpleColliders = StaticMesh->GetBodySetup()->AggGeom;

		// Export BOX colliders
		for (auto& CurBox : SimpleColliders.BoxElems)
		{
			FVector BoxCenter = CurBox.Center;
			FVector BoxExtent = FVector(CurBox.X, CurBox.Y, CurBox.Z);
			FRotator BoxRotation = CurBox.Rotation;

			HAPI_NodeId BoxNodeId = -1;
			if (!CreateInputNodeForBox(
				BoxNodeId, InputObjectNodeId, NextMergeIndex,
				BoxCenter, BoxExtent, BoxRotation))
				continue;

			if (BoxNodeId < 0)
				continue;

			// Connect the Box node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, BoxNodeId, 0), false);

			NextMergeIndex++;
		}

		// Export SPHERE colliders
		for (auto& CurSphere : SimpleColliders.SphereElems)
		{
			HAPI_NodeId SphereNodeId = -1;
			if (!CreateInputNodeForSphere(
				SphereNodeId, InputObjectNodeId, NextMergeIndex,
				CurSphere.Center, CurSphere.Radius))
				continue;

			if (SphereNodeId < 0)
				continue;

			// Connect the Sphere node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, SphereNodeId, 0), false);

			NextMergeIndex++;
		}

		// Export CAPSULE colliders
		for (auto& CurSphyl : SimpleColliders.SphylElems)
		{
			HAPI_NodeId SphylNodeId = -1;
			if (!CreateInputNodeForSphyl(
				SphylNodeId, InputObjectNodeId, NextMergeIndex,
				CurSphyl.Center, CurSphyl.Rotation, CurSphyl.Radius, CurSphyl.Length))
				continue;

			if (SphylNodeId < 0)
				continue;

			// Connect the capsule node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, SphylNodeId, 0), false);

			NextMergeIndex++;
		}

		// Export CONVEX colliders
		for (auto& CurConvex : SimpleColliders.ConvexElems)
		{
			HAPI_NodeId ConvexNodeId = -1;
			if (!CreateInputNodeForConvex(
				ConvexNodeId, InputObjectNodeId, NextMergeIndex, CurConvex))
				continue;

			if (ConvexNodeId < 0)
				continue;

			// Connect the capsule node to the merge node.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				NewNodeId, NextMergeIndex, ConvexNodeId, 0), false);

			NextMergeIndex++;
		}
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

	//
	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForMeshSockets(
	const TArray<UStaticMeshSocket*>& InMeshSocket, const HAPI_NodeId& InParentNodeId, HAPI_NodeId& OutSocketsNodeId)
{
	int32 NumSockets = InMeshSocket.Num();
	if (NumSockets <= 0)
		return false;

	// Create a new input node for the sockets
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		SocketPos, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPos),	false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		SocketRot, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, AttributeInfoRot), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		SocketScale, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE, AttributeInfoScale), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		SocketNames, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, AttributeInfoName), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		SocketTags, OutSocketsNodeId, 0, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG, AttributeInfoTag), false);

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			SocketTags[Idx], OutSocketsNodeId, 0, TagAttr, AttributeInfoTag), false);
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), OutSocketsNodeId), false);

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForRawMesh(
	const HAPI_NodeId& NodeId,
	const FStaticMeshSourceModel& SourceModel,
	const int32& InLODIndex,
	const bool& bAddLODGroups,
	UStaticMesh* StaticMesh,
	UStaticMeshComponent* StaticMeshComponent )
{
	// Convert the Mesh using FRawMesh	
	FRawMesh RawMesh;
	SourceModel.LoadRawMesh(RawMesh);
	
	// Create part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);

	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = RawMesh.WedgeIndices.Num();
	Part.faceCount = RawMesh.WedgeIndices.Num() / 3;
	Part.pointCount = RawMesh.VertexPositions.Num();
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part), false);

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPoint );
	AttributeInfoPoint.count = RawMesh.VertexPositions.Num();
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	// Grab the build scale
	FVector BuildScaleVector = SourceModel.BuildSettings.BuildScale3D;

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITION (P)
	//--------------------------------------------------------------------------------------------------------------------- 
	if (RawMesh.VertexPositions.Num() > 3)
	{
		TArray<float> StaticMeshVertices;
		StaticMeshVertices.SetNumZeroed(RawMesh.VertexPositions.Num() * 3);
		for (int32 VertexIdx = 0; VertexIdx < RawMesh.VertexPositions.Num(); ++VertexIdx)
		{
			// Convert Unreal to Houdini
			const FVector & PositionVector = RawMesh.VertexPositions[VertexIdx];
			StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.X;
			StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Z;
			StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Y;
		}

		// Now that we have raw positions, we can upload them for our attribute.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			StaticMeshVertices, NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// UVS (uvX)
	//--------------------------------------------------------------------------------------------------------------------- 
	for (int32 MeshTexCoordIdx = 0; MeshTexCoordIdx < MAX_STATIC_TEXCOORDS; MeshTexCoordIdx++)
	{
		int32 StaticMeshUVCount = RawMesh.WedgeTexCoords[MeshTexCoordIdx].Num();
		if (StaticMeshUVCount > 0)
		{
			const TArray<FVector2f> & RawMeshUVs = RawMesh.WedgeTexCoords[MeshTexCoordIdx];
			TArray<FVector> StaticMeshUVs;
			StaticMeshUVs.Reserve(StaticMeshUVCount);

			// Transfer UV data.
			for (int32 UVIdx = 0; UVIdx < StaticMeshUVCount; UVIdx++)
				StaticMeshUVs.Emplace(RawMeshUVs[UVIdx].X, 1.0 - RawMeshUVs[UVIdx].Y, 0);

			// Convert Unreal to Houdini
			// We need to re-index UVs for wedges we swapped (due to winding differences).
			for (int32 WedgeIdx = 0; WedgeIdx + 2 < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
			{
				// We do not touch wedge 0 of this triangle, Swap 2 and 3 to reverse the winding order.
				StaticMeshUVs.SwapMemory(WedgeIdx + 1, WedgeIdx + 2);
			}

			// Construct the attribute name for this UV index.
			FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;
			if (MeshTexCoordIdx > 0)
				UVAttributeName += FString::Printf(TEXT("%d"), MeshTexCoordIdx + 1);

			// Create attribute for UVs
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.count = StaticMeshUVCount;
			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId,	0, TCHAR_TO_ANSI(*UVAttributeName), &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				(const float *)StaticMeshUVs.GetData(), NodeId, 0, UVAttributeName, AttributeInfoVertex), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// NORMALS (N)
	//---------------------------------------------------------------------------------------------------------------------
	if (RawMesh.WedgeTangentZ.Num() > 0)
	{
		TArray<FVector> ChangedNormals(RawMesh.WedgeTangentZ);
		
		// We need to re-index normals for wedges we swapped (due to winding differences).
		for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
		{
			FVector TangentZ1 = ChangedNormals[WedgeIdx + 1];
			FVector TangentZ2 = ChangedNormals[WedgeIdx + 2];

			ChangedNormals[WedgeIdx + 1] = TangentZ2;
			ChangedNormals[WedgeIdx + 2] = TangentZ1;
		}

		// We also need to swap the vector's Y and Z components
		for (int32 WedgeIdx = 0; WedgeIdx + 2 < RawMesh.WedgeIndices.Num(); WedgeIdx++)
			Swap(ChangedNormals[WedgeIdx].Y, ChangedNormals[WedgeIdx].Z);

		// Create attribute for normals.
		HAPI_AttributeInfo AttributeInfoVertex;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

		AttributeInfoVertex.count = ChangedNormals.Num();
		AttributeInfoVertex.tupleSize = 3;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId,	0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			(const float*)ChangedNormals.GetData(), NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL,AttributeInfoVertex), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// TANGENT (tangentu)
	//---------------------------------------------------------------------------------------------------------------------
	if (RawMesh.WedgeTangentX.Num() > 0)
	{
		TArray<FVector> ChangedTangentU(RawMesh.WedgeTangentX);

		// We need to re-index tangents for wedges we swapped (due to winding differences).
		for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
		{
			FVector TangentU1 = ChangedTangentU[WedgeIdx + 1];
			FVector TangentU2 = ChangedTangentU[WedgeIdx + 2];

			ChangedTangentU[WedgeIdx + 1] = TangentU2;
			ChangedTangentU[WedgeIdx + 2] = TangentU1;
		}

		// We also need to swap the vector's Y and Z components
		for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx++)
			Swap(ChangedTangentU[WedgeIdx].Y, ChangedTangentU[WedgeIdx].Z);

		// Create attribute for tangentu.
		HAPI_AttributeInfo AttributeInfoVertex;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

		AttributeInfoVertex.count = ChangedTangentU.Num();
		AttributeInfoVertex.tupleSize = 3;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId,	0, HAPI_UNREAL_ATTRIB_TANGENTU, &AttributeInfoVertex), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			(const float*)ChangedTangentU.GetData(), NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, AttributeInfoVertex), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// BINORMAL (tangentv)
	//---------------------------------------------------------------------------------------------------------------------
	if (RawMesh.WedgeTangentY.Num() > 0)
	{
		TArray<FVector> ChangedTangentV(RawMesh.WedgeTangentY);
		// We need to re-index normals for wedges we swapped (due to winding differences).
		for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
		{
			FVector TangentV1 = ChangedTangentV[WedgeIdx + 1];
			FVector TangentV2 = ChangedTangentV[WedgeIdx + 2];

			ChangedTangentV[WedgeIdx + 1] = TangentV2;
			ChangedTangentV[WedgeIdx + 2] = TangentV1;
		}

		for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx++)
			Swap(ChangedTangentV[WedgeIdx].Y, ChangedTangentV[WedgeIdx].Z);

		// Create attribute for normals.
		HAPI_AttributeInfo AttributeInfoVertex;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

		AttributeInfoVertex.count = ChangedTangentV.Num();
		AttributeInfoVertex.tupleSize = 3;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), 
			NodeId,	0, HAPI_UNREAL_ATTRIB_TANGENTV, &AttributeInfoVertex), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			(const float*)ChangedTangentV.GetData(), NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, AttributeInfoVertex), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// COLORS (Cd)
	//---------------------------------------------------------------------------------------------------------------------
	{
		// If we have instance override vertex colors on the StaticMeshComponent, 
		// we first need to propagate them to our copy of the RawMesh Vert Colors
		TArray<FLinearColor> ChangedColors;
		FStaticMeshRenderData* SMRenderData = StaticMesh->GetRenderData();

		if (StaticMeshComponent &&
			StaticMeshComponent->LODData.IsValidIndex(InLODIndex) &&
			StaticMeshComponent->LODData[InLODIndex].OverrideVertexColors &&
			SMRenderData &&
			SMRenderData->LODResources.IsValidIndex(InLODIndex))
		{
			FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
			FStaticMeshLODResources& RenderModel = SMRenderData->LODResources[InLODIndex];
			FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

			if (RenderModel.WedgeMap.Num() > 0 && ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
			{
				// Use the wedge map if it is available as it is lossless.
				int32 NumWedges = RawMesh.WedgeIndices.Num();
				if (RenderModel.WedgeMap.Num() == NumWedges)
				{
					int32 NumExistingColors = RawMesh.WedgeColors.Num();
					if (NumExistingColors < NumWedges)
					{
						RawMesh.WedgeColors.AddUninitialized(NumWedges - NumExistingColors);
					}

					// Replace mesh colors with override colors
					for (int32 i = 0; i < NumWedges; i++)
					{
						FColor WedgeColor = FColor::White;
						int32 Index = RenderModel.WedgeMap[i];
						if (Index != INDEX_NONE)
						{
							WedgeColor = ColorVertexBuffer.VertexColor(Index);
						}
						RawMesh.WedgeColors[i] = WedgeColor;
					}
				}
			}
		}

		// See if we have colors to upload.
		if (RawMesh.WedgeColors.Num() > 0)
		{
			ChangedColors.SetNumUninitialized(RawMesh.WedgeColors.Num());

			// Convert Unreal to Houdini
			// We need to re-index colors for wedges we swapped (due to winding differences).
			for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
			{
				ChangedColors[WedgeIdx + 0] = RawMesh.WedgeColors[WedgeIdx + 0].ReinterpretAsLinear();
				ChangedColors[WedgeIdx + 1] = RawMesh.WedgeColors[WedgeIdx + 2].ReinterpretAsLinear();
				ChangedColors[WedgeIdx + 2] = RawMesh.WedgeColors[WedgeIdx + 1].ReinterpretAsLinear();
			}
		}

		if (ChangedColors.Num() > 0)
		{
			// Extract the RGB colors
			TArray<float> ColorValues;
			ColorValues.SetNum(ChangedColors.Num() * 3);
			for (int32 colorIndex = 0; colorIndex < ChangedColors.Num(); colorIndex++)
			{
				ColorValues[colorIndex * 3] = ChangedColors[colorIndex].R;
				ColorValues[colorIndex * 3 + 1] = ChangedColors[colorIndex].G;
				ColorValues[colorIndex * 3 + 2] = ChangedColors[colorIndex].B;
			}

			// Create attribute for colors.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.count = ChangedColors.Num();
			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId,	0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				ColorValues, NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, AttributeInfoVertex), false);

			// Create the attribute for Alpha
			TArray<float> AlphaValues;
			AlphaValues.SetNum(ChangedColors.Num());
			for (int32 alphaIndex = 0; alphaIndex < ChangedColors.Num(); alphaIndex++)
				AlphaValues[alphaIndex] = ChangedColors[alphaIndex].A;

			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);
			AttributeInfoVertex.count = AlphaValues.Num();
			AttributeInfoVertex.tupleSize = 1;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId,	0, HAPI_UNREAL_ATTRIB_ALPHA, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				AlphaValues, NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, AttributeInfoVertex), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INDICES (VertexList)
	//---------------------------------------------------------------------------------------------------------------------
	if (RawMesh.WedgeIndices.Num() > 0)
	{
		TArray<int32> StaticMeshIndices;
		StaticMeshIndices.SetNumUninitialized(RawMesh.WedgeIndices.Num());

		// Convert Unreal to Houdini
		for (int32 IndexIdx = 0; IndexIdx < RawMesh.WedgeIndices.Num(); IndexIdx += 3)
		{
			// Swap indices to fix winding order.
			StaticMeshIndices[IndexIdx + 0] = RawMesh.WedgeIndices[IndexIdx + 0];
			StaticMeshIndices[IndexIdx + 1] = RawMesh.WedgeIndices[IndexIdx + 2];
			StaticMeshIndices[IndexIdx + 2] = RawMesh.WedgeIndices[IndexIdx + 1];
		}

		// We can now set vertex list.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
			StaticMeshIndices, NodeId, 0), false);

		// We need to generate array of face counts.
		TArray<int32> StaticMeshFaceCounts;
		StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < Part.faceCount; n++)
			StaticMeshFaceCounts[n] = 3;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
			StaticMeshFaceCounts, NodeId, 0), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// FACE MATERIALS
	//---------------------------------------------------------------------------------------------------------------------
	// Marshall face material indices.
	if (RawMesh.FaceMaterialIndices.Num() > 0)
	{
		// Create an array of Material Interfaces
		TArray<UMaterialInterface *> MaterialInterfaces;
		if (IsValid(StaticMeshComponent) && StaticMeshComponent->IsValidLowLevel())
		{
			// StaticMeshComponent->GetUsedMaterials(MaterialInterfaces, false);

			int32 NumMeshBasedMaterials = StaticMeshComponent->GetNumMaterials();
			TArray<UMaterialInterface *> MeshBasedMaterialInterfaces;
			MeshBasedMaterialInterfaces.SetNumUninitialized(NumMeshBasedMaterials);
			for (int32 i = 0; i < NumMeshBasedMaterials; i++)
				MeshBasedMaterialInterfaces[i] = StaticMeshComponent->GetMaterial(i);

			int32 NumSections = StaticMesh->GetNumSections(InLODIndex);
			MaterialInterfaces.SetNumUninitialized(NumSections);
			for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
			{
				FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(InLODIndex, SectionIndex);
				check(Info.MaterialIndex < NumMeshBasedMaterials);
				MaterialInterfaces[SectionIndex] = MeshBasedMaterialInterfaces[Info.MaterialIndex];
			}
		}
		else
		{
			// Query the Static mesh's materials
			for (int32 MatIdx = 0; MatIdx < StaticMesh->GetStaticMaterials().Num(); MatIdx++)
			{
				MaterialInterfaces.Add(StaticMesh->GetMaterial(MatIdx));
			}

			// Try to fix up inconsistencies between the RawMesh / StaticMesh material indexes
			// by using the meshes sections...
			// TODO: Fix me properly!
			// Proper fix would be to export the meshes via the FStaticMeshLODResources obtained
			// by GetLODForExport(), and then export the mesh by sections.
			FStaticMeshRenderData* SMRenderData = StaticMesh->GetRenderData();
			if (SMRenderData && SMRenderData->LODResources.IsValidIndex(InLODIndex))
			{
				TMap<int32, UMaterialInterface*> MapOfMaterials;
				FStaticMeshLODResources& LODResources = SMRenderData->LODResources[InLODIndex];
				for (int32 SectionIndex = 0; SectionIndex < LODResources.Sections.Num(); SectionIndex++)
				{
					// Get the material for each element at the current lod index
					int32 MaterialIndex = LODResources.Sections[SectionIndex].MaterialIndex;
					if (!MapOfMaterials.Contains(MaterialIndex))
					{
						MapOfMaterials.Add(MaterialIndex, StaticMesh->GetMaterial(MaterialIndex));
					}
				}

				if (MapOfMaterials.Num() > 0)
				{
					// Sort the output material in the correct order (by material index)
					MapOfMaterials.KeySort([](int32 A, int32 B) { return A < B; });

					// Set the value in the correct order
					// Do not reduce the array of materials, this could cause crahses in some weird cases..
					if (MapOfMaterials.Num() > MaterialInterfaces.Num())
						MaterialInterfaces.SetNumZeroed(MapOfMaterials.Num());

					int32 MaterialIndex = 0;
					for (auto Kvp : MapOfMaterials)
					{
						MaterialInterfaces[MaterialIndex++] = Kvp.Value;
					}
				}
			}
		}

		// List of materials, one for each face.
		TArray<FString> StaticMeshFaceMaterials;

		//Lists of material parameters
		TMap<FString, TArray<float>> ScalarMaterialParameters;
		TMap<FString, TArray<float>> VectorMaterialParameters;
		TMap<FString, TArray<FString>> TextureMaterialParameters;

		bool bAttributeSuccess = false;
		bool bAddMaterialParametersAsAttributes = false;

		if (bAddMaterialParametersAsAttributes)
		{
			// Create attributes for the material and all its parameters
			// Get material attribute data, and all material parameters data
			FUnrealMeshTranslator::CreateFaceMaterialArray(
				MaterialInterfaces, RawMesh.FaceMaterialIndices, StaticMeshFaceMaterials,
				ScalarMaterialParameters, VectorMaterialParameters, TextureMaterialParameters);

			// Create attribute for materials and all attributes for material parameters
			bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
				NodeId,
				0,
				RawMesh.FaceMaterialIndices.Num(),
				StaticMeshFaceMaterials,
				ScalarMaterialParameters,
				VectorMaterialParameters,
				TextureMaterialParameters);
		}
		else
		{
			// Create attributes only for the materials
			// Only get the material attribute data
			FUnrealMeshTranslator::CreateFaceMaterialArray(
				MaterialInterfaces, RawMesh.FaceMaterialIndices, StaticMeshFaceMaterials);

			// Create attribute for materials
			bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
				NodeId,
				0,
				RawMesh.FaceMaterialIndices.Num(),
				StaticMeshFaceMaterials,
				ScalarMaterialParameters,
				VectorMaterialParameters,
				TextureMaterialParameters);
		}

		// Delete material names.
		//FUnrealMeshTranslator::DeleteFaceMaterialArray(StaticMeshFaceMaterials);

		/*// Delete texture material parameter names
		for (auto & Pair : TextureMaterialParameters)
		{
			FUnrealMeshTranslator::DeleteFaceMaterialArray(Pair.Value);
		}*/

		if (!bAttributeSuccess)
		{
			check(0);
			return false;
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// FACE SMOOTHING MASKS
	//---------------------------------------------------------------------------------------------------------------------
	if (RawMesh.FaceSmoothingMasks.Num() > 0)
	{
		HAPI_AttributeInfo AttributeInfoSmoothingMasks;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoSmoothingMasks);

		AttributeInfoSmoothingMasks.count = RawMesh.FaceSmoothingMasks.Num();
		AttributeInfoSmoothingMasks.tupleSize = 1;
		AttributeInfoSmoothingMasks.exists = true;
		AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoSmoothingMasks.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), 
			NodeId,	0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, &AttributeInfoSmoothingMasks), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
			(const int32*)RawMesh.FaceSmoothingMasks.GetData(), NodeId, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, AttributeInfoSmoothingMasks), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// LIGHTMAP RESOLUTION
	//---------------------------------------------------------------------------------------------------------------------

	// TODO:
	// Fetch default lightmap res from settings...
	int32 GeneratedLightMapResolution = 32;
	if (StaticMesh->GetLightMapResolution() != GeneratedLightMapResolution)
	{
		TArray< int32 > LightMapResolutions;
		LightMapResolutions.Add(StaticMesh->GetLightMapResolution());

		HAPI_AttributeInfo AttributeInfoLightMapResolution;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoLightMapResolution);
		AttributeInfoLightMapResolution.count = LightMapResolutions.Num();
		AttributeInfoLightMapResolution.tupleSize = 1;
		AttributeInfoLightMapResolution.exists = true;
		AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, &AttributeInfoLightMapResolution), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
			LightMapResolutions, NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, AttributeInfoLightMapResolution), false);
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

		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), 
			NodeId,	0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME, &AttributeInfo), false);

		HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::HapiSetAttributeStringData(
			StaticMesh->GetPathName(), NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME, AttributeInfo), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT SOURCE FILE
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		FString Filename;		
		if (UAssetImportData* ImportData = StaticMesh->AssetImportData)
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
				NodeId,	0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE, &AttributeInfo), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
				Filename, NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE, AttributeInfo), false);
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
	// LOD GROUP AND SCREENSIZE
	//---------------------------------------------------------------------------------------------------------------------
	if (bAddLODGroups)
	{
		// LOD Group
		const char * LODGroupStr = "";
		{
			FString LODGroup = TEXT("lod") + FString::FromInt(InLODIndex);
			LODGroupStr = TCHAR_TO_UTF8(*LODGroup);
		}

		// Add a LOD group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr), false);

		// Set GroupMembership
		TArray<int> GroupArray;
		GroupArray.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < GroupArray.Num(); n++)
			GroupArray[n] = 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr,
			GroupArray.GetData(), 0, Part.faceCount), false);

		if (!StaticMesh->bAutoComputeLODScreenSize)
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
				NodeId, 0, TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize), false);

			// TODO: FIX?
			// Get the actual screensize instead of the src model default?
			float lodscreensize = SourceModel.ScreenSize.Default;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(), NodeId, 0,
				TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize,
				&lodscreensize, 0, 1), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// COMPONENT AND ACTOR TAGS
	//---------------------------------------------------------------------------------------------------------------------
	if (IsValid(StaticMeshComponent))
	{
		TArray<FName> AllTags;
		for (auto& ComponentTag : StaticMeshComponent->ComponentTags)
			AllTags.AddUnique(ComponentTag);

		AActor* ParentActor = StaticMeshComponent->GetOwner();
		if (IsValid(ParentActor))
		{
			for (auto& ActorTag : ParentActor->Tags)
				AllTags.AddUnique(ActorTag);
		}

		// Try to create groups for the tags
		if (!FHoudiniEngineUtils::CreateGroupsFromTags(NodeId, 0, AllTags))
			HOUDINI_LOG_WARNING(TEXT("Could not create groups for the Static Mesh Component and Actor tags!"));

		if (IsValid(ParentActor))
		{
			// Add the unreal_actor_path attribute
			FHoudiniEngineUtils::AddActorPathAttribute(NodeId, 0, ParentActor, Part.faceCount);

			// Add the unreal_level_path attribute
			FHoudiniEngineUtils::AddLevelPathAttribute(NodeId, 0, ParentActor->GetLevel(), Part.faceCount);
			/*
			if (ULevel* Level = ParentActor->GetLevel())
			{
				LevelPath = Level->GetPathName();

				// We just want the path up to the first point
				int32 DotIndex;
				if (LevelPath.FindChar('.', DotIndex))
					LevelPath.LeftInline(DotIndex, false);

				AddLevelPathAttributeToMesh(NodeId, 0, LevelPath, Part.faceCount);
			}
			*/
		}
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), NodeId), false);

	return true;
}


bool
FUnrealMeshTranslator::CreateInputNodeForStaticMeshLODResources(
	const HAPI_NodeId& NodeId,
	const FStaticMeshLODResources& LODResources,
	const int32& InLODIndex,
	const bool& bAddLODGroups,
	UStaticMesh* StaticMesh,
	UStaticMeshComponent* StaticMeshComponent)
{
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
	FVector BuildScaleVector = SourceModel.BuildSettings.BuildScale3D;

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITION (P)
	//--------------------------------------------------------------------------------------------------------------------- 
	// In FStaticMeshLODResources each vertex instances stores its position, even if the positions are not unique (in other
	// words, in Houdini terminology, the number of points and vertices are the same. We'll do the same thing that Epic
	// does in FBX export: we'll run through all vertex instances and use a hash to determine which instances share a 
	// position, so that we can a smaller number of points than vertices, and vertices share point positions
	TArray<int32> UEVertexInstanceIdxToPointIdx;
	UEVertexInstanceIdxToPointIdx.Reserve(OrigNumVertexInstances);

	TMap<FVector, int32> PositionToPointIndexMap;
	PositionToPointIndexMap.Reserve(OrigNumVertexInstances);

	TArray<float> StaticMeshVertices;
	StaticMeshVertices.Reserve(OrigNumVertexInstances * 3);
	for (uint32 VertexInstanceIndex = 0; VertexInstanceIndex < OrigNumVertexInstances; ++VertexInstanceIndex)
	{
		// Convert Unreal to Houdini
		const FVector &PositionVector = LODResources.VertexBuffers.PositionVertexBuffer.VertexPosition(VertexInstanceIndex);
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		StaticMeshVertices, NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);

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
		FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
		FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

		if (ColorVertexBuffer.GetNumVertices() == LODResources.GetNumVertices())
		{
			bUseComponentOverrideColors = true;
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIAL INDEX -> MATERIAL INTERFACE
	//---------------------------------------------------------------------------------------------------------------------
	TArray<UMaterialInterface*> MaterialInterfaces;
	TArray<int32> TriangleMaterialIndices;

	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

	// If the static mesh component is valid, get the materials via the component to account for overrides
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
			if (bIsStaticMeshComponentValid)
			{
				Material = StaticMeshComponent->GetMaterial(MaterialIndex);
			}
			else
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
						for (uint32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
						{
							const FVector2f &UV = LODResources.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(UEVertexIndex, UVLayerIndex);
							UVs[UVLayerIndex][Float3Index + 0] = UV.X;
							UVs[UVLayerIndex][Float3Index + 1] = 1.0f - UV.Y;
							UVs[UVLayerIndex][Float3Index + 2] = 0;
						}
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// NORMALS (N)
					//---------------------------------------------------------------------------------------------------------------------
					if (bIsVertexInstanceNormalsValid)
					{
						const FVector4f &Normal = LODResources.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(UEVertexIndex);
						Normals[Float3Index + 0] = Normal.X;
						Normals[Float3Index + 1] = Normal.Z;
						Normals[Float3Index + 2] = Normal.Y;
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// TANGENT (tangentu)
					//---------------------------------------------------------------------------------------------------------------------
					if (bIsVertexInstanceTangentsValid)
					{
						const FVector4f &Tangent = LODResources.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(UEVertexIndex);
						Tangents[Float3Index + 0] = Tangent.X;
						Tangents[Float3Index + 1] = Tangent.Z;
						Tangents[Float3Index + 2] = Tangent.Y;
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// BINORMAL (tangentv)
					//---------------------------------------------------------------------------------------------------------------------
					// In order to calculate the binormal we also need the tangent and normal
					if (bIsVertexInstanceBinormalsValid)
					{
						FVector Binormal = LODResources.VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(UEVertexIndex);
						Binormals[Float3Index + 0] = Binormal.X;
						Binormals[Float3Index + 1] = Binormal.Z;
						Binormals[Float3Index + 2] = Binormal.Y;
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// COLORS (Cd)
					//---------------------------------------------------------------------------------------------------------------------
					if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
					{
						FVector4 Color = FLinearColor::White;
						if (bUseComponentOverrideColors)
						{
							FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
							FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;
							Color = ColorVertexBuffer.VertexColor(UEVertexIndex).ReinterpretAsLinear();
						}
						else
						{
							Color = LODResources.VertexBuffers.ColorVertexBuffer.VertexColor(UEVertexIndex).ReinterpretAsLinear();
						}
						RGBColors[Float3Index + 0] = Color[0];
						RGBColors[Float3Index + 1] = Color[1];
						RGBColors[Float3Index + 2] = Color[2];
						Alphas[HoudiniVertexIdx] = Color[3];
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

				HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
					UVs[UVLayerIndex], NodeId, 0, UVAttributeName, AttributeInfoVertex), false);
			}
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// NORMALS (N)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceNormalsValid)
		{
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Normals, NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// TANGENT (tangentu)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceTangentsValid)
		{
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Tangents, NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// BINORMAL (tangentv)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceBinormalsValid)
		{
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Binormals, NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// COLORS (Cd)
		//---------------------------------------------------------------------------------------------------------------------
		if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				RGBColors, NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, AttributeInfoVertex), false);

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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Alphas, NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, AttributeInfoVertex), false);
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
			TArray<FString> TriangleMaterials;

			//Lists of material parameters
			TMap<FString, TArray<float>> ScalarMaterialParameters;
			TMap<FString, TArray<float>> VectorMaterialParameters;
			TMap<FString, TArray<FString>> TextureMaterialParameters;

			bool bAttributeSuccess = false;
			bool bAddMaterialParametersAsAttributes = false;

			if (bAddMaterialParametersAsAttributes)
			{
				// Create attributes for the material and all its parameters
				// Get material attribute data, and all material parameters data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, TriangleMaterialIndices, TriangleMaterials,
					ScalarMaterialParameters, VectorMaterialParameters, TextureMaterialParameters);

				// Create attribute for materials and all attributes for material parameters
				bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
					NodeId,
					0,
					TriangleMaterials.Num(),
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters);
			}
			else
			{
				// Create attributes only for the materials
				// Only get the material attribute data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, TriangleMaterialIndices, TriangleMaterials);

				// Create attribute for materials
				bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
					NodeId,
					0,
					TriangleMaterials.Num(),
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters);
			}

			// Delete material names.
			//FUnrealMeshTranslator::DeleteFaceMaterialArray(TriangleMaterials);

			/*// Delete texture material parameter names
			for (auto & Pair : TextureMaterialParameters)
			{
				FUnrealMeshTranslator::DeleteFaceMaterialArray(Pair.Value);
			}*/

			if (!bAttributeSuccess)
			{
				check(0);
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

	// TODO:
	// Fetch default lightmap res from settings...
	int32 GeneratedLightMapResolution = 32;
	if (StaticMesh->GetLightMapResolution() != GeneratedLightMapResolution)
	{
		TArray< int32 > LightMapResolutions;
		LightMapResolutions.Add(StaticMesh->GetLightMapResolution());

		HAPI_AttributeInfo AttributeInfoLightMapResolution;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoLightMapResolution);
		AttributeInfoLightMapResolution.count = LightMapResolutions.Num();
		AttributeInfoLightMapResolution.tupleSize = 1;
		AttributeInfoLightMapResolution.exists = true;
		AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, &AttributeInfoLightMapResolution), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
			LightMapResolutions, NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, AttributeInfoLightMapResolution), false);
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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			StaticMesh->GetPathName(), NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME, AttributeInfo), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT SOURCE FILE
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		FString Filename;
		if (UAssetImportData* ImportData = StaticMesh->AssetImportData)
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
				Filename, NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE, AttributeInfo), false);
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
	// LOD GROUP AND SCREENSIZE
	//---------------------------------------------------------------------------------------------------------------------
	if (bAddLODGroups)
	{
		// LOD Group
		const char * LODGroupStr = "";
		{
			FString LODGroup = TEXT("lod") + FString::FromInt(InLODIndex);
			LODGroupStr = TCHAR_TO_UTF8(*LODGroup);
		}

		// Add a LOD group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr), false);

		// Set GroupMembership
		TArray<int> GroupArray;
		GroupArray.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < GroupArray.Num(); n++)
			GroupArray[n] = 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr,
			GroupArray.GetData(), 0, Part.faceCount), false);

		if (!StaticMesh->bAutoComputeLODScreenSize)
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
				NodeId, 0, TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize), false);

			// TODO: FIX?
			// Get the actual screensize instead of the src model default?
			float lodscreensize = SourceModel.ScreenSize.Default;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(), NodeId, 0,
				TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize,
				&lodscreensize, 0, 1), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// COMPONENT AND ACTOR TAGS
	//---------------------------------------------------------------------------------------------------------------------
	if (IsValid(StaticMeshComponent))
	{
		// Try to create groups for the static mesh component's tags
		if (StaticMeshComponent->ComponentTags.Num() > 0
			&& !FHoudiniEngineUtils::CreateGroupsFromTags(NodeId, 0, StaticMeshComponent->ComponentTags))
			HOUDINI_LOG_WARNING(TEXT("Could not create groups from the Static Mesh Component's tags!"));

		AActor* ParentActor = StaticMeshComponent->GetOwner();
		if (IsValid(ParentActor))
		{
			// Try to create groups for the parent Actor's tags
			if (ParentActor->Tags.Num() > 0
				&& !FHoudiniEngineUtils::CreateGroupsFromTags(NodeId, 0, ParentActor->Tags))
				HOUDINI_LOG_WARNING(TEXT("Could not create groups from the Static Mesh Component's parent actor tags!"));

			// Add the unreal_actor_path attribute
			FHoudiniEngineUtils::AddActorPathAttribute(NodeId, 0, ParentActor, Part.faceCount);

			// Add the unreal_level_path attribute
			FHoudiniEngineUtils::AddLevelPathAttribute(NodeId, 0, ParentActor->GetLevel(), Part.faceCount);

			/*
			// Add the unreal_level_path attribute
			FString LevelPath = ActorPath;// FString();
			if (ULevel* Level = ParentActor->GetLevel())
			{
				LevelPath = Level->GetPathName();

				// We just want the path up to the first point
				int32 DotIndex;
				if (LevelPath.FindChar('.', DotIndex))
					LevelPath.LeftInline(DotIndex, false);

				AddLevelPathAttributeToMesh(NodeId, 0, LevelPath, Part.faceCount);
			}
			*/
		}
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), NodeId), false);

	return true;
}


bool
FUnrealMeshTranslator::CreateInputNodeForMeshDescription(
	const HAPI_NodeId& NodeId,
	const FMeshDescription& MeshDescription,
	const int32& InLODIndex,
	const bool& bAddLODGroups,
	UStaticMesh* StaticMesh,
	UStaticMeshComponent* StaticMeshComponent)
{
	// Convert the Mesh using FMeshDescription
	// Get references to the attributes we are interested in
	// before sending to Houdini we'll check if each attribute is valid
	FStaticMeshConstAttributes MeshDescriptionAttributes(MeshDescription);

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

	// Grab the build scale
	const FStaticMeshSourceModel &SourceModel = StaticMesh->GetSourceModel(InLODIndex);
	FVector BuildScaleVector = SourceModel.BuildSettings.BuildScale3D;

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
			const FVector &PositionVector = VertexPositions.Get(VertexID);
			StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.X;
			StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Z;
			StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION * BuildScaleVector.Y;

			// Record the UE Vertex ID to Houdini Point Index lookup
			VertexIDToHIndex[VertexID.GetValue()] = VertexIdx;
			VertexIdx++;
		}

		// Now that we have raw positions, we can upload them for our attribute.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			StaticMeshVertices, NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
	}

	bool bUseComponentOverrideColors = false;
	FStaticMeshRenderData* SMRenderData = StaticMesh->GetRenderData();

	// Determine if have override colors on the static mesh component, if so prefer to use those
	if (StaticMeshComponent &&
		StaticMeshComponent->LODData.IsValidIndex(InLODIndex) &&
		StaticMeshComponent->LODData[InLODIndex].OverrideVertexColors &&
		SMRenderData &&
		SMRenderData->LODResources.IsValidIndex(InLODIndex))
	{
		FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
		FStaticMeshLODResources& RenderModel = SMRenderData->LODResources[InLODIndex];
		FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

		if (RenderModel.WedgeMap.Num() > 0 && ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
		{
			// Use the wedge map if it is available as it is lossless.
			if (RenderModel.WedgeMap.Num() == NumVertexInstances)
			{
				bUseComponentOverrideColors = true;
			}
		}
	}

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

	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

	// If the static mesh component is valid, get the materials via the component to account for overrides
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
			if (bIsStaticMeshComponentValid)
			{
				Material = StaticMeshComponent->GetMaterial(MaterialIndex);
			}
			else
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
		if (StaticMesh->GetNumSections(InLODIndex) <= SectionIndex)
		{
			HOUDINI_LOG_ERROR(TEXT("Found more non-empty polygon groups in the mesh description for LOD %d than sections in the static mesh..."), InLODIndex);
			return false;
		}

		// If the MaterialIndex referenced by this Section is out of range, fill MaterialInterfaces with UEDefaultMaterial
		// up to and including MaterialIndex and log a warning
		int32 MaterialIndex = StaticMesh->GetSectionInfoMap().Get(InLODIndex, SectionIndex).MaterialIndex;
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

		if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
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
						for (int32 UVLayerIndex = 0; UVLayerIndex < NumUVLayers; ++UVLayerIndex)
						{
							const FVector2f &UV = VertexInstanceUVs.Get(VertexInstanceID, UVLayerIndex);
							UVs[UVLayerIndex][Float3Index + 0] = UV.X;
							UVs[UVLayerIndex][Float3Index + 1] = 1.0f - UV.Y;
							UVs[UVLayerIndex][Float3Index + 2] = 0;
						}
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// NORMALS (N)
					//---------------------------------------------------------------------------------------------------------------------
					if (bIsVertexInstanceNormalsValid)
					{
						const FVector &Normal = VertexInstanceNormals.Get(VertexInstanceID);
						Normals[Float3Index + 0] = Normal.X;
						Normals[Float3Index + 1] = Normal.Z;
						Normals[Float3Index + 2] = Normal.Y;
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// TANGENT (tangentu)
					//---------------------------------------------------------------------------------------------------------------------
					if (bIsVertexInstanceTangentsValid)
					{
						const FVector &Tangent = VertexInstanceTangents.Get(VertexInstanceID);
						Tangents[Float3Index + 0] = Tangent.X;
						Tangents[Float3Index + 1] = Tangent.Z;
						Tangents[Float3Index + 2] = Tangent.Y;
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// BINORMAL (tangentv)
					//---------------------------------------------------------------------------------------------------------------------
					// In order to calculate the binormal we also need the tangent and normal
					if (bIsVertexInstanceBinormalSignsValid && bIsVertexInstanceTangentsValid && bIsVertexInstanceNormalsValid)
					{
						const float &BinormalSign = VertexInstanceBinormalSigns.Get(VertexInstanceID);
						FVector Binormal = FVector::CrossProduct(
							FVector(Tangents[Float3Index + 0], Tangents[Float3Index + 1], Tangents[Float3Index + 2]),
							FVector(Normals[Float3Index + 0], Normals[Float3Index + 1], Normals[Float3Index + 2])
						) * BinormalSign;
						Binormals[Float3Index + 0] = Binormal.X;
						Binormals[Float3Index + 1] = Binormal.Y;
						Binormals[Float3Index + 2] = Binormal.Z;
					}

					//--------------------------------------------------------------------------------------------------------------------- 
					// COLORS (Cd)
					//---------------------------------------------------------------------------------------------------------------------
					if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
					{
						FVector4f Color = FLinearColor::White;
						if (bUseComponentOverrideColors && SMRenderData)
						{
							FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[InLODIndex];
							FStaticMeshLODResources& RenderModel = SMRenderData->LODResources[InLODIndex];
							FColorVertexBuffer& ColorVertexBuffer = *ComponentLODInfo.OverrideVertexColors;

							int32 Index = RenderModel.WedgeMap[VertexInstanceIdx];
							if (Index != INDEX_NONE)
							{
								Color = ColorVertexBuffer.VertexColor(Index).ReinterpretAsLinear();
							}
						}
						else
						{
							Color = VertexInstanceColors.Get(VertexInstanceID);
						}
						RGBColors[Float3Index + 0] = Color[0];
						RGBColors[Float3Index + 1] = Color[1];
						RGBColors[Float3Index + 2] = Color[2];
						Alphas[VertexInstanceIdx] = Color[3];
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
				const FPolygonGroupID &PolygonGroupID = MeshDescription.GetPolygonPolygonGroup(PolygonID);
				const int32 MaterialIndex = PolygonGroupToMaterialIndex.FindChecked(PolygonGroupID);
				TriangleMaterialIndices.Add(MaterialIndex);

				TriangleIdx++;
			}
		}

		// Now transfer valid vertex instance attributes to Houdini vertex attributes

		//--------------------------------------------------------------------------------------------------------------------- 
		// UVS (uvX)
		//--------------------------------------------------------------------------------------------------------------------- 
		if (bIsVertexInstanceUVsValid)
		{
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

				HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
					UVs[UVLayerIndex], NodeId, 0, UVAttributeName, AttributeInfoVertex), false);
			}
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// NORMALS (N)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceNormalsValid)
		{
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Normals, NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// TANGENT (tangentu)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceTangentsValid)
		{
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Tangents, NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// BINORMAL (tangentv)
		//---------------------------------------------------------------------------------------------------------------------
		if (bIsVertexInstanceBinormalSignsValid)
		{
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Binormals, NodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// COLORS (Cd)
		//---------------------------------------------------------------------------------------------------------------------
		if (bUseComponentOverrideColors || bIsVertexInstanceColorsValid)
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				RGBColors, NodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, AttributeInfoVertex), false);

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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Alphas, NodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, AttributeInfoVertex), false);
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
			TArray<FString> TriangleMaterials;

			//Lists of material parameters
			TMap<FString, TArray<float>> ScalarMaterialParameters;
			TMap<FString, TArray<float>> VectorMaterialParameters;
			TMap<FString, TArray<FString>> TextureMaterialParameters;

			bool bAttributeSuccess = false;
			bool bAddMaterialParametersAsAttributes = false;

			if (bAddMaterialParametersAsAttributes)
			{
				// Create attributes for the material and all its parameters
				// Get material attribute data, and all material parameters data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, TriangleMaterialIndices, TriangleMaterials,
					ScalarMaterialParameters, VectorMaterialParameters, TextureMaterialParameters);

				// Create attribute for materials and all attributes for material parameters
				bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
					NodeId,
					0,
					TriangleMaterials.Num(),
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters);
			}
			else
			{
				// Create attributes only for the materials
				// Only get the material attribute data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, TriangleMaterialIndices, TriangleMaterials);

				// Create attribute for materials
				bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
					NodeId,
					0,
					TriangleMaterials.Num(),
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters);
			}

			// Delete material names.
			//FUnrealMeshTranslator::DeleteFaceMaterialArray(TriangleMaterials);
			/*
			// Delete texture material parameter names
			for (auto & Pair : TextureMaterialParameters)
			{
				FUnrealMeshTranslator::DeleteFaceMaterialArray(Pair.Value);
			}
			*/

			if (!bAttributeSuccess)
			{
				check(0);
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
				TriangleSmoothingMasks, NodeId, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, AttributeInfoSmoothingMasks), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// LIGHTMAP RESOLUTION
	//---------------------------------------------------------------------------------------------------------------------

	// TODO:
	// Fetch default lightmap res from settings...
	int32 GeneratedLightMapResolution = 32;	
	if (StaticMesh->GetLightMapResolution() != GeneratedLightMapResolution)
	{
		TArray<int32> LightMapResolutions;
		LightMapResolutions.Add(StaticMesh->GetLightMapResolution());

		HAPI_AttributeInfo AttributeInfoLightMapResolution;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoLightMapResolution);
		AttributeInfoLightMapResolution.count = LightMapResolutions.Num();
		AttributeInfoLightMapResolution.tupleSize = 1;
		AttributeInfoLightMapResolution.exists = true;
		AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, &AttributeInfoLightMapResolution), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
			LightMapResolutions, NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION, AttributeInfoLightMapResolution), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT MESH NAME
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		const FString MeshAssetPath = StaticMesh->GetPathName();
		TArray<FString> PrimitiveAttrs;
		PrimitiveAttrs.SetNum(Part.faceCount);
		for (int32 Ix = 0; Ix < Part.faceCount; ++Ix)
		{
			PrimitiveAttrs[Ix] = MeshAssetPath;
		}

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			PrimitiveAttrs, NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_MESH_NAME, AttributeInfo), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INPUT SOURCE FILE
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		FString Filename;
		if (UAssetImportData* ImportData = StaticMesh->AssetImportData)
		{
			for (const auto& SourceFile : ImportData->SourceData.SourceFiles)
			{
				Filename = UAssetImportData::ResolveImportFilename(SourceFile.RelativeFilename, ImportData->GetOutermost());
				break;
			}
		}

		if (!Filename.IsEmpty())
		{
			TArray<FString> PrimitiveAttrs;
			PrimitiveAttrs.SetNum(Part.faceCount);
			for (int32 Ix = 0; Ix < Part.faceCount; ++Ix)
			{
				//PrimitiveAttrs[Ix] = TEXT("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
				PrimitiveAttrs[Ix] = Filename;
			}

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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
				PrimitiveAttrs, NodeId, 0, HAPI_UNREAL_ATTRIB_INPUT_SOURCE_FILE, AttributeInfo), false);
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
	// LOD GROUP AND SCREENSIZE
	//---------------------------------------------------------------------------------------------------------------------
	if (bAddLODGroups)
	{
		// LOD Group
		const char * LODGroupStr = "";
		{
			FString LODGroup = TEXT("lod") + FString::FromInt(InLODIndex);
			LODGroupStr = TCHAR_TO_UTF8(*LODGroup);
		}

		// Add a LOD group
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr), false);

		// Set GroupMembership
		TArray<int> GroupArray;
		GroupArray.SetNumUninitialized(Part.faceCount);
		for (int32 n = 0; n < GroupArray.Num(); n++)
			GroupArray[n] = 1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, LODGroupStr,
			GroupArray.GetData(), 0, Part.faceCount), false);

		if (!StaticMesh->bAutoComputeLODScreenSize)
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
				NodeId, 0, TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize), false);

			// TODO: FIX?
			// Get the actual screensize instead of the src model default?
			float lodscreensize = SourceModel.ScreenSize.Default;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(), NodeId, 0,
				TCHAR_TO_UTF8(*LODAttributeName), &AttributeInfoLODScreenSize,
				&lodscreensize, 0, 1), false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// COMPONENT AND ACTOR TAGS
	//---------------------------------------------------------------------------------------------------------------------
	if (IsValid(StaticMeshComponent))
	{
		// Try to create groups for the static mesh component's tags
		if (StaticMeshComponent->ComponentTags.Num() > 0
			&& !FHoudiniEngineUtils::CreateGroupsFromTags(NodeId, 0, StaticMeshComponent->ComponentTags))
			HOUDINI_LOG_WARNING(TEXT("Could not create groups from the Static Mesh Component's tags!"));

		AActor* ParentActor = StaticMeshComponent->GetOwner();
		if (IsValid(ParentActor))
		{
			// Try to create groups for the parent Actor's tags
			if (ParentActor->Tags.Num() > 0
				&& !FHoudiniEngineUtils::CreateGroupsFromTags(NodeId, 0, ParentActor->Tags))
				HOUDINI_LOG_WARNING(TEXT("Could not create groups from the Static Mesh Component's parent actor tags!"));

			// Add the unreal_actor_path attribute
			FHoudiniEngineUtils::AddActorPathAttribute(NodeId, 0, ParentActor, Part.faceCount);

			// Add the unreal_level_path attribute
			FHoudiniEngineUtils::AddLevelPathAttribute(NodeId, 0, ParentActor->GetLevel(), Part.faceCount);

			/*
			FString LevelPath = FString();
			if (IsValid(ParentActor))
			{
				if (ULevel* Level = ParentActor->GetLevel())
				{
					LevelPath = Level->GetPathName();

					// We just want the path up to the first point
					int32 DotIndex;
					if (LevelPath.FindChar('.', DotIndex))
						LevelPath.LeftInline(DotIndex, false);

					AddLevelPathAttributeToMesh(NodeId, 0, LevelPath, Part.faceCount);
				}
			}
			*/
		}
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), NodeId), false);

	return true;
}


void
FUnrealMeshTranslator::CreateFaceMaterialArray(
	const TArray<UMaterialInterface* >& Materials,
	const TArray<int32>& FaceMaterialIndices,
	TArray<FString>& OutStaticMeshFaceMaterials)
{	
	// Get the default material
	UMaterialInterface * DefaultMaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
	FString DefaultMaterialName = DefaultMaterialInterface ? DefaultMaterialInterface->GetPathName() : TEXT("default");

	// We need to create list of unique materials.
	TArray<FString> UniqueMaterialList;

	UMaterialInterface* MaterialInterface = nullptr;
	if (Materials.Num())
	{
		// We have materials.
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); MaterialIdx++)
		{
			MaterialInterface = Materials[MaterialIdx];
			if (!MaterialInterface)
			{
				// Null material interface found, add default instead.
				UniqueMaterialList.Add(DefaultMaterialName);
			}
			else
			{
				// We found a material, get its name		
				UniqueMaterialList.Add(MaterialInterface->GetPathName());
			}
		}
	}
	else
	{
		// We do not have any materials, just add default.
		UniqueMaterialList.Add(DefaultMaterialName);
	}

	// TODO: Needs to be improved!
	// We shouldnt be testing for each face, but only for each unique facematerial value...
	for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
	{
		int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
		if (UniqueMaterialList.IsValidIndex(FaceMaterialIdx))
		{
			OutStaticMeshFaceMaterials.Add(UniqueMaterialList[FaceMaterialIdx]);
		}
		else
		{
			OutStaticMeshFaceMaterials.Add(DefaultMaterialName);
		}
	}
}


void
FUnrealMeshTranslator::CreateFaceMaterialArray(
	const TArray<UMaterialInterface* >& Materials,
	const TArray<int32>& FaceMaterialIndices,
	TArray<FString>& OutStaticMeshFaceMaterials,
	TMap<FString, TArray<float>> & OutScalarMaterialParameters,
	TMap<FString, TArray<float>> & OutVectorMaterialParameters,
	TMap<FString, TArray<FString>> & OutTextureMaterialParameters)
{
	// Get the default material
	UMaterialInterface* DefaultMaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());
	FString DefaultMaterialName = DefaultMaterialInterface ? DefaultMaterialInterface->GetPathName() : TEXT("default");

	// We need to create list of unique materials.
	TArray<FString> UniqueMaterialList;

	// Initialize material parameter arrays
	TMap<FString, TArray<float>> ScalarParams;
	TMap<FString, TArray<FLinearColor>> VectorParams;
	TMap<FString, TArray<FString>> TextureParams;

	UMaterialInterface* MaterialInterface = nullptr;
	if (Materials.Num() > 0)
	{
		// We have materials.
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); MaterialIdx++)
		{
			MaterialInterface = Materials[MaterialIdx];
			if (!MaterialInterface)
			{
				// Null material interface found, add default instead.
				UniqueMaterialList.Add(DefaultMaterialName);

				// No need to collect material parameters on the default material
				continue;
			}

			// We found a material, get its name and material parameters
			UniqueMaterialList.Add(MaterialInterface->GetPathName());

			// Collect all scalar parameters in this material
			{
				TArray<FMaterialParameterInfo> MaterialScalarParamInfos;
				TArray<FGuid> MaterialScalarParamGuids;
				MaterialInterface->GetAllScalarParameterInfo(MaterialScalarParamInfos, MaterialScalarParamGuids);

				for (auto& CurScalarParam : MaterialScalarParamInfos)
				{
					FString CurScalarParamName = CurScalarParam.Name.ToString();
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
					FString CurVectorParamName = CurVectorParam.Name.ToString();
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
					FString CurTextureParamName = CurTextureParam.Name.ToString();
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

		}
	}
	else
	{
		// We do not have any materials, add default.
		UniqueMaterialList.Add(DefaultMaterialName);
	}

	// TODO: Needs to be improved!
	// We shouldnt be testing for each face, but only for each unique facematerial value...
	for (int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
	{
		int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
		if(UniqueMaterialList.IsValidIndex(FaceMaterialIdx))
		{
			OutStaticMeshFaceMaterials.Add(UniqueMaterialList[FaceMaterialIdx]);

			for (auto & Pair : ScalarParams)
			{
				OutScalarMaterialParameters[Pair.Key].Add(Pair.Value[FaceMaterialIdx]);
			}
			
			for (auto & Pair : VectorParams)
			{
				OutVectorMaterialParameters[Pair.Key].Add(Pair.Value[FaceMaterialIdx].R);
				OutVectorMaterialParameters[Pair.Key].Add(Pair.Value[FaceMaterialIdx].G);
				OutVectorMaterialParameters[Pair.Key].Add(Pair.Value[FaceMaterialIdx].B);
				OutVectorMaterialParameters[Pair.Key].Add(Pair.Value[FaceMaterialIdx].A);
			}

			for (auto & Pair : TextureParams)
			{
				OutTextureMaterialParameters[Pair.Key].Add(Pair.Value[FaceMaterialIdx]);
			}
		}
		else
		{
			OutStaticMeshFaceMaterials.Add(DefaultMaterialName);
		}
	}
}

/*
void
FUnrealMeshTranslator::DeleteFaceMaterialArray(TArray<char *>& OutStaticMeshFaceMaterials)
{
	// Clean up the memory allocated by CreateFaceMaterialArray()
	TSet<char *> UniqueMaterials(OutStaticMeshFaceMaterials);
	for (TSet<char *>::TIterator Iter = UniqueMaterials.CreateIterator(); Iter; ++Iter)
	{
		char* MaterialName = *Iter;
		FMemory::Free(MaterialName);
	}

	OutStaticMeshFaceMaterials.Empty();
}*/

bool
FUnrealMeshTranslator::CreateInputNodeForBox(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId& InParentNodeID,
	const int32& ColliderIndex,
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
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "size", 0, BoxExtent.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "size", 1, BoxExtent.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "size", 2, BoxExtent.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "t", 0, BoxCenter.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "t", 1, BoxCenter.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "t", 2, BoxCenter.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	// Do coordinate system conversion before sending to Houdini
	FQuat RotationQuat = BoxRotation.Quaternion();

	Swap(RotationQuat.Y, RotationQuat.Z);
	RotationQuat.W = -RotationQuat.W;
	const FRotator Rotator = RotationQuat.Rotator();

	// Negate roll and pitch since they are actually RHR
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "r", 0, -Rotator.Roll);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "r", 1, -Rotator.Pitch);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), BoxNodeId, "r", 2, Rotator.Yaw);
	
	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), BoxNodeId, &CookOptions);
	*/
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
	const char * GroupNameStr = "";
	{
		FString LODGroup = TEXT("collision_geo_simple_box") + FString::FromInt(ColliderIndex);
		GroupNameStr = TCHAR_TO_UTF8(*LODGroup);
	}
	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, GroupNameStr, parmId, 0);

	// Connect the box to the group
	FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, BoxNodeId, 0);

	OutNodeId = GroupNodeId;

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForSphere(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId& InParentNodeID,
	const int32& ColliderIndex,
	const FVector& SphereCenter,
	const float& SphereRadius)
{
	// Create a new input node for the sphere collider
	const char * SphereName = "";
	{
		FString SPH = TEXT("Sphere") + FString::FromInt(ColliderIndex);
		SphereName = TCHAR_TO_UTF8(*SPH);
	}

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
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "t", 0, SphereCenter.X / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "t", 1, SphereCenter.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "t", 2, SphereCenter.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION);

	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "type", 0, 1);
	/*
	FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), SphereNodeId, "scale", 0, SphereRadius / HAPI_UNREAL_SCALE_FACTOR_POSITION);
	*/

	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), SphereNodeId, &CookOptions);
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
	const char * GroupNameStr = "";
	{
		FString LODGroup = TEXT("collision_geo_simple_sphere") + FString::FromInt(ColliderIndex);
		GroupNameStr = TCHAR_TO_UTF8(*LODGroup);
	}
	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, GroupNameStr, parmId, 0);

	// Connect the box to the group
	FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, SphereNodeId, 0);

	OutNodeId = GroupNodeId;

	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForSphyl(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId& InParentNodeID,
	const int32& ColliderIndex,
	const FVector& SphylCenter,
	const FRotator& SphylRotation,
	const float& SphylRadius,
	const float& SphereLength)
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
			Vertices[VIx * 3 + 0] = CurPosition.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[VIx * 3 + 1] = CurPosition.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[VIx * 3 + 2] = CurPosition.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;
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
	const char * GroupNameStr = "";
	{
		FString LODGroup = TEXT("collision_geo_simple_capsule") + FString::FromInt(ColliderIndex);
		GroupNameStr = TCHAR_TO_UTF8(*LODGroup);
	}
	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, GroupNameStr, parmId, 0);

	// Connect the box to the group
	FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, 0, SphylNodeId, 0);

	OutNodeId = GroupNodeId;
	
	return true;
}

bool
FUnrealMeshTranslator::CreateInputNodeForConvex(
	HAPI_NodeId& OutNodeId,
	const HAPI_NodeId& InParentNodeID,
	const int32& ColliderIndex,
	const FKConvexElem& ConvexCollider)
{
	TArray<float> Vertices;
	TArray<int32> Indices;

	FTransform ConvexTransform = ConvexCollider.GetTransform();

	FVector TransformOffset = ConvexTransform.GetLocation();
	FVector ScaleOffset = ConvexTransform.GetScale3D();
	FQuat RotationOffset = ConvexTransform.GetRotation();

#if PHYSICS_INTERFACE_PHYSX
	if (ConvexCollider.GetConvexMesh() || ConvexCollider.GetMirroredConvexMesh())
#elif WITH_CHAOS
	//if (ConvexCollider.GetChaosConvexMesh().IsValid())
	if (ConvexCollider.IndexData.Num() > 0 && ConvexCollider.IndexData.Num() % 3 == 0)
#else
	if(false)
#endif
	{
		// Get the convex colliders vertices and indices from the mesh
		TArray<FDynamicMeshVertex> VertexBuffer;
		TArray<uint32> IndexBuffer;
		ConvexCollider.AddCachedSolidConvexGeom(VertexBuffer, IndexBuffer, FColor::White);

		for (int32 i = 0; i < VertexBuffer.Num(); i++)
		{
			VertexBuffer[i].Position =  TransformOffset + (RotationOffset * (ScaleOffset * VertexBuffer[i].Position));
		}

		Vertices.SetNum(VertexBuffer.Num() * 3);
		int32 CurIndex = 0;
		for (auto& CurVert : VertexBuffer)
		{
			Vertices[CurIndex * 3 + 0] = CurVert.Position.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 1] = CurVert.Position.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 2] = CurVert.Position.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;
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
			Vertices[CurIndex * 3 + 0] = CurVert.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 1] = CurVert.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[CurIndex * 3 + 2] = CurVert.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;

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

		/*
		for (int32 Idx = 0; Idx + 3 < NumVert; Idx+= 4)
		{
			Indices.Add(Idx + 0);
			Indices.Add(Idx + 1);
			Indices.Add(Idx + 2);

			Indices.Add(Idx + 2);
			Indices.Add(Idx + 1);
			Indices.Add(Idx + 3);
		}
		*/
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
	const char * GroupNameStr = "";
	{
		FString LODGroup = TEXT("collision_geo_simple_ucx") + FString::FromInt(ColliderIndex);
		GroupNameStr = TCHAR_TO_UTF8(*LODGroup);
	}
	FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), GroupNodeId, GroupNameStr, parmId, 0);

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
	const HAPI_NodeId& InParentNodeID,
	const int32& ColliderIndex,
	const FString& ColliderName,
	const TArray<float>& ColliderVertices,
	const TArray<int32>& ColliderIndices)
{
	// Create a new input node for the collider
	const char * ColliderNameStr = TCHAR_TO_UTF8(*ColliderName);

	// Create the node in this input object's OBJ node
	HAPI_NodeId ColliderNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		InParentNodeID, "null", ColliderNameStr, false, &ColliderNodeId), false);

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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
		ColliderVertices, ColliderNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);

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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), ColliderNodeId), false);

	OutNodeId = ColliderNodeId;

	return true;
}

bool 
FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
	const int32 & NodeId,
	const int32 & PartId,
	const int32 & Count,
	const TArray<FString> & TriangleMaterials,
	const TMap<FString, TArray<float>>& ScalarMaterialParameters,
	const TMap<FString, TArray<float>>& VectorMaterialParameters,
	const TMap<FString, TArray<FString>>& TextureMaterialParameters)
{
	if (NodeId < 0)
		return false;

	bool bSuccess = true;

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
		if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiSetAttributeStringData(
			TriangleMaterials, NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL, AttributeInfoMaterial))
		{
			bSuccess = false;
		}
	}

	// Add scalar material parameter attributes
	for (auto& Pair : ScalarMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;

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
			if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Pair.Value, NodeId, PartId, CurMaterialParamAttribName, AttributeInfoMaterialParameter))
			{
				bSuccess = false;
			}
		}
	}

	// Add vector material parameters
	for (auto& Pair : VectorMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;

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
			if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Pair.Value, NodeId, PartId, CurMaterialParamAttribName, AttributeInfoMaterialParameter))
			{
				bSuccess = false;
			}
		}
	}

	// Add texture material parameter attributes
	for (auto& Pair : TextureMaterialParameters)
	{
		FString CurMaterialParamAttribName = FString(HAPI_UNREAL_ATTRIB_MATERIAL) + "_parameter_" + Pair.Key;

		// Create attribute for material parameter.
		HAPI_AttributeInfo AttributeInfoMaterialParameter;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterialParameter);
		AttributeInfoMaterialParameter.tupleSize = 1;
		AttributeInfoMaterialParameter.count = Count;
		AttributeInfoMaterialParameter.exists = true;
		AttributeInfoMaterialParameter.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterialParameter.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterialParameter.originalOwner = HAPI_ATTROWNER_INVALID;

		// Create the new attribute
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*CurMaterialParamAttribName), &AttributeInfoMaterialParameter))
		{
			/*
			// Replace null strings by empty strings to prevent crashes when setting the attribute.
			char* EmptyString = nullptr;
			TArray<char*> StringData = Pair.Value;
			for (auto& CurValue : StringData)
			{
				if (CurValue != nullptr)
					continue;

				if (!EmptyString)
				{
					// Allocate the empty string the first time it is needed. This is free'd along with
					// the other strings in the arrays in TextureMaterialParameters by calls to DeleteFaceMaterialArray
					EmptyString = FHoudiniEngineUtils::ExtractRawString(FString(TEXT("")));
				}
				CurValue = EmptyString;
			}
			*/

			// The New attribute has been successfully created, set its value
			if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiSetAttributeStringData(				
				Pair.Value, NodeId, PartId, CurMaterialParamAttribName, AttributeInfoMaterialParameter))
			{
				bSuccess = false;
			}
		}
	}

	return bSuccess;
}

/*
bool
FUnrealMeshTranslator::AddLevelPathAttributeToMesh(
	const HAPI_NodeId& NodeId,
	const HAPI_PartId& PartId,
	const FString& LevelPath,
	const int32& Count)
{
	if (NodeId == -1 || LevelPath.IsEmpty() || Count <= 0)
		return false;

	// Get name of attribute used for level path
	std::string MarshallingAttributeLevelPath = HAPI_UNREAL_ATTRIB_LEVEL_PATH;

	// Marshall in level path.
	HAPI_AttributeInfo AttributeInfoLevelPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLevelPath);
	AttributeInfoLevelPath.count = Count;
	AttributeInfoLevelPath.tupleSize = 1;
	AttributeInfoLevelPath.exists = true;
	AttributeInfoLevelPath.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoLevelPath.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoLevelPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		MarshallingAttributeLevelPath.c_str(), &AttributeInfoLevelPath);

	if (HAPI_RESULT_SUCCESS == Result)
	{
		// Convert the FString to a cont char * array
		std::string LevelPathCStr = TCHAR_TO_ANSI(*LevelPath);
		const char* LevelPathCStrRaw = LevelPathCStr.c_str();
		TArray<const char*> PrimitiveAttrs;
		PrimitiveAttrs.AddUninitialized(Count);
		for (int32 Idx = 0; Idx < Count; ++Idx)
		{
			PrimitiveAttrs[Idx] = LevelPathCStrRaw;
		}

		// Set the attribute's string data
		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId,
			MarshallingAttributeLevelPath.c_str(), &AttributeInfoLevelPath,
			PrimitiveAttrs.GetData(), 0, AttributeInfoLevelPath.count);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_level_path attribute for mesh: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());
	}

	return true;
}
*/