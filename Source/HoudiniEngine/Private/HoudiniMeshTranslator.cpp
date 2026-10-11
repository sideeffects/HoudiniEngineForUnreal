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
#include "HoudiniMeshTranslatorUtils.h"
#include "HoudiniDynamicMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniCookable.h"
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
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"

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
#include <HoudiniHLODLayerUtils.h>

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE


bool
FHoudiniMeshTranslator::CreateAllMeshesAndComponentsFromHoudiniOutput(
	UHoudiniOutput* InOutput, 
	const FHoudiniPackageParams& InPackageParams,
	EHoudiniStaticMeshMethod InStaticMeshMethod,
	bool bSplitMeshSupport,
	const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
	const FMeshBuildSettings& InMeshBuildSettings,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
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
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& AssignementMaterials = InOutput->GetAssignementMaterials();
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& ReplacementMaterials = InOutput->GetReplacementMaterials();

	bool InForceRebuild = false; 
	if (InOutput->HasAnyCurrentProxy() && InStaticMeshMethod != EHoudiniStaticMeshMethod::UHoudiniStaticMesh)
	{
		// Make sure we're not preventing refinement
		InForceRebuild = true;
	}

	bool bAllMeshesCreated = true;
	bool bIsDynamicMeshOutput = false;
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->HoudiniGeoPartObjects)
	{
		const bool bIsPartDynamicMeshOutput = FHoudiniDynamicMeshTranslator::IsDynamicMeshOutput(CurHGPO);
		if (bIsPartDynamicMeshOutput)
		{
			bIsDynamicMeshOutput = true;
			break;
		}
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

			UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InOuterComponent);
			if (HAC)
			{
				// Also try on the HAC's cookable
				FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
					HAC->GetCookable(), PropertyAttributes);
			}
		}

		if (bIsDynamicMeshOutput || FHoudiniDynamicMeshTranslator::IsDynamicMeshOutput(CurHGPO))
		{
			bAllMeshesCreated &= FHoudiniDynamicMeshTranslator::CreateDynamicMeshesFromHoudiniGeoPartObject(
				CurHGPO,
				InPackageParams,
				OldOutputObjects,
				NewOutputObjects,
				AssignementMaterials,
				ReplacementMaterials,
				InAllOutputMaterials,
				InOuterComponent,
				InForceRebuild,
				bInTreatExistingMaterialsAsUpToDate);
		}
		else
		{
			bAllMeshesCreated &= CreateStaticMeshFromHoudiniGeoPartObject(
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
	}

	if (!bAllMeshesCreated)
		return false;

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
				if (NewOutputObj.Value.ProxyComponent == FoundOldOutputObj->ProxyComponent)
				{
					// Remove it from the old map to avoid its destruction
					OldOutputObjects.Remove(OutputIdentifier);
				}
				else
				{
					// The new proxy is the same as the old one
					// Only destroy the proxy's component
					FoundOldOutputObj->ProxyObject = nullptr;
				}
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
			FHoudiniEngineUtils::RemoveAndDestroyComponent(Component);
		OldOutputObject.OutputComponents.Empty();

		// Remove the old proxy component from the map
		FHoudiniEngineUtils::RemoveAndDestroyComponent(OldOutputObject.ProxyComponent);
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
				FHoudiniEngineUtils::RemoveAndDestroyComponent(Component);
			}
			OutputObject.OutputComponents.Empty();

			// Remove the old proxy component from the map
			FHoudiniEngineUtils::RemoveAndDestroyComponent(OutputObject.ProxyComponent);
			OutputObject.ProxyComponent = nullptr;

			continue; // Skip any proxy / component creation below
		}

		// Check if we should create a Proxy/SMC
		if (OutputObject.bProxyIsCurrent && InOutput->bCreateSceneComponents)
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
			UMeshComponent *MeshComponent = FHoudiniMeshTranslatorUtils::CreateOrUpdateMeshComponent(
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
				FHoudiniMeshTranslatorUtils::UpdateMeshComponent(
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
			if (Mesh->IsA<UStaticMesh>() && InOutput->bCreateSceneComponents)
			{
				TSubclassOf<UMeshComponent> ComponentType = UStaticMeshComponent::StaticClass();
				bool bCreated = false;
				MeshComponent = FHoudiniMeshTranslatorUtils::CreateOrUpdateMeshComponent(InOutput, InOuterComponent, OutputIdentifier, ComponentType, OutputObject, FoundHGPO, bCreated);
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
					
					FHoudiniMeshTranslatorUtils::UpdateMeshComponent(
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
			else if (Mesh->IsA<USkeletalMesh>() && InOutput->bCreateSceneComponents)
			{
				
				TSubclassOf<UMeshComponent> SKComponentType = USkeletalMeshComponent::StaticClass();
				bool bSKCreated = false;
				MeshComponent = FHoudiniMeshTranslatorUtils::CreateOrUpdateMeshComponent(InOutput, InOuterComponent, OutputIdentifier, SKComponentType, OutputObject, FoundHGPO, bSKCreated);
				if (MeshComponent)
				{
					FHoudiniMeshTranslatorUtils::UpdateMeshComponent(
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
			else if (Mesh->IsA<UDynamicMesh>() && InOutput->bCreateSceneComponents)
			{
				TSubclassOf<UMeshComponent> ComponentType = UDynamicMeshComponent::StaticClass();
				bool bCreated = false;
				MeshComponent = FHoudiniMeshTranslatorUtils::CreateOrUpdateMeshComponent(
					InOutput,
					InOuterComponent,
					OutputIdentifier,
					ComponentType,
					OutputObject,
					FoundHGPO,
					bCreated);
				if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(MeshComponent))
				{
					DynamicMeshComponent->SetDynamicMesh(CastChecked<UDynamicMesh>(Mesh));
					DynamicMeshComponent->CollisionType = ECollisionTraceFlag::CTF_UseSimpleAsComplex;
					DynamicMeshComponent->SetSimpleCollisionShapes(OutputObject.DynamicMeshSimpleCollision, true);
					DynamicMeshComponent->EmptyOverrideMaterials();
					for (int32 MaterialIndex = 0; MaterialIndex < OutputObject.OutputMaterials.Num(); ++MaterialIndex)
					{
						DynamicMeshComponent->SetMaterial(MaterialIndex, OutputObject.OutputMaterials[MaterialIndex]);
					}
					FHoudiniMeshTranslatorUtils::UpdateMeshComponent(
						DynamicMeshComponent,
						Mesh,
						OutputIdentifier,
						OutputObject,
						FoundHGPO,
						InOutput->HoudiniCreatedSocketActors,
						InOutput->HoudiniAttachedSocketActors,
						bInApplyGenericProperties);
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


bool
FHoudiniMeshTranslator::CreateStaticMeshFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& AssignmentMaterialMap,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& ReplacementMaterialMap,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
	UObject* const InOuterComponent,
	bool InForceRebuild,
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

	FHoudiniGeoPartObject HGPO = InHGPO;

	FHoudiniMeshTranslationContext TranslationContext;
	TranslationContext.InputObjects = InOutputObjects;
	TranslationContext.OutputObjects = OutOutputObjects;
	TranslationContext.bTreatExistingMaterialsAsUpToDate = bInTreatExistingMaterialsAsUpToDate;
	TranslationContext.bForceRebuild = InForceRebuild;
	TranslationContext.StaticMeshGenerationProperties = InSMGenerationProperties;
	TranslationContext.StaticMeshBuildSettings = InSMBuildSettings;
	FHoudiniPackageParams PackageParams = InPackageParams;
	PackageParams.ObjectId = HGPO.ObjectId;
	PackageParams.GeoId = HGPO.GeoId;
	PackageParams.PartId = HGPO.PartId;

	// TODO: Fetch from settings/HAC
	TranslationContext.DefaultMeshSmoothing = 1;
	if (false)
		TranslationContext.DefaultMeshSmoothing = 0;

	FHoudiniCachedMeshPart CachedMeshPart;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> OutputAssignmentMaterials;
	if (bSplitMeshSupport)
	{
		FHoudiniMeshTranslatorUtils::UpdateSplitGroups(HGPO);
	}

	if (!CachedMeshPart.ExtractHoudiniMeshPart(HGPO))
		return false;

	if (bSplitMeshSupport)
		CachedMeshPart.PrepareSplitMeshBuildPlan(HGPO);

	FHoudiniMeshTranslatorUtils::CreateNeededMaterials(HGPO, CachedMeshPart, PackageParams, InOuterComponent, TranslationContext, OutputAssignmentMaterials, AssignmentMaterialMap, InAllOutputMaterials);
	// Create the Static Mesh with the desired method
	bool bCreated = false;
	switch (InStaticMeshMethod)
	{
		case EHoudiniStaticMeshMethod::FMeshDescription:
		{
			FHoudiniStaticMeshTranslator::FBuildContext StaticMeshBuildContext{
				HGPO,
				CachedMeshPart,
				PackageParams,
				InOuterComponent,
				TranslationContext,
				OutputAssignmentMaterials,
				ReplacementMaterialMap };
			if (bSplitMeshSupport)
				bCreated = FHoudiniStaticMeshTranslator::CreateStaticMeshesFromSplitGroups(StaticMeshBuildContext);
			else
				bCreated = FHoudiniStaticMeshTranslator::CreateStaticMesh_MeshDescription(StaticMeshBuildContext);
			break;
		}

		case EHoudiniStaticMeshMethod::UHoudiniStaticMesh:
		{
			FHoudiniProxyMeshTranslator::FBuildContext ProxyMeshBuildContext{
				HGPO,
				CachedMeshPart,
				PackageParams,
				InOuterComponent,
				TranslationContext,
				OutputAssignmentMaterials,
				ReplacementMaterialMap };
			if (bSplitMeshSupport)
				bCreated = FHoudiniProxyMeshTranslator::CreateHoudiniStaticMeshesFromSplitGroups(ProxyMeshBuildContext);
			else
				bCreated = FHoudiniProxyMeshTranslator::CreateHoudiniStaticMesh(ProxyMeshBuildContext);

			break;
		}
	}

	if (!bCreated)
		return false;

	// Copy the output objects/materials
	OutOutputObjects = TranslationContext.OutputObjects;
	AssignmentMaterialMap = OutputAssignmentMaterials;

	return true;
}

bool
FHoudiniCachedMeshPart::UpdatePartVertexList(const FHoudiniGeoPartObject& InHGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::UpdatePartVertexList);

	if (InHGPO.PartInfo.VertexCount <= 0)
		return false;

	// Get the vertex List
	VertexList.SetNumUninitialized(InHGPO.PartInfo.VertexCount);

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVertexList(
		FHoudiniEngine::Get().GetSession(),
		InHGPO.GeoId, InHGPO.PartId, &VertexList[0], 0, InHGPO.PartInfo.VertexCount))
	{
		// Error getting the vertex list.
		HOUDINI_LOG_MESSAGE(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list - skipping."),
			InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName);

		return false;
	}

	return true;
}

bool
FHoudiniCachedMeshPart::ExtractHoudiniMeshPart(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::ExtractHoudiniMeshPart);

	ResetCache();
	MeshesToBuild.Meshes.Empty();
	SplitGroups.Empty();
	Splits.Empty();

	if (!UpdatePartVertexList(HGPO))
		return false;

	SortSplitGroups(HGPO);

	if (!UpdateSplitsFacesAndIndices(HGPO))
		return false;

	return true;
}

void
FHoudiniCachedMeshPart::PrepareSplitMeshBuildPlan(const FHoudiniGeoPartObject& HGPO)
{
	MeshesToBuild = FHoudiniStaticMeshTranslator::ScanOutputForMeshesToBuild(HGPO, SplitGroups);
}

void
FHoudiniCachedMeshPart::SortSplitGroups(const FHoudiniGeoPartObject& InHGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::SortSplitGroups);

	// Sort the splits in the order that we want to process them:
	// Simple/Convex invisible colliders should be treated first as they will need to be attached to the visible meshes
	TArray<FString> First;
	
	// The main geo and its LODs should be created after.
	TArray<FString> Main;
	TArray<FString> LODs;

	// Finally, visible colliders and invisible complex colliders as they need their own static mesh
	TArray<FString> Last;

	for (auto& curSplit : InHGPO.SplitGroups)
	{
		EHoudiniSplitType curSplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit);
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
	SplitGroups.Empty();
	for (auto& splitName : First)
		SplitGroups.Add(splitName);

	for (auto& splitName : Main)
		SplitGroups.Add(splitName);

	for (auto& splitName : LODs)
		SplitGroups.Add(splitName);

	for (auto& splitName : Last)
		SplitGroups.Add(splitName);
}

bool
FHoudiniCachedMeshPart::UpdateSplitsFacesAndIndices(const FHoudiniGeoPartObject& InHGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::UpdateSplitsFacesAndIndices);

	// Reset the splits faces/indices arrays
	Splits.Empty();

	bool bHasSplit = SplitGroups.Num() > 0;
	if (bHasSplit)
	{
		HAPI_PartInfo PartInfo = FHoudiniEngineUtils::ToHAPIPartInfo(InHGPO.PartInfo);

		// Buffer for all vertex indices used for split groups.
		// We need this to figure out all vertex indices that are not part of them. 
		TArray<int32> PartUsedVertices;
		PartUsedVertices.SetNumZeroed(VertexList.Num());

		// Buffer for all face indices used for split groups.
		// We need this to figure out all face indices that are not part of them.
		TArray<int32> AllGroupFaceIndices;
		AllGroupFaceIndices.SetNumZeroed(InHGPO.PartInfo.FaceCount);

		// Some of the groups may contain invalid geometry 
		// Store them here so we can remove them afterwards
		TArray<int32> InvalidGroupNameIndices;

		// Extract the vertices/faces for each of the split groups
		for (int32 SplitIdx = 0; SplitIdx < SplitGroups.Num(); SplitIdx++)
		{
			const FString& GroupName = SplitGroups[SplitIdx];

			// New vertex list just for this group.
			TArray< int32 > GroupVertexList;
			TArray< int32 > AllFaceList;
			
			int32 FirstValidPrimIndex = 0;
			int32 FirstValidVertexIndex = 0;
			// Extract vertex indices for this split.
			int32 GroupVertexListCount = FHoudiniEngineUtils::HapiGetVertexListForGroup(
				InHGPO.GeoId, PartInfo, GroupName,
				VertexList, GroupVertexList,
				PartUsedVertices, AllFaceList, AllGroupFaceIndices,
				FirstValidVertexIndex, FirstValidPrimIndex,
				InHGPO.PartInfo.bIsInstanced);

			if (GroupVertexListCount <= 0)
			{
				// This group doesn't have vertices/faces, mark it as invalid
				InvalidGroupNameIndices.Add(SplitIdx);

				// Error getting the vertex list.
				HOUDINI_LOG_MESSAGE(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list for group %s - skipping."),
					InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, *GroupName);

				continue;
			}

			// If list is not empty, we store it for this group - this will define new mesh.
			FPreparedHoudiniMeshSplit& PreparedSplit = Splits.Add(GroupName);
			PreparedSplit.VertexList = MoveTemp(GroupVertexList);
			PreparedSplit.VertexCount = GroupVertexListCount;
			PreparedSplit.FaceIndices = MoveTemp(AllFaceList);
			PreparedSplit.FirstValidVertexIndex = FirstValidVertexIndex;
			PreparedSplit.FirstValidPrimIndex = FirstValidPrimIndex;
		}

		if (InvalidGroupNameIndices.Num() > 0)
		{
			// Remove all invalid split groups
			for (int32 InvalIdx = InvalidGroupNameIndices.Num() - 1; InvalIdx >= 0; InvalIdx--)
			{
				int32 Index = InvalidGroupNameIndices[InvalIdx];
				SplitGroups.RemoveAt(Index);
			}
		}

		// We also need to figure out / construct the vertex list for everything that's not in a split group
		TArray<int32> GroupSplitFacesRemaining;
		GroupSplitFacesRemaining.SetNumUninitialized(VertexList.Num());
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
				GroupSplitFacesRemaining[SplitVertexIdx] = VertexList[SplitVertexIdx];
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
			SplitGroups.Add(RemainingGroupName);
			FPreparedHoudiniMeshSplit& PreparedSplit = Splits.Add(RemainingGroupName);
			PreparedSplit.VertexList = MoveTemp(GroupSplitFacesRemaining);
			PreparedSplit.VertexCount = GroupVertexListCount;
			PreparedSplit.FaceIndices = MoveTemp(GroupSplitFaceIndicesRemaining);
			PreparedSplit.FirstValidPrimIndex = FistUnusedPrimIndex;
			PreparedSplit.FirstValidVertexIndex = FistUnusedVertexIndex;
		}
	}
	else
	{
		// No splitting required
		// Mark everything as the main geo group
		static const FString RemainingGroupName = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
		SplitGroups.Add(RemainingGroupName);
		FPreparedHoudiniMeshSplit& PreparedSplit = Splits.Add(RemainingGroupName);
		PreparedSplit.VertexList = VertexList;
		PreparedSplit.VertexCount = VertexList.Num();
		PreparedSplit.FirstValidPrimIndex = 0;
		PreparedSplit.FirstValidVertexIndex = 0;

		TArray<int32> AllFaces;
		for (int32 FaceIdx = 0; FaceIdx < InHGPO.PartInfo.FaceCount; ++FaceIdx)
			AllFaces.Add(FaceIdx);

		PreparedSplit.FaceIndices = MoveTemp(AllFaces);
	}

	return true;
}

FHoudiniCachedMeshPart::FHoudiniCachedMeshPart()
{
	ResetCache();
}

void
FHoudiniCachedMeshPart::ResetCache()
{
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	// Vertex Positions
	CachedMeshPart.PartPositions.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoPositions);

	// Vertex Normals
	CachedMeshPart.PartNormals.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoNormals);

	// Vertex TangentU
	CachedMeshPart.PartTangentU.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoTangentU);

	// Vertex TangentV
	CachedMeshPart.PartTangentV.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoTangentV);

	// Vertex Colors
	CachedMeshPart.PartColors.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoColors);

	// Vertex Alpha values
	CachedMeshPart.PartAlphas.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoAlpha);

	// FaceSmoothing values
	CachedMeshPart.PartFaceSmoothingMasks.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoFaceSmoothingMasks);

	// UVs
	CachedMeshPart.PartUVSets.Empty();
	CachedMeshPart.AttribInfoUVSets.Empty();

	// UVs
	CachedMeshPart.PartLightMapResolutions.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoLightmapResolution);

	// Material IDs per face
	CachedMeshPart.PartFaceMaterialIds.Empty();
	FHoudiniApi::AttributeInfo_Init(&CachedMeshPart.AttribInfoFaceMaterialIds);
	// Unique material IDs
	CachedMeshPart.PartUniqueMaterialIds.Empty();
	// Material infos for each unique Material
	CachedMeshPart.PartUniqueMaterialInfos.Empty();
	//
	CachedMeshPart.bOnlyOneFaceMaterial = false;

	// Face Materials override
	CachedMeshPart.PartFaceMaterialOverrides.Empty();
	CachedMeshPart.bHaveMaterialOverrides = false;
	CachedMeshPart.bHavePrimMaterialOverrides = false;
	CachedMeshPart.bMaterialOverrideNeedsCreateInstance = false;

	// LOD Screensize
	CachedMeshPart.PartLODScreensize.Empty();
}

