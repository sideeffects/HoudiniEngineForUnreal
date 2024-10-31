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
#include "HoudiniPackageParams.h"
#include "HoudiniInput.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "HoudiniLandscapeUtils.h"
#include "AssetToolsModule.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineTimers.h"
#include "Misc/Guid.h"
#include "Engine/LevelBounds.h"
#include "HAL/IConsoleManager.h"
#include "Engine/AssetManager.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "Async/ParallelFor.h"
#if WITH_EDITOR
	#include "EditorLevelUtils.h"
#endif

typedef FHoudiniEngineUtils FHUtils;

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

HOUDINI_LANDSCAPE_DEFINE_LOG_CATEGORY();

bool
FHoudiniLandscapeTranslator::ProcessLandscapeOutput(
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	const FString& CoookedLandscapeActorPrefix,
	UWorld* InWorld, // Persistent / root world for the landscape
	const FHoudiniPackageParams & InPackageParams,
	TMap<FString, ALandscape*>& LandscapeMap,
	FHoudiniClearedEditLayers& ClearedLayers,
	TArray<UPackage*>& OutCreatedPackages)
{
	H_SCOPED_FUNCTION_TIMER();

	UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput);

	//------------------------------------------------------------------------------------------------------------------------------
	// Get a list of layers to update from HDA
	//------------------------------------------------------------------------------------------------------------------------------

	TArray<FHoudiniHeightFieldPartData> Parts = GetPartsToTranslate(InOutput);

	//------------------------------------------------------------------------------------------------------------------------------
	// Remove any layers from last cook.
	//------------------------------------------------------------------------------------------------------------------------------

	FHoudiniLandscapeRuntimeUtils::DeleteLandscapeCookedData(InOutput);

	//------------------------------------------------------------------------------------------------------------------------------
	// Resolve landscape Actors. This means we either find existing landscapes, if modifying landscapes, or create new landscapes
	// as needed. Most landscape operations do not create new packages, however Target Layers do.
	//------------------------------------------------------------------------------------------------------------------------------

	FHoudiniLayersToUnrealLandscapeMapping LandscapeMapping = FHoudiniLandscapeUtils::ResolveLandscapes(
			CoookedLandscapeActorPrefix,
			InPackageParams, 
			HAC,
			LandscapeMap,
			Parts, 
			InWorld, 
			InAllInputLandscapes);

	OutCreatedPackages += LandscapeMapping.CreatedPackages;

	//------------------------------------------------------------------------------------------------------------------------------
	// Process each layer, cooking to a temporary object.
	//------------------------------------------------------------------------------------------------------------------------------

	TArray<UHoudiniLandscapeTargetLayerOutput*> AllOutputs;

	for (FHoudiniHeightFieldPartData& Part : Parts)
	{
		if (!LandscapeMapping.HoudiniLayerToUnrealLandscape.Contains(&Part))
		{
			HOUDINI_LOG_WARNING(TEXT("Part was ignored: %s"), *Part.TargetLayerName);
			continue;
		}

		int Index = LandscapeMapping.HoudiniLayerToUnrealLandscape[&Part];
		FHoudiniUnrealLandscapeTarget& Landscape = LandscapeMapping.TargetLandscapes[Index];
		UHoudiniLandscapeTargetLayerOutput* Result = TranslateHeightFieldPart(InOutput, Landscape, Part, *HAC, ClearedLayers, InPackageParams);
		if (!Result)
			continue;
		AllOutputs.Add(Result);

		FHoudiniOutputObjectIdentifier OutputObjectIdentifier(Part.HeightField->ObjectId, Part.HeightField->GeoId, Part.HeightField->PartId, "EditableLayer");
		FHoudiniOutputObject& OutputObj = InOutput->GetOutputObjects().FindOrAdd(OutputObjectIdentifier);
		OutputObj.OutputObject = Result;

		// Hide baked layer, make cooked layer visible, if using temporary landscape layers
		if (Result->BakedEditLayer != Result->CookedEditLayer)
		{
			int EditLayerIndex = Result->Landscape->GetLayerIndex(FName(Result->BakedEditLayer));
			if (EditLayerIndex != INDEX_NONE)
				Result->Landscape->SetLayerVisibility(EditLayerIndex, false);

			EditLayerIndex = Result->Landscape->GetLayerIndex(FName(Result->CookedEditLayer));
			if (EditLayerIndex != INDEX_NONE)
				Result->Landscape->SetLayerVisibility(EditLayerIndex, true);
		}

		OutputObj.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, Part.BakeOutlinerFolder);
	}


	// ------------------------------------------------------------------------------------------------------------------
	// Once done with processing parts, lock layers
	// ------------------------------------------------------------------------------------------------------------------

	for (UHoudiniLandscapeTargetLayerOutput* Output : AllOutputs)
	{
		FHoudiniLandscapeUtils::ApplyLocks(Output);
	}

	return true;
}


