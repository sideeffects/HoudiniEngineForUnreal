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

#include "HoudiniParameterFloat.h"

UHoudiniParameterFloat::UHoudiniParameterFloat(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Unit(TEXT(""))
	, bNoSwap(false)
	, bHasMin(false)
	, bHasMax(false)
	, bHasUIMin(false)
	, bHasUIMax(false)
	, bIsLogarithmic(false)
	, Min(TNumericLimits<float>::Lowest())
	, Max(TNumericLimits<float>::Max())
	, UIMin(TNumericLimits<float>::Lowest())
	, UIMax(TNumericLimits<float>::Max())
{
	ParmType = EHoudiniParameterType::Float;
}

UHoudiniParameterFloat *
UHoudiniParameterFloat::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterFloat_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterFloat::StaticClass(), *ParamNameStr);
	// We need to create a new parameter
	UHoudiniParameterFloat * HoudiniAssetParameter = NewObject< UHoudiniParameterFloat >(
		InOuter, UHoudiniParameterFloat::StaticClass(), ParamName, RF_Public | RF_Transactional);

	//HoudiniAssetParameter->UpdateFromParmInfo(InParentParameter, InNodeId, ParmInfo);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::Float);

	HoudiniAssetParameter->bIsChildOfRamp = false;
	HoudiniAssetParameter->bIsLogarithmic = false;
	return HoudiniAssetParameter;
}

TOptional< float >
UHoudiniParameterFloat::GetValue(int32 Idx) const
{
	if (Values.IsValidIndex(Idx))
		return TOptional< float >(Values[Idx]);
	else
		return TOptional< float >();
}

bool
UHoudiniParameterFloat::GetValueAt(const int32& AtIndex, float& OutValue) const
{
	if (!Values.IsValidIndex(AtIndex))
		return false;

	OutValue = Values[AtIndex];

	return true;
}

bool
UHoudiniParameterFloat::SetValueAt(const float& InValue, const int32& AtIndex)
{
	if (!Values.IsValidIndex(AtIndex))
		return false;

	if (InValue == Values[AtIndex])
		return false;

	Values[AtIndex] = FMath::Clamp< float >(InValue, Min, Max);

	return true;
}

void 
UHoudiniParameterFloat::SetDefaultValues() 
{
	if (DefaultValues.Num() > 0)
		return;

	DefaultValues.Empty();
	for (int32 Idx = 0; Idx < Values.Num(); ++Idx) 
	{
		DefaultValues.Add(Values[Idx]);
	}
}

bool 
UHoudiniParameterFloat::IsDefaultValueAtIndex(const int32& Idx) const 
{
	if (!Values.IsValidIndex(Idx) || !DefaultValues.IsValidIndex(Idx))
		return true;

	return Values[Idx] == DefaultValues[Idx];
}

bool
UHoudiniParameterFloat::IsDefault() const 
{
	if (bIsChildOfRamp)
		return true;

	for (int32 Idx = 0; Idx < Values.Num(); ++Idx) 
	{
		if (!IsDefaultValueAtIndex(Idx))
			return false;
	}

	return true;
}

void
UHoudiniParameterFloat::SetValue(float InValue, int32 Idx)
{
	if (!Values.IsValidIndex(Idx))
		return;

	if (InValue == Values[Idx])
		return;

	Values[Idx] = FMath::Clamp< float >(InValue, Min, Max);
}

void 
UHoudiniParameterFloat::RevertToDefault() 
{
	if (!bIsChildOfRamp)
	{
		bPendingRevertToDefault = true;
		TuplePendingRevertToDefault.Empty();
		TuplePendingRevertToDefault.Add(-1);

		MarkChanged(true);
	}
}

void 
UHoudiniParameterFloat::RevertToDefault(const int32& TupleIndex) 
{
	bPendingRevertToDefault = true;
	TuplePendingRevertToDefault.AddUnique(TupleIndex);

	MarkChanged(true);
}
