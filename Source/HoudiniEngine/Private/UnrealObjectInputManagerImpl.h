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

#include "HAPI/HAPI_Common.h"

#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputRuntimeTypes.h"


/**
 * Implementation of IUnrealObjectInputManager.
 *
 * Input node entries are stored in the InputNodes TMap by FUnrealObjectInputIdentifier.
 */
class HOUDINIENGINE_API FUnrealObjectInputManagerImpl : public IUnrealObjectInputManager 
{
public:

	FUnrealObjectInputManagerImpl();
	
	virtual ~FUnrealObjectInputManagerImpl();

	virtual bool FindNode(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle) const override;

	virtual inline bool Contains(const FUnrealObjectInputHandle& InHandle) const override { return InputNodes.Contains(InHandle.GetIdentifier()); }
	virtual inline bool Contains(const FUnrealObjectInputIdentifier& InIdentifier) const override { return InputNodes.Contains(InIdentifier); }
	
	virtual bool GetNode(const FUnrealObjectInputHandle& InHandle, const FUnrealObjectInputNode*& OutNode) const override { return GetNodeByIdentifier(InHandle.GetIdentifier(), OutNode); } 
	virtual bool GetNode(const FUnrealObjectInputHandle& InHandle, FUnrealObjectInputNode*& OutNode) const override { return GetNodeByIdentifier(InHandle.GetIdentifier(), OutNode); } 

	virtual bool AddContainer(
		const FUnrealObjectInputIdentifier& InIdentifier, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual bool AddContainer(
		UObject const* const InObject, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual bool AddContainer(
		UPackage const* const InPackage, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;
	virtual bool AddContainer(
		const FName& InPath, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle) override;

	virtual bool AddReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr) override;
	virtual bool AddReferenceNode(
		UObject const* const InObject,
		const FUnrealObjectInputOptions& InOptions,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr) override;

	virtual bool AddLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) override;
	virtual bool AddLeaf(
		UObject const* const InObject,
		const FUnrealObjectInputOptions& InOptions,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		FUnrealObjectInputHandle& OutHandle) override;

	virtual bool UpdateContainer(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) override;

	virtual bool UpdateReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		int32 const* const InObjectNodeId=nullptr,
		int32 const* const InNodeId=nullptr,
		TSet<FUnrealObjectInputHandle> const* const InReferencedNodes=nullptr,
		const bool bInClearDirtyFlag=true) override;

	virtual bool UpdateLeaf(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const bool bInClearDirtyFlag=true) override;

	virtual FString GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const override;

	virtual bool AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle) const override;
	virtual bool AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const override;
    virtual bool IsHAPINodeValid(const int32 InNodeId) const override;

    virtual bool DeleteHAPINode(const int32 InNodeId) const override;
	virtual bool GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const override;
	virtual bool GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const override;
	
	virtual bool EnsureParentsExist(
		const FUnrealObjectInputIdentifier& InIdentifier,
		FUnrealObjectInputHandle& OutParentHandle,
		const bool& bInputNodesCanBeDeleted);

	virtual bool IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const override;
	virtual bool MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier) override;
	virtual bool ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier) override;

	virtual bool Clear() override;

	virtual int32 GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid=true) override;

	virtual bool AddRef(const FUnrealObjectInputIdentifier& InIdentifier) override;
	virtual bool RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier) override;

protected:
	/** Helper function to get FUnrealObjectInputNode entries by identifier (FUnrealObjectInputIdentifier). */
	virtual bool GetNodeByIdentifier(const FUnrealObjectInputIdentifier& InputIdentifier, const FUnrealObjectInputNode*& OutNode) const;

	/** Helper function to get FUnrealObjectInputNode entries by identifier (FUnrealObjectInputIdentifier). */
	virtual bool GetNodeByIdentifier(const FUnrealObjectInputIdentifier& InputIdentifier, FUnrealObjectInputNode*& OutNode) const;

private:
	/** The input node entries by identifier. */
	TMap<FUnrealObjectInputIdentifier, FUnrealObjectInputNode*> InputNodes;

	/** The HAPI Node id of the world origin null, use for transforms when object merging. */
	HAPI_NodeId WorldOriginNodeId;
};
