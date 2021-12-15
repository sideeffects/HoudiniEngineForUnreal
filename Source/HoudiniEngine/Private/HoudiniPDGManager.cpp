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

#include "HoudiniPDGManager.h"

#include "Modules/ModuleManager.h"
#include "MessageEndpointBuilder.h"
#include "HAL/FileManager.h"

#include "HoudiniApi.h"
#include "HoudiniAsset.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniPackageParams.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniPDGTranslator.h"
#include "HoudiniPDGImporterMessages.h"

#include "HAPI/HAPI_Common.h"

HOUDINI_PDG_DEFINE_LOG_CATEGORY();

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FHoudiniPDGManager::FHoudiniPDGManager()
{
}

FHoudiniPDGManager::~FHoudiniPDGManager()
{
}

bool
FHoudiniPDGManager::InitializePDGAssetLink(UHoudiniAssetComponent* InHAC)
{
	if (!IsValid(InHAC))
		return false;

	int32 AssetId = InHAC->GetAssetId();
	if (AssetId < 0)
		return false;

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid((HAPI_NodeId)AssetId))
		return false;

	// Create a new PDG Asset Link Object
	bool bRegisterPDGAssetLink = false;
	UHoudiniPDGAssetLink* PDGAssetLink = InHAC->GetPDGAssetLink();		
	if (!IsValid(PDGAssetLink))
	{
		PDGAssetLink = NewObject<UHoudiniPDGAssetLink>(InHAC, UHoudiniPDGAssetLink::StaticClass(), NAME_None, RF_Transactional);
		bRegisterPDGAssetLink = true;
	}

	if (!IsValid(PDGAssetLink))
		return false;
	
	PDGAssetLink->AssetID = AssetId;
	
	// Get the HDA's info
	HAPI_NodeInfo AssetInfo;
	FHoudiniApi::NodeInfo_Init(&AssetInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID, &AssetInfo), false);

	// Get the node path
	FString AssetNodePath;
	FHoudiniEngineString::ToFString(AssetInfo.internalNodePathSH, PDGAssetLink->AssetNodePath);

	// Get the node name
	FString AssetName;
	FHoudiniEngineString::ToFString(AssetInfo.nameSH, PDGAssetLink->AssetName);

	const bool bZeroWorkItemTallys = true;
	if (!FHoudiniPDGManager::PopulateTOPNetworks(PDGAssetLink, bZeroWorkItemTallys))
	{
		// We couldn't find any valid TOPNet/TOPNode, this is not a PDG Asset
		// Make sure the HDA doesn't have a PDGAssetLink
		InHAC->SetPDGAssetLink(nullptr);
		return false;
	}

	// If the PDG asset link comes from a loaded asset, we also need to register it
	if (InHAC->HasBeenLoaded())
	{
		bRegisterPDGAssetLink = true;
	}

	// We have found valid TOPNetworks and TOPNodes,
	// This is a PDG HDA, so add the AssetLink to it
	PDGAssetLink->LinkState = EPDGLinkState::Linked;

	if (PDGAssetLink->SelectedTOPNetworkIndex < 0)
		PDGAssetLink->SelectedTOPNetworkIndex = 0;

	InHAC->SetPDGAssetLink(PDGAssetLink);

	if (bRegisterPDGAssetLink)
	{
		// Register this PDG Asset Link to the PDG Manager
		TWeakObjectPtr<UHoudiniPDGAssetLink> AssetLinkPtr(PDGAssetLink);
		PDGAssetLinks.Add(AssetLinkPtr);
	}

	// If the commandlet is enabled, check if we have started and established communication with the commandlet yet
	// if not, try to start the commandlet
	bool bCommandletIsEnabled = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (IsValid(HoudiniRuntimeSettings))
	{
		bCommandletIsEnabled = HoudiniRuntimeSettings->bPDGAsyncCommandletImportEnabled;
	}

	if (bCommandletIsEnabled)
	{
		const EHoudiniBGEOCommandletStatus CommandletStatus = UpdateAndGetBGEOCommandletStatus();
		if (CommandletStatus == EHoudiniBGEOCommandletStatus::NotStarted && bCommandletIsEnabled)
		{
			CreateBGEOCommandletAndEndpoint();
		}
	}

	return true;
}

bool
FHoudiniPDGManager::UpdatePDGAssetLink(UHoudiniPDGAssetLink* PDGAssetLink)
{
	if (!IsValid(PDGAssetLink))
		return false;

	// If the PDG Asset link is inactive, indicate that our HDA must be instantiated
	if (PDGAssetLink->LinkState == EPDGLinkState::Inactive)
	{
		UHoudiniAssetComponent* ParentHAC = Cast<UHoudiniAssetComponent>(PDGAssetLink->GetOuter());
		if(!ParentHAC)
		{
			// No valid parent HAC, error!
			PDGAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
			HOUDINI_LOG_ERROR(TEXT("No valid Houdini Asset Component parent for PDG Asset Link!"));
		}
		else if (ParentHAC && ParentHAC->GetAssetState() == EHoudiniAssetState::NeedInstantiation)
		{
			PDGAssetLink->LinkState = EPDGLinkState::Linking;
			ParentHAC->SetAssetState(EHoudiniAssetState::PreInstantiation);
		}
		else
		{
			PDGAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
			HOUDINI_LOG_ERROR(TEXT("Unable to link the PDG Asset link! Try to rebuild the HDA."));
		}
	}
	
	if (PDGAssetLink->LinkState == EPDGLinkState::Linking)
	{
		return true;
	}

	if (PDGAssetLink->LinkState != EPDGLinkState::Linked)
	{
		UHoudiniAssetComponent* ParentHAC = Cast<UHoudiniAssetComponent>(PDGAssetLink->GetOuter());
		int32 AssetId = ParentHAC->GetAssetId();
		if (AssetId < 0)
			return false;

		if (!FHoudiniEngineUtils::IsHoudiniNodeValid((HAPI_NodeId)AssetId))
			return false;

		PDGAssetLink->AssetID = AssetId;
	}

	if(!PopulateTOPNetworks(PDGAssetLink))
	{
		PDGAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
		HOUDINI_LOG_ERROR(TEXT("Failed to populte the PDG Asset Link."));
		return false;
	}

	return true;
}


