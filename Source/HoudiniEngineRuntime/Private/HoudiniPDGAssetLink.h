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

#pragma once

//#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"

// #include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniTranslatorTypes.h"

#include "HoudiniPDGAssetLink.generated.h"

struct FHoudiniPackageParams;

UENUM()
enum class EPDGLinkState : uint8
{
	Inactive,
	Linking,
	Linked,
	Error_Not_Linked
};


UENUM()
enum class EPDGNodeState : uint8
{
	None,
	Dirtied,
	Dirtying,
	Cooking,
	Cook_Complete,
	Cook_Failed
};

UENUM()
enum class EPDGWorkResultState : uint8
{
	None,
	ToLoad,
	Loading,
	Loaded,
	ToDelete,
	Deleting,
	Deleted,
	NotLoaded
};


USTRUCT()
struct HOUDINIENGINERUNTIME_API FOutputActorOwner
{
	GENERATED_BODY();
public:
	FOutputActorOwner()
		: OutputActor(nullptr) {};
	
	virtual ~FOutputActorOwner() {};
	
	// Create OutputActor, an actor to hold work result output
	virtual bool CreateOutputActor(UWorld* InWorld, UHoudiniPDGAssetLink* InAssetLink, AActor *InParentActor, const FName& InName);

	// Return OutputActor
	virtual AActor* GetOutputActor() const { return OutputActor; }

	// Setter for OutputActor
	virtual void SetOutputActor(AActor* InActor) { OutputActor = InActor; }
	
	// Destroy OutputActor if it is valid.
	virtual bool DestroyOutputActor();

private:
	UPROPERTY(NonTransactional)
	AActor* OutputActor;
	
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPWorkResultObject
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FTOPWorkResultObject();

	// Call DestroyResultObjects in the destructor
	virtual ~FTOPWorkResultObject();

	// Set ResultObjects to a copy of InUpdatedOutputs
	void SetResultOutputs(const TArray<UHoudiniOutput*>& InUpdatedOutputs) { ResultOutputs = InUpdatedOutputs; }

	// Getter for ResultOutputs
	TArray<UHoudiniOutput*>& GetResultOutputs() { return ResultOutputs; }
	
	// Getter for ResultOutputs
	const TArray<UHoudiniOutput*>& GetResultOutputs() const { return ResultOutputs; }

	// Destroy ResultOutputs
	void DestroyResultOutputs(const FGuid& InHoudiniComponentGuid);

	// Get the OutputActor owner struct
	FOutputActorOwner& GetOutputActorOwner() { return OutputActorOwner; }

	// Get the OutputActor owner struct
	const FOutputActorOwner& GetOutputActorOwner() const { return OutputActorOwner; }

	// Destroy the ResultOutputs and remove the output actor.
	void DestroyResultOutputsAndRemoveOutputActor(const FGuid& InHoudiniComponentGuid);

	// Getter for bAutoBakedSinceLastLoad: indicates if this work result object has been auto-baked since it's last load.
	bool AutoBakedSinceLastLoad() const { return bAutoBakedSinceLastLoad; }
	// Setter for bAutoBakedSinceLastLoad
	void SetAutoBakedSinceLastLoad(bool bInAutoBakedSinceLastLoad) { bAutoBakedSinceLastLoad = bInAutoBakedSinceLastLoad; }

public:

	UPROPERTY(NonTransactional)
	FString					Name;
	UPROPERTY(NonTransactional)
	FString					FilePath;
	UPROPERTY(NonTransactional)
	EPDGWorkResultState		State;
	// The index in the WorkItemResultInfo array of this item as it was received from HAPI.
	UPROPERTY(NonTransactional)
	int32					WorkItemResultInfoIndex;

protected:
	// UPROPERTY()
	// TArray<UObject*>		ResultObjects;

	UPROPERTY(NonTransactional)
	TArray<UHoudiniOutput*> ResultOutputs;