TArray<float>&
FHoudiniCachedMeshPart::GetPositions(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetPositions);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	// Only Retrieve the vertices positions if necessary
	if (CachedMeshPart.PartPositions.Num() > 0)
		return CachedMeshPart.PartPositions;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_POSITION);
	Accessor.GetInfo(CachedMeshPart.AttribInfoPositions);

	if (!Accessor.GetAttributeData(CachedMeshPart.AttribInfoPositions, CachedMeshPart.PartPositions))
	{
		// Error retrieving positions.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve position data")
			TEXT("- skipping."),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return CachedMeshPart.PartPositions;
	}
	return CachedMeshPart.PartPositions;
}

TArray<float>&
FHoudiniCachedMeshPart::GetNormals(const FHoudiniGeoPartObject& HGPO, const bool bForceRead)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetNormals);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	if (CachedMeshPart.PartNormals.Num() > 0)
		return CachedMeshPart.PartNormals;

	// No need to read the normals if we want unreal to recompute them after
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bReadNormals = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->RecomputeNormalsFlag != EHoudiniRuntimeSettingsRecomputeFlag::HRSRF_Always : true;
	if (!bForceRead && !bReadNormals)
		return CachedMeshPart.PartNormals;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_NORMAL);
	Accessor.GetInfo(CachedMeshPart.AttribInfoNormals);

	// Retrieve normal data for this part
	bool Success = Accessor.GetAttributeData(CachedMeshPart.AttribInfoNormals, CachedMeshPart.PartNormals);

	// There is no normals to fetch
	if (!CachedMeshPart.AttribInfoNormals.exists)
		return CachedMeshPart.PartNormals;

	if	(!Success && CachedMeshPart.AttribInfoNormals.exists)
	{
		// Error retrieving normals.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve normal data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return CachedMeshPart.PartNormals;
	}

	return CachedMeshPart.PartNormals;
}

bool
FHoudiniCachedMeshPart::CacheTangents(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::CacheTangents);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;
	if (CachedMeshPart.PartTangentU.Num() > 0 && CachedMeshPart.PartTangentV.Num() > 0)
		return true;

	bool bReturn = true;
	if (CachedMeshPart.PartTangentU.Num() <= 0)
	{
		// Retrieve TangentU data for this part
		FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_TANGENTU);
		Accessor.GetInfo(CachedMeshPart.AttribInfoTangentU);
		bool Success = Accessor.GetAttributeData(CachedMeshPart.AttribInfoTangentU, CachedMeshPart.PartTangentU);
		
		if (!Success && CachedMeshPart.AttribInfoTangentU.exists)
		{
			// Error retrieving tangent.
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve tangentU data"),
				HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
			bReturn = false;
		}
	}

	if (CachedMeshPart.PartTangentV.Num() <= 0)
	{
		FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_TANGENTV);
		Accessor.GetInfo(CachedMeshPart.AttribInfoTangentV);
		bool Success = Accessor.GetAttributeData(CachedMeshPart.AttribInfoTangentV, CachedMeshPart.PartTangentV);

		if (!Success && CachedMeshPart.AttribInfoTangentV.exists)
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

TArray<float>&
FHoudiniCachedMeshPart::GetColors(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetColors);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	// Only Retrieve the vertices colors if necessary
	if (CachedMeshPart.PartColors.Num() > 0)
		return CachedMeshPart.PartColors;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_COLOR);
	Accessor.GetInfo(CachedMeshPart.AttribInfoColors);
	bool Success = Accessor.GetAttributeData(CachedMeshPart.AttribInfoColors, CachedMeshPart.PartColors);

	if (!Success && CachedMeshPart.AttribInfoColors.exists)
	{
		// Error retrieving colors.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve color data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return CachedMeshPart.PartColors;
	}

	return CachedMeshPart.PartColors;
}

TArray<float>&
FHoudiniCachedMeshPart::GetAlphas(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetAlphas);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	// Only Retrieve the vertices alphas if necessary
	if (CachedMeshPart.PartAlphas.Num() > 0)
		return CachedMeshPart.PartAlphas;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_ALPHA);
	Accessor.GetInfo(CachedMeshPart.AttribInfoAlpha);
	bool Success = Accessor.GetAttributeData(CachedMeshPart.AttribInfoAlpha, CachedMeshPart.PartAlphas);

	if (!Success && CachedMeshPart.AttribInfoAlpha.exists)
	{
		// Error retrieving alpha values.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve alpha data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return CachedMeshPart.PartAlphas;
	}

	return CachedMeshPart.PartAlphas;
}

TArray<int32>&
FHoudiniCachedMeshPart::GetFaceSmoothingMasks(const FHoudiniGeoPartObject& HGPO)
{
	FHoudiniCachedMeshPart& CachedMeshPart = *this;
	// Only Retrieve the vertices FaceSmoothing if necessary
	if (CachedMeshPart.PartFaceSmoothingMasks.Num() > 0)
		return CachedMeshPart.PartFaceSmoothingMasks;

	FHoudiniHapiAccessor Accessor(HGPO.GeoInfo.NodeId, HGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK);
	Accessor.GetInfo(CachedMeshPart.AttribInfoFaceSmoothingMasks);
	bool Success = Accessor.GetAttributeData(CachedMeshPart.AttribInfoFaceSmoothingMasks, CachedMeshPart.PartFaceSmoothingMasks);

	if (!Success && CachedMeshPart.AttribInfoFaceSmoothingMasks.exists)
	{
		// Error retrieving FaceSmoothing values.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve FaceSmoothing data"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return CachedMeshPart.PartFaceSmoothingMasks;
	}

	return CachedMeshPart.PartFaceSmoothingMasks;
}

TArray<TArray<float>>&
FHoudiniCachedMeshPart::GetUVSets(const FHoudiniGeoPartObject& HGPO, bool bRemoveUnused)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetUVSets);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;
	if (CachedMeshPart.PartUVSets.Num() > 0)
		return CachedMeshPart.PartUVSets;

	FHoudiniEngineUtils::UpdateMeshPartUVSets(
		HGPO.GeoId,
		HGPO.PartId,
		bRemoveUnused,
		CachedMeshPart.PartUVSets,
		CachedMeshPart.AttribInfoUVSets);
	return CachedMeshPart.PartUVSets;
}

TArray<int32>&
FHoudiniCachedMeshPart::GetLightmapResolutions(const FHoudiniGeoPartObject& InHGPO)
{
	FHoudiniCachedMeshPart& CachedMeshPart = *this;
	// Only Retrieve the vertices light map resolution if necessary
	if (CachedMeshPart.PartLightMapResolutions.Num() > 0)
		return CachedMeshPart.PartLightMapResolutions;

	FHoudiniHapiAccessor Accessor(InHGPO.GeoInfo.NodeId, InHGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION);
	Accessor.GetInfo(CachedMeshPart.AttribInfoLightmapResolution);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, CachedMeshPart.PartLightMapResolutions);

	if (!bSuccess && CachedMeshPart.AttribInfoLightmapResolution.exists)
	{
		// Error retrieving lightmap resolution values.
		HOUDINI_LOG_WARNING(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], unable to retrieve lightmap resolution data"),
			InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName);
		return CachedMeshPart.PartLightMapResolutions;
	}

	return CachedMeshPart.PartLightMapResolutions;
}

TArray<int32>&
FHoudiniCachedMeshPart::GetFaceMaterialIds(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetFaceMaterialIds);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	// Only Retrieve the material IDs if necessary
	if (CachedMeshPart.PartFaceMaterialIds.Num() > 0)
		return CachedMeshPart.PartFaceMaterialIds;

	int32 NumFaces = HGPO.PartInfo.FaceCount;
	if (NumFaces <= 0)
		return CachedMeshPart.PartFaceMaterialIds;

	CachedMeshPart.PartFaceMaterialIds.SetNum(NumFaces);

	// Get the materials IDs per face
	HAPI_Bool bSingleFaceMaterial = false;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialNodeIdsOnFaces(
		FHoudiniEngine::Get().GetSession(),
		HGPO.GeoId, HGPO.PartId, &bSingleFaceMaterial,
		&CachedMeshPart.PartFaceMaterialIds[0], 0, NumFaces))
	{
		// Error retrieving material face assignments.
		HOUDINI_LOG_MESSAGE(
			TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments"),
			HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName);
		return CachedMeshPart.PartFaceMaterialIds;
	}

	CachedMeshPart.bOnlyOneFaceMaterial = bSingleFaceMaterial;

	return CachedMeshPart.PartFaceMaterialIds;
}

TArray<FHoudiniMaterialInfo>&
FHoudiniCachedMeshPart::GetFaceMaterialOverrides(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::GetFaceMaterialOverrides);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;

	// Only Retrieve the material overrides if necessary
	if (CachedMeshPart.PartFaceMaterialOverrides.Num() > 0)
		return CachedMeshPart.PartFaceMaterialOverrides;

	CachedMeshPart.bHaveMaterialOverrides = false;
	CachedMeshPart.bHavePrimMaterialOverrides = false;
	CachedMeshPart.bMaterialOverrideNeedsCreateInstance = false;

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
		CachedMeshPart.PartFaceMaterialOverrides.Empty();

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
		return CachedMeshPart.PartFaceMaterialOverrides;

	if ((!bMaterialAttributeExists || MaterialAttrOwner == HAPI_ATTROWNER_DETAIL) && (!bMaterialInstanceAttributeExists || MaterialInstanceAttrOwner == HAPI_ATTROWNER_DETAIL))
	{
		// either only one attribute exists and is a detail attribute, or both exist and are detail attributes
		CachedMeshPart.bHavePrimMaterialOverrides = false;
		FHoudiniMaterialInfo MatInfo;
		if (MaterialOverrides.IsValidIndex(0) && !MaterialOverrides[0].IsEmpty())
		{
			MatInfo.MaterialObjectPath = MaterialOverrides[0];
			FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
		}
		else if (MaterialInstanceOverrides.IsValidIndex(0) && !MaterialInstanceOverrides[0].IsEmpty())
		{
			MatInfo.bMakeMaterialInstance = true;
			CachedMeshPart.bMaterialOverrideNeedsCreateInstance = true;
			MatInfo.MaterialObjectPath = MaterialInstanceOverrides[0];
			FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
		}
		else
		{
			MatInfo.MaterialObjectPath = FString();
		}
		// PartFaceMaterialOverrides must have an entry for each face, or be empty
		if (!MatInfo.MaterialObjectPath.IsEmpty())
			CachedMeshPart.PartFaceMaterialOverrides.Init(MatInfo, HGPO.PartInfo.FaceCount);
		else
			CachedMeshPart.PartFaceMaterialOverrides.Empty();
	}
	else
	{
		// Cases to handle here: both exist and are prim, or one is prim and one detail, or only one exists and is prim
		CachedMeshPart.bHavePrimMaterialOverrides = true;
		// PartFaceMaterialOverrides must have an entry for each face, or be empty
		CachedMeshPart.PartFaceMaterialOverrides.Reset(HGPO.PartInfo.FaceCount);
		for (int32 Index = 0; Index < HGPO.PartInfo.FaceCount; ++Index)
		{
			FHoudiniMaterialInfo& MatInfo = CachedMeshPart.PartFaceMaterialOverrides.AddDefaulted_GetRef();

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
				FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
			}
			else if (MaterialInstanceOverrides.IsValidIndex(MaterialInstanceOverridesIndex) && !MaterialInstanceOverrides[MaterialInstanceOverridesIndex].IsEmpty())
			{
				MatInfo.bMakeMaterialInstance = true;
				CachedMeshPart.bMaterialOverrideNeedsCreateInstance = true;
				MatInfo.MaterialObjectPath = MaterialInstanceOverrides[MaterialInstanceOverridesIndex];
				FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MatInfo.MaterialObjectPath, MatInfo.MaterialIndex);
			}
			else
			{
				MatInfo.MaterialObjectPath = FString();
			}
		}
	}

	CachedMeshPart.bHaveMaterialOverrides = CachedMeshPart.PartFaceMaterialOverrides.Num() > 0;
	if (CachedMeshPart.bHaveMaterialOverrides)
		FHoudiniMaterialTranslator::GetMaterialParameters(CachedMeshPart.PartFaceMaterialOverrides, HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_PRIM);

	return CachedMeshPart.PartFaceMaterialOverrides;
}

