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

#include "HoudiniEditorTestBaking.h"

#include "HoudiniParameterInt.h"
#include "HoudiniParameterToggle.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesMeshes, "Houdini.UnitTests.Baking.Meshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInstancesMeshes::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestBaking::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

  AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "meshes", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instance_actors", false, 0);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
		TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 2, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 2, return true);

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

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 2, return false);

		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 2, return true);

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
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 2, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[1]->IsA<UStaticMeshComponent>(), 1, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return false);

		return true;
	}));

	

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestBakingGrouped, "Houdini.UnitTests.Baking.MultipleComponentsOneActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestBakingGrouped::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestBaking::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "meshes", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instance_meshes", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instance_actors", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "geometry_collections", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL(Outputs.Num(), 7);
		TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL(StaticMeshOutputs.Num(), 2);
		TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL(InstancedStaticMeshOutputs.Num(), 1);
		auto GeometryCollections = FHoudiniEditorUnitTestUtils::GetOutputsWithActor<AGeometryCollectionActor>(Outputs);
		HOUDINI_TEST_EQUAL(GeometryCollections.Num(), 1);
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 7, return true);

		// Gather outputs

		TArray<AActor*> Actors = FHoudiniEditorUnitTestUtils::GetOutputActors(BakedOutputs);

		// There should be one geometry collection actor.
		TArray<AGeometryCollectionActor*> GeometryCollectionActors = FHoudiniEditorUnitTestUtils::FilterActors<AGeometryCollectionActor>(Actors);
		HOUDINI_TEST_EQUAL_ON_FAIL(GeometryCollectionActors.Num(), 1, return true);

		TArray<AActor*> StaticMeshActors = FHoudiniEditorUnitTestUtils::FilterActorsWithComponent<UStaticMeshComponent>(Actors);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshActors.Num(), 4, return true);

		TArray<AActor*> InstancedStaticMeshActors = FHoudiniEditorUnitTestUtils::FilterActorsWithComponent<UInstancedStaticMeshComponent>(Actors);
		HOUDINI_TEST_EQUAL_ON_FAIL(InstancedStaticMeshActors.Num(), 4, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestBakingGroupedToBlueprint, "Houdini.UnitTests.Baking.MultipleComponentsToOneBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestBakingGroupedToBlueprint::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking multiple components to a single blueprint
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestBaking::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "meshes", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instance_meshes", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instance_actors", true, 0);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 4, return true);
		TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 2, return true);
		TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(InstancedStaticMeshOutputs.Num(), 1, return true);

		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, EHoudiniEngineBakeOption::ToBlueprint, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 4, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TSet<FString> BlueprintNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Blueprint));
				UWorld * World = Context->HAC->GetHACWorld();
				AActor* Actor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, FVector::ZeroVector, FRotator::ZeroRotator);

				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				TArray<UActorComponent*> Components;
				Actor->GetComponents(Components);

				TArray<UStaticMeshComponent*> MeshComponents = FHoudiniEditorUnitTestUtils::FilterComponents<UStaticMeshComponent>(Components);
				HOUDINI_TEST_EQUAL_ON_FAIL(MeshComponents.Num(), 2, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(MeshComponents[0]->IsA<UStaticMeshComponent>(), true, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(MeshComponents[1]->IsA<UStaticMeshComponent>(), true, continue);

				TArray<UInstancedStaticMeshComponent*> InstancedComponents = FHoudiniEditorUnitTestUtils::FilterComponents<UInstancedStaticMeshComponent>(Components);;
				HOUDINI_TEST_EQUAL_ON_FAIL(InstancedComponents.Num(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(InstancedComponents[0]->IsA<UInstancedStaticMeshComponent>(), true, continue);

			    BlueprintNames.Add(*OutputObject.Blueprint);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(BlueprintNames.Num(), 1, return true);

		return true;
	}));

	return true;
}



#endif