	// If true, indicates that the work result object has been auto-baked since it was last loaded.
	UPROPERTY(NonTransactional)
	bool bAutoBakedSinceLastLoad = false;

private:
	UPROPERTY(NonTransactional)
	FOutputActorOwner OutputActorOwner;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPWorkResult
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FTOPWorkResult();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const FTOPWorkResult& OtherWorkResult) const;

	// Calls FTOPWorkResultObject::DestroyResultOutputsAndRemoveOutputActor on each entry in ResultObjects and clears the array.
	void ClearAndDestroyResultObjects(const FGuid& InHoudiniComponentGuid);

	// Search for the first FTOPWorkResultObject entry by WorkItemResultInfoIndex and return it, or nullptr if it could not be found.
	int32 IndexOfWorkResultObjectByHAPIResultInfoIndex(const int32& InWorkItemResultInfoIndex);
	// Search for the first FTOPWorkResultObject entry by WorkItemResultInfoIndex and return it, or nullptr if it could not be found.
	FTOPWorkResultObject* GetWorkResultObjectByHAPIResultInfoIndex(const int32& InWorkItemResultInfoIndex);
	// Return the FTOPWorkResultObject at InArrayIndex in the ResultObjects array, or nullptr if InArrayIndex is not a valid index.
	FTOPWorkResultObject* GetWorkResultObjectByArrayIndex(const int32& InArrayIndex);

public:

	UPROPERTY(NonTransactional)
	int32							WorkItemIndex;
	UPROPERTY(Transient)
	int32							WorkItemID;
	
	UPROPERTY(NonTransactional)
	TArray<FTOPWorkResultObject>	ResultObjects;

	/*
	UPROPERTY()
	TArray<UObject*>				ResultObjects;

	UPROPERTY()
	TArray<FString>					ResultNames;
	UPROPERTY()
	TArray<FString>					ResultFilePaths;
	UPROPERTY()
	TArray<EPDGWorkResultState>		ResultStates;
	*/
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FWorkItemTallyBase
{
	GENERATED_USTRUCT_BODY()
	
public:
	virtual ~FWorkItemTallyBase();
	
	//
	// Mutators
	//

	// Zero all counts, including total.
	virtual void ZeroAll() {};
	
	//
	// Accessors
	//
	
	bool AreAllWorkItemsComplete() const;
	bool AnyWorkItemsFailed() const;
	bool AnyWorkItemsPending() const;

	virtual int32 NumWorkItems() const { return 0; };
	virtual int32 NumWaitingWorkItems() const { return 0; };
	virtual int32 NumScheduledWorkItems() const { return 0; };
	virtual int32 NumCookingWorkItems() const { return 0; };
	virtual int32 NumCookedWorkItems() const { return 0; };
	virtual int32 NumErroredWorkItems() const { return 0; };
	virtual int32 NumCookCancelledWorkItems() const { return 0; };

	FString ProgressRatio() const;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FWorkItemTally : public FWorkItemTallyBase
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FWorkItemTally();

	//
	// Mutators
	//

	// Empty all state sets, as well as AllWorkItems.
	virtual void ZeroAll() override;
	
	// Remove a work item from all state sets and AllWorkItems.
	void RemoveWorkItem(int32 InWorkItemID);

	void RecordWorkItemAsWaiting(int32 InWorkItemID);
	void RecordWorkItemAsScheduled(int32 InWorkItemID);
	void RecordWorkItemAsCooking(int32 InWorkItemID);
	void RecordWorkItemAsCooked(int32 InWorkItemID);
	void RecordWorkItemAsErrored(int32 InWorkItemID);
	void RecordWorkItemAsCookCancelled(int32 InWorkItemID);

	//
	// Accessors
	//

	virtual int32 NumWorkItems() const override { return AllWorkItems.Num(); }
	virtual int32 NumWaitingWorkItems() const override { return WaitingWorkItems.Num(); }
	virtual int32 NumScheduledWorkItems() const override { return ScheduledWorkItems.Num(); }
	virtual int32 NumCookingWorkItems() const override { return CookingWorkItems.Num(); }
	virtual int32 NumCookedWorkItems() const override { return CookedWorkItems.Num(); }
	virtual int32 NumErroredWorkItems() const override { return ErroredWorkItems.Num(); }
	virtual int32 NumCookCancelledWorkItems() const override { return CookCancelledWorkItems.Num(); }
	
protected:

	// Removes the work item id from all state sets (but not from AllWorkItems -- use RemoveWorkItem for that).
	void RemoveWorkItemFromAllStateSets(int32 InWorkItemID);

	// We use sets to keep track of in what state a work item is. The set stores the WorkItemID.
	
	UPROPERTY()
	TSet<int32> AllWorkItems;
	UPROPERTY()
	TSet<int32> WaitingWorkItems;
	UPROPERTY()
	TSet<int32> ScheduledWorkItems;
	UPROPERTY()
	TSet<int32> CookingWorkItems;
	UPROPERTY()
	TSet<int32> CookedWorkItems;
	UPROPERTY()
	TSet<int32> ErroredWorkItems;
	UPROPERTY()
	TSet<int32> CookCancelledWorkItems;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FAggregatedWorkItemTally : public FWorkItemTallyBase
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FAggregatedWorkItemTally();

	virtual void ZeroAll() override;

	void Add(const FWorkItemTallyBase& InWorkItemTally);

	void Subtract(const FWorkItemTallyBase& InWorkItemTally);

	virtual int32 NumWorkItems() const override { return TotalWorkItems; }
	virtual int32 NumWaitingWorkItems() const override { return WaitingWorkItems; }
	virtual int32 NumScheduledWorkItems() const override { return ScheduledWorkItems; }
	virtual int32 NumCookingWorkItems() const override { return CookingWorkItems; }
	virtual int32 NumCookedWorkItems() const override { return CookedWorkItems; }
	virtual int32 NumErroredWorkItems() const override { return ErroredWorkItems; }

protected:
	UPROPERTY()
	int32 TotalWorkItems;
	UPROPERTY()
	int32 WaitingWorkItems;
	UPROPERTY()
	int32 ScheduledWorkItems;
	UPROPERTY()
	int32 CookingWorkItems;
	UPROPERTY()
	int32 CookedWorkItems;
	UPROPERTY()
	int32 ErroredWorkItems;
	UPROPERTY()
	int32 CookCancelledWorkItems;
	
};

