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
#include "HoudiniInput.h"
#include "HoudiniInputObject.h"
#include "HoudiniInputTranslator.h"
#include "HoudiniInputTypes.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "Misc/DefaultValueHelper.h"
#include "HoudiniEditorTests.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "HoudiniAsset.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "StaticMeshAttributes.h"
#include "UDynamicMesh.h"

TArray<FString> GetCollisionGroups(HAPI_NodeId NodeId);

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DynamicMesh, "Houdini.UnitTests.Inputs.DynamicMesh.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DynamicMesh::RunTest(const FString& Parameters)
{
	bool bSessionCreated = FHoudiniEditorTestUtils::CreateSessionIfInvalid(FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName);
	HOUDINI_TEST_EQUAL_ON_FAIL(bSessionCreated, true, return false);

	UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	DynamicMesh->EditMesh([](UE::Geometry::FDynamicMesh3& Mesh)
	{
		Mesh.AppendVertex(FVector3d(100.0, 200.0, 300.0));
		Mesh.AppendVertex(FVector3d(200.0, 200.0, 300.0));
		Mesh.AppendVertex(FVector3d(100.0, 400.0, 300.0));
		Mesh.AppendTriangle(0, 1, 2);
		Mesh.EnableAttributes();
		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();
		NormalOverlay->SetTriangle(0, UE::Geometry::FIndex3i(
			NormalOverlay->AppendElement(FVector3f(0.0f, 0.0f, 1.0f)),
			NormalOverlay->AppendElement(FVector3f(0.0f, 0.0f, 1.0f)),
			NormalOverlay->AppendElement(FVector3f(0.0f, 0.0f, 1.0f))));
		Mesh.Attributes()->EnableTangents();
		UE::Geometry::FDynamicMeshNormalOverlay* TangentOverlay = Mesh.Attributes()->PrimaryTangents();
		UE::Geometry::FDynamicMeshNormalOverlay* BitangentOverlay = Mesh.Attributes()->PrimaryBiTangents();
		UE::Geometry::FIndex3i TangentTriangle(
			TangentOverlay->AppendElement(FVector3f(1.0f, 0.0f, 0.0f)),
			TangentOverlay->AppendElement(FVector3f(1.0f, 0.0f, 0.0f)),
			TangentOverlay->AppendElement(FVector3f(1.0f, 0.0f, 0.0f)));
		TangentOverlay->SetTriangle(0, TangentTriangle);
		BitangentOverlay->SetTriangle(0, UE::Geometry::FIndex3i(
			BitangentOverlay->AppendElement(FVector3f(0.0f, 1.0f, 0.0f)),
			BitangentOverlay->AppendElement(FVector3f(0.0f, 1.0f, 0.0f)),
			BitangentOverlay->AppendElement(FVector3f(0.0f, 1.0f, 0.0f))));
	});

	UDynamicMesh* ComponentDynamicMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	ComponentDynamicMesh->EditMesh([](UE::Geometry::FDynamicMesh3& Mesh)
	{
		Mesh.AppendVertex(FVector3d::Zero());
		Mesh.AppendVertex(FVector3d(100.0, 0.0, 0.0));
		Mesh.AppendVertex(FVector3d(100.0, 100.0, 0.0));
		Mesh.AppendVertex(FVector3d(0.0, 100.0, 0.0));
		Mesh.AppendTriangle(0, 1, 2);
		Mesh.AppendTriangle(0, 2, 3);
		Mesh.EnableAttributes();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
		UVOverlay->SetTriangle(0, UE::Geometry::FIndex3i(
			UVOverlay->AppendElement(FVector2f(0.25f, 0.25f)),
			UVOverlay->AppendElement(FVector2f(0.75f, 0.25f)),
			UVOverlay->AppendElement(FVector2f(0.75f, 0.75f))));
		UVOverlay->SetTriangle(1, UE::Geometry::FIndex3i(
			UVOverlay->AppendElement(FVector2f(0.25f, 0.25f)),
			UVOverlay->AppendElement(FVector2f(0.75f, 0.75f)),
			UVOverlay->AppendElement(FVector2f(0.25f, 0.75f))));
	});

	UDynamicMeshComponent* DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetTransientPackage());
	DynamicMeshComponent->SetDynamicMesh(ComponentDynamicMesh);

	FKAggregateGeom SimpleColliders;
	FKBoxElem& BoxCollider = SimpleColliders.BoxElems.AddDefaulted_GetRef();
	BoxCollider.Center = FVector(100.0, 200.0, 300.0);
	BoxCollider.X = 50.0f;
	BoxCollider.Y = 100.0f;
	BoxCollider.Z = 150.0f;

	FKSphereElem& SphereCollider = SimpleColliders.SphereElems.AddDefaulted_GetRef();
	SphereCollider.Center = FVector(200.0, 300.0, 400.0);
	SphereCollider.Radius = 75.0f;

	FKSphylElem& CapsuleCollider = SimpleColliders.SphylElems.AddDefaulted_GetRef();
	CapsuleCollider.Center = FVector(300.0, 400.0, 500.0);
	CapsuleCollider.Rotation = FRotator(0.0, 45.0, 0.0);
	CapsuleCollider.Radius = 25.0f;
	CapsuleCollider.Length = 200.0f;

	FKConvexElem& ConvexCollider = SimpleColliders.ConvexElems.AddDefaulted_GetRef();
	ConvexCollider.VertexData = {
		FVector(0.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0), FVector(0.0, 100.0, 0.0), FVector(0.0, 0.0, 100.0) };
	ConvexCollider.IndexData = { 0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3 };
	DynamicMeshComponent->SetSimpleCollisionShapes(SimpleColliders, false);

	FHoudiniInputObjectSettings InputSettings;
	InputSettings.bExportColliders = true;
	UHoudiniInputDynamicMesh* DynamicMeshInput = CastChecked<UHoudiniInputDynamicMesh>(
		UHoudiniInputObject::CreateTypedInputObject(DynamicMesh, GetTransientPackage(), TEXT("DynamicMesh"), InputSettings));
	UHoudiniInputDynamicMeshComponent* DynamicMeshComponentInput = CastChecked<UHoudiniInputDynamicMeshComponent>(
		UHoudiniInputObject::CreateTypedInputObject(DynamicMeshComponent, GetTransientPackage(), TEXT("DynamicMeshComponent"), InputSettings));

	bool bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForDynamicMesh(
		TEXT("DynamicMesh"), DynamicMeshInput, InputSettings, true);
	HOUDINI_TEST_EQUAL_ON_FAIL(bSuccess, true, return false);

	bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForDynamicMeshComponent(
		TEXT("DynamicMeshComponent"), DynamicMeshComponentInput, InputSettings, true);
	HOUDINI_TEST_EQUAL_ON_FAIL(bSuccess, true, return false);

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();
	HAPI_NodeId DynamicMeshNodeId = DynamicMeshInput->GetInputNodeId();
	HAPI_NodeId DynamicMeshComponentNodeId = DynamicMeshComponentInput->GetInputNodeId();
	HOUDINI_TEST_EQUAL_ON_FAIL(DynamicMeshNodeId >= 0, true, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(DynamicMeshComponentNodeId >= 0, true, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(DynamicMeshNodeId == DynamicMeshComponentNodeId, false, return false);

	HAPI_PartInfo DynamicMeshPartInfo;
	FHoudiniApi::PartInfo_Init(&DynamicMeshPartInfo);
	HAPI_Result Result = FHoudiniApi::GetPartInfo(Session, DynamicMeshNodeId, 0, &DynamicMeshPartInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(DynamicMeshPartInfo.type, HAPI_PARTTYPE_MESH);
	HOUDINI_TEST_EQUAL(DynamicMeshPartInfo.pointCount, 3);
	HOUDINI_TEST_EQUAL(DynamicMeshPartInfo.vertexCount, 3);
	HOUDINI_TEST_EQUAL(DynamicMeshPartInfo.faceCount, 1);

	HAPI_PartInfo DynamicMeshComponentPartInfo;
	FHoudiniApi::PartInfo_Init(&DynamicMeshComponentPartInfo);
	Result = FHoudiniApi::GetPartInfo(Session, DynamicMeshComponentNodeId, 0, &DynamicMeshComponentPartInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(DynamicMeshComponentPartInfo.type, HAPI_PARTTYPE_MESH);
	HOUDINI_TEST_EQUAL(DynamicMeshComponentPartInfo.pointCount > 4, true);
	HOUDINI_TEST_EQUAL(DynamicMeshComponentPartInfo.vertexCount > 6, true);
	HOUDINI_TEST_EQUAL(DynamicMeshComponentPartInfo.faceCount > 2, true);

	HAPI_AttributeInfo ComponentUVInfo;
	FHoudiniApi::AttributeInfo_Init(&ComponentUVInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, DynamicMeshComponentNodeId, 0, HAPI_UNREAL_ATTRIB_UV, HAPI_ATTROWNER_VERTEX, &ComponentUVInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(ComponentUVInfo.exists, true, return false);
	HOUDINI_TEST_EQUAL(ComponentUVInfo.tupleSize, 3);
	HOUDINI_TEST_EQUAL(ComponentUVInfo.count, DynamicMeshComponentPartInfo.vertexCount);

	TArray<float> ComponentUVs;
	ComponentUVs.SetNumUninitialized(ComponentUVInfo.count * ComponentUVInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, DynamicMeshComponentNodeId, 0, HAPI_UNREAL_ATTRIB_UV, &ComponentUVInfo, -1, ComponentUVs.GetData(), 0, ComponentUVInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	bool bFoundRenderUV = false;
	for (float UVValue : ComponentUVs)
	{
		if (FMath::IsNearlyEqual(UVValue, 0.25f) || FMath::IsNearlyEqual(UVValue, 0.75f))
		{
			bFoundRenderUV = true;
			break;
		}
	}
	HOUDINI_TEST_EQUAL(bFoundRenderUV, true);

	TArray<FString> CollisionGroups = GetCollisionGroups(DynamicMeshComponentNodeId);
	HOUDINI_TEST_EQUAL_ON_FAIL(CollisionGroups.Num(), 4, return false);
	HOUDINI_TEST_EQUAL(CollisionGroups[0], TEXT("collision_geo_simple_box0"));
	HOUDINI_TEST_EQUAL(CollisionGroups[1], TEXT("collision_geo_simple_capsule2"));
	HOUDINI_TEST_EQUAL(CollisionGroups[2], TEXT("collision_geo_simple_sphere1"));
	HOUDINI_TEST_EQUAL(CollisionGroups[3], TEXT("collision_geo_simple_ucx3"));

	SimpleColliders.BoxElems[0].X = 75.0f;
	DynamicMeshComponent->SetSimpleCollisionShapes(SimpleColliders, false);
	HOUDINI_TEST_EQUAL(DynamicMeshComponentInput->HasComponentChanged(InputSettings), true);

	HAPI_AttributeInfo PositionInfo;
	FHoudiniApi::AttributeInfo_Init(&PositionInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, HAPI_ATTROWNER_POINT, &PositionInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(PositionInfo.exists, true, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(PositionInfo.tupleSize, 3, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(PositionInfo.count, 3, return false);

	TArray<float> Positions;
	Positions.SetNumUninitialized(PositionInfo.count * PositionInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &PositionInfo, -1, Positions.GetData(), 0, PositionInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(Positions[0], 1.0f);
	HOUDINI_TEST_EQUAL(Positions[1], 3.0f);
	HOUDINI_TEST_EQUAL(Positions[2], 2.0f);

	HAPI_AttributeInfo NormalInfo;
	FHoudiniApi::AttributeInfo_Init(&NormalInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, HAPI_ATTROWNER_VERTEX, &NormalInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(NormalInfo.exists, true, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(NormalInfo.tupleSize, 3, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(NormalInfo.count, 3, return false);

	TArray<float> Normals;
	Normals.SetNumUninitialized(NormalInfo.count * NormalInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &NormalInfo, -1, Normals.GetData(), 0, NormalInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(Normals[0], 0.00f);
	HOUDINI_TEST_EQUAL(Normals[1], 1.00f);
	HOUDINI_TEST_EQUAL(Normals[2], 0.00f);
	// Houdini uses reverse winding after the Unreal-to-Houdini axis conversion.
	HOUDINI_TEST_EQUAL(Normals[3], 0.00f);
	HOUDINI_TEST_EQUAL(Normals[4], 1.00f);
	HOUDINI_TEST_EQUAL(Normals[5], 0.00f);
	HOUDINI_TEST_EQUAL(Normals[6], 0.00f);
	HOUDINI_TEST_EQUAL(Normals[7], 1.00f);
	HOUDINI_TEST_EQUAL(Normals[8], 0.00f);

	HAPI_AttributeInfo TangentInfo;
	FHoudiniApi::AttributeInfo_Init(&TangentInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, HAPI_ATTROWNER_VERTEX, &TangentInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(TangentInfo.exists, true, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(TangentInfo.tupleSize, 3, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(TangentInfo.count, 3, return false);

	TArray<float> Tangents;
	Tangents.SetNumUninitialized(TangentInfo.count * TangentInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, &TangentInfo, -1, Tangents.GetData(), 0, TangentInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(Tangents[0], 1.00f);
	HOUDINI_TEST_EQUAL(Tangents[1], 0.00f);
	HOUDINI_TEST_EQUAL(Tangents[2], 0.00f);

	HAPI_AttributeInfo BitangentInfo;
	FHoudiniApi::AttributeInfo_Init(&BitangentInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, HAPI_ATTROWNER_VERTEX, &BitangentInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(BitangentInfo.exists, true, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(BitangentInfo.tupleSize, 3, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(BitangentInfo.count, 3, return false);

	TArray<float> Bitangents;
	Bitangents.SetNumUninitialized(BitangentInfo.count * BitangentInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, DynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, &BitangentInfo, -1, Bitangents.GetData(), 0, BitangentInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(Bitangents[0], 0.00f);
	HOUDINI_TEST_EQUAL(Bitangents[1], 0.00f);
	HOUDINI_TEST_EQUAL(Bitangents[2], 1.00f);

	UDynamicMesh* ZeroTangentDynamicMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	ZeroTangentDynamicMesh->EditMesh([](UE::Geometry::FDynamicMesh3& Mesh)
	{
		Mesh.AppendVertex(FVector3d(100.0, 200.0, 300.0));
		Mesh.AppendVertex(FVector3d(200.0, 200.0, 300.0));
		Mesh.AppendVertex(FVector3d(100.0, 400.0, 300.0));
		Mesh.AppendTriangle(0, 1, 2);
		Mesh.EnableAttributes();
		UE::Geometry::FDynamicMeshNormalOverlay* ZeroNormalOverlay = Mesh.Attributes()->PrimaryNormals();
		ZeroNormalOverlay->SetTriangle(0, UE::Geometry::FIndex3i(
			ZeroNormalOverlay->AppendElement(FVector3f::ZeroVector),
			ZeroNormalOverlay->AppendElement(FVector3f::ZeroVector),
			ZeroNormalOverlay->AppendElement(FVector3f::ZeroVector)));
		Mesh.Attributes()->EnableTangents();
		UE::Geometry::FDynamicMeshNormalOverlay* ZeroTangentOverlay = Mesh.Attributes()->PrimaryTangents();
		UE::Geometry::FDynamicMeshNormalOverlay* ZeroBitangentOverlay = Mesh.Attributes()->PrimaryBiTangents();
		UE::Geometry::FIndex3i ZeroTangentTriangle(
			ZeroTangentOverlay->AppendElement(FVector3f::ZeroVector),
			ZeroTangentOverlay->AppendElement(FVector3f::ZeroVector),
			ZeroTangentOverlay->AppendElement(FVector3f::ZeroVector));
		ZeroTangentOverlay->SetTriangle(0, ZeroTangentTriangle);
		ZeroBitangentOverlay->SetTriangle(0, UE::Geometry::FIndex3i(
			ZeroBitangentOverlay->AppendElement(FVector3f::ZeroVector),
			ZeroBitangentOverlay->AppendElement(FVector3f::ZeroVector),
			ZeroBitangentOverlay->AppendElement(FVector3f::ZeroVector)));
	});

	UHoudiniInputDynamicMesh* ZeroTangentDynamicMeshInput = CastChecked<UHoudiniInputDynamicMesh>(
		UHoudiniInputObject::CreateTypedInputObject(ZeroTangentDynamicMesh, GetTransientPackage(), TEXT("ZeroTangentDynamicMesh"), InputSettings));
	bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForDynamicMesh(
		TEXT("ZeroTangentDynamicMesh"), ZeroTangentDynamicMeshInput, InputSettings, true);
	HOUDINI_TEST_EQUAL_ON_FAIL(bSuccess, true, return false);

	HAPI_NodeId ZeroTangentDynamicMeshNodeId = ZeroTangentDynamicMeshInput->GetInputNodeId();
	HOUDINI_TEST_EQUAL_ON_FAIL(ZeroTangentDynamicMeshNodeId >= 0, true, return false);
	HAPI_AttributeInfo RebuiltNormalInfo;
	FHoudiniApi::AttributeInfo_Init(&RebuiltNormalInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, ZeroTangentDynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, HAPI_ATTROWNER_VERTEX, &RebuiltNormalInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(RebuiltNormalInfo.exists, true, return false);

	TArray<float> RebuiltNormals;
	RebuiltNormals.SetNumUninitialized(RebuiltNormalInfo.count * RebuiltNormalInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, ZeroTangentDynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &RebuiltNormalInfo, -1, RebuiltNormals.GetData(), 0, RebuiltNormalInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(
		FMath::IsNearlyZero(RebuiltNormals[0]) && FMath::IsNearlyZero(RebuiltNormals[1]) && FMath::IsNearlyZero(RebuiltNormals[2]),
		false);

	HAPI_AttributeInfo RebuiltTangentInfo;
	FHoudiniApi::AttributeInfo_Init(&RebuiltTangentInfo);
	Result = FHoudiniApi::GetAttributeInfo(
		Session, ZeroTangentDynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, HAPI_ATTROWNER_VERTEX, &RebuiltTangentInfo);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL_ON_FAIL(RebuiltTangentInfo.exists, true, return false);

	TArray<float> RebuiltTangents;
	RebuiltTangents.SetNumUninitialized(RebuiltTangentInfo.count * RebuiltTangentInfo.tupleSize);
	Result = FHoudiniApi::GetAttributeFloatData(
		Session, ZeroTangentDynamicMeshNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, &RebuiltTangentInfo, -1, RebuiltTangents.GetData(), 0, RebuiltTangentInfo.count);
	HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return false);
	HOUDINI_TEST_EQUAL(
		FMath::IsNearlyZero(RebuiltTangents[0]) && FMath::IsNearlyZero(RebuiltTangents[1]) && FMath::IsNearlyZero(RebuiltTangents[2]),
		false);

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_StaticMeshRoundTrip, "Houdini.UnitTests.Inputs.StaticMesh.RoundTripColliders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_StaticMeshRoundTrip::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UStaticMesh* SourceMesh = Cast<UStaticMesh>(StaticLoadObject(
		UStaticMesh::StaticClass(), nullptr, TEXT("/Game/TestObjects/SM_Cube.SM_Cube")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceMesh, return false);
	UBodySetup* SourceBodySetup = SourceMesh->GetBodySetup();
	HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceBodySetup, return false);
	const FKAggregateGeom& SourceCollision = SourceBodySetup->AggGeom;
	HOUDINI_TEST_EQUAL_ON_FAIL(SourceCollision.BoxElems.Num() > 0, true, return false);

	FMeshDescription* SourceMeshDescription = SourceMesh->GetMeshDescription(0);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceMeshDescription, return false);
	FStaticMeshAttributes SourceAttributes(*SourceMeshDescription);
	TVertexInstanceAttributesRef<FVector2f> SourceTextureCoordinates = SourceAttributes.GetVertexInstanceUVs();
	HOUDINI_TEST_EQUAL_ON_FAIL(SourceTextureCoordinates.GetNumChannels() > 0, true, return false);

	TArray<FVector2f> SourceUVs;
	TArray<FString> SourceUVPairs;
	TVertexAttributesRef<FVector3f> SourceVertexPositions = SourceAttributes.GetVertexPositions();
	SourceUVs.Reserve(SourceMeshDescription->VertexInstances().Num());
	SourceUVPairs.Reserve(SourceMeshDescription->VertexInstances().Num());
	for (FVertexInstanceID VertexInstanceID : SourceMeshDescription->VertexInstances().GetElementIDs())
	{
		SourceUVs.Add(SourceTextureCoordinates.Get(VertexInstanceID, 0));
		FVector3f Position = SourceVertexPositions.Get(SourceMeshDescription->GetVertexInstanceVertex(VertexInstanceID));
		FVector2f UV = SourceTextureCoordinates.Get(VertexInstanceID, 0);
		SourceUVPairs.Add(FString::Printf(TEXT("%.0f,%.0f,%.0f:%.6f,%.6f"), Position.X, Position.Y, Position.Z, UV.X, UV.Y));
	}
	SourceUVs.Sort([](FVector2f Left, FVector2f Right) { return Left.X == Right.X ? Left.Y < Right.Y : Left.X < Right.X; });
	SourceUVPairs.Sort();

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SourceMesh]()
	{
		UHoudiniInput* Input = Context->GetInputAt(0);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

		Input->SetExportColliders(true);
		Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, SourceMesh);
		Input->MarkChanged(true);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SourceCollision, SourceUVs, SourceUVPairs]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);
		TArray<UStaticMesh*> StaticMeshes = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UStaticMesh>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshes.Num(), 1, return true);

		UBodySetup* OutputBodySetup = StaticMeshes[0]->GetBodySetup();
		HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputBodySetup, return true);
		const FKAggregateGeom& OutputCollision = OutputBodySetup->AggGeom;
		HOUDINI_TEST_EQUAL(OutputCollision.BoxElems.Num(), SourceCollision.BoxElems.Num());
		HOUDINI_TEST_EQUAL(OutputCollision.SphereElems.Num(), SourceCollision.SphereElems.Num());
		HOUDINI_TEST_EQUAL(OutputCollision.SphylElems.Num(), SourceCollision.SphylElems.Num());
		HOUDINI_TEST_EQUAL(OutputCollision.ConvexElems.Num(), SourceCollision.ConvexElems.Num());

		FMeshDescription* OutputMeshDescription = StaticMeshes[0]->GetMeshDescription(0);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputMeshDescription, return true);
		FStaticMeshAttributes OutputAttributes(*OutputMeshDescription);
		TVertexInstanceAttributesRef<FVector2f> OutputTextureCoordinates = OutputAttributes.GetVertexInstanceUVs();
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputTextureCoordinates.GetNumChannels() > 0, true, return true);

		TArray<FVector2f> OutputUVs;
		TArray<FString> OutputUVPairs;
		TVertexAttributesRef<FVector3f> OutputVertexPositions = OutputAttributes.GetVertexPositions();
		OutputUVs.Reserve(OutputMeshDescription->VertexInstances().Num());
		OutputUVPairs.Reserve(OutputMeshDescription->VertexInstances().Num());
		for (FVertexInstanceID VertexInstanceID : OutputMeshDescription->VertexInstances().GetElementIDs())
		{
			OutputUVs.Add(OutputTextureCoordinates.Get(VertexInstanceID, 0));
			FVector3f Position = OutputVertexPositions.Get(OutputMeshDescription->GetVertexInstanceVertex(VertexInstanceID));
			FVector2f UV = OutputTextureCoordinates.Get(VertexInstanceID, 0);
			OutputUVPairs.Add(FString::Printf(TEXT("%.0f,%.0f,%.0f:%.6f,%.6f"), Position.X, Position.Y, Position.Z, UV.X, UV.Y));
		}
		OutputUVs.Sort([](FVector2f Left, FVector2f Right) { return Left.X == Right.X ? Left.Y < Right.Y : Left.X < Right.X; });
		OutputUVPairs.Sort();

		HOUDINI_TEST_EQUAL(OutputUVs, SourceUVs);
		for (int32 PairIndex = 0; PairIndex < FMath::Min(OutputUVPairs.Num(), SourceUVPairs.Num()); PairIndex++)
		{
			if (OutputUVPairs[PairIndex] != SourceUVPairs[PairIndex])
			{
				AddInfo(FString::Printf(TEXT("UV pair mismatch: expected %s, got %s"), *SourceUVPairs[PairIndex], *OutputUVPairs[PairIndex]));
				break;
			}
		}
		HOUDINI_TEST_EQUAL(OutputUVPairs, SourceUVPairs);

		UHoudiniPublicAPIAssetWrapper* Wrapper = UHoudiniPublicAPIAssetWrapper::CreateWrapper(GetTransientPackage(), Context->HAC);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Wrapper, return true);

		TArray<FHoudiniPublicAPIOutputObjectIdentifier> OutputIdentifiers;
		bool bSuccess = Wrapper->GetOutputIdentifiersAt(0, OutputIdentifiers);
		HOUDINI_TEST_EQUAL_ON_FAIL(bSuccess, true, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputIdentifiers.Num(), 1, return true);

		UStaticMesh* PublicOutputMesh = Cast<UStaticMesh>(Wrapper->GetOutputObjectAt(0, OutputIdentifiers[0]));
		HOUDINI_TEST_EQUAL(PublicOutputMesh, StaticMeshes[0]);
		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DynamicMeshRoundTrip, "Houdini.UnitTests.Inputs.DynamicMesh.RoundTripColliders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DynamicMeshRoundTrip::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	DynamicMesh->AddToRoot();
	DynamicMesh->EditMesh([](UE::Geometry::FDynamicMesh3& Mesh)
	{
		Mesh.AppendVertex(FVector3d::Zero());
		Mesh.AppendVertex(FVector3d(100.0, 0.0, 0.0));
		Mesh.AppendVertex(FVector3d(0.0, 100.0, 0.0));
		Mesh.AppendTriangle(0, 1, 2);
		Mesh.EnableAttributes();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
		UVOverlay->SetTriangle(0, UE::Geometry::FIndex3i(
			UVOverlay->AppendElement(FVector2f(0.125f, 0.25f)),
			UVOverlay->AppendElement(FVector2f(0.375f, 0.50f)),
			UVOverlay->AppendElement(FVector2f(0.625f, 0.75f))));
	});

	UDynamicMeshComponent* DynamicMeshComponent = NewObject<UDynamicMeshComponent>(GetTransientPackage());
	DynamicMeshComponent->AddToRoot();
	DynamicMeshComponent->SetDynamicMesh(DynamicMesh);

	FKAggregateGeom Colliders;
	FKBoxElem& BoxCollider = Colliders.BoxElems.AddDefaulted_GetRef();
	BoxCollider.X = 50.0f;
	BoxCollider.Y = 50.0f;
	BoxCollider.Z = 50.0f;
	DynamicMeshComponent->SetSimpleCollisionShapes(Colliders, false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DynamicMeshComponent]()
	{
		UHoudiniInput* Input = Context->GetInputAt(0);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

		Input->SetExportColliders(true);
		Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DynamicMeshComponent);
		Input->MarkChanged(true);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DynamicMesh, DynamicMeshComponent]()
	{
		TArray<HAPI_NodeId> OutputNodes;
		bool bSuccess = FHoudiniEngineUtils::GatherAllAssetOutputs(Context->HAC->GetAssetId(), true, false, false, OutputNodes);
		HOUDINI_TEST_EQUAL_ON_FAIL(bSuccess, true, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

		HAPI_PartInfo PartInfo;
		FHoudiniApi::PartInfo_Init(&PartInfo);
		HAPI_Result Result = FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), OutputNodes[0], 0, &PartInfo);
		HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return true);

		HAPI_AttributeInfo UVInfo;
		FHoudiniApi::AttributeInfo_Init(&UVInfo);
		Result = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(), OutputNodes[0], 0, HAPI_UNREAL_ATTRIB_UV, HAPI_ATTROWNER_VERTEX, &UVInfo);
		HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(UVInfo.exists, true, return true);
		HOUDINI_TEST_EQUAL(UVInfo.tupleSize, 3);
		HOUDINI_TEST_EQUAL(UVInfo.count, PartInfo.vertexCount);

		TArray<float> UVs;
		UVs.SetNumUninitialized(UVInfo.count * UVInfo.tupleSize);
		Result = FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(), OutputNodes[0], 0, HAPI_UNREAL_ATTRIB_UV, &UVInfo, -1, UVs.GetData(), 0, UVInfo.count);
		HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return true);

		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);
		TArray<UStaticMesh*> StaticMeshes = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UStaticMesh>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshes.Num(), 1, return true);

		FMeshDescription* MeshDescription = StaticMeshes[0]->GetMeshDescription(0);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MeshDescription, return true);
		FStaticMeshAttributes Attributes(*MeshDescription);
		TVertexInstanceAttributesRef<FVector2f> TextureCoordinates = Attributes.GetVertexInstanceUVs();
		HOUDINI_TEST_EQUAL_ON_FAIL(TextureCoordinates.GetNumChannels(), 1, return true);

		TArray<FVector2f> OutputUVs;
		OutputUVs.Reserve(MeshDescription->VertexInstances().Num());
		for (FVertexInstanceID VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
			OutputUVs.Add(TextureCoordinates.Get(VertexInstanceID, 0));
		OutputUVs.Sort([](FVector2f Left, FVector2f Right) { return Left.X == Right.X ? Left.Y < Right.Y : Left.X < Right.X; });
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputUVs.Num(), 3, return true);
		HOUDINI_TEST_EQUAL(OutputUVs[0], FVector2f(0.125f, 0.25f));
		HOUDINI_TEST_EQUAL(OutputUVs[1], FVector2f(0.375f, 0.50f));
		HOUDINI_TEST_EQUAL(OutputUVs[2], FVector2f(0.625f, 0.75f));

		DynamicMeshComponent->RemoveFromRoot();
		DynamicMesh->RemoveFromRoot();
		return true;
	}));

	return true;
}

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

