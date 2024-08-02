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

#include "MaterialTypes.h"
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
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InMaterials,
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& OutMaterials,
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
	UMaterialFactoryNew * MaterialFactory = NewObject<UMaterialFactoryNew>();
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
		UMaterial * Material = nullptr;
		UMaterialInterface* const * FoundMaterial = InMaterials.Find(MaterialIdentifier);
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
			UPackage * MaterialPackage = FHoudiniMaterialTranslator::CreatePackageForMaterial(
				MaterialInfo.nodeId, MaterialName, InPackageParams, MaterialPackageName);

			Material = (UMaterial *)MaterialFactory->FactoryCreateNew(
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
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InMaterials,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& OutMaterials,
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
		UPackage * MaterialInstancePackage = nullptr;
		UMaterialInterface * const * FoundMatPtr = InMaterials.Find(Identifier);
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
			UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject< UMaterialInstanceConstantFactoryNew >();
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
			MaterialInstancePackage->SetDirtyFlag( true );
			MaterialInstancePackage->FullyLoad();
			UPackage::SavePackage(
				MaterialInstancePackage, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
				*FPackageName::LongPackageNameToFilename( MaterialInstancePackage->GetName(), FPackageName::GetAssetPackageExtension() ) );
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
	const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InMaterials,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& OutMaterials,
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
	MyPackageParams.PackageMode = FHoudiniPackageParams::GetDefaultMaterialAndTextureCookMode();

	return MyPackageParams.CreatePackageForObject(OutMaterialName);
}


UPackage*
FHoudiniMaterialTranslator::CreatePackageForTexture(
	const HAPI_NodeId& InMaterialNodeId,
	const FString& InTextureType,
	const FHoudiniPackageParams& InPackageParams,
	FString& OutTextureName)
{
	FString TextureInfoDescriptor = TEXT("_texture_") + FString::FromInt(InMaterialNodeId) + TEXT("_") + InTextureType;
	FHoudiniPackageParams MyPackageParams = InPackageParams;
	if (!MyPackageParams.ObjectName.IsEmpty())
	{
		MyPackageParams.ObjectName += TextureInfoDescriptor;
	}
	else if (!MyPackageParams.HoudiniAssetName.IsEmpty())
	{
		MyPackageParams.ObjectName = MyPackageParams.HoudiniAssetName + TextureInfoDescriptor;
	}
	else
	{
		MyPackageParams.ObjectName = TextureInfoDescriptor;
	}
	MyPackageParams.PackageMode = FHoudiniPackageParams::GetDefaultMaterialAndTextureCookMode();

	return MyPackageParams.CreatePackageForObject(OutTextureName);
}


UTexture2D *
FHoudiniMaterialTranslator::CreateUnrealTexture(
	UTexture2D* ExistingTexture,
	const HAPI_ImageInfo& ImageInfo,
	UPackage* Package,
	const FString& TextureName,
	const TArray<char>& ImageBuffer,
	const FCreateTexture2DParameters& TextureParameters,
	const TextureGroup& LODGroup, 
	const FString& TextureType,
	const FString& NodePath)
{
	if (!IsValid(Package))
		return nullptr;

	UTexture2D * Texture = nullptr;
	if (ExistingTexture)
	{
		Texture = ExistingTexture;
	}
	else
	{
		// Create new texture object.
		Texture = NewObject< UTexture2D >(
			Package, UTexture2D::StaticClass(), *TextureName,
			RF_Transactional);

		// Assign texture group.
		Texture->LODGroup = LODGroup;
	}

	// Add/Update meta information to package.
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName);
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Texture, HAPI_UNREAL_PACKAGE_META_NODE_PATH, *NodePath);

	// Initialize texture source.
	Texture->Source.Init(ImageInfo.xRes, ImageInfo.yRes, 1, 1, TSF_BGRA8);

	// Lock the texture.
	uint8 * MipData = Texture->Source.LockMip(0);

	// Create base map.
	uint8* DestPtr = nullptr;
	uint32 SrcWidth = ImageInfo.xRes;
	uint32 SrcHeight = ImageInfo.yRes;
	const char * SrcData = &ImageBuffer[0];

	// Handle the different packing for the source Houdini texture
	uint32 PackOffset = 4;
	uint32 OffsetR = 0;
	uint32 OffsetG = 1;
	uint32 OffsetB = 2;
	uint32 OffsetA = 3;
	switch (ImageInfo.packing)
	{
		case HAPI_IMAGE_PACKING_SINGLE:
			PackOffset = 1;
			OffsetR = 0;
			OffsetG = 0;
			OffsetB = 0;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_DUAL:
			PackOffset = 2;
			OffsetR = 0;
			OffsetG = 1;
			OffsetB = 1;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_RGB:
			PackOffset = 3;
			OffsetR = 0;
			OffsetG = 1;
			OffsetB = 2;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_BGR:
			PackOffset = 3;
			OffsetR = 2;
			OffsetG = 1;
			OffsetB = 0;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_RGBA:
			PackOffset = 4;
			OffsetR = 0;
			OffsetG = 1;
			OffsetB = 2;
			OffsetA = 3;
			break;

		case HAPI_IMAGE_PACKING_ABGR:
			PackOffset = 4;
			OffsetR = 3;
			OffsetG = 2;
			OffsetB = 1;
			OffsetA = 0;
			break;

		case HAPI_IMAGE_PACKING_UNKNOWN:
		case HAPI_IMAGE_PACKING_MAX:
			// invalid packing
			HOUDINI_CHECK_RETURN(false, nullptr);
			break;
	}

	for (uint32 y = 0; y < SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];

		for (uint32 x = 0; x < SrcWidth; x++)
		{
			uint32 DataOffset = y * SrcWidth * PackOffset + x * PackOffset;

			*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetB); // B
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetG); // G
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetR); // R

			if (TextureParameters.bUseAlpha && PackOffset == 4)
				*DestPtr++ = *(uint8*)(SrcData + DataOffset + OffsetA); // A
			else
				*DestPtr++ = 0xFF;
		}
	}

	bool bHasAlphaValue = false;
	if (TextureParameters.bUseAlpha)
	{
		// See if there is an actual alpha value in the texture or if we can ignore the texture alpha
		for (uint32 y = 0; y < SrcHeight; y++)
		{
			for (uint32 x = 0; x < SrcWidth; x++)
			{
				uint32 DataOffset = y * SrcWidth * 4 + x * 4;
				if (*(uint8*)(SrcData + DataOffset + 3) != 0xFF)
				{
					bHasAlphaValue = true;
					break;
				}
			}

			if (bHasAlphaValue)
				break;
		}
	}

	// Unlock the texture.
	Texture->Source.UnlockMip(0);

	// Texture creation parameters.
	Texture->SRGB = TextureParameters.bSRGB;
	Texture->CompressionSettings = TextureParameters.CompressionSettings;
	Texture->CompressionNoAlpha = !bHasAlphaValue;
	Texture->DeferCompression = TextureParameters.bDeferCompression;

	// Set the Source Guid/Hash if specified.
	/*
	if ( TextureParameters.SourceGuidHash.IsValid() )
	{
		Texture->Source.SetId( TextureParameters.SourceGuidHash, true );
	}
	*/

	Texture->PostEditChange();

	return Texture;
}



bool
FHoudiniMaterialTranslator::HapiExtractImage(
	const HAPI_ParmId& NodeParmId, 
	const HAPI_MaterialInfo& MaterialInfo,
	const char * PlaneType,
	const HAPI_ImageDataFormat& ImageDataFormat,
	HAPI_ImagePacking ImagePacking,
	bool bRenderToImage,
	TArray<char>& OutImageBuffer )
{
	if (bRenderToImage)
	{
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::RenderTextureToImage(
			FHoudiniEngine::Get().GetSession(),
			MaterialInfo.nodeId, NodeParmId), false);
	}

	// See if we have the images planes we want
	int NumImagePlanes = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlaneCount(
		FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, &NumImagePlanes), false);

	TArray<int32> ImagePlanesSHArray;
	ImagePlanesSHArray.SetNum(NumImagePlanes);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlanes(
		FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, ImagePlanesSHArray.GetData(), NumImagePlanes), false);

	TArray<FString> ImagePlanesStringArray;
	FHoudiniEngineString::SHArrayToFStringArray(ImagePlanesSHArray, ImagePlanesStringArray);

	bool bFound = false;
	bool bCFound = false;
	bool bAFound = false;
	FString InPlaneTypeString(PlaneType);
	for (int32 n = 0; n < ImagePlanesStringArray.Num(); n++)
	{
		if (ImagePlanesStringArray[n].Equals(InPlaneTypeString, ESearchCase::IgnoreCase))
			bFound = true;
		else if (InPlaneTypeString.Equals("C A"))
		{
			if (ImagePlanesStringArray[n].Equals("C"))
			{				
				bCFound = true;
				// If only color is found, still allow image extraction
				bFound = true;
			}				
			else if (ImagePlanesStringArray[n].Equals("A"))
			{
				bAFound = true;
			}

			if (bCFound && bAFound)
				bFound = true;
		}
	}

	if (!bFound)
		return false;

	HAPI_ImageInfo ImageInfo;
	FHoudiniApi::ImageInfo_Init(&ImageInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetImageInfo(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, &ImageInfo), false);

	ImageInfo.dataFormat = ImageDataFormat;
	ImageInfo.interleaved = true;
	ImageInfo.packing = ImagePacking;

	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetImageInfo(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, &ImageInfo), false);

	int32 ImageBufferSize = 0;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ExtractImageToMemory(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, HAPI_RAW_FORMAT_NAME,
		PlaneType, &ImageBufferSize), false);

	if (ImageBufferSize <= 0)
		return false;

	OutImageBuffer.SetNumUninitialized(ImageBufferSize);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImageMemoryBuffer(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, &OutImageBuffer[0],
		ImageBufferSize), false);

	return true;
}

