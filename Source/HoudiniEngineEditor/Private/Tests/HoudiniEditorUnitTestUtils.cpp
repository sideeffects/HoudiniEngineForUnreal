#include "HoudiniEditorUnitTestUtils.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniCookable.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterInt.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"

#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniAssetActorFactory.h"
#include "HoudiniEngineOutputStats.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEditorTestUtils.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniPDGManager.h"

#include "FileHelpers.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/AutomationTest.h"


UWorld* 
FHoudiniEditorUnitTestUtils::CreateEmptyMap(bool bOpenWorld)
{
	FString MapName = bOpenWorld ? TEXT("/Engine/Maps/Templates/OpenWorld.umap") : TEXT("/Engine/Maps/Templates/Template_Default.umap");

	UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(MapName, false);
	return World;
}

UHoudiniAssetComponent* FHoudiniEditorUnitTestUtils::LoadHDAIntoNewMap(
	const FString& PackageName, 
	const FTransform& Transform, 
	bool bOpenWorld)
{
	UWorld * World = CreateEmptyMap(bOpenWorld);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByPackageName(FName(PackageName), AssetData);
	if (AssetData.IsEmpty())
		return nullptr;
	UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(AssetData[0].GetAsset());

	UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
	if (!Factory)
		return nullptr;

	AActor* CreatedActor = Factory->CreateActor(Cast<UObject>(HoudiniAsset), World->GetCurrentLevel(), Transform);

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(CreatedActor->GetComponentByClass(UHoudiniAssetComponent::StaticClass()));
	return HAC;
}


bool
FHoudiniEditorUnitTestUtils::IsTemporary(const FString& TempFolder, const FString& ObjectPath)
{
	bool bIsTempAsset = ObjectPath.StartsWith(TempFolder);
	return bIsTempAsset;
}



AActor* 
FHoudiniEditorUnitTestUtils::GetActorWithName(UWorld* World, FString& Name)
{
	if (!IsValid(World))
		return nullptr;

	ULevel* Level = World->GetLevel(0);
	if (!IsValid(Level))
		return nullptr;

	for (AActor* Actor : Level->Actors)
	{
		if (IsValid(Actor) && Actor->GetName() == Name)
			return Actor;
	}
	return nullptr;
}


bool
FHoudiniLatentTestCommand::Update()
{
	if(SingleContext.IsValid())
	{
		bool bDone = CheckForCookingComplete(SingleContext.Get());
		if(!bDone)
		{
			return false;
		}
	}
	else if (MultiContext.IsValid())
	{
		for (auto Context : MultiContext->Contexts)
		{
			bool bDone = CheckForCookingComplete(Context.Get());
			if(!bDone)
			{
				return false;
			}
		}
	}

	bool bDone = FFunctionLatentCommand::Update();
	return bDone;

}

bool
FHoudiniLatentTestCommand::CheckForCookingComplete(FHoudiniTestContext* Context)
{
	double DeltaTime = FPlatformTime::Seconds() - Context->TimeStarted;
	if(DeltaTime > Context->MaxTime)
	{
		Context->Test->AddError(FString::Printf(TEXT("***************** Test timed out After %.2f seconds*************"), DeltaTime));
		return true;
	}

	int CurrentFrame = GFrameCounter;
	if(Context->WaitTickFrame)
	{
		if(CurrentFrame < Context->WaitTickFrame)
		{
			return false;
		}
		else
		{
			Context->WaitTickFrame = 0;
		}
	}

	if(Context->CookingState == EHoudiniContextState::Cooking)
	{
		return false;
	}
	
	if(Context->PDGState == EHoudiniContextState::Cooking)
	{
		return false;
	}

	return true;
}

void 
FHoudiniTestContext::StartCookingHDA()
{
	if (HC)
		HC->MarkAsNeedCook();
	else
		HAC->MarkAsNeedCook();

	CookingState = EHoudiniContextState::Cooking;
}

