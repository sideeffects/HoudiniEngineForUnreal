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

#include "HoudiniEditorTestUtils.h"

#include "HoudiniEditorEquivalenceUtils.h"
#include "HoudiniPublicAPI.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIBlueprintLib.h"

#include "EngineUtils.h"
#include "HoudiniEngineCommands.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"

#include "Tests/AutomationEditorCommon.h"
#include "IAssetViewport.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Public/HAL/FileManager.h"
#include "Core/Public/HAL/PlatformFileManager.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Engine/Selection.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

const FVector2D FHoudiniEditorTestUtils::GDefaultEditorSize = FVector2D(1280, 720);
const FString FHoudiniEditorTestUtils::SavedPathFolder = "/Game/TestSaved/";
const FString FHoudiniEditorTestUtils::TempPathFolder = "/Game/TestTemp/";
const float FHoudiniEditorTestUtils::TimeoutTime = 180; // Timeout, in seconds
const FName FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName = TEXT("hapiunreal");

static TAutoConsoleVariable<int32> CVarHoudiniEngineSetupRegTest(
	TEXT("HoudiniEngine.SetupRegTests"),
	0,
	TEXT("If enabled, the regressions tests will set themselves up before being executed.\n")
	TEXT("The needed levels will automatically be created in the TestSaved directory with the default HDA.\n")
	TEXT("0: Disabled\n")
	TEXT("1: Enabled\n")
);

void FHoudiniAutomationTest::AddWarning(const FString& InWarning, int32 StackOffset)
{
	if (!UseSupressedWarnings)
	{
		FAutomationTestBase::AddWarning(InWarning, StackOffset);
	}
	else
	{
		if (!IsSupressedWarning(InWarning))
		{
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Warning, InWarning), StackOffset + 1);
		}
	}
}

void FHoudiniAutomationTest::AddSupressedWarning(const FString& InWarningPattern)
{
	SupressedWarnings.Add(InWarningPattern);
}

bool FHoudiniAutomationTest::IsSupressedWarning(const FString& InWarning)
{
	for (auto& Warning : SupressedWarnings)
	{
		FRegexPattern Pattern(Warning);
		FRegexMatcher ErrorMatcher(Pattern, InWarning);

		if (ErrorMatcher.FindNext())
		{
			return true;
		}
	}

	return false;
}

void FHoudiniEditorTestUtils::InitializeTests(FHoudiniAutomationTest* Test, const TFunction<void()>& OnSuccess, const TFunction<void()>& OnFail)
{
	LoadMap(Test, TEXT("/Game/TestLevel"), [=]
	{
		CreateSessionIfInvalidWithLatentRetries(Test, HoudiniEngineSessionPipeName, OnSuccess, OnFail);
	});
}

UObject* FHoudiniEditorTestUtils::FindAssetUObject(FHoudiniAutomationTest* Test, const FName AssetUObjectPath)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByPackageName(AssetUObjectPath, AssetData);
	if (AssetData.Num() > 0)
	{
		return AssetData[0].GetAsset();
	}

	if (Test != nullptr)
	{
		Test->AddError(FString::Printf(TEXT("Could not find UObject: %s"), *AssetUObjectPath.ToString()));
	}

	return nullptr;
}

void FHoudiniEditorTestUtils::InstantiateAsset(FHoudiniAutomationTest* Test,
	const FName AssetUObjectPath, const FPostTestHDAInstantiationCallback & OnFinishInstantiate,
	const bool ErrorOnFail, const FString DefaultCookFolder,
	const FPreInstantiationCallback& OnPreInstantiation)
{
	SetUseLessCPUInTheBackground();

	UHoudiniPublicAPI* HoudiniAPI = UHoudiniPublicAPIBlueprintLib::GetAPI();
	if (HoudiniAPI == nullptr || !IsValid(HoudiniAPI) || HoudiniAPI->IsUnreachable())
	{
		UE_LOG(LogTemp, Error, TEXT("HoudiniAPI was not instantiated! Call FHoudiniEditorTestUtils::InitializeTest"));
		OnFinishInstantiate(nullptr, false);
		return;
	}

	UHoudiniAsset * HoudiniAsset = Cast<UHoudiniAsset>(FindAssetUObject(Test, AssetUObjectPath));

	if (!HoudiniAsset)
	{
		Test->AddError(FString::Printf(TEXT("Could not find UObject: %s"), *AssetUObjectPath.ToString()));
		OnFinishInstantiate(nullptr, false);
		return;
	}

	SetDefaultCookFolder(DefaultCookFolder);

	UHoudiniEditorTestObject* TestObject = GetTestObject();
	TestObject->ResetFields();

	TestObject->IsInstantiating = true;

	const FTransform Location = FTransform::Identity;
	UHoudiniPublicAPIAssetWrapper* Wrapper = HoudiniAPI->InstantiateAsset(HoudiniAsset, Location);
	TestObject->InAssetWrappers.Add(Wrapper); // Need to assign it to TestObject otherwise it will be garbage collected!!!

	// Bind delegates from the asset wrapper to UHoudiniEditorTestObject which we use to proxy to non-dynamic delegates
	// like OnPreInstantiation.
	Wrapper->GetOnPreInstantiationDelegate().AddUniqueDynamic(TestObject, &UHoudiniEditorTestObject::PreInstantiationCallback);
	Wrapper->GetOnPostInstantiationDelegate().AddUniqueDynamic(TestObject, &UHoudiniEditorTestObject::PostInstantiationCallback);
	Wrapper->GetOnPostCookDelegate().AddUniqueDynamic(TestObject, &UHoudiniEditorTestObject::PostCookCallback);
	Wrapper->GetOnPostProcessingDelegate().AddUniqueDynamic(TestObject, &UHoudiniEditorTestObject::PostProcessingCallback);

	if (OnPreInstantiation)
		TestObject->OnPreInstantiationDelegate.AddLambda(OnPreInstantiation);

	Wrapper->Recook(); // Make sure the callback is called!

	UHoudiniAssetComponent* HoudiniComponent = Wrapper->GetHoudiniAssetComponent();

	const double StartTime = FPlatformTime::Seconds();

	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		const bool IsInstantiating = TestObject->IsInstantiating;
		const bool CookSuccessfulResult = TestObject->InCookSuccess;

		const double CurrentTime = FPlatformTime::Seconds();

		if (IsInstantiating == false && Wrapper->GetHoudiniAssetComponent()->GetAssetState() == EHoudiniAssetState::None)
		{
			if (ErrorOnFail && CookSuccessfulResult == false)
			{
				Test->AddError(FString::Printf(TEXT("Cook was unsuccessful: %s"), *AssetUObjectPath.ToString()));
			}

			// Sometimes hangs if things happen too fast. Hacky way to fix this!
			if (CurrentTime - StartTime < 1)
			{
				return false;
			}

			ForceRefreshViewport();

			// Only complete once the asset has cooked the expected number of times, or if a cook fails (depending on
			// ErrorOnFail). Cook success/failure is updated after each cook
			if (TestObject->HasReachedExpectedCookCount() || (ErrorOnFail && !CookSuccessfulResult))
			{
				SetDefaultCookFolder(TEXT("/Game/HoudiniEngine/Temp"));
				TestObject->ResetFields();

				OnFinishInstantiate(Wrapper, CookSuccessfulResult);
				return true;
			}
		}

		if (CurrentTime - StartTime > TimeoutTime)
		{
			Test->AddError("Instantiation timeout!");
			OnFinishInstantiate(Wrapper, CookSuccessfulResult);
			return true;
		}

		return false;
	}
	));
}

