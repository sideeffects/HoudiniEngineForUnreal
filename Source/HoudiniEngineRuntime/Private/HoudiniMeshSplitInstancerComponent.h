/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "HoudiniMeshSplitInstancerComponent.generated.h"

/**
* UHoudiniMeshSplitInstancerComponent is used to manage a single static mesh being
* 'instanced' multiple times by multiple UStaticMeshComponents.  This is as opposed to the
* UInstancedStaticMeshComponent wherein a single mesh is instanced multiple times by one component.
*/

UCLASS()//( config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniMeshSplitInstancerComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniMeshSplitInstancerComponent_V1;
	friend class FHoudiniEditorEquivalenceUtils;

	public:

		virtual void Serialize(FArchive & Ar) override;

		virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

		static void AddReferencedObjects(UObject * InThis, FReferenceCollector & Collector);

		// Static Mesh mutator
		void SetStaticMesh(class UStaticMesh* StaticMesh) { InstancedMesh = StaticMesh; }

		// Static mesh accessor
		class UStaticMesh* GetStaticMesh() const { return InstancedMesh; }

		// Overide material mutator
		void SetOverrideMaterials(const TArray<class UMaterialInterface*>& InMaterialOverrides) { OverrideMaterials = InMaterialOverrides; }

		// Destroy existing instances, keeping a given number of them to be reused
		void ClearInstances(int32 NumToKeep);

		// Set the instances. Transforms are given in local space of this component.
		bool SetInstanceTransforms(const TArray<FTransform>& InstanceTransforms);
    		
		// Instance Accessor
		TArray<class UStaticMeshComponent*>& GetInstancesForWrite() { return Instances; }		
		// const Instance accessor
		const TArray<class UStaticMeshComponent*>& GetInstances() const { return Instances; }

		TArray<class UMaterialInterface*> GetOverrideMaterials() const { return OverrideMaterials; }

	private:

		UPROPERTY(VisibleInstanceOnly, Category = Instances)
		TArray<class UStaticMeshComponent*> Instances;

		UPROPERTY(VisibleInstanceOnly, Category = Instances)
		TArray<class UMaterialInterface*> OverrideMaterials;

		UPROPERTY(VisibleAnywhere, Category = Instances)
		class UStaticMesh* InstancedMesh;
};
