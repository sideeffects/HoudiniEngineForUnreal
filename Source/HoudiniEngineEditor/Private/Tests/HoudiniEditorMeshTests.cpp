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

#include "HoudiniEditorMeshTests.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"

FString FHoudiniEditorMeshTests::EquivalenceTestMapName = TEXT("Mesh");
FString FHoudiniEditorMeshTests::TestHDAPath = TEXT("/Game/TestHDAs/Mesh/");

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMeshTest_Single, "Houdini.Editor.Mesh.Single", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMeshTest_Single::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorMeshTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Single");
		const FString HDAAssetPath = FHoudiniEditorMeshTests::TestHDAPath + TEXT("Single");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMeshTest_Multiple, "Houdini.Editor.Mesh.Multiple", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMeshTest_Multiple::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorMeshTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Multiple");
		const FString HDAAssetPath = FHoudiniEditorMeshTests::TestHDAPath + TEXT("Multiple");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMeshTest_LODs_Common, "Houdini.Editor.Mesh.LODs_Common", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMeshTest_LODs_Common::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [=]
	{
		const FString MapName = FHoudiniEditorMeshTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("LODs_Common");
		const FString HDAAssetPath = FHoudiniEditorMeshTests::TestHDAPath + TEXT("LODs_Common");

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, nullptr);
	});

	return true;
}


#endif