// Container for baked outputs of a PDG work result object. 
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniPDGWorkResultObjectBakedOutput
{
	GENERATED_BODY()

	public:
		// Array of baked output per output index of the work result object's outputs.
		UPROPERTY()
		TArray<FHoudiniBakedOutput> BakedOutputs;
};

// Forward declare the UTOPNetwork here for some references in the UTOPNode
class UTOPNetwork;

UCLASS()
class HOUDINIENGINERUNTIME_API UTOPNode : public UObject
{
	GENERATED_BODY()

public:
	// Constructor
	UTOPNode();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const UTOPNode& Other) const;

	void Reset();

	/** Get the owning/outer UHoudiniPDGAssetLink of this UTOPNode. */
	UHoudiniPDGAssetLink* GetOuterAssetLink() const;

	/**
	 * Get the Guid of the HoudiniAssetComponent that owns the AssetLink. Returns an invalid Guid if the HAC or
	 * asset link could not be found / is invalid.
	 */
	FGuid GetHoudiniComponentGuid() const;

	const FWorkItemTallyBase& GetWorkItemTally() const
	{
		if (bHasChildNodes)
			return AggregatedWorkItemTally;
		return WorkItemTally;
	}

	void AggregateTallyFromChildNode(const UTOPNode* InChildNode)
	{
		if (IsValid(InChildNode))
			AggregatedWorkItemTally.Add(InChildNode->GetWorkItemTally());
	}

	bool AreAllWorkItemsComplete() const { return GetWorkItemTally().AreAllWorkItemsComplete(); };
	bool AnyWorkItemsFailed() const { return GetWorkItemTally().AnyWorkItemsFailed(); };
	bool AnyWorkItemsPending() const { return GetWorkItemTally().AnyWorkItemsPending(); };
	void ZeroWorkItemTally()
	{
		WorkItemTally.ZeroAll();
		AggregatedWorkItemTally.ZeroAll();
	}

	// Called by PDG manager when work item events are received
	
	// Notification that a work item has been created
	void OnWorkItemCreated(int32 InWorkItemID) { };

	// Notification that a work item has been removed.
	void OnWorkItemRemoved(int32 InWorkItemID) { WorkItemTally.RemoveWorkItem(InWorkItemID); };

	// Notification that a work item has moved to the waiting state.
	void OnWorkItemWaiting(int32 InWorkItemID);

	// Notification that a work item has been scheduled.
	void OnWorkItemScheduled(int32 InWorkItemID) { WorkItemTally.RecordWorkItemAsScheduled(InWorkItemID); };

	// Notification that a work item has started cooking.
	void OnWorkItemCooking(int32 InWorkItemID) { WorkItemTally.RecordWorkItemAsCooking(InWorkItemID); };

	// Notification that a work item has been cooked.
	void OnWorkItemCooked(int32 InWorkItemID);
	
	// Notification that a work item has errored.
	void OnWorkItemErrored(int32 InWorkItemID) { WorkItemTally.RecordWorkItemAsErrored(InWorkItemID); };

	// Notification that a work item cook has been cancelled.
	void OnWorkItemCookCancelled(int32 InWorkItemID) { WorkItemTally.RecordWorkItemAsCookCancelled(InWorkItemID); };

	bool IsVisibleInLevel() const { return bShow; }
	void SetVisibleInLevel(bool bInVisible);
	void UpdateOutputVisibilityInLevel();

	// Sets all WorkResultObjects that are in the NotLoaded state to ToLoad.
	void SetNotLoadedWorkResultsToLoad(bool bInAlsoSetDeletedToLoad=false);

	// Sets all WorkResultObjects that are in the Loaded state to ToDelete (will delete output objects and output
	// actors).
	void SetLoadedWorkResultsToDelete();

	// Immediately delete the Loaded work result output object (keeps the work item and result structs in the arrays but
	// deletes the output object and the actor and sets the state to Deleted.
	void DeleteWorkResultObjectOutputs(const int32 InWorkResultArrayIndex, const int32 InWorkResultObjectArrayIndex, const bool bInDeleteOutputActors=true);

	// Immediately delete Loaded work result output objects for the specified work item (keeps the work item and result
	// arrays but deletes the output objects and actors and sets the state to Deleted.
	void DeleteWorkItemOutputs(const int32 InWorkResultArrayIndex, const bool bInDeleteOutputActors=true);

	// Immediately delete Loaded work result output objects (keeps the work items and result arrays but deletes the output
	// objects and actors and sets the state to Deleted.
	void DeleteAllWorkResultObjectOutputs(const bool bInDeleteOutputActors=true);

	// Get the OutputActor owner struct
	FOutputActorOwner& GetOutputActorOwner() { return OutputActorOwner; }

	// Get the OutputActor owner struct
	const FOutputActorOwner& GetOutputActorOwner() const { return OutputActorOwner; }

	// Get the baked outputs from the last bake. The map keys are [work_result.work_item_index]_[work_result_object_index]
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>& GetBakedWorkResultObjectsOutputs() { return BakedWorkResultObjectOutputs; }
	const TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>& GetBakedWorkResultObjectsOutputs() const { return BakedWorkResultObjectOutputs; }
	// Helper to construct the key used to look up baked work results.
	static FString GetBakedWorkResultObjectOutputsKey(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex);
	// Helper to construct the key used to look up baked work results.
	FString GetBakedWorkResultObjectOutputsKey(const FTOPWorkResult& InWorkResult, int32 InWorkResultObjectArrayIndex) const;
	// Helper to construct the key used to look up baked work results.
	bool GetBakedWorkResultObjectOutputsKey(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex, FString& OutKey) const;
	// Get the FHoudiniPDGWorkResultObjectBakedOutput for a work item (FTOPWorkResult) and specific result object.
	bool GetBakedWorkResultObjectOutputs(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex, FHoudiniPDGWorkResultObjectBakedOutput*& OutBakedOutput);
	// Get the FHoudiniPDGWorkResultObjectBakedOutput for a work item (FTOPWorkResult) and specific result object (const version).
	bool GetBakedWorkResultObjectOutputs(int32 InWorkResultArrayIndex, int32 InWorkResultObjectArrayIndex, FHoudiniPDGWorkResultObjectBakedOutput const*& OutBakedOutput) const;

	// Search for the first FTOPWorkResult entry by WorkItemID and return its array index or INDEX_NONE, if it could not be found.
	int32 ArrayIndexOfWorkResultByID(const int32& InWorkItemID) const;
	// Search for the first FTOPWorkResult entry by WorkItemID and return it, or nullptr if it could not be found.
	FTOPWorkResult* GetWorkResultByID(const int32& InWorkItemID);
	// Search for the first FTOPWorkResult entry with an invalid (INDEX_NONE) work item id and return it, or INDEX_None
	// if no invalid entry could be found.
	int32 ArrayIndexOfFirstInvalidWorkResult() const;
	// Return the FTOPWorkResult at InArrayIndex in the WorkResult array, or nullptr if InArrayIndex is not a valid index.
	FTOPWorkResult* GetWorkResultByArrayIndex(const int32& InArrayIndex);

	// Returns true if InNetwork is the parent TOP Net of this node.
	bool IsParentTOPNetwork(UTOPNetwork const * const InNetwork) const;

	// Returns true if this node can still be auto-baked
	bool CanStillBeAutoBaked() const;

