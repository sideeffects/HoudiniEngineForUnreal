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

#include "HoudiniOutputTranslator.h"

#include "HAPI/HAPI.h"

#include "HoudiniOutput.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"

#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniInput.h"
#include "HoudiniStaticMesh.h"

#include "HoudiniDataTableTranslator.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniSkeletalMeshTranslator.h"
#include "HoudiniSplineTranslator.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniLandscapeSplineTranslator.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniGeometryCollectionTranslator.h"

#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "FileHelpers.h"
#include "HoudiniDataLayerUtils.h"
#include "LandscapeInfo.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Engine/WorldComposition.h"
#include "Modules/ModuleManager.h"
#include "WorldBrowserModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollection/GeometryCollectionObject.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#endif

#include "HoudiniFoliageTools.h"
#include "HoudiniLevelInstanceUtils.h"
#include "Engine/UserDefinedStruct.h"
#include "HoudiniHLODLayerUtils.h"
#include <HoudiniAnimationTranslator.h>
#include "HoudiniFoliageUtils.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

//
bool
FHoudiniOutputTranslator::UpdateOutputs(
	UHoudiniAssetComponent* HAC,
	const bool& bInForceUpdate,
	bool& bOutHasHoudiniStaticMeshOutput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::UpdateOutputs);

	if (!IsValid(HAC))
		return false;

	RemovePreviousOutputs(HAC);

	// Outputs that should be cleared, but only AFTER new output processing have taken place.
	// This is needed for landscape resizing where the new landscape needs to copy data from the original landscape
	// before the original landscape gets destroyed.
	TArray<UHoudiniOutput*> DeferredClearOutputs;

	// Check if the HDA has been marked as not producing outputs
	if (!HAC->bOutputless)
	{
		TArray<UHoudiniOutput*> NewOutputs;
		TArray<HAPI_NodeId> OutputNodes = HAC->GetOutputNodeIds();
		TMap<HAPI_NodeId, int32> OutputNodeCookCounts = HAC->GetOutputNodeCookCounts();
		if (FHoudiniOutputTranslator::BuildAllOutputs(
			HAC->GetAssetId(), HAC, OutputNodes, OutputNodeCookCounts,
			HAC->Outputs, NewOutputs, HAC->bOutputTemplateGeos, HAC->bUseOutputNodes, HAC->bEnableCurveEditing))
		{
			// NOTE: For now we are currently forcing all outputs to be cleared here. There is still an issue where, in some
			// circumstances, landscape tiles disappear when clearing outputs after processing.
			// The reason we may need to defer landscape clearing is to allow the landscape creation code to
			// capture the extent of the landscape. The extent of the landscape can only be calculated if all landscape
			// tiles are still present in the map. If we find that we don't need this for updating of Input landscapes,
			// we can safely remove this feature.
			ClearAndRemoveOutputs(HAC, DeferredClearOutputs, true);
			// Replace with the new parameters
			HAC->Outputs = NewOutputs;
		}
	}
	else
	{
		// This HDA is marked as not supposed to produce any output
		ClearAndRemoveOutputs(HAC, DeferredClearOutputs, true);
	}

	// At the moment we don't support controlling KeepTags separately for components and actors, so if we find any
	// HGPOs with KeepTags set to true, we'll keep the tags on both actors and components. In the future we may
	// want to control these separately.
	bool bKeepTags = false; 
	
	// Look for details generic property attributes on the outputs,
	// and try to apply them to the HAC.
	// This can be used to preset some of the HDA's uproperty via attribute
	TArray<FHoudiniGenericAttribute> GenericAttributes;
	for (auto& CurrentOutput : HAC->Outputs)
	{
		const TArray<FHoudiniGeoPartObject>& CurrentOutputHGPO = CurrentOutput->GetHoudiniGeoPartObjects();
		for (auto& CurrentHGPO : CurrentOutputHGPO)
		{
			FHoudiniEngineUtils::GetGenericAttributeList(
				CurrentHGPO.GeoId,
				CurrentHGPO.PartId,
				HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, 
				GenericAttributes,
				HAPI_ATTROWNER_DETAIL);
			bKeepTags = bKeepTags || CurrentHGPO.bKeepTags;
		}
	}

	if (bKeepTags == false)
	{
		if (AActor* HActor = HAC->GetOwner())
		{
			HActor->Tags.Empty();
		}
		HAC->ComponentTags.Empty();
	}
	
	// Attempt to apply the attributes to the HAC if we have any
	for (const auto& CurrentPropAttribute : GenericAttributes)
	{
		// Get the current Property Attribute
		const FString& CurrentPropertyName = CurrentPropAttribute.AttributeName;
		if (CurrentPropertyName.IsEmpty())
			continue;

		if (!FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(HAC, CurrentPropAttribute))
			continue;

		// Success!
		HOUDINI_LOG_MESSAGE(TEXT("Modified UProperty %s on Houdini Asset Component named %s"), *CurrentPropertyName, *HAC->GetName());
	}
	
	// NOTE: PersistentWorld can be NULL when, for example, working with
	// HoudiniAssetComponents in Blueprints.
	UWorld* PersistentWorld = HAC->GetHACWorld();
	UWorldComposition* WorldComposition = nullptr;
	if (PersistentWorld)
	{
		WorldComposition = PersistentWorld->WorldComposition;
	}
	
	if (IsValid(WorldComposition))
	{
		// We don't want the origin to shift as we're potentially updating levels.
		WorldComposition->bTemporarilyDisableOriginTracking = true;
	}

	// "Process" the mesh.
	// TODO: Move this to the actual processing stage,
	// And see if some of this could be threaded
	UObject* OuterComponent = HAC;
	
	FString HoudiniAssetPath = FPaths::GetPath(HAC->GetPathName());
	FString ComponentGUIDString = HAC->GetComponentGUID().ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);
	FString HoudiniAssetNameString = HAC->GetDisplayName();

	FHoudiniPackageParams PackageParams;
	PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
	PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

	PackageParams.BakeFolder = HAC->GetBakeFolderOrDefault();
	PackageParams.TempCookFolder = HAC->GetTemporaryCookFolderOrDefault();

	PackageParams.OuterPackage = HAC->GetComponentLevel();
	PackageParams.HoudiniAssetName = HAC->GetHoudiniAssetName();
	PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetActorNameOrLabel();
	PackageParams.ComponentGUID = HAC->GetComponentGUID();
	PackageParams.ObjectName = FString();

	// ----------------------------------------------------
	// Outputs prepass
	// ----------------------------------------------------
	
	TArray<UPackage*> CreatedWorldCompositionPackages;
	bool bCreatedNewMaps = false;
	//...  for heightfield outputs  ...//

	// Collect all the landscape layers' global min/max values.
	TMap<FString, float> LandscapeLayerGlobalMinimums;
	TMap<FString, float> LandscapeLayerGlobalMaximums;
	
	// Store the instancer outputs separately so we can process them later, after all mesh output are processed.
	// Determine the total number of instances, if we have more than 1 then mesh parts with instanced geo we will not create proxy meshes
	// Also if we have object instancer (or oldschool attribute instancers), we won't be creating any proxy at all
	TArray<UHoudiniOutput*> InstancerOutputs;
	int32 NumInstances = 0;
	bool bHasObjectInstancer = false;
	
	for (auto& CurOutput : HAC->Outputs)
	{
		if (CurOutput->GetType() == EHoudiniOutputType::Instancer)
		{
			// InstancerOutputs.Add(CurOutput);
			for (const FHoudiniGeoPartObject &HGPO : CurOutput->GetHoudiniGeoPartObjects())
			{
				if (HGPO.Type == EHoudiniPartType::Instancer)
				{
					if (HGPO.InstancerType == EHoudiniInstancerType::PackedPrimitive || HGPO.InstancerType == EHoudiniInstancerType::GeometryCollection)
					{
						NumInstances += HGPO.PartInfo.InstanceCount;
					}
					else
					{
						NumInstances += HGPO.PartInfo.PointCount;
					}

					if ((HGPO.InstancerType == EHoudiniInstancerType::ObjectInstancer)
						|| (HGPO.InstancerType == EHoudiniInstancerType::OldSchoolAttributeInstancer))
					{
						bHasObjectInstancer = true;
					}
				}
			}
		}
		else if (CurOutput->GetType() == EHoudiniOutputType::Landscape)
		{
			FHoudiniLandscapeTranslator::CalcHeightFieldsArrayGlobalZMinZMax(CurOutput->GetHoudiniGeoPartObjects(), LandscapeLayerGlobalMinimums, LandscapeLayerGlobalMaximums, false);
		}
	}

	bOutHasHoudiniStaticMeshOutput = false;
	int32 NumVisibleOutputs = 0;
	int32 NumOutputs = HAC->Outputs.Num();
	bool bHasLandscape = false;

	// Get all our landscape inputs
	TArray<ALandscapeProxy *> AllInputLandscapes;
	FHoudiniEngineUtils::GatherLandscapeInputs(HAC, AllInputLandscapes);

	// ----------------------------------------------------
	// Process outputs
	// ----------------------------------------------------
	// Landscape creation will cache the first tile as a reference location
	// in this struct to be used by during construction of subsequent tiles.
	// Landscape Size info will be cached by the first tile, similar to LandscapeReferenceLocation
	FHoudiniClearedEditLayers ClearedLandscapeLayers;
	TMap<FString, ALandscape*> LandscapeMap;

	// Landscape splines track edit layers that were cleared per-landscape
	TMap<ALandscape*, TSet<FName>> ClearedLandscapeEditLayersForSplines;

	// The houdini materials that have been generated by this HDA.
	// We track them to prevent recreate the same houdini material over and over if it is assigned to multiple parts.
	// (this can easily happen when using packed prims)
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> AllOutputMaterials;

	TArray<UPackage*> CreatedPackages;
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; OutputIdx++)
	{
		UHoudiniOutput* CurOutput = HAC->Outputs[OutputIdx];
		if (!IsValid(CurOutput))
			continue;

		FString Notification = FString::Format(TEXT("Processing output {0} / {1}..."), {FString::FromInt(OutputIdx + 1), FString::FromInt(NumOutputs)});
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

		if (!HAC->IsOutputTypeSupported(CurOutput->GetType()))
		{
			continue;
		}

		switch (CurOutput->GetType())
		{
			case EHoudiniOutputType::Mesh:
			{
				bool bIsProxyStaticMeshEnabled = (
					HAC->IsProxyStaticMeshEnabled() &&
					!HAC->HasNoProxyMeshNextCookBeenRequested() &&
					!HAC->IsBakeAfterNextCookEnabled());
				if (bIsProxyStaticMeshEnabled && NumInstances > 1)
				{
					if (bHasObjectInstancer)
					{
						// Completely disable proxies if we have object instancers/old school attribute instancers
						// as they rely on having a static mesh created (and the instanced mesh HGPO is not marked as instanced...)
						bIsProxyStaticMeshEnabled = false;
					}
					else
					{
						// If we dont have proxy instancer, enable proxy only for non-instanced mesh
						for (const FHoudiniGeoPartObject &HGPO : CurOutput->GetHoudiniGeoPartObjects())
						{
							if (HGPO.bIsInstanced && HGPO.Type == EHoudiniPartType::Mesh)
							{
								bIsProxyStaticMeshEnabled = false;
								break;
							}
						}
					}
				}

				EHoudiniStaticMeshMethod MeshMethod = EHoudiniStaticMeshMethod::FMeshDescription;
				if (bIsProxyStaticMeshEnabled)
					MeshMethod = EHoudiniStaticMeshMethod::UHoudiniStaticMesh;
				
				FHoudiniMeshTranslator::CreateAllMeshesAndComponentsFromHoudiniOutput(
					CurOutput, 
					PackageParams, 
					MeshMethod,
					HAC->bSplitMeshSupport,
					HAC->StaticMeshGenerationProperties,
					HAC->StaticMeshBuildSettings,
					AllOutputMaterials,
					OuterComponent);

				NumVisibleOutputs++;

				// Look for UHoudiniStaticMesh in the output, and set bOutHasHoudiniStaticMeshOutput accordingly
				if (bIsProxyStaticMeshEnabled && !bOutHasHoudiniStaticMeshOutput)
				{
					bOutHasHoudiniStaticMeshOutput = bOutHasHoudiniStaticMeshOutput || CurOutput->HasAnyCurrentProxy();
				}

				break;
			}


			case EHoudiniOutputType::Curve:
			{
				const TArray<FHoudiniGeoPartObject> &GeoPartObjects = CurOutput->GetHoudiniGeoPartObjects();

				if (GeoPartObjects.Num() <= 0)
					continue;

				const FHoudiniGeoPartObject & CurHGPO = GeoPartObjects[0];

				if (CurOutput->IsEditableNode())
				{
					if (!CurOutput->HasEditableNodeBuilt())
					{
						// Editable curve, only need to be built once. 
						UHoudiniSplineComponent* HoudiniSplineComponent = FHoudiniSplineTranslator::CreateHoudiniSplineComponentFromHoudiniEditableNode(
							CurHGPO.GeoId, 
							CurHGPO.PartName,
							HAC);

						HoudiniSplineComponent->SetIsEditableOutputCurve(true);

						FHoudiniOutputObjectIdentifier EditableSplineComponentIdentifier;
						EditableSplineComponentIdentifier.ObjectId = CurHGPO.ObjectId;
						EditableSplineComponentIdentifier.GeoId = CurHGPO.GeoId;
						EditableSplineComponentIdentifier.PartId = CurHGPO.PartId;
						EditableSplineComponentIdentifier.PartName = CurHGPO.PartName;
						
						TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurOutput->GetOutputObjects();
						FHoudiniOutputObject& FoundOutputObject = OutputObjects.FindOrAdd(EditableSplineComponentIdentifier);
						check(FoundOutputObject.OutputComponents.Num() < 2); // Multiple components not supported yet.
						FoundOutputObject.OutputComponents.Empty();
						FoundOutputObject.OutputComponents.Add(HoudiniSplineComponent);
						CurOutput->SetHasEditableNodeBuilt(true);
					}
				}
				else
				{	
					// Output curve
					FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput(CurOutput, OuterComponent);
					NumVisibleOutputs += CurOutput->GetOutputObjects().Num();
					break;
				}
			}
			break;

		case EHoudiniOutputType::Instancer:
			InstancerOutputs.Add(CurOutput);
			break;

		case EHoudiniOutputType::Landscape:
		{
			NumVisibleOutputs++;

			// This gets called for each heightfield primitive from Houdini, i.e., each "tile".
			bool bNewMapCreated = false;

			// No Cooked prefixed needed when cooking an HDA, the name is derived internally.
			FString CookedPrefix;

			FHoudiniLandscapeTranslator::ProcessLandscapeOutput(
				CurOutput,
				AllInputLandscapes,
				CookedPrefix,
				PersistentWorld,
				PackageParams,
				LandscapeMap,
				ClearedLandscapeLayers,
				CreatedPackages);

			bHasLandscape = true;

			for (auto& Pair : CurOutput->GetOutputObjects()) 
			{
				UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(Pair.Value.OutputObject);
				if (IsValid(LayerOutput))
				{
					ALandscapeProxy* OutputLandscape = LayerOutput->Landscape;

					if (OutputLandscape && !LayerOutput->PropertyAttributes.IsEmpty())
					{
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(OutputLandscape, LayerOutput->PropertyAttributes);
						OutputLandscape->GetLandscapeInfo()->FixupProxiesTransform();
						OutputLandscape->GetLandscapeInfo()->RecreateLandscapeInfo(PersistentWorld, true);
						OutputLandscape->RecreateCollisionComponents();
						FEditorDelegates::PostLandscapeLayerUpdated.Broadcast();
					}

				}
				break;
			}

			bCreatedNewMaps |= bNewMapCreated;
			break;
		}

		case EHoudiniOutputType::DataTable:
		{
			for (auto&& HGPO : CurOutput->HoudiniGeoPartObjects)
			{
				FHoudiniDataTableTranslator::BuildDataTable(HGPO, CurOutput, PackageParams);
			}
			break;
		}

		case EHoudiniOutputType::LandscapeSpline:
		{
			if (!FHoudiniLandscapeSplineTranslator::ProcessLandscapeSplineOutput(
					CurOutput,
					AllInputLandscapes,
					PersistentWorld,
					PackageParams,
					ClearedLandscapeEditLayersForSplines))
			{
				break;
			}

			// Translation successful
			NumVisibleOutputs += CurOutput->GetOutputObjects().Num();
			break;
		}

		case EHoudiniOutputType::AnimSequence:
		{
			FHoudiniAnimationTranslator::CreateAnimSequenceFromOutput(CurOutput, PackageParams, OuterComponent);
			break;
		}

		case EHoudiniOutputType::Skeletal:
		{
			FHoudiniSkeletalMeshTranslator::ProcessSkeletalMeshOutputs(
				CurOutput, PackageParams, AllOutputMaterials, OuterComponent);

			NumVisibleOutputs++;
			break;
		}

		default:
			// Do Nothing for now
			break;
		}

		for (auto& CurMat : CurOutput->AssignmentMaterialsById)
		{
			// Add the newly generated materials if any
			if (!AllOutputMaterials.Contains(CurMat.Key))
				AllOutputMaterials.Add(CurMat);
		}
	}

	bool HasGeometryCollection = false;
	
	// Now that all meshes have been created, process the instancers
	int InstanceCount = FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs(HAC->Outputs, OuterComponent, PackageParams);
	NumVisibleOutputs += InstanceCount;

	for (auto& CurOutput : InstancerOutputs)
	{
		if (!HasGeometryCollection && FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancer(CurOutput))
		{
			HasGeometryCollection = true;
			break;
		}
	}

	if (HasGeometryCollection)
	{
		FHoudiniGeometryCollectionTranslator::SetupGeometryCollectionComponentFromOutputs(HAC->Outputs, OuterComponent, PackageParams, HAC->GetHACWorld());
	}

	if (NumVisibleOutputs > 0)
	{
		// If we have valid outputs, we don't need to display the houdini logo anymore...
		FHoudiniEngineUtils::RemoveHoudiniLogoFromComponent(HAC);
	}
	else
	{
		// ... if we don't have any valid outputs however, we should
		FHoudiniEngineUtils::AddHoudiniLogoToComponent(HAC);
	}

	// Clear any old outputs that was marked as "Should Defer Clear".
	// This should happen before SharedLandscapeActor cleanup
	// since this needs to remove old landscape proxies so that empty SharedLandscapeActors
	// can be removed afterward.
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniOutputTranslator::UpdateOutputs] Clearing old outputs: %d"), DeferredClearOutputs.Num());
	for(UHoudiniOutput* OldOutput : DeferredClearOutputs)
	{
		ClearOutput(OldOutput);
	}

	// if (IsValid(LandscapeExtents.IntermediateResizeLandscape))
	// {
	// 	LandscapeExtents.IntermediateResizeLandscape->Destroy();
	// 	LandscapeExtents.IntermediateResizeLandscape = nullptr;
	// }

	if (bHasLandscape)
	{
		// ----------------------------------------------------
		// Cleanup untracked shared landscape actors
		// ----------------------------------------------------
		// This is a nasty hack to clean up SharedLandscape actors generated by the
		// Landscape translator but aren't tracked by an HoudiniOutputObject, since the
		// translators can't dynamically create outputs.

		{
			// First collect all the landscapes that is being tracked by the HAC.
			TSet<ALandscapeProxy*> TrackedLandscapes;
			for(UHoudiniOutput* Output : HAC->Outputs)
			{
				if (Output->GetType() == EHoudiniOutputType::Landscape)
				{
					for(auto& Elem : Output->GetOutputObjects())
					{
						UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(Elem.Value.OutputObject);
						if (!IsValid(LandscapePtr))
							continue;
						ALandscapeProxy* LandscapeProxy = LandscapePtr->GetRawPtr();
						if (IsValid(LandscapeProxy))
						{
							TrackedLandscapes.Add(LandscapeProxy);

							// We need to recreate component states for landscapes if a tile was created, moved, or resized
							// otherwise the landscape will exhibit render artifacts (such as only rendering every other
							// component.)
							LandscapeProxy->RecreateComponentsState();
						}
					}
				}
			}

			// Iterate over Houdini asset child assets in order to find dangling Landscape actors
			TArray<USceneComponent*> AttachedComponents = HAC->GetAttachChildren();
			for(USceneComponent* Component : AttachedComponents)
			{
				if (!IsValid(Component))
					continue;

				AActor* Actor = Component->GetOwner();
				ALandscape* Landscape = Cast<ALandscape>(Actor);
				if (!IsValid(Landscape))
					continue;

				if (TrackedLandscapes.Contains(Landscape))
					continue;

				ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
				if (!IsValid(LandscapeInfo))
				{
					Landscape->Destroy();
					continue;
				}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 1
				if (LandscapeInfo->Proxies.Num() == 0)
#else
				if (LandscapeInfo->StreamingProxies.Num() == 0)
#endif
				{
					Landscape->Destroy();
				}	
			}
		}

		// Recreate Landscape Info calls WorldChange, so no need to do it manually.
		ULandscapeInfo::RecreateLandscapeInfo(PersistentWorld, true);

#if WITH_EDITOR
		if (GEditor)
		{
			// We force a viewport refresh since some actions, such as updating landscape
			// edit layers will not reflect until the user moves the viewport camera.
			GEditor->RedrawLevelEditingViewports(true);
		}
#endif
	}

	// Destroy the intermediate resize landscape, if there is one.
	// if (IsValid(LandscapeExtents.IntermediateResizeLandscape))
	// {
	// 	FHoudiniLandscapeTranslator::DestroyLandscape(LandscapeExtents.IntermediateResizeLandscape);
	// }

	if (IsValid(WorldComposition))
	{
		// Disable the flag that we set before starting the import process.
		WorldComposition->bTemporarilyDisableOriginTracking = false;
	}

	// If the owner component was marked as loaded, unmark all outputs
	if (HAC->HasBeenLoaded())
	{
		for (auto& CurrentOutput : HAC->Outputs)
		{
			CurrentOutput->MarkAsLoaded(false);
		}
	}

	if (bCreatedNewMaps)
	{
		// Force the asset registry to update its cache of packages paths
		// recursively for this world, otherwise world composition won't
		// detect new maps during the WorldComposition::Rescan().
		FHoudiniEngineUtils::RescanWorldPath(PersistentWorld);

		ULandscapeInfo::RecreateLandscapeInfo(PersistentWorld, true);

		FHoudiniEngineUtils::LogWorldInfo(PersistentWorld);
		if (WorldComposition)
		{
			UWorldComposition::WorldCompositionChangedEvent.Broadcast(PersistentWorld);
		}

		// NOTE: We are unable to force the world outliner to update it's list of actors from sublevels so the
		// user has to unload / reload the sublevels to see the tiles in the world outliner.

		FEditorDelegates::RefreshLevelBrowser.Broadcast();
		FEditorDelegates::RefreshAllBrowsers.Broadcast();
	}

	for (auto& CurrentOutput : HAC->Outputs)
	{
		for(auto & It : CurrentOutput->OutputObjects)
		{
			FHoudiniOutputObjectIdentifier & Id = It.Key;
			FHoudiniOutputObject & Obj = It.Value;

			if (Obj.DataLayers.IsEmpty())
				Obj.DataLayers = FHoudiniDataLayerUtils::GetDataLayers(Id.GeoId, Id.PartId);

			if (Obj.HLODLayers.IsEmpty())
				Obj.HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(Id.GeoId, Id.PartId);
		}
	}

	FHoudiniLevelInstanceUtils::FetchLevelInstanceParameters(HAC->Outputs);

	if (CreatedPackages.Num() > 0)
	{
		// Save created packages. For example, we don't want landscape layers deleted 
		// along with the HDA.
		FEditorFileUtils::PromptForCheckoutAndSave(CreatedPackages, true, false);
	}

	return true;
}

