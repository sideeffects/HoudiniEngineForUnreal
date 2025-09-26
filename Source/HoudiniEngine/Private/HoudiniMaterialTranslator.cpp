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

#include "HoudiniMaterialTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniTextureTranslator.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#include "Materials/MaterialParameters.h"
#else
#include "MaterialTypes.h"
#endif
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "ImageUtils.h"
#include "PackageTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/MetaData.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "MaterialShared.h"
#endif
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Serialization/BufferWriter.h"

#if WITH_EDITOR
	#include "Factories/MaterialFactoryNew.h"
	#include "Factories/MaterialInstanceConstantFactoryNew.h"
#endif

const int32 FHoudiniMaterialTranslator::MaterialExpressionNodeX = -400;
const int32 FHoudiniMaterialTranslator::MaterialExpressionNodeY = -150;
const int32 FHoudiniMaterialTranslator::MaterialExpressionNodeStepX = 220;
const int32 FHoudiniMaterialTranslator::MaterialExpressionNodeStepY = 220;

// Helper to get StaticParameters from UMaterialInterface in <=5.1
// This copied from 5.3's UMaterialInterface::GetStaticParameterValues() function
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 1)
void GetStaticParameterValues(UMaterialInterface* MaterialInterface, FStaticParameterSet& OutStaticParameters)
{
	if (!IsValid(MaterialInterface))
		return;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
	TArray<FStaticSwitchParameter>& StaticSwitchParameters = OutStaticParameters.EditorOnly.StaticSwitchParameters;
#else
	TArray<FStaticSwitchParameter>& StaticSwitchParameters = OutStaticParameters.StaticSwitchParameters;
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	TArray<FStaticComponentMaskParameter>& StaticComponentMaskParameters = OutStaticParameters.EditorOnly.StaticComponentMaskParameters;
#else
	TArray<FStaticComponentMaskParameter>& StaticComponentMaskParameters = OutStaticParameters.StaticComponentMaskParameters;
#endif
	TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ParameterValues;
	for (int32 ParameterTypeIndex = 0; ParameterTypeIndex < NumMaterialParameterTypes; ++ParameterTypeIndex)
	{
		const EMaterialParameterType ParameterType = (EMaterialParameterType)ParameterTypeIndex;
		if (IsStaticMaterialParameter(ParameterType))
		{
			ParameterValues.Reset();
			MaterialInterface->GetAllParametersOfType(ParameterType, ParameterValues);
			// OutStaticParameters.AddParametersOfType(ParameterType, ParameterValues);
			// Copied the code from FStaticParameterSet::AddParametersOfType due to missing API export
			switch (ParameterType)
			{
			case EMaterialParameterType::StaticSwitch:
				StaticSwitchParameters.Empty(ParameterValues.Num());
				for (const auto& It : ParameterValues)
				{
					const FMaterialParameterMetadata& Meta = It.Value;
					check(Meta.Value.Type == ParameterType);
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2)
					if(!Meta.bDynamicSwitchParameter)
					{
						StaticSwitchParameters.Emplace(It.Key, Meta.Value.AsStaticSwitch(), Meta.bOverride, Meta.ExpressionGuid);
					}
#else
					StaticSwitchParameters.Emplace(It.Key, Meta.Value.AsStaticSwitch(), Meta.bOverride, Meta.ExpressionGuid);
#endif
				}
				break;
			case EMaterialParameterType::StaticComponentMask:
				StaticComponentMaskParameters.Empty(ParameterValues.Num());
				for (const auto& It : ParameterValues)
				{
					const FMaterialParameterMetadata& Meta = It.Value;
					check(Meta.Value.Type == ParameterType);
					StaticComponentMaskParameters.Emplace(It.Key,
						Meta.Value.Bool[0],
						Meta.Value.Bool[1],
						Meta.Value.Bool[2],
						Meta.Value.Bool[3],
						Meta.bOverride,
						Meta.ExpressionGuid);
				}
				break;
			default: checkNoEntry();
			}
		}
	}

	if(UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface))
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
		if (UMaterialInstanceEditorOnlyData* EditorOnly = MaterialInstance->GetEditorOnlyData())
		{
			OutStaticParameters.EditorOnly.TerrainLayerWeightParameters = EditorOnly->StaticParameters.TerrainLayerWeightParameters;
		}
#else
		OutStaticParameters.TerrainLayerWeightParameters = MaterialInstance->GetStaticParameters().TerrainLayerWeightParameters;
#endif
	}

	FMaterialLayersFunctions MaterialLayers;
	OutStaticParameters.bHasMaterialLayers = MaterialInterface->GetMaterialLayers(MaterialLayers);
	if (OutStaticParameters.bHasMaterialLayers)
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
		OutStaticParameters.MaterialLayers = MoveTemp(MaterialLayers.GetRuntime());
		OutStaticParameters.EditorOnly.MaterialLayers = MoveTemp(MaterialLayers.EditorOnly);
#else
		OutStaticParameters.MaterialLayers = MoveTemp(MaterialLayers);
#endif
	}

	// OutStaticParameters.Validate();
	// Copied the code from FStaticParameterSet::Validate() due to missing API export
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
	FMaterialLayersFunctions::Validate(OutStaticParameters.MaterialLayers, OutStaticParameters.EditorOnly.MaterialLayers);
#endif
}
#endif

FHoudiniMaterialParameterValue::FHoudiniMaterialParameterValue()
	: ParamType(EHoudiniUnrealMaterialParameterType::Invalid)
	, DataType(EHoudiniUnrealMaterialParameterDataType::Invalid)
	, ByteValue(0)
	, FloatValue(0)
	, StringValue()
	, VectorValue(0, 0, 0, 0)
{
}

void 
FHoudiniMaterialParameterValue::SetValue(const uint8 InValue) 
{ 
	DataType = EHoudiniUnrealMaterialParameterDataType::Byte; 
	ByteValue = InValue;
	CleanValue();
}

void 
FHoudiniMaterialParameterValue::SetValue(const float InValue) 
{ 
	DataType = EHoudiniUnrealMaterialParameterDataType::Float; 
	FloatValue = InValue; 
	CleanValue();
}

void 
FHoudiniMaterialParameterValue::SetValue(const FString& InValue)
{ 
	DataType = EHoudiniUnrealMaterialParameterDataType::String; 
	StringValue = InValue; 
	CleanValue();
}

void 
FHoudiniMaterialParameterValue::SetValue(const FLinearColor& InValue)
{ 
	DataType = EHoudiniUnrealMaterialParameterDataType::Vector; 
	VectorValue = InValue; 
	CleanValue();
}

void
FHoudiniMaterialParameterValue::CleanValue()
{
	if (DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
		ByteValue = 0;
	if (DataType != EHoudiniUnrealMaterialParameterDataType::Float)
		FloatValue = 0;
	if (DataType != EHoudiniUnrealMaterialParameterDataType::String)
		StringValue.Empty();
	if (DataType != EHoudiniUnrealMaterialParameterDataType::Vector)
		VectorValue = {0, 0, 0, 0};
}

FString
FHoudiniMaterialInfo::MakeMaterialInstanceParametersSlug() const
{
	if (!bMakeMaterialInstance)
		return FString();

	TArray<FName> Keys;
	MaterialInstanceParameters.GetKeys(Keys);
	Algo::SortBy(Keys, [](const FName& InName)
		{
			return InName.ToString().ToLower();
		}
	);

	FBufferWriter Ar(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	int32 NumKeys = Keys.Num();
	Ar << NumKeys;
	for (const FName& Key : Keys)
	{
		FHoudiniMaterialParameterValue ParamValue = MaterialInstanceParameters.FindChecked(Key);
		ParamValue.CleanValue();
		FString Name = Key.ToString().ToLower();
		Ar << Name;
		FHoudiniMaterialParameterValue::StaticStruct()->SerializeBin(Ar, &ParamValue);
	}
	
	const FString OutputString = BytesToString(static_cast<uint8*>(Ar.GetWriterData()), Ar.Tell());
	Ar.Close();

	return OutputString;
}

FHoudiniMaterialIdentifier
FHoudiniMaterialInfo::MakeIdentifier() const
{
	return FHoudiniMaterialIdentifier(
		MaterialObjectPath,
		bMakeMaterialInstance,
		MakeMaterialInstanceParametersSlug()
	);
}

bool
FHoudiniMaterialTranslator::CreateHoudiniMaterials(
	const HAPI_NodeId& InAssetId,
	const FHoudiniPackageParams& InPackageParams,
	const TArray<int32>& InUniqueMaterialIds,
	const TArray<HAPI_MaterialInfo>& InUniqueMaterialInfos,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutMaterials,
	TArray<UMaterialInterface*>& OutMaterialArray,
	TArray<UPackage*>& OutPackages,
	const bool& bForceRecookAll,
	bool bInTreatExistingMaterialsAsUpToDate,
	bool bAddDefaultMaterial)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMaterialTranslator::CreateHoudiniMaterials);

	if (InUniqueMaterialIds.Num() <= 0)
		return false;

	if (InUniqueMaterialInfos.Num() != InUniqueMaterialIds.Num())
		return false;

	// Empty returned materials.
	OutMaterials.Empty();

	// Update context for generated materials (will trigger when object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	// Default Houdini material.
	if (bAddDefaultMaterial)
	{
		UMaterial* DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
		OutMaterials.Add(
			FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false),
			DefaultMaterial);
	}

	// Factory to create materials.
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	MaterialFactory->AddToRoot();

	OutMaterialArray.SetNumZeroed(InUniqueMaterialIds.Num());

	for (int32 MaterialIdx = 0; MaterialIdx < InUniqueMaterialIds.Num(); MaterialIdx++)
	{
		HAPI_NodeId MaterialId = (HAPI_NodeId)InUniqueMaterialIds[MaterialIdx];
		
		const HAPI_MaterialInfo& MaterialInfo = InUniqueMaterialInfos[MaterialIdx];
		if (!MaterialInfo.exists)
		{
			// The material does not exist,
			// we will use the default Houdini material in this case.
			continue;
		}

		// Get the material node's node information.
		HAPI_NodeInfo NodeInfo;
		FHoudiniApi::NodeInfo_Init(&NodeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), 
			MaterialInfo.nodeId, &NodeInfo))
		{
			continue;
		}

		FString MaterialName = TEXT("");
		if (!FHoudiniEngineString::ToFString(NodeInfo.nameSH, MaterialName))
		{
			// shouldnt happen, give a generic name
			HOUDINI_LOG_WARNING(TEXT("Failed to retrieve material name!"));
			MaterialName = TEXT("Material_") + FString::FromInt(MaterialInfo.nodeId);
		}

		FString MaterialPathName = TEXT("");
		if (!FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, MaterialInfo, MaterialPathName))
			continue;
		const FHoudiniMaterialIdentifier MaterialIdentifier(MaterialPathName, true);
		
		// Check first in the existing material map
		UMaterial* Material = nullptr;
		const TObjectPtr<UMaterialInterface> * FoundMaterial = InMaterials.Find(MaterialIdentifier);
		bool bCanReuseExistingMaterial = false;
		if (FoundMaterial)
		{
			bCanReuseExistingMaterial = (bInTreatExistingMaterialsAsUpToDate || !MaterialInfo.hasChanged) && !bForceRecookAll;
			Material = Cast<UMaterial>(*FoundMaterial);
		}
		
		if(!Material || !bCanReuseExistingMaterial)
		{
			// Try to see if another output/part of this HDA has already recreated this material
			// Since those materials have just been recreated, they are considered up to date and can always be reused.
			FoundMaterial = InAllOutputMaterials.Find(MaterialIdentifier);
			if (FoundMaterial)
			{
				Material = Cast<UMaterial>(*FoundMaterial);
				bCanReuseExistingMaterial = true;
			}
		}

		// Check that the existing material is in the expected directory (temp folder could have been changed between
		// cooks).
		if (IsValid(Material) && !InPackageParams.HasMatchingPackageDirectories(Material))
		{
			bCanReuseExistingMaterial = false;
			Material = nullptr;
		}
		
		bool bCreatedNewMaterial = false;
		if (IsValid(Material))
		{
			// If the cached material exists and is up to date, we can reuse it.
			if (bCanReuseExistingMaterial)
			{
				OutMaterialArray[MaterialIdx] = Material;
				OutMaterials.Add(MaterialIdentifier, Material);
				continue;
			}
		}
		else
		{
			// Previous Material was not found, we need to create a new one.
			EObjectFlags ObjFlags = RF_Public | RF_Standalone;

			// Create material package and get material name.
			FString MaterialPackageName;
			UPackage* MaterialPackage = FHoudiniMaterialTranslator::CreatePackageForMaterial(
				MaterialInfo.nodeId, MaterialName, InPackageParams, MaterialPackageName);

			Material = (UMaterial*)MaterialFactory->FactoryCreateNew(
				UMaterial::StaticClass(), MaterialPackage, *MaterialPackageName, ObjFlags, NULL, GWarn);

			// Add meta information to this package.
			FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
				MaterialPackage, Material, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
			FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
				MaterialPackage, Material, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName);

			bCreatedNewMaterial = true;
		}

		if (!IsValid(Material))
			continue;

		OutMaterialArray[MaterialIdx] = Material;

		// Get the asset name from the package params
		FString AssetName = InPackageParams.HoudiniAssetName.IsEmpty() ? TEXT("HoudiniAsset") : InPackageParams.HoudiniAssetName;

		// Get the package and add it to our list
		UPackage* Package = Material->GetOutermost();
		OutPackages.AddUnique(Package);

		/*
		// TODO: This should be handled in the mesh/instance translator
		// If this is an instancer material, enable the instancing flag.
		if (UniqueInstancerMaterialIds.Contains(MaterialId))
			Material->bUsedWithInstancedStaticMeshes = true;
			*/

		// Reset material expressions.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		Material->GetExpressionCollection().Empty(); 
