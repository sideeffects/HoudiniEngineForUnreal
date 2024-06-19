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

#pragma once

#include "CoreMinimal.h"

struct FHoudiniPackageParams;

struct FMeshDescription;
struct FMeshMergingSettings;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;


/**
 * 
 */
class HOUDINIENGINE_API FHoudiniMeshUtils
{
public:
	/**
	 * @brief Merges the meshes of InMeshComponents into one new UStaticMesh.
	 * @param InMeshComponents The primitive components' meshes to merge.
	 * @param InPackageParams The params to use for determining the package and object path of the new static mesh.
	 * @param InSettings The mesh merging settings.
	 * @param OutStaticMesh The new / output static mesh.
	 * @param OutMergedLocation The output transform of the merged mesh.
	 * @param OutAssets All assets that were created during the merge operation.
	 * @return True if the operation was successful.
	 */
	static bool MergeMeshes(
		const TArray<UPrimitiveComponent*>& InMeshComponents,
		const FHoudiniPackageParams& InPackageParams,
		const FMeshMergingSettings& InSettings,
		UStaticMesh*& OutStaticMesh,
		FVector& OutMergedLocation,
		TArray<UObject*>* OutAssets=nullptr);

	static bool RetrieveMesh(
		const UStaticMeshComponent* InStaticMeshComponent,
		int32 InLODIndex,
		FMeshDescription& OutMeshDescription,
		bool bInPropagateVertexColours=false,
		bool bInApplyComponentTransform=true);

};
