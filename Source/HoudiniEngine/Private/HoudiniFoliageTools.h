/*
* Copyright (c) <2022> Side Effects Software Inc.
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
#include "InstancedFoliage.h"

struct FHoudiniEngineOutputStats;
class UHoudiniOutput;
class UHoudiniAssetComponent;
struct FHoudiniOutputObjectIdentifier;
struct FHoudiniOutputObject;
class AInstancedFoliageActor;
class ULevel;
struct FHoudiniPackageParams;
class UStaticMesh;
class USceneComponent;
class UFoliageType;

class HOUDINIENGINE_API FHoudiniFoliageTools
{
public:
	// Create a new Foliage type using the InstancedStaticMesh as an Asset.
	static UFoliageType* CreateFoliageType(const FHoudiniPackageParams& Params, int OutputIndex, ULevel* DesiredLevel, UStaticMesh* InstancedStaticMesh);

	// Get the Foliage Type which uses the Instanced Static Mesh. If more than one is found, a warning is printed.
	static UFoliageType* GetFoliageType(const ULevel* DesiredLevel, const UStaticMesh* InstancedStaticMesh);

	// Get all Foliage Types which use the Instanced Static Mesh.
	static TArray<UFoliageType*> GetFoliageTypes(const ULevel* DesiredLevel, const UStaticMesh* InstancedStaticMesh);

	// Remove all FoliageInstances which have been associated with the component (FFoliageInstance::BaseComponent).
	static bool CleanupFoliageInstances(USceneComponent * BaseComponent);

	// Spawn the Foliage Instances into the given World/Foliage Type.
	static void SpawnFoliageInstance(UWorld* InWorld, UFoliageType* Settings, const TArray<FFoliageInstance>& PlacedInstances, bool InRebuildFoliageTree);

	// Return all Foliage Types used by the AInstancedFoliageActor.
	static TArray<UFoliageType*> GetFoliageTypes(AInstancedFoliageActor* IFA);

	// Return all FFoliageInfo which reference the FoliageType in the given world.
	static TArray<FFoliageInfo*> GetAllFoliageInfo(UWorld * World, UFoliageType * FoliageType);

	// Remove all references to the foliage in the world.
	static void RemoveFoliageType(UWorld * World, UFoliageType* FoliageType, const UHoudiniAssetComponent* HoudiniAssetComponent);

};

