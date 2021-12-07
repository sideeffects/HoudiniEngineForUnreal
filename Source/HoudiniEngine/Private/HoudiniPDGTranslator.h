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

#pragma once

#include "CoreMinimal.h"

class UHoudiniPDGAssetLink;
class UHoudiniOutput;
class AActor;
class UTOPNode;
class ALandscapeProxy;

enum class EHoudiniOutputType : uint8;

struct FHoudiniPackageParams;
struct FTOPWorkResultObject;
struct FHoudiniOutputObjectIdentifier;
struct FHoudiniInstancedOutputPartData;
struct FHoudiniLandscapeExtent;
struct FHoudiniLandscapeReferenceLocation;
struct FHoudiniLandscapeTileSizeInfo;

struct HOUDINIENGINE_API FHoudiniPDGTranslator
{
	public:
		// Create/update assets/geometry for all PDG outputs of InWorkResultObject. This will use
		// InWorkResultObject.FilePath to load the BGEO file and update/build outputs.
		static bool CreateAllResultObjectsForPDGWorkItem(
			UHoudiniPDGAssetLink* InAssetLink,
			UTOPNode* InTOPNode,
			FTOPWorkResultObject& InWorkResultObject,
			const FHoudiniPackageParams& InPackageParams,
			TArray<EHoudiniOutputType> InOutputTypesToProcess={},
			bool bInTreatExistingMaterialsAsUpToDate=false);
	
		static bool LoadExistingAssetsAsResultObjectsForPDGWorkItem(
			UHoudiniPDGAssetLink* InAssetLink,
			UTOPNode* InTOPNode,
			FTOPWorkResultObject& InWorkResultObject,
			const FHoudiniPackageParams& InPackageParams,
			TArray<UHoudiniOutput*>& InOutputs,
			TArray<EHoudiniOutputType> InOutputTypesToProcess={},
			const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* InPreBuiltInstancedOutputPartData=nullptr);

		// Use the relevant translators to create assets/geometry for all PDG outputs (InOutputs).
		// InOuterComponent is the component to attach the created output objects/components to.
		static bool CreateAllResultObjectsFromPDGOutputs(
			TArray<UHoudiniOutput*>& InOutputs,
			const FHoudiniPackageParams& InPackageParams,
			UObject* InOuterComponent,
			FHoudiniLandscapeExtent& CachedLandscapeExtent,
			FHoudiniLandscapeReferenceLocation& CachedLandscapeRefLoc,
			FHoudiniLandscapeTileSizeInfo& CachedLandscapeSizeInfo,
			TSet<FString>& ClearedLandscapeLayers,
			TArray<ALandscapeProxy*> AllInputLandscapes,
			TArray<ALandscapeProxy*> InputLandscapesToUpdate,
			UHoudiniPDGAssetLink* const InAssetLink,
			TArray<EHoudiniOutputType> InOutputTypesToProcess={},
			bool bInTreatExistingMaterialsAsUpToDate=false,
			bool bInOnlyUseExistingAssets=false,
			const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* InPreBuiltInstancedOutputPartData=nullptr
			);
};