void
FHoudiniOutputTranslator::RemovePreviousOutputs(UHoudiniAssetComponent* HAC)
{
	for(auto Output : HAC->Outputs)
	{
			Output->DestroyCookedData();
	}
	HAC->Outputs.Empty();
}

bool
FHoudiniOutputTranslator::BuildStaticMeshesOnHoudiniProxyMeshOutputs(UHoudiniAssetComponent* HAC, bool bInDestroyProxies)
{
	if (!IsValid(HAC))
		return false;

	UObject* OuterComponent = HAC;

	FHoudiniPackageParams PackageParams;
	PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
	PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

	PackageParams.BakeFolder = HAC->GetBakeFolderOrDefault();
	PackageParams.TempCookFolder = HAC->GetTemporaryCookFolderOrDefault();

	PackageParams.OuterPackage = HAC->GetComponentLevel();
	PackageParams.HoudiniAssetName = HAC->GetHoudiniAssetName();
	PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetActorNameOrLabel();
	PackageParams.ComponentGUID = HAC->GetComponentGUID();
	PackageParams.ObjectName = FString();

	// Keep track of all generated houdini materials to avoid recreating them over and over
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> AllOutputMaterials;

	bool bFoundProxies = false;
	TArray<UHoudiniOutput*> InstancerOutputs;
	for (auto& CurOutput : HAC->Outputs)
	{
		const EHoudiniOutputType OutputType = CurOutput->GetType();
		if (OutputType == EHoudiniOutputType::Mesh)
		{
			if (CurOutput->HasAnyCurrentProxy())
			{
				bFoundProxies = true;
				FHoudiniMeshTranslator::CreateAllMeshesAndComponentsFromHoudiniOutput(
					CurOutput,
					PackageParams,
					EHoudiniStaticMeshMethod::FMeshDescription,
					HAC->bSplitMeshSupport,
					HAC->StaticMeshGenerationProperties,
					HAC->StaticMeshBuildSettings,
					AllOutputMaterials,
					OuterComponent,
					true,  // bInTreatExistingMaterialsAsUpToDate
					bInDestroyProxies
				);  
			}
		}
		else if (OutputType == EHoudiniOutputType::Instancer)
		{
			InstancerOutputs.Add(CurOutput);
		}

		for (auto& CurMat : CurOutput->AssignmentMaterialsById)
		{
			//Adds the generated materials if any
			if (!AllOutputMaterials.Contains(CurMat.Key))
				AllOutputMaterials.Add(CurMat);
		}
	}

	// Rebuild instancers if we built any static meshes from proxies
	if (bFoundProxies)
	{
		FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs(HAC->Outputs, OuterComponent, PackageParams);
	}

	return true;
}