void 
FHoudiniTestContext::WaitForTicks(int Count)
{
	WaitTickFrame = Count + GFrameCounter;
}


bool 
FHoudiniTestContext::StartCookingSelectedTOPNetwork()
{
	UHoudiniPDGAssetLink * AssetLink = HC ? HC->GetPDGAssetLink() : HAC->GetPDGAssetLink();
	if(!AssetLink)
		return false;

	UTOPNetwork* TopNetwork = AssetLink->GetSelectedTOPNetwork();
	if(!TopNetwork)
		return false;

	TopNetwork->GetOnPostCookDelegate().AddLambda([this](UTOPNetwork* Link, bool bSuccess)
	{
		this->PDGState = EHoudiniContextState::Complete;
		return;
	});

	FHoudiniPDGManager::CookOutput(TopNetwork);

	this->PDGState = EHoudiniContextState::Cooking;

	return true;
}

TArray<FHoudiniEngineBakedActor> 
FHoudiniTestContext::BakeSelectedTopNetwork()
{
	UHoudiniPDGAssetLink * PDGAssetLink = HAC->GetPDGAssetLink();

	FHoudiniBakedObjectData BakeOutputs;
	TArray<FHoudiniEngineBakedActor> BakedActors;

	FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(
		PDGAssetLink,
		PDGAssetLink->PDGBakeSelectionOption, 
		PDGAssetLink->PDGBakePackageReplaceMode, 
		PDGAssetLink->bRecenterBakedActors,
		BakeOutputs,
		BakedActors);

	return BakedActors;
}

FString 
FHoudiniEditorUnitTestUtils::GetAbsolutePathOfProjectFile(const FString& Object)
{
	FString Path =  FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FString File = Path + TEXT("/") + Object;
	FPaths::MakePlatformFilename(File);
	return File;

}

UHoudiniParameter* 
FHoudiniEditorUnitTestUtils::GetTypedParameter(UHoudiniAssetComponent* HAC, UClass* Class, const char* Name)
{
	FString ParamName = Name;
	UHoudiniParameter * Parameter = HAC->FindParameterByName(FString(ParamName));
	if (!Parameter)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find parameter called %s. Dumping Parameters:"), *ParamName);
		for(int Index = 0; Index < HAC->GetNumParameters(); Index++)
		{
			UHoudiniParameter * Param = HAC->GetParameterAt(Index);
			HOUDINI_LOG_ERROR(TEXT("Parameter %d name=%s label=%s class=%s"), 
				Index, *Param->GetParameterName(), *Param->GetParameterLabel(), *Param->GetClass()->GetName());
		}
		return nullptr;
	}

	if (!Parameter->IsA(Class))
	{
		HOUDINI_LOG_ERROR(TEXT("Parameter '%s' is of wrong type. IsA '%s' expected '%s'"), *ParamName, *Parameter->GetClass()->GetName(), *Class->GetName());
		return nullptr;
	}
	return Parameter;
}

UHoudiniParameter*
FHoudiniEditorUnitTestUtils::GetTypedParameter(UHoudiniCookable* HC, UClass* Class, const char* Name)
{
	FString ParamName = Name;
	UHoudiniParameter* Parameter = HC->FindParameterByName(FString(ParamName));
	if (!Parameter)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find parameter called %s. Dumping Parameters:"), *ParamName);
		for (int Index = 0; Index < HC->GetNumParameters(); Index++)
		{
			UHoudiniParameter* Param = HC->GetParameterAt(Index);
			HOUDINI_LOG_ERROR(TEXT("Parameter %d name=%s label=%s class=%s"),
				Index, *Param->GetParameterName(), *Param->GetParameterLabel(), *Param->GetClass()->GetName());
		}
		return nullptr;
	}

	if (!Parameter->IsA(Class))
	{
		HOUDINI_LOG_ERROR(TEXT("Parameter '%s' is of wrong type. IsA '%s' expected '%s'"), *ParamName, *Parameter->GetClass()->GetName(), *Class->GetName());
		return nullptr;
	}
	return Parameter;
}

