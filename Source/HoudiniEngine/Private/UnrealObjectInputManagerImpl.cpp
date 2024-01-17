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
	FUnrealObjectInputHandle& OutHandle) const
{
	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputNode* const* const NodeEntry = InputNodes.Find(InIdentifier);
	if (!NodeEntry)
		return false;

	FUnrealObjectInputNode const* const Node = *NodeEntry;
	if (!Node)
		return false; 

	OutHandle = FUnrealObjectInputHandle(InIdentifier);
	return true;
}

bool
FUnrealObjectInputManagerImpl::GetNodeByIdentifier(const FUnrealObjectInputIdentifier& InputIdentifier, const FUnrealObjectInputNode*& OutNode) const
{
	if (!InputIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputNode* const* NodeEntry = InputNodes.Find(InputIdentifier);
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
	
	FUnrealObjectInputNode* const* NodeEntry = InputNodes.Find(InputIdentifier);
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
	InputNodes.Add(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);

	if (OnNodeAddedDelegate.IsBound())
		OnNodeAddedDelegate.Broadcast(InIdentifier);

	return true;
}

bool
FUnrealObjectInputManagerImpl::AddContainer(UPackage const* const InPackage, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (!IsValid(InPackage))
		return false;

	const FUnrealObjectInputIdentifier Identifier(InPackage);
	return AddContainer(Identifier, InNodeId, OutHandle);
}

bool
FUnrealObjectInputManagerImpl::AddContainer(UObject const* const InObject, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (!IsValid(InObject))
		return false;

	UPackage const* const Package = Cast<UPackage>(InObject);
	if (IsValid(Package))
		return AddContainer(Package, InNodeId, OutHandle);
	
	const FUnrealObjectInputIdentifier Identifier(InObject);
	return AddContainer(Identifier, InNodeId, OutHandle);
}

bool
FUnrealObjectInputManagerImpl::AddContainer(const FName& InPath, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	const FUnrealObjectInputIdentifier Identifier(InPath);
	return AddContainer(Identifier, InNodeId, OutHandle);
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

	if (InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;
	
	if (Contains(InIdentifier))
		return false;

	FUnrealObjectInputHandle ParentHandle;
	if (!EnsureParentsExist(InIdentifier, ParentHandle, true))
		return false;
	
	FUnrealObjectInputReferenceNode* Node = InReferencedNodes
		? new FUnrealObjectInputReferenceNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId, *InReferencedNodes, InReferencesConnectToNodeId)
		: new FUnrealObjectInputReferenceNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId, InReferencesConnectToNodeId);
	InputNodes.Add(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);

	if (OnNodeAddedDelegate.IsBound())
		OnNodeAddedDelegate.Broadcast(InIdentifier);

	return true;
}

bool
FUnrealObjectInputManagerImpl::AddReferenceNode(
	UObject const* const InObject,
	const FUnrealObjectInputOptions& InOptions,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const int32 InReferencesConnectToNodeId)
{
	constexpr bool bIsLeaf = false;
	const FUnrealObjectInputIdentifier Identifier(InObject, InOptions, bIsLeaf);
	return AddReferenceNode(Identifier, InObjectNodeId, InNodeId, OutHandle, InReferencedNodes, InReferencesConnectToNodeId);
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
	InputNodes.Add(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);

	if (OnNodeAddedDelegate.IsBound())
		OnNodeAddedDelegate.Broadcast(InIdentifier);
	
	return true;
}

