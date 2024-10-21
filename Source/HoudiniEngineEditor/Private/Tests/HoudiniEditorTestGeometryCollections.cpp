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

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#else
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"	
#endif
#include "HoudiniEditorTestGeometryCollections.h"
#include "HoudiniParameterToggle.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestGeometryCollections, "Houdini.UnitTests.GeometryCollection.GeometryCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestGeometryCollections::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Tests baking of instances meshes
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestGeometryCollection::GeometryCollectionHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 3, return true);
		TArray<UGeometryCollection*> OutputObjects = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UGeometryCollection>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Bake the geometry actor
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->HAC->GetBakedOutputs();
		// There should be two outputs as we have two meshes.
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 3, return true);

		// Go through each output and check we have two actors with one mesh component each.
		TArray<AGeometryCollectionActor*> GeometryActors;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;

				AGeometryCollectionActor* Actor = Cast<AGeometryCollectionActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
				if (IsValid(Actor))
					GeometryActors.Add(Actor);
			}
		}

		HOUDINI_TEST_EQUAL_ON_FAIL(GeometryActors.Num(), 1, return true);

		return true;
	}));

	return true;
}

#endif