FHoudiniTestContext::FHoudiniTestContext(FAutomationTestBase* CurrentTest, UWorld* InWorld, const FString& ActorLabel)
{
	World = InWorld;
	Test = CurrentTest;
	TimeStarted = FPlatformTime::Seconds();

	FindHACInWorld(ActorLabel);
}

void
FHoudiniTestContext::FindHACInWorld(const FString& ActorLabel)
{
	// Find Houdini Asset Actor and then component.
	UHoudiniAssetComponent* FoundHAC = nullptr;
	for(TActorIterator<AActor> ActorItr(World, AHoudiniAssetActor::StaticClass()); ActorItr; ++ActorItr)
	{
		AActor* FoundActor = *ActorItr;
		if(!FoundActor)
			continue;

		bool bNameMatches = true;
		if(!ActorLabel.IsEmpty())
			bNameMatches = ActorLabel == FoundActor->GetActorLabel();

		if (bNameMatches)
		{
			FoundHAC = FoundActor->FindComponentByClass<UHoudiniAssetComponent>();
			break;
		}
	}

	if(!FoundHAC)
		return;

	SetHAC(FoundHAC);

	// Set time last so we don't include instantiation time.
	TimeStarted = FPlatformTime::Seconds();
}

FHoudiniTestContext::FHoudiniTestContext(
	FAutomationTestBase* CurrentTest,
	const FString& MapName,
	const FString& ActorLabel)
{
	World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(MapName, false);
	Test = CurrentTest;
	TimeStarted = FPlatformTime::Seconds();

	FindHACInWorld(ActorLabel);
}

FHoudiniTestContext::FHoudiniTestContext(
	FAutomationTestBase* CurrentTest,
	bool bOpenWorld)
{
	World = FHoudiniEditorUnitTestUtils::CreateEmptyMap(bOpenWorld);
	Test = CurrentTest;
	TimeStarted = FPlatformTime::Seconds();
}

FHoudiniTestContext::FHoudiniTestContext(
	FAutomationTestBase* CurrentTest, 
	const FString& HDAName,
	const FTransform& Transform,
	bool bOpenWorld)
{
	Test = CurrentTest;

	// Load the HDA into a new map and kick start the cook. We do an initial cook to make sure the parameters are available.
	UHoudiniAssetComponent* CreatedHAC = FHoudiniEditorUnitTestUtils::LoadHDAIntoNewMap(HDAName, Transform, bOpenWorld);
	if (!CreatedHAC)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load HDA %s into map. Missing uasset?"), *HDAName);
		return;
	}

	// 
	SetHAC(CreatedHAC);

	World = HAC->GetHACWorld();

	// Set time last so we don't include instantiation time.
	TimeStarted = FPlatformTime::Seconds();
}

void
FHoudiniTestContext::SetHAC(UHoudiniAssetComponent* HACToUse)
{
	if (HACToUse && HACToUse->GetCookable())
		return SetCookable(HACToUse->GetCookable());

	HAC = HACToUse;
	OutputDelegateHandle = HAC->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniAssetComponent* _HAC, bool  bSuccess)
	{
			this->CookingState = EHoudiniContextState::Complete;
	});
}


void
FHoudiniTestContext::SetCookable(UHoudiniCookable* HCToUse)
{
	HC = HCToUse;
	HAC = Cast<UHoudiniAssetComponent>(HCToUse->GetComponent());

	OutputDelegateHandle = HC->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniCookable* _HC, bool  bSuccess)
	{
		this->CookingState = EHoudiniContextState::Complete;
	});
}

UHoudiniAssetComponent*
FHoudiniTestContext::GetHAC()
{
	return HAC;
}

UHoudiniCookable* 
FHoudiniTestContext::GetCookable()
{
	return HC;
}

void
FHoudiniTestContext::GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const
{
	if (HC)
		HC->GetOutputs(OutOutputs);
	else if (HAC)
		HAC->GetOutputs(OutOutputs);
}

