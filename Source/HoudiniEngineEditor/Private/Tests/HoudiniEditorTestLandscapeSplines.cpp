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

#include "HoudiniEditorTestLandscapes.h"

#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterToggle.h"
#include "Landscape.h"
#include "LandscapeSplineControlPoint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "LandscapeEdit.h"
#include "HoudiniEditorTests.h"
#include "HoudiniOutput.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "UObject/StrongObjectPtr.h"
#include "HoudiniLandscapeSplineTranslator.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "Misc/ScopeExit.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(
	FHoudiniEditorTestLandscapeSplines_SegmentEndPointAttributes,
	"Houdini.UnitTests.LandscapeSplines.SegmentEndPointAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_SegmentEndPointAttributes::RunTest(const FString& Parameters)
{
#if !DISABLE_LANDSCAPE_RELATED_TEST
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(
		this,
		FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName,
		{},
		{});
	TSharedPtr<FHoudiniTestContext> Context = MakeShared<FHoudiniTestContext>(
		this,
		TEXT("/Game/TestHDAs/LandscapeSplines/Test_LandscapeSpline"),
		FTransform::Identity,
		false);
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);
	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> ExistingOutputs;
		Context->GetOutputs(ExistingOutputs);
		TArray<UHoudiniLandscapeTargetLayerOutput*> Landscapes =
			FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(ExistingOutputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(Landscapes.Num(), 1, return true);

		HAPI_NodeId ParentId = -1;
		bool bSuccess = FHoudiniApi::CreateNode(
			FHoudiniEngine::Get().GetSession(),
			-1,
			"Object/geo",
			"landscape_segment_endpoint_test",
			false,
			&ParentId) == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Create endpoint test parent"), bSuccess))
			return true;
		ON_SCOPE_EXIT
		{
			bool bSuccess = FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(),
				ParentId) == HAPI_RESULT_SUCCESS;
			TestTrue(TEXT("Delete endpoint test parent"), bSuccess);
		};
		HAPI_NodeId NodeId = -1;
		bSuccess = FHoudiniApi::CreateInputNode(
			FHoudiniEngine::Get().GetSession(),
			ParentId,
			&NodeId,
			"endpoint_curve") == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Create endpoint curve input"), bSuccess))
			return true;

		HAPI_PartInfo PartInfo;
		FHoudiniApi::PartInfo_Init(&PartInfo);
		PartInfo.type = HAPI_PARTTYPE_CURVE;
		PartInfo.pointCount = 5;
		PartInfo.vertexCount = 5;
		PartInfo.faceCount = 1;
		bSuccess = FHoudiniApi::SetPartInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			0,
			&PartInfo) == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Set endpoint curve part"), bSuccess))
			return true;
		HAPI_CurveInfo CurveInfo;
		FHoudiniApi::CurveInfo_Init(&CurveInfo);
		CurveInfo.curveType = HAPI_CURVETYPE_LINEAR;
		CurveInfo.curveCount = 1;
		CurveInfo.vertexCount = 5;
		bSuccess = FHoudiniApi::SetCurveInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			0,
			&CurveInfo) == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Set endpoint curve info"), bSuccess))
			return true;
		int32 PointCount = 5;
		bSuccess = FHoudiniApi::SetCurveCounts(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			0,
			&PointCount,
			0,
			1) == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Set endpoint curve count"), bSuccess))
			return true;

		// Unreal clears connection socket names unless they exist on the control point mesh.
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (!TestNotNull(TEXT("Load control point mesh source"), Cube))
			return true;
		TStrongObjectPtr<UStaticMesh> ControlPointMesh(DuplicateObject<UStaticMesh>(Cube, GetTransientPackage()));
		for (const TCHAR* SocketName : {
			TEXT("Start0"), TEXT("Middle0"), TEXT("End0"),
			TEXT("Start1"), TEXT("Middle1"), TEXT("End1")})
		{
			UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(ControlPointMesh.Get());
			Socket->SocketName = SocketName;
			ControlPointMesh->Sockets.Add(Socket);
		}

		auto SetPointAttribute = [this, NodeId](
			const char* Name,
			HAPI_StorageType Storage,
			int32 TupleSize,
			const auto& Values)
		{
			FHoudiniHapiAccessor Accessor(NodeId, 0, Name);
			HAPI_AttributeInfo Info;
			bool bSuccess = Accessor.AddAttribute(HAPI_ATTROWNER_POINT, Storage, TupleSize, 5, &Info);
			if (bSuccess)
				bSuccess = Accessor.SetAttributeData(Info, Values);
			TestTrue(UTF8_TO_TCHAR(Name), bSuccess);
			return bSuccess;
		};
		// Three control points at indices 0, 2 and 4, with two skipped intermediate points.
		bSuccess = SetPointAttribute(
			"P",
			HAPI_STORAGETYPE_FLOAT,
			3,
			TArray<float>{0, 0, 0, 2.5f, 0, 0, 5, 0, 0, 7.5f, 0, 0, 10, 0, 0});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_control_point_id",
			HAPI_STORAGETYPE_INT,
			1,
			TArray<int32>{0, -1, 1, -1, 2});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_control_point_mesh",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{
				ControlPointMesh->GetPathName(), ControlPointMesh->GetPathName(), ControlPointMesh->GetPathName(),
				ControlPointMesh->GetPathName(), ControlPointMesh->GetPathName()});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_segment_mesh",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{
				TEXT("/Engine/BasicShapes/Cube.Cube"),
				TEXT("/Engine/BasicShapes/Cone.Cone"),
				TEXT("/Engine/BasicShapes/Sphere.Sphere"),
				TEXT("/Engine/BasicShapes/Cone.Cone"),
				TEXT("/Engine/BasicShapes/Cylinder.Cylinder")});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_segment_paint_layer_name",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{TEXT("Start"), TEXT("Skipped1"), TEXT("Middle"), TEXT("Skipped3"), TEXT("End")});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_segment_cast_shadow",
			HAPI_STORAGETYPE_INT,
			1,
			TArray<int32>{0, 0, 1, 0, 0});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_connection0_mesh_socket_name",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{TEXT("Start0"), TEXT("Skip0"), TEXT("Middle0"), TEXT("Skip0"), TEXT("End0")});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_connection1_mesh_socket_name",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{TEXT("Start1"), TEXT("Skip1"), TEXT("Middle1"), TEXT("Skip1"), TEXT("End1")});
		FString BasicMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
		FString GridMaterialPath = TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial");
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_segment_mesh_material_override",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{GridMaterialPath, GridMaterialPath, BasicMaterialPath, GridMaterialPath, GridMaterialPath});
		bSuccess &= SetPointAttribute(
			"unreal_landscape_spline_segment_mesh_material_override1",
			HAPI_STORAGETYPE_STRING,
			1,
			TArray<FString>{BasicMaterialPath, BasicMaterialPath, GridMaterialPath, BasicMaterialPath, BasicMaterialPath});
		auto SetPrimitiveString = [this, NodeId](const char* Name, const FString& Value)
		{
			FHoudiniHapiAccessor Accessor(NodeId, 0, Name);
			HAPI_AttributeInfo Info;
			bool bSuccess = Accessor.AddAttribute(HAPI_ATTROWNER_PRIM, HAPI_STORAGETYPE_STRING, 1, 1, &Info);
			if (bSuccess)
				bSuccess = Accessor.SetAttributeData(Info, TArray<FString>{Value});
			TestTrue(UTF8_TO_TCHAR(Name), bSuccess);
			return bSuccess;
		};
		bSuccess &= SetPrimitiveString(
			"unreal_landscape_spline_segment_mesh",
			TEXT("/Engine/BasicShapes/Cube.Cube"));
		bSuccess &= SetPrimitiveString(
			"unreal_landscape_spline_segment_mesh_material_override",
			GridMaterialPath);
		bSuccess &= SetPrimitiveString(
			"unreal_landscape_spline_segment_mesh_material_override1",
			BasicMaterialPath);
		if (!bSuccess)
			return true;
		bSuccess = FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), NodeId) == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Commit endpoint attributes"), bSuccess))
			return true;
		bSuccess = FHoudiniEngineUtils::HapiCookNode(NodeId, nullptr, true);
		if (!TestTrue(TEXT("Cook endpoint curve"), bSuccess))
			return true;

		TStrongObjectPtr<UHoudiniOutput> Output(NewObject<UHoudiniOutput>());
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;
		ON_SCOPE_EXIT
		{
			for (auto& Entry : OutputObjects)
			{
				UHoudiniLandscapeSplinesOutput* Splines = Cast<UHoudiniLandscapeSplinesOutput>(Entry.Value.OutputObject);
				if (IsValid(Splines))
					Splines->Clear(false);
			}
		};
		FHoudiniGeoPartObject HGPO;
		HGPO.AssetId = NodeId;
		HGPO.ObjectId = ParentId;
		HGPO.GeoId = NodeId;
		HGPO.PartId = 0;
		HGPO.PartName = TEXT("endpoint_curve");
		HGPO.Type = EHoudiniPartType::LandscapeSpline;
		FHoudiniPackageParams PackageParams;
		TArray<ALandscapeProxy*> InputLandscapes;
		TMap<ALandscape*, TSet<FName>> ClearedLayers;
		TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData> SegmentsToApply;
		bSuccess = FHoudiniLandscapeSplineTranslator::CreateOutputLandscapeSpline(
			HGPO,
			Output.Get(),
			InputLandscapes,
			Context->GetWorld(),
			PackageParams,
			Landscapes[0]->Landscape,
			ClearedLayers,
			SegmentsToApply,
			OutputObjects);
		if (!TestTrue(TEXT("Translate endpoint curve through segment creation"), bSuccess))
			return true;
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
		UHoudiniLandscapeSplinesOutput* Splines =
			Cast<UHoudiniLandscapeSplinesOutput>(OutputObjects.CreateIterator().Value().OutputObject);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Splines, return true);
		auto& Segments = Splines->GetSegments();
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments.Num(), 2, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments[0]->SplineMeshes.Num(), 1, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments[1]->SplineMeshes.Num(), 1, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Segments[0]->SplineMeshes[0].Mesh.Get(), return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Segments[1]->SplineMeshes[0].Mesh.Get(), return true);
		TestEqual(
			TEXT("First segment reads mesh from middle control point"),
			Segments[0]->SplineMeshes[0].Mesh->GetPathName(),
			FString(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		TestEqual(
			TEXT("Second segment reads mesh from final control point"),
			Segments[1]->SplineMeshes[0].Mesh->GetPathName(),
			FString(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		TestEqual(TEXT("First segment uses end-point paint layer"), Segments[0]->LayerName, FName(TEXT("Middle")));
		TestEqual(TEXT("Second segment uses end-point paint layer"), Segments[1]->LayerName, FName(TEXT("End")));
		TestTrue(TEXT("First segment uses end-point shadow flag"), Segments[0]->bCastShadow);
		TestFalse(TEXT("Second segment uses end-point shadow flag"), Segments[1]->bCastShadow);
		TestEqual(
			TEXT("First connection 0 reads start point"),
			Segments[0]->Connections[0].SocketName,
			FName(TEXT("Start0")));
		TestEqual(
			TEXT("First connection 1 reads end point"),
			Segments[0]->Connections[1].SocketName,
			FName(TEXT("Middle1")));
		TestEqual(
			TEXT("Second connection 0 reads start point"),
			Segments[1]->Connections[0].SocketName,
			FName(TEXT("Middle0")));
		TestEqual(
			TEXT("Second connection 1 reads end point"),
			Segments[1]->Connections[1].SocketName,
			FName(TEXT("End1")));

		UMaterialInterface* BasicMaterial = LoadObject<UMaterialInterface>(nullptr, *BasicMaterialPath);
		UMaterialInterface* GridMaterial = LoadObject<UMaterialInterface>(nullptr, *GridMaterialPath);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(BasicMaterial, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(GridMaterial, return true);
		auto CheckMaterials = [this](
			ULandscapeSplineSegment* Segment,
			UMaterialInterface* ExpectedSlot0,
			UMaterialInterface* ExpectedSlot1)
		{
			bool bSuccess = TestEqual(TEXT("One spline mesh entry"), Segment->SplineMeshes.Num(), 1);
			if (!bSuccess)
				return bSuccess;
			auto& Overrides = Segment->SplineMeshes[0].MaterialOverrides;
			bSuccess = TestEqual(TEXT("Two material override slots"), Overrides.Num(), 2);
			if (!bSuccess)
				return bSuccess;
			bSuccess &= TestTrue(TEXT("Entry material slot 0"), Overrides[0] == ExpectedSlot0);
			bSuccess &= TestTrue(TEXT("Entry material slot 1"), Overrides[1] == ExpectedSlot1);
			// The C++ getter is not exported by Landscape's MinimalAPI class.
			FArrayProperty* ComponentsProperty = FindFProperty<FArrayProperty>(
				Segment->GetClass(),
				TEXT("LocalMeshComponents"));
			bSuccess &= TestNotNull(TEXT("Spline mesh component array property"), ComponentsProperty);
			if (!ComponentsProperty)
				return bSuccess;
			FScriptArrayHelper Components(
				ComponentsProperty,
				ComponentsProperty->ContainerPtrToValuePtr<void>(Segment));
			FObjectPropertyBase* ComponentProperty = CastFieldChecked<FObjectPropertyBase>(ComponentsProperty->Inner);
			bSuccess &= TestTrue(TEXT("Spline mesh components generated"), Components.Num() > 0);
			for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
			{
				USplineMeshComponent* Component = Cast<USplineMeshComponent>(
					ComponentProperty->GetObjectPropertyValue(Components.GetRawPtr(ComponentIndex)));
				bSuccess &= TestNotNull(TEXT("Generated spline mesh component"), Component);
				if (!Component)
					continue;
				bSuccess &= TestEqual(TEXT("Component has two override slots"), Component->OverrideMaterials.Num(), 2);
				if (Component->OverrideMaterials.Num() == 2)
				{
					bSuccess &= TestTrue(TEXT("Component material slot 0"), Component->OverrideMaterials[0] == ExpectedSlot0);
					bSuccess &= TestTrue(TEXT("Component material slot 1"), Component->OverrideMaterials[1] == ExpectedSlot1);
				}
			}
			return bSuccess;
		};
		// Different end points must retain their own slot order and override primitive defaults.
		CheckMaterials(Segments[0], BasicMaterial, GridMaterial);
		CheckMaterials(Segments[1], GridMaterial, BasicMaterial);

		// Reimport the same curve with only primitive mesh/material attributes.
		Splines->Clear(false);
		OutputObjects.Empty();
		SegmentsToApply.Empty();
		for (const char* AttributeName : {
			"unreal_landscape_spline_segment_mesh",
			"unreal_landscape_spline_segment_mesh_material_override",
			"unreal_landscape_spline_segment_mesh_material_override1"})
		{
			HAPI_AttributeInfo Info;
			FHoudiniApi::AttributeInfo_Init(&Info);
			bSuccess = FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				NodeId,
				0,
				AttributeName,
				HAPI_ATTROWNER_POINT,
				&Info) == HAPI_RESULT_SUCCESS;
			if (!TestTrue(TEXT("Find point mesh attribute for primitive fallback"), bSuccess))
				return true;
			bSuccess = FHoudiniApi::DeleteAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId,
				0,
				AttributeName,
				&Info) == HAPI_RESULT_SUCCESS;
			if (!TestTrue(TEXT("Remove point mesh attribute for primitive fallback"), bSuccess))
				return true;
		}
		bSuccess = FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), NodeId) == HAPI_RESULT_SUCCESS;
		if (!TestTrue(TEXT("Commit primitive-only mesh attributes"), bSuccess))
			return true;
		bSuccess = FHoudiniEngineUtils::HapiCookNode(NodeId, nullptr, true);
		if (!TestTrue(TEXT("Cook primitive-only mesh attributes"), bSuccess))
			return true;
		bSuccess = FHoudiniLandscapeSplineTranslator::CreateOutputLandscapeSpline(
			HGPO,
			Output.Get(),
			InputLandscapes,
			Context->GetWorld(),
			PackageParams,
			Landscapes[0]->Landscape,
			ClearedLayers,
			SegmentsToApply,
			OutputObjects);
		if (!TestTrue(TEXT("Translate primitive-only material overrides"), bSuccess))
			return true;
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
		Splines = Cast<UHoudiniLandscapeSplinesOutput>(OutputObjects.CreateIterator().Value().OutputObject);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Splines, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(Splines->GetSegments().Num(), 2, return true);
		for (ULandscapeSplineSegment* Segment : Splines->GetSegments())
		{
			HOUDINI_TEST_EQUAL_ON_FAIL(Segment->SplineMeshes.Num(), 1, return true);
			TestTrue(TEXT("Primitive mesh selected"), Segment->SplineMeshes[0].Mesh.Get() == Cube);
			CheckMaterials(Segment, GridMaterial, BasicMaterial);
		}
		return true;
	}));
