/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*	 this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*	 promote products derived from this software without specific prior
*	 written permission.
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

#include "HoudiniMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniOutput.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniMaterialTranslator.h"
#include "HoudiniAssetActor.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniStaticMeshComponent.h"
#include "HoudiniSkeletalMeshTranslator.h"

#include "Engine/StaticMeshSocket.h"

#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "PackageTools.h"
#include "RawMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "MeshDescriptionOperations.h"

#include "BSPOps.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "GeometryToolsEngine.h"
#include "Engine/SkeletalMesh.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineString.h" 

#include "Components/SkeletalMeshComponent.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "EditorSupportDelegates.h"
#include "HoudiniGeometryCollectionTranslator.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if WITH_EDITOR
	#include "ConvexDecompTool.h"
	#include "Editor/UnrealEd/Private/GeomFitUtils.h"
	#include "LevelEditorViewport.h"
	#include "FileHelpers.h"
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

static TAutoConsoleVariable<float> CVarHoudiniEngineMeshBuildTimer(
	TEXT("HoudiniEngine.MeshBuildTimer"),
	0.0,
	TEXT("When enabled, the plugin will output timings during the Mesh creation.\n")
);

bool
FHoudiniMeshTranslator::CreateAllMeshesAndComponentsFromHoudiniOutput(
	UHoudiniOutput* InOutput, 
	const FHoudiniPackageParams& InPackageParams,
	EHoudiniStaticMeshMethod InStaticMeshMethod,
	bool bSplitMeshSupport,
	const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
	const FMeshBuildSettings& InMeshBuildSettings,
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
	UObject* InOuterComponent,
	bool bInTreatExistingMaterialsAsUpToDate,
	bool bInDestroyProxies)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateAllMeshesAndComponentsFromHoudiniOutput);

	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InPackageParams.OuterPackage))
		return false;

	if (!IsValid(InOuterComponent))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OldOutputObjects = InOutput->GetOutputObjects();
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignementMaterials = InOutput->GetAssignementMaterials();
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterials = InOutput->GetReplacementMaterials();

	bool InForceRebuild = false; 
	if (InOutput->HasAnyCurrentProxy() && InStaticMeshMethod != EHoudiniStaticMeshMethod::UHoudiniStaticMesh)
	{
		// Make sure we're not preventing refinement
		InForceRebuild = true;
	}

	// Iterate on all of the output's HGPO, creating meshes as we go
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->HoudiniGeoPartObjects)
	{
		// Not a mesh, skip
		if (CurHGPO.Type != EHoudiniPartType::Mesh)
			continue;

		// See if we have some uproperty attributes to update on 
		// the outer component (in most case, the HAC)
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			CurHGPO.GeoId, CurHGPO.PartId,
			true, 0, 0, 0,
			PropertyAttributes))
		{
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
				InOuterComponent, PropertyAttributes);
		}

		CreateStaticMeshFromHoudiniGeoPartObject(
			CurHGPO,
			InPackageParams,
			OldOutputObjects,
			NewOutputObjects,
			AssignementMaterials,
			ReplacementMaterials,
			InAllOutputMaterials,
			InOuterComponent,
			InForceRebuild,
			InStaticMeshMethod,
			bSplitMeshSupport,
			InSMGenerationProperties,
			InMeshBuildSettings,
			bInTreatExistingMaterialsAsUpToDate);
	}

	return FHoudiniMeshTranslator::CreateOrUpdateAllComponents(
		InOutput,
		InOuterComponent,
		NewOutputObjects,
		bInDestroyProxies);
}

bool
FHoudiniMeshTranslator::CreateOrUpdateAllComponents(
	UHoudiniOutput* InOutput,
	UObject* InOuterComponent,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InNewOutputObjects,
	bool bInDestroyProxies,
	bool bInApplyGenericProperties)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateOrUpdateAllComponents);

	if (!IsValid(InOutput))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OldOutputObjects = InOutput->GetOutputObjects();

	// Remove Static Meshes and their components from the old map 
	// to avoid their deletion if new proxies were created for them
	for (auto& NewOutputObj : InNewOutputObjects)
	{
		FHoudiniOutputObjectIdentifier OutputIdentifier = NewOutputObj.Key;

		// See if we already had that pair in the old map of static mesh
		FHoudiniOutputObject* FoundOldOutputObj = OldOutputObjects.Find(NewOutputObj.Key);
		if (!FoundOldOutputObj)
			continue;
		
		UObject* NewStaticMesh = NewOutputObj.Value.OutputObject;
		UObject* NewProxyMesh = NewOutputObj.Value.ProxyObject;

		UObject* OldStaticMesh = FoundOldOutputObj->OutputObject;
		if (IsValid(OldStaticMesh))
		{
			// If a proxy was created for an existing static mesh, keep the existing static
			// mesh (will be hidden)
			if (NewProxyMesh && NewOutputObj.Value.bProxyIsCurrent)
			{
				// Remove it from the old map to avoid its destruction
				OldOutputObjects.Remove(OutputIdentifier);
			}
			else if (NewStaticMesh && NewStaticMesh == OldStaticMesh)
			{
				// Remove it from the old map to avoid its destruction
				OldOutputObjects.Remove(OutputIdentifier);
			}
		}
		
		UObject* OldProxyMesh = FoundOldOutputObj->ProxyObject;
		if (IsValid(OldProxyMesh))
		{
			// If a new static mesh was created for a proxy, keep the proxy (will be hidden)
			// ... unless we want to explicitly destroy proxies
			if (NewStaticMesh && !bInDestroyProxies)
			{
				// Remove it from the old map to avoid its destruction
				OldOutputObjects.Remove(OutputIdentifier);
			}
			else if (NewProxyMesh && (NewProxyMesh == OldProxyMesh))
			{
				// Remove it from the old map to avoid its destruction
				OldOutputObjects.Remove(OutputIdentifier);
			}
		}
	}	

	// The old map now only contains unused/stale Meshes/Components, delete them
	for (auto& OldPair : OldOutputObjects)
	{
		// Get the old Identifier / StaticMesh
		FHoudiniOutputObjectIdentifier& OutputIdentifier = OldPair.Key;
		FHoudiniOutputObject& OldOutputObject = OldPair.Value;

		// Remove the old component from the map
		for(auto Component : OldOutputObject.OutputComponents)
		    RemoveAndDestroyComponent(Component);
		OldOutputObject.OutputComponents.Empty();

		// Remove the old proxy component from the map
		RemoveAndDestroyComponent(OldOutputObject.ProxyComponent);
		OldOutputObject.ProxyComponent = nullptr;

		if (IsValid(OldOutputObject.OutputObject))
		{
			OldOutputObject.OutputObject->MarkAsGarbage();
		}

		if (IsValid(OldOutputObject.ProxyObject))
		{
			OldOutputObject.ProxyObject->MarkAsGarbage();
		}		
	}
	OldOutputObjects.Empty();

	/*
	// Remove any stale components, these are components with OutputIdentifiers that are not 
	// in NewOutputObjects. This seems to happen mostly with the first or second cook after a
	// "Rebuild Asset"
	if (OutputComponents.Num() > 0 || OutputProxyComponents.Num() > 0)
	{
		TArray<TPair<FHoudiniOutputObjectIdentifier, UObject*>> StaleComponents;
		const uint32 MaxNumStale = FMath::Max(OutputComponents.Num(), OutputProxyComponents.Num());
		StaleComponents.Reserve(MaxNumStale);
		for (auto& ComponentPair : OutputComponents)
		{
			if (!NewOutputObjects.Contains(ComponentPair.Key) && !OldOutputObjectsReplacedByProxy.Contains(ComponentPair.Key))
			{
				StaleComponents.Add(ComponentPair);
			}
		}
		for (auto& ComponentPair : StaleComponents)
		{
			RemoveAndDestroyComponent(ComponentPair.Key, OutputComponents);
		}
		StaleComponents.Empty(MaxNumStale);

		for (auto& ComponentPair : OutputProxyComponents)
		{
			if (!NewOutputProxyObjects.Contains(ComponentPair.Key) && !OldOutputProxyObjectsReplacedByStaticMesh.Contains(ComponentPair.Key))
			{
				StaleComponents.Add(ComponentPair);
			}
		}
		for (auto& ComponentPair : StaleComponents)
		{
			RemoveAndDestroyComponent(ComponentPair.Key, OutputProxyComponents);
		}
		StaleComponents.Empty();
	}
	*/

	// Exit early if we have no component to update
	if (!IsValid(InOuterComponent))
	{
		// Assign the new output objects to the output
		InOutput->SetOutputObjects(InNewOutputObjects);

		return true;
	}

	// Now create/update the new static mesh components
	for (auto& NewPair : InNewOutputObjects)
	{
		// Get the old Identifier / StaticMesh
		const FHoudiniOutputObjectIdentifier& OutputIdentifier = NewPair.Key;
		FHoudiniOutputObject& OutputObject = NewPair.Value;

		if (OutputObject.bIsImplicit)
		{
			// This output is implicit and shouldn't have a representative component/proxy in the scene
			// Remove the old component from the map
			for(auto Component : OutputObject.OutputComponents)
			{
				RemoveAndDestroyComponent(Component);
			}
			OutputObject.OutputComponents.Empty();

			// Remove the old proxy component from the map
			RemoveAndDestroyComponent(OutputObject.ProxyComponent);
			OutputObject.ProxyComponent = nullptr;

			continue; // Skip any proxy / component creation below
		}

		// Check if we should create a Proxy/SMC
		if (OutputObject.bProxyIsCurrent)
		{
			UObject *Mesh = OutputObject.ProxyObject;
			if (!IsValid(Mesh) || !Mesh->IsA<UHoudiniStaticMesh>())
			{
				HOUDINI_LOG_ERROR(TEXT("Proxy Mesh is invalid (wrong type or pending kill)..."));
				continue;
			}

			// Create or update a new proxy component
			TSubclassOf<UMeshComponent> ComponentType = UHoudiniStaticMeshComponent::StaticClass();
			const FHoudiniGeoPartObject *FoundHGPO = nullptr;
			bool bCreated = false;
			UMeshComponent *MeshComponent = CreateOrUpdateMeshComponent(
				InOutput, 
				InOuterComponent, 
				OutputIdentifier, 
				ComponentType, 
				OutputObject, 
				FoundHGPO,
				bCreated);
			if (MeshComponent)
			{
				UHoudiniStaticMeshComponent *HSMC = Cast<UHoudiniStaticMeshComponent>(MeshComponent);
				UpdateMeshComponent(
					MeshComponent,
					Mesh,
					OutputIdentifier,
					OutputObject,
					FoundHGPO, 
					InOutput->HoudiniCreatedSocketActors, 
					InOutput->HoudiniAttachedSocketActors,
					bInApplyGenericProperties);

				if (!bCreated)
				{
					// For proxy meshes: notify that the mesh has been updated
					HSMC->NotifyMeshUpdated();
					HSMC->SetHoudiniIconVisible(true);
				}
			}

			// Now, ensure that meshes replaced by proxies are still kept but hidden
			for(auto Component : OutputObject.OutputComponents)
			{
			    USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
			    if (SceneComponent)
			    {
				    SceneComponent->SetVisibility(false);
				    SceneComponent->SetHiddenInGame(true);
			    }
			}

			// If the proxy mesh we just created is templated, hide it in game
			bool bIsTemplated = FoundHGPO ? FoundHGPO->bIsTemplated : false;
			if (bIsTemplated)
			{
				MeshComponent->SetHiddenInGame(true);
			}
		}
		else
		{
			// Create a new SMC if needed
			UObject* Mesh = OutputObject.OutputObject;
			if (!IsValid(Mesh))
			{
				HOUDINI_LOG_ERROR(TEXT("Mesh is invalid (wrong type or pending kill)..."));
				continue;
			}

			const FHoudiniGeoPartObject* FoundHGPO = nullptr;
			UMeshComponent* MeshComponent = nullptr;
			if (Mesh->IsA<UStaticMesh>())
			{
				TSubclassOf<UMeshComponent> ComponentType = UStaticMeshComponent::StaticClass();
				bool bCreated = false;
				MeshComponent = CreateOrUpdateMeshComponent(InOutput, InOuterComponent, OutputIdentifier, ComponentType, OutputObject, FoundHGPO, bCreated);
				if (MeshComponent)
				{
					UActorComponent* ProxyComponent = Cast<UActorComponent>(OutputObject.ProxyComponent);
					if (IsValid(ProxyComponent))
					{
						// If this static mesh component has a proxy component, it was likely refined from the proxy component.
						// This means that if the user enabled KeepTags, tags would have accumulated on the Proxy component so
						// we need to copy the proxy mesh component tags over to the static mesh.
						MeshComponent->ComponentTags = ProxyComponent->ComponentTags;
					}
					
					UpdateMeshComponent(
						MeshComponent,
						Mesh,
						OutputIdentifier,
						OutputObject,
						FoundHGPO,
						InOutput->HoudiniCreatedSocketActors,
						InOutput->HoudiniAttachedSocketActors,
						bInApplyGenericProperties);

					// UE5: Make sure we update/recreate the Component's render state
					// after the update or the mesh component will not be rendered!
					if (MeshComponent->IsRenderStateCreated())
					{
						// Need to send this to render thread at some point
						MeshComponent->MarkRenderStateDirty();
					}
					else if (MeshComponent->ShouldCreateRenderState())
					{
						// If we didn't have a valid StaticMesh assigned before
						// our render state might not have been created so
						// do it now.
						MeshComponent->RecreateRenderState_Concurrent();
					}
				}
			}
			else if (Mesh->IsA<USkeletalMesh>())
			{
				
				TSubclassOf<UMeshComponent> SKComponentType = USkeletalMeshComponent::StaticClass();
				bool bSKCreated = false;
				MeshComponent = CreateOrUpdateMeshComponent(InOutput, InOuterComponent, OutputIdentifier, SKComponentType, OutputObject, FoundHGPO, bSKCreated);
				if (MeshComponent)
				{
					UpdateMeshComponent(
						MeshComponent,
						Mesh,
						OutputIdentifier,
						OutputObject,
						FoundHGPO,
						InOutput->HoudiniCreatedSocketActors,
						InOutput->HoudiniAttachedSocketActors,
						bInApplyGenericProperties);

					USkeletalMeshComponent* SKMC = Cast<USkeletalMeshComponent>(MeshComponent);
					if (IsValid(SKMC))
					{
						USkeletalMesh* SKMesh = Cast<USkeletalMesh>(Mesh);
						if (IsValid(SKMesh))
						{
							SKMC->SetSkeletalMesh(SKMesh);
						}

						// TODO: we're currently unable to retrieve point position on the SK mesh/pose? 
						/*
						// Skeletal Mesh need to get their transform set separtely from the shape instancer's point transform
						//if (FoundHGPO != nullptr)
						for (auto& CurHGPO : InOutput->HoudiniGeoPartObjects)
						{
							// Retrieve Position
							HAPI_AttributeInfo PointInfo;
							FHoudiniApi::AttributeInfo_Init(&PointInfo);

							HAPI_Result PointInfoResult = FHoudiniApi::GetAttributeInfo(
								FHoudiniEngine::Get().GetSession(),
								CurHGPO.GeoId, CurHGPO.PartId,
								HAPI_UNREAL_ATTRIB_POSITION, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, &PointInfo);

							TArray<FVector3f> PositionData;
							PositionData.SetNum(PointInfo.count);  //dont need * PositionInfo.tupleSize, its already a vector container
							FHoudiniApi::GetAttributeFloatData(
								FHoudiniEngine::Get().GetSession(),
								CurHGPO.GeoId, CurHGPO.PartId, HAPI_UNREAL_ATTRIB_POSITION, &PointInfo, -1, (float*)&PositionData[0], 0, PointInfo.count);

							if (PositionData.Num() > 0)
							{
								SKMC->SetRelativeLocation(FVector3d(FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector3f(PositionData[0])));
							}

						}
						else
						{
							TArray<FTransform> Transforms;
							for (auto& CurHGPO : InOutput->HoudiniGeoPartObjects)
							{
								if (FHoudiniInstanceTranslator::HapiGetInstanceTransforms(CurHGPO, Transforms))
								{
									SKMC->SetRelativeTransform(Transforms[0]);
								}
							}
						}
						*/
					}
				}
			}

			// Now, ensure that proxies replaced by meshes are still kept but hidden
			UHoudiniStaticMeshComponent* HSMC = Cast<UHoudiniStaticMeshComponent>(OutputObject.ProxyComponent);
			if (HSMC)
			{
				HSMC->SetVisibility(false);
				HSMC->SetHiddenInGame(true);
				HSMC->SetHoudiniIconVisible(false);
			}

			// If the mesh we just created is templated, hide it in game
			bool bIsTemplated = FoundHGPO ? FoundHGPO->bIsTemplated : false;
			if (IsValid(MeshComponent) && bIsTemplated)
			{
				MeshComponent->SetHiddenInGame(true);
			}
		}
	}

	// Assign the new output objects to the output
	InOutput->SetOutputObjects(InNewOutputObjects);

	return true;
}

void
FHoudiniMeshTranslator::UpdateMeshComponent(
	UMeshComponent *InMeshComponent, 
	UObject* InMesh, 
	const FHoudiniOutputObjectIdentifier &InOutputIdentifier,
	const FHoudiniOutputObject& OutputObject,
	const FHoudiniGeoPartObject *InHGPO, 
	TArray<AActor*> &HoudiniCreatedSocketActors, 
	TArray<AActor*> &HoudiniAttachedSocketActors,
	bool bInApplyGenericProperties)
{
	UStaticMeshComponent* const SMC = Cast<UStaticMeshComponent>(InMeshComponent);
	UHoudiniStaticMeshComponent* const HSMC = Cast<UHoudiniStaticMeshComponent>(InMeshComponent);
	if (IsValid(SMC))
	{
		UpdateMeshOnStaticMeshComponent(SMC, InMesh);
	}
	else if (IsValid(HSMC))
	{
		UpdateMeshOnHoudiniStaticMeshComponent(HSMC, InMesh);
	}
	
	// Update collision/visibility
	EHoudiniSplitType SplitType = GetSplitTypeFromSplitName(InOutputIdentifier.SplitIdentifier);
	if (SplitType == EHoudiniSplitType::InvisibleComplexCollider || OutputObject.bIsInvisibleCollisionMesh)
	{
		// Invisible complex collider should not be seen
		InMeshComponent->SetVisibility(false);
		InMeshComponent->SetHiddenInGame(true);
		InMeshComponent->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
		InMeshComponent->SetCastShadow(false);
	}
	else
	{
		// Update visiblity
		bool bVisible = InHGPO ? InHGPO->bIsVisible : true;
		InMeshComponent->SetVisibility(bVisible);
		InMeshComponent->SetHiddenInGame(!bVisible);
		
		FPropertyChangedEvent Evt(FindFieldChecked<FProperty>(InMeshComponent->GetClass(), "bVisible"));
		InMeshComponent->PostEditChangeProperty(Evt);
	}

	// TODO:
	// Update navmesh?

	// Transform the component by transformation provided by HAPI.
	InMeshComponent->SetRelativeTransform(InHGPO ? InHGPO->TransformMatrix : FTransform::Identity);

	// If the static mesh had sockets, we can assign the desired actor to them now
	UStaticMeshComponent * StaticMeshComponent = Cast<UStaticMeshComponent>(InMeshComponent);
	UStaticMesh * StaticMesh = nullptr;
	if (IsValid(StaticMeshComponent))
		StaticMesh = StaticMeshComponent->GetStaticMesh();

	if (IsValid(StaticMesh)) 
	{
		int32 NumberOfSockets = StaticMesh == nullptr ? 0 : StaticMesh->Sockets.Num();
		for (int32 nSocket = 0; nSocket < NumberOfSockets; nSocket++)
		{
			UStaticMeshSocket* MeshSocket = StaticMesh->Sockets[nSocket];
			if (IsValid(MeshSocket) && (MeshSocket->Tag.IsEmpty()))
				continue;

			AddActorsToMeshSocket(StaticMesh->Sockets[nSocket], StaticMeshComponent, HoudiniCreatedSocketActors, HoudiniAttachedSocketActors);
		}

		// Iterate all remaining created socket actors, destroy the ones that are not assigned to socket after re-cook
		{
			for (int32 Idx = HoudiniCreatedSocketActors.Num() - 1; Idx >= 0; --Idx) 
			{
				AActor * CurActor = HoudiniCreatedSocketActors[Idx];

				if (!IsValid(CurActor))
				{
					HoudiniCreatedSocketActors.RemoveAt(Idx);
					continue;
				}

				bool bFoundSocket = false;
				for (auto & CurSocket : StaticMesh->Sockets)
				{
					if (CurSocket->SocketName == CurActor->GetAttachParentSocketName())
					{
						bFoundSocket = true;
						break;
					}
				}
				// cur actor's attaching socket is found, skip
				if (bFoundSocket)
					continue;

				// Destroy the previous created socket actor if not found
				HoudiniCreatedSocketActors.RemoveAt(Idx);
				CurActor->Destroy();
			}
		}

		// Detach the in level actors which is not attached to any socket now
		{
			for (int32 Idx = HoudiniAttachedSocketActors.Num() - 1; Idx >= 0; --Idx) 
			{
				AActor* CurActor = HoudiniAttachedSocketActors[Idx];
				if (!IsValid(CurActor)) 
				{
					HoudiniAttachedSocketActors.RemoveAt(Idx);
					continue;
				}

				bool bFoundSocket = false;
				for (auto & CurSocket : StaticMesh->Sockets)
				{
					if (CurSocket->SocketName == CurActor->GetAttachParentSocketName())
					{
						bFoundSocket = true;
						break;
					}
				}

				if (bFoundSocket)
					continue;

				// If the attached socket name is not found in current socket, detach it and remove from the array
				CurActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
				HoudiniAttachedSocketActors.RemoveAt(Idx);			
			}
		}

	}

	if (bInApplyGenericProperties)
	{
		// Clear the component tags, if permitted by HGPOs
		FHoudiniEngineUtils::KeepOrClearComponentTags(InMeshComponent, InHGPO);
		
		// Update the property attributes on the component
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			InOutputIdentifier.GeoId, InOutputIdentifier.PartId,
			true,
			InOutputIdentifier.PrimitiveIndex,
			INDEX_NONE,
			InOutputIdentifier.PointIndex,
			PropertyAttributes))
		{
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(InMeshComponent, PropertyAttributes);
		}
	}
}

bool
FHoudiniMeshTranslator::CreateStaticMeshFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignmentMaterialMap,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterialMap,
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
	UObject* const InOuterComponent,
	const bool& InForceRebuild,
	EHoudiniStaticMeshMethod InStaticMeshMethod,
	bool bSplitMeshSupport,
	const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
	const FMeshBuildSettings& InSMBuildSettings,
	bool bInTreatExistingMaterialsAsUpToDate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateStaticMeshFromHoudiniGeoPartObject);

	// If we're not forcing the rebuild
	// No need to recreate something that hasn't changed
	if (!InForceRebuild && !InHGPO.bHasGeoChanged && !InHGPO.bHasPartChanged && InOutputObjects.Num() > 0)
	{
		// Simply reuse the existing meshes
		OutOutputObjects = InOutputObjects;
		return true;
	}

	// Create a new mesh translator to handle the output data creation
	FHoudiniMeshTranslator CurrentTranslator;
	CurrentTranslator.ForceRebuild = InForceRebuild;
	CurrentTranslator.SetHoudiniGeoPartObject(InHGPO);
	CurrentTranslator.SetInputObjects(InOutputObjects);
	CurrentTranslator.SetOutputObjects(OutOutputObjects);
	CurrentTranslator.SetInputAssignmentMaterials(AssignmentMaterialMap);
	CurrentTranslator.SetAllOutputMaterials(InAllOutputMaterials);
	CurrentTranslator.SetReplacementMaterials(ReplacementMaterialMap);
	CurrentTranslator.SetPackageParams(InPackageParams, true);
	CurrentTranslator.SetTreatExistingMaterialsAsUpToDate(bInTreatExistingMaterialsAsUpToDate);
	CurrentTranslator.SetStaticMeshGenerationProperties(InSMGenerationProperties);
	CurrentTranslator.SetStaticMeshBuildSettings(InSMBuildSettings);
	CurrentTranslator.SetOuterComponent(InOuterComponent);

	// TODO: Fetch from settings/HAC
	CurrentTranslator.DefaultMeshSmoothing = 1;
	if (false)
		CurrentTranslator.DefaultMeshSmoothing = 0;

	// Create the Static Mesh with the desired method
	switch (InStaticMeshMethod)
	{
		case EHoudiniStaticMeshMethod::FMeshDescription:
			if (bSplitMeshSupport)
				CurrentTranslator.CreateStaticMeshesFromSplitGroups();
			else
				CurrentTranslator.CreateStaticMesh_MeshDescription();
			break;

		case EHoudiniStaticMeshMethod::UHoudiniStaticMesh:
			if (bSplitMeshSupport)
				CurrentTranslator.CreateHoudiniStaticMeshesFromSplitGroups();
			else
				CurrentTranslator.CreateHoudiniStaticMesh();

			break;
	}

	// Copy the output objects/materials
	OutOutputObjects = CurrentTranslator.OutputObjects;
	AssignmentMaterialMap = CurrentTranslator.OutputAssignmentMaterials;

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartVertexList()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartVertexList);

	if (HGPO.PartInfo.VertexCount <= 0)
		return false;

	// Get the vertex List
	PartVertexList.SetNumUninitialized(HGPO.PartInfo.VertexCount);

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVertexList(
		FHoudiniEngine::Get().GetSession(),
		HGPO.GeoId, HGPO.PartId, &PartVertexList[0], 0, HGPO.PartInfo.VertexCount))
	{
		// Error getting the vertex list.
		HOUDINI_LOG_MESSAGE(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list - skipping."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);

		return false;
	}

	return true;
}

void
FHoudiniMeshTranslator::SortSplitGroups()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::SortSplitGroups);

	// Sort the splits in the order that we want to process them:
	// Simple/Convex invisible colliders should be treated first as they will need to be attached to the visible meshes
	TArray<FString> First;
	
	// The main geo and its LODs should be created after.
	TArray<FString> Main;
	TArray<FString> LODs;

	// Finally, visible colliders and invisible complex colliders as they need their own static mesh
	TArray<FString> Last;

	for (auto& curSplit : HGPO.SplitGroups)
	{
		EHoudiniSplitType curSplitType = GetSplitTypeFromSplitName(curSplit);
		switch (curSplitType)
		{
			case EHoudiniSplitType::InvisibleSimpleCollider:
			case EHoudiniSplitType::InvisibleUCXCollider:
				First.Add(curSplit);
				break;

			case EHoudiniSplitType::Normal:
				Main.Add(curSplit);
				break;

			case EHoudiniSplitType::LOD:
				LODs.Add(curSplit);
				break;

			case EHoudiniSplitType::RenderedSimpleCollider:
			case EHoudiniSplitType::RenderedUCXCollider:
			case EHoudiniSplitType::RenderedComplexCollider:
			case EHoudiniSplitType::InvisibleComplexCollider:
				Last.Add(curSplit);
				break;
		}
	}

	// Make sure LODs are order by name
	LODs.Sort();

	// Copy the split names in order
	AllSplitGroups.Empty();
	for (auto& splitName : First)
		AllSplitGroups.Add(splitName);

	for (auto& splitName : Main)
		AllSplitGroups.Add(splitName);

	for (auto& splitName : LODs)
		AllSplitGroups.Add(splitName);

	for (auto& splitName : Last)
		AllSplitGroups.Add(splitName);
}

bool
FHoudiniMeshTranslator::UpdateSplitsFacesAndIndices()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdateSplitsFacesAndIndices);

	// Reset the splits faces/indices arrays
	AllSplitVertexLists.Empty();
	AllSplitVertexCounts.Empty();
	AllSplitFaceIndices.Empty();
	AllSplitFirstValidVertexIndex.Empty();
	AllSplitFirstValidPrimIndex.Empty();

	bool bHasSplit = AllSplitGroups.Num() > 0;
	if (bHasSplit)
	{
		HAPI_PartInfo PartInfo = FHoudiniEngineUtils::ToHAPIPartInfo(HGPO.PartInfo);

		// Buffer for all vertex indices used for split groups.
		// We need this to figure out all vertex indices that are not part of them. 
		TArray<int32> PartUsedVertices;
		PartUsedVertices.SetNumZeroed(PartVertexList.Num());

		// Buffer for all face indices used for split groups.
		// We need this to figure out all face indices that are not part of them.
		TArray<int32> AllGroupFaceIndices;
		AllGroupFaceIndices.SetNumZeroed(HGPO.PartInfo.FaceCount);

		// Some of the groups may contain invalid geometry 
		// Store them here so we can remove them afterwards
		TArray<int32> InvalidGroupNameIndices;

		// Extract the vertices/faces for each of the split groups
		for (int32 SplitIdx = 0; SplitIdx < AllSplitGroups.Num(); SplitIdx++)
		{
			const FString& GroupName = AllSplitGroups[SplitIdx];

			// New vertex list just for this group.
			TArray< int32 > GroupVertexList;
			TArray< int32 > AllFaceList;
			
			int32 FirstValidPrimIndex = 0;
			int32 FirstValidVertexIndex = 0;
			// Extract vertex indices for this split.
			int32 GroupVertexListCount = FHoudiniEngineUtils::HapiGetVertexListForGroup(
				HGPO.GeoId, PartInfo, GroupName,
				PartVertexList, GroupVertexList,
				PartUsedVertices, AllFaceList, AllGroupFaceIndices,
				FirstValidVertexIndex, FirstValidPrimIndex,
				HGPO.PartInfo.bIsInstanced);

			if (GroupVertexListCount <= 0)
			{
				// This group doesn't have vertices/faces, mark it as invalid
				InvalidGroupNameIndices.Add(SplitIdx);

				// Error getting the vertex list.
				HOUDINI_LOG_MESSAGE(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list for group %s - skipping."),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, *GroupName);

				continue;
			}

			// If list is not empty, we store it for this group - this will define new mesh.
			AllSplitVertexLists.Add(GroupName, GroupVertexList);
			AllSplitVertexCounts.Add(GroupName, GroupVertexListCount);
			AllSplitFaceIndices.Add(GroupName, AllFaceList);
			AllSplitFirstValidVertexIndex.Add(GroupName, FirstValidVertexIndex);
			AllSplitFirstValidPrimIndex.Add(GroupName, FirstValidPrimIndex);
		}

		if (InvalidGroupNameIndices.Num() > 0)
		{
			// Remove all invalid split groups
			for (int32 InvalIdx = InvalidGroupNameIndices.Num() - 1; InvalIdx >= 0; InvalIdx--)
			{
				int32 Index = InvalidGroupNameIndices[InvalIdx];
				AllSplitGroups.RemoveAt(Index);
			}
		}

		// We also need to figure out / construct the vertex list for everything that's not in a split group
		TArray<int32> GroupSplitFacesRemaining;
		GroupSplitFacesRemaining.SetNumUninitialized(PartVertexList.Num());
		for (int32 n = 0; n < GroupSplitFacesRemaining.Num(); n++)
			GroupSplitFacesRemaining[n] = -1;

		int32 GroupVertexListCount = 0;
		bool bHasMainSplitGroup = false;
		TArray< int32 > GroupSplitFaceIndicesRemaining;
		int32 FistUnusedVertexIndex = -1;		
		for (int32 SplitVertexIdx = 0; SplitVertexIdx < PartUsedVertices.Num(); SplitVertexIdx++)
		{
			if (PartUsedVertices[SplitVertexIdx] == 0)
			{
				// This is an unused index, we need to add it to unused vertex list.
				FistUnusedVertexIndex = SplitVertexIdx;
				GroupSplitFacesRemaining[SplitVertexIdx] = PartVertexList[SplitVertexIdx];
				bHasMainSplitGroup = true;
				GroupVertexListCount++;
			}
		}

		int32 FistUnusedPrimIndex = -1;
		for (int32 SplitFaceIdx = 0; SplitFaceIdx < AllGroupFaceIndices.Num(); SplitFaceIdx++)
		{
			if (AllGroupFaceIndices[SplitFaceIdx] == 0)
			{
				// This is unused face, we need to add it to unused faces list.
				GroupSplitFaceIndicesRemaining.Add(SplitFaceIdx);
				FistUnusedPrimIndex = SplitFaceIdx;
			}
		}

		// We store the remaining geo vertex list as a special split named "main geo"
		// and make sure its treated before the collider meshes
		if (bHasMainSplitGroup)
		{
			static const FString RemainingGroupName = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
			AllSplitGroups.Add(RemainingGroupName);
			AllSplitVertexLists.Add(RemainingGroupName, GroupSplitFacesRemaining);
			AllSplitVertexCounts.Add(RemainingGroupName, GroupVertexListCount);
			AllSplitFaceIndices.Add(RemainingGroupName, GroupSplitFaceIndicesRemaining);
			AllSplitFirstValidPrimIndex.Add(RemainingGroupName, FistUnusedPrimIndex);
			AllSplitFirstValidVertexIndex.Add(RemainingGroupName, FistUnusedVertexIndex);
		}
	}
	else
	{
		// No splitting required
		// Mark everything as the main geo group
		static const FString RemainingGroupName = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
		AllSplitGroups.Add(RemainingGroupName);
		AllSplitVertexLists.Add(RemainingGroupName, PartVertexList);
		AllSplitVertexCounts.Add(RemainingGroupName, PartVertexList.Num());
		AllSplitFirstValidPrimIndex.Add(RemainingGroupName, 0);
		AllSplitFirstValidVertexIndex.Add(RemainingGroupName, 0);

		TArray<int32> AllFaces;
		for (int32 FaceIdx = 0; FaceIdx < HGPO.PartInfo.FaceCount; ++FaceIdx)
			AllFaces.Add(FaceIdx);

		AllSplitFaceIndices.Add(RemainingGroupName, AllFaces);
	}

	return true;
}

void
FHoudiniMeshTranslator::ResetPartCache()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::ResetPartCache);

	// Vertex Positions
	PartPositions.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

	// Vertex Normals
	PartNormals.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNormals);

	// Vertex TangentU
	PartTangentU.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTangentU);

	// Vertex TangentV
	PartTangentV.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTangentV);

	// Vertex Colors
	PartColors.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoColors);

	// Vertex Alpha values
	PartAlphas.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoAlpha);

	// FaceSmoothing values
	PartFaceSmoothingMasks.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoFaceSmoothingMasks);

	// UVs
	PartUVSets.Empty();
	AttribInfoUVSets.Empty();

	// UVs
	PartLightMapResolutions.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoLightmapResolution);

	// Material IDs per face
	PartFaceMaterialIds.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribInfoFaceMaterialIds);
	// Unique material IDs
	PartUniqueMaterialIds.Empty();
	// Material infos for each unique Material
	PartUniqueMaterialInfos.Empty();
	//
	bOnlyOneFaceMaterial = false;

	// Face Materials override
	PartFaceMaterialOverrides.Empty();
	bHaveMaterialOverrides = false;
	bHavePrimMaterialOverrides = false;
	bMaterialOverrideNeedsCreateInstance = false;

	// LOD Screensize
	PartLODScreensize.Empty();
}

bool
FHoudiniMeshTranslator::UpdatePartPositionIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartPositionIfNeeded);

	// Only Retrieve the vertices positions if necessary
	if (PartPositions.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_POSITION);
	Accessor.GetInfo(AttribInfoPositions);

	if (!Accessor.GetAttributeData(AttribInfoPositions, PartPositions))
	{
		// Error retrieving positions.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve position data")
			TEXT("- skipping."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}
	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartNormalsIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartNormalsIfNeeded);

	// No need to read the normals if we want unreal to recompute them after
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bReadNormals = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->RecomputeNormalsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always : true;
	if (!bReadNormals)
		return true;

	// Only Retrieve the normals if we haven't already
	if (PartNormals.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_NORMAL);
	Accessor.GetInfo(AttribInfoNormals);

	// Retrieve normal data for this part
	bool Success = Accessor.GetAttributeData(AttribInfoNormals, PartNormals);

	// There is no normals to fetch
	if (!AttribInfoNormals.exists)
		return true;

	if	(!Success && AttribInfoNormals.exists)
	{
		// Error retrieving normals.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve normal data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartTangentsIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartTangentsIfNeeded);

	bool bReturn = true;
	if (PartTangentU.Num() <= 0)
	{
		// Retrieve TangentU data for this part
		FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_TANGENTU);
		Accessor.GetInfo(AttribInfoTangentU);
		bool Success = Accessor.GetAttributeData(AttribInfoTangentU, PartTangentU);
		
		if (!Success && AttribInfoTangentU.exists)
		{
			// Error retrieving tangent.
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve tangentU data"),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
			bReturn = false;
		}
	}

	if (PartTangentV.Num() <= 0)
	{
		FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_TANGENTV);
		Accessor.GetInfo(AttribInfoTangentV);
		bool Success = Accessor.GetAttributeData(AttribInfoTangentV, PartTangentV);

		if (!Success && AttribInfoTangentV.exists)
		{
			// Error retrieving tangent.
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve tangentV data"),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
			bReturn = false;
		}
	}

	return bReturn;
}

