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

#include "HoudiniParameterMultiParm.h"
#include <HoudiniParameterUpdater.h>

#include "HoudiniCookable.h"

UHoudiniParameterMultiParm::UHoudiniParameterMultiParm(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer), bIsShown(false), InstanceStartOffset(0)
{
	// TODO Proper Init
	ParmType = EHoudiniParameterType::MultiParm;
}

UHoudiniParameterMultiParm *
UHoudiniParameterMultiParm::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterMultiParm_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterMultiParm::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterMultiParm * HoudiniAssetParameter = NewObject< UHoudiniParameterMultiParm >(
		InOuter, UHoudiniParameterMultiParm::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::MultiParm);
	HoudiniAssetParameter->DefaultInstanceCount = -1;

	return HoudiniAssetParameter;
}

bool 
UHoudiniParameterMultiParm::IsDefault() const 
{
	return DefaultInstanceCount == MultiParmInstanceCount;
}

void 
UHoudiniParameterMultiParm::SetDefaultInstanceCount(int32 InCount)
{
	if (DefaultInstanceCount >= 0)
		return;

	DefaultInstanceCount = InCount;
}

void UHoudiniParameterMultiParm::MarkDefault(const bool& bInDefault)
{
	Super::MarkDefault(bInDefault);
}

bool UHoudiniParameterMultiParm::CanModifyMultiParm() const
{
	return true;
#if 0
	UHoudiniCookable* HC = GetCookable();
	auto State = HC->GetCurrentState();

	if(State == EHoudiniAssetState::NeedInstantiation || State == EHoudiniAssetState::NewHDA)
		return false;
	else if (HC->GetNodeId() == INDEX_NONE)
		return false;
	else
		return true;
#endif
}

