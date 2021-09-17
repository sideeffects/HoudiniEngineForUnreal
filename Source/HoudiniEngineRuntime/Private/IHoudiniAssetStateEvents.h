/*
* Copyright (c) <2018> Side Effects Software Inc.
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
#include "UObject/Interface.h"

#include "HoudiniAssetStateTypes.h"

#include "IHoudiniAssetStateEvents.generated.h"

// Delegate for when EHoudiniAssetState changes from InFromState to InToState on an instantiated Houdini Asset (InHoudiniAssetContext).
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnHoudiniAssetStateChange, UObject*, const EHoudiniAssetState, const EHoudiniAssetState);

UINTERFACE()
class HOUDINIENGINERUNTIME_API UHoudiniAssetStateEvents : public UInterface
{
	GENERATED_BODY()
};


/**
 * EHoudiniAssetState events: event handlers for when a Houdini Asset changes state.
 */
class HOUDINIENGINERUNTIME_API IHoudiniAssetStateEvents
{
	GENERATED_BODY()

public:
	virtual void HandleOnHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState);

	virtual FOnHoudiniAssetStateChange& GetOnHoudiniAssetStateChangeDelegate() = 0;
};
