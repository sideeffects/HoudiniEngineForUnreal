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
#include "HoudiniStaticMesh.h"
#include "HoudiniStaticMeshComponent.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#else
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"	
#endif

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

#define HOUDINI_TEST_EQUAL(A,...)	TestEqual(#A, A, __VA_ARGS__)
#define HOUDINI_TEST_EQUAL_ON_FAIL(A,B,_FAIL)	if (!TestEqual(#A, A, B)) _FAIL;
#define HOUDINI_TEST_EQUALISH_ON_FAIL(A,B, C, _FAIL)	if (!TestEqual(#A, A, B, C)) _FAIL;
#define HOUDINI_TEST_NOT_EQUAL(A,B)	TestNotEqual(#A, A, B)
#define HOUDINI_TEST_NOT_EQUAL_ON_FAIL(A,B,_FAIL)	if (!TestNotEqual(#A, A, B)) _FAIL;
#define HOUDINI_TEST_NOT_NULL(A)	TestNotNull(#A, A)
#define HOUDINI_TEST_NOT_NULL_ON_FAIL(A, _FAIL)	if (!TestNotNull(#A, A) ) _FAIL;

#define HOUDINI_TEST_NULL(A)	TestNull(#A, A)

// Utils functions

struct FHoudiniEditorUnitTestUtils
{
	static UHoudiniAssetComponent* LoadHDAIntoNewMap(const FString& PackageName, const FTransform& Transform, bool bOpenWorld);

	static UWorld * CreateEmptyMap(bool bOpenWorld);

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

