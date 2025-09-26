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

class FUnrealObjectInputOptions;
class FUnrealObjectInputHandle;
class FUnrealObjectInputNode;

class FUnrealObjectInputIdentifier;

class HOUDINIENGINERUNTIME_API FUnrealObjectInputManager
{
public:
	/** Multicast delegate type to be used for notifications when a node entry in the manager is added, updated or deleted. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNodeAddUpdateDelete, const FUnrealObjectInputIdentifier&)

	virtual ~FUnrealObjectInputManager();

	static FUnrealObjectInputManager* Get();

	virtual bool FindNode(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle) const ;

	virtual bool Contains(const FUnrealObjectInputHandle& InHandle) const ;
	virtual bool Contains(const FUnrealObjectInputIdentifier& InIdentifier) const ;
	
	virtual bool GetNode(const FUnrealObjectInputHandle& InHandle, const FUnrealObjectInputNode*& OutNode) const ;
	virtual bool GetNode(const FUnrealObjectInputHandle& InHandle, FUnrealObjectInputNode*& OutNode) const ;

	virtual bool AddContainer(
		const FUnrealObjectInputIdentifier& InIdentifier, 
		const int32 InNodeId, 
		FUnrealObjectInputHandle& OutHandle) ;

	virtual bool AddReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr,
		const int32 InReferencesConnectToNodeId=INDEX_NONE) ;

	virtual bool AddLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) ;

	virtual bool UpdateContainer(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) ;

	virtual bool UpdateReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const TOptional<int32> InObjectNodeId=TOptional<int32>(),
		const TOptional<int32> InNodeId=TOptional<int32>(),
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr,
		const TOptional<int32> InReferencesConnectToNodeId=TOptional<int32>(),
		const bool bInClearDirtyFlag=true) ;

	virtual bool UpdateLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) ;

	virtual FString GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const ;

	virtual bool GetUniqueHoudiniNodeId(const int32 InHAPINodeId, int32& OutUniqueHoudiniNodeId) const ;

	virtual bool AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const ;
    virtual bool IsHAPINodeValid(const FUnrealObjectInputHAPINodeId& InNodeId) const ;
    virtual bool DeleteHAPINode(FUnrealObjectInputHAPINodeId& InNodeId) const ;
    virtual bool SetHAPINodeDisplay(const FUnrealObjectInputHAPINodeId& InNodeId, const bool bInOnOff) const ;
    virtual bool SetHAPINodeDisplay(const int32 InNodeId, const bool bInOnOff) const ;
	virtual bool GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const ;
	virtual bool GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const ;
	virtual bool GetAllHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const ;
	virtual bool GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const ;

	virtual bool EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted) ;

	virtual bool IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const ;
	virtual bool MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier, bool bInAlsoDirtyReferencedNodes) ;
	virtual bool ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier) ;

	virtual bool Clear();

	virtual FUnrealObjectInputHAPINodeId GetWorldOriginNodeId(const bool bInCreateIfMissingOrInvalid=true) ;
	virtual int32 GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid=true) ;

	virtual bool AddRef(const FUnrealObjectInputIdentifier& InIdentifier) ;
	virtual bool RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier) ;

	virtual bool AddBackLink(const FUnrealObjectInputIdentifier& InReferencedIdentifier, const FUnrealObjectInputIdentifier& InReferencedBy) ;
	virtual bool RemoveBackLink(const FUnrealObjectInputIdentifier& InReferencedIdentifier, const FUnrealObjectInputIdentifier& InReferencedBy) ;
	virtual bool GetReferencedBy(const FUnrealObjectInputIdentifier& InReferencedIdentifier, TSet<FUnrealObjectInputIdentifier>& OutReferencedBy) const ;

	virtual void Dump();

	virtual FOnNodeAddUpdateDelete& GetOnNodeAddedDelegate() ;
	virtual FOnNodeAddUpdateDelete& GetOnNodeUpdatedDelegate() ;
	virtual FOnNodeAddUpdateDelete& GetOnNodeDeletedDelegate() ;
	
protected:
	friend class FHoudiniEngine;
	
	/** Sets the singleton */
	static bool SetSingleton(FUnrealObjectInputManager* InImplementation);

	/** Destroys the Singleton and InImplementation. */
	static bool DestroySingleton();

private:

	static FUnrealObjectInputManager* Singleton;
};

