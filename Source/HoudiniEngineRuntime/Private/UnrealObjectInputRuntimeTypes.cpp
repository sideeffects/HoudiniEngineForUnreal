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
#include "Components/StaticMeshComponent.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "Misc/Paths.h"
#include "Runtime/Launch/Resources/Version.h"

#include "UnrealObjectInputManager.h"

bool
FUnrealObjectInputHAPINodeId::Set(const int32 InHAPINodeId, const int32 InUniqueHoudiniNodeId)
{
	HAPINodeId = InHAPINodeId;
	UniqueHoudiniNodeId = InUniqueHoudiniNodeId;
	if (!IsValid())
	{
		Reset();
		return false;
	}
	return true;
}
bool
FUnrealObjectInputHAPINodeId::Set(const int32 InHAPINodeId)
{
	Reset();

	if (InHAPINodeId < 0)
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	int32 TheUniqueHoudiniNodeId = -1;
	if (!Manager->GetUniqueHoudiniNodeId(InHAPINodeId, TheUniqueHoudiniNodeId))
		return false;

	HAPINodeId = InHAPINodeId;
	UniqueHoudiniNodeId = TheUniqueHoudiniNodeId;

	return true;
}

bool
FUnrealObjectInputHAPINodeId::IsSet() const
{
	return HAPINodeId >= 0 && UniqueHoudiniNodeId >= 0;
}

bool
FUnrealObjectInputHAPINodeId::IsValid() const
{
	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->IsHAPINodeValid(*this);
}

