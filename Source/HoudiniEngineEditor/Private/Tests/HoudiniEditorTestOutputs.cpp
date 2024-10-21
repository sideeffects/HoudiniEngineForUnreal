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

#include "HoudiniEditorTestOutputs.h"

#include "FileHelpers.h"
#include "HoudiniAsset.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineEditorUtils.h"

#include "HoudiniParameter.h"
#include "HoudiniParameterInt.h"
#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "HoudiniAssetActorFactory.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HoudiniParameterToggle.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "FoliageType_InstancedStaticMesh.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput, "Houdini.UnitTests.OutputTests", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput::RunTest(const FString & Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test enusres that we can cook an HDA multiple times and the outputs are removed on each recook. The test HDA
	///	can create multiple outputs based off the parameters. By changing the parameters we can get different scenarios on
	///	the same HDA.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context. This should be the last step before the tests start as it starts the timeout timer. Note
	// the context live in a SharedPtr<> because each part of the test, in AddCommand(), are executed asyncronously
	// after the test returns.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Outputs/Test_Outputs"), FTransform::Identity, false));
	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Firstly: Enable the cube and disable the height field. This should result in one output, which is a static mesh.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", true, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instances", false, 0);
			Context->StartCookingHDA();
			return true;
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one output.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

			// And the one output should have a static mesh.
			TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL(StaticMeshOutputs.Num(), 1);

			return true;
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Next: Disable the cube and disable the height field. We should have no outputs.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", false, 0);
			Context->StartCookingHDA();
			return true; // This part of the test is complete.
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 0, return true);
			return true; // This part of the test is complete.
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Next: Enable the cube and enable the height field. This should result in two outputs.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", true, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", true, 0);
			Context->StartCookingHDA();
			return true; // This part of the test is complete.
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one mesh and one landscape
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

			// Check one output is a static mesh.
			TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL(StaticMeshOutputs.Num(), 1);

			// Check one output is a landscape.
			TArray<UHoudiniLandscapeTargetLayerOutput*>  LandscapeOutput = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
			HOUDINI_TEST_EQUAL(LandscapeOutput.Num(), 1);

			// Cache the name of the landscape actor. We'll make sure its deleted in the next part of the test.
			FString LandscapeName = LandscapeOutput[0]->Landscape->GetName();
			FString Key = FString("landscape");
			Context->Data.Add(Key, LandscapeName);
			return true; // This part of the test is complete.
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Next: Disable the cube and disable the height field. This should result in no outputs.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", false, 0);
			Context->StartCookingHDA();
			return true; // This part of the test is complete.
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// Check there are no outputs.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 0, return true);

			// Check the landscape actor (whose name was cached from the last test) is deleted.
			FString LandscapeName = Context->Data[TEXT("landscape")];
			AActor * LandscapeActor = FHoudiniEditorUnitTestUtils::GetActorWithName(Context->HAC->GetWorld(), LandscapeName);
			HOUDINI_TEST_NULL(LandscapeActor);

			return true; // This part of the test is complete.
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Next: Just enable instances, make sure they're generated.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instances", true, 0);
			Context->StartCookingHDA();
			return true; // This part of the test is complete.
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one output.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

			// And the one output should have an instanced static mesh.
			TArray<UInstancedStaticMeshComponent*> Components = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, return true);

			UInstancedStaticMeshComponent * Component = Components[0];

			// Check we have 4 instances.
			HOUDINI_TEST_EQUAL_ON_FAIL(Component->GetNumRenderInstances(), 4, return true);

			return true; // This part of the test is complete.
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Next: Just enable instances  as foliage, make sure they're generated.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instances", true, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "as_foliage", true, 0);
			Context->StartCookingHDA();
			return true; // This part of the test is complete.
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one output.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

			// There should be no instance components as we're creating foliage.
			TArray<UInstancedStaticMeshComponent*> ISMs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UInstancedStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(ISMs.Num(), 0, return true);

			// Unreal should have created one Foliage  Instanced Static Mesh.
			TArray<UFoliageInstancedStaticMeshComponent*> Foliages = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UFoliageInstancedStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Foliages.Num(), 1, return true);

			return true; // This part of the test is complete.
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Next: Disable everything. This should result be no outputs.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "cube", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "heightfield", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "instances", false, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "as_foliage", false, 0);
			Context->StartCookingHDA();
			return true; // This part of the test is complete.
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// Check there are no outputs.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 0, return true);

			return true; // This part of the test is complete.
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}


#endif

