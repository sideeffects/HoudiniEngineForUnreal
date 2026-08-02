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

#include "UnrealObjectInputManagerImpl.h"

#include "HAPI/HAPI_Common.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"

FUnrealObjectInputManagerImpl::FUnrealObjectInputManagerImpl()
	: WorldOriginNodeId()
{
}

FUnrealObjectInputManagerImpl::~FUnrealObjectInputManagerImpl()
{
	Clear();
}

bool
FUnrealObjectInputManagerImpl::FindNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	FUnrealObjectInputHandle& OutHandle)
{
	OutHandle = FUnrealObjectInputHandle();

	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputNode* const* const NodeEntry = FindNode(InIdentifier);
	if (!NodeEntry)
		return false;

	FUnrealObjectInputNode const* const Node = *NodeEntry;
	if (!Node)
		return false; 

	if (!Node->AreHAPINodesValid())
	{
		FUnrealObjectInputNode* const MutableNode = *NodeEntry;
		RemoveNode(InIdentifier);
		delete MutableNode;
		return false;
	}

	OutHandle = FUnrealObjectInputHandle(InIdentifier);
	return true;
}

bool
FUnrealObjectInputManagerImpl::GetNodeByIdentifier(const FUnrealObjectInputIdentifier& InputIdentifier, const FUnrealObjectInputNode*& OutNode) const
{
	if (!InputIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputNode* const* NodeEntry = FindNode(InputIdentifier);
	if (!NodeEntry)
		return false;

	OutNode = *NodeEntry;
	return true;
}

bool
FUnrealObjectInputManagerImpl::GetNodeByIdentifier(const FUnrealObjectInputIdentifier& InputIdentifier, FUnrealObjectInputNode*& OutNode) const
{
	if (!InputIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputNode* const* NodeEntry = FindNode(InputIdentifier);
	if (!NodeEntry)
		return false;

	OutNode = *NodeEntry;
	return true;
}

bool
FUnrealObjectInputManagerImpl::AddContainer(const FUnrealObjectInputIdentifier& InIdentifier, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (!InIdentifier.IsValid())
		return false;

	if (InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Container)
		return false;
	
	if (Contains(InIdentifier))
		return false;

	FUnrealObjectInputHandle ParentHandle;
	if (!EnsureParentsExist(InIdentifier, ParentHandle, true))
		return false;
	
	FUnrealObjectInputContainerNode* const Node = new FUnrealObjectInputContainerNode(InIdentifier, ParentHandle, InNodeId);
	AddNode(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);

	if (OnNodeAddedDelegate.IsBound())
		OnNodeAddedDelegate.Broadcast(InIdentifier);

	return true;
}

bool
FUnrealObjectInputManagerImpl::AddReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const int32 InReferencesConnectToNodeId)
{
	if (!InIdentifier.IsValid())
		return false;

	if (InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::LeafWithReferences)
	{
		HOUDINI_LOG_ERROR(TEXT("Mismatch Node Types."));
		return false;
	}
	
	if (Contains(InIdentifier))
		return false;

	FUnrealObjectInputHandle ParentHandle;
	if (!EnsureParentsExist(InIdentifier, ParentHandle, true))
		return false;
	
	FUnrealObjectInputReferenceNode* Node = InReferencedNodes
		? new FUnrealObjectInputReferenceNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId, *InReferencedNodes, InReferencesConnectToNodeId)
		: new FUnrealObjectInputReferenceNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId, InReferencesConnectToNodeId);

	AddNode(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);

	if (OnNodeAddedDelegate.IsBound())
		OnNodeAddedDelegate.Broadcast(InIdentifier);

	return true;
}

bool
FUnrealObjectInputManagerImpl::AddLeaf(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle)
{
	if (!InIdentifier.IsValid())
		return false;

	if (InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Leaf)
		return false;

	if (Contains(InIdentifier))
		return false;

	FUnrealObjectInputHandle ParentHandle;
	if (!EnsureParentsExist(InIdentifier, ParentHandle, true))
		return false;
	
	FUnrealObjectInputLeafNode* Node = new FUnrealObjectInputLeafNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId);
	AddNode(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);

	if (OnNodeAddedDelegate.IsBound())
		OnNodeAddedDelegate.Broadcast(InIdentifier);
	
	return true;
}

