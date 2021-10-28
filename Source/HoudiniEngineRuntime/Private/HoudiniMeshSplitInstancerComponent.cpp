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

#include "HoudiniMeshSplitInstancerComponent.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniCompatibilityHelpers.h"

#include "UObject/DevObjectVersion.h"
#include "Serialization/CustomVersion.h"

#include "Components/StaticMeshComponent.h"

/*
#if WITH_EDITOR
	#include "ScopedTransaction.h"
	#include "LevelEditorViewport.h"
	#include "MeshPaintHelpers.h"
#endif
*/

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE  

UHoudiniMeshSplitInstancerComponent::UHoudiniMeshSplitInstancerComponent(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
	, InstancedMesh( nullptr )
{
}

void
UHoudiniMeshSplitInstancerComponent::Serialize(FArchive& Ar)
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
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniMeshSplitInstancerComponent : converting v1 object to v2."));

			Super::Serialize(Ar);

			UHoudiniMeshSplitInstancerComponent_V1* CompatibilityMSIC = NewObject<UHoudiniMeshSplitInstancerComponent_V1>();
			CompatibilityMSIC->Serialize(Ar);
			CompatibilityMSIC->UpdateFromLegacyData(this);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniMeshSplitInstancerComponent : serialization will be skipped."));

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

void
UHoudiniMeshSplitInstancerComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
    ClearInstances(0);
    Super::OnComponentDestroyed( bDestroyingHierarchy );
}


void 
UHoudiniMeshSplitInstancerComponent::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniMeshSplitInstancerComponent * ThisMSIC = Cast< UHoudiniMeshSplitInstancerComponent >(InThis);
    if ( IsValid(ThisMSIC) )
    {
        Collector.AddReferencedObject(ThisMSIC->InstancedMesh, ThisMSIC);
		for(auto& Mat : ThisMSIC->OverrideMaterials)
			Collector.AddReferencedObject(Mat, ThisMSIC);
        Collector.AddReferencedObjects(ThisMSIC->Instances, ThisMSIC);
    }
}

bool 
UHoudiniMeshSplitInstancerComponent::SetInstanceTransforms( 
    const TArray<FTransform>& InstanceTransforms)
{
	if (Instances.Num() <= 0 && InstanceTransforms.Num() <= 0)
		return false;

    if (!IsValid(GetOwner()))
        return false;

    // Destroy previous instances while keeping some of the one that we'll be able to reuse
    ClearInstances(InstanceTransforms.Num());

	//
    if( !IsValid(InstancedMesh) )
    {
        HOUDINI_LOG_ERROR(TEXT("%s: Null InstancedMesh for split instanced mesh override"), *GetOwner()->GetName());
        return false;
    }

    // Only create new SMC for newly added instances
    for (int32 iAdd = Instances.Num(); iAdd < InstanceTransforms.Num(); iAdd++)
    {
        const FTransform& InstanceTransform = InstanceTransforms[iAdd];
        UStaticMeshComponent* SMC = NewObject< UStaticMeshComponent >(
            GetOwner(), UStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);

        SMC->SetRelativeTransform(InstanceTransform);
        Instances.Add(SMC);
		GetOwner()->AddInstanceComponent(SMC);
    }

	// We should now have the same number of instances than transform
	ensure(InstanceTransforms.Num() == Instances.Num());	
	if (InstanceTransforms.Num() != Instances.Num())
		return false;

    for (int32 iIns = 0; iIns < Instances.Num(); ++iIns)
    {
        UStaticMeshComponent* SMC = Instances[iIns];
        const FTransform& InstanceTransform = InstanceTransforms[iIns];

        if (!IsValid(SMC))
            continue;

        SMC->SetRelativeTransform(InstanceTransform);

        // Attach created static mesh component to this thing
        SMC->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

        SMC->SetStaticMesh(InstancedMesh);
        SMC->SetVisibility(IsVisible());
        SMC->SetMobility(Mobility);

		// TODO: Revert to default if override is null??
		UMaterialInterface* MI = nullptr;
		if (OverrideMaterials.Num() > 0)
		{
			if (OverrideMaterials.IsValidIndex(iIns))
				MI = OverrideMaterials[iIns];
			else
				MI = OverrideMaterials[0];
		}		

		if (IsValid(MI))
        {
            int32 MeshMaterialCount = InstancedMesh->GetStaticMaterials().Num();
            for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
                SMC->SetMaterial(Idx, MI);
        }

        SMC->RegisterComponent();

		/*
		// TODO:
        // Properties not being propagated to newly created UStaticMeshComponents
        if (UHoudiniAssetComponent * pHoudiniAsset = Cast<UHoudiniAssetComponent>(GetAttachParent()))
        {
            pHoudiniAsset->CopyComponentPropertiesTo(SMC);
        }
		*/
    }

	return true;
}

void 
UHoudiniMeshSplitInstancerComponent::ClearInstances(int32 NumToKeep)
{
    if (NumToKeep <= 0)
    {
        for (auto&& Instance : Instances)
        {
            if (Instance)
            {
                Instance->ConditionalBeginDestroy();
            }
        }
        Instances.Empty();
    }
    else if (NumToKeep > 0 && NumToKeep < Instances.Num())
    {
        for (int32 i = NumToKeep; i < Instances.Num(); ++i)
        {
            UStaticMeshComponent * Instance = Instances[i];
            if (Instance)
            {
                Instance->ConditionalBeginDestroy();
            }
        }
        Instances.SetNum(NumToKeep);
    }
}

#undef LOCTEXT_NAMESPACE