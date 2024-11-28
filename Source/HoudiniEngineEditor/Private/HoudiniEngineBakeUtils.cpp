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

#include "HoudiniEngineBakeUtils.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEnginePrivatePCH.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniBakeLandscape.h"
#include "HoudiniDataLayerUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineOutputStats.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniFoliageTools.h"
#include "HoudiniGeometryCollectionTranslator.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniOutput.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniPackageParams.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniStringResolver.h"
#include "UnrealLandscapeTranslator.h"

#include "ActorFactories/ActorFactoryClass.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetSelection.h"
#include "AssetToolsModule.h"
#include "BusyCursor.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelBounds.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/WorldComposition.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/WorldFactory.h"
#include "FileHelpers.h"
#include "FoliageEditUtility.h"
#include "GameFramework/Actor.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollection/GeometryCollectionObject.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"	
#endif
#include "HAL/FileManager.h"
#include "InstancedFoliageActor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "LandscapeStreamingProxy.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "MaterialEditingLibrary.h"
#else
	#include "MaterialEditor/Public/MaterialEditingLibrary.h"
#endif

#include "Materials/Material.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h" 
#include "Materials/MaterialInstance.h"
#include "Math/Box.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "PackageTools.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RawMesh.h"
#include "SkeletalMeshTypes.h"
#include "Sound/SoundBase.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif
#include "HoudiniBakeLevelInstanceUtils.h"
#include "HoudiniLevelInstanceUtils.h"
#include "IDetailTreeNode.h"
#include "LevelInstance/LevelInstanceActor.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	#include "LevelInstance/LevelInstanceComponent.h"
#endif
#include "Materials/MaterialExpressionTextureSample.h" 
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "HoudiniHLODLayerUtils.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif

#include "Animation/Skeleton.h"
#include "Engine/DataTable.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "HoudiniFoliageUtils.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/AnimSequence.h"


HOUDINI_BAKING_DEFINE_LOG_CATEGORY();

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE


FHoudiniEngineBakeState::FHoudiniEngineBakeState(const int32 InNumOutputs, const TArray<FHoudiniBakedOutput>& InOldBakedOutputs)
{
	OldBakedOutputs = InOldBakedOutputs;
	OldBakedOutputs.SetNum(InNumOutputs);
	NewBakedOutputs.SetNum(InNumOutputs);
}


const FHoudiniBakedOutputObject*
FHoudiniEngineBakeState::FindOldBakedOutputObject(const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	check(OldBakedOutputs.IsValidIndex(InOutputIndex));
	return OldBakedOutputs[InOutputIndex].BakedOutputObjects.Find(InIdentifier);
}


FHoudiniBakedOutputObject
FHoudiniEngineBakeState::MakeNewBakedOutputObject(const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier, bool& bOutHasPreviousBakeData)
{
	FHoudiniBakedOutputObject NewBakedOutputObject;

	if (FHoudiniBakedOutputObject const* const Entry = FindOldBakedOutputObject(InOutputIndex, InIdentifier))
	{
		bOutHasPreviousBakeData = true;
		NewBakedOutputObject = *Entry;
	}
	else
	{
		bOutHasPreviousBakeData = false;
	}

	return NewBakedOutputObject;
}


const FHoudiniBakedOutputObject&
FHoudiniEngineBakeState::FindNewBakedOutputObjectChecked(const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	check(NewBakedOutputs.IsValidIndex(InOutputIndex));
	return NewBakedOutputs[InOutputIndex].BakedOutputObjects.FindChecked(InIdentifier);
}


FHoudiniBakedOutputObject&
FHoudiniEngineBakeState::FindOrAddNewBakedOutputObject(const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	check(NewBakedOutputs.IsValidIndex(InOutputIndex));
	return NewBakedOutputs[InOutputIndex].BakedOutputObjects.FindOrAdd(InIdentifier);
}


FHoudiniBakedOutputObject&
FHoudiniEngineBakeState::SetNewBakedOutputObject(const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier, const FHoudiniBakedOutputObject& InBakedOutputObject)
{
	check(NewBakedOutputs.IsValidIndex(InOutputIndex));
	return NewBakedOutputs[InOutputIndex].BakedOutputObjects.Emplace(InIdentifier, InBakedOutputObject);
}


FHoudiniBakedOutputObject&
FHoudiniEngineBakeState::SetNewBakedOutputObject(const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier, FHoudiniBakedOutputObject&& InBakedOutputObject)
{
	check(NewBakedOutputs.IsValidIndex(InOutputIndex));
	return NewBakedOutputs[InOutputIndex].BakedOutputObjects.Emplace(InIdentifier, InBakedOutputObject);
}


USkeleton*
FHoudiniEngineBakeState::FindBakedSkeleton(USkeleton const* const InTempSkeleton, bool& bFoundEntry) const
{
	USkeleton* const* const BakedSkeleton = BakedSkeletons.Find(InTempSkeleton);
	if (!BakedSkeleton)
	{
		bFoundEntry = false;
		return nullptr;
	}

	bFoundEntry = true;
	return *BakedSkeleton;
}


FHoudiniEngineBakedActor::FHoudiniEngineBakedActor()
	: Actor(nullptr)
	, OutputIndex(INDEX_NONE)
	, OutputObjectIdentifier()
	, ActorBakeName(NAME_None)
	, BakedObject(nullptr)
	, SourceObject(nullptr)
	, BakeFolderPath()
	, bInstancerOutput(false)
	, bPostBakeProcessPostponed(false)
{
}

FHoudiniEngineBakedActor::FHoudiniEngineBakedActor(
	AActor* InActor,
	FName InActorBakeName,
	FName InWorldOutlinerFolder,
	int32 InOutputIndex,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	UObject* InBakedObject,
	UObject* InSourceObject,
	UObject* InBakedComponent,
	const FString& InBakeFolderPath,
	const FHoudiniPackageParams& InBakedObjectPackageParams)
	: Actor(InActor)
	, OutputIndex(InOutputIndex)
	, OutputObjectIdentifier(InOutputObjectIdentifier)
	, ActorBakeName(InActorBakeName)
	, WorldOutlinerFolder(InWorldOutlinerFolder)
	, BakedObject(InBakedObject)
	, SourceObject(InSourceObject)
	, BakedComponent(InBakedComponent)
	, BakeFolderPath(InBakeFolderPath)
	, BakedObjectPackageParams(InBakedObjectPackageParams)
	, bInstancerOutput(false)
	, bPostBakeProcessPostponed(false)
{
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
	UHoudiniAssetComponent* InHACToBake,
	FHoudiniBakeSettings& BakeSettings,
	EHoudiniEngineBakeOption InBakeOption,
	bool bInRemoveHACOutputOnSuccess)
{
	if (!IsValid(InHACToBake))
		return false;

	// Handle proxies: if the output has any current proxies, first refine them
	bool bHACNeedsToReCook;
	if (!CheckForAndRefineHoudiniProxyMesh(InHACToBake, BakeSettings.bReplaceActors, InBakeOption, bInRemoveHACOutputOnSuccess, BakeSettings.bRecenterBakedActors, bHACNeedsToReCook))
	{
		// Either the component is invalid, or needs a recook to refine a proxy mesh
		return false;
	}

	bool bSuccess = false;
	switch (InBakeOption)
	{
	case EHoudiniEngineBakeOption::ToActor:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeHDAToActors(InHACToBake, BakeSettings);
	}
	break;

	case EHoudiniEngineBakeOption::ToBlueprint:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(InHACToBake, BakeSettings);
	}
	break;
	}

	if (bSuccess && bInRemoveHACOutputOnSuccess)
	{
		TArray<UHoudiniOutput*> DeferredClearOutputs;
		FHoudiniOutputTranslator::ClearAndRemoveOutputs(InHACToBake, DeferredClearOutputs, true);
	}
	
	return bSuccess;
}

bool 
FHoudiniEngineBakeUtils::BakeHDAToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent, 
	const FHoudiniBakeSettings& BakeSettings) 
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	TArray<FHoudiniEngineBakedActor> NewActors;
	FHoudiniBakedObjectData BakedObjectData;

	const bool bBakedWithErrors = !FHoudiniEngineBakeUtils::BakeHDAToActors(
		HoudiniAssetComponent, BakeSettings, NewActors, BakedObjectData);
	if (bBakedWithErrors)
	{
		// TODO ?
		HOUDINI_LOG_WARNING(TEXT("Errors when baking"));
	}

	// Save the created packages
	FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && NewActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : NewActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (BakeSettings.bRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}


	FHoudiniBakeLevelInstanceUtils::CreateLevelInstances(HoudiniAssetComponent, NewActors, HoudiniAssetComponent->GetBakeFolderOrDefault(), BakedObjectData);

	if (GEditor && NewActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakedObjectData.BakeStats.NumPackagesCreated, BakedObjectData.BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	// Broadcast that the bake is complete
	HoudiniAssetComponent->HandleOnPostBake(!bBakedWithErrors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeHDAToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniBakeSettings & BakeSettings,
	TArray<FHoudiniEngineBakedActor>& OutNewActors, 
	FHoudiniBakedObjectData& BakedObjectData,
	TArray<EHoudiniOutputType> * InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> * InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	// Get an array of the outputs
	const int32 NumOutputs = HoudiniAssetComponent->GetNumOutputs();
	TArray<UHoudiniOutput*> Outputs;
	Outputs.Reserve(NumOutputs);
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		Outputs.Add(HoudiniAssetComponent->GetOutputAt(OutputIdx));
	}

	FHoudiniEngineBakeState BakeState(NumOutputs, HoudiniAssetComponent->GetBakedOutputs());

	const TArray<FHoudiniEngineBakedActor> AllBakedActors;
	const bool bSuccess = BakeHoudiniOutputsToActors(
		HoudiniAssetComponent,
		Outputs,
		BakeState,
		HoudiniAssetComponent->GetComponentTransform(),
		HoudiniAssetComponent->BakeFolder,
		HoudiniAssetComponent->TemporaryCookFolder,
		BakeSettings,
		AllBakedActors,
		OutNewActors,
		BakedObjectData,
		InOutputTypesToBake,
		InInstancerComponentTypesToBake,
		InFallbackActor,
		InFallbackWorldOutlinerFolder);

	// Copy any relevant new / update data from the bake state to the HAC
	HoudiniAssetComponent->GetBakedOutputs() = BakeState.GetNewBakedOutputs();

	return bSuccess;
}


void
FHoudiniEngineBakeUtils::DeleteBakedDataTableObjects(TArray<FHoudiniBakedOutput>& InBakedOutputs)
{
	// Must remove data tables before their structures to prevent Unreal complaining.

	for (FHoudiniBakedOutput& BakedOutput : InBakedOutputs)
	{
		for (auto& It : BakedOutput.BakedOutputObjects)
		{
			FHoudiniBakedOutputObject& BakedObjectOutput = It.Value;
			UObject* Object = It.Value.GetBakedObjectIfValid();

			if (!IsValid(Object))
				continue;

			if (Object->IsA<UDataTable>())
			{
				FHoudiniEngineUtils::ForceDeleteObject(Object);
				It.Value.BakedObject.Empty();
			}
		}
	}

	// Now remove the structures.
	for (FHoudiniBakedOutput& BakedOutput : InBakedOutputs)
	{
		for (auto& It : BakedOutput.BakedOutputObjects)
		{
			UObject* Object = It.Value.GetBakedObjectIfValid();

			if (!IsValid(Object))
				continue;

			if (Object->IsA<UUserDefinedStruct>() || Object->IsA<UUserDefinedStructEditorData>())
			{
				FHoudiniEngineUtils::ForceDeleteObject(Object);
				It.Value.BakedObject.Empty();
			}
		}
	}
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniOutputsToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FTransform& InParentTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
	TArray<FHoudiniEngineBakedActor>& OutNewActors, 
	FHoudiniBakedObjectData& BakedObjectData,
	TArray<EHoudiniOutputType> * InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> * InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	const int32 NumOutputs = InOutputs.Num();
	
	const FString MsgTemplate = TEXT("Baking output: {0}/{1}.");
	FString Msg = FString::Format(*MsgTemplate, { 0, NumOutputs });
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Msg));

	RemoveBakedLevelInstances(HoudiniAssetComponent, InBakeState.GetOldBakedOutputs(), BakeSettings);

	if (BakeSettings.bReplaceAssets)
	{
		// Make sure all old data tables are removed prior to baking. Data tables must be fully deleted
		// before creating new data tables with the same new or Unreal gets very upset.
		DeleteBakedDataTableObjects(InBakeState.GetOldBakedOutputs());
	}


	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	// Landscape layers needs to be cleared during baking, but only once. So keep track of which ones
	// have been cleared.
	TMap<ALandscape*, FHoudiniClearedEditLayers> ClearedLandscapeLayers;

	// First bake everything except instancers, then bake instancers. Since instancers might use meshes in
	// from the other outputs.
	bool bHasAnyInstancers = false;
	int32 NumProcessedOutputs = 0;

	TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;
	TMap<UStaticMesh*, UStaticMesh*> AlreadyBakedStaticMeshMap;
	TArray<FHoudiniEngineBakedActor> OutputBakedActors;
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		UHoudiniOutput* Output = InOutputs[OutputIdx];
		if (!IsValid(Output))
		{
			NumProcessedOutputs++;
			continue;
		}

		Msg = FString::Format(*MsgTemplate, { NumProcessedOutputs + 1, NumOutputs });
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));

		const EHoudiniOutputType OutputType = Output->GetType();
		// Check if we should skip this output type
		if (InOutputTypesToBake && InOutputTypesToBake->Find(OutputType) == INDEX_NONE)
		{
			NumProcessedOutputs++;
			continue;
		}

		OutputBakedActors.Reset();
		switch (OutputType)
		{
			case EHoudiniOutputType::Mesh:
			{
				FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
					HoudiniAssetComponent,
					OutputIdx,
					InOutputs,
					InBakeState,
					InBakeFolder,
					InTempCookFolder,
					BakeSettings,
					AllBakedActors,
					OutputBakedActors,
					BakedObjectData,
					AlreadyBakedStaticMeshMap,
					AlreadyBakedMaterialsMap,
					InFallbackActor,
					InFallbackWorldOutlinerFolder);
			}
			break;

			case EHoudiniOutputType::Instancer:
			{
				if (!bHasAnyInstancers)
					bHasAnyInstancers = true;
				NumProcessedOutputs--;
			}
			break;

			case EHoudiniOutputType::Landscape:
			{
				const bool bResult = FHoudiniLandscapeBake::BakeLandscape(
					HoudiniAssetComponent,
					OutputIdx,
					InOutputs,
					InBakeState,
					BakeSettings,
					InBakeFolder,
					ClearedLandscapeLayers,
					BakedObjectData);
			}
			break;

			case EHoudiniOutputType::Skeletal:
			{
				FHoudiniEngineBakeUtils::BakeSkeletalMeshOutputToActors(
					HoudiniAssetComponent,
					OutputIdx,
					InOutputs,
					InBakeState,
					InBakeFolder,
					InTempCookFolder,
					BakeSettings,
					AllBakedActors,
					OutputBakedActors,
					BakedObjectData,
					AlreadyBakedStaticMeshMap,
					AlreadyBakedMaterialsMap,
					InFallbackActor,
					InFallbackWorldOutlinerFolder);
			}
			break;

			case EHoudiniOutputType::Curve:
			{
				FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
					HoudiniAssetComponent,
					OutputIdx,
					InOutputs,
					InBakeState,
					InBakeFolder,
					BakeSettings,
					AllBakedActors,
					OutputBakedActors,
					InFallbackActor,
					InFallbackWorldOutlinerFolder);
			}
			break;

			case EHoudiniOutputType::GeometryCollection:
			{
				FHoudiniEngineBakeUtils::BakeGeometryCollectionOutputToActors(
					HoudiniAssetComponent,
					OutputIdx,
					InOutputs,
					InBakeState,
					InBakeFolder,
					InTempCookFolder,
					BakeSettings,
					AllBakedActors,
					OutputBakedActors,
					BakedObjectData,
					AlreadyBakedStaticMeshMap,
					AlreadyBakedMaterialsMap,
					InFallbackActor,
					InFallbackWorldOutlinerFolder);
			}
			break;

			case EHoudiniOutputType::LandscapeSpline:
			{
				const bool bResult = FHoudiniLandscapeBake::BakeLandscapeSplines(
					HoudiniAssetComponent,
					OutputIdx,
					InOutputs,
					InBakeState,
					BakeSettings,
					InBakeFolder,
					ClearedLandscapeLayers,
					BakedObjectData);
			}
			break;

		case EHoudiniOutputType::DataTable:
		{
			FHoudiniEngineBakeUtils::BakeDataTables(
				HoudiniAssetComponent,
				OutputIdx,
				InOutputs,
				InBakeState,
				InBakeFolder,
				InTempCookFolder,
				BakeSettings,
				AllBakedActors,
				OutputBakedActors,
				BakedObjectData,
				AlreadyBakedStaticMeshMap,
				AlreadyBakedMaterialsMap,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
			}
			break;

		case EHoudiniOutputType::AnimSequence:
		{
			FHoudiniEngineBakeUtils::BakeAnimSequence(
				HoudiniAssetComponent,
				OutputIdx,
				InOutputs,
				InBakeState,
				InBakeFolder,
				InTempCookFolder,
				BakeSettings,
				AllBakedActors,
				OutputBakedActors,
				BakedObjectData,
				AlreadyBakedStaticMeshMap,
				AlreadyBakedMaterialsMap,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		break;

		case EHoudiniOutputType::Invalid:
				break;
		}

		AllBakedActors.Append(OutputBakedActors);
		NewBakedActors.Append(OutputBakedActors);

		NumProcessedOutputs++;
	}

	if (bHasAnyInstancers)
	{
	    FHoudiniEngineBakeUtils::BakeAllFoliageTypes(
			HoudiniAssetComponent,
			AlreadyBakedStaticMeshMap,
			InBakeState,
			InOutputs,
			InBakeFolder,
			InTempCookFolder,
			BakeSettings,
			AllBakedActors,
			AlreadyBakedMaterialsMap,
			BakedObjectData);

	    for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
		{
			UHoudiniOutput* Output = InOutputs[OutputIdx];
			if (!IsValid(Output))
			{
				continue;
			}

			if (Output->GetType() == EHoudiniOutputType::Instancer)
			{
				OutputBakedActors.Reset();
				
				Msg = FString::Format(*MsgTemplate, { NumProcessedOutputs + 1, NumOutputs });
				FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));

				FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
					HoudiniAssetComponent,
                    OutputIdx,
                    InOutputs,
                    InBakeState,
                    InParentTransform,
                    InBakeFolder,
                    InTempCookFolder,
					BakeSettings,
                    AllBakedActors,
                    OutputBakedActors,
					BakedObjectData,
                    AlreadyBakedStaticMeshMap,
                    AlreadyBakedMaterialsMap,
                    InInstancerComponentTypesToBake,
                    InFallbackActor,
                    InFallbackWorldOutlinerFolder);

				AllBakedActors.Append(OutputBakedActors);
				NewBakedActors.Append(OutputBakedActors);
				
				NumProcessedOutputs++;
			}
		}
	}

	// Moved Cooked to Baked Landscapes. 
	{


		TArray<FHoudiniEngineBakedActor> BakedLandscapeActors = FHoudiniLandscapeBake::MoveCookedToBakedLandscapes(
			HoudiniAssetComponent,
			FName(InFallbackWorldOutlinerFolder), 
			InOutputs,
			InBakeState,
			BakeSettings,
			InBakeFolder,
			BakedObjectData);
		
		AllBakedActors.Append(BakedLandscapeActors);
		NewBakedActors.Append(BakedLandscapeActors);
	}

	// Only do the post bake post-process once per Actor
	TSet<AActor*> UniqueActors;
	for (FHoudiniEngineBakedActor& BakedActor : NewBakedActors)
	{
		if (BakedActor.bPostBakeProcessPostponed && BakedActor.Actor)
		{
			BakedActor.bPostBakeProcessPostponed = false;
			AActor* Actor = BakedActor.Actor;
			bool bIsAlreadyInSet = false;
			UniqueActors.Add(Actor, &bIsAlreadyInSet);
			if (!bIsAlreadyInSet)
			{
				Actor->InvalidateLightingCache();
				Actor->PostEditMove(true);
				Actor->MarkPackageDirty();
			}
		}
	}

	// Create package params we will use for data layers and HLODs. Code is simpler if we do this up front.
	TArray<FHoudiniPackageParams> PackageParams;
	PackageParams.SetNum(NewBakedActors.Num());

	for (int Index = 0; Index < NewBakedActors.Num(); Index++)
	{
		FHoudiniEngineBakedActor& BakedActor = NewBakedActors[Index];
		UHoudiniOutput* Output = InOutputs[BakedActor.OutputIndex];
		FHoudiniOutputObject& OutputObject = Output->GetOutputObjects()[BakedActor.OutputObjectIdentifier];

		const bool bHasPreviousBakeData = InBakeState.FindOldBakedOutputObject(BakedActor.OutputIndex, BakedActor.OutputObjectIdentifier) != nullptr;

		const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ? EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniAttributeResolver Resolver;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			BakedActor.Actor->GetWorld(),
			HoudiniAssetComponent,
			BakedActor.OutputObjectIdentifier,
			OutputObject,
			bHasPreviousBakeData,
			"",
			PackageParams[Index],
			Resolver,
			InBakeFolder.Path,
			AssetPackageReplaceMode);
	}

	// Due to a bug in 5.3 and earlier we need to create all the data layers in one go and store their values,
	// since there seem to be a delay in creating new data layers.

	TMap<FString, UDataLayerInstance*> DataLayerLookup;

	for(int Index = 0; Index < NewBakedActors.Num(); Index++)
	{
		FHoudiniEngineBakedActor& BakedActor = NewBakedActors[Index];
		UHoudiniOutput* Output = InOutputs[BakedActor.OutputIndex];
		FHoudiniOutputObject& OutputObject = Output->GetOutputObjects()[BakedActor.OutputObjectIdentifier];

		UWorld* World = BakedActor.Actor->GetWorld();

		AWorldDataLayers* WorldDataLayers = World->GetWorldDataLayers();
		if(!WorldDataLayers)
		{
			if (!OutputObject.DataLayers.IsEmpty())
				HOUDINI_LOG_ERROR(TEXT("Unable to apply Data Layer because this map is not world partitioned."));
			continue;
		}

		for(auto& DataLayer : OutputObject.DataLayers)
		{
			if (!DataLayerLookup.Contains(DataLayer.Name))
			{
				UDataLayerInstance* DataLayerInstance = FHoudiniDataLayerUtils::FindOrCreateDataLayerInstance(PackageParams[Index], WorldDataLayers, DataLayer);
				if(DataLayerInstance)
				{
					DataLayerLookup.Add(DataLayer.Name, DataLayerInstance);
				}
			}
		}
	}

	for(int Index = 0; Index < NewBakedActors.Num(); Index++)
	{
		FHoudiniEngineBakedActor& BakedActor = NewBakedActors[Index];
		UHoudiniOutput* Output = InOutputs[BakedActor.OutputIndex];
		FHoudiniOutputObject& OutputObject = Output->GetOutputObjects()[BakedActor.OutputObjectIdentifier];

		const bool bHasPreviousBakeData = InBakeState.FindOldBakedOutputObject(BakedActor.OutputIndex, BakedActor.OutputObjectIdentifier) != nullptr;
		
		if (IsValid(BakedActor.Actor))
		{
			FHoudiniDataLayerUtils::ApplyDataLayersToActor(BakedActor.Actor, OutputObject.DataLayers, DataLayerLookup);
			FHoudiniHLODLayerUtils::ApplyHLODLayersToActor(PackageParams[Index], BakedActor.Actor, OutputObject.HLODLayers);
		}
	}
	
	OutNewActors = MoveTemp(NewBakedActors);

	return true;
}

void
FHoudiniEngineBakeUtils::RemoveBakedFoliageInstances(UHoudiniAssetComponent* HoudiniAssetComponent, TArray<FHoudiniBakedOutput>& InBakedOutputs)
{
	for(int Index = 0; Index < InBakedOutputs.Num(); Index++)
	{
		FHoudiniBakedOutput & BakedOutput = InBakedOutputs[Index];
		for(auto & BakedObject : BakedOutput.BakedOutputObjects)
		{
			if (IsValid(BakedObject.Value.FoliageType))
			{
				FHoudiniFoliageTools::RemoveFoliageInstances(
					HoudiniAssetComponent->GetHACWorld(),
					BakedObject.Value.FoliageType,
					BakedObject.Value.FoliageInstancePositions);
			}
			// Remember the foliage type in the previous bake data, but remove the instance positions (since
			// that is what we cleared). We need the previous bake foliage type when baking in "replace existing assets"
			// mode, so that we replace the correct increment.
			// BakedObject.Value.FoliageType = nullptr;
			BakedObject.Value.FoliageInstancePositions.Empty();

		}
	}
}

void
FHoudiniEngineBakeUtils::BakeAllFoliageTypes(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	const TMap<UStaticMesh*, UStaticMesh*>& AlreadyBakedStaticMeshMap,
	FHoudiniEngineBakeState& InBakeState,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& BakeResults,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	FHoudiniBakedObjectData& BakedObjectData)
{
	TMap<UFoliageType*, UFoliageType*> FoliageMap;

	UWorld* World = HoudiniAssetComponent->GetHACWorld();

	// Remove previous bake if required.
	if (BakeSettings.bReplaceAssets)
	{
		RemoveBakedFoliageInstances(HoudiniAssetComponent, InBakeState.GetOldBakedOutputs());
	}

    // Create Foliage Types associated with each output.
    for(int InOutputIndex = 0; InOutputIndex < InAllOutputs.Num(); InOutputIndex++)
    {
		BakeFoliageTypes(
			FoliageMap,
			HoudiniAssetComponent,
			InOutputIndex,
			InBakeState,
			InAllOutputs,
			InBakeFolder,
			InTempCookFolder,
			BakeSettings,
			BakeResults,
			AlreadyBakedStaticMeshMap,
			InOutAlreadyBakedMaterialsMap,
			BakedObjectData);
    }

	// Remove all cooked existing foliage.
	//FHoudiniFoliageTools::CleanupFoliageInstances(HoudiniAssetComponent);
	for (auto It : FoliageMap)
	{
		auto* CookedFoliageType = Cast<UFoliageType_InstancedStaticMesh>(It.Key);
		FHoudiniFoliageUtils::RemoveFoliageTypeFromWorld(World, CookedFoliageType);
	}
}


bool
FHoudiniEngineBakeUtils::BakeFoliageTypes(
	TMap<UFoliageType*, UFoliageType*> & FoliageMap,
	UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	FHoudiniEngineBakeState& InBakeState,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& BakeResults,
	const TMap<UStaticMesh*, UStaticMesh*>& InAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	FHoudiniBakedObjectData& BakedObjectData)
{
	const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets
		? EPackageReplaceMode::ReplaceExistingAssets
		: EPackageReplaceMode::CreateNewAssets;

	UHoudiniOutput* Output = InAllOutputs[InOutputIndex];

	UWorld* DesiredWorld = Output ? Output->GetWorld() : GWorld;
	auto & OutputObjects = Output->GetOutputObjects();

	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		FHoudiniOutputObject* OutputObject = &Pair.Value;

		// Skip non foliage outputs
        if (OutputObject->FoliageType == nullptr)
		    continue;

		if (FoliageMap.Contains(OutputObject->FoliageType))
		    continue;

		FHoudiniBakedOutputObject BakedObject;

		bool bHasPreviousBakeData = false;
		BakedObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, Identifier, bHasPreviousBakeData);

		UFoliageType* const UserFoliageType = IsValid(OutputObject->UserFoliageType)
			? Cast<UFoliageType>(OutputObject->UserFoliageType)
			: nullptr;

		UFoliageType* TargetFoliageType = nullptr;
		bool bUseUserFoliageType = false;
		if (IsValid(UserFoliageType))
		{
			// The user specified a Foliage Type. Only use it directly if there are no differences between it and
			// the cooked version. For example, if the user specified material overrides, or different values for
			// foliage parameters, such as radius, then we need to bake a new foliage type.
			if (FHoudiniFoliageTools::AreFoliageTypesEqual(UserFoliageType, OutputObject->FoliageType))
			{
				bUseUserFoliageType = true;
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT(
					"Baking a new foliage type, since the cooked foliage type has been modified with respect to the "
					"user specified foliage type %s"), *UserFoliageType->GetName());
			}
		}
		
		if (bUseUserFoliageType)
		{
		    // The user specified a Foliage Type, so store it.
			TargetFoliageType = UserFoliageType;
			FoliageMap.Add(OutputObject->FoliageType, TargetFoliageType);
		}
		else
		{
			// TODO: Support "ReplaceExistingAssets"

			// The Foliage Type was created by this plugin. Copy it to the Baked output.
			FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(OutputObject->FoliageType);

			FHoudiniPackageParams PackageParams;
			FHoudiniAttributeResolver InstancerResolver;
			FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
				DesiredWorld, HoudiniAssetComponent, Identifier, *OutputObject, bHasPreviousBakeData,
			ObjectName, PackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);

			UFoliageType* const PreviousBakeFoliageType = bHasPreviousBakeData ? BakedObject.FoliageType : nullptr;
			TargetFoliageType = DuplicateFoliageTypeAndCreatePackageIfNeeded(
				OutputObject->FoliageType,
				PreviousBakeFoliageType,
				PackageParams,
				InAllOutputs,
				BakeResults,
				InTempCookFolder.Path,
				FoliageMap,
				InOutAlreadyBakedMaterialsMap,
				BakeResults,
				BakedObjectData);

			FoliageMap.Add(OutputObject->FoliageType, TargetFoliageType);
		}

		check(IsValid(TargetFoliageType));

		// Replace any mesh referenced in the cooked foliage with the new reference.
		UFoliageType_InstancedStaticMesh* const CookedFoliageType = Cast<UFoliageType_InstancedStaticMesh>(OutputObject->FoliageType);
		UFoliageType_InstancedStaticMesh* const BakedFoliageType = Cast<UFoliageType_InstancedStaticMesh>(TargetFoliageType);
		if (CookedFoliageType && TargetFoliageType && InAlreadyBakedStaticMeshMap.Contains(CookedFoliageType->GetStaticMesh()))
		{
			BakedFoliageType->SetStaticMesh(InAlreadyBakedStaticMeshMap[CookedFoliageType->GetStaticMesh()]);
		}

		// Copy all cooked instances to reference the baked instances.
		auto Instances = FHoudiniFoliageTools::GetAllFoliageInstances(DesiredWorld, OutputObject->FoliageType);

		TArray<AInstancedFoliageActor*> FoliageActors = FHoudiniFoliageTools::SpawnFoliageInstances(DesiredWorld, TargetFoliageType, Instances, {});

		TArray<FVector> InstancesPositions;
		InstancesPositions.Reserve(Instances.Num());
		for(auto & Instance : Instances)
		{
			InstancesPositions.Add(Instance.Location);	
		}

		TArray<FString> ActorInstancePaths;
		ActorInstancePaths.Reserve(FoliageActors.Num());
		for (auto & Instance : FoliageActors) 
		{
			ActorInstancePaths.Add(Instance->GetPathName());
		}

		// Store back output object.
		BakedObject.FoliageType = TargetFoliageType;
		BakedObject.FoliageInstancePositions = InstancesPositions;
		BakedObject.FoliageActors = ActorInstancePaths;
		InBakeState.SetNewBakedOutputObject(InOutputIndex, Identifier, BakedObject);
    }

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];	
	if (!IsValid(InOutput))
		return false;

	// Geometry collection instancers will be done on the geometry collection output component.
	if (FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancer(InOutput))
	{
		return true;
	}

	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;
	TArray<FHoudiniEngineBakedActor> OutputBakedActors;

    // Iterate on the output objects, baking their object/component as we go
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		FHoudiniOutputObject& CurrentOutputObject = Pair.Value;

		if (CurrentOutputObject.bProxyIsCurrent)
		{
			// TODO: we need to refine the SM first!
			// ?? 
		}

		for(auto Component : CurrentOutputObject.OutputComponents)
		{
		    if (!IsValid(Component))
			    continue;

		    OutputBakedActors.Reset();

        if (Component->IsA<UInstancedStaticMeshComponent>()
			    && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::InstancedStaticMeshComponent)))
		    {
			    BakeInstancerOutputToActors_ISMC(
				    HoudiniAssetComponent,
				    InOutputIndex,
				    InAllOutputs,
				    InBakeState,
				    HGPOs,
				    Pair.Key, 
				    CurrentOutputObject, 
				    InTransform,
				    InBakeFolder,
				    InTempCookFolder,
						BakeSettings,
				    AllBakedActors,
				    OutputBakedActors,
						BakedObjectData,
				    InOutAlreadyBakedStaticMeshMap,
				    InOutAlreadyBakedMaterialsMap,
				    InFallbackActor,
				    InFallbackWorldOutlinerFolder);
		    }
		    else if (Component->IsA<UHoudiniInstancedActorComponent>()
				    && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::InstancedActorComponent)))
		    {
			    BakeInstancerOutputToActors_IAC(
				    HoudiniAssetComponent,
				    InOutputIndex,
				    HGPOs,
				    Pair.Key, 
				    CurrentOutputObject, 
				    InBakeState,
				    InBakeFolder,
						BakeSettings,
				    AllBakedActors,
				    OutputBakedActors,
						BakedObjectData);
		    }
		    else if (Component->IsA<UHoudiniMeshSplitInstancerComponent>()
		 		     && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::MeshSplitInstancerComponent)))
		    {
			    FHoudiniEngineBakedActor BakedActorEntry;
			    if (BakeInstancerOutputToActors_MSIC(
					    HoudiniAssetComponent,
					    InOutputIndex,
					    InAllOutputs,
					    InBakeState,
					    HGPOs,
					    Pair.Key, 
					    CurrentOutputObject,
					    InTransform,
					    InBakeFolder,
					    InTempCookFolder,
						BakeSettings,
					    AllBakedActors,
					    BakedActorEntry,
						BakedObjectData,
					    InOutAlreadyBakedStaticMeshMap,
					    InOutAlreadyBakedMaterialsMap,
					    InFallbackActor,
					    InFallbackWorldOutlinerFolder))
			    {
				    OutputBakedActors.Add(BakedActorEntry);
			    }
		    }
		    else if (Component->IsA<UStaticMeshComponent>()
	  			     && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::StaticMeshComponent)))
		    {
			    FHoudiniEngineBakedActor BakedActorEntry;
			    if (BakeInstancerOutputToActors_SMC(
					    HoudiniAssetComponent,
					    InOutputIndex,
					    InAllOutputs,
					    InBakeState,
					    HGPOs,
					    Pair.Key, 
					    CurrentOutputObject, 
					    InBakeFolder,
					    InTempCookFolder,
					    BakeSettings,
					    AllBakedActors,
					    BakedActorEntry,
						BakedObjectData,
					    InOutAlreadyBakedStaticMeshMap,
					    InOutAlreadyBakedMaterialsMap,
					    InFallbackActor,
					    InFallbackWorldOutlinerFolder))
			    {
				    OutputBakedActors.Add(BakedActorEntry);
			    }
			    
		    }
		    else
		    {
			    // Unsupported component!
		    }


		    AllBakedActors.Append(OutputBakedActors);
		    NewBakedActors.Append(OutputBakedActors);
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1

		// Bake any level instances. They will be stored on the OutputActors member, 
		// but we want to return one output for all instances on  this output, so do
		// them all at once.

		if (!CurrentOutputObject.OutputActors.IsEmpty())
		{
			OutputBakedActors.Reset();

			FHoudiniEngineBakedActor BakedActorEntry;
			if (BakeInstancerOutputToActors_LevelInstances(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				InBakeState,
				Pair.Key,
				CurrentOutputObject,
				InBakeFolder,
				InTempCookFolder,
				BakeSettings,
				AllBakedActors,
				BakedActorEntry,
				BakedObjectData,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				InFallbackActor,
				InFallbackWorldOutlinerFolder))
			{
				OutputBakedActors.Add(BakedActorEntry);
			}

			AllBakedActors.Append(OutputBakedActors);
			NewBakedActors.Append(OutputBakedActors);
		}
		