//
bool
FHoudiniOutputTranslator::UpdateLoadedOutputs(UHoudiniAssetComponent* HAC)
{
	// Nothing to do for Node Sync Components!
	if (HAC->IsA<UHoudiniNodeSyncComponent>())
		return true;

	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::UpdateLoadedOutputs);

	HAPI_NodeId & AssetId = HAC->AssetId;

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	TArray<HAPI_Transform> ObjectTransforms;
	if (!FHoudiniEngineUtils::HapiGetObjectInfos(AssetId, ObjectInfos, ObjectTransforms))
		return false;

	TArray<HAPI_NodeId> EditableCurveObjIds;
	TArray<HAPI_NodeId> EditableCurveGeoIds;
	TArray<HAPI_NodeId> EditableCurvePartIds;
	TArray<FString> EditableCurvePartNames;

	// Iterate through all objects to get all editable curve's object geo and part Ids.

	for (int32 ObjectId = 0; ObjectId < ObjectInfos.Num(); ++ObjectId)
	{
		// Retrieve the object info
		const HAPI_ObjectInfo& CurrentHapiObjectInfo = ObjectInfos[ObjectId];

		// Cache/convert them
		FHoudiniObjectInfo CurrentObjectInfo;
		CacheObjectInfo(CurrentHapiObjectInfo, CurrentObjectInfo);

		// Start by getting the number of editable nodes
		int32 EditableNodeCount = 0;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::UpdateLoadedOutputs-ComposeChildNodeList-EditableNodes);
			HOUDINI_CHECK_ERROR(FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				CurrentHapiObjectInfo.nodeId,
				HAPI_NODETYPE_SOP, 
				HAPI_NODEFLAGS_EDITABLE | HAPI_NODEFLAGS_NON_BYPASS,
				true,
				&EditableNodeCount));
		}

		if (EditableNodeCount > 0)
		{
			TArray< HAPI_NodeId > EditableNodeIds;
			EditableNodeIds.SetNumUninitialized(EditableNodeCount);
			HOUDINI_CHECK_ERROR(FHoudiniApi::GetComposedChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				AssetId, EditableNodeIds.GetData(), EditableNodeCount));

			for (int32 nEditable = 0; nEditable < EditableNodeCount; nEditable++)
			{
				HAPI_GeoInfo CurrentEditableGeoInfo;
				FHoudiniApi::GeoInfo_Init(&CurrentEditableGeoInfo);
				HOUDINI_CHECK_ERROR(FHoudiniApi::GetGeoInfo(
					FHoudiniEngine::Get().GetSession(),
					EditableNodeIds[nEditable], &CurrentEditableGeoInfo));

				// Do not process the main display geo twice!
				if (CurrentEditableGeoInfo.isDisplayGeo)
					continue;

				// We only handle editable curves for now
				if (CurrentEditableGeoInfo.type != HAPI_GeoType::HAPI_GEOTYPE_CURVE)
					continue;

				// Check if the curve is closed (-1 unknown, could not find parameter on node). A closed curve will
				// be returned as a mesh by HAPI instead of a curve
				int32 CurveClosed = -1;
				if (!FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
						EditableNodeIds[nEditable], HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, CurveClosed))
				{
					CurveClosed = -1;
				}
				else
				{
					if (CurveClosed)
						CurveClosed = 1;
					else
						CurveClosed = 0;
				}
				
				// Cook the editable node to get its parts
				if (CurrentEditableGeoInfo.partCount <= 0)
				{
					//HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
					//FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), CurrentEditableGeoInfo.nodeId, &CookOptions);
					FHoudiniEngineUtils::HapiCookNode(CurrentEditableGeoInfo.nodeId, nullptr, true);

					HOUDINI_CHECK_ERROR(FHoudiniApi::GetGeoInfo(
						FHoudiniEngine::Get().GetSession(),
						CurrentEditableGeoInfo.nodeId,
						&CurrentEditableGeoInfo));
				}

				// Iterate on this geo's parts
				for (int32 PartId = 0; PartId < CurrentEditableGeoInfo.partCount; ++PartId)
				{
					// Get part information.
					HAPI_PartInfo CurrentHapiPartInfo;
					FHoudiniApi::PartInfo_Init(&CurrentHapiPartInfo);

					if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(
						FHoudiniEngine::Get().GetSession(), CurrentEditableGeoInfo.nodeId, PartId, &CurrentHapiPartInfo))
						continue;

					// A closed curve will be returned as a mesh in HAPI
					if (CurrentHapiPartInfo.type != HAPI_PartType::HAPI_PARTTYPE_CURVE &&
							(CurveClosed <= 0 || CurrentHapiPartInfo.type != HAPI_PartType::HAPI_PARTTYPE_MESH))
						continue;

					// Get the editable curve's part name
					FHoudiniEngineString hapiSTR(CurrentHapiPartInfo.nameSH);
					FString PartName;
					hapiSTR.ToFString(PartName);
					
					EditableCurveObjIds.Add(CurrentHapiObjectInfo.nodeId);
					EditableCurveGeoIds.Add(CurrentEditableGeoInfo.nodeId);
					EditableCurvePartIds.Add(CurrentHapiPartInfo.id);
					EditableCurvePartNames.Add(PartName);
				}
			}
		}
	}

	int32 Idx = 0;
	for (auto& CurrentOutput : HAC->Outputs)
	{
		if (CurrentOutput->IsEditableNode()) 
		{
			// The HAC is Loaded, re-assign node id to its editable curves
			if (CurrentOutput->HasEditableNodeBuilt())
			{
				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurrentOutput->GetOutputObjects();
				for (auto& Pair : OutputObjects)
				{
					if (Idx >= EditableCurvePartIds.Num())
						break;

					for(auto Component : Pair.Value.OutputComponents)
					{
					    UHoudiniSplineComponent * HoudiniSplineComponent = Cast<UHoudiniSplineComponent>(Component);
					    if (IsValid(HoudiniSplineComponent))
					    {
						    HoudiniSplineComponent->SetNodeId(EditableCurveGeoIds[Idx]);

						    Pair.Key.ObjectId = EditableCurveObjIds[Idx];
						    Pair.Key.GeoId = EditableCurveGeoIds[Idx];
						    Pair.Key.PartId = EditableCurvePartIds[Idx];
						    Pair.Key.PartName = EditableCurvePartNames[Idx];

						    Idx += 1;
					    }
					}
				}
			}
			// The HAC is a Duplication, re-construct output objects with attached duplicated editable curves, matching by part name
			else 
			{
				const TArray<USceneComponent*> &Children = HAC->GetAttachChildren();
				for (auto & CurAttachedComp : Children) 
				{
					if (!IsValid(CurAttachedComp))
						continue;

					if (!CurAttachedComp->IsA<UHoudiniSplineComponent>())
						continue;

					UHoudiniSplineComponent * CurAttachedSplineComp = Cast<UHoudiniSplineComponent>(CurAttachedComp);
					if (!CurAttachedSplineComp)
						continue;

					if (!CurAttachedSplineComp->IsEditableOutputCurve())
						continue;

					if (Idx >= EditableCurvePartIds.Num())
						break;

					// Found a match
					if (CurAttachedSplineComp->GetGeoPartName().Equals(EditableCurvePartNames[Idx])) 
					{
						FHoudiniOutputObjectIdentifier EditableSplineComponentIdentifier;
						EditableSplineComponentIdentifier.ObjectId = EditableCurveObjIds[Idx];
						EditableSplineComponentIdentifier.GeoId = EditableCurveGeoIds[Idx];
						EditableSplineComponentIdentifier.PartId = EditableCurvePartIds[Idx];
						EditableSplineComponentIdentifier.PartName = EditableCurvePartNames[Idx];

						CurAttachedSplineComp->SetNodeId(EditableSplineComponentIdentifier.GeoId);

						TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurrentOutput->GetOutputObjects();
						FHoudiniOutputObject& NewOutputObject = OutputObjects.FindOrAdd(EditableSplineComponentIdentifier);
						check(NewOutputObject.OutputComponents.Num() < 2); // Multiple components not supported yet.
						NewOutputObject.OutputComponents.Empty();
						NewOutputObject.OutputComponents.Add(CurAttachedSplineComp);
						CurrentOutput->SetHasEditableNodeBuilt(true);

						// Never add additional rot/scale attributes on editable curves as this crashes HAPI
						FHoudiniSplineTranslator::HapiUpdateNodeForHoudiniSplineComponent(
							CurAttachedSplineComp, false);
						
						Idx += 1;
						break;
					}
				}
			}
		}
		else 
		{
			// Output curve
			FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput(CurrentOutput, HAC); 
		}
		
		// Mark our outputs as loaded so they can be matched for potential reuse
		// This indicates that the HGPO's ids are invalid and that HGPO should be matched using partnames instead
		CurrentOutput->MarkAsLoaded(true);
	}

	return true;
}

//
bool 
FHoudiniOutputTranslator::UploadChangedEditableOutput(
	UHoudiniAssetComponent* HAC,
	const bool& bInForceUpdate) 
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::UploadChangedEditableOutput);

	if (!IsValid(HAC))
		return false;

	// Nothing to do for Node Sync Components!
	if (HAC->IsA<UHoudiniNodeSyncComponent>())
		return true;

	TArray<UHoudiniOutput*> &Outputs = HAC->Outputs;

	// Iterate through the outputs array of HAC.
	for (auto& CurrentOutput : HAC->Outputs)
	{
		if (!CurrentOutput)
			continue;

		// Only update the editable nodes that have been built before.
		if (!CurrentOutput->IsEditableNode() || !CurrentOutput->HasEditableNodeBuilt())
			continue;

		for (auto& CurrentOutputObj : CurrentOutput->GetOutputObjects())
		{
			for(auto Component : CurrentOutputObj.Value.OutputComponents)
			{
			    UHoudiniSplineComponent* HoudiniSplineComponent = Cast<UHoudiniSplineComponent>(Component);
			    if (!IsValid(HoudiniSplineComponent))
				    continue;

			    if (!HoudiniSplineComponent->HasChanged())
				    continue;

			    // Dont add rot/scale on editable curves as this crashes HAPI
			    if (FHoudiniSplineTranslator::HapiUpdateNodeForHoudiniSplineComponent(
					    HoudiniSplineComponent, false))
				    HoudiniSplineComponent->MarkChanged(false);
			    else
				    HoudiniSplineComponent->SetNeedsToTriggerUpdate(false);
			}
		}
	}

	return true;
}