#endif
	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(
	FHoudiniEditorTestLandscapeSplines_SegmentMeshFallback,
	"Houdini.UnitTests.LandscapeSplines.SegmentMeshFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_SegmentMeshFallback::RunTest(const FString& Parameters)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	TStrongObjectPtr<ULandscapeSplinesComponent> Component(NewObject<ULandscapeSplinesComponent>());
	FHoudiniLandscapeSplineData SplineData;
	// Attribute reading allocates a slot for every point even without point mesh attributes.
	SplineData.SegmentMeshData.SetNum(3);
	SplineData.DefaultMeshSegmentData.SetNum(1);
	FHoudiniLandscapeSplineMesh PrimitiveMesh;
	PrimitiveMesh.MeshRef = TEXT("/Engine/BasicShapes/Cube.Cube");
	PrimitiveMesh.MeshScale = FVector(2.0, 3.0, 4.0);
	SplineData.DefaultMeshSegmentData[0].Meshes.Add(PrimitiveMesh);
	SplineData.DefaultMeshSegmentData[0].Meshes.Add(PrimitiveMesh);

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, *PrimitiveMesh.MeshRef);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	bool bSuccess = TestNotNull(TEXT("Primitive mesh asset loaded"), Cube);
	bSuccess &= TestNotNull(TEXT("Point mesh asset loaded"), Sphere);
	if (!bSuccess)
		return bSuccess;

	auto CheckSegmentMeshes = [this, &SplineData, &Component](
		const TCHAR* CaseName,
		int32 PointIndex,
		UStaticMesh* ExpectedMesh,
		int32 ExpectedCount,
		FVector ExpectedScale)
	{
		ULandscapeSplineSegment* Segment = NewObject<ULandscapeSplineSegment>(Component.Get());
		bool bSuccess = FHoudiniLandscapeSplineTranslator::SetSegmentData(
			Segment,
			SplineData,
			PointIndex);
		bSuccess &= TestTrue(CaseName, bSuccess);
		bSuccess &= TestEqual(CaseName, Segment->SplineMeshes.Num(), ExpectedCount);
		for (FLandscapeSplineMeshEntry& MeshEntry : Segment->SplineMeshes)
		{
			bSuccess &= TestTrue(CaseName, MeshEntry.Mesh.Get() == ExpectedMesh);
			bSuccess &= TestEqual(CaseName, MeshEntry.Scale, ExpectedScale);
		}
		return bSuccess;
	};

	bSuccess &= CheckSegmentMeshes(
		TEXT("Primitive-only attributes supply all mesh entries"),
		1,
		Cube,
		2,
		PrimitiveMesh.MeshScale);

	FHoudiniLandscapeSplineMesh PointMesh;
	PointMesh.MeshRef = Sphere->GetPathName();
	SplineData.SegmentMeshData[1].Meshes.Add(PointMesh);
	bSuccess &= CheckSegmentMeshes(
		TEXT("Point mesh overrides primitive meshes"),
		1,
		Sphere,
		1,
		PointMesh.MeshScale);
	bSuccess &= CheckSegmentMeshes(
		TEXT("Point without mesh entries retains primitive meshes"),
		2,
		Cube,
		2,
		PrimitiveMesh.MeshScale);

	SplineData.DefaultMeshSegmentData.Empty();
	bSuccess &= CheckSegmentMeshes(
		TEXT("Point-only mesh remains supported"),
		1,
		Sphere,
		1,
		PointMesh.MeshScale);
	bSuccess &= CheckSegmentMeshes(
		TEXT("No mesh attributes create no entries"),
		2,
		nullptr,
		0,
		FVector::OneVector);
	return bSuccess;