#endif
	}

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_ISMC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, bHasPreviousBakeData);
	
	for(auto Component : InOutputObject.OutputComponents)
	{
	    UInstancedStaticMeshComponent * InISMC = Cast<UInstancedStaticMeshComponent>(Component);
	    if (!IsValid(InISMC))
		    continue;

	    AActor * OwnerActor = InISMC->GetOwner();
	    if (!IsValid(OwnerActor))
		    return false;

	    UStaticMesh * StaticMesh = InISMC->GetStaticMesh();
	    if (!IsValid(StaticMesh))
		    return false;

		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		FindHGPO(InOutputObjectIdentifier, InHGPOs, FoundHGPO);

	    // Certain SMC materials may need to be duplicated if we didn't generate the mesh object.
		// Map of duplicated overrides materials (oldTempMaterial , newBakedMaterial)
		TMap<UMaterialInterface*, UMaterialInterface*> DuplicatedISMCOverrideMaterials;
	    
	    const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		    EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	    UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;

	    // Determine if the incoming mesh is temporary
	    // If not temporary set the ObjectName from its package. (Also use this as a fallback default)
	    FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	    UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid());
	    UStaticMesh* BakedStaticMesh = nullptr;

	    // Construct PackageParams for the instancer itself. When baking to actor we technically won't create a stand-alone
	    // disk package for the instancer, but certain attributes (such as level path) use tokens populated from the package params.
	    FHoudiniPackageParams InstancerPackageParams;
	    FHoudiniAttributeResolver InstancerResolver;
	    FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		    DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, bHasPreviousBakeData, ObjectName,
		    InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);

	    FHoudiniPackageParams MeshPackageParams;
	    FString BakeFolderPath = FString();
	    const bool bIsTemporary = IsObjectTemporary(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, InstancerPackageParams.TempCookFolder, InstancerPackageParams.ComponentGUID);
	    if (!bIsTemporary)
	    {
		    // We can reuse the mesh
		    BakedStaticMesh = StaticMesh;
	    }
	    else
	    {
		    BakeFolderPath = InBakeFolder.Path;

		    // See if we can find the mesh in the outputs
	    	FHoudiniBakedOutputObject MeshBakedOutputObject;
		    int32 MeshOutputIndex = INDEX_NONE;
		    FHoudiniOutputObjectIdentifier MeshIdentifier;
		    const bool bFoundMeshOutput = FindOutputObject(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
		    if(bFoundMeshOutput)
		    {
			    FHoudiniAttributeResolver MeshResolver;
			    // Found the instanced mesh in the mesh outputs
			    const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);

				// Check if the mesh itself has previous bake data
				bool bMeshHasPreviousBakeData = false;
				MeshBakedOutputObject = InBakeState.MakeNewBakedOutputObject(MeshOutputIndex, MeshIdentifier, bMeshHasPreviousBakeData);
	
			    FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
				    DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, bMeshHasPreviousBakeData, ObjectName,
				    MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);
			    // Update with resolved object name
			    ObjectName = MeshPackageParams.ObjectName;
			    BakeFolderPath = MeshPackageParams.BakeFolder;
		    }

		    // This will bake/duplicate the mesh if temporary, or return the input one if it is not
		    BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			    StaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
				BakedObjectData, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap);

			MeshBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();
	    	InBakeState.SetNewBakedOutputObject(MeshOutputIndex, MeshIdentifier, MeshBakedOutputObject);
	    }

		// We may need to duplicate materials overrides if they are temporary
		// (typically, material instances generated by the plugin)
		TArray<UMaterialInterface*> Materials = InISMC->GetMaterials();
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if (!IsValid(MaterialInterface))
				continue;

			// Only duplicate the material if it is temporary
			if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InAllOutputs, InTempCookFolder.Path, InstancerPackageParams.ComponentGUID))
			{
				UMaterialInterface* DuplicatedMaterial = BakeSingleMaterialToPackage(
					MaterialInterface, InstancerPackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);
				DuplicatedISMCOverrideMaterials.Add(MaterialInterface, DuplicatedMaterial);
			}
		}

	    // Update the baked object
	    BakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	    // Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	    FString InstancerName = ObjectName + "_instancer";
		if (InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
			InstancerName = InOutputObject.CachedAttributes[HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2];
		InstancerName += "_" + InOutputObjectIdentifier.SplitIdentifier;


	    const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			InstancerResolver,
		    FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

	    // By default spawn in the current level unless specified via the unreal_level_path attribute
	    ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	    bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	    if (bHasLevelPathAttribute)
	    {
		    // Get the package path from the unreal_level_apth attribute
		    FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		    bool bCreatedPackage = false;
		    if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			    LevelPackagePath,
			    DesiredLevel,
			    DesiredWorld,
			    bCreatedPackage))
		    {
			    // TODO: LOG ERROR IF NO LEVEL
			    return false;
		    }

		    // If we have created a new level, add it to the packages to save
		    // TODO: ? always add?
		    if (bCreatedPackage && DesiredLevel)
		    {
			    BakedObjectData.BakeStats.NotifyPackageCreated(1);
			    BakedObjectData.BakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			    // We can now save the package again, and unload it.
			    BakedObjectData.PackagesToSave.Add(DesiredLevel->GetOutermost());
		    }
	    }

	    if(!DesiredLevel)
		    return false;

	    // Try to find the unreal_bake_actor, if specified, or fallback to the default named actor
	    FName BakeActorName;
	    AActor* FoundActor = nullptr;
	    bool bHasBakeActorName = false;
		FName DefaultActorName = *InstancerName;
		if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
		{
			FHoudiniAttributeResolver OutResolver;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			InstancerPackageParams.UpdateTokensFromParams(HoudiniAssetComponent->GetWorld(), HoudiniAssetComponent, Tokens);
			OutResolver.SetTokensFromStringMap(Tokens);
			DefaultActorName = FName(OutResolver.ResolveString(BakeSettings.DefaultBakeName));
		}
	  FindUnrealBakeActor(InOutputObject, BakedOutputObject, InBakedActors, DesiredLevel, DefaultActorName, BakeSettings, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName);

		// Store the initial tags that the FoundActor spawned with. 
		// We will be adding additional tags from the HGPOs.
		TArray<FName> ActorTags;

	    // Should we create one actor with an ISMC or multiple actors with one SMC?
	    bool bSpawnMultipleSMC = false;
	    if (bSpawnMultipleSMC)
	    {
		    // Deactivated for now, as generating multiple actors curently has issues with BakeReplace mode.
		    // A similar result could be achieve by specifying individual actor names and splitting the instancer to multiple components
		    // (via unreal_bake_actor and unreal_split_attr)
		    // Get the StaticMesh ActorFactory
		    TSubclassOf<AActor> BakeActorClass = nullptr;
		    UActorFactory* ActorFactory = GetActorFactory(NAME_None, BakeSettings, BakeActorClass, UActorFactoryStaticMesh::StaticClass(), BakedStaticMesh);
		    if (!ActorFactory)
			    return false;

		    // Split the instances to multiple StaticMeshActors
		    for (int32 InstanceIdx = 0; InstanceIdx < InISMC->GetInstanceCount(); InstanceIdx++)
		    {
			    FTransform InstanceTransform;
			    InISMC->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

			    FName BakeActorNameWithIndex = FName(BakeActorName.ToString() + "_instance_" + FString::FromInt(InstanceIdx), InstanceIdx);
			    FoundActor = nullptr;
			    FindUnrealBakeActor(InOutputObject, BakedOutputObject, InBakedActors, DesiredLevel, *InstancerName, BakeSettings, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName);

			    if (!FoundActor)
			    {
				    FoundActor = SpawnBakeActor(ActorFactory, BakedStaticMesh, DesiredLevel, BakeSettings, InstanceTransform, HoudiniAssetComponent, BakeActorClass);
				    if (!IsValid(FoundActor))
					    continue;
			    }

		    	// Capture the current tags on the actor, in case we need to keep them.
		    	ActorTags = FoundActor->Tags;

			    const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, ActorFactory->NewActorClass, BakeActorName.ToString(), FoundActor);
			    RenameAndRelabelActor(FoundActor, NewNameStr, false);

			    // The folder is named after the original actor and contains all generated actors
			    SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

			    AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
			    if (!IsValid(SMActor))
				    continue;

			    // Copy properties from the existing component
			    CopyPropertyToNewActorAndComponent(FoundActor, SMActor->GetStaticMeshComponent(), InISMC);

		    	// Restore the actor tags, in case we want to keep them.
		    	FHoudiniEngineUtils::KeepOrClearActorTags(FoundActor, true, true, FoundHGPO);
		    	if (FoundHGPO)
		    	{
		    		FHoudiniEngineUtils::ApplyTagsToActorAndComponents(FoundActor, FHoudiniEngineUtils::IsKeepTagsEnabled(FoundHGPO), FoundHGPO->GenericPropertyAttributes);
		    	}


			    FHoudiniEngineBakedActor& OutputEntry = OutActors.Add_GetRef(FHoudiniEngineBakedActor(
				    FoundActor,
				    BakeActorName,
				    WorldOutlinerFolderPath,
				    InOutputIndex,
				    InOutputObjectIdentifier,
				    BakedStaticMesh,
				    StaticMesh,
				    SMActor->GetStaticMeshComponent(),
				    BakeFolderPath,
				    MeshPackageParams));
			    OutputEntry.bInstancerOutput = true;
			    OutputEntry.InstancerPackageParams = InstancerPackageParams;
		    }
	    }
	    else
	    {
		    bool bSpawnedActor = false;
		    if (!FoundActor)
		    {
			    // Only create one actor
			    FActorSpawnParameters SpawnInfo;
			    SpawnInfo.OverrideLevel = DesiredLevel;
			    SpawnInfo.ObjectFlags = RF_Transactional;

			    if (!DesiredLevel->bUseExternalActors)
			    {
				    SpawnInfo.Name = FName(MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString()));
			    }
			    SpawnInfo.bDeferConstruction = true;

			    // Spawn the new Actor
			    UClass* ActorClass = GetBakeActorClassOverride(InOutputObject);
			    if (!ActorClass)
				    ActorClass = AActor::StaticClass();
			    FoundActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(ActorClass, SpawnInfo);
			    if (!IsValid(FoundActor))
				    return false;
			    bSpawnedActor = true;

			    BakedObjectData.BakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);

			    FHoudiniEngineRuntimeUtils::SetActorLabel(FoundActor, BakeActorName.ToString());
			    FoundActor->SetActorHiddenInGame(InISMC->bHiddenInGame);
		    }
		    else
		    {
			    // If there is a previously baked component, and we are in replace mode, remove it
			    if (BakeSettings.bReplaceAssets)
			    {
				    USceneComponent* InPrevComponent = Cast<USceneComponent>(BakedOutputObject.GetBakedComponentIfValid());
				    if (IsValid(InPrevComponent) && InPrevComponent->GetOwner() == FoundActor)
					    RemovePreviouslyBakedComponent(InPrevComponent);
			    }
			    
			    const FString UniqueActorNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString(), FoundActor);
			    RenameAndRelabelActor(FoundActor, UniqueActorNameStr, false);

			    BakedObjectData.BakeStats.NotifyObjectsUpdated(FoundActor->GetClass()->GetName(), 1);
		    }

	    	// Capture the current actor tags, in case the user wants to keep them.
	    	ActorTags = FoundActor->Tags;
	    	
		    // The folder is named after the original actor and contains all generated actors
		    SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

		    // Get/create the actor's root component
		    const bool bCreateIfMissing = true;
		    USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);
		    if (bSpawnedActor && IsValid(RootComponent))
			    RootComponent->SetWorldTransform(InTransform);
		    
		    // Duplicate the instancer component, create a Hierarchical ISMC if needed
		    UInstancedStaticMeshComponent* NewISMC = nullptr;
		    UHierarchicalInstancedStaticMeshComponent* InHISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InISMC);
		    if (InHISMC)
		    {
			    // Handle foliage: don't duplicate foliage component, create a new hierarchical one and copy what we can
			    // from the foliage component
			    if (InHISMC->IsA<UFoliageInstancedStaticMeshComponent>())
			    {
				    NewISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(
					    FoundActor,
					    FName(MakeUniqueObjectNameIfNeeded(FoundActor, InHISMC->GetClass(), InISMC->GetName())));
				    CopyPropertyToNewActorAndComponent(FoundActor, NewISMC, InISMC);

				    BakedObjectData.BakeStats.NotifyObjectsCreated(UHierarchicalInstancedStaticMeshComponent::StaticClass()->GetName(), 1);
			    }
			    else
			    {
				    NewISMC = DuplicateObject<UHierarchicalInstancedStaticMeshComponent>(
					    InHISMC,
					    FoundActor,
					    FName(MakeUniqueObjectNameIfNeeded(FoundActor, InHISMC->GetClass(), InISMC->GetName())));

				    BakedObjectData.BakeStats.NotifyObjectsCreated(InHISMC->GetClass()->GetName(), 1);
			    }
		    }
		    else
		    {
			    NewISMC = DuplicateObject<UInstancedStaticMeshComponent>(
				    InISMC,
				    FoundActor,
				    FName(MakeUniqueObjectNameIfNeeded(FoundActor, InISMC->GetClass(), InISMC->GetName())));

			    BakedObjectData.BakeStats.NotifyObjectsCreated(InISMC->GetClass()->GetName(), 1);
		    }

		    if (!NewISMC)
		    {
			    return false;
		    }

		    BakedOutputObject.BakedComponent = FSoftObjectPath(NewISMC).ToString();

		    NewISMC->RegisterComponent();
		    NewISMC->SetStaticMesh(BakedStaticMesh);
		    FoundActor->AddInstanceComponent(NewISMC);

		    if (DuplicatedISMCOverrideMaterials.Num() > 0)
		    {
				// If we have baked some temporary materials, make sure to update them on the new component
				for (int32 Idx = 0; Idx < NewISMC->OverrideMaterials.Num(); Idx++)
				{
					UMaterialInterface* CurMat = NewISMC->GetMaterial(Idx);

					UMaterialInterface** DuplicatedMat = DuplicatedISMCOverrideMaterials.Find(CurMat);
					if (!DuplicatedMat || !IsValid(*DuplicatedMat))
						continue;

					NewISMC->SetMaterial(Idx, *DuplicatedMat);
				}
		    }
		    
		    // NewActor->SetRootComponent(NewISMC);
		    if (IsValid(RootComponent))
			    NewISMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		    NewISMC->SetWorldTransform(InISMC->GetComponentTransform());

		    // TODO: do we need to copy properties here, we duplicated the component
		    // // Copy properties from the existing component
		    // CopyPropertyToNewActorAndComponent(FoundActor, NewISMC, InISMC);

	    	FHoudiniEngineUtils::KeepOrClearActorTags(FoundActor, true, false, FoundHGPO);
			if (FoundHGPO)
			{
				// Add actor tags from generic property attributes
				FHoudiniEngineUtils::ApplyTagsToActorOnly(FoundHGPO->GenericPropertyAttributes, FoundActor->Tags);
			}

		    if (bSpawnedActor)
			    FoundActor->FinishSpawning(InTransform);

		    BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		    FHoudiniEngineBakedActor& OutputEntry = OutActors.Add_GetRef(FHoudiniEngineBakedActor(
			    FoundActor,
			    BakeActorName,
			    WorldOutlinerFolderPath,
			    InOutputIndex,
			    InOutputObjectIdentifier,
			    BakedStaticMesh,
			    StaticMesh,
			    NewISMC,
			    BakeFolderPath,
			    MeshPackageParams));
		    OutputEntry.bInstancerOutput = true;
		    OutputEntry.InstancerPackageParams = InstancerPackageParams;

		    // Postpone post-bake calls to do them once per actor
		    OutActors.Last().bPostBakeProcessPostponed = true;
	    }

	    // If we are baking in replace mode, remove previously baked components/instancers
	    if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
	    {
		    const bool bInDestroyBakedComponent = false;
		    const bool bInDestroyBakedInstancedActors = true;
		    const bool bInDestroyBakedInstancedComponents = true;
		    DestroyPreviousBakeOutput(
			    BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	    }
	}

	// Set the updated baked output data in the state
	InBakeState.SetNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, BakedOutputObject);
	
	return true;
}

bool FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_LevelInstances(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, bHasPreviousBakeData);
	
	FString ObjectName;
	UWorld* World = HoudiniAssetComponent->GetWorld();
	const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	FHoudiniPackageParams InstancerPackageParams;
	FHoudiniAttributeResolver InstancerResolver;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		World, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, bHasPreviousBakeData, ObjectName,
		InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);

	const FName OutlinerPath = GetOutlinerFolderPath(InstancerResolver, FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

	for (auto & Actor : InOutputObject.OutputActors)
	{
		ALevelInstance* LevelInstance = Cast<ALevelInstance>(Actor.Get());
		if (!IsValid(LevelInstance))
			continue;

		// Determine the name for the baked actor. Destroy any old ones if needed.
		const FString* BackActorPrefix = InOutputObject.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_BAKE_ACTOR);
		FName BakedName;

		if (BackActorPrefix == nullptr || BackActorPrefix->IsEmpty())
		{
			BakedName = LevelInstance->GetFName();
		}
		else
		{
			BakedName = FName(*BackActorPrefix);
		}

		// If replacing existing bake assets, find thsoe actors with the same name and delete them. But only if they are not
		// attached to the HDA Actor as this means they are cooked (temp) objects.

		if(AssetPackageReplaceMode == EPackageReplaceMode::ReplaceExistingAssets)
		{
			TArray<AActor*> Actors = FHoudiniEngineUtils::FindActorsWithNameNoNumber(AActor::StaticClass(), World, BakedName.GetPlainNameString());
			for (AActor* OldBakedActor : Actors)
			{
				if (OldBakedActor->GetOwner() != HoudiniAssetComponent->GetOwner())
				{
					OldBakedActor->Destroy();
				}
			}
		}

		FActorSpawnParameters Parameters;
		Parameters.Template = LevelInstance;
		Parameters.Name = BakedName;
		Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		AActor * BakedActor = World->SpawnActor<ALevelInstance>(Parameters);
		BakedActor->bDefaultOutlinerExpansionState = false;
		BakedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		BakedActor->SetActorTransform(LevelInstance->GetActorTransform()); // WHY IS THIS NEEDED? Don't know, but it is...

		BakedActor->SetActorLabel(LevelInstance->GetActorLabel());
		BakedObjectData.BakeStats.NotifyObjectsCreated(BakedActor->GetClass()->GetName(), 1);
		BakedActor->SetActorLabel(BakedName.ToString());
		BakedActor->SetFolderPath(OutlinerPath);

		BakedOutputObject.LevelInstanceActors.Add(BakedActor->GetPathName());

		if (HoudiniAssetComponent->bRemoveOutputAfterBake)
		{
			LevelInstance->Destroy();
		}

		OutBakedActorEntry.OutputIndex = InOutputIndex;
		OutBakedActorEntry.Actor = BakedActor;
		OutBakedActorEntry.ActorBakeName = FName(BakedActor->GetName());
		OutBakedActorEntry.OutputObjectIdentifier = InOutputObjectIdentifier;

	}

	// Set the updated baked output object in the state
	InBakeState.SetNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, BakedOutputObject);

	return true;

#else
    return false;
#endif
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_SMC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, bHasPreviousBakeData);
	
	for(auto Component : InOutputObject.OutputComponents)
	{
	    UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(Component);
	    if (!IsValid(InSMC))
		    return false;

	    AActor* OwnerActor = InSMC->GetOwner();
	    if (!IsValid(OwnerActor))
		    return false;

	    UStaticMesh* StaticMesh = InSMC->GetStaticMesh();
	    if (!IsValid(StaticMesh))
		    return false;

		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		FindHGPO(InOutputObjectIdentifier, InHGPOs, FoundHGPO);
		
	    UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
	    const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		    EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

		// Certain SMC materials may need to be duplicated if we didn't generate the mesh object.
		// Map of duplicated overrides materials (oldTempMaterial , newBakedMaterial)
		TMap<UMaterialInterface*, UMaterialInterface*> DuplicatedSMCOverrideMaterials;

	    // Determine if the incoming mesh is temporary by looking for it in the mesh outputs. Populate mesh package params
	    // for baking from it.
	    // If not temporary set the ObjectName from the its package. (Also use this as a fallback default)
	    FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	    UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid());
	    UStaticMesh* BakedStaticMesh = nullptr;

	    // Package params for the instancer
	    // See if the instanced static mesh is still a temporary Houdini created Static Mesh
	    // If it is, we need to bake the StaticMesh first
	    FHoudiniPackageParams InstancerPackageParams;
	    // Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	    // The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	    FHoudiniAttributeResolver InstancerResolver;
	    FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent,  InOutputObjectIdentifier, InOutputObject, bHasPreviousBakeData, ObjectName,
			InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);

	    FHoudiniPackageParams MeshPackageParams;
	    FString BakeFolderPath = FString();
	    const bool bIsTemporary = IsObjectTemporary(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, InstancerPackageParams.TempCookFolder, InstancerPackageParams.ComponentGUID);
	    if (!bIsTemporary)
	    {
		    // We can reuse the mesh
		    BakedStaticMesh = StaticMesh;
	    }
	    else
	    {
		    // See if we can find the mesh in the outputs
			FHoudiniBakedOutputObject MeshBakedOutputObject;
		    int32 MeshOutputIndex = INDEX_NONE;
		    FHoudiniOutputObjectIdentifier MeshIdentifier = InOutputObjectIdentifier;
		    BakeFolderPath = InBakeFolder.Path;
		    const bool bFoundMeshOutput = FindOutputObject(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
		    if (bFoundMeshOutput)
		    {
			    FHoudiniAttributeResolver MeshResolver;

			    // Found the instanced mesh in the mesh outputs
			    const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);
				bool bMeshHasPreviousBakeData = false;
				MeshBakedOutputObject = InBakeState.MakeNewBakedOutputObject(MeshOutputIndex, MeshIdentifier, bMeshHasPreviousBakeData);

		    	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
				    DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, bMeshHasPreviousBakeData, ObjectName,
				    MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);
			    // Update with resolved object name
			    ObjectName = MeshPackageParams.ObjectName;
			    BakeFolderPath = MeshPackageParams.BakeFolder;
		    }

		    // This will bake/duplicate the mesh if temporary, or return the input one if it is not
		    BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			    StaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
				BakedObjectData, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap);

	    	MeshBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();
			InBakeState.SetNewBakedOutputObject(MeshOutputIndex, MeshIdentifier, MeshBakedOutputObject);
	    }

		// We may need to duplicate materials overrides if they are temporary
		// (typically, material instances generated by the plugin)
		TArray<UMaterialInterface*> Materials = InSMC->GetMaterials();
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if (!IsValid(MaterialInterface))
				continue;

			// Only duplicate the material if it is temporary
			if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InAllOutputs, InTempCookFolder.Path, InstancerPackageParams.ComponentGUID))
			{
				UMaterialInterface* DuplicatedMaterial = BakeSingleMaterialToPackage(
					MaterialInterface, InstancerPackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);
				DuplicatedSMCOverrideMaterials.Add(MaterialInterface, DuplicatedMaterial);
			}
		}

	    // Update the previous baked object
	    BakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	    // Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	    FString InstancerName = ObjectName + "_instancer";
		if (InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
			InstancerName = InOutputObject.CachedAttributes[HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2];
		InstancerName += "_" + InOutputObjectIdentifier.SplitIdentifier;

	    const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			InstancerResolver,
		    FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

	    // By default spawn in the current level unless specified via the unreal_level_path attribute
	    ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	    bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	    if (bHasLevelPathAttribute)
	    {
		    // Get the package path from the unreal_level_apth attribute
		    FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		    bool bCreatedPackage = false;
		    if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			    LevelPackagePath,
			    DesiredLevel,
			    DesiredWorld,
			    bCreatedPackage))
		    {
			    // TODO: LOG ERROR IF NO LEVEL
			    return false;
		    }

		    // If we have created a level, add it to the packages to save
		    // TODO: ? always add?
		    if (bCreatedPackage && DesiredLevel)
		    {
			    BakedObjectData.BakeStats.NotifyPackageCreated(1);
			    BakedObjectData.BakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			    // We can now save the package again, and unload it.
			    BakedObjectData.PackagesToSave.Add(DesiredLevel->GetOutermost());
		    }
	    }

	    if (!DesiredLevel)
		    return false;

	    // Try to find the unreal_bake_actor, if specified
	    FName BakeActorName;
	    AActor* FoundActor = nullptr;
	    bool bHasBakeActorName = false;
		FName DefaultBakeActorName = *InstancerName;
		if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
		{
			FHoudiniAttributeResolver OutResolver;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			InstancerPackageParams.UpdateTokensFromParams(HoudiniAssetComponent->GetWorld(), HoudiniAssetComponent, Tokens);
			OutResolver.SetTokensFromStringMap(Tokens);
			DefaultBakeActorName = FName(OutResolver.ResolveString(BakeSettings.DefaultBakeName));
		}
	    FindUnrealBakeActor(InOutputObject, BakedOutputObject, InBakedActors, DesiredLevel, DefaultBakeActorName, BakeSettings, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName);

	    UStaticMeshComponent* StaticMeshComponent = nullptr;
	    // Create an actor if we didn't find one
	    bool bCreatedNewActor = false;
	    if (!FoundActor)
	    {
		    // Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
		    TSubclassOf<AActor> BakeActorClass = nullptr;
		    UActorFactory* ActorFactory = GetActorFactory(InOutputObject, BakeSettings, BakeActorClass, UActorFactoryEmptyActor::StaticClass(), BakedStaticMesh);
		    if (!ActorFactory)
		    {
			    return false;
		    }

		    FoundActor = SpawnBakeActor(ActorFactory, BakedStaticMesh, DesiredLevel, BakeSettings, InSMC->GetComponentTransform(), HoudiniAssetComponent, BakeActorClass);
		    if (!IsValid(FoundActor))
			    return false;

		    BakedObjectData.BakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);
		    bCreatedNewActor = true;

		    // If the factory created a static mesh actor, get its component.
		    AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
		    if (IsValid(SMActor))
			    StaticMeshComponent = SMActor->GetStaticMeshComponent();
	    }

	    // We have an existing actor (or we created one without a static mesh component) 
	    if (!IsValid(StaticMeshComponent))
	    {
		    USceneComponent* RootComponent = GetActorRootComponent(FoundActor);
		    if (!IsValid(RootComponent))
			    return false;

		    if (BakeSettings.bReplaceAssets && !bCreatedNewActor)
		    {
			    // Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
			    UStaticMeshComponent* PrevSMC = Cast<UStaticMeshComponent>(BakedOutputObject.GetBakedComponentIfValid());
			    if (IsValid(PrevSMC) && (PrevSMC->GetOwner() == FoundActor))
			    {
				    StaticMeshComponent = PrevSMC;
			    }
		    }
		    
		    if (!IsValid(StaticMeshComponent))
		    {
			    // Create a new static mesh component
			    StaticMeshComponent = NewObject<UStaticMeshComponent>(FoundActor, NAME_None, RF_Transactional);

			    FoundActor->AddInstanceComponent(StaticMeshComponent);
			    StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			    StaticMeshComponent->RegisterComponent();

			    BakedObjectData.BakeStats.NotifyObjectsCreated(StaticMeshComponent->GetClass()->GetName(), 1);
		    }
	    }

	    const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, FoundActor->GetClass(), BakeActorName.ToString(), FoundActor);
	    RenameAndRelabelActor(FoundActor, NewNameStr, false);

	    // The folder is named after the original actor and contains all generated actors
	    SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

	    // Update the previous baked component
	    BakedOutputObject.BakedComponent = FSoftObjectPath(StaticMeshComponent).ToString();
	    
	    if (!IsValid(StaticMeshComponent))
		    return false;
	    
	    // Copy properties from the existing component
	    const bool bCopyWorldTransform = true;
	    CopyPropertyToNewActorAndComponent(FoundActor, StaticMeshComponent, InSMC, bCopyWorldTransform);
	    StaticMeshComponent->SetStaticMesh(BakedStaticMesh);

		// Keep or clear existing actor tags
		FHoudiniEngineUtils::KeepOrClearActorTags(FoundActor, true, false, FoundHGPO);
		if (FoundHGPO)
		{
			// Add actor tags from generic property attributes
			FHoudiniEngineUtils::ApplyTagsToActorOnly(FoundHGPO->GenericPropertyAttributes, FoundActor->Tags);
		}

	    if (DuplicatedSMCOverrideMaterials.Num() > 0)
	    {
			// If we have baked some temporary materials, make sure to update them on the new component
			for (int32 Idx = 0; Idx < StaticMeshComponent->OverrideMaterials.Num(); Idx++)
			{
				UMaterialInterface* CurMat = StaticMeshComponent->GetMaterial(Idx);

				UMaterialInterface** DuplicatedMat = DuplicatedSMCOverrideMaterials.Find(CurMat);
				if (!DuplicatedMat || !IsValid(*DuplicatedMat))
					continue;

				StaticMeshComponent->SetMaterial(Idx, *DuplicatedMat);
			}
	    }
	    
	    BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	    FHoudiniEngineBakedActor OutputEntry(
		    FoundActor,
		    BakeActorName,
		    WorldOutlinerFolderPath,
		    InOutputIndex,
		    InOutputObjectIdentifier,
		    BakedStaticMesh,
		    StaticMesh,
		    StaticMeshComponent,
		    BakeFolderPath,
		    MeshPackageParams);
	    OutputEntry.bInstancerOutput = true;
	    OutputEntry.InstancerPackageParams = InstancerPackageParams;

	    OutBakedActorEntry = OutputEntry;
     
	    // If we are baking in replace mode, remove previously baked components/instancers
	    if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
	    {
		    const bool bInDestroyBakedComponent = false;
		    const bool bInDestroyBakedInstancedActors = true;
		    const bool bInDestroyBakedInstancedComponents = true;
		    DestroyPreviousBakeOutput(
			    BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	    }
	}

	// Set the updated baked output object in the state
	InBakeState.SetNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, BakedOutputObject);
	
	return true;
}


bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_IAC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData)
{
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(
		InOutputIndex, InOutputObjectIdentifier, bHasPreviousBakeData);

	for (auto Component : InOutputObject.OutputComponents)
	{
		UHoudiniInstancedActorComponent* InIAC = Cast<UHoudiniInstancedActorComponent>(Component);
		if (!IsValid(InIAC))
		{
			continue;
		}

		AActor* OwnerActor = InIAC->GetOwner();
		if (!IsValid(OwnerActor))
		{
			return false;
		}

		// Get the object instanced by this IAC
		UObject* InstancedObject = InIAC->GetInstancedObject();
		if (!IsValid(InstancedObject))
		{
			return false;
		}

		// Find the HGPO for this instanced output
		bool FoundHGPO = false;
		FHoudiniGeoPartObject InstancerHGPO;
		for (const auto& curHGPO : InHGPOs)
		{
			if (InOutputObjectIdentifier.Matches(curHGPO))
			{
				InstancerHGPO = curHGPO;
				FoundHGPO = true;
				break;
			}
		}

		// Set the default object name to the 
		const FString DefaultObjectName = InstancedObject->GetName();

		FHoudiniPackageParams PackageParams;
		const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets
			                                                    ? EPackageReplaceMode::ReplaceExistingAssets
			                                                    : EPackageReplaceMode::CreateNewAssets;
		// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
		// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
		FHoudiniAttributeResolver Resolver;
		UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, bHasPreviousBakeData,
			DefaultObjectName,
			PackageParams, Resolver, InBakeFolder.Path, AssetPackageReplaceMode);

		// By default spawn in the current level unless specified via the unreal_level_path attribute
		ULevel* DesiredLevel = GWorld->GetCurrentLevel();

		bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
		if (bHasLevelPathAttribute)
		{
			// Get the package path from the unreal_level_apth attribute
			FString LevelPackagePath = Resolver.ResolveFullLevelPath();

			bool bCreatedPackage = false;
			if (!FindOrCreateDesiredLevelFromLevelPath(
				LevelPackagePath,
				DesiredLevel,
				DesiredWorld,
				bCreatedPackage))
			{
				// TODO: LOG ERROR IF NO LEVEL
				return false;
			}

			// If we have created a level, add it to the packages to save
			// TODO: ? always add?
			if (bCreatedPackage && DesiredLevel)
			{
				BakedObjectData.BakeStats.NotifyPackageCreated(1);
				BakedObjectData.BakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
				// We can now save the package again, and unload it.
				BakedObjectData.PackagesToSave.Add(DesiredLevel->GetOutermost());
			}
		}

		if (!DesiredLevel)
		{
			return false;
		}

		FName WorldOutlinerFolderPath = GetOutlinerFolderPath(Resolver, *PackageParams.HoudiniAssetActorName);

		// Try to find the unreal_bake_actor, if specified. If we found the actor, we will attach the instanced actors
		// to it. If we did not find an actor, but unreal_bake_actor was set, then we create a new actor with that name
		// and parent the instanced actors to it. Otherwise, we don't attach the instanced actors to anything.
		FName ParentActorName;
		FName ParentBakeActorName;
		AActor* ParentActor = nullptr;
		bool bHasBakeActorName = false;
		constexpr AActor* FallbackActor = nullptr;
		FName DefaultBakeActorName = NAME_None;
		if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
		{
			FHoudiniAttributeResolver OutResolver;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(HoudiniAssetComponent->GetWorld(), HoudiniAssetComponent, Tokens);
			OutResolver.SetTokensFromStringMap(Tokens);
			DefaultBakeActorName = FName(OutResolver.ResolveString(BakeSettings.DefaultBakeName));
		}

		FindUnrealBakeActor(InOutputObject, BakedOutputObject, InBakedActors, DesiredLevel, DefaultBakeActorName,
		                    BakeSettings, FallbackActor, ParentActor, bHasBakeActorName, ParentActorName);

		OutActors.Reset();

		if (!ParentActor && bHasBakeActorName)
		{
			// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
			TSubclassOf<AActor> BakeActorClass = nullptr;
			UActorFactory* ActorFactory = GetActorFactory(InOutputObject, BakeSettings, BakeActorClass,
			                                                    UActorFactoryEmptyActor::StaticClass());
			if (!ActorFactory)
			{
				return false;
			}

			constexpr UObject* AssetToSpawn = nullptr;
			constexpr EObjectFlags ObjectFlags = RF_Transactional;
			ParentBakeActorName = *MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), ParentActorName.ToString());

			FActorSpawnParameters SpawnParam;
			SpawnParam.ObjectFlags = ObjectFlags;
			SpawnParam.Name = ParentBakeActorName;
			ParentActor = SpawnBakeActor(ActorFactory, AssetToSpawn, DesiredLevel, BakeSettings,
			                             InIAC->GetComponentTransform(), HoudiniAssetComponent, BakeActorClass, SpawnParam);

			if (!IsValid(ParentActor))
			{
				ParentActor = nullptr;
			}
			else
			{
				BakedObjectData.BakeStats.NotifyObjectsCreated(ParentActor->GetClass()->GetName(), 1);

				ParentActor->SetActorLabel(ParentBakeActorName.ToString());
				OutActors.Emplace(FHoudiniEngineBakedActor(
					ParentActor,
					ParentActorName,
					WorldOutlinerFolderPath,
					InOutputIndex,
					InOutputObjectIdentifier,
					nullptr, // InBakedObject
					nullptr, // InSourceObject
					nullptr, // InBakedComponent
					PackageParams.BakeFolder,
					PackageParams));
			}
		}

		if (ParentActor)
		{
			BakedOutputObject.ActorBakeName = ParentActorName;
			BakedOutputObject.Actor = FSoftObjectPath(ParentActor).ToString();
		}

		// If we are baking in actor replacement mode, remove any previously baked instanced actors for this output
		if (BakeSettings.bReplaceActors && BakedOutputObject.InstancedActors.Num() > 0)
		{
			UWorld* LevelWorld = DesiredLevel->GetWorld();
			if (IsValid(LevelWorld))
			{
				for (const FString& ActorPathStr : BakedOutputObject.InstancedActors)
				{
					const FSoftObjectPath ActorPath(ActorPathStr);

					if (!ActorPath.IsValid())
					{
						continue;
					}

					AActor* Actor = Cast<AActor>(ActorPath.TryLoad());
					// Destroy Actor if it is valid and part of DesiredLevel
					if (IsValid(Actor) && Actor->GetLevel() == DesiredLevel)
					{
						// Just before we destroy the actor, rename it with a _DELETE suffix, so that we can re-use
						// its original name before garbage collection
						FHoudiniEngineUtils::SafeRenameActor(Actor, Actor->GetName() + TEXT("_DELETE"));
#if WITH_EDITOR
						LevelWorld->EditorDestroyActor(Actor, true);
#else
					  LevelWorld->DestroyActor(Actor);
#endif
					}
				}
			}
		}

		// Empty and reserve enough space for new instanced actors
		BakedOutputObject.InstancedActors.Empty(InIAC->GetInstancedActors().Num());

		// Iterates on all the instances of the IAC
		for (AActor* CurrentInstancedActor : InIAC->GetInstancedActors())
		{
			if (!IsValid(CurrentInstancedActor))
			{
				continue;
			}

			// Make sure we have a globally unique name and use it to name the new actor at spawn time.
			const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, CurrentInstancedActor->GetClass(),
			                                                        PackageParams.ObjectName);

			FTransform CurrentTransform = CurrentInstancedActor->GetTransform();

			AActor* NewActor = FHoudiniInstanceTranslator::SpawnInstanceActor(CurrentTransform, DesiredLevel, InIAC, CurrentInstancedActor);
			if (!IsValid(NewActor))
			{
				continue;
			}

			// Explicitly set the actor label as there appears to be a bug in AActor::GetActorLabel() which sets the first
			// duplicate actor name to "name-1" (minus one) instead of leaving off the 0.
			NewActor->SetActorLabel(NewNameStr);

			// Copy properties from the Instanced object, but only for actors.
			constexpr auto CopyOptions = static_cast<EditorUtilities::ECopyOptions::Type>(
				EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
				EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances |
				EditorUtilities::ECopyOptions::CallPostEditChangeProperty |
				EditorUtilities::ECopyOptions::CallPostEditMove);

			// BUG: CopyActorProperties are not copying properties for components (at least on Blueprint type actors).
			EditorUtilities::CopyActorProperties(CurrentInstancedActor, NewActor, CopyOptions);

			// TODO: Copy over component properties!

			// Since we can't properly copy over component properties, the least we can do is apply actor and component tags
			FHoudiniEngineUtils::ApplyTagsToActorAndComponents(
				NewActor, FHoudiniEngineUtils::IsKeepTagsEnabled(&InstancerHGPO), InstancerHGPO.GenericPropertyAttributes);

			BakedObjectData.BakeStats.NotifyObjectsCreated(NewActor->GetClass()->GetName(), 1);

			SetOutlinerFolderPath(NewActor, WorldOutlinerFolderPath);
			NewActor->SetActorTransform(CurrentTransform);

			if (ParentActor)
			{
				NewActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
			}

			BakedOutputObject.InstancedActors.Add(FSoftObjectPath(NewActor).ToString());

			FHoudiniEngineBakedActor& OutputEntry = OutActors.Add_GetRef(FHoudiniEngineBakedActor(
				NewActor,
				*PackageParams.ObjectName,
				WorldOutlinerFolderPath,
				InOutputIndex,
				InOutputObjectIdentifier,
				nullptr,
				InstancedObject,
				nullptr,
				PackageParams.BakeFolder,
				PackageParams));
			OutputEntry.bInstancerOutput = true;
			OutputEntry.InstancerPackageParams = PackageParams;
		}

		// TODO:
		// Move Actors to DesiredLevel if needed??

		// If we are baking in replace mode, remove previously baked components/instancers
		if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
		{
			constexpr bool bInDestroyBakedComponent = true;
			constexpr bool bInDestroyBakedInstancedActors = false;
			constexpr bool bInDestroyBakedInstancedComponents = true;
			DestroyPreviousBakeOutput(
				BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors,
				bInDestroyBakedInstancedComponents);
		}
	}

	// Set the updated bake output object in the state
	InBakeState.SetNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, BakedOutputObject);
	
	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_MSIC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, bHasPreviousBakeData);
	
	for(auto Component : InOutputObject.OutputComponents)
	{
    UHoudiniMeshSplitInstancerComponent * InMSIC = Cast<UHoudiniMeshSplitInstancerComponent>(Component);
    if (!IsValid(InMSIC))
	    continue;

    AActor * OwnerActor = InMSIC->GetOwner();
    if (!IsValid(OwnerActor))
	    return false;

    UStaticMesh * StaticMesh = InMSIC->GetStaticMesh();
    if (!IsValid(StaticMesh))
	    return false;

		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		FindHGPO(InOutputObjectIdentifier, InHGPOs, FoundHGPO);

		// Certain SMC materials may need to be duplicated if we didn't generate the mesh object.
		// Map of duplicated overrides materials (oldTempMaterial , newBakedMaterial)
		TMap<UMaterialInterface*, UMaterialInterface*> DuplicatedMSICOverrideMaterials;
	    
	    UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
	    const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		    EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

	    // Determine if the incoming mesh is temporary by looking for it in the mesh outputs. Populate mesh package params
	    // for baking from it.
	    // If not temporary set the ObjectName from the its package. (Also use this as a fallback default)
	    FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	    UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid());
	    UStaticMesh* BakedStaticMesh = nullptr;

	    // See if the instanced static mesh is still a temporary Houdini created Static Mesh
	    // If it is, we need to bake the StaticMesh first
	    FHoudiniPackageParams InstancerPackageParams;
	    // Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	    // The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	    FHoudiniAttributeResolver InstancerResolver;
	    FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		    DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, bHasPreviousBakeData, ObjectName,
		    InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);
	    
	    FHoudiniPackageParams MeshPackageParams;
	    FString BakeFolderPath = FString();
	    const bool bIsTemporary = IsObjectTemporary(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, InstancerPackageParams.TempCookFolder, InstancerPackageParams.ComponentGUID);
	    if (!bIsTemporary)
	    {
		    BakedStaticMesh = StaticMesh;
	    }
	    else
	    {
		    BakeFolderPath = InBakeFolder.Path;
		    // Try to find the mesh in the outputs
			FHoudiniBakedOutputObject MeshBakedOutputObject;
		    int32 MeshOutputIndex = INDEX_NONE;
		    FHoudiniOutputObjectIdentifier MeshIdentifier;
		    const bool bFoundMeshOutput = FindOutputObject(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
		    if (bFoundMeshOutput)
		    {
			    FHoudiniAttributeResolver MeshResolver;

			    // Found the mesh in the mesh outputs, is temporary
			    const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);
				bool bMeshHasPreviousBakeData = false;
		    	MeshBakedOutputObject = InBakeState.MakeNewBakedOutputObject(MeshOutputIndex, MeshIdentifier, bMeshHasPreviousBakeData);

			    FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
				    DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, bMeshHasPreviousBakeData, ObjectName,
				    MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);

			    // Update with resolved object name
			    ObjectName = MeshPackageParams.ObjectName;
			    BakeFolderPath = MeshPackageParams.BakeFolder;
		    }

		    // This will bake/duplicate the mesh if temporary, or return the input one if it is not
		    BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			    StaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
				BakedObjectData, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap);

			MeshBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();
	    	InBakeState.SetNewBakedOutputObject(MeshOutputIndex, MeshIdentifier, MeshBakedOutputObject);
	    }

		// We may need to duplicate materials overrides if they are temporary
		// (typically, material instances generated by the plugin)
		TArray<UMaterialInterface*> Materials = InMSIC->GetOverrideMaterials();
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if (!IsValid(MaterialInterface))
				continue;

			// Only duplicate the material if it is temporary
			if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InAllOutputs, InTempCookFolder.Path, InstancerPackageParams.ComponentGUID))
			{
				UMaterialInterface* DuplicatedMaterial = BakeSingleMaterialToPackage(
					MaterialInterface, InstancerPackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);
				DuplicatedMSICOverrideMaterials.Add(MaterialInterface, DuplicatedMaterial);
			}
		}

	    // Update the baked output
	    BakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	    // Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	    FString InstancerName = ObjectName + "_instancer";
		if (InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
			InstancerName = InOutputObject.CachedAttributes[HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2];
		InstancerName += "_" + InOutputObjectIdentifier.SplitIdentifier;

		FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			InstancerResolver,
		    FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

	    // By default spawn in the current level unless specified via the unreal_level_path attribute
	    ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	    bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	    if (bHasLevelPathAttribute)
	    {
		    // Get the package path from the unreal_level_path attribute
		    FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		    bool bCreatedPackage = false;
		    if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			    LevelPackagePath,
			    DesiredLevel,
			    DesiredWorld,
			    bCreatedPackage))
		    {
			    // TODO: LOG ERROR IF NO LEVEL
			    return false;
		    }

		    // If we have created a level, add it to the packages to save
		    // TODO: ? always add?
		    if (bCreatedPackage && DesiredLevel)
		    {
			    BakedObjectData.BakeStats.NotifyPackageCreated(1);
			    BakedObjectData.BakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			    // We can now save the package again, and unload it.
			    BakedObjectData.PackagesToSave.Add(DesiredLevel->GetOutermost());
		    }
	    }

	    if (!DesiredLevel)
		    return false;

	    // Try to find the unreal_bake_actor, if specified
	    FName BakeActorName;
	    AActor* FoundActor = nullptr;
	    bool bHasBakeActorName = false;
	    bool bSpawnedActor = false;
	    FindUnrealBakeActor(InOutputObject, BakedOutputObject, InBakedActors, DesiredLevel, *InstancerName, BakeSettings, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName);

	    if (!FoundActor)
	    {
		    // This is a split mesh instancer component - we will create a generic AActor with a bunch of SMC
		    FActorSpawnParameters SpawnInfo;
		    SpawnInfo.OverrideLevel = DesiredLevel;
		    SpawnInfo.ObjectFlags = RF_Transactional;

		    if (!DesiredLevel->bUseExternalActors)
		    {
			    SpawnInfo.Name = FName(MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString()));
		    }
		    SpawnInfo.bDeferConstruction = true;

		    // Spawn the new Actor
		    UClass* ActorClass = GetBakeActorClassOverride(InOutputObject);
		    if (!ActorClass)
			    ActorClass = AActor::StaticClass();
		    FoundActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo);
		    if (!IsValid(FoundActor))
			    return false;
		    bSpawnedActor = true;

		    BakedObjectData.BakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);
		    
		    FHoudiniEngineRuntimeUtils::SetActorLabel(FoundActor, DesiredLevel->bUseExternalActors ? BakeActorName.ToString() : FoundActor->GetActorNameOrLabel());

		    FoundActor->SetActorHiddenInGame(InMSIC->bHiddenInGame);
	    }
	    else
	    {
		    // If we are baking in replacement mode, remove the previous components (if they belong to FoundActor)
		    for (const FString& PrevComponentPathStr : BakedOutputObject.InstancedComponents)
		    {
			    const FSoftObjectPath PrevComponentPath(PrevComponentPathStr);

			    if (!PrevComponentPath.IsValid())
				    continue;
			    
			    UActorComponent* PrevComponent = Cast<UActorComponent>(PrevComponentPath.TryLoad());
			    if (!IsValid(PrevComponent) || PrevComponent->GetOwner() != FoundActor)
				    continue;

			    RemovePreviouslyBakedComponent(PrevComponent);
		    }

		    const FString UniqueActorNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString(), FoundActor);
		    RenameAndRelabelActor(FoundActor, UniqueActorNameStr, false);

		    BakedObjectData.BakeStats.NotifyObjectsUpdated(FoundActor->GetClass()->GetName(), 1);
	    }
	    // The folder is named after the original actor and contains all generated actors
	    SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

	    // Get/create the actor's root component
	    const bool bCreateIfMissing = true;
	    USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);
	    if (bSpawnedActor && IsValid(RootComponent))
		    RootComponent->SetWorldTransform(InTransform);

	    // Empty and reserve enough space in the baked components array for the new components
	    BakedOutputObject.InstancedComponents.Empty(InMSIC->GetInstances().Num());

	    // Now add s SMC component for each of the SMC's instance
	    for (UStaticMeshComponent* CurrentSMC : InMSIC->GetInstances())
	    {
		    if (!IsValid(CurrentSMC))
			    continue;

		    UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(
			    CurrentSMC,
			    FoundActor,
			    FName(MakeUniqueObjectNameIfNeeded(FoundActor, CurrentSMC->GetClass(), CurrentSMC->GetName())));
		    if (!IsValid(NewSMC))
			    continue;

		    BakedObjectData.BakeStats.NotifyObjectsCreated(NewSMC->GetClass()->GetName(), 1);
		    
		    BakedOutputObject.InstancedComponents.Add(FSoftObjectPath(NewSMC).ToString());
		    
		    NewSMC->RegisterComponent();
		    // NewSMC->SetupAttachment(nullptr);
		    NewSMC->SetStaticMesh(BakedStaticMesh);
		    FoundActor->AddInstanceComponent(NewSMC);
		    NewSMC->SetWorldTransform(CurrentSMC->GetComponentTransform());

			if (DuplicatedMSICOverrideMaterials.Num() > 0)
			{
				// If we have baked some temporary materials, make sure to update them on the new component
				for (int32 Idx = 0; Idx < NewSMC->OverrideMaterials.Num(); Idx++)
				{
					UMaterialInterface* CurMat = NewSMC->GetMaterial(Idx);

					UMaterialInterface** DuplicatedMat = DuplicatedMSICOverrideMaterials.Find(CurMat);
					if (!DuplicatedMat || !IsValid(*DuplicatedMat))
						continue;

					NewSMC->SetMaterial(Idx, *DuplicatedMat);
				}
			}
		    
		    if (IsValid(RootComponent))
			    NewSMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

		    // TODO: Do we need to copy properties here, we duplicated the component
		    // // Copy properties from the existing component
		    // CopyPropertyToNewActorAndComponent(FoundActor, NewSMC, CurrentSMC);
	    	
	    }

		// We always have to set the tags _after_ any calls to CopyPropertyToNewActorAndComponent, since
		// CopyPropertyToNewActorAndComponent is not able to enforce the KeepTags mechanism
		
		FHoudiniEngineUtils::KeepOrClearActorTags(FoundActor, true, false, FoundHGPO);
		if (FoundHGPO)
		{
			// Add actor tags from generic property attributes
			FHoudiniEngineUtils::ApplyTagsToActorOnly(FoundHGPO->GenericPropertyAttributes, FoundActor->Tags);
		}

	    if (bSpawnedActor)
		    FoundActor->FinishSpawning(InTransform);

	    BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	    FHoudiniEngineBakedActor OutputEntry(
		    FoundActor,
		    BakeActorName,
		    WorldOutlinerFolderPath,
		    InOutputIndex,
		    InOutputObjectIdentifier,
		    BakedStaticMesh,
		    StaticMesh,
		    nullptr,
		    BakeFolderPath,
		    MeshPackageParams);
	    OutputEntry.bInstancerOutput = true;
	    OutputEntry.InstancerPackageParams = InstancerPackageParams;

	    // Postpone these calls to do them once per actor
	    OutputEntry.bPostBakeProcessPostponed = true;

	    OutBakedActorEntry = OutputEntry;

	    // If we are baking in replace mode, remove previously baked components/instancers
	    if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
	    {
		    const bool bInDestroyBakedComponent = true;
		    const bool bInDestroyBakedInstancedActors = true;
		    const bool bInDestroyBakedInstancedComponents = false;
		    DestroyPreviousBakeOutput(
			    BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	    }
	}

	InBakeState.SetNewBakedOutputObject(InOutputIndex, InOutputObjectIdentifier, BakedOutputObject);
	
	return true;
}

bool
FHoudiniEngineBakeUtils::FindHGPO(
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	FHoudiniGeoPartObject const*& OutHGPO)
{
	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	for (auto & NextHGPO : InHGPOs) 
	{
		// We use Matches() here as it handles the case where the HDA was loaded,
		// which likely means that the the obj/geo/part ids dont match the output identifier
		if(InIdentifier.Matches(NextHGPO))
		{
			FoundHGPO = &NextHGPO;
			break;
		}
	}

	OutHGPO = FoundHGPO;
	return !OutHGPO;
}

void
FHoudiniEngineBakeUtils::GetTemporaryOutputObjectBakeName(
	const UObject* InObject,
	const FHoudiniOutputObject& InMeshOutputObject,
	FString& OutBakeName)
{
	// The bake name override has priority
	OutBakeName = InMeshOutputObject.BakeName;
	if (OutBakeName.IsEmpty())
	{
		FHoudiniAttributeResolver Resolver;
		Resolver.SetCachedAttributes(InMeshOutputObject.CachedAttributes);
		Resolver.SetTokensFromStringMap(InMeshOutputObject.CachedTokens);
		const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(InObject);
		// The default output name (if not set via attributes) is {object_name}, which look for an object_name
		// key-value token
		if (!Resolver.GetCachedTokens().Contains(TEXT("object_name")))
			Resolver.SetToken(TEXT("object_name"), DefaultObjectName);
		OutBakeName = Resolver.ResolveOutputName();
		// const TArray<FHoudiniGeoPartObject>& HGPOs = InAllOutputs[MeshOutputIdx]->GetHoudiniGeoPartObjects();
		// const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		// FindHGPO(MeshIdentifier, HGPOs, FoundHGPO);
		// // ... finally the part name
		// if (FoundHGPO && FoundHGPO->bHasCustomPartName)
		// 	OutBakeName = FoundHGPO->PartName;
		if (OutBakeName.IsEmpty())
			OutBakeName = DefaultObjectName;
	}
}

bool
FHoudiniEngineBakeUtils::GetTemporaryOutputObjectBakeName(
	const UObject* InObject,
	EHoudiniOutputType InOutputType,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FString& OutBakeName)
{
	if (!IsValid(InObject))
		return false;
	
	OutBakeName.Empty();
	
	int32 MeshOutputIdx = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	if (FindOutputObject(InObject, InOutputType, InAllOutputs, MeshOutputIdx, MeshIdentifier))
	{
		// Found the mesh, get its name
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIdx]->GetOutputObjects().FindChecked(MeshIdentifier);
		GetTemporaryOutputObjectBakeName(InObject, MeshOutputObject, OutBakeName);
		
		return true;
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::BakeStaticMeshOutputObjectToActor(
	const UHoudiniAssetComponent* InHoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InTempCookFolder,
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder,
	const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniBakedObjectData& BakedObjectData,
	bool& bOutBakedToActor,
	FHoudiniEngineBakedActor& OutBakedActorEntry
)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;
	UHoudiniOutput* const InOutput = InAllOutputs[InOutputIndex];
	
	// Initialize the baked output object entry (use the previous bake's data, if available).
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, InIdentifier, bHasPreviousBakeData);

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(InOutputObject.OutputObject);
	if (!IsValid(StaticMesh))
		return false;

	// Allow baking of static mesh output objects without components here: it could be complex collision meshes and
	// meshes used in instancers / foliage
	// HOUDINI_CHECK_RETURN(InOutputObject.OutputComponents.Num() == 1 || (InOutputObject.OutputComponents.IsEmpty() && InOutputObject.bIsImplicit), false);
	
	UStaticMeshComponent* InSMC = nullptr;
	if (InOutputObject.OutputComponents.Num() >= 1)
		InSMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponents[0]);
	const bool bHasOutputSMC = IsValid(InSMC);
	// if (!bHasOutputSMC && !InOutputObject.bIsImplicit)
	// 	return false;

	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	FindHGPO(InIdentifier, InHGPOs, FoundHGPO);

	// We do not bake templated geos
	if (FoundHGPO && FoundHGPO->bIsTemplated)
		return true;

	const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);

	UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	FHoudiniPackageParams PackageParams;

	FHoudiniAttributeResolver Resolver;

	if (!ResolvePackageParamsWithResolver(
		InHoudiniAssetComponent,
		InOutput,
		InIdentifier,
		InOutputObject,
		bHasPreviousBakeData,
		DefaultObjectName,
		InBakeFolder,
		BakeSettings,
		PackageParams,
		Resolver,
		BakedObjectData))
	{
		return false;
	}
	
	// Bake the static mesh if it is still temporary
	UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh,
		Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid()),
		PackageParams,
		InAllOutputs,
		InAllBakedActors,
		InTempCookFolder.Path,
		BakedObjectData,
		InOutAlreadyBakedStaticMeshMap,
		InOutAlreadyBakedMaterialsMap);

	if (!IsValid(BakedSM))
		return false;

	// Record the baked object
	BakedOutputObject.BakedObject = FSoftObjectPath(BakedSM).ToString();

	if (bHasOutputSMC)
	{
		const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			Resolver,
			FName(InFallbackWorldOutlinerFolder.IsEmpty() ? PackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

		// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
		TSubclassOf<AActor> BakeActorClass = nullptr;
		UActorFactory* Factory = GetActorFactory(InOutputObject, BakeSettings, BakeActorClass, UActorFactoryEmptyActor::StaticClass(), BakedSM);

		// If we could not find a factory, we have to skip this output object
		if (!Factory)
			return false;

		// Make sure we have a level to spawn to
		if (!IsValid(DesiredLevel))
			return false;

		// Try to find the unreal_bake_actor, if specified
		FName BakeActorName;
		AActor* FoundActor = nullptr;
		bool bHasBakeActorName = false;
		FName DefaultActorName = FName(*(PackageParams.ObjectName));
		if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
		{
			FHoudiniAttributeResolver OutResolver;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(InHoudiniAssetComponent->GetWorld(), InHoudiniAssetComponent, Tokens);
			OutResolver.SetTokensFromStringMap(Tokens);
			DefaultActorName = FName(OutResolver.ResolveString(BakeSettings.DefaultBakeName));
		}

		FindUnrealBakeActor(
			InOutputObject,
			BakedOutputObject,
			InAllBakedActors,
			DesiredLevel,
			DefaultActorName,
			BakeSettings,
			InFallbackActor,
			FoundActor,
			bHasBakeActorName,
			BakeActorName);

		bool bCreatedNewActor = false;
		UStaticMeshComponent* SMC = nullptr;
		if (!FoundActor)
		{
			// Spawn the new actor
			FoundActor = SpawnBakeActor(Factory, BakedSM, DesiredLevel, BakeSettings, InSMC->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);
			if (!IsValid(FoundActor))
				return false;

			bCreatedNewActor = true;
			
			// Copy properties to new actor
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
			if (IsValid(SMActor))
				SMC = SMActor->GetStaticMeshComponent();
		}


		
		if (!IsValid(SMC))
		{
			if (BakeSettings.bReplaceAssets && !bCreatedNewActor)
			{
				// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
				UStaticMeshComponent* PrevSMC = Cast<UStaticMeshComponent>(BakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(PrevSMC) && (PrevSMC->GetOwner() == FoundActor))
				{
					SMC = PrevSMC;
				}
			}

			const bool bCreateIfMissing = false;
			USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);

			if (!IsValid(SMC))
			{
				// Create a new static mesh component on the existing actor
				SMC = NewObject<UStaticMeshComponent>(FoundActor, NAME_None, RF_Transactional);

				FoundActor->AddInstanceComponent(SMC);
				if (IsValid(RootComponent))
					SMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				else
					FoundActor->SetRootComponent(SMC);
				SMC->RegisterComponent();
			}
		}

		// We need to make a unique name for the actor, renaming an object on top of another is a fatal error
		const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, BakeActorName.ToString(), FoundActor);
		RenameAndRelabelActor(FoundActor, NewNameStr, false);
		SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

		if (IsValid(SMC))
		{
			constexpr bool bCopyWorldTransform = true;
			CopyPropertyToNewActorAndComponent(FoundActor, SMC, InSMC, bCopyWorldTransform);
			SMC->SetStaticMesh(BakedSM);
			BakedOutputObject.BakedComponent = FSoftObjectPath(SMC).ToString();
		}

		FHoudiniEngineUtils::KeepOrClearActorTags(FoundActor, true, false, FoundHGPO);
		if (FoundHGPO)
		{
			// Add actor tags from generic property attributes
			FHoudiniEngineUtils::ApplyTagsToActorOnly(FoundHGPO->GenericPropertyAttributes, FoundActor->Tags);
		}

		BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		OutBakedActorEntry = FHoudiniEngineBakedActor(
			FoundActor, BakeActorName, WorldOutlinerFolderPath, InOutputIndex, InIdentifier, BakedSM, StaticMesh, SMC,
			PackageParams.BakeFolder, PackageParams);
		bOutBakedToActor = true;
	}
	else
	{
		// Implicit object, no component and no actor
		BakedOutputObject.BakedComponent = nullptr;
		BakedOutputObject.Actor = nullptr;
		bOutBakedToActor = false;
	}

	// If we are baking in replace mode, remove previously baked components/instancers
	if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
	{
		constexpr bool bInDestroyBakedComponent = false;
		constexpr bool bInDestroyBakedInstancedActors = true;
		constexpr bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	// Record bake data
	InBakeState.SetNewBakedOutputObject(InOutputIndex, InIdentifier, BakedOutputObject);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeSkeletalMeshOutputObjectToActor(
	const UHoudiniAssetComponent* InHoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InTempCookFolder,
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder,
	const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
	TMap<USkeletalMesh*, USkeletalMesh*>& InOutAlreadyBakedSkeletalMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	FHoudiniBakedObjectData& BakedObjectData,
	bool& bOutBakedToActor,
	FHoudiniEngineBakedActor& OutBakedActorEntry
)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;
	UHoudiniOutput* const InOutput = InAllOutputs[InOutputIndex];

	// Initialize the baked output object entry (use the previous bake's data, if available).
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, InIdentifier, bHasPreviousBakeData);

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(InOutputObject.OutputObject);
	if (!IsValid(SkeletalMesh))
		return false;

	HOUDINI_CHECK_RETURN(InOutputObject.OutputComponents.Num() == 1 || (InOutputObject.OutputComponents.IsEmpty() && InOutputObject.bIsImplicit), false);
	
	USkeletalMeshComponent* InSKC = nullptr;
	if (InOutputObject.OutputComponents.Num() >= 1)
		InSKC = Cast<USkeletalMeshComponent>(InOutputObject.OutputComponents[0]);
	const bool bHasOutputSKC = IsValid(InSKC);
	if (!bHasOutputSKC && !InOutputObject.bIsImplicit)
		return false;

	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	FindHGPO(InIdentifier, InHGPOs, FoundHGPO);

	// We do not bake templated geos
	if (FoundHGPO && FoundHGPO->bIsTemplated)
		return true;

	const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(SkeletalMesh);

	UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	// Bake Skeleton

	FHoudiniPackageParams SkeletonPackageParams;
	FHoudiniOutputObjectIdentifier SkeletonIdentifier = InIdentifier;
	SkeletonIdentifier.SplitIdentifier = TEXT("skeleton");
	if (!ResolvePackageParams(
		InHoudiniAssetComponent,
		InOutput,
		SkeletonIdentifier,
		InOutputObject,
		bHasPreviousBakeData,
		DefaultObjectName + TEXT("_skeleton"),
		InBakeFolder,
		BakeSettings,
		SkeletonPackageParams,
		BakedObjectData))
	{
		return false;
	}
	// TODO: add an attribute for controlling the skeleton's bake name?
	if (!SkeletonPackageParams.ObjectName.Contains(TEXT("skeleton"), ESearchCase::IgnoreCase))
		SkeletonPackageParams.ObjectName += TEXT("_skeleton");

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	USkeleton* BakedSkeleton = DuplicateSkeletonAndCreatePackageIfNeeded(
		Skeleton,
		BakedOutputObject.GetBakedSkeletonIfValid(),
		SkeletonPackageParams,
		InAllOutputs,
		InAllBakedActors,
		InTempCookFolder.Path,
		BakedObjectData,
		InBakeState.GetBakedSkeletons());

	if (Skeleton != BakedSkeleton)
		BakedOutputObject.BakedSkeleton = FSoftObjectPath(BakedSkeleton).ToString();
	else
		BakedOutputObject.BakedSkeleton = FSoftObjectPath(nullptr).ToString();

	// Bake Physics Asset

	FHoudiniPackageParams PhysicsAssetsPackageParams;
	FHoudiniOutputObjectIdentifier PhysicsAsssetIdentifier = InIdentifier;
	PhysicsAsssetIdentifier.SplitIdentifier = TEXT("physics_asset");
	if (!ResolvePackageParams(
		InHoudiniAssetComponent,
		InOutput,
		PhysicsAsssetIdentifier,
		InOutputObject,
		bHasPreviousBakeData,
		DefaultObjectName + TEXT("_physics_asset"),
		InBakeFolder,
		BakeSettings,
		PhysicsAssetsPackageParams,
		BakedObjectData))
	{
		return false;
	}
	// TODO: add an attribute for controlling the bake name?
	if (!PhysicsAssetsPackageParams.ObjectName.Contains(TEXT("physics_asset"), ESearchCase::IgnoreCase))
		PhysicsAssetsPackageParams.ObjectName += TEXT("physics_asset");

	// Bake the skeleton if it is temporary
	UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	UPhysicsAsset* BakedPhysicsAsset = DuplicatePhysicsAssetAndCreatePackageIfNeeded(
		PhysicsAsset,
		BakedOutputObject.GetBakedPhysicsAssetIfValid(),
		PhysicsAssetsPackageParams,
		InAllOutputs,
		InAllBakedActors,
		InTempCookFolder.Path,
		BakedObjectData,
		InBakeState.GetBakedPhysicsAssets());

	if (PhysicsAsset != BakedPhysicsAsset)
		BakedOutputObject.BakedPhysicsAsset = FSoftObjectPath(BakedPhysicsAsset).ToString();
	else
		BakedOutputObject.BakedPhysicsAsset = FSoftObjectPath(nullptr).ToString();

	FHoudiniAttributeResolver Resolver;

	FHoudiniPackageParams PackageParams;
	if (!ResolvePackageParamsWithResolver(
		InHoudiniAssetComponent,
		InOutput,
		InIdentifier,
		InOutputObject,
		bHasPreviousBakeData,
		DefaultObjectName,
		InBakeFolder,
		BakeSettings,
		PackageParams,
		Resolver,
		BakedObjectData))
	{
		return false;
	}

	// Bake the skeletal mesh if it is still temporary
	USkeletalMesh* BakedSK = FHoudiniEngineBakeUtils::DuplicateSkeletalMeshAndCreatePackageIfNeeded(
		SkeletalMesh,
		Cast<USkeletalMesh>(BakedOutputObject.GetBakedObjectIfValid()),
		PackageParams,
		InAllOutputs,
		InAllBakedActors,
		InTempCookFolder.Path,
		BakedObjectData,
		InOutAlreadyBakedSkeletalMeshMap,
		InOutAlreadyBakedMaterialsMap);

	if (!IsValid(BakedSK))
		return false;

	// Update the skeleton of the BakedSK if the skeleton was baked
	if (BakedSK->GetSkeleton() != BakedSkeleton)
		BakedSK->SetSkeleton(BakedSkeleton);

	if (BakedSK->GetPhysicsAsset() != BakedPhysicsAsset)
	{
		BakedSK->SetPhysicsAsset(BakedPhysicsAsset);
		BakedPhysicsAsset->SetPreviewMesh(BakedSK);
	}

	// Record the baked object
	BakedOutputObject.BakedObject = FSoftObjectPath(BakedSK).ToString();

	if (bHasOutputSKC)
	{
		const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			Resolver,
			FName(InFallbackWorldOutlinerFolder.IsEmpty() ? PackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

		// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
		TSubclassOf<AActor> BakeActorClass = nullptr;
		UActorFactory* const Factory = GetActorFactory(InOutputObject, BakeSettings, BakeActorClass, UActorFactoryEmptyActor::StaticClass(), BakedSK);

		// If we could not find a factory, we have to skip this output object
		if (!Factory)
			return false;

		// Make sure we have a level to spawn to
		if (!IsValid(DesiredLevel))
			return false;

		// Try to find the unreal_bake_actor, if specified
		FName BakeActorName;
		AActor* FoundActor = nullptr;
		bool bHasBakeActorName = false;
		FindUnrealBakeActor(
			InOutputObject,
			BakedOutputObject,
			InAllBakedActors,
			DesiredLevel,
			*(PackageParams.ObjectName),
			BakeSettings,
			InFallbackActor,
			FoundActor,
			bHasBakeActorName,
			BakeActorName);

		bool bCreatedNewActor = false;
		USkeletalMeshComponent* SKC = nullptr;
		if (!FoundActor)
		{
			// Spawn the new actor
			FoundActor = SpawnBakeActor(Factory, BakedSK, DesiredLevel, BakeSettings, InSKC->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);
			if (!IsValid(FoundActor))
				return false;

			bCreatedNewActor = true;

			// Copy properties to new actor
			ASkeletalMeshActor* SMActor = Cast<ASkeletalMeshActor >(FoundActor);
			if (IsValid(SMActor))
				SKC = SMActor->GetSkeletalMeshComponent();
		}

		if (!IsValid(SKC))
		{
			if (BakeSettings.bReplaceAssets && !bCreatedNewActor)
			{
				// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
				USkeletalMeshComponent* PrevSKC = Cast<USkeletalMeshComponent>(BakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(PrevSKC) && (PrevSKC->GetOwner() == FoundActor))
				{
					SKC = PrevSKC;
				}
			}

			const bool bCreateIfMissing = true;
			USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);

			if (!IsValid(SKC))
			{
				// Create a new static mesh component on the existing actor
				SKC = NewObject<USkeletalMeshComponent>(FoundActor, NAME_None, RF_Transactional);

				FoundActor->AddInstanceComponent(SKC);
				if (IsValid(RootComponent))
					SKC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				else
					FoundActor->SetRootComponent(SKC);
				SKC->RegisterComponent();
			}
		}

		// We need to make a unique name for the actor, renaming an object on top of another is a fatal error
		const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, BakeActorName.ToString(), FoundActor);
		RenameAndRelabelActor(FoundActor, NewNameStr, false);
		SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

		if (IsValid(SKC))
		{
			constexpr bool bCopyWorldTransform = true;
			CopyPropertyToNewActorAndSkeletalComponent(FoundActor, SKC, InSKC, bCopyWorldTransform);
			SKC->SetSkeletalMesh(BakedSK);
			BakedOutputObject.BakedComponent = FSoftObjectPath(SKC).ToString();
		}

		BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		OutBakedActorEntry = FHoudiniEngineBakedActor(
			FoundActor, BakeActorName, WorldOutlinerFolderPath, InOutputIndex, InIdentifier, BakedSK, SkeletalMesh, SKC,
			PackageParams.BakeFolder, PackageParams);
		bOutBakedToActor = true;
	}
	else
	{
		// Implicit object, no component and no actor
		BakedOutputObject.BakedComponent = nullptr;
		BakedOutputObject.Actor = nullptr;
		bOutBakedToActor = false;
	}

	// If we are baking in replace mode, remove previously baked components/instancers
	if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
	{
		constexpr bool bInDestroyBakedComponent = false;
		constexpr bool bInDestroyBakedInstancedActors = true;
		constexpr bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	// Record bake data
	InBakeState.SetNewBakedOutputObject(InOutputIndex, InIdentifier, MoveTemp(BakedOutputObject));

	return true;
}


