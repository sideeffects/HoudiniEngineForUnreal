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

#include "HoudiniInstancedActorComponent.h"

#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniCompatibilityHelpers.h"

#include "UObject/DevObjectVersion.h"
#include "Serialization/CustomVersion.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniInstancedActorComponent::UHoudiniInstancedActorComponent( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
, InstancedObject( nullptr )
{
	//
	// 	Set default component properties.
	//
	Mobility = EComponentMobility::Static;
	bCanEverAffectNavigation = true;
	bNeverNeedsRenderUpdate = false;
	Bounds = FBox(ForceInitToZero);
}


void
UHoudiniInstancedActorComponent::Serialize(FArchive& Ar)
{
	int64 InitialOffset = Ar.Tell();

	bool bLegacyComponent = false;
	if (Ar.IsLoading())
	{
		int32 Ver = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE && Ver >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
		{
			bLegacyComponent = true;
		}
	}

	if (bLegacyComponent)
	{
		// Legacy serialization
		// Either try to convert or skip depending on the setting value
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		bool bEnableBackwardCompatibility = HoudiniRuntimeSettings->bEnableBackwardCompatibility;
		if (bEnableBackwardCompatibility)
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniInstancedActorComponent : converting v1 object to v2."));

			Super::Serialize(Ar);

			UHoudiniInstancedActorComponent_V1* CompatibilityIAC = NewObject<UHoudiniInstancedActorComponent_V1>();
			CompatibilityIAC->Serialize(Ar);
			CompatibilityIAC->UpdateFromLegacyData(this);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniInstancedActorComponent : serialization will be skipped."));

			Super::Serialize(Ar);

			// Skip v1 Serialized data
			if (FLinker* Linker = Ar.GetLinker())
			{
				int32 const ExportIndex = this->GetLinkerIndex();
				FObjectExport& Export = Linker->ExportMap[ExportIndex];
				Ar.Seek(InitialOffset + Export.SerialSize);
				return;
			}
		}
	}
	else
	{
		// Normal v2 serialization
		Super::Serialize(Ar);
	}
}


void UHoudiniInstancedActorComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
    ClearAllInstances();
    Super::OnComponentDestroyed( bDestroyingHierarchy );
}


void 
UHoudiniInstancedActorComponent::AddReferencedObjects(UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniInstancedActorComponent * ThisHIAC = Cast< UHoudiniInstancedActorComponent >(InThis);
    if ( IsValid(ThisHIAC) )
    {
        if ( IsValid(ThisHIAC->InstancedObject) )
            Collector.AddReferencedObject( ThisHIAC->InstancedObject, ThisHIAC );

        Collector.AddReferencedObjects(ThisHIAC->InstancedActors, ThisHIAC );
    }
}


int32
UHoudiniInstancedActorComponent::AddInstance(const FTransform& InstanceTransform, AActor * NewActor)
{
	if (!IsValid(NewActor))
		return -1;

	NewActor->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	NewActor->SetActorRelativeTransform(InstanceTransform);
	return InstancedActors.Add(NewActor);
}


bool
UHoudiniInstancedActorComponent::SetInstanceAt(const int32& Idx, const FTransform& InstanceTransform, AActor * NewActor)
{
	if (!IsValid(NewActor))
		return false;

	if (!InstancedActors.IsValidIndex(Idx))
		return false;

	NewActor->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	NewActor->SetActorRelativeTransform(InstanceTransform);
	NewActor->RegisterAllComponents();
	InstancedActors[Idx] = NewActor;

	return true;
}


bool
UHoudiniInstancedActorComponent::SetInstanceTransformAt(const int32& Idx, const FTransform& InstanceTransform)
{
	if (!InstancedActors.IsValidIndex(Idx))
		return false;

	InstancedActors[Idx]->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	InstancedActors[Idx]->SetActorRelativeTransform(InstanceTransform);

	return true;
}


void 
UHoudiniInstancedActorComponent::ClearAllInstances()
{
    for ( AActor* Instance : InstancedActors )
    {
        if ( IsValid(Instance) )
            Instance->Destroy();
    }
    InstancedActors.Empty();
}


void
UHoudiniInstancedActorComponent::SetNumberOfInstances(const int32& NewInstanceNum)
{
	int32 OldInstanceNum = InstancedActors.Num();

	// If we want less instances than we already have, destroy the extra properly
	if (NewInstanceNum < OldInstanceNum)
	{
		for (int32 Idx = NewInstanceNum - 1; Idx < InstancedActors.Num(); Idx++)
		{
			AActor* Instance = InstancedActors.IsValidIndex(Idx) ? InstancedActors[Idx] : nullptr;
			if (IsValid(Instance))
				Instance->Destroy();
		}
	}
	
	// Grow the array with nulls if needed
	InstancedActors.SetNumZeroed(NewInstanceNum);
}


void 
UHoudiniInstancedActorComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

    // If our instances are parented to another actor we should duplicate them
    bool bNeedDuplicate = false;
    for (auto CurrentInstance : InstancedActors)
    {
        if ( !IsValid(CurrentInstance) )
            continue;

        if ( CurrentInstance->GetAttachParentActor() != GetOwner() )
            bNeedDuplicate = true;
    }

    if ( !bNeedDuplicate )
        return;

	// TODO: CHECK ME!
    // We need to duplicate our instances
    TArray<AActor*> SourceInstances = InstancedActors;
    InstancedActors.Empty();
    for (AActor* CurrentInstance : SourceInstances)
    {
        if ( !IsValid(CurrentInstance) )
            continue;

        FTransform InstanceTransform;
        if ( CurrentInstance->GetRootComponent() )
            InstanceTransform = CurrentInstance->GetRootComponent()->GetRelativeTransform();

       // AddInstance( InstanceTransform );
    }
}

#undef LOCTEXT_NAMESPACE