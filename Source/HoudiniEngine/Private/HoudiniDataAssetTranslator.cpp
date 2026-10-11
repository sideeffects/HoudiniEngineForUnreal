/*
* Copyright (c) <2026> Side Effects Software Inc.
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
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
* EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniDataAssetTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineString.h"
#include "HoudiniPackageParams.h"
#include "UnrealDataAssetTranslator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataAsset.h"
#include "JsonObjectConverter.h"
#include "Math/Color.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

UClass* FHoudiniDataAssetTranslator::LoadDataAssetClass(const FString& ClassPath)
{
	// The Unreal Editor omits the generated Blueprint _C suffix from object paths, which can be
	// confusing when users provide a class path from the editor. Add it when that path does not load.
	UClass* DataAssetClass = LoadObject<UClass>(nullptr, *ClassPath);
	if (!IsValid(DataAssetClass) && !ClassPath.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
	{
		const FString GeneratedClassPath = ClassPath + TEXT("_C");
		DataAssetClass = LoadObject<UClass>(nullptr, *GeneratedClassPath);
	}

	return DataAssetClass;
}

void FHoudiniDataAssetTranslator::NormalizeDataAssetPropertyNamesInJsonObject(
	UClass* DataAssetClass,
	const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!IsValid(DataAssetClass) || !JsonObject.IsValid())
		return;

	for (TFieldIterator<FProperty> PropertyIt(DataAssetClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			continue;

		const FString PropertyName = Property->GetAuthoredName();
		FString PrefixedPropertyName = DataAssetClass->GetName() + TEXT("_") + PropertyName;
		FHoudiniEngineUtils::SanitizeHAPIVariableName(PrefixedPropertyName);
		if (JsonObject->HasField(PropertyName) || !JsonObject->HasField(PrefixedPropertyName))
			continue;

		JsonObject->SetField(PropertyName, JsonObject->TryGetField(PrefixedPropertyName));
		JsonObject->RemoveField(PrefixedPropertyName);
	}
}

bool FHoudiniDataAssetTranslator::IsObjectReferenceProperty(FProperty* Property)
{
	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		return SoftObjectProperty->PropertyClass && SoftObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass());

	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		return ObjectProperty->PropertyClass && ObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass());

	return false;
}

bool FHoudiniDataAssetTranslator::ResolveDataAssetReferencesInJsonObject(
	UClass* DataAssetClass,
	const TSharedPtr<FJsonObject>& JsonObject,
	UDataAsset* RootDataAsset,
	const FHoudiniPackageParams& PackageParams,
	TMap<FString, FGeneratedDataAssetInfo>& GeneratedDataAssets)
{
	if (!DataAssetClass || !JsonObject.IsValid() || !RootDataAsset)
		return false;

	bool bSuccess = true;

	for (TFieldIterator<FProperty> PropertyIt(DataAssetClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			continue;

		const FString PropertyName = Property->GetAuthoredName();
		TSharedPtr<FJsonValue> JsonValue = JsonObject->TryGetField(PropertyName);
		if (!JsonValue.IsValid())
			continue;

		bSuccess = ResolveDataAssetReferenceJsonValue(Property, JsonValue, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;
		JsonObject->SetField(PropertyName, JsonValue);
	}

	return bSuccess;
}

bool FHoudiniDataAssetTranslator::ResolveDataAssetReferenceJsonValue(
	FProperty* Property,
	TSharedPtr<FJsonValue>& JsonValue,
	UDataAsset* RootDataAsset,
	const FHoudiniPackageParams& PackageParams,
	TMap<FString, FGeneratedDataAssetInfo>& GeneratedDataAssets)
{
	if (!Property || !JsonValue.IsValid())
		return false;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues = JsonValue->AsArray();
		bool bSuccess = true;
		for (TSharedPtr<FJsonValue>& ElementValue : JsonValues)
			bSuccess = ResolveDataAssetReferenceJsonValue(ArrayProperty->Inner, ElementValue, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;
		JsonValue = MakeShared<FJsonValueArray>(MoveTemp(JsonValues));
		return bSuccess;
	}

	if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues = JsonValue->AsArray();
		bool bSuccess = true;
		for (TSharedPtr<FJsonValue>& ElementValue : JsonValues)
			bSuccess = ResolveDataAssetReferenceJsonValue(SetProperty->ElementProp, ElementValue, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;
		JsonValue = MakeShared<FJsonValueArray>(MoveTemp(JsonValues));
		return bSuccess;
	}

	if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		if (JsonValue->Type == EJson::Array)
		{
			// Dictionary-mode maps use explicit { "__key": ..., "__value": ... } entries because JSON
			// object member names can only be strings, whereas reflected TMap keys can use any supported
			// property type. This also allows nested Data Assets on either side of the map. Convert that
			// shape back to the native JSON-object representation expected by JsonObjectConverter.
			TSharedRef<FJsonObject> MapJsonObject = MakeShared<FJsonObject>();
			bool bSuccess = true;
			for (const TSharedPtr<FJsonValue>& EntryValue : JsonValue->AsArray())
			{
				TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
				if (!EntryObject.IsValid())
					continue;

				TSharedPtr<FJsonValue> KeyJsonValue = EntryObject->TryGetField(FUnrealDataAssetTranslator::MapKeyDictionaryKey);
				TSharedPtr<FJsonValue> ValueJsonValue = EntryObject->TryGetField(FUnrealDataAssetTranslator::MapValueDictionaryKey);
				if (!KeyJsonValue.IsValid() || !ValueJsonValue.IsValid())
					continue;

				bSuccess = ResolveDataAssetReferenceJsonValue(MapProperty->KeyProp, KeyJsonValue, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;
				bSuccess = ResolveDataAssetReferenceJsonValue(MapProperty->ValueProp, ValueJsonValue, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;

				FString MapKey;
				if (!KeyJsonValue->TryGetString(MapKey))
					continue;

				MapJsonObject->SetField(MapKey, ValueJsonValue);
			}

			JsonValue = MakeShared<FJsonValueObject>(MapJsonObject);
			return bSuccess;
		}

		TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
		if (!JsonObject.IsValid())
			return false;

		// A map's keys must remain JSON object keys, but its values can contain full nested Data Asset
		// dictionaries and are converted back to their paths before reflection imports the map.
		bool bSuccess = true;
		for (auto& Pair : JsonObject->Values)
			bSuccess = ResolveDataAssetReferenceJsonValue(MapProperty->ValueProp, Pair.Value, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;
		return bSuccess;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
		if (!JsonObject.IsValid())
			return false;

		bool bSuccess = true;

		for (TFieldIterator<FProperty> MemberIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); MemberIt; ++MemberIt)
		{
			FProperty* MemberProperty = *MemberIt;
			if (!MemberProperty->HasAnyPropertyFlags(CPF_Edit) || MemberProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
				continue;

			const FString MemberName = MemberProperty->GetAuthoredName();
			TSharedPtr<FJsonValue> MemberJsonValue = JsonObject->TryGetField(MemberName);
			if (MemberJsonValue.IsValid())
			{
				bSuccess = ResolveDataAssetReferenceJsonValue(MemberProperty, MemberJsonValue, RootDataAsset, PackageParams, GeneratedDataAssets) && bSuccess;
				JsonObject->SetField(MemberName, MemberJsonValue);
			}
		}
		return bSuccess;
	}

	if (JsonValue->Type != EJson::Object)
		return true;

	// An object path must resolve to an existing Data Asset or one generated earlier in this output.
	// A nested Data Asset without an object path is a new, distinct output.
	// The reflected JSON importer needs only a final object path, including when the Data Asset is held in
	// a base object-reference property instead of a property declared directly as UDataAsset.
	const TSharedPtr<FJsonObject> ReferenceObject = JsonValue->AsObject();
	if (!ReferenceObject.IsValid())
		return false;

	// A base UObject property can hold a Data Asset at runtime, so use the reserved Data Asset keys
	// to recognize that dictionary. Leave ordinary UObject dictionaries untouched.
	const bool bHasDataAssetReferenceKey = ReferenceObject->HasField(FUnrealDataAssetTranslator::ObjectPathDictionaryKey)
		|| ReferenceObject->HasField(FUnrealDataAssetTranslator::ClassPathDictionaryKey)
		|| ReferenceObject->HasField(FUnrealDataAssetTranslator::UniqueIdDictionaryKey);
	if (!bHasDataAssetReferenceKey && !IsObjectReferenceProperty(Property))
		return true;

	FString ReferencePath;
	const bool bHasReferencePath = ReferenceObject->TryGetStringField(FUnrealDataAssetTranslator::ObjectPathDictionaryKey, ReferencePath);
	if (bHasReferencePath && (ReferencePath.IsEmpty() || ReferencePath == TEXT("None")))
	{
		JsonValue = MakeShared<FJsonValueString>(ReferencePath);
		return true;
	}

	if (bHasReferencePath)
	{
		if (FGeneratedDataAssetInfo* GeneratedDataAssetInfo = GeneratedDataAssets.Find(ReferencePath))
		{
			JsonValue = MakeShared<FJsonValueString>(GeneratedDataAssetInfo->DataAsset->GetPathName());
			return true;
		}

		if (UDataAsset* ExistingDataAsset = LoadObject<UDataAsset>(nullptr, *ReferencePath))
		{
			JsonValue = MakeShared<FJsonValueString>(ExistingDataAsset->GetPathName());
			return true;
		}

		HOUDINI_LOG_ERROR(TEXT("Could not find nested Data Asset '%s'."), *ReferencePath);
		return false;
	}

	UDataAsset* ReferencedDataAsset = CreateNestedDataAsset(ReferenceObject, RootDataAsset, PackageParams, GeneratedDataAssets);
	if (!IsValid(ReferencedDataAsset))
		return false;

	JsonValue = MakeShared<FJsonValueString>(ReferencedDataAsset->GetPathName());
	return true;
}

UDataAsset* FHoudiniDataAssetTranslator::CreateNestedDataAsset(
	const TSharedPtr<FJsonObject>& JsonObject,
	UDataAsset* RootDataAsset,
	const FHoudiniPackageParams& PackageParams,
	TMap<FString, FGeneratedDataAssetInfo>& GeneratedDataAssets)
{
	if (!JsonObject.IsValid() || !IsValid(RootDataAsset))
		return nullptr;

	FString UniqueId;
	if (JsonObject->HasField(FUnrealDataAssetTranslator::UniqueIdDictionaryKey))
	{
		if (!JsonObject->TryGetStringField(FUnrealDataAssetTranslator::UniqueIdDictionaryKey, UniqueId) || UniqueId.IsEmpty())
		{
			HOUDINI_LOG_ERROR(TEXT("Nested Data Asset unique ID must be a non-empty string."));
			return nullptr;
		}

		for (const TPair<FString, FGeneratedDataAssetInfo>& Pair : GeneratedDataAssets)
		{
			if (Pair.Value.UniqueId == UniqueId)
				return Pair.Value.DataAsset;
		}
	}

	FString ClassPath;
	if (!JsonObject->TryGetStringField(FUnrealDataAssetTranslator::ClassPathDictionaryKey, ClassPath))
	{
		HOUDINI_LOG_ERROR(TEXT("Nested Data Asset dictionary without an object path is missing its class path."));
		return nullptr;
	}

	FString GeneratedDataAssetIdentifier;
	int32 GeneratedDataAssetIndex = GeneratedDataAssets.Num();
	do
	{
		GeneratedDataAssetIdentifier = FString::Printf(TEXT("__generated_nested_data_asset_%d"), GeneratedDataAssetIndex++);
	}
	while (GeneratedDataAssets.Contains(GeneratedDataAssetIdentifier));

	UClass* DataAssetClass = LoadDataAssetClass(ClassPath);
	if (!IsValid(DataAssetClass) || !DataAssetClass->IsChildOf(UDataAsset::StaticClass()) || DataAssetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid nested Data Asset class '%s'."), *ClassPath);
		return nullptr;
	}

	FHoudiniPackageParams NestedPackageParams = PackageParams;
	NestedPackageParams.SplitStr = FString::Printf(TEXT("dataasset_nested_%d"), GeneratedDataAssets.Num());
	UDataAsset* DataAsset = Cast<UDataAsset>(NestedPackageParams.CreateObjectAndPackageFromClass(DataAssetClass));
	if (!IsValid(DataAsset))
		return nullptr;

	FGeneratedDataAssetInfo& GeneratedDataAssetInfo = GeneratedDataAssets.Add(GeneratedDataAssetIdentifier);
	GeneratedDataAssetInfo.DataAsset = DataAsset;
	GeneratedDataAssetInfo.UniqueId = UniqueId;
	JsonObject->TryGetStringField(FUnrealDataAssetTranslator::BakeFolderDictionaryKey, GeneratedDataAssetInfo.BakeFolder);
	JsonObject->TryGetStringField(FUnrealDataAssetTranslator::BakeOutputNameDictionaryKey, GeneratedDataAssetInfo.BakeOutputName);
	NormalizeDataAssetPropertyNamesInJsonObject(DataAssetClass, JsonObject);
	if (!ResolveDataAssetReferencesInJsonObject(DataAssetClass, JsonObject, RootDataAsset, NestedPackageParams, GeneratedDataAssets))
		return nullptr;

	// Apply the resolved JSON dictionary to this nested Data Asset instance. DataAssetClass
	// supplies the reflection metadata; DataAsset is the object whose properties are written.
	if (!FJsonObjectConverter::JsonObjectToUStruct(
		JsonObject.ToSharedRef(), DataAssetClass, DataAsset, CPF_Edit, CPF_Transient | CPF_Deprecated, false, nullptr, nullptr))
	{
		HOUDINI_LOG_ERROR(TEXT("Could not apply nested Data Asset values for '%s'."), *GeneratedDataAssetIdentifier);
		return nullptr;
	}

	DataAsset->PostEditChange();
	DataAsset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(DataAsset);
	return DataAsset;
}

bool
FHoudiniDataAssetTranslator::BuildDataAssetsFromPointAttributes(
	const FHoudiniGeoPartObject& HGPO,
	UHoudiniOutput* CurOutput,
	const FHoudiniPackageParams& PackageParams)
{
	const int32 PointCount = HGPO.PartInfo.PointCount;
	if (PointCount <= 0)
	{
		HOUDINI_LOG_WARNING(TEXT("Cannot create Data Asset output %s without points."), *HGPO.PartName);
		return false;
	}

	TArray<FString> ClassPaths;
	FHoudiniHapiAccessor ClassAccessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_DATA_ASSET_CLASS);
	if (!ClassAccessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, ClassPaths, 0, PointCount)
		|| ClassPaths.Num() != PointCount)
	{
		HOUDINI_LOG_WARNING(TEXT("Could not find a Data Asset class for every point on output %s."), *HGPO.PartName);
		return false;
	}

	TArray<UClass*> DataAssetClasses;
	TArray<UDataAsset*> DataAssets;
	DataAssetClasses.SetNumZeroed(PointCount);
	DataAssets.SetNumZeroed(PointCount);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		if (ClassPaths[PointIndex].IsEmpty())
			continue;

		UClass* DataAssetClass = LoadDataAssetClass(ClassPaths[PointIndex]);
		if (!IsValid(DataAssetClass) || !DataAssetClass->IsChildOf(UDataAsset::StaticClass()) || DataAssetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			HOUDINI_LOG_WARNING(TEXT("Invalid Data Asset class '%s' on output point %d of %s."),
				*ClassPaths[PointIndex], PointIndex, *HGPO.PartName);
			return false;
		}

		FHoudiniPackageParams DataAssetPackageParams = PackageParams;
		DataAssetPackageParams.ObjectId = HGPO.ObjectId;
		DataAssetPackageParams.GeoId = HGPO.GeoId;
		DataAssetPackageParams.PartId = HGPO.PartId;
		DataAssetPackageParams.SplitStr = FString::Printf(TEXT("dataasset_%d"), PointIndex);

		UDataAsset* DataAsset = Cast<UDataAsset>(DataAssetPackageParams.CreateObjectAndPackageFromClass(DataAssetClass));
		if (!IsValid(DataAsset))
		{
			HOUDINI_LOG_WARNING(TEXT("Could not create Data Asset output point %d for %s."), PointIndex, *HGPO.PartName);
			return false;
		}

		DataAssetClasses[PointIndex] = DataAssetClass;
		DataAssets[PointIndex] = DataAsset;
	}

	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		UClass* DataAssetClass = DataAssetClasses[PointIndex];
		UDataAsset* DataAsset = DataAssets[PointIndex];
		if (!IsValid(DataAssetClass) || !IsValid(DataAsset))
			continue;

		for (TFieldIterator<FProperty> PropertyIt(DataAssetClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
				continue;

			FString AttributeName = DataAssetClass->GetName() + TEXT("_") + Property->GetAuthoredName();
			FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);
			if (!ApplyPointPropertyAttribute(
				HGPO, Property, Property->ContainerPtrToValuePtr<void>(DataAsset), AttributeName, PointIndex, &DataAssets))
			{
				AttributeName = Property->GetAuthoredName();
				FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);
				ApplyPointPropertyAttribute(
					HGPO, Property, Property->ContainerPtrToValuePtr<void>(DataAsset), AttributeName, PointIndex, &DataAssets);
			}
		}

		DataAsset->PostEditChange();
		DataAsset->MarkPackageDirty();
		DataAsset->GetPackage()->FullyLoad();
		FAssetRegistryModule::AssetCreated(DataAsset);
	}

	FString BakeFolder;
	FHoudiniEngineUtils::GetBakeFolderAttribute(HGPO.GeoId, HGPO.PartId, BakeFolder);
	FString OutputName;
	FHoudiniEngineUtils::GetOutputNameAttribute(HGPO.GeoId, HGPO.PartId, OutputName, 0, 0);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		if (!IsValid(DataAssets[PointIndex]))
			continue;

		const FString PointIdentifier = HGPO.PartName + TEXT("_") + LexToString(PointIndex);
		const FHoudiniOutputObjectIdentifier OutputIdentifier(HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, PointIdentifier);
		FHoudiniOutputObject& OutputObject = CurOutput->GetOutputObjects().FindOrAdd(OutputIdentifier);
		OutputObject.OutputComponents.Empty();
		OutputObject.OutputObject = DataAssets[PointIndex];
		OutputObject.CachedAttributes.Empty();
		if (!BakeFolder.IsEmpty())
			OutputObject.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolder);
		if (!OutputName.IsEmpty())
		{
			const FString PointOutputName = PointCount == 1 ? OutputName : OutputName + TEXT("_") + LexToString(PointIndex);
			OutputObject.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, PointOutputName);
		}
	}

	return true;
}

bool
FHoudiniDataAssetTranslator::BuildDataAsset(
	const FHoudiniGeoPartObject& HGPO,
	UHoudiniOutput* CurOutput,
	const FHoudiniPackageParams& PackageParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniDataAssetTranslator::BuildDataAsset);

	// Dictionary values retain the full reflected property structure, including nested types and arrays.
	// Multi-point outputs use the individual attributes emitted by the non-dictionary Data Asset input:
	// one Data Asset per point and integer point indices for Data Asset references.
	HAPI_AttributeInfo ValuesInfo;
	FHoudiniHapiAccessor ValuesAccessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_DATA_ASSET_VALUES);
	bool bUseDictionaryValues = ValuesAccessor.GetInfo(ValuesInfo, HAPI_ATTROWNER_POINT)
		&& ValuesInfo.storage == HAPI_STORAGETYPE_DICTIONARY && ValuesInfo.count == 1 && HGPO.PartInfo.PointCount == 1;
	TSharedPtr<FJsonObject> JsonObject;
	if (bUseDictionaryValues)
	{
		HAPI_StringHandle DictionaryHandle = -1;
		if (FHoudiniApi::GetAttributeDictionaryData(
			FHoudiniEngine::Get().GetSession(),
			HGPO.GeoId,
			HGPO.PartId,
			HAPI_UNREAL_ATTRIB_DATA_ASSET_VALUES,
			&ValuesInfo,
			&DictionaryHandle,
			0,
			1) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("Could not read Data Asset values on output %s."), *HGPO.PartName);
			return false;
		}

		FString Dictionary;
		FHoudiniEngineString(DictionaryHandle).ToFString(Dictionary);
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Dictionary);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			HOUDINI_LOG_WARNING(TEXT("Could not parse Data Asset values on output %s."), *HGPO.PartName);
			return false;
		}
	}
	else
	{
		// Point-wise outputs validate and create every Data Asset class from the corresponding point.
		// Do this before querying a root class so a valid multi-point output is not incorrectly treated
		// as a single-root Data Asset.
		if (!bUseDictionaryValues)
			return BuildDataAssetsFromPointAttributes(HGPO, CurOutput, PackageParams);
	}


	// A dictionary-provided class describes the root output itself. Without one, use the attribute.

	FString DataAssetClassPath;
	if (!JsonObject->TryGetStringField(FUnrealDataAssetTranslator::ClassPathDictionaryKey, DataAssetClassPath))
	{
		TArray<FString> ClassPaths;
		FHoudiniHapiAccessor ClassAccessor(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_DATA_ASSET_CLASS);
		if (!ClassAccessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, ClassPaths, 0, 1) || ClassPaths.IsEmpty())
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find a Data Asset class on output %s."), *HGPO.PartName);
			return false;
		}

		DataAssetClassPath = ClassPaths[0];
	}

	UClass* DataAssetClass = LoadDataAssetClass(DataAssetClassPath);
	if (!IsValid(DataAssetClass) || !DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Data Asset class '%s' on output %s."), *DataAssetClassPath, *HGPO.PartName);
		return false;
	}

	if (DataAssetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot create Data Asset output %s from abstract class '%s'."), *HGPO.PartName, *DataAssetClass->GetPathName());
		return false;
	}

	FHoudiniPackageParams DataAssetPackageParams = PackageParams;
	DataAssetPackageParams.ObjectId = HGPO.ObjectId;
	DataAssetPackageParams.GeoId = HGPO.GeoId;
	DataAssetPackageParams.PartId = HGPO.PartId;
	DataAssetPackageParams.SplitStr = TEXT("dataasset");

	UDataAsset* DataAsset = Cast<UDataAsset>(DataAssetPackageParams.CreateObjectAndPackageFromClass(DataAssetClass));
	if (!IsValid(DataAsset))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not create Data Asset output %s."), *HGPO.PartName);
		return false;
	}

	TMap<FString, FGeneratedDataAssetInfo> GeneratedDataAssets;
	if (bUseDictionaryValues)
	{
		// The top-level path identifies the source root exported by Unreal. The output always creates
		// a new root, but retaining this mapping lets nested circular references resolve to that root.
		FString RootSourcePath;
		const FString& RootDataAssetIdentifier = JsonObject->TryGetStringField(
			FUnrealDataAssetTranslator::ObjectPathDictionaryKey, RootSourcePath) && !RootSourcePath.IsEmpty()
			? RootSourcePath : DataAsset->GetPathName();
		FGeneratedDataAssetInfo& RootDataAssetInfo = GeneratedDataAssets.Add(RootDataAssetIdentifier);
		RootDataAssetInfo.DataAsset = DataAsset;
		JsonObject->TryGetStringField(FUnrealDataAssetTranslator::BakeFolderDictionaryKey, RootDataAssetInfo.BakeFolder);
		JsonObject->TryGetStringField(FUnrealDataAssetTranslator::BakeOutputNameDictionaryKey, RootDataAssetInfo.BakeOutputName);
		NormalizeDataAssetPropertyNamesInJsonObject(DataAssetClass, JsonObject);

		if (!ResolveDataAssetReferencesInJsonObject(DataAssetClass, JsonObject, DataAsset, DataAssetPackageParams, GeneratedDataAssets))
		{
			HOUDINI_LOG_ERROR(TEXT("Could not generate nested Data Assets on output %s."), *HGPO.PartName);
			return false;
		}

		// Apply the resolved JSON dictionary to the root Data Asset instance. DataAssetClass
		// supplies the reflection metadata; DataAsset is the object whose properties are written.
		if (!FJsonObjectConverter::JsonObjectToUStruct(
			JsonObject.ToSharedRef(),
			DataAssetClass,
			DataAsset,
			CPF_Edit,
			CPF_Transient | CPF_Deprecated,
			false,
			nullptr,
			nullptr))
		{
			HOUDINI_LOG_WARNING(TEXT("Could not apply Data Asset values on output %s."), *HGPO.PartName);
			return false;
		}
	}
	else
	{
		FGeneratedDataAssetInfo& RootDataAssetInfo = GeneratedDataAssets.Add(DataAsset->GetPathName());
		RootDataAssetInfo.DataAsset = DataAsset;

		for (TFieldIterator<FProperty> PropertyIt(DataAssetClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
				continue;

			// An absent or incompatible attribute is not an error: leave the class default on the new object.
			// Prefer the class-prefixed form emitted when that input option is enabled, then retain
			// compatibility with the established authored-property-name form.
			FString AttributeName = DataAssetClass->GetName() + TEXT("_") + Property->GetAuthoredName();
			FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);
			if (!ApplyPointPropertyAttribute(HGPO, Property, Property->ContainerPtrToValuePtr<void>(DataAsset), AttributeName))
			{
				AttributeName = Property->GetAuthoredName();
				FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);
				ApplyPointPropertyAttribute(HGPO, Property, Property->ContainerPtrToValuePtr<void>(DataAsset), AttributeName);
			}
		}
	}

	DataAsset->PostEditChange();
	DataAsset->MarkPackageDirty();
	DataAsset->GetPackage()->FullyLoad();
	FAssetRegistryModule::AssetCreated(DataAsset);

	FString BakeFolder;
	FHoudiniEngineUtils::GetBakeFolderAttribute(HGPO.GeoId, HGPO.PartId, BakeFolder);

	FString OutputName;
	FHoudiniEngineUtils::GetOutputNameAttribute(HGPO.GeoId, HGPO.PartId, OutputName, 0, 0);

	// Each generated Data Asset needs an output object of its own. Besides exposing the child in
	// the output details panel, that gives baking and later cleanup a distinct, persistent record.
	for (const TPair<FString, FGeneratedDataAssetInfo>& Pair : GeneratedDataAssets)
	{
		const FGeneratedDataAssetInfo& GeneratedDataAssetInfo = Pair.Value;
		UDataAsset* GeneratedDataAsset = GeneratedDataAssetInfo.DataAsset;
		if (!IsValid(GeneratedDataAsset))
			continue;

		const bool bIsRootDataAsset = GeneratedDataAsset == DataAsset;
		const FString& SplitIdentifier = bIsRootDataAsset ? HGPO.PartName : Pair.Key;
		const FHoudiniOutputObjectIdentifier GeneratedOutputIdentifier(
			HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, SplitIdentifier);
		FHoudiniOutputObject& OutputObject = CurOutput->GetOutputObjects().FindOrAdd(GeneratedOutputIdentifier);
		OutputObject.OutputComponents.Empty();
		OutputObject.OutputObject = GeneratedDataAsset;
		OutputObject.CachedAttributes.Empty();
		FString GeneratedBakeFolder = BakeFolder;
		if (!GeneratedDataAssetInfo.BakeFolder.IsEmpty() && (!bIsRootDataAsset || BakeFolder.IsEmpty()))
			GeneratedBakeFolder = GeneratedDataAssetInfo.BakeFolder;
		if (!GeneratedBakeFolder.IsEmpty())
			OutputObject.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, GeneratedBakeFolder);

		// The output-name attribute belongs to the root Houdini output. Child assets use their own
		// generated names so that each receives a unique baked package.
		const FString& GeneratedOutputName = bIsRootDataAsset && !OutputName.IsEmpty()
			? OutputName : GeneratedDataAssetInfo.BakeOutputName;
		if (!GeneratedOutputName.IsEmpty())
			OutputObject.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, GeneratedOutputName);
	}

	return true;
}

bool
FHoudiniDataAssetTranslator::ApplyPointPropertyAttribute(
	const FHoudiniGeoPartObject& HGPO,
	FProperty* Property,
	void* ValuePtr,
	const FString& AttributeName,
	int32 PointIndex,
	const TArray<UDataAsset*>* PointDataAssets)
{
	// Rebuild one property from this Data Asset's single Houdini point. Missing or incompatible
	// attributes return false without assigning a value, allowing the newly created Data Asset to keep
	// its class defaults. Arrays and ordinary structs recurse through the indexed/_-delimited layout.
	if (!ValuePtr || !Property || Property->ArrayDim != 1)
		return false;

	auto GetContainerSize = [&HGPO, PointIndex](const FString& AttributeName, int32& OutSize)
	{
		const FString CountAttributeName = AttributeName + TEXT("_num");
		FHoudiniHapiAccessor CountAccessor(HGPO.GeoId, HGPO.PartId, *CountAttributeName);
		HAPI_AttributeInfo CountAttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&CountAttributeInfo);
		if (!CountAccessor.GetInfo(CountAttributeInfo, HAPI_ATTROWNER_POINT) || CountAttributeInfo.count <= PointIndex)
			return false;

		TArray<int64> CountValues;
		if (!CountAccessor.GetAttributeData(CountAttributeInfo, CountValues, PointIndex, 1)
			|| CountValues.Num() != 1 || CountValues[0] < 0 || CountValues[0] > MAX_int32)
		{
			return false;
		}

		OutSize = static_cast<int32>(CountValues[0]);
		return true;
	};

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		// Indexed element attributes are paired with an explicit count so nested and empty arrays can
		// round-trip without relying on any particular member attribute being present.
		int32 ArraySize = 0;
		if (!GetContainerSize(AttributeName, ArraySize))
			return false;

		FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
		ArrayHelper.EmptyValues();
		ArrayHelper.AddValues(ArraySize);

		bool bAppliedElement = ArraySize == 0;
		for (int32 Index = 0; Index < ArraySize; ++Index)
		{
			bAppliedElement |= ApplyPointPropertyAttribute(
				HGPO, ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index),
				AttributeName + TEXT("_") + LexToString(Index), PointIndex, PointDataAssets);
		}
		return bAppliedElement;
	}

	if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		int32 SetSize = 0;
		if (!GetContainerSize(AttributeName, SetSize))
			return false;

		FScriptSetHelper SetHelper(SetProperty, ValuePtr);
		SetHelper.EmptyElements(SetSize);

		bool bAppliedElement = SetSize == 0;
		for (int32 Index = 0; Index < SetSize; ++Index)
		{
			const int32 NewIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			bAppliedElement |= ApplyPointPropertyAttribute(
				HGPO, SetProperty->ElementProp, SetHelper.GetElementPtr(NewIndex),
				AttributeName + TEXT("_") + LexToString(Index), PointIndex, PointDataAssets);
		}
		SetHelper.Rehash();
		return bAppliedElement;
	}

	if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		int32 MapSize = 0;
		if (!GetContainerSize(AttributeName, MapSize))
			return false;

		FScriptMapHelper MapHelper(MapProperty, ValuePtr);
		MapHelper.EmptyValues(MapSize);

		bool bAppliedElement = MapSize == 0;
		for (int32 Index = 0; Index < MapSize; ++Index)
		{
			const int32 NewIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			const FString ElementPrefix = AttributeName + TEXT("_") + LexToString(Index);
			bAppliedElement |= ApplyPointPropertyAttribute(
				HGPO, MapProperty->KeyProp, MapHelper.GetKeyPtr(NewIndex), ElementPrefix + TEXT("_key"), PointIndex, PointDataAssets);
			bAppliedElement |= ApplyPointPropertyAttribute(
				HGPO, MapProperty->ValueProp, MapHelper.GetValuePtr(NewIndex), ElementPrefix + TEXT("_value"), PointIndex, PointDataAssets);
		}
		MapHelper.Rehash();
		return bAppliedElement;
	}

	// Keep the UTF-8 storage alive while the accessor copies the name. This avoids passing the
	// temporary buffer produced by H_TCHAR_TO_UTF8 through the TCHAR accessor constructor.
	FTCHARToUTF8 Utf8AttributeName(*AttributeName);
	FHoudiniHapiAccessor Accessor(HGPO.GeoId, HGPO.PartId, Utf8AttributeName.Get());
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	bool bHasPointAttribute = Accessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT) && AttributeInfo.count > PointIndex;
	if (!bHasPointAttribute && !CastField<FStructProperty>(Property))
		return false;

	// Accept only scalar storage here. Array properties returned above and use indexed attributes.
	bool bIntegerStorage = AttributeInfo.storage == HAPI_STORAGETYPE_INT
		|| AttributeInfo.storage == HAPI_STORAGETYPE_INT64
		|| AttributeInfo.storage == HAPI_STORAGETYPE_UINT8
		|| AttributeInfo.storage == HAPI_STORAGETYPE_INT8
		|| AttributeInfo.storage == HAPI_STORAGETYPE_INT16;
	bool bFloatingStorage = AttributeInfo.storage == HAPI_STORAGETYPE_FLOAT || AttributeInfo.storage == HAPI_STORAGETYPE_FLOAT64;
	if (PointDataAssets && IsObjectReferenceProperty(Property) && AttributeInfo.tupleSize == 1)
	{
		auto SetPointReference = [PointDataAssets, Property, ValuePtr](const int64 ReferencedPointIndex)
		{
			if (ReferencedPointIndex < INDEX_NONE || ReferencedPointIndex >= PointDataAssets->Num())
				return false;

			UDataAsset* ReferencedDataAsset = ReferencedPointIndex == INDEX_NONE
				? nullptr : (*PointDataAssets)[static_cast<int32>(ReferencedPointIndex)];
			if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
			{
				SoftObjectProperty->SetPropertyValue(ValuePtr, FSoftObjectPtr(ReferencedDataAsset));
				return true;
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				ObjectProperty->SetObjectPropertyValue(ValuePtr, ReferencedDataAsset);
				return true;
			}

			return false;
		};

		if (bIntegerStorage)
		{
			TArray<int64> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
				return false;

			return SetPointReference(Values[0]);
		}

		if (AttributeInfo.storage == HAPI_STORAGETYPE_STRING)
		{
			TArray<FString> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
				return false;

			int64 ReferencedPointIndex = INDEX_NONE;
			if (LexTryParseString(ReferencedPointIndex, *Values[0]))
				return SetPointReference(ReferencedPointIndex);
		}
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == TBaseStructure<FVector>::Get())
		{
			if (!bHasPointAttribute || AttributeInfo.tupleSize != 3 || !bFloatingStorage)
				return false;

			TArray<double> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 3)
				return false;

			// Data Asset FVector values are not necessarily positions, so preserve their components without
			// applying Houdini-to-Unreal coordinate conversion.
			FVector* Vector = static_cast<FVector*>(ValuePtr);
			*Vector = FVector(
				Values[0],
				Values[1],
				Values[2]);
			return true;
		}

		if (StructProperty->Struct == TBaseStructure<FColor>::Get())
		{
			if (!bHasPointAttribute || AttributeInfo.tupleSize != 4 || !bIntegerStorage)
				return false;

			TArray<int64> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 4)
				return false;

			FColor* Color = static_cast<FColor*>(ValuePtr);
			*Color = FColor(
				static_cast<uint8>(Values[0]),
				static_cast<uint8>(Values[1]),
				static_cast<uint8>(Values[2]),
				static_cast<uint8>(Values[3]));
			return true;
		}

		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
		{
			if (!bHasPointAttribute || AttributeInfo.tupleSize != 4 || !bFloatingStorage)
				return false;

			TArray<double> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 4)
				return false;

			FLinearColor* LinearColor = static_cast<FLinearColor*>(ValuePtr);
			*LinearColor = FLinearColor(
				static_cast<float>(Values[0]),
				static_cast<float>(Values[1]),
				static_cast<float>(Values[2]),
				static_cast<float>(Values[3]));
			return true;
		}

		// Structs have no parent attribute in the point-attribute representation. Rebuild each
		// editable member from the corresponding Parent_Member attribute, leaving missing members
		// at their defaults.
		bool bAppliedMember = false;
		for (TFieldIterator<FProperty> MemberIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); MemberIt; ++MemberIt)
		{
			FProperty* MemberProperty = *MemberIt;
			if (!MemberProperty->HasAnyPropertyFlags(CPF_Edit) || MemberProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
				continue;

			FString MemberName = MemberProperty->GetAuthoredName();
			FHoudiniEngineUtils::SanitizeHAPIVariableName(MemberName);
			bAppliedMember |= ApplyPointPropertyAttribute(
				HGPO, MemberProperty, MemberProperty->ContainerPtrToValuePtr<void>(ValuePtr), AttributeName + TEXT("_") + MemberName, PointIndex, PointDataAssets);
		}
		return bAppliedMember;
	}

	if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
			return false;

		TArray<FString> Values;
		if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;
		StringProperty->SetPropertyValue(ValuePtr, Values[0]);
		return true;
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
			return false;

		TArray<FString> Values;
		if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;
		NameProperty->SetPropertyValue(ValuePtr, FName(*Values[0]));
		return true;
	}

	if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
			return false;

		TArray<FString> Values;
		if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;
		TextProperty->SetPropertyValue(ValuePtr, FText::FromString(Values[0]));
		return true;
	}

	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
			return false;

		TArray<FString> Values;
		if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;
		SoftObjectProperty->SetPropertyValue(ValuePtr, FSoftObjectPtr(FSoftObjectPath(Values[0])));
		return true;
	}

	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
			return false;

		TArray<FString> Values;
		if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;

		// An empty path represents a null reference. Non-empty object paths must load as the
		// property's declared class before the existing property value is replaced.
		UObject* Object = Values[0].IsEmpty() ? nullptr : StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *Values[0]);
		if (!Object && !Values[0].IsEmpty())
			return false;
		ObjectProperty->SetObjectPropertyValue(ValuePtr, Object);
		return true;
	}

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
			return false;

		FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		UEnum* Enum = EnumProperty->GetEnum();
		TArray<FString> Values;
		if (!UnderlyingProperty || !Enum || !Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;

		int64 EnumValue = Enum->GetValueByNameString(Values[0]);
		if (EnumValue == INDEX_NONE)
			return false;
		UnderlyingProperty->SetIntPropertyValue(ValuePtr, EnumValue);
		return true;
	}

	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		if (!bIntegerStorage)
			return false;

		TArray<int8> Values;
		if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
			return false;
		BoolProperty->SetPropertyValue(ValuePtr, Values[0] != 0);
		return true;
	}

	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		if (NumericProperty->IsEnum())
		{
			if (AttributeInfo.storage != HAPI_STORAGETYPE_STRING)
				return false;

			UEnum* Enum = NumericProperty->GetIntPropertyEnum();
			TArray<FString> Values;
			if (!Enum || !Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
				return false;

			int64 EnumValue = Enum->GetValueByNameString(Values[0]);
			if (EnumValue == INDEX_NONE)
				return false;
			NumericProperty->SetIntPropertyValue(ValuePtr, EnumValue);
			return true;
		}

		if (NumericProperty->IsFloatingPoint())
		{
			if (!bFloatingStorage)
				return false;

			TArray<double> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
				return false;
			NumericProperty->SetFloatingPointPropertyValue(ValuePtr, Values[0]);
			return true;
		}

		if (NumericProperty->IsInteger())
		{
			if (!bIntegerStorage)
				return false;

			TArray<int64> Values;
			if (!Accessor.GetAttributeData(AttributeInfo, Values, PointIndex, 1) || Values.Num() != 1)
				return false;
			NumericProperty->SetIntPropertyValue(ValuePtr, Values[0]);
			return true;
		}
	}

	return false;
}
