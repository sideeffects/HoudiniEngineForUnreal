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
	Super(PCIP)
{
	PrimaryComponentTick.bCanEverTick = false;
}


void 
UHoudiniAssetComponent::OnRep_HoudiniAsset(UHoudiniAsset* OldHoudiniAsset)
{
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

	// Need to send this to render thread at some point
	MarkRenderStateDirty();

	// Update physics representation right away
	RecreatePhysicsState();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	GStreamingManager->NotifyPrimitiveAttached(this, DPT_Spawned);

	// Since we have new mesh, we need to update bounds
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


void UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	
	Super::OnComponentCreated();



}


void 
UHoudiniAssetComponent::OnComponentDestroyed()
{
	Super::OnComponentDestroyed();
}


void 
UHoudiniAssetComponent::GetComponentInstanceData(FComponentInstanceDataCache& Cache) const
{
	Super::GetComponentInstanceData(Cache);
}


void 
UHoudiniAssetComponent::ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache)
{
	Super::ApplyComponentInstanceData(Cache);
}