bool
FHoudiniOutputTranslator::BuildAllOutputs(
	const HAPI_NodeId& AssetId,
	UObject* InOuterObject,	
	const TArray<HAPI_NodeId>& OutputNodes,
	const TMap<HAPI_NodeId, int32>& OutputNodeCookCounts,
	TArray<UHoudiniOutput*>& InOldOutputs,
	TArray<UHoudiniOutput*>& OutNewOutputs,
	bool InOutputTemplatedGeos,
	bool InUseOutputNodes, 
	bool bGatherEditableCurves)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::BuildAllOutputs);

	// NOTE: This function still gathers output nodes from the asset id. This is old behaviour.
	//       Output nodes are now being gathered before cooking starts and is passed in through
	//       the OutputNodes array. Clean up this function by only using output nodes from the
	//       aforementioned array.
	
	// Ensure the asset has a valid node ID
	if (AssetId < 0)
	{
		return false;
	}

	// See if we are a NodeSync component
	bool bIsNodeSyncComponent = InOuterObject ? InOuterObject->IsA<UHoudiniNodeSyncComponent>() : false;

	// Get the AssetInfo
	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);
	bool bAssetInfoSuccess = (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo));

	// Get the Asset NodeInfo
	HAPI_NodeInfo AssetNodeInfo;
	FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetNodeInfo), false);

	if (!bAssetInfoSuccess)
	{
		// IF we try to pull outputs from a non asset node, fix a few things
		AssetInfo.nameSH = AssetNodeInfo.nameSH;
		AssetInfo.nodeId = AssetNodeInfo.id;

		// TODO: improve this?
		AssetInfo.objectNodeId = (AssetNodeInfo.type == HAPI_NODETYPE_OBJ) ? AssetNodeInfo.id : AssetNodeInfo.parentId;
	}

	FString CurrentAssetName;
	{
		FHoudiniEngineString hapiSTR(AssetInfo.nameSH);
		hapiSTR.ToFString(CurrentAssetName);
	}

	// In certain cases, such as PDG output processing we might end up with a SOP node instead of a
	// container. In that case, don't try to run child queries on this node. They will fail.
	const bool bAssetHasChildren = !(AssetNodeInfo.type == HAPI_NODETYPE_SOP && AssetNodeInfo.childNodeCount == 0);

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	TArray<HAPI_Transform> ObjectTransforms;
	if (!FHoudiniEngineUtils::HapiGetObjectInfos(AssetId, ObjectInfos, ObjectTransforms))
		return false;

	// Mark all the previous HGPOs on the outputs as stale
	// This indicates that they were from a previous cook and should then be deleted
	for (auto& CurOutput : InOldOutputs)
	{
		if (CurOutput)
			CurOutput->MarkAllHGPOsAsStale(true);
	}

	// For HF / Volumes, we only create new Outputs for height volume
	// Store all the other volumes (masks etc)  on the side and we will
	// match them with theit corresponding height volume after
	TArray<FHoudiniGeoPartObject> UnassignedVolumeParts;
	
	// When receiving landscape edit layers, we are no longer to split
	// outputs based on 'height' volumes 
	TSet<int32> TileIds;
	
	// VA: Editable nodes fetching have been moved here to fetch them for the whole asset, only once.
	//     It seemed unnecessary to have to fetch these for every Object node. Instead,
	//     we'll collect all the editable nodes for the HDA and process them only on the first loop.
	//     This also allows us to use more 'strict' Object node retrieval for output processing since
	//     we don't have to worry that we might miss any editable nodes.
	
	// Start by getting the number of editable nodes
	TArray<HAPI_GeoInfo> EditableGeoInfos;
	int32 EditableNodeCount = 0;
	if (bAssetHasChildren)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::BuildAllOutputs-ComposeChildNodeList-EditableNodes);
		HOUDINI_CHECK_ERROR(FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			AssetId, HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE | HAPI_NODEFLAGS_NON_BYPASS,
			true, &EditableNodeCount));
	}
	
	// All editable nodes will be output, regardless
	// of whether the subnet is considered visible or not.
	if (EditableNodeCount > 0)
	{
		TArray<HAPI_NodeId> EditableNodeIds;
		EditableNodeIds.SetNumUninitialized(EditableNodeCount);
		HOUDINI_CHECK_ERROR(FHoudiniApi::GetComposedChildNodeList(
			FHoudiniEngine::Get().GetSession(), 
			AssetId, EditableNodeIds.GetData(), EditableNodeCount));

		for (int32 nEditable = 0; nEditable < EditableNodeCount; nEditable++)
		{
			HAPI_GeoInfo CurrentEditableGeoInfo;
			FHoudiniApi::GeoInfo_Init(&CurrentEditableGeoInfo);
			HOUDINI_CHECK_ERROR(FHoudiniApi::GetGeoInfo(
				FHoudiniEngine::Get().GetSession(), 
				EditableNodeIds[nEditable], &CurrentEditableGeoInfo));

			// TODO: Check whether this display geo is actually being output
			//       Just because this is a display node doesn't mean that it will be output (it
			//       might be in a hidden subnet)
			
			// Do not process the main display geo twice!
			if (CurrentEditableGeoInfo.isDisplayGeo)
				continue;

			// We only handle editable curves for now
			if (CurrentEditableGeoInfo.type != HAPI_GEOTYPE_CURVE || !bGatherEditableCurves)
				continue;

			// Add this geo to the geo info array
			EditableGeoInfos.Add(CurrentEditableGeoInfo);
		}
	}

	

	const bool bIsSopAsset = AssetInfo.nodeId != AssetInfo.objectNodeId;
	bool bUseOutputFromSubnets = true;
	if (bAssetHasChildren)
	{
		if (FHoudiniEngineUtils::ContainsSopNodes(AssetInfo.nodeId))
		{
			// This HDA contains immediate SOP nodes. Don't look for subnets to output.
			bUseOutputFromSubnets = false;
		}
		else
		{
			// Assume we're using a subnet-based HDA
			bUseOutputFromSubnets = true;
		}
	}
	else
	{
		// This asset doesn't have any children. Don't try to find subnets.
		bUseOutputFromSubnets = false;
	}

	// Before we can perform visibility checks on the Object nodes, we have
	// to build a set of all the Object node ids. The 'AllObjectIds' act
	// as a visibility filter. If an Object node is not present in this
	// list, the content of that node will not be displayed (display / output / templated nodes).
	// NOTE that if the HDA contains immediate SOP nodes we will ignore
	// all subnets and only use the data outputs directly from the HDA. 

	TSet<HAPI_NodeId> AllObjectIds;
	if (bUseOutputFromSubnets)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::BuildAllOutputs-ComposeChildNodeList-AllSubnets);

		int NumObjSubnets;
		TArray<HAPI_NodeId> ObjectIds;
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				AssetId,
				HAPI_NODETYPE_OBJ,
				HAPI_NODEFLAGS_OBJ_SUBNET | HAPI_NODEFLAGS_NON_BYPASS,
				true,
				&NumObjSubnets),
			false);

		ObjectIds.SetNumUninitialized(NumObjSubnets);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetComposedChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				AssetId,
				ObjectIds.GetData(),
				NumObjSubnets),
			false);
		AllObjectIds.Append(ObjectIds);
	}
	else
	{
		AllObjectIds.Add(AssetInfo.objectNodeId);
	}

	TMap<HAPI_NodeId, int32> CurrentCookCounts;
	
	// Iterate through all objects.
	int32 OutputIdx = 1;
	for (int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ObjectIdx++)
	{
		// Retrieve the object info
		const HAPI_ObjectInfo& CurrentHapiObjectInfo = ObjectInfos[ObjectIdx];

		// Determine whether this object node is fully visible.
		bool bObjectIsVisible = false;
		HAPI_NodeId GatherOutputsNodeId = -1; // Outputs will be gathered from this node.
		if (!bAssetHasChildren)
		{
			// If the asset doesn't have children, we have to gather outputs from the asset's parent in order to output
			// this asset node
			bObjectIsVisible = true;
			GatherOutputsNodeId = AssetNodeInfo.parentId;
		}
		else if (bIsSopAsset && CurrentHapiObjectInfo.nodeId == AssetInfo.objectNodeId)
		{
			// When dealing with a SOP asset, be sure to gather outputs from the SOP node, not the
			// outer object node.
			bObjectIsVisible = true;
			GatherOutputsNodeId = AssetInfo.nodeId;
		}
		else
		{
			bObjectIsVisible = FHoudiniEngineUtils::IsObjNodeFullyVisible(AllObjectIds, AssetId, CurrentHapiObjectInfo.nodeId);
			GatherOutputsNodeId = CurrentHapiObjectInfo.nodeId;
		}

		// Cache/convert them
		FHoudiniObjectInfo CurrentObjectInfo;
		CacheObjectInfo(CurrentHapiObjectInfo, CurrentObjectInfo);

		// Retrieve object name.
		FString CurrentObjectName = CurrentObjectInfo.Name;

		// Get transformation for this object.
		FTransform TransformMatrix = FTransform::Identity;
		if (ObjectTransforms.IsValidIndex(ObjectIdx))
		{
			const HAPI_Transform & ObjectTransform = ObjectTransforms[ObjectIdx];
			FHoudiniEngineUtils::TranslateHapiTransform(ObjectTransform, TransformMatrix);
		}
		else
		{
			HOUDINI_LOG_WARNING(
				TEXT("Creating Static Meshes: No HAPI transform for Object [%d %s] - using identity."),
				CurrentHapiObjectInfo.nodeId, *CurrentObjectName);
		}

		// Build an array of the geos we'll need to process
		// In most case, it will only be the display geo, 
		// but we may also want to process editable geos as well
		TArray<HAPI_GeoInfo> GeoInfos;

		// These node ids may need to be cooked in order to extract part counts.
		TSet<HAPI_NodeId> ForceNodesToCook;

		// Track (heightfield) tile ids in order to determine
		// when to create new tiles (used when outputting landscape edit layers).
		TSet<uint32> FoundTileIndices;
		
		// Append the initial set of editable geo infos here
		// then clear the editable geo infos array since we
		// only want to process them once.
		GeoInfos.Append(EditableGeoInfos);
		EditableGeoInfos.Empty();

		if (bObjectIsVisible || bIsNodeSyncComponent)
		{
			// NOTE: The HAPI_GetDisplayGeoInfo will not always return the expected Geometry subnet's
			//     Display flag geometry. If the Geometry subnet contains an Object subnet somewhere, the
			//     GetDisplayGeoInfo will sometimes fetch the display SOP from within the subnet which is
			//     not what we want.

			// Resolve and gather outputs (display / output / template nodes) from the GatherOutputsNodeId.
			FHoudiniEngineUtils::GatherImmediateOutputGeoInfos(GatherOutputsNodeId,
				InUseOutputNodes,
				InOutputTemplatedGeos,
				GeoInfos,
				ForceNodesToCook);
			
		} // if (bObjectIsVisible)

		// Iterates through the geos we want to process
		for (int32 GeoIdx = 0; GeoIdx < GeoInfos.Num(); GeoIdx++)
		{
			// Cache the geo nodes ids for this asset
			const HAPI_GeoInfo& CurrentHapiGeoInfo = GeoInfos[GeoIdx];
			// We shouldn't add display nodes for cooking since the
			// if (!CurrentHapiGeoInfo.isDisplayGeo)
			// {
			// 	OutNodeIdsToCook.Add(CurrentHapiGeoInfo.nodeId);
			// }
			
			// We cannot rely on the bGeoHasChanged flag when dealing with session sync. Since the
			// property will be set to false for any node that has cooked twice. Instead, we compare
			// current cook counts against the last cached count that we have in order to determine
			// whether geo has changed.
			bool bHasChanged = false;

			if (!CurrentCookCounts.Contains(CurrentHapiGeoInfo.nodeId))
			{
				CurrentCookCounts.Add(CurrentHapiGeoInfo.nodeId, FHoudiniEngineUtils::HapiGetCookCount(CurrentHapiGeoInfo.nodeId));
			}
			
			if (OutputNodeCookCounts.Contains(CurrentHapiGeoInfo.nodeId))
			{
				// If the cook counts changed, we assume the geo has changed.
				bHasChanged =  OutputNodeCookCounts[CurrentHapiGeoInfo.nodeId] != CurrentCookCounts[CurrentHapiGeoInfo.nodeId]; 
			}
			else
			{
				// Something is new! We don't have a cook count for this node.
				bHasChanged = true;
			}

			// Left in here for debugging convenience.
			// if (bHasChanged)
			// {
			// 	FString NodePath;
			// 	FHoudiniEngineUtils::HapiGetAbsNodePath(CurrentHapiGeoInfo.nodeId, NodePath);
			// 	HOUDINI_LOG_MESSAGE(TEXT("[TaskCookAsset] We say Geo Has Changed!: %d, %s"), CurrentHapiGeoInfo.nodeId, *NodePath);
			// }

			// HERE BE FUDGING!
			// Change the hasGeoChanged flag on the GeoInfo to match our expectation
			// of whether geo has changed.
			GeoInfos[GeoIdx].hasGeoChanged = CurrentHapiGeoInfo.hasGeoChanged || bHasChanged; 

			// Cook editable/templated nodes to get their parts.
			if ((ForceNodesToCook.Contains(CurrentHapiGeoInfo.nodeId) && CurrentHapiGeoInfo.partCount <= 0)
				|| (CurrentHapiGeoInfo.isEditable && CurrentHapiGeoInfo.partCount <= 0)
				|| (CurrentHapiGeoInfo.isTemplated && CurrentHapiGeoInfo.partCount <= 0)
				|| (!CurrentHapiGeoInfo.isDisplayGeo && CurrentHapiGeoInfo.partCount <= 0))
			{
				FHoudiniEngineUtils::HapiCookNode(CurrentHapiGeoInfo.nodeId, nullptr, true);

				HOUDINI_CHECK_ERROR(FHoudiniApi::GetGeoInfo(
					FHoudiniEngine::Get().GetSession(),
					CurrentHapiGeoInfo.nodeId,
					&GeoInfos[GeoIdx]));
			}

			// Cache/convert the display geo's info
			FHoudiniGeoInfo CurrentGeoInfo;
			CacheGeoInfo(CurrentHapiGeoInfo, CurrentGeoInfo);

			// Simply create an empty array for this geo's group names
			// We might need it later for splitting
			TArray<FString> GeoGroupNames;

			// Store all the sockets found for this geo's part
			TArray<FHoudiniMeshSocket> GeoMeshSockets;

			// Flags to track whether we think this Geo respresents either a
			// motion clip or a skeletal mesh
			bool bIsMotionClip = false;

			// ------------------
			// Parts PrePass
			// ------------------
			
			// Do a pre-pass to determine whether we're dealing a motion clip of a skeletal mesh
			// MotionClip: requires motion clip attrs on the packed primitive and requires bones to be present inside that same packed prim.
			// Skeletal Mesh: requires a packed prim containing a mesh with capture weights. Requires a packed prim with bones.
			bool bHasMotionClipTopologyFrame = false;
			bool bHasMotionClipAnimFrame = false;

			//---------------------------------------------------------------------------------------------------------------------------------------
			// Fetch all part infos. In addition, cook templated geos if needed
			//---------------------------------------------------------------------------------------------------------------------------------------

			TArray<HAPI_PartInfo> HapiPartInfos;
			TArray<FHoudiniPartInfo> PartInfos;
			HapiPartInfos.SetNum(CurrentGeoInfo.PartCount);
			PartInfos.SetNum(HapiPartInfos.Num());

			for (int32 PartId = 0; PartId < CurrentGeoInfo.PartCount; ++PartId)
			{
				// Get part information.
				HAPI_PartInfo& CurrentHapiPartInfo = HapiPartInfos[PartId];
				FHoudiniApi::PartInfo_Init(&CurrentHapiPartInfo);

				// If the geo is templated, cook it manually
				if (CurrentHapiGeoInfo.isTemplated && InOutputTemplatedGeos)
					FHoudiniEngineUtils::HapiCookNode(CurrentHapiGeoInfo.nodeId, nullptr, true);

				bool bPartInfoFailed = false;
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), CurrentHapiGeoInfo.nodeId, PartId, &CurrentHapiPartInfo))
				{
					bPartInfoFailed = true;

					// If the geo is templated, attempt to cook it manually.
					if (CurrentHapiGeoInfo.isTemplated && InOutputTemplatedGeos)
					{
						FHoudiniEngineUtils::HapiCookNode(CurrentHapiGeoInfo.nodeId, nullptr, true);

						HOUDINI_CHECK_ERROR(FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), CurrentHapiGeoInfo.nodeId, &GeoInfos[GeoIdx]));

						if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), CurrentHapiGeoInfo.nodeId, PartId, &CurrentHapiPartInfo))
						{
							bPartInfoFailed = false;
						}
					}
				}

				if (bPartInfoFailed)
				{
					// Error retrieving part info.
					HOUDINI_LOG_MESSAGE(TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d] unable to retrieve PartInfo - skipping."),
						CurrentHapiObjectInfo.nodeId, *CurrentObjectName, CurrentHapiGeoInfo.nodeId, PartId);
					CurrentHapiPartInfo.type = HAPI_PartType::HAPI_PARTTYPE_INVALID;
				}

				// Convert/cache the part info
				FHoudiniPartInfo& CurrentPartInfo = PartInfos[PartId];
				CachePartInfo(CurrentHapiPartInfo, CurrentPartInfo);
			}

			//---------------------------------------------------------------------------------------------------------------------------------------
			//// Motion Clip checks. Note this currentrly assumes one part/motion clip per geo.
			//---------------------------------------------------------------------------------------------------------------------------------------

			for (int32 PartId = 0; PartId < CurrentGeoInfo.PartCount; ++PartId)
			{
				HAPI_PartInfo& CurrentHapiPartInfo = HapiPartInfos[PartId];
				FHoudiniPartInfo& CurrentPartInfo = PartInfos[PartId];

				// Check to for motion clip topology frame
				if ((CurrentHapiPartInfo.type == HAPI_PARTTYPE_INSTANCER) && PartId == 0)
				{
					bHasMotionClipTopologyFrame = FHoudiniAnimationTranslator::IsMotionClipFrame(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, false);
				}

				// Check for the first motion clip anim frame 
				if ((CurrentHapiPartInfo.type == HAPI_PARTTYPE_INSTANCER) && PartId == 2)
				{
					bHasMotionClipAnimFrame = FHoudiniAnimationTranslator::IsMotionClipFrame(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, true);
				}

				if (bHasMotionClipTopologyFrame && bHasMotionClipAnimFrame)
				{
					// We have identified that this Geo data is likely a motion clip.
					bIsMotionClip = true;
					break;
				}
			}


			//---------------------------------------------------------------------------------------------------------------------------------------
			// Skeletal Mesh checks. Try to collect all the parts needed for a skeletal mesh.
			//---------------------------------------------------------------------------------------------------------------------------------------

			struct FHoudiniSkeletalMeshParts
			{
				int32 ShapeInstancerPartId = INDEX_NONE;
				int32 ShapeMeshPartId = INDEX_NONE;
				int32 PoseInstancerPartId = INDEX_NONE;
				int32 PoseMeshPartId = INDEX_NONE;
				int32 PhysAssetInstancerPartId = INDEX_NONE;
				int32 PhysAssetMeshPartId = INDEX_NONE;
			};

			// Track shapes / skeleton parts so that we can pair them when building skeletal mesh outputs.
			TMap<FString, FHoudiniSkeletalMeshParts> SkelMeshParts;

			// Names of valid / complete skeletal meshes (both Rest Geometry and Packed Primitives are present). 
			TSet<FString> ValidSkelMeshNames;
			// Track the base name for the given PartId
			TMap<HAPI_PartId, FString> PartIdBaseNameMap;
			// Track the output associate with each skeletal mesh base name
			TMap<FString, UHoudiniOutput*> SkeletalMeshOutputs;


			TArray<EHoudiniPartType> SkeletalMeshPartIds;
			SkeletalMeshPartIds.Init(EHoudiniPartType::Invalid, CurrentGeoInfo.PartCount);

			for (int32 PartId = 0; PartId < CurrentGeoInfo.PartCount; ++PartId)
			{
				HAPI_PartInfo& CurrentHapiPartInfo = HapiPartInfos[PartId];

				if (CurrentHapiPartInfo.type == HAPI_PARTTYPE_INSTANCER)
				{

					// Check for skeletal mesh Rest Geometry (Shape, in Houdini terms))
					FString BaseName;

					int32 ShapeMeshPartId = INDEX_NONE;
					int32 PhysAssetId = INDEX_NONE;
					int32 PoseCurveId = INDEX_NONE;

					if (FHoudiniSkeletalMeshTranslator::IsRestShapeInstancer(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, BaseName, ShapeMeshPartId))
					{
						PartIdBaseNameMap.Add(PartId, BaseName);
						FHoudiniSkeletalMeshParts& SkelParts = SkelMeshParts.FindOrAdd(BaseName);
						if (SkelParts.ShapeInstancerPartId == INDEX_NONE)
						{
							// Set the shape parts
							SkelParts.ShapeInstancerPartId = PartId;
							SkelParts.ShapeMeshPartId = ShapeMeshPartId;
							PartIdBaseNameMap.Add(PartId, BaseName);
							PartIdBaseNameMap.Add(ShapeMeshPartId, BaseName);
							SkeletalMeshPartIds[PartId] = EHoudiniPartType::SkeletalMeshShape;
							SkeletalMeshPartIds[ShapeMeshPartId] = EHoudiniPartType::SkeletalMeshShape;
							ValidSkelMeshNames.Add(BaseName);

						}
					}
					else if (FHoudiniSkeletalMeshTranslator::IsCapturePoseInstancer(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, BaseName, PoseCurveId))
					{
						PartIdBaseNameMap.Add(PartId, BaseName);
						FHoudiniSkeletalMeshParts& SkelParts = SkelMeshParts.FindOrAdd(BaseName);
						if (SkelParts.PoseInstancerPartId == INDEX_NONE)
						{
							// Set the Pose parts
							SkelParts.PoseInstancerPartId = PartId;
							SkelParts.PoseMeshPartId = PoseCurveId;
							PartIdBaseNameMap.Add(PartId, BaseName);
							PartIdBaseNameMap.Add(PoseCurveId, BaseName);
							SkeletalMeshPartIds[PartId] = EHoudiniPartType::SkeletalMeshPose;
							SkeletalMeshPartIds[PoseCurveId] = EHoudiniPartType::SkeletalMeshPose;
							ValidSkelMeshNames.Add(BaseName);
						}
					}
					else if (FHoudiniSkeletalMeshTranslator::IsPhysAssetInstancer(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, BaseName, PhysAssetId))
					{
						PartIdBaseNameMap.Add(PartId, BaseName);
						FHoudiniSkeletalMeshParts& SkelParts = SkelMeshParts.FindOrAdd(BaseName);
						if (SkelParts.PhysAssetInstancerPartId == INDEX_NONE)
						{
							// Set the Phys Assets parts
							SkelParts.PhysAssetInstancerPartId = PartId;
							SkelParts.PhysAssetMeshPartId = PhysAssetId;
							PartIdBaseNameMap.Add(PartId, BaseName);
							PartIdBaseNameMap.Add(PhysAssetId, BaseName);
							SkeletalMeshPartIds[PartId] = EHoudiniPartType::SkeletalMeshPhysAsset;
							SkeletalMeshPartIds[PhysAssetId] = EHoudiniPartType::SkeletalMeshPhysAsset;
							ValidSkelMeshNames.Add(BaseName);
						}
					}
				}
			} 


			//---------------------------------------------------------------------------------------------------------------------------------------
			// Parts Processing
			//---------------------------------------------------------------------------------------------------------------------------------------

			// Iterate on this geo's parts
			for (int32 PartId = 0; PartId < CurrentGeoInfo.PartCount; ++PartId)
			{
				// Get part information.
				HAPI_PartInfo CurrentHapiPartInfo = HapiPartInfos[PartId];
				FHoudiniPartInfo CurrentPartInfo = PartInfos[PartId];

				// Unsupported/Invalid part
				if (CurrentPartInfo.Type == EHoudiniPartType::Invalid)
					continue;


				// Retrieve part name.
				FString CurrentPartName = CurrentPartInfo.Name;


				// Update part/instancer type from the part infos
				EHoudiniPartType CurrentPartType = EHoudiniPartType::Invalid;
				EHoudiniInstancerType CurrentInstancerType = EHoudiniInstancerType::Invalid;

				bool bInstancerTypeFound = bIsMotionClip || (SkeletalMeshPartIds[PartId] != EHoudiniPartType::Invalid);
				bool bIsGeometryCollection = false;

				if (CurrentHapiPartInfo.type == HAPI_PARTTYPE_INSTANCER && !bInstancerTypeFound)
				{
					bIsGeometryCollection = FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancerPart(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id);
					bInstancerTypeFound = true;
				}

				switch (CurrentHapiPartInfo.type)
				{
					case HAPI_PARTTYPE_BOX:
					case HAPI_PARTTYPE_SPHERE:
					case HAPI_PARTTYPE_MESH:
					{
						if (CurrentHapiGeoInfo.type == HAPI_GEOTYPE_CURVE)
						{
							// Closed curve will be seen as mesh
							if (FHoudiniEngineUtils::IsLandscapeSpline(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id))
							{
								// This could be a landscape spline...
								CurrentPartType = EHoudiniPartType::LandscapeSpline;
							}
							else
							{
								// This is actually a curve
								CurrentPartType = EHoudiniPartType::Curve;
							}
						}
						else
						{
							CurrentPartType = SkeletalMeshPartIds[PartId];

							if (CurrentPartType == EHoudiniPartType::Invalid)
							{
								CurrentPartType = EHoudiniPartType::Mesh;

								if (bIsMotionClip)
								{
									// We don't care about tracking Mesh objects for motion clips.
									// We just want to track the packed primitives, and extract the mesh data in the translator.
									continue;
								}
								else if (CurrentHapiObjectInfo.isInstancer)
								{
									if (FHoudiniEngineUtils::IsAttributeInstancer(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, CurrentInstancerType))
									{
										// That part is actually an attribute instancer
										CurrentPartType = EHoudiniPartType::Instancer;
										// Instancer type is set by IsAttributeInstancer
									}
									else
									{
										// That part is actually an instancer
										CurrentPartType = EHoudiniPartType::Instancer;
										CurrentInstancerType = EHoudiniInstancerType::ObjectInstancer;
									}
									
								}
								else if (CurrentHapiPartInfo.vertexCount <= 0 && CurrentHapiPartInfo.pointCount <= 0)
								{
									// No points, no vertices, we're likely invalid
									CurrentPartType = EHoudiniPartType::Invalid;
									HOUDINI_LOG_MESSAGE(
										TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] is a mesh with no points or vertices - skipping."),
										CurrentHapiObjectInfo.nodeId, *CurrentObjectName, CurrentHapiGeoInfo.nodeId, PartId, *CurrentPartName);
								}
								else if (CurrentHapiPartInfo.vertexCount <= 0)
								{
									// This is not an instancer, we do not have vertices, but we have points
									// Maybe this is a point cloud with attribute override instancing
									if(FHoudiniEngineUtils::IsAttributeInstancer(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, CurrentInstancerType))
									{
										// Mark it as an instancer
										CurrentPartType = EHoudiniPartType::Instancer;
										// Instancer type is set by IsAttributeInstancer
										//CurrentInstancerType = EHoudiniInstancerType::OldSchoolAttributeInstancer;
									}
									else
									{
										// No vertices, not an instancer, just a point cloud
										if (FHoudiniEngineUtils::IsValidDataTable(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id))
										{
											CurrentPartType = EHoudiniPartType::DataTable;
										}
										else
										{
											CurrentPartType = EHoudiniPartType::Invalid;
										}
									}
								}
							}
						}
					}
					break;

					case HAPI_PARTTYPE_CURVE:
					{
						if (SkeletalMeshPartIds[PartId] == EHoudiniPartType::SkeletalMeshPose)
						{
							// The Capture Pose mesh is reported by HAPI to be a Curve part type, so be sure to intercept
							// it here
							CurrentPartType = EHoudiniPartType::SkeletalMeshPose;
						}
						// Make sure that this curve is not an an attribute instancer!
						else if (FHoudiniEngineUtils::IsAttributeInstancer(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, CurrentInstancerType))
						{
							// Mark the part as an instancer it as an instancer
							CurrentPartType = EHoudiniPartType::Instancer;
						}
						else if (FHoudiniEngineUtils::IsValidDataTable(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id))
						{
							// the curve is actually a data table!
							CurrentPartType = EHoudiniPartType::DataTable;
						}
						else if (FHoudiniEngineUtils::IsLandscapeSpline(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id))
						{
							// the curve is actually a landscape spline!
							CurrentPartType = EHoudiniPartType::LandscapeSpline;
						}
						else
						{
							// The curve is a curve!
							CurrentPartType = EHoudiniPartType::Curve;
						}
					}
					break;

					case HAPI_PARTTYPE_INSTANCER:
					{
						// This is a packed primitive instancer
						CurrentPartType = EHoudiniPartType::Instancer;
						if (bIsMotionClip)
						{
							CurrentPartType = EHoudiniPartType::MotionClip;
							CurrentInstancerType = EHoudiniInstancerType::MotionClip;
						}
						else if (SkeletalMeshPartIds[PartId] != EHoudiniPartType::Invalid)
						{
							CurrentPartType = SkeletalMeshPartIds[PartId];
							CurrentInstancerType = EHoudiniInstancerType::SkeletalMesh;
						}
						else if (bIsGeometryCollection)
						{
							CurrentPartType = EHoudiniPartType::Instancer;
							CurrentInstancerType = EHoudiniInstancerType::GeometryCollection;
						}
						else
						{
							CurrentPartType = EHoudiniPartType::Instancer;
							CurrentInstancerType = EHoudiniInstancerType::PackedPrimitive;
						}
					}
					break;

					case HAPI_PARTTYPE_VOLUME:
						// Volume data, likely a Heightfield height / mask	
						CurrentPartType = EHoudiniPartType::Volume;
						break;

					default:
						// Unsupported Part Type
						break;
				}

				// There are no vertices AND no points and this part is not a packed prim instancer
				if ((CurrentPartInfo.VertexCount <= 0 && CurrentPartInfo.PointCount <= 0) &&
					(CurrentPartType != EHoudiniPartType::Instancer || (CurrentInstancerType != EHoudiniInstancerType::PackedPrimitive && CurrentInstancerType != EHoudiniInstancerType::GeometryCollection)) &&
					(CurrentPartType != EHoudiniPartType::MotionClip) &&
					(CurrentPartType != EHoudiniPartType::SkeletalMeshPose) &&
					(CurrentPartType != EHoudiniPartType::SkeletalMeshShape) &&
					(CurrentPartType != EHoudiniPartType::SkeletalMeshPhysAsset)
					)
				{
					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] no points or vertices found - skipping."),
						CurrentHapiObjectInfo.nodeId, *CurrentObjectName, CurrentHapiGeoInfo.nodeId, PartId, *CurrentPartName);
					continue;
				}

				// This is an instancer with no points.
				if (CurrentHapiObjectInfo.isInstancer && CurrentHapiPartInfo.pointCount <= 0)
				{
					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] is instancer but has 0 points - skipping."),
						CurrentHapiObjectInfo.nodeId, *CurrentObjectName, CurrentHapiGeoInfo.nodeId, PartId, *CurrentPartName);
					continue;
				}
				
				// Extract Mesh sockets
				// Do this before ignoring invalid parts, as socket groups/attributes could be set on parts
				// that don't have any mesh, just points! Those would be be considered "invalid" parts but
				// could still have valid sockets!
				TArray<FHoudiniMeshSocket> PartMeshSockets;
				FHoudiniEngineUtils::AddMeshSocketsToArray_DetailAttribute(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, PartMeshSockets, CurrentHapiPartInfo.isInstanced);
				FHoudiniEngineUtils::AddMeshSocketsToArray_Group(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, PartMeshSockets, CurrentHapiPartInfo.isInstanced);

				// Ignore invalid parts
				if (CurrentPartType == EHoudiniPartType::Invalid)
				{
					if(PartMeshSockets.Num() > 0)
					{
						// Store these Part sockets for the Geo
						// We'll copy them to the outputs produced by this Geo later
						GeoMeshSockets.Append(PartMeshSockets);
					}

					continue;
				}

				// Build the HGPO corresponding to this part
				FHoudiniGeoPartObject currentHGPO;
				currentHGPO.AssetId = AssetId;
				currentHGPO.AssetName = CurrentAssetName;

				currentHGPO.ObjectId = CurrentHapiObjectInfo.nodeId;
				currentHGPO.ObjectName = CurrentObjectName;

				currentHGPO.GeoId = CurrentHapiGeoInfo.nodeId;

				currentHGPO.PartId = CurrentHapiPartInfo.id;

				currentHGPO.Type = CurrentPartType;
				currentHGPO.InstancerType = CurrentInstancerType;

				currentHGPO.TransformMatrix = TransformMatrix;

				currentHGPO.NodePath = TEXT("");

				currentHGPO.bIsVisible = CurrentHapiObjectInfo.isVisible && !CurrentHapiPartInfo.isInstanced;
				if (bIsNodeSyncComponent)
					currentHGPO.bIsVisible = true;

				currentHGPO.bIsEditable = CurrentHapiGeoInfo.isEditable;
				currentHGPO.bIsInstanced = CurrentHapiPartInfo.isInstanced;
				// Never consider a display geo as templated!
				currentHGPO.bIsTemplated = CurrentHapiGeoInfo.isDisplayGeo ? false : CurrentHapiGeoInfo.isTemplated;

				currentHGPO.bHasGeoChanged = CurrentHapiGeoInfo.hasGeoChanged;
				currentHGPO.bHasPartChanged = CurrentHapiPartInfo.hasChanged;
				currentHGPO.bHasMaterialsChanged = CurrentHapiGeoInfo.hasMaterialChanged;
				currentHGPO.bHasTransformChanged = CurrentHapiObjectInfo.hasTransformChanged;
				
				// Copy the HAPI info caches 
				currentHGPO.ObjectInfo = CurrentObjectInfo;
				currentHGPO.GeoInfo = CurrentGeoInfo;
				currentHGPO.PartInfo = CurrentPartInfo;

				currentHGPO.AllMeshSockets = PartMeshSockets;
				
				// If the mesh is NOT visible and is NOT instanced, skip it.
				if (!currentHGPO.bIsVisible && !currentHGPO.bIsInstanced)
				{
					continue;
				}

				// We only support meshes for templated geos
				if (currentHGPO.bIsTemplated && (CurrentPartType != EHoudiniPartType::Mesh))
					continue;

				// Update the HGPO's node path
				FHoudiniEngineUtils::HapiGetNodePath(currentHGPO, currentHGPO.NodePath);

				// Try to get the custom part name from attribute
				FString CustomPartName;
				if (FHoudiniOutputTranslator::GetCustomPartNameFromAttribute(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, CustomPartName))
					currentHGPO.SetCustomPartName(CustomPartName);
				else
					currentHGPO.PartName = CurrentPartName;

				FHoudiniEngineUtils::GetGenericPropertiesAttributes(
					currentHGPO.GeoId, currentHGPO.PartId,
					true, 0, 0, 0,
					currentHGPO.GenericPropertyAttributes);

				{
					currentHGPO.bKeepTags = false;

					TArray<int> KeepTagData;
					FHoudiniHapiAccessor Accessor(currentHGPO.GeoId, currentHGPO.PartId, HAPI_UNREAL_ATTRIB_TAG_KEEP);
					bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, KeepTagData);

					if (bSuccess)
					{
						if (KeepTagData.Num() > 0)
						{
							currentHGPO.bKeepTags = KeepTagData[0] == 0 ? false : true; 
						}
					}
				}

				//
				// Mesh Only - Extract split groups
				// 
				// Extract the group names used by this part to see if it will require splitting
				// Only meshes can be split, via their primitive groups
				TArray<FString> SplitGroupNames;
				if (CurrentPartType == EHoudiniPartType::Mesh)
				{
					if (!CurrentHapiPartInfo.isInstanced && GeoGroupNames.Num() > 0)
					{
						// We are not instanced and already have extracted the geo's group names
						// We can simply reuse the Geo group names / socket groups
						currentHGPO.SplitGroups = GeoGroupNames;
					}
					else
					{
						// We need to get the primitive group names from HAPI
						TArray<FString> GroupNames;
						if (!FHoudiniEngineUtils::HapiGetGroupNames(
							CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id,
							HAPI_GROUPTYPE_PRIM, CurrentHapiPartInfo.isInstanced,
							GroupNames))
						{
							GroupNames.Empty();
						}

						// Convert the string handles to FStrings
						for (const FString& GroupName : GroupNames)
						{
							FString LodGroup = HAPI_UNREAL_GROUP_LOD_PREFIX;
							FString CollisionGroup = HAPI_UNREAL_GROUP_INVISIBLE_COLLISION_PREFIX;
							FString RenderedCollisionGroup = HAPI_UNREAL_GROUP_RENDERED_COLLISION_PREFIX;
							if (GroupName.StartsWith(LodGroup, ESearchCase::IgnoreCase)
								|| GroupName.StartsWith(CollisionGroup, ESearchCase::IgnoreCase)
								|| GroupName.StartsWith(RenderedCollisionGroup, ESearchCase::IgnoreCase))
								//|| GroupName.StartsWith(HAPI_UNREAL_GROUP_USER_SPLIT_PREFIX, ESearchCase::IgnoreCase))
							{
								// Split by collisions / lods
								currentHGPO.SplitGroups.Add(GroupName);
							}
						}

						// Sort the Group name array by name, 
						// this will order the LODs and other incremental group names
						currentHGPO.SplitGroups.Sort();

						// If this part is not instanced, we can copy the geo
						// group names so we can reuse them for another part
						if (!CurrentHapiPartInfo.isInstanced)
						{
							GeoGroupNames = currentHGPO.SplitGroups;
						}
					}
				}

				//
				// Volume Only - Extract volume name/tile index
				// 
				// Extract the volume's name, and see if a tile attribute is present
				FHoudiniVolumeInfo CurrentVolumeInfo;
				if (CurrentPartType == EHoudiniPartType::Volume)
				{
					// Get this volume's info
					HAPI_VolumeInfo CurrentHapiVolumeInfo;
					FHoudiniApi::VolumeInfo_Init(&CurrentHapiVolumeInfo);

					bool bVolumeValid = true;

					HAPI_Result Result = FHoudiniApi::GetVolumeInfo(
						FHoudiniEngine::Get().GetSession(),
						CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id,
						&CurrentHapiVolumeInfo);

					if (HAPI_RESULT_SUCCESS != Result)
					{
						HOUDINI_LOG_ERROR(TEXT("Failed to get VolumeInfo (%d)"), *FHoudiniEngineUtils::GetErrorDescription(Result));
						bVolumeValid = false;
					}
					else if (CurrentHapiVolumeInfo.tupleSize != 1)
					{
						HOUDINI_LOG_ERROR(TEXT("Invalid tuple size (%d)"), CurrentHapiVolumeInfo.tupleSize);
						bVolumeValid = false;
					}
					else if (CurrentHapiVolumeInfo.zLength != 1)
					{
						HOUDINI_LOG_ERROR(TEXT("Invalid zlength (%d)"), CurrentHapiVolumeInfo.zLength);
						bVolumeValid = false;
					}
					else if (CurrentHapiVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT)
					{
						HOUDINI_LOG_ERROR(TEXT("Invalid storage (%d)"), CurrentHapiVolumeInfo.storage);
						bVolumeValid = false;
					}

					// Only cache valid volumes
					if (bVolumeValid)
					{
						// Convert/Cache the volume info
						CacheVolumeInfo(CurrentHapiVolumeInfo, CurrentVolumeInfo);

						// Get the volume's name
						currentHGPO.VolumeName = CurrentVolumeInfo.Name;

						// Now see if this volume has a tile attribute
						TArray<int32> TileValues;
						if (FHoudiniEngineUtils::GetTileAttribute(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, TileValues, HAPI_ATTROWNER_PRIM, 0, 1))
						{
							if (TileValues.Num() > 0 && TileValues[0] >= 0)
								currentHGPO.VolumeTileIndex = TileValues[0];
							else
								currentHGPO.VolumeTileIndex = -1;
						}

						currentHGPO.bHasEditLayers = FHoudiniEngineUtils::GetEditLayerName(CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id, currentHGPO.VolumeLayerName, HAPI_ATTROWNER_PRIM);
					}
				}
				currentHGPO.VolumeInfo = CurrentVolumeInfo;

				// Cache the curve info as well, and also for landscape splines
				// !!! Only call GetCurveInfo if the PartType is Curve
				// !!! Closed curves are actually Meshes, and calling GetCurveInfo on a Mesh will crash HAPI!
				FHoudiniCurveInfo CurrentCurveInfo;
				if ((CurrentPartType == EHoudiniPartType::Curve || CurrentPartType == EHoudiniPartType::LandscapeSpline) && CurrentPartInfo.Type == EHoudiniPartType::Curve)
				{
					HAPI_CurveInfo CurrentHapiCurveInfo;
					FHoudiniApi::CurveInfo_Init(&CurrentHapiCurveInfo);
					if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetCurveInfo(
						FHoudiniEngine::Get().GetSession(),
						CurrentHapiGeoInfo.nodeId, CurrentHapiPartInfo.id,
						&CurrentHapiCurveInfo))
					{
						// Cache/Convert this part's curve info
						CacheCurveInfo(CurrentHapiCurveInfo, CurrentCurveInfo);
					}
				}
				currentHGPO.CurveInfo = CurrentCurveInfo;

				if (CurrentPartType == EHoudiniPartType::SkeletalMeshPose || 
					CurrentPartType == EHoudiniPartType::SkeletalMeshShape || 
					CurrentPartType == EHoudiniPartType::SkeletalMeshPhysAsset)
				{
					FString SkeletalMeshBaseName = PartIdBaseNameMap.FindChecked(PartId);
					currentHGPO.InstancerName = SkeletalMeshBaseName;
				}


				// TODO:
				// DONE? bake folders are handled out of this loop?
				// See if a custom bake folder override for the mesh was assigned via the "unreal_bake_folder" attribute
				//TArray<FString> BakeFolderOverrides;

				// See if we have an existing output that matches this HGPO or if we need to create a new one
				// We handle volumes, motion clips and skeletal meshes differently than other outputs types.
				// These are treated as a single output that has multiple HGPOs
				bool IsFoundOutputValid = false;
				UHoudiniOutput ** FoundHoudiniOutput = nullptr;	
				if (currentHGPO.Type != EHoudiniPartType::Volume &&
					currentHGPO.Type != EHoudiniPartType::MotionClip &&
					currentHGPO.Type != EHoudiniPartType::SkeletalMeshPose &&
					currentHGPO.Type != EHoudiniPartType::SkeletalMeshShape &&
					currentHGPO.Type != EHoudiniPartType::SkeletalMeshPhysAsset
					)
				{
					// Create single output per HGPO. never reuse old outputs, as its deprecated anyway
					FoundHoudiniOutput = nullptr;
				}
				else if (currentHGPO.Type == EHoudiniPartType::SkeletalMeshPose || 
						currentHGPO.Type == EHoudiniPartType::SkeletalMeshShape || 
					currentHGPO.Type == EHoudiniPartType::SkeletalMeshPhysAsset)
				{
					// Each "complete" skeletal mesh (2 packed prims) will result in a single output.
					// Collect HGPOs that match the skeletal mesh BaseName into a single output.
					TFunction<bool(UHoudiniOutput*)> MatchFn = [currentHGPO](UHoudiniOutput* Output) -> bool { return Output ? Output->InstancerNameMatch(currentHGPO) : false; };

					// Look in the previous outputs if we have a match
					FoundHoudiniOutput = InOldOutputs.FindByPredicate(MatchFn);

					if (FoundHoudiniOutput && IsValid(*FoundHoudiniOutput))
						IsFoundOutputValid = true;

					// If we dont have a match in the old maps, also look in the newly created outputs
					if (!IsFoundOutputValid)
					{
						FoundHoudiniOutput = OutNewOutputs.FindByPredicate(MatchFn);

						if (FoundHoudiniOutput && IsValid(*FoundHoudiniOutput))
							IsFoundOutputValid = true;
					}
				}
				else
				{
					// Collect HGPOs into a single output.
					TFunction<bool(UHoudiniOutput*)> MatchFn = [currentHGPO](UHoudiniOutput* Output) -> bool { return Output ? Output->GeoMatch(currentHGPO) : false; };

					if (currentHGPO.Type == EHoudiniPartType::Volume)
					{
						// Heightfields use a special match function
						MatchFn = [currentHGPO](UHoudiniOutput* Output) { return Output ? Output->HeightfieldMatch(currentHGPO, true) : false; };
					}
					
					// Look in the previous outputs if we have a match
					FoundHoudiniOutput = InOldOutputs.FindByPredicate(MatchFn);
					
					if (FoundHoudiniOutput && IsValid(*FoundHoudiniOutput))
						IsFoundOutputValid = true;

					// If we dont have a match in the old maps, also look in the newly created outputs
					if (!IsFoundOutputValid)
					{
						if (currentHGPO.Type == EHoudiniPartType::Volume)
						{
							// Heightfields use a special match function
							MatchFn = [currentHGPO](UHoudiniOutput* Output) { return Output ? Output->HeightfieldMatch(currentHGPO, false) : false; };
						}
						
						FoundHoudiniOutput = OutNewOutputs.FindByPredicate(MatchFn);

						if (FoundHoudiniOutput && IsValid(*FoundHoudiniOutput))
							IsFoundOutputValid = true;
					}
				}

				UHoudiniOutput * HoudiniOutput = nullptr;
				if (IsFoundOutputValid)
				{
					// We can reuse the existing output
					HoudiniOutput = *FoundHoudiniOutput;
					HoudiniOutput->SetIsUpdating(true);
					// Transfer this output from the old array to the new one
					InOldOutputs.Remove(HoudiniOutput);
				}
				else
				{
					// We couldn't find a valid output object, so create a new one
					if (currentHGPO.Type == EHoudiniPartType::Volume)
					{
						bool bBatchHGPO = false;
						if(!currentHGPO.VolumeName.Equals(HAPI_UNREAL_LANDSCAPE_HEIGHT_VOLUME_NAME, ESearchCase::IgnoreCase))
						{
							// This volume is not a height volume, so it will be batched into a single HGPO.
							bBatchHGPO = true;
						}
						else if (currentHGPO.bHasEditLayers)
						{
							if (FoundTileIndices.Contains(currentHGPO.VolumeTileIndex))
							{
								// If this volume name is height, AND we have edit layers enabled, check to see whether
								// this is a new tile. If this is NOT a new tile, we assume that this is simply content
								// for a new edit layer on the current tile. Batch it!
								bBatchHGPO = true;
							}
						}
						// Ensure this tile is tracked
						FoundTileIndices.Add(currentHGPO.VolumeTileIndex);

					}

					// Create a new output object
					//FString OutputName = TEXT("Output") + FString::FromInt(OutputIdx++);
					HoudiniOutput = NewObject<UHoudiniOutput>(
						InOuterObject,
						UHoudiniOutput::StaticClass(),
						NAME_None,//FName(*OutputName),
						RF_NoFlags);

					// Make sure the created object is valid 
					if (!IsValid(HoudiniOutput))
					{
						//HOUDINI_LOG_WARNING("Failed to create asset output");
						continue;
					}

					// Mark if the HoudiniOutput is editable
				}
				// Ensure that we always update the 'Editable' state of the output since this
				// may very well change between cooks (for example, the User is editina the HDA is session sync).
				HoudiniOutput->SetIsEditableNode(currentHGPO.bIsEditable && bGatherEditableCurves);

				// Add the HGPO to the output
				HoudiniOutput->AddNewHGPO(currentHGPO);
				// Add this output object to the new ouput array
				OutNewOutputs.AddUnique(HoudiniOutput);
			} 
			// END: for Part

			if (GeoMeshSockets.Num() > 0)
			{
				// If we have any mesh socket, assign them to the HGPO for this geo
				for (auto& CurNewOutput : OutNewOutputs)
				{
					if (!IsValid(CurNewOutput))
						continue;

					int32 FirstValidIdx = CurNewOutput->StaleCount;
					if (!CurNewOutput->HoudiniGeoPartObjects.IsValidIndex(FirstValidIdx))
						FirstValidIdx = 0;

					for (int32 Idx = FirstValidIdx; Idx < CurNewOutput->HoudiniGeoPartObjects.Num(); Idx++)
					{
						// Only add sockets to valid/non stale HGPOs
						FHoudiniGeoPartObject& CurHGPO = CurNewOutput->HoudiniGeoPartObjects[Idx];
						if (CurHGPO.ObjectId != CurrentHapiObjectInfo.nodeId)
							continue;

						if (CurHGPO.GeoId != CurrentHapiGeoInfo.nodeId)
							continue;

						CurHGPO.AllMeshSockets.Append(GeoMeshSockets);
					}
				}
			}
		}
		// END: for GEO
	}
	// END: for OBJ

	// Update the output/HGPO associations from the map
	// Clear the old HGPO since we don't need them anymore
	for (auto& CurrentOuput : OutNewOutputs)
	{
		if (!IsValid(CurrentOuput))
			continue;

		CurrentOuput->DeleteAllStaleHGPOs();
	}

	// If we have unassigned volumes,
	// try to find their corresponding output
	if (UnassignedVolumeParts.Num() > 0)
	{
		for (auto& currentVolumeHGPO : UnassignedVolumeParts)
		{
			UHoudiniOutput ** FoundHoudiniOutput = OutNewOutputs.FindByPredicate(
				[currentVolumeHGPO](UHoudiniOutput* Output) 
				{
					return Output ? Output->HeightfieldMatch(currentVolumeHGPO, false) : false;
				});

			if (!FoundHoudiniOutput || !IsValid(*FoundHoudiniOutput))
			{
				// Skip - consider this volume as invalid
				continue;
			}

			// Add this HGPO to the output
			(*FoundHoudiniOutput)->AddNewHGPO(currentVolumeHGPO);
		} 
	}

	// Reuse geometry collection outputs.
	TSet<FString> GCNames;
	FHoudiniGeometryCollectionTranslator::GetGeometryCollectionNames(OutNewOutputs, GCNames);
	for (int i = InOldOutputs.Num() - 1; i >= 0; i--)
	{
		UHoudiniOutput * OldOutput = InOldOutputs[i];
		if (OldOutput->Type == EHoudiniOutputType::GeometryCollection)
		{
			bool ReuseOutput = false;
			// If a suitable output is not found in the new outputs, do not reuse the output.
			for (auto & Pair : OldOutput->GetOutputObjects())
			{
				if (IsValid(Pair.Value.OutputObject))
				{
					if (!GCNames.Contains(Pair.Value.OutputObject->GetName()))
					{
						ReuseOutput = false;
						AGeometryCollectionActor * Actor = Cast<AGeometryCollectionActor>(Pair.Value.OutputObject);
						if (IsValid(Actor))
						{
							Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
							Actor->MarkAsGarbage();
						}
					}
					else
					{
						ReuseOutput = true;
					}
				}
			}

			if (ReuseOutput)
			{
				InOldOutputs.RemoveAt(i);
				OutNewOutputs.Add(OldOutput);
			}
		}
	}

	// All our output objects now have their HGPO assigned
	// We can now parse them to update the output type
	for (auto& Output : OutNewOutputs)
	{
		Output->UpdateOutputType();
	}

	return true;
}

