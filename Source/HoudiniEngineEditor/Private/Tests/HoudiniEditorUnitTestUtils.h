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

#pragma once

#include "CoreMinimal.h"
#include "HoudiniEditorAssetStateSubsystem.h"
#include "HoudiniEditorTestUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniOutput.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

//#include "HoudiniEditorLatentUtils.generated.h"
class UHoudiniAssetComponent;

// Test Macros.
//
//  Use these macros to automatically print out useful info about the test automatically.
//  The tests that end with "ON_FAIL" can be used to specify a statement to be executed on failure,
//  for example a continue, break or return statement.

#define HOUDINI_TEST_EQUAL(A,B)	TestEqual(#A, A, B)
#define HOUDINI_TEST_EQUAL_ON_FAIL(A,B,_FAIL)	if (!TestEqual(#A, A, B)) _FAIL;
#define HOUDINI_TEST_NOT_EQUAL(A,B)	TestNotEqual(#A, A, B)
#define HOUDINI_TEST_NOT_EQUAL_ON_FAIL(A,B,_FAIL)	if (!TestNotEqual(#A, A, B)) _FAIL;
#define HOUDINI_TEST_NOT_NULL(A)	TestNotNull(#A, A)
#define HOUDINI_TEST_NULL(A)	TestNull(#A, A)

// Utils functions

struct FHoudiniEditorUnitTestUtils
{
	static UHoudiniAssetComponent* LoadHDAIntoNewMap(const FString& PackageName, const FTransform& Transform, bool bOpenWorld);

	static FString GetAbsolutePathOfProjectFile(const FString & Object);

	// Helper function to returns components from an output.
	template<typename COMPONENT_TYPE>
	static TArray<COMPONENT_TYPE*>  GetOutputsWithComponent(const TArray<UHoudiniOutput*>& Outputs)
	{
		TArray<COMPONENT_TYPE*> Results;

		for (UHoudiniOutput* Output : Outputs)
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				if (OutputObject.Value.OutputComponents.Num() > 0)
				{
					if (OutputObject.Value.OutputComponents[0]->GetClass() ==  COMPONENT_TYPE::StaticClass())
					{
						COMPONENT_TYPE* Out = Cast<COMPONENT_TYPE>(OutputObject.Value.OutputComponents[0]);
						Results.Add(Out);
					}
				}
			}
		}
		return  Results;
	}

	// Helper function to returns components from a baked output.
	template<typename COMPONENT_TYPE>
	static TArray<COMPONENT_TYPE*>  GetOutputsWithComponent(const TArray<FHoudiniEngineBakedActor>& Outputs)
	{
		TArray<COMPONENT_TYPE*> Results;

		for (const FHoudiniEngineBakedActor & Output : Outputs)
		{
			if (IsValid(Output.BakedComponent) && Output.BakedComponent->GetClass() == COMPONENT_TYPE::StaticClass())
			{
				COMPONENT_TYPE* Out = Cast<COMPONENT_TYPE>(Output.BakedComponent);
				Results.Add(Out);
			}
		}
		return  Results;
	}

	// Helper function to returns objects from an output.
	template<typename ACTOR_TYPE>
	static TArray<FHoudiniOutputObject*> GetOutputsWithActor(const TArray<UHoudiniOutput*>& Outputs)
	{
		TArray<FHoudiniOutputObject*> Results;

		for (UHoudiniOutput* Output : Outputs)
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				if (OutputObject.Value.OutputActors.Num() > 0)
				{
					if (OutputObject.Value.OutputActors[0]->IsA(ACTOR_TYPE::StaticClass()))
					{
						FHoudiniOutputObject* Out = &OutputObject.Value;
						Results.Add(Out);
					}
				}
			}
		}
		return  Results;
	}

	// Helper function to return actors from an output.
	template<typename OBJECT_TYPE>
	static TArray<OBJECT_TYPE*> GetOutputsWithObject(const TArray<UHoudiniOutput*>& Outputs)
	{
		TArray<OBJECT_TYPE*> Results;

		for (UHoudiniOutput* Output : Outputs)
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				if (IsValid(OutputObject.Value.OutputObject))
				{
					if (OutputObject.Value.OutputObject->IsA<OBJECT_TYPE>())
					{
						OBJECT_TYPE* Out = Cast<OBJECT_TYPE>(OutputObject.Value.OutputObject);
						Results.Add(Out);
					}
				}
			}
		}
		return  Results;
	}

	static AActor* GetActorWithName(UWorld* World, FString& Name);
	static bool IsHDAIdle(UHoudiniAssetComponent* HAC);


};

// Helper macro to set parm, ensures the parameter is valid.
#define SET_HDA_PARAMETER(_HAC, _PARAMETER_TYPE, _PARAMATER_NAME, _PARAMETER_VALUE, _PARAMETER_INDEX)\
	{\
		_PARAMETER_TYPE* __Parameter = Cast<_PARAMETER_TYPE>(_HAC->FindParameterByName(_PARAMATER_NAME));\
		if (!TestNotNull(#_PARAMATER_NAME, __Parameter))\
			return true;\
		__Parameter->SetValueAt(_PARAMETER_VALUE, _PARAMETER_INDEX);\
	}

struct FHoudiniTestContext
{
	// Create and pass one of these structures between different latent parts of a test  (ie. those added
	// with AddCommand()). It keeps track of the test and stores timer info to handle timeouts (to avoid
	// hanging build PCs).
	//
	// The "Data" map can be used to pass data between tests.
	//

	FHoudiniTestContext(FAutomationTestBase* CurrentTest)
	{
		TimeStarted = FPlatformTime::Seconds();
		Test = CurrentTest;
	}

	// Starts cooking the HDA asynchrously.
	void StartCookingHDA();

	// Starts cooking the Selected top network in the HDA asynchronously.
	void StartCookingSelectedTOPNetwork();

	// Bakes the top network. Synchronous, returns the baked actors.
	TArray<FHoudiniEngineBakedActor> BakeSelectedTopNetwork();

	double MaxTime = 15.0f;						// Max time (seconds) this test can run.
	double TimeStarted = 0.0f;					// Time this test started. Used to test for timeout.

	FAutomationTestBase* Test = nullptr;		// Unit test underway
	UHoudiniAssetComponent* HAC = nullptr;		// HAC being tested
	TMap<FString, FString> Data;				// Use this to pass data between different tests.
	bool bCookInProgress = false;
	bool bPDGCookInProgress = false;
	bool bPDGPostCookDelegateCalled = false;
};

class FHoudiniLatentTestCommand : public FFunctionLatentCommand
{
public:
	// Each part of an HDA that requires a Cook should be its own FFunctionLatentCommand. Before the Update()
	// function is called this class ensures the previous cook completed.

	FHoudiniLatentTestCommand(TSharedPtr<FHoudiniTestContext> InContext, TFunction<bool()> InLatentPredicate)
		: FFunctionLatentCommand(InLatentPredicate), Context(InContext) {}

	TSharedPtr<FHoudiniTestContext> Context;

	// Like its base class, return true when the command is complete, false when it should be called again.
	virtual bool Update() override;
};


#if WITH_DEV_AUTOMATION_TESTS

#endif