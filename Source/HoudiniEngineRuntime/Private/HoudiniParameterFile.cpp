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

#include "HoudiniParameterFile.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"

#include "Misc/Paths.h"

UHoudiniParameterFile::UHoudiniParameterFile(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Filters()
	, bIsReadOnly(false)
{
	ParmType = EHoudiniParameterType::File;
}

UHoudiniParameterFile *
UHoudiniParameterFile::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterFile_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterFile::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterFile * HoudiniAssetParameter = NewObject< UHoudiniParameterFile >(
		InOuter, UHoudiniParameterFile::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::File);

	return HoudiniAssetParameter;
}


bool
UHoudiniParameterFile::SetValueAt(const FString& InValue, const uint32& Index)
{
	if (!Values.IsValidIndex(Index))
		return false;

	if (Values[Index] == InValue)
		return false;

	Values[Index] = InValue;

	return true;
}

bool 
UHoudiniParameterFile::IsDefault() const 
{
	for (int32 Idx = 0; Idx < Values.Num(); ++Idx) 
	{
		if (!DefaultValues.IsValidIndex(Idx))
			break;

		if (Values[Idx] != DefaultValues[Idx])
			return false;
	}

	return true;
}

void 
UHoudiniParameterFile::SetDefaultValues()
{
	if (DefaultValues.Num() > 0)
		return;

	DefaultValues.Empty();
	for (int32 Idx = 0; Idx < Values.Num(); ++Idx)
	{
		DefaultValues.Add(Values[Idx]);
	}
}

