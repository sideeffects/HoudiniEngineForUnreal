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
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetStateTypes.h"
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

	UHoudiniAssetComponent* const HAC = Cast<UHoudiniAssetComponent>(InHoudiniAssetContext);
	if (!IsValid(HAC))
		return;

	// If we went from PostCook -> PreProcess, the cook was successful, and auto bake is enabled, auto bake!
	if (InFromState == EHoudiniAssetState::PostCook && InToState == EHoudiniAssetState::PreProcess && HAC->WasLastCookSuccessful() && HAC->IsBakeAfterNextCookEnabled())
	{
		FHoudiniBakeSettings BakeSettings;
		BakeSettings.SetFromHAC(HAC);
		
		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
			HAC,
			BakeSettings,
			HAC->HoudiniEngineBakeOption,
			HAC->bRemoveOutputAfterBake);

		if (HAC->GetBakeAfterNextCook() == EHoudiniBakeAfterNextCook::Once)
			HAC->SetBakeAfterNextCook(EHoudiniBakeAfterNextCook::Disabled);
	}
}