bool
FHoudiniPDGManager::PopulateTOPNetworks(UHoudiniPDGAssetLink* PDGAssetLink, bool bInZeroWorkItemTallys)
{
	// Find all TOP networks from linked HDA, as well as the TOP nodes within, and populate internal state.
	if (!IsValid(PDGAssetLink))
		return false;

	// Get all the network nodes within the asset, recursively.
	// We're getting all networks because TOP network SOPs aren't considered being of TOP network type, but SOP type
	int32 NetworkNodeCount = 0;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID,
		HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_NETWORK, true, &NetworkNodeCount), false);

	if (NetworkNodeCount <= 0)
		return false;

	TArray<HAPI_NodeId> AllNetworkNodeIDs;
	AllNetworkNodeIDs.SetNum(NetworkNodeCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
		FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID,
		AllNetworkNodeIDs.GetData(), NetworkNodeCount), false);

	// There is currently no way to only get non bypassed nodes via HAPI
	// So we now need to get a list of all the bypassed top nets, in order to remove them from the previous list...
	TArray<HAPI_NodeId> AllBypassedTOPNetNodeIDs;
	{
		int32 BypassedTOPNetNodeCount = 0;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID,
			HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_NETWORK | HAPI_NODEFLAGS_BYPASS, true, &BypassedTOPNetNodeCount), false);

		if (BypassedTOPNetNodeCount > 0)
		{
			// Get the list of all bypassed TOP Net...
			AllBypassedTOPNetNodeIDs.SetNum(BypassedTOPNetNodeCount);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
				FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID,
				AllBypassedTOPNetNodeIDs.GetData(), BypassedTOPNetNodeCount), false);

			// ... and remove them from the network list
			for (int32 Idx = AllNetworkNodeIDs.Num() - 1; Idx >= 0; Idx--)
			{
				if (AllBypassedTOPNetNodeIDs.Contains(AllNetworkNodeIDs[Idx]))
					AllNetworkNodeIDs.RemoveAt(Idx);
			}
		}
	}
	

	// For each Network we found earlier, only add those with TOP child nodes 
	// Therefore guaranteeing that we only add TOP networks
	TArray<UTOPNetwork*> AllTOPNetworks;
	for (const HAPI_NodeId& CurrentNodeId : AllNetworkNodeIDs)
	{
		if (CurrentNodeId < 0)
		{
			continue;
		}

		HAPI_NodeInfo CurrentNodeInfo;
		FHoudiniApi::NodeInfo_Init(&CurrentNodeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId, &CurrentNodeInfo))
		{
			continue;
		}
		
		// Skip non TOP or SOP networks
		if (CurrentNodeInfo.type != HAPI_NodeType::HAPI_NODETYPE_TOP
			&& CurrentNodeInfo.type != HAPI_NodeType::HAPI_NODETYPE_SOP)
		{
			continue;
		}

		// Check that this TOP Net is not nested in another TOP Net...
		// This will happen with ROP Geometry TOPs for example...
		bool bIsNestedInTOPNet = false;
		HAPI_NodeId CurrentParentId = CurrentNodeInfo.parentId;
		while (CurrentParentId > 0)
		{
			if (AllNetworkNodeIDs.Contains(CurrentParentId))
			{
				bIsNestedInTOPNet = true;
				break;
			}

			if(AllBypassedTOPNetNodeIDs.Contains(CurrentParentId))
			{
				bIsNestedInTOPNet = true;
				break;
			}

			HAPI_NodeInfo ParentNodeInfo;
			FHoudiniApi::NodeInfo_Init(&ParentNodeInfo);
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
				FHoudiniEngine::Get().GetSession(), CurrentParentId, &ParentNodeInfo))
			{
				break;
			}

			// Get our parent's parent
			CurrentParentId = ParentNodeInfo.parentId;
		}

		// Skip nested TOP nets
		if (bIsNestedInTOPNet)
			continue;

		// Get the list of all TOP nodes within the current network (ignoring schedulers)		
		int32 TOPNodeCount = 0;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId,
			HAPI_NodeType::HAPI_NODETYPE_TOP, HAPI_NODEFLAGS_TOP_NONSCHEDULER, true, &TOPNodeCount))
		{
			continue;
		}

		TArray<HAPI_NodeId> AllTOPNodeIDs;
		AllTOPNodeIDs.SetNum(TOPNodeCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId, AllTOPNodeIDs.GetData(), TOPNodeCount), false);

		// Skip networks without TOP nodes
		if (AllTOPNodeIDs.Num() <= 0)
		{
			continue;
		}

		// Since there is currently no way to get only non-bypassed nodes via HAPI
		// we need to get a list of all the bypassed top nodes to remove them from the previous list
		{
			int32 BypassedTOPNodeCount = 0;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(), CurrentNodeId,
				HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_TOP_NONSCHEDULER | HAPI_NODEFLAGS_BYPASS, true, &BypassedTOPNodeCount), false);

			if (BypassedTOPNodeCount > 0)
			{
				// Get the list of all bypassed TOP Nodes...
				TArray<HAPI_NodeId> AllBypassedTOPNodes;
				AllBypassedTOPNodes.SetNum(BypassedTOPNodeCount);
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
					FHoudiniEngine::Get().GetSession(), CurrentNodeId,
					AllBypassedTOPNodes.GetData(), BypassedTOPNodeCount), false);

				// ... and remove them from the top node  list
				for (int32 Idx = AllTOPNodeIDs.Num() - 1; Idx >= 0; Idx--)
				{
					if (AllBypassedTOPNodes.Contains(AllTOPNodeIDs[Idx]))
						AllTOPNodeIDs.RemoveAt(Idx);
				}
			}
		}

		// TODO:
		// Apply the show and output filter on that node
		bool bShow = true;

		// Get the node path
		FString CurrentNodePath;
		FHoudiniEngineUtils::HapiGetNodePath(CurrentNodeInfo.id, PDGAssetLink->AssetID, CurrentNodePath);
		
		// Get the node name
		FString CurrentNodeName;
		FHoudiniEngineString::ToFString(CurrentNodeInfo.nameSH, CurrentNodeName);

		UTOPNetwork* CurrentTOPNetwork = nullptr;
		int32 FoundTOPNetIndex = INDEX_NONE;	
		UTOPNetwork* FoundTOPNet = PDGAssetLink->GetTOPNetworkByNodePath(CurrentNodeName, PDGAssetLink->AllTOPNetworks, FoundTOPNetIndex);
		if (IsValid(FoundTOPNet))
		{
			// Reuse the existing corresponding TOP NET
			CurrentTOPNetwork = FoundTOPNet;
			PDGAssetLink->AllTOPNetworks.RemoveAt(FoundTOPNetIndex);
		}
		else
		{
			// Create a new instance for the TOP NET
			CurrentTOPNetwork = NewObject<UTOPNetwork>(PDGAssetLink, UTOPNetwork::StaticClass(), NAME_None, RF_Transactional);
		}

		// Update the TOP NET
		CurrentTOPNetwork->NodeId = CurrentNodeId;
		CurrentTOPNetwork->NodeName = CurrentNodeName;
		CurrentTOPNetwork->NodePath = CurrentNodePath;
		CurrentTOPNetwork->ParentName = PDGAssetLink->AssetName;
		CurrentTOPNetwork->bShowResults = bShow;
		
		// Only add network that have valid TOP Nodes
		if (PopulateTOPNodes(AllTOPNodeIDs, CurrentTOPNetwork, PDGAssetLink, bInZeroWorkItemTallys))
		{
			// See if we need to select a new TOP node
			bool bReselectValidTOP = false;
			if (CurrentTOPNetwork->SelectedTOPIndex < 0)
				bReselectValidTOP = true;
			else if (!CurrentTOPNetwork->AllTOPNodes.IsValidIndex(CurrentTOPNetwork->SelectedTOPIndex))
				bReselectValidTOP = true;
			else if (!IsValid(CurrentTOPNetwork->AllTOPNodes[CurrentTOPNetwork->SelectedTOPIndex]) ||
						CurrentTOPNetwork->AllTOPNodes[CurrentTOPNetwork->SelectedTOPIndex]->bHidden)
				bReselectValidTOP = true;

			if (bReselectValidTOP)
			{
				// Select the first valid TOP node (not hidden)
				for (int Idx = 0; Idx < CurrentTOPNetwork->AllTOPNodes.Num(); Idx++)
				{
					UTOPNode *TOPNode = CurrentTOPNetwork->AllTOPNodes[Idx];
					if (!IsValid(TOPNode) || TOPNode->bHidden)
						continue;

					CurrentTOPNetwork->SelectedTOPIndex = Idx;
					break;
				}
			}

			AllTOPNetworks.Add(CurrentTOPNetwork);
		}
	}

	// Clear previous TOP networks, nodes and generated data
	for (UTOPNetwork* CurrentTOPNet : PDGAssetLink->AllTOPNetworks)
	{
		if (!IsValid(CurrentTOPNet))
			continue;
		
		PDGAssetLink->ClearTOPNetworkWorkItemResults(CurrentTOPNet);
	}
	//PDGAssetLink->ClearAllTOPData();
	PDGAssetLink->AllTOPNetworks = AllTOPNetworks;

	return (AllTOPNetworks.Num() > 0);
}


bool
FHoudiniPDGManager::PopulateTOPNodes(
	const TArray<HAPI_NodeId>& InTopNodeIDs, UTOPNetwork* InTOPNetwork, UHoudiniPDGAssetLink* InPDGAssetLink, bool bInZeroWorkItemTallys)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InTOPNetwork))
		return false;

	// 
	int32 TOPNodeCount = 0;

	// Holds list of found TOP nodes
	TArray<UTOPNode*> AllTOPNodes;
	for(const HAPI_NodeId& CurrentTOPNodeID : InTopNodeIDs)
	{
		HAPI_NodeInfo CurrentNodeInfo;
		FHoudiniApi::NodeInfo_Init(&CurrentNodeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), CurrentTOPNodeID, &CurrentNodeInfo))
		{
			continue;
		}

		// Increase the number of valid TOP Node
		// (before applying the node filter)
		TOPNodeCount++;

		// Get the node path
		FString NodePath;
		FHoudiniEngineUtils::HapiGetNodePath(CurrentNodeInfo.id, InTOPNetwork->NodeId, NodePath);

		// Get the node name
		FString NodeName;
		FHoudiniEngineString::ToFString(CurrentNodeInfo.nameSH, NodeName);

		// See if we can find an existing version of this TOPNOde
		UTOPNode* CurrentTOPNode = nullptr;
		int32 FoundNodeIndex = INDEX_NONE;
		UTOPNode* FoundNode = InPDGAssetLink->GetTOPNodeByNodePath(NodePath, InTOPNetwork->AllTOPNodes, FoundNodeIndex);
		if (IsValid(FoundNode))
		{
			CurrentTOPNode = FoundNode;
			InTOPNetwork->AllTOPNodes.RemoveAt(FoundNodeIndex);

			if (bInZeroWorkItemTallys)
			{
				CurrentTOPNode->ZeroWorkItemTally();
				CurrentTOPNode->NodeState = EPDGNodeState::None;
			}
		}
		else
		{
			// Didn't find an existing UTOPNode for this node, create a new UTOPNode instance
			CurrentTOPNode = NewObject<UTOPNode>(InPDGAssetLink, UTOPNode::StaticClass(), NAME_None, RF_Transactional);
		}

		CurrentTOPNode->NodeId = CurrentTOPNodeID;
		CurrentTOPNode->NodeName = NodeName;
		CurrentTOPNode->NodePath = NodePath;
		CurrentTOPNode->ParentName = InTOPNetwork->ParentName + TEXT("_") + InTOPNetwork->NodeName;
		CurrentTOPNode->bHasChildNodes = CurrentNodeInfo.childNodeCount > 0;

		// Filter display/autoload using name
		CurrentTOPNode->bHidden = false;
		if (InPDGAssetLink->bUseTOPNodeFilter && !InPDGAssetLink->TOPNodeFilter.IsEmpty())
		{
			// Only display nodes that matches the filter
			if (!NodeName.StartsWith(InPDGAssetLink->TOPNodeFilter))
				CurrentTOPNode->bHidden = true;
		}

		// Automatically load results for nodes that match the filter
		if (InPDGAssetLink->bUseTOPOutputFilter)
		{
			bool bAutoLoad = false;
			if (InPDGAssetLink->TOPOutputFilter.IsEmpty())
				bAutoLoad = true;
			else if (NodeName.StartsWith(InPDGAssetLink->TOPOutputFilter))
				bAutoLoad = true;

			CurrentTOPNode->bAutoLoad = bAutoLoad;

			// Show autoloaded results by default
			CurrentTOPNode->SetVisibleInLevel(bAutoLoad);
		}

		AllTOPNodes.Add(CurrentTOPNode);
	}

	for (UTOPNode* CurTOPNode : InTOPNetwork->AllTOPNodes)
	{
		if (!IsValid(CurTOPNode))
			continue;
		
		UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(CurTOPNode);
	}

	InTOPNetwork->AllTOPNodes = AllTOPNodes;

	return (TOPNodeCount > 0);
}


void 
FHoudiniPDGManager::DirtyTOPNode(UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return;
	
	// Dirty the specified TOP node...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DirtyPDGNode(
		FHoudiniEngine::Get().GetSession(), InTOPNode->NodeId, true))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Dirty TOP Node - Failed to dirty %s!"), *(InTOPNode->NodeName));
	}
	
	// ... and clear its work item results.
	UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(InTOPNode);
}

// void
// FHoudiniPDGManager::DirtyAllTasksOfTOPNode(FTOPNode& InTOPNode)
// {
// 	// Dirty the specified TOP node...
// 	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DirtyPDGNode(
// 		FHoudiniEngine::Get().GetSession(), InTOPNode.NodeId, true))
// 	{
// 		HOUDINI_LOG_ERROR(TEXT("PDG Dirty TOP Node - Failed to dirty %s!"), *InTOPNode.NodeName);
// 	}
// }

void
FHoudiniPDGManager::CookTOPNode(UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return;
		
	if (!FHoudiniEngine::Get().GetSession())
		return;

	if (InTOPNode->NodeState == EPDGNodeState::Cooking || InTOPNode->AnyWorkItemsPending())
	{
		HOUDINI_LOG_WARNING(TEXT("PDG Cook TOP Node - %s is already/still cooking, ignoring 'Cook TOP Node' request."), *(InTOPNode->NodePath));
		return;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::CookPDG(
		FHoudiniEngine::Get().GetSession(), InTOPNode->NodeId, 0, 0))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cook TOP Node - Failed to cook %s!"), *(InTOPNode->NodeName));
	}
}