bool
FUnrealObjectInputManagerImpl::UpdateContainer(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InNodeId,
	const bool bInClearDirtyFlag)
{
	if (!InIdentifier.IsValid() || InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Container)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node) || !Node)
		return false;
	FUnrealObjectInputContainerNode* ContainerNode = static_cast<FUnrealObjectInputContainerNode*>(Node);
	if (!ContainerNode)
		return false;

	// As part of updating the node, ensure that its parents exist and make sure it is pointing to its parent.
	FUnrealObjectInputHandle ParentHandle;
	if (EnsureParentsExist(InIdentifier, ParentHandle, true))
		ContainerNode->SetParent(ParentHandle);

	ContainerNode->SetHAPINodeId(InNodeId);

	// Clear the dirty flag if set (based on bInClearDirtyFlag)
	if (bInClearDirtyFlag)
		ContainerNode->ClearDirtyFlag();

	if (OnNodeUpdatedDelegate.IsBound())
		OnNodeUpdatedDelegate.Broadcast(InIdentifier);

	return true;
}

bool
FUnrealObjectInputManagerImpl::UpdateReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const TOptional<int32> InObjectNodeId,
	const TOptional<int32> InNodeId,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const TOptional<int32> InReferencesConnectToNodeId,
	const bool bInClearDirtyFlag)
{
	if (!InIdentifier.IsValid() || InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::LeafWithReferences)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node) || !Node)
		return false;
	FUnrealObjectInputReferenceNode* ReferenceNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!ReferenceNode)
		return false;

	// As part of updating the node, ensure that its parents exist and make sure it is pointing to its parent.
	FUnrealObjectInputHandle ParentHandle;
	if (EnsureParentsExist(InIdentifier, ParentHandle, true))
		ReferenceNode->SetParent(ParentHandle);

	bool bSuccess = true;
	if (InObjectNodeId.IsSet() && InObjectNodeId.GetValue() >= 0)
	{
		if (!ReferenceNode->SetObjectHAPINodeId(InObjectNodeId.GetValue()))
			bSuccess = false;
	}
	if (InNodeId.IsSet() && InNodeId.GetValue() >= 0)
	{
		if (!ReferenceNode->SetHAPINodeId(InNodeId.GetValue()))
			bSuccess = false;
	}
	if (InReferencesConnectToNodeId.IsSet())
	{
		const int32 ReferencesConnectToNodeId = InReferencesConnectToNodeId.GetValue(); 
		if (ReferencesConnectToNodeId < 0)
			ReferenceNode->ResetReferencesConnectToNodeId();
		else
			ReferenceNode->SetReferencesConnectToNodeId(ReferencesConnectToNodeId);
	}
	if (InReferencedNodes)
		ReferenceNode->SetReferencedNodes(*InReferencedNodes);

	// Clear the dirty flag if set (based on bInClearDirtyFlag)
	if (bInClearDirtyFlag)
		ReferenceNode->ClearDirtyFlag();

	if (OnNodeUpdatedDelegate.IsBound())
		OnNodeUpdatedDelegate.Broadcast(InIdentifier);

	return bSuccess;
}

bool
FUnrealObjectInputManagerImpl::UpdateLeaf(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	const bool bInClearDirtyFlag)
{
	if (!InIdentifier.IsValid() || InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Leaf)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node) || !Node)
		return false;
	FUnrealObjectInputLeafNode* LeafNode = static_cast<FUnrealObjectInputLeafNode*>(Node);
	if (!LeafNode)
		return false;

	// As part of updating the node, ensure that its parents exist and make sure it is pointing to its parent.
	FUnrealObjectInputHandle ParentHandle;
	if (EnsureParentsExist(InIdentifier, ParentHandle, true))
		LeafNode->SetParent(ParentHandle);

	bool bSuccess = true;
	if (!LeafNode->SetObjectHAPINodeId(InObjectNodeId))
		bSuccess = false;
	if (!LeafNode->SetHAPINodeId(InNodeId))
		bSuccess = false;

	// Clear the dirty flag if set (based on bInClearDirtyFlag)
	if (bInClearDirtyFlag)
		LeafNode->ClearDirtyFlag();
	
	if (OnNodeUpdatedDelegate.IsBound())
		OnNodeUpdatedDelegate.Broadcast(InIdentifier);

	return bSuccess;
}

