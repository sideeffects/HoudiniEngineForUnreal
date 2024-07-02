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
#include "HoudiniParameterString.h"

#include "HoudiniParameterToggle.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "HoudiniEditorTestInstances.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesActors, "Houdini.UnitTests.Instances.InstancedMeshes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInstancesActors::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of instances meshes
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestInstances::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "max_instances", 100, 0);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<UInstancedStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 1, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Step 1: Bake the output using ungroup components. We should have one actor per outputs (so 2 in this case), and one component per
	// actor
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

					ActorNames.Add(*OutputObject.Actor);
				}
			}

			HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

			return true;
		}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Part 2: Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
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

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));


	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestBakingInstanceActors, "Houdini.UnitTests.Instances.InstancedActors", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestBakingInstanceActors::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of instanced actors.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestInstances::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "instance_object", "/Script/Engine.Blueprint'/Game/TestObjects/BP_Cube.BP_Cube'" ,0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "max_instances", 100, 0);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two actors
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Step 1: Bake the output using ungroup components. We should have one actor per outputs (so 2 in this case), and one component per
	// actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TArray<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				for (FString& InstanceActorName : OutputObject.InstancedActors)
				{
					AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *InstanceActorName));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

					ActorNames.Add(*OutputObject.Actor);
				}
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 100, return true);

		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Part 2: Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TArray<FString> ActorNames;
		AActor * ParentActor = nullptr;

		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);
				ParentActor = Actor;

				TArray<UStaticMeshComponent*> Components;
				Actor->GetComponents(Components);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 0, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		// We should have found only one actor.
		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		TArray<AActor*> ChildActors;
		ParentActor->GetAttachedActors(ChildActors);
		HOUDINI_TEST_EQUAL_ON_FAIL(ChildActors.Num(), 100, return true);


		return true;
	}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestBakingSplitInstanceMeshes, "Houdini.UnitTests.Instances.SplitInstances", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestBakingSplitInstanceMeshes::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of split instances.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestInstances::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "split_instance_meshes", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two actors
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Step 1: Bake the output using ungroup components. We should have one actor per outputs (so 2 in this case), and one component per
	// actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TArray<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				FString InstanceActorName  = OutputObject.Actor;
				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *InstanceActorName));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 3, return true);

		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Part 2: Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
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
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 3, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UInstancedStaticMeshComponent>(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[1]->IsA<UInstancedStaticMeshComponent>(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[2]->IsA<UInstancedStaticMeshComponent>(), 1, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSingleInstancedMesh, "Houdini.UnitTests.Instances.SingleInstancedMeshes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSingleInstancedMesh::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of a single instance of a mesh.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestInstances::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;
	
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "max_instances", 1, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 1, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Step 1: Bake the output using ungroup components. We should have one actor per outputs (so 2 in this case), and one component per
	// actor
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

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Part 2: Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
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

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesHSM, "Houdini.UnitTests.Instances.HierarchicalInstancedStaticMeshes", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInstancesHSM::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of hierarchical instanced meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestInstances::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_LODs.SM_LODs'", 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "max_instances", 100, 0);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<UHierarchicalInstancedStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UHierarchicalInstancedStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 1, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Step 1: Bake the output using ungroup components. We should have one actor per outputs (so 2 in this case), and one component per
	// actor
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

				TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
				Actor->GetComponents(Components);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Part 2: Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
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

				TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
				Actor->GetComponents(Components);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	return true;
}
#endif

