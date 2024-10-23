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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEnginePrivatePCH.h"
#include "UnrealLandscapeTranslator.h"
#include "HoudiniStringResolver.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniLandscapeUtils.h"
#include "HoudiniLandscapeSplineTranslator.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "RawMesh.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "LandscapeStreamingProxy.h"
#include "HoudiniBakeLandscape.h"
#include "HoudiniEngineOutputStats.h"
#include "FileHelpers.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniMeshTranslator.h"
#include "LandscapeEdit.h"
#include "PackageTools.h"
#include "Containers/UnrealString.h"
#include "Components/AudioComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Landscape.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "LandscapeSplineActor.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "LandscapeEditLayer.h"
#endif

bool
FHoudiniLandscapeBake::BakeLandscapeLayer(
	FHoudiniPackageParams& PackageParams, 
	UHoudiniLandscapeTargetLayerOutput& LayerOutput,
	const FHoudiniBakeSettings& BakeSettings,
	FHoudiniBakedObjectData& BakedObjectData,
	FHoudiniClearedEditLayers& ClearedLayers)
{
	ALandscape* OutputLandscape = LayerOutput.Landscape;
	ULandscapeInfo* TargetLandscapeInfo = OutputLandscape->GetLandscapeInfo();
	FHoudiniExtents Extents = LayerOutput.Extents;

	if (!OutputLandscape->bCanHaveLayersContent)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Landscape {0} has no edit layers, so baking does nothing."), *OutputLandscape->GetActorLabel());
		return true;
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// For landscape layers baking is the act of copying cooked data to a baked layer. We do not need to do that if we already
	// wrote directly to the final layer.
	//---------------------------------------------------------------------------------------------------------------------------

	if (!LayerOutput.bCookedLayerRequiresBaking)
		return true;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* BakedLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(OutputLandscape, FName(LayerOutput.BakedEditLayer));
#else
	FLandscapeLayer* BakedLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(OutputLandscape, FName(LayerOutput.BakedEditLayer));
#endif
	ULandscapeLayerInfoObject* TargetLayerInfo = OutputLandscape->GetLandscapeInfo()->GetLayerInfoByName(FName(LayerOutput.TargetLayer));

	bool bWasLocked = LayerOutput.bLockLayer;
	if (LayerOutput.bWriteLockedLayers)
		LayerOutput.bLockLayer = false;

	//---------------------------------------------------------------------------------------------------------------------------
	// Clear the layer, but only once per bake.
	//---------------------------------------------------------------------------------------------------------------------------

	bool bIsHeightFieldLayer = LayerOutput.TargetLayer == "height";

	if (OutputLandscape->bHasLayersContent && LayerOutput.bClearLayer && 
		!ClearedLayers.Contains(LayerOutput.BakedEditLayer, LayerOutput.TargetLayer))
	{
		ClearedLayers.Add(LayerOutput.BakedEditLayer, LayerOutput.TargetLayer);
		if (bIsHeightFieldLayer)
		{
			OutputLandscape->ClearLayer(BakedLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);
		}
		else
		{
			HOUDINI_CHECK_RETURN(TargetLayerInfo, false);
			OutputLandscape->ClearPaintLayer(BakedLayer->Guid, TargetLayerInfo);
		}

	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Copy cooked layer to baked layer
	//---------------------------------------------------------------------------------------------------------------------------

	if (!bIsHeightFieldLayer)
	{
		HOUDINI_CHECK_RETURN(TargetLayerInfo, false);

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, BakedLayer->Guid, [&] { OutputLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });

		TArray<uint8_t> Values = FHoudiniLandscapeUtils::GetLayerData(OutputLandscape, Extents, FName(LayerOutput.CookedEditLayer), FName(LayerOutput.TargetLayer));

		if (LayerOutput.TargetLayer == HAPI_UNREAL_VISIBILITY_LAYER_NAME)
		{
			FAlphamapAccessor<false, false> AlphaAccessor(OutputLandscape->GetLandscapeInfo(), ALandscapeProxy::VisibilityLayer);

			AlphaAccessor.SetData(
				Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
				Values.GetData(),
				ELandscapeLayerPaintingRestriction::None);
		}
		else
		{
			FAlphamapAccessor<false, true> AlphaAccessor(OutputLandscape->GetLandscapeInfo(), TargetLayerInfo);

			AlphaAccessor.SetData(
				Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
				Values.GetData(),
				ELandscapeLayerPaintingRestriction::None);
		}
	}
	else
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		const FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayer(OutputLandscape, FName(LayerOutput.CookedEditLayer));
#else
		FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayer(OutputLandscape, FName(LayerOutput.CookedEditLayer));
#endif
		HOUDINI_CHECK_RETURN(EditLayer != nullptr, false);
		TArray<uint16_t> Values = FHoudiniLandscapeUtils::GetHeightData(OutputLandscape, Extents, EditLayer);

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, BakedLayer->Guid, [&] { OutputLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });

		FLandscapeEditDataInterface LandscapeEdit(TargetLandscapeInfo);
		FHeightmapAccessor<false> HeightmapAccessor(TargetLandscapeInfo);
		HeightmapAccessor.SetData(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData());

	}

	int EditLayerIndex = OutputLandscape->GetLayerIndex(FName(LayerOutput.BakedEditLayer));

	if (bWasLocked && BakedLayer)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		OutputLandscape->SetLayerLocked(EditLayerIndex, true);
