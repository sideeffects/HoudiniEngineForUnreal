/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"


UHoudiniAssetComponent::UHoudiniAssetComponent(const FPostConstructInitializeProperties& PCIP) : 
	Super(PCIP),
	AssetId(-1)
{
	PrimaryComponentTick.bCanEverTick = false;
}


void 
UHoudiniAssetComponent::OnRep_HoudiniAsset(UHoudiniAsset* OldHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("OnRep_HoudiniAsset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// Only do stuff if this actually changed from the last local value.
	if(OldHoudiniAsset != HoudiniAsset)
	{
		// We have to force a call to SetHoudiniAsset with a new HoudiniAsset.
		UHoudiniAsset* NewHoudiniAsset = HoudiniAsset;
		HoudiniAsset = NULL;

		SetHoudiniAsset(NewHoudiniAsset);
	}
}


void 
UHoudiniAssetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHoudiniAssetComponent, HoudiniAsset);
}


bool 
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("Setting asset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// Do nothing if we are already using the supplied Houdini asset.
	if(NewHoudiniAsset == HoudiniAsset)
	{
		return false;
	}

	// Don't allow changing Houdini assets if "static" and registered.
	AActor* Owner = GetOwner();
	if(Mobility == EComponentMobility::Static && IsRegistered() && Owner != NULL)
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetHoudiniAssetOnStatic", "Calling SetHoudiniAsset on '{0}' but Mobility is Static."), FText::FromString(GetPathName(this))));
		return false;
	}

	HoudiniAsset = NewHoudiniAsset;

	// Need to send this to render thread at some point.
	MarkRenderStateDirty();

	// Update physics representation right away.
	RecreatePhysicsState();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	GStreamingManager->NotifyPrimitiveAttached(this, DPT_Spawned);

	// Since we have new asset, we need to update bounds.
	UpdateBounds();
	return true;
}


int32 
UHoudiniAssetComponent::GetNumMaterials() const
{
	return 1;
}


FPrimitiveSceneProxy* 
UHoudiniAssetComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	
	if(HoudiniMeshTris.Num() > 0)
	{
		Proxy = new FHoudiniMeshSceneProxy(this);
	}

	return Proxy;
}


bool 
UHoudiniAssetComponent::DoesBelongToPreviewActor() const
{
	const TArray<TWeakObjectPtr<AActor> >& DropPreviewActors = FLevelEditorViewportClient::GetDropPreviewActors();
	AActor* OwnerActor = GetOwner();

	HOUDINI_LOG_MESSAGE(TEXT("We have %d preview actors."), DropPreviewActors.Num());

	for(auto ActorIt = DropPreviewActors.CreateConstIterator(); ActorIt; ++ActorIt)
	{
		AActor* PreviewActor = (*ActorIt).Get();
		if(PreviewActor == OwnerActor)
		{
			return true;
		}
	}

	return false;
}


/*
void 
UHoudiniAssetComponent::BeginDestroy()
{
	Super::BeginDestroy();
	HOUDINI_LOG_MESSAGE(TEXT("Starting destruction, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}

void 
UHoudiniAssetComponent::FinishDestroy()
{
	Super::FinishDestroy();
	HOUDINI_LOG_MESSAGE(TEXT("Finishing destruction, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}
*/


void 
UHoudiniAssetComponent::OnRegister()
{
	Super::OnRegister();
	HOUDINI_LOG_MESSAGE(TEXT("Registering component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// If this component does not belong to a preview actor.
	if(HoudiniAsset)
	{
		if(DoesBelongToPreviewActor())
		{
			HOUDINI_LOG_MESSAGE(TEXT("Belongs to preview actor!"));
		}
		else
		{
			HOUDINI_LOG_MESSAGE(TEXT("Does not belong to preview actor."));
		}
	}
}


void 
UHoudiniAssetComponent::OnUnregister()
{
	Super::OnUnregister();
	HOUDINI_LOG_MESSAGE(TEXT("Unregistering component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	if(HoudiniAsset)
	{
		if(DoesBelongToPreviewActor())
		{
			HOUDINI_LOG_MESSAGE(TEXT("Belongs to preview actor"));
		}
		else
		{
			HOUDINI_LOG_MESSAGE(TEXT("Does not belong to preview actor"));
		}
	}

	if(GetOwner() && GetOwner()->GetOwner())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Actor's owner = 0x%0.8p"), GetOwner()->GetOwner());
	}
}


void 
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();
	HOUDINI_LOG_MESSAGE(TEXT("Creating component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void 
UHoudiniAssetComponent::OnComponentDestroyed()
{
	Super::OnComponentDestroyed();
	HOUDINI_LOG_MESSAGE(TEXT("Destroying component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void 
UHoudiniAssetComponent::GetComponentInstanceData(FComponentInstanceDataCache& Cache) const
{
	// Called before we throw away components during RerunConstructionScripts, to cache any data we wish to persist across that operation.
	Super::GetComponentInstanceData(Cache);
	HOUDINI_LOG_MESSAGE(TEXT("Requesting data for caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void 
UHoudiniAssetComponent::ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache)
{
	// Called after we create new components during RerunConstructionScripts, to optionally apply any data backed up during GetComponentInstanceData.
	Super::ApplyComponentInstanceData(Cache);
	HOUDINI_LOG_MESSAGE(TEXT("Restoring data from caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}
