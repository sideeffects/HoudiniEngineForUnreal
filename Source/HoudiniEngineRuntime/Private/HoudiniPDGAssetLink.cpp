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

#include "HoudiniPDGAssetLink.h"

#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniOutput.h"

#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "UObject/MetaData.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"

#if WITH_EDITOR
	#include "FileHelpers.h"
	#include "EditorModeManager.h"
	#include "EditorModes.h"
#endif

//
UHoudiniPDGAssetLink::UHoudiniPDGAssetLink(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AssetName()
	, AssetNodePath()
	, AssetID(-1)
	, SelectedTOPNetworkIndex(-1)
	, LinkState(EPDGLinkState::Inactive)
	, bAutoCook(false)
	, bUseTOPNodeFilter(true)
	, bUseTOPOutputFilter(true)
	, NumWorkitems(0)
	, WorkItemTally()
	, OutputCachePath()
	, bNeedsUIRefresh(false)
	, OutputParentActor(nullptr)
{
	TOPNodeFilter = HAPI_UNREAL_PDG_DEFAULT_TOP_FILTER;
	TOPOutputFilter = HAPI_UNREAL_PDG_DEFAULT_TOP_OUTPUT_FILTER;

#if WITH_EDITORONLY_DATA
	bBakeMenuExpanded = true;
	HoudiniEngineBakeOption = EHoudiniEngineBakeOption::ToActor;
	PDGBakeSelectionOption = EPDGBakeSelectionOption::All;
	PDGBakePackageReplaceMode = EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
	bRecenterBakedActors = false;
	bBakeAfterAllWorkResultObjectsLoaded = false;
#endif
	
	// Folder used for baking PDG outputs
	BakeFolder.Path = HAPI_UNREAL_DEFAULT_BAKE_FOLDER;

	// TODO:
	// Update init, move default filter to PCH
}

FTOPWorkResultObject::FTOPWorkResultObject()
{
	// ResultObjects = nullptr;
	Name = FString();
	FilePath = FString();
	State = EPDGWorkResultState::None;
	WorkItemResultInfoIndex = INDEX_NONE;
	bAutoBakedSinceLastLoad = false;
}

FTOPWorkResultObject::~FTOPWorkResultObject()
{
	// DestroyResultOutputs();
}

FTOPWorkResult::FTOPWorkResult()
{
	WorkItemIndex = -1;
	WorkItemID = -1;

	ResultObjects.SetNum(0);
}

bool 
FTOPWorkResult::operator==(const FTOPWorkResult& OtherWorkResult) const
{
	if (WorkItemIndex != OtherWorkResult.WorkItemIndex)
		return false;
	if (WorkItemID != OtherWorkResult.WorkItemID)
		return false;
	/*
	if (ResultObjects != OtherWorkResult.ResultObjects)
		return false;
	*/

	return true;
}

void
FTOPWorkResult::ClearAndDestroyResultObjects(const FGuid& InHoudiniComponentGuid)
{
	if (ResultObjects.Num() <= 0)
		return;

	for (FTOPWorkResultObject& ResultObject : ResultObjects)
	{
		ResultObject.DestroyResultOutputsAndRemoveOutputActor(InHoudiniComponentGuid);
	}
	
	ResultObjects.Empty();
}

int32 
FTOPWorkResult::IndexOfWorkResultObjectByHAPIResultInfoIndex(const int32& InWorkItemResultInfoIndex)
{
	const int32 NumEntries = ResultObjects.Num();
	for (int32 Index = 0; Index < NumEntries; ++Index)
	{
		const FTOPWorkResultObject& CurResultObject = ResultObjects[Index];
		if (CurResultObject.WorkItemResultInfoIndex == InWorkItemResultInfoIndex)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FTOPWorkResultObject*
FTOPWorkResult::GetWorkResultObjectByHAPIResultInfoIndex(const int32& InWorkItemResultInfoIndex)
{
	const int32 ArrayIndex = IndexOfWorkResultObjectByHAPIResultInfoIndex(InWorkItemResultInfoIndex);
	if (ArrayIndex == INDEX_NONE)
	{
		return nullptr;
	}
	
	return GetWorkResultObjectByArrayIndex(ArrayIndex);
}

FTOPWorkResultObject*
FTOPWorkResult::GetWorkResultObjectByArrayIndex(const int32& InArrayIndex)
{
	if (!ResultObjects.IsValidIndex(InArrayIndex))
	{
		return nullptr;
	}

	return &ResultObjects[InArrayIndex];
}


FWorkItemTallyBase::~FWorkItemTallyBase()
{
}

bool
FWorkItemTallyBase::AreAllWorkItemsComplete() const
{
	return (
		NumWaitingWorkItems() == 0 && NumCookingWorkItems() == 0 && NumScheduledWorkItems() == 0 
		&& (NumWorkItems() == (NumCookedWorkItems() + NumErroredWorkItems())) );
}

bool
FWorkItemTallyBase::AnyWorkItemsFailed() const
{
	return NumErroredWorkItems() > 0;
}

bool
FWorkItemTallyBase::AnyWorkItemsPending() const
{
	return (NumWorkItems() > 0 && (NumWaitingWorkItems() > 0 || NumCookingWorkItems() > 0 || NumScheduledWorkItems() > 0));
}

FString
FWorkItemTallyBase::ProgressRatio() const
{
	const float Ratio = NumWorkItems() > 0 ? (NumCookedWorkItems() / NumWorkItems()) * 100.f : 0;

	return FString::Printf(TEXT("%.1f%%"), Ratio);
}


FWorkItemTally::FWorkItemTally()
{
	AllWorkItems.Empty();
	WaitingWorkItems.Empty();
	ScheduledWorkItems.Empty();
	CookingWorkItems.Empty();
	CookedWorkItems.Empty();
	ErroredWorkItems.Empty();
	CookCancelledWorkItems.Empty();
}

void
FWorkItemTally::ZeroAll()
{
	AllWorkItems.Empty();
	WaitingWorkItems.Empty();
	ScheduledWorkItems.Empty();
	CookingWorkItems.Empty();
	CookedWorkItems.Empty();
	ErroredWorkItems.Empty();
	CookCancelledWorkItems.Empty();
}

void 
FWorkItemTally::RemoveWorkItem(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	AllWorkItems.Remove(InWorkItemID);
}

void 
FWorkItemTally::RecordWorkItemAsWaiting(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	WaitingWorkItems.Add(InWorkItemID);
	AllWorkItems.Add(InWorkItemID);
}

void 
FWorkItemTally::RecordWorkItemAsScheduled(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	ScheduledWorkItems.Add(InWorkItemID);
	AllWorkItems.Add(InWorkItemID);
}

void 
FWorkItemTally::RecordWorkItemAsCooking(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	CookingWorkItems.Add(InWorkItemID);
	AllWorkItems.Add(InWorkItemID);
}

void 
FWorkItemTally::RecordWorkItemAsCooked(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	CookedWorkItems.Add(InWorkItemID);
	AllWorkItems.Add(InWorkItemID);
}

void 
FWorkItemTally::RecordWorkItemAsErrored(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	ErroredWorkItems.Add(InWorkItemID);
	AllWorkItems.Add(InWorkItemID);
}

void 
FWorkItemTally::RecordWorkItemAsCookCancelled(int32 InWorkItemID)
{
	RemoveWorkItemFromAllStateSets(InWorkItemID);
	CookCancelledWorkItems.Add(InWorkItemID);
	AllWorkItems.Add(InWorkItemID);
}

void
FWorkItemTally::RemoveWorkItemFromAllStateSets(int32 InWorkItemID)
{
	WaitingWorkItems.Remove(InWorkItemID);
	ScheduledWorkItems.Remove(InWorkItemID);
	CookingWorkItems.Remove(InWorkItemID);
	CookedWorkItems.Remove(InWorkItemID);
	ErroredWorkItems.Remove(InWorkItemID);
	CookCancelledWorkItems.Remove(InWorkItemID);
}


FAggregatedWorkItemTally::FAggregatedWorkItemTally()
{
	TotalWorkItems = 0;
	WaitingWorkItems = 0;
	ScheduledWorkItems = 0;
	CookingWorkItems = 0;
	CookedWorkItems = 0;
	ErroredWorkItems = 0;
	CookCancelledWorkItems = 0;
}

void
FAggregatedWorkItemTally::ZeroAll()
{
	TotalWorkItems = 0;
	WaitingWorkItems = 0;
	ScheduledWorkItems = 0;
	CookingWorkItems = 0;
	CookedWorkItems = 0;
	ErroredWorkItems = 0;
	CookCancelledWorkItems = 0;
}

void 
FAggregatedWorkItemTally::Add(const FWorkItemTallyBase& InWorkItemTally)
{
	TotalWorkItems += InWorkItemTally.NumWorkItems();
	WaitingWorkItems += InWorkItemTally.NumWaitingWorkItems();
	ScheduledWorkItems += InWorkItemTally.NumScheduledWorkItems();
	CookingWorkItems += InWorkItemTally.NumCookingWorkItems();
	CookedWorkItems += InWorkItemTally.NumCookedWorkItems();
	ErroredWorkItems += InWorkItemTally.NumErroredWorkItems();
	CookCancelledWorkItems += InWorkItemTally.NumCookCancelledWorkItems();
}

void 
FAggregatedWorkItemTally::Subtract(const FWorkItemTallyBase& InWorkItemTally)
{
	TotalWorkItems -= InWorkItemTally.NumWorkItems();
	WaitingWorkItems -= InWorkItemTally.NumWaitingWorkItems();
	ScheduledWorkItems -= InWorkItemTally.NumScheduledWorkItems();
	CookingWorkItems -= InWorkItemTally.NumCookingWorkItems();
	CookedWorkItems -= InWorkItemTally.NumCookedWorkItems();
	ErroredWorkItems -= InWorkItemTally.NumErroredWorkItems();
	CookCancelledWorkItems -= InWorkItemTally.NumCookCancelledWorkItems();
}


UTOPNode::UTOPNode()
{
	NodeId = -1;
	NodeName = FString();
	NodePath = FString();
	ParentName = FString();

	WorkResultParent = nullptr;
	WorkResult.SetNum(0);

	bHidden = false;
	bAutoLoad = false;

	NodeState = EPDGNodeState::None;
	
	bCachedHaveNotLoadedWorkResults = false;
	bCachedHaveLoadedWorkResults = false;
	bHasChildNodes = false;
	
	bShow = false;

	bHasReceivedCookCompleteEvent = false;

	InvalidateLandscapeCache();
}

bool
UTOPNode::operator==(const UTOPNode& Other) const
{
	if (!NodeName.Equals(Other.NodeName))
		return false;

	if (!ParentName.Equals(Other.ParentName))
		return false;

	//if (NodeId != Other.NodeId)
	//	return false;

	return true;
}

void 
UTOPNode::Reset()
{
	NodeState = EPDGNodeState::None;
	WorkItemTally.ZeroAll();
	AggregatedWorkItemTally.ZeroAll();
}

UHoudiniPDGAssetLink* UTOPNode::GetOuterAssetLink() const
{
	return GetTypedOuter<UHoudiniPDGAssetLink>();
}

void UTOPNode::OnWorkItemWaiting(int32 InWorkItemID)
{
	FTOPWorkResult* const WorkItem = GetWorkResultByID(InWorkItemID);
	if (WorkItem)
	{
		// Clear the bAutoBakedSinceLastLoad flag on the work results since we are expecting a cook of the work item
		for (FTOPWorkResultObject& WRO : WorkItem->ResultObjects)
		{
			WRO.SetAutoBakedSinceLastLoad(false);
		}
	}
	WorkItemTally.RecordWorkItemAsWaiting(InWorkItemID);
}

void
UTOPNode::OnWorkItemCooked(int32 InWorkItemID)
{
	if (GetWorkItemTally().NumCookedWorkItems() == 0)
	{
		// We want to invalidate the landscape cache values in any situation where
		// all the work items are being recooked.
		InvalidateLandscapeCache();
	}
	WorkItemTally.RecordWorkItemAsCooked(InWorkItemID);
}

void
UTOPNode::SetVisibleInLevel(bool bInVisible)
{
	if (bShow == bInVisible)
		return;
	
	bShow = bInVisible;
	UpdateOutputVisibilityInLevel();
}

void
UTOPNode::UpdateOutputVisibilityInLevel()
{
	AActor* Actor = OutputActorOwner.GetOutputActor();
	if (IsValid(Actor))
	{
		Actor->SetHidden(!bShow);
#if WITH_EDITOR
		Actor->SetIsTemporarilyHiddenInEditor(!bShow);
#endif
	}
	for (FTOPWorkResult& WorkItem : WorkResult)
	{
		for (FTOPWorkResultObject& WRO : WorkItem.ResultObjects)
		{
			AActor* WROActor = WRO.GetOutputActorOwner().GetOutputActor();
			if (IsValid(WROActor))
			{
				WROActor->SetHidden(!bShow);
#if WITH_EDITOR
				WROActor->SetIsTemporarilyHiddenInEditor(!bShow);
#endif
			}

			// We need to manually handle child landscape's visiblity
			for (UHoudiniOutput* ResultOutput : WRO.GetResultOutputs())
			{
				if (!IsValid(ResultOutput))
					continue;

				for (auto& Pair : ResultOutput->GetOutputObjects())
				{
					FHoudiniOutputObject& OutputObject = Pair.Value;
					ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(OutputObject.OutputObject);
					if (!IsValid(LandscapeProxy))
						continue;

					ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
					if (!IsValid(Landscape))
						continue;

					Landscape->SetHidden(!bShow);
#if WITH_EDITOR
					Landscape->SetIsTemporarilyHiddenInEditor(!bShow);
#endif
				}
			}
		}
	}	
}

void
UTOPNode::SetNotLoadedWorkResultsToLoad(bool bInAlsoSetDeletedToLoad)
{
	for (FTOPWorkResult& WorkItem : WorkResult)
	{
		for (FTOPWorkResultObject& WRO : WorkItem.ResultObjects)
		{
			if (WRO.State == EPDGWorkResultState::NotLoaded ||
					(WRO.State == EPDGWorkResultState::Deleted && bInAlsoSetDeletedToLoad))
			{
				WRO.State = EPDGWorkResultState::ToLoad;
				WRO.SetAutoBakedSinceLastLoad(false);
			}
		}
	}
}

void
UTOPNode::SetLoadedWorkResultsToDelete()
{
	for (FTOPWorkResult& WorkItem : WorkResult)
	{
		for (FTOPWorkResultObject& WRO : WorkItem.ResultObjects)
		{
			if (WRO.State == EPDGWorkResultState::Loaded)
				WRO.State = EPDGWorkResultState::ToDelete;
		}
    }	
}

FGuid
UTOPNode::GetHoudiniComponentGuid() const
{
	UHoudiniPDGAssetLink const* const AssetLink = GetOuterAssetLink();
	if (!IsValid(AssetLink))
		return FGuid();
	
	return AssetLink->GetOuterHoudiniComponentGuid();
}

void
UTOPNode::DeleteWorkResultObjectOutputs(const int32 InWorkResultArrayIndex, const int32 InWorkResultObjectArrayIndex, const bool bInDeleteOutputActors)
{
	if (!WorkResult.IsValidIndex(InWorkResultArrayIndex))
		return;
	
	FTOPWorkResult& WorkItem = WorkResult[InWorkResultArrayIndex];
	if (!WorkItem.ResultObjects.IsValidIndex(InWorkResultObjectArrayIndex))
		return;
	
	FTOPWorkResultObject& WRO = WorkItem.ResultObjects[InWorkResultObjectArrayIndex];
	// Delete and clean up that WRObj
	WRO.DestroyResultOutputs(GetHoudiniComponentGuid());
	if (bInDeleteOutputActors)
		WRO.GetOutputActorOwner().DestroyOutputActor();
	WRO.State = EPDGWorkResultState::Deleted;
}

void
UTOPNode::DeleteWorkItemOutputs(const int32 InWorkResultArrayIndex, const bool bInDeleteOutputActors)
{
	if (!WorkResult.IsValidIndex(InWorkResultArrayIndex))
		return;
	
	const FTOPWorkResult& WorkItem = WorkResult[InWorkResultArrayIndex];
	const int32 NumResultObjects = WorkItem.ResultObjects.Num();
	for (int32 ResultObjectIndex = 0; ResultObjectIndex < NumResultObjects; ++ResultObjectIndex)
	{
		DeleteWorkResultObjectOutputs(InWorkResultArrayIndex, ResultObjectIndex, bInDeleteOutputActors);
	}
}

void
UTOPNode::DeleteAllWorkResultObjectOutputs(const bool bInDeleteOutputActors)
{
	const int32 NumWorkItems = WorkResult.Num();
	for (int32 WorkItemIndex = 0; WorkItemIndex < NumWorkItems; ++WorkItemIndex)
	{
		DeleteWorkItemOutputs(WorkItemIndex, bInDeleteOutputActors);
	}
	bCachedHaveLoadedWorkResults = false;
}

FString
UTOPNode::GetBakedWorkResultObjectOutputsKey(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex)
{
	return FString::Printf(TEXT("%d_%d"), InWorkResultArrayIndex, InWorkResultObjectArrayIndex);
}

FString
UTOPNode::GetBakedWorkResultObjectOutputsKey(const FTOPWorkResult& InWorkResult, int32 InWorkResultObjectArrayIndex) const
{
	if (InWorkResult.WorkItemID == INDEX_NONE)
		return FString();

	const int32 WorkResultArrayIndex = ArrayIndexOfWorkResultByID(InWorkResult.WorkItemID);
	if (WorkResultArrayIndex == INDEX_NONE)
		return FString();
	
	return GetBakedWorkResultObjectOutputsKey(WorkResultArrayIndex, InWorkResultObjectArrayIndex);
}

bool
UTOPNode::GetBakedWorkResultObjectOutputsKey(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex, FString& OutKey) const
{
	// Check that indices are valid
	if (!WorkResult.IsValidIndex(InWorkResultArrayIndex))
		return false;
	const FTOPWorkResult& WorkResultEntry = WorkResult[InWorkResultArrayIndex];
	if (!WorkResultEntry.ResultObjects.IsValidIndex(InWorkResultObjectArrayIndex))
		return false;

	OutKey = GetBakedWorkResultObjectOutputsKey(InWorkResultArrayIndex, InWorkResultObjectArrayIndex);

	return true;
}

bool
UTOPNode::GetBakedWorkResultObjectOutputs(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex, FHoudiniPDGWorkResultObjectBakedOutput*& OutBakedOutput)
{
	FString Key;
	if (!GetBakedWorkResultObjectOutputsKey(InWorkResultArrayIndex, InWorkResultObjectArrayIndex, Key))
		return false;
	OutBakedOutput = BakedWorkResultObjectOutputs.Find(Key);
	if (!OutBakedOutput)
		return false;

	return true;
}

bool
UTOPNode::GetBakedWorkResultObjectOutputs(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex, FHoudiniPDGWorkResultObjectBakedOutput const*& OutBakedOutput) const
{
	FString Key;
	if (!GetBakedWorkResultObjectOutputsKey(InWorkResultArrayIndex, InWorkResultObjectArrayIndex, Key))
		return false;
	OutBakedOutput = BakedWorkResultObjectOutputs.Find(Key);
	if (!OutBakedOutput)
		return false;

	return true;
}

int32
UTOPNode::ArrayIndexOfWorkResultByID(const int32& InWorkItemID) const
{
	const int32 NumEntries = WorkResult.Num();
	for (int32 Index = 0; Index < NumEntries; ++Index)
	{
		const FTOPWorkResult& CurResult = WorkResult[Index];
		if (CurResult.WorkItemID == InWorkItemID)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FTOPWorkResult*
UTOPNode::GetWorkResultByID(const int32& InWorkItemID)
{
	const int32 ArrayIndex = ArrayIndexOfWorkResultByID(InWorkItemID);
	if (!WorkResult.IsValidIndex(ArrayIndex))
		return nullptr;

	return &WorkResult[ArrayIndex];
}

int32
UTOPNode::ArrayIndexOfFirstInvalidWorkResult() const
{
	const int32 NumEntries = WorkResult.Num();
	for (int32 Index = 0; Index < NumEntries; ++Index)
	{
		const FTOPWorkResult& CurResult = WorkResult[Index];
		if (CurResult.WorkItemID == INDEX_NONE)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FTOPWorkResult*
UTOPNode::GetWorkResultByArrayIndex(const int32& InArrayIndex)
{
	if (!WorkResult.IsValidIndex(InArrayIndex))
		return nullptr;
	return &WorkResult[InArrayIndex];
}

bool
UTOPNode::IsParentTOPNetwork(UTOPNetwork const * const InNetwork) const
{
	if (!IsValid(InNetwork))
	{
		return false;
	}

	return ParentName == FString::Printf(TEXT("%s_%s"), *InNetwork->ParentName, *InNetwork->NodeName);
}

bool
UTOPNode::CanStillBeAutoBaked() const
{
	// Only nodes that have results auto-loaded are auto-baked
	if (!bAutoLoad)
		return false;

	// Nodes with failures are not auto-baked
	if (AnyWorkItemsFailed())
		return false;

	// All work items are not yet complete, so node cannot yet be baked
	if (!AreAllWorkItemsComplete())
		return true;

	// Work items that are currently loaded or has not tagged has auto baked since last load can still be baked
	for (const FTOPWorkResult& WorkResultEntry : WorkResult)
	{
		for (const FTOPWorkResultObject& WRO : WorkResultEntry.ResultObjects)
		{
			switch (WRO.State)
			{
				case EPDGWorkResultState::NotLoaded:
				case EPDGWorkResultState::ToLoad:
				case EPDGWorkResultState::Loading:
					return true;
				case EPDGWorkResultState::Loaded:
					if (!WRO.AutoBakedSinceLastLoad())
						return true;
					break;
				case EPDGWorkResultState::ToDelete:
				case EPDGWorkResultState::Deleting:
				case EPDGWorkResultState::Deleted:
				case EPDGWorkResultState::None:
					break;
			}
		}
	}

	return false;
}

#if WITH_EDITOR
void
UTOPNode::PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTOPNode, bShow))
	{
		UpdateOutputVisibilityInLevel();
	}
}
#endif

#if WITH_EDITOR
void
UTOPNode::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() != ETransactionObjectEventType::UndoRedo)
		return;

	bool bUpdateVisibility = false;
	for (const FName& PropName : TransactionEvent.GetChangedProperties())
	{
		if (PropName == GET_MEMBER_NAME_CHECKED(UTOPNode, bShow))
		{
			bUpdateVisibility = true;
		}
	}

	if (bUpdateVisibility)
		UpdateOutputVisibilityInLevel();
}
#endif

void
UTOPNode::OnDirtyNode()
{
	InvalidateLandscapeCache();
	bHasReceivedCookCompleteEvent = false;
}

void
UTOPNode::InvalidateLandscapeCache()
{
	LandscapeReferenceLocation.bIsCached = false;
	LandscapeSizeInfo.bIsCached = false;
	ClearedLandscapeLayers.Empty();
}

UTOPNetwork::UTOPNetwork()
{
	NodeId = -1;
	NodeName = FString();

	AllTOPNodes.SetNum(0);
	SelectedTOPIndex = -1;

	ParentName = FString();

	bShowResults = false;
	bAutoLoadResults = false;
}

bool
UTOPNetwork::operator==(const UTOPNetwork& Other) const
{
	if (!NodeName.Equals(Other.NodeName))
		return false;

	if (!ParentName.Equals(Other.ParentName))
		return false;

	//if (NodeId != Other.NodeId)
	//	return false;

	return true;
}

void
UTOPNetwork::SetLoadedWorkResultsToDelete()
{
	for (UTOPNode* Node : AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;
		
		Node->SetLoadedWorkResultsToDelete();
	}
}

void
UTOPNetwork::DeleteAllWorkResultObjectOutputs()
{
	for (UTOPNode* Node : AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;
		
		Node->DeleteAllWorkResultObjectOutputs();
	}
}

bool
UTOPNetwork::AnyWorkItemsPending() const
{
	for (const UTOPNode* const TOPNode : AllTOPNodes)
	{
		if (!IsValid(TOPNode))
			continue;

		if (TOPNode->AnyWorkItemsPending())
			return true;
	}

	return false;
}

bool
UTOPNetwork::AnyWorkItemsFailed() const
{
	for (const UTOPNode* const TOPNode : AllTOPNodes)
	{
		if (!IsValid(TOPNode))
			continue;

		if (TOPNode->AnyWorkItemsFailed())
			return true;
	}

	return false;
}

bool
UTOPNetwork::CanStillBeAutoBaked() const
{
	for (const UTOPNode* const TOPNode : AllTOPNodes)
	{
		if (TOPNode->CanStillBeAutoBaked())
			return true;
	}

	return false;
}

void
UTOPNetwork::HandleOnPDGEventCookCompleteReceivedByChildNode(UHoudiniPDGAssetLink* const InAssetLink, UTOPNode* const InTOPNode)
{
	if (!IsValid(InAssetLink))
		return;

	// Check if all nodes have received the HAPI_PDG_EVENT_COOK_COMPLETE event, if so, broadcast the OnPostCook handler.
	for (const UTOPNode* const TOPNode : AllTOPNodes)
	{
		if (!IsValid(TOPNode))
			continue;

		if (!TOPNode->HasReceivedCookCompleteEvent())
			return;
	}

	if (OnPostCookDelegate.IsBound())
		OnPostCookDelegate.Broadcast(this, AnyWorkItemsFailed());

	InAssetLink->HandleOnTOPNetworkCookComplete(this);
}

void
UHoudiniPDGAssetLink::SelectTOPNetwork(const int32& AtIndex)
{
	if (!AllTOPNetworks.IsValidIndex(AtIndex))
		return;

	SelectedTOPNetworkIndex = AtIndex;
}


void
UHoudiniPDGAssetLink::SelectTOPNode(UTOPNetwork* InTOPNetwork, const int32& AtIndex)
{
	if (!IsValid(InTOPNetwork))
		return;
	
	if (!InTOPNetwork->AllTOPNodes.IsValidIndex(AtIndex))
		return;

	InTOPNetwork->SelectedTOPIndex = AtIndex;
}


UTOPNetwork*
UHoudiniPDGAssetLink::GetSelectedTOPNetwork()
{
	return GetTOPNetwork(SelectedTOPNetworkIndex);
}

const UTOPNetwork*
UHoudiniPDGAssetLink::GetSelectedTOPNetwork() const
{
	return GetTOPNetwork(SelectedTOPNetworkIndex);
}

UTOPNode*
UHoudiniPDGAssetLink::GetSelectedTOPNode()
{
	UTOPNetwork* const SelectedTOPNetwork = GetSelectedTOPNetwork();
	if (!IsValid(SelectedTOPNetwork))
		return nullptr;

	if (!SelectedTOPNetwork->AllTOPNodes.IsValidIndex(SelectedTOPNetwork->SelectedTOPIndex))
		return nullptr;

	UTOPNode* const SelectedTOPNode = SelectedTOPNetwork->AllTOPNodes[SelectedTOPNetwork->SelectedTOPIndex];
	if (!IsValid(SelectedTOPNode))
		return nullptr;

	return SelectedTOPNode;
}

const UTOPNode*
UHoudiniPDGAssetLink::GetSelectedTOPNode() const
{
	UTOPNetwork const* const SelectedTOPNetwork = GetSelectedTOPNetwork();
	if (!IsValid(SelectedTOPNetwork))
		return nullptr;

	if (!SelectedTOPNetwork->AllTOPNodes.IsValidIndex(SelectedTOPNetwork->SelectedTOPIndex))
		return nullptr;

	UTOPNode const* const SelectedTOPNode = SelectedTOPNetwork->AllTOPNodes[SelectedTOPNetwork->SelectedTOPIndex];
	if (!IsValid(SelectedTOPNode))
		return nullptr;

	return SelectedTOPNode;
}

FString
UHoudiniPDGAssetLink::GetSelectedTOPNodeName()
{
	FString NodeName = FString();

	const UTOPNode* const SelectedTOPNode = GetSelectedTOPNode();
	if (IsValid(SelectedTOPNode))
		NodeName = SelectedTOPNode->NodeName;

	return NodeName;
}

FString
UHoudiniPDGAssetLink::GetSelectedTOPNetworkName()
{
	FString NetworkName = FString();

	const UTOPNetwork* const SelectedTOPNetwork = GetSelectedTOPNetwork();
	if (IsValid(SelectedTOPNetwork))
		NetworkName = SelectedTOPNetwork->NodeName;

	return NetworkName;
}

UTOPNetwork* 
UHoudiniPDGAssetLink::GetTOPNetwork(const int32& AtIndex)
{
	if(AllTOPNetworks.IsValidIndex(AtIndex))
	{
		return AllTOPNetworks[AtIndex];
	}

	return nullptr;
}

const UTOPNetwork* 
UHoudiniPDGAssetLink::GetTOPNetwork(const int32& AtIndex) const
{
	if(AllTOPNetworks.IsValidIndex(AtIndex))
	{
		return AllTOPNetworks[AtIndex];
	}

	return nullptr;
}

UTOPNetwork*
UHoudiniPDGAssetLink::GetTOPNetworkByNodePath(const FString& InNodePath, const TArray<UTOPNetwork*>& InTOPNetworks, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	int32 Index = -1;
	for (UTOPNetwork* CurrentTOPNet : InTOPNetworks)
	{
		Index += 1;

		if (!IsValid(CurrentTOPNet))
			continue;
		
		if (CurrentTOPNet->NodePath.Equals(InNodePath))
		{
			OutIndex = Index;
			return CurrentTOPNet;
		}
	}

	return nullptr;
}

UTOPNode*
UHoudiniPDGAssetLink::GetParentTOPNode(const UTOPNode* InNode)
{
	if (!IsValid(InNode))
		return nullptr;
	
	FString NodePath = InNode->NodePath;
	FString ParentPath;

	if (NodePath.EndsWith("/"))
		NodePath.LeftChopInline(1);

	if (NodePath.Split("/", &ParentPath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && !ParentPath.IsEmpty())
	{
		for (UTOPNetwork* TOPNet : AllTOPNetworks)
		{
			if (!IsValid(TOPNet))
				continue;
			
			for (UTOPNode* TOPNode : TOPNet->AllTOPNodes)
			{
				if (!IsValid(TOPNode))
					continue;
				
				if (TOPNode->NodePath == ParentPath && InNode->NodeId != TOPNode->NodeId)
				{
					return TOPNode;
				}
			}
		}
	}

	return nullptr;
}

UTOPNode*
UHoudiniPDGAssetLink::GetTOPNodeByNodePath(const FString& InNodePath, const TArray<UTOPNode*>& InTOPNodes, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	int32 Index = -1;
	for (UTOPNode* CurrentTOPNode : InTOPNodes)
	{
		Index += 1;
		
		if (!IsValid(CurrentTOPNode))
			continue;
		
		if (CurrentTOPNode->NodePath.Equals(InNodePath))
		{
			OutIndex = Index;
			return CurrentTOPNode;
		}
	}

	return nullptr;
}

void
UHoudiniPDGAssetLink::ClearAllTOPData()
{
	// Clears all TOP data
	for(UTOPNetwork* CurrentNetwork : AllTOPNetworks)
	{
		if (!IsValid(CurrentNetwork))
			continue;
		
		for(UTOPNode* CurrentTOPNode : CurrentNetwork->AllTOPNodes)
		{
			if (!IsValid(CurrentTOPNode))
				continue;
			
			ClearTOPNodeWorkItemResults(CurrentTOPNode);
		}
	}

	AllTOPNetworks.Empty();
}

void 
UHoudiniPDGAssetLink::ClearTOPNetworkWorkItemResults(UTOPNetwork* TOPNetwork)
{
	if (!IsValid(TOPNetwork))
		return;
	
	for(UTOPNode* CurrentTOPNode : TOPNetwork->AllTOPNodes)
	{
		if (!IsValid(CurrentTOPNode))
			continue;
		
		ClearTOPNodeWorkItemResults(CurrentTOPNode);
	}
}

void
UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(UTOPNode* TOPNode)
{
	if (!IsValid(TOPNode))
		return;

	TOPNode->OnDirtyNode();

	const FGuid HoudiniComponentGuid(TOPNode->GetHoudiniComponentGuid());
	for(FTOPWorkResult& CurrentWorkResult : TOPNode->WorkResult)
	{
		CurrentWorkResult.ClearAndDestroyResultObjects(HoudiniComponentGuid);
	}
	TOPNode->WorkResult.Empty();

	FOutputActorOwner& OutputActorOwner = TOPNode->GetOutputActorOwner();
	AActor* OutputActor = OutputActorOwner.GetOutputActor();
	if (IsValid(OutputActor))
	{
		// Destroy any attached actors (which we'll assume that any attachments left
		// are untracked actors associated with the TOPNode)
		TArray<AActor*> AttachedActors;
		OutputActor->GetAttachedActors(AttachedActors);
		for (AActor* Actor : AttachedActors)
		{
			if (!IsValid(Actor))
				continue;
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Actor->Destroy();
		}
	}

	if (IsValid(TOPNode->WorkResultParent))
	{

		// TODO: Destroy the Parent Object
		// DestroyImmediate(topNode._workResultParentGO);
	}

	OutputActorOwner.DestroyOutputActor();
}


void
UHoudiniPDGAssetLink::ClearWorkItemResultByID(const int32& InWorkItemID, UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return;
	
	FTOPWorkResult* WorkResult = GetWorkResultByID(InWorkItemID, InTOPNode);
	if (WorkResult)
	{
		WorkResult->ClearAndDestroyResultObjects(InTOPNode->GetHoudiniComponentGuid());
		// TODO: Should we destroy the FTOPWorkResult struct entirely here?
		//TOPNode.WorkResult.RemoveByPredicate 
	}
}

void
UHoudiniPDGAssetLink::DestroyWorkItemByID(const int32& InWorkItemID, UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return;
	
	// TODO: Update ClearWorkItemResultByID or GetWorkResultByID to return the index of the work item
	// so that we don't have to find its index again to remove it from the array
	ClearWorkItemResultByID(InWorkItemID, InTOPNode);
	// Find the index of the FTOPWorkResult for InWorkItemID in InTOPNode.WorkResult and remove it
	const int32 Index = InTOPNode->WorkResult.IndexOfByPredicate(
		[InWorkItemID](const FTOPWorkResult& InWorkItem) { return InWorkItem.WorkItemID == InWorkItemID; });
	if (Index != INDEX_NONE && Index >= 0)
		InTOPNode->WorkResult.RemoveAt(Index);
}

FTOPWorkResult*
UHoudiniPDGAssetLink::GetWorkResultByID(const int32& InWorkItemID, UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return nullptr;
	return InTOPNode->GetWorkResultByID(InWorkItemID);
}

FDirectoryPath
UHoudiniPDGAssetLink::GetTemporaryCookFolder() const
{
	UHoudiniAssetComponent* HAC = GetOuterHoudiniAssetComponent();
	if (HAC)
		return HAC->TemporaryCookFolder;
	
	FDirectoryPath TempPath;
	TempPath.Path = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
	return TempPath;
}

FGuid
UHoudiniPDGAssetLink::GetOuterHoudiniComponentGuid() const
{
	UHoudiniAssetComponent const* const HAC = GetOuterHoudiniAssetComponent();
	if (!IsValid(HAC))
		return FGuid();

	return HAC->GetComponentGUID();
}

void
UHoudiniPDGAssetLink::HandleOnTOPNetworkCookComplete(UTOPNetwork* const InTOPNet)
{
	if (!IsValid(InTOPNet))
		return;

	if (OnPostTOPNetworkCookDelegate.IsBound())
	{
		OnPostTOPNetworkCookDelegate.Broadcast(this, InTOPNet, InTOPNet->AnyWorkItemsFailed());
	}
}


UTOPNode*
UHoudiniPDGAssetLink::GetTOPNode(const int32& InNodeID)
{
	UTOPNetwork* Network = nullptr;
	UTOPNode* Node = nullptr;

	if (GetTOPNodeAndNetworkByNodeId(InNodeID, Network, Node))
		return Node;

	return nullptr;
}


bool
UHoudiniPDGAssetLink::GetTOPNodeAndNetworkByNodeId(const int32& InNodeID, UTOPNetwork*& OutNetwork, UTOPNode*& OutNode)
{
	OutNetwork = nullptr;
	OutNode = nullptr;

	for (UTOPNetwork* CurrentTOPNet : AllTOPNetworks)
	{
		if (!IsValid(CurrentTOPNet))
			continue;
		
		for (UTOPNode* CurrentTOPNode : CurrentTOPNet->AllTOPNodes)
		{
			if (!IsValid(CurrentTOPNode))
				continue;
			
			if (CurrentTOPNode->NodeId == InNodeID)
			{
				OutNetwork = CurrentTOPNet;
				OutNode = CurrentTOPNode;
				return true;
			}
		}
	}

	return false;
}


void
UHoudiniPDGAssetLink::UpdateTOPNodeWithChildrenWorkItemTallyAndState(UTOPNode* InNode, UTOPNetwork* InNetwork)
{
	if (!IsValid(InNode) || !IsValid(InNetwork))
		return;
	
	if (!InNode->bHasChildNodes)
		return;

	FString PrefixPath = InNode->NodePath;
	if (!PrefixPath.EndsWith("/"))
		PrefixPath += "/";
	InNode->ZeroWorkItemTally();
	InNode->NodeState = EPDGNodeState::None;

	TMap<EPDGNodeState, int8> NodeStateOrder;
	NodeStateOrder.Add(EPDGNodeState::None, 0);
	NodeStateOrder.Add(EPDGNodeState::Cook_Complete, 1);
	NodeStateOrder.Add(EPDGNodeState::Dirtied, 2);
	NodeStateOrder.Add(EPDGNodeState::Cook_Failed, 3);
	NodeStateOrder.Add(EPDGNodeState::Dirtying, 4);
	NodeStateOrder.Add(EPDGNodeState::Cooking, 5);

	int8 CurrentState = 0;
	
	for (const UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;
		
		if (Node->NodePath.StartsWith(PrefixPath) && !Node->bHasChildNodes)
		{
			InNode->AggregateTallyFromChildNode(Node);
			const int8 VisitedNodeState = NodeStateOrder.FindChecked(Node->NodeState);
			if (VisitedNodeState > CurrentState)
				CurrentState = VisitedNodeState;
		}
	}

	EPDGNodeState const* const NewState = NodeStateOrder.FindKey(CurrentState);
	if (NewState)
		InNode->NodeState = *NewState;
}

void
UHoudiniPDGAssetLink::UpdateWorkItemTally()
{
	WorkItemTally.ZeroAll();		
	for(UTOPNetwork* CurrentTOPNet : AllTOPNetworks)
	{
		if (!IsValid(CurrentTOPNet))
			continue;
		
		for(UTOPNode* CurrentTOPNode : CurrentTOPNet->AllTOPNodes)
		{
			if (!IsValid(CurrentTOPNode))
				continue;
			
			// Only add up the tallys from nodes without children (since parent's aggregate the child work items counts)
			if (CurrentTOPNode->bHasChildNodes)
			{
				UpdateTOPNodeWithChildrenWorkItemTallyAndState(CurrentTOPNode, CurrentTOPNet);
			}
			else
			{
				WorkItemTally.Add(CurrentTOPNode->GetWorkItemTally());
			}
		}
	}
}


void
UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(UTOPNetwork* TOPNetwork)
{
	if (!IsValid(TOPNetwork))
		return;
	
	for (UTOPNode* CurTOPNode : TOPNetwork->AllTOPNodes)
	{
		if (!IsValid(CurTOPNode))
			continue;
		
		CurTOPNode->ZeroWorkItemTally();
	}
}


FString 
UHoudiniPDGAssetLink::GetAssetLinkStatus(const EPDGLinkState& InLinkState)
{
	FString Status;
	switch (InLinkState)
	{
	case EPDGLinkState::Inactive:
		Status = TEXT("Inactive");
	case EPDGLinkState::Linking:
		Status = TEXT("Linking");
	case EPDGLinkState::Linked:
		Status = TEXT("Linked");
	case EPDGLinkState::Error_Not_Linked:
		Status = TEXT("Not Linked");
	default:
		Status = TEXT("");
	}

	return Status;
}

FString
UHoudiniPDGAssetLink::GetTOPNodeStatus(const UTOPNode* InTOPNode)
{
	static const FString InvalidOrUnknownStatus = TEXT("");
	
	if (!IsValid(InTOPNode))
		return InvalidOrUnknownStatus;
	
	if (InTOPNode->NodeState == EPDGNodeState::Cook_Failed || InTOPNode->AnyWorkItemsFailed())
	{
		return TEXT("Cook Failed");
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Cook_Complete)
	{
		return TEXT("Cook Completed");
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Cooking)
	{
		return TEXT("Cook In Progress");
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Dirtied)
	{
		return TEXT("Dirtied");
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Dirtying)
	{
		return TEXT("Dirtying");
	}

	return InvalidOrUnknownStatus;
}

FLinearColor
UHoudiniPDGAssetLink::GetTOPNodeStatusColor(const UTOPNode* InTOPNode)
{
	if (!IsValid(InTOPNode))
		return FLinearColor::White;
	
	if (InTOPNode->NodeState == EPDGNodeState::Cook_Failed || InTOPNode->AnyWorkItemsFailed())
	{
		return FLinearColor::Red;
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Cook_Complete)
	{
		return FLinearColor::Green;
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Cooking)
	{
		return FLinearColor(0.0, 1.0f, 1.0f);
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Dirtied)
	{
		return FLinearColor(1.0f, 0.5f, 0.0f);
	}
	else if (InTOPNode->NodeState == EPDGNodeState::Dirtying)
	{
		return FLinearColor::Yellow;
	}

	return FLinearColor::White;
}

AActor*
UHoudiniPDGAssetLink::GetOwnerActor() const
{
	UObject* Outer = GetOuter();
	UActorComponent* Component = Cast<UActorComponent>(Outer);
	if (IsValid(Component))
		return Component->GetOwner();
	else
		return Cast<AActor>(Outer);
}

bool
UHoudiniPDGAssetLink::HasTemporaryOutputs() const
{
	// Loop over all networks, all nodes, all work items and check for any valid output objects
	for (const UTOPNetwork* TOPNetwork : AllTOPNetworks)
	{
		if (!IsValid(TOPNetwork))
			continue;
		
		for (const UTOPNode* TOPNode : TOPNetwork->AllTOPNodes)
		{
			if (!IsValid(TOPNode))
				continue;
			
			for (const FTOPWorkResult& WorkResult : TOPNode->WorkResult)
			{
				for (const FTOPWorkResultObject& WorkResultObject : WorkResult.ResultObjects)
				{
					// If the WorkResultObject's actor is not valid, then it no longer has temporary objects in the
					// scene
					if (!IsValid(WorkResultObject.GetOutputActorOwner().GetOutputActor()))
						continue;
					
					for (UHoudiniOutput* Output : WorkResultObject.GetResultOutputs())
					{
						if (!IsValid(Output))
							continue;

						const EHoudiniOutputType OutputType = Output->GetType();
						for (const auto& OutputObjectPair : Output->GetOutputObjects())
						{
							if ((OutputType == EHoudiniOutputType::Landscape && IsValid(OutputObjectPair.Value.OutputObject)) ||
								IsValid(OutputObjectPair.Value.OutputComponent))
							{
								return true;								
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void
UHoudiniPDGAssetLink::UpdatePostDuplicate()
{
	// Loop over all networks, all nodes, all work items and clear output actors
	for (UTOPNetwork* TOPNetwork : AllTOPNetworks)
	{
		if (!IsValid(TOPNetwork))
			continue;
		
		for (UTOPNode* TOPNode : TOPNetwork->AllTOPNodes)
		{
			if (!IsValid(TOPNode))
				continue;
			
			for (FTOPWorkResult& WorkResult : TOPNode->WorkResult)
			{
				for (FTOPWorkResultObject& WorkResultObject : WorkResult.ResultObjects)
				{
					WorkResultObject.GetOutputActorOwner().SetOutputActor(nullptr);
					WorkResultObject.State = EPDGWorkResultState::None;
					WorkResultObject.SetResultOutputs(TArray<UHoudiniOutput*>());
				}
			}
			TOPNode->GetOutputActorOwner().SetOutputActor(nullptr);
			TOPNode->bCachedHaveNotLoadedWorkResults = false;
			TOPNode->bCachedHaveLoadedWorkResults = false;
		}
	}
}

UHoudiniAssetComponent* UHoudiniPDGAssetLink::GetOuterHoudiniAssetComponent() const
{
	return Cast<UHoudiniAssetComponent>( GetTypedOuter<UHoudiniAssetComponent>() );
}

void
UHoudiniPDGAssetLink::UpdateTOPNodeAutoloadAndVisibility()
{
	for (UTOPNetwork* TOPNetwork : AllTOPNetworks)
	{
		if (!IsValid(TOPNetwork))
			continue;
		
		for (UTOPNode* TOPNode : TOPNetwork->AllTOPNodes)
		{
			if (!IsValid(TOPNode))
				continue;
			
			if (TOPNode->bAutoLoad)
			{
				// // Set work results that are cooked but in NotLoaded state to ToLoad
				// TOPNode.SetNotLoadedWorkResultsToLoad();
			}
			
			TOPNode->UpdateOutputVisibilityInLevel();
		}
	}
}

void
UHoudiniPDGAssetLink::FilterTOPNodesAndOutputs()
{
	for (UTOPNetwork* TOPNetwork : AllTOPNetworks)
	{
		if (!IsValid(TOPNetwork))
			continue;
		
		for (UTOPNode* TOPNode : TOPNetwork->AllTOPNodes)
		{
			if (!IsValid(TOPNode))
				continue;
			
			// TOP Node visibility filter via TOPNodeFilter
			if (bUseTOPNodeFilter)
			{
				TOPNode->bHidden = !TOPNodeFilter.IsEmpty() && !TOPNode->NodeName.StartsWith(TOPNodeFilter);
			}
			else
			{
				TOPNode->bHidden = false;
			}

			// Auto load results filter via TOPNodeOutputFilter
			if (bUseTOPOutputFilter)
			{
				const bool bNewAutoLoad = TOPOutputFilter.IsEmpty() || TOPNode->NodeName.StartsWith(TOPOutputFilter);
				if (bNewAutoLoad != TOPNode->bAutoLoad)
				{
					if (bNewAutoLoad)
					{
						// Set work results that are cooked but in NotLoaded state to ToLoad
						TOPNode->bAutoLoad = true;
						// TOPNode->SetNotLoadedWorkResultsToLoad();
						TOPNode->SetVisibleInLevel(true);
					}
					else
					{
						TOPNode->bAutoLoad = false;
						TOPNode->SetVisibleInLevel(false);
					}
					TOPNode->UpdateOutputVisibilityInLevel();
				}
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
bool
UHoudiniPDGAssetLink::AnyRemainingAutoBakeNodes() const
{
	if (!bBakeAfterAllWorkResultObjectsLoaded)
		return false;

	switch (PDGBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
		{
			for (const UTOPNetwork* const TOPNet : AllTOPNetworks)
			{
				if (!IsValid(TOPNet))
					continue;

				if (TOPNet->CanStillBeAutoBaked())
				{
					return true;
				}
			}
			break;
		}
		case EPDGBakeSelectionOption::SelectedNetwork:
		{
			const UTOPNetwork* const TOPNet = GetSelectedTOPNetwork();
			if (IsValid(TOPNet) && TOPNet->CanStillBeAutoBaked())
			{
				return true;
			}
		}
		case EPDGBakeSelectionOption::SelectedNode:
		{
			UTOPNode const* const TOPNode = GetSelectedTOPNode();
			if (IsValid(TOPNode) && TOPNode->CanStillBeAutoBaked())
			{
				return true;
			}
		}
		default:
			return false;
	}

	return false;
}
#endif

void
UHoudiniPDGAssetLink::HandleOnPostBake(const bool bInSuccess)
{
	if (OnPostBakeDelegate.IsBound())
		OnPostBakeDelegate.Broadcast(this, bInSuccess);
}

#if WITH_EDITORONLY_DATA
void
UHoudiniPDGAssetLink::PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedChainEvent);

	const FName PropertyName = InPropertyChangedChainEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPNodeFilter) ||
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPNodeFilter) ||
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPOutputFilter) ||
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPOutputFilter))
	{
		// Refilter TOP nodes
		FilterTOPNodesAndOutputs();
		bNeedsUIRefresh = true;
	}
	else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, SelectedTOPNetworkIndex) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeMenuExpanded))
	{
		bNeedsUIRefresh = true;
	}
}

#endif

#if WITH_EDITORONLY_DATA
void
UHoudiniPDGAssetLink::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	
	if (TransactionEvent.GetEventType() != ETransactionObjectEventType::UndoRedo)
		return;

	bool bDoFilterTOPNodesAndOutputs = false;
	for (const FName& PropName : TransactionEvent.GetChangedProperties())
	{
		if (PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPNodeFilter) ||
			PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPNodeFilter) ||
			PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPOutputFilter) ||
			PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPOutputFilter))
		{
			bDoFilterTOPNodesAndOutputs = true;
			bNeedsUIRefresh = true;
		}
		else if (PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, SelectedTOPNetworkIndex) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, HoudiniEngineBakeOption) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakeSelectionOption) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakePackageReplaceMode) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeMenuExpanded))
		{
			bNeedsUIRefresh = true;
		}
	}

	if (bDoFilterTOPNodesAndOutputs)
		FilterTOPNodesAndOutputs();
}
#endif

