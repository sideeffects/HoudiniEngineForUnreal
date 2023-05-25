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

#include "HoudiniEditorTests.h"

#include "HoudiniEditorEquivalenceUtils.h"
#include "HoudiniPublicAPIAssetWrapper.h"

#include "FileHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HoudiniEditorTestUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"

#include "Core/Public/HAL/FileManager.h"
#include "Misc/AutomationTest.h"


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorEvergreenTest, "Houdini.Editor.Screenshots.EvergreenScreenshots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorEvergreenTest::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		FHoudiniEditorTestUtils::GetMainFrameWindow()->Resize(FHoudiniEditorTestUtils::GDefaultEditorSize);

		FHoudiniEditorTestUtils::InstantiateAsset(this, TEXT("/Game/TestHDAs/Evergreen"), 
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
		{
			// Entire editor screenshots don't work well with build machines. Disable for now.
			// FHoudiniEditorTestUtils::TakeScreenshotEditor(this, TEXT("EverGreen_EntireEditor.png"), FHoudiniEditorTestUtils::ENTIRE_EDITOR, FHoudiniEditorTestUtils::GDefaultEditorSize);
			FHoudiniEditorTestUtils::TakeScreenshotEditor(this, TEXT("EverGreen_Details.png"), FHoudiniEditorTestUtils::DETAILS_WINDOW, FVector2D(400, 1000));
			FHoudiniEditorTestUtils::TakeScreenshotEditor(this, TEXT("EverGreen_EditorViewport.png"), FHoudiniEditorTestUtils::VIEWPORT, FVector2D(640, 360));
		});
	});
	return true;
}

// Tests whether or not we need to update the FHoudiniEditorEquivalenceUtils::IsEquivalent function
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorDuplicateTest, "Houdini.Editor.Random.DuplicateTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorDuplicateTest::RunTest(const FString & Parameters)
{
	// Really force editor size
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		FHoudiniEditorTestUtils::InstantiateAsset(this, TEXT("/Game/TestHDAs/Evergreen"), 
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool IsSuccessful)
		{
		FHoudiniEditorTestUtils::InstantiateAsset(this, TEXT("/Game/TestHDAs/Evergreen"), 
		[=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper2, const bool IsSuccessful2)
		{
			if (FHoudiniEditorEquivalenceUtils::IsEquivalent(InAssetWrapper->GetHoudiniAssetComponent(), InAssetWrapper2->GetHoudiniAssetComponent()))
			{
				return true;
			}
			else
			{
				this->AddError("Duplication is not Equivalent! This is a sign we need to update the FHoudiniEditorEquivalenceUtils::IsEquivalent function");
				return false;
			}
			
		});
	});
	});
	
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorEvergreenEquivalenceTest, "Houdini.Editor.Random.EvergreenEquivalence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorEvergreenEquivalenceTest::RunTest(const FString & Parameters)
{
	// Really force editor size
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = TEXT("RandomTests");
		const FString ActorName = TEXT("EvergreenEquivalence");
		const FString HDAAssetPath = TEXT("/Game/TestHDAs/Evergreen");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, [=](bool IsSuccessful)
		{
			
		});
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorRandomEquivalenceTest, "Houdini.Editor.Random.RandomEquivalence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorRandomEquivalenceTest::RunTest(const FString & Parameters)
{
	// Really force editor size
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, TEXT("RandomTests"), TEXT("/Game/TestHDAs/Random/simple_curve"), TEXT("simple_curve"), [=](bool IsSuccessful)
		{
			FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, TEXT("RandomTests"), TEXT("/Game/TestHDAs/Random/simple_heightfield"), TEXT("simple_heightfield"), [=](bool IsSuccessful)
			{
				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, TEXT("RandomTests"), TEXT("/Game/TestHDAs/Random/Instancing_ThreeWays"), TEXT("Instancing_ThreeWays"), [=](bool IsSuccessful)
				{
					FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, TEXT("RandomTests"), TEXT("/Game/TestHDAs/Random/SideFX__scott_spookytable"), TEXT("SideFX__scott_spookytable"), [=](bool IsSuccessful)
					{
		
					});
				});
			});
		});
	});
	
	return true;
}

#endif