#if WITH_EDITOR
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITOR
	void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

public:

	UPROPERTY(Transient, NonTransactional)
	int32					NodeId;
	UPROPERTY(NonTransactional)
	FString					NodeName;
	UPROPERTY(NonTransactional)
	FString					NodePath;
	UPROPERTY(NonTransactional)
	FString					ParentName;
	
	UPROPERTY()
	UObject*				WorkResultParent;
	UPROPERTY(NonTransactional)
	TArray<FTOPWorkResult>	WorkResult;

	// Hidden in the nodes combobox
	UPROPERTY()
	bool					bHidden;
	UPROPERTY()
	bool					bAutoLoad;

	UPROPERTY(Transient, NonTransactional)
	EPDGNodeState 			NodeState;

	// This is set when the TOP node's work items are processed by
	// FHoudiniPDGManager based on if any NotLoaded work result objects are found
	UPROPERTY(NonTransactional)
	bool bCachedHaveNotLoadedWorkResults;

	// This is set when the TOP node's work items are processed by
	// FHoudiniPDGManager based on if any Loaded work result objects are found
	UPROPERTY(NonTransactional)
	bool bCachedHaveLoadedWorkResults;

	// True if this node has child nodes
	UPROPERTY(NonTransactional)
	bool bHasChildNodes;

	// These notification events have been introduced so that we can start encapsulating code.
	// in this class as opposed to modifying this object in various places throughout the codebase.

	// Notification that this TOP node has been dirtied.
	void OnDirtyNode();

	// Accessors for the landscape data caches
	FHoudiniLandscapeExtent& GetLandscapeExtent() { return LandscapeExtent; }
	FHoudiniLandscapeReferenceLocation& GetLandscapeReferenceLocation() { return LandscapeReferenceLocation; }
	FHoudiniLandscapeTileSizeInfo& GetLandscapeSizeInfo() { return LandscapeSizeInfo; }
	// More cached landscape data
	UPROPERTY()
	TSet<FString> ClearedLandscapeLayers;

	// Returns true if the node has received the HAPI_PDG_EVENT_COOK_COMPLETE event since the last the cook started 
	bool HasReceivedCookCompleteEvent() const { return bHasReceivedCookCompleteEvent; }
	// Handler for when the node receives the HAPI_PDG_EVENT_COOK_START (called for each node when a TOPNet starts cooking)
	void HandleOnPDGEventCookStart() { bHasReceivedCookCompleteEvent = false; }
	// Handler for when the node receives the HAPI_PDG_EVENT_COOK_COMPLETE event (called for each node when a TOPNet completes cooking)
	void HandleOnPDGEventCookComplete() { bHasReceivedCookCompleteEvent = true; }

