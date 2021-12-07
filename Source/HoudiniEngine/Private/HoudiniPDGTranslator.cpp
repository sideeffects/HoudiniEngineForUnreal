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

#include "HoudiniPDGTranslator.h"


#include "Editor.h"
#include "Containers/Array.h"
#include "FileHelpers.h"
#include "LandscapeInfo.h"
// #include "Engine/WorldComposition.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniGeoImporter.h"
#include "HoudiniPackageParams.h"
#include "HoudiniOutput.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniSplineTranslator.h"
#include "HoudiniTranslatorTypes.h"

#define LOCTEXT_NAMESPACE "HoudiniEngine"

bool
FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem(
	UHoudiniPDGAssetLink* InAssetLink,
	UTOPNode* InTOPNode,
	FTOPWorkResultObject& InWorkResultObject,
	const FHoudiniPackageParams& InPackageParams,
	TArray<EHoudiniOutputType> InOutputTypesToProcess,
	bool bInTreatExistingMaterialsAsUpToDate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem);

	if (!IsValid(InAssetLink))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem]: InAssetLink is null."));
		return false;
	}

	if (!IsValid(InTOPNode))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem]: InTOPNode is null."));
		return false;
	}
	
	TArray<UHoudiniOutput*> OldTOPOutputs = InWorkResultObject.GetResultOutputs();
	TArray<UHoudiniOutput*> NewTOPOutputs;

	FHoudiniEngine::Get().CreateTaskSlateNotification(LOCTEXT("LoadPDGBGEO", "Loading PDG Output BGEO File..."));
	
	bool bResult = false;
	// Create a new file node in HAPI for the bgeo and cook it
	HAPI_NodeId FileNodeId = -1;
	bResult = UHoudiniGeoImporter::OpenBGEOFile(InWorkResultObject.FilePath, FileNodeId);
	if (bResult)
		bResult = UHoudiniGeoImporter::CookFileNode(FileNodeId);

	// If the cook was successful, build outputs
	if (bResult)
	{
		FHoudiniEngine::Get().UpdateTaskSlateNotification(
			LOCTEXT("BuildPDGBGEOOutputs", "Building Ouputs from BGEO File..."));
		
		const bool bAddOutputsToRootSet = false;
		bResult = UHoudiniGeoImporter::BuildAllOutputsForNode(
	        FileNodeId,
	        InAssetLink,
	        OldTOPOutputs,
	        NewTOPOutputs,
	        bAddOutputsToRootSet);
	}

	if (bResult)
	{
		FHoudiniEngine::Get().UpdateTaskSlateNotification(
            LOCTEXT("TranslatePDGBGEOOutputs", "Translating PDG/BGEO Outputs..."));

		// If we successfully received outputs from the BGEO file, process the outputs
		FOutputActorOwner& WROOutputActorOwner = InWorkResultObject.GetOutputActorOwner();
		AActor* WorkItemOutputActor = WROOutputActorOwner.GetOutputActor();
		if (!IsValid(WorkItemOutputActor))
		{
			UWorld* World = InAssetLink->GetWorld();
			FOutputActorOwner& NodeOutputActorOwner = InTOPNode->GetOutputActorOwner();
			AActor* TOPNodeOutputActor = NodeOutputActorOwner.GetOutputActor();
			if (!IsValid(TOPNodeOutputActor))
			{
				if (NodeOutputActorOwner.CreateOutputActor(World, InAssetLink, InAssetLink->OutputParentActor, FName(InTOPNode->NodeName)))
					TOPNodeOutputActor = NodeOutputActorOwner.GetOutputActor();
			}
			if (WROOutputActorOwner.CreateOutputActor(World, InAssetLink, TOPNodeOutputActor, FName(InWorkResultObject.Name)))
				WorkItemOutputActor = WROOutputActorOwner.GetOutputActor();
		}

		for (auto& OldOutput : OldTOPOutputs)
		{
			FHoudiniOutputTranslator::ClearOutput(OldOutput);
		}
		OldTOPOutputs.Empty();
		InWorkResultObject.GetResultOutputs().Empty();
		InWorkResultObject.SetResultOutputs(NewTOPOutputs);

		// Gather landscape actors from inputs.
		// NOTE: If performance becomes a problem, cache these on the TOPNode along with all the other cached landscape
		// data.
		TArray<ALandscapeProxy *> AllInputLandscapes;
		TArray<ALandscapeProxy *> InputLandscapesToUpdate;
		UHoudiniAssetComponent* HAC = InAssetLink->GetOuterHoudiniAssetComponent();
		FHoudiniEngineUtils::GatherLandscapeInputs(HAC, AllInputLandscapes, InputLandscapesToUpdate);

		bResult = CreateAllResultObjectsFromPDGOutputs(
			NewTOPOutputs,
			InPackageParams,
			WorkItemOutputActor->GetRootComponent(),
			InTOPNode->GetLandscapeExtent(),
			InTOPNode->GetLandscapeReferenceLocation(),
			InTOPNode->GetLandscapeSizeInfo(),
			InTOPNode->ClearedLandscapeLayers,
			AllInputLandscapes,
			InputLandscapesToUpdate,
			InAssetLink,
			InOutputTypesToProcess,
			bInTreatExistingMaterialsAsUpToDate);
		
		if (!bResult)
			FHoudiniEngine::Get().FinishTaskSlateNotification(
				LOCTEXT("TranslatePDGBGEOOutputsFail", "Failed to translate all PDG/BGEO Outputs..."));
		else
			FHoudiniEngine::Get().FinishTaskSlateNotification(
				LOCTEXT("TranslatePDGBGEOOutputsDone", "Done: Translating PDG/BGEO Outputs."));		

		InTOPNode->UpdateOutputVisibilityInLevel();
	}
	else
	{
		FHoudiniEngine::Get().FinishTaskSlateNotification(
			LOCTEXT("BuildPDGBGEOOutputsFail", "Failed building outputs from BGEO file..."));
	}

	// Delete the file node used to load the BGEO via HAPI
	if (FileNodeId >= 0)
	{
		UHoudiniGeoImporter::CloseBGEOFile(FileNodeId);
		FileNodeId = -1;
	}

	return bResult;
}