bool
FHoudiniCachedMeshPart::UpdateNeededMaterials(const FHoudiniGeoPartObject& HGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniCachedMeshPart::UpdateNeededMaterials);
	FHoudiniCachedMeshPart& CachedMeshPart = *this;
	if (CachedMeshPart.PartUniqueMaterialIds.Num() > 0)
		return true;

	// Update the per face material IDs
	GetFaceMaterialIds(HGPO);

	// See if we have some material overides
	GetFaceMaterialOverrides(HGPO);

	// If we have houdini materials AND overrides:
	// We want to only create the Houdini materials that are not "covered" by overrides
	// If we have material instance attributes, create all the houdini material anyway
	// as their textures could be referenced by the material instance parameters
	if (CachedMeshPart.PartFaceMaterialOverrides.Num() > 0 && !CachedMeshPart.bMaterialOverrideNeedsCreateInstance)
	{
		// If the material override was set on the detail, no need to look for houdini material IDs, as only the override will be used
		if (CachedMeshPart.bHaveMaterialOverrides && CachedMeshPart.bHavePrimMaterialOverrides)
		{
			for (int32 MaterialIdx = 0; MaterialIdx < CachedMeshPart.PartFaceMaterialIds.Num(); ++MaterialIdx)
			{
				// Add a material ID to the unique array only if that face is not using the override
				if (CachedMeshPart.PartFaceMaterialOverrides[MaterialIdx].MaterialObjectPath.IsEmpty())
					CachedMeshPart.PartUniqueMaterialIds.AddUnique(CachedMeshPart.PartFaceMaterialIds[MaterialIdx]);
			}
		}
	}
	else
	{
		// No material overrides, simply update the unique material array
		for (int32 MaterialIdx = 0; MaterialIdx < CachedMeshPart.PartFaceMaterialIds.Num(); ++MaterialIdx)
			CachedMeshPart.PartUniqueMaterialIds.AddUnique(CachedMeshPart.PartFaceMaterialIds[MaterialIdx]);
	}

	// Remove the invalid material ID from the unique array
	CachedMeshPart.PartUniqueMaterialIds.RemoveSingle(-1);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::UpdatePartNeededMaterials - Get the unique material infos);
		// Get the unique material infos
		CachedMeshPart.PartUniqueMaterialInfos.SetNum(CachedMeshPart.PartUniqueMaterialIds.Num());
		for (int32 MaterialIdx = 0; MaterialIdx < CachedMeshPart.PartUniqueMaterialIds.Num(); MaterialIdx++)
		{

			FHoudiniApi::MaterialInfo_Init(&CachedMeshPart.PartUniqueMaterialInfos[MaterialIdx]);
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialInfo(
				FHoudiniEngine::Get().GetSession(),
				CachedMeshPart.PartUniqueMaterialIds[MaterialIdx],
				&CachedMeshPart.PartUniqueMaterialInfos[MaterialIdx]))
			{
				// Error retrieving material face assignments.
				HOUDINI_LOG_MESSAGE(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material info for material %d"),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, CachedMeshPart.PartUniqueMaterialIds[MaterialIdx]);
				continue;
			}
		}
	}
	return true;
}

TArray<float>&
FHoudiniCachedMeshPart::GetLODScreensizes(const FHoudiniGeoPartObject& InHGPO)
{
	FHoudiniCachedMeshPart& CachedMeshPart = *this;
	// Only retrieve LOD screensizes if necessary
	if (CachedMeshPart.PartLODScreensize.Num() > 0)
		return CachedMeshPart.PartLODScreensize;

	FHoudiniHapiAccessor Accessor(InHGPO.GeoInfo.NodeId, InHGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE);
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, CachedMeshPart.PartLODScreensize);
	return CachedMeshPart.PartLODScreensize;
}

const HAPI_AttributeInfo&
FHoudiniCachedMeshPart::GetNormalAttributeInfo(const FHoudiniGeoPartObject& InHGPO)
{
	GetNormals(InHGPO);
	return AttribInfoNormals;
}

TArray<float>&
FHoudiniCachedMeshPart::GetTangentU(const FHoudiniGeoPartObject& InHGPO)
{
	CacheTangents(InHGPO);
	return PartTangentU;
}

const HAPI_AttributeInfo&
FHoudiniCachedMeshPart::GetTangentUAttributeInfo(const FHoudiniGeoPartObject& InHGPO)
{
	CacheTangents(InHGPO);
	return AttribInfoTangentU;
}

TArray<float>&
FHoudiniCachedMeshPart::GetTangentV(const FHoudiniGeoPartObject& InHGPO)
{
	CacheTangents(InHGPO);
	return PartTangentV;
}

const HAPI_AttributeInfo&
FHoudiniCachedMeshPart::GetTangentVAttributeInfo(const FHoudiniGeoPartObject& InHGPO)
{
	CacheTangents(InHGPO);
	return AttribInfoTangentV;
}

const HAPI_AttributeInfo&
FHoudiniCachedMeshPart::GetColorAttributeInfo(const FHoudiniGeoPartObject& InHGPO)
{
	GetColors(InHGPO);
	return AttribInfoColors;
}

const HAPI_AttributeInfo&
FHoudiniCachedMeshPart::GetAlphaAttributeInfo(const FHoudiniGeoPartObject& InHGPO)
{
	GetAlphas(InHGPO);
	return AttribInfoAlpha;
}

const HAPI_AttributeInfo&
FHoudiniCachedMeshPart::GetFaceSmoothingMaskAttributeInfo(const FHoudiniGeoPartObject& InHGPO)
{
	GetFaceSmoothingMasks(InHGPO);
	return AttribInfoFaceSmoothingMasks;
}

TArray<HAPI_AttributeInfo>&
FHoudiniCachedMeshPart::GetUVSetAttributeInfos(const FHoudiniGeoPartObject& InHGPO, bool bRemoveUnused)
{
	GetUVSets(InHGPO, bRemoveUnused);
	return AttribInfoUVSets;
}

TArray<int32>&
FHoudiniCachedMeshPart::GetUniqueMaterialIds(const FHoudiniGeoPartObject& InHGPO)
{
	UpdateNeededMaterials(InHGPO);
	return PartUniqueMaterialIds;
}

TArray<HAPI_MaterialInfo>&
FHoudiniCachedMeshPart::GetUniqueMaterialInfos(const FHoudiniGeoPartObject& InHGPO)
{
	UpdateNeededMaterials(InHGPO);
	return PartUniqueMaterialInfos;
}

bool
FHoudiniCachedMeshPart::IsOnlyOneFaceMaterial(const FHoudiniGeoPartObject& InHGPO)
{
	GetFaceMaterialIds(InHGPO);
	return bOnlyOneFaceMaterial;
}

bool
FHoudiniCachedMeshPart::HasMaterialOverrides(const FHoudiniGeoPartObject& InHGPO)
{
	GetFaceMaterialOverrides(InHGPO);
	return bHaveMaterialOverrides;
}

bool
FHoudiniCachedMeshPart::HasPrimitiveMaterialOverrides(const FHoudiniGeoPartObject& InHGPO)
{
	GetFaceMaterialOverrides(InHGPO);
	return bHavePrimMaterialOverrides;
}

bool
FHoudiniCachedMeshPart::NeedsMaterialOverrideInstance(const FHoudiniGeoPartObject& InHGPO)
{
	GetFaceMaterialOverrides(InHGPO);
	return bMaterialOverrideNeedsCreateInstance;
}







UStaticMesh*
FHoudiniStaticMeshTranslator::CreateNewUnrealStaticMesh(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniPackageParams& InOutPackageParams,
	const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	InOutPackageParams.ObjectId = HGPO.ObjectId;
	InOutPackageParams.GeoId = HGPO.GeoId;
	InOutPackageParams.PartId = HGPO.PartId;
	InOutPackageParams.SplitStr = InSplitIdentifier;

	UStaticMesh * NewStaticMesh = InOutPackageParams.CreateObjectAndPackage<UStaticMesh>();
	if (!IsValid(NewStaticMesh))
		return nullptr;

	return NewStaticMesh;
}

UHoudiniStaticMesh*
FHoudiniProxyMeshTranslator::CreateNewHoudiniStaticMesh(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniPackageParams& InOutPackageParams,
	const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	InOutPackageParams.ObjectId = HGPO.ObjectId;
	InOutPackageParams.GeoId = HGPO.GeoId;
	InOutPackageParams.PartId = HGPO.PartId;
	// Add _HSM suffix to the split str, to distinguish the temporary HoudiniStaticMesh
	// from the UStaticMesh
	InOutPackageParams.SplitStr = InSplitIdentifier + "_HSM";

	UHoudiniStaticMesh * NewStaticMesh = InOutPackageParams.CreateObjectAndPackage<UHoudiniStaticMesh>();
	if (!IsValid(NewStaticMesh))
		return nullptr;

	return NewStaticMesh;
}