TArray<FHoudiniBakedOutput>&
FHoudiniTestContext::GetBakedOutputs()
{
	if (HC)
		return HC->GetBakedOutputs();
	else
		return HAC->GetBakedOutputs();
}


bool
FHoudiniTestContext::Bake(const FHoudiniBakeSettings& InBakeSettings)
{
	if (HC)
	{
		return FHoudiniEngineBakeUtils::BakeCookable(
			HC,
			InBakeSettings,
			HC->GetHoudiniEngineBakeOption(),
			HC->GetRemoveOutputAfterBake());
	}
	/*
	else if (HAC)
	{
		return FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
			HAC,
			InBakeSettings,
			HAC->GetHoudiniEngineBakeOption(),
			HAC->GetRemoveOutputAfterBake());
	}
	*/

	return false;
}

UHoudiniInput*
FHoudiniTestContext::GetInputAt(const int Idx)
{
	if (HC)
		return HC->GetInputAt(Idx);
	else
		return HAC->GetInputAt(Idx);
}

void
FHoudiniTestContext::SetProxyMeshEnabled(const bool bEnabled)
{
	if (HC)
	{
		HC->SetOverrideGlobalProxyStaticMeshSettings(true);
		HC->SetEnableProxyStaticMeshOverride(bEnabled);
	}		
	else
	{
		HAC->SetOverrideGlobalProxyStaticMeshSettings(true);
		HAC->SetEnableProxyStaticMeshOverride(bEnabled);
	}
}

FString
FHoudiniTestContext::GetBakeFolderOrDefault() const
{
	if (HC)
		return HC->GetBakeFolderOrDefault();
	else
		return HAC->GetBakeFolderOrDefault();
}

UWorld* 
FHoudiniTestContext::GetWorld() const
{
	if (HC)
		return HC->GetWorld();
	else
		return HAC->GetHACWorld();
}

UHoudiniPDGAssetLink*
FHoudiniTestContext::GetPDGAssetLink()
{
	if (HC)
		return HC->GetPDGAssetLink();
	else
		return HAC->GetPDGAssetLink();
}

FString
FHoudiniTestContext::GetTemporaryCookFolderOrDefault() const
{
	if (HC)
		return HC->GetTemporaryCookFolderOrDefault();
	else
		return HAC->GetTemporaryCookFolderOrDefault();
}

FHoudiniTestContext::~FHoudiniTestContext()
{
	if(HC)
		HC->GetOnPostOutputProcessingDelegate().Remove(OutputDelegateHandle);
	else
		HAC->GetOnPostOutputProcessingDelegate().Remove(OutputDelegateHandle);
}

bool 
FHoudiniTestContext::IsValid()
{
	return (HAC != nullptr || HC != nullptr);
}

TArray<AActor*>
FHoudiniEditorUnitTestUtils::GetOutputActors(TArray<FHoudiniBakedOutput>& BakedOutputs)
{
	TArray<AActor*> Results;
	for(auto & BakeOutput : BakedOutputs)
	{
		for(auto & OutputObject : BakeOutput.BakedOutputObjects)
		{
			if (!OutputObject.Value.Actor.IsEmpty())
			{
				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Value.Actor));
				if (IsValid(Actor))
				{
					Results.Add(Actor);
				}
			}
		}
	}
	return Results;
}

TArray<AActor*>
FHoudiniEditorUnitTestUtils::GetOutputInstancedActors(TArray<FHoudiniBakedOutput>& BakedOutputs)
{
	TArray<AActor*> Results;
	for(auto& BakeOutput : BakedOutputs)
	{
		for(auto& OutputObject : BakeOutput.BakedOutputObjects)
		{
			if(!OutputObject.Value.InstancedActors.IsEmpty())
			{
				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Value.InstancedActors[0]));
				if(IsValid(Actor))
				{
					Results.Add(Actor);
				}
			}
		}
	}
	return Results;
}

#endif