#include "UnrealObjectInputUtils.h"

#include "UObject/Object.h"
#include "LandscapeSplinesComponent.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputRuntimeUtils.h"


bool
FUnrealObjectInputUtils::FindNodeViaManager(
	const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle)
{
	if (!InIdentifier.IsValid())
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputHandle Handle;
	if (!Manager->FindNode(InIdentifier, Handle))
		return false;

	OutHandle = Handle;
	return true;
}

FUnrealObjectInputNode*
FUnrealObjectInputUtils::GetNodeViaManager(const FUnrealObjectInputHandle& InHandle)
{
	if (!InHandle.IsValid())
		return nullptr;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return nullptr;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InHandle, Node))
		return nullptr;

	return Node;
}

FUnrealObjectInputNode*
FUnrealObjectInputUtils::GetNodeViaManager(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle)
{
	if (!InIdentifier.IsValid())
		return nullptr;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return nullptr;

	FUnrealObjectInputHandle Handle;
	if (!Manager->FindNode(InIdentifier, Handle))
		return nullptr;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(Handle, Node))
		return nullptr;

	OutHandle = Handle;
	return Node;
}

bool
FUnrealObjectInputUtils::FindParentNodeViaManager(
	const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle)
{
	if (!InIdentifier.IsValid())
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InIdentifier, Node) || !Node)
		return false;
	
	OutParentHandle = Node->GetParent();
	return true;
}

bool
FUnrealObjectInputUtils::AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle)
{
	if (!InHandle.IsValid())
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->AreHAPINodesValid(InHandle);
}

bool
FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle)
{
	if (!FindNodeViaManager(InIdentifier, OutHandle) || !AreHAPINodesValid(OutHandle) || FUnrealObjectInputRuntimeUtils::IsInputNodeDirty(InIdentifier))
		return false;

	return true;
}

bool
FUnrealObjectInputUtils::AreReferencedHAPINodesValid(const FUnrealObjectInputHandle& InHandle)
{
	if (!InHandle.IsValid() || InHandle.GetIdentifier().GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode const* Node = nullptr;
	if (!Manager->GetNode(InHandle, Node))
		return false;

	if (!Node)
		return false;

	FUnrealObjectInputReferenceNode const* const RefNode = static_cast<FUnrealObjectInputReferenceNode const*>(Node);
	return RefNode->AreReferencedHAPINodesValid();
}

bool
FUnrealObjectInputUtils::AddNodeOrUpdateNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const int32 InObjectNodeId,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const bool& bInputNodesCanBeDeleted,
	const TOptional<int32> InReferencesConnectToNodeId)
{
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputHandle Handle;
	const bool bNodeExists = FindNodeViaManager(InIdentifier, Handle) && Handle.IsValid();

	bool bSuccess = false;
	switch (InIdentifier.GetNodeType())
	{
		case EUnrealObjectInputNodeType::Container:
			if (bNodeExists)
			{
				bSuccess = Manager->UpdateContainer(InIdentifier, InNodeId);
			}
			else
			{
				bSuccess = Manager->AddContainer(InIdentifier, InNodeId, Handle);
			}
			break;
		case EUnrealObjectInputNodeType::Reference:
			if (bNodeExists)
			{
				bSuccess = Manager->UpdateReferenceNode(InIdentifier, InObjectNodeId, InNodeId, InReferencedNodes, InReferencesConnectToNodeId);
			}
			else
			{
				bSuccess = Manager->AddReferenceNode(InIdentifier, InObjectNodeId, InNodeId, Handle, InReferencedNodes, InReferencesConnectToNodeId.Get(INDEX_NONE));
			}
			break;
		case EUnrealObjectInputNodeType::Leaf:
			if (bNodeExists)
			{
				bSuccess = Manager->UpdateLeaf(InIdentifier, InObjectNodeId, InNodeId);
			}
			else
			{
				bSuccess = Manager->AddLeaf(InIdentifier, InObjectNodeId, InNodeId, Handle);
			}
			break;
		case EUnrealObjectInputNodeType::Invalid:
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::AddNodeOrUpdateNode] Invalid NodeType in identifier."));
			bSuccess = false;
			break;
	}

	if (!bSuccess)
		return false;

	// Make sure to prevent deletion of the node and its parents if needed
	if (!bInputNodesCanBeDeleted)
	{
		//Manager->EnsureParentsExist(InIdentifier, Handle, bInputNodesCanBeDeleted);
		FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);
	}

	OutHandle = Handle;
	return bSuccess;
}

bool
FUnrealObjectInputUtils::GetHAPINodeId(const FUnrealObjectInputHandle& InHandle, int32& OutNodeId)
{
	if (!InHandle.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;
	
	const FUnrealObjectInputNode* Node;
	Manager->GetNode(InHandle, Node);
	OutNodeId = Node->GetHAPINodeId();

	return true;
}

bool
FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(const FUnrealObjectInputHandle& InHandle, const bool& bCanBeDeleted)
{
	if (!InHandle.IsValid())
		return false;

	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	Manager->GetNode(InHandle, Node);

	if (!Node)
		return false;

	// Only set the value to false if it isnt already
	if(!bCanBeDeleted)
		Node->SetCanBeDeleted(bCanBeDeleted);

	return true;
}

bool
FUnrealObjectInputUtils::GetDefaultInputNodeName(const FUnrealObjectInputIdentifier& InIdentifier, FString& OutNodeName)
{
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	OutNodeName = Manager->GetDefaultNodeName(InIdentifier);
	return true;
}

bool
FUnrealObjectInputUtils::EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted)
{
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->EnsureParentsExist(InIdentifier, OutParentHandle, bInputNodesCanBeDeleted);
}

bool
FUnrealObjectInputUtils::SetReferencedNodes(const FUnrealObjectInputHandle& InRefNodeHandle, const TSet<FUnrealObjectInputHandle>& InReferencedNodes)
{
	if (!InRefNodeHandle.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeHandle, Node))
		return false;

	FUnrealObjectInputReferenceNode* RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	RefNode->SetReferencedNodes(InReferencedNodes);
	return true;
}

bool
FUnrealObjectInputUtils::GetReferencedNodes(const FUnrealObjectInputHandle& InRefNodeHandle, TSet<FUnrealObjectInputHandle>& OutReferencedNodes)
{
	if (!InRefNodeHandle.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeHandle, Node))
		return false;

	FUnrealObjectInputReferenceNode* RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	RefNode->GetReferencedNodes(OutReferencedNodes);
	return true;
}

bool
FUnrealObjectInputUtils::BuildMeshInputObjectIdentifiers(
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
	TArray<FUnrealObjectInputIdentifier>& OutPerOptionIdentifiers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUnrealObjectInputUtils::BuildMeshInputObjectIdentifiers);

	FUnrealObjectInputOptions DefaultOptions;
	DefaultOptions.bImportAsReference = false;
	DefaultOptions.bImportAsReferenceRotScaleEnabled = false;
	DefaultOptions.bExportLODs = false;
	DefaultOptions.bExportSockets = false;
	DefaultOptions.bExportColliders = false;
	DefaultOptions.bMainMeshIsNaniteFallbackMesh = false;
	DefaultOptions.bExportMaterialParameters = bExportMaterialParameters;

	// The Nanite fallback mesh is only applicable if we are exporting the main mesh and/or LODs
	const bool bEffectiveMainMeshIsNaniteFallbackMesh = bInMainMeshIsNaniteFallbackMesh && (bInExportMainMesh || bInExportLODs);
	
	// Determine number of leaves
	uint32 NumLeaves = 0;
	if (bInExportMainMesh)
		NumLeaves++;
	if (bInExportLODs)
		NumLeaves++;
	if (bInExportSockets)
		NumLeaves++;
	if (bInExportColliders)
		NumLeaves++;
	if (NumLeaves <= 1 && !bForceCreateReferenceNode)
	{
		// Only one active option, we can create a leaf node and not a reference node
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLODs = bInExportLODs;
		Options.bExportSockets = bInExportSockets;
		Options.bExportColliders = bInExportColliders;
		Options.bMainMeshIsNaniteFallbackMesh = bEffectiveMainMeshIsNaniteFallbackMesh;

		constexpr bool bIsLeaf = true;
		bOutSingleLeafNodeOnly = true;
		OutReferenceNode = FUnrealObjectInputIdentifier();
		OutPerOptionIdentifiers = {FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf)};
		return true;
	}

	bOutSingleLeafNodeOnly = false;
	// Construct the reference node's identifier
	{
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLODs = bInExportLODs;
		Options.bExportSockets = bInExportSockets;
		Options.bExportColliders = bInExportColliders;
		Options.bMainMeshIsNaniteFallbackMesh = bEffectiveMainMeshIsNaniteFallbackMesh;
		
		constexpr bool bIsLeaf = false;
		OutReferenceNode = FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf);
	}

	// Construct per-option identifiers
	TArray<FUnrealObjectInputIdentifier> PerOptionIdentifiers;
	// First one is the main mesh only, so all options false
	if (bInExportMainMesh)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bMainMeshIsNaniteFallbackMesh = bEffectiveMainMeshIsNaniteFallbackMesh;
		// TODO: add a specific main mesh option?
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}
	
	if (bInExportLODs)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLODs = true;
		Options.bMainMeshIsNaniteFallbackMesh = bEffectiveMainMeshIsNaniteFallbackMesh;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}

	if (bInExportSockets)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportSockets = true;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}

	if (bInExportColliders)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportColliders = true;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}

	OutPerOptionIdentifiers = MoveTemp(PerOptionIdentifiers);
	return true;
}

bool
FUnrealObjectInputUtils::BuildLandscapeSplinesInputObjectIdentifiers(
	ULandscapeSplinesComponent const* const InSplinesComponent,
	const bool bInExportSplineCurves,
	const bool bInExportControlPoints,
	const bool bInExportLeftRightCurves,
	const float InUnrealSplineResolution,
	const bool bForceCreateReferenceNode,
	bool &bOutSingleLeafNodeOnly,
	FUnrealObjectInputIdentifier& OutReferenceNode,
	TArray<FUnrealObjectInputIdentifier>& OutPerOptionIdentifiers)
{
	const FUnrealObjectInputOptions DefaultOptions;

	// Determine number of leaves
	uint32 NumLeaves = 0;
	if (bInExportSplineCurves)
		NumLeaves++;
	if (bInExportControlPoints)
		NumLeaves++;
	if (bInExportLeftRightCurves)
		NumLeaves++;
	if (NumLeaves <= 1 && !bForceCreateReferenceNode)
	{
		// Only one active option, we can create a leaf node and not a reference node
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLandscapeSplineControlPoints = bInExportControlPoints;
		Options.bExportLandscapeSplineLeftRightCurves = bInExportLeftRightCurves;
		// Spline resolution only affects the curves, not the control point cloud
		if (!bInExportControlPoints)
			Options.UnrealSplineResolution = InUnrealSplineResolution;

		constexpr bool bIsLeaf = true;
		bOutSingleLeafNodeOnly = true;
		OutReferenceNode = FUnrealObjectInputIdentifier();
		OutPerOptionIdentifiers = {FUnrealObjectInputIdentifier(InSplinesComponent, Options, bIsLeaf)};
		return true;
	}

	bOutSingleLeafNodeOnly = false;
	// Construct the reference node's identifier
	{
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLandscapeSplineControlPoints = bInExportControlPoints;
		Options.bExportLandscapeSplineLeftRightCurves = bInExportLeftRightCurves;
		Options.UnrealSplineResolution = InUnrealSplineResolution;
		
		constexpr bool bIsLeaf = false;
		OutReferenceNode = FUnrealObjectInputIdentifier(InSplinesComponent, Options, bIsLeaf);
	}

	// Construct per-option identifiers
	TArray<FUnrealObjectInputIdentifier> PerOptionIdentifiers;
	// First one is the spline curves only, so all options false
	if (bInExportSplineCurves)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		// TODO: add a specific spline curves option?
		Options.UnrealSplineResolution = InUnrealSplineResolution;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InSplinesComponent, Options, bIsLeaf));
	}
	
	if (bInExportControlPoints)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLandscapeSplineControlPoints = true;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InSplinesComponent, Options, bIsLeaf));
	}

	if (bInExportLeftRightCurves)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLandscapeSplineLeftRightCurves = true;
		Options.UnrealSplineResolution = InUnrealSplineResolution;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InSplinesComponent, Options, bIsLeaf));
	}

	OutPerOptionIdentifiers = MoveTemp(PerOptionIdentifiers);
	return true;
}

bool
FUnrealObjectInputUtils::SetObjectMergeXFormTypeToWorldOrigin(const HAPI_NodeId& InObjMergeNodeId)
{
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;
		
	const HAPI_NodeId WorldOriginNodeId = Manager->GetWorldOriginHAPINodeId();
	if (WorldOriginNodeId < 0)
	{
		HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::SetObjectMergeXFormTypeToWorldOrigin] Could not find/create world origin null."));
		return false;
	}

	// Set the transform value to "Into Specified Object"
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, InObjMergeNodeId, TCHAR_TO_UTF8(TEXT("xformtype")), 0, 2), false);
	// Set the transform object to the world origin null from the manager
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmNodeValue(Session, InObjMergeNodeId, TCHAR_TO_UTF8(TEXT("xformpath")), WorldOriginNodeId), false);

	return true;
}

bool
FUnrealObjectInputUtils::SetReferencesNodeConnectToNodeId(const FUnrealObjectInputIdentifier& InRefNodeIdentifier, const HAPI_NodeId InNodeId)
{
	// Identifier must be valid and for a reference node
	if (!InRefNodeIdentifier.IsValid() || InRefNodeIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;

	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeIdentifier, Node))
		return false;

	FUnrealObjectInputReferenceNode* const RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	RefNode->SetReferencesConnectToNodeId(InNodeId);

	return true;
}

bool
FUnrealObjectInputUtils::ConnectReferencedNodesToMerge(const FUnrealObjectInputIdentifier& InRefNodeIdentifier)
{
	// Identifier must be valid and for a reference node
	if (!InRefNodeIdentifier.IsValid() || InRefNodeIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;

	if (!AreHAPINodesValid(InRefNodeIdentifier))
		return false;
	
	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeIdentifier, Node))
		return false;

	FUnrealObjectInputReferenceNode* const RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	const HAPI_Session* const Session = FHoudiniEngine::Get().GetSession();
	const FUnrealObjectInputHAPINodeId RefNodeId = RefNode->GetReferencesConnectToNodeId();
	if (!RefNodeId.IsValid())
		return false;

	const HAPI_NodeId RefHAPINodeId = RefNodeId.GetHAPINodeId();

	// Get currently connected inputs
	TArray<HAPI_NodeId> PrevConnectedNodes;
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	if (FHoudiniApi::GetNodeInfo(Session, RefHAPINodeId, &NodeInfo) == HAPI_RESULT_SUCCESS)
	{
		// There is no function in HAPI currently to directly get the number of _connected_ input nodes or to
		// compose a list of the connected input nodes.
		// Nodes with "infinite" inputs, such as the Merge SOP, always have NodeInfo.inputCount == 9999. So we
		// stop iteration at the first disconnected input instead of visiting all 9999 possible indices.
		for (int32 InputIndex = 0; InputIndex < NodeInfo.inputCount; ++InputIndex)
		{
			HAPI_NodeId ConnectedInputNodeId = -1;
			if (FHoudiniApi::QueryNodeInput(Session, RefHAPINodeId, InputIndex, &ConnectedInputNodeId) != HAPI_RESULT_SUCCESS || ConnectedInputNodeId < 0)
				break;
			PrevConnectedNodes.Add(ConnectedInputNodeId);
		}
	}

	// Connect referenced nodes
	const TSet<FUnrealObjectInputBackLinkHandle>& ReferencedNodes = RefNode->GetReferencedNodes();
	TSet<HAPI_NodeId> ConnectedNodeSet;
	ConnectedNodeSet.Reserve(ReferencedNodes.Num());
	int32 InputIndex = 0;
	for (const FUnrealObjectInputHandle& Handle : ReferencedNodes)
	{
		if (!Handle.IsValid())
			continue;

		HAPI_NodeId NodeId = -1;
		if (!GetHAPINodeId(Handle, NodeId))
			continue;

		// Connect the current input object to the merge node
		if (FHoudiniApi::ConnectNodeInput(Session, RefHAPINodeId, InputIndex, NodeId, 0) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::ConnectReferencedNodes] Failed to connected node input: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			continue;
		}
		// HAPI will automatically create a object_merge node (we are expecting that NodeId and RefNodeId are never in
		// the same network), we need to set the xformtype to "Into specified object"
		HAPI_NodeId ConnectedNodeId = -1;
		if (FHoudiniApi::QueryNodeInput(Session, RefHAPINodeId, InputIndex, &ConnectedNodeId) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::ConnectReferencedNodes] Failed to query connected node input: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			continue;

		}
		InputIndex++;

		if (ConnectedNodeId < 0)
		{
			// No connection was made even though the previous functions were successful!?
			continue;
		}

		ConnectedNodeSet.Add(ConnectedNodeId);
		
		// Set the transform value to "Into Specified Object"
		// Set the transform object to the world origin null from the manager
		SetObjectMergeXFormTypeToWorldOrigin(ConnectedNodeId);
	}

	// Disconnect previously connected nodes at indices >= FirstUnusedInputIndex
	// Disconnect in reverse: inputs are consolidated on disconnect on "infinite" input nodes like the Merge SOP
	const int32 FirstUnusedInputIndex = InputIndex; 
	for (int32 InputIndexToDelete = PrevConnectedNodes.Num(); InputIndexToDelete >= FirstUnusedInputIndex; --InputIndexToDelete)
	{
		HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(Session, RefHAPINodeId, InputIndexToDelete));
	}

	// Delete nodes from previous connections that are no longer used (the object merge SOPs automatically created
	// by HAPI) 
	for (const HAPI_NodeId& NodeToDeleteId : PrevConnectedNodes)
	{
		if (ConnectedNodeSet.Contains(NodeToDeleteId))
			continue;
		// Check that the node is valid / still exists before attempting to delete the node
		HAPI_NodeInfo NodeToDeleteInfo;
		FHoudiniApi::NodeInfo_Init(&NodeToDeleteInfo);
		if (FHoudiniApi::GetNodeInfo(Session, NodeToDeleteId, &NodeInfo) != HAPI_RESULT_SUCCESS)
			continue;
		bool bNodeIsValid = false;
		if (FHoudiniApi::IsNodeValid(Session, NodeToDeleteId, NodeInfo.uniqueHoudiniNodeId, &bNodeIsValid) != HAPI_RESULT_SUCCESS || !bNodeIsValid)
			continue;
		HOUDINI_CHECK_ERROR(FHoudiniApi::DeleteNode(Session, NodeToDeleteId));
	}
	
	return true;
}

bool
FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const TSet<FUnrealObjectInputHandle>& InReferencedNodes,
	FUnrealObjectInputHandle& OutHandle,
	const bool bInConnectReferencedNodes,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode);

	// Identifier must be valid and for a reference node
	if (!InIdentifier.IsValid() || InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;
	
	FUnrealObjectInputHandle Handle;
	if (!FindNodeViaManager(InIdentifier, Handle) || !AreHAPINodesValid(Handle))
	{
		// Add the node without an node id first
		// Then get its parent id and create the HAPI node inside the parent's network
		int32 ObjectNodeId = -1;
		int32 NodeId = -1;		
		if (!AddNodeOrUpdateNode(InIdentifier, NodeId, Handle, ObjectNodeId, &InReferencedNodes, bInputNodesCanBeDeleted))
			return false;

		// Get the parent node id
		int32 ParentNodeId = -1;
		FUnrealObjectInputHandle ParentHandle;
		if (EnsureParentsExist(InIdentifier, ParentHandle, bInputNodesCanBeDeleted))
			GetHAPINodeId(ParentHandle, ParentNodeId);

		FString NodeName;
		GetDefaultInputNodeName(InIdentifier, NodeName);
		// Create the object node
		if (FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("geo"), NodeName, true, &ObjectNodeId) != HAPI_RESULT_SUCCESS)
			return false;
		// Create the merge node
		if (FHoudiniEngineUtils::CreateNode(ObjectNodeId, TEXT("merge"), NodeName, true, &NodeId) != HAPI_RESULT_SUCCESS)
			return false;
		
		if (!AddNodeOrUpdateNode(InIdentifier, NodeId, Handle, ObjectNodeId, &InReferencedNodes, bInputNodesCanBeDeleted))
			return false;

		if (bInConnectReferencedNodes)
		{
			if (!ConnectReferencedNodesToMerge(InIdentifier))
				return false;
		}

		FUnrealObjectInputRuntimeUtils::ClearInputNodeDirtyFlag(InIdentifier);
		
		OutHandle = Handle;
		return true;
	}

	SetReferencedNodes(Handle, InReferencedNodes);
	if (bInConnectReferencedNodes)
	{
		if (!ConnectReferencedNodesToMerge(InIdentifier))
			return false;
	}

	FUnrealObjectInputRuntimeUtils::ClearInputNodeDirtyFlag(InIdentifier);

	OutHandle = Handle;
	return true;
}

bool
FUnrealObjectInputUtils::AddModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, const int32 InNodeIdToConnectTo)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;
	
	return Node->AddModifierChain(InChainName, InNodeIdToConnectTo);
}

bool
FUnrealObjectInputUtils::SetModifierChainNodeToConnectTo(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, const int32 InNodeToConnectTo)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;
	
	return Node->SetModifierChainNodeToConnectTo(InChainName, InNodeToConnectTo);
}

bool
FUnrealObjectInputUtils::DoesModifierChainExist(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->GetModifierChain(InChainName) != nullptr; 
}

HAPI_NodeId
FUnrealObjectInputUtils::GetInputNodeOfModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return INDEX_NONE;

	return Node->GetInputHAPINodeIdOfModifierChain(InChainName);
}

HAPI_NodeId
FUnrealObjectInputUtils::GetOutputNodeOfModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return INDEX_NONE;

	return Node->GetOutputHAPINodeIdOfModifierChain(InChainName);
}

bool
FUnrealObjectInputUtils::RemoveModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->RemoveModifierChain(InChainName);
}

bool
FUnrealObjectInputUtils::DestroyModifier(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, FUnrealObjectInputModifier* const InModifierToDestroy)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->DestroyModifier(InChainName, InModifierToDestroy);
}

FUnrealObjectInputModifier*
FUnrealObjectInputUtils::FindFirstModifierOfType(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, EUnrealObjectInputModifierType InModifierType)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return nullptr;

	return Node->FindFirstModifierOfType(InChainName, InModifierType);
}

bool
FUnrealObjectInputUtils::UpdateModifiers(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->UpdateModifiers(InChainName);
}

bool
FUnrealObjectInputUtils::UpdateAllModifierChains(const FUnrealObjectInputIdentifier& InIdentifier)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->UpdateAllModifierChains();
}

