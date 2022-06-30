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
#include "UnrealObjectInputRuntimeTypes.h"
//#include "HoudiniOutput.h"
//#include "UnrealObjectInputTypes.h"

// #include "UnrealObjectInputManager.generated.h"

struct FUnrealObjectInputOptions;
class FUnrealObjectInputHandle;
class FUnrealObjectInputNode;

class FUnrealObjectInputIdentifier;


/**
 * Interface for an input manager.
 *
 * The input manager tracks input "nodes" (FUnrealObjectInputNode) via an identifier, FUnrealObjectInputIdentifier.
 * Entries in the manager is reference counted (via AddRef() and RemoveRef()). The FUnrealObjectInputHandle wraps
 * a FUnrealObjectInputIdentifier with automatic reference counting via its constructor and destructor. The handle
 * is the preferred way to hold references to entries in the manager.
 *
 * Identifiers essentially map an object's full path, import options (FUnrealObjectInputOptions) and a node type to
 * uniquely identify an entry in the manager. In cases where we want to identify paths (paths to a package for example,
 * but not a package or object itself) the identifier supports a string Path.
 *
 * There are different kinds of nodes: Container, Reference and Leaf. Container nodes are analogous to directories
 * on disk and are identified by Identifiers with the Path property.
 *
 * Leaf nodes map to assets / objects in Unreal and are identified via Identifiers with the Object and Options set.
 *
 * Reference nodes are nodes that reference other nodes, usually a set of Leaf nodes. They are typically also identified
 * by an Object + Options.
 */
class HOUDINIENGINERUNTIME_API IUnrealObjectInputManager
{
public:

	virtual ~IUnrealObjectInputManager();

	/**
	 * Find an entry in the manager via an identifier.
	 * @param InIdentifier The identifier of the entry.
	 * @param OutHandle A handle to the entry that is valid if the entry exists.
	 * @return true if the entry could be found, false otherwise (including if InIdentifier is invalid).
	 */
	virtual bool FindNode(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle) const = 0;

	/** Returns true if the entry identified by InHandle exists. */
	virtual bool Contains(const FUnrealObjectInputHandle& InHandle) const = 0;
	/** Returns true if the entry identified by InIdentifier exists. */
	virtual bool Contains(const FUnrealObjectInputIdentifier& InIdentifier) const = 0;
	
	/**
	 * Gets the (const) entry from a manager via its Handle.
	 * @param InHandle The handle for the entry.
	 * @param OutNode Pointer to the entry.
	 * @return true if the entry could be found, false otherwise (including for invalid InHandle).
	 */
	virtual bool GetNode(const FUnrealObjectInputHandle& InHandle, const FUnrealObjectInputNode*& OutNode) const = 0;
	/**
	 * Gets the entry from a manager via its Handle.
	 * @param InHandle The handle for the entry.
	 * @param OutNode Pointer to the entry.
	 * @return true if the entry could be found, false otherwise (including for invalid InHandle).
	 */
	virtual bool GetNode(const FUnrealObjectInputHandle& InHandle, FUnrealObjectInputNode*& OutNode) const = 0;

	/**
	 * Adds a Container entry to the manager if it does not exist in the manager yet.
	 * @param InIdentifier The identifier for the entry.
	 * @param InNodeId The main HAPI node id for the entry. For containers this is usually a Subnet Object. This can
	 *				   be -1 if the HAPI node does not exist yet.
	 * @param OutHandle The handle for the newly added entry.
	 * @returns true if the entry was added, otherwise false (including if InIdentifier is invalid, or if the entry
	 *			for that identifier already exists).
	 */
	virtual bool AddContainer(const FUnrealObjectInputIdentifier& InIdentifier, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) = 0;
	/**
	 * Convenience version of AddContainer(const FUnrealObjectInputIdentifier&, const int32, FUnrealObjectInputHandle&).
	 * Constructs the identifier from InObject.
	 */
	virtual bool AddContainer(UObject const* const InObject, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) = 0;
	/**
	 * Convenience version of AddContainer(const FUnrealObjectInputIdentifier&, const int32, FUnrealObjectInputHandle&).
	 * Constructs the identifier from InPackage.
	 */
	virtual bool AddContainer(UPackage const* const InPackage, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) = 0;
	/**
	 * Convenience version of AddContainer(const FUnrealObjectInputIdentifier&, const int32, FUnrealObjectInputHandle&).
	 * Constructs the identifier from InPath.
	 */
	virtual bool AddContainer(const FName& InPath, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) = 0;

