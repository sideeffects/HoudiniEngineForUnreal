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

#include "UnrealObjectInputRuntimeTypes.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#include "CoreMinimal.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "Misc/Paths.h"
#include "Runtime/Launch/Resources/Version.h"

#include "UnrealObjectInputManager.h"

FUnrealObjectInputOptions::FUnrealObjectInputOptions(
	const bool bInImportAsReference,
	const bool bInImportAsReferenceRotScaleEnabled,
	const bool bInExportLODs,
	const bool bInExportSockets,
	const bool bInExportColliders)
		: bImportAsReference(bInImportAsReference)
		, bImportAsReferenceRotScaleEnabled(bInImportAsReferenceRotScaleEnabled)
		, bExportLODs(bInExportLODs)
		, bExportSockets(bInExportSockets)
		, bExportColliders(bInExportColliders)
{
}

uint32
FUnrealObjectInputOptions::GetTypeHash() const
{
	return FCrc::MemCrc32(this, sizeof(*this));
}

bool
FUnrealObjectInputOptions::operator==(const FUnrealObjectInputOptions& InOther) const
{
	return (bImportAsReference == InOther.bImportAsReference
		&& bImportAsReferenceRotScaleEnabled == InOther.bImportAsReferenceRotScaleEnabled
		&& bExportLODs == InOther.bExportLODs
		&& bExportSockets == InOther.bExportSockets
		&& bExportColliders == InOther.bExportColliders);
}


FUnrealObjectInputIdentifier::FUnrealObjectInputIdentifier()
	: Object(nullptr)
	, Path(NAME_None)
	, Options()
	, NodeType(EUnrealObjectInputNodeType::Invalid)
{
}

FUnrealObjectInputIdentifier::FUnrealObjectInputIdentifier(UObject const* const InObject, const FUnrealObjectInputOptions& InOptions, const bool bIsLeaf)
	: Object(InObject)
	, Path(NAME_None)
	, Options(InOptions)
	, NodeType(bIsLeaf ? EUnrealObjectInputNodeType::Leaf : EUnrealObjectInputNodeType::Reference)
{
}

FUnrealObjectInputIdentifier::FUnrealObjectInputIdentifier(UObject const* const InObject)
	: Object(InObject)
	, Path(NAME_None)
	, Options()
	, NodeType(EUnrealObjectInputNodeType::Container)
{
}

FUnrealObjectInputIdentifier::FUnrealObjectInputIdentifier(UPackage const* const InPackage)
	: Object(nullptr)
	, Path(::IsValid(InPackage) ? FName(InPackage->GetPathName()) : NAME_None)
	, Options()
	, NodeType(EUnrealObjectInputNodeType::Container)
{
}

FUnrealObjectInputIdentifier::FUnrealObjectInputIdentifier(const FName& InPath)
	: Object(nullptr)
	, Path(InPath)
	, Options()
	, NodeType(EUnrealObjectInputNodeType::Container)
{
}

FUnrealObjectInputIdentifier::FUnrealObjectInputIdentifier(const FUnrealObjectInputHandle& InHandle)
	: FUnrealObjectInputIdentifier(InHandle.GetIdentifier())
{
}

bool FUnrealObjectInputIdentifier::IsValid() const
{
	switch(NodeType)
	{
		case EUnrealObjectInputNodeType::Invalid:
			return false;
		case EUnrealObjectInputNodeType::Container:
			return Object.IsValid() || Path != NAME_None;
		case EUnrealObjectInputNodeType::Reference:
		case EUnrealObjectInputNodeType::Leaf:
			return Object.IsValid();
	}

	return false;
}

void
FUnrealObjectInputIdentifier::Reset()
{
	Object.Reset();
	Path = NAME_None;
	Options = FUnrealObjectInputOptions();
	NodeType = EUnrealObjectInputNodeType::Invalid;
}

uint32
FUnrealObjectInputIdentifier::GetTypeHash() const
{
	const FName ObjectPath = Object.IsValid() ? FName(Object->GetPathName()) : Path;

	switch(NodeType)
	{
		case EUnrealObjectInputNodeType::Invalid:
#if ENGINE_MINOR_VERSION < 2
			return ::GetTypeHash(FString());
#else
			return 0;
#endif
		case EUnrealObjectInputNodeType::Container:
			return ::GetTypeHash(ObjectPath);
		case EUnrealObjectInputNodeType::Reference:
		case EUnrealObjectInputNodeType::Leaf:
			const TPair<FName, FUnrealObjectInputOptions> Pair(ObjectPath, Options);
			return ::GetTypeHash(Pair);
	}

#if ENGINE_MINOR_VERSION < 2
	return ::GetTypeHash(FString());
#else
	return 0;
#endif
}