int GetPrimitiveCount(HAPI_NodeId NodeId)
{
	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	HAPI_PartInfo PartInfo;
	FHoudiniApi::GetPartInfo(Session, NodeId, 0, &PartInfo);

	int NumPrims = PartInfo.faceCount;
	
	return NumPrims;
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

	for (const FString& GroupName : GroupNames)
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


namespace
{
	const FString TestInputMeshesMap = TEXT("/Game/TestHDAs/Inputs/Meshes/TestInputMesh.umap");
	const FString TestInputMeshActorsMap = TEXT("/Game/TestHDAs/Inputs/Meshes/TestInputActorMesh.umap");
	const FString CubeMaterialName = TEXT("/Game/TestHDAs/Inputs/Meshes/CubeMaterial.CubeMaterial");
	const FString SphereMaterialName = TEXT("/Game/TestHDAs/Inputs/Meshes/SphereMaterial.SphereMaterial");
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_Meshes, "Houdini.UnitTests.Inputs.Meshes.Assets.Simultaneous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_Meshes::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniMultiTestContext> Context(new FHoudiniMultiTestContext());

	TSharedPtr<FHoudiniTestContext> MeshMainContext(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_Main")));
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

	AddCommand(new FHoudiniLatentTestCommand(MeshMainContext, [MeshMainContext]()
		{
			MeshMainContext->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MainMeshLODs, [MainMeshLODs]()
		{
			MainMeshLODs->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MeshLODsCollidersSockets, [MeshLODsCollidersSockets]()
		{
			MeshLODsCollidersSockets->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MeshLODsMaterialParams, [MeshLODsMaterialParams]()
		{
			MeshLODsMaterialParams->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, MeshMainContext, MainMeshLODs, MeshLODsCollidersSockets, MeshLODsMaterialParams]()
		{
			{
				HAPI_NodeId NodeId = MeshMainContext->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 1, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = MainMeshLODs->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = MeshLODsCollidersSockets->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 2);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 2);
			}

			{
				HAPI_NodeId NodeId = MeshLODsMaterialParams->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, 0.5f);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, 1.0f);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshMain, "Houdini.UnitTests.Inputs.Meshes.Assets.MeshMain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshMain::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_Main")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 0);

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshLODs, "Houdini.UnitTests.Inputs.Meshes.Assets.MeshLODs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshLODs::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_LODs")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
			FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 0);

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshLODsCollidersSockets, "Houdini.UnitTests.Inputs.Meshes.Assets.MeshLODsCollidersSockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshLODsCollidersSockets::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_LODsCollidersSockets")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
			FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 2);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 2);

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshLODsMaterialParams, "Houdini.UnitTests.Inputs.Meshes.Assets.MeshLODsMaterialParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshLODsMaterialParams::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_LODsMaterialParams")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
			FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, 0.5f);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, 1.0f);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 0);

			return true;
		}));

