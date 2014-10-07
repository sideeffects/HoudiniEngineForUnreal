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
	OriginalStaticMesh(InStaticMesh)
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
		Component->ClearInstances();

		for(int32 InstanceIdx = 0; InstanceIdx < Transformations.Num(); ++InstanceIdx)
		{
			Component->AddInstance(Transformations[InstanceIdx]);
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