FUnrealObjectInputOptions::FUnrealObjectInputOptions(
	const bool bInImportAsReference,
	const bool bInImportAsReferenceRotScaleEnabled,
	const bool bInExportLODs,
	const bool bInExportSockets,
	const bool bInExportColliders,
	const EHoudiniLandscapeExportType LandscapeExportType)
		: bImportAsReference(bInImportAsReference)
		, bImportAsReferenceRotScaleEnabled(bInImportAsReferenceRotScaleEnabled)
		, bExportLODs(bInExportLODs)
		, bExportSockets(bInExportSockets)
		, bExportColliders(bInExportColliders)
		, LandscapeExportType(LandscapeExportType)
		, bExportLandscapeSplineControlPoints(false)
		, bExportLandscapeSplineLeftRightCurves(false)
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
		&& bExportColliders == InOther.bExportColliders
		&& LandscapeExportType == InOther.LandscapeExportType
		&& bExportLandscapeSplineControlPoints == InOther.bExportLandscapeSplineControlPoints
		&& bExportLandscapeSplineLeftRightCurves == InOther.bExportLandscapeSplineLeftRightCurves);
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
	FName ObjectPath = Object.IsValid() ? FName(Object->GetPathName()) : Path;

	switch(NodeType)
	{
		case EUnrealObjectInputNodeType::Invalid:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
			return 0;
#else
			return ::GetTypeHash(FString());
#endif
		case EUnrealObjectInputNodeType::Container:
		{
			//return FName::GetTypeHash(ObjectPath);
			//return ::GetTypeHash(ObjectPath.GetComparisonIndex()) + ObjectPath.GetNumber();
			const TPair<FName, FUnrealObjectInputOptions> Pair(ObjectPath, Options);
			return ::GetTypeHash(Pair);
		}

		case EUnrealObjectInputNodeType::Reference:
		case EUnrealObjectInputNodeType::Leaf:
		{
			const TPair<FName, FUnrealObjectInputOptions> Pair(ObjectPath, Options);
			return ::GetTypeHash(Pair);
		}
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	return 0;
#else
	return ::GetTypeHash(FString());
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
	: bIsInitialized(false)
	, Identifier()
{
}

FUnrealObjectInputHandle::FUnrealObjectInputHandle(const FUnrealObjectInputIdentifier& InIdentifier)
	: bIsInitialized(false)
	, Identifier()
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


FUnrealObjectInputBackLinkHandle::FUnrealObjectInputBackLinkHandle()
	: FUnrealObjectInputHandle()
	, SourceIdentifier()
{
}

FUnrealObjectInputBackLinkHandle::FUnrealObjectInputBackLinkHandle(const FUnrealObjectInputIdentifier& InSourceIdentifier, const FUnrealObjectInputIdentifier& InTargetIdentifier)
	: FUnrealObjectInputHandle()
	, SourceIdentifier()
{
	Initialize(InSourceIdentifier, InTargetIdentifier);
}

FUnrealObjectInputBackLinkHandle::FUnrealObjectInputBackLinkHandle(const FUnrealObjectInputBackLinkHandle& InHandle)
{
	bIsInitialized = false;
	if (InHandle.bIsInitialized)
	{
		Initialize(InHandle.GetSourceIdentifier(), InHandle.GetTargetIdentifier());
	}
	else
	{
		SourceIdentifier = InHandle.GetSourceIdentifier();
		Identifier = InHandle.GetTargetIdentifier();
	}
}

FUnrealObjectInputBackLinkHandle::~FUnrealObjectInputBackLinkHandle()
{
	DeInitialize();
}

bool
FUnrealObjectInputBackLinkHandle::Initialize(const FUnrealObjectInputIdentifier& InSourceIdentifier, const FUnrealObjectInputIdentifier& InTargetIdentifier)
{
	if (bIsInitialized)
		return false;

	Identifier = InTargetIdentifier;
	SourceIdentifier = InSourceIdentifier;
	if (!Identifier.IsValid() || !SourceIdentifier.IsValid())
		return false;
	
	if (FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get())
	{
		if (Manager->AddRef(Identifier))
		{
			bIsInitialized = true;
			Manager->AddBackLink(Identifier, SourceIdentifier);
		}
	}

	return bIsInitialized;
}

void
FUnrealObjectInputBackLinkHandle::DeInitialize()
{
	if (!bIsInitialized)
		return;

	if (!Identifier.IsValid())
	{
		// Should not happen...
		HOUDINI_LOG_WARNING(TEXT("Found an initialized FUnrealObjectInputBackLinkHandle with invalid identifier..."));
		bIsInitialized = false;
		return;
	}

	if (FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get())
	{
		Manager->RemoveBackLink(Identifier, SourceIdentifier);
		Manager->RemoveRef(Identifier);
	}

	SourceIdentifier = FUnrealObjectInputIdentifier();
	Identifier = FUnrealObjectInputIdentifier();
	bIsInitialized = false;
}

uint32
FUnrealObjectInputBackLinkHandle::GetTypeHash() const
{
	const uint32 Values[3] = {bIsInitialized, Identifier.GetTypeHash(), SourceIdentifier.GetTypeHash()};
	return FCrc::MemCrc32(Values, sizeof(Values));
}

bool
FUnrealObjectInputBackLinkHandle::operator==(const FUnrealObjectInputBackLinkHandle& InOther) const
{
	return bIsInitialized == InOther.bIsInitialized && Identifier == InOther.GetTargetIdentifier()
		&& SourceIdentifier == InOther.GetSourceIdentifier();
}

void FUnrealObjectInputBackLinkHandle::Reset()
{
	if (bIsInitialized)
	{
		DeInitialize();
	}
	else
	{
		Identifier = FUnrealObjectInputIdentifier();
		SourceIdentifier = FUnrealObjectInputIdentifier();
	}
}

FUnrealObjectInputBackLinkHandle&
FUnrealObjectInputBackLinkHandle::operator=(const FUnrealObjectInputBackLinkHandle& InOther)
{
	if (InOther == *this)
		return *this;

	if (bIsInitialized)
		DeInitialize();

	if (InOther.bIsInitialized)
		Initialize(InOther.GetSourceIdentifier(), InOther.GetTargetIdentifier());

	return *this;
}


const FName FUnrealObjectInputNode::OutputChainName(TEXT("output"));


FUnrealObjectInputNode::FUnrealObjectInputNode(const FUnrealObjectInputIdentifier& InIdentifier)
	: Identifier(InIdentifier)
	, Parent()
	, bIsDirty(false)
	, ReferenceCount(0)
	, bCanBeDeleted(true)
{
	
}

FUnrealObjectInputNode::FUnrealObjectInputNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InNodeId)
	: Identifier(InIdentifier)
	, Parent(InParent)
	, bIsDirty(false)
	, ReferenceCount(0)
	, bCanBeDeleted(true)
{
	NodeId.Set(InNodeId);
}

FUnrealObjectInputNode::~FUnrealObjectInputNode()
{
	DestroyAllModifierChains();
	if (AreHAPINodesValid())
		DeleteHAPINodes();
}

