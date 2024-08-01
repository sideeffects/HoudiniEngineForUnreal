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

#include "HoudiniAssetComponent.h"

#include "CoreMinimal.h"

#include "HoudiniNodeSyncComponent.generated.h"


UENUM()
enum class EHoudiniNodeSyncStatus : uint8
{
	
	None,				// Fetch/Send not used yet
	Failed,				// Last operation failed
	Success,			// Last operation was successful
	SuccessWithErrors,	// Last operation was successful, but reported errors
	Running,			// Sending/Fetching
	Warning				// Display a warning
};


UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, Activation, "Components|Activation", HoudiniAsset), ShowCategories = (Mobility), editinlinenew)
class HOUDINIENGINERUNTIME_API UHoudiniNodeSyncComponent : public UHoudiniAssetComponent
{
	GENERATED_BODY()

public:
	UHoudiniNodeSyncComponent(const FObjectInitializer & ObjectInitializer);

	virtual bool IsValidComponent() const override;

	virtual FString GetHoudiniAssetName() const override;

	void SetHoudiniAssetState(EHoudiniAssetState InNewState) { SetAssetState(InNewState); };

	void SetFetchNodePath(const FString& InNodePath) { FetchNodePath = InNodePath; };
	FString GetFetchNodePath() { return FetchNodePath; };

	void SetLiveSyncEnabled(const bool& bEnableLiveSync) { bLiveSyncEnabled = bEnableLiveSync; };
	bool GetLiveSyncEnabled() { return bLiveSyncEnabled; };

	// Whether or not this component should be able to delete the Houdini nodes
	// that correspond to the HoudiniAsset when being deregistered.
	// Node Sync component shall NOT delete nodes!
	virtual bool CanDeleteHoudiniNodes() const { return false; }

	// Node Sync component should never auto start a session without being touched
	virtual bool ShouldTryToStartFirstSession() const override { return false; };

	//
	// Public Members
	//
	
	// Last status
	EHoudiniNodeSyncStatus FetchStatus;
	// Last Fetch message
	FString FetchMessage;

protected:

	UPROPERTY()
	FString FetchNodePath;

	UPROPERTY()
	bool bLiveSyncEnabled;
};
