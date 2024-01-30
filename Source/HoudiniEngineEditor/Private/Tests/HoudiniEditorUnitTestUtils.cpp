#include "HoudiniEditorUnitTestUtils.h"
#include "FileHelpers.h"
#include "HoudiniAsset.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineEditorUtils.h"

#include "HoudiniParameter.h"
#include "HoudiniParameterInt.h"
#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "HoudiniAssetActorFactory.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HoudiniParameterToggle.h"

UHoudiniAssetComponent* FHoudiniEditorUnitTestUtils::LoadHDAIntoNewMap(const FString& PackageName, const FTransform& Transform)
{
	UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(TEXT("/Engine/Maps/Templates/Template_Default"), false);

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
	bool bDone = FFunctionLatentCommand::Update();
	return bDone;
}

void FHoudiniTestContext::StartCookingHDA()
{
	HAC->MarkAsNeedCook();
	bCookInProgress = true;
}


#endif