void
FTOPWorkResultObject::DestroyResultOutputs(const FGuid& InHoudiniComponentGuid)
{
	TSet<UObject*> OutputObjectsToDelete;

	const FString ComponentGuidString = InHoudiniComponentGuid.IsValid() ? InHoudiniComponentGuid.ToString() : FString();

	// Delete output components and gather output objects for deletion
	bool bDidDestroyObjects = false;
	bool bDidModifyFoliage = false;

	AActor* const OutputActor = OutputActorOwner.GetOutputActor();
	
	for (UHoudiniOutput* CurOutput : ResultOutputs)
	{
		for (auto& Pair : CurOutput->GetOutputObjects())
		{
			FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
			FHoudiniOutputObject& OutputObject = Pair.Value;
			if (IsValid(OutputObject.OutputComponent))
			{
				// Instancer components require some special handling around foliage
				// TODO: move/refactor so that we can use the InstanceTranslator's helper functions (RemoveAndDestroyComponent and CleanupFoliageInstances)
				bool bDestroyComponent = true;
				if (OutputObject.OutputComponent->IsA<UHierarchicalInstancedStaticMeshComponent>())
				{
					UHierarchicalInstancedStaticMeshComponent* HISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(OutputObject.OutputComponent);
					if (HISMC->GetOwner() && HISMC->GetOwner()->IsA<AInstancedFoliageActor>())
					{
						// Make sure foliage our foliage instances have been removed
						USceneComponent* ParentComponent = nullptr;
						if (IsValid(OutputActor))
							ParentComponent = Cast<USceneComponent>(OutputActor->GetRootComponent());
						else
							ParentComponent = Cast<USceneComponent>(HISMC->GetOuter()); 
						if (IsValid(ParentComponent))
						{
							UStaticMesh* FoliageSM = HISMC->GetStaticMesh();
							if (!IsValid(FoliageSM))
								return;

							// If we are a foliage HISMC, then our owner is an Instanced Foliage Actor,
							// if it is not, then we are just a "regular" HISMC
							AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(HISMC->GetOwner());
							if (!IsValid(InstancedFoliageActor))
								return;

							UFoliageType *FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(FoliageSM);
							if (!IsValid(FoliageType))
								return;
#if WITH_EDITOR
							// Clean up the instances previously generated for that component
							InstancedFoliageActor->DeleteInstancesForComponent(ParentComponent, FoliageType);

							// Remove the foliage type if it doesn't have any more instances
							if(HISMC->GetInstanceCount() == 0)
								InstancedFoliageActor->RemoveFoliageType(&FoliageType, 1);

							bDidModifyFoliage = true;
#endif
						}

						// // do not delete FISMC that still have instances left
						// // as we have cleaned up our instances before, these have been hand-placed
						// if (HISMC->GetInstanceCount() > 0)
						bDestroyComponent = false;

						OutputObject.OutputComponent = nullptr;
					}
				}
				
				if (bDestroyComponent)
				{
					USceneComponent* SceneComponent = Cast<USceneComponent>(OutputObject.OutputComponent);
					if (IsValid(SceneComponent))
					{
						// Remove from its actor first
						if (SceneComponent->GetOwner())
							SceneComponent->GetOwner()->RemoveOwnedComponent(SceneComponent);
					
						// Detach from its parent component if attached
						SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
						SceneComponent->UnregisterComponent();
						SceneComponent->DestroyComponent();

						bDidDestroyObjects = true;

						OutputObject.OutputComponent = nullptr;
					}
				}
			}
			if (IsValid(OutputObject.OutputObject))
			{
				// For actors we detach them first and then destroy
				AActor* Actor = Cast<AActor>(OutputObject.OutputObject);
				UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.OutputObject);
				if (LandscapePtr)
				{
					Actor = LandscapePtr->GetRawPtr();
				}

				if (Actor)
				{
					Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
					Actor->Destroy();

					bDidDestroyObjects = true;
				}
				else
				{
					// ... if not an actor, destroy the object if it is a temp object created by the owning
					// HoudiniAssetComponent. Don't delete anything if we don't have a valid component GUID.
					if (IsValid(OutputObject.OutputObject) && !OutputObject.OutputObject->HasAnyFlags(RF_Transient) &&
						!ComponentGuidString.IsEmpty() && !OutputObjectsToDelete.Contains(OutputObject.OutputObject))
					{
						// Only delete if the object has metadata indicating it is a plugin created temp object
						UPackage* const Package = OutputObject.OutputObject->GetOutermost();
						if (IsValid(Package))
						{
							UMetaData* const MetaData = Package->GetMetaData();
							if (IsValid(MetaData))
							{
								// Check for HAPI_UNREAL_PACKAGE_META_TEMP_GUID in the package metadata to confirm that
								// this is a temp package, and then ensure that the component GUID in the package metadata
								// for the object matches the GUID of the owning HAC
								if (MetaData->RootMetaDataMap.Contains(HAPI_UNREAL_PACKAGE_META_TEMP_GUID))
								{
									FString TempGUID;
									TempGUID = MetaData->RootMetaDataMap.FindChecked(HAPI_UNREAL_PACKAGE_META_TEMP_GUID);
									TempGUID.TrimStartAndEndInline();
									if (!TempGUID.IsEmpty())
									{
										// PackageComponentGuidString will be the empty string if the object does not
										// have the HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID metadata key in the package
										const FString PackageComponentGuidString = MetaData->GetValue(OutputObject.OutputObject, HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID);
										if (!PackageComponentGuidString.IsEmpty() && PackageComponentGuidString == ComponentGuidString)
											OutputObjectsToDelete.Add(OutputObject.OutputObject);
									}
								}
							}
						}
					}
					OutputObject.OutputObject = nullptr;
				}
			}
		}
	}

	ResultOutputs.Empty();

	if (bDidDestroyObjects)
		TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	
	// Delete the output objects we found
	if (OutputObjectsToDelete.Num() > 0)
	{
		TArray<UObject*> ObjectsToDelete(OutputObjectsToDelete.Array());
		FHoudiniEngineRuntimeUtils::SafeDeleteObjects(ObjectsToDelete);
	}

