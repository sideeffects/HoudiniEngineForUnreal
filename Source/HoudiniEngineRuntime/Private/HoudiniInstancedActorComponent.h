/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Chris Grebeldinger
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once

#include "HoudiniInstancedActorComponent.generated.h"

UCLASS( config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniInstancedActorComponent : public USceneComponent
{
   GENERATED_UCLASS_BODY()

public:
    virtual void OnComponentCreated() override;
    virtual void OnComponentDestroyed( bool bDestroyingHierarchy ) override;
    virtual void Serialize( FArchive & Ar ) override;

    static void AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector );
    
    /** Set the instances. Transforms are given in local space of this component. */
    void SetInstances( const TArray<FTransform>& InstanceTransforms );

    /** Add an instance to this component. Transform is given in local space of this component. */
    int32 AddInstance( const FTransform& InstanceTransform );
    
    /** Destroy all extant instances */
    void ClearInstances();

    /** Spawn a single instance */
    AActor* SpawnInstancedActor( const FTransform& InstancedTransform ) const;

    UPROPERTY( SkipSerialization, VisibleAnywhere, Category = Instances )
    UObject* InstancedAsset;

    UPROPERTY( SkipSerialization, VisibleInstanceOnly, Category = Instances )
    TArray< AActor* > Instances;

};
