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

#include "HoudiniEditorInputTests.h"

#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniAssetActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"

FString FHoudiniEditorInputTests::EquivalenceTestMapName = TEXT("Inputs");
FString FHoudiniEditorInputTests::TestHDAPath = TEXT("/Game/TestHDAs/Inputs/");

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInputTest_Mesh_Input, "Houdini.Editor.Inputs.Mesh_Input", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorInputTest_Mesh_Input::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorInputTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Mesh_Input");
		const FString HDAAssetPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("InputAsOutput");

		// Suppress temp file warning
		this->AddSupressedWarning("failed to load '/Game/HoudiniEngine/Temp/plain_cube");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr, nullptr,
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			const FString InputHDAPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("Helpers/plain_cube");
			const FString InputActorName = TEXT("Mesh_Input_input");

			if (bSaveAssets)
				FHoudiniEditorTestUtils::DeleteHACWithNameInLevelAndSave(InputActorName);
			
			FHoudiniEditorTestUtils::InstantiateAsset(this, FName(InputHDAPath),
			[=] (UHoudiniPublicAPIAssetWrapper* InInputAssetWrapper, const bool Success)
			{
				if (!Success)
				{
					this->AddError("Instantiation failed!");
					ContinueCallback(false);
		
					return;
				}
				
				InInputAssetWrapper->GetHoudiniAssetActor()->SetActorLabel(InputActorName);

				// Sets the input to the curve
				const TSubclassOf<UHoudiniPublicAPIInput> APIInputClass = UHoudiniPublicAPIAssetInput::StaticClass();

				UHoudiniPublicAPIInput* AssetInput = InAssetWrapper->CreateEmptyInput(APIInputClass);
				AssetInput->SetInputObjects({InInputAssetWrapper});
				InAssetWrapper->SetInputAtIndex(0, AssetInput);

				FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();
				
					ContinueCallback(true);
				}, DefaultCookFolder
				);
			});
		}
		);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInputTest_Heightfield_Input, "Houdini.Editor.Inputs.Heightfield_Input", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorInputTest_Heightfield_Input::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorInputTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Heightfield_Input");
		const FString HDAAssetPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("InputAsOutput");

		// Suppress temp file warning
		 this->AddSupressedWarning("failed to load '/Game/HoudiniEngine/Temp/plain_cube");
		
		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr, nullptr,
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			const FString InputHDAPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("Helpers/plain_heightfield");
			const FString InputActorName = TEXT("Heightfield_Input_input");

			if (bSaveAssets)
				FHoudiniEditorTestUtils::DeleteHACWithNameInLevelAndSave(InputActorName);
			
			FHoudiniEditorTestUtils::InstantiateAsset(this, FName(InputHDAPath),
			[=] (UHoudiniPublicAPIAssetWrapper* InInputAssetWrapper, const bool Success)
			{
				if (!Success)
				{
					this->AddError("Instantiation failed!");
					ContinueCallback(false);
		
					return;
				}
				
				InInputAssetWrapper->GetHoudiniAssetActor()->SetActorLabel(InputActorName);

				// I can't get assigning Landscape Input using the public API to work, so I use  UHoudiniPublicAPIAssetInput instead.
				/*
				const TSubclassOf<UHoudiniPublicAPIInput> APIInputClass = UHoudiniPublicAPILandscapeInput::StaticClass();
				TArray<FHoudiniPublicAPIOutputObjectIdentifier> Identifiers;
				InInputAssetWrapper->GetOutputIdentifiersAt(0, Identifiers);

				UObject * OutputObject = InInputAssetWrapper->GetOutputObjectAt(0, Identifiers[0]);
				
				UHoudiniPublicAPIInput* AssetInput = InAssetWrapper->CreateEmptyInput(APIInputClass);
				AssetInput->SetInputObjects({OutputObject});
				InAssetWrapper->SetInputAtIndex(0, AssetInput);
				***/
				
				// Sets the input to the curve
				const TSubclassOf<UHoudiniPublicAPIInput> APIInputClass = UHoudiniPublicAPIAssetInput::StaticClass();

				UHoudiniPublicAPIInput* AssetInput = InAssetWrapper->CreateEmptyInput(APIInputClass);
				AssetInput->SetInputObjects({InInputAssetWrapper});
				InAssetWrapper->SetInputAtIndex(0, AssetInput);

				FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();
				
					ContinueCallback(true);
				}, DefaultCookFolder
				);
			});
		}
		);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInputTest_Mesh_Geo_Input, "Houdini.Editor.Inputs.Mesh_Geo_Input", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorInputTest_Mesh_Geo_Input::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorInputTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Mesh_Geo_Input");
		const FString HDAAssetPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("InputAsOutput");

		// Suppress temp file warning
 		this->AddSupressedWarning("failed to load '/Game/HoudiniEngine/Temp/plain_cube");
		
		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr, nullptr,
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{

			// const FString AssetName = "Blueprint'/Game/TestObjects/BP_Cube.BP_Cube'"; // BUGGED - Please copy/paste this test and remove this line when fixed.
			const FString AssetName = "Blueprint'/Game/TestObjects/SM_Cube.SM_Cube'";
			UObject * InputObject = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetName, nullptr, LOAD_None, nullptr);

			const TSubclassOf<UHoudiniPublicAPIInput> APIInputClass = UHoudiniPublicAPIGeoInput::StaticClass();

			UHoudiniPublicAPIInput* AssetInput = InAssetWrapper->CreateEmptyInput(APIInputClass);
			AssetInput->SetInputObjects({InputObject});
			InAssetWrapper->SetInputAtIndex(0, AssetInput);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
			[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();
				
				ContinueCallback(true);
			}, DefaultCookFolder
			);
		}
		);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInputTest_Curve_Input, "Houdini.Editor.Inputs.Curve_Input", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorInputTest_Curve_Input::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorInputTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Curve_Input");
		const FString HDAAssetPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("simple_curve_input");

		// Suppress temp file warning
		this->AddSupressedWarning("failed to load '/Game/HoudiniEngine/Temp/plain_cube");
		
		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr, nullptr,
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			const FString CurveHDAPath = FHoudiniEditorInputTests::TestHDAPath + TEXT("Helpers/plain_editable_curve");
			const FString CurveActorName = TEXT("curve_input_hda");

			if (bSaveAssets)
				FHoudiniEditorTestUtils::DeleteHACWithNameInLevelAndSave(CurveActorName);
			
			FHoudiniEditorTestUtils::InstantiateAsset(this, FName(CurveHDAPath),
			[=] (UHoudiniPublicAPIAssetWrapper* InAssetWrapperCurve, const bool Success)
			{
				if (!Success)
				{
					this->AddError("Instantiation failed!");
					ContinueCallback(false);
		
					return;
				}
				
				InAssetWrapperCurve->GetHoudiniAssetActor()->SetActorLabel(CurveActorName);

				// Sets the input to the curve
				const TSubclassOf<UHoudiniPublicAPIInput> APIInputClass = UHoudiniPublicAPIAssetInput::StaticClass();

				UHoudiniPublicAPIInput* AssetInput = InAssetWrapper->CreateEmptyInput(APIInputClass);
				AssetInput->SetInputObjects({InAssetWrapperCurve});
				InAssetWrapper->SetInputAtIndex(0, AssetInput);


				FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();
				
					ContinueCallback(true);
				}, DefaultCookFolder
				);
			});
		}
		);
	});

	return true;
}


#endif

