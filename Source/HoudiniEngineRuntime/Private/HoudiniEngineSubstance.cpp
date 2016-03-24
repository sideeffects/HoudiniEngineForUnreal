/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineSubstance.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniMaterialObject.h"
#include "HoudiniParameterObject.h"


bool
FHoudiniEngineSubstance::GetSubstanceMaterialName(const HAPI_MaterialInfo& MaterialInfo,
	FString& SubstanceMaterialName)
{
	SubstanceMaterialName = TEXT("");
	FHoudiniMaterialObject HoudiniMaterialObject(MaterialInfo);

	if(!HoudiniMaterialObject.IsSubstance())
	{
		return false;
	}

	FHoudiniParameterObject HoudiniParameterObject;
	if(!HoudiniMaterialObject.HapiLocateParameterByName(HAPI_UNREAL_PARAM_SUBSTANCE_FILENAME, HoudiniParameterObject))
	{
		return false;
	}

	if(!HoudiniParameterObject.HapiCheckParmCategoryPath())
	{
		return false;
	}

	FHoudiniEngineString HoudiniEngineString;
	if(!HoudiniParameterObject.HapiGetValue(HoudiniEngineString))
	{
		return false;
	}

	if(!HoudiniEngineString.ToFString(SubstanceMaterialName))
	{
		return false;
	}

	return true;
}


#if WITH_EDITOR

UObject*
FHoudiniEngineSubstance::LoadSubstanceInstanceFactory(UClass* InstanceFactoryClass,
	const FString& SubstanceMaterialName)
{
	UObject* SubstanceInstanceFactory = nullptr;
	TArray<FAssetData> SubstanceInstaceFactories;

	FAssetRegistryModule& AssetRegistryModule
		= FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssetsByClass(InstanceFactoryClass->GetFName(), SubstanceInstaceFactories);

	for(int32 FactoryIdx = 0, FactoryNum = SubstanceInstaceFactories.Num(); FactoryIdx < FactoryNum; ++FactoryIdx)
	{
		const FAssetData& AssetData = SubstanceInstaceFactories[FactoryIdx];
		const FString& AssetName = AssetData.AssetName.ToString();
		if(AssetName.Equals(SubstanceMaterialName))
		{
			const FString& AssetObjectPath = AssetData.ObjectPath.ToString();
			SubstanceInstanceFactory = StaticLoadObject(InstanceFactoryClass, nullptr, *AssetObjectPath, nullptr,
				LOAD_None, nullptr);
			if(SubstanceInstanceFactory)
			{
				break;
			}
		}
	}

	return SubstanceInstanceFactory;
}


UObject*
FHoudiniEngineSubstance::LoadSubstanceGraphInstance(UClass* GraphInstanceClass, UObject* InstanceFactory)
{
	UObject* MatchedSubstanceGraphInstance = nullptr;
	TArray<FAssetData> SubstanceGraphInstances;

	// Get Substance instance factory pointer property.
	UObjectProperty* FactoryParentProperty =
		Cast<UObjectProperty>(GraphInstanceClass->FindPropertyByName(HAPI_UNREAL_SUBSTANCE_PROPERTY_FACTORY_PARENT));
	if(!FactoryParentProperty)
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule
		= FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssetsByClass(GraphInstanceClass->GetFName(), SubstanceGraphInstances);

	for(int32 GraphInstanceIdx = 0, GraphInstanceNum = SubstanceGraphInstances.Num();
		GraphInstanceIdx < GraphInstanceNum; ++GraphInstanceIdx)
	{
		const FAssetData& AssetData = SubstanceGraphInstances[GraphInstanceIdx];
		const FString& AssetObjectPath = AssetData.ObjectPath.ToString();
		UObject* GraphInstanceObject = StaticLoadObject(GraphInstanceClass, nullptr, *AssetObjectPath, nullptr,
			LOAD_None, nullptr);
		if(GraphInstanceObject)
		{
			void* FactoryParentPropertyValue = FactoryParentProperty->ContainerPtrToValuePtr<void>(GraphInstanceObject);
			UObject* InstanceFactoryObject = FactoryParentProperty->GetObjectPropertyValue(FactoryParentPropertyValue);
			if(InstanceFactory == InstanceFactoryObject)
			{
				MatchedSubstanceGraphInstance = GraphInstanceObject;
				break;
			}
		}
	}

	return MatchedSubstanceGraphInstance;
}

#endif


bool
FHoudiniEngineSubstance::RetrieveSubstanceRTTIClasses(UClass*& InstanceFactoryClass, UClass*& GraphInstanceClass,
	UClass*& UtilityClass)
{
	InstanceFactoryClass = nullptr;
	GraphInstanceClass = nullptr;
	UtilityClass = nullptr;

	UClass* SubstanceInstaceFactoryClass = FindObject<UClass>(ANY_PACKAGE,
		HAPI_UNREAL_SUBSTANCE_CLASS_INSTANCE_FACTORY);
	if(!SubstanceInstaceFactoryClass)
	{
		return false;
	}

	UClass* SubstanceGraphInstanceClass =
		FindObject<UClass>(ANY_PACKAGE, HAPI_UNREAL_SUBSTANCE_CLASS_GRAPH_INSTANCE);
	if(!SubstanceGraphInstanceClass)
	{
		return false;
	}

	UClass* SubstanceUtilityClass = FindObject<UClass>(ANY_PACKAGE, HAPI_UNREAL_SUBSTANCE_CLASS_UTILITY);
	if(!SubstanceUtilityClass)
	{
		return false;
	}

	InstanceFactoryClass = SubstanceInstaceFactoryClass;
	GraphInstanceClass = SubstanceGraphInstanceClass;
	UtilityClass = SubstanceUtilityClass;

	return true;
}
