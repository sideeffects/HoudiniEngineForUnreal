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
#include "CoreMinimal.h"
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

#define REPORT_ERROR(_X) if (!(_X)) AddError(FString::Printf(TEXT("%s:%d"), TEXT(__FILE__), __LINE__))

#define HOUDINI_TEST_EQUAL(A,...)		REPORT_ERROR(TestEqual(#A, A, __VA_ARGS__))

#define HOUDINI_TEST_EQUALISH(A,B,C)	REPORT_ERROR(TestEqual(#A, A, B, C));
#define HOUDINI_TEST_NOT_EQUAL(A,B)		REPORT_ERROR(TestNotEqual(#A, A, B))
#define HOUDINI_TEST_NULL(A)			REPORT_ERROR(TestNull(#A, A))
#define HOUDINI_TEST_NOT_NULL(A)		REPORT_ERROR(TestNotNull(#A, A))

#define HOUDINI_TEST_EQUAL_ON_FAIL(A,B,_FAIL)\
	{\
		bool ____bSuccess = TestEqual(#A, A, B);\
		if (!____bSuccess)\
		{\
			REPORT_ERROR(true);\
			_FAIL;\
		}\
	}

#define HOUDINI_TEST_EQUALISH_ON_FAIL(A,B, C, _FAIL)\
	{\
			bool ____bSuccess = TestEqual(#A, A, B, C);\
			if(!____bSuccess)\
			{\
				REPORT_ERROR(true);\
				_FAIL;\
			}\
	}

#define HOUDINI_TEST_NOT_EQUAL_ON_FAIL(A,B,_FAIL)\
	{\
			bool ____bSuccess = TestNotEqual(#A, A, B);\
			if(!____bSuccess)\
			{\
				REPORT_ERROR(true);\
				_FAIL;\
			}\
	}

#define HOUDINI_TEST_NULL_ON_FAIL(A,_FAIL)\
	{\
			bool ____bSuccess = TestNull(#A, A);\
			if(!____bSuccess)\
			{\
				REPORT_ERROR(true);\
				_FAIL;\
			}\
	}

#define HOUDINI_TEST_NOT_NULL_ON_FAIL(A, _FAIL)\
	{\
			bool ____bSuccess = TestNotNull(#A, A);\
			if(!____bSuccess)\
			{\
				REPORT_ERROR(true);\
				_FAIL;\
			}\
	}

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

	// Finds a Cookable parameter of a specific class.
	static UHoudiniParameter* GetTypedParameter(UHoudiniCookable* HC, UClass* Class, const char* Name);

	// Finds an HAC parameter of a specific class.
	template <typename TYPED_PARAMETER>
	static TYPED_PARAMETER*  GetTypedParameter(UHoudiniAssetComponent* HAC, const char * Name)
	{
		return Cast<TYPED_PARAMETER>(GetTypedParameter(HAC, TYPED_PARAMETER::StaticClass(), Name));
	}

	// Finds a Cookable parameter of a specific class.
	template <typename TYPED_PARAMETER>
	static TYPED_PARAMETER* GetTypedParameter(UHoudiniCookable* HC, const char* Name)
	{
		return Cast<TYPED_PARAMETER>(GetTypedParameter(HC, TYPED_PARAMETER::StaticClass(), Name));
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

	// Returns all the output instacnce actors in all the outputs.
	static TArray<AActor*> GetOutputInstancedActors(TArray<FHoudiniBakedOutput>& BakedOutputs);

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
	static bool IsTemporary(const FString& TempFolder, const FString& ObjectPath);
};


// Helper macro to set parm, ensures the parameter is valid.
#define SET_HDA_PARAMETER(_CTX, _PARAMETER_TYPE, _PARAMETER_NAME, _PARAMETER_VALUE, _PARAMETER_INDEX)\
	{\
		_PARAMETER_TYPE* __Parameter = nullptr;\
		if(_CTX->GetCookable())\
			__Parameter = FHoudiniEditorUnitTestUtils::GetTypedParameter<_PARAMETER_TYPE>(_CTX->GetCookable(), _PARAMETER_NAME);\
		else\
			__Parameter = FHoudiniEditorUnitTestUtils::GetTypedParameter<_PARAMETER_TYPE>(_CTX->GetHAC(), _PARAMETER_NAME); \
		if (!TestNotNull(#_PARAMETER_NAME, __Parameter))\
		{\
			return true;\
		}\
		__Parameter->SetValueAt(_PARAMETER_VALUE, _PARAMETER_INDEX);\
	}

// Helper macro to set parm, ensures the parameter is valid.
#define SET_HDA_PARAMETER_NUM_ELEMENTS(_CTX, _PARAMETER_TYPE, _PARAMETER_NAME, _PARAMETER_VALUE)\
	{\
		_PARAMETER_TYPE* __Parameter = nullptr;\
		if(_CTX->GetCookable())\
			__Parameter = FHoudiniEditorUnitTestUtils::GetTypedParameter<_PARAMETER_TYPE>(_CTX->GetCookable(), _PARAMETER_NAME);\
		else\
			__Parameter = FHoudiniEditorUnitTestUtils::GetTypedParameter<_PARAMETER_TYPE>(_CTX->GetHAC(), _PARAMETER_NAME);\
		if (!TestNotNull(#_PARAMETER_NAME, __Parameter))\
		{\
			return true;\
		}\
		__Parameter->SetNumElements(_PARAMETER_VALUE);\
	}

enum class EHoudiniContextState : uint8
{
	Idle,
	Cooking,
	Complete
};

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

	FHoudiniTestContext(FAutomationTestBase* CurrentTest, const FString& MapName, const FString& ActorLabel = TEXT(""));

	FHoudiniTestContext(FAutomationTestBase* CurrentTest, bool bOpenWorld);

	FHoudiniTestContext(FAutomationTestBase* CurrentTest, UWorld* World, const FString& ActorLabel);

	~FHoudiniTestContext();

	void FindHACInWorld(const FString& ActorLabel = TEXT(""));

	// Starts cooking the HDA asynchrously.
	void StartCookingHDA();

	// Starts cooking the Selected top network in the HDA asynchronously.
	bool StartCookingSelectedTOPNetwork();

	void WaitForTicks(int Count);

	void SetHAC(UHoudiniAssetComponent* HACToUse);
	void SetCookable(UHoudiniCookable* HCToUse);

	UHoudiniAssetComponent* GetHAC();
	UHoudiniCookable* GetCookable();

	// Helper function to simplify the code when dealing with HAC and Cookables
	bool Bake(const FHoudiniBakeSettings& InBakeSettings);
	void GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const;
	TArray<FHoudiniBakedOutput>& GetBakedOutputs();
	UHoudiniInput* GetInputAt(const int Idx);
	void SetProxyMeshEnabled(const bool bEnabled);
	FString GetBakeFolderOrDefault() const;
	UWorld* GetWorld() const;
	UHoudiniPDGAssetLink* GetPDGAssetLink();
	FString GetTemporaryCookFolderOrDefault() const;

	//  Check if the context is valid. This will be false if, for example, the HDA failed to load.
	bool IsValid();

	// Bakes the top network. Synchronous, returns the baked actors.
	TArray<FHoudiniEngineBakedActor> BakeSelectedTopNetwork();

	double MaxTime = 120.0f;					// Max time (seconds) this test can run.
	double TimeStarted = 0.0f;					// Time this test started. Used to test for timeout.

	FAutomationTestBase* Test = nullptr;		// Unit test underway
	
	UHoudiniAssetComponent* HAC = nullptr;		// HAC being tested
	UHoudiniCookable* HC = nullptr;				// Cookable being tested

	TMap<FString, FString> Data;				// Use this to pass data between different tests.

	EHoudiniContextState CookingState = EHoudiniContextState::Idle;
	EHoudiniContextState PDGState = EHoudiniContextState::Idle;

	int WaitTickFrame = 0;
	UWorld * World = nullptr;

private:
	FDelegateHandle OutputDelegateHandle;

};

class FHoudiniMultiTestContext
{
public:
	// Use this context if you have multuiple contexts, for example 2 HACs in one map.
	TArray<TSharedPtr<FHoudiniTestContext>> Contexts;
};

#if WITH_DEV_AUTOMATION_TESTS
class FHoudiniLatentTestCommand : public FFunctionLatentCommand
{
public:
	// Each part of an HDA that requires a Cook should be its own FFunctionLatentCommand. Before the Update()
	// function is called this class ensures the previous cook completed.

	// Use this constructor if you have one HDA.
	FHoudiniLatentTestCommand(TSharedPtr<FHoudiniTestContext> InContext, TFunction<bool()> InLatentPredicate)
		: FFunctionLatentCommand(InLatentPredicate), SingleContext(InContext) {}

	FHoudiniLatentTestCommand(TSharedPtr<FHoudiniMultiTestContext> InContext, TFunction<bool()> InLatentPredicate)
		: FFunctionLatentCommand(InLatentPredicate), MultiContext(InContext) {
	}

	TSharedPtr<FHoudiniTestContext> SingleContext;
	TSharedPtr<FHoudiniMultiTestContext> MultiContext;

	// Like its base class, return true when the command is complete, false when it should be called again.
	virtual bool Update() override;

	bool CheckForCookingComplete(FHoudiniTestContext* Context);
};
#endif