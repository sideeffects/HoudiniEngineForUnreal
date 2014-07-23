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


UHoudiniAssetMaterial::UHoudiniAssetMaterial(const class FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{

}


void
UHoudiniAssetMaterial::AddGeneratedTexture(UTexture2D* Texture)
{
	GeneratedTextures.Add(Texture);
}


void
UHoudiniAssetMaterial::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetMaterial* HoudiniAssetMaterial = (UHoudiniAssetMaterial*) InThis;
	if(HoudiniAssetMaterial && !HoudiniAssetMaterial->IsPendingKill())
	{
		// Add all textures as dependencies.
		for(TArray<UTexture2D*>::TIterator Iter = HoudiniAssetMaterial->GeneratedTextures.CreateIterator(); Iter; ++Iter)
		{
			UTexture2D* Texture = *Iter;
			Collector.AddReferencedObject(Texture, InThis);
		}
	}
}
