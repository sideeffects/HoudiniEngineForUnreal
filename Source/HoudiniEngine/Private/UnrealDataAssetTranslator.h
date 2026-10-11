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

#pragma once

#include "HAPI/HAPI_Common.h"
#include "Math/Color.h"
#include "Math/Vector.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class UDataAsset;
class UObject;
class FUnrealObjectInputHandle;
class FProperty;
class FJsonObject;
class FJsonValue;

struct HOUDINIENGINE_API FUnrealDataAssetTranslator
{
	// Converts a Data Asset's editable properties to Houdini point attributes. Non-dictionary inputs
	// use one point per reachable Data Asset; Data Asset references store their target point index.
	// Strings and non-Data-Asset object references are exported as strings; FVector and color values
	// are exported as typed tuples.
	// When requested, editable values are instead stored together as a JSON Houdini dictionary.
	static bool CreateInputNodeForDataAsset(
		UDataAsset* DataAsset,
		HAPI_NodeId& InputNodeId,
		const FString& InputNodeName,
		FUnrealObjectInputHandle& OutHandle,
		bool bInputNodesCanBeDeleted,
		bool bUseDictionaryForDataAssetValues,
		bool bPrefixPropertyNamesWithClassName,
		bool bForceRebuild);

	// Reserved dictionary entry containing an Unreal object path for a nested Data Asset reference.
	static const FString ObjectPathDictionaryKey;
	static const FString ClassPathDictionaryKey;
	// Reserved dictionary entry that deduplicates pathless nested Data Asset outputs.
	static const FString UniqueIdDictionaryKey;
	// Reserved dictionary entries that control a generated Data Asset's baked package.
	static const FString BakeFolderDictionaryKey;
	static const FString BakeOutputNameDictionaryKey;
	static const FString MapKeyDictionaryKey;
	static const FString MapValueDictionaryKey;

	// Serializes editable Data Asset properties to the dictionary representation used for dictionary inputs.
	static bool GetDataAssetJsonObject(
		UDataAsset* DataAsset,
		TSharedPtr<FJsonObject>& OutJsonObject,
		const FString& PropertyPrefix,
		bool bPrefixPropertyNamesWithClassName,
		TSet<UDataAsset*>& ActiveDataAssets);

private:

	struct FDataAssetProperty
	{
		FProperty* Property;
		FString AttributeName;
	};

	struct FDataAssetAttributeDefinition
	{
		HAPI_StorageType StorageType;
		int32 TupleSize;
	};

	struct FDataAssetAttributeContext
	{
		int32 PointIndex;
		int32 PointCount;
		const TMap<UDataAsset*, int32>& DataAssetPointIndices;
		TMap<FString, FDataAssetAttributeDefinition>& AddedAttributes;
	};

	static UDataAsset* GetReferencedDataAsset(FProperty* Property, void* ValuePtr);
	static bool GetReferencedDataAssetPath(FProperty* Property, void* ValuePtr, FString& OutPath);
	static TSharedPtr<FJsonValue> GetDataAssetJsonValue(FProperty* Property, void* ValuePtr, const FString& PropertyPath, bool bPrefixPropertyNamesWithClassName, TSet<UDataAsset*>& ActiveDataAssets, FProperty* OuterProperty = nullptr);
	static void GatherReferencedDataAssets(FProperty* Property, void* ValuePtr, TArray<UDataAsset*>& DataAssets, TMap<UDataAsset*, int32>& DataAssetPointIndices);
	static void GetDataAssetPropertyAttributeNames(FProperty* Property, void* ValuePtr, const FString& AttributeName, TSet<FString>& AttributeNames);
	static bool IsDataAssetReferenceProperty(FProperty* Property);
	static bool AddPointAttributeDefinition(HAPI_NodeId InputNodeId, const FString& AttributeName, const HAPI_AttributeInfo& AttributeInfo, const FDataAssetAttributeContext* AttributeContext);

	static bool AddPointStringAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FString& Value, const FDataAssetAttributeContext* AttributeContext = nullptr);
	static bool AddPointDictionaryAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FString& Value);
	static bool AddPointInt64Attribute(HAPI_NodeId InputNodeId, const FString& AttributeName, int64 Value, const FDataAssetAttributeContext* AttributeContext = nullptr);
	static bool AddPointVectorAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FVector& Value, const FDataAssetAttributeContext* AttributeContext = nullptr);
	static bool AddPointColorAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FColor& Value, const FDataAssetAttributeContext* AttributeContext = nullptr);
	static bool AddPointLinearColorAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, const FLinearColor& Value, const FDataAssetAttributeContext* AttributeContext = nullptr);

	template<typename DataType>
	static bool AddPointArrayAttribute(HAPI_NodeId InputNodeId, const FString& AttributeName, HAPI_StorageType StorageType, TArray<DataType>& Values);

	static bool AddDataAssetDictionaryAttribute(HAPI_NodeId InputNodeId, UDataAsset* DataAsset, bool bPrefixPropertyNamesWithClassName);
	static bool AddDataAssetPropertyAttributes(HAPI_NodeId InputNodeId, UDataAsset* DataAsset, const FDataAssetAttributeContext& AttributeContext, bool bPrefixPropertyNamesWithClassName);
	static bool AddPointPropertyAttribute(HAPI_NodeId InputNodeId, const FDataAssetProperty& DataAssetProperty, void* ValuePtr, UObject* ExportObject, const FDataAssetAttributeContext& AttributeContext);
};