#else
		Material->Expressions.Empty();
#endif

		// Generate various components for this material.
		bool bMaterialComponentCreated = false;
		int32 MaterialNodeY = FHoudiniMaterialTranslator::MaterialExpressionNodeY;

		// By default we mark material as opaque. Some of component creators can change this.
		Material->BlendMode = BLEND_Opaque;

		// Extract diffuse plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentDiffuse(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract metallic plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentMetallic(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract specular plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentSpecular(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract roughness plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentRoughness(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract emissive plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentEmissive(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract opacity plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentOpacity(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract opacity mask plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentOpacityMask(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Extract normal plane.
		bMaterialComponentCreated |= FHoudiniMaterialTranslator::CreateMaterialComponentNormal(
			InAssetId, AssetName, MaterialInfo, InPackageParams, Material, OutPackages, MaterialNodeY);

		// Set other material properties.
		Material->TwoSided = true;
		Material->SetShadingModel(MSM_DefaultLit);

		// Schedule this material for update.
		MaterialUpdateContext.AddMaterial(Material);

		// Cache material.
		OutMaterials.Add(MaterialIdentifier, Material);

		// Propagate and trigger material updates.
		if (bCreatedNewMaterial)
			FAssetRegistryModule::AssetCreated(Material);

		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}

	MaterialFactory->RemoveFromRoot();

	return true;
}

//
bool
FHoudiniMaterialTranslator::CreateMaterialInstances(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniMaterialIdentifier, FHoudiniMaterialInfo>& UniqueMaterialInstanceOverrides,
	const TArray<UPackage*>& InPackages,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InMaterials,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutMaterials,
	const bool& bForceRecookAll)
{
	// Check the node ID is valid
	if (InHGPO.AssetId < 0)
		return false;

	// No material instance attributes
	if (UniqueMaterialInstanceOverrides.Num() <= 0)
		return false;

	for (const auto& Entry : UniqueMaterialInstanceOverrides)
	{
		const FHoudiniMaterialIdentifier& Identifier = Entry.Key;
		const FHoudiniMaterialInfo& MatInfo = Entry.Value;

		const uint32 InstanceParametersGUID = FCrc::StrCrc32(*Identifier.MaterialInstanceParametersSlug);
		
		if (!MatInfo.bMakeMaterialInstance)
			continue;

		// Try to find the material we want to create an instance of
		UMaterialInterface* CurrentSourceMaterialInterface = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MatInfo.MaterialObjectPath, nullptr, LOAD_NoWarn, nullptr));
		
		if (!IsValid(CurrentSourceMaterialInterface))
		{
			// Couldn't find the source material
			HOUDINI_LOG_WARNING(TEXT("Couldn't find the source material %s to create a material instance."), *MatInfo.MaterialObjectPath);
			continue;
		}

		// Create/Retrieve the package for the MI
		FString MaterialInstanceName;
		FString MaterialInstanceNamePrefix = UPackageTools::SanitizePackageName(
			CurrentSourceMaterialInterface->GetName() + TEXT("_instance_") + FString::Printf(TEXT("%u"), InstanceParametersGUID));

		// See if we can find an existing package for that instance
		UPackage* MaterialInstancePackage = nullptr;
		const TObjectPtr<UMaterialInterface> * FoundMatPtr = InMaterials.Find(Identifier);
		if (FoundMatPtr && *FoundMatPtr)
		{
			// We found an already existing MI, get its package
			MaterialInstancePackage = Cast<UPackage>((*FoundMatPtr)->GetOuter());
		}

		if (MaterialInstancePackage)
		{
			MaterialInstanceName = MaterialInstancePackage->GetName();
		}
		else
		{
			// We couldnt find the corresponding M_I package, so create a new one
			MaterialInstancePackage = CreatePackageForMaterial(InHGPO.AssetId, MaterialInstanceNamePrefix, InPackageParams, MaterialInstanceName);
		}

		// Couldn't create a package for that Material Instance
		if (!MaterialInstancePackage)
			continue;

		bool bNewMaterialCreated = false;
		UMaterialInstanceConstant* NewMaterialInstance = LoadObject<UMaterialInstanceConstant>(MaterialInstancePackage, *MaterialInstanceName, nullptr, LOAD_None, nullptr);
		if (!NewMaterialInstance)
		{
			// Factory to create materials.
			UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
			if (!MaterialInstanceFactory)
				continue;

			// Create the new material instance
			MaterialInstanceFactory->AddToRoot();
			MaterialInstanceFactory->InitialParent = CurrentSourceMaterialInterface;
			NewMaterialInstance = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(
				UMaterialInstanceConstant::StaticClass(), MaterialInstancePackage, FName(*MaterialInstanceName),
				RF_Public | RF_Standalone, NULL, GWarn);

			if (NewMaterialInstance)
				bNewMaterialCreated = true;

			MaterialInstanceFactory->RemoveFromRoot();
		}

		if (!NewMaterialInstance)
		{
			HOUDINI_LOG_WARNING(TEXT("Couldn't access the material instance for %s"), *MatInfo.MaterialObjectPath);
			continue;
		}			

		// Update context for generated materials (will trigger when the object goes out of scope).
		FMaterialUpdateContext MaterialUpdateContext;

		// Apply material instance parameters
		bool bModifiedMaterialParameters = false;
		for (const auto& MatParamEntry : MatInfo.MaterialInstanceParameters)
		{
			const FName& MaterialParameterName = MatParamEntry.Key;
			const FHoudiniMaterialParameterValue& MaterialParameterValue = MatParamEntry.Value;

			// Try to update the material instance parameter corresponding to the attribute
			if (UpdateMaterialInstanceParameter(MaterialParameterName, MaterialParameterValue, NewMaterialInstance, InPackages))
				bModifiedMaterialParameters = true;
		}

		// Schedule this material for update if needed.
		if (bNewMaterialCreated || bModifiedMaterialParameters)
			MaterialUpdateContext.AddMaterialInstance(NewMaterialInstance);

		if (bNewMaterialCreated)
		{
			// Add meta information to this package.
			FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
				MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
			FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
				MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialInstanceName);
			// Notify registry that we have created a new material.
			FAssetRegistryModule::AssetCreated(NewMaterialInstance);
		}

		if (bNewMaterialCreated || bModifiedMaterialParameters)
		{
			// Dirty the material
			NewMaterialInstance->MarkPackageDirty();

			// Update the material instance
			NewMaterialInstance->InitStaticPermutation();
			NewMaterialInstance->PreEditChange(nullptr);
			NewMaterialInstance->PostEditChange();
			/*
			// Automatically save the package to avoid further issue
			MaterialInstancePackage->SetDirtyFlag(true);
			MaterialInstancePackage->FullyLoad();
			UPackage::SavePackage(
				MaterialInstancePackage, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
				*FPackageName::LongPackageNameToFilename(MaterialInstancePackage->GetName(), FPackageName::GetAssetPackageExtension()));
				*/
		}

		// Add the created material to the output assignement map
		// Use the "source" material name as we want the instance to replace it
		OutMaterials.Add(Identifier, NewMaterialInstance);
	}

	return true;
}

bool
FHoudiniMaterialTranslator::GetMaterialParameterAttributes(
	const int32 InGeoId,
	const int32 InPartId,
	const HAPI_AttributeOwner InAttributeOwner,
	TArray<FHoudiniGenericAttribute>& OutAllMatParams,
	const int32 InAttributeIndex)
{
	ensure(InAttributeOwner == HAPI_ATTROWNER_PRIM || InAttributeOwner == HAPI_ATTROWNER_POINT || InAttributeOwner == HAPI_ATTROWNER_DETAIL);
	if (InAttributeOwner != HAPI_ATTROWNER_PRIM && InAttributeOwner != HAPI_ATTROWNER_POINT && InAttributeOwner != HAPI_ATTROWNER_DETAIL)
	{
		HOUDINI_LOG_WARNING(TEXT(
			"[FHoudiniMaterialTranslator::GetMaterialParameterAttributes] Invalid InAttributeOwner: must be detail, "
			"prim or point. Not fetching material parameters via attributes."));
		return false;
	}

	OutAllMatParams.Empty();
	// Get the detail material parameters
	FHoudiniEngineUtils::GetGenericAttributeList(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_MAT_PARAM_PREFIX,
		OutAllMatParams, HAPI_ATTROWNER_DETAIL, -1);

	if (InAttributeOwner != HAPI_ATTROWNER_DETAIL)
	{
		FHoudiniEngineUtils::GetGenericAttributeList(
			InGeoId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_MAT_PARAM_PREFIX,
			OutAllMatParams, InAttributeOwner, InAttributeIndex);
	}

	return true;
}

bool FHoudiniMaterialTranslator::GetMaterialParameters(
	FHoudiniMaterialInfo& MaterialInfo,
	const TArray<FHoudiniGenericAttribute>& InAllMatParams,
	int32 InAttributeIndex)
{
	MaterialInfo.MaterialInstanceParameters.Empty();

	// Material parameters are only relevant if we are making material instances
	if (!MaterialInfo.bMakeMaterialInstance)
		return true;

	// We have no material parameters, so nothing to do
	if (InAllMatParams.Num() <= 0)
		return true;

	// Try to find the material we want to create an instance of, so that we can determine valid parameter names
	UMaterialInterface* CurrentSourceMaterialInterface = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialInfo.MaterialObjectPath, nullptr, LOAD_NoWarn, nullptr));
	
	if (!IsValid(CurrentSourceMaterialInterface))
	{
		// Couldn't find the source material
		HOUDINI_LOG_WARNING(TEXT("Couldn't find the source material %s to create a material instance."), *MaterialInfo.MaterialObjectPath);
		return false;
	}

	const int32 NumMatParams = InAllMatParams.Num();
	for (int32 ParamIdx = 0; ParamIdx < NumMatParams; ParamIdx++)
	{
		const FHoudiniGenericAttribute& ParamOverride = InAllMatParams[ParamIdx];
		// Skip if the AttribName is empty
		if (ParamOverride.AttributeName.IsEmpty())
			continue;

		// Copy the name since we'll remove the slot prefix if present
		FString AttribName = ParamOverride.AttributeName;

		// If no index specified, assume it applies to all mats
		int32 OverrideIndex = -1;
		int32 TentativeIndex = 0;
		char CurChar = AttribName[0];
		int32 AttribNameIndex = 0;
		while (CurChar >= '0' && CurChar <= '9')
		{
			TentativeIndex *= 10;
			TentativeIndex += CurChar - '0';
			CurChar = AttribName[++AttribNameIndex];
		}
		if (CurChar == '_')
		{
			AttribName = AttribName.Mid(AttribNameIndex + 1);
			OverrideIndex = TentativeIndex;
		}

		if (OverrideIndex != -1 && OverrideIndex != MaterialInfo.MaterialIndex)
			continue;

		// Check if AttribName is a valid parameter of our source material
		FHoudiniMaterialParameterValue MaterialParameterValue;
		if (!GetAndValidateMaterialInstanceParameterValue(
				FName(AttribName), ParamOverride, InAttributeIndex, CurrentSourceMaterialInterface, MaterialParameterValue))
		{
			continue;
		}

		MaterialInfo.MaterialInstanceParameters.Add(FName(AttribName), MaterialParameterValue);
	}

	return true;
}

bool FHoudiniMaterialTranslator::GetMaterialParameters(
	TArray<FHoudiniMaterialInfo>& MaterialsByAttributeIndex,
	const int32 InGeoId,
	const int32 InPartId,
	const HAPI_AttributeOwner InAttributeOwner)
{
	ensure(InAttributeOwner == HAPI_ATTROWNER_PRIM || InAttributeOwner == HAPI_ATTROWNER_POINT);
	if (InAttributeOwner != HAPI_ATTROWNER_PRIM && InAttributeOwner != HAPI_ATTROWNER_POINT)
	{
		HOUDINI_LOG_WARNING(TEXT(
			"[FHoudiniMaterialTranslator::GetMaterialParameters] Invalid InAttributeOwner: must be prim or point. Not "
			"fetching material parameters via attributes."));
		return false;
	}

	bool bHaveMaterialInstances = false;
	for (const FHoudiniMaterialInfo& MatInfo : MaterialsByAttributeIndex)
	{
		if (!MatInfo.bMakeMaterialInstance)
			continue;

		bHaveMaterialInstances = true;
		break;
	}

	// We have no material instances, no need to process material parameters
	if (!bHaveMaterialInstances)
		return true;

	TArray<FHoudiniGenericAttribute> AllMatParams;
	// See if we need to override some of the material instance's parameters
	GetMaterialParameterAttributes(InGeoId, InPartId, InAttributeOwner, AllMatParams);

	// Map containing unique face materials override attribute
	// and their first valid prim index
	// We create only one material instance per attribute
	for (int32 AttributeIndex = 0; AttributeIndex < MaterialsByAttributeIndex.Num(); AttributeIndex++)
	{
		FHoudiniMaterialInfo& MatInfo = MaterialsByAttributeIndex[AttributeIndex];
		if (!MatInfo.bMakeMaterialInstance)
			continue;

		if (!GetMaterialParameters(MatInfo, AllMatParams, AttributeIndex))
			return false;
	}
	
	return true;
}