bool
FHoudiniMeshTranslator::UpdatePartColorsIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartColorsIfNeeded);

	// Only Retrieve the vertices colors if necessary
	if (PartColors.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_COLOR);
	Accessor.GetInfo(AttribInfoColors);
	bool Success = Accessor.GetAttributeData(AttribInfoColors, PartColors);

	if (!Success && AttribInfoColors.exists)
	{
		// Error retrieving colors.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve color data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartAlphasIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartAlphasIfNeeded);

	// Only Retrieve the vertices alphas if necessary
	if (PartAlphas.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_ALPHA);
	Accessor.GetInfo(AttribInfoAlpha);
	bool Success = Accessor.GetAttributeData(AttribInfoAlpha, PartAlphas);

	if (!Success && AttribInfoAlpha.exists)
	{
		// Error retrieving alpha values.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve alpha data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartFaceSmoothingIfNeeded()
{
	// Only Retrieve the vertices FaceSmoothing if necessary
	if (PartFaceSmoothingMasks.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK);
	Accessor.GetInfo(AttribInfoFaceSmoothingMasks);
	bool Success = Accessor.GetAttributeData(AttribInfoFaceSmoothingMasks, PartFaceSmoothingMasks);

	if (!Success && AttribInfoFaceSmoothingMasks.exists)
	{
		// Error retrieving FaceSmoothing values.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve FaceSmoothing data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartUVSetsIfNeeded(const bool& bRemoveUnused)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartUVSetsIfNeeded);

	FHoudiniEngineUtils::UpdateMeshPartUVSets(HGPO.GeoId, HGPO.PartId, bRemoveUnused, PartUVSets, AttribInfoUVSets);

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartLightmapResolutionsIfNeeded()
{
	// Only Retrieve the vertices light map resolution if necessary
	if (PartLightMapResolutions.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION);
	Accessor.GetInfo(AttribInfoLightmapResolution);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, PartLightMapResolutions);

	if (!bSuccess && AttribInfoLightmapResolution.exists)
	{
		// Error retrieving lightmap resolution values.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve lightmap resolution data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartFaceMaterialIDsIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartFaceMaterialIDsIfNeeded);

	// Only Retrieve the material IDs if necessary
	if (PartFaceMaterialIds.Num() > 0)
		return true;

	int32 NumFaces = HGPO.PartInfo.FaceCount;
	if (NumFaces <= 0)
		return true;

	PartFaceMaterialIds.SetNum(NumFaces);

	// Get the materials IDs per face
	HAPI_Bool bSingleFaceMaterial = false;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialNodeIdsOnFaces(
		FHoudiniEngine::Get().GetSession(),
		HGPO.GeoId, HGPO.PartId, &bSingleFaceMaterial,
		&PartFaceMaterialIds[0], 0, NumFaces))
	{
		// Error retrieving material face assignments.
		HOUDINI_LOG_MESSAGE(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return false;
	}

	bOnlyOneFaceMaterial = bSingleFaceMaterial;

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartFaceMaterialOverridesIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartFaceMaterialOverridesIfNeeded);

	// Only Retrieve the material overrides if necessary
	if (PartFaceMaterialOverrides.Num() > 0)
		return true;

	bHaveMaterialOverrides = false;
	bHavePrimMaterialOverrides = false;
	bMaterialOverrideNeedsCreateInstance = false;

	TArray<FString> MaterialOverrides;
	TArray<FString> MaterialInstanceOverrides;
	HAPI_AttributeInfo AttribInfoFaceMaterialOverrides;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoFaceMaterialOverrides);

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_MATERIAL);
	Accessor.GetInfo(AttribInfoFaceMaterialOverrides, HAPI_ATTROWNER_INVALID);
	Accessor.GetAttributeData(AttribInfoFaceMaterialOverrides, MaterialOverrides);

	bool bMaterialAttributeExists = AttribInfoFaceMaterialOverrides.exists;
	HAPI_AttributeOwner MaterialAttrOwner = bMaterialAttributeExists ? AttribInfoFaceMaterialOverrides.owner : HAPI_ATTROWNER_INVALID;
	if (bMaterialAttributeExists && MaterialAttrOwner != HAPI_ATTROWNER_DETAIL && MaterialAttrOwner != HAPI_ATTROWNER_PRIM)
	{
		HOUDINI_LOG_WARNING(TEXT("Static Mesh [%d %s], Geo [%d], Part [%d %s]: " HAPI_UNREAL_ATTRIB_MATERIAL " must be a primitive or detail attribute, ignoring attribute."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		MaterialOverrides.Empty();
		bMaterialAttributeExists = false;
	}

	// If material attribute and fallbacks were not found, check the material instance attribute.
	Accessor.Init(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE);
	Accessor.GetInfo(AttribInfoFaceMaterialOverrides, HAPI_ATTROWNER_INVALID);
	Accessor.GetAttributeData(AttribInfoFaceMaterialOverrides, MaterialInstanceOverrides);

	bool bMaterialInstanceAttributeExists = AttribInfoFaceMaterialOverrides.exists;
	const HAPI_AttributeOwner MaterialInstanceAttrOwner = bMaterialInstanceAttributeExists ? AttribInfoFaceMaterialOverrides.owner : HAPI_ATTROWNER_INVALID;
	if (bMaterialInstanceAttributeExists && MaterialInstanceAttrOwner != HAPI_ATTROWNER_DETAIL && MaterialInstanceAttrOwner != HAPI_ATTROWNER_PRIM)
	{
		HOUDINI_LOG_WARNING(TEXT("Static Mesh [%d %s], Geo [%d], Part [%d %s]: " HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE " must be a primitive or detail attribute, ignoring attribute."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		MaterialInstanceOverrides.Empty();
		bMaterialInstanceAttributeExists = false;
	}

	// If material attribute was not found, check fallback compatibility attribute.
	if ((!bMaterialAttributeExists && !bMaterialInstanceAttributeExists) || (MaterialOverrides.Num() == 0 && MaterialInstanceOverrides.Num() == 0))
	{
		PartFaceMaterialOverrides.Empty();

		Accessor.Init(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_MATERIAL_FALLBACK);
		Accessor.GetInfo(AttribInfoFaceMaterialOverrides, HAPI_ATTROWNER_INVALID);
		Accessor.GetAttributeData(AttribInfoFaceMaterialOverrides, MaterialOverrides);

		bMaterialAttributeExists = AttribInfoFaceMaterialOverrides.exists;
		MaterialAttrOwner = bMaterialAttributeExists ? AttribInfoFaceMaterialOverrides.owner : HAPI_ATTROWNER_INVALID;
		if (bMaterialAttributeExists && MaterialAttrOwner != HAPI_ATTROWNER_DETAIL && MaterialAttrOwner != HAPI_ATTROWNER_PRIM)
		{
			HOUDINI_LOG_WARNING(TEXT("Static Mesh [%d %s], Geo [%d], Part [%d %s]: " HAPI_UNREAL_ATTRIB_MATERIAL_FALLBACK " must be a primitive or detail attribute, ignoring attribute."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
			MaterialOverrides.Empty();
			bMaterialAttributeExists = false;
		}
	}

	// If no override attributes exist then there is nothing more to do
	if (!bMaterialAttributeExists && !bMaterialInstanceAttributeExists)
		return false;

	if ((!bMaterialAttributeExists || MaterialAttrOwner == HAPI_ATTROWNER_DETAIL) && (!bMaterialInstanceAttributeExists || MaterialInstanceAttrOwner == HAPI_ATTROWNER_DETAIL))
	{
		// either only one attribute exists and is a detail attribute, or both exist and are detail attributes
		bHavePrimMaterialOverrides = false;
		FHoudiniMaterialInfo MatInfo;
		if (MaterialOverrides.IsValidIndex(0) && !MaterialOverrides[0].IsEmpty())
		{
			MatInfo.MaterialObjectPath = MaterialOverrides[0];
			ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
		}
		else if (MaterialInstanceOverrides.IsValidIndex(0) && !MaterialInstanceOverrides[0].IsEmpty())
		{
			MatInfo.bMakeMaterialInstance = true;
			bMaterialOverrideNeedsCreateInstance = true;
			MatInfo.MaterialObjectPath = MaterialInstanceOverrides[0];
			ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
		}
		else
		{
			MatInfo.MaterialObjectPath = FString();
		}
		// PartFaceMaterialOverrides must have an entry for each face, or be empty
		if (!MatInfo.MaterialObjectPath.IsEmpty())
			PartFaceMaterialOverrides.Init(MatInfo, HGPO.PartInfo.FaceCount);
		else
			PartFaceMaterialOverrides.Empty();
	}
	else
	{
		// Cases to handle here: both exist and are prim, or one is prim and one detail, or only one exists and is prim
		bHavePrimMaterialOverrides = true;
		// PartFaceMaterialOverrides must have an entry for each face, or be empty
		PartFaceMaterialOverrides.Reset(HGPO.PartInfo.FaceCount);
		for (int32 Index = 0; Index < HGPO.PartInfo.FaceCount; ++Index)
		{
			FHoudiniMaterialInfo& MatInfo = PartFaceMaterialOverrides.AddDefaulted_GetRef();

			// Determine the potential indexes: Index for primitive attributes and 0 for detail attribute
			int32 MaterialOverridesIndex = INDEX_NONE;
			int32 MaterialInstanceOverridesIndex = INDEX_NONE;
			if (MaterialAttrOwner == MaterialInstanceAttrOwner)
			{
				// Both attributes are prim
				MaterialOverridesIndex = Index;
				MaterialInstanceOverridesIndex = Index;
			}
			else if (MaterialAttrOwner == HAPI_ATTROWNER_PRIM)
			{
				// MaterialOverrides is prim, MaterialInstanceOverrides is detail or does not exist
				MaterialOverridesIndex = Index;
				MaterialInstanceOverridesIndex = 0;
			}
			else
			{
				// MaterialOverrides is detail, MaterialInstanceOverrides is prim (one of the two might not exist)
				MaterialOverridesIndex = 0;
				MaterialInstanceOverridesIndex = Index;
			}

			// MaterialOverrides (unreal_material) takes precedence, if non-empty, over MaterialInstanceOverrides (unreal_material_instance)
			if (MaterialOverrides.IsValidIndex(MaterialOverridesIndex) && !MaterialOverrides[MaterialOverridesIndex].IsEmpty())
			{
				MatInfo.MaterialObjectPath = MaterialOverrides[MaterialOverridesIndex];
				ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
			}
			else if (MaterialInstanceOverrides.IsValidIndex(MaterialInstanceOverridesIndex) && !MaterialInstanceOverrides[MaterialInstanceOverridesIndex].IsEmpty())
			{
				MatInfo.bMakeMaterialInstance = true;
				bMaterialOverrideNeedsCreateInstance = true;
				MatInfo.MaterialObjectPath = MaterialInstanceOverrides[MaterialInstanceOverridesIndex];
				ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
			}
			else
			{
				MatInfo.MaterialObjectPath = FString();
			}
		}
	}

	bHaveMaterialOverrides = PartFaceMaterialOverrides.Num() > 0;
	if (bHaveMaterialOverrides)
		FHoudiniMaterialTranslator::GetMaterialParameters(PartFaceMaterialOverrides, HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_PRIM);

	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartNeededMaterials()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartNeededMaterials);

	// Update the per face material IDs
	UpdatePartFaceMaterialIDsIfNeeded();

	// See if we have some material overides
	UpdatePartFaceMaterialOverridesIfNeeded();

	// If we have houdini materials AND overrides:
	// We want to only create the Houdini materials that are not "covered" by overrides
	// If we have material instance attributes, create all the houdini material anyway
	// as their textures could be referenced by the material instance parameters
	if (PartFaceMaterialOverrides.Num() > 0 && !bMaterialOverrideNeedsCreateInstance)
	{
		// If the material override was set on the detail, no need to look for houdini material IDs, as only the override will be used
		if (bHaveMaterialOverrides && bHavePrimMaterialOverrides)
		{
			for (int32 MaterialIdx = 0; MaterialIdx < PartFaceMaterialIds.Num(); ++MaterialIdx)
			{
				// Add a material ID to the unique array only if that face is not using the override
				if (PartFaceMaterialOverrides[MaterialIdx].MaterialObjectPath.IsEmpty())
					PartUniqueMaterialIds.AddUnique(PartFaceMaterialIds[MaterialIdx]);
			}
		}
	}
	else
	{
		// No material overrides, simply update the unique material array
		for (int32 MaterialIdx = 0; MaterialIdx < PartFaceMaterialIds.Num(); ++MaterialIdx)
			PartUniqueMaterialIds.AddUnique(PartFaceMaterialIds[MaterialIdx]);
	}

	// Remove the invalid material ID from the unique array
	PartUniqueMaterialIds.RemoveSingle(-1);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartNeededMaterials - Get the unique material infos);
		// Get the unique material infos
		PartUniqueMaterialInfos.SetNum(PartUniqueMaterialIds.Num());
		for (int32 MaterialIdx = 0; MaterialIdx < PartUniqueMaterialIds.Num(); MaterialIdx++)
		{

			FHoudiniApi::MaterialInfo_Init(&PartUniqueMaterialInfos[MaterialIdx]);
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialInfo(
				FHoudiniEngine::Get().GetSession(),
				PartUniqueMaterialIds[MaterialIdx],
				&PartUniqueMaterialInfos[MaterialIdx]))
			{
				// Error retrieving material face assignments.
				HOUDINI_LOG_MESSAGE(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material info for material %d"),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, PartUniqueMaterialIds[MaterialIdx]);
				continue;
			}
		}
	}
	return true;
}

bool
FHoudiniMeshTranslator::UpdatePartLODScreensizeIfNeeded()
{
	// Only retrieve LOD screensizes if necessary
	if (PartLODScreensize.Num() > 0)
		return true;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE);
	bool Success = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, PartLODScreensize);
	return Success;
}


void
FHoudiniMeshTranslator::UpdateStaticMeshNaniteSettings(const int32& GeoId, const int32& PartId, const int32& PrimIndex, UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
		return;

	// 
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	// Start by looking for the nanite enabled attribute, disabled by default
	bool bEnableNanite = false;
	TArray<int32> IntData;

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_ENABLED);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, IntData, PrimIndex, 1))
	{
		Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, IntData, 0, 1);
	}
	if (IntData.Num() > 0)
	{
		bEnableNanite = (IntData[0] != 0);
	}

	// Then look for the position precision attribute, auto by default (MIN_int32)
	IntData.Empty();
	StaticMesh->NaniteSettings.PositionPrecision = MIN_int32;

	// Look for a specific prim attribute first
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_POSITION_PRECISION);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, IntData, PrimIndex, 1))
	{
		Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, IntData, 0, 1);
	}

	if (IntData.Num() > 0)
	{
		StaticMesh->NaniteSettings.PositionPrecision = IntData[0];
	}

	// Look for the percent triangle attribute, one by default (all triangles)
	// as this mesh is also used in the physics engine as the complex collision version

	{
		StaticMesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
		TArray<float> FloatData;
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_PERCENT_TRIANGLES);

		// Look for a specific prim attribute first
		if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, FloatData, PrimIndex, 1))
		{
			//Global search for the attribute
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, FloatData, PrimIndex, 1);
		}

		if (FloatData.Num() > 0)
		{
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			// If a nanite percent triangles attribute was found, we likely also want to set the fallback target to PercentTriangles
			StaticMesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
	#endif
			StaticMesh->NaniteSettings.FallbackPercentTriangles = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
		}
	}

	{
		// Also look for an attribute setting the relative error (default to 1)
		StaticMesh->NaniteSettings.FallbackRelativeError = 1.0f;

		TArray<float> FloatData;
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_FB_RELATIVE_ERROR);
		if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, FloatData, PrimIndex, 1))
		{
			//Global search for the attribute
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, FloatData, PrimIndex, 1);
		}

		if (FloatData.Num() > 0)
		{
	#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			// If a nanite relative error attribute was found, we likely also want to set the fallback target to RelativeError
			StaticMesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::RelativeError;
	#endif
			StaticMesh->NaniteSettings.FallbackRelativeError = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
		}
	}

	{
		// And do the same for the trim relative error (default to 0)
		StaticMesh->NaniteSettings.TrimRelativeError = 0.0f;
		TArray<float> FloatData;
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_TRIM_RELATIVE_ERROR);
		if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, FloatData, PrimIndex, 1))
		{
			//Global search for the attribute
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, FloatData, PrimIndex, 1);
		}

		if (FloatData.Num() > 0)
		{
			StaticMesh->NaniteSettings.TrimRelativeError = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
		}
	}

	StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
}

void FHoudiniMeshTranslator::CopyAttributesFromHGPOForSplit(
	const FHoudiniGeoPartObject& InHGPO, const int32 InPointIndex, const int32 InPrimIndex, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens)
{
	// Get all the supported attributes from the HGPO
	{
		FString TempFolder;
		if (FHoudiniEngineUtils::GetTempFolderAttribute(InHGPO.GeoId, TempFolder, InHGPO.PartId, InPrimIndex))
		{
			if (!TempFolder.IsEmpty())
			{
				// cache the level path attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_TEMP_FOLDER, TempFolder);
			}
		}
	}

	{
		FString LevelPath;
		if (FHoudiniEngineUtils::GetLevelPathAttribute(InHGPO.GeoId, InHGPO.PartId, LevelPath, InPointIndex, InPrimIndex))
		{
			if (!LevelPath.IsEmpty())
			{
				// cache the level path attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPath);
			}
		}
	}

	{
		FString OutputName;
		if (FHoudiniEngineUtils::GetOutputNameAttribute(InHGPO.GeoId, InHGPO.PartId, OutputName, InPointIndex, InPrimIndex))
		{
			if (!OutputName.IsEmpty())
			{
				// cache the output name attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, OutputName);
			}
		}
	}

	{
		FString BakeName;
		if (FHoudiniEngineUtils::GetBakeNameAttribute(InHGPO.GeoId, InHGPO.PartId, BakeName, InPointIndex, InPrimIndex))
		{
			if (!BakeName.IsEmpty())
			{
				// cache the bake name attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_NAME, BakeName);
			}
		}
	}

	{
		int32 TileValue;
		if (FHoudiniEngineUtils::GetTileAttribute(InHGPO.GeoId, InHGPO.PartId, TileValue, InPointIndex, InPrimIndex))
		{
			if (TileValue >= 0)
			{
				// cache the tile attribute as a token on the output object
				OutTokens.Add(TEXT("tile"), FString::FromInt(TileValue));
			}
		}
	}

	{
		FString BakeOutputActorName;
		if (FHoudiniEngineUtils::GetBakeActorAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutputActorName, InPointIndex, InPrimIndex))
		{
			if (!BakeOutputActorName.IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, BakeOutputActorName);
			}
		}
	}

	{
		FString BakeOutputActorClassName;
		if (FHoudiniEngineUtils::GetBakeActorClassAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutputActorClassName, InPointIndex, InPrimIndex))
		{
			if (!BakeOutputActorClassName.IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, BakeOutputActorClassName);
			}
		}
	}

	{
		FString BakeFolder;
		if (FHoudiniEngineUtils::GetBakeFolderAttribute(InHGPO.GeoId, InHGPO.PartId, BakeFolder, InPrimIndex))
		{
			if (!BakeFolder.IsEmpty())
			{
				// cache the unreal_bake_folder attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolder);
			}
		}
	}

	{
		FString BakeOutlinerFolder;
		if (FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutlinerFolder, InPointIndex, InPrimIndex))
		{
			if (!BakeOutlinerFolder.IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, BakeOutlinerFolder);
			}
		}
	}
}

void FHoudiniMeshTranslator::CopyAttributesFromHGPOForSplit(
	const FString& InSplitGroupName, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens)
{
	int32 PointIndex = INDEX_NONE;
	const int32 PrimIndex = AllSplitFirstValidPrimIndex[InSplitGroupName];

	const int32 FirstValidVertexIndex = AllSplitFirstValidVertexIndex[InSplitGroupName];
	if (FirstValidVertexIndex >= 0 && AllSplitVertexLists[InSplitGroupName].IsValidIndex(FirstValidVertexIndex))
	{
		PointIndex = AllSplitVertexLists[InSplitGroupName][FirstValidVertexIndex];
	}

	CopyAttributesFromHGPOForSplit(HGPO, PointIndex, PrimIndex, OutAttributes, OutTokens);
}

void FHoudiniMeshTranslator::CopyAttributesFromHGPOForSplit(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens)
{
	const int32 PrimIndex = InOutputObjectIdentifier.PrimitiveIndex;
	const int32 PointIndex = InOutputObjectIdentifier.PointIndex;

	CopyAttributesFromHGPOForSplit(HGPO, PointIndex, PrimIndex, OutAttributes, OutTokens);
}


UStaticMesh*
FHoudiniMeshTranslator::CreateNewUnrealStaticMesh(const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	PackageParams.ObjectId = HGPO.ObjectId;
	PackageParams.GeoId = HGPO.GeoId;
	PackageParams.PartId = HGPO.PartId;
	PackageParams.SplitStr = InSplitIdentifier;

	UStaticMesh * NewStaticMesh = PackageParams.CreateObjectAndPackage<UStaticMesh>();
	if (!IsValid(NewStaticMesh))
		return nullptr;

	return NewStaticMesh;
}

UHoudiniStaticMesh*
FHoudiniMeshTranslator::CreateNewHoudiniStaticMesh(const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	PackageParams.ObjectId = HGPO.ObjectId;
	PackageParams.GeoId = HGPO.GeoId;
	PackageParams.PartId = HGPO.PartId;
	// Add _HSM suffix to the split str, to distinguish the temporary HoudiniStaticMesh
	// from the UStaticMesh
	PackageParams.SplitStr = InSplitIdentifier + "_HSM";

	UHoudiniStaticMesh * NewStaticMesh = PackageParams.CreateObjectAndPackage<UHoudiniStaticMesh>();
	if (!IsValid(NewStaticMesh))
		return nullptr;

	return NewStaticMesh;
}


FHoudiniOutputObjectIdentifier
FHoudiniMeshTranslator::MakeOutputObjectIdentifier(const FString& InSplitGroupName, const EHoudiniSplitType InSplitType)
{
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier(
		HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, GetMeshIdentifierFromSplit(InSplitGroupName, InSplitType));
	OutputObjectIdentifier.PartName = HGPO.PartName;
	OutputObjectIdentifier.PrimitiveIndex = AllSplitFirstValidPrimIndex[InSplitGroupName];
	const int32 FirstValidVertexIndex = AllSplitFirstValidVertexIndex[InSplitGroupName];
	if (FirstValidVertexIndex >= 0 && AllSplitVertexLists[InSplitGroupName].IsValidIndex(FirstValidVertexIndex))
	{
		OutputObjectIdentifier.PointIndex = AllSplitVertexLists[InSplitGroupName][FirstValidVertexIndex];
	}
	else
	{
		OutputObjectIdentifier.PointIndex = -1;
	}

	return OutputObjectIdentifier;
}

