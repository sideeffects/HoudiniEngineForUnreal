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
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

// Houdini Engine forward declarations
class FUnrealObjectInputModifier;

// UE forward declarations


/**
 * A struct of options that are used by FUnrealObjectInputIdentifier to differentiate between variations of the
 * same object.
 */
struct HOUDINIENGINERUNTIME_API FUnrealObjectInputOptions
{
	FUnrealObjectInputOptions(
		const bool bImportAsReference=false,
		const bool bImportAsReferenceRotScaleEnabled=false,
		const bool bExportLODs=false,
		const bool bExportSockets=false,
		const bool bExportColliders=false);
	
	/** Return hash value for this object, used when using this object as a key inside hashing containers. */
	uint32 GetTypeHash() const;

	/** Hashing containers need the == operator. */
	bool operator==(const FUnrealObjectInputOptions& InOther) const;

	/**
	 * Indicates that all the input objects are imported to Houdini as references instead of actual geo
	 * (for Geo/World/Asset input types only
	 */
	bool bImportAsReference;

	/** Indicates that whether or not to add the rot / scale attributes for reference imports */
	bool bImportAsReferenceRotScaleEnabled;
	
	/** Indicates that all LODs in the input should be marshalled to Houdini */
	bool bExportLODs;

	/** Indicates that all sockets in the input should be marshalled to Houdini */
	bool bExportSockets;

	/** Indicates that all colliders in the input should be marshalled to Houdini */
	bool bExportColliders;
};

/** Function used by hashing containers to create a unique hash for this type of object. */
inline HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FUnrealObjectInputOptions& InOptions) { return InOptions.GetTypeHash(); }

/**
 * An enum that defines the different node types. Used to differentiate identifiers to the same object for different
 * purposes.
 */
enum class EUnrealObjectInputNodeType : uint8
{
	Invalid,
	/** A node that contains other nodes. */
	Container,
	/** A node that references (or merges) other leaf nodes. */
	Reference,
	/** A node that contains an object/asset, such as static mesh geometry. */
	Leaf
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
	/** Construct a blank, invalid identifier */
	FUnrealObjectInputIdentifier();

	/**
	 * Construct an identifier to a leaf or reference node for an object with certain import options.
	 * @param InObject The object to construct an identifier for.
	 * @param InOptions The import options. Typically this would be a combination of options (such as LODs + Sockets)
	 *					for a reference node, or a single option or no options for a leaf node.
	 * @param bIsLeaf If true, this is an identifier ot a leaf node, otherwise it is an identifier to a reference node.
	 */
	FUnrealObjectInputIdentifier(UObject const* const InObject, const FUnrealObjectInputOptions& InOptions, const bool bIsLeaf);

	/**
	 * Construct an identifier for a container associated with InObject. This is useful for actors or blueprints, that
	 * that do not directly contain geo but are objects that contain other nodes with geo (such as static mesh
	 * components).
	 */
	FUnrealObjectInputIdentifier(UObject const* const InObject);

	/** Construct an identifier for a container associated with a package. Analogous to directories. */
	FUnrealObjectInputIdentifier(UPackage const* const InPackage);
	
	/** Construct an identifier for a container associated with a path. Analogous to directories. */
	FUnrealObjectInputIdentifier(const FName& InPath);

	/** Construct from a handle (copies the handle's identifier). */
	FUnrealObjectInputIdentifier(const class FUnrealObjectInputHandle& InHandle);

	/**
	 * Returns true if the identifier is valid.
	 * A leaf identifier is valid if it has a valid Object.
	 * A reference identifier is valid if it has a valid Object.
	 * A container is valid if it has a valid Object or non-empty Path.
	 */
	bool IsValid() const;

	/** Reset/clear the identifier. It makes the identifier blank/invalid (same state as calling the default constructor). */
	void Reset();

	/** Return hash value for this object, used when using this object as a key inside hashing containers. */
	uint32 GetTypeHash() const;

	/** Hashing containers need the == operator. */
	bool operator==(const FUnrealObjectInputIdentifier& InOther) const;

	/**
	 * Creates an identifier for the parent of this node.
	 * @param OutIdentifier Set to the parent identifier.
	 * @return true if a valid parent identifier could be built.
	 */
	bool MakeParentIdentifier(FUnrealObjectInputIdentifier& OutParentIdentifier) const;

