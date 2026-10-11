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

#include "UnrealDataAssetTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInputObject.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"

#include "Engine/DataAsset.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

const FString FUnrealDataAssetTranslator::ObjectPathDictionaryKey(TEXT("__object_path"));
const FString FUnrealDataAssetTranslator::ClassPathDictionaryKey(TEXT("__class_path"));
const FString FUnrealDataAssetTranslator::UniqueIdDictionaryKey(TEXT("__unique_id"));
const FString FUnrealDataAssetTranslator::BakeFolderDictionaryKey(TEXT("__bake_folder"));
const FString FUnrealDataAssetTranslator::BakeOutputNameDictionaryKey(TEXT("__bake_output_name"));
const FString FUnrealDataAssetTranslator::MapKeyDictionaryKey(TEXT("__key"));
const FString FUnrealDataAssetTranslator::MapValueDictionaryKey(TEXT("__value"));

UDataAsset* FUnrealDataAssetTranslator::GetReferencedDataAsset(FProperty* Property, void* ValuePtr)
{
	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		// Only load soft references that are explicitly declared as Data Assets. Other soft references
		// retain the existing path-only behavior and do not cause unexpected asset loads during upload.
		if (SoftObjectProperty->PropertyClass && SoftObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass()))
			return Cast<UDataAsset>(SoftObjectProperty->GetPropertyValue(ValuePtr).LoadSynchronous());
		return nullptr;
	}

	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		return Cast<UDataAsset>(ObjectProperty->GetObjectPropertyValue(ValuePtr));

	return nullptr;
}

bool FUnrealDataAssetTranslator::GetReferencedDataAssetPath(FProperty* Property, void* ValuePtr, FString& OutPath)
{
	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		if (!SoftObjectProperty->PropertyClass || !SoftObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass()))
			return false;

		OutPath = SoftObjectProperty->GetPropertyValue(ValuePtr).ToSoftObjectPath().ToString();
	}
	else if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		if (UObject* Object = ObjectProperty->GetObjectPropertyValue(ValuePtr))
		{
			if (!Cast<UDataAsset>(Object))
				return false;
			OutPath = Object->GetPathName();
		}
		else if (!ObjectProperty->PropertyClass || !ObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass()))
		{
			// An unassigned base-object property does not reveal whether it is meant to hold a Data Asset.
			return false;
		}
	}
	else
	{
		return false;
	}

	// A null reference must still be a dictionary entry. This keeps an array of Data Asset
	// references homogeneously dictionary-valued, so Houdini does not coerce every entry to a string.
	if (OutPath.IsEmpty())
		OutPath = TEXT("None");
	return true;
}

bool FUnrealDataAssetTranslator::GetDataAssetJsonObject(
	UDataAsset* DataAsset,
	TSharedPtr<FJsonObject>& OutJsonObject,
	const FString& PropertyPrefix,
	bool bPrefixPropertyNamesWithClassName,
	TSet<UDataAsset*>& ActiveDataAssets)
{
	if (!DataAsset)
		return false;

	ActiveDataAssets.Add(DataAsset);
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	// Add identity and class first so nested Data Assets can be recreated without referring back to their source asset.
	JsonObject->SetStringField(ObjectPathDictionaryKey, DataAsset->GetPathName());
	JsonObject->SetStringField(ClassPathDictionaryKey, DataAsset->GetClass()->GetPathName());

	// Build the dictionary property-by-property rather than converting the whole UObject. That lets
	// this translator retain direct control of authored property names and replace Data Asset
	// references with their own recursively built dictionaries.
	for (TFieldIterator<FProperty> PropertyIt(DataAsset->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			continue;

		const FString PropertyName = Property->GetAuthoredName();
		FString DictionaryPropertyName = PropertyName;
		if (bPrefixPropertyNamesWithClassName)
		{
			DictionaryPropertyName = DataAsset->GetClass()->GetName() + TEXT("_") + DictionaryPropertyName;
			FHoudiniEngineUtils::SanitizeHAPIVariableName(DictionaryPropertyName);
		}
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(DataAsset);
		const FString PropertyPath = PropertyPrefix.IsEmpty() ? DictionaryPropertyName : PropertyPrefix + TEXT("__") + DictionaryPropertyName;
		TSharedPtr<FJsonValue> JsonValue = GetDataAssetJsonValue(Property, ValuePtr, PropertyPath, bPrefixPropertyNamesWithClassName, ActiveDataAssets);
		if (!JsonValue.IsValid())
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to serialize Data Asset property %s on %s."),
				*PropertyName, *DataAsset->GetPathName());
			continue;
		}

		JsonObject->SetField(DictionaryPropertyName, JsonValue);
	}

	OutJsonObject = JsonObject;
	ActiveDataAssets.Remove(DataAsset);
	return true;
}