#else
		BakedLayer->bLocked = true;
#endif
	}
	//---------------------------------------------------------------------------------------------------------------------------
	// Make sure baked layer is visible.
	//---------------------------------------------------------------------------------------------------------------------------
	if (EditLayerIndex != INDEX_NONE)
		OutputLandscape->SetLayerVisibility(EditLayerIndex, true);

	return true;
}

bool
FHoudiniLandscapeBake::BakeLandscape(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FHoudiniBakeSettings& BakeSettings,
	const FDirectoryPath& BakePath,
	TMap<ALandscape*, FHoudiniClearedEditLayers> & ClearedLandscapeLayers,
	FHoudiniBakedObjectData& BakedObjectData)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* Output = InAllOutputs[InOutputIndex];
	if (!IsValid(Output))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	TArray<UWorld*> LandscapeWorldsToUpdate;

	const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

	TArray<UHoudiniLandscapeTargetLayerOutput*> LayerOutputs;

	for (auto& Elem : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& ObjectIdentifier = Elem.Key;
		FHoudiniOutputObject& OutputObject = Elem.Value;
		bool bHasPreviousBakeData = false;
		FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, ObjectIdentifier, bHasPreviousBakeData);

		// Populate the package params for baking this output object.
		if (!IsValid(OutputObject.OutputObject))
			continue;

		if (OutputObject.OutputObject->IsA<UHoudiniLandscapePtr>())
		{
			HOUDINI_LOG_ERROR(TEXT("Old Landscape Data found, rebuild the HDA Actor"));
			continue;
		}

		FHoudiniPackageParams PackageParams;
		FHoudiniEngineBakeUtils::ResolvePackageParams(
			HoudiniAssetComponent,
			Output,
			Elem.Key,
			Elem.Value,
			bHasPreviousBakeData,
			FString(""),
			BakePath,
			BakeSettings,
			PackageParams,
			BakedObjectData);

		UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObject.OutputObject);
		HOUDINI_CHECK_RETURN(IsValid(LayerOutput), false);

		if (!IsValid(LayerOutput->Landscape))
		{
			HOUDINI_LOG_WARNING(TEXT("Cooked Landscape was not found, so nothing will be baked"));
			continue;
		}

		FHoudiniClearedEditLayers & ClearedLayers = ClearedLandscapeLayers.FindOrAdd(LayerOutput->Landscape);
		FHoudiniLandscapeBake::BakeLandscapeLayer(PackageParams, *LayerOutput, BakeSettings, BakedObjectData, ClearedLayers);

		LayerOutputs.Add(LayerOutput);

		InBakeState.SetNewBakedOutputObject(InOutputIndex, ObjectIdentifier, BakedOutputObject);
	}

	// Once layers are baked, delete the cooked layers if they existed.
	for(UHoudiniLandscapeTargetLayerOutput * LayerOutput : LayerOutputs)
	{
		if (LayerOutput->bCookedLayerRequiresBaking)
		{
			FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(LayerOutput->Landscape, FName(LayerOutput->CookedEditLayer));
		}
	}

	// Once each layer has been modified see if they need to be locked.
	for (auto& Elem : OutputObjects)
	{
		FHoudiniOutputObject& OutputObject = Elem.Value;
		UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObject.OutputObject);
		if (IsValid(LayerOutput))
			FHoudiniLandscapeUtils::ApplyLocks(LayerOutput);
	
	}

	if (BakedObjectData.PackagesToSave.Num() > 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(BakedObjectData.PackagesToSave, true, false);
	}

	if (BakedObjectData.PackagesToSave.Num() > 0)
	{
		// These packages were either created during the Bake process or they weren't
		// loaded in the first place so be sure to unload them again to preserve their "state".

		TArray<UPackage*> PackagesToUnload;
		for (UPackage* Package : BakedObjectData.PackagesToSave)
		{
			if (!Package->IsDirty())
				PackagesToUnload.Add(Package);
		}
		UPackageTools::UnloadPackages(PackagesToUnload);
	}

