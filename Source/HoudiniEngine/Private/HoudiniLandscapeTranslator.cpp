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

#include "HoudiniLandscapeTranslator.h"

#include "HoudiniMaterialTranslator.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniEngineString.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniPackageParams.h"
#include "HoudiniStringResolver.h"
#include "HoudiniInput.h"

#include "ObjectTools.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UnrealType.h"

#include "GameFramework/WorldSettings.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "LandscapeDataAccess.h"
#include "Factories/WorldFactory.h"
#include "Misc/Guid.h"
#include "Engine/LevelBounds.h"

#include "HAL/IConsoleManager.h"
#include "Engine/AssetManager.h"
#include "Misc/ScopedSlowTask.h"

#if WITH_EDITOR
	#include "EditorLevelUtils.h"
	#include "LandscapeEditorModule.h"
	#include "LandscapeFileFormatInterface.h"
#endif

static TAutoConsoleVariable<int32> CVarHoudiniEngineExportLandscapeTextures(
	TEXT("HoudiniEngine.ExportLandscapeTextures"),
	0,
	TEXT("If enabled, landscape layers and heightmap will be exported as textures in the temp directory when converting a Heightfield to a Landscape.\n")
	TEXT("0: Disabled\n")
	TEXT("1: Enabled\n")
);

typedef FHoudiniEngineUtils FHUtils;

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

HOUDINI_LANDSCAPE_DEFINE_LOG_CATEGORY();

bool
FHoudiniLandscapeTranslator::CreateLandscape(
	UHoudiniOutput* InOutput,
	TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedOutputs,
	TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	USceneComponent* SharedLandscapeActorParent,
	const FString& DefaultLandscapeActorPrefix,
	UWorld* InWorld, // Persistent / root world for the landscape
	const TMap<FString, float>& LayerMinimums,
	const TMap<FString, float>& LayerMaximums,
	FHoudiniLandscapeExtent& LandscapeExtent,
	FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
	FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
	FHoudiniPackageParams InPackageParams,
	TSet<FString>& ClearedLayers,
	TArray<UPackage*>& OutCreatedPackages
)
{
	// Do the absolute minimum in order to determine which output mode we're dealing with (Temp or Editable Layers).

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("CreateLandscape!"));
	
	if (!IsValid(InOutput))
		return false;

	//  Get the height map.
	const FHoudiniGeoPartObject* Heightfield = GetHoudiniHeightFieldFromOutput(InOutput, false, NAME_None);
	if (!Heightfield)
		return false;

	if (Heightfield->Type != EHoudiniPartType::Volume)
		return false;

	const HAPI_NodeId GeoId = Heightfield->GeoId;
	const HAPI_PartId PartId = Heightfield->PartId;

	// Check whether we're running in edit layer mode, or the usual temp mode

	TArray<int32> IntData;
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	
	// ---------------------------------------------
	// Attribute: unreal_landscape_output_mode
	// ---------------------------------------------
	IntData.Empty();
	int32 LandscapeOutputMode = 0;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_OUTPUT_MODE, 
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (IntData.Num() > 0)
		{
			LandscapeOutputMode = IntData[0];
		}
	}

	switch (LandscapeOutputMode)
	{
		case HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_MODIFY_LAYER:
		{
			return OutputLandscape_ModifyLayers(InOutput,
				CreatedUntrackedOutputs,
				InputLandscapesToUpdate,
				InAllInputLandscapes,
				SharedLandscapeActorParent,
				DefaultLandscapeActorPrefix,
				InWorld,
				LayerMinimums,
				LayerMaximums,
				LandscapeExtent,
				LandscapeTileSizeInfo,
				LandscapeReferenceLocation,
				InPackageParams,
				ClearedLayers,
				OutCreatedPackages);
		}
		break;
		case HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_GENERATE:
		default:
		{
			return OutputLandscape_Generate(InOutput,
				CreatedUntrackedOutputs,
				InputLandscapesToUpdate,
				InAllInputLandscapes,
				SharedLandscapeActorParent,
				DefaultLandscapeActorPrefix,
				InWorld,
				LayerMinimums,
				LayerMaximums,
				LandscapeExtent,
				LandscapeTileSizeInfo,
				LandscapeReferenceLocation,
				ClearedLayers,
				InPackageParams,
				OutCreatedPackages
				);
		}
		break;
	}
}

bool
FHoudiniLandscapeTranslator::OutputLandscape_Generate(
	UHoudiniOutput* InOutput,
	TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedOutputs,
	TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	USceneComponent* SharedLandscapeActorParent,
	const FString& DefaultLandscapeActorPrefix,
	UWorld* InWorld, // Persistent / root world for the landscape
	const TMap<FString, float>& LayerMinimums,
	const TMap<FString, float>& LayerMaximums,
	FHoudiniLandscapeExtent& LandscapeExtent,
	FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
	FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
	TSet<FString>& ClearedLayers,
	FHoudiniPackageParams InPackageParams,
	TArray<UPackage*>& OutCreatedPackages
)
{
	TArray<FString> EditLayerNames;
	const bool bHasEditLayers = GetEditLayersFromOutput(InOutput, EditLayerNames);

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::Output_Landscape_Generate] Generating landscape tile. Has edit layers?: %d"), bHasEditLayers);

	TArray<FName> AllLayerNames;
	for (const FString& LayerName : EditLayerNames)
	{
		AllLayerNames.Add(FName(LayerName));
	}

	if (!bHasEditLayers)
	{
		// Add a dummy edit layer to simply get us into the following for-loop.
		EditLayerNames.Add(FString());
	}
	
	FName AfterLayerName = NAME_None;

	TSet<ALandscapeProxy*> ActiveLandscapes;
	TArray<FHoudiniOutputObjectIdentifier> StaleOutputObjects;

	InOutput->GetOutputObjects().GenerateKeyArray(StaleOutputObjects);

	// Collect current edit layers and their respective landscapes so that we can detect and remove stale edit layers.
	TMap<FName, ALandscape*> StaleEditLayers;
	{
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
		for (auto& Entry : OutputObjects)
		{
			UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(Entry.Value.OutputObject);
			if (LandscapePtr)
			{
				ALandscapeProxy* Proxy = LandscapePtr->GetRawPtr();
				if (Proxy)
				{
					ALandscape* Landscape = Proxy->GetLandscapeActor();
					if (Landscape)
					{
						StaleEditLayers.Add(LandscapePtr->EditLayerName, Landscape);
					}
				}
			}
		}
	}
	
	for (const FString& LayerName : EditLayerNames)
	{
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::Output_Landscape_Generate] Generating tile for heightfield: %s"), *LayerName);
		// If edit layers are enabled for this volume, create each layer for this landscape tile
		FName LayerFName(LayerName);
		StaleEditLayers.Remove(LayerFName);
		
		OutputLandscape_GenerateTile(InOutput,
			StaleOutputObjects,
			CreatedUntrackedOutputs,
			InputLandscapesToUpdate,
			InAllInputLandscapes,
			SharedLandscapeActorParent,
			DefaultLandscapeActorPrefix,
			InWorld,
			LayerMinimums,
			LayerMaximums,
			LandscapeExtent,
			LandscapeTileSizeInfo,
			LandscapeReferenceLocation,
			InPackageParams,
			bHasEditLayers,
			LayerName,
			AfterLayerName,
			AllLayerNames,
			ClearedLayers,
			OutCreatedPackages,
			ActiveLandscapes);
		AfterLayerName = LayerFName;
	}

	// Clean up stale output objects
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for(FHoudiniOutputObjectIdentifier ObjectId : StaleOutputObjects)
	{
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_Generate] Processing stale output: %s"), *ObjectId.PartName);
		FHoudiniOutputObject* OutputObject = OutputObjects.Find(ObjectId);
		if (!OutputObject)
			continue;

		UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject->OutputObject);
		if (LandscapePtr)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_Generate] LandscapePtr: %s"), *LandscapePtr->GetFullName());
			// Cleanup stale landscape outputs
			ALandscapeProxy* LandscapeProxy = LandscapePtr->GetRawPtr();
			if (IsValid(LandscapeProxy))
			{
				if (!ActiveLandscapes.Contains(LandscapeProxy))
				{
					// This landscape actor is no longer in use. Trash it.
					LandscapeProxy->Destroy();
				}
			}
			LandscapePtr->SetSoftPtr(nullptr);
		}
		OutputObjects.Remove(ObjectId);
	}

	// Clean up stale layers
	for (auto& Entry : StaleEditLayers)
	{
		ALandscape* Landscape = Entry.Value;
		if (IsValid(Landscape) && Landscape->HasLayersContent())
		{
			const int32 LayerIndex = Landscape->GetLayerIndex(Entry.Key);
			Landscape->DeleteLayer(LayerIndex);
		}
	}

	return true;
}

bool
FHoudiniLandscapeTranslator::OutputLandscape_GenerateTile(
	UHoudiniOutput* InOutput,
	TArray<FHoudiniOutputObjectIdentifier> &StaleOutputObjects,
	TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedOutputs,
	TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	USceneComponent* SharedLandscapeActorParent,
	const FString& DefaultLandscapeActorPrefix,
	UWorld* InWorld, // Persistent / root world for the landscape
	const TMap<FString, float>& LayerMinimums,
	const TMap<FString, float>& LayerMaximums,
	FHoudiniLandscapeExtent& LandscapeExtent,
	FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
	FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
	FHoudiniPackageParams InPackageParams,
	bool bHasEditLayers,
	const FString& InEditLayerName,
	const FName& InAfterLayerName,
	const TArray<FName>& AllLayerNames,
	TSet<FString>& ClearedLayers,
	TArray<UPackage*>& OutCreatedPackages,
	TSet<ALandscapeProxy*>& OutActiveLandscapes
)
{
	FName InEditLayerFName = FName(InEditLayerName);
	check(LayerMinimums.Contains(TEXT("height")));
	check(LayerMaximums.Contains(TEXT("height")));

	float fGlobalMin = LayerMinimums.FindChecked(TEXT("height"));
	float fGlobalMax = LayerMaximums.FindChecked(TEXT("height"));

	if (!IsValid(InOutput))
		return false;

	//  Get the height map.
	const FHoudiniGeoPartObject* Heightfield = GetHoudiniHeightFieldFromOutput(InOutput, bHasEditLayers, InEditLayerFName);
	if (!Heightfield)
		return false;

	if (Heightfield->Type != EHoudiniPartType::Volume)
		return false;

	const HAPI_NodeId GeoId = Heightfield->GeoId;
	const HAPI_PartId PartId = Heightfield->PartId;

	// Construct the identifier of the Heightfield geo part.
	FHoudiniOutputObjectIdentifier HeightfieldIdentifier(Heightfield->ObjectId, GeoId, PartId, "Heightfield");
	HeightfieldIdentifier.PartName = Heightfield->PartName;

	FString NodeNameSuffix = GetActorNameSuffix(InPackageParams.PackageMode);
	bool bAddLandscapeNameSuffix = true;
	bool bAddLandscapeTileNameSuffix = true;

	const UHoudiniAssetComponent* HoudiniAssetComponent = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput); 

	TArray<int> IntData;
	TArray<FString> StrData;
	// Output attributes will be stored on the Output object and will be used again during baking to determine
	// where content should be baked to and what they should be named, etc.
	// At the end of this function, the output attributes and tokens will be copied to the output object.
	TMap<FString,FString> OutputAttributes;
	TMap<FString,FString> OutputTokens;

	bool bHasTile = Heightfield->VolumeTileIndex >= 0;

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Volume Tile Index: %d"), Heightfield->VolumeTileIndex);
	
	// ---------------------------------------------
	// Attribute: unreal_landscape_tile_actor_type, unreal_landscape_streaming_proxy (v1)
	// ---------------------------------------------
	// Determine the actor type for the tile
	bool bCreateLandscapeStreamingProxy = false;
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	LandscapeActorType TileActorType = LandscapeActorType::LandscapeActor;
	IntData.Empty();
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_ACTOR_TYPE, 
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (IntData.Num() > 0)
		{
			TileActorType = static_cast<LandscapeActorType>(IntData[0]);
		}
	}
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_STREAMING_PROXY, 
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (IntData.Num() > 0 && IntData[0] != 0)
			TileActorType = LandscapeActorType::LandscapeStreamingProxy;
	}

	OutputAttributes.Add(HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_ACTOR_TYPE, FString::FromInt(static_cast<int32>(TileActorType)));

	// ---------------------------------------------
	// Attribute: unreal_landscape_actor_name
	// ---------------------------------------------
	// Retrieve the name of the main Landscape actor to look for
	FString SharedLandscapeActorName = DefaultLandscapeActorPrefix + "SharedLandscape"; // If this is an empty string, don't affirm a root landscape actor?
	StrData.Empty();
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME, 
		AttributeInfo, StrData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (StrData.Num() > 0 && !StrData[0].IsEmpty())
			SharedLandscapeActorName = StrData[0];
	}

	OutputAttributes.Add(HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME, SharedLandscapeActorName);

	// ---------------------------------------------
	// Attribute: unreal_level_path
	// --------------------------------------------- 
	// FString LevelPath = bHasTile ? "{world}/Landscape/Tile{tile}" : "{world}/Landscape";
	FString LevelPath;
	TArray<FString> LevelPaths;
	if (FHoudiniEngineUtils::GetLevelPathAttribute(GeoId, PartId, LevelPaths, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (LevelPaths.Num() > 0 && !LevelPaths[0].IsEmpty())
			LevelPath = LevelPaths[0];
	}
	if (!LevelPath.IsEmpty())
		OutputAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPath);
	
	// ---------------------------------------------
	// Attribute: unreal_output_name
	// ---------------------------------------------
	FString LandscapeTileActorName = bHasTile ? "LandscapeTile{tile}" : "Landscape";
	TArray<FString> AllOutputNames;
	if (FHoudiniEngineUtils::GetOutputNameAttribute(GeoId, PartId, AllOutputNames, 0, 1))
	{
		if (AllOutputNames.Num() > 0 && !AllOutputNames[0].IsEmpty())
		{
			LandscapeTileActorName = AllOutputNames[0];
		}
	}
	OutputAttributes.Add(FString(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2), LandscapeTileActorName);

	// ---------------------------------------------
	// Attribute: unreal_bake_name
	// ---------------------------------------------
	FString LandscapeTileBakeName = bHasTile ? "LandscapeTile{tile}" : "Landscape";
	TArray<FString> AllBakeNames;
	if (FHoudiniEngineUtils::GetBakeNameAttribute(GeoId, PartId, AllBakeNames, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (AllBakeNames.Num() > 0 && !AllBakeNames[0].IsEmpty())
		{
			LandscapeTileBakeName = AllBakeNames[0];
		}
	}
	OutputAttributes.Add(FString(HAPI_UNREAL_ATTRIB_BAKE_NAME), LandscapeTileBakeName);

	// ---------------------------------------------
	// Attribute: unreal_landscape_editlayer_after
	// ---------------------------------------------
	StrData.Empty();
	FName AfterLayerName = InAfterLayerName;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(GeoId, PartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER, AttributeInfo, StrData, 1))
	{
		if (StrData.Num() > 0)
		{
			AfterLayerName = FName(StrData[0]);
		}
	}

	// ---------------------------------------------
	// Attribute: unreal_landscape_editlayer_type
	// ---------------------------------------------
	int32 EditLayerType = HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_BASE;
	IntData.Empty();
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TYPE, AttributeInfo, IntData, 1))
	{
		if (IntData.Num() > 0)
		{
			EditLayerType = IntData[0];
		}
	}


	// By default, if we're in EditLayer mode, always 'check' this option
	// otherwise the paint layers will be additively blended which is
	// typically not expected.
	// The user should be able to override this behaviour by setting
	// the HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS attribute on the paint layer heightfields in Houdini. 
	bool bLayerNoWeightBlend = bHasEditLayers ? true : false;

	// ---------------------------------------------
	// Attribute: unreal_bake_folder
	// ---------------------------------------------
	TArray<FString> AllBakeFolders;
	if (FHoudiniEngineUtils::GetBakeFolderAttribute(GeoId, AllBakeFolders, PartId, 0, 1))
	{
		FString BakeFolder;
		if (AllBakeFolders.Num() > 0 && !AllBakeFolders[0].IsEmpty())
			BakeFolder = AllBakeFolders[0];
		OutputAttributes.Add(FString(HAPI_UNREAL_ATTRIB_BAKE_FOLDER), BakeFolder);
	}

	// ---------------------------------------------
	// Attribute: unreal_temp_folder
	// ---------------------------------------------
	{
		FString TempFolder;
		if (FHoudiniEngineUtils::GetTempFolderAttribute(GeoId, TempFolder, PartId, 0))
		{
			OutputAttributes.Add(FString(HAPI_UNREAL_ATTRIB_TEMP_FOLDER), TempFolder);
		}
	}

	// Streaming proxy actors/tiles requires a "main" landscape actor
	// that contains the shared landscape state. 
	bool bRequiresSharedLandscape = false;
	if (TileActorType == LandscapeActorType::LandscapeStreamingProxy)
		bRequiresSharedLandscape = true;

	// ----------------------------------
	// Inject landscape specific tokens
	// ----------------------------------
	if (bHasTile)
	{
		const FString TileValue = FString::FromInt(Heightfield->VolumeTileIndex);
		// Tile value needs to go into Output arguments to be available during the bake.
		OutputTokens.Add(TEXT("tile"), TileValue);
	}

	// ----------------------------------
	// Expand string arguments for various landscape naming aspects.
	// ----------------------------------

	// Update package params and resolver attributes and tokens before we start resolving attributes.
	FHoudiniPackageParams PackageParams;
	FHoudiniAttributeResolver Resolver;
	FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
		InPackageParams,
		InWorld,
		HoudiniAssetComponent,
		OutputAttributes,
		OutputTokens,
		PackageParams,
		Resolver);

	SharedLandscapeActorName = Resolver.ResolveAttribute(HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME, SharedLandscapeActorName);
	SharedLandscapeActorName += NodeNameSuffix;

	LandscapeTileActorName = Resolver.ResolveAttribute(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, LandscapeTileActorName);
	LandscapeTileActorName += NodeNameSuffix;
	
	LevelPath = Resolver.ResolveAttribute(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPath);

	FString TileName = LandscapeTileActorName;

	// Note that relative level paths are always interpreted as relative to the default output directory (temp / bake). 
	// FString TilePackagePath = FPaths::Combine(DefaultOutputPath, LevelPath);
	FString TilePackagePath = Resolver.ResolveFullLevelPath();

	// This crashes UE if the package name does not resolve
	//FString TileMapFileName = FPackageName::LongPackageNameToFilename(TilePackagePath, FPackageName::GetMapPackageExtension());

	FText NotValidReason;
	bool bIsValidLongName = FPackageName::IsValidLongPackageName(TilePackagePath, false, &NotValidReason);
	if (!bIsValidLongName)
	{
		// Try a more naive approach
		TilePackagePath = FPaths::Combine(PackageParams.BakeFolder, LevelPath);
		bIsValidLongName = FPackageName::IsValidLongPackageName(TilePackagePath, false, &NotValidReason);
	}

	if (!bIsValidLongName)
	{
		HOUDINI_LOG_ERROR(TEXT("[CreateOrUpdateLandscapeOutputHoudini] TilePackagePath is not a valid long name. Reason: %s"), *(NotValidReason.ToString()));
		return false;
	}

	FString TileMapFileName;
	if (!FPackageName::TryConvertLongPackageNameToFilename(TilePackagePath, TileMapFileName, FPackageName::GetMapPackageExtension()))
	{
		// Rather stop here than crash!
		HOUDINI_LOG_ERROR(TEXT("[CreateOrUpdateLandscapeOutputHoudini] Failed to resolve the TilePackagePath: %s"), *(TilePackagePath));
		return false;
	}

	// Find the package for both the world and the tile.
	// The world should contain the main landscape actor while 
	// the tile will contain a Landscape, LandscapeProxy or LandscapeStreamingProxy depending on user settings.

	bool bTileisStreamingProxy = (TileActorType == LandscapeActorType::LandscapeStreamingProxy);
	UWorld* TileWorld = nullptr; // World from which to spawn tile actor
	ULevel* TileLevel = nullptr; // Level in which to spawn tile actor
	ALandscapeProxy* TileActor = nullptr; // Spawned tile actor.

	// ----------------------------------
	// Update package parameters for this tile
	// ----------------------------------

	// NOTE: we don't manually inject a tile number in the object name. This should
	// already be encoded in the TileName string.
	FHoudiniPackageParams TilePackageParams = PackageParams;
	TilePackageParams.ObjectName = TileName;

	FHoudiniPackageParams LayerPackageParams = PackageParams;
	if (bRequiresSharedLandscape)
	{
		// Note that layers are shared amongst all the tiles for a given landscape.
		LayerPackageParams.ObjectName = SharedLandscapeActorName;
	}
	else
	{
		// This landscape tile is a standalone landscape and should have its own material layers.
		LayerPackageParams.ObjectName = TileName;
	}

	// See if the current heightfield has an unreal_material or unreal_hole_material assigned to it
	UMaterialInterface* LandscapeMaterial = nullptr;
	UMaterialInterface* LandscapeHoleMaterial = nullptr;
	UPhysicalMaterial* LandscapePhysicalMaterial = nullptr;
	FHoudiniLandscapeTranslator::GetLandscapeMaterials(*Heightfield, PackageParams, LandscapeMaterial, LandscapeHoleMaterial, LandscapePhysicalMaterial);

	// Extract the float data from the Heightfield.
	const FHoudiniVolumeInfo &VolumeInfo = Heightfield->VolumeInfo;
	TArray<float> FloatValues;
	float FloatMin, FloatMax;
	if (!GetHoudiniHeightfieldFloatData(Heightfield, FloatValues, FloatMin, FloatMax))
		return false;

	// Heightfield conversions should always use the global float min/max
	// since they need to be calculated externally, potentially across multiple tiles.
	FloatMin = fGlobalMin;
	FloatMax = fGlobalMax;

	// Get the Unreal landscape size 
	const int32 HoudiniHeightfieldXSize = VolumeInfo.YLength;
	const int32 HoudiniHeightfieldYSize = VolumeInfo.XLength;

	if (!LandscapeTileSizeInfo.bIsCached)
	{
		// Calculate a landscape size info from this heightfield to be
		// used by subsequent tiles on the same landscape
		if (FHoudiniLandscapeTranslator::CalcLandscapeSizeFromHeightfieldSize(
			HoudiniHeightfieldXSize,
			HoudiniHeightfieldYSize,
			LandscapeTileSizeInfo.UnrealSizeX,
			LandscapeTileSizeInfo.UnrealSizeY,
			LandscapeTileSizeInfo.NumSectionsPerComponent,
			LandscapeTileSizeInfo.NumQuadsPerSection))
		{
			LandscapeTileSizeInfo.bIsCached = true;
		}
		else
		{
			return false;
		}
	}
	
	const int32 UnrealTileSizeX = LandscapeTileSizeInfo.UnrealSizeX;
	const int32 UnrealTileSizeY = LandscapeTileSizeInfo.UnrealSizeY;
	const int32 NumSectionPerLandscapeComponent = LandscapeTileSizeInfo.NumSectionsPerComponent;
	const int32 NumQuadsPerLandscapeSection = LandscapeTileSizeInfo.NumQuadsPerSection;

	// ----------------------------------------------------
	// Export of layer textures
	// ----------------------------------------------------
	// Export textures, if enabled. Mostly used for debugging at the moment.
	bool bExportTexture = CVarHoudiniEngineExportLandscapeTextures.GetValueOnAnyThread() == 1 ? true : false;
	if (bExportTexture)
	{
		// Export raw height data to texture
		FString TextureName = TilePackageParams.ObjectName + TEXT("_height_raw");
		FHoudiniLandscapeTranslator::CreateUnrealTexture(
			TilePackageParams,
			TextureName,
			HoudiniHeightfieldXSize,
			HoudiniHeightfieldYSize,
			FloatValues,
			FloatMin,
			FloatMax);
	}

	// Look for all the layers/masks corresponding to the current heightfield.
	TArray< const FHoudiniGeoPartObject* > FoundLayers;
	FHoudiniLandscapeTranslator::GetHeightfieldsLayersFromOutput(InOutput, *Heightfield, bHasEditLayers, InEditLayerFName, FoundLayers);

	// Get the updated layers.
	TArray<FLandscapeImportLayerInfo> LayerInfos;

	// If we are operating in EditLayer mode, then any layers we create or manage should
	// have their bNoWeightBlend set to true otherwise all the paint layers will act as additive
	// which, most of the time, is not what we expect.
	bool bDefaultNoWeightBlend = bHasEditLayers ? true : false;

	TMap<FName, const FHoudiniGeoPartObject*> LayerObjectMapping;
	if (!CreateOrUpdateLandscapeLayerData(FoundLayers, *Heightfield, UnrealTileSizeX, UnrealTileSizeY, 
		LayerMinimums, LayerMaximums, LayerInfos, false, bDefaultNoWeightBlend,
		TilePackageParams,
		LayerPackageParams,
		OutCreatedPackages,
		LayerObjectMapping))
		return false;

	// Convert Houdini's heightfield data to Unreal's landscape data
	TArray<uint16> IntHeightData;
	FTransform TileTransform;
	if (!FHoudiniLandscapeTranslator::ConvertHeightfieldDataToLandscapeData(
		FloatValues, VolumeInfo,
		UnrealTileSizeX, UnrealTileSizeY,
		FloatMin, FloatMax,
		IntHeightData, TileTransform,
		false,
		false,
		100.f,
		EditLayerType == HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_ADDITIVE))
		return false;

	// ----------------------------------------------------
	// Property changes that we want to track
	// ----------------------------------------------------

	bool bModifiedLandscapeActor = false;
	bool bModifiedSharedLandscapeActor = false;
	bool bSharedLandscapeMaterialChanged = false;
	bool bSharedLandscapeHoleMaterialChanged = false;
	bool bSharedPhysicalMaterialChanged = false;
	bool bTileLandscapeMaterialChanged = false;
	bool bTileLandscapeHoleMaterialChanged = false;
	bool bTilePhysicalMaterialChanged = false;
	bool bCreatedMap = false;
	bool bCreatedTileActor = false;
	bool bHeightLayerDataChanged = false;
	bool bCustomLayerDataChanged = false;

	// ----------------------------------------------------
	// Calculate Tile location and landscape offset
	// ----------------------------------------------------
	FTransform LandscapeTransform;
	FIntPoint TileLoc;
	
	// Calculate the tile location (in quad space) as well as the corrected TileTransform which will compensate
	// for any landscape shifts due to section base alignment offsets.
	CalculateTileLocation(NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection, TileTransform, LandscapeReferenceLocation, LandscapeTransform, TileLoc);

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Tile Transform: %s"), *TileTransform.ToString());
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Landscape Transform: %s"), *LandscapeTransform.ToString());

	// Determine the level of the owning HAC (if we have a valid owning HAC), and use that as the Tile and shared
	// landscape's world and level
	UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput);
	if (IsValid(HAC))
	{
		TileWorld = HAC->GetWorld();
		TileLevel = HAC->GetComponentLevel();
	}
	else
	{
		// If we don't have a valid HAC fallback to the persistent level of InWorld
		TileWorld = InWorld;
		TileLevel = InWorld->PersistentLevel;
	}

	check(TileWorld);
	check(TileLevel);

	// ----------------------------------------------------
	// Find or create *shared* landscape
	// ----------------------------------------------------
	
	ALandscape* SharedLandscapeActor = nullptr;
	bool bCreatedSharedLandscape = false;
	
	if (bRequiresSharedLandscape)
	{
		// Streaming proxy tiles always require a "shared landscape" that contains the
		// various landscape properties to be shared amongst all the tiles.
		AActor* FoundActor = nullptr;
		SharedLandscapeActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscape>(TileWorld, SharedLandscapeActorName, FoundActor);

		bool bIsValidSharedLandscape = IsValid(SharedLandscapeActor);

		if (bIsValidSharedLandscape)
		{
			// We have a target landscape. Check whether it is compatible with the Houdini volume.
			ULandscapeInfo* LandscapeInfo = SharedLandscapeActor->GetLandscapeInfo();
			
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Found existing landscape: %s"), *(SharedLandscapeActor->GetPathName()));
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Found existing landscape with num proxies: %d"), LandscapeInfo->Proxies.Num());
			
			if (!LandscapeExtent.bIsCached)
			{
				LandscapeInfo->FixupProxiesTransform();
				// Cache the landscape extents. Note that GetLandscapeExtent() will only take into account the currently loaded landscape tiles. 
				PopulateLandscapeExtents(LandscapeExtent, LandscapeInfo);
			}
			
			bool bIsCompatible = IsLandscapeInfoCompatible(
				LandscapeInfo,
				NumSectionPerLandscapeComponent,
				NumQuadsPerLandscapeSection);
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Checking landscape compatibility ..."));
			bIsCompatible = bIsCompatible && IsLandscapeTypeCompatible(SharedLandscapeActor, LandscapeActorType::LandscapeActor);
			if (!bIsCompatible)
			{
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Shared landscape is incompatible. Cannot reuse."));
				// We can't resize the landscape in-place. We have to create a new one.
				DestroyLandscape(SharedLandscapeActor);
				SharedLandscapeActor = nullptr;
				bIsValidSharedLandscape = false;
			}
			else
			{
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Existing shared landscape is compatible."));
			}
		}

		if (!bIsValidSharedLandscape)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Create new shared landscape..."));
			// Create and configure the main landscape actor.
			// We need to create the landscape now and assign it a new GUID so we can create the LayerInfos
			FActorSpawnParameters SpawnParameters;
			if (IsValid(TileLevel))
				SpawnParameters.OverrideLevel = TileLevel;
			SharedLandscapeActor = TileWorld->SpawnActor<ALandscape>(SpawnParameters);
			if (SharedLandscapeActor)
			{
				CreatedUntrackedOutputs.Add( SharedLandscapeActor );

				// NOTE that shared landscape is always located at the origin, but not the tile actors. The
				// tiles are properly transformed.

				SharedLandscapeActor->bCanHaveLayersContent = bHasEditLayers;
				SharedLandscapeActor->ComponentSizeQuads = NumQuadsPerLandscapeSection*NumSectionPerLandscapeComponent;
				SharedLandscapeActor->NumSubsections = NumSectionPerLandscapeComponent;
				SharedLandscapeActor->SubsectionSizeQuads = NumQuadsPerLandscapeSection;
				SharedLandscapeActor->SetLandscapeGuid( FGuid::NewGuid() );
				SharedLandscapeActor->bCastStaticShadow = false;
				for (const auto& ImportLayerInfo : LayerInfos)
				{
					SharedLandscapeActor->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(ImportLayerInfo.LayerInfo));
				}
				SharedLandscapeActor->CreateLandscapeInfo();
				bCreatedSharedLandscape = true;
				
				// NOTE: It is important to set Landscape materials BEFORE blitting layer data. For example, setting
				// data in the visibility layer (on tiles) will have no effect until Landscape materials have been applied / processed.
				SharedLandscapeActor->LandscapeMaterial = LandscapeMaterial;
				SharedLandscapeActor->LandscapeHoleMaterial = LandscapeHoleMaterial;
				DoPreEditChangeProperty(SharedLandscapeActor, "LandscapeMaterial");
				
				// Ensure the landscape actor name and label matches `LandscapeActorName`.
				FHoudiniEngineUtils::SafeRenameActor(SharedLandscapeActor, SharedLandscapeActorName);

				SharedLandscapeActor->MarkPackageDirty();
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Could not create main landscape actor (%s) in world (%s)"), *(SharedLandscapeActorName), *(TileWorld->GetPathName()) );
				return false;
			}
		}
		// else -- Reusing shared landscape
	}

	if (SharedLandscapeActor)
	{
		SharedLandscapeActor->bCanHaveLayersContent = bHasEditLayers;
		
		// Ensure the existing landscape actor transform is correct.
		SharedLandscapeActor->SetActorRelativeTransform(LandscapeTransform);

		bSharedLandscapeMaterialChanged = (SharedLandscapeActor->LandscapeMaterial != LandscapeMaterial);
		bSharedLandscapeHoleMaterialChanged = SharedLandscapeActor->LandscapeHoleMaterial != LandscapeHoleMaterial;
		
		if (bSharedLandscapeMaterialChanged || bSharedLandscapeHoleMaterialChanged)
		{
			DoPreEditChangeProperty(SharedLandscapeActor, "LandscapeMaterial");
		}
		
		if (bSharedLandscapeMaterialChanged)
		{
			SharedLandscapeActor->LandscapeMaterial = LandscapeMaterial;
			
		}
		if (bSharedLandscapeHoleMaterialChanged)
		{
			SharedLandscapeActor->LandscapeHoleMaterial = LandscapeHoleMaterial;
			
		}

		bSharedPhysicalMaterialChanged = LandscapePhysicalMaterial != nullptr ? (SharedLandscapeActor->DefaultPhysMaterial != LandscapePhysicalMaterial) : false;
		if (bSharedPhysicalMaterialChanged)
		{
			DoPreEditChangeProperty(SharedLandscapeActor, "DefaultPhysMaterial");
			SharedLandscapeActor->DefaultPhysMaterial = LandscapePhysicalMaterial;
			SharedLandscapeActor->ChangedPhysMaterial();
		}

		if (bSharedLandscapeMaterialChanged || bSharedLandscapeHoleMaterialChanged)
		{
			check(SharedLandscapeActor);
			DoPostEditChangeProperty(SharedLandscapeActor, "LandscapeMaterial");
		}
	}

	// ----------------------------------------------------
	// Find Landscape actor / tile
	// ----------------------------------------------------
	
	// Find an actor with the given name. The TileWorld and TileLevel returned should be
	// used to spawn the new actor, if the actor itself could not be found.
	//bool bCreatedPackage = false;	
	// TileActor = FindExistingLandscapeActor<PackageModeT>(
	// 	InWorld, InOutput, ValidLandscapes,
	// 	UnrealLandscapeSizeX, UnrealLandscapeSizeY, LandscapeTileActorName,
	// 	LevelPath, TileWorld, TileLevel, bCreatedPackage);

	// Currently the Temp Cook mode is not concerned with creating packages. This will, at the time of writing,
	// exclusively be dealt with during Bake mode so don't bother with searching / creating other packages.

	AActor* FoundActor = nullptr;
	if (PackageParams.PackageMode == EPackageMode::Bake)
	{
		// When baking, See if we can find any landscape / proxy actors for this tile in the TileLevel.
		// If we find any actors that match the name but not the type, or the actors are pending kill, then
		// rename them so that we can spawn new actors.
		switch (TileActorType)
		{
			case LandscapeActorType::LandscapeActor:
				TileActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscape>(TileWorld, LandscapeTileActorName, FoundActor);
				break;
			case LandscapeActorType::LandscapeStreamingProxy:
				TileActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscapeStreamingProxy>(TileWorld, LandscapeTileActorName, FoundActor);
				break;
			default:
				TileActor = nullptr;
		}
	}
	else
	{
		// In temp mode, only consider our previous output landscapes,
		// or our input landscapes that have the "update input landscape" option enabled
		ALandscapeProxy* FoundLandscapeProxy = nullptr;

		// Try to see if we have an input landscape that matches the size of the current HGPO	
		for (int nIdx = 0; nIdx < InputLandscapesToUpdate.Num(); nIdx++)
		{
			ALandscapeProxy* CurrentInputLandscape = InputLandscapesToUpdate[nIdx];
			if (!CurrentInputLandscape)
				continue;

			if (SharedLandscapeActor && CurrentInputLandscape->GetLandscapeActor() != SharedLandscapeActor)
				// This tile actor no longer associated with the current shared landscape
				continue;

			ULandscapeInfo* CurrentInfo = CurrentInputLandscape->GetLandscapeInfo();
			if (!CurrentInfo)
				continue;

			int32 InputMinX = 0;
			int32 InputMinY = 0;
			int32 InputMaxX = 0;
			int32 InputMaxY = 0;
			if (!LandscapeExtent.bIsCached)
			{
				PopulateLandscapeExtents(LandscapeExtent, CurrentInfo);
			}

			if (!LandscapeExtent.bIsCached)
			{
				HOUDINI_LOG_WARNING(TEXT("Warning: Could not determine landscape extents. Cannot re-use input landscape actor."));
				continue;
			}
			
			InputMinX = LandscapeExtent.MinY;
			InputMinY = LandscapeExtent.MinY;
			InputMaxX = LandscapeExtent.MaxX;
			InputMaxY = LandscapeExtent.MaxY;

			// If the full size matches, we'll update that input landscape
			bool SizeMatch = false;
			if ((InputMaxX - InputMinX + 1) == UnrealTileSizeX && (InputMaxY - InputMinY + 1) == UnrealTileSizeY)
				SizeMatch = true;

			// HF and landscape don't match, try another one
			if (!SizeMatch)
				continue;

			// Replace FoundLandscape by that input landscape
			FoundLandscapeProxy = CurrentInputLandscape;

			// We've found a valid input landscape, remove it from the input array so we don't try to update it multiple times
			InputLandscapesToUpdate.RemoveAt(nIdx);
			break;
		}

		if (!FoundLandscapeProxy)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Could not find input landscape to update. Searching output objects..."));
			
			// Try to see if we can reuse one of our previous output landscape.
			// Keep track of the previous cook's landscapes
			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OldOutputObjects = InOutput->GetOutputObjects();
			for (auto& CurrentLandscape : OldOutputObjects)
			{
				UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(CurrentLandscape.Value.OutputObject);
				if (!LandscapePtr)
					continue;

				FoundLandscapeProxy = LandscapePtr->GetRawPtr();
				if (!FoundLandscapeProxy)
				{
					// We may need to manually load the object
					//OldLandscapeProxy = LandscapePtr->GetSoftPtr().LoadSynchronous();
					FoundLandscapeProxy = LandscapePtr->LandscapeSoftPtr.LoadSynchronous();
				}

				if (!IsValid(FoundLandscapeProxy))
					continue;

				// We need to make sure that this landscape is not one of our input landscape
				// This would happen if we were previously updating it, but just turned the option off
				// In that case, the landscape would be in both our inputs and outputs,
				// but with the "Update Input Data" option off
				if (InAllInputLandscapes.Contains(FoundLandscapeProxy))
				{
					FoundLandscapeProxy = nullptr;
					continue;
				}

				if (SharedLandscapeActor && FoundLandscapeProxy->GetLandscapeActor() != SharedLandscapeActor)
				{
					// This landscape proxy is no longer part of the shared landscape.
					HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Output landscape proxy is no longer part of the landscape. Skipping"));
					FoundLandscapeProxy = nullptr;
					continue;
				}

				// If we found a possible candidate, make sure that its size matches ours as we can only update a landscape tile of the same size
				if (!IsLandscapeTileCompatible(
					FoundLandscapeProxy, 
					UnrealTileSizeX-1, 
					UnrealTileSizeY-1, 
					NumSectionPerLandscapeComponent, 
					NumQuadsPerLandscapeSection))
				{
					FoundLandscapeProxy = nullptr;
					continue;
				}

				if (SharedLandscapeActor)
				{
					if (FoundLandscapeProxy->GetLandscapeGuid() != SharedLandscapeActor->GetLandscapeGuid())
					{
						FoundLandscapeProxy = nullptr;
						continue;
					}
				}

				// TODO: we probably need to do some more checks with tiled landscapes as well?

				// We found a valid Candidate!
				if (FoundLandscapeProxy)
				{
					break;
				}
			}
		}

		if (IsValid(FoundLandscapeProxy))
		{
			TileActor = FoundLandscapeProxy;
			if (TileActor->GetName() != LandscapeTileActorName)
			{
				// Ensure the TileActor is named correctly
				FHoudiniEngineUtils::SafeRenameActor(TileActor, LandscapeTileActorName);
			}
		}
	}

	// NOTE: We don't need to delete old landscape tiles (FoundActor != TileActor) here. That is an old
	// output that should get cleaned up automatically.

	if (IsValid(TileActor))
	{
		// ----------------------------------------------------
		// Check landscape compatibility
		// ----------------------------------------------------

		bool bIsCompatible = IsLandscapeTileCompatible(
			TileActor,
			UnrealTileSizeX-1,
			UnrealTileSizeY-1,
			NumSectionPerLandscapeComponent,
			NumQuadsPerLandscapeSection);

		bIsCompatible = bIsCompatible && IsLandscapeTypeCompatible(TileActor, TileActorType);

		if (!bIsCompatible)
		{
			// Can't reuse this tile actor since the landscape dimensions doesn't match or the actor type has changed.
			if (TileActor->IsA<ALandscapeStreamingProxy>())
			{
				// This landscape tile needs to be unregistered from the landscape info.
				ULandscapeInfo* LandscapeInfo = TileActor->GetLandscapeInfo();
				if (IsValid(LandscapeInfo))
				{
					LandscapeInfo->UnregisterActor(TileActor);
				}
			}
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Incompatible tile actor. Destroying: %s"), *(TileActor->GetPathName()));
			TileActor->Destroy();
			TileActor = nullptr;
		}
		else
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Landscape tile is compatible: %s"), *(TileActor->GetPathName()));
		}
	}

	// ----------------------------------------------------
	// Create or update landscape / tile. 
	// ----------------------------------------------------
	// Note that a single heightfield generated in Houdini can be treated
	// as either a landscape tile (LandscapeStreamingProxy) or a standalone
	// landscape (ALandscape). This determination is made purely from user specified
	// attributes. No "clever logic" in here, please!

	ALandscapeStreamingProxy* CachedStreamingProxyActor = nullptr;
	ALandscape* CachedLandscapeActor = nullptr;
	ULandscapeInfo *LandscapeInfo;