bool
FHoudiniEngineBakeUtils::BakeSkeletalMeshOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(InOutput))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	//DAMIEN - Do we need to do something with this
	TMap<USkeletalMesh*, USkeletalMesh*> AlreadyBakedSkeletalMeshMap;

	// We need to bake invisible complex colliders first, since they are static meshes themselves but are referenced
	// by the main static mesh
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		const EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(Identifier.SplitIdentifier);
		if (SplitType != EHoudiniSplitType::InvisibleComplexCollider)
			continue;

		const FHoudiniOutputObject& OutputObject = Pair.Value;

		bool bBakedToActor = false;
		FHoudiniEngineBakedActor BakedActorEntry;
		bool WasBaked = false;

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(OutputObject.OutputObject);
		if (IsValid(SkeletalMesh))
		{
			WasBaked = BakeSkeletalMeshOutputObjectToActor(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				Identifier,
				OutputObject,
				HGPOs,
				InBakeState,
				InTempCookFolder,
				InBakeFolder,
				BakeSettings,
				InFallbackActor,
				InFallbackWorldOutlinerFolder,
				AllBakedActors,
				AlreadyBakedSkeletalMeshMap,
				InOutAlreadyBakedMaterialsMap,
				BakedObjectData,
				bBakedToActor,
				BakedActorEntry);
		}

		if (WasBaked && bBakedToActor)
		{
			NewBakedActors.Add(BakedActorEntry);
			AllBakedActors.Add(BakedActorEntry);
		}
	}

	// Now bake the other output objects
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		const EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(Identifier.SplitIdentifier);
		if (SplitType == EHoudiniSplitType::InvisibleComplexCollider)
			continue;

		const FHoudiniOutputObject& OutputObject = Pair.Value;

		bool bBakedToActor = false;
		FHoudiniEngineBakedActor BakedActorEntry;
		bool WasBaked = false;

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(OutputObject.OutputObject);
		if (IsValid(SkeletalMesh))
		{
			WasBaked = BakeSkeletalMeshOutputObjectToActor(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				Identifier,
				OutputObject,
				HGPOs,
				InBakeState,
				InTempCookFolder,
				InBakeFolder,
				BakeSettings,
				InFallbackActor,
				InFallbackWorldOutlinerFolder,
				AllBakedActors,
				AlreadyBakedSkeletalMeshMap,
				InOutAlreadyBakedMaterialsMap,
				BakedObjectData,
				bBakedToActor,
				BakedActorEntry);
		}
		
		if (WasBaked && bBakedToActor)
		{
			NewBakedActors.Add(BakedActorEntry);
			AllBakedActors.Add(BakedActorEntry);
		}
	}

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(InOutput))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	//DAMIEN - Do we need to do something with this
	TMap<USkeletalMesh*, USkeletalMesh*> AlreadyBakedSkeletalMeshMap;

	// We need to bake invisible complex colliders first, since they are static meshes themselves but are referenced
	// by the main static mesh
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		const EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(Identifier.SplitIdentifier);
		if (SplitType != EHoudiniSplitType::InvisibleComplexCollider)
			continue;
		
		const FHoudiniOutputObject& OutputObject = Pair.Value;

		bool bBakedToActor = false;
		FHoudiniEngineBakedActor BakedActorEntry;
		bool WasBaked = false;

		UStaticMesh* StaticMesh = Cast<UStaticMesh>(OutputObject.OutputObject);
		if (IsValid(StaticMesh))
		{
			WasBaked = BakeStaticMeshOutputObjectToActor(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				Identifier,
				OutputObject,
				HGPOs,
				InBakeState,
				InTempCookFolder,
				InBakeFolder,
				BakeSettings,
				InFallbackActor,
				InFallbackWorldOutlinerFolder,
				AllBakedActors,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				BakedObjectData,
				bBakedToActor,
				BakedActorEntry);
		}
		else
		{
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(OutputObject.OutputObject);
			if (IsValid(SkeletalMesh))
			{
				WasBaked = BakeSkeletalMeshOutputObjectToActor(
					HoudiniAssetComponent,
					InOutputIndex,
					InAllOutputs,
					Identifier,
					OutputObject,
					HGPOs,
					InBakeState,
					InTempCookFolder,
					InBakeFolder,
					BakeSettings,
					InFallbackActor,
					InFallbackWorldOutlinerFolder,
					AllBakedActors,
					AlreadyBakedSkeletalMeshMap,
					InOutAlreadyBakedMaterialsMap,
					BakedObjectData,
					bBakedToActor,
					BakedActorEntry);
			}
		}

		if (WasBaked && bBakedToActor)
		{
			NewBakedActors.Add(BakedActorEntry);
			AllBakedActors.Add(BakedActorEntry);
		}
	}
	
	// Now bake the other output objects
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		const EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(Identifier.SplitIdentifier);
		if (SplitType == EHoudiniSplitType::InvisibleComplexCollider)
			continue;

		const FHoudiniOutputObject& OutputObject = Pair.Value;

		bool bBakedToActor = false;
		FHoudiniEngineBakedActor BakedActorEntry;
		bool WasBaked = false;

		UStaticMesh* StaticMesh = Cast<UStaticMesh>(OutputObject.OutputObject);
		if (IsValid(StaticMesh))
		{
			WasBaked = BakeStaticMeshOutputObjectToActor(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				Identifier,
				OutputObject,
				HGPOs,
				InBakeState,
				InTempCookFolder,
				InBakeFolder,
				BakeSettings,
				InFallbackActor,
				InFallbackWorldOutlinerFolder,
				AllBakedActors,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				BakedObjectData,
				bBakedToActor,
				BakedActorEntry);
		}
		else
		{
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(OutputObject.OutputObject);
			if (IsValid(SkeletalMesh))
			{
				WasBaked = BakeSkeletalMeshOutputObjectToActor(
					HoudiniAssetComponent,
					InOutputIndex,
					InAllOutputs,
					Identifier,
					OutputObject,
					HGPOs,
					InBakeState,
					InTempCookFolder,
					InBakeFolder,
					BakeSettings,
					InFallbackActor,
					InFallbackWorldOutlinerFolder,
					AllBakedActors,
					AlreadyBakedSkeletalMeshMap,
					InOutAlreadyBakedMaterialsMap,
					BakedObjectData,
					bBakedToActor,
					BakedActorEntry);
			}
		}
		if (WasBaked && bBakedToActor)
		{
			NewBakedActors.Add(BakedActorEntry);
			AllBakedActors.Add(BakedActorEntry);
		}
	}

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool FHoudiniEngineBakeUtils::ResolvePackageParams(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	UHoudiniOutput* InOutput,
	const FHoudiniOutputObjectIdentifier& Identifier,
	const FHoudiniOutputObject& InOutputObject,
	const bool bInHasPreviousBakeData,
	const FString& DefaultObjectName,
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniPackageParams& OutPackageParams,
	FHoudiniBakedObjectData& BakedObjectData,
	const FString& InHoudiniAssetName,
	const FString& InHoudiniAssetActorName)
{
	FHoudiniAttributeResolver Resolver;

	return ResolvePackageParamsWithResolver(
		HoudiniAssetComponent, 
		InOutput, 
		Identifier, 
		InOutputObject, 
		bInHasPreviousBakeData, 
		DefaultObjectName, 
		InBakeFolder, 
		BakeSettings,
		OutPackageParams, 
		Resolver, 
		BakedObjectData,
		InHoudiniAssetName,
		InHoudiniAssetActorName);


}
bool FHoudiniEngineBakeUtils::ResolvePackageParamsWithResolver(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	UHoudiniOutput* InOutput, 
	const FHoudiniOutputObjectIdentifier& Identifier,
	const FHoudiniOutputObject& InOutputObject,
	const bool bInHasPreviousBakeData,
	const FString& DefaultObjectName, 
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniPackageParams& OutPackageParams,
	FHoudiniAttributeResolver& Resolver,
	FHoudiniBakedObjectData& BakedObjectData,
	const FString& InHoudiniAssetName,
	const FString& InHoudiniAssetActorName)
{
	UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	// Set the replace mode based on if we are doing a replacement or incremental asset bake
	const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		DesiredWorld, HoudiniAssetComponent, Identifier, InOutputObject, bInHasPreviousBakeData, DefaultObjectName,
		OutPackageParams, Resolver, InBakeFolder.Path, AssetPackageReplaceMode,
		InHoudiniAssetName, InHoudiniAssetActorName);

	// See if this output object has an unreal_level_path attribute specified
	// In which case, we need to create/find the desired level for baking instead of using the current one
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_path attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add the level to the packages to save?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			BakedObjectData.PackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	return true;
}

UUserDefinedStruct * FHoudiniEngineBakeUtils::CreateBakedUserDefinedStruct(
	UHoudiniOutput* CookedOutput,
	const FHoudiniOutputObjectIdentifier& Identifier,
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniBakedOutput& InPreviousBakedOutput,
	FHoudiniBakedOutput& InNewBakedOutput,
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniBakedObjectData& BakedObjectData)
{
	FHoudiniPackageParams PackageParams;

	FHoudiniOutputObject& OutputObject =  CookedOutput->GetOutputObjects().FindOrAdd(Identifier);
	FHoudiniBakedOutputObject BakedOutputObject;
	const bool bHasPreviousBakeData = InPreviousBakedOutput.BakedOutputObjects.Contains(Identifier);
	if (bHasPreviousBakeData)
		BakedOutputObject = InPreviousBakedOutput.BakedOutputObjects.FindChecked(Identifier);

	auto * UserStruct = Cast<UUserDefinedStruct>(OutputObject.OutputObject);

	FHoudiniOutputObjectIdentifier BakeIdentifier = Identifier;
	BakeIdentifier.SplitIdentifier = TEXT("rowstruct");
	
	if (!ResolvePackageParams(HoudiniAssetComponent,
		CookedOutput,
		BakeIdentifier,
		OutputObject,
		bHasPreviousBakeData,
		FString(""),
		InBakeFolder,
		BakeSettings,
		PackageParams,
		BakedObjectData))
	{
		return nullptr;
	}

	FString* OutputName = nullptr;

	if ((OutputName = OutputObject.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWSTRUCT)))
	{
		// use the name verbatim from the user.
		PackageParams.ObjectName = *OutputName;
	}
	else if (!PackageParams.ObjectName.IsEmpty() && !PackageParams.ObjectName.Contains(TEXT("rowstruct")))
	{
		PackageParams.ObjectName += TEXT("_rowstruct");
	}

	FString PackageName = PackageParams.GetPackagePath();
	FString CreatedPackageName;
	UPackage* Package = PackageParams.CreatePackageForObject(CreatedPackageName);

	UUserDefinedStruct* BakedObject = DuplicateUserDefinedStruct(UserStruct, Package, CreatedPackageName);
	BakedOutputObject.BakedObject = BakedObject->GetPathName();
	BakedObjectData.PackagesToSave.Add(Package);

	InNewBakedOutput.BakedOutputObjects.Emplace(Identifier, BakedOutputObject);

	return BakedObject;
}

UDataTable* FHoudiniEngineBakeUtils::CreateBakedDataTable(
	UScriptStruct* UserDefinedStruct,
	const FString & ObjectName,
	UHoudiniOutput* CookedOutput,
	const FHoudiniOutputObjectIdentifier& Identifier,
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniBakedOutput& InPreviousBakedOutput,
	FHoudiniBakedOutput& InNewBakedOutput,
	const FDirectoryPath& BakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniBakedObjectData& BakedObjectData)
{
	FHoudiniOutputObject& OutputObject = CookedOutput->GetOutputObjects().FindOrAdd(Identifier);
	FHoudiniBakedOutputObject BakedOutputObject;
	const bool bHasPreviousBakeData = InPreviousBakedOutput.BakedOutputObjects.Contains(Identifier);
	if (bHasPreviousBakeData)
		BakedOutputObject = InPreviousBakedOutput.BakedOutputObjects.FindChecked(Identifier);

	FHoudiniPackageParams PackageParams;

	FHoudiniOutputObjectIdentifier BakeIdentifier = Identifier;
	BakeIdentifier.SplitIdentifier = "datatable";

	if (!ResolvePackageParams(HoudiniAssetComponent,
		CookedOutput,
		BakeIdentifier,
		OutputObject,
		bHasPreviousBakeData,
		FString(""),
		BakeFolder,
		BakeSettings,
		PackageParams,
		BakedObjectData))
	{
		return nullptr;
	}

	UDataTable* CookedDataTable = Cast<UDataTable>(OutputObject.OutputObject);

	UDataTable* BakedDataTable = static_cast<UDataTable*>(PackageParams.CreateObjectAndPackageFromClass(UDataTable::StaticClass()));

	BakedDataTable->PreEditChange(nullptr);

	// Get Row Data. Due to type mismatches in Unreal, we need to make a copy of it.
	TMap<FName, const uint8*> ConstMap;
	auto& RowMap = CookedDataTable->GetRowMap();
	for (auto It : RowMap)
		ConstMap.Add(It.Key, (const uint8*)It.Value);

	// If no User Defined Struct was specified, use the one from the cooked table.
	UScriptStruct * StructToUse = UserDefinedStruct;
	if (!IsValid(StructToUse))
		StructToUse = (UScriptStruct*)CookedDataTable->GetRowStruct();

	BakedDataTable->CreateTableFromRawData(ConstMap, StructToUse);

	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(BakedDataTable->GetPackage());
	BakedDataTable->MarkPackageDirty();

	BakedOutputObject.BakedObject = BakedDataTable->GetPathName();
	InNewBakedOutput.BakedOutputObjects.Emplace(Identifier, BakedOutputObject);

	return BakedDataTable;
}

bool
FHoudiniEngineBakeUtils::BakeDataTables(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if ((InOutputIndex < 0) || !InAllOutputs.IsValidIndex(InOutputIndex) )
		return false;

	// Get previously cooked output.
	UHoudiniOutput* CookedOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(CookedOutput))
		return false;

	//----------------------------------------------------------------------------------------------------------
	// See if we created a UserDefinedStruct during COOKING. If so, we must create a new version in the Bake folder
	//----------------------------------------------------------------------------------------------------------

	UUserDefinedStruct * BakedUserStruct = nullptr;
	FHoudiniPackageParams PackageParams;

	const FString DefaultObjectName = TEXT("Default");

	for(auto & It : CookedOutput->GetOutputObjects())
	{
		if (!IsValid(It.Value.OutputObject))
			continue;

		if (It.Value.OutputObject->IsA<UUserDefinedStruct>())
		{
			FDirectoryPath BakeFolder = InBakeFolder;
			FString* Attribute = It.Value.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_BAKE_FOLDER);
			if (Attribute != nullptr)
			{
				BakeFolder.Path = *Attribute;
			}

			BakedUserStruct = CreateBakedUserDefinedStruct(
				CookedOutput,
				It.Key,
				HoudiniAssetComponent,
				InBakeState.GetOldBakedOutputs()[InOutputIndex],
				InBakeState.GetNewBakedOutputs()[InOutputIndex],
				BakeFolder,
				BakeSettings,
				BakedObjectData);

			if (!BakedUserStruct)
				return false;

			break;
		}
	}


	//----------------------------------------------------------------------------------------------------------
	// Create a baked copy of the data table. We don't just duplicate the Data Table and change the UUserDefinedStruct
	// because Unreal does not allow this. So we need to actually bake a new table and copy the data over.
	//----------------------------------------------------------------------------------------------------------

	UDataTable* BakedDataTable = nullptr;
	for (auto& It : CookedOutput->GetOutputObjects())
	{
		if (!IsValid(It.Value.OutputObject))
			continue;

		FHoudiniOutputObject & OutputObject = It.Value;

		if (OutputObject.OutputObject->IsA<UDataTable>())
		{
			FDirectoryPath BakeFolder = InBakeFolder;
			FString * Attribute = It.Value.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_BAKE_FOLDER);
			if (Attribute != nullptr)
			{
				BakeFolder.Path = *Attribute;
			}

			FString ObjectName = "";
			if (FString * Value = OutputObject.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
			{
				ObjectName = *Value;
			}
			
			BakedDataTable = CreateBakedDataTable(
				BakedUserStruct,
				ObjectName,
				CookedOutput,
				It.Key,
				HoudiniAssetComponent,
				InBakeState.GetOldBakedOutputs()[InOutputIndex],
				InBakeState.GetNewBakedOutputs()[InOutputIndex],
				BakeFolder,
				BakeSettings,
				BakedObjectData);

			if (!BakedDataTable)
				return false;

			break;
		}
	}

	return true;

		
}

bool
FHoudiniEngineBakeUtils::BakeAnimSequence(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if ((InOutputIndex < 0) || !InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	// Get previously cooked output.
	UHoudiniOutput* CookedOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(CookedOutput))
		return false;


	FHoudiniPackageParams PackageParams;

	const FString DefaultObjectName = TEXT("Default");

	for (auto& It : CookedOutput->GetOutputObjects())
	{
		if (!IsValid(It.Value.OutputObject))
			continue;

		FHoudiniOutputObject& OutputObject = It.Value;

		if (OutputObject.OutputObject->IsA<UAnimSequence>())
		{
			FDirectoryPath BakeFolder = InBakeFolder;
			FString* Attribute = It.Value.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_BAKE_FOLDER);
			if (Attribute != nullptr)
			{
				BakeFolder.Path = *Attribute;
			}

			FString ObjectName = "";
			if (FString* Value = OutputObject.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
			{
				ObjectName = *Value;
			}

			UAnimSequence * BakedAnimSequence = CreateBakedAnimSequence(
				ObjectName,
				CookedOutput,
				It.Key,
				HoudiniAssetComponent,
				InBakeState.GetOldBakedOutputs()[InOutputIndex],
				InBakeState.GetNewBakedOutputs()[InOutputIndex],
				BakeFolder,
				BakeSettings,
				BakedObjectData);

			if (!BakedAnimSequence)
				return false;

			break;
		}
	}

	return true;
}

UAnimSequence * FHoudiniEngineBakeUtils::CreateBakedAnimSequence(
	const FString& ObjectName,
	UHoudiniOutput* CookedOutput,
	const FHoudiniOutputObjectIdentifier& Identifier,
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniBakedOutput& InPreviousBakedOutput,
	FHoudiniBakedOutput& InNewBakedOutput,
	const FDirectoryPath& BakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniBakedObjectData& BakedObjectData)
{
	FHoudiniOutputObject& OutputObject = CookedOutput->GetOutputObjects().FindOrAdd(Identifier);
	FHoudiniBakedOutputObject BakedOutputObject;
	const bool bHasPreviousBakeData = InPreviousBakedOutput.BakedOutputObjects.Contains(Identifier);
	if (bHasPreviousBakeData)
		BakedOutputObject = InPreviousBakedOutput.BakedOutputObjects.FindChecked(Identifier);

	FHoudiniPackageParams PackageParams;

	FHoudiniOutputObjectIdentifier BakeIdentifier = Identifier;
	BakeIdentifier.SplitIdentifier = "anim";

	if (!ResolvePackageParams(HoudiniAssetComponent,
		CookedOutput,
		BakeIdentifier,
		OutputObject,
		bHasPreviousBakeData,
		FString(""),
		BakeFolder,
		BakeSettings,
		PackageParams,
		BakedObjectData))
	{
		return nullptr;
	}

	UAnimSequence * CookedAnimSequence = Cast<UAnimSequence>(OutputObject.OutputObject);

	// Create the package for the object
	FString NewObjectName;
	UPackage* Package = PackageParams.CreatePackageForObject(NewObjectName);
	if (!IsValid(Package))
		return nullptr;

	if (!Package->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!Package->GetOuter())
		{
			Package->FullyLoad();
		}
		else
		{
			Package->GetOutermost()->FullyLoad();
		}
	}
	UAnimSequence* BakedAnimSequence = Cast<UAnimSequence>(DuplicateObject(CookedAnimSequence, Package, *NewObjectName));

	//BakedAnimSequence->PreEditChange(nullptr);

	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(BakedAnimSequence->GetPackage());
	BakedAnimSequence->MarkPackageDirty();

	BakedOutputObject.BakedObject = BakedAnimSequence->GetPathName();
	InNewBakedOutput.BakedOutputObjects.Emplace(Identifier, BakedOutputObject);

	return BakedAnimSequence;
}


bool FHoudiniEngineBakeUtils::BakeGeometryCollectionOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex, 
	const TArray<UHoudiniOutput*>& InAllOutputs, 
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder, 
	const FDirectoryPath& InTempCookFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, 
	UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	AActor* InFallbackActor, 
	const FString& InFallbackWorldOutlinerFolder)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	FString HoudiniAssetActorName = IsValid(OwnerActor) ? OwnerActor->GetActorNameOrLabel() : FString();

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();

	FString GCOutputName = InOutput->GetName();
	
	// Map from old static mesh map to new baked static mesh
	TMap<FSoftObjectPath, UStaticMesh*> OldToNewStaticMeshMap;
	TMap<UMaterialInterface*, UMaterialInterface*> OldToNewMaterialMap;

	// Need to make sure that all geometry collection meshes are generated before we generate the geometry collection.
	int32 NumOutputs = InAllOutputs.Num();
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		UHoudiniOutput *  Output = InAllOutputs[OutputIdx];
		if (!FHoudiniGeometryCollectionTranslator::IsGeometryCollectionMesh(Output))
			continue;

		for (auto Pair : Output->GetOutputObjects())
		{
			const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
			const FHoudiniOutputObject& OutputObject = Pair.Value;

			if (!OutputObject.GeometryCollectionPieceName.IsEmpty() && OutputObject.GeometryCollectionPieceName != GCOutputName)
			{
				continue;
			}

			UStaticMesh * StaticMesh = Cast<UStaticMesh>(OutputObject.OutputObject);
			if (!IsValid(StaticMesh))
				continue;

			// Add a new baked output object entry and update it with the previous bake's data, if available
			bool bHasPreviousBakeData = false;
			FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(OutputIdx, Identifier, bHasPreviousBakeData);
			
			const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);

			FHoudiniPackageParams PackageParams;
			
			if (!ResolvePackageParams(
				HoudiniAssetComponent,
				InOutput,
				Identifier,
				OutputObject,
				bHasPreviousBakeData,
				DefaultObjectName,
				InBakeFolder,
				BakeSettings,
				PackageParams,
				BakedObjectData))
			{
				continue;
			}

			UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
				StaticMesh,
				Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid()),
				PackageParams,
				InAllOutputs,
				InBakedActors,
				InTempCookFolder.Path,
				BakedObjectData,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap);

			if (!IsValid(BakedSM))
				continue;

			BakedOutputObject.BakedObject = FSoftObjectPath(BakedSM).ToString();

			// If we are baking in replace mode, remove previously baked components/instancers
			if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
			{
				const bool bInDestroyBakedComponent = false;
				const bool bInDestroyBakedInstancedActors = true;
				const bool bInDestroyBakedInstancedComponents = true;
				DestroyPreviousBakeOutput(
					BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
			}

			OldToNewStaticMeshMap.Add(FSoftObjectPath(StaticMesh), BakedSM);

			const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
			const TArray<FStaticMaterial>& BakedStaticMaterials = BakedSM->GetStaticMaterials();
			for (int32 i = 0; i < StaticMaterials.Num(); i++)
			{
				if (i >= BakedStaticMaterials.Num())
					continue;

				OldToNewMaterialMap.Add(StaticMaterials[i].MaterialInterface, BakedStaticMaterials[i].MaterialInterface);
			}

			// Update baked output object entry in state
			InBakeState.SetNewBakedOutputObject(OutputIdx, Identifier, BakedOutputObject);
		}
	}

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		const FHoudiniOutputObject& OutputObject = Pair.Value;

		// Add a new baked output object entry and update it with the previous bake's data, if available
		bool bHasPreviousBakeData = false;
		FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, Identifier, bHasPreviousBakeData);

		if (OutputObject.OutputActors.IsEmpty())
			continue;

		AGeometryCollectionActor* GeometryCollectionActor = Cast<AGeometryCollectionActor>(OutputObject.OutputActors[0].Get());
		if (!IsValid(GeometryCollectionActor))
			return false;

		UGeometryCollectionComponent * GeometryCollectionComponent = GeometryCollectionActor->GeometryCollectionComponent;
		if (!IsValid(GeometryCollectionComponent))
			return false;

		FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionActor->GetGeometryCollectionComponent()->EditRestCollection(GeometryCollection::EEditUpdate::RestPhysicsDynamic);
		UGeometryCollection* InGeometryCollection = GeometryCollectionEdit.GetRestCollection();

		if (!IsValid(InGeometryCollection))
			return false;
		
		
		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		FindHGPO(Identifier, HGPOs, FoundHGPO);

		// We do not bake templated geos
		if (FoundHGPO && FoundHGPO->bIsTemplated)
			continue;

		const FString DefaultObjectName = HoudiniAssetActorName + Identifier.SplitIdentifier;

		UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
		ULevel* DesiredLevel = DesiredWorld->GetCurrentLevel();

		FHoudiniPackageParams PackageParams;

		FHoudiniAttributeResolver Resolver;

		if (!ResolvePackageParamsWithResolver(
			HoudiniAssetComponent,
			InOutput,
			Identifier,
			OutputObject,
			bHasPreviousBakeData,
			DefaultObjectName,
			InBakeFolder,
			BakeSettings,
			PackageParams,
			Resolver,
			BakedObjectData))
		{
			continue;
		}

		FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			Resolver,
			FName(InFallbackWorldOutlinerFolder.IsEmpty() ? PackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));
		
		// Bake the static mesh if it is still temporary
		UGeometryCollection* BakedGC = FHoudiniEngineBakeUtils::DuplicateGeometryCollectionAndCreatePackageIfNeeded(
			InGeometryCollection,
			Cast<UGeometryCollection>(BakedOutputObject.GetBakedObjectIfValid()),
			PackageParams,
			InAllOutputs,
			AllBakedActors,
			InTempCookFolder.Path,
			OldToNewStaticMeshMap,
			OldToNewMaterialMap,
			BakedObjectData);

		if (!IsValid(BakedGC))
			continue;

		// Record the baked object
		BakedOutputObject.BakedObject = FSoftObjectPath(BakedGC).ToString();

		// Make sure we have a level to spawn to
		if (!IsValid(DesiredLevel))
			continue;

		// Try to find the unreal_bake_actor, if specified
		FName BakeActorName;
		AActor* FoundActor = nullptr;
		bool bHasBakeActorName = false;
		FindUnrealBakeActor(OutputObject, BakedOutputObject, AllBakedActors, DesiredLevel, *(PackageParams.ObjectName), BakeSettings, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName);

		AGeometryCollectionActor* NewGCActor = nullptr;
		UGeometryCollectionComponent* NewGCC = nullptr;
		if (!FoundActor)
		{

			FoundActor = FHoudiniGeometryCollectionTranslator::CreateNewGeometryActor(DesiredWorld, BakeActorName.ToString(), GeometryCollectionComponent->GetComponentTransform());
			// Spawn the new actor
			if (!IsValid(FoundActor))
				continue;

			BakedObjectData.BakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);

			// Copy properties to new actor
			NewGCActor = Cast<AGeometryCollectionActor>(FoundActor);
			if (!IsValid(NewGCActor))
				continue;

			NewGCC = NewGCActor->GetGeometryCollectionComponent();
		}
		else
		{
			if (BakeSettings.bReplaceAssets)
			{
				// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
				UGeometryCollectionComponent* PrevGCC = Cast<UGeometryCollectionComponent>(BakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(PrevGCC) && (PrevGCC->GetOwner() == FoundActor))
				{
					NewGCC = PrevGCC;
				}
			}

			const bool bCreateIfMissing = true;
			USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);

			if (!IsValid(NewGCC))
			{
				// Create a new static mesh component on the existing actor
				NewGCC = NewObject<UGeometryCollectionComponent>(FoundActor, NAME_None, RF_Transactional);

				FoundActor->AddInstanceComponent(NewGCC);
				if (IsValid(RootComponent))
					NewGCC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				else
					FoundActor->SetRootComponent(NewGCC);
				NewGCC->RegisterComponent();
			}

			NewGCActor = Cast<AGeometryCollectionActor>(FoundActor);

			BakedObjectData.BakeStats.NotifyObjectsUpdated(FoundActor->GetClass()->GetName(), 1);
		}


		if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
		{
			// Parent to an actor
			FHoudiniAttributeResolver OutResolver;
			TMap<FString, FString> Tokens = OutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(HoudiniAssetComponent->GetWorld(), HoudiniAssetComponent, Tokens);
			OutResolver.SetTokensFromStringMap(Tokens);
			FName ParentActorName = FName(OutResolver.ResolveString(BakeSettings.DefaultBakeName));
			AActor* FoundParent = Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), DesiredLevel, ParentActorName));

			if (!IsValid(FoundParent))
			{
				// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
				TSubclassOf<AActor> BakeActorClass = nullptr;
				UActorFactory* ActorFactory = GetActorFactory(OutputObject, BakeSettings, BakeActorClass, UActorFactoryEmptyActor::StaticClass());
				if (!ActorFactory)
				{
					return false;
				}

				constexpr UObject* AssetToSpawn = nullptr;
				constexpr EObjectFlags ObjectFlags = RF_Transactional;
				ParentActorName = *MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), ParentActorName.ToString());

				FActorSpawnParameters SpawnParam;
				SpawnParam.ObjectFlags = ObjectFlags;
				SpawnParam.Name = ParentActorName;
				FoundParent = SpawnBakeActor(ActorFactory, AssetToSpawn, DesiredLevel, BakeSettings, HoudiniAssetComponent->GetComponentTransform(), HoudiniAssetComponent, BakeActorClass, SpawnParam);
				FoundParent->SetActorLabel(ParentActorName.ToString());
			}

			FoundActor->AttachToActor(FoundParent, FAttachmentTransformRules::KeepWorldTransform);
		}

		// We need to make a unique name for the actor, renaming an object on top of another is a fatal error
		FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, AGeometryCollectionActor::StaticClass(), BakeActorName.ToString(), FoundActor);
		RenameAndRelabelActor(FoundActor, NewNameStr, false);
		SetOutlinerFolderPath(FoundActor, WorldOutlinerFolderPath);

		if (IsValid(NewGCC))
		{
			const bool bCopyWorldTransform = true;
			CopyPropertyToNewGeometryCollectionActorAndComponent(NewGCActor, NewGCC, GeometryCollectionComponent, bCopyWorldTransform);
			
			NewGCC->SetRestCollection(BakedGC);
			BakedOutputObject.BakedComponent = FSoftObjectPath(NewGCC).ToString();
		}
		
		BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		const FHoudiniEngineBakedActor& BakedActorEntry = AllBakedActors.Add_GetRef(FHoudiniEngineBakedActor(
			FoundActor, BakeActorName, WorldOutlinerFolderPath, InOutputIndex, Identifier, BakedGC, InGeometryCollection, GeometryCollectionComponent,
			PackageParams.BakeFolder, PackageParams));
		NewBakedActors.Add(BakedActorEntry);

		// If we are baking in replace mode, remove previously baked components/instancers
		if (BakeSettings.bReplaceActors && BakeSettings.bReplaceAssets)
		{
			const bool bInDestroyBakedComponent = false;
			const bool bInDestroyBakedInstancedActors = true;
			const bool bInDestroyBakedInstancedComponents = true;
			DestroyPreviousBakeOutput(BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
		}

		// Update baked output object entry in state
		InBakeState.SetNewBakedOutputObject(InOutputIndex, Identifier, BakedOutputObject);
	}

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FDirectoryPath& InBakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder) 
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;
	
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;
	
	UHoudiniOutput* const Output = InAllOutputs[InOutputIndex];
	if (!IsValid(Output))
		return false;

	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	const FString HoudiniAssetActorName = IsValid(OwnerActor) ? OwnerActor->GetActorNameOrLabel() : FString();

	FHoudiniBakedObjectData BakedObjectData;
	
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	
	const TArray<FHoudiniGeoPartObject> & HGPOs = Output->GetHoudiniGeoPartObjects();

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	for (auto & Pair : OutputObjects) 
	{
		FHoudiniOutputObject& OutputObject = Pair.Value;

		if (OutputObject.OutputComponents.IsEmpty())
			continue;

		HOUDINI_CHECK_RETURN(OutputObject.OutputComponents.Num() == 1, false);

		USplineComponent* SplineComponent = Cast<USplineComponent>(OutputObject.OutputComponents[0]);
		if (!IsValid(SplineComponent))
			continue;
		
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		bool bHasPreviousBakeData = false;
		FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, Identifier, bHasPreviousBakeData);

		// TODO: FIX ME!! May not work 100%
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		for (auto & NextHGPO : HGPOs)
		{
			if (Identifier.GeoId == NextHGPO.GeoId &&
				Identifier.ObjectId == NextHGPO.ObjectId &&
				Identifier.PartId == NextHGPO.PartId)
			{
				FoundHGPO = &NextHGPO;
				break;
			}
		}

		if (!FoundHGPO)
			continue;

		const FString DefaultObjectName = HoudiniAssetActorName + "_" + SplineComponent->GetName();

		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ? EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

		// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
		// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
		FHoudiniAttributeResolver Resolver;
		UWorld* const DesiredWorld = SplineComponent ? SplineComponent->GetWorld() : GWorld;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, Identifier, OutputObject, bHasPreviousBakeData, DefaultObjectName,
			PackageParams, Resolver, InBakeFolder.Path, AssetPackageReplaceMode);

		FHoudiniEngineBakedActor OutputBakedActor;
		BakeCurve(
			HoudiniAssetComponent, OutputObject, BakedOutputObject, PackageParams, Resolver, BakeSettings,
			AllBakedActors, OutputBakedActor, BakedObjectData, InFallbackActor, InFallbackWorldOutlinerFolder);

		OutputBakedActor.OutputIndex = InOutputIndex;
		OutputBakedActor.OutputObjectIdentifier = Identifier;

		// Don't forget to copy the tags to the curve's actor
		if (FoundHGPO && IsValid(OutputBakedActor.Actor))
		{
			FHoudiniEngineUtils::KeepOrClearActorTags(OutputBakedActor.Actor, true, false, FoundHGPO);
			// Add actor tags from generic property attributes
			FHoudiniEngineUtils::ApplyTagsToActorOnly(FoundHGPO->GenericPropertyAttributes, OutputBakedActor.Actor->Tags);
		}

		AllBakedActors.Add(OutputBakedActor);
		NewBakedActors.Add(OutputBakedActor);

		// Update the cached bake output results
		InBakeState.SetNewBakedOutputObject(InOutputIndex, Identifier, BakedOutputObject);
	}

	OutActors = MoveTemp(NewBakedActors);
	
	SaveBakedPackages(BakedObjectData.PackagesToSave);

	return true;
}

bool 
FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(AActor * InActor, UBlueprint * OutBlueprint, bool bInRenameComponentsWithInvalidNames) 
{
	if (!IsValid(InActor))
		return false;

	if (!IsValid(OutBlueprint))
		return false;

	if (bInRenameComponentsWithInvalidNames)
	{
		for (UActorComponent* const Comp : InActor->GetInstanceComponents())
		{
			if (!IsValid(Comp))
				continue;

			// If the component name would not be a valid variable name in the BP, rename it
			if (!FComponentEditorUtils::IsValidVariableNameString(Comp, Comp->GetName()))
			{
				FString NewComponentName = FComponentEditorUtils::GenerateValidVariableName(Comp->GetClass(), Comp->GetOwner());
				NewComponentName = MakeUniqueObjectNameIfNeeded(Comp->GetOuter(), Comp->GetClass(), NewComponentName, Comp);
				if (NewComponentName != Comp->GetName())
					Comp->Rename(*NewComponentName);
			}
		}
	}

	if (InActor->GetInstanceComponents().Num() > 0)
		FKismetEditorUtilities::AddComponentsToBlueprint(
			OutBlueprint,
			InActor->GetInstanceComponents());

	if (OutBlueprint->GeneratedClass)
	{
		AActor * CDO = Cast< AActor >(OutBlueprint->GeneratedClass->GetDefaultObject());
		if (!IsValid(CDO))
			return false;

		const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)
			(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
				EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances);

		EditorUtilities::CopyActorProperties(InActor, CDO, CopyOptions);

		USceneComponent * Scene = CDO->GetRootComponent();
		if (IsValid(Scene))
		{
			Scene->SetRelativeLocation(FVector::ZeroVector);
			Scene->SetRelativeRotation(FRotator::ZeroRotator);

			// Clear out the attachment info after having copied the properties from the source actor
			Scene->SetupAttachment(nullptr);
			while (true)
			{
				const int32 ChildCount = Scene->GetAttachChildren().Num();
				if (ChildCount < 1)
					break;

				USceneComponent * Component = Scene->GetAttachChildren()[ChildCount - 1];
				if (IsValid(Component))
					Component->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			}
			check(Scene->GetAttachChildren().Num() == 0);

			// Ensure the light mass information is cleaned up
			Scene->InvalidateLightingCache();

			// Copy relative scale from source to target.
			if (USceneComponent* SrcSceneRoot = InActor->GetRootComponent())
			{
				Scene->SetRelativeScale3D_Direct(SrcSceneRoot->GetRelativeScale3D());
			}
		}
	}

	// Compile our blueprint and notify asset system about blueprint.
	FBlueprintEditorUtils::MarkBlueprintAsModified(OutBlueprint);
	//FKismetEditorUtilities::CompileBlueprint(OutBlueprint);
	//FAssetRegistryModule::AssetCreated(OutBlueprint);

	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeBlueprints(UHoudiniAssetComponent* HoudiniAssetComponent, const FHoudiniBakeSettings& BakeSettings)
{
	FHoudiniBakedObjectData BakedObjectData;
	const bool bSuccess = BakeBlueprints(HoudiniAssetComponent, BakeSettings, BakedObjectData);
	if (!bSuccess)
	{
		HOUDINI_LOG_WARNING(TEXT("Errors while baking to blueprints."));
	}

	// Compile the new/updated blueprints
	for (UBlueprint* const Blueprint : BakedObjectData.Blueprints)
	{
		if (!IsValid(Blueprint))
			continue;
		
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && BakedObjectData.Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(BakedObjectData.Blueprints.Num());
		for (UBlueprint* Blueprint : BakedObjectData.Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakedObjectData.BakeStats.NumPackagesCreated, BakedObjectData.BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// Broadcast that the bake is complete
	HoudiniAssetComponent->HandleOnPostBake(bSuccess);

	return bSuccess;
}

bool 
FHoudiniEngineBakeUtils::BakeBlueprints(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniBakedObjectData& BakedObjectData)
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	const bool bIsOwnerActorValid = IsValid(OwnerActor);
	
	// Don't process outputs that are not supported in blueprints
	TArray<EHoudiniOutputType> OutputsToBake = {
		EHoudiniOutputType::Mesh,
		EHoudiniOutputType::Instancer,
		EHoudiniOutputType::Curve,
		EHoudiniOutputType::GeometryCollection
	};
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake = {
		EHoudiniInstancerComponentType::StaticMeshComponent,
		EHoudiniInstancerComponentType::InstancedStaticMeshComponent,
		EHoudiniInstancerComponentType::MeshSplitInstancerComponent,
		EHoudiniInstancerComponentType::FoliageAsHierarchicalInstancedStaticMeshComponent,
		EHoudiniInstancerComponentType::GeometryCollectionComponent/*,
		EHoudiniInstancerComponentType::InstancedActorComponent*/
	};


	// When baking blueprints we always create new actors since they are deleted from the world once copied into the
	// blueprint
	FHoudiniBakeSettings ActorBakeSettings = BakeSettings;
	ActorBakeSettings.bReplaceActors  = false;

	TArray<FHoudiniEngineBakedActor> TempActors;
	bool bBakeSuccess = BakeHDAToActors(
		HoudiniAssetComponent,
		ActorBakeSettings,
		TempActors,
		BakedObjectData,
		&OutputsToBake,
		&InstancerComponentTypesToBake);
	if (!bBakeSuccess)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not create output actors for baking to blueprint."));
		return false;
	}

	// Get the previous baked outputs
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();

	bBakeSuccess = BakeBlueprintsFromBakedActors(
		TempActors,
		BakeSettings,
		HoudiniAssetComponent->GetHoudiniAssetName(), 
		bIsOwnerActorValid ? OwnerActor->GetActorNameOrLabel() : FString(),
		HoudiniAssetComponent->BakeFolder,
		&BakedOutputs,
		nullptr,
		BakedObjectData);

	return bBakeSuccess;
}

