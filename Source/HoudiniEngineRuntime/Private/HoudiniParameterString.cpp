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

#include "HoudiniParameterString.h"

UHoudiniParameterString::UHoudiniParameterString(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsAssetRef(false)
{
	ParmType = EHoudiniParameterType::String;
}

UHoudiniParameterString *
UHoudiniParameterString::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterString_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterString::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterString * HoudiniAssetParameter = NewObject< UHoudiniParameterString >(
		InOuter, UHoudiniParameterString::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::String);
	//HoudiniAssetParameter->UpdateFromParmInfo(InParentParameter, InNodeId, ParmInfo);

	return HoudiniAssetParameter;
}

bool
UHoudiniParameterString::SetValueAt(const FString& InValue, const uint32& Index)
{
	if (!Values.IsValidIndex(Index))
		return false;

	if (Values[Index].Equals(InValue, ESearchCase::CaseSensitive))
		return false;

	Values[Index] = InValue;

	return true;
}

void
UHoudiniParameterString::SetAssetAt(UObject* InObject, const uint32& Index)
{
	if (!ChosenAssets.IsValidIndex(Index))
		return;

	ChosenAssets[Index] = InObject;

}

FString
UHoudiniParameterString::GetAssetReference(UObject* InObject)
{
	// Get the asset reference string for a given UObject
	if (!IsValid(InObject))
		return FString();

	// Start by getting the Object's full name
	FString AssetReference = InObject->GetFullName();

	// Replace the first space to '\''
	for (int32 Itr = 0; Itr < AssetReference.Len(); Itr++)
	{
		if (AssetReference[Itr] == ' ')
		{
			AssetReference[Itr] = '\'';
			break;
		}
	}

	// Attach another '\'' to the end
	AssetReference += FString("'");

	return AssetReference;
}

void 
UHoudiniParameterString::SetDefaultValues() 
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
UHoudiniParameterString::IsDefaultValueAtIndex(const int32& Idx) const 
{
	if (!Values.IsValidIndex(Idx) || !DefaultValues.IsValidIndex(Idx))
		return true;

	return Values[Idx] == DefaultValues[Idx];
}

bool 
UHoudiniParameterString::IsDefault() const 
{
	for (int32 Idx = 0; Idx < Values.Num(); ++Idx) 
	{
		if (!IsDefaultValueAtIndex(Idx))
			return false;
	}

	return true;
}