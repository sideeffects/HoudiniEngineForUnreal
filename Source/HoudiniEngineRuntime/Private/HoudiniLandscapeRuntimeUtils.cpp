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

#include "HoudiniLandscapeRuntimeUtils.h"
#include "HoudiniOutput.h"
#include "LandscapeEdit.h"
#include "HoudiniAssetComponent.h"
#include "Landscape.h"
#include "HoudiniAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LandscapeStreamingProxy.h"

void FHoudiniLandscapeRuntimeUtils::DeleteLandscapeCookedData(UHoudiniOutput* InOutput)
{
	TSet<ALandscape*> LandscapesToDelete;

	for (auto& OutputObjectPair : InOutput->GetOutputObjects())
	{
		const FHoudiniOutputObject& PrevObj = OutputObjectPair.Value;
		if (IsValid(PrevObj.OutputObject) && PrevObj.OutputObject->IsA<UHoudiniLandscapeTargetLayerOutput>())
		{
			// Get the layer data stored during cooking
			UHoudiniLandscapeTargetLayerOutput* OldLayer = Cast<UHoudiniLandscapeTargetLayerOutput>(PrevObj.OutputObject);

			// Check if the data is valid; possibly a user deleted the cooked data by hand.
			if (!IsValid(OldLayer->Landscape))
				continue;

			// Delete the edit layers
			if (OldLayer->BakedEditLayer != OldLayer->CookedEditLayer)
			{
				DeleteEditLayer(OldLayer->Landscape, FName(OldLayer->CookedEditLayer));
			}

			// Remove any landscapes that were created.
			if (OldLayer->bCreatedLandscape && OldLayer->Landscape)
				LandscapesToDelete.Add(OldLayer->Landscape);
		}
	}

	for (ALandscape* Landscape : LandscapesToDelete)
		DestroyLandscape(Landscape);
}

void FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(ALandscape* Landscape, const FName& LayerName)
{
#if WITH_EDITOR
	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	if (EditLayerIndex == INDEX_NONE)
		return;
	Landscape->DeleteLayer(EditLayerIndex);
#endif
}


void
FHoudiniLandscapeRuntimeUtils::DestroyLandscape(ALandscape* Landscape)
{
	if (!IsValid(Landscape))
		return;

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!IsValid(Info))
		return;
#if ENGINE_MINOR_VERSION < 1
	TArray<ALandscapeStreamingProxy*> Proxies = Info->Proxies;
	for (ALandscapeStreamingProxy* Proxy : Proxies)
	{
#else
	TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Info->StreamingProxies;
	for (auto ProxyPtr : Proxies)
	{
		ALandscapeStreamingProxy* Proxy = ProxyPtr.Get();
#endif	
		if (!IsValid(Proxy))
			continue;

		Info->UnregisterActor(Proxy);
		FHoudiniLandscapeRuntimeUtils::DestroyLandscapeProxy(Proxy);
	}
	Landscape->Destroy();
}

void
FHoudiniLandscapeRuntimeUtils::DestroyLandscapeProxy(ALandscapeProxy* Proxy)
{
	// UE5 does not automatically delete streaming proxies, so clean them up before detroying the parent actor.
	// Note the Proxies array must be copied, not referenced, as it is modified when each Proxy is destroyed.

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Proxy->GetLandscapeInfo()->StreamingProxies;
#else
	TArray<TObjectPtr<ALandscapeStreamingProxy>> Proxies = Proxy->GetLandscapeInfo()->Proxies;
#endif

	for (TWeakObjectPtr<ALandscapeStreamingProxy> Child : Proxies)
	{
		if (Child.Get() != nullptr && IsValid(Child.Get()))
			Child.Get()->Destroy();
	}

	// Once we've removed the streaming proxies delete the landscape actor itself.

	Proxy->Destroy();
}