TSharedPtr<FJsonValue> FUnrealDataAssetTranslator::GetDataAssetJsonValue(
	FProperty* Property,
	void* ValuePtr,
	const FString& PropertyPath,
	bool bPrefixPropertyNamesWithClassName,
	TSet<UDataAsset*>& ActiveDataAssets,
	FProperty* OuterProperty)
{
	if (!Property || !ValuePtr)
		return nullptr;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		// Arrays retain their native JSON-array shape while each element is built independently.
		FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(ArrayHelper.Num());

		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			TSharedPtr<FJsonValue> ElementValue = GetDataAssetJsonValue(
				ArrayProperty->Inner,
				ArrayHelper.GetRawPtr(Index),
				PropertyPath + TEXT("__") + LexToString(Index),
				bPrefixPropertyNamesWithClassName,
				ActiveDataAssets,
				ArrayProperty);
			JsonValues.Add(ElementValue.IsValid() ? ElementValue : MakeShared<FJsonValueNull>());
		}
		return MakeShared<FJsonValueArray>(MoveTemp(JsonValues));
	}

	if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		// Sets use JSON arrays. Their order only needs to stay stable for this single export.
		FScriptSetHelper SetHelper(SetProperty, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(SetHelper.Num());

		int32 JsonIndex = 0;
		for (FScriptSetHelper::FIterator It(SetHelper); It; ++It, ++JsonIndex)
		{
			TSharedPtr<FJsonValue> ElementValue = GetDataAssetJsonValue(
				SetProperty->ElementProp,
				SetHelper.GetElementPtr(It),
				PropertyPath + TEXT("__") + LexToString(JsonIndex),
				bPrefixPropertyNamesWithClassName,
				ActiveDataAssets,
				SetProperty);
			JsonValues.Add(ElementValue.IsValid() ? ElementValue : MakeShared<FJsonValueNull>());
		}
		return MakeShared<FJsonValueArray>(MoveTemp(JsonValues));
	}

	if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		// JSON object keys cannot be dictionaries, so maps use explicit entries. This lets either a key
		// or a value be a nested Data Asset while preserving the complete reflected map type.
		FScriptMapHelper MapHelper(MapProperty, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> MapEntries;
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
		{
			TSharedPtr<FJsonValue> KeyJsonValue = GetDataAssetJsonValue(
				MapProperty->KeyProp, MapHelper.GetKeyPtr(It), PropertyPath + TEXT("__key"), bPrefixPropertyNamesWithClassName, ActiveDataAssets, MapProperty);
			TSharedPtr<FJsonValue> ValueJsonValue = GetDataAssetJsonValue(
				MapProperty->ValueProp, MapHelper.GetValuePtr(It), PropertyPath + TEXT("__value"), bPrefixPropertyNamesWithClassName, ActiveDataAssets, MapProperty);
			if (!KeyJsonValue.IsValid() || !ValueJsonValue.IsValid())
				continue;

			TSharedRef<FJsonObject> MapEntry = MakeShared<FJsonObject>();
			MapEntry->SetField(MapKeyDictionaryKey, KeyJsonValue);
			MapEntry->SetField(MapValueDictionaryKey, ValueJsonValue);
			MapEntries.Add(MakeShared<FJsonValueObject>(MapEntry));
		}
		return MakeShared<FJsonValueArray>(MoveTemp(MapEntries));
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();

		for (TFieldIterator<FProperty> MemberIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); MemberIt; ++MemberIt)
		{
			FProperty* MemberProperty = *MemberIt;
			if (!MemberProperty->HasAnyPropertyFlags(CPF_Edit) || MemberProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
				continue;

			const FString MemberName = MemberProperty->GetAuthoredName();
			TSharedPtr<FJsonValue> MemberJsonValue = GetDataAssetJsonValue(
				MemberProperty,
				MemberProperty->ContainerPtrToValuePtr<void>(ValuePtr),
				PropertyPath + TEXT("__") + MemberName,
				bPrefixPropertyNamesWithClassName,
				ActiveDataAssets,
				StructProperty);
			if (!MemberJsonValue.IsValid())
				continue;

			JsonObject->SetField(MemberName, MemberJsonValue);
		}
		return MakeShared<FJsonValueObject>(JsonObject);
	}

	FString ReferencePath;
	// Data Asset references are represented as nested dictionaries so their path and editable values
	// can be transferred together. All other leaf properties use Unreal's normal JSON conversion.
	// This also distinguishes a null Data Asset reference, which still has a dictionary containing
	// `__object_path`, from an unrelated object-reference property that should serialize normally.
	if (!GetReferencedDataAssetPath(Property, ValuePtr, ReferencePath))
	{
		return FJsonObjectConverter::UPropertyToJsonValue(
			Property,
			ValuePtr,
			CPF_Edit,
			CPF_Transient | CPF_Deprecated,
			nullptr,
			OuterProperty,
			EJsonObjectConversionFlags::SkipStandardizeCase);
	}

	UDataAsset* ReferencedDataAsset = GetReferencedDataAsset(Property, ValuePtr);
	TSharedPtr<FJsonObject> ReferenceObject = MakeShared<FJsonObject>();
	ReferenceObject->SetStringField(ObjectPathDictionaryKey, ReferencePath);
	if (!ReferencedDataAsset)
		return MakeShared<FJsonValueObject>(ReferenceObject);

	if (ActiveDataAssets.Contains(ReferencedDataAsset))
	{
		// Keep a path-only entry for the circular reference, but omit values that would recurse forever.
		HOUDINI_LOG_WARNING(TEXT("Detected a circular Data Asset reference at %s. Skipping its nested dictionary values."), *PropertyPath);
		return MakeShared<FJsonValueObject>(ReferenceObject);
	}

	TSharedPtr<FJsonObject> ReferencedValues;
	if (!GetDataAssetJsonObject(ReferencedDataAsset, ReferencedValues, PropertyPath, bPrefixPropertyNamesWithClassName, ActiveDataAssets))
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to serialize referenced Data Asset %s at %s."),
			*ReferencedDataAsset->GetPathName(), *PropertyPath);
		return MakeShared<FJsonValueObject>(ReferenceObject);
	}

	// The referenced object already contains its own values. Preserve the property path used by this
	// reference, which is the path output must use if it later recreates the reference.
	ReferencedValues->SetStringField(ObjectPathDictionaryKey, ReferencePath);
	return MakeShared<FJsonValueObject>(ReferencedValues);
}