bool
FHoudiniMeshTranslator::CreateStaticMesh_MeshDescription()
{
	///////////////////////////////////////////////////////////////////////
	// THIS FUNCTION IS DEPRECATED AND WILL REMOVED IN THE NEXT RELEASE.
	///////////////////////////////////////////////////////////////////////

	// Time limit for processing
	bDoTiming = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;

	double time_start = FPlatformTime::Seconds();

	bool bIsGammaCorrectionDisabled = IsGammaCorrectionDisabled(HGPO.GeoId, HGPO.PartId);

	// Keep a copy of the initial package params, since PackageParams is modified in place when resolving attributes
	FHoudiniPackageParams InitialPackageParams = PackageParams;

	// Start by updating the vertex list
	if (!UpdatePartVertexList())
		return false;

	// Sort the split groups
	// Simple colliders first, lods and finally, invisible colliders (that are separate Static Mesh)
	SortSplitGroups();

	// Handles the split groups found in the part
	// and builds the corresponding faces and indices arrays
	if (!UpdateSplitsFacesAndIndices())
		return true;

	// Resets the containers used for the raw data extraction.
	ResetPartCache();

	// Prepare the object that will store UCX and simple colliders
	AllAggregateCollisions.Empty();

	// We need to know the number of LODs that will be needed for this part, and we also need the identifier for the
	// main split (Normal or LOD0)
	int32 NumberOfLODs = 0;
	bool bHasMainGeo = false;
	FHoudiniOutputObjectIdentifier MainIdentifier;
	bool bHasMainIdentifier = false;
	for (auto& curSplit : AllSplitGroups)
	{
		if (GetSplitTypeFromSplitName(curSplit) == EHoudiniSplitType::LOD)
		{
			NumberOfLODs++;
			if (NumberOfLODs == 1 && !bHasMainGeo)
			{
				MainIdentifier = MakeOutputObjectIdentifier(curSplit, GetSplitTypeFromSplitName(curSplit));
				bHasMainIdentifier = true;
			}
		}
		else if (GetSplitTypeFromSplitName(curSplit) == EHoudiniSplitType::Normal)
		{
			bHasMainGeo = true;
			MainIdentifier = MakeOutputObjectIdentifier(curSplit, GetSplitTypeFromSplitName(curSplit));
			bHasMainIdentifier = true;
		}
	}

	// Update the part's material's IDS and info now
	CreateNeededMaterials();

	// Check if the materials were updated
	bool bMaterialHasChanged = false;
	for (const auto& MatInfo : PartUniqueMaterialInfos)
	{
		if (MatInfo.hasChanged)
		{
			bMaterialHasChanged = true;
			break;
		}
	}

	// Get the current target platform for default lod policies
	ITargetPlatform * CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);

	// New mesh list
	TMap<FHoudiniOutputObjectIdentifier, UStaticMesh*> StaticMeshToBuild;

	// Map of Houdini Material IDs to Unreal Material Interface
	TMap<HAPI_NodeId, UMaterialInterface*> MapHoudiniMatIdToUnrealInterface;
	// Map of Houdini Material Attributes to Unreal Material Interface
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> MapHoudiniMatAttributesToUnrealInterface;
	// Map of Unreal Material Interface to Unreal Material Index, per visible mesh
	TMap<UStaticMesh*, TMap<UMaterialInterface*, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	bool MeshMaterialsHaveBeenReset = false;

	double tick = FPlatformTime::Seconds();
	if (bDoTiming)
		HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Pre Split-Loop in %f seconds."), tick - time_start);

	UStaticMesh* MainStaticMesh = nullptr;
	bool bAssignedCustomCollisionMesh = false;
	ECollisionTraceFlag MainStaticMeshCTF = StaticMeshGenerationProperties.GeneratedCollisionTraceFlag;

	// Map of object identifiers to package params
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniPackageParams> ObjectIdentifiersToPackageParams;

	// Iterate through all detected split groups we care about and split geometry.
	// The split are ordered in the following way:
	// Invisible Simple/Convex Colliders > LODs > MainGeo > Visible Colliders > Invisible Colliders
	for (int32 SplitId = 0; SplitId < AllSplitGroups.Num(); SplitId++)
	{
		double split_tick = FPlatformTime::Seconds();

		// Get split group name
		const FString& SplitGroupName = AllSplitGroups[SplitId];

		// Get the vertex indices for this group
		TArray<int32>& SplitVertexList = AllSplitVertexLists[SplitGroupName];

		// Get valid count of vertex indices for this split.
		const int32& SplitVertexCount = AllSplitVertexCounts[SplitGroupName];

		// Make sure we have a	valid vertex count for this split
		if (SplitVertexCount % 3 != 0 || SplitVertexList.Num() % 3 != 0)
		{
			// Invalid vertex count, skip this split or we'd crash trying to create a mesh for it.
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] invalid vertex count.")
				TEXT("- skipping."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);

			continue;
		}

		// Get the current split type
		EHoudiniSplitType SplitType = GetSplitTypeFromSplitName(SplitGroupName);
		if (SplitType == EHoudiniSplitType::Invalid)
		{
			// Invalid split, skip
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] unknown split type.")
				TEXT("- skipping."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			continue;
		}

		// Get the output identifer for this split
		FHoudiniOutputObjectIdentifier OutputObjectIdentifier = MakeOutputObjectIdentifier(SplitGroupName, SplitType);

		// Get/Create the Aggregate Collisions for this mesh identifier
		FKAggregateGeom& AggregateCollisions = AllAggregateCollisions.FindOrAdd(OutputObjectIdentifier);

		// Handle UCX / Convex Hull colliders
		if (SplitType == EHoudiniSplitType::InvisibleUCXCollider || SplitType == EHoudiniSplitType::RenderedUCXCollider)
		{
			// Get the part position if needed
			UpdatePartPositionIfNeeded();

			// Create the convex hull colliders and add them to the Aggregate
			if (!AddConvexCollisionToAggregate(SplitGroupName, AggregateCollisions))
			{
				MainStaticMeshCTF = ECollisionTraceFlag::CTF_UseDefault;
				// Failed to generate a convex collider
				HOUDINI_LOG_WARNING(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] failed to create convex collider."),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			}

			// If the collider is not visible, stop here
			if (SplitType == EHoudiniSplitType::InvisibleUCXCollider)
				continue;
		}
		else if (SplitType == EHoudiniSplitType::InvisibleSimpleCollider || SplitType == EHoudiniSplitType::RenderedSimpleCollider)
		{
			MainStaticMeshCTF = ECollisionTraceFlag::CTF_UseDefault;
			// Get the part position if needed
			UpdatePartPositionIfNeeded();

			// Create the simple colliders and add them to the aggregate
			if (!AddSimpleCollisionToAggregate(SplitGroupName, AggregateCollisions))
			{
				// Failed to generate a convex collider
				HOUDINI_LOG_WARNING(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] failed to create simple collider."),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			}

			// If the collider is not visible, stop here
			if (SplitType == EHoudiniSplitType::InvisibleSimpleCollider)
				continue;
		}

		// Try to find existing properties for this identifier
		// First check the OutputObjects (for LODs and Normal geo the same FHoudiniOutputObject entry is used, so
		// we check OutputObjects first to see if we have already created/updated it for the main mesh)
		FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(OutputObjectIdentifier);
		if (!FoundOutputObject)
			FoundOutputObject = InputObjects.Find(OutputObjectIdentifier);

		// If we don't yet have package params for this object identifier, fetch and resolve attributes for the split
		// and update the package params
		TMap<FString, FString> TempAttributes;
		TMap<FString, FString> TempTokens;
		bool bCopyAttributesAndTokens = false;
		if (!ObjectIdentifiersToPackageParams.Contains(OutputObjectIdentifier))
		{
			// Get all the supported attributes from the HGPO
			// For LOD / Normal mesh we use the MainIdentifier to read attributes (Normal or LOD0) since they all
			// go into the same mesh/package
			if ((SplitType == EHoudiniSplitType::Normal || SplitType == EHoudiniSplitType::LOD) && bHasMainIdentifier)
				CopyAttributesFromHGPOForSplit(MainIdentifier, TempAttributes, TempTokens);
			else
				CopyAttributesFromHGPOForSplit(OutputObjectIdentifier, TempAttributes, TempTokens);

			// Resolve our final package params
			FHoudiniAttributeResolver Resolver;
			FHoudiniPackageParams FinalPackageParams;
			FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
				InitialPackageParams,
				IsValid(OuterComponent) ? OuterComponent->GetWorld() : nullptr,
				OuterComponent,
				TempAttributes,
				TempTokens,
				PackageParams,
				Resolver);

			bCopyAttributesAndTokens = true;
			ObjectIdentifiersToPackageParams.Emplace(OutputObjectIdentifier, PackageParams);
		}
		else
		{
			PackageParams = ObjectIdentifiersToPackageParams.FindChecked(OutputObjectIdentifier);
		}

		// Try to find an existing SM from a previous cook
		UStaticMesh* FoundStaticMesh = FindExistingStaticMesh(OutputObjectIdentifier);

		// Flag whether or not we need to rebuild the mesh
		bool bRebuildStaticMesh = false;
		if (HGPO.GeoInfo.bHasGeoChanged || HGPO.PartInfo.bHasChanged || ForceRebuild || !FoundStaticMesh || !FoundOutputObject)
			bRebuildStaticMesh = true;

		// TODO: Handle materials
		if (!bRebuildStaticMesh && !bMaterialHasChanged)
		{
			// We can simply reuse the found static mesh
			OutputObjects.Add(OutputObjectIdentifier, *FoundOutputObject);
			continue;
		}

		// Prepare LOD Group data for this static mesh
		FStaticMeshLODGroup LODGroup;

		bool bNewStaticMeshCreated = false;
		if (!FoundStaticMesh)
		{
			// If we couldn't find a valid existing static mesh, create a new one
			FoundStaticMesh = CreateNewUnrealStaticMesh(OutputObjectIdentifier.SplitIdentifier);
			if (!IsValid(FoundStaticMesh))
				continue;

			bNewStaticMeshCreated = true;

			// Use the platform's default LODGroup policy
			// TODO? Add setting for default LOD Group?
			LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
		}
		else
		{
			// Try to reuse the existing SM's LOD group instead of the default one
			LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(FoundStaticMesh->LODGroup);
		}

		if (SplitType == EHoudiniSplitType::Normal)
		{
			MainStaticMesh = FoundStaticMesh;
			MainStaticMesh->ComplexCollisionMesh = nullptr;
			MainStaticMesh->bCustomizedCollision = false;
		}

		if (!FoundOutputObject)
		{
			FHoudiniOutputObject NewOutputObject;
			FoundOutputObject = &OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
		}
		FoundOutputObject->bProxyIsCurrent = false;

		// Update the attributes and tokens if this is the first split for this object identifier
		if (bCopyAttributesAndTokens)
		{
			FoundOutputObject->CachedAttributes = MoveTemp(TempAttributes);
			FoundOutputObject->CachedTokens = MoveTemp(TempTokens);
		}

		// TODO: Needed?
		// Free any RHI resources for existing mesh before we re-create in place.
		FoundStaticMesh->PreEditChange(nullptr);

		// Check that the Static Mesh we found has the appropriate number of Source models/LODs
		int32 NeededNumberOfLODs = FMath::Max(NumberOfLODs + (bHasMainGeo ? 1 : 0), LODGroup.GetDefaultNumLODs());

		// LODs are only for the "main" mesh, not for complex colliders!
		if (SplitType == EHoudiniSplitType::InvisibleComplexCollider || SplitType == EHoudiniSplitType::RenderedComplexCollider)
			NeededNumberOfLODs = FMath::Max(1, LODGroup.GetDefaultNumLODs());


		if (FoundStaticMesh->GetNumSourceModels() != NeededNumberOfLODs)
		{
			while (FoundStaticMesh->GetNumSourceModels() < NeededNumberOfLODs)
				FoundStaticMesh->AddSourceModel();

			// We may have to remove excessive LOD levels
			if (FoundStaticMesh->GetNumSourceModels() > NeededNumberOfLODs)
				FoundStaticMesh->SetNumSourceModels(NeededNumberOfLODs);

			// Initialize their default reduction setting
			for (int32 ModelLODIndex = 0; ModelLODIndex < NeededNumberOfLODs; ModelLODIndex++)
			{
				FoundStaticMesh->GetSourceModel(ModelLODIndex).ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
			}
			FoundStaticMesh->SetLightMapResolution(LODGroup.GetDefaultLightMapResolution());
		}

		// By default, always work on the first source model, unless we're a LOD
		int32 SrcModelIndex = 0;
		int32 LODIndex = 0;
		if (SplitType == EHoudiniSplitType::LOD)
		{
			for (auto& curSplit : AllSplitGroups)
			{
				EHoudiniSplitType CurrentSplitType = GetSplitTypeFromSplitName(curSplit);
				if (CurrentSplitType == EHoudiniSplitType::LOD
					|| CurrentSplitType == EHoudiniSplitType::Normal)
				{
					LODIndex++;
				}

				if (curSplit == SplitGroupName)
					break;
			}

			// Fix for the case where we don't have a main geo
			if(!bHasMainGeo)
				LODIndex--;
		}

		// Grab the appropriate SourceModel
		FStaticMeshSourceModel* SrcModel = (FoundStaticMesh->IsSourceModelValid(LODIndex)) ? &(FoundStaticMesh->GetSourceModel(LODIndex)) : nullptr;
		if (!SrcModel)
		{
			HOUDINI_LOG_ERROR(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d, %s] Could not access SourceModel for the LOD %d - skipping."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName, LODIndex);
			continue;
		}

		if (bDoTiming)
		{
			HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - PreMeshDescription in %f seconds."), FPlatformTime::Seconds() - tick);
			tick = FPlatformTime::Seconds();
		}

		bool bHasNormal = false;
		bool bHasTangents = false;

		// Load the existing mesh description if we don't need to rebuild the mesh		
		FMeshDescription* MeshDescription;
		if (!bRebuildStaticMesh)
		{
			// We dont need to rebuild the mesh itself:
			// the geometry hasn't changed, but the materials have.
			// We can just reuse the old MeshDescription and reuse it.
			MeshDescription = FoundStaticMesh->GetMeshDescription(LODIndex);
		}
		else
		{
			// Extract all the data needed for this split
			// Start by initializing the MeshDescription for this LOD			
			MeshDescription = FoundStaticMesh->CreateMeshDescription(LODIndex);
			FStaticMeshAttributes(*MeshDescription).Register();

			// Mesh description uses material to create its PolygonGroups,
			// so we first need to know how many different materials we have for this split
			// and what vertices/indices belong to each material for remapping

			//--------------------------------------------------------------------------------------------------------------------- 
			//  INDICES
			//--------------------------------------------------------------------------------------------------------------------- 

			//
			// Because of the splits, we don't need to declare all the vertices in the Part, 
			// but only the one that are currently used by the split's faces.
			// The indicesMapper array is used to map those indices from Part Vertices to Split Vertices.
			// We also keep track of the needed vertices index to declare them easily afterwards.
			//

			// SplitNeededVertices
			// Array containing the (unique) part indices for the vertices that are needed for this split
			TArray<int32> SplitNeededVertices;

			// IndicesMapper:
			// Maps index values for all vertices in the Part:
			// - Vertices unused by the split will be set to -1
			// - Used vertices will have their value set to the "NewIndex" so that IndicesMapper[ partIndex ] => splitIndex
			TArray<int32> PartToSplitIndicesMapper;
			PartToSplitIndicesMapper.SetNumUninitialized(SplitVertexList.Num());
			for (int32 n = 0; n < PartToSplitIndicesMapper.Num(); n++)
				PartToSplitIndicesMapper[n] = -1;

			// SplitIndices
			// Array of SplitIndices used to describe this split's polygons
			TArray<uint32> SplitIndices;
			SplitIndices.SetNumZeroed(SplitVertexCount);

			int32 CurrentSplitIndex = 0;
			int32 ValidVertexId = 0;
			bool bHasInvalidFaceIndices = false;
			for (int32 VertexIdx = 0; VertexIdx < SplitVertexList.Num(); VertexIdx += 3)
			{
				int32 WedgeCheck = SplitVertexList[VertexIdx + 0];
				if (WedgeCheck == -1)
					continue;

				int32 WedgeIndices[3] =
				{
					SplitVertexList[VertexIdx + 0],
					SplitVertexList[VertexIdx + 1],
					SplitVertexList[VertexIdx + 2]
				};

				// Ensure the indices are valid
				if (!PartToSplitIndicesMapper.IsValidIndex(WedgeIndices[0])
					|| !PartToSplitIndicesMapper.IsValidIndex(WedgeIndices[1])
					|| !PartToSplitIndicesMapper.IsValidIndex(WedgeIndices[2]))
				{
					// Invalid face index.
					bHasInvalidFaceIndices = true;
					continue;
				}

				// Converting Old (Part) Indices to New (Split) Indices:
				for (int32 i = 0; i < 3; i++)
				{
					if (PartToSplitIndicesMapper[WedgeIndices[i]] < 0)
					{
						// This part index has not yet been "converted" to a new split index
						SplitNeededVertices.Add(WedgeIndices[i]);
						PartToSplitIndicesMapper[WedgeIndices[i]] = CurrentSplitIndex;
						CurrentSplitIndex++;
					}

					// Replace the old part index with the new split index
					WedgeIndices[i] = PartToSplitIndicesMapper[WedgeIndices[i]];
				}

				if (!SplitIndices.IsValidIndex(ValidVertexId + 2))
					break;

				// Flip wedge indices to fix the winding order.
				SplitIndices[ValidVertexId + 0] = WedgeIndices[0];
				SplitIndices[ValidVertexId + 1] = WedgeIndices[2];
				SplitIndices[ValidVertexId + 2] = WedgeIndices[1];

				ValidVertexId += 3;
			}
			if (bHasInvalidFaceIndices)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] has some invalid face indices"),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			}

			if (bDoTiming)
			{
				HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Indices in %f seconds."), FPlatformTime::Seconds() - tick);
				tick = FPlatformTime::Seconds();
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// POSITIONS
			//---------------------------------------------------------------------------------------------------------------------				

			// Extract position for this part
			UpdatePartPositionIfNeeded();

			// Transfer vertex positions:
			//
			// Because of the split, we're only interested in the needed vertices.
			// Instead of declaring all the Positions, we'll only declare the vertices
			// needed by the current split.
			//
			TVertexAttributesRef<FVector3f> VertexPositions = 
				MeshDescription->VertexAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Vertex::Position);

			bool bHasInvalidPositionIndexData = false;
			MeshDescription->ReserveNewVertices(SplitNeededVertices.Num());
			for (const int32& NeededVertexIndex : SplitNeededVertices)
			{
				// Create a new Vertex
				FVertexID VertexID = MeshDescription->CreateVertex();
				if (PartPositions.IsValidIndex(NeededVertexIndex * 3 + 2))
				{
					// We need to swap Z and Y coordinate here, and convert from m to cm. 
					VertexPositions[VertexID].X = PartPositions[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
					VertexPositions[VertexID].Y = PartPositions[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
					VertexPositions[VertexID].Z = PartPositions[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
				}
				else
				{
					// Error when retrieving positions.
					bHasInvalidPositionIndexData = true;

					continue;
				}
			}

			if (bHasInvalidPositionIndexData)
			{
				HOUDINI_LOG_WARNING(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] invalid position/index data ")
					TEXT("- skipping."),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			}

			if (bDoTiming)
			{
				HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Positions in %f seconds."), FPlatformTime::Seconds() - tick);
				tick = FPlatformTime::Seconds();
			}


			//--------------------------------------------------------------------------------------------------------------------- 
			// MATERIALS
			//---------------------------------------------------------------------------------------------------------------------

			TArray<FStaticMaterial>& FoundStaticMaterials = FoundStaticMesh->GetStaticMaterials();

			// // TODO: Check if still needed for MeshDescription
			// // We need to reset the Static Mesh's materials once per SM:
			// // so, for the first lod, or the main geo...
			// if (!MeshMaterialsHaveBeenReset && (SplitType == EHoudiniSplitType::LOD || SplitType == EHoudiniSplitType::Normal))
			// {
			// 	FoundStaticMaterials.Empty();
			// 	MeshMaterialsHaveBeenReset = true;
			// }
			//
			// // ..  or for each visible complex collider
			// if (SplitType == EHoudiniSplitType::RenderedComplexCollider)
			// 	FoundStaticMaterials.Empty();

			// Clear the materials array of the mesh the first time we encounter it
			if (!MapUnrealMaterialInterfaceToUnrealIndexPerMesh.Contains(FoundStaticMesh))
			{
				FoundStaticMaterials.Empty();
			}
			TMap<UMaterialInterface*, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

			// Get this split's faces
			TArray<int32>& SplitGroupFaceIndices = AllSplitFaceIndices[SplitGroupName];
			// Array holding the materials needed for this split
			//TArray<UMaterialInterface*> SplitMaterials;
			// Split Material indices per face, by default all faces are set to use the first Material
			TArray<int32> SplitFaceMaterialIndices;
			SplitFaceMaterialIndices.SetNumZeroed(SplitGroupFaceIndices.Num());

			bool HasHoudiniMaterials = PartUniqueMaterialIds.Num() > 0;
			bool HasMaterialOverrides = PartFaceMaterialOverrides.Num() > 0;
			if (!HasHoudiniMaterials && !HasMaterialOverrides)
			{
				// We don't have any material override or houdini material
				// we just need one polygon group using the default Houdini material.
				UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

				// See if we have a replacement material and use it on the mesh instead
				UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(
					FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, ""));
				if (ReplacementMaterial && *ReplacementMaterial)
					MaterialInterface = *ReplacementMaterial;

				FoundStaticMaterials.Empty();
				FoundStaticMaterials.Add(MaterialInterface);

				// TODO: ? Add default mat to the assignement map?
			}
			else if (HasHoudiniMaterials && !HasMaterialOverrides)
			{
				// We have Houdini Material but no overrides
				if (bOnlyOneFaceMaterial || PartUniqueMaterialIds.Num() == 1)
				{
					// We have only one Houdini material.
					// Use default Houdini material if no valid material is assigned to any of the faces.
					UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

					// Get id of this single material.
					FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
					const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, PartFaceMaterialIds[0], MaterialPathName);
					const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
					UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
					if (FoundMaterial)
						MaterialInterface = *FoundMaterial;

					// See if we have a replacement material and use it on the mesh instead
					UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
					if (ReplacementMaterial && *ReplacementMaterial)
						MaterialInterface = *ReplacementMaterial;

					FoundStaticMaterials.Empty();
					FoundStaticMaterials.Add(MaterialInterface);

					// TODO: ? Add the mat to the assignement map?
				}
				else
				{
					// We have multiple houdini materials
					// Get default Houdini material.
					UMaterial* MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get();

					// Reset Rawmesh material face assignments.
					for (int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
					{
						int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];
						if (!PartFaceMaterialIds.IsValidIndex(SplitFaceIndex))
							continue;

						// Get material id for this face.
						HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

						// See if we have already treated that material
						UMaterialInterface** FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
						UMaterialInterface* MaterialInterface = nullptr;
						if (FoundMaterialInterface)
							MaterialInterface = *FoundMaterialInterface;

						if (MaterialInterface)
						{
							int32 const* FoundUnrealMatIndex = MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Find(MaterialInterface);
							if (FoundUnrealMatIndex)
							{
								// This material has been mapped already, just assign the mat index
								SplitFaceMaterialIndices[FaceIdx] = *FoundUnrealMatIndex;
								continue;
							}
						}
						else
						{
							MaterialInterface = Cast<UMaterialInterface>(MaterialDefault);

							FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
							const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
							const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
							UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
							if (FoundMaterial)
								MaterialInterface = *FoundMaterial;

							// See if we have a replacement material and use it on the mesh instead
							UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
							if (ReplacementMaterial && *ReplacementMaterial)
								MaterialInterface = *ReplacementMaterial;

							MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
						}

						if (MaterialInterface)
						{
							// Add the material to the Static mesh
							//int32 UnrealMatIndex = SplitMaterials.Add(Material);
							int32 UnrealMatIndex = FoundStaticMaterials.Add(MaterialInterface);

							// Map the houdini ID to the unreal one
							MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Add(MaterialInterface, UnrealMatIndex);

							// Update the face index
							SplitFaceMaterialIndices[FaceIdx] = UnrealMatIndex;
						}
					}
				}
			}
			else
			{
				// Array used to avoid constantly attempting to load invalid materials
				TArray<FString> InvalidMaterials;

				// If we have material overrides
				for (int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
				{
					int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];

					UMaterialInterface * MaterialInterface = nullptr;
					int32 CurrentFaceMaterialIdx = -1;
					int32 MatIndex = -1;
					if (PartFaceMaterialOverrides.IsValidIndex(SplitFaceIndex))
					{
						// For MeshDescription specifically, we need OutputAssignmentMaterials/MapHoudiniMatAttributesToUnrealInterface
						// to have an entry for each index (distinguish between identical materials in different slots).
						// This is so we have the correct size and create enough polygon groups later.
						const FHoudiniMaterialInfo& MatInfo = PartFaceMaterialOverrides[SplitFaceIndex];
						const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
						FString MaterialName = MatInfo.MaterialObjectPath;
						ExtractMaterialIndex(MaterialName, MatIndex);
						UMaterialInterface** FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
						if (FoundMaterialInterface)
							MaterialInterface = *FoundMaterialInterface;

						if (!MaterialInterface)
						{
							// Try to locate the corresponding material interface

							// Start by looking in our assignment map
							FoundMaterialInterface = OutputAssignmentMaterials.Find(MaterialIdentifier);
							if (FoundMaterialInterface)
								MaterialInterface = *FoundMaterialInterface;

							if (!MaterialInterface && !MaterialName.IsEmpty() && !InvalidMaterials.Contains(MaterialName))
							{
								// Only try to load a material if has a chance to be valid!
								MaterialInterface = Cast< UMaterialInterface >(
									StaticLoadObject(UMaterialInterface::StaticClass(),
										nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr));

								if (!MaterialInterface)
									InvalidMaterials.Add(MaterialName);
							}

							if (MaterialInterface)
							{
								// We managed to load the UE4 material
								// Make sure this material is in the assignments before replacing it.
								OutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);
								
								// See if we have a replacement material and use it on the mesh instead
								UMaterialInterface * const *ReplacementMaterialInterface = ReplacementMaterials.Find(MaterialIdentifier);
								if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
									MaterialInterface = *ReplacementMaterialInterface;

								// Add this material to the map
								MapHoudiniMatAttributesToUnrealInterface.Add(MaterialIdentifier, MaterialInterface);
							}
						}

						if (!MaterialInterface)
						{
							// The attribute Material or its replacement do not exist
							// See if we can fallback to the Houdini material assigned on the face

							// Get the unreal material corresponding to this houdini one
							HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

							// See if we have already treated that material
							FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
							if (FoundMaterialInterface)
								MaterialInterface = *FoundMaterialInterface;

							if (!MaterialInterface)
							{
								// If everything else fails, we'll use the default material
								MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

								// We need to add this material to the map
								FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
								const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
								const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
								UMaterialInterface * const * FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
								if (FoundMaterial)
									MaterialInterface = *FoundMaterial;

								if (MaterialInterface)
								{
									// Make sure this material is in the assignments before replacing it.
									OutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

									// See if we have a replacement material and use it on the mesh instead
									UMaterialInterface* const* ReplacementMaterialInterface = ReplacementMaterials.Find(DefaultMatIdentifier);
									if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
										MaterialInterface = *ReplacementMaterialInterface;

									// Map the Houdini ID to the unreal one
									MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
								}
							}
						}
					}

					CurrentFaceMaterialIdx = GetFaceMaterialIndex(MaterialInterface,
						MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
						MatIndex,
						FoundStaticMaterials);

					// Update the Face Material on the mesh
					SplitFaceMaterialIndices[FaceIdx] = CurrentFaceMaterialIdx;
				}
			}

			// Create a Polygon Group for each material slot
			TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
				MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

			// We must use the number of assignment materials found to reserve the number of material slots
			// Don't use the SM's StaticMaterials here as we may not reserve enough polygon groups when adding more materials
			// Create a polygon group for each material slot.
			int32 NumberOfMaterials = OutputAssignmentMaterials.Num();
			if (NumberOfMaterials <= 0)
			{
				// No materials, create a polygon group for the default one
				const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
				PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = FName(HAPI_UNREAL_DEFAULT_MATERIAL_NAME);
			}
			else
			{
				MeshDescription->ReserveNewPolygonGroups(NumberOfMaterials);
				//for (int32 MatIndex = 0; MatIndex < NumberOfMaterials; ++MatIndex)
				for (auto& CurrentMatAssignement : OutputAssignmentMaterials)
				{
					const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
					PolygonGroupImportedMaterialSlotNames[PolygonGroupID] =
						FName(CurrentMatAssignement.Value ? *(CurrentMatAssignement.Value->GetName()) : *(CurrentMatAssignement.Key.MaterialObjectPath));
				}
			}

			if (bDoTiming)
			{
				HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Materials in %f seconds."), FPlatformTime::Seconds() - tick);
				tick = FPlatformTime::Seconds();
			}

			//
			// VERTEX INSTANCE ATTRIBUTES
			// NORMALS, TANGENTS, COLORS, UVS, Alpha
			//

			// Extract the normals
			UpdatePartNormalsIfNeeded();
			// Get the normals for this split
			TArray<float> SplitNormals;
			FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
				SplitVertexList, AttribInfoNormals, PartNormals, SplitNormals);

			TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal);

			// No need to read the tangents if we want unreal to recompute them after
			const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
			bool bReadTangents = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->RecomputeTangentsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always : true;

			// Extract the tangents
			TArray<float> SplitTangentU;
			TArray<float> SplitTangentV;
			if (bReadTangents)
			{
				// Extract this part's Tangents if needed
				UpdatePartTangentsIfNeeded();

				// Get the Tangents for this split
				FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
					SplitVertexList, AttribInfoTangentU, PartTangentU, SplitTangentU);

				// Get the binormals for this split
				FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
					SplitVertexList, AttribInfoTangentV, PartTangentV, SplitTangentV);

				// We need to manually generate tangents if:
				// - we have normals but dont have tangentu or tangentv attributes
				// - we have not specified that we wanted unreal to generate them
				int32 NormalCount = SplitNormals.Num();
				bool bGenerateTangents = (NormalCount > 0) && (SplitTangentU.Num() <= 0 || SplitTangentV.Num() <= 0);
				// Check that the number of tangents read matches the number of normals
				if (SplitTangentU.Num() != NormalCount || SplitTangentV.Num() != NormalCount)
					bGenerateTangents = true;

				if (bGenerateTangents && (HoudiniRuntimeSettings->RecomputeTangentsFlag == EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always))
				{
					// No need to generate tangents if we want unreal to recompute them after
					bGenerateTangents = false;
				}

				// Generate the tangents if needed
				if (bGenerateTangents)
				{
					SplitTangentU.SetNumZeroed(NormalCount);
					SplitTangentV.SetNumZeroed(NormalCount);
					for (int32 Idx = 0; Idx + 2 < NormalCount; Idx += 3)
					{
						FVector3f TangentZ;
						TangentZ.X = SplitNormals[Idx + 0];
						TangentZ.Y = SplitNormals[Idx + 2];
						TangentZ.Z = SplitNormals[Idx + 1];

						FVector3f TangentX, TangentY;
						TangentZ.FindBestAxisVectors(TangentX, TangentY);

						SplitTangentU[Idx + 0] = TangentX.X;
						SplitTangentU[Idx + 2] = TangentX.Y;
						SplitTangentU[Idx + 1] = TangentX.Z;

						SplitTangentV[Idx + 0] = TangentY.X;
						SplitTangentV[Idx + 2] = TangentY.Y;
						SplitTangentV[Idx + 1] = TangentY.Z;
					}
				}
			}
			TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = 
				MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Tangent);
			TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = 
				MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);

			// Extract the color values
			UpdatePartColorsIfNeeded();
			// Get the colors values for this split
			TArray<float> SplitColors;
			FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
				SplitVertexList, AttribInfoColors, PartColors, SplitColors);

			// Extract the alpha values
			UpdatePartAlphasIfNeeded();
			// Get the colors values for this split
			TArray<float> SplitAlphas;
			FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
				SplitVertexList, AttribInfoAlpha, PartAlphas, SplitAlphas);
			TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = 
				MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);

			// Extract UVs
			UpdatePartUVSetsIfNeeded(true);
			// See if we need to transfer uv point attributes to vertex attributes.
			int32 UVSetCount = PartUVSets.Num();
			TArray<TArray<float>> SplitUVSets;
			SplitUVSets.SetNum(UVSetCount);
			for (int32 TexCoordIdx = 0; TexCoordIdx < UVSetCount; TexCoordIdx++)
			{
				FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
					SplitVertexList, AttribInfoUVSets[TexCoordIdx], PartUVSets[TexCoordIdx], SplitUVSets[TexCoordIdx]);
			}
			TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = 
				MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate);					
			VertexInstanceUVs.SetNumChannels(UVSetCount);

			if (bDoTiming)
			{
				HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - VertexAttr extracted in %f seconds."), FPlatformTime::Seconds() - tick);
				tick = FPlatformTime::Seconds();
			}

			// Allocate space for the vertex instances and polygons
			MeshDescription->ReserveNewVertexInstances(SplitIndices.Num());
			MeshDescription->ReserveNewPolygons(SplitIndices.Num() / 3);
			//Approximately 2.5 edges per polygons
			MeshDescription->ReserveNewEdges(SplitIndices.Num() * 2.5f / 3);

			bHasNormal = SplitNormals.Num() > 0;
			bHasTangents = SplitTangentU.Num() > 0 && SplitTangentV.Num() > 0;
			bool bHasRGB = SplitColors.Num() > 0;
			bool bHasRGBA = bHasRGB && AttribInfoColors.tupleSize == 4;
			bool bHasAlpha = SplitAlphas.Num() > 0;

			TArray<bool> HasUVSets;
			HasUVSets.SetNumZeroed(PartUVSets.Num());
			for (int32 Idx = 0; Idx < PartUVSets.Num(); Idx++)
				HasUVSets[Idx] = PartUVSets[Idx].Num() > 0;

			uint32 FaceCount = SplitIndices.Num() / 3;
			for (uint32 FaceIndex = 0; FaceIndex < FaceCount; FaceIndex++)
			{
				TArray<FVertexInstanceID> FaceVertexInstanceIDs;
				FaceVertexInstanceIDs.SetNum(3);

				// Ignore degenerate triangles
				FVertexID VertexIDs[3];
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					VertexIDs[Corner] = FVertexID(SplitIndices[(FaceIndex * 3) + Corner]);
				}
				if (VertexIDs[0] == VertexIDs[1] || VertexIDs[0] == VertexIDs[2] || VertexIDs[1] == VertexIDs[2])
					continue;

				//FVertexID FaceVertexIDs[3];
				for (int32 Corner = 0; Corner < 3; Corner++)
				{
					uint32 SplitIndex = (FaceIndex * 3) + Corner;
					uint32 SplitVertexIndex = SplitIndices[SplitIndex];
					const FVertexInstanceID& VertexInstanceID = MeshDescription->CreateVertexInstance(FVertexID(SplitVertexIndex));

					// Fix the winding order by updating the SplitIndex (invert corner 1 and 2)
					// instead of going 0 1 2 go 0 2 1
					// TODO; this slows down StaticMesh->Build() considerably!
					Corner == 1 ? SplitIndex++ : Corner == 2 ? SplitIndex-- : SplitIndex;

					const uint32 SplitVertexIndex_X = SplitIndex * 3 + 0;
					const uint32 SplitVertexIndex_Y = SplitIndex * 3 + 2;
					const uint32 SplitVertexIndex_Z = SplitIndex * 3 + 1;
					// Normals
					if (bHasNormal)
					{
						// We need to swap Z and Y coordinate here, and convert from m to cm. 
						VertexInstanceNormals[VertexInstanceID].X = SplitNormals[SplitVertexIndex_X];
						VertexInstanceNormals[VertexInstanceID].Y = SplitNormals[SplitVertexIndex_Y];
						VertexInstanceNormals[VertexInstanceID].Z = SplitNormals[SplitVertexIndex_Z];
					}

					// Tangents and binormals
					if (bHasTangents)
					{
						// We need to swap Z and Y coordinate here, and convert from m to cm.
						VertexInstanceTangents[VertexInstanceID].X = SplitTangentU[SplitVertexIndex_X];
						VertexInstanceTangents[VertexInstanceID].Y = SplitTangentU[SplitVertexIndex_Y];
						VertexInstanceTangents[VertexInstanceID].Z = SplitTangentU[SplitVertexIndex_Z];

						FVector3f TangentY;
						TangentY.X = SplitTangentV[SplitVertexIndex_X];
						TangentY.Y = SplitTangentV[SplitVertexIndex_Y];
						TangentY.Z = SplitTangentV[SplitVertexIndex_Z];

						VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(
							(FVector)VertexInstanceTangents[VertexInstanceID].GetSafeNormal(),
							(FVector)TangentY.GetSafeNormal(),
							(FVector)VertexInstanceNormals[VertexInstanceID].GetSafeNormal());
					}

					// Color
					FLinearColor Color = FLinearColor::White;
					if (bHasRGB)
					{
						Color.R = FMath::Clamp(
							SplitColors[SplitIndex * AttribInfoColors.tupleSize + 0], 0.0f, 1.0f);
						Color.G = FMath::Clamp(
							SplitColors[SplitIndex * AttribInfoColors.tupleSize + 1], 0.0f, 1.0f);
						Color.B = FMath::Clamp(
							SplitColors[SplitIndex * AttribInfoColors.tupleSize + 2], 0.0f, 1.0f);
					}
					// Alpha
					if (bHasAlpha)
					{
						Color.A = FMath::Clamp(SplitAlphas[SplitIndex], 0.0f, 1.0f);
					}
					else if (bHasRGBA)
					{
						Color.A = FMath::Clamp(SplitColors[SplitIndex * AttribInfoColors.tupleSize + 3], 0.0f, 1.0f);
					}

					if (bIsGammaCorrectionDisabled)
					{
						// Mesh Description colors are always gamma corrected by Unreal. So we have to reverse the correction
						// if this flag is enabled.
						Color =  FLinearColor::FromSRGBColor(Color.ToFColor(false));
					}
					FVector4f VertexColor = FVector4f(Color);
					VertexInstanceColors[VertexInstanceID] = VertexColor;

					// UVs
					for (int32 UVIndex = 0; UVIndex < SplitUVSets.Num(); UVIndex++)
					{
						if (HasUVSets[UVIndex])
						{
							// We need to flip V coordinate when it's coming from HAPI.
							FVector2f CurrentUV;
							CurrentUV.X = SplitUVSets[UVIndex][SplitIndex * 2 + 0];
							CurrentUV.Y = 1.0f - SplitUVSets[UVIndex][SplitIndex * 2 + 1];

							VertexInstanceUVs.Set(VertexInstanceID, UVIndex, CurrentUV);
						}
					}

					FaceVertexInstanceIDs[Corner] = VertexInstanceID;
				}

				const FPolygonGroupID PolygonGroupID(SplitFaceMaterialIndices[FaceIndex]);

				// Insert a triangle into the mesh
				MeshDescription->CreateTriangle(PolygonGroupID, FaceVertexInstanceIDs);
			}

			if (bDoTiming)
			{
				HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - VertexAttr filled in %f seconds."), FPlatformTime::Seconds() - tick);
				tick = FPlatformTime::Seconds();
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			//  FACE SMOOTHING
			//---------------------------------------------------------------------------------------------------------------------

			// Extract this part's FaceSmoothing values if needed
			UpdatePartFaceSmoothingIfNeeded();

			// Get the FaceSmoothing values for this split
			TArray<int32> SplitFaceSmoothingMasks;
			FHoudiniMeshTranslator::TransferPartAttributesToSplit<int32>(
				SplitVertexList, AttribInfoFaceSmoothingMasks, PartFaceSmoothingMasks, SplitFaceSmoothingMasks);

			// FaceSmoothing masks must be initialized even if we don't have a value from Houdini!
			// TODO: Expose the default FaceSmoothing value
			// 0 will make hard face
			TArray<uint32> FaceSmoothingMasks;
			FaceSmoothingMasks.SetNumUninitialized(SplitVertexCount / 3);
			for (int32 n = 0; n < FaceSmoothingMasks.Num(); n++)
				FaceSmoothingMasks[n] = DefaultMeshSmoothing;

			// Check that the number of face smoothing values we retrieved is correct
			int32 WedgeFaceSmoothCount = SplitFaceSmoothingMasks.Num() / 3;
			if (SplitFaceSmoothingMasks.Num() != 0 && !SplitFaceSmoothingMasks.IsValidIndex((WedgeFaceSmoothCount - 1) * 3 + 2))
			{
				// Ignore our face smoothing values
				WedgeFaceSmoothCount = 0;
				HOUDINI_LOG_WARNING(TEXT("Invalid face smoothing mask count detected - Skipping them."));
			}

			// Transfer the face smoothing masks to the raw mesh if we have any
			for (int32 WedgeFaceSmoothIdx = 0; WedgeFaceSmoothIdx < WedgeFaceSmoothCount; WedgeFaceSmoothIdx += 3)
			{
				FaceSmoothingMasks[WedgeFaceSmoothIdx] = SplitFaceSmoothingMasks[WedgeFaceSmoothIdx * 3];
			}

			// TODO
			// Check
			FStaticMeshOperations::ConvertSmoothGroupToHardEdges(FaceSmoothingMasks, *MeshDescription);

			if (bDoTiming)
			{
				HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - FaceSoothing filled in %f seconds."), FPlatformTime::Seconds() - tick);
				tick = FPlatformTime::Seconds();
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// LIGHTMAP RESOLUTION
			//--------------------------------------------------------------------------------------------------------------------- 
			// Extract this part's LightmapResolution values if needed
			UpdatePartLightmapResolutionsIfNeeded();

			// make sure the mesh has a new lighting guid
			FoundStaticMesh->SetLightingGuid(FGuid::NewGuid());
		}

		// Update the Build Settings using the default setting values
		UpdateMeshBuildSettings(
			SrcModel->BuildSettings,
			bHasNormal,
			bHasTangents,
			PartUVSets.Num() > 0);

		// Set the lightmap Coordinate Index
		// If we have more than one UV set, the 2nd valid set is used for lightmaps by convention
		FoundStaticMesh->SetLightMapCoordinateIndex(PartUVSets.Num() > 1 ? 1 : 0);

		// Check for a lightmapa resolution override
		int32 LightMapResolutionOverride = -1;
		if ( PartLightMapResolutions.Num() > 0)
			LightMapResolutionOverride = PartLightMapResolutions[0];

		if (LightMapResolutionOverride > 0)
			FoundStaticMesh->SetLightMapResolution(LightMapResolutionOverride);
		else
			FoundStaticMesh->SetLightMapResolution(64);

		// TODO:
		// Turnoff bGenerateLightmapUVs if lightmap uv sets has bad uvs ?

		// RAW MESH CHECKS

		// Check if the mesh has at least one triangle, if not, log a message
		if (MeshDescription->Triangles().Num() == 0)
		{
			HOUDINI_LOG_WARNING(
				TEXT("[CreateStaticMesh_MeshDescription]: 0 valid triangles in StaticMesh data for %s LOD %i! Please check the log."),
				*FoundStaticMesh->GetName(), LODIndex);
		}

		// Store the new MeshDescription
		FoundStaticMesh->CommitMeshDescription(LODIndex);
		//Set the Imported version before calling the build
		FoundStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
		
		// LOD Screensize
		// default values has already been set, see if we have any attribute override for this
		float screensize = GetLODSCreensizeForSplit(SplitGroupName);
		if (screensize >= 0.0f)
		{
			// Only apply the LOD screensize if it's valid
			SrcModel->ScreenSize = screensize;
			//FoundStaticMesh->GetSourceModel(LodIndex).ScreenSize = screensize;
			FoundStaticMesh->bAutoComputeLODScreenSize = false;
		}

		UpdateStaticMeshNaniteSettings(HGPO.GeoId, HGPO.PartId, OutputObjectIdentifier.PrimitiveIndex, FoundStaticMesh);

		// SET STATIC MESH GENERATION PARAM
		// HANDLE COLLIDERS
		// REMOVE OLD COLLIDERS
		// CUSTOM BAKE NAME OVERRIDE
		
		// UPDATE UPROPERTY ATTRIBS
		// Update property attributes on the source model
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			HGPO.GeoId,
			HGPO.PartId,
			true,
			OutputObjectIdentifier.PrimitiveIndex,
			INDEX_NONE,
			OutputObjectIdentifier.PointIndex,
			PropertyAttributes))
		{
			auto FindPropertyOnSourceModelLamba = [LODIndex](UObject* const InObject, const FString& InPropertyName, bool& bOutSkipDefaultIfPropertyNotFound, FEditPropertyChain& InPropertyChain, FProperty*& OutFoundProperty, UObject*& OutFoundPropertyObject, void*& OutContainer)
			{
				if (!IsValid(InObject))
					return false;
				
				UStaticMesh* const SM = Cast<UStaticMesh>(InObject);
				if (!IsValid(SM))
					return false;

				return TryToFindPropertyOnSourceModel(
					SM, LODIndex, InPropertyName, InPropertyChain, bOutSkipDefaultIfPropertyNotFound, OutFoundProperty, OutFoundPropertyObject, OutContainer);
			};

			// Defer post edit change calls until after all property values have been set, since the static mesh
			// build function is called from PostEditChangeProperty.
			constexpr bool bDeferPostEditChangePropertyCalls = true;
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
				FoundStaticMesh, PropertyAttributes, 0, bDeferPostEditChangePropertyCalls, FindPropertyOnSourceModelLamba);
		}


		// Notify that we created a new Static Mesh if needed
		if(bNewStaticMeshCreated)
			FAssetRegistryModule::AssetCreated(FoundStaticMesh);

		// Add the Static mesh to the output maps and the build map if we haven't already
		if (FoundOutputObject)
		{
			FoundOutputObject->OutputObject = FoundStaticMesh;
			FoundOutputObject->bProxyIsCurrent = false;
			FoundOutputObject->bIsImplicit = false;
			OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
		}

		StaticMeshToBuild.FindOrAdd(OutputObjectIdentifier, FoundStaticMesh);

		if (bDoTiming)
		{
			HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Finished Split in %f seconds."), FPlatformTime::Seconds() - tick);
			tick = FPlatformTime::Seconds();
			HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Total Split time: %f seconds."), tick - split_tick);
		}
	}

	// Look if we only have colliders
	// If we do, we'll allow attaching sockets to the collider meshes
	bool bCollidersOnly = true;
	for (auto& Current : StaticMeshToBuild)
	{
		EHoudiniSplitType CurrentSplitType = GetSplitTypeFromSplitName(Current.Key.SplitIdentifier);
		if (CurrentSplitType == EHoudiniSplitType::LOD || CurrentSplitType == EHoudiniSplitType::Normal)
		{
			bCollidersOnly = false;
			break;
		}
	}

	FHoudiniScopedGlobalSilence ScopedGlobalSilence;
	for (auto& Current : StaticMeshToBuild)
	{		
		tick = FPlatformTime::Seconds();

		UStaticMesh* SM = Current.Value;
		if (!IsValid(SM))
			continue;
		
		const FHoudiniOutputObjectIdentifier& CurrentObjId = Current.Key;

		UBodySetup * BodySetup = SM->GetBodySetup();
		if (!BodySetup)
		{
			SM->CreateBodySetup();
			BodySetup = SM->GetBodySetup();
		}

		EHoudiniSplitType SplitType = GetSplitTypeFromSplitName(CurrentObjId.SplitIdentifier);

		// Handle the Static Mesh's colliders
		if (IsValid(BodySetup))
		{
			// Make sure rendering is done - so we are not changing data being used by collision drawing.
			FlushRenderingCommands();

			// Clean up old colliders from a previous cook
			BodySetup->Modify();
			BodySetup->RemoveSimpleCollision();
			// Create new GUID
			BodySetup->InvalidatePhysicsData();

			FKAggregateGeom* CurrentAggColl = AllAggregateCollisions.Find(CurrentObjId);
			if (CurrentAggColl && CurrentAggColl->GetElementCount() > 0)
			{
				BodySetup->AddCollisionFrom(*CurrentAggColl);
				BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
			}

			// Set physical material if present
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

			TArray<FString> AttributeValues;

			FHoudiniHapiAccessor Accessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL);
			Accessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_PRIM);
			AttributeInfo.tupleSize = 1;
			bool bSuccess = Accessor.GetAttributeData(AttributeInfo, AttributeValues);

			if (bSuccess && AttributeValues.Num() > 0)
			{
			    // Fetch the physics material name based off the first primitve attribute
			    auto& MaterialName = AttributeValues[0];
				if (!MaterialName.IsEmpty() && MaterialName != "None")
				{
					BodySetup->PhysMaterial = LoadObject<UPhysicalMaterial>(nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr);
					if (!BodySetup->PhysMaterial)
					{
						HOUDINI_LOG_HELPER(Error, TEXT("Physical Material not found: %s."), *MaterialName);
					}
				}
			}

			// Moved RefreshCollisionChange to after the SM->Build call
			// RefreshCollisionChange(*SM);
			// SM->bCustomizedCollision = true;

			// See if we need to enable collisions on the whole mesh
			if (SplitType == EHoudiniSplitType::InvisibleComplexCollider || SplitType == EHoudiniSplitType::RenderedComplexCollider)
			{
				// Complex collider, enable collisions for this static mesh.
				BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
				ApplyComplexColliderHelper(
					MainStaticMesh,
					SM,
					SplitType,
					bAssignedCustomCollisionMesh,
					OutputObjects.Find(CurrentObjId));
			}
			else
			{
				// TODO
				// if the LODForCollision uproperty attribute is set, we need to activate complex collision
				// on the static mesh for that lod to be picked up properly as a collider
				if ( FHoudiniEngineUtils::HapiCheckAttributeExists(	HGPO.GeoId, HGPO.PartId, 
					"unreal_uproperty_LODForCollision", HAPI_ATTROWNER_DETAIL))
				{
					BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
				}
			}
		}

		// Add the Sockets to the StaticMesh
		// We only add them to the main geo, or to the colliders if we only generate colliders
		bool bAddSocket = SplitType == EHoudiniSplitType::Normal ? true : bCollidersOnly ? true : false;
		if (bAddSocket)
		{
			if (!FHoudiniEngineUtils::AddMeshSocketsToStaticMesh(SM, HGPO.AllMeshSockets, true))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to import sockets for StaticMesh %s."), *(SM->GetName()));
			}
		}

		if (MainStaticMesh)
		{
			UBodySetup* MainBodySetup = MainStaticMesh->GetBodySetup();
			if (!IsValid(MainBodySetup))
			{
				MainStaticMesh->CreateBodySetup();
				MainBodySetup = MainStaticMesh->GetBodySetup();
			}

			check(MainBodySetup);
			// Set the main static mesh to whatever the final CTF should be.
			MainBodySetup->CollisionTraceFlag = MainStaticMeshCTF;
		}

		// Update property attributes on the SM
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			CurrentObjId.GeoId,
			CurrentObjId.PartId,
			true,
			CurrentObjId.PrimitiveIndex,
			INDEX_NONE,
			CurrentObjId.PointIndex,
			PropertyAttributes))
		{
			// Defer post edit change calls until after all property values have been set, since the static mesh
			// build function is called from PostEditChangeProperty.
			constexpr bool bDeferPostEditChangePropertyCalls = true;
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(SM, PropertyAttributes, 0, bDeferPostEditChangePropertyCalls);
		}

		if (bDoTiming)
		{
			HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Pre SM->Build() in %f seconds."), FPlatformTime::Seconds() - tick);
			tick = FPlatformTime::Seconds();
		}

		// BUILD the Static Mesh
		// bSilent doesnt add the Build Errors...
		double build_start = FPlatformTime::Seconds();
		TArray<FText> SMBuildErrors;
		SM->Build(true, &SMBuildErrors);		

		if (bDoTiming)
		{			
			tick = FPlatformTime::Seconds();
			HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - StaticMesh->Build() executed in %f seconds."), tick - build_start);
		}

		// This replaces the call to RefreshCollision below, but without CreateNavCollision
		// as it is already called by UStaticMesh::PostBuildInternal as part of the ::Build call,
		// and can be expensive depending on the vert/poly count of the mesh
		// RefreshCollisionChange(*SM);
		{
			for (FThreadSafeObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
			{
				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(*Iter);
				if (StaticMeshComponent->GetStaticMesh() == SM)
				{
					// it needs to recreate IF it already has been created
					if (StaticMeshComponent->IsPhysicsStateCreated())
					{
						StaticMeshComponent->RecreatePhysicsState();
					}
				}
			}

			FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		}

		SM->GetOnMeshChanged().Broadcast();

		UPackage* MeshPackage = SM->GetOutermost();
		if (IsValid(MeshPackage))
		{
			MeshPackage->MarkPackageDirty();
		}

		if (bDoTiming)
		{
			HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Post SM->Build() in %f seconds."), FPlatformTime::Seconds() - tick);
		}
	}

	// !!! No need to call InvalidatePhysicsData / CreatePhysicsMeshes / GetNavCollision()->Setup
	// Here as it has already been handled by the StaticMesh Build call
	
	double time_end = FPlatformTime::Seconds();
	HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() executed in %f seconds."), time_end - time_start);

	return true;
}