bool
FHoudiniStaticMeshTranslator::CreateStaticMesh_MeshDescription(FBuildContext& InOutBuildContext)
{
	FHoudiniGeoPartObject& HGPO = InOutBuildContext.HGPO;
	FHoudiniCachedMeshPart& InOutCachedMeshPart = InOutBuildContext.CachedMeshPart;
	FHoudiniPackageParams& InOutPackageParams = InOutBuildContext.PackageParams;
	UObject* InOuterComponent = InOutBuildContext.OuterComponent;
	FHoudiniMeshTranslationContext& InOutTranslationContext = InOutBuildContext.TranslationContext;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials = InOutBuildContext.OutputAssignmentMaterials;
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials = InOutBuildContext.ReplacementMaterials;
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;

	const bool bDoTiming = FHoudiniMeshTranslatorUtils::IsMeshBuildTimingEnabled();

	double TimeStart = FPlatformTime::Seconds();

	bool bIsGammaCorrectionDisabled = FHoudiniMeshTranslatorUtils::IsGammaCorrectionDisabled(HGPO.GeoId, HGPO.PartId);

	// Keep a copy of the initial package params, since InOutPackageParams is modified in place when resolving attributes
	FHoudiniPackageParams InitialPackageParams = InOutPackageParams;

	// Prepare the object that will store UCX and simple colliders
	InOutTranslationContext.AllAggregateCollisions.Empty();

	// We need to know the number of LODs that will be needed for this part, and we also need the identifier for the
	// main split (Normal or LOD0)
	int32 NumberOfLODs = 0;
	bool bHasMainGeo = false;
	FHoudiniOutputObjectIdentifier MainIdentifier;
	bool bHasMainIdentifier = false;
	for (auto& curSplit : CachedMeshPart.SplitGroups)
	{
		if (FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit) == EHoudiniSplitType::LOD)
		{
			NumberOfLODs++;
			if (NumberOfLODs == 1 && !bHasMainGeo)
			{
				MainIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(HGPO, InOutCachedMeshPart, curSplit, FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit));
				bHasMainIdentifier = true;
			}
		}
		else if (FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit) == EHoudiniSplitType::Normal)
		{
			bHasMainGeo = true;
			MainIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(HGPO, InOutCachedMeshPart, curSplit, FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit));
			bHasMainIdentifier = true;
		}
	}

	if (NumberOfLODs > MAX_STATIC_MESH_LODS)
	{
		HOUDINI_LOG_ERROR(TEXT("Maximum number of LODs is %d, found %d "), MAX_STATIC_MESH_LODS, NumberOfLODs);
		NumberOfLODs = MAX_STATIC_MESH_LODS;
	}
	const bool bMaterialHasChanged = FHoudiniMeshTranslatorUtils::HavePartMaterialsChanged(HGPO, CachedMeshPart);

	// Get the current target platform for default lod policies
	ITargetPlatform * CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);

	// New mesh list
	TMap<FHoudiniOutputObjectIdentifier, UStaticMesh*> StaticMeshToBuild;

	// Map of Houdini Material IDs to Unreal Material Interface
	TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>> MapHoudiniMatIdToUnrealInterface;
	// Map of Houdini Material Attributes to Unreal Material Interface
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> MapHoudiniMatAttributesToUnrealInterface;
	// Map of Unreal Material Interface to Unreal Material Index, per visible mesh
	TMap<UStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	bool MeshMaterialsHaveBeenReset = false;

	double tick = FPlatformTime::Seconds();
	if (bDoTiming)
		HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Pre Split-Loop in %f seconds."), tick - TimeStart);

	UStaticMesh* MainStaticMesh = nullptr;
	bool bAssignedCustomCollisionMesh = false;
	ECollisionTraceFlag MainStaticMeshCTF = InOutTranslationContext.StaticMeshGenerationProperties.GeneratedCollisionTraceFlag;

	// Map of object identifiers to package params
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniPackageParams> ObjectIdentifiersToPackageParams;

	// Iterate through all detected split groups we care about and split geometry.
	// The split are ordered in the following way:
	// Invisible Simple/Convex Colliders > LODs > MainGeo > Visible Colliders > Invisible Colliders
	for (int32 SplitId = 0; SplitId < CachedMeshPart.SplitGroups.Num(); SplitId++)
	{
		double split_tick = FPlatformTime::Seconds();

		// Get split group name
		const FString& SplitGroupName = CachedMeshPart.SplitGroups[SplitId];
		FPreparedHoudiniMeshSplit& PreparedSplit = CachedMeshPart.Splits.FindChecked(SplitGroupName);

		// Get the vertex indices for this group
		TArray<int32>& SplitVertexList = PreparedSplit.VertexList;

		// Get valid count of vertex indices for this split.
		const int32& SplitVertexCount = PreparedSplit.VertexCount;

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
		EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(SplitGroupName);
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
		FHoudiniOutputObjectIdentifier OutputObjectIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(HGPO, InOutCachedMeshPart, SplitGroupName, SplitType);

		// Get/Create the Aggregate Collisions for this mesh identifier
		FKAggregateGeom& AggregateCollisions = InOutTranslationContext.AllAggregateCollisions.FindOrAdd(OutputObjectIdentifier);

		// Handle UCX / Convex Hull colliders
		if (SplitType == EHoudiniSplitType::InvisibleUCXCollider || SplitType == EHoudiniSplitType::RenderedUCXCollider)
		{
			// Create the convex hull colliders and add them to the Aggregate
			if (!FHoudiniStaticMeshTranslator::AddConvexCollisionToAggregate(HGPO, InOutCachedMeshPart, SplitGroupName, AggregateCollisions))
			{
				MainStaticMeshCTF = ECollisionTraceFlag::CTF_UseDefault;
				// Failed to generate a convex collider
				HOUDINI_LOG_WARNING(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] failed to create convex collider."),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			}

			// If the collider is not visible, stop here - unless if we dont have mainGeo
			if (SplitType == EHoudiniSplitType::InvisibleUCXCollider && bHasMainGeo)
				continue;
		}
		else if (SplitType == EHoudiniSplitType::InvisibleSimpleCollider || SplitType == EHoudiniSplitType::RenderedSimpleCollider)
		{
			MainStaticMeshCTF = ECollisionTraceFlag::CTF_UseDefault;
			// Create the simple colliders and add them to the aggregate
			if (!FHoudiniMeshTranslatorUtils::AddSimpleCollisionToAggregate(HGPO, InOutCachedMeshPart, SplitGroupName, AggregateCollisions))
			{
				// Failed to generate a convex collider
				HOUDINI_LOG_WARNING(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d %s] failed to create simple collider."),
					HGPO.ObjectId, *HGPO.ObjectName, HGPO.GeoId, HGPO.PartId, *HGPO.PartName, SplitId, *SplitGroupName);
			}

			// If the collider is not visible, stop here - unless if we dont have a main geo
			if (SplitType == EHoudiniSplitType::InvisibleSimpleCollider && bHasMainGeo)
				continue;
		}

		// Try to find existing properties for this identifier
		// First check the OutputObjects (for LODs and Normal geo the same FHoudiniOutputObject entry is used, so
		// we check OutputObjects first to see if we have already created/updated it for the main mesh)
		FHoudiniOutputObject* FoundOutputObject = InOutTranslationContext.OutputObjects.Find(OutputObjectIdentifier);
		if (!FoundOutputObject)
			FoundOutputObject = InOutTranslationContext.InputObjects.Find(OutputObjectIdentifier);

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
				FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, MainIdentifier, TempAttributes, TempTokens);
			else
				FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, OutputObjectIdentifier, TempAttributes, TempTokens);

			// Resolve our final package params
			FHoudiniAttributeResolver Resolver;
			FHoudiniPackageParams FinalPackageParams;
			FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
				InitialPackageParams,
				IsValid(InOuterComponent) ? InOuterComponent->GetWorld() : nullptr,
				InOuterComponent,
				TempAttributes,
				TempTokens,
				InOutPackageParams,
				Resolver);

			bCopyAttributesAndTokens = true;
			ObjectIdentifiersToPackageParams.Emplace(OutputObjectIdentifier, InOutPackageParams);
		}
		else
		{
			InOutPackageParams = ObjectIdentifiersToPackageParams.FindChecked(OutputObjectIdentifier);
		}

		// Try to find an existing SM from a previous cook
		UStaticMesh* FoundStaticMesh = FHoudiniStaticMeshTranslator::FindExistingStaticMesh(OutputObjectIdentifier, InOutPackageParams, InOutTranslationContext);

		// Flag whether or not we need to rebuild the mesh
		bool bRebuildStaticMesh = false;
		if (HGPO.GeoInfo.bHasGeoChanged || HGPO.PartInfo.bHasChanged || InOutTranslationContext.bForceRebuild || !FoundStaticMesh || !FoundOutputObject)
			bRebuildStaticMesh = true;

		// TODO: Handle materials
		if (!bRebuildStaticMesh && !bMaterialHasChanged)
		{
			// We can simply reuse the found static mesh
			InOutTranslationContext.OutputObjects.Add(OutputObjectIdentifier, *FoundOutputObject);
			continue;
		}

		// Prepare LOD Group data for this static mesh
		FStaticMeshLODGroup LODGroup;

		bool bNewStaticMeshCreated = false;
		if (!FoundStaticMesh)
		{
			// If we couldn't find a valid existing static mesh, create a new one
			FoundStaticMesh = FHoudiniStaticMeshTranslator::CreateNewUnrealStaticMesh(HGPO, InOutPackageParams, OutputObjectIdentifier.SplitIdentifier);
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(FoundStaticMesh->GetLODGroup());
#else
			LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(FoundStaticMesh->LODGroup);
#endif
		}

		if (SplitType == EHoudiniSplitType::Normal)
		{
			MainStaticMesh = FoundStaticMesh;
			MainStaticMesh->ComplexCollisionMesh = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			MainStaticMesh->SetCustomizedCollision(false);
#else
			MainStaticMesh->bCustomizedCollision = false;
#endif
		}

		if (!FoundOutputObject)
		{
			FHoudiniOutputObject NewOutputObject;
			FoundOutputObject = &InOutTranslationContext.OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
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

		FoundOutputObject->DataLayers = FHoudiniDataLayerUtils::GetDataLayers(OutputObjectIdentifier.GeoId, OutputObjectIdentifier.PartId);
		FoundOutputObject->HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(OutputObjectIdentifier.GeoId, OutputObjectIdentifier.PartId);

		// By default, always work on the first source model, unless we're a LOD
		int32 SrcModelIndex = 0;
		int32 LODIndex = 0;
		if (SplitType == EHoudiniSplitType::LOD)
		{
			for (auto& curSplit : CachedMeshPart.SplitGroups)
			{
				EHoudiniSplitType CurrentSplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit);
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
				if (CachedMeshPart.GetPositions(HGPO).IsValidIndex(NeededVertexIndex * 3 + 2))
				{
					// We need to swap Z and Y coordinate here, and convert from m to cm. 
					VertexPositions[VertexID].X = CachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
					VertexPositions[VertexID].Y = CachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
					VertexPositions[VertexID].Z = CachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
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
			TMap<TObjectPtr<UMaterialInterface>, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

			// Get this split's faces
			TArray<int32>& SplitGroupFaceIndices = CachedMeshPart.Splits.FindChecked(SplitGroupName).FaceIndices;
			// Array holding the materials needed for this split
			//TArray<UMaterialInterface*> SplitMaterials;
			// Split Material indices per face, by default all faces are set to use the first Material
			TArray<int32> SplitFaceMaterialIndices;
			SplitFaceMaterialIndices.SetNumZeroed(SplitGroupFaceIndices.Num());

			bool HasHoudiniMaterials = CachedMeshPart.GetUniqueMaterialIds(HGPO).Num() > 0;
			bool HasMaterialOverrides = CachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0;
			if (!HasHoudiniMaterials && !HasMaterialOverrides)
			{
				// We don't have any material override or houdini material
				// we just need one polygon group using the default Houdini material.
				UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

				// See if we have a replacement material and use it on the mesh instead
				const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(
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
				if (CachedMeshPart.IsOnlyOneFaceMaterial(HGPO) || CachedMeshPart.GetUniqueMaterialIds(HGPO).Num() == 1)
				{
					// We have only one Houdini material.
					// Use default Houdini material if no valid material is assigned to any of the faces.
					UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

					// Get id of this single material.
					FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
					const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, CachedMeshPart.GetFaceMaterialIds(HGPO)[0], MaterialPathName);
					const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
					TObjectPtr<UMaterialInterface> * FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
					if (FoundMaterial)
						MaterialInterface = *FoundMaterial;

					// See if we have a replacement material and use it on the mesh instead
					const TObjectPtr<UMaterialInterface>* ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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

					// Reset material face assignments.
					for (int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
					{
						int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];
						if (!CachedMeshPart.GetFaceMaterialIds(HGPO).IsValidIndex(SplitFaceIndex))
							continue;

						// Get material id for this face.
						HAPI_NodeId MaterialId = CachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

						// See if we have already treated that material
						TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
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
							TObjectPtr<UMaterialInterface> * FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
							if (FoundMaterial)
								MaterialInterface = *FoundMaterial;

							// See if we have a replacement material and use it on the mesh instead
							const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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
					if (CachedMeshPart.GetFaceMaterialOverrides(HGPO).IsValidIndex(SplitFaceIndex))
					{
						// For MeshDescription specifically, we need OutputAssignmentMaterials/MapHoudiniMatAttributesToUnrealInterface
						// to have an entry for each index (distinguish between identical materials in different slots).
						// This is so we have the correct size and create enough polygon groups later.
						const FHoudiniMaterialInfo& MatInfo = CachedMeshPart.GetFaceMaterialOverrides(HGPO)[SplitFaceIndex];
						const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
						FString MaterialName = MatInfo.MaterialObjectPath;
					FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MaterialName, MatIndex);
						const TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
						if (FoundMaterialInterface)
							MaterialInterface = *FoundMaterialInterface;

						if (!MaterialInterface)
						{
							// Try to locate the corresponding material interface

							// Start by looking in our assignment map
							FoundMaterialInterface = InOutOutputAssignmentMaterials.Find(MaterialIdentifier);
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
								InOutOutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);
								
								// See if we have a replacement material and use it on the mesh instead
								const TObjectPtr<UMaterialInterface> *ReplacementMaterialInterface = InReplacementMaterials.Find(MaterialIdentifier);
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
							HAPI_NodeId MaterialId = CachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

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
								TObjectPtr<UMaterialInterface> * FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
								if (FoundMaterial)
									MaterialInterface = *FoundMaterial;

								if (MaterialInterface)
								{
									// Make sure this material is in the assignments before replacing it.
									InOutOutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

									// See if we have a replacement material and use it on the mesh instead
									const TObjectPtr<UMaterialInterface> * ReplacementMaterialInterface = InReplacementMaterials.Find(DefaultMatIdentifier);
									if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
										MaterialInterface = *ReplacementMaterialInterface;

									// Map the Houdini ID to the unreal one
									MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
								}
							}
						}
					}

					CurrentFaceMaterialIdx = FHoudiniMeshTranslatorUtils::GetFaceMaterialIndex(MaterialInterface,
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
			int32 NumberOfMaterials = InOutOutputAssignmentMaterials.Num();
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
				for (auto& CurrentMatAssignement : InOutOutputAssignmentMaterials)
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
			// Get the normals for this split
			TArray<float> SplitNormals;
			FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
				SplitVertexList, CachedMeshPart.GetNormalAttributeInfo(HGPO), CachedMeshPart.GetNormals(HGPO), SplitNormals);

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

				// Get the Tangents for this split
				FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
					SplitVertexList, CachedMeshPart.GetTangentUAttributeInfo(HGPO), CachedMeshPart.GetTangentU(HGPO), SplitTangentU);

				// Get the binormals for this split
				FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
					SplitVertexList, CachedMeshPart.GetTangentVAttributeInfo(HGPO), CachedMeshPart.GetTangentV(HGPO), SplitTangentV);

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
			// Get the colors values for this split
			TArray<float> SplitColors;
			FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
				SplitVertexList, CachedMeshPart.GetColorAttributeInfo(HGPO), CachedMeshPart.GetColors(HGPO), SplitColors);

			// Extract the alpha values
			// Get the colors values for this split
			TArray<float> SplitAlphas;
			FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
				SplitVertexList, CachedMeshPart.GetAlphaAttributeInfo(HGPO), CachedMeshPart.GetAlphas(HGPO), SplitAlphas);
			TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = 
				MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);

			// Extract UVs
			// See if we need to transfer uv point attributes to vertex attributes.
			int32 UVSetCount = CachedMeshPart.GetUVSets(HGPO, true).Num();
			TArray<TArray<float>> SplitUVSets;
			SplitUVSets.SetNum(UVSetCount);
			for (int32 TexCoordIdx = 0; TexCoordIdx < UVSetCount; TexCoordIdx++)
			{
				FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
					SplitVertexList, CachedMeshPart.GetUVSetAttributeInfos(HGPO)[TexCoordIdx], CachedMeshPart.GetUVSets(HGPO)[TexCoordIdx], SplitUVSets[TexCoordIdx]);
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
			bool bHasRGBA = bHasRGB && CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize == 4;
			bool bHasAlpha = SplitAlphas.Num() > 0;

			// Bug #147854
			// Use SplitUVs here - as that where we'll read UVs from
			TArray<bool> HasUVSets;
			HasUVSets.SetNumZeroed(SplitUVSets.Num());
			for (int32 Idx = 0; Idx < SplitUVSets.Num(); Idx++)
				HasUVSets[Idx] = SplitUVSets[Idx].Num() > 0;

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
							SplitColors[SplitIndex * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + 0], 0.0f, 1.0f);
						Color.G = FMath::Clamp(
							SplitColors[SplitIndex * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + 1], 0.0f, 1.0f);
						Color.B = FMath::Clamp(
							SplitColors[SplitIndex * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + 2], 0.0f, 1.0f);
					}
					// Alpha
					if (bHasAlpha)
					{
						Color.A = FMath::Clamp(SplitAlphas[SplitIndex], 0.0f, 1.0f);
					}
					else if (bHasRGBA)
					{
						Color.A = FMath::Clamp(SplitColors[SplitIndex * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + 3], 0.0f, 1.0f);
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
			// Get the FaceSmoothing values for this split
			TArray<int32> SplitFaceSmoothingMasks;
			FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<int32>(
				SplitVertexList, CachedMeshPart.GetFaceSmoothingMaskAttributeInfo(HGPO), CachedMeshPart.GetFaceSmoothingMasks(HGPO), SplitFaceSmoothingMasks);

			// FaceSmoothing masks must be initialized even if we don't have a value from Houdini!
			// TODO: Expose the default FaceSmoothing value
			// 0 will make hard face
			TArray<uint32> FaceSmoothingMasks;
			FaceSmoothingMasks.SetNumUninitialized(SplitVertexCount / 3);
			for (int32 n = 0; n < FaceSmoothingMasks.Num(); n++)
				FaceSmoothingMasks[n] = InOutTranslationContext.DefaultMeshSmoothing;

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
			// make sure the mesh has a new lighting guid
			FoundStaticMesh->SetLightingGuid(FGuid::NewGuid());
		}

		// Update the Build Settings using the default setting values
		FHoudiniMeshTranslatorUtils::UpdateMeshBuildSettings(
			InOutTranslationContext.StaticMeshBuildSettings,
			SrcModel->BuildSettings,
			bHasNormal,
			bHasTangents,
			CachedMeshPart.GetUVSets(HGPO).Num() > 0);

		// Set the lightmap Coordinate Index
		// If we have more than one UV set, the 2nd valid set is used for lightmaps by convention
		FoundStaticMesh->SetLightMapCoordinateIndex(CachedMeshPart.GetUVSets(HGPO).Num() > 1 ? 1 : 0);

		// Check for a lightmapa resolution override
		int32 LightMapResolutionOverride = -1;
		if (CachedMeshPart.GetLightmapResolutions(HGPO).Num() > 0)
			LightMapResolutionOverride = CachedMeshPart.GetLightmapResolutions(HGPO)[0];

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		FoundStaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);
#else
		FoundStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
#endif
		
		// LOD Screensize
		// default values has already been set, see if we have any attribute override for this
		float screensize = FHoudiniMeshTranslatorUtils::GetLODSCreensizeForSplit(HGPO, InOutCachedMeshPart, SplitGroupName);
		if (screensize >= 0.0f)
		{
			// Only apply the LOD screensize if it's valid
			SrcModel->ScreenSize = screensize;
			//FoundStaticMesh->GetSourceModel(LodIndex).ScreenSize = screensize;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			FoundStaticMesh->SetAutoComputeLODScreenSize(false);
#else
			FoundStaticMesh->bAutoComputeLODScreenSize = false;
#endif
		}

		FHoudiniMeshTranslatorUtils::UpdateStaticMeshNaniteSettings(HGPO.GeoId, HGPO.PartId, OutputObjectIdentifier.PrimitiveIndex, FoundStaticMesh);

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

				return FHoudiniMeshTranslatorUtils::TryToFindPropertyOnSourceModel(
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
			InOutTranslationContext.OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
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
		EHoudiniSplitType CurrentSplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(Current.Key.SplitIdentifier);
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

		// Set the default Static Mesh Generation Properties
		if (IsValid(BodySetup))
		{
			// Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
			BodySetup->bDoubleSidedGeometry = InOutTranslationContext.StaticMeshGenerationProperties.bGeneratedDoubleSidedGeometry;
			// Assign physical material for simple collision.
			BodySetup->PhysMaterial = InOutTranslationContext.StaticMeshGenerationProperties.GeneratedPhysMaterial;
			// Body Instance Properties
			BodySetup->DefaultInstance.CopyBodyInstancePropertiesFrom(&InOutTranslationContext.StaticMeshGenerationProperties.DefaultBodyInstance);
		}

		EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(CurrentObjId.SplitIdentifier);

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

			FKAggregateGeom* CurrentAggColl = InOutTranslationContext.AllAggregateCollisions.Find(CurrentObjId);
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
			FHoudiniStaticMeshTranslator::ApplyComplexColliderHelper(
					MainStaticMesh,
					SM,
					SplitType,
					bAssignedCustomCollisionMesh,
					InOutTranslationContext.OutputObjects.Find(CurrentObjId));
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
	HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() executed in %f seconds."), time_end - TimeStart);

	return true;
}