bool FHoudiniMaterialTranslator::SortUniqueFaceMaterialOverridesAndCreateMaterialInstances(
	const TArray<FHoudiniMaterialInfo>& Materials,
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	const TArray<UPackage*>& InPackages, 
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InMaterials,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutMaterials,
	const bool& bForceRecookAll)
{
	// Map containing unique face materials override attribute
	// and their first valid prim index
	// We create only one material instance per attribute
	int MaterialIndex = 0;
	TMap<FHoudiniMaterialIdentifier, FHoudiniMaterialInfo> UniqueFaceMaterialOverrides;
	for (int MatArrayIdx = 0; MatArrayIdx < Materials.Num(); MatArrayIdx++)
	{
		FHoudiniMaterialInfo MatInfo = Materials[MatArrayIdx];

		MatInfo.MaterialIndex = MaterialIndex;
		if (!MatInfo.bMakeMaterialInstance)
		{
			FHoudiniMaterialIdentifier Identifier = MatInfo.MakeIdentifier();
			if (!UniqueFaceMaterialOverrides.Contains(Identifier))
			{
				UniqueFaceMaterialOverrides.Add(Identifier, MatInfo);
				MaterialIndex++;
			}
			continue;
		}

		FHoudiniMaterialIdentifier Identifier = MatInfo.MakeIdentifier();
		if (UniqueFaceMaterialOverrides.Contains(Identifier))
			continue;

		UniqueFaceMaterialOverrides.Add(Identifier, MatInfo);
		MaterialIndex++;
	}

	return FHoudiniMaterialTranslator::CreateMaterialInstances(
		InHGPO, InPackageParams,
		UniqueFaceMaterialOverrides, InPackages,
		InMaterials, OutMaterials,
		bForceRecookAll);
}

bool
FHoudiniMaterialTranslator::GetMaterialRelativePath(const HAPI_NodeId& InAssetId, const HAPI_NodeId& InMaterialNodeId, FString& OutRelativePath)
{
	HAPI_MaterialInfo MaterialInfo;
	FHoudiniApi::MaterialInfo_Init(&MaterialInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetMaterialInfo(
		FHoudiniEngine::Get().GetSession(), InMaterialNodeId,
		&MaterialInfo), false);

	return GetMaterialRelativePath(InAssetId, MaterialInfo, OutRelativePath);
}
bool
FHoudiniMaterialTranslator::GetMaterialRelativePath(const HAPI_NodeId& InAssetId, const HAPI_MaterialInfo& InMaterialInfo, FString& OutRelativePath)
{
	if (InAssetId < 0 || !InMaterialInfo.exists)
		return false;

	// We want to get the asset node path so we can remove it from the material name
	FString AssetNodeName = TEXT("");
	{
		HAPI_NodeId AssetNodeId = -1;

		HAPI_AssetInfo AssetInfo;
		FHoudiniApi::AssetInfo_Init(&AssetInfo);
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
			FHoudiniEngine::Get().GetSession(), InAssetId, &AssetInfo))
		{
			AssetNodeId = AssetInfo.nodeId;
		}
		else
		{
			AssetNodeId = InAssetId;
		}

		HAPI_NodeInfo AssetNodeInfo;
		FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), AssetNodeId, &AssetNodeInfo), false);

		FHoudiniEngineString::ToFString(AssetNodeInfo.internalNodePathSH, AssetNodeName);
	}

	// Get the material name from the info
	FString MaterialNodeName = TEXT("");
	{
		HAPI_NodeInfo MaterialNodeInfo;
		FHoudiniApi::NodeInfo_Init(&MaterialNodeInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, &MaterialNodeInfo), false);

		FHoudiniEngineString::ToFString(MaterialNodeInfo.internalNodePathSH, MaterialNodeName);
	}

	if (AssetNodeName.Len() > 0 && MaterialNodeName.Len() > 0)
	{
		// Remove AssetNodeName part from MaterialNodeName. Extra position is for separator.
		OutRelativePath = MaterialNodeName.Mid(AssetNodeName.Len() + 1);
		return true;
	}

	return false;
}

UPackage*
FHoudiniMaterialTranslator::CreatePackageForMaterial(
	const HAPI_NodeId& InMaterialNodeId, 
	const FString& InMaterialName,
	const FHoudiniPackageParams& InPackageParams,
	FString& OutMaterialName)
{
	FString MaterialDescriptor = TEXT("_material_") + FString::FromInt(InMaterialNodeId) + TEXT("_") + InMaterialName;
	//FString MaterialDescriptor = TEXT("_material_") + FString::FromInt(InMaterialNodeId) + TEXT("_") + FString::FromInt(InPackageParams.PartId) + InPackageParams.SplitStr +  TEXT("_") + InMaterialName;

	FHoudiniPackageParams MyPackageParams = InPackageParams;
	if (!MyPackageParams.ObjectName.IsEmpty())
	{
		MyPackageParams.ObjectName += MaterialDescriptor;
	}	
	else if (!MyPackageParams.HoudiniAssetName.IsEmpty())
	{
		MyPackageParams.ObjectName = MyPackageParams.HoudiniAssetName + MaterialDescriptor;
	}
	else
	{
		MyPackageParams.ObjectName = MaterialDescriptor;
	}

	return MyPackageParams.CreatePackageForObject(OutMaterialName);
}

UMaterialExpression*
FHoudiniMaterialTranslator::MaterialLocateExpression(UMaterialExpression* Expression, UClass* ExpressionClass)
{
	if (!Expression)
		return nullptr;

#if WITH_EDITOR
	if (ExpressionClass == Expression->GetClass())
		return Expression;

	// If this is a channel multiply expression, we can recurse.
	UMaterialExpressionMultiply* MaterialExpressionMultiply = Cast<UMaterialExpressionMultiply>(Expression);
	if (MaterialExpressionMultiply)
	{
		{
			UMaterialExpression* MaterialExpression = MaterialExpressionMultiply->A.Expression;
			if (MaterialExpression)
			{
				if (MaterialExpression->GetClass() == ExpressionClass)
					return MaterialExpression;

				MaterialExpression = FHoudiniMaterialTranslator::MaterialLocateExpression(
					Cast<UMaterialExpressionMultiply>(MaterialExpression), ExpressionClass);

				if (MaterialExpression)
					return MaterialExpression;
			}
		}

		{
			UMaterialExpression* MaterialExpression = MaterialExpressionMultiply->B.Expression;
			if (MaterialExpression)
			{
				if (MaterialExpression->GetClass() == ExpressionClass)
					return MaterialExpression;

				MaterialExpression = FHoudiniMaterialTranslator::MaterialLocateExpression(
					Cast<UMaterialExpressionMultiply>(MaterialExpression), ExpressionClass);

				if (MaterialExpression)
					return MaterialExpression;
			}
		}
	}
#endif

	return nullptr;
}

void
FHoudiniMaterialTranslator::_AddMaterialExpression(UMaterial* InMaterial, UMaterialExpression* InMatExp)
{
	if (!InMaterial || !InMatExp)
		return;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	// Access to material expressions has changed in UE5.1
	InMaterial->GetExpressionCollection().AddExpression(InMatExp);
#else
	InMaterial->Expressions.Add(InMatExp);
#endif
}

