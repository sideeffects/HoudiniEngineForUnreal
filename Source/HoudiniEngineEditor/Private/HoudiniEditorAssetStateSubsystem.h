/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "HoudiniEditorAssetStateSubsystemInterface.h"

#include "CoreTypes.h"
#include "EditorSubsystem.h"

#include "HoudiniEditorAssetStateSubsystem.generated.h"


// UE forward declarations
class UObject;

// Houdini Engine forward declarations
enum class EHoudiniAssetState : uint8;

/**
 * An editor subsystem that gets notified about HoudiniAssetComponent state changes. Currently this subsystem is used
 * for auto-baking functionality (receiving the state update from the HoudiniEngineRuntime module and then baking
 * the HDA via this subsystem (HoudiniEngineEditor module)).
 */
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorAssetStateSubsystem : public UEditorSubsystem, public IHoudiniEditorAssetStateSubsystemInterface
{
	GENERATED_BODY()

public:
	UHoudiniEditorAssetStateSubsystem() {}

	//~ Begin UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEditorSubsystem interface

protected:
	//~ Begin IHoudiniEditorAssetStateSubsystemInterface interface
	
	// This should be called by the HoudiniEngineManager to notify the subsystem that a Houdini Asset object, such
	// as a HoudiniAssetComponent, has changed state.
	virtual void NotifyOfHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState) override;

	//~ End IHoudiniEditorAssetStateSubsystemInterface interface
};
