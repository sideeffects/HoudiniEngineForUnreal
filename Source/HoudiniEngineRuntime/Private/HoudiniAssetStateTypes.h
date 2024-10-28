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
#include "UObject/ObjectMacros.h"


UENUM()
enum class EHoudiniAssetState : uint8
{
	// Loaded / Duplicated HDA,
	// Will need to be instantiated upon change/update
	NeedInstantiation,

	// Newly created HDA, fetch its default parameters then proceed to PreInstantiation
	NewHDA,

	// Newly created HDA, after default parameters fetch, needs to be instantiated immediately
	PreInstantiation,

	// Instantiating task in progress
	Instantiating,	

	// Instantiated HDA, needs to be cooked immediately
	PreCook,

	// Cooking task in progress
	Cooking,

	// Cooking has finished
	PostCook,

	// Cooked HDA, needs to be processed immediately
	PreProcess,

	// Processing task in progress
	Processing,

	// Processed / Updated HDA
	// Will need to be cooked upon change/update
	None,

	// Asset needs to be rebuilt (Deleted/Instantiated/Cooked)
	NeedRebuild,

	// Asset needs to be deleted
	NeedDelete,

	// Deleting
	Deleting,

	// Process component template. This is ticking has very limited
	// functionality, typically limited to checking for parameter updates
	// in order to trigger PostEditChange() to run construction scripts again.
	ProcessTemplate,

	// Used when an HDA has been instanced using Level Instances.
	Dormant,
};

UENUM()
enum class EHoudiniAssetStateResult : uint8
{
	None,
	Working,
	Success,
	FinishedWithError,
	FinishedWithFatalError,
	Aborted
};