bool
FUnrealObjectInputManagerImpl::AddLeaf(
	UObject const* const InObject,
	const FUnrealObjectInputOptions& InOptions,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle)
{
	constexpr bool bIsLeaf = true;
	const FUnrealObjectInputIdentifier Identifier(InObject, InOptions, bIsLeaf);
	return AddLeaf(Identifier, InObjectNodeId, InNodeId, OutHandle);
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
	if (!InIdentifier.IsValid() || InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
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
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputIdentifier ParentIdentifier;
	if (!InIdentifier.MakeParentIdentifier(ParentIdentifier))
	{
		// If InIdentifier is valid, but does not have a valid parent identifier, then it cannot have a parent
		OutParentHandle = FUnrealObjectInputHandle();
		return true;
	}
		
	FUnrealObjectInputHandle ParentHandle;
	const bool bParentEntryExists = FindNode(ParentIdentifier, ParentHandle);
	if (bParentEntryExists && AreHAPINodesValid(ParentIdentifier))
	{
		// Make sure we prevent node destruction if needed
		if (!bInputNodesCanBeDeleted)
			FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(ParentHandle, bInputNodesCanBeDeleted);

		OutParentHandle = ParentHandle;
		return true;
	}
	
	FUnrealObjectInputHandle GrandParentHandle;
	if (!EnsureParentsExist(ParentIdentifier, GrandParentHandle, bInputNodesCanBeDeleted))
		return false;
	
	int32 GrandParentNodeId = -1;
	if (GrandParentHandle.IsValid())
	{
		const FUnrealObjectInputNode* Node = nullptr;
		if (GetNode(GrandParentHandle, Node))
			GrandParentNodeId = Node->GetHAPINodeId();
	}

	// Create an obj subnet
	const FString NodeLabel = GetDefaultNodeName(ParentIdentifier);
	static constexpr bool bCookOnCreation = true;
	const bool bIsTopLevelNode = GrandParentNodeId < 0;
	const FString OperatorName = bIsTopLevelNode ? TEXT("Object/subnet") : TEXT("subnet");
	int32 ParentNodeId = -1;
	const HAPI_Result ResultVariable = FHoudiniEngineUtils::CreateNode(
		GrandParentNodeId, OperatorName, NodeLabel, bCookOnCreation, &ParentNodeId);
	if (ResultVariable != HAPI_RESULT_SUCCESS)
	{
		const FString ErrorMessage = FHoudiniEngineUtils::GetErrorDescription();
		HOUDINI_LOG_WARNING(TEXT( "Failed to create node via HAPI: %s" ), *ErrorMessage);
		return false;
	}

	// In session sync, if the subnet's display flag is enabled, it can cause slow deletion / creation of nodes inside
	// nested subnets due to display flag propagation checks. So we attempt to disable the display flag of top level
	// subnets here.
	if (bIsTopLevelNode)
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), ParentNodeId, 0))
		{
			const FString ErrorMessage = FHoudiniEngineUtils::GetErrorDescription();
			HOUDINI_LOG_WARNING(TEXT("Failed to disable the display flag of an input subnet via HAPI: %s"), *ErrorMessage);
		}
	}

	if (!bParentEntryExists || !ParentHandle.IsValid())
		AddContainer(ParentIdentifier, ParentNodeId, ParentHandle);
	else
		UpdateContainer(ParentIdentifier, ParentNodeId);

	// Make sure we prevent node destruction if needed
	if (!bInputNodesCanBeDeleted)
		FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(ParentHandle, bInputNodesCanBeDeleted);
	else
		FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(ParentHandle, bInputNodesCanBeDeleted);
	
	OutParentHandle = ParentHandle;

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

	if (bInAlsoDirtyReferencedNodes && InIdentifier.GetNodeType() == EUnrealObjectInputNodeType::Reference)
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
	// Destroy the Node structs
	for (auto& Entry : InputNodes)
	{
		FUnrealObjectInputNode* const Node = Entry.Value;
		if (!Node)
			continue;

		delete Node;
		Entry.Value = nullptr;
	}
	InputNodes.Empty();
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

	const AActor* Actor = Cast<const AActor>(InIdentifier.GetObject());

	const EUnrealObjectInputNodeType Type = InIdentifier.GetNodeType();
	if (Type == EUnrealObjectInputNodeType::Leaf || Type == EUnrealObjectInputNodeType::Reference)
	{
		TArray<FString> NameParts;
		NameParts.Reserve(4);

		// Get the object name, or for actors, get their label
		/*FString ObjectName = IsValid(Actor) ? Actor->GetActorNameOrLabel() : InIdentifier.GetObject()->GetName();
		FHoudiniEngineUtils::SanitizeHAPIVariableName(ObjectName);*/

		FString ObjectName = InIdentifier.GetObject()->GetName();
		FHoudiniEngineUtils::SanitizeHAPIVariableName(ObjectName);

		NameParts.Add(ObjectName);

		const FUnrealObjectInputOptions& Options = InIdentifier.GetOptions();
		if (Type == EUnrealObjectInputNodeType::Reference)
			NameParts.Add(TEXT("merge"));

		// Add part generated from Options 
		const FString OptionsSuffix = Options.GenerateNodeNameSuffix();
		if (!OptionsSuffix.IsEmpty())
			NameParts.Add(OptionsSuffix);

		// Add empty part so that name ends with _ so that numeric suffixes on name clashes are easier to see
		NameParts.Add(TEXT(""));
		return FString::Join(NameParts, TEXT("_"));
	}
	
	// Get the object name, or for actors, get their label
	FString ObjectName;
	if (IsValid(Actor))
		ObjectName = Actor->GetActorNameOrLabel();
	else
		ObjectName = FPaths::GetBaseFilename(InIdentifier.GetNormalizedObjectPath().ToString());

	FHoudiniEngineUtils::SanitizeHAPIVariableName(ObjectName);
	return ObjectName;
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
FUnrealObjectInputManagerImpl::AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle) const
{
	if (!InHandle.IsValid())
		return false;
	return AreHAPINodesValid(InHandle.GetIdentifier());
}

bool
FUnrealObjectInputManagerImpl::AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (!InIdentifier.IsValid())
		return false;
	FUnrealObjectInputNode const* Node = nullptr;
	if (!GetNode(InIdentifier, Node) || !Node)
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
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(Session, NodeId, &NodeInfo))
		return false;

	if (InNodeId.GetUniqueHoudiniNodeId() != NodeInfo.uniqueHoudiniNodeId)
		return false;

	bool ValidationAnswer = false;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsNodeValid(Session, NodeId, NodeInfo.uniqueHoudiniNodeId, &ValidationAnswer))
		return false;
	
	return ValidationAnswer;
}

bool
FUnrealObjectInputManagerImpl::DeleteHAPINode(const FUnrealObjectInputHAPINodeId& InNodeId) const
{
	if (!InNodeId.IsValid())
		return false;

	return FHoudiniEngineUtils::DeleteHoudiniNode(InNodeId.GetHAPINodeId());
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

	return Node->AddRef();
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

		InputNodes.Remove(InIdentifier);
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
