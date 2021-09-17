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

#include "HoudiniPublicAPIProcessHDANode.h"

#include "HoudiniPublicAPI.h"
#include "HoudiniPublicAPIBlueprintLib.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"

#include "HoudiniEngineRuntimePrivatePCH.h"


UHoudiniPublicAPIProcessHDANode::UHoudiniPublicAPIProcessHDANode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if ( HasAnyFlags(RF_ClassDefaultObject) == false )
	{
		AddToRoot();
	}

	AssetWrapper = nullptr;
	bCookSuccess = false;
	bBakeSuccess = false;
	
	HoudiniAsset = nullptr;
	InstantiateAt = FTransform::Identity;
	WorldContextObject = nullptr;
	SpawnInLevelOverride = nullptr;
	bEnableAutoCook = true;
	bEnableAutoBake = false;
	BakeDirectoryPath = FString();
	BakeMethod = EHoudiniEngineBakeOption::ToActor;
	bRemoveOutputAfterBake = false;
	bRecenterBakedActors = false;
	bReplacePreviousBake = false;
	bDeleteInstantiatedAssetOnCompletionOrFailure = false;
}

UHoudiniPublicAPIProcessHDANode*
UHoudiniPublicAPIProcessHDANode::ProcessHDA(
	UHoudiniAsset* InHoudiniAsset,
	const FTransform& InInstantiateAt,
	const TMap<FName, FHoudiniParameterTuple>& InParameters,
	const TMap<int32, UHoudiniPublicAPIInput*>& InNodeInputs,
	const TMap<FName, UHoudiniPublicAPIInput*>& InParameterInputs,
	UObject* InWorldContextObject,
	ULevel* InSpawnInLevelOverride,
	const bool bInEnableAutoCook,
	const bool bInEnableAutoBake,
	const FString& InBakeDirectoryPath,
	const EHoudiniEngineBakeOption InBakeMethod,
	const bool bInRemoveOutputAfterBake,
	const bool bInRecenterBakedActors,
	const bool bInReplacePreviousBake,
	const bool bInDeleteInstantiatedAssetOnCompletionOrFailure)
{
	UHoudiniPublicAPIProcessHDANode* Node = NewObject<UHoudiniPublicAPIProcessHDANode>();
	
	Node->HoudiniAsset = InHoudiniAsset;
	Node->InstantiateAt = InInstantiateAt;
	Node->Parameters = InParameters;
	Node->NodeInputs = InNodeInputs;
	Node->ParameterInputs = InParameterInputs;
	Node->WorldContextObject = InWorldContextObject;
	Node->SpawnInLevelOverride = InSpawnInLevelOverride;
	Node->bEnableAutoCook = bInEnableAutoCook;
	Node->bEnableAutoBake = bInEnableAutoBake;
	Node->BakeDirectoryPath = InBakeDirectoryPath;
	Node->BakeMethod = InBakeMethod;
	Node->bRemoveOutputAfterBake = bInRemoveOutputAfterBake;
	Node->bRecenterBakedActors = bInRecenterBakedActors;
	Node->bReplacePreviousBake = bInReplacePreviousBake;
	Node->bDeleteInstantiatedAssetOnCompletionOrFailure = bInDeleteInstantiatedAssetOnCompletionOrFailure;

	return Node;
}


void
UHoudiniPublicAPIProcessHDANode::Activate()
{
	UHoudiniPublicAPI* API = UHoudiniPublicAPIBlueprintLib::GetAPI();
	if (!IsValid(API))
	{
		HandleFailure();
		return;
	}

	AssetWrapper = UHoudiniPublicAPIAssetWrapper::CreateEmptyWrapper(API);
	if (!IsValid(AssetWrapper))
	{
		HandleFailure();
		return;
	}

	AssetWrapper->GetOnPreInstantiationDelegate().AddDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePreInstantiation);
	AssetWrapper->GetOnPostInstantiationDelegate().AddDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostInstantiation);
	AssetWrapper->GetOnPostCookDelegate().AddDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostAutoCook);
	AssetWrapper->GetOnPreProcessStateExitedDelegate().AddDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePreProcess);
	AssetWrapper->GetOnPostProcessingDelegate().AddDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostProcessing);
	AssetWrapper->GetOnPostBakeDelegate().AddDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostAutoBake);

	if (!API->InstantiateAssetWithExistingWrapper(
			AssetWrapper,
			HoudiniAsset,
			InstantiateAt,
			WorldContextObject,
			SpawnInLevelOverride,
			bEnableAutoCook,
			bEnableAutoBake,
			BakeDirectoryPath,
			BakeMethod,
			bRemoveOutputAfterBake,
			bRecenterBakedActors,
			bReplacePreviousBake))
	{
		HandleFailure();
		return;
	}
}

void
UHoudiniPublicAPIProcessHDANode::UnbindDelegates()
{
	AssetWrapper->GetOnPreInstantiationDelegate().RemoveDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePreInstantiation);
	AssetWrapper->GetOnPostInstantiationDelegate().RemoveDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostInstantiation);
	AssetWrapper->GetOnPostCookDelegate().RemoveDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostAutoCook);
	AssetWrapper->GetOnPreProcessStateExitedDelegate().RemoveDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePreProcess);
	AssetWrapper->GetOnPostProcessingDelegate().RemoveDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostProcessing);
	AssetWrapper->GetOnPostBakeDelegate().RemoveDynamic(this, &UHoudiniPublicAPIProcessHDANode::HandlePostAutoBake);
}