void FHoudiniEditorTestUtils::SetUseLessCPUInTheBackground()
{
	// Equivalent of setting Edit > Editor Preferences > General > Performance > "Use less CPU when in background" is OFF
	// This ensures that objects are rendered even in the background
	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bThrottleCPUWhenNotForeground = false;
	Settings->bMonitorEditorPerformance = false;
	Settings->PostEditChange();
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::GetMainFrameWindow()
{

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		// Check if the main frame is loaded. When using the old main frame it may not be.
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		return MainFrame.GetParentWindow();
	}

	return nullptr;
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::GetActiveTopLevelWindow()
{
	return FSlateApplication::Get().GetActiveTopLevelWindow();
}

static bool ShouldShowProperty(const FPropertyAndParent& PropertyAndParent, bool bHaveTemplate)
{
	const FProperty& Property = PropertyAndParent.Property;

	if (bHaveTemplate)
	{
		const UClass* PropertyOwnerClass = Property.GetOwner<const UClass>();
		const bool bDisableEditOnTemplate = PropertyOwnerClass
			&& PropertyOwnerClass->IsNative()
			&& Property.HasAnyPropertyFlags(CPF_DisableEditOnTemplate);
		if (bDisableEditOnTemplate)
		{
			return false;
		}
	}
	return true;
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::CreateNewDetailsWindow()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		// Check if the main frame is loaded. When using the old main frame it may not be.
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		USelection* SelectedActors = GEditor->GetSelectedActors();
		TArray<UObject*> Actors;
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AActor* Actor = Cast<AActor>(*Iter);
			if (Actor)
			{
				Actors.Add(Actor);
			}
		}

		TSharedRef<SWindow> Details = PropertyEditorModule.CreateFloatingDetailsView(Actors, false);//

		return Details;
	}

	return nullptr;
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::CreateViewportWindow()
{
	if (!FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		return nullptr;
	}

	TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
		.Title(NSLOCTEXT("ViewportEditor", "WindowTitle", "Viewport Editor"))
		.ClientSize(FVector2D(400, 550));

	// If the main frame exists parent the window to it
	TSharedPtr< SWindow > ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild(NewSlateWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(NewSlateWindow);
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<class IAssetViewport> Viewport = LevelEditor.GetFirstActiveViewport();

	NewSlateWindow->SetContent(
		SNew(SBorder)
		[
			Viewport->AsWidget()
		]
	);

	return NewSlateWindow;
}

void FHoudiniEditorTestUtils::TakeScreenshotEditor(FHoudiniAutomationTest* Test, const FString ScreenshotName, const EEditorScreenshotType EditorScreenshotType, const FVector2D Size)
{
	// Wait one frame just in case for pending redraws
	Test->AddCommand(new FDelayedFunctionLatentCommand([=]()
	{
		TSharedPtr<SWindow> ScreenshotWindow;

		bool DestroyWindowOnEnd = false;

		switch (EditorScreenshotType)
		{
		case EEditorScreenshotType::ENTIRE_EDITOR:
			UE_LOG(LogTemp, Warning, TEXT("Entire editor screenshots don't work well with build machines. Use sparingly!"));
			ScreenshotWindow = GetMainFrameWindow(); // Entire editor doesn't work well with build machines!
			break;
		case EEditorScreenshotType::ACTIVE_WINDOW:
			ScreenshotWindow = GetActiveTopLevelWindow();
			break;
		case EEditorScreenshotType::DETAILS_WINDOW:
			ScreenshotWindow = CreateNewDetailsWindow();
			DestroyWindowOnEnd = true;
			break;
		case EEditorScreenshotType::VIEWPORT:
			ScreenshotWindow = CreateViewportWindow();
			DestroyWindowOnEnd = true;
			break;
		default:
			break;
		}

		if (!ScreenshotWindow)
		{
			return;
		}

		WindowScreenshotParameters ScreenshotParameters;
		ScreenshotParameters.ScreenshotName = ScreenshotName;
		ScreenshotParameters.CurrentWindow = ScreenshotWindow;

		// Creates a file in Engine\Saved\Automation\Tmp
		TSharedRef<SWidget> WidgetToFind = ScreenshotWindow.ToSharedRef();

		bool ScreenshotSuccessful;

		TArray<FColor> OutImageData;
		FIntVector OutImageSize;

		bool bRenderOffScreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"));

		if (!bRenderOffScreen)
		{
			// Take a screenshot like a normal person
			// Note that this sizing method is slightly different than the offscreen one, so DO NOT copy the result to dev_reg
			ScreenshotWindow->Resize(Size);
			ScreenshotSuccessful = FSlateApplication::Get().TakeScreenshot(WidgetToFind, OutImageData, OutImageSize);
		}
		else
		{
			// Rendering offscreen results in a slightly different render pipeline.
			// Resizing as usual doesn't seem to work unless we do it in this very specific way
			// Mostly copied from  FSlateApplication::Get().TakeScreenshot(WindowRef, OutImageData, OutImageSize) , but forces size
			FWidgetPath WidgetPath;
			FSlateApplication::Get().GeneratePathToWidgetChecked(WidgetToFind, WidgetPath);

			FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(WidgetToFind).Get(FArrangedWidget::GetNullWidget());
			FVector2f Position = ArrangedWidget.Geometry.AbsolutePosition;
			FVector2D WindowPosition = ScreenshotWindow->GetPositionInScreen();

			FIntRect ScreenshotRect = FIntRect(0, 0, (int32)Size.X, (int32)Size.Y);

			ScreenshotRect.Min.X += (Position.X - WindowPosition.X);
			ScreenshotRect.Min.Y += (Position.Y - WindowPosition.Y);
			ScreenshotRect.Max.X += (Position.X - WindowPosition.X);
			ScreenshotRect.Max.Y += (Position.Y - WindowPosition.Y);

			FSlateApplication::Get().GetRenderer()->RequestResize(ScreenshotWindow, Size.X, Size.Y);
			ScreenshotWindow->Resize(Size);
			FSlateApplication::Get().ForceRedrawWindow(ScreenshotWindow.ToSharedRef());

			FSlateApplication::Get().GetRenderer()->PrepareToTakeScreenshot(ScreenshotRect, &OutImageData, ScreenshotWindow.Get());
			FSlateApplication::Get().ForceRedrawWindow(ScreenshotWindow.ToSharedRef());
			ScreenshotSuccessful = (ScreenshotRect.Size().X != 0 && ScreenshotRect.Size().Y != 0 && OutImageData.Num() >= ScreenshotRect.Size().X * ScreenshotRect.Size().Y);
			OutImageSize.X = ScreenshotRect.Size().X;
			OutImageSize.Y = ScreenshotRect.Size().Y;
		}

		if (!ScreenshotSuccessful)
		{
			Test->AddError("Taking screenshot not successful!");
			return;
		}

		FAutomationScreenshotData Data;
		Data.Width = OutImageSize.X;
		Data.Height = OutImageSize.Y;
		Data.ScreenshotName = ScreenshotName;

		// Sometimes errors on 4.25 if not offscreen
		if (bRenderOffScreen)
		{
			FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound(OutImageData, Data);
		}

		WaitForScreenshotAndCopy(Test, ScreenshotName, [=] (FHoudiniAutomationTest* AutomationTest, FString BaseName)
		{
			CopyScreenshotToTestFolder(AutomationTest, BaseName);

			if (DestroyWindowOnEnd)
			{
				ScreenshotWindow->RequestDestroyWindow();
			}
		});

	}, 0.1f));
}

void FHoudiniEditorTestUtils::TakeScreenshotViewport(FHoudiniAutomationTest* Test, const FString ScreenshotName)
{
	Test->AddCommand(new FDelayedFunctionLatentCommand([=]()
	{
		const FString BaseName = ScreenshotName;
		const FString ScreenshotPath = GetUnrealTestDirectory() + BaseName;
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
		ForceRefreshViewport();

		WaitForScreenshotAndCopy(Test, BaseName, CopyScreenshotToTestFolder);
	}, 0.1f));
}

UHoudiniAssetComponent* FHoudiniEditorTestUtils::GetAssetComponentWithName(const FString Name)
{
	TArray<AHoudiniAssetActor*> Out;

	for (TActorIterator<AHoudiniAssetActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
	{
		if (It->GetName() == Name)
		{
			Out.Add(*It);
		}
	}

	if (Out.Num() > 0)
	{
		return Out[0]->GetHoudiniAssetComponent();
	}

	return nullptr;
}

bool FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(
	FHoudiniAutomationTest* InTest, UHoudiniPublicAPIAssetWrapper* const InAssetWrapper,
	UHoudiniEditorTestObject* const InTestObject, const bool bInAddTestErrors)
{
	bool bHasErrors = false;
	if (!IsValid(InAssetWrapper))
	{
		if (bInAddTestErrors)
			InTest->AddError(TEXT("[FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject] Asset wrapper is invalid!"));
		bHasErrors = true;
	}

	if (!IsValid(InTestObject))
	{
		if (bInAddTestErrors)
			InTest->AddError(TEXT("[FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject] Test object is invalid!"));
		bHasErrors = true;
	}

	return !bHasErrors;
}

bool FHoudiniEditorTestUtils::CheckAssetWrapperTestObjectAndGetValidHAC(
	FHoudiniAutomationTest* InTest, UHoudiniPublicAPIAssetWrapper* const InAssetWrapper,
	UHoudiniEditorTestObject* const InTestObject, UHoudiniAssetComponent*& OutHAC, const bool bInAddTestErrors)
{
	if (!CheckAssetWrapperAndTestObject(InTest, InAssetWrapper, InTestObject, bInAddTestErrors))
		return false;

	UHoudiniAssetComponent* const HAC = InAssetWrapper->GetHoudiniAssetComponent();
	if (!IsValid(HAC))
	{
		if (bInAddTestErrors)
			InTest->AddError(TEXT("[FHoudiniEditorTestUtils::CheckAssetWrapperTestObjectAndGetValidHAC] HAC is invalid!"));
		return false;
	}

	OutHAC = HAC;
	return true;
}

void FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(
		FHoudiniAutomationTest* const InTest,
		UHoudiniPublicAPIAssetWrapper const * const InAssetWrapper,
		const FString& InParamType,
		const FName& InParamTupleName,
		const FString& InValueAsString,
		const int32 InIndex)
{
	FString ErrorMessage;
	if (IsValid(InAssetWrapper))
		InAssetWrapper->GetLastErrorMessage(ErrorMessage);
	else
		ErrorMessage = TEXT("InAssetWrapper is null");

	InTest->AddError(FString::Printf(
		TEXT("API call to set %s parameter '%s' at index %d to %s failed: %s."),
		*InParamType, *InParamTupleName.ToString(), InIndex, *InValueAsString, *ErrorMessage));
}

void FHoudiniEditorTestUtils::SetupDifferentialTest(
	FHoudiniAutomationTest* Test, 
	const FString MapName,
	const FString HDAAssetPath, 
	const FString ActorName, 
	const TFunction<void(bool)>& OnFinishedCallback,
	const FPreInstantiationCallback& OnPreInstantiation,
	const FPostDiffTestHDAInstantiationCallback& OnInstantiationFinishedCallback)
{
	const FString SavedLevelPath = GetSavedLevelPath(MapName);
	const FString SavedAssetsPath = GetSavedAssetsPath(MapName, ActorName);

	const FString TempLevelPath = GetTempLevelPath(MapName);
	const FString TempAssetsPath = GetTempAssetsPath(MapName, ActorName);

	UObject * AssetObject = FHoudiniEditorTestUtils::FindAssetUObject(nullptr, FName(SavedLevelPath));
	bool Valid = AssetObject != nullptr;
	
	// If we don't have a Saved level, then create it, and end the test here. 
	if (!Valid)
	{
		// Save map, load it
		FHoudiniEditorTestUtils::CreateAndLoadNewLevelWithAsset(
			Test,
			SavedLevelPath,
			HDAAssetPath,
			ActorName,
			[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
			{
				Test->AddWarning("Created a new scene because it didn't exist! Please commit to dev_reg: " + SavedLevelPath + " and " + SavedAssetsPath);
				if (OnFinishedCallback != nullptr)
					OnFinishedCallback(false);
			},
			SavedAssetsPath,
			OnPreInstantiation,
			OnInstantiationFinishedCallback
		);

		return;
	}

	// Still need to check to see if the asset exists in the map, because I want you to be able to create multiple tests using the same saved scene
	LoadMap(Test, SavedLevelPath, [=]
	{
		UHoudiniAssetComponent* SceneHAC = FHoudiniEditorTestUtils::GetAssetComponentWithName(ActorName);
		if (SceneHAC == nullptr)
		{
			Test->AddWarning("Cannot find asset in saved scene. Instantiating the asset..." + SavedLevelPath);

			FHoudiniEditorTestUtils::InstantiateAsset(
				Test,
				FName(HDAAssetPath),
				[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
				{
					if (!IsSuccessful)
					{
						Test->AddError("Instantiation unsuccessful!");
						if (OnFinishedCallback != nullptr)
							OnFinishedCallback(false);
						return;
					}

					InAssetWrapper->GetHoudiniAssetActor()->SetActorLabel(ActorName);

					auto ContinueTests = [=](bool bIsSuccessful)
					{
						if (!bIsSuccessful)
						{
							if (OnFinishedCallback != nullptr)
								OnFinishedCallback(false);
							return;
						}
					
						if (!SaveCurrentLevel())
						{
							Test->AddError("Unable to save level!");
							if (OnFinishedCallback != nullptr)
								OnFinishedCallback(false);
							return;
						}

						if (OnFinishedCallback != nullptr)
							OnFinishedCallback(true);

						Test->AddWarning("Instantiated asset into level. Please commit to dev_reg: " + SavedLevelPath + " and " + SavedAssetsPath);
					};

					if (OnInstantiationFinishedCallback != nullptr)
					{
						OnInstantiationFinishedCallback(InAssetWrapper, ContinueTests, false, SavedAssetsPath);
					}
					else
					{
						ContinueTests(true);
					}
				},
				true,
				SavedAssetsPath,
				OnPreInstantiation
			);

			return;
		}

		// Now actually run the differential test
		RunDifferentialTest(Test, MapName, HDAAssetPath, ActorName, OnFinishedCallback, OnPreInstantiation, OnInstantiationFinishedCallback);		
	});
}


void FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
	FHoudiniAutomationTest* Test,
	const FString MapName,
	const FString HDAAssetPath,
	const FString ActorName,
	const TFunction<void(bool)>& OnFinishedCallback,
	const FPreInstantiationCallback& OnPreInstantiationCallback,
	const FPostDiffTestHDAInstantiationCallback& OnPostInstantiationCallback)
{
	bool bSetupTest = CVarHoudiniEngineSetupRegTest.GetValueOnAnyThread() == 1 ? true : false;

	if (bSetupTest)
	{
		SetupDifferentialTest(
			Test, MapName, HDAAssetPath, ActorName,
			OnFinishedCallback, OnPreInstantiationCallback, OnPostInstantiationCallback);
	}
	else
	{
		RunDifferentialTest(
			Test, MapName, HDAAssetPath, ActorName,
			OnFinishedCallback, OnPreInstantiationCallback, OnPostInstantiationCallback);
	}
}

void FHoudiniEditorTestUtils::RunDifferentialTest(
	FHoudiniAutomationTest* Test,
	const FString MapName,
	const FString HDAAssetPath,
	const FString ActorName,
	const TFunction<void(bool)>& OnFinishedCallback,
	const FPreInstantiationCallback& OnPreInstantiationCallback,
	const FPostDiffTestHDAInstantiationCallback& OnPostInstantiationCallback)
{

	// Suppress warnings - This is so that the output file is more readable

	// Warning that happens for most HDA instantiations
	Test->AddSupressedWarning("loading from Memory: source asset file not found.");

	// Warning that often happens for landscapes
	Test->AddSupressedWarning("RelativeLocation disagrees with its section base, attempted automated fix");

	// We don't care about landscape data resize warning
	Test->AddSupressedWarning("Landscape data had to be resized from");

	// Warning that sometimes happens when switching maps
	Test->AddSupressedWarning("Destroying .* which doesn't have a valid world pointer");
	
	const FString SavedLevelPath = GetSavedLevelPath(MapName);
	const FString SavedAssetsPath = GetSavedAssetsPath(MapName, ActorName);

	const FString TempLevelPath = GetTempLevelPath(MapName);
	const FString TempAssetsPath = GetTempAssetsPath(MapName, ActorName);

	UObject * AssetObject = FHoudiniEditorTestUtils::FindAssetUObject(nullptr, FName(SavedLevelPath));

	// If we don't have a Saved level, end the test here. 
	if (AssetObject == nullptr)
	{
		Test->AddError("Unable to find the asset in the saved level: " + SavedLevelPath);
		if (OnFinishedCallback != nullptr)
			OnFinishedCallback(false);
		return;
	}

	// Load the saved level that contains the cached HDA
	LoadMap(Test, SavedLevelPath, [=]
	{
		UHoudiniAssetComponent* SceneHAC = FHoudiniEditorTestUtils::GetAssetComponentWithName(ActorName);
		if (SceneHAC == nullptr)
		{
			Test->AddError("Unable to find the asset in the saved level: " + SavedLevelPath);
			if (OnFinishedCallback != nullptr)
				OnFinishedCallback(false);
			return;
		}

		const bool bErrorOnFail = true;
		const FString DefaultCookFolder = "/Game/HoudiniEngine/Temp";
		FHoudiniEditorTestUtils::InstantiateAsset(
			Test,
			FName(HDAAssetPath),
			[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
		{
			if (!IsSuccessful)
			{
				Test->AddError("Instantiation unsuccessful!");
				if (OnFinishedCallback != nullptr)
					OnFinishedCallback(false);
				return;
			}

			InAssetWrapper->GetHoudiniAssetActor()->SetActorLabel(ActorName + "_Compare");

			auto ContinueTests = [=](const bool bIsSuccessful)
			{
				if (!FHoudiniEditorEquivalenceUtils::IsEquivalent(InAssetWrapper->GetHoudiniAssetComponent(), SceneHAC))
				{
					Test->AddError(FString::Printf(
						TEXT("HDA is not equivalent. If this change is intended, then DELETE %s and %s and MOVE %s and %s to those locations, fix redirectors in the TestCached folder from the content browser and commit to dev_reg. Do not copy/paste as it will break essential DuplicateTransient references like OutputObjects."),
						*SavedLevelPath, *SavedAssetsPath, *TempLevelPath, *TempAssetsPath));

					CreateTestTempLevel(Test, MapName, HDAAssetPath, ActorName, OnFinishedCallback, OnPreInstantiationCallback, OnPostInstantiationCallback);

					return;
				}

				if (OnFinishedCallback != nullptr)
					OnFinishedCallback(true);
			};

			if (OnPostInstantiationCallback != nullptr)
			{
				OnPostInstantiationCallback(InAssetWrapper, ContinueTests, false, DefaultCookFolder);
			}
			else
			{
				ContinueTests(true);
			}

		}, bErrorOnFail, DefaultCookFolder, OnPreInstantiationCallback
		);
	});
}


void FHoudiniEditorTestUtils::CreateTestTempLevel(
	FHoudiniAutomationTest* Test,
	const FString MapName,
	const FString HDAAssetPath,
	const FString ActorName,
	const TFunction<void(bool)>& OnFinishedCallback,
	const FPreInstantiationCallback& OnPreInstantiationCallback,
	const FPostDiffTestHDAInstantiationCallback& OnPostInstantiationCallback)
{
	//const FString SavedLevelPath = GetSavedLevelPath(MapName);
	//const FString SavedAssetsPath = GetSavedAssetsPath(MapName, ActorName);

	const FString TempLevelPath = GetTempLevelPath(MapName);
	const FString TempAssetsPath = GetTempAssetsPath(MapName, ActorName);

	LoadMap(Test, TEXT("/Game/TestLevel"), [=]
	{
		// Cook the object in the "Cached" Level.
		// Note: The reason why we do this is because we can't use the map "Save As" functionality, because it counts as duplicating
		// and will remove essential Transient properties like OutputObjects
		UObject * AssetObject = FHoudiniEditorTestUtils::FindAssetUObject(nullptr, FName(TempLevelPath));
		bool Valid = AssetObject != nullptr;

		// If the scene doesn't exists, then create a new one.
		if (!Valid)
		{
			// Create the cached version of the map
			FHoudiniEditorTestUtils::CreateAndLoadNewLevelWithAsset(
				Test,
				TempLevelPath,
				HDAAssetPath,
				ActorName,
				/*[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
				{
					RunDifferentialTest(Test, MapName, HDAAssetPath, ActorName, OnFinishedCallback, OnPreInstantiation, OnInstantiationFinishedCallback);
				},*/
				[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful) { return; },
				TempAssetsPath,
				OnPreInstantiationCallback,
				OnPostInstantiationCallback
				);
		}
		else
		{
			// If the scene does exist, then load it, delete the existing object, cook the HDA, and replace the result in the Level
			// and save it
			LoadMap(Test, TempLevelPath, [=]
			{
				DeleteHACWithNameInLevelAndSave(ActorName);

				FHoudiniEditorTestUtils::InstantiateAsset(
					Test,
					FName(HDAAssetPath),
					[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
					{
						if (!IsSuccessful)
						{
							Test->AddError("Instantiation unsuccessful!");
							if (OnFinishedCallback != nullptr)
								OnFinishedCallback(false);
							return;
						}

						InAssetWrapper->GetHoudiniAssetActor()->SetActorLabel(ActorName);

						auto ContinueTests = [=](bool bIsSuccessful)
						{
							if (!bIsSuccessful)
							{
								if (OnFinishedCallback != nullptr)
									OnFinishedCallback(false);
								return;
							}

							if (!SaveCurrentLevel())
							{
								Test->AddError("Unable to save level!");
								if (OnFinishedCallback != nullptr)
									OnFinishedCallback(false);
								return;
							}

							//RunDifferentialTest(Test, MapName, HDAAssetPath, ActorName, OnFinishedCallback, OnPreInstantiation, OnInstantiationFinishedCallback);
						};

						if (OnPostInstantiationCallback != nullptr)
						{
							OnPostInstantiationCallback(InAssetWrapper, ContinueTests, true, TempAssetsPath);
						}
						else
						{
							ContinueTests(true);
						}

					},
					true,
					TempAssetsPath,
						OnPreInstantiationCallback
				);
			});
		}
	});
}


bool FHoudiniEditorTestUtils::CreateAndLoadNewLevelWithAsset(FHoudiniAutomationTest* Test, const FString MapAssetPath,
	const FString HDAAssetPath, const FString ActorName, const FPostTestHDAInstantiationCallback & OnFinishInstantiate, FString DefaultCookFolder,
	const FPreInstantiationCallback& OnPreInstantiationCallback,
	const FPostDiffTestHDAInstantiationCallback & OnPostInstantiation)
{
	// Create a new default map! dont reuse the existing one!
	UEditorLoadingAndSavingUtils::NewMapFromTemplate(TEXT("/Engine/Maps/Templates/Template_Default"), false);

	UWorld * World = GEditor->GetEditorWorldContext().World();

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapAssetPath))
	{
		Test->AddError("Cannot save world!");
		OnFinishInstantiate(nullptr, false);
		return false;
	}

	LoadMap(Test, MapAssetPath, [=]
	{
		FHoudiniEditorTestUtils::InstantiateAsset(Test, FName(HDAAssetPath),
			[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool bIsSuccessful)
		{
			if (!bIsSuccessful)
			{
				Test->AddError("Instantiation unsuccessful!");
				OnFinishInstantiate(nullptr, false);
				return;
			}

			InAssetWrapper->GetHoudiniAssetActor()->SetActorLabel(ActorName);

			auto ContinueTests = [=](const bool bIsSuccessful)
			{
				if (!bIsSuccessful)
				{
					OnFinishInstantiate(nullptr, false);
					return;
				}

				if (!SaveCurrentLevel())
				{
					Test->AddError("Unable to save level!");
					OnFinishInstantiate(nullptr, false);
					return;
				}

				OnFinishInstantiate(InAssetWrapper, bIsSuccessful);
			};

			if (OnPostInstantiation != nullptr)
			{
				OnPostInstantiation(InAssetWrapper, ContinueTests, true, DefaultCookFolder);
			}
			else
			{
				ContinueTests(true);
			}

		}, true, DefaultCookFolder, OnPreInstantiationCallback);
	});

	return true;
}


void FHoudiniEditorTestUtils::WaitForScreenshotAndCopy(FHoudiniAutomationTest* Test, FString BaseName, const TFunction<void(FHoudiniAutomationTest*, FString)>& OnScreenshotGenerated)
{
	const FString TestDirectory = GetUnrealTestDirectory();
	const FString FileName = TestDirectory + BaseName;

	// Wait for screenshot to finish generating, and then copy to $RT/hapi/unreal/
	const double StartTime = FPlatformTime::Seconds();

	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();

		if (FileManager.FileExists(*FileName))
		{
			OnScreenshotGenerated(Test, BaseName);
			return true;
		}
		else
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime - StartTime > TimeoutTime)
			{
				Test->AddError("WaitForScreenshotAndCopy timeout!");
				return true;
			}

			ForceRefreshViewport();
			return false;
		}
	}));
}