bool FUnrealDataAssetTranslator::IsDataAssetReferenceProperty(FProperty* Property)
{
	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		return SoftObjectProperty->PropertyClass && SoftObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass());

	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		return ObjectProperty->PropertyClass && ObjectProperty->PropertyClass->IsChildOf(UDataAsset::StaticClass());

	return false;
}

void FUnrealDataAssetTranslator::GatherReferencedDataAssets(
	FProperty* Property,
	void* ValuePtr,
	TArray<UDataAsset*>& DataAssets,
	TMap<UDataAsset*, int32>& DataAssetPointIndices)
{
	if (!Property || !ValuePtr)
		return;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			GatherReferencedDataAssets(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index), DataAssets, DataAssetPointIndices);
		return;
	}

	if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper SetHelper(SetProperty, ValuePtr);
		for (FScriptSetHelper::FIterator It(SetHelper); It; ++It)
			GatherReferencedDataAssets(SetProperty->ElementProp, SetHelper.GetElementPtr(It), DataAssets, DataAssetPointIndices);
		return;
	}

	if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper MapHelper(MapProperty, ValuePtr);
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
		{
			GatherReferencedDataAssets(MapProperty->KeyProp, MapHelper.GetKeyPtr(It), DataAssets, DataAssetPointIndices);
			GatherReferencedDataAssets(MapProperty->ValueProp, MapHelper.GetValuePtr(It), DataAssets, DataAssetPointIndices);
		}
		return;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		for (TFieldIterator<FProperty> MemberIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); MemberIt; ++MemberIt)
		{
			FProperty* MemberProperty = *MemberIt;
			if (MemberProperty->HasAnyPropertyFlags(CPF_Edit)
				&& !MemberProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			{
				GatherReferencedDataAssets(MemberProperty, MemberProperty->ContainerPtrToValuePtr<void>(ValuePtr), DataAssets, DataAssetPointIndices);
			}
		}
		return;
	}

	if (IsDataAssetReferenceProperty(Property))
	{
		UDataAsset* ReferencedDataAsset = GetReferencedDataAsset(Property, ValuePtr);
		if (!ReferencedDataAsset)
			return;

		if (DataAssetPointIndices.Contains(ReferencedDataAsset))
			return;

		DataAssetPointIndices.Add(ReferencedDataAsset, DataAssets.Num());
		DataAssets.Add(ReferencedDataAsset);
		for (TFieldIterator<FProperty> PropertyIt(ReferencedDataAsset->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* ReferencedProperty = *PropertyIt;
			if (!ReferencedProperty->HasAnyPropertyFlags(CPF_Edit) || ReferencedProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
				continue;

			GatherReferencedDataAssets(
				ReferencedProperty, ReferencedProperty->ContainerPtrToValuePtr<void>(ReferencedDataAsset), DataAssets, DataAssetPointIndices);
		}
	}
}

void FUnrealDataAssetTranslator::GetDataAssetPropertyAttributeNames(
	FProperty* Property,
	void* ValuePtr,
	const FString& AttributeName,
	TSet<FString>& AttributeNames)
{
	if (!Property || !ValuePtr)
		return;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		AttributeNames.Add(AttributeName + TEXT("_num"));
		FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			GetDataAssetPropertyAttributeNames(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index), AttributeName + TEXT("_") + LexToString(Index), AttributeNames);
		return;
	}

	if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		AttributeNames.Add(AttributeName + TEXT("_num"));
		FScriptSetHelper SetHelper(SetProperty, ValuePtr);
		int32 Index = 0;
		for (FScriptSetHelper::FIterator It(SetHelper); It; ++It, ++Index)
			GetDataAssetPropertyAttributeNames(SetProperty->ElementProp, SetHelper.GetElementPtr(It), AttributeName + TEXT("_") + LexToString(Index), AttributeNames);
		return;
	}

	if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		AttributeNames.Add(AttributeName + TEXT("_num"));
		FScriptMapHelper MapHelper(MapProperty, ValuePtr);
		int32 Index = 0;
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It, ++Index)
		{
			const FString ElementPrefix = AttributeName + TEXT("_") + LexToString(Index);
			GetDataAssetPropertyAttributeNames(MapProperty->KeyProp, MapHelper.GetKeyPtr(It), ElementPrefix + TEXT("_key"), AttributeNames);
			GetDataAssetPropertyAttributeNames(MapProperty->ValueProp, MapHelper.GetValuePtr(It), ElementPrefix + TEXT("_value"), AttributeNames);
		}
		return;
	}

	if (Property->ArrayDim == 1)
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FVector>::Get()
				|| StructProperty->Struct == TBaseStructure<FColor>::Get()
				|| StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				AttributeNames.Add(AttributeName);
				return;
			}

			bool bHasEditableMember = false;
			for (TFieldIterator<FProperty> MemberIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); MemberIt; ++MemberIt)
			{
				FProperty* MemberProperty = *MemberIt;
				if (!MemberProperty->HasAnyPropertyFlags(CPF_Edit) || MemberProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
					continue;

				bHasEditableMember = true;
				FString MemberName = MemberProperty->GetAuthoredName();
				FHoudiniEngineUtils::SanitizeHAPIVariableName(MemberName);
				GetDataAssetPropertyAttributeNames(
					MemberProperty, MemberProperty->ContainerPtrToValuePtr<void>(ValuePtr), AttributeName + TEXT("_") + MemberName, AttributeNames);
			}
			if (bHasEditableMember)
				return;
		}

		AttributeNames.Add(AttributeName);
		return;
	}

	AttributeNames.Add(AttributeName);
}

