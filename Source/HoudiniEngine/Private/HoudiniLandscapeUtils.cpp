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
#include "HoudiniCookable.h"
#include "Landscape.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "UObject/UObjectGlobals.h"
#include "LandscapeDataAccess.h"
#include "HoudiniAsset.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineTimers.h"
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
#include "LandscapeUtils.h"
#include "Async/ParallelFor.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "LandscapeEditLayer.h"
#endif

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
FHoudiniLandscapeUtils::GetCookedLandscapeLayers(UHoudiniCookable& HC, ALandscape& Landscape)
{
	TSet<FString> Layers;
	for(int OutputIndex = 0; OutputIndex < HC.GetNumOutputs(); OutputIndex++)
	{
		UHoudiniOutput* Output 	= HC.GetOutputAt(OutputIndex);
		if (!IsValid(Output))
			continue;

		TSet<UHoudiniLandscapeTargetLayerOutput*> LandscapeEditLayers = GetEditLayers(*Output);
		for(UHoudiniLandscapeTargetLayerOutput* Layer : LandscapeEditLayers)
		{
			if (Layer->Landscape == &Landscape)
				Layers.Add(Layer->CookedEditLayer);
		}
	}
	return Layers;
}

void
FHoudiniLandscapeUtils::SetNonCookedLayersVisibility(UHoudiniCookable& HC, ALandscape& Landscape, bool bVisible)
{
	FString LayerName;
	TSet<FString> CookedLayers = GetCookedLandscapeLayers(HC, Landscape);	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	TArrayView<const FLandscapeLayer> Layers = Landscape.GetLayersConst();
	for (int LayerIndex = 0; LayerIndex < Landscape.GetLayersConst().Num(); LayerIndex++)
	{
		LayerName = Layers[LayerIndex].EditLayer->GetName().ToString();
		if (!CookedLayers.Contains(LayerName))
		{
			// Non cooked Layer
			Layers[LayerIndex].EditLayer->SetVisible(bVisible, true);
		}
	}
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	TArrayView<const FLandscapeLayer> Layers = Landscape.GetLayers();
	for (int LayerIndex = 0; LayerIndex < Landscape.GetLayerCount(); LayerIndex++)
	{
		LayerName = Layers[LayerIndex].Name.ToString();
		if (!CookedLayers.Contains(LayerName))
		{
			// Non cooked Layer
			Landscape.SetLayerVisibility(LayerIndex, bVisible);
		}
	}
#else
	for (int LayerIndex = 0; LayerIndex < Landscape.GetLayerCount(); LayerIndex++)
	{
		LayerName = Landscape.LandscapeLayers[LayerIndex].Name.ToString();
		if (!CookedLayers.Contains(LayerName))
		{
			// Non cooked Layer
			Landscape.SetLayerVisibility(LayerIndex, bVisible);
		}
	}
#endif
}

void
FHoudiniLandscapeUtils::SetCookedLayersVisibility(UHoudiniCookable& HC, ALandscape& Landscape, bool bVisible)
{
	FString LayerName;
	TSet<FString> CookedLayers = GetCookedLandscapeLayers(HC, Landscape);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	TArrayView<const FLandscapeLayer> Layers = Landscape.GetLayersConst();
	for (int LayerIndex = 0; LayerIndex < Landscape.GetLayersConst().Num(); LayerIndex++)
	{
		LayerName = Layers[LayerIndex].EditLayer->GetName().ToString();
		if (CookedLayers.Contains(LayerName))
		{
			// Cooked Layer
			Layers[LayerIndex].EditLayer->SetVisible(bVisible, true);
		}
	}
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	TArrayView<const FLandscapeLayer> Layers = Landscape.GetLayers();
	for (int LayerIndex = 0; LayerIndex < Landscape.GetLayerCount(); LayerIndex++)
	{
		LayerName = Layers[LayerIndex].Name.ToString();
		if (CookedLayers.Contains(LayerName))
		{
			// Cooked Layer
			Landscape.SetLayerVisibility(LayerIndex, bVisible);
		}
	}
#else
	for (int LayerIndex = 0; LayerIndex < Landscape.GetLayerCount(); LayerIndex++)
	{
		LayerName = Landscape.LandscapeLayers[LayerIndex].Name.ToString();
		if (CookedLayers.Contains(LayerName))
		{
			// Cooked Layer
			Landscape.SetLayerVisibility(LayerIndex, bVisible);
		}
	}
#endif
}

