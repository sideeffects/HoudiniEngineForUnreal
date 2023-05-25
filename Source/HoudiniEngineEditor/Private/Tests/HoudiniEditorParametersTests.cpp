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

#include "HoudiniEditorParametersTests.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniAssetComponent.h"

#include "Core/Public/HAL/FileManager.h"
#include "Misc/AutomationTest.h"

FString FHoudiniEditorParametersTests::EquivalenceTestMapName = TEXT("Parameters");
FString FHoudiniEditorParametersTests::TestHDAPath = TEXT("/Game/TestHDAs/Parameters/");

// Test setting a single float value
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Float_Single, "Houdini.Editor.Parameters.Float_Single", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Float_Single::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Float_Single");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InWrapper->SetFloatParameterValue(TEXT("uniform_scale"), 2.5f))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("uniform_scale"), TEXT("2.5"), 0);
					}
				});
			});
	});

	return true;
}

// Test setting the values of a float tuple
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Float_Tuple, "Houdini.Editor.Parameters.Float_Tuple", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Float_Tuple::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Float_Tuple");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InWrapper->SetFloatParameterValue(TEXT("scale"), 1.5f, 0))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("scale"), TEXT("1.5f"), 0);
					}
					if (!InWrapper->SetFloatParameterValue(TEXT("scale"), 3.0f, 1))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("scale"), TEXT("3.0f"), 1);
					}
					if (!InWrapper->SetFloatParameterValue(TEXT("scale"), 6.0f, 2))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("scale"), TEXT("6.0f"), 2);
					}
				});
			});
	});

	return true;
}


// Test setting the values of a float vector
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Float_Vector, "Houdini.Editor.Parameters.Float_Vector", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Float_Vector::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Float_Vector");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InAssetWrapper->SetFloatParameterValue(TEXT("t"), 100.0f, 0))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("float"), TEXT("t"), TEXT("100.0f"), 0);
					}
					if (!InAssetWrapper->SetFloatParameterValue(TEXT("t"), 0.0f, 1))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("float"), TEXT("t"), TEXT("0.0f"), 1);
					}
					if (!InAssetWrapper->SetFloatParameterValue(TEXT("t"), 10.0f, 2))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("float"), TEXT("t"), TEXT("10.0f"), 2);
					}

					if (!InAssetWrapper->SetFloatParameterValue(TEXT("r"), 33.3f, 0))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("float"), TEXT("r"), TEXT("33.3f"), 0);
					}
					if (!InAssetWrapper->SetFloatParameterValue(TEXT("r"), 66.6f, 1))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("float"), TEXT("r"), TEXT("66.6f"), 1);
					}
					if (!InAssetWrapper->SetFloatParameterValue(TEXT("r"), 99.9f, 2))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("float"), TEXT("r"), TEXT("99.9f"), 2);
					}
				});
			});
	});

	return true;
}


// Test setting the values of a float tuple
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Float_Tuple_PreInstantiate, "Houdini.Editor.Parameters.Float_Tuple_PreInstantiate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Float_Tuple_PreInstantiate::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Float_Tuple_PreInstantiate");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update during pre-instantiate
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InWrapper->SetFloatParameterValue(TEXT("scale"), 1.5f, 0))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("scale"), TEXT("1.5f"), 0);
					}
					if (!InWrapper->SetFloatParameterValue(TEXT("scale"), 3.0f, 1))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("scale"), TEXT("3.0f"), 1);
					}
					if (!InWrapper->SetFloatParameterValue(TEXT("scale"), 6.0f, 2))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("float"), TEXT("scale"), TEXT("6.0f"), 2);
					}
				});
			});
	});

	return true;
}


// Test unchecking a toggle
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Toggle_Uncheck, "Houdini.Editor.Parameters.Toggle_Uncheck", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Toggle_Uncheck::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Toggle_Uncheck");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InAssetWrapper->SetBoolParameterValue(TEXT("enable_polyreduce"), false))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("bool"), TEXT("enable_polyreduce"), TEXT("false"), 0);
					}
				});
			});
	});

	return true;
}


// Test unchecking a toggle
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Toggle_Check, "Houdini.Editor.Parameters.Toggle_Check", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Toggle_Check::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Toggle_Check");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InAssetWrapper->SetBoolParameterValue(TEXT("screws"), true))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("bool"), TEXT("screws"), TEXT("true"), 0);
					}
				});
			});
	});

	return true;
}

// Test unchecking a toggle
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Toggle_Check_PreInstantiate, "Houdini.Editor.Parameters.Toggle_Check_PreInstantiate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Toggle_Check_PreInstantiate::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Toggle_Check_PreInstantiate");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update during preinstantiate
				if (!InAssetWrapper->SetBoolParameterValue(TEXT("screws"), true))
				{
					FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("bool"), TEXT("screws"), TEXT("true"), 0);
				}
			});
	});

	return true;
}

// Test pressing a button
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Button, "Houdini.Editor.Parameters.Button", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Button::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Button");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Triggering buttons during pre-instantiation is not supported, we have to wait for 1 cook and then
				// triggering the button and check the results after the 2nd cook
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
					{
						if (!InWrapper->TriggerButtonParameter(TEXT("instances_preset_1_button")))
						{
							FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InWrapper, TEXT("button"), TEXT("instances_preset_1_button"), TEXT("trigger"), 0);
						}
					});
			});
	});

	return true;
}


// Test setting a single value (r) of a color
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Color_Single, "Houdini.Editor.Parameters.Color_Single", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Color_Single::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Color_Single");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InAssetWrapper->SetFloatParameterValue(TEXT("color"), 0.27f))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("color"), TEXT("color"), TEXT("0.27"), 0);
					}
				});
			});
	});

	return true;
}

// Test setting a color parameter
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Color, "Houdini.Editor.Parameters.Color", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Color::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Color");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InAssetWrapper->SetColorParameterValue(TEXT("color"), FColor(153, 50, 204)))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("color"), TEXT("color"), TEXT("(153, 50, 204)"), 0);
					}
				});
			});
	});

	return true;
}


// Test setting a color parameter
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Color_PreInstantiate, "Houdini.Editor.Parameters.Color_PreInstantiate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Color_PreInstantiate::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Color_PreInstantiate");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update during pre-instantiate
				if (!InAssetWrapper->SetColorParameterValue(TEXT("color"), FColor(153, 50, 204)))
				{
					FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("color"), TEXT("color"), TEXT("(153, 50, 204)"), 0);
				}
			});
	});

	return true;
}


// Test setting a radio button / folder
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_String_Choice_Single, "Houdini.Editor.Parameters.String_Choice_Single", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_String_Choice_Single::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("String_Choice_Single");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					if (!InAssetWrapper->SetStringParameterValue(TEXT("copy_type"), TEXT("copy")))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("string_choice"), TEXT("copy_type"), TEXT("copy"), 0);
					}
				});
			});
	});

	return true;
}

// Test selecting a menu item in a single selection menu
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorParametersTest_Menu_Single_Select, "Houdini.Editor.Parameters.Int_Choice_Single", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorParametersTest_Menu_Single_Select::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString TestName = TEXT("Int_Choice_Single");
		const FString MapName = FHoudiniEditorParametersTests::EquivalenceTestMapName + TEXT("/") + TestName;
		const FString ActorName = TestName;
		const FString HDAAssetPath = FHoudiniEditorParametersTests::TestHDAPath + TEXT("Parameters_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr,
			[=](UHoudiniPublicAPIAssetWrapper* const InAssetWrapper, UHoudiniEditorTestObject* const InTestObject)
			{
				if (!FHoudiniEditorTestUtils::CheckAssetWrapperAndTestObject(this, InAssetWrapper, InTestObject))
					return;

				// Trigger a parameter update after the first cook, test setting on pre-instantiate as a separate test
				InTestObject->ExpectedCookCount = 2;
				InTestObject->OnPostProcessingDelegate.AddLambda([=](UHoudiniPublicAPIAssetWrapper* const InWrapper, UHoudiniEditorTestObject* const InTestObj)
				{
					// Enable instancers
					if (!InAssetWrapper->SetStringParameterValue(TEXT("copy_type"), TEXT("pack")))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("string_choice"), TEXT("copy_type"), TEXT("pack"), 0);
					}

					// Set 2 * 3 grid
					if (!InAssetWrapper->SetIntParameterValue(TEXT("rows2"), 2))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("integer"), TEXT("rows2"), TEXT("2"), 0);
					}
					if (!InAssetWrapper->SetIntParameterValue(TEXT("cols2"), 3))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("integer"), TEXT("cols2"), TEXT("3"), 0);
					}

					// Grid orient: YZ Plane
					if (!InAssetWrapper->SetIntParameterValue(TEXT("orient2"), 1))
					{
						FHoudiniEditorTestUtils::LogSetParameterValueViaAPIFailure(this, InAssetWrapper, TEXT("int_choice/ordered_menu"), TEXT("orient2"), TEXT("1"), 0);
					}
				});
			});
	});

	return true;
}

#endif
