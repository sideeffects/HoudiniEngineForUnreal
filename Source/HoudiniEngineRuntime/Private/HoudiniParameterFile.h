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

#include "HoudiniParameterFile.generated.h"

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterFile : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create instance of this class.
	static UHoudiniParameterFile * Create(
		UObject* Outer,
		const FString& ParamName);

	// Accessors
	FString GetFileFilters() const { return Filters; };
	bool IsReadOnly() const { return bIsReadOnly; };
	FString GetValueAt(int32 Index) const { return Values[Index]; };
	int32 GetNumValues() const { return Values.Num(); };

	void SetNumberOfValues(const uint32& NumValues) { Values.SetNum(NumValues); };
	bool SetValueAt(const FString& InValue, const uint32& Index);

	bool IsDefault() const override;

	// Mutators
	void SetFileFilters(const FString& InFilters) { Filters = InFilters; };
	void SetReadOnly(const bool& InReadOnly) { bIsReadOnly = InReadOnly; };

	void SetDefaultValues();

protected:

	// Values of this property.
	UPROPERTY()
	TArray<FString> Values;

	UPROPERTY()
	TArray<FString> DefaultValues;

	// Filters of this property.
	UPROPERTY()
	FString Filters;

	// Is the file parameter read-only?
	UPROPERTY()
	bool bIsReadOnly;
};