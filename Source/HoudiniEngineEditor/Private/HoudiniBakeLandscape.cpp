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

bool
FHoudiniLandscapeBake::BakeLandscapeLayer(
	FHoudiniPackageParams& PackageParams, 
	UHoudiniLandscapeTargetLayerOutput& LayerOutput,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats,
	TSet<FString>& ClearedLayers)
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

	FLandscapeLayer* BakedLayer = FHoudiniLandscapeUtils::GetEditLayerForWriting(OutputLandscape, FName(LayerOutput.BakedEditLayer));
	ULandscapeLayerInfoObject* TargetLayerInfo = OutputLandscape->GetLandscapeInfo()->GetLayerInfoByName(FName(LayerOutput.TargetLayer));

	//---------------------------------------------------------------------------------------------------------------------------
	// Clear the layer, but only once per bake.
	//---------------------------------------------------------------------------------------------------------------------------

	bool bIsHeightFieldLayer = LayerOutput.TargetLayer == "height";

	if (OutputLandscape->bHasLayersContent && LayerOutput.bClearLayer && !ClearedLayers.Contains(LayerOutput.BakedEditLayer))
	{
		ClearedLayers.Add(LayerOutput.BakedEditLayer);
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
		FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayerForReading(OutputLandscape, FName(LayerOutput.CookedEditLayer));
		HOUDINI_CHECK_RETURN(EditLayer != nullptr, false);
		TArray<uint16_t> Values = FHoudiniLandscapeUtils::GetHeightData(OutputLandscape, Extents, EditLayer);

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, BakedLayer->Guid, [&] { OutputLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });

		FLandscapeEditDataInterface LandscapeEdit(TargetLandscapeInfo);
		FHeightmapAccessor<false> HeightmapAccessor(TargetLandscapeInfo);
		HeightmapAccessor.SetData(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData());

	}

	// Delete cooked layer.
	FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(OutputLandscape, FName(LayerOutput.CookedEditLayer));

	//---------------------------------------------------------------------------------------------------------------------------
	// Make sure baked layer is visible.
	//---------------------------------------------------------------------------------------------------------------------------
	int EditLayerIndex = OutputLandscape->GetLayerIndex(FName(LayerOutput.BakedEditLayer));
	if (EditLayerIndex != INDEX_NONE)
		OutputLandscape->SetLayerVisibility(EditLayerIndex, true);



	return true;
}

bool
FHoudiniLandscapeBake::BakeLandscape(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const FDirectoryPath& BakePath,
	FHoudiniEngineOutputStats& BakeStats,
	TSet<FString> & ClearedLandscapeLayers,
	TArray<UPackage*>& OutPackagesToSave)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* Output = InAllOutputs[InOutputIndex];
	if (!IsValid(Output))
		return false;

	// Find the previous baked output data for this output index. If an entry
	// does not exist, create entries up to and including this output index
	if (!InBakedOutputs.IsValidIndex(InOutputIndex))
		InBakedOutputs.SetNum(InOutputIndex + 1);

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniBakedOutput& BakedOutput = InBakedOutputs[InOutputIndex];
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = BakedOutput.BakedOutputObjects;
	TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;
	TArray<UPackage*> PackagesToSave;
	TArray<UWorld*> LandscapeWorldsToUpdate;

	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

	for (auto& Elem : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& ObjectIdentifier = Elem.Key;
		FHoudiniOutputObject& OutputObject = Elem.Value;
		FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(ObjectIdentifier);
		if (OldBakedOutputObjects.Contains(ObjectIdentifier))
			BakedOutputObject = OldBakedOutputObjects.FindChecked(ObjectIdentifier);

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
			FString(""),
			BakePath,
			bInReplaceAssets,
			PackageParams,
			OutPackagesToSave);

		UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObject.OutputObject);
		HOUDINI_CHECK_RETURN(IsValid(LayerOutput), false);

		if (!IsValid(LayerOutput->Landscape))
		{
			HOUDINI_LOG_WARNING(TEXT("Cooked Landscape was not found, so nothing will be baked"));
			continue;
		}

		FHoudiniLandscapeBake::BakeLandscapeLayer(PackageParams, *LayerOutput, bInReplaceActors, bInReplaceAssets, PackagesToSave, BakeStats, ClearedLandscapeLayers);
	}

	// Update the cached baked output data
	BakedOutput.BakedOutputObjects = NewBakedOutputObjects;

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