	/**
	 * Adds a reference entry to the manager, if it does not exist for the identifier yet.
	 * @param InIdentifier The identifier for the entry.
	 * @param InObjectNodeId The object HAPI node id for the entry. This is usually a geo Object. This can be -1 if the
	 *						 HAPI node does not exist yet.
	 * @param InNodeId The main HAPI node id for the entry. This is usually a merge SOP. This can be -1 if the HAPI
	 *				   node does not exist yet.
	 * @param OutHandle The handle for the newly added entry.
	 * @param InReferencedNodes The entries, by handle, that this entry references. Optional.
	 * @returns true if the entry was added, otherwise false (including if InIdentifier is invalid, or if the entry
	 *			for that identifier already exists).
	 */
	virtual bool AddReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr) = 0;
	/**
	 * Convenience version of AddReferenceNode(const FUnrealObjectInputIdentifier&, const int32, const int32, FUnrealObjectInputHandle&, TSet<FUnrealObjectInputHandle> const* const).
	 * Constructs the identifier from InObject and InOptions.
	 */
	virtual bool AddReferenceNode(
		UObject const* const InObject,
		const FUnrealObjectInputOptions& InOptions,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr) = 0;

	/**
	 * Adds a leaf entry to the manager, if it does not exist for the identifier yet.
	 * @param InIdentifier The identifier for the entry.
	 * @param InObjectNodeId The object HAPI node id for the entry. This is usually a geo Object. This can be -1 if the
	 *						 HAPI node does not exist yet.
	 * @param InNodeId The main HAPI node id for the entry. For example, this could be a locked null SOP to where the
	 *				   plugin sent geometry for an asset from Unreal. This can be -1 if the HAPI node does not exist
	 *				   yet.
	 * @param OutHandle The handle for the newly added entry.
	 * @returns true if the entry was added, otherwise false (including if InIdentifier is invalid, or if the entry
	 *			for that identifier already exists).
	 */
	virtual bool AddLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) = 0;
	/**
	 * Convenience version of AddLeaf(const FUnrealObjectInputIdentifier&, const int32, const int32, FUnrealObjectInputHandle&).
	 * Constructs the identifier from InObject and InOptions.
	 */
	virtual bool AddLeaf(
		UObject const* const InObject,
		const FUnrealObjectInputOptions& InOptions,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) = 0;

	/**
	 * Update a container entry.
	 * @param InIdentifier The identifier of the container entry.
	 * @param InNodeId The HAPI node id to set / update for the container.
	 * @param bInClearDirtyFlag If true (the default) clear the dirty flag of the entry after updating it.
	 * @return true if the entry was found.
	 */
	virtual bool UpdateContainer(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) = 0;

	/**
	 * Update a reference entry.
	 * @param InIdentifier The identifier of the reference entry.
	 * @param InObjectNodeId The updated object node id. This is optional, a nullptr means the object node id will
	 *						 not be updated.
	 * @param InNodeId The updated node id. This is optional, a nullptr means the node id will not be updated.
	 * @param InReferencedNodes The update set of nodes that this entry references. This is optional, a nullptr means
	 *							that the node set will not be updated.
	 * @param bInClearDirtyFlag If true (the default) clear the dirty flag of the entry after updating it.
	 * @return true if the entry was found.
	 */
	virtual bool UpdateReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		int32 const* const InObjectNodeId=nullptr,
		int32 const* const InNodeId=nullptr,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr,
		const bool bInClearDirtyFlag=true) = 0;

	/**
	 * Updates a leaf entry to the manager.
	 * @param InIdentifier The identifier for the entry.
	 * @param InObjectNodeId The object HAPI node id for the entry. This is usually a geo Object. This can be -1 if the
	 *						 HAPI node does not exist yet.
	 * @param InNodeId The main HAPI node id for the entry. For example, this could be a locked null SOP to where the
	 *				   plugin sent geometry for an asset from Unreal. This can be -1 if the HAPI node does not exist
	 *				   yet.
	 * @param bInClearDirtyFlag If true (the default) clear the dirty flag of the entry after updating it.
	 * @returns true if the entry was found.
	 */
	virtual bool UpdateLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) = 0;

	/**
	 * Construct a default node name for a HAPI node based on an identifier.
	 * 
	 * For container nodes this returns the last part of the path (last directory name).
	 * 
	 * For leaf nodes this constructs a name by joining the object name and import options, for example:
	 *   Cube_lods_sockets
	 * for a Cube asset for which we are also sending sockets and LODs.
	 * 
	 * Reference nodes following the same naming as leaf nodes, except they contain "merge" in the name as well.
	 * 
	 * @param InIdentifier The identifier to construct the node name from.
	 * @return The default node name.
	 */
	virtual FString GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const = 0;

	/** Returns true if the HAPI nodes associated with the entry for InHandle exist and are valid. */
	virtual bool AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle) const = 0;
	/** Returns true if the HAPI nodes associated with the entry for InIdentifier exist and are valid. */
	virtual bool AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const = 0;
	/** Helper function that returns true if the HAPI node with id InNodeId exists and is valid. */
    virtual bool IsHAPINodeValid(const int32 InNodeId) const = 0;
	/** Helper function that deletes the given HAPI node. Returns true if the HAPI node was successfully deleted. */
    virtual bool DeleteHAPINode(const int32 InNodeId) const = 0;
	/** Helper function that gets the HAPI nodes used by a particular entry. */
	virtual bool GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const = 0;
	/** Get all HAPI node ids that are managed by the manager. */
	virtual bool GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const = 0;

	/**
	 * This function recursively checks if the parent entry for an identifier (see
	 * FUnrealObjectInputIdentifier::MakeParentIdentifier()) exists. If not it creates the entry, and its Subnet object.
	 * @param InIdentifier The identifier to check the parents of.
	 * @param OutParentHandle If successful, the valid handle to the parent, or an invalid handle if the entry by
	 *                        definition does not have a parent.
	 * @return true if the parents already existed, or did not exist but were successfully created, or if InIdentifier
	 *         by virtue of its type/settings does not have a parent.
	 */
	virtual bool EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted) = 0;

	/**
	 * Returns true if the node is dirty. That means that even though the node exists, it has been dirtied and
	 * its data should be resent to Houdini at the next opportunity.
	 * @param InIdentifier The identifier of the node to check.
	 */
	virtual bool IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const = 0;

	/**
	 * Marks the node as dirty. See IsDirty().
	 * @param InIdentifier The identifier of the node to mark as dirty.
	 * @returns true if the node was found.
	 */
	virtual bool MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier) = 0;

	/**
	 * Clears the dirty flag on the node. See IsDirty().
	 * @param InIdentifier The identifier of the node to clear the dirty flag on.
	 * @returns true if the node was found.
	 */
	virtual bool ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier) = 0;

	/** Clear the manager. This removes and destroys all input node entries. */
	virtual bool Clear() = 0;

	/**
	 * Gets, or optionally creates and returns, the WorldOrigin null. This is used for transforms when object merging
	 * in HAPI.
	 * @param bInCreateIfMissingOrInvalid If true, then if the node does not exist yet, create it. Defaults to true.
	 * @returns The id of the node. -1 if the node does not exist and bInCreateIfMissingOrInvalid == false.
	 */
	virtual int32 GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid=true) = 0;

	/**
	 * Increments the reference count of the entry identified by InIdentifier.
	 * @param InIdentifier The identifier of the entry to increase the reference count for.
	 * @return true if the entry was found and the reference count increased.
	 */
	virtual bool AddRef(const FUnrealObjectInputIdentifier& InIdentifier) = 0;
	/**
	 * Decrements the reference count of the entry identified by InIdentifier. If the reference count is 0 after
	 * decrementing it, the entry is removed and destroyed.
	 * @param InIdentifier The identifier of the entry to decrease the reference count for.
	 * @return true if the entry was found and the reference count decreased.
	 */
	virtual bool RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier) = 0;
};