void
FHoudiniPDGManager::DirtyAll(UTOPNetwork* InTOPNet)
{
	if (!IsValid(InTOPNet))
		return;
	
	// Dirty the specified TOP network...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DirtyPDGNode(
		FHoudiniEngine::Get().GetSession(), InTOPNet->NodeId, true))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Dirty All - Failed to dirty all of %s's TOP nodes!"), *(InTOPNet->NodeName));
		return;
	}

	// ... and clear its work item results.
	UHoudiniPDGAssetLink::ClearTOPNetworkWorkItemResults(InTOPNet);
}


void
FHoudiniPDGManager::CookOutput(UTOPNetwork* InTOPNet)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniPDGManager::CookOutput);

	// Cook the output TOP node of the currently selected TOP network.
	//WorkItemTally.ZeroAll();
	//UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(InTOPNet);

	if (!IsValid(InTOPNet))
		return;
	
	if (!FHoudiniEngine::Get().GetSession())
		return;

	bool bAlreadyCooking = InTOPNet->AnyWorkItemsPending();

	if (!bAlreadyCooking)
	{
		HAPI_PDG_GraphContextId GraphContextId = -1;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContextId(
            FHoudiniEngine::Get().GetSession(), InTOPNet->NodeId, &GraphContextId))
		{
			HOUDINI_LOG_ERROR(TEXT("PDG Cook Output - Failed to get %s's graph context ID!"), *(InTOPNet->NodeName));
			return;
		}

		int32 PDGState = -1;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGState(
            FHoudiniEngine::Get().GetSession(), GraphContextId, &PDGState))
		{
			HOUDINI_LOG_ERROR(TEXT("PDG Cook Output - Failed to get %s's PDG state."), *(InTOPNet->NodeName));
			return;
		}
		bAlreadyCooking = ((HAPI_PDG_State) PDGState == HAPI_PDG_STATE_COOKING);
	}

	if (bAlreadyCooking)
	{
		HOUDINI_LOG_WARNING(TEXT("PDG Cook Output - %s is already/still cooking, ignoring 'Cook Output' request."), *(InTOPNet->NodeName));
		return;
	}

	// TODO: ???
	// Cancel all cooks. This is required as otherwise the graph gets into an infinite cook state (bug?)
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::CookPDGAllOutputs(
		FHoudiniEngine::Get().GetSession(), InTOPNet->NodeId, 0, 0))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cook Output - Failed to cook %s's output!"), *(InTOPNet->NodeName));
	}
}


void 
FHoudiniPDGManager::PauseCook(UTOPNetwork* InTOPNet)
{
	// Pause the PDG cook of the currently selected TOP network
	//WorkItemTally.ZeroAll();
	//UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(InTOPNet);

	if (!IsValid(InTOPNet))
		return;

	if (!FHoudiniEngine::Get().GetSession())
		return;

	HAPI_PDG_GraphContextId GraphContextId = -1;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContextId(
		FHoudiniEngine::Get().GetSession(), InTOPNet->NodeId, &GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Pause Cook - Failed to get %s's graph context ID!"), *(InTOPNet->NodeName));
		return;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::PausePDGCook(
		FHoudiniEngine::Get().GetSession(), GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Pause Cook - Failed to pause %s!"), *(InTOPNet->NodeName));
		return;
	}
}


void
FHoudiniPDGManager::CancelCook(UTOPNetwork* InTOPNet)
{
	// Cancel the PDG cook of the currently selected TOP network
	//WorkItemTally.ZeroAll();
	//UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(InTOPNet);

	if (!IsValid(InTOPNet))
		return;

	if (!FHoudiniEngine::Get().GetSession())
		return;

	HAPI_PDG_GraphContextId GraphContextId = -1;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContextId(
		FHoudiniEngine::Get().GetSession(), InTOPNet->NodeId, &GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cancel Cook - Failed to get %s's graph context ID!"), *(InTOPNet->NodeName));
		return;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::CancelPDGCook(
		FHoudiniEngine::Get().GetSession(), GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cancel Cook - Failed to cancel cook for %s!"), *(InTOPNet->NodeName));
		return;
	}
}

void
FHoudiniPDGManager::Update()
{
	// Clean up registered PDG Asset Links
	for(int32 Idx = PDGAssetLinks.Num() - 1; Idx >= 0; Idx--)
	{
		TWeakObjectPtr<UHoudiniPDGAssetLink> Ptr = PDGAssetLinks[Idx];
		if (!Ptr.IsValid() || Ptr.IsStale())
		{
			PDGAssetLinks.RemoveAt(Idx);
			continue;
		}

		UHoudiniPDGAssetLink* CurPDGAssetLink = PDGAssetLinks[Idx].Get();
		if (!IsValid(CurPDGAssetLink))
		{
			PDGAssetLinks.RemoveAt(Idx);
			continue;
		}
	}

	// Do nothing if we dont have any valid PDG asset Link
	if (PDGAssetLinks.Num() <= 0)
		return;

	// Update the PDG contexts and handle all pdg events and work item status updates
	UpdatePDGContexts();

	// Prcoess any workitem result if we have any
	ProcessWorkItemResults();
}

// Query all the PDG graph context in the current Houdini Engine session.
// Handle PDG events, work item status updates.
// Forward relevant events to PDGAssetLink objects.
void
FHoudiniPDGManager::UpdatePDGContexts()
{
	// Get current PDG graph contexts
	ReinitializePDGContext();

	// Process next set of events for each graph context
	if (PDGContextIDs.Num() > 0)
	{
		// Only initialize event array if not valid, or user resized max size
		if(PDGEventInfos.Num() != MaxNumberOfPDGEvents)
			PDGEventInfos.SetNum(MaxNumberOfPDGEvents);

		// TODO: member?
		//HAPI_PDG_State PDGState;
		for(const HAPI_PDG_GraphContextId& CurrentContextID : PDGContextIDs)
		{
			/*
			// TODO: No need to reset events at each tick
			int32 PDGStateInt;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGState(
			FHoudiniEngine::Get().GetSession(), CurrentContextID, &PDGStateInt))
			{
			HOUDINI_LOG_ERROR(TEXT("Failed to get PDG state"));
			continue;
			}

			PDGState = (HAPI_PDG_State)PDGStateInt;
			
			for (int32 Idx = 0; Idx < PDGEventInfos.Num(); Idx++)
			{
			ResetPDGEventInfo(PDGEventInfos[Idx]);
			}
			*/

			int32 PDGEventCount = 0;
			int32 RemainingPDGEventCount = 0;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGEvents(
				FHoudiniEngine::Get().GetSession(), CurrentContextID, PDGEventInfos.GetData(),
				MaxNumberOfPDGEvents, &PDGEventCount, &RemainingPDGEventCount))
			{
				HOUDINI_LOG_ERROR(TEXT("Failed to get PDG events"));
				continue;
			}

			if (PDGEventCount < 1)
				continue;
			
			for (int32 EventIdx = 0; EventIdx < PDGEventCount; EventIdx++)
			{
				ProcessPDGEvent(CurrentContextID, PDGEventInfos[EventIdx]);
			}

			HOUDINI_LOG_MESSAGE(TEXT("PDG: Tick processed %d events, %d remaining."), PDGEventCount, RemainingPDGEventCount);
		}
	}

	// Refresh UI if necessary
	for (auto CurAssetLink : PDGAssetLinks)
	{
		UHoudiniPDGAssetLink* AssetLink = CurAssetLink.Get();
		if (AssetLink)
		{
			if (AssetLink->bNeedsUIRefresh)
			{
				FHoudiniPDGManager::RefreshPDGAssetLinkUI(AssetLink);
				AssetLink->bNeedsUIRefresh = false;
			}
			else
			{
				AssetLink->UpdateWorkItemTally();
			}
		}
	}
}

// Query the currently active PDG graph contexts in the Houdini Engine session.
// Should be done each time to get latest set of graph contexts.
void
FHoudiniPDGManager::ReinitializePDGContext()
{
	int32 NumContexts = 0;

	PDGContextNames.SetNum(MaxNumberOPDGContexts);
	PDGContextIDs.SetNum(MaxNumberOPDGContexts);
	
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContexts(
		FHoudiniEngine::Get().GetSession(),
		&NumContexts, PDGContextNames.GetData(), PDGContextIDs.GetData(), MaxNumberOPDGContexts) || NumContexts <= 0)
	{
		PDGContextNames.SetNum(0);
		PDGContextIDs.SetNum(0);
		return;
	}

	if(PDGContextIDs.Num() != NumContexts)
		PDGContextIDs.SetNum(NumContexts);

	if (PDGContextNames.Num() != NumContexts)
		PDGContextNames.SetNum(NumContexts);
}