bool FUnrealDataAssetTranslator::AddPointAttributeDefinition(
	HAPI_NodeId InputNodeId,
	const FString& AttributeName,
	const HAPI_AttributeInfo& AttributeInfo,
	const FDataAssetAttributeContext* AttributeContext)
{
	if (AttributeContext)
	{
		if (const FDataAssetAttributeDefinition* ExistingDefinition = AttributeContext->AddedAttributes.Find(AttributeName))
		{
			if (ExistingDefinition->StorageType != AttributeInfo.storage || ExistingDefinition->TupleSize != AttributeInfo.tupleSize)
			{
				HOUDINI_LOG_ERROR(TEXT("Cannot export Data Asset attribute '%s': it is used with incompatible property types. Enable 'Prefix Data Asset Property Names with Class Name' on the Data Asset input to avoid same-named properties from different classes."), *AttributeName);
				return false;
			}

			return true;
		}
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo), false);
	if (AttributeContext)
	{
		AttributeContext->AddedAttributes.Add(AttributeName, {
			AttributeInfo.storage,
			AttributeInfo.tupleSize
		});
	}

	return true;
}

bool FUnrealDataAssetTranslator::AddPointStringAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FString& Value, const FDataAssetAttributeContext* AttributeContext)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = AttributeContext ? AttributeContext->PointCount : 1;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	if (!AddPointAttributeDefinition(InputNodeId, AttributeName, AttributeInfo, AttributeContext))
		return false;

	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return AttributeContext
		? Accessor.SetAttributeData(AttributeInfo, &Value, AttributeContext->PointIndex, 1)
		: Accessor.SetAttributeUniqueData(AttributeInfo, Value);
}

bool FUnrealDataAssetTranslator::AddPointDictionaryAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FString& Value)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = 1;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_DICTIONARY;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo), false);

	TArray<FString> DictionaryValues = { Value };
	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return Accessor.SetAttributeDictionary(AttributeInfo, DictionaryValues);
}

bool FUnrealDataAssetTranslator::AddPointInt64Attribute(HAPI_NodeId InputNodeId, const FString& AttributeName, int64 Value, const FDataAssetAttributeContext* AttributeContext)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = AttributeContext ? AttributeContext->PointCount : 1;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	if (!AddPointAttributeDefinition(InputNodeId, AttributeName, AttributeInfo, AttributeContext))
		return false;

	TArray<int64> Values = { Value };
	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return AttributeContext
		? Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext->PointIndex, 1)
		: Accessor.SetAttributeData(AttributeInfo, Values);
}

bool FUnrealDataAssetTranslator::AddPointVectorAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FVector& Value, const FDataAssetAttributeContext* AttributeContext)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = AttributeContext ? AttributeContext->PointCount : 1;
	AttributeInfo.tupleSize = 3;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_VECTOR;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	if (!AddPointAttributeDefinition(InputNodeId, AttributeName, AttributeInfo, AttributeContext))
		return false;

	// Data Asset FVector properties can contain arbitrary values, not only positions. Deliberately do
	// not apply Unreal-to-Houdini coordinate conversion: preserve their authored components exactly.
	TArray<float> Values = {
		static_cast<float>(Value.X),
		static_cast<float>(Value.Y),
		static_cast<float>(Value.Z)
	};
	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return AttributeContext
		? Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext->PointIndex, 1)
		: Accessor.SetAttributeData(AttributeInfo, Values);
}

