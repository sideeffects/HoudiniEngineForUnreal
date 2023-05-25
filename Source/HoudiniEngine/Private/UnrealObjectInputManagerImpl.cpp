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

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"

FUnrealObjectInputManagerImpl::FUnrealObjectInputManagerImpl()
	: WorldOriginNodeId(-1)
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
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes)
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
		? new FUnrealObjectInputReferenceNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId, *InReferencedNodes)
		: new FUnrealObjectInputReferenceNode(InIdentifier, ParentHandle, InObjectNodeId, InNodeId);
	InputNodes.Add(InIdentifier, Node);

	OutHandle = FUnrealObjectInputHandle(InIdentifier);
	return true;
}

bool
FUnrealObjectInputManagerImpl::AddReferenceNode(
	UObject const* const InObject,
	const FUnrealObjectInputOptions& InOptions,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes)
{
	constexpr bool bIsLeaf = false;
	const FUnrealObjectInputIdentifier Identifier(InObject, InOptions, bIsLeaf);
	return AddReferenceNode(Identifier, InObjectNodeId, InNodeId, OutHandle, InReferencedNodes);
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

	ContainerNode->SetNodeId(InNodeId);

	// Clear the dirty flag if set (based on bInClearDirtyFlag)
	if (bInClearDirtyFlag)
		ContainerNode->ClearDirtyFlag();

	return true;
}

bool
FUnrealObjectInputManagerImpl::UpdateReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	int32 const* const InObjectNodeId,
	int32 const* const InNodeId,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
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

	if (InObjectNodeId)
		ReferenceNode->SetObjectNodeId(*InObjectNodeId);
	if (InNodeId)
		ReferenceNode->SetNodeId(*InNodeId);
	if (InReferencedNodes)
		ReferenceNode->SetReferencedNodes(*InReferencedNodes);

	// Clear the dirty flag if set (based on bInClearDirtyFlag)
	if (bInClearDirtyFlag)
		ReferenceNode->ClearDirtyFlag();

	return true;
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

	LeafNode->SetObjectNodeId(InObjectNodeId);
	LeafNode->SetNodeId(InNodeId);

	// Clear the dirty flag if set (based on bInClearDirtyFlag)
	if (bInClearDirtyFlag)
		LeafNode->ClearDirtyFlag();
	
	return true;
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
			FHoudiniEngineUtils::UpdateInputNodeCanBeDeleted(ParentHandle, bInputNodesCanBeDeleted);

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
			GrandParentNodeId = Node->GetNodeId();
	}

	// Create an obj subnet
	const FString NodeLabel = GetDefaultNodeName(ParentIdentifier);
	constexpr bool bCookOnCreation = true;
	const FString OperatorName = GrandParentNodeId < 0 ? TEXT("Object/subnet") : TEXT("subnet");
	int32 ParentNodeId = -1;
    const HAPI_Result ResultVariable = FHoudiniEngineUtils::CreateNode(
    	GrandParentNodeId, OperatorName, NodeLabel, bCookOnCreation, &ParentNodeId);
    if (ResultVariable != HAPI_RESULT_SUCCESS)
    {
    	const FString ErrorMessage = FHoudiniEngineUtils::GetErrorDescription();
        HOUDINI_LOG_WARNING(TEXT( "Failed to create node via HAPI: %s" ), *ErrorMessage);
        return false;
    }

	if (!bParentEntryExists || !ParentHandle.IsValid())
		AddContainer(ParentIdentifier, ParentNodeId, ParentHandle);
	else
		UpdateContainer(ParentIdentifier, ParentNodeId);

	// Make sure we prevent node destruction if needed
	if (!bInputNodesCanBeDeleted)
		FHoudiniEngineUtils::UpdateInputNodeCanBeDeleted(ParentHandle, bInputNodesCanBeDeleted);
	else
		FHoudiniEngineUtils::UpdateInputNodeCanBeDeleted(ParentHandle, bInputNodesCanBeDeleted);
	
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
FUnrealObjectInputManagerImpl::MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier)
{
	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;
	
	Node->MarkAsDirty();
	
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

bool FUnrealObjectInputManagerImpl::GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const
{
	FUnrealObjectInputNode* Node = nullptr;
	if (!GetNodeByIdentifier(InIdentifier, Node))
		return false;
	if (!Node)
		return false;

	Node->GetHAPINodeIds(OutNodeIds);

	return true;
}

bool FUnrealObjectInputManagerImpl::GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const
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

int32
FUnrealObjectInputManagerImpl::GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid)
{
	if (FHoudiniEngineUtils::IsHoudiniNodeValid(WorldOriginNodeId))
		return WorldOriginNodeId;

	if (!bInCreateIfMissingOrInvalid)
		return -1;

	// Create a OBJ/null with default/identity transform
	constexpr HAPI_NodeId ParentNodeId = -1;
	constexpr bool bCookOnCreation = true;
	HAPI_NodeId NodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("Object/null"), TEXT("WorldOrigin"), bCookOnCreation, &NodeId), -1);

	WorldOriginNodeId = NodeId;
	return WorldOriginNodeId;
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
		FString ObjectName = IsValid(Actor) ? Actor->GetActorNameOrLabel() : InIdentifier.GetObject()->GetName();

		NameParts.Add(InIdentifier.GetObject()->GetName());

		const FUnrealObjectInputOptions& Options = InIdentifier.GetOptions();
		if (Type == EUnrealObjectInputNodeType::Reference)
			NameParts.Add(TEXT("merge"));
		if (Options.bExportColliders)
			NameParts.Add(TEXT("colliders"));
		if (Options.bExportLODs)
			NameParts.Add(TEXT("lods"));
		if (Options.bExportSockets)
			NameParts.Add(TEXT("sockets"));
		if (Options.bImportAsReference)
			NameParts.Add(TEXT("reference"));
		if (Options.bImportAsReferenceRotScaleEnabled)
			NameParts.Add(TEXT("reference_with_rot_scale"));
		return FString::Join(NameParts, TEXT("_"));		
	}
	
	// Get the object name, or for actors, get their label
	if (IsValid(Actor))
		return Actor->GetActorNameOrLabel();
	else
		return FPaths::GetBaseFilename(InIdentifier.GetNormalizedObjectPath().ToString());
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
FUnrealObjectInputManagerImpl::IsHAPINodeValid(const int32 InNodeId) const
{
	return FHoudiniEngineUtils::IsHoudiniNodeValid(InNodeId);
}

bool
FUnrealObjectInputManagerImpl::DeleteHAPINode(const int32 InNodeId) const
{
	return FHoudiniEngineUtils::DeleteHoudiniNode(InNodeId);
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
			if (Node->DeleteHAPINodes())
			{
				// Log if we could not delete the node
				HOUDINI_LOG_WARNING(TEXT("Could not delete HAPI node: %d"), Node->GetNodeId());
			}
		}

		InputNodes.Remove(InIdentifier);
		delete Node;
		Node = nullptr;
	}

	return true;
}