bool
FUnrealObjectInputIdentifier::operator==(const FUnrealObjectInputIdentifier& InOther) const
{
	// NodeTypes must be the same
	if (NodeType != InOther.NodeType)
		return false;

	// NodeTypes are equal and invalid
	if (NodeType == EUnrealObjectInputNodeType::Invalid)
		return true;

	if (NodeType == EUnrealObjectInputNodeType::Leaf || NodeType == EUnrealObjectInputNodeType::Reference)
		return Object == InOther.Object && Options == InOther.Options;

	if (Object.IsValid() && InOther.Object.IsValid())
		return Object == InOther.Object;

	if (Object.Get() == nullptr && InOther.Object.Get() == nullptr)
		return Path == InOther.Path;

	return false;
}

bool
FUnrealObjectInputIdentifier::MakeParentIdentifier(FUnrealObjectInputIdentifier& OutParentIdentifier) const
{
	if (!IsValid())
		return false;

	// if (NodeType == EUnrealObjectInputNodeType::Leaf || NodeType == EUnrealObjectInputNodeType::Reference)
	if (Object.IsValid())
	{
		FUnrealObjectInputIdentifier ParentIdentifier;
		UObject const* const Outer = Object->GetOuter();
		if (::IsValid(Outer))
		{
			UPackage const* const Package = Cast<UPackage>(Outer);
			if (::IsValid(Package))
			{
				ParentIdentifier = FUnrealObjectInputIdentifier(Package);				
			}
			else
			{
				ParentIdentifier = FUnrealObjectInputIdentifier(Outer);
			}
		}
		else
		{
			const FName ParentPath(FPaths::GetPath(Object->GetPathName()));
			ParentIdentifier = FUnrealObjectInputIdentifier(ParentPath);
		}

		if (ParentIdentifier.IsValid())
		{
			OutParentIdentifier = ParentIdentifier;
			return true;
		}
		
		return false;
	}

	const FUnrealObjectInputIdentifier ParentIdentifier = FUnrealObjectInputIdentifier(
		FName(FPaths::GetPath(Path.ToString())));
	if (ParentIdentifier.IsValid())
	{
		OutParentIdentifier = ParentIdentifier;
		return true;
	}
	
	return false;
}


FUnrealObjectInputHandle::FUnrealObjectInputHandle()
	: Identifier()
	, bIsInitialized(false)
{
}

FUnrealObjectInputHandle::FUnrealObjectInputHandle(const FUnrealObjectInputIdentifier& InIdentifier)
	: Identifier()
	, bIsInitialized(false)
{
	Initialize(InIdentifier);
}

FUnrealObjectInputHandle::FUnrealObjectInputHandle(const FUnrealObjectInputHandle& InHandle)
{
	bIsInitialized = false;
	if (InHandle.bIsInitialized)
		Initialize(InHandle.Identifier);
	else
		Identifier = InHandle.Identifier;
}

FUnrealObjectInputHandle::~FUnrealObjectInputHandle()
{
	DeInitialize();
}

bool
FUnrealObjectInputHandle::Initialize(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (bIsInitialized)
		return false;

	Identifier = InIdentifier;
	if (!Identifier.IsValid())
		return false;
	
	if (FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get())
	{
		bIsInitialized = Manager->AddRef(Identifier);
	}

	return bIsInitialized;
}

void
FUnrealObjectInputHandle::DeInitialize()
{
	if (!bIsInitialized)
		return;

	if (!Identifier.IsValid())
	{
		// Should not happen...
		HOUDINI_LOG_WARNING(TEXT("Found an initialized FUnrealObjectInputHandle with invalid identifier..."));
		bIsInitialized = false;
		return;
	}

	if (FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get())
	{
		Manager->RemoveRef(Identifier);
	}

	Identifier = FUnrealObjectInputIdentifier();
	bIsInitialized = false;
}

uint32
FUnrealObjectInputHandle::GetTypeHash() const
{
	const uint32 Values[2] = {bIsInitialized, Identifier.GetTypeHash()};
	return FCrc::MemCrc32(Values, sizeof(Values));	
}

bool
FUnrealObjectInputHandle::operator==(const FUnrealObjectInputHandle& InOther) const
{
	return bIsInitialized == InOther.bIsInitialized && Identifier == InOther.Identifier;
}

bool
FUnrealObjectInputHandle::IsValid() const
{
	if (!Identifier.IsValid() || !bIsInitialized)
		return false;
	
	FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->Contains(*this);
}

void FUnrealObjectInputHandle::Reset()
{
	if (bIsInitialized)
		DeInitialize();
	else
		Identifier = FUnrealObjectInputIdentifier();
}

FUnrealObjectInputHandle&
FUnrealObjectInputHandle::operator=(const FUnrealObjectInputHandle& InOther)
{
	if (Identifier == InOther.Identifier && bIsInitialized == InOther.bIsInitialized)
		return *this;

	if (bIsInitialized)
		DeInitialize();

	if (InOther.bIsInitialized)
		Initialize(InOther.Identifier);

	return *this;
}


FUnrealObjectInputNode::FUnrealObjectInputNode(const FUnrealObjectInputIdentifier& InIdentifier)
	: Identifier(InIdentifier)
	, Parent()
	, NodeId(INDEX_NONE)
	, bIsDirty(false)
	, ReferenceCount(0)
	, bCanBeDeleted(true)
{
	
}