bool
FHoudiniMeshTranslator::CreateHoudiniStaticMesh()
{
	///////////////////////////////////////////////////////////////////////
	// THIS FUNCTION IS DEPRECATED AND WILL REMOVED IN THE NEXT RELEASE.
	///////////////////////////////////////////////////////////////////////

	// Time limit for processing

	bDoTiming = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh);

	const double time_start = FPlatformTime::Seconds();

	// Keep a copy of the initial package params, since PackageParams is modified in place when resolving attributes
	FHoudiniPackageParams InitialPackageParams = PackageParams;

	bool bIsGammaCorrectionDisabled = IsGammaCorrectionDisabled(HGPO.GeoId, HGPO.PartId);

	// Start by updating the vertex list
	if (!UpdatePartVertexList())
		return false;

	// Sort the split groups
	SortSplitGroups();

	// Handles the split groups found in the part
	// and builds the corresponding faces and indices arrays
	if (!UpdateSplitsFacesAndIndices())
		return true;

	// Resets the containers used for the raw data extraction.
	ResetPartCache();

	// Determine if there is "main" geo, if not we'll use the first LOD
	// as main geo
	bool bHasMainGeo = false;
	for (auto& curSplit : AllSplitGroups)
	{
		if (GetSplitTypeFromSplitName(curSplit) == EHoudiniSplitType::Normal)
		{
			bHasMainGeo = true;
			break;
		}
	}

	// Update the part's material's IDS and info now
	//UpdatePartFaceMaterialsIfNeeded();
	CreateNeededMaterials();

	// Check if the materials were updated
	bool bMaterialHasChanged = false;
	for (const auto& MatInfo : PartUniqueMaterialInfos)
	{
		if (MatInfo.hasChanged)
		{
			bMaterialHasChanged = true;
			break;
		}
	}

	// Map of Houdini Material IDs to Unreal Material Interface
	TMap<HAPI_NodeId, UMaterialInterface*> MapHoudiniMatIdToUnrealInterface;
	// Map of Houdini Material Attributes to Unreal Material Interface
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> MapHoudiniMatAttributesToUnrealInterface;
	// Map of Unreal Material Interface to Unreal Material Index, per visible mesh
	TMap<UHoudiniStaticMesh*, TMap<UMaterialInterface*, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	// bool MeshMaterialsHaveBeenReset = false;

	double tick = FPlatformTime::Seconds();
	if(bDoTiming)
		HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh() - Pre Split-Loop in %f seconds."), tick - time_start);

	// Map of object identifiers to package params
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniPackageParams> ObjectIdentifiersToPackageParams;

	// Iterate through all detected split groups we care about and split geometry.
	bool bMainGeoOrFirstLODFound = false;
	for (int32 SplitId = 0; SplitId < AllSplitGroups.Num(); SplitId++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Per Split);

		// Get split group name
		const FString& SplitGroupName = AllSplitGroups[SplitId];

		// Get the current split type
		EHoudiniSplitType SplitType = GetSplitTypeFromSplitName(SplitGroupName);
		if (SplitType == EHoudiniSplitType::Invalid)
		{
			// Invalid split, skip
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] unknown split type.")
				TEXT("- skipping."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			continue;
		}

		// We are only interested in the Normal/main geo and visible colliders
		if (SplitType != EHoudiniSplitType::Normal &&
			SplitType != EHoudiniSplitType::LOD &&
			SplitType != EHoudiniSplitType::RenderedComplexCollider &&
			SplitType != EHoudiniSplitType::RenderedSimpleCollider &&
			SplitType != EHoudiniSplitType::RenderedUCXCollider)
		{
			// Only create the invisible colliders if we don't have other splits
			if(AllSplitGroups.Num() > 1)
				continue;
		}

		// We only use LOD if there is no Normal geo
		if (SplitType == EHoudiniSplitType::Normal)
		{
			bMainGeoOrFirstLODFound = true;
			HOUDINI_LOG_MESSAGE(TEXT("Found Normal geo for mesh."));
		}
		else if (SplitType == EHoudiniSplitType::LOD)
		{
			if (bHasMainGeo)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Skipping LOD since the mesh has Normal geo."));
				continue;
			}
			else if (bMainGeoOrFirstLODFound)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Skipping LOD since we have already processed the first LOD."));
				continue;
			}
			else
			{
				bMainGeoOrFirstLODFound = true;
				HOUDINI_LOG_MESSAGE(TEXT("Mesh does not have Normal geo, found first LOD."));
			}
		}

		// Get the vertex indices for this group
		TArray<int32>& SplitVertexList = AllSplitVertexLists[SplitGroupName];

		// Get valid count of vertex indices for this split.
		const int32& SplitVertexCount = AllSplitVertexCounts[SplitGroupName];

		// Make sure we have a valid vertex count for this split
		if (SplitVertexCount % 3 != 0 || SplitVertexList.Num() % 3 != 0)
		{
			// Invalid vertex count, skip this split or we'd crash trying to create a mesh for it.
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] invalid vertex count.")
				TEXT("- skipping."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);

			continue;
		}

		// Get the output identifer for this split
		FHoudiniOutputObjectIdentifier OutputObjectIdentifier = MakeOutputObjectIdentifier(SplitGroupName, SplitType);

		// Try to find existing properties for this identifier
		FHoudiniOutputObject* FoundOutputObject = InputObjects.Find(OutputObjectIdentifier);

		// If we don't yet have package params for this object identifier, fetch and resolve attributes for the split
		// and update the package params
		TMap<FString, FString> TempAttributes;
		TMap<FString, FString> TempTokens;
		bool bCopyAttributesAndTokens = false;
		if (!ObjectIdentifiersToPackageParams.Contains(OutputObjectIdentifier))
		{
			// Get all the supported attributes from the HGPO
			CopyAttributesFromHGPOForSplit(OutputObjectIdentifier, TempAttributes, TempTokens);

			// Resolve our final package params
			FHoudiniAttributeResolver Resolver;
			FHoudiniPackageParams FinalPackageParams;
			FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
				InitialPackageParams,
				IsValid(OuterComponent) ? OuterComponent->GetWorld() : nullptr,
				OuterComponent,
				TempAttributes,
				TempTokens,
				PackageParams,
				Resolver);

			bCopyAttributesAndTokens = true;
			ObjectIdentifiersToPackageParams.Emplace(OutputObjectIdentifier, PackageParams);
		}
		else
		{
			PackageParams = ObjectIdentifiersToPackageParams.FindChecked(OutputObjectIdentifier);
		}

		// Try to find an existing DM from a previous cook
		UHoudiniStaticMesh* FoundStaticMesh = FindExistingHoudiniStaticMesh(OutputObjectIdentifier);

		// Flag whether or not we need to rebuild the mesh
		bool bRebuildStaticMesh = false;
		if (HGPO.GeoInfo.bHasGeoChanged || HGPO.PartInfo.bHasChanged || ForceRebuild || !FoundStaticMesh || !FoundOutputObject)
			bRebuildStaticMesh = true;

		// TODO: Handle materials
		if (!bRebuildStaticMesh && !bMaterialHasChanged)
		{
			// We can simply reuse the found static mesh
			OutputObjects.Add(OutputObjectIdentifier, *FoundOutputObject);
			continue;
		}

		bool bNewStaticMeshCreated = false;
		if (!FoundStaticMesh)
		{
			// If we couldn't find a valid existing dynamic mesh, create a new one
			FoundStaticMesh = CreateNewHoudiniStaticMesh(OutputObjectIdentifier.SplitIdentifier);
			if (!IsValid(FoundStaticMesh))
				continue;

			bNewStaticMeshCreated = true;
		}

		if (!FoundOutputObject)
		{
			// If we couldnt find a previous output object, create a new one
			FHoudiniOutputObject NewOutputObject;
			FoundOutputObject = &OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
		}
		FoundOutputObject->bProxyIsCurrent = true;

		// Update the attributes and tokens if this is the first split for this object identifier
		if (bCopyAttributesAndTokens)
		{
			FoundOutputObject->CachedAttributes = MoveTemp(TempAttributes);
			FoundOutputObject->CachedTokens = MoveTemp(TempTokens);
		}

		if (bDoTiming)
		{
			HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh() - PreBuildMesh in %f seconds."), FPlatformTime::Seconds() - tick);
			tick = FPlatformTime::Seconds();
		}

		if (bRebuildStaticMesh)
		{
			//--------------------------------------------------------------------------------------------------------------------- 
			//  INDICES
			//--------------------------------------------------------------------------------------------------------------------- 

			//
			// Because of the splits, we don't need to declare all the vertices in the Part, 
			// but only the one that are currently used by the split's faces.
			// The indicesMapper array is used to map those indices from Part Vertices to Split Vertices.
			// We also keep track of the needed vertices index to declare them easily afterwards.
			//

			// IndicesMapper:
			// Maps index values for all vertices in the Part:
			// - Vertices unused by the split will be set to -1
			// - Used vertices will have their value set to the "NewIndex"
			// So that IndicesMapper[ oldIndex ] => newIndex
			TArray<int32> IndicesMapper;
			IndicesMapper.SetNumUninitialized(SplitVertexList.Num());
			for (int32 n = 0; n < IndicesMapper.Num(); n++)
				IndicesMapper[n] = -1;

			int32 CurrentMapperIndex = 0;

			// NeededVertices:
			// Array containing the old index of the needed vertices for the current split
			// NeededVertices[ newIndex ] => oldIndex
			TArray< int32 > NeededVertices;
			NeededVertices.Reserve(SplitVertexList.Num() / 3);
			TArray< int32 > TriangleIndices;
			TriangleIndices.Reserve(SplitVertexList.Num());

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Build IndicesMapper and NeededVertices);

				bool bHasInvalidFaceIndices = false;
				int32 ValidVertexId = 0;
				for (int32 VertexIdx = 0; VertexIdx < SplitVertexList.Num(); VertexIdx += 3)
				{
					int32 WedgeCheck = SplitVertexList[VertexIdx + 0];
					if (WedgeCheck == -1)
						continue;

					int32 WedgeIndices[3] =
					{
						SplitVertexList[VertexIdx + 0],
						SplitVertexList[VertexIdx + 1],
						SplitVertexList[VertexIdx + 2]
					};

					// Ensure the indices are valid
					if (!IndicesMapper.IsValidIndex(WedgeIndices[0])
						|| !IndicesMapper.IsValidIndex(WedgeIndices[1])
						|| !IndicesMapper.IsValidIndex(WedgeIndices[2]))
					{
						// Invalid face index. Don't log in the loop.
						bHasInvalidFaceIndices = true;
						continue;
					}

					// Converting Old (Part) Indices to New (Split) Indices:
					for (int32 i = 0; i < 3; i++)
					{
						if (IndicesMapper[WedgeIndices[i]] < 0)
						{
							// This old index has not yet been "converted" to a new index
							NeededVertices.Add(WedgeIndices[i]);
							IndicesMapper[WedgeIndices[i]] = CurrentMapperIndex;
							CurrentMapperIndex++;
						}

						// Replace the old index with the new one
						WedgeIndices[i] = IndicesMapper[WedgeIndices[i]];
					}

					// Flip wedge indices to fix the winding order.
					TriangleIndices.Add(WedgeIndices[0]);
					TriangleIndices.Add(WedgeIndices[2]);
					TriangleIndices.Add(WedgeIndices[1]);

					ValidVertexId += 3;
				}

				if (bHasInvalidFaceIndices)
				{
					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Dynamic Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] has some invalid face indices"),
						HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
				}
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// NORMALS 
			//--------------------------------------------------------------------------------------------------------------------- 

			// Extract this part's normal if needed
			UpdatePartNormalsIfNeeded();

			// Get the normals for this split
			TArray<float> SplitNormals;
			FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
				SplitVertexList, AttribInfoNormals, PartNormals, SplitNormals);

			// Check that the number of normal we retrieved is correct
			int32 NormalCount = SplitNormals.Num() / 3;
			if (NormalCount < 0 || NormalCount < NeededVertices.Num())
			{
				// Ignore normals
				NormalCount = 0;
				if (SplitNormals.Num() != 0)
					HOUDINI_LOG_WARNING(TEXT("Invalid normal count detected - Skipping normals."));
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// TANGENTS
			//--------------------------------------------------------------------------------------------------------------------- 

			TArray<float> SplitTangentU;
			TArray<float> SplitTangentV;
			int32 TangentUCount = 0;
			int32 TangentVCount = 0;
			// No need to read the tangents if we want unreal to recompute them after		
			const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
			bool bReadTangents = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->RecomputeTangentsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always : true;

			bool bGenerateTangentsFromNormalAttribute = false;
			if (bReadTangents)
			{
				// Extract this part's Tangents if needed
				UpdatePartTangentsIfNeeded();

				// Get the Tangents for this split
				FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
					SplitVertexList, AttribInfoTangentU, PartTangentU, SplitTangentU);

				// Get the binormals for this split
				FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
					SplitVertexList, AttribInfoTangentV, PartTangentV, SplitTangentV);

				if ((SplitTangentU.Num() <= 0 || SplitTangentV.Num() <= 0))
					bReadTangents = false;

				// We need to manually generate tangents if:
				// - we have normals but dont have tangentu or tangentv attributes
				// - we have not specified that we wanted unreal to generate them
				bGenerateTangentsFromNormalAttribute = (NormalCount > 0) && !bReadTangents;

				// Check that the number of tangents read matches the number of normals
				TangentUCount = SplitTangentU.Num() / 3;
				TangentVCount = SplitTangentV.Num() / 3;
				if (NormalCount > 0 && (TangentUCount != NormalCount || TangentVCount != NormalCount))
				{
					HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh: Generate tangents due to count mismatch (# U Tangents = %d; # V Tangents = %d; # Normals = %d)"), TangentUCount, TangentVCount, NormalCount);
					bGenerateTangentsFromNormalAttribute = true;
					bReadTangents = false;
				}

				if (bGenerateTangentsFromNormalAttribute && (HoudiniRuntimeSettings->RecomputeTangentsFlag == EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always))
				{
					// No need to generate tangents if we want unreal to recompute them after
					bGenerateTangentsFromNormalAttribute = false;
				}
			}
			else
			{
				bGenerateTangentsFromNormalAttribute = (NormalCount > 0);
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			//  VERTEX COLORS AND ALPHAS
			//---------------------------------------------------------------------------------------------------------------------

			// Extract this part's colors if needed
			UpdatePartColorsIfNeeded();

			// Get the colors values for this split
			TArray<float> SplitColors;
			FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
				SplitVertexList, AttribInfoColors, PartColors, SplitColors);

			// Extract this part's alpha values if needed
			UpdatePartAlphasIfNeeded();

			// Get the colors values for this split
			TArray<float> SplitAlphas;
			FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
				SplitVertexList, AttribInfoAlpha, PartAlphas, SplitAlphas);

			const int32 ColorsCount = AttribInfoColors.exists ? SplitColors.Num() / AttribInfoColors.tupleSize : 0;
			const bool bSplitColorValid = AttribInfoColors.exists && (AttribInfoColors.tupleSize >= 3) && ColorsCount > 0;
			const bool bSplitAlphaValid = AttribInfoAlpha.exists && (SplitAlphas.Num() == ColorsCount);

			//--------------------------------------------------------------------------------------------------------------------- 
			//  UVS
			//--------------------------------------------------------------------------------------------------------------------- 

			// Extract this part's UV sets if needed
			UpdatePartUVSetsIfNeeded();

			// See if we need to transfer uv point attributes to vertex attributes.
			int32 NumUVLayers = 0;
			TArray<TArray<float>> SplitUVSets;
			SplitUVSets.SetNum(MAX_STATIC_TEXCOORDS);
			for (int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
			{
				FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
					SplitVertexList, AttribInfoUVSets[TexCoordIdx], PartUVSets[TexCoordIdx], SplitUVSets[TexCoordIdx]);
				if (SplitUVSets[TexCoordIdx].Num() > 0)
				{
					NumUVLayers++;
				}
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// MATERIAL ATTRIBUTE OVERRIDES
			//---------------------------------------------------------------------------------------------------------------------

			// TODO: These are actually per faces, not per vertices...
			// Need to update!!
			UpdatePartFaceMaterialOverridesIfNeeded();

			//
			// Initialize mesh
			// 
			const int32 NumVertexPositions = NeededVertices.Num();
			const int32 NumTriangles = TriangleIndices.Num() / 3;
			const bool bHasPerFaceMaterials = PartFaceMaterialOverrides.Num() > 0 || (PartUniqueMaterialIds.Num() > 0 && !bOnlyOneFaceMaterial);

			FoundStaticMesh->Initialize(
				NumVertexPositions,
				NumTriangles,
				NumUVLayers,											   // NumUVLayers
				0,														   // InitialNumStaticMaterials
				NormalCount > 0,										   // HasNormals
				bReadTangents || bGenerateTangentsFromNormalAttribute,	   // HasTangents
				bSplitColorValid,										   // HasColors
				bHasPerFaceMaterials									   // HasPerFaceMaterials
			);

			//--------------------------------------------------------------------------------------------------------------------- 
			// POSITIONS
			//--------------------------------------------------------------------------------------------------------------------- 
			UpdatePartPositionIfNeeded();

			//
			// Transfer vertex positions:
			//
			// Because of the split, we're only interested in the needed vertices.
			// Instead of declaring all the Positions, we'll only declare the vertices
			// needed by the current split.
			//
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Vertex Positions);

				bool bHasInvalidPositionIndexData = false;
				for (int32 VertexPositionIdx = 0; VertexPositionIdx < NumVertexPositions; ++VertexPositionIdx)
				//ParallelFor(NumVertexPositions, [&](uint32 VertexPositionIdx)
				{
					int32 NeededVertexIndex = NeededVertices[VertexPositionIdx];
					if (!PartPositions.IsValidIndex(NeededVertexIndex * 3 + 2))
					{
						// Error retrieving positions.
						bHasInvalidPositionIndexData = true;
						continue;
					}

					// We need to swap Z and Y coordinate here, and convert from m to cm. 
					FoundStaticMesh->SetVertexPosition(VertexPositionIdx, FVector3f(
						PartPositions[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
						PartPositions[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
						PartPositions[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION
					));
				}//);

				if (bHasInvalidPositionIndexData)
				{
					HOUDINI_LOG_WARNING(
						TEXT("Creating Dynamic Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] invalid position/index data ")
						TEXT("- skipping."),
						HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
				}
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// FACES / TRIS
			// Now set Normals, UVs and Colors on mesh points and AttributeSet
			//---------------------------------------------------------------------------------------------------------------------

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Triangle Indices & Per Vertex Instance Attribute Values);

				// Now add the triangles to the mesh
				for (int32 TriangleIdx = 0; TriangleIdx < NumTriangles; ++TriangleIdx)
				// ParallelFor(NumTriangles, [&](uint32 TriangleIdx)
				{
					// TODO: add some additional intermediate consts for index calculations to make the indexing
					// TODO: code a bit more readable
					const int32 TriVertIdx0 = TriangleIdx * 3;
					FoundStaticMesh->SetTriangleVertexIndices(TriangleIdx, FIntVector(
						TriangleIndices[TriVertIdx0 + 0],
						TriangleIndices[TriVertIdx0 + 1],
						TriangleIndices[TriVertIdx0 + 2]
					));

					const int32 TriWindingIndex[3] = { 0, 2, 1 };
					// Normals and tangents (either getting tangents from attributes or generating tangents from the
					// normals
					if (NormalCount > 0 || bReadTangents)
					{
						for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
						{
							const bool bHasNormal = (NormalCount > 0 && SplitNormals.IsValidIndex(TriVertIdx0 * 3 + 3 * 3 - 1));
							FVector3f Normal = FVector3f::ZeroVector;
							if (bHasNormal)
							{
								// Flip Z and Y coordinate for normal, but don't scale
								Normal.Set(
									SplitNormals[TriVertIdx0 * 3 + 3 * ElementIdx + 0],
									SplitNormals[TriVertIdx0 * 3 + 3 * ElementIdx + 2],
									SplitNormals[TriVertIdx0 * 3 + 3 * ElementIdx + 1]
								);

								FoundStaticMesh->SetTriangleVertexNormal(TriangleIdx, TriWindingIndex[ElementIdx], Normal);
							}

							if (bReadTangents || bGenerateTangentsFromNormalAttribute)
							{
								FVector3f TangentU, TangentV;
								if (bGenerateTangentsFromNormalAttribute)
								{
									if (bHasNormal)
									{
										// Generate the tangents if needed
										Normal.FindBestAxisVectors(TangentU, TangentV);

										FoundStaticMesh->SetTriangleVertexUTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentU);
										FoundStaticMesh->SetTriangleVertexVTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentV);
									}
								}
								else
								{
									// Transfer the tangents from Houdini
									TangentU.X = SplitTangentU[TriVertIdx0 * 3 + 3 * ElementIdx + 0];
									TangentU.Y = SplitTangentU[TriVertIdx0 * 3 + 3 * ElementIdx + 2];
									TangentU.Z = SplitTangentU[TriVertIdx0 * 3 + 3 * ElementIdx + 1];

									TangentU.X = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 0];
									TangentU.Y = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 2];
									TangentU.Z = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 1];

									FoundStaticMesh->SetTriangleVertexUTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentU);
									FoundStaticMesh->SetTriangleVertexVTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentV);
								}
							}
						}
					}

					// Vertex Colors
					if (bSplitColorValid && SplitColors.IsValidIndex(TriVertIdx0 * AttribInfoColors.tupleSize + 3 * AttribInfoColors.tupleSize - 1))
					{
						FLinearColor VertexLinearColor;
						for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
						{
							VertexLinearColor.R = FMath::Clamp(
								SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 0], 0.0f, 1.0f);
							VertexLinearColor.G = FMath::Clamp(
								SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 1], 0.0f, 1.0f);
							VertexLinearColor.B = FMath::Clamp(
								SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 2], 0.0f, 1.0f);

							if (bSplitAlphaValid)
							{
								VertexLinearColor.A = FMath::Clamp(SplitAlphas[TriVertIdx0 + ElementIdx], 0.0f, 1.0f);
							}
							else if (AttribInfoColors.tupleSize >= 4)
							{
								VertexLinearColor.A = FMath::Clamp(
									SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 3], 0.0f, 1.0f);
							}
							else
							{
								VertexLinearColor.A = 1.0f;
							}

							FColor VertexColor = VertexLinearColor.ToFColor(false);

							// If Gamma correction is disabled, de-convert the color. Since SetTriangleVertexColor() will apply gamma.
							if (bIsGammaCorrectionDisabled)
								VertexColor = FLinearColor::FromSRGBColor(VertexColor).ToFColor(false);

							FoundStaticMesh->SetTriangleVertexColor(TriangleIdx, TriWindingIndex[ElementIdx], VertexColor);
						}
					}

					// UVs
					if (NumUVLayers > 0)
					{
						// Dynamic mesh supports only 1 UV layer on the mesh it self. So we set the first layer
						// on the mesh itself only, and we set all layers on the AttributeSet
						for (int32 TexCoordIdx = 0; TexCoordIdx < NumUVLayers; ++TexCoordIdx)
						{
							const TArray<float>& SplitUVs = SplitUVSets[TexCoordIdx];
							if (SplitUVs.IsValidIndex(TriVertIdx0 * 2 + 3 * 2 - 1))
							{
								for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
								{
									const int32 UVIdx = TriVertIdx0 * 2 + ElementIdx * 2;
									// We need to flip V coordinate when it's coming from HAPI.
									const FVector2f UV(SplitUVs[UVIdx + 0], 1.0f - SplitUVs[UVIdx + 1]);
									// Set the UV on the vertex instance in the UVLayer
									FoundStaticMesh->SetTriangleVertexUV(TriangleIdx, TriWindingIndex[ElementIdx], TexCoordIdx, UV);
								}
							}
						}
					}
				}
			}

			FMeshBuildSettings BuildSettings;
			UpdateMeshBuildSettings(
				BuildSettings,
				FoundStaticMesh->HasNormals(),
				FoundStaticMesh->HasTangents(),
				false);
			// Compute normals if requested or needed/missing
			if (BuildSettings.bRecomputeNormals)
			{
				FoundStaticMesh->CalculateNormals(BuildSettings.bComputeWeightedNormals);
			}

			// Compute tangents if requested or needed/missing
			if (BuildSettings.bRecomputeTangents)
			{
				FoundStaticMesh->CalculateTangents(BuildSettings.bComputeWeightedNormals);
			}
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// MATERIALS / FACE MATERIALS
		//---------------------------------------------------------------------------------------------------------------------

		// Get face indices for this split.
		TArray<int32>& SplitFaceIndices = AllSplitFaceIndices[SplitGroupName];

		// Fetch the FoundMesh's Static Materials array
		TArray<FStaticMaterial>& FoundStaticMaterials = FoundStaticMesh->GetStaticMaterials();

		// Clear the materials array of the mesh the first time we encounter it
		if (!MapUnrealMaterialInterfaceToUnrealIndexPerMesh.Contains(FoundStaticMesh))
		{
			FoundStaticMaterials.Empty();
		}
		TMap<UMaterialInterface*, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

		// Process material overrides first
		if (PartFaceMaterialOverrides.Num() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Per Face Material Overrides);

			// Array used to avoid constantly attempting to load invalid materials
			TArray<FHoudiniMaterialIdentifier> InvalidMaterials;

			for (int32 FaceIdx = 0; FaceIdx < SplitFaceIndices.Num(); ++FaceIdx)
			{
				int32 SplitFaceIndex = SplitFaceIndices[FaceIdx];
				if (!PartFaceMaterialOverrides.IsValidIndex(SplitFaceIndex))
					continue;

				UMaterialInterface * MaterialInterface = nullptr;
				int32 CurrentFaceMaterialIdx = 0;
				const FHoudiniMaterialInfo& MatInfo = PartFaceMaterialOverrides[SplitFaceIndex];
				const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
				UMaterialInterface** FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				if (!MaterialInterface)
				{
					// Try to locate the corresponding material interface

					// Start by looking in our assignment map
					FoundMaterialInterface = OutputAssignmentMaterials.Find(MaterialIdentifier);
					if (FoundMaterialInterface)
						MaterialInterface = *FoundMaterialInterface;

					// Only try to load a material if it has a chance to be valid!
					if (!MaterialInterface && MaterialIdentifier.IsValid() && !InvalidMaterials.Contains(MaterialIdentifier))
					{
						MaterialInterface = Cast<UMaterialInterface>(
							StaticLoadObject(UMaterialInterface::StaticClass(),
								nullptr, *MaterialIdentifier.MaterialObjectPath, nullptr, LOAD_NoWarn, nullptr));

						if (!MaterialInterface)
							InvalidMaterials.Add(MaterialIdentifier);
					}

					if (MaterialInterface)
					{
						// We managed to load the UE4 material
						// Make sure this material is in the assignments before replacing it.
						OutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

						// See if we have a replacement material and use it on the mesh instead
						UMaterialInterface * const *ReplacementMaterialInterface = ReplacementMaterials.Find(MaterialIdentifier);
						if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
							MaterialInterface = *ReplacementMaterialInterface;

						// Add this material to the map
						MapHoudiniMatAttributesToUnrealInterface.Add(MaterialIdentifier, MaterialInterface);
					}
					else
					{
						// The Attribute Material and its replacement do not exist
						// See if we can fallback to the Houdini material assigned on the face

						// Get the unreal material corresponding to this houdini one
						HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

						// See if we have already treated that material
						FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
						if (FoundMaterialInterface)
							MaterialInterface = *FoundMaterialInterface;
						if (!MaterialInterface)
						{
							// If everything fails, we'll use the default material
							MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

							// We need to add this material to the map
							FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
							const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
							const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
							UMaterialInterface * const * FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
							if (FoundMaterial)
								MaterialInterface = *FoundMaterial;

							// See if we have a replacement material and use it on the mesh instead
							UMaterialInterface * const * ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
							if (ReplacementMaterial && *ReplacementMaterial)
								MaterialInterface = *ReplacementMaterial;

							// Map the Houdini ID to the unreal one
							MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
						}
					}
				}

				if (MaterialInterface)
				{
					CurrentFaceMaterialIdx = GetFaceMaterialIndex(MaterialInterface,
						MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
						MatInfo.MaterialIndex,
						FoundStaticMaterials);
					// Update the Face Material on the mesh
					FoundStaticMesh->SetTriangleMaterialID(FaceIdx, CurrentFaceMaterialIdx);
				}
			}
		}
		else if (PartUniqueMaterialIds.Num() > 0)
		{
			// The part has houdini materials
			if (bOnlyOneFaceMaterial)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Single Material);

				// Use default Houdini material if no valid material is assigned to any of the faces.
				UMaterialInterface * MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

				// Get id of this single material.
				FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
				const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, PartFaceMaterialIds[0], MaterialPathName);
				const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
				UMaterialInterface * const * FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
				if (FoundMaterial)
					MaterialInterface = *FoundMaterial;

				// See if we have a replacement material and use it on the mesh instead
				UMaterialInterface * const * ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
				if (ReplacementMaterial && *ReplacementMaterial)
					MaterialInterface = *ReplacementMaterial;

				FoundStaticMaterials.Empty();
				FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Per Face Materials);

				// We have multiple houdini materials
				// Get default Houdini material.
				UMaterial * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get();

				for (int32 FaceIdx = 0; FaceIdx < SplitFaceIndices.Num(); ++FaceIdx)
				{
					int32 SplitFaceIndex = SplitFaceIndices[FaceIdx];
					if (!PartFaceMaterialIds.IsValidIndex(SplitFaceIndex))
						continue;

					// Get material id for this face.
					HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

					// See if we have already treated that material
					UMaterialInterface** FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
					UMaterialInterface* MaterialInterface = nullptr;
					if (FoundMaterialInterface)
						MaterialInterface = *FoundMaterialInterface;

					if (MaterialInterface)
					{
						int32 const * FoundUnrealMatIndex = MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Find(MaterialInterface);
						if (FoundUnrealMatIndex)
						{
							// This material has been mapped already, just assign the mat index
							FoundStaticMesh->SetTriangleMaterialID(FaceIdx, *FoundUnrealMatIndex);
							continue;
						}
					}
					else
					{
						MaterialInterface = Cast<UMaterialInterface>(DefaultMaterial);

						FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
						const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
						const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
						UMaterialInterface * const * FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
						if (FoundMaterial)
							MaterialInterface = *FoundMaterial;

						// See if we have a replacement material and use it on the mesh instead
						UMaterialInterface * const * ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
						if (ReplacementMaterial && *ReplacementMaterial)
							MaterialInterface = *ReplacementMaterial;

						// Map the houdini ID to the unreal one
						MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
					}

					if (MaterialInterface)
					{
						// Add the material to the Static mesh
						int32 UnrealMatIndex = FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));

						// Map the houdini ID to the unreal one
						MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Add(MaterialInterface, UnrealMatIndex);
						
						// Update the face index
						FoundStaticMesh->SetTriangleMaterialID(FaceIdx, UnrealMatIndex);
					}
				}
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Default Material);
		
			// No materials were found, we need to use default Houdini material.
			UMaterialInterface * MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

			// See if we have a replacement material and use it on the mesh instead
			UMaterialInterface * const * ReplacementMaterial = ReplacementMaterials.Find(
				FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, ""));
			if (ReplacementMaterial && *ReplacementMaterial)
				MaterialInterface = *ReplacementMaterial;

			FoundStaticMaterials.Empty();
			FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
		}

		//// Update property attributes on the mesh
		//TArray<FHoudiniGenericAttribute> PropertyAttributes;
		//if (GetGenericPropertiesAttributes(
		//	HGPO.GeoId, HGPO.PartId,
		//	AllSplitFirstValidVertexIndex[SplitGroupName],
		//	AllSplitFirstValidPrimIndex[SplitGroupName],
		//	PropertyAttributes))
		//{
		//	UpdateGenericPropertiesAttributes(
		//		FoundStaticMesh, PropertyAttributes);
		//}

		FoundStaticMesh->Optimize();

		// Check if the mesh is valid (check all the counts (vertex, triangles, vertex instances, UVs etc) but skip
		// looping over each individual triangle vertex index to check if the value is valid).
		const bool bSkipVertexIndicesCheck = true;
		if (!FoundStaticMesh->IsValid(bSkipVertexIndicesCheck))
		{
			HOUDINI_LOG_WARNING(
				TEXT("[CreateHoudiniStaticMesh]: Invalid StaticMesh data for %s in cook output! Please check the log."),
				*FoundStaticMesh->GetName());
		}

		//// Try to find the outer package so we can dirty it up
		//if (FoundStaticMesh->GetOuter())
		//{
		//	FoundStaticMesh->GetOuter()->MarkPackageDirty();
		//}
		//else
		//{
		//	FoundStaticMesh->MarkPackageDirty();
		//}
		UPackage *MeshPackage = FoundStaticMesh->GetOutermost();
		if (IsValid(MeshPackage))
		{
			MeshPackage->MarkPackageDirty();
			
			/*
			// DPT: deactivated auto saving mesh/material package
			// only dirty for now, as we'll save them when saving the world.
			// Save the created/updated package
			FEditorFileUtils::PromptForCheckoutAndSave({ MeshPackage }, false, false);
			*/
		}

		// Add the Proxy mesh to the output maps
		if (FoundOutputObject)
		{
			FoundOutputObject->ProxyObject = FoundStaticMesh;
			FoundOutputObject->bProxyIsCurrent = true;
			OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
		}
	}

	const double time_end = FPlatformTime::Seconds();
	HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh() executed in %f seconds."), time_end - time_start);

	return true;
}

void
FHoudiniMeshTranslator::ApplyComplexColliderHelper(
	UStaticMesh* TargetStaticMesh,
	UStaticMesh* ComplexStaticMesh,
	const EHoudiniSplitType SplitType,
	bool& bAssignedCustomCollisionMesh,
	FHoudiniOutputObject* OutputObject)
{
	if (SplitType == EHoudiniSplitType::InvisibleComplexCollider && TargetStaticMesh)
	{
		if (!bAssignedCustomCollisionMesh)
		{
			bAssignedCustomCollisionMesh = true;
			TargetStaticMesh->ComplexCollisionMesh = ComplexStaticMesh;
			TargetStaticMesh->bCustomizedCollision = true;
			bAssignedCustomCollisionMesh = true;
			// We don't want an actor/component for this object in the scene, so flag it as an implicit output.
			if (OutputObject)
			{
				OutputObject->bIsImplicit = true;
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("More than one (invisible) complex collision mesh found. Static Mesh assets only support a single complex collision mesh. Creating additional collision geo as Static Mesh Components."));
		}
	}
}


