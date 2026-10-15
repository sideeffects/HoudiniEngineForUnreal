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

#include "Engine/DataAsset.h"

#include "HoudiniEditorTestDataAssets.generated.h"

USTRUCT()
struct FHoudiniEditorTestNestedStructData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 IntegerValue = 789;
};

USTRUCT()
struct FHoudiniEditorTestStructData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	FString StringValue = TEXT("StructValue");

	UPROPERTY(EditAnywhere, Category = "Test")
	FHoudiniEditorTestNestedStructData NestedValue;

	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<FHoudiniEditorTestNestedStructData> NestedArrayValue;
};

USTRUCT()
struct FHoudiniEditorTestDataAssetContainer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	UDataAsset* DataAsset = nullptr;
};

// Transient test data with the reflected property types covered by the typed-attribute input test.
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorTestTypedDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UHoudiniEditorTestTypedDataAsset()
	{
		FHoudiniEditorTestStructData FirstValue;
		FirstValue.StringValue = TEXT("FirstStructArrayValue");
		FirstValue.NestedValue.IntegerValue = 123;
		FHoudiniEditorTestNestedStructData FirstNestedArrayValue;
		FirstNestedArrayValue.IntegerValue = 321;
		FirstValue.NestedArrayValue.Add(FirstNestedArrayValue);
		FHoudiniEditorTestNestedStructData SecondNestedArrayValue;
		SecondNestedArrayValue.IntegerValue = 654;
		FirstValue.NestedArrayValue.Add(SecondNestedArrayValue);
		StructArrayValue.Add(FirstValue);

		FHoudiniEditorTestStructData SecondValue;
		SecondValue.StringValue = TEXT("SecondStructArrayValue");
		SecondValue.NestedValue.IntegerValue = 456;
		FHoudiniEditorTestNestedStructData ThirdNestedArrayValue;
		ThirdNestedArrayValue.IntegerValue = 987;
		SecondValue.NestedArrayValue.Add(ThirdNestedArrayValue);
		StructArrayValue.Add(SecondValue);
	}

	UPROPERTY(EditAnywhere, Category = "Test")
	FVector VectorValue = FVector(101.0, 202.0, 303.0);

	UPROPERTY(EditAnywhere, Category = "Test")
	FColor ColorValue = FColor(4, 55, 123, 246);

	UPROPERTY(EditAnywhere, Category = "Test")
	FLinearColor LinearColorValue = FLinearColor(0.125f, 0.25f, 0.5f, 0.75f);

	UPROPERTY(EditAnywhere, Category = "Test")
	FHoudiniEditorTestStructData StructValue;

	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<FHoudiniEditorTestStructData> StructArrayValue;

	UPROPERTY(EditAnywhere, Category = "Test")
	UDataAsset* ChildDataAsset = nullptr;

	// This deliberately uses a base object property. The dictionary exporter must recognize the
	// Data Asset from its runtime value and keep its nested path-and-values dictionary.
	UPROPERTY(EditAnywhere, Category = "Test")
	UObject* GenericDataAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Test")
	TSoftObjectPtr<UDataAsset> SoftChildDataAsset;

	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<UDataAsset*> ChildDataAssetArray;

	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<FHoudiniEditorTestDataAssetContainer> DataAssetContainers;

	UPROPERTY(EditAnywhere, Category = "Test")
	TSet<UDataAsset*> ChildDataAssetSet;

	UPROPERTY(EditAnywhere, Category = "Test")
	TMap<FString, UDataAsset*> ChildDataAssetMap;

	UPROPERTY(EditAnywhere, Category = "Test")
	TMap<UDataAsset*, int32> DataAssetKeyMap;
};

// Test-only hierarchy for dictionary tests that need Data Assets nested in a primary Data Asset array.
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorTestPrimaryDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<UDataAsset*> Variant;
};

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorTestPrimaryDataAssetArray : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<UPrimaryDataAsset*> Species;
};

UCLASS(BlueprintType)
class HOUDINIENGINEEDITOR_API UHoudiniSubDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	float Float = 0.0f;
};


UCLASS(BlueprintType)
class HOUDINIENGINEEDITOR_API UHoudiniTestNestedDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	float Float = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	TArray<UHoudiniSubDataAsset*> Children;
};