#if WITH_EDITOR
	if (bDidModifyFoliage)
	{
		// Repopulate the foliage types in the foliage mode UI if foliage mode is active
		// There is a helper function in FHoudiniEngineUtils for this, but we cannot access it from this module.
		// TODO: refactor?
		FEditorModeTools& EditorModeTools = GLevelEditorModeTools();
		if (EditorModeTools.IsModeActive(FBuiltinEditorModes::EM_Foliage))
		{
			EditorModeTools.DeactivateMode(FBuiltinEditorModes::EM_Foliage);
			EditorModeTools.ActivateMode(FBuiltinEditorModes::EM_Foliage);
		}
	}
#endif
}

void FTOPWorkResultObject::DestroyResultOutputsAndRemoveOutputActor(const FGuid& InHoudiniComponentGuid)
{
	DestroyResultOutputs(InHoudiniComponentGuid);
	GetOutputActorOwner().DestroyOutputActor();
}

bool
FOutputActorOwner::CreateOutputActor(UWorld* InWorld, UHoudiniPDGAssetLink* InAssetLink, AActor *InParentActor, const FName& InName)
{
	// InAssetLink and InWorld must not be null
	if (!IsValid(InAssetLink))
	{
		HOUDINI_LOG_ERROR(TEXT("[FTOPWorkResultObject::CreateWorkResultActor]: InAssetLink is null!"));
		return false;
	}
	if (!IsValid(InWorld))
	{
		HOUDINI_LOG_ERROR(TEXT("[FTOPWorkResultObject::CreateWorkResultActor]: InWorld is null!"));
		return false;
	}

	AActor* AssetLinkActor = InAssetLink->GetOwnerActor();

	const bool bParentActorIsValid = IsValid(InParentActor);
	ULevel* LevelToSpawnIn = nullptr;
	if (bParentActorIsValid)
	{
		LevelToSpawnIn = InParentActor->GetLevel();
	}
	else
	{
		// Get the level containing the asset link's actor
		if (IsValid(AssetLinkActor))
			LevelToSpawnIn = AssetLinkActor->GetLevel();
	}

	// Fallback to InWorld's current level
	UWorld* WorldToSpawnIn = nullptr;
	if (!IsValid(LevelToSpawnIn))
	{
		LevelToSpawnIn = InWorld->GetCurrentLevel();
		WorldToSpawnIn = InWorld;
	}
	else
	{
		WorldToSpawnIn = LevelToSpawnIn->GetWorld();
	}

	if (!IsValid(WorldToSpawnIn) || !IsValid(LevelToSpawnIn))
	{
		HOUDINI_LOG_WARNING(
			TEXT("Could not determine level and world to spawn PDG output actor in: asset link %s, name %s"),
			*(InAssetLink->GetPathName()),
			*(InName.ToString()));
		return false;
	}
	
	FActorSpawnParameters SpawnParams;
#if WITH_EDITOR
	if (!LevelToSpawnIn->bUseExternalActors)
#endif
	{
		SpawnParams.Name = MakeUniqueObjectName(InWorld, AActor::StaticClass(), InName);
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.OverrideLevel = LevelToSpawnIn;
	AActor *Actor = WorldToSpawnIn->SpawnActor<AActor>(SpawnParams);
	SetOutputActor(Actor);
#if WITH_EDITOR
	FHoudiniEngineRuntimeUtils::SetActorLabel(Actor, InName.ToString());
#endif
	
	// Set the actor transform: create a root component if it does not have one
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!IsValid(RootComponent))
	{
		RootComponent = NewObject<USceneComponent>(Actor, USceneComponent::StaticClass(), NAME_None, RF_Transactional);

		// Change the creation method so the component is listed in the details panels
		RootComponent->CreationMethod = EComponentCreationMethod::Instance;
		Actor->SetRootComponent(RootComponent);
		RootComponent->OnComponentCreated();
		RootComponent->RegisterComponent();
	}
	
	RootComponent->SetVisibility(true);
	RootComponent->SetMobility(EComponentMobility::Static);

	const FVector ActorSpawnLocation = InParentActor ? InParentActor->GetActorLocation() : FVector::ZeroVector;
	const FRotator ActorSpawnRotator = InParentActor ? InParentActor->GetActorRotation() : FRotator::ZeroRotator;
	Actor->SetActorLocation(ActorSpawnLocation);
	Actor->SetActorRotation(ActorSpawnRotator);

#if WITH_EDITOR
	if (IsValid(InParentActor) && InParentActor->GetLevel() == LevelToSpawnIn)
	{
		Actor->SetFolderPath(InParentActor->GetFolderPath());
		Actor->AttachToActor(InParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}
	else if (IsValid(AssetLinkActor) && AssetLinkActor->GetLevel() == LevelToSpawnIn)
	{
		Actor->SetFolderPath(*FString::Format(
			TEXT("{0}/{1}_Output"), 
			{ FStringFormatArg(AssetLinkActor->GetFolderPath().ToString()), FStringFormatArg(AssetLinkActor->GetActorLabel()) }
		));
	}
	else
	{
		Actor->SetFolderPath(*FString::Format(TEXT("{0}_Output"), { FStringFormatArg(InAssetLink->GetName()) }));
	}
#else
	if(IsValid(InParentActor) && InParentActor->GetLevel() == LevelToSpawnIn)
	{
		OutputActor->AttachToActor(InParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}
#endif

	return true;
}

bool
FOutputActorOwner::DestroyOutputActor()
{
	bool bDestroyed = false;
	AActor *Actor = GetOutputActor();
	if (IsValid(Actor))
	{
		// Detach from parent before destroying the actor
		Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Actor->Destroy();

		bDestroyed = true;
	}

	SetOutputActor(nullptr);

	return bDestroyed;
}
