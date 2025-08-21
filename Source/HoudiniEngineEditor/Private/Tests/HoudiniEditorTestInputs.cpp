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


#include "HoudiniEditorTestMeshMisc.h"

#include "HoudiniApi.h"
#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "Misc/DefaultValueHelper.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMiscMeshes_SplineMeshInput, "Houdini.UnitTests.Inputs.SplineMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMiscMeshes_SplineMeshInput::RunTest(const FString& Parameters)
{
	// Test we can input spline meshes correctly to Houdini From Unreal. We input two mesh components which share the same input
	// UStaticMesh to make sure the ref input system creates two different copies of the mesh.

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FString(TEXT("/Game/TestHDAs/Mesh/Misc/TestSplineMesh.umap"))));
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

			// We should have two outputs, two meshes
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
			TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 2, return true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FHoudiniBakeSettings BakeSettings;
			Context->Bake(BakeSettings);

			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
			// There should be two outputs as we have two meshes.
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 2, return true);

			// Go through each output and check we have two actors with one mesh component each.
			TArray<AActor*> Actors;
			for(auto& BakedOutput : BakedOutputs)
			{
				for(auto It : BakedOutput.BakedOutputObjects)
				{
					FHoudiniBakedOutputObject& OutputObject = It.Value;

					AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

					TArray<UStaticMeshComponent*> Components;
					Actor->GetComponents(Components);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);

					Actors.Add(Actor);
				}
			}

			HOUDINI_TEST_EQUAL_ON_FAIL(Actors.Num(), 2, return false);

			TArray<FBoxSphereBounds> Bounds;
			for (auto Actor : Actors)
			{
				TArray<UStaticMeshComponent*> StaticMeshComponents;
				Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
				HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshComponents.Num(), 1, return false);
				UStaticMeshComponent* SMC = StaticMeshComponents[0];

				Bounds.Add(SMC->Bounds.GetBox());

			}

			Bounds.Sort([](const FBoxSphereBounds& A, const FBoxSphereBounds& B)
				{
					return A.GetBox().GetVolume() < B.GetBox().GetVolume();
				});

			// We have exported the cube as two spline meshes, they should be different sizes.

			HOUDINI_TEST_EQUAL(Bounds[0].Origin, FVector3d(250.0, 100.0, 0.0));
			HOUDINI_TEST_EQUAL(Bounds[0].BoxExtent, FVector3d(250.0, 50.0, 50.0));
			HOUDINI_TEST_EQUAL(Bounds[1].Origin, FVector3d(500.0, -100.0, 0.0));
			HOUDINI_TEST_EQUAL(Bounds[1].BoxExtent, FVector3d(500.0, 50.0, 50.0));

			return true;
		}));

	return true;
}
