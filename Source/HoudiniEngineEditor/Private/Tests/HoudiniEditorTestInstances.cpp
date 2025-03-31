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
#include "HoudiniEngineCommands.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"

#include "HoudiniParameterToggle.h"
#include "InstancedFoliage.h"
#include "Chaos/HeightField.h"
#include "Materials/Material.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "HoudiniEditorTestInstances.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
#include "Math/UnrealMathUtility.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "LevelInstance/LevelInstanceComponent.h"
#endif


FVector FHoudiniInstanceAutomationTest::GetHDAInstancePosition(int Index)
{
	// the Test HDA uses a magic formula to calculate the position. This is replicated here, but with Y/Z
	// swapped....

	FVector ExpectedGlobalPosition;
	ExpectedGlobalPosition.X = 10.0 + Index * 10;
	ExpectedGlobalPosition.Z = 20.0;
	ExpectedGlobalPosition.Y = 30.0 + Index * 20;

	// ... and scale by 100 (as H->U conversion always does)
	ExpectedGlobalPosition = ExpectedGlobalPosition * 100.0f;

	return ExpectedGlobalPosition;
}

void FHoudiniInstanceAutomationTest::CheckPositions(const TArray<FVector>& Positions, int StartIndex)
{
	for(int Index = 0; Index < Positions.Num(); Index++)
	{
		int HDA_AttribIndex = Index + StartIndex;

		FVector ExpectedGlobalPosition = GetHDAInstancePosition(HDA_AttribIndex);

		HOUDINI_TEST_EQUALISH_ON_FAIL(Positions[Index], ExpectedGlobalPosition, 0.1, break);
	}
}

TArray<UFoliageType*> FHoudiniInstanceAutomationTest::GetAllFoliageTypes(UWorld* InWorld)
{
	TSet<UFoliageType*> Results;

	for (TActorIterator<AActor> It(InWorld, AInstancedFoliageActor::StaticClass()); It; ++It)
	{
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(*It);

		auto InstanceMap = IFA->GetAllInstancesFoliageType();
		for (auto& InstanceIt : InstanceMap)
		{
			FFoliageInfo* FoliageInfo = InstanceIt.Value;
			Results.Add(InstanceIt.Key);
		}
	}

	TArray<UFoliageType*> Ar = Results.Array();
	return Ar;
}

TArray<FFoliageInstance> FHoudiniInstanceAutomationTest::GetAllFoliageInstances(UWorld* InWorld, UFoliageType* FoliageType)
{
	TArray<FFoliageInstance> Results;

	for (TActorIterator<AActor> It(InWorld, AInstancedFoliageActor::StaticClass()); It; ++It)
	{
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(*It);

		auto InstanceMap = IFA->GetAllInstancesFoliageType();
		for (auto& InstanceIt : InstanceMap)
		{
			FFoliageInfo* FoliageInfo = InstanceIt.Value;
			if (InstanceIt.Key == FoliageType)
				Results.Append(FoliageInfo->Instances);

		}
	}
	return Results;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesActors, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.InstancedMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInstancesActors::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of instances meshes
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniInstanceAutomationTest::BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

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
			Context->Bake(BakeSettings);

			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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

					TArray<UInstancedStaticMeshComponent*> Components;
					Actor->GetComponents(Components);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UInstancedStaticMeshComponent>(), 1, continue);

					UInstancedStaticMeshComponent * ISMC = Components[0];

					const FTransform & Transform = ISMC->GetComponentTransform();

					TArray<FVector> Positions;
					Positions.SetNum(ISMC->PerInstanceSMData.Num());
					for(int Index = 0; Index < ISMC->PerInstanceSMData.Num(); Index++)
					{
						Positions[Index] = Transform.TransformPosition(ISMC->PerInstanceSMData[Index].Transform.GetOrigin());
					}

					CheckPositions(Positions);

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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestBakingInstanceActors, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.InstancedActors", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestBakingInstanceActors::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of instanced actors.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.Blueprint'/Game/TestObjects/BP_Cube.BP_Cube'" ,0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestBakingSplitInstanceMeshes, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.SplitInstances", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestBakingSplitInstanceMeshes::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of split instances.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", true, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 4, return true);

		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Part 2: Test baking multiple components to a single actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have the instances. Build an array of instances.

		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				TArray<UInstancedStaticMeshComponent*> Components;
				Actor->GetComponents(Components);

				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 4, continue);

				// Each component should have a different transform using unreal_instance_origin. Since we don't know which
				// component came from which split we need to look them up by position. We have 4 instancers, each containing
				// 25 points.

				TArray<FVector> Origins = {
					GetHDAInstancePosition(0),
					GetHDAInstancePosition(25),
					GetHDAInstancePosition(50),
					GetHDAInstancePosition(75)
				};

				for(int Index = 0; Index < Components.Num(); Index++)
				{
					int OriginIndex = 0;
					for(; OriginIndex < Origins.Num(); OriginIndex++)
					{
						const FTransform RelativeTransform = Components[Index]->GetRelativeTransform();
						if (RelativeTransform.GetLocation().Equals(Origins[OriginIndex], 0.1))
							break;
					}
					if (OriginIndex == Origins.Num())
					{
						TestEqual(TEXT("Failed to find a component with an expected origin"), false, true);
						return true;
					}

					HOUDINI_TEST_EQUAL_ON_FAIL(Components[Index]->IsA<UInstancedStaticMeshComponent>(), 1, continue);
					HOUDINI_TEST_EQUAL(Components[Index]->GetNumRenderInstances(), 25);
					const FTransform & Transform = Components[Index]->GetRelativeTransform();

					TArray<FVector> InstancePositions;
					InstancePositions.SetNum(Components[Index]->GetNumRenderInstances());
					for (int InstanceIndex = 0; InstanceIndex < Components[Index]->PerInstanceSMData.Num(); InstanceIndex++)
					{
						InstancePositions[InstanceIndex] = 
							Transform.TransformPosition(Components[Index]->PerInstanceSMData[InstanceIndex].Transform.GetOrigin());
					}
					CheckPositions(InstancePositions, OriginIndex * 25);
				}

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSingleInstancedMesh, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.SingleInstancedMeshes", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSingleInstancedMesh::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of a single instance of a mesh.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);
	
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 1, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesHSM, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.HierarchicalInstancedStaticMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInstancesHSM::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of hierarchical instanced meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_LODs.SM_LODs'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
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

				UHierarchicalInstancedStaticMeshComponent* ISMC = Components[0];
				TArray<FVector> Positions;
				Positions.SetNum(ISMC->PerInstanceSMData.Num());
				for (int Index = 0; Index < ISMC->PerInstanceSMData.Num(); Index++)
					Positions[Index] = ISMC->PerInstanceSMData[Index].Transform.GetOrigin();

				CheckPositions(Positions);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPackedInstances, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.PackedInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPackedInstances::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of packed instanced meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, PackedInstancesHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "num_copies", 3, 0);
		Context->StartCookingHDA();
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
		TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(InstancedStaticMeshComponents.Num(), 1, return true);
		auto InstancedStaticMeshComponent = InstancedStaticMeshComponents[0];


		HOUDINI_TEST_EQUAL_ON_FAIL(InstancedStaticMeshComponent->GetNumRenderInstances(), 3, return true);

		// Check expected positions and scales on each object.
		float PrevScale = 1.0f;
		for(int Index = 0; Index < 3; Index++)
		{
			auto InstanceTransform = InstancedStaticMeshComponent->PerInstanceSMData[Index].Transform;
			FVector Pos = InstanceTransform.GetOrigin();
			FVector ExpectedPos = FVector(Index * 200.0, 0.0, 0.0);
			HOUDINI_TEST_EQUAL(Pos, ExpectedPos);

			FVector Scale = InstanceTransform.GetScaleVector();
			FVector ExpectedScale = FVector(PrevScale, PrevScale, PrevScale);
			HOUDINI_TEST_EQUAL(Scale, ExpectedScale);
			PrevScale *= 1.5f;

		}

		TArray<UStaticMesh*> StaticMeshes = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UStaticMesh>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshes.Num(), 1, return true);

		return true;
	}));


	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSinglePackedInstance, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.SinglePackedInstance", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSinglePackedInstance::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of 1 packed instanced meshes - this should result in a single instance.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, PackedInstancesHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "num_copies", 1, 0);
		Context->StartCookingHDA();
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
		TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(InstancedStaticMeshComponents.Num(), 0, return true);

		TArray<UStaticMeshComponent*> StaticMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshComponents.Num(), 1, return true);


		TArray<UStaticMesh*> StaticMeshes = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UStaticMesh>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshes.Num(), 1, return true);


		return true;
	}));


	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestFoliageStaticMesh, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.FoliageStaticMesh", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestFoliageStaticMesh::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking using a custom mesh. A foliage type should be created.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		// Set parameters.

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", true, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		// Post cook, check results.
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		auto OutputObjects = FHoudiniEditorUnitTestUtils::GetOutputsWithFoliageType(Outputs);


		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
		UFoliageType * FoliageType = OutputObjects[0]->FoliageType;

		TArray<FFoliageInstance>  Instances = GetAllFoliageInstances(Context->GetWorld(), FoliageType);
		HOUDINI_TEST_EQUAL(Instances.Num(), 100);

		TArray<FVector> Positions;
		Positions.SetNum(Instances.Num());
		for(int Index = 0; Index < Instances.Num(); Index++)
			Positions[Index] = Instances[Index].Location;

		CheckPositions(Positions);

		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		// Post bake, check results.

		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				HOUDINI_TEST_NOT_NULL(OutputObject.FoliageType.Get());
				HOUDINI_TEST_EQUAL(OutputObject.FoliageInstancePositions.Num(), 100);

				CheckPositions(OutputObject.FoliageInstancePositions);

			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestFoliageUserFoliageType, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.UserFoliageType", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestFoliageUserFoliageType::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking user a custom foliage type.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	const char* UserFoliageType = "/Script/Foliage.FoliageType_InstancedStaticMesh'/Game/TestObjects/FoliageType.FoliageType'";

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, UserFoliageType]()
	{
		// Set parameters

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", UserFoliageType, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", true, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "instance_origin", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		// Post Cook, test.

		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		auto OutputObjects = FHoudiniEditorUnitTestUtils::GetOutputsWithFoliageType(Outputs);


		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
		UFoliageType* FoliageType = OutputObjects[0]->FoliageType;

		TArray<FFoliageInstance>  Instances = GetAllFoliageInstances(Context->GetWorld(), FoliageType);
		HOUDINI_TEST_EQUAL(Instances.Num(), 100);

		UFoliageType* UserFoliageType = Cast<UFoliageType>(OutputObjects[0]->UserFoliageType);
		HOUDINI_TEST_NOT_NULL(UserFoliageType);

		TArray<FVector> Positions;
		Positions.SetNum(Instances.Num());
		for (int Index = 0; Index < Instances.Num(); Index++)
			Positions[Index] = Instances[Index].Location;

		CheckPositions(Positions);

		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, UserFoliageType]()
	{
		// Bake

		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				HOUDINI_TEST_NOT_NULL(OutputObject.FoliageType.Get());
				HOUDINI_TEST_EQUAL(OutputObject.FoliageInstancePositions.Num(), 100);

				CheckPositions(OutputObject.FoliageInstancePositions);

				FString AssetName(UserFoliageType);

				UFoliageType* UserFoliageTypeObject = Cast<UFoliageType>(StaticLoadObject(UObject::StaticClass(), nullptr, *AssetName));;

				HOUDINI_TEST_EQUAL(UserFoliageTypeObject, OutputObject.FoliageType.Get());

				TArray<UFoliageType*> FoliageTypes = GetAllFoliageTypes(Context->GetWorld());
				HOUDINI_TEST_EQUAL_ON_FAIL(FoliageTypes.Num(), 1, return true);
				HOUDINI_TEST_EQUAL(FoliageTypes[0], UserFoliageTypeObject);
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLevelInstances, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.LevelInstances", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLevelInstances::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of a single instance of a mesh.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.World'/Game/TestObjects/LevelInstance.LevelInstance'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 10, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<FHoudiniOutputObject*> LevelInstances = FHoudiniEditorUnitTestUtils::GetOutputsWithActor<ALevelInstance>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(LevelInstances.Num(), 1, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(LevelInstances[0]->OutputActors.Num(), 10, return true);

		for(int ActorIndex = 0; ActorIndex < 10; ActorIndex++)
		{
			ALevelInstance * LevelInstance = Cast<ALevelInstance>(LevelInstances[0]->OutputActors[ActorIndex].Get());
			HOUDINI_TEST_NOT_NULL(LevelInstance);
		}
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				ActorNames.Append(OutputObject.LevelInstanceActors);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 10, return true);

		return true;
	}));

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestActorInstances, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.ActorInstances", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestActorInstances::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of a single instance of a mesh.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.Blueprint'/Game/TestObjects/BP_Cube.BP_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 10, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				ActorNames.Append(OutputObject.InstancedActors);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 10, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestProxyMeshInstances, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.ProxyMeshes", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestProxyMeshInstances::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of a single instance of a mesh.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(true);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/HoudiniEngineRuntime.HoudiniStaticMesh'/Game/TestObjects/HoudiniMesh.HoudiniMesh'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 1, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPDGInstances, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.PDGInstances", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPDGInstances::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test PDG.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, PDGHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(true);

	// HDA Path and kick Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FString TempDir = FPaths::ProjectIntermediateDir() / "Temp";
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "working_dir", TempDir, 0);

		Context->StartCookingHDA();
		return true;
	}));

	// kick PDG Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingSelectedTOPNetwork();
		return true;
	}));

	// Bake and check results.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		UHoudiniPDGAssetLink* AssetLink = Context->GetPDGAssetLink();
		UTOPNetwork* Network = AssetLink->GetTOPNetwork(0);
		HOUDINI_TEST_NOT_NULL(Network);

		UTOPNode* Node = nullptr;
		for (UTOPNode* It : Network->AllTOPNodes)
		{
			if (It->NodeName == "HE_OUT_X")
			{
				Node = It;
				break;
			}
		}
		HOUDINI_TEST_NOT_NULL(Node);

		HOUDINI_TEST_EQUAL_ON_FAIL(Node->WorkResult.Num(), 10, return true);


		for( auto& Result : Node->WorkResult)
		{
			auto ResultOutputs = Result.ResultObjects[0].GetResultOutputs();
			HOUDINI_TEST_EQUAL_ON_FAIL(ResultOutputs.Num(), 1, return true);

			UHoudiniOutput * Output = ResultOutputs[0];

			TArray<FHoudiniOutputObject> OutputObjects;
			Output->GetOutputObjects().GenerateValueArray(OutputObjects);

			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);

			const FHoudiniOutputObject & OutputObject = OutputObjects[0];

			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents[0]->IsA(UInstancedStaticMeshComponent::StaticClass()), 1, return true);
		}

		return true;
	}));

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPDGInstancesAsync, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.PDGInstances", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPDGInstancesAsync::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test PDG.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	FHoudiniEngineCommands::SetPDGCommandletEnabled(true);
	FHoudiniEngineCommands::StartPDGCommandlet();

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, PDGHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(true);

	// HDA Path and kick Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FString TempDir = FPaths::ProjectIntermediateDir() / "Temp";
			SET_HDA_PARAMETER(Context, UHoudiniParameterString, "working_dir", TempDir, 0);

			Context->StartCookingHDA();
			return true;
		}));

	// kick PDG Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			Context->StartCookingSelectedTOPNetwork();
			return true;
		}));

	// Bake and check results.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			UHoudiniPDGAssetLink* AssetLink = Context->GetPDGAssetLink();
			UTOPNetwork* Network = AssetLink->GetTOPNetwork(0);
			HOUDINI_TEST_NOT_NULL(Network);

			UTOPNode* Node = nullptr;
			for (UTOPNode* It : Network->AllTOPNodes)
			{
				if (It->NodeName == "HE_OUT_X")
				{
					Node = It;
					break;
				}
			}
			HOUDINI_TEST_NOT_NULL(Node);

			HOUDINI_TEST_EQUAL_ON_FAIL(Node->WorkResult.Num(), 10, return true);


			for (auto& Result : Node->WorkResult)
			{
				auto ResultOutputs = Result.ResultObjects[0].GetResultOutputs();
				HOUDINI_TEST_EQUAL_ON_FAIL(ResultOutputs.Num(), 1, return true);

				UHoudiniOutput* Output = ResultOutputs[0];

				TArray<FHoudiniOutputObject> OutputObjects;
				Output->GetOutputObjects().GenerateValueArray(OutputObjects);

				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);

				const FHoudiniOutputObject& OutputObject = OutputObjects[0];

				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents.Num(), 1, return true);
				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents[0]->IsA(UInstancedStaticMeshComponent::StaticClass()), 1, return true);
			}

			FHoudiniEngineCommands::StopPDGCommandlet();
			FHoudiniEngineCommands::SetPDGCommandletEnabled(false);
			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSplitInstanceMeshesMaterials, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.SplitInstancesMaterials", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSplitInstanceMeshesMaterials::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of split instances.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", true, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "custom_materials", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two actors
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TArray<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				FString InstanceActorName = OutputObject.Actor;
				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *InstanceActorName));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 4, return true);

		return true;
	}));



	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have the instances. Build an array of instances.

		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				const int NumInstanceComponents = 4;

				TArray<UInstancedStaticMeshComponent*> Components;
				Actor->GetComponents(Components);
				HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), NumInstanceComponents, continue);
				for (int Index = 0; Index < NumInstanceComponents; Index++)
				{
					HOUDINI_TEST_EQUAL_ON_FAIL(Components[Index]->IsA<UInstancedStaticMeshComponent>(), 1, continue);
					HOUDINI_TEST_EQUAL(Components[Index]->GetNumRenderInstances(), 25);

					UMaterial * Material = Cast<UMaterial>(Components[Index]->GetMaterial(0));
					FString MaterialName = Material->GetPathName();
					FString ExpectName = FString::Printf(TEXT("/Game/TestObjects/InstanceMaterial_%d.InstanceMaterial_%d"), Index, Index);
					HOUDINI_TEST_EQUAL(MaterialName, ExpectName);


				}

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		TArray<FVector> InstancePositions;
		InstancePositions.Reserve(100);

		AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *(*ActorNames.CreateConstIterator())));
		TArray<UInstancedStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (int Index = 0; Index < Components.Num(); Index++)
		{
			for (int InstanceIndex = 0; InstanceIndex < Components[Index]->PerInstanceSMData.Num(); InstanceIndex++)
			{
				InstancePositions.Add(Components[Index]->PerInstanceSMData[InstanceIndex].Transform.GetOrigin());
			}
		}

		HOUDINI_TEST_EQUAL(InstancePositions.Num(), 100);
		InstancePositions.Sort([](const FVector& First, const FVector& Second) { return First.X < Second.X; });
		CheckPositions(InstancePositions);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSplitInstanceCustomFloats, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.SplitInstanceCustomData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSplitInstanceCustomFloats::RunTest(const FString& Parameters)
{

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, BakingHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "instance_object", "/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'", 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "max_instances", 100, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "split_instance_meshes", true, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "foliage", false, 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "custom_floats", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two actors
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TArray<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				FString InstanceActorName = OutputObject.Actor;
				AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *InstanceActorName));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 4, return true);

		return true;
	}));



	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		// Go through each output and check we have the instances. Build an array of instances.

		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		TArray<FVector> InstancePositions;
		InstancePositions.Reserve(100);

		AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *(*ActorNames.CreateConstIterator())));
		TArray<UInstancedStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (int Index = 0; Index < Components.Num(); Index++)
		{
			for (int InstanceIndex = 0; InstanceIndex < Components[Index]->PerInstanceSMData.Num(); InstanceIndex++)
			{
				InstancePositions.Add(Components[Index]->PerInstanceSMData[InstanceIndex].Transform.GetOrigin());
			}

			HOUDINI_TEST_EQUAL_ON_FAIL(Components[Index]->IsA<UInstancedStaticMeshComponent>(), 1, continue);
			HOUDINI_TEST_EQUAL(Components[Index]->GetNumRenderInstances(), 25);

			HOUDINI_TEST_EQUAL(Components[Index]->NumCustomDataFloats, Index + 1);

			for (int Instance = 0; Instance < Components[Index]->GetNumRenderInstances(); Instance++)
			{
				for (int CustomFloatIndex = 0; CustomFloatIndex < Components[Index]->NumCustomDataFloats; CustomFloatIndex++)
				{
					// Use unique calculated values so we can test values arrived in the correct slots.
					float ExpectedValue = Index * 10000 + Instance * 100 + CustomFloatIndex;
					float ActualValue = Components[Index]->PerInstanceSMCustomData[Instance * Components[Index]->NumCustomDataFloats + CustomFloatIndex];
					HOUDINI_TEST_EQUAL(ActualValue, ExpectedValue);
				}
			}
		}

		HOUDINI_TEST_EQUAL(InstancePositions.Num(), 100);
		InstancePositions.Sort([](const FVector& First, const FVector& Second) { return First.X < Second.X; });
		CheckPositions(InstancePositions);

		return true;
	}));

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSplitPackedInstancer, FHoudiniInstanceAutomationTest, "Houdini.UnitTests.Instances.SplitPackedInstancer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSplitPackedInstancer::RunTest(const FString& Parameters)
{

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, SplitPackedInstancesHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two actors
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		FHoudiniBakeSettings BakeSettings;
		BakeSettings.ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerHDA;
		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 2, return true);

		// Go through each output and check we have the instances. Build an array of instances.

		TSet<FString> ActorNames;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				if (!OutputObject.Actor.IsEmpty())
					ActorNames.Add(*OutputObject.Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(ActorNames.Num(), 1, return true);

		TArray<FVector> InstancePositions;
		InstancePositions.Reserve(100);

		AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *(*ActorNames.CreateConstIterator())));
		TArray<UInstancedStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 3, return true);

		for (int Index = 0; Index < Components.Num(); Index++)
		{
			for (int InstanceIndex = 0; InstanceIndex < Components[Index]->PerInstanceSMData.Num(); InstanceIndex++)
			{
				InstancePositions.Add(Components[Index]->PerInstanceSMData[InstanceIndex].Transform.GetOrigin());
			}

			HOUDINI_TEST_EQUAL_ON_FAIL(Components[Index]->IsA<UInstancedStaticMeshComponent>(), 1, continue);
			HOUDINI_TEST_EQUAL(Components[Index]->GetNumRenderInstances(), 10);
		}

		return true;
	}));

	return true;
}


#endif