/**
 * The FUnrealObjectInputManager singleton. The singleton implements the IUnrealObjectInputManager interface, but acts
 * as a proxy for the actual implementation, FUnrealObjectInputManagerImpl.
 *
 * The singleton/proxy is in the HoudiniEngineRuntime module, while the implementation is in the HoudiniEngine module.
 * The FHoudiniEngine module arranges for instantiation of the singleton and implementation.
 * 
 */
class HOUDINIENGINERUNTIME_API FUnrealObjectInputManager : public IUnrealObjectInputManager
{
public:

	// Delete the default, copy and move constructors since this is a singleton
	FUnrealObjectInputManager() = delete;
	FUnrealObjectInputManager(const FUnrealObjectInputManager& InOther) = delete;
	FUnrealObjectInputManager(FUnrealObjectInputManager&& InOther) = delete;

	virtual ~FUnrealObjectInputManager();

	static FUnrealObjectInputManager* Get() { return Singleton; }

	virtual inline bool FindNode(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle) const override;

	virtual inline bool Contains(const FUnrealObjectInputHandle& InHandle) const override;
	virtual inline bool Contains(const FUnrealObjectInputIdentifier& InIdentifier) const override;
	
	virtual inline bool GetNode(const FUnrealObjectInputHandle& InHandle, const FUnrealObjectInputNode*& OutNode) const override;
	virtual inline bool GetNode(const FUnrealObjectInputHandle& InHandle, FUnrealObjectInputNode*& OutNode) const override;

