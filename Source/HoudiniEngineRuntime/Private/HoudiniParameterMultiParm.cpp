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
	//HoudiniAssetParameter->UpdateFromParmInfo(InParentParameter, InNodeId, ParmInfo);

	HoudiniAssetParameter->DefaultInstanceCount = -1;

	return HoudiniAssetParameter;
}

bool
UHoudiniParameterMultiParm::SetValue(const int32 InValue)
{
	if (InValue == Value)
		return false;
	
	Value = InValue;

	this->MarkChanged(true);
	return true;
}

void
UHoudiniParameterMultiParm::InsertElement(int32 Index) 
{
	this->Modification.Type = EHoudiniMultiParmModificationType::Insert;
	this->Modification.Value = Index;
	this->MarkChanged(true);
}

void 
UHoudiniParameterMultiParm::RemoveElement(int32 Index) 
{
	if(Index < 0)
		return;

	this->Modification.Type = EHoudiniMultiParmModificationType::Removed;
	this->Modification.Value = Index;
	this->MarkChanged(true);
}

bool 
UHoudiniParameterMultiParm::SetNumElements(int Count) 
{
	if(this->GetInstanceCount() == Count)
		return false;

	this->Modification.Type = EHoudiniMultiParmModificationType::Resize;
	this->Modification.Value = Count;
	this->MarkChanged(true);

	return true;
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
	Modification = FHoudiniMultiParmModification();
}
