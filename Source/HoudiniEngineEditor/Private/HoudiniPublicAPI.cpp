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


#include "HoudiniPublicAPI.h"

#include "HoudiniAsset.h"
#include "HoudiniCookable.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineManager.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniPreset.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniEngineCommands.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopedSlowTask.h"

namespace
{
bool WaitForInstantiation(UHoudiniPublicAPIAssetWrapper* Wrapper, FString& OutErrorMessage)
{
	if (!IsValid(Wrapper))
	{
		OutErrorMessage = TEXT("Could not wait for an invalid asset wrapper.");
		return false;
	}

	UHoudiniCookable* const Cookable = Wrapper->GetHoudiniCookable();
	if (!IsValid(Cookable))
	{
		OutErrorMessage = TEXT("Could not find the cookable to instantiate.");
		return false;
	}

	if (!FHoudiniEngine::Get().IsCookingEnabled())
	{
		OutErrorMessage = TEXT("Could not instantiate the asset because Houdini Engine cooking is disabled.");
		return false;
	}

	FHoudiniEngineManager* const HoudiniEngineManager = FHoudiniEngine::Get().GetHoudiniEngineManager();
	if (!HoudiniEngineManager)
	{
		OutErrorMessage = TEXT("Could not find a valid Houdini Engine manager to instantiate the asset.");
		return false;
	}

	const double TimeoutSeconds = 60.0;
	const double StartTime = FPlatformTime::Seconds();
	const FString HoudiniAssetName = Cookable->GetHoudiniAssetName();
	const FString CookableName = Cookable->GetDisplayName();
	FScopedSlowTask SlowTask(
		0.0f,
		FText::FromString(FString::Printf(
			TEXT("Instantiating HDA '%s' on cookable '%s'..."),
			*HoudiniAssetName,
			*CookableName)));
	SlowTask.MakeDialog(/* bShowCancelButton= */ true);

	bool bInstantiationStarted = false;
	while (FPlatformTime::Seconds() - StartTime < TimeoutSeconds)
	{
		const EHoudiniAssetState PreviousState = Cookable->GetCurrentState();
		if (PreviousState == EHoudiniAssetState::PreCook || PreviousState == EHoudiniAssetState::None)
			return Cookable->GetNodeId() >= 0;

		if (SlowTask.ShouldCancel())
		{
			OutErrorMessage = TEXT("Asset instantiation was cancelled.");
			return false;
		}

		if (PreviousState == EHoudiniAssetState::Dormant
			|| (PreviousState == EHoudiniAssetState::NeedInstantiation && bInstantiationStarted))
		{
			OutErrorMessage = TEXT("The asset could not be instantiated.");
			return false;
		}

		if (Cookable->ShouldTryToStartFirstSession())
			HoudiniEngineManager->AutoStartFirstSessionIfNeeded();

		HoudiniEngineManager->ProcessCookable(Cookable);

		const EHoudiniAssetState CurrentState = Cookable->GetCurrentState();
		if (CurrentState == EHoudiniAssetState::Instantiating)
			bInstantiationStarted = true;

		if (CurrentState == EHoudiniAssetState::PreCook || CurrentState == EHoudiniAssetState::None)
			return Cookable->GetNodeId() >= 0;

		if (CurrentState == EHoudiniAssetState::Dormant
			|| (CurrentState == EHoudiniAssetState::NeedInstantiation && bInstantiationStarted))
		{
			OutErrorMessage = TEXT("The asset could not be instantiated.");
			return false;
		}

		if (CurrentState == PreviousState && CurrentState != EHoudiniAssetState::Instantiating)
		{
			OutErrorMessage = TEXT("The asset could not make progress while instantiating.");
			return false;
		}

		SlowTask.EnterProgressFrame(0.0f);
		FPlatformProcess::Sleep(0.01f);
	}

	OutErrorMessage = TEXT("Asset instantiation timed out.");
	return false;
}
}

UHoudiniPublicAPI::UHoudiniPublicAPI()
{
}

bool
UHoudiniPublicAPI::IsSessionValid_Implementation() const
{
	return FHoudiniEngineCommands::IsSessionValid(); 
}

void
UHoudiniPublicAPI::CreateSession_Implementation()
{
	if (!IsSessionValid())
		FHoudiniEngineCommands::CreateSession();
}

void
UHoudiniPublicAPI::StopSession_Implementation()
{
	if (IsSessionValid())
		FHoudiniEngineCommands::StopSession();
}

void
UHoudiniPublicAPI::RestartSession_Implementation()
{
	if (IsSessionValid())
		FHoudiniEngineCommands::RestartSession();
	else
		FHoudiniEngineCommands::CreateSession();
}

UHoudiniPublicAPIAssetWrapper*
UHoudiniPublicAPI::Instantiate_Implementation(
	UHoudiniAsset* InHoudiniAsset,
	EHoudiniPublicAPIInstantiationType InInstantiationType,
	FHoudiniPublicAPISettings InSettings)
{
	UHoudiniPublicAPIAssetWrapper* Wrapper = nullptr;
	switch (InInstantiationType)
	{
		case EHoudiniPublicAPIInstantiationType::HoudiniAssetComponent:
			Wrapper = InstantiateAsset(
				InHoudiniAsset,
				InSettings.Transform,
				InSettings.WorldContextObject,
				InSettings.SpawnInLevelOverride,
				InSettings.bEnableAutoCook,
				InSettings.bEnableAutoBake,
				InSettings.BakeDirectoryPath,
				InSettings.BakeMethod,
				InSettings.bRemoveOutputAfterBake,
				InSettings.bRecenterBakedActors,
				InSettings.bReplacePreviousBake);
			break;

		case EHoudiniPublicAPIInstantiationType::Cookable:
			Wrapper = InstantiateAssetAsCookable(
				InHoudiniAsset,
				InSettings.bEnableAutoCook,
				InSettings.bEnableAutoBake,
				InSettings.BakeDirectoryPath,
				InSettings.BakeMethod,
				InSettings.bRemoveOutputAfterBake,
				InSettings.bRecenterBakedActors,
				InSettings.bReplacePreviousBake);
			break;

		default:
			SetErrorMessage(TEXT("InInstantiationType is invalid."));
			return nullptr;
	}

	if (!IsValid(Wrapper))
		return nullptr;

	FString ErrorMessage;
	if (!WaitForInstantiation(Wrapper, ErrorMessage))
	{
		SetErrorMessage(ErrorMessage);
		return nullptr;
	}

	return Wrapper;
}

UHoudiniPublicAPIAssetWrapper*
UHoudiniPublicAPI::InstantiateAsset_Implementation(
	UHoudiniAsset* InHoudiniAsset,
	const FTransform& InInstantiateAt,
	UObject* InWorldContextObject,
	ULevel* InSpawnInLevelOverride,
	const bool bInEnableAutoCook,
	const bool bInEnableAutoBake,
	const FString& InBakeDirectoryPath,
	const EHoudiniEngineBakeOption InBakeMethod,
	const bool bInRemoveOutputAfterBake,
	const bool bInRecenterBakedActors,
	const bool bInReplacePreviousBake)
{
	if (!IsValid(InHoudiniAsset) || !(InHoudiniAsset->AssetImportData))
	{
		SetErrorMessage(TEXT("InHoudiniAsset is invalid or does not have AssetImportData."));
		return nullptr;
	}

	// Create wrapper for asset instance
	UHoudiniPublicAPIAssetWrapper* Wrapper = UHoudiniPublicAPIAssetWrapper::CreateEmptyWrapper(this);

	if (Wrapper)
	{
		// Enable/disable error logging based on the API setting
		Wrapper->SetLoggingErrorsEnabled(IsLoggingErrors());

		if (!InstantiateAssetWithExistingWrapper(
				Wrapper,
				InHoudiniAsset,
				InInstantiateAt,
				InWorldContextObject,
				InSpawnInLevelOverride,
				bInEnableAutoCook,
				bInEnableAutoBake,
				InBakeDirectoryPath,
				InBakeMethod,
				bInRemoveOutputAfterBake,
				bInRecenterBakedActors,
				bInReplacePreviousBake))
		{
			// failed to instantiate asset, return null
			return nullptr;
		}
	}

	return Wrapper;
}

UHoudiniPublicAPIAssetWrapper*
UHoudiniPublicAPI::InstantiateAssetAsCookable(
	UHoudiniAsset* InHoudiniAsset,
	const bool bInEnableAutoCook,
	const bool bInEnableAutoBake,
	const FString& InBakeDirectoryPath,
	const EHoudiniEngineBakeOption InBakeMethod,
	const bool bInRemoveOutputAfterBake,
	const bool bInRecenterBakedActors,
	const bool bInReplacePreviousBake)
{
	if (!IsValid(InHoudiniAsset) || !(InHoudiniAsset->AssetImportData))
	{
		SetErrorMessage(TEXT("InHoudiniAsset is invalid or does not have AssetImportData."));
		return nullptr;
	}

	UHoudiniPublicAPIAssetWrapper* Wrapper = UHoudiniPublicAPIAssetWrapper::CreateEmptyWrapper(this);
	if (!IsValid(Wrapper))
	{
		SetErrorMessage(TEXT("Could not create a UHoudiniPublicAPIAssetWrapper."));
		return nullptr;
	}

	Wrapper->SetLoggingErrorsEnabled(IsLoggingErrors());

	UHoudiniCookable* Cookable = NewObject<UHoudiniCookable>(Wrapper, NAME_None, RF_Public);
	if (!IsValid(Cookable))
	{
		SetErrorMessage(TEXT("Could not create a UHoudiniCookable."));
		return nullptr;
	}

	Cookable->SetHoudiniAssetSupported(true);
	Cookable->SetParameterSupported(true);
	Cookable->SetInputSupported(true);
	Cookable->SetOutputSupported(true);
	Cookable->SetComponentSupported(false);
	Cookable->SetPDGSupported(true);
	Cookable->SetBakingSupported(true);
	Cookable->SetProxySupported(true);
	Cookable->SetImageSupported(true);
	Cookable->GetOutputData()->bCreateSceneComponents = false;
	Cookable->SetHoudiniAsset(InHoudiniAsset);

	if (!Wrapper->WrapHoudiniAssetObject(Cookable))
	{
		FString WrapperError;
		Wrapper->GetLastErrorMessage(WrapperError);
		SetErrorMessage(FString::Printf(TEXT("Failed to wrap the cookable: %s."), *WrapperError));
		return nullptr;
	}

	Wrapper->SetAutoCookingEnabled(bInEnableAutoCook);

	FDirectoryPath BakeDirectoryPath;
	BakeDirectoryPath.Path = InBakeDirectoryPath;
	Wrapper->SetBakeFolder(BakeDirectoryPath);
	Wrapper->SetBakeMethod(InBakeMethod);
	Wrapper->SetRemoveOutputAfterBake(bInRemoveOutputAfterBake);
	Wrapper->SetRecenterBakedActors(bInRecenterBakedActors);
	Wrapper->SetReplacePreviousBake(bInReplacePreviousBake);
	Wrapper->SetAutoBakeEnabled(bInEnableAutoBake);

	FHoudiniEngineRuntime::Get().RegisterHoudiniCookable(Cookable);

	return Wrapper;
}

bool
UHoudiniPublicAPI::InstantiateAssetWithExistingWrapper_Implementation(
	UHoudiniPublicAPIAssetWrapper* InWrapper,
	UHoudiniAsset* InHoudiniAsset,
	const FTransform& InInstantiateAt,
	UObject* InWorldContextObject,
	ULevel* InSpawnInLevelOverride,
	const bool bInEnableAutoCook,
	const bool bInEnableAutoBake,
	const FString& InBakeDirectoryPath,
	const EHoudiniEngineBakeOption InBakeMethod,
	const bool bInRemoveOutputAfterBake,
	const bool bInRecenterBakedActors,
	const bool bInReplacePreviousBake)
{
	if (!IsValid(InWrapper))
	{
		SetErrorMessage(TEXT("InWrapper is not valid."));
		return false;
	}

	if (!IsValid(InHoudiniAsset) || !(InHoudiniAsset->AssetImportData))
	{
		SetErrorMessage(TEXT("InHoudiniAsset is invalid or does not have AssetImportData."));
		return false;
	}

	UWorld* OverrideWorldToSpawnIn = IsValid(InWorldContextObject) ? InWorldContextObject->GetWorld() : nullptr;
	AActor* HoudiniAssetActor = FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(InHoudiniAsset, InInstantiateAt, OverrideWorldToSpawnIn, InSpawnInLevelOverride);
	if (!IsValid(HoudiniAssetActor))
	{
		// Determine the path of what would have been the owning level/world of the actor for error logging purposes
		const FString LevelPath = IsValid(InSpawnInLevelOverride) ? InSpawnInLevelOverride->GetPathName() : FString();
		const FString WorldPath = (OverrideWorldToSpawnIn && IsValid(OverrideWorldToSpawnIn)) ? OverrideWorldToSpawnIn->GetPathName() : FString();
		const FString OwnerPath = LevelPath.IsEmpty() ? WorldPath : LevelPath;
		SetErrorMessage(FString::Printf(TEXT("Failed to spawn a AHoudiniAssetActor in %s."), *OwnerPath));
		return false;
	}

	// Wrap the instantiated asset
	if (!InWrapper->WrapHoudiniAssetObject(HoudiniAssetActor))
	{
		FString WrapperError;
		InWrapper->GetLastErrorMessage(WrapperError);
		SetErrorMessage(FString::Printf(
			TEXT("Failed to wrap '%s': %s."), *(HoudiniAssetActor->GetActorNameOrLabel()), *WrapperError));
		return false;
	}

	InWrapper->SetAutoCookingEnabled(bInEnableAutoCook);

	FDirectoryPath BakeDirectoryPath;
	BakeDirectoryPath.Path = InBakeDirectoryPath;
	InWrapper->SetBakeFolder(BakeDirectoryPath);
	InWrapper->SetBakeMethod(InBakeMethod);
	InWrapper->SetRemoveOutputAfterBake(bInRemoveOutputAfterBake);
	InWrapper->SetRecenterBakedActors(bInRecenterBakedActors);
	InWrapper->SetReplacePreviousBake(bInReplacePreviousBake);
	InWrapper->SetAutoBakeEnabled(bInEnableAutoBake);
	
	return true;
}

UHoudiniPublicAPIAssetWrapper*
UHoudiniPublicAPI::InstantiatePreset_Implementation(
	UHoudiniPreset* InHoudiniPreset,
	const FTransform& InInstantiateAt,
	UObject* InWorldContextObject,
	ULevel* InSpawnInLevelOverride,
	const bool bInEnableAutoCook,
	const bool bInEnableAutoBake,
	const FString& InBakeDirectoryPath,
	const EHoudiniEngineBakeOption InBakeMethod,
	const bool bInRemoveOutputAfterBake,
	const bool bInRecenterBakedActors,
	const bool bInReplacePreviousBake)
{
	if (!IsValid(InHoudiniPreset))
	{
		SetErrorMessage(TEXT("InHoudiniPreset is invalid."));
		return nullptr;
	}

	// Get the preset's HDA
	UHoudiniAsset* SourceHDA = InHoudiniPreset->SourceHoudiniAsset;
	if (!IsValid(SourceHDA) || !(SourceHDA->AssetImportData))
	{
		SetErrorMessage(TEXT("The preset's Houdini Asset is invalid or does not have AssetImportData."));
		return nullptr;
	}

	// Create wrapper for asset instance
	UHoudiniPublicAPIAssetWrapper* Wrapper = UHoudiniPublicAPIAssetWrapper::CreateEmptyWrapper(this);

	if (Wrapper)
	{
		// Enable/disable error logging based on the API setting
		Wrapper->SetLoggingErrorsEnabled(IsLoggingErrors());

		if (!InstantiatePresetWithExistingWrapper(
			Wrapper,
			InHoudiniPreset,
			InInstantiateAt,
			InWorldContextObject,
			InSpawnInLevelOverride,
			bInEnableAutoCook,
			bInEnableAutoBake,
			InBakeDirectoryPath,
			InBakeMethod,
			bInRemoveOutputAfterBake,
			bInRecenterBakedActors,
			bInReplacePreviousBake))
		{
			// failed to instantiate preset, return null
			return nullptr;
		}
	}

	return Wrapper;
}

bool
UHoudiniPublicAPI::InstantiatePresetWithExistingWrapper_Implementation(
	UHoudiniPublicAPIAssetWrapper* InWrapper,
	UHoudiniPreset* InHoudiniPreset,
	const FTransform& InInstantiateAt,
	UObject* InWorldContextObject,
	ULevel* InSpawnInLevelOverride,
	const bool bInEnableAutoCook,
	const bool bInEnableAutoBake,
	const FString& InBakeDirectoryPath,
	const EHoudiniEngineBakeOption InBakeMethod,
	const bool bInRemoveOutputAfterBake,
	const bool bInRecenterBakedActors,
	const bool bInReplacePreviousBake)
{
	if (!IsValid(InWrapper))
	{
		SetErrorMessage(TEXT("InWrapper is not valid."));
		return false;
	}

	if (!IsValid(InHoudiniPreset))
	{
		SetErrorMessage(TEXT("InHoudiniPreset is invalid."));
		return false;
	}

	// Get the preset's HDA
	UHoudiniAsset* SourceHDA = InHoudiniPreset->SourceHoudiniAsset;
	if (!IsValid(SourceHDA) || !(SourceHDA->AssetImportData))
	{
		SetErrorMessage(TEXT("The preset's Houdini Asset is invalid or does not have AssetImportData."));
		return false;
	}

	UWorld* OverrideWorldToSpawnIn = IsValid(InWorldContextObject) ? InWorldContextObject->GetWorld() : nullptr;
	AActor* HoudiniAssetActor = FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(SourceHDA, InInstantiateAt, OverrideWorldToSpawnIn, InSpawnInLevelOverride, InHoudiniPreset);
	if (!IsValid(HoudiniAssetActor))
	{
		// Determine the path of what would have been the owning level/world of the actor for error logging purposes
		const FString LevelPath = IsValid(InSpawnInLevelOverride) ? InSpawnInLevelOverride->GetPathName() : FString();
		const FString WorldPath = IsValid(OverrideWorldToSpawnIn) ? OverrideWorldToSpawnIn->GetPathName() : FString();
		const FString OwnerPath = LevelPath.IsEmpty() ? WorldPath : LevelPath;
		SetErrorMessage(FString::Printf(TEXT("Failed to spawn a AHoudiniAssetActor in %s."), *OwnerPath));
		return false;
	}

	// Wrap the instantiated asset
	if (!InWrapper->WrapHoudiniAssetObject(HoudiniAssetActor))
	{
		FString WrapperError;
		InWrapper->GetLastErrorMessage(WrapperError);
		SetErrorMessage(FString::Printf(
			TEXT("Failed to wrap '%s': %s."), *(HoudiniAssetActor->GetActorNameOrLabel()), *WrapperError));
		return false;
	}

	InWrapper->SetAutoCookingEnabled(bInEnableAutoCook);

	FDirectoryPath BakeDirectoryPath;
	BakeDirectoryPath.Path = InBakeDirectoryPath;
	InWrapper->SetBakeFolder(BakeDirectoryPath);
	InWrapper->SetBakeMethod(InBakeMethod);
	InWrapper->SetRemoveOutputAfterBake(bInRemoveOutputAfterBake);
	InWrapper->SetRecenterBakedActors(bInRecenterBakedActors);
	InWrapper->SetReplacePreviousBake(bInReplacePreviousBake);
	InWrapper->SetAutoBakeEnabled(bInEnableAutoBake);

	return true;
}


bool
UHoudiniPublicAPI::IsAssetCookingPaused_Implementation() const 
{ 
	return FHoudiniEngineCommands::IsAssetCookingPaused(); 
}

void
UHoudiniPublicAPI::PauseAssetCooking_Implementation()
{
	if (!IsAssetCookingPaused())
		FHoudiniEngineCommands::PauseAssetCooking();
}

void
UHoudiniPublicAPI::ResumeAssetCooking_Implementation()
{
	if (IsAssetCookingPaused())
		FHoudiniEngineCommands::PauseAssetCooking();
}

UHoudiniPublicAPIInput*
UHoudiniPublicAPI::CreateEmptyInput_Implementation(TSubclassOf<UHoudiniPublicAPIInput> InInputClass, UObject* InOuter)
{
	UObject* Outer = InOuter;
	if (!IsValid(Outer))
		Outer = this;
	UHoudiniPublicAPIInput* const NewInput = NewObject<UHoudiniPublicAPIInput>(Outer, InInputClass.Get());
	if (!IsValid(NewInput))
	{
		SetErrorMessage(TEXT("Could not create a new valid UHoudiniPublicAPIInput instance."));
	}
	else if (Outer)
	{
		// Enable/disable error logging based on the outer's setting (if it is a sub-class of UHoudiniPublicAPIObjectBase)
		const bool bShouldLogErrors = Outer->IsA<UHoudiniPublicAPIObjectBase>() ?
			Cast<UHoudiniPublicAPIObjectBase>(Outer)->IsLoggingErrors() : IsLoggingErrors();
		NewInput->SetLoggingErrorsEnabled(bShouldLogErrors);
	}

	return NewInput;
}

EHoudiniPublicAPIRampInterpolationType
UHoudiniPublicAPI::ToHoudiniPublicAPIRampInterpolationType(const EHoudiniRampInterpolationType InInterpolationType)
{
	switch (InInterpolationType)
	{
		case EHoudiniRampInterpolationType::InValid:
			return EHoudiniPublicAPIRampInterpolationType::InValid;
		case EHoudiniRampInterpolationType::BEZIER:
			return EHoudiniPublicAPIRampInterpolationType::BEZIER;
		case EHoudiniRampInterpolationType::LINEAR:
			return EHoudiniPublicAPIRampInterpolationType::LINEAR;
		case EHoudiniRampInterpolationType::BSPLINE:
			return EHoudiniPublicAPIRampInterpolationType::BSPLINE;
		case EHoudiniRampInterpolationType::HERMITE:
			return EHoudiniPublicAPIRampInterpolationType::HERMITE;
		case EHoudiniRampInterpolationType::CONSTANT:
			return EHoudiniPublicAPIRampInterpolationType::CONSTANT;
		case EHoudiniRampInterpolationType::CATMULL_ROM:
			return EHoudiniPublicAPIRampInterpolationType::CATMULL_ROM;
		case EHoudiniRampInterpolationType::MONOTONE_CUBIC:
			return EHoudiniPublicAPIRampInterpolationType::MONOTONE_CUBIC;
	}

	return EHoudiniPublicAPIRampInterpolationType::InValid;
}

EHoudiniRampInterpolationType
UHoudiniPublicAPI::ToHoudiniRampInterpolationType(const EHoudiniPublicAPIRampInterpolationType InInterpolationType)
{
	switch (InInterpolationType)
	{
		case EHoudiniPublicAPIRampInterpolationType::InValid:
			return EHoudiniRampInterpolationType::InValid;
		case EHoudiniPublicAPIRampInterpolationType::BEZIER:
			return EHoudiniRampInterpolationType::BEZIER;
		case EHoudiniPublicAPIRampInterpolationType::LINEAR:
			return EHoudiniRampInterpolationType::LINEAR;
		case EHoudiniPublicAPIRampInterpolationType::BSPLINE:
			return EHoudiniRampInterpolationType::BSPLINE;
		case EHoudiniPublicAPIRampInterpolationType::HERMITE:
			return EHoudiniRampInterpolationType::HERMITE;
		case EHoudiniPublicAPIRampInterpolationType::CONSTANT:
			return EHoudiniRampInterpolationType::CONSTANT;
		case EHoudiniPublicAPIRampInterpolationType::CATMULL_ROM:
			return EHoudiniRampInterpolationType::CATMULL_ROM;
		case EHoudiniPublicAPIRampInterpolationType::MONOTONE_CUBIC:
			return EHoudiniRampInterpolationType::MONOTONE_CUBIC;
	}

	return EHoudiniRampInterpolationType::InValid;
}
