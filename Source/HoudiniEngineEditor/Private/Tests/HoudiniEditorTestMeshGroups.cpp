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

#include "HoudiniEditorTestMeshGroups.h"

#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "Landscape.h"
#include "LandscapeSplineControlPoint.h"
#include "NetworkMessage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/HeightField.h"
#include "PhysicsEngine/BodySetup.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "LandscapeEdit.h"


FString BoolToString(bool value)
{
	return value ? TEXT("True") : TEXT("False");
}

void FHoudiniMeshAutomationTest::CheckMeshType(const FStaticMeshLODResources & Resource, EHoudiniMeshType MeshType)
{
	FString Result;

	auto& Section = Resource.Sections[0];

	int ExpectedNumTriangles = 0;
	switch(MeshType)
	{
	case EHoudiniMeshType::Cube:
		ExpectedNumTriangles = 12;
		break;
	case EHoudiniMeshType::Sphere:
		ExpectedNumTriangles = 528;
		break;
	case EHoudiniMeshType::Torus:
		break;
	default:
		break;
	}

	HOUDINI_TEST_EQUAL(Section.NumTriangles, ExpectedNumTriangles);

}


FString FHoudiniMeshAutomationTest::GetCollisionTypeName(EHoudiniCollisionType Type)
{
	switch(Type)
	{
	case EHoudiniCollisionType::None:
		return TEXT("");
		break;
	case EHoudiniCollisionType::MainMesh:
		return TEXT("");
		break;
	case EHoudiniCollisionType::SimpleBox:
		return TEXT("simple_box");
		break;
	case EHoudiniCollisionType::SimpleSphere:
		return TEXT("simple_sphere");
		break;
	case EHoudiniCollisionType::SimpleCapsule:
		return TEXT("simple_capsule");
		break;
	case EHoudiniCollisionType::Kdop10x:
		return TEXT("simple_kdop10x");
		break;
	case EHoudiniCollisionType::Kdop10y:
		return TEXT("simple_kdop10y");
		break;
	case EHoudiniCollisionType::Kdop10z:
		return TEXT("simple_kdop10z");
		break;
	case EHoudiniCollisionType::Kdop18:
		return TEXT("simple_kdop18");
		break;
	case EHoudiniCollisionType::Kdop26:
		return TEXT("simple_kdop266");
	}
	return TEXT("unknown_collision_type");
}

void FHoudiniMeshAutomationTest::CheckMesh(UStaticMeshComponent* StaticMeshComponent, const FHoudiniMeshCheck & MeshCheck)
{
	HOUDINI_TEST_EQUAL_ON_FAIL(MeshCheck.bComponentIsVisible, StaticMeshComponent->IsVisible(), return);

	auto StaticMesh = StaticMeshComponent->GetStaticMesh();

	HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh.Get(),  return);

	FStaticMeshRenderData * RenderData =  StaticMesh->GetRenderData();
	HOUDINI_TEST_NOT_NULL_ON_FAIL(RenderData, return);

	int NumExpectedLODs = MeshCheck.LODMeshes.Num();
	HOUDINI_TEST_EQUAL(NumExpectedLODs, RenderData->LODResources.Num());

	if (NumExpectedLODs == RenderData->LODResources.Num())
	{
		// now check we got the right object in the right slot.
		for(int LODIndex = 0; LODIndex < NumExpectedLODs; LODIndex++)
		{
			CheckMeshType(RenderData->LODResources[LODIndex], MeshCheck.LODMeshes[LODIndex]);
		}
	}

	bool bMeshHasComplexCollision = IsValid(StaticMesh->ComplexCollisionMesh);

	HOUDINI_TEST_EQUAL(bMeshHasComplexCollision, MeshCheck.ComplexCollisionType != EHoudiniMeshType::None);

	if (bMeshHasComplexCollision)
	{
		UStaticMesh * ComplexCollisionMesh = StaticMesh->ComplexCollisionMesh;
		FStaticMeshRenderData* CollisionRenderData = ComplexCollisionMesh->GetRenderData();
		HOUDINI_TEST_NOT_NULL_ON_FAIL(CollisionRenderData, return);
		HOUDINI_TEST_EQUAL_ON_FAIL(CollisionRenderData->LODResources.Num(), 1, return);

		CheckMeshType(CollisionRenderData->LODResources[0], MeshCheck.ComplexCollisionType);
	}

	UBodySetup *BodySetup = StaticMesh->GetBodySetup();
	HOUDINI_TEST_NOT_NULL_ON_FAIL(BodySetup, return);
	FKAggregateGeom & AggGeom = BodySetup->AggGeom;

	HOUDINI_TEST_EQUAL(AggGeom.BoxElems.Num(), MeshCheck.NumBoxCollisions);
	HOUDINI_TEST_EQUAL(AggGeom.SphereElems.Num(), MeshCheck.NumSphereCollisions);
	HOUDINI_TEST_EQUAL(AggGeom.SphylElems.Num(), MeshCheck.NumSphylCollisions);
	HOUDINI_TEST_EQUAL(AggGeom.ConvexElems.Num(), MeshCheck.NumConvexCollisions);

}