// Process a PDG event. Notify the relevant PDGAssetLink object.
void
FHoudiniPDGManager::ProcessPDGEvent(const HAPI_PDG_GraphContextId& InContextID, HAPI_PDG_EventInfo& EventInfo)
{
	UHoudiniPDGAssetLink* PDGAssetLink = nullptr;
	UTOPNetwork* TOPNetwork = nullptr;
	UTOPNode* TOPNode = nullptr;

	HAPI_PDG_EventType EventType = (HAPI_PDG_EventType)EventInfo.eventType;
	HAPI_PDG_WorkitemState CurrentWorkItemState = (HAPI_PDG_WorkitemState)EventInfo.currentState;
	HAPI_PDG_WorkitemState LastWorkItemState = (HAPI_PDG_WorkitemState)EventInfo.lastState;

	// Debug: get the HAPI_PDG_EventType as a string
	const FString EventName = FHoudiniEngineUtils::HapiGetEventTypeAsString(EventType);
	const FString CurrentWorkitemStateName = FHoudiniEngineUtils::HapiGetWorkitemStateAsString(CurrentWorkItemState);
	const FString LastWorkitemStateName = FHoudiniEngineUtils::HapiGetWorkitemStateAsString(LastWorkItemState);

	if(!GetTOPAssetLinkNetworkAndNode(EventInfo.nodeId, PDGAssetLink, TOPNetwork, TOPNode)
		|| !IsValid(PDGAssetLink) || !IsValid(TOPNetwork) || !IsValid(TOPNode) || !IsValid(TOPNode))
	{		
		HOUDINI_LOG_WARNING(TEXT("[ProcessPDGEvent]: Could not find matching TOPNode for event %s, workitem id %d, node id %d"), *EventName, EventInfo.workitemId, EventInfo.nodeId);
		return;
	}

	HOUDINI_PDG_MESSAGE(
		TEXT("[ProcessPDGEvent]: TOPNode: %s, WorkItem ID: %d, Event Type: %s, Current State: %s, Last State %s"),
		*(TOPNode->NodePath), EventInfo.workitemId, *EventName, *CurrentWorkitemStateName, *LastWorkitemStateName);
	
	FLinearColor MsgColor = FLinearColor::White;

	bool bUpdatePDGNodeState = false;
	switch (EventType)
	{	
		case HAPI_PDG_EVENT_NULL:
			SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::None);
			break;

		case HAPI_PDG_EVENT_NODE_CLEAR:
			NotifyTOPNodePDGStateClear(PDGAssetLink, TOPNode);
			break;

		case HAPI_PDG_EVENT_WORKITEM_ADD:
			CreateOrRelinkWorkItem(TOPNode, InContextID, EventInfo.workitemId);
			bUpdatePDGNodeState = true;
			NotifyTOPNodeCreatedWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
			break;

		case HAPI_PDG_EVENT_WORKITEM_REMOVE:
			RemoveWorkItem(PDGAssetLink, EventInfo.workitemId, TOPNode);
			bUpdatePDGNodeState = true;
			NotifyTOPNodeRemovedWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
			break;

		case HAPI_PDG_EVENT_COOK_WARNING:
			MsgColor = FLinearColor::Yellow;
			break;

		case HAPI_PDG_EVENT_COOK_ERROR:
			MsgColor = FLinearColor::Red;
			break;

		case HAPI_PDG_EVENT_COOK_COMPLETE:
			SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::Cook_Complete);
			TOPNode->HandleOnPDGEventCookComplete();
			TOPNetwork->HandleOnPDGEventCookCompleteReceivedByChildNode(PDGAssetLink, TOPNode);
			break;

		case HAPI_PDG_EVENT_DIRTY_START:
			SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::Dirtying);
			break;

		case HAPI_PDG_EVENT_DIRTY_STOP:
			SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::Dirtied);
			break;

		case HAPI_PDG_EVENT_WORKITEM_STATE_CHANGE:
		{
			// Last states
			bUpdatePDGNodeState = true;
			if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_WAITING && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_WAITING)
			{
			}
			else if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKING && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKING)
			{
			}
			else if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_SCHEDULED && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_SCHEDULED)
			{
			}
			else if ( (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CACHE || LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_SUCCESS)
					&& CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CACHE &&  CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_SUCCESS)
			{
				// Handled previously cooked WI
			}
			else if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_FAIL && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_FAIL)
			{
			}
			else
			{
				// TODO: 
				// unhandled state change
				HOUDINI_PDG_WARNING(TEXT("Unhandled PDG state change! Node: %s, WorkItemID %d"), IsValid(TOPNode) ? *TOPNode->NodePath : TEXT(""), EventInfo.workitemId);
			}

			if (LastWorkItemState == CurrentWorkItemState)
			{
				// TODO: 
				// Not a change!! shouldnt happen!
				HOUDINI_PDG_WARNING(TEXT("Last state == current state! Node: %s, WorkItemID %d, state %d"), IsValid(TOPNode) ? *TOPNode->NodePath : TEXT(""), EventInfo.workitemId, EventInfo.lastState);
			}

			// New states
			if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_WAITING)
			{
				NotifyTOPNodeWaitingWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_UNCOOKED)
			{

			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_DIRTY)
			{
				// ClearWorkItemResult(InContextID, EventInfo, *TOPNode);
				ClearWorkItemResult(PDGAssetLink, EventInfo.workitemId, TOPNode);
				// RemoveWorkItem(PDGAssetLink, EventInfo.workitemId, *TOPNode);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_SCHEDULED)
			{
				NotifyTOPNodeScheduledWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKING)
			{
				NotifyTOPNodeCookingWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_SUCCESS 
				|| CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CACHE)
			{
				NotifyTOPNodeCookedWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);

				// On cook success, handle results
				CreateOrRelinkWorkItemResult(TOPNode, InContextID, EventInfo.workitemId, TOPNode->bAutoLoad);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_FAIL)
			{
				// TODO: on cook failure, get log path?
				NotifyTOPNodeErrorWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
				MsgColor = FLinearColor::Red;
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CANCEL)
			{
				NotifyTOPNodeCookCancelledWorkItem(PDGAssetLink, TOPNode, EventInfo.workitemId);
			}
		}
		break;

		case HAPI_PDG_EVENT_COOK_START:
			TOPNode->HandleOnPDGEventCookStart();
			break;
		// Unhandled events
		case HAPI_PDG_EVENT_DIRTY_ALL:
		case HAPI_PDG_EVENT_WORKITEM_ADD_DEP:
		case HAPI_PDG_EVENT_WORKITEM_REMOVE_DEP:
		case HAPI_PDG_EVENT_WORKITEM_ADD_PARENT:
		case HAPI_PDG_EVENT_WORKITEM_REMOVE_PARENT:
		case HAPI_PDG_EVENT_UI_SELECT:
		case HAPI_PDG_EVENT_NODE_CREATE:
		case HAPI_PDG_EVENT_NODE_REMOVE:
		case HAPI_PDG_EVENT_NODE_RENAME:
		case HAPI_PDG_EVENT_NODE_CONNECT:
		case HAPI_PDG_EVENT_NODE_DISCONNECT:
		case HAPI_PDG_EVENT_WORKITEM_SET_INT:					// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_SET_FLOAT:					// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_SET_STRING:				// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_SET_FILE:					// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_SET_PYOBJECT:				// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_SET_GEOMETRY:				// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_RESULT:
		case HAPI_PDG_EVENT_WORKITEM_PRIORITY:					// DEPRECATED 
		case HAPI_PDG_EVENT_WORKITEM_ADD_STATIC_ANCESTOR:
		case HAPI_PDG_EVENT_WORKITEM_REMOVE_STATIC_ANCESTOR:
		case HAPI_PDG_EVENT_NODE_PROGRESS_UPDATE:
		case HAPI_PDG_EVENT_ALL:
		case HAPI_PDG_EVENT_LOG:
		case HAPI_PDG_CONTEXT_EVENTS:
			break;
	}

	if (bUpdatePDGNodeState)
	{
		// Work item events
		EPDGNodeState CurrentTOPNodeState = TOPNode->NodeState;
		if (CurrentTOPNodeState == EPDGNodeState::Cooking)
		{
			if (TOPNode->AreAllWorkItemsComplete())
			{
				// At the end of a node/net cook, ensure that the work items are in sync with HAPI and remove any
				// work items with invalid ids or that don't exist on the HAPI side anymore.
				SyncAndPruneWorkItems(TOPNode);
				// Check that all work items are still complete after the sync
				if (TOPNode->AreAllWorkItemsComplete())
				{
					if (TOPNode->AnyWorkItemsFailed())
					{
						SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::Cook_Failed);
					}
					else
					{
						SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::Cook_Complete);
					}
				}
			}
		}
		else if (TOPNode->AnyWorkItemsPending())
		{
			SetTOPNodePDGState(PDGAssetLink, TOPNode, EPDGNodeState::Cooking);
		}
	}

	if (EventInfo.msgSH >= 0)
	{
		FString EventMsg;
		FHoudiniEngineString::ToFString(EventInfo.msgSH, EventMsg);
		if (!EventMsg.IsEmpty())
		{
			// TODO: Event MSG?
			// Somehow update the PDG event msg UI ??
			// Simply log for now...
			if (MsgColor == FLinearColor::Red)
			{
				HOUDINI_LOG_ERROR(TEXT("%s"), *EventMsg);
			}
			else if (MsgColor == FLinearColor::Yellow)
			{
				HOUDINI_LOG_WARNING(TEXT("%s"), *EventMsg);
			}
			else
			{
				HOUDINI_LOG_MESSAGE(TEXT("%s"), *EventMsg);
			}
		}
	}
}

void
FHoudiniPDGManager::ResetPDGEventInfo(HAPI_PDG_EventInfo& InEventInfo)
{
	InEventInfo.nodeId = -1;
	InEventInfo.workitemId = -1;
	InEventInfo.dependencyId = -1;
	InEventInfo.currentState = HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_UNDEFINED;
	InEventInfo.lastState = HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_UNDEFINED;
	InEventInfo.eventType = HAPI_PDG_EventType::HAPI_PDG_EVENT_NULL;
}


bool
FHoudiniPDGManager::GetTOPAssetLinkNetworkAndNode(
	const HAPI_NodeId& InNodeID, UHoudiniPDGAssetLink*& OutAssetLink, UTOPNetwork*& OutTOPNetwork, UTOPNode*& OutTOPNode)
{	
	// Returns the PDGAssetLink and FTOPNode data associated with this TOP node ID
	OutAssetLink = nullptr;
	OutTOPNetwork = nullptr;
	OutTOPNode = nullptr;
	for (TWeakObjectPtr<UHoudiniPDGAssetLink>& CurAssetLinkPtr : PDGAssetLinks)
	{
		if (!CurAssetLinkPtr.IsValid() || CurAssetLinkPtr.IsStale())
			continue;

		UHoudiniPDGAssetLink* CurAssetLink = CurAssetLinkPtr.Get();
		if (!IsValid(CurAssetLink))
			continue;

		if (CurAssetLink->GetTOPNodeAndNetworkByNodeId((int32)InNodeID, OutTOPNetwork, OutTOPNode))
		{
			if (OutTOPNetwork != nullptr && OutTOPNode != nullptr)
			{
				OutAssetLink = CurAssetLink;
				return true;
			}
		}
	}

	OutAssetLink = nullptr;
	OutTOPNetwork = nullptr;
	OutTOPNode = nullptr;

	return false;
}