bool
FHoudiniMaterialTranslator::HapiGetImagePlanes(
	const HAPI_ParmId& NodeParmId, const HAPI_MaterialInfo& MaterialInfo, TArray<FString>& OutImagePlanes)
{
	OutImagePlanes.Empty();
		
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::RenderTextureToImage(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, NodeParmId), false);

	int32 ImagePlaneCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlaneCount(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, &ImagePlaneCount), false);

	if (ImagePlaneCount <= 0)
		return true;

	TArray<HAPI_StringHandle> ImagePlaneStringHandles;
	ImagePlaneStringHandles.SetNumZeroed(ImagePlaneCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetImagePlanes(
		FHoudiniEngine::Get().GetSession(),
		MaterialInfo.nodeId, &ImagePlaneStringHandles[0], ImagePlaneCount), false);
	
	FHoudiniEngineString::SHArrayToFStringArray(ImagePlaneStringHandles, OutImagePlanes);

	return true;
}


UMaterialExpression *
FHoudiniMaterialTranslator::MaterialLocateExpression(UMaterialExpression* Expression, UClass* ExpressionClass)
{
	if (!Expression)
		return nullptr;

#if WITH_EDITOR
	if (ExpressionClass == Expression->GetClass())
		return Expression;

	// If this is a channel multiply expression, we can recurse.
	UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast< UMaterialExpressionMultiply >(Expression);
	if (MaterialExpressionMultiply)
	{
		{
			UMaterialExpression * MaterialExpression = MaterialExpressionMultiply->A.Expression;
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
			UMaterialExpression * MaterialExpression = MaterialExpressionMultiply->B.Expression;
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

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

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
	FColorMaterialInput& MatDiffuse = MaterialEditorOnly->BaseColor;
#else
	FColorMaterialInput& MatDiffuse = Material->BaseColor;
#endif

	// Locate sampling expression.
	UMaterialExpressionTextureSampleParameter2D * ExpressionTextureSample =
		Cast< UMaterialExpressionTextureSampleParameter2D >(FHoudiniMaterialTranslator::MaterialLocateExpression(
			MatDiffuse.Expression, UMaterialExpressionTextureSampleParameter2D::StaticClass()));

	// If texture sampling expression does exist, attempt to look up corresponding texture.
	UTexture2D * TextureDiffuse = nullptr;
	if (IsValid(ExpressionTextureSample))
		TextureDiffuse = Cast< UTexture2D >(ExpressionTextureSample->Texture);

	// Locate uniform color expression.
	UMaterialExpressionVectorParameter * ExpressionConstant4Vector =
		Cast< UMaterialExpressionVectorParameter >(FHoudiniMaterialTranslator::MaterialLocateExpression(
			MatDiffuse.Expression, UMaterialExpressionVectorParameter::StaticClass()));

	// If uniform color expression does not exist, create it.
	if (!IsValid(ExpressionConstant4Vector))
	{
		ExpressionConstant4Vector = NewObject< UMaterialExpressionVectorParameter >(
			Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag);
		ExpressionConstant4Vector->DefaultValue = FLinearColor::White;
	}

	// Add expression.
	_AddMaterialExpression(Material, ExpressionConstant4Vector);

	// Locate vertex color expression.
	UMaterialExpressionVertexColor * ExpressionVertexColor =
		Cast< UMaterialExpressionVertexColor >(FHoudiniMaterialTranslator::MaterialLocateExpression(
			MatDiffuse.Expression, UMaterialExpressionVertexColor::StaticClass()));

	// If vertex color expression does not exist, create it.
	if (!IsValid(ExpressionVertexColor))
	{
		ExpressionVertexColor = NewObject< UMaterialExpressionVertexColor >(
			Material, UMaterialExpressionVertexColor::StaticClass(), NAME_None, ObjectFlag);
		ExpressionVertexColor->Desc = GeneratingParameterNameVertexColor;
	}

	// Add expression.
	_AddMaterialExpression(Material, ExpressionVertexColor);

	// Material should have at least one multiply expression.
	UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast<UMaterialExpressionMultiply>(MatDiffuse.Expression);
	if (!IsValid(MaterialExpressionMultiply))
		MaterialExpressionMultiply = NewObject<UMaterialExpressionMultiply>(
			Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

	// Add expression.
	_AddMaterialExpression(Material, MaterialExpressionMultiply);

	// See if primary multiplication has secondary multiplication as A input.
	UMaterialExpressionMultiply * MaterialExpressionMultiplySecondary = nullptr;
	if (MaterialExpressionMultiply->A.Expression)
		MaterialExpressionMultiplySecondary =
		Cast<UMaterialExpressionMultiply>(MaterialExpressionMultiply->A.Expression);

	// See if a diffuse texture is available.
	HAPI_ParmInfo ParmDiffuseTextureInfo;
	HAPI_ParmId ParmDiffuseTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL_ENABLED,
		true,
		ParmDiffuseTextureId,
		ParmDiffuseTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterNameDiffuseTexture = TEXT(HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE,
		HAPI_UNREAL_PARAM_MAP_DIFFUSE_ENABLED,
		false,
		ParmDiffuseTextureId,
		ParmDiffuseTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterNameDiffuseTexture = TEXT(HAPI_UNREAL_PARAM_MAP_DIFFUSE);
	}
	else
	{
		// failed to find the texture
		ParmDiffuseTextureId = -1;
	}

	// If we have diffuse texture parameter.
	if (ParmDiffuseTextureId >= 0)
	{
		TArray<char> ImageBuffer;

		// Get image planes of diffuse map.
		TArray<FString> DiffuseImagePlanes;
		bool bFoundImagePlanes = FHoudiniMaterialTranslator::HapiGetImagePlanes(
			ParmDiffuseTextureId, InMaterialInfo, DiffuseImagePlanes);

		HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
		const char * PlaneType = "";

		if (bFoundImagePlanes && DiffuseImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_COLOR)))
		{
			if (DiffuseImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA)))
			{
				ImagePacking = HAPI_IMAGE_PACKING_RGBA;
				PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;

				// Material does use alpha.
				CreateTexture2DParameters.bUseAlpha = true;
			}
			else
			{
				// We still need to have the Alpha plane, just not the CreateTexture2DParameters
				// alpha option. This is because all texture data from Houdini Engine contains
				// the alpha plane by default.
				ImagePacking = HAPI_IMAGE_PACKING_RGBA;
				PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
			}
		}
		else
		{
			bFoundImagePlanes = false;
		}

		// Retrieve color plane.
		if (bFoundImagePlanes && FHoudiniMaterialTranslator::HapiExtractImage(
			ParmDiffuseTextureId, InMaterialInfo, PlaneType,
			HAPI_IMAGE_DATA_INT8, ImagePacking, false, ImageBuffer))
		{
			UPackage * TextureDiffusePackage = nullptr;
			if (IsValid(TextureDiffuse))
				TextureDiffusePackage = Cast<UPackage>(TextureDiffuse->GetOuter());

			HAPI_ImageInfo ImageInfo;
			FHoudiniApi::ImageInfo_Init(&ImageInfo);
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureDiffuseName;
				bool bCreatedNewTextureDiffuse = false;

				// Create diffuse texture package, if this is a new diffuse texture.
				if (!TextureDiffusePackage)
				{
					TextureDiffusePackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
						InPackageParams,
						TextureDiffuseName);
				}
				else if (IsValid(TextureDiffuse))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureDiffuseName = TextureDiffuse->GetName();
				}
				else
				{
					TextureDiffuseName = FPaths::GetBaseFilename(TextureDiffusePackage->GetName(), true);
				}

				// Create diffuse texture, if we need to create one.
				if (!IsValid(TextureDiffuse))
					bCreatedNewTextureDiffuse = true;

				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing diffuse texture, or create new one.
				TextureDiffuse = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureDiffuse,
					ImageInfo,
					TextureDiffusePackage,
					TextureDiffuseName,
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_World,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
					NodePath);

				//if (BakeMode == EBakeMode::CookToTemp)
				TextureDiffuse->SetFlags(RF_Public | RF_Standalone);

				// Create diffuse sampling expression, if needed.
				if (!ExpressionTextureSample)
				{
					ExpressionTextureSample = NewObject<UMaterialExpressionTextureSampleParameter2D>(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);
				}

				// Record generating parameter.
				ExpressionTextureSample->Desc = GeneratingParameterNameDiffuseTexture;
				ExpressionTextureSample->ParameterName = *GeneratingParameterNameDiffuseTexture;
				ExpressionTextureSample->Texture = TextureDiffuse;
				ExpressionTextureSample->SamplerType = SAMPLERTYPE_Color;

				// Add expression.
				_AddMaterialExpression(Material, ExpressionTextureSample);

				// Propagate and trigger diffuse texture updates.
				if (bCreatedNewTextureDiffuse)
					FAssetRegistryModule::AssetCreated(TextureDiffuse);

				TextureDiffuse->PreEditChange(nullptr);
				TextureDiffuse->PostEditChange();
				TextureDiffuse->MarkPackageDirty();
			}

			// Cache the texture package
			OutPackages.AddUnique(TextureDiffusePackage);
		}
	}

	// See if uniform color is available.
	HAPI_ParmInfo ParmDiffuseColorInfo;
	HAPI_ParmId ParmDiffuseColorId =
		FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_COLOR_DIFFUSE_OGL, ParmDiffuseColorInfo);

	if (ParmDiffuseColorId >= 0)
	{
		GeneratingParameterNameUniformColor = TEXT(HAPI_UNREAL_PARAM_COLOR_DIFFUSE_OGL);
	}
	else
	{
		ParmDiffuseColorId =
			FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_COLOR_DIFFUSE, ParmDiffuseColorInfo);

		if (ParmDiffuseColorId >= 0)
			GeneratingParameterNameUniformColor = TEXT(HAPI_UNREAL_PARAM_COLOR_DIFFUSE);
	}

	// If we have uniform color parameter.
	if (ParmDiffuseColorId >= 0)
	{
		FLinearColor Color = FLinearColor::White;

		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, (float *)&Color.R,
			ParmDiffuseColorInfo.floatValuesIndex, ParmDiffuseColorInfo.size) == HAPI_RESULT_SUCCESS)
		{
			if (ParmDiffuseColorInfo.size == 3)
				Color.A = 1.0f;

			// Record generating parameter.
			ExpressionConstant4Vector->Desc = GeneratingParameterNameUniformColor;
			ExpressionConstant4Vector->ParameterName = *GeneratingParameterNameUniformColor;
			ExpressionConstant4Vector->DefaultValue = Color;
		}
	}

	// If we have have texture sample expression present, we need a secondary multiplication expression.
	if (ExpressionTextureSample)
	{
		if (!MaterialExpressionMultiplySecondary)
		{
			MaterialExpressionMultiplySecondary = NewObject<UMaterialExpressionMultiply>(
				Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

			// Add expression.
			_AddMaterialExpression(Material, MaterialExpressionMultiplySecondary);
		}
	}
	else
	{
		// If secondary multiplication exists, but we have no sampling, we can free it.
		if (MaterialExpressionMultiplySecondary)
		{
			MaterialExpressionMultiplySecondary->A.Expression = nullptr;
			MaterialExpressionMultiplySecondary->B.Expression = nullptr;
			MaterialExpressionMultiplySecondary->ConditionalBeginDestroy();
		}
	}

	float SecondaryExpressionScale = 1.0f;
	if (MaterialExpressionMultiplySecondary)
		SecondaryExpressionScale = 1.5f;

	// Create multiplication expression which has uniform color and vertex color.
	MaterialExpressionMultiply->A.Expression = ExpressionConstant4Vector;
	MaterialExpressionMultiply->B.Expression = ExpressionVertexColor;

	ExpressionConstant4Vector->MaterialExpressionEditorX =
		FHoudiniMaterialTranslator::MaterialExpressionNodeX -
		FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * SecondaryExpressionScale;
	ExpressionConstant4Vector->MaterialExpressionEditorY = MaterialNodeY;
	MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

	ExpressionVertexColor->MaterialExpressionEditorX =
		FHoudiniMaterialTranslator::MaterialExpressionNodeX -
		FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * SecondaryExpressionScale;
	ExpressionVertexColor->MaterialExpressionEditorY = MaterialNodeY;
	MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

	MaterialExpressionMultiply->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
	MaterialExpressionMultiply->MaterialExpressionEditorY =
		(ExpressionVertexColor->MaterialExpressionEditorY + ExpressionConstant4Vector->MaterialExpressionEditorY) / 2;

	// Hook up secondary multiplication expression to first one.
	if (MaterialExpressionMultiplySecondary)
	{
		MaterialExpressionMultiplySecondary->A.Expression = MaterialExpressionMultiply;
		MaterialExpressionMultiplySecondary->B.Expression = ExpressionTextureSample;

		if (ExpressionTextureSample)
		{
			ExpressionTextureSample->MaterialExpressionEditorX =
				FHoudiniMaterialTranslator::MaterialExpressionNodeX -
				FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * SecondaryExpressionScale;
			ExpressionTextureSample->MaterialExpressionEditorY = MaterialNodeY;
		}		

		MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		MaterialExpressionMultiplySecondary->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
		MaterialExpressionMultiplySecondary->MaterialExpressionEditorY =
			MaterialExpressionMultiply->MaterialExpressionEditorY + FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		// Assign expression.
		MatDiffuse.Expression = MaterialExpressionMultiplySecondary;
	}
	else
	{
		// Assign expression.
		MatDiffuse.Expression = MaterialExpressionMultiply;
	}

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
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Name of generating Houdini parameters.
	FString GeneratingParameterNameTexture = TEXT("");

	// Attempt to look up previously created expressions.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatOpacityMask = MaterialEditorOnly->OpacityMask;
#else
	FScalarMaterialInput& MatOpacityMask = Material->OpacityMask;
#endif

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Opacity expressions.
	UMaterialExpressionTextureSampleParameter2D * ExpressionTextureOpacitySample = nullptr;
	UTexture2D * TextureOpacity = nullptr;

	// Opacity texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = true;

	// See if opacity texture is available.
	HAPI_ParmInfo ParmOpacityTextureInfo;
	HAPI_ParmId ParmOpacityTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_OPACITY_OGL,
		HAPI_UNREAL_PARAM_MAP_OPACITY_OGL_ENABLED,
		true,
		ParmOpacityTextureId,
		ParmOpacityTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterNameTexture = TEXT(HAPI_UNREAL_PARAM_MAP_OPACITY_OGL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_OPACITY,
		HAPI_UNREAL_PARAM_MAP_OPACITY_ENABLED,
		false,
		ParmOpacityTextureId,
		ParmOpacityTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterNameTexture = TEXT(HAPI_UNREAL_PARAM_MAP_OPACITY);
	}
	else
	{
		// failed to find the texture
		ParmOpacityTextureId = -1;
	}

	// If we have opacity texture parameter.
	if (ParmOpacityTextureId >= 0)
	{
		TArray<char> ImageBuffer;

		// Get image planes of opacity map.
		TArray<FString> OpacityImagePlanes;
		bool bFoundImagePlanes = FHoudiniMaterialTranslator::HapiGetImagePlanes(
			ParmOpacityTextureId, InMaterialInfo, OpacityImagePlanes);

		HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
		const char * PlaneType = "";

		bool bColorAlphaFound = (OpacityImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA)) && OpacityImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_COLOR)));

		if (bFoundImagePlanes && OpacityImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_COLOR)))
		{
			if (OpacityImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA)))
			{
				ImagePacking = HAPI_IMAGE_PACKING_RGBA;
				PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
				CreateTexture2DParameters.bUseAlpha = true;
			}
			else
			{
				// We still need to have the Alpha plane, just not the CreateTexture2DParameters
				// alpha option. This is because all texture data from Houdini Engine contains
				// the alpha plane by default.
				ImagePacking = HAPI_IMAGE_PACKING_RGBA;
				PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA; 
				CreateTexture2DParameters.bUseAlpha = false;
			}
		}
		else
		{
			bFoundImagePlanes = false;
		}

		if (bFoundImagePlanes && FHoudiniMaterialTranslator::HapiExtractImage(
			ParmOpacityTextureId, InMaterialInfo, PlaneType,
			HAPI_IMAGE_DATA_INT8, ImagePacking, false, ImageBuffer))
		{
			// Locate sampling expression.
			ExpressionTextureOpacitySample = Cast<UMaterialExpressionTextureSampleParameter2D>(
				FHoudiniMaterialTranslator::MaterialLocateExpression(
					MatOpacityMask.Expression, UMaterialExpressionTextureSampleParameter2D::StaticClass()));

			// Locate opacity texture, if valid.
			if (ExpressionTextureOpacitySample)
				TextureOpacity = Cast< UTexture2D >(ExpressionTextureOpacitySample->Texture);

			UPackage * TextureOpacityPackage = nullptr;
			if (TextureOpacity)
				TextureOpacityPackage = Cast< UPackage >(TextureOpacity->GetOuter());

			HAPI_ImageInfo ImageInfo;
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureOpacityName;
				bool bCreatedNewTextureOpacity = false;

				// Create opacity texture package, if this is a new opacity texture.
				if (!TextureOpacityPackage)
				{
					TextureOpacityPackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
						InPackageParams,
						TextureOpacityName);
				}
				else if (IsValid(TextureOpacity))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureOpacityName = TextureOpacity->GetName();
				}
				else
				{
					TextureOpacityName = FPaths::GetBaseFilename(TextureOpacityPackage->GetName(), true);
				}

				// Create opacity texture, if we need to create one.
				if (!TextureOpacity)
					bCreatedNewTextureOpacity = true;

				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing opacity texture, or create new one.
				TextureOpacity = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureOpacity,
					ImageInfo,
					TextureOpacityPackage, 
					TextureOpacityName, 
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_World,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
					NodePath);

 				// if (BakeMode == EBakeMode::CookToTemp)
				TextureOpacity->SetFlags(RF_Public | RF_Standalone);

				// Create opacity sampling expression, if needed.
				if (!ExpressionTextureOpacitySample)
				{
					ExpressionTextureOpacitySample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);
				}

				// Record generating parameter.
				ExpressionTextureOpacitySample->Desc = GeneratingParameterNameTexture;
				ExpressionTextureOpacitySample->ParameterName = *GeneratingParameterNameTexture;
				ExpressionTextureOpacitySample->Texture = TextureOpacity;
				ExpressionTextureOpacitySample->SamplerType = SAMPLERTYPE_Grayscale;

				// Offset node placement.
				ExpressionTextureOpacitySample->MaterialExpressionEditorX =
					FHoudiniMaterialTranslator::MaterialExpressionNodeX;
				ExpressionTextureOpacitySample->MaterialExpressionEditorY = MaterialNodeY;
				MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

				// Add expression.
				_AddMaterialExpression(Material, ExpressionTextureOpacitySample);

				// We need to set material type to masked.
				TArray< FExpressionOutput > ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
				FExpressionOutput* ExpressionOutput = ExpressionOutputs.GetData();

				MatOpacityMask.Expression = ExpressionTextureOpacitySample;
				Material->BlendMode = BLEND_Masked;

				MatOpacityMask.Mask = ExpressionOutput->Mask;
				MatOpacityMask.MaskR = 1;
				MatOpacityMask.MaskG = 0;
				MatOpacityMask.MaskB = 0;
				MatOpacityMask.MaskA = 0;

				// Propagate and trigger opacity texture updates.
				if (bCreatedNewTextureOpacity)
					FAssetRegistryModule::AssetCreated(TextureOpacity);

				TextureOpacity->PreEditChange(nullptr);
				TextureOpacity->PostEditChange();
				TextureOpacity->MarkPackageDirty();

				bExpressionCreated = true;
			}

			// Switch the material's blend mode to Masked
			Material->BlendMode = BLEND_Masked;

			// Cache the texture package
			OutPackages.AddUnique(TextureOpacityPackage);
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
	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	float OpacityValue = 1.0f;
	bool bNeedsTranslucency = false;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Name of generating Houdini parameters.
	FString GeneratingParameterNameScalar = TEXT("");
	FString GeneratingParameterNameTexture = TEXT("");

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatOpacity = MaterialEditorOnly->Opacity;
#else
	FScalarMaterialInput& MatOpacity = Material->Opacity;