bool
FUnrealObjectInputManagerImpl::EnsureParentsExist(
	const FUnrealObjectInputIdentifier& InIdentifier,
	FUnrealObjectInputHandle& OutParentHandle,
	const bool& bInputNodesCanBeDeleted)
{
	if(!InIdentifier.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid FUnrealObjectInputIdentifier"));
		return false;
	}
	
	FUnrealObjectInputIdentifier ParentIdentifier;
	if (!InIdentifier.MakeParentIdentifier(ParentIdentifier))
	{
		OutParentHandle = FUnrealObjectInputHandle();
		return true;
	}

	bool bParentCreated = false;
	return EnsureContainerExists(ParentIdentifier, OutParentHandle, bParentCreated, bInputNodesCanBeDeleted);
}

bool
FUnrealObjectInputManagerImpl::EnsureContainerExists(
	const FUnrealObjectInputIdentifier& InIdentifier,
	FUnrealObjectInputHandle& OutContainerHandle,
	bool& bCreated,
	const bool& bInputNodesCanBeDeleted)
{
	OutContainerHandle = FUnrealObjectInputHandle();
	bCreated = false;

	if (!InIdentifier.IsValid())
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid FUnrealObjectInputIdentifier"));
		return false;
	}

	if (InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Container)
	{
		HOUDINI_LOG_ERROR(TEXT("EnsureContainerExists requires a Container identifier"));
		return false;
	}

	FUnrealObjectInputHandle ContainerHandle;
	const bool bContainerEntryExists = FindNode(InIdentifier, ContainerHandle);
	if (bContainerEntryExists && AreHAPINodesValid(InIdentifier))
	{
		if (!bInputNodesCanBeDeleted)
			FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(ContainerHandle, bInputNodesCanBeDeleted);

		OutContainerHandle = std::move(ContainerHandle);
		return true;
	}

	FUnrealObjectInputHandle ParentHandle;
	if (!EnsureParentsExist(InIdentifier, ParentHandle, bInputNodesCanBeDeleted))
		return false;

	int32 ParentNodeId = -1;
	if (ParentHandle.IsValid())
	{
		FUnrealObjectInputNode* Node = nullptr;
		if (GetNode(ParentHandle, Node))
			ParentNodeId = Node->GetHAPINodeId();
	}

	FUnrealObjectInputIdentifier ParentIdentifier;
	const bool bHasParent = InIdentifier.MakeParentIdentifier(ParentIdentifier) && ParentIdentifier.IsValid();

	const FString NodeLabel = GetDefaultNodeName(InIdentifier);
	static constexpr bool bCookOnCreation = true;
	const bool bIsTopLevelNode = !bHasParent;
	const FString OperatorName = bIsTopLevelNode ? TEXT("Object/subnet") : TEXT("subnet");
	int32 ContainerNodeId = -1;
	const HAPI_Result ResultVariable = FHoudiniEngineUtils::CreateNode(
		ParentNodeId, OperatorName, NodeLabel, bCookOnCreation, &ContainerNodeId);
	if (ResultVariable != HAPI_RESULT_SUCCESS)
	{
		const FString ErrorMessage = FHoudiniEngineUtils::GetErrorDescription();
		HOUDINI_LOG_WARNING(TEXT("Failed to create node via HAPI: %s"), *ErrorMessage);
		return false;
	}

	if (bIsTopLevelNode)
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), ContainerNodeId, 0))
		{
			const FString ErrorMessage = FHoudiniEngineUtils::GetErrorDescription();
			HOUDINI_LOG_WARNING(TEXT("Failed to disable the display flag of an input subnet via HAPI: %s"), *ErrorMessage);
		}
	}

	bool bContainerUpdated = false;
	if (!bContainerEntryExists || !ContainerHandle.IsValid())
	{
		bContainerUpdated = AddContainer(InIdentifier, ContainerNodeId, ContainerHandle);
	}
	else
	{
		bContainerUpdated = UpdateContainer(InIdentifier, ContainerNodeId);
	}

	if (!bContainerUpdated)
		return false;

	if (!ContainerHandle.IsValid())
		ContainerHandle = FUnrealObjectInputHandle(InIdentifier);

	FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(ContainerHandle, bInputNodesCanBeDeleted);

	bCreated = true;
	OutContainerHandle = std::move(ContainerHandle);
	return true;
}

