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

#include "HoudiniParameterInt.generated.h"

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterInt : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create instance of this class.
	static UHoudiniParameterInt * Create(
		UObject* Outer,
		const FString& ParamName);

	// Accessors
	FString GetUnit() const { return Unit; };

	bool HasMin() const { return bHasMin; };
	bool HasMax() const { return bHasMax; };
	bool HasUIMin() const { return bHasUIMin; };
	bool HasUIMax() const { return bHasUIMax; };
	bool IsLogarithmic() const { return bIsLogarithmic; };

	int32 GetMin() const { return Min; };
	int32 GetMax() const { return Max; };
	int32 GetUIMin() const { return UIMin; };
	int32 GetUIMax() const { return UIMax; };

	// Get value of this property
	TOptional<int32> GetValue(int32 Idx) const;	
	bool GetValueAt(const int32& AtIndex, int32& OutValue) const;

	int32* GetValuesPtr() { return Values.Num() > 0 ? &Values[0] : nullptr; };

	int32 GetNumberOfValues() { return Values.Num(); };

	bool IsDefaultValueAtIndex(const int32& Idx) const;
	bool IsDefault() const override;

	// Mutators
	void SetHasMin(const bool& InHasMin) { bHasMin = InHasMin; };
	void SetHasMax(const bool& InHasMax) { bHasMax = InHasMax; };
	void SetHasUIMin(const bool& InHasUIMin) { bHasUIMin = InHasUIMin; };
	void SetHasUIMax(const bool& InHasUIMax) { bHasUIMax = InHasUIMax; };

	void SetMin(const int32& InMin) { Min = InMin; };
	void SetMax(const int32& InMax) { Max = InMax; };
	void SetUIMin(const int32& InUIMin) { UIMin = InUIMin; };
	void SetUIMax(const int32& InUIMax) { UIMax = InUIMax; };
	void SetIsLogarithmic(const bool& bInIsLog) { bIsLogarithmic = bInIsLog; };

	void SetUnit(const FString& InUnit) { Unit = InUnit; };
	
	void SetNumberOfValues(const uint32& InNumValues) { Values.SetNum(InNumValues); };
	bool SetValueAt(const int32& InValue, const int32& AtIndex);

	void SetDefaultValues();

protected:

	// Int Values
	UPROPERTY()
	TArray<int32> Values;

	UPROPERTY()
	TArray<int32> DefaultValues;

	// Unit for this property
	UPROPERTY()
	FString Unit;

	// Indicates we have a min/max value
	UPROPERTY()
	bool bHasMin;
	// 
	UPROPERTY()
	bool bHasMax;

	// Indicates we have a UI min/max
	UPROPERTY()
	bool bHasUIMin;
	// 
	UPROPERTY()
	bool bHasUIMax;

	UPROPERTY()
	bool bIsLogarithmic;

	// Min and Max values for this property.
	UPROPERTY()
	int32 Min;
	// 
	UPROPERTY()
	int32 Max;

	// Min and Max values of this property for slider UI
	UPROPERTY()
	int32 UIMin;
	// 
	UPROPERTY()
	int32 UIMax;
};