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


#include "HoudiniEditorTestPCG.h"

#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "HoudiniParameterFloat.h"
#include <FileHelpers.h>
#include "PCGComponent.h"
#include "PCGVolume.h"
#include "PCGGraph.h"
#include "PCGDataAsset.h"
#include "Data/PCGPointData.h"
#include "HoudiniPCGDataObject.h"
#include "InstancedFoliageActor.h"
#include "Landscape.h"
#include "PCGParamData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"

enum class EHoudiniTestPCGContextState : int
{
	None,
	Cleanup,
	Cleaned,
	Generate,
	Done
};

class EHoudiniTestPCGContext
{
public:
	void LoadPCGTestMap(const FString& MapName);

	void CleanupAndGenerateAsync();
	void Cleanup();
	void GenerateAsync();

	bool Update();
	
	UPCGComponent* PCGComponent = nullptr;
	EHoudiniTestPCGContextState State = EHoudiniTestPCGContextState::None;

private:
	void OnGraphCleaned(UPCGComponent* PCGComponent_);
	void OnGraphGenerated(UPCGComponent* PCGComponent_);
	bool bDoGenerateAfterClean;
};


void EHoudiniTestPCGContext::LoadPCGTestMap(const FString & MapName)
{
	// Now create the test context.
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapName);
	if(!IsValid(World))
		return;

	for(TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		APCGVolume* PCGVolume = Cast<APCGVolume>(*ActorItr);
		if(IsValid(PCGVolume))
		{
			PCGComponent = PCGVolume->GetComponentByClass<UPCGComponent>();
			PCGComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &EHoudiniTestPCGContext::OnGraphCleaned);
			PCGComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &EHoudiniTestPCGContext::OnGraphGenerated);
		}
	}
}


void EHoudiniTestPCGContext::OnGraphCleaned(UPCGComponent* PCGComponent_)
{
	this->State = EHoudiniTestPCGContextState::Cleaned;
}

void EHoudiniTestPCGContext::OnGraphGenerated(UPCGComponent* PCGComponent_)
{
	if (this->State == EHoudiniTestPCGContextState::Generate)
	{
		this->State = EHoudiniTestPCGContextState::Done;
	}

}		

void EHoudiniTestPCGContext::GenerateAsync()
{
	this->State = EHoudiniTestPCGContextState::Generate;
	PCGComponent->GenerateLocal(true);
}

void EHoudiniTestPCGContext::CleanupAndGenerateAsync()
{
	this->bDoGenerateAfterClean = true;

	if(PCGComponent->bGenerated)
	{
		this->State = EHoudiniTestPCGContextState::Cleanup;
		PCGComponent->Cleanup();
	}
	else
	{
		this->State = EHoudiniTestPCGContextState::Cleaned;
	}
}

void EHoudiniTestPCGContext::Cleanup()
{
	this->bDoGenerateAfterClean = false;

	if(PCGComponent->bGenerated)
	{
		this->State = EHoudiniTestPCGContextState::Cleanup;
		PCGComponent->Cleanup();
	}
}


bool EHoudiniTestPCGContext::Update()
{
	switch(this->State)
	{
	case EHoudiniTestPCGContextState::Cleaned:
		if(bDoGenerateAfterClean)
		{
			GenerateAsync();
		}
		else
		{
			this->State = EHoudiniTestPCGContextState::Done;
		}
		break;
	default:
		break;
	}

	return this->State == EHoudiniTestPCGContextState::Done;
}