bool
FHoudiniProxyMeshTranslator::CreateHoudiniStaticMesh(FBuildContext& InOutBuildContext)
{
	const FHoudiniGeoPartObject& HGPO = InOutBuildContext.HGPO;
	FHoudiniCachedMeshPart& InOutCachedMeshPart = InOutBuildContext.CachedMeshPart;
	FHoudiniPackageParams& InOutPackageParams = InOutBuildContext.PackageParams;
	UObject* InOuterComponent = InOutBuildContext.OuterComponent;
	FHoudiniMeshTranslationContext& InOutTranslationContext = InOutBuildContext.TranslationContext;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials = InOutBuildContext.OutputAssignmentMaterials;
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials = InOutBuildContext.ReplacementMaterials;
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;

	const bool bDoTiming = FHoudiniMeshTranslatorUtils::IsMeshBuildTimingEnabled();

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh);

	const double time_start = FPlatformTime::Seconds();

	// Keep a copy of the initial package params, since InOutPackageParams is modified in place when resolving attributes
	FHoudiniPackageParams InitialPackageParams = InOutPackageParams;

	bool bIsGammaCorrectionDisabled = FHoudiniMeshTranslatorUtils::IsGammaCorrectionDisabled(HGPO.GeoId, HGPO.PartId);

	// Determine if there is "main" geo, if not we'll use the first LOD
	// as main geo
	bool bHasMainGeo = false;
	for (auto& curSplit : CachedMeshPart.SplitGroups)
	{
		if (FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(curSplit) == EHoudiniSplitType::Normal)
		{
			bHasMainGeo = true;
			break;
		}
	}

	const bool bMaterialHasChanged = FHoudiniMeshTranslatorUtils::HavePartMaterialsChanged(HGPO, CachedMeshPart);

	// Map of Houdini Material IDs to Unreal Material Interface
	TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>> MapHoudiniMatIdToUnrealInterface;
	// Map of Houdini Material Attributes to Unreal Material Interface
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> MapHoudiniMatAttributesToUnrealInterface;
	// Map of Unreal Material Interface to Unreal Material Index, per visible mesh
	TMap<UHoudiniStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	// bool MeshMaterialsHaveBeenReset = false;

	double tick = FPlatformTime::Seconds();
	if(bDoTiming)
		HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh() - Pre Split-Loop in %f seconds."), tick - time_start);

	// Map of object identifiers to package params
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniPackageParams> ObjectIdentifiersToPackageParams;

	// Iterate through all detected split groups we care about and split geometry.
	bool bMainGeoOrFirstLODFound = false;
	for (int32 SplitId = 0; SplitId < CachedMeshPart.SplitGroups.Num(); SplitId++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Per Split);

		// Get split group name
		const FString& SplitGroupName = CachedMeshPart.SplitGroups[SplitId];
		FPreparedHoudiniMeshSplit& PreparedSplit = CachedMeshPart.Splits.FindChecked(SplitGroupName);

		// Get the current split type
		EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(SplitGroupName);
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
			if(CachedMeshPart.SplitGroups.Num() > 1)
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
		TArray<int32>& SplitVertexList = PreparedSplit.VertexList;

		// Get valid count of vertex indices for this split.
		const int32& SplitVertexCount = PreparedSplit.VertexCount;

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
		FHoudiniOutputObjectIdentifier OutputObjectIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(HGPO, InOutCachedMeshPart, SplitGroupName, SplitType);

		// Try to find existing properties for this identifier
		FHoudiniOutputObject* FoundOutputObject = InOutTranslationContext.InputObjects.Find(OutputObjectIdentifier);

		// If we don't yet have package params for this object identifier, fetch and resolve attributes for the split
		// and update the package params
		TMap<FString, FString> TempAttributes;
		TMap<FString, FString> TempTokens;
		bool bCopyAttributesAndTokens = false;
		if (!ObjectIdentifiersToPackageParams.Contains(OutputObjectIdentifier))
		{
			// Get all the supported attributes from the HGPO
			FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, OutputObjectIdentifier, TempAttributes, TempTokens);

			// Resolve our final package params
			FHoudiniAttributeResolver Resolver;
			FHoudiniPackageParams FinalPackageParams;
			FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
				InitialPackageParams,
				IsValid(InOuterComponent) ? InOuterComponent->GetWorld() : nullptr,
				InOuterComponent,
				TempAttributes,
				TempTokens,
				InOutPackageParams,
				Resolver);

			bCopyAttributesAndTokens = true;
			ObjectIdentifiersToPackageParams.Emplace(OutputObjectIdentifier, InOutPackageParams);
		}
		else
		{
			InOutPackageParams = ObjectIdentifiersToPackageParams.FindChecked(OutputObjectIdentifier);
		}

		// Try to find an existing DM from a previous cook
		UHoudiniStaticMesh* FoundStaticMesh = FHoudiniProxyMeshTranslator::FindExistingHoudiniStaticMesh(OutputObjectIdentifier, InOutPackageParams, InOutTranslationContext);

		// Flag whether or not we need to rebuild the mesh
		bool bRebuildStaticMesh = false;
		if (HGPO.GeoInfo.bHasGeoChanged || HGPO.PartInfo.bHasChanged || InOutTranslationContext.bForceRebuild || !FoundStaticMesh || !FoundOutputObject)
			bRebuildStaticMesh = true;

		// TODO: Handle materials
		if (!bRebuildStaticMesh && !bMaterialHasChanged)
		{
			// We can simply reuse the found static mesh
			InOutTranslationContext.OutputObjects.Add(OutputObjectIdentifier, *FoundOutputObject);
			continue;
		}

		bool bNewStaticMeshCreated = false;
		if (!FoundStaticMesh)
		{
			// If we couldn't find a valid existing dynamic mesh, create a new one
			FoundStaticMesh = FHoudiniProxyMeshTranslator::CreateNewHoudiniStaticMesh(HGPO, InOutPackageParams, OutputObjectIdentifier.SplitIdentifier);
			if (!IsValid(FoundStaticMesh))
				continue;

			bNewStaticMeshCreated = true;
		}

		if (!FoundOutputObject)
		{
			// If we couldnt find a previous output object, create a new one
			FHoudiniOutputObject NewOutputObject;
			FoundOutputObject = &InOutTranslationContext.OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
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
			// Get the normals for this split
			TArray<float> SplitNormals;
			FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
				SplitVertexList, CachedMeshPart.GetNormalAttributeInfo(HGPO), CachedMeshPart.GetNormals(HGPO), SplitNormals);

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

				// Get the Tangents for this split
				FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
					SplitVertexList, CachedMeshPart.GetTangentUAttributeInfo(HGPO), CachedMeshPart.GetTangentU(HGPO), SplitTangentU);

				// Get the binormals for this split
				FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
					SplitVertexList, CachedMeshPart.GetTangentVAttributeInfo(HGPO), CachedMeshPart.GetTangentV(HGPO), SplitTangentV);

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
			// Get the colors values for this split
			TArray<float> SplitColors;
			FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
				SplitVertexList, CachedMeshPart.GetColorAttributeInfo(HGPO), CachedMeshPart.GetColors(HGPO), SplitColors);

			// Extract this part's alpha values if needed
			// Get the colors values for this split
			TArray<float> SplitAlphas;
			FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
				SplitVertexList, CachedMeshPart.GetAlphaAttributeInfo(HGPO), CachedMeshPart.GetAlphas(HGPO), SplitAlphas);

			const int32 ColorsCount = CachedMeshPart.GetColorAttributeInfo(HGPO).exists ? SplitColors.Num() / CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize : 0;
			const bool bSplitColorValid = CachedMeshPart.GetColorAttributeInfo(HGPO).exists && (CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize >= 3) && ColorsCount > 0;
			const bool bSplitAlphaValid = CachedMeshPart.GetAlphaAttributeInfo(HGPO).exists && (SplitAlphas.Num() == ColorsCount);

			//--------------------------------------------------------------------------------------------------------------------- 
			//  UVS
			//--------------------------------------------------------------------------------------------------------------------- 

			// Extract this part's UV sets if needed
			// See if we need to transfer uv point attributes to vertex attributes.
			int32 NumUVLayers = 0;
			TArray<TArray<float>> SplitUVSets;
			SplitUVSets.SetNum(MAX_STATIC_TEXCOORDS);
			for (int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
			{
				FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
					SplitVertexList, CachedMeshPart.GetUVSetAttributeInfos(HGPO)[TexCoordIdx], CachedMeshPart.GetUVSets(HGPO)[TexCoordIdx], SplitUVSets[TexCoordIdx]);
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
			//
			// Initialize mesh
			// 
			const int32 NumVertexPositions = NeededVertices.Num();
			const int32 NumTriangles = TriangleIndices.Num() / 3;
			const bool bHasPerFaceMaterials = CachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0 || (CachedMeshPart.GetUniqueMaterialIds(HGPO).Num() > 0 && !CachedMeshPart.IsOnlyOneFaceMaterial(HGPO));

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
					if (!CachedMeshPart.GetPositions(HGPO).IsValidIndex(NeededVertexIndex * 3 + 2))
					{
						// Error retrieving positions.
						bHasInvalidPositionIndexData = true;
						continue;
					}

					// We need to swap Z and Y coordinate here, and convert from m to cm. 
					FoundStaticMesh->SetVertexPosition(VertexPositionIdx, FVector3f(
						CachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
						CachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
						CachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION
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
					if (bSplitColorValid && SplitColors.IsValidIndex(TriVertIdx0 * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + 3 * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize - 1))
					{
						FLinearColor VertexLinearColor;
						for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
						{
							VertexLinearColor.R = FMath::Clamp(
								SplitColors[TriVertIdx0 * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 0], 0.0f, 1.0f);
							VertexLinearColor.G = FMath::Clamp(
								SplitColors[TriVertIdx0 * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 1], 0.0f, 1.0f);
							VertexLinearColor.B = FMath::Clamp(
								SplitColors[TriVertIdx0 * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 2], 0.0f, 1.0f);

							if (bSplitAlphaValid)
							{
								VertexLinearColor.A = FMath::Clamp(SplitAlphas[TriVertIdx0 + ElementIdx], 0.0f, 1.0f);
							}
							else if (CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize >= 4)
							{
								VertexLinearColor.A = FMath::Clamp(
									SplitColors[TriVertIdx0 * CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + CachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 3], 0.0f, 1.0f);
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
			FHoudiniMeshTranslatorUtils::UpdateMeshBuildSettings(
				InOutTranslationContext.StaticMeshBuildSettings,
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
		TArray<int32>& SplitFaceIndices = CachedMeshPart.Splits.FindChecked(SplitGroupName).FaceIndices;

		// Fetch the FoundMesh's Static Materials array
		TArray<FStaticMaterial>& FoundStaticMaterials = FoundStaticMesh->GetStaticMaterials();

		// Clear the materials array of the mesh the first time we encounter it
		if (!MapUnrealMaterialInterfaceToUnrealIndexPerMesh.Contains(FoundStaticMesh))
		{
			FoundStaticMaterials.Empty();
		}
		TMap<TObjectPtr<UMaterialInterface>, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

		// Process material overrides first
		if (CachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Per Face Material Overrides);

			// Array used to avoid constantly attempting to load invalid materials
			TArray<FHoudiniMaterialIdentifier> InvalidMaterials;

			for (int32 FaceIdx = 0; FaceIdx < SplitFaceIndices.Num(); ++FaceIdx)
			{
				int32 SplitFaceIndex = SplitFaceIndices[FaceIdx];
				if (!CachedMeshPart.GetFaceMaterialOverrides(HGPO).IsValidIndex(SplitFaceIndex))
					continue;

				UMaterialInterface * MaterialInterface = nullptr;
				int32 CurrentFaceMaterialIdx = 0;
				const FHoudiniMaterialInfo& MatInfo = CachedMeshPart.GetFaceMaterialOverrides(HGPO)[SplitFaceIndex];
				const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
				TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				if (!MaterialInterface)
				{
					// Try to locate the corresponding material interface

					// Start by looking in our assignment map
					FoundMaterialInterface = InOutOutputAssignmentMaterials.Find(MaterialIdentifier);
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
						InOutOutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

						// See if we have a replacement material and use it on the mesh instead
						const TObjectPtr<UMaterialInterface>* ReplacementMaterialInterface = InReplacementMaterials.Find(MaterialIdentifier);
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
						HAPI_NodeId MaterialId = CachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

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
							TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
							if (FoundMaterial)
								MaterialInterface = *FoundMaterial;

							// See if we have a replacement material and use it on the mesh instead
							const TObjectPtr<UMaterialInterface>* ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
							if (ReplacementMaterial && *ReplacementMaterial)
								MaterialInterface = *ReplacementMaterial;

							// Map the Houdini ID to the unreal one
							MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
						}
					}
				}

				if (MaterialInterface)
				{
					CurrentFaceMaterialIdx = FHoudiniMeshTranslatorUtils::GetFaceMaterialIndex(MaterialInterface,
						MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
						MatInfo.MaterialIndex,
						FoundStaticMaterials);
					// Update the Face Material on the mesh
					FoundStaticMesh->SetTriangleMaterialID(FaceIdx, CurrentFaceMaterialIdx);
				}
			}
		}
		else if (CachedMeshPart.GetUniqueMaterialIds(HGPO).Num() > 0)
		{
			// The part has houdini materials
			if (CachedMeshPart.IsOnlyOneFaceMaterial(HGPO))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Single Material);

				// Use default Houdini material if no valid material is assigned to any of the faces.
				UMaterialInterface * MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

				// Get id of this single material.
				FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
				const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, CachedMeshPart.GetFaceMaterialIds(HGPO)[0], MaterialPathName);
				const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
				TObjectPtr<UMaterialInterface> * FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
				if (FoundMaterial)
					MaterialInterface = *FoundMaterial;

				// See if we have a replacement material and use it on the mesh instead
				const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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
					if (!CachedMeshPart.GetFaceMaterialIds(HGPO).IsValidIndex(SplitFaceIndex))
						continue;

					// Get material id for this face.
					HAPI_NodeId MaterialId = CachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

					// See if we have already treated that material
					TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
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
						TObjectPtr<UMaterialInterface> * FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
						if (FoundMaterial)
							MaterialInterface = *FoundMaterial;

						// See if we have a replacement material and use it on the mesh instead
						const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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
			const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(
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
			InOutTranslationContext.OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
		}
	}

	const double time_end = FPlatformTime::Seconds();
	HOUDINI_LOG_MESSAGE(TEXT("CreateHoudiniStaticMesh() executed in %f seconds."), time_end - time_start);

	return true;
}

void
FHoudiniStaticMeshTranslator::ApplyComplexColliderHelper(
	UStaticMesh* TargetStaticMesh,
	UStaticMesh* ComplexStaticMesh,
	EHoudiniSplitType SplitType,
	bool& bAssignedCustomCollisionMesh,
	FHoudiniOutputObject* OutputObject)
{
	if (SplitType == EHoudiniSplitType::InvisibleComplexCollider && TargetStaticMesh)
	{
		if (!bAssignedCustomCollisionMesh)
		{
			bAssignedCustomCollisionMesh = true;
			TargetStaticMesh->ComplexCollisionMesh = ComplexStaticMesh;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			TargetStaticMesh->SetCustomizedCollision(true);
#else
			TargetStaticMesh->bCustomizedCollision = true;
#endif
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







UStaticMesh*
FHoudiniStaticMeshTranslator::FindExistingStaticMesh(
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniPackageParams& InPackageParams,
	const FHoudiniMeshTranslationContext& InTranslationContext)
{
	// See if we already have an input object for that output identifier
	FHoudiniOutputObject const * FoundOutputObjectPtr = InTranslationContext.InputObjects.Find(InIdentifier);
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
		FoundOutputObjectPtr = InTranslationContext.OutputObjects.Find(InIdentifier);
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
		if (!InPackageParams.HasMatchingPackageDirectories(FoundStaticMesh))
		{
			FoundStaticMesh = nullptr;
		}
	}

	return FoundStaticMesh;
}

UHoudiniStaticMesh*
FHoudiniProxyMeshTranslator::FindExistingHoudiniStaticMesh(
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniPackageParams& InPackageParams,
	const FHoudiniMeshTranslationContext& InTranslationContext)
{
	// See if we already have an input object for that output identifier
	FHoudiniOutputObject const * FoundOutputObjectPtr = InTranslationContext.InputObjects.Find(InIdentifier);
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
		FoundOutputObjectPtr = InTranslationContext.OutputObjects.Find(InIdentifier);
		if (!FoundOutputObjectPtr)
			return nullptr;

		// Make sure it's a valid static mesh
		FoundStaticMesh = Cast<UHoudiniStaticMesh>(FoundOutputObjectPtr->ProxyObject);
		if (!IsValid(FoundStaticMesh))
			return nullptr;
	}

	// Check that the existing mesh is in the expected directory (temp folder could have been changed between
	// cooks).
	if (!InPackageParams.HasMatchingPackageDirectories(FoundStaticMesh))
	{
		FoundStaticMesh = nullptr;
	}

	return FoundStaticMesh;
}


bool
FHoudiniStaticMeshTranslator::AddConvexCollisionToAggregate(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FString& SplitGroupName,
	FKAggregateGeom& AggCollisions)
{
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;
	// Get the vertex indices for the split group
	TArray<int32>& SplitGroupVertexList = CachedMeshPart.Splits.FindChecked(SplitGroupName).VertexList;

	// We're only interested in unique vertices
	TArray<int32> UniqueVertexIndexes;
	for (int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++)
	{
		int32 Index = SplitGroupVertexList[VertexIdx];
		if (!CachedMeshPart.GetPositions(HGPO).IsValidIndex(Index))
			continue;

		UniqueVertexIndexes.AddUnique(Index);
	}

	// Extract the collision geo's vertices
	TArray< FVector > VertexArray;
	VertexArray.SetNum(UniqueVertexIndexes.Num());
	for (int32 Idx = 0; Idx < UniqueVertexIndexes.Num(); Idx++)
	{
		int32 VertexIndex = UniqueVertexIndexes[Idx];
		if (!CachedMeshPart.GetPositions(HGPO).IsValidIndex(VertexIndex * 3 + 2))
			continue;

		VertexArray[Idx].X = CachedMeshPart.GetPositions(HGPO)[VertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Idx].Y = CachedMeshPart.GetPositions(HGPO)[VertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Idx].Z = CachedMeshPart.GetPositions(HGPO)[VertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
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
			if (!CachedMeshPart.GetPositions(HGPO).IsValidIndex(Index))
				continue;

			Indices.Add(Index);
		}

		// But we need all the positions as vertex
		TArray<FVector3f> Vertices;
		Vertices.SetNum(CachedMeshPart.GetPositions(HGPO).Num() / 3);

		for (int32 Idx = 0; Idx < Vertices.Num(); Idx++)
		{
			Vertices[Idx].X = CachedMeshPart.GetPositions(HGPO)[Idx * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[Idx].Y = CachedMeshPart.GetPositions(HGPO)[Idx * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			Vertices[Idx].Z = CachedMeshPart.GetPositions(HGPO)[Idx * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
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


/*
int32
FHoudiniMeshTranslator::GetSplitNormals(
	const TArray<int32>& InSplitVertexList, TArray<FVector>& OutNormals)
{
	// Extract the normals
	UpdatePartNormalsIfNeeded();

	// Get the normals for this split
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
		InSplitVertexList, CachedMeshPart.GetNormalAttributeInfo(HGPO), CachedMeshPart.GetNormals(HGPO), VertexData);

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
	int32 WedgeCount = FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
		InSplitVertexList, CachedMeshPart.GetNormalAttributeInfo(HGPO), CachedMeshPart.GetNormals(HGPO), VertexData);

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
FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<FVector2D>& OutData,
	const float& ScaleFactor)
{
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
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
FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<FVector2D>& OutData,
	const float& ScaleFactor)
{
	TArray<float> VertexData;
	int32 WedgeCount = FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
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


















bool
FHoudiniStaticMeshTranslator::UpdateMeshOnStaticMeshComponent(UStaticMeshComponent *InComponent, UObject *InMesh)
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
FHoudiniProxyMeshTranslator::UpdateMeshOnHoudiniStaticMeshComponent(UHoudiniStaticMeshComponent* InComponent, UObject* InMesh)
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


bool 
FHoudiniStaticMeshTranslator::AddActorsToMeshSocket(UStaticMeshSocket * Socket, UStaticMeshComponent * StaticMeshComponent, 
		TArray<TObjectPtr<AActor>> & HoudiniCreatedSocketActors, TArray<TObjectPtr<AActor>> & HoudiniAttachedSocketActors)
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

		if (!EditorWorld)
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




void 
FHoudiniStaticMeshTranslator::BuildMeshDescription(
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FHoudiniGeoPartObject& InHGPO,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutputAssignmentMaterials,
	int32 InDefaultMeshSmoothing,
	FMeshDescription* MeshDescription,
	FHoudiniGroupedMeshPrimitives& SplitMeshData)
{
	FHoudiniCachedMeshPart& InCachedMeshPart = InOutCachedMeshPart;
	bool bHasNormal = SplitMeshData.Normals.Num() > 0;
	bool bHasTangents = SplitMeshData.TangentU.Num() > 0 && SplitMeshData.TangentV.Num() > 0;
	bool bHasRGB = SplitMeshData.Colors.Num() > 0;
	bool bHasRGBA = bHasRGB && InCachedMeshPart.GetColorAttributeInfo(InHGPO).tupleSize == 4;
	bool bHasAlpha = SplitMeshData.Alphas.Num() > 0;
	int UVSetCount = InCachedMeshPart.GetUVSets(InHGPO).Num();
	uint32 FaceCount = SplitMeshData.Indices.Num() / 3;

	// Create a Polygon Group for each material slot
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
		MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	// We must use the number of assignment materials found to reserve the number of material slots
	// Don't use the SM's StaticMaterials here as we may not reserve enough polygon groups when adding more materials
	// Create a polygon group for each material slot.
	int32 NumberOfMaterials = InOutputAssignmentMaterials.Num();
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
		for (const auto& CurrentMatAssignement : InOutputAssignmentMaterials)
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
		if (InCachedMeshPart.GetPositions(InHGPO).IsValidIndex(NeededVertexIndex * 3 + 2))
		{
			// We need to swap Z and Y coordinate here, and convert from m to cm. 
			VertexPositions[VertexID].X = InCachedMeshPart.GetPositions(InHGPO)[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			VertexPositions[VertexID].Y = InCachedMeshPart.GetPositions(InHGPO)[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			VertexPositions[VertexID].Z = InCachedMeshPart.GetPositions(InHGPO)[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
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
			InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, SplitMeshData.SplitId, *SplitMeshData.SplitGroupName);
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

	// Bug #147854
	// Use SplitUVs here - as that where we'll read UVs from
	TArray<bool> HasUVSets;
	HasUVSets.SetNumZeroed(SplitMeshData.UVSets.Num());
	for (int32 Idx = 0; Idx < SplitMeshData.UVSets.Num(); Idx++)
		HasUVSets[Idx] = SplitMeshData.UVSets[Idx].Num() > 0;

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
				Color.R = FMath::Clamp(SplitMeshData.Colors[SplitIndex * InCachedMeshPart.GetColorAttributeInfo(InHGPO).tupleSize + 0], 0.0f, 1.0f);
				Color.G = FMath::Clamp(SplitMeshData.Colors[SplitIndex * InCachedMeshPart.GetColorAttributeInfo(InHGPO).tupleSize + 1], 0.0f, 1.0f);
				Color.B = FMath::Clamp(SplitMeshData.Colors[SplitIndex * InCachedMeshPart.GetColorAttributeInfo(InHGPO).tupleSize + 2], 0.0f, 1.0f);
			}
			// Alpha
			if (bHasAlpha)
			{
				Color.A = FMath::Clamp(SplitMeshData.Alphas[SplitIndex], 0.0f, 1.0f);
			}
			else if (bHasRGBA)
			{
				Color.A = FMath::Clamp(SplitMeshData.Colors[SplitIndex * InCachedMeshPart.GetColorAttributeInfo(InHGPO).tupleSize + 3], 0.0f, 1.0f);
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
	const int32 SplitVertexCount = InCachedMeshPart.Splits.FindChecked(SplitMeshData.SplitGroupName).VertexCount;


	// FaceSmoothing masks must be initialized even if we don't have a value from Houdini!
	// TODO: Expose the default FaceSmoothing value  0 will make hard face
	TArray<uint32> FaceSmoothingMasks;
	FaceSmoothingMasks.SetNumUninitialized(SplitVertexCount);
	for (int32 n = 0; n < FaceSmoothingMasks.Num(); n++)
		FaceSmoothingMasks[n] = InDefaultMeshSmoothing;


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

void FHoudiniStaticMeshTranslator::ProcessMaterials(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
	UStaticMesh* FoundStaticMesh,
	FHoudiniGroupedMeshPrimitives& SplitMeshData)
{
	FHoudiniCachedMeshPart& InCachedMeshPart = InOutCachedMeshPart;
	// Map of Houdini Material IDs to Unreal Material Interface
	TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>> MapHoudiniMatIdToUnrealInterface;
	// Map of Houdini Material Attributes to Unreal Material Interface
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> MapHoudiniMatAttributesToUnrealInterface;
	// Map of Unreal Material Interface to Unreal Material Index, per visible mesh
	TMap<UStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

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
	TMap<TObjectPtr<UMaterialInterface>, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

	// Get this split's faces
	const TArray<int32>& SplitGroupFaceIndices = InCachedMeshPart.Splits.FindChecked(SplitMeshData.SplitGroupName).FaceIndices;
	// Array holding the materials needed for this split
	//TArray<UMaterialInterface*> SplitMaterials;
	// Split Material indices per face, by default all faces are set to use the first Material

	SplitMeshData.FaceMaterialIndices.SetNumZeroed(SplitGroupFaceIndices.Num());

	bool HasHoudiniMaterials = InCachedMeshPart.GetUniqueMaterialIds(HGPO).Num() > 0;
	bool HasMaterialOverrides = InCachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0;
	if (!HasHoudiniMaterials && !HasMaterialOverrides)
	{
		// We don't have any material override or houdini material
		// we just need one polygon group using the default Houdini material.
		UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

		// See if we have a replacement material and use it on the mesh instead
		const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(
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
		if (InCachedMeshPart.IsOnlyOneFaceMaterial(HGPO) || InCachedMeshPart.GetUniqueMaterialIds(HGPO).Num() == 1)
		{
			// We have only one Houdini material.
			// Use default Houdini material if no valid material is assigned to any of the faces.
			UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

			// Get id of this single material.
			FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, InCachedMeshPart.GetFaceMaterialIds(HGPO)[0], MaterialPathName);
			const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
			TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
			if (FoundMaterial)
				MaterialInterface = *FoundMaterial;

			// See if we have a replacement material and use it on the mesh instead
			const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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

			// Reset material face assignments.
			for (int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
			{
				int32 SplitFaceIndex = SplitGroupFaceIndices[FaceIdx];
				if (!InCachedMeshPart.GetFaceMaterialIds(HGPO).IsValidIndex(SplitFaceIndex))
					continue;

				// Get material id for this face.
				HAPI_NodeId MaterialId = InCachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

				// See if we have already treated that material
				TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
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
					TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
					if (FoundMaterial)
						MaterialInterface = *FoundMaterial;

					// See if we have a replacement material and use it on the mesh instead
					const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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
			if (InCachedMeshPart.GetFaceMaterialOverrides(HGPO).IsValidIndex(SplitFaceIndex))
			{
				// For MeshDescription specifically, we need OutputAssignmentMaterials/MapHoudiniMatAttributesToUnrealInterface
				// to have an entry for each index (distinguish between identical materials in different slots).
				// This is so we have the correct size and create enough polygon groups later.
				const FHoudiniMaterialInfo& MatInfo = InCachedMeshPart.GetFaceMaterialOverrides(HGPO)[SplitFaceIndex];
				const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
				FString MaterialName = MatInfo.MaterialObjectPath;
			FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MaterialName, MatIndex);
				TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
				if (FoundMaterialInterface)
					MaterialInterface = *FoundMaterialInterface;

				if (!MaterialInterface)
				{
					// Try to locate the corresponding material interface

					// Start by looking in our assignment map
					FoundMaterialInterface = InOutOutputAssignmentMaterials.Find(MaterialIdentifier);
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
						InOutOutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

						// See if we have a replacement material and use it on the mesh instead
						const TObjectPtr<UMaterialInterface> * ReplacementMaterialInterface = InReplacementMaterials.Find(MaterialIdentifier);
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
					HAPI_NodeId MaterialId = InCachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

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
						TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
						if (FoundMaterial)
							MaterialInterface = *FoundMaterial;

						// See if we have a replacement material and use it on the mesh instead
						const TObjectPtr<UMaterialInterface> * ReplacementMaterialInterface = InReplacementMaterials.Find(DefaultMatIdentifier);
						if (ReplacementMaterialInterface && *ReplacementMaterialInterface)
							MaterialInterface = *ReplacementMaterialInterface;

						// Map the Houdini ID to the unreal one
						MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
					}
				}
			}

			CurrentFaceMaterialIdx = FHoudiniMeshTranslatorUtils::GetFaceMaterialIndex(MaterialInterface,
				MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
				MatIndex,
				FoundStaticMaterials);

			// Update the Face Material on the mesh
			SplitMeshData.FaceMaterialIndices[FaceIdx] = CurrentFaceMaterialIdx;
		}
	}
}

void FHoudiniStaticMeshTranslator::PullMeshData(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
	FHoudiniGroupedMeshPrimitives& SplitMeshData,
	UStaticMesh* FoundStaticMesh,
	int LODIndex,
	bool bReadTangents)
{
	const bool bDoTiming = FHoudiniMeshTranslatorUtils::IsMeshBuildTimingEnabled();
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

	const int32 SplitVertexCount = InOutCachedMeshPart.Splits.FindChecked(SplitMeshData.SplitGroupName).VertexCount;
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

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIALS
	//---------------------------------------------------------------------------------------------------------------------

	ProcessMaterials(HGPO, InOutCachedMeshPart, InOutOutputAssignmentMaterials, InReplacementMaterials, FoundStaticMesh, SplitMeshData);


	if (bDoTiming)
	{
		HOUDINI_LOG_MESSAGE(TEXT("CreateStaticMesh_MeshDescription() - Materials in %f seconds."), FPlatformTime::Seconds() - tick);
		tick = FPlatformTime::Seconds();
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// NORMALS
	//---------------------------------------------------------------------------------------------------------------------

	// Extract the normals
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitMeshData.VertexList,
		InOutCachedMeshPart.GetNormalAttributeInfo(HGPO),
		InOutCachedMeshPart.GetNormals(HGPO),
		SplitMeshData.Normals);

	//--------------------------------------------------------------------------------------------------------------------- 
	// TANGENTS
	//---------------------------------------------------------------------------------------------------------------------

	if (bReadTangents)
	{
		// Extract this part's Tangents if needed

		// Get the Tangents and binormals for this split
		FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
			SplitMeshData.VertexList,
			InOutCachedMeshPart.GetTangentUAttributeInfo(HGPO),
			InOutCachedMeshPart.GetTangentU(HGPO),
			SplitMeshData.TangentU);
		FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
			SplitMeshData.VertexList,
			InOutCachedMeshPart.GetTangentVAttributeInfo(HGPO),
			InOutCachedMeshPart.GetTangentV(HGPO),
			SplitMeshData.TangentV);

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

	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitMeshData.VertexList,
		InOutCachedMeshPart.GetColorAttributeInfo(HGPO),
		InOutCachedMeshPart.GetColors(HGPO),
		SplitMeshData.Colors);

	//--------------------------------------------------------------------------------------------------------------------- 
	// ALPHA
	//---------------------------------------------------------------------------------------------------------------------

	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitMeshData.VertexList,
		InOutCachedMeshPart.GetAlphaAttributeInfo(HGPO),
		InOutCachedMeshPart.GetAlphas(HGPO),
		SplitMeshData.Alphas);

	//--------------------------------------------------------------------------------------------------------------------- 
	// UV SETS
	//---------------------------------------------------------------------------------------------------------------------

	// See if we need to transfer uv point attributes to vertex attributes.
	int32 UVSetCount = InOutCachedMeshPart.GetUVSets(HGPO, true).Num();

	SplitMeshData.UVSets.SetNum(UVSetCount);
	for (int32 TexCoordIdx = 0; TexCoordIdx < UVSetCount; TexCoordIdx++)
	{
		FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
			SplitMeshData.VertexList,
			InOutCachedMeshPart.GetUVSetAttributeInfos(HGPO)[TexCoordIdx],
			InOutCachedMeshPart.GetUVSets(HGPO)[TexCoordIdx],
			SplitMeshData.UVSets[TexCoordIdx]);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// FACE SMOOTHING
	//---------------------------------------------------------------------------------------------------------------------

	FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<int32>(
		SplitMeshData.VertexList,
		InOutCachedMeshPart.GetFaceSmoothingMaskAttributeInfo(HGPO),
		InOutCachedMeshPart.GetFaceSmoothingMasks(HGPO),
		SplitMeshData.FaceSmoothingMasks);

}

void
FHoudiniStaticMeshTranslator::SetPhysicsMaterialFromHGPO(const FHoudiniGeoPartObject& InHGPO, UBodySetup* BodySetup)
{
	// Set physical material if present
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	TArray<FString> AttributeValues;

	FHoudiniHapiAccessor Accessor(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL);
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

bool FHoudiniStaticMeshTranslator::ParseSplitToken(FString & Name, const FString & Token)
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

void FHoudiniStaticMeshTranslator::ClassifySplitGroup(FHoudiniGroupedMeshPrimitives& SplitMeshData)
{
	// This function takes the Houdini name of the split group and classifies as renderable,
	// lod, collision or a combination of all.

	FString Name = SplitMeshData.SplitGroupName;
	const FString OriginalName = Name;

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
				SplitMeshData.ComplexCollisionOwner = Name.IsEmpty() ? FString(HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION) : Name;
				SplitMeshData.StaticMeshName = Name.IsEmpty() ? OriginalName : Name;
				SplitMeshData.bRendered = true;
			}
		}
	}

	if (!bFoundRender && !bFoundCollision)
	{
		// If neither render nor collision found then assume the name represents a split mesh.
		SplitMeshData.bRendered = true;
	}

	if (SplitMeshData.CollisionType == EHoudiniCollisionType::CustomComplex)
		return;

	if (SplitMeshData.bIsLOD || (bFoundCollision && !bFoundRender))
	{
		// LODs and invisible simple colliders are built into the default main mesh.
		SplitMeshData.StaticMeshName = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
	}
	else if (bFoundCollision && bFoundRender)
	{
		// Rendered colliders are standalone meshes.
		SplitMeshData.StaticMeshName = OriginalName;
	}
	else
	{
		SplitMeshData.StaticMeshName = Name;
	}
}

FHoudiniMeshToBuild FHoudiniStaticMeshTranslator::ScanOutputForMeshesToBuild(const FHoudiniGeoPartObject& HGPO, const TArray<FString>& InSplitGroups)
{
	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Parse the split group names already extracted for this part.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	TArray<FHoudiniGroupedMeshPrimitives> SplitMeshes;

	for (int Index = 0; Index < InSplitGroups.Num(); ++Index)
	{
		FHoudiniGroupedMeshPrimitives SplitMeshData;
		SplitMeshData.SplitGroupName = InSplitGroups[Index];
		ClassifySplitGroup(SplitMeshData);
		SplitMeshes.Add(SplitMeshData);
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
	// 1. There is an owner static mesh, in which case we generate a second static mesh and link it as a custom complex collision
	//	or
	// 2. There is no static mesh already with this name. So we just create ONE static mesh and do some processing when we create the components to make it invisible.

	for (auto& SplitMesh : SplitMeshes)
	{
		if (SplitMesh.CollisionType == EHoudiniCollisionType::CustomComplex)
		{
			if (MeshesToBuild.Meshes.Contains(SplitMesh.ComplexCollisionOwner))
			{
				// case 1 above
				if (SplitMesh.StaticMeshName == SplitMesh.ComplexCollisionOwner)
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

void
FHoudiniMeshTranslationContext::RemovePreviousOutputs()
{
	for(auto It : InputObjects)
	{
		FHoudiniOutputObject* FoundOutputObject = &It.Value;
		for(auto Component : FoundOutputObject->OutputComponents)
		{
			FHoudiniEngineUtils::RemoveAndDestroyComponent(Component);
		}
		FoundOutputObject->OutputComponents.Empty();

		if(IsValid(FoundOutputObject->ProxyComponent))
		{
			FHoudiniEngineUtils::RemoveAndDestroyComponent(FoundOutputObject->ProxyComponent);
			FoundOutputObject->ProxyComponent = nullptr;
		}
	}
	InputObjects.Empty();
}

UStaticMesh* 
FHoudiniStaticMeshTranslator::CreateStaticMesh(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniPackageParams& InOutPackageParams,
	const FString& MeshName,
	int NumLODs)
{
	UStaticMesh* StaticMesh = CreateNewUnrealStaticMesh(HGPO, InOutPackageParams, MeshName);

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
FHoudiniStaticMeshTranslator::CreateStaticMeshesFromSplitGroups(FBuildContext& InOutBuildContext)
{
	FHoudiniGeoPartObject& HGPO = InOutBuildContext.HGPO;
	FHoudiniCachedMeshPart& InOutCachedMeshPart = InOutBuildContext.CachedMeshPart;
	FHoudiniPackageParams& InOutPackageParams = InOutBuildContext.PackageParams;
	UObject* InOuterComponent = InOutBuildContext.OuterComponent;
	FHoudiniMeshTranslationContext& InOutTranslationContext = InOutBuildContext.TranslationContext;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials = InOutBuildContext.OutputAssignmentMaterials;
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials = InOutBuildContext.ReplacementMaterials;
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;
	InOutTranslationContext.RemovePreviousOutputs();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Pull various settings before creating the mesh
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Loop through and build each mesh.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	bool bSuccess = true;
	for (auto & It : CachedMeshPart.MeshesToBuild.Meshes)
	{
		bSuccess &= CreateStaticMeshFromSplitGroups(HGPO, InOutCachedMeshPart, InOutPackageParams, InOutTranslationContext, InOutOutputAssignmentMaterials, InReplacementMaterials, It.Key, It.Value);
	}

	// Once all meshes have been built, patch up custom collision refences
	for (auto& It : CachedMeshPart.MeshesToBuild.Meshes)
	{
		auto & Mesh =  It.Value;
		if (!Mesh.CustomCollisionOwner.IsEmpty())
		{
			auto * Owner = CachedMeshPart.MeshesToBuild.Meshes.Find(Mesh.CustomCollisionOwner);
			if (Owner && Owner->UnrealStaticMesh)
			{
				Owner->UnrealStaticMesh->ComplexCollisionMesh = Mesh.UnrealStaticMesh;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
				Owner->UnrealStaticMesh->SetCustomizedCollision(true);
#else
				Owner->UnrealStaticMesh->bCustomizedCollision = true;
#endif
			}
		}
	}

	return bSuccess;

}

bool
FHoudiniStaticMeshTranslator::CreateStaticMeshFromSplitGroups(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	FHoudiniPackageParams& InOutPackageParams,
	FHoudiniMeshTranslationContext& InOutTranslationContext,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
	const FString& MeshName,
	FHoudiniSplitGroupMesh& SplitMeshData)
{
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;
	const bool bDoTiming = FHoudiniMeshTranslatorUtils::IsMeshBuildTimingEnabled();
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

	SplitMeshData.UnrealStaticMesh = CreateStaticMesh(HGPO, InOutPackageParams, MeshName, NumLODs);
	if (!IsValid(SplitMeshData.UnrealStaticMesh))
		return false;

	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	FStaticMeshLODGroup LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);

	// Create Output
	const int32 OutputSplitGroupIndex = SplitMeshData.LODRenders.IsEmpty() ? 0 : SplitMeshData.LODRenders[0];
	const FString& OutputSplitGroupName = SplitMeshData.SplitMeshData[OutputSplitGroupIndex].SplitGroupName;
	const EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(OutputSplitGroupName);
	SplitMeshData.OutputObjectIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(
		HGPO, CachedMeshPart, OutputSplitGroupName, SplitType);

	FHoudiniOutputObject* OutputObject = &InOutTranslationContext.OutputObjects.Add(SplitMeshData.OutputObjectIdentifier, {});
	InOutTranslationContext.InputObjects.Remove(SplitMeshData.OutputObjectIdentifier);
	OutputObject->bProxyIsCurrent = false;
	OutputObject->OutputObject = SplitMeshData.UnrealStaticMesh;
	// Component creation handles instanced mesh parts separately. Keep this aligned with the
	// non-split path so the source mesh output is retained alongside its instancer output.
	OutputObject->bIsImplicit = false;
	OutputObject->bIsInvisibleCollisionMesh = !SplitMeshData.bIsVisible;
	OutputObject->DataLayers = FHoudiniDataLayerUtils::GetDataLayers(HGPO.GeoId, HGPO.PartId);
	OutputObject->HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(HGPO.GeoId, HGPO.PartId);



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

		RenderGroup.VertexList = CachedMeshPart.Splits.FindChecked(RenderGroup.SplitGroupName).VertexList;
		PullMeshData(HGPO, CachedMeshPart, InOutOutputAssignmentMaterials, InReplacementMaterials, RenderGroup, SplitMeshData.UnrealStaticMesh, LODIndex, bReadTangents);

		FMeshDescription* MeshDescription = SplitMeshData.UnrealStaticMesh->CreateMeshDescription(LODIndex);
		FStaticMeshAttributes(*MeshDescription).Register();
		BuildMeshDescription(
			CachedMeshPart,
			HGPO,
			InOutOutputAssignmentMaterials,
			InOutTranslationContext.DefaultMeshSmoothing,
			MeshDescription,
			RenderGroup);

		bool bHasNormal = RenderGroup.Normals.Num() > 0;
		bool bHasTangents = RenderGroup.TangentU.Num() > 0 || RenderGroup.TangentV.Num() > 0;

		// Update the Build Settings using the default setting values
		FStaticMeshSourceModel* SrcModel = (SplitMeshData.UnrealStaticMesh->IsSourceModelValid(LODIndex)) ? &(SplitMeshData.UnrealStaticMesh->GetSourceModel(LODIndex)) : nullptr;
		FHoudiniMeshTranslatorUtils::UpdateMeshBuildSettings(InOutTranslationContext.StaticMeshBuildSettings, SrcModel->BuildSettings, bHasNormal, bHasTangents, CachedMeshPart.GetUVSets(HGPO).Num() > 0);

		// Store the new MeshDescription
		SplitMeshData.UnrealStaticMesh->CommitMeshDescription(LODIndex);

		// Set screen size.
		float ScreenSize = FHoudiniMeshTranslatorUtils::GetLODSCreensizeForSplit(HGPO, InOutCachedMeshPart, RenderGroup.SplitGroupName);
		if (ScreenSize >= 0.0f)
		{
			SrcModel->ScreenSize = ScreenSize;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			SplitMeshData.UnrealStaticMesh->SetAutoComputeLODScreenSize(false);
#else
			SplitMeshData.UnrealStaticMesh->bAutoComputeLODScreenSize = false;
#endif
		}

		FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, InOutCachedMeshPart, RenderGroup.SplitGroupName, OutputObject->CachedAttributes, OutputObject->CachedTokens);

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

				return FHoudiniMeshTranslatorUtils::TryToFindPropertyOnSourceModel(
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
	SplitMeshData.UnrealStaticMesh->SetLightMapCoordinateIndex(CachedMeshPart.GetUVSets(HGPO).Num() > 1 ? 1 : 0);

	// Check for a lightmapa resolution override.
	if (CachedMeshPart.GetLightmapResolutions(HGPO).Num() > 0 && CachedMeshPart.GetLightmapResolutions(HGPO)[0] > 0)
		SplitMeshData.UnrealStaticMesh->SetLightMapResolution(CachedMeshPart.GetLightmapResolutions(HGPO)[0]);
	else
		SplitMeshData.UnrealStaticMesh->SetLightMapResolution(64);

	// Nananite settings.
	FHoudiniMeshTranslatorUtils::UpdateStaticMeshNaniteSettings(HGPO.GeoId, HGPO.PartId, SplitMeshData.OutputObjectIdentifier.PrimitiveIndex, SplitMeshData.UnrealStaticMesh);

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

		FKAggregateGeom CollisionGeometry = FHoudiniMeshTranslatorUtils::BuildAggregateCollision(HGPO, InOutCachedMeshPart, SplitMeshData);

		if (CollisionGeometry.GetElementCount() > 0)
		{
			BodySetup->AddCollisionFrom(CollisionGeometry);
			BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
		}

		SetPhysicsMaterialFromHGPO(HGPO, BodySetup);

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	SplitMeshData.UnrealStaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);
#else
	SplitMeshData.UnrealStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
#endif
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


bool
FHoudiniProxyMeshTranslator::CreateHoudiniStaticMeshesFromSplitGroups(FBuildContext& InOutBuildContext)
{
	FHoudiniGeoPartObject& HGPO = InOutBuildContext.HGPO;
	FHoudiniCachedMeshPart& InOutCachedMeshPart = InOutBuildContext.CachedMeshPart;
	FHoudiniPackageParams& InOutPackageParams = InOutBuildContext.PackageParams;
	UObject* InOuterComponent = InOutBuildContext.OuterComponent;
	FHoudiniMeshTranslationContext& InOutTranslationContext = InOutBuildContext.TranslationContext;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials = InOutBuildContext.OutputAssignmentMaterials;
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials = InOutBuildContext.ReplacementMaterials;
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;
	InOutTranslationContext.RemovePreviousOutputs();

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Pull various settings before creating the mesh
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	//-----------------------------------------------------------------------------------------------------------------------------------------------
	// Loop through and build each mesh.
	//-----------------------------------------------------------------------------------------------------------------------------------------------

	TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>> MapHoudiniMatIdToUnrealInterface;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> MapHoudiniMatAttributesToUnrealInterface;
	TMap<UHoudiniStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>> MapUnrealMaterialInterfaceToUnrealIndexPerMesh;

	bool bSuccess = true;
	for (auto& It : CachedMeshPart.MeshesToBuild.Meshes)
	{
		bSuccess &= CreateHoudiniStaticMeshFromSplitGroups(HGPO, InOutCachedMeshPart, InOutPackageParams, InOuterComponent, InOutTranslationContext, InOutOutputAssignmentMaterials, InReplacementMaterials, It.Key, It.Value, MapHoudiniMatIdToUnrealInterface, MapHoudiniMatAttributesToUnrealInterface, MapUnrealMaterialInterfaceToUnrealIndexPerMesh);
	}

	// Once all meshes have been built, patch up custom collision refences
	for (auto& It : CachedMeshPart.MeshesToBuild.Meshes)
	{
		auto& Mesh = It.Value;
		if (!Mesh.CustomCollisionOwner.IsEmpty())
		{
			auto* Owner = CachedMeshPart.MeshesToBuild.Meshes.Find(Mesh.CustomCollisionOwner);
			if (Owner && Owner->UnrealStaticMesh)
			{

				Owner->UnrealStaticMesh->ComplexCollisionMesh = Mesh.UnrealStaticMesh;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
				Owner->UnrealStaticMesh->SetCustomizedCollision(true);
#else
				Owner->UnrealStaticMesh->bCustomizedCollision = true;
#endif
			}
		}
	}

	return bSuccess;

}


bool
FHoudiniProxyMeshTranslator::CreateHoudiniStaticMeshFromSplitGroups(const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	FHoudiniPackageParams& InOutPackageParams,
	UObject* InOuterComponent,
	FHoudiniMeshTranslationContext& InOutTranslationContext,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
	const FString& MeshName,
	FHoudiniSplitGroupMesh& SplitMeshData,
	TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>> & MapHoudiniMatIdToUnrealInterface,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> & MapHoudiniMatAttributesToUnrealInterface,
	TMap<UHoudiniStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>> & MapUnrealMaterialInterfaceToUnrealIndexPerMesh)
{
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;
	const bool bDoTiming = FHoudiniMeshTranslatorUtils::IsMeshBuildTimingEnabled();
	double tick = FPlatformTime::Seconds();

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMeshFromSplitGroups -- Per Split);

	// Houdini Static Meshes only create a mesh for the top LOD.
	if (SplitMeshData.LODRenders.Num() == 0)
		return true;

	FHoudiniGroupedMeshPrimitives & Group =  SplitMeshData.SplitMeshData[SplitMeshData.LODRenders[0]];

	FString & SplitGroupName = Group.SplitGroupName;
	FPreparedHoudiniMeshSplit& PreparedSplit = CachedMeshPart.Splits.FindChecked(SplitGroupName);

	// Get the vertex indices for this group
	TArray<int32>& SplitVertexList = PreparedSplit.VertexList;

	// Get valid count of vertex indices for this split.
	const int32& SplitVertexCount = PreparedSplit.VertexCount;

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
	const EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(SplitGroupName);
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(
		HGPO, CachedMeshPart, SplitGroupName, SplitType);

	// Try to find existing properties for this identifier
	FHoudiniOutputObject* FoundOutputObject = InOutTranslationContext.InputObjects.Find(OutputObjectIdentifier);

	// If we don't yet have package params for this object identifier, fetch and resolve attributes for the split
	// and update the package params
	TMap<FString, FString> TempAttributes;
	TMap<FString, FString> TempTokens;
	bool bCopyAttributesAndTokens = false;

	// Get all the supported attributes from the HGPO
	FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, OutputObjectIdentifier, TempAttributes, TempTokens);

	// Resolve our final package params
	FHoudiniAttributeResolver Resolver;
	FHoudiniPackageParams FinalPackageParams;
	FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
		InOutPackageParams,
		IsValid(InOuterComponent) ? InOuterComponent->GetWorld() : nullptr,
		InOuterComponent,
		TempAttributes,
		TempTokens,
		InOutPackageParams,
		Resolver);

	bCopyAttributesAndTokens = true;


	// Try to find an existing DM from a previous cook
	UHoudiniStaticMesh* FoundStaticMesh = CreateNewHoudiniStaticMesh(HGPO, InOutPackageParams, OutputObjectIdentifier.SplitIdentifier);

	if (!FoundOutputObject)
	{
		// If we couldnt find a previous output object, create a new one
		FHoudiniOutputObject NewOutputObject;
		FoundOutputObject = &InOutTranslationContext.OutputObjects.Add(OutputObjectIdentifier, NewOutputObject);
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

	BuildHoudiniMesh(HGPO, CachedMeshPart, InOutTranslationContext, SplitGroupName, FoundStaticMesh);

	//--------------------------------------------------------------------------------------------------------------------- 
	// MATERIALS / FACE MATERIALS
	//---------------------------------------------------------------------------------------------------------------------

	ProcessMaterialsForHSM(HGPO, CachedMeshPart, SplitGroupName, FoundStaticMesh, InOutOutputAssignmentMaterials, InReplacementMaterials, MapHoudiniMatIdToUnrealInterface, MapHoudiniMatAttributesToUnrealInterface, MapUnrealMaterialInterfaceToUnrealIndexPerMesh);

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
		InOutTranslationContext.OutputObjects.FindOrAdd(OutputObjectIdentifier, *FoundOutputObject);
	}
	return true;
}

void FHoudiniProxyMeshTranslator::BuildHoudiniMesh(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FHoudiniMeshTranslationContext& InTranslationContext,
	const FString& SplitGroupName,
	UHoudiniStaticMesh* FoundStaticMesh)
{
	FPreparedHoudiniMeshSplit& PreparedSplit = InOutCachedMeshPart.Splits.FindChecked(SplitGroupName);
	// Get the vertex indices for this group
	TArray<int32>& SplitVertexList = PreparedSplit.VertexList;

	// Get valid count of vertex indices for this split.
	const int32& SplitVertexCount = PreparedSplit.VertexCount;

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
	// Get the normals for this split
	TArray<float> SplitNormals;
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitVertexList,
		InOutCachedMeshPart.GetNormalAttributeInfo(HGPO),
		InOutCachedMeshPart.GetNormals(HGPO),
		SplitNormals);

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

		// Get the Tangents for this split
		FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
			SplitVertexList,
			InOutCachedMeshPart.GetTangentUAttributeInfo(HGPO),
			InOutCachedMeshPart.GetTangentU(HGPO),
			SplitTangentU);

		// Get the binormals for this split
		FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
			SplitVertexList,
			InOutCachedMeshPart.GetTangentVAttributeInfo(HGPO),
			InOutCachedMeshPart.GetTangentV(HGPO),
			SplitTangentV);

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
	// Get the colors values for this split
	TArray<float> SplitColors;
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitVertexList,
		InOutCachedMeshPart.GetColorAttributeInfo(HGPO),
		InOutCachedMeshPart.GetColors(HGPO),
		SplitColors);

	// Extract this part's alpha values if needed
	// Get the colors values for this split
	TArray<float> SplitAlphas;
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitVertexList,
		InOutCachedMeshPart.GetAlphaAttributeInfo(HGPO),
		InOutCachedMeshPart.GetAlphas(HGPO),
		SplitAlphas);

	const int32 ColorsCount = InOutCachedMeshPart.GetColorAttributeInfo(HGPO).exists ? SplitColors.Num() / InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize : 0;
	const bool bSplitColorValid = InOutCachedMeshPart.GetColorAttributeInfo(HGPO).exists && (InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize >= 3) && ColorsCount > 0;
	const bool bSplitAlphaValid = InOutCachedMeshPart.GetAlphaAttributeInfo(HGPO).exists && (SplitAlphas.Num() == ColorsCount);

	//--------------------------------------------------------------------------------------------------------------------- 
	//  UVS
	//--------------------------------------------------------------------------------------------------------------------- 

	// Extract this part's UV sets if needed
	// See if we need to transfer uv point attributes to vertex attributes.
	int32 NumUVLayers = 0;
	TArray<TArray<float>> SplitUVSets;
	SplitUVSets.SetNum(MAX_STATIC_TEXCOORDS);
	for (int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
	{
		FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
			SplitVertexList,
			InOutCachedMeshPart.GetUVSetAttributeInfos(HGPO)[TexCoordIdx],
			InOutCachedMeshPart.GetUVSets(HGPO)[TexCoordIdx],
			SplitUVSets[TexCoordIdx]);
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
	//
	// Initialize mesh
	// 
	const int32 NumVertexPositions = NeededVertices.Num();
	const int32 NumTriangles = TriangleIndices.Num() / 3;
	const bool bHasPerFaceMaterials = InOutCachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0
		|| (InOutCachedMeshPart.GetUniqueMaterialIds(HGPO).Num() > 0 && !InOutCachedMeshPart.IsOnlyOneFaceMaterial(HGPO));

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
			if (!InOutCachedMeshPart.GetPositions(HGPO).IsValidIndex(NeededVertexIndex * 3 + 2))
			{
				// Error retrieving positions.
				bHasInvalidPositionIndexData = true;
				continue;
			}

			// We need to swap Z and Y coordinate here, and convert from m to cm. 
			FoundStaticMesh->SetVertexPosition(VertexPositionIdx, FVector3f(
				InOutCachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
				InOutCachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
				InOutCachedMeshPart.GetPositions(HGPO)[NeededVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION
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

							TangentV.X = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 0];
							TangentV.Y = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 2];
							TangentV.Z = SplitTangentV[TriVertIdx0 * 3 + 3 * ElementIdx + 1];

							FoundStaticMesh->SetTriangleVertexUTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentU);
							FoundStaticMesh->SetTriangleVertexVTangent(TriangleIdx, TriWindingIndex[ElementIdx], TangentV);
						}
					}
				}
			}

			// Vertex Colors
			if (bSplitColorValid && SplitColors.IsValidIndex(TriVertIdx0 * InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + 3 * InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize - 1))
			{
				FLinearColor VertexLinearColor;
				for (int32 ElementIdx = 0; ElementIdx < 3; ++ElementIdx)
				{
					VertexLinearColor.R = FMath::Clamp(
						SplitColors[TriVertIdx0 * InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 0], 0.0f, 1.0f);
					VertexLinearColor.G = FMath::Clamp(
						SplitColors[TriVertIdx0 * InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 1], 0.0f, 1.0f);
					VertexLinearColor.B = FMath::Clamp(
						SplitColors[TriVertIdx0 * InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 2], 0.0f, 1.0f);

					if (bSplitAlphaValid)
					{
						VertexLinearColor.A = FMath::Clamp(SplitAlphas[TriVertIdx0 + ElementIdx], 0.0f, 1.0f);
					}
					else if (InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize >= 4)
					{
						VertexLinearColor.A = FMath::Clamp(
							SplitColors[TriVertIdx0 * InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize + InOutCachedMeshPart.GetColorAttributeInfo(HGPO).tupleSize * ElementIdx + 3], 0.0f, 1.0f);
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
	FHoudiniMeshTranslatorUtils::UpdateMeshBuildSettings(
		InTranslationContext.StaticMeshBuildSettings,
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
FHoudiniProxyMeshTranslator::ProcessMaterialsForHSM(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FString& SplitGroupName, 
	UHoudiniStaticMesh* FoundStaticMesh,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
	TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>> & MapHoudiniMatIdToUnrealInterface,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> & MapHoudiniMatAttributesToUnrealInterface,
	TMap<UHoudiniStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>> & MapUnrealMaterialInterfaceToUnrealIndexPerMesh)
{
	FHoudiniCachedMeshPart& InCachedMeshPart = InOutCachedMeshPart;
	// Get face indices for this split.
	const TArray<int32>& SplitFaceIndices = InCachedMeshPart.Splits.FindChecked(SplitGroupName).FaceIndices;

	// Fetch the FoundMesh's Static Materials array
	TArray<FStaticMaterial>& FoundStaticMaterials = FoundStaticMesh->GetStaticMaterials();

	// Clear the materials array of the mesh the first time we encounter it
	if (!MapUnrealMaterialInterfaceToUnrealIndexPerMesh.Contains(FoundStaticMesh))
	{
		FoundStaticMaterials.Empty();
	}
	TMap<TObjectPtr<UMaterialInterface>, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh = MapUnrealMaterialInterfaceToUnrealIndexPerMesh.FindOrAdd(FoundStaticMesh);

	// Process material overrides first
	if (InCachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Per Face Material Overrides);

		// Array used to avoid constantly attempting to load invalid materials
		TArray<FHoudiniMaterialIdentifier> InvalidMaterials;

		for (int32 FaceIdx = 0; FaceIdx < SplitFaceIndices.Num(); ++FaceIdx)
		{
			int32 SplitFaceIndex = SplitFaceIndices[FaceIdx];
			if (!InCachedMeshPart.GetFaceMaterialOverrides(HGPO).IsValidIndex(SplitFaceIndex))
				continue;

			UMaterialInterface* MaterialInterface = nullptr;
			int32 CurrentFaceMaterialIdx = 0;
			const FHoudiniMaterialInfo& MatInfo = InCachedMeshPart.GetFaceMaterialOverrides(HGPO)[SplitFaceIndex];
			const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
			TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatAttributesToUnrealInterface.Find(MaterialIdentifier);
			if (FoundMaterialInterface)
				MaterialInterface = *FoundMaterialInterface;

			if (!MaterialInterface)
			{
				// Try to locate the corresponding material interface

				// Start by looking in our assignment map
				FoundMaterialInterface = InOutOutputAssignmentMaterials.Find(MaterialIdentifier);
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
					InOutOutputAssignmentMaterials.Add(MaterialIdentifier, MaterialInterface);

					// See if we have a replacement material and use it on the mesh instead
					const TObjectPtr<UMaterialInterface> * ReplacementMaterialInterface = InReplacementMaterials.Find(MaterialIdentifier);
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
					HAPI_NodeId MaterialId = InCachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

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
						TObjectPtr<UMaterialInterface> * FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
						if (FoundMaterial)
							MaterialInterface = *FoundMaterial;

						// See if we have a replacement material and use it on the mesh instead
						const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
						if (ReplacementMaterial && *ReplacementMaterial)
							MaterialInterface = *ReplacementMaterial;

						// Map the Houdini ID to the unreal one
						MapHoudiniMatIdToUnrealInterface.Add(MaterialId, MaterialInterface);
					}
				}
			}

			if (MaterialInterface)
			{
				CurrentFaceMaterialIdx = FHoudiniMeshTranslatorUtils::GetFaceMaterialIndex(MaterialInterface,
					MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
					MatInfo.MaterialIndex,
					FoundStaticMaterials);
				// Update the Face Material on the mesh
				FoundStaticMesh->SetTriangleMaterialID(FaceIdx, CurrentFaceMaterialIdx);
			}
		}
	}
	else if (InCachedMeshPart.GetUniqueMaterialIds(HGPO).Num() > 0)
	{
		// The part has houdini materials
		if (InCachedMeshPart.IsOnlyOneFaceMaterial(HGPO))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslator::CreateHoudiniStaticMesh -- Set Single Material);

			// Use default Houdini material if no valid material is assigned to any of the faces.
			UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial(HGPO.bIsTemplated).Get());

			// Get id of this single material.
			FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			const bool bFoundHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(HGPO.AssetId, InCachedMeshPart.GetFaceMaterialIds(HGPO)[0], MaterialPathName);
			const FHoudiniMaterialIdentifier DefaultMatIdentifier(MaterialPathName, bFoundHoudiniMaterial);
			TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
			if (FoundMaterial)
				MaterialInterface = *FoundMaterial;

			// See if we have a replacement material and use it on the mesh instead
			const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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
				if (!InCachedMeshPart.GetFaceMaterialIds(HGPO).IsValidIndex(SplitFaceIndex))
					continue;

				// Get material id for this face.
				HAPI_NodeId MaterialId = InCachedMeshPart.GetFaceMaterialIds(HGPO)[SplitFaceIndex];

				// See if we have already treated that material
				TObjectPtr<UMaterialInterface>* FoundMaterialInterface = MapHoudiniMatIdToUnrealInterface.Find(MaterialId);
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
					TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(DefaultMatIdentifier);
					if (FoundMaterial)
						MaterialInterface = *FoundMaterial;

					// See if we have a replacement material and use it on the mesh instead
					const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(DefaultMatIdentifier);
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
		const TObjectPtr<UMaterialInterface> * ReplacementMaterial = InReplacementMaterials.Find(
			FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, ""));
		if (ReplacementMaterial && *ReplacementMaterial)
			MaterialInterface = *ReplacementMaterial;

		FoundStaticMaterials.Empty();
		FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
	}
}






#undef LOCTEXT_NAMESPACE