FUnrealObjectInputNode::FUnrealObjectInputNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InNodeId)
	: Identifier(InIdentifier)
	, Parent(InParent)
	, NodeId(InNodeId)
	, bIsDirty(false)
	, ReferenceCount(0)
	, bCanBeDeleted(true)
{
	
}

FUnrealObjectInputNode::~FUnrealObjectInputNode()
{
	if (AreHAPINodesValid())
		DeleteHAPINodes();
}

bool FUnrealObjectInputNode::AreHAPINodesValid() const
{
	if (NodeId < 0)
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->IsHAPINodeValid(NodeId);
}

bool FUnrealObjectInputNode::DeleteHAPINodes()
{
	if (!CanBeDeleted())
		return true;

	if (NodeId < 0)
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->DeleteHAPINode(NodeId);
}

void
FUnrealObjectInputNode::GetHAPINodeIds(TArray<int32>& OutNodeIds) const
{
	if (NodeId < 0)
		return;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return;

	if (!Manager->IsHAPINodeValid(NodeId))
		return;

	OutNodeIds.Add(NodeId);
}

bool
FUnrealObjectInputNode::AddRef() const
{
	if (!IsRefCounted())
		return false;
	
	FPlatformAtomics::InterlockedIncrement(&ReferenceCount);
	return true;
}

bool
FUnrealObjectInputNode::RemoveRef() const
{
	if (!IsRefCounted())
		return false;

#if DO_GUARD_SLOW
	if (ReferenceCount == 0)
	{
		check(ReferenceCount != 0 && TEXT("Trying to RemoveRef when ReferenceCount is 0."));
	}
#endif

	FPlatformAtomics::InterlockedDecrement(&ReferenceCount);
	return true;
}

FUnrealObjectInputContainerNode::FUnrealObjectInputContainerNode(const FUnrealObjectInputIdentifier& InIdentifier)
	: FUnrealObjectInputNode(InIdentifier)
{
	
}

FUnrealObjectInputContainerNode::FUnrealObjectInputContainerNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InNodeId)
	: FUnrealObjectInputNode(InIdentifier, InParent, InNodeId)
{
	
}


FUnrealObjectInputLeafNode::FUnrealObjectInputLeafNode(const FUnrealObjectInputIdentifier& InIdentifier)
	: FUnrealObjectInputNode(InIdentifier)
	, ObjectNodeId(INDEX_NONE)
{
	
}

FUnrealObjectInputLeafNode::FUnrealObjectInputLeafNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId)
	: FUnrealObjectInputNode(InIdentifier, InParent, InNodeId)
	, ObjectNodeId(InObjectNodeId)
{
	
}

FUnrealObjectInputLeafNode::~FUnrealObjectInputLeafNode()
{
	if (AreHAPINodesValid())
		DeleteHAPINodes();
}

bool
FUnrealObjectInputLeafNode::AreHAPINodesValid() const
{
	if (ObjectNodeId < 0)
		return false;

	if (!FUnrealObjectInputNode::AreHAPINodesValid())
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->IsHAPINodeValid(ObjectNodeId);
}

bool FUnrealObjectInputLeafNode::DeleteHAPINodes()
{
	// If the node cant be deleted, return true
	if (!CanBeDeleted())
		return true;

	if (!FUnrealObjectInputNode::DeleteHAPINodes())
		return false;
	
	if (ObjectNodeId < 0)
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->DeleteHAPINode(ObjectNodeId);
}

void
FUnrealObjectInputLeafNode::GetHAPINodeIds(TArray<int32>& OutNodeIds) const
{
	FUnrealObjectInputNode::GetHAPINodeIds(OutNodeIds);

	if (ObjectNodeId < 0)
		return;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return;

	if (!Manager->IsHAPINodeValid(ObjectNodeId))
		return;

	OutNodeIds.Add(ObjectNodeId);
}

FUnrealObjectInputReferenceNode::FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier)
	: FUnrealObjectInputLeafNode(InIdentifier)
{
	
}

FUnrealObjectInputReferenceNode::FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId)
	: FUnrealObjectInputLeafNode(InIdentifier, InParent, InObjectNodeId, InNodeId)
{
	
}

FUnrealObjectInputReferenceNode::FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId, const TSet<FUnrealObjectInputHandle>& InReferencedNodes)
	: FUnrealObjectInputLeafNode(InIdentifier, InParent, InObjectNodeId, InNodeId)
	, ReferencedNodes(InReferencedNodes)
{
	
}

bool FUnrealObjectInputReferenceNode::AreReferencedHAPINodesValid() const
{
	if (ReferencedNodes.IsEmpty())
		return true;

	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	for (const FUnrealObjectInputHandle& Handle : ReferencedNodes)
	{
		if (!Manager->AreHAPINodesValid(Handle))
			return false;
	}

	return true;
}