void FHoudiniEditorTestUtils::CopyScreenshotToTestFolder(FHoudiniAutomationTest* Test, FString BaseName)
{
	const FString TestDirectory = GetUnrealTestDirectory();
	const FString FileName = TestDirectory + BaseName;
	FString DestFileName = GetTestDirectory();
	if (!DestFileName.IsEmpty())
	{
		DestFileName += FormatScreenshotOutputName(BaseName);
	}

	IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();

	// Copy output file to our test directory, if it exists.
	if (!DestFileName.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose, TEXT("Copied file to: %s"), *DestFileName);
		FileManager.CopyFile(*DestFileName, *FileName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to copy file!"));
	}

	const double StartTime = FPlatformTime::Seconds();

	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		IPlatformFile& CopyFileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (CopyFileManager.FileExists(*FileName))
		{
			return true;
		}
		else
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime - StartTime > TimeoutTime)
			{
				Test->AddError("CopyScreenshotToTestFolder timeout!");
				return true;
			}
			return false;
		}
	}));
}

FString FHoudiniEditorTestUtils::GetTestDirectory()
{
	return FPlatformMisc::GetEnvironmentVariable(TEXT("TEST_OUTPUT_DIR")) + FPlatformMisc::GetDefaultPathSeparator();
}

FString FHoudiniEditorTestUtils::GetUnrealTestDirectory()
{
	//return FPaths::AutomationDir() + "/Incoming/"; // 4.25
	return FPaths::AutomationTransientDir(); // 4.26
}

