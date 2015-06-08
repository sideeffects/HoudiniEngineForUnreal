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

#pragma once
#include "HoudiniAssetActorFactory.generated.h"


class FText;
class AActor;
class UObject;
class FAssetData;

UCLASS(config=Editor)
class UHoudiniAssetActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

/** UActorFactory methods. **/
public:

	/** Return true if Actor can be created from a given asset. **/
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;

	/** Given an instance of an actor pertaining to this factory, find the asset that should be used to create a new actor. **/
	virtual UObject* GetAssetFromActorInstance(AActor* Instance) override;

	/** Subclasses may implement this to modify the actor after it has been spawned. **/
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;

	/** Override this in derived factory classes if needed.  This is called after a blueprint is created by this factory to **/
	/** update the blueprint's CDO properties with state from the asset for this factory.									**/
	virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;
};
