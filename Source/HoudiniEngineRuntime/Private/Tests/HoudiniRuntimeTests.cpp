// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "HoudiniRuntimeTests.h"
#include "HoudiniEngineRuntime.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(HoudiniRuntimeTestAutomation, "Houdini.Runtime.TestAutomation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniRuntimeTestAutomation::RunTest(const FString & Parameters)
{

	return true;
}

#endif