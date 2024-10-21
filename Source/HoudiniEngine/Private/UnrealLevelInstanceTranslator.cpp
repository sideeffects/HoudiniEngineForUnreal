/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "UnrealLevelInstanceTranslator.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"

bool FUnrealLevelInstanceTranslator::AddLevelInstance(
	ALevelInstance* LevelInstance,
	UHoudiniInput* HoudiniInput,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	const bool bInputNodesCanBeDeleted)
{
	CreateNodeForLevelInstance(
			LevelInstance,
		HoudiniInput,
			InputNodeId,
			InputNodeName,
			OutHandle,
			bInputNodesCanBeDeleted);

	CreateAttributeData(InputNodeId, LevelInstance);

	return true;
}

bool
FUnrealLevelInstanceTranslator::CreateNodeForLevelInstance(
	ALevelInstance* LevelInstance,
	UHoudiniInput* HoudiniInput,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	const bool bInputNodesCanBeDeleted)
{
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputIdentifier GeoNodeIdentifier;
	FUnrealObjectInputHandle ParentHandle;

	FString FinalInputNodeName = InputNodeName;
	HAPI_NodeId ParentNodeId = -1;

	{
		FUnrealObjectInputOptions Options;
		Options.bExportLevelInstanceContent = false;
		Identifier = FUnrealObjectInputIdentifier(LevelInstance, Options, true);

		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			{
				if (!bInputNodesCanBeDeleted)
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted))
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

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

	HAPI_NodeId InputObjectNodeId = -1;

	if (FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
	{
		InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);
	}
	else
	{
		HAPI_NodeId NewNodeId = -1;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(FinalInputNodeName, NewNodeId, ParentNodeId), false);

		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
			return false;

		InputNodeId = NewNodeId;
		InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);
	}

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 1;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = 1;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &Part), false);

	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return true;

}

bool FUnrealLevelInstanceTranslator::AddStringPointAttribute(HAPI_NodeId NodeId, const char* Name, TArray<FString>& Data)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	AttributeInfo.count = 1;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), NodeId, 0, Name, &AttributeInfo), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, Name);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Data), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), false);

	return true;
}


bool FUnrealLevelInstanceTranslator::AddFloatPointAttribute(HAPI_NodeId NodeId, const char * Name, const float * Data, int TupleSize, int Count)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	AttributeInfo.count = 1;
	AttributeInfo.tupleSize = 3;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(),NodeId,0,Name, &AttributeInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, 0, Name, &AttributeInfo, Data, 0, Count), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NodeId), false);

	return true;
}

void FUnrealLevelInstanceTranslator::CreateAttributeData(HAPI_NodeId NodeId, ALevelInstance* LevelInstance)
{
	constexpr int NumInstances = 1;
	TArray<float> Positions;
	TArray<FString> ActorPaths;
	TArray<FString> ActorLevels;

	Positions.Reserve(NumInstances * 3);
	ActorPaths.Reserve(NumInstances);
	ActorLevels.Reserve(NumInstances);

	ActorPaths.Add(LevelInstance->GetPathName());

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
	FString Level = LevelInstance->GetWorldAsset().ToSoftObjectPath().GetAssetPath().ToString();
#else
	FString Level = LevelInstance->GetWorldAsset().ToSoftObjectPath().GetAssetPathName().ToString();
#endif
	ActorLevels.Add(Level);

	const FTransform& Transform = LevelInstance->GetActorTransform();

	FVector PositionVector = Transform.GetLocation();
	Positions.Add(0.0f);
	Positions.Add(0.0f);
	Positions.Add(0.0f);

	AddFloatPointAttribute(NodeId, HAPI_UNREAL_ATTRIB_POSITION, Positions.GetData(), 3, 1);
	AddStringPointAttribute(NodeId, HAPI_UNREAL_ATTRIB_ACTOR_PATH, ActorPaths);
	AddStringPointAttribute(NodeId, HAPI_UNREAL_ATTRIB_LEVEL_INSTANCE_NAME, ActorLevels);

	FHoudiniEngineUtils::HapiCommitGeo(NodeId);
}


