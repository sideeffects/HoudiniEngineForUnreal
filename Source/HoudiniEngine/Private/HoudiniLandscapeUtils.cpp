/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "HoudiniLandscapeUtils.h"
#include "LandscapeEdit.h"
#include "HoudiniAssetComponent.h"
#include "Landscape.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "UObject/UObjectGlobals.h"
#include "LandscapeDataAccess.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HoudiniPackageParams.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LandscapeConfigHelper.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "HoudiniMaterialTranslator.h"
#include "PackageTools.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"

TSet<UHoudiniLandscapeTargetLayerOutput *>
FHoudiniLandscapeUtils::GetEditLayers(UHoudiniOutput& Output)
{
	TSet<UHoudiniLandscapeTargetLayerOutput *> Result;

	for (auto& OutputObjectPair : Output.GetOutputObjects())
	{
		const FHoudiniOutputObject& PrevObj = OutputObjectPair.Value;
		if (PrevObj.OutputObject->IsA<UHoudiniLandscapeTargetLayerOutput>())
		{
			UHoudiniLandscapeTargetLayerOutput* Layer = Cast<UHoudiniLandscapeTargetLayerOutput>(PrevObj.OutputObject);
			Result.Add(Layer);
		}
	}
	return Result;
}

TSet<FString>
FHoudiniLandscapeUtils::GetCookedLandscapeLayers(UHoudiniAssetComponent& HAC, ALandscape& Landscape)
{
	TSet<FString> Layers;

	for(int OutputIndex = 0; OutputIndex < HAC.GetNumOutputs(); OutputIndex++)
	{
		UHoudiniOutput* Output 	= HAC.GetOutputAt(OutputIndex);
		if (!IsValid(Output))
			continue;

		TSet<UHoudiniLandscapeTargetLayerOutput * > LandscapeEditLayers = GetEditLayers(*Output);

		for(UHoudiniLandscapeTargetLayerOutput * Layer : LandscapeEditLayers)
		{
			if (Layer->Landscape == &Landscape)
				Layers.Add(Layer->CookedEditLayer);
		}
	}
	return Layers;
}

void
FHoudiniLandscapeUtils::SetNonCookedLayersVisibility(UHoudiniAssetComponent& HAC, ALandscape& Landscape, bool bVisible)
{
	TSet<FString> CookedLayers = GetCookedLandscapeLayers(HAC, Landscape);

	for(int LayerIndex = 0; LayerIndex < Landscape.LandscapeLayers.Num(); LayerIndex++)
	{
		if (!CookedLayers.Contains(Landscape.LandscapeLayers[LayerIndex].Name.ToString()))
		{
			// Non cooked Layer
			Landscape.SetLayerVisibility(LayerIndex, bVisible);
		}
	}
}

void
FHoudiniLandscapeUtils::SetCookedLayersVisibility(UHoudiniAssetComponent& HAC, ALandscape& Landscape, bool bVisible)
{
	TSet<FString> CookedLayers = GetCookedLandscapeLayers(HAC, Landscape);

	for (int LayerIndex = 0; LayerIndex < Landscape.LandscapeLayers.Num(); LayerIndex++)
	{
		if (CookedLayers.Contains(Landscape.LandscapeLayers[LayerIndex].Name.ToString()))
		{
			// Cooked Layer
			Landscape.SetLayerVisibility(LayerIndex, bVisible);
		}
	}
}

void FHoudiniLandscapeUtils::RealignHeightFieldData(TArray<float>& Data, float ZeroPoint, float Scale)
{
	for(int Index = 0; Index < Data.Num(); Index++)
	{
		Data[Index] = Data[Index] * Scale + ZeroPoint;
	}
}


bool FHoudiniLandscapeUtils::ClampHeightFieldData(TArray<float>& Data, float MinValue, float MaxValue)
{
	TArray<float> Result;
	bool bClamped = false;
	Result.SetNumUninitialized(Data.Num());
	for (int Index = 0; Index < Result.Num(); Index++)
	{
		float Value = Data[Index];
		Data[Index] = FMath::Clamp(Value, MinValue, MaxValue);
		bClamped |= (Data[Index] != Value);
	}
	return bClamped;
}

TArray<uint16>
FHoudiniLandscapeUtils::QuantizeNormalizedDataTo16Bit(const TArray<float>& Data)
{
	TArray<uint16> Result;
	Result.SetNumUninitialized(Data.Num());
	for(int Index = 0; Index < Data.Num(); Index++)
	{
		int Quantized = static_cast<int>(Data[Index] * 65535);
		Result[Index] = FMath::Clamp<int>(Quantized, 0, 65535);
	}
	return Result;
}

static float Convert(int NewValue, int NewMax, int OldMax)
{
	float Scale = float(NewValue) / float(NewMax - 1);
	return (Scale * OldMax);
}

float FHoudiniLandscapeUtils::GetLandscapeHeightRangeInCM(ALandscape& Landscape)
{
	float Scale = Landscape.GetTransform().GetScale3D().Z;

	return Scale * 256.0f;

}

TArray<uint16> FHoudiniLandscapeUtils::GetHeightData(ALandscape* Landscape, const FHoudiniExtents& Extents, FLandscapeLayer* EditLayer)
{
	int DiffX = 1 + Extents.Max.X - Extents.Min.X;
	int DiffY = 1 + Extents.Max.Y - Extents.Min.Y;
	int NumPoints = DiffX * DiffY;

	TArray<uint16> Values;
	Values.SetNum(NumPoints);

	FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayer->Guid, [&] { /*Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); */});

	FLandscapeEditDataInterface LandscapeEdit(Landscape->GetLandscapeInfo());
	LandscapeEdit.SetShouldDirtyPackage(false);
	LandscapeEdit.GetHeightDataFast(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData(), 0);

	return Values;
}

FLandscapeLayer* FHoudiniLandscapeUtils::GetOrCreateEditLayer(ALandscape* Landscape, const FName& LayerName)
{
	FLandscapeLayer* UnrealEditLayer = GetEditLayer(Landscape, LayerName);
	if (UnrealEditLayer == nullptr)
	{
		int EditLayerIndex = Landscape->CreateLayer(LayerName);

		if (EditLayerIndex == INDEX_NONE)
		{
			HOUDINI_LOG_ERROR(TEXT("Could not create edit layer %s"), *LayerName.ToString());
			return nullptr;
		}
		UnrealEditLayer = Landscape->GetLayer(EditLayerIndex);
	}

	return UnrealEditLayer;
}

