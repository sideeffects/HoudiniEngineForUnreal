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


#include "HoudiniEditorTestMeshMisc.h"

#include "HoudiniApi.h"
#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "Misc/DefaultValueHelper.h"
#include "HoudiniEditorTests.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_SplineMeshes, "Houdini.UnitTests.Inputs.SplineMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_SplineMeshes::RunTest(const FString& Parameters)
{
	// Test we can input spline meshes correctly to Houdini From Unreal. We input two mesh components which share the same input
	// UStaticMesh to make sure the ref input system creates two different copies of the mesh.

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FString(TEXT("/Game/TestHDAs/Mesh/Misc/TestSplineMesh.umap"))));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);

			// We should have two outputs, two meshes
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
			TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 2, return true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FHoudiniBakeSettings BakeSettings;
			Context->Bake(BakeSettings);

			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
			// There should be two outputs as we have two meshes.
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 2, return true);

			// Go through each output and check we have two actors with one mesh component each.
			TArray<AActor*> Actors;
			for(auto& BakedOutput : BakedOutputs)
			{
				for(auto It : BakedOutput.BakedOutputObjects)
				{
					FHoudiniBakedOutputObject& OutputObject = It.Value;

					AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.ActorPath.ToString()));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

					TArray<UStaticMeshComponent*> Components;
					Actor->GetComponents(Components);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);

					Actors.Add(Actor);
				}
			}

			HOUDINI_TEST_EQUAL_ON_FAIL(Actors.Num(), 2, return false);

			TArray<FBoxSphereBounds> Bounds;
			for (auto Actor : Actors)
			{
				TArray<UStaticMeshComponent*> StaticMeshComponents;
				Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
				HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshComponents.Num(), 1, return false);
				UStaticMeshComponent* SMC = StaticMeshComponents[0];

				Bounds.Add(SMC->Bounds.GetBox());

			}

			Bounds.Sort([](const FBoxSphereBounds& A, const FBoxSphereBounds& B)
				{
					return A.GetBox().GetVolume() < B.GetBox().GetVolume();
				});

			// We have exported the cube as two spline meshes, they should be different sizes.

			HOUDINI_TEST_EQUAL(Bounds[0].Origin, FVector3d(250.0, 100.0, 0.0));
			HOUDINI_TEST_EQUAL(Bounds[0].BoxExtent, FVector3d(250.0, 50.0, 50.0));
			HOUDINI_TEST_EQUAL(Bounds[1].Origin, FVector3d(500.0, -100.0, 0.0));
			HOUDINI_TEST_EQUAL(Bounds[1].BoxExtent, FVector3d(500.0, 50.0, 50.0));

			return true;
		}));

	return true;
}


TArray<int> GetPrimitiveLOD(HAPI_NodeId NodeId)
{
	// Returns an array, one per primitive, that indicated LOD Index. 0 if not set.

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	HAPI_PartInfo PartInfo;
	FHoudiniApi::GetPartInfo(Session, NodeId, 0, &PartInfo);

	int NumPrims = PartInfo.faceCount;

	TArray<int> Results;
	Results.SetNum(NumPrims);
	for(int Index = 0; Index < NumPrims; Index++)
	{
		Results[Index] = -1;
	}

	TArray<int> Membership;
	Membership.SetNum(NumPrims);

	for (int LODIndex = 0; LODIndex < 2; LODIndex++)
	{
		FString LODName = FString::Printf(TEXT("lod%d"), LODIndex);

		HAPI_Bool AllEqual;

		HAPI_Result Result = FHoudiniApi::GetGroupMembership(Session, 
			NodeId, 0,
			HAPI_GROUPTYPE_PRIM, 
			H_TCHAR_TO_UTF8(*LODName),
			&AllEqual,
			Membership.GetData(), 
			0, NumPrims);

		if (Result != HAPI_RESULT_SUCCESS)
			continue;

		for(int Index = 0; Index < NumPrims; Index++)
		{
			if (Membership[Index] != 0)
				Results[Index] = LODIndex;
		}
	}
	return Results;
}


