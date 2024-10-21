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

#include "HoudiniEditorTestProxyMesh.h"

#include "HoudiniPDGAssetLink.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestsProxyMeshVertices, "Houdini.UnitTests.ProxyMesh.Vertices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestsProxyMeshVertices::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of hierarchical instanced meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	 
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestProxyMeshes::HDAAsset, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = true;

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<UHoudiniStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithProxyComponent(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 1, return true);

		UHoudiniStaticMesh * Mesh = StaticMeshOutputs[0]->GetMesh();
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Mesh, return true);

		FHoudiniTestMeshData ExpectedMeshData = FHoudiniEditorUnitTestMeshUtils::GetExpectedMeshData();
		FHoudiniTestMeshData ActualData = FHoudiniEditorUnitTestMeshUtils::ExtractMeshData(*Mesh);
		auto Errors = FHoudiniEditorUnitTestMeshUtils::CheckMesh(ExpectedMeshData , ActualData);

		HOUDINI_TEST_EQUAL(Errors.Num(), 0);
		for(auto & Error : Errors)
			HOUDINI_LOG_ERROR(TEXT("Mesh Error: %s"), *Error);

		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Bake
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				TArray<UStaticMeshComponent*> Components;
				Actor->GetComponents(Components);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);

				UStaticMeshComponent * StaticMeshComponent = Cast<UStaticMeshComponent>(Components[0]);
				UStaticMesh * StaticMesh = StaticMeshComponent->GetStaticMesh();

				FHoudiniTestMeshData ExpectedMeshData = FHoudiniEditorUnitTestMeshUtils::GetExpectedMeshData();
				FHoudiniTestMeshData ActualData = FHoudiniEditorUnitTestMeshUtils::ExtractMeshData(*StaticMesh, 0);
				auto Errors = FHoudiniEditorUnitTestMeshUtils::CheckMesh(ExpectedMeshData, ActualData);

				HOUDINI_TEST_EQUAL(Errors.Num(), 0);
				for (auto& Error : Errors)
					HOUDINI_LOG_ERROR(TEXT("Mesh Error: %s"), *Error);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);
		return true;
	}));

	return true;
}


#endif

