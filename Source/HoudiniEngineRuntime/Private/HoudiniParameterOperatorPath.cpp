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

#include "HoudiniParameterOperatorPath.h"

#include "HoudiniInput.h"


UHoudiniParameterOperatorPath::UHoudiniParameterOperatorPath(
    const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{

}

UHoudiniParameterOperatorPath *
UHoudiniParameterOperatorPath::Create(UObject *InOuter, const FString &InParamName)
{
    FString ParamNameStr = "HoudiniParameterMultiParm_" + InParamName;
    FName ParamName = MakeUniqueObjectName(
        InOuter, UHoudiniParameterOperatorPath::StaticClass(), *ParamNameStr);

    // We need to create a new parameter
    UHoudiniParameterOperatorPath *HoudiniAssetParameter =
        NewObject<UHoudiniParameterOperatorPath>(
            InOuter, UHoudiniParameterOperatorPath::StaticClass(), ParamName,
            RF_Public | RF_Transactional);

    HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::Input);


    return HoudiniAssetParameter;
}

bool UHoudiniParameterOperatorPath::HasChanged() const
{
	if (Super::HasChanged())
		return true;
	if (HoudiniInput.IsValid() && HoudiniInput->HasChanged())
		return true;
	return false;
}

bool UHoudiniParameterOperatorPath::NeedsToTriggerUpdate() const
{
	if (Super::NeedsToTriggerUpdate())
		return true;
	if (HoudiniInput.IsValid() && HoudiniInput->NeedsToTriggerUpdate())
		return true;
	return false;
}

void UHoudiniParameterOperatorPath::MarkChanged(const bool & bInChanged)
{
	Super::MarkChanged(bInChanged);
}

void
UHoudiniParameterOperatorPath::RemapInputs(const TMap<UHoudiniInput*, UHoudiniInput*>& InputMapping)
{
	if (!HoudiniInput.IsValid())
		return;

	if (InputMapping.Contains(HoudiniInput.Get()))
	{
		HoudiniInput = InputMapping.FindRef(HoudiniInput.Get());
	}
	else
	{
		HoudiniInput = nullptr;
	}
}
