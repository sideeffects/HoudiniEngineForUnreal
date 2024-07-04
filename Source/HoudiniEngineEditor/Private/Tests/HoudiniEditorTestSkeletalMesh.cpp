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
#include "Chaos/HeightField.h"
#include "Animation/Skeleton.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "Engine/SkinnedAssetCommon.h"
#endif

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectra, "Houdini.UnitTests.SkeletalMesh.Electra", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

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

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "unreal_skeleton", TEXT(""), 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

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

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		FString BakeFolder = Context->HAC->GetBakeFolderOrDefault();

		// There should be one baked output object.
		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto & BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
		auto & BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
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



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestSkeletalMeshElectraExistingSkeleton, "Houdini.UnitTests.SkeletalMesh.ElectraExistingSkeleton", 
		EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

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

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Cook and Bake Electra, but use an existing Unreal Skeleton
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	FString SkeletonAssetName = TEXT("/Script/Engine.Skeleton'/Game/TestObjects/SkeletalMeshes/Test_Ref_Skeleton.Test_Ref_Skeleton'");

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SkeletonAssetName]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "unreal_skeleton", SkeletonAssetName, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

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

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		FString BakeFolder = Context->HAC->GetBakeFolderOrDefault();

		// There should be one baked output object.
		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto& BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedObject.BakedSkeleton.IsEmpty(), true, return true);

		// Check the skeletal mesh 
		HOUDINI_TEST_EQUAL(FPaths::GetPath(*BakedObject.BakedObject), BakeFolder);

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.BakedObject));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SkeletalMesh, return false);
		auto& Materials = SkeletalMesh->GetMaterials();
		HOUDINI_TEST_EQUAL_ON_FAIL(Materials.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(FPaths::GetPath(Materials[0].MaterialInterface->GetPackage()->GetPathName()), BakeFolder);

		// Check skeleton
		USkeleton * Skeleton = SkeletalMesh->GetSkeleton();
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

#endif