FLandscapeLayer* 
FHoudiniLandscapeUtils::GetEditLayer(ALandscape* Landscape, const FName& LayerName)
{
	if (!Landscape->bCanHaveLayersContent)
		return Landscape->GetLayer(0);

	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	if (EditLayerIndex == INDEX_NONE)
		return nullptr;

	FLandscapeLayer* UnrealEditLayer = Landscape->GetLayer(EditLayerIndex);
	return UnrealEditLayer;

}

FLandscapeLayer* 
FHoudiniLandscapeUtils::MoveEditLayerAfter(ALandscape* Landscape, const FName& LayerName, const FName& AfterLayerName)
{
	if (!Landscape->bCanHaveLayersContent)
		return Landscape->GetLayer(0);

	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	int32 NewLayerIndex = Landscape->GetLayerIndex(AfterLayerName);
	if (NewLayerIndex == INDEX_NONE || EditLayerIndex == INDEX_NONE)
		return nullptr;

	if (NewLayerIndex < EditLayerIndex)
	{
		NewLayerIndex += 1;
	}
	Landscape->ReorderLayer(EditLayerIndex, NewLayerIndex);

	// Ensure we have the correct layer/index
	EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	return Landscape->GetLayer(EditLayerIndex);


}

TArray<uint8_t> FHoudiniLandscapeUtils::GetLayerData(ALandscape* Landscape, const FHoudiniExtents& Extents, const FName& EditLayerName, const FName& TargetLayerName)
{
	int DiffX = 1 + Extents.Max.X - Extents.Min.X;
	int DiffY = 1 + Extents.Max.Y - Extents.Min.Y;
	int NumPoints = DiffX * DiffY;

	TArray<uint8_t> Values;
	Values.SetNum(NumPoints);

	FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayer(Landscape, EditLayerName);
	ULandscapeLayerInfoObject* TargetLayerInfo = Landscape->GetLandscapeInfo()->GetLayerInfoByName(TargetLayerName);

	FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayer->Guid, [&] { /*Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); */});

	FLandscapeEditDataInterface LandscapeEdit(Landscape->GetLandscapeInfo());
	LandscapeEdit.SetShouldDirtyPackage(false);
	LandscapeEdit.GetWeightDataFast(TargetLayerInfo, Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData(), 0);

	return Values;
}