#if WITH_EDITOR
	FEditorDelegates::RefreshLevelBrowser.Broadcast();
	FEditorDelegates::RefreshAllBrowsers.Broadcast();
#endif

	return true;
}


TArray<FHoudiniEngineBakedActor>
FHoudiniLandscapeBake::MoveCookedToBakedLandscapes(
	const UHoudiniAssetComponent* HAC,
	const FName & InFallbackWorldOutlinerFolder,
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FHoudiniBakeSettings& BakeSettings,
	const FDirectoryPath& BakeFolder,
	FHoudiniBakedObjectData& BakedObjectData)
{
	TSet<ALandscape*> ProcessedLandscapes;
	TArray<FHoudiniEngineBakedActor> Results;

	for(int OutputIndex = 0; OutputIndex < InOutputs.Num(); OutputIndex++)
	{
		UHoudiniOutput* HoudiniOutput = InOutputs[OutputIndex];

		// Get a valid output layer from the output.
		if (!IsValid(HoudiniOutput) || HoudiniOutput->GetType() != EHoudiniOutputType::Landscape)
			continue;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = HoudiniOutput->GetOutputObjects();

		for (auto& Elem : OutputObjects)
		{
			UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(Elem.Value.OutputObject);
			if (!LayerOutput)
				continue;
				
			if (!IsValid(LayerOutput->Landscape))
				continue;

			bool bHasPreviousBakeData = false;
			FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(OutputIndex, Elem.Key, bHasPreviousBakeData);

			FDirectoryPath BakePath;
			BakePath.Path = LayerOutput->BakeFolder;

			FHoudiniPackageParams PackageParams;
			FHoudiniAttributeResolver Resolver;
			FHoudiniEngineBakeUtils::ResolvePackageParamsWithResolver(
				HAC,
				HoudiniOutput,
				Elem.Key,
				Elem.Value,
				bHasPreviousBakeData,
				FString(""), 
				BakePath,
				BakeSettings,
				PackageParams,
				Resolver,
				BakedObjectData);

			// Bake Material instance. 
			BakeMaterials(*LayerOutput, PackageParams, BakedObjectData);

			//---------------------------------------------------------------------------------------------------------------------------
			// Make sure we only process each parent landscape once.
			//---------------------------------------------------------------------------------------------------------------------------

			if (ProcessedLandscapes.Contains(LayerOutput->Landscape))
				continue;
			ProcessedLandscapes.Add(LayerOutput->Landscape);

			//---------------------------------------------------------------------------------------------------------------------------
			// Bake all the LayerInfoObjects, if they were not specified by the user, and patch up the result inside the landscape.
			//---------------------------------------------------------------------------------------------------------------------------

			ULandscapeInfo* LandscapeInfo = LayerOutput->Landscape->GetLandscapeInfo();
			LandscapeInfo->Modify();

			for(int Index = 0; Index < LayerOutput->LayerInfoObjects.Num(); Index++)
			{
				ULandscapeLayerInfoObject* CookedLayerInfoObject = LayerOutput->LayerInfoObjects[Index];

				FString TempFolder = HAC->GetTemporaryCookFolderOrDefault();

				if (CookedLayerInfoObject->GetPathName().StartsWith(TempFolder))
				{
					ULandscapeLayerInfoObject* BakedLayerInfo = CreateBakedLandscapeLayerInfoObject(
						PackageParams,
						LayerOutput->Landscape,
						CookedLayerInfoObject,
						BakedObjectData);

					LandscapeInfo->ReplaceLayer(CookedLayerInfoObject, BakedLayerInfo);

					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[Index];
					LayerSettings.LayerInfoObj = BakedLayerInfo;

					BakedOutputObject.LandscapeLayers.Emplace(BakedLayerInfo->LayerName, FSoftObjectPath(BakedLayerInfo).ToString());
				}
			}

			LandscapeInfo->LandscapeActor->ForceLayersFullUpdate();

			//---------------------------------------------------------------------------------------------------------------------------
			// Rename the landscape. Making a copy of the landscape and copying data would be slow, especially with world partition.
			//---------------------------------------------------------------------------------------------------------------------------

			if (LayerOutput->bCreatedLandscape)
			{
				// Rename the actor, make sure we only do this once.
				AActor* FoundActor = nullptr;
				ALandscape* ExistingLandscape = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscape>(
						LayerOutput->Landscape->GetWorld(), LayerOutput->BakedLandscapeName, FoundActor);

				if (ExistingLandscape && PackageParams.ReplaceMode == EPackageReplaceMode::ReplaceExistingAssets)
				{
					// Even though we found an existing landscape with the desired type, we're just going to destroy/replace
					// it for now.
					FHoudiniEngineUtils::RenameToUniqueActor(ExistingLandscape, LayerOutput->BakedLandscapeName + "_0");
					FHoudiniLandscapeRuntimeUtils::DestroyLandscape(ExistingLandscape);
				}

				BakedObjectData.BakeStats.NotifyPackageUpdated(1);

				FHoudiniEngineUtils::SafeRenameActor(LayerOutput->Landscape, LayerOutput->BakedLandscapeName);
				
				FName WorldOutlinerFolder =
					FHoudiniEngineBakeUtils::GetOutlinerFolderPath(Resolver, InFallbackWorldOutlinerFolder);

				if (!WorldOutlinerFolder.ToString().IsEmpty())
					LayerOutput->Landscape->SetFolderPath(FName(WorldOutlinerFolder));

				FHoudiniEngineBakedActor BakeActor(
						LayerOutput->Landscape,
						FName(LayerOutput->Landscape->GetActorLabel()),
						FName(WorldOutlinerFolder),
						OutputIndex,
						Elem.Key,
						nullptr, 
						nullptr, 
						nullptr,
						PackageParams.BakeFolder,
						PackageParams);

				Results.Add(BakeActor);
				BakedOutputObject.Landscape = *BakeActor.Actor->GetPathName();	
				
			}

			// Set the updated baked output object in the state
			InBakeState.SetNewBakedOutputObject(OutputIndex, Elem.Key, BakedOutputObject);
		}
	}

	// Clear old cooked data.
	for (int OutputIndex = 0; OutputIndex < InOutputs.Num(); OutputIndex++)
	{
		UHoudiniOutput* HoudiniOutput = InOutputs[OutputIndex];
		if (!IsValid(HoudiniOutput) || HoudiniOutput->GetType() != EHoudiniOutputType::Landscape)
			continue;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = HoudiniOutput->GetOutputObjects();

		for (auto& Elem : OutputObjects)
		{
			UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(Elem.Value.OutputObject);
			if (!LayerOutput)
				continue;

			LayerOutput->Landscape = nullptr;
		}
	}
	return Results;
}



