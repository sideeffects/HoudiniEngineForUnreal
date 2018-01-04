/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once

#include "Components/SceneComponent.h"
#include "HoudiniMeshSplitInstancerComponent.generated.h"

/**
* UHoudiniMeshSplitInstancerComponent is used to manage a single static mesh being
* 'instanced' multiple times by multiple UStaticMeshComponents.  This is as opposed to the
* UInstancedStaticMeshComponent wherein a signle mesh is instanced multiple times by one component.
*/
UCLASS( config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniMeshSplitInstancerComponent : public USceneComponent
{
   GENERATED_UCLASS_BODY()

public:
    virtual void OnComponentDestroyed( bool bDestroyingHierarchy ) override;
    virtual void Serialize( FArchive & Ar ) override;

    static void AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector );

    void SetStaticMesh(class UStaticMesh* StaticMesh) { InstancedMesh = StaticMesh; }
    class UStaticMesh* GetStaticMesh() const { return InstancedMesh; }

    void SetOverrideMaterial(class UMaterialInterface* MI) { OverrideMaterial = MI; }
    
    /** Set the instances. Transforms are given in local space of this component. */
    void SetInstances( const TArray<FTransform>& InstanceTransforms, const TArray<FLinearColor> & InstancedColors );
    
    /** Destroy all extant instances */
    void ClearInstances();

    const TArray< class UStaticMeshComponent* >& GetInstances() const { return Instances; }

private:
    UPROPERTY( SkipSerialization, VisibleInstanceOnly, Category = Instances )
    TArray< class UStaticMeshComponent* > Instances;

    UPROPERTY( SkipSerialization, VisibleInstanceOnly, Category=Instances)
    class UMaterialInterface* OverrideMaterial;

    UPROPERTY(SkipSerialization, VisibleAnywhere, Category = Instances )
    class UStaticMesh* InstancedMesh;
};