FString FHoudiniEditorTestUtils::FormatScreenshotOutputName(FString BaseName)
{
	const FString Prefix = FPlatformMisc::GetEnvironmentVariable(TEXT("TEST_PREFIX"));
	return FString::Printf(TEXT("%s_%s"), *Prefix, *BaseName);
}

void FHoudiniEditorTestUtils::ForceRefreshViewport()
{
	// Force redraws viewport even if not in focus to prevent hanging
	FLevelEditorModule &PropertyEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		PropertyEditorModule.BroadcastRedrawViewports(false);
	}
}

void FHoudiniEditorTestUtils::SetDefaultCookFolder(const FString Folder)
{
	FString FolderToUse = Folder;
	if (Folder.IsEmpty())
	{
		FolderToUse = TEXT("/Game/HoudiniEngine/Temp");
	}

	UHoudiniRuntimeSettings* const Settings = GetMutableDefault<UHoudiniRuntimeSettings>();
	if (IsValid(Settings))
	{
		Settings->DefaultTemporaryCookFolder = FolderToUse;
	}
}

FString FHoudiniEditorTestUtils::GetSavedLevelPath(const FString MapName)
{
	return SavedPathFolder + "Levels/" + MapName;
}

FString FHoudiniEditorTestUtils::GetSavedAssetsPath(const FString MapName, const FString ActorName)
{
	return SavedPathFolder + "Assets/" + MapName + "/" + ActorName;
}

