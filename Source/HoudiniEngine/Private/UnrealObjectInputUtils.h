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

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Misc/Optional.h"
#include "UObject/NameTypes.h"

#include "HAPI/HAPI_Common.h"

#include "UnrealObjectInputRuntimeTypes.h"


// UE forward declarations
class UObject;
class ULandscapeSplinesComponent;


struct HOUDINIENGINE_API FUnrealObjectInputUtils
{
	public:
		// Find the node in the input manager that corresponds to Identifier. If it could not be found return false.
		// If found, return true and set Handle to reference the node.
		static bool FindNodeViaManager(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle);

		// Returns the node associated with InHandle. Returns nullptr if the handle is invalid or the node does not exist.
		static FUnrealObjectInputNode* GetNodeViaManager(const FUnrealObjectInputHandle& InHandle);

		// Returns the node associated with InIdentifier. Returns nullptr if the handle is invalid or the node does not exist.
		static FUnrealObjectInputNode* GetNodeViaManager(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle);

		// Helper to get a handle to the parent of node of InHandle.
		static bool FindParentNodeViaManager(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle);

		// Returns true if the HAPI nodes for input referenced by InHandle are valid (exist).
		static bool AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle);

		// Helper that checks that the input entry associated with InIdentifier exists, the HAPI nodes for are valid and
		// the entry is not marked as dirty.
		static bool NodeExistsAndIsNotDirty(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle);

		// Returns true if the HAPI nodes referenced by the input reference node of InHandle are valid (exist).
		static bool AreReferencedHAPINodesValid(const FUnrealObjectInputHandle& InHandle);

		// Add an entry for an input to track and reference count in the manager.
		// Returns true on success and sets OutHandle to point to the entry.
		static bool AddNodeOrUpdateNode(
			const FUnrealObjectInputIdentifier& InIdentifier,
			const int32 NodeId,
			FUnrealObjectInputHandle& OutHandle,
			const int32 InObjectNodeId,
			TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
			const bool& bInputNodesCanBeDeleted,
			const TOptional<int32> InReferencesConnectToNodeId=TOptional<int32>());

		// Helper to get the HAPI NodeId associated with InHandle.
		static bool GetHAPINodeId(const FUnrealObjectInputHandle& InHandle, int32& OutNodeId);

		// Helper to set the CanBeDeleted property on the input object associated with InHandle
		static bool UpdateInputNodeCanBeDeleted(const FUnrealObjectInputHandle& InHandle, const bool& bCanBeDeleted);

		// Helper to get the default input node name to use via the new input system.
		static bool GetDefaultInputNodeName(const FUnrealObjectInputIdentifier& InIdentifier, FString& OutNodeName);

		// Helper to ensure that the parent/container nodes of a given identifier exist
		static bool EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted);

		// Helper to set the references on a reference node
		static bool SetReferencedNodes(const FUnrealObjectInputHandle& InRefNodeHandle, const TSet<FUnrealObjectInputHandle>& InReferencedNodes);

		// Helper to get the references on a reference node
		static bool GetReferencedNodes(const FUnrealObjectInputHandle& InRefNodeHandle, TSet<FUnrealObjectInputHandle>& OutReferencedNodes);

		// Helper to build identifiers for static/skeletal mesh inputs based on options (such as LODs, Colliders, Sockets)
		static bool BuildMeshInputObjectIdentifiers(
			UObject const* const InInputObject,
			const bool bInExportMainMesh,
			const bool bInExportLODs,
			const bool bInExportSockets,
			const bool bInExportColliders,
			const bool bInMainMeshIsNaniteFallbackMesh,
			const bool bExportMaterialParameters,
			const bool bForceCreateReferenceNode,
			bool &bOutSingleLeafNodeOnly,
			FUnrealObjectInputIdentifier& OutReferenceNode,
			TArray<FUnrealObjectInputIdentifier>& OutPerOptionIdentifiers);

		// Helper to build identifiers for landscape spline inputs based on options (such as send control points)
		static bool BuildLandscapeSplinesInputObjectIdentifiers(
			ULandscapeSplinesComponent const* const InSplinesComponent,
			const bool bInExportSplineCurves,
			const bool bInExportControlPoints,
			const bool bInExportLeftRightCurves,
			const float InUnrealSplineResolution,
			const bool bForceCreateReferenceNode,
			bool &bOutSingleLeafNodeOnly,
			FUnrealObjectInputIdentifier& OutReferenceNode,
			TArray<FUnrealObjectInputIdentifier>& OutPerOptionIdentifiers);

		// Helper to set an object_merge SOP's xformtype to "Into Specified Object" and the xformpath to the manager's
		// WorldOrigin null.
		static bool SetObjectMergeXFormTypeToWorldOrigin(const HAPI_NodeId& InObjMergeNodeId);

		// Helper to set the node that references connect to (such as a Merge SOP) for a reference node in the input system	
		static bool SetReferencesNodeConnectToNodeId(const FUnrealObjectInputIdentifier& InRefNodeIdentifier, HAPI_NodeId InNodeId);

		// Helper to connect a reference node's referenced nodes to its merge SOP
		static bool ConnectReferencedNodesToMerge(const FUnrealObjectInputIdentifier& InRefNodeIdentifier);

		// Helper to create a SOP/merge for merging InReferencedNodes.
		static bool CreateOrUpdateReferenceInputMergeNode(
			const FUnrealObjectInputIdentifier& InIdentifier,
			const TSet<FUnrealObjectInputHandle>& InReferencedNodes,
			FUnrealObjectInputHandle& OutHandle,
			const bool bInConnectReferencedNode,
			const bool& bInputNodesCanBeDeleted);

		// Add a named modifier chain to the input node. Returns false if the chain was not added (if it existed already, for example)
		static bool AddModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, int32 InNodeIdToConnectTo);

		// Set the node that the first node of the chain connects to.
		static bool SetModifierChainNodeToConnectTo(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, int32 InNodeToConnectTo);

		// Returns true if the named modifier chain exists on the node.
		static bool DoesModifierChainExist(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName);

		// Returns the input node id of the named modifier chain on the node. Returns < 0 if the chain does not exist.
		static HAPI_NodeId GetInputNodeOfModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName);

		// Returns the output node id of the named modifier chain on the node. Returns < 0 if the chain does not exist.
		static HAPI_NodeId GetOutputNodeOfModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName);

		// Removed the named modifier chain. Returns true if the chain existed and was removed.
		static bool RemoveModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName);

		// Destroy the given modifier in the named chain. This deletes the modifier, so InModifierToDestroy will be invalid after a successful call to this function.
		static bool DestroyModifier(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, FUnrealObjectInputModifier* InModifierToDestroy);

		// Find and return the first modifier of class T on the node identified by InIdentifier. Returns null if it has
		// no such modifier.
		static FUnrealObjectInputModifier* FindFirstModifierOfType(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, EUnrealObjectInputModifierType InModifierType);

		// Add a modifier of class T to the node identified by InIdentifier if it does not already have a modifier
		// of that class. Return the newly created and added (or existing) modifier.
		template<class T, class... Args>
		static T* CreateAndAddModifier(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, Args... ConstructorArguments);

		// Call FUnrealInputObjectModifier::Update() on all modifiers in the chain.
		static bool UpdateModifiers(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName);

		// Call FUnrealInputObjectModifier::Update() on all modifiers on the node identified by InIdentifier.
		static bool UpdateAllModifierChains(const FUnrealObjectInputIdentifier& InIdentifier);
};


template<class T, class... Args>
T* FUnrealObjectInputUtils::CreateAndAddModifier(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, Args... ConstructorArguments)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return nullptr;

	return Node->CreateAndAddModifier<T>(InChainName, ConstructorArguments...);
}