bool
FHoudiniMeshTranslator::CreateNeededMaterials()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateNeededMaterials);

	UpdatePartNeededMaterials();

	// Update package params with resolved attributes
	TMap<FString, FString> Attributes;
	TMap<FString, FString> Tokens;
	FHoudiniAttributeResolver Resolver;
	FHoudiniPackageParams FinalPackageParams;

	// Get the attributes from normal geo, or first LOD if there is no normal geo. Fallback to use index 0.
	FString SplitToUse;
	for (const FString& SplitGroupName : AllSplitGroups)
	{
		const EHoudiniSplitType SplitType = GetSplitTypeFromSplitName(SplitGroupName);
		if (SplitType == EHoudiniSplitType::Normal)
		{
			SplitToUse = SplitGroupName;
			break;
		}
		else if (SplitType == EHoudiniSplitType::LOD && SplitToUse.IsEmpty())
		{
			SplitToUse = SplitGroupName;
			// don't break here since we might still find normal geo after the LOD splits
		}
	}

	if (!SplitToUse.IsEmpty())
	{
		CopyAttributesFromHGPOForSplit(SplitToUse, Attributes, Tokens);
	}
	else
	{
		CopyAttributesFromHGPOForSplit(HGPO, 0, 0, Attributes, Tokens);
	}
	
	FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
		PackageParams,
		IsValid(OuterComponent) ? OuterComponent->GetWorld() : nullptr,
		OuterComponent,
		Attributes,
		Tokens,
		FinalPackageParams,
		Resolver);
	
	TArray<UPackage*> MaterialAndTexturePackages;

	TArray<UMaterialInterface*> OutMaterialArray;

	FinalPackageParams.OverideEnabled = false;
	FHoudiniMaterialTranslator::CreateHoudiniMaterials(
		HGPO.AssetId,
		FinalPackageParams,
		PartUniqueMaterialIds,
		PartUniqueMaterialInfos,
		InputAssignmentMaterials,
		AllOutputMaterials,
		OutputAssignmentMaterials,
		OutMaterialArray,
		MaterialAndTexturePackages,
		false, 
		bTreatExistingMaterialsAsUpToDate);

	if (bMaterialOverrideNeedsCreateInstance && PartFaceMaterialOverrides.Num() > 0)
	{
		// Map containing unique face materials override attribute
		// and their first valid prim index
		// We create only one material instance per attribute

		FHoudiniMaterialTranslator::SortUniqueFaceMaterialOverridesAndCreateMaterialInstances(PartFaceMaterialOverrides, HGPO, PackageParams, MaterialAndTexturePackages,
			InputAssignmentMaterials, OutputAssignmentMaterials,
			false);
	}

	return true;
}

bool
FHoudiniMeshTranslator::ExtractMaterialIndex(FString& MaterialName, int32& MatIndex)
{
	// Attempt to extract an index
	if (MaterialName.StartsWith("["))
	{
		int32 End = MaterialName.Find("]");
		if (End != -1)
		{
			FString Fragment = MaterialName.Mid(1, End - 1);
			if (FCString::IsNumeric(*Fragment))
			{
				MatIndex = FCString::Atoi(*Fragment);
				MaterialName = MaterialName.Mid(End + 1);
			}
		}
	}
	// correct invalid index
	if (MatIndex < -1)
	{
		MaterialName = "[" + FString::FromInt(MatIndex) + "]" + MaterialName;
		MatIndex = -1;
	}

	return true;
}

int32
FHoudiniMeshTranslator::GetFaceMaterialIndex(UMaterialInterface* MaterialInterface,
	TMap<UMaterialInterface*, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
	int32 MatIndex,
	TArray<FStaticMaterial>& FoundStaticMaterials)
{
	int32 CurrentFaceMaterialIdx = 0;
	// only rely on our map if no index specified in the attribute value
	int32 const* FoundFaceMaterialIdx = MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Find(MaterialInterface);
	if (FoundFaceMaterialIdx && MatIndex == -1)
	{
		// We already know what material index to use for that override
		CurrentFaceMaterialIdx = *FoundFaceMaterialIdx;
	}
	else
	{
		// assign a default index for this mat
		if (MatIndex == -1)
		{
			CurrentFaceMaterialIdx = FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
			MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Add(MaterialInterface, CurrentFaceMaterialIdx);
		}
		// else, there is a specific index specified in the attrib value
		else
		{
			// add the mat to the mesh's material list if needed
			if (FoundStaticMaterials.Num() <= MatIndex || FoundStaticMaterials[MatIndex].MaterialInterface == nullptr)
			{
				FoundStaticMaterials.Reserve(MatIndex + 1);
				// pad with empty materials
				while (FoundStaticMaterials.Num() <= MatIndex)
				{
					FoundStaticMaterials.Add(FStaticMaterial());
				}
				FoundStaticMaterials[MatIndex] = FStaticMaterial(MaterialInterface);
			}
			CurrentFaceMaterialIdx = MatIndex;
		}
	}

	return CurrentFaceMaterialIdx;
}

FString
FHoudiniMeshTranslator::GetMeshIdentifierFromSplit(const FString& InSplitName, const EHoudiniSplitType& InSplitType)
{
	FString MeshIdentifier = TEXT("");
	switch (InSplitType)
	{
		case EHoudiniSplitType::Normal:
		case EHoudiniSplitType::LOD:
		case EHoudiniSplitType::InvisibleUCXCollider:
		case EHoudiniSplitType::InvisibleSimpleCollider:
			// LODs and Invisible simple colliders use the main mesh
			MeshIdentifier = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
			break;

		case EHoudiniSplitType::InvisibleComplexCollider:
		case EHoudiniSplitType::RenderedComplexCollider:
		case EHoudiniSplitType::RenderedUCXCollider:
		case EHoudiniSplitType::RenderedSimpleCollider:
			// Rendered colliders or invisible complex colliders have their own static mesh
			MeshIdentifier = InSplitName;
			break;

		default:
			break;
	}

	return MeshIdentifier;
}

UStaticMesh*
FHoudiniMeshTranslator::FindExistingStaticMesh(const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	// See if we already have an input object for that output identifier
	FHoudiniOutputObject const * FoundOutputObjectPtr = InputObjects.Find(InIdentifier);
	UStaticMesh* FoundStaticMesh = nullptr;
	if (FoundOutputObjectPtr)
	{
		// Make sure it's a valid static mesh
		FoundStaticMesh = Cast<UStaticMesh>(FoundOutputObjectPtr->OutputObject);
		if (!IsValid(FoundStaticMesh))
			FoundStaticMesh = nullptr;
	}

	if (!FoundStaticMesh)
	{
		// No input object matching this identifier, see if we have created an output object that matches
		FoundOutputObjectPtr = OutputObjects.Find(InIdentifier);
		if (!FoundOutputObjectPtr)
			return nullptr;

		// Make sure it's a valid static mesh
		FoundStaticMesh = Cast<UStaticMesh>(FoundOutputObjectPtr->OutputObject);
		if (!IsValid(FoundStaticMesh))
			return nullptr;
	}

	if (FoundStaticMesh)
	{
		UObject* OuterMost = FoundStaticMesh->GetOutermostObject();
		if (OuterMost->IsA<ULevel>())
		{
			// The Outermost for this static mesh is a level
			// This is likely a SM created by V1, and we should not reuse it.
			// This will force the plugin to recreate a "proper" SM in the temp folder.
			FoundStaticMesh->MarkAsGarbage();
			FoundStaticMesh = nullptr;
		}

		// Check that the existing mesh is in the expected directory (temp folder could have been changed between
		// cooks).
		if (!PackageParams.HasMatchingPackageDirectories(FoundStaticMesh))
		{
			FoundStaticMesh = nullptr;
		}
	}

	return FoundStaticMesh;
}

UHoudiniStaticMesh*
FHoudiniMeshTranslator::FindExistingHoudiniStaticMesh(const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	// See if we already have an input object for that output identifier
	FHoudiniOutputObject const * FoundOutputObjectPtr = InputObjects.Find(InIdentifier);
	UHoudiniStaticMesh* FoundStaticMesh = nullptr;
	if (FoundOutputObjectPtr)
	{
		// Make sure it's a valid static mesh
		FoundStaticMesh = Cast<UHoudiniStaticMesh>(FoundOutputObjectPtr->ProxyObject);
		if (!IsValid(FoundStaticMesh))
			FoundStaticMesh = nullptr;
	}

	if (!FoundStaticMesh)
	{
		// No input object matching this identifier, see if we have created an output object that matches
		FoundOutputObjectPtr = OutputObjects.Find(InIdentifier);
		if (!FoundOutputObjectPtr)
			return nullptr;

		// Make sure it's a valid static mesh
		FoundStaticMesh = Cast<UHoudiniStaticMesh>(FoundOutputObjectPtr->ProxyObject);
		if (!IsValid(FoundStaticMesh))
			return nullptr;
	}

	// Check that the existing mesh is in the expected directory (temp folder could have been changed between
	// cooks).
	if (!PackageParams.HasMatchingPackageDirectories(FoundStaticMesh))
	{
		FoundStaticMesh = nullptr;
	}

	return FoundStaticMesh;
}

EHoudiniSplitType
FHoudiniMeshTranslator::GetSplitTypeFromSplitName(const FString& InSplitName)
{
	const FString MainGroup = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
	if (InSplitName.StartsWith(MainGroup, ESearchCase::IgnoreCase))
		return EHoudiniSplitType::Normal;

	const FString LODGroupPrefix = HAPI_UNREAL_GROUP_LOD_PREFIX;
	if (InSplitName.StartsWith(LODGroupPrefix, ESearchCase::IgnoreCase))
	{
		return EHoudiniSplitType::LOD;
	}
		
	const FString RenderedCollisionPrefix = HAPI_UNREAL_GROUP_RENDERED_COLLISION_PREFIX;
	if (InSplitName.StartsWith(RenderedCollisionPrefix, ESearchCase::IgnoreCase))
	{
		// Rendered colliders
		// See if it is a simple/ucx/complex
		const FString RenderedUCXCollisionPrefix = HAPI_UNREAL_GROUP_RENDERED_UCX_COLLISION_PREFIX;
		const FString RenderedSimpleCollisionPrefix = HAPI_UNREAL_GROUP_RENDERED_SIMPLE_COLLISION_PREFIX;
		if (InSplitName.StartsWith(RenderedUCXCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::RenderedUCXCollider;
		}
		else if (InSplitName.StartsWith(RenderedSimpleCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::RenderedSimpleCollider;
		}
		else
		{
			return EHoudiniSplitType::RenderedComplexCollider;
		}
	}

	const FString InvisibleCollisionPrefix = HAPI_UNREAL_GROUP_INVISIBLE_COLLISION_PREFIX;
	if (InSplitName.StartsWith(InvisibleCollisionPrefix, ESearchCase::IgnoreCase))
	{
		// Invisible colliders
		// See if it is a simple/ucx/complex
		const FString InvisibleUCXCollisionPrefix = HAPI_UNREAL_GROUP_INVISIBLE_UCX_COLLISION_PREFIX;
		const FString InvisibleSimpleCollisionPrefix = HAPI_UNREAL_GROUP_INVISIBLE_SIMPLE_COLLISION_PREFIX;
		if (InSplitName.StartsWith(InvisibleUCXCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::InvisibleUCXCollider;
		}
		else if (InSplitName.StartsWith(InvisibleSimpleCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::InvisibleSimpleCollider;
		}
		else
		{
			return EHoudiniSplitType::InvisibleComplexCollider;
		}
	}

	// ?
	return EHoudiniSplitType::Invalid;
	//return EHoudiniSplitType::Normal;
}

bool
FHoudiniMeshTranslator::AddConvexCollisionToAggregate(const FString& SplitGroupName, FKAggregateGeom& AggCollisions)
{
	// Get the vertex indices for the split group
	TArray<int32>& SplitGroupVertexList = AllSplitVertexLists[SplitGroupName];

	// We're only interested in unique vertices
	TArray<int32> UniqueVertexIndexes;
	for (int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++)
	{
		int32 Index = SplitGroupVertexList[VertexIdx];
		if (!PartPositions.IsValidIndex(Index))
			continue;

		UniqueVertexIndexes.AddUnique(Index);
	}

	// Extract the collision geo's vertices
	TArray< FVector > VertexArray;
	VertexArray.SetNum(UniqueVertexIndexes.Num());
	for (int32 Idx = 0; Idx < UniqueVertexIndexes.Num(); Idx++)
	{
		int32 VertexIndex = UniqueVertexIndexes[Idx];
		if (!PartPositions.IsValidIndex(VertexIndex * 3 + 2))
			continue;

		VertexArray[Idx].X = PartPositions[VertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Idx].Y = PartPositions[VertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Idx].Z = PartPositions[VertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

#if WITH_EDITOR
	// Do we want to create multiple convex hulls?
	bool bDoMultiHullDecomp = false;
	if (SplitGroupName.Contains(TEXT("ucx_multi"), ESearchCase::IgnoreCase))
		bDoMultiHullDecomp = true;

	uint32 HullCount = 8;
	int32 MaxHullVerts = 16;
	if (bDoMultiHullDecomp)
	{
		// TODO:
		// Look for extra attributes for the decomposition parameters? (HullCount/MaxHullVerts)
	}

	if (bDoMultiHullDecomp && (VertexArray.Num() >= 3 || UniqueVertexIndexes.Num() >= 3))
	{
		// creating multiple convex hull collision
		// ... this might take a while

		// We're only interested in the valid indices!
		TArray<uint32> Indices;
		for (int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++)
		{
			int32 Index = SplitGroupVertexList[VertexIdx];
			if (!PartPositions.IsValidIndex(Index))
				continue;

			Indices.Add(Index);
		}

		// But we need all the positions as vertex
		TArray<FVector3f> Vertices;
		Vertices.SetNum(PartPositions.Num() / 3);

		for (int32 Idx = 0; Idx < Vertices.Num(); Idx++)
		{
			Vertices[Idx].X = PartPositions[Idx * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[Idx].Y = PartPositions[Idx * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[Idx].Z = PartPositions[Idx * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		// We are using Unreal's DecomposeMeshToHulls() 
		// We need a BodySetup so create a fake/transient one
		UBodySetup* BodySetup = NewObject<UBodySetup>();

		// Run actual util to do the work (if we have some valid input)
		DecomposeMeshToHulls(BodySetup, Vertices, Indices, HullCount, MaxHullVerts);

		// If we succeed, return here
		// If not, keep going and we'll try to do a single hull decomposition
		if (BodySetup->AggGeom.ConvexElems.Num() > 0)
		{
			// Copy the convex elem to our aggregate
			for (int32 n = 0; n < BodySetup->AggGeom.ConvexElems.Num(); n++)
				AggCollisions.ConvexElems.Add(BodySetup->AggGeom.ConvexElems[n]);

			return true;
		}
	}
#endif

	// Creating a single Convex collision
	FKConvexElem ConvexCollision;
	ConvexCollision.VertexData = VertexArray;
	ConvexCollision.UpdateElemBox();

	AggCollisions.ConvexElems.Add(ConvexCollision);

	return true;
}

TArray<FVector> FHoudiniMeshTranslator::GetKdopDirections(const FString& SplitGroupName)
{
	uint32 NumDirections = 26;
	const FVector* Directions = KDopDir26;
	if (SplitGroupName.Contains("kdop10X"))
	{
		NumDirections = 10;
		Directions = KDopDir10X;
	}
	else if (SplitGroupName.Contains("kdop10Y"))
	{
		NumDirections = 10;
		Directions = KDopDir10Y;
	}
	else if (SplitGroupName.Contains("kdop10Z"))
	{
		NumDirections = 10;
		Directions = KDopDir10Z;
	}
	else if (SplitGroupName.Contains("kdop18"))
	{
		NumDirections = 18;
		Directions = KDopDir18;
	}

	// Converting the directions to a TArray
	TArray<FVector> DirArray;
	DirArray.SetNum(NumDirections);
	for (uint32 DirectionIndex = 0; DirectionIndex < NumDirections; DirectionIndex++)
	{
		DirArray[DirectionIndex] = Directions[DirectionIndex];
	}
	return DirArray;
}

bool
FHoudiniMeshTranslator::AddSimpleCollisionToAggregate(const FString& SplitGroupName, FKAggregateGeom& AggCollisions)
{
	// Get the vertex indices for the split group
	TArray<int32>& SplitGroupVertexList = AllSplitVertexLists[SplitGroupName];

	// We're only interested in unique vertices
	TArray<int32> UniqueVertexIndexes;

	for (int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++)
	{
		int32 Index = SplitGroupVertexList[VertexIdx];
		if (!PartPositions.IsValidIndex(Index))
			continue;

		UniqueVertexIndexes.AddUnique(Index);
	}

	// Extract the collision geo's vertices
	TArray< FVector > VertexArray;
	VertexArray.SetNum(UniqueVertexIndexes.Num());
	for (int32 Idx = 0; Idx < UniqueVertexIndexes.Num(); Idx++)
	{
		int32 VertexIndex = UniqueVertexIndexes[Idx];
		if (!PartPositions.IsValidIndex(VertexIndex * 3 + 2))
			continue;

		VertexArray[Idx].X = PartPositions[VertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Idx].Y = PartPositions[VertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Idx].Z = PartPositions[VertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	int32 NewColliders = 0;
	if (SplitGroupName.Contains("Box"))
	{
		NewColliders = FHoudiniMeshTranslator::GenerateOrientedBoxAsSimpleCollision(VertexArray, AggCollisions);
	}
	else if (SplitGroupName.Contains("Sphere"))
	{
		NewColliders = FHoudiniMeshTranslator::GenerateSphereAsSimpleCollision(VertexArray, AggCollisions);
	}
	else if (SplitGroupName.Contains("Capsule"))
	{
		NewColliders = FHoudiniMeshTranslator::GenerateOrientedSphylAsSimpleCollision(VertexArray, AggCollisions);
	}
	else
	{
		TArray<FVector> DirArray = GetKdopDirections(SplitGroupName);

		NewColliders = FHoudiniMeshTranslator::GenerateKDopAsSimpleCollision(VertexArray, DirArray, AggCollisions);
	}

	return (NewColliders > 0);
}

int32
FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<float>& OutVertexData)
{
	return FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
		InVertexList, InAttribInfo,	InData,	OutVertexData);
}

/*
int32
FHoudiniMeshTranslator::GetSplitNormals(
	const TArray<int32>& InSplitVertexList, TArray<FVector>& OutNormals)
{
	// Extract the normals
	UpdatePartNormalsIfNeeded();

	// Get the normals for this split
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
		InSplitVertexList, AttribInfoNormals, PartNormals, VertexData);

	// Convert the float data to Vectors and fix the winding order
	OutNormals.SetNum(WedgeCount);
	for (int32 WedgeIdx = 0; WedgeIdx + 2 < WedgeCount; WedgeIdx += 3)
	{
		OutNormals[WedgeIdx].X = VertexData[WedgeIdx * 3 + 0];
		OutNormals[WedgeIdx].Y = VertexData[WedgeIdx * 3 + 2];
		OutNormals[WedgeIdx].Z = VertexData[WedgeIdx * 3 + 1];

		OutNormals[WedgeIdx + 2].X = VertexData[(WedgeIdx + 1) * 3 + 0];
		OutNormals[WedgeIdx + 2].Y = VertexData[(WedgeIdx + 1) * 3 + 2];
		OutNormals[WedgeIdx + 2].Z = VertexData[(WedgeIdx + 1) * 3 + 1];

		OutNormals[WedgeIdx + 1].X = VertexData[(WedgeIdx + 2) * 3 + 0];
		OutNormals[WedgeIdx + 1].Y = VertexData[(WedgeIdx + 2) * 3 + 2];
		OutNormals[WedgeIdx + 1].Z = VertexData[(WedgeIdx + 2) * 3 + 1];
	}

	return WedgeCount;
}

int32
FHoudiniMeshTranslator::GetSplitUVs(
	const TArray<int32>& InSplitVertexList, TArray<FVector2D>& OutUVs)
{
	// Extract the normals
	UpdatePartUVSetsIfNeeded();

	// Get the normals for this split
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
		InSplitVertexList, AttribInfoNormals, PartNormals, VertexData);

	// Convert the float data to Vectors and fix the winding order
	OutNormals.SetNum(WedgeCount);
	for (int32 WedgeIdx = 0; WedgeIdx + 2 < WedgeCount; WedgeIdx += 3)
	{
		OutNormals[WedgeIdx].X = VertexData[WedgeIdx * 3 + 0];
		OutNormals[WedgeIdx].Y = VertexData[WedgeIdx * 3 + 2];
		OutNormals[WedgeIdx].Z = VertexData[WedgeIdx * 3 + 1];

		OutNormals[WedgeIdx + 2].X = VertexData[(WedgeIdx + 1) * 3 + 0];
		OutNormals[WedgeIdx + 2].Y = VertexData[(WedgeIdx + 1) * 3 + 2];
		OutNormals[WedgeIdx + 2].Z = VertexData[(WedgeIdx + 1) * 3 + 1];

		OutNormals[WedgeIdx + 1].X = VertexData[(WedgeIdx + 2) * 3 + 0];
		OutNormals[WedgeIdx + 1].Y = VertexData[(WedgeIdx + 2) * 3 + 2];
		OutNormals[WedgeIdx + 1].Z = VertexData[(WedgeIdx + 2) * 3 + 1];
	}

	return WedgeCount;
}


int32
FHoudiniMeshTranslator::TransferPartAttributesToSplit(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<FVector2D>& OutData,
	const float& ScaleFactor)
{
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
		InVertexList, InAttribInfo, InData, VertexData);

	// Convert the float data to Vectors and fix the winding order
	OutData.SetNum(WedgeCount);
	for (int32 WedgeIdx = 0; WedgeIdx + 2 < WedgeCount; WedgeIdx += 3)
	{
		OutData[WedgeIdx].X = VertexData[WedgeIdx * 2 + 0];
		OutData[WedgeIdx].Y = VertexData[WedgeIdx * 2 + 1];

		OutData[WedgeIdx + 2].X = VertexData[(WedgeIdx + 1) * 2 + 0];
		OutData[WedgeIdx + 2].Y = VertexData[(WedgeIdx + 1) * 2 + 1];

		OutData[WedgeIdx + 1].X = VertexData[(WedgeIdx + 2) * 2 + 0];
		OutData[WedgeIdx + 1].Y = VertexData[(WedgeIdx + 2) * 2 + 1];
	}

	return WedgeCount;
}

int32
FHoudiniMeshTranslator::TransferPartAttributesToSplit(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<FVector2D>& OutData,
	const float& ScaleFactor)
{
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
		InVertexList, InAttribInfo, InData, VertexData);

	// Convert the float data to Vectors and fix the winding order
	OutData.SetNum(WedgeCount);
	for (int32 WedgeIdx = 0; WedgeIdx + 2 < WedgeCount; WedgeIdx += 3)
	{
		OutData[WedgeIdx].X = VertexData[WedgeIdx * 2 + 0];
		OutData[WedgeIdx].Y = VertexData[WedgeIdx * 2 + 1];

		OutData[WedgeIdx + 2].X = VertexData[(WedgeIdx + 1) * 2 + 0];
		OutData[WedgeIdx + 2].Y = VertexData[(WedgeIdx + 1) * 2 + 1];

		OutData[WedgeIdx + 1].X = VertexData[(WedgeIdx + 2) * 2 + 0];
		OutData[WedgeIdx + 1].Y = VertexData[(WedgeIdx + 2) * 2 + 1];
	}

	return WedgeCount;
}
*/


template <typename TYPE>
int32 FHoudiniMeshTranslator::TransferPartAttributesToSplit(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<TYPE>& InData,
	TArray<TYPE>& OutVertexData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::TransferPartAttributesToSplit);

	if (!InAttribInfo.exists || InAttribInfo.tupleSize <= 0)
		return 0;

	if (InData.Num() <= 0)
		return 0;

	int32 ValidWedgeCount = 0;

	// Future optimization - see if we can do direct vertex transfer.
	int32 WedgeCount = InVertexList.Num();
	int32 LastValidWedgeIdx = 0;
	if (InAttribInfo.owner == HAPI_ATTROWNER_POINT)
	{		
		// Point attribute transfer
		OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			int32 VertexIdx = InVertexList[WedgeIdx];
			if (VertexIdx < 0)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
			for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
			{
				OutVertexData[OutIdx + TupleIdx] = InData[VertexIdx * InAttribInfo.tupleSize + TupleIdx];
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;
		}
	}
	else if (InAttribInfo.owner == HAPI_ATTROWNER_VERTEX)
	{
		// Vertex attribute transfer
		OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			if (InVertexList[WedgeIdx] < 0)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
			for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
			{				
				OutVertexData[OutIdx + TupleIdx] = InData[WedgeIdx * InAttribInfo.tupleSize + TupleIdx];
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;
		}
	}
	else if (InAttribInfo.owner == HAPI_ATTROWNER_PRIM)
	{
		// Primitive attribute transfer
		OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			if (InVertexList[WedgeIdx] < 0)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			int32 PrimIdx = WedgeIdx / 3;
			int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
			for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
			{
				OutVertexData[OutIdx + TupleIdx] = InData[PrimIdx * InAttribInfo.tupleSize + TupleIdx];
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;
		}
	}
	else if (InAttribInfo.owner == HAPI_ATTROWNER_DETAIL)
	{
		// Detail attribute transfer
		// We have one value to copy for all output split vertices
		// if the attribute is a single value (not a tuple)
		// then we can simply use the array init function instead of looping
		if (InAttribInfo.tupleSize == 1)
		{
			OutVertexData.SetNumUninitialized(WedgeCount);
			for (int32 n = 0; n < OutVertexData.Num(); n++)
				OutVertexData[n] = InData[0];
		}
		else
		{
			OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
			for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
			{
				if (InVertexList[WedgeIdx] < 0)
				{
					// This is an index/wedge we are skipping due to split.
					continue;
				}

				int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
				for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
				{
					OutVertexData[OutIdx + TupleIdx] = InData[TupleIdx];
				}

				// We are re-indexing wedges.
				LastValidWedgeIdx++;
				// Increment wedge count, since this is a valid wedge.
				ValidWedgeCount++;
			}
		}
	}
	else
	{
		// Invalid attribute owner, shouldn't happen
		check(false);
	}

	OutVertexData.SetNumZeroed(ValidWedgeCount * InAttribInfo.tupleSize);

	return ValidWedgeCount;
}

bool
FHoudiniMeshTranslator::TryToFindPropertyOnSourceModel(
	UStaticMesh* const InStaticMesh,
	const int32 InSourceModelIndex,
	const FString& InPropertyName,
	FEditPropertyChain& InPropertyChain,
	bool& bOutSkipDefaultIfPropertyNotFound,
	FProperty*& OutFoundProperty,
	UObject*& OutFoundPropertyObject,
	void*& OutContainer
	)
{
	if (!IsValid(InStaticMesh))
		return false;

	if (!InStaticMesh->IsSourceModelValid(InSourceModelIndex))
		return false;

	FStaticMeshSourceModel& SourceModel = InStaticMesh->GetSourceModel(InSourceModelIndex);

	// Restrict to only applying properties to the source model -- if the property is not found on the
	// source model, don't search via the static mesh
	bOutSkipDefaultIfPropertyNotFound = true;
	FProperty* const SourceModelsProperty = InStaticMesh->GetClass()->FindPropertyByName(FName(TEXT("SourceModels")));
	if (SourceModelsProperty)
		InPropertyChain.AddTail(SourceModelsProperty);

	if (!TryToFindPropertyOnSourceModel(SourceModel, InPropertyName, InPropertyChain, OutFoundProperty, OutContainer))
		return false;

	OutFoundPropertyObject = InStaticMesh;
	return true;
}

bool
FHoudiniMeshTranslator::TryToFindPropertyOnSourceModel(
	FStaticMeshSourceModel& InSourceModel,
	const FString& InPropertyName,
	FEditPropertyChain& InPropertyChain,
	FProperty*& OutFoundProperty,
	void*& OutContainer)
{
	bool bFoundProperty = false;
	FHoudiniGenericAttribute::TryToFindProperty(
		&InSourceModel, InSourceModel.StaticStruct(), InPropertyName, InPropertyChain, OutFoundProperty, bFoundProperty, OutContainer, false);
	return bFoundProperty;
}

float
FHoudiniMeshTranslator::GetLODSCreensizeForSplit(const FString& SplitGroupName)
{
	// LOD Screensize
	// default values has already been set, see if we have any attribute override for this
	float screensize = -1.0f;

	// Start by looking at the lod_screensize primitive attribute
	bool bAttribValid = false;
	UpdatePartLODScreensizeIfNeeded();

	if (PartLODScreensize.Num() > 0)
	{
		// use the "lod_screensize" primitive attribute
		int32 FirstValidPrimIndex = AllSplitFirstValidPrimIndex[SplitGroupName];
		if (PartLODScreensize.IsValidIndex(FirstValidPrimIndex))
			screensize = PartLODScreensize[FirstValidPrimIndex];
	}
	
	if (screensize < 0.0f)
	{
		// We couldn't find the primitive attribute, look for a "lodX_screensize" detail attribute
		FString LODAttributeName = SplitGroupName + HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_POSTFIX;

		TArray<float> LODScreenSizes;

		FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, TCHAR_TO_ANSI(*LODAttributeName));
		bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, LODScreenSizes, 0, 1);

		if (bSuccess)
		{
			screensize = LODScreenSizes[0];
		}			
	}

	if (screensize < 0.0f)
	{
		// finally, look for a potential uproperty style attribute
		// aka, "unreal_uproperty_screensize"
		TArray<float> LODScreenSizes;
		HAPI_AttributeInfo AttribInfoScreenSize;
		FHoudiniApi::AttributeInfo_Init(&AttribInfoScreenSize);

		FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE);
		bool bSuccess = Accessor.GetInfo(AttribInfoScreenSize);

		if (bSuccess)
			Accessor.GetAttributeData(AttribInfoScreenSize, PartLODScreensize, 0, 1);

		if (bSuccess && AttribInfoScreenSize.exists)
		{
			if (AttribInfoScreenSize.owner == HAPI_ATTROWNER_DETAIL && LODScreenSizes.Num() > 0)
			{
				screensize = LODScreenSizes[0];
			}
			else if (AttribInfoScreenSize.owner == HAPI_ATTROWNER_PRIM)
			{
				int32 FirstValidPrimIndex = AllSplitFirstValidPrimIndex[SplitGroupName];
				if (LODScreenSizes.IsValidIndex(FirstValidPrimIndex))
					screensize = LODScreenSizes[FirstValidPrimIndex];
			}
		}
	}

	// Make sure the screensize is in percent, so if its above 1, divide by 100
	if (screensize > 1.0f)
		screensize /= 100.0f;

	return screensize;
}

int32 
FHoudiniMeshTranslator::GenerateBoxAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	// Calculate bounding Box.	
	FVector Center, Extents;
	FVector unitVec = FVector::OneVector;// bs->BuildScale3D;
	CalcBoundingBox(InPositionArray, Center, Extents, unitVec);

	FKBoxElem BoxElem;
	BoxElem.Center = Center;
	BoxElem.X = Extents.X * 2.0f;
	BoxElem.Y = Extents.Y * 2.0f;
	BoxElem.Z = Extents.Z * 2.0f;
	OutAggregateCollisions.BoxElems.Add(BoxElem);

	return 1;
}

int32 
FHoudiniMeshTranslator::GenerateOrientedBoxAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code adapted from Experimental GeometryProcessing plugin for simple mesh approximation.
	//

	const int32 NumPoints = InPositionArray.Num();
	TArray<houdini::gte::Vector3<double>> Points;
	Points.SetNumUninitialized(InPositionArray.Num());
	for(int32 i = 0; i < NumPoints; ++i)
	{
		Points[i] = Convert<double>(InPositionArray[i]);
	}
	// Calculate bounding Box.
	houdini::gte::OrientedBox3<double> MinimalBox = houdini::gte::OrientedBox3<double>();
	houdini::gte::MinimumVolumeBox3<double, double> BoxCompute;
	MinimalBox = BoxCompute(NumPoints, Points.GetData(), nullptr);
	
	// FVector unitVec = FVector::OneVector;// bs->BuildScale3D;
	// CalcBoundingBox(InPositionArray, Center, Extents, unitVec);
	
	const FVector X = Convert<double>(MinimalBox.axis[0]);
	const FVector Y = Convert<double>(MinimalBox.axis[1]);
	const FRotator Rot = FRotationMatrix::MakeFromXY(X,Y).Rotator();
	
	const FVector Extents = Convert(MinimalBox.extent);
	FKBoxElem BoxElem;
	BoxElem.Center = Convert(MinimalBox.center);
	BoxElem.X = Extents.X * 2.0f;
	BoxElem.Y = Extents.Y * 2.0f;
	BoxElem.Z = Extents.Z * 2.0f;
	BoxElem.Rotation = Rot;
	OutAggregateCollisions.BoxElems.Add(BoxElem);

	return 1;
}

void
FHoudiniMeshTranslator::CalcBoundingBox(const TArray<FVector>& PositionArray, FVector& Center, FVector& Extents, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FBox Box(ForceInit);
	for (const FVector& CurPos : PositionArray)
	{
		Box += CurPos;
	}
	Box.GetCenterAndExtents(Center, Extents);
}

int32
FHoudiniMeshTranslator::GenerateSphereAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FSphere bSphere, bSphere2, bestSphere;
	FVector unitVec = FVector::OneVector;

	// Calculate bounding sphere.
	CalcBoundingSphere(InPositionArray, bSphere, unitVec);
	CalcBoundingSphere2(InPositionArray, bSphere2, unitVec);

	if (bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;

	// Don't use if radius is zero.
	if (bestSphere.W <= 0.f)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple Sphere collider."));
		return 0;
	}

	FKSphereElem SphereElem;
	SphereElem.Center = bestSphere.Center;
	SphereElem.Radius = bestSphere.W;
	OutAggregateCollisions.SphereElems.Add(SphereElem);

	return 1;
}

void
FHoudiniMeshTranslator::CalcBoundingSphere(const TArray<FVector>& PositionArray, FSphere& sphere, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	if (PositionArray.Num() == 0)
		return;

	FBox Box;
	FVector MinIx[3];
	FVector MaxIx[3];

	bool bFirstVertex = true;
	for (const FVector& CurPosition : PositionArray)
	{
		FVector p = CurPosition * LimitVec;
		if (bFirstVertex)
		{
			// First, find AABB, remembering furthest points in each dir.
			Box.Min = p;
			Box.Max = Box.Min;

			MinIx[0] = CurPosition;
			MinIx[1] = CurPosition;
			MinIx[2] = CurPosition;

			MaxIx[0] = CurPosition;
			MaxIx[1] = CurPosition;
			MaxIx[2] = CurPosition;
			bFirstVertex = false;
			continue;
		}

		// X //
		if (p.X < Box.Min.X)
		{
			Box.Min.X = p.X;
			MinIx[0] = CurPosition;
		}
		else if (p.X > Box.Max.X)
		{
			Box.Max.X = p.X;
			MaxIx[0] = CurPosition;
		}

		// Y //
		if (p.Y < Box.Min.Y)
		{
			Box.Min.Y = p.Y;
			MinIx[1] = CurPosition;
		}
		else if (p.Y > Box.Max.Y)
		{
			Box.Max.Y = p.Y;
			MaxIx[1] = CurPosition;
		}

		// Z //
		if (p.Z < Box.Min.Z)
		{
			Box.Min.Z = p.Z;
			MinIx[2] = CurPosition;
		}
		else if (p.Z > Box.Max.Z)
		{
			Box.Max.Z = p.Z;
			MaxIx[2] = CurPosition;
		}
	}

	const FVector Extremes[3] = { (MaxIx[0] - MinIx[0]) * LimitVec,
		(MaxIx[1] - MinIx[1]) * LimitVec,
		(MaxIx[2] - MinIx[2]) * LimitVec };

	// Now find extreme points furthest apart, and initial center and radius of sphere.
	float d2 = 0.f;
	for (int32 i = 0; i < 3; i++)
	{
		const float tmpd2 = Extremes[i].SizeSquared();
		if (tmpd2 > d2)
		{
			d2 = tmpd2;
			sphere.Center = (MinIx[i] + (0.5f * Extremes[i])) * LimitVec;
			sphere.W = 0.f;
		}
	}

	const FVector Extents = FVector(Extremes[0].X, Extremes[1].Y, Extremes[2].Z);

	// radius and radius squared
	float r = 0.5f * Extents.GetMax();
	float r2 = FMath::Square(r);

	// Now check each point lies within this sphere. If not - expand it a bit.
	for (const FVector& curPos : PositionArray)
	{
		const FVector cToP = (curPos * LimitVec) - sphere.Center;

		const float pr2 = cToP.SizeSquared();

		// If this point is outside our current bounding sphere's radius
		if (pr2 > r2)
		{
			// ..expand radius just enough to include this point.
			const float pr = FMath::Sqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = FMath::Square(r);

			sphere.Center += ((pr - r) / pr * cToP);
		}
	}

	sphere.W = r;
}

void
FHoudiniMeshTranslator::CalcBoundingSphere2(const TArray<FVector>& PositionArray, FSphere& sphere, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FVector Center, Extents;
	CalcBoundingBox(PositionArray, Center, Extents, LimitVec);

	sphere.Center = Center;
	sphere.W = 0.0f;

	for (const FVector& curPos : PositionArray)
	{
		float Dist = FVector::DistSquared(curPos * LimitVec, sphere.Center);
		if (Dist > sphere.W)
			sphere.W = Dist;
	}
	sphere.W = FMath::Sqrt(sphere.W);
}

int32 
FHoudiniMeshTranslator::GenerateSphylAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FSphere sphere;
	float length;
	FRotator rotation;
	FVector unitVec = FVector::OneVector;

	// Calculate bounding sphyl.
	CalcBoundingSphyl(InPositionArray, sphere, length, rotation, unitVec);

	// Dont use if radius is zero.
	if (sphere.W <= 0.f)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple Capsule collider."));
		return 0;
	}

	// If height is zero, then a sphere would be better (should we just create one instead?)
	if (length <= 0.f)
	{
		length = SMALL_NUMBER;
	}

	FKSphylElem SphylElem;
	SphylElem.Center = sphere.Center;
	SphylElem.Rotation = rotation;
	SphylElem.Radius = sphere.W;
	SphylElem.Length = length;
	OutAggregateCollisions.SphylElems.Add(SphylElem);

	return 1;
}

int32 
FHoudiniMeshTranslator::GenerateOrientedSphylAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	//
	// Code adapted from Experimental GeometryProcessing plugin for simple mesh approximation.
	//

	const int32 NumPoints = InPositionArray.Num();
	TArray<houdini::gte::Vector3<double>> Points;
	Points.SetNumUninitialized(InPositionArray.Num());
	for(int32 i = 0; i < NumPoints; ++i)
	{
		Points[i] = Convert<double>(InPositionArray[i]);
	}

	houdini::gte::Capsule3<double> FitCapsule;
	const bool bResultValid = GetContainer(NumPoints, Points.GetData(), FitCapsule);
	if (!bResultValid)
	{
		return 0;
	}

	houdini::gte::Vector3<double> GteCenter, GteDirection;
	double Extent;
	FitCapsule.segment.GetCenteredForm(GteCenter, GteDirection, Extent);
	const FVector Direction = Convert<double>(GteDirection);
	const FRotator Rot = FRotationMatrix::MakeFromZ(Direction).Rotator();

	// Dont use if radius is zero.
	if (FitCapsule.radius <= 0.f)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple Capsule collider."));
		return 0;
	}

	// If height is zero, then a sphere would be better (should we just create one instead?)
	if (Extent <= 0.f)
	{
		Extent = SMALL_NUMBER;
	}

	FKSphylElem SphylElem;
	SphylElem.Center = Convert<double>(GteCenter);
	SphylElem.Rotation = Rot;
	SphylElem.Radius = FitCapsule.radius;
	SphylElem.Length = Extent*2.f;
	OutAggregateCollisions.SphylElems.Add(SphylElem);

	return 1;
}