	/** Returns the UObject that this identifier is associated with, if Object was set during construction. */
	const UObject* GetObject() const { return Object.Get(); }

	/** Returns the Path this identifier is associated with, if Path was set during construction. */
	FName GetPath() const { return Path; }

	/** Helper to return object paths with . and : replaced by / */
	static FString NormalizeObjectPath(const FString& InObjectPath) { return InObjectPath.Replace(TEXT("."), TEXT("/")).Replace(TEXT(":"), TEXT("/")); }

	/** Helper to return object paths with . and : replaced by / */
	static FName NormalizeObjectPath(const FName& InObjectPath) { return FName(NormalizeObjectPath(InObjectPath.ToString())); }

	/** Get the object path. This is the full path of Object if it's valid, otherwise this returns Path. */
	FName GetObjectPath() const { return Object.IsValid() ? FName(Object->GetPathName()) : Path; }

	/** Get the normalized object path. See NormalizeObjectPath() */
	FName GetNormalizedObjectPath() const { return NormalizeObjectPath(GetObjectPath()); }

	/** Gets the node type this identifier represents. See EUnrealObjectInputNodeType. */ 
	EUnrealObjectInputNodeType GetNodeType() const { return NodeType; }

	/** Gets the Options that forms part of this identifier. */
	const FUnrealObjectInputOptions& GetOptions() const { return Options; }

private:
	/** The object this identifier is associated with. */
	TWeakObjectPtr<const UObject> Object;

	/** The path this identifier is associated, for cases where it does not reference an object. */
	FName Path;

	/** The import options associated with the Object of this identifier. */
	FUnrealObjectInputOptions Options;

	/** The type of node this identifier represents. */
	EUnrealObjectInputNodeType NodeType;
};

/** Function used by hashing containers to create a unique hash for this type of object. */
inline HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FUnrealObjectInputIdentifier& InIdentifier) { return InIdentifier.GetTypeHash(); }


/**
 * A reference counting handle that wraps FUnrealObjectInputIdentifier.
 * Upon construction and destruction, if the identifier is valid, the handle automatically increments/decrements the
 * reference count to the entry in the manager via the manager.
 *
 * Reference counting is also handled with the assignment operator.
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputHandle
{
public:
	/** Construct and empty invalid handle. */
	FUnrealObjectInputHandle();
	/**
	 * Construct a handle for InIdentifier. Increments the reference count to the entry in the manager if the
	 * identifier is valid.
	 */
	FUnrealObjectInputHandle(const FUnrealObjectInputIdentifier& InIdentifier);
	/**
	 * Copies InHandle. Increments the reference count to the entry in the manager if the
	 * identifier is valid.
	 */
	FUnrealObjectInputHandle(const FUnrealObjectInputHandle& InHandle);

	/** Destructor: decrements reference count via the manager if the handle is valid. */
	virtual ~FUnrealObjectInputHandle();

	/** Return hash value for this object, used when using this object as a key inside hashing containers. */
	uint32 GetTypeHash() const;

	/** Hashing containers need the == operator. */
	bool operator==(const FUnrealObjectInputHandle& InOther) const;

	/** 
	 * Returns true if the handle is valid. A handle is valid if it is initialized (see bIsInitialized) and its
	 * identifier is valid.
	 */
	bool IsValid() const;

	/**
	 * Resets the handle. If the handle is valid the reference count of the entry it points to will be decremented and
	 * the handle will be reset so that it is invalid/points to nothing.
	 */
	void Reset();

	/** Gets the identifier that this handle wraps. */
	const FUnrealObjectInputIdentifier& GetIdentifier() const { return Identifier; }

	/**
	 * Assignment operator. After assignment this handle will wrap the identifier from InOther. If this handle was
	 * initialized, it will decrement the reference count of the previous identifier via the manager. If InOther is
	 * initialized, this handle will increment the reference count to the new identifier.
	 */
	FUnrealObjectInputHandle& operator=(const FUnrealObjectInputHandle& InOther);
	
protected:
	/**
	 * Initialize the handle: check that InIdentifier is valid, assign it internally and increment the reference count
	 * via the manager. If successful, bIsInitialized is set to true.
	 * @return true if successfully initialized.
	 */
	bool Initialize(const FUnrealObjectInputIdentifier& InIdentifier);

	/**
	 * Deinitialize the handle if it was initialized. Decrements the reference count for Identifier (if valid) via
	 * the manager.
	 */
	void DeInitialize();
	