void FHoudiniLandscapeUtils::RealignHeightFieldData(TArray<float>& Data, float ZeroPoint, float Scale)
{
	H_SCOPED_FUNCTION_TIMER();

	for(int Index = 0; Index < Data.Num(); Index++)
	{
		Data[Index] = Data[Index] * Scale + ZeroPoint;
	}
}


bool FHoudiniLandscapeUtils::ClampHeightFieldData(TArray<float>& Data, float MinValue, float MaxValue)
{
	H_SCOPED_FUNCTION_TIMER();

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
	H_SCOPED_FUNCTION_TIMER();

	TArray<uint16> Result;
	Result.SetNumUninitialized(Data.Num());
	ParallelFor(Result.Num(), [&](int Index) {
		int Quantized = static_cast<int>(Data[Index] * 65535);
		Result[Index] = FMath::Clamp<int>(Quantized, 0, 65535);
	});

	return Result;
}

float 
FHoudiniLandscapeUtils::GetLandscapeHeightRangeInCM(const ALandscape& Landscape)
{
	float Scale = Landscape.GetTransform().GetScale3D().Z;

	return Scale * 256.0f;

}

TArray<uint16> 
FHoudiniLandscapeUtils::GetHeightData(
	ALandscape* Landscape,
	const FHoudiniExtents& Extents,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* EditLayer)
#else
	FLandscapeLayer* EditLayer)