bool
FHoudiniOutputTranslator::UpdateChangedOutputs(UHoudiniAssetComponent* HAC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::UpdateChangedOutputs);

	if (!IsValid(HAC))
		return false;

	UObject* OuterComponent = HAC;

	FHoudiniPackageParams PackageParams;
	PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
	PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

	PackageParams.BakeFolder = HAC->GetBakeFolderOrDefault();
	PackageParams.TempCookFolder = HAC->GetTemporaryCookFolderOrDefault();

	PackageParams.OuterPackage = HAC->GetComponentLevel();
	PackageParams.HoudiniAssetName = HAC->GetHoudiniAssetName();
	PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetActorNameOrLabel();
	PackageParams.ComponentGUID = HAC->GetComponentGUID();
	PackageParams.ObjectName = FString();
	
	TArray<UHoudiniOutput*>& Outputs = HAC->Outputs;

	TArray<UHoudiniOutput *> OutputsToUpdate;

	// Iterate through the outputs array of HAC.
	for (int32 Index = 0; Index < HAC->GetNumOutputs(); ++Index)
	{
		UHoudiniOutput* CurrentOutput = HAC->GetOutputAt(Index);
		if (!CurrentOutput)
			continue;

		if (!HAC->IsOutputTypeSupported(CurrentOutput->GetType()))
			continue;

		switch (CurrentOutput->GetType())
		{
			case EHoudiniOutputType::Instancer:
			{
				bool bNeedToRecreateInstancers = false;
				for (auto& Iter : CurrentOutput->GetInstancedOutputs())
				{
					FHoudiniInstancedOutput& InstOutput = Iter.Value;
					if (!InstOutput.bChanged)
						continue;

					/*
					FHoudiniInstanceTranslator::UpdateChangedInstancedOutput(
						InstOutput, Iter.Key, CurrentOutput, HAC);
					*/

					// TODO:
					// UpdateChangedInstancedOutput needs some improvements
					// as it currently destroy too many components.
					// For now, we'll update all the instancers
					bNeedToRecreateInstancers = true;

					InstOutput.MarkChanged(false);
				}

				if (bNeedToRecreateInstancers)
				{
					if (HAC->GetAssetState() == EHoudiniAssetState::NeedInstantiation || HAC->HasBeenLoaded())
					{
						// Instantiate the HDA if it's not been
						// This is because CreateAllInstancersFromHoudiniOutput() actually reads the transform from HAPI
						// Calling it on a HDA not yet instantiated causes a crash...
						HAC->SetAssetState(EHoudiniAssetState::PreInstantiation);
					}
					else
					{
						OutputsToUpdate.Add(CurrentOutput);

					}
				}
			}
			break;

			case EHoudiniOutputType::Curve:
			{
				//FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput(CurrentOutput, HAC);
			}
			break;

			default:
				break;
		}
	}

	FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs(OutputsToUpdate, Outputs, HAC, PackageParams);

	return true;
}