bool
FUnrealObjectInputManagerImpl::IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	FUnrealObjectInputNode const* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;
	
	return Node->IsDirty();
}

bool
FUnrealObjectInputManagerImpl::MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier, const bool bInAlsoDirtyReferencedNodes)
{
	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;

	if (bInAlsoDirtyReferencedNodes && InIdentifier.GetNodeType() == EUnrealObjectInputNodeType::LeafWithReferences)
	{
		FUnrealObjectInputReferenceNode* const RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
		if (!RefNode)
			return false;
		RefNode->MarkAsDirty(bInAlsoDirtyReferencedNodes);
	}
	else
	{
		Node->MarkAsDirty();
	}

	return true;
}

bool
FUnrealObjectInputManagerImpl::ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier)
{
	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;
	
	Node->ClearDirtyFlag();

	return true;
}

bool
FUnrealObjectInputManagerImpl::GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const
{
	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;

	Node->GetHAPINodeIds(OutNodeIds);

	return true;
}

bool
FUnrealObjectInputManagerImpl::GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const
{
	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;

	Node->GetHAPINodeIds(OutNodeIds);

	return true;
}

bool
FUnrealObjectInputManagerImpl::GetAllHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const
{
	const int32 NumEntries = InputNodes.Num();
	// Estimate 2 node ids per entry
	OutNodeIds.Reserve(2 * NumEntries);
	TArray<FUnrealObjectInputHAPINodeId> NodeIds;
	for (const auto& Entry : InputNodes)
	{
		FUnrealObjectInputNode const* const Node = Entry.Value;
		if (!Node)
			continue;
		if (!NodeIds.IsEmpty())
			NodeIds.Reset();
		Node->GetHAPINodeIds(NodeIds);
		OutNodeIds.Append(NodeIds);
	}

	return true;
}

bool
FUnrealObjectInputManagerImpl::GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const
{
	const int32 NumEntries = InputNodes.Num();
	// Estimate 2 node ids per entry
	OutNodeIds.Reserve(2 * NumEntries);
	TArray<int32> NodeIds;
	for (const auto& Entry : InputNodes)
	{
		FUnrealObjectInputNode const* const Node = Entry.Value;
		if (!Node)
			continue;
		if (!NodeIds.IsEmpty())
			NodeIds.Reset();
		Node->GetHAPINodeIds(NodeIds);
		OutNodeIds.Append(NodeIds);
	}

	return true;
}

bool
FUnrealObjectInputManagerImpl::Clear()
{
	TArray<FUnrealObjectInputNode*> NodesToDelete;
	NodesToDelete.Reserve(InputNodes.Num());

	for (const auto& Entry : InputNodes)
	{
		if (Entry.Value)
			NodesToDelete.Add(Entry.Value);
	}

	// Clear manager-owned state before deleting nodes: node destructors release handles that can call back into the
	// manager and mutate InputNodes / BackLinks.
	InputNodes.Empty();
	BackLinks.Empty();
	WorldOriginNodeId.Reset();

	for (FUnrealObjectInputNode* Node : NodesToDelete)
	{
		delete Node;
	}

	return true;
}

FUnrealObjectInputHAPINodeId
FUnrealObjectInputManagerImpl::GetWorldOriginNodeId(const bool bInCreateIfMissingOrInvalid)
{
	static const FUnrealObjectInputHAPINodeId InvalidNode;
	
	if (WorldOriginNodeId.IsValid())
		return WorldOriginNodeId;

	if (!bInCreateIfMissingOrInvalid)
		return InvalidNode;

	// Create a OBJ/null with default/identity transform
	constexpr HAPI_NodeId ParentNodeId = -1;
	constexpr bool bCookOnCreation = true;
	HAPI_NodeId NodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("Object/null"), TEXT("WorldOrigin"), bCookOnCreation, &NodeId), InvalidNode);

	WorldOriginNodeId.Set(NodeId);
	return WorldOriginNodeId;
}

int32
FUnrealObjectInputManagerImpl::GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid)
{
	const FUnrealObjectInputHAPINodeId NewWorldOriginNodeId = GetWorldOriginNodeId(bInCreateIfMissingOrInvalid);
	return NewWorldOriginNodeId.GetHAPINodeId();
}