TArray<FHoudiniHeightFieldPartData> FHoudiniLandscapeTranslator::GetPartsToTranslate(UHoudiniOutput* InOutput)
{
	TArray<FHoudiniHeightFieldPartData> Results;
	const TArray<FHoudiniGeoPartObject>& GeoObjects = InOutput->GetHoudiniGeoPartObjects();
	for (const FHoudiniGeoPartObject& PartObj : GeoObjects)
	{
		if (PartObj.Type != EHoudiniPartType::Volume)
			continue;

		FHoudiniHeightFieldPartData PartData;
		PartData.ObjectId = PartObj.ObjectId;
		PartData.GeoId = PartObj.GeoId;
		PartData.PartId = PartObj.PartId;
		PartData.UnrealLayerName = PartObj.VolumeLayerName;
		PartData.TargetLayerName = PartObj.VolumeName;
		PartData.HeightField = &PartObj;

		// If no  layer was specified, set the name to the default "Layer"
		if (PartData.UnrealLayerName.IsEmpty())
		{
			PartData.UnrealLayerName = TEXT("Layer");
		}

		//
		// Read attributes.
		//

		// Get Edit layer type.
		PartData.EditLayerType = HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_BASE;
		FHoudiniHapiAccessor Accessor;
		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TYPE);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.EditLayerType);

		//-----------------------------------------------------------------------------------------------------------------------------
		// Get clear type.
		//-----------------------------------------------------------------------------------------------------------------------------

		int ClearLayer = 0;
		Accessor.Init(PartObj.GeoId, PartObj.PartId,HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, ClearLayer);
		PartData.bClearLayer = (ClearLayer == 1);


		//-----------------------------------------------------------------------------------------------------------------------------
		// Treat as Unit Data?
		//-----------------------------------------------------------------------------------------------------------------------------

		int UnitData = 0;

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_UNIT_LANDSCAPE_LAYER);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, UnitData);
		PartData.bIsUnitData = (UnitData == 1);

		//-----------------------------------------------------------------------------------------------------------------------------
		// After Layer name
		//-----------------------------------------------------------------------------------------------------------------------------

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.AfterLayerName);

		//-----------------------------------------------------------------------------------------------------------------------------
		// Output Mode
		//-----------------------------------------------------------------------------------------------------------------------------

		int LandscapeOutputMode = HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_GENERATE;
		FHoudiniLandscapeUtils::GetOutputMode(PartObj.GeoId, PartObj.PartId, HAPI_ATTROWNER_INVALID, LandscapeOutputMode);
		PartData.bCreateNewLandscape = LandscapeOutputMode == HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_GENERATE;

		//-----------------------------------------------------------------------------------------------------------------------------
		// Landscape Locking / Unlocking
		//-----------------------------------------------------------------------------------------------------------------------------

		int LockValue = 0;

		Accessor.Init(PartObj.GeoId, PartObj.PartId,HAPI_UNREAL_ATTRIB_LANDSCAPE_WRITE_LOCKED_LAYERS);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, LockValue);
		PartData.bWriteLockedLayers = LockValue != 0;

		LockValue = 0;
		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LOCK_LAYERS);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, LockValue);
		PartData.bLockLayer = LockValue != 0;

		//-----------------------------------------------------------------------------------------------------------------------------
		// HAPI_UNREAL_ATTRIB_LANDSCAPE_NORMALIZE
		//-----------------------------------------------------------------------------------------------------------------------------

		int NormalizeValue = 0;
		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NORMALIZE);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, NormalizeValue);
		PartData.bNormalizePaintLayers = NormalizeValue != 0;

		//-----------------------------------------------------------------------------------------------------------------------------
		// unreal_landscape_editlayer_subtractive
		//-----------------------------------------------------------------------------------------------------------------------------

		int SubtractiveMode = HAPI_UNREAL_LANDSCAPE_EDITLAYER_SUBTRACTIVE_OFF;
		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_SUBTRACTIVE);
		bool bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, SubtractiveMode);
		PartData.bSubtractiveEditLayer = SubtractiveMode == HAPI_UNREAL_LANDSCAPE_EDITLAYER_SUBTRACTIVE_ON;

		//-----------------------------------------------------------------------------------------------------------------------------
		// Properties
		//-----------------------------------------------------------------------------------------------------------------------------

		FHoudiniEngineUtils::GetGenericPropertiesAttributes(PartObj.GeoId, PartObj.PartId, true, 0, 0, 0, PartData.PropertyAttributes);

		//-----------------------------------------------------------------------------------------------------------------------------
		// See if this HAPI Volume is part of a larger landscape, ie. it is a tile. we do this by looking for the
		// HAPI_UNREAL_ATTRIB_LANDSCAPE_SIZE attribute, which tells us the overall size of the landscape.
		//-----------------------------------------------------------------------------------------------------------------------------

		FHoudiniTileInfo TileInfo;
		bool bValid = true;

		// Look for the landscape attribute
		TArray<int> LandscapeDimensions;
		LandscapeDimensions.SetNumZeroed(2);

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SIZE);
		bValid &= Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 2, LandscapeDimensions, 0, 1);
	

		HAPI_AttributeInfo PositionInfo;
		FHoudiniApi::AttributeInfo_Init(&PositionInfo);

		// Get the center of the tile - this is stored in the "P" attribute.

		TArray<float> TIleCenterRelativeToOrigin;
		TIleCenterRelativeToOrigin.SetNumZeroed(3);

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_POSITION);
		bValid &= Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, TIleCenterRelativeToOrigin);

		if (bValid && TIleCenterRelativeToOrigin.Num() == 3)
		{
			// Convert "center" of tile (supplied by HAPI) to global lower corner (x,y)
			// of entire landscape.
			float TileX = PartData.HeightField->VolumeInfo.XLength;
			float TileY = PartData.HeightField->VolumeInfo.YLength;
			float HalfTileX = TileX * 0.5f;
			float HalfTileY = TileY * 0.5f;
			float CornerX = TIleCenterRelativeToOrigin[0] - HalfTileX;
			float CornerY = TIleCenterRelativeToOrigin[2] - HalfTileY;
			float CenterX = CornerX + (LandscapeDimensions[0]) * 0.5f;
			float CenterY = CornerY + (LandscapeDimensions[1]) * 0.5f;

			TileInfo.TileStart.X = static_cast<int>(CenterX);
			TileInfo.TileStart.Y = static_cast<int>(CenterY);
			TileInfo.LandscapeDimensions.X = LandscapeDimensions[0];
			TileInfo.LandscapeDimensions.Y = LandscapeDimensions[1];

			// A valid tile is stored.
			PartData.TileInfo = TileInfo;
		}

		//-----------------------------------------------------------------------------------------------------------------------------
		// If creating a new landscape, look for size info.
		//-----------------------------------------------------------------------------------------------------------------------------

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_PARTITION_GRID_SIZE);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.SizeInfo.WorldPartitionGridSize);

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SECTION_SIZE);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.SizeInfo.NumQuadsPerSection);

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SECTIONS_PER_COMPONENT);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.SizeInfo.NumSectionsPerComponent);

		// Make sure both Sections Per Component and Section Size were specified.
		bool bSectionSizeSet = PartData.SizeInfo.NumQuadsPerSection != 0;
		bool bSectionsPerComponentSet = PartData.SizeInfo.NumSectionsPerComponent != 0;
		if ((bSectionSizeSet && !bSectionsPerComponentSet) || (bSectionsPerComponentSet && !bSectionSizeSet))
		{
			HOUDINI_LOG_ERROR(TEXT("Both " HAPI_UNREAL_ATTRIB_LANDSCAPE_SECTION_SIZE " and " HAPI_UNREAL_ATTRIB_LANDSCAPE_SECTIONS_PER_COMPONENT " must be set. Ignoring."));
			PartData.SizeInfo.NumQuadsPerSection = 0;
			PartData.SizeInfo.NumSectionsPerComponent = 0;
		}
		else
		{
			bool bValidSizes = true;
			if (!FMath::IsPowerOfTwo(PartData.SizeInfo.NumQuadsPerSection + 1))
			{
				HOUDINI_LOG_ERROR(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SECTION_SIZE " must be 1 less than a power of two."));
				bValidSizes = false;
			}
			if (PartData.SizeInfo.NumSectionsPerComponent < 0 || PartData.SizeInfo.NumSectionsPerComponent > 2)
			{
				HOUDINI_LOG_ERROR(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SECTIONS_PER_COMPONENT " must be 1 or 2."));
				bValidSizes = false;
			}
			if (!bValidSizes)
			{
				PartData.SizeInfo.NumQuadsPerSection = 0;
				PartData.SizeInfo.NumSectionsPerComponent = 0;
			}
		}

		//-----------------------------------------------------------------------------------------------------------------------------
		// At this point we have all the information to determine the landscapes size in Unreal. 
		//-----------------------------------------------------------------------------------------------------------------------------

		// Start with the size of the height field Part.
		PartData.SizeInfo.UnrealGridDimensions = FHoudiniLandscapeUtils::GetVolumeDimensionsInUnrealSpace(PartObj);

		// If we have a tile, use the specified size to override.
		if (PartData.TileInfo.IsSet())
			PartData.SizeInfo.UnrealGridDimensions = PartData.TileInfo.GetValue().LandscapeDimensions;

		// Determine Quads Per section and Num Sections Per Component if they were not specified.
		if (PartData.SizeInfo.NumQuadsPerSection == 0 || PartData.SizeInfo.NumSectionsPerComponent == 0)
		{
			FHoudiniLandscapeUtils::CalcLandscapeSizeFromHeightFieldSize(PartData.SizeInfo.UnrealGridDimensions.X, PartData.SizeInfo.UnrealGridDimensions.Y, PartData.SizeInfo);

		}

		// if the SizeInfo is invalid, print errors and exit.
		if (PartData.SizeInfo.Validate())
		{
			HOUDINI_LOG_ERROR(TEXT("Skipping part %s as it has generated invalid Unreal size info."), *PartObj.VolumeName );
			continue;
		}

		//-----------------------------------------------------------------------------------------------------------------------------
		// Weight blend.
		//-----------------------------------------------------------------------------------------------------------------------------

		int NonWeightBlend = HAPI_UNREAL_LANDSCAPE_LAYER_NOWEIGHTBLEND_OFF;

		TSet<FString> NonWeightBlendedLayers = FHoudiniLandscapeUtils::GetNonWeightBlendedLayerNames(PartObj);
		if (NonWeightBlendedLayers.Contains(PartData.TargetLayerName))
			NonWeightBlend = HAPI_UNREAL_LANDSCAPE_LAYER_NOWEIGHTBLEND_ON;

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND);
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, NonWeightBlend);

		// Note layer stores Weight Blended, not NOT Weight Blended. I can't deal with double negatives.
		PartData.bIsWeightBlended = NonWeightBlend == HAPI_UNREAL_LANDSCAPE_LAYER_NOWEIGHTBLEND_OFF;

		//---------------------------------------------------------------------------------------------------------------------------------
		// Layer Info Object, if it exists.
		//---------------------------------------------------------------------------------------------------------------------------------

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_INFO);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.LayerInfoObjectName);

		//-----------------------------------------------------------------------------------------------------------------------------
		// target landscape name.
		//-----------------------------------------------------------------------------------------------------------------------------

		PartData.TargetLandscapeName.Empty();

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.TargetLandscapeName);

		if (PartData.TargetLandscapeName.IsEmpty())
		{
			// The previous implementation of the "Edit Layer" mode used the Shared Landscape Actor name. If the (new)
			// Edit Layer Target attribute wasn't specified, check whether the old attribute is present.

			Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME);
			bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.TargetLandscapeName);
		}

		if (PartData.TargetLandscapeName.IsEmpty())
		{
			// The previous implementation of the "Edit Layer" mode used the Shared Landscape Actor name. If the (new)
			// Edit Layer Target attribute wasn't specified, check whether the old attribute is present.

			Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TARGET);
			bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.TargetLandscapeName);
		}

		if (PartData.TargetLandscapeName.IsEmpty())
		{
			// No name was specified, set a default depending on whether the Output is creating or modifying a landscape
			PartData.TargetLandscapeName = (LandscapeOutputMode == HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_GENERATE) ? FString("Landscape") : FString("Input0");
		}

		//-----------------------------------------------------------------------------------------------------------------------------
		// Height Range
		//-----------------------------------------------------------------------------------------------------------------------------

		FHoudiniMinMax MinMax;

		const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
		if (HoudiniRuntimeSettings->MarshallingLandscapesForceMinMaxValues)
		{
			MinMax.MinValue = HoudiniRuntimeSettings->MarshallingLandscapesForcedMinValue;
			MinMax.MaxValue = HoudiniRuntimeSettings->MarshallingLandscapesForcedMaxValue;
			PartData.HeightRange = MinMax;
		}
		else
		{
			Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MIN);
			bool bHasMin = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, MinMax.MinValue);

			Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MAX);
			bool bHasMax = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, MinMax.MaxValue);

			if (bHasMin != bHasMax)
				HOUDINI_LOG_ERROR(TEXT("Must specify both " HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MIN " and " HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MAX));

			if (bHasMin && bHasMax)
				PartData.HeightRange = MinMax;
		}


		//-----------------------------------------------------------------------------------------------------------------------------
		// Material name
		//-----------------------------------------------------------------------------------------------------------------------------

		// Regular material

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_MATERIAL);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.Materials.Material);

		if (PartData.Materials.Material.IsEmpty())
		{
			Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE);
			bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.Materials.Material);
			PartData.Materials.bCreateMaterialInstance = !PartData.Materials.Material.IsEmpty();
		}

		// Hole material

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.Materials.HoleMaterial);

		if (PartData.Materials.HoleMaterial.IsEmpty())
		{
			Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE_INSTANCE);
			bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.Materials.HoleMaterial);
		}

		// Physical Material

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_PHYSICAL_MATERIAL);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.Materials.PhysicalMaterial);

		//-----------------------------------------------------------------------------------------------------------------------------
		// Bake folders
		//-----------------------------------------------------------------------------------------------------------------------------

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.BakeOutlinerFolder);

		Accessor.Init(PartObj.GeoId, PartObj.PartId, HAPI_UNREAL_ATTRIB_BAKE_FOLDER);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, PartData.BakeFolder);

		//-----------------------------------------------------------------------------------------------------------------------------
		// Add new layer.
		//-----------------------------------------------------------------------------------------------------------------------------

		Results.Add(std::move(PartData));
	}
	return Results;

}