FString FHoudiniEditorTestUtils::GetTempLevelPath(const FString MapName)
{
	return TempPathFolder + "Levels/" + MapName;
}

FString FHoudiniEditorTestUtils::GetTempAssetsPath(const FString MapName, const FString ActorName)
{
	return TempPathFolder + "Assets/" + MapName + "/" + ActorName;
}

void FHoudiniEditorTestUtils::LoadMap(FHoudiniAutomationTest* Test, const FString Path, const TFunction<void()>& OnLoadedMap)
{
	const double StartTime = FPlatformTime::Seconds();

	bool * HasLoadedMap = new bool(false);

	FDelegateHandle Handle = FEditorDelegates::OnMapOpened.AddLambda(
	[=](const FString& Filename, bool bAsTemplate)
	{
		*HasLoadedMap = true;
	});
	
	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		const double CurrentTime = FPlatformTime::Seconds();
		// Hacky way to allocate some time before and after loading map to prevent build machine from hanging!
		if (*HasLoadedMap == false && CurrentTime - StartTime > 1)
		{
			FAutomationEditorCommonUtils::LoadMap(Path);
			return false;
		}

		if (*HasLoadedMap == true && CurrentTime - StartTime > 2)
		{
			FEditorDelegates::OnMapOpened.Remove(Handle);

			if (OnLoadedMap != nullptr)
			{
				OnLoadedMap();
			}

			delete HasLoadedMap;

			return true;
		}
		
		return false;
	}));
}