private:
	/** The identifier that is wrapped by this handled and for which reference counts will be incremented/decremented. */
	FUnrealObjectInputIdentifier Identifier;

	/** True if the handle has been successfully initialized and the reference count for Identifier incremented. */
	bool bIsInitialized;
	
};

/** Function used by hashing containers to create a unique hash for this type of object. */
inline HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FUnrealObjectInputHandle& InHandle) { return InHandle.GetTypeHash(); }


enum class EUnrealObjectInputModifierType : uint8
{
	Invalid,
	MaterialOverrides,
	PhysicalMaterialOverride,
	ActorAsReference
};

/** Represents a chain of FUnrealObjectInputModifiers, owned by a single FUnrealObjectInputNode. See FUnrealObjectInputModifier. */
struct HOUDINIENGINERUNTIME_API FUnrealObjectInputModifierChain
{
	/** The node to connect to the input of the first node of the first modifiers. */ 
	int32 ConnectToNodeId = INDEX_NONE;

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

	/** Getter for the identifier of this node. */
	const FUnrealObjectInputIdentifier& GetIdentifier() const { return Identifier; }
	/** Gets a handle to the parent of this node. */
	const FUnrealObjectInputHandle& GetParent() const { return Parent; }

	/** Get the HAPI node id for this node. This can be -1 if no node has yet been created. */
	int32 GetNodeId() const { return NodeId; }
	/** Set the HAPI node id for this node.*/
	void SetNodeId(const int32 InNodeId) { NodeId = InNodeId; }

	/**
	 * Returns true if the node is dirty. That means that even though the node exists, it has been dirtied and
	 * its data should be resent to Houdini at the next opportunity.
	 */
	virtual bool IsDirty() const { return bIsDirty; }

	/** Marks the node as dirty. See IsDirty(). */
	virtual void MarkAsDirty() { bIsDirty = true; }

	/** Clears the dirty flag on the node. See IsDirty(). */
	virtual void ClearDirtyFlag() { bIsDirty = false; }

	/** Returns true if this node is ref counted by the manager. */
	virtual bool IsRefCounted() const { return false; }

	/** Gets the current reference count of this node. */
	virtual uint32 GetRefCount() const { return static_cast<uint32>(ReferenceCount); }

	/** Returns true if the HAPI nodes (NodeId) associated with this node are valid. */
	virtual bool AreHAPINodesValid() const;

	/** Delete the HAPI node(s) of this node, if valid/exists. */
	virtual bool DeleteHAPINodes();

	/** Get the the valid HAPI node ids that this node is using. */
	virtual void GetHAPINodeIds(TArray<int32>& OutNodeIds) const;

	/** Returns true if the node can be deleted. */
	virtual bool CanBeDeleted() const { return bCanBeDeleted; }

	/** Returns true if the node can be deleted. */
	virtual void SetCanBeDeleted(const bool& InCanBeDeleted) { bCanBeDeleted = InCanBeDeleted; }

	/**
	 * Add a new modifier chain to the node.
	 * @param InChainName The name of the chain.
	 * @param InNodeIdToConnectTo The node to connect the first modifier's input to.
	 * @return True if the chain was added (and did not already exist).
	 */
	bool AddModifierChain(FName InChainName, int32 InNodeIdToConnectTo);

	/** Set the node that the first modifier's input should be connected to for the given chain. */
	bool SetModifierChainNodeToConnectTo(FName InChainName, int32 InNodeToConnectTo);

	/** Returns the number of modifier chains of this node. */
	int32 GetNumModifierChains() const { return ModifierChains.Num(); }

	/** Gets a modifier chain by name. Returns nullptr if no such chain exists. */
	const FUnrealObjectInputModifierChain* GetModifierChain(FName InChainName) const { return ModifierChains.Find(InChainName); }

	/** Get the output node of the chain given by InChainName. Returns < 0 if there is no such chain. */
	int32 GetOutputNodeOfModifierChain(FName InChainName) const;

	/** Removes the modifier chain with the name InChainName. Returns True if a chain was removed. */
	bool RemoveModifierChain(FName InChainName);

