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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponentMaterials.h"


UHoudiniAssetComponentMaterials::UHoudiniAssetComponentMaterials(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetComponentMaterialsFlagsPacked(0u)
{

}


void
UHoudiniAssetComponentMaterials::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << Assignments;
	Ar << Replacements;
}


void
UHoudiniAssetComponentMaterials::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetComponentMaterials* HoudiniAssetComponentMaterials = Cast<UHoudiniAssetComponentMaterials>(InThis);
	if(HoudiniAssetComponentMaterials)
	{
		// Add references to all cached materials.
		for(TMap<FString, UMaterial*>::TIterator
			Iter(HoudiniAssetComponentMaterials->Assignments); Iter; ++Iter)
		{
			UMaterial* Material = Iter.Value();
			Collector.AddReferencedObject(Material, InThis);
		}

		// Add references for replaced materials.
		for(TMap<FHoudiniGeoPartObject, TMap<FString, UMaterialInterface*> >::TIterator
			Iter(HoudiniAssetComponentMaterials->Replacements); Iter; ++Iter)
		{
			TMap<FString, UMaterialInterface*>& MaterialReplacementsValues = Iter.Value();

			for(TMap<FString, UMaterialInterface*>::TIterator
				IterInterfaces(MaterialReplacementsValues); IterInterfaces; ++IterInterfaces)
			{
				UMaterialInterface* MaterialInterface = IterInterfaces.Value();
				if(MaterialInterface)
				{
					Collector.AddReferencedObject(MaterialInterface, InThis);
				}
			}
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetComponentMaterials::ResetMaterialInfo()
{
	Assignments.Empty();
	Replacements.Empty();
}
