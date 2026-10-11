/*
 * Copyright (c) <2026> Side Effects Software Inc.
 * All rights reserved.
 */

#include "UnrealDynamicMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInputTypes.h"
#include "UnrealMeshTranslator.h"
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputUtils.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTangents.h"
#include "DynamicMeshToMeshDescription.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "StaticMeshAttributes.h"
#include "UDynamicMesh.h"

bool
FUnrealDynamicMeshTranslator::HasCompleteOverlay(
	const UE::Geometry::FDynamicMesh3& Mesh,
	const UE::Geometry::FDynamicMeshNormalOverlay* Overlay)
{
	if (!Overlay || Overlay->ElementCount() == 0)
		return false;

	for (int32 TriangleId : Mesh.TriangleIndicesItr())
	{
		if (!Overlay->IsSetTriangle(TriangleId))
			return false;

		UE::Geometry::FIndex3i OverlayTriangle = Overlay->GetTriangle(TriangleId);
		if (Overlay->GetElement(OverlayTriangle.A).IsNearlyZero()
			|| Overlay->GetElement(OverlayTriangle.B).IsNearlyZero()
			|| Overlay->GetElement(OverlayTriangle.C).IsNearlyZero())
		{
			return false;
		}
	}

	return true;
}

void
FUnrealDynamicMeshTranslator::InitializeTangentsFromNormals(UE::Geometry::FDynamicMesh3& Mesh)
{
	UE::Geometry::FDynamicMeshAttributeSet* Attributes = Mesh.Attributes();
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = Attributes->PrimaryNormals();
	Attributes->DisableTangents();
	Attributes->EnableTangents();
	UE::Geometry::FDynamicMeshNormalOverlay* TangentOverlay = Attributes->PrimaryTangents();
	UE::Geometry::FDynamicMeshNormalOverlay* BitangentOverlay = Attributes->PrimaryBiTangents();

	TArray<int32> NormalToTangentElement;
	NormalToTangentElement.Init(INDEX_NONE, NormalOverlay->MaxElementID());
	for (int32 NormalElementId : NormalOverlay->ElementIndicesItr())
	{
		FVector3f Normal = NormalOverlay->GetElement(NormalElementId).GetSafeNormal();
		if (Normal.IsNearlyZero())
			Normal = FVector3f::UpVector;

		FVector3f Tangent;
		FVector3f Bitangent;
		Normal.FindBestAxisVectors(Tangent, Bitangent);
		NormalToTangentElement[NormalElementId] = TangentOverlay->AppendElement(Tangent);
		BitangentOverlay->AppendElement(Bitangent);
	}

	for (int32 TriangleId : Mesh.TriangleIndicesItr())
	{
		UE::Geometry::FIndex3i NormalTriangle = NormalOverlay->GetTriangle(TriangleId);
		UE::Geometry::FIndex3i TangentTriangle(
			NormalToTangentElement[NormalTriangle.A],
			NormalToTangentElement[NormalTriangle.B],
			NormalToTangentElement[NormalTriangle.C]);
		TangentOverlay->SetTriangle(TriangleId, TangentTriangle);
		BitangentOverlay->SetTriangle(TriangleId, TangentTriangle);
	}
}