void
FHoudiniPDGManager::SetTOPNodePDGState(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const EPDGNodeState& InPDGState)
{
	if (!IsValid(InTOPNode))
		return;
	
	InTOPNode->NodeState = InPDGState;

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodePDGStateClear(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return;

	//Debug.LogFormat("NotifyTOPNodePDGStateClear:: {0}", topNode._nodeName);
	InTOPNode->NodeState = EPDGNodeState::None;
	InTOPNode->ZeroWorkItemTally();
	InTOPNode->OnDirtyNode();

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally ZeroAll"), *(InTOPNode->NodePath));

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
	
}

void
FHoudiniPDGManager::NotifyTOPNodeCreatedWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;
	
	InTOPNode->OnWorkItemCreated(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally Created WorkItem, Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeRemovedWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;
	
	InTOPNode->OnWorkItemRemoved(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally Removed WorkItem, Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeCookedWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;

	InTOPNode->OnWorkItemCooked(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally CookedWorkItems Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumCookedWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeErrorWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;

	InTOPNode->OnWorkItemErrored(InWorkItemID);
	
	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally ErroredWorkItems Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumErroredWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeWaitingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;

	InTOPNode->OnWorkItemWaiting(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally WaitingWorkItems Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumErroredWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeScheduledWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;

	InTOPNode->OnWorkItemScheduled(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally ScheduledWorkItems Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumScheduledWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeCookingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;

	InTOPNode->OnWorkItemCooking(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally CookingWorkItems Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumCookingWorkItems());

	// InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeCookCancelledWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& InWorkItemID)
{
	if (!IsValid(InTOPNode))
		return;

	InTOPNode->OnWorkItemCookCancelled(InWorkItemID);

	HOUDINI_PDG_MESSAGE(TEXT("PDG: %s: WorkItemTally CookCancelledWorkItems Total %d"), *(InTOPNode->NodePath), InTOPNode->GetWorkItemTally().NumCookCancelledWorkItems());
}

void
FHoudiniPDGManager::ClearWorkItemResult(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, UTOPNode* InTOPNode)
{
	if (!IsValid(InAssetLink))
		return;

	// TODO!!!
	// Clear all work items' results for the specified TOP node. 
	// This destroys any loaded results (geometry etc).
	//session.LogErrorOverride = false;
	InAssetLink->ClearWorkItemResultByID(InWorkItemID, InTOPNode);
	// session.LogErrorOverride = true;
}

void
FHoudiniPDGManager::RemoveWorkItem(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, UTOPNode* InTOPNode)
{
	if (!IsValid(InAssetLink))
		return;

	// Clear all of the work item's results for the specified TOP node and also remove the work item itself from
	// the TOP node.
	InAssetLink->DestroyWorkItemByID(InWorkItemID, InTOPNode);
}

void
FHoudiniPDGManager::RefreshPDGAssetLinkUI(UHoudiniPDGAssetLink* InAssetLink)
{
	if (!IsValid(InAssetLink))
		return;

	// Only update the editor properties if the PDG asset link's Actor is selected
	// else, just update the workitemtally
	InAssetLink->UpdateWorkItemTally();

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InAssetLink->GetOuter());
	if (!IsValid(HAC))
		return;
	
	AActor* ActorOwner = HAC->GetOwner();
	if (ActorOwner != nullptr && ActorOwner->IsSelected())
	{
		FHoudiniEngineUtils::UpdateEditorProperties(HAC, true);
	}
}

void
FHoudiniPDGManager::NotifyAssetCooked(UHoudiniPDGAssetLink* InAssetLink, const bool& bSuccess)
{
	if (!IsValid(InAssetLink))
		return;

	if (bSuccess)
	{
		if (InAssetLink->LinkState == EPDGLinkState::Linked)
		{
			if (InAssetLink->bAutoCook)
			{
				FHoudiniPDGManager::CookOutput(InAssetLink->GetSelectedTOPNetwork());
			}
		}
		else
		{
			UpdatePDGAssetLink(InAssetLink);
		}
	}
	else
	{
		InAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
	}
}

int32
FHoudiniPDGManager::CreateOrRelinkWorkItem(
	UTOPNode* InTOPNode,
	const HAPI_PDG_GraphContextId& InContextID,
	HAPI_PDG_WorkitemId InWorkItemID)
{
	if (!IsValid(InTOPNode))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to get work %d info: InTOPNode is null."), InWorkItemID);
		return INDEX_NONE;
	}
	
	HAPI_PDG_WorkitemInfo WorkItemInfo;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetWorkitemInfo(
		FHoudiniEngine::Get().GetSession(), InContextID, InWorkItemID, &WorkItemInfo))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to get work item %d info for %s"), InWorkItemID, *(InTOPNode->NodeName));
		// TODO? continue?
		return INDEX_NONE;
	}

	// Try to find the existing WorkItem by ID.
	int32 Index = InTOPNode->ArrayIndexOfWorkResultByID(InWorkItemID);
	if (Index == INDEX_NONE)
	{
		// Try to find the first entry with WorkItemID == INDEX_NONE. The WorkItemIDs are
		// transient, so not saved when the map / asset link is saved. So when loading a map containing the asset
		// link all the IDs are INDEX_NONE and so we re-use any stale entries in array index order (should be reliable
		// if work items generate in the same order. In the future we might have to consider adding support for a
		// custom ID attribute for more stable re-linking of work items).
		Index = InTOPNode->ArrayIndexOfFirstInvalidWorkResult();
		if (Index == INDEX_NONE)
		{
			// If we couldn't find a stale entry to re-use, create a new one
			FTOPWorkResult LocalWorkResult;
			LocalWorkResult.WorkItemID = InWorkItemID;
			LocalWorkResult.WorkItemIndex = WorkItemInfo.index;
			Index = InTOPNode->WorkResult.Add(LocalWorkResult);
		}
		else
		{
			// We found a stale entry, re-use it
			FTOPWorkResult& ReUsedWorkResult = InTOPNode->WorkResult[Index];
			ReUsedWorkResult.WorkItemID = InWorkItemID;
			ReUsedWorkResult.WorkItemIndex = WorkItemInfo.index;
		}
	}

	return Index;
}

bool
FHoudiniPDGManager::CreateOrRelinkWorkItemResult(
	UTOPNode* InTOPNode,
	const HAPI_PDG_GraphContextId& InContextID,
	HAPI_PDG_WorkitemId InWorkItemID,
	bool bInLoadResultObjects)
{
	if (!IsValid(InTOPNode))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to get work %d info: InTOPNode is null."), InWorkItemID);
		return false;
	}
	
	HAPI_PDG_WorkitemInfo WorkItemInfo;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetWorkitemInfo(
		FHoudiniEngine::Get().GetSession(), InContextID, InWorkItemID, &WorkItemInfo))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to get work item %d info for %s"), InWorkItemID, *(InTOPNode->NodeName));
		// TODO? continue?
		return false;
	}

	// Try to find the existing WorkResult by ID.
	int32 WorkResultArrayIndex = InTOPNode->ArrayIndexOfWorkResultByID(InWorkItemID);
	FTOPWorkResult* WorkResult = nullptr;
	if (WorkResultArrayIndex != INDEX_NONE)
		WorkResult = InTOPNode->GetWorkResultByArrayIndex(WorkResultArrayIndex);
	if (!WorkResult)
	{
		// TODO: This shouldn't really happen, it means a work item finished cooking and generated a result before
		// we received an event that the work item was added/generated.
		WorkResultArrayIndex = CreateOrRelinkWorkItem(InTOPNode, InContextID, InWorkItemID);
		if (WorkResultArrayIndex != INDEX_NONE)
		{
			WorkResult = InTOPNode->GetWorkResultByArrayIndex(WorkResultArrayIndex);
		}
	}

	if (!WorkResult)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to get or add a FTOPWorkResult for WorkItemID %d for %s"), InWorkItemID, *(InTOPNode->NodeName));
		return false;
	}

	TArray<FTOPWorkResultObject> NewResultObjects;
	TSet<int32> ResultIndicesThatWereReused;
	if (WorkItemInfo.numResults > 0)
	{
		TArray<HAPI_PDG_WorkitemResultInfo> ResultInfos;
		ResultInfos.SetNum(WorkItemInfo.numResults);
		const int32 resultCount = WorkItemInfo.numResults;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetWorkitemResultInfo(
            FHoudiniEngine::Get().GetSession(),
            InTOPNode->NodeId, InWorkItemID, ResultInfos.GetData(), resultCount))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to get work item %d result info for %s"), InWorkItemID, *(InTOPNode->NodeName));
			// TODO? continue?
			return false;
		}

		FString WorkItemName;
		FHoudiniEngineString::ToFString(WorkItemInfo.nameSH, WorkItemName);

		// Load each result geometry
		const int32 NumResults = ResultInfos.Num();
		for (int32 Idx = 0; Idx < NumResults; Idx++)
		{
			const HAPI_PDG_WorkitemResultInfo& ResultInfo = ResultInfos[Idx];
			if (ResultInfo.resultTagSH <= 0 || ResultInfo.resultSH <= 0)
				continue;

			FString CurrentTag;
			FHoudiniEngineString::ToFString(ResultInfo.resultTagSH, CurrentTag);
			if(CurrentTag.IsEmpty() || !CurrentTag.StartsWith(TEXT("file")))
				continue;

			FString CurrentPath = FString();
			FHoudiniEngineString::ToFString(ResultInfo.resultSH, CurrentPath);

			// Construct the name and look for an existing work result, re-use it if found, otherwise create a new one
			const FString WorkResultName = FString::Printf(
				TEXT("%s_%s_%d_%d"),
				*(InTOPNode->ParentName),
				*WorkItemName,
				WorkResultArrayIndex,
				Idx);

			// int32 ExistingObjectIndex = WorkResult->ResultObjects.IndexOfByPredicate([WorkResultName](const FTOPWorkResultObject& InResultObject)
			// {
			// 	return InResultObject.Name == WorkResultName;
			// });
			int32 ExistingObjectIndex = WorkResult->ResultObjects.IndexOfByPredicate([Idx](const FTOPWorkResultObject& InResultObject)
			{
				return InResultObject.WorkItemResultInfoIndex == Idx;
			});
			if (WorkResult->ResultObjects.IsValidIndex(ExistingObjectIndex))
			{
				FTOPWorkResultObject& ExistingResultObject = WorkResult->ResultObjects[ExistingObjectIndex];

				ExistingResultObject.Name = WorkResultName;
				ExistingResultObject.FilePath = CurrentPath;
				ExistingResultObject.SetAutoBakedSinceLastLoad(false);
				if (ExistingResultObject.State == EPDGWorkResultState::Loaded && !bInLoadResultObjects)
				{
					ExistingResultObject.State = EPDGWorkResultState::ToDelete;
				}
				else
				{
					// When the commandlet is not being used, we could leave the outputs in place and have
					// translators try to re-use objects/components. When the commandlet is being used, the packages
					// are always saved and standalone, so if we want to automatically clean up old results then we
					// need to destroy the existing outputs
					if (BGEOCommandletStatus == EHoudiniBGEOCommandletStatus::Connected)
					{
						constexpr bool bDeleteOutputActors = false;
						InTOPNode->DeleteWorkResultObjectOutputs(WorkResultArrayIndex, ExistingObjectIndex, bDeleteOutputActors);
					}
					
					if ((ExistingResultObject.State == EPDGWorkResultState::Loaded ||
						 ExistingResultObject.State ==  EPDGWorkResultState::ToDelete ||
						 ExistingResultObject.State == EPDGWorkResultState::Deleting) && bInLoadResultObjects)
					{
						ExistingResultObject.State = EPDGWorkResultState::ToLoad;
					}
					else
					{
						ExistingResultObject.State = bInLoadResultObjects ? EPDGWorkResultState::ToLoad : EPDGWorkResultState::NotLoaded;
					}
				}

				NewResultObjects.Add(ExistingResultObject);
				ResultIndicesThatWereReused.Add(ExistingObjectIndex);
			}
			else
			{
				FTOPWorkResultObject ResultObj;
				ResultObj.Name = WorkResultName;
				ResultObj.FilePath = CurrentPath;
				ResultObj.State = bInLoadResultObjects ? EPDGWorkResultState::ToLoad : EPDGWorkResultState::NotLoaded;
				ResultObj.WorkItemResultInfoIndex = Idx;
				ResultObj.SetAutoBakedSinceLastLoad(false);

				NewResultObjects.Add(ResultObj);
			}
		}
	}
	// Destroy any old ResultObjects that were not re-used
	const int32 NumPrevResultObjects = WorkResult->ResultObjects.Num();
	for (int32 ResultObjectIndex = 0; ResultObjectIndex < NumPrevResultObjects; ++ResultObjectIndex)
	{
		if (ResultIndicesThatWereReused.Contains(ResultObjectIndex))
			continue;
		InTOPNode->DeleteWorkResultObjectOutputs(WorkResultArrayIndex, ResultObjectIndex);
	}
	WorkResult->ResultObjects = NewResultObjects;

	return true;
}