int GetPrimitiveCount(HAPI_NodeId NodeId)
{
	// Returns an array, one per primitive, that indicated LOD Index. 0 if not set.

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	HAPI_PartInfo PartInfo;
	FHoudiniApi::GetPartInfo(Session, NodeId, 0, &PartInfo);

	int NumPrims = PartInfo.faceCount;
	return NumPrims;
}

TArray<int> CountLODPrimitives(TArray<int> LODs)
{
	TArray<int> Results;

	for (int Index = 0; Index < LODs.Num(); Index++)
	{
		if (LODs[Index] == -1)
			continue;

		if (!Results.IsValidIndex(LODs[Index]))
		{
			Results.SetNum(LODs[Index] + 1);
		}
		Results[LODs[Index]]++;
	}
	return Results;
}

TArray<FString> GetCollisionGroups(HAPI_NodeId NodeId)
{
	TArray<FString> GroupNames;
	FHoudiniEngineUtils::HapiGetGroupNames(NodeId, 0, HAPI_GROUPTYPE_PRIM, false, GroupNames);

	TArray<FString> CollisionGroupNames;

	for (const FString GroupName : GroupNames)
	{
		if (GroupName.StartsWith(TEXT("collision")))
		{
			CollisionGroupNames.Add(GroupName);
		}
	}

	CollisionGroupNames.Sort();
	return CollisionGroupNames;
}

FString GetMaterial(HAPI_NodeId NodeId)
{

	TArray<FString> Data;
	FHoudiniHapiAccessor Accessor(NodeId, 0, "unreal_material");
	Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, Data, 0, 1);

	FString MaterialName = Data[0];
	int TrimCharacter = 0;
	if(MaterialName.FindChar(']', TrimCharacter))
	{
		MaterialName = MaterialName.Mid(TrimCharacter + 1).TrimStart();
	}
	return MaterialName;

}

FString GetMaterialForLOD(HAPI_NodeId NodeId, int LODIndex, const TArray<int>&  PrimitiveLODs)
{
	for (int PrimIndex = 0; PrimIndex < PrimitiveLODs.Num(); PrimIndex++)
	{
		if(LODIndex != PrimitiveLODs[PrimIndex])
			continue;

		TArray<FString> Data;
		FHoudiniHapiAccessor Accessor(NodeId, 0, "unreal_material");
		Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, Data, PrimIndex, 1);

		FString MaterialName = Data[0];
		int TrimCharacter = 0;
		if(MaterialName.FindChar(']', TrimCharacter))
		{
			MaterialName = MaterialName.Mid(TrimCharacter + 1).TrimStart();
		}

		return MaterialName;
	}

	return TEXT("");
}

static float InvalidParam = std::numeric_limits<float>::quiet_NaN();

float GetScalarParameterForLOD(HAPI_NodeId NodeId, const char* Name, int LODIndex, const TArray<int>& PrimitiveLODs)
{
	for(int PrimIndex = 0; PrimIndex < PrimitiveLODs.Num(); PrimIndex++)
	{
		if(LODIndex != PrimitiveLODs[PrimIndex])
			continue;

		TArray<float> Data;
		Data.SetNum(1);

		FHoudiniHapiAccessor Accessor(NodeId, 0, Name);
		Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, Data, PrimIndex, 1);

		return Data[0];
	}

	return InvalidParam;
}


float GetScalarParameter(HAPI_NodeId NodeId, const char* Name)
{
	TArray<float> Data;
	Data.SetNum(1);

	FHoudiniHapiAccessor Accessor(NodeId, 0, Name);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, Data, 0, 1);

	if (bSuccess)
		return Data[0];
	else
		return InvalidParam;
}