bool FUnrealObjectInputNode::AreHAPINodesValid() const
{
	return NodeId.IsValid();
}

bool FUnrealObjectInputNode::DeleteHAPINodes()
{
	if (!CanBeDeleted())
		return true;

	if (!NodeId.IsValid())
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->DeleteHAPINode(NodeId);
}

void
FUnrealObjectInputNode::GetHAPINodeIds(TArray<int32>& OutHAPINodeIds) const
{
	TArray<FUnrealObjectInputHAPINodeId> NodeIds;
	GetHAPINodeIds(NodeIds);
	OutHAPINodeIds.Empty(NodeIds.Num());
	for (const FUnrealObjectInputHAPINodeId& TheNodeId : NodeIds)
		OutHAPINodeIds.Add(TheNodeId.GetHAPINodeId());
}

void
FUnrealObjectInputNode::GetHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const
{
	if (!NodeId.IsValid())
		return;

	OutNodeIds.Add(NodeId);
}

bool
FUnrealObjectInputNode::AddModifierChain(const FName InChainName, const int32 InNodeIdToConnectTo)
{
	FUnrealObjectInputHAPINodeId NodeToConnectTo;
	if (!NodeToConnectTo.Set(InNodeIdToConnectTo))
		return false;
	return AddModifierChain(InChainName, NodeToConnectTo);
}

bool
FUnrealObjectInputNode::AddModifierChain(const FName InChainName, const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	if (ModifierChains.Contains(InChainName))
		return false;
	FUnrealObjectInputModifierChain& Chain = ModifierChains.Add(InChainName);
	Chain.ConnectToNodeId = InNodeIdToConnectTo;
	return true;
}

bool
FUnrealObjectInputNode::SetModifierChainNodeToConnectTo(const FName InChainName, const int32 InNodeToConnectTo)
{
	FUnrealObjectInputHAPINodeId NodeToConnectTo;
	if (!NodeToConnectTo.Set(InNodeToConnectTo))
		return false;
	return SetModifierChainNodeToConnectTo(InChainName, NodeToConnectTo);
}

bool
FUnrealObjectInputNode::SetModifierChainNodeToConnectTo(const FName InChainName, const FUnrealObjectInputHAPINodeId& InNodeToConnectTo)
{
	FUnrealObjectInputModifierChain* Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;
	Chain->ConnectToNodeId = InNodeToConnectTo;
	return true;
}

int32
FUnrealObjectInputNode::GetInputHAPINodeIdOfModifierChain(const FName InChainName) const
{
	const FUnrealObjectInputHAPINodeId InputNodeId = GetInputNodeIdOfModifierChain(InChainName);
	if (!InputNodeId.IsValid())
		return -1;
	return InputNodeId.GetHAPINodeId();
}

FUnrealObjectInputHAPINodeId
FUnrealObjectInputNode::GetInputNodeIdOfModifierChain(const FName InChainName) const
{
	static const FUnrealObjectInputHAPINodeId InvalidNodeId;
	FUnrealObjectInputModifierChain const* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return InvalidNodeId;
	if (Chain->Modifiers.Num() <= 0)
		return InvalidNodeId;
	FUnrealObjectInputModifier const* const Modifier = Chain->Modifiers[0];
	if (!Modifier)
		return InvalidNodeId;
	return Modifier->GetInputNodeId();
}

int32
FUnrealObjectInputNode::GetOutputHAPINodeIdOfModifierChain(const FName InChainName) const
{
	const FUnrealObjectInputHAPINodeId OutputNodeId = GetOutputNodeIdOfModifierChain(InChainName);
	if (!OutputNodeId.IsValid())
		return -1;
	return OutputNodeId.GetHAPINodeId();
}

FUnrealObjectInputHAPINodeId
FUnrealObjectInputNode::GetOutputNodeIdOfModifierChain(const FName InChainName) const
{
	static const FUnrealObjectInputHAPINodeId InvalidNodeId;
	FUnrealObjectInputModifierChain const* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return InvalidNodeId;
	FUnrealObjectInputModifier const* const Modifier = Chain->Modifiers.Last();
	if (!Modifier)
		return InvalidNodeId;
	return Modifier->GetOutputNodeId();
}

