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

#include "HoudiniAssetActorFactory.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineUtils.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetActorFactory::UHoudiniAssetActorFactory(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("HoudiniAssetDisplayName", "Houdini Engine Asset");
	NewActorClass = AHoudiniAssetActor::StaticClass();
}

bool
UHoudiniAssetActorFactory::CanCreateActorFrom(const FAssetData & AssetData, FText & OutErrorMsg)
{
	if (AssetData.IsValid() && AssetData.IsInstanceOf(UHoudiniAsset::StaticClass()))
	{
		return true;
	}
	else
	{
		OutErrorMsg = NSLOCTEXT("HoudiniEngine", "NoHoudiniAsset", "A valid Houdini Engine asset must be specified.");
		return false;
	}
}

UObject *
UHoudiniAssetActorFactory::GetAssetFromActorInstance(AActor * Instance)
{
	check(Instance->IsA(NewActorClass));
	AHoudiniAssetActor * HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(Instance);

	check(HoudiniAssetActor->GetHoudiniAssetComponent());
	return HoudiniAssetActor->GetHoudiniAssetComponent()->HoudiniAsset;
}

void
UHoudiniAssetActorFactory::PostSpawnActor(UObject * Asset, AActor * NewActor)
{
	HOUDINI_LOG_MESSAGE(TEXT("PostSpawnActor %s, supplied Asset = 0x%0.8p"), *NewActor->GetActorNameOrLabel(), Asset);

	UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(Asset);
	AHoudiniAssetActor * HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(NewActor);
	UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
	check(HoudiniAssetComponent);

	FHoudiniEngineUtils::AddHoudiniLogoToComponent(HoudiniAssetComponent);

	if (!HoudiniAssetActor->IsUsedForPreview())
	{
		if (IsValid(HoudiniAsset))
		{
			HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);
		}
		FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(HoudiniAssetComponent);
	}
}

void
UHoudiniAssetActorFactory::PostCreateBlueprint(UObject * Asset, AActor * CDO)
{
	HOUDINI_LOG_MESSAGE(TEXT("PostCreateBlueprint, supplied Asset = 0x%0.8p"), Asset);

	UHoudiniAsset * HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
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
		}
	}
}

#undef LOCTEXT_NAMESPACE