bool
FHoudiniPDGTranslator::LoadExistingAssetsAsResultObjectsForPDGWorkItem(
	UHoudiniPDGAssetLink* InAssetLink,
	UTOPNode* InTOPNode,
	FTOPWorkResultObject& InWorkResultObject,
	const FHoudiniPackageParams& InPackageParams,
	TArray<UHoudiniOutput*>& InOutputs,
	TArray<EHoudiniOutputType> InOutputTypesToProcess,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* InPreBuiltInstancedOutputPartData)
{
	if (!IsValid(InAssetLink))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniPDGTranslator::LoadExistingAssetsAsResultObjectsForPDGWorkItem]: InAssetLink is null."));
		return false;
	}

	if (!IsValid(InTOPNode))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniPDGTranslator::LoadExistingAssetsAsResultObjectsForPDGWorkItem]: InTOPNode is null."));
		return false;
	}

	FHoudiniEngine::Get().CreateTaskSlateNotification(
		LOCTEXT("TranslatePDGBGEOOutputs", "Translating PDG/BGEO Outputs..."));

	// If we successfully received outputs from the BGEO file, process the outputs
	FOutputActorOwner& WROOutputActorOwner = InWorkResultObject.GetOutputActorOwner();
	AActor* WorkItemOutputActor = WROOutputActorOwner.GetOutputActor();
	if (!IsValid(WorkItemOutputActor))
	{
		UWorld* World = InAssetLink->GetWorld();
		FOutputActorOwner& NodeOutputActorOwner = InTOPNode->GetOutputActorOwner();
		AActor* TOPNodeOutputActor = NodeOutputActorOwner.GetOutputActor();
		if (!IsValid(TOPNodeOutputActor))
		{
			if (NodeOutputActorOwner.CreateOutputActor(World, InAssetLink, InAssetLink->OutputParentActor, FName(InTOPNode->NodeName)))
				TOPNodeOutputActor = NodeOutputActorOwner.GetOutputActor();
		}
		if (WROOutputActorOwner.CreateOutputActor(World, InAssetLink, TOPNodeOutputActor, FName(InWorkResultObject.Name)))
			WorkItemOutputActor = WROOutputActorOwner.GetOutputActor();
	}

	InWorkResultObject.SetResultOutputs(InOutputs);

	// Gather landscape actors from inputs.
	// NOTE: If performance becomes a problem, cache these on the TOPNode along with all the other cached landscape
	// data.
	TArray<ALandscapeProxy *> AllInputLandscapes;
	TArray<ALandscapeProxy *> InputLandscapesToUpdate;
	UHoudiniAssetComponent* HAC = InAssetLink->GetOuterHoudiniAssetComponent();
	FHoudiniEngineUtils::GatherLandscapeInputs(HAC, AllInputLandscapes, InputLandscapesToUpdate);

	const bool bInTreatExistingMaterialsAsUpToDate = true;
	const bool bOnlyUseExistingAssets = true;
	const bool bResult = CreateAllResultObjectsFromPDGOutputs(
		InOutputs,
		InPackageParams,
		WorkItemOutputActor->GetRootComponent(),
		InTOPNode->GetLandscapeExtent(),
		InTOPNode->GetLandscapeReferenceLocation(),
		InTOPNode->GetLandscapeSizeInfo(),
		InTOPNode->ClearedLandscapeLayers,
		AllInputLandscapes,
		InputLandscapesToUpdate,
		InAssetLink,
		InOutputTypesToProcess,
		bInTreatExistingMaterialsAsUpToDate,
		bOnlyUseExistingAssets,
		InPreBuiltInstancedOutputPartData);

	if (!bResult)
		FHoudiniEngine::Get().FinishTaskSlateNotification(
			LOCTEXT("TranslatePDGBGEOOutputsFail", "Failed to translate all PDG/BGEO Outputs..."));
	else
		FHoudiniEngine::Get().FinishTaskSlateNotification(
			LOCTEXT("TranslatePDGBGEOOutputsDone", "Done: Translating PDG/BGEO Outputs."));

	InTOPNode->UpdateOutputVisibilityInLevel();

	return bResult;
}