#else
	return true;
#endif
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(
	FHoudiniEditorTestLandscapeSplines_OutputCleanup,
	"Houdini.UnitTests.LandscapeSplines.OutputCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_OutputCleanup::RunTest(const FString& Parameters)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	TStrongObjectPtr<ULandscapeSplinesComponent> Component(NewObject<ULandscapeSplinesComponent>());
	ULandscapeSplineControlPoint* UnrelatedPoint = NewObject<ULandscapeSplineControlPoint>(Component.Get());
	Component->GetControlPoints().Add(UnrelatedPoint);
	bool bSuccess = true;

	// Reuse the component as recooks do when targeting an existing landscape.
	for (int32 CookIndex = 0; CookIndex < 2; ++CookIndex)
	{
		TStrongObjectPtr<UHoudiniLandscapeSplinesOutput> Output(NewObject<UHoudiniLandscapeSplinesOutput>());
		Output->SetLandscapeSplinesComponent(Component.Get());
		ULandscapeSplineControlPoint* Start = NewObject<ULandscapeSplineControlPoint>(Component.Get());
		ULandscapeSplineControlPoint* End = NewObject<ULandscapeSplineControlPoint>(Component.Get());
		ULandscapeSplineSegment* Segment = NewObject<ULandscapeSplineSegment>(Component.Get());
		Segment->Connections[0].ControlPoint = Start;
		Segment->Connections[1].ControlPoint = End;
		Start->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
		End->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));
		Component->GetControlPoints().Add(Start);
		Component->GetControlPoints().Add(End);
		Component->GetSegments().Add(Segment);
		Output->GetControlPoints().Add(Start);
		Output->GetControlPoints().Add(End);
		Output->GetSegments().Add(Segment);

		Output->Clear();
		bSuccess &= TestEqual(
			TEXT("Deleted segments removed from component"),
			Component->GetSegments().Num(),
			0);
		bSuccess &= TestEqual(
			TEXT("Only unrelated control point remains"),
			Component->GetControlPoints().Num(),
			1);
		bSuccess &= TestTrue(
			TEXT("Unrelated control point preserved"),
			Component->GetControlPoints().Contains(UnrelatedPoint));
		bSuccess &= TestEqual(TEXT("Start connections cleared"), Start->ConnectedSegments.Num(), 0);
		bSuccess &= TestEqual(TEXT("End connections cleared"), End->ConnectedSegments.Num(), 0);
		bSuccess &= TestEqual(TEXT("Output segments cleared"), Output->GetSegments().Num(), 0);
		bSuccess &= TestEqual(TEXT("Output control points cleared"), Output->GetControlPoints().Num(), 0);

		if (!bSuccess)
		{
			// Avoid asserting inside Unreal's validity check when testing the broken cleanup.
			Component->GetSegments().Empty();
			Component->GetControlPoints().Empty();
			return bSuccess;
		}
		Component->CheckSplinesValid();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		Component->CheckSplinesValid();
	}

	// A point shared with a segment outside this output must keep its owner and connections.
	TStrongObjectPtr<UHoudiniLandscapeSplinesOutput> Output(NewObject<UHoudiniLandscapeSplinesOutput>());
	Output->SetLandscapeSplinesComponent(Component.Get());
	ULandscapeSplineControlPoint* SharedPoint = NewObject<ULandscapeSplineControlPoint>(Component.Get());
	ULandscapeSplineSegment* SharedSegment = NewObject<ULandscapeSplineSegment>(Component.Get());
	SharedSegment->Connections[0].ControlPoint = SharedPoint;
	SharedSegment->Connections[1].ControlPoint = UnrelatedPoint;
	SharedPoint->ConnectedSegments.Add(FLandscapeSplineConnection(SharedSegment, 0));
	UnrelatedPoint->ConnectedSegments.Add(FLandscapeSplineConnection(SharedSegment, 1));
	Component->GetControlPoints().Add(SharedPoint);
	Component->GetSegments().Add(SharedSegment);
	Output->GetControlPoints().Add(SharedPoint);
	Output->Clear();
	bSuccess &= TestTrue(
		TEXT("Shared point keeps its spline component owner"),
		SharedPoint->GetOuter() == Component.Get());
	bSuccess &= TestEqual(TEXT("Shared point stays connected"), SharedPoint->ConnectedSegments.Num(), 1);
	bSuccess &= TestEqual(TEXT("Shared segment preserved"), Component->GetSegments().Num(), 1);
	bSuccess &= TestEqual(TEXT("Shared control points preserved"), Component->GetControlPoints().Num(), 2);
	if (bSuccess)
	{
		Component->CheckSplinesValid();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		Component->CheckSplinesValid();
	}
	return bSuccess;
