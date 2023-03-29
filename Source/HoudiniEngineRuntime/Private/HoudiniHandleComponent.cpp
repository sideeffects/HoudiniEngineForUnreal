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

#include "HoudiniHandleComponent.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniParameter.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniRuntimeSettings.h"

#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniCompatibilityHelpers.h"

#include "Runtime/Launch/Resources/Version.h"

#include "UObject/DevObjectVersion.h"
#if ENGINE_MINOR_VERSION >= 2
	#include "UObject/Linker.h"
#endif

#include "Serialization/CustomVersion.h"

void
UHoudiniHandleComponent::Serialize(FArchive& Ar)
{
	int64 InitialOffset = Ar.Tell();
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	bool bLegacyComponent = false;
	if (Ar.IsLoading())
	{
		int32 Ver = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE && Ver >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
		{
			bLegacyComponent = true;
		}
	}

	if (bLegacyComponent)
	{
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		bool bEnableBackwardCompatibility = HoudiniRuntimeSettings->bEnableBackwardCompatibility;

		if (bEnableBackwardCompatibility)
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniHandleComponent : converting v1 object to v2."));

			Super::Serialize(Ar);

			UHoudiniHandleComponent_V1* CompatibilityHC = NewObject<UHoudiniHandleComponent_V1>();
			CompatibilityHC->Serialize(Ar);
			CompatibilityHC->UpdateFromLegacyData(this);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniHandleComponent : serialized data will be skipped."));

			Super::Serialize(Ar);

			// Skip v1 Serialized data
			if (FLinker* Linker = Ar.GetLinker())
			{
				int32 const ExportIndex = this->GetLinkerIndex();
				FObjectExport& Export = Linker->ExportMap[ExportIndex];
				Ar.Seek(InitialOffset + Export.SerialSize);
				return;
			}
		}
	}
	else
	{
		// Normal v2 serialization
		Super::Serialize(Ar);
	}
}

UHoudiniHandleParameter::UHoudiniHandleParameter(const FObjectInitializer & ObjectInitializer) 
	:Super(ObjectInitializer)
{};

UHoudiniHandleComponent::UHoudiniHandleComponent(const FObjectInitializer & ObjectInitializer)
	:Super(ObjectInitializer) 
{};


bool 
UHoudiniHandleParameter::Bind(float & OutValue, const char * CmpName, int32 InTupleIdx,
			const FString & HandleParmName, UHoudiniParameter* Parameter) 
{
	if (!Parameter)
		return false;

	if (HandleParmName != CmpName)
		return false;

	UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(Parameter);

	if (!FloatParameter)
		return false;

	AssetParameter = Parameter;

	if (FloatParameter) 
	{
		// It is possible that the handle param is bound to a single tuple param.
		// Ignore the preset tuple index if that's the case or we'll crash.
		if (Parameter->GetTupleSize() <= InTupleIdx)
			InTupleIdx = 0;

		auto Optional = FloatParameter->GetValue(InTupleIdx);
		if (Optional.IsSet())
		{
			TupleIndex = InTupleIdx;
			OutValue = Optional.GetValue();
			return true;
		}
	}

	return false;
}

bool 
UHoudiniHandleParameter::Bind(TSharedPtr<FString> & OutValue, const char * CmpName,
			int32 InTupleIdx, const FString & HandleParmName, UHoudiniParameter* Parameter) 
{
	if (!Parameter)
		return false;

	if (HandleParmName != CmpName)
		return false;

	UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(Parameter);

	if (!ChoiceParameter)
		return false;

	AssetParameter = Parameter;

	if (ChoiceParameter)
	{
		// It is possible that the handle param is bound to a single tuple param.
		// Ignore the preset tuple index if that's the case or we'll crash.
		if (Parameter->GetTupleSize() <= InTupleIdx)
			InTupleIdx = 0;

		auto Optional = ChoiceParameter->GetValue(InTupleIdx);
		if (Optional.IsSet())
		{
			TupleIndex = InTupleIdx;
			OutValue = Optional.GetValue();
			return true;
		}
	}

	return false;
}

TSharedPtr<FString> 
UHoudiniHandleParameter::Get(TSharedPtr<FString> DefaultValue) const 
{
	UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(AssetParameter);
	if (ChoiceParameter)
	{
		auto Optional = ChoiceParameter->GetValue(TupleIndex);
		if (Optional.IsSet())
			return Optional.GetValue();
	}

	return DefaultValue;
}

UHoudiniHandleParameter & 
UHoudiniHandleParameter::operator=(float Value) 
{
	UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(AssetParameter);
	if (FloatParameter)
	{
		FloatParameter->SetValue(Value, TupleIndex);
		FloatParameter->MarkChanged(true);
	}

	return *this;
}

void 
UHoudiniHandleComponent::InitializeHandleParameters() 
{
	if (XformParms.Num() < int32(EXformParameter::COUNT)) 
	{
		XformParms.Empty();
		for (int32 n = 0; n < int32(EXformParameter::COUNT); ++n)
		{
			UHoudiniHandleParameter* XformHandle = NewObject<UHoudiniHandleParameter>(this, UHoudiniHandleParameter::StaticClass());
			XformParms.Add(XformHandle);
		}
	}

	if (!RSTParm) 
	{
		RSTParm = NewObject<UHoudiniHandleParameter>(this, UHoudiniHandleParameter::StaticClass());
	}

	if (!RotOrderParm) 
	{
		RotOrderParm = NewObject<UHoudiniHandleParameter>(this, UHoudiniHandleParameter::StaticClass());
	}
}

bool 
UHoudiniHandleComponent::CheckHandleValid() const
{
	if (XformParms.Num() < int32(EXformParameter::COUNT))
		return false;

	for (auto& XformParm : XformParms) 
	{
		if (!XformParm)
			return false;
	}

	if (!RSTParm)
		return false;

	if (!RotOrderParm)
		return false;

	return true;
}

FBox
UHoudiniHandleComponent::GetBounds() const 
{
	FBox BoxBounds(ForceInitToZero);
	return BoxBounds + GetComponentLocation();
}