	virtual inline bool AddContainer(const FUnrealObjectInputIdentifier& InIdentifier, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual inline bool AddContainer(UObject const* const InObject, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual inline bool AddContainer(UPackage const* const InPackage, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual inline bool AddContainer(const FName& InPath, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual inline bool AddReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr) override;
	virtual inline bool AddReferenceNode(
		UObject const* const InObject,
		const FUnrealObjectInputOptions& InOptions,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr) override;
	virtual inline bool AddLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) override;
	virtual inline bool AddLeaf(
		UObject const* const InObject,
		const FUnrealObjectInputOptions& InOptions,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) override;

	virtual inline bool UpdateContainer(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) override;
	virtual inline bool UpdateReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		int32 const* const InObjectNodeId=nullptr,
		int32 const* const InNodeId=nullptr,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr,
		const bool bInClearDirtyFlag=true) override;
	virtual inline bool UpdateLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) override;

	virtual inline FString GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const override;

	virtual inline bool AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle) const override;
	virtual inline bool AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const override;
    virtual inline bool IsHAPINodeValid(const int32 InNodeId) const override;
    virtual inline bool DeleteHAPINode(const int32 InNodeId) const override;
	virtual inline bool GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const override;
	virtual inline bool GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const override;

	virtual inline bool EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted) override;

	virtual inline bool IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const override;
	virtual inline bool MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier) override;
	virtual inline bool ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier) override;

	virtual inline bool Clear() override;

	virtual inline int32 GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid=true) override;

	virtual inline bool AddRef(const FUnrealObjectInputIdentifier& InIdentifier) override;
	virtual inline bool RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier) override;

protected:
	friend class FHoudiniEngine;

	/** The implementation must be given when instantiating the singleton */
	FUnrealObjectInputManager(IUnrealObjectInputManager* InImplementation);
	
	/** Instantiates the singleton and takes ownership of InImplementation. */
	static bool CreateSingleton(IUnrealObjectInputManager* InImplementation);

	/** Destroys the Singleton and InImplementation. */
	static bool DestroySingleton();

	IUnrealObjectInputManager* GetImplementation() { return Implementation.Get(); }
	IUnrealObjectInputManager const* GetImplementation() const { return Implementation.Get(); }

private:
	TUniquePtr<IUnrealObjectInputManager> Implementation;

	static FUnrealObjectInputManager* Singleton;
};

bool 
FUnrealObjectInputManager::FindNode(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->FindNode(InIdentifier, OutHandle);
	return false;
}

bool 
FUnrealObjectInputManager::Contains(const FUnrealObjectInputHandle& InHandle) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->Contains(InHandle);
	return false;
}

bool 
FUnrealObjectInputManager::Contains(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->Contains(InIdentifier);
	return false;
}

bool 
FUnrealObjectInputManager::GetNode(const FUnrealObjectInputHandle& InHandle, const FUnrealObjectInputNode*& OutNode) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->GetNode(InHandle, OutNode);
	return false;
}

bool 
FUnrealObjectInputManager::GetNode(const FUnrealObjectInputHandle& InHandle, FUnrealObjectInputNode*& OutNode) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->GetNode(InHandle, OutNode);
	return false;
}