bool
FHoudiniLandscapeUtils::CalcLandscapeSizeFromHeightFieldSize(
	const int32 ProposedUnrealSizeX, const int32 ProposedUnrealSizeY, 
	FHoudiniLandscapeCreationInfo & Info)
{
	// TODO: We already know Proposed Size will fit, so some of this function is redundant.

	if ((ProposedUnrealSizeX < 2) || (ProposedUnrealSizeY < 2))
		return false;

	Info.NumSectionsPerComponent = 1;
	Info.NumQuadsPerSection = 1;
	Info.UnrealSize.X = -1;
	Info.UnrealSize.Y = -1;

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
		ComponentsCountX = FMath::Clamp(ComponentsCountX, 1, FMath::Min(32, FMath::FloorToInt(8191.0f / (float)(Info.NumSectionsPerComponent * Info.NumQuadsPerSection))));
		ComponentsCountY = FMath::Clamp(ComponentsCountY, 1, FMath::Min(32, FMath::FloorToInt(8191.0f / (float)(Info.NumSectionsPerComponent * Info.NumQuadsPerSection))));
	};

	// Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
	bool bFoundMatch = false;
	for (int32 SectionSizesIdx = UE_ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
	{
		for (int32 NumSectionsIdx = UE_ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
		{
			int32 SectionSize = SectionSizes[SectionSizesIdx];
			int32 NumSection = NumSections[NumSectionsIdx];
			int32 Total = SectionSize * NumSection;
			if (((ProposedUnrealSizeX - 1) % Total == 0 && ((ProposedUnrealSizeX - 1) / Total) <= 32 &&
				((ProposedUnrealSizeY - 1) % Total) == 0 && ((ProposedUnrealSizeY - 1) / Total) <= 32))
			{
				bFoundMatch = true;
				Info.NumQuadsPerSection = SectionSize;
				Info.NumSectionsPerComponent = NumSection;
				ComponentsCountX = (ProposedUnrealSizeX - 1) / Total;
				ComponentsCountY = (ProposedUnrealSizeY - 1) / Total;
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
		// if there was no exact match, try increasing the section size until we encompass the whole height field
		const int32 CurrentSectionSize = Info.NumQuadsPerSection;
		const int32 CurrentNumSections = Info.NumSectionsPerComponent;
		for (int32 SectionSizesIdx = 0; SectionSizesIdx < UE_ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
		{
			if (SectionSizes[SectionSizesIdx] < CurrentSectionSize)
			{
				continue;
			}

			const int32 ComponentsX = FMath::DivideAndRoundUp((ProposedUnrealSizeX - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
			const int32 ComponentsY = FMath::DivideAndRoundUp((ProposedUnrealSizeY - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
			if (ComponentsX <= 32 && ComponentsY <= 32)
			{
				bFoundMatch = true;
				Info.NumQuadsPerSection = SectionSizes[SectionSizesIdx];
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
		const int32 ComponentsX = FMath::DivideAndRoundUp((ProposedUnrealSizeX - 1), MaxSectionSize * MaxNumSubSections);
		const int32 ComponentsY = FMath::DivideAndRoundUp((ProposedUnrealSizeY - 1), MaxSectionSize * MaxNumSubSections);

		bFoundMatch = true;
		Info.NumQuadsPerSection = MaxSectionSize;
		Info.NumSectionsPerComponent = MaxNumSubSections;
		ComponentsCountX = ComponentsX;
		ComponentsCountY = ComponentsY;
		ClampLandscapeSize();
	}

	if (!bFoundMatch)
	{
		// Using default size just to not crash..
		Info.UnrealSize.X = 512;
		Info.UnrealSize.Y = 512;
		Info.NumSectionsPerComponent = 1;
		Info.NumQuadsPerSection = 511;
		ComponentsCountX = 1;
		ComponentsCountY = 1;
	}
	else
	{
		// Calculating the desired size
		int32 QuadsPerComponent = Info.NumSectionsPerComponent * Info.NumQuadsPerSection;

		Info.UnrealSize.X = ComponentsCountX * QuadsPerComponent + 1;
		Info.UnrealSize.Y = ComponentsCountY * QuadsPerComponent + 1;
	}

	return bFoundMatch;
}

FHoudiniLayersToUnrealLandscapeMapping
FHoudiniLandscapeUtils::ResolveLandscapes(
	const FString& CookedLandscapePrefix,
	const FHoudiniPackageParams& PackageParams, 
	UHoudiniAssetComponent* HAC,
	TMap<FString, ALandscape*>& LandscapeMap,
	TArray<FHoudiniHeightFieldPartData>& Parts, 
	UWorld * World, 
	const TArray<ALandscapeProxy*>& LandscapeInputs)
{
	FHoudiniLayersToUnrealLandscapeMapping Result;

	//--------------------------------------------------------------------------------------------------------------------------
	// Go through each layer and find the Landscape actor. If "Create New Landscape" is specified then do nothing and create
	// the actor landscape later; this is so we can create one new landscape of the correct name.
	//--------------------------------------------------------------------------------------------------------------------------

	TMap<FString, TMap<FString, FHoudiniHeightFieldPartData*> > LandscapesToCreate;
	TMap<ALandscapeProxy * , TMap<FString, FHoudiniHeightFieldPartData*> > ExistingLandscapes;

	for(int Index = 0; Index < Parts.Num(); Index++)
	{
		FHoudiniHeightFieldPartData& Part = Parts[Index];
		if (Part.bCreateNewLandscape)
		{
			if (LandscapeMap.Contains(Part.TargetLandscapeName))
			{
				// reuse a previously cooked landscape. We assume that any primitve on the same HDA want's the same
				// actor if they have the same name.
				LandscapeMap[Part.TargetLandscapeName];
				Result.HoudiniLayerToUnrealLandscape.Add(&Part, Result.TargetLandscapes.Num());
				FHoudiniUnrealLandscapeTarget LandscapeTarget;
				LandscapeTarget.Proxy = LandscapeMap[Part.TargetLandscapeName];
				LandscapeTarget.bWasCreated = false;
				Result.TargetLandscapes.Add(LandscapeTarget);
			}
			else
			{
				auto & LandscapeParts = LandscapesToCreate.FindOrAdd(Part.TargetLandscapeName);

				if (LandscapeParts.Contains(Part.TargetLayerName))
				{
					HOUDINI_LOG_WARNING(TEXT("Duplicate Layer \"%s\" for landscape \"%s\" was ignored."), *Part.TargetLayerName, *Part.TargetLandscapeName);
				}
				else
				{
					LandscapeParts.Add(Part.TargetLayerName, &Part);
				}
			}
		}
		else
		{
			ALandscapeProxy * LandscapeProxy = FindTargetLandscapeProxy(Part.TargetLandscapeName, World, LandscapeInputs);
			if (!IsValid(LandscapeProxy) || !LandscapeProxy->IsA<ALandscapeProxy>())
			{
				HOUDINI_LOG_ERROR(TEXT("%s is not a valid Landscape Actor."), *Part.TargetLandscapeName);
				continue;
			}

			Result.HoudiniLayerToUnrealLandscape.Add(&Part, Result.TargetLandscapes.Num());
			FHoudiniUnrealLandscapeTarget LandscapeTarget;
			LandscapeTarget.Proxy = LandscapeProxy;
			LandscapeTarget.bWasCreated = false;
			Result.TargetLandscapes.Add(LandscapeTarget);
			auto& PartArray = ExistingLandscapes.FindOrAdd(LandscapeProxy);
			PartArray.Add(Part.TargetLayerName, &Part);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------------
	// Create new actors, stored off above.
	//--------------------------------------------------------------------------------------------------------------------------

	for(auto It : LandscapesToCreate)
	{
		const FString LandscapeActorName = It.Key;
		
		TMap<FString, FHoudiniHeightFieldPartData*> & PartsForLandscape = It.Value;

		//---------------------------------------------------------------------------------------------------------------------------------
		// Look for a height field and use that to initialize the height field to zero.
		// If no height field exists then use the first layer.
		//---------------------------------------------------------------------------------------------------------------------------------

		FHoudiniHeightFieldPartData* PartForSizing = GetPartWithHeightData(PartsForLandscape);
		if (!PartForSizing)
		{
    		PartForSizing = PartsForLandscape.CreateIterator().Value();
			HOUDINI_BAKING_WARNING(TEXT("No height primitve was found, using %s"), *BaseLayer->TargetLayer);
		}

		auto LayerPackageParams = PackageParams;
		LayerPackageParams.ObjectId = PartForSizing->ObjectId;
		LayerPackageParams.GeoId = PartForSizing->GeoId;
		LayerPackageParams.PartId = PartForSizing->PartId;
		LayerPackageParams.SplitStr = CookedLandscapePrefix + LandscapeActorName;
		FString CookingActorName = LayerPackageParams.GetPackageName();

		// Spawn the new Landscape Actor. Note that we only create ALandscape actors here, not Proxies or Streaming Proxies
		// as was the case in World Composition. The name is a temporary one derived from the HDA name.

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = FName(CookingActorName);
		SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		ALandscape * LandscapeActor = World->SpawnActor<ALandscape>(SpawnParameters);
		if (!LandscapeActor)
		{
			HOUDINI_BAKING_ERROR(TEXT("Failed to create actor: %s"), *LandscapeActorName);
			continue;
		}
		LandscapeMap.Add(LandscapeActorName, LandscapeActor);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Set world transform of the landscape. In this version of the code we no longer parent the landscape to the HDA as it breaks
		// up World Partition. 
		//---------------------------------------------------------------------------------------------------------------------------------

		FTransform LocalHeightFieldTransform = GetHeightFieldTransformInUnrealSpace(PartForSizing->HeightField->VolumeInfo);

		if (PartForSizing->TileInfo.IsSet())
		{
			// Adjust the transform of the Landscape actor we are creating if this is a single tile.
			LocalHeightFieldTransform = GetLandscapeActorTransformFromTileTransform(LocalHeightFieldTransform, PartForSizing->TileInfo.GetValue());
		}
		FTransform HACTransform = HAC->GetComponentToWorld();
		FTransform LandscapeTransform = LocalHeightFieldTransform * HACTransform;
		LandscapeActor->SetActorTransform(LandscapeTransform);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Initial settings for landscapes created by Houdini in Unreal.
		//---------------------------------------------------------------------------------------------------------------------------------

		LandscapeActor->PreEditChange(nullptr);
		LandscapeActor->SetLandscapeGuid(FGuid::NewGuid());
		LandscapeActor->bCastStaticShadow = false;
		LandscapeActor->bCanHaveLayersContent = true;

		//---------------------------------------------------------------------------------------------------------------------------------
		// Order is important: Assign materials, create landscape info, Create TargetLayerInfo assets.
		//---------------------------------------------------------------------------------------------------------------------------------

		TArray<UPackage*> CreatedPackages;

		PartForSizing->MaterialInstance = AssignGraphicsMaterialsToLandscape(
					LandscapeActor, 
					PartForSizing->Materials, 
					PackageParams, 
					CreatedPackages);

		LandscapeActor->CreateLandscapeInfo();

		TArray<ULandscapeLayerInfoObject*> CreateLayerInfoObjects =
									CreateTargetLayerInfoAssets(LandscapeActor, PackageParams, PartsForLandscape, CreatedPackages);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Create an empty, zeroed height field. The actual height field, if supplied, will be applied after the landscape is created
		// in an Edit Layer.
		//---------------------------------------------------------------------------------------------------------------------------------

		CreateDefaultHeightField(LandscapeActor, PartForSizing->SizeInfo);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Set the landscape scale. We will need the actual data for the height field for this, so fetch it and cache it for later.
		//---------------------------------------------------------------------------------------------------------------------------------

		PartForSizing->CachedData = MakeUnique<FHoudiniHeightFieldData>(
								FHoudiniLandscapeUtils::FetchVolumeInUnrealSpace(*PartForSizing->HeightField, true));

		FHoudiniLandscapeUtils::AdjustLandscapeTransformToLayerHeight(*LandscapeActor, *PartForSizing, *PartForSizing->CachedData);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Set label. Doing this earlier results in Unreal errors as the Landscape is not fully initialized.
		//---------------------------------------------------------------------------------------------------------------------------------

		LandscapeActor->SetActorLabel(CookingActorName);

		//---------------------------------------------------------------------------------------------------------------------------------
		// World Partition
		//---------------------------------------------------------------------------------------------------------------------------------

		SetWorldPartitionGridSize(LandscapeActor, PartForSizing->SizeInfo.WorldPartitionGridSize);

		//---------------------------------------------------------------------------------------------------------------------------------
		// and store the results.
		//---------------------------------------------------------------------------------------------------------------------------------

		for (auto PartIt : PartsForLandscape)
		{
			FHoudiniHeightFieldPartData* Part = PartIt.Value;
			Result.HoudiniLayerToUnrealLandscape.Add(Part, Result.TargetLandscapes.Num());
		}
		FHoudiniUnrealLandscapeTarget Output;
		Output.Proxy = LandscapeActor;
		Output.BakedName = FName(LandscapeActorName);
		Output.CreatedLayerInfoObjects = CreateLayerInfoObjects;
		Output.bWasCreated = true;
		Output.Dimensions = PartForSizing->SizeInfo.UnrealSize;
		Result.TargetLandscapes.Add(Output);
		Result.CreatedPackages = CreatedPackages;
	}

	//--------------------------------------------------------------------------------------------------------------------------
	// For existing landscape, go through and apply new data. eg. material assignments
	//--------------------------------------------------------------------------------------------------------------------------

	for (auto It : ExistingLandscapes)
	{
		ALandscapeProxy* LandscapeProxy = It.Key;
		TMap<FString, FHoudiniHeightFieldPartData*>& PartsForLandscape = It.Value;

		ApplyMaterialsFromParts(LandscapeProxy, PartsForLandscape, PackageParams, Result.CreatedPackages);

	}

	return Result;
}

void FHoudiniLandscapeUtils::CreateDefaultHeightField(ALandscape* LandscapeActor, const FHoudiniLandscapeCreationInfo& Info)
{
	// Create an height field of zeros.

	TArray<uint16> Values;
	int NumPoints = (Info.UnrealSize.X + 1) * (Info.UnrealSize.Y + 1);
	Values.SetNumUninitialized(NumPoints);
	uint16 ZeroHeight = LandscapeDataAccess::GetTexHeight(0.0f);
	for (int Index = 0; Index < NumPoints; Index++)
		Values[Index] = ZeroHeight;

	TMap<FGuid, TArray<uint16>> HeightMapDataPerLayers;
	HeightMapDataPerLayers.Add(FGuid(), Values);

	// Create Material Layer data.

	TArray<FLandscapeImportLayerInfo> CustomImportLayerInfos;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	MaterialLayerDataPerLayer.Add(FGuid(), CustomImportLayerInfos);

	// Now call the UE Import() function to actually create the layer
	LandscapeActor->Import(
		LandscapeActor->GetLandscapeGuid(),
		0, 0, Info.UnrealSize.X, Info.UnrealSize.Y,
		Info.NumSectionsPerComponent,
		Info.NumQuadsPerSection,
		HeightMapDataPerLayers,
		NULL,
		MaterialLayerDataPerLayer,
		ELandscapeImportAlphamapType::Layered);
}

ALandscapeProxy* FHoudiniLandscapeUtils::FindTargetLandscapeProxy(const FString& ActorName, UWorld* World,
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
		}
	}

	if (InputIndex != INDEX_NONE)
	{
		if (!LandscapeInputs.IsValidIndex(InputIndex))
			return nullptr;
		return LandscapeInputs[InputIndex];
	}

	return FHoudiniEngineRuntimeUtils::FindActorInWorldByLabelOrName<ALandscapeProxy>(World, ActorName);
}

FHoudiniHeightFieldPartData* FHoudiniLandscapeUtils::GetPartWithHeightData(TMap<FString, FHoudiniHeightFieldPartData*> & Parts)
{
	FString PartNameForHeight = "height";

	if (Parts.Find(PartNameForHeight))
		return Parts[PartNameForHeight];
	else
		return nullptr;
}

FTransform FHoudiniLandscapeUtils::GetHeightFieldTransformInUnrealSpace(const FHoudiniVolumeInfo& VolumeInfo)
{
	FTransform Result;
	Result.SetIdentity();


	Result.SetLocation(VolumeInfo.Transform.GetLocation());

	// Unreal has a X/Y resolution of 1m per point while Houdini is dependent on the height field's grid spacing
	// Swap Y/Z axis from H to UE
	FVector LandscapeScale;
	LandscapeScale.X = VolumeInfo.Transform.GetScale3D().X * 2.0f;
	LandscapeScale.Y = VolumeInfo.Transform.GetScale3D().Z * 2.0f;
	LandscapeScale.Z = VolumeInfo.Transform.GetScale3D().Y * 2.0f;
	LandscapeScale *= 100.0f;

	Result.SetScale3D(LandscapeScale);

	// Rotate the vector using the H rotation	
	FRotator Rotator = VolumeInfo.Transform.GetRotation().Rotator();
	// We need to compensate for the "default" HF Transform
	Rotator.Yaw -= 90.0f;
	Rotator.Roll += 90.0f;

	// Only rotate if the rotator is far from zero
	if (!Rotator.IsNearlyZero())
		Result.SetRotation(FQuat(Rotator));
	return Result;

}


UMaterialInterface*
FHoudiniLandscapeUtils::AssignGraphicsMaterialsToLandscape(
		ALandscapeProxy* LandscapeProxy, 
		FHoudiniLandscapeMaterial& Materials, 
		const FHoudiniPackageParams& Params,
		TArray<UPackage*> & CreatedPackages)
{
	UMaterialInterface* MaterialInstance = nullptr;

	if (!Materials.Material.IsEmpty())
	{
		UMaterialInterface* Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
			nullptr, *(Materials.Material), nullptr, LOAD_NoWarn, nullptr));

		if (!IsValid(Material))
		{
			HOUDINI_LOG_ERROR(TEXT("Could not load material: %s"), *Materials.Material);
		}

		if (Materials.bCreateMaterialInstance && IsValid(Material))
		{
			MaterialInstance = CreateMaterialInstance(LandscapeProxy->GetName(), Material, Params, CreatedPackages);
			Material = MaterialInstance;
		}

		LandscapeProxy->LandscapeMaterial = Material;
	}

	if (!Materials.HoleMaterial.IsEmpty())
	{
		UMaterialInterface* Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
			nullptr, *(Materials.HoleMaterial), nullptr, LOAD_NoWarn, nullptr));
		LandscapeProxy->LandscapeHoleMaterial = Material;

		if (!IsValid(Material))
		{
			HOUDINI_LOG_ERROR(TEXT("Could not load material: %s"), *Materials.HoleMaterial);
		}

	}

	LandscapeProxy->GetLandscapeActor()->ForceUpdateLayersContent();

	return MaterialInstance;
}

void FHoudiniLandscapeUtils::AssignPhysicsMaterialsToLandscape(ALandscapeProxy* LandscapeProxy, const FString& LayerName, FHoudiniLandscapeMaterial& Materials)
{
	if (Materials.PhysicalMaterial.IsEmpty())
		return;

	UPhysicalMaterial* Material = Cast<UPhysicalMaterial>(StaticLoadObject(UPhysicalMaterial::StaticClass(),
		nullptr, *(Materials.PhysicalMaterial), nullptr, LOAD_NoWarn, nullptr));

	ULandscapeLayerInfoObject* TargetLayerInfo = LandscapeProxy->GetLandscapeInfo()->GetLayerInfoByName(FName(LayerName));
	if (!TargetLayerInfo)
	{
		HOUDINI_LOG_ERROR(TEXT("Missing target layer: %s"), *LayerName);
		return;
	}

	TargetLayerInfo->PhysMaterial = Material;


}

TArray<ULandscapeLayerInfoObject*> FHoudiniLandscapeUtils::CreateTargetLayerInfoAssets(
	ALandscapeProxy* LandscapeProxy, 
	const FHoudiniPackageParams& PackageParams, 
	TMap<FString, FHoudiniHeightFieldPartData*>& PartsForLandscape,
	TArray<UPackage*>& CreatedPackages)
{
	TArray<ULandscapeLayerInfoObject*> Results;

	auto * LandscapeInfo = LandscapeProxy->GetLandscapeInfo();

	FHoudiniPackageParams LayerPackageParams = PackageParams;

	for(int Index = 0; Index < LandscapeInfo->Layers.Num(); Index++)
	{
		FLandscapeInfoLayerSettings & TargetLayerSettings = LandscapeInfo->Layers[Index];
		if (!IsValid(TargetLayerSettings.LayerInfoObj))
		{
			FString TargetLayerName = TargetLayerSettings.LayerName.ToString();

			ULandscapeLayerInfoObject* Layer = nullptr;

			if (PartsForLandscape.Contains(TargetLayerName) &&
				!PartsForLandscape[TargetLayerName]->LayerInfoObjectName.IsEmpty())
			{
				// Load an existing layer object if the user specified it.
				Layer = LoadObject<ULandscapeLayerInfoObject>(
					nullptr, *PartsForLandscape[TargetLayerName]->LayerInfoObjectName, nullptr, LOAD_None, nullptr);
			}
			else
			{
				// Normally we create packages with a name based off geo/part ids. But this doesn't make sense here
				// as we're creating a layer info based off the material and name of the landscape.
				ALandscape * ParentLandscape = LandscapeProxy->GetLandscapeActor();
				FString PackageName = ParentLandscape->GetName() + FString("_") + TargetLayerName;
				FString PackagePath = LayerPackageParams.GetPackagePath();
				UPackage* Package = nullptr;
				Layer = FindOrCreateLandscapeLayerInfoObject(TargetLayerName, PackagePath, PackageName, Package);
				CreatedPackages.Add(Package);
			}

			if (IsValid(Layer))
			{
				Results.Add(Layer);
			}
			LandscapeProxy->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(Layer));
		}
	}

	return Results;
}

UPackage* FHoudiniLandscapeUtils::FindOrCreate(const FString& PackageFullPath)
{
	UPackage* OutPackage;

	// See if package exists, if it does, reuse it
	OutPackage = FindPackage(nullptr, *PackageFullPath);
	if (!IsValid(OutPackage))
		OutPackage = CreatePackage(*PackageFullPath);

	if (!IsValid(OutPackage))
		return nullptr;

	if (!OutPackage->IsFullyLoaded())
		OutPackage->FullyLoad();

	return OutPackage;
}

ULandscapeLayerInfoObject*
FHoudiniLandscapeUtils::FindOrCreateLandscapeLayerInfoObject(const FString & InLayerName, const FString & InPackagePath, const FString & InPackageName, UPackage * &OutPackage)
{
	FString PackageFullName = InPackagePath + TEXT("/") + InPackageName;
	OutPackage = FindOrCreate(PackageFullName);
	if (!OutPackage)
		return nullptr;

	ULandscapeLayerInfoObject* LayerInfo =  NewObject<ULandscapeLayerInfoObject>(OutPackage, FName(*InPackageName), RF_Public | RF_Standalone /*| RF_Transactional*/);

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(LayerInfo);

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

void FHoudiniLandscapeUtils::SetWorldPartitionGridSize(ALandscape* LandscapeProxy, int WorldPartitionGridSize)
{
	UWorld * World = LandscapeProxy->GetWorld();
	if (!World->IsPartitionedWorld())
		return;

	FLandscapeConfig Config(LandscapeProxy->GetLandscapeInfo());

	if (Config.GridSizeInComponents == WorldPartitionGridSize)
		return;

	if (World->IsPartitionedWorld())
	{
		TSet<AActor*> ActorsToDelete;
		FLandscapeConfigHelper::ChangeGridSize(LandscapeProxy->GetLandscapeInfo(), WorldPartitionGridSize, ActorsToDelete);
		for (AActor* ActorToDelete : ActorsToDelete)
		{
			World->DestroyActor(ActorToDelete);
		}
	}
}


FHoudiniExtents FHoudiniLandscapeUtils::GetLandscapeExtents(ALandscapeProxy* LandscapeProxy)
{
	FHoudiniExtents Extents;

	// Get the landscape X/Y Size
	Extents.Min.X = MAX_int32;
	Extents.Min.Y = MAX_int32;
	Extents.Max.X = -MAX_int32;
	Extents.Max.Y = -MAX_int32;

	ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
	if (LandscapeProxy == Landscape)
	{
		// The proxy is a landscape actor, so we have to use the landscape extent (landscape components
		// may have been moved to proxies and may not be present on this actor).
		Landscape->GetLandscapeInfo()->GetLandscapeExtent(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y);
	}
	else
	{
		// We only want to get the data for this landscape proxy.
		// To handle streaming proxies correctly, get the extents via all the components,
		// not by calling GetLandscapeExtent or we'll end up sending ALL the streaming proxies.
		for (const ULandscapeComponent* Comp : LandscapeProxy->LandscapeComponents)
		{
			Comp->GetComponentExtent(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y);
		}
	}

	return Extents;
}

FHoudiniExtents
FHoudiniLandscapeUtils::GetExtents(
	const ALandscape* TargetLandscape,
	const FHoudiniHeightFieldData& HeightFieldData)
{
	FHoudiniExtents Extents;

	const FTransform TargetLandscapeTransform = TargetLandscape->GetActorTransform();

	FTransform UnscaledLandscapeTransform = TargetLandscapeTransform;
	UnscaledLandscapeTransform.SetScale3D(FVector::OneVector);

	const FTransform RelativeTileTransform = HeightFieldData.Transform * UnscaledLandscapeTransform.Inverse();

	FVector LandscapeScale = TargetLandscapeTransform.GetScale3D();
	const FVector RelativeTileCoordinate = RelativeTileTransform.GetLocation() / LandscapeScale;

	const FIntPoint LandscapeBaseLoc = TargetLandscape->GetSectionBaseOffset();

	// Calculate the final draw coordinates

	FIntPoint TargetTileLoc;
	TargetTileLoc.X = LandscapeBaseLoc.X + FMath::RoundToInt(RelativeTileCoordinate.X);
	TargetTileLoc.Y = LandscapeBaseLoc.Y + FMath::RoundToInt(RelativeTileCoordinate.Y);

	Extents.Min.X = TargetTileLoc.X;
	Extents.Min.Y = TargetTileLoc.Y;
	Extents.Max.X = TargetTileLoc.X + HeightFieldData.Dimensions.X - 1;
	Extents.Max.Y = TargetTileLoc.Y + HeightFieldData.Dimensions.Y - 1;

	return Extents;
}

FIntPoint
FHoudiniLandscapeUtils::GetVolumeDimensionsInUnrealSpace(const FHoudiniGeoPartObject& HeightField)
{
	FIntPoint Dimension;
	Dimension.X = HeightField.VolumeInfo.YLength;
	Dimension.Y = HeightField.VolumeInfo.XLength;
	return Dimension;
}

FHoudiniHeightFieldData FHoudiniLandscapeUtils::FetchVolumeInUnrealSpace(const FHoudiniGeoPartObject& HeightField, bool bTansposeData)
{
	FHoudiniHeightFieldData Result;
	Result.Transform = GetHeightFieldTransformInUnrealSpace(HeightField.VolumeInfo);
	Result.Dimensions = GetVolumeDimensionsInUnrealSpace(HeightField);

	TArray<float> HoudiniValues;
	HoudiniValues.SetNumZeroed(Result.GetNumPoints());
	Result.Values.SetNumZeroed(HoudiniValues.Num());

	auto Status = FHoudiniEngineUtils::HapiGetHeightFieldData(
							HeightField.GeoId, HeightField.PartId, HoudiniValues);
	HOUDINI_CHECK_RETURN(Status == HAPI_RESULT_SUCCESS, Result);


	Result.Values.SetNum(HoudiniValues.Num());

	if (bTansposeData)
	{
		int Offset = 0;
		for(int Y = 0; Y < Result.Dimensions.Y; Y++)
		{
			for (int X = 0; X < Result.Dimensions.X; X++)
			{
				int HIndex = Y + Result.Dimensions.Y * X;
				Result.Values[Offset++] = HoudiniValues[HIndex];
			}
		}
	}
	else
	{
		Result.Values = HoudiniValues;
	}

	return Result;
}

FHoudiniHeightFieldData
FHoudiniLandscapeUtils::ReDimensionLandscape(const FHoudiniHeightFieldData & HeightField, FIntPoint NewDimensions)
{
	FHoudiniHeightFieldData Result;
	Result.Transform = HeightField.Transform;
	Result.Dimensions = NewDimensions;
	Result.Values.SetNumZeroed(Result.GetNumPoints());

	const float XScale = (float)(HeightField.Dimensions.X - 1) / (Result.Dimensions.X - 1);
	const float YScale = (float)(HeightField.Dimensions.Y - 1) / (Result.Dimensions.Y - 1);
	for (int32 Y = 0; Y < Result.Dimensions.Y; ++Y)
	{
		for (int32 X = 0; X < Result.Dimensions.X; ++X)
		{
			float OldY = Y * YScale;
			float OldX = X * XScale;
			int32 X0 = FMath::FloorToInt(OldX);
			int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, HeightField.Dimensions.X - 1);
			int32 Y0 = FMath::FloorToInt(OldY);
			int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, HeightField.Dimensions.Y - 1);
			float Original00 = HeightField.Values[Y0 * HeightField.Dimensions.X + X0];
			float Original10 = HeightField.Values[Y0 * HeightField.Dimensions.X + X1];
			float Original01 = HeightField.Values[Y1 * HeightField.Dimensions.X + X0];
			float Original11 = HeightField.Values[Y1 * HeightField.Dimensions.X + X1];
			float NewValue = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
			Result.Values[Y * Result.Dimensions.X + X] = NewValue;

		}
	}

	return Result;
}


FHoudiniMinMax
FHoudiniLandscapeUtils::GetHeightFieldRange(const FHoudiniHeightFieldData& HeightField)
{
	FHoudiniMinMax Range;
	for(float Value : HeightField.Values)
	{
		Range.Add(Value);
	}
	return Range;
}


float FHoudiniLandscapeUtils::GetAbsRange(const FHoudiniMinMax& Range, float MaxUsableRange)
{
	// Divide them by the useable range
	float MinValue = Range.MinValue / MaxUsableRange;
	float MaxValue = Range.MaxValue / MaxUsableRange;

	// Find the absolute range of values.
	float AbsRange = FMath::Max(FMath::Abs(MinValue), FMath::Abs(MaxValue));

	return AbsRange;
}

void FHoudiniLandscapeUtils::AdjustLandscapeTransformToLayerHeight(ALandscape& TargetLandscape, const FHoudiniHeightFieldPartData& LayerData, const FHoudiniHeightFieldData& HeightFieldData)
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	if (!HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
	{
		// If we are applying the height field for the first time we must work out scaling. The range of values

		// Get the max absolute range of the landscape in meters.
		FHoudiniMinMax Range = FHoudiniLandscapeUtils::GetHeightFieldRange(HeightFieldData);
		const float MaxUseableRange = HoudiniRuntimeSettings->MarshallingLandscapesUseFullResolution ? 1.0f : 0.75f;

		float MaxAbsRange = FHoudiniLandscapeUtils::GetAbsRange(Range, MaxUseableRange);

		// a UE Landscape can hold is -256cm -> + 256cm. To get larger scales we adjust the Landscape Actor's
		// transform z-scale. So to get a range of -512cm -> 512cm we would set a scale of 2.0. Also,
		// multiple by 100.0 to convert Houdini Meters to Centimeters.

		float TransformScale = 100.0f * MaxAbsRange / 256.0f;
		if (TransformScale < 100.0f)
		{
			// Don't go smaller than the default scale.
			return;
		}

		FTransform Transform = TargetLandscape.GetTransform();
		auto Scale = Transform.GetScale3D();
		Scale.Z = TransformScale;
		Transform.SetScale3D(Scale);
		TargetLandscape.SetActorTransform(Transform);
	}
	else if (LayerData.HeightRange.IsSet())
	{
		FHoudiniMinMax HeightRange = LayerData.HeightRange.GetValue();
		FTransform Transform = TargetLandscape.GetTransform();

		// Move the position of the landscape to be in the middle of the range.
		auto Position = Transform.GetLocation();
		Position.Z = 0.5f * (HeightRange.MinValue + HeightRange.MaxValue);
		Transform.SetLocation(Position);

		// Adjust the scale to accomodate the full range. The default range of -1 to 1
		// encompasses 256cm, so scale accordingly.
		auto Scale = Transform.GetScale3D();
		Scale.Z = HeightRange.Diff() / 256.0f;
		Transform.SetScale3D(Scale);

		TargetLandscape.SetActorTransform(Transform);
	}
}

TSet<FString>
FHoudiniLandscapeUtils::GetNonWeightBlendedLayerNames(const FHoudiniGeoPartObject& InHGPO)
{
	TSet<FString> Results;

	// Check the attribute exists on primitive or detail
	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS, HAPI_ATTROWNER_PRIM))
		Owner = HAPI_ATTROWNER_PRIM;
	else if (FHoudiniEngineUtils::HapiCheckAttributeExists(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS, HAPI_ATTROWNER_DETAIL))
		Owner = HAPI_ATTROWNER_DETAIL;
	else
		return Results;

	// Get the values
	HAPI_AttributeInfo AttribInfoNonWBLayer;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNonWBLayer);
	TArray<FString> AttribValues;
	FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS, AttribInfoNonWBLayer, AttribValues, 1, Owner);

	if (AttribValues.Num() <= 0)
		return Results;

	// Convert them to FString
	for (int32 Idx = 0; Idx < AttribValues.Num(); Idx++)
	{
		TArray<FString> Tokens;
		AttribValues[Idx].ParseIntoArray(Tokens, TEXT(" "), true);

		for (int32 n = 0; n < Tokens.Num(); n++)
			Results.Add(Tokens[n]);
	}

	return Results;
}

