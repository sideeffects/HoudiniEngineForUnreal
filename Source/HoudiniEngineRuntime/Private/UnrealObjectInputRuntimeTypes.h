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

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "HoudiniEngineRuntimeCommon.h"
#include "HoudiniInputTypes.h"

// Houdini Engine forward declarations
class FUnrealObjectInputModifier;
struct FHoudiniInputObjectSettings;
enum class EHoudiniInputObjectType : uint8;

// UE forward declarations
class UActorComponent;
class ULandscapeComponent;


/**
 * This class wraps a HAPI_NodeId and the Unique Houdini Node Id. The IsValid() function (via the manager) confirms
 * that HAPINodeId is still a valid node in the Houdini session, and that that node's unique Houdini Node Id matches
 * the one cached in the instance of this class. HAPI Node Ids can be reused if nodes are deleted, so this added
 * validation is necessary.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputHAPINodeId
{
public:
	/** Constructs a new invalid node id */
	FUnrealObjectInputHAPINodeId() : HAPINodeId(-1), UniqueHoudiniNodeId(-1) {}

	/** Get the HAPI Node Id */
	int32 GetHAPINodeId() const { return HAPINodeId; }
	/** Get the unique Houdini node Id that was cached the last time this Id was set. */
	int32 GetUniqueHoudiniNodeId() const { return UniqueHoudiniNodeId; }

	/** Set both the HAPI node id and unique Houdini node id. */
	bool Set(const int32 InHAPINodeId, const int32 InUniqueHoudiniNodeId);
	/** Set the HAPI Node Id and fetch the unique Houdini node Id from Houdini and cache it. */
	bool Set(const int32 InHAPINodeId);

	/** Reset this id to be invalid. */
	void Reset() { HAPINodeId = -1; UniqueHoudiniNodeId = -1; }

	/** Returns true if the both id values are >= 0. */
	bool IsSet() const;
	/**
	 * Uses the manager to check that HAPINodeId is a valid node in the Houdini session, and that that node's unique
	 * Houdini Node Id matches the cached UniqueHoudiniNodeId.
	 */
	bool IsValid() const;
	
private:
	int32 HAPINodeId;
	int32 UniqueHoudiniNodeId;
};