#endif
{
	int DiffX = 1 + Extents.Max.X - Extents.Min.X;
	int DiffY = 1 + Extents.Max.Y - Extents.Min.Y;
	int NumPoints = DiffX * DiffY;

	TArray<uint16> Values;
	Values.SetNum(NumPoints);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	FGuid LayerGuid = EditLayer->EditLayer->GetGuid();
#else
	FGuid LayerGuid = EditLayer->Guid;
#endif
	FScopedSetLandscapeEditingLayer Scope(Landscape, LayerGuid, [&] { /*Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); */});

	FLandscapeEditDataInterface LandscapeEdit(Landscape->GetLandscapeInfo());
	LandscapeEdit.SetShouldDirtyPackage(false);
	LandscapeEdit.GetHeightDataFast(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData(), 0);

	return Values;
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
const FLandscapeLayer*
#else
FLandscapeLayer*
#endif
FHoudiniLandscapeUtils::GetOrCreateEditLayer(ALandscape* Landscape, const FName& LayerName, bool* bCreated)
{
	if (bCreated)
		*bCreated = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* UnrealEditLayer = GetEditLayer(Landscape, LayerName);
#else
	FLandscapeLayer* UnrealEditLayer = GetEditLayer(Landscape, LayerName);
#endif
	if (UnrealEditLayer == nullptr)
	{
		int EditLayerIndex = Landscape->CreateLayer(LayerName);
		if (EditLayerIndex == INDEX_NONE)
		{
			HOUDINI_LOG_ERROR(TEXT("Could not create edit layer %s"), *LayerName.ToString());
			return nullptr;
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		UnrealEditLayer = Landscape->GetLayerConst(EditLayerIndex);
#else
		UnrealEditLayer = Landscape->GetLayer(EditLayerIndex);
#endif
		if (bCreated)
		*bCreated = true;
	}
	return UnrealEditLayer;
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
const FLandscapeLayer*
#else
FLandscapeLayer*
#endif
FHoudiniLandscapeUtils::GetEditLayer(ALandscape* Landscape, const FName& LayerName)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	// Landscape layers are mandatory since 5.7
	// Nothing to do
#else
	if (!Landscape->bCanHaveLayersContent)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		return Landscape->GetLayerConst(0);
#else
		return Landscape->GetLayer(0);
#endif
#endif

	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	if (EditLayerIndex == INDEX_NONE)
		return nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	return Landscape->GetLayerConst(EditLayerIndex);
#else
	return Landscape->GetLayer(EditLayerIndex);
#endif
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
const FLandscapeLayer*
#else
FLandscapeLayer*
#endif
FHoudiniLandscapeUtils::MoveEditLayerAfter(ALandscape* Landscape, const FName& LayerName, const FName& AfterLayerName)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	// Landscape layers are mandatory since 5.7
	// Nothing to do
#else
	if (!Landscape->bCanHaveLayersContent)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		return Landscape->GetLayerConst(0);
#else
		return Landscape->GetLayer(0);
#endif
#endif

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	return Landscape->GetLayerConst(EditLayerIndex);
#else
	return Landscape->GetLayer(EditLayerIndex);
#endif
}

TArray<uint8_t>
FHoudiniLandscapeUtils::GetLayerData(ALandscape* Landscape, const FHoudiniExtents& Extents, const FName& EditLayerName, const FName& TargetLayerName)
{
	int DiffX = 1 + Extents.Max.X - Extents.Min.X;
	int DiffY = 1 + Extents.Max.Y - Extents.Min.Y;
	int NumPoints = DiffX * DiffY;

	TArray<uint8_t> Values;
	Values.SetNum(NumPoints);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayer(Landscape, EditLayerName);
#else
	FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayer(Landscape, EditLayerName);
#endif
	ULandscapeLayerInfoObject* TargetLayerInfo = Landscape->GetLandscapeInfo()->GetLayerInfoByName(TargetLayerName);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	FGuid LayerGuid = EditLayer->EditLayer->GetGuid();
#else
	FGuid LayerGuid = EditLayer->Guid;
#endif
	FScopedSetLandscapeEditingLayer Scope(Landscape, LayerGuid, [&] { /*Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); */});

	FLandscapeEditDataInterface LandscapeEdit(Landscape->GetLandscapeInfo());
	LandscapeEdit.SetShouldDirtyPackage(false);
	LandscapeEdit.GetWeightDataFast(TargetLayerInfo, Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData(), 0);

	return Values;
}


bool
FHoudiniLandscapeUtils::CalcLandscapeSizeFromHeightFieldSize(
	int32 ProposedUnrealSizeX, int32 ProposedUnrealSizeY, 
	FHoudiniLandscapeCreationInfo & Info)
{
	// TODO: We already know Proposed Size will fit, so some of this function is redundant.

	if ((ProposedUnrealSizeX < 2) || (ProposedUnrealSizeY < 2))
		return false;

	Info.NumSectionsPerComponent = 1;
	Info.NumQuadsPerSection = 1;
	Info.UnrealGridDimensions.X = -1;
	Info.UnrealGridDimensions.Y = -1;

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
		Info.UnrealGridDimensions.X = 512;
		Info.UnrealGridDimensions.Y = 512;
		Info.NumSectionsPerComponent = 1;
		Info.NumQuadsPerSection = 511;
		ComponentsCountX = 1;
		ComponentsCountY = 1;
	}
	else
	{
		// Calculating the desired size
		int32 QuadsPerComponent = Info.NumSectionsPerComponent * Info.NumQuadsPerSection;

		Info.UnrealGridDimensions.X = ComponentsCountX * QuadsPerComponent + 1;
		Info.UnrealGridDimensions.Y = ComponentsCountY * QuadsPerComponent + 1;
	}

	return bFoundMatch;
}

FHoudiniLayersToUnrealLandscapeMapping
FHoudiniLandscapeUtils::ResolveLandscapes(
	const FString& CookedLandscapePrefix,
	const FHoudiniPackageParams& PackageParams, 
	const FHoudiniLandscapeSettings& LandscapeSettings,
	TMap<FString, ALandscape*>& LandscapeMap,
	TArray<FHoudiniHeightFieldPartData>& Parts, 
	UWorld * World, 
	const TArray<ALandscapeProxy*>& LandscapeInputs)
{
	H_SCOPED_FUNCTION_TIMER();

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
				//LandscapeMap[Part.TargetLandscapeName];
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

		FHoudiniHeightFieldPartData* HeightPart = GetPartWithHeightData(PartsForLandscape);
		if (!HeightPart)
		{
    		HeightPart = PartsForLandscape.CreateIterator().Value();
			HOUDINI_BAKING_WARNING(TEXT("No height primitve was found, using %s"), *BaseLayer->TargetLayer);
		}

		auto LayerPackageParams = PackageParams;
		LayerPackageParams.ObjectId = HeightPart->ObjectId;
		LayerPackageParams.GeoId = HeightPart->GeoId;
		LayerPackageParams.PartId = HeightPart->PartId;
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

		FTransform LocalHeightFieldTransform = GetHeightFieldTransformInUnrealSpace(HeightPart->HeightField->VolumeInfo, HeightPart->SizeInfo.UnrealGridDimensions);

		if (HeightPart->TileInfo.IsSet())
		{
			// Adjust the transform of the Landscape actor we are creating if this is a single tile.
			LocalHeightFieldTransform = GetLandscapeActorTransformFromTileTransform(LocalHeightFieldTransform, HeightPart->TileInfo.GetValue());
		}
		FTransform LandscapeTransform = LocalHeightFieldTransform * HeightPart->Transform * LandscapeSettings.LocalToWorldTransform;
		LandscapeActor->SetActorTransform(LandscapeTransform);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Initial settings for landscapes created by Houdini in Unreal.
		//---------------------------------------------------------------------------------------------------------------------------------

		LandscapeActor->PreEditChange(nullptr);
		LandscapeActor->SetLandscapeGuid(FGuid::NewGuid());
		LandscapeActor->bCastStaticShadow = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		// Landscape layers are mandatory since 5.7 - Nothing to do
#else
		LandscapeActor->bCanHaveLayersContent = true;
#endif

		//---------------------------------------------------------------------------------------------------------------------------------
		// Order is important: Assign materials, create landscape info, Create TargetLayerInfo assets.
		//---------------------------------------------------------------------------------------------------------------------------------

		TArray<UPackage*> CreatedPackages;

		HeightPart->MaterialInstance = AssignGraphicsMaterialsToLandscape(
			LandscapeActor, 
			HeightPart->Materials, 
			PackageParams, 
			CreatedPackages);

		LandscapeActor->CreateLandscapeInfo();

		//---------------------------------------------------------------------------------------------------------------------------------
		// Fetch the data for the height field and use to create the landscape.
		//---------------------------------------------------------------------------------------------------------------------------------

		FHoudiniHeightFieldData HeightFieldData = FHoudiniLandscapeUtils::FetchVolumeInUnrealSpace(
			*HeightPart->HeightField, HeightPart->SizeInfo.UnrealGridDimensions, true);

		HeightFieldData = FHoudiniLandscapeUtils::ReDimensionLandscape(HeightFieldData, HeightPart->SizeInfo.UnrealGridDimensions);

		FHoudiniLandscapeUtils::AdjustLandscapeTransformToLayerHeight(*LandscapeActor, *HeightPart, HeightFieldData);

		TArray<uint16> QuantizedData = FHoudiniLandscapeUtils::ConvertHeightFieldData(LandscapeActor, HeightFieldData.Values);


		ImportLandscape(LandscapeActor, HeightPart->SizeInfo, QuantizedData);


		TArray<ULandscapeLayerInfoObject*> CreateLayerInfoObjects =
			CreateTargetLayerInfoAssets(LandscapeActor, PackageParams, PartsForLandscape, CreatedPackages);

		// Rename the default height layer if needed.
		const FString DefaultLayerName = TEXT("Layer");
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		// Landscape layers are mandatory since 5.7
		if (HeightPart->UnrealLayerName != DefaultLayerName)
		{
			LandscapeActor->GetEditLayer(0)->SetName(FName(HeightPart->UnrealLayerName), true);
		}
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		if (LandscapeActor->HasLayersContent() && HeightPart->UnrealLayerName != DefaultLayerName)
		{
			LandscapeActor->GetEditLayer(0)->SetName(FName(HeightPart->UnrealLayerName), true);
		}
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 5
		if (LandscapeActor->HasLayersContent() && HeightPart->UnrealLayerName != DefaultLayerName)
		{
			LandscapeActor->SetLayerName(0, FName(HeightPart->UnrealLayerName));
		}
#else
		if (!LandscapeActor->LandscapeLayers.IsEmpty() && HeightPart->UnrealLayerName != DefaultLayerName)
		{
			LandscapeActor->LandscapeLayers[0].Name = FName(HeightPart->UnrealLayerName);
		}
#endif

		//---------------------------------------------------------------------------------------------------------------------------------
		// Set label. Doing this earlier results in Unreal errors as the Landscape is not fully initialized.
		//---------------------------------------------------------------------------------------------------------------------------------

		LandscapeActor->SetActorLabel(CookingActorName);


		//---------------------------------------------------------------------------------------------------------------------------------
		// World Partition
		//---------------------------------------------------------------------------------------------------------------------------------

		SetWorldPartitionGridSize(LandscapeActor, HeightPart->SizeInfo.WorldPartitionGridSize);

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
		Output.Dimensions = HeightPart->SizeInfo.UnrealGridDimensions;
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

void FHoudiniLandscapeUtils::ImportLandscape(ALandscape* LandscapeActor, const FHoudiniLandscapeCreationInfo& Info, const TArray<uint16>& Values)
{
	TMap<FGuid, TArray<uint16>> HeightMapDataPerLayers;
	HeightMapDataPerLayers.Add(FGuid(), Values);

	// Create Material Layer data.

	TArray<FLandscapeImportLayerInfo> CustomImportLayerInfos;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	MaterialLayerDataPerLayer.Add(FGuid(), CustomImportLayerInfos);

	// Now call the UE Import() function to actually create the layer
	LandscapeActor->Import(
		LandscapeActor->GetLandscapeGuid(),
		0, 0, Info.UnrealGridDimensions.X - 1, Info.UnrealGridDimensions.Y - 1,
		Info.NumSectionsPerComponent,
		Info.NumQuadsPerSection,
		HeightMapDataPerLayers,
		NULL,
		MaterialLayerDataPerLayer,
		ELandscapeImportAlphamapType::Layered
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		,MakeArrayView<FLandscapeLayer>({})
#endif
	);
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

FTransform FHoudiniLandscapeUtils::GetHeightFieldTransformInUnrealSpace(const FHoudiniVolumeInfo& VolumeInfo, const FIntPoint & UnrealDimensions)
{
	FTransform Result;
	Result.SetIdentity();

	Result.SetLocation(VolumeInfo.Transform.GetLocation());

	// Unreal has a X/Y resolution of 1m per point while Houdini is dependent on the height field's grid spacing
	// Swap Y/Z axis from H to UE. We must also take into account that the landscape may have been resized.

	FVector LandscapeScale;
	LandscapeScale.X = VolumeInfo.Transform.GetScale3D().X * 2.0f * (VolumeInfo.YLength - 1) / (UnrealDimensions.X - 1);
	LandscapeScale.Y = VolumeInfo.Transform.GetScale3D().Z * 2.0f * (VolumeInfo.XLength - 1) / (UnrealDimensions.Y - 1);

	// NOTE: Ignore vertical scaling intentionally; the height field grid scale is also applied to the volume's scale.y
	// received from HAPI, however the actual height values do not change. So we can ignore it.
	LandscapeScale.Z = 1.0f;
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
#if ENGINE_MAJOR_VERSION  >= 5 && ENGINE_MINOR_VERSION  >= 5
	TArray<ULandscapeLayerInfoObject*> Results;

	auto* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();

	FHoudiniPackageParams LayerPackageParams = PackageParams;

	TSet<FName> LayerNames;
	LandscapeProxy->GetLandscapeInfo()->ForEachLandscapeProxy([&LayerNames](ALandscapeProxy* LandscapeProxy)
		{
			LayerNames.Append(LandscapeProxy->RetrieveTargetLayerNamesFromMaterials());
			return true;
		});

	auto LandscapeTargetLayers = LandscapeProxy->GetTargetLayers();
	for(FName TargetLayerName : LayerNames)
	{
		// if the landscape info already exists, don't create one.
		if(LandscapeTargetLayers.Find(TargetLayerName))
			continue;

		// if the user did not specify the target info, do not create it
		if(!PartsForLandscape.Contains(TargetLayerName.ToString()))
			continue;

		// Normally we create packages with a name based off geo/part ids. But this doesn't make sense here
		// as we're creating a layer info based off the material and name of the landscape.
		ALandscape* ParentLandscape = LandscapeProxy->GetLandscapeActor();
		FString PackageName = ParentLandscape->GetName() + FString("_") + TargetLayerName.ToString();
		FString PackagePath = LayerPackageParams.GetPackagePath();
		UPackage* Package = nullptr;
		ULandscapeLayerInfoObject* LandscapeLayerInfo = FindOrCreateLandscapeLayerInfoObject(TargetLayerName.ToString(), PackagePath, PackageName, Package);
		CreatedPackages.Add(Package);

		FLandscapeTargetLayerSettings LayerSettings(LandscapeLayerInfo);
		LandscapeProxy->AddTargetLayer(TargetLayerName, LayerSettings);
		Results.Add(LandscapeLayerInfo);
	}

	LandscapeInfo->UpdateLayerInfoMap(LandscapeProxy, false);

	return Results;
#else
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
			if (PartsForLandscape.Contains(TargetLayerName))
			{
				if (!PartsForLandscape[TargetLayerName]->LayerInfoObjectName.IsEmpty())
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
			}

			if (IsValid(Layer))
			{
				Results.Add(Layer);
				LandscapeProxy->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(Layer));
				TargetLayerSettings.LayerInfoObj = Layer;
			}

		}
	}

	LandscapeInfo->UpdateLayerInfoMap(LandscapeProxy, false);

	return Results;
#endif
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

#if ENGINE_MAJOR_VERSION  >= 5 && ENGINE_MINOR_VERSION  >= 7
	const FIntPoint LandscapeBaseLoc = TargetLandscape->GetSectionBase();
#else
	const FIntPoint LandscapeBaseLoc = TargetLandscape->GetSectionBaseOffset();
#endif

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

void FHoudiniLandscapeUtils::TransposeValues(TArray<float> & Values, const FIntPoint & Dimensions)
{
	H_SCOPED_FUNCTION_TIMER();

	TArray<float> Result;
	Result.SetNumUninitialized(Values.Num());

	ParallelFor(Dimensions.Y, [&](int Y)
	{
		for(int X = 0; X < Dimensions.X; X++)
		{
			int Index1 = X + Dimensions.X * Y;
			int Index2 = Y + Dimensions.Y * X;
			Result[Index1] = Values[Index2];
		}
	});
	Values = Result;
}

FHoudiniHeightFieldData FHoudiniLandscapeUtils::FetchVolumeInUnrealSpace(
	const FHoudiniGeoPartObject& HeightField, 
	const FIntPoint& UnrealLandscapeDimensions,
	bool bFetchData)
{
	H_SCOPED_FUNCTION_TIMER();

	FHoudiniHeightFieldData Result;
	Result.Dimensions = GetVolumeDimensionsInUnrealSpace(HeightField);
	Result.Transform = GetHeightFieldTransformInUnrealSpace(HeightField.VolumeInfo, UnrealLandscapeDimensions);

	if (bFetchData)
	{
		// We have to fetch the volume before fetching height field data so HAPI knows which volume we're using.
		HAPI_VolumeInfo VolumeInfo;
		FHoudiniApi::GetVolumeInfo(FHoudiniEngine::Get().GetSession(), HeightField.GeoId, HeightField.PartId, &VolumeInfo);

		FHoudiniHapiAccessor Accessor(HeightField.GeoId, HeightField.PartId, "");

		bool bSuccess = Accessor.GetHeightFieldData(Result.Values, Result.GetNumPoints());

		if (!bSuccess)
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to fetch height field data"));
			Result.Values.SetNumZeroed(Result.GetNumPoints());

		}

		TransposeValues(Result.Values, Result.Dimensions);
	}

	return Result;
}