#if defined(HOUDINI_ENGINE_DEBUG_LANDSCAPE)
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Tile Loc: %d, %d"), TileLoc.X, TileLoc.Y);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Tile Size: %d, %d"), UnrealTileSizeX, UnrealTileSizeY);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Tile Num Quads/Section: %d"), NumQuadsPerLandscapeSection);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Tile Num Sections/Component: %d"), NumSectionPerLandscapeComponent);
#endif

	// ----------------------------------------------------
	// Update tile materials
	// ----------------------------------------------------
	auto UpdateTileMaterialsFn = [&] ()
	{
		bTileLandscapeMaterialChanged = (TileActor->LandscapeMaterial != LandscapeMaterial);
		bTileLandscapeHoleMaterialChanged = (TileActor->LandscapeHoleMaterial != LandscapeHoleMaterial);
		
		if (bTileLandscapeMaterialChanged || bTileLandscapeHoleMaterialChanged)
			DoPreEditChangeProperty(TileActor, "LandscapeMaterial");
		
		if (bTileLandscapeMaterialChanged)
			TileActor->LandscapeMaterial = LandscapeMaterial;

		if (bTileLandscapeHoleMaterialChanged)
			TileActor->LandscapeHoleMaterial = LandscapeHoleMaterial;

		bTilePhysicalMaterialChanged = LandscapePhysicalMaterial != nullptr ? TileActor->DefaultPhysMaterial != LandscapePhysicalMaterial : false;
		if (bTilePhysicalMaterialChanged)
		{
			DoPreEditChangeProperty(TileActor, "DefaultPhysMaterial");
			TileActor->DefaultPhysMaterial = LandscapePhysicalMaterial;
			//TileActor->ChangedPhysMaterial();
		}

		if (bTileLandscapeMaterialChanged || bTileLandscapeHoleMaterialChanged)
		{
			DoPostEditChangeProperty(TileActor, "LandscapeMaterial");
		}
	};

	if (!TileActor)
	{
		// Create a new Landscape tile
		
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Creating new tile actor: %s"), *(LandscapeTileActorName));
		TileActor = FHoudiniLandscapeTranslator::CreateLandscapeTileInWorld(
			IntHeightData, LayerInfos, TileTransform, TileLoc,
			UnrealTileSizeX, UnrealTileSizeY,
			NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
			LandscapeMaterial, LandscapeHoleMaterial, LandscapePhysicalMaterial,
			LandscapeTileActorName,
			TileActorType,
			SharedLandscapeActor,
			TileWorld,
			TileLevel,
			PackageParams,
			bHasEditLayers,
			InEditLayerFName,
			AfterLayerName);

		if (!TileActor || !TileActor->IsValidLowLevel())
			return false;

		TileActor->GetLandscapeActor()->bCanHaveLayersContent = bHasEditLayers;
		LandscapeInfo = TileActor->GetLandscapeInfo();

		UpdateTileMaterialsFn();

		TMap<FName, int32> ExistingLayers;
		UpdateLandscapeMaterialLayers(
			TileActor->GetLandscapeActor(),
			LayerInfos,
			ExistingLayers,
			bHasEditLayers,
			InEditLayerFName);

		bCreatedTileActor = true;
		bHeightLayerDataChanged = true;
		bCustomLayerDataChanged = true;
	}
	else
	{
		// Re-use existing tile actor
		ALandscape* TargetLandscape = TileActor->GetLandscapeActor();
		check(TargetLandscape);
		
		LandscapeInfo = TileActor->GetLandscapeInfo();
		TargetLandscape->bCanHaveLayersContent = bHasEditLayers;

		// Always update the transform, even if the HGPO transform hasn't changed,
		// If we change the number of tiles, or switch from outputting single tile to multiple,
		// then its fairly likely that the unreal transform has changed even if the
		// Houdini Transform remained the same
		bool bUpdateTransform = !TileActor->GetActorTransform().Equals(TileTransform);

		// Update existing landscape / tile
		if (SharedLandscapeActor && TileActorType == LandscapeActorType::LandscapeStreamingProxy)
		{
			TileActor->FixupSharedData(SharedLandscapeActor);
			if (bUpdateTransform)
			{
				TileActor->SetAbsoluteSectionBase(TileLoc);
				LandscapeInfo->FixupProxiesTransform();
				LandscapeInfo->RecreateLandscapeInfo(TileWorld,true);
			}
			
			// This is a tile with a shared landscape.
			// Check whether the LandscapeActor changed. If so, update the proxy's shared properties.
			CachedStreamingProxyActor = Cast<ALandscapeStreamingProxy>(TileActor);
			if (SharedLandscapeActor)
			{
				if (CachedStreamingProxyActor)
					bModifiedLandscapeActor = CachedStreamingProxyActor->LandscapeActor != SharedLandscapeActor;
				else
					bModifiedLandscapeActor = true;

				if (bModifiedLandscapeActor)
				{
					CachedStreamingProxyActor->LandscapeActor = SharedLandscapeActor;
					// We need to force a state update through PostEditChangeProperty here in order to initialize
					// since we're about to perform additional data updates on this tile.
					DoPostEditChangeProperty(CachedStreamingProxyActor, "LandscapeActor");
				}
			}
			else
			{
				CachedStreamingProxyActor->LandscapeActor = nullptr;
			}
			
		}
		else
		{
			// This is a standalone tile / landscape actor.
			if (bUpdateTransform)
			{
				TileActor->SetActorRelativeTransform(TileTransform);
				TileActor->SetAbsoluteSectionBase(TileLoc);
			}
		}

		UpdateTileMaterialsFn();

		// Update landscape edit layers to match layer infos
		TMap<FName, int32> ExistingLayers;
		UpdateLandscapeMaterialLayers(TargetLandscape, LayerInfos, ExistingLayers, bHasEditLayers, InEditLayerFName);
		
		CachedLandscapeActor = TileActor->GetLandscapeActor();

		ULandscapeInfo* PreviousInfo = TileActor->GetLandscapeInfo();
		if (!PreviousInfo)
			return false;

		int32 EditLayerIndex = INDEX_NONE;
		FLandscapeLayer* TargetLayer = nullptr;
		FGuid LayerGuid;
		if (bHasEditLayers)
		{
			EditLayerIndex = FindOrCreateEditLayer(CachedLandscapeActor, InEditLayerFName, AfterLayerName);
			TargetLayer = CachedLandscapeActor->GetLayer(EditLayerIndex);
			if (TargetLayer)
			{
				// Ensure target layer blend mode is additive
				TargetLayer->BlendMode = ELandscapeBlendMode::LSBM_AdditiveBlend;
				LayerGuid = TargetLayer->Guid;
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_GenerateTile] Setting edit layer to additive: %d, %s"), EditLayerIndex, *(TargetLayer->Name.ToString()));
			}
		}

		FIntRect BoundingRect = TileActor->GetBoundingRect();
		FIntPoint SectionOffset = TileActor->GetSectionBaseOffset();
		
		// Landscape region to update
		const int32 MinX = TileLoc.X;
		const int32 MaxX = TileLoc.X + UnrealTileSizeX - 1;
		const int32 MinY = TileLoc.Y;
		const int32 MaxY = TileLoc.Y + UnrealTileSizeY - 1;

		if (!bHasEditLayers || (bHasEditLayers && LayerGuid.IsValid()))
		{
			FScopedSetLandscapeEditingLayer Scope(CachedLandscapeActor, LayerGuid, [=] { CachedLandscapeActor->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });
			FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);

			bool bClearLayers = false;
			if (!ClearedLayers.Contains(InEditLayerName))
			{
				// We haven't cleared layers for this edit layer yet.
				ClearedLayers.Add(InEditLayerName);
				bClearLayers = true;
			}

			// NOTE: Use HeightmapAccessor / AlphamapAccessor instead of FLandscapeEditDataInterface.
			// FLandscapeEditDataInterface is a more low level data interface, used internally by the *Accessor tools
			// though the *Accessors do additional things like update normals and foliage.

			// Update height if it has been changed.
			if (Heightfield->bHasGeoChanged)
			{
				// It is important to update the heightmap through HeightmapAccessor this since it will properly
				// update normals and foliage.
				FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
				HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, IntHeightData.GetData());
				bHeightLayerDataChanged = true;
			}

			// Collect the stale layers so that we can clear them afterward.
			TSet<FName> StaleLayers;
			for (FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
			{
				StaleLayers.Add(Layer.LayerName);
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("Tracking potential stale layer: %s"), *(Layer.LayerName.ToString()));
			}

			// Update the layers on the landscape.
			for (FLandscapeImportLayerInfo &LayerInfo : LayerInfos)
			{
				FString PaintLayerName(LayerInfo.LayerName.ToString());

				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_GenerateTile] Drawing found layer: %s"), *(PaintLayerName) );

				if (LayerInfo.LayerName.IsEqual(HAPI_UNREAL_VISIBILITY_LAYER_NAME))
				{
					// Visibility layer has 'special' naming.
					StaleLayers.Remove(ALandscape::VisibilityLayer->LayerName);
					HOUDINI_LANDSCAPE_MESSAGE(TEXT("Removing visibility from stale layer set: %s"), *(ALandscape::VisibilityLayer->LayerName.ToString()));
				}
				else
				{
					HOUDINI_LANDSCAPE_MESSAGE(TEXT("Removing layer from stale layer set: %s"), *(LayerInfo.LayerName.ToString()));
					StaleLayers.Remove(LayerInfo.LayerName);
				}

				// Update layer properties from geo attributes
				UpdateLayerProperties(TargetLandscape, TargetLayer, LayerInfo, LayerObjectMapping);
				
				if (LayerInfo.LayerName.IsEqual(HAPI_UNREAL_VISIBILITY_LAYER_NAME))
				{
					if (LayerInfo.LayerInfo)
					{
						HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_GenerateTile] Drawing visibility layer: %s on EditLayer (%s) (bNoWeightBlend: %d)"), *(LayerInfo.LayerName.ToString()), *(InEditLayerName), (LayerInfo.LayerInfo->bNoWeightBlend) );
						// NOTE: AProxyLandscape::VisibilityLayer is a STATIC property (Info objects is being shared by ALL landscapes). Don't try to update / replace it.
						FAlphamapAccessor<false, false> AlphaAccessor(LandscapeInfo, ALandscapeProxy::VisibilityLayer);
						AlphaAccessor.SetData(MinX, MinY, MaxX, MaxY, LayerInfo.LayerData.GetData(), ELandscapeLayerPaintingRestriction::None);
					}
					else
					{
						HOUDINI_LOG_WARNING(TEXT("Cannot draw on NULL visibility layer"));
					}
						
				}
				else
				{
					ULandscapeLayerInfoObject* TargetLayerInfo = LandscapeInfo->GetLayerInfoByName(LayerInfo.LayerName);
					if (TargetLayerInfo)
					{
						HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_GenerateTile] Drawing paint layer: %s on EditLayer (%s) (bNoWeightBlend: %d)"), *(LayerInfo.LayerName.ToString()), *(InEditLayerName), (LayerInfo.LayerInfo->bNoWeightBlend) );
						FAlphamapAccessor<false, true> AlphaAccessor(LandscapeInfo, TargetLayerInfo);
						AlphaAccessor.SetData(MinX, MinY, MaxX, MaxY, LayerInfo.LayerData.GetData(), ELandscapeLayerPaintingRestriction::None);
					}
					else
					{
						HOUDINI_LOG_WARNING(TEXT("Cannot draw on NULL landscape layer: %s"), *(LayerInfo.LayerName.ToString()));
					}
				}

				bCustomLayerDataChanged = true;
			}

			// Clear stale layers
			if (bClearLayers)
			{
				for (FName& StaleLayerName : StaleLayers)
				{
					ULandscapeLayerInfoObject* LayerInfo = LandscapeInfo->GetLayerInfoByName(StaleLayerName);
					if (!LayerInfo)
					{
						continue;
					}
					HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_GenerateTile] Clearing stale layer name %s, layer info name: %s on EditLayer (%s)"), *(StaleLayerName.ToString()), *(LayerInfo->LayerName.ToString()), *(InEditLayerName) );
					TargetLandscape->ClearPaintLayer(LayerGuid, LayerInfo);
				}
			}
		}
		else
		{
			HOUDINI_LOG_ERROR(TEXT("Could not retrieve valid Guid for edit layer: %s"), *(InEditLayerName));
		}

		bModifiedLandscapeActor = true;
	}

	

	// ----------------------------------------------------
	// Apply actor tags
	// ----------------------------------------------------

	// See if we have unreal_tag_ attribute
	TArray<FName> Tags;
	if (TileActor && FHoudiniEngineUtils::GetUnrealTagAttributes(GeoId, PartId, Tags)) 
	{
		TileActor->Tags = Tags;
	}

	// ----------------------------------------------------
	// Update actor states based on data updates
	// ----------------------------------------------------
	// Based on ALandscape and ALandscapeStreamingProxy PostEditChangeProperty() implementations,
	// effect appropriate state updates based on the property updates that was performed in
	// the above code.

	// if (bSharedLandscapeMaterialChanged || bSharedLandscapeHoleMaterialChanged)
	// {
	// 	check(SharedLandscapeActor);
	// 	DoPostEditChangeProperty(SharedLandscapeActor, "LandscapeMaterial");
	// }

	// if (bTileLandscapeMaterialChanged || bTileLandscapeHoleMaterialChanged)
	// {
	// 	check(TileActor);
	// 	// Tile material changes are only processed if it wasn't already done for a shared
	// 	// landscape since the shared landscape should have already propagated the changes to associated proxies.
	// 	DoPostEditChangeProperty(TileActor, "LandscapeMaterial");
	// }
	
	if (bSharedPhysicalMaterialChanged)
	{
		check(SharedLandscapeActor);
		DoPostEditChangeProperty(SharedLandscapeActor, "DefaultPhysMaterial");
	}

	if (bTilePhysicalMaterialChanged)
	{
		check(TileActor);
		DoPostEditChangeProperty(TileActor, "DefaultPhysMaterial");
	}

	if (bModifiedSharedLandscapeActor)
	{
		SharedLandscapeActor->PostEditChange();
	}

	if (bModifiedLandscapeActor)
	{
		TileActor->PostEditChange();
	}

	{
		FLandscapeEditDataInterface LandscapeEdit(TileActor->GetLandscapeInfo());
		LandscapeEdit.RecalculateNormals();
	}

	if (LandscapeInfo)
	{
		LandscapeInfo->RecreateLandscapeInfo(TileWorld, true);
		LandscapeInfo->RecreateCollisionComponents();
	}

	{
		// Update UProperties
		
		// Apply detail attributes to both the Shared Landscape and the Landscape Tile actor
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
            GeoId, PartId,
            true,
            INDEX_NONE, INDEX_NONE, INDEX_NONE,
            PropertyAttributes))
		{
			if (IsValid(TileActor))
			{
				FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(TileActor, PropertyAttributes);
			}
			if (IsValid(SharedLandscapeActor))
			{
				FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(SharedLandscapeActor, PropertyAttributes);
			}
		}

		// Apply point attributes only to the Shared Landscape and the Landscape Tile actor
		PropertyAttributes.Empty();
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
            GeoId, PartId,
            false,
            0, INDEX_NONE, 0,
            PropertyAttributes))
		{
			if (IsValid(TileActor))
			{
				FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(TileActor, PropertyAttributes);
			}
		}
	}

	// Add objects to the HAC output.
	SetLandscapeActorAsOutput(
		InOutput,
		StaleOutputObjects,
		OutActiveLandscapes,
		InAllInputLandscapes,
		OutputAttributes,
		OutputTokens,
		InEditLayerFName,
		SharedLandscapeActor,
		SharedLandscapeActorParent,
		bCreatedSharedLandscape,
		HeightfieldIdentifier,
		TileActor,
		PackageParams.PackageMode);

