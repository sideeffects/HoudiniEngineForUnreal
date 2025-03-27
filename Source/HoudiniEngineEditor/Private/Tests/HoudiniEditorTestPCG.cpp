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
#include "Landscape.h"
#include "PCGParamData.h"

enum class EHoudiniTestPCGContextState : int
{
	None,
	Cleanup,
	Generate,
	Done
};

class EHoudiniTestPCGContext
{
public:
	void LoadPCGTestMap(const FString& MapName);
	void Generate(bool bCleanup, bool bGenerate);

	void OnGraphCleaned(UPCGComponent* PCGComponent_);
	void OnGraphGenerated(UPCGComponent* PCGComponent_);

	UPCGComponent* PCGComponent = nullptr;
	EHoudiniTestPCGContextState State = EHoudiniTestPCGContextState::None;

	bool bDoGenerate;
	bool bDoCleanup;
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
	if(this->bDoGenerate)
	{
		PCGComponent->GenerateLocal(true);
		this->State = EHoudiniTestPCGContextState::Generate;
	}
	else
	{
		this->State = EHoudiniTestPCGContextState::Done;
	}
}

void EHoudiniTestPCGContext::OnGraphGenerated(UPCGComponent* PCGComponent_)
{
	this->State = EHoudiniTestPCGContextState::Done;
}		

void EHoudiniTestPCGContext::Generate(bool bCleanup, bool bGenerate)
{
	bDoGenerate = bGenerate;
	bDoCleanup = bCleanup;

	if(bCleanup && PCGComponent->bGenerated)
	{
		this->State = EHoudiniTestPCGContextState::Cleanup;
		PCGComponent->Cleanup();
	}
	else if(bGenerate)
	{
		this->State = EHoudiniTestPCGContextState::Generate;
		PCGComponent->GenerateLocal(true);
	}
}