bool 
FUnrealObjectInputManager::AddContainer(const FUnrealObjectInputIdentifier& InIdentifier, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddContainer(InIdentifier, InNodeId, OutHandle);
	return false;
}

bool 
FUnrealObjectInputManager::AddContainer(UObject const* const InObject, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddContainer(InObject, InNodeId, OutHandle);
	return false;
}

bool 
FUnrealObjectInputManager::AddContainer(UPackage const* const InPackage, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddContainer(InPackage, InNodeId, OutHandle);
	return false;
}

bool 
FUnrealObjectInputManager::AddContainer(const FName& InPath, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddContainer(InPath, InNodeId, OutHandle);
	return false;
}

bool
FUnrealObjectInputManager::AddReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddReferenceNode(InIdentifier, InObjectNodeId, InNodeId, OutHandle, InReferencedNodes);
	return false;
}

bool
FUnrealObjectInputManager::AddReferenceNode(
	UObject const* const InObject,
	const FUnrealObjectInputOptions& InOptions,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddReferenceNode(InObject, InOptions, InObjectNodeId, InNodeId, OutHandle, InReferencedNodes);
	return false;
}

bool 
FUnrealObjectInputManager::AddLeaf(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddLeaf(InIdentifier, InObjectNodeId, InNodeId, OutHandle);
	return false;
}

bool 
FUnrealObjectInputManager::AddLeaf(
	UObject const* const InObject,
	const FUnrealObjectInputOptions& InOptions,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddLeaf(InObject, InOptions, InObjectNodeId, InNodeId, OutHandle);
	return false;
}

bool
FUnrealObjectInputManager::UpdateContainer(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InNodeId,
	const bool bInClearDirtyFlag)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->UpdateContainer(InIdentifier, InNodeId, bInClearDirtyFlag);
	return false;
}

bool
FUnrealObjectInputManager::UpdateReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	int32 const* const InObjectNodeId,
	int32 const* const InNodeId,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const bool bInClearDirtyFlag)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->UpdateReferenceNode(InIdentifier, InObjectNodeId, InNodeId, InReferencedNodes, bInClearDirtyFlag);
	return false;
}

bool
FUnrealObjectInputManager::UpdateLeaf(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	const bool bInClearDirtyFlag)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->UpdateLeaf(InIdentifier, InObjectNodeId, InNodeId, bInClearDirtyFlag);
	return false;
}

FString 
FUnrealObjectInputManager::GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->GetDefaultNodeName(InIdentifier);
	return FString();
}

bool
FUnrealObjectInputManager::AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->AreHAPINodesValid(InHandle);
	return false;
}

bool
FUnrealObjectInputManager::AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->AreHAPINodesValid(InIdentifier);
	return false;
}

bool
FUnrealObjectInputManager::IsHAPINodeValid(const int32 InNodeId) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->IsHAPINodeValid(InNodeId);
	return false;
}

bool
FUnrealObjectInputManager::DeleteHAPINode(const int32 InNodeId) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->DeleteHAPINode(InNodeId);
	return false;
}

bool
FUnrealObjectInputManager::GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->GetHAPINodeIds(InIdentifier, OutNodeIds);
	return false;
}

bool
FUnrealObjectInputManager::GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->GetAllHAPINodeIds(OutNodeIds);
	return false;
}

bool
FUnrealObjectInputManager::EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->EnsureParentsExist(InIdentifier, OutParentHandle, bInputNodesCanBeDeleted);
	return false;
}

bool
FUnrealObjectInputManager::IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	if (IUnrealObjectInputManager const* const Impl = GetImplementation())
		return Impl->IsDirty(InIdentifier);
	return false;
}

bool
FUnrealObjectInputManager::MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->MarkAsDirty(InIdentifier);
	return false;
}

bool
FUnrealObjectInputManager::ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->ClearDirtyFlag(InIdentifier);
	return false;
}

bool 
FUnrealObjectInputManager::Clear()
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->Clear();
	return false;
}

int32
FUnrealObjectInputManager::GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->GetWorldOriginHAPINodeId(bInCreateIfMissingOrInvalid);
	return false;
}

bool 
FUnrealObjectInputManager::AddRef(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->AddRef(InIdentifier);
	return false;
}

bool 
FUnrealObjectInputManager::RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (IUnrealObjectInputManager* const Impl = GetImplementation())
		return Impl->RemoveRef(InIdentifier);
	return false;
}
