#include "../HoudiniEngine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(HoudiniCoreTest, "Houdini.Core.TestAutomation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniCoreTest::RunTest(const FString & Parameters)
{
	return true;
}

#endif