bool
FUnrealObjectInputNode::RemoveModifierChain(const FName InChainName)
{
	FUnrealObjectInputModifierChain* Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;
	DestroyModifiers(InChainName);
	ModifierChains.Remove(InChainName);
	return true;
}

bool
FUnrealObjectInputNode::AddModifier(const FName InChainName, FUnrealObjectInputModifier* const InModifierToAdd)
{
	FUnrealObjectInputModifierChain* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;
	for (FUnrealObjectInputModifier const* const Modifier : Chain->Modifiers)
	{
		if (Modifier == InModifierToAdd)
			return false;
	}
	Chain->Modifiers.Emplace(InModifierToAdd);
	InModifierToAdd->OnAddedToOwner();
	return true;
}

FUnrealObjectInputModifier*
FUnrealObjectInputNode::FindFirstModifierOfType(const FName InChainName, const EUnrealObjectInputModifierType InModifierType) const
{
	FUnrealObjectInputModifierChain const* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return nullptr;
	FUnrealObjectInputModifier* const* const Result = Chain->Modifiers.FindByPredicate([InModifierType](FUnrealObjectInputModifier const* const InModifier)
	{
		if (!InModifier)
			return false;
		return InModifier->GetType() == InModifierType;
	});

	if (!Result)
		return nullptr;

	return *Result;
}

bool
FUnrealObjectInputNode::GetAllModifiersOfType(const FName InChainName, const EUnrealObjectInputModifierType InModifierType, TArray<FUnrealObjectInputModifier*>& OutModifiers) const
{
	FUnrealObjectInputModifierChain const* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;
	TArray<FUnrealObjectInputModifier*> FoundModifiers;
	for (FUnrealObjectInputModifier* const Modifier : Chain->Modifiers)
	{
		if (!Modifier)
			continue;
		if (Modifier->GetType() != InModifierType)
			continue;
		FoundModifiers.Emplace(Modifier);
	}

	if (FoundModifiers.Num() <= 0)
		return false;

	OutModifiers = MoveTemp(FoundModifiers);
	return true;
}

bool
FUnrealObjectInputNode::DestroyModifier(const FName InChainName, FUnrealObjectInputModifier* InModifier)
{
	if (!InModifier)
		return false;

	FUnrealObjectInputModifierChain* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;

	int32 ModifierIndex = INDEX_NONE;
	const int32 NumModifiers = Chain->Modifiers.Num();
	for (int32 Index = 0; Index < NumModifiers; ++Index)
	{
		FUnrealObjectInputModifier const* const Modifier = Chain->Modifiers[Index];
		if (!Modifier)
			continue;
		if (Modifier != InModifier)
			continue;

		ModifierIndex = Index;
		break;
	}

	if (ModifierIndex == INDEX_NONE)
		return false;

	Chain->Modifiers.RemoveAt(ModifierIndex);
	InModifier->OnRemovedFromOwner();
	delete InModifier;

	return true;
}

bool
FUnrealObjectInputNode::DestroyModifiers(const FName InChainName)
{
	FUnrealObjectInputModifierChain* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;

	while (Chain->Modifiers.Num() > 0)
	{
		static constexpr bool bAllowShrinking = false;
		FUnrealObjectInputModifier* const Modifier = Chain->Modifiers.Pop(bAllowShrinking);
		if (!Modifier)
			continue;
		Modifier->OnRemovedFromOwner();
		delete Modifier;
	}

	Chain->Modifiers.Empty();

	return true;
}

bool
FUnrealObjectInputNode::DestroyAllModifierChains()
{
	bool bSuccess = true;
	TArray<FName> ChainNames;
	ModifierChains.GetKeys(ChainNames);
	for (const FName& ChainName : ChainNames)
	{
		if (!DestroyModifiers(ChainName))
			bSuccess = false;
	}

	return bSuccess;
}