TArray<FString> GetMeshSockets(HAPI_NodeId NodeId)
{
	TArray<FString> Data;
	FHoudiniHapiAccessor Accessor(NodeId, 0, "mesh_socket_name");

	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Data);

	TSet<FString> Sockets;
	for (FString Attr : Data)
	{
		if(!Attr.IsEmpty() && !Sockets.Contains(Attr))
			Sockets.Add(Attr);
	}
	return Sockets.Array();

}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_Meshes, "Houdini.UnitTests.Inputs.Meshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_Meshes::RunTest(const FString& Parameters)
{
	// This test cooks the same HDA with different export options to verify the ability to export meshes
	// with different options, eg. lods, material parameters, collisions, sockets.

	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.

	TSharedPtr<FHoudiniMultiTestContext> Context(new FHoudiniMultiTestContext());

	TSharedPtr<FHoudiniTestContext> MeshMainContext(new FHoudiniTestContext(this, FString(TEXT("/Game/TestHDAs/Inputs/Meshes/TestInputMesh.umap")), TEXT("Mesh_Main")));
	HOUDINI_TEST_EQUAL_ON_FAIL(MeshMainContext->IsValid(), true, return false);
	MeshMainContext->SetProxyMeshEnabled(false);
	Context->Contexts.Add(MeshMainContext);

	TSharedPtr<FHoudiniTestContext> MainMeshLODs(new FHoudiniTestContext(this, MeshMainContext->GetWorld(), TEXT("Mesh_LODs")));
	HOUDINI_TEST_EQUAL_ON_FAIL(MainMeshLODs->IsValid(), true, return false);
	MainMeshLODs->SetProxyMeshEnabled(false);
	Context->Contexts.Add(MainMeshLODs);

	TSharedPtr<FHoudiniTestContext> MeshLODsCollidersSockets(new FHoudiniTestContext(this, MeshMainContext->GetWorld(), TEXT("Mesh_LODsCollidersSockets")));
	HOUDINI_TEST_EQUAL_ON_FAIL(MeshLODsCollidersSockets->IsValid(), true, return false);
	MeshLODsCollidersSockets->SetProxyMeshEnabled(false);
	Context->Contexts.Add(MeshLODsCollidersSockets);


	TSharedPtr<FHoudiniTestContext> MeshLODsMaterialParams(new FHoudiniTestContext(this, MeshMainContext->GetWorld(), TEXT("Mesh_LODsMaterialParams")));
	HOUDINI_TEST_EQUAL_ON_FAIL(MeshLODsMaterialParams->IsValid(), true, return false);
	MeshLODsMaterialParams->SetProxyMeshEnabled(false);
	Context->Contexts.Add(MeshLODsMaterialParams);

	AddCommand(new FHoudiniLatentTestCommand(MeshMainContext, [this, MeshMainContext ]()
		{
			MeshMainContext->StartCookingHDA();
			return true;
		}));
	

	AddCommand(new FHoudiniLatentTestCommand(MainMeshLODs, [this, MainMeshLODs]()
		{
			MainMeshLODs->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MeshLODsCollidersSockets, [this, MeshLODsCollidersSockets]()
		{
			MeshLODsCollidersSockets->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MeshLODsMaterialParams, [this, MeshLODsMaterialParams]()
		{
			MeshLODsMaterialParams->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, MeshMainContext, MainMeshLODs, MeshLODsCollidersSockets, MeshLODsMaterialParams]()
		{
			const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

			FString CubeMaterialName = TEXT("/Game/TestHDAs/Inputs/Meshes/CubeMaterial.CubeMaterial");
			FString SphereMaterialName = TEXT("/Game/TestHDAs/Inputs/Meshes/SphereMaterial.SphereMaterial");

			{
				HAPI_NodeId NodeId = MeshMainContext->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 1 LOD, 528 prims
				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 1, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);

				// Check MaterialName
				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

				// We should have no material parameters
				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				// No Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				// No Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = MainMeshLODs->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 2 LODs, 528 prims in the first one, 12 in the second one.
				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				// Check MaterialNames
				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				// We should have no material parameters
				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				// We should have no material parameters
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				// No Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				// No Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = MeshLODsCollidersSockets->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 2 LODs, 528 prims in the first one, 12 in the second one.
				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				// Check MaterialNames
				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				// We should have no material parameters
				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				// Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 2);

				// 2 Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 2);
			}

			{
				HAPI_NodeId NodeId = MeshLODsMaterialParams->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 2 LODs, 528 prims in the first one, 12 in the second one.
				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				// Check MaterialNames
				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				// We should have material parameters
				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, 0.5f);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, 1.0f);

				// No Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				// No Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_NaniteMeshes, "Houdini.UnitTests.Inputs.NaniteMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestInput_NaniteMeshes::RunTest(const FString& Parameters)
{
	// This test cooks the same HDA with different export options to verify the ability to export meshes
	// with different options, eg. lods, material parameters, collisions, sockets.

	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.

	TSharedPtr<FHoudiniMultiTestContext> Context(new FHoudiniMultiTestContext());

	TSharedPtr<FHoudiniTestContext> NaniteMesh(new FHoudiniTestContext(this, FString(TEXT("/Game/TestHDAs/Inputs/Meshes/TestInputNaniteMesh.umap")), TEXT("Nanite")));
	HOUDINI_TEST_EQUAL_ON_FAIL(NaniteMesh->IsValid(), true, return false);
	NaniteMesh->SetProxyMeshEnabled(false);
	Context->Contexts.Add(NaniteMesh);

	TSharedPtr<FHoudiniTestContext> NaniteMesh_Fallback(new FHoudiniTestContext(this, NaniteMesh->GetWorld(), TEXT("Nanite_Fallback")));
	HOUDINI_TEST_EQUAL_ON_FAIL(NaniteMesh_Fallback->IsValid(), true, return false);
	NaniteMesh_Fallback->SetProxyMeshEnabled(false);
	Context->Contexts.Add(NaniteMesh_Fallback);

	TSharedPtr<FHoudiniTestContext> NaniteMesh_MaterialParams(new FHoudiniTestContext(this, NaniteMesh->GetWorld(), TEXT("Nanite_MaterialParams")));
	HOUDINI_TEST_EQUAL_ON_FAIL(NaniteMesh_MaterialParams->IsValid(), true, return false);
	NaniteMesh_MaterialParams->SetProxyMeshEnabled(false);
	Context->Contexts.Add(NaniteMesh_MaterialParams);

	AddCommand(new FHoudiniLatentTestCommand(NaniteMesh, [this, NaniteMesh]()
		{
			NaniteMesh->StartCookingHDA();
			return true;
		}));


	AddCommand(new FHoudiniLatentTestCommand(NaniteMesh_Fallback, [this, NaniteMesh_Fallback]()
		{
			NaniteMesh_Fallback->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(NaniteMesh_MaterialParams, [this, NaniteMesh_MaterialParams]()
		{
			NaniteMesh_MaterialParams->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, NaniteMesh, NaniteMesh_Fallback, NaniteMesh_MaterialParams]()
		{
			const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

			FString CubeMaterialName = TEXT("/Game/TestHDAs/Inputs/Meshes/CubeMaterial.CubeMaterial");
			FString SphereMaterialName = TEXT("/Game/TestHDAs/Inputs/Meshes/SphereMaterial.SphereMaterial");

			{
				HAPI_NodeId NodeId = NaniteMesh->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 1 LOD, 755677 prims
				int NumPrimitives = GetPrimitiveCount(NodeId);
				HOUDINI_TEST_EQUAL(NumPrimitives > 0, true);

				// Check MaterialName
				FString Material0 = GetMaterial(NodeId);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

				// We should have no material parameters
				float ScalarParam0 = GetScalarParameter(NodeId, "unreal_material_parameter_0_SphereScalarParam");
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);

				// No Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				// No Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = NaniteMesh_Fallback->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 1 LOD, different number of prims depending on Unreal version.

				int PrimitiveCount = GetPrimitiveCount(NodeId);
				HOUDINI_TEST_EQUAL(PrimitiveCount > 0, true);

				// Check MaterialName
				FString Material0 = GetMaterial(NodeId);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

				// We should have no material parameters
				float ScalarParam0 = GetScalarParameter(NodeId, "unreal_material_parameter_0_SphereScalarParam");
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);

				// No Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				// No Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = NaniteMesh_MaterialParams->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, true);

				// We should have 1 LOD, 755677 prims
				int LODPrimitiveCount = GetPrimitiveCount(NodeId);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount, 755677);

				// Check MaterialName
				FString Material0 = GetMaterial(NodeId);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

				// We should have no material parameters
				float ScalarParam0 = GetScalarParameter(NodeId, "unreal_material_parameter_0_SphereScalarParam");
				HOUDINI_TEST_EQUAL(ScalarParam0, 0.5f);

				// No Collisions
				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				// No Sockets
				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			return true;
		}));

	return true;
}