UStaticMesh* 
FHoudiniEngineBakeUtils::BakeStaticMesh(
	UStaticMesh * StaticMesh,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FDirectoryPath& InTempCookFolder,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap) 
{
	if (!IsValid(StaticMesh))
		return nullptr;

	FHoudiniBakedObjectData BakedObjectData;
	TArray<UHoudiniOutput*> Outputs;
	const TArray<FHoudiniEngineBakedActor> BakedResults;
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, nullptr, PackageParams, InAllOutputs, BakedResults, InTempCookFolder.Path, BakedObjectData,
		InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap);

	if (BakedStaticMesh) 
	{
		FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

		// Sync the CB to the baked objects
		if(GEditor)
		{
			TArray<UObject*> Objects;
			Objects.Add(BakedStaticMesh);
			GEditor->SyncBrowserToObjects(Objects);
		}
	}

	return BakedStaticMesh;
}

UFoliageType* 
FHoudiniEngineBakeUtils::DuplicateFoliageTypeAndCreatePackageIfNeeded(
	UFoliageType* InFoliageType,
	UFoliageType* InPreviousBakeFoliageType,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakeResults,
	const FString& InTemporaryCookFolder,
	TMap<UFoliageType*, UFoliageType*>& InOutAlreadyBakedFoliageTypes,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
	const TArray<FHoudiniEngineBakedActor>& BakedResults,
	FHoudiniBakedObjectData& BakedObjectData)
{
	HOUDINI_CHECK_RETURN(IsValid(InFoliageType), nullptr);

	// The not a temporary one/already baked, we can simply reuse it
    // instead of duplicating it.

	const bool bIsTemporary = IsObjectTemporary(InFoliageType, EHoudiniOutputType::Instancer, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID);
	if (!bIsTemporary)
	{
		return InFoliageType;
	}

	// See if we already baked this one. If so, use it.

	UFoliageType ** AlreadyBaked = InOutAlreadyBakedFoliageTypes.Find(InFoliageType);
	if (AlreadyBaked && IsValid(*AlreadyBaked))
		return *AlreadyBaked;

    // Look to see if its in the baked results.

	for (const FHoudiniEngineBakedActor& BakeResult : BakedResults)
	{
		if (BakeResult.SourceObject == InFoliageType && IsValid(BakeResult.BakedObject)
			&& BakeResult.BakedObject->IsA(InFoliageType->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<UFoliageType>(BakeResult.BakedObject);
		}
	}

    // Not previously baked, so make a copy of the cooked asset.

	// If we have a previously baked object, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeObjectValid = IsValid(InPreviousBakeFoliageType);
	TArray<UMaterialInterface*> PreviousBakeMaterials;
	if (bPreviousBakeObjectValid)
	{
		bPreviousBakeObjectValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeFoliageType);
		if (bPreviousBakeObjectValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeFoliageType, BakeCounter);
			UFoliageType_InstancedStaticMesh const* const PreviousBakeMeshFoliageType = Cast<UFoliageType_InstancedStaticMesh>(InPreviousBakeFoliageType);
			if (IsValid(PreviousBakeMeshFoliageType))
				PreviousBakeMaterials = PreviousBakeMeshFoliageType->OverrideMaterials;
		}
	}

	FString CreatedPackageName;
	UPackage* Package = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	HOUDINI_CHECK_RETURN(IsValid(Package), nullptr);

    BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(Package);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!Package->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!Package->GetOuter())
		{
			Package->FullyLoad();
		}
		else
		{
			Package->GetOutermost()->FullyLoad();
		}
	}

	// Duplicate the foliage type.

	UFoliageType* DuplicatedFoliageType = nullptr;
	UFoliageType* ExistingFoliageType = FindObject<UFoliageType>(Package, *CreatedPackageName);
	bool bFoundExisting= false;
	if (IsValid(ExistingFoliageType))
	{
		bFoundExisting = true;
		DuplicatedFoliageType = DuplicateObject<UFoliageType>(InFoliageType, Package, *CreatedPackageName);
		BakedObjectData.BakeStats.NotifyObjectsReplaced(UFoliageType::StaticClass()->GetName(), 1);
		BakedObjectData.BakeStats.NotifyPackageCreated(1);
	}
	else
	{
		DuplicatedFoliageType = DuplicateObject<UFoliageType>(InFoliageType, Package, *CreatedPackageName);
		BakedObjectData.BakeStats.NotifyObjectsUpdated(UFoliageType::StaticClass()->GetName(), 1);
		BakedObjectData.BakeStats.NotifyPackageUpdated(2);
	}

	if (!IsValid(DuplicatedFoliageType))
		return nullptr;

	InOutAlreadyBakedFoliageTypes.Add(InFoliageType, DuplicatedFoliageType);

	// Add meta information.
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(Package, DuplicatedFoliageType, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(Package, DuplicatedFoliageType, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(Package, DuplicatedFoliageType, HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));
	
	// See if we need to duplicate materials and textures.
	UFoliageType_InstancedStaticMesh* const DuplicatedMeshFoliageType = Cast<UFoliageType_InstancedStaticMesh>(DuplicatedFoliageType);
	const bool bIsMeshFoliageType = IsValid(DuplicatedMeshFoliageType);
	TArray<UMaterialInterface*> DuplicatedMaterials;
	TArray<UMaterialInterface*> Materials;
	if (bIsMeshFoliageType)
		Materials = DuplicatedMeshFoliageType->OverrideMaterials; 
	for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* const MaterialInterface = Materials[MaterialIdx];

		// Only duplicate the material if it is temporary
		if (IsValid(MaterialInterface) && IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID))
		{
			UPackage * MaterialPackage = Cast<UPackage>(MaterialInterface->GetOuter());
			if (IsValid(MaterialPackage))
			{
				FString MaterialName;
				if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
					Package, DuplicatedFoliageType, MaterialName))
				{
					MaterialName = MaterialName + "_Material" + FString::FromInt(MaterialIdx + 1);

					// We only deal with materials.
					if (!MaterialInterface->IsA(UMaterial::StaticClass()) && !MaterialInterface->IsA(UMaterialInstance::StaticClass()))
					{
						continue;
					}
					
					UMaterialInterface * Material = MaterialInterface;

					if (IsValid(Material))
					{
						// Look for a previous bake material at this index
						UMaterialInterface* PreviousBakeMaterial = nullptr;
						if (bPreviousBakeObjectValid && PreviousBakeMaterials.IsValidIndex(MaterialIdx))
						{
							PreviousBakeMaterial = PreviousBakeMaterials[MaterialIdx];
						}
						// Duplicate material resource.
						UMaterialInterface * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
							Material, PreviousBakeMaterial, MaterialName, PackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);

						if (!IsValid(DuplicatedMaterial))
							continue;

						// Store duplicated material.
						DuplicatedMaterials.Add(DuplicatedMaterial);
						continue;
					}
				}
			}
		}
		
		// We can simply reuse the source material
		DuplicatedMaterials.Add(Materials[MaterialIdx]);
	}
		
	// Assign duplicated materials.
	if (bIsMeshFoliageType)
		DuplicatedMeshFoliageType->OverrideMaterials = DuplicatedMaterials;

	if (!bFoundExisting)
		FAssetRegistryModule::AssetCreated(DuplicatedFoliageType);

	DuplicatedFoliageType->MarkPackageDirty();

	return DuplicatedFoliageType;
}


/*
UStaticMesh*
FHoudiniEngineBakeUtils::DuplicateStaticMesh(UStaticMesh* SourceStaticMesh, UObject* Outer, const FName Name)
{
	//
	// Copied from FDatasmithImporterUtils::DuplicateStaticMesh
	// 
	
	// Since static mesh can be quite heavy, remove source models for cloning to reduce useless work.
	// Will be reinserted on the new duplicated asset or restored on the SourceStaticMesh if bIgnoreBulkData is true.
	TArray<FStaticMeshSourceModel> SourceModels = SourceStaticMesh->MoveSourceModels();
	FStaticMeshSourceModel HiResSourceModel = SourceStaticMesh->MoveHiResSourceModel();

	// Temporary flag to skip Postload during DuplicateObject
	SourceStaticMesh->SetFlags(RF_ArchetypeObject);

	// Duplicate is used only to move our object from its temporary package into its final package replacing any asset
	// already at that location. This function also takes care of fixing internal dependencies among the object's children.
	// Since Duplicate has some rather heavy consequence, like calling PostLoad and doing all kind of stuff on an object
	// that is not even fully initialized yet, we might want to find an alternative way of moving our objects in future
	// releases but keep it for the current release cycle.
	UStaticMesh* DuplicateMesh = ::DuplicateObject<UStaticMesh>(SourceStaticMesh, Outer, Name);

	// Get rid of our temporary flag
	SourceStaticMesh->ClearFlags(RF_ArchetypeObject);
	DuplicateMesh->ClearFlags(RF_ArchetypeObject);
	DuplicateMesh->GetHiResSourceModel().CreateSubObjects(DuplicateMesh);

	// The source mesh is stripped from it's source model, it is not buildable anymore.
	// -> MarkPendingKill to avoid use-after-move crash in the StaticMesh::Build()
	SourceStaticMesh->MarkAsGarbage();

	// Apply source models to the duplicated mesh
	DuplicateMesh->SetSourceModels(MoveTemp(SourceModels));
	DuplicateMesh->SetHiResSourceModel(MoveTemp(HiResSourceModel));

	return DuplicateMesh;
}
*/


UStaticMesh * 
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
	UStaticMesh * InStaticMesh,
	UStaticMesh * InPreviousBakeStaticMesh,
	const FHoudiniPackageParams &PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs, 
	const TArray<FHoudiniEngineBakedActor>& BakedResults,
	const FString& InTemporaryCookFolder,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap) 
{
	if (!IsValid(InStaticMesh))
		return nullptr;

	const bool bIsTemporaryStaticMesh = IsObjectTemporary(InStaticMesh, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID);
	if (!bIsTemporaryStaticMesh)
	{
		// The Static Mesh is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InStaticMesh;
	}

	UStaticMesh** AlreadyBakedSM = InOutAlreadyBakedStaticMeshMap.Find(InStaticMesh);
	if (AlreadyBakedSM && IsValid(*AlreadyBakedSM))
		return *AlreadyBakedSM;

	// Look for InStaticMesh as the SourceObject in BakedResults (it could have already been baked along with
	// a previous output: instancers etc)
	for (const FHoudiniEngineBakedActor& BakeResult : BakedResults)
	{
		if (BakeResult.SourceObject == InStaticMesh && IsValid(BakeResult.BakedObject)
			&& BakeResult.BakedObject->IsA(InStaticMesh->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<UStaticMesh>(BakeResult.BakedObject);
		}
	}

	// InStaticMesh is temporary and we didn't find a baked version of it in our current bake output, we need to bake it
	
	// If we have a previously baked static mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeStaticMeshValid = IsValid(InPreviousBakeStaticMesh);
	TArray<FStaticMaterial> PreviousBakeMaterials;
	if (bPreviousBakeStaticMeshValid)
	{
		bPreviousBakeStaticMeshValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeStaticMesh);
		if (bPreviousBakeStaticMeshValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeStaticMesh, BakeCounter);
			PreviousBakeMaterials = InPreviousBakeStaticMesh->GetStaticMaterials();
		}
	}
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(MeshPackage))
		return nullptr;
	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(MeshPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!MeshPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!MeshPackage->GetOuter())
		{
			MeshPackage->FullyLoad();
		}
		else
		{
			MeshPackage->GetOutermost()->FullyLoad();
		}
	}

	FString ObjectName = PackageParams.ObjectName;
	// If the a UStaticMesh with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	UStaticMesh * DuplicatedStaticMesh = nullptr;
	UStaticMesh* ExistingMesh = FindObject<UStaticMesh>(MeshPackage, *CreatedPackageName);
	bool bFoundExistingMesh = false;
	if (IsValid(ExistingMesh))
	{
		FStaticMeshComponentRecreateRenderStateContext SMRecreateContext(ExistingMesh);	
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(InStaticMesh, MeshPackage, *CreatedPackageName);
		//DuplicatedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMesh(InStaticMesh, MeshPackage, *CreatedPackageName);
		bFoundExistingMesh = true;
		BakedObjectData.BakeStats.NotifyObjectsReplaced(UStaticMesh::StaticClass()->GetName(), 1);
	}
	else
	{
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(InStaticMesh, MeshPackage, *CreatedPackageName);
		//DuplicatedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMesh(InStaticMesh, MeshPackage, *CreatedPackageName);
		BakedObjectData.BakeStats.NotifyObjectsUpdated(UStaticMesh::StaticClass()->GetName(), 1);
	}
	
	if (!IsValid(DuplicatedStaticMesh))
		return nullptr;

	InOutAlreadyBakedStaticMeshMap.Add(InStaticMesh, DuplicatedStaticMesh);

	// Add meta information.
	// Houdini Generated
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	// Houdini Generated Name
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);
	// Baked object! this is not temporary anymore
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

	// See if we need to duplicate materials and textures.
	TArray<FStaticMaterial>DuplicatedMaterials;
	TArray<FStaticMaterial> Materials;
	if(InStaticMesh->GetStaticMaterials().Num() > 0)
		Materials = InStaticMesh->GetStaticMaterials();

	for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = Materials[MaterialIdx].MaterialInterface;
		if (!IsValid(MaterialInterface))
			continue;

		// Only duplicate the material if it is temporary
		if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID))
		{
			UPackage * MaterialPackage = Cast<UPackage>(MaterialInterface->GetOuter());
			if (IsValid(MaterialPackage))
			{
				FString MaterialName;
				if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
					MeshPackage, DuplicatedStaticMesh, MaterialName))
				{
					MaterialName = MaterialName + "_Material" + FString::FromInt(MaterialIdx + 1);

					// We only deal with materials.
					if (!MaterialInterface->IsA(UMaterial::StaticClass()) && !MaterialInterface->IsA(UMaterialInstance::StaticClass()))
					{
						continue;
					}
					
					UMaterialInterface * Material = MaterialInterface;

					if (IsValid(Material))
					{
						// Look for a previous bake material at this index
						UMaterialInterface* PreviousBakeMaterial = nullptr;
						if (bPreviousBakeStaticMeshValid && PreviousBakeMaterials.IsValidIndex(MaterialIdx))
						{
							PreviousBakeMaterial = Cast<UMaterialInterface>(PreviousBakeMaterials[MaterialIdx].MaterialInterface);
						}
						// Duplicate material resource.
						UMaterialInterface * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
							Material, PreviousBakeMaterial, MaterialName, PackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);

						if (!IsValid(DuplicatedMaterial))
							continue;

						// Store duplicated material.
						FStaticMaterial DupeStaticMaterial = Materials[MaterialIdx];
						DupeStaticMaterial.MaterialInterface = DuplicatedMaterial;
						DuplicatedMaterials.Add(DupeStaticMaterial);
						continue;
					}
				}
			}
		}
		
		// We can simply reuse the source material
		DuplicatedMaterials.Add(Materials[MaterialIdx]);
	}
		
	// Assign duplicated materials.
	DuplicatedStaticMesh->SetStaticMaterials(DuplicatedMaterials);

	// Check if the complex collision mesh of the SM is a temporary SM, if so try to get its baked version
	if (IsValid(DuplicatedStaticMesh->ComplexCollisionMesh) &&
			IsObjectTemporary(DuplicatedStaticMesh->ComplexCollisionMesh, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder))
	{
		UStaticMesh** BakedComplexCollisionMesh = InOutAlreadyBakedStaticMeshMap.Find(DuplicatedStaticMesh->ComplexCollisionMesh);
		if (BakedComplexCollisionMesh && IsValid(*BakedComplexCollisionMesh))
		{
			DuplicatedStaticMesh->ComplexCollisionMesh = *BakedComplexCollisionMesh;
		}
	}

	// Notify registry that we have created a new duplicate mesh.
	if (!bFoundExistingMesh)
		FAssetRegistryModule::AssetCreated(DuplicatedStaticMesh);

	// Dirty the static mesh package.
	DuplicatedStaticMesh->MarkPackageDirty();

	return DuplicatedStaticMesh;
}

USkeletalMesh*
FHoudiniEngineBakeUtils::DuplicateSkeletalMeshAndCreatePackageIfNeeded(
	USkeletalMesh* InSkeletalMesh,
	USkeletalMesh* InPreviousBakeSkeletalMesh,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
	const FString& InTemporaryCookFolder,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<USkeletalMesh*, USkeletalMesh*>& InOutAlreadyBakedSkeletalMeshMap,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap)
{
	if (!IsValid(InSkeletalMesh))
		return nullptr;

	const bool bIsTemporarySkeletalMesh = IsObjectTemporary(InSkeletalMesh, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID);
	if (!bIsTemporarySkeletalMesh)
	{
		// The Skeletal Mesh is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InSkeletalMesh;
	}

	USkeletalMesh** AlreadyBakedSK = InOutAlreadyBakedSkeletalMeshMap.Find(InSkeletalMesh);
	if (AlreadyBakedSK && IsValid(*AlreadyBakedSK))
		return *AlreadyBakedSK;

	// Look for InSkeletalMesh as the SourceObject in InCurrentBakedActors (it could have already been baked along with
	// a previous output: instancers etc)
	for (const FHoudiniEngineBakedActor& BakedActor : InCurrentBakedActors)
	{
		if (BakedActor.SourceObject == InSkeletalMesh && IsValid(BakedActor.BakedObject)
			&& BakedActor.BakedObject->IsA(InSkeletalMesh->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<USkeletalMesh>(BakedActor.BakedObject);
		}
	}

	// InSkeletalMesh is temporary and we didn't find a baked version of it in our current bake output, we need to bake it

	// If we have a previously baked skeletal mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeStaticMeshValid = IsValid(InPreviousBakeSkeletalMesh);
	TArray<FSkeletalMaterial> PreviousBakeMaterials;
	if (bPreviousBakeStaticMeshValid)
	{
		bPreviousBakeStaticMeshValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeSkeletalMesh);
		if (bPreviousBakeStaticMeshValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeSkeletalMesh, BakeCounter);
			PreviousBakeMaterials = InPreviousBakeSkeletalMesh->GetMaterials();
		}
	}
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(MeshPackage))
		return nullptr;
	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(MeshPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!MeshPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!MeshPackage->GetOuter())
		{
			MeshPackage->FullyLoad();
		}
		else
		{
			MeshPackage->GetOutermost()->FullyLoad();
		}
	}

	// If the a USkeletalMesh with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	USkeletalMesh* DuplicatedSkeletalMesh = nullptr;
	USkeletalMesh* ExistingMesh = FindObject<USkeletalMesh>(MeshPackage, *CreatedPackageName);
	bool bFoundExistingMesh = false;
	if (IsValid(ExistingMesh))
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		//FSkinnedMeshComponentRecreateRenderStateContext SMRecreateContext(ExistingMesh);	
#else
		FSkinnedMeshComponentRecreateRenderStateContext SMRecreateContext(ExistingMesh);
#endif
		DuplicatedSkeletalMesh = DuplicateObject<USkeletalMesh>(InSkeletalMesh, MeshPackage, *CreatedPackageName);
		bFoundExistingMesh = true;
		BakedObjectData.BakeStats.NotifyObjectsReplaced(USkeletalMesh::StaticClass()->GetName(), 1);
	}
	else
	{
		DuplicatedSkeletalMesh = DuplicateObject<USkeletalMesh>(InSkeletalMesh, MeshPackage, *CreatedPackageName);
		BakedObjectData.BakeStats.NotifyObjectsUpdated(USkeletalMesh::StaticClass()->GetName(), 1);
	}

	if (!IsValid(DuplicatedSkeletalMesh))
		return nullptr;

	InOutAlreadyBakedSkeletalMeshMap.Add(InSkeletalMesh, DuplicatedSkeletalMesh);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedSkeletalMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedSkeletalMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);
	// Baked object! this is not temporary anymore
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedSkeletalMesh,
		HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

	// See if we need to duplicate materials and textures.
	TArray<FSkeletalMaterial>DuplicatedMaterials;
	TArray<FSkeletalMaterial>& Materials = DuplicatedSkeletalMesh->GetMaterials();
	for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = Materials[MaterialIdx].MaterialInterface;
		if (!IsValid(MaterialInterface))
			continue;

		// Only duplicate the material if it is temporary
		if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID))
		{
			UPackage* MaterialPackage = Cast<UPackage>(MaterialInterface->GetOuter());
			if (IsValid(MaterialPackage))
			{
				FString MaterialName;
				if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
					MeshPackage, DuplicatedSkeletalMesh, MaterialName))
				{
					MaterialName = MaterialName + "_Material" + FString::FromInt(MaterialIdx + 1);

					// We only deal with materials.
					if (!MaterialInterface->IsA(UMaterial::StaticClass()) && !MaterialInterface->IsA(UMaterialInstance::StaticClass()))
					{
						continue;
					}

					UMaterialInterface* Material = MaterialInterface;

					if (IsValid(Material))
					{
						// Look for a previous bake material at this index
						UMaterialInterface* PreviousBakeMaterial = nullptr;
						if (bPreviousBakeStaticMeshValid && PreviousBakeMaterials.IsValidIndex(MaterialIdx))
						{
							PreviousBakeMaterial = Cast<UMaterialInterface>(PreviousBakeMaterials[MaterialIdx].MaterialInterface);
						}
						// Duplicate material resource.
						UMaterialInterface* DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
							Material, PreviousBakeMaterial, MaterialName, PackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);

						if (!IsValid(DuplicatedMaterial))
							continue;

						// Store duplicated material.
						FSkeletalMaterial DupeStaticMaterial = Materials[MaterialIdx];
						DupeStaticMaterial.MaterialInterface = DuplicatedMaterial;
						DuplicatedMaterials.Add(DupeStaticMaterial);
						continue;
					}
				}
			}
		}

		// We can simply reuse the source material
		DuplicatedMaterials.Add(Materials[MaterialIdx]);
	}

	// Assign duplicated materials.
	DuplicatedSkeletalMesh->SetMaterials(DuplicatedMaterials);

	// Notify registry that we have created a new duplicate mesh.
	if (!bFoundExistingMesh)
		FAssetRegistryModule::AssetCreated(DuplicatedSkeletalMesh);

	// Dirty the static mesh package.
	DuplicatedSkeletalMesh->MarkPackageDirty();

	return DuplicatedSkeletalMesh;
}


USkeleton* FHoudiniEngineBakeUtils::DuplicateSkeletonAndCreatePackageIfNeeded(
	USkeleton* InSkeleton,
	USkeleton const* InPreviousBakeSkeleton,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
	const FString& InTemporaryCookFolder,
	FHoudiniBakedObjectData& BakedObjectData, 
	TMap<USkeleton*, USkeleton*>& InOutAlreadyBakedSkeletonMap)
{
	if (!IsValid(InSkeleton))
		return nullptr;

	const bool bIsTemporarySkeleton = IsObjectTemporary(InSkeleton, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID);
	if (!bIsTemporarySkeleton)
	{
		// The Skeletal Mesh is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InSkeleton;
	}

	USkeleton** AlreadyBakedSK = InOutAlreadyBakedSkeletonMap.Find(InSkeleton);
	if (AlreadyBakedSK && IsValid(*AlreadyBakedSK))
		return *AlreadyBakedSK;

	// // Look for InSkeleton as the SourceObject in InCurrentBakedActors (it could have already been baked along with
	// // a previous output: instancers etc)
	// for (const FHoudiniEngineBakedActor& BakedActor : InCurrentBakedActors)
	// {
	// 	if (BakedActor.SourceObject == InSkeleton && IsValid(BakedActor.BakedObject)
	// 		&& BakedActor.BakedObject->IsA(InSkeleton->GetClass()))
	// 	{
	// 		// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
	// 		// of a compatible class
	// 		return Cast<USkeleton>(BakedActor.BakedObject);
	// 	}
	// }

	// InSkeleton is temporary and we didn't find a baked version of it in our current bake output, we need to bake it

	// If we have a previously baked skeletal mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeSkeletonValid = IsValid(InPreviousBakeSkeleton);
	if (bPreviousBakeSkeletonValid)
	{
		bPreviousBakeSkeletonValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeSkeleton);
		if (bPreviousBakeSkeletonValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeSkeleton, BakeCounter);
		}
	}
	FString CreatedPackageName;
	UPackage* SkeletonPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(SkeletonPackage))
		return nullptr;
	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(SkeletonPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!SkeletonPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!SkeletonPackage->GetOuter())
		{
			SkeletonPackage->FullyLoad();
		}
		else
		{
			SkeletonPackage->GetOutermost()->FullyLoad();
		}
	}

	// If the a USkeleton with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	USkeleton* DuplicatedSkeleton = nullptr;
	USkeleton* ExistingSkeleton = FindObject<USkeleton>(SkeletonPackage, *CreatedPackageName);
	bool bFoundExistingSkeleton = false;
	if (IsValid(ExistingSkeleton))
	{
		DuplicatedSkeleton = DuplicateObject<USkeleton>(InSkeleton, SkeletonPackage, *CreatedPackageName);
		bFoundExistingSkeleton = true;
		BakedObjectData.BakeStats.NotifyObjectsReplaced(USkeleton::StaticClass()->GetName(), 1);
	}
	else
	{
		DuplicatedSkeleton = DuplicateObject<USkeleton>(InSkeleton, SkeletonPackage, *CreatedPackageName);
		BakedObjectData.BakeStats.NotifyObjectsUpdated(USkeleton::StaticClass()->GetName(), 1);
	}

	if (!IsValid(DuplicatedSkeleton))
		return nullptr;

	InOutAlreadyBakedSkeletonMap.Add(InSkeleton, DuplicatedSkeleton);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		SkeletonPackage, DuplicatedSkeleton,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		SkeletonPackage, DuplicatedSkeleton,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);
	// Baked object! this is not temporary anymore
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		SkeletonPackage, DuplicatedSkeleton,
		HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

	// Notify registry that we have created a new duplicate skeleton
	if (!bFoundExistingSkeleton)
		FAssetRegistryModule::AssetCreated(DuplicatedSkeleton);

	// Dirty the skeleton package.
	DuplicatedSkeleton->MarkPackageDirty();

	return DuplicatedSkeleton;
}


UPhysicsAsset* FHoudiniEngineBakeUtils::DuplicatePhysicsAssetAndCreatePackageIfNeeded(
	UPhysicsAsset* InPhysicsAsset,
	UPhysicsAsset * InPreviousBakePhysicsAsset,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
	const FString& InTemporaryCookFolder,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UPhysicsAsset*, UPhysicsAsset*>& InOutAlreadyBakedPhysicsAssetMap)
{
	if (!IsValid(InPhysicsAsset))
		return nullptr;

	// Don't bake a new object if this isn't temporary
	bool bIsTemporary = IsObjectTemporary(InPhysicsAsset, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID);
	if (!bIsTemporary)
	{
		return InPhysicsAsset;
	}

	// Was it already baked?
	UPhysicsAsset** AlreadyBakedPhysicsAsset = InOutAlreadyBakedPhysicsAssetMap.Find(InPhysicsAsset);
	if (AlreadyBakedPhysicsAsset && IsValid(*AlreadyBakedPhysicsAsset))
		return *AlreadyBakedPhysicsAsset;

	// If we have a previously baked object, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeValid = IsValid(InPreviousBakePhysicsAsset);
	if (bPreviousBakeValid)
	{
		bPreviousBakeValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakePhysicsAsset);
		if (bPreviousBakeValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakePhysicsAsset, BakeCounter);
		}
	}
	FString CreatedPackageName;
	UPackage* PhysicsAssetPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(PhysicsAssetPackage))
		return nullptr;
	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(PhysicsAssetPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!PhysicsAssetPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!PhysicsAssetPackage->GetOuter())
		{
			PhysicsAssetPackage->FullyLoad();
		}
		else
		{
			PhysicsAssetPackage->GetOutermost()->FullyLoad();
		}
	}

	// If an object with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	UPhysicsAsset* DuplicatedPhysicsAsset = nullptr;
	UPhysicsAsset* ExistingSkeleton = FindObject<UPhysicsAsset>(PhysicsAssetPackage, *CreatedPackageName);
	bool bFoundExistingPhysicsAsset = false;
	if (IsValid(ExistingSkeleton))
	{
		DuplicatedPhysicsAsset = DuplicateObject<UPhysicsAsset>(InPhysicsAsset, PhysicsAssetPackage, *CreatedPackageName);
		bFoundExistingPhysicsAsset = true;
		BakedObjectData.BakeStats.NotifyObjectsReplaced(USkeleton::StaticClass()->GetName(), 1);
	}
	else
	{
		DuplicatedPhysicsAsset = DuplicateObject<UPhysicsAsset>(InPhysicsAsset, PhysicsAssetPackage, *CreatedPackageName);
		BakedObjectData.BakeStats.NotifyObjectsUpdated(UPhysicsAsset::StaticClass()->GetName(), 1);
	}

	if (!IsValid(DuplicatedPhysicsAsset))
		return nullptr;

	InOutAlreadyBakedPhysicsAssetMap.Add(InPhysicsAsset, DuplicatedPhysicsAsset);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		PhysicsAssetPackage, DuplicatedPhysicsAsset,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		PhysicsAssetPackage, DuplicatedPhysicsAsset,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);
	// Baked object! this is not temporary anymore
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		PhysicsAssetPackage, DuplicatedPhysicsAsset,
		HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

	// Notify registry that we have created a new duplicate skeleton
	if (!bFoundExistingPhysicsAsset)
		FAssetRegistryModule::AssetCreated(DuplicatedPhysicsAsset);

	// Dirty the skeleton package.
	DuplicatedPhysicsAsset->MarkPackageDirty();

	return DuplicatedPhysicsAsset;
}

UGeometryCollection* FHoudiniEngineBakeUtils::DuplicateGeometryCollectionAndCreatePackageIfNeeded(
	UGeometryCollection* InGeometryCollection, 
	UGeometryCollection* InPreviousBakeGeometryCollection,
	const FHoudiniPackageParams& PackageParams, 
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors, 
	const FString& InTemporaryCookFolder,
	const TMap<FSoftObjectPath, UStaticMesh*>& InOldToNewStaticMesh,
	const TMap<UMaterialInterface*, UMaterialInterface*>& InOldToNewMaterialMap,
	FHoudiniBakedObjectData& BakedObjectData)
{
	if (!IsValid(InGeometryCollection))
		return nullptr;

	const bool bIsTemporaryStaticMesh = IsObjectTemporary(InGeometryCollection, EHoudiniOutputType::GeometryCollection, InParentOutputs, InTemporaryCookFolder, PackageParams.ComponentGUID);
	if (!bIsTemporaryStaticMesh)
	{
		// The output is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InGeometryCollection;
	}

	// Look for InGeometryCollection as the SourceObject in InCurrentBakedActors (it could have already been baked along with
	// a previous output: instancers etc)
	for (const FHoudiniEngineBakedActor& BakedActor : InCurrentBakedActors)
	{
		if (BakedActor.SourceObject == InGeometryCollection && IsValid(BakedActor.BakedObject)
			&& BakedActor.BakedObject->IsA(InGeometryCollection->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<UGeometryCollection>(BakedActor.BakedObject);
		}
	}

	// InGeometryCollection is temporary and we didn't find a baked version of it in our current bake output, we need to bake it
	
	// If we have a previously baked static mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter

	
	int32 BakeCounter = 0;
	bool bPreviousBakeStaticMeshValid = IsValid(InPreviousBakeGeometryCollection);
	TArray<UMaterialInterface*> PreviousBakeMaterials;
	if (bPreviousBakeStaticMeshValid)
	{
		bPreviousBakeStaticMeshValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeGeometryCollection);
		if (bPreviousBakeStaticMeshValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeGeometryCollection, BakeCounter);
			PreviousBakeMaterials = InPreviousBakeGeometryCollection->Materials;
		}
	}
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(MeshPackage))
		return nullptr;
	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	BakedObjectData.PackagesToSave.Add(MeshPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!MeshPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!MeshPackage->GetOuter())
		{
			MeshPackage->FullyLoad();
		}
		else
		{
			MeshPackage->GetOutermost()->FullyLoad();
		}
	}

	// If the a UGeometryCollection with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	UGeometryCollection * DuplicatedGeometryCollection = nullptr;
	UGeometryCollection * ExistingGeometryCollection = FindObject<UGeometryCollection>(MeshPackage, *CreatedPackageName);
	bool bFoundExistingObject = false;
	if (IsValid(ExistingGeometryCollection))
	{
		DuplicatedGeometryCollection = DuplicateObject<UGeometryCollection>(InGeometryCollection, MeshPackage, *CreatedPackageName);
		bFoundExistingObject = true;
	}
	else
	{
		DuplicatedGeometryCollection = DuplicateObject<UGeometryCollection>(InGeometryCollection, MeshPackage, *CreatedPackageName);
	}
	
	if (!IsValid(DuplicatedGeometryCollection))
		return nullptr;

	BakedObjectData.BakeStats.NotifyObjectsCreated(DuplicatedGeometryCollection->GetClass()->GetName(), 1);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedGeometryCollection,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedGeometryCollection,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);
	// Baked object! this is not temporary anymore
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedGeometryCollection,
		HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

	for (FGeometryCollectionSource& Source : DuplicatedGeometryCollection->GeometrySource)
	{
		if (!InOldToNewStaticMesh.Contains(Source.SourceGeometryObject))
		{
			continue;
		}

		UStaticMesh * BakedSM = InOldToNewStaticMesh[Source.SourceGeometryObject];
		
		Source.SourceGeometryObject = FSoftObjectPath(BakedSM);
		Source.SourceMaterial.Empty();
		
		for (const auto& Material : BakedSM->GetStaticMaterials())
		{
			Source.SourceMaterial.Add(Material.MaterialInterface);
		}
	}


	// Duplicate geometry collection materials	
	for (int32 i = 0; i < DuplicatedGeometryCollection->Materials.Num(); i++)
	{
		if (!InOldToNewMaterialMap.Contains(DuplicatedGeometryCollection->Materials[i]))
		{
			continue;
		}

		DuplicatedGeometryCollection->Materials[i] = InOldToNewMaterialMap[DuplicatedGeometryCollection->Materials[i]];
	}
		

	// Notify registry that we have created a new duplicate mesh.
	if (!bFoundExistingObject)
		FAssetRegistryModule::AssetCreated(DuplicatedGeometryCollection);

	// Dirty the static mesh package.
	DuplicatedGeometryCollection->MarkPackageDirty();

	return DuplicatedGeometryCollection;
}