UObject* FHoudiniEditorTestPCG::GetOutputObject(UHoudiniPCGDataObject* PCGDataObject, const FString & Field)
{
	auto * Attr = Cast<UHoudiniPCGDataAttributeSoftObjectPath>(PCGDataObject->FindAttribute(Field));
	if(!IsValid(Attr) || Attr->Values.IsEmpty())
		return nullptr;

	const FString & ObjectPath = Attr->Values[0].ToString();

	UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	return Object;

}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_Meshes, "Houdini.UnitTests.PCG.Meshes",
                                         EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_Meshes::RunTest(const FString& Parameters)
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
		Context->Generate(true, true);
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(Context->State != EHoudiniTestPCGContextState::Done)
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->Initialize(PCGDataAsset->Data.TaggedData[0].Data.Get());

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
		if(Context->State != EHoudiniTestPCGContextState::Done)
			return false;

		Context->State = EHoudiniTestPCGContextState::Generate;
		GraphInstance->SetGraphParameter<float>(FName("scale_factor"), 2.0f);
		Context->Generate(true, true);
	
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(Context->State != EHoudiniTestPCGContextState::Done)
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>(GetTransientPackage());
		PCGDataObject->Initialize(PCGDataAsset->Data.TaggedData[0].Data.Get());

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


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_Landscapes, "Houdini.UnitTests.PCG.Landscapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_Landscapes::RunTest(const FString& Parameters)
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
		Context->Generate(true, true);
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(Context->State != EHoudiniTestPCGContextState::Done)
			return false;

		UPCGDataAsset* MyObject = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MyObject, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(MyObject->Data.TaggedData.Num(), 1, return true);
		// ... it should have data ...
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MyObject->Data.TaggedData[0].Data.Get(), return true);
		// ... which we'll now convert to an PCGDataObject so we can easily ready it...
		UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
		PCGDataObject->Initialize(MyObject->Data.TaggedData[0].Data.Get());

		// ... check we have a mesh
		ALandscape* Landscape = Cast<ALandscape>(FHoudiniEditorTestPCG::GetOutputObject(PCGDataObject, TEXT("actor")));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Landscape, return true);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGNativeOutputs, "Houdini.UnitTests.PCG.PCGOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGNativeOutputs::RunTest(const FString& Parameters)
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
		Context->Generate(true, true);
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(Context->State != EHoudiniTestPCGContextState::Done)
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for (int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			HOUDINI_TEST_EQUAL_ON_FAIL(TaggedData.Tags.Num(), 1, continue);
			FString Tag = TaggedData.Tags.Array()[0];

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// CHECK POINTS OUTPUT
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if (Tag.EndsWith(TEXT("Points")))
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
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK VERTICES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Vertices")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->Initialize(PCGParam);
				auto * VertexIds = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("__vertex_id")));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(VertexIds, continue);

				// 3 vertices per triangle, 2 triangles per face = 6 * 2 * 3
				HOUDINI_TEST_EQUAL(VertexIds->Values.Num(), 36);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK PRIMITIVES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Primitives")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->Initialize(PCGParam);

				auto* PrimitiveIds = Cast<UHoudiniPCGDataAttributeInt>(PCGDataObject->FindAttribute(TEXT("__primitive_id")));
				HOUDINI_TEST_NOT_NULL_ON_FAIL(PrimitiveIds, continue);

				// 3 vertices per triangle, 2 triangles per face 
				HOUDINI_TEST_EQUAL(PrimitiveIds->Values.Num(), 12);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PCGDataObject = NewObject<UHoudiniPCGDataObject>();
				PCGDataObject->Initialize(PCGParam);

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

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestPCG_PCGNativeInputs, "Houdini.UnitTests.PCG.PCGInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestPCG_PCGNativeInputs::RunTest(const FString& Parameters)
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
		Context->Generate(true, true);
		return true;
	}));

	AddCommand(new FFunctionLatentCommand([this, Context, PCGAssetFullPath]()
	{
		if(Context->State != EHoudiniTestPCGContextState::Done)
			return false;

		UPCGDataAsset* PCGDataAsset = Cast<UPCGDataAsset>(StaticLoadObject(UPCGDataAsset::StaticClass(), nullptr, *PCGAssetFullPath));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(PCGDataAsset, return true);

		// We should have one output...
		HOUDINI_TEST_EQUAL_ON_FAIL(PCGDataAsset->Data.TaggedData.Num(), 4, return true);

		for(int TagIndex = 0; TagIndex < PCGDataAsset->Data.TaggedData.Num(); TagIndex++)
		{
			auto& TaggedData = PCGDataAsset->Data.TaggedData[TagIndex];
			HOUDINI_TEST_EQUAL_ON_FAIL(TaggedData.Tags.Num(), 1, continue);
			FString Tag = TaggedData.Tags.Array()[0];

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// CHECK POINTS OUTPUT
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Points")))
			{
				const UPCGPointData* PCGPointData = Cast<UPCGPointData>(TaggedData.Data.Get());
				HOUDINI_TEST_EQUAL(PCGPointData->GetNumPoints(), 148);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK VERTICES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Vertices")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* VerticesObject = NewObject<UHoudiniPCGDataObject>();
				VerticesObject->Initialize(PCGParam);
				HOUDINI_TEST_EQUAL(VerticesObject->Attributes.Num(), 0);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK PRIMITIVES
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Primitives")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* PrimitivesObject = NewObject<UHoudiniPCGDataObject>();
				PrimitivesObject->Initialize(PCGParam);
				HOUDINI_TEST_EQUAL(PrimitivesObject->Attributes.Num(), 0);
			}

			///////////////////////////////////////////////////////////////////////////////////////////////////////
			/// CHECK DETAILS
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			if(Tag.EndsWith(TEXT("Details")))
			{
				const UPCGParamData* PCGParam = Cast<UPCGParamData>(TaggedData.Data.Get());
				UHoudiniPCGDataObject* DetailsObject = NewObject<UHoudiniPCGDataObject>();
				DetailsObject->Initialize(PCGParam);
				// topology, primitive list and unreal_pcg_params
				HOUDINI_TEST_EQUAL(DetailsObject->Attributes.Num(), 3);
			}
		}

		return true;
	}));

	return true;
}