FHoudiniHeightFieldData
FHoudiniLandscapeUtils::ReDimensionLandscape(const FHoudiniHeightFieldData & HeightField, FIntPoint NewDimensions)
{
	H_SCOPED_FUNCTION_TIMER();

	FHoudiniHeightFieldData Result;
	Result.Transform = HeightField.Transform;
	Result.Dimensions = NewDimensions;
	Result.Values.SetNumZeroed(Result.GetNumPoints());

	const float XScale = (float)(HeightField.Dimensions.X - 1) / (Result.Dimensions.X - 1);
	const float YScale = (float)(HeightField.Dimensions.Y - 1) / (Result.Dimensions.Y - 1);

	ParallelFor(Result.Dimensions.Y, [&](int Y)
	{
		float OldY = Y * YScale;
		int32 Y0 = FMath::FloorToInt(OldY);
		int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, HeightField.Dimensions.Y - 1);

		for (int32 X = 0; X < Result.Dimensions.X; ++X)
		{
			float OldX = X * XScale;
			int32 X0 = FMath::FloorToInt(OldX);
			int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, HeightField.Dimensions.X - 1);
			float Original00 = HeightField.Values[Y0 * HeightField.Dimensions.X + X0];
			float Original10 = HeightField.Values[Y0 * HeightField.Dimensions.X + X1];
			float Original01 = HeightField.Values[Y1 * HeightField.Dimensions.X + X0];
			float Original11 = HeightField.Values[Y1 * HeightField.Dimensions.X + X1];
			float NewValue = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
			Result.Values[Y * Result.Dimensions.X + X] = NewValue;
		}
	}
	);

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
	TArray<FString> AttribValues;
	FHoudiniHapiAccessor Accessor(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_NONWEIGHTBLENDED_LAYERS);
	Accessor.GetAttributeData(Owner, 1,  AttribValues);

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

