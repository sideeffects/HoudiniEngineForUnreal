/*
* Copyright (c) <2025> Side Effects Software Inc.
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

#include "HoudiniEditorTestBaking.h"

#include "HoudiniCookable.h"
#include "HoudiniEditorTestPDG.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"

#include "Chaos/HeightField.h"
#include "Materials/Material.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorUnitTestUtils.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/AutomationTest.h"
#include <HoudiniEngine.h>

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPDGCommandletMesh, "Houdini.UnitTests.PDG.Commandlet.MeshExternalMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPDGCommandletMesh::RunTest(const FString& Parameters)
{
	FHoudiniEngine::Get().StartPDGCommandlet();

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestPDG::TestHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	TSharedPtr<int> WorkItemsComplete = MakeShared<int>(0);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "pig_head", false, 0);

			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, WorkItemsComplete]()
		{
			UHoudiniPDGAssetLink* AssetLink = Context->GetPDGAssetLink();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(AssetLink, return true);

			AssetLink->OnWorkResultObjectLoaded.AddLambda([this, WorkItemsComplete](UHoudiniPDGAssetLink* AL, UTOPNode* Node, int32 WorkItemArrayIndex, int32 WorkItemResultInfoIndex)
				{
					(*WorkItemsComplete)++;
				});



			bool bSuccess = Context->StartCookingSelectedTOPNetwork();
			HOUDINI_TEST_EQUAL(bSuccess, true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, WorkItemsComplete]()
		{
			UHoudiniPDGAssetLink* AssetLink = Context->GetPDGAssetLink();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(AssetLink, return true);

			UTOPNetwork* Network = AssetLink->GetTOPNetwork(0);
			HOUDINI_TEST_NOT_NULL(Network);

			if (*WorkItemsComplete != 2)
			{
				return false;
			}

			UTOPNode* Node = nullptr;
			for(UTOPNode* It : Network->AllTOPNodes)
			{
				if(It->NodeName == "HE_OUT_X")
				{
					Node = It;
					break;
				}
			}
			HOUDINI_TEST_NOT_NULL(Node);

			HOUDINI_TEST_EQUAL_ON_FAIL(Node->WorkResult.Num(), 2, return true);


			for(auto& Result : Node->WorkResult)
			{
				auto ResultOutputs = Result.ResultObjects[0].GetResultOutputs();
				HOUDINI_TEST_EQUAL_ON_FAIL(ResultOutputs.Num(), 1, return true);

				UHoudiniOutput* Output = ResultOutputs[0];

				TArray<FHoudiniOutputObject> OutputObjects;
				Output->GetOutputObjects().GenerateValueArray(OutputObjects);

				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);

				const FHoudiniOutputObject& OutputObject = OutputObjects[0];

				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents.Num(), 1, return true);
				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents[0]->IsA(UStaticMeshComponent::StaticClass()), 1, return true);

				auto * SMC = Cast<UStaticMeshComponent>(OutputObject.OutputComponents[0]);
				UStaticMesh * StaticMesh = SMC->GetStaticMesh();

				int32 MaterialCount = StaticMesh->GetStaticMaterials().Num();
				HOUDINI_TEST_EQUAL_ON_FAIL(MaterialCount, 1, return true);

				UMaterial * Material = StaticMesh->GetMaterial(0)->GetMaterial();
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Material, return true);

				FString MaterialName = Material->GetPathName();

				HOUDINI_TEST_EQUAL(MaterialName, TEXT("/Game/TestObjects/M_TestMaterial.M_TestMaterial"));


			}

			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FHoudiniEngine::Get().StopPDGCommandlet();
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPDGCommandletMeshInternalMaterials, "Houdini.UnitTests.PDG.Commandlet.MeshInternalsMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPDGCommandletMeshInternalMaterials::RunTest(const FString& Parameters)
{
	FHoudiniEngine::Get().StartPDGCommandlet();

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestPDG::TestHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	TSharedPtr<int> WorkItemsComplete = MakeShared<int>(0);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context, UHoudiniParameterToggle, "pig_head", true, 0);

			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, WorkItemsComplete]()
		{
			UHoudiniPDGAssetLink* AssetLink = Context->GetPDGAssetLink();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(AssetLink, return true);

			AssetLink->OnWorkResultObjectLoaded.AddLambda([this, WorkItemsComplete](UHoudiniPDGAssetLink* AL, UTOPNode* Node, int32 WorkItemArrayIndex, int32 WorkItemResultInfoIndex)
				{
					(*WorkItemsComplete)++;
				});



			bool bSuccess = Context->StartCookingSelectedTOPNetwork();
			HOUDINI_TEST_EQUAL(bSuccess, true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, WorkItemsComplete]()
		{
			UHoudiniPDGAssetLink* AssetLink = Context->GetPDGAssetLink();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(AssetLink, return true);

			UTOPNetwork* Network = AssetLink->GetTOPNetwork(0);
			HOUDINI_TEST_NOT_NULL(Network);

			if(*WorkItemsComplete != 2)
			{
				return false;
			}

			UTOPNode* Node = nullptr;
			for(UTOPNode* It : Network->AllTOPNodes)
			{
				if(It->NodeName == "HE_OUT_X")
				{
					Node = It;
					break;
				}
			}
			HOUDINI_TEST_NOT_NULL(Node);

			HOUDINI_TEST_EQUAL_ON_FAIL(Node->WorkResult.Num(), 2, return true);


			for(auto& Result : Node->WorkResult)
			{
				auto ResultOutputs = Result.ResultObjects[0].GetResultOutputs();
				HOUDINI_TEST_EQUAL_ON_FAIL(ResultOutputs.Num(), 1, return true);

				UHoudiniOutput* Output = ResultOutputs[0];

				TArray<FHoudiniOutputObject> OutputObjects;
				Output->GetOutputObjects().GenerateValueArray(OutputObjects);

				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);

				const FHoudiniOutputObject& OutputObject = OutputObjects[0];

				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents.Num(), 1, return true);
				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObject.OutputComponents[0]->IsA(UStaticMeshComponent::StaticClass()), 1, return true);

				auto* SMC = Cast<UStaticMeshComponent>(OutputObject.OutputComponents[0]);
				UStaticMesh* StaticMesh = SMC->GetStaticMesh();

				int32 MaterialCount = StaticMesh->GetStaticMaterials().Num();
				HOUDINI_TEST_EQUAL_ON_FAIL(MaterialCount, 3, return true);
			}

			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FHoudiniEngine::Get().StopPDGCommandlet();
			return true;
		}));

	return true;
}

#endif

