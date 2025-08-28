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

#include "UnrealObjectInputManager.h"

FUnrealObjectInputManager* FUnrealObjectInputManager::Singleton = nullptr;

FUnrealObjectInputManager::~FUnrealObjectInputManager() {}

bool
FUnrealObjectInputManager::SetSingleton(FUnrealObjectInputManager* InImplementation)
{
	Singleton = InImplementation;
	return Singleton != nullptr;
}

bool
FUnrealObjectInputManager::DestroySingleton()
{
	if (!Singleton)
		return false;

	delete Singleton;
	Singleton = nullptr;

	return true;
}

FUnrealObjectInputManager* FUnrealObjectInputManager::Get()
{
	if (Singleton == nullptr)
	{
		// This should not happen!
		HOUDINI_LOG_ERROR(TEXT("FUnrealObjectInputManager not initialized correctly!!!!!!!!!!!!!!!!!!!!"));
		HOUDINI_LOG_ERROR(TEXT("Plugin will not function correctly !!!!!!!!!!!!!!!!!!!!"));
		SetSingleton(new FUnrealObjectInputManager());
	}
	return Singleton;
}


bool
FUnrealObjectInputManager::FindNode(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle) const
{
	return false;
}

bool
FUnrealObjectInputManager::Contains(const FUnrealObjectInputHandle& InHandle) const
{
	return false;
}

bool
FUnrealObjectInputManager::Contains(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	return false;
}

bool
FUnrealObjectInputManager::GetNode(const FUnrealObjectInputHandle& InHandle, const FUnrealObjectInputNode*& OutNode) const
{
	return false;
}

bool
FUnrealObjectInputManager::GetNode(const FUnrealObjectInputHandle& InHandle, FUnrealObjectInputNode*& OutNode) const
{
	return false;
}

bool
FUnrealObjectInputManager::AddContainer(const FUnrealObjectInputIdentifier& InIdentifier, const int32 InNodeId, FUnrealObjectInputHandle& OutHandle)
{
	return false;
}

bool
FUnrealObjectInputManager::AddReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const int32 InReferencesConnectToNodeId)
{
	return false;
}

bool
FUnrealObjectInputManager::AddLeaf(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle)
{
	return false;
}

bool
FUnrealObjectInputManager::UpdateContainer(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InNodeId,
	const bool bInClearDirtyFlag)
{
	return false;
}

bool
FUnrealObjectInputManager::UpdateReferenceNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const TOptional<int32> InObjectNodeId,
	const TOptional<int32> InNodeId,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const TOptional<int32> InReferencesConnectToNodeId,
	const bool bInClearDirtyFlag)
{
	return false;
}

bool
FUnrealObjectInputManager::UpdateLeaf(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InObjectNodeId,
	const int32 InNodeId,
	const bool bInClearDirtyFlag)
{
	return false;
}

FString
FUnrealObjectInputManager::GetDefaultNodeName(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	return FString();
}

bool
FUnrealObjectInputManager::GetUniqueHoudiniNodeId(const int32 InHAPINodeId, int32& OutUniqueHoudiniNodeId) const
{
	return false;
}

bool
FUnrealObjectInputManager::AreHAPINodesValid(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	return false;
}

bool
FUnrealObjectInputManager::IsHAPINodeValid(const FUnrealObjectInputHAPINodeId& InNodeId) const
{
	return false;
}

bool
FUnrealObjectInputManager::DeleteHAPINode(FUnrealObjectInputHAPINodeId& InNodeId) const
{
	return false;
}

bool
FUnrealObjectInputManager::SetHAPINodeDisplay(const FUnrealObjectInputHAPINodeId& InNodeId, const bool bInOnOff) const
{
	return false;
}

bool
FUnrealObjectInputManager::SetHAPINodeDisplay(const int32 InNodeId, const bool bInOnOff) const
{
	return false;
}

bool
FUnrealObjectInputManager::GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const
{
	return false;
}

bool
FUnrealObjectInputManager::GetHAPINodeIds(const FUnrealObjectInputIdentifier& InIdentifier, TArray<int32>& OutNodeIds) const
{
	return false;
}

bool
FUnrealObjectInputManager::GetAllHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const
{
	return false;
}

bool
FUnrealObjectInputManager::GetAllHAPINodeIds(TArray<int32>& OutNodeIds) const
{
	return false;
}

bool
FUnrealObjectInputManager::EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted)
{
	return false;
}

bool
FUnrealObjectInputManager::IsDirty(const FUnrealObjectInputIdentifier& InIdentifier) const
{
	return false;
}

bool
FUnrealObjectInputManager::MarkAsDirty(const FUnrealObjectInputIdentifier& InIdentifier, const bool bInAlsoDirtyReferencedNodes)
{
	return false;
}

bool
FUnrealObjectInputManager::ClearDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier)
{
	return false;
}

bool
FUnrealObjectInputManager::Clear()
{
	return false;
}

FUnrealObjectInputHAPINodeId
FUnrealObjectInputManager::GetWorldOriginNodeId(const bool bInCreateIfMissingOrInvalid)
{
	return FUnrealObjectInputHAPINodeId();
}

int32
FUnrealObjectInputManager::GetWorldOriginHAPINodeId(const bool bInCreateIfMissingOrInvalid)
{
	return -1;
}

bool
FUnrealObjectInputManager::AddRef(const FUnrealObjectInputIdentifier& InIdentifier)
{
	return false;
}

bool
FUnrealObjectInputManager::RemoveRef(const FUnrealObjectInputIdentifier& InIdentifier)
{
	return false;
}

bool
FUnrealObjectInputManager::AddBackLink(const FUnrealObjectInputIdentifier& InReferencedIdentifier, const FUnrealObjectInputIdentifier& InReferencedBy)
{
	return false;
}

bool
FUnrealObjectInputManager::RemoveBackLink(const FUnrealObjectInputIdentifier& InReferencedIdentifier, const FUnrealObjectInputIdentifier& InReferencedBy)
{
	return false;
}

bool
FUnrealObjectInputManager::GetReferencedBy(const FUnrealObjectInputIdentifier& InReferencedIdentifier, TSet<FUnrealObjectInputIdentifier>& OutReferencedBy) const
{
	return false;
}

FUnrealObjectInputManager::FOnNodeAddUpdateDelete&
FUnrealObjectInputManager::GetOnNodeAddedDelegate()
{
	static FOnNodeAddUpdateDelete Dummy;
	return Dummy;
}

FUnrealObjectInputManager::FOnNodeAddUpdateDelete&
FUnrealObjectInputManager::GetOnNodeUpdatedDelegate()
{
	static FOnNodeAddUpdateDelete Dummy;
	return Dummy;
}

FUnrealObjectInputManager::FOnNodeAddUpdateDelete&
FUnrealObjectInputManager::GetOnNodeDeletedDelegate()
{
	static FOnNodeAddUpdateDelete Dummy;
	return Dummy;
}

void FUnrealObjectInputManager::Dump()
{
	
}