ALandscapeProxy* 
FHoudiniEngineBakeUtils::BakeHeightfield(
	ALandscapeProxy * InLandscapeProxy,
	const FHoudiniPackageParams & PackageParams,
	const EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType,
	FHoudiniBakedObjectData& BakedObjectData)
{
	if (!IsValid(InLandscapeProxy))
		return nullptr;

	const FString & BakeFolder = PackageParams.BakeFolder;
	const FString & AssetName = PackageParams.HoudiniAssetName;

	switch (LandscapeOutputBakeType) 
	{
		case EHoudiniLandscapeOutputBakeType::Detachment:
		{
			// Detach the landscape from the Houdini Asset Actor
			InLandscapeProxy->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
		}
		break;

		case EHoudiniLandscapeOutputBakeType::BakeToImage:
		{
			// Create heightmap image to the bake folder
			ULandscapeInfo * InLandscapeInfo = InLandscapeProxy->GetLandscapeInfo();
			if (!IsValid(InLandscapeInfo))
				return nullptr;
		
			// bake to image must use absoluate path, 
			// and the file name has a file extension (.png)
			FString BakeFolderInFullPath = BakeFolder;

			// Figure absolute path,
			if (!BakeFolderInFullPath.EndsWith("/"))
				BakeFolderInFullPath += "/";

			if (BakeFolderInFullPath.StartsWith("/Game"))
				BakeFolderInFullPath = BakeFolderInFullPath.Mid(5, BakeFolderInFullPath.Len() - 5);

			if (BakeFolderInFullPath.StartsWith("/"))
				BakeFolderInFullPath = BakeFolderInFullPath.Mid(1, BakeFolderInFullPath.Len() - 1);

			FString FullPath = FPaths::ProjectContentDir() + BakeFolderInFullPath + AssetName + "_" + InLandscapeProxy->GetName() + ".png";

			InLandscapeInfo->ExportHeightmap(FullPath);

			// TODO:
			// We should update this to have an asset/package..
		}
		break;

		case EHoudiniLandscapeOutputBakeType::BakeToWorld:
		{
			ULandscapeInfo * InLandscapeInfo = InLandscapeProxy->GetLandscapeInfo();
			if (!IsValid(InLandscapeInfo))
				return nullptr;

			// 0.  Get Landscape Data //
			
			// Extract landscape height data
			TArray<uint16> InLandscapeHeightData;
			int32 XSize, YSize;
			FVector3d Min, Max;
			if (!FUnrealLandscapeTranslator::GetLandscapeData(InLandscapeProxy, InLandscapeHeightData, XSize, YSize, Min, Max))
				return nullptr;

			// Extract landscape Layers data
			TArray<FLandscapeImportLayerInfo> InLandscapeImportLayerInfos;
			for (int32 n = 0; n < InLandscapeInfo->Layers.Num(); ++n) 
			{
				TArray<uint8> CurrentLayerIntData;
				FLinearColor LayerUsageDebugColor;
				FString LayerName;
				if (!FUnrealLandscapeTranslator::GetLandscapeTargetLayerData(InLandscapeProxy, InLandscapeInfo, n, CurrentLayerIntData, LayerUsageDebugColor, LayerName))
					continue;

				FLandscapeImportLayerInfo CurrentLayerInfo;
				CurrentLayerInfo.LayerName = FName(LayerName);
				CurrentLayerInfo.LayerInfo = InLandscapeInfo->Layers[n].LayerInfoObj;
				CurrentLayerInfo.LayerData = CurrentLayerIntData;

				CurrentLayerInfo.LayerInfo->LayerUsageDebugColor = LayerUsageDebugColor;

				InLandscapeImportLayerInfos.Add(CurrentLayerInfo);
			}

			// 1. Create package  //

			FString PackagePath = PackageParams.GetPackagePath();
			FString PackageName = PackageParams.GetPackageName();

			UPackage *CreatedPackage = nullptr;
			FString CreatedPackageName;

			CreatedPackage = PackageParams.CreatePackageForObject(CreatedPackageName);

			if (!CreatedPackage)
				return nullptr;

			BakedObjectData.BakeStats.NotifyPackageCreated(1);

			// 2. Create a new world asset with dialog //
			UWorldFactory* Factory = NewObject<UWorldFactory>();
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			UObject* Asset = AssetToolsModule.Get().CreateAssetWithDialog(
				PackageName, PackagePath,
				UWorld::StaticClass(), Factory, FName("ContentBrowserNewAsset"));


			UWorld* NewWorld = Cast<UWorld>(Asset);
			if (!NewWorld)
				return nullptr;

			BakedObjectData.BakeStats.NotifyObjectsCreated(NewWorld->GetClass()->GetName(), 1);
			NewWorld->SetCurrentLevel(NewWorld->PersistentLevel);

			// 4. Spawn a landscape proxy actor in the created world
			ALandscapeStreamingProxy * BakedLandscapeProxy = NewWorld->SpawnActor<ALandscapeStreamingProxy>();
			if (!BakedLandscapeProxy)
				return nullptr;

			BakedObjectData.BakeStats.NotifyObjectsCreated(BakedLandscapeProxy->GetClass()->GetName(), 1);
			// Create a new GUID
			FGuid currentGUID = FGuid::NewGuid();
			BakedLandscapeProxy->SetLandscapeGuid(currentGUID);

			// Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
			BakedLandscapeProxy->bCastStaticShadow = false;
			

			// 5. Import data to the created landscape proxy
			TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;

			HeightmapDataPerLayers.Add(FGuid(), InLandscapeHeightData);
			MaterialLayerDataPerLayer.Add(FGuid(), InLandscapeImportLayerInfos);

			ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

			BakedLandscapeProxy->Import(
				currentGUID,
				0, 0, XSize-1, YSize-1,
				InLandscapeInfo->ComponentNumSubsections, InLandscapeInfo->SubsectionSizeQuads,
				HeightmapDataPerLayers, NULL,
				MaterialLayerDataPerLayer,
				ImportLayerType
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
				, MakeArrayView<FLandscapeLayer>({})
#endif
			);

			BakedLandscapeProxy->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((XSize * YSize) / (2048 * 2048) + 1), (uint32)2);

			TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;

			if (IsValid(BakedLandscapeProxy->LandscapeMaterial))
			{
				// No need to check the outputs for materials, only check  if the material is in the temp folder
				//if (IsObjectTemporary(BakedLandscapeProxy->LandscapeMaterial, EHoudiniOutputType::Invalid, InParentOutputs, PackageParams.TempCookFolder))
				if (IsObjectInTempFolder(BakedLandscapeProxy->LandscapeMaterial, PackageParams.TempCookFolder))
				{
					UMaterialInterface* DuplicatedMaterial = BakeSingleMaterialToPackage(
						BakedLandscapeProxy->LandscapeMaterial, PackageParams, BakedObjectData, AlreadyBakedMaterialsMap);
					BakedLandscapeProxy->LandscapeMaterial = DuplicatedMaterial;
				}
			}

			if (IsValid(BakedLandscapeProxy->LandscapeHoleMaterial))
			{
				// No need to check the outputs for materials, only check if the material is in the temp folder
				//if (IsObjectTemporary(BakedLandscapeProxy->LandscapeHoleMaterial, EHoudiniOutputType::Invalid, InParentOutputs, PackageParams.TempCookFolder))
				if (IsObjectInTempFolder(BakedLandscapeProxy->LandscapeHoleMaterial, PackageParams.TempCookFolder))
				{
					UMaterialInterface* DuplicatedMaterial = BakeSingleMaterialToPackage(
						BakedLandscapeProxy->LandscapeHoleMaterial, PackageParams, BakedObjectData, AlreadyBakedMaterialsMap);
					BakedLandscapeProxy->LandscapeHoleMaterial = DuplicatedMaterial;
				}
			}

			// 6. Register all the landscape components, and set landscape actor transform
			BakedLandscapeProxy->RegisterAllComponents();
			BakedLandscapeProxy->SetActorTransform(InLandscapeProxy->GetTransform());

			// 7. Save Package
			BakedObjectData.PackagesToSave.Add(CreatedPackage);
			FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

			// Sync the CB to the baked objects
			if(GEditor)
			{
				TArray<UObject*> Objects;
				Objects.Add(NewWorld);
				GEditor->SyncBrowserToObjects(Objects);
			}
		}
		break;
	}

	return InLandscapeProxy;
}

bool
FHoudiniEngineBakeUtils::BakeCurve(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	USplineComponent* InSplineComponent,
	ULevel* InLevel,
	const FHoudiniPackageParams &PackageParams,
	const FHoudiniBakeSettings& BakeSettings,
	const FName& InActorName,
	AActor*& OutActor,
	USplineComponent*& OutSplineComponent,
	FHoudiniBakedObjectData& BakedObjectData,
	FName InOverrideFolderPath,
	AActor* InActor,
	TSubclassOf<AActor> BakeActorClass)
{
	if (!IsValid(InActor))
	{
		UActorFactory* Factory = nullptr;
		if (IsValid(BakeActorClass))
		{
			Factory = GEditor->FindActorFactoryForActorClass(BakeActorClass);
			if (!Factory)
				Factory = GEditor->FindActorFactoryByClass(UActorFactoryClass::StaticClass());
		}
		else
		{
			Factory = GetActorFactory(NAME_None, BakeSettings, BakeActorClass, UActorFactoryEmptyActor::StaticClass());
		}
		if (!Factory)
			return false;

		OutActor = SpawnBakeActor(Factory, nullptr, InLevel, BakeSettings, InSplineComponent->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);
		if (IsValid(OutActor))
			BakedObjectData.BakeStats.NotifyObjectsCreated(OutActor->GetClass()->GetName(), 1);
	}
	else
	{
		OutActor = InActor;
		if (IsValid(OutActor))
			BakedObjectData.BakeStats.NotifyObjectsUpdated(OutActor->GetClass()->GetName(), 1);
	}

	// Fallback to ObjectName from package params if InActorName is not set
	const FName ActorName = InActorName.IsNone() ? FName(PackageParams.ObjectName) : InActorName;
	const FString NewNameStr = MakeUniqueObjectNameIfNeeded(InLevel, OutActor->GetClass(), InActorName.ToString(), OutActor);
	RenameAndRelabelActor(OutActor, NewNameStr, false);
	OutActor->SetFolderPath(InOverrideFolderPath.IsNone() ? FName(PackageParams.HoudiniAssetActorName) : InOverrideFolderPath);

	USplineComponent* DuplicatedSplineComponent = DuplicateObject<USplineComponent>(
		InSplineComponent,
		OutActor,
		FName(MakeUniqueObjectNameIfNeeded(OutActor, InSplineComponent->GetClass(), PackageParams.ObjectName)));

	if (IsValid(DuplicatedSplineComponent))
		BakedObjectData.BakeStats.NotifyObjectsCreated(DuplicatedSplineComponent->GetClass()->GetName(), 1);
	
	OutActor->AddInstanceComponent(DuplicatedSplineComponent);
	const bool bCreateIfMissing = true;
	USceneComponent* RootComponent = GetActorRootComponent(OutActor, bCreateIfMissing);
	DuplicatedSplineComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	// We duplicated the InSplineComponent, so we don't have to copy all of its properties, but we must set the
	// world transform
	DuplicatedSplineComponent->SetWorldTransform(InSplineComponent->GetComponentTransform());

	FAssetRegistryModule::AssetCreated(DuplicatedSplineComponent);
	DuplicatedSplineComponent->RegisterComponent();

	OutSplineComponent = DuplicatedSplineComponent;
	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeCurve(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FHoudiniPackageParams &PackageParams,
	FHoudiniAttributeResolver& InResolver,
	const FHoudiniBakeSettings& BakeSettings,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	FHoudiniBakedObjectData& BakedObjectData,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (InOutputObject.OutputComponents.IsEmpty())
		return false;

	HOUDINI_CHECK_RETURN(InOutputObject.OutputComponents.Num() == 1, false);

	USplineComponent* SplineComponent = Cast<USplineComponent>(InOutputObject.OutputComponents[0]);
	if (!IsValid(SplineComponent))
		return false;

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = SplineComponent ? SplineComponent->GetWorld() : GWorld;

		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = InResolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a new level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			BakedObjectData.BakeStats.NotifyPackageCreated(1);
			BakedObjectData.BakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			BakedObjectData.PackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if(!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified, or fallback to the default named actor
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	FindUnrealBakeActor(InOutputObject, InBakedOutputObject, InBakedActors, DesiredLevel, *(PackageParams.ObjectName), BakeSettings, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName);

	// If we are baking in replace mode, remove the previous bake component
	if (BakeSettings.bReplaceAssets && !InBakedOutputObject.BakedComponent.IsEmpty())
	{
		UActorComponent* PrevComponent = Cast<UActorComponent>(InBakedOutputObject.GetBakedComponentIfValid());
		if (PrevComponent && PrevComponent->GetOwner() == FoundActor)
		{
			RemovePreviouslyBakedComponent(PrevComponent);
		}
	}

	TSubclassOf<AActor> BakeActorClass = GetBakeActorClassOverride(InOutputObject);

	USplineComponent* NewSplineComponent = nullptr;
	const FName OutlinerFolderPath = GetOutlinerFolderPath(InResolver, *(PackageParams.HoudiniAssetActorName));
	if (!BakeCurve(
		InHoudiniAssetComponent, 
		SplineComponent, 
		DesiredLevel, 
		PackageParams, 
		BakeSettings, 
		BakeActorName, 
		FoundActor, 
		NewSplineComponent, 
		BakedObjectData, 
		OutlinerFolderPath, 
		FoundActor, 
		BakeActorClass))
		return false;

	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	InBakedOutputObject.BakedComponent = FSoftObjectPath(NewSplineComponent).ToString();

	// If we are baking in replace mode, remove previously baked components/instancers
	if (BakeSettings.bReplaceAssets && BakeSettings.bReplaceActors)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	FHoudiniEngineBakedActor Result(
		FoundActor,
		BakeActorName,
		OutlinerFolderPath.IsNone() ? FName(PackageParams.HoudiniAssetActorName) : OutlinerFolderPath,
		INDEX_NONE,  // Output index
		FHoudiniOutputObjectIdentifier(),
		nullptr,  // InBakedObject
		nullptr,  // InSourceObject
		NewSplineComponent,
		PackageParams.BakeFolder,
		PackageParams);

	OutBakedActorEntry = Result;

	return true;
}

AActor*
FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	UHoudiniSplineComponent * InHoudiniSplineComponent,
	const FHoudiniPackageParams & PackageParams,
	const FHoudiniBakeSettings& BakeSettings,
	UWorld* WorldToSpawn,
	const FTransform & SpawnTransform) 
{
	if (!IsValid(InHoudiniSplineComponent))
		return nullptr;

	TArray<FVector3d>& DisplayPoints = InHoudiniSplineComponent->DisplayPoints;
	if (DisplayPoints.Num() < 2)
		return nullptr;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	TSubclassOf<AActor> BakeActorClass = nullptr;
	UActorFactory* const Factory = GetActorFactory(NAME_None, BakeSettings, BakeActorClass, UActorFactoryEmptyActor::StaticClass());
	if (!Factory)
		return nullptr;

	// Remove the actor if it exists
	for (auto & Actor : DesiredLevel->Actors)
	{
		if (!Actor)
			continue;

		if (Actor->GetActorNameOrLabel() == PackageParams.ObjectName)
		{
			UWorld* World = Actor->GetWorld();
			if (!World)
				World = GWorld;

			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();
			World->EditorDestroyActor(Actor, true);

			break;
		}
	}

	AActor* NewActor = SpawnBakeActor(Factory, nullptr, DesiredLevel, BakeSettings, InHoudiniSplineComponent->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);

	USplineComponent* BakedUnrealSplineComponent = NewObject<USplineComponent>(NewActor);
	if (!BakedUnrealSplineComponent)
		return nullptr;

	// add display points to created unreal spline component
	for (int32 n = 0; n < DisplayPoints.Num(); ++n) 
	{
		FVector3d& NextPoint = DisplayPoints[n];
		BakedUnrealSplineComponent->AddSplinePoint(NextPoint, ESplineCoordinateSpace::Local);
		// Set the curve point type to be linear, since we are using display points
		BakedUnrealSplineComponent->SetSplinePointType(n, ESplinePointType::Linear);
	}
	NewActor->AddInstanceComponent(BakedUnrealSplineComponent);

	BakedUnrealSplineComponent->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	FAssetRegistryModule::AssetCreated(NewActor);
	FAssetRegistryModule::AssetCreated(BakedUnrealSplineComponent);
	BakedUnrealSplineComponent->RegisterComponent();

	// The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
	const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, *(PackageParams.ObjectName), NewActor);
	RenameAndRelabelActor(NewActor, NewNameStr, false);
	NewActor->SetFolderPath(FName(PackageParams.HoudiniAssetName));

	return NewActor;
}

UBlueprint* 
FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToBlueprint(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	UHoudiniSplineComponent * InHoudiniSplineComponent,
	const FHoudiniPackageParams & PackageParams,
	const FHoudiniBakeSettings& BakeSettings,
	UWorld* WorldToSpawn,
	const FTransform & SpawnTransform) 
{
	if (!IsValid(InHoudiniSplineComponent))
		return nullptr;

	FGuid BakeGUID = FGuid::NewGuid();

	if (!BakeGUID.IsValid())
		BakeGUID = FGuid::NewGuid();

	// We only want half of generated guid string.
	FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

	// Generate Blueprint name.
	FString BlueprintName = PackageParams.ObjectName + "_BP";

	// Generate unique package name.
	FString PackageName = PackageParams.BakeFolder + "/" + BlueprintName;
	PackageName = UPackageTools::SanitizePackageName(PackageName);

	// See if package exists, if it does, we need to regenerate the name.
	UPackage * Package = FindPackage(nullptr, *PackageName);

	if (IsValid(Package))
	{
		// Package does exist, there's a collision, we need to generate a new name.
		BakeGUID.Invalidate();
	}
	else
	{
		// Create actual package.
		Package = CreatePackage(*PackageName);
	}

	AActor * CreatedHoudiniSplineActor = FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
		InHoudiniAssetComponent, InHoudiniSplineComponent, PackageParams, BakeSettings, WorldToSpawn, SpawnTransform);

	FHoudiniBakedObjectData BakedObjectData;

	UBlueprint * Blueprint = nullptr;
	if (IsValid(CreatedHoudiniSplineActor))
	{

		UObject* Asset = nullptr;

		Asset = StaticFindObjectFast(UObject::StaticClass(), Package, FName(*BlueprintName));
		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
				BlueprintName, PackageParams.BakeFolder,
				UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
		}

		TArray<UActorComponent*> Components;
		for (auto & Next : CreatedHoudiniSplineActor->GetComponents())
		{
			Components.Add(Next);
		}

		Blueprint = Cast<UBlueprint>(Asset);

		// Clear old Blueprint Node tree
		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

		int32 NodeSize = SCS->GetAllNodes().Num();
		for (int32 n = NodeSize - 1; n >= 0; --n)
			SCS->RemoveNode(SCS->GetAllNodes()[n]);

		FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, Components);

		CreatedHoudiniSplineActor->RemoveFromRoot();
		CreatedHoudiniSplineActor->ConditionalBeginDestroy();

		GWorld->EditorDestroyActor(CreatedHoudiniSplineActor, true);

		Package->MarkPackageDirty();
		BakedObjectData.PackagesToSave.Add(Package);
	}

	// Compile the new/updated blueprints
	if (!IsValid(Blueprint))
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	// Save the created BP package.
	FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

	return Blueprint;
}


void
FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
	UPackage * Package, UObject * Object, const TCHAR * Key,
	const TCHAR * Value)
{
	if (!IsValid(Package))
		return;

	UMetaData * MetaData = Package->GetMetaData();
	if (IsValid(MetaData))
		MetaData->SetValue(Object, Key, Value);
}


bool
FHoudiniEngineBakeUtils::
GetHoudiniGeneratedNameFromMetaInformation(
	UPackage * Package, UObject * Object, FString & HoudiniName)
{
	if (!IsValid(Package))
		return false;

	UMetaData * MetaData = Package->GetMetaData();
	if (!IsValid(MetaData))
		return false;

	if (MetaData->HasValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
	{
		// Retrieve name used for package generation.
		const FString NameFull = MetaData->GetValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME);

		//HoudiniName = NameFull.Left(FMath::Min(NameFull.Len(), FHoudiniEngineUtils::PackageGUIDItemNameLength));
		HoudiniName = NameFull;
		return true;
	}

	return false;
}

UMaterialInterface *
FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
	UMaterialInterface * Material, UMaterialInterface* PreviousBakeMaterial, const FString & MaterialName, const FHoudiniPackageParams& ObjectPackageParams,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap)
{
	if (InOutAlreadyBakedMaterialsMap.Contains(Material))
	{
		return InOutAlreadyBakedMaterialsMap[Material];
	}
	
	UMaterialInterface * DuplicatedMaterial = nullptr;

	FString CreatedMaterialName;
	// Create material package.  Use the same package params as static mesh, but with the material's name
	FHoudiniPackageParams MaterialPackageParams = ObjectPackageParams;
	MaterialPackageParams.ObjectName = MaterialName;

	// Check if there is a valid previous material. If so, get the bake counter for consistency in
	// replace or iterative package naming
	bool bIsPreviousBakeMaterialValid = IsValid(PreviousBakeMaterial);
	int32 BakeCounter = 0;
	TArray<UMaterialExpression*> PreviousBakeMaterialExpressions;

	
	if (bIsPreviousBakeMaterialValid && PreviousBakeMaterial->IsA(UMaterial::StaticClass()))
	{
		UMaterial * PreviousMaterialCast = Cast<UMaterial>(PreviousBakeMaterial);
		bIsPreviousBakeMaterialValid = MaterialPackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakeMaterial);

		if (bIsPreviousBakeMaterialValid && PreviousMaterialCast)
		{
			MaterialPackageParams.GetBakeCounterFromBakedAsset(PreviousBakeMaterial, BakeCounter);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
			PreviousBakeMaterialExpressions = PreviousMaterialCast->GetExpressionCollection().Expressions;
#else
			PreviousBakeMaterialExpressions = PreviousMaterialCast->Expressions;
#endif
		}
	}
	
	UPackage * MaterialPackage = MaterialPackageParams.CreatePackageForObject(CreatedMaterialName, BakeCounter);
	if (!IsValid(MaterialPackage))
		return nullptr;

	BakedObjectData.BakeStats.NotifyPackageCreated(1);
	
	// Clone material.
	DuplicatedMaterial = DuplicateObject< UMaterialInterface >(Material, MaterialPackage, *CreatedMaterialName);
	if (!IsValid(DuplicatedMaterial))
		return nullptr;

	BakedObjectData.BakeStats.NotifyObjectsCreated(DuplicatedMaterial->GetClass()->GetName(), 1);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedMaterialName);
	// Baked object! this is not temporary anymore
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

	// Retrieve and check various sampling expressions. If they contain textures, duplicate (and bake) them.
	UMaterial * DuplicatedMaterialCast = Cast<UMaterial>(DuplicatedMaterial);
	if (DuplicatedMaterialCast)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		auto& MatExpressions = DuplicatedMaterialCast->GetExpressionCollection().Expressions;
#else
		auto& MatExpressions = DuplicatedMaterialCast->Expressions;
#endif
		const int32 NumExpressions = MatExpressions.Num();
		for (int32 ExpressionIdx = 0; ExpressionIdx < NumExpressions; ++ExpressionIdx)
		{
			UMaterialExpression* Expression = MatExpressions[ExpressionIdx];
			UMaterialExpression* PreviousBakeExpression = nullptr;
			if (bIsPreviousBakeMaterialValid && PreviousBakeMaterialExpressions.IsValidIndex(ExpressionIdx))
			{
				PreviousBakeExpression = PreviousBakeMaterialExpressions[ExpressionIdx];
			}
			FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
				Expression, PreviousBakeExpression, MaterialPackageParams, BakedObjectData);
		}
	}

	// Notify registry that we have created a new duplicate material.
	FAssetRegistryModule::AssetCreated(DuplicatedMaterial);

	// Dirty the material package.
	DuplicatedMaterial->MarkPackageDirty();

	// Recompile the baked material
	// DuplicatedMaterial->ForceRecompileForRendering();
	// Use UMaterialEditingLibrary::RecompileMaterial since it correctly updates texture references in the material
	// which ForceRecompileForRendering does not do
	if (DuplicatedMaterialCast)
	{
		UMaterialEditingLibrary::RecompileMaterial(DuplicatedMaterialCast);
	}

	BakedObjectData.PackagesToSave.Add(MaterialPackage);

	InOutAlreadyBakedMaterialsMap.Add(Material, DuplicatedMaterial);

	return DuplicatedMaterial;
}

void
FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
	UMaterialExpression * MaterialExpression, UMaterialExpression* PreviousBakeMaterialExpression,
	const FHoudiniPackageParams& PackageParams, FHoudiniBakedObjectData& BakedObjectData)
{
	UMaterialExpressionTextureSample * TextureSample = Cast< UMaterialExpressionTextureSample >(MaterialExpression);
	if (!IsValid(TextureSample))
		return;

	UTexture2D * Texture = Cast< UTexture2D >(TextureSample->Texture);
	if (!IsValid(Texture))
		return;

	UPackage * TexturePackage = Cast< UPackage >(Texture->GetOuter());
	if (!IsValid(TexturePackage))
		return;

	// Try to get the previous bake's texture
	UTexture2D* PreviousBakeTexture = nullptr;
	if (IsValid(PreviousBakeMaterialExpression))
	{
		UMaterialExpressionTextureSample* PreviousBakeTextureSample = Cast< UMaterialExpressionTextureSample >(PreviousBakeMaterialExpression);
		if (IsValid(PreviousBakeTextureSample))
			PreviousBakeTexture = Cast< UTexture2D >(PreviousBakeTextureSample->Texture);
	}

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
		TexturePackage, Texture, GeneratedTextureName))
	{
		// Duplicate texture.
		UTexture2D * DuplicatedTexture = FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
			Texture, PreviousBakeTexture, GeneratedTextureName, PackageParams, BakedObjectData);

		// Re-assign generated texture.
		TextureSample->Texture = DuplicatedTexture;
	}
}

UTexture2D *
FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
	UTexture2D * Texture, 
	UTexture2D* PreviousBakeTexture, 
	const FString & SubTextureName, 
	const FHoudiniPackageParams& PackageParams,
	FHoudiniBakedObjectData& BakedObjectData)
{
	UTexture2D* DuplicatedTexture = nullptr;
#if WITH_EDITOR
	// Retrieve original package of this texture.
	UPackage * TexturePackage = Cast< UPackage >(Texture->GetOuter());
	if (!IsValid(TexturePackage))
		return nullptr;

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(TexturePackage, Texture, GeneratedTextureName))
	{
		UMetaData * MetaData = TexturePackage->GetMetaData();
		if (!IsValid(MetaData))
			return nullptr;

		// Retrieve texture type.
		const FString & TextureType =
			MetaData->GetValue(Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);

		FString CreatedTextureName;

		// Create texture package. Use the same package params as material's, but with object name appended by generated texture's name
		FHoudiniPackageParams TexturePackageParams = PackageParams;
		TexturePackageParams.ObjectName = TexturePackageParams.ObjectName + "_" + GeneratedTextureName;

		// Determine the bake counter of the previous bake's texture (if exists/valid) for naming consistency when
		// replacing/iterating
		bool bIsPreviousBakeTextureValid = IsValid(PreviousBakeTexture);
		int32 BakeCounter = 0;
		if (bIsPreviousBakeTextureValid)
		{
			bIsPreviousBakeTextureValid = TexturePackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakeTexture);
			if (bIsPreviousBakeTextureValid)
			{
				TexturePackageParams.GetBakeCounterFromBakedAsset(PreviousBakeTexture, BakeCounter);
			}
		}

		UPackage * NewTexturePackage = TexturePackageParams.CreatePackageForObject(CreatedTextureName, BakeCounter);

		if (!IsValid(NewTexturePackage))
			return nullptr;

		BakedObjectData.BakeStats.NotifyPackageCreated(1);
		
		// Clone texture.
		DuplicatedTexture = DuplicateObject< UTexture2D >(Texture, NewTexturePackage, *CreatedTextureName);
		if (!IsValid(DuplicatedTexture))
			return nullptr;

		BakedObjectData.BakeStats.NotifyObjectsCreated(DuplicatedTexture->GetClass()->GetName(), 1);

		// Add meta information.
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedTextureName);
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);
		// Baked object! this is not temporary anymore
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT, TEXT("true"));

		// Notify registry that we have created a new duplicate texture.
		FAssetRegistryModule::AssetCreated(DuplicatedTexture);
		
		// Dirty the texture package.
		DuplicatedTexture->MarkPackageDirty();

		BakedObjectData.PackagesToSave.Add(NewTexturePackage);
	}
#endif
	return DuplicatedTexture;
}


bool 
FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor * ActorOwner = HoudiniAssetComponent->GetOwner();

	if (!IsValid(ActorOwner))
		return false;

	UWorld* World = ActorOwner->GetWorld();
	if (!World)
		World = GWorld;

	World->EditorDestroyActor(ActorOwner, false);

	return true;
}


void 
FHoudiniEngineBakeUtils::SaveBakedPackages(TArray<UPackage*> & PackagesToSave, bool bSaveCurrentWorld) 
{
	UWorld * CurrentWorld = nullptr;
	if (bSaveCurrentWorld && GEditor)
		CurrentWorld = GEditor->GetEditorWorldContext().World();

	if (CurrentWorld)
	{
		// Save the current map
		FString CurrentWorldPath = FPaths::GetBaseFilename(CurrentWorld->GetPathName(), false);
		UPackage* CurrentWorldPackage = CreatePackage(*CurrentWorldPath);

		if (CurrentWorldPackage)
		{
			CurrentWorldPackage->MarkPackageDirty();
			PackagesToSave.Add(CurrentWorldPackage);
		}
	}

	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
}

bool
FHoudiniEngineBakeUtils::FindOutputObject(
	const UObject* InObjectToFind, 
	const EHoudiniOutputType& InOutputType,
	const TArray<UHoudiniOutput*> InOutputs,
	int32& OutOutputIndex,
	FHoudiniOutputObjectIdentifier &OutIdentifier)
{
	if (!IsValid(InObjectToFind))
		return false;

	const int32 NumOutputs = InOutputs.Num();
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		const UHoudiniOutput* CurOutput = InOutputs[OutputIdx];
		if (!IsValid(CurOutput))
			continue;

		if (CurOutput->GetType() != InOutputType)
			continue;
		
		for (auto& CurOutputObject : CurOutput->GetOutputObjects())
		{
			if (CurOutputObject.Value.OutputObject == InObjectToFind
				|| CurOutputObject.Value.ProxyObject == InObjectToFind
				|| CurOutputObject.Value.ProxyComponent == InObjectToFind)
			{
				OutOutputIndex = OutputIdx;
				OutIdentifier = CurOutputObject.Key;
				return true;
			}

			for(auto CurrentComponent : CurOutputObject.Value.OutputComponents)
			{
			    if (CurrentComponent == InObjectToFind)
			    {
				    OutOutputIndex = OutputIdx;
				    OutIdentifier = CurOutputObject.Key;
				    return true;
			    }
			}
		}
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::IsObjectTemporary(
	UObject* InObject,
	const EHoudiniOutputType& InOutputType,
	UHoudiniAssetComponent* InHAC)
{
	if (!IsValid(InObject))
		return false;

	FString TempPath = FString();

	TArray<UHoudiniOutput*> Outputs;
	if (IsValid(InHAC))
	{
		const int32 NumOutputs = InHAC->GetNumOutputs();
		Outputs.SetNum(NumOutputs);
		for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
		{
			Outputs[OutputIdx] = InHAC->GetOutputAt(OutputIdx);
		}

		TempPath = InHAC->TemporaryCookFolder.Path;
	}

	return IsObjectTemporary(InObject, InOutputType, Outputs, TempPath, InHAC->GetComponentGUID());
}

bool 
FHoudiniEngineBakeUtils::IsObjectInTempFolder(
	UObject* const InObject, 
	const FString& InTemporaryCookFolder)
{
	if (!IsValid(InObject))
		return false;

	// Check the package path for this object
	// If it is in the HAC temp directory, assume it is temporary, and will need to be duplicated
	UPackage* ObjectPackage = InObject->GetOutermost();
	if (IsValid(ObjectPackage))
	{
		const FString PathName = ObjectPackage->GetPathName();
		if (PathName.StartsWith(InTemporaryCookFolder))
			return true;

		// Also check the default temp folder
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		if (PathName.StartsWith(HoudiniRuntimeSettings->DefaultTemporaryCookFolder))
			return true;
	}

	return false;
}

bool 
FHoudiniEngineBakeUtils::IsObjectTemporary(
	UObject* InObject,
	const EHoudiniOutputType& InOutputType,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const FString& InTemporaryCookFolder,
	const FGuid& InComponentGuid)
{
	if (!IsValid(InObject))
		return false;

	// Check the object's meta-data first
	if (IsObjectTemporary(InObject, InOutputType, InComponentGuid))
		return true;

	// Previous IsObjectTemporary tests 
	// Only kept here for compatibility with assets previously baked before adding the "Baked" metadata.
	// Check that the object is in the outputs, and stored in the temp directory

	// Object not in the outputs, assume not temp
	int32 ParentOutputIndex = -1;
	FHoudiniOutputObjectIdentifier Identifier;
	// Generated materials will have an invalid output type, dont look for them in the outputs.
	if (InOutputType != EHoudiniOutputType::Invalid && !FindOutputObject(InObject, InOutputType, InParentOutputs, ParentOutputIndex, Identifier))
		return false;

	// Check the package path for this object
	// If it is in the temp directory, assume it is temporary, and will need to be duplicated
	if (IsObjectInTempFolder(InObject, InTemporaryCookFolder))
		return true;

	return false;
}

bool
FHoudiniEngineBakeUtils::IsObjectTemporary(
	UObject* InObject,
	const EHoudiniOutputType& InOutputType,
	const FGuid& InComponentGuid)
{
	if (!IsValid(InObject))
		return false;

	UPackage* ObjectPackage = InObject->GetOutermost();
	if (IsValid(ObjectPackage))
	{
		// Look for the meta data
		UMetaData* MetaData = ObjectPackage->GetMetaData();
		if (IsValid(MetaData))
		{
			// This object was not generated by Houdini, so not a temp object
			if (!MetaData->HasValue(InObject, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
				return false;

			// The object has been baked, so not a temp object as well
			if (MetaData->HasValue(InObject, HAPI_UNREAL_PACKAGE_META_BAKED_OBJECT))
				return false;

			// If InComponentGuid is valid, check that against HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID
			if (InComponentGuid.IsValid())
			{
				const FString GuidStr = InComponentGuid.ToString();
				// If the object has the HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID key but the value does not match GuidStr,
				// then, while this is a temporary object, it was not generated by this HAC, so we should not bake it
				if (MetaData->HasValue(InObject, HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID)
						&& MetaData->GetValue(InObject, HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID) != GuidStr)
				{
					return false;
				}
			}
		}
	}

	return true;
}

void
FHoudiniEngineBakeUtils::CopyPropertyToNewActorAndSkeletalComponent(
	AActor* NewActor,
	USkeletalMeshComponent* NewSKC,
	USkeletalMeshComponent* InSKC,
	bool bInCopyWorldTransform)
{
	if (!IsValid(NewSKC))
		return;

	if (!IsValid(InSKC))
		return;
}


void 
FHoudiniEngineBakeUtils::CopyPropertyToNewActorAndComponent(
	AActor* NewActor,
	UStaticMeshComponent* NewSMC,
	UStaticMeshComponent* InSMC,
	bool bInCopyWorldTransform)
{
	if (!IsValid(NewSMC))
		return;

	if (!IsValid(InSMC))
		return;

	// Copy properties to new actor
	//UStaticMeshComponent* OtherSMC_NonConst = const_cast<UStaticMeshComponent*>(StaticMeshComponent);
	NewSMC->SetCollisionProfileName(InSMC->GetCollisionProfileName());
	NewSMC->SetCollisionEnabled(InSMC->GetCollisionEnabled());
	NewSMC->LightmassSettings = InSMC->LightmassSettings;
	NewSMC->CastShadow = InSMC->CastShadow;
	NewSMC->SetMobility(InSMC->Mobility);

	UBodySetup* InBodySetup = InSMC->GetBodySetup();
	UBodySetup* NewBodySetup = NewSMC->GetBodySetup();

	// See if we need to create a body setup for the new SMC
	if (InBodySetup && !NewBodySetup)
	{
		if (NewSMC->GetStaticMesh())
		{
			NewSMC->GetStaticMesh()->CreateBodySetup();
			NewBodySetup = NewSMC->GetBodySetup();
		}
	}

	if (InBodySetup && NewBodySetup)
	{
		// Copy the BodySetup
		NewBodySetup->CopyBodyPropertiesFrom(InBodySetup);

		// We need to recreate the physics mesh for the new body setup
		NewBodySetup->InvalidatePhysicsData();
		NewBodySetup->CreatePhysicsMeshes();

		// Only copy the physical material if it's different from the default one,
		// As this was causing crashes on BakeToActors in some cases
		if (GEngine != NULL && NewBodySetup->GetPhysMaterial() != GEngine->DefaultPhysMaterial)
			NewSMC->SetPhysMaterialOverride(InBodySetup->GetPhysMaterial());
	}

	if (IsValid(NewActor))
		NewActor->SetActorHiddenInGame(InSMC->bHiddenInGame);

	NewSMC->SetVisibility(InSMC->IsVisible());

	//--------------------------
	// Copy Actor Properties
	//--------------------------
	// The below code is from EditorUtilities::CopyActorProperties and simplified
	bool bCopyActorProperties = true;
	AActor* SourceActor = InSMC->GetOwner();
	if (IsValid(SourceActor) && bCopyActorProperties)
	{
		// The actor's classes should be compatible, right?
		UClass* ActorClass = SourceActor->GetClass();
		//check(NewActor->GetClass()->IsChildOf(ActorClass));

		bool bTransformChanged = false;
		EditorUtilities::FCopyOptions Options(EditorUtilities::ECopyOptions::Default);
		
		// Copy non-component properties from the old actor to the new actor
		TSet<UObject*> ModifiedObjects;

		if (NewActor->GetClass()->IsChildOf(ActorClass))
		{
			for (FProperty* Property = ActorClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
			{
				const bool bIsTransient = !!(Property->PropertyFlags & CPF_Transient);
				const bool bIsComponentContainer = !!(Property->PropertyFlags & CPF_ContainsInstancedReference);
				const bool bIsComponentProp = !!(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference));
				const bool bIsBlueprintReadonly = !!(Property->PropertyFlags & CPF_BlueprintReadOnly);
				const bool bIsIdentical = Property->Identical_InContainer(SourceActor, NewActor);

				if (!bIsTransient && !bIsIdentical && !bIsComponentContainer && !bIsComponentProp && !bIsBlueprintReadonly)
				{
					const bool bIsSafeToCopy = (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp))
						&& (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate));
					if (bIsSafeToCopy)
					{
						if (!Options.CanCopyProperty(*Property, *SourceActor))
						{
							continue;
						}

						if (!ModifiedObjects.Contains(NewActor))
						{
							// Start modifying the target object
							NewActor->Modify();
							ModifiedObjects.Add(NewActor);
						}

						if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
						{
							NewActor->PreEditChange(Property);
						}

						EditorUtilities::CopySingleProperty(SourceActor, NewActor, Property);

						if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
						{
							FPropertyChangedEvent PropertyChangedEvent(Property);
							NewActor->PostEditChangeProperty(PropertyChangedEvent);
						}
					}
				}
			}
		}
	}

	// TODO:
	// // Reapply the uproperties modified by attributes on the new component
	// FHoudiniEngineUtils::UpdateAllPropertyAttributesOnObject(InSMC, InHGPO);
	// The below code is from EditorUtilities::CopyActorProperties and modified to only copy from one component to another
	UClass* ComponentClass = nullptr;
	if (InSMC->GetClass()->IsChildOf(NewSMC->GetClass()))
	{
		ComponentClass = NewSMC->GetClass();
	}
	else if (NewSMC->GetClass()->IsChildOf(InSMC->GetClass()))
	{
		ComponentClass = InSMC->GetClass();
	}
	else
	{
		HOUDINI_LOG_WARNING(
			TEXT("Incompatible component classes in CopyPropertyToNewActorAndComponent: %s vs %s"),
			*(InSMC->GetName()),
			*(NewSMC->GetClass()->GetName()));

		NewSMC->PostEditChange();
		return;
	}

	TSet<const FProperty*> SourceUCSModifiedProperties;
	InSMC->GetUCSModifiedProperties(SourceUCSModifiedProperties);

	//AActor* SourceActor = InSMC->GetOwner();
	if (!IsValid(SourceActor))
	{
		NewSMC->PostEditChange();
		return;
	}

	TArray<UObject*> ModifiedObjects;
	const EditorUtilities::FCopyOptions Options(EditorUtilities::ECopyOptions::CallPostEditChangeProperty);
	// Copy component properties
	for( FProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsIdentical = Property->Identical_InContainer( InSMC, NewSMC );
		const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
		const bool bIsTransform =
			Property->GetFName() == USceneComponent::GetRelativeScale3DPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeLocationPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeRotationPropertyName();

		// auto SourceComponentIsRoot = [&]()
		// {
		// 	USceneComponent* RootComponent = SourceActor->GetRootComponent();
		// 	if (InSMC == RootComponent)
		// 	{
		// 		return true;
		// 	}
		// 	return false;
		// };

		// if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
		// 	&& ( !bIsTransform || !SourceComponentIsRoot() ) )
		if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
			&& !bIsTransform )
		{
			// const bool bIsSafeToCopy = (!(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)))
			// 							&& (!(Options.Flags & EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties) || (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate)));
			const bool bIsSafeToCopy = true;
			if( bIsSafeToCopy )
			{
				if (!Options.CanCopyProperty(*Property, *SourceActor))
				{
					continue;
				}
					
				if( !ModifiedObjects.Contains(NewSMC) )
				{
					NewSMC->SetFlags(RF_Transactional);
					NewSMC->Modify();
					ModifiedObjects.Add(NewSMC);
				}

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					// @todo simulate: Should we be calling this on the component instead?
					NewActor->PreEditChange( Property );
				}

				EditorUtilities::CopySingleProperty(InSMC, NewSMC, Property);

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					FPropertyChangedEvent PropertyChangedEvent( Property );
					NewActor->PostEditChangeProperty( PropertyChangedEvent );
				}
			}
		}
	}

	if (bInCopyWorldTransform)
	{
		NewSMC->SetWorldTransform(InSMC->GetComponentTransform());
	}

	NewSMC->PostEditChange();
}