protected:
	void InvalidateLandscapeCache();
	
	// Value caches used during landscape tile creation.
	FHoudiniLandscapeReferenceLocation LandscapeReferenceLocation;
	FHoudiniLandscapeTileSizeInfo LandscapeSizeInfo;
	FHoudiniLandscapeExtent LandscapeExtent;

	// Visible in the level
	UPROPERTY()
	bool					bShow;

	// Map of [work_result_index]_[work_result_object_index] to the work result object's baked outputs. 
	UPROPERTY()
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput> BakedWorkResultObjectOutputs;

	// This node's own work items, used when bHasChildNodes is false.
	UPROPERTY(Transient, NonTransactional)
	FWorkItemTally			WorkItemTally;
	// This node's aggregated work item tallys (sum of child work item tally, use when bHasChildNodes is true)
	UPROPERTY(Transient, NonTransactional)
	FAggregatedWorkItemTally	AggregatedWorkItemTally;

	// Set to true when the node recieves HAPI_PDG_EVENT_COOK_COMPLETE event
	UPROPERTY(Transient, NonTransactional)
	bool bHasReceivedCookCompleteEvent;

private:
	UPROPERTY()
	FOutputActorOwner OutputActorOwner;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UTOPNetwork : public UObject
{
	GENERATED_BODY()

public:

	// Delegate that is broadcast when cook of the network is complete. Parameters are the UTOPNetwork and bAnyFailedWorkItems.
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostCookDelegate, UTOPNetwork*, const bool);

	// Constructor
	UTOPNetwork();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const UTOPNetwork& Other) const;

	// Sets all WorkResultObjects that are in the Loaded state to ToDelete (will delete output objects and output
	// actors).
	void SetLoadedWorkResultsToDelete();

	// Immediately delete Loaded work result output objects (keeps the work items and result arrays but deletes the output
	// objects and actors and sets the state to Deleted.
	void DeleteAllWorkResultObjectOutputs();

	// Returns true if any node in this TOP net has pending (waiting, scheduled, cooking) work items.
	bool AnyWorkItemsPending() const;

	// Returns true if any node in this TOP net has failed/errored work items.
	bool AnyWorkItemsFailed() const;

	// Returns true if this network has nodes that can still be auto-baked
	bool CanStillBeAutoBaked() const;

	// Handler for when a node in the newtork receives the HAPI_PDG_EVENT_COOK_COMPLETE event (called for each node when a TOPNet completes cooking)
	void HandleOnPDGEventCookCompleteReceivedByChildNode(UHoudiniPDGAssetLink* const InAssetLink, UTOPNode* const InTOPNode);

	FOnPostCookDelegate& GetOnPostCookDelegate() { return OnPostCookDelegate; }
	