#else
	return true;
#endif
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeSplines_Simple, "Houdini.UnitTests.LandscapeSplines.Simple", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_Simple::RunTest(const FString& Parameters)
{
#if !DISABLE_LANDSCAPE_RELATED_TEST
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes Splines.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/LandscapeSplines/Test_LandscapeSpline"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		//	SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "size", LandscapeSize, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

		TArray<UHoudiniLandscapeSplinesOutput*> SplineOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeSplinesOutput>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SplineOutputs.Num(), 1, return true);

		TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
		HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
		ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

		//
		// Check control points. 6 are specified in the Test HDA but only 4 should be output (the first and last + those marked
		// with >= 0 ids).
		//

		ULandscapeSplinesComponent * SplineComponent = SplineOutputs[0]->GetLandscapeSplinesComponent();
		auto ControlPoints = SplineComponent->GetControlPoints();

		FTransform Transform = SplineComponent->GetComponentTransform();
		HOUDINI_TEST_EQUAL_ON_FAIL(ControlPoints.Num(), 4, return true);
		TArray<FVector> Positions;
		for (int Index = 0; Index < ControlPoints.Num(); Index++)
		{
			Positions.Add(Transform.TransformPosition(ControlPoints[Index]->Location));
		}
		HOUDINI_TEST_EQUAL(Positions[0], FVector(-50000.0f, -50000.0f, 0.0f), 0.1f);
		HOUDINI_TEST_EQUAL(Positions[1], FVector(0.0f, -50000.0f, 0.0f), 0.1f);
		HOUDINI_TEST_EQUAL(Positions[2], FVector(0.0f, 50000.0f, 0.0f), 0.1f);
		HOUDINI_TEST_EQUAL(Positions[3], FVector(0.0f, 0.0f, 0.0f), 0.1f);

		HOUDINI_TEST_EQUAL(IsValid(ControlPoints[1]->Mesh), true);
		HOUDINI_TEST_EQUAL(ControlPoints[1]->MeshScale, FVector(99.0, 99.0, 99.0));

		//
		// Check segments
		//
		auto Segments = SplineComponent->GetSegments();
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments.Num(), 3, return true);

		return true;
	}));


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///