void
FHoudiniMeshTranslator::CalcBoundingSphyl(const TArray<FVector>& PositionArray, FSphere& sphere, float& length, FRotator& rotation, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	if (PositionArray.Num() == 0)
		return;

	FVector Center, Extents;
	CalcBoundingBox(PositionArray, Center, Extents, LimitVec);

	// @todo sphere.Center could perhaps be adjusted to best fit if model is non-symmetric on it's longest axis
	sphere.Center = Center;

	// Work out best axis aligned orientation (longest side)
	float Extent = Extents.GetMax();
	if (Extent == Extents.X)
	{
		rotation = FRotator(90.f, 0.f, 0.f);
		Extents.X = 0.0f;
	}
	else if (Extent == Extents.Y)
	{
		rotation = FRotator(0.f, 0.f, 90.f);
		Extents.Y = 0.0f;
	}
	else
	{
		rotation = FRotator(0.f, 0.f, 0.f);
		Extents.Z = 0.0f;
	}

	// Cleared the largest axis above, remaining determines the radius
	float r = Extents.GetMax();
	float r2 = FMath::Square(r);

	// Now check each point lies within this the radius. If not - expand it a bit.
	for (const FVector& CurPos : PositionArray)
	{
		FVector cToP = (CurPos * LimitVec) - sphere.Center;
		cToP = rotation.UnrotateVector(cToP);

		const float pr2 = cToP.SizeSquared2D();	// Ignore Z here...

		// If this point is outside our current bounding sphere's radius
		if (pr2 > r2)
		{
			// ..expand radius just enough to include this point.
			const float pr = FMath::Sqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = FMath::Square(r);
		}
	}

	// The length is the longest side minus the radius.
	float hl = FMath::Max(0.0f, Extent - r);

	// Now check each point lies within the length. If not - expand it a bit.
	for (const FVector& CurPos : PositionArray)
	{
		FVector cToP = (CurPos * LimitVec) - sphere.Center;
		cToP = rotation.UnrotateVector(cToP);

		// If this point is outside our current bounding sphyl's length
		if (FMath::Abs(cToP.Z) > hl)
		{
			const bool bFlip = (cToP.Z < 0.f ? true : false);
			const FVector cOrigin(0.f, 0.f, (bFlip ? -hl : hl));

			const float pr2 = (cOrigin - cToP).SizeSquared();

			// If this point is outside our current bounding sphyl's radius
			if (pr2 > r2)
			{
				FVector cPoint;
				FMath::SphereDistToLine(cOrigin, r, cToP, (bFlip ? FVector(0.f, 0.f, 1.f) : FVector(0.f, 0.f, -1.f)), cPoint);

				// Don't accept zero as a valid diff when we know it's outside the sphere (saves needless retest on further iterations of like points)
				hl += FMath::Max(FMath::Abs(cToP.Z - cPoint.Z), 1.e-6f);
			}
		}
	}

	sphere.W = r;
	length = hl * 2.0f;
}

int32
FHoudiniMeshTranslator::GenerateKDopAsSimpleCollision(const TArray<FVector>& InPositionArray, const TArray<FVector>& Dirs, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	const float my_flt_max = 3.402823466e+38F;

	// Do k- specific stuff.
	int32 kCount = Dirs.Num();

	TArray<float> maxDist;
	maxDist.SetNumUninitialized(kCount);
	for (int32 n = 0; n < maxDist.Num(); n++)
		maxDist[n] = -my_flt_max;

	// Construct temporary UModel for kdop creation. We keep no refs to it, so it can be GC'd.
	auto TempModel = NewObject<UModel>();
	TempModel->Initialize(nullptr, 1);

	// For each vertex, project along each kdop direction, to find the max in that direction.
	for (int32 i = 0; i < InPositionArray.Num(); i++)
	{
		for (int32 j = 0; j < kCount; j++)
		{
			float dist = InPositionArray[i] | Dirs[j];
			maxDist[j] = FMath::Max(dist, maxDist[j]);
		}
	}

	// Inflate kdop to ensure it is no degenerate
	const float MinSize = 0.1f;
	for (int32 i = 0; i < kCount; i++)
	{
		maxDist[i] += MinSize;
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane4f> planes;
	for (int32 i = 0; i < kCount; i++)
		planes.Add(FPlane4f((FVector3f)Dirs[i], maxDist[i]));

	for (int32 i = 0; i < planes.Num(); i++)
	{
		FPoly*	Polygon = new(TempModel->Polys->Element) FPoly();
		FVector3f Base, AxisX, AxisY;

		Polygon->Init();
		Polygon->Normal = planes[i];
		Polygon->Normal.FindBestAxisVectors(AxisX, AxisY);

		Base = planes[i] * planes[i].W;

		new(Polygon->Vertices) FVector3f(Base + AxisX * UE_OLD_HALF_WORLD_MAX + AxisY * UE_OLD_HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector3f(Base + AxisX * UE_OLD_HALF_WORLD_MAX - AxisY * UE_OLD_HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector3f(Base - AxisX * UE_OLD_HALF_WORLD_MAX - AxisY * UE_OLD_HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector3f(Base - AxisX * UE_OLD_HALF_WORLD_MAX + AxisY * UE_OLD_HALF_WORLD_MAX);

		for (int32 j = 0; j < planes.Num(); j++)
		{
			if (i != j)
			{
				if (!Polygon->Split(-FVector3f(planes[j]), planes[j] * planes[j].W))
				{
					Polygon->Vertices.Empty();
					break;
				}
			}
		}

		if (Polygon->Vertices.Num() < 3)
		{
			// If poly resulted in no verts, remove from array
			TempModel->Polys->Element.RemoveAt(TempModel->Polys->Element.Num() - 1);
		}
		else
		{
			// Other stuff...
			Polygon->iLink = i;
			Polygon->CalcNormal(1);
		}
	}

	if (TempModel->Polys->Element.Num() < 4)
	{
		TempModel = NULL;
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple KDOP collider."));
		return 0;
	}

	// Build bounding box.
	TempModel->BuildBound();

	// Build BSP for the brush.
	FBSPOps::bspBuild(TempModel, FBSPOps::BSP_Good, 15, 70, 1, 0);
	FBSPOps::bspRefresh(TempModel, 1);
	FBSPOps::bspBuildBounds(TempModel);

	// Now, create a temporary BodySetup to build the colliders
	UBodySetup* TempBS = NewObject<UBodySetup>();
	TempBS->CreateFromModel(TempModel, false);

	// Copy the convex elements back to our aggregate
	int32 NewConvexElems = 0;
	if (TempBS && TempBS->AggGeom.ConvexElems.Num() > 0)
	{
		for (const auto& CurConvexElem : TempBS->AggGeom.ConvexElems)
		{
			OutAggregateCollisions.ConvexElems.Add(CurConvexElem);
			NewConvexElems++;
		}
	}

	return NewConvexElems;
}


void 
FHoudiniMeshTranslator::SetPackageParams(const FHoudiniPackageParams& InPackageParams, const bool& bUpdateHGPO)
{
	PackageParams = InPackageParams;

	if (bUpdateHGPO)
	{
		PackageParams.ObjectId = HGPO.ObjectId;
		PackageParams.GeoId = HGPO.GeoId;
		PackageParams.PartId = HGPO.PartId;
	}
}

bool 
FHoudiniMeshTranslator::RemoveAndDestroyComponent(UObject* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent);
	if (IsValid(SceneComponent))
	{
		// Remove from the HoudiniAssetActor
		if (SceneComponent->GetOwner())
			SceneComponent->GetOwner()->RemoveOwnedComponent(SceneComponent);

		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SceneComponent->UnregisterComponent();
		SceneComponent->DestroyComponent();

		return true;
	}

	return false;
}

UMeshComponent*
FHoudiniMeshTranslator::CreateMeshComponent(UObject *InOuterComponent, const TSubclassOf<UMeshComponent> &InComponentType)
{
	// Create a new SMC as we couldn't find an existing one
	USceneComponent* OuterSceneComponent = Cast<USceneComponent>(InOuterComponent);
	UObject * Outer = nullptr;
	if (IsValid(OuterSceneComponent))
		Outer = OuterSceneComponent->GetOwner() ? OuterSceneComponent->GetOwner() : OuterSceneComponent->GetOuter();

	UMeshComponent *MeshComponent = NewObject<UMeshComponent>(Outer, InComponentType, NAME_None, RF_Transactional);

	// Initialize it
	MeshComponent->SetVisibility(true);
	//MeshComponent->SetMobility(Mobility);

	// TODO:
	// Property propagation: set the new SMC's properties to the HAC's current settings
	//CopyComponentPropertiesTo(MeshComponent);

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(OuterSceneComponent);
	if (PrimComponent)
	{
		// The outer component is a primitive, so we can copy the collision profile from the scene component.  
		MeshComponent->SetCollisionProfileName(PrimComponent->GetCollisionProfileName());
	}

	// Change the creation method so the component is listed in the details panels
	FHoudiniEngineRuntimeUtils::AddOrSetAsInstanceComponent(MeshComponent);

	// Attach created static mesh component to our Houdini component.
	MeshComponent->AttachToComponent(OuterSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
	MeshComponent->OnComponentCreated();
	MeshComponent->RegisterComponent();

	return MeshComponent;
}

bool
FHoudiniMeshTranslator::UpdateMeshOnStaticMeshComponent(UStaticMeshComponent *InComponent, UObject *InMesh)
{
	if (!IsValid(InComponent))
		return false;

	if (UStaticMesh* const Mesh = Cast<UStaticMesh>(InMesh))
	{
		if (Mesh != InComponent->GetStaticMesh())
			InComponent->SetStaticMesh(Mesh);
		return true;
	}

	return false;
}

bool
FHoudiniMeshTranslator::UpdateMeshOnHoudiniStaticMeshComponent(UHoudiniStaticMeshComponent *InComponent, UObject *InMesh)
{
	if (!IsValid(InComponent))
		return false;

	if (UHoudiniStaticMesh* const Mesh = Cast<UHoudiniStaticMesh>(InMesh))
	{
		if (Mesh != InComponent->GetMesh())
			InComponent->SetMesh(Mesh);
		return true;
	}

	return false;
}

UMeshComponent*
FHoudiniMeshTranslator::CreateOrUpdateMeshComponent(
	const UHoudiniOutput *InOutput, 
	UObject *InOuterComponent, 
	const FHoudiniOutputObjectIdentifier& InOutputIdentifier,
	const TSubclassOf<UMeshComponent>& InComponentType,
	FHoudiniOutputObject& OutputObject,
	FHoudiniGeoPartObject const *& OutFoundHGPO,
	bool& bCreated)
{
	bCreated = false;
	OutFoundHGPO = nullptr;

	// Find the HGPO that matches this mesh
	for (auto& curHGPO : InOutput->HoudiniGeoPartObjects)
	{
		if (curHGPO.ObjectId != InOutputIdentifier.ObjectId
			|| curHGPO.GeoId != InOutputIdentifier.GeoId
			|| curHGPO.PartId != InOutputIdentifier.PartId)
		{
			continue;
		}

		if (InOutputIdentifier.SplitIdentifier.Equals(HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION)
			|| curHGPO.SplitGroups.Contains(InOutputIdentifier.SplitIdentifier))
		{
			OutFoundHGPO = &curHGPO;
			break;
		}

		if (InOutput->GetType() == EHoudiniOutputType::Skeletal)
		{
			OutFoundHGPO = &curHGPO;
			break;
		}
	}

	// No need to create a component for instanced static meshes!
	if (OutFoundHGPO && OutFoundHGPO->bIsInstanced)// && InOutput->GetType() != EHoudiniOutputType::Skeletal)
		return nullptr;

	bool bIsProxyComponent = InComponentType == UHoudiniStaticMeshComponent::StaticClass();

	// See if we already have a component for that mesh
	UMeshComponent* MeshComponent = nullptr;
	if (bIsProxyComponent) 
	{
		MeshComponent = Cast<UMeshComponent>(OutputObject.ProxyComponent);
	} 
	else if (OutputObject.OutputComponents.Num() > 0) 
	{
		MeshComponent = Cast<UMeshComponent>(OutputObject.OutputComponents[0]);
	}

	// If there is an existing component, but it is pending kill, then it was likely
	// deleted by some other process, such as by the user in the editor, so don't use it
	if (!IsValid(MeshComponent) || !MeshComponent->IsA(InComponentType))
	{
		// If the component is not of type InComponentType, or the found component is pending kill, destroy 
		// the existing component (a new one is then created below)
		RemoveAndDestroyComponent(MeshComponent);
		MeshComponent = nullptr;
	}

	if (!MeshComponent)
	{
		// Create a new SMC/HSMC as we couldn't find an existing one
		MeshComponent = CreateMeshComponent(InOuterComponent, InComponentType);

		if (MeshComponent)
		{
			// Add to the output object
			if (bIsProxyComponent)
				OutputObject.ProxyComponent = MeshComponent;
			else
			{
				check(OutputObject.OutputComponents.Num() < 2); // Multiple components not supported yet.
				OutputObject.OutputComponents.Empty();
				OutputObject.OutputComponents.Add(MeshComponent);
			}

			bCreated = true;
		}
	}

	return MeshComponent;
}

bool 
FHoudiniMeshTranslator::AddActorsToMeshSocket(UStaticMeshSocket * Socket, UStaticMeshComponent * StaticMeshComponent, 
		TArray<AActor*> & HoudiniCreatedSocketActors, TArray<AActor*> & HoudiniAttachedSocketActors)
{
	if (!IsValid(Socket) || !IsValid(StaticMeshComponent))
		return false;

	// The actor to assign is stored is the socket's tag
	FString ActorString = Socket->Tag;
	if (ActorString.IsEmpty())
		return false;

	// The actor to assign are listed after a |
	TArray<FString> ActorStringArray;
	ActorString.ParseIntoArray(ActorStringArray, TEXT("|"), false);

	// The "real" Tag is the first
	if (ActorStringArray.Num() > 0)
		Socket->Tag = ActorStringArray[0];

	// We just add a Tag, no Actor
	if (ActorStringArray.Num() == 1)
		return false;

	// Extract the parsed actor string to split it further
	ActorString = ActorStringArray[1];

	// Converting the string to a string array using delimiters
	const TCHAR* Delims[] = { TEXT(","), TEXT(";") };
	ActorString.ParseIntoArray(ActorStringArray, Delims, 2);

	// And try to find the corresponding HoudiniAssetActor in the editor world
	// to avoid finding "deleted" assets with the same name
	//UWorld* editorWorld = GEditor->GetEditorWorldContext().World();
#if WITH_EDITOR
	UWorld* EditorWorld = StaticMeshComponent->GetOwner() ? StaticMeshComponent->GetOwner()->GetWorld() : nullptr;
	if (!IsValid(EditorWorld))
		return false;

	// Remove the previously created actors which were attached to this socket
	{
		for (int32 Idx = HoudiniCreatedSocketActors.Num() - 1; Idx >= 0; --Idx) 
		{
			AActor * CurActor = HoudiniCreatedSocketActors[Idx];
			if (!IsValid(CurActor)) 
			{
				HoudiniCreatedSocketActors.RemoveAt(Idx);
				continue;
			}

			if (CurActor->GetAttachParentSocketName() == Socket->SocketName) 
			{
				HoudiniCreatedSocketActors.RemoveAt(Idx);
				CurActor->Destroy();
			}
		}
	}

	// Detach the previous in level actors which was attached to this socket
	{
		for (int32 Idx = HoudiniAttachedSocketActors.Num() - 1; Idx >= 0; --Idx) 
		{
			AActor * CurActor = HoudiniAttachedSocketActors[Idx];
			if (!IsValid(CurActor)) 
			{
				HoudiniAttachedSocketActors.RemoveAt(Idx);
				continue;
			}

			if (CurActor->GetAttachParentSocketName() == Socket->SocketName) 
			{
				CurActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
				HoudiniAttachedSocketActors.RemoveAt(Idx);		
			}
		}
	}

	auto CreateDefaultActor = [EditorWorld, StaticMeshComponent, Socket, HoudiniCreatedSocketActors]() 
	{
		AActor * CreatedDefaultActor = nullptr;

		UStaticMesh * DefaultReferenceSM = FHoudiniEngine::Get().GetHoudiniDefaultReferenceMesh().Get();
		if (IsValid(DefaultReferenceSM))
		{
			TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject(
				EditorWorld->GetCurrentLevel(), DefaultReferenceSM, false, RF_Transactional, nullptr);

			if (NewActors.Num() <= 0 || !IsValid(NewActors[0]))
			{
				HOUDINI_LOG_WARNING(
					TEXT("Failed to load default mesh."));
			}
			else
			{

				// Set the default mesh actor components mobility to the same as output SMC's
				EComponentMobility::Type OutputSMCMobility = StaticMeshComponent->Mobility;
				for (auto & CurComp : NewActors[0]->GetComponents())
				{
					UStaticMeshComponent * CurSMC = Cast<UStaticMeshComponent>(CurComp);
					if (IsValid(CurSMC))
						CurSMC->SetMobility(OutputSMCMobility);
				}

				// Set the default mesh actor hidden in game.
				NewActors[0]->SetActorHiddenInGame(true);

				Socket->AttachActor(NewActors[0], StaticMeshComponent);
				CreatedDefaultActor = NewActors[0];

				FVector SocketScale = Socket->RelativeScale;
				if (!SocketScale.IsZero() && !SocketScale.Equals(FVector::OneVector))
				{
					NewActors[0]->SetActorRelativeScale3D(SocketScale);
				}
				//HoudiniCreatedSocketActors.Add(NewActors[0]);
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(
				TEXT("Failed to load default mesh."));
		}

		return CreatedDefaultActor;
	};

	// If nothing was specified, we're done
	if (ActorStringArray.Num() <= 0)
		return true;

	bool bUseDefaultActor = true;
	// Get from the Houdini runtime setting if use default object when the reference is invalid
	// true by default if fail to access HoudiniRuntimeSettings
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings) 
	{
		bUseDefaultActor = HoudiniRuntimeSettings->bShowDefaultMesh;
	}

	/*
	// !! Only use the default mesh if we failed to find/spawn the actor to attach
	// not if we didn't specify any actor to attach!
	if (ActorStringArray.Num() <= 0) 
	{
		if (!bUseDefaultActor)
			return true;

		HOUDINI_LOG_WARNING(
			TEXT("Output static mesh: Socket '%s' actor is not specified. Spawn a default mesh (hidden in game)."), *(Socket->GetName()));

		AActor * DefaultActor = CreateDefaultActor();
		if (IsValid(DefaultActor))
			HoudiniCreatedSocketActors.Add(DefaultActor);

		return true;
	}
	*/

	// try to find the actor in level first
	for (TActorIterator<AActor> ActorItr(EditorWorld); ActorItr; ++ActorItr)
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AActor *Actor = *ActorItr;
		if (!IsValid(Actor) || Actor->IsUnreachable())
			continue;

		for (int32 StringIdx = 0; StringIdx < ActorStringArray.Num(); StringIdx++)
		{
			if (Actor->GetName() != ActorStringArray[StringIdx]
				&& Actor->GetActorLabel() != ActorStringArray[StringIdx])
				continue;

			// Set the actor components mobility to the same as output SMC's
			EComponentMobility::Type OutputSMCMobility = StaticMeshComponent->Mobility;
			for (auto & CurComp : Actor->GetComponents()) 
			{
				UStaticMeshComponent * SMC = Cast<UStaticMeshComponent>(CurComp);
				if (IsValid(SMC))
					SMC->SetMobility(OutputSMCMobility);
			}

			Socket->AttachActor(Actor, StaticMeshComponent);
			HoudiniAttachedSocketActors.Add(Actor);

			FVector SocketScale = Socket->RelativeScale;
			if (!SocketScale.IsZero() && !SocketScale.Equals(FVector::OneVector))
			{
				Actor->SetActorRelativeScale3D(SocketScale);
			}

			// Remove the string if the actor is found in the editor level
			ActorStringArray.RemoveAt(StringIdx);
			break;
		}
	}

	bool bSuccess = true;
	// If some of the actors are not found in the level, try to find them in the content browser. Spawn one if existed
	for (int32 Idx = ActorStringArray.Num() - 1; Idx>= 0; --Idx) 
	{
		UObject * Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *ActorStringArray[Idx]);
		if (!IsValid(Obj)) 
		{
			bSuccess = false;
			continue;
		}

		// Spawn a new actor with the found object
		TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject(
			EditorWorld->GetCurrentLevel(), Obj, false, RF_Transactional, nullptr);

		if (NewActors.Num() <= 0 || !IsValid(NewActors[0])) 
		{
			bSuccess = false;
			continue;
		}

		// Set the new actor components mobility to the same as output SMC's
		EComponentMobility::Type OutputSMCMobility = StaticMeshComponent->Mobility;
		for (auto & CurComp : NewActors[0]->GetComponents()) 
		{
			UStaticMeshComponent * CurSMC = Cast<UStaticMeshComponent>(CurComp);
			if (IsValid(CurSMC))
				CurSMC->SetMobility(OutputSMCMobility);
		}

		Socket->AttachActor(NewActors[0], StaticMeshComponent);
		HoudiniCreatedSocketActors.Add(NewActors[0]);
		
		FVector SocketScale = Socket->RelativeScale;
		if (!SocketScale.IsZero() && !SocketScale.Equals(FVector::OneVector))
		{
			NewActors[0]->SetActorRelativeScale3D(SocketScale);
		}

		ActorStringArray.RemoveAt(Idx);
	}

	// Failed to find actors in both level and content browser
	// Spawn default actors if enabled
	if (bUseDefaultActor)
	{
		for (int32 Idx = ActorStringArray.Num() - 1; Idx >= 0; --Idx)
		{
			HOUDINI_LOG_WARNING(
				TEXT("Output static mesh: Failed to attach '%s' to socket '%s', spawn a default mesh (hidden in game)."), *(ActorStringArray[Idx]), *(Socket->GetName()));

			// If failed to load this object, spawn a default mesh
			AActor * CurDefaultActor = CreateDefaultActor();
			if (IsValid(CurDefaultActor))
				HoudiniCreatedSocketActors.Add(CurDefaultActor);
		}
	}

	if (ActorStringArray.Num() > 0)
		return false;
#endif

	return bSuccess;
}


bool FHoudiniMeshTranslator::HasFracturePieceAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId)
{
	bool bHISM = false;
	TArray<int> IntData;
	IntData.Empty();

	FHoudiniHapiAccessor Accessor(GeoId, PartId, HAPI_UNREAL_ATTRIB_GC_PIECE);

	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, IntData);

	if (bSuccess)
	{
		if (IntData.Num() > 0)
			bHISM = true;
	}

	return bHISM;
}