UObject* FHoudiniEditorTestPCG::GetOutputObject(UHoudiniPCGDataObject* PCGDataObject, const FString & Field, int Index)
{
	auto * Attr = Cast<UHoudiniPCGDataAttributeSoftObjectPath>(PCGDataObject->FindAttribute(Field));
	if(!IsValid(Attr) || !Attr->Values.IsValidIndex(Index))
		return nullptr;

	const FString & ObjectPath = Attr->Values[Index].ToString();

	UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	return Object;

}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_MeshesCooked, "Houdini.UnitTests.PCG.Meshes.Cooked",
                                         EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_MeshesCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGMesh/PCGMeshLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FSoftObjectPath>(FName("object"), FSoftObjectPath(TEXT("/Game/TestObjects/SM_Cube.SM_Cube")));
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);
	GraphInstance->SetGraphParameter<float>(FName("scale_factor"), 1.0f);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

		// ... check the mesh's bounding box.
		FBox Box = StaticMesh->GetBoundingBox();

		HOUDINI_TEST_EQUAL(Box.Min.X, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Y, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Z, -50.0);
		HOUDINI_TEST_EQUAL(Box.Max.X, 50.0);
		HOUDINI_TEST_EQUAL(Box.Max.Y, 50.0);
		HOUDINI_TEST_EQUAL(Box.Max.Z, 50.0);

		// ... check we have a mesh component
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);

		return true;
	}));

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 2: Load a cube, then use it to generate a new cube but using parameters to scale it.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context, GraphInstance]
	{
		Context->State = EHoudiniTestPCGContextState::Generate;
		GraphInstance->SetGraphParameter<float>(FName("scale_factor"), 2.0f);
		Context->CleanupAndGenerateAsync();
	
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>(GetTransientPackage());
		PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

		// ... check the mesh's bounding box.
		FBox Box = StaticMesh->GetBoundingBox();
		HOUDINI_TEST_EQUAL(Box.Min.X, -100.0);
		HOUDINI_TEST_EQUAL(Box.Min.Y, -100.0);
		HOUDINI_TEST_EQUAL(Box.Min.Z, -100.0);
		HOUDINI_TEST_EQUAL(Box.Max.X, 100.0);
		HOUDINI_TEST_EQUAL(Box.Max.Y, 100.0);
		HOUDINI_TEST_EQUAL(Box.Max.Z, 100.0);

		// ... check we have a mesh component
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_MeshesBaked, "Houdini.UnitTests.PCG.Meshes.Baked.SceneComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_MeshesBaked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGMesh/PCGMeshLevelBaked.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FSoftObjectPath>(FName("object"), FSoftObjectPath(TEXT("/Game/TestObjects/SM_Cube.SM_Cube")));
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);
	GraphInstance->SetGraphParameter<float>(FName("scale_factor"), 1.0f);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

		// ... check the mesh's bounding box.
		FBox Box = StaticMesh->GetBoundingBox();

		HOUDINI_TEST_EQUAL(Box.Min.X, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Y, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Z, -50.0);
		HOUDINI_TEST_EQUAL(Box.Max.X, 50.0);
		HOUDINI_TEST_EQUAL(Box.Max.Y, 50.0);
		HOUDINI_TEST_EQUAL(Box.Max.Z, 50.0);

		// ... check we have a mesh actor
		AStaticMeshActor* StaticMeshComponent = Cast<AStaticMeshActor>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("actor")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);

		// The parent should be an actor.

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_MeshesBakedNoSceneComponents, "Houdini.UnitTests.PCG.Meshes.Baked.NoSceneComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_MeshesBakedNoSceneComponents::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGMesh/PCGMeshLevelBakedNoSceneComponents.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FSoftObjectPath>(FName("object"), FSoftObjectPath(TEXT("/Game/TestObjects/SM_Cube.SM_Cube")));
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);
	GraphInstance->SetGraphParameter<float>(FName("scale_factor"), 1.0f);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
		{
			if(!Context->Update())
				return false;

			UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

			// We should have one output...
			HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
			// ... it should have data ...
			HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
			// ... which we'll now convert to an PCGDataObject so we can easily ready it...
			UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
			PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

			// ... check we have a mesh
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object")));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

			// ... check the mesh's bounding box.
			FBox Box = StaticMesh->GetBoundingBox();

			HOUDINI_TEST_EQUAL(Box.Min.X, -50.0);
			HOUDINI_TEST_EQUAL(Box.Min.Y, -50.0);
			HOUDINI_TEST_EQUAL(Box.Min.Z, -50.0);
			HOUDINI_TEST_EQUAL(Box.Max.X, 50.0);
			HOUDINI_TEST_EQUAL(Box.Max.Y, 50.0);
			HOUDINI_TEST_EQUAL(Box.Max.Z, 50.0);

			// ... check we have a mesh actor
			AStaticMeshActor* StaticMeshComponent = Cast<AStaticMeshActor>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("actor")));
			HOUDINI_TEST_NULL_ON_FAIL(StaticMeshComponent, return true);

			// The parent should be an actor.

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_FoliageBaked, "Houdini.UnitTests.PCG.Foliage.Baked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_FoliageBaked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});


	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGFoliage/PCGTestFoliageMap.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	auto GetFoliageCount = [](UWorld* World)
		{
			int32 TotalCount = 0;

			for(TActorIterator<AActor> It(World, AInstancedFoliageActor::StaticClass()); It; ++It)
			{
				AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(*It);

				auto InstanceMap = IFA->GetAllInstancesFoliageType();
				for(auto& InstanceIt : InstanceMap)
				{
					FFoliageInfo* FoliageInfo = InstanceIt.Value;
					TotalCount += FoliageInfo->Instances.Num();
				}
			}

			return TotalCount;
		};

	int BeginCount = GetFoliageCount(Context->PCGComponent->GetWorld());
	HOUDINI_TEST_EQUAL(BeginCount, 0);

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, GetFoliageCount]()
	{
		if(!Context->Update())
				return false;

		int FoliageCount = GetFoliageCount(Context->PCGComponent->GetWorld());
		HOUDINI_TEST_NOT_EQUAL(FoliageCount, 0);

		// Now clean to make sure clean up is performed.

		Context->Cleanup();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, GetFoliageCount]()
		{
			if(!Context->Update())
				return false;

			int FoliageCount = GetFoliageCount(Context->PCGComponent->GetWorld());
			HOUDINI_TEST_EQUAL(FoliageCount, 0);

			return true;
		}));
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_LandscapesCookedCreate, "Houdini.UnitTests.PCG.Landscapes.Cooked.Create",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_LandscapesCookedCreate::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGLandscape/PCGTestLandscapeLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);
	GraphInstance->SetGraphParameter<float>(FName("scale_factor"), 1.0f);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
			if(!Context->Update())
				return false;

		UPCGDataAsset* MyObject = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MyObject, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(MyObject->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MyObject->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->SetFromPCGData(MyObject->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		ALandscape* Landscape = Cast<ALandscape>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("actor")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Landscape, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_LandscapesCookedModiy, "Houdini.UnitTests.PCG.Landscapes.Cooked.Modify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_LandscapesCookedModiy::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGLandscapeMods/PCGTestLandscapeCookedLevel.map"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);


	ALandscape* Landscape = nullptr;
	for(TActorIterator<AActor> It(Context->PCGComponent->GetWorld()); It; ++It)
	{
		Landscape = Cast<ALandscape>(*It);
		if(Landscape)
			break;
	}

	FName LayerName = TEXT("Noise");

	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	HOUDINI_TEST_EQUAL_ON_FAIL(EditLayerIndex, INDEX_NONE, true);

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, Landscape, LayerName]()
		{
			if(!Context->Update())
				return false;

			// Make sure the layer was created.
			int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(EditLayerIndex, int32(INDEX_NONE), true);

			//	Now start clean.
			Context->Cleanup();

			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, Landscape, LayerName]()
		{
			if(!Context->Update())
				return false;

			// Make sure the layer was removed.
			int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
			HOUDINI_TEST_EQUAL_ON_FAIL(EditLayerIndex, int32(INDEX_NONE), true);

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_LandscapesBakedModiy, "Houdini.UnitTests.PCG.Landscapes.Baked.Modify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_LandscapesBakedModiy::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGLandscapeMods/PCGTestLandscapeLevel.map"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);


	ALandscape* Landscape = nullptr;
	for(TActorIterator<AActor> It(Context->PCGComponent->GetWorld()); It; ++It)
	{
		Landscape = Cast<ALandscape>(*It);
		if(Landscape)
			break;
	}

	FName LayerName = TEXT("Noise");

	int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
	HOUDINI_TEST_EQUAL_ON_FAIL(EditLayerIndex, INDEX_NONE, true);

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, Landscape, LayerName]()
		{
			if(!Context->Update())
				return false;

			// Make sure the layer was created.
			int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(EditLayerIndex, int32(INDEX_NONE), true);

			//	Now start clean.
			Context->Cleanup();

			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, Landscape, LayerName]()
		{
			if(!Context->Update())
				return false;

			// Make sure the layer was removed.
			int32 EditLayerIndex = Landscape->GetLayerIndex(LayerName);
			HOUDINI_TEST_EQUAL_ON_FAIL(EditLayerIndex, int32(INDEX_NONE), true);

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGNativeOutputsCooked, "Houdini.UnitTests.PCG.PCGOutputs.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGNativeOutputsCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGInputsOutputs/PCGTestOutputsLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for (int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString> & Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// CHECK POINTS OUTPUT
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if (Tags.Contains(TEXT("Points")))
			{

				TArray<FPCGPoint> ExpectedPoints;
				ExpectedPoints.SetNum(8);
				float CubeSize = 50.0f;
				ExpectedPoints[0].Transform.SetLocation(FVector3d(+CubeSize, +CubeSize, -CubeSize));
				ExpectedPoints[1].Transform.SetLocation(FVector3d(-CubeSize, +CubeSize, -CubeSize));
				ExpectedPoints[2].Transform.SetLocation(FVector3d(+CubeSize, +CubeSize, +CubeSize));
				ExpectedPoints[3].Transform.SetLocation(FVector3d(-CubeSize, +CubeSize, +CubeSize));
				ExpectedPoints[4].Transform.SetLocation(FVector3d(-CubeSize, -CubeSize, -CubeSize));
				ExpectedPoints[5].Transform.SetLocation(FVector3d(+CubeSize, -CubeSize, -CubeSize));
				ExpectedPoints[6].Transform.SetLocation(FVector3d(-CubeSize, -CubeSize, +CubeSize));
				ExpectedPoints[7].Transform.SetLocation(FVector3d(+CubeSize, -CubeSize, +CubeSize));

				for (int PointIndex = 0; PointIndex < ExpectedPoints.Num(); PointIndex++)
				{
					ExpectedPoints[PointIndex].Color = FVector4(0.25, 0.5, 0.75, 1.0);
					ExpectedPoints[PointIndex].Density = 0.5;
					ExpectedPoints[PointIndex].Steepness = 0.25;
					ExpectedPoints[PointIndex].Seed = static_cast<float>(PointIndex);
				}
				const UPCGPointData* PCGPointData = Cast<UPCGPointData>(TaggedData.Data.Get());
				HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGPointData, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(PCGPointData->GetNumPoints(), 8, continue);
				for (int PointIndex = 0; PointIndex < PCGPointData->GetNumPoints(); PointIndex++)
				{
					FPCGPoint Point = PCGPointData->GetPoint(PointIndex);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Transform.GetLocation(), ExpectedPoints[PointIndex].Transform.GetLocation(), continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Color, ExpectedPoints[PointIndex].Color, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Density, ExpectedPoints[PointIndex].Density, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Steepness, ExpectedPoints[PointIndex].Steepness, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Seed, ExpectedPoints[PointIndex].Seed, continue);
				}

				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGPointData);
				auto* BaseAttr = PCGDataObject->FindAttribute(TEXT("TestInt"));
				auto* AttrInt = Cast< UHoudiniPCGDataAttributeInt>(BaseAttr);

				HOUDINI_TEST_NOT_NULL_ON_FAIL(AttrInt, return true);

				HOUDINI_TEST_EQUAL(AttrInt->GetNumValues(), 8);
				for(int Index = 0; Index < AttrInt->GetNumValues(); Index++)
				{
					HOUDINI_TEST_EQUAL(AttrInt->Values[Index], Index * 10);
				}

				BaseAttr = PCGDataObject->FindAttribute(TEXT("TestFloat"));
				auto* AttrFloat = Cast< UHoudiniPCGDataAttributeFloat>(BaseAttr);

				HOUDINI_TEST_NOT_NULL_ON_FAIL(AttrFloat, return true);

				HOUDINI_TEST_EQUAL(AttrFloat->GetNumValues(), 8);
				for(int Index = 0; Index < AttrFloat->GetNumValues(); Index++)
				{
					HOUDINI_TEST_EQUAL(AttrFloat->Values[Index], Index * 10.0f);
				}

				BaseAttr = PCGDataObject->FindAttribute(TEXT("TestString"));
				auto* AttrString = Cast< UHoudiniPCGDataAttributeString>(BaseAttr);

				HOUDINI_TEST_NOT_NULL_ON_FAIL(AttrString, return true);

				HOUDINI_TEST_EQUAL(AttrString->GetNumValues(), 8);
				for(int Index = 0; Index < AttrString->GetNumValues(); Index++)
				{
					FString Expected = FString::Printf(TEXT("str-%d"), Index);
					HOUDINI_TEST_EQUAL(AttrString->Values[Index], Expected);
				}

				BaseAttr = PCGDataObject->FindAttribute(TEXT("TestVec3"));
				auto* AttrVec3 = Cast<UHoudiniPCGDataAttributeVector3d>(BaseAttr);

				HOUDINI_TEST_NOT_NULL_ON_FAIL(AttrVec3, return true);

				HOUDINI_TEST_EQUAL(AttrVec3->GetNumValues(), 8);
				for(int Index = 0; Index < AttrVec3->GetNumValues(); Index++)
				{
					FVector3d Expected = FVector3d(Index * 1.0f, Index * 2.0, Index * 3.0);
					HOUDINI_TEST_EQUAL(AttrVec3->Values[Index], Expected);
				}
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK VERTICES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Vertices")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);
				auto * VertexIds = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("__vertex_id")));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(VertexIds, continue);

				// 3 vertices per triangle, 2 triangles per face = 6 * 2 * 3
				HOUDINI_TEST_EQUAL(VertexIds->Values.Num(), 36);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK PRIMITIVES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Primitives")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				auto* PrimitiveIds = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("__primitive_id")));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(PrimitiveIds, continue);

				// 3 vertices per triangle, 2 triangles per face 
				HOUDINI_TEST_EQUAL(PrimitiveIds->Values.Num(), 12);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				{
					UHoudiniPCGDataAttributeInt64* Attrs = Cast<UHoudiniPCGDataAttributeInt64>(PCGDataObject->FindAttribute(TEXT("__primitivelist")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
				{
					UHoudiniPCGDataAttributeInt64* Attrs = Cast<UHoudiniPCGDataAttributeInt64>(PCGDataObject->FindAttribute(TEXT("__topology")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
				{
					UHoudiniPCGDataAttributeInt* Attrs = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("unreal_pcg_params")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGNativeOutputsBaked, "Houdini.UnitTests.PCG.PCGOutputs.Baked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_PCGNativeOutputsBaked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGInputsOutputs/PCGTestOutputsLevelBaked.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString>& Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// CHECK POINTS OUTPUT
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Points")))
			{

				TArray<FPCGPoint> ExpectedPoints;
				ExpectedPoints.SetNum(8);
				float CubeSize = 50.0f;
				ExpectedPoints[0].Transform.SetLocation(FVector3d(+CubeSize, +CubeSize, -CubeSize));
				ExpectedPoints[1].Transform.SetLocation(FVector3d(-CubeSize, +CubeSize, -CubeSize));
				ExpectedPoints[2].Transform.SetLocation(FVector3d(+CubeSize, +CubeSize, +CubeSize));
				ExpectedPoints[3].Transform.SetLocation(FVector3d(-CubeSize, +CubeSize, +CubeSize));
				ExpectedPoints[4].Transform.SetLocation(FVector3d(-CubeSize, -CubeSize, -CubeSize));
				ExpectedPoints[5].Transform.SetLocation(FVector3d(+CubeSize, -CubeSize, -CubeSize));
				ExpectedPoints[6].Transform.SetLocation(FVector3d(-CubeSize, -CubeSize, +CubeSize));
				ExpectedPoints[7].Transform.SetLocation(FVector3d(+CubeSize, -CubeSize, +CubeSize));

				for(int PointIndex = 0; PointIndex < ExpectedPoints.Num(); PointIndex++)
				{
					ExpectedPoints[PointIndex].Color = FVector4(0.25, 0.5, 0.75, 1.0);
					ExpectedPoints[PointIndex].Density = 0.5;
					ExpectedPoints[PointIndex].Steepness = 0.25;
					ExpectedPoints[PointIndex].Seed = static_cast<float>(PointIndex);
				}
				const UPCGPointData* PCGPointData = Cast<UPCGPointData>(TaggedData.Data.Get());
				HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGPointData, continue);
				HOUDINI_TEST_EQUAL_ON_FAIL(PCGPointData->GetNumPoints(), 8, continue);
				for(int PointIndex = 0; PointIndex < PCGPointData->GetNumPoints(); PointIndex++)
				{
					FPCGPoint Point = PCGPointData->GetPoint(PointIndex);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Transform.GetLocation(), ExpectedPoints[PointIndex].Transform.GetLocation(), continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Color, ExpectedPoints[PointIndex].Color, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Density, ExpectedPoints[PointIndex].Density, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Steepness, ExpectedPoints[PointIndex].Steepness, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Point.Seed, ExpectedPoints[PointIndex].Seed, continue);
				}
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK VERTICES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Vertices")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);
				auto* VertexIds = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("__vertex_id")));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(VertexIds, continue);

				// 3 vertices per triangle, 2 triangles per face = 6 * 2 * 3
				HOUDINI_TEST_EQUAL(VertexIds->Values.Num(), 36);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK PRIMITIVES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Primitives")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				auto* PrimitiveIds = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("__primitive_id")));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(PrimitiveIds, continue);

				// 3 vertices per triangle, 2 triangles per face 
				HOUDINI_TEST_EQUAL(PrimitiveIds->Values.Num(), 12);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				{
					UHoudiniPCGDataAttributeInt64* Attrs = Cast<UHoudiniPCGDataAttributeInt64>(PCGDataObject->FindAttribute(TEXT("__primitivelist")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
				{
					UHoudiniPCGDataAttributeInt64* Attrs = Cast<UHoudiniPCGDataAttributeInt64>(PCGDataObject->FindAttribute(TEXT("__topology")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
				{
					UHoudiniPCGDataAttributeInt* Attrs = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("unreal_pcg_params")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGNativeInputsCooked, "Houdini.UnitTests.PCG.PCGInputs.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGNativeInputsCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGInputsOutputs/PCGTestInputsLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString>&  Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// CHECK POINTS OUTPUT
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Points")))
			{
				const UPCGPointData* PCGPointData = Cast<UPCGPointData>(TaggedData.Data.Get());
				HOUDINI_TEST_EQUAL(PCGPointData->GetNumPoints(), 148);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK VERTICES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Vertices")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* VerticesObject = NewObject<UHoudiniPCGDataObject>();
				VerticesObject->SetFromPCGData(PCGParam);
				HOUDINI_TEST_EQUAL(VerticesObject->Attributes.Num(), 0);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK PRIMITIVES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Primitives")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PrimitivesObject = NewObject<UHoudiniPCGDataObject>();
				PrimitivesObject->SetFromPCGData(PCGParam);
				HOUDINI_TEST_EQUAL(PrimitivesObject->Attributes.Num(), 0);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* DetailsObject = NewObject<UHoudiniPCGDataObject>();
				DetailsObject->SetFromPCGData(PCGParam);
				// topology, primitive list and unreal_pcg_params
				HOUDINI_TEST_EQUAL(DetailsObject->Attributes.Num(), 3);
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGNativeMultiInputsCooked, "Houdini.UnitTests.PCG.PCGMultiInputs.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGNativeMultiInputsCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGInputsOutputs/PCGMultipleInputsLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString>& Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// CHECK POINTS OUTPUT
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Points")))
			{
				const UPCGPointData* PCGPointData = Cast<UPCGPointData>(TaggedData.Data.Get());
				HOUDINI_TEST_EQUAL(PCGPointData->GetNumPoints(), 148);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK VERTICES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Vertices")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* VerticesObject = NewObject<UHoudiniPCGDataObject>();
				VerticesObject->SetFromPCGData(PCGParam);
				HOUDINI_TEST_EQUAL(VerticesObject->Attributes.Num(), 0);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK PRIMITIVES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Primitives")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PrimitivesObject = NewObject<UHoudiniPCGDataObject>();
				PrimitivesObject->SetFromPCGData(PCGParam);
				HOUDINI_TEST_EQUAL(PrimitivesObject->Attributes.Num(), 0);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* DetailsObject = NewObject<UHoudiniPCGDataObject>();
				DetailsObject->SetFromPCGData(PCGParam);
				// topology, primitive list and unreal_pcg_params
				HOUDINI_TEST_EQUAL(DetailsObject->Attributes.Num(), 3);
			}
		}

		return true;
	}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGSplinesCooked, "Houdini.UnitTests.PCG.PCGSplines.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGSplinesCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGSplines/PCGSplinesLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
			if(!Context->Update())
				return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 2, return true);

		{
			const UPCGSplineData* PCGSplineData = Cast<UPCGSplineData>(PCGDataAsset->Data.TaggedData[0].Data);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
			HOUDINI_TEST_EQUAL_ON_FAIL(PCGSplineData->SplineStruct.GetNumberOfPoints(), 4, return true);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[0].OutVal, FVector(-542.820597, 742.795944, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[1].OutVal, FVector(588.031721, 665.821791, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[2].OutVal, FVector(319.931078, -1339.993763, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[3].OutVal, FVector(864.991283, -1053.272057, 0.000000));
#else
			HOUDINI_TEST_EQUAL_ON_FAIL(PCGSplineData->SplineStruct.SplineCurves.Position.Points.Num(), 4, return true);			
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[0].OutVal,FVector(-542.820597, 742.795944, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[1].OutVal, FVector(588.031721, 665.821791, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[2].OutVal, FVector(319.931078, -1339.993763, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[3].OutVal, FVector(864.991283, -1053.272057, 0.000000));
#endif
		}

		{
			const UPCGSplineData* PCGSplineData = Cast<UPCGSplineData>(PCGDataAsset->Data.TaggedData[1].Data);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetNumberOfPoints(), 3);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[0].OutVal, FVector(100.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[1].OutVal, FVector(200.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[2].OutVal, FVector(200.000000, -60.000002, 0.000000));
#else
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points.Num(), 3);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[0].OutVal, FVector(100.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[1].OutVal, FVector(200.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[2].OutVal, FVector(200.000000, -60.000002, 0.000000));
#endif
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGSplinesBaked, "Houdini.UnitTests.PCG.PCGSplines.Baked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGSplinesBaked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGSplines/PCGSplinesLevelBaked.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	FString AssetPath = TEXT("/Game/");
	FString AssetName = TEXT("PCG_Out");
	FString PCGAssetFullPath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

	UPCGGraphInstance* GraphInstance = Context->PCGComponent->GetGraphInstance();
	GraphInstance->SetGraphParameter<FString>(FName("out_path"), AssetPath);
	GraphInstance->SetGraphParameter<FString>(FName("out_name"), AssetName);


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(!Context->Update())
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 2, return true);

		{
			const UPCGSplineData* PCGSplineData = Cast<UPCGSplineData>(PCGDataAsset->Data.TaggedData[0].Data);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
			HOUDINI_TEST_EQUAL_ON_FAIL(PCGSplineData->SplineStruct.GetNumberOfPoints(), 4, return true);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[0].OutVal, FVector(-542.820597, 742.795944, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[1].OutVal, FVector(588.031721, 665.821791, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[2].OutVal, FVector(319.931078, -1339.993763, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[3].OutVal, FVector(864.991283, -1053.272057, 0.000000));
#else
			HOUDINI_TEST_EQUAL_ON_FAIL(PCGSplineData->SplineStruct.SplineCurves.Position.Points.Num(), 4, return true);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[0].OutVal, FVector(-542.820597, 742.795944, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[1].OutVal, FVector(588.031721, 665.821791, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[2].OutVal, FVector(319.931078, -1339.993763, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[3].OutVal, FVector(864.991283, -1053.272057, 0.000000));
#endif
		}

		{
			const UPCGSplineData* PCGSplineData = Cast<UPCGSplineData>(PCGDataAsset->Data.TaggedData[1].Data);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetNumberOfPoints(), 3);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[0].OutVal, FVector(100.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[1].OutVal, FVector(200.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.GetSplinePointsPosition().Points[2].OutVal, FVector(200.000000, -60.000002, 0.000000));

#else
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points.Num(), 3);
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[0].OutVal, FVector(100.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[1].OutVal, FVector(200.000000, 0.000000, 0.000000));
			HOUDINI_TEST_EQUAL(PCGSplineData->SplineStruct.SplineCurves.Position.Points[2].OutVal, FVector(200.000000, -60.000002, 0.000000));
#endif
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGParametersDefaultsCooked, "Houdini.UnitTests.PCG.Parameters.Defaults.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGParametersDefaultsCooked::RunTest(const FString& Parameters)
{
	// This test uses a simple HDA which reads its parameter and sets it back on the output. It tests whether the HDA can process
	// its default parameter.

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGTestParameters/PCGTestParametersDefaultLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
			if(!Context->Update())
				return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/ParametersOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString>& Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS for the results.
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				{
					UHoudiniPCGDataAttributeInt* TestOutput = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("test_output")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(TestOutput, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(TestOutput->Values.Num(), 1, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(TestOutput->Values[0], 3, return true);

				}
				{
					UHoudiniPCGDataAttributeInt* Attrs = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("unreal_pcg_params")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGParametersMultiparm, "Houdini.UnitTests.PCG.Parameters.Defaults.Multiparm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_PCGParametersMultiparm::RunTest(const FString& Parameters)
{
	// This test uses a simple HDA which reads its parameter and sets it back on the output. It tests whether the HDA can process
	// its default parameter.

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGTestParameters/PCGMultiTestParmsLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
		{
			if(!Context->Update())
				return false;

			FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/ParametersOutput");

			UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

			// We should have one output...
			HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 2, return true);

			UInstancedStaticMeshComponent* ISM = nullptr;

			for (int Index = 0; Index < 2; Index++)
			{
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[Index].Data.Get());

				if (!IsValid(ISM))
					ISM = Cast<UInstancedStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));

			}

			// The HDA creates 5 instances, jamming a 100.0 * instance number in the transform

			HOUDINI_TEST_NOT_NULL_ON_FAIL(ISM, return true);
			HOUDINI_TEST_EQUAL(ISM->GetNumInstances(), 5);
			for (int Index = 0; Index < 5; Index++)
			{
				FTransform Transform;
				ISM->GetInstanceTransform(Index, Transform);
				HOUDINI_TEST_EQUAL(Transform.GetLocation().X, (Index + 1) * 500.0);
			}

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGParametersSetCooked, "Houdini.UnitTests.PCG.Parameters.Set.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGParametersSetCooked::RunTest(const FString& Parameters)
{
	// This test uses a simple HDA which reads its parameter and sets it back on the output. It tests whether the HDA can process
	// a set parameter.

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGTestParameters/PCGTestParametersSetLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
			if(!Context->Update())
				return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/ParametersOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString>& Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS for the results.
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				{
					UHoudiniPCGDataAttributeInt* TestOutput = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("test_output")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(TestOutput, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(TestOutput->Values.Num(), 1, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(TestOutput->Values[0], 5, return true);

				}
				{
					UHoudiniPCGDataAttributeInt* Attrs = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("unreal_pcg_params")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGParametersOverrideCooked, "Houdini.UnitTests.PCG.Parameters.Overrides.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGParametersOverrideCooked::RunTest(const FString& Parameters)
{
	// This test uses a simple HDA which reads its parameter and sets it back on the output. It tests whether the HDA can process
	// a set parameter.

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGTestParameters/PCGTestParametersOverridesLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));


	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
		if(!Context->Update())
			return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/ParametersOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			TSet<FString>& Tags = TaggedData.Tags;

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS for the results.
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tags.Contains(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->SetFromPCGData(PCGParam);

				{
					UHoudiniPCGDataAttributeInt* TestOutput = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("test_output")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(TestOutput, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(TestOutput->Values.Num(), 1, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(TestOutput->Values[0], 9, return true);

				}
				{
					UHoudiniPCGDataAttributeInt* Attrs = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("unreal_pcg_params")));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Attrs, continue);
					HOUDINI_TEST_EQUAL(Attrs->Values.Num(), 1);
				}
			}
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_InputSetCooked, "Houdini.UnitTests.PCG.Inputs.Set.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_InputSetCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGTestInputs/PCGTestInputsSetLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
		{
			Context->CleanupAndGenerateAsync();
			return true;
		}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
		if(!Context->Update())
				return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/InputsOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

		// ... check the mesh's bounding box.
		FBox Box = StaticMesh->GetBoundingBox();

		HOUDINI_TEST_EQUAL(Box.Min.X, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Y, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Z, -50.0);
		HOUDINI_TEST_EQUAL(Box.Max.X, 250.0); // <- duplicated: 2 extra boxes, so add 2x100
		HOUDINI_TEST_EQUAL(Box.Max.Y, 50.0);
		HOUDINI_TEST_EQUAL(Box.Max.Z, 50.0);

		// ... check we have a mesh component
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_InputOverrideCooked, "Houdini.UnitTests.PCG.Inputs.Override.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_InputOverrideCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGTestInputs/PCGTestInputsOverrideLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
		if(!Context->Update())
			return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/InputsOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

		// ... check the mesh's bounding box.
		FBox Box = StaticMesh->GetBoundingBox();

		HOUDINI_TEST_EQUAL(Box.Min.X, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Y, -50.0);
		HOUDINI_TEST_EQUAL(Box.Min.Z, -50.0);
		HOUDINI_TEST_EQUAL(Box.Max.X, 450.0); // <- duplicated: 2 extra boxes, so add 2x100
		HOUDINI_TEST_EQUAL(Box.Max.Y, 50.0);
		HOUDINI_TEST_EQUAL(Box.Max.Z, 50.0);

		// ... check we have a mesh component
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_ForLoopsCooked, "Houdini.UnitTests.PCG.Inputs.ForLoops.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_ForLoopsCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGForLoops/PCGForLoopsLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
		if(!Context->Update())
			return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/TestForLoop");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have 5 outputs...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);

		// ... it should have data ...


		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());


		// ... check we have a mesh for each point
		for(int Index = 0; Index < 5; Index++)
		{

			UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object"), Index));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

			// ... check the mesh's bounding box.
			FBox Box = StaticMesh->GetBoundingBox();

			double expectedSize = 100.0 * static_cast<double>(Index + 1);

			double size = Box.Max.X - Box.Min.X;

			// Accurate to 1% as copy to points is not that accurate.
			HOUDINI_TEST_EQUAL(size, expectedSize, expectedSize * 0.01);

			// ... check we have a mesh component
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);
		}
			

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PDGCooked, "Houdini.UnitTests.PCG.PDG.Cooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PDGCooked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGPDG/PCGPDGTestLevel.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
		if(!Context->Update())
			return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/TestPDGOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have 5 outputs...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 6, return true);

		// ... it should have data ...

		// ... check we have a mesh for each point
		for(int Index = 0; Index < 6; Index++)
		{

			HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[Index].Data.Get(), return true);
			// ... which we'll now convert to an PCGDataObject so we can easily ready it...
			UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
			PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

			UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object"), 0));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

			// ... check we have a mesh component
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);
		}
		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PDGBaked, "Houdini.UnitTests.PCG.PDG.Baked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestPCG_PDGBaked::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	FString MapName(TEXT("/Game/TestHDAs/PCG/PCGPDG/PCGPDGTestLevelBaked.umap"));
	TSharedPtr<EHoudiniTestPCGContext> Context(new EHoudiniTestPCGContext());
	Context->LoadPCGTestMap(MapName);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Context->PCGComponent, return true);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Test 1: Load a cube, then use it to generate a new cube.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FFunctionLatentCommand([Context]
	{
		Context->CleanupAndGenerateAsync();
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context]()
	{
		if(!Context->Update())
			return false;

		FString OutputPath = TEXT("/Game/HoudiniEngine/Temp/TestPDGOutput");

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *OutputPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have 5 outputs...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 6, return true);

		// ... it should have data ...

		// ... check we have a mesh for each point
		for(int Index = 0; Index < 6; Index++)
		{

			HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[Index].Data.Get(), return true);
			// ... which we'll now convert to an PCGDataObject so we can easily ready it...
			UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
			PCGDataObject->SetFromPCGData(PCGDataAsset->Data.TaggedData[0].Data.Get());

			UStaticMesh* StaticMesh = Cast<UStaticMesh>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("object"), 0));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMesh, return true);

			// ... check we have a mesh component
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("component")));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StaticMeshComponent, return true);
		}
		return true;
	}));

	return true;
}
