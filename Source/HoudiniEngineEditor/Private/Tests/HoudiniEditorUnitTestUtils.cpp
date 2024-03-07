#include "HoudiniEditorUnitTestUtils.h"
#include "FileHelpers.h"
#include "HoudiniAsset.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineEditorUtils.h"

#include "HoudiniParameter.h"
#include "HoudiniParameterInt.h"
#include "HoudiniPDGAssetLink.h"
#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "HoudiniAssetActorFactory.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniEngineOutputStats.h"
#include "HoudiniPDGManager.h"

UHoudiniAssetComponent* FHoudiniEditorUnitTestUtils::LoadHDAIntoNewMap(
	const FString& PackageName, 
	const FTransform& Transform, 
	bool bOpenWorld)
{
	FString MapName = bOpenWorld ? TEXT("/Engine/Maps/Templates/OpenWorld.umap") : TEXT("/Engine/Maps/Templates/Template_Default.umap");

	UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(MapName, false);

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

AActor* FHoudiniEditorUnitTestUtils::GetActorWithName(UWorld* World, FString& Name)
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


bool FHoudiniLatentTestCommand::Update()
{
	double DeltaTime = FPlatformTime::Seconds() - Context->TimeStarted;
	if (DeltaTime > Context->MaxTime)
	{
		Context->Test->AddError(FString::Printf(TEXT("***************** Test timed out After %.2f seconds*************"), DeltaTime));
		return true;
	}

	int CurrentFrame = GFrameCounter;
	if (Context->WaitTickFrame)
	{
		if (CurrentFrame < Context->WaitTickFrame)
		{
			return false;
		}
		else
		{
			Context->WaitTickFrame = 0;
		}

	}

	if (Context->bCookInProgress && IsValid(Context->HAC))
	{
		if (Context->bPostOutputDelegateCalled)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Cook Finished after %.2f seconds"), DeltaTime);
			Context->bCookInProgress = false;
			Context->bPostOutputDelegateCalled = false;
		}
		else
		{
			return false;
		}
	}

	if (Context->bPDGCookInProgress)
	{
		// bPDGPostCookDelegateCalled is set in a callback, so do the checking now. If set, continue,
		// else wait for it be set.
		if (Context->bPDGPostCookDelegateCalled)
		{
			HOUDINI_LOG_MESSAGE(TEXT("PDG Cook Finished after %.2f seconds"), DeltaTime);
			Context->bPDGCookInProgress = false;
			Context->bPDGPostCookDelegateCalled = false;
		}
		else
		{
			return false;
		}
	}

	bool bDone = FFunctionLatentCommand::Update();
	return bDone;
}

void FHoudiniTestContext::StartCookingHDA()
{
	HAC->MarkAsNeedCook();
	bCookInProgress = true;
	bPostOutputDelegateCalled = false;
}

void FHoudiniTestContext::WaitForTicks(int Count)
{
	WaitTickFrame = Count + GFrameCounter;
}


void FHoudiniTestContext::StartCookingSelectedTOPNetwork()
{
	UHoudiniPDGAssetLink * AssetLink = HAC->GetPDGAssetLink();
	UTOPNetwork* TopNetwork = AssetLink->GetSelectedTOPNetwork();

	this->bPDGPostCookDelegateCalled = false;

	TopNetwork->GetOnPostCookDelegate().AddLambda([this](UTOPNetwork* Link, bool bSuccess)
	{
		this->bPDGPostCookDelegateCalled = true;
		return;
	});

	FHoudiniPDGManager::CookOutput(TopNetwork);

	bPDGCookInProgress = true;
}

TArray<FHoudiniEngineBakedActor> FHoudiniTestContext::BakeSelectedTopNetwork()
{
	UHoudiniPDGAssetLink * PDGAssetLink = HAC->GetPDGAssetLink();

	FHoudiniBakedObjectData BakeOutputs;
	BakeOutputs;
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

FString FHoudiniEditorUnitTestUtils::GetAbsolutePathOfProjectFile(const FString& Object)
{
	FString Path =  FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FString File = Path + TEXT("/") + Object;
	FPaths::MakePlatformFilename(File);
	return File;

}

UHoudiniParameter* FHoudiniEditorUnitTestUtils::GetTypedParameter(UHoudiniAssetComponent* HAC, UClass* Class, const char* Name)
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

FHoudiniTestContext::FHoudiniTestContext(
	FAutomationTestBase* CurrentTest, 
	const FString & HDAName,
	const FTransform& Transform,
	bool bOpenWorld)
{
	Test = CurrentTest;

	// Load the HDA into a new map and kick start the cook. We do an initial cook to make sure the parameters are available.
	HAC = FHoudiniEditorUnitTestUtils::LoadHDAIntoNewMap(HDAName, Transform, bOpenWorld);
	if (!HAC)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load HDA %s into map. Missing uasset?"), *HDAName);
		return;
	}

	this->bCookInProgress = true;
	this->bPostOutputDelegateCalled = true;
	OutputDelegateHandle = HAC->GetOnPostOutputProcessingDelegate().AddLambda([this](UHoudiniAssetComponent* _HAC, bool  bSuccess)
	{
		this->bPostOutputDelegateCalled = true;
	});

	// Set time last so we don't include instantiation time.
	TimeStarted = FPlatformTime::Seconds();
}

FHoudiniTestContext::~FHoudiniTestContext()
{
	HAC->GetOnPostOutputProcessingDelegate().Remove(OutputDelegateHandle);
}

bool FHoudiniTestContext::IsValid()
{
	return HAC != nullptr;
}


#endif