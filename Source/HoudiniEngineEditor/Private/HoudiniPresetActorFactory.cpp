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

#include "HoudiniPresetActorFactory.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniPreset.h"
#include "HoudiniToolsEditor.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniPresetActorFactory::UHoudiniPresetActorFactory(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("HoudiniAssetDisplayName", "Houdini Engine Asset");
	NewActorClass = AHoudiniAssetActor::StaticClass();
}

bool
UHoudiniPresetActorFactory::CanCreateActorFrom(const FAssetData & AssetData, FText & OutErrorMsg)
{
	UHoudiniPreset* Preset = Cast<UHoudiniPreset>(AssetData.GetAsset() );
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UHoudiniPreset::StaticClass()) || !IsValid(Preset))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoHoudiniPreset", "A valid Houdini Engine Preset asset must be specified.");
		return false;
	}

	
	if (!IsValid(Preset->SourceHoudiniAsset))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoHoudiniPresetSourceAsset", "The preset requires a source asset.");
		return false;
	}

	return true;
}

UObject *
UHoudiniPresetActorFactory::GetAssetFromActorInstance(AActor * Instance)
{
	check(Instance->IsA(NewActorClass));
	AHoudiniAssetActor * HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(Instance);

	check(HoudiniAssetActor->GetHoudiniAssetComponent());
	return HoudiniAssetActor->GetHoudiniAssetComponent()->HoudiniAsset;
}

void
UHoudiniPresetActorFactory::PostSpawnActor(UObject* InObject, AActor* NewActor)
{
	HOUDINI_LOG_MESSAGE(TEXT("PostSpawnActor (Preset) %s, supplied Preset = 0x%0.8p"), *NewActor->GetActorNameOrLabel(), InObject);
	UHoudiniPreset* Preset = Cast<UHoudiniPreset>(InObject);
	if (!IsValid(Preset))
	{
		return;
	}

	UHoudiniAsset* HoudiniAsset = Preset->SourceHoudiniAsset;
	if (HoudiniAsset)
	{
		AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(NewActor);
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		check(HoudiniAssetComponent);

		FHoudiniEngineUtils::AddHoudiniLogoToComponent(HoudiniAssetComponent);

		if (!HoudiniAssetActor->IsUsedForPreview())
		{
			HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);
			FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(HoudiniAssetComponent);

			// Apply the preset once the HoudiniAssetComponent has reached the PreCookCallback (which is when
			// both the HAC inputs and parameters have been initialized).
			HoudiniAssetComponent->QueuePreCookCallback([Preset](UHoudiniAssetComponent* InHoudiniAssetComponent)
			{
				if (IsValid(InHoudiniAssetComponent) && IsValid(Preset))
				{
					FHoudiniToolsEditor::ApplyPresetToHoudiniAssetComponent(Preset, InHoudiniAssetComponent);
				}
			});
		}
	}
}

void
UHoudiniPresetActorFactory::PostCreateBlueprint(UObject* InObject, AActor * CDO)
{
	HOUDINI_LOG_MESSAGE(TEXT("PostCreateBlueprint, supplied Asset = 0x%0.8p"), InObject);
	
	UHoudiniPreset* Preset = Cast<UHoudiniPreset>(InObject);
	if (!IsValid(Preset))
	{
		return;
	}

	UHoudiniAsset* HoudiniAsset = Preset->SourceHoudiniAsset;
	if (HoudiniAsset)
	{
		AHoudiniAssetActor * HoudiniAssetActor = CastChecked< AHoudiniAssetActor >(CDO);
		UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		check(HoudiniAssetComponent);

		FHoudiniEngineUtils::AddHoudiniLogoToComponent(HoudiniAssetComponent);

		if (!HoudiniAssetActor->IsUsedForPreview())
		{
			HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);
			FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(HoudiniAssetComponent);
			// We probably need to wait for a cook before we can apply the preset?
			FHoudiniToolsEditor::ApplyPresetToHoudiniAssetComponent(Preset, HoudiniAssetComponent);
		}
	}
}

#undef LOCTEXT_NAMESPACE