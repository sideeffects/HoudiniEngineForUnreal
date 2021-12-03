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

#pragma once

#include "HoudiniGenericAttribute.generated.h"

UENUM()
enum class EAttribStorageType : int8
{
	Invalid		= -1,

	INT			= 0,
	INT64		= 1,
	FLOAT		= 2,
	FLOAT64		= 3,
	STRING		= 4
};

UENUM()
enum class EAttribOwner : int8
{
	Invalid = -1,

	Vertex,
	Point,
	Prim,
	Detail,
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniGenericAttributeChangedProperty
{
	GENERATED_USTRUCT_BODY()
	
	FHoudiniGenericAttributeChangedProperty();

	FHoudiniGenericAttributeChangedProperty(const FHoudiniGenericAttributeChangedProperty& InOther);
	
	FHoudiniGenericAttributeChangedProperty(UObject* InObject, FEditPropertyChain& InPropertyChain, FProperty* InProperty);

	void operator=(const FHoudiniGenericAttributeChangedProperty& InOther);
	
	/** The object from where to follow the PropertyChain to the changed property. */
	UPROPERTY()
	UObject* Object;

	/** The property chain, relative to Object, for the changed property. */
	FEditPropertyChain PropertyChain;

	/** The changed property. */
	FProperty* Property;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniGenericAttribute
{
	GENERATED_USTRUCT_BODY()

	typedef TFunction<bool(UObject* const InObject, const FString& InPropertyName, bool& bOutSkipDefaultIfPropertyNotFound, FEditPropertyChain& InPropertyChain, FProperty*& OutFoundProperty, UObject*& OutFoundPropertyObject, void*& OutContainer)> FFindPropertyFunctionType;

	UPROPERTY()
	FString AttributeName;

	UPROPERTY()
	EAttribStorageType AttributeType = EAttribStorageType::Invalid;
	UPROPERTY()
	EAttribOwner AttributeOwner = EAttribOwner::Invalid;

	UPROPERTY()
	int32 AttributeCount = -1;
	UPROPERTY()
	int32 AttributeTupleSize = -1;

	UPROPERTY()
	TArray<double> DoubleValues;
	UPROPERTY()
	TArray<int64> IntValues;
	UPROPERTY()
	TArray<FString> StringValues;

	// Accessors
	
	double GetDoubleValue(int32 index = 0) const;
	void GetDoubleTuple(TArray<double>& TupleValues, int32 index = 0) const;

	int64 GetIntValue(int32 index = 0) const;
	void GetIntTuple(TArray<int64>& TupleValues, int32 index = 0) const;

	FString GetStringValue(int32 index = 0) const;
	void GetStringTuple(TArray<FString>& TupleValues, int32 index = 0) const;

	bool GetBoolValue(int32 index = 0) const;
	void GetBoolTuple(TArray<bool>& TupleValues, int32 index = 0) const;

	void* GetData();

	// Mutators
	
	void SetDoubleValue(double InValue, int32 index = 0);
	void SetDoubleTuple(const TArray<double>& InTupleValues, int32 index = 0);

	void SetIntValue(int64 InValue, int32 index = 0);
	void SetIntTuple(const TArray<int64>& InTupleValues, int32 index = 0);

	void SetStringValue(const FString& InValue, int32 index = 0);
	void SetStringTuple(const TArray<FString>& InTupleValues, int32 index = 0);

	void SetBoolValue(bool InValue, int32 index = 0);
	void SetBoolTuple(const TArray<bool>& InTupleValues, int32 index = 0);

	//
	static bool UpdatePropertyAttributeOnObject(
		UObject* InObject, const FHoudiniGenericAttribute& InPropertyAttribute, const int32& AtIndex = 0,
		const bool bInDeferPostPropertyChangedEvents=false,
		TArray<FHoudiniGenericAttributeChangedProperty>* OutChangedProperties=nullptr,
		const FFindPropertyFunctionType& InFindPropertyFunction=nullptr);

	// Tries to find a Uproperty by name/label on an object
	// FoundPropertyObject will be the object that actually contains the property
	// and can be different from InObject if the property is nested.
	static bool FindPropertyOnObject(
		UObject* InObject,
		const FString& InPropertyName,
		FEditPropertyChain& InPropertyChain,
		FProperty*& OutFoundProperty,
		UObject*& OutFoundPropertyObject,
		void*& OutContainer);

	// Modifies the value of a found Property
	static bool ModifyPropertyValueOnObject(
		UObject* InObject,
		const FHoudiniGenericAttribute& InGenericAttribute,
		FEditPropertyChain& InPropertyChain, 
		FProperty* FoundProperty,
		void* InContainer,
		const int32& AtIndex = 0,
		const bool bInDeferPostPropertyChangedEvents=false,
		TArray<FHoudiniGenericAttributeChangedProperty>* OutChangedProperties=nullptr);

	// Gets the value of a found Property and sets it in the appropriate
	// array and index in InGenericAttribute. 
	static bool GetPropertyValueFromObject(
		UObject* InObject,
		FProperty* InFoundProperty,
		void* InContainer,
		FHoudiniGenericAttribute& InGenericAttribute,
		const int32& InAtIndex = 0);

	// Helper: determines a valid tuple size and storage type for a Houdini attribute from an Unreal FProperty
	static bool GetAttributeTupleSizeAndStorageFromProperty(
		UObject* InObject,
		FProperty* InFoundProperty,
		void* InContainer,
		int32& OutAttributeTupleSize,
		EAttribStorageType& OutAttributeStorageType);

	// Recursive search for a given property on a UObject
	static bool TryToFindProperty(
		void* InContainer,
		UStruct* InStruct,
		const FString& InPropertyName,
		FEditPropertyChain& InPropertyChain,
		FProperty*& OutFoundProperty,
		bool& bOutPropertyHasBeenFound,
		void*& OutContainer);

	// Helper to call PostEditChangePropertyChain on InObject for the InPropertyChain. 
	static bool HandlePostEditChangeProperty(UObject* InObject, FEditPropertyChain& InPropertyChain, FProperty* InProperty);

};