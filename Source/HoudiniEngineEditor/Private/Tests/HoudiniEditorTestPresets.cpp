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

#include "HoudiniEditorTestPresets.h"

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniParameterFloat.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniPreset.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "Engine/SkinnedAssetCommon.h"
#endif

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPresetLoading, "Houdini.UnitTests.Presets.Loading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPresetLoading::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Test baking of skeletal meshes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, false));

	UHoudiniPreset* Preset = Cast<UHoudiniPreset>(StaticLoadObject(UHoudiniPreset::StaticClass(), nullptr, *FHoudiniEditorTestPresets::PresetAsset));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Preset, return true);

	volatile bool * bPresetInstantiated = new bool(false);

	Preset->PostInstantiationCallbacks.Add([bPresetInstantiated, Context](const UHoudiniPreset* Preset, UHoudiniAssetComponent* HAC)
	{
		*bPresetInstantiated = true;
		Context->HAC = HAC;
	});


	UWorld * World = Context->World;
	AActor* HoudiniAssetActor = FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(Preset->SourceHoudiniAsset, FTransform::Identity, World, nullptr, Preset);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, bPresetInstantiated, Preset]()
	{
		if (*bPresetInstantiated == false)
			return false;
		delete bPresetInstantiated;

		Preset->PostInstantiationCallbacks.Empty();

		HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->HAC, return true);

		TMap<FString, UHoudiniParameter*> Parameters;
		for(int Index = 0; Index < Context->HAC->GetNumParameters();Index++)
		{
			auto * Parameter = Context->HAC->GetParameterAt(Index);
			Parameters.Add(Parameter->GetParameterName(), Parameter);
		}

		UHoudiniParameter * * Parm = nullptr;
		UHoudiniParameterMultiParm * MultiParm;

		Parm = Parameters.Find(TEXT("multi_parm_folder1"));
		MultiParm = Cast<UHoudiniParameterMultiParm>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MultiParm, return true);
		HOUDINI_TEST_EQUAL(MultiParm->GetInstanceCount(), 1);

		Parm = Parameters.Find(TEXT("multi_parm_folder2"));
		MultiParm = Cast<UHoudiniParameterMultiParm>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MultiParm, return true);
		HOUDINI_TEST_EQUAL(MultiParm->GetInstanceCount(), 2);

		Parm = Parameters.Find(TEXT("multi_parm_nested1"));
		MultiParm = Cast<UHoudiniParameterMultiParm>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MultiParm, return true);
		HOUDINI_TEST_EQUAL(MultiParm->GetInstanceCount(), 0);

		Parm = Parameters.Find(TEXT("multi_parm_nested2"));
		MultiParm = Cast<UHoudiniParameterMultiParm>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MultiParm, return true);
		HOUDINI_TEST_EQUAL(MultiParm->GetInstanceCount(), 2);

		UHoudiniParameterInt * IntParm;

		Parm = Parameters.Find(TEXT("multi1_int1"));
		IntParm = Cast<UHoudiniParameterInt>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(IntParm, return true);
		HOUDINI_TEST_EQUAL(IntParm->GetValue(0).GetValue(), 2);

		UHoudiniParameterFloat* FloatParm;

		Parm = Parameters.Find(TEXT("multi1_float1"));
		FloatParm = Cast<UHoudiniParameterFloat>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(FloatParm, return true);
		HOUDINI_TEST_EQUAL(FloatParm->GetValue(0).GetValue(), 0.0f);

		UHoudiniParameterString* StringParam;

		Parm = Parameters.Find(TEXT("multi_parm_double_nested2_1"));
		StringParam = Cast<UHoudiniParameterString>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StringParam, return true);
		HOUDINI_TEST_EQUAL(StringParam->GetValueAt(0), TEXT("b"));

		Parm = Parameters.Find(TEXT("multi_parm_double_nested2_2"));
		StringParam = Cast<UHoudiniParameterString>(Parm ? *Parm : nullptr);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StringParam, return true);
		HOUDINI_TEST_EQUAL(StringParam->GetValueAt(0), TEXT("c"));


		return true;
	}));

	return true;
}




#endif

