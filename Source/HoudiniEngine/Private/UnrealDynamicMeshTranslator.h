/*
 * Copyright (c) <2026> Side Effects Software Inc.
 * All rights reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "UnrealObjectInputRuntimeTypes.h"

class UDynamicMesh;
class UDynamicMeshComponent;
struct FHoudiniInputObjectSettings;

struct HOUDINIENGINE_API FUnrealDynamicMeshTranslator
{
	static bool CreateInputNodeForDynamicMesh(
		FUnrealObjectInputHandle& OutHandle,
		UDynamicMesh* DynamicMesh,
		UDynamicMeshComponent* DynamicMeshComponent,
		const FHoudiniInputObjectSettings& ExportOptions,
		bool bInputNodesCanBeDeleted);

private:
	static bool HasCompleteOverlay(const UE::Geometry::FDynamicMesh3& Mesh, const UE::Geometry::FDynamicMeshNormalOverlay* Overlay);
	static void InitializeTangentsFromNormals(UE::Geometry::FDynamicMesh3& Mesh);
};