#if defined(HOUDINI_ENGINE_DEBUG_LANDSCAPE)
	if (LandscapeInfo)
	{
		int32 MinX, MinY, MaxX, MaxY;
		LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Num proxies for landscape: %d"), LandscapeInfo->Proxies.Num());
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Landscape extent: %d, %d  ->  %d, %d"), MinX, MinY, MaxX, MaxY);
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Cached extent: %d, %d  ->  %d, %d"), LandscapeExtent.MinX, LandscapeExtent.MinY, LandscapeExtent.MaxX, LandscapeExtent.MaxY);
	}

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_Generate] Ending with num of output objects: %d"), InOutput->GetOutputObjects().Num());
#endif

	return true;
}

bool
FHoudiniLandscapeTranslator::OutputLandscape_ModifyLayers(
	UHoudiniOutput* InOutput,
	TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
	TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	USceneComponent* SharedLandscapeActorParent,
	const FString& DefaultLandscapeActorPrefix,
	UWorld* InWorld,
	const TMap<FString, float>& LayerMinimums,
	const TMap<FString, float>& LayerMaximums,
	FHoudiniLandscapeExtent& LandscapeExtent,
	FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
	FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
	FHoudiniPackageParams InPackageParams,
	TSet<FString>& ClearedLayers,
	TArray<UPackage*>& OutCreatedPackages)
{
	TArray<FString> EditLayerNames;
	const bool bHasEditLayers = GetEditLayersFromOutput(InOutput, EditLayerNames);

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_ModifyLayers] Generating landscape tile. Has edit layers?: %d"), bHasEditLayers);

	TArray<FName> AllLayerNames;
	for (const FString& LayerName : EditLayerNames)
	{
		AllLayerNames.Add(FName(LayerName));
	}

	if (!bHasEditLayers)
	{
		// Add a dummy edit layer to simply get us into the following for-loop.
		EditLayerNames.Add(FString());
	}

	bool Result = true;
	for (const FString& LayerName : EditLayerNames)
	{
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::OutputLandscape_ModifyLayers] Creating edit layer for heightfield: %s"), *LayerName);
		// If edit layers are enabled for this volume, create each layer for this landscape tile
		FName LayerFName(LayerName);
		
		Result = Result && OutputLandscape_ModifyLayer(InOutput,
			CreatedUntrackedActors,
			InputLandscapesToUpdate,
			InAllInputLandscapes,
			SharedLandscapeActorParent,
			DefaultLandscapeActorPrefix,
			InWorld,
			LayerMinimums,
			LayerMaximums,
			LandscapeExtent,
			LandscapeTileSizeInfo,
			LandscapeReferenceLocation,
			InPackageParams,
			bHasEditLayers,
			LayerFName,
			ClearedLayers,
			OutCreatedPackages);
	}
	return Result;
}

bool
FHoudiniLandscapeTranslator::OutputLandscape_ModifyLayer(
	UHoudiniOutput* InOutput,
	TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
	TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	USceneComponent* SharedLandscapeActorParent,
	const FString& DefaultLandscapeActorPrefix,
	UWorld* World,
	const TMap<FString, float>& LayerMinimums,
	const TMap<FString, float>& LayerMaximums,
	FHoudiniLandscapeExtent& LandscapeExtent,
	FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
	FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
	FHoudiniPackageParams InPackageParams,
	const bool bHasEditLayers,
	const FName& EditLayerName,
	TSet<FString>& ClearedLayers,
	TArray<UPackage*>& OutCreatedPackages)
{
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::OutputLandscape_EditableLayer] ======================================================================="));
	
	check(LayerMinimums.Contains(TEXT("height")));
	check(LayerMaximums.Contains(TEXT("height")));

	float fGlobalMin = LayerMinimums.FindChecked(TEXT("height"));
	float fGlobalMax = LayerMaximums.FindChecked(TEXT("height"));

	if (!IsValid(InOutput))
		return false;

	UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput);
	if (!IsValid(HAC))
		return false;

	//  Get the height map.
	const FHoudiniGeoPartObject* Heightfield = GetHoudiniHeightFieldFromOutput(InOutput, bHasEditLayers, EditLayerName);
	if (!Heightfield)
		return false;

	if (Heightfield->Type != EHoudiniPartType::Volume)
		return false;

	const HAPI_NodeId GeoId = Heightfield->GeoId;
	const HAPI_PartId PartId = Heightfield->PartId;

	TArray<int> IntData;
	TArray<FString> StrData;
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	// ---------------------------------------------
	// Attribute: unreal_landscape_editlayer_type
	// ---------------------------------------------
	int32 EditLayerType = HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_BASE;
	IntData.Empty();
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TYPE, AttributeInfo, IntData, 1))
	{
		if (IntData.Num() > 0)
		{
			EditLayerType = IntData[0];
		}
	}

	const bool bLayerNoWeightBlend = EditLayerType == HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_BASE ? false : true;

	// ---------------------------------------------
	// Attribute: unreal_landscape_editlayer_name
	// ---------------------------------------------
	StrData.Empty();
	FString EditableLayerName;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME,
		AttributeInfo, StrData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (StrData.Num() > 0)
		{
			EditableLayerName = StrData[0];
		}
	}
	if (EditableLayerName.IsEmpty())
		return false;

	// Construct the identifier of the Heightfield geo part.
	FHoudiniOutputObjectIdentifier HeightfieldIdentifier(Heightfield->ObjectId, GeoId, PartId, "Heightfield");
	HeightfieldIdentifier.PartName = Heightfield->PartName;

	// Extract the float data from the Heightfield.
	const FHoudiniVolumeInfo &VolumeInfo = Heightfield->VolumeInfo;
	TArray<float> FloatValues;
	float FloatMin, FloatMax;
	if (!GetHoudiniHeightfieldFloatData(Heightfield, FloatValues, FloatMin, FloatMax))
		return false;

	// Get the Unreal landscape size 
	const int32 HoudiniHeightfieldXSize = VolumeInfo.YLength;
	const int32 HoudiniHeightfieldYSize = VolumeInfo.XLength;

	if (!LandscapeTileSizeInfo.bIsCached)
	{
		// Calculate a landscape size info from this heightfield to be
		// used by subsequent tiles on the same landscape
		if (FHoudiniLandscapeTranslator::CalcLandscapeSizeFromHeightfieldSize(
			HoudiniHeightfieldXSize,
			HoudiniHeightfieldYSize,
			LandscapeTileSizeInfo.UnrealSizeX,
			LandscapeTileSizeInfo.UnrealSizeY,
			LandscapeTileSizeInfo.NumSectionsPerComponent,
			LandscapeTileSizeInfo.NumQuadsPerSection))
		{
			LandscapeTileSizeInfo.bIsCached = true;
		}
		else
		{
			return false;
		}
	}

	TMap<FString,FString> OutputTokens;
	FHoudiniAttributeResolver Resolver;
	// Update resolver attributes and tokens before we start resolving attributes.
	InPackageParams.UpdateTokensFromParams(World, HAC, OutputTokens);

	// ---------------------------------------------
	// Attribute: unreal_landscape_actor_name
	// ---------------------------------------------
	// Retrieve the name of the main Landscape actor to look for
	FString TargetLandscapeName = "Input0";
	bool bHasEditLayerTarget = false;
	StrData.Empty();
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TARGET,
		AttributeInfo, StrData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
	{
		if (StrData.Num() > 0 && !StrData[0].IsEmpty())
		{
			TargetLandscapeName = StrData[0];
			bHasEditLayerTarget = true;
		}
	}

	if (!bHasEditLayerTarget)
	{
		// The previous implementation of the "Edit Layer" mode used the Shared Landscape Actor name. If the (new)
		// Edit Layer Target attribute wasn't specified, check whether the old attribute is present.
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME,
		AttributeInfo, StrData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (StrData.Num() > 0 && !StrData[0].IsEmpty())
			{
				TargetLandscapeName = StrData[0];
			}
		}
	}

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Target Landscape Name (Attrib): %s"), *(TargetLandscapeName));

	Resolver.SetAttribute(HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TARGET, TargetLandscapeName);
	Resolver.SetTokensFromStringMap(OutputTokens);
	TargetLandscapeName = Resolver.ResolveAttribute(HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TARGET, TargetLandscapeName);

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Target Landscape Name (Resolved): %s"), *(TargetLandscapeName));

	// ---------------------------------------------
	// Find the landscape that we're targeting for output
	// ---------------------------------------------
	ALandscapeProxy* TargetLandscapeProxy = FindTargetLandscapeProxy(TargetLandscapeName, World, InAllInputLandscapes);
	if (!IsValid(TargetLandscapeProxy))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not find landscape actor: %s"), *(TargetLandscapeName));
		return false;
	}
	ALandscape* TargetLandscape = TargetLandscapeProxy->GetLandscapeActor();
	if (!IsValid(TargetLandscape))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not retrieve the landscape actor for: %s"), *(TargetLandscapeName));
		return false;
	}

	if (!TargetLandscape->bCanHaveLayersContent)
	{
		HOUDINI_LOG_WARNING(TEXT("Target landscape does not have edit layers enabled!: %s"), *(TargetLandscapeName));
		return false;
	}

	ULandscapeInfo* TargetLandscapeInfo = TargetLandscapeProxy->GetLandscapeInfo();
	const FTransform TargetLandscapeTransform = TargetLandscape->GetActorTransform();

	const float DestHeightScale = TargetLandscapeProxy->LandscapeActorToWorld().GetScale3D().Z;
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Dest Height Scale: %f"), DestHeightScale);

	// Create the layer if it doesn't exist
	int32 EditLayerIndex = TargetLandscape->GetLayerIndex(FName(EditableLayerName));
	const FLandscapeLayer* TargetLayer = TargetLandscape->GetLayer(EditLayerIndex);
	if (!TargetLayer)
	{
		// Create new edit layer
		EditLayerIndex = TargetLandscape->CreateLayer(FName(EditableLayerName));
		TargetLayer = TargetLandscape->GetLayer(FName(EditableLayerName));
	}

	if (!TargetLayer)
	{
		HOUDINI_LOG_WARNING(TEXT("Could not find or create target layer: %s"), *(TargetLandscapeName));
		return false;
	}

	{
		// ---------------------------------------------
		// Attribute: unreal_landscape_editlayer_after
		// ---------------------------------------------
		StrData.Empty();
		FString AfterLayerName;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER,
			AttributeInfo, StrData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (StrData.Num() > 0)
			{
				AfterLayerName = StrData[0];
			}
		}
		if (!AfterLayerName.IsEmpty())
		{
			// If we have an "after layer", move the output layer into position.
			int32 NewLayerIndex = TargetLandscape->GetLayerIndex(FName(AfterLayerName));
			if (NewLayerIndex != INDEX_NONE && EditLayerIndex != NewLayerIndex)
			{
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Moving layer from %d to %d"), EditLayerIndex, NewLayerIndex);
				if (NewLayerIndex < EditLayerIndex)
				{
					NewLayerIndex += 1;
				}
				TargetLandscape->ReorderLayer(EditLayerIndex, NewLayerIndex);

				// Ensure we have the correct layer/index
				EditLayerIndex = TargetLandscape->GetLayerIndex(FName(EditableLayerName));
				TargetLayer = TargetLandscape->GetLayer(EditLayerIndex);
			}
		}
	}
	
	// ----------------------------------------------------
	// Convert Heightfield data
	// ----------------------------------------------------
	// Convert Houdini's heightfield data to Unreal's landscape data
	TArray<uint16> IntHeightData;
	FTransform TileTransform;
	if (!FHoudiniLandscapeTranslator::ConvertHeightfieldDataToLandscapeData(
		FloatValues, VolumeInfo,
		LandscapeTileSizeInfo.UnrealSizeX, LandscapeTileSizeInfo.UnrealSizeY,
		FloatMin, FloatMax,
		IntHeightData, TileTransform,
		false, true, DestHeightScale,
		EditLayerType == HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_ADDITIVE))
			return false;

	// ----------------------------------------------------
	//  Calculate the draw location (in quad space) of
	//  where the Modify Layer should be drawn 
	// ----------------------------------------------------

	// In order to determine the 'draw location' in quad space we have to do the following:
	// 1. Calculate the heightfield's world space position
	// 2. Calculate the transform of the heightfield _relative_ to the Landscape's origin 
	// 3. Convert the relative HF transform to a quad space coordinate
	// 4. Final draw coordinate: Landscape's section base offset + relative quad space coordinate

	FTransform UnscaledLandscapeTransform = TargetLandscapeTransform;
	UnscaledLandscapeTransform.SetScale3D(FVector::OneVector);
	
	const FTransform TileWSTransform = TileTransform * HAC->GetComponentTransform();
	const FTransform RelativeTileTransform = TileWSTransform * UnscaledLandscapeTransform.Inverse();

	FVector LandscapeScale = TargetLandscapeTransform.GetScale3D();
	const FVector RelativeTileCoordinate = RelativeTileTransform.GetLocation() / LandscapeScale;
	
	const FIntPoint LandscapeBaseLoc = TargetLandscape->GetSectionBaseOffset();

	// Calculate the final draw coordinates

	FIntPoint TargetTileLoc;
	TargetTileLoc.X = LandscapeBaseLoc.X + FMath::RoundToInt(RelativeTileCoordinate.X);
	TargetTileLoc.Y = LandscapeBaseLoc.Y + FMath::RoundToInt(RelativeTileCoordinate.Y);

	FVector TileMin, TileMax;
	TileMin.X = TargetTileLoc.X;
	TileMin.Y = TargetTileLoc.Y;
	TileMax.X = TargetTileLoc.X + LandscapeTileSizeInfo.UnrealSizeX - 1;
	TileMax.Y = TargetTileLoc.Y + LandscapeTileSizeInfo.UnrealSizeY - 1;
	TileMin.Z = TileMax.Z = 0.f;

	// NOTE: we don't manually inject a tile number in the object name. This should
	// already be encoded in the TileName string.
	FHoudiniPackageParams TilePackageParams = InPackageParams;
	FHoudiniPackageParams LayerPackageParams = InPackageParams;
	
	TilePackageParams.ObjectName = TargetLandscapeName;
	LayerPackageParams.ObjectName = TargetLandscapeName;

	// Look for all the layers/masks corresponding to the current heightfield.
	TArray< const FHoudiniGeoPartObject* > FoundLayers;
	FHoudiniLandscapeTranslator::GetHeightfieldsLayersFromOutput(InOutput, *Heightfield, bHasEditLayers, EditLayerName, FoundLayers);

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Found %d output layers."), FoundLayers.Num());

	// If we are operating in EditLayer mode, then any layers we create or manage should
	// have their bNoWeightBlend set to true otherwise all the paint layers will act as additive
	// which, most of the time, is not what we expect.
	const bool bDefaultNoWeightBlend = bHasEditLayers ? true : false;
	
	// Get the updated layers.
	TArray<FLandscapeImportLayerInfo> LayerInfos;
	TMap<FName, const FHoudiniGeoPartObject*> LayerObjectMapping;
	if (!CreateOrUpdateLandscapeLayerData(FoundLayers, *Heightfield, LandscapeTileSizeInfo.UnrealSizeX, LandscapeTileSizeInfo.UnrealSizeY, 
		LayerMinimums, LayerMaximums, LayerInfos, false, bDefaultNoWeightBlend,
		TilePackageParams,
		LayerPackageParams,
		OutCreatedPackages,
		LayerObjectMapping
		))
		return false;

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Generated %d layer infos."), LayerInfos.Num());

	// Update landscape edit layers to match layer infos
	TMap<FName, int32> ExistingLayers;
	UpdateLandscapeMaterialLayers(TargetLandscape, LayerInfos, ExistingLayers, bHasEditLayers, EditLayerName);

	// Clear layers
	if (!ClearedLayers.Contains(EditableLayerName))
	{
		bool bClearLayer = false;
		// ---------------------------------------------
		// Attribute: unreal_landscape_editlayer_clear
		// ---------------------------------------------
		// Check whether we should clear the target edit layer.
		IntData.Empty();
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
			GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR, 
			AttributeInfo, IntData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (IntData.Num() > 0)
			{
				bClearLayer = IntData[0] != 0;
			}
		}

		if (bClearLayer)
		{
			if (TargetLayer)
			{
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Clearing layer: %s"), *EditableLayerName);
				ClearedLayers.Add(EditableLayerName);

				// Clear the heightmap
				TargetLandscape->ClearLayer(TargetLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);

				// Clear the paint layers, but only the ones that are being output from Houdini.
				for (FLandscapeImportLayerInfo &InLayerInfo : LayerInfos)
				{
					if (!ExistingLayers.Contains(InLayerInfo.LayerName))
						continue;

					TargetLandscape->ClearPaintLayer(TargetLayer->Guid, InLayerInfo.LayerInfo);
				}
			}
		}
	}


	{
		// Scope the Edit Layer before we start drawing on ANY of the layers
		FScopedSetLandscapeEditingLayer Scope(TargetLandscape, TargetLayer->Guid, [=] { TargetLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });
        FLandscapeEditDataInterface LandscapeEdit(TargetLandscapeInfo);

		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Drawing heightmap.."));
		// Draw Heightmap
		FHeightmapAccessor<false> HeightmapAccessor(TargetLandscapeInfo);
        HeightmapAccessor.SetData(TileMin.X, TileMin.Y, TileMax.X, TileMax.Y, IntHeightData.GetData());

		// Draw material layers on the landscape
		// Update the layers on the landscape.
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Target has layers content: %d"), TargetLandscape->HasLayersContent());
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] IsEditingLayer? %d"), TargetLandscape->GetEditingLayer().IsValid());
		
		for (FLandscapeImportLayerInfo &InLayerInfo : LayerInfos)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Trying to draw on layer: %s"), *(InLayerInfo.LayerName.ToString()));
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Dest Region: %f, %f, -> %f, %f"), TileMin.X, TileMin.Y, TileMax.X, TileMax.Y);

			// Update layer properties from geo attributes
			UpdateLayerProperties(TargetLandscape, TargetLayer, InLayerInfo, LayerObjectMapping);
			
			if (InLayerInfo.LayerInfo && InLayerInfo.LayerName.IsEqual(HAPI_UNREAL_VISIBILITY_LAYER_NAME))
			{
				// NOTE: AProxyLandscape::VisibilityLayer is a STATIC property (Info objects is being shared by ALL landscapes). Don't try to update / replace it.
				FAlphamapAccessor<false, false> AlphaAccessor(TargetLandscapeInfo, ALandscapeProxy::VisibilityLayer);
				AlphaAccessor.SetData(TileMin.X, TileMin.Y, TileMax.X, TileMax.Y, InLayerInfo.LayerData.GetData(), ELandscapeLayerPaintingRestriction::None);
			}
			else
			{
				if (!ExistingLayers.Contains(InLayerInfo.LayerName))
					continue;
				
				int32 LayerIndex = ExistingLayers.FindChecked(InLayerInfo.LayerName);
				if (TargetLandscape->EditorLayerSettings.IsValidIndex(LayerIndex))
				{
					FLandscapeEditorLayerSettings& CurLayer = TargetLandscape->EditorLayerSettings[LayerIndex];
					// Draw on the current layer, if it is valid.
					if (CurLayer.LayerInfoObj)
					{
						HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Drawing using Alpha accessor. Dest Region: %f, %f, -> %f, %f"), TileMin.X, TileMin.Y, TileMax.X, TileMax.Y);
						FAlphamapAccessor<false, true> AlphaAccessor(TargetLandscapeInfo, CurLayer.LayerInfoObj);
						AlphaAccessor.SetData(TileMin.X, TileMin.Y, TileMax.X, TileMax.Y, InLayerInfo.LayerData.GetData(), ELandscapeLayerPaintingRestriction::None);
					}
				}
			}
		}
		
	} // Landscape layer drawing scope

	// Only keep output the output object that corresponds to this layer. Everything else should be removed.
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects = InOutput->GetOutputObjects();
	TSet<FHoudiniOutputObjectIdentifier> StaleOutputs;
	OutputObjects.GetKeys(StaleOutputs);
	bool bFoundOutputObject = false;
	for(auto& Entry : OutputObjects)
	{
		if (bFoundOutputObject)
			continue; // We already have a matching layer output object. Anything else is stale.
		
		FHoudiniOutputObjectIdentifier& OutputId = Entry.Key;
		FHoudiniOutputObject& Object = Entry.Value;
		UHoudiniLandscapeEditLayer* EditLayer = Cast<UHoudiniLandscapeEditLayer>(Object.OutputObject);
		if (!IsValid(EditLayer))
			continue;
		StaleOutputs.Remove(OutputId);
	}

	// Clean up stale outputs
	for(FHoudiniOutputObjectIdentifier& StaleId : StaleOutputs)
	{
		FHoudiniOutputObject& OutputObject = OutputObjects.FindChecked(StaleId);
		if (UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.OutputObject))
		{
			ALandscapeProxy* LandscapeProxy = LandscapePtr->GetRawPtr();

			if (LandscapeProxy)
			{
				// We shouldn't destroy any input landscapes
				if (!InAllInputLandscapes.Contains(LandscapeProxy))
				{
					LandscapeProxy->Destroy();
					LandscapePtr->SetSoftPtr(nullptr);
				}
			}
		}
		
		OutputObjects.Remove(StaleId);
	}

	// Update the output object
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier(Heightfield->ObjectId, GeoId, PartId, "EditableLayer");
	FHoudiniOutputObject& OutputObj = InOutput->GetOutputObjects().FindOrAdd(OutputObjectIdentifier);
	UHoudiniLandscapeEditLayer* LayerPtr = NewObject<UHoudiniLandscapeEditLayer>(InOutput);
	LayerPtr->SetSoftPtr(TargetLandscape);
	LayerPtr->LayerName = EditableLayerName;
	OutputObj.OutputObject = LayerPtr;
	// Editable layers doesn't currently require any attributes / tokens to be cached.
	// OutputObj.CachedAttributes = OutputAttributes;
	// OutputObj.CachedTokens = OutputTokens;
	
	return true;
}