	/**
	 * Create and add a new modifier to the chain with name InChainName. The node owns the modifier and is responsible
	 * for deleting it (and freeing memory) when appropriate.
	 * @tparam T The class of the modifier, a subclass for FUnrealInputObjectModifier.
	 * @tparam Args The argument types to the constructor of T.
	 * @param InChainName The name of the modifier chain to create the modifier in.
	 * @param ConstructorArguments The arguments to the constructor of the modifier.
	 * @return The newly created and added modifier, or nullptr if the operation failed.
	 */
	template <class T, class... Args>
	T* CreateAndAddModifier(FName InChainName, Args... ConstructorArguments);

	/**
	 * Add a pre-created modifier to the given chain. The node takes over ownership of the modifier and becomes
	 * responsible for deleting it (and freeing memory) when appropriate.
	 */
	bool AddModifier(FName InChainName, FUnrealObjectInputModifier* InModifierToAdd);

	/** Return the first modifier in the chain InChainName of the given InModifierType. Returns nullptr if no such modifier was found. */
	FUnrealObjectInputModifier* FindFirstModifierOfType(FName InChainName, EUnrealObjectInputModifierType InModifierType) const;

	/**
	 * Find the first modifier in the InChainName chain of the given class.
	 * @tparam T The class of the modifier to find.
	 * @param InChainName The name of the chain to search in.
	 * @return The modifier instance or nullptr if none of class T could be found in the chain.
	 */
	template <class T>
	T* FindFirstModifierByClass(FName InChainName) const;

	/**
	 * Sets OutModifiers to contain all modifiers from the chain InChainName with type InModifierType to OutModifiers.
	 * @return True if the chain exists.
	 */
	bool GetAllModifiersOfType(FName InChainName, EUnrealObjectInputModifierType InModifierType, TArray<FUnrealObjectInputModifier*>& OutModifiers) const;
	
	/**
	 * Sets OutModifiers to contain all modifiers from the chain InChainName with class T to OutModifiers.
	 * @tparam T The class of the modifiers to search for.
	 * @return True if the chain exists.
	 */
	template <class T>
	bool GetAllModifiersByClass(FName InChainName, TArray<T*>& OutModifiers) const;

	/**
	 * Remove the given modifier, InModifier, from chain InChainName and delete it. 
	 * @param InChainName The name of the chain that contains InModifier.
	 * @param InModifier The modifier to remove and delete.
	 * @return True if the chain exists and modifier was found, removed and deleted.
	 */
	bool DestroyModifier(FName InChainName, FUnrealObjectInputModifier* InModifier);

	/** Remove and delete all modifiers from the chain InChainName. Returns true if the chain exists. */
	bool DestroyModifiers(FName InChainName);

	/**
	 * Remove and delete all modifiers from all chains and also remove the chains themselves.
	 * @return True if all underlying calls to DestroyModifiers() returned true.
	 */
	bool DestroyAllModifierChains();

	/** Get a const reference to the map of modifier chains. */
	const TMap<FName, FUnrealObjectInputModifierChain>& GetModifierChains() const { return ModifierChains; }

	/**
	 * Call FUnrealObjectInputModifier::Update() on all modifiers in chain InChainName.
	 * @return True if InChainName exists.
	 */
	bool UpdateModifiers(FName InChainName);

	/**
	 * Call UpdateModifiers() on all chains.
	 * @return True if all calls to UpdateModifiers() returned true.
	 */
	bool UpdateAllModifierChains();
	
protected:
	friend class FUnrealObjectInputManagerImpl;

	/** Set the handle to the parent of this node. */
	void SetParent(const FUnrealObjectInputHandle& InParent) { Parent = InParent; }

	/**
	 * Increment the reference count of this node. Returns true if the count was increased. This should not be called
	 * directly, only via the manager.
	 */
	virtual bool AddRef() const;
	
	/**
	 * Decrement the reference count of this node. Returns true if the count was decreased (ie, it was > 0 when the
	 * function was called.
	 * This function does not delete anything if the count reaches 0, use FUnrealObjectInputManager::RemoveRef() for
	 * that.
	 * This should not be called directly, only via the manager.
	 */
	virtual bool RemoveRef() const;
	
private:
	/** The identifier of this node. */
	FUnrealObjectInputIdentifier Identifier;

	/** A handle to the parent of this node. */
	FUnrealObjectInputHandle Parent;

	/** The id of the main HAPI node associated with this node. */
	int32 NodeId;

	/** Indicates if the node is dirty or not. Dirty nodes should have their data resent to Houdini. */
	bool bIsDirty;