UHoudiniLandscapeTargetLayerOutput*
FHoudiniLandscapeTranslator::TranslateHeightFieldPart(
		UHoudiniOutput* OwningOutput,
		FHoudiniUnrealLandscapeTarget& Landscape,
		FHoudiniHeightFieldPartData& Part,
		UHoudiniAssetComponent& HAC,
		FHoudiniClearedEditLayers& ClearedLayers,
		const FHoudiniPackageParams& InPackageParams)
{
	H_SCOPED_FUNCTION_TIMER();

	enum TargetLayerType
	{
		Height, Visibility, Paint
	};

	//----------------------------------------------------------------------------------------------------------------------------------------
	// Resolve Landscape Actors
	//----------------------------------------------------------------------------------------------------------------------------------------

	ALandscapeProxy& LandscapeProxy = *Landscape.Proxy.Get();
	ALandscape * OutputLandscape = nullptr;

	OutputLandscape = LandscapeProxy.GetLandscapeActor();
	if (!IsValid(OutputLandscape))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not retrieve the landscape actor for: %s"), *(Part.TargetLandscapeName));
		return nullptr;
	}

	if (!OutputLandscape->bCanHaveLayersContent)
	{
		HOUDINI_LOG_WARNING(TEXT("Target landscape does not have edit layers enabled. Cooking will directly affect the landscape: %s"), *(Part.TargetLandscapeName));
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Set Layer names. The Baked layer name is always what the user specifies; If we are modifying an existing landscape,
	// use temporary names if specified or user name baked names.
	// -----------------------------------------------------------------------------------------------------------------

	FString BakedLayerName = Part.UnrealLayerName;

	// For the cooked name, but the layer name first so it is easier to read in the Landscape Editor UI.
	auto LayerPackageParams = InPackageParams;
	FString CookedLayerName = BakedLayerName;

	if (HAC.bLandscapeUseTempLayers)
	{
		CookedLayerName = CookedLayerName + FString(" : ") + LayerPackageParams.GetPackageName() + HAC.GetComponentGUID().ToString();
	}	

	// ------------------------------------------------------------------------------------------------------------------
	// Make sure the target layer exists before we do anything else. If its missing, we can't do anything
	//-------------------------------------------------------------------------------------------------------------------

	ULandscapeLayerInfoObject* TargetLayerInfo = OutputLandscape->GetLandscapeInfo()->GetLayerInfoByName(FName(Part.TargetLayerName));

	if (!TargetLayerInfo && Part.TargetLayerName != "height" && Part.TargetLayerName != "visibility" && Part.TargetLayerName != "mask")
	{
		HOUDINI_LOG_WARNING(TEXT("Could not find target layer: %s. This should be 'height', 'visibility' or the name of a layer in the material."), *(Part.TargetLayerName));
		return nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Create the Edit Layer if it doesn't exist
	// ------------------------------------------------------------------------------------------------------------------

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* UnrealEditLayer = nullptr;
#else
	FLandscapeLayer* UnrealEditLayer = nullptr;
#endif
	bool bWasLocked = false;
	if (OutputLandscape->bCanHaveLayersContent)
	{
		UnrealEditLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(OutputLandscape, FName(CookedLayerName));
		if (!UnrealEditLayer)
			return nullptr;
		
		bWasLocked = UnrealEditLayer->bLocked;
		if (UnrealEditLayer->bLocked)
		{
			if (Part.bWriteLockedLayers)
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
				OutputLandscape->SetLayerLocked(OutputLandscape->GetLayerIndex(UnrealEditLayer->Name), false);
#else
				UnrealEditLayer->bLocked = false;
#endif
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Cannot write to locked Edit Layer: %s"), *CookedLayerName);
				return nullptr;
			}
		}
	

		// Move this layer after another layer if required.
		if (!Part.AfterLayerName.IsEmpty())
		{
			UnrealEditLayer = FHoudiniLandscapeUtils::MoveEditLayerAfter(OutputLandscape, FName(CookedLayerName), FName(Part.AfterLayerName));
		}
	}
	int UnrealEditLayerIndex = INDEX_NONE;
	if (UnrealEditLayer != nullptr)
		UnrealEditLayerIndex = OutputLandscape->GetLayerIndex(UnrealEditLayer->Name);

	// ------------------------------------------------------------------------------------------------------------------
	// Clear layer
	// ------------------------------------------------------------------------------------------------------------------

	TargetLayerType LayerType = TargetLayerType::Paint;
	if (Part.TargetLayerName == "height")
		LayerType = TargetLayerType::Height;
	else if (Part.TargetLayerName == "visibility")
		LayerType = TargetLayerType::Visibility;

	if (UnrealEditLayer != nullptr && 
		OutputLandscape->bHasLayersContent &&
		Part.bClearLayer &&
		!ClearedLayers.Contains(CookedLayerName, Part.TargetLayerName))
	{
		if (LayerType != TargetLayerType::Paint)
		{
			OutputLandscape->ClearLayer(UnrealEditLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);
		}
		else
		{
			OutputLandscape->ClearPaintLayer(UnrealEditLayer->Guid, TargetLayerInfo);
		}
		ClearedLayers.Add(CookedLayerName, Part.TargetLayerName);
	}


	if (LayerType == TargetLayerType::Paint && TargetLayerInfo == nullptr)
	{
		// The target layer doesn't exist, so report an error and do nothing. The target layers are defined by the material
		// and trying to create new ones is probably not correct. Note, this is different from what we do if a Layer is missing.

		// mask is very common, so silently ignore it.
		if (Part.TargetLayerName != "mask")
			HOUDINI_LOG_WARNING(TEXT("Tried to export to a target layer called %s but it does not exist"), *Part.TargetLayerName);

		return nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Layer controls
	// ------------------------------------------------------------------------------------------------------------------

	if (OutputLandscape->bHasLayersContent)
	{
		if (Part.bSubtractiveEditLayer != OutputLandscape->IsLayerBlendSubstractive(UnrealEditLayerIndex, TargetLayerInfo))
		{
			OutputLandscape->SetLayerSubstractiveBlendStatus(UnrealEditLayerIndex, Part.bSubtractiveEditLayer, TargetLayerInfo);
		}

		if (TargetLayerInfo)
			TargetLayerInfo->bNoWeightBlend = !Part.bIsWeightBlended;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Fetch the actual height field data
	// ------------------------------------------------------------------------------------------------------------------

	// Fetch the height field data from Houdini into Unreal Space. This data may have already been fetched during landscape
	// creation, so if it's already present.

	const bool bFetchData = !Landscape.bWasCreated || LayerType != TargetLayerType::Height;
	
	FHoudiniHeightFieldData HeightFieldData = FHoudiniLandscapeUtils::FetchVolumeInUnrealSpace(
			*Part.HeightField, 
			Part.SizeInfo.UnrealGridDimensions,
			bFetchData);

	// The transform we get from Houdini should be relative to the HDA:
	HeightFieldData.Transform = HeightFieldData.Transform * HAC.GetComponentTransform();

	// If a new landscape was created, resize the layer to match the created landscape size. (We resize the landscape if it does
	// not fit one of Unreal's predetermined sizes. Only do this for non-tiles.
	if (Landscape.bWasCreated && !Part.TileInfo.IsSet() && bFetchData)
	{
		if (Landscape.Dimensions != HeightFieldData.Dimensions)
			HeightFieldData = FHoudiniLandscapeUtils::ReDimensionLandscape(HeightFieldData, Landscape.Dimensions);
	}

	auto Extents = FHoudiniLandscapeUtils::GetExtents(OutputLandscape, HeightFieldData);

	// ------------------------------------------------------------------------------------------------------------------
	// Is a paint layer or visibility layer
	// ------------------------------------------------------------------------------------------------------------------

	ULandscapeInfo* TargetLandscapeInfo = OutputLandscape->GetLandscapeInfo();
	if (LayerType == TargetLayerType::Paint || LayerType == TargetLayerType::Visibility)
	{
		FGuid LayerGUID;
		if (OutputLandscape->bCanHaveLayersContent)
			LayerGUID = UnrealEditLayer->Guid;

		bool bExceededRange = FHoudiniLandscapeUtils::NormalizePaintLayers(HeightFieldData.Values, Part.bNormalizePaintLayers);

		if (bExceededRange)
			HOUDINI_LOG_WARNING(TEXT("Target layer %s contains values outside the range 0 to 1."), *Part.TargetLayerName);


		TArray<uint8> Values;
		Values.SetNum(HeightFieldData.Values.Num());

		ParallelFor(Values.Num(), [&](int Index)
		{
			float Value = HeightFieldData.Values[Index];
			Values[Index] = static_cast<uint8>(Value * 255);
		});

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, LayerGUID, [&] { OutputLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });

		if (LayerType == TargetLayerType::Visibility)
		{
			FAlphamapAccessor<false, false> AlphaAccessor(OutputLandscape->GetLandscapeInfo(), ALandscapeProxy::VisibilityLayer);
			AlphaAccessor.SetData(
				Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
				Values.GetData(),
				ELandscapeLayerPaintingRestriction::None);
		}
		else
		{
			FAlphamapAccessor<false, false> AlphaAccessor(OutputLandscape->GetLandscapeInfo(), TargetLayerInfo);
			AlphaAccessor.SetData(
				Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
				Values.GetData(),
				ELandscapeLayerPaintingRestriction::None);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Is this the height layer and we did not create a new landscape, apply it.
	// ------------------------------------------------------------------------------------------------------------------

	if (LayerType == TargetLayerType::Height && !Landscape.bWasCreated)
	{

		TArray<uint16> QuantizedData = FHoudiniLandscapeUtils::ConvertHeightFieldData(OutputLandscape, HeightFieldData.Values);

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, UnrealEditLayer->Guid, [&] { OutputLandscape->ForceUpdateLayersContent(); });

		FLandscapeEditDataInterface LandscapeEdit(TargetLandscapeInfo);
		FHeightmapAccessor<false> HeightMapAccessor(TargetLandscapeInfo);
		HeightMapAccessor.SetData(
			Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
			QuantizedData.GetData());

	}

	if (bWasLocked && UnrealEditLayer)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		OutputLandscape->SetLayerLocked(OutputLandscape->GetLayerIndex(UnrealEditLayer->Name), true);
#else
		UnrealEditLayer->bLocked = true;
#endif
	}

	// ------------------------------------------------------------------------------------------------------------------
	// We successfully did what we came to, return an Object
	// ------------------------------------------------------------------------------------------------------------------

	UHoudiniLandscapeTargetLayerOutput * Obj = NewObject<UHoudiniLandscapeTargetLayerOutput>(OwningOutput);
	Obj->BakedEditLayer = BakedLayerName;
	Obj->CookedEditLayer = CookedLayerName;
	Obj->Landscape = OutputLandscape;
	Obj->LandscapeProxy = &LandscapeProxy;
	Obj->Extents = Extents;
	Obj->bCreatedLandscape = Part.bCreateNewLandscape;
	Obj->TargetLayer = Part.TargetLayerName;
	Obj->bClearLayer = Part.bClearLayer;
	Obj->BakedLandscapeName = Landscape.BakedName.ToString();
	Obj->LayerInfoObjects = Landscape.CreatedLayerInfoObjects;
	Obj->bCookedLayerRequiresBaking = OutputLandscape->bCanHaveLayersContent && (CookedLayerName != BakedLayerName);
	Obj->BakeFolder = Part.BakeFolder;
	Obj->MaterialInstance = Part.MaterialInstance;
	Obj->bWriteLockedLayers = Part.bWriteLockedLayers;
	Obj->bLockLayer = Part.bLockLayer;
	Obj->PropertyAttributes = Part.PropertyAttributes;
	return Obj;


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
FHoudiniLandscapeTranslator::CalcHeightFieldsArrayGlobalZMinZMax(
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

	// Get runtime settings.
	float ForcedZMin = 0.0f;
	float ForcedZMax = 0.0f;
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bUseForcedMinMax = (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesForceMinMaxValues);
	if(bUseForcedMinMax)
	{
		ForcedZMin = HoudiniRuntimeSettings->MarshallingLandscapesForcedMinValue;
		ForcedZMax = HoudiniRuntimeSettings->MarshallingLandscapesForcedMaxValue;
	}

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	TArray<float> FloatData;

	for (const FHoudiniGeoPartObject& CurrentHeightfield: InHeightfieldArray)
	{
		// Get the current Height Field GeoPartObject
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

		FHoudiniHapiAccessor LayerMinAccessor(CurrentHeightfield.GeoId, CurrentHeightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MIN);
		LayerMinAccessor.GetInfo(AttributeInfo);
		AttributeInfo.tupleSize = 1;

		if (LayerMinAccessor.GetAttributeData(HAPI_ATTROWNER_INVALID, FloatData, 0, 1))
		{
			if (FloatData.Num() > 0)
			{
				GlobalMinimums.Add(VolumeName, FloatData[0]);
				bHasMinAttr = true;
			}
		}

		// If this volume has an attribute defining maximum value use it as is.
		FloatData.Empty();
		FHoudiniHapiAccessor LayerMaxAccessor(CurrentHeightfield.GeoId, CurrentHeightfield.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_MAX);
		LayerMaxAccessor.GetInfo(AttributeInfo);
		AttributeInfo.tupleSize = 1;

		if (LayerMaxAccessor.GetAttributeData(HAPI_ATTROWNER_INVALID, FloatData, 0, 1))
		{
			if (FloatData.Num() > 0)
			{
				GlobalMaximums.Add(VolumeName, FloatData[0]);
				bHasMaxAttr = true;
			}
		}

		if (!bHasMinAttr || !bHasMaxAttr)
		{
			// We haven't specified both min/max values
			// Unreal's Z values are Y in Houdini
			float ymin, ymax;

			if (bUseForcedMinMax)
			{
				// First, see if we should use the forced min/max values from the settings
				ymin = ForcedZMin;
				ymax = ForcedZMax;
			}
			else
			{
				// Get the min/max value from the volume
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds(
					FHoudiniEngine::Get().GetSession(),
					NodeId,
					CurrentHeightfield.PartId,
					nullptr, &ymin, nullptr,
					nullptr, &ymax, nullptr,
					nullptr, nullptr, nullptr))
					continue;
			}
		
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

bool FHoudiniLandscapeCreationInfo::Validate()
{
	bool bError = false;

	if (this->NumQuadsPerSection == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error creating landscape, NumQuadsPerSection is zero"));
		bError = true;
	}
	if (this->NumSectionsPerComponent == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error creating landscape, NumSectionsPerComponent is zero"));
		bError = true;
	}
	if (this->UnrealGridDimensions.X == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error creating landscape, X Size is zero"));
		bError = true;
	}
	if (this->UnrealGridDimensions.Y == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Error creating landscape, Y Size is zero"));
		bError = true;
	}
	return bError;
}

#undef LOCTEXT_NAMESPACE
