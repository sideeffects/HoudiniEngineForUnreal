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

#include "HoudiniParameterButtonStrip.h"

UHoudiniParameterButtonStrip::UHoudiniParameterButtonStrip(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParmType = EHoudiniParameterType::ButtonStrip;
}

UHoudiniParameterButtonStrip * 
UHoudiniParameterButtonStrip::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterButtonStrip_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterButtonStrip::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterButtonStrip * HoudiniAssetParameter = NewObject< UHoudiniParameterButtonStrip >(
		InOuter, UHoudiniParameterButtonStrip::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::ButtonStrip);

	return HoudiniAssetParameter;
}

bool UHoudiniParameterButtonStrip::SetValueAt(const bool InValue, const uint32 Index)
{
	if (!Labels.IsValidIndex(Index))
	{
		return false;
	}

	if (InValue == GetValueAt(Index))
	{
		return false;
	}

	Value ^= (1U << Index);

	return true;
}