	/** The reference count of this node. */
	mutable int32 ReferenceCount;

	/** Indicates if this node can be deleted. Used to prevent the destruction of input nodes created by NodeSync*/
	bool bCanBeDeleted;

	/** Modifiers on this node */
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
	/** Construct via an identifier, parent, HAPI object node id (for example, a geo Object) and HAPI node id (for example a null SOP). */
	FUnrealObjectInputLeafNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId);
	/** Destructor: destroys the HAPI nodes is valid/exists. */
	virtual ~FUnrealObjectInputLeafNode();

	virtual bool IsRefCounted() const override { return true; }

	/** Gets the HAPI object node id for this node. Can be -1 if the node has not yet been created. */
	int32 GetObjectNodeId() const { return ObjectNodeId; }
	/** Sets the HAPI object node id for this node. */
	void SetObjectNodeId(const int32 InObjectNodeId) { ObjectNodeId = InObjectNodeId; }

	virtual bool AreHAPINodesValid() const override;

	virtual bool DeleteHAPINodes() override;

	virtual void GetHAPINodeIds(TArray<int32>& OutNodeIds) const override;
private:
	/** The HAPI object node id for this node, for example a geo Object. */
	int32 ObjectNodeId;
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
	/** Construct via an identifier, parent, HAPI object node id (for example, a geo Object) and HAPI node id (for example a merge SOP). */
	FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId);
	/**
	 * Construct via an identifier, parent, HAPI object node id (for example, a geo Object) and HAPI node id (for
	 * example a merge SOP), and a set of handles for nodes it requires.
	 */
	FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId, const TSet<FUnrealObjectInputHandle>& InReferencedNodes);

	virtual bool IsRefCounted() const override { return true; }

	/**
	 * Add a node to the set of nodes this node references.
	 * @param InHandle Handle to the node to add.
	 * @return if the node was added to the references.
	 */
	inline bool AddReferencedNode(const FUnrealObjectInputHandle& InHandle);

	/**
	 * Remove a node from the node's set of referenced nodes.
	 * @param InHandle Handle to the node to remove.
	 * @return if the node was removed to the references.
	 */
	bool RemoveReferencedNode(const FUnrealObjectInputHandle& InHandle) { return ReferencedNodes.Remove(InHandle) > 0; }

	/** Set the set of nodes that this node references via handles. */
	void SetReferencedNodes(const TSet<FUnrealObjectInputHandle>& InReferencedNodes) { ReferencedNodes = InReferencedNodes; }

	/** Get the set of handles to the nodes that this node references. */
	const TSet<FUnrealObjectInputHandle>& GetReferencedNodes() const { return ReferencedNodes; }

	/** Get the set of handles to the nodes that this node references. */
	void GetReferencedNodes(TSet<FUnrealObjectInputHandle>& OutReferencedNodes) const { OutReferencedNodes = ReferencedNodes; }

	/** Returns true if all of the nodes that this node references have valid HAPI nodes. */
	virtual bool AreReferencedHAPINodesValid() const;

	/** Marks this and optionally its referenced nodes as dirty. See IsDirty(). */
	virtual void MarkAsDirty(bool bInAlsoDirtyReferencedNodes);