FString
FUnrealObjectInputManagerImpl::GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (!InIdentifier.IsValid())
		return FString();
	return InIdentifier.GetNodeName();
}

bool
FUnrealObjectInputManagerImpl::GetUniqueHoudiniNodeId(const int32 InHAPINodeId, int32& OutUniqueHoudiniNodeId) const
{
	if (InHAPINodeId < 0)
		return false;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	if (!Session)
		return false;
	
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(Session, InHAPINodeId, &NodeInfo), false);

	OutUniqueHoudiniNodeId = NodeInfo.uniqueHoudiniNodeId;
	return true;
}

bool
FUnrealObjectInputManagerImpl::AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (!InIdentifier.IsValid())
		return false;
	FUnrealObjectInputNode const* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node) || !Node)
		return false;
	return Node->AreHAPINodesValid();
}

bool
FUnrealObjectInputManagerImpl::IsHAPINodeValid(const FUnrealObjectInputHAPINodeId& InNodeId) const
{
	if (!InNodeId.IsSet())
		return false;

	const HAPI_NodeId NodeId = InNodeId.GetHAPINodeId();
	
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();

	auto Success = FHoudiniApi::GetNodeInfo(Session, NodeId, &NodeInfo);
	if(Success != HAPI_RESULT_SUCCESS)
		return false;

	if (InNodeId.GetUniqueHoudiniNodeId() != NodeInfo.uniqueHoudiniNodeId)
		return false;

	bool ValidationAnswer = false;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsNodeValid(Session, NodeId, NodeInfo.uniqueHoudiniNodeId, &ValidationAnswer))
		return false;
	
	return ValidationAnswer;
}

bool
FUnrealObjectInputManagerImpl::DeleteHAPINode(
	FUnrealObjectInputHAPINodeId& InNodeId) const
{
	if (!InNodeId.IsValid())
		return false;

	bool bResult = FHoudiniEngineUtils::DeleteHoudiniNode(InNodeId.GetHAPINodeId());
	InNodeId.Set(INDEX_NONE, INDEX_NONE);
	return bResult;
}

bool
FUnrealObjectInputManagerImpl::SetHAPINodeDisplay(const FUnrealObjectInputHAPINodeId& InNodeId, const bool bInOnOff) const
{
	if (!InNodeId.IsValid())
		return false;

	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), InNodeId.GetHAPINodeId(), bInOnOff), false);
	return true;
}

bool
FUnrealObjectInputManagerImpl::SetHAPINodeDisplay(const int32 InNodeId, const bool bInOnOff) const
{
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), InNodeId, bInOnOff), false);
	return true;
}

bool
FUnrealObjectInputManagerImpl::AddRef(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputNode** NodeEntry = InputNodes.Find(InIdentifier);
	if (!NodeEntry)
		return false;

	FUnrealObjectInputNode* Node = *NodeEntry;
	if (!Node)
		return false;

	bool bSuccess =  Node->AddRef();

	//HOUDINI_LOG_MESSAGE(TEXT("added ref %s %d"), *InIdentifier.ToString(), Node->GetRefCount());

	return bSuccess;

}

bool
FUnrealObjectInputManagerImpl::RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputNode** NodeEntry = InputNodes.Find(InIdentifier);
	if (!NodeEntry)
		return false;
	
	FUnrealObjectInputNode* Node = *NodeEntry;
	if (!Node)
		return false;

	if (!Node->RemoveRef())
		return false;


	//HOUDINI_LOG_MESSAGE(TEXT("removed ref %s %d"), *InIdentifier.ToString(), Node->GetRefCount());

	if (Node->IsRefCounted() && Node->GetRefCount() == 0 && Node->CanBeDeleted())
	{
		// Destroy HAPI nodes
		if (Node->AreHAPINodesValid())
		{
			// if (FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), Node->GetNodeId()) != HAPI_RESULT_SUCCESS)
			if (!Node->DeleteHAPINodes())
			{
				// Log if we could not delete the node
				HOUDINI_LOG_WARNING(TEXT("Could not delete HAPI node: %d"), Node->GetHAPINodeId());
			}
		}

		RemoveNode(InIdentifier);
		delete Node;
		Node = nullptr;

		if (OnNodeDeletedDelegate.IsBound())
			OnNodeDeletedDelegate.Broadcast(InIdentifier);
	}

	return true;
}

