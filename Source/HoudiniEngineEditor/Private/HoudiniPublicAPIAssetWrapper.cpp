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


#include "HoudiniPublicAPIAssetWrapper.h"

#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOutputDetails.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterFolder.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniPDGManager.h"
#include "HoudiniPublicAPI.h"
#include "HoudiniPublicAPIBlueprintLib.h"
#include "HoudiniPublicAPIInputTypes.h"

FHoudiniPublicAPIRampPoint::FHoudiniPublicAPIRampPoint()
	: Position(0)
	, Interpolation(EHoudiniPublicAPIRampInterpolationType::LINEAR)
{
}

FHoudiniPublicAPIRampPoint::FHoudiniPublicAPIRampPoint(
		const float InPosition,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation)
	: Position(InPosition)
	, Interpolation(InInterpolation)
{
	
}

FHoudiniPublicAPIFloatRampPoint::FHoudiniPublicAPIFloatRampPoint()
	: Value(0)
{
}

FHoudiniPublicAPIFloatRampPoint::FHoudiniPublicAPIFloatRampPoint(
		const float InPosition,
		const float InValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation)
	: FHoudiniPublicAPIRampPoint(InPosition, InInterpolation)
	, Value(InValue)
{
	
}

FHoudiniPublicAPIColorRampPoint::FHoudiniPublicAPIColorRampPoint()
	: Value(FLinearColor::Black)
{
}

FHoudiniPublicAPIColorRampPoint::FHoudiniPublicAPIColorRampPoint(
		const float InPosition,
		const FLinearColor& InValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation)
	: FHoudiniPublicAPIRampPoint(InPosition, InInterpolation)
	, Value(InValue)
{
	
}

FHoudiniParameterTuple::FHoudiniParameterTuple()
	: BoolValues()
	, Int32Values()
	, FloatValues()
	, StringValues()
{
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const bool& InValue)
	: FHoudiniParameterTuple()
{
	BoolValues.Add(InValue);
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const TArray<bool>& InValues)
	: BoolValues(InValues)
{
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const int32& InValue)
	: FHoudiniParameterTuple()
{
	Int32Values.Add(InValue);
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const TArray<int32>& InValues)
	: Int32Values(InValues)
{
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const float& InValue)
	: FHoudiniParameterTuple()
{
	FloatValues.Add(InValue);
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const TArray<float>& InValues)
	: FloatValues(InValues)
{
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const FString& InValue)
	: FHoudiniParameterTuple()
{
	StringValues.Add(InValue);
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const TArray<FString>& InValues)
	: StringValues(InValues)
{
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const TArray<FHoudiniPublicAPIFloatRampPoint>& InRampPoints)
	: FloatRampPoints(InRampPoints)
{
}

FHoudiniParameterTuple::FHoudiniParameterTuple(const TArray<FHoudiniPublicAPIColorRampPoint>& InRampPoints)
	: ColorRampPoints(InRampPoints)
{
}

//
// UHoudiniPublicAPIAssetWrapper
//


UHoudiniPublicAPIAssetWrapper::UHoudiniPublicAPIAssetWrapper()
	: HoudiniAssetObject(nullptr)
	, bAssetLinkSetupAttemptComplete(false)
{
	
}

UHoudiniPublicAPIAssetWrapper*
UHoudiniPublicAPIAssetWrapper::CreateWrapper(UObject* InOuter, UObject* InHoudiniAssetActorOrComponent)
{
	if (!IsValid(InHoudiniAssetActorOrComponent))
		return nullptr;

	// Check if InHoudiniAssetActorOrComponent is supported
	if (!CanWrapHoudiniObject(InHoudiniAssetActorOrComponent))
		return nullptr;

	UHoudiniPublicAPIAssetWrapper* NewWrapper = CreateEmptyWrapper(InOuter);
	if (!IsValid(NewWrapper))
		return nullptr;

	// If we cannot wrap the specified actor, return nullptr.
	if (!NewWrapper->WrapHoudiniAssetObject(InHoudiniAssetActorOrComponent))
	{
		NewWrapper->MarkAsGarbage();
		return nullptr;
	}
	
	return NewWrapper;
}

UHoudiniPublicAPIAssetWrapper*
UHoudiniPublicAPIAssetWrapper::CreateEmptyWrapper(UObject* InOuter)
{
	UObject* const Outer = InOuter ? InOuter : GetTransientPackage();
	UClass* const Class = StaticClass();
	UHoudiniPublicAPIAssetWrapper* NewWrapper = NewObject<UHoudiniPublicAPIAssetWrapper>(
		Outer, Class,
		MakeUniqueObjectName(Outer, Class));
	if (!IsValid(NewWrapper))
		return nullptr;

	return NewWrapper;
}

bool
UHoudiniPublicAPIAssetWrapper::CanWrapHoudiniObject(UObject* InObject)
{
	if (!IsValid(InObject))
		return false;

	return InObject->IsA<AHoudiniAssetActor>() || InObject->IsA<UHoudiniAssetComponent>();
}

bool 
UHoudiniPublicAPIAssetWrapper::GetTemporaryCookFolder_Implementation(FDirectoryPath& OutDirectoryPath) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	OutDirectoryPath = HAC->TemporaryCookFolder;
	return true;
}

bool 
UHoudiniPublicAPIAssetWrapper::SetTemporaryCookFolder_Implementation(const FDirectoryPath& InDirectoryPath) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	if (HAC->TemporaryCookFolder.Path != InDirectoryPath.Path)
		HAC->TemporaryCookFolder = InDirectoryPath;
	return true;
}

bool 
UHoudiniPublicAPIAssetWrapper::GetBakeFolder_Implementation(FDirectoryPath& OutDirectoryPath) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	OutDirectoryPath = HAC->BakeFolder;
	return true;
}

bool 
UHoudiniPublicAPIAssetWrapper::SetBakeFolder_Implementation(const FDirectoryPath& InDirectoryPath) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	if (HAC->BakeFolder.Path != InDirectoryPath.Path)
		HAC->BakeFolder = InDirectoryPath;
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::BakeAllOutputs_Implementation()
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
		HAC,
		HAC->bReplacePreviousBake,
		HAC->HoudiniEngineBakeOption,
		HAC->bRemoveOutputAfterBake,
		HAC->bRecenterBakedActors);
}

bool
UHoudiniPublicAPIAssetWrapper::BakeAllOutputsWithSettings_Implementation(
	EHoudiniEngineBakeOption InBakeOption,
	bool bInReplacePreviousBake,
	bool bInRemoveTempOutputsOnSuccess,
	bool bInRecenterBakedActors)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(HAC, bInReplacePreviousBake, InBakeOption, bInRemoveTempOutputsOnSuccess, bInRecenterBakedActors);
}

bool
UHoudiniPublicAPIAssetWrapper::SetAutoBakeEnabled_Implementation(const bool bInAutoBakeEnabled)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->SetBakeAfterNextCookEnabled(bInAutoBakeEnabled);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::IsAutoBakeEnabled_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return HAC->IsBakeAfterNextCookEnabled();
}

bool
UHoudiniPublicAPIAssetWrapper::SetBakeMethod_Implementation(const EHoudiniEngineBakeOption InBakeMethod)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->HoudiniEngineBakeOption = InBakeMethod;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetBakeMethod_Implementation(EHoudiniEngineBakeOption& OutBakeMethod)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	OutBakeMethod = HAC->HoudiniEngineBakeOption;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetRemoveOutputAfterBake_Implementation(const bool bInRemoveOutputAfterBake)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->bRemoveOutputAfterBake = bInRemoveOutputAfterBake;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetRemoveOutputAfterBake_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return HAC->bRemoveOutputAfterBake;
}

bool
UHoudiniPublicAPIAssetWrapper::SetRecenterBakedActors_Implementation(const bool bInRecenterBakedActors)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->bRecenterBakedActors = bInRecenterBakedActors;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetRecenterBakedActors_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return HAC->bRecenterBakedActors;
}

bool
UHoudiniPublicAPIAssetWrapper::SetReplacePreviousBake_Implementation(const bool bInReplacePreviousBake)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->bReplacePreviousBake = bInReplacePreviousBake;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetReplacePreviousBake_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return HAC->bReplacePreviousBake;
}

bool
UHoudiniPublicAPIAssetWrapper::GetValidHoudiniAssetActorWithError(AHoudiniAssetActor*& OutActor) const
{
	AHoudiniAssetActor* const Actor = GetHoudiniAssetActor();
	if (!IsValid(Actor))
	{
		SetErrorMessage(
			TEXT("Could not find a valid AHoudiniAssetActor for the wrapped asset, or no asset has been wrapped."));
		return false;
	}

	OutActor = Actor;
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetValidHoudiniAssetComponentWithError(UHoudiniAssetComponent*& OutHAC) const
{
	UHoudiniAssetComponent* const HAC = GetHoudiniAssetComponent();
	if (!IsValid(HAC))
	{
		SetErrorMessage(
			TEXT("Could not find a valid HoudiniAssetComponent for the wrapped asset, or no asset has been wrapped."));
		return false;
	}

	OutHAC = HAC;
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetValidOutputAtWithError(const int32 InOutputIndex, UHoudiniOutput*& OutOutput) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	// Check if InOutputIndex is a valid/in-range index
	const int32 NumOutputs = HAC->GetNumOutputs();
	if (InOutputIndex < 0 || InOutputIndex >= NumOutputs)
	{
		SetErrorMessage(FString::Printf(
			TEXT("Output index %d is out of range [0, %d]"), InOutputIndex, NumOutputs));
		return false;
	}
	
	UHoudiniOutput* const Output= HAC->GetOutputAt(InOutputIndex);
	if (!IsValid(Output))
	{
		SetErrorMessage(FString::Printf(TEXT("Output at index %d is invalid."), InOutputIndex));
		return false;
	}

	OutOutput = Output;
	return true;
}


UHoudiniPDGAssetLink*
UHoudiniPublicAPIAssetWrapper::GetHoudiniPDGAssetLink() const
{
	UHoudiniAssetComponent* const HAC = GetHoudiniAssetComponent();
	if (!IsValid(HAC))
		return nullptr;

	return HAC->GetPDGAssetLink();
}

bool
UHoudiniPublicAPIAssetWrapper::GetValidHoudiniPDGAssetLinkWithError(UHoudiniPDGAssetLink*& OutAssetLink) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	UHoudiniPDGAssetLink* const AssetLink = HAC->GetPDGAssetLink();
	if (!IsValid(AssetLink))
	{
		SetErrorMessage(
			TEXT("Could not find a valid HoudiniPDGAssetLink for the wrapped asset. Does it contain a TOP network?"));
		return false;
	}

	OutAssetLink = AssetLink;
	return true;
}

void
UHoudiniPublicAPIAssetWrapper::ClearHoudiniAssetObject_Implementation()
{
	UHoudiniPDGAssetLink* const AssetLink = GetHoudiniPDGAssetLink();
	if (IsValid(AssetLink))
	{
		if (OnPDGPostTOPNetworkCookDelegateHandle.IsValid())
			AssetLink->GetOnPostTOPNetworkCookDelegate().Remove(OnPDGPostTOPNetworkCookDelegateHandle);
		if (OnPDGPostBakeDelegateHandle.IsValid())
			AssetLink->GetOnPostBakeDelegate().Remove(OnPDGPostBakeDelegateHandle);
	}

	bAssetLinkSetupAttemptComplete = false;
	
	FHoudiniEngineCommands::GetOnHoudiniProxyMeshesRefinedDelegate().Remove(OnHoudiniProxyMeshesRefinedDelegateHandle);

	UHoudiniAssetComponent* const HAC = GetHoudiniAssetComponent();
	if (IsValid(HAC))
	{
		if (OnAssetStateChangeDelegateHandle.IsValid())
			HAC->GetOnAssetStateChangeDelegate().Remove(OnAssetStateChangeDelegateHandle);
		if (OnPostCookDelegateHandle.IsValid())
			HAC->GetOnPostCookDelegate().Remove(OnPostCookDelegateHandle);
		if (OnPostBakeDelegateHandle.IsValid())
			HAC->GetOnPostBakeDelegate().Remove(OnPostBakeDelegateHandle);
	}
	
	OnPDGPostTOPNetworkCookDelegateHandle.Reset();
	OnPDGPostBakeDelegateHandle.Reset();
	OnAssetStateChangeDelegateHandle.Reset();
	OnPostCookDelegateHandle.Reset();
	OnPostBakeDelegateHandle.Reset();
	OnHoudiniProxyMeshesRefinedDelegateHandle.Reset();

	HoudiniAssetObject = nullptr;
	CachedHoudiniAssetActor = nullptr;
	CachedHoudiniAssetComponent = nullptr;
}

bool
UHoudiniPublicAPIAssetWrapper::WrapHoudiniAssetObject_Implementation(UObject* InHoudiniAssetObjectToWrap)
{
	// If InHoudiniAssetObjectToWrap is null, just unwrap any currently wrapped asset
	if (!InHoudiniAssetObjectToWrap)
	{
		ClearHoudiniAssetObject();
		return true;
	}
	
	// Check if InHoudiniAssetObjectToWrap is supported
	if (!CanWrapHoudiniObject(InHoudiniAssetObjectToWrap))
	{
		UClass* const ObjectClass = IsValid(InHoudiniAssetObjectToWrap) ? InHoudiniAssetObjectToWrap->GetClass() : nullptr; 
		SetErrorMessage(FString::Printf(
			TEXT("Cannot wrap objects of class '%s'."), ObjectClass ? *(ObjectClass->GetName()) : TEXT("Unknown")));
		return false;
	}

	// First unwrap/unbind if we are currently wrapping an instantiated asset
	ClearHoudiniAssetObject();

	HoudiniAssetObject = InHoudiniAssetObjectToWrap;

	// Cache the HoudiniAssetActor and HoudiniAssetComponent
	if (HoudiniAssetObject->IsA<AHoudiniAssetActor>())
	{
		CachedHoudiniAssetActor = Cast<AHoudiniAssetActor>(InHoudiniAssetObjectToWrap);
		CachedHoudiniAssetComponent = CachedHoudiniAssetActor->HoudiniAssetComponent;
	}
	else if (HoudiniAssetObject->IsA<UHoudiniAssetComponent>())
	{
		CachedHoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InHoudiniAssetObjectToWrap);
		CachedHoudiniAssetActor = Cast<AHoudiniAssetActor>(CachedHoudiniAssetComponent->GetOwner());
	}

	UHoudiniAssetComponent* const HAC = GetHoudiniAssetComponent();
	if (IsValid(HAC))
	{
		// Bind to HandleOnHoudiniAssetStateChange from the HAC: we also implement IHoudiniAssetStateEvents, and
		// in the default implementation HandleOnHoudiniAssetStateChange will call the appropriate Handle functions
		// for PostInstantiate, PostCook etc
		OnAssetStateChangeDelegateHandle = HAC->GetOnAssetStateChangeDelegate().AddUFunction(this, TEXT("HandleOnHoudiniAssetComponentStateChange"));
		OnPostCookDelegateHandle = HAC->GetOnPostCookDelegate().AddUFunction(this, TEXT("HandleOnHoudiniAssetComponentPostCook"));
		OnPostBakeDelegateHandle = HAC->GetOnPostBakeDelegate().AddUFunction(this, TEXT("HandleOnHoudiniAssetComponentPostBake"));
	}

	OnHoudiniProxyMeshesRefinedDelegateHandle = FHoudiniEngineCommands::GetOnHoudiniProxyMeshesRefinedDelegate().AddUFunction(this, TEXT("HandleOnHoudiniProxyMeshesRefinedGlobal"));

	// PDG asset link bindings: We attempt to bind to PDG here, but it likely is not available yet.
	// We have to wait until post instantiation in order to know if there is a PDG asset link
	// for this HDA. This is checked again in HandleOnHoudiniAssetComponentStateChange and sets
	// bAssetLinkSetupAttemptComplete.
	BindToPDGAssetLink();

	return true;
}