	// Helper function to returns proxy mesh components from an output.
	static inline TArray<UHoudiniStaticMeshComponent*>  GetOutputsWithProxyComponent(const TArray<UHoudiniOutput*>& Outputs)
	{
		TArray<UHoudiniStaticMeshComponent*> Results;

		for (UHoudiniOutput* Output : Outputs)
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				if (OutputObject.Value.ProxyComponent != nullptr)
				{
					UObject * Ptr = OutputObject.Value.ProxyComponent;
					UHoudiniStaticMeshComponent* ProxyMesh = Cast<UHoudiniStaticMeshComponent>(Ptr);
					Results.Add(ProxyMesh);
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

	// Helper function to return foliage objects from an output.
	static TArray<FHoudiniOutputObject*> GetOutputsWithFoliageType(const TArray<UHoudiniOutput*>& Outputs)
	{
		TArray<FHoudiniOutputObject*> Results;

		for (UHoudiniOutput* Output : Outputs)
		{
			for (auto& OutputObject : Output->GetOutputObjects())
			{
				if (OutputObject.Value.FoliageType)
				{
					Results.Add(&OutputObject.Value);
				}
			}
		}
		return  Results;
	}

	// Finds an actor by name.
	static AActor* GetActorWithName(UWorld* World, FString& Name);

	// Finds an HAC parameter of a specific class.
	static UHoudiniParameter * GetTypedParameter(UHoudiniAssetComponent * HAC, UClass * Class, const char* Name);

	// Finds an HAC parameter of a specific class.
	template <typename TYPED_PARAMETER>
	static TYPED_PARAMETER*  GetTypedParameter(UHoudiniAssetComponent* HAC, const char * Name)
	{
		return Cast<TYPED_PARAMETER>(GetTypedParameter(HAC, TYPED_PARAMETER::StaticClass(), Name));
	}

	// Returns only components on the exact type
	template<typename COMPONENT>
	static TArray<COMPONENT *> FilterComponents(const TArray<UActorComponent*> & Components)
	{
		TArray<COMPONENT *> Results;
		for(auto Component : Components)
		{
			if (Component->GetClass() == COMPONENT::StaticClass())
				Results.Add((COMPONENT*)Component);
		}
		return Results;
	}

	// Returns all the output actors in all the outputs.
	static TArray<AActor*> GetOutputActors(TArray<FHoudiniBakedOutput>& BakedOutputs);

	// Returns only the actors that are based off a given type.
	template<typename ACTORCLASS>
	static TArray<ACTORCLASS*> FilterActors(const TArray<AActor*> & Actors)
	{
		TArray<ACTORCLASS*> Results;
		for(AActor * Actor : Actors)
			if (Actor->IsA<ACTORCLASS>())
				Results.Add(Cast<ACTORCLASS>(Actor));
		return Results;
	}

	// Filters out those actors that have components of the given type.
	template<typename COMPONENT_CLASS>
	static TArray<AActor*> FilterActorsWithComponent(TArray<AActor*>& Actors)
	{
		TArray<AActor*> Results;
		for (auto Actor : Actors)
		{

			TArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			for (auto Component : Components)
			{
				if (Component->IsA<COMPONENT_CLASS>())
				{
					Results.Add(Actor);
					break;
				}
			}
		}
		return Results;
	}

	// Checks if an object was saved to the HAC's temp folder.
	static bool IsTemporary(UHoudiniAssetComponent * HAC, const FString & ObjectPath);
};


// Helper macro to set parm, ensures the parameter is valid.
#define SET_HDA_PARAMETER(_HAC, _PARAMETER_TYPE, _PARAMATER_NAME, _PARAMETER_VALUE, _PARAMETER_INDEX)\
	{\
		_PARAMETER_TYPE* __Parameter = FHoudiniEditorUnitTestUtils::GetTypedParameter<_PARAMETER_TYPE>(_HAC, _PARAMATER_NAME);\
		if (!TestNotNull(#_PARAMATER_NAME, __Parameter))\
		{\
			return true;\
		}\
		__Parameter->SetValueAt(_PARAMETER_VALUE, _PARAMETER_INDEX);\
	}

// Helper macro to set parm, ensures the parameter is valid.
#define SET_HDA_PARAMETER_NUM_ELEMENTS(_HAC, _PARAMETER_TYPE, _PARAMATER_NAME, _PARAMETER_VALUE)\
	{\
		_PARAMETER_TYPE* __Parameter = FHoudiniEditorUnitTestUtils::GetTypedParameter<_PARAMETER_TYPE>(_HAC, _PARAMATER_NAME);\
		if (!TestNotNull(#_PARAMATER_NAME, __Parameter))\
		{\
			return true;\
		}\
		__Parameter->SetNumElements(_PARAMETER_VALUE);\
	}

struct FHoudiniTestContext
{
	// Create and pass one of these structures between different latent parts of a test  (ie. those added
	// with AddCommand()). It keeps track of the test and stores timer info to handle timeouts (to avoid
	// hanging build PCs).
	//
	// The "Data" map can be used to pass data between tests.
	//
	FHoudiniTestContext(FAutomationTestBase* CurrentTest,
		const FString& HDAName,
		const FTransform& Transform,
		bool bOpenWorld);

	FHoudiniTestContext(FAutomationTestBase* CurrentTest, const FString& MapName);

	FHoudiniTestContext(FAutomationTestBase* CurrentTest,	bool bOpenWorld);

	~FHoudiniTestContext();

	// Starts cooking the HDA asynchrously.
	void StartCookingHDA();

	// Starts cooking the Selected top network in the HDA asynchronously.
	void StartCookingSelectedTOPNetwork();

	void WaitForTicks(int Count);

	void SetHAC(UHoudiniAssetComponent* HACToUse);

	//  Check if the context is valid. This will be false if, for example, the HDA failed to load.
	bool IsValid();

	// Bakes the top network. Synchronous, returns the baked actors.
	TArray<FHoudiniEngineBakedActor> BakeSelectedTopNetwork();

	double MaxTime = 120.0f;						// Max time (seconds) this test can run.
	double TimeStarted = 0.0f;					// Time this test started. Used to test for timeout.

	FAutomationTestBase* Test = nullptr;		// Unit test underway
	UHoudiniAssetComponent* HAC = nullptr;		// HAC being tested
	TMap<FString, FString> Data;				// Use this to pass data between different tests.
	bool bCookInProgress = false;
	bool bPostOutputDelegateCalled = false;
	bool bPDGCookInProgress = false;
	bool bPDGPostCookDelegateCalled = false;
	int WaitTickFrame = 0;
	UWorld * World = nullptr;

private:
	FDelegateHandle OutputDelegateHandle;

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