bool
FUnrealObjectInputManagerImpl::AddBackLink(const FUnrealObjectInputIdentifier& InReferencedIdentifier, const FUnrealObjectInputIdentifier& InReferencedBy)
{
	if (!InReferencedIdentifier.IsValid() || !InReferencedBy.IsValid())
		return false;

	FUnrealObjectInputBackLinkReferences& ReferencesData = BackLinks.FindOrAdd(InReferencedIdentifier);
	int32& NumReferences = ReferencesData.NumReferencesBy.FindOrAdd(InReferencedBy);
	NumReferences++;

	return true;
}

bool
FUnrealObjectInputManagerImpl::RemoveBackLink(const FUnrealObjectInputIdentifier& InReferencedIdentifier, const FUnrealObjectInputIdentifier& InReferencedBy)
{
	if (!InReferencedIdentifier.IsValid())
		return false;

	FUnrealObjectInputBackLinkReferences* const ReferencesData = BackLinks.Find(InReferencedIdentifier);
	if (!ReferencesData)
		return false;

	int32* NumReferences = ReferencesData->NumReferencesBy.Find(InReferencedBy);
	if (!NumReferences)
		return false;
	
	(*NumReferences) -= 1;
	if (*NumReferences <= 0)
	{
		NumReferences = nullptr;
		ReferencesData->NumReferencesBy.Remove(InReferencedBy);
	}

	return true;
}

bool
FUnrealObjectInputManagerImpl::GetReferencedBy(const FUnrealObjectInputIdentifier& InReferencedIdentifier, TSet<FUnrealObjectInputIdentifier>& OutReferencedBy) const
{
	if (!InReferencedIdentifier.IsValid())
		return false;

	FUnrealObjectInputBackLinkReferences const* const ReferencesData = BackLinks.Find(InReferencedIdentifier);
	if (!ReferencesData)
		return false;

	if (ReferencesData->NumReferencesBy.Num() <= 0)
	{
		OutReferencedBy.Reset();
		return true;
	}

	OutReferencedBy.Empty(ReferencesData->NumReferencesBy.Num());
	for (const auto& Pair : ReferencesData->NumReferencesBy)
	{
		OutReferencedBy.Add(Pair.Key);
	}

	return true;
}


void FUnrealObjectInputManagerImpl::Dump() 
{
	for (auto It : InputNodes)
	{
		FString IdentifierString = It.Key.ToString();
		int RefCount = It.Value->GetRefCount();

		FString Str =  FString::Printf(TEXT("%s ref count %d"), *IdentifierString, RefCount);

		FUnrealObjectInputNode* Node = nullptr;
		this->GetNodeByIdentifier(It.Key, Node);
		if(Node)
		{
			HOUDINI_LOG_MESSAGE(TEXT("   parent %s"), *Node->GetParent().GetIdentifier().ToString());
		}

		HOUDINI_LOG_MESSAGE(TEXT("%s"), *Str);
		TSet<FUnrealObjectInputIdentifier> ReferencedBy;

		FUnrealObjectInputManagerImpl::GetReferencedBy(It.Key, ReferencedBy);

		for (auto Ref : ReferencedBy)
		{
			Str = FString::Printf(TEXT("%s ref"), *IdentifierString);
			HOUDINI_LOG_MESSAGE(TEXT("%s"), *Str);
		}
	}
}


FUnrealObjectInputNode* const * FUnrealObjectInputManagerImpl::FindNode(const FUnrealObjectInputIdentifier& Identifier) const
{
	return InputNodes.Find(Identifier);
}

void FUnrealObjectInputManagerImpl::AddNode(const FUnrealObjectInputIdentifier& Identifier, FUnrealObjectInputNode* Node)
{
	InputNodes.Add(Identifier, Node);
}
void FUnrealObjectInputManagerImpl::RemoveNode(const FUnrealObjectInputIdentifier& Identifier)
{
	InputNodes.Remove(Identifier);
}

bool FUnrealObjectInputManagerImpl::Contains(const FUnrealObjectInputHandle& InHandle) const 
{
	return InputNodes.Contains(InHandle.GetIdentifier());
}

bool FUnrealObjectInputManagerImpl::Contains(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	return InputNodes.Contains(InIdentifier);
}