bool
FUnrealObjectInputNode::UpdateModifiers(const FName InChainName)
{
	FUnrealObjectInputModifierChain* const Chain = ModifierChains.Find(InChainName);
	if (!Chain)
		return false;
	
	bool bSuccess = true;
	FUnrealObjectInputHAPINodeId NodeToConnectTo = Chain->ConnectToNodeId;
	if (!NodeToConnectTo.IsValid())
		NodeToConnectTo = NodeId;
	for (FUnrealObjectInputModifier* const Modifier : Chain->Modifiers)
	{
		if (!Modifier)
			continue;
		if (!Modifier->Update(NodeToConnectTo))
		{
			bSuccess = false;
		}
		else
		{
			const FUnrealObjectInputHAPINodeId OutputNodeId = Modifier->GetOutputNodeId();
			if (OutputNodeId.IsSet())
				NodeToConnectTo = OutputNodeId;
		}
	}

	// If this is the output chain, set it its display node
	if (OutputChainName == InChainName)
	{
		const FUnrealObjectInputHAPINodeId OutputNodeId = GetOutputNodeIdOfModifierChain(OutputChainName);
		if (OutputNodeId.IsValid())
		{
			IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
			if (Manager)
				Manager->SetHAPINodeDisplay(OutputNodeId, true);
		}
	}
	
	return bSuccess;
}

bool
FUnrealObjectInputNode::UpdateAllModifierChains()
{
	bool bSuccess = true;
	TArray<FName> ChainNames;
	ModifierChains.GetKeys(ChainNames);
	for (const FName& ChainName : ChainNames)
	{
		if (!UpdateModifiers(ChainName))
			bSuccess = false;
	}

	

	return bSuccess;
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
{
	
}

FUnrealObjectInputLeafNode::FUnrealObjectInputLeafNode(const FUnrealObjectInputIdentifier& InIdentifier, const FUnrealObjectInputHandle& InParent, const int32 InObjectNodeId, const int32 InNodeId)
	: FUnrealObjectInputNode(InIdentifier, InParent, InNodeId)
{
	ObjectNodeId.Set(InObjectNodeId);
}

FUnrealObjectInputLeafNode::~FUnrealObjectInputLeafNode()
{
	if (AreHAPINodesValid())
		DeleteHAPINodes();
}

bool
FUnrealObjectInputLeafNode::AreHAPINodesValid() const
{
	if (!ObjectNodeId.IsValid())
		return false;

	if (!FUnrealObjectInputNode::AreHAPINodesValid())
		return false;

	return true;
}

bool FUnrealObjectInputLeafNode::DeleteHAPINodes()
{
	// If the node cant be deleted, return true
	if (!CanBeDeleted())
		return true;

	if (!FUnrealObjectInputNode::DeleteHAPINodes())
		return false;
	
	if (!ObjectNodeId.IsValid())
		return false;

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->DeleteHAPINode(ObjectNodeId);
}

void
FUnrealObjectInputLeafNode::GetHAPINodeIds(TArray<FUnrealObjectInputHAPINodeId>& OutNodeIds) const
{
	FUnrealObjectInputNode::GetHAPINodeIds(OutNodeIds);

	if (!ObjectNodeId.IsValid())
		return;

	OutNodeIds.Add(ObjectNodeId);
}

FUnrealObjectInputReferenceNode::FUnrealObjectInputReferenceNode(const FUnrealObjectInputIdentifier& InIdentifier)
	: FUnrealObjectInputLeafNode(InIdentifier)
{
	
}

FUnrealObjectInputReferenceNode::FUnrealObjectInputReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const FUnrealObjectInputHandle& InParent,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const int32 InReferencesConnectToNodeId)
	: FUnrealObjectInputLeafNode(InIdentifier, InParent, InObjectNodeId, InNodeId)
{
	SetReferencesConnectToNodeId(InReferencesConnectToNodeId);
}

FUnrealObjectInputReferenceNode::FUnrealObjectInputReferenceNode(
		const FUnrealObjectInputIdentifier& InIdentifier,
		const FUnrealObjectInputHandle& InParent,
		const int32 InObjectNodeId,
		const int32 InNodeId,
		const TSet<FUnrealObjectInputHandle>& InReferencedNodes,
		const int32 InReferencesConnectToNodeId)
	: FUnrealObjectInputReferenceNode(InIdentifier, InParent, InObjectNodeId, InNodeId, InReferencesConnectToNodeId)
{
	SetReferencedNodes(InReferencedNodes);
}

bool FUnrealObjectInputReferenceNode::AddReferencedNode(const FUnrealObjectInputHandle& InHandle)
{
	bool bAlreadySet = false;
	ReferencedNodes.Add(
		FUnrealObjectInputBackLinkHandle(GetIdentifier(), InHandle.GetIdentifier()),
		&bAlreadySet);

	return !bAlreadySet;
}