bool
FHoudiniMaterialTranslator::CreateTextureExpression(
	UTexture2D* Texture,
	UMaterialExpressionTextureSampleParameter2D*& TextureExpression,
	UMaterialExpression*& MatInputExpression,
	const bool SetMatInputExpression,
	UMaterial* Material,
	const EObjectFlags ObjectFlag,
	const FString& GeneratingParameterName,
	const EMaterialSamplerType SamplerType)
{
	// Create the sampling expression, if it hasn't been created yet.
	if (!TextureExpression)
		TextureExpression = NewObject<UMaterialExpressionTextureSampleParameter2D>(
			Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);

	// Record generating parameter.
	TextureExpression->Desc = GeneratingParameterName;
	TextureExpression->ParameterName = *GeneratingParameterName;

	TextureExpression->Texture = Texture;
	TextureExpression->SamplerType = SamplerType;

	// Assign expression to material.
	_AddMaterialExpression(Material, TextureExpression);
	if (SetMatInputExpression)
		MatInputExpression = TextureExpression;

	return true;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentDiffuse(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName,
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material,
	TArray<UPackage*>& OutPackages,
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Names of generating Houdini parameters.
	FString GeneratingParameterNameDiffuseTexture = TEXT("");
	FString GeneratingParameterNameUniformColor = TEXT("");
	FString GeneratingParameterNameVertexColor = TEXT(HAPI_UNREAL_ATTRIB_COLOR);

	// Diffuse texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Default;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = true;

	// Attempt to look up previously created expressions.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FColorMaterialInput& MatInputDiffuse = MaterialEditorOnly->BaseColor;
#else
	FColorMaterialInput& MatInputDiffuse = Material->BaseColor;
#endif

	UMaterialExpressionVectorParameter* ExpressionBaseColor =
		FHoudiniMaterialTranslator::CreateColorExpression(MatInputDiffuse.Expression, Material, ObjectFlag);

	// The diffuse constant color parameter on Principled Shaders is "basecolor", but COP Preview Material's diffuse texture parameter is also "basecolor".
	// We search for parameters by name without checking the node type. If we're on a CPM, then we would accidentally find the texture parameter when we want a constant color.
	// So, we check if we're in a CPM by looking for the CPM Diffuse Switch parameter. If we are in a CPM, don't search for "basecolor".
	HAPI_ParmInfo CPMSwitchInfo;
	HAPI_ParmId CPMSwitchId = FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM_SWITCH, CPMSwitchInfo);
	const char* DiffuseString = CPMSwitchId >= 0 ? "" : HAPI_UNREAL_PARAM_COLOR_DIFFUSE;

	FHoudiniMaterialTranslator::SetColorExpression(
		InMaterialInfo.nodeId,
		DiffuseString,
		HAPI_UNREAL_PARAM_COLOR_DIFFUSE_OGL,
		HAPI_UNREAL_PARAM_COLOR_DIFFUSE_CPM,
		HAPI_UNREAL_PARAM_COLOR_DIFFUSE_CPM_DEFAULT,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM_SWITCH,
		ExpressionBaseColor,
		GeneratingParameterNameUniformColor);

	UMaterialExpressionVertexColor* ExpressionVertexColor =
		FHoudiniMaterialTranslator::CreateVertexColorExpression(MatInputDiffuse.Expression, Material, ObjectFlag, GeneratingParameterNameVertexColor);

	// Locate sampling expression and its texture.
	UTexture2D* TextureDiffuse;
	UMaterialExpressionTextureSampleParameter2D* ExpressionTextureSample;
	FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputDiffuse.Expression, true, TextureDiffuse, ExpressionTextureSample);

	// See if a diffuse texture is available.
	// The diffuse constant color parameter on Principled Shaders is "basecolor", but COP Preview Material's diffuse texture parameter is also "basecolor".
	// We search for parameters by name without checking the node type. If we're on a Principled Shader, then we would accidentally find the constant color parameter when we want a texture.
	// So, we check if we're in a CPM by looking for the CPM Diffuse Switch parameter. If we aren't in a CPM, don't search for "basecolor".
	const char* CPMDiffuseString = CPMSwitchId >= 0 ? HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM : "";
	HAPI_ParmInfo ParmDiffuseTextureInfo;
	HAPI_ParmId ParmDiffuseTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_ENABLED,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL_ENABLED,
		CPMDiffuseString,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM_SWITCH,
		ParmDiffuseTextureInfo,
		GeneratingParameterNameDiffuseTexture);

	// If we have diffuse texture parameter.
	if (ParmDiffuseTextureId >= 0)
	{
		HAPI_ImagePacking ImagePacking;
		const char* PlaneType;
		bool bFoundImagePlanes = FHoudiniTextureTranslator::GetPlaneInfo(
			ParmDiffuseTextureId, InMaterialInfo,
			ImagePacking, PlaneType, CreateTexture2DParameters.bUseAlpha);

		if (bFoundImagePlanes)
		{
			// Get the node path for the texture package's meta data
			FString NodePath;
			FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				PlaneType,
				HAPI_IMAGE_DATA_INT8,
				ImagePacking,
				TextureDiffuse,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_World,
				OutPackages);

			if (bTextureCreated)
			{
				FHoudiniMaterialTranslator::CreateTextureExpression(
					TextureDiffuse,
					ExpressionTextureSample,
					MatInputDiffuse.Expression,
					false,
					Material,
					ObjectFlag,
					GeneratingParameterNameDiffuseTexture,
					SAMPLERTYPE_Color);
			}
		}
	}

	MatInputDiffuse.Expression = FHoudiniMaterialTranslator::CreateMultiplyExpressions(
		MatInputDiffuse.Expression,
		ExpressionBaseColor,
		ExpressionVertexColor,
		ExpressionTextureSample,
		Material, MaterialNodeY, ObjectFlag);

	return true;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentOpacityMask(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName,
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material,
	TArray<UPackage*>& OutPackages,
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	bool bExpressionCreated = false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameters.
	FString GeneratingParameterNameTexture = TEXT("");

	// Opacity texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = true;

	// Attempt to look up previously created expressions.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatInputOpacityMask = MaterialEditorOnly->OpacityMask;
#else
	FScalarMaterialInput& MatInputOpacityMask = Material->OpacityMask;
#endif

	// See if opacity texture is available.
	HAPI_ParmInfo ParmOpacityTextureInfo;
	HAPI_ParmId ParmOpacityTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_OPACITY,
		HAPI_UNREAL_PARAM_MAP_OPACITY_ENABLED,
		HAPI_UNREAL_PARAM_MAP_OPACITY_OGL,
		HAPI_UNREAL_PARAM_MAP_OPACITY_OGL_ENABLED,
		HAPI_UNREAL_PARAM_MAP_OPACITY_CPM,
		HAPI_UNREAL_PARAM_MAP_OPACITY_CPM_SWITCH,
		ParmOpacityTextureInfo,
		GeneratingParameterNameTexture);

	// If we have opacity texture parameter.
	if (ParmOpacityTextureId >= 0)
	{
		HAPI_ImagePacking ImagePacking;
		const char* PlaneType;
		bool bFoundImagePlanes = FHoudiniTextureTranslator::GetPlaneInfo(
			ParmOpacityTextureId, InMaterialInfo,
			ImagePacking, PlaneType, CreateTexture2DParameters.bUseAlpha);

		if (bFoundImagePlanes)
		{
			UTexture2D* TextureOpacity;
			UMaterialExpressionTextureSampleParameter2D* ExpressionTextureOpacitySample;
			FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputOpacityMask.Expression, true, TextureOpacity, ExpressionTextureOpacitySample);

			// Get the node path for the texture package's meta data
			FString NodePath;
			FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				PlaneType,
				HAPI_IMAGE_DATA_INT8,
				ImagePacking,
				TextureOpacity,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_World,
				OutPackages);

			if (bTextureCreated)
			{
				bExpressionCreated = FHoudiniMaterialTranslator::CreateTextureExpression(
					TextureOpacity,
					ExpressionTextureOpacitySample,
					MatInputOpacityMask.Expression,
					true,
					Material,
					ObjectFlag,
					GeneratingParameterNameTexture,
					SAMPLERTYPE_Grayscale);
			}

			if (bExpressionCreated)
			{
				Material->BlendMode = BLEND_Masked;

				FHoudiniMaterialTranslator::PositionExpression(ExpressionTextureOpacitySample, MaterialNodeY, 0.0f);

				// We need to set material type to masked.
				TArray<FExpressionOutput> ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
				FExpressionOutput* ExpressionOutput = ExpressionOutputs.GetData();

				MatInputOpacityMask.Mask = ExpressionOutput->Mask;
				MatInputOpacityMask.MaskR = 1;
				MatInputOpacityMask.MaskG = 0;
				MatInputOpacityMask.MaskB = 0;
				MatInputOpacityMask.MaskA = 0;
			}
		}
	}

	return bExpressionCreated;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentOpacity(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName, 
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material, 
	TArray<UPackage*>& OutPackages, 
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	bool bExpressionCreated = false;
	bool bNeedsTranslucency = false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameters.
	FString GeneratingParameterNameScalar = TEXT("");
	FString GeneratingParameterNameTexture = TEXT("");

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatInputOpacity = MaterialEditorOnly->Opacity;
#else
	FScalarMaterialInput& MatInputOpacity = Material->Opacity;
#endif

	// Opacity expressions.
	UMaterialExpressionTextureSampleParameter2D* ExpressionTextureOpacitySample = nullptr;
	UMaterialExpressionScalarParameter* ExpressionScalarOpacity = nullptr;
	UTexture2D* TextureOpacity = nullptr;

	// If opacity sampling expression was not created, check if diffuse contains an alpha plane.
	if (!ExpressionTextureOpacitySample)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		UMaterialExpression* MaterialExpressionDiffuse = MaterialEditorOnly->BaseColor.Expression;
#else
		UMaterialExpression* MaterialExpressionDiffuse = Material->BaseColor.Expression;
#endif

		if (MaterialExpressionDiffuse)
		{
			// Locate diffuse sampling expression.
			UMaterialExpressionTextureSampleParameter2D* ExpressionTextureDiffuseSample =
				Cast<UMaterialExpressionTextureSampleParameter2D>(
					FHoudiniMaterialTranslator::MaterialLocateExpression(
						MaterialExpressionDiffuse,
						UMaterialExpressionTextureSampleParameter2D::StaticClass()));

			// See if there's an alpha plane in this expression's texture.
			if (ExpressionTextureDiffuseSample)
			{
				UTexture2D* DiffuseTexture = Cast<UTexture2D>(ExpressionTextureDiffuseSample->Texture);
				if (DiffuseTexture && !DiffuseTexture->CompressionNoAlpha)
				{
					// The diffuse texture has an alpha channel (that wasn't discarded), so we can use it
					ExpressionTextureOpacitySample = ExpressionTextureDiffuseSample;
					bNeedsTranslucency = true;
				}
			}
		}
	}

	// Retrieve opacity value
	HAPI_ParmInfo ParmOpacityValueInfo;
	HAPI_ParmId ParmOpacityValueId =
		FHoudiniMaterialTranslator::FindConstantParam(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_ALPHA,
			HAPI_UNREAL_PARAM_ALPHA_OGL,
			HAPI_UNREAL_PARAM_ALPHA_CPM,
			HAPI_UNREAL_PARAM_ALPHA_CPM_DEFAULT,
			HAPI_UNREAL_PARAM_MAP_OPACITY_CPM_SWITCH,
			ParmOpacityValueInfo,
			GeneratingParameterNameScalar);

	if (ParmOpacityValueId >= 0)
	{
		if (ParmOpacityValueInfo.size > 0 && ParmOpacityValueInfo.floatValuesIndex >= 0)
		{
			float OpacityValue = 1.0f;
			if (FHoudiniApi::GetParmFloatValues(
				FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId,
				(float*)&OpacityValue, ParmOpacityValueInfo.floatValuesIndex, 1) == HAPI_RESULT_SUCCESS)
			{
				if (!ExpressionScalarOpacity)
				{
					ExpressionScalarOpacity = NewObject<UMaterialExpressionScalarParameter>(
						Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
				}

				// Clamp retrieved value.
				OpacityValue = FMath::Clamp<float>(OpacityValue, 0.0f, 1.0f);

				// Set expression fields.
				ExpressionScalarOpacity->DefaultValue = OpacityValue;
				ExpressionScalarOpacity->SliderMin = 0.0f;
				ExpressionScalarOpacity->SliderMax = 1.0f;
				ExpressionScalarOpacity->Desc = GeneratingParameterNameScalar;
				ExpressionScalarOpacity->ParameterName = *GeneratingParameterNameScalar;

				// Add expression.
				_AddMaterialExpression(Material, ExpressionScalarOpacity);

				// If alpha is less than 1, we need translucency.
				bNeedsTranslucency |= (OpacityValue != 1.0f);
			}
		}
	}

	if (bNeedsTranslucency)
		Material->BlendMode = BLEND_Translucent;

	if (ExpressionScalarOpacity && ExpressionTextureOpacitySample)
	{
		// We have both alpha and alpha uniform, attempt to locate multiply expression.
		UMaterialExpressionMultiply* ExpressionMultiply =
			Cast<UMaterialExpressionMultiply>(
				FHoudiniMaterialTranslator::MaterialLocateExpression(
					MatInputOpacity.Expression,
					UMaterialExpressionMultiply::StaticClass()));

		if (!ExpressionMultiply)
			ExpressionMultiply = NewObject<UMaterialExpressionMultiply>(
				Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

		_AddMaterialExpression(Material, ExpressionMultiply);

		TArray<FExpressionOutput> ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
		FExpressionOutput* ExpressionOutput = ExpressionOutputs.GetData();

		ExpressionMultiply->A.Expression = ExpressionTextureOpacitySample;
		ExpressionMultiply->A.Mask = ExpressionOutput->Mask;
		ExpressionMultiply->A.MaskR = 0;
		ExpressionMultiply->A.MaskG = 0;
		ExpressionMultiply->A.MaskB = 0;
		ExpressionMultiply->A.MaskA = 1;
		ExpressionMultiply->B.Expression = ExpressionScalarOpacity;

		MatInputOpacity.Expression = ExpressionMultiply;

		ExpressionMultiply->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
		ExpressionMultiply->MaterialExpressionEditorY = MaterialNodeY;

		FHoudiniMaterialTranslator::PositionExpression(ExpressionScalarOpacity, MaterialNodeY, 1.0);

		bExpressionCreated = true;
	}
	else if (ExpressionScalarOpacity)
	{
		MatInputOpacity.Expression = ExpressionScalarOpacity;

		FHoudiniMaterialTranslator::PositionExpression(ExpressionScalarOpacity, MaterialNodeY, 0.0);

		bExpressionCreated = true;
	}
	else if (ExpressionTextureOpacitySample)
	{
		TArray<FExpressionOutput> ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
		FExpressionOutput* ExpressionOutput = ExpressionOutputs.GetData();

		MatInputOpacity.Expression = ExpressionTextureOpacitySample;
		MatInputOpacity.Mask = ExpressionOutput->Mask;
		MatInputOpacity.MaskR = 0;
		MatInputOpacity.MaskG = 0;
		MatInputOpacity.MaskB = 0;
		MatInputOpacity.MaskA = 1;

		bExpressionCreated = true;
	}

	return bExpressionCreated;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentNormal(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName, 
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material,
	TArray<UPackage*>& OutPackages,
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	bool bExpressionCreated = false;
	bool bTangentSpaceNormal = true;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameter.
	FString GeneratingParameterName = TEXT("");

	// Normal texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Normalmap;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = false;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FVectorMaterialInput& MatInputNormal = MaterialEditorOnly->Normal;
#else
	FVectorMaterialInput& MatInputNormal = Material->Normal;
#endif

	// See if separate normal texture is available.
	HAPI_ParmInfo ParmNormalTextureInfo;
	HAPI_ParmId ParmNormalTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_NORMAL,
		HAPI_UNREAL_PARAM_MAP_NORMAL_ENABLED,
		HAPI_UNREAL_PARAM_MAP_NORMAL_OGL,
		"",
		HAPI_UNREAL_PARAM_MAP_NORMAL_CPM,
		"",
		ParmNormalTextureInfo,
		GeneratingParameterName);

	if (ParmNormalTextureId >= 0)
	{
		bTangentSpaceNormal = FHoudiniMaterialTranslator::RequiresWorldSpaceNormals(InMaterialInfo.nodeId);

		UTexture2D* Texture;
		UMaterialExpressionTextureSampleParameter2D* TextureExpression;
		FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputNormal.Expression, false, Texture, TextureExpression);

		// Get the node path for the texture package's meta data
		FString NodePath;
		FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

		bool bRenderSuccessful = FHoudiniTextureTranslator::HapiRenderTexture(InMaterialInfo.nodeId, ParmNormalTextureId);
		if (bRenderSuccessful)
		{
			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
				HAPI_IMAGE_DATA_INT8,
				HAPI_IMAGE_PACKING_RGBA,
				Texture,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_WorldNormalMap,
				OutPackages);

			if (bTextureCreated)
			{
				bExpressionCreated = FHoudiniMaterialTranslator::CreateTextureExpression(
					Texture,
					TextureExpression,
					MatInputNormal.Expression,
					true,
					Material,
					ObjectFlag,
					GeneratingParameterName,
					SAMPLERTYPE_Normal);
			}
		}

		if (bExpressionCreated)
		{
			FHoudiniMaterialTranslator::PositionExpression(MatInputNormal.Expression, MaterialNodeY, 0.0f);
			Material->bTangentSpaceNormal = bTangentSpaceNormal;
		}
	}

	// If separate normal map was not found, see if normal plane exists in diffuse map.
	if (!bExpressionCreated)
	{
		// See if diffuse texture is available.
		// The diffuse constant color parameter on Principled Shaders is "basecolor", but COP Preview Material's diffuse texture parameter is also "basecolor".
		// We search for parameters by name without checking the node type. If we're on a Principled Shader, then we would accidentally find the constant color parameter when we want a texture.
		// So, we check if we're in a CPM by looking for the CPM Diffuse Switch parameter. If we aren't in a CPM, don't search for "basecolor".
		HAPI_ParmInfo CPMSwitchInfo;
		HAPI_ParmId CPMSwitchId = FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM_SWITCH, CPMSwitchInfo);
		const char* CPMDiffuseString = CPMSwitchId >= 0 ? HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM : "";
		HAPI_ParmInfo ParmDiffuseTextureInfo;
		HAPI_ParmId ParmDiffuseTextureId = FHoudiniMaterialTranslator::FindTextureParam(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_ENABLED,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL_ENABLED,
			CPMDiffuseString,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_CPM_SWITCH,
			ParmDiffuseTextureInfo,
			GeneratingParameterName);

		// If normal plane is available in diffuse map.
		if (ParmDiffuseTextureId >= 0)
		{
			UTexture2D* Texture;
			UMaterialExpressionTextureSampleParameter2D* TextureExpression;
			FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputNormal.Expression, false, Texture, TextureExpression);

			// Get the node path for the texture package's meta data
			FString NodePath;
			FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

			bool bRenderSuccessful = FHoudiniTextureTranslator::HapiRenderTexture(InMaterialInfo.nodeId, ParmDiffuseTextureId);
			if (bRenderSuccessful)
			{
				bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
					InMaterialInfo.nodeId,
					HAPI_UNREAL_MATERIAL_TEXTURE_NORMAL,
					HAPI_IMAGE_DATA_INT8,
					HAPI_IMAGE_PACKING_RGB,
					Texture,
					NodePath,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
					InPackageParams,
					CreateTexture2DParameters,
					TEXTUREGROUP_WorldNormalMap,
					OutPackages);

				if (bTextureCreated)
				{
					bExpressionCreated = FHoudiniMaterialTranslator::CreateTextureExpression(
						Texture,
						TextureExpression,
						MatInputNormal.Expression,
						true,
						Material,
						ObjectFlag,
						GeneratingParameterName,
						SAMPLERTYPE_Normal);
				}
			}

			if (bExpressionCreated)
			{
				FHoudiniMaterialTranslator::PositionExpression(MatInputNormal.Expression, MaterialNodeY, 0.0f);
				Material->bTangentSpaceNormal = bTangentSpaceNormal;
			}
		}
	}

	return bExpressionCreated;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentSpecular(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName,
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material, 
	TArray<UPackage*>& OutPackages, 
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	bool bExpressionCreated = false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameter.
	FString GeneratingParameterName = TEXT("");

	// Specular texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = false;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatInputSpecular = MaterialEditorOnly->Specular;
#else
	FScalarMaterialInput& MatInputSpecular = Material->Specular;
#endif

	// See if specular texture is available.
	HAPI_ParmInfo ParmSpecularTextureInfo;
	HAPI_ParmId ParmSpecularTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_SPECULAR,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_ENABLED,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_OGL,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_OGL_ENABLED,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_CPM,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_CPM_SWITCH,
		ParmSpecularTextureInfo,
		GeneratingParameterName);

	if (ParmSpecularTextureId >= 0)
	{
		UTexture2D* Texture;
		UMaterialExpressionTextureSampleParameter2D* TextureExpression;
		FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputSpecular.Expression, false, Texture, TextureExpression);

		// Get the node path for the texture package's meta data
		FString NodePath;
		FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

		bool bRenderSuccessful = FHoudiniTextureTranslator::HapiRenderTexture(InMaterialInfo.nodeId, ParmSpecularTextureId);
		if (bRenderSuccessful)
		{
			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
				HAPI_IMAGE_DATA_INT8,
				HAPI_IMAGE_PACKING_RGBA,
				Texture,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_World,
				OutPackages);

			if (bTextureCreated)
			{
				bExpressionCreated = FHoudiniMaterialTranslator::CreateTextureExpression(
					Texture,
					TextureExpression,
					MatInputSpecular.Expression,
					true,
					Material,
					ObjectFlag,
					GeneratingParameterName,
					SAMPLERTYPE_LinearGrayscale);
			}
		}

		if (bExpressionCreated)
			FHoudiniMaterialTranslator::PositionExpression(MatInputSpecular.Expression, MaterialNodeY, 0.0f);
	}

	if (!bExpressionCreated)
		bExpressionCreated = FHoudiniMaterialTranslator::CreateScalarExpressionFromFloatParam(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_VALUE_SPECULAR,
			HAPI_UNREAL_PARAM_VALUE_SPECULAR_OGL,
			HAPI_UNREAL_PARAM_VALUE_SPECULAR_CPM,
			HAPI_UNREAL_PARAM_VALUE_SPECULAR_CPM_DEFAULT,
			HAPI_UNREAL_PARAM_MAP_SPECULAR_CPM_SWITCH,
			MatInputSpecular.Expression, Material, MaterialNodeY, ObjectFlag);

	return bExpressionCreated;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentRoughness(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName,
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material, 
	TArray<UPackage*>& OutPackages, 
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	bool bExpressionCreated = false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameter.
	FString GeneratingParameterName = TEXT("");

	// Roughness texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = false;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatInputRoughness = MaterialEditorOnly->Roughness;
#else
	FScalarMaterialInput& MatInputRoughness = Material->Roughness;
#endif

	// See if roughness texture is available.
	HAPI_ParmInfo ParmRoughnessTextureInfo;
	HAPI_ParmId ParmRoughnessTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_ENABLED,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_OGL,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_OGL_ENABLED,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_CPM,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_CPM_SWITCH,
		ParmRoughnessTextureInfo,
		GeneratingParameterName);

	if (ParmRoughnessTextureId >= 0)
	{
		UTexture2D* Texture;
		UMaterialExpressionTextureSampleParameter2D* TextureExpression;
		FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputRoughness.Expression, false, Texture, TextureExpression);

		// Get the node path for the texture package's meta data
		FString NodePath;
		FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

		bool bRenderSuccessful = FHoudiniTextureTranslator::HapiRenderTexture(InMaterialInfo.nodeId, ParmRoughnessTextureId);
		if (bRenderSuccessful)
		{
			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
				HAPI_IMAGE_DATA_INT8,
				HAPI_IMAGE_PACKING_RGBA,
				Texture,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_World,
				OutPackages);

			if (bTextureCreated)
			{
				bExpressionCreated = FHoudiniMaterialTranslator::CreateTextureExpression(
					Texture,
					TextureExpression,
					MatInputRoughness.Expression,
					true,
					Material,
					ObjectFlag,
					GeneratingParameterName,
					SAMPLERTYPE_LinearGrayscale);
			}
		}

		if (bExpressionCreated)
			FHoudiniMaterialTranslator::PositionExpression(MatInputRoughness.Expression, MaterialNodeY, 0.0f);
	}

	if (!bExpressionCreated)
		bExpressionCreated = FHoudiniMaterialTranslator::CreateScalarExpressionFromFloatParam(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_VALUE_ROUGHNESS,
			HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_OGL,
			HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_CPM,
			HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_CPM_DEFAULT,
			HAPI_UNREAL_PARAM_MAP_ROUGHNESS_CPM_SWITCH,
			MatInputRoughness.Expression, Material, MaterialNodeY, ObjectFlag);

	return bExpressionCreated;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentMetallic(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName, 
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material,
	TArray<UPackage*>& OutPackages,
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	bool bExpressionCreated = false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameter.
	FString GeneratingParameterName = TEXT("");

	// Metallic texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = false;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatInputMetallic = MaterialEditorOnly->Metallic;
#else
	FScalarMaterialInput& MatInputMetallic = Material->Metallic;	
#endif

	// See if metallic texture is available.
	HAPI_ParmInfo ParmMetallicTextureInfo;
	HAPI_ParmId ParmMetallicTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_METALLIC,
		HAPI_UNREAL_PARAM_MAP_METALLIC_ENABLED,
		HAPI_UNREAL_PARAM_MAP_METALLIC_OGL,
		HAPI_UNREAL_PARAM_MAP_METALLIC_OGL_ENABLED,
		HAPI_UNREAL_PARAM_MAP_METALLIC_CPM,
		HAPI_UNREAL_PARAM_MAP_METALLIC_CPM_SWITCH,
		ParmMetallicTextureInfo,
		GeneratingParameterName);

	if (ParmMetallicTextureId >= 0)
	{
		UTexture2D* Texture;
		UMaterialExpressionTextureSampleParameter2D* TextureExpression;
		FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputMetallic.Expression, false, Texture, TextureExpression);

		// Get the node path for the texture package's meta data
		FString NodePath;
		FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

		bool bRenderSuccessful = FHoudiniTextureTranslator::HapiRenderTexture(InMaterialInfo.nodeId, ParmMetallicTextureId);
		if (bRenderSuccessful)
		{
			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
				HAPI_IMAGE_DATA_INT8,
				HAPI_IMAGE_PACKING_RGBA,
				Texture,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_World,
				OutPackages);

			if (bTextureCreated)
			{
				bExpressionCreated = FHoudiniMaterialTranslator::CreateTextureExpression(
					Texture,
					TextureExpression,
					MatInputMetallic.Expression,
					true,
					Material,
					ObjectFlag,
					GeneratingParameterName,
					SAMPLERTYPE_LinearGrayscale);
			}
		}

		if (bExpressionCreated)
			FHoudiniMaterialTranslator::PositionExpression(MatInputMetallic.Expression, MaterialNodeY, 0.0f);
	}

	if (!bExpressionCreated)
		bExpressionCreated = FHoudiniMaterialTranslator::CreateScalarExpressionFromFloatParam(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_VALUE_METALLIC,
			HAPI_UNREAL_PARAM_VALUE_METALLIC_OGL,
			HAPI_UNREAL_PARAM_VALUE_METALLIC_CPM,
			HAPI_UNREAL_PARAM_VALUE_METALLIC_CPM_DEFAULT,
			HAPI_UNREAL_PARAM_MAP_METALLIC_CPM_SWITCH,
			MatInputMetallic.Expression, Material, MaterialNodeY, ObjectFlag);

	return bExpressionCreated;
}

