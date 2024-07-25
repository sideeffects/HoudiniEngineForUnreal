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

#include "HoudiniParameter.h"

#include "HoudiniParameterString.generated.h"

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterString : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create instance of this class.
	static UHoudiniParameterString * Create(
		UObject* Outer,	const FString& ParamName);

	// Accessor
	FString GetValueAt(const uint32& Index) const { return Values.IsValidIndex(Index) ? Values[Index] : FString(); };

	UObject* GetAssetAt(const uint32& Index) const { return ChosenAssets.IsValidIndex(Index) ? ChosenAssets[Index] : nullptr; };

	int32 GetNumberOfValues() const { return Values.Num(); };

	bool IsAssetRef() const { return bIsAssetRef; };

	bool IsDefaultValueAtIndex(const int32& Idx) const;
	bool IsDefault() const override;
	
	// Mutators
	void SetIsAssetRef(const bool& InIsAssetRef) { bIsAssetRef = InIsAssetRef; };

	bool SetValueAt(const FString& InValue, const uint32& Index);

	void SetAssetAt(UObject* InObject, const uint32& Index);

	void SetNumberOfValues(const uint32& InNumValues) { Values.SetNum(InNumValues); ChosenAssets.SetNum(InNumValues); };

	TArray<UObject*> & GetChosenAssets() { return ChosenAssets; };

	void SetDefaultValues();

	// Utility

	// Get the asset reference string for a given UObject
	static FString GetAssetReference(UObject* InObject);


protected:

	// Values of this property.
	UPROPERTY()
	TArray< FString > Values;

	UPROPERTY()
	TArray< FString > DefaultValues;

	UPROPERTY()
	TArray<UObject*> ChosenAssets;

	// Indicates this string parameter should be treated as an asset reference
	// and display an object picker
	UPROPERTY()
	bool bIsAssetRef;
};