void FUnrealObjectInputReferenceNode::SetReferencedNodes(const TSet<FUnrealObjectInputHandle>& InReferencedNodes)
{
	ReferencedNodes.Empty(InReferencedNodes.Num());
	const FUnrealObjectInputIdentifier& MyIdentifier = GetIdentifier();
	for (const FUnrealObjectInputHandle& Handle : InReferencedNodes)
	{
		ReferencedNodes.Add(FUnrealObjectInputBackLinkHandle(MyIdentifier, Handle.GetIdentifier()));
	}
}

void FUnrealObjectInputReferenceNode::GetReferencedNodes(TSet<FUnrealObjectInputHandle>& OutReferencedNodes) const
{
	OutReferencedNodes.Empty(ReferencedNodes.Num());
	for (const FUnrealObjectInputBackLinkHandle& Handle : ReferencedNodes)
	{
		OutReferencedNodes.Add(Handle);
	}
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


// Marks this and its referenced nodes as dirty. See IsDirty().
void FUnrealObjectInputReferenceNode::MarkAsDirty(const bool bInAlsoDirtyReferencedNodes)
{
	MarkAsDirty();
	
	if (!bInAlsoDirtyReferencedNodes || ReferencedNodes.IsEmpty())
		return;

	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return;

	for (const FUnrealObjectInputHandle& Handle : ReferencedNodes)
	{
		Manager->MarkAsDirty(Handle.GetIdentifier(), bInAlsoDirtyReferencedNodes);
	}
}


void
FUnrealObjectInputReferenceNode::SetReferencesConnectToNodeId(const int32 InReferencesConnectToNodeId)
{
	if (InReferencesConnectToNodeId < 0)
	{
		ReferencesConnectToNodeId.Reset();
	}
	else
	{
		FUnrealObjectInputHAPINodeId ConnectToNodeId;
		ConnectToNodeId.Set(InReferencesConnectToNodeId);
		ReferencesConnectToNodeId = ConnectToNodeId;
	}
}


bool
FUnrealObjectInputModifier::DestroyHAPINodes()
{
	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	bool bSuccess = true;
	for (const FUnrealObjectInputHAPINodeId NodeId : HAPINodeIds)
	{
		if (!NodeId.IsValid())
			continue;
		// TODO: can we ensure that node input and output nodes are linked after deleting node?
		if (!Manager->DeleteHAPINode(NodeId))
			bSuccess = false;
	}
	HAPINodeIds.Empty();

	return bSuccess;
}

FUnrealObjectInputUpdateScope::FUnrealObjectInputUpdateScope()
{
	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (Manager)
	{
		OnCreatedHandle = Manager->GetOnNodeAddedDelegate().AddRaw(this, &FUnrealObjectInputUpdateScope::OnNodeCreatedOrUpdated);
		OnUpdatedHandle = Manager->GetOnNodeUpdatedDelegate().AddRaw(this, &FUnrealObjectInputUpdateScope::OnNodeCreatedOrUpdated);
		OnDestroyedHandle = Manager->GetOnNodeDeletedDelegate().AddRaw(this, &FUnrealObjectInputUpdateScope::OnNodeDestroyed);
	}
}

FUnrealObjectInputUpdateScope::~FUnrealObjectInputUpdateScope()
{
	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (Manager)
	{
		if (OnDestroyedHandle.IsValid())
			Manager->GetOnNodeDeletedDelegate().Remove(OnDestroyedHandle);
		if (OnUpdatedHandle.IsValid())
			Manager->GetOnNodeUpdatedDelegate().Remove(OnUpdatedHandle);
		if (OnCreatedHandle.IsValid())
			Manager->GetOnNodeAddedDelegate().Remove(OnCreatedHandle);
	}
}

void FUnrealObjectInputUpdateScope::OnNodeCreatedOrUpdated(const FUnrealObjectInputIdentifier& InIdentifier)
{
	NodesDestroyed.Remove(InIdentifier);
	NodesCreatedOrUpdated.Add(InIdentifier);
}

void FUnrealObjectInputUpdateScope::OnNodeDestroyed(const FUnrealObjectInputIdentifier& InIdentifier)
{
	NodesCreatedOrUpdated.Remove(InIdentifier);
	NodesDestroyed.Add(InIdentifier);
}