#endif

	// Opacity expressions.
	UMaterialExpressionTextureSampleParameter2D * ExpressionTextureOpacitySample = nullptr;
	UMaterialExpressionScalarParameter * ExpressionScalarOpacity = nullptr;
	UTexture2D * TextureOpacity = nullptr;

	// Opacity texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = true;

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
			UMaterialExpressionTextureSampleParameter2D * ExpressionTextureDiffuseSample =
				Cast< UMaterialExpressionTextureSampleParameter2D >(
					FHoudiniMaterialTranslator::MaterialLocateExpression(
						MaterialExpressionDiffuse,
						UMaterialExpressionTextureSampleParameter2D::StaticClass()));

			// See if there's an alpha plane in this expression's texture.
			if (ExpressionTextureDiffuseSample)
			{
				UTexture2D * DiffuseTexture = Cast< UTexture2D >(ExpressionTextureDiffuseSample->Texture);
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
		FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_ALPHA_OGL, ParmOpacityValueInfo);

	if (ParmOpacityValueId >= 0)
	{
		GeneratingParameterNameScalar = TEXT(HAPI_UNREAL_PARAM_ALPHA_OGL);
	}
	else
	{
		ParmOpacityValueId =
			FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_ALPHA, ParmOpacityValueInfo);

		if (ParmOpacityValueId >= 0)
			GeneratingParameterNameScalar = TEXT(HAPI_UNREAL_PARAM_ALPHA);
	}

	if (ParmOpacityValueId >= 0)
	{
		if (ParmOpacityValueInfo.size > 0 && ParmOpacityValueInfo.floatValuesIndex >= 0)
		{
			float OpacityValueRetrieved = 1.0f;
			if (FHoudiniApi::GetParmFloatValues(
				FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId,
				(float *)&OpacityValue, ParmOpacityValueInfo.floatValuesIndex, 1) == HAPI_RESULT_SUCCESS)
			{
				if (!ExpressionScalarOpacity)
				{
					ExpressionScalarOpacity = NewObject< UMaterialExpressionScalarParameter >(
						Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
				}

				// Clamp retrieved value.
				OpacityValueRetrieved = FMath::Clamp< float >(OpacityValueRetrieved, 0.0f, 1.0f);
				OpacityValue = OpacityValueRetrieved;

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
		UMaterialExpressionMultiply * ExpressionMultiply =
			Cast< UMaterialExpressionMultiply >(
				FHoudiniMaterialTranslator::MaterialLocateExpression(
					MatOpacity.Expression,
					UMaterialExpressionMultiply::StaticClass()));

		if (!ExpressionMultiply)
			ExpressionMultiply = NewObject< UMaterialExpressionMultiply >(
				Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

		_AddMaterialExpression(Material, ExpressionMultiply);

		TArray< FExpressionOutput > ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
		FExpressionOutput * ExpressionOutput = ExpressionOutputs.GetData();

		ExpressionMultiply->A.Expression = ExpressionTextureOpacitySample;
		ExpressionMultiply->B.Expression = ExpressionScalarOpacity;

		MatOpacity.Expression = ExpressionMultiply;
		MatOpacity.Mask = ExpressionOutput->Mask;
		MatOpacity.MaskR = 0;
		MatOpacity.MaskG = 0;
		MatOpacity.MaskB = 0;
		MatOpacity.MaskA = 1;

		ExpressionMultiply->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
		ExpressionMultiply->MaterialExpressionEditorY = MaterialNodeY;

		ExpressionScalarOpacity->MaterialExpressionEditorX =
			FHoudiniMaterialTranslator::MaterialExpressionNodeX - FHoudiniMaterialTranslator::MaterialExpressionNodeStepX;
		ExpressionScalarOpacity->MaterialExpressionEditorY = MaterialNodeY;
		MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		bExpressionCreated = true;
	}
	else if (ExpressionScalarOpacity)
	{
		MatOpacity.Expression = ExpressionScalarOpacity;

		ExpressionScalarOpacity->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
		ExpressionScalarOpacity->MaterialExpressionEditorY = MaterialNodeY;
		MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		bExpressionCreated = true;
	}
	else if (ExpressionTextureOpacitySample)
	{
		TArray<FExpressionOutput> ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
		FExpressionOutput * ExpressionOutput = ExpressionOutputs.GetData();

		MatOpacity.Expression = ExpressionTextureOpacitySample;
		MatOpacity.Mask = ExpressionOutput->Mask;
		MatOpacity.MaskR = 0;
		MatOpacity.MaskG = 0;
		MatOpacity.MaskB = 0;
		MatOpacity.MaskA = 1;

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
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

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

	// See if separate normal texture is available.
	HAPI_ParmInfo ParmNormalTextureInfo;
	HAPI_ParmId ParmNormalTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_NORMAL,
		HAPI_UNREAL_PARAM_MAP_NORMAL_ENABLED,
		false,
		ParmNormalTextureId,
		ParmNormalTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_NORMAL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_NORMAL_OGL,
		"",
		true,
		ParmNormalTextureId,
		ParmNormalTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_NORMAL_OGL);
	}
	else
	{
		// failed to find the texture
		ParmNormalTextureId = -1;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FVectorMaterialInput& MatNormal = MaterialEditorOnly->Normal;
#else
	FVectorMaterialInput& MatNormal = Material->Normal;
#endif

	if (ParmNormalTextureId >= 0)
	{
		// Retrieve space for this normal texture.
		HAPI_ParmInfo ParmInfoNormalType;
		int32 ParmNormalTypeId =
			FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE, ParmInfoNormalType);

		// Retrieve value for normal type choice list (if exists).
		if (ParmNormalTypeId >= 0)
		{
			FString NormalType = TEXT(HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_TANGENT);
			if (ParmInfoNormalType.size > 0 && ParmInfoNormalType.stringValuesIndex >= 0)
			{
				HAPI_StringHandle StringHandle;
				if (FHoudiniApi::GetParmStringValues(
					FHoudiniEngine::Get().GetSession(),
					InMaterialInfo.nodeId, false, &StringHandle, ParmInfoNormalType.stringValuesIndex, ParmInfoNormalType.size) == HAPI_RESULT_SUCCESS)
				{
					// Get the actual string value.
					FString NormalTypeString = TEXT("");
					FHoudiniEngineString HoudiniEngineString(StringHandle);
					if (HoudiniEngineString.ToFString(NormalTypeString))
						NormalType = NormalTypeString;
				}
			}

			// Check if we require world space normals.
			if (NormalType.Equals(TEXT(HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_WORLD), ESearchCase::IgnoreCase))
				bTangentSpaceNormal = false;
		}
			
		// Retrieve color plane.
		TArray<char> ImageBuffer;
		if (FHoudiniMaterialTranslator::HapiExtractImage(
			ParmNormalTextureId, InMaterialInfo, HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
			HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true, ImageBuffer))
		{
			UMaterialExpressionTextureSampleParameter2D * ExpressionNormal =
				Cast< UMaterialExpressionTextureSampleParameter2D >(MatNormal.Expression);

			UTexture2D * TextureNormal = nullptr;
			if (ExpressionNormal)
			{
				TextureNormal = Cast< UTexture2D >(ExpressionNormal->Texture);
			}
			else
			{
				// Otherwise new expression is of a different type.
				if (MatNormal.Expression)
				{
					MatNormal.Expression->ConditionalBeginDestroy();
					MatNormal.Expression = nullptr;
				}
			}

			UPackage * TextureNormalPackage = nullptr;
			if (TextureNormal)
				TextureNormalPackage = Cast< UPackage >(TextureNormal->GetOuter());

			HAPI_ImageInfo ImageInfo;
			FHoudiniApi::ImageInfo_Init(&ImageInfo);
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureNormalName;
				bool bCreatedNewTextureNormal = false;

				// Create normal texture package, if this is a new normal texture.
				if (!TextureNormalPackage)
				{
					TextureNormalPackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
						InPackageParams,
						TextureNormalName);
				}
				else if (IsValid(TextureNormal))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureNormalName = TextureNormal->GetName();
				}
				else
				{
					TextureNormalName = FPaths::GetBaseFilename(TextureNormalPackage->GetName(), true);
				}

				// Create normal texture, if we need to create one.
				if (!TextureNormal)
					bCreatedNewTextureNormal = true;

				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing normal texture, or create new one.
				TextureNormal = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureNormal,
					ImageInfo,
					TextureNormalPackage,
					TextureNormalName,
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_WorldNormalMap,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
					NodePath);

				//if (BakeMode == EBakeMode::CookToTemp)
				TextureNormal->SetFlags(RF_Public | RF_Standalone);

				// Create normal sampling expression, if needed.
				if (!ExpressionNormal)
					ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);

				// Record generating parameter.
				ExpressionNormal->Desc = GeneratingParameterName;
				ExpressionNormal->ParameterName = *GeneratingParameterName;

				ExpressionNormal->Texture = TextureNormal;
				ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

				// Offset node placement.
				ExpressionNormal->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
				ExpressionNormal->MaterialExpressionEditorY = MaterialNodeY;
				MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

				// Set normal space.
				Material->bTangentSpaceNormal = bTangentSpaceNormal;

				// Assign expression to material.
				_AddMaterialExpression(Material, ExpressionNormal);
				MatNormal.Expression = ExpressionNormal;

				bExpressionCreated = true;

				// Propagate and trigger normal texture updates.
				if (bCreatedNewTextureNormal)
					FAssetRegistryModule::AssetCreated(TextureNormal);

				TextureNormal->PreEditChange(nullptr);
				TextureNormal->PostEditChange();
				TextureNormal->MarkPackageDirty();
			}

			// Cache the texture package
			OutPackages.AddUnique(TextureNormalPackage);
		}
	}

	// If separate normal map was not found, see if normal plane exists in diffuse map.
	if (!bExpressionCreated)
	{
		// See if diffuse texture is available.
		HAPI_ParmInfo ParmDiffuseTextureInfo;
		HAPI_ParmId ParmDiffuseTextureId = -1;
		if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL_ENABLED,
			true,
			ParmDiffuseTextureId,
			ParmDiffuseTextureInfo))
		{
			// Found via OGL tag
			GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_DIFFUSE_OGL);
		}
		else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
			InMaterialInfo.nodeId,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE,
			HAPI_UNREAL_PARAM_MAP_DIFFUSE_ENABLED,
			false,
			ParmDiffuseTextureId,
			ParmDiffuseTextureInfo))
		{
			// Found via Parm name
			GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_DIFFUSE);
		}
		else
		{
			// failed to find the texture
			ParmDiffuseTextureId = -1;
		}

		if (ParmDiffuseTextureId >= 0)
		{
			// Normal plane is available in diffuse map.
			TArray<char> ImageBuffer;

			// Retrieve color plane - this will contain normal data.
			if (FHoudiniMaterialTranslator::HapiExtractImage(
				ParmDiffuseTextureId, InMaterialInfo, HAPI_UNREAL_MATERIAL_TEXTURE_NORMAL,
				HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGB, true, ImageBuffer))
			{
				UMaterialExpressionTextureSampleParameter2D * ExpressionNormal =
					Cast<UMaterialExpressionTextureSampleParameter2D>(MatNormal.Expression);

				UTexture2D* TextureNormal = nullptr;
				if (ExpressionNormal)
				{
					TextureNormal = Cast< UTexture2D >(ExpressionNormal->Texture);
				}
				else
				{
					// Otherwise new expression is of a different type.
					if (MatNormal.Expression)
					{
						MatNormal.Expression->ConditionalBeginDestroy();
						MatNormal.Expression = nullptr;
					}
				}

				UPackage* TextureNormalPackage = nullptr;
				if (TextureNormal)
					TextureNormalPackage = Cast<UPackage>(TextureNormal->GetOuter());

				HAPI_ImageInfo ImageInfo;
				FHoudiniApi::ImageInfo_Init(&ImageInfo);
				Result = FHoudiniApi::GetImageInfo(
					FHoudiniEngine::Get().GetSession(),	InMaterialInfo.nodeId, &ImageInfo);

				if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
				{
					// Create texture.
					FString TextureNormalName;
					bool bCreatedNewTextureNormal = false;

					// Create normal texture package, if this is a new normal texture.
					if (!TextureNormalPackage)
					{
						TextureNormalPackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
							InMaterialInfo.nodeId,
							HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
							InPackageParams,
							TextureNormalName);
					}
					else if (IsValid(TextureNormal))
					{
						// Get the name of the texture if we are overwriting the exist asset
						TextureNormalName = TextureNormal->GetName();
					}
					else
					{
						TextureNormalName = FPaths::GetBaseFilename(TextureNormalPackage->GetName(), true);
					}

					// Create normal texture, if we need to create one.
					if (!TextureNormal)
						bCreatedNewTextureNormal = true;

					FString NodePath;
					FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

					// Reuse existing normal texture, or create new one.
					TextureNormal = FHoudiniMaterialTranslator::CreateUnrealTexture(
						TextureNormal, 
						ImageInfo,
						TextureNormalPackage, 
						TextureNormalName,
						ImageBuffer,
						CreateTexture2DParameters,
						TEXTUREGROUP_WorldNormalMap,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
						NodePath);

					//if (BakeMode == EBakeMode::CookToTemp)
					TextureNormal->SetFlags(RF_Public | RF_Standalone);

					// Create normal sampling expression, if needed.
					if (!ExpressionNormal)
						ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
							Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);

					// Record generating parameter.
					ExpressionNormal->Desc = GeneratingParameterName;
					ExpressionNormal->ParameterName = *GeneratingParameterName;

					ExpressionNormal->Texture = TextureNormal;
					ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

					// Offset node placement.
					ExpressionNormal->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
					ExpressionNormal->MaterialExpressionEditorY = MaterialNodeY;
					MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

					// Set normal space.
					Material->bTangentSpaceNormal = bTangentSpaceNormal;

					// Assign expression to material.
					_AddMaterialExpression(Material, ExpressionNormal);
					MatNormal.Expression = ExpressionNormal;

					// Propagate and trigger diffuse texture updates.
					if (bCreatedNewTextureNormal)
						FAssetRegistryModule::AssetCreated(TextureNormal);

					TextureNormal->PreEditChange(nullptr);
					TextureNormal->PostEditChange();
					TextureNormal->MarkPackageDirty();

					bExpressionCreated = true;
				}

				// Cache the texture package
				OutPackages.AddUnique(TextureNormalPackage);
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
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

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

	// See if specular texture is available.
	HAPI_ParmInfo ParmSpecularTextureInfo;
	HAPI_ParmId ParmSpecularTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_OGL,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_OGL_ENABLED,
		true,
		ParmSpecularTextureId,
		ParmSpecularTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_SPECULAR_OGL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_SPECULAR,
		HAPI_UNREAL_PARAM_MAP_SPECULAR_ENABLED,
		false,
		ParmSpecularTextureId,
		ParmSpecularTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_SPECULAR);
	}
	else
	{
		// failed to find the texture
		ParmSpecularTextureId = -1;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatSpecular = MaterialEditorOnly->Specular;
#else
	FScalarMaterialInput& MatSpecular = Material->Specular;
#endif

	if (ParmSpecularTextureId >= 0)
	{
		TArray<char> ImageBuffer;

		// Retrieve color plane.
		if (FHoudiniMaterialTranslator::HapiExtractImage(
			ParmSpecularTextureId, InMaterialInfo, HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
			HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true, ImageBuffer))
		{
			UMaterialExpressionTextureSampleParameter2D * ExpressionSpecular =
				Cast< UMaterialExpressionTextureSampleParameter2D >(MatSpecular.Expression);

			UTexture2D * TextureSpecular = nullptr;
			if (ExpressionSpecular)
			{
				TextureSpecular = Cast< UTexture2D >(ExpressionSpecular->Texture);
			}
			else
			{
				// Otherwise new expression is of a different type.
				if (MatSpecular.Expression)
				{
					MatSpecular.Expression->ConditionalBeginDestroy();
					MatSpecular.Expression = nullptr;
				}
			}

			UPackage * TextureSpecularPackage = nullptr;
			if (TextureSpecular)
				TextureSpecularPackage = Cast< UPackage >(TextureSpecular->GetOuter());

			HAPI_ImageInfo ImageInfo;
			FHoudiniApi::ImageInfo_Init(&ImageInfo);
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureSpecularName;
				bool bCreatedNewTextureSpecular = false;

				// Create specular texture package, if this is a new specular texture.
				if (!TextureSpecularPackage)
				{
					TextureSpecularPackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
						InPackageParams,
						TextureSpecularName);
				}
				else if (IsValid(TextureSpecular))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureSpecularName = TextureSpecular->GetName();
				}
				else
				{
					TextureSpecularName = FPaths::GetBaseFilename(TextureSpecularPackage->GetName(), true);
				}

				// Create specular texture, if we need to create one.
				if (!TextureSpecular)
					bCreatedNewTextureSpecular = true;

				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing specular texture, or create new one.
				TextureSpecular = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureSpecular,
					ImageInfo,
					TextureSpecularPackage,
					TextureSpecularName,
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_World,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
					NodePath);

				//if (BakeMode == EBakeMode::CookToTemp)
				TextureSpecular->SetFlags(RF_Public | RF_Standalone);

				// Create specular sampling expression, if needed.
				if (!ExpressionSpecular)
				{
					ExpressionSpecular = NewObject< UMaterialExpressionTextureSampleParameter2D >(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);
				}

				// Record generating parameter.
				ExpressionSpecular->Desc = GeneratingParameterName;
				ExpressionSpecular->ParameterName = *GeneratingParameterName;

				ExpressionSpecular->Texture = TextureSpecular;
				ExpressionSpecular->SamplerType = SAMPLERTYPE_LinearGrayscale;

				// Offset node placement.
				ExpressionSpecular->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
				ExpressionSpecular->MaterialExpressionEditorY = MaterialNodeY;
				MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

				// Assign expression to material.
				_AddMaterialExpression(Material, ExpressionSpecular);
				MatSpecular.Expression = ExpressionSpecular;

				bExpressionCreated = true;

				// Propagate and trigger specular texture updates.
				if (bCreatedNewTextureSpecular)
					FAssetRegistryModule::AssetCreated(TextureSpecular);

				TextureSpecular->PreEditChange(nullptr);
				TextureSpecular->PostEditChange();
				TextureSpecular->MarkPackageDirty();
			}

			// Cache the texture package
			OutPackages.AddUnique(TextureSpecularPackage);
		}
	}

	// See if we have a specular color
	HAPI_ParmInfo ParmSpecularValueInfo;
	HAPI_ParmId ParmSpecularValueId =
		FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_COLOR_SPECULAR, ParmSpecularValueInfo);		

	if (ParmSpecularValueId >= 0)
	{
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_COLOR_SPECULAR);
	}
	else
	{
		ParmSpecularValueId = FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_COLOR_SPECULAR_OGL, ParmSpecularValueInfo);			

		if (ParmSpecularValueId >= 0)
			GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_COLOR_SPECULAR_OGL);
	}

	if (!bExpressionCreated && ParmSpecularValueId >= 0)
	{
		// Specular value is available.
		float SpecularValue = 0.0f;
				
		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, (float*)&SpecularValue,
			ParmSpecularValueInfo.floatValuesIndex, 1) == HAPI_RESULT_SUCCESS)
		{
			UMaterialExpressionScalarParameter* ExpressionSpecularValue =
				Cast<UMaterialExpressionScalarParameter>(MatSpecular.Expression);

			// Clamp retrieved value.
			SpecularValue = FMath::Clamp<float>(SpecularValue, 0.0f, 1.0f);

			// Create color const expression and add it to material, if we don't have one.
			if (!ExpressionSpecularValue)
			{
				// Otherwise new expression is of a different type.
				if (MatSpecular.Expression)
				{
					MatSpecular.Expression->ConditionalBeginDestroy();
					MatSpecular.Expression = nullptr;
				}

				ExpressionSpecularValue = NewObject<UMaterialExpressionScalarParameter>(
					Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
			}

			// Record generating parameter.
			ExpressionSpecularValue->Desc = GeneratingParameterName;
			ExpressionSpecularValue->ParameterName = *GeneratingParameterName;

			ExpressionSpecularValue->DefaultValue = SpecularValue;
			ExpressionSpecularValue->SliderMin = 0.0f;
			ExpressionSpecularValue->SliderMax = 1.0f;

			// Offset node placement.
			ExpressionSpecularValue->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
			ExpressionSpecularValue->MaterialExpressionEditorY = MaterialNodeY;
			MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

			// Assign expression to material.
			_AddMaterialExpression(Material, ExpressionSpecularValue);
			MatSpecular.Expression = ExpressionSpecularValue;

			bExpressionCreated = true;
		}
	}

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
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

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

	// See if roughness texture is available.
	HAPI_ParmInfo ParmRoughnessTextureInfo;
	HAPI_ParmId ParmRoughnessTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_OGL,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_OGL_ENABLED,
		true,
		ParmRoughnessTextureId,
		ParmRoughnessTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_ROUGHNESS_OGL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS,
		HAPI_UNREAL_PARAM_MAP_ROUGHNESS_ENABLED,
		false,
		ParmRoughnessTextureId,
		ParmRoughnessTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_ROUGHNESS);
	}
	else
	{
		// failed to find the texture
		ParmRoughnessTextureId = -1;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatRoughness = MaterialEditorOnly->Roughness;
#else
	FScalarMaterialInput& MatRoughness = Material->Roughness;
#endif

	if (ParmRoughnessTextureId >= 0)
	{
		TArray<char> ImageBuffer;
		// Retrieve color plane.
		if (FHoudiniMaterialTranslator::HapiExtractImage(
			ParmRoughnessTextureId, InMaterialInfo, HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
			HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true, ImageBuffer ) )
		{
			UMaterialExpressionTextureSampleParameter2D* ExpressionRoughness =
				Cast< UMaterialExpressionTextureSampleParameter2D >(MatRoughness.Expression);

			UTexture2D* TextureRoughness = nullptr;
			if (ExpressionRoughness)
			{
				TextureRoughness = Cast< UTexture2D >(ExpressionRoughness->Texture);
			}
			else
			{
				// Otherwise new expression is of a different type.
				if (MatRoughness.Expression)
				{
					MatRoughness.Expression->ConditionalBeginDestroy();
					MatRoughness.Expression = nullptr;
				}
			}

			UPackage * TextureRoughnessPackage = nullptr;
			if (TextureRoughness)
				TextureRoughnessPackage = Cast< UPackage >(TextureRoughness->GetOuter());

			HAPI_ImageInfo ImageInfo;
			FHoudiniApi::ImageInfo_Init(&ImageInfo);
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureRoughnessName;
				bool bCreatedNewTextureRoughness = false;

				// Create roughness texture package, if this is a new roughness texture.
				if (!TextureRoughnessPackage)
				{
					TextureRoughnessPackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
						InPackageParams,
						TextureRoughnessName);
				}
				else if (IsValid(TextureRoughness))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureRoughnessName = TextureRoughness->GetName();
				}
				else
				{
					TextureRoughnessName = FPaths::GetBaseFilename(TextureRoughnessPackage->GetName(), true);
				}

				// Create roughness texture, if we need to create one.
				if (!TextureRoughness)
					bCreatedNewTextureRoughness = true;

				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing roughness texture, or create new one.
				TextureRoughness = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureRoughness,
					ImageInfo,
					TextureRoughnessPackage,
					TextureRoughnessName,
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_World,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
					NodePath);

				//if (BakeMode == EBakeMode::CookToTemp)
				TextureRoughness->SetFlags(RF_Public | RF_Standalone);

				// Create roughness sampling expression, if needed.
				if (!ExpressionRoughness)
					ExpressionRoughness = NewObject< UMaterialExpressionTextureSampleParameter2D >(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);

				// Record generating parameter.
				ExpressionRoughness->Desc = GeneratingParameterName;
				ExpressionRoughness->ParameterName = *GeneratingParameterName;

				ExpressionRoughness->Texture = TextureRoughness;
				ExpressionRoughness->SamplerType = SAMPLERTYPE_LinearGrayscale;

				// Offset node placement.
				ExpressionRoughness->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
				ExpressionRoughness->MaterialExpressionEditorY = MaterialNodeY;
				MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

				// Assign expression to material.
				_AddMaterialExpression(Material, ExpressionRoughness);
				MatRoughness.Expression = ExpressionRoughness;

				bExpressionCreated = true;

				// Propagate and trigger roughness texture updates.
				if (bCreatedNewTextureRoughness)
					FAssetRegistryModule::AssetCreated(TextureRoughness);

				TextureRoughness->PreEditChange(nullptr);
				TextureRoughness->PostEditChange();
				TextureRoughness->MarkPackageDirty();
			}

			// Cache the texture package
			OutPackages.AddUnique(TextureRoughnessPackage);
		}
	}

	// See if we have a roughness value
	HAPI_ParmInfo ParmRoughnessValueInfo;
	HAPI_ParmId ParmRoughnessValueId =
		FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_OGL, ParmRoughnessValueInfo);

	if (ParmRoughnessValueId >= 0)
	{
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_OGL);
	}
	else
	{
		ParmRoughnessValueId =
			FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_ROUGHNESS, ParmRoughnessValueInfo);

		if (ParmRoughnessValueId >= 0)
			GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_VALUE_ROUGHNESS);
	}

	if (!bExpressionCreated && ParmRoughnessValueId >= 0)
	{
		// Roughness value is available.

		float RoughnessValue = 0.0f;

		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, (float *)&RoughnessValue,
			ParmRoughnessValueInfo.floatValuesIndex, 1) == HAPI_RESULT_SUCCESS)
		{
			UMaterialExpressionScalarParameter * ExpressionRoughnessValue =
				Cast< UMaterialExpressionScalarParameter >(MatRoughness.Expression);

			// Clamp retrieved value.
			RoughnessValue = FMath::Clamp< float >(RoughnessValue, 0.0f, 1.0f);

			// Create color const expression and add it to material, if we don't have one.
			if (!ExpressionRoughnessValue)
			{
				// Otherwise new expression is of a different type.
				if (MatRoughness.Expression)
				{
					MatRoughness.Expression->ConditionalBeginDestroy();
					MatRoughness.Expression = nullptr;
				}

				ExpressionRoughnessValue = NewObject< UMaterialExpressionScalarParameter >(
					Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
			}

			// Record generating parameter.
			ExpressionRoughnessValue->Desc = GeneratingParameterName;
			ExpressionRoughnessValue->ParameterName = *GeneratingParameterName;

			ExpressionRoughnessValue->DefaultValue = RoughnessValue;
			ExpressionRoughnessValue->SliderMin = 0.0f;
			ExpressionRoughnessValue->SliderMax = 1.0f;

			// Offset node placement.
			ExpressionRoughnessValue->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
			ExpressionRoughnessValue->MaterialExpressionEditorY = MaterialNodeY;
			MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

			// Assign expression to material.
			_AddMaterialExpression(Material, ExpressionRoughnessValue);
			MatRoughness.Expression = ExpressionRoughnessValue;

			bExpressionCreated = true;
		}
	}

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
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

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

	// See if metallic texture is available.
	HAPI_ParmInfo ParmMetallicTextureInfo;
	HAPI_ParmId ParmMetallicTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_METALLIC_OGL,
		HAPI_UNREAL_PARAM_MAP_METALLIC_OGL_ENABLED,
		true,
		ParmMetallicTextureId,
		ParmMetallicTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_METALLIC_OGL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_METALLIC,
		HAPI_UNREAL_PARAM_MAP_METALLIC_ENABLED,
		false,
		ParmMetallicTextureId,
		ParmMetallicTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_MAP_METALLIC);
	}
	else
	{
		// failed to find the texture
		ParmMetallicTextureId = -1;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FScalarMaterialInput& MatMetallic = MaterialEditorOnly->Metallic;
#else
	FScalarMaterialInput& MatMetallic = Material->Metallic;	
#endif

	if (ParmMetallicTextureId >= 0)
	{
		TArray<char> ImageBuffer;

		// Retrieve color plane.
		if (FHoudiniMaterialTranslator::HapiExtractImage(
			ParmMetallicTextureId, InMaterialInfo, HAPI_UNREAL_MATERIAL_TEXTURE_COLOR,
			HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true, ImageBuffer))
		{
			UMaterialExpressionTextureSampleParameter2D * ExpressionMetallic =
				Cast< UMaterialExpressionTextureSampleParameter2D >(MatMetallic.Expression);

			UTexture2D * TextureMetallic = nullptr;
			if (ExpressionMetallic)
			{
				TextureMetallic = Cast<UTexture2D>(ExpressionMetallic->Texture);
			}
			else
			{
				// Otherwise new expression is of a different type.
				if (MatMetallic.Expression)
				{
					MatMetallic.Expression->ConditionalBeginDestroy();
					MatMetallic.Expression = nullptr;
				}
			}

			UPackage * TextureMetallicPackage = nullptr;
			if (TextureMetallic)
				TextureMetallicPackage = Cast< UPackage >(TextureMetallic->GetOuter());

			HAPI_ImageInfo ImageInfo;
			FHoudiniApi::ImageInfo_Init(&ImageInfo);
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureMetallicName;
				bool bCreatedNewTextureMetallic = false;

				// Create metallic texture package, if this is a new metallic texture.
				if (!TextureMetallicPackage)
				{
					TextureMetallicPackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
						InPackageParams,
						TextureMetallicName);
				}
				else if (IsValid(TextureMetallic))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureMetallicName = TextureMetallic->GetName();
				}
				else
				{
					TextureMetallicName = FPaths::GetBaseFilename(TextureMetallicPackage->GetName(), true);
				}

				// Create metallic texture, if we need to create one.
				if (!TextureMetallic)
					bCreatedNewTextureMetallic = true;

				// Get the node path to add it to the meta data
				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing metallic texture, or create new one.
				TextureMetallic = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureMetallic, 
					ImageInfo,
					TextureMetallicPackage,
					TextureMetallicName,
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_World,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
					NodePath);

				//if (BakeMode == EBakeMode::CookToTemp)
				TextureMetallic->SetFlags(RF_Public | RF_Standalone);

				// Create metallic sampling expression, if needed.
				if (!ExpressionMetallic)
					ExpressionMetallic = NewObject< UMaterialExpressionTextureSampleParameter2D >(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);

				// Record generating parameter.
				ExpressionMetallic->Desc = GeneratingParameterName;
				ExpressionMetallic->ParameterName = *GeneratingParameterName;

				ExpressionMetallic->Texture = TextureMetallic;
				ExpressionMetallic->SamplerType = SAMPLERTYPE_LinearGrayscale;

				// Offset node placement.
				ExpressionMetallic->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
				ExpressionMetallic->MaterialExpressionEditorY = MaterialNodeY;
				MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

				// Assign expression to material.
				_AddMaterialExpression(Material, ExpressionMetallic);
				MatMetallic.Expression = ExpressionMetallic;

				bExpressionCreated = true;

				// Propagate and trigger metallic texture updates.
				if (bCreatedNewTextureMetallic)
					FAssetRegistryModule::AssetCreated(TextureMetallic);

				TextureMetallic->PreEditChange(nullptr);
				TextureMetallic->PostEditChange();
				TextureMetallic->MarkPackageDirty();
			}

			// Cache the texture package
			OutPackages.AddUnique(TextureMetallicPackage);
		}
	}

	// Get the metallic value
	HAPI_ParmInfo ParmMetallicValueInfo;
	HAPI_ParmId ParmMetallicValueId =
		FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_METALLIC_OGL, ParmMetallicValueInfo);

	if (ParmMetallicValueId >= 0)
	{
		GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_VALUE_METALLIC_OGL);
	}
	else
	{
		ParmMetallicValueId =
			FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_METALLIC, ParmMetallicValueInfo);

		if (ParmMetallicValueId >= 0)
			GeneratingParameterName = TEXT(HAPI_UNREAL_PARAM_VALUE_METALLIC);
	}

	if (!bExpressionCreated && ParmMetallicValueId >= 0)
	{
		// Metallic value is available.
		float MetallicValue = 0.0f;

		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, (float *)&MetallicValue,
			ParmMetallicValueInfo.floatValuesIndex, 1) == HAPI_RESULT_SUCCESS)
		{
			UMaterialExpressionScalarParameter* ExpressionMetallicValue =
				Cast<UMaterialExpressionScalarParameter>(MatMetallic.Expression);

			// Clamp retrieved value.
			MetallicValue = FMath::Clamp<float>(MetallicValue, 0.0f, 1.0f);

			// Create color const expression and add it to material, if we don't have one.
			if (!ExpressionMetallicValue)
			{
				// Otherwise new expression is of a different type.
				if (MatMetallic.Expression)
				{
					MatMetallic.Expression->ConditionalBeginDestroy();
					MatMetallic.Expression = nullptr;
				}

				ExpressionMetallicValue = NewObject<UMaterialExpressionScalarParameter>(
					Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
			}

			// Record generating parameter.
			ExpressionMetallicValue->Desc = GeneratingParameterName;
			ExpressionMetallicValue->ParameterName = *GeneratingParameterName;

			ExpressionMetallicValue->DefaultValue = MetallicValue;
			ExpressionMetallicValue->SliderMin = 0.0f;
			ExpressionMetallicValue->SliderMax = 1.0f;

			// Offset node placement.
			ExpressionMetallicValue->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
			ExpressionMetallicValue->MaterialExpressionEditorY = MaterialNodeY;
			MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

			// Assign expression to material.
			_AddMaterialExpression(Material, ExpressionMetallicValue);
			MatMetallic.Expression = ExpressionMetallicValue;

			bExpressionCreated = true;
		}
	}

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

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	EObjectFlags ObjectFlag = (InPackageParams.PackageMode == EPackageMode::Bake) ? RF_Standalone : RF_NoFlags;

	// Names of generating Houdini parameters.
	FString GeneratingParameterNameEmissiveTexture = TEXT("");
	FString GeneratingParameterNameEmissiveColor = TEXT("");
	FString GeneratingParameterNameEmissiveIntensity = TEXT("");

	// Emissive texture creation parameters.
	FCreateTexture2DParameters CreateTexture2DParameters;
	CreateTexture2DParameters.SourceGuidHash = FGuid();
	CreateTexture2DParameters.bUseAlpha = false;
	CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
	CreateTexture2DParameters.bDeferCompression = true;
	CreateTexture2DParameters.bSRGB = false;

	// Attempt to look up previously created expressions.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
	FColorMaterialInput& MatEmissive = MaterialEditorOnly->EmissiveColor;