#endif
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeSplines_WorldPartition, "Houdini.UnitTests.LandscapeSplines.WorldPartition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapeSplines_WorldPartition::RunTest(const FString& Parameters)
{
#if !DISABLE_LANDSCAPE_RELATED_TEST
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/LandscapeSplines/Test_LandscapeSpline"), FTransform::Identity, true));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		//	SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "size", LandscapeSize, 0);
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

		TArray<UHoudiniLandscapeSplinesOutput*> SplineOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeSplinesOutput>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(SplineOutputs.Num(), 1, return true);

		TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
		HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
		ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

		//
		// Check control points. 6 are specified in the Test HDA but only 4 should be output (the first and last + those marked
		// with >= 0 ids).
		//

		ULandscapeSplinesComponent* SplineComponent = SplineOutputs[0]->GetLandscapeSplinesComponent();
		auto ControlPoints = SplineComponent->GetControlPoints();

		FTransform Transform = SplineComponent->GetComponentTransform();
		HOUDINI_TEST_EQUAL_ON_FAIL(ControlPoints.Num(), 4, return true);
		TArray<FVector> Positions;
		for (int Index = 0; Index < ControlPoints.Num(); Index++)
		{
			Positions.Add(Transform.TransformPosition(ControlPoints[Index]->Location));
		}
		HOUDINI_TEST_EQUAL(Positions[0], FVector(-50000.0f, -50000.0f, 0.0f), 0.1f);
		HOUDINI_TEST_EQUAL(Positions[1], FVector(0.0f, -50000.0f, 0.0f), 0.1f);
		HOUDINI_TEST_EQUAL(Positions[2], FVector(0.0f, 50000.0f, 0.0f), 0.1f);
		HOUDINI_TEST_EQUAL(Positions[3], FVector(0.0f, 0.0f, 0.0f), 0.1f);

		HOUDINI_TEST_EQUAL(IsValid(ControlPoints[1]->Mesh), true);
		HOUDINI_TEST_EQUAL(ControlPoints[1]->MeshScale, FVector(99.0, 99.0, 99.0));

		//
		// Check segments
		//
		auto Segments = SplineComponent->GetSegments();
		HOUDINI_TEST_EQUAL_ON_FAIL(Segments.Num(), 3, return true);

		return true;
	}));


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapeSplineMeshes_Simple, "Houdini.UnitTests.LandscapeSplines.Meshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

	bool FHoudiniEditorTestLandscapeSplineMeshes_Simple::RunTest(const FString& Parameters)
{
#if !DISABLE_LANDSCAPE_RELATED_TEST
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes Splines.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/LandscapeSplines/Test_LandscapeSplineMeshes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			//	SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "size", LandscapeSize, 0);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);

			// We should have two outputs, two meshes
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

			TArray<UHoudiniLandscapeSplinesOutput*> SplineOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeSplinesOutput>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(SplineOutputs.Num(), 1, return true);

			TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
			HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
			ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

			//
			// Check control points. 6 are specified in the Test HDA but only 4 should be output (the first and last + those marked
			// with >= 0 ids).
			//

			ULandscapeSplinesComponent* SplineComponent = SplineOutputs[0]->GetLandscapeSplinesComponent();
			auto ControlPoints = SplineComponent->GetControlPoints();

			FTransform Transform = SplineComponent->GetComponentTransform();
			HOUDINI_TEST_EQUAL_ON_FAIL(ControlPoints.Num(), 4, return true);
			TArray<FVector> Positions;
			for (int Index = 0; Index < ControlPoints.Num(); Index++)
			{
				Positions.Add(Transform.TransformPosition(ControlPoints[Index]->Location));
			}
			HOUDINI_TEST_EQUAL(Positions[0], FVector(-50000.0f, -50000.0f, 0.0f), 0.1f);
			HOUDINI_TEST_EQUAL(Positions[1], FVector(0.0f, -50000.0f, 0.0f), 0.1f);
			HOUDINI_TEST_EQUAL(Positions[2], FVector(0.0f, 50000.0f, 0.0f), 0.1f);
			HOUDINI_TEST_EQUAL(Positions[3], FVector(0.0f, 0.0f, 0.0f), 0.1f);

			HOUDINI_TEST_EQUAL(IsValid(ControlPoints[1]->Mesh), true);
			HOUDINI_TEST_EQUAL(ControlPoints[1]->MeshScale, FVector(99.0, 99.0, 99.0));

			//
			// Check segments
			//
			auto Segments = SplineComponent->GetSegments();
			HOUDINI_TEST_EQUAL_ON_FAIL(Segments.Num(), 3, return true);

			HOUDINI_TEST_EQUAL(Segments[0]->SplineMeshes.Num(), 1);
			HOUDINI_TEST_NOT_NULL(Segments[0]->SplineMeshes[0].Mesh.Get());
			HOUDINI_TEST_EQUAL(Segments[0]->SplineMeshes[0].bScaleToWidth, false);
			HOUDINI_TEST_EQUAL(Segments[0]->SplineMeshes[0].bCenterH, true);
			HOUDINI_TEST_EQUAL(Segments[0]->bCastShadow, false);
			return true;
		}));


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///

#endif
	return true;
}

#endif