/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"


UHoudiniAssetObject::UHoudiniAssetObject(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	Material(nullptr)
{

}


void
UHoudiniAssetObject::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}


void
UHoudiniAssetObject::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetObject* HoudiniAssetObject = CastChecked<UHoudiniAssetObject>(InThis);
	if(HoudiniAssetObject)
	{
		// Manually add material reference.
		if(HoudiniAssetObject->Material)
		{
			Collector.AddReferencedObject(HoudiniAssetObject->Material, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetObject::ReleaseObjects(TArray<UHoudiniAssetObject*>& HoudiniAssetObjects)
{
	// If we have assets already in the list, remove them.
	if(HoudiniAssetObjects.Num() > 0)
	{
		for(TArray<UHoudiniAssetObject*>::TIterator Iter = HoudiniAssetObjects.CreateIterator(); Iter; ++Iter)
		{
			UHoudiniAssetObject* HoudiniAssetObject = *Iter;
			if(HoudiniAssetObject)
			{
				HoudiniAssetObject->ConditionalBeginDestroy();
			}
		}

		HoudiniAssetObjects.Empty();

		// Force GC.
		GEditor->GetWorld()->ForceGarbageCollection(true);
	}
}