bool FHoudiniLandscapeUtils::GetOutputMode(int GeoId, int PartId, HAPI_AttributeOwner Owner, int& LandscapeOutputMode)
{
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_OUTPUT_MODE);
	bool bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, LandscapeOutputMode);
	return bSuccess;
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
	if (!IsValid(InLandscape)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		|| !InLandscape->FindEditLayerOfType(ULandscapeEditLayerSplines::StaticClass()))
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		|| !InLandscape->FindLayerOfType(ULandscapeEditLayerSplines::StaticClass()))
#else
		|| !InLandscape->GetLandscapeSplinesReservedLayer())
#endif
	{
		return false;
	}

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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		FLandscapeLayer const* const Layer = Landscape->GetLayerConst(LayerName);
#else
		FLandscapeLayer const* const Layer = Landscape->GetLayer(LayerName);
#endif

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		Landscape->UpdateLandscapeSplines(Layer->EditLayer->GetGuid(), false);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		static constexpr bool bUpdateOnlySelected = true;
		Landscape->UpdateLandscapeSplines(Layer->EditLayer->GetGuid(), bUpdateOnlySelected);
#else
		static constexpr bool bUpdateOnlySelected = true;
		Landscape->UpdateLandscapeSplines(Layer->Guid, bUpdateOnlySelected);