public:

	UPROPERTY(Transient, NonTransactional)
	int32				NodeId;
	UPROPERTY(NonTransactional)
	FString				NodeName;
	// HAPI path to this node (relative to the HDA)
	UPROPERTY(NonTransactional)
	FString				NodePath;

	UPROPERTY()
	TArray<UTOPNode*>	AllTOPNodes;

	// TODO: Should be using SelectedNodeName instead?
	// Index is not consistent after updating filter
	UPROPERTY()
	int32				SelectedTOPIndex;
	
	UPROPERTY(NonTransactional)
	FString 			ParentName;

	UPROPERTY()
	bool				bShowResults;
	UPROPERTY()
	bool				bAutoLoadResults;

	FOnPostCookDelegate OnPostCookDelegate;
};


class UHoudiniPDGAssetLink;
DECLARE_MULTICAST_DELEGATE_FourParams(FHoudiniPDGAssetLinkWorkResultObjectLoaded, UHoudiniPDGAssetLink*, UTOPNode*, int32 /*WorkItemArrayIndex*/, int32 /*WorkItemResultInfoIndex*/);

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPDGAssetLink : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	friend class UHoudiniAssetComponent;

	// Delegate for when the entire bake operation is complete (all selected nodes/networks have been baked).
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostBakeDelegate, UHoudiniPDGAssetLink*, const bool);
	// Delegate for when a network completes a cook. Passes the asset link, the network, a bAnyWorkItemsFailed.
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPostTOPNetworkCookDelegate, UHoudiniPDGAssetLink*, UTOPNetwork*, const bool);
	
	static FString GetAssetLinkStatus(const EPDGLinkState& InLinkState);
	static FString GetTOPNodeStatus(const UTOPNode* InTOPNode);
	static FLinearColor GetTOPNodeStatusColor(const UTOPNode* InTOPNode);

	void UpdateTOPNodeWithChildrenWorkItemTallyAndState(UTOPNode* InNode, UTOPNetwork* InNetwork);
	void UpdateWorkItemTally();
	static void ResetTOPNetworkWorkItemTally(UTOPNetwork* TOPNetwork);

	// Set the TOP network at the given index as currently selected TOP network
	void SelectTOPNetwork(const int32& AtIndex);
	// Set the TOP node at the given index in the given TOP network as currently selected TOP node
	void SelectTOPNode(UTOPNetwork* InTOPNetwork, const int32& AtIndex);

	UTOPNode* GetSelectedTOPNode();
	const UTOPNode* GetSelectedTOPNode() const;
	UTOPNetwork* GetSelectedTOPNetwork();
	const UTOPNetwork* GetSelectedTOPNetwork() const;
	
	FString GetSelectedTOPNodeName();
	FString GetSelectedTOPNetworkName();

	UTOPNode* GetTOPNode(const int32& InNodeID);
	bool GetTOPNodeAndNetworkByNodeId(const int32& InNodeID, UTOPNetwork*& OutNetwork, UTOPNode*& OutNode);
	UTOPNetwork* GetTOPNetwork(const int32& AtIndex);
	const UTOPNetwork* GetTOPNetwork(const int32& AtIndex) const;

	// Find the node with relative path 'InNodePath' from its topnet.
	static UTOPNode* GetTOPNodeByNodePath(const FString& InNodePath, const TArray<UTOPNode*>& InTOPNodes, int32& OutIndex);
	// Find the network with relative path 'InNetPath' from the HDA
	static UTOPNetwork* GetTOPNetworkByNodePath(const FString& InNodePath, const TArray<UTOPNetwork*>& InTOPNetworks, int32& OutIndex);

	// Get the parent TOP node of the specified node. This is resolved 
	UTOPNode* GetParentTOPNode(const UTOPNode* InNode);

	static void ClearTOPNodeWorkItemResults(UTOPNode* TOPNode);
	static void ClearTOPNetworkWorkItemResults(UTOPNetwork* TOPNetwork);
	// Clear the result objects of a work item (FTOPWorkResult.ResultObjects), but don't delete the work item from
	// TOPNode.WorkResults (for example, the work item was dirtied but not removed from PDG)
	static void ClearWorkItemResultByID(const int32& InWorkItemID, UTOPNode* InTOPNode);
	// Calls ClearWorkItemResultByID and then deletes the FTOPWorkResult from InTOPNode.Result as well. For example:
	// the work item was removed in PDG.
	static void DestroyWorkItemByID(const int32& InWorkItemID, UTOPNode* InTOPNode);
	static FTOPWorkResult* GetWorkResultByID(const int32& InWorkItemID, UTOPNode* InTOPNode);

	// This should be called after the owner and this PDG asset link is duplicated. Set all output parent actors to
	// null in all TOP networks/nodes. Since the TOP Networks/TOP nodes are all structs, we cannot set
	// DuplicateTransient property on their OutputActor properties.
	void UpdatePostDuplicate();

	// Load the geometry generated as results of the given work item, of the given TOP node.
	// The load will be done asynchronously.
	// Results must be tagged with 'file', and must have a file path, otherwise will not be loaded.
	//void LoadResults(FTOPNode TOPNode, HAPI_PDG_WorkitemInfo workItemInfo, HAPI_PDG_WorkitemResultInfo[] resultInfos, HAPI_PDG_WorkitemId workItemID)

	// Return the first UHoudiniAssetComponent in the parent chain. If this asset link is not
	// owned by a HoudiniAssetComponent, a nullptr will be returned.
	UHoudiniAssetComponent* GetOuterHoudiniAssetComponent() const;

	// Helper function to get the GUID of the owning HoudiniAssetComponent. Returns an invalid FGuid if
	// GetOuterHoudiniAssetComponent() returns null.
	FGuid GetOuterHoudiniComponentGuid() const;

	// Gets the temporary cook folder. If the parent of this asset link is a HoudiniAssetComponent use that, otherwise
	// use the default static mesh temporary cook folder.
	FDirectoryPath GetTemporaryCookFolder() const;

	// Get the actor that owns this PDG asset link. If the asset link is owned by a component,
	// then the component's owning actor is returned. Can return null if this is now owned by
	// an actor or component.
	AActor* GetOwnerActor() const;

	// Checks if the asset link has any temporary outputs and returns true if it has
	bool HasTemporaryOutputs() const;

	// Filter TOP nodes and outputs (hidden/visible) by TOPNodeFilter and TOPOutputFilter.
	void FilterTOPNodesAndOutputs();

	// On all FTOPNodes: Load not loaded items if bAutoload is true, and update the level visibility of work items
	// result. Used when FTOPNode.bShow and/or FTOPNode.bAutoload changed.
	void UpdateTOPNodeAutoloadAndVisibility();