bool
FUnrealDynamicMeshTranslator::CreateInputNodeForDynamicMesh(
	FUnrealObjectInputHandle& OutHandle,
	UDynamicMesh* DynamicMesh,
	UDynamicMeshComponent* DynamicMeshComponent,
	const FHoudiniInputObjectSettings& ExportOptions,
	bool bInputNodesCanBeDeleted)
{
	if (!IsValid(DynamicMesh))
		return false;

	// A component owns material assignments and therefore must not share a node with an asset-only input.
	UObject* IdentifierObject = IsValid(DynamicMeshComponent) ? static_cast<UObject*>(DynamicMeshComponent) : DynamicMesh;
	FUnrealObjectInputOptions Options = FUnrealObjectInputOptions::MakeOptionsForMesh(ExportOptions);
	FUnrealObjectInputIdentifier Identifier(IdentifierObject, Options, EUnrealObjectInputNodeType::Leaf);

	FUnrealObjectInputHandle Handle;
	if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
	{
		OutHandle = MoveTemp(Handle);
		return true;
	}

	FUnrealObjectInputHandle ParentHandle;
	if (!FUnrealObjectInputManager::Get().EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted))
		return false;

	HAPI_NodeId ParentNodeId = FUnrealObjectInputManager::Get().GetHAPINodeId(ParentHandle);
	FString NodeName = FUnrealObjectInputManager::Get().GetDefaultNodeName(Identifier);
	HAPI_NodeId NodeId = FHoudiniEngineUtils::CreateInputHapiNode(NodeName, ParentNodeId);
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NodeId))
		return false;

	HAPI_NodeId PreviousNodeId = FUnrealObjectInputManager::Get().GetHAPINodeId(Handle);
	if (PreviousNodeId >= 0)
	{
		// Create the replacement first: HAPI can reuse a deleted node id before stale input cleanup runs.
		HAPI_NodeId PreviousObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousNodeId);
		FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousNodeId);
		FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousObjectNodeId);
	}

	HAPI_NodeId ObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NodeId);

	// Dynamic-mesh overlays hold per-triangle-corner attributes, preserving splits such as hard normals and UV seams.
	// MeshDescription needs complete normal and tangent overlays for its vertex instances, so get them on a copy.

	UE::Geometry::FDynamicMesh3 MeshForConversion(DynamicMesh->GetMeshRef());
	MeshForConversion.EnableAttributes();
	UE::Geometry::FDynamicMeshAttributeSet* MeshAttributesForConversion = MeshForConversion.Attributes();
	if (!HasCompleteOverlay(MeshForConversion, MeshAttributesForConversion->PrimaryNormals()))
		UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(MeshAttributesForConversion->PrimaryNormals());

	// MeshDescription export requires complete tangent overlays. Use the normal-derived fallback if UE cannot compute them.
	if (!HasCompleteOverlay(MeshForConversion, MeshAttributesForConversion->PrimaryTangents())
		|| !HasCompleteOverlay(MeshForConversion, MeshAttributesForConversion->PrimaryBiTangents()))
	{
		bool bTangentsComputed = UE::Geometry::FMeshTangentsf::ComputeDefaultOverlayTangents(MeshForConversion);
		if (!bTangentsComputed
			|| !HasCompleteOverlay(MeshForConversion, MeshAttributesForConversion->PrimaryTangents())
			|| !HasCompleteOverlay(MeshForConversion, MeshAttributesForConversion->PrimaryBiTangents()))
			InitializeTangentsFromNormals(MeshForConversion);
	}

	FMeshDescription MeshDescription;
	FStaticMeshAttributes StaticMeshAttributes(MeshDescription);
	StaticMeshAttributes.Register();
	FDynamicMeshToMeshDescription Converter;
	Converter.Convert(&MeshForConversion, MeshDescription, true);

	FStaticMeshConstAttributes MeshAttributes(MeshDescription);
	TArray<UMaterialInterface*> Materials;
	if (IsValid(DynamicMeshComponent))
		Materials = DynamicMeshComponent->GetMaterials();
	// UDynamicMesh stores triangle material ids, not material interfaces. Asset-only inputs therefore need a valid fallback slot.
	if (Materials.IsEmpty())
		Materials.Add(UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface));

	TArray<uint16> SectionMaterialIndices;
	for (FPolygonGroupID PolygonGroupId : MeshDescription.PolygonGroups().GetElementIDs())
	{
		if (MeshDescription.GetNumPolygonGroupPolygons(PolygonGroupId) > 0)
			SectionMaterialIndices.Add(static_cast<uint16>(PolygonGroupId.GetValue()));
	}

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	bool bSuccess = FUnrealMeshTranslator::CreateAndPopulateMeshPartFromMeshDescription(
		NodeId,
		MeshDescription,
		MeshAttributes,
		0,
		false,
		ExportOptions.bExportMaterialParameters,
		DynamicMesh,
		DynamicMeshComponent,
		Materials,
		SectionMaterialIndices,
		FVector3f::OneVector,
		FString(),
		true,
		TOptional<int32>(),
		TOptional<float>(),
		TOptional<FMeshNaniteSettings>(),
		nullptr,
		true,
		PartInfo);
	if (!bSuccess)
		return false;

	if (!FHoudiniEngineUtils::HapiCookNode(NodeId, nullptr, true))
		return false;

	HAPI_NodeId OutputNodeId = NodeId;
	if (IsValid(DynamicMeshComponent) && ExportOptions.bExportColliders)
	{
		const FKAggregateGeom& SimpleColliders = DynamicMeshComponent->GetSimpleCollisionShapes();
		if (SimpleColliders.BoxElems.Num() + SimpleColliders.SphereElems.Num() + SimpleColliders.SphylElems.Num() + SimpleColliders.ConvexElems.Num() > 0)
		{
			// Keep the render mesh and simple collision primitives in one final input node.
			HAPI_NodeId MergeNodeId = -1;
			HAPI_Result Result = FHoudiniEngineUtils::CreateNode(
				ObjectNodeId, TEXT("merge"), NodeName + TEXT("_with_colliders"), true, &MergeNodeId);
			if (Result != HAPI_RESULT_SUCCESS)
				return false;

			HAPI_Result ConnectResult = FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(), MergeNodeId, 0, NodeId, 0);
			if (ConnectResult != HAPI_RESULT_SUCCESS)
				return false;

			int32 NextMergeIndex = 1;
			int32 NextColliderIndex = 0;
			bool bCollidersCreated = FUnrealMeshTranslator::CreateAndConnectSimpleColliders(
				SimpleColliders, ObjectNodeId, MergeNodeId, NextMergeIndex, NextColliderIndex);
			if (!bCollidersCreated)
				return false;

			if (!FHoudiniEngineUtils::HapiCookNode(MergeNodeId, nullptr, true))
				return false;

			OutputNodeId = MergeNodeId;
		}
	}

	// The managed leaf is the final merge when collision was exported, but it remains owned by the input object node.
	if (!FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, OutputNodeId, Handle, ObjectNodeId, nullptr, bInputNodesCanBeDeleted))
		return false;

	OutHandle = MoveTemp(Handle);
	return true;
}