private:
	/** Marks this node as dirty. See IsDirty(). Internal use only, prefer MarkAsDirty(bool bInAlsoDirtyReferencedNodes). */
	virtual void MarkAsDirty() override { FUnrealObjectInputLeafNode::MarkAsDirty(); }

	/** The set of nodes that this node references, by handle. */
	TSet<FUnrealObjectInputHandle> ReferencedNodes;
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
	// Don't allow copy constructing since the modifier is linked to HAPI nodes
	FUnrealObjectInputModifier(const FUnrealObjectInputModifier& InCopyFrom) = delete;

	FUnrealObjectInputModifier(FUnrealObjectInputNode& InOwner) : bNeedsRebuild(true), Owner(InOwner) {}

	/** Static function for getting the modifier type as EUnrealObjectInputModifierType enum. */
	static EUnrealObjectInputModifierType StaticGetType() { return EUnrealObjectInputModifierType::Invalid; }
	/** Return the modifier type as EUnrealObjectInputModifierType enum. */
	virtual EUnrealObjectInputModifierType GetType() const { return StaticGetType(); }

	virtual ~FUnrealObjectInputModifier() { DestroyHAPINodes(); }

	/** Returns the FUnrealObjectInputNode that owns this modifier. */ 
	FUnrealObjectInputNode& GetOwner() const { return Owner; }

	/** Mark this modifier as requiring rebuild: at the next update all of its HAPI nodes will be removed and recreated. */
	void MarkAsNeedsRebuild() { bNeedsRebuild = true; }

	/** Returns true if this modifier needs to be rebuilt. See MarkAsNeedsRebuild(). */ 
	bool NeedsRebuild() const { return bNeedsRebuild; }

	/** Get the HAPI nodes that this modifier created. */
	const TArray<int32>& GetHAPINodeIds() const { return HAPINodeIds; }

	/** Return the first node / input node of the modifier. */
	virtual int32 GetInputHAPINodeId() const { return HAPINodeIds.Num() > 0 ? HAPINodeIds[0] : INDEX_NONE; }

	/** Return the last node / output node of the modifier. */
	virtual int32 GetOutputHAPINodeId() const { return HAPINodeIds.Num() > 0 ? HAPINodeIds.Last() : INDEX_NONE; }

	/** This function is called when a modifier is added to its owner FUnrealObjectInputNode, just after creation. */
	virtual void OnAddedToOwner() {}

	/** This function is called when this modifier is removed from its owner. */
	virtual void OnRemovedFromOwner() { DestroyHAPINodes(); }

	/** Destroy all existing / valid nodes in HAPINodeIds and empty HAPINodeIds. */
	bool DestroyHAPINodes();

	/**
	 * Updates the modifier. Creates missing nodes and re-sets all parameters on the nodes.
	 * @param InNodeIdToConnectTo The node to connect to the input of GetInputHAPINodeId().
	 */
	virtual bool Update(int32 InNodeIdToConnectTo) = 0;

protected:
	/** HAPI nodes created/managed by this modifier. */
	TArray<int32> HAPINodeIds;

	/** If true indicates that the existing, if any, HAPI nodes need to be deleted and rebuilt. */
	bool bNeedsRebuild;

private:
	/** The FUnrealObjectInputNode that owns this modifier. */
	FUnrealObjectInputNode& Owner;
};


bool
FUnrealObjectInputReferenceNode::AddReferencedNode(const FUnrealObjectInputHandle& InHandle)
{
	bool bAlreadySet = false;
	ReferencedNodes.Add(InHandle, &bAlreadySet);

	return !bAlreadySet;
}


template <class T, class... Args>
T* FUnrealObjectInputNode::CreateAndAddModifier(const FName InChainName, Args... ConstructorArguments)
{
	static_assert(std::is_base_of_v<FUnrealObjectInputModifier, T>, "T must derive from FUnrealObjectInputModifier");

	FUnrealObjectInputModifierChain* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return nullptr; 
	
	T* Modifier = new T(*this, ConstructorArguments...);
	check(Modifier);
	
	Chain->Modifiers.Emplace(Modifier);
	
	Modifier->OnAddedToOwner();

	return Modifier;
}

template <class T>
T* FUnrealObjectInputNode::FindFirstModifierByClass(const FName InChainName) const
{
	static_assert(std::is_base_of_v<FUnrealObjectInputModifier, T>, "T must derive from FUnrealObjectInputModifier");
	FUnrealObjectInputModifier* Modifier = FindFirstModifierOfType(InChainName, T::StaticGetType());
	if (!Modifier)
		return nullptr;
	return static_cast<T*>(Modifier);
}

template <class T>
bool FUnrealObjectInputNode::GetAllModifiersByClass(const FName InChainName, TArray<T*>& OutModifiers) const
{
	static_assert(std::is_base_of_v<FUnrealObjectInputModifier, T>, "T must derive from FUnrealObjectInputModifier");

	FUnrealObjectInputModifierChain const* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;
	
	const EUnrealObjectInputModifierType ModifierType = T::StaticGetType();
	TArray<T*> FoundModifiers;
	for (FUnrealObjectInputModifier const* const Modifier : Chain->Modifiers)
	{
		if (!Modifier)
			continue;
		if (Modifier->GetType() != ModifierType)
			continue;
		T* TypedModifier = Cast<T*>(Modifier);
		FoundModifiers.Emplace(TypedModifier);
	}

	if (FoundModifiers.Num() <= 0)
		return false;

	OutModifiers = MoveTemp(FoundModifiers);
	return true;
}