bool
FHoudiniPDGTranslator::CreateAllResultObjectsFromPDGOutputs(
	TArray<UHoudiniOutput*>& InOutputs,
	const FHoudiniPackageParams& InPackageParams,
	UObject* InOuterComponent,
	FHoudiniLandscapeExtent& CachedLandscapeExtent,
	FHoudiniLandscapeReferenceLocation& CachedLandscapeRefLoc,
	FHoudiniLandscapeTileSizeInfo& CachedLandscapeSizeInfo,
	TSet<FString>& ClearedLandscapeLayers,
	TArray<ALandscapeProxy*> AllInputLandscapes,
	TArray<ALandscapeProxy*> InputLandscapesToUpdate,
	UHoudiniPDGAssetLink* const InAssetLink,
	TArray<EHoudiniOutputType> InOutputTypesToProcess,
	bool bInTreatExistingMaterialsAsUpToDate,
	bool bInOnlyUseExistingAssets,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* InPreBuiltInstancedOutputPartData
	)
{
	// Process the new/updated outputs via the various translators
	// We try to maintain as much parity with the existing HoudiniAssetComponent workflow
	// as possible.


	// // Before processing any of the output,
	// // we need to get the min/max value for all Height volumes in this output (if any)
	TMap<FString, float> LandscapeLayerGlobalMinimums;
	TMap<FString, float> LandscapeLayerGlobalMaximums;

	for (UHoudiniOutput* CurOutput : InOutputs)
	{
		const EHoudiniOutputType OutputType = CurOutput->GetType();
		if (OutputType == EHoudiniOutputType::Landscape)
		{
			// Populate all layer minimums/maximums with default values since, in PDG mode,
			// we can't collect the values across all tiles. The user would have to do this
			// manually in the Topnet.
			FHoudiniLandscapeTranslator::GetLayersZMinZMax(CurOutput->GetHoudiniGeoPartObjects(), LandscapeLayerGlobalMinimums, LandscapeLayerGlobalMaximums);
		}
	}
	
	TArray<UHoudiniOutput*> InstancerOutputs;
	TArray<UHoudiniOutput*> LandscapeOutputs;
	TArray<UPackage*> CreatedPackages;

	//bool bCreatedNewMaps = false;
	UWorld* PersistentWorld = InOuterComponent->GetTypedOuter<UWorld>();
	check(PersistentWorld);

	// Fetch the HAC if the asset link is associated with one
	UHoudiniAssetComponent const* const HAC = IsValid(InAssetLink) ? InAssetLink->GetOuterHoudiniAssetComponent() : nullptr;
	const bool bIsHACValid = IsValid(HAC);
	
	// Keep track of all generated houdini materials to avoid recreating them over and over
	TMap<FString, UMaterialInterface*> AllOutputMaterials;

	for (UHoudiniOutput* CurOutput : InOutputs)
	{
		const EHoudiniOutputType OutputType = CurOutput->GetType();
		if (InOutputTypesToProcess.Num() > 0 && !InOutputTypesToProcess.Contains(OutputType))
		{
			continue;
		}
		switch (OutputType)
		{
			case EHoudiniOutputType::Mesh:
			{
				const bool bInDestroyProxies = false;
				if (bInOnlyUseExistingAssets)
				{
					const bool bInApplyGenericProperties = false;
					TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects(CurOutput->GetOutputObjects());
					FHoudiniMeshTranslator::CreateOrUpdateAllComponents(
						CurOutput,
						InOuterComponent,
						NewOutputObjects,
						bInDestroyProxies,
						bInApplyGenericProperties
					);
				}
				else
				{
					// If we have a valid HAC get SMGP/MBS from it, otherwise use the plugin settings
					FHoudiniMeshTranslator::CreateAllMeshesAndComponentsFromHoudiniOutput(
						CurOutput,
						InPackageParams,
						EHoudiniStaticMeshMethod::RawMesh,
						bIsHACValid ? HAC->StaticMeshGenerationProperties : FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties(),
						bIsHACValid ? HAC->StaticMeshBuildSettings : FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings(),
						AllOutputMaterials,
						InOuterComponent,
						bInTreatExistingMaterialsAsUpToDate,
						bInDestroyProxies
					);
				}
			}
			break;

			case EHoudiniOutputType::Curve:
			{
				// Output curve
				FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput(CurOutput, InOuterComponent);
				break;
			}

			case EHoudiniOutputType::Instancer:
			{
				InstancerOutputs.Add(CurOutput);
			}
			break;

			case EHoudiniOutputType::Landscape:
			{
				// Retrieve the topnet parent to which Sharedlandscapes will be attached.
				AActor* WorkItemActor = InOuterComponent->GetTypedOuter<AActor>();
				USceneComponent* TopnetParent = nullptr;
				if (WorkItemActor)
				{
					AActor* TopnetParentActor = WorkItemActor->GetAttachParentActor();
					if (TopnetParentActor)
					{
						TopnetParent = TopnetParentActor->GetRootComponent();
					}
				}
				TArray<TWeakObjectPtr<AActor>> CreatedUntrackedOutputs;

				FHoudiniLandscapeTranslator::CreateLandscape(
					CurOutput,
					CreatedUntrackedOutputs,
					InputLandscapesToUpdate,
					AllInputLandscapes,
					TopnetParent,
					TEXT("{hda_actor_name}_{pdg_topnet_name}_"),
					PersistentWorld,
					LandscapeLayerGlobalMinimums,
					LandscapeLayerGlobalMaximums,
					CachedLandscapeExtent,
					CachedLandscapeSizeInfo,
					CachedLandscapeRefLoc,
					InPackageParams,
					//bCreatedNewMaps,
					ClearedLandscapeLayers,
					CreatedPackages);
				// Attach any landscape actors to InOuterComponent
				LandscapeOutputs.Add(CurOutput);
			}
			break;

			default:
			{
				HOUDINI_LOG_WARNING(TEXT("[FTOPWorkResultObject::UpdateResultOutputs]: Unsupported output type: %s"), *UHoudiniOutput::OutputTypeToString(OutputType));
			}
			break;
		}

		for (auto& CurMat : CurOutput->GetAssignementMaterials())
		{
			//Adds the generated materials if any
			if (!AllOutputMaterials.Contains(CurMat.Key))
				AllOutputMaterials.Add(CurMat);
		}
	}

	// Process instancer outputs after all other outputs have been processed, since it
	// might depend on meshes etc from other outputs
	if (InstancerOutputs.Num() > 0)
	{
		for (UHoudiniOutput* CurOutput : InstancerOutputs)
		{
			FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutput(
				CurOutput,
				InOutputs,
				InOuterComponent,
				InPackageParams,
				InPreBuiltInstancedOutputPartData);
		}
	}
	
	USceneComponent* ParentComponent = Cast<USceneComponent>(InOuterComponent);
	if (IsValid(ParentComponent))
	{
		AActor* ParentActor = ParentComponent->GetOwner();
		for (UHoudiniOutput* LandscapeOutput : LandscapeOutputs)
		{
			if(!IsValid(LandscapeOutput))
				continue;

			for (auto& Pair  : LandscapeOutput->GetOutputObjects())
			{
				FHoudiniOutputObject& OutputObject = Pair.Value;

				// If the OutputObject has an OutputComponent, try to attach it to ParentComponent
				if (IsValid(OutputObject.OutputComponent))
				{
					USceneComponent* Component = Cast<USceneComponent>(OutputObject.OutputComponent);
					if (IsValid(Component) && !Component->IsAttachedTo(ParentComponent))
					{
						Component->AttachToComponent(ParentComponent, FAttachmentTransformRules::KeepWorldTransform);
						continue;
					}
				}

				// If OutputObject does not have an OutputComponent, check if OutputObject is an AActor and attach
				// it to ParentComponent
				if (IsValid(OutputObject.OutputObject))
				{
					UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.OutputObject);
					if (IsValid(LandscapePtr))
					{
						ALandscapeProxy* LandscapeProxy = LandscapePtr->GetRawPtr();
						if (IsValid(LandscapeProxy))
						{
							if (!LandscapeProxy->IsAttachedTo(ParentActor))
							{
								LandscapeProxy->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
								LandscapeProxy->RecreateCollisionComponents();
							}

							if (LandscapeProxy)
							{
								// We need to recreate component states for landscapes if a tile was created, moved, or resized
								// otherwise the landscape will exhibit render artifacts (such as only rendering every other
								// component.)
								LandscapeProxy->RecreateComponentsState();
							}
						}
					}
				}
			}
		}
	}

	/*
	if (bCreatedNewMaps && PersistentWorld)
	{
		// Force the asset registry to update its cache of packages paths
		// recursively for this world, otherwise world composition won't
		// pick them up during the WorldComposition::Rescan().
		FHoudiniEngineUtils::RescanWorldPath(PersistentWorld);

		ULandscapeInfo::RecreateLandscapeInfo(PersistentWorld, true);

		if (IsValid(PersistentWorld->WorldComposition))
		{
			UWorldComposition::WorldCompositionChangedEvent.Broadcast(PersistentWorld);
		}
		
		FEditorDelegates::RefreshLevelBrowser.Broadcast();
		FEditorDelegates::RefreshAllBrowsers.Broadcast();
	}
	*/

	if (CreatedPackages.Num() > 0)
	{
		// Save created packages. For example, we don't want landscape layers deleted 
		// along with the HDA.
		FEditorFileUtils::PromptForCheckoutAndSave(CreatedPackages, true, false);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