bool
FHoudiniMaterialTranslator::CreateMaterialComponentEmissive(
	const HAPI_NodeId& InAssetId,
	const FString& InHoudiniAssetName,
	const HAPI_MaterialInfo& InMaterialInfo,
	const FHoudiniPackageParams& InPackageParams,
	UMaterial* Material,
	TArray<UPackage*>& OutPackages,
	int32& MaterialNodeY)
{
	if (!IsValid(Material))
		return false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Names of generating Houdini parameters.
	FString GeneratingParameterNameEmissiveTexture = TEXT("");
	FString GeneratingParameterNameEmissiveColor = TEXT("");
	FString GeneratingParameterNameEmissiveIntensity = TEXT("");

	// Emissive texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Default;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = true;

	// Attempt to look up previously created expressions.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FColorMaterialInput& MatInputEmissive = MaterialEditorOnly->EmissiveColor;
#else
	FColorMaterialInput& MatInputEmissive = Material->EmissiveColor;
#endif

	// Locate emissive color expression.
	UMaterialExpressionVectorParameter* ExpressionEmissiveColor =
		FHoudiniMaterialTranslator::CreateColorExpression(MatInputEmissive.Expression, Material, ObjectFlag);
	FHoudiniMaterialTranslator::SetColorExpression(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_OGL,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_CPM,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_CPM_DEFAULT,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_CPM_SWITCH,
		ExpressionEmissiveColor,
		GeneratingParameterNameEmissiveColor);

	// Locate emissive intensity expression.
	UMaterialExpressionScalarParameter* ExpressionEmissiveIntensity =
		FHoudiniMaterialTranslator::CreateScalarExpression(MatInputEmissive.Expression, Material, ObjectFlag, GeneratingParameterNameEmissiveIntensity);

	// See if emissive intensity is available.
	HAPI_ParmInfo ParmEmissiveIntensityInfo;
	HAPI_ParmId ParmEmissiveIntensityId = FHoudiniMaterialTranslator::FindConstantParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY_OGL,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY_CPM,
		HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY_CPM_DEFAULT,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_INTENSITY_CPM_SWITCH,
		ParmEmissiveIntensityInfo,
		GeneratingParameterNameEmissiveIntensity);

	bool bHasEmissiveIntensity = false;
	float EmmissiveIntensity = 0.0f;
	if (ParmEmissiveIntensityId >= 0)
	{
		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, (float*)&EmmissiveIntensity,
			ParmEmissiveIntensityInfo.floatValuesIndex, 1) == HAPI_RESULT_SUCCESS)
		{
			bHasEmissiveIntensity = true;
		}
	}

	ExpressionEmissiveIntensity->DefaultValue = EmmissiveIntensity;
	ExpressionEmissiveIntensity->Desc = GeneratingParameterNameEmissiveIntensity;
	ExpressionEmissiveIntensity->ParameterName = *GeneratingParameterNameEmissiveIntensity;

	// Locate sampling expression and its texture.
	UTexture2D* TextureEmissive;
	UMaterialExpressionTextureSampleParameter2D* ExpressionTextureSample;
	FHoudiniMaterialTranslator::GetTextureAndExpression(MatInputEmissive.Expression, true, TextureEmissive, ExpressionTextureSample);

	// See if an emissive texture is available.
	HAPI_ParmInfo ParmEmissiveTextureInfo;
	HAPI_ParmId ParmEmissiveTextureId = FHoudiniMaterialTranslator::FindTextureParam(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_ENABLED,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_OGL,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_OGL_ENABLED,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_CPM,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_CPM_SWITCH,
		ParmEmissiveTextureInfo,
		GeneratingParameterNameEmissiveTexture);

	// If we have an emissive texture parameter.
	if (ParmEmissiveTextureId >= 0)
	{
		HAPI_ImagePacking ImagePacking;
		const char* PlaneType;
		bool bFoundImagePlanes = FHoudiniTextureTranslator::GetPlaneInfo(
			ParmEmissiveTextureId, InMaterialInfo,
			ImagePacking, PlaneType, CreateTexture2DParameters.bUseAlpha);

		if (bFoundImagePlanes)
		{
			// Get the node path for the texture package's meta data
			FString NodePath;
			FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

			bool bTextureCreated = FHoudiniTextureTranslator::CreateTexture(
				InMaterialInfo.nodeId,
				PlaneType,
				HAPI_IMAGE_DATA_INT8,
				ImagePacking,
				TextureEmissive,
				NodePath,
				HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
				InPackageParams,
				CreateTexture2DParameters,
				TEXTUREGROUP_World,
				OutPackages);

			if (bTextureCreated)
			{
				FHoudiniMaterialTranslator::CreateTextureExpression(
					TextureEmissive,
					ExpressionTextureSample,
					MatInputEmissive.Expression,
					false,
					Material,
					ObjectFlag,
					GeneratingParameterNameEmissiveTexture,
					SAMPLERTYPE_Color);
			}
		}
	}

	MatInputEmissive.Expression = FHoudiniMaterialTranslator::CreateMultiplyExpressions(
		MatInputEmissive.Expression,
		ExpressionEmissiveColor,
		ExpressionEmissiveIntensity,
		ExpressionTextureSample,
		Material, MaterialNodeY, ObjectFlag);

	return true;
}

