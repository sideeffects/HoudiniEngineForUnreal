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

#include "HoudiniInstancedActorComponent.generated.h"


UCLASS()//( config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniInstancedActorComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

		friend class UHoudiniInstancedActorComponent_V1;
		friend class FHoudiniEditorEquivalenceUtils; 

	public:

		virtual void Serialize(FArchive & Ar) override;

		virtual void OnComponentCreated() override;
		virtual void OnComponentDestroyed( bool bDestroyingHierarchy ) override;

		static void AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector );

		// Object mutator
		void SetInstancedObject(class UObject* InObject);
		void SetInstancedActorClass(class UClass* InClass) { InstancedActorClass = InClass; }
		// Object accessor
		class UObject* GetInstancedObject() const { return InstancedObject; }
		class UClass* GetInstancedActorClass() const { return InstancedActorClass; }

		// Instance Accessor
		TArray<class AActor*>& GetInstancedActorsForWrite() { return InstancedActors; }
		// const Instance accessor
		const TArray<class AActor*>& GetInstancedActors() const { return InstancedActors; }

		// Returns the instanced actor at a given index
		AActor* GetInstancedActorAt(const int32& Idx) { return InstancedActors.IsValidIndex(Idx) ? InstancedActors[Idx] : nullptr; }

		// Add an instance to this component. Transform is given in local space of this component.
		int32 AddInstance(const FTransform& InstanceTransform, AActor * NewActor);

		// Sets the instance at a given index in this component. Transform is given in local space of this component. 
		bool SetInstanceAt(const int32& Idx, const FTransform& InstanceTransform, AActor * NewActor);

		// Updates the transform for a given actor. Transform is given in local space of this component.
		bool SetInstanceTransformAt(const int32& Idx, const FTransform& InstanceTransform);
    
		// Destroy all existing instances
		void ClearAllInstances();

		// Sets the number of instances needed
		// Properly deletes extras, new instance actors are nulled 
		void SetNumberOfInstances(const int32& NewInstanceNum);

		// Set the instances. Transforms are given in local space of this component.
		bool SetInstanceTransforms(const TArray<FTransform>& InstanceTransforms);
  
	private:

		UPROPERTY(VisibleAnywhere, Category = Instances )
		UObject* InstancedObject;

		UPROPERTY()
		UClass* InstancedActorClass;

		UPROPERTY(VisibleInstanceOnly, Category = Instances )
		TArray<AActor*> InstancedActors;

};
