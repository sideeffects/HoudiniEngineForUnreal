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

#include "HoudiniParameterInt.h"

UHoudiniParameterInt::UHoudiniParameterInt(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Unit()
	, bHasMin(false)
	, bHasMax(false)
	, bHasUIMin(false)
	, bHasUIMax(false)
	, bIsLogarithmic(false)
	, Min(0)
	, Max(0)
	, UIMin(0)
	, UIMax(0)
{
	ParmType = EHoudiniParameterType::Int;
}

UHoudiniParameterInt *
UHoudiniParameterInt::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterInt_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterInt::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterInt * HoudiniAssetParameter = NewObject< UHoudiniParameterInt >(
		InOuter, UHoudiniParameterInt::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::Int);
	HoudiniAssetParameter->bIsLogarithmic = false;
	//HoudiniAssetParameter->UpdateFromParmInfo(InParentParameter, InNodeId, ParmInfo);

	return HoudiniAssetParameter;
}

TOptional<int32>
UHoudiniParameterInt::GetValue(int32 Idx) const
{
	if (Values.IsValidIndex(Idx))
		return TOptional<int32>(Values[Idx]);
	else
		return TOptional<int32>();
}

bool
UHoudiniParameterInt::GetValueAt(const int32& AtIndex, int32& OutValue) const
{
	if (!Values.IsValidIndex(AtIndex))
		return false;

	OutValue = Values[AtIndex];

	return true;
}

bool
UHoudiniParameterInt::SetValueAt(const int32& InValue, const int32& AtIndex)
{
	if (!Values.IsValidIndex(AtIndex))
		return false;

	if (InValue == Values[AtIndex])
		return false;

	Values[AtIndex] = FMath::Clamp< int32 >(InValue, Min, Max);

	return true;
}

void 
UHoudiniParameterInt::SetDefaultValues() 
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
UHoudiniParameterInt::IsDefaultValueAtIndex(const int32& Idx) const 
{
	if (!Values.IsValidIndex(Idx) || !DefaultValues.IsValidIndex(Idx))
		return true;

	return Values[Idx] == DefaultValues[Idx];
}

bool 
UHoudiniParameterInt::IsDefault() const 
{
	for (int32 Idx = 0; Idx < Values.Num(); ++Idx) 
	{
		if (!IsDefaultValueAtIndex(Idx))
			return false;
	}

	return true;
}