bool FUnrealDataAssetTranslator::AddPointColorAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FColor& Value, const FDataAssetAttributeContext* AttributeContext)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = AttributeContext ? AttributeContext->PointCount : 1;
	AttributeInfo.tupleSize = 4;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_UINT8;
	AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_COLOR;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	if (!AddPointAttributeDefinition(InputNodeId, AttributeName, AttributeInfo, AttributeContext))
		return false;

	// Keep FColor in its native 8-bit RGBA representation so all four authored channels round-trip.
	TArray<uint8> Values = { Value.R, Value.G, Value.B, Value.A };
	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return AttributeContext
		? Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext->PointIndex, 1)
		: Accessor.SetAttributeData(AttributeInfo, Values);
}

bool FUnrealDataAssetTranslator::AddPointLinearColorAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FLinearColor& Value, const FDataAssetAttributeContext* AttributeContext)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = AttributeContext ? AttributeContext->PointCount : 1;
	AttributeInfo.tupleSize = 4;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_COLOR;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	if (!AddPointAttributeDefinition(InputNodeId, AttributeName, AttributeInfo, AttributeContext))
		return false;

	// Linear colors stay in their authored linear RGBA form; they are values, not positions.
	TArray<float> Values = { Value.R, Value.G, Value.B, Value.A };
	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return AttributeContext
		? Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext->PointIndex, 1)
		: Accessor.SetAttributeData(AttributeInfo, Values);
}

template<typename DataType>
bool FUnrealDataAssetTranslator::AddPointArrayAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, HAPI_StorageType StorageType, TArray<DataType>& Values)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = 1;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.storage = StorageType;
	AttributeInfo.totalArrayElements = Values.Num();
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo), false);

	// There is one Data Asset point, so HAPI expects one entry describing this property's array length.
	TArray<int> Sizes = { Values.Num() };
	FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
	return Accessor.SetAttributeArrayData(AttributeInfo, Values, Sizes);
}

bool FUnrealDataAssetTranslator::AddDataAssetPropertyAttributes(
	HAPI_NodeId InputNodeId,
	UDataAsset* DataAsset,
	const FDataAssetAttributeContext& AttributeContext,
	bool bPrefixPropertyNamesWithClassName)
{
	if (!DataAsset)
		return true;

	for (TFieldIterator<FProperty> PropertyIt(DataAsset->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			continue;

		FString PropertyName = Property->GetAuthoredName();
		if (bPrefixPropertyNamesWithClassName)
			PropertyName = DataAsset->GetClass()->GetName() + TEXT("_") + PropertyName;
		FHoudiniEngineUtils::SanitizeHAPIVariableName(PropertyName);
		FDataAssetProperty DataAssetProperty = {
			Property,
			PropertyName
		};
		if (!AddPointPropertyAttribute(
			InputNodeId, DataAssetProperty, Property->ContainerPtrToValuePtr<void>(DataAsset), DataAsset, AttributeContext))
			return false;
	}

	return true;
}

bool FUnrealDataAssetTranslator::AddDataAssetDictionaryAttribute(
	HAPI_NodeId InputNodeId,
	UDataAsset* DataAsset,
	bool bPrefixPropertyNamesWithClassName)
{
	// Houdini dictionaries store JSON. Use the engine's reflection converter so nested structs,
	// arrays, and scalar values retain their JSON types in one attribute. Referenced Data Assets
	// become nested objects containing their path and editable values.
	FString DataAssetValues;
	TSharedPtr<FJsonObject> DataAssetValuesObject;
	TSet<UDataAsset*> ActiveDataAssets;
	if (!GetDataAssetJsonObject(DataAsset, DataAssetValuesObject, FString(), bPrefixPropertyNamesWithClassName, ActiveDataAssets))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to serialize Data Asset %s to a Houdini dictionary."), *DataAsset->GetPathName());
		return false;
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DataAssetValues);
	if (!FJsonSerializer::Serialize(DataAssetValuesObject.ToSharedRef(), Writer))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to write Data Asset %s as a Houdini dictionary."), *DataAsset->GetPathName());
		return false;
	}

	return AddPointDictionaryAttribute(InputNodeId, HAPI_UNREAL_ATTRIB_DATA_ASSET_VALUES, DataAssetValues);
}