#if WITH_EDITORONLY_DATA
	// Returns true if there are any nodes left that can/must still be auto-baked.
	bool AnyRemainingAutoBakeNodes() const;
#endif

	// Delegate handlers

	// Get the post bake delegate
	FOnPostBakeDelegate& GetOnPostBakeDelegate() { return OnPostBakeDelegate; }
	
	// Called by baking code after baking all of the outputs
	void HandleOnPostBake(const bool bInSuccess);

	FOnPostTOPNetworkCookDelegate& GetOnPostTOPNetworkCookDelegate() { return OnPostTOPNetworkCookDelegate; }

	// Handler for when a TOP network completes a cook. Called by the TOP Net once all of its nodes have received
	// HAPI_PDG_EVENT_COOK_COMPLETE.
	void HandleOnTOPNetworkCookComplete(UTOPNetwork* const InTOPNet);

#if WITH_EDITORONLY_DATA
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITORONLY_DATA
	void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

private:

	void ClearAllTOPData();
	
public:

	//UPROPERTY()
	//UHoudiniAsset*				HoudiniAsset;

	//UPROPERTY()
	//UHoudiniAssetComponent*		ParentHAC;

	UPROPERTY(DuplicateTransient, NonTransactional)
	FString						AssetName;

	// The full path to the HDA in HAPI
	UPROPERTY(DuplicateTransient, NonTransactional)
	FString						AssetNodePath;

	UPROPERTY(DuplicateTransient, NonTransactional)
	int32						AssetID;

	UPROPERTY()
	TArray<UTOPNetwork*>		AllTOPNetworks;

	UPROPERTY()
	int32						SelectedTOPNetworkIndex;

	UPROPERTY(Transient, NonTransactional)
	EPDGLinkState				LinkState;

	UPROPERTY()
	bool						bAutoCook;
	UPROPERTY()
	bool						bUseTOPNodeFilter;
	UPROPERTY()
	bool						bUseTOPOutputFilter;
	UPROPERTY()
	FString						TOPNodeFilter;
	UPROPERTY()
	FString						TOPOutputFilter;

	UPROPERTY(NonTransactional)
	int32						NumWorkitems;
	UPROPERTY(Transient, NonTransactional)
	FAggregatedWorkItemTally		WorkItemTally;

	UPROPERTY()
	FString						OutputCachePath;

	UPROPERTY(Transient)
	bool						bNeedsUIRefresh;

	// A parent actor to serve as the parent of any output actors
	// that are created.
	// If null, then output actors are created under a folder
	UPROPERTY(EditAnywhere, Category="Output")
	AActor*					 	OutputParentActor;

	// Folder used for baking PDG outputs
	UPROPERTY()
	FDirectoryPath BakeFolder;

	//
	// Notifications
	//

	// Delegate that is broadcast when a work result object has been loaded
	FHoudiniPDGAssetLinkWorkResultObjectLoaded OnWorkResultObjectLoaded;

	// Delegate that is broadcast after a bake.
	FOnPostBakeDelegate OnPostBakeDelegate;

	// Delegate that is broadcast after a TOP Network completes a cook.
	FOnPostTOPNetworkCookDelegate OnPostTOPNetworkCookDelegate;

	//
	// End: Notifications
	//

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bBakeMenuExpanded;

	// What kind of output to bake, for example, bake actors, bake to blueprint
	UPROPERTY()
	EHoudiniEngineBakeOption HoudiniEngineBakeOption;

	// Which outputs to bake, for example, all, selected network, selected node
	UPROPERTY()
	EPDGBakeSelectionOption PDGBakeSelectionOption;

	// This determines if the baked assets should replace existing assets with the same name,
	// or always generate new assets (with numerical suffixes if needed to create unique names)
	UPROPERTY()
	EPDGBakePackageReplaceModeOption PDGBakePackageReplaceMode;

	// If true, recenter baked actors to their bounding box center after bake
	UPROPERTY()
	bool bRecenterBakedActors;

	// Auto-bake: if this is true, it indicates that once all work result objects for the node is loaded they should
	// all be baked 
	UPROPERTY()
	bool bBakeAfterAllWorkResultObjectsLoaded;

	// The delegate handle of the auto bake helper function bound to OnWorkResultObjectLoaded.
	FDelegateHandle AutoBakeDelegateHandle;
#endif
};
