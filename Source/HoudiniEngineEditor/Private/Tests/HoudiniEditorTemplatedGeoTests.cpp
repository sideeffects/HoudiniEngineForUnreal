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

#include "HoudiniEditorTemplatedGeoTests.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"
#include "HoudiniPublicAPIAssetWrapper.h"

#include "Core/Public/HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "HoudiniAssetComponent.h"

FString FHoudiniEditorTemplatedGeoTests::EquivalenceTestMapName = TEXT("TemplatedGeo");
FString FHoudiniEditorTemplatedGeoTests::TestHDAPath = TEXT("/Game/TestHDAs/TemplatedGeo/");

/** Tests with an HDA that has templated geo, but with the "Output Templated Geos" checkbox unchecked */
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorTemplatedGeoTest_HACDisabled, "Houdini.Editor.TemplatedGeo.HACDisabled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorTemplatedGeoTest_HACDisabled::RunTest(const FString& InParameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("HACDisabled");
		const FString MapName = FHoudiniEditorTemplatedGeoTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorTemplatedGeoTests::TestHDAPath + TEXT("TemplatedGeo_Enabled_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			// OnPreInstantiate -- set parameters or HAC properties etc
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!IsValid(InAssetWrapper))
				{
					this->AddError(TEXT("Asset wrapper is invalid during pre-instantiation!"));
					return;
				}

				// Do nothing... templated geo is off by default
			});
	});

	return true;
}


/** Tests with an HDA that has templated geo, but with the "Output Templated Geos" checkbox checked */
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorTemplatedGeoTest_HACEnabled, "Houdini.Editor.TemplatedGeo.HACEnabled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorTemplatedGeoTest_HACEnabled::RunTest(const FString& InParameters)
{
	// Currently there is an issue in HAPI that will add an error about HAPI_CookOptions::cookTemplatedGeos == false,
	// even though it is not, ignore that for now
	/*
	AddExpectedError(
		TEXT("Hapi failed: Invalid argument given: Tempated non-editable geo did not cook because "
			"HAPI_CookOptions::cookTemplatedGeos is false. Geo info may be out of date."),
		EAutomationExpectedErrorFlags::Contains, 0);
	*/
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("HACEnabled");
		const FString MapName = FHoudiniEditorTemplatedGeoTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorTemplatedGeoTests::TestHDAPath + TEXT("TemplatedGeo_Enabled_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			// OnPreInstantiate -- set parameters or HAC properties etc
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				UHoudiniAssetComponent* HAC = nullptr;
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperTestObjectAndGetValidHAC(this, InAssetWrapper, InTestObject, HAC))
					return;
				
				// Enable templated geo output via the HAC
				HAC->bOutputTemplateGeos = true;
			});
	});

	return true;
}


/** Tests with an HDA that does not have templated geo, but with the "Output Templated Geos" checkbox checked */
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorTemplatedGeoTest_HACEnabled_NoTemplateInOutput, "Houdini.Editor.TemplatedGeo.HACEnabled_NoTemplateInOutput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorTemplatedGeoTest_HACEnabled_NoTemplateInOutput::RunTest(const FString& InParameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("HACEnabled_NoTemplateInOutput");
		const FString MapName = FHoudiniEditorTemplatedGeoTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorTemplatedGeoTests::TestHDAPath + TEXT("TemplatedGeo_Disabled_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			// OnPreInstantiate -- set parameters or HAC properties etc
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				UHoudiniAssetComponent* HAC = nullptr;
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperTestObjectAndGetValidHAC(this, InAssetWrapper, InTestObject, HAC))
					return;

				// Enable templated geo output via the HAC
				HAC->bOutputTemplateGeos = true;
			});
	});

	return true;
}

#endif