bool FUnrealDataAssetTranslator::AddPointPropertyAttribute(
	HAPI_NodeId InputNodeId,
	const FDataAssetProperty& DataAssetProperty,
	void* ValuePtr,
	UObject* ExportObject,
	const FDataAssetAttributeContext& AttributeContext)
{
	FProperty* Property = DataAssetProperty.Property;
	if (!Property || !ValuePtr)
		return false;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = 1;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_POINT;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);

		// HAPI point attributes cannot directly express nested arrays. Represent every element by an
		// indexed attribute path, e.g. Array_0_Struct_Value. The count keeps empty arrays distinct
		// from absent attributes and lets output rebuild the exact array length.
		if (!AddPointInt64Attribute(InputNodeId, DataAssetProperty.AttributeName + TEXT("_num"), ArrayHelper.Num(), &AttributeContext))
			return false;

		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			FDataAssetProperty ElementDataAssetProperty = {
				ArrayProperty->Inner,
				DataAssetProperty.AttributeName + TEXT("_") + LexToString(Index)
			};
			if (!AddPointPropertyAttribute(InputNodeId, ElementDataAssetProperty, ArrayHelper.GetRawPtr(Index), ExportObject, AttributeContext))
				return false;
		}

		return true;
	}

	if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper SetHelper(SetProperty, ValuePtr);
		if (!AddPointInt64Attribute(InputNodeId, DataAssetProperty.AttributeName + TEXT("_num"), SetHelper.Num(), &AttributeContext))
			return false;

		int32 Index = 0;
		for (FScriptSetHelper::FIterator It(SetHelper); It; ++It, ++Index)
		{
			FDataAssetProperty ElementDataAssetProperty = {
				SetProperty->ElementProp,
				DataAssetProperty.AttributeName + TEXT("_") + LexToString(Index)
			};
			if (!AddPointPropertyAttribute(InputNodeId, ElementDataAssetProperty, SetHelper.GetElementPtr(It), ExportObject, AttributeContext))
				return false;
		}

		return true;
	}

	if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper MapHelper(MapProperty, ValuePtr);
		if (!AddPointInt64Attribute(InputNodeId, DataAssetProperty.AttributeName + TEXT("_num"), MapHelper.Num(), &AttributeContext))
			return false;

		int32 Index = 0;
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It, ++Index)
		{
			const FString ElementPrefix = DataAssetProperty.AttributeName + TEXT("_") + LexToString(Index);
			FDataAssetProperty KeyDataAssetProperty = { MapProperty->KeyProp, ElementPrefix + TEXT("_key") };
			FDataAssetProperty ValueDataAssetProperty = { MapProperty->ValueProp, ElementPrefix + TEXT("_value") };
			if (!AddPointPropertyAttribute(InputNodeId, KeyDataAssetProperty, MapHelper.GetKeyPtr(It), ExportObject, AttributeContext)
				|| !AddPointPropertyAttribute(InputNodeId, ValueDataAssetProperty, MapHelper.GetValuePtr(It), ExportObject, AttributeContext))
			{
				return false;
			}
		}

		return true;
	}

	if (Property->ArrayDim == 1)
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
				return AddPointVectorAttribute(InputNodeId, DataAssetProperty.AttributeName, *static_cast<FVector*>(ValuePtr), &AttributeContext);
			if (StructProperty->Struct == TBaseStructure<FColor>::Get())
				return AddPointColorAttribute(InputNodeId, DataAssetProperty.AttributeName, *static_cast<FColor*>(ValuePtr), &AttributeContext);
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
				return AddPointLinearColorAttribute(InputNodeId, DataAssetProperty.AttributeName, *static_cast<FLinearColor*>(ValuePtr), &AttributeContext);

			// Flatten regular structs into individual attributes. The double underscore keeps the
			// struct-property boundary unambiguous while allowing Houdini to address each member.
			bool bExportedMember = false;
			for (TFieldIterator<FProperty> MemberIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); MemberIt; ++MemberIt)
			{
				FProperty* MemberProperty = *MemberIt;
				if (!MemberProperty->HasAnyPropertyFlags(CPF_Edit) || MemberProperty->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
					continue;

				// Blueprint-authored properties use generated internal names that include an index and GUID.
				// Use the authored name so Houdini attributes stay stable and match the Data Asset UI.
				FString MemberName = MemberProperty->GetAuthoredName();
				FHoudiniEngineUtils::SanitizeHAPIVariableName(MemberName);
				FDataAssetProperty MemberDataAssetProperty = {
					MemberProperty,
					DataAssetProperty.AttributeName + TEXT("_") + MemberName
				};
				void* MemberValuePtr = MemberProperty->ContainerPtrToValuePtr<void>(ValuePtr);
				bExportedMember |= AddPointPropertyAttribute(InputNodeId, MemberDataAssetProperty, MemberValuePtr, ExportObject, AttributeContext);
			}
			return bExportedMember;
		}

		// Keep the common user-facing text types as direct Houdini strings rather than using
		// Unreal's general text export format.
		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
			return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, StringProperty->GetPropertyValue(ValuePtr), &AttributeContext);

		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
			return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, NameProperty->GetPropertyValue(ValuePtr).ToString(), &AttributeContext);

		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, TextProperty->GetPropertyValue(ValuePtr).ToString(), &AttributeContext);

		if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		{
			if (IsDataAssetReferenceProperty(Property))
			{
				const int32* ReferencedPointIndex = AttributeContext.DataAssetPointIndices.Find(GetReferencedDataAsset(Property, ValuePtr));
				return AddPointInt64Attribute(
					InputNodeId, DataAssetProperty.AttributeName, ReferencedPointIndex ? *ReferencedPointIndex : INDEX_NONE, &AttributeContext);
			}

			return AddPointStringAttribute(
				InputNodeId, DataAssetProperty.AttributeName, SoftObjectProperty->GetPropertyValue(ValuePtr).ToSoftObjectPath().GetAssetPathString(), &AttributeContext);
		}

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* Object = ObjectProperty->GetObjectPropertyValue(ValuePtr);
			if (IsDataAssetReferenceProperty(Property))
			{
				const int32* ReferencedPointIndex = AttributeContext.DataAssetPointIndices.Find(Cast<UDataAsset>(Object));
				return AddPointInt64Attribute(
					InputNodeId, DataAssetProperty.AttributeName, ReferencedPointIndex ? *ReferencedPointIndex : INDEX_NONE, &AttributeContext);
			}

			return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, IsValid(Object) ? Object->GetPathName() : FString(), &AttributeContext);
		}

		// Enums are more useful and stable to Houdini as their authored name than as their backing integer.
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			UEnum* Enum = EnumProperty->GetEnum();
			if (UnderlyingProperty && Enum)
				return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, Enum->GetNameStringByValue(UnderlyingProperty->GetSignedIntPropertyValue(ValuePtr)), &AttributeContext);
		}

		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			AttributeInfo.count = AttributeContext.PointCount;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT8;
			if (!AddPointAttributeDefinition(InputNodeId, DataAssetProperty.AttributeName, AttributeInfo, &AttributeContext))
				return false;

			TArray<int8> Values = { static_cast<int8>(BoolProperty->GetPropertyValue(ValuePtr)) };
			FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*DataAssetProperty.AttributeName));
			return Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext.PointIndex, 1);
		}

		if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
		{
			if (NumericProperty->IsEnum())
			{
				if (UEnum* Enum = NumericProperty->GetIntPropertyEnum())
					return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, Enum->GetNameStringByValue(NumericProperty->GetSignedIntPropertyValue(ValuePtr)), &AttributeContext);
			}

			if (NumericProperty->IsFloatingPoint())
			{
				AttributeInfo.count = AttributeContext.PointCount;
				AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT64;
				if (!AddPointAttributeDefinition(InputNodeId, DataAssetProperty.AttributeName, AttributeInfo, &AttributeContext))
					return false;

				TArray<double> Values = { NumericProperty->GetFloatingPointPropertyValue(ValuePtr) };
				FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*DataAssetProperty.AttributeName));
				return Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext.PointIndex, 1);
			}

			if (NumericProperty->IsInteger())
			{
				AttributeInfo.count = AttributeContext.PointCount;
				AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
				if (!AddPointAttributeDefinition(InputNodeId, DataAssetProperty.AttributeName, AttributeInfo, &AttributeContext))
					return false;

				TArray<int64> Values = { NumericProperty->GetSignedIntPropertyValue(ValuePtr) };
				FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*DataAssetProperty.AttributeName));
				return Accessor.SetAttributeData(AttributeInfo, Values.GetData(), AttributeContext.PointIndex, 1);
			}
		}
	}

	// Structs, maps, sets, nested arrays, and unsupported array element types retain Unreal's text
	// serialization instead of being dropped. Struct arrays can later become a separate element-point stream.
	FString Value;
	Property->ExportTextItem_Direct(Value, ValuePtr, nullptr, ExportObject, PPF_None);
	return AddPointStringAttribute(InputNodeId, DataAssetProperty.AttributeName, Value, &AttributeContext);
}