void
UHoudiniPublicAPIProcessHDANode::HandleFailure()
{
	if (Failed.IsBound())
		Failed.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);

	UnbindDelegates();

	RemoveFromRoot();

	if (bDeleteInstantiatedAssetOnCompletionOrFailure && IsValid(AssetWrapper))
		AssetWrapper->DeleteInstantiatedAsset();
}

void
UHoudiniPublicAPIProcessHDANode::HandleComplete()
{
	if (Completed.IsBound())
		Completed.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);

	UnbindDelegates();

	RemoveFromRoot();
	
	if (bDeleteInstantiatedAssetOnCompletionOrFailure && IsValid(AssetWrapper))
		AssetWrapper->DeleteInstantiatedAsset();
}

void
UHoudiniPublicAPIProcessHDANode::HandlePreInstantiation(UHoudiniPublicAPIAssetWrapper* InAssetWrapper)
{
	if (InAssetWrapper != AssetWrapper)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[UHoudiniPublicAPIProcessHDANode] Received delegate event from unexpected asset wrapper (%s vs %s)!"),
			IsValid(AssetWrapper) ? *(AssetWrapper->GetName()) : TEXT(""), 
			IsValid(InAssetWrapper) ? *(InAssetWrapper->GetName()) : TEXT(""));
		return;
	}

	// Set any parameters specified when the node was created 
	if (Parameters.Num() > 0 && IsValid(AssetWrapper))
	{
		AssetWrapper->SetParameterTuples(Parameters);
	}

	if (PreInstantiation.IsBound())
		PreInstantiation.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);
}

void
UHoudiniPublicAPIProcessHDANode::HandlePostInstantiation(UHoudiniPublicAPIAssetWrapper* InAssetWrapper)
{
	if (InAssetWrapper != AssetWrapper)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[UHoudiniPublicAPIProcessHDANode] Received delegate event from unexpected asset wrapper (%s vs %s)!"),
			IsValid(AssetWrapper) ? *(AssetWrapper->GetName()) : TEXT(""), 
			IsValid(InAssetWrapper) ? *(InAssetWrapper->GetName()) : TEXT(""));
		return;
	}

	// Set any inputs specified when the node was created
	if (IsValid(AssetWrapper))
	{
		if (NodeInputs.Num() > 0)
		{
			AssetWrapper->SetInputsAtIndices(NodeInputs);
		}
		if (ParameterInputs.Num() > 0)
		{
			AssetWrapper->SetInputParameters(ParameterInputs);
		}

		// // Set any parameters specified when the node was created 
		// if (Parameters.Num() > 0)
		// {
		// 	AssetWrapper->SetParameterTuples(Parameters);
		// }
	}

	if (PostInstantiation.IsBound())
		PostInstantiation.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);

	if (!bEnableAutoCook)
		HandleComplete();
}

void
UHoudiniPublicAPIProcessHDANode::HandlePostAutoCook(UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool bInCookSuccess)
{
	if (InAssetWrapper != AssetWrapper)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[UHoudiniPublicAPIProcessHDANode] Received delegate event from unexpected asset wrapper (%s vs %s)!"),
			IsValid(AssetWrapper) ? *(AssetWrapper->GetName()) : TEXT(""), 
			IsValid(InAssetWrapper) ? *(InAssetWrapper->GetName()) : TEXT(""));
		return;
	}

	bCookSuccess = bInCookSuccess;

	if (PostAutoCook.IsBound())
		PostAutoCook.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);
}

void
UHoudiniPublicAPIProcessHDANode::HandlePreProcess(UHoudiniPublicAPIAssetWrapper* InAssetWrapper)
{
	if (InAssetWrapper != AssetWrapper)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[UHoudiniPublicAPIProcessHDANode] Received delegate event from unexpected asset wrapper (%s vs %s)!"),
			IsValid(AssetWrapper) ? *(AssetWrapper->GetName()) : TEXT(""), 
			IsValid(InAssetWrapper) ? *(InAssetWrapper->GetName()) : TEXT(""));
		return;
	}

	if (PreProcess.IsBound())
		PreProcess.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);
}

void
UHoudiniPublicAPIProcessHDANode::HandlePostProcessing(UHoudiniPublicAPIAssetWrapper* InAssetWrapper)
{
	if (InAssetWrapper != AssetWrapper)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[UHoudiniPublicAPIProcessHDANode] Received delegate event from unexpected asset wrapper (%s vs %s)!"),
			IsValid(AssetWrapper) ? *(AssetWrapper->GetName()) : TEXT(""), 
			IsValid(InAssetWrapper) ? *(InAssetWrapper->GetName()) : TEXT(""));
		return;
	}

	if (PostProcessing.IsBound())
		PostProcessing.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);

	if (!bEnableAutoBake)
		HandleComplete();
}

void
UHoudiniPublicAPIProcessHDANode::HandlePostAutoBake(UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool bInBakeSuccess)
{
	if (InAssetWrapper != AssetWrapper)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[UHoudiniPublicAPIProcessHDANode] Received delegate event from unexpected asset wrapper (%s vs %s)!"),
			IsValid(AssetWrapper) ? *(AssetWrapper->GetName()) : TEXT(""), 
			IsValid(InAssetWrapper) ? *(InAssetWrapper->GetName()) : TEXT(""));
		return;
	}

	bBakeSuccess = bInBakeSuccess;

	if (PostAutoBake.IsBound())
		PostAutoBake.Broadcast(AssetWrapper, bCookSuccess, bBakeSuccess);

	HandleComplete();
}