ULandscapeLayerInfoObject* 
FHoudiniLandscapeBake::CreateBakedLandscapeLayerInfoObject(
	const FHoudiniPackageParams& PackageParams, 
	ALandscape* Landscape, 
	ULandscapeLayerInfoObject* LandscapeLayerInfoObject,
	FHoudiniBakedObjectData& BakedObjectData)
{
	ULandscapeLayerInfoObject * BakedObject = BakeGeneric(
		LandscapeLayerInfoObject,
		PackageParams,
		LandscapeLayerInfoObject->GetName(),
		BakedObjectData);

	BakedObject->MarkPackageDirty();

	return BakedObject;

}


void FHoudiniLandscapeBake::BakeMaterials(
	const UHoudiniLandscapeTargetLayerOutput& Layer,
	const FHoudiniPackageParams& PackageParams,
	FHoudiniBakedObjectData& BakedObjectData)
{
	UMaterialInterface * MaterialInstance = Layer.MaterialInstance;
	if (!IsValid(MaterialInstance))
		return;

	auto * BakedMaterialInstance = BakeGeneric(
		MaterialInstance,
		PackageParams,
		MaterialInstance->GetName(),
		BakedObjectData);

	//------------------------------------------------------------------------------------------------------------------------------
	// Replace the material instance.  If this is a parent landscape actor apply material change to every proxy, because
	// unreal will not do this for us.
	//------------------------------------------------------------------------------------------------------------------------------

	Layer.LandscapeProxy->LandscapeMaterial = BakedMaterialInstance;

	if (Layer.LandscapeProxy->IsA<ALandscape>())
	{
		ULandscapeInfo* Info = Layer.Landscape->GetLandscapeInfo();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>>& Proxies = Info->StreamingProxies;
		for (auto ProxyPtr : Proxies)
		{
			ALandscapeStreamingProxy* Proxy = ProxyPtr.Get();
#else
		TArray<ALandscapeStreamingProxy*>& Proxies = Info->Proxies;
		for (ALandscapeStreamingProxy* Proxy : Proxies)
		{
#endif	
			if (!IsValid(Proxy))
				continue;

			Proxy->LandscapeMaterial = BakedMaterialInstance;
		}
	}

	Layer.LandscapeProxy->GetLandscapeActor()->ForceUpdateLayersContent();
}

template<typename UObjectType>
UObjectType* FHoudiniLandscapeBake::BakeGeneric(
	UObjectType* CookedObject,
	const FHoudiniPackageParams& PackageParams,
	const FString& ObjectName,
	FHoudiniBakedObjectData& BakedObjectData)
{
	FHoudiniPackageParams LayerPackageParams = PackageParams;
	LayerPackageParams.ObjectName = ObjectName;
	FString CreatedPackageName;
	UPackage* Package = LayerPackageParams.CreatePackageForObject(CreatedPackageName);
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

	BakedObjectData.PackagesToSave.Add(Package);

	//------------------------------------------------------------------------------------------------------------------------------
	// Remove existing layer object
	//------------------------------------------------------------------------------------------------------------------------------

	UObjectType* ExistingObject = FindObject<UObjectType>(Package, *ObjectName);
	if (IsValid(ExistingObject))
	{
		BakedObjectData.BakeStats.NotifyObjectsReplaced(UObjectType::StaticClass()->GetName(), 1);
		BakedObjectData.BakeStats.NotifyPackageUpdated(1);
	}
	else
	{
		BakedObjectData.BakeStats.NotifyObjectsCreated(UObjectType::StaticClass()->GetName(), 1);
		BakedObjectData.BakeStats.NotifyPackageCreated(1);
	}

	//------------------------------------------------------------------------------------------------------------------------------
	// Replace the layer.
	//------------------------------------------------------------------------------------------------------------------------------

	UObjectType* DuplicatedObject = DuplicateObject<UObjectType>(CookedObject, Package, *ObjectName);
	Package->MarkPackageDirty();

	return DuplicatedObject;

}


bool
FHoudiniLandscapeBake::BakeLandscapeSplinesLayer(
	FHoudiniPackageParams& PackageParams,
	UHoudiniLandscapeSplineTargetLayerOutput& LayerOutput,
	FHoudiniClearedEditLayers& ClearedLayers,
	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers)
{
	ALandscape* const OutputLandscape = LayerOutput.Landscape;
	ULandscapeInfo* const TargetLandscapeInfo = OutputLandscape->GetLandscapeInfo();

	if (!OutputLandscape->bCanHaveLayersContent)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Landscape {0} has no edit layers, so baking does nothing."), *OutputLandscape->GetActorLabel());
		return true;
	}

	const FName BakedEditLayer = *LayerOutput.BakedEditLayer;

	// If the landscape has a reserved splines layer, then we don't create any named temp/bake layers on the landscape for splines

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	if (OutputLandscape->FindLayerOfType(ULandscapeEditLayerSplines::StaticClass()))
#else
	if (OutputLandscape->GetLandscapeSplinesReservedLayer())
#endif
	{
		FHoudiniLandscapeSplineApplyLayerData& LayerData = SegmentsToApplyToLayers.FindOrAdd({ OutputLandscape, BakedEditLayer });
		LayerData.bIsReservedSplineLayer = true;
		LayerData.Landscape = OutputLandscape;
		LayerData.EditLayerName = BakedEditLayer;
		return true;
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// For landscape layers baking is the act of copying cooked data to a baked layer. We do not need to do that if we already
	// wrote directly to the final layer.
	//---------------------------------------------------------------------------------------------------------------------------

	if (!LayerOutput.bCookedLayerRequiresBaking)
		return true;

	// Ensure that the baked layer exists
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* const BakedLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(OutputLandscape, BakedEditLayer);
#else
	FLandscapeLayer* const BakedLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(OutputLandscape, BakedEditLayer);
#endif

	//---------------------------------------------------------------------------------------------------------------------------
	// Clear the layer, but only once per bake.
	//---------------------------------------------------------------------------------------------------------------------------

	if (OutputLandscape->HasLayersContent() && LayerOutput.bClearLayer && 
		!ClearedLayers.Contains(LayerOutput.BakedEditLayer, LayerOutput.TargetLayer))
	{
		ClearedLayers.Add(LayerOutput.BakedEditLayer, LayerOutput.TargetLayer);
		OutputLandscape->ClearLayer(BakedLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Record the segments to apply to the baked layer
	//---------------------------------------------------------------------------------------------------------------------------
	FHoudiniLandscapeSplineApplyLayerData& LayerData = SegmentsToApplyToLayers.FindOrAdd({ OutputLandscape, BakedEditLayer });
	LayerData.bIsReservedSplineLayer = false;
	LayerData.Landscape = OutputLandscape;
	LayerData.EditLayerName = BakedEditLayer;
	LayerData.SegmentsToApply.Append(LayerOutput.Segments);

	// Delete the temp/cooked layer
	FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(OutputLandscape, FName(LayerOutput.CookedEditLayer));

	//---------------------------------------------------------------------------------------------------------------------------
	// Make sure baked layer is visible.
	//---------------------------------------------------------------------------------------------------------------------------
	int EditLayerIndex = OutputLandscape->GetLayerIndex(BakedEditLayer);
	if (EditLayerIndex != INDEX_NONE)
		OutputLandscape->SetLayerVisibility(EditLayerIndex, true);

	return true;
}

bool FHoudiniLandscapeBake::BakeLandscapeSplines(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniEngineBakeState& InBakeState,
	const FHoudiniBakeSettings& BakeSettings,
	const FDirectoryPath& BakePath,
	TMap<ALandscape*, FHoudiniClearedEditLayers>& ClearedLandscapeEditLayers,
	FHoudiniBakedObjectData& BakedObjectData)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* const Output = InAllOutputs[InOutputIndex];
	if (!IsValid(Output) || Output->GetType() != EHoudiniOutputType::LandscapeSpline)
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	TArray<UPackage*> PackagesToSave;
	TArray<UWorld*> LandscapeWorldsToUpdate;

	const EPackageReplaceMode AssetPackageReplaceMode = BakeSettings.bReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

	TArray<FHoudiniOutputObjectIdentifier> OutputObjectsBaked;

	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData> SegmentsToApplyToLayers;
	for (auto& Entry : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& ObjectIdentifier = Entry.Key;
		FHoudiniOutputObject& OutputObject = Entry.Value;
		bool bHasPreviousBakeData = false;
		FHoudiniBakedOutputObject BakedOutputObject = InBakeState.MakeNewBakedOutputObject(InOutputIndex, ObjectIdentifier, bHasPreviousBakeData);

		// Populate the package params for baking this output object.
		if (!IsValid(OutputObject.OutputObject))
			continue;

		UHoudiniLandscapeSplinesOutput* const SplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(OutputObject.OutputObject);
		if (!SplinesOutputObject)
			continue;

		OutputObjectsBaked.Add(Entry.Key);

		FHoudiniPackageParams PackageParams;
		FHoudiniEngineBakeUtils::ResolvePackageParams(
			HoudiniAssetComponent,
			Output,
			ObjectIdentifier,
			OutputObject,
			bHasPreviousBakeData,
			FString(""),
			BakePath,
			BakeSettings,
			PackageParams,
			BakedObjectData);

		const FString DesiredBakeName = PackageParams.GetPackageName();
		ALandscape* const Landscape = SplinesOutputObject->GetLandscape();

		// Bake the landscape spline actors: for this, in replace mode, delete the previous bake actor if any, and then rename
		// the temp landscape spline actor to the bake name.
		ALandscapeSplineActor* const ActorToBake = SplinesOutputObject->GetLandscapeSplineActor(); 
		if (IsValid(ActorToBake))
		{
			// For a replace, delete previous baked actor for this output identifier, if any. Also check that
			// it belongs to the same landscape.
			if (BakeSettings.bReplaceActors && !BakedOutputObject.Actor.IsEmpty()
					&& BakedOutputObject.Landscape == FSoftObjectPath(Landscape).ToString())
			{
				ULandscapeInfo* const LandscapeInfo = Landscape->GetLandscapeInfo();
				if (IsValid(LandscapeInfo))
				{
					// Only remove the previous actor if it has the LandscapeInfo object
					ALandscapeSplineActor* const PreviousActor = Cast<ALandscapeSplineActor>(BakedOutputObject.GetActorIfValid());
					if (IsValid(PreviousActor) && PreviousActor->GetLandscapeInfo() == LandscapeInfo)
					{
						LandscapeInfo->UnregisterSplineActor(PreviousActor);
						PreviousActor->Destroy();
					}
				}
			}

			// Rename to bake name
			FHoudiniEngineBakeUtils::RenameAndRelabelActor(ActorToBake, DesiredBakeName);

			// Record in baked object entry
			BakedOutputObject.Actor = FSoftObjectPath(ActorToBake).ToString();
			BakedOutputObject.BakedComponent = FSoftObjectPath(ActorToBake->GetSplinesComponent()).ToString();
		}
		else
		{
			// Non-WP case, there are no landscape spline actors, so we just track the landscape's LandscapeSplinesComponent
			BakedOutputObject.Actor.Empty();
			ULandscapeSplinesComponent* const SplinesComponent = SplinesOutputObject->GetLandscapeSplinesComponent();
			if (IsValid(SplinesComponent))
				BakedOutputObject.BakedComponent = FSoftObjectPath(SplinesComponent).ToString();
			else
				BakedOutputObject.BakedComponent.Empty();
		}

		// Updated baked object entry
		BakedOutputObject.ActorBakeName = *DesiredBakeName;
		BakedOutputObject.BakedObject.Empty();
		BakedOutputObject.Landscape = FSoftObjectPath(Landscape).ToString();

		// Delete temp edit layers, create baked layers and collect all segments per-landscape-layer.
		for (const auto& LayerEntry : SplinesOutputObject->GetLayerOutputs())
		{
			UHoudiniLandscapeSplineTargetLayerOutput* const LayerOutput = LayerEntry.Value;
			if (!IsValid(LayerOutput))
				continue;

			if (!IsValid(LayerOutput->Landscape))
			{
				HOUDINI_LOG_WARNING(TEXT("Cooked Landscape was not found, so nothing will be baked"));
				continue;
			}

			FHoudiniClearedEditLayers& ClearedLandscapeLayers = ClearedLandscapeEditLayers.FindOrAdd(LayerOutput->Landscape);
			BakeLandscapeSplinesLayer(PackageParams, *LayerOutput, ClearedLandscapeLayers, SegmentsToApplyToLayers);
		}

		// Save baked object entry in bake state
		InBakeState.SetNewBakedOutputObject(InOutputIndex, ObjectIdentifier, BakedOutputObject);
	}

	// Apply segments to baked/reserved layers
	FHoudiniLandscapeUtils::ApplySegmentsToLandscapeEditLayers(SegmentsToApplyToLayers);

	// Remove all output objects: since we don't duplicate anything the temp actors/segments
	// essentially become the baked ones. We have also already removed all temp layers.
	for (const FHoudiniOutputObjectIdentifier& Identifier : OutputObjectsBaked)
	{
		OutputObjects.Remove(Identifier);
	}

	if (PackagesToSave.Num() > 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
	}

	if (PackagesToSave.Num() > 0)
	{
		// These packages were either created during the Bake process or they weren't
		// loaded in the first place so be sure to unload them again to preserve their "state".

		TArray<UPackage*> PackagesToUnload;
		for (UPackage* Package : PackagesToSave)
		{
			if (!Package->IsDirty())
				PackagesToUnload.Add(Package);
		}
		UPackageTools::UnloadPackages(PackagesToUnload);
	}

#if WITH_EDITOR
	FEditorDelegates::RefreshLevelBrowser.Broadcast();
	FEditorDelegates::RefreshAllBrowsers.Broadcast();
#endif

	return true;
}
