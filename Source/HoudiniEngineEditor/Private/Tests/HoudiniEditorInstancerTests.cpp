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

#include "HoudiniEditorInstancerTests.h"

#include "HoudiniEditorEquivalenceUtils.h"
#include "HoudiniPublicAPIAssetWrapper.h"

#include "FileHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HoudiniEditorTestUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"

#include "Core/Public/HAL/FileManager.h"
#include "Misc/AutomationTest.h"

FString FHoudiniEditorInstancerTests::EquivalenceTestMapName = TEXT("Instancer");
FString FHoudiniEditorInstancerTests::TestHDAPath = TEXT("/Game/TestHDAs/Instancer/");



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerPackedPrimsOBJ, "Houdini.Editor.Instancer.PackedPrimitivesOBJ", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerPackedPrimsOBJ::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("PackedPrimitives_OBJ");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("packedpriminstancers_obj");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerAttributeInstancerOBJ, "Houdini.Editor.Instancer.AttributeInstancerOBJ", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerAttributeInstancerOBJ::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("AttributeInstancer_OBJ");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("attributeinstancers_obj");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingOBJ, "Houdini.Editor.Instancer.SplittingOBJ", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingOBJ::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_OBJ");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_obj");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesOBJ, "Houdini.Editor.Instancer.TypesOBJ", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesOBJ::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_OBJ");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_obj");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerCustomDataOBJ, "Houdini.Editor.Instancer.CustomDataOBJ", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerCustomDataOBJ::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerCustomData_OBJ");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_customdata_obj");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerPackedPrimsSOPOneInstancer, "Houdini.Editor.Instancer.PackedPrimitivesSOP.OneInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerPackedPrimsSOPOneInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("PackedPrimitives_SOP_OneInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("packedprimitives_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 0;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this,InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerPackedPrimsSOPMultipleInstancer, "Houdini.Editor.Instancer.PackedPrimitivesSOP.MultipleInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerPackedPrimsSOPMultipleInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("PackedPrimitives_SOP_MultipleInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("packedprimitives_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 1;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerPackedPrimsSOPSingleInstance, "Houdini.Editor.Instancer.PackedPrimitivesSOP.SingleInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerPackedPrimsSOPSingleInstance::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("PackedPrimitives_SOP_SingleInstance");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("packedprimitives_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 2;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerPackedPrimsSOPMultipleSingleInstance, "Houdini.Editor.Instancer.PackedPrimitivesSOP.MultipleSingleInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerPackedPrimsSOPMultipleSingleInstance::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("PackedPrimitives_SOP_MultipleSingleInstance");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("packedprimitives_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 2;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerAttributeInstancerSOPOneInstancer, "Houdini.Editor.Instancer.AttributeInstancerSOP.OneInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerAttributeInstancerSOPOneInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("AttributeInstancer_SOP_OneInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("attribinstancer_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 0;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerAttributeInstancerSOPMultipleInstancer, "Houdini.Editor.Instancer.AttributeInstancerSOP.MultipleInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerAttributeInstancerSOPMultipleInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("AttributeInstancer_SOP_MultipleInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("attribinstancer_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 1;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerAttributeInstancerSOPSingleInstance, "Houdini.Editor.Instancer.AttributeInstancerSOP.SingleInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerAttributeInstancerSOPSingleInstance::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("AttributeInstancer_SOP_SingleInstance");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("attribinstancer_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 2;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerAttributeInstancerSOPMultipleSingleInstance, "Houdini.Editor.Instancer.AttributeInstancerSOP.MultipleSingleInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerAttributeInstancerSOPMultipleSingleInstance::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("AttributeInstancer_SOP_MultipleSingleInstance");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("attribinstancer_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 2;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOPByColorPacked, "Houdini.Editor.Instancer.SplittingSOP.ByColorPacked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOPByColorPacked::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP_ByColorPacked");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 0;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOPByColorAttrInstancer, "Houdini.Editor.Instancer.SplittingSOP.ByColorAttrInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOPByColorAttrInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP_ByColorAttrInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 1;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOPByStringPacked, "Houdini.Editor.Instancer.SplittingSOP.ByStringPacked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOPByStringPacked::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP_ByStringPacked");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 2;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOPByStringAttrInstancer, "Houdini.Editor.Instancer.SplittingSOP.ByStringAttrInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOPByStringAttrInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP_ByStringAttrInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 3;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOPByMatPacked, "Houdini.Editor.Instancer.SplittingSOP.ByMatPacked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOPByMatPacked::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP_ByMatPacked");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 4;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOPByMatAttrInstancer, "Houdini.Editor.Instancer.SplittingSOP.ByMatAttrInstancer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOPByMatAttrInstancer::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP_ByMatAttrInstancer");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 5;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPISMC, "Houdini.Editor.Instancer.TypesSOP.ISMC", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPISMC::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_ISMC");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 0;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPHISMC, "Houdini.Editor.Instancer.TypesSOP.HISMC", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPHISMC::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_HISMC");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 1;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPHISMCForced, "Houdini.Editor.Instancer.TypesSOP.HISMCForced", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPHISMCForced::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_HISMCForced");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 2;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPHISMCForcedOff, "Houdini.Editor.Instancer.TypesSOP.HISMCForcedOff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPHISMCForcedOff::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_HISMCForcedOff");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 3;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPMSIC, "Houdini.Editor.Instancer.TypesSOP.MSIC", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPMSIC::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_MSIC");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 4;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPIAC, "Houdini.Editor.Instancer.TypesSOP.IAC", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPIAC::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_IAC");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 5;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPAttribClass, "Houdini.Editor.Instancer.TypesSOP.AttribClass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPAttribClass::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_AttribClass");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 6;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPFoliage, "Houdini.Editor.Instancer.TypesSOP.Foliage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPFoliage::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_Foliage");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 7;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPDecals, "Houdini.Editor.Instancer.TypesSOP.Decals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPDecals::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_Decals");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");
		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 8;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOPInvalid, "Houdini.Editor.Instancer.TypesSOP.Invalid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOPInvalid::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP_Invalid");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 9;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerCustomDataSOPNumberSuite, "Houdini.Editor.Instancer.CustomDataSOP.NumberSuite", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerCustomDataSOPNumberSuite::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerCustomData_SOP_NumberSuite");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_customdata_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 0;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerCustomDataSOPColors, "Houdini.Editor.Instancer.CustomDataSOP.Colors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerCustomDataSOPColors::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerCustomData_SOP_Colors");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_customdata_sop");

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			int32 ParamValue = 1;
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(this, InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
			{
				if (bSaveAssets)
					FHoudiniEditorTestUtils::SaveCurrentLevel();

				ContinueCallback(true);
			},
				DefaultCookFolder
				);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,
			nullptr, nullptr, SetParamsOnPostInstantiate);
	});

	return true;
}



/*
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerPackedPrimsSOP, "Houdini.Editor.Instancer.PackedPrimitivesSOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerPackedPrimsSOP::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("PackedPrimitives_SOP");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("packedprimitives_sop");

		// Param Value, we need to test 0 - 3
		int32 ParamValue = 0;

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(
				this,
				InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,	
			[=, &ParamValue](bool IsSuccessful)
			{
				ParamValue = 1;
				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
					this, MapName, HDAAssetPath, ActorName, 
					[=, &ParamValue](bool IsSuccessful)
					{
						ParamValue = 2;
						FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
							this, MapName, HDAAssetPath, ActorName, 
							[=, &ParamValue](bool IsSuccessful)
							{
								ParamValue = 3;
								FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
									this, MapName, HDAAssetPath, ActorName, 
									nullptr, nullptr, SetParamsOnPostInstantiate);
							},
							nullptr, SetParamsOnPostInstantiate
						);
					},
					nullptr, SetParamsOnPostInstantiate
				);
			},
			nullptr, SetParamsOnPostInstantiate
		);
	});

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerAttributeInstancerSOP, "Houdini.Editor.Instancer.AttributeInstancerSOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerAttributeInstancerSOP::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("AttributeInstancer_SOP");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("attribinstancer_sop");

		// Param Value, we need to test 0 - 3
		int32 ParamValue = 0;

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(
				this,
				InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,	
			[=, &ParamValue](bool IsSuccessful)
			{
				ParamValue = 1;
				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
					this, MapName, HDAAssetPath, ActorName, 
					[=, &ParamValue](bool IsSuccessful)
					{
						ParamValue = 2;
						FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
							this, MapName, HDAAssetPath, ActorName, 
							[=, &ParamValue](bool IsSuccessful)
							{
								ParamValue = 3;
								FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
									this, MapName, HDAAssetPath, ActorName, 
									nullptr, nullptr, SetParamsOnPostInstantiate);
							},
							nullptr, SetParamsOnPostInstantiate
						);
					},
					nullptr, SetParamsOnPostInstantiate
				);
			},
			nullptr, SetParamsOnPostInstantiate
		);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerSplittingSOP, "Houdini.Editor.Instancer.SplittingSOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerSplittingSOP::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerSplitting_SOP");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_splitting_sop");

		// Param Value, we need to test 0 - 5
		int32 ParamValue = 0;

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(
				this,
				InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,	
			[=, &ParamValue](bool IsSuccessful)
			{
				ParamValue = 1;
				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
					this, MapName, HDAAssetPath, ActorName, 
					[=, &ParamValue](bool IsSuccessful)
					{
						ParamValue = 2;
						FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
							this, MapName, HDAAssetPath, ActorName, 
							[=, &ParamValue](bool IsSuccessful)
							{
								ParamValue = 3;
								FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
									this, MapName, HDAAssetPath, ActorName,
									[=, &ParamValue](bool IsSuccessful)
									{
										ParamValue = 4;
										FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
											this, MapName, HDAAssetPath, ActorName,
											[=, &ParamValue](bool IsSuccessful)
											{
												ParamValue = 5;
												FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
													this, MapName, HDAAssetPath, ActorName,
													nullptr, nullptr, SetParamsOnPostInstantiate);
											},
											nullptr, SetParamsOnPostInstantiate
										);
									},
									nullptr, SetParamsOnPostInstantiate
								);
							},
							nullptr, SetParamsOnPostInstantiate
						);
					},
					nullptr, SetParamsOnPostInstantiate
				);
			},
			nullptr, SetParamsOnPostInstantiate
		);
	});

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerTypesSOP, "Houdini.Editor.Instancer.TypesSOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerTypesSOP::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerTypes_SOP");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_types_sop");

		// Param Value, we need to test 0 - 9
		int32 ParamValue = 0;

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(
				this,
				InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,	
			[=, &ParamValue](bool IsSuccessful)
			{
				ParamValue = 1;
				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
					this, MapName, HDAAssetPath, ActorName, 
					[=, &ParamValue](bool IsSuccessful)
					{
						ParamValue = 2;
						FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
							this, MapName, HDAAssetPath, ActorName, 
							[=, &ParamValue](bool IsSuccessful)
							{
								ParamValue = 3;
								FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
									this, MapName, HDAAssetPath, ActorName,
									[=, &ParamValue](bool IsSuccessful)
									{
										ParamValue = 4;
										FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
											this, MapName, HDAAssetPath, ActorName,
											[=, &ParamValue](bool IsSuccessful)
											{
												ParamValue = 5;
												FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
													this, MapName, HDAAssetPath, ActorName,
													[=, &ParamValue](bool IsSuccessful)
													{
														ParamValue = 6;
														FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
															this, MapName, HDAAssetPath, ActorName,
															[=, &ParamValue](bool IsSuccessful)
															{
																ParamValue = 7;
																FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
																	this, MapName, HDAAssetPath, ActorName,
																	[=, &ParamValue](bool IsSuccessful)
																	{
																		ParamValue = 8;
																		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
																			this, MapName, HDAAssetPath, ActorName,
																			[=, &ParamValue](bool IsSuccessful)
																			{
																				ParamValue = 9;
																				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
																					this, MapName, HDAAssetPath, ActorName,
																					nullptr, nullptr, SetParamsOnPostInstantiate);
																			},
																			nullptr, SetParamsOnPostInstantiate
																		);
																	},
																	nullptr, SetParamsOnPostInstantiate
																);
															},
															nullptr, SetParamsOnPostInstantiate
														);
													},
													nullptr, SetParamsOnPostInstantiate
												);
											},
											nullptr, SetParamsOnPostInstantiate
										);
									},
									nullptr, SetParamsOnPostInstantiate
								);
							},
							nullptr, SetParamsOnPostInstantiate
						);
					},
					nullptr, SetParamsOnPostInstantiate
				);
			},
			nullptr, SetParamsOnPostInstantiate
		);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorInstancerCustomDataSOP, "Houdini.Editor.Instancer.CustomDataSOP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool HoudiniEditorInstancerCustomDataSOP::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("InstancerCustomData_SOP");

		const FString MapName = FHoudiniEditorInstancerTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorInstancerTests::TestHDAPath + TEXT("instancer_customdata_sop");

		// Param Value, we need to test 0 - 1
		int32 ParamValue = 0;

		// Lambda that sets the "input" parameter after instantiation, then recooks before proceeding with the test.
		auto SetParamsOnPostInstantiate = [=](UHoudiniPublicAPIAssetWrapper* InAssetWrapper, TFunction<void(bool)> ContinueCallback, const bool bSaveAssets, const FString DefaultCookFolder)
		{
			FString ParamName = TEXT("input");
			InAssetWrapper->SetIntParameterValue(FName(ParamName), ParamValue, 0);

			FHoudiniEditorTestUtils::RecookAndWait(
				this,
				InAssetWrapper,
				[=](UHoudiniPublicAPIAssetWrapper* _, const bool __)
				{
					if (bSaveAssets)
						FHoudiniEditorTestUtils::SaveCurrentLevel();

					ContinueCallback(true);
				},
				DefaultCookFolder
			);
		};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
			this, MapName, HDAAssetPath, ActorName,	
			[=, &ParamValue](bool IsSuccessful)
			{
				ParamValue = 1;
				FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(
					this, MapName, HDAAssetPath, ActorName,
					nullptr, nullptr, SetParamsOnPostInstantiate);
			},
			nullptr, SetParamsOnPostInstantiate
		);
	});

	return true;
}
*/

#endif
