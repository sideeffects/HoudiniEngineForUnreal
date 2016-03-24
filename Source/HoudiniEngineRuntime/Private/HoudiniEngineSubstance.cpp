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


bool
FHoudiniEngineSubstance::GetSubstanceMaterialName(const HAPI_MaterialInfo& MaterialInfo,
	FString& SubstanceMaterialName)
{
	SubstanceMaterialName = TEXT("");

	HAPI_NodeInfo NodeInfo;
	FMemory::Memset<HAPI_NodeInfo>(NodeInfo, 0);
	if(HAPI_RESULT_SUCCESS !=
		FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, &NodeInfo))
	{
		return false;
	}

	if(NodeInfo.parmCount > 0)
	{
		TArray<HAPI_ParmInfo> ParmInfos;
		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeInfo.id,
			&ParmInfos[0], 0, NodeInfo.parmCount))
		{
			return false;
		}

		FString ParameterLabel = TEXT("");

		{
			FHoudiniEngineString HoudiniEngineString(ParmInfos[0].labelSH);
			if(!HoudiniEngineString.ToFString(ParameterLabel))
			{
				return false;
			}
		}

		// Check if we are dealing with Substance SHOP.
		if(!ParameterLabel.Equals(HAPI_UNREAL_PARAM_SUBSTANCE_LABEL))
		{
			return false;
		}

		// We need to locate Substance filename.
		TArray<FString> ParameterNames;
		FHoudiniEngineUtils::HapiRetrieveParameterNames(ParmInfos, ParameterNames);
		int32 SubstaceFileNameParmIdx =
			FHoudiniEngineUtils::HapiFindParameterByName(HAPI_UNREAL_PARAM_SUBSTANCE_FILENAME, ParameterNames);

		if(-1 == SubstaceFileNameParmIdx)
		{
			return false;
		}

		// Get corresponding parameter.
		const HAPI_ParmInfo& ParmSubstanceFileName = ParmInfos[SubstaceFileNameParmIdx];

		// We have located Substance filename parameter, we need to extract its value.
		HAPI_StringHandle SubstanceFilenameStringHandle;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(), NodeInfo.id,
			true, &SubstanceFilenameStringHandle, ParmSubstanceFileName.stringValuesIndex, 1))
		{
			return false;
		}

		// Get file parameter value.
		{
			FHoudiniEngineString HoudiniEngineString(SubstanceFilenameStringHandle);
			if(!HoudiniEngineString.ToFString(SubstanceMaterialName))
			{
				return false;
			}
		}

		// Any additional processing?

		return true;
	}

	return false;
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