bool
FHoudiniMaterialTranslator::GetAndValidateMaterialInstanceParameterValue(
	const FName& InMaterialParameterName,
	const FHoudiniGenericAttribute& MaterialParameterAttribute,
	const int32 InAttributeIndex,
	UMaterialInterface* const MaterialInterface,
	FHoudiniMaterialParameterValue& OutMaterialParameterValue)
{
	// This function is tightly coupled with UpdateMaterialInstanceParameter(): changes to the one function likely
	// require changes to the other!
#if WITH_EDITOR
	if (!MaterialInterface)
		return false;

	if (InMaterialParameterName.IsNone())
		return false;

	if (MaterialParameterAttribute.AttributeOwner == EAttribOwner::Invalid)
		return false;

	int32 ValueIdx = InAttributeIndex;
	if (ValueIdx < 0 || MaterialParameterAttribute.AttributeOwner == EAttribOwner::Detail)
		ValueIdx = 0;

	// Check if the MaterialParameter corresponds to material instance property first
	static const FName FloatProperties[]
	{
		"EmissiveBoost",
		"DiffuseBoost",
		"ExportResolutionScale",
		"OpacityMaskClipValue",
	};
	static const FName BoolProperties[]
	{
		"CastShadowAsMasked",
		"TwoSided",
		"DitheredLODTransition",
	};

	for (const FName& PropertyName : FloatProperties)
	{
		if (InMaterialParameterName != PropertyName)
			continue;

		if (MaterialParameterAttribute.AttributeTupleSize != 1)
			return false;

		OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::StandardParameter;
		OutMaterialParameterValue.SetValue((float)MaterialParameterAttribute.GetDoubleValue(ValueIdx));

		return true;
	}
	
	for (const FName& PropertyName : BoolProperties)
	{
		if (InMaterialParameterName != PropertyName)
			continue;
		
		if (MaterialParameterAttribute.AttributeTupleSize != 1)
			return false;

		OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::StandardParameter;
		OutMaterialParameterValue.SetValue(static_cast<uint8>(MaterialParameterAttribute.GetBoolValue(ValueIdx)));

		return true;
	}

	if (InMaterialParameterName == "BlendMode")
	{
		if (MaterialParameterAttribute.AttributeTupleSize != 1)
			return false;

		OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::StandardParameter;
		EBlendMode EnumValue = (EBlendMode)MaterialParameterAttribute.GetIntValue(ValueIdx);
		if (MaterialParameterAttribute.AttributeType == EAttribStorageType::STRING)
		{
			FString StringValue = MaterialParameterAttribute.GetStringValue(ValueIdx);
			if (StringValue.Compare("Opaque", ESearchCase::IgnoreCase) == 0)
				EnumValue = EBlendMode::BLEND_Opaque;
			else if (StringValue.Compare("Masked", ESearchCase::IgnoreCase) == 0)
				EnumValue = EBlendMode::BLEND_Masked;
			else if (StringValue.Compare("Translucent", ESearchCase::IgnoreCase) == 0)
				EnumValue = EBlendMode::BLEND_Translucent;
			else if (StringValue.Compare("Additive", ESearchCase::IgnoreCase) == 0)
				EnumValue = EBlendMode::BLEND_Additive;
			else if (StringValue.Compare("Modulate", ESearchCase::IgnoreCase) == 0)
				EnumValue = EBlendMode::BLEND_Modulate;
			else if (StringValue.StartsWith("Alpha", ESearchCase::IgnoreCase))
				EnumValue = EBlendMode::BLEND_AlphaComposite;
		}
		OutMaterialParameterValue.SetValue(static_cast<uint8>(EnumValue));

		return true;
	}
	
	if (InMaterialParameterName == "ShadingModel")
	{
		if (MaterialParameterAttribute.AttributeTupleSize != 1)
			return false;

		OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::StandardParameter;
		EMaterialShadingModel EnumValue = (EMaterialShadingModel)MaterialParameterAttribute.GetIntValue(ValueIdx);
		if (MaterialParameterAttribute.AttributeType == EAttribStorageType::STRING)
		{
			FString StringValue = MaterialParameterAttribute.GetStringValue(ValueIdx);
			if (StringValue.Compare("Unlit", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_Unlit;
			else if (StringValue.StartsWith("Default", ESearchCase::IgnoreCase))
				EnumValue = EMaterialShadingModel::MSM_DefaultLit;
			else if (StringValue.Compare("Subsurface", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_Subsurface;
			else if (StringValue.StartsWith("Preintegrated", ESearchCase::IgnoreCase))
				EnumValue = EMaterialShadingModel::MSM_PreintegratedSkin;
			else if (StringValue.StartsWith("Clear", ESearchCase::IgnoreCase))
				EnumValue = EMaterialShadingModel::MSM_ClearCoat;
			else if (StringValue.Compare("SubsurfaceProfile", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_SubsurfaceProfile;
			else if (StringValue.Compare("TwoSidedFoliage", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_TwoSidedFoliage;
			else if (StringValue.Compare("Hair", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_Hair;
			else if (StringValue.Compare("Cloth", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_Cloth;
			else if (StringValue.Compare("Eye", ESearchCase::IgnoreCase) == 0)
				EnumValue = EMaterialShadingModel::MSM_Eye;
		}
		OutMaterialParameterValue.SetValue(static_cast<uint8>(EnumValue));

		return true;
	}
	
	if (InMaterialParameterName == "PhysMaterial")
	{
		if (MaterialParameterAttribute.AttributeTupleSize != 1)
			return false;

		OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::StandardParameter;
		OutMaterialParameterValue.SetValue(MaterialParameterAttribute.GetStringValue(ValueIdx));

		return true;
	}

	// Handling custom material parameters
	if (MaterialParameterAttribute.AttributeType == EAttribStorageType::STRING)
	{
		// If there is no texture parameter by this name, return false (parameter should be excluded)
		UTexture* OldTexture = nullptr;
		if (!MaterialInterface->GetTextureParameterValue(InMaterialParameterName, OldTexture))
			return false;

		TArray<FString> StringTuple;
		MaterialParameterAttribute.GetStringTuple(StringTuple, ValueIdx);
		if (StringTuple.Num() <= 0)
			return false;
		
		OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::Texture;
		OutMaterialParameterValue.SetValue(StringTuple[0]);

		return true;
	}

	if (MaterialParameterAttribute.AttributeTupleSize == 1)
	{
		// Single attributes are either for scalar parameters or static switches
		float OldValue;
		if (MaterialInterface->GetScalarParameterValue(InMaterialParameterName, OldValue))
		{
			// The material parameter is a scalar
			OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::Scalar;
			OutMaterialParameterValue.SetValue((float)MaterialParameterAttribute.GetDoubleValue(ValueIdx));

			return true;
		}

		// See if the underlying parameter is a static switch
		// We need to iterate over the material's static parameter set
		FStaticParameterSet StaticParameters;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2)
		MaterialInterface->GetStaticParameterValues(StaticParameters);
#else
		GetStaticParameterValues(MaterialInterface, StaticParameters);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
		TArray<FStaticSwitchParameter>& StaticSwitchParams = StaticParameters.StaticSwitchParameters;
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
		TArray<FStaticSwitchParameter>& StaticSwitchParams = StaticParameters.EditorOnly.StaticSwitchParameters;
#else
		TArray<FStaticSwitchParameter>& StaticSwitchParams = StaticParameters.StaticSwitchParameters;
#endif
		for (int32 SwitchParameterIdx = 0; SwitchParameterIdx < StaticSwitchParams.Num(); ++SwitchParameterIdx)
		{
			const FStaticSwitchParameter& SwitchParameter = StaticSwitchParams[SwitchParameterIdx];
			if (SwitchParameter.ParameterInfo.Name != InMaterialParameterName)
				continue;

			OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::StaticSwitch;
			OutMaterialParameterValue.SetValue(static_cast<uint8>(MaterialParameterAttribute.GetBoolValue(ValueIdx)));
			return true;
		}

		return false;
	}
	
	// Tuple attributes are for vector parameters
	FLinearColor OldValue;
	if (!MaterialInterface->GetVectorParameterValue(InMaterialParameterName, OldValue))
		return false;
	
	FLinearColor NewLinearColor(0, 0, 0, 0);
	// if the attribute is stored in an int, we'll have to convert a color to a linear color
	if (MaterialParameterAttribute.AttributeType == EAttribStorageType::INT || MaterialParameterAttribute.AttributeType == EAttribStorageType::INT64)
	{
		TArray<int64> IntTuple;
		MaterialParameterAttribute.GetIntTuple(IntTuple, ValueIdx);
		
		FColor IntColor(0, 0, 0, 0);
		if (IntTuple.IsValidIndex(0))
			IntColor.R = (int8)IntTuple[0];
		if (IntTuple.IsValidIndex(1))
			IntColor.G = (int8)IntTuple[1];
		if (IntTuple.IsValidIndex(2))
			IntColor.B = (int8)IntTuple[2];
		if (IntTuple.IsValidIndex(3))
			IntColor.A = (int8)IntTuple[3];
		else
			IntColor.A = 1;

		NewLinearColor = FLinearColor(IntColor);
	}
	else
	{
		TArray<double> DoubleTuple;
		MaterialParameterAttribute.GetDoubleTuple(DoubleTuple, ValueIdx);
		if (DoubleTuple.IsValidIndex(0))
			NewLinearColor.R = (float)DoubleTuple[0];
		if (DoubleTuple.IsValidIndex(1))
			NewLinearColor.G = (float)DoubleTuple[1];
		if (DoubleTuple.IsValidIndex(2))
			NewLinearColor.B = (float)DoubleTuple[2];
		if (DoubleTuple.IsValidIndex(3))
			NewLinearColor.A = (float)DoubleTuple[3];
		else
			NewLinearColor.A = 1.0f;
	}

	OutMaterialParameterValue.ParamType = EHoudiniUnrealMaterialParameterType::Vector;
	OutMaterialParameterValue.SetValue(NewLinearColor);
	return true;
#else
	return false;
#endif
}

bool
FHoudiniMaterialTranslator::UpdateMaterialInstanceParameter(
	const FName& InMaterialParameterName,
	const FHoudiniMaterialParameterValue& InMaterialParameterValue,
	UMaterialInstanceConstant* MaterialInstance,
	const TArray<UPackage*>& InPackages)
{
	// This function is tightly coupled with GetAndValidateMaterialInstanceParameterValue(): changes to the one function
	// likely require changes to the other!

#if WITH_EDITOR
	if (!MaterialInstance)
		return false;

	if (InMaterialParameterName.IsNone())
		return false;

	// The default material instance parameters needs to be handled manually as they cant be changed via generic SetParameters functions
	switch (InMaterialParameterValue.ParamType)
	{
		case EHoudiniUnrealMaterialParameterType::Invalid:
			return false;
		case EHoudiniUnrealMaterialParameterType::StandardParameter:
		{
			if (InMaterialParameterName == "CastShadowAsMasked")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
					return false;
				const bool Value = static_cast<bool>(InMaterialParameterValue.ByteValue);

				// Update the parameter value only if necessary
				if (MaterialInstance->GetOverrideCastShadowAsMasked() && (MaterialInstance->GetCastShadowAsMasked() == Value))
					return false;

				MaterialInstance->SetOverrideCastShadowAsMasked(true);
				MaterialInstance->SetCastShadowAsMasked(Value);
				
				return true;
			}
			
			if (InMaterialParameterName == "EmissiveBoost")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Float)
					return false;
				const float Value = InMaterialParameterValue.FloatValue;

				// Update the parameter value only if necessary
				if (MaterialInstance->GetOverrideEmissiveBoost() && (MaterialInstance->GetEmissiveBoost() == Value))
					return false;

				MaterialInstance->SetOverrideEmissiveBoost(true);
				MaterialInstance->SetEmissiveBoost(Value);
				
				return true;
			}
			
			if (InMaterialParameterName == "DiffuseBoost")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Float)
					return false;
				const float Value = InMaterialParameterValue.FloatValue;

				// Update the parameter value only if necessary
				if (MaterialInstance->GetOverrideDiffuseBoost() && (MaterialInstance->GetDiffuseBoost() == Value))
					return false;

				MaterialInstance->SetOverrideDiffuseBoost(true);
				MaterialInstance->SetDiffuseBoost(Value);
				
				return true;
			}
			
			if (InMaterialParameterName == "ExportResolutionScale")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Float)
					return false;
				const float Value = InMaterialParameterValue.FloatValue;

				// Update the parameter value only if necessary
				if (MaterialInstance->GetOverrideExportResolutionScale() && (MaterialInstance->GetExportResolutionScale() == Value))
					return false;

				MaterialInstance->SetOverrideExportResolutionScale(true);
				MaterialInstance->SetExportResolutionScale(Value);
				
				return true;
			}
			
			if (InMaterialParameterName == "OpacityMaskClipValue")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Float)
					return false;
				const float Value = InMaterialParameterValue.FloatValue;

				// Update the parameter value only if necessary
				if (MaterialInstance->BasePropertyOverrides.bOverride_OpacityMaskClipValue && (MaterialInstance->BasePropertyOverrides.OpacityMaskClipValue == Value))
					return false;

				MaterialInstance->BasePropertyOverrides.bOverride_OpacityMaskClipValue = true;
				MaterialInstance->BasePropertyOverrides.OpacityMaskClipValue = Value;
				
				return true;
			}
			
			if (InMaterialParameterName == "BlendMode")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
					return false;
				const EBlendMode EnumValue = (EBlendMode)InMaterialParameterValue.ByteValue;

				// Update the parameter value only if necessary
				if (MaterialInstance->BasePropertyOverrides.bOverride_BlendMode && (MaterialInstance->BasePropertyOverrides.BlendMode == EnumValue))
					return false;

				MaterialInstance->BasePropertyOverrides.bOverride_BlendMode = true;
				MaterialInstance->BasePropertyOverrides.BlendMode = EnumValue;
				
				return true;
			}
			
			if (InMaterialParameterName == "ShadingModel")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
					return false;
				const EMaterialShadingModel EnumValue = (EMaterialShadingModel)InMaterialParameterValue.ByteValue;

				// Update the parameter value only if necessary
				if (MaterialInstance->BasePropertyOverrides.bOverride_ShadingModel && (MaterialInstance->BasePropertyOverrides.ShadingModel == EnumValue))
					return false;

				MaterialInstance->BasePropertyOverrides.bOverride_ShadingModel = true;
				MaterialInstance->BasePropertyOverrides.ShadingModel = EnumValue;
				
				return true;
			}
			
			if (InMaterialParameterName == "TwoSided")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
					return false;
				const bool Value = static_cast<bool>(InMaterialParameterValue.ByteValue);

				// Update the parameter value only if necessary
				if (MaterialInstance->BasePropertyOverrides.bOverride_TwoSided && (MaterialInstance->BasePropertyOverrides.TwoSided == Value))
					return false;

				MaterialInstance->BasePropertyOverrides.bOverride_TwoSided = true;
				MaterialInstance->BasePropertyOverrides.TwoSided = Value;
				
				return true;
			}
			
			if (InMaterialParameterName == "DitheredLODTransition")
			{
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
					return false;
				const bool Value = static_cast<bool>(InMaterialParameterValue.ByteValue);

				// Update the parameter value only if necessary
				if (MaterialInstance->BasePropertyOverrides.bOverride_DitheredLODTransition && (MaterialInstance->BasePropertyOverrides.DitheredLODTransition == Value))
					return false;

				MaterialInstance->BasePropertyOverrides.bOverride_DitheredLODTransition = true;
				MaterialInstance->BasePropertyOverrides.DitheredLODTransition = Value;
				
				return true;
			}
			
			if (InMaterialParameterName == "PhysMaterial")
			{
				// Try to load a Material corresponding to the parameter value
				if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::String)
					return false;
				
				UPhysicalMaterial* FoundPhysMaterial = Cast<UPhysicalMaterial>(
					StaticLoadObject(UPhysicalMaterial::StaticClass(), nullptr, *InMaterialParameterValue.StringValue, nullptr, LOAD_NoWarn, nullptr));
				
				// Update the parameter value if necessary
				if (!FoundPhysMaterial || (MaterialInstance->PhysMaterial == FoundPhysMaterial))
					return false;

				MaterialInstance->PhysMaterial = FoundPhysMaterial;
				
				return true;
			}
			break;
		}
		// Handling custom parameters
		case EHoudiniUnrealMaterialParameterType::Texture:
		{
			// String attributes are used for textures parameters
			// We need to find the texture corresponding to the param
			UTexture* FoundTexture = nullptr;
			const FString ParamValue = InMaterialParameterValue.StringValue;

			// Texture can either be already existing texture assets in UE4, or a newly generated textures by this asset
			// Try to find the texture corresponding to the param value in the existing assets first.
			FoundTexture = Cast<UTexture>(
				StaticLoadObject(UTexture::StaticClass(), nullptr, *ParamValue, nullptr, LOAD_NoWarn, nullptr));

			if (!FoundTexture)
			{
				// We couldn't find a texture corresponding to the parameter in the existing UE4 assets
				// Try to find the corresponding texture in the cooked temporary package we just generated
				FoundTexture = FHoudiniMaterialTranslator::FindGeneratedTexture(ParamValue, InPackages);
			}

			if (!FoundTexture)
				return false;

			// Do not update if unnecessary
			UTexture* OldTexture = nullptr;
			bool FoundOldParam = MaterialInstance->GetTextureParameterValue(InMaterialParameterName, OldTexture);
			if (FoundOldParam && (OldTexture == FoundTexture))
				return false;

			MaterialInstance->SetTextureParameterValueEditorOnly(InMaterialParameterName, FoundTexture);
			return true;
		}
		case EHoudiniUnrealMaterialParameterType::Scalar:
		{
			// Single attributes are either for scalar parameters or static switches
			float OldValue;
			if (!MaterialInstance->GetScalarParameterValue(InMaterialParameterName, OldValue))
				return false;
			// The material parameter is a scalar
			if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Float)
				return false;
			const float NewValue = InMaterialParameterValue.FloatValue;

			// Do not update if unnecessary
			if (OldValue == NewValue)
				return false;

			MaterialInstance->SetScalarParameterValueEditorOnly(InMaterialParameterName, NewValue);
			
			return true;
		}
		case EHoudiniUnrealMaterialParameterType::StaticSwitch:
		{
			// See if the underlying parameter is a static switch
			if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Byte)
				return false;
			const bool NewBoolValue = static_cast<bool>(InMaterialParameterValue.ByteValue);

			// We need to iterate over the material's static parameter set
			FStaticParameterSet StaticParameters;
			MaterialInstance->GetStaticParameterValues(StaticParameters);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
			TArray<FStaticSwitchParameter>& StaticSwitchParams = StaticParameters.StaticSwitchParameters;
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
			TArray<FStaticSwitchParameter>& StaticSwitchParams = StaticParameters.EditorOnly.StaticSwitchParameters;
#else
			TArray<FStaticSwitchParameter>& StaticSwitchParams = StaticParameters.StaticSwitchParameters;
#endif
			for (int32 SwitchParameterIdx = 0; SwitchParameterIdx < StaticSwitchParams.Num(); ++SwitchParameterIdx)
			{
				FStaticSwitchParameter& SwitchParameter = StaticSwitchParams[SwitchParameterIdx];
				if (SwitchParameter.ParameterInfo.Name != InMaterialParameterName)
					continue;

				if (SwitchParameter.Value == NewBoolValue)
					return false;

				SwitchParameter.Value = NewBoolValue;
				SwitchParameter.bOverride = true;

				MaterialInstance->UpdateStaticPermutation(StaticParameters);
				return true;
			}

			return false;
		}
		case EHoudiniUnrealMaterialParameterType::Vector:
		{
			if (InMaterialParameterValue.DataType != EHoudiniUnrealMaterialParameterDataType::Vector)
				return false;
			const FLinearColor NewLinearColor = InMaterialParameterValue.VectorValue;

			// Do not update if unnecessary
			FLinearColor OldValue;
			bool FoundOldParam = MaterialInstance->GetVectorParameterValue(InMaterialParameterName, OldValue);
			if (FoundOldParam && (OldValue == NewLinearColor))
				return false;

			MaterialInstance->SetVectorParameterValueEditorOnly(InMaterialParameterName, NewLinearColor);
			return true;
		}
	}
#endif

	return false;
}

UTexture*
FHoudiniMaterialTranslator::FindGeneratedTexture(const FString& TextureString, const TArray<UPackage*>& InPackages)
{
	if (TextureString.IsEmpty())
		return nullptr;

	// Try to find the corresponding texture in the cooked temporary package generated by an HDA
	UTexture* FoundTexture = nullptr;
	for (const auto& CurrentPackage : InPackages)
	{
		// Iterate through the cooked packages
		if (!IsValid(CurrentPackage))
			continue;

		// First, check if the package contains a texture
		FString CurrentPackageName = CurrentPackage->GetName();
		UTexture* PackageTexture = LoadObject<UTexture>(CurrentPackage, *CurrentPackageName, nullptr, LOAD_None, nullptr);
		if (!PackageTexture)
			continue;

		// Then check if the package's metadata match what we're looking for
		// Make sure this texture was generated by Houdini Engine
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		FMetaData MetaData = CurrentPackage->GetMetaData();
		if (MetaData.HasValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
			continue;
#else
		UMetaData* MetaData = CurrentPackage->GetMetaData();
		if (!MetaData || !MetaData->HasValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
			continue;
#endif
		// Get the texture type from the meta data
		// Texture type store has meta data will be C_A, N, S, R etc..
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		const FString TextureTypeString = MetaData.GetValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);
#else
		const FString TextureTypeString = MetaData->GetValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);
#endif
		if (TextureTypeString.Compare(TextureString, ESearchCase::IgnoreCase) == 0)
		{
			FoundTexture = PackageTexture;
			break;
		}

		// Convert the texture type to a "friendly" version
		// C_A to diffuse, N to Normal, S to Specular etc...
		FString TextureTypeFriendlyString = TextureTypeString;
		FString TextureTypeFriendlyAlternateString = TEXT("");
		if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE, ESearchCase::IgnoreCase) == 0)
		{
			TextureTypeFriendlyString = TEXT("diffuse");
			TextureTypeFriendlyAlternateString = TEXT("basecolor");
		}
		else if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL, ESearchCase::IgnoreCase) == 0)
			TextureTypeFriendlyString = TEXT("normal");
		else if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE, ESearchCase::IgnoreCase) == 0)
			TextureTypeFriendlyString = TEXT("emissive");
		else if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR, ESearchCase::IgnoreCase) == 0)
			TextureTypeFriendlyString = TEXT("specular");
		else if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS, ESearchCase::IgnoreCase) == 0)
			TextureTypeFriendlyString = TEXT("roughness");
		else if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC, ESearchCase::IgnoreCase) == 0)
			TextureTypeFriendlyString = TEXT("metallic");
		else if (TextureTypeString.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK, ESearchCase::IgnoreCase) == 0)
			TextureTypeFriendlyString = TEXT("opacity");

		// See if we have a match between the texture string and the friendly name
		if ((TextureTypeFriendlyString.Compare(TextureString, ESearchCase::IgnoreCase) == 0)
			|| (!TextureTypeFriendlyAlternateString.IsEmpty() && TextureTypeFriendlyAlternateString.Compare(TextureString, ESearchCase::IgnoreCase) == 0))
		{
			FoundTexture = PackageTexture;
			break;
		}

		// Get the node path from the meta data
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		const FString NodePath = MetaData.GetValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_NODE_PATH);
#else
		const FString NodePath = MetaData->GetValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_NODE_PATH);
#endif
		if (NodePath.IsEmpty())
			continue;

		// See if we have a match with the path and texture type
		FString PathAndType = NodePath + TEXT("/") + TextureTypeString;
		if (PathAndType.Compare(TextureString, ESearchCase::IgnoreCase) == 0)
		{
			FoundTexture = PackageTexture;
			break;
		}

		// See if we have a match with the friendly path and texture type
		FString PathAndFriendlyType = NodePath + TEXT("/") + TextureTypeFriendlyString;
		if (PathAndFriendlyType.Compare(TextureString, ESearchCase::IgnoreCase) == 0)
		{
			FoundTexture = PackageTexture;
			break;
		}

		// Try the alternate friendly string
		if (!TextureTypeFriendlyAlternateString.IsEmpty())
		{
			PathAndFriendlyType = NodePath + TEXT("/") + TextureTypeFriendlyAlternateString;
			if (PathAndFriendlyType.Compare(TextureString, ESearchCase::IgnoreCase) == 0)
			{
				FoundTexture = PackageTexture;
				break;
			}
		}
	}

	return FoundTexture;
}

bool
FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
	const HAPI_NodeId& InNodeId,
	const std::string& InTextureParmName,
	const std::string& InUseTextureParmName,
	const bool& bFindByTag,
	const bool& bIsCPM,
	HAPI_ParmId& OutParmId,
	HAPI_ParmInfo& OutParmInfo)
{
	OutParmId = -1;
	
	if(bFindByTag)
		OutParmId = FHoudiniEngineUtils::HapiFindParameterByTag(InNodeId, InTextureParmName, OutParmInfo);
	else
		OutParmId = FHoudiniEngineUtils::HapiFindParameterByName(InNodeId, InTextureParmName, OutParmInfo);

	if (OutParmId < 0)
	{
		// Failed to find the texture
		return false;
	}

	// We found a valid parameter, check if the matching "use" parameter exists
	HAPI_ParmInfo FoundUseParmInfo;
	HAPI_ParmId FoundUseParmId = -1;
	if(bFindByTag)
		FoundUseParmId = FHoudiniEngineUtils::HapiFindParameterByTag(InNodeId, InUseTextureParmName, FoundUseParmInfo);
	else
		FoundUseParmId = FHoudiniEngineUtils::HapiFindParameterByName(InNodeId, InUseTextureParmName, FoundUseParmInfo);

	if (FoundUseParmId >= 0)
	{
		// We found a valid "use" parameter, check its value to see if the texture should be used
		if (!bIsCPM)
		{
			int32 UseValue = 0;
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmIntValues(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, &UseValue, FoundUseParmInfo.intValuesIndex, 1))
			{
				if (UseValue == 0)
				{
					// We found the texture parm, but the "use" param/tag is disabled, so don't use it!
					// We still return true as we found the parameter, this will prevent looking for other parms
					OutParmId = -1;
					return true;
				}
			}
		}
		else
		{
			float UseValue = 0;
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmFloatValues(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, &UseValue, FoundUseParmInfo.floatValuesIndex, 1))
			{
				// For some reason, in CPMs the switch parameter is a float, but it's used like a boolean.
				// 0.0 means the source is either File or COP, and 1.0 means the source is Constant.
				if (UseValue != 0.0f)
				{
					// We still return true as we found the parameter, this will prevent looking for other parms
					OutParmId = -1;
					return true;
				}
			}
		}
	}

	// Finally, make sure that the found texture Parm is not empty!		
	FString ParmValue = FString();
	HAPI_StringHandle StringHandle;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmStringValues(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, false, &StringHandle, OutParmInfo.stringValuesIndex, 1))
	{
		// Convert the string handle to FString
		FHoudiniEngineString::ToFString(StringHandle, ParmValue);
	}

	if (ParmValue.IsEmpty())
	{
		// We found the parm, but it's empty, don't use it!
		// We still return true as we found the parameter, this will prevent looking for other parms
		OutParmId = -1;
		return true;
	}

	return true;
}

HAPI_ParmId
FHoudiniMaterialTranslator::FindConstantParam(
	const HAPI_NodeId& NodeId,
	const char* Name,
	const char* Tag,
	const char* CPMConst,
	const char* CPMDefault,
	const char* CPMSwitch,
	HAPI_ParmInfo& Info,
	FString& GeneratingParameterName)
{
	// Attempt to get the parameter by name.
	HAPI_ParmId ValueId = FHoudiniEngineUtils::HapiFindParameterByName(NodeId, Name, Info);
	if (ValueId >= 0)
	{
		GeneratingParameterName = FString(Name);
		return ValueId;
	}

	// Attempt to get the parameter by tag.
	ValueId = FHoudiniEngineUtils::HapiFindParameterByTag(NodeId, Tag, Info);
	if (ValueId >= 0)
	{
		GeneratingParameterName = FString(Tag);
		return ValueId;
	}

	// Attempt to get the COP Preview Material constant parameter.
	// First, check that the switch is set to 1.0.
	HAPI_ParmInfo CPMSwitchInfo;
	HAPI_ParmId CPMSwitchId = FHoudiniEngineUtils::HapiFindParameterByName(NodeId, CPMSwitch, CPMSwitchInfo);
	if (CPMSwitchId >= 0)
	{
		float CPMSwitchValue = 0;
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(),
			NodeId, &CPMSwitchValue, CPMSwitchInfo.floatValuesIndex, 1))
		{
			// For some reason, in CPMs the switch parameter is a float, but it's used like a boolean.
			// 0.0 means the source is either File or COP, and 1.0 means the source is Constant.
			if (CPMSwitchValue != 0.0) {
				ValueId = FHoudiniEngineUtils::HapiFindParameterByName(NodeId, CPMConst, Info);
				if (ValueId >= 0)
				{
					GeneratingParameterName = FString(CPMConst);
					return ValueId;
				}
			}
		}
	}

	// Attempt to get the COP Preview Material default parameter.
	ValueId = FHoudiniEngineUtils::HapiFindParameterByName(NodeId, CPMDefault, Info);
	if (ValueId >= 0)
	{
		GeneratingParameterName = FString(CPMDefault);
		return ValueId;
	}

	return -1;
}

HAPI_ParmId
FHoudiniMaterialTranslator::FindTextureParam(
	const HAPI_NodeId& NodeId,
	const char* Name,
	const char* NameEnabled,
	const char* Tag,
	const char* TagEnabled,
	const char* CPMName,
	const char* CPMSwitch,
	HAPI_ParmInfo& TextureInfo,
	FString& GeneratingParameterName)
{
	HAPI_ParmId TextureId = -1;

	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		NodeId, Name, NameEnabled, false, false, TextureId, TextureInfo))
	{
		// Found via parm name
		GeneratingParameterName = FString(Name);
		return TextureId;
	}

	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		NodeId, Tag, TagEnabled, true, false, TextureId, TextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterName = FString(Tag);
		return TextureId;
	}

	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		NodeId, CPMName, CPMSwitch, false, true, TextureId, TextureInfo))
	{
		// Found via COP Preview Material parm name
		GeneratingParameterName = FString(CPMName);
		return TextureId;
	}

	return -1;
}

void
FHoudiniMaterialTranslator::PositionExpression(
	UMaterialExpression* Expression,
	int32& MaterialNodeY,
	const float HorizontalPositionScale)
{
	Expression->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX -
		FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * HorizontalPositionScale;
	Expression->MaterialExpressionEditorY = MaterialNodeY;
	MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;
}

bool
FHoudiniMaterialTranslator::RequiresWorldSpaceNormals(HAPI_NodeId HapiMaterial)
{
	// Retrieve space for this normal texture.
	HAPI_ParmInfo ParmInfoNormalType;
	int32 ParmNormalTypeId =
		FHoudiniEngineUtils::HapiFindParameterByTag(HapiMaterial, HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE, ParmInfoNormalType);

	// Retrieve value for normal type choice list (if exists).
	if (ParmNormalTypeId >= 0)
	{
		FString NormalType = TEXT(HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_TANGENT);
		if (ParmInfoNormalType.size > 0 && ParmInfoNormalType.stringValuesIndex >= 0)
		{
			HAPI_StringHandle StringHandle;
			if (FHoudiniApi::GetParmStringValues(
				FHoudiniEngine::Get().GetSession(),
				HapiMaterial, false, &StringHandle, ParmInfoNormalType.stringValuesIndex, ParmInfoNormalType.size) == HAPI_RESULT_SUCCESS)
			{
				// Get the actual string value.
				FString NormalTypeString = TEXT("");
				FHoudiniEngineString HoudiniEngineString(StringHandle);
				if (HoudiniEngineString.ToFString(NormalTypeString))
					NormalType = NormalTypeString;
			}
		}

		if (NormalType.Equals(TEXT(HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_WORLD), ESearchCase::IgnoreCase))
			return false;
	}

	return true;
}

UMaterialExpressionVertexColor*
FHoudiniMaterialTranslator::CreateVertexColorExpression(
	UMaterialExpression* ExistingExpression,
	UMaterial* Material,
	const EObjectFlags& ObjectFlag,
	const FString& GeneratingParameterName)
{
	// If the expression already exists, use that.
	UMaterialExpressionVertexColor* VertexExpression =
		Cast<UMaterialExpressionVertexColor>(FHoudiniMaterialTranslator::MaterialLocateExpression(
			ExistingExpression, UMaterialExpressionVertexColor::StaticClass()));

	// Otherwise, create it.
	if (!IsValid(VertexExpression))
	{
		VertexExpression = NewObject<UMaterialExpressionVertexColor>(
			Material, UMaterialExpressionVertexColor::StaticClass(), NAME_None, ObjectFlag);
		VertexExpression->Desc = GeneratingParameterName;
	}

	_AddMaterialExpression(Material, VertexExpression);

	return VertexExpression;
}

void
FHoudiniMaterialTranslator::GetTextureAndExpression(
	UMaterialExpression*& MatInputExpression,
	const bool bLocateExpression,
	UTexture2D*& OutTexture,
	UMaterialExpressionTextureSampleParameter2D*& OutExpression)
{
	if (bLocateExpression)
		OutExpression =
			Cast<UMaterialExpressionTextureSampleParameter2D>(FHoudiniMaterialTranslator::MaterialLocateExpression(
				MatInputExpression, UMaterialExpressionTextureSampleParameter2D::StaticClass()));
	else
		OutExpression =
			Cast<UMaterialExpressionTextureSampleParameter2D>(MatInputExpression);

	if (IsValid(OutExpression))
		OutTexture = Cast<UTexture2D>(OutExpression->Texture);
	else
	{
		OutTexture = nullptr;

		// The input expression is not a texture expression. Destroy it.
		if (MatInputExpression)
		{
			MatInputExpression->ConditionalBeginDestroy();
			MatInputExpression = nullptr;
		}
	}
}

UMaterialExpressionScalarParameter*
FHoudiniMaterialTranslator::CreateScalarExpression(
	UMaterialExpression* ExistingExpression,
	UMaterial* Material,
	const EObjectFlags& ObjectFlag,
	const FString& GeneratingParameterName)
{
	// If the expression already exists, use that.
	UMaterialExpressionScalarParameter* ScalarExpression =
		Cast<UMaterialExpressionScalarParameter>(FHoudiniMaterialTranslator::MaterialLocateExpression(
			ExistingExpression, UMaterialExpressionScalarParameter::StaticClass()));

	// Otherwise, create it.
	if (!IsValid(ScalarExpression))
	{
		ScalarExpression = NewObject<UMaterialExpressionScalarParameter>(
			Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
		ScalarExpression->Desc = GeneratingParameterName;
	}

	_AddMaterialExpression(Material, ScalarExpression);

	return ScalarExpression;
}

UMaterialExpressionVectorParameter*
FHoudiniMaterialTranslator::CreateColorExpression(
	UMaterialExpression* ExistingExpression,
	UMaterial* Material,
	const EObjectFlags& ObjectFlag)
{
	// If the expression already exists, use that.
	UMaterialExpressionVectorParameter* ColorExpression =
		Cast<UMaterialExpressionVectorParameter>(FHoudiniMaterialTranslator::MaterialLocateExpression(
			ExistingExpression, UMaterialExpressionVectorParameter::StaticClass()));

	// Otherwise, create it.
	if (!IsValid(ColorExpression))
	{
		ColorExpression = NewObject<UMaterialExpressionVectorParameter>(
			Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag);
		ColorExpression->DefaultValue = FLinearColor::White;
	}

	_AddMaterialExpression(Material, ColorExpression);

	return ColorExpression;
}

bool
FHoudiniMaterialTranslator::SetColorExpression(
	const HAPI_NodeId& NodeId,
	const char* ParamName,
	const char* ParamTag,
	const char* ParamCPMConst,
	const char* ParamCPMDefault,
	const char* ParamCPMSwitch,
	UMaterialExpressionVectorParameter* ColorExpression,
	FString& GeneratingParameterName)
{
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniMaterialTranslator::FindConstantParam(
		NodeId, ParamName, ParamTag, ParamCPMConst, ParamCPMDefault, ParamCPMSwitch, ParmInfo, GeneratingParameterName);

	if (ParmId >= 0)
	{
		FLinearColor Color = FLinearColor::White;
		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), NodeId, (float*)&Color.R,
			ParmInfo.floatValuesIndex, ParmInfo.size) == HAPI_RESULT_SUCCESS)
		{
			if (ParmInfo.size == 3)
				Color.A = 1.0f;

			// Record generating parameter.
			ColorExpression->Desc = GeneratingParameterName;
			ColorExpression->ParameterName = *GeneratingParameterName;
			ColorExpression->DefaultValue = Color;

			return true;
		}
	}

	return false;
}

UMaterialExpressionMultiply*
FHoudiniMaterialTranslator::CreateMultiplyExpressions(
	UMaterialExpression* MatInputExpression,
	UMaterialExpression* ExpressionA,
	UMaterialExpression* ExpressionB,
	UMaterialExpression* ExpressionC,
	UMaterial* Material,
	int32& MaterialNodeY,
	const EObjectFlags& ObjectFlag)
{
	// If a multiply expression is already attached to the material's input, use that one. Otherwise, create a new one.
	UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(MatInputExpression);
	if (!IsValid(Multiply))
		Multiply = NewObject<UMaterialExpressionMultiply>(
			Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

	_AddMaterialExpression(Material, Multiply);

	// See if primary multiplication has secondary multiplication as input A.
	UMaterialExpressionMultiply* MultiplySecondary = nullptr;
	if (Multiply->A.Expression)
		MultiplySecondary = Cast<UMaterialExpressionMultiply>(Multiply->A.Expression);

	if (ExpressionC)
	{
		// If third expression is provided, create the second multiplication expression.
		if (!MultiplySecondary)
		{
			MultiplySecondary = NewObject<UMaterialExpressionMultiply>(
				Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

			_AddMaterialExpression(Material, MultiplySecondary);
		}
	}
	else
	{
		// If third expression does not exist, destroy the second multiplication expression.
		if (MultiplySecondary)
		{
			MultiplySecondary->A.Expression = nullptr;
			MultiplySecondary->B.Expression = nullptr;
			MultiplySecondary->ConditionalBeginDestroy();
		}
	}

	Multiply->A.Expression = ExpressionA;
	Multiply->B.Expression = ExpressionB;

	// Position the expressions.
	float HorizontalPositionScale = 1.0f;
	if (MultiplySecondary)
		HorizontalPositionScale = 1.5f;

	FHoudiniMaterialTranslator::PositionExpression(ExpressionA, MaterialNodeY, HorizontalPositionScale);
	FHoudiniMaterialTranslator::PositionExpression(ExpressionB, MaterialNodeY, HorizontalPositionScale);

	Multiply->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
	Multiply->MaterialExpressionEditorY =
		(ExpressionB->MaterialExpressionEditorY + ExpressionA->MaterialExpressionEditorY) / 2;

	// Hook up secondary multiplication expression.
	if (MultiplySecondary)
	{
		MultiplySecondary->A.Expression = Multiply;
		MultiplySecondary->B.Expression = ExpressionC;

		FHoudiniMaterialTranslator::PositionExpression(ExpressionC, MaterialNodeY, HorizontalPositionScale);

		MultiplySecondary->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
		MultiplySecondary->MaterialExpressionEditorY =
			Multiply->MaterialExpressionEditorY + FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		return MultiplySecondary;
	}
	else
	{
		return Multiply;
	}
}

bool
FHoudiniMaterialTranslator::CreateScalarExpressionFromFloatParam(
	HAPI_NodeId Node,
	const char* ParamName,
	const char* ParamTag,
	const char* ParamCPMConst,
	const char* ParamCPMDefault,
	const char* ParamCPMSwitch,
	UMaterialExpression*& ExistingExpression,
	UMaterial* Material,
	int32& MaterialNodeY,
	const EObjectFlags& ObjectFlag)
{
	// Find parameter.
	FString GeneratingParameterName = TEXT("");
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniMaterialTranslator::FindConstantParam(
		Node, ParamName, ParamTag, ParamCPMConst, ParamCPMDefault, ParamCPMSwitch, ParmInfo, GeneratingParameterName);
	if (ParmId < 0)
		return false;

	// Get parameter value.
	float Value = 0.0f;
	HAPI_Result Result = FHoudiniApi::GetParmFloatValues(
		FHoudiniEngine::Get().GetSession(), Node, (float*)&Value, ParmInfo.floatValuesIndex, 1);
	if (Result != HAPI_RESULT_SUCCESS)
		return false;

	// Clamp retrieved value.
	Value = FMath::Clamp<float>(Value, 0.0f, 1.0f);

	// If there's already an input expression, check if it's a scalar expression.
	UMaterialExpressionScalarParameter* Expression =
		Cast<UMaterialExpressionScalarParameter>(ExistingExpression);

	if (!Expression)
	{
		if (ExistingExpression)
		{
			// The input expression is not a scalar expression. Destroy it.
			ExistingExpression->ConditionalBeginDestroy();
			ExistingExpression = nullptr;
		}

		Expression = NewObject<UMaterialExpressionScalarParameter>(
			Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
	}

	Expression->Desc = GeneratingParameterName;
	Expression->ParameterName = *GeneratingParameterName;

	Expression->DefaultValue = Value;
	Expression->SliderMin = 0.0f;
	Expression->SliderMax = 1.0f;

	FHoudiniMaterialTranslator::PositionExpression(Expression, MaterialNodeY, 0.0f);

	_AddMaterialExpression(Material, Expression);
	ExistingExpression = Expression;

	return true;
}