return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshesActors, "Houdini.UnitTests.Inputs.Meshes.Actors.Simultaneous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshesActors::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniMultiTestContext> Context(new FHoudiniMultiTestContext());

	TSharedPtr<FHoudiniTestContext> MeshMainContext(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_Main")));
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

	AddCommand(new FHoudiniLatentTestCommand(MeshMainContext, [MeshMainContext]()
		{
			MeshMainContext->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MainMeshLODs, [MainMeshLODs]()
		{
			MainMeshLODs->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MeshLODsCollidersSockets, [MeshLODsCollidersSockets]()
		{
			MeshLODsCollidersSockets->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(MeshLODsMaterialParams, [MeshLODsMaterialParams]()
		{
			MeshLODsMaterialParams->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, MeshMainContext, MainMeshLODs, MeshLODsCollidersSockets, MeshLODsMaterialParams]()
		{
			{
				HAPI_NodeId NodeId = MeshMainContext->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 1, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = MainMeshLODs->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			{
				HAPI_NodeId NodeId = MeshLODsCollidersSockets->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 2);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 2);
			}

			{
				HAPI_NodeId NodeId = MeshLODsMaterialParams->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

				TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
				TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
				HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
				HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

				FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
				FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

				float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam0, 0.5f);
				float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
				HOUDINI_TEST_EQUAL(ScalarParam1, 1.0f);

				TArray<FString> Collisions = GetCollisionGroups(NodeId);
				HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

				TArray<FString> Sockets = GetMeshSockets(NodeId);
				HOUDINI_TEST_EQUAL(Sockets.Num(), 0);
			}

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshMainActors, "Houdini.UnitTests.Inputs.Meshes.Actors.MeshMain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshMainActors::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshesMap, TEXT("Mesh_Main")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 0);

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshLODsActors, "Houdini.UnitTests.Inputs.Meshes.Actors.MeshLODs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshLODsActors::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshActorsMap, TEXT("Mesh_LODs")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
			FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 0);

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshLODsCollidersSocketsActors, "Houdini.UnitTests.Inputs.Meshes.Actors.MeshLODsCollidersSockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshLODsCollidersSocketsActors::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshActorsMap, TEXT("Mesh_LODsCollidersSockets")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
			FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, InvalidParam);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, InvalidParam);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 2);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 2);

			return true;
		}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_MeshLODsMaterialParamsActors, "Houdini.UnitTests.Inputs.Meshes.Actors.MeshLODsMaterialParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_MeshLODsMaterialParamsActors::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TestInputMeshActorsMap, TEXT("Mesh_LODsMaterialParams")));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			HAPI_NodeId NodeId = Context->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			TArray<int> PrimitiveLODs = GetPrimitiveLOD(NodeId);
			TArray<int> LODPrimitiveCount = CountLODPrimitives(PrimitiveLODs);
			HOUDINI_TEST_EQUAL_ON_FAIL(LODPrimitiveCount.Num(), 2, return true);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[0], 528);
			HOUDINI_TEST_EQUAL(LODPrimitiveCount[1], 12);

			FString Material0 = GetMaterialForLOD(NodeId, 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material0, SphereMaterialName);
			FString Material1 = GetMaterialForLOD(NodeId, 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(Material1, CubeMaterialName);

			float ScalarParam0 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_0_SphereScalarParam", 0, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam0, 0.5f);
			float ScalarParam1 = GetScalarParameterForLOD(NodeId, "unreal_material_parameter_1_CubeScalarParam", 1, PrimitiveLODs);
			HOUDINI_TEST_EQUAL(ScalarParam1, 1.0f);

			TArray<FString> Collisions = GetCollisionGroups(NodeId);
			HOUDINI_TEST_EQUAL(Collisions.Num(), 0);

			TArray<FString> Sockets = GetMeshSockets(NodeId);
			HOUDINI_TEST_EQUAL(Sockets.Num(), 0);

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
			{
				HAPI_NodeId NodeId = NaniteMesh->HAC->GetOutputAt(0)->GetHoudiniGeoPartObjects()[0].GeoId;

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

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

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

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

				HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

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
