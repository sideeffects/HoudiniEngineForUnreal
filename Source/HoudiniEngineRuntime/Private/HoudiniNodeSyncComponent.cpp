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

#include "HoudiniNodeSyncComponent.h"

UHoudiniNodeSyncComponent::UHoudiniNodeSyncComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bForceNeedUpdate = false;

	// AssetState will be updated by changes to the HoudiniAsset
	// or parameter changes on the Component template.
	AssetState = EHoudiniAssetState::None;
	bHasBeenLoaded = false;

	/*
	// Disable proxy mesh by default (unsupported for now)
	bOverrideGlobalProxyStaticMeshSettings = true;
	bEnableProxyStaticMeshOverride = false;
	bEnableProxyStaticMeshRefinementByTimerOverride = false;
	bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = false;
	bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = false;
	StaticMeshMethod = EHoudiniStaticMeshMethod::RawMesh;
	*/

	// Set default mobility to Movable
	Mobility = EComponentMobility::Movable;

	bLiveSyncEnabled = true;

	FetchStatus = EHoudiniNodeSyncStatus::None;
	FetchMessage = FString();
}


bool
UHoudiniNodeSyncComponent::IsValidComponent() const
{
	return true;
}

FString
UHoudiniNodeSyncComponent::GetHoudiniAssetName() const
{
	if (FetchNodePath.IsEmpty())
		return FString();

	// Extract the node name from our fetch node path
	int32 FoundPos = -1;
	if (FetchNodePath.FindLastChar(TEXT('/'), FoundPos))
	{
		return FetchNodePath.RightChop(FoundPos + 1);
	}
	else
	{
		return FetchNodePath;
	}
}