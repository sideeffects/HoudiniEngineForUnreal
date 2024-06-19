/*
* Copyright (c) <2023> Side Effects Software Inc.
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


#include "HoudiniMeshUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ComponentReregisterContext.h"
#include "Components/PrimitiveComponent.h"
#include "ContentBrowserModule.h"
// #include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Modules/ModuleManager.h"
#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "MeshMergeHelpers.h"
#else
	#include "MeshMergeUtilities/Private/MeshMergeHelpers.h"
#endif

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2)
#include "StaticMeshOperations.h"
#endif

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniPackageParams.h"


bool FHoudiniMeshUtils::MergeMeshes(
	const TArray<UPrimitiveComponent*>& InMeshComponents,
	const FHoudiniPackageParams& InPackageParams,
	const FMeshMergingSettings& InSettings,
	UStaticMesh*& OutStaticMesh,
	FVector& OutMergedLocation,
	TArray<UObject*>* OutAssets)
{
	if (InMeshComponents.IsEmpty())
		return false;

	// Filter out invalid components or components without valid static meshes
	TArray<UPrimitiveComponent*> MeshComponents = InMeshComponents.FilterByPredicate([](UPrimitiveComponent const* const InComponent)
	{
		if (!IsValid(InComponent))
			return false;
		
		UStaticMeshComponent const* const MeshComponent = Cast<UStaticMeshComponent>(InComponent);
		if (IsValid(MeshComponent) && !IsValid(MeshComponent->GetStaticMesh()))
			return false;
		
		return true;
	});
	
	if (MeshComponents.IsEmpty())
		return false;

	// From: MeshMergingTool in the MergeActors plugin
	
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	UWorld* const World = MeshComponents[0]->GetWorld();
	if (!World)
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid World retrieved from mesh component \"%s\"!"), *(MeshComponents[0]->GetPathName()));
		return false;
	}
	TArray<UObject*> AssetsToSync;
	static constexpr float ScreenAreaSize = TNumericLimits<float>::Max();

	// Determine the package name via FHoudiniPackageParams
	// Pretend we are an HDA to generate a temp / cook package path
	const FString PackageName = FPaths::Combine(InPackageParams.GetPackagePath(), InPackageParams.GetPackageName());
	
	// If the merge destination package already exists, it is possible that the mesh is already used in a scene somewhere, or its materials or even just its textures.
	// Static primitives uniform buffers could become invalid after the operation completes and lead to memory corruption. To avoid it, we force a global reregister.
	if (FindObject<UObject>(nullptr, *PackageName))
	{
		FGlobalComponentReregisterContext GlobalReregister;
		MeshUtilities.MergeComponentsToStaticMesh(MeshComponents, World, InSettings, nullptr, nullptr, PackageName, AssetsToSync, OutMergedLocation, ScreenAreaSize, true);
	}
	else
	{
		MeshUtilities.MergeComponentsToStaticMesh(MeshComponents, World, InSettings, nullptr, nullptr, PackageName, AssetsToSync, OutMergedLocation, ScreenAreaSize, true);
	}

	if (AssetsToSync.IsEmpty())
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate static mesh for mesh component \"%s\""), *(MeshComponents[0]->GetPathName()));
		return false;
	}
	
	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const int32 AssetCount = AssetsToSync.Num();
	for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
	{
		AssetRegistry.AssetCreated(AssetsToSync[AssetIndex]);
		GEditor->BroadcastObjectReimported(AssetsToSync[AssetIndex]);
	}

	// //Also notify the content browser that the new assets exists
	// FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	// ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, true);

	// Set the new generated mesh in the input object
	OutStaticMesh = Cast<UStaticMesh>(AssetsToSync[0]);
	if (OutAssets)
		*OutAssets = AssetsToSync;

	return true;
}

bool
FHoudiniMeshUtils::RetrieveMesh(
	const UStaticMeshComponent* InStaticMeshComponent,
	int32 InLODIndex,
	FMeshDescription& OutMeshDescription,
	bool bInPropagateVertexColours,
	bool bInApplyComponentTransform)
{
	if (!IsValid(InStaticMeshComponent))
		return false;

	// Deform mesh data according to the Spline Mesh Component's data
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2)
	FMeshMergeHelpers::RetrieveMesh(InStaticMeshComponent, InLODIndex, OutMeshDescription, bInPropagateVertexColours, bInApplyComponentTransform);
#else
	FMeshMergeHelpers::RetrieveMesh(InStaticMeshComponent, InLODIndex, OutMeshDescription, bInPropagateVertexColours);
	if (!bInApplyComponentTransform)
	{
		// In UE <= 5.1 the component transform is applied by RetrieveMesh, undo it
		FStaticMeshOperations::ApplyTransform(OutMeshDescription, InStaticMeshComponent->GetComponentTransform().Inverse());
	}
#endif

	return true;
}
