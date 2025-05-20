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


#include "HoudiniEditorAssetStateSubsystem.h"
#include "HoudiniAssetStateTypes.h"
#include "HoudiniCookable.h"
#include "HoudiniEngineBakeUtils.h"

#include "UObject/Object.h"

void
UHoudiniEditorAssetStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	IHoudiniEditorAssetStateSubsystemInterface::SetInstance(this);
}

void
UHoudiniEditorAssetStateSubsystem::Deinitialize()
{
	IHoudiniEditorAssetStateSubsystemInterface::SetInstance(nullptr);
}

void
UHoudiniEditorAssetStateSubsystem::NotifyOfHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState)
{
	if (!IsValid(InHoudiniAssetContext))
		return;

	UHoudiniCookable* const HC = Cast<UHoudiniCookable>(InHoudiniAssetContext);
	if (!IsValid(HC))
		return;

	// If we went from PostCook -> PreProcess, the cook was successful, and auto bake is enabled, auto bake!
	if (InFromState == EHoudiniAssetState::PostCook && InToState == EHoudiniAssetState::PreProcess && HC->WasLastCookSuccessful() && HC->IsBakeAfterNextCookEnabled())
	{
		FHoudiniBakeSettings BakeSettings;
		BakeSettings.SetFromCookable(HC);
		FHoudiniEngineBakeUtils::BakeCookable(
			HC,
			BakeSettings,
			HC->GetBakingData()->HoudiniEngineBakeOption,
			HC->GetBakingData()->bRemoveOutputAfterBake);

		if (HC->GetBakeAfterNextCook() == EHoudiniBakeAfterNextCook::Once)
			HC->SetBakeAfterNextCook(EHoudiniBakeAfterNextCook::Disabled);
	}

	// Done, no need to bake for the HC
	return;
}
