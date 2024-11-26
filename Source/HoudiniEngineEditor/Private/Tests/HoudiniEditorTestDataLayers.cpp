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
#include "LandscapeStreamingProxy.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestsPDGDataLayers, "Houdini.UnitTests.DataLayers.PDGTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestsPDGDataLayers::RunTest(const FString& Parameters)
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

	FString HDAName = TEXT("/Game/TestHDAs/PDG/PDGHarness");

	// Now create the test context. This should be the last step before the tests start as it starts the timeout timer. Note
	// the context live in a SharedPtr<> because each part of the test, in AddCommand(), are executed asyncronously
	// after the test returns.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, HDAName, FTransform::Identity, true));
	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	// HDA Path and kick Cook.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FString HDAPath = FHoudiniEditorUnitTestUtils::GetAbsolutePathOfProjectFile(TEXT("TestHDAS/DataLayers/CreateMeshWithDataLayer.hda"));
		HOUDINI_LOG_MESSAGE(TEXT("Resolved HDA to %s"), *HDAPath);

		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterString, "hda_path", HDAPath, 0);
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
		UHoudiniPDGAssetLink * AssetLink = Context->HAC->GetPDGAssetLink();
		UTOPNetwork * Network = AssetLink->GetTOPNetwork(0);
		HOUDINI_TEST_NOT_NULL(Network);

		UTOPNode * Node = nullptr;
		for(UTOPNode * It : Network->AllTOPNodes)
		{
			if (It->NodeName == "HE_OUT_X")
			{
				Node = It;
				break;
			}
		}
		HOUDINI_TEST_NOT_NULL(Node);

		auto & Results = Node->WorkResult;


		// We should have one work result. Check this before baking.
		HOUDINI_TEST_EQUAL_ON_FAIL(Results.Num(), 1, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(Results[0].ResultObjects.Num(), 1, return true);

		// Bake PDG Output
		TArray<FHoudiniEngineBakedActor> BakedActors = Context->BakeSelectedTopNetwork();
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedActors.Num(), 1, return true);

		TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(BakedActors);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 1, return true);

		AActor * Actor = StaticMeshOutputs[0]->GetOwner();

		TArray<FHoudiniUnrealDataLayerInfo> DataLayers = FHoudiniDataLayerUtils::GetDataLayerInfoForActor(Actor);

		FString ExpectedName(TEXT("MyDataLayer"));

		HOUDINI_TEST_EQUAL_ON_FAIL(DataLayers.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(DataLayers[0].Name.Left(ExpectedName.Len()), ExpectedName);
		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeDataLayers, "Houdini.UnitTests.DataLayers.Landscapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeDataLayers::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString HDAName = TEXT("/Game/TestHDAs/DataLayers/CreateLandscapeWithDataLayers");

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

		TArray<FHoudiniUnrealDataLayerInfo> DataLayers = FHoudiniDataLayerUtils::GetDataLayerInfoForActor(Landscape);

		FString ExpectedName(TEXT("TestDataLayer"));
		HOUDINI_TEST_EQUAL_ON_FAIL(DataLayers.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(DataLayers[0].Name.Left(ExpectedName.Len()), ExpectedName);

		ULandscapeInfo* Info = Landscape->GetLandscapeInfo();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>>& Proxies = Info->StreamingProxies;
		for(auto ProxyPtr : Proxies)
		{
			ALandscapeStreamingProxy* Proxy = ProxyPtr.Get();
#else
		TArray<ALandscapeStreamingProxy*>& Proxies = Info->Proxies;
		for(ALandscapeStreamingProxy* Proxy : Proxies)
		{
#endif
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Proxy, return true);

			TArray<FHoudiniUnrealDataLayerInfo> ProxyDataLayers = FHoudiniDataLayerUtils::GetDataLayerInfoForActor(Cast<AActor>(Proxy));

			FString ProxyName(TEXT("TestDataLayer"));
			HOUDINI_TEST_EQUAL_ON_FAIL(ProxyDataLayers.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(ProxyDataLayers[0].Name.Left(ProxyName.Len()), ProxyName);

		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInstancesDataLayers, "Houdini.UnitTests.DataLayers.Instances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
	
	bool FHoudiniEditorTestInstancesDataLayers::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString HDAName = TEXT("/Game/TestHDAs/DataLayers/CreateInstancesDataLayers");

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

		auto ObjIt = BakedOutput.BakedOutputObjects.begin();

		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutput.BakedOutputObjects.Num(), 2, return true);
		auto& BakedObject0 = ObjIt.Value();

		// Check first output instancer has DataLayer1.
		AActor * Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject0.Actor));
		TArray<FHoudiniUnrealDataLayerInfo> DataLayers = FHoudiniDataLayerUtils::GetDataLayerInfoForActor(Actor);
		HOUDINI_TEST_EQUAL_ON_FAIL(DataLayers.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(DataLayers[0].Name, TEXT("DataLayer1"));

		// Check second output instanxer has DataLayer2.
		++ObjIt;
		auto& BakedObject1 = ObjIt.Value();
		Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakedObject1.Actor));
		DataLayers = FHoudiniDataLayerUtils::GetDataLayerInfoForActor(Actor);
		HOUDINI_TEST_EQUAL_ON_FAIL(DataLayers.Num(), 1, return true);
		HOUDINI_TEST_EQUAL(DataLayers[0].Name, TEXT("DataLayer2"));

		return true;
	}));

	return true;
}

#endif

