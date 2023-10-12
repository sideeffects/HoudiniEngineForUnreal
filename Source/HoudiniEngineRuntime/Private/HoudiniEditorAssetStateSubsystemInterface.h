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

#include "CoreTypes.h"

#include "HoudiniAssetStateTypes.h"

#if WITH_EDITOR 
/**
 * Interface for the HoudiniEditorAssetStateSubsystem. The interface defines / give us access to the API of the
 * HoudiniEditorAssetStateSubsystem in the runtime module, while the implementation of the editor subsystem is in
 * the editor module.
 *
 * Based on WorldPartition/ContentBundle/ContentBundleEditorSubsystemInterface.h
 */
class IHoudiniEditorAssetStateSubsystemInterface
{

public:
	static IHoudiniEditorAssetStateSubsystemInterface* Get() { return Instance; }

	virtual ~IHoudiniEditorAssetStateSubsystemInterface() {};

protected:
	// This should be called by the HoudiniEngineManager to notify the subsystem that a Houdini Asset object, such
	// as a HoudiniAssetComponent, has changed state.
	virtual void NotifyOfHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState) = 0;

	friend class UHoudiniAssetComponent;

	static HOUDINIENGINERUNTIME_API void SetInstance(IHoudiniEditorAssetStateSubsystemInterface* InInstance);
	static HOUDINIENGINERUNTIME_API IHoudiniEditorAssetStateSubsystemInterface* Instance;
};
#endif
