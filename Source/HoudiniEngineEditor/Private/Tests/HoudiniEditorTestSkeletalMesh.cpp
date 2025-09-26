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

#include "HoudiniEditorTestSkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "Animation/SkeletalMeshActor.h"
#include "Chaos/HeightField.h"
#include "Animation/Skeleton.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSources.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "Engine/SkinnedAssetCommon.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "PhysicsEngine/BodySetup.h"
	#include "PhysicsEngine/SkeletalBodySetup.h"
#endif
#include "PhysicsEngine/PhysicsAsset.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectra, "Houdini.UnitTests.SkeletalMesh.Electra",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSkeletalMeshElectra::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of skeletal meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestSkeletalMeshUtils::SkeletalMeshHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "unreal_skeleton", TEXT(""), 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<USkeletalMeshComponent>(Outputs);
		TArray<USkeleton*> Skeleton = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<USkeleton>(Outputs);
 		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponents.Num(), 1, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		FString BakeFolder = Context->GetBakeFolderOrDefault();

		// There should be one baked output object.
		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto & BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().ElementIt->Value.Value;
#else
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
#endif

		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedSkeleton.IsEmpty(), false, return true);

		// For now, check we have the correct number of bones. Can add more complicated checks in the future if needed, such as checking
		// parents, etc. Probably should not check the bone order though.
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedSkeleton), BakeFolder);

		FString SkeletonName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
		HOUDINI_TEST_EQUAL(SkeletonName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

		USkeleton * Skeleton = Cast<USkeleton>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedSkeleton));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Skeleton, return false);
		auto & ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		HOUDINI_TEST_EQUAL_ON_FAIL(ReferenceSkeleton.GetRawBoneNum(), 53, return true);

		// Check the skeletal mesh 
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedObject), BakeFolder);
		FString SkeletalMeshName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
		HOUDINI_TEST_EQUAL(SkeletalMeshName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

		USkeletalMesh * SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedObject));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMesh, return false);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMesh->GetSkeleton(), Skeleton, return true);
		auto & Materials = SkeletalMesh->GetMaterials();
		HOUDINI_TEST_EQUAL_ON_FAIL(Materials.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(FPaths::GetPath(Materials[0].MaterialInterface->GetPackage()->GetPathName()), BakeFolder);

		// Check the skeletal mesh component
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedComponent));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMeshComponent, return false);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalMesh, return true);
		FString ActorName = SkeletalMeshComponent->GetOwner()->GetActorLabel();
		HOUDINI_TEST_EQUAL(ActorName.StartsWith(TEXT("TestSkeletonBakeActor")), true);


		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectraDefaultPhysicsAsset, "Houdini.UnitTests.SkeletalMesh.ElectraDefaultPhysicsAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSkeletalMeshElectraDefaultPhysicsAsset::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of skeletal meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestSkeletalMeshUtils::SkeletalMeshHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "unreal_skeleton", TEXT(""), 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "create_default_physics_asset", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<USkeletalMeshComponent>(Outputs);
		TArray<USkeleton*> Skeleton = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<USkeleton>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponents.Num(), 1, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		FString BakeFolder = Context->GetBakeFolderOrDefault();

		// There should be one baked output object.
		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto& BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().ElementIt->Value.Value;
#else
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
#endif
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedSkeleton.IsEmpty(), false, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedPhysicsAsset.IsEmpty(), false, return true);

		// For now, check we have the correct number of bones. Can add more complicated checks in the future if needed, such as checking
		// parents, etc. Probably should not check the bone order though.
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedSkeleton), BakeFolder);

		FString SkeletonName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
		HOUDINI_TEST_EQUAL(SkeletonName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

		USkeleton* Skeleton = Cast<USkeleton>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedSkeleton));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Skeleton, return false);
		auto& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		HOUDINI_TEST_EQUAL_ON_FAIL(ReferenceSkeleton.GetRawBoneNum(), 53, return true);

		// Check the skeletal mesh 
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedObject), BakeFolder);
		FString SkeletalMeshName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
		HOUDINI_TEST_EQUAL(SkeletalMeshName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedObject));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMesh, return false);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMesh->GetSkeleton(), Skeleton, return true);
		auto& Materials = SkeletalMesh->GetMaterials();
		HOUDINI_TEST_EQUAL_ON_FAIL(Materials.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(FPaths::GetPath(Materials[0].MaterialInterface->GetPackage()->GetPathName()), BakeFolder);

		// Check the skeletal mesh component
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedComponent));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMeshComponent, return false);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalMesh, return true);
		FString ActorName = SkeletalMeshComponent->GetOwner()->GetActorLabel();
		HOUDINI_TEST_EQUAL(ActorName.StartsWith(TEXT("TestSkeletonBakeActor")), true);

		// Check the Physics Asset has some body setups.
		UPhysicsAsset * PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PhysicsAsset, return false);
		HOUDINI_TEST_NOT_EQUAL(PhysicsAsset->SkeletalBodySetups.Num(), 0);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectraCustomPhysicsAsset, "Houdini.UnitTests.SkeletalMesh.ElectraCustomPhysicsAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSkeletalMeshElectraCustomPhysicsAsset::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of skeletal meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestSkeletalMeshUtils::SkeletalMeshHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context, UHoudiniParameterString, "unreal_skeleton", TEXT(""), 0);
			SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "enable_custom_physics_asset", true, 0);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);

			// We should have two outputs, two meshes
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
			TArray<USkeletalMeshComponent*> SkeletalMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<USkeletalMeshComponent>(Outputs);
			TArray<USkeleton*> Skeleton = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<USkeleton>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponents.Num(), 1, return true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FHoudiniBakeSettings BakeSettings;
			Context->Bake(BakeSettings);

			FString BakeFolder = Context->GetBakeFolderOrDefault();

			// There should be one baked output object.
			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
			auto& BakedOutput = BakedOutputs[0];
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			auto& BakedObject = BakedOutput.BakedOutputObjects.begin().ElementIt->Value.Value;
#else
			auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