void
FHoudiniOutputTranslator::CacheObjectInfo(const HAPI_ObjectInfo& InObjInfo, FHoudiniObjectInfo& OutObjInfoCache)
{
	FHoudiniEngineString hapiSTR(InObjInfo.nameSH);
	hapiSTR.ToFString(OutObjInfoCache.Name);
	//OutObjInfoCache.Name = InObjInfo.nameSH;

	OutObjInfoCache.NodeId = InObjInfo.nodeId;
	OutObjInfoCache.ObjectToInstanceID = InObjInfo.objectToInstanceId;

	OutObjInfoCache.bHasTransformChanged = InObjInfo.hasTransformChanged;
	OutObjInfoCache.bHaveGeosChanged = InObjInfo.haveGeosChanged;

	OutObjInfoCache.bIsVisible = InObjInfo.isVisible;
	OutObjInfoCache.bIsInstancer = InObjInfo.isInstancer;
	OutObjInfoCache.bIsInstanced = InObjInfo.isInstanced;

	OutObjInfoCache.GeoCount = InObjInfo.geoCount;
};

EHoudiniGeoType
FHoudiniOutputTranslator::ConvertHapiGeoType(const HAPI_GeoType& InType)
{
	EHoudiniGeoType OutType = EHoudiniGeoType::Invalid;
	switch (InType)
	{
	case HAPI_GEOTYPE_DEFAULT:
		OutType = EHoudiniGeoType::Default;
		break;

	case HAPI_GEOTYPE_INTERMEDIATE:
		OutType = EHoudiniGeoType::Intermediate;
		break;

	case HAPI_GEOTYPE_INPUT:
		OutType = EHoudiniGeoType::Input;
		break;

	case HAPI_GEOTYPE_CURVE:
		OutType = EHoudiniGeoType::Curve;
		break;

	default:
		OutType = EHoudiniGeoType::Invalid;
		break;
	}

	return OutType;
}