bool
FHoudiniLandscapeTranslator::IsLandscapeInfoCompatible(
	const ULandscapeInfo* LandscapeInfo,
	const int32 InNumSectionsPerComponent,
	const int32 InNumQuadsPerSection
	)
{
	if (!IsValid(LandscapeInfo))
		return false;
	

	if (LandscapeInfo->ComponentNumSubsections != InNumSectionsPerComponent)
		return false;

	if (LandscapeInfo->SubsectionSizeQuads != InNumQuadsPerSection)
		return false;

	int32 MinX, MinY, MaxX, MaxY;
	if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		const int32 NumComponentsX = (MaxX - MinX) / (InNumQuadsPerSection*InNumSectionsPerComponent);
		const int32 NumComponentsY = (MaxY - MinY) / (InNumQuadsPerSection*InNumSectionsPerComponent);
	}
	
	return true;
}

bool
FHoudiniLandscapeTranslator::IsLandscapeTileCompatible(
	const ALandscapeProxy* TileActor,
	const int32 InTileSizeX,
	const int32 InTileSizeY,
	const int32 InNumSectionsPerComponent,
	const int32 InNumQuadsPerSection
)
{
	check(TileActor);

	// NOTE: We can't compare landscape extents here since the Houdini only knows about the size for single tile.
	// and LandscapeInfo will only return extents for the *loaded* landscape tiles.

	// TODO: Add more robust checks to determine landscape compatibility.

	if (!IsLandscapeInfoCompatible(TileActor->GetLandscapeInfo(), InNumSectionsPerComponent, InNumQuadsPerSection))
		return false;

	const FIntRect Bounds = TileActor->GetBoundingRect();
	const FIntPoint Size = Bounds.Size();
	if (Size.X != InTileSizeX && Size.Y != InTileSizeY)
		return false;

	return true;
}


bool FHoudiniLandscapeTranslator::IsLandscapeTypeCompatible(const AActor* Actor, LandscapeActorType ActorType)
{
	if (!IsValid(Actor))
		return false;

	switch (ActorType)
	{
	case LandscapeActorType::LandscapeActor:
		return Actor->IsA<ALandscape>();
		break;
	case LandscapeActorType::LandscapeStreamingProxy:
		return Actor->IsA<ALandscapeStreamingProxy>();
		break;
	default:
		break;
	}

	return false;
}

bool FHoudiniLandscapeTranslator::PopulateLandscapeExtents(FHoudiniLandscapeExtent& Extent,
	const ULandscapeInfo* LandscapeInfo)
{
	if (LandscapeInfo->GetLandscapeExtent(Extent.MinX, Extent.MinY, Extent.MaxX, Extent.MaxY))
	{
		Extent.ExtentsX = Extent.MaxX - Extent.MinX;
		Extent.ExtentsY = Extent.MaxY - Extent.MinY;
		Extent.bIsCached = true;
		
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::PopulateLandscapeExtents] Num proxies for landscape: %d"), LandscapeInfo->Proxies.Num());
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::PopulateLandscapeExtents] Cached extent: %d, %d  ->  %d, %d"), Extent.MinX, Extent.MinY, Extent.MaxX, Extent.MaxY);
		
		return true;
	}
	return false;
}

bool FHoudiniLandscapeTranslator::UpdateLandscapeMaterialLayers(ALandscape* InLandscape,
	const TArray<FLandscapeImportLayerInfo>& LayerInfos,
	TMap<FName, int32>& OutPaintLayers,
	bool bHasEditLayers,
	const FName& EditLayerName)
{
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::UpdateLandscapeEditLayers] Num Layer Infos: %d"), (LayerInfos.Num()));
	
	check(InLandscape);

	ULandscapeInfo* LandscapeInfo = InLandscape->GetLandscapeInfo();

	FGuid LayerGuid;
	if (bHasEditLayers)
	{
		if (const FLandscapeLayer* Layer = InLandscape->GetLayer(EditLayerName))
		{
			LayerGuid = Layer->Guid;
		}
	}

	// If we are dealing with edit layers we need Lock the landscape edit layer, i.e., if the Guid is valid.
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::UpdateLandscapeEditLayers] Scope locking edit layer: %s (%s)"), *(EditLayerName.ToString()), *(LayerGuid.ToString()));
	FScopedSetLandscapeEditingLayer Scope(InLandscape, LayerGuid, [=] { InLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });
	
	OutPaintLayers.Empty();
	GetLandscapePaintLayers(InLandscape, OutPaintLayers);

	bool bLayerHasChanged = false;
	for (const FLandscapeImportLayerInfo &InLayerInfo : LayerInfos)
	{
		// NOTE: Don't update layer blend weight settings here. It should already have been set in CreateOrUpdateLandscapeLayerData.

		// The following layer adding / replacing code have been referenced from:
		// - ALandscapeProxy::CreateLayerInfo(...)
		// - FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerSetObject(...)

		int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(InLayerInfo.LayerName);
		if (LayerInfoIndex == INDEX_NONE)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::UpdateLandscapeEditLayers] Creating new LandscapeLayerInfo: %s"), *(InLayerInfo.LayerName.ToString()));
			// The material layer does not yet exist
			LayerInfoIndex = LandscapeInfo->Layers.Add(FLandscapeInfoLayerSettings(InLayerInfo.LayerInfo, InLandscape));
			LandscapeInfo->CreateLayerEditorSettingsFor(InLayerInfo.LayerInfo);
			bLayerHasChanged = true;
		}
		else
		{
			FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerInfoIndex];
			if (!LayerSettings.LayerInfoObj)
			{
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::UpdateLandscapeEditLayers] Setting new LandscapeLayerInfo on existing layer: %s"), *(InLayerInfo.LayerName.ToString()));
				LayerSettings.LayerInfoObj = InLayerInfo.LayerInfo;
				LandscapeInfo->CreateLayerEditorSettingsFor(InLayerInfo.LayerInfo);
				bLayerHasChanged = true;
			}
			else if (LayerSettings.LayerInfoObj != InLayerInfo.LayerInfo)
			{
				// Existing material layer has mismatching Layer Info object. Update it.
				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::UpdateLandscapeEditLayers] Replacing existing LandscapeLayerInfo: %s"), *(InLayerInfo.LayerName.ToString()));
				LandscapeInfo->ReplaceLayer(LayerSettings.LayerInfoObj, InLayerInfo.LayerInfo);
				LayerSettings.LayerInfoObj = InLayerInfo.LayerInfo;
				bLayerHasChanged = true;
			}
		}

		if (LayerInfoIndex != INDEX_NONE)
		{
			OutPaintLayers.Add(InLayerInfo.LayerName, LayerInfoIndex);
		}
	}

	if (bLayerHasChanged)
	{
		LandscapeInfo->UpdateLayerInfoMap();
	}

	return bLayerHasChanged;
}

// bool
// FHoudiniLandscapeTranslator::ClearEditLayer(
// 	ALandscape* InLandscape,
// 	const TArray<FLandscapeImportLayerInfo>& LayerInfos,
// 	TSet<FString>& ClearedLayers,
// 	bool bHasEditLayer,
// 	const FString& EditLayerName)
// {
// 	TArray<int> IntData;
// 	// Clear layers
// 	if (!ClearedLayers.Contains(EditLayerName))
// 	{
// 		bool bClearLayer = false;
// 		// ---------------------------------------------
// 		// Attribute: unreal_landscape_editlayer_clear
// 		// ---------------------------------------------
// 		// Check whether we should clear the target edit layer.
// 		IntData.Empty();
// 		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
// 			HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR, AttributeInfo, IntData, 1))
// 		{
// 			if (IntData.Num() > 0)
// 			{
// 				bClearLayer = IntData[0] != 0;
// 			}
// 		}
//
// 		if (bClearLayer)
// 		{
// 			if (TargetLayer)
// 			{
// 				HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Clearing layer heightmap: %s"), *EditableLayerName);
// 				ClearedLayers.Add(EditLayerName);
//
// 				// Clear the heightmap
// 				TargetLandscape->ClearLayer(TargetLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);
//
// 				// Clear the paint layers, but only the ones that are being output from Houdini.
// 				for (FLandscapeImportLayerInfo &InLayerInfo : LayerInfos)
// 				{
// 					if (!ExistingLayers.Contains(InLayerInfo.LayerName))
// 						continue;
// 					
// 					int32 LayerIndex = ExistingLayers.FindChecked(InLayerInfo.LayerName);
// 					TargetLandscape->ClearPaintLayer(TargetLayer->Guid, InLayerInfo.LayerInfo);
// 				}
// 			}
// 		}
// }

ALandscapeProxy*
FHoudiniLandscapeTranslator::FindExistingLandscapeActor(
	UWorld* InWorld,
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy *>& ValidLandscapes,
	int32 UnrealLandscapeSizeX,
	int32 UnrealLandscapeSizeY,
	const FString& InActorName,
	const FString& InPackagePath, 
	UWorld*& OutWorld,
	ULevel*& OutLevel,
	bool& bCreatedPackage,
	const EPackageMode& InPackageMode)
{
	if (InPackageMode == EPackageMode::Bake)
		return FindExistingLandscapeActor_Bake(InWorld, InOutput, ValidLandscapes, UnrealLandscapeSizeX, UnrealLandscapeSizeY, InActorName, InPackagePath, OutWorld, OutLevel, bCreatedPackage);
	else
		return FindExistingLandscapeActor_Temp(InWorld, InOutput, ValidLandscapes, UnrealLandscapeSizeX, UnrealLandscapeSizeY, InActorName, InPackagePath, OutWorld, OutLevel, bCreatedPackage);
}

ALandscapeProxy*
FHoudiniLandscapeTranslator::FindExistingLandscapeActor_Bake(
	UWorld* InWorld,
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy *>& ValidLandscapes,
	int32 UnrealLandscapeSizeX,
	int32 UnrealLandscapeSizeY,
	const FString& InActorName,
	const FString& InPackagePath, // Package path to search if not found in the world
	UWorld*& OutWorld,
	ULevel*& OutLevel,
	bool& bCreatedPackage)
{
	bCreatedPackage = false;
	
	// // Locate landscape proxy actor when running in baked mode
	// AActor* FoundActor = nullptr;
	ALandscapeProxy* OutActor = nullptr;
	// OutActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscapeProxy>(InWorld, InActorName, FoundActor);
	// // OutActor = FHoudiniEngineUtils::FindActorInWorld<ALandscapeProxy>(InWorld, FName(InActorName));
	// if (FoundActor && FoundActor != OutActor)
	// 	FoundActor->Destroy(); // nuke it!
	//
	// if (OutActor)
	// {
	// 	// TODO: make sure that the found is actor is actually assigned to the level defined by package path.
	// 	//       If the found actor is not from that level, it should be moved there.
	//
	// 	OutWorld = OutActor->GetWorld();
	// 	OutLevel = OutActor->GetLevel();
	// }
	// else
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

		// if (!bActorInWorld)
		// {
		// 	// The OutLevel is not present in the current world which means we might
		// 	// still find the tile actor in OutWorld.
			OutActor = FHoudiniEngineUtils::FindActorInWorld<ALandscapeProxy>(OutWorld, FName(InActorName));
		// }
	}

	if (OutWorld && OutLevel)
	{
		// Ensure that a LevelBounds actor is present in the sublevel. This is needed for world composition.
		bool bHasLevelBounds = false;
		for (TActorIterator<ALevelBounds> ActorItr(OutLevel->GetWorld()); ActorItr; ++ActorItr)
		{
			if (ActorItr->GetLevel() == OutLevel)
			{
				bHasLevelBounds = true;
				break; 
			}
		}

		if (!bHasLevelBounds)
		{
			FActorSpawnParameters SpawnParms;
			SpawnParms.OverrideLevel = OutLevel;
			ALevelBounds* LevelBoundsActor = OutWorld->SpawnActor<ALevelBounds>(SpawnParms);
			LevelBoundsActor->MarkPackageDirty();
		}
	}

	return OutActor;
}


ALandscapeProxy* FHoudiniLandscapeTranslator::FindTargetLandscapeProxy(const FString& ActorName, UWorld* World,
	const TArray<ALandscapeProxy*>& LandscapeInputs)
{
	int32 InputIndex = INDEX_NONE;
	if (ActorName.StartsWith(TEXT("Input")))
	{
		// Extract the numeric value after 'Input'.
		FString IndexStr;
		ActorName.Split(TEXT("Input"), nullptr, &IndexStr);
		if (IndexStr.IsNumeric())
		{
			InputIndex = FPlatformString::Atoi(*IndexStr);
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FindTargetLandscapeProxy] Extract index %d from actor name: %s"), InputIndex, *ActorName);
		}
	}
	
	if (InputIndex != INDEX_NONE)
	{
		if (!LandscapeInputs.IsValidIndex(InputIndex))
			return nullptr;
		return LandscapeInputs[InputIndex];
	}

	return FHoudiniEngineUtils::FindActorInWorldByLabel<ALandscapeProxy>(World, ActorName);
}

bool FHoudiniLandscapeTranslator::GetEditLayersFromOutput(UHoudiniOutput* InOutput, TArray<FString>& InEditLayers)
{
	if (!InOutput)
		return false;

	InEditLayers.Empty();
	bool bFoundEditLayers = false;
	const TArray<FHoudiniGeoPartObject>& GeoObjects = InOutput->GetHoudiniGeoPartObjects();
	for (const FHoudiniGeoPartObject& PartObj : GeoObjects)
	{
		if (PartObj.Type != EHoudiniPartType::Volume)
			continue;
		
		if (!PartObj.bHasEditLayers)
			continue;
		
		if (PartObj.VolumeLayerName.IsEmpty())
			continue;
		
		if (!InEditLayers.Contains(PartObj.VolumeLayerName))
		{
			InEditLayers.Add(PartObj.VolumeLayerName);
			bFoundEditLayers = true;
		}
	}
	
	return bFoundEditLayers;
}

void FHoudiniLandscapeTranslator::GetLandscapePaintLayers(ALandscape* Landscape, TMap<FName, int32>& OutEditLayers)
{
	check(Landscape);
	
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::GetLandscapePaintLayers] Collecting paint layers..."));

	if (Landscape->HasLayersContent())
	{
		const int32 NumTargetLayers = Landscape->EditorLayerSettings.Num();
		for(int32 LayerIndex = 0; LayerIndex < NumTargetLayers; LayerIndex++)
		{
			FLandscapeEditorLayerSettings& Settings = Landscape->EditorLayerSettings[LayerIndex];
			if (!Settings.LayerInfoObj)
				continue;
			OutEditLayers.Add(Settings.LayerInfoObj->LayerName, LayerIndex);
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::GetLandscapePaintLayers] Found existing landscape material layer: %s"), *(Settings.LayerInfoObj->LayerName.ToString()));
		}
	}
	else
	{
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();

		const int32 NumLayers = LandscapeInfo->Layers.Num();
		for(int32 LayerIndex = 0; LayerIndex < NumLayers; LayerIndex++)
		{
			FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerIndex];
			// FLandscapeLayer* Layer = Landscape->GetLayer(LayerIndex);
			OutEditLayers.Add(LayerSettings.LayerName, LayerIndex);
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::GetLandscapePaintLayers] Found existing landscape material layer: %s"), *(LayerSettings.LayerName.ToString()));
		}
	}
}


ALandscapeProxy*
FHoudiniLandscapeTranslator::FindExistingLandscapeActor_Temp(
	UWorld* InWorld,
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy *>& ValidLandscapes,
	int32 UnrealLandscapeSizeX,
	int32 UnrealLandscapeSizeY,
	const FString& InActorName,
	const FString& InPackagePath, 
	UWorld*& OutWorld,
	ULevel*& OutLevel,
	bool& bCreatedPackage)
{
	ALandscapeProxy* OutActor = nullptr;
	FString ActorName = InActorName + TEXT("_Temp");
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& PrevCookObjects = InOutput->GetOutputObjects();

	OutWorld = InWorld;
	OutLevel = InWorld->PersistentLevel;

	bCreatedPackage = false;
	
	// Find Landscape proxy for output when running in Temp mode
	for(auto& PrevObject : PrevCookObjects)
	{
		UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(PrevObject.Value.OutputObject);
		if (!LandscapePtr)
			continue;

		OutActor = LandscapePtr->GetRawPtr();
		if (!OutActor)
		{
			// We may need to manually load the object
			OutActor = LandscapePtr->LandscapeSoftPtr.LoadSynchronous();
		}

		if (!OutActor)
			continue;

		// If we were updating the input landscape before, but aren't anymore,
		// we could still find it here in the output, ignore them now as we're only looking for previous output
		if (ValidLandscapes.Contains(OutActor))
			continue;

		if (OutActor->GetName() != ActorName)
			// This is not the droid we're looking for
			continue;

		if (!IsValid(OutActor))
		{
			FHoudiniEngineUtils::RenameToUniqueActor(OutActor, ActorName + "_pendingkill");
			continue;
		}

		// If we found a possible candidate, make sure that its size matches ours
		// as we can only update a landscape of the same size
		ULandscapeInfo* PreviousInfo = OutActor->GetLandscapeInfo();
		if (PreviousInfo)
		{
			int32 PrevMinX = 0;
			int32 PrevMinY = 0;
			int32 PrevMaxX = 0;
			int32 PrevMaxY = 0;
			PreviousInfo->GetLandscapeExtent(PrevMinX, PrevMinY, PrevMaxX, PrevMaxY);

			if ((PrevMaxX - PrevMinX + 1) == UnrealLandscapeSizeX && (PrevMaxY - PrevMinY + 1) == UnrealLandscapeSizeY)
			{
				// The size matches, we can reuse the old landscape.
				break;
			}
			else
			{
				// We can't reuse this actor. The dimensions does not match.
				// We need to rename this actor in order to create a new one with the specified name.
				FHoudiniEngineUtils::RenameToUniqueActor(OutActor, ActorName + TEXT("_old") );
				OutActor = nullptr;
			}
		}
	}

	return OutActor;
}

void
FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput(
	UHoudiniOutput* InOutput,
	TArray<FHoudiniOutputObjectIdentifier> &StaleOutputObjects,
	TSet<ALandscapeProxy*>& OutActiveLandscapes,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	const TMap<FString,FString>& OutputAttributes,
	const TMap<FString,FString>& OutputTokens,
	const FName& EditLayerName,
	ALandscape* SharedLandscapeActor,
	USceneComponent* SharedLandscapeActorParent,
	bool bCreatedMainLandscape,
	const FHoudiniOutputObjectIdentifier& Identifier,
	ALandscapeProxy* LandscapeActor,
	const EPackageMode InPackageMode)
{
	if (InPackageMode == EPackageMode::Bake)
		return SetLandscapeActorAsOutput_Bake(InOutput, InAllInputLandscapes, OutputAttributes, OutputTokens, EditLayerName, SharedLandscapeActor, SharedLandscapeActorParent, bCreatedMainLandscape, Identifier, LandscapeActor);
	else
		return SetLandscapeActorAsOutput_Temp(InOutput, StaleOutputObjects, OutActiveLandscapes, InAllInputLandscapes, OutputAttributes, OutputTokens, EditLayerName, SharedLandscapeActor, SharedLandscapeActorParent, bCreatedMainLandscape, Identifier, LandscapeActor);
}

void
FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput_Bake(
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	const TMap<FString,FString>& OutputAttributes,
	const TMap<FString,FString>& OutputTokens,
	const FName& EditLayerName,
	ALandscape* SharedLandscapeActor,
	USceneComponent* SharedLandscapeActorParent,
	bool bCreatedMainLandscape,
	const FHoudiniOutputObjectIdentifier& Identifier,
	ALandscapeProxy* LandscapeActor)
{
	// We are in bake mode. No outputs to register / add here.
	// Do nothing, for now.
}

void
FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput_Temp(
	UHoudiniOutput* InOutput,
	TArray<FHoudiniOutputObjectIdentifier> &StaleOutputObjects,
	TSet<ALandscapeProxy*>& OutActiveLandscapes,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	const TMap<FString,FString>& OutputAttributes,
	const TMap<FString,FString>& OutputTokens,
	const FName& EditLayerName,
	ALandscape* SharedLandscapeActor,
	USceneComponent* SharedLandscapeActorParent,
	bool bCreatedSharedLandscape,
	const FHoudiniOutputObjectIdentifier& Identifier,
	ALandscapeProxy* LandscapeActor)
{
	// The main landscape is a special case here. It cannot be registered with the
	// output object here, since it is possibly shared by *multiple* outputs so
	// we have to deal with the attached and cleanup of the actor manually.
	if (bCreatedSharedLandscape && IsValid(SharedLandscapeActorParent))
	{
		AttachActorToHAC(InOutput, SharedLandscapeActor);
	}

	// Since this function will be called multiple times for the same Output (but different output objects)
	// we can't perform any stale object cleanup here since we don't what the other calls are going to generate.
	// Instead, stale output tracking and cleanup is by caller of the OutputLandscape_* functions. All we're doing
	// here is removing objects from the StaleObjects array that we know for a fact is not stale.
	
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& Outputs = InOutput->GetOutputObjects();

	for (auto& Elem : Outputs)
	{
		UHoudiniLandscapePtr* LandscapePtr = nullptr;
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput_Temp] Checking output: %s"), *Elem.Key.PartName);

		if (!(Elem.Key == Identifier))
		{
			// Output identifiers doesn't match so we can't remove this from the stale objects.
			continue;
		}

		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput_Temp] Removing from stale outputs ... "));
		// Identifiers for output object match. Not stale.
		StaleOutputObjects.Remove(Elem.Key);
	}

	// Send a landscape pointer back to the Output Object for this landscape tile.
	FHoudiniOutputObject& OutputObj = InOutput->GetOutputObjects().FindOrAdd(Identifier);
	UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObj.OutputObject);

	if (LandscapePtr)
	{
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput_Temp] LandscapePtr: %s"), *LandscapePtr->GetFullName());
		// If we have a Landscape ptr, ensure the landscape actors match. Mismatching actors should be destroyed.
		ALandscapeProxy* Proxy = LandscapePtr->GetRawPtr();
		if (Proxy)
		{
			if (Proxy != LandscapeActor)
			{
				Proxy->Destroy();
				LandscapePtr->GetSoftPtr().Reset();
			}
		}
	}
	
	if (!IsValid(LandscapePtr))
	{
		// We don't have a valid landscape ptr. Create a new one.
		LandscapePtr = NewObject<UHoudiniLandscapePtr>(InOutput);
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::SetLandscapeActorAsOutput_Temp] Creating new UHoudiniLandscapePtr for identifier: %s"), *Identifier.PartName);
	}

	check(LandscapePtr);

	// Update the Landscape Ptr.
	LandscapePtr->SetSoftPtr(LandscapeActor);
	LandscapePtr->EditLayerName = EditLayerName;

	OutputObj.OutputObject = LandscapePtr;
	OutputObj.CachedAttributes = OutputAttributes;
	OutputObj.CachedTokens = OutputTokens;

	// Be sure to track the landscapes that we are using for this tile/layer to ensure
	// that they don't get deleted when stale outputs are being processed higher up the call stack.
	if (LandscapeActor)
	{
		OutActiveLandscapes.Add(LandscapeActor);
	}
	
	if (SharedLandscapeActor)
	{
		OutActiveLandscapes.Add(SharedLandscapeActor);
	}
}


bool
FHoudiniLandscapeTranslator::AttachActorToHAC(UHoudiniOutput* InOutput, AActor* InActor)
{
	UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput);
	if (IsValid(HAC))
	{
		InActor->AttachToComponent(HAC, FAttachmentTransformRules::KeepRelativeTransform);

		return true;
	}
	return false;
}


FString
FHoudiniLandscapeTranslator::GetActorNameSuffix(const EPackageMode& InPackageMode)
{
	if(InPackageMode == EPackageMode::CookToTemp)
		return "_Temp";
	else
		return FString();
}

void
FHoudiniLandscapeTranslator::DoPreEditChangeProperty(UObject* Obj, FName PropertyName)
{
	Obj->PreEditChange(FindFProperty<FProperty>(Obj->GetClass(), PropertyName));
}

void
FHoudiniLandscapeTranslator::DoPostEditChangeProperty(UObject* Obj, FName PropertyName)
{
	FPropertyChangedEvent Evt(FindFieldChecked<FProperty>(Obj->GetClass(), PropertyName));
	Obj->PostEditChangeProperty(Evt);
}

bool
FHoudiniLandscapeTranslator::ConvertHeightfieldDataToLandscapeData(
	const TArray< float >& HeightfieldFloatValues,
	const FHoudiniVolumeInfo& HeightfieldVolumeInfo,
	const int32& FinalXSize, const int32& FinalYSize,
	float FloatMin, float FloatMax,
	TArray< uint16 >& IntHeightData,
	FTransform& LandscapeTransform,
	const bool NoResize,
	const bool bOverrideZScale,
	const float CustomZScale,
	const bool bIsAdditive)
{
	IntHeightData.Empty();
	LandscapeTransform.SetIdentity();

	// HF sizes needs an X/Y swap
	// NOPE.. not anymore
	int32 HoudiniXSize = HeightfieldVolumeInfo.YLength;
	int32 HoudiniYSize = HeightfieldVolumeInfo.XLength;
	int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
	if ((HoudiniXSize < 2) || (HoudiniYSize < 2))
		return false;

	// Test for potential special cases...
	// Just print a warning for now
	if (HeightfieldVolumeInfo.MinX != 0)
		HOUDINI_LOG_WARNING(TEXT("Converting Landscape: heightfield's min X is not zero."));

	if (HeightfieldVolumeInfo.MinY != 0)
		HOUDINI_LOG_WARNING(TEXT("Converting Landscape: heightfield's min Y is not zero."));

	//--------------------------------------------------------------------------------------------------
	// 1. Convert values to uint16 using doubles to get the maximum precision during the conversion
	//--------------------------------------------------------------------------------------------------

	FTransform CurrentVolumeTransform = HeightfieldVolumeInfo.Transform;

	// Zero value for landscape texture data
	const uint16 LandscapeZeroValue = LandscapeDataAccess::GetTexHeight(0.f);

	// The ZRange in Houdini (in m)
	double MeterZRange = (double)(FloatMax - FloatMin);

	// The corresponding unreal digit range (as unreal uses uint16, max is 65535)
	// We may want to not use the full range in order to be able to sculpt the landscape past the min/max values after.
	const double dUINT16_MAX = (double)UINT16_MAX;
	double DigitZRange = 49152.0;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseFullResolution)
		DigitZRange = dUINT16_MAX - 1.0;

	// If we  are not using the full range, we need to center the digit values so the terrain can be edited up and down
	double DigitCenterOffset = 0;

	if (!bIsAdditive)
	{
		DigitCenterOffset = FMath::FloorToDouble((dUINT16_MAX - DigitZRange) / 2.0);
	}

	// The factor used to convert from Houdini's ZRange to the desired digit range
	double ZSpacing = (MeterZRange != 0.0) ? (DigitZRange / MeterZRange) : 0.0;

	// Changes these values if the user wants to loose a lot of precision
	// just to keep the same transform as the landscape input
	bool bUseDefaultUE4Scaling = false;
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
		bUseDefaultUE4Scaling = HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling;

	bUseDefaultUE4Scaling |= bOverrideZScale;
	
	if (bUseDefaultUE4Scaling)
	{
		//Check that our values are compatible with UE4's default scale values
		if (FloatMin < -256.0f || FloatMin > 256.0f || FloatMax < -256.0f || FloatMax > 256.0f)
		{
			// Warn the user that the landscape conversion will have issues 
			// invite him to change that setting
			HOUDINI_LOG_WARNING(
				TEXT("The heightfield's min and max height values are too large for being used with the \"Use Default UE4 scaling\" option.\n \
                      The generated Heightfield will likely be incorrectly converted to landscape unless you disable that option in the project settings and recook the asset."));
		}

		DigitZRange = dUINT16_MAX - 1.0;
		DigitCenterOffset = 0;

		// Default unreal landscape scaling is -256m:256m at Scale = 100, 
		// We need to apply the scale back, and swap Y/Z axis
		FloatMin = -256.0f * CurrentVolumeTransform.GetScale3D().Y * 2.0f * CustomZScale/100.f;
		FloatMax = 256.0f * CurrentVolumeTransform.GetScale3D().Y * 2.0f * CustomZScale/100.f;

		MeterZRange = (double)(FloatMax - FloatMin);

		ZSpacing = ((double)DigitZRange) / MeterZRange;
	}

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] bIsAdditive: %d"), bIsAdditive);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] CustomZScale: %f"), CustomZScale);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] DigitCenterOffset: %f"), DigitZRange);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] DigitZRange: %f"), DigitZRange);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] MeterZRange: %f"), MeterZRange);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] ZSpacing: %f"), ZSpacing);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] Volume YScale: %f"), CurrentVolumeTransform.GetScale3D().Y);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] Float Range: %f - %f"), FloatMin, FloatMax);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] Zero Value: %d"), LandscapeZeroValue);

	// Converting the data from Houdini to Unreal
	// For correct orientation in unreal, the point matrix has to be transposed.
	IntHeightData.SetNumUninitialized(SizeInPoints);
	
	int32 nUnreal = 0;
	bool bValueClippedMin = false;
	bool bValueClippedMax = false;
	
	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// Copying values X then Y in Unreal but reading them Y then X in Houdini due to swapped X/Y
			int32 nHoudini = nY + nX * HoudiniYSize;

			// Get the double values in [0 - ZRange]
			double DoubleValue = (double)HeightfieldFloatValues[nHoudini];

			// NOTE: Additive (edit) layers should not have their values offset, but they
			// should be scaled using the same zspacing values as the base layer on the target landscape.
			if (!bIsAdditive)
			{
				DoubleValue -= (double)FloatMin;
			}

			// Then convert it to [0 - DesiredRange] and center it 
			DoubleValue = DoubleValue * ZSpacing + DigitCenterOffset;

			if (bIsAdditive)
			{
				DoubleValue += LandscapeZeroValue;
			}
			bValueClippedMin = bValueClippedMin || (DoubleValue < 0);
			bValueClippedMax = bValueClippedMax || (DoubleValue >= DigitZRange);
			
			const int32 IntValue = FMath::RoundToInt(DoubleValue);
			IntHeightData[nUnreal++] = FMath::Clamp(IntValue, 0, FMath::RoundToInt(DigitZRange)); 
		}
	}

	if (bValueClippedMin)
	{
		HOUDINI_LOG_WARNING(TEXT("Heightfield values exceeded minimum range. Consider lowering `unreal_landscape_layer_min`."));
	}

	if (bValueClippedMax)
	{
		HOUDINI_LOG_WARNING(TEXT("Heightfield values exceeded range. Consider increasing `unreal_landscape_layer_max`."));
	}

	//--------------------------------------------------------------------------------------------------
	// 2. Resample / Pad the int data so that if fits unreal size requirements
	//--------------------------------------------------------------------------------------------------

	// UE has specific size requirements for landscape,
	// so we might need to pad/resample the heightfield data
	FVector LandscapeResizeFactor = FVector::OneVector;
	FVector LandscapePositionOffsetInPixels = FVector::ZeroVector;
	if (!NoResize)
	{
		// Try to resize the data
		if (!FHoudiniLandscapeTranslator::ResizeHeightDataForLandscape(
			IntHeightData,
			HoudiniXSize, HoudiniYSize, FinalXSize, FinalYSize,
			LandscapeResizeFactor, LandscapePositionOffsetInPixels))
			return false;
	}

	//--------------------------------------------------------------------------------------------------
	// 3. Calculating the proper transform for the landscape to be sized and positionned properly
	//--------------------------------------------------------------------------------------------------

	// Scale:
	// Calculating the equivalent scale to match Houdini's Terrain Size in Unreal
	FVector LandscapeScale;

	// Unreal has a X/Y resolution of 1m per point while Houdini is dependant on the heighfield's grid spacing
	// Swap Y/Z axis from H to UE
	LandscapeScale.X = CurrentVolumeTransform.GetScale3D().X * 2.0f;
	LandscapeScale.Y = CurrentVolumeTransform.GetScale3D().Z * 2.0f;

	// Calculating the Z Scale so that the Z values in Unreal are the same as in Houdini
	// Unreal has a default Z range is 512m for a scale of a 100%
	LandscapeScale.Z = (float)((double)(dUINT16_MAX / DigitZRange) * MeterZRange / 512.0);
	if (bUseDefaultUE4Scaling)
	{
		// Swap Y/Z axis from H to UE
		LandscapeScale.Z = CurrentVolumeTransform.GetScale3D().Y * 2.0f;
	}
	LandscapeScale *= 100.f;

	// If the data was resized and not expanded, we need to modify the landscape's scale
	LandscapeScale *= LandscapeResizeFactor;

	// Don't allow a zero scale, as this results in divide by 0 operations in FMatrix::InverseFast in the landscape component.
	if (FMath::IsNearlyZero(LandscapeScale.Z))
		LandscapeScale.Z = 1.0f;

	// We'll use the position from Houdini, but we will need to offset the Z Position to center the 
	// values properly as the data has been offset by the conversion to uint16
	FVector LandscapePosition = CurrentVolumeTransform.GetLocation();
	//LandscapePosition.Z = 0.0f;

	// We need to calculate the position offset so that Houdini and Unreal have the same Zero position
	// In Unreal, zero has a height value of 32768.
	// These values are then divided by 128 internally, and then multiplied by the Landscape's Z scale
	// ( DIGIT - 32768 ) / 128 * ZScale = ZOffset

	// We need the Digit (Unreal) value of Houdini's zero for the scale calculation
	// ( float and int32 are used for this because 0 might be out of the landscape Z range!
	// when using the full range, this would cause an overflow for a uint16!! )
	float HoudiniZeroValueInDigit = (float)FMath::RoundToInt((0.0 - (double)FloatMin) * ZSpacing + DigitCenterOffset);
	float ZOffset = -(HoudiniZeroValueInDigit - LandscapeZeroValue ) / 128.0f * LandscapeScale.Z;

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] Houdini Zero Digit Value: %f"), HoudiniZeroValueInDigit);
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] Z Offset: %f"), ZOffset);
	
	LandscapePosition.Z += ZOffset;
	
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[ConvertHeightfieldDataToLandscapeData] LandscapePosition.Z: %f"), LandscapePosition.Z);

	// If we have padded the data when resizing the landscape, we need to offset the position because of
	// the added values on the topLeft Corner of the Landscape
	if (LandscapePositionOffsetInPixels != FVector::ZeroVector)
	{
		FVector LandscapeOffset = LandscapePositionOffsetInPixels * LandscapeScale;
		LandscapeOffset.Z = 0.0f;

		LandscapePosition += LandscapeOffset;
	}
	
	/*
	FTransform TempTransform;
	TempTransform.SetIdentity();
	{
		// Houdini Pivot (center of the Landscape)
		FVector HoudiniPivot = FVector((FinalXSize-1) * 100.0f / 2.0f, (FinalYSize-1) * 100.0f / 2.0f, 0.0f);

		// Center the landscape
		FVector CenterLocation = LandscapePosition - HoudiniPivot;
		
		// Rotate the vector using the H rotation
		// We need to compensate for the "default" HF Transform
		FRotator Rotator = CurrentVolumeTransform.GetRotation().Rotator();
		Rotator.Yaw -= 90.0f;
		Rotator.Roll += 90.0f;
		FVector RotatedLocation = Rotator.RotateVector(CenterLocation);

		FQuat LandscapeRotation = FQuat(Rotator) * FQuat::Identity;
		
		// Return to previous origin
		FVector Uncentered = RotatedLocation + HoudiniPivot;
		TempTransform = FTransform(LandscapeRotation, Uncentered, LandscapeScale);
	}

	LandscapeTransform = TempTransform;
	*/
	
	// We can now set the Landscape position
	LandscapeTransform.SetLocation(LandscapePosition);
	LandscapeTransform.SetScale3D(LandscapeScale);

	// Rotate the vector using the H rotation	
	FRotator Rotator = CurrentVolumeTransform.GetRotation().Rotator();
	// We need to compensate for the "default" HF Transform
	Rotator.Yaw -= 90.0f;
	Rotator.Roll += 90.0f;

	// Only rotate if the rotator is far from zero
	if(!Rotator.IsNearlyZero())
		LandscapeTransform.SetRotation(FQuat(Rotator));

	return true;
}

template<typename T>
TArray<T> ResampleData(const TArray<T>& Data, int32 OldWidth, int32 OldHeight, int32 NewWidth, int32 NewHeight)
{
	TArray<T> Result;
	Result.Empty(NewWidth * NewHeight);
	Result.AddUninitialized(NewWidth * NewHeight);

	const float XScale = (float)(OldWidth - 1) / (NewWidth - 1);
	const float YScale = (float)(OldHeight - 1) / (NewHeight - 1);
	for (int32 Y = 0; Y < NewHeight; ++Y)
	{
		for (int32 X = 0; X < NewWidth; ++X)
		{
			const float OldY = Y * YScale;
			const float OldX = X * XScale;
			const int32 X0 = FMath::FloorToInt(OldX);
			const int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, OldWidth - 1);
			const int32 Y0 = FMath::FloorToInt(OldY);
			const int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, OldHeight - 1);
			const T& Original00 = Data[Y0 * OldWidth + X0];
			const T& Original10 = Data[Y0 * OldWidth + X1];
			const T& Original01 = Data[Y1 * OldWidth + X0];
			const T& Original11 = Data[Y1 * OldWidth + X1];
			Result[Y * NewWidth + X] = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
		}
	}

	return Result;
}

template<typename T>
void ExpandData(T* OutData, const T* InData,
	int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
	int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
{
	const int32 OldWidth = OldMaxX - OldMinX + 1;
	const int32 OldHeight = OldMaxY - OldMinY + 1;
	const int32 NewWidth = NewMaxX - NewMinX + 1;
	const int32 NewHeight = NewMaxY - NewMinY + 1;
	const int32 OffsetX = NewMinX - OldMinX;
	const int32 OffsetY = NewMinY - OldMinY;

	for (int32 Y = 0; Y < NewHeight; ++Y)
	{
		const int32 OldY = FMath::Clamp<int32>(Y + OffsetY, 0, OldHeight - 1);

		// Pad anything to the left
		const T PadLeft = InData[OldY * OldWidth + 0];
		for (int32 X = 0; X < -OffsetX; ++X)
		{
			OutData[Y * NewWidth + X] = PadLeft;
		}

		// Copy one row of the old data
		{
			const int32 X = FMath::Max(0, -OffsetX);
			const int32 OldX = FMath::Clamp<int32>(X + OffsetX, 0, OldWidth - 1);
			FMemory::Memcpy(&OutData[Y * NewWidth + X], &InData[OldY * OldWidth + OldX], FMath::Min<int32>(OldWidth, NewWidth) * sizeof(T));
		}

		const T PadRight = InData[OldY * OldWidth + OldWidth - 1];
		for (int32 X = -OffsetX + OldWidth; X < NewWidth; ++X)
		{
			OutData[Y * NewWidth + X] = PadRight;
		}
	}
}

template<typename T>
TArray<T> ExpandData(const TArray<T>& Data,
	int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
	int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY,
	int32* PadOffsetX = nullptr, int32* PadOffsetY = nullptr)
{
	const int32 NewWidth = NewMaxX - NewMinX + 1;
	const int32 NewHeight = NewMaxY - NewMinY + 1;

	TArray<T> Result;
	Result.Empty(NewWidth * NewHeight);
	Result.AddUninitialized(NewWidth * NewHeight);

	ExpandData(Result.GetData(), Data.GetData(),
		OldMinX, OldMinY, OldMaxX, OldMaxY,
		NewMinX, NewMinY, NewMaxX, NewMaxY);

	// Return the padding so we can offset the terrain position after
	if (PadOffsetX)
		*PadOffsetX = NewMinX;

	if (PadOffsetY)
		*PadOffsetY = NewMinY;

	return Result;
}

bool
FHoudiniLandscapeTranslator::ResizeHeightDataForLandscape(
	TArray<uint16>& HeightData,
	const int32& SizeX, const int32& SizeY,
	const int32& NewSizeX, const int32& NewSizeY,
	FVector& LandscapeResizeFactor,
	FVector& LandscapePositionOffset)
{
	LandscapeResizeFactor = FVector::OneVector;
	LandscapePositionOffset = FVector::ZeroVector;

	if (HeightData.Num() <= 4)
		return false;

	if ((SizeX < 2) || (SizeY < 2))
		return false;

	// No need to resize anything
	if (SizeX == NewSizeX && SizeY == NewSizeY)
		return true;

	// Always resample, for now. We may enable padding functionality again at some point via
	// a plugin setting.
	bool bForceResample = true;
	bool bResample = bForceResample ? true : ((NewSizeX <= SizeX) && (NewSizeY <= SizeY));

	TArray<uint16> NewData;
	if (!bResample)
	{
		// Expanding the data by padding
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);

		const int32 OffsetX = (int32)(NewSizeX - SizeX) / 2;
		const int32 OffsetY = (int32)(NewSizeY - SizeY) / 2;

		// Store the offset in pixel due to the padding
		int32 PadOffsetX = 0;
		int32 PadOffsetY = 0;

		// Expanding the Data
		NewData = ExpandData(
			HeightData, 0, 0, SizeX - 1, SizeY - 1,
			-OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1,
			&PadOffsetX, &PadOffsetY);

		// We will need to offset the landscape position due to the value added by the padding
		LandscapePositionOffset.X = (float)PadOffsetX;
		LandscapePositionOffset.Y = (float)PadOffsetY;

		// Notify the user that the data was padded
		HOUDINI_LOG_WARNING(
			TEXT("Landscape data had to be padded from ( %d x %d ) to ( %d x %d )."),
			SizeX, SizeY, NewSizeX, NewSizeY);
	}
	else
	{
		// Resampling the data
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);
		NewData = ResampleData(HeightData, SizeX, SizeY, NewSizeX, NewSizeY);

		// The landscape has been resized, we'll need to take that into account when sizing it
		LandscapeResizeFactor.X = (float)SizeX / (float)NewSizeX;
		LandscapeResizeFactor.Y = (float)SizeY / (float)NewSizeY;
		LandscapeResizeFactor.Z = 1.0f;

		// Notify the user if the heightfield data was resized
		HOUDINI_LOG_WARNING(
			TEXT("Landscape data had to be resized from ( %d x %d ) to ( %d x %d )."),
			SizeX, SizeY, NewSizeX, NewSizeY);
	}

	// Replaces Old data with the new one
	HeightData = NewData;

	return true;
}


bool 
FHoudiniLandscapeTranslator::CalcLandscapeSizeFromHeightfieldSize(
	const int32& HoudiniSizeX, const int32& HoudiniSizeY, 
	int32& UnrealSizeX, int32& UnrealSizeY, 
	int32& NumSectionsPerComponent, int32& NumQuadsPerSection)
{
	if ((HoudiniSizeX < 2) || (HoudiniSizeY < 2))
		return false;

	NumSectionsPerComponent = 1;
	NumQuadsPerSection = 1;
	UnrealSizeX = -1;
	UnrealSizeY = -1;

	// Unreal's default sizes
	int32 SectionSizes[] = { 7, 15, 31, 63, 127, 255 };
	int32 NumSections[] = { 1, 2 };

	// Component count used to calculate the final size of the landscape
	int32 ComponentsCountX = 1;
	int32 ComponentsCountY = 1;

	// Lambda for clamping the number of component in X/Y
	auto ClampLandscapeSize = [&]()
	{
		// Max size is either whole components below 8192 verts, or 32 components
		ComponentsCountX = FMath::Clamp(ComponentsCountX, 1, FMath::Min(32, FMath::FloorToInt(8191.0f / (float)(NumSectionsPerComponent * NumQuadsPerSection))));
		ComponentsCountY = FMath::Clamp(ComponentsCountY, 1, FMath::Min(32, FMath::FloorToInt(8191.0f / (float)(NumSectionsPerComponent * NumQuadsPerSection))));
	};

	// Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
	bool bFoundMatch = false;
	for (int32 SectionSizesIdx = UE_ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
	{
		for (int32 NumSectionsIdx = UE_ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
		{
			int32 ss = SectionSizes[SectionSizesIdx];
			int32 ns = NumSections[NumSectionsIdx];

			if (((HoudiniSizeX - 1) % (ss * ns)) == 0 && ((HoudiniSizeX - 1) / (ss * ns)) <= 32 &&
				((HoudiniSizeY - 1) % (ss * ns)) == 0 && ((HoudiniSizeY - 1) / (ss * ns)) <= 32)
			{
				bFoundMatch = true;
				NumQuadsPerSection = ss;
				NumSectionsPerComponent = ns;
				ComponentsCountX = (HoudiniSizeX - 1) / (ss * ns);
				ComponentsCountY = (HoudiniSizeY - 1) / (ss * ns);
				ClampLandscapeSize();
				break;
			}
		}
		if (bFoundMatch)
		{
			break;
		}
	}

	if (!bFoundMatch)
	{
		// if there was no exact match, try increasing the section size until we encompass the whole heightmap
		const int32 CurrentSectionSize = NumQuadsPerSection;
		const int32 CurrentNumSections = NumSectionsPerComponent;
		for (int32 SectionSizesIdx = 0; SectionSizesIdx < UE_ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
		{
			if (SectionSizes[SectionSizesIdx] < CurrentSectionSize)
			{
				continue;
			}

			const int32 ComponentsX = FMath::DivideAndRoundUp((HoudiniSizeX - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
			const int32 ComponentsY = FMath::DivideAndRoundUp((HoudiniSizeY - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
			if (ComponentsX <= 32 && ComponentsY <= 32)
			{
				bFoundMatch = true;
				NumQuadsPerSection = SectionSizes[SectionSizesIdx];
				ComponentsCountX = ComponentsX;
				ComponentsCountY = ComponentsY;
				ClampLandscapeSize();
				break;
			}
		}
	}

	if (!bFoundMatch)
	{
		// if the heightmap is very large, fall back to using the largest values we support
		const int32 MaxSectionSize = SectionSizes[UE_ARRAY_COUNT(SectionSizes) - 1];
		const int32 MaxNumSubSections = NumSections[UE_ARRAY_COUNT(NumSections) - 1];
		const int32 ComponentsX = FMath::DivideAndRoundUp((HoudiniSizeX - 1), MaxSectionSize * MaxNumSubSections);
		const int32 ComponentsY = FMath::DivideAndRoundUp((HoudiniSizeY - 1), MaxSectionSize * MaxNumSubSections);

		bFoundMatch = true;
		NumQuadsPerSection = MaxSectionSize;
		NumSectionsPerComponent = MaxNumSubSections;
		ComponentsCountX = ComponentsX;
		ComponentsCountY = ComponentsY;
		ClampLandscapeSize();
	}

	if (!bFoundMatch)
	{
		// Using default size just to not crash..
		UnrealSizeX = 512;
		UnrealSizeY = 512;
		NumSectionsPerComponent = 1;
		NumQuadsPerSection = 511;
		ComponentsCountX = 1;
		ComponentsCountY = 1;
	}
	else
	{
		// Calculating the desired size
		int32 QuadsPerComponent = NumSectionsPerComponent * NumQuadsPerSection;

		UnrealSizeX = ComponentsCountX * QuadsPerComponent + 1;
		UnrealSizeY = ComponentsCountY * QuadsPerComponent + 1;
	}

	return bFoundMatch;
}

const FHoudiniGeoPartObject*
FHoudiniLandscapeTranslator::GetHoudiniHeightFieldFromOutput(
	UHoudiniOutput* InOutput,
	const bool bMatchEditLayer,
	const FName& EditLayerName)
{
	if (!IsValid(InOutput))
		return nullptr;

	if (InOutput->GetHoudiniGeoPartObjects().Num() < 1)
		return nullptr;

	for (const FHoudiniGeoPartObject& HGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		if (HGPO.Type != EHoudiniPartType::Volume)
			continue;

		FHoudiniVolumeInfo CurVolumeInfo = HGPO.VolumeInfo;
		if (!CurVolumeInfo.Name.Contains("height"))
			continue;

		if (bMatchEditLayer)
		{
			if (!HGPO.bHasEditLayers)
				continue;
			FName LayerName(HGPO.VolumeLayerName);
			if (!LayerName.IsEqual(EditLayerName))
				continue;
		}

		// We're only handling single values for now
		if (CurVolumeInfo.TupleSize != 1)
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to create landscape output: the height volume has an invalide tuple size!"));
			return nullptr;
		}	

		// Terrains always have a ZSize of 1.
		if (CurVolumeInfo.ZLength != 1)
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to create landscape output: the height volume's z length is not 1!"));
			return nullptr;
		}

		// Values should be float
		if (!CurVolumeInfo.bIsFloat)
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to create landscape output, the height volume's data is not stored as floats!"));
			return nullptr;
		}

		return &HGPO;
	}

	return nullptr;
}

void 
FHoudiniLandscapeTranslator::GetHeightfieldsLayersFromOutput(
	const UHoudiniOutput* InOutput,
	const FHoudiniGeoPartObject& Heightfield,
	const bool bMatchEditLayer,
	const FName& EditLayerName,
	TArray< const FHoudiniGeoPartObject* >& FoundLayers) 
{
	FoundLayers.Empty();

	// Get node id
	HAPI_NodeId HeightFieldNodeId = Heightfield.GeoId;

	// We need the tile attribute if the height has it
	bool bParentHeightfieldHasTile = false;
	int32 HeightFieldTile = -1;
	{
		HAPI_AttributeInfo AttribInfoTile;
		FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);
		TArray< int32 > TileValues;

		FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
			HeightFieldNodeId, Heightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE,
			AttribInfoTile, TileValues, 0, HAPI_ATTROWNER_INVALID, 0, 1);

		if (AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM && TileValues.Num() > 0)
		{
			HeightFieldTile = TileValues[0];
			bParentHeightfieldHasTile = true;
		}
	}

	for (TArray< FHoudiniGeoPartObject >::TConstIterator IterLayers(InOutput->GetHoudiniGeoPartObjects()); IterLayers; ++IterLayers)
	{
		const FHoudiniGeoPartObject & HoudiniGeoPartObject = *IterLayers;

		HAPI_NodeId NodeId = HoudiniGeoPartObject.GeoId;
		if (NodeId == -1 || NodeId != HeightFieldNodeId)
			continue;

		if (bMatchEditLayer)
		{
			// If this layer does not match the incoming edit layer, ignore it.
			FString CurrentLayerName;
			FHoudiniEngineUtils::GetEditLayerName(HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, CurrentLayerName);
			const FName CurrentLayerFName(CurrentLayerName);
			if (!CurrentLayerFName.IsEqual(EditLayerName))
				continue;
		}

		if (bParentHeightfieldHasTile) 
		{
			int32 CurrentTile = -1;

			HAPI_AttributeInfo AttribInfoTile;
			FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);
			TArray<int32> TileValues;

			FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE,
				AttribInfoTile, TileValues, 0, HAPI_ATTROWNER_INVALID, 0, 1);

			if (AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM && TileValues.Num() > 0)
			{
				CurrentTile = TileValues[0];
			}

			// Does this layer come from the same tile as the height?
			if ((CurrentTile != HeightFieldTile) || (CurrentTile == -1))
				continue;
		}

		// Retrieve the VolumeInfo
		HAPI_VolumeInfo CurrentVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId, HoudiniGeoPartObject.PartId,
			&CurrentVolumeInfo))
			continue;

		// We're interesting in anything but height data
		FString CurrentVolumeName;
		FHoudiniEngineString(CurrentVolumeInfo.nameSH).ToFString(CurrentVolumeName);
		if (CurrentVolumeName.Contains("height"))
			continue;

		// We're only handling single values for now
		if (CurrentVolumeInfo.tupleSize != 1)
			continue;

		// Terrains always have a ZSize of 1.
		if (CurrentVolumeInfo.zLength != 1)
			continue;

		// Values should be float
		if (CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT)
			continue;

		FoundLayers.Add(&HoudiniGeoPartObject);
	}
}

bool FHoudiniLandscapeTranslator::GetHoudiniHeightfieldVolumeInfo(const FHoudiniGeoPartObject* HGPO, HAPI_VolumeInfo& VolumeInfo)
{
	if (HGPO->Type != EHoudiniPartType::Volume)
		return false;

	FHoudiniApi::VolumeInfo_Init(&VolumeInfo);

	HAPI_Result Result = FHoudiniApi::GetVolumeInfo(
		FHoudiniEngine::Get().GetSession(),
		HGPO->GeoId, HGPO->PartId, &VolumeInfo);

	// We're only handling single values for now
	if (VolumeInfo.tupleSize != 1)
		return false;

	// Terrains always have a ZSize of 1.
	if (VolumeInfo.zLength != 1)
		return false;

	// Values must be float
	if (VolumeInfo.storage != HAPI_STORAGETYPE_FLOAT)
		return false;

	if ((VolumeInfo.xLength < 2) || (VolumeInfo.yLength < 2))
		return false;

	return true;
}

bool 
FHoudiniLandscapeTranslator::GetHoudiniHeightfieldFloatData(const FHoudiniGeoPartObject* HGPO, TArray<float> &OutFloatArr, float &OutFloatMin, float &OutFloatMax) 
{
	OutFloatArr.Empty();
	OutFloatMin = 0.f;
	OutFloatMax = 0.f;

	HAPI_VolumeInfo VolumeInfo;
	if (!GetHoudiniHeightfieldVolumeInfo(HGPO, VolumeInfo))
		return false;
	
	const int32 SizeInPoints = VolumeInfo.xLength *  VolumeInfo.yLength;

	OutFloatArr.SetNum(SizeInPoints);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiGetHeightFieldData(
		HGPO->GeoId, HGPO->PartId, OutFloatArr), false);

	
	OutFloatMin = OutFloatArr[0];
	OutFloatMax = OutFloatMin;

	for (float NextFloatVal : OutFloatArr) 
	{
		if (NextFloatVal > OutFloatMax)
		{
			OutFloatMax = NextFloatVal;
		}
		else if (NextFloatVal < OutFloatMin)
			OutFloatMin = NextFloatVal;
	}

	return true;
}

bool
FHoudiniLandscapeTranslator::GetNonWeightBlendedLayerNames(const FHoudiniGeoPartObject& InHGPO, TArray<FString>& NonWeightBlendedLayerNames)
{
	// Check the attribute exists on primitive or detail
	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS, HAPI_ATTROWNER_PRIM))
		Owner = HAPI_ATTROWNER_PRIM;
	else if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS, HAPI_ATTROWNER_DETAIL))
		Owner = HAPI_ATTROWNER_DETAIL;
	else
		return false;

	// Get the values
	HAPI_AttributeInfo AttribInfoNonWBLayer;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNonWBLayer);
	TArray<FString> AttribValues;
	FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS, AttribInfoNonWBLayer, AttribValues, 1, Owner);

	if (AttribValues.Num() <= 0)
		return false;

	// Convert them to FString
	for (int32 Idx = 0; Idx < AttribValues.Num(); Idx++)
	{
		TArray<FString> Tokens;
		AttribValues[Idx].ParseIntoArray(Tokens, TEXT(" "), true);

		for (int32 n = 0; n < Tokens.Num(); n++)
			NonWeightBlendedLayerNames.AddUnique(Tokens[n]);
	}

	return true;
}

bool
FHoudiniLandscapeTranslator::GetIsLayerSubtractive(
	const FHoudiniGeoPartObject& InHGPO,
	int& LayerCompositeMode)
{
	if (InHGPO.VolumeName.Equals(HAPI_UNREAL_VISIBILITY_LAYER_NAME, ESearchCase::IgnoreCase))
	{
		// Cannot set composite mode for visibility layer.
		return false;
	}
	
	// Check the attribute exists on primitive or detail
	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_SUBTRACTIVE, HAPI_ATTROWNER_PRIM))
		Owner = HAPI_ATTROWNER_PRIM;
	else if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_SUBTRACTIVE, HAPI_ATTROWNER_DETAIL))
		Owner = HAPI_ATTROWNER_DETAIL;
	else
		return false;

	// Check the value
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	TArray< int32 > AttribValues;
	FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_SUBTRACTIVE, 
		AttrInfo, AttribValues, 1, Owner, 0, 1);

	if (AttribValues.Num() > 0)
	{
		LayerCompositeMode = AttribValues[0];
		return true;
	}

	return false;
}

bool
FHoudiniLandscapeTranslator::GetIsLayerWeightBlended(const FHoudiniGeoPartObject& InHGPO,
	bool& bIsLayerNoWeightBlended)
{
	if (InHGPO.VolumeName.Equals(HAPI_UNREAL_VISIBILITY_LAYER_NAME, ESearchCase::IgnoreCase))
	{
		// Cannot set weight blending for visibility layer.
		return false;
	}
	
	// Check the attribute exists on primitive or detail
	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND, HAPI_ATTROWNER_PRIM))
	{
		Owner = HAPI_ATTROWNER_PRIM;
	}
	else if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND, HAPI_ATTROWNER_DETAIL))
	{
		Owner = HAPI_ATTROWNER_DETAIL;
	}
	else
	{
		// No prim/detail attribute found.  
		return false;
	}

	// Check the value
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	TArray< int32 > AttribValues;
	FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND, 
		AttrInfo, AttribValues, 1, Owner, 0, 1);

	if (AttribValues.Num() > 0)
	{
		bIsLayerNoWeightBlended = AttribValues[0] == HAPI_UNREAL_LANDSCAPE_LAYER_NOWEIGHTBLEND_ON;
		return true;
	}

	return false;
}

bool
FHoudiniLandscapeTranslator::IsUnitLandscapeLayer(const FHoudiniGeoPartObject& LayerGeoPartObject)
{
	if (LayerGeoPartObject.VolumeName.Equals(HAPI_UNREAL_VISIBILITY_LAYER_NAME, ESearchCase::IgnoreCase))
	{
		// Visibility layers should always be treated as Unit layers.
		return true;
	}
	
	// Check the attribute exists on primitive or detail
	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(LayerGeoPartObject.GeoId, LayerGeoPartObject.PartId, HAPI_UNREAL_ATTRIB_UNIT_LANDSCAPE_LAYER, HAPI_ATTROWNER_PRIM))
		Owner = HAPI_ATTROWNER_PRIM;
	else if (FHoudiniEngineUtils::HapiCheckAttributeExists(LayerGeoPartObject.GeoId, LayerGeoPartObject.PartId, HAPI_UNREAL_ATTRIB_UNIT_LANDSCAPE_LAYER, HAPI_ATTROWNER_DETAIL))
		Owner = HAPI_ATTROWNER_DETAIL;
	else
		return false;

	// Check the value
	HAPI_AttributeInfo AttribInfoUnitLayer;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoUnitLayer);
	TArray< int32 > AttribValues;
	FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		LayerGeoPartObject.GeoId, LayerGeoPartObject.PartId, HAPI_UNREAL_ATTRIB_UNIT_LANDSCAPE_LAYER, 
		AttribInfoUnitLayer, AttribValues, 1, Owner, 0, 1);

	if (AttribValues.Num() > 0 && AttribValues[0] == 1)
		return true;

	return false;
}

bool
FHoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayerData(
	const TArray<const FHoudiniGeoPartObject*>& FoundLayers,
	const FHoudiniGeoPartObject& Heightfield,
	const int32& LandscapeXSize, const int32& LandscapeYSize,
	const TMap<FString, float>& GlobalMinimums,
	const TMap<FString, float>& GlobalMaximums,
	TArray<FLandscapeImportLayerInfo>& OutLayerInfos,
	bool bIsUpdate,
	bool bDefaultNoWeightBlending,
	const FHoudiniPackageParams& InTilePackageParams,
	const FHoudiniPackageParams& InLayerPackageParams,
	TArray<UPackage*>& OutCreatedPackages,
	TMap<FName, const FHoudiniGeoPartObject*>& OutLayerObjectMapping
	)
{
	OutLayerInfos.Empty();

	// Get the names of all non weight blended layers
	TArray<FString> NonWeightBlendedLayerNames;
	FHoudiniLandscapeTranslator::GetNonWeightBlendedLayerNames(Heightfield, NonWeightBlendedLayerNames);

	// Used for exporting layer info objects (per landscape layer)
	FHoudiniPackageParams LayerPackageParams = InLayerPackageParams;
	// Used for exporting textures (per landscape tile)
	FHoudiniPackageParams TilePackageParams = InTilePackageParams;

	// For Debugging, do we want to export layers as textures?
	bool bExportTexture = CVarHoudiniEngineExportLandscapeTextures.GetValueOnAnyThread() == 1 ? true : false;

	// Try to create all the layers
	ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;
	for (TArray<const FHoudiniGeoPartObject *>::TConstIterator IterLayers(FoundLayers); IterLayers; ++IterLayers)
	{
		const FHoudiniGeoPartObject * LayerGeoPartObject = *IterLayers;
		if (!LayerGeoPartObject)
			continue;

		if (!LayerGeoPartObject->IsValid())
			continue;

		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(LayerGeoPartObject->AssetId))
			continue;

		if (bIsUpdate && !LayerGeoPartObject->bHasGeoChanged)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayers]: Skipping geo part (no changes): %s, %s, %d, %d"),  *(LayerGeoPartObject->VolumeName), *(LayerGeoPartObject->VolumeLayerName), LayerGeoPartObject->GeoId, LayerGeoPartObject->PartId);
			continue;
		}

		TArray<float> FloatLayerData;
		float LayerMin = 0;
		float LayerMax = 0;
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayers]: Retrieving heightfield float data for geo part: %s, %s, %d, %d"),  *(LayerGeoPartObject->VolumeName), *(LayerGeoPartObject->VolumeLayerName), LayerGeoPartObject->GeoId, LayerGeoPartObject->PartId);
		if (!FHoudiniLandscapeTranslator::GetHoudiniHeightfieldFloatData(LayerGeoPartObject, FloatLayerData, LayerMin, LayerMax))
			continue;

		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayers]: Layer Min/Max: %f, %f"),  LayerMin, LayerMax);

		// NOTE: We don't want to just skip the processing of flat layers here because it is completely valid for layers to be flat but have non-zero values.
		// Even if UE culls a flat (zero) layer, it is still important for any existing layer content to be properly cleared -- if we just skip processing on this
		// layer we could end up with tiles containing stale data.
		// Also, we do not want to simply *delete* this layer because we may be drawing zero values to a subregion on a landscape (we may not be looking at the data
		// for the whole landscape layer here!!) so we cannot make any assumptions at this. Process the layer normally even if it only contains zero values. 

		// // No need to create flat layers as Unreal will remove them afterwards..
		// if (LayerMin == LayerMax)
		// {
		// 	HOUDINI_LOG_WARNING(TEXT("[FHoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayers] Ignoring flat heightfield: %s, %s (Min: %f, Max: %f)"),  *(LayerGeoPartObject->VolumeName), *(LayerGeoPartObject->VolumeLayerName), LayerMin, LayerMax);
		// 	continue;
		// }

		const FHoudiniVolumeInfo& LayerVolumeInfo = LayerGeoPartObject->VolumeInfo;

		// Get the layer's name
		FString LayerName = LayerVolumeInfo.Name;
		const FString SanitizedLayerName = ObjectTools::SanitizeObjectName(LayerName);
		const FName SanitizedLayerFName = FName(SanitizedLayerName);
		
		TilePackageParams.ObjectName = InTilePackageParams.ObjectName + TEXT("_layer_") + SanitizedLayerName;
		LayerPackageParams.ObjectName = InLayerPackageParams.ObjectName + TEXT("_layer_") + SanitizedLayerName;

		// Check if that landscape layer has been marked as unit (range in [0-1])
		if (IsUnitLandscapeLayer(*LayerGeoPartObject))
		{
			LayerMin = 0.0f;
			LayerMax = 1.0f;
		}
		else
		{
			// We want to convert the layer using the global Min/Max
			if (GlobalMaximums.Contains(LayerName))
				LayerMax = GlobalMaximums[LayerName];

			if (GlobalMinimums.Contains(LayerName))
				LayerMin = GlobalMinimums[LayerName];
		}

		if (bExportTexture)
		{
			// Create a raw texture export of the layer on this tile
			FString TextureName = TilePackageParams.ObjectName + "_raw";
			FHoudiniLandscapeTranslator::CreateUnrealTexture(
				TilePackageParams,
				TextureName,
				LayerVolumeInfo.YLength,  // Y and X inverted?? why?
				LayerVolumeInfo.XLength,
				FloatLayerData,
				LayerMin,
				LayerMax);
		}

		// Get the layer package path
		// FString LayerNameString = FString::Printf(TEXT("%s_%d"), LayerString.GetCharArray().GetData(), (int32)LayerGeoPartObject->PartId);
		// LayerNameString = UPackageTools::SanitizePackageName(LayerNameString);

		// Build an object name for the current layer
		LayerPackageParams.SplitStr = SanitizedLayerName;

		// Creating the ImportLayerInfo and LayerInfo objects
		FLandscapeImportLayerInfo ImportLayerInfo(*LayerName);


		// See if the user has assigned a layer info object via attribute
		UPackage * Package = nullptr;
		ULandscapeLayerInfoObject* LayerInfo = GetLandscapeLayerInfoForLayer(*LayerGeoPartObject, *LayerName);
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[CreateOrUpdateLandscapeLayers] GetLandscapeLayerInfoForLayer. LayerName: %s."), *(LayerName));
		bool bCreatedLayerInfo = false;
		if (!IsValid(LayerInfo))
		{
			// No assignment, try to find or create a landscape layer info object for that layer
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[CreateOrUpdateLandscapeLayers] No layer info. FindOrCreate layer info object..."));
			LayerInfo = FindOrCreateLandscapeLayerInfoObject(LayerName, LayerPackageParams.GetPackagePath(), LayerPackageParams.GetPackageName(), Package, bCreatedLayerInfo);
		}

		if (!IsValid(LayerInfo))
		{
			continue;
		}

		if (NonWeightBlendedLayerNames.Contains(LayerName) || SanitizedLayerFName.IsEqual(HAPI_UNREAL_VISIBILITY_LAYER_NAME))
		{
			// Visibility layer is, by default, non weight blended
			LayerInfo->bNoWeightBlend = true;
		}
		else
		{
			bool bNoWeightBlending = bDefaultNoWeightBlending;
			const bool bHasWeightblendAttribute = GetIsLayerWeightBlended(*LayerGeoPartObject, bNoWeightBlending);
			if (bHasWeightblendAttribute || bCreatedLayerInfo)
			{
				// Only initialize the NoWeightBlend property if layer was created or the bNoWeightBlending attribute
				// is present on the heightfield.
				LayerInfo->bNoWeightBlend = bNoWeightBlending;
			}
		}

		// Convert the float data to uint8
		// HF masks need their X/Y sizes swapped
		if (!FHoudiniLandscapeTranslator::ConvertHeightfieldLayerToLandscapeLayer(
			FloatLayerData, LayerVolumeInfo.YLength, LayerVolumeInfo.XLength,
			LayerMin, LayerMax,
			LandscapeXSize, LandscapeYSize,
			ImportLayerInfo.LayerData))
			continue;
		
		// We will store the data used to convert from Houdini values to int in the DebugColor
		// This is the only way we'll be able to reconvert those values back to their houdini equivalent afterwards...
		// R = Min, G = Max, B = Spacing, A = ?
		LayerInfo->LayerUsageDebugColor.R = LayerMin;
		LayerInfo->LayerUsageDebugColor.G = LayerMax;
		LayerInfo->LayerUsageDebugColor.B = (LayerMax - LayerMin) / 255.0f;
		LayerInfo->LayerUsageDebugColor.A = PI;

		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayers] Processing layer: %s (bDefaultNoWeightBlend: %d"), *(LayerName), bDefaultNoWeightBlending);

		if (!bIsUpdate && IsValid(Package))
		{
			// Mark the package dirty...
			Package->MarkPackageDirty();
			OutCreatedPackages.Add(Package);
		}
		
		if (bExportTexture)
		{
			// Create an export of the converted data to texture
			// FString TextureName = LayerString;
			// if (LayerGeoPartObject->VolumeTileIndex >= 0)
			// 	TextureName = TEXT("Tile") + FString::FromInt(LayerGeoPartObject->VolumeTileIndex) + TEXT("_") + LayerString;
			// TextureName += TEXT("_conv");
			
			const FString TextureName = TilePackageParams.ObjectName + TEXT("_conv");

			FHoudiniLandscapeTranslator::CreateUnrealTexture(
				TilePackageParams,
				TextureName,
				LandscapeXSize, LandscapeYSize,
				ImportLayerInfo.LayerData);
		}

		// See if there is a physical material assigned via attribute for that landscape layer
		UPhysicalMaterial* PhysMaterial = FHoudiniLandscapeTranslator::GetLandscapePhysicalMaterial(*LayerGeoPartObject);
		if (IsValid(PhysMaterial))
		{
			LayerInfo->PhysMaterial = PhysMaterial;
		}

		// Assign the layer info object to the import layer infos
		ImportLayerInfo.LayerInfo = LayerInfo;
		OutLayerInfos.Add(ImportLayerInfo);
		OutLayerObjectMapping.Add(LayerInfo->LayerName, LayerGeoPartObject);
	}

	// Autosaving the layers prevents them for being deleted with the Asset
	// Save the packages created for the LayerInfos
	// Do this only for when creating layers.
	/*
	if (!bIsUpdate)
		FEditorFileUtils::PromptForCheckoutAndSave(CreatedLandscapeLayerPackage, true, false);
	*/

	return true;
}

void
FHoudiniLandscapeTranslator::UpdateLayerProperties(
	ALandscape* TargetLandscape,
	const FLandscapeLayer* TargetLayer,
	const FLandscapeImportLayerInfo& InLayerInfo,
	const TMap<FName, const FHoudiniGeoPartObject*>& LayerObjectMapping
	)
{
	if (!IsValid(TargetLandscape))
		return;

	if (!TargetLayer)
		return;
	
	int32 TargetLayerIndex = INDEX_NONE;
	if (TargetLayer->Guid.IsValid())
	{
		TargetLayerIndex  = TargetLandscape->GetLayerIndex(TargetLayer->Name);
	}
	
	if (TargetLayerIndex == INDEX_NONE)
		return;
	
	// We have a valid target layer. Check whether we have attributes on the Houdini side that needs
	// to adjust properties on this layer.
	
	const FHoudiniGeoPartObject* LayerObject = nullptr;
	if (LayerObjectMapping.Contains(InLayerInfo.LayerName))
	{
		LayerObject = LayerObjectMapping.FindChecked(InLayerInfo.LayerName);
	}

	if (LayerObject)
	{
		// We found a layer object that we can use for attribute lookups.

		// Check whether we should set the layer compositing mode.
		int LayerCompositeMode = 0;
		if (GetIsLayerSubtractive(*LayerObject, LayerCompositeMode))
		{
			// TODO: Only set blend status if status has changed.
			const bool IsSubtractive = (LayerCompositeMode == 1);
			TargetLandscape->SetLayerSubstractiveBlendStatus(TargetLayerIndex, IsSubtractive, InLayerInfo.LayerInfo);
		}
	}
}

void
FHoudiniLandscapeTranslator::CalcHeightfieldsArrayGlobalZMinZMax(
	const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
	TMap<FString, float>& GlobalMinimums,
	TMap<FString, float>& GlobalMaximums,
	bool bShouldEmptyMaps)
{
	if (bShouldEmptyMaps)
	{
		GlobalMinimums.Empty();
		GlobalMaximums.Empty();
	}

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	TArray<float> FloatData;

	for (const FHoudiniGeoPartObject& CurrentHeightfield: InHeightfieldArray)
	{
		// Get the current Heightfield GeoPartObject
		if ( CurrentHeightfield.VolumeInfo.TupleSize != 1)
			continue;

		// Retrieve node id from geo part.
		HAPI_NodeId NodeId = CurrentHeightfield.GeoId;
		if (NodeId == -1)
			continue;

		// Retrieve the VolumeInfo
		HAPI_VolumeInfo CurrentVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId, CurrentHeightfield.PartId,
			&CurrentVolumeInfo))
			continue;

		// Retrieve the volume name.
		FString VolumeName;
		FHoudiniEngineString HoudiniEngineStringPartName(CurrentVolumeInfo.nameSH);
		HoudiniEngineStringPartName.ToFString(VolumeName);

		bool bHasMinAttr = false;
		bool bHasMaxAttr = false;

		// If this volume has an attribute defining a minimum value use it as is.
		FloatData.Empty();
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			CurrentHeightfield.GeoId, CurrentHeightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MIN,
			AttributeInfo, FloatData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (FloatData.Num() > 0)
			{
				GlobalMinimums.Add(VolumeName, FloatData[0]);
				bHasMinAttr = true;
			}
		}

		// If this volume has an attribute defining maximum value use it as is.
		FloatData.Empty();
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			CurrentHeightfield.GeoId, CurrentHeightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MAX,
			AttributeInfo, FloatData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (FloatData.Num() > 0)
			{
				GlobalMaximums.Add(VolumeName, FloatData[0]);
				bHasMaxAttr = true;
			}
		}

		if (!(bHasMinAttr && bHasMaxAttr))
		{
			// Unreal's Z values are Y in Houdini
			float ymin, ymax;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds(FHoudiniEngine::Get().GetSession(),
				NodeId, CurrentHeightfield.PartId,
				nullptr, &ymin, nullptr,
				nullptr, &ymax, nullptr,
				nullptr, nullptr, nullptr))
				continue;

		
			if (!bHasMinAttr)
			{
				// Read the global min value for this volume
				if (!GlobalMinimums.Contains(VolumeName))
				{
					GlobalMinimums.Add(VolumeName, ymin);
				}
				else
				{
					// Update the min if necessary
					if (ymin < GlobalMinimums[VolumeName])
						GlobalMinimums[VolumeName] = ymin;
				}
			}

			if (!bHasMaxAttr)
			{
				// Read the global max value for this volume
				if (!GlobalMaximums.Contains(VolumeName))
				{
					GlobalMaximums.Add(VolumeName, ymax);
				}
				else
				{
					// Update the max if necessary
					if (ymax > GlobalMaximums[VolumeName])
						GlobalMaximums[VolumeName] = ymax;
				}
			}
		}
	}
}

void 
FHoudiniLandscapeTranslator::GetLayersZMinZMax(
	const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
	TMap<FString, float>& GlobalMinimums,
	TMap<FString, float>& GlobalMaximums)

{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	TArray<float> FloatData;

	for (const FHoudiniGeoPartObject& CurrentHeightfield : InHeightfieldArray)
	{
		// Get the current Heightfield GeoPartObject
		if (CurrentHeightfield.VolumeInfo.TupleSize != 1)
			continue;

		// Retrieve node id from geo part.
		HAPI_NodeId NodeId = CurrentHeightfield.GeoId;
		if (NodeId == -1)
			continue;

		// Retrieve the VolumeInfo
		HAPI_VolumeInfo CurrentVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId, CurrentHeightfield.PartId,
			&CurrentVolumeInfo))
			continue;

		// Retrieve the volume name.
		FString VolumeName;
		FHoudiniEngineString HoudiniEngineStringPartName(CurrentVolumeInfo.nameSH);
		HoudiniEngineStringPartName.ToFString(VolumeName);

		// Read the global min value for this volume

		float MinValue;
		float MaxValue;
		bool bHasMin = false;
		bool bHasMax = false;

		if (!GlobalMinimums.Contains(VolumeName))
		{
			// Extract min value
			FloatData.Empty();
			if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
				CurrentHeightfield.GeoId, CurrentHeightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MIN,
				AttributeInfo, FloatData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
			{
				if (FloatData.Num() > 0)
				{
					MinValue = FloatData[0];
					bHasMin = true;
				}
			}
			if (!bHasMin)
			{
				if (VolumeName == TEXT("height"))
				{
					MinValue = -1000.f;
				}
				else
				{
					MinValue = 0.f;
				}
			}
			GlobalMinimums.Add(VolumeName, MinValue);
		}

		if (!GlobalMaximums.Contains(VolumeName))
		{
			// Extract max value
			FloatData.Empty();
			if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
				CurrentHeightfield.GeoId, CurrentHeightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MAX,
				AttributeInfo, FloatData, 1, HAPI_ATTROWNER_INVALID, 0, 1))
			{
				if (FloatData.Num() > 0)
				{
					MaxValue = FloatData[0];
					bHasMax = true;
				}
			}
			if (!bHasMax)
			{
				if (VolumeName == TEXT("height"))
				{
					MaxValue = 1000.f;
				}
				else
				{
					MaxValue = 1.f;
				}
			}
			GlobalMaximums.Add(VolumeName, MaxValue);
		}


		
	}
}

bool 
FHoudiniLandscapeTranslator::ConvertHeightfieldLayerToLandscapeLayer(
	const TArray<float>& FloatLayerData,
	const int32& HoudiniXSize, const int32& HoudiniYSize,
	const float& LayerMin, const float& LayerMax,
	const int32& LandscapeXSize, const int32& LandscapeYSize,
	TArray<uint8>& LayerData, const bool& NoResize)
{
	// Convert the float data to uint8
	LayerData.SetNumUninitialized(HoudiniXSize * HoudiniYSize);

	// Calculating the factor used to convert from Houdini's ZRange to [0 255]
	double LayerZRange = (LayerMax - LayerMin);
	double LayerZSpacing = (LayerZRange != 0.0) ? (255.0 / (double)(LayerZRange)) : 0.0;

	int32 nUnrealIndex = 0;
	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// Copying values X then Y in Unreal but reading them Y then X in Houdini due to swapped X/Y
			int32 nHoudini = nY + nX * HoudiniYSize;

			// Get the double values in [0 - ZRange]
			double DoubleValue = (double)FMath::Clamp(FloatLayerData[nHoudini], LayerMin, LayerMax) - (double)LayerMin;

			// Then convert it to [0 - 255]
			DoubleValue *= LayerZSpacing;

			LayerData[nUnrealIndex++] = FMath::RoundToInt(DoubleValue);
		}
	}

	// Finally, resize the data to fit with the new landscape size if needed
	if (NoResize)
		return true;

	return FHoudiniLandscapeTranslator::ResizeLayerDataForLandscape(
		LayerData, HoudiniXSize, HoudiniYSize,
		LandscapeXSize, LandscapeYSize);
}

bool
FHoudiniLandscapeTranslator::ResizeLayerDataForLandscape(
	TArray< uint8 >& LayerData,
	const int32& SizeX, const int32& SizeY,
	const int32& NewSizeX, const int32& NewSizeY)
{
	if ((NewSizeX == SizeX) && (NewSizeY == SizeY))
		return true;

	bool bForceResample = true;
	bool bResample = bForceResample ? true : ((NewSizeX <= SizeX) && (NewSizeY <= SizeY));

	TArray<uint8> NewData;
	if (!bResample)
	{
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);

		const int32 OffsetX = (int32)(NewSizeX - SizeX) / 2;
		const int32 OffsetY = (int32)(NewSizeY - SizeY) / 2;

		// Expanding the Data
		NewData = ExpandData(
			LayerData,
			0, 0, SizeX - 1, SizeY - 1,
			-OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1);
	}
	else
	{
		// Resampling the data
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);
		NewData = ResampleData(LayerData, SizeX, SizeY, NewSizeX, NewSizeY);
	}

	LayerData = NewData;

	return true;
}

ALandscapeProxy *
FHoudiniLandscapeTranslator::CreateLandscapeTileInWorld(
	const TArray< uint16 >& IntHeightData,
	const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
	const FTransform& TileTransform,
	const FIntPoint& TileLocation,
	const int32& XSize, 
	const int32& YSize,
	const int32& NumSectionPerLandscapeComponent, 
	const int32& NumQuadsPerLandscapeSection,
	UMaterialInterface* LandscapeMaterial,
	UMaterialInterface* LandscapeHoleMaterial,
	UPhysicalMaterial* LandscapePhysicalMaterial,
	const FString& LandscapeTileActorName,
	LandscapeActorType ActorType,
	ALandscape* SharedLandscapeActor,
	UWorld* InWorld,
	ULevel* InLevel,
	FHoudiniPackageParams InPackageParams,
	bool bHasEditLayers,
	const FName& EditLayerName,
	const FName& AfterLayerName)
{
	if (!IsValid(InWorld))
		return nullptr;

	// if (!IsValid(MainLandscapeActor))
	// 	return nullptr;

	if ((XSize < 2) || (YSize < 2))
		return nullptr;

	if (IntHeightData.Num() != (XSize * YSize))
		return nullptr;

	if (!GEditor)
		return nullptr;

	ALandscapeProxy* LandscapeTile = nullptr;
	UPackage *CreatedPackage = nullptr;
	
	ALandscapeStreamingProxy* CachedStreamingProxyActor = nullptr;
	ALandscape* CachedLandscapeActor = nullptr;
	
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::CreateLandscapeTileInWorld] Creating tile with layer: %s"), *(EditLayerName.ToString()));

	UWorld* NewWorld = nullptr;
	FString MapFileName;
	bool bBroadcastMaterialUpdate = false;
	bool bRenameDefaultEditLayer = false;
	//... Create landscape tile ...//
	{
		// We need to create the landscape now and assign it a new GUID so we can create the LayerInfos
		if (ActorType == LandscapeActorType::LandscapeStreamingProxy)
		{
			// Set the level to spawn in to InLevel
			FActorSpawnParameters SpawnParameters;
			if (IsValid(InLevel))
				SpawnParameters.OverrideLevel = InLevel;
			CachedStreamingProxyActor = InWorld->SpawnActor<ALandscapeStreamingProxy>(SpawnParameters);
			if (CachedStreamingProxyActor)
			{
				check(SharedLandscapeActor);
				CachedStreamingProxyActor->PreEditChange(nullptr);

				// Update landscape tile properties from the main landscape actor.
				CachedStreamingProxyActor->GetSharedProperties(SharedLandscapeActor);
				CachedStreamingProxyActor->LandscapeActor = SharedLandscapeActor;
				CachedStreamingProxyActor->bCastStaticShadow = false;

				LandscapeTile = CachedStreamingProxyActor;
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Could not spawn ALandscapeStreamingProxy with name: %s"), *(LandscapeTileActorName) );
				return nullptr;
			}
		}
		else
		{
			// Create a normal landscape actor
			// Set the level to spawn in to InLevel
			FActorSpawnParameters SpawnParameters;
			if (IsValid(InLevel))
				SpawnParameters.OverrideLevel = InLevel;
			CachedLandscapeActor = InWorld->SpawnActor<ALandscape>(SpawnParameters);
			if (CachedLandscapeActor)
			{
				CachedLandscapeActor->PreEditChange(nullptr);
				CachedLandscapeActor->SetLandscapeGuid(FGuid::NewGuid());
				CachedLandscapeActor->LandscapeMaterial = LandscapeMaterial;
				CachedLandscapeActor->LandscapeHoleMaterial = LandscapeHoleMaterial;
				CachedLandscapeActor->bCastStaticShadow = false;
				CachedLandscapeActor->bCanHaveLayersContent = bHasEditLayers;
				bBroadcastMaterialUpdate = true;
				LandscapeTile = CachedLandscapeActor;
				bRenameDefaultEditLayer = true;
			}
		}
	}


	if (!LandscapeTile)
		return nullptr;

	CachedLandscapeActor = LandscapeTile->GetLandscapeActor();

	check(CachedLandscapeActor);
	CachedLandscapeActor->bCanHaveLayersContent = bHasEditLayers;

	// Only import non-visibility layers. Visibility will be handled explicitly.
	TArray<FLandscapeImportLayerInfo> CustomImportLayerInfos;
	for (const FLandscapeImportLayerInfo& LayerInfo : ImportLayerInfos)
	{
		if (LayerInfo.LayerName.IsEqual(HAPI_UNREAL_VISIBILITY_LAYER_NAME))
			continue;
		CustomImportLayerInfos.Add(LayerInfo);
	}

	// At this point we either has a ALandscapeStreamingProxy or ALandscape actor for the landscape tile.

	// Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
	LandscapeTile->bCastStaticShadow = false;

	// TODO: Check me?
	//if (LandscapePhsyicalMaterial)
	//	LandscapeTile->DefaultPhysMaterial = LandscapePhsyicalMaterial;

	// Setting the layer type here.
	ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

	TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	HeightmapDataPerLayers.Add(FGuid(), IntHeightData);
	TArray<FLandscapeImportLayerInfo>& MaterialImportLayerInfos = MaterialLayerDataPerLayer.Add(FGuid(), CustomImportLayerInfos);

	// Before we import new tile data, remove previous tile data that will be overlapped by the new tile.
	TSet<ULandscapeComponent*> OverlappingComponents;
	const int32 DestMinX = TileLocation.X;
	const int32 DestMinY = TileLocation.Y;
	const int32 DestMaxX = TileLocation.X + XSize - 1;
	const int32 DestMaxY = TileLocation.Y + YSize - 1;

	ULandscapeInfo* LandscapeInfo = LandscapeTile->GetLandscapeInfo();

	if (LandscapeInfo)
	{
		// If there is a preexisting LandscapeInfo object, check for overlapping components.

		// Shrink bounds by 1,1 to avoid GetComponentsInRegion picking up extra components on all sides due to the overlap between components
		LandscapeInfo->GetComponentsInRegion(DestMinX+1, DestMinY+1, DestMaxX-1, DestMaxY-1, OverlappingComponents);
		TSet<ALandscapeProxy*> StaleActors;

		for (ULandscapeComponent* Component : OverlappingComponents)
		{
			// Remove the overlapped component from the LandscapeInfo and then from 
			LandscapeInfo->Modify();

			ALandscapeProxy* Proxy = Component->GetLandscapeProxy();
			if (!IsValid(Proxy))
				continue;
			check(Proxy);
			FIntRect Bounds = Proxy->GetBoundingRect();
			// If this landscape proxy has no more components left, remove it from the LandscapeInfo.
			LandscapeInfo->UnregisterActor(Proxy);
			Proxy->Destroy();
		}

		ULandscapeInfo::RecreateLandscapeInfo(InWorld, true);
	}

	ALandscape* TargetLandscape = LandscapeTile->GetLandscapeActor();
	check(TargetLandscape);
	
	if (bHasEditLayers)
	{
		int32 CurrentLayerIndex = FindOrCreateEditLayer(TargetLandscape, EditLayerName, AfterLayerName);
		SetActiveLandscapeLayer(TargetLandscape, CurrentLayerIndex);
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::CreateLandscapeTileInWorld] Setting active landscape layer: %d"), CurrentLayerIndex);
	}

	// Import tile data
	// The Import function will correctly compute the tile section locations. No need to set it explicitly.
	// TODO: Verify this with world composition!!

	bool bIsStandalone = LandscapeTile->GetLandscapeActor() == LandscapeTile;

	// We set the actor transform and absolute section base before importing heightfield data. This allows us to
	// use the correct (quad-space) blitting region without causing overlaps during import.

	// NOTE: The following SetAbsoluteSectionBase() calls are very important. This tells the Landscape system
	// where on the landscape, in quad space, a specific tile is located. This is used by various
	// mechanisms and tools that tie into the Landscape system such as World Composition and Landscape editor tools.
	// If the Section Offset for a landscape tile is wrong, it will appear in the wrong place in the World Composition
	// viewer. Worse than that, none of Landscape Editor tools will work properly because it won't be able to
	// locate the correct Landscape component when calculating the "Landscape Component Key" for the given world position.
	// HeightmapAccessor and the AlphamapAccessor as well as the FLandscapeEditDataInterface blitting functions use the
	// same underlying Landscape Component Key calculation to find the correct landscape component to draw to. So if the
	// Section Offsets are wrong ... all manner of chaos will follow.
	// It is also super important to call ULandscapeInfo::RecreateLandscapeInfo() after change a LandscapeProxy's
	// section offset in order to update the landscape's internal caches (more specifically the component keys, which
	// are based on the section offsets) otherwise component key calculations won't work correctly.

	LandscapeTile->SetActorRelativeTransform(TileTransform);
	LandscapeTile->SetAbsoluteSectionBase(TileLocation);

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::CreateLandscapeTileInWorld] Importing tile for actor: %s "), *(LandscapeTile->GetPathName()));
	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[HoudiniLandscapeTranslator::CreateLandscapeTileInWorld] Dest region: %d, %d  ->  %d, %d"), DestMinX, DestMinY, DestMaxX, DestMaxY);

	LandscapeTile->Import(
		LandscapeTile->GetLandscapeGuid(),
		DestMinX, DestMinY, DestMaxX, DestMaxY,
		NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
		HeightmapDataPerLayers, NULL,
		MaterialLayerDataPerLayer, ImportLayerType);


	if (!LandscapeInfo)
	{
		LandscapeInfo = LandscapeTile->GetLandscapeInfo();
	}

	check(LandscapeInfo);

	if (bHasEditLayers && bRenameDefaultEditLayer)
	{
		// If this is a new landscape tile, rename the default edit layer.
		// We have to do this after the Import() since the landscape layers
		// only get initialized during that call.
		FLandscapeLayer* DefaultEditLayer = TargetLandscape->GetLayer(0);
		if (DefaultEditLayer)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::CreateLandscapeTileInWorld] Updating default layer name to: %s"), *(EditLayerName.ToString()));
			DefaultEditLayer->Name = EditLayerName;
		}
	}

	// We updated the landscape tile section offsets (SetAbsoluteSectionBase) so
	// we need to recreate the landscape info for these changes reflect correctly in the ULandscapeInfo component keys.
	// Only then are we able to "blit" the new alpha data into the correct place on the landscape.

	ULandscapeInfo::RecreateLandscapeInfo(InWorld, true);
	LandscapeTile->RecreateComponentsState();

	// TODO: Verify whether we still need to manually set the LandscapeLightingLOD setting or whether
	// calling PostEditChange() will properly fix the state.
	
	// Copied straight from UE source code to avoid crash after importing the landscape:
	// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
	// < 2048x2048 -> LOD0,  >=2048x2048 -> LOD1,  >= 4096x4096 -> LOD2,  >= 8192x8192 -> LOD3
	LandscapeTile->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((XSize * YSize) / (2048 * 2048) + 1), (uint32)2);

	// ----------------------------------------------------
	// Update visibility layer
	// ----------------------------------------------------
	
	// Update the visibility mask / layer if we have any (TileImport does not update the visibility layer).
	{
		FGuid LayerGuid;
		ALandscape* LandscapeActor = LandscapeTile->GetLandscapeActor();

		if (bHasEditLayers)
		{
			const FLandscapeLayer* Layer = LandscapeActor->GetLayer(EditLayerName);
			LayerGuid = Layer->Guid;
		}

		// If we are dealing with edit layers we need Lock the landscape edit layer, i.e., if the Guid is valid.
		FScopedSetLandscapeEditingLayer Scope(LandscapeActor, LayerGuid, [=] { CachedLandscapeActor->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		
		for (auto CurLayerInfo : ImportLayerInfos)
		{
			if (CurLayerInfo.LayerInfo && CurLayerInfo.LayerName.IsEqual(HAPI_UNREAL_VISIBILITY_LAYER_NAME))
			{
				FAlphamapAccessor<false, false> AlphaAccessor(LandscapeInfo, ALandscapeProxy::VisibilityLayer);
				AlphaAccessor.SetData(DestMinX, DestMinY, DestMaxX, DestMaxY, CurLayerInfo.LayerData.GetData(), ELandscapeLayerPaintingRestriction::None);
			}
			else
			{
				ULandscapeLayerInfoObject* LayerInfo = LandscapeInfo->GetLayerInfoByName(CurLayerInfo.LayerName);
				if (!IsValid(LayerInfo))
				{
					HOUDINI_LANDSCAPE_MESSAGE(TEXT("Missing layer with: %s"), *(CurLayerInfo.LayerName.ToString()));
				}
				else
				{
					HOUDINI_LANDSCAPE_MESSAGE(TEXT("Drawing layer with normalization: %s"), *(CurLayerInfo.LayerName.ToString()));
					FAlphamapAccessor<false, false> AlphaAccessor(LandscapeInfo, LayerInfo);
					AlphaAccessor.SetData(DestMinX, DestMinY, DestMaxX, DestMaxY, CurLayerInfo.LayerData.GetData(), ELandscapeLayerPaintingRestriction::None);
				}
			}
		}
	}

	// ----------------------------------------------------
	// Rename the actor
	// ----------------------------------------------------

	// NOTE: The LandscapeProxy needs to be properly initialized before renaming (which is why the rename is taking
	// place at the end) since the rename will trigger PostEditChange and can crash if the actor has not been
	// correctly setup.
	FHoudiniEngineUtils::SafeRenameActor(LandscapeTile, LandscapeTileActorName);
	
	if (!LandscapeTile->MarkPackageDirty())
	{
		HOUDINI_LOG_WARNING(TEXT("Editor suppressed MarkPackageDirty() for landscape tile: %s"), *(LandscapeTile->GetPathName()));
	}

#if WITH_EDITOR
	GEngine->BroadcastOnActorMoved(LandscapeTile);
#endif

	return LandscapeTile;
}


void
FHoudiniLandscapeTranslator::DestroyLandscape(ALandscape* Landscape)
{
	if (!IsValid(Landscape))
		return;

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!IsValid(Info))
		return;

	TArray<ALandscapeStreamingProxy*> Proxies = Info->Proxies;
	for(ALandscapeStreamingProxy* Proxy : Proxies)
	{
		Info->UnregisterActor(Proxy);
		Proxy->Destroy();
	}
	Landscape->Destroy();
}


void
FHoudiniLandscapeTranslator::CalculateTileLocation(
	int32 NumSectionsPerComponent,
	int32 NumQuadsPerSection,
	const FTransform& TileTransformWS,
	FHoudiniLandscapeReferenceLocation& RefLoc,
	FTransform& OutLandscapeTransform,
	FIntPoint& OutTileLocation)
{	
	// IMPORTANT: Tile Location (Section Base) needs to be in multiples of the landscape component's quad size
	const int32 ComponentSize = NumSectionsPerComponent * NumQuadsPerSection;

	OutLandscapeTransform = FTransform::Identity;
	const FTransform TileSR = FTransform(TileTransformWS.GetRotation(), FVector::ZeroVector, TileTransformWS.GetScale3D());

	const FVector BaseLoc = TileSR.InverseTransformPosition(TileTransformWS.GetLocation());

	const FVector TileScale = TileTransformWS.GetScale3D();
	const float TileLocX = BaseLoc.X; // / TileScale.X;
	const float TileLocY = BaseLoc.Y; // / TileScale.Y;

	if (!RefLoc.bIsCached)
	{
		// If there is no landscape reference location yet, calculate one now.

		// We cache this tile as a reference point for the other landscape tiles so that they can calculate
		// section base offsets in a consistent manner, relative to this tile.
		const float NearestMultipleX = FMath::RoundHalfFromZero(TileLocX / ComponentSize) * ComponentSize;
		const float NearestMultipleY = FMath::RoundHalfFromZero(TileLocY / ComponentSize) * ComponentSize;

		RefLoc.SectionCoordX = FMath::RoundHalfFromZero(NearestMultipleX);
		RefLoc.SectionCoordY = FMath::RoundHalfFromZero(NearestMultipleY);
		RefLoc.TileLocationX = TileLocX;
		RefLoc.TileLocationY = TileLocY;
	}

	// Calculate the section coordinate for this tile
	const float DeltaLocX = TileLocX - RefLoc.TileLocationX;
	const float DeltaLocY = TileLocY - RefLoc.TileLocationY;
	
	const float DeltaCoordX = FMath::RoundHalfFromZero(DeltaLocX / ComponentSize) * ComponentSize;
	const float DeltaCoordY = FMath::RoundHalfFromZero(DeltaLocY / ComponentSize) * ComponentSize;
	
	OutTileLocation.X = RefLoc.SectionCoordX + DeltaCoordX;
	OutTileLocation.Y = RefLoc.SectionCoordY + DeltaCoordY;
	
	// Adjust landscape offset to compensate for tile location / section base shifting.
	if (!RefLoc.bIsCached)
	{
		FVector Offset((TileLocX - OutTileLocation.X), (TileLocY - OutTileLocation.Y), BaseLoc.Z);
		Offset = TileSR.TransformPosition(Offset);

		RefLoc.MainTransform = TileTransformWS;
		RefLoc.MainTransform.SetTranslation(Offset);
		// Reference locations are now fully cached.
		RefLoc.bIsCached = true;
	}

	OutLandscapeTransform = RefLoc.MainTransform;
}


void 
FHoudiniLandscapeTranslator::GetLandscapeMaterials(
	const FHoudiniGeoPartObject& InHeightHGPO,
	const FHoudiniPackageParams& InPackageParams,
	UMaterialInterface*& OutLandscapeMaterial,
	UMaterialInterface*& OutLandscapeHoleMaterial,
	UPhysicalMaterial*& OutLandscapePhysicalMaterial)
{
	OutLandscapeMaterial = nullptr;
	OutLandscapeHoleMaterial = nullptr;
	OutLandscapePhysicalMaterial = nullptr;

	if (InHeightHGPO.Type != EHoudiniPartType::Volume)
		return;

	TArray<FString> Materials;
	HAPI_AttributeInfo AttribMaterials;
	FHoudiniApi::AttributeInfo_Init(&AttribMaterials);

	// First, look for landscape material
	{
		FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InHeightHGPO.GeoId, InHeightHGPO.PartId,
			HAPI_UNREAL_ATTRIB_MATERIAL,
			AttribMaterials, Materials);

		bool bMaterialOverrideNeedsCreateInstance = false;
		
		// If the material attribute was not found, check the material instance attribute.
		if (!AttribMaterials.exists)
		{
			Materials.Empty();
			FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InHeightHGPO.GeoId, InHeightHGPO.PartId,
				HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE,
				AttribMaterials, Materials);

			bMaterialOverrideNeedsCreateInstance = true;
		}

		// For some reason, HF attributes come as Vertex attrib, we should check the original owner instead.. 
		//if (AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL)
		if (AttribMaterials.exists && AttribMaterials.originalOwner == HAPI_ATTROWNER_VERTEX)
		{
			HOUDINI_LOG_WARNING(TEXT("Landscape: unreal_material must be a point, primitive or detail attribute, ignoring attribute."));
			AttribMaterials.exists = false;
			Materials.Empty();
		}

		if (AttribMaterials.exists && Materials.Num() > 0)
		{
			// Load the material

			if (!bMaterialOverrideNeedsCreateInstance)
			{
				OutLandscapeMaterial = Cast<UMaterialInterface>(StaticLoadObject(
					UMaterialInterface::StaticClass(),
					nullptr, *(Materials[0]), nullptr, LOAD_NoWarn, nullptr));
			}
			else
			{
				TArray<UPackage*> MaterialAndTexturePackages;
				
				// Purposefully empty material since it is satisfied by the override parameter
				TMap<FString, UMaterialInterface*> InputAssignmentMaterials;
				TMap<FString, UMaterialInterface*> OutputAssignmentMaterials;

				if (FHoudiniMaterialTranslator::SortUniqueFaceMaterialOverridesAndCreateMaterialInstances(Materials, InHeightHGPO, InPackageParams,
					MaterialAndTexturePackages,
					InputAssignmentMaterials, OutputAssignmentMaterials,
					false))
				{
					TArray<UMaterialInterface*> Values;
					OutputAssignmentMaterials.GenerateValueArray(Values);
					if (Values.Num() > 0)
					{
						OutLandscapeMaterial = Values[0];
					}
				}
				

			}
		}
	}

	Materials.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribMaterials);

	// Then, for the hole_material
	{
		FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InHeightHGPO.GeoId, InHeightHGPO.PartId,
			HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
			AttribMaterials, Materials);

		// If the material attribute was not found, check the material instance attribute.
		if (!AttribMaterials.exists)
		{
			Materials.Empty();
			FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InHeightHGPO.GeoId, InHeightHGPO.PartId,
				HAPI_UNREAL_ATTRIB_MATERIAL_HOLE_INSTANCE,
				AttribMaterials, Materials);
		}

		// For some reason, HF attributes come as Vertex attrib, we should check the original owner instead.. 
		//if (AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL)
		if (AttribMaterials.exists && AttribMaterials.originalOwner == HAPI_ATTROWNER_VERTEX)
		{
			HOUDINI_LOG_WARNING(TEXT("Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute."));
			AttribMaterials.exists = false;
			Materials.Empty();
		}

		if (AttribMaterials.exists && Materials.Num() > 0)
		{
			// Load the material
			OutLandscapeHoleMaterial = Cast< UMaterialInterface >(StaticLoadObject(
				UMaterialInterface::StaticClass(),
				nullptr, *(Materials[0]), nullptr, LOAD_NoWarn, nullptr));
		}
	}

	// Then for the physical material
	OutLandscapePhysicalMaterial = GetLandscapePhysicalMaterial(InHeightHGPO);
}

// Read the landscape component extent attribute from a heightfield
bool
FHoudiniLandscapeTranslator::GetLandscapeComponentExtentAttributes(
		const FHoudiniGeoPartObject& HoudiniGeoPartObject,
		int32& MinX, int32& MaxX,
		int32& MinY, int32& MaxY)
{
	// If we dont have minX, we likely dont have the others too
	if (!FHoudiniEngineUtils::HapiCheckAttributeExists(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_min_X", HAPI_ATTROWNER_PRIM))
		return false;

	// Create an AttributeInfo
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	// Get MinX
	TArray<int32> IntData;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_min_X",
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		return false;

	if (IntData.Num() > 0)
		MinX = IntData[0];

	// Get MaxX
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_max_X",
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		return false;

	if (IntData.Num() > 0)
		MaxX = IntData[0];

	// Get MinY
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_min_Y",
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		return false;

	if (IntData.Num() > 0)
		MinY = IntData[0];

	// Get MaxX
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_max_Y",
		AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		return false;

	if (IntData.Num() > 0)
		MaxY = IntData[0];

	return true;
}

ULandscapeLayerInfoObject *
FHoudiniLandscapeTranslator::FindOrCreateLandscapeLayerInfoObject(const FString& InLayerName, const FString& InPackagePath, const FString& InPackageName, UPackage*& OutPackage, bool& bCreatedPackage)
{
	FString PackageFullName = InPackagePath + TEXT("/") + InPackageName;

	// See if package exists, if it does, reuse it
	OutPackage = FindPackage(nullptr, *PackageFullName);
	if (!IsValid(OutPackage))
	{
		// We need to create a new package
		OutPackage = CreatePackage( *PackageFullName);
		bCreatedPackage = true;
	}

	if (!IsValid(OutPackage))
		return nullptr;

	if (!OutPackage->IsFullyLoaded())
		OutPackage->FullyLoad();

	HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::FindOrCreateLandscapeLayerInfoObject] Find LayerInfoObject package: %s"), *(InPackageName));

	ULandscapeLayerInfoObject* LayerInfo = nullptr;
	if (!bCreatedPackage)
	{
		// See if we can load the layer info instead of creating a new one
		LayerInfo = (ULandscapeLayerInfoObject*)StaticFindObjectFast(ULandscapeLayerInfoObject::StaticClass(), OutPackage, FName(*InPackageName));
	}

	if (!IsValid(LayerInfo))
	{
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FHoudiniLandscapeTranslator::FindOrCreateLandscapeLayerInfoObject] Could not find layer info. Creating new layer info package: %s"), *(InPackageName));
		// Create a new LandscapeLayerInfoObject in the package
		LayerInfo = NewObject<ULandscapeLayerInfoObject>(OutPackage, FName(*InPackageName), RF_Public | RF_Standalone /*| RF_Transactional*/);

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(LayerInfo);
	}

	if (IsValid(LayerInfo))
	{
		LayerInfo->LayerName = FName(*InLayerName);

		// Trigger update of the Layer Info
		LayerInfo->PreEditChange(nullptr);
		LayerInfo->PostEditChange();
		LayerInfo->MarkPackageDirty();

		// Mark the package dirty...
		OutPackage->MarkPackageDirty();
	}

	return LayerInfo;
}

bool 
FHoudiniLandscapeTranslator::CalcHeightGlobalZminZMax(
	const TArray<UHoudiniOutput*>& AllOutputs, float& OutGlobalMin, float& OutGlobalMax) 
{
	OutGlobalMin = 0.f;
	OutGlobalMax = 0.f;

	for (const auto& CurrentOutput : AllOutputs)
	{
		if (!CurrentOutput)
			continue;

		if (CurrentOutput->GetType() != EHoudiniOutputType::Landscape)
			continue;

		const TArray<FHoudiniGeoPartObject>& HGPOs = CurrentOutput->GetHoudiniGeoPartObjects();
		for (const FHoudiniGeoPartObject& CurrentHGPO : HGPOs) 
		{
			if (CurrentHGPO.Type != EHoudiniPartType::Volume)
				continue;

			if (!CurrentHGPO.VolumeInfo.Name.Contains("height"))
				continue;

			// We're only handling single values for now
			if (CurrentHGPO.VolumeInfo.TupleSize != 1)
				continue;

			// Terrains always have a ZSize of 1.
			if (CurrentHGPO.VolumeInfo.ZLength != 1)
				continue;

			// Values should be float
			if (!CurrentHGPO.VolumeInfo.bIsFloat)
				continue;

			if (!FHoudiniEngineUtils::IsHoudiniNodeValid(CurrentHGPO.GeoId))
				continue;

			// Retrieve the VolumeInfo
			HAPI_VolumeInfo CurrentVolumeInfo;
			FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
				FHoudiniEngine::Get().GetSession(),
				CurrentHGPO.GeoId, CurrentHGPO.PartId, &CurrentVolumeInfo))
				continue;

			// Unreal's Z values are Y in Houdini
			float yMin = OutGlobalMin, yMax = OutGlobalMax;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds(FHoudiniEngine::Get().GetSession(),
				CurrentHGPO.GeoId, CurrentHGPO.PartId,
				nullptr, &yMin, nullptr,
				nullptr, &yMax, nullptr,
				nullptr, nullptr, nullptr))
				continue;

			if (yMin < OutGlobalMin)
				OutGlobalMin = yMin;

			if (yMax > OutGlobalMax)
				OutGlobalMax = yMax;
		}

		if (OutGlobalMin > OutGlobalMax)
		{
			OutGlobalMin = 0.f;
			OutGlobalMax = 0.f;
		}
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::EnableWorldComposition()
{
	HOUDINI_LOG_WARNING(TEXT("[FHoudiniLandscapeTranslator::EnableWorldComposition] We should never enable world composition from within the plugin."));
	// Get the world
	UWorld* MyWorld = nullptr;
	{
		// We want to create the landscape in the landscape editor mode's world
		FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
		MyWorld = EditorWorldContext.World();
	}

	if (!MyWorld)
		return false;

	ULevel* CurrentLevel = MyWorld->GetCurrentLevel();

	if (!CurrentLevel)
		return false;

	AWorldSettings* WorldSettings = CurrentLevel->GetWorldSettings();
	if (!WorldSettings)
		return false;
	
	// Enable world composition in WorldSettings
	WorldSettings->bEnableWorldComposition = true;

	CurrentLevel->PostEditChange();
	
	return true;
}

bool
FHoudiniLandscapeTranslator::UpdateGenericPropertiesAttributes(
	UObject* InObject, const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes)
{
	if (!IsValid(InObject))
		return false;

	// Iterate over the found Property attributes
	int32 NumSuccess = 0;
	for (const auto& CurrentPropAttribute : InAllPropertyAttributes)
	{
		// Update the current Property Attribute
		if (!FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(InObject, CurrentPropAttribute))
			continue;

		// Success!
		NumSuccess++;
		FString ClassName = InObject->GetClass() ? InObject->GetClass()->GetName() : TEXT("Object");
		FString ObjectName = InObject->GetName();
		HOUDINI_LOG_MESSAGE(TEXT("Modified UProperty %s on %s named %s"), *CurrentPropAttribute.AttributeName, *ClassName, *ObjectName);
	}

	return (NumSuccess > 0);
}


bool
FHoudiniLandscapeTranslator::BackupLandscapeToImageFiles(const FString& BaseName, ALandscapeProxy* Landscape)
{
	// We need to cache the input landscape to a file    
	if (!Landscape)
		return false;

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Save Height data to file
	//FString HeightSave = TEXT("/Game/HoudiniEngine/Temp/HeightCache.png");    
	FString HeightSave = BaseName + TEXT("_height.png");
	LandscapeInfo->ExportHeightmap(HeightSave);
	Landscape->ReimportHeightmapFilePath = HeightSave;

	// Save each layer to a file
	for (int LayerIndex = 0; LayerIndex < LandscapeInfo->Layers.Num(); LayerIndex++)
	{
		FName CurrentLayerName = LandscapeInfo->Layers[LayerIndex].GetLayerName();
		//ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->GetLayerInfoByName(CurrentLayerName, Landscape);
		ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->Layers[LayerIndex].LayerInfoObj;
		if (!IsValid(CurrentLayerInfo))
			continue;

		FString LayerSave = BaseName + CurrentLayerName.ToString() + TEXT(".png");
		LandscapeInfo->ExportLayer(CurrentLayerInfo, LayerSave);

		// Update the file reimport path on the input landscape for this layer
		LandscapeInfo->GetLayerEditorSettings(CurrentLayerInfo).ReimportLayerFilePath = LayerSave;
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::RestoreLandscapeFromImageFiles(ALandscapeProxy* LandscapeProxy)
{
	if (!LandscapeProxy)
		return false;

	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Restore Height data from the backup file
	FString ReimportFile = LandscapeProxy->ReimportHeightmapFilePath;
	if (!FHoudiniLandscapeTranslator::ImportLandscapeData(LandscapeInfo, ReimportFile, TEXT("height")))
		HOUDINI_LOG_ERROR(TEXT("Could not restore the landscape actor's source height data."));

	// Restore each layer from the backup file
	TArray< ULandscapeLayerInfoObject* > SourceLayers;
	for (int LayerIndex = 0; LayerIndex < LandscapeProxy->EditorLayerSettings.Num(); LayerIndex++)
	{
		ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeProxy->EditorLayerSettings[LayerIndex].LayerInfoObj;
		if (!IsValid(CurrentLayerInfo))
			continue;

		FString CurrentLayerName = CurrentLayerInfo->LayerName.ToString();
		ReimportFile = LandscapeProxy->EditorLayerSettings[LayerIndex].ReimportLayerFilePath;

		if (!FHoudiniLandscapeTranslator::ImportLandscapeData(LandscapeInfo, ReimportFile, CurrentLayerName, CurrentLayerInfo))
			HOUDINI_LOG_ERROR(TEXT("Could not restore the landscape actor's source height data."));

		SourceLayers.Add(CurrentLayerInfo);
	}

	// Iterate on the landscape info's layer to remove any layer that could have been added by Houdini
	for (int LayerIndex = 0; LayerIndex < LandscapeInfo->Layers.Num(); LayerIndex++)
	{
		ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->Layers[LayerIndex].LayerInfoObj;
		if (SourceLayers.Contains(CurrentLayerInfo))
			continue;

		// Delete the added layer
		FName LayerName = LandscapeInfo->Layers[LayerIndex].LayerName;
		LandscapeInfo->DeleteLayer(CurrentLayerInfo, LayerName);
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::ImportLandscapeData(
	ULandscapeInfo* LandscapeInfo, const FString& Filename, const FString& LayerName, ULandscapeLayerInfoObject* LayerInfoObject)
{
	//
	// Code copied/edited from FEdModeLandscape::ImportData as we cannot access that function
	//
	if (!LandscapeInfo)
		return false;

	bool IsHeight = LayerName.Equals(TEXT("height"), ESearchCase::IgnoreCase);

	int32 MinX, MinY, MaxX, MaxY;
	if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		const FLandscapeFileResolution LandscapeResolution = { (uint32)(1 + MaxX - MinX), (uint32)(1 + MaxY - MinY) };

		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");

		if (IsHeight)
		{
			const ILandscapeHeightmapFileFormat* HeightmapFormat = LandscapeEditorModule.GetHeightmapFormatByExtension(*FPaths::GetExtension(Filename, true));

			if (!HeightmapFormat)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, File type not recognised"), *LayerName);
				return false;
			}

			FLandscapeFileResolution ImportResolution = { 0, 0 };

			const FLandscapeHeightmapInfo HeightmapInfo = HeightmapFormat->Validate(*Filename);

			// display error message if there is one, and abort the import
			if (HeightmapInfo.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, %s"), *LayerName, *(HeightmapInfo.ErrorMessage.ToString()));
				return false;
			}

			// if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
			if (HeightmapInfo.PossibleResolutions.Num() > 1)
			{
				if (!HeightmapInfo.PossibleResolutions.Contains(LandscapeResolution))
				{
					HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. The heightmap file does not match the Landscape extent and its exact resolution could not be determined"));
					return false;
				}
				else
				{
					ImportResolution = LandscapeResolution;
				}
			}

			// display warning message if there is one and allow user to cancel
			if (HeightmapInfo.ResultCode == ELandscapeImportResult::Warning)
				HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. %s"), *LayerName, *(HeightmapInfo.ErrorMessage.ToString()));

			// if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
			// unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
			if (HeightmapInfo.PossibleResolutions.Num() == 1)
			{
				ImportResolution = HeightmapInfo.PossibleResolutions[0];
				if (ImportResolution != LandscapeResolution)
					HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. The heightmap file's size does not match the current Landscape extent, data will be padded/clipped"), *LayerName);
			}

			FLandscapeHeightmapImportData ImportData = HeightmapFormat->Import(*Filename, ImportResolution);
			if (ImportData.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. %s"), *LayerName, *(ImportData.ErrorMessage.ToString()));
				return false;
			}

			TArray<uint16> Data;
			if (ImportResolution != LandscapeResolution)
			{
				// Cloned from FLandscapeEditorDetailCustomization_NewLandscape.OnCreateButtonClicked
				// so that reimports behave the same as the initial import :)

				const int32 OffsetX = (int32)(LandscapeResolution.Width - ImportResolution.Width) / 2;
				const int32 OffsetY = (int32)(LandscapeResolution.Height - ImportResolution.Height) / 2;

				Data.SetNumUninitialized(LandscapeResolution.Width * LandscapeResolution.Height * sizeof(uint16));

				ExpandData<uint16>(Data.GetData(), ImportData.Data.GetData(),
					0, 0, ImportResolution.Width - 1, ImportResolution.Height - 1,
					-OffsetX, -OffsetY, LandscapeResolution.Width - OffsetX - 1, LandscapeResolution.Height - OffsetY - 1);
			}
			else
			{
				Data = MoveTemp(ImportData.Data);
			}

			//FScopedTransaction Transaction(TEXT("Undo_ImportHeightmap", "Importing Landscape Heightmap"));

			FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
			HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData());
		}
		else
		{
			// We're importing a Landscape layer
			if (!IsValid(LayerInfoObject))
				return false;

			const ILandscapeWeightmapFileFormat* WeightmapFormat = LandscapeEditorModule.GetWeightmapFormatByExtension(*FPaths::GetExtension(Filename, true));
			if (!WeightmapFormat)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, File type not recognised"), *LayerName);
				return false;
			}

			FLandscapeFileResolution ImportResolution = { 0, 0 };

			const FLandscapeWeightmapInfo WeightmapInfo = WeightmapFormat->Validate(*Filename, FName(*LayerName));

			// display error message if there is one, and abort the import
			if (WeightmapInfo.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, %s"), *LayerName, *(WeightmapInfo.ErrorMessage.ToString()));
				return false;
			}

			// if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
			if (WeightmapInfo.PossibleResolutions.Num() > 1)
			{
				if (!WeightmapInfo.PossibleResolutions.Contains(LandscapeResolution))
				{
					HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. The weightmap file does not match the Landscape extent and its exact resolution could not be determined"));
					return false;
				}
				else
				{
					ImportResolution = LandscapeResolution;
				}
			}

			// display warning message if there is one and allow user to cancel
			if (WeightmapInfo.ResultCode == ELandscapeImportResult::Warning)
				HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. %s"), *LayerName, *(WeightmapInfo.ErrorMessage.ToString()));

			// if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
			// unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
			if (WeightmapInfo.PossibleResolutions.Num() == 1)
			{
				ImportResolution = WeightmapInfo.PossibleResolutions[0];
				if (ImportResolution != LandscapeResolution)
					HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. The heightmap file's size does not match the current Landscape extent, data will be padded/clipped"), *LayerName);
			}

			FLandscapeWeightmapImportData ImportData = WeightmapFormat->Import(*Filename, FName(*LayerName), ImportResolution);

			if (ImportData.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. %s"), *LayerName, *(ImportData.ErrorMessage.ToString()));
				return false;
			}

			TArray<uint8> Data;
			if (ImportResolution != LandscapeResolution)
			{
				// Cloned from FLandscapeEditorDetailCustomization_NewLandscape.OnCreateButtonClicked
				// so that reimports behave the same as the initial import :)
				const int32 OffsetX = (int32)(LandscapeResolution.Width - ImportResolution.Width) / 2;
				const int32 OffsetY = (int32)(LandscapeResolution.Height - ImportResolution.Height) / 2;

				Data.SetNumUninitialized(LandscapeResolution.Width * LandscapeResolution.Height * sizeof(uint8));

				ExpandData<uint8>(Data.GetData(), ImportData.Data.GetData(),
					0, 0, ImportResolution.Width - 1, ImportResolution.Height - 1,
					-OffsetX, -OffsetY, LandscapeResolution.Width - OffsetX - 1, LandscapeResolution.Height - OffsetY - 1);
			}
			else
			{
				Data = MoveTemp(ImportData.Data);
			}

			//FScopedTransaction Transaction(LOCTEXT("Undo_ImportWeightmap", "Importing Landscape Layer"));
			FAlphamapAccessor<false, false> AlphamapAccessor(LandscapeInfo, LayerInfoObject);
			AlphamapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData(), ELandscapeLayerPaintingRestriction::None);
		}
	}

	return true;
}

UTexture2D*
FHoudiniLandscapeTranslator::CreateUnrealTexture(
	const FHoudiniPackageParams& InPackageParams,
	const FString& LayerName,
	const int32& InXSize,
	const int32& InYSize,
	const TArray<float>& InFloatBuffer,
	const float& InMin,
	const float& InMax)
{

	// Convert the float values to uint8
	double Range = (double)InMax - (double)InMin;
	TArray<uint8> IntBuffer;
	IntBuffer.SetNum(InFloatBuffer.Num());
	for(int32 i = 0; i < InFloatBuffer.Num(); i++)
	{
		double dNormalizedValue = ((double)InFloatBuffer[i] - (double)InMin) / (double)Range;
		IntBuffer[i] = (uint8)(dNormalizedValue * 255.0);
	}

	return FHoudiniLandscapeTranslator::CreateUnrealTexture(
		InPackageParams, LayerName, InXSize, InYSize, IntBuffer);
}

UTexture2D*
FHoudiniLandscapeTranslator::CreateUnrealTexture(
	const FHoudiniPackageParams& InPackageParams,
	const FString& LayerName, 
	const int32& InXSize,
	const int32& InYSize,
	const TArray<uint8>& IntBuffer)
{
	FHoudiniPackageParams MyPackageParams = InPackageParams;
	MyPackageParams.ObjectName = LayerName;
	MyPackageParams.PackageMode = EPackageMode::CookToTemp;
	MyPackageParams.ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;

	FString CreatedPackageName;
	UPackage* Package = MyPackageParams.CreatePackageForObject(CreatedPackageName);
	if (!IsValid(Package))
		return nullptr;

	// Create new texture object.
	UTexture2D * Texture = NewObject<UTexture2D>(Package, UTexture2D::StaticClass(), *LayerName, RF_Public | RF_Standalone);

	// Add/Update meta information to package.
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *LayerName);

	/*// Texture Settings
	Texture->PlatformData = new FTexturePlatformData();
	Texture->PlatformData->SizeX = InXSize;
	Texture->PlatformData->SizeY = InYSize;
	Texture->PlatformData->PixelFormat = PF_R8G8B8A8;*/

	// Initialize texture source.
	Texture->Source.Init(InXSize, InYSize, 1, 1, TSF_BGRA8);

	// Lock the texture.
	uint8 * MipData = Texture->Source.LockMip(0);

	// Create base map.
	uint8* DestPtr = nullptr;
	uint32 SrcWidth = InXSize;
	uint32 SrcHeight = InYSize;
	const uint8 * SrcData = &IntBuffer[0];

	for (uint32 y = 0; y < SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];

		for (uint32 x = 0; x < SrcWidth; x++)
		{
			uint32 DataOffset = y * SrcWidth + x;

			*DestPtr++ = *(SrcData + DataOffset); // B greyscale, same value 3 times
			*DestPtr++ = *(SrcData + DataOffset); // G
			*DestPtr++ = *(SrcData + DataOffset); // R

			*DestPtr++ = 0xFF; // A to 1
		}
	}

	// Unlock the texture.
	Texture->Source.UnlockMip(0);

	// Texture creation parameters.
	//Texture->SRGB = TextureParameters.bSRGB;
	Texture->CompressionSettings = TC_Grayscale;
	Texture->CompressionNoAlpha = true;
	Texture->MipGenSettings = TMGS_NoMipmaps;

	// Set the Source Guid/Hash if specified.
	/*
	if ( TextureParameters.SourceGuidHash.IsValid() )
	{
		Texture->Source.SetId( TextureParameters.SourceGuidHash, true );
	}
	*/

	// Updating Texture & mark it as unsaved
	//Texture->AddToRoot();
	//Texture->UpdateResource();
	Package->MarkPackageDirty();

	Texture->PostEditChange();

	FString PathName = Texture->GetPathName();	
	HOUDINI_LOG_MESSAGE(TEXT("Created texture when for %s in %s"), *LayerName, *PathName);

	return Texture;
}

UPhysicalMaterial*
FHoudiniLandscapeTranslator::GetLandscapePhysicalMaterial(const FHoudiniGeoPartObject& InLayerHGPO)
{
	// See if we have assigned a physical material to this layer via attribute
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	TArray<FString> AttributeValues;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InLayerHGPO.GeoId, InLayerHGPO.PartId, HAPI_UNREAL_ATTRIB_PHYSICAL_MATERIAL,
		AttributeInfo, AttributeValues, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		return nullptr;

	if (AttributeValues.Num() > 0)
	{
		return LoadObject<UPhysicalMaterial>(nullptr, *AttributeValues[0], nullptr, LOAD_NoWarn, nullptr);
	}

	return nullptr;
}

ULandscapeLayerInfoObject*
FHoudiniLandscapeTranslator::GetLandscapeLayerInfoForLayer(const FHoudiniGeoPartObject& InLayerHGPO, const FName& InLayerName)
{
	// See if we have assigned a landscape layer info object to this layer via attribute
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	TArray<FString> AttributeValues;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InLayerHGPO.GeoId, InLayerHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_INFO,
		AttributeInfo, AttributeValues, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		return nullptr;

	if (AttributeValues.Num() > 0)
	{
		ULandscapeLayerInfoObject* FoundLayerInfo = LoadObject<ULandscapeLayerInfoObject>(nullptr, *AttributeValues[0], nullptr, LOAD_NoWarn, nullptr);
		if (!IsValid(FoundLayerInfo))
			return nullptr;

		// The layer info's name must match this layer's name or Unreal will not like this!
		if (!FoundLayerInfo->LayerName.IsEqual(InLayerName))
		{
			FString NameStr = InLayerName.ToString();
			HOUDINI_LOG_WARNING(TEXT("Failed to use the assigned layer info object for %s by the unreal_landscape_layer_info attribute as the found layer info object's layer name does not match."), *NameStr);
		}

		return FoundLayerInfo;
	}

	return nullptr;
}

int32 FHoudiniLandscapeTranslator::FindOrCreateEditLayer(ALandscape* Landscape, FName LayerName, FName AfterLayerName)
{
	check(Landscape);
	check(Landscape->bCanHaveLayersContent);

	if (LayerName.IsNone())
	{
		return INDEX_NONE;
	}

	Landscape->SetEditingLayer(FGuid()); // Clear edit layer before adding a new one!
	int32 LayerIndex = Landscape->GetLayerIndex(LayerName);
	if (LayerIndex == INDEX_NONE)
	{
		// Create a new edit layer
		LayerIndex = Landscape->CreateLayer(LayerName);
	}
	
	if (LayerIndex == INDEX_NONE)
		return INDEX_NONE; // Something went wrong.

	if (!AfterLayerName.IsNone())
	{
		// If we have an "after layer", move the edit layer into position.
		int32 NewLayerIndex = Landscape->GetLayerIndex(AfterLayerName);
		if (NewLayerIndex != INDEX_NONE && LayerIndex != NewLayerIndex)
		{
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[OutputLandscape_EditableLayer] Moving layer from %d to %d"), LayerIndex, NewLayerIndex);
			if (NewLayerIndex < LayerIndex)
			{
				NewLayerIndex += 1;
			}
			Landscape->ReorderLayer(LayerIndex, NewLayerIndex);
			// Ensure we have the correct layer/index
			LayerIndex = Landscape->GetLayerIndex(LayerName);
		}
	}

	return LayerIndex;
}

bool FHoudiniLandscapeTranslator::SetActiveLandscapeLayer(ALandscape* Landscape, int32 LayerIndex)
{
	if (!Landscape->bCanHaveLayersContent)
		return false;

	if (LayerIndex == INDEX_NONE)
	{
		// If the edit layer could not be found, or created, revert to the 'default' layer.
		LayerIndex = 0;
	}
	
	FLandscapeLayer* Layer = Landscape->GetLayer(LayerIndex);
	if (Layer)
	{
		HOUDINI_LANDSCAPE_MESSAGE(TEXT("[SetActiveLandscapeLayer] Set Editing Layer from index: %d"), LayerIndex);
		Landscape->SetEditingLayer(Layer->Guid);
		return true;
	}

	return false;
}