void FHoudiniEngineBakeUtils::CopyPropertyToNewGeometryCollectionActorAndComponent(AGeometryCollectionActor* NewActor,
	UGeometryCollectionComponent* NewGCC, UGeometryCollectionComponent* InGCC, bool bInCopyWorldTransform)
{
	if (!IsValid(NewGCC))
		return;

	if (!IsValid(InGCC))
		return;

	// Copy properties to new actor
	NewGCC->ChaosSolverActor = InGCC->ChaosSolverActor;
	NewGCC->InitializationFields = InGCC->InitializationFields;
	NewGCC->InitializationState = InGCC->InitializationState;
	NewGCC->ObjectType= InGCC->ObjectType;
	NewGCC->EnableClustering = InGCC->EnableClustering;
	NewGCC->ClusterGroupIndex = InGCC->ClusterGroupIndex;
	NewGCC->MaxClusterLevel = InGCC->MaxClusterLevel;
	NewGCC->DamageThreshold = InGCC->DamageThreshold;
	NewGCC->CollisionGroup = InGCC->CollisionGroup;
	NewGCC->CollisionSampleFraction = InGCC->CollisionSampleFraction;
	NewGCC->InitialVelocityType = InGCC->InitialVelocityType;
	NewGCC->InitialLinearVelocity = InGCC->InitialLinearVelocity;
	NewGCC->InitialAngularVelocity = InGCC->InitialAngularVelocity;
	
	if (IsValid(NewActor))
		NewActor->SetActorHiddenInGame(InGCC->bHiddenInGame);

	NewGCC->SetVisibility(InGCC->IsVisible());

	// The below code is from EditorUtilities::CopyActorProperties and modified to only copy from one component to another
	UClass* ComponentClass = nullptr;
	if (InGCC->GetClass()->IsChildOf(NewGCC->GetClass()))
	{
		ComponentClass = NewGCC->GetClass();
	}
	else if (NewGCC->GetClass()->IsChildOf(InGCC->GetClass()))
	{
		ComponentClass = InGCC->GetClass();
	}
	else
	{
		HOUDINI_LOG_WARNING(
			TEXT("Incompatible component classes in CopyPropertyToNewActorAndComponent: %s vs %s"),
			*(InGCC->GetName()),
			*(NewGCC->GetClass()->GetName()));

		NewGCC->PostEditChange();
		return;
	}

	TSet<const FProperty*> SourceUCSModifiedProperties;
	InGCC->GetUCSModifiedProperties(SourceUCSModifiedProperties);

	AActor* SourceActor = InGCC->GetOwner();
	if (!IsValid(SourceActor))
	{
		NewGCC->PostEditChange();
		return;
	}

	TArray<UObject*> ModifiedObjects;
	const EditorUtilities::FCopyOptions Options(EditorUtilities::ECopyOptions::CallPostEditChangeProperty);
	// Copy component properties
	for( FProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsIdentical = Property->Identical_InContainer( InGCC, NewGCC );
		const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
		const bool bIsTransform =
			Property->GetFName() == USceneComponent::GetRelativeScale3DPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeLocationPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeRotationPropertyName();

		// auto SourceComponentIsRoot = [&]()
		// {
		// 	USceneComponent* RootComponent = SourceActor->GetRootComponent();
		// 	if (InSMC == RootComponent)
		// 	{
		// 		return true;
		// 	}
		// 	return false;
		// };

		// if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
		// 	&& ( !bIsTransform || !SourceComponentIsRoot() ) )
		if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
			&& !bIsTransform )
		{
			// const bool bIsSafeToCopy = (!(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)))
			// 							&& (!(Options.Flags & EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties) || (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate)));
			const bool bIsSafeToCopy = true;
			if( bIsSafeToCopy )
			{
				if (!Options.CanCopyProperty(*Property, *SourceActor))
				{
					continue;
				}
					
				if( !ModifiedObjects.Contains(NewGCC) )
				{
					NewGCC->SetFlags(RF_Transactional);
					NewGCC->Modify();
					ModifiedObjects.Add(NewGCC);
				}

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					// @todo simulate: Should we be calling this on the component instead?
					NewActor->PreEditChange( Property );
				}

				EditorUtilities::CopySingleProperty(InGCC, NewGCC, Property);

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					FPropertyChangedEvent PropertyChangedEvent( Property );
					NewActor->PostEditChangeProperty( PropertyChangedEvent );
				}
			}
		}
	}

	if (bInCopyWorldTransform)
	{
		NewGCC->SetWorldTransform(InGCC->GetComponentTransform());
	}

	NewGCC->PostEditChange();
};

bool
FHoudiniEngineBakeUtils::RemovePreviouslyBakedActor(
	AActor* InNewBakedActor,
	ULevel* InLevel,
	const FHoudiniPackageParams& InPackageParams)
{
	// Remove a previous bake actor if it exists
	for (auto & Actor : InLevel->Actors)
	{
		if (!Actor)
			continue;

		if (Actor != InNewBakedActor && Actor->GetActorNameOrLabel() == InPackageParams.ObjectName)
		{
			UWorld* World = Actor->GetWorld();
			if (!World)
				World = GWorld;

			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();
			World->EditorDestroyActor(Actor, true);

			return true;
		}
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::RemovePreviouslyBakedComponent(UActorComponent* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	// Remove from its actor first
	if (InComponent->GetOwner())
		InComponent->GetOwner()->RemoveOwnedComponent(InComponent);

	// Detach from its parent component if attached
	USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent);
	if (IsValid(SceneComponent))
		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	InComponent->UnregisterComponent();
	InComponent->DestroyComponent();

	return true;
}

FName
FHoudiniEngineBakeUtils::GetOutputFolderPath(UObject* InOutputOwner)
{
	// Get an output folder path for PDG outputs generated by InOutputOwner.
	// The folder path is: <InOutputOwner's folder path (if it is an actor)>/<InOutputOwner's name>
	FString FolderName;
	FName FolderDirName;
	AActor* OuterActor = Cast<AActor>(InOutputOwner);
	if (OuterActor)
	{
		FolderName = OuterActor->GetActorLabel();
		FolderDirName = OuterActor->GetFolderPath();	
	}
	else
	{
		FolderName = InOutputOwner->GetName();
	}
	if (!FolderDirName.IsNone())		
		return FName(FString::Printf(TEXT("%s/%s"), *FolderDirName.ToString(), *FolderName));
	else
		return FName(FolderName);
}

void
FHoudiniEngineBakeUtils::RenameAsset(UObject* InAsset, const FString& InNewName, bool bMakeUniqueIfNotUnique)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const FSoftObjectPath OldPath = FSoftObjectPath(InAsset);

	FString NewName;
	if (bMakeUniqueIfNotUnique)
		NewName = MakeUniqueObjectNameIfNeeded(InAsset->GetPackage(), InAsset->GetClass(), InNewName, InAsset);
	else
		NewName = InNewName;

	FHoudiniEngineUtils::RenameObject(InAsset, *NewName);

	const FSoftObjectPath NewPath = FSoftObjectPath(InAsset);
	if (OldPath != NewPath)
	{
		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(OldPath, NewPath, true));
		AssetToolsModule.Get().RenameAssets(RenameData);
	}
}

void
FHoudiniEngineBakeUtils::RenameAndRelabelActor(AActor* InActor, const FString& InNewName, bool bMakeUniqueIfNotUnique)
{
	if (!IsValid(InActor))
		return;
	
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const FSoftObjectPath OldPath = FSoftObjectPath(InActor);

	FString NewName;
	if (bMakeUniqueIfNotUnique)
		NewName = MakeUniqueObjectNameIfNeeded(InActor->GetOuter(), InActor->GetClass(), InNewName, InActor);
	else
		NewName = InNewName;

	FHoudiniEngineUtils::RenameObject(InActor, *NewName);
	FHoudiniEngineRuntimeUtils::SetActorLabel(InActor, NewName);

	const FSoftObjectPath NewPath = FSoftObjectPath(InActor);
	if (OldPath != NewPath)
	{
		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(OldPath, NewPath, true));
		AssetToolsModule.Get().RenameAssets(RenameData);
	}
}

bool
FHoudiniEngineBakeUtils::DetachAndRenameBakedPDGOutputActor(
	AActor* InActor,
	const FString& InNewName,
	const FName& InFolderPath)
{
	if (!IsValid(InActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::DetachAndRenameBakedPDGOutputActor]: InActor is null."));
		return false;
	}

	if (InNewName.TrimStartAndEnd().IsEmpty())
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::DetachAndRenameBakedPDGOutputActor]: A valid actor name must be specified."));
		return false;
	}

	// Detach from parent
	InActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// Rename
	const bool bMakeUniqueIfNotUnique = true;
	RenameAndRelabelActor(InActor, InNewName, bMakeUniqueIfNotUnique);

	InActor->SetFolderPath(InFolderPath);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGWorkResultObject(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkResultArrayIndex,
	int32 InWorkResultObjectArrayIndex,
	const FHoudiniBakeSettings& BakeSettings,
	bool bInBakeToWorkResultActor,
	bool bInIsAutoBake,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors,
	FHoudiniBakedObjectData& BakedObjectData,
	TArray<EHoudiniOutputType> * InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> * InInstancerComponentTypesToBake,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNode))
		return false;

	if (!InNode->WorkResult.IsValidIndex(InWorkResultArrayIndex))
		return false;

	FTOPWorkResult& WorkResult = InNode->WorkResult[InWorkResultArrayIndex];
	if (!WorkResult.ResultObjects.IsValidIndex(InWorkResultObjectArrayIndex))
		return false;
	
	FTOPWorkResultObject& WorkResultObject = WorkResult.ResultObjects[InWorkResultObjectArrayIndex];
	TArray<UHoudiniOutput*>& Outputs = WorkResultObject.GetResultOutputs();
	if (Outputs.Num() == 0)
		return true;

	if (WorkResultObject.State != EPDGWorkResultState::Loaded)
	{
		if (bInIsAutoBake && WorkResultObject.AutoBakedSinceLastLoad())
		{
			HOUDINI_LOG_MESSAGE(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObject (%s) is not loaded but was auto-baked since its last load."), *WorkResultObject.Name);
			return true;
		}

		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObject (%s) is not loaded, cannot bake it."), *WorkResultObject.Name);
		return false;
	}

	AActor* WorkResultObjectActor = WorkResultObject.GetOutputActorOwner().GetOutputActor();
	if (!IsValid(WorkResultObjectActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObjectActor (%s) is null (unexpected since # Outputs > 0)"), *WorkResultObject.Name);
		return false;
	}

	// Find the previous bake output for this work result object
	FString Key;
	InNode->GetBakedWorkResultObjectOutputsKey(InWorkResultArrayIndex, InWorkResultObjectArrayIndex, Key);
	FHoudiniPDGWorkResultObjectBakedOutput& BakedOutputContainer = InNode->GetBakedWorkResultObjectsOutputs().FindOrAdd(Key);
	
	FHoudiniEngineBakeState BakeState(Outputs.Num(), BakedOutputContainer.BakedOutputs);

	UHoudiniAssetComponent* HoudiniAssetComponent = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InPDGAssetLink);
	check(IsValid(HoudiniAssetComponent));

	TArray<FHoudiniEngineBakedActor> WROBakedActors;
	BakeHoudiniOutputsToActors(
		HoudiniAssetComponent,
		Outputs,
		BakeState,
		WorkResultObjectActor->GetActorTransform(),
		InPDGAssetLink->BakeFolder,
		InPDGAssetLink->GetTemporaryCookFolder(),
		BakeSettings,
		InBakedActors,
		WROBakedActors, 
		BakedObjectData,
		InOutputTypesToBake,
		InInstancerComponentTypesToBake,
		bInBakeToWorkResultActor ? WorkResultObjectActor : nullptr,
		InFallbackWorldOutlinerFolder);

	// Set the PDG indices on the output baked actor entries
	FOutputActorOwner& OutputActorOwner = WorkResultObject.GetOutputActorOwner();
	AActor* const WROActor = OutputActorOwner.GetOutputActor();
	FHoudiniEngineBakedActor const * BakedWROActorEntry = nullptr;
	if (WROBakedActors.Num() > 0)
	{
		for (FHoudiniEngineBakedActor& BakedActorEntry : WROBakedActors)
		{
			BakedActorEntry.PDGWorkResultArrayIndex = InWorkResultArrayIndex;
			BakedActorEntry.PDGWorkItemIndex = WorkResult.WorkItemIndex;
			BakedActorEntry.PDGWorkResultObjectArrayIndex = InWorkResultObjectArrayIndex;

			if (WROActor && BakedActorEntry.Actor == WROActor)
			{
				BakedWROActorEntry = &BakedActorEntry;
			}
		}
	}

	// If anything was baked to WorkResultObjectActor, detach it from its parent
	if (bInBakeToWorkResultActor)
	{
		// if we re-used the temp actor as a bake actor, then remove its temp outputs
		constexpr bool bDeleteOutputActor = false;
		InNode->DeleteWorkResultObjectOutputs(InWorkResultArrayIndex, InWorkResultObjectArrayIndex, bDeleteOutputActor);
		if (WROActor)
		{
			if (BakedWROActorEntry)
			{
				OutputActorOwner.SetOutputActor(nullptr);
				const FString OldActorPath = FSoftObjectPath(WROActor).ToString();
				DetachAndRenameBakedPDGOutputActor(
					WROActor, BakedWROActorEntry->ActorBakeName.ToString(), BakedWROActorEntry->WorldOutlinerFolder);
				const FString NewActorPath = FSoftObjectPath(WROActor).ToString();
				if (OldActorPath != NewActorPath)
				{
					// Fix cached string reference in baked outputs to WROActor
					for (FHoudiniBakedOutput& BakedOutput : BakedOutputContainer.BakedOutputs)
					{
						for (auto& Entry : BakedOutput.BakedOutputObjects)
						{
							if (Entry.Value.Actor == OldActorPath)
								Entry.Value.Actor = NewActorPath;
						}
					}
				}
			}
			else
			{
				OutputActorOwner.DestroyOutputActor();
			}
		}
	}

	if (bInIsAutoBake)
		WorkResultObject.SetAutoBakedSinceLastLoad(true);
	
	OutBakedActors = MoveTemp(WROBakedActors);

	BakedOutputContainer.BakedOutputs = BakeState.GetNewBakedOutputs();
	
	// Ensure that the outer level (or actor in the case of OFPA) is marked as dirty so that references to the
	// output actors / objects and baked actor results are saved
	InNode->MarkPackageDirty();
	
	return true;
}

void
FHoudiniEngineBakeUtils::CheckPDGAutoBakeAfterResultObjectLoaded(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkItemHAPIIndex,
	int32 InWorkItemResultInfoIndex)
{
	TArray<FHoudiniEngineBakedActor> BakedActors;
	PDGAutoBakeAfterResultObjectLoaded(InPDGAssetLink, InNode, InWorkItemHAPIIndex, InWorkItemResultInfoIndex, BakedActors);
}

void
FHoudiniEngineBakeUtils::PDGAutoBakeAfterResultObjectLoaded(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkItemHAPIIndex,
	int32 InWorkItemResultInfoIndex,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors)
{
	if (!IsValid(InPDGAssetLink))
		return;

	if (!InPDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded)
		return;

	if (!IsValid(InNode))
		return;

	// Check if the node is ready for baking: all work items must be complete
	bool bDoNotBake = false;
	if (!InNode->AreAllWorkItemsComplete() || (!InPDGAssetLink->IsAutoBakeNodesWithFailedWorkItemsEnabled() && InNode->AnyWorkItemsFailed()))
		bDoNotBake = true;

	// Check if the node is ready for baking: all work items must be loaded
	if (!bDoNotBake)
	{
		for (const FTOPWorkResult& WorkResult : InNode->WorkResult)
		{
			for (const FTOPWorkResultObject& WRO : WorkResult.ResultObjects)
			{
				if (WRO.State != EPDGWorkResultState::Loaded && !WRO.AutoBakedSinceLastLoad())
				{
					bDoNotBake = true;
					break;
				}
			}
			if (bDoNotBake)
				break;
		}
	}

	if (!bDoNotBake)
	{
		// Check which outputs are selected for baking: selected node, selected network or all
		// And only bake if the node falls within the criteria
		UTOPNetwork const * const SelectedTOPNetwork = InPDGAssetLink->GetSelectedTOPNetwork();
		UTOPNode const * const SelectedTOPNode = InPDGAssetLink->GetSelectedTOPNode();
		switch (InPDGAssetLink->PDGBakeSelectionOption)
		{
			case EPDGBakeSelectionOption::SelectedNetwork:
				if (!IsValid(SelectedTOPNetwork) || !InNode->IsParentTOPNetwork(SelectedTOPNetwork))
				{
					HOUDINI_LOG_WARNING(
						TEXT("Not baking Node %s (Net %s): not in selected network"),
						InNode ? *InNode->GetName() : TEXT(""),
						SelectedTOPNetwork ? *SelectedTOPNetwork->GetName() : TEXT(""));
					bDoNotBake = true;
				}
				break;
			case EPDGBakeSelectionOption::SelectedNode:
				if (InNode != SelectedTOPNode)
				{
					HOUDINI_LOG_WARNING(
						TEXT("Not baking Node %s (Net %s): not the selected node"),
						InNode ? *InNode->GetName() : TEXT(""),
						SelectedTOPNetwork ? *SelectedTOPNetwork->GetName() : TEXT(""));
					bDoNotBake = true;
				}
				break;
			case EPDGBakeSelectionOption::All:
			default:
				break;
		}
	}

	if (bDoNotBake)
		return;

	TArray<FHoudiniEngineBakedActor> BakedActors;
	bool bSuccess = false;
	const bool bIsAutoBake = true;
	switch (InPDGAssetLink->HoudiniEngineBakeOption)
	{
		case EHoudiniEngineBakeOption::ToActor:
			bSuccess = FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, InNode, bIsAutoBake, InPDGAssetLink->PDGBakePackageReplaceMode, InPDGAssetLink->bRecenterBakedActors, BakedActors);
			break;

		case EHoudiniEngineBakeOption::ToBlueprint:
			bSuccess = FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(InPDGAssetLink, InNode, bIsAutoBake, InPDGAssetLink->PDGBakePackageReplaceMode, InPDGAssetLink->bRecenterBakedActors);
			break;

		default:
			HOUDINI_LOG_WARNING(TEXT("Unsupported HoudiniEngineBakeOption %i"), InPDGAssetLink->HoudiniEngineBakeOption);
	}

	if (bSuccess)
		OutBakedActors = MoveTemp(BakedActors);

	InPDGAssetLink->OnNodeAutoBaked(InNode, bSuccess);
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	bool bInBakeForBlueprint,
	bool bInIsAutoBake,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors,
	FHoudiniBakedObjectData& BakedObjectData) 
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNode))
		return false;

	// Determine the output world outliner folder path via the PDG asset link's
	// owner's folder path and name
	UObject* PDGOwner = InPDGAssetLink->GetOwnerActor();
	if (!PDGOwner)
		PDGOwner = InPDGAssetLink->GetOuter();
	const FName& FallbackWorldOutlinerFolderPath = GetOutputFolderPath(PDGOwner);

	// Determine the actor/package replacement settings
	FHoudiniBakeSettings BakeSettings;
	BakeSettings.bReplaceActors = !bInBakeForBlueprint && InPDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
	BakeSettings.bReplaceAssets = InPDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;

	// Determine the output types to bake: don't bake landscapes in blueprint baking mode
	TArray<EHoudiniOutputType> OutputTypesToBake;
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake;
	if (bInBakeForBlueprint)
	{
		OutputTypesToBake.Add(EHoudiniOutputType::Mesh);
		OutputTypesToBake.Add(EHoudiniOutputType::Instancer);
		OutputTypesToBake.Add(EHoudiniOutputType::Curve);

		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::StaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::InstancedStaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::MeshSplitInstancerComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::FoliageAsHierarchicalInstancedStaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::GeometryCollectionComponent);
		//InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::InstancedActorComponent);
	}
	
	const int32 NumWorkResults = InNode->WorkResult.Num();
	FScopedSlowTask Progress(NumWorkResults, FText::FromString(FString::Printf(TEXT("Baking PDG Node Output %s ..."), *InNode->GetName())));
	Progress.MakeDialog();

	TArray<FHoudiniEngineBakedActor> OurBakedActors;
	TArray<FHoudiniEngineBakedActor> WorkResultObjectBakedActors;
	for (int32 WorkResultArrayIdx = 0; WorkResultArrayIdx < NumWorkResults; ++WorkResultArrayIdx)
	{
		// Bug: #126086
		// Fixed ensure failure due to invalid amount of work passed to the FSlowTask
		Progress.EnterProgressFrame(1.0f);

		FTOPWorkResult& WorkResult = InNode->WorkResult[WorkResultArrayIdx];
		const int32 NumWorkResultObjects = WorkResult.ResultObjects.Num();
		for (int32 WorkResultObjectArrayIdx = 0; WorkResultObjectArrayIdx < NumWorkResultObjects; ++WorkResultObjectArrayIdx)
		{
			WorkResultObjectBakedActors.Reset();			

			BakePDGWorkResultObject(
				InPDGAssetLink,
				InNode,
				WorkResultArrayIdx,
				WorkResultObjectArrayIdx,
				BakeSettings,
				!bInBakeForBlueprint,
				bInIsAutoBake,
				OurBakedActors,
				WorkResultObjectBakedActors,
				BakedObjectData,
				OutputTypesToBake.Num() > 0 ? &OutputTypesToBake : nullptr,
				InstancerComponentTypesToBake.Num() > 0 ? &InstancerComponentTypesToBake : nullptr,
				FallbackWorldOutlinerFolderPath.ToString()
			);

			OurBakedActors.Append(WorkResultObjectBakedActors);
		}
	}

	OutBakedActors = MoveTemp(OurBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink, 
	UTOPNode* InTOPNode, 
	bool bInIsAutoBake, 
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
	bool bInRecenterBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors)
{
	FHoudiniBakedObjectData BakedObjectData;

	const bool bBakeBlueprints = false;

	bool bSuccess = BakePDGTOPNodeOutputsKeepActors(
		InPDGAssetLink, InTOPNode, bBakeBlueprints, bInIsAutoBake, InPDGBakePackageReplaceMode, OutBakedActors, BakedObjectData);

	SaveBakedPackages(BakedObjectData.PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && OutBakedActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : OutBakedActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && OutBakedActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakedObjectData.BakeStats.NumPackagesCreated, BakedObjectData.BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNetwork* InNetwork,
	bool bInBakeForBlueprint,
	bool bInIsAutoBake,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	TArray<FHoudiniEngineBakedActor>& BakedActors,
	FHoudiniBakedObjectData& BakedObjectData)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNetwork))
		return false;

	bool bSuccess = true;
	for (UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;

		bSuccess &= BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, Node, bInBakeForBlueprint, bInIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, BakedObjectData);
	}

	return bSuccess;
}

bool FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	EPDGBakeSelectionOption InBakeSelectionOption,
	EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	bool bInRecenterBakedActors,
	FHoudiniBakedObjectData& BakedObjectData, 
	TArray<FHoudiniEngineBakedActor>& BakedActors)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	const bool bBakeBlueprints = false;
	const bool bIsAutoBake = false;

	bool bSuccess = true;
	switch (InBakeSelectionOption)
	{
	case EPDGBakeSelectionOption::All:
		for (UTOPNetwork* Network : InPDGAssetLink->AllTOPNetworks)
		{
			if (!IsValid(Network))
				continue;

			for (UTOPNode* Node : Network->AllTOPNodes)
			{
				if (!IsValid(Node))
					continue;

				bSuccess &= BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, Node, bBakeBlueprints, bIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, BakedObjectData);
			}
		}
		break;

	case EPDGBakeSelectionOption::SelectedNetwork:
		bSuccess = BakePDGTOPNetworkOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNetwork(), bBakeBlueprints, bIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, BakedObjectData);
		break;

	case EPDGBakeSelectionOption::SelectedNode:
		bSuccess = BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNode(), bBakeBlueprints, bIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, BakedObjectData);
		break;
	}

	SaveBakedPackages(BakedObjectData.PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && BakedActors.Num() > 0)
		GEditor->SelectNone(false, true);

	for (const FHoudiniEngineBakedActor& Entry : BakedActors)
	{
		if (!IsValid(Entry.Actor))
			continue;

		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}

	if (GEditor && BakedActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakedObjectData.BakeStats.NumPackagesCreated, BakedObjectData.BakeStats.NumPackagesUpdated });
		FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(Msg));
	}

	// Broadcast that the bake is complete
	InPDGAssetLink->HandleOnPostBake(bSuccess);

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink, 
	EPDGBakeSelectionOption InBakeSelectionOption, 
	EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
	bool bInRecenterBakedActors)
{
	FHoudiniBakedObjectData BakedObjectData;
	TArray<FHoudiniEngineBakedActor> BakedActors;

	bool bSuccess = BakePDGAssetLinkOutputsKeepActors(
		InPDGAssetLink,
		InBakeSelectionOption,
		InPDGBakePackageReplaceMode,
		bInRecenterBakedActors,
		BakedObjectData,
		BakedActors);

	return bSuccess;
}
bool
FHoudiniEngineBakeUtils::BakeBlueprintsFromBakedActors(
	const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
	const FHoudiniBakeSettings& BakeSettings,
	const FString& InHoudiniAssetName,
	const FString& InHoudiniAssetActorName,
	const FDirectoryPath& InBakeFolder,
	TArray<FHoudiniBakedOutput>* const InNonPDGBakedOutputs,
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>* const InPDGBakedOutputs,
	FHoudiniBakedObjectData& BakedObjectData)
{
	// // Clear selection
	// if (GEditor)
	// {
	// 	GEditor->SelectNone(false, true);
	// 	GEditor->NoteSelectionChange();
	// }

	// Iterate over the baked actors. An actor might appear multiple times if multiple OutputComponents were
	// baked to the same actor, so keep track of actors we have already processed in BakedActorSet
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	const bool bIsAssetEditorSubsystemValid = IsValid(AssetEditorSubsystem);
	TArray<UObject*> AssetsToReOpenEditors;
	TMap<AActor*, UBlueprint*> BakedActorMap;

	for (const FHoudiniEngineBakedActor& Entry : InBakedActors)
	{
		AActor *Actor = Entry.Actor;
		
		if (!IsValid(Actor))
			continue;

		// If we have a previously baked a blueprint, get the bake counter from it so that both replace and increment
		// is consistent with the bake counter
		int32 BakeCounter = 0;
		FHoudiniBakedOutputObject* BakedOutputObject = nullptr;
		// Get the baked output object
		if (Entry.PDGWorkResultArrayIndex >= 0 && Entry.PDGWorkItemIndex >= 0 && Entry.PDGWorkResultObjectArrayIndex >= 0 && InPDGBakedOutputs)
		{
			const FString Key = UTOPNode::GetBakedWorkResultObjectOutputsKey(Entry.PDGWorkResultArrayIndex, Entry.PDGWorkResultObjectArrayIndex);
			FHoudiniPDGWorkResultObjectBakedOutput* WorkResultObjectBakedOutput = InPDGBakedOutputs->Find(Key);
			if (WorkResultObjectBakedOutput)
			{
				if (Entry.OutputIndex >= 0 && WorkResultObjectBakedOutput->BakedOutputs.IsValidIndex(Entry.OutputIndex))
				{
					BakedOutputObject = WorkResultObjectBakedOutput->BakedOutputs[Entry.OutputIndex].BakedOutputObjects.Find(Entry.OutputObjectIdentifier);
				}
			}
		}
		else if (Entry.OutputIndex >= 0 && InNonPDGBakedOutputs)
		{
			if (Entry.OutputIndex >= 0 && InNonPDGBakedOutputs->IsValidIndex(Entry.OutputIndex))
			{
				BakedOutputObject = (*InNonPDGBakedOutputs)[Entry.OutputIndex].BakedOutputObjects.Find(Entry.OutputObjectIdentifier);
			}
		}

		if (BakedActorMap.Contains(Actor))
		{
			// Record the blueprint as the previous bake blueprint and clear the info of the temp bake actor/component
			if (BakedOutputObject)
			{
				UBlueprint* const BakedBlueprint = BakedActorMap[Actor];
				if (BakedBlueprint)
					BakedOutputObject->Blueprint = FSoftObjectPath(BakedBlueprint).ToString();
				else
					BakedOutputObject->Blueprint.Empty();
				BakedOutputObject->Actor.Empty();
				// TODO: Set the baked component to the corresponding component in the blueprint?
				BakedOutputObject->BakedComponent.Empty();
			}
			continue;
		}

		// Add a placeholder entry since we've started processing the actor, we'll replace the null with the blueprint
		// if successful and leave it as null if the bake fails (the actor will then be skipped if it appears in the
		// array again).
		BakedActorMap.Add(Actor, nullptr);

		UObject* Asset = nullptr;

		// Recenter the actor to its bounding box center
		if (BakeSettings.bRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Actor);

		// Create package for out Blueprint
		FString BlueprintName;

		// For instancers we determine the bake folder from the instancer,
		// for everything else we use the baked object's bake folder
		// If all of that is blank, we fall back to InBakeFolder.
		FString BakeFolderPath;
		if (Entry.bInstancerOutput)
			BakeFolderPath = Entry.InstancerPackageParams.BakeFolder;
		else
			BakeFolderPath = Entry.BakeFolderPath;
		if (BakeFolderPath.IsEmpty())
			BakeFolderPath = InBakeFolder.Path;
		
		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
            EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
            PackageParams,
            FHoudiniOutputObjectIdentifier(),
            BakeFolderPath,
            Entry.ActorBakeName.ToString() + "_BP",
            InHoudiniAssetName,
            InHoudiniAssetActorName,
            AssetPackageReplaceMode);
		
		if (BakedOutputObject)
		{
			UBlueprint* const PreviousBlueprint = BakedOutputObject->GetBlueprintIfValid();
			if (IsValid(PreviousBlueprint))
			{
				if (PackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBlueprint))
				{
					PackageParams.GetBakeCounterFromBakedAsset(PreviousBlueprint, BakeCounter);
				}
			}
		}

		UPackage* Package = PackageParams.CreatePackageForObject(BlueprintName, BakeCounter);
		
		if (!IsValid(Package))
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find or create a package for the blueprint of %s"), *(Actor->GetPathName()));
			continue;
		}

		BakedObjectData.BakeStats.NotifyPackageCreated(1);

		if (!Package->IsFullyLoaded())
			Package->FullyLoad();

		//Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(*BlueprintName, Package, Actor, false);
		// Find existing asset first (only relevant if we are in replacement mode). If the existing asset has a
		// different base class than the incoming actor, we reparent the blueprint to the new base class before
		// clearing the SCS graph and repopulating it from the temp actor.
		Asset = StaticFindObjectFast(UBlueprint::StaticClass(), Package, FName(*BlueprintName));
		if (IsValid(Asset))
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
			if (IsValid(Blueprint))
			{
				if (Blueprint->GeneratedClass && Blueprint->GeneratedClass != Actor->GetClass())
				{
					// Close editors opened on existing asset if applicable
					if (Asset && bIsAssetEditorSubsystemValid && AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr)
					{
						AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
						AssetsToReOpenEditors.Add(Asset);
					}

					Blueprint->ParentClass = Actor->GetClass();

					FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
				}
			}
		}
		else if (Asset && !IsValid(Asset))
		{
			// Rename to pending kill so that we can use the desired name
			const FString AssetPendingKillName(BlueprintName + "_PENDING_KILL");
			RenameAsset(Asset, AssetPendingKillName, true);
			Asset = nullptr;
		}

		bool bCreatedNewBlueprint = false;
		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
			Factory->ParentClass = Actor->GetClass();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
				BlueprintName, PackageParams.GetPackagePath(),
				UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));

			if (Asset)
				bCreatedNewBlueprint = true;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);

		if (!IsValid(Blueprint))
		{
			HOUDINI_LOG_WARNING(
				TEXT("Found an asset at %s/%s, but it was not a blueprint or was pending kill."),
				*(InBakeFolder.Path), *BlueprintName);
			
			continue;
		}

		if (bCreatedNewBlueprint)
		{
			BakedObjectData.BakeStats.NotifyObjectsCreated(Blueprint->GetClass()->GetName(), 1);
		}
		else
		{
			BakedObjectData.BakeStats.NotifyObjectsUpdated(Blueprint->GetClass()->GetName(), 1);
		}
		
		// Close editors opened on existing asset if applicable
		if (Blueprint && bIsAssetEditorSubsystemValid && AssetEditorSubsystem->FindEditorForAsset(Blueprint, false) != nullptr)
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(Blueprint);
			AssetsToReOpenEditors.Add(Blueprint);
		}
		
		// Record the blueprint as the previous bake blueprint and clear the info of the temp bake actor/component
		if (BakedOutputObject)
		{
			BakedOutputObject->Blueprint = FSoftObjectPath(Blueprint).ToString();
			BakedOutputObject->Actor.Empty();
			// TODO: Set the baked component to the corresponding component in the blueprint?
			BakedOutputObject->BakedComponent.Empty();
		}
		
		BakedObjectData.Blueprints.Add(Blueprint);
		BakedActorMap[Actor] = Blueprint;

		// Clear old Blueprint Node tree
		{
			USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

			int32 NodeSize = SCS->GetAllNodes().Num();
			for (int32 n = NodeSize - 1; n >= 0; --n)
				SCS->RemoveNode(SCS->GetAllNodes()[n]);
		}

		constexpr bool bRenameComponents = true;
		FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(Actor, Blueprint, bRenameComponents);

		// Save the created BP package.
		Package->MarkPackageDirty();
		BakedObjectData.PackagesToSave.Add(Package);
	}

	// Destroy the actors that were baked
	for (const auto& BakedActorEntry : BakedActorMap)
	{
		AActor* const Actor = BakedActorEntry.Key;
		if (!IsValid(Actor))
			continue;

		UWorld* World = Actor->GetWorld();
		if (!World)
			World = GWorld;
		
		if (World)
			World->EditorDestroyActor(Actor, true);
	}

	// Re-open asset editors for updated blueprints that were open in editors
	if (bIsAssetEditorSubsystemValid && AssetsToReOpenEditors.Num() > 0)
	{
		for (UObject* Asset : AssetsToReOpenEditors)
		{
			if (IsValid(Asset))
			{
				AssetEditorSubsystem->OpenEditorForAsset(Asset);
			}
		}
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	bool bInIsAutoBake,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	bool bInRecenterBakedActors,
	FHoudiniBakedObjectData& BakedObjectData)
{
	TArray<AActor*> BPActors;

	if (!IsValid(InPDGAssetLink))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InPDGAssetLink is null"));
		return false;
	}
		
	if (!IsValid(InNode))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InNode is null"));
		return false;
	}

	FHoudiniBakeSettings BakeSettings;
	BakeSettings.bReplaceAssets = InPDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
	BakeSettings.bRecenterBakedActors = bInRecenterBakedActors;

	// Bake PDG output to new actors
	// bInBakeForBlueprint == true will skip landscapes and instanced actor components
	const bool bInBakeForBlueprint = true;
	TArray<FHoudiniEngineBakedActor> BakedActors;
	bool bSuccess = BakePDGTOPNodeOutputsKeepActors(
		InPDGAssetLink,
		InNode,
		bInBakeForBlueprint,
		bInIsAutoBake,
		InPDGBakePackageReplaceMode,
		BakedActors,
		BakedObjectData);

	if (bSuccess)
	{
		AActor* OwnerActor = InPDGAssetLink->GetOwnerActor();
		bSuccess = BakeBlueprintsFromBakedActors(
			BakedActors,
			BakeSettings,
			InPDGAssetLink->AssetName,
			IsValid(OwnerActor) ? OwnerActor->GetActorNameOrLabel() : FString(),
			InPDGAssetLink->BakeFolder,
			nullptr,
			&InNode->GetBakedWorkResultObjectsOutputs(),
			BakedObjectData);
	}
	
	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, bool bInIsAutoBake, const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, bool bInRecenterBakedActors)
{
	FHoudiniBakedObjectData BakedObjectData;
	
	if (!IsValid(InPDGAssetLink))
		return false;

	const bool bSuccess = BakePDGTOPNodeBlueprints(
		InPDGAssetLink,
		InTOPNode,
		bInIsAutoBake,
		InPDGBakePackageReplaceMode,
		bInRecenterBakedActors,
		BakedObjectData);

	// Compile the new/updated blueprints
	for (UBlueprint* const Blueprint : BakedObjectData.Blueprints)
	{
		if (!IsValid(Blueprint))
			continue;
		
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && BakedObjectData.Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(BakedObjectData.Blueprints.Num());
		for (UBlueprint* Blueprint : BakedObjectData.Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakedObjectData.BakeStats.NumPackagesCreated, BakedObjectData.BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkBlueprints(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNetwork* InNetwork,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	bool bInRecenterBakedActors,
	FHoudiniBakedObjectData& BakedObjectData)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNetwork))
		return false;

	const bool bIsAutoBake = false;
	bool bSuccess = true;
	for (UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;
		
		bSuccess &= BakePDGTOPNodeBlueprints(InPDGAssetLink, Node, bIsAutoBake, InPDGBakePackageReplaceMode, bInRecenterBakedActors, BakedObjectData);
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, const EPDGBakeSelectionOption InBakeSelectionOption, const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, bool bInRecenterBakedActors)
{
	FHoudiniBakedObjectData BakedObjectData;

	if (!IsValid(InPDGAssetLink))
		return false;

	const bool bIsAutoBake = false;
	bool bSuccess = true;
	switch(InBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (UTOPNetwork* Network : InPDGAssetLink->AllTOPNetworks)
			{
				if (!IsValid(Network))
					continue;
				
				for (UTOPNode* Node : Network->AllTOPNodes)
				{
					if (!IsValid(Node))
						continue;
					
					bSuccess &= BakePDGTOPNodeBlueprints(
									InPDGAssetLink, 
									Node, 
									bIsAutoBake, 
									InPDGBakePackageReplaceMode, 
									bInRecenterBakedActors, 
									BakedObjectData);
				}
			}
			break;

		case EPDGBakeSelectionOption::SelectedNetwork:
			bSuccess &= BakePDGTOPNetworkBlueprints(
				InPDGAssetLink,
				InPDGAssetLink->GetSelectedTOPNetwork(),
				InPDGBakePackageReplaceMode,
				bInRecenterBakedActors,
				BakedObjectData);
			break;

		case EPDGBakeSelectionOption::SelectedNode:
			bSuccess &= BakePDGTOPNodeBlueprints(
				InPDGAssetLink,
				InPDGAssetLink->GetSelectedTOPNode(),
				bIsAutoBake,
				InPDGBakePackageReplaceMode,
				bInRecenterBakedActors,
				BakedObjectData);
			break;
	}

	// Compile the new/updated blueprints
	for (UBlueprint* const Blueprint : BakedObjectData.Blueprints)
	{
		if (!IsValid(Blueprint))
			continue;
		
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	FHoudiniEngineBakeUtils::SaveBakedPackages(BakedObjectData.PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && BakedObjectData.Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(BakedObjectData.Blueprints.Num());
		for (UBlueprint* Blueprint : BakedObjectData.Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakedObjectData.BakeStats.NumPackagesCreated, BakedObjectData.BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// Broadcast that the bake is complete
	InPDGAssetLink->HandleOnPostBake(bSuccess);
	
	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
	const FString& InLevelPath,
	ULevel*& OutDesiredLevel,
	UWorld*& OutDesiredWorld,
	bool& OutCreatedPackage)
{
	OutDesiredLevel = nullptr;
	OutDesiredWorld = nullptr;
	if (InLevelPath.IsEmpty())
	{
		OutDesiredWorld = GWorld;
		OutDesiredLevel = GWorld->GetCurrentLevel();
	}
	else
	{
		OutCreatedPackage = false;

		UWorld* FoundWorld = nullptr;
		ULevel* FoundLevel = nullptr;		
		bool bActorInWorld = false;
		if (FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			GWorld,
			InLevelPath,
			true,
			FoundWorld,
			FoundLevel,
			OutCreatedPackage,
			bActorInWorld))
		{
			OutDesiredLevel = FoundLevel;
			OutDesiredWorld = FoundWorld;
		}
	}

	return ((OutDesiredWorld != nullptr) && (OutDesiredLevel != nullptr));
}


bool
FHoudiniEngineBakeUtils::FindBakedActor(
	const FString& InBakeActorName,
	const TSubclassOf<AActor>& InBakeActorClass,
	ULevel* InLevel,
	AActor*& OutActor,
	bool bInNoPendingKillActors,
	bool bRenamePendingKillActor)
{
	OutActor = nullptr;
	
	if (!IsValid(InLevel))
		return false;

	UWorld* const World = InLevel->GetWorld();
	if (!IsValid(World))
		return false;

	// Look for an actor with the given name in the world
	const FName BakeActorFName(InBakeActorName);
	const TSubclassOf<AActor> ActorClass = IsValid(InBakeActorClass.Get()) ? InBakeActorClass.Get() : AActor::StaticClass();
	AActor* FoundActor = Cast<AActor>(StaticFindObjectFast(ActorClass.Get(), InLevel, BakeActorFName));

	// If we found an actor and it is pending kill, rename it and don't use it
	if (FoundActor)
	{
		if (!IsValid(FoundActor))
		{
			if (bRenamePendingKillActor)
			{
				RenameAndRelabelActor(
					FoundActor,
                    *MakeUniqueObjectNameIfNeeded(
                        FoundActor->GetOuter(),
                        FoundActor->GetClass(),
                        FoundActor->GetActorNameOrLabel() + "_Pending_Kill",
                        FoundActor),
                    false);
			}
			if (bInNoPendingKillActors)
				FoundActor = nullptr;
			else
				OutActor = FoundActor;
		}
		else
		{
			OutActor = FoundActor;
		}
	}

	return true;
}

void FHoudiniEngineBakeUtils::FindUnrealBakeActor(
	const FHoudiniOutputObject& InOutputObject,
	const FHoudiniBakedOutputObject& InBakedOutputObject,
	const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
	ULevel* InLevel,
	FName InDefaultActorName,
	const FHoudiniBakeSettings& BakeSettings,
	AActor* InFallbackActor,
	AActor*& OutFoundActor,
	bool& bOutHasBakeActorName,
	FName& OutBakeActorName)
{
	// Determine the desired actor class via unreal_bake_actor_class
	TSubclassOf<AActor> BakeActorClass = GetBakeActorClassOverride(InOutputObject);
	if (!IsValid(BakeActorClass.Get()))
		BakeActorClass = AActor::StaticClass();
	
	// Determine desired actor name via unreal_bake_actor, fallback to InDefaultActorName
	OutBakeActorName = NAME_None;
	OutFoundActor = nullptr;
	bOutHasBakeActorName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR);

	FString BakeActorNameStr;

	if (bOutHasBakeActorName)
	{
		BakeActorNameStr = InOutputObject.CachedAttributes[HAPI_UNREAL_ATTRIB_BAKE_ACTOR];
	}

	if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
	{
		BakeActorNameStr = InDefaultActorName.ToString();
		bOutHasBakeActorName = true;
	}

	if (bOutHasBakeActorName)
	{
		if (BakeActorNameStr.IsEmpty())
		{
			OutBakeActorName = NAME_None;
			bOutHasBakeActorName = false;
		}
		else
		{
			OutBakeActorName = FName(BakeActorNameStr, NAME_NO_NUMBER_INTERNAL);
			// We have a bake actor name, look for the actor
			AActor* BakeNameActor = nullptr;
			if (FindBakedActor(BakeActorNameStr, BakeActorClass, InLevel, BakeNameActor))
			{
				// Found an actor with that name, check that we "own" it (we created in during baking previously)
				AActor* IncrementedBakedActor = nullptr;
				for (const FHoudiniEngineBakedActor& BakedActor : InAllBakedActors)
				{
					if (!IsValid(BakedActor.Actor))
						continue;
					// Ensure the class is of the appropriate type
					if (!BakedActor.Actor->IsA(BakeActorClass.Get()))
						continue;
					if (BakedActor.Actor == BakeNameActor)
					{
						OutFoundActor = BakeNameActor;
						break;
					}
					else if (!IncrementedBakedActor && BakedActor.ActorBakeName == OutBakeActorName)
					{
						// Found an actor we have baked named OutBakeActorName_# (incremental version of our desired name)
						IncrementedBakedActor = BakedActor.Actor;
					}
				}
				if (!OutFoundActor && IncrementedBakedActor)
					OutFoundActor = IncrementedBakedActor;
			}
		}
	}

	// If unreal_bake_actor is not set, or is blank, fallback to InDefaultActorName
	if (!bOutHasBakeActorName || (OutBakeActorName.IsNone() || OutBakeActorName.ToString().TrimStartAndEnd().IsEmpty()))
	{
		OutBakeActorName = InDefaultActorName;
	}

	if (!OutFoundActor)
	{
		// If in replace mode, use previous bake actor if valid and in InLevel
		if (BakeSettings.bReplaceActors)
		{
			const FSoftObjectPath PrevActorPath(InBakedOutputObject.Actor);
			const FString ActorPath = PrevActorPath.IsSubobject()
                ? PrevActorPath.GetAssetPathString() + ":" + PrevActorPath.GetSubPathString()
                : PrevActorPath.GetAssetPathString();
			const FString LevelPath = IsValid(InLevel) ? InLevel->GetPathName() : "";
			if (PrevActorPath.IsValid() && (LevelPath.IsEmpty() || ActorPath.StartsWith(LevelPath)))
			{
				AActor* PrevBakedActor = InBakedOutputObject.GetActorIfValid();
				if (IsValid(PrevBakedActor) && PrevBakedActor->IsA(BakeActorClass.Get()))
					OutFoundActor = PrevBakedActor;
			}
		}

		// Fallback to InFallbackActor if valid and in InLevel
		if (!OutFoundActor && IsValid(InFallbackActor) && (!InLevel || InFallbackActor->GetLevel() == InLevel))
		{
			if (IsValid(InFallbackActor) && InFallbackActor->IsA(BakeActorClass.Get()))
				OutFoundActor = InFallbackActor;
		}
	}
}

AActor*
FHoudiniEngineBakeUtils::FindExistingActor_Bake(
	UWorld* InWorld,
	UHoudiniOutput* InOutput,	
	const FString& InActorName,
	const FString& InPackagePath,
	UWorld*& OutWorld,
	ULevel*& OutLevel,
	bool& bCreatedPackage)
{
	bCreatedPackage = false;

	// Try to Locate a previous actor
	AActor* FoundActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<AActor>(InWorld, InActorName, FoundActor);
	if (FoundActor)
		FoundActor->Destroy(); // nuke it!

	if (FoundActor)
	{
		// TODO: make sure that the found is actor is actually assigned to the level defined by package path.
		//       If the found actor is not from that level, it should be moved there.

		OutWorld = FoundActor->GetWorld();
		OutLevel = FoundActor->GetLevel();
	}
	else
	{
		// Actor is not present, BUT target package might be loaded. Find the appropriate World and Level for spawning. 
		bool bActorInWorld = false;
		const bool bResult = FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			InWorld,
			InPackagePath,
			true,
			OutWorld,
			OutLevel,
			bCreatedPackage,
			bActorInWorld);

		if (!bResult)
		{
			return nullptr;
		}

		if (!bActorInWorld)
		{
			// The OutLevel is not present in the current world which means we might
			// still find the tile actor in OutWorld.
			FoundActor = FHoudiniEngineRuntimeUtils::FindActorInWorldByLabelOrName<AActor>(OutWorld, InActorName);
		}
	}

	return FoundActor;
}

bool
FHoudiniEngineBakeUtils::CheckForAndRefineHoudiniProxyMesh(
	UHoudiniAssetComponent* InHoudiniAssetComponent,
	bool bInReplacePreviousBake,
	EHoudiniEngineBakeOption InBakeOption,
	bool bInRemoveHACOutputOnSuccess,
	bool bInRecenterBakedActors,
	bool& bOutNeedsReCook)
{
	if (!IsValid(InHoudiniAssetComponent))
	{
		return false;
	}
		
	// Handle proxies: if the output has any current proxies, first refine them
	bOutNeedsReCook = false;
	if (InHoudiniAssetComponent->HasAnyCurrentProxyOutput())
	{
		bool bNeedsRebuildOrDelete;
		bool bInvalidState;
		const bool bCookedDataAvailable = InHoudiniAssetComponent->IsHoudiniCookedDataAvailable(bNeedsRebuildOrDelete, bInvalidState);

		if (bCookedDataAvailable)
		{
			// Cook data is available, refine the mesh
			AHoudiniAssetActor* HoudiniActor = Cast<AHoudiniAssetActor>(InHoudiniAssetComponent->GetOwner());
			if (IsValid(HoudiniActor))
			{
				FHoudiniEngineCommands::RefineHoudiniProxyMeshActorArrayToStaticMeshes({ HoudiniActor });
			}
		}
		else if (!bNeedsRebuildOrDelete && !bInvalidState)
		{
			// A cook is needed: request the cook, but with no proxy and with a bake after cook
			InHoudiniAssetComponent->SetNoProxyMeshNextCookRequested(true);
			// Only
			if (!InHoudiniAssetComponent->IsBakeAfterNextCookEnabled())
				InHoudiniAssetComponent->SetBakeAfterNextCook(EHoudiniBakeAfterNextCook::Once);

			InHoudiniAssetComponent->MarkAsNeedCook();

			bOutNeedsReCook = true;

			// The cook has to complete first (asynchronously) before the bake can happen
			// The SetBakeAfterNextCookEnabled flag will result in a bake after cook
			return false;
		}
		else
		{
			// The HAC is in an unsupported state
			const EHoudiniAssetState AssetState = InHoudiniAssetComponent->GetAssetState();
			HOUDINI_LOG_ERROR(TEXT("Could not refine (in order to bake) %s, the asset is in an unsupported state: %s"), *(InHoudiniAssetComponent->GetPathName()), *(UEnum::GetValueAsString(AssetState)));
			return false;
		}
	}

	return true;
}

void
FHoudiniEngineBakeUtils::CenterActorToBoundingBoxCenter(AActor* InActor)
{
	if (!IsValid(InActor))
		return;

	USceneComponent * const RootComponent = InActor->GetRootComponent();
	if (!IsValid(RootComponent))
		return;

	// If the root component does not have any child components, then there is nothing to recenter
	if (RootComponent->GetNumChildrenComponents() <= 0)
		return;

	const bool bOnlyCollidingComponents = false;
	const bool bIncludeFromChildActors = true;

	// InActor->GetActorBounds(bOnlyCollidingComponents, Origin, BoxExtent, bIncludeFromChildActors);
	FBox Box(ForceInit);
	InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [&](const UPrimitiveComponent* InPrimComp)
	{
		// Only use non-editor-only components for the bounds calculation (to exclude things like editor only sprite/billboard components)
		if (InPrimComp->IsRegistered() && !InPrimComp->IsEditorOnly() &&
			(!bOnlyCollidingComponents || InPrimComp->IsCollisionEnabled()))
		{
			Box += InPrimComp->Bounds.GetBox();
		}
	});

	FVector3d Origin;
	FVector3d BoxExtent;
	Box.GetCenterAndExtents(Origin, BoxExtent);

	const FVector3d Delta = Origin - RootComponent->GetComponentLocation();
	// Actor->SetActorLocation(Origin);
	RootComponent->SetWorldLocation(Origin);

	for (USceneComponent* SceneComponent : RootComponent->GetAttachChildren())
	{
		if (!IsValid(SceneComponent))
			continue;
		
		SceneComponent->SetWorldLocation(SceneComponent->GetComponentLocation() - Delta);
	}
}

void
FHoudiniEngineBakeUtils::CenterActorsToBoundingBoxCenter(const TArray<AActor*>& InActors)
{
	for (AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
			continue;

		CenterActorToBoundingBoxCenter(Actor);
	}
}

USceneComponent*
FHoudiniEngineBakeUtils::GetActorRootComponent(AActor* InActor, bool bCreateIfMissing, EComponentMobility::Type InMobilityIfCreated)
{
	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (!IsValid(RootComponent))
	{
		RootComponent = NewObject<USceneComponent>(InActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);

		// Change the creation method so the component is listed in the details panels
		InActor->SetRootComponent(RootComponent);
		InActor->AddInstanceComponent(RootComponent);
		RootComponent->RegisterComponent();
		RootComponent->SetMobility(InMobilityIfCreated);
	}

	return RootComponent;
}

FString
FHoudiniEngineBakeUtils::MakeUniqueObjectNameIfNeeded(UObject* InOuter, const UClass* InClass, const FString& InName, UObject* InObjectThatWouldBeRenamed)
{
	if (IsValid(InObjectThatWouldBeRenamed))
	{
		const FName CurrentName = InObjectThatWouldBeRenamed->GetFName();
		if (CurrentName.ToString() == InName)
			return InName;

		// Check if the prefix matches (without counter suffix) the new name
		// In other words, if InName is 'my_actor' and the object is already an increment of it, 'my_actor_5' then
		// don't we can just keep the current name
		if (CurrentName.GetPlainNameString() == InName)
			return CurrentName.ToString();
	}

	UObject* ExistingObject = nullptr;
	FName CandidateName(InName);
	bool bAppendedNumber = false;
	// Do our own loop for generate suffixes as sequentially as possible. If this turns out to be expensive we can
	// revert to MakeUniqueObjectName.
	// return MakeUniqueObjectName(InOuter, InClass, CandidateName).ToString();
	do
	{
		if (!IsValid(InOuter))
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
			// UE5.1 deprecated ANY_PACKAGE
			ExistingObject = StaticFindFirstObject(nullptr, *(CandidateName.ToString()), EFindFirstObjectOptions::NativeFirst);
#else
			ExistingObject = StaticFindObject(nullptr, ANY_PACKAGE, *(CandidateName.ToString()));
#endif
		}
		else
		{
			ExistingObject = StaticFindObjectFast(nullptr, InOuter, CandidateName);
		}

		if (ExistingObject)
		{
			// We don't want to create unique names when actors are saved in their own package 
			// because we don't care about the name, we only care about the label.
			AActor* ExistingActor = Cast<AActor>(ExistingObject);
			AActor* RenamedActor = Cast<AActor>(InObjectThatWouldBeRenamed);
			if (ExistingActor && ExistingActor->IsPackageExternal() && RenamedActor && RenamedActor->IsPackageExternal())
			{
				return InName;
			}

			if (!bAppendedNumber)
			{
				const bool bSplitName = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				CandidateName = FName(*InName, NAME_EXTERNAL_TO_INTERNAL(1), bSplitName);
#else
				CandidateName = FName(*InName, NAME_EXTERNAL_TO_INTERNAL(1), FNAME_Add, bSplitName);
#endif
				bAppendedNumber = true;
			}
			else
			{
				CandidateName.SetNumber(CandidateName.GetNumber() + 1);
			}
			// CandidateName = FString::Printf(TEXT("%s_%d"), *InName, ++Counter);
		}
	} while (ExistingObject);

	return CandidateName.ToString();
}

FName
FHoudiniEngineBakeUtils::GetOutlinerFolderPath(const FHoudiniAttributeResolver& Resolver, FName DefaultFolder)
{
	FString ResolvedString = Resolver.ResolveAttribute( HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, DefaultFolder.ToString(), true);
	return FName(ResolvedString);
}

bool
FHoudiniEngineBakeUtils::SetOutlinerFolderPath(AActor* InActor, FName Folder)
{
	if (!IsValid(InActor))
		return false;

	InActor->SetFolderPath(Folder);
	return true;
}

uint32
FHoudiniEngineBakeUtils::DestroyPreviousBakeOutput(
	FHoudiniBakedOutputObject& InBakedOutputObject,
	bool bInDestroyBakedComponent,
	bool bInDestroyBakedInstancedActors,
	bool bInDestroyBakedInstancedComponents)
{
	uint32 NumDeleted = 0;

	if (bInDestroyBakedComponent)
	{
		UActorComponent* Component = Cast<UActorComponent>(InBakedOutputObject.GetBakedComponentIfValid());
		if (Component)
		{
			if (RemovePreviouslyBakedComponent(Component))
			{
				InBakedOutputObject.BakedComponent = nullptr;
				NumDeleted++;
			}
		}
	}

	if (bInDestroyBakedInstancedActors)
	{
		for (const FString& ActorPathStr : InBakedOutputObject.InstancedActors)
		{
			const FSoftObjectPath ActorPath(ActorPathStr);

			if (!ActorPath.IsValid())
				continue;

			AActor* Actor = Cast<AActor>(ActorPath.TryLoad());
			if (IsValid(Actor))
			{
				UWorld* World = Actor->GetWorld();
				if (IsValid(World))
				{
#if WITH_EDITOR
					World->EditorDestroyActor(Actor, true);
#else
					World->DestroyActor(Actor);
#endif
					NumDeleted++;
				}
			}
		}
		InBakedOutputObject.InstancedActors.Empty();
	}

	if (bInDestroyBakedInstancedComponents)
	{
		for (const FString& ComponentPathStr : InBakedOutputObject.InstancedComponents)
		{
			const FSoftObjectPath ComponentPath(ComponentPathStr);

			if (!ComponentPath.IsValid())
				continue;

			UActorComponent* Component = Cast<UActorComponent>(ComponentPath.TryLoad());
			if (IsValid(Component))
			{
				if (RemovePreviouslyBakedComponent(Component))
					NumDeleted++;
			}
		}
		InBakedOutputObject.InstancedComponents.Empty();
	}
	
	return NumDeleted;
}

UMaterialInterface* 
FHoudiniEngineBakeUtils::BakeSingleMaterialToPackage(
	UMaterialInterface* InOriginalMaterial,
	const FHoudiniPackageParams& InPackageParams,
	FHoudiniBakedObjectData& BakedObjectData,
	TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap)
{
	if (!IsValid(InOriginalMaterial))
	{
		return nullptr;
	}

	// We only deal with materials.
	if (!InOriginalMaterial->IsA(UMaterial::StaticClass()) && !InOriginalMaterial->IsA(UMaterialInstance::StaticClass()))
	{
		return nullptr;
	}

	FString MaterialName = InOriginalMaterial->GetName();

	// Duplicate material resource.
	UMaterialInterface * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
		InOriginalMaterial, nullptr, MaterialName, InPackageParams, BakedObjectData, InOutAlreadyBakedMaterialsMap);

	if (!IsValid(DuplicatedMaterial))
		return nullptr;
	
	return DuplicatedMaterial;
}

UClass*
FHoudiniEngineBakeUtils::GetBakeActorClassOverride(const FName& InActorClassName)
{
	if (InActorClassName.IsNone())
		return nullptr;

	// Try to the find the user specified actor class
	const FString ActorClassNameString = *InActorClassName.ToString();
	UClass* FoundClass = LoadClass<AActor>(nullptr, *ActorClassNameString);
	if (!IsValid(FoundClass))
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		// UE5.1 deprecated ANY_PACKAGE
		FoundClass = FindFirstObjectSafe<UClass>(*ActorClassNameString, EFindFirstObjectOptions::NativeFirst); 
#else
		FoundClass = FindObjectSafe<UClass>(ANY_PACKAGE, *ActorClassNameString);
#endif
	}

	if (!IsValid(FoundClass))
		return nullptr;

	// The class must be a child of AActor
	if (!FoundClass->IsChildOf<AActor>())
		return nullptr;

	return FoundClass;
}

UClass*
FHoudiniEngineBakeUtils::GetBakeActorClassOverride(const FHoudiniOutputObject& InOutputObject)
{
	// Find the unreal_bake_actor_class attribute in InOutputObject
	const FName ActorClassName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS) ?
		FName(InOutputObject.CachedAttributes.FindChecked(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS)) : NAME_None;
	return GetBakeActorClassOverride(ActorClassName);
}

UActorFactory*
FHoudiniEngineBakeUtils::GetActorFactory(
	const FName& InActorClassName, 
	const FHoudiniBakeSettings& BakeSettings, 
	TSubclassOf<AActor>& OutActorClass, 
	const TSubclassOf<UActorFactory>& InFactoryClass, 
	UObject* const InAsset)
{
	if (!GEditor)
		return nullptr;

	// If grouping components under one actor, choose an empty actor factory.
	if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
	{
		OutActorClass = GetBakeActorClassOverride(InActorClassName);

		UActorFactory* ActorFactory = GEditor->FindActorFactoryByClass(UActorFactoryClass::StaticClass());
		return ActorFactory;

	}

	// If InActorClassName is not blank, try to find an actor factory that spawns actors of this class.
	OutActorClass = nullptr;
	if (!InActorClassName.IsNone())
	{
		UClass* ActorClass = GetBakeActorClassOverride(InActorClassName);
		if (IsValid(ActorClass))
		{
			OutActorClass = ActorClass;
			UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ActorClass);
			if (IsValid(ActorFactory))
				return ActorFactory;
			// If we could not find a factory that specifically spawns ActorClass, then fallback to the
			// UActorFactoryClass factory
			ActorFactory = GEditor->FindActorFactoryByClass(UActorFactoryClass::StaticClass());
			if (IsValid(ActorFactory))
				return ActorFactory;
		}
	}

	// If InActorClassName was blank, or we could not find a factory for it,
	// Then if InFactoryClass was specified, try to find a factory of that class
	UClass * ActorFactoryClass = InFactoryClass.Get();
	if (IsValid(ActorFactoryClass) && ActorFactoryClass != UActorFactoryEmptyActor::StaticClass())
	{
		UActorFactory* ActorFactory = GEditor->FindActorFactoryByClass(ActorFactoryClass);
		if (IsValid(ActorFactory))
			return ActorFactory;
	}

	// If we couldn't find a factory via InActorClassName or InFactoryClass, then if InAsset was specified try to find
	// a factory that spawns actors for InAsset
	if (IsValid(InAsset))
	{
		UActorFactory* ActorFactory = FActorFactoryAssetProxy::GetFactoryForAssetObject(InAsset);
		if (IsValid(ActorFactory))
			return ActorFactory;
	}

	if (IsValid(ActorFactoryClass))
	{
		// Return the empty actor factory if we had ignored it above
		UActorFactory* ActorFactory = GEditor->FindActorFactoryByClass(ActorFactoryClass);
		if (IsValid(ActorFactory))
			return ActorFactory;
	}

	HOUDINI_LOG_ERROR(
		TEXT("[FHoudiniEngineBakeUtils::GetActorFactory] Could not find actor factory:\n\tunreal_bake_actor_class = %s\n\tfallback actor factory class = %s\n\tasset = %s"),
		InActorClassName.IsNone() ? TEXT("not specified") : *InActorClassName.ToString(),
		IsValid(InFactoryClass.Get()) ? *InFactoryClass->GetName() : TEXT("null"),
		IsValid(InAsset) ? *InAsset->GetFullName() : TEXT("null"));
	
	return nullptr;
}

UActorFactory*
FHoudiniEngineBakeUtils::GetActorFactory(
	const FHoudiniOutputObject& InOutputObject, 
	const FHoudiniBakeSettings& BakeSettings, 
	TSubclassOf<AActor>& OutActorClass, 
	const TSubclassOf<UActorFactory>& InFactoryClass, UObject* InAsset)
{
	// Find the unreal_bake_actor_class attribute in InOutputObject
	const FName ActorClassName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS) ?
		FName(InOutputObject.CachedAttributes.FindChecked(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS)) : NAME_None;
	return GetActorFactory(ActorClassName, BakeSettings, OutActorClass, InFactoryClass, InAsset);
}

AActor*
FHoudiniEngineBakeUtils::SpawnBakeActor(
	UActorFactory* InActorFactory, 
	UObject* InAsset, 
	ULevel* InLevel,
	const FHoudiniBakeSettings & BakeSettings,
	const FTransform& InTransform, 
	const UHoudiniAssetComponent * InHAC, 
	const TSubclassOf<AActor>& InActorClass, 
	const FActorSpawnParameters& InSpawnParams)
{
	if (!IsValid(InActorFactory))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::SpawnBakeActor] Could not spawn an actor, since InActorFactory is nullptr."));
		return nullptr;
	}

	AActor* SpawnedActor = nullptr;

	if (BakeSettings.ActorBakeOption == EHoudiniEngineActorBakeOption::OneActorPerHDA)
	{
		// If we are grouping components, just create a default actor.
		UClass * ActorClass = InActorClass.Get();
		if (ActorClass == nullptr)
			ActorClass = AActor::StaticClass();

		SpawnedActor = InActorFactory->CreateActor(ActorClass, InLevel, InTransform, InSpawnParams);

		// Ensure there is a root component. It seems empty actor's don't have one.
		if (SpawnedActor->GetRootComponent() == nullptr)
		{
			USceneComponent* RootComponent = NewObject<USceneComponent>(SpawnedActor, USceneComponent::GetDefaultSceneRootVariableName());
			SpawnedActor->SetRootComponent(RootComponent);
		}
	}
	else if (InActorFactory->IsA<UActorFactoryClass>())
	{
		if (!IsValid(InActorClass.Get()))
		{
			HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::SpawnBakeActor] Could not spawn an actor: InActorFactory is a UActorFactoryClass, but InActorClass is nullptr."));
			return nullptr;
		}
		
		// UActorFactoryClass spawns an actor of the specified class (specified via InAsset to the CreateActor function)
		SpawnedActor = InActorFactory->CreateActor(InActorClass.Get(), InLevel, InTransform, InSpawnParams);
	}
	else
	{
		SpawnedActor = InActorFactory->CreateActor(InAsset, InLevel, InTransform, InSpawnParams);
	}

	if (IsValid(SpawnedActor))
	{
		PostSpawnBakeActor(SpawnedActor, InHAC);
	}
	
	return SpawnedActor;
}

void
FHoudiniEngineBakeUtils::PostSpawnBakeActor(AActor* const InSpawnedActor, UHoudiniAssetComponent const* const InHAC)
{
	if (!IsValid(InSpawnedActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::PostSpawnBakeActor] InSpawnedActor is null."));
		return;
	}

	if (!IsValid(InHAC))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::PostSpawnBakeActor] InHAC is null."));
		return;
	}

	// Copy the mobility of the HAC (root component of the houdini asset actor) to the root component of the spawned
	// bake actor
	USceneComponent* const BakedRootComponent = InSpawnedActor->GetRootComponent();
	if (IsValid(BakedRootComponent))
	{
		BakedRootComponent->SetMobility(InHAC->Mobility);
	}
}

void
FHoudiniEngineBakeUtils::RemoveBakedLevelInstances(
	UHoudiniAssetComponent* HoudiniAssetComponent, 
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FHoudiniBakeSettings& BakeSettings)
{
	// Re-using previously baked outputs for level instances is problematic, so to simplfiy
	// everything we just delete the previous outputs. If we are Replacing Actors, we delete
	// the old actors first.

	for (int Index = 0; Index < InBakedOutputs.Num(); Index++)
	{
		FHoudiniBakedOutput& BakedOutput = InBakedOutputs[Index];

		TSet<FHoudiniBakedOutputObjectIdentifier> OutputObjectsToRemove;

		for (auto& BakedOutputObject : BakedOutput.BakedOutputObjects)
		{
			auto & BakedObj = BakedOutputObject.Value;

			// If there are no level instance actors associated with this output object, ignore.
			if (BakedObj.LevelInstanceActors.IsEmpty())
				continue;

			if (BakeSettings.bReplaceActors)
			{
				for(const FString & Name : BakedObj.LevelInstanceActors)
				{
					ALevelInstance* LevelInstance = Cast<ALevelInstance>(
						StaticLoadObject(
							ALevelInstance::StaticClass(),
							nullptr, 
							*Name,
							nullptr, 
							LOAD_NoWarn, 
							nullptr));

					if (!IsValid(LevelInstance))
						continue;

					LevelInstance->Destroy();
				}
			}
			OutputObjectsToRemove.Add(BakedOutputObject.Key);
		}

		// Clean up any references
		for(auto Id : OutputObjectsToRemove)
		{
			BakedOutput.BakedOutputObjects.Remove(Id);
		}
	}
}

UUserDefinedStruct* FHoudiniEngineBakeUtils::DuplicateUserDefinedStruct(UUserDefinedStruct* UserStruct, UPackage * Package, FString & PackageName)
{
	ObjectTools::FPackageGroupName PGN;
	PGN.PackageName = Package->GetPathName();
	PGN.GroupName = TEXT("");
	PGN.ObjectName = PackageName;

	Package->FullyLoad();

	TSet<UPackage*> Others;
	UUserDefinedStruct* DuplicatedStruct = DuplicateObject<UUserDefinedStruct>(UserStruct, Package, *PackageName);
	CastChecked<UUserDefinedStructEditorData>(DuplicatedStruct->EditorData)->RecreateDefaultInstance();

	return DuplicatedStruct;
}

void FHoudiniBakeSettings::SetFromHAC(UHoudiniAssetComponent* HAC)
{
	bReplaceActors = HAC->bReplacePreviousBake;
	bReplaceAssets = HAC->bReplacePreviousBake;
	bRecenterBakedActors = HAC->bRecenterBakedActors;
	ActorBakeOption = HAC->ActorBakeOption;

	
}

#undef LOCTEXT_NAMESPACE