void
FHoudiniOutputTranslator::CacheGeoInfo(const HAPI_GeoInfo& InGeoInfo, FHoudiniGeoInfo& OutGeoInfoCache)
{
	OutGeoInfoCache.Type = ConvertHapiGeoType(InGeoInfo.type);

	FHoudiniEngineString hapiSTR(InGeoInfo.nameSH);
	hapiSTR.ToFString(OutGeoInfoCache.Name);

	OutGeoInfoCache.NodeId = InGeoInfo.nodeId;

	OutGeoInfoCache.bIsEditable = InGeoInfo.isEditable;
	OutGeoInfoCache.bIsTemplated = InGeoInfo.isTemplated;
	OutGeoInfoCache.bIsDisplayGeo = InGeoInfo.isDisplayGeo;
	OutGeoInfoCache.bHasGeoChanged = InGeoInfo.hasGeoChanged;
	OutGeoInfoCache.bHasMaterialChanged = InGeoInfo.hasMaterialChanged;

	OutGeoInfoCache.PartCount = InGeoInfo.partCount;
	OutGeoInfoCache.PointGroupCount = InGeoInfo.pointGroupCount;
	OutGeoInfoCache.PrimitiveGroupCount = InGeoInfo.primitiveGroupCount;
};


EHoudiniPartType
FHoudiniOutputTranslator::ConvertHapiPartType(const HAPI_PartType& InType)
{
	EHoudiniPartType OutType = EHoudiniPartType::Invalid;
	switch (InType)
	{
		case HAPI_PARTTYPE_BOX:
		case HAPI_PARTTYPE_SPHERE:
		case HAPI_PARTTYPE_MESH:
			OutType = EHoudiniPartType::Mesh;
			break;

		case HAPI_PARTTYPE_CURVE:
			OutType = EHoudiniPartType::Curve;
			break;

		case HAPI_PARTTYPE_INSTANCER:
			OutType = EHoudiniPartType::Instancer;
			break;

		case HAPI_PARTTYPE_VOLUME:
			OutType = EHoudiniPartType::Volume;
			break;

		default:
			OutType = EHoudiniPartType::Invalid;
			break;
	}

	return OutType;
}

