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

#include "HoudiniEditorTestLandscapes.h"

#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterToggle.h"
#include "Landscape.h"
#include "LandscapeSplineControlPoint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "LandscapeEdit.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeSplines_Simple, "Houdini.UnitTests.LandscapeSplines.Simple", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_Simple::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes Splines.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/LandscapeSplines/Test_LandscapeSpline"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		//	SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

		TArray<UHoudiniLandscapeSplinesOutput*> SplineOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeSplinesOutput>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SplineOutputs.Num(), 1, return true);

		TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
		HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
		ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

		//
		// Check control points. 6 are specified in the Test HDA but only 4 should be output (the first and last + those marked
		// with >= 0 ids).
		//

		ULandscapeSplinesComponent * SplineComponent = SplineOutputs[0]->GetLandscapeSplinesComponent();
		auto ControlPoints = SplineComponent->GetControlPoints();

		FTransform Transform = SplineComponent->GetComponentTransform();
		HOUDINI_TEST_EQUAL_ON_FAIL(ControlPoints.Num(), 4, return true);
		TArray<FVector> Positions;
		for (int Index = 0; Index < ControlPoints.Num(); Index++)
		{
			Positions.Add(Transform.TransformPosition(ControlPoints[Index]->Location));
		}
		HOUDINI_TEST_EQUAL(Positions[0], FVector(-50000.0, -50000.0, 0.0), 0.1);
		HOUDINI_TEST_EQUAL(Positions[1], FVector(0.0, -50000.0, 0.0), 0.1);
		HOUDINI_TEST_EQUAL(Positions[2], FVector(0.0, 50000.0, 0.0), 0.1);
		HOUDINI_TEST_EQUAL(Positions[3], FVector(0.0, 0.0, 0.0), 0.1);

		HOUDINI_TEST_EQUAL(IsValid(ControlPoints[1]->Mesh), true);
		HOUDINI_TEST_EQUAL(ControlPoints[1]->MeshScale, FVector(99.0, 99.0, 99.0));

		//
		// Check segments
		//
		auto Segments = SplineComponent->GetSegments();
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments.Num(), 3, return true);

		return true;
	}));


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeSplines_WorldPartition, "Houdini.UnitTests.LandscapeSplines.WorldPartition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_WorldPartition::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/LandscapeSplines/Test_LandscapeSpline"), FTransform::Identity, true));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		//	SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

		TArray<UHoudiniLandscapeSplinesOutput*> SplineOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeSplinesOutput>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SplineOutputs.Num(), 1, return true);

		TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
		HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
		ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

		//
		// Check control points. 6 are specified in the Test HDA but only 4 should be output (the first and last + those marked
		// with >= 0 ids).
		//

		ULandscapeSplinesComponent* SplineComponent = SplineOutputs[0]->GetLandscapeSplinesComponent();
		auto ControlPoints = SplineComponent->GetControlPoints();

		FTransform Transform = SplineComponent->GetComponentTransform();
		HOUDINI_TEST_EQUAL_ON_FAIL(ControlPoints.Num(), 4, return true);
		TArray<FVector> Positions;
		for (int Index = 0; Index < ControlPoints.Num(); Index++)
		{
			Positions.Add(Transform.TransformPosition(ControlPoints[Index]->Location));
		}
		HOUDINI_TEST_EQUAL(Positions[0], FVector(-50000.0, -50000.0, 0.0), 0.1);
		HOUDINI_TEST_EQUAL(Positions[1], FVector(0.0, -50000.0, 0.0), 0.1);
		HOUDINI_TEST_EQUAL(Positions[2], FVector(0.0, 50000.0, 0.0), 0.1);
		HOUDINI_TEST_EQUAL(Positions[3], FVector(0.0, 0.0, 0.0), 0.1);

		HOUDINI_TEST_EQUAL(IsValid(ControlPoints[1]->Mesh), true);
		HOUDINI_TEST_EQUAL(ControlPoints[1]->MeshScale, FVector(99.0, 99.0, 99.0));

		//
		// Check segments
		//
		auto Segments = SplineComponent->GetSegments();
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments.Num(), 3, return true);

		return true;
	}));


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}

#endif