int32
FHoudiniPDGManager::SyncAndPruneWorkItems(UTOPNode* InTOPNode)
{
	TSet<int32> WorkItemIDSet;
	TArray<HAPI_PDG_WorkitemId> WorkItemIDs;
	int NumWorkItems = -1;

	if (!IsValid(InTOPNode))
		return -1;
	
	HAPI_Session const * const HAPISession = FHoudiniEngine::Get().GetSession();
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNumWorkitems(HAPISession, InTOPNode->NodeId, &NumWorkItems))
	{
		HOUDINI_LOG_WARNING(TEXT("GetNumWorkitems call failed on TOP Node %s (%d)"), *(InTOPNode->NodeName), InTOPNode->NodeId);
		return -1;
	}
	
	WorkItemIDs.SetNum(NumWorkItems);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetWorkitems(HAPISession, InTOPNode->NodeId, WorkItemIDs.GetData(), NumWorkItems))
	{
		HOUDINI_LOG_WARNING(TEXT("GetWorkitems call failed on TOP Node %s (%d)"), *(InTOPNode->NodeName), InTOPNode->NodeId);
		return -1;
	}

	HAPI_PDG_GraphContextId ContextId;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContextId(HAPISession, InTOPNode->NodeId, &ContextId))
	{
		HOUDINI_LOG_WARNING(TEXT("GetPDGGraphContextId call failed on TOP Node %s (%d)"), *(InTOPNode->NodeName), InTOPNode->NodeId);
		return -1;
	}
	
	for (const HAPI_PDG_WorkitemId& WorkItemID : WorkItemIDs)
	{
		WorkItemIDSet.Add(static_cast<int32>(WorkItemID));

		// If the WorkItemID is not present in FTOPWorkResult array, sync from HAPI
		if (InTOPNode->GetWorkResultByID(WorkItemID) == nullptr)
		{
			CreateOrRelinkWorkItemResult(InTOPNode, ContextId, WorkItemID);
			InTOPNode->OnWorkItemCreated(WorkItemID);
		}
	}

	// TODO: refactor functions that access the TOPNode's array and properties directly to rather be functions on the
	// UTOPNode and make access to these arrays protected/private

	// Remove any work result entries with invalid IDs or where the WorkItemID is not in the set of ids returned by
	// HAPI (only if we could get the IDs from HAPI).
	const FGuid HoudiniComponentGuid(InTOPNode->GetHoudiniComponentGuid());
	int32 NumRemoved = 0;
	const int32 NumWorkItemsInArray = InTOPNode->WorkResult.Num();
	for (int32 Index = NumWorkItemsInArray - 1; Index >= 0; --Index)
	{
		FTOPWorkResult& WorkResult = InTOPNode->WorkResult[Index];
		if (WorkResult.WorkItemID == INDEX_NONE || !WorkItemIDSet.Contains(WorkResult.WorkItemID))
		{
			HOUDINI_PDG_WARNING(
				TEXT("Pruning a FTOPWorkResult entry from TOP Node %d, WorkItemID %d, WorkItemIndex %d, Array Index %d"),
				InTOPNode->NodeId, WorkResult.WorkItemID, WorkResult.WorkItemIndex, Index);
			WorkResult.ClearAndDestroyResultObjects(HoudiniComponentGuid);
			InTOPNode->WorkResult.RemoveAt(Index);
			InTOPNode->OnWorkItemRemoved(WorkResult.WorkItemID);
			NumRemoved++;
		}
	}

	return NumRemoved;
}

void
FHoudiniPDGManager::ProcessWorkItemResults()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniPDGManager::ProcessWorkItemResults);

	const EHoudiniBGEOCommandletStatus CommandletStatus = UpdateAndGetBGEOCommandletStatus();
	for (auto& CurrentPDGAssetLink : PDGAssetLinks)
	{
		// Iterate through all PDG Asset Link
		UHoudiniPDGAssetLink* AssetLink = CurrentPDGAssetLink.Get();
		if (!AssetLink)
			continue;

		// Set up package parameters to:
		// Cook to temp houdini engine directory
		// and if the PDG asset link is associated with a Houdini Asset Component (HAC):
		//		set the outer package to the HAC
		//		set the HoudiniAssetName according to the HAC
		//		set the ComponentGUID according to the HAC
		// otherwise we set the outer to the asset link's parent and leave naming and GUID blank
		FHoudiniPackageParams PackageParams;
		PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
		PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

		PackageParams.BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
		PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

		// AActor* ParentActor = nullptr;
		UObject* AssetLinkParent = AssetLink->GetOuter();
		UHoudiniAssetComponent* HAC = AssetLinkParent != nullptr ? Cast<UHoudiniAssetComponent>(AssetLinkParent) : nullptr;
		if (HAC)
		{
			PackageParams.OuterPackage = HAC->GetComponentLevel();
			PackageParams.HoudiniAssetName = HAC->GetHoudiniAsset() ? HAC->GetHoudiniAsset()->GetName() : FString();
			PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetName();
			PackageParams.ComponentGUID = HAC->GetComponentGUID();

			// ParentActor = HAC->GetOwner();
		}
		else
		{
			PackageParams.OuterPackage = AssetLinkParent ? AssetLinkParent->GetOutermost() : nullptr;
			PackageParams.HoudiniAssetName = FString();
			PackageParams.HoudiniAssetActorName = FString();
			// PackageParams.ComponentGUID = HAC->GetComponentGUID();

			// // Try to find a parent actor
			// UObject* Parent = AssetLinkParent;
			// while (Parent && !ParentActor)
			// {
			// 	ParentActor = Cast<AActor>(Parent);
			// 	if (!ParentActor)
			// 		Parent = ParentActor->GetOuter();
			// }
		}
		PackageParams.ObjectName = FString();

		// Static mesh generation / build settings, get it from the HAC if available, otherwise from the plugin
		// defaults
		const FHoudiniStaticMeshGenerationProperties& StaticMeshGenerationProperties = HAC ? HAC->StaticMeshGenerationProperties : FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
		const FMeshBuildSettings& MeshBuildSettings = HAC ? HAC->StaticMeshBuildSettings : FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();

		// UWorld *World = ParentActor ? ParentActor->GetWorld() : AssetLink->GetWorld();
		UWorld *World = AssetLink->GetWorld();

		// .. All TOP Nets
		for (UTOPNetwork* CurrentTOPNet : AssetLink->AllTOPNetworks)
		{
			if (!IsValid(CurrentTOPNet))
				continue;
			
			// .. All TOP Nodes
			for (UTOPNode* CurrentTOPNode : CurrentTOPNet->AllTOPNodes)
			{
				if (!IsValid(CurrentTOPNode))
					continue;
				
				// ... All WorkResult
				CurrentTOPNode->bCachedHaveNotLoadedWorkResults = false;
				CurrentTOPNode->bCachedHaveLoadedWorkResults = false;
				
				const int32 NumWorkResults = CurrentTOPNode->WorkResult.Num();
				for (int32 WorkResultArrayIndex = 0; WorkResultArrayIndex < NumWorkResults; ++WorkResultArrayIndex)
				// for (FTOPWorkResult& CurrentWorkResult : CurrentTOPNode->WorkResult)
				{
					FTOPWorkResult& CurrentWorkResult = CurrentTOPNode->WorkResult[WorkResultArrayIndex];
					// ... All WorkResultObjects
					const int32 NumWorkResultObjects = CurrentWorkResult.ResultObjects.Num();
					for (int32 WorkResultObjectArrayIndex = 0; WorkResultObjectArrayIndex < NumWorkResultObjects; ++WorkResultObjectArrayIndex)
					// for (FTOPWorkResultObject& CurrentWorkResultObj : CurrentWorkResult.ResultObjects)
					{
						FTOPWorkResultObject& CurrentWorkResultObj = CurrentWorkResult.ResultObjects[WorkResultObjectArrayIndex];
						if (CurrentWorkResultObj.State == EPDGWorkResultState::ToLoad)
						{
							CurrentWorkResultObj.State = EPDGWorkResultState::Loading;

							// Load this WRObj
							PackageParams.PDGTOPNetworkName = CurrentTOPNet->NodeName;
							PackageParams.PDGTOPNodeName = CurrentTOPNode->NodeName;
							PackageParams.PDGWorkItemIndex = CurrentWorkResult.WorkItemIndex;
							// Use the array index to ensure uniqueness among the work items of the node (
							// CurrentWorkResult.WorkItemIndex is not necessarily unique)
							PackageParams.PDGWorkResultArrayIndex = WorkResultArrayIndex;

							if (CommandletStatus == EHoudiniBGEOCommandletStatus::Connected)
							{
								BGEOCommandletEndpoint->Send(new FHoudiniPDGImportBGEOMessage(
									CurrentWorkResultObj.FilePath,
									CurrentWorkResultObj.Name,
									PackageParams,
									CurrentTOPNode->NodeId,
									CurrentWorkResult.WorkItemID,
									StaticMeshGenerationProperties,
									MeshBuildSettings
								), BGEOCommandletAddress);
							}
							else
							{
								if (FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem(
									AssetLink,
									CurrentTOPNode,
									CurrentWorkResultObj,
									PackageParams))
								{
									CurrentWorkResultObj.State = EPDGWorkResultState::Loaded;
									CurrentWorkResultObj.SetAutoBakedSinceLastLoad(false);
									CurrentTOPNode->bCachedHaveLoadedWorkResults = true;
									
									// Broadcast that we have loaded the work result object to those interested
									AssetLink->OnWorkResultObjectLoaded.Broadcast(
										AssetLink, CurrentTOPNode, WorkResultArrayIndex,
										CurrentWorkResultObj.WorkItemResultInfoIndex);
								}
								else
								{
									CurrentWorkResultObj.State = EPDGWorkResultState::None;
								}
							}
						}
						else if (CurrentWorkResultObj.State == EPDGWorkResultState::Loaded)
						{
							// If the work item result obj is in the "Loaded" state, confirm that the output actor
							// is still valid (the user could have manually deleted the output
							if (!IsValid(CurrentWorkResultObj.GetOutputActorOwner().GetOutputActor()))
							{
								// If the output actor is invalid, set the state to ToDelete to complete the
								// unload/deletion process
								CurrentWorkResultObj.State = EPDGWorkResultState::ToDelete;
							}
							else
							{
								CurrentTOPNode->bCachedHaveLoadedWorkResults = true;
							}
						}
						else if (CurrentWorkResultObj.State == EPDGWorkResultState::ToDelete)
						{
							CurrentWorkResultObj.State = EPDGWorkResultState::Deleting;

							// Delete and clean up that WRObj
							CurrentTOPNode->DeleteWorkResultObjectOutputs(WorkResultArrayIndex, WorkResultObjectArrayIndex);
							CurrentTOPNode->bCachedHaveNotLoadedWorkResults = true;
						}
						else if (CurrentWorkResultObj.State == EPDGWorkResultState::Deleted)
						{
							CurrentTOPNode->bCachedHaveNotLoadedWorkResults = true;
						}
						else if (CurrentWorkResultObj.State == EPDGWorkResultState::NotLoaded)
						{
							CurrentTOPNode->bCachedHaveNotLoadedWorkResults = true;
						}
					}
				}
			}
		}
	}
}