void
FHoudiniOutputTranslator::CachePartInfo(const HAPI_PartInfo& InPartInfo, FHoudiniPartInfo& OutPartInfoCache)
{
	OutPartInfoCache.PartId = InPartInfo.id;
	
	FHoudiniEngineString hapiSTR(InPartInfo.nameSH);
	hapiSTR.ToFString(OutPartInfoCache.Name);

	OutPartInfoCache.Type = ConvertHapiPartType(InPartInfo.type);

	OutPartInfoCache.FaceCount = InPartInfo.faceCount;
	OutPartInfoCache.VertexCount = InPartInfo.vertexCount;
	OutPartInfoCache.PointCount = InPartInfo.pointCount;

	OutPartInfoCache.PointAttributeCounts = InPartInfo.attributeCounts[HAPI_ATTROWNER_POINT];
	OutPartInfoCache.VertexAttributeCounts = InPartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX];
	OutPartInfoCache.PrimitiveAttributeCounts = InPartInfo.attributeCounts[HAPI_ATTROWNER_PRIM];
	OutPartInfoCache.DetailAttributeCounts = InPartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL];

	OutPartInfoCache.bIsInstanced = InPartInfo.isInstanced;

	OutPartInfoCache.InstancedPartCount = InPartInfo.instancedPartCount;
	OutPartInfoCache.InstanceCount = InPartInfo.instanceCount;

	OutPartInfoCache.bHasChanged = InPartInfo.hasChanged;
};

void
FHoudiniOutputTranslator::CacheVolumeInfo(const HAPI_VolumeInfo& InVolumeInfo, FHoudiniVolumeInfo& OutVolumeInfoCache)
{
	FHoudiniEngineString hapiSTR(InVolumeInfo.nameSH);
	hapiSTR.ToFString(OutVolumeInfoCache.Name);
	
	OutVolumeInfoCache.bIsVDB = (InVolumeInfo.type == HAPI_VOLUMETYPE_VDB); // replaces VolumeType Type;

	OutVolumeInfoCache.TupleSize = InVolumeInfo.tupleSize;
	OutVolumeInfoCache.bIsFloat = (InVolumeInfo.storage == HAPI_STORAGETYPE_FLOAT); // replaces StorageType StorageType;
	OutVolumeInfoCache.TileSize = InVolumeInfo.tileSize;

	FHoudiniEngineUtils::TranslateHapiTransform(InVolumeInfo.transform, OutVolumeInfoCache.Transform);
	OutVolumeInfoCache.bHasTaper = InVolumeInfo.hasTaper;

	OutVolumeInfoCache.XLength = InVolumeInfo.xLength;
	OutVolumeInfoCache.YLength = InVolumeInfo.yLength;
	OutVolumeInfoCache.ZLength = InVolumeInfo.zLength;

	OutVolumeInfoCache.MinX = InVolumeInfo.minX;
	OutVolumeInfoCache.MinY = InVolumeInfo.minY;
	OutVolumeInfoCache.MinZ = InVolumeInfo.minZ;

	OutVolumeInfoCache.XTaper = InVolumeInfo.xTaper;
	OutVolumeInfoCache.YTaper = InVolumeInfo.yTaper;
};

EHoudiniCurveType
FHoudiniOutputTranslator::ConvertHapiCurveType(const HAPI_CurveType& InType)
{
	EHoudiniCurveType OutType = EHoudiniCurveType::Invalid;
	switch (InType)
	{
	case HAPI_CURVETYPE_LINEAR:
		OutType = EHoudiniCurveType::Polygon;
		break;

	case HAPI_CURVETYPE_NURBS:
		OutType = EHoudiniCurveType::Nurbs;
		break;

	case HAPI_CURVETYPE_BEZIER:
		OutType = EHoudiniCurveType::Bezier;
		break;

	case HAPI_CURVETYPE_MAX:
		OutType = EHoudiniCurveType::Points;
		break;

	default:
		OutType = EHoudiniCurveType::Invalid;
		break;
	}

	return OutType;
}

void
FHoudiniOutputTranslator::CacheCurveInfo(const HAPI_CurveInfo& InCurveInfo, FHoudiniCurveInfo& OutCurveInfoCache)
{
	OutCurveInfoCache.Type = ConvertHapiCurveType(InCurveInfo.curveType);

	OutCurveInfoCache.CurveCount = InCurveInfo.curveCount;
	OutCurveInfoCache.VertexCount = InCurveInfo.vertexCount;
	OutCurveInfoCache.KnotCount = InCurveInfo.knotCount;

	OutCurveInfoCache.bIsPeriodic = InCurveInfo.isPeriodic;
	OutCurveInfoCache.bIsRational = InCurveInfo.isRational;

	OutCurveInfoCache.Order = InCurveInfo.order;

	OutCurveInfoCache.bHasKnots = InCurveInfo.hasKnots;
};


void
FHoudiniOutputTranslator::ClearAndRemoveOutputs(UHoudiniAssetComponent *InHAC, TArray<UHoudiniOutput*>& OutputsPendingClear, bool bForceClearAll)
{
	if (!IsValid(InHAC))
		return;
	
	// DO NOT MANUALLY DESTROY THE OLD/DANGLING OUTPUTS!
	// This messes up unreal's Garbage collection and would cause crashes on duplication
	// Simply clearing the array is enough
	for (auto& OldOutput : InHAC->Outputs)
	{
		if (OldOutput->ShouldDeferClear() && !bForceClearAll)
		{
			OutputsPendingClear.Add(OldOutput);
		}
		else
		{
			OldOutput->DestroyCookedData();
		}
	}

	InHAC->Outputs.Empty();	
}

void 
FHoudiniOutputTranslator::ClearOutput(UHoudiniOutput* Output) 
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniOutputTranslator::ClearOutput);

	switch (Output->GetType()) 
	{
		case EHoudiniOutputType::Landscape:
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				// Currently, any Landscape managed by an HDA is always present in the current level.
				// Only when it gets baked will Landscapes be serialized to other levels so for now
				// assume that a landscape should be available, unless explicitly deleted the user.
				UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.Value.OutputObject);
				if (!LandscapePtr)
					continue;

				ALandscapeProxy* Landscape = LandscapePtr->GetRawPtr();

				if (!IsValid(Landscape) || !Landscape->IsValidLowLevel())
					continue;

				Landscape->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				Landscape->ConditionalBeginDestroy();
				Landscape->UnregisterAllComponents();
				Landscape->Destroy();
				LandscapePtr->SetSoftPtr(nullptr);
			}
		}
		break;

		case EHoudiniOutputType::Instancer:
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				// Note: don't delete the actual components created by Unreal - they are "owned" by Unreal-  instead expcility remove the foliage types.

				if (IsValid(OutputObject.Value.FoliageType))
        {
				  if (IsValid(OutputObject.Value.World))
				  {
						FHoudiniFoliageUtils::RemoveFoliageTypeFromWorld(OutputObject.Value.World, OutputObject.Value.FoliageType);
				  }
					else
				  {
						HOUDINI_LOG_ERROR(TEXT("Trying to delete Foliage Type, but no World was set. "
                            "Most likely this foliage was cooked with a previous vertion of the Houdini Plugin, try deleting the foliage manually."));
				  }
        }
			}
		}
		break;
		// ... Other output types ...//

		default:
		break;
	
	}

	Output->Clear();
}


bool
FHoudiniOutputTranslator::GetCustomPartNameFromAttribute(const HAPI_NodeId & NodeId, const HAPI_PartId & PartId, FString & OutCustomPartName) 
{
	TArray<FString> CustomNames;

	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2);
	bool bHasCustomName = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, CustomNames);

	if (!bHasCustomName)
	{
		Accessor.Init(NodeId, PartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1);
		bHasCustomName = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, CustomNames);
	}
	
	if (!bHasCustomName)
		return false;

	if (CustomNames.Num() <= 0)
		return false;

	OutCustomPartName = CustomNames[0];

	if (OutCustomPartName.IsEmpty())
		return false;

	return true;
}

#undef LOCTEXT_NAMESPACE