void FHoudiniEditorTestUtils::DeleteHACWithNameInLevelAndSave(const FString Name)
{
	UHoudiniAssetComponent* SceneHAC = FHoudiniEditorTestUtils::GetAssetComponentWithName(Name);
	if (SceneHAC != nullptr)
	{
		SceneHAC->GetOwner()->MarkAsGarbage();
		GEditor->ForceGarbageCollection(true);

		SaveCurrentLevel();
	}
}



UHoudiniEditorTestObject* FHoudiniEditorTestUtils::GetTestObject()
{
	static UHoudiniEditorTestObject* Obj = NewObject<UHoudiniEditorTestObject>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	return Obj;
}

// Copied from UEditorLoadingAndSavingUtils::SaveCurrentLevel from 4.25. For some reason, it always returns false in 4.26+, so make our own version.
bool FHoudiniEditorTestUtils::SaveCurrentLevel()
{
	//return UEditorLoadingAndSavingUtils::SaveCurrentLevel(); // Original in 4.25

	bool bReturnCode = false;

	ULevel* Level = GWorld->GetCurrentLevel();
	if (Level && FEditorFileUtils::PromptToCheckoutLevels(false, Level))
	{
		bReturnCode = FEditorFileUtils::SaveLevel(Level);
	}

	return bReturnCode;
}

void FHoudiniEditorTestUtils::RecookAndWait(FHoudiniAutomationTest* Test, UHoudiniPublicAPIAssetWrapper* InAssetWrapper,
	const FPostTestHDAInstantiationCallback& OnCookFinished, const FString DefaultCookFolder)
{
	UHoudiniPublicAPI* HoudiniAPI = UHoudiniPublicAPIBlueprintLib::GetAPI();

	SetDefaultCookFolder(DefaultCookFolder);

	UHoudiniEditorTestObject* TestObject = GetTestObject();
	TestObject->ResetFields();

	TestObject->IsInstantiating = true;

	TestObject->InAssetWrappers.Add(InAssetWrapper); // Need to assign it to TestObject otherwise it will be garbage collected!!!

	// Bind delegates from the asset wrapper to UHoudiniEditorTestObject which we use to proxy to non-dynamic delegates
	// like OnPreInstantiation.
	InAssetWrapper->GetOnPostCookDelegate().AddUniqueDynamic(TestObject, &UHoudiniEditorTestObject::PostCookCallback);
	InAssetWrapper->GetOnPostProcessingDelegate().AddUniqueDynamic(TestObject, &UHoudiniEditorTestObject::PostProcessingCallback);

	InAssetWrapper->Recook(); // Make sure the callback is called!

	const double StartTime = FPlatformTime::Seconds();

	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		const bool CookSuccessfulResult = TestObject->InCookSuccess;

		const double CurrentTime = FPlatformTime::Seconds();

		if (TestObject->CookCount >= TestObject->ExpectedCookCount && InAssetWrapper->GetHoudiniAssetComponent()->GetAssetState() == EHoudiniAssetState::None)
		{
			if (CookSuccessfulResult == false)
			{
				Test->AddError(FString::Printf(TEXT("Cook was unsuccessful")));
			}

			// Sometimes hangs if things happen too fast. Hacky way to fix this!
			if (CurrentTime - StartTime < 1)
			{
				return false;
			}

			ForceRefreshViewport();

			// Only complete once the asset has cooked the expected number of times, or if a cook fails (depending on
			// ErrorOnFail). Cook success/failure is updated after each cook
			if (TestObject->HasReachedExpectedCookCount() || !CookSuccessfulResult)
			{
				SetDefaultCookFolder(TEXT("/Game/HoudiniEngine/Temp"));
				OnCookFinished(InAssetWrapper, CookSuccessfulResult);

				TestObject->ResetFields();
				return true;
			}
		}

		if (CurrentTime - StartTime > TimeoutTime)
		{
			Test->AddError("Instantiation timeout!");
			OnCookFinished(InAssetWrapper, CookSuccessfulResult);
			return true;
		}

		return false;
	}
	));
}

bool FHoudiniEditorTestUtils::CreateSessionIfInvalid(const FName& SessionPipeName)
{
	// If the session is valid then we can just return true
	if (FHoudiniEngineCommands::IsSessionValid())
		return true;

	// No existing valid HE session: try to create a Houdini Engine Session
	HOUDINI_LOG_MESSAGE(TEXT("[FHoudiniEditorTestUtils::CreateSessionIfInvalid] No current valid Houdini Engine session, attempting to create one..."));
	
	UHoudiniRuntimeSettings const* const HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	const bool bSuccess = FHoudiniEngine::Get().CreateSession(HoudiniRuntimeSettings->SessionType, SessionPipeName);
	if (bSuccess)
	{
		// We've successfully created the Houdini Engine session,
		// We now need to notify all the HoudiniAssetComponent that they need to re instantiate 
		// themselves in the new Houdini engine session.
		FHoudiniEngineUtils::MarkAllHACsAsNeedInstantiation();
	}

	return bSuccess;
}

bool FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(
	FHoudiniAutomationTest* Test, const FName& SessionPipeName, const TFunction<void()>& OnSuccess,
	const TFunction<void()>& OnFail, const int8 NumRetries, const double TimeBetweenRetriesSeconds)
{
	// Check if a valid session already exists, or try to create one
	if (CreateSessionIfInvalid(SessionPipeName))
	{
		// A valid session exists or was created
		if (OnSuccess != nullptr)
		{
			OnSuccess();
		}

		return true;
	}
	
	// If a session did not exist and could not be created, set up a latent command to try 2 more times
	struct FSessionCheckState
	{
		double CountDown = 0;
		int8 NumRetriesRemaining = 0;
	};
	TSharedPtr<FSessionCheckState> SessionCheckState(new FSessionCheckState());
	SessionCheckState->CountDown = TimeBetweenRetriesSeconds;
	SessionCheckState->NumRetriesRemaining = NumRetries;

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine session is not valid and a new session could not be created."));

	HOUDINI_LOG_MESSAGE(
		TEXT("Retrying Houdini Engine session creation in %.2f seconds (%d remaining retries)"),
		SessionCheckState->CountDown, SessionCheckState->NumRetriesRemaining);
	
	Test->AddCommand(new FFunctionLatentCommand([SessionCheckState, TimeBetweenRetriesSeconds, Test, OnSuccess, OnFail]()
	{
		if (SessionCheckState->CountDown > 0)
		{
			SessionCheckState->CountDown -= GWorld->GetDeltaSeconds();
			
			return false;
		}

		SessionCheckState->NumRetriesRemaining -= 1;
		if (!CreateSessionIfInvalid(TEXT("hapiunreal")))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine session is not valid and a new session could not be created."));

			if (SessionCheckState->NumRetriesRemaining > 0)
			{
				SessionCheckState->CountDown = TimeBetweenRetriesSeconds;
				
				HOUDINI_LOG_MESSAGE(
					TEXT("Retrying Houdini Engine session creation in %.2f seconds (%d remaining retries)"),
					SessionCheckState->CountDown, SessionCheckState->NumRetriesRemaining);
				
				return false;
			}

			HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine session creation retries exhausted."));
			
			Test->AddError(TEXT("Failed to create the Houdini Engine session!"));
			if (OnFail != nullptr)
			{
				OnFail();
			}
		}
		else if (OnSuccess != nullptr)
		{
			OnSuccess();
		}

		return true;
	}));

	return false;
}

#endif

// UHoudiniEditorTestObject (Required due to dynamic delegates)s
UHoudiniEditorTestObject::UHoudiniEditorTestObject()
{
}

void UHoudiniEditorTestObject::PreInstantiationCallback(UHoudiniPublicAPIAssetWrapper* InAssetWrapper)
{
	InAssetWrapper->GetOnPreInstantiationDelegate().RemoveDynamic(this, &UHoudiniEditorTestObject::PreInstantiationCallback);

	if (OnPreInstantiationDelegate.IsBound())
		OnPreInstantiationDelegate.Broadcast(InAssetWrapper, this);
}

void UHoudiniEditorTestObject::PostInstantiationCallback(UHoudiniPublicAPIAssetWrapper* AssetWrapper)
{
	this->IsInstantiating = false;

	AssetWrapper->GetOnPostInstantiationDelegate().RemoveDynamic(this, &UHoudiniEditorTestObject::PostInstantiationCallback);
}

void UHoudiniEditorTestObject::PostCookCallback(UHoudiniPublicAPIAssetWrapper* AssetWrapper, const bool bInCookSuccess)
{
	this->CookCount++;
	this->InCookSuccess = bInCookSuccess;

	// Unbind from the wrapper's post cook callback after the expected number of cooks have been done
	if (HasReachedExpectedCookCount())
		AssetWrapper->GetOnPostCookDelegate().RemoveDynamic(this, &UHoudiniEditorTestObject::PostCookCallback);
}

void UHoudiniEditorTestObject::PostProcessingCallback(UHoudiniPublicAPIAssetWrapper* InAssetWrapper)
{
	// Unbind from the wrapper's post processing callback after the expected number of cooks have been done
	if (HasReachedExpectedCookCount())
		InAssetWrapper->GetOnPostProcessingDelegate().RemoveDynamic(this, &UHoudiniEditorTestObject::PostProcessingCallback);

	if (OnPostProcessingDelegate.IsBound())
		OnPostProcessingDelegate.Broadcast(InAssetWrapper, this);
}

void UHoudiniEditorTestObject::ResetFields()
{
	this->IsInstantiating = false;
	this->CookCount = 0;
	this->ExpectedCookCount = 1;
	this->InCookSuccess = false;

	OnPreInstantiationDelegate.Clear();
	OnPostProcessingDelegate.Clear();
}