AHoudiniAssetActor* 
UHoudiniPublicAPIAssetWrapper::GetHoudiniAssetActor_Implementation() const
{ 
	return CachedHoudiniAssetActor.Get();
}

UHoudiniAssetComponent* 
UHoudiniPublicAPIAssetWrapper::GetHoudiniAssetComponent_Implementation() const
{
	return CachedHoudiniAssetComponent.Get();
}

bool
UHoudiniPublicAPIAssetWrapper::DeleteInstantiatedAsset_Implementation()
{
	AHoudiniAssetActor* AssetActor = nullptr;
	if (!GetValidHoudiniAssetActorWithError(AssetActor))
		return false;

	// Unbind / unwrap the HDA actor
	ClearHoudiniAssetObject();
	AssetActor->Destroy();

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::Rebuild_Implementation()
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->MarkAsNeedRebuild();

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::Recook_Implementation()
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	HAC->MarkAsNeedCook();
	
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetAutoCookingEnabled_Implementation(const bool bInSetEnabled)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	if (HAC->IsCookingEnabled() == bInSetEnabled)
		return false;

	HAC->SetCookingEnabled(bInSetEnabled);
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::IsAutoCookingEnabled_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return HAC->IsCookingEnabled();
}

bool
UHoudiniPublicAPIAssetWrapper::SetFloatParameterValue_Implementation(FName InParameterTupleName, float InValue, int32 InAtIndex, bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is a float
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	bool bDidChangeValue = false;
	if (ParamType == EHoudiniParameterType::Float)
	{
		UHoudiniParameterFloat* FloatParam = Cast<UHoudiniParameterFloat>(Param);
		if (!IsValid(FloatParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= FloatParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, FloatParam->GetNumberOfValues()));
			return false;
		}

		bDidChangeValue = FloatParam->SetValueAt(InValue, InAtIndex);
	}
	else if (ParamType == EHoudiniParameterType::Color)
	{
		UHoudiniParameterColor* ColorParam = Cast<UHoudiniParameterColor>(Param);
		if (!IsValid(ColorParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		static const int32 NumColorChannels = 4;
		if (InAtIndex >= NumColorChannels)
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, NumColorChannels));
			return false;
		}

		FLinearColor CurrentColorValue = ColorParam->GetColorValue(); 
		if (CurrentColorValue.Component(InAtIndex) != InValue)
		{
			CurrentColorValue.Component(InAtIndex) = InValue;
			ColorParam->SetColorValue(CurrentColorValue);
			bDidChangeValue = true;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is of a type that cannot be set via SetFloatParamterValue."), *InParameterTupleName.ToString()));
		return false;
	}

	if (bDidChangeValue && bInMarkChanged)
		Param->MarkChanged(true);
	
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetFloatParameterValue_Implementation(FName InParameterTupleName, float& OutValue, int32 InAtIndex) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is a float
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::Float)
	{
		UHoudiniParameterFloat* FloatParam = Cast<UHoudiniParameterFloat>(Param);
		if (!IsValid(FloatParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= FloatParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, FloatParam->GetNumberOfValues()));
			return false;
		}

		return FloatParam->GetValueAt(InAtIndex, OutValue);
	}
	else if (ParamType == EHoudiniParameterType::Color)
	{
		UHoudiniParameterColor* ColorParam = Cast<UHoudiniParameterColor>(Param);
		if (!IsValid(ColorParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		static const int32 NumColorChannels = 4;
		if (InAtIndex >= NumColorChannels)
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, NumColorChannels));
			return false;
		}

		OutValue = ColorParam->GetColorValue().Component(InAtIndex);
		return true;
	}

	return false;
}

bool
UHoudiniPublicAPIAssetWrapper::SetColorParameterValue_Implementation(FName InParameterTupleName, const FLinearColor& InValue, bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is a float
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	bool bDidChangeValue = false;
	if (ParamType == EHoudiniParameterType::Color)
	{
		UHoudiniParameterColor* ColorParam = Cast<UHoudiniParameterColor>(Param);
		if (!IsValid(ColorParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		bDidChangeValue = ColorParam->SetColorValue(InValue);
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is of a type that cannot be set via SetColorParamterValue."), *InParameterTupleName.ToString()));
		return false;
	}

	if (bDidChangeValue && bInMarkChanged)
		Param->MarkChanged(true);
	
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetColorParameterValue_Implementation(FName InParameterTupleName, FLinearColor& OutValue) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is a float
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::Color)
	{
		UHoudiniParameterColor* ColorParam = Cast<UHoudiniParameterColor>(Param);
		if (!IsValid(ColorParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = ColorParam->GetColorValue();
		return true;
	}

	return false;
}

bool
UHoudiniPublicAPIAssetWrapper::SetIntParameterValue_Implementation(FName InParameterTupleName, int32 InValue, int32 InAtIndex, bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	bool bDidChangeValue = false;
	if (ParamType == EHoudiniParameterType::Int)
	{
		UHoudiniParameterInt* IntParam = Cast<UHoudiniParameterInt>(Param);
		if (!IsValid(IntParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= IntParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, IntParam->GetNumberOfValues()));
			return false;
		}

		bDidChangeValue = IntParam->SetValueAt(InValue, InAtIndex);
	}
	else if (ParamType == EHoudiniParameterType::IntChoice)
	{
		UHoudiniParameterChoice* ChoiceParam = Cast<UHoudiniParameterChoice>(Param);
		if (!IsValid(ChoiceParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		bDidChangeValue = ChoiceParam->SetIntValue(InValue);
	}
	else if (ParamType == EHoudiniParameterType::MultiParm)
	{
		UHoudiniParameterMultiParm* MultiParam = Cast<UHoudiniParameterMultiParm>(Param);
		if (!IsValid(MultiParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		bDidChangeValue = MultiParam->SetNumElements(InValue);
	}
	else if (ParamType == EHoudiniParameterType::Toggle)
	{
		UHoudiniParameterToggle* ToggleParam = Cast<UHoudiniParameterToggle>(Param);
		if (!IsValid(ToggleParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		bDidChangeValue = ToggleParam->SetValueAt(InValue != 0, InAtIndex);
	}
	else if (ParamType == EHoudiniParameterType::Folder)
	{
		UHoudiniParameterFolder* FolderParam = Cast<UHoudiniParameterFolder>(Param);
		if (!IsValid(FolderParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		const bool NewValue = InValue != 0;
		if (FolderParam->IsChosen() != NewValue)
		{
			FolderParam->SetChosen(NewValue);
			bDidChangeValue = true;
		}
	}
	else if (ParamType == EHoudiniParameterType::FloatRamp || ParamType == EHoudiniParameterType::ColorRamp)
	{
		// For ramps we have to use the appropriate function so that delete/insert operations are managed correctly
		bDidChangeValue = SetRampParameterNumPoints(InParameterTupleName, InValue);
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is of a type that cannot be set via SetIntParameterValue."), *InParameterTupleName.ToString()));
		return false;
	}

	if (bDidChangeValue && bInMarkChanged)
		Param->MarkChanged(true);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetIntParameterValue_Implementation(FName InParameterTupleName, int32& OutValue, int32 InAtIndex) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::Int)
	{
		UHoudiniParameterInt* IntParam = Cast<UHoudiniParameterInt>(Param);
		if (!IsValid(IntParam))
			return false;

		if (InAtIndex >= IntParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, IntParam->GetNumberOfValues()));
			return false;
		}

		return IntParam->GetValueAt(InAtIndex, OutValue);
	}
	else if (ParamType == EHoudiniParameterType::IntChoice)
	{
		UHoudiniParameterChoice* ChoiceParam = Cast<UHoudiniParameterChoice>(Param);
		if (!IsValid(ChoiceParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = ChoiceParam->GetIntValue(ChoiceParam->GetIntValueIndex());
		return true;
	}
	else if (ParamType == EHoudiniParameterType::MultiParm)
	{
		UHoudiniParameterMultiParm* MultiParam = Cast<UHoudiniParameterMultiParm>(Param);
		if (!IsValid(MultiParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = MultiParam->GetNextInstanceCount();
		return true;
	}
	else if (ParamType == EHoudiniParameterType::Toggle)
	{
		UHoudiniParameterToggle* ToggleParam = Cast<UHoudiniParameterToggle>(Param);
		if (!IsValid(ToggleParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = ToggleParam->GetValueAt(InAtIndex);
		return true;
	}
	else if (ParamType == EHoudiniParameterType::Folder)
	{
		UHoudiniParameterFolder* FolderParam = Cast<UHoudiniParameterFolder>(Param);
		if (!IsValid(FolderParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = FolderParam->IsChosen();
		return true;
	}

	return false;
}

bool
UHoudiniPublicAPIAssetWrapper::SetBoolParameterValue_Implementation(FName InParameterTupleName, bool InValue, int32 InAtIndex, bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	bool bDidChangeValue = false;
	if (ParamType == EHoudiniParameterType::Toggle)
	{
		UHoudiniParameterToggle* ToggleParam = Cast<UHoudiniParameterToggle>(Param);
		if (!IsValid(ToggleParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		bDidChangeValue = ToggleParam->SetValueAt(InValue, InAtIndex);
	}
	else if (ParamType == EHoudiniParameterType::Folder)
	{
		UHoudiniParameterFolder* FolderParam = Cast<UHoudiniParameterFolder>(Param);
		if (!IsValid(FolderParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (FolderParam->IsChosen() != InValue)
		{
			FolderParam->SetChosen(InValue);
			bDidChangeValue = true;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is of a type that cannot be set via SetBoolParameterValue."), *InParameterTupleName.ToString()));
		return false;
	}

	if (bDidChangeValue && bInMarkChanged)
		Param->MarkChanged(true);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetBoolParameterValue_Implementation(FName InParameterTupleName, bool& OutValue, int32 InAtIndex) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::Toggle)
	{
		UHoudiniParameterToggle* ToggleParam = Cast<UHoudiniParameterToggle>(Param);
		if (!IsValid(ToggleParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = ToggleParam->GetValueAt(InAtIndex);
		return true;
	}
	else if (ParamType == EHoudiniParameterType::Folder)
	{
		UHoudiniParameterFolder* FolderParam = Cast<UHoudiniParameterFolder>(Param);
		if (!IsValid(FolderParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = FolderParam->IsChosen();
		return true;
	}

	return false;
}

bool
UHoudiniPublicAPIAssetWrapper::SetStringParameterValue_Implementation(FName InParameterTupleName, const FString& InValue, int32 InAtIndex, bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	bool bDidChangeValue = false;
	if (ParamType == EHoudiniParameterType::String || ParamType == EHoudiniParameterType::StringAssetRef)
	{
		UHoudiniParameterString* StringParam = Cast<UHoudiniParameterString>(Param);
		if (!IsValid(StringParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= StringParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, StringParam->GetNumberOfValues()));
			return false;
		}

		// We have to handle asset references differently
		if (ParamType == EHoudiniParameterType::StringAssetRef)
		{
			// Find/load the asset, make sure it is a valid reference/object
			const FSoftObjectPath AssetRef(InValue);
			UObject* const Asset = AssetRef.TryLoad();
			if (IsValid(Asset))
			{
				UObject* const CurrentAsset = StringParam->GetAssetAt(InAtIndex);
				if (CurrentAsset != Asset)
				{
					StringParam->SetAssetAt(Asset, InAtIndex);
					bDidChangeValue = true;
				}
			}
			else
			{
				SetErrorMessage(FString::Printf(
					TEXT("Asset reference '%s' is invalid. Not setting parameter value."), *InValue));
				return false;
			}
		}
		else
		{
			bDidChangeValue = StringParam->SetValueAt(InValue, InAtIndex);
		}
	}
	else if (ParamType == EHoudiniParameterType::StringChoice)
	{
		UHoudiniParameterChoice* ChoiceParam = Cast<UHoudiniParameterChoice>(Param);
		if (!IsValid(ChoiceParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		bDidChangeValue = ChoiceParam->SetStringValue(InValue);
	}
	else if (ParamType == EHoudiniParameterType::File || ParamType == EHoudiniParameterType::FileDir ||
		ParamType == EHoudiniParameterType::FileGeo || ParamType == EHoudiniParameterType::FileImage)
	{
		UHoudiniParameterFile* FileParam = Cast<UHoudiniParameterFile>(Param);
		if (!IsValid(FileParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= FileParam->GetNumValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, FileParam->GetNumValues()));
			return false;
		}
		
		bDidChangeValue = FileParam->SetValueAt(InValue, InAtIndex);
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is of a type that cannot be set via SetStringParameterValue."), *InParameterTupleName.ToString()));
		return false;
	}

	if (bDidChangeValue && bInMarkChanged)
		Param->MarkChanged(true);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetStringParameterValue_Implementation(FName InParameterTupleName, FString& OutValue, int32 InAtIndex) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::String || ParamType == EHoudiniParameterType::StringAssetRef)
	{
		UHoudiniParameterString* StringParam = Cast<UHoudiniParameterString>(Param);
		if (!IsValid(StringParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= StringParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, StringParam->GetNumberOfValues()));
			return false;
		}

		// For asset references: get the asset, and then get the string reference from it and return that.
		// If the asset is null, return the empty string.
		if (ParamType == EHoudiniParameterType::StringAssetRef)
		{
			UObject* const Asset = StringParam->GetAssetAt(InAtIndex);
			if (IsValid(Asset))
			{
				OutValue = UHoudiniParameterString::GetAssetReference(Asset);
			}
			else
			{
				OutValue.Empty();
			}
		}
		else
		{
			OutValue = StringParam->GetValueAt(InAtIndex);
		}
		return true;
	}
	else if (ParamType == EHoudiniParameterType::StringChoice)
	{
		UHoudiniParameterChoice* ChoiceParam = Cast<UHoudiniParameterChoice>(Param);
		if (!IsValid(ChoiceParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		OutValue = ChoiceParam->GetStringValue();
		return true;
	}
	else if (ParamType == EHoudiniParameterType::File || ParamType == EHoudiniParameterType::FileDir ||
		ParamType == EHoudiniParameterType::FileGeo || ParamType == EHoudiniParameterType::FileImage)
	{
		UHoudiniParameterFile* FileParam = Cast<UHoudiniParameterFile>(Param);
		if (!IsValid(FileParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= FileParam->GetNumValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, FileParam->GetNumValues()));
			return false;
		}
		
		OutValue = FileParam->GetValueAt(InAtIndex);
		return true;
	}

	return false;
}

bool
UHoudiniPublicAPIAssetWrapper::SetAssetRefParameterValue_Implementation(FName InParameterTupleName, UObject* InValue, int32 InAtIndex, bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	bool bDidChangeValue = false;
	if (ParamType == EHoudiniParameterType::StringAssetRef)
	{
		UHoudiniParameterString* StringParam = Cast<UHoudiniParameterString>(Param);
		if (!IsValid(StringParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= StringParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, StringParam->GetNumberOfValues()));
			return false;
		}

		// Find/load the asset, make sure it is a valid reference/object
		UObject* const CurrentAsset = StringParam->GetAssetAt(InAtIndex);
		if (CurrentAsset != InValue)
		{
			StringParam->SetAssetAt(InValue, InAtIndex);
			bDidChangeValue = true;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is of a type that cannot be set via SetAssetRefParamter."), *InParameterTupleName.ToString()));
		return false;
	}

	if (bDidChangeValue && bInMarkChanged)
		Param->MarkChanged(true);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetAssetRefParameterValue_Implementation(FName InParameterTupleName, UObject*& OutValue, int32 InAtIndex) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::StringAssetRef)
	{
		UHoudiniParameterString* StringParam = Cast<UHoudiniParameterString>(Param);
		if (!IsValid(StringParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		if (InAtIndex >= StringParam->GetNumberOfValues())
		{
			SetErrorMessage(FString::Printf(
				TEXT("Parameter tuple index %d is out of range (tuple size == %d)."), InAtIndex, StringParam->GetNumberOfValues()));
			return false;
		}

		OutValue = StringParam->GetAssetAt(InAtIndex);
		return true;
	}

	return false;
}

bool
UHoudiniPublicAPIAssetWrapper::SetRampParameterNumPoints_Implementation(FName InParameterTupleName, const int32 InNumPoints) const
{
	if (InNumPoints < 1)
	{
		SetErrorMessage(TEXT("InNumPoints must be >= 1."));
		return false;
	}
	
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// If the parameter is not set to auto update, or if cooking is paused, we have to set the cached points and
	// not the main points.
	// const bool bCookingEnabled = !FHoudiniEngineCommands::IsAssetCookingPaused();
	// const bool bUseCachedPoints = (!Param->IsAutoUpdate() || !bCookingEnabled);
	const bool bUseCachedPoints = !Param->IsAutoUpdate();
	
	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	UHoudiniParameterRampColor* ColorRampParam = nullptr;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(Param);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(Param);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter '%s' is not a float or color ramp parameter."), *(InParameterTupleName.ToString())));
		return false;
	}

	if (bUseCachedPoints)
	{
		// When using the cached points we only have to resize the array
		if (FloatRampParam)
		{
			const int32 CurrentNumPoints = FloatRampParam->CachedPoints.Num();
			if (CurrentNumPoints != InNumPoints)
			{
				FloatRampParam->CachedPoints.SetNum(InNumPoints);
				// FloatRampParam->MarkChanged(true);
			}
		}
		else
		{
			const int32 CurrentNumPoints = ColorRampParam->CachedPoints.Num();
			if (CurrentNumPoints != InNumPoints)
			{
				ColorRampParam->CachedPoints.SetNum(InNumPoints);
				// ColorRampParam->MarkChanged(true);
			}
		}

		// Update the ramp's widget if it is currently visible/selected
		const bool bForceFullUpdate = true;
		FHoudiniEngineUtils::UpdateEditorProperties(Param, bForceFullUpdate);
	}
	else
	{
		int32 NumPendingInsertOperations = 0;
		int32 NumPendingDeleteOperations = 0;
		TSet<int32> InstanceIndexesPendingDelete;
		TArray<UHoudiniParameterRampModificationEvent*>& ModificationEvents = FloatRampParam ? FloatRampParam->ModificationEvents : ColorRampParam->ModificationEvents;
		for (UHoudiniParameterRampModificationEvent const* const Event : ModificationEvents)
		{
			if (!IsValid(Event))
				continue;

			if (Event->IsInsertEvent())
				NumPendingInsertOperations++;
			else if (Event->IsDeleteEvent())
			{
				InstanceIndexesPendingDelete.Add(Event->DeleteInstanceIndex);
				NumPendingDeleteOperations++;
			}
		}

		const int32 PointsArraySize = FloatRampParam ? FloatRampParam->Points.Num() : ColorRampParam->Points.Num(); 
		int32 CurrentNumPoints = PointsArraySize + NumPendingInsertOperations - NumPendingDeleteOperations;

		if (InNumPoints < CurrentNumPoints)
		{
			// When deleting points, first remove pending insert operations from the end
			if (NumPendingInsertOperations > 0)
			{
				const int32 NumEvents = ModificationEvents.Num();
				TArray<UHoudiniParameterRampModificationEvent*> TempModificationArray;
				TempModificationArray.Reserve(NumEvents);

				for (int32 Index = NumEvents - 1; Index >= 0; --Index)
				{
					UHoudiniParameterRampModificationEvent* const Event = ModificationEvents[Index];
					if (InNumPoints < CurrentNumPoints && IsValid(Event) && Event->IsInsertEvent())
					{
						CurrentNumPoints--;
						NumPendingInsertOperations--;
						continue;
					}

					TempModificationArray.Add(Event);
				}

				Algo::Reverse(TempModificationArray);
				ModificationEvents = MoveTemp(TempModificationArray);
			}

			// If we still have points to delete...
			if (InNumPoints < CurrentNumPoints)
			{
				// Deleting points, add delete operations, deleting from the end of Points (points that are not yet
				// pending delete)
				for (int32 Index = PointsArraySize - 1; (InNumPoints < CurrentNumPoints && Index >= 0); --Index)
				{
					int32 InstanceIndex = INDEX_NONE;

					if (FloatRampParam)
					{
						UHoudiniParameterRampFloatPoint const* const PointData = FloatRampParam->Points[Index];
						if (!IsValid(PointData))
							continue;

						InstanceIndex = PointData->InstanceIndex;
					}
					else
					{
						UHoudiniParameterRampColorPoint const* const PointData = ColorRampParam->Points[Index];
						if (!IsValid(PointData))
							continue;

						InstanceIndex = PointData->InstanceIndex;
					}

					if (!InstanceIndexesPendingDelete.Contains(InstanceIndex))
					{
						InstanceIndexesPendingDelete.Add(InstanceIndex);
						CurrentNumPoints--;
						
						UHoudiniParameterRampModificationEvent* DeleteEvent = NewObject<UHoudiniParameterRampModificationEvent>(
							FloatRampParam, UHoudiniParameterRampModificationEvent::StaticClass());

						if (DeleteEvent)
						{
							if (FloatRampParam)
							{
								DeleteEvent->SetFloatRampEvent();
							}
							else
							{
								DeleteEvent->SetColorRampEvent();
							}
							DeleteEvent->SetDeleteEvent();
							DeleteEvent->DeleteInstanceIndex = InstanceIndex;

							ModificationEvents.Add(DeleteEvent);
						}
					}
				}
			}

			Param->MarkChanged(true);
		}
		else if (InNumPoints > CurrentNumPoints)
		{
			// Adding points, add insert operations
			while (InNumPoints > CurrentNumPoints)
			{
				CurrentNumPoints++;
				UHoudiniParameterRampModificationEvent* InsertEvent = NewObject<UHoudiniParameterRampModificationEvent>(
					Param, UHoudiniParameterRampModificationEvent::StaticClass());

				if (InsertEvent)
				{
					if (FloatRampParam)
					{
						InsertEvent->SetFloatRampEvent();
					}
					else
					{
						InsertEvent->SetColorRampEvent();
					}
					InsertEvent->SetInsertEvent();
					// Leave point position, value and interpolation at default

					ModificationEvents.Add(InsertEvent);
				}
			}
			
			Param->MarkChanged(true);
		}

		// If at this point InNumPoints != CurrentNumPoints then something went wrong, we couldn't delete all the
		// desired points
		if (InNumPoints != CurrentNumPoints)
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected error: could not delete the required number of ramp points "
					 "(target # points = %d; have # points %d)."), InNumPoints, CurrentNumPoints));
			return false;
		}
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetRampParameterNumPoints_Implementation(FName InParameterTupleName, int32& OutNumPoints) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	// If the parameter is not set to auto update, or if cooking is paused, we have to set the cached points and
	// not the main points.
	// const bool bCookingEnabled = !FHoudiniEngineCommands::IsAssetCookingPaused();
	// const bool bUseCachedPoints = (!Param->IsAutoUpdate() || !bCookingEnabled);
	const bool bUseCachedPoints = !Param->IsAutoUpdate();
	
	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	UHoudiniParameterRampColor* ColorRampParam = nullptr;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(Param);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(Param);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter '%s' is not a float or color ramp parameter."), *(InParameterTupleName.ToString())));
		return false;
	}

	if (bUseCachedPoints)
	{
		// When using the cached points we only have to resize the array
		if (FloatRampParam)
		{
			OutNumPoints = FloatRampParam->CachedPoints.Num();
		}
		else
		{
			OutNumPoints = ColorRampParam->CachedPoints.Num();
		}
	}
	else
	{
		int32 NumPendingInsertOperations = 0;
		int32 NumPendingDeleteOperations = 0;
		TArray<UHoudiniParameterRampModificationEvent*>& ModificationEvents = FloatRampParam ? FloatRampParam->ModificationEvents : ColorRampParam->ModificationEvents;
		for (UHoudiniParameterRampModificationEvent const* const Event : ModificationEvents)
		{
			if (!IsValid(Event))
				continue;

			if (Event->IsInsertEvent())
				NumPendingInsertOperations++;
			else if (Event->IsDeleteEvent())
				NumPendingDeleteOperations++;
		}

		const int32 PointsArraySize = FloatRampParam ? FloatRampParam->Points.Num() : ColorRampParam->Points.Num(); 
		OutNumPoints = PointsArraySize + NumPendingInsertOperations - NumPendingDeleteOperations;
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetFloatRampParameterPointValue_Implementation(
	FName InParameterTupleName,
	const int32 InPointIndex,
	const float InPointPosition,
	const float InPointValue,
	const EHoudiniPublicAPIRampInterpolationType InInterpolationType,
	const bool bInMarkChanged)
{
	return SetRampParameterPointValue(
		InParameterTupleName, InPointIndex, InPointPosition, InPointValue, FLinearColor::Black, InInterpolationType, bInMarkChanged);
}

bool
UHoudiniPublicAPIAssetWrapper::GetFloatRampParameterPointValue_Implementation(
	FName InParameterTupleName,
	const int32 InPointIndex,
	float& OutPointPosition,
	float& OutPointValue,
	EHoudiniPublicAPIRampInterpolationType& OutInterpolationType) const
{
	FLinearColor ColorValue;
	return GetRampParameterPointValue(
		InParameterTupleName, InPointIndex, OutPointPosition, OutPointValue, ColorValue, OutInterpolationType);
}

bool
UHoudiniPublicAPIAssetWrapper::SetFloatRampParameterPoints_Implementation(
	FName InParameterTupleName,
	const TArray<FHoudiniPublicAPIFloatRampPoint>& InRampPoints,
	const bool bInMarkChanged)
{
	const int32 TargetNumPoints = InRampPoints.Num();
	if (TargetNumPoints == 0)
	{
		SetErrorMessage(TEXT("InRampPoints must have at least one entry."));
		return false;
	}
	
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(Param);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(TEXT("Parameter '%s' is not a float ramp or color ramp parameter."), *(Param->GetName())));
		return false;
	}

	// If the parameter is not set to auto update, or if cooking is paused, we have to set the cached points and
	// not the main points.
	// const bool bCookingEnabled = !FHoudiniEngineCommands::IsAssetCookingPaused();
	// const bool bUseCachedPoints = (!Param->IsAutoUpdate() || !bCookingEnabled);
	const bool bUseCachedPoints = !Param->IsAutoUpdate();

	// Set the ramp point count to match the size of InRampPoints
	if (!SetRampParameterNumPoints(InParameterTupleName, TargetNumPoints))
		return false;
	
	// Get all ramp points (INDEX_NONE == get all points)
	TArray<TPair<UObject*, bool>> RampPointData;
	if (!FindRampPointData(Param, INDEX_NONE, RampPointData) || RampPointData.Num() <= 0)
		return false;

	// Check that we fetched the correct number of point data objects
	if (RampPointData.Num() != TargetNumPoints)
	{
		SetErrorMessage(FString::Printf(TEXT("Failed to set the number of ramp points to %d."), TargetNumPoints));
		return false;
	}

	for (int32 Index = 0; Index < TargetNumPoints; ++Index)
	{
		TPair<UObject*, bool> const& Entry = RampPointData[Index];
		UObject* const PointData = Entry.Key;
		const bool bIsPointData = Entry.Value;
		if (!IsValid(PointData))
			return false;

		const FHoudiniPublicAPIFloatRampPoint& NewRampPoint = InRampPoints[Index];
		const EHoudiniRampInterpolationType NewInterpolation = UHoudiniPublicAPI::ToHoudiniRampInterpolationType(
			NewRampPoint.Interpolation); 
		
		if (bIsPointData)
		{
			UHoudiniParameterRampFloatPoint* FloatPointData = Cast<UHoudiniParameterRampFloatPoint>(PointData);
			if (!IsValid(FloatPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampFloatPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
			
			if (bUseCachedPoints)
			{
				// When setting the cached points, we set the values directly instead of using the setters, but we set
				// the bCaching flag on the parameter and mark the position/value/interpolation parent parameters as changed
				if (FloatPointData->Position != NewRampPoint.Position)
				{
					FloatPointData->Position = NewRampPoint.Position;
					FloatRampParam->bCaching = true;
				}

				if (FloatPointData->Value != NewRampPoint.Value)
				{
					FloatPointData->Value = NewRampPoint.Value;
					FloatRampParam->bCaching = true;
				}

				if (FloatPointData->Interpolation != NewInterpolation)
				{
					FloatPointData->Interpolation = NewInterpolation;
					FloatRampParam->bCaching = true;
				}
			}
			else
			{
				// When setting the main points, we set the values using the setters on the point data but still manually
				// mark the position/value/interpolation parent parameters as changed
				if (FloatPointData->Position != NewRampPoint.Position && FloatPointData->PositionParentParm)
				{
					FloatPointData->SetPosition(NewRampPoint.Position);
					if (bInMarkChanged)
						FloatPointData->PositionParentParm->MarkChanged(bInMarkChanged);
				}

				if (FloatPointData->Value != NewRampPoint.Value && FloatPointData->ValueParentParm)
				{
					FloatPointData->SetValue(NewRampPoint.Value);
					if (bInMarkChanged)
						FloatPointData->ValueParentParm->MarkChanged(bInMarkChanged);
				}

				if (FloatPointData->Interpolation != NewInterpolation && FloatPointData->InterpolationParentParm)
				{
					FloatPointData->SetInterpolation(NewInterpolation);
					if (bInMarkChanged)
						FloatPointData->InterpolationParentParm->MarkChanged(bInMarkChanged);
				}
			}
		}
		else
		{
			UHoudiniParameterRampModificationEvent* const Event = Cast<UHoudiniParameterRampModificationEvent>(PointData);
			if (!IsValid(Event))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampModificationEvent instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}

			Event->InsertPosition = NewRampPoint.Position;
			Event->InsertFloat = NewRampPoint.Value;
			Event->InsertInterpolation = NewInterpolation;
		}
	}

	if (bUseCachedPoints)
	{
		// Update the ramp's widget if it is currently visible/selected
		const bool bForceFullUpdate = true;
		FHoudiniEngineUtils::UpdateEditorProperties(Param, bForceFullUpdate);
	}
	
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetFloatRampParameterPoints_Implementation(
	FName InParameterTupleName,
	TArray<FHoudiniPublicAPIFloatRampPoint>& OutRampPoints) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(Param);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(TEXT("Parameter '%s' is not a float ramp parameter."), *(Param->GetName())));
		return false;
	}

	// Get all ramp points (INDEX_NONE == get all points)
	TArray<TPair<UObject*, bool>> RampPointData;
	if (!FindRampPointData(Param, INDEX_NONE, RampPointData) || RampPointData.Num() <= 0)
		return false;

	OutRampPoints.Reserve(RampPointData.Num());
	const bool bAllowShrinking = false;
	OutRampPoints.SetNum(0, bAllowShrinking);
	for (TPair<UObject*, bool> const& Entry : RampPointData)
	{
		UObject* const PointData = Entry.Key;
		const bool bIsPointData = Entry.Value;
		if (!IsValid(PointData))
			return false;

		FHoudiniPublicAPIFloatRampPoint TempPointData;
		
		if (bIsPointData)
		{
			UHoudiniParameterRampFloatPoint* const FloatPointData = Cast<UHoudiniParameterRampFloatPoint>(PointData);
			if (!IsValid(FloatPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampFloatPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
			
			TempPointData.Position = FloatPointData->Position;
			TempPointData.Value = FloatPointData->Value;
			TempPointData.Interpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(
				FloatPointData->Interpolation);
		}
		else
		{
			UHoudiniParameterRampModificationEvent* const Event = Cast<UHoudiniParameterRampModificationEvent>(PointData);
			if (!IsValid(Event))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampModificationEvent instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}

			TempPointData.Position = Event->InsertPosition;
			TempPointData.Value = Event->InsertFloat;
			TempPointData.Interpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(Event->InsertInterpolation);
		}

		OutRampPoints.Add(TempPointData);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetColorRampParameterPointValue_Implementation(
	FName InParameterTupleName,
	const int32 InPointIndex,
	const float InPointPosition,
	const FLinearColor& InPointValue,
	const EHoudiniPublicAPIRampInterpolationType InInterpolationType,
	const bool bInMarkChanged)
{
	const float FloatValue = 0;
	return SetRampParameterPointValue(
		InParameterTupleName, InPointIndex, InPointPosition, FloatValue, InPointValue, InInterpolationType, bInMarkChanged);
}

bool
UHoudiniPublicAPIAssetWrapper::GetColorRampParameterPointValue_Implementation(
	FName InParameterTupleName,
	const int32 InPointIndex,
	float& OutPointPosition,
	FLinearColor& OutPointValue,
	EHoudiniPublicAPIRampInterpolationType& OutInterpolationType) const
{
	float FloatValue = 0;
	return GetRampParameterPointValue(
		InParameterTupleName, InPointIndex, OutPointPosition, FloatValue, OutPointValue, OutInterpolationType);
}

bool
UHoudiniPublicAPIAssetWrapper::SetColorRampParameterPoints_Implementation(
	FName InParameterTupleName,
	const TArray<FHoudiniPublicAPIColorRampPoint>& InRampPoints,
	const bool bInMarkChanged)
{
	const int32 TargetNumPoints = InRampPoints.Num();
	if (TargetNumPoints == 0)
	{
		SetErrorMessage(TEXT("InRampPoints must have at least one entry."));
		return false;
	}
	
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	UHoudiniParameterRampColor* ColorRampParam = nullptr;
	
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(Param);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(TEXT("Parameter '%s' is not a color ramp parameter."), *(Param->GetName())));
		return false;
	}

	// If the parameter is not set to auto update, or if cooking is paused, we have to set the cached points and
	// not the main points.
	// const bool bCookingEnabled = !FHoudiniEngineCommands::IsAssetCookingPaused();
	// const bool bUseCachedPoints = (!Param->IsAutoUpdate() || !bCookingEnabled);
	const bool bUseCachedPoints = !Param->IsAutoUpdate();

	// Set the ramp point count to match the size of InRampPoints
	if (!SetRampParameterNumPoints(InParameterTupleName, TargetNumPoints))
		return false;
	
	// Get all ramp points (INDEX_NONE == get all points)
	TArray<TPair<UObject*, bool>> RampPointData;
	if (!FindRampPointData(Param, INDEX_NONE, RampPointData) || RampPointData.Num() <= 0)
		return false;

	// Check that we fetched the correct number of point data objects
	if (RampPointData.Num() != TargetNumPoints)
	{
		SetErrorMessage(FString::Printf(TEXT("Failed to set the number of ramp points to %d."), TargetNumPoints));
		return false;
	}

	for (int32 Index = 0; Index < TargetNumPoints; ++Index)
	{
		TPair<UObject*, bool> const& Entry = RampPointData[Index];
		UObject* const PointData = Entry.Key;
		const bool bIsPointData = Entry.Value;
		if (!IsValid(PointData))
			return false;

		const FHoudiniPublicAPIColorRampPoint& NewRampPoint = InRampPoints[Index];
		const EHoudiniRampInterpolationType NewInterpolation = UHoudiniPublicAPI::ToHoudiniRampInterpolationType(
			NewRampPoint.Interpolation); 
		
		if (bIsPointData)
		{
			UHoudiniParameterRampColorPoint* ColorPointData = Cast<UHoudiniParameterRampColorPoint>(PointData);
			if (!IsValid(ColorPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampColorPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
			
			if (bUseCachedPoints)
			{
				// When setting the cached points, we set the values directly instead of using the setters, but we set
				// the bCaching flag on the parameter and mark the position/value/interpolation parent parameters as changed
				if (ColorPointData->Position != NewRampPoint.Position)
				{
					ColorPointData->Position = NewRampPoint.Position;
					ColorRampParam->bCaching = true;
				}

				if (ColorPointData->Value != NewRampPoint.Value)
				{
					ColorPointData->Value = NewRampPoint.Value;
					ColorRampParam->bCaching = true;
				}

				if (ColorPointData->Interpolation != NewInterpolation)
				{
					ColorPointData->Interpolation = NewInterpolation;
					ColorRampParam->bCaching = true;
				}
			}
			else
			{
				// When setting the main points, we set the values using the setters on the point data but still manually
				// mark the position/value/interpolation parent parameters as changed
				if (ColorPointData->Position != NewRampPoint.Position && ColorPointData->PositionParentParm)
				{
					ColorPointData->SetPosition(NewRampPoint.Position);
					if (bInMarkChanged)
						ColorPointData->PositionParentParm->MarkChanged(bInMarkChanged);
				}

				if (ColorPointData->Value != NewRampPoint.Value && ColorPointData->ValueParentParm)
				{
					ColorPointData->SetValue(NewRampPoint.Value);
					if (bInMarkChanged)
						ColorPointData->ValueParentParm->MarkChanged(bInMarkChanged);
				}

				if (ColorPointData->Interpolation != NewInterpolation && ColorPointData->InterpolationParentParm)
				{
					ColorPointData->SetInterpolation(NewInterpolation);
					if (bInMarkChanged)
						ColorPointData->InterpolationParentParm->MarkChanged(bInMarkChanged);
				}
			}
		}
		else
		{
			UHoudiniParameterRampModificationEvent* const Event = Cast<UHoudiniParameterRampModificationEvent>(PointData);
			if (!IsValid(Event))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampModificationEvent instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}

			Event->InsertPosition = NewRampPoint.Position;
			Event->InsertColor = NewRampPoint.Value;
			Event->InsertInterpolation = NewInterpolation;
		}
	}

	if (bUseCachedPoints)
	{
		// Update the ramp's widget if it is currently visible/selected
		const bool bForceFullUpdate = true;
		FHoudiniEngineUtils::UpdateEditorProperties(Param, bForceFullUpdate);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetColorRampParameterPoints_Implementation(
	FName InParameterTupleName,
	TArray<FHoudiniPublicAPIColorRampPoint>& OutRampPoints) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	UHoudiniParameterRampColor* ColorRampParam = nullptr;
	
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(Param);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(TEXT("Parameter '%s' is not a float ramp parameter."), *(Param->GetName())));
		return false;
	}

	// Get all ramp points (INDEX_NONE == get all points)
	TArray<TPair<UObject*, bool>> RampPointData;
	if (!FindRampPointData(Param, INDEX_NONE, RampPointData) || RampPointData.Num() <= 0)
		return false;

	OutRampPoints.Reserve(RampPointData.Num());
	const bool bAllowShrinking = false;
	OutRampPoints.SetNum(0, bAllowShrinking);
	for (TPair<UObject*, bool> const& Entry : RampPointData)
	{
		UObject* const PointData = Entry.Key;
		const bool bIsPointData = Entry.Value;
		if (!IsValid(PointData))
			return false;

		FHoudiniPublicAPIColorRampPoint TempPointData;
		
		if (bIsPointData)
		{
			UHoudiniParameterRampColorPoint* const ColorPointData = Cast<UHoudiniParameterRampColorPoint>(PointData);
			if (!IsValid(ColorPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampColorPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
			
			TempPointData.Position = ColorPointData->Position;
			TempPointData.Value = ColorPointData->Value;
			TempPointData.Interpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(
				ColorPointData->Interpolation);
		}
		else
		{
			UHoudiniParameterRampModificationEvent* const Event = Cast<UHoudiniParameterRampModificationEvent>(PointData);
			if (!IsValid(Event))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampModificationEvent instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}

			TempPointData.Position = Event->InsertPosition;
			TempPointData.Value = Event->InsertColor;
			TempPointData.Interpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(Event->InsertInterpolation);
		}

		OutRampPoints.Add(TempPointData);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::TriggerButtonParameter_Implementation(FName InButtonParameterName)
{
	UHoudiniParameter* Param = FindValidParameterByName(InButtonParameterName);
	if (!Param)
		return false;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	// bool bDidTrigger = false;
	if (ParamType == EHoudiniParameterType::Button)
	{
		UHoudiniParameterButton* ButtonParam = Cast<UHoudiniParameterButton>(Param);
		if (!IsValid(ButtonParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}

		// Marking the button as changed will result in it being triggered/clicked via HAPI
		if (!ButtonParam->HasChanged() || !ButtonParam->NeedsToTriggerUpdate())
		{
			ButtonParam->MarkChanged(true);
			// bDidTrigger = true;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter tuple '%s' is not a button."), *InButtonParameterName.ToString()));
		return false;
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetParameterTuples_Implementation(TMap<FName, FHoudiniParameterTuple>& OutParameterTuples) const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	const int32 NumParameters = HAC->GetNumParameters();
	OutParameterTuples.Empty(NumParameters);
	OutParameterTuples.Reserve(NumParameters);
	for (int32 Index = 0; Index < NumParameters; ++Index)
	{
		const UHoudiniParameter* const Param = HAC->GetParameterAt(Index);
		const EHoudiniParameterType ParameterType = Param->GetParameterType();
		const int32 TupleSize = Param->GetTupleSize();
		const FName PTName(Param->GetParameterName());

		FHoudiniParameterTuple ParameterTuple;

		bool bSkipped = false;
		switch (ParameterType)
		{
			case EHoudiniParameterType::Color:
			case EHoudiniParameterType::Float:
			{
				// Output as float
				ParameterTuple.FloatValues.SetNumZeroed(TupleSize);
				for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
				{
					GetFloatParameterValue(PTName, ParameterTuple.FloatValues[TupleIndex], TupleIndex);
				}
				break;
			}
			
			case EHoudiniParameterType::Int:
			case EHoudiniParameterType::IntChoice:
			case EHoudiniParameterType::MultiParm:
			{
				// Output as int
				ParameterTuple.Int32Values.SetNumZeroed(TupleSize);
				for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
				{
					GetIntParameterValue(PTName, ParameterTuple.Int32Values[TupleIndex], TupleIndex);
				}
				break;
			}
			
			case EHoudiniParameterType::String:
			case EHoudiniParameterType::StringChoice:
			case EHoudiniParameterType::StringAssetRef:
			case EHoudiniParameterType::File:
			case EHoudiniParameterType::FileDir:
			case EHoudiniParameterType::FileGeo:
			case EHoudiniParameterType::FileImage:
			{
				// Output as string
				ParameterTuple.StringValues.SetNumZeroed(TupleSize);
				for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
				{
					GetStringParameterValue(PTName, ParameterTuple.StringValues[TupleIndex], TupleIndex);
				}
				break;
			}

			case EHoudiniParameterType::Toggle:
			{
				// Output as bool
				ParameterTuple.BoolValues.SetNumZeroed(TupleSize);
				for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
				{
					GetBoolParameterValue(PTName, ParameterTuple.BoolValues[TupleIndex], TupleIndex);
				}
				break;
			}

			case EHoudiniParameterType::ColorRamp:
			{
				GetColorRampParameterPoints(PTName, ParameterTuple.ColorRampPoints);
				break;
			}
			case EHoudiniParameterType::FloatRamp:
			{
				GetFloatRampParameterPoints(PTName, ParameterTuple.FloatRampPoints);
				break;
			}
			
			case EHoudiniParameterType::Button:
			case EHoudiniParameterType::ButtonStrip:
			case EHoudiniParameterType::Input:
			case EHoudiniParameterType::Invalid:
			case EHoudiniParameterType::Folder:
			case EHoudiniParameterType::FolderList:
			case EHoudiniParameterType::Label:
			case EHoudiniParameterType::Separator:
			default:
				// Skipped
				bSkipped = true;
				break;
		}

		if (!bSkipped)
			OutParameterTuples.Add(PTName, ParameterTuple);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetParameterTuples_Implementation(const TMap<FName, FHoudiniParameterTuple>& InParameterTuples)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	bool bSuccess = true;
	for (const TPair<FName, FHoudiniParameterTuple>& Entry : InParameterTuples)
	{
		const FName& ParameterTupleName = Entry.Key;
		const FHoudiniParameterTuple& ParameterTuple = Entry.Value;
		if (ParameterTuple.BoolValues.Num() > 0)
		{
			// Set as bool
			const int32 TupleSize = ParameterTuple.BoolValues.Num();
			for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
			{
				if (!SetBoolParameterValue(ParameterTupleName, ParameterTuple.BoolValues[TupleIndex], TupleIndex))
				{
					SetErrorMessage(FString::Printf(
						TEXT("SetParameterTuples: Failed to set %s as a bool at tuple index %d."), *ParameterTupleName.ToString(), TupleIndex));
					bSuccess = false;
					break;
				}
			}
		}
		else if (ParameterTuple.FloatValues.Num() > 0)
		{
			// Set as float
			const int32 TupleSize = ParameterTuple.FloatValues.Num();
			for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
			{
				if (!SetFloatParameterValue(ParameterTupleName, ParameterTuple.FloatValues[TupleIndex], TupleIndex))
				{
					SetErrorMessage(FString::Printf(
						TEXT("SetParameterTuples: Failed to set %s as a float at tuple index %d."),
						*ParameterTupleName.ToString(), TupleIndex));
					bSuccess = false;
					break;
				}
			}
		}
		else if (ParameterTuple.Int32Values.Num() > 0)
		{
			// Set as int
			const int32 TupleSize = ParameterTuple.Int32Values.Num();
			for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
			{
				if (!SetIntParameterValue(ParameterTupleName, ParameterTuple.Int32Values[TupleIndex], TupleIndex))
				{
					SetErrorMessage(FString::Printf(
						TEXT("SetParameterTuples: Failed to set %s as a int at tuple index %d."),
						*ParameterTupleName.ToString(), TupleIndex));
					bSuccess = false;
					break;
				}
			}
		}
		else if (ParameterTuple.StringValues.Num() > 0)
		{
			// Set as string
			const int32 TupleSize = ParameterTuple.StringValues.Num();
			for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
			{
				if (!SetStringParameterValue(ParameterTupleName, ParameterTuple.StringValues[TupleIndex], TupleIndex))
				{
					SetErrorMessage(FString::Printf(
						TEXT("SetParameterTuples: Failed to set %s as a string at tuple index %d."),
						*ParameterTupleName.ToString(), TupleIndex));
					bSuccess = false;
					break;
				}
			}
		}
		else if (ParameterTuple.FloatRampPoints.Num() > 0)
		{
			// Set as a float ramp
			if (!SetFloatRampParameterPoints(ParameterTupleName, ParameterTuple.FloatRampPoints))
				bSuccess = false;
		}
		else if (ParameterTuple.ColorRampPoints.Num() > 0)
		{
			// Set as a color ramp
			if (!SetColorRampParameterPoints(ParameterTupleName, ParameterTuple.ColorRampPoints))
				bSuccess = false;
		}
	}

	return bSuccess;
}

UHoudiniPublicAPIInput*
UHoudiniPublicAPIAssetWrapper::CreateEmptyInput_Implementation(TSubclassOf<UHoudiniPublicAPIInput> InInputClass)
{
	UHoudiniPublicAPI* API = UHoudiniPublicAPIBlueprintLib::GetAPI();
	if (!IsValid(API))
		return nullptr;

	UHoudiniPublicAPIInput* const NewInput = API->CreateEmptyInput(InInputClass, this);
	if (!IsValid(NewInput))
	{
		SetErrorMessage(FString::Printf(
			TEXT("Failed to create a new input of class '%s'."),
			*(IsValid(InInputClass.Get()) ? InInputClass->GetName() : FString())));

		return nullptr;
	}

	return NewInput;
}

int32
UHoudiniPublicAPIAssetWrapper::GetNumNodeInputs_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return -1;

	int32 NumNodeInputs = 0;
	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput const* const Input = HAC->GetInputAt(Index);
		if (!IsValid(Input))
			continue;

		if (!Input->IsObjectPathParameter())
			NumNodeInputs++;
	}

	return NumNodeInputs;
}

bool
UHoudiniPublicAPIAssetWrapper::SetInputAtIndex_Implementation(const int32 InNodeInputIndex, const UHoudiniPublicAPIInput* InInput)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	UHoudiniInput* HoudiniInput = GetHoudiniNodeInputByIndex(InNodeInputIndex);
	if (!IsValid(HoudiniInput))
	{
		SetErrorMessage(FString::Printf(
			TEXT("SetInputAtIndex: Could not find a HoudiniInput for InNodeInputIndex %d. Has the HDA been instantiated?"),
			InNodeInputIndex));
		return false;
	}

	const bool bSuccess = PopulateHoudiniInput(InInput, HoudiniInput);

	// Update the details panel (mostly for when new curves/components are created where visualizers are driven
	// through the details panel)
	FHoudiniEngineEditorUtils::ReselectComponentOwnerIfSelected(HAC);

	return bSuccess;
}

bool
UHoudiniPublicAPIAssetWrapper::GetInputAtIndex_Implementation(const int32 InNodeInputIndex, UHoudiniPublicAPIInput*& OutInput)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	const UHoudiniInput* HoudiniInput = GetHoudiniNodeInputByIndex(InNodeInputIndex);
	if (!IsValid(HoudiniInput))
	{
		SetErrorMessage(FString::Printf(
			TEXT("GetInputAtIndex: Could not find a HoudiniInput for InNodeInputIndex %d. Has the HDA been instantiated?"),
			InNodeInputIndex));
		return false;
	}

	UHoudiniPublicAPIInput* APIInput = nullptr;
	const bool bSuccessfullyCopied = CreateAndPopulateAPIInput(HoudiniInput, APIInput);
	if (!IsValid(APIInput))
		return false;

	OutInput = APIInput;
	return bSuccessfullyCopied;
}

bool
UHoudiniPublicAPIAssetWrapper::SetInputsAtIndices_Implementation(const TMap<int32, UHoudiniPublicAPIInput*>& InInputs)
{
	bool bAnyFailures = false;
	for (const TPair<int32, UHoudiniPublicAPIInput*>& Entry : InInputs)
	{
		if (!SetInputAtIndex(Entry.Key, Entry.Value))
		{
			SetErrorMessage(FString::Printf(
				TEXT("SetInputsAtIndices: Failed to set node input at index %d"), Entry.Key));
			bAnyFailures = true;
		}
	}

	return !bAnyFailures;
}

bool
UHoudiniPublicAPIAssetWrapper::GetInputsAtIndices_Implementation(TMap<int32, UHoudiniPublicAPIInput*>& OutInputs)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	bool bAnyFailures = false;
	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput const* const HoudiniInput = HAC->GetInputAt(Index);
		if (!IsValid(HoudiniInput) || HoudiniInput->IsObjectPathParameter())
			continue;
		
		UHoudiniPublicAPIInput* APIInput = nullptr;
		const bool bSuccessfullyCopied = CreateAndPopulateAPIInput(HoudiniInput, APIInput);
		if (!IsValid(APIInput))
		{
			bAnyFailures = true;
			continue;
		}
		if (!bSuccessfullyCopied)
			bAnyFailures = true;
		
		OutInputs.Add(HoudiniInput->GetInputIndex(), APIInput);
	}

	return !bAnyFailures; 
}

bool
UHoudiniPublicAPIAssetWrapper::SetInputParameter_Implementation(const FName& InParameterName, const UHoudiniPublicAPIInput* InInput)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	UHoudiniInput* HoudiniInput = FindValidHoudiniNodeInputParameter(InParameterName);
	if (!IsValid(HoudiniInput))
	{
		SetErrorMessage(FString::Printf(
			TEXT("SetInputParameter: Could not find a parameter-based HoudiniInput with name %s. Has the HDA been instantiated?"),
			*(InParameterName.ToString())));
		return false;
	}

	const bool bSuccess = PopulateHoudiniInput(InInput, HoudiniInput);

	// Update the details panel (mostly for when new curves/components are created where visualizers are driven
	// through the details panel)
	FHoudiniEngineEditorUtils::ReselectComponentOwnerIfSelected(HAC);

	return bSuccess;
}

bool
UHoudiniPublicAPIAssetWrapper::GetInputParameter_Implementation(const FName& InParameterName, UHoudiniPublicAPIInput*& OutInput)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	const UHoudiniInput* HoudiniInput = FindValidHoudiniNodeInputParameter(InParameterName);
	if (!IsValid(HoudiniInput))
	{
		SetErrorMessage(FString::Printf(
			TEXT("GetInputParameter: Could not find a parameter-based HoudiniInput with name %s. Has the HDA been instantiated?"),
			*(InParameterName.ToString())));
		return false;
	}

	UHoudiniPublicAPIInput* APIInput = nullptr;
	const bool bSuccessfullyCopied = CreateAndPopulateAPIInput(HoudiniInput, APIInput);
	if (!IsValid(APIInput))
		return false;

	OutInput = APIInput;
	return bSuccessfullyCopied;
}

bool
UHoudiniPublicAPIAssetWrapper::SetInputParameters_Implementation(const TMap<FName, UHoudiniPublicAPIInput*>& InInputs)
{
	bool bAnyFailures = false;
	for (const TPair<FName, UHoudiniPublicAPIInput*>& Entry : InInputs)
	{
		if (!SetInputParameter(Entry.Key, Entry.Value))
		{
			SetErrorMessage(FString::Printf(
				TEXT("SetInputParameters: Failed to set input parameter %s"), *(Entry.Key.ToString())));
			bAnyFailures = true;
		}
	}

	return !bAnyFailures;
}

bool
UHoudiniPublicAPIAssetWrapper::GetInputParameters_Implementation(TMap<FName, UHoudiniPublicAPIInput*>& OutInputs)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	bool bAnyFailures = false;
	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput const* const HoudiniInput = HAC->GetInputAt(Index);
		if (!IsValid(HoudiniInput) || !HoudiniInput->IsObjectPathParameter())
			continue;
		
		UHoudiniPublicAPIInput* APIInput = nullptr;
		const bool bSuccessfullyCopied = CreateAndPopulateAPIInput(HoudiniInput, APIInput);
		if (!IsValid(APIInput))
		{
			bAnyFailures = true;
			continue;
		}
		if (!bSuccessfullyCopied)
			bAnyFailures = true;
		
		OutInputs.Add(FName(HoudiniInput->GetName()), APIInput);
	}

	return !bAnyFailures; 
}

int32
UHoudiniPublicAPIAssetWrapper::GetNumOutputs_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return -1;

	return HAC->GetNumOutputs();
}

EHoudiniOutputType
UHoudiniPublicAPIAssetWrapper::GetOutputTypeAt_Implementation(const int32 InIndex) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return EHoudiniOutputType::Invalid;

	return Output->GetType();
}

bool
UHoudiniPublicAPIAssetWrapper::GetOutputIdentifiersAt_Implementation(const int32 InIndex, TArray<FHoudiniPublicAPIOutputObjectIdentifier>& OutIdentifiers) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return false;

	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	OutIdentifiers.Empty();
	OutIdentifiers.Reserve(OutputObjects.Num());
	for (const TPair<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& Entry : OutputObjects)
	{
		OutIdentifiers.Add(FHoudiniPublicAPIOutputObjectIdentifier(Entry.Key));
	}

	return true;
}

UObject*
UHoudiniPublicAPIAssetWrapper::GetOutputObjectAt_Implementation(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return nullptr;

	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniOutputObject const* const OutputObject = OutputObjects.Find(InIdentifier.GetIdentifier());
	if (!OutputObject)
		return nullptr;

	return OutputObject->bProxyIsCurrent ? OutputObject->ProxyObject : OutputObject->OutputObject;
}

UObject*
UHoudiniPublicAPIAssetWrapper::GetOutputComponentAt_Implementation(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return nullptr;

	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniOutputObject const* const OutputObject = OutputObjects.Find(InIdentifier.GetIdentifier());
	if (!OutputObject)
		return nullptr;

	return OutputObject->bProxyIsCurrent ? OutputObject->ProxyComponent : OutputObject->OutputComponent;
}

bool
UHoudiniPublicAPIAssetWrapper::GetOutputBakeNameFallbackAt_Implementation(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier, FString& OutBakeName) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return false;

	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniOutputObject const* const OutputObject =OutputObjects.Find(InIdentifier.GetIdentifier());
	if (!OutputObject)
		return false;

	OutBakeName = OutputObject->BakeName;
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetOutputBakeNameFallbackAt_Implementation(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier, const FString& InBakeName)
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniOutputObject* const OutputObject = OutputObjects.Find(InIdentifier.GetIdentifier());
	if (!OutputObject)
		return false;

	OutputObject->BakeName = InBakeName;
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::BakeOutputObjectAt_Implementation(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier, const FName InBakeName, const EHoudiniLandscapeOutputBakeType InLandscapeBakeType)
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	const FHoudiniOutputObjectIdentifier& Identifier = InIdentifier.GetIdentifier();
	FHoudiniOutputObject* const OutputObject = OutputObjects.Find(Identifier);
	if (!OutputObject)
	{
		SetErrorMessage(FString::Printf(
			TEXT("BakeOutputObjectAt: Could not find an output object using the specified identifier.")));
		return false;
	}

	// Determine the object to bake (this is different depending on landscape, curve or mesh
	const EHoudiniOutputType OutputType = Output->GetType();

	if (OutputType == EHoudiniOutputType::Mesh && OutputObject->bProxyIsCurrent)
	{
		// Output is currently a proxy, this cannot be baked without cooking first.
		SetErrorMessage(FString::Printf(
			TEXT("BakeOutputObjectAt: Object is a proxy mesh, please refine it before baking to CB.")));

		return false;
	}
	
	UObject* ObjectToBake = nullptr;
	switch (OutputType)
	{
		case EHoudiniOutputType::Landscape:
		{
			UHoudiniLandscapePtr* const LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject->OutputObject);
			if (IsValid(LandscapePtr))
			{
				ObjectToBake = LandscapePtr->LandscapeSoftPtr.IsValid() ? LandscapePtr->LandscapeSoftPtr.Get() : nullptr;
			}
			break;
		}
		case EHoudiniOutputType::Curve:
			ObjectToBake = OutputObject->OutputComponent;
			break;
		case EHoudiniOutputType::Mesh:
			ObjectToBake = OutputObject->OutputObject;
			break;
		case EHoudiniOutputType::Instancer:
		case EHoudiniOutputType::Skeletal:
		case EHoudiniOutputType::Invalid:
		default:
			SetErrorMessage(FString::Printf(
				TEXT("BakeOutputObjectAt: unsupported output type (%d) for baking to CB."), OutputType));
			return false;
	}

	if (!IsValid(ObjectToBake))
	{
		SetErrorMessage(FString::Printf(TEXT("BakeOutputObjectAt: Could not find a valid object to bake to CB.")));
		return false;
	}

	// Find the corresponding HGPO in the output
	FHoudiniGeoPartObject HoudiniGeoPartObject;
	for (const auto& HGPO : Output->GetHoudiniGeoPartObjects())
	{
		if (!Identifier.Matches(HGPO))
			continue;

		HoudiniGeoPartObject = HGPO;
		break;
	}

	if (!HoudiniGeoPartObject.IsValid())
	{
		SetErrorMessage(TEXT("BakeOutputObjectAt: Could not find a valid HGPO for the output object. Please recook."));
		return false;
	}

	TArray<UHoudiniOutput*> AllOutputs;
	HAC->GetOutputs(AllOutputs);

	FHoudiniOutputDetails::OnBakeOutputObject(
		InBakeName.IsNone() ? OutputObject->BakeName : InBakeName.ToString(),
		ObjectToBake,
		Identifier,
		*OutputObject,
		HoudiniGeoPartObject,
		HAC,
		HAC->BakeFolder.Path,
		HAC->TemporaryCookFolder.Path,
		OutputType,
		InLandscapeBakeType,
		AllOutputs);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::HasAnyCurrentProxyOutput_Implementation() const
{
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return false;

	return HAC->HasAnyCurrentProxyOutput();
}

bool
UHoudiniPublicAPIAssetWrapper::HasAnyCurrentProxyOutputAt_Implementation(const int32 InIndex) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return false;

	return Output->HasAnyCurrentProxy();
}

bool
UHoudiniPublicAPIAssetWrapper::IsOutputCurrentProxyAt_Implementation(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier) const
{
	UHoudiniOutput* Output = nullptr;
	if (!GetValidOutputAtWithError(InIndex, Output))
		return false;

	return Output->IsProxyCurrent(InIdentifier.GetIdentifier());
}

EHoudiniProxyRefineRequestResult
UHoudiniPublicAPIAssetWrapper::RefineAllCurrentProxyOutputs_Implementation(const bool bInSilent)
{
	AHoudiniAssetActor* AssetActor = nullptr;
	if (!GetValidHoudiniAssetActorWithError(AssetActor))
		return EHoudiniProxyRefineRequestResult::Invalid;

	return FHoudiniEngineCommands::RefineHoudiniProxyMeshActorArrayToStaticMeshes({ AssetActor }, bInSilent);
}

bool
UHoudiniPublicAPIAssetWrapper::HasPDGAssetLink_Implementation() const
{
	return IsValid(GetHoudiniPDGAssetLink());
}

bool
UHoudiniPublicAPIAssetWrapper::GetPDGTOPNetworkPaths_Implementation(TArray<FString>& OutTOPNetworkPaths) const
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	const uint32 NumNetworks = AssetLink->AllTOPNetworks.Num();
	OutTOPNetworkPaths.Empty(NumNetworks);
	for (UTOPNetwork const* const TOPNet : AssetLink->AllTOPNetworks)
	{
		OutTOPNetworkPaths.Add(TOPNet->NodePath);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetPDGTOPNodePaths_Implementation(const FString& InNetworkRelativePath, TArray<FString>& OutTOPNodePaths) const
{
	int32 NetworkIndex = INDEX_NONE;
	UTOPNetwork* TOPNet = nullptr;
	if (!GetValidTOPNetworkByPathWithError(InNetworkRelativePath, NetworkIndex, TOPNet))
		return false;

	const uint32 NumNodes = TOPNet->AllTOPNodes.Num();
	OutTOPNodePaths.Empty(NumNodes);
	for (UTOPNode const* const TOPNode : TOPNet->AllTOPNodes)
	{
		OutTOPNodePaths.Add(TOPNode->NodePath);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::PDGDirtyAllNetworks_Implementation()
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	for (UTOPNetwork* const TOPNetwork : AssetLink->AllTOPNetworks)
	{
		if (!IsValid(TOPNetwork))
			continue;

		FHoudiniPDGManager::DirtyAll(TOPNetwork);
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::PDGDirtyNetwork_Implementation(const FString& InNetworkRelativePath)
{
	int32 NetworkIndex = INDEX_NONE;
	UTOPNetwork* TOPNet = nullptr;
	if (!GetValidTOPNetworkByPathWithError(InNetworkRelativePath, NetworkIndex, TOPNet))
		return false;

	FHoudiniPDGManager::DirtyAll(TOPNet);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::PDGDirtyNode_Implementation(const FString& InNetworkRelativePath, const FString& InNodeRelativePath)
{
	int32 NetworkIndex = INDEX_NONE;
	int32 NodeIndex = INDEX_NONE;
	UTOPNode* TOPNode = nullptr;
	if (!GetValidTOPNodeByPathWithError(InNetworkRelativePath, InNodeRelativePath, NetworkIndex, NodeIndex, TOPNode))
		return false;

	FHoudiniPDGManager::DirtyTOPNode(TOPNode);

	return true;
}

// bool
// UHoudiniPublicAPIAssetWrapper::PDGCookOutputsForAllNetworks_Implementation()
// {
// 	UHoudiniPDGAssetLink* const AssetLink = GetHoudiniPDGAssetLink();
// 	if (!IsValid(AssetLink))
// 		return false;
//
// 	for (UTOPNetwork* const TOPNetwork : AssetLink->AllTOPNetworks)
// 	{
// 		if (!IsValid(TOPNetwork))
// 			continue;
//
// 		FHoudiniPDGManager::CookOutput(TOPNetwork);
// 	}
//
// 	return true;
// }

bool
UHoudiniPublicAPIAssetWrapper::PDGCookOutputsForNetwork_Implementation(const FString& InNetworkRelativePath)
{
	int32 NetworkIndex = INDEX_NONE;
	UTOPNetwork* TOPNet = nullptr;
	if (!GetValidTOPNetworkByPathWithError(InNetworkRelativePath, NetworkIndex, TOPNet))
		return false;

	FHoudiniPDGManager::CookOutput(TOPNet);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::PDGCookNode_Implementation(const FString& InNetworkRelativePath, const FString& InNodeRelativePath)
{
	int32 NetworkIndex = INDEX_NONE;
	int32 NodeIndex = INDEX_NONE;
	UTOPNode* TOPNode = nullptr;
	if (!GetValidTOPNodeByPathWithError(InNetworkRelativePath, InNodeRelativePath, NetworkIndex, NodeIndex, TOPNode))
		return false;

	FHoudiniPDGManager::CookTOPNode(TOPNode);

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::PDGBakeAllOutputs_Implementation()
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;
	
	return PDGBakeAllOutputsWithSettings(
		AssetLink->HoudiniEngineBakeOption,
		AssetLink->PDGBakeSelectionOption,
		AssetLink->PDGBakePackageReplaceMode,
		AssetLink->bRecenterBakedActors);
}

bool
UHoudiniPublicAPIAssetWrapper::PDGBakeAllOutputsWithSettings_Implementation(
	const EHoudiniEngineBakeOption InBakeOption,
	const EPDGBakeSelectionOption InBakeSelection,
	const EPDGBakePackageReplaceModeOption InBakeReplacementMode,
	const bool bInRecenterBakedActors)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	bool bBakeSuccess = false;
	switch (InBakeOption)
	{
		case EHoudiniEngineBakeOption::ToActor:
			bBakeSuccess = FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(
				AssetLink,
				InBakeSelection,
				InBakeReplacementMode,
				bInRecenterBakedActors);
			break;
		case EHoudiniEngineBakeOption::ToBlueprint:
			bBakeSuccess = FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(
				AssetLink,
				InBakeSelection,
				InBakeReplacementMode,
				bInRecenterBakedActors);
			break;
		default:
			bBakeSuccess = false;
			break;
	}

	return bBakeSuccess;
}

bool
UHoudiniPublicAPIAssetWrapper::SetPDGAutoBakeEnabled_Implementation(const bool bInAutoBakeEnabled)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	AssetLink->bBakeAfterAllWorkResultObjectsLoaded = bInAutoBakeEnabled;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::IsPDGAutoBakeEnabled_Implementation() const
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	return AssetLink->bBakeAfterAllWorkResultObjectsLoaded;
}

bool
UHoudiniPublicAPIAssetWrapper::SetPDGBakeMethod_Implementation(const EHoudiniEngineBakeOption InBakeMethod)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	AssetLink->HoudiniEngineBakeOption = InBakeMethod;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetPDGBakeMethod_Implementation(EHoudiniEngineBakeOption& OutBakeMethod)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	OutBakeMethod = AssetLink->HoudiniEngineBakeOption;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetPDGBakeSelection_Implementation(const EPDGBakeSelectionOption InBakeSelection)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	AssetLink->PDGBakeSelectionOption = InBakeSelection;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetPDGBakeSelection_Implementation(EPDGBakeSelectionOption& OutBakeSelection)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	OutBakeSelection = AssetLink->PDGBakeSelectionOption;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::SetPDGRecenterBakedActors_Implementation(const bool bInRecenterBakedActors)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	AssetLink->bRecenterBakedActors = bInRecenterBakedActors;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetPDGRecenterBakedActors_Implementation() const
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	return AssetLink->bRecenterBakedActors;
}

bool
UHoudiniPublicAPIAssetWrapper::SetPDGBakingReplacementMode_Implementation(const EPDGBakePackageReplaceModeOption InBakingReplacementMode)
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	AssetLink->PDGBakePackageReplaceMode = InBakingReplacementMode;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetPDGBakingReplacementMode_Implementation(EPDGBakePackageReplaceModeOption& OutBakingReplacementMode) const
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	OutBakingReplacementMode = AssetLink->PDGBakePackageReplaceMode;

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::BindToPDGAssetLink()
{
	if (bAssetLinkSetupAttemptComplete)
		return true;

	UHoudiniPDGAssetLink* const PDGAssetLink = GetHoudiniPDGAssetLink();
	if (IsValid(PDGAssetLink))
	{
		OnPDGPostTOPNetworkCookDelegateHandle = PDGAssetLink->GetOnPostTOPNetworkCookDelegate().AddUFunction(this, TEXT("HandleOnHoudiniPDGAssetLinkTOPNetPostCook"));
		OnPDGPostBakeDelegateHandle = PDGAssetLink->GetOnPostBakeDelegate().AddUFunction(this, TEXT("HandleOnHoudiniPDGAssetLinkPostBake"));
		bAssetLinkSetupAttemptComplete = true;
		
		return true;
	}

	return false;
}

void
UHoudiniPublicAPIAssetWrapper::HandleOnHoudiniAssetComponentStateChange(UHoudiniAssetComponent* InHAC, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState)
{
	if (!IsValid(InHAC))
		return;
	
	if (InHAC != GetHoudiniAssetComponent())
	{
		SetErrorMessage(FString::Printf(
			TEXT("HandleOnHoudiniAssetComponentStateChange: unexpected InHAC: %s, expected the wrapper's HAC."),
			IsValid(InHAC) ? *InHAC->GetName() : TEXT("")));
		return;
	}

	if (InToState == EHoudiniAssetState::PreInstantiation)
	{
		if (OnPreInstantiationDelegate.IsBound())
			OnPreInstantiationDelegate.Broadcast(this);
	}
	
	if (InFromState == EHoudiniAssetState::Instantiating && InToState == EHoudiniAssetState::PreCook)
	{
		// PDG link setup / bindings: we have to wait until post instantiation to check if we have an asset link and
		// configure bindings
		if (!bAssetLinkSetupAttemptComplete)
		{
			BindToPDGAssetLink();
			bAssetLinkSetupAttemptComplete = true;
		}
		
		if (OnPostInstantiationDelegate.IsBound())
			OnPostInstantiationDelegate.Broadcast(this);
	}
	
	if (InFromState == EHoudiniAssetState::PreProcess)
	{
		if (OnPreProcessStateExitedDelegate.IsBound())
			OnPreProcessStateExitedDelegate.Broadcast(this);
	}
	
	if (InFromState == EHoudiniAssetState::Processing && InToState == EHoudiniAssetState::None)
	{
		if (OnPostProcessingDelegate.IsBound())
			OnPostProcessingDelegate.Broadcast(this);
	}
}

void
UHoudiniPublicAPIAssetWrapper::HandleOnHoudiniAssetComponentPostCook(UHoudiniAssetComponent* InHAC, const bool bInCookSuccess)
{
	if (!IsValid(InHAC))
		return;
	
	if (InHAC != GetHoudiniAssetComponent())
	{
		SetErrorMessage(FString::Printf(
			TEXT("HandleOnHoudiniAssetComponentPostCook: unexpected InHAC: %s, expected the wrapper's HAC."),
			IsValid(InHAC) ? *InHAC->GetName() : TEXT("")));
		return;
	}

	if (OnPostCookDelegate.IsBound())
		OnPostCookDelegate.Broadcast(this, bInCookSuccess);
}

void
UHoudiniPublicAPIAssetWrapper::HandleOnHoudiniAssetComponentPostBake(UHoudiniAssetComponent* InHAC, const bool bInBakeSuccess)
{
	if (!IsValid(InHAC))
		return;
	
	if (InHAC != GetHoudiniAssetComponent())
	{
		SetErrorMessage(FString::Printf(
			TEXT("HandleOnHoudiniAssetComponentPostBake: unexpected InHAC: %s, expected the wrapper's HAC."),
			IsValid(InHAC) ? *InHAC->GetName() : TEXT("")));
		return;
	}

	if (OnPostBakeDelegate.IsBound())
		OnPostBakeDelegate.Broadcast(this, bInBakeSuccess);
}

void
UHoudiniPublicAPIAssetWrapper::HandleOnHoudiniPDGAssetLinkTOPNetPostCook(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNetwork* InTOPNet, const bool bInAnyWorkItemsFailed)
{
	if (!IsValid(InPDGAssetLink))
		return;
	
	if (InPDGAssetLink != GetHoudiniPDGAssetLink())
	{
		SetErrorMessage(FString::Printf(
			TEXT("HandleOnHoudiniPDGAssetLinkTOPNetPostCook: unexpected InPDGAssetLink: %s, expected the wrapper's PDGAssetLink."),
			IsValid(InPDGAssetLink) ? *InPDGAssetLink->GetName() : TEXT("")));
		return;
	}

	if (OnPostPDGTOPNetworkCookDelegate.IsBound())
		OnPostPDGTOPNetworkCookDelegate.Broadcast(this, !bInAnyWorkItemsFailed);
}

void
UHoudiniPublicAPIAssetWrapper::HandleOnHoudiniPDGAssetLinkPostBake(UHoudiniPDGAssetLink* InPDGAssetLink, const bool bInBakeSuccess)
{
	if (!IsValid(InPDGAssetLink))
		return;
	
	if (InPDGAssetLink != GetHoudiniPDGAssetLink())
	{
		SetErrorMessage(FString::Printf(
			TEXT("HandleOnHoudiniPDGAssetLinkPostBake: unexpected InPDGAssetLink: %s, expected the wrapper's PDGAssetLink."),
			IsValid(InPDGAssetLink) ? *InPDGAssetLink->GetName() : TEXT("")));
		return;
	}

	if (OnPostPDGBakeDelegate.IsBound())
		OnPostPDGBakeDelegate.Broadcast(this, bInBakeSuccess);
}

void
UHoudiniPublicAPIAssetWrapper::HandleOnHoudiniProxyMeshesRefinedGlobal(UHoudiniAssetComponent* InHAC, const EHoudiniProxyRefineResult InResult)
{
	if (!IsValid(InHAC))
		return;
	
	if (InHAC != GetHoudiniAssetComponent())
		return;

	if (OnProxyMeshesRefinedDelegate.IsBound())
		OnProxyMeshesRefinedDelegate.Broadcast(this, InResult);
}

UHoudiniParameter*
UHoudiniPublicAPIAssetWrapper::FindValidParameterByName(const FName& InParameterTupleName) const
{
	AActor* const Actor = GetHoudiniAssetActor();
	const FString ActorName = IsValid(Actor) ? Actor->GetName() : FString();
	
	UHoudiniAssetComponent* const HAC = GetHoudiniAssetComponent();
	if (!IsValid(HAC))
	{
		SetErrorMessage(FString::Printf(TEXT("Could not find HAC on Actor '%s'"), *ActorName));
		return nullptr;
	}

	UHoudiniParameter* const Param = HAC->FindParameterByName(InParameterTupleName.ToString());
	if (!IsValid(Param))
	{
		SetErrorMessage(FString::Printf(
			TEXT("Could not find valid parameter tuple '%s' on '%s'."),
			*InParameterTupleName.ToString(), *ActorName));
		return nullptr;
	}

	return Param;
}

bool
UHoudiniPublicAPIAssetWrapper::FindRampPointData(
	UHoudiniParameter* const InParam,
	const int32 InIndex,
	TArray<TPair<UObject*, bool>>& OutPointData) const
{
	if (!IsValid(InParam))
		return false;

	// If the parameter is not set to auto update, or if cooking is paused, we have to set the cached points and
	// not the main points.
	// const bool bCookingEnabled = !FHoudiniEngineCommands::IsAssetCookingPaused();
	// const bool bUseCachedPoints = (!InParam->IsAutoUpdate() || !bCookingEnabled);
	const bool bUseCachedPoints = !InParam->IsAutoUpdate();

	const bool bFetchAllPoints = (InIndex == INDEX_NONE);
	
	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	UHoudiniParameterRampColor* ColorRampParam = nullptr;

	// Handle all the cases where the underlying parameter value is an int or bool
	const EHoudiniParameterType ParamType = InParam->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(InParam);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *InParam->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(InParam);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *InParam->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(
			TEXT("Parameter '%s' is not a float or color ramp parameter."), *(InParam->GetName())));
		return false;
	}

	if (bUseCachedPoints)
	{
		// When using the cached points we only have to resize the array
		if (FloatRampParam)
		{
			if (!bFetchAllPoints && !FloatRampParam->CachedPoints.IsValidIndex(InIndex))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Ramp point index %d is out of range [0, %d]."), InIndex, FloatRampParam->CachedPoints.Num() - 1));
				return false;
			}

			if (bFetchAllPoints)
			{
				// Get all points
				OutPointData.Reserve(FloatRampParam->CachedPoints.Num());
				const bool bAllowShrinking = false;
				OutPointData.SetNum(0, bAllowShrinking);
				for (UHoudiniParameterRampFloatPoint* const RampPoint : FloatRampParam->CachedPoints)
				{
					const bool bIsPointData = true;
					OutPointData.Add(TPair<UObject*, bool>(RampPoint, bIsPointData));
				}
			}
			else
			{
				OutPointData.Reserve(1);
				const bool bAllowShrinking = false;
				OutPointData.SetNum(0, bAllowShrinking);
				
				const bool bIsPointData = true;
				OutPointData.Add(TPair<UObject*, bool>(FloatRampParam->CachedPoints[InIndex], bIsPointData));
			}
			
			return true;
		}
		else
		{
			if (!bFetchAllPoints && !ColorRampParam->CachedPoints.IsValidIndex(InIndex))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Ramp point index %d is out of range [0, %d]."), InIndex, ColorRampParam->CachedPoints.Num() - 1));
				return false;
			}
			
			if (bFetchAllPoints)
			{
				// Get all points
				OutPointData.Reserve(ColorRampParam->CachedPoints.Num());
				const bool bAllowShrinking = false;
				OutPointData.SetNum(0, bAllowShrinking);
				for (UHoudiniParameterRampColorPoint* const RampPoint : ColorRampParam->CachedPoints)
				{
					const bool bIsPointData = true;
					OutPointData.Add(TPair<UObject*, bool>(RampPoint, bIsPointData));
				}
			}
			else
			{
				OutPointData.Reserve(1);
				const bool bAllowShrinking = false;
				OutPointData.SetNum(0, bAllowShrinking);

				const bool bIsPointData = true;
				OutPointData.Add(TPair<UObject*, bool>(ColorRampParam->CachedPoints[InIndex], bIsPointData));
			}
			
			return true;
		}
	}
	else
	{
		TSet<int32> InstanceIndexesPendingDelete;
		int32 NumInsertOps = 0;
		TArray<UHoudiniParameterRampModificationEvent*>& ModificationEvents = FloatRampParam ? FloatRampParam->ModificationEvents : ColorRampParam->ModificationEvents;
		for (UHoudiniParameterRampModificationEvent const* const Event : ModificationEvents)
		{
			if (!IsValid(Event))
				continue;

			if (Event->IsInsertEvent())
				NumInsertOps++;
			else if (Event->IsDeleteEvent())
				InstanceIndexesPendingDelete.Add(Event->DeleteInstanceIndex);
		}

		const int32 PointsArraySize = FloatRampParam ? FloatRampParam->Points.Num() : ColorRampParam->Points.Num();
		const int32 NumActivePointsInArray = PointsArraySize - InstanceIndexesPendingDelete.Num();
		const int32 TotalNumPoints = NumActivePointsInArray + NumInsertOps;

		// Reserve the expected amount of space needed in the array and reset / destruct existing items so we can
		// add from index 0
		if (bFetchAllPoints)
			OutPointData.Reserve(TotalNumPoints);
		else
			OutPointData.Reserve(1);
		const bool bAllowShrinking = false;
		OutPointData.SetNum(0, bAllowShrinking);
		
		if (bFetchAllPoints || InIndex < NumActivePointsInArray)
		{
			// Getting all points or point is in the points array
			if (FloatRampParam)
			{
				const int32 ArraySize = FloatRampParam->Points.Num();
				int32 PointIndex = 0;
				for (int32 Index = 0; Index < ArraySize && (bFetchAllPoints || PointIndex <= InIndex); ++Index)
				{
					UHoudiniParameterRampFloatPoint* const PointData = FloatRampParam->Points[Index];
					const bool bIsDeletedPoint = (PointData && InstanceIndexesPendingDelete.Contains(PointData->InstanceIndex));
					if (!bIsDeletedPoint)
					{
						if (PointIndex == InIndex || bFetchAllPoints)
						{
							const bool bIsPointData = true;
							OutPointData.Add(TPair<UObject*, bool>(PointData, bIsPointData));
						}

						// If we are fetching only the point at InIndex, then we are done here
						if (PointIndex == InIndex)
							return true;

						PointIndex++;
					}
				}
			}
			else
			{
				const int32 ArraySize = ColorRampParam->Points.Num();
				int32 PointIndex = 0;
				for (int32 Index = 0; Index < ArraySize && (bFetchAllPoints || PointIndex <= InIndex); ++Index)
				{
					UHoudiniParameterRampColorPoint* const PointData = ColorRampParam->Points[Index];
					const bool bIsDeletedPoint = (PointData && InstanceIndexesPendingDelete.Contains(PointData->InstanceIndex));
					if (!bIsDeletedPoint)
					{
						if (PointIndex == InIndex || bFetchAllPoints)
						{
							const bool bIsPointData = true;
							OutPointData.Add(TPair<UObject*, bool>(PointData, bIsPointData));
						}

						// If we are fetching only the point at InIndex, then we are done here
						if (PointIndex == InIndex)
							return true;

						PointIndex++;
					}
				}
			}
		}
		
		if (bFetchAllPoints || InIndex < TotalNumPoints)
		{
			// Point is an insert operation
			const int32 NumEvents = ModificationEvents.Num();
			int32 PointIndex = NumActivePointsInArray;
			for (int32 Index = 0; Index < NumEvents && (bFetchAllPoints || PointIndex <= InIndex); ++Index)
			{
				UHoudiniParameterRampModificationEvent* const Event = ModificationEvents[Index];
				if (!IsValid(Event))
					continue;

				if (!Event->IsInsertEvent())
					continue;

				if (PointIndex == InIndex || bFetchAllPoints)
				{
					const bool bIsPointData = false;
					OutPointData.Add(TPair<UObject*, bool>(Event, bIsPointData));
				}

				if (PointIndex == InIndex)
					return true;
				
				PointIndex++;
			}
		}
		else
		{
			// Point is out of range
			SetErrorMessage(FString::Printf(
				TEXT("Ramp point index %d is out of range [0, %d]."), InIndex, TotalNumPoints));
			return false;
		}

		if (bFetchAllPoints)
		{
			if (TotalNumPoints != OutPointData.Num())
			{
				SetErrorMessage(FString::Printf(
					TEXT("Failed to fetch all ramp points. Got %d, expected %d."), OutPointData.Num(), TotalNumPoints));
				return false;
			}

			return true;
		}
	}

	// If we reach this point we didn't find the point
	SetErrorMessage(FString::Printf(TEXT("Could not find valid ramp point at index %d."), InIndex));
	return false;
}

bool UHoudiniPublicAPIAssetWrapper::SetRampParameterPointValue(
	FName InParameterTupleName,
	const int32 InPointIndex,
	const float InPosition,
	const float InFloatValue,
	const FLinearColor& InColorValue,
	const EHoudiniPublicAPIRampInterpolationType InInterpolation,
	const bool bInMarkChanged)
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	UHoudiniParameterRampColor* ColorRampParam = nullptr;
	
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(Param);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(Param);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(TEXT("Parameter '%s' is not a float ramp or color ramp parameter."), *(Param->GetName())));
		return false;
	}

	const EHoudiniRampInterpolationType NewInterpolation = UHoudiniPublicAPI::ToHoudiniRampInterpolationType(
		InInterpolation);
	
	// If the parameter is not set to auto update, or if cooking is paused, we have to set the cached points and
	// not the main points.
	// const bool bCookingEnabled = !FHoudiniEngineCommands::IsAssetCookingPaused();
	// const bool bUseCachedPoints = (!Param->IsAutoUpdate() || !bCookingEnabled);
	const bool bUseCachedPoints = !Param->IsAutoUpdate();

	// Get the point at InPointIndex's data
	TArray<TPair<UObject*, bool>> RampPointData;
	if (!FindRampPointData(Param, InPointIndex, RampPointData) || RampPointData.Num() < 1)
		return false;

	UObject* const PointData = RampPointData[0].Key;
	const bool bIsPointData = RampPointData[0].Value;
	if (!IsValid(PointData))
		return false;
	
	if (bIsPointData)
	{
		UHoudiniParameterRampFloatPoint* FloatPointData = nullptr;
		UHoudiniParameterRampColorPoint* ColorPointData = nullptr;

		if (FloatRampParam)
		{
			FloatPointData = Cast<UHoudiniParameterRampFloatPoint>(PointData);
			if (!IsValid(FloatPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampFloatPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
		}
		else
		{
			ColorPointData = Cast<UHoudiniParameterRampColorPoint>(PointData);
			if (!IsValid(ColorPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampColorPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
		}
		
		if (bUseCachedPoints)
		{
			// When setting the cached points, we set the values directly instead of using the setters, but we set
			// the bCaching flag on the parameter and mark the position/value/interpolation parent parameters as changed
			if (FloatPointData)
			{
				if (FloatPointData->Position != InPosition)
				{
					FloatPointData->Position = InPosition;
					FloatRampParam->bCaching = true;
				}

				if (FloatPointData->Value != InFloatValue)
				{
					FloatPointData->Value = InFloatValue;
					FloatRampParam->bCaching = true;
				}

				if (FloatPointData->Interpolation != NewInterpolation)
				{
					FloatPointData->Interpolation = NewInterpolation;
					FloatRampParam->bCaching = true;
				}
			}
			else if (ColorPointData)
			{
				if (ColorPointData->Position != InPosition)
				{
					ColorPointData->Position = InPosition;
					ColorRampParam->bCaching = true;
				}

				if (ColorPointData->Value != InColorValue)
				{
					ColorPointData->Value = InColorValue;
					ColorRampParam->bCaching = true;
				}

				if (ColorPointData->Interpolation != NewInterpolation)
				{
					ColorPointData->Interpolation = NewInterpolation;
					ColorRampParam->bCaching = true;
				}
			}

			// Update the ramp's widget if it is currently visible/selected
			const bool bForceFullUpdate = true;
			FHoudiniEngineUtils::UpdateEditorProperties(Param, bForceFullUpdate);
		}
		else
		{
			// When setting the main points, we set the values using the setters on the point data but still manually
			// mark the position/value/interpolation parent parameters as changed
			if (FloatPointData)
			{
				if (FloatPointData->Position != InPosition && FloatPointData->PositionParentParm)
				{
					FloatPointData->SetPosition(InPosition);
					if (bInMarkChanged)
						FloatPointData->PositionParentParm->MarkChanged(bInMarkChanged);
				}

				if (FloatPointData->Value != InFloatValue && FloatPointData->ValueParentParm)
				{
					FloatPointData->SetValue(InFloatValue);
					if (bInMarkChanged)
						FloatPointData->ValueParentParm->MarkChanged(bInMarkChanged);
				}

				if (FloatPointData->Interpolation != NewInterpolation && FloatPointData->InterpolationParentParm)
				{
					FloatPointData->SetInterpolation(NewInterpolation);
					if (bInMarkChanged)
						FloatPointData->InterpolationParentParm->MarkChanged(bInMarkChanged);
				}
			}
			else if (ColorPointData)
			{
				if (ColorPointData->Position != InPosition && ColorPointData->PositionParentParm)
				{
					ColorPointData->SetPosition(InPosition);
					if (bInMarkChanged)
						ColorPointData->PositionParentParm->MarkChanged(bInMarkChanged);
				}

				if (ColorPointData->Value != InColorValue && ColorPointData->ValueParentParm)
				{
					ColorPointData->SetValue(InColorValue);
					if (bInMarkChanged)
						ColorPointData->ValueParentParm->MarkChanged(bInMarkChanged);
				}

				if (ColorPointData->Interpolation != NewInterpolation && ColorPointData->InterpolationParentParm)
				{
					ColorPointData->SetInterpolation(NewInterpolation);
					if (bInMarkChanged)
						ColorPointData->InterpolationParentParm->MarkChanged(bInMarkChanged);
				}
			}
		}
	}
	else
	{
		UHoudiniParameterRampModificationEvent* const Event = Cast<UHoudiniParameterRampModificationEvent>(PointData);
		if (!IsValid(Event))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Expected UHoudiniParameterRampModificationEvent instance, but received incompatible class '%s'."),
				*(PointData->GetClass()->GetName())));
			return false;
		}

		Event->InsertPosition = InPosition;
		if (FloatRampParam)
		{
			Event->InsertFloat = InFloatValue;
		}
		else if (ColorRampParam)
		{
			Event->InsertColor = InColorValue;
		}
		Event->InsertInterpolation = NewInterpolation;
	}

	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetRampParameterPointValue(
	FName InParameterTupleName,
	const int32 InPointIndex,
	float& OutPosition,
	float& OutFloatValue,
	FLinearColor& OutColorValue,
	EHoudiniPublicAPIRampInterpolationType& OutInterpolation) const
{
	UHoudiniParameter* Param = FindValidParameterByName(InParameterTupleName);
	if (!Param)
		return false;

	UHoudiniParameterRampFloat* FloatRampParam = nullptr;
	UHoudiniParameterRampColor* ColorRampParam = nullptr;
	
	const EHoudiniParameterType ParamType = Param->GetParameterType();
	if (ParamType == EHoudiniParameterType::FloatRamp)
	{
		FloatRampParam = Cast<UHoudiniParameterRampFloat>(Param);
		if (!IsValid(FloatRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else if (ParamType == EHoudiniParameterType::ColorRamp)
	{
		ColorRampParam = Cast<UHoudiniParameterRampColor>(Param);
		if (!IsValid(ColorRampParam))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Unexpected parameter class (%s) vs type (%d)"), *Param->GetClass()->GetName(), ParamType));
			return false;
		}
	}
	else
	{
		SetErrorMessage(FString::Printf(TEXT("Parameter '%s' is not a float ramp or color ramp parameter."), *(Param->GetName())));
		return false;
	}

	TArray<TPair<UObject*, bool>> RampPointData;
	if (!FindRampPointData(Param, InPointIndex, RampPointData) || RampPointData.Num() < 1)
		return false;
	
	UObject* const PointData = RampPointData[0].Key;
	const bool bIsPointData = RampPointData[0].Value;
	if (!IsValid(PointData))
		return false;
	
	if (bIsPointData)
	{
		UHoudiniParameterRampFloatPoint* FloatPointData = nullptr;
		UHoudiniParameterRampColorPoint* ColorPointData = nullptr;

		if (FloatRampParam)
		{
			FloatPointData = Cast<UHoudiniParameterRampFloatPoint>(PointData);
			if (!IsValid(FloatPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampFloatPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
		}
		else
		{
			ColorPointData = Cast<UHoudiniParameterRampColorPoint>(PointData);
			if (!IsValid(ColorPointData))
			{
				SetErrorMessage(FString::Printf(
					TEXT("Expected UHoudiniParameterRampColorPoint instance, but received incompatible class '%s'."),
					*(PointData->GetClass()->GetName())));
				return false;
			}
		}
		
		// When setting the cached points, we set the values directly instead of using the setters, but we set
		// the bCaching flag on the parameter and mark the position/value/interpolation parent parameters as changed
		if (FloatPointData)
		{
			OutPosition = FloatPointData->Position;
			OutFloatValue = FloatPointData->Value;
			OutInterpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(
				FloatPointData->Interpolation);
		}
		else if (ColorPointData)
		{
			OutPosition = ColorPointData->Position;
			OutColorValue = ColorPointData->Value;
			OutInterpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(
				ColorPointData->Interpolation);
		}
	}
	else
	{
		UHoudiniParameterRampModificationEvent* const Event = Cast<UHoudiniParameterRampModificationEvent>(PointData);
		if (!IsValid(Event))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Expected UHoudiniParameterRampModificationEvent instance, but received incompatible class '%s'."),
				*(PointData->GetClass()->GetName())));
			return false;
		}

		OutPosition = Event->InsertPosition;
		if (FloatRampParam)
		{
			OutFloatValue = Event->InsertFloat;
		}
		else if (ColorRampParam)
		{
			OutColorValue = Event->InsertColor;
		}
		OutInterpolation = UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(Event->InsertInterpolation);
	}

	return true;
}

UHoudiniInput*
UHoudiniPublicAPIAssetWrapper::GetHoudiniNodeInputByIndex(const int32 InNodeInputIndex)
{
	if (InNodeInputIndex < 0)
		return nullptr;
	
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return nullptr;

	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput* const Input = HAC->GetInputAt(Index);
		if (!IsValid(Input))
			continue;
		if (Input->GetInputIndex() == InNodeInputIndex)
			return Input;
	}

	return nullptr;
}

const UHoudiniInput*
UHoudiniPublicAPIAssetWrapper::GetHoudiniNodeInputByIndex(const int32 InNodeInputIndex) const
{
	if (InNodeInputIndex < 0)
		return nullptr;
	
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return nullptr;

	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput const* const Input = HAC->GetInputAt(Index);
		if (!IsValid(Input))
			continue;
		if (Input->GetInputIndex() == InNodeInputIndex)
			return Input;
	}

	return nullptr;
}

UHoudiniInput*
UHoudiniPublicAPIAssetWrapper::FindValidHoudiniNodeInputParameter(const FName& InInputParameterName)
{
	if (InInputParameterName == NAME_None)
		return nullptr;
	
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return nullptr;

	const FString InputParameterName = InInputParameterName.ToString();
	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput* const Input = HAC->GetInputAt(Index);
		if (!IsValid(Input))
			continue;
		if (Input->IsObjectPathParameter() && Input->GetName() == InputParameterName)
			return Input;
	}

	return nullptr;
}

const UHoudiniInput*
UHoudiniPublicAPIAssetWrapper::FindValidHoudiniNodeInputParameter(const FName& InInputParameterName) const
{
	if (InInputParameterName == NAME_None)
		return nullptr;
	
	UHoudiniAssetComponent* HAC = nullptr;
	if (!GetValidHoudiniAssetComponentWithError(HAC))
		return nullptr;

	const FString InputParameterName = InInputParameterName.ToString();
	const int32 NumInputs = HAC->GetNumInputs();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		UHoudiniInput const* const Input = HAC->GetInputAt(Index);
		if (!IsValid(Input))
			continue;
		if (Input->IsObjectPathParameter() && Input->GetName() == InputParameterName)
			return Input;
	}

	return nullptr;
}

bool
UHoudiniPublicAPIAssetWrapper::CreateAndPopulateAPIInput(const UHoudiniInput* InHoudiniInput, UHoudiniPublicAPIInput*& OutAPIInput)
{
	if (!IsValid(InHoudiniInput))
		return false;

	TSubclassOf<UHoudiniPublicAPIInput> APIInputClass;
	const EHoudiniInputType InputType = InHoudiniInput->GetInputType();
	switch (InputType)
	{
		case EHoudiniInputType::Geometry:
			APIInputClass = UHoudiniPublicAPIGeoInput::StaticClass();
			break;
		case EHoudiniInputType::Curve:
			APIInputClass = UHoudiniPublicAPICurveInput::StaticClass();
			break;
		case EHoudiniInputType::Asset:
			APIInputClass = UHoudiniPublicAPIAssetInput::StaticClass();
			break;
		case EHoudiniInputType::World:
			APIInputClass = UHoudiniPublicAPIWorldInput::StaticClass();
			break;
		case EHoudiniInputType::Landscape:
			APIInputClass = UHoudiniPublicAPILandscapeInput::StaticClass();
			break;
		case EHoudiniInputType::Skeletal:
			// Not yet implemented
			SetErrorMessage(FString::Printf(TEXT("GetInputAtIndex: Input type not yet implemented %d"), InputType));
			return false;
		case EHoudiniInputType::GeometryCollection:
			APIInputClass = UHoudiniPublicAPIGeometryCollectionInput::StaticClass();
			break;
		case EHoudiniInputType::Invalid:
			SetErrorMessage(FString::Printf(TEXT("GetInputAtIndex: Invalid input type %d"), InputType));
			return false;
	}
	
	UHoudiniPublicAPIInput* APIInput = CreateEmptyInput(APIInputClass);
	if (!IsValid(APIInput))
	{
		return false;
	}

	const bool bSuccessfullyCopied = APIInput->PopulateFromHoudiniInput(InHoudiniInput);
	OutAPIInput = APIInput;
	return bSuccessfullyCopied;
}

bool
UHoudiniPublicAPIAssetWrapper::PopulateHoudiniInput(const UHoudiniPublicAPIInput* InAPIInput, UHoudiniInput* InHoudiniInput) const
{
	if (!IsValid(InHoudiniInput))
		return false;

	return InAPIInput->UpdateHoudiniInput(InHoudiniInput);
}

bool
UHoudiniPublicAPIAssetWrapper::GetValidTOPNetworkByPathWithError(const FString& InNetworkRelativePath, int32& OutNetworkIndex, UTOPNetwork*& OutNetwork) const
{
	UHoudiniPDGAssetLink* AssetLink = nullptr;
	if (!GetValidHoudiniPDGAssetLinkWithError(AssetLink))
		return false;

	int32 NetworkIndex = INDEX_NONE;
	UTOPNetwork* const Network = UHoudiniPDGAssetLink::GetTOPNetworkByNodePath(
		InNetworkRelativePath, AssetLink->AllTOPNetworks, NetworkIndex);
	if (!IsValid(Network))
	{
		SetErrorMessage(FString::Printf(
			TEXT("Could not find valid TOP network at relative path '%s'."), *InNetworkRelativePath));
		return false;
	}

	OutNetworkIndex = NetworkIndex;
	OutNetwork = Network;
	return true;
}

bool
UHoudiniPublicAPIAssetWrapper::GetValidTOPNodeByPathWithError(
	const FString& InNetworkRelativePath,
	const FString& InNodeRelativePath,
	int32& OutNetworkIndex,
	int32& OutNodeIndex,
	UTOPNode*& OutNode) const
{
	int32 NetworkIndex = INDEX_NONE;
	UTOPNetwork* Network = nullptr;
	if (!GetValidTOPNetworkByPathWithError(InNetworkRelativePath, NetworkIndex, Network))
		return false;

	int32 NodeIndex = INDEX_NONE;
	UTOPNode* const Node = UHoudiniPDGAssetLink::GetTOPNodeByNodePath(
		InNodeRelativePath, Network->AllTOPNodes, NodeIndex);
	if (!IsValid(Node))
	{
		SetErrorMessage(FString::Printf(
			TEXT("Could not find valid TOP node at relative path '%s'."), *InNodeRelativePath));
		return false;
	}

	OutNetworkIndex = NetworkIndex;
	OutNodeIndex = NodeIndex;
	OutNode = Node;
	return true;
}