FTransform FHoudiniLandscapeUtils::GetLandscapeActorTransformFromTileTransform(const FTransform& TileTransform, const FHoudiniTileInfo& TileInfo)
{
	FTransform Result = TileTransform;

	FVector3d OffsetX = TileTransform.GetScaledAxis(EAxis::Y) * TileInfo.TileStart.Y;
	FVector3d OffsetY = TileTransform.GetScaledAxis(EAxis::X) * TileInfo.TileStart.X;

	Result.SetLocation(Result.GetLocation() - OffsetX - OffsetY);
	return Result;
}

ULandscapeLayerInfoObject*
FHoudiniLandscapeUtils::GetLandscapeLayerInfoForLayer(const FHoudiniGeoPartObject& Part, const FName& InLayerName)
{
	// See if we have assigned a landscape layer info object to this layer via attribute
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	TArray<FString> AttributeValues;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		Part.GeoId, Part.PartId, 
		HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_INFO,
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

bool FHoudiniLandscapeUtils::GetOutputMode(int GeoId, int PartId, HAPI_AttributeOwner Owner, int& LandscapeOutputMode)
{
	return FHoudiniEngineUtils::HapiGetFirstAttributeValueAsInteger(
		GeoId, PartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_OUTPUT_MODE,
		Owner,
		LandscapeOutputMode);
}

UMaterialInterface*
FHoudiniLandscapeUtils::CreateMaterialInstance(
		const FString & Prefix, 
		UMaterialInterface* Material, 
		const FHoudiniPackageParams& Params, 
		TArray<UPackage*>& CreatedPackages)
{
	if (!IsValid(Material))
	{
		return nullptr;
	}


	// Factory to create materials.
	UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject< UMaterialInstanceConstantFactoryNew >();
	if (!MaterialInstanceFactory)
		return nullptr;


	FString MaterialName = Prefix + FString("_") + Material->GetName() + TEXT("_instance");
	MaterialName = UPackageTools::SanitizePackageName(MaterialName);

	FString MaterialInstanceName;

	FHoudiniPackageParams MaterialParams = Params;
	MaterialParams.ObjectName = MaterialName;
	UPackage* MaterialPackage = MaterialParams.CreatePackageForObject(MaterialInstanceName);

	// Create the new material instance
	MaterialInstanceFactory->AddToRoot();
	MaterialInstanceFactory->InitialParent = Material;
	auto MaterialInstance = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(
		UMaterialInstanceConstant::StaticClass(), MaterialPackage, FName(*MaterialInstanceName),
		RF_Public | RF_Standalone, NULL, GWarn);

	MaterialInstanceFactory->RemoveFromRoot();

	FAssetRegistryModule::AssetCreated(MaterialInstance);
	MaterialPackage->MarkPackageDirty();
	CreatedPackages.Add(MaterialPackage);

	return MaterialInstance;
}

void FHoudiniLandscapeUtils::ApplyMaterialsFromParts(
		ALandscapeProxy* LandscapeProxy, 
		TMap<FString, FHoudiniHeightFieldPartData*> & Parts, 
		const FHoudiniPackageParams& PackageParams,
		TArray<UPackage*>& CreatedPackages)
{
	// The user can specify a different materials per part, but this is an error.
	// Pick the first one we find. TODO: Add checks to see if they conflict.

	for(auto & It : Parts)
	{
		It.Value->MaterialInstance = AssignGraphicsMaterialsToLandscape(
								LandscapeProxy, It.Value->Materials, PackageParams, CreatedPackages);

		AssignPhysicsMaterialsToLandscape(LandscapeProxy, It.Value->TargetLayerName, It.Value->Materials);

		break; // just the first one.
	}
	



}


bool
FHoudiniLandscapeUtils::ApplyLandscapeSplinesToReservedLayer(ALandscape* const InLandscape)
{
	if (!IsValid(InLandscape) || !InLandscape->GetLandscapeSplinesReservedLayer())
		return false;

	InLandscape->RequestSplineLayerUpdate();

	return true;
}


bool
FHoudiniLandscapeUtils::ApplySegmentsToLandscapeEditLayers(
	const TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& InSegmentsToApplyToLayers)
{
	bool bSuccess = true;
	for (const auto& Entry : InSegmentsToApplyToLayers)
	{
		ALandscape* const Landscape = Entry.Key.Key;
		const FName LayerName = Entry.Key.Value;
		const FHoudiniLandscapeSplineApplyLayerData& LayerData = Entry.Value;

		if (!IsValid(Landscape) || LayerName == NAME_None)
			continue;

		// For landscapes with reserved layers all splines of the landscape are applied to the reserved layer
		if (LayerData.bIsReservedSplineLayer)
		{
			if (!ApplyLandscapeSplinesToReservedLayer(Landscape))
				bSuccess = false;
			continue;
		}

		FLandscapeLayer const* const Layer = Landscape->GetLayer(LayerName);
		if (!Layer)
		{
			HOUDINI_LOG_WARNING(
				TEXT("Layer '%s' unexpectedly not found on landscape '%s': cannot apply splines to layer."),
				*LayerName.ToString(), *Landscape->GetFName().ToString());
			continue;
		}

		// Not a landscape + reserved layer, so we must select each segment and its control points and then apply it
		// to the specified edit layer
		if (LayerData.SegmentsToApply.Num() == 0)
			continue;

		// Select the segments and their control points
		for (ULandscapeSplineSegment* const Segment : LayerData.SegmentsToApply)
		{
			if (!IsValid(Segment))
				continue;

			Segment->SetSplineSelected(true);

			ULandscapeSplineControlPoint* const CP0 = Segment->Connections[0].ControlPoint;
			if (IsValid(CP0))
				CP0->SetSplineSelected(true);

			ULandscapeSplineControlPoint* const CP1 = Segment->Connections[1].ControlPoint;
			if (IsValid(CP1))
				CP1->SetSplineSelected(true);
		}

		// Apply splines to layer
		static constexpr bool bUpdateOnlySelected = true;
		Landscape->UpdateLandscapeSplines(Layer->Guid, bUpdateOnlySelected);

		// Unselect the segments and their control points
		for (ULandscapeSplineSegment* const Segment : LayerData.SegmentsToApply)
		{
			if (!IsValid(Segment))
				continue;

			Segment->SetSplineSelected(false);

			ULandscapeSplineControlPoint* const CP0 = Segment->Connections[0].ControlPoint;
			if (IsValid(CP0))
				CP0->SetSplineSelected(false);

			ULandscapeSplineControlPoint* const CP1 = Segment->Connections[1].ControlPoint;
			if (IsValid(CP1))
				CP1->SetSplineSelected(false);
		}
	}

	return bSuccess;
}

void FHoudiniLandscapeUtils::ApplyLocks(UHoudiniLandscapeTargetLayerOutput* Output)
{
	if (!Output->bLockLayer)
		return;

	int32 EditLayerIndex = Output->Landscape->GetLayerIndex(FName(Output->CookedEditLayer));
	if (EditLayerIndex == INDEX_NONE)
		return;

	FLandscapeLayer* UnrealEditLayer = Output->Landscape->GetLayer(EditLayerIndex);
	UnrealEditLayer->bLocked = true;
}
