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

UCLASS(config=Editor)
class HOUDINIENGINE_API UHoudiniAssetActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

public: /** UActorFactory inherited methods. **/

	/** Return true if Actor can be created from a given asset. **/
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) OVERRIDE;

	/** Given an instance of an actor pertaining to this factory, find the asset that should be used to create a new actor. **/
	virtual UObject* GetAssetFromActorInstance(AActor* Instance) OVERRIDE;
	
	/** Subclasses may implement this to modify the actor after it has been spawned. **/
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) OVERRIDE;

	/** Override this in derived factory classes if needed.  This is called after a blueprint is created by this factory to **/
	/** update the blueprint's CDO properties with state from the asset for this factory.									**/
	virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) OVERRIDE;

	//virtual bool PreSpawnActor(UObject* Asset, FVector& InOutLocation, FRotator& InOutRotation, bool bRotationWasSupplied) OVERRIDE;
	//virtual AActor* SpawnActor(UObject* Asset, ULevel* InLevel, const FVector& Location, const FRotator& Rotation, EObjectFlags ObjectFlags, const FName& Name) OVERRIDE;
};
