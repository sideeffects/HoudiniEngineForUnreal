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
	UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(AssetData[0].GetAsset());

	UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
	if (!Factory)
		return nullptr;

	AActor* CreatedActor = Factory->CreateActor(Cast<UObject>(HoudiniAsset), World->GetCurrentLevel(), Transform);

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(CreatedActor->GetComponentByClass(UHoudiniAssetComponent::StaticClass()));
	return HAC;
}

bool FHoudiniEditorUnitTestUtils::IsHDAIdle(UHoudiniAssetComponent* HAC)
{
	if (HAC->NeedUpdate())
		return false;

	return 	HAC->GetAssetState() == EHoudiniAssetState::None;
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
		Context->Test->AddError(TEXT("Test timed out."));
		return true;
	}

	if (Context->bCookInProgress && IsValid(Context->HAC))
	{
		if (!FHoudiniEditorUnitTestUtils::IsHDAIdle(Context->HAC))
			return false;
		Context->bCookInProgress = false;
	}

	if (Context->bPDGCookInProgress)
	{
		// bPDGPostCookDelegateCalled is set in a callback, so do the checking now. If set, continue,
		// else wait for it be set.
		if (Context->bPDGPostCookDelegateCalled)
		{
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

	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	TArray<FHoudiniEngineBakedActor> BakedActors;

	FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(
		PDGAssetLink,
		PDGAssetLink->PDGBakeSelectionOption, 
		PDGAssetLink->PDGBakePackageReplaceMode, 
		PDGAssetLink->bRecenterBakedActors,
		PackagesToSave,
		BakeStats,
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

#endif