#endif

			HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedSkeleton.IsEmpty(), false, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedPhysicsAsset.IsEmpty(), false, return true);

			// For now, check we have the correct number of bones. Can add more complicated checks in the future if needed, such as checking
			// parents, etc. Probably should not check the bone order though.
			HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedSkeleton), BakeFolder);

			FString SkeletonName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
			HOUDINI_TEST_EQUAL(SkeletonName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

			USkeleton* Skeleton = Cast<USkeleton>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedSkeleton));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Skeleton, return false);
			auto& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
			HOUDINI_TEST_EQUAL_ON_FAIL(ReferenceSkeleton.GetRawBoneNum(), 53, return true);

			// Check the skeletal mesh 
			HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedObject), BakeFolder);
			FString SkeletalMeshName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
			HOUDINI_TEST_EQUAL(SkeletalMeshName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedObject));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMesh, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMesh->GetSkeleton(), Skeleton, return true);
			auto& Materials = SkeletalMesh->GetMaterials();
			HOUDINI_TEST_EQUAL_ON_FAIL(Materials.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(FPaths::GetPath(Materials[0].MaterialInterface->GetPackage()->GetPathName()), BakeFolder);

			// Check the skeletal mesh component
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedComponent));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMeshComponent, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalMesh, return true);
			FString ActorName = SkeletalMeshComponent->GetOwner()->GetActorLabel();
			HOUDINI_TEST_EQUAL(ActorName.StartsWith(TEXT("TestSkeletonBakeActor")), true);

			// Check the Physics Asset has some body setups.
			UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(PhysicsAsset, return true);

			HOUDINI_TEST_EQUAL(PhysicsAsset->SkeletalBodySetups.Num(), 6);

			int32 BodyIndex = PhysicsAsset->FindBodyIndex(TEXT("head"));
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(BodyIndex, (int32)INDEX_NONE, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			UBodySetup* BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyIndex].Get());
#else
			UBodySetup * BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
#endif
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.BoxElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphereElems.Num(), 1);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphylElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.ConvexElems.Num(), 0);

			BodyIndex = PhysicsAsset->FindBodyIndex(TEXT("calf_l"));
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(BodyIndex, (int32)INDEX_NONE, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyIndex].Get());
#else
			BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
#endif
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.BoxElems.Num(), 1);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphereElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphylElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.ConvexElems.Num(), 0);

			BodyIndex = PhysicsAsset->FindBodyIndex(TEXT("calf_r"));
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(BodyIndex, (int32)INDEX_NONE, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyIndex].Get());
#else
			BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
#endif
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.BoxElems.Num(), 1);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphereElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphylElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.ConvexElems.Num(), 0);

			BodyIndex = PhysicsAsset->FindBodyIndex(TEXT("thigh_l"));
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(BodyIndex, (int32)INDEX_NONE, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyIndex].Get());
#else
			BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
#endif
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.BoxElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphereElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphylElems.Num(), 1);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.ConvexElems.Num(), 0);

			BodyIndex = PhysicsAsset->FindBodyIndex(TEXT("thigh_r"));
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(BodyIndex, (int32)INDEX_NONE, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyIndex].Get());
#else
			BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