TArray<FHoudiniEngineBakedActor>
FHoudiniLandscapeBake::MoveCookedToBakedLandscapes(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const FName & InFallbackWorldOutlinerFolder,
	const TArray<UHoudiniOutput*>& InOutputs, 
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const FDirectoryPath& BakeFolder,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
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

			FHoudiniPackageParams PackageParams;
			FHoudiniEngineBakeUtils::ResolvePackageParams(
				HoudiniAssetComponent,
				HoudiniOutput,
				Elem.Key,
				Elem.Value,
				FString(""), 
				BakeFolder,
				bInReplaceAssets,
				PackageParams,
				OutPackagesToSave);

			// Bake Material instance. 
			BakeMaterials(*LayerOutput, PackageParams, OutPackagesToSave, BakeStats);

			//---------------------------------------------------------------------------------------------------------------------------
			// Make sure we only process each parent landscape once.
			//---------------------------------------------------------------------------------------------------------------------------

			if (ProcessedLandscapes.Contains(LayerOutput->Landscape))
				continue;
			ProcessedLandscapes.Add(LayerOutput->Landscape);

			//---------------------------------------------------------------------------------------------------------------------------
			// Bake all the LayerInfoObjects, and patch up the result inside the landscape.
			//---------------------------------------------------------------------------------------------------------------------------

			ULandscapeInfo* LandscapeInfo = LayerOutput->Landscape->GetLandscapeInfo();
			LandscapeInfo->Modify();

			for(int Index = 0; Index < LayerOutput->LayerInfoObjects.Num(); Index++)
			{
				ULandscapeLayerInfoObject* CookedLayerInfoObject = LayerOutput->LayerInfoObjects[Index];

				ULandscapeLayerInfoObject * BakedLayerInfo = CreateBakedLandscapeLayerInfoObject(
					PackageParams,
					LayerOutput->Landscape,
					CookedLayerInfoObject,
					OutPackagesToSave,
					BakeStats);

				LandscapeInfo->ReplaceLayer(CookedLayerInfoObject, BakedLayerInfo);

				FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[Index];
				LayerSettings.LayerInfoObj = BakedLayerInfo;
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

				if (ExistingLandscape && bInReplaceActors)
				{
					// Even though we found an existing landscape with the desired type, we're just going to destroy/replace
					// it for now.
					FHoudiniEngineUtils::RenameToUniqueActor(ExistingLandscape, LayerOutput->BakedLandscapeName + "_0");
					FHoudiniLandscapeRuntimeUtils::DestroyLandscape(ExistingLandscape);
				}

				BakeStats.NotifyPackageUpdated(1);

				FHoudiniEngineUtils::SafeRenameActor(LayerOutput->Landscape, LayerOutput->BakedLandscapeName);

				
				FString WorldOutlinerFolder =
					!LayerOutput->BakeOutlinerFolder.IsEmpty() ? LayerOutput->BakeOutlinerFolder : InFallbackWorldOutlinerFolder.ToString();


				if (!WorldOutlinerFolder.IsEmpty())
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
				
			}
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
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
{
	ULandscapeLayerInfoObject * BakedObject = BakeGeneric(
		LandscapeLayerInfoObject,
		PackageParams,
		LandscapeLayerInfoObject->GetName(),
		OutPackagesToSave,
		BakeStats);

	BakedObject->MarkPackageDirty();

	return BakedObject;

}


void FHoudiniLandscapeBake::BakeMaterials(
	const UHoudiniLandscapeTargetLayerOutput& Layer,
	const FHoudiniPackageParams& PackageParams,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
{
	UMaterialInterface * MaterialInstance = Layer.MaterialInstance;
	if (!IsValid(MaterialInstance))
		return;

	auto * BakedMaterialInstance = BakeGeneric(
		MaterialInstance,
		PackageParams,
		MaterialInstance->GetName(),
		OutPackagesToSave,
		BakeStats);

	//------------------------------------------------------------------------------------------------------------------------------
	// Replace the material instance.  If this is a parent landscape actor apply material change to every proxy, because
	// unreal will not do this for us.
	//------------------------------------------------------------------------------------------------------------------------------

	Layer.LandscapeProxy->LandscapeMaterial = BakedMaterialInstance;

	if (Layer.LandscapeProxy->IsA<ALandscape>())
	{
		ULandscapeInfo* Info = Layer.Landscape->GetLandscapeInfo();

#if ENGINE_MINOR_VERSION < 1
		TArray<ALandscapeStreamingProxy*> & Proxies = Info->Proxies;
		for (ALandscapeStreamingProxy* Proxy : Proxies)
		{
#else
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> & Proxies = Info->StreamingProxies;
		for (auto ProxyPtr : Proxies)
		{
			ALandscapeStreamingProxy* Proxy = ProxyPtr.Get();
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
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
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

	OutPackagesToSave.Add(Package);

	//------------------------------------------------------------------------------------------------------------------------------
	// Remove existing layer object
	//------------------------------------------------------------------------------------------------------------------------------

	UObjectType* ExistingObject = FindObject<UObjectType>(Package, *ObjectName);
	if (IsValid(ExistingObject))
	{
		BakeStats.NotifyObjectsReplaced(UObjectType::StaticClass()->GetName(), 1);
		BakeStats.NotifyPackageUpdated(1);
	}
	else
	{
		BakeStats.NotifyObjectsCreated(UObjectType::StaticClass()->GetName(), 1);
		BakeStats.NotifyPackageCreated(1);
	}

	//------------------------------------------------------------------------------------------------------------------------------
	// Replace the layer.
	//------------------------------------------------------------------------------------------------------------------------------

	UObjectType* DuplicatedObject = DuplicateObject<UObjectType>(CookedObject, Package, *ObjectName);
	Package->MarkPackageDirty();

	return DuplicatedObject;

}