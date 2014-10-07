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


FHoudiniEngineInstancer::FHoudiniEngineInstancer(UStaticMesh* InStaticMesh) :
	ObjectProperty(nullptr),
	Component(nullptr),
	StaticMesh(InStaticMesh),
	OriginalStaticMesh(InStaticMesh),
	bIsUsed(false)
{

}


void
FHoudiniEngineInstancer::Reset()
{
	// Reset instance transformations.
	Transformations.Empty();
}


void
FHoudiniEngineInstancer::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	StaticMesh = InStaticMesh;
}


void
FHoudiniEngineInstancer::SetObjectProperty(UObjectProperty* InObjectProperty)
{
	ObjectProperty = InObjectProperty;
}


void
FHoudiniEngineInstancer::SetInstancedStaticMeshComponent(UInstancedStaticMeshComponent* InComponent)
{
	Component = InComponent;
}


UInstancedStaticMeshComponent*
FHoudiniEngineInstancer::GetInstancedStaticMeshComponent() const
{
	return Component;
}


void
FHoudiniEngineInstancer::AddTransformation(const FTransform& Transform)
{
	Transformations.Add(Transform);
}


void
FHoudiniEngineInstancer::AddTransformations(const TArray<FTransform>& InTransforms)
{
	Transformations.Append(InTransforms);
}


void
FHoudiniEngineInstancer::AddInstancesToComponent()
{
	if(Component)
	{
		/*
		// Get the number of current instances.
		int32 ExistingInstanceCount = Component->GetInstanceCount();

		int32 InstanceIdx = 0;
		for(TArray<FTransform>::TIterator Iter(Transformations); Iter; ++Iter)
		{
			const FTransform& Transform = *Iter;
			const FVector& Scale3D = Transform.GetScale3D();

			// Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances. Happens in blueprint as well.
			if(!Scale3D.IsNearlyZero(SMALL_NUMBER))
			{
				if(InstanceIdx < ExistingInstanceCount)
				{
					// This is an existing instance, we can just update transformation.
					Component->UpdateInstanceTransform(InstanceIdx, Transform);
				}
				else
				{
					// This is a new instance, we need to add it.
					Component->AddInstance(Transform);
				}

				InstanceIdx++;
			}
		}

		// Deallocate unused instances.
		for(int32 RemoveIdx = ExistingInstanceCount - 1; InstanceIdx <= RemoveIdx; --RemoveIdx)
		{
			Component->RemoveInstance(RemoveIdx);
		}
		*/

		Component->ClearInstances();

		for(int32 InstanceIdx = 0; InstanceIdx < Transformations.Num(); ++InstanceIdx)
		{
			const FTransform& Transform = Transformations[InstanceIdx];
			const FVector& Scale3D = Transform.GetScale3D();

			// Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances. Happens in blueprint as well.
			if(!Scale3D.IsNearlyZero(SMALL_NUMBER))
			{
				Component->AddInstance(Transform);
			}
		}
	}
}


void
FHoudiniEngineInstancer::SetComponentStaticMesh()
{
	if(Component)
	{
		if(Component->StaticMesh != StaticMesh)
		{
			Component->SetStaticMesh(StaticMesh);
		}
	}
}


UStaticMesh*
FHoudiniEngineInstancer::GetStaticMesh() const
{
	return StaticMesh;
}


void
FHoudiniEngineInstancer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(StaticMesh);
	Collector.AddReferencedObject(OriginalStaticMesh);

	if(Component)
	{
		Collector.AddReferencedObject(Component);
	}

	//Collector.AddReferencedObject(ObjectProperty);
}


bool
FHoudiniEngineInstancer::IsUsed() const
{
	return bIsUsed;
}


void
FHoudiniEngineInstancer::MarkUsed(bool bUsed)
{
	bIsUsed = bUsed;
}


UObjectProperty*
FHoudiniEngineInstancer::GetObjectProperty() const
{
	return ObjectProperty;
}


UStaticMesh*
FHoudiniEngineInstancer::GetOriginalStaticMesh() const
{
	return OriginalStaticMesh;
}
