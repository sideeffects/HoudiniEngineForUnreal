/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Represents a Houdini Asset in the UE4 Content Browser.
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

#pragma once
#include "HoudiniGeoPartObject.h"
#include "HoudiniAssetComponentMaterials.generated.h"


class UMaterial;
class UMaterialInterface;
class UHoudiniAssetComponent;


UCLASS(EditInlineNew, config=Engine)
class HOUDINIENGINERUNTIME_API UHoudiniAssetComponentMaterials : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniAssetComponent;
	friend struct FHoudiniEngineUtils;

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:

	/** Reset the object. **/
	void ResetMaterialInfo();

protected:

	/** Material assignments. **/
	TMap<FString, UMaterial*> Assignments;

	/** Material replacements. **/
	TMap<FHoudiniGeoPartObject, TMap<FString, UMaterialInterface*> > Replacements;

	/** Flags used by this instance. **/
	uint32 HoudiniAssetComponentMaterialsFlagsPacked;
};