#endif
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

void FHoudiniLandscapeUtils::DeleteCookedLayer(UHoudiniLandscapeTargetLayerOutput* Layer)
{
	int32 EditLayerIndex = Layer->Landscape->GetLayerIndex(FName(Layer->CookedEditLayer));
	if(EditLayerIndex == INDEX_NONE)
		return;

	Layer->Landscape->DeleteLayer(EditLayerIndex);

}

void FHoudiniLandscapeUtils::ApplyLocks(UHoudiniLandscapeTargetLayerOutput* Output)
{
	if (!Output->bLockLayer)
		return;

	int32 EditLayerIndex = Output->Landscape->GetLayerIndex(FName(Output->CookedEditLayer));
	if (EditLayerIndex == INDEX_NONE)
		return;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	Output->Landscape->GetEditLayer(EditLayerIndex)->SetLocked(true, true);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 5
	Output->Landscape->SetLayerLocked(EditLayerIndex, true);
#else
	FLandscapeLayer* UnrealEditLayer = Output->Landscape->GetLayer(EditLayerIndex);
	UnrealEditLayer->bLocked = true;
#endif
}

bool
FHoudiniLandscapeUtils::NormalizePaintLayers(TArray<float>& Data, bool bNormalize)
{
	H_SCOPED_FUNCTION_TIMER();

	if (Data.Num() == 0)
		return false;

	float MaxValue = Data[0];
	bool bExceedsRange = false;

	// Scan data to see if any value exceeds 1.0, while keeping track of the max values.
	for(float & Value : Data)
	{
		MaxValue = FMath::Max(Value, MaxValue);

		if (Value > 1.0)
		{
			bExceedsRange = true;
		}
	}

	if (!bExceedsRange)
		return false;

	if (bNormalize)
	{
		for (float & Value : Data)
		{
			if (Value < 0.0f)
				Value = 0.0f;
			else
				Value = Value / MaxValue;
		}
	}
	else
	{
		// If value exceeded range and not normalizing, clamp
		for (float & Value : Data)
		{
			Value = FMath::Clamp(Value, 0.0f, 1.0f);
		}

	}
	return true;

}

TArray<uint16> FHoudiniLandscapeUtils::ConvertHeightFieldData(const ALandscape* LandscapeActor, const TArray<float>& Values)
{
	H_SCOPED_FUNCTION_TIMER();

	float Range = FHoudiniLandscapeUtils::GetLandscapeHeightRangeInCM(*LandscapeActor);

	float Scale = 100.0f; // Scale from Meters to CM.
	Scale /= Range; // Remap to -1.0f to 1.0 Range

	TArray<float> AlignedValues = Values;
	FHoudiniLandscapeUtils::RealignHeightFieldData(AlignedValues, 0.5f, Scale * 0.5f);

	// Explicitly clamp the values, and report if clamped.
	bool bClamped = FHoudiniLandscapeUtils::ClampHeightFieldData(AlignedValues, 0.0, 1.0f);
	if (bClamped)
	{
		HOUDINI_BAKING_WARNING(TEXT("Landscape layer exceeded max heights so was clamped."));
	}

	// Quantized to 16-bit and set the data.
	auto QuantizedData = FHoudiniLandscapeUtils::QuantizeNormalizedDataTo16Bit(AlignedValues);
	return QuantizedData;
}