TSharedPtr<FHoudiniTestContext> FHoudiniMeshAutomationTest::LoadHDA(FAutomationTestBase* Test)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(Test, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(Test, TEXT("/Game/TestHDAs/Mesh/Test_MeshGroups"), FTransform::Identity, false));
	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	return Context;
}

TArray<UStaticMeshComponent*> FHoudiniMeshAutomationTest::GetStaticMeshes(TSharedPtr<FHoudiniTestContext> Context)
{
	TArray<UHoudiniOutput*> Outputs;
	Context->HAC->GetOutputs(Outputs);

	TArray<UStaticMeshComponent*> StaticMeshOutputComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
	return StaticMeshOutputComponents;
}


void FHoudiniMeshAutomationTest::ExecuteMeshTest(TSharedPtr<FHoudiniTestContext> Context, const FHoudiniTestSettings& Settings, const FHoudiniMeshCheck& MeshCheck)
{
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, Settings]()
	{
		// Set number of multiparms; cook before we update he values.
		SET_HDA_PARAMETER_NUM_ELEMENTS(Context->HAC, UHoudiniParameterMultiParm, "cube_groups", Settings.CubeGroups.Num());
		SET_HDA_PARAMETER_NUM_ELEMENTS(Context->HAC, UHoudiniParameterMultiParm, "sphere_groups", Settings.SphereGroups.Num());
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, Settings]()
	{
		// Set the parm values.
		for(int Index = 0; Index < Settings.CubeGroups.Num(); Index++)
		{
			FString ParmName = FString::Format(TEXT("cube_group{0}"), { Index + 1});
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, TCHAR_TO_ANSI(*ParmName), Settings.CubeGroups[Index], 0);
		}
		for (int Index = 0; Index < Settings.SphereGroups.Num(); Index++)
		{
			FString ParmName = FString::Format(TEXT("sphere_group{0}"), { Index + 1 });
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, TCHAR_TO_ANSI(*ParmName), Settings.SphereGroups[Index], 0);
		}

		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "pack", Settings.bPack, 0);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, MeshCheck]()
	{
		// Check results
		auto StaticMeshes = FHoudiniMeshAutomationTest::GetStaticMeshes(Context);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshes.Num(), 1, return true);
		FHoudiniMeshAutomationTest::CheckMesh(StaticMeshes[0], MeshCheck);

		return true;
	}));
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_None, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.None",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_None::RunTest(const FString& Parameters)
{
	// No special groups should export a single mesh,
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_RenderedCollisionGeo, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.RenderedCollisionGeo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_RenderedCollisionGeo::RunTest(const FString& Parameters)
{
	// rendered_collision_geo should export a simple mesh
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_CollisionGeo, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.CollisionGeo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_CollisionGeo::RunTest(const FString& Parameters)
{
	// collision_geo should export a simple mesh where the component is invisible.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("collision_geo");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;
	MeshCheck.bComponentIsVisible = false;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_MainGeo, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.MainGeo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_MainGeo::RunTest(const FString& Parameters)
{
	// main_geo should export a simple mesh
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("main_geo");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleKDOP18Collision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.SimpleKDOP18Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleKDOP18Collision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop18");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleKDOP26Collision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.SimpleKDOP26Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleKDOP26Collision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop26");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleKDOP10xCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.SimpleKDOP10xCollision", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleKDOP10xCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop10x");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleKDOP10yCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.SimpleKDOP10yCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleKDOP10yCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop10y");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleKDOP10zCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.SimpleKDOP10zCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleKDOP10zCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop10z");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleBoxCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.SimpleBoxCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleBoxCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_box");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}
IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_SimpleBoxAndSphereCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.Simple1Box1SphereCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_SimpleBoxAndSphereCollision::RunTest(const FString& Parameters)
{
	// Test that 2 simple collisions can be added to the same mesh.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.SphereGroups.Add("collision_geo_simple_sphere_1");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 1;
	MeshCheck.NumSphereCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_Simple2BoxCollisions, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.Simple2BoxesCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_Simple2BoxCollisions::RunTest(const FString& Parameters)
{
	// Test that 2 simple collisions can be added to the same mesh.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.CubeGroups.Add("collision_geo_simple_box_2");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 2;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}
IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_MainGeoBoxAndSphereCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.MainGeoSimpleBoxAndSphereCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_MainGeoBoxAndSphereCollision::RunTest(const FString& Parameters)
{
	// Test that 2 simple collisions can be added to the same mesh. with main_geo as the name.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("main_geo");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.SphereGroups.Add("collision_geo_simple_sphere_1");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 1;
	MeshCheck.NumSphereCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_LODs, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.LODs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_LODs::RunTest(const FString& Parameters)
{
	// test lods.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("lod1");
	Settings.SphereGroups.Add("lod2");
	Settings.CubeGroups.Add("lod3");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Sphere);
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_MainGeoCustomCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.ComplexCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_MainGeoCustomCollision::RunTest(const FString& Parameters)
{
	// Test a complex collision, where a second mesh is created.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.SphereGroups.Add("collision_geo");
	Settings.CubeGroups.Add("rendered_geo");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.ComplexCollisionType = EHoudiniMeshType::Sphere;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_Everything, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Unpacked.Everything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_Everything::RunTest(const FString& Parameters)
{
	// Lots of things tested together.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.SphereGroups.Add("collision_geo");
	Settings.CubeGroups.Add("rendered_geo");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.SphereGroups.Add("lod1");

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Sphere);
	MeshCheck.ComplexCollisionType = EHoudiniMeshType::Sphere;
	MeshCheck.NumBoxCollisions = 1;
	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedNone, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.None",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedNone::RunTest(const FString& Parameters)
{
	// No special groups should export a single mesh,
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedRenderedCollisionGeo, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.RenderedCollisionGeo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedRenderedCollisionGeo::RunTest(const FString& Parameters)
{
	// rendered_collision_geo should export a simple mesh
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedCollisionGeo, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.CollisionGeo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedCollisionGeo::RunTest(const FString& Parameters)
{
	// collision_geo should export a simple mesh where the component is invisible.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("collision_geo");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;
	MeshCheck.bComponentIsVisible = false;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedMainGeo, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.MainGeo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedMainGeo::RunTest(const FString& Parameters)
{
	// main_geo should export a simple mesh
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("main_geo");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 0;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleKDOP18Collision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.SimpleKDOP18Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleKDOP18Collision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop18");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleKDOP26Collision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.SimpleKDOP26Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleKDOP26Collision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop26");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleKDOP10xCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.SimpleKDOP10xCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleKDOP10xCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop10x");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleKDOP10yCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.SimpleKDOP10yCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleKDOP10yCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop10y");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleKDOP10zCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.SimpleKDOP10zCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleKDOP10zCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_kdop10z");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumConvexCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleBoxCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.SimpleBoxCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleBoxCollision::RunTest(const FString& Parameters)
{
	// Convex hull test
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("rendered_collision_geo_simple_box");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}
IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimpleBoxAndSphereCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.Simple1Box1SphereCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimpleBoxAndSphereCollision::RunTest(const FString& Parameters)
{
	// Test that 2 simple collisions can be added to the same mesh.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.SphereGroups.Add("collision_geo_simple_sphere_1");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 1;
	MeshCheck.NumSphereCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedSimple2BoxCollisions, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.Simple2BoxesCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedSimple2BoxCollisions::RunTest(const FString& Parameters)
{
	// Test that 2 simple collisions can be added to the same mesh.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.CubeGroups.Add("collision_geo_simple_box_2");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 2;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}
IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedMainGeoBoxAndSphereCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.MainGeoSimpleBoxAndSphereCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedMainGeoBoxAndSphereCollision::RunTest(const FString& Parameters)
{
	// Test that 2 simple collisions can be added to the same mesh. with main_geo as the name.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("main_geo");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.SphereGroups.Add("collision_geo_simple_sphere_1");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.NumBoxCollisions = 1;
	MeshCheck.NumSphereCollisions = 1;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedLODs, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.LODs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedLODs::RunTest(const FString& Parameters)
{
	// test lods.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.CubeGroups.Add("lod1");
	Settings.SphereGroups.Add("lod2");
	Settings.CubeGroups.Add("lod3");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Sphere);
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}

IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedMainGeoCustomCollision, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.ComplexCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedMainGeoCustomCollision::RunTest(const FString& Parameters)
{
	// Test a complex collision, where a second mesh is created.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.SphereGroups.Add("collision_geo");
	Settings.CubeGroups.Add("rendered_geo");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.ComplexCollisionType = EHoudiniMeshType::Sphere;

	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


IMPLEMENT_SIMPLE_CLASS_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMeshGroups_PackedEverything, FHoudiniMeshAutomationTest, "Houdini.UnitTests.MeshGroups.MeshDesc.Packed.Everything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMeshGroups_PackedEverything::RunTest(const FString& Parameters)
{
	// Lots of things tested together.
	TSharedPtr<FHoudiniTestContext> Context = FHoudiniMeshAutomationTest::LoadHDA(this);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	FHoudiniTestSettings Settings;
	Settings.SphereGroups.Add("collision_geo");
	Settings.CubeGroups.Add("rendered_geo");
	Settings.CubeGroups.Add("collision_geo_simple_box_1");
	Settings.SphereGroups.Add("lod1");
	Settings.bPack = true;

	FHoudiniMeshCheck MeshCheck;
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Cube);
	MeshCheck.LODMeshes.Add(EHoudiniMeshType::Sphere);
	MeshCheck.ComplexCollisionType = EHoudiniMeshType::Sphere;
	MeshCheck.NumBoxCollisions = 1;
	ExecuteMeshTest(Context, Settings, MeshCheck);

	return true;
}


#endif