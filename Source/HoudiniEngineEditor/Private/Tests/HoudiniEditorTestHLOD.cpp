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

#include "HoudiniEditorTestDataLayers.h"

#include "FileHelpers.h"
#include "HoudiniAsset.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniPDGManager.h"
#include "HoudiniParameterString.h"
#include "HoudiniPDGAssetLink.h"
#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "HoudiniAssetActorFactory.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HoudiniParameterToggle.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "HoudiniEngineBakeUtils.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeHLOD, "Houdini.UnitTests.HLOD.Landscapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeHLOD::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString HDAName = TEXT("/Game/TestHDAs/HLOD/CreateHeightFieldHLOD");

	// Now create the test context. This should be the last step before the tests start as it starts the timeout timer. Note
	// the context live in a SharedPtr<> because each part of the test, in AddCommand(), are executed asyncronously
	// after the test returns.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, HDAName, FTransform::Identity, true));
	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	// HDA Path and kick Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	// Bake and check results.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto& BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 1, return true);
		auto& BakedObject = BakedOutput.BakedOutputObjects.begin().Value();

		ALandscape* Landscape = Cast<ALandscape>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject.Landscape));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Landscape, return true);

		UHLODLayer * HODLayer = Landscape->GetHLODLayer();

		HOUDINI_TEST_NOT_NULL(HODLayer);
		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesHLOD, "Houdini.UnitTests.HLOD.Instances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestInstancesHLOD::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString HDAName = TEXT("/Game/TestHDAs/HLOD/CreateHLODInstances");

	// Now create the test context. This should be the last step before the tests start as it starts the timeout timer. Note
	// the context live in a SharedPtr<> because each part of the test, in AddCommand(), are executed asyncronously
	// after the test returns.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, HDAName, FTransform::Identity, true));
	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	// HDA Path and kick Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	// Bake and check results.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
		auto& BakedOutput = BakedOutputs[0];
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 2, return true);

		auto ObjIt = BakedOutput.BakedOutputObjects.begin();

		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 2, return true);
		auto& BakedObject0 = ObjIt.Value();

		// Check first output instancer has DataLayer1.
		AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject0.Actor));
		UHLODLayer* HODLayer0 = Actor->GetHLODLayer();
		HOUDINI_TEST_NOT_NULL(HODLayer0);
		HOUDINI_TEST_EQUAL(HODLayer0->GetName(), TEXT("TestHLODLayer1"));
		//HOUDINI_TEST_EQUAL();

		// Check second output instancer has HLODLayer.
		++ObjIt;
		auto& BakedObject1 = ObjIt.Value();
		Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject1.Actor));
		UHLODLayer* HODLayer1 = Actor->GetHLODLayer();
		HOUDINI_TEST_NOT_NULL(HODLayer1);
		HOUDINI_TEST_EQUAL(HODLayer1->GetName(), TEXT("TestHLODLayer"));

		return true;
	}));

	return true;
}

#endif