#endif
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.BoxElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphereElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphylElems.Num(), 1);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.ConvexElems.Num(), 0);

			BodyIndex = PhysicsAsset->FindBodyIndex(TEXT("spine_03"));
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(BodyIndex, (int32)INDEX_NONE, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			BodySetup = Cast<UBodySetup>(PhysicsAsset->SkeletalBodySetups[BodyIndex].Get());
#else
			BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
#endif
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.BoxElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphereElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.SphylElems.Num(), 0);
			HOUDINI_TEST_EQUAL(BodySetup->AggGeom.ConvexElems.Num(), 1);

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectraExistingPhysicsAsset, "Houdini.UnitTests.SkeletalMesh.ElectraExistingPhysicsAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSkeletalMeshElectraExistingPhysicsAsset::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of skeletal meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestSkeletalMeshUtils::SkeletalMeshHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "unreal_skeleton", TEXT(""), 0);
		SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "use_test_physics_asset", true, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<USkeletalMeshComponent>(Outputs);
		TArray<USkeleton*> Skeleton = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<USkeleton>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponents.Num(), 1, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		FString BakeFolder = Context->GetBakeFolderOrDefault();

		// There should be one baked output object.
		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto& BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
		
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().ElementIt->Value.Value;
#else
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
#endif

		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedSkeleton.IsEmpty(), false, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedPhysicsAsset.IsEmpty(), true, return true);

		// For now, check we have the correct number of bones. Can add more complicated checks in the future if needed, such as checking
		// parents, etc. Probably should not check the bone order though.
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedSkeleton), BakeFolder);

		FString SkeletonName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
		HOUDINI_TEST_EQUAL(SkeletonName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

		USkeleton* Skeleton = Cast<USkeleton>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedSkeleton));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Skeleton, return false);
		auto& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		HOUDINI_TEST_EQUAL_ON_FAIL(ReferenceSkeleton.GetRawBoneNum(), 53, return true);

		// Check the skeletal mesh 
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedObject), BakeFolder);
		FString SkeletalMeshName = FPaths::GetBaseFilename(*BakedObject.BakedSkeleton);
		HOUDINI_TEST_EQUAL(SkeletalMeshName.StartsWith(TEXT("TestSkeletalMeshOutputName")), true);

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedObject));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMesh, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMesh->GetSkeleton(), Skeleton, return true);
		auto& Materials = SkeletalMesh->GetMaterials();
		HOUDINI_TEST_EQUAL_ON_FAIL(Materials.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(FPaths::GetPath(Materials[0].MaterialInterface->GetPackage()->GetPathName()), BakeFolder);

		// Check the skeletal mesh component
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedComponent));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMeshComponent, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalMesh, return true);
		FString ActorName = SkeletalMeshComponent->GetOwner()->GetActorLabel();
		HOUDINI_TEST_EQUAL(ActorName.StartsWith(TEXT("TestSkeletonBakeActor")), true);

		// Check the Physics Asset has some body setups.
		UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PhysicsAsset, return true);

		FString PhysicsPathName = PhysicsAsset->GetPathName();
		HOUDINI_TEST_EQUAL(PhysicsPathName, TEXT("/Game/TestObjects/SkeletalMeshes/Test_Ref_Physics_Asset.Test_Ref_Physics_Asset"));

		return true;
	}));

	return true;
}
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectraExistingSkeleton, "Houdini.UnitTests.SkeletalMesh.ElectraExistingSkeleton", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSkeletalMeshElectraExistingSkeleton::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of skeletal meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestSkeletalMeshUtils::SkeletalMeshHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra, but use an existing Unreal Skeleton
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	FString SkeletonAssetName = TEXT("/Script/Engine.Skeleton'/Game/TestObjects/SkeletalMeshes/Test_Ref_Skeleton.Test_Ref_Skeleton'");

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SkeletonAssetName]()
	{
		SET_HDA_PARAMETER(Context, UHoudiniParameterString, "unreal_skeleton", SkeletonAssetName, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<USkeletalMeshComponent>(Outputs);
		TArray<USkeleton*> Skeleton = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<USkeleton>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponents.Num(), 1, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SkeletonAssetName]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		FString BakeFolder = Context->GetBakeFolderOrDefault();

		// There should be one baked output object.
		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto& BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().ElementIt->Value.Value;
#else
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
#endif

		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedSkeleton.IsEmpty(), true, return true);

		// Check the skeletal mesh 
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedObject), BakeFolder);

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedObject));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMesh, return true);
		auto& Materials = SkeletalMesh->GetMaterials();
		HOUDINI_TEST_EQUAL_ON_FAIL(Materials.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(FPaths::GetPath(Materials[0].MaterialInterface->GetPackage()->GetPathName()), BakeFolder);

		// Check skeleton
		USkeleton * Skeleton = SkeletalMesh->GetSkeleton();
        HOUDINI_TEST_NOT_NULL_ON_FAIL(Skeleton, return true);
		HOUDINI_TEST_EQUAL(Skeleton->GetName(), TEXT("Test_Ref_Skeleton"));

		// Check the skeletal mesh component
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedComponent));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMeshComponent, return false);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponent->GetSkeletalMeshAsset(), SkeletalMesh, return true);
		FString ActorName = SkeletalMeshComponent->GetOwner()->GetActorLabel();
		HOUDINI_TEST_EQUAL(ActorName.StartsWith(TEXT("TestSkeletonBakeActor")), true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshRoundtrip, "Houdini.UnitTests.SkeletalMesh.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestSkeletalMeshRoundtrip::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestSkeletalMeshUtils::RoundtripHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	USkeletalMesh* OrigSkeletalMesh = LoadObject<USkeletalMesh>(Context->World, TEXT("/Script/Engine.SkeletalMesh'/Game/TestObjects/SkeletalMeshes/Test_Roundtrip_SKM.Test_Roundtrip_SKM'"));

	FActorSpawnParameters SpawnParams;
	ASkeletalMeshActor * OrigSkeletalMeshActor = Context->World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), SpawnParams);
	OrigSkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(OrigSkeletalMesh);


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, OrigSkeletalMeshActor]()
	{
		UHoudiniInput* CurrentInput = Context->GetInputAt(0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, OrigSkeletalMeshActor]()
	{
		TArray<AActor*> Actors;
		Actors.Add(OrigSkeletalMeshActor);
		UHoudiniInput* CurrentInput = Context->GetInputAt(0);
		bool bChanged = true;
		CurrentInput->SetInputType(EHoudiniInputType::World, bChanged);

		CurrentInput->UpdateWorldSelection(Actors);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, OrigSkeletalMesh]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<USkeletalMeshComponent>(Outputs);
		TArray<USkeleton*> Skeleton = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<USkeleton>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SkeletalMeshComponents.Num(), 1, return true);

		USkeletalMesh * CookedSkeletalMesh = SkeletalMeshComponents[0]->GetSkeletalMeshAsset();

		TArray<FName> CookedBoneNames =  CookedSkeletalMesh->GetRefSkeleton().GetRawRefBoneNames();
		TArray<FName> OrigBoneNames = OrigSkeletalMesh->GetRefSkeleton().GetRawRefBoneNames();

		TArray<FTransform> CookedRefPose = CookedSkeletalMesh->GetRefSkeleton().GetRefBonePose();
		TArray<FTransform> OrigRefPose = OrigSkeletalMesh->GetRefSkeleton().GetRefBonePose();

		HOUDINI_TEST_EQUAL_ON_FAIL(CookedBoneNames.Num(), CookedRefPose.Num(), return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(OrigBoneNames.Num(), OrigRefPose.Num(), return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(CookedBoneNames.Num(), OrigBoneNames.Num(), return true);

		TMap<FString, FTransform> OrigBoneMap;
		TMap<FString, FTransform> CoookedBoneMap;

		for(int Index = 0; Index < OrigBoneNames.Num(); Index++)
		{
			OrigBoneMap.Add(OrigBoneNames[Index].GetPlainNameString(), OrigRefPose[Index]);
			CoookedBoneMap.Add(CookedBoneNames[Index].GetPlainNameString(), CookedRefPose[Index]);
		}

		for(const FName & BoneName : OrigBoneNames)
		{
			FString Bone = BoneName.ToString();
			FTransform * OrigTransform = OrigBoneMap.Find(Bone);
			FTransform * CookedTransform = CoookedBoneMap.Find(Bone);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(OrigTransform, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(CookedTransform, return true);

			if (!OrigTransform->GetRotation().Equals(CookedTransform->GetRotation(), 0.01f))
			{
				FString Error = FString::Printf(TEXT("Bone %s Rotation Differs: Original %s Cooked %s"), *Bone,
					*OrigTransform->GetRotation().ToString(),
					*CookedTransform->GetRotation().ToString());

				AddError(Error);
			}

			if (!OrigTransform->GetScale3D().Equals(CookedTransform->GetScale3D(), 0.01f))
			{
				FString Error = FString::Printf(TEXT("Bone %s Scale Differs: Original %s Cooked %s"), *Bone,
					*OrigTransform->GetScale3D().ToString(),
					*CookedTransform->GetScale3D().ToString());

				AddError(Error);
			}

		}
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		Context->Bake(BakeSettings);

		return true;
	}));

	return true;
}

#endif