void FHoudiniPDGManager::HandleImportBGEODiscoverMessage(
	const FHoudiniPDGImportBGEODiscoverMessage& InMessage,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	HOUDINI_LOG_DISPLAY(TEXT("Received Discover from %s"), *InContext->GetSender().ToString());
	// Ignore any discover acks received if we already have a valid local address
	// for the commandlet
	if (BGEOCommandletAddress.IsValid())
		return;

	if (BGEOCommandletProcHandle.IsValid() && InMessage.CommandletGuid.IsValid() && BGEOCommandletGuid == InMessage.CommandletGuid)
	{
		BGEOCommandletAddress = InContext->GetSender();
	}
}

void FHoudiniPDGManager::HandleImportBGEOResultMessage(
	const FHoudiniPDGImportBGEOResultMessage& InMessage, 
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	HOUDINI_LOG_MESSAGE(TEXT("Received BGEO import result message"));
	if (InMessage.ImportResult == EHoudiniPDGImportBGEOResult::HPIBR_Success || InMessage.ImportResult == EHoudiniPDGImportBGEOResult::HPIBR_PartialSuccess)
	{
		FHoudiniPackageParams PackageParams;
		InMessage.PopulatePackageParams(PackageParams);

		// Find asset link and work result object
		UHoudiniPDGAssetLink *AssetLink = nullptr;
		UTOPNetwork *TOPNetwork = nullptr;
		UTOPNode *TOPNode = nullptr;
		if (!GetTOPAssetLinkNetworkAndNode(InMessage.TOPNodeId, AssetLink, TOPNetwork, TOPNode) ||
			!IsValid(AssetLink) || !IsValid(TOPNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to find TOP node with id %d, aborting output object creation."), InMessage.TOPNodeId);
			return;
		}

		FTOPWorkResult* WorkResult = nullptr;
		const int32 WorkResultArrayIndex = TOPNode->ArrayIndexOfWorkResultByID(InMessage.WorkItemId);
		if (WorkResultArrayIndex != INDEX_NONE)
			WorkResult = TOPNode->GetWorkResultByArrayIndex(WorkResultArrayIndex);
		if (WorkResult == nullptr)
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to find TOP work result with id %d, aborting output object creation."), InMessage.WorkItemId);
			return;
		}
		const FString& WorkResultObjectName = InMessage.Name;
		FTOPWorkResultObject* WorkResultObject = WorkResult->ResultObjects.FindByPredicate(
			[&WorkResultObjectName](const FTOPWorkResultObject& WorkResultObject) 
			{ 
				return WorkResultObject.Name == WorkResultObjectName; 
			}
		);
		if (WorkResultObject == nullptr)
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to find TOP work result object with name %s, aborting output object creation."), *InMessage.Name);
			return;
		}

		if (WorkResultObject->State != EPDGWorkResultState::Loading)
		{
			HOUDINI_LOG_WARNING(TEXT("TOP work result object (%s) not in Loading state, aborting output object creation."), *InMessage.Name);
			return;
		}

		// Set package params outer
		UObject* AssetLinkParent = AssetLink->GetOuter();
		UHoudiniAssetComponent* HAC = AssetLinkParent != nullptr ? Cast<UHoudiniAssetComponent>(AssetLinkParent) : nullptr;
		if (HAC)
		{
			PackageParams.OuterPackage = HAC->GetComponentLevel();
		}
		else
		{
			PackageParams.OuterPackage = AssetLinkParent->GetOutermost();
		}

		// Construct UHoudiniOutputs
		bool bHasUnsupportedOutputs = false;
		TArray<UHoudiniOutput*> NewOutputs;
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData> InstancedOutputPartData;
		NewOutputs.Reserve(InMessage.Outputs.Num());
		for (const FHoudiniPDGImportNodeOutput& Output : InMessage.Outputs)
		{
			UHoudiniOutput* NewOutput = NewObject<UHoudiniOutput>(
				AssetLink,
				UHoudiniOutput::StaticClass(),
				NAME_None,//FName(*OutputName),
				RF_NoFlags);
			NewOutputs.Add(NewOutput);
			const int32 NumHGPO = Output.HoudiniGeoPartObjects.Num();
			for (int32 Index = 0; Index < NumHGPO; ++Index)
			{
				const FHoudiniGeoPartObject& HGPO = Output.HoudiniGeoPartObjects[Index];
				NewOutput->AddNewHGPO(HGPO);
				
				if (Output.InstancedOutputPartData.IsValidIndex(Index))
				{
					FHoudiniOutputObjectIdentifier Identifier;
					Identifier.ObjectId = HGPO.ObjectId;
					Identifier.GeoId = HGPO.GeoId;
					Identifier.PartId = HGPO.PartId;
					Identifier.PartName = HGPO.PartName;
					FHoudiniInstancedOutputPartData InstancedPartData = Output.InstancedOutputPartData[Index];
					InstancedPartData.BuildOriginalInstancedTransformsAndObjectArrays();
					InstancedOutputPartData.Add(Identifier, InstancedPartData);
				}
			}
			const int32 NumObjects = Output.OutputObjects.Num();
			for (int32 Index = 0; Index < NumObjects; ++Index)
			{
				const FHoudiniPDGImportNodeOutputObject& ImportOutputObject = Output.OutputObjects[Index];
				FHoudiniOutputObjectIdentifier Identifier = ImportOutputObject.Identifier;

				const FString& FullPackagePath = ImportOutputObject.PackagePath;
				FString PackagePath;
				FString PackageName;
				const bool bDidSplit = FullPackagePath.Split(TEXT("."), &PackagePath, &PackageName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (!bDidSplit)
					PackagePath = FullPackagePath;

				FHoudiniOutputObject OutputObject;
				UPackage* Package = FindPackage(nullptr, *PackagePath);
				if (!IsValid(Package))
				{
					// Editor might have picked up the package yet, try to load it
					Package = LoadPackage(nullptr, *PackagePath, LOAD_NoWarn);
				}
				if (IsValid(Package))
				{
					OutputObject.OutputObject = FindObject<UObject>(Package, *PackageName);
				}
				Identifier.bLoaded = true;
				NewOutput->GetOutputObjects().Add(Identifier, OutputObject);
			}
			NewOutput->UpdateOutputType();
			const EHoudiniOutputType OutputType = NewOutput->GetType();
			if (OutputType != EHoudiniOutputType::Mesh && OutputType != EHoudiniOutputType::Instancer)
			{
				bHasUnsupportedOutputs = true;
			}
		}

		bool bSuccess = true;
		if (bHasUnsupportedOutputs)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Processing output types not supported by commandlet for %s"), *InMessage.Name);
			bSuccess = FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem(
				AssetLink, TOPNode, *WorkResultObject, PackageParams, 
				{
					EHoudiniOutputType::Landscape,
					EHoudiniOutputType::Curve,
					EHoudiniOutputType::Skeletal
				}
			);
			
			if (bSuccess)
			{
				// Clear/remove the outputs on WorkResultObject that are supported by the commandlet, since we
				// are going to replace them with NewOutputs now
				TArray<UHoudiniOutput*>& CurrentOutputs = WorkResultObject->GetResultOutputs();
				const int32 NumCurrentOutputs = CurrentOutputs.Num();
				for (int32 Index = 0; Index < NumCurrentOutputs; ++Index)
				{
					UHoudiniOutput* CurOutput = CurrentOutputs[Index];
					const EHoudiniOutputType OutputType = CurOutput->GetType();
					if (OutputType != EHoudiniOutputType::Mesh && OutputType != EHoudiniOutputType::Instancer)
					{
						// Was created in editor, override the dummy one in NewOutputs with CurOutput
						if (NewOutputs.IsValidIndex(Index))
						{
							if (OutputType == NewOutputs[Index]->GetType())
							{
								UHoudiniOutput* TempOutput = NewOutputs[Index];
								FHoudiniOutputTranslator::ClearOutput(TempOutput);
								NewOutputs[Index] = CurOutput;
							}
							else
							{
								HOUDINI_LOG_ERROR(TEXT("Unexpected commandlet output type at index %d!"), Index);
							}
						}
						else
						{
							HOUDINI_LOG_ERROR(TEXT("Expected output index %d from commandlet to be exist!"), Index);
						}
					}
				}
			}
		}
		
		if (bSuccess && FHoudiniPDGTranslator::LoadExistingAssetsAsResultObjectsForPDGWorkItem(
				AssetLink,
				TOPNode,
				*WorkResultObject,
				PackageParams,
				NewOutputs,
				{EHoudiniOutputType::Mesh, EHoudiniOutputType::Instancer},
				&InstancedOutputPartData))
		{
			const int32 NumOutputs = NewOutputs.Num();
			for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
			{
				UHoudiniOutput *NewOutput = NewOutputs[OutputIndex];

				if (NewOutput->GetType() != EHoudiniOutputType::Mesh)
					continue;
				
				const FHoudiniPDGImportNodeOutput& Output = InMessage.Outputs[OutputIndex];
				int32 NumObjects = Output.OutputObjects.Num();
				for (int32 Index = 0; Index < NumObjects; ++Index)
				{
					const FHoudiniPDGImportNodeOutputObject& ImportOutputObject = Output.OutputObjects[Index];
					FHoudiniOutputObjectIdentifier Identifier = ImportOutputObject.Identifier;
					FHoudiniOutputObject *OutputObject = NewOutput->GetOutputObjects().Find(Identifier);
					if (OutputObject)
					{
						if (IsValid(OutputObject->OutputComponent))
						{
							// Update generic property attributes
							FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
								OutputObject->OutputComponent,
								ImportOutputObject.GenericAttributes.PropertyAttributes);
						}

						// Copy cached attributes
						OutputObject->CachedAttributes.Append(ImportOutputObject.CachedAttributes);
					}
				}
			}
		}
		else
		{
			bSuccess = false;
		}
		
		if (bSuccess)
		{
			WorkResultObject->State = EPDGWorkResultState::Loaded;
			WorkResultObject->SetAutoBakedSinceLastLoad(false);
			HOUDINI_LOG_MESSAGE(TEXT("Loaded geo for %s"), *InMessage.Name);
			// Broadcast that we have loaded the work result object to those interested
			AssetLink->OnWorkResultObjectLoaded.Broadcast(
				AssetLink, TOPNode, WorkResultArrayIndex, WorkResultObject->WorkItemResultInfoIndex);
		}
		else
		{
			WorkResultObject->State = EPDGWorkResultState::None;
			HOUDINI_LOG_WARNING(TEXT("Failed to process loaded assets for %s"), *InMessage.Name);
		}
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Commandlet failed to import bgeo for %s"), *InMessage.Name);
	}
}

bool FHoudiniPDGManager::CreateBGEOCommandletAndEndpoint()
{
	if (!BGEOCommandletEndpoint.IsValid())
	{
		BGEOCommandletAddress.Invalidate();
		BGEOCommandletEndpoint = FMessageEndpoint::Builder(TEXT("Houdini BGEO Commandlet"))
			.Handling<FHoudiniPDGImportBGEOResultMessage>(this, &FHoudiniPDGManager::HandleImportBGEOResultMessage)
			.Handling<FHoudiniPDGImportBGEODiscoverMessage>(this, &FHoudiniPDGManager::HandleImportBGEODiscoverMessage)
			.ReceivingOnThread(ENamedThreads::GameThread);
		
		if (!BGEOCommandletEndpoint.IsValid())
		{
			HOUDINI_LOG_WARNING(TEXT("Could not set up messaging end point for BGEO commandlet"));
			return false;
		}

		BGEOCommandletEndpoint->Subscribe<FHoudiniPDGImportBGEODiscoverMessage>();
	}

	if (!BGEOCommandletProcHandle.IsValid() || !FPlatformProcess::IsProcRunning(BGEOCommandletProcHandle))
	{
		// Start the bgeo commandlet
		static const FString BGEOCommandletName = TEXT("HoudiniGeoImport");
		BGEOCommandletGuid = FGuid::NewGuid();
		BGEOCommandletAddress.Invalidate();

		// Get the absolute path to the project file, if known, otherwise get
		// the project name. For the path: quote it for the command line.
		IFileManager& FileManager = IFileManager::Get();
		FString ProjectPathOrName = FApp::GetProjectName();
		if (FPaths::IsProjectFilePathSet())
		{
			const FString ProjectPath = FPaths::GetProjectFilePath();
			if (!ProjectPath.IsEmpty())
			{
				ProjectPathOrName = FString::Printf(
                    TEXT("\"%s\""),
                    *FileManager.ConvertToAbsolutePathForExternalAppForRead(*ProjectPath)
                );
			}
		}

		if (ProjectPathOrName.IsEmpty())
			return false;

		// Get the executable path for the app/editor
		FString ExePath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		if (!ExePath.IsEmpty())
			ExePath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*ExePath);

		if (ExePath.IsEmpty())
			return false;
		
		const FString CommandLineParameters = FString::Printf(
			TEXT("%s -messaging -run=%s -guid=%s -listen=%s -managerpid=%d"),
			*ProjectPathOrName,
			*BGEOCommandletName,
			*BGEOCommandletGuid.ToString(),
			*BGEOCommandletEndpoint->GetAddress().ToString(),
			FPlatformProcess::GetCurrentProcessId());

		BGEOCommandletProcHandle = FPlatformProcess::CreateProc(
			*ExePath,
			*CommandLineParameters,
			false,
			true,
			false,
			&BGEOCommandletProcessId,
			0,
			NULL,
			NULL);
		if (!BGEOCommandletProcHandle.IsValid())
		{
			return false;
		}
	}

	return true;
}

void FHoudiniPDGManager::StopBGEOCommandletAndEndpoint()
{
	BGEOCommandletEndpoint.Reset();
	BGEOCommandletAddress.Invalidate();
	BGEOCommandletGuid.Invalidate();

	if (BGEOCommandletProcHandle.IsValid() && FPlatformProcess::IsProcRunning(BGEOCommandletProcHandle))
	{
		FPlatformProcess::TerminateProc(BGEOCommandletProcHandle, true);
		if (BGEOCommandletProcHandle.IsValid())
		{
			FPlatformProcess::WaitForProc(BGEOCommandletProcHandle);
			FPlatformProcess::CloseProc(BGEOCommandletProcHandle);
		}
	}
}

EHoudiniBGEOCommandletStatus FHoudiniPDGManager::UpdateAndGetBGEOCommandletStatus()
{
	if (BGEOCommandletProcHandle.IsValid())
	{
		if (!FPlatformProcess::IsProcRunning(BGEOCommandletProcHandle))
			BGEOCommandletStatus = EHoudiniBGEOCommandletStatus::Crashed;
		else if (BGEOCommandletAddress.IsValid())
			BGEOCommandletStatus = EHoudiniBGEOCommandletStatus::Connected;
		else
			BGEOCommandletStatus = EHoudiniBGEOCommandletStatus::Running;
	}
	else
		BGEOCommandletStatus = EHoudiniBGEOCommandletStatus::NotStarted;

	return BGEOCommandletStatus;
}


bool
FHoudiniPDGManager::IsPDGAsset(const HAPI_NodeId& InAssetId)
{
	if (InAssetId < 0)
		return false;

	// Get the list of all non bypassed TOP nodes within the current network (ignoring schedulers)
	int32 TOPNodeCount = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(), InAssetId,
		HAPI_NodeType::HAPI_NODETYPE_TOP, HAPI_NODEFLAGS_TOP_NONSCHEDULER | HAPI_NODEFLAGS_NON_BYPASS, true, &TOPNodeCount))
	{
		return false;
	}

	// We found valid TOP Nodes, this is a PDG HDA
	if (TOPNodeCount > 0)
		return true;

	/*
	// Get all the network nodes within the asset, recursively.
	// We're getting all networks because TOP network SOPs aren't considered being of TOP network type, but SOP type
	int32 NetworkNodeCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(), InAssetId,
		HAPI_NODETYPE_SOP | HAPI_NODETYPE_TOP, HAPI_NODEFLAGS_NETWORK, true, & NetworkNodeCount), false);

	if (NetworkNodeCount <= 0)
		return false;

	TArray<HAPI_NodeId> AllNetworkNodeIDs;
	AllNetworkNodeIDs.SetNum(NetworkNodeCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
		FHoudiniEngine::Get().GetSession(), InAssetId,
		AllNetworkNodeIDs.GetData(), NetworkNodeCount), false);

	// There is currently no way to only get non bypassed nodes via HAPI
	// So we now need to get a list of all the bypassed top nets, in order to remove them from the previous list...
	TArray<HAPI_NodeId> AllBypassedTOPNetNodeIDs;
	{
		int32 BypassedTOPNetNodeCount = 0;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(), InAssetId,
			HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_NETWORK | HAPI_NODEFLAGS_BYPASS, true, &BypassedTOPNetNodeCount), false);

		if (BypassedTOPNetNodeCount > 0)
		{
			// Get the list of all bypassed TOP Net...
			AllBypassedTOPNetNodeIDs.SetNum(BypassedTOPNetNodeCount);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
				FHoudiniEngine::Get().GetSession(), InAssetId,
				AllBypassedTOPNetNodeIDs.GetData(), BypassedTOPNetNodeCount), false);

			// ... and remove them from the network list
			for (int32 Idx = AllNetworkNodeIDs.Num() - 1; Idx >= 0; Idx--)
			{
				if (AllBypassedTOPNetNodeIDs.Contains(AllNetworkNodeIDs[Idx]))
					AllNetworkNodeIDs.RemoveAt(Idx);
			}
		}
	}

	// For each Network we found earlier, only consider those with TOP child nodes
	// If we find TOP nodes in a valid network, then consider this HDA a PDG HDA
	HAPI_NodeInfo CurrentNodeInfo;
	FHoudiniApi::NodeInfo_Init(&CurrentNodeInfo);
	for (const HAPI_NodeId& CurrentNodeId : AllNetworkNodeIDs)
	{
		if (CurrentNodeId < 0)
		{
			continue;
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId, &CurrentNodeInfo))
		{
			continue;
		}

		// Skip non TOP or SOP networks
		if (CurrentNodeInfo.type != HAPI_NodeType::HAPI_NODETYPE_TOP
			&& CurrentNodeInfo.type != HAPI_NodeType::HAPI_NODETYPE_SOP)
		{
			continue;
		}

		// Get the list of all TOP nodes within the current network (ignoring schedulers)		
		int32 TOPNodeCount = 0;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId,
			HAPI_NodeType::HAPI_NODETYPE_TOP, HAPI_NODEFLAGS_TOP_NONSCHEDULER, true, &TOPNodeCount))
		{
			continue;
		}

		// We found valid TOP Nodes, this is a PDG HDA
		if (TOPNodeCount > 0)
			return true;
	}
	*/

	// No valid TOP node found in SOP/TOP Networks, this is not a PDG HDA
	return false;
}

#undef LOCTEXT_NAMESPACE