/**
 * A struct of options that are used by FUnrealObjectInputIdentifier to differentiate between variations of the
 * same object.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputOptions
{
public:
	FUnrealObjectInputOptions();

	static FUnrealObjectInputOptions MakeOptionsForPackedLevelActor(const FHoudiniInputObjectSettings& InInputSettings);
	
	static FUnrealObjectInputOptions MakeOptionsForLevelInstanceActor(const FHoudiniInputObjectSettings& InInputSettings);
	
	static FUnrealObjectInputOptions MakeOptionsForLandscapeActor(
		const FHoudiniInputObjectSettings& InInputSettings, 
		const TSet<TObjectPtr<ULandscapeComponent>>* InSelectedComponents=nullptr);
	
	static FUnrealObjectInputOptions MakeOptionsForLandscapeData(
		const FHoudiniInputObjectSettings& InInputSettings, 
		const TSet<TObjectPtr<ULandscapeComponent>>* InSelectedComponents=nullptr);

	static FUnrealObjectInputOptions MakeOptionsForLandscapeSplineActor(const FHoudiniInputObjectSettings& InInputSettings);

	static FUnrealObjectInputOptions MakeOptionsForMesh(const FHoudiniInputObjectSettings& InInputSettings);

	static FUnrealObjectInputOptions MakeOptionsForGenericActor(const FHoudiniInputObjectSettings& InInputSettings);

	void SetBoolOptions(const TMap<FName, bool>& InBoolOptions) { BoolOptions = InBoolOptions; ComputeBoolOptionsHash(); }
	void AddBoolOption(const FName InBoolOption, const bool bInValue) { BoolOptions.Add(InBoolOption, bInValue); ComputeBoolOptionsHash(); }
	bool RemoveBoolOption(const FName InBoolOptionToRemove)
	{
		if (BoolOptions.Remove(InBoolOptionToRemove) <= 0)
			return false;

		ComputeBoolOptionsHash();
		return true;
	}
	
	const TMap<FName, bool>& GetBoolOptions() const { return BoolOptions; }

	uint32 GetBoolOptionsHash() const { return BoolOptionsHash; }

	void SetSelectedComponents(const TSet<TWeakObjectPtr<UActorComponent>>& InSelectedComponents);
	void SetSelectedComponents(TSet<TWeakObjectPtr<UActorComponent>>&& InSelectedComponents);

	template <class T>
	void SetSelectedComponents(const TSet<TObjectPtr<T>>& InSelectedComponents);

	const TSet<TWeakObjectPtr<UActorComponent>>& GetSelectedComponents() const { return SelectedComponents; }

	uint32 GetSelectedComponentsHash() const { return SelectedComponentsHash; }
	
	/** Return a suffix to apply to input node name's in Houdini that represent the current options selection. */
	FString GenerateNodeNameSuffix() const;
	
	/** Return hash value for this object, used when using this object as a key inside hashing containers. */
	uint32 GetTypeHash() const;

	/** Hashing containers need the == operator. */
	bool operator==(const FUnrealObjectInputOptions& InOther) const;
	
	bool bImportAsReference;
	bool bImportAsReferenceRotScaleEnabled;
	
	bool bExportMainGeometry;
	bool bExportLODs;
	bool bExportSockets;
	bool bExportColliders;
	bool bMainMeshIsNaniteFallbackMesh;
	bool bExportMaterialParameters;
	bool bAddRotAndScaleAttributesOnCurves;
	bool bUseLegacyInputCurves;
	float UnrealSplineResolution;
	EHoudiniLandscapeExportType LandscapeExportType;
	bool bLandscapeExportMaterials;
	bool bLandscapeExportLighting;
	bool bLandscapeExportNormalizedUVs;
	bool bLandscapeExportTileUVs;
	bool bExportLandscapeSplineControlPoints;
	bool bExportLandscapeSplineLeftRightCurves;
	bool bExportPerEditLayerData;
	bool bExportLevelInstanceContent;
	bool bExportSelectedComponentsOnly;

protected:
	void ComputeBoolOptionsHash();
	void ComputeSelectedComponentsHash();
	
private:
	TMap<FName, bool> BoolOptions;
	uint32 BoolOptionsHash;
	TSet<TWeakObjectPtr<UActorComponent>> SelectedComponents;
	uint32 SelectedComponentsHash;
};

/** Function used by hashing containers to create a unique hash for this type of object. */
inline HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FUnrealObjectInputOptions& InOptions) { return InOptions.GetTypeHash(); }

/**
 * An enum that defines the different node types. Used to differentiate identifiers to the same object for different
 * purposes.
 */
enum class EUnrealObjectInputNodeType : uint8
{
	Invalid,			// Not a valid references.
	Container, 			// Contains other nodes.
	Leaf,				// A geo node with internal nodes.
	LeafWithReferences	// A leaf node that references other leaf nodes (for example a merge node)

	// TODO: Should be able to combine Leaf and LeafWithReferences.
};


/**
 * A class that acts as an identifier of objects in the input system, differentiated by type and import options.
 * It can be constructed from an Object + import options, representing a leaf node (single option variations of a SM
 * for example) or a reference node (merge of a combination of options for a SM, for example).
 *
 * It can also be constructed from an Object, Package or Path to act as a container (analogous to directories, usually
 * represented with subnet Objects).
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputIdentifier
{
public:
	// FUnrealObjectInputIdentifier creates a unique identifier for an HAPI Input Node.
	//	It's basically a path, most likely generated from a UObject Path (though it doesn't have to be). In addition
	// the actual node name may have an additional string appended based off FUnrealObjectInputOptions.
	// Each node has a node type (though really this should be removed, its not needed, they could be merged into one type).

	FUnrealObjectInputIdentifier();
	FUnrealObjectInputIdentifier(const UObject* InObject, const FUnrealObjectInputOptions& InOptions, EUnrealObjectInputNodeType InNodeType);
	FUnrealObjectInputIdentifier(const UObject* InObject, EUnrealObjectInputNodeType InNodeType);
	FUnrealObjectInputIdentifier(const UPackage* InPackage, EUnrealObjectInputNodeType InNodeType);
	FUnrealObjectInputIdentifier(const FString& InPath, EUnrealObjectInputNodeType Type);
	FUnrealObjectInputIdentifier(const FUnrealObjectInputIdentifier& InParent, const FString& ChildName, EUnrealObjectInputNodeType Type);

	void Reset();

	FString GetPath() const;
	FString GetParentPath() const;
	FString GetNodeName() const;
	EUnrealObjectInputNodeType GetNodeType() const;
	FString ToString() const;

	bool MakeParentIdentifier(FUnrealObjectInputIdentifier& OutParentIdentifier) const;

	bool IsValid() const;

	// Functions for hashing.
	uint32 GetTypeHash() const;
	bool operator==(const FUnrealObjectInputIdentifier& InOther) const;
private:
	static FString MakeNormalizedPath(const UObject* InObject);
	static FString NormalizeObjectPath(const FString& InObjectPath);

	FString Path;
	EUnrealObjectInputNodeType NodeType;
};

/** Function used by hashing containers to create a unique hash for this type of object. */
inline HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FUnrealObjectInputIdentifier& InIdentifier) { return InIdentifier.GetTypeHash(); }


/**
 * A reference counting handle that wraps FUnrealObjectInputIdentifier.
 * Upon construction and destruction, if the identifier is valid, the handle automatically increments/decrements the
 * reference count to the entry in the manager via the manager.
 *
 * LeafWithReferences counting is also handled with the assignment operator.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputHandle
{
public:
	FUnrealObjectInputHandle();
	FUnrealObjectInputHandle(const FUnrealObjectInputIdentifier& InIdentifier);
	FUnrealObjectInputHandle(const FUnrealObjectInputHandle& InHandle);

	FUnrealObjectInputHandle(FUnrealObjectInputHandle&& InHandle);

	virtual ~FUnrealObjectInputHandle();

	virtual uint32 GetTypeHash() const;
	bool operator==(const FUnrealObjectInputHandle& InOther) const;
	virtual bool IsValid() const;
	virtual void Reset();
	const FUnrealObjectInputIdentifier& GetIdentifier() const { return Identifier; }
	FUnrealObjectInputHandle& operator=(const FUnrealObjectInputHandle& InOther);
	FUnrealObjectInputHandle& operator=(FUnrealObjectInputHandle&& InOther);

protected:
	virtual bool Initialize(const FUnrealObjectInputIdentifier& InIdentifier);
	virtual void DeInitialize();
	bool bIsInitialized;
	FUnrealObjectInputIdentifier Identifier;

};

/**
 * A reference counting handle derived from FUnrealObjectInputHandle. The back link handle
 * stores the source object as well, so that manager is aware that A is referencing B versus a
 * normal handle where the manager would only be aware that B is being referenced.
 * Upon construction and destruction, if the target identifier is valid, the handle automatically increments/decrements the
 * reference count to the entry in the manager via the manager.
 *
 * LeafWithReferences counting is also handled with the assignment operator.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputBackLinkHandle : public FUnrealObjectInputHandle
{
public:
	FUnrealObjectInputBackLinkHandle();
	FUnrealObjectInputBackLinkHandle(const FUnrealObjectInputIdentifier& InSourceIdentifier, const FUnrealObjectInputIdentifier& InTargetIdentifier);
	FUnrealObjectInputBackLinkHandle(const FUnrealObjectInputBackLinkHandle& InHandle);
	virtual ~FUnrealObjectInputBackLinkHandle();

	virtual uint32 GetTypeHash() const override;
	bool operator==(const FUnrealObjectInputBackLinkHandle& InOther) const;
	virtual void Reset();
	const FUnrealObjectInputIdentifier& GetTargetIdentifier() const { return GetIdentifier(); }
	const FUnrealObjectInputIdentifier& GetSourceIdentifier() const { return SourceIdentifier; }
	FUnrealObjectInputBackLinkHandle& operator=(const FUnrealObjectInputBackLinkHandle& InOther);
	
protected:
	virtual bool Initialize(const FUnrealObjectInputIdentifier& InSourceIdentifier, const FUnrealObjectInputIdentifier& InTargetIdentifier);
	virtual void DeInitialize() override;
	FUnrealObjectInputIdentifier SourceIdentifier;

private:
	virtual bool Initialize(const FUnrealObjectInputIdentifier& InIdentifier) override { return false; }

};

/** Function used by hashing containers to create a unique hash for this type of object. */
inline HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FUnrealObjectInputHandle& InHandle) { return InHandle.GetTypeHash(); }


enum class EUnrealObjectInputModifierType : uint8
{
	Invalid,
	MaterialOverrides,
	PhysicalMaterialOverride,
	ActorAsReference,
	DataLayerGroups,
	HLODAttributes,
	ActorProperties,
	CustomPrimitiveData
};

/** Represents a chain of FUnrealObjectInputModifiers, owned by a single FUnrealObjectInputNode. See FUnrealObjectInputModifier. */
struct HOUDINIENGINERUNTIME_API FUnrealObjectInputModifierChain
{
	/** The node to connect to the input of the first node of the first modifiers. */ 
	FUnrealObjectInputHAPINodeId ConnectToNodeId;

	/** The modifiers of this chain. */
	TArray<FUnrealObjectInputModifier*> Modifiers;
};

/**
 * The base class for entries in the FUnrealObjectInputManager.
 * 
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputNode
{
public:
	static const FName OutputChainName;

	/** Do not allow construction without an identifier. */
	FUnrealObjectInputNode() = delete;

	/** Construct with only the identifier. */
	FUnrealObjectInputNode(const FUnrealObjectInputIdentifier& InIdentifier);
	/** Construct with identifier, parent and optionally HAPI node id. */
	FUnrealObjectInputNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InNodeId=INDEX_NONE);

	/** Destructor: deletes the HAPI node, if any, that is associated with this node. */
	virtual ~FUnrealObjectInputNode();

	const FUnrealObjectInputIdentifier& GetIdentifier() const { return Identifier; }
	const FUnrealObjectInputHandle& GetParent() const { return Parent; }

	/** Get the HAPI node id for this node. This can be -1 if no node has yet been created. */
	int32 GetHAPINodeId() const { return NodeId.IsValid() ? NodeId.GetHAPINodeId() : -1; }
	FUnrealObjectInputHAPINodeId GetNodeId() const { return NodeId; }
	bool SetHAPINodeId(const int32 InHAPINodeId) { return NodeId.Set(InHAPINodeId); }
	void SetNodeId(const FUnrealObjectInputHAPINodeId& InNodeId) { NodeId = InNodeId; }

	virtual bool IsDirty() const { return bIsDirty; }
	virtual void MarkAsDirty() { bIsDirty = true; }
	virtual void ClearDirtyFlag() { bIsDirty = false; }

	virtual bool IsRefCounted() const { return false; }
	virtual uint32 GetRefCount() const { return static_cast<uint32>(ReferenceCount); }
	virtual bool AreHAPINodesValid() const;
	virtual bool DeleteHAPINodes();
	virtual void GetHAPINodeIds(TArray<int32>& OutHAPINodeIds) const;
	virtual void GetHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const;
	virtual bool CanBeDeleted() const { return bCanBeDeleted; }
	virtual void SetCanBeDeleted(const bool& InCanBeDeleted) { bCanBeDeleted = InCanBeDeleted; }

	bool AddModifierChain(FName InChainName, int32 InNodeIdToConnectTo);
	bool AddModifierChain(FName InChainName, const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo);
	bool SetModifierChainNodeToConnectTo(FName InChainName, int32 InNodeToConnectTo);
	bool SetModifierChainNodeToConnectTo(FName InChainName, const FUnrealObjectInputHAPINodeId& InNodeToConnectTo);
	int32 GetNumModifierChains() const { return ModifierChains.Num(); }
	const FUnrealObjectInputModifierChain* GetModifierChain(FName InChainName) const { return ModifierChains.Find(InChainName); }
	int32 GetInputHAPINodeIdOfModifierChain(FName InChainName) const;
	FUnrealObjectInputHAPINodeId GetInputNodeIdOfModifierChain(FName InChainName) const;
	int32 GetOutputHAPINodeIdOfModifierChain(FName InChainName) const;
	FUnrealObjectInputHAPINodeId GetOutputNodeIdOfModifierChain(FName InChainName) const;
	bool RemoveModifierChain(FName InChainName);

	template <class T, class... Args>
	T* CreateAndAddModifier(FName InChainName, Args... ConstructorArguments);

	bool AddModifier(FName InChainName, FUnrealObjectInputModifier* InModifierToAdd);
	FUnrealObjectInputModifier* FindFirstModifierOfType(FName InChainName, EUnrealObjectInputModifierType InModifierType) const;
	bool GetAllModifiersOfType(FName InChainName, EUnrealObjectInputModifierType InModifierType, TArray<FUnrealObjectInputModifier*>& OutModifiers) const;
	bool DestroyModifier(FName InChainName, FUnrealObjectInputModifier* InModifier);
	bool DestroyModifiers(FName InChainName);

	bool DestroyAllModifierChains();

	const TMap<FName, FUnrealObjectInputModifierChain>& GetModifierChains() const { return ModifierChains; }
	bool UpdateModifiers(FName InChainName);
	bool UpdateAllModifierChains();
	
protected:
	friend class FUnrealObjectInputManagerImpl;

	void SetParent(const FUnrealObjectInputHandle& InParent) { Parent = InParent; }
	virtual bool AddRef() const;
	virtual bool RemoveRef() const;
	
private:
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle Parent;
	FUnrealObjectInputHAPINodeId NodeId;
	bool bIsDirty;
	mutable int32 ReferenceCount;
	bool bCanBeDeleted;
	TMap<FName, FUnrealObjectInputModifierChain> ModifierChains;
};


/**
 * A container node: a node that acts as a parent node to other nodes, usually a Subnet object.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputContainerNode : public FUnrealObjectInputNode
{
public:
	FUnrealObjectInputContainerNode(const FUnrealObjectInputIdentifier& InIdentifier);
	FUnrealObjectInputContainerNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InNodeId);

	virtual bool IsRefCounted() const override { return true; }
};


/**
 * A leaf node: a node that represents an object/asset from Unreal. For example, a locked null SOP with geometry sent
 * from Unreal for a static mesh.
 * The identifier must have Options set.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputLeafNode : public FUnrealObjectInputNode
{
public:
	FUnrealObjectInputLeafNode(const FUnrealObjectInputIdentifier& InIdentifier);
	FUnrealObjectInputLeafNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId);
	virtual ~FUnrealObjectInputLeafNode();

	virtual bool IsRefCounted() const override { return true; }
	int32 GetObjectHAPINodeId() const { return ObjectNodeId.GetHAPINodeId(); }
	bool SetObjectHAPINodeId(const int32 InObjectNodeId) { return ObjectNodeId.Set(InObjectNodeId); }
	FUnrealObjectInputHAPINodeId GetObjectNodeId() const { return ObjectNodeId; }
	void SetObjectNodeId(const FUnrealObjectInputHAPINodeId& InObjectNodeId) { ObjectNodeId = InObjectNodeId; }
	virtual bool AreHAPINodesValid() const override;
	virtual bool DeleteHAPINodes() override;
	virtual void GetHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const override;
private:
	/** The HAPI object node id for this node, for example a geo Object. */
	FUnrealObjectInputHAPINodeId ObjectNodeId;
};


/**
 * A reference node, a node that references leaf nodes. For example, a node for a static mesh asset that references
 * specific nodes for main mesh, sockets, lods and colliders.
 * The identifier must have Options set.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputReferenceNode : public FUnrealObjectInputLeafNode
{
public:
	FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier);
	FUnrealObjectInputReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const FUnrealObjectInputHandle& InParent,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const int32 InReferencesConnectToNodeId=INDEX_NONE);
	FUnrealObjectInputReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const FUnrealObjectInputHandle& InParent, 
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const TSet<FUnrealObjectInputHandle>& InReferencedNodes,
		const int32 InReferencesConnectToNodeId=INDEX_NONE);

	virtual bool IsRefCounted() const override { return true; }

	bool AddReferencedNode(const FUnrealObjectInputHandle& InHandle);
	bool RemoveReferencedNode(const FUnrealObjectInputHandle& InHandle) { return ReferencedNodes.Remove(FUnrealObjectInputBackLinkHandle(GetIdentifier(), InHandle.GetIdentifier())) > 0; }
	void SetReferencedNodes(const TSet<FUnrealObjectInputHandle>& InReferencedNodes);
	const TSet<FUnrealObjectInputBackLinkHandle>& GetReferencedNodes() const { return ReferencedNodes; }
	void GetReferencedNodes(TSet<FUnrealObjectInputBackLinkHandle>& OutReferencedNodes) const { OutReferencedNodes = ReferencedNodes; }
	void GetReferencedNodes(TSet<FUnrealObjectInputHandle>& OutReferencedNodes) const;
	virtual bool AreReferencedHAPINodesValid() const;
	virtual void MarkAsDirty(bool bInAlsoDirtyReferencedNodes);
	FUnrealObjectInputHAPINodeId GetReferencesConnectToNodeId() const
	{
		if (!ReferencesConnectToNodeId.IsSet())
			return GetNodeId();
		return ReferencesConnectToNodeId.GetValue();
	}

	void SetReferencesConnectToNodeId(const FUnrealObjectInputHAPINodeId& InReferencesConnectToNodeId) { ReferencesConnectToNodeId = InReferencesConnectToNodeId; }
	void SetReferencesConnectToNodeId(const int32 InReferencesConnectToNodeId);
	void ResetReferencesConnectToNodeId() { ReferencesConnectToNodeId.Reset(); }

private:
	virtual void MarkAsDirty() override { FUnrealObjectInputLeafNode::MarkAsDirty(); }
	TSet<FUnrealObjectInputBackLinkHandle> ReferencedNodes;
	TOptional<FUnrealObjectInputHAPINodeId> ReferencesConnectToNodeId;
};


/**
 * Modifiers represent additional nodes that are applied to input nodes. For example, adding a wrangle for setting
 * material overrides for an input node that represents a StaticMeshComponent.
 *
 * The modifiers are added to chains (FUnrealObjectInputModifierChain) and the chains and the modifiers are owned
 * by a FUnrealObjectInputNode.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputModifier
{
public:
	FUnrealObjectInputModifier() = delete;
	FUnrealObjectInputModifier(const FUnrealObjectInputModifier& InCopyFrom) = delete;
	FUnrealObjectInputModifier(FUnrealObjectInputNode& InOwner) : bNeedsRebuild(true), Owner(InOwner) {}
	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::Invalid; }
	virtual EUnrealObjectInputModifierType GetType() const { return StaticGetType(); }

	virtual ~FUnrealObjectInputModifier() { DestroyHAPINodes(); }
	FUnrealObjectInputNode& GetOwner() const { return Owner; }
	void MarkAsNeedsRebuild() { bNeedsRebuild = true; }
	bool NeedsRebuild() const { return bNeedsRebuild; }
	const TArray<FUnrealObjectInputHAPINodeId>& GetHAPINodeIds() const { return HAPINodeIds; }
	virtual FUnrealObjectInputHAPINodeId GetInputNodeId() const { return HAPINodeIds.Num() > 0 ? HAPINodeIds[0] : FUnrealObjectInputHAPINodeId(); }
	virtual int32 GetInputHAPINodeId() const { return GetInputNodeId().GetHAPINodeId(); }
	virtual FUnrealObjectInputHAPINodeId GetOutputNodeId() const { return HAPINodeIds.Num() > 0 ? HAPINodeIds.Last() : FUnrealObjectInputHAPINodeId(); }
	virtual int32 GetOutputHAPINodeId() const { return GetOutputNodeId().GetHAPINodeId(); }
	virtual void OnAddedToOwner() {}
	virtual void OnRemovedFromOwner() { DestroyHAPINodes(); }
	bool DestroyHAPINodes();
	virtual bool Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo) = 0;

protected:
	TArray<FUnrealObjectInputHAPINodeId> HAPINodeIds;
	bool bNeedsRebuild;

private:
	FUnrealObjectInputNode& Owner;
};

/**
 * An update scope that registers itself with the manager on construction and removes itself on destruction.
 *
 * While active the manager reports every node that is created or updated to the scope. Identifiers to the nodes are
 * available as a set before destruction of the scope.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputUpdateScope
{
public:
	FUnrealObjectInputUpdateScope();

	~FUnrealObjectInputUpdateScope();

public:

	const TSet<FUnrealObjectInputIdentifier>& GetNodesCreatedOrUpdated() const { return NodesCreatedOrUpdated; }
	const TSet<FUnrealObjectInputIdentifier>& GetNodesDestroyed() const { return NodesDestroyed; }

protected:
	void OnNodeCreatedOrUpdated(const FUnrealObjectInputIdentifier& InIdentifier);
	void OnNodeDestroyed(const FUnrealObjectInputIdentifier& InIdentifier);

private:
	TSet<FUnrealObjectInputIdentifier> NodesCreatedOrUpdated;
	TSet<FUnrealObjectInputIdentifier> NodesDestroyed;

	FDelegateHandle OnCreatedHandle;
	FDelegateHandle OnUpdatedHandle;
	FDelegateHandle OnDestroyedHandle;
};


template <class T>
void FUnrealObjectInputOptions::SetSelectedComponents(const TSet<TObjectPtr<T>>& InSelectedComponents)
{
	static_assert(std::is_base_of<UActorComponent, T>::value, "T must derive from UActorComponent");

	SelectedComponents.Empty(InSelectedComponents.Num());
	for (T* const ActorComponent : InSelectedComponents)
	{
		if (!IsValid(ActorComponent))
			continue;
		SelectedComponents.Add(ActorComponent);
	}
	ComputeSelectedComponentsHash();
}


template <class T, class... Args>
T* FUnrealObjectInputNode::CreateAndAddModifier(const FName InChainName, Args... ConstructorArguments)
{
	static_assert(std::is_base_of<FUnrealObjectInputModifier, T>::value, "T must derive from FUnrealObjectInputModifier");

	FUnrealObjectInputModifierChain* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return nullptr; 
	
	T* Modifier = new T(*this, ConstructorArguments...);
	check(Modifier);
	
	Chain->Modifiers.Emplace(Modifier);
	
	Modifier->OnAddedToOwner();

	return Modifier;
}