#else
	FColorMaterialInput& MatEmissive = Material->EmissiveColor;
#endif

	// Locate Texture sampling expression.
	UMaterialExpressionTextureSampleParameter2D* ExpressionTextureSample =
		Cast<UMaterialExpressionTextureSampleParameter2D>(FHoudiniMaterialTranslator::MaterialLocateExpression(
			MatEmissive.Expression, UMaterialExpressionTextureSampleParameter2D::StaticClass()));

	// If texture sampling expression exists, attempt to look up corresponding texture.
	UTexture2D* TextureEmissive = nullptr;
	if (IsValid(ExpressionTextureSample))
		TextureEmissive = Cast<UTexture2D>(ExpressionTextureSample->Texture);

	// Locate emissive color expression.
	UMaterialExpressionVectorParameter* ExpressionEmissiveColor =
		Cast<UMaterialExpressionVectorParameter>(FHoudiniMaterialTranslator::MaterialLocateExpression(
			MatEmissive.Expression, UMaterialExpressionVectorParameter::StaticClass()));

	// If emissive color expression does not exist, create it.
	if (!IsValid(ExpressionEmissiveColor))
	{
		ExpressionEmissiveColor = NewObject<UMaterialExpressionVectorParameter>(
			Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag);
		ExpressionEmissiveColor->DefaultValue = FLinearColor::White;
	}

	// Add expression.
	_AddMaterialExpression(Material, ExpressionEmissiveColor);

	// Locate emissive intensity expression.
	UMaterialExpressionScalarParameter* ExpressionEmissiveIntensity =
		Cast<UMaterialExpressionScalarParameter>(FHoudiniMaterialTranslator::MaterialLocateExpression(
			MatEmissive.Expression, UMaterialExpressionScalarParameter::StaticClass()));

	// If emissive intensity expression does not exist, create it.
	if (!IsValid(ExpressionEmissiveIntensity))
	{
		ExpressionEmissiveIntensity = NewObject<UMaterialExpressionScalarParameter>(
			Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag);
		ExpressionEmissiveIntensity->Desc = GeneratingParameterNameEmissiveIntensity;
	}

	// Add expression.
	_AddMaterialExpression(Material, ExpressionEmissiveIntensity);

	// See if emissive intensity is available.
	HAPI_ParmInfo ParmEmissiveIntensityInfo;
	HAPI_ParmId ParmEmissiveIntensityId =
		FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY_OGL, ParmEmissiveIntensityInfo);

	if (ParmEmissiveIntensityId >= 0)
	{
		GeneratingParameterNameEmissiveIntensity = TEXT(HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY_OGL);
	}
	else
	{
		ParmEmissiveIntensityId =
			FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY, ParmEmissiveIntensityInfo);

		if (ParmEmissiveIntensityId >= 0)
			GeneratingParameterNameEmissiveIntensity = TEXT(HAPI_UNREAL_PARAM_VALUE_EMISSIVE_INTENSITY);
	}

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

	// Material should have at least one multiply expression.
	UMaterialExpressionMultiply* MaterialExpressionMultiply = Cast<UMaterialExpressionMultiply>(MatEmissive.Expression);
	if (!IsValid(MaterialExpressionMultiply))
		MaterialExpressionMultiply = NewObject<UMaterialExpressionMultiply>(
			Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

	// Add expression.
	_AddMaterialExpression(Material, MaterialExpressionMultiply);

	// See if primary multiplication has secondary multiplication as A input.
	UMaterialExpressionMultiply* MaterialExpressionMultiplySecondary = nullptr;
	if (MaterialExpressionMultiply->A.Expression)
		MaterialExpressionMultiplySecondary =
		Cast<UMaterialExpressionMultiply>(MaterialExpressionMultiply->A.Expression);

	// See if an emissive texture is available.
	HAPI_ParmInfo ParmEmissiveTextureInfo;
	HAPI_ParmId ParmEmissiveTextureId = -1;
	if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_OGL,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_OGL_ENABLED,
		true,
		ParmEmissiveTextureId,
		ParmEmissiveTextureInfo))
	{
		// Found via OGL tag
		GeneratingParameterNameEmissiveTexture = TEXT(HAPI_UNREAL_PARAM_MAP_EMISSIVE_OGL);
	}
	else if (FHoudiniMaterialTranslator::FindTextureParamByNameOrTag(
		InMaterialInfo.nodeId,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE,
		HAPI_UNREAL_PARAM_MAP_EMISSIVE_ENABLED,
		false,
		ParmEmissiveTextureId,
		ParmEmissiveTextureInfo))
	{
		// Found via Parm name
		GeneratingParameterNameEmissiveTexture = TEXT(HAPI_UNREAL_PARAM_MAP_EMISSIVE);
	}
	else
	{
		// failed to find the texture
		ParmEmissiveTextureId = -1;
	}

	// If we have an emissive texture parameter.
	if (ParmEmissiveTextureId >= 0)
	{
		TArray<char> ImageBuffer;

		// Get image planes of the emissive map.
		TArray<FString> EmissiveImagePlanes;
		bool bFoundImagePlanes = FHoudiniMaterialTranslator::HapiGetImagePlanes(
			ParmEmissiveTextureId, InMaterialInfo, EmissiveImagePlanes);

		HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
		const char* PlaneType = "";

		if (bFoundImagePlanes && EmissiveImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_COLOR)))
		{
			if (EmissiveImagePlanes.Contains(TEXT(HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA)))
			{
				ImagePacking = HAPI_IMAGE_PACKING_RGBA;
				PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;

				// Material does use alpha.
				CreateTexture2DParameters.bUseAlpha = true;
			}
			else
			{
				// We still need to have the Alpha plane, just not the CreateTexture2DParameters
				// alpha option. This is because all texture data from Houdini Engine contains
				// the alpha plane by default.
				ImagePacking = HAPI_IMAGE_PACKING_RGBA;
				PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
			}
		}
		else
		{
			bFoundImagePlanes = false;
		}

		// Retrieve color plane.
		if (bFoundImagePlanes && FHoudiniMaterialTranslator::HapiExtractImage(
			ParmEmissiveTextureId, InMaterialInfo, PlaneType,
			HAPI_IMAGE_DATA_INT8, ImagePacking, false, ImageBuffer))
		{
			UPackage* TextureEmissivePackage = nullptr;
			if (IsValid(TextureEmissive))
				TextureEmissivePackage = Cast<UPackage>(TextureEmissive->GetOuter());

			HAPI_ImageInfo ImageInfo;
			FHoudiniApi::ImageInfo_Init(&ImageInfo);
			Result = FHoudiniApi::GetImageInfo(
				FHoudiniEngine::Get().GetSession(),
				InMaterialInfo.nodeId, &ImageInfo);

			if (Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureEmissiveName;
				bool bCreatedNewTextureEmissive = false;

				// Create emissive texture package, if this is a new emissive texture.
				if (!TextureEmissivePackage)
				{
					TextureEmissivePackage = FHoudiniMaterialTranslator::CreatePackageForTexture(
						InMaterialInfo.nodeId,
						HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
						InPackageParams,
						TextureEmissiveName);
				}
				else if (IsValid(TextureEmissive))
				{
					// Get the name of the texture if we are overwriting the exist asset
					TextureEmissiveName = TextureEmissive->GetName();
				}
				else
				{
					TextureEmissiveName = FPaths::GetBaseFilename(TextureEmissivePackage->GetName(), true);
				}

				// Create emissive texture, if we need to create one.
				if (!IsValid(TextureEmissive))
					bCreatedNewTextureEmissive = true;

				FString NodePath;
				FHoudiniMaterialTranslator::GetMaterialRelativePath(InAssetId, InMaterialInfo.nodeId, NodePath);

				// Reuse existing emissive texture, or create new one.
				TextureEmissive = FHoudiniMaterialTranslator::CreateUnrealTexture(
					TextureEmissive,
					ImageInfo,
					TextureEmissivePackage,
					TextureEmissiveName,
					ImageBuffer,
					CreateTexture2DParameters,
					TEXTUREGROUP_World,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
					NodePath);

				//if (BakeMode == EBakeMode::CookToTemp)
				TextureEmissive->SetFlags(RF_Public | RF_Standalone);

				// Create emissive sampling expression, if needed.
				if (!ExpressionTextureSample)
				{
					ExpressionTextureSample = NewObject<UMaterialExpressionTextureSampleParameter2D>(
						Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag);
				}

				// Record generating parameter.
				ExpressionTextureSample->Desc = GeneratingParameterNameEmissiveTexture;
				ExpressionTextureSample->ParameterName = *GeneratingParameterNameEmissiveTexture;
				ExpressionTextureSample->Texture = TextureEmissive;
				ExpressionTextureSample->SamplerType = SAMPLERTYPE_LinearGrayscale;

				// Add expression.
				_AddMaterialExpression(Material, ExpressionTextureSample);

				// Propagate and trigger emissive texture updates.
				if (bCreatedNewTextureEmissive)
					FAssetRegistryModule::AssetCreated(TextureEmissive);

				TextureEmissive->PreEditChange(nullptr);
				TextureEmissive->PostEditChange();
				TextureEmissive->MarkPackageDirty();
			}

			// Cache the texture package
			OutPackages.AddUnique(TextureEmissivePackage);
		}
	}

	// See if emissive color is available.
	HAPI_ParmInfo ParmEmissiveColorInfo;
	HAPI_ParmId ParmEmissiveColorId =
		FHoudiniEngineUtils::HapiFindParameterByTag(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_OGL, ParmEmissiveColorInfo);

	if (ParmEmissiveColorId >= 0)
	{
		GeneratingParameterNameEmissiveColor = TEXT(HAPI_UNREAL_PARAM_VALUE_EMISSIVE_OGL);
	}
	else
	{
		ParmEmissiveColorId =
			FHoudiniEngineUtils::HapiFindParameterByName(InMaterialInfo.nodeId, HAPI_UNREAL_PARAM_VALUE_EMISSIVE, ParmEmissiveColorInfo);

		if (ParmEmissiveColorId >= 0)
			GeneratingParameterNameEmissiveColor = TEXT(HAPI_UNREAL_PARAM_VALUE_EMISSIVE);
	}

	// If we have an emissive color parameter.
	if (ParmEmissiveColorId >= 0)
	{
		FLinearColor Color = FLinearColor::White;
		if (FHoudiniApi::GetParmFloatValues(
			FHoudiniEngine::Get().GetSession(), InMaterialInfo.nodeId, (float*)&Color.R,
			ParmEmissiveColorInfo.floatValuesIndex, ParmEmissiveColorInfo.size) == HAPI_RESULT_SUCCESS)
		{
			if (ParmEmissiveColorInfo.size == 3)
				Color.A = 1.0f;

			// Record generating parameter.
			ExpressionEmissiveColor->Desc = GeneratingParameterNameEmissiveColor;
			ExpressionEmissiveColor->ParameterName = *GeneratingParameterNameEmissiveColor;
			ExpressionEmissiveColor->DefaultValue = Color;
		}
	}

	// If we have have texture sample expression present, we need a secondary multiplication expression.
	if (ExpressionTextureSample)
	{
		if (!MaterialExpressionMultiplySecondary)
		{
			MaterialExpressionMultiplySecondary = NewObject<UMaterialExpressionMultiply>(
				Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag);

			// Add expression.
			_AddMaterialExpression(Material, MaterialExpressionMultiplySecondary);
		}
	}
	else
	{
		// If secondary multiplication exists, but we have no sampling, we can free it.
		if (MaterialExpressionMultiplySecondary)
		{
			MaterialExpressionMultiplySecondary->A.Expression = nullptr;
			MaterialExpressionMultiplySecondary->B.Expression = nullptr;
			MaterialExpressionMultiplySecondary->ConditionalBeginDestroy();
		}
	}

	float SecondaryExpressionScale = 1.0f;
	if (MaterialExpressionMultiplySecondary)
		SecondaryExpressionScale = 1.5f;

	// Create multiplication expression which has emissive color and emissive intesnity
	MaterialExpressionMultiply->A.Expression = ExpressionEmissiveColor;
	MaterialExpressionMultiply->B.Expression = ExpressionEmissiveIntensity;

	ExpressionEmissiveColor->MaterialExpressionEditorX =
		FHoudiniMaterialTranslator::MaterialExpressionNodeX -
		FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * SecondaryExpressionScale;
	ExpressionEmissiveColor->MaterialExpressionEditorY = MaterialNodeY;
	MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

	ExpressionEmissiveIntensity->MaterialExpressionEditorX =
		FHoudiniMaterialTranslator::MaterialExpressionNodeX -
		FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * SecondaryExpressionScale;
	ExpressionEmissiveIntensity->MaterialExpressionEditorY = MaterialNodeY;
	MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

	MaterialExpressionMultiply->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
	MaterialExpressionMultiply->MaterialExpressionEditorY =
		(ExpressionEmissiveIntensity->MaterialExpressionEditorY + ExpressionEmissiveColor->MaterialExpressionEditorY) / 2;

	// Hook up secondary multiplication expression to first one.
	if (MaterialExpressionMultiplySecondary)
	{
		MaterialExpressionMultiplySecondary->A.Expression = MaterialExpressionMultiply;
		MaterialExpressionMultiplySecondary->B.Expression = ExpressionTextureSample;

		if (ExpressionTextureSample)
		{
			ExpressionTextureSample->MaterialExpressionEditorX =
				FHoudiniMaterialTranslator::MaterialExpressionNodeX -
				FHoudiniMaterialTranslator::MaterialExpressionNodeStepX * SecondaryExpressionScale;
			ExpressionTextureSample->MaterialExpressionEditorY = MaterialNodeY;
		}

		MaterialNodeY += FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		MaterialExpressionMultiplySecondary->MaterialExpressionEditorX = FHoudiniMaterialTranslator::MaterialExpressionNodeX;
		MaterialExpressionMultiplySecondary->MaterialExpressionEditorY =
			MaterialExpressionMultiply->MaterialExpressionEditorY + FHoudiniMaterialTranslator::MaterialExpressionNodeStepY;

		// Assign expression.
		MatEmissive.Expression = MaterialExpressionMultiplySecondary;
	}
	else
	{
		// Assign expression.
		MatEmissive.Expression = MaterialExpressionMultiply;
	}

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
	UMetaData* MetaData = CurrentPackage->GetMetaData();
	if (!MetaData || !MetaData->HasValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
		continue;

	// Get the texture type from the meta data
	// Texture type store has meta data will be C_A, N, S, R etc..
	const FString TextureTypeString = MetaData->GetValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);
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
	const FString NodePath = MetaData->GetValue(PackageTexture, HAPI_UNREAL_PACKAGE_META_NODE_PATH);
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
		// We found a valid "use" parameter, check if it is disabled
		// Get the param value
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