bool FUnrealDataAssetTranslator::CreateInputNodeForDataAsset(
	UDataAsset* DataAsset,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	bool bInputNodesCanBeDeleted,
	bool bUseDictionaryForDataAssetValues,
	bool bPrefixPropertyNamesWithClassName,
	bool bForceRebuild)
{
	if (!IsValid(DataAsset))
		return true;

	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;
	FString FinalInputNodeName = InputNodeName;

	{
		FUnrealObjectInputOptions Options;
		Identifier = FUnrealObjectInputIdentifier(DataAsset, Options, EUnrealObjectInputNodeType::Leaf);

		// The input recook button marks the wrapper changed. Rebuild the shared leaf in that case
		// so the Data Asset's current values are uploaded instead of reusing its cached Houdini node.
		FUnrealObjectInputHandle Handle;
		if (!bForceRebuild && FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle) && Handle.IsValid())
		{
			if (!bInputNodesCanBeDeleted)
				FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

			OutHandle = Handle;
			InputNodeId = FUnrealObjectInputManager::Get().GetHAPINodeId(Handle);
			return true;
		}

		FinalInputNodeName = FUnrealObjectInputManager::Get().GetDefaultNodeName(Identifier);
		if (FUnrealObjectInputManager::Get().EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			ParentNodeId = FUnrealObjectInputManager::Get().GetHAPINodeId(ParentHandle);

		InputNodeId = Handle.IsValid() ? FUnrealObjectInputManager::Get().GetHAPINodeId(Handle) : -1;
	}

	HAPI_NodeId NewNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(FinalInputNodeName, NewNodeId, ParentNodeId), false);
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;

	HAPI_NodeId PreviousInputNodeId = InputNodeId;
	InputNodeId = NewNodeId;
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);

	if (PreviousInputNodeId >= 0)
	{
		HAPI_NodeId PreviousInputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);
		if (FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputNodeId) != HAPI_RESULT_SUCCESS)
			HOUDINI_LOG_WARNING(TEXT("Failed to clean up the previous input node for %s."), *FinalInputNodeName);
		if (FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputObjectNodeId) != HAPI_RESULT_SUCCESS)
			HOUDINI_LOG_WARNING(TEXT("Failed to clean up the previous input object node for %s."), *FinalInputNodeName);
	}

	TArray<UDataAsset*> PointDataAssets;
	TMap<UDataAsset*, int32> DataAssetPointIndices;
	TSet<FString> PropertyAttributeNames;
	if (!bUseDictionaryForDataAssetValues)
	{
		PointDataAssets.Add(DataAsset);
		DataAssetPointIndices.Add(DataAsset, 0);
		for (int32 PointIndex = 0; PointIndex < PointDataAssets.Num(); ++PointIndex)
		{
			UDataAsset* PointDataAsset = PointDataAssets[PointIndex];
			for (TFieldIterator<FProperty> PropertyIt(PointDataAsset->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
					continue;

				GatherReferencedDataAssets(
					Property, Property->ContainerPtrToValuePtr<void>(PointDataAsset), PointDataAssets, DataAssetPointIndices);
			}
		}

		for (UDataAsset* PointDataAsset : PointDataAssets)
		{
			for (TFieldIterator<FProperty> PropertyIt(PointDataAsset->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
					continue;

				FString AttributeName = Property->GetAuthoredName();
				if (bPrefixPropertyNamesWithClassName)
					AttributeName = PointDataAsset->GetClass()->GetName() + TEXT("_") + AttributeName;
				FHoudiniEngineUtils::SanitizeHAPIVariableName(AttributeName);
				GetDataAssetPropertyAttributeNames(
					Property, Property->ContainerPtrToValuePtr<void>(PointDataAsset), AttributeName, PropertyAttributeNames);
			}
		}
	}

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	PartInfo.id = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = bUseDictionaryForDataAssetValues ? 4 : 3 + PropertyAttributeNames.Num();
	PartInfo.pointCount = bUseDictionaryForDataAssetValues ? 1 : PointDataAssets.Num();
	PartInfo.type = HAPI_PARTTYPE_MESH;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &PartInfo), false);

	HAPI_AttributeInfo PositionAttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&PositionAttributeInfo);
	PositionAttributeInfo.count = PartInfo.pointCount;
	PositionAttributeInfo.tupleSize = 3;
	PositionAttributeInfo.exists = true;
	PositionAttributeInfo.owner = HAPI_ATTROWNER_POINT;
	PositionAttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	PositionAttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	if (bUseDictionaryForDataAssetValues)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &PositionAttributeInfo), false);
		TArray<float> Position = { 0.0f, 0.0f, 0.0f };
		FHoudiniHapiAccessor PositionAccessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(PositionAccessor.SetAttributeData(PositionAttributeInfo, Position), false);
		HOUDINI_CHECK_RETURN(AddPointStringAttribute(InputNodeId, HAPI_UNREAL_ATTRIB_OBJECT_PATH, DataAsset->GetPathName()), false);
		HOUDINI_CHECK_RETURN(AddPointStringAttribute(InputNodeId, HAPI_UNREAL_ATTRIB_DATA_ASSET_CLASS, DataAsset->GetClass()->GetPathName()), false);
		HOUDINI_CHECK_RETURN(AddDataAssetDictionaryAttribute(InputNodeId, DataAsset, bPrefixPropertyNamesWithClassName), false);
	}
	else
	{
	TMap<FString, FDataAssetAttributeDefinition> AddedAttributes;
		for (int32 PointIndex = 0; PointIndex < PointDataAssets.Num(); ++PointIndex)
		{
			const FDataAssetAttributeContext AttributeContext = {
				PointIndex,
				PointDataAssets.Num(),
				DataAssetPointIndices,
				AddedAttributes
			};
			HOUDINI_CHECK_RETURN(AddPointVectorAttribute(InputNodeId, HAPI_UNREAL_ATTRIB_POSITION, FVector::ZeroVector, &AttributeContext), false);
			HOUDINI_CHECK_RETURN(AddPointStringAttribute(InputNodeId, HAPI_UNREAL_ATTRIB_OBJECT_PATH, PointDataAssets[PointIndex]->GetPathName(), &AttributeContext), false);
			HOUDINI_CHECK_RETURN(AddPointStringAttribute(InputNodeId, HAPI_UNREAL_ATTRIB_DATA_ASSET_CLASS, PointDataAssets[PointIndex]->GetClass()->GetPathName(), &AttributeContext), false);
			HOUDINI_CHECK_RETURN(AddDataAssetPropertyAttributes(
				InputNodeId, PointDataAssets[PointIndex], AttributeContext, bPrefixPropertyNamesWithClassName), false);
		}
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(InputNodeId), false);
	if (!FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true))
		return false;

	FUnrealObjectInputHandle Handle;
	if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
		OutHandle = Handle;

	return true;
}