void
FHoudiniMeshTranslator::UpdateMeshBuildSettings(
	FMeshBuildSettings& OutMeshBuildSettings,
	const bool& bHasNormals, 
	const bool& bHasTangents, 
	const bool& bHasLightmapUVSet)
{
	// Use the values provided to the translator
	OutMeshBuildSettings = StaticMeshBuildSettings;

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	// Recomputing normals.
	EHoudiniRuntimeSettingsRecomputeFlag RecomputeNormalFlag = HoudiniRuntimeSettings ? (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeNormalsFlag : HRSRF_OnlyIfMissing;
	if(RecomputeNormalFlag == HRSRF_OnlyIfMissing)
		OutMeshBuildSettings.bRecomputeNormals = !bHasNormals;

	// Recomputing tangents.
	EHoudiniRuntimeSettingsRecomputeFlag RecomputeTangentFlag = HoudiniRuntimeSettings ? (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeTangentsFlag : HRSRF_OnlyIfMissing;
	if (RecomputeTangentFlag == HRSRF_OnlyIfMissing)
		OutMeshBuildSettings.bRecomputeTangents = !bHasTangents;

	// Lightmap UV generation.
	EHoudiniRuntimeSettingsRecomputeFlag GenerateLightmapUVsFlag = HoudiniRuntimeSettings ? (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->GenerateLightmapUVsFlag : HRSRF_OnlyIfMissing;
	if (GenerateLightmapUVsFlag == HRSRF_OnlyIfMissing)
		OutMeshBuildSettings.bGenerateLightmapUVs = !bHasLightmapUVSet;
}


void FHoudiniMeshTranslator::BuildMeshDescription(FMeshDescription* MeshDescription, FHoudiniGroupedMeshPrimitives& SplitMeshData)
{
	bool bHasNormal = SplitMeshData.Normals.Num() > 0;
	bool bHasTangents = SplitMeshData.TangentU.Num() > 0 && SplitMeshData.TangentV.Num() > 0;
	bool bHasRGB = SplitMeshData.Colors.Num() > 0;
	bool bHasRGBA = bHasRGB && AttribInfoColors.tupleSize == 4;
	bool bHasAlpha = SplitMeshData.Alphas.Num() > 0;
	int UVSetCount = PartUVSets.Num();
	uint32 FaceCount = SplitMeshData.Indices.Num() / 3;

	// Create a Polygon Group for each material slot
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
		MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	// We must use the number of assignment materials found to reserve the number of material slots
	// Don't use the SM's StaticMaterials here as we may not reserve enough polygon groups when adding more materials
	// Create a polygon group for each material slot.
	int32 NumberOfMaterials = OutputAssignmentMaterials.Num();
	if (NumberOfMaterials <= 0)
	{
		// No materials, create a polygon group for the default one
		const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
		PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = FName(HAPI_UNREAL_DEFAULT_MATERIAL_NAME);
	}
	else
	{
		MeshDescription->ReserveNewPolygonGroups(NumberOfMaterials);
		//for (int32 MatIndex = 0; MatIndex < NumberOfMaterials; ++MatIndex)
		for (auto& CurrentMatAssignement : OutputAssignmentMaterials)
		{
			const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
			PolygonGroupImportedMaterialSlotNames[PolygonGroupID] =
				FName(CurrentMatAssignement.Value ? *(CurrentMatAssignement.Value->GetName()) : *(CurrentMatAssignement.Key.MaterialObjectPath));
		}
	}

	TVertexAttributesRef<FVector3f> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Vertex::Position);

	bool bHasInvalidPositionIndexData = false;
	MeshDescription->ReserveNewVertices(SplitMeshData.NeededVertices.Num());
	for (const int32& NeededVertexIndex : SplitMeshData.NeededVertices)
	{
		// Create a new Vertex
		FVertexID VertexID = MeshDescription->CreateVertex();
		if (PartPositions.IsValidIndex(NeededVertexIndex * 3 + 2))
		{
			// We need to swap Z and Y coordinate here, and convert from m to cm. 
			VertexPositions[VertexID].X = PartPositions[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			VertexPositions[VertexID].Y = PartPositions[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			VertexPositions[VertexID].Z = PartPositions[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}
		else
		{
			// Error when retrieving positions.
			bHasInvalidPositionIndexData = true;

			continue;
		}
	}

	if (bHasInvalidPositionIndexData)
	{
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] invalid position/index data ")
			TEXT("- skipping."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitMeshData.SplitId, *SplitMeshData.SplitGroupName);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	//  BUILD GEOMETRY (FACE/VERTICES)
	//---------------------------------------------------------------------------------------------------------------------

	MeshDescription->ReserveNewVertexInstances(SplitMeshData.Indices.Num());
	MeshDescription->ReserveNewPolygons(SplitMeshData.Indices.Num() / 3);
	//Approximately 2.5 edges per polygons
	MeshDescription->ReserveNewEdges(SplitMeshData.Indices.Num() * 2.5f / 3);

	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate);
	VertexInstanceUVs.SetNumChannels(UVSetCount);

	TArray<bool> HasUVSets;
	HasUVSets.SetNumZeroed(PartUVSets.Num());
	for (int32 Idx = 0; Idx < PartUVSets.Num(); Idx++)
		HasUVSets[Idx] = PartUVSets[Idx].Num() > 0;

	for (uint32 FaceIndex = 0; FaceIndex < FaceCount; FaceIndex++)
	{
		TArray<FVertexInstanceID> FaceVertexInstanceIDs;
		FaceVertexInstanceIDs.SetNum(3);

		// Ignore degenerate triangles
		FVertexID VertexIDs[3];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			VertexIDs[Corner] = FVertexID(SplitMeshData.Indices[(FaceIndex * 3) + Corner]);
		}
		if (VertexIDs[0] == VertexIDs[1] || VertexIDs[0] == VertexIDs[2] || VertexIDs[1] == VertexIDs[2])
			continue;

		//FVertexID FaceVertexIDs[3];
		for (int32 Corner = 0; Corner < 3; Corner++)
		{
			uint32 SplitIndex = (FaceIndex * 3) + Corner;
			uint32 SplitVertexIndex = SplitMeshData.Indices[SplitIndex];
			const FVertexInstanceID& VertexInstanceID = MeshDescription->CreateVertexInstance(FVertexID(SplitVertexIndex));

			// Fix the winding order by updating the SplitIndex (invert corner 1 and 2)
			// instead of going 0 1 2 go 0 2 1
			// TODO; this slows down StaticMesh->Build() considerably!
			Corner == 1 ? SplitIndex++ : Corner == 2 ? SplitIndex-- : SplitIndex;

			const uint32 SplitVertexIndex_X = SplitIndex * 3 + 0;
			const uint32 SplitVertexIndex_Y = SplitIndex * 3 + 2;
			const uint32 SplitVertexIndex_Z = SplitIndex * 3 + 1;
			// Normals
			if (bHasNormal)
			{
				// We need to swap Z and Y coordinate here, and convert from m to cm. 
				VertexInstanceNormals[VertexInstanceID].X = SplitMeshData.Normals[SplitVertexIndex_X];
				VertexInstanceNormals[VertexInstanceID].Y = SplitMeshData.Normals[SplitVertexIndex_Y];
				VertexInstanceNormals[VertexInstanceID].Z = SplitMeshData.Normals[SplitVertexIndex_Z];
			}

			// Tangents and binormals
			if (bHasTangents)
			{
				// We need to swap Z and Y coordinate here, and convert from m to cm.
				VertexInstanceTangents[VertexInstanceID].X = SplitMeshData.TangentU[SplitVertexIndex_X];
				VertexInstanceTangents[VertexInstanceID].Y = SplitMeshData.TangentU[SplitVertexIndex_Y];
				VertexInstanceTangents[VertexInstanceID].Z = SplitMeshData.TangentU[SplitVertexIndex_Z];

				FVector3f TangentY;
				TangentY.X = SplitMeshData.TangentV[SplitVertexIndex_X];
				TangentY.Y = SplitMeshData.TangentV[SplitVertexIndex_Y];
				TangentY.Z = SplitMeshData.TangentV[SplitVertexIndex_Z];

				VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(
					(FVector)VertexInstanceTangents[VertexInstanceID].GetSafeNormal(),
					(FVector)TangentY.GetSafeNormal(),
					(FVector)VertexInstanceNormals[VertexInstanceID].GetSafeNormal());
			}

			// Color
			FLinearColor Color = FLinearColor::White;
			if (bHasRGB)
			{
				Color.R = FMath::Clamp(SplitMeshData.Colors[SplitIndex * AttribInfoColors.tupleSize + 0], 0.0f, 1.0f);
				Color.G = FMath::Clamp(SplitMeshData.Colors[SplitIndex * AttribInfoColors.tupleSize + 1], 0.0f, 1.0f);
				Color.B = FMath::Clamp(SplitMeshData.Colors[SplitIndex * AttribInfoColors.tupleSize + 2], 0.0f, 1.0f);
			}
			// Alpha
			if (bHasAlpha)
			{
				Color.A = FMath::Clamp(SplitMeshData.Alphas[SplitIndex], 0.0f, 1.0f);
			}
			else if (bHasRGBA)
			{
				Color.A = FMath::Clamp(SplitMeshData.Colors[SplitIndex * AttribInfoColors.tupleSize + 3], 0.0f, 1.0f);
			}
			VertexInstanceColors[VertexInstanceID] = FVector4f(Color);

			// UVs
			for (int32 UVIndex = 0; UVIndex < SplitMeshData.UVSets.Num(); UVIndex++)
			{
				if (HasUVSets[UVIndex])
				{
					// We need to flip V coordinate when it's coming from HAPI.
					FVector2f CurrentUV;
					CurrentUV.X = SplitMeshData.UVSets[UVIndex][SplitIndex * 2 + 0];
					CurrentUV.Y = 1.0f - SplitMeshData.UVSets[UVIndex][SplitIndex * 2 + 1];

					VertexInstanceUVs.Set(VertexInstanceID, UVIndex, CurrentUV);
				}
			}

			FaceVertexInstanceIDs[Corner] = VertexInstanceID;
		}

		const FPolygonGroupID PolygonGroupID(SplitMeshData.FaceMaterialIndices[FaceIndex]);

		// Insert a triangle into the mesh
		MeshDescription->CreateTriangle(PolygonGroupID, FaceVertexInstanceIDs);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	//  FACE SMOOTHING
	//---------------------------------------------------------------------------------------------------------------------

	// Check that the number of face smoothing values we retrieved is correct
	int32 WedgeFaceSmoothCount = SplitMeshData.FaceSmoothingMasks.Num() / 3;

	// Get valid count of vertex indices for this split.
	const int32 SplitVertexCount = AllSplitVertexCounts[SplitMeshData.SplitGroupName];


	// FaceSmoothing masks must be initialized even if we don't have a value from Houdini!
	// TODO: Expose the default FaceSmoothing value  0 will make hard face
	TArray<uint32> FaceSmoothingMasks;
	FaceSmoothingMasks.SetNumUninitialized(SplitVertexCount);
	for (int32 n = 0; n < FaceSmoothingMasks.Num(); n++)
		FaceSmoothingMasks[n] = DefaultMeshSmoothing;


	if (SplitMeshData.FaceSmoothingMasks.Num() != 0 && !SplitMeshData.FaceSmoothingMasks.IsValidIndex((WedgeFaceSmoothCount - 1) * 3 + 2))
	{
		// Ignore our face smoothing values
		WedgeFaceSmoothCount = 0;
		HOUDINI_LOG_WARNING(TEXT("Invalid face smoothing mask count detected - Skipping them."));
	}

	// Transfer the face smoothing masks to the raw mesh if we have any
	for (int32 WedgeFaceSmoothIdx = 0; WedgeFaceSmoothIdx < WedgeFaceSmoothCount; WedgeFaceSmoothIdx += 3)
	{
		FaceSmoothingMasks[WedgeFaceSmoothIdx] = SplitMeshData.FaceSmoothingMasks[WedgeFaceSmoothIdx * 3];
	}

	FStaticMeshOperations::ConvertSmoothGroupToHardEdges(FaceSmoothingMasks, *MeshDescription);
}

void FHoudiniMeshTranslator::ProcessMaterials(UStaticMesh* FoundStaticMesh, FHoudiniGroupedMeshPrimitives& SplitMeshData)
{
	// Map of Houdini Material IDs to Unreal Material Interface
	TMap<HAPI_NodeId, UMaterialInterface*> MapHoudiniMatIdToUnrealInterface;
	// Map of Houdini Material Attributes to Unreal Material Interface
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> MapHoudiniMatAttributesToUnrealInterface;
	// Map of Unreal Material Interface to Unreal Material Index, per visible mesh
	TMap<UStaticMesh*, TMap<UMaterialInterface*, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	//ONEONE

	TArray<FStaticMaterial>& FoundStaticMaterials = FoundStaticMesh->GetStaticMaterials();

	// // TODO: Check if still needed for MeshDescription
	// // We need to reset the Static Mesh's materials once per SM:
	// // so, for the first lod, or the main geo...
	// if (!MeshMaterialsHaveBeenReset && (SplitType == EHoudiniSplitType::LOD || SplitType == EHoudiniSplitType::Normal))
	// {
	// 	FoundStaticMaterials.Empty();
	// 	MeshMaterialsHaveBeenReset = true;
	// }
	//
	// // ..  or for each visible complex collider
	// if (SplitType == EHoudiniSplitType::RenderedComplexCollider)
	// 	FoundStaticMaterials.Empty();

	// Clear the materials array of the mesh the first time we encounter it
	if (!MapUnrealMaterialInterfaceToUnrealIndexPerMesh.Contains(FoundStaticMesh))
	{
		FoundStaticMaterials.Empty();
	}
	TMap<UMaterialInterface*, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

	// Get this split's faces
	TArray<int32>& SplitGroupFaceIndices = AllSplitFaceIndices[SplitMeshData.SplitGroupName];
	// Array holding the materials needed for this split
	//TArray<UMaterialInterface*> SplitMaterials;
	// Split Material indices per face, by default all faces are set to use the first Material

	SplitMeshData.FaceMaterialIndices.SetNumZeroed(SplitGroupFaceIndices.Num());

	bool HasHoudiniMaterials = PartUniqueMaterialIds.Num() > 0;
	bool HasMaterialOverrides = PartFaceMaterialOverrides.Num() > 0;
	if (!HasHoudiniMaterials && !HasMaterialOverrides)
	{
		// We don't have any material override or houdini material
		// we just need one polygon group using the default Houdini material.
		UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

		// See if we have a replacement material and use it on the mesh instead
		UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(
			FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, ""));
		if (ReplacementMaterial && *ReplacementMaterial)
			MaterialInterface = *ReplacementMaterial;

		FoundStaticMaterials.Empty();
		FoundStaticMaterials.Add(MaterialInterface);

		// TODO: ? Add default mat to the assignement map?
	}
	else if (HasHoudiniMaterials && !HasMaterialOverrides)
	{
		// We have Houdini Material but no overrides
		if (bOnlyOneFaceMaterial || PartUniqueMaterialIds.Num() == 1)
		{
			// We have only one Houdini material.
			// Use default Houdini material if no valid material is assigned to any of the faces.
			UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

			// Get id of this single material.
			FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, PartFaceMaterialIds[0], MaterialPathName);
			const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
			UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
			if (FoundMaterial)
				MaterialInterface = *FoundMaterial;

			// See if we have a replacement material and use it on the mesh instead
			UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
			if (ReplacementMaterial && *ReplacementMaterial)
				MaterialInterface = *ReplacementMaterial;

			FoundStaticMaterials.Empty();
			FoundStaticMaterials.Add(MaterialInterface);

			// TODO: ? Add the mat to the assignement map?
		}
		else
		{
			// We have multiple houdini materials
			// Get default Houdini material.
			UMaterial* MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get();

			// Reset Rawmesh material face assignments.
			for (int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
			{
				int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];
				if (!PartFaceMaterialIds.IsValidIndex(SplitFaceIndex))
					continue;

				// Get material id for this face.
				HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

				// See if we have already treated that material
				UMaterialInterface** FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
				UMaterialInterface* MaterialInterface = nullptr;
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				if (MaterialInterface)
				{
					int32 const* FoundUnrealMatIndex = MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Find(MaterialInterface);
					if (FoundUnrealMatIndex)
					{
						// This material has been mapped already, just assign the mat index
						SplitMeshData.FaceMaterialIndices[FaceIdx] = *FoundUnrealMatIndex;
						continue;
					}
				}
				else
				{
					MaterialInterface = Cast<UMaterialInterface>(MaterialDefault);

					FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
					const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
					const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
					UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
					if (FoundMaterial)
						MaterialInterface = *FoundMaterial;

					// See if we have a replacement material and use it on the mesh instead
					UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
					if (ReplacementMaterial && *ReplacementMaterial)
						MaterialInterface = *ReplacementMaterial;

					MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
				}

				if (MaterialInterface)
				{
					// Add the material to the Static mesh
					//int32 UnrealMatIndex = SplitMaterials.Add(Material);
					int32 UnrealMatIndex = FoundStaticMaterials.Add(MaterialInterface);

					// Map the houdini ID to the unreal one
					MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Add(MaterialInterface, UnrealMatIndex);

					// Update the face index
					SplitMeshData.Indices[FaceIdx] = UnrealMatIndex;
				}
			}
		}
	}
	else
	{
		// Array used to avoid constantly attempting to load invalid materials
		TArray<FString> InvalidMaterials;

		// If we have material overrides
		for (int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
		{
			int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];

			UMaterialInterface* MaterialInterface = nullptr;
			int32 CurrentFaceMaterialIdx = -1;
			int32 MatIndex = -1;
			if (PartFaceMaterialOverrides.IsValidIndex(SplitFaceIndex))
			{
				// For MeshDescription specifically, we need OutputAssignmentMaterials/MapHoudiniMatAttributesToUnrealInterface
				// to have an entry for each index (distinguish between identical materials in different slots).
				// This is so we have the correct size and create enough polygon groups later.
				const FHoudiniMaterialInfo& MatInfo = PartFaceMaterialOverrides[SplitFaceIndex];
				const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
				FString MaterialName = MatInfo.MaterialObjectPath;
				ExtractMaterialIndex(MaterialName, MatIndex);
				UMaterialInterface** FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				if (!MaterialInterface)
				{
					// Try to locate the corresponding material interface

					// Start by looking in our assignment map
					FoundMaterialInterface = OutputAssignmentMaterials.Find(MaterialIdentifier);
					if (FoundMaterialInterface)
						MaterialInterface = *FoundMaterialInterface;

					if (!MaterialInterface && !MaterialName.IsEmpty() && !InvalidMaterials.Contains(MaterialName))
					{
						// Only try to load a material if has a chance to be valid!
						MaterialInterface = Cast< UMaterialInterface >(
							StaticLoadObject(UMaterialInterface::StaticClass(),
								nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr));

						if (!MaterialInterface)
							InvalidMaterials.Add(MaterialName);
					}

					if (MaterialInterface)
					{
						// We managed to load the UE4 material
						// Make sure this material is in the assignments before replacing it.
						OutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

						// See if we have a replacement material and use it on the mesh instead
						UMaterialInterface* const* ReplacementMaterialInterface = ReplacementMaterials.Find(MaterialIdentifier);
						if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
							MaterialInterface = *ReplacementMaterialInterface;

						// Add this material to the map
						MapHoudiniMatAttributesToUnrealInterface.Add(MaterialIdentifier, MaterialInterface);
					}
				}

				if (!MaterialInterface)
				{
					// The attribute Material or its replacement do not exist
					// See if we can fallback to the Houdini material assigned on the face

					// Get the unreal material corresponding to this houdini one
					HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

					// See if we have already treated that material
					FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
					if (FoundMaterialInterface)
						MaterialInterface = *FoundMaterialInterface;

					if (!MaterialInterface)
					{
						// If everything else fails, we'll use the default material
						MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

						// We need to add this material to the map
						FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
						const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
						const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
						UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
						if (FoundMaterial)
							MaterialInterface = *FoundMaterial;

						// See if we have a replacement material and use it on the mesh instead
						UMaterialInterface* const* ReplacementMaterialInterface = ReplacementMaterials.Find(DefaultMatIdentifier);
						if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
							MaterialInterface = *ReplacementMaterialInterface;

						// Map the Houdini ID to the unreal one
						MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
					}
				}
			}

			CurrentFaceMaterialIdx = GetFaceMaterialIndex(MaterialInterface,
				MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
				MatIndex,
				FoundStaticMaterials);

			// Update the Face Material on the mesh
			SplitMeshData.FaceMaterialIndices[FaceIdx] = CurrentFaceMaterialIdx;
		}
	}
}

void FHoudiniMeshTranslator::PullMeshData(FHoudiniGroupedMeshPrimitives& SplitMeshData, UStaticMesh* FoundStaticMesh, int LODIndex, bool bReadTangents)
{
	auto tick = FPlatformTime::Seconds();
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	// Mesh description uses material to create its PolygonGroups,
	// so we first need to know how many different materials we have for this split
	// and what vertices/indices belong to each material for remapping

	//--------------------------------------------------------------------------------------------------------------------- 
	//  INDICES
	//--------------------------------------------------------------------------------------------------------------------- 

	//
	// Because of the splits, we don't need to declare all the vertices in the Part, 
	// but only the one that are currently used by the split's faces.
	// The indicesMapper array is used to map those indices from Part Vertices to Split Vertices.
	// We also keep track of the needed vertices index to declare them easily afterwards.
	//

	// IndicesMapper:
	// Maps index values for all vertices in the Part:
	// - Vertices unused by the split will be set to -1
	// - Used vertices will have their value set to the "NewIndex" so that IndicesMapper[ partIndex ] => splitIndex
	TArray<int32> PartToSplitIndicesMapper;
	PartToSplitIndicesMapper.SetNumUninitialized(SplitMeshData.VertexList.Num());
	for (int32 n = 0; n < PartToSplitIndicesMapper.Num(); n++)
		PartToSplitIndicesMapper[n] = -1;

	// SplitIndices
	// Array of SplitIndices used to describe this split's polygons

	const int32 SplitVertexCount = AllSplitVertexCounts[SplitMeshData.SplitGroupName];
	SplitMeshData.Indices.SetNumZeroed(SplitVertexCount);

	int32 CurrentSplitIndex = 0;
	int32 ValidVertexId = 0;
	bool bHasInvalidFaceIndices = false;
	for (int32 VertexIdx = 0; VertexIdx < SplitMeshData.VertexList.Num(); VertexIdx += 3)
	{
		int32 WedgeCheck = SplitMeshData.VertexList[VertexIdx + 0];
		if (WedgeCheck == -1)
			continue;

		int32 WedgeIndices[3] =
		{
			SplitMeshData.VertexList[VertexIdx + 0],
			SplitMeshData.VertexList[VertexIdx + 1],
			SplitMeshData.VertexList[VertexIdx + 2]
		};

		// Ensure the indices are valid
		if (!PartToSplitIndicesMapper.IsValidIndex(WedgeIndices[0])
			|| !PartToSplitIndicesMapper.IsValidIndex(WedgeIndices[1])
			|| !PartToSplitIndicesMapper.IsValidIndex(WedgeIndices[2]))
		{
			// Invalid face index.
			bHasInvalidFaceIndices = true;
			continue;
		}

		// Converting Old (Part) Indices to New (Split) Indices:
		for (int32 i = 0; i < 3; i++)
		{
			if (PartToSplitIndicesMapper[WedgeIndices[i]] < 0)
			{
				// This part index has not yet been "converted" to a new split index
				SplitMeshData.NeededVertices.Add(WedgeIndices[i]);
				PartToSplitIndicesMapper[WedgeIndices[i]] = CurrentSplitIndex;
				CurrentSplitIndex++;
			}

			// Replace the old part index with the new split index
			WedgeIndices[i] = PartToSplitIndicesMapper[WedgeIndices[i]];
		}

		if (!SplitMeshData.Indices.IsValidIndex(ValidVertexId + 2))
			break;

		// Flip wedge indices to fix the winding order.
		SplitMeshData.Indices[ValidVertexId + 0] = WedgeIndices[0];
		SplitMeshData.Indices[ValidVertexId + 1] = WedgeIndices[2];
		SplitMeshData.Indices[ValidVertexId + 2] = WedgeIndices[1];

		ValidVertexId += 3;
	}
	if (bHasInvalidFaceIndices)
	{
		HOUDINI_LOG_MESSAGE(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] has some invalid face indices"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitMeshData.SplitId, *SplitMeshData.SplitGroupName);
	}
	if (SplitMeshData.Indices.Num() == 0)
	{
		HOUDINI_LOG_WARNING(
			TEXT("[CreateStaticMesh_MeshDescription]: 0 valid triangles in StaticMesh data for %s LOD %i! Please check the log."),
			*SplitMeshData.SplitGroupName, LODIndex);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITIONS
	//---------------------------------------------------------------------------------------------------------------------				

	UpdatePartPositionIfNeeded();

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIALS
	//---------------------------------------------------------------------------------------------------------------------

	ProcessMaterials(FoundStaticMesh, SplitMeshData);


	if (bDoTiming)
	{
		HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Materials in %f seconds."), FPlatformTime::Seconds() - tick);
		tick = FPlatformTime::Seconds();
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// NORMALS
	//---------------------------------------------------------------------------------------------------------------------

	// Extract the normals
	UpdatePartNormalsIfNeeded();
	FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(SplitMeshData.VertexList, AttribInfoNormals, PartNormals, SplitMeshData.Normals);

	//--------------------------------------------------------------------------------------------------------------------- 
	// TANGENTS
	//---------------------------------------------------------------------------------------------------------------------

	if (bReadTangents)
	{
		// Extract this part's Tangents if needed
		UpdatePartTangentsIfNeeded();

		// Get the Tangents and binormals for this split
		FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(SplitMeshData.VertexList, AttribInfoTangentU, PartTangentU, SplitMeshData.TangentU);
		FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(SplitMeshData.VertexList, AttribInfoTangentV, PartTangentV, SplitMeshData.TangentV);

		// We need to manually generate tangents if:
		// - we have normals but dont have tangentu or tangentv attributes
		// - we have not specified that we wanted unreal to generate them
		int32 NormalCount = SplitMeshData.Normals.Num();
		bool bGenerateTangents = (NormalCount > 0) && (SplitMeshData.TangentU.Num() <= 0 || SplitMeshData.TangentV.Num() <= 0);
		// Check that the number of tangents read matches the number of normals
		if (SplitMeshData.TangentU.Num() != NormalCount || SplitMeshData.TangentV.Num() != NormalCount)
			bGenerateTangents = true;

		if (bGenerateTangents && (HoudiniRuntimeSettings->RecomputeTangentsFlag == EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always))
		{
			// No need to generate tangents if we want unreal to recompute them after
			bGenerateTangents = false;
		}

		// Generate the tangents if needed
		if (bGenerateTangents)
		{
			SplitMeshData.TangentU.SetNumZeroed(NormalCount);
			SplitMeshData.TangentV.SetNumZeroed(NormalCount);
			for (int32 Idx = 0; Idx + 2 < NormalCount; Idx += 3)
			{
				FVector3f TangentZ;
				TangentZ.X = SplitMeshData.Normals[Idx + 0];
				TangentZ.Y = SplitMeshData.Normals[Idx + 2];
				TangentZ.Z = SplitMeshData.Normals[Idx + 1];

				FVector3f TangentX, TangentY;
				TangentZ.FindBestAxisVectors(TangentX, TangentY);

				SplitMeshData.TangentU[Idx + 0] = TangentX.X;
				SplitMeshData.TangentU[Idx + 2] = TangentX.Y;
				SplitMeshData.TangentU[Idx + 1] = TangentX.Z;

				SplitMeshData.TangentV[Idx + 0] = TangentY.X;
				SplitMeshData.TangentV[Idx + 2] = TangentY.Y;
				SplitMeshData.TangentV[Idx + 1] = TangentY.Z;
			}
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// COLORS
	//---------------------------------------------------------------------------------------------------------------------

	UpdatePartColorsIfNeeded();
	FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(SplitMeshData.VertexList, AttribInfoColors, PartColors, SplitMeshData.Colors);

	//--------------------------------------------------------------------------------------------------------------------- 
	// ALPHA
	//---------------------------------------------------------------------------------------------------------------------

	UpdatePartAlphasIfNeeded();
	FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(SplitMeshData.VertexList, AttribInfoAlpha, PartAlphas, SplitMeshData.Alphas);

	//--------------------------------------------------------------------------------------------------------------------- 
	// UV SETS
	//---------------------------------------------------------------------------------------------------------------------

	UpdatePartUVSetsIfNeeded(true);
	// See if we need to transfer uv point attributes to vertex attributes.
	int32 UVSetCount = PartUVSets.Num();

	SplitMeshData.UVSets.SetNum(UVSetCount);
	for (int32 TexCoordIdx = 0; TexCoordIdx < UVSetCount; TexCoordIdx++)
	{
		FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(SplitMeshData.VertexList, AttribInfoUVSets[TexCoordIdx], PartUVSets[TexCoordIdx], SplitMeshData.UVSets[TexCoordIdx]);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// FACE SMOOTHING
	//---------------------------------------------------------------------------------------------------------------------

	UpdatePartFaceSmoothingIfNeeded();
	FHoudiniMeshTranslator::TransferPartAttributesToSplit<int32>(SplitMeshData.VertexList, AttribInfoFaceSmoothingMasks, PartFaceSmoothingMasks, SplitMeshData.FaceSmoothingMasks);

}

void
FHoudiniMeshTranslator::SetPhysicsMaterialFromHGPO(UBodySetup* BodySetup)
{
	// Set physical material if present
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	TArray<FString> AttributeValues;

	FHoudiniHapiAccessor Accessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL);
	Accessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_PRIM);
	AttributeInfo.tupleSize = 1;
	bool bSuccess = Accessor.GetAttributeData(AttributeInfo, AttributeValues, 0, 1);

	if (bSuccess && AttributeValues.Num() > 0)
	{
		// Fetch the physics material name based off the first primitve attribute
		auto& MaterialName = AttributeValues[0];
		if (!MaterialName.IsEmpty() && MaterialName != "None")
		{
			BodySetup->PhysMaterial = LoadObject<UPhysicalMaterial>(nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr);
			if (!BodySetup->PhysMaterial)
			{
				HOUDINI_LOG_HELPER(Error, TEXT("Physical Material not found: %s."), *MaterialName);
			}
		}
	}

};

bool FHoudiniMeshTranslator::ParseSplitToken(FString & Name, const FString & Token)
{
	// See if the Name matches the token exacly or token followed by an underscore.

	FString UnderscoreToken = Token + TEXT("_");
	if (Name.RemoveFromStart(UnderscoreToken))
	{
		return true;
	}
	else if (Name.Equals(Token, ESearchCase::IgnoreCase))
	{
		Name.Empty();
		return true;
	}
	else
	{
		return false;
	}
}

void FHoudiniMeshTranslator::ClassifySplitGroup(FHoudiniGroupedMeshPrimitives& SplitMeshData)
{
	// This function takes the Houdini name of the split group and classifies as renderable,
	// lod, collision or a combination of all.

	FString Name = SplitMeshData.SplitGroupName;

	bool bFoundRender = false;
	bool bFoundCollision = false;

	SplitMeshData.bRendered = false;
	SplitMeshData.CollisionType = EHoudiniCollisionType::None;

	if (ParseSplitToken(Name, HAPI_UNREAL_GROUP_RENDERED_PREFIX))
	{
		bFoundRender = true;
		SplitMeshData.bIsLOD = false;
		SplitMeshData.bRendered = true;
	}

	if (Name.RemoveFromStart(HAPI_UNREAL_GROUP_LOD_PREFIX))
	{
		bFoundRender = true;
		SplitMeshData.bRendered = true;
		SplitMeshData.bIsLOD = true;

		// New For 2024! Anything after the _ is a split name.
		FString First, Last;
		Name.Split(TEXT("_"), &First, &Last);
		Name = Last;
	}

	if (ParseSplitToken(Name, TEXT("collision_geo")))
	{
		bFoundCollision = true;

		if (ParseSplitToken(Name, "simple"))
		{
			if (ParseSplitToken(Name, "sphere"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::SimpleSphere;
			if (ParseSplitToken(Name, "box"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::SimpleBox;
			if (ParseSplitToken(Name, "capsule"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::SimpleCapsule;
			if (ParseSplitToken(Name, "kdop10x"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::Kdop10x;
			if (ParseSplitToken(Name, "kdop10y"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::Kdop10y;
			if (ParseSplitToken(Name, "xdop10z"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::Kdop10z;
			if (ParseSplitToken(Name, "kdop18"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::Kdop18;
			if (ParseSplitToken(Name, "xkop26"))
				SplitMeshData.CollisionType = EHoudiniCollisionType::Kdop26;
			else
				SplitMeshData.CollisionType = EHoudiniCollisionType::Simple;

			// Look for a numeric string after the tokens, this allows us to have more than one collision per split.
			FString First,Last;
			Name.Split(TEXT("_"), &First, &Last);
			if (First.IsNumeric())
				ParseSplitToken(Name, First);
		}
		else
		{
			if (bFoundRender)
			{
				// render_collision_geo_* means use the collision for the main (render) mesh.
				SplitMeshData.CollisionType = EHoudiniCollisionType::MainMesh;
				SplitMeshData.ComplexCollisionOwner.Empty();
			}
			else
			{
				// collision_geo could mean two things. It could mean "create a second UStaticMesh and use it as a custom collision".
				// But if the user specified "collision_geo" without specifying a rendered_collision_geo then we follow a slightly
				// different path and create a static mesh with the collision geo but mark the component invisible in-game.
				//
				// For now, mark it was as Complex Collision and it will be post-processed.

				SplitMeshData.CollisionType = EHoudiniCollisionType::CustomComplex;
				SplitMeshData.ComplexCollisionOwner = Name;
				SplitMeshData.bRendered = true;
			}
		}
	}

	if (!bFoundRender && !bFoundCollision)
	{
		// If neither render nor collision found then assume the name represents a split mesh.
		SplitMeshData.bRendered = true;
	}

	SplitMeshData.StaticMeshName = Name;
}

void FHoudiniMeshTranslator::AddDefaultMesh(FHoudiniMeshToBuild & MeshesToBuild, const FString& Name)
{
	FHoudiniGroupedMeshPrimitives SplitGroup;
	SplitGroup.CollisionType = EHoudiniCollisionType::MainMesh;
	SplitGroup.bIsLOD  = false;
	SplitGroup.bRendered = true;
	SplitGroup.SplitGroupName = Name;

	FHoudiniSplitGroupMesh Mesh;
	Mesh.SplitMeshData.Add(SplitGroup);
	Mesh.LODRenders.Add(0);

	MeshesToBuild.Meshes.Add(HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION, Mesh);
}

FHoudiniMeshToBuild FHoudiniMeshTranslator::ScanOutputForMeshesToBuild()
{
	UpdateSplitGroups();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Parse all the split group names in the HGPO
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	TArray<FHoudiniGroupedMeshPrimitives> SplitMeshes;

	for (int Index = 0; Index < HGPO.SplitGroups.Num(); ++Index)
	{
		FHoudiniGroupedMeshPrimitives SplitMeshData;
		SplitMeshData.SplitGroupName = HGPO.SplitGroups[Index];
		ClassifySplitGroup(SplitMeshData);
		SplitMeshes.Add(SplitMeshData);
		AllSplitGroups.Add(SplitMeshData.SplitGroupName);
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Now we have all the split groups, group them together per mesh
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	FHoudiniMeshToBuild MeshesToBuild;

	// Go though all split groups and create a FHoudiniSplitGroupMesh entry for each named static mesh. Ignore custom collisions as we need to
	// some custom processing below.
	for(auto & SplitMesh : SplitMeshes)
	{
		if (SplitMesh.CollisionType != EHoudiniCollisionType::CustomComplex)
			MeshesToBuild.Meshes.FindOrAdd(SplitMesh.StaticMeshName);
	}

	// Now go through each complex collision. There are two cases:
	// 1. There is a static mesh already with this name, in which case we generate a second static mesh and link it as a custom complex collision
	//	or
	// 2. There is no static mesh already with this name. So we just create ONE static mesh and do some processing when we create the components to make it invisible.

	for (auto& SplitMesh : SplitMeshes)
	{
		if (SplitMesh.CollisionType == EHoudiniCollisionType::CustomComplex)
		{
			if (MeshesToBuild.Meshes.Contains(SplitMesh.StaticMeshName))
			{
				// case 1 above
				SplitMesh.StaticMeshName += TEXT("custom_complex");
				SplitMesh.bRendered = true;
				MeshesToBuild.Meshes.FindOrAdd(SplitMesh.StaticMeshName);
			}
			else
			{
				// case 2 above
				auto & Mesh =  MeshesToBuild.Meshes.FindOrAdd(SplitMesh.StaticMeshName);
				SplitMesh.ComplexCollisionOwner.Empty();
				Mesh.bIsVisible = false;
			}
		}
	}

	for (auto& SplitMesh : SplitMeshes)
	{
		auto& Mesh = MeshesToBuild.Meshes.FindOrAdd(SplitMesh.StaticMeshName);

		int Index = Mesh.SplitMeshData.Num();
		Mesh.SplitMeshData.Add(SplitMesh);

		if (SplitMesh.bRendered)
		{
			Mesh.LODRenders.Add(Index);
		}

		bool bIsSimple = (SplitMesh.CollisionType != EHoudiniCollisionType::None) && 
						 (SplitMesh.CollisionType != EHoudiniCollisionType::CustomComplex) &&
						 (SplitMesh.CollisionType != EHoudiniCollisionType::MainMesh);

		if (bIsSimple)
		{
			Mesh.SimpleCollisions.Add(Index);
		}
		else if (SplitMesh.CollisionType == EHoudiniCollisionType::CustomComplex)
		{
			if (Mesh.CustomCollisionOwner.IsEmpty())
			{
				Mesh.CustomCollisionOwner = SplitMesh.ComplexCollisionOwner;
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("More than one custom group was found %s %d %d %s, ignoring -- skipping."),
					*HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *SplitMesh.SplitGroupName);
			}
		}
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Sort LODS. The order is determined alphetically, unless the top group is a "rendered_" node
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	for (auto & It : MeshesToBuild.Meshes)
	{
		FHoudiniSplitGroupMesh& Mesh = It.Value;

		Mesh.LODRenders.Sort([&](int a, int b)
		{
			// Sort all groups. Groups that were not classified as "LOD" groups come first. There should be only one of these.

			auto & MeshA = Mesh.SplitMeshData[a];
			auto & MeshB = Mesh.SplitMeshData[b];

			// If both groups are LODs, sort by name.
			if (MeshA.bIsLOD && MeshB.bIsLOD)
			{
				FString& NameA = MeshA.SplitGroupName;
				FString& NameB = MeshB.SplitGroupName;
				return NameA < NameB;
			}

			// If A is not an LOD and B is, A comes first.
			if (!MeshA.bIsLOD)
			{
				return MeshB.bIsLOD;
			}
			return false;
		});

	}

	return MeshesToBuild;
}

FKAggregateGeom FHoudiniMeshTranslator::BuildAggregateCollision(FHoudiniSplitGroupMesh& Mesh)
{
	FKAggregateGeom AggregateCollisions;

	UpdatePartPositionIfNeeded();

	for(int Index : Mesh.SimpleCollisions)
	{
		const FHoudiniGroupedMeshPrimitives& SplitMesh = Mesh.SplitMeshData[Index];
		// Create the simple colliders and add them to the aggregate
		if (!AddSimpleCollisionToAggregate(SplitMesh.SplitGroupName, AggregateCollisions))
		{
			// Failed to generate a convex collider
			HOUDINI_LOG_WARNING(TEXT("failed to create simple collider."));
		}
	}

	return AggregateCollisions;
}

void
FHoudiniMeshTranslator::RemovePreviousOutputs()
{
	for(auto It : InputObjects)
	{
		FHoudiniOutputObject* FoundOutputObject = &It.Value;
		for(auto Component : FoundOutputObject->OutputComponents)
		{
			RemoveAndDestroyComponent(Component);
		}
		FoundOutputObject->OutputComponents.Empty();

		if(IsValid(FoundOutputObject->ProxyComponent))
		{
			RemoveAndDestroyComponent(FoundOutputObject->ProxyComponent);
			FoundOutputObject->ProxyComponent = nullptr;
		}
	}
	InputObjects.Empty();
}

UStaticMesh* 
FHoudiniMeshTranslator::CreateStaticMesh(const FString & MeshName, int NumLODs)
{
	UStaticMesh* StaticMesh = CreateNewUnrealStaticMesh(MeshName);

	if (!IsValid(StaticMesh))
		return nullptr;

	int NeededNumberOfLODs = 1;

	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	FStaticMeshLODGroup LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);

	if (StaticMesh->GetNumSourceModels() != NeededNumberOfLODs)
	{
		while (StaticMesh->GetNumSourceModels() < NeededNumberOfLODs)
			StaticMesh->AddSourceModel();

		// We may have to remove excessive LOD levels
		if (StaticMesh->GetNumSourceModels() > NeededNumberOfLODs)
			StaticMesh->SetNumSourceModels(NeededNumberOfLODs);

		// Initialize their default reduction setting
		for (int32 ModelLODIndex = 0; ModelLODIndex < NeededNumberOfLODs; ModelLODIndex++)
		{
			StaticMesh->GetSourceModel(ModelLODIndex).ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
		}
		StaticMesh->SetLightMapResolution(LODGroup.GetDefaultLightMapResolution());
	}

	FAssetRegistryModule::AssetCreated(StaticMesh);

	return StaticMesh;
}

bool
FHoudiniMeshTranslator::CreateStaticMeshesFromSplitGroups()
{
	RemovePreviousOutputs();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Pull various settings before creating the mesh
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	bDoTiming = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;

	// Update the part's material's IDS and info now
	CreateNeededMaterials();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Get a list of meshes to build.
	// Fetch all part data that is need to generated meshes.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	UpdatePartVertexList();

	//  Get a list of all Static Meshes  to build.
	FHoudiniMeshToBuild MeshesToBuild = FHoudiniMeshTranslator::ScanOutputForMeshesToBuild();
	AllSplitGroups = HGPO.SplitGroups;


	// Builds the corresponding faces and indices arrays. This will also add a new split group if it finds any un-assinged primitives. These
	// are added to the main_geo group,.
	
	if (!UpdateSplitsFacesAndIndices())
		return true;

	// was the main_geo group added?
	if (AllSplitGroups.Num() > HGPO.SplitGroups.Num())
	{
		AddDefaultMesh(MeshesToBuild, AllSplitGroups[AllSplitGroups.Num() - 1]);
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Loop through and build each mesh.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	for (auto & It : MeshesToBuild.Meshes)
	{
		CreateStaticMeshFromSplitGroups(It.Key, It.Value);
	}

	// Once all meshes have been built, patch up custom collision refences
	for (auto& It : MeshesToBuild.Meshes)
	{
		auto & Mesh =  It.Value;
		if (!Mesh.CustomCollisionOwner.IsEmpty())
		{
			auto * Owner = MeshesToBuild.Meshes.Find(Mesh.CustomCollisionOwner);
			if (Owner && Owner->UnrealStaticMesh)
			{
				Owner->UnrealStaticMesh->ComplexCollisionMesh = Mesh.UnrealStaticMesh;
				Owner->UnrealStaticMesh->bCustomizedCollision = true;
			}
		}
	}

	return true;

}

bool
FHoudiniMeshTranslator::CreateStaticMeshFromSplitGroups(const FString& MeshName, FHoudiniSplitGroupMesh& SplitMeshData)
{
	double TimeStart = FPlatformTime::Seconds();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Set up data
	//-----------------------------------------------------------------------------------------------------------------------------------------------
;
	int NumLODs = SplitMeshData.LODRenders.Num();
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bReadTangents = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->RecomputeTangentsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always : true;

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Create a new static mesh. Render, collision & other data will be added to this structure and then StaticMesh->Build() will be called
	// at the end of the function to finalize the mesh.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	SplitMeshData.UnrealStaticMesh = CreateStaticMesh(MeshName, NumLODs);
	if (!IsValid(SplitMeshData.UnrealStaticMesh))
		return false;

	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	FStaticMeshLODGroup LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);

	// Create Output
	SplitMeshData.OutputObjectIdentifier = FHoudiniOutputObjectIdentifier(HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, MeshName);

	FHoudiniOutputObject* OutputObject = &OutputObjects.Add(SplitMeshData.OutputObjectIdentifier, {});
	InputObjects.Remove(SplitMeshData.OutputObjectIdentifier);
	OutputObject->bProxyIsCurrent = false;
	OutputObject->OutputObject = SplitMeshData.UnrealStaticMesh;
	OutputObject->bIsInvisibleCollisionMesh = !SplitMeshData.bIsVisible;



	if (SplitMeshData.UnrealStaticMesh->GetNumSourceModels() != NumLODs)
	{
		while (SplitMeshData.UnrealStaticMesh->GetNumSourceModels() < NumLODs)
			SplitMeshData.UnrealStaticMesh->AddSourceModel();

		// We may have to remove excessive LOD levels
		if (SplitMeshData.UnrealStaticMesh->GetNumSourceModels() > NumLODs)
			SplitMeshData.UnrealStaticMesh->SetNumSourceModels(NumLODs);

		// Initialize their default reduction setting
		for (int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ModelLODIndex++)
		{
			SplitMeshData.UnrealStaticMesh->GetSourceModel(ModelLODIndex).ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
		}
		SplitMeshData.UnrealStaticMesh->SetLightMapResolution(LODGroup.GetDefaultLightMapResolution());
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Build Description based off the Houdini data.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	for(int LODIndex = 0; LODIndex < NumLODs; LODIndex++)
	{

		auto & RenderGroup = SplitMeshData.SplitMeshData[SplitMeshData.LODRenders[LODIndex]];

		RenderGroup.VertexList = AllSplitVertexLists[RenderGroup.SplitGroupName];
		PullMeshData(RenderGroup, SplitMeshData.UnrealStaticMesh, LODIndex, bReadTangents);

		FMeshDescription* MeshDescription = SplitMeshData.UnrealStaticMesh->CreateMeshDescription(LODIndex);
		FStaticMeshAttributes(*MeshDescription).Register();
		BuildMeshDescription(MeshDescription, RenderGroup);

		bool bHasNormal = RenderGroup.Normals.Num() > 0;
		bool bHasTangents = RenderGroup.TangentU.Num() > 0 || RenderGroup.TangentV.Num() > 0;

		// Update the Build Settings using the default setting values
		FStaticMeshSourceModel* SrcModel = (SplitMeshData.UnrealStaticMesh->IsSourceModelValid(LODIndex)) ? &(SplitMeshData.UnrealStaticMesh->GetSourceModel(LODIndex)) : nullptr;
		UpdateMeshBuildSettings(SrcModel->BuildSettings, bHasNormal, bHasTangents, PartUVSets.Num() > 0);

		// Store the new MeshDescription
		SplitMeshData.UnrealStaticMesh->CommitMeshDescription(LODIndex);

		// Set screen size.
		float ScreenSize = GetLODSCreensizeForSplit(RenderGroup.SplitGroupName);
		if (ScreenSize >= 0.0f)
		{
			SrcModel->ScreenSize = ScreenSize;
			SplitMeshData.UnrealStaticMesh->bAutoComputeLODScreenSize = false;
		}

		CopyAttributesFromHGPOForSplit(RenderGroup.SplitGroupName, OutputObject->CachedAttributes, OutputObject->CachedTokens);

		// Update property attributes on the source model
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			HGPO.GeoId,
			HGPO.PartId,
			true,
			SplitMeshData.OutputObjectIdentifier.PrimitiveIndex,
			INDEX_NONE,
			SplitMeshData.OutputObjectIdentifier.PointIndex,
			PropertyAttributes))
		{
			auto FindPropertyOnSourceModelLamba = [LODIndex](UObject* const InObject, const FString& InPropertyName, bool& bOutSkipDefaultIfPropertyNotFound, FEditPropertyChain& InPropertyChain, FProperty*& OutFoundProperty, UObject*& OutFoundPropertyObject, void*& OutContainer)
			{
				if (!IsValid(InObject))
					return false;

				UStaticMesh* const SM = Cast<UStaticMesh>(InObject);
				if (!IsValid(SM))
					return false;

				return TryToFindPropertyOnSourceModel(
					SM, LODIndex, InPropertyName, InPropertyChain, bOutSkipDefaultIfPropertyNotFound, OutFoundProperty, OutFoundPropertyObject, OutContainer);
			};

			// Defer post edit change calls until after all property values have been set, since the static mesh
			// build function is called from PostEditChangeProperty.
			constexpr bool bDeferPostEditChangePropertyCalls = true;
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
				SplitMeshData.UnrealStaticMesh, PropertyAttributes, 0, bDeferPostEditChangePropertyCalls, FindPropertyOnSourceModelLamba);
		}

	}


	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Set various custom settings.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	// Set the lightmap Coordinate Index. If we have more than one UV set, the 2nd valid set is used for lightmaps by convention
	SplitMeshData.UnrealStaticMesh->SetLightMapCoordinateIndex(PartUVSets.Num() > 1 ? 1 : 0);

	// Check for a lightmapa resolution override.
	if (PartLightMapResolutions.Num() > 0 && PartLightMapResolutions[0] > 0)
		SplitMeshData.UnrealStaticMesh->SetLightMapResolution(PartLightMapResolutions[0]);
	else
		SplitMeshData.UnrealStaticMesh->SetLightMapResolution(64);

	// Nananite settings.
	UpdateStaticMeshNaniteSettings(HGPO.GeoId, HGPO.PartId, SplitMeshData.OutputObjectIdentifier.PrimitiveIndex, SplitMeshData.UnrealStaticMesh);

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Build collision
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	UBodySetup* BodySetup = SplitMeshData.UnrealStaticMesh->GetBodySetup();
	if (!BodySetup)
	{
		SplitMeshData.UnrealStaticMesh->CreateBodySetup();
		BodySetup = SplitMeshData.UnrealStaticMesh->GetBodySetup();
	}

	if (IsValid(BodySetup))
	{
		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		// Clean up old colliders from a previous cook
		BodySetup->Modify();
		BodySetup->RemoveSimpleCollision();

		FKAggregateGeom CollisionGeometry = BuildAggregateCollision(SplitMeshData);

		if (CollisionGeometry.GetElementCount() > 0)
		{
			BodySetup->AddCollisionFrom(CollisionGeometry);
			BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
		}

		SetPhysicsMaterialFromHGPO(BodySetup);

		if (FHoudiniEngineUtils::HapiCheckAttributeExists(HGPO.GeoId, HGPO.PartId,
			"unreal_uproperty_LODForCollision", HAPI_ATTROWNER_DETAIL))
		{
			BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		}
	}

	// If this is a custom collision object, mark it as implicit so that it doesn't get an actor created.
	if (!SplitMeshData.CustomCollisionOwner.IsEmpty())
	{
		OutputObject->bIsImplicit = true;
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Finalize mesh
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	double BuildTimeStart = FPlatformTime::Seconds();

	TArray<FText> SMBuildErrors;
	SplitMeshData.UnrealStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	SplitMeshData.UnrealStaticMesh->Build(true, &SMBuildErrors);

	for (FThreadSafeObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(*Iter);
		if (StaticMeshComponent->GetStaticMesh() == SplitMeshData.UnrealStaticMesh)
		{
			// it needs to recreate IF it already has been created
			if (StaticMeshComponent->IsPhysicsStateCreated())
			{
				StaticMeshComponent->RecreatePhysicsState();
			}
		}
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

	SplitMeshData.UnrealStaticMesh->GetOnMeshChanged().Broadcast();

	UPackage* MeshPackage = SplitMeshData.UnrealStaticMesh->GetOutermost();
	if (IsValid(MeshPackage))
	{
		MeshPackage->MarkPackageDirty();
	}

	double BuildTimeEnd = FPlatformTime::Seconds();
	if (bDoTiming)
		HOUDINI_LOG_MESSAGE(TEXT("StaticMesh->Build() executed in %f seconds."), BuildTimeEnd - BuildTimeStart);

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Print results.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	double TimeEnd = FPlatformTime::Seconds();
	if (bDoTiming)
		HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMeshFromSplitGroups() executed in %f seconds."), TimeEnd - TimeStart);

	return true;
}

void FHoudiniMeshTranslator::UpdateSplitGroups()
{
	// The old code (per-split groups) uses slightly different conditions to fill in the HGPO.SplitGroups. This function
	// fetches the groups using the new method.

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HAPI_Result Error = HAPI_RESULT_FAILURE;
	Error = FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), HGPO.GeoId, HGPO.PartId, &PartInfo);

	TArray<FString> GroupNames;
	if (!FHoudiniEngineUtils::HapiGetGroupNames(HGPO.GeoId, HGPO.PartId, HAPI_GROUPTYPE_PRIM, PartInfo.isInstanced, GroupNames))
	{
		return;
	}

	TArray<FString> Results;

	for (const FString& GroupName : GroupNames)
	{
		if (GroupName.StartsWith(HAPI_UNREAL_GROUP_LOD_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_INVISIBLE_COLLISION_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_RENDERED_COLLISION_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_RENDERED_PREFIX, ESearchCase::IgnoreCase))
		{
			// Split by collisions / lods
			Results.Add(GroupName);
		}
	}

	HGPO.SplitGroups = Results;
}

bool
FHoudiniMeshTranslator::CreateHoudiniStaticMeshesFromSplitGroups()
{
	RemovePreviousOutputs();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Pull various settings before creating the mesh
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	bDoTiming = CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;

	// Update the part's material's IDS and info now
	CreateNeededMaterials();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Get a list of meshes to build.
	// Fetch all part data that is need to generated meshes.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	UpdatePartVertexList();

	//  Get a list of all Static Meshes  to build.
	FHoudiniMeshToBuild MeshesToBuild = FHoudiniMeshTranslator::ScanOutputForMeshesToBuild();
	AllSplitGroups = HGPO.SplitGroups;


	// Builds the corresponding faces and indices arrays. This will also add a new split group if it finds any un-assinged primitives. These
	// are added to the main_geo group,.

	if (!UpdateSplitsFacesAndIndices())
		return true;

	// was the main_geo group added?
	if (AllSplitGroups.Num() > HGPO.SplitGroups.Num())
	{
		AddDefaultMesh(MeshesToBuild, AllSplitGroups[AllSplitGroups.Num() - 1]);
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Loop through and build each mesh.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	TMap<HAPI_NodeId, UMaterialInterface*> MapHoudiniMatIdToUnrealInterface;
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> MapHoudiniMatAttributesToUnrealInterface;
	TMap<UHoudiniStaticMesh*, TMap<UMaterialInterface*, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	for (auto& It : MeshesToBuild.Meshes)
	{
		CreateHoudiniStaticMeshFromSplitGroups(It.Key, It.Value, MapHoudiniMatIdToUnrealInterface, MapHoudiniMatAttributesToUnrealInterface, MapUnrealMaterialInterfaceToUnrealIndexPerMesh);
	}

	// Once all meshes have been built, patch up custom collision refences
	for (auto& It : MeshesToBuild.Meshes)
	{
		auto& Mesh = It.Value;
		if (!Mesh.CustomCollisionOwner.IsEmpty())
		{
			auto* Owner = MeshesToBuild.Meshes.Find(Mesh.CustomCollisionOwner);
			if (Owner && Owner->UnrealStaticMesh)
			{
				Owner->UnrealStaticMesh->ComplexCollisionMesh = Mesh.UnrealStaticMesh;
				Owner->UnrealStaticMesh->bCustomizedCollision = true;
			}
		}
	}

	return true;

}


bool
FHoudiniMeshTranslator::CreateHoudiniStaticMeshFromSplitGroups(const FString& MeshName, FHoudiniSplitGroupMesh& SplitMeshData,
	TMap<HAPI_NodeId, UMaterialInterface*> & MapHoudiniMatIdToUnrealInterface,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> & MapHoudiniMatAttributesToUnrealInterface,
	TMap<UHoudiniStaticMesh*, TMap<UMaterialInterface*, int32>> & MapUnrealMaterialInterfaceToUnrealIndexPerMesh)
{
	double tick = FPlatformTime::Seconds();

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMeshFromSplitGroups -- Per Split);

	// Houdini Static Meshes only create a mesh for the top LOD.
	if (SplitMeshData.LODRenders.Num() == 0)
		return true;

	FHoudiniGroupedMeshPrimitives & Group =  SplitMeshData.SplitMeshData[SplitMeshData.LODRenders[0]];

	FString & SplitGroupName = Group.SplitGroupName;

	// Get the vertex indices for this group
	TArray<int32>& SplitVertexList = AllSplitVertexLists[SplitGroupName];

	// Get valid count of vertex indices for this split.
	const int32& SplitVertexCount = AllSplitVertexCounts[SplitGroupName];

	// Make sure we have a valid vertex count for this split
	if (SplitVertexCount % 3 != 0 || SplitVertexList.Num() % 3 != 0)
	{
		// Invalid vertex count, skip this split or we'd crash trying to create a mesh for it.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%s] invalid vertex count.")
			TEXT("- skipping."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, *SplitGroupName);

		return true;
	}

	// Get the output identifer for this split
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier = FHoudiniOutputObjectIdentifier(HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, MeshName);

	// Try to find existing properties for this identifier
	FHoudiniOutputObject* FoundOutputObject = InputObjects.Find(OutputObjectIdentifier);

	// If we don't yet have package params for this object identifier, fetch and resolve attributes for the split
	// and update the package params
	TMap<FString, FString> TempAttributes;
	TMap<FString, FString> TempTokens;
	bool bCopyAttributesAndTokens = false;

	// Get all the supported attributes from the HGPO
	CopyAttributesFromHGPOForSplit(OutputObjectIdentifier, TempAttributes, TempTokens);

	// Resolve our final package params
	FHoudiniAttributeResolver Resolver;
	FHoudiniPackageParams FinalPackageParams;
	FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
		PackageParams,
		IsValid(OuterComponent) ? OuterComponent->GetWorld() : nullptr,
		OuterComponent,
		TempAttributes,
		TempTokens,
		PackageParams,
		Resolver);

	bCopyAttributesAndTokens = true;


	// Try to find an existing DM from a previous cook
	UHoudiniStaticMesh* FoundStaticMesh = CreateNewHoudiniStaticMesh(OutputObjectIdentifier.SplitIdentifier);

	if (!FoundOutputObject)
	{
		// If we couldnt find a previous output object, create a new one
		FHoudiniOutputObject NewOutputObject;
		FoundOutputObject = &OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
	}
	FoundOutputObject->bProxyIsCurrent = true;

	// Update the attributes and tokens if this is the first split for this object identifier
	if (bCopyAttributesAndTokens)
	{
		FoundOutputObject->CachedAttributes = MoveTemp(TempAttributes);
		FoundOutputObject->CachedTokens = MoveTemp(TempTokens);
	}

	if (bDoTiming)
	{
		HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh() - PreBuildMesh in %f seconds."), FPlatformTime::Seconds() - tick);
		tick = FPlatformTime::Seconds();
	}

	BuildHoudiniMesh(SplitGroupName, FoundStaticMesh);

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIALS / FACE MATERIALS
	//---------------------------------------------------------------------------------------------------------------------

	ProcessMaterialsForHSM(SplitGroupName, FoundStaticMesh, MapHoudiniMatIdToUnrealInterface, MapHoudiniMatAttributesToUnrealInterface, MapUnrealMaterialInterfaceToUnrealIndexPerMesh);

	//// Update property attributes on the mesh
	//TArray<FHoudiniGenericAttribute> PropertyAttributes;
	//if (GetGenericPropertiesAttributes(
	//	HGPO.GeoId, HGPO.PartId,
	//	AllSplitFirstValidVertexIndex[SplitGroupName],
	//	AllSplitFirstValidPrimIndex[SplitGroupName],
	//	PropertyAttributes))
	//{
	//	UpdateGenericPropertiesAttributes(
	//		FoundStaticMesh, PropertyAttributes);
	//}

	FoundStaticMesh->Optimize();

	// Check if the mesh is valid (check all the counts (vertex, triangles, vertex instances, UVs etc) but skip
	// looping over each individual triangle vertex index to check if the value is valid).
	const bool bSkipVertexIndicesCheck = true;
	if (!FoundStaticMesh->IsValid(bSkipVertexIndicesCheck))
	{
		HOUDINI_LOG_WARNING(
			TEXT("[CreateHoudiniStaticMesh]: Invalid StaticMesh data for %s in cook output! Please check the log."),
			*FoundStaticMesh->GetName());
	}

	//// Try to find the outer package so we can dirty it up
	//if (FoundStaticMesh->GetOuter())
	//{
	//	FoundStaticMesh->GetOuter()->MarkPackageDirty();
	//}
	//else
	//{
	//	FoundStaticMesh->MarkPackageDirty();
	//}
	UPackage* MeshPackage = FoundStaticMesh->GetOutermost();
	if (IsValid(MeshPackage))
	{
		MeshPackage->MarkPackageDirty();

		/*
		// DPT: deactivated auto saving mesh/material package
		// only dirty for now, as we'll save them when saving the world.
		// Save the created/updated package
		FEditorFileUtils::PromptForCheckoutAndSave({ MeshPackage }, false, false);
		*/
	}

	// Add the Proxy mesh to the output maps
	if (FoundOutputObject)
	{
		FoundOutputObject->ProxyObject = FoundStaticMesh;
		FoundOutputObject->bProxyIsCurrent = true;
		OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
	}
	return true;
}

void FHoudiniMeshTranslator::BuildHoudiniMesh(const FString& SplitGroupName, UHoudiniStaticMesh* FoundStaticMesh)
{
	// Get the vertex indices for this group
	TArray<int32>& SplitVertexList = AllSplitVertexLists[SplitGroupName];

	// Get valid count of vertex indices for this split.
	const int32& SplitVertexCount = AllSplitVertexCounts[SplitGroupName];

	// Make sure we have a valid vertex count for this split
	if (SplitVertexCount % 3 != 0 || SplitVertexList.Num() % 3 != 0)
	{
		// Invalid vertex count, skip this split or we'd crash trying to create a mesh for it.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%s] invalid vertex count.")
			TEXT("- skipping."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, *SplitGroupName);

		return;
	}

	// WONWON

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Build/Rebuild UHoudiniStaticMesh);

	//--------------------------------------------------------------------------------------------------------------------- 
	//  INDICES
	//--------------------------------------------------------------------------------------------------------------------- 

	//
	// Because of the splits, we don't need to declare all the vertices in the Part, 
	// but only the one that are currently used by the split's faces.
	// The indicesMapper array is used to map those indices from Part Vertices to Split Vertices.
	// We also keep track of the needed vertices index to declare them easily afterwards.
	//

	// IndicesMapper:
	// Maps index values for all vertices in the Part:
	// - Vertices unused by the split will be set to -1
	// - Used vertices will have their value set to the "NewIndex"
	// So that IndicesMapper[ oldIndex ] => newIndex
	TArray<int32> IndicesMapper;
	IndicesMapper.SetNumUninitialized(SplitVertexList.Num());
	for (int32 n = 0; n < IndicesMapper.Num(); n++)
		IndicesMapper[n] = -1;

	int32 CurrentMapperIndex = 0;

	// NeededVertices:
	// Array containing the old index of the needed vertices for the current split
	// NeededVertices[ newIndex ] => oldIndex
	TArray< int32 > NeededVertices;
	NeededVertices.Reserve(SplitVertexList.Num() / 3);
	TArray< int32 > TriangleIndices;
	TriangleIndices.Reserve(SplitVertexList.Num());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Build IndicesMapper and NeededVertices);

		bool bHasInvalidFaceIndices = false;
		int32 ValidVertexId = 0;
		for (int32 VertexIdx = 0; VertexIdx < SplitVertexList.Num(); VertexIdx += 3)
		{
			int32 WedgeCheck = SplitVertexList[VertexIdx + 0];
			if (WedgeCheck == -1)
				continue;

			int32 WedgeIndices[3] =
			{
				SplitVertexList[VertexIdx + 0],
				SplitVertexList[VertexIdx + 1],
				SplitVertexList[VertexIdx + 2]
			};

			// Ensure the indices are valid
			if (!IndicesMapper.IsValidIndex(WedgeIndices[0])
				|| !IndicesMapper.IsValidIndex(WedgeIndices[1])
				|| !IndicesMapper.IsValidIndex(WedgeIndices[2]))
			{
				// Invalid face index. Don't log in the loop.
				bHasInvalidFaceIndices = true;
				continue;
			}

			// Converting Old (Part) Indices to New (Split) Indices:
			for (int32 i = 0; i < 3; i++)
			{
				if (IndicesMapper[WedgeIndices[i]] < 0)
				{
					// This old index has not yet been "converted" to a new index
					NeededVertices.Add(WedgeIndices[i]);
					IndicesMapper[WedgeIndices[i]] = CurrentMapperIndex;
					CurrentMapperIndex++;
				}

				// Replace the old index with the new one
				WedgeIndices[i] = IndicesMapper[WedgeIndices[i]];
			}

			// Flip wedge indices to fix the winding order.
			TriangleIndices.Add(WedgeIndices[0]);
			TriangleIndices.Add(WedgeIndices[2]);
			TriangleIndices.Add(WedgeIndices[1]);

			ValidVertexId += 3;
		}

		if (bHasInvalidFaceIndices)
		{
			HOUDINI_LOG_MESSAGE(
				TEXT("Creating Dynamic Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%s] has some invalid face indices"),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, *SplitGroupName);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// NORMALS 
	//--------------------------------------------------------------------------------------------------------------------- 

	// Extract this part's normal if needed
	UpdatePartNormalsIfNeeded();

	// Get the normals for this split
	TArray<float> SplitNormals;
	FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
		SplitVertexList, AttribInfoNormals, PartNormals, SplitNormals);

	// Check that the number of normal we retrieved is correct
	int32 NormalCount = SplitNormals.Num() / 3;
	if (NormalCount < 0 || NormalCount < NeededVertices.Num())
	{
		// Ignore normals
		NormalCount = 0;
		if (SplitNormals.Num() != 0)
			HOUDINI_LOG_WARNING(TEXT("Invalid normal count detected - Skipping normals."));
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// TANGENTS
	//--------------------------------------------------------------------------------------------------------------------- 

	TArray<float> SplitTangentU;
	TArray<float> SplitTangentV;
	int32 TangentUCount = 0;
	int32 TangentVCount = 0;
	// No need to read the tangents if we want unreal to recompute them after		
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bReadTangents = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->RecomputeTangentsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always : true;

	bool bGenerateTangentsFromNormalAttribute = false;
	if (bReadTangents)
	{
		// Extract this part's Tangents if needed
		UpdatePartTangentsIfNeeded();

		// Get the Tangents for this split
		FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
			SplitVertexList, AttribInfoTangentU, PartTangentU, SplitTangentU);

		// Get the binormals for this split
		FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
			SplitVertexList, AttribInfoTangentV, PartTangentV, SplitTangentV);

		if ((SplitTangentU.Num() <= 0 || SplitTangentV.Num() <= 0))
			bReadTangents = false;

		// We need to manually generate tangents if:
		// - we have normals but dont have tangentu or tangentv attributes
		// - we have not specified that we wanted unreal to generate them
		bGenerateTangentsFromNormalAttribute = (NormalCount > 0) && !bReadTangents;

		// Check that the number of tangents read matches the number of normals
		TangentUCount = SplitTangentU.Num() / 3;
		TangentVCount = SplitTangentV.Num() / 3;
		if (NormalCount > 0 && (TangentUCount != NormalCount || TangentVCount != NormalCount))
		{
			HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh: Generate tangents due to count mismatch (# U Tangents = %d; # V Tangents = %d; # Normals = %d)"), TangentUCount, TangentVCount, NormalCount);
			bGenerateTangentsFromNormalAttribute = true;
			bReadTangents = false;
		}

		if (bGenerateTangentsFromNormalAttribute && (HoudiniRuntimeSettings->RecomputeTangentsFlag == EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always))
		{
			// No need to generate tangents if we want unreal to recompute them after
			bGenerateTangentsFromNormalAttribute = false;
		}
	}
	else
	{
		bGenerateTangentsFromNormalAttribute = (NormalCount > 0);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	//  VERTEX COLORS AND ALPHAS
	//---------------------------------------------------------------------------------------------------------------------

	// Extract this part's colors if needed
	UpdatePartColorsIfNeeded();

	// Get the colors values for this split
	TArray<float> SplitColors;
	FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
		SplitVertexList, AttribInfoColors, PartColors, SplitColors);

	// Extract this part's alpha values if needed
	UpdatePartAlphasIfNeeded();

	// Get the colors values for this split
	TArray<float> SplitAlphas;
	FHoudiniMeshTranslator::TransferRegularPointAttributesToVertices(
		SplitVertexList, AttribInfoAlpha, PartAlphas, SplitAlphas);

	const int32 ColorsCount = AttribInfoColors.exists ? SplitColors.Num() / AttribInfoColors.tupleSize : 0;
	const bool bSplitColorValid = AttribInfoColors.exists && (AttribInfoColors.tupleSize >= 3) && ColorsCount > 0;
	const bool bSplitAlphaValid = AttribInfoAlpha.exists && (SplitAlphas.Num() == ColorsCount);

	//--------------------------------------------------------------------------------------------------------------------- 
	//  UVS
	//--------------------------------------------------------------------------------------------------------------------- 

	// Extract this part's UV sets if needed
	UpdatePartUVSetsIfNeeded();

	// See if we need to transfer uv point attributes to vertex attributes.
	int32 NumUVLayers = 0;
	TArray<TArray<float>> SplitUVSets;
	SplitUVSets.SetNum(MAX_STATIC_TEXCOORDS);
	for (int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
	{
		FHoudiniMeshTranslator::TransferPartAttributesToSplit<float>(
			SplitVertexList, AttribInfoUVSets[TexCoordIdx], PartUVSets[TexCoordIdx], SplitUVSets[TexCoordIdx]);
		if (SplitUVSets[TexCoordIdx].Num() > 0)
		{
			NumUVLayers++;
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIAL ATTRIBUTE OVERRIDES
	//---------------------------------------------------------------------------------------------------------------------

	// TODO: These are actually per faces, not per vertices...
	// Need to update!!
	UpdatePartFaceMaterialOverridesIfNeeded();

	//
	// Initialize mesh
	// 
	const int32 NumVertexPositions = NeededVertices.Num();
	const int32 NumTriangles = TriangleIndices.Num() / 3;
	const bool bHasPerFaceMaterials = PartFaceMaterialOverrides.Num() > 0 || (PartUniqueMaterialIds.Num() > 0 && !bOnlyOneFaceMaterial);

	FoundStaticMesh->Initialize(
		NumVertexPositions,
		NumTriangles,
		NumUVLayers,											   // NumUVLayers
		0,														   // InitialNumStaticMaterials
		NormalCount > 0,										   // HasNormals
		bReadTangents || bGenerateTangentsFromNormalAttribute,	   // HasTangents
		bSplitColorValid,										   // HasColors
		bHasPerFaceMaterials									   // HasPerFaceMaterials
	);

	//--------------------------------------------------------------------------------------------------------------------- 
	// POSITIONS
	//--------------------------------------------------------------------------------------------------------------------- 
	UpdatePartPositionIfNeeded();

	//
	// Transfer vertex positions:
	//
	// Because of the split, we're only interested in the needed vertices.
	// Instead of declaring all the Positions, we'll only declare the vertices
	// needed by the current split.
	//
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Vertex Positions);

		bool bHasInvalidPositionIndexData = false;
		for (int32 VertexPositionIdx = 0; VertexPositionIdx < NumVertexPositions; ++VertexPositionIdx)
			//ParallelFor(NumVertexPositions, [&](uint32 VertexPositionIdx)
		{
			int32 NeededVertexIndex = NeededVertices[VertexPositionIdx];
			if (!PartPositions.IsValidIndex(NeededVertexIndex * 3 + 2))
			{
				// Error retrieving positions.
				bHasInvalidPositionIndexData = true;
				continue;
			}

			// We need to swap Z and Y coordinate here, and convert from m to cm. 
			FoundStaticMesh->SetVertexPosition(VertexPositionIdx, FVector3f(
				PartPositions[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
				PartPositions[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
				PartPositions[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION
			));
		}//);

		if (bHasInvalidPositionIndexData)
		{
			HOUDINI_LOG_WARNING(
				TEXT("Creating Dynamic Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%s] invalid position/index data ")
				TEXT("- skipping."),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, *SplitGroupName);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// FACES / TRIS
	// Now set Normals, UVs and Colors on mesh points and AttributeSet
	//---------------------------------------------------------------------------------------------------------------------

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Triangle Indices & Per Vertex Instance Attribute Values);

		// Now add the triangles to the mesh
		for (int32 TriangleIdx = 0; TriangleIdx < NumTriangles; ++TriangleIdx)
			// ParallelFor(NumTriangles, [&](uint32 TriangleIdx)
		{
			// TODO: add some additional intermediate consts for index calculations to make the indexing
			// TODO: code a bit more readable
			const int32 TriVertIdx0 = TriangleIdx * 3;
			FoundStaticMesh->SetTriangleVertexIndices(TriangleIdx, FIntVector(
				TriangleIndices[TriVertIdx0 + 0],
				TriangleIndices[TriVertIdx0 + 1],
				TriangleIndices[TriVertIdx0 + 2]
			));

			const int32 TriWindingIndex[3] = { 0, 2, 1 };
			// Normals and tangents (either getting tangents from attributes or generating tangents from the
			// normals
			if (NormalCount > 0 || bReadTangents)
			{
				for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
				{
					const bool bHasNormal = (NormalCount > 0 && SplitNormals.IsValidIndex(TriVertIdx0 * 3 + 3 * 3 - 1));
					FVector3f Normal = FVector3f::ZeroVector;
					if (bHasNormal)
					{
						// Flip Z and Y coordinate for normal, but don't scale
						Normal.Set(
							SplitNormals[TriVertIdx0 * 3 + 3 * ElementIdx + 0],
							SplitNormals[TriVertIdx0 * 3 + 3 * ElementIdx + 2],
							SplitNormals[TriVertIdx0 * 3 + 3 * ElementIdx + 1]
						);

						FoundStaticMesh->SetTriangleVertexNormal(TriangleIdx, TriWindingIndex[ElementIdx], Normal);
					}

					if (bReadTangents || bGenerateTangentsFromNormalAttribute)
					{
						FVector3f TangentU, TangentV;
						if (bGenerateTangentsFromNormalAttribute)
						{
							if (bHasNormal)
							{
								// Generate the tangents if needed
								Normal.FindBestAxisVectors(TangentU, TangentV);

								FoundStaticMesh->SetTriangleVertexUTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentU);
								FoundStaticMesh->SetTriangleVertexVTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentV);
							}
						}
						else
						{
							// Transfer the tangents from Houdini
							TangentU.X = SplitTangentU[TriVertIdx0 * 3 + 3 * ElementIdx + 0];
							TangentU.Y = SplitTangentU[TriVertIdx0 * 3 + 3 * ElementIdx + 2];
							TangentU.Z = SplitTangentU[TriVertIdx0 * 3 + 3 * ElementIdx + 1];

							TangentU.X = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 0];
							TangentU.Y = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 2];
							TangentU.Z = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 1];

							FoundStaticMesh->SetTriangleVertexUTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentU);
							FoundStaticMesh->SetTriangleVertexVTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentV);
						}
					}
				}
			}

			// Vertex Colors
			if (bSplitColorValid && SplitColors.IsValidIndex(TriVertIdx0 * AttribInfoColors.tupleSize + 3 * AttribInfoColors.tupleSize - 1))
			{
				FLinearColor VertexLinearColor;
				for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
				{
					VertexLinearColor.R = FMath::Clamp(
						SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 0], 0.0f, 1.0f);
					VertexLinearColor.G = FMath::Clamp(
						SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 1], 0.0f, 1.0f);
					VertexLinearColor.B = FMath::Clamp(
						SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 2], 0.0f, 1.0f);

					if (bSplitAlphaValid)
					{
						VertexLinearColor.A = FMath::Clamp(SplitAlphas[TriVertIdx0 + ElementIdx], 0.0f, 1.0f);
					}
					else if (AttribInfoColors.tupleSize >= 4)
					{
						VertexLinearColor.A = FMath::Clamp(
							SplitColors[TriVertIdx0 * AttribInfoColors.tupleSize + AttribInfoColors.tupleSize * ElementIdx + 3], 0.0f, 1.0f);
					}
					else
					{
						VertexLinearColor.A = 1.0f;
					}
					const FColor VertexColor = VertexLinearColor.ToFColor(false);
					FoundStaticMesh->SetTriangleVertexColor(TriangleIdx, TriWindingIndex[ElementIdx], VertexColor);
				}
			}

			// UVs
			if (NumUVLayers > 0)
			{
				// Dynamic mesh supports only 1 UV layer on the mesh it self. So we set the first layer
				// on the mesh itself only, and we set all layers on the AttributeSet
				for (int32 TexCoordIdx = 0; TexCoordIdx < NumUVLayers; ++TexCoordIdx)
				{
					const TArray<float>& SplitUVs = SplitUVSets[TexCoordIdx];
					if (SplitUVs.IsValidIndex(TriVertIdx0 * 2 + 3 * 2 - 1))
					{
						for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
						{
							const int32 UVIdx = TriVertIdx0 * 2 + ElementIdx * 2;
							// We need to flip V coordinate when it's coming from HAPI.
							const FVector2f UV(SplitUVs[UVIdx + 0], 1.0f - SplitUVs[UVIdx + 1]);
							// Set the UV on the vertex instance in the UVLayer
							FoundStaticMesh->SetTriangleVertexUV(TriangleIdx, TriWindingIndex[ElementIdx], TexCoordIdx, UV);
						}
					}
				}
			}
		}
	}

	FMeshBuildSettings BuildSettings;
	UpdateMeshBuildSettings(
		BuildSettings,
		FoundStaticMesh->HasNormals(),
		FoundStaticMesh->HasTangents(),
		false);
	// Compute normals if requested or needed/missing
	if (BuildSettings.bRecomputeNormals)
	{
		FoundStaticMesh->CalculateNormals(BuildSettings.bComputeWeightedNormals);
	}

	// Compute tangents if requested or needed/missing
	if (BuildSettings.bRecomputeTangents)
	{
		FoundStaticMesh->CalculateTangents(BuildSettings.bComputeWeightedNormals);
	}
}

void
FHoudiniMeshTranslator::ProcessMaterialsForHSM(
	const FString& SplitGroupName, 
	UHoudiniStaticMesh* FoundStaticMesh,
	TMap<HAPI_NodeId, UMaterialInterface*> & MapHoudiniMatIdToUnrealInterface,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> & MapHoudiniMatAttributesToUnrealInterface,
	TMap<UHoudiniStaticMesh*, TMap<UMaterialInterface*, int32>> & MapUnrealMaterialInterfaceToUnrealIndexPerMesh)
{
	// Get face indices for this split.
	TArray<int32>& SplitFaceIndices = AllSplitFaceIndices[SplitGroupName];

	// Fetch the FoundMesh's Static Materials array
	TArray<FStaticMaterial>& FoundStaticMaterials = FoundStaticMesh->GetStaticMaterials();

	// Clear the materials array of the mesh the first time we encounter it
	if (!MapUnrealMaterialInterfaceToUnrealIndexPerMesh.Contains(FoundStaticMesh))
	{
		FoundStaticMaterials.Empty();
	}
	TMap<UMaterialInterface*, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

	// Process material overrides first
	if (PartFaceMaterialOverrides.Num() > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Per Face Material Overrides);

		// Array used to avoid constantly attempting to load invalid materials
		TArray<FHoudiniMaterialIdentifier> InvalidMaterials;

		for (int32 FaceIdx = 0; FaceIdx < SplitFaceIndices.Num(); ++FaceIdx)
		{
			int32 SplitFaceIndex = SplitFaceIndices[FaceIdx];
			if (!PartFaceMaterialOverrides.IsValidIndex(SplitFaceIndex))
				continue;

			UMaterialInterface* MaterialInterface = nullptr;
			int32 CurrentFaceMaterialIdx = 0;
			const FHoudiniMaterialInfo& MatInfo = PartFaceMaterialOverrides[SplitFaceIndex];
			const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
			UMaterialInterface** FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
			if (FoundMaterialInterface)
				MaterialInterface = *FoundMaterialInterface;

			if (!MaterialInterface)
			{
				// Try to locate the corresponding material interface

				// Start by looking in our assignment map
				FoundMaterialInterface = OutputAssignmentMaterials.Find(MaterialIdentifier);
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				// Only try to load a material if it has a chance to be valid!
				if (!MaterialInterface && MaterialIdentifier.IsValid() && !InvalidMaterials.Contains(MaterialIdentifier))
				{
					MaterialInterface = Cast<UMaterialInterface>(
						StaticLoadObject(UMaterialInterface::StaticClass(),
							nullptr, *MaterialIdentifier.MaterialObjectPath, nullptr, LOAD_NoWarn, nullptr));

					if (!MaterialInterface)
						InvalidMaterials.Add(MaterialIdentifier);
				}

				if (MaterialInterface)
				{
					// We managed to load the UE4 material
					// Make sure this material is in the assignments before replacing it.
					OutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

					// See if we have a replacement material and use it on the mesh instead
					UMaterialInterface* const* ReplacementMaterialInterface = ReplacementMaterials.Find(MaterialIdentifier);
					if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
						MaterialInterface = *ReplacementMaterialInterface;

					// Add this material to the map
					MapHoudiniMatAttributesToUnrealInterface.Add(MaterialIdentifier, MaterialInterface);
				}
				else
				{
					// The Attribute Material and its replacement do not exist
					// See if we can fallback to the Houdini material assigned on the face

					// Get the unreal material corresponding to this houdini one
					HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

					// See if we have already treated that material
					FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
					if (FoundMaterialInterface)
						MaterialInterface = *FoundMaterialInterface;
					if (!MaterialInterface)
					{
						// If everything fails, we'll use the default material
						MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

						// We need to add this material to the map
						FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
						const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
						const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
						UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
						if (FoundMaterial)
							MaterialInterface = *FoundMaterial;

						// See if we have a replacement material and use it on the mesh instead
						UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
						if (ReplacementMaterial && *ReplacementMaterial)
							MaterialInterface = *ReplacementMaterial;

						// Map the Houdini ID to the unreal one
						MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
					}
				}
			}

			if (MaterialInterface)
			{
				CurrentFaceMaterialIdx = GetFaceMaterialIndex(MaterialInterface,
					MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
					MatInfo.MaterialIndex,
					FoundStaticMaterials);
				// Update the Face Material on the mesh
				FoundStaticMesh->SetTriangleMaterialID(FaceIdx, CurrentFaceMaterialIdx);
			}
		}
	}
	else if (PartUniqueMaterialIds.Num() > 0)
	{
		// The part has houdini materials
		if (bOnlyOneFaceMaterial)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Single Material);

			// Use default Houdini material if no valid material is assigned to any of the faces.
			UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

			// Get id of this single material.
			FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, PartFaceMaterialIds[0], MaterialPathName);
			const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
			UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
			if (FoundMaterial)
				MaterialInterface = *FoundMaterial;

			// See if we have a replacement material and use it on the mesh instead
			UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
			if (ReplacementMaterial && *ReplacementMaterial)
				MaterialInterface = *ReplacementMaterial;

			FoundStaticMaterials.Empty();
			FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Per Face Materials);

			// We have multiple houdini materials
			// Get default Houdini material.
			UMaterial* DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get();

			for (int32 FaceIdx = 0; FaceIdx < SplitFaceIndices.Num(); ++FaceIdx)
			{
				int32 SplitFaceIndex = SplitFaceIndices[FaceIdx];
				if (!PartFaceMaterialIds.IsValidIndex(SplitFaceIndex))
					continue;

				// Get material id for this face.
				HAPI_NodeId MaterialId = PartFaceMaterialIds[SplitFaceIndex];

				// See if we have already treated that material
				UMaterialInterface** FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
				UMaterialInterface* MaterialInterface = nullptr;
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				if (MaterialInterface)
				{
					int32 const* FoundUnrealMatIndex = MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Find(MaterialInterface);
					if (FoundUnrealMatIndex)
					{
						// This material has been mapped already, just assign the mat index
						FoundStaticMesh->SetTriangleMaterialID(FaceIdx, *FoundUnrealMatIndex);
						continue;
					}
				}
				else
				{
					MaterialInterface = Cast<UMaterialInterface>(DefaultMaterial);

					FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
					const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, MaterialId, MaterialPathName);
					const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
					UMaterialInterface* const* FoundMaterial = OutputAssignmentMaterials.Find(DefaultMatIdentifier);
					if (FoundMaterial)
						MaterialInterface = *FoundMaterial;

					// See if we have a replacement material and use it on the mesh instead
					UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(DefaultMatIdentifier);
					if (ReplacementMaterial && *ReplacementMaterial)
						MaterialInterface = *ReplacementMaterial;

					// Map the houdini ID to the unreal one
					MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
				}

				if (MaterialInterface)
				{
					// Add the material to the Static mesh
					int32 UnrealMatIndex = FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));

					// Map the houdini ID to the unreal one
					MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Add(MaterialInterface, UnrealMatIndex);

					// Update the face index
					FoundStaticMesh->SetTriangleMaterialID(FaceIdx, UnrealMatIndex);
				}
			}
		}
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Default Material);

		// No materials were found, we need to use default Houdini material.
		UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

		// See if we have a replacement material and use it on the mesh instead
		UMaterialInterface* const* ReplacementMaterial = ReplacementMaterials.Find(
			FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, ""));
		if (ReplacementMaterial && *ReplacementMaterial)
			MaterialInterface = *ReplacementMaterial;

		FoundStaticMaterials.Empty();
		FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
	}
}


bool FHoudiniMeshTranslator::IsGammaCorrectionDisabled(HAPI_NodeId  NodeId, HAPI_PartId PartId)
{
	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_DISABLE_GAMMA_CORRECTION);
	TArray<int> Values;
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Values);
	if (Values.IsEmpty())
		return false;

	return Values[0] != 0;
}

#undef LOCTEXT_NAMESPACE

