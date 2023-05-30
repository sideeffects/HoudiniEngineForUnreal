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

struct FHoudiniOutputObject;
struct FHoudiniBakedOutputObject;
struct FHoudiniPackageParams;
struct FHoudiniAttributeResolver;
struct FHoudiniEngineOutputStats;
class UWorld;
class UPackage;
class UHoudiniLandscapeTargetLayerOutput;

struct HOUDINIENGINEEDITOR_API FHoudiniLandscapeBake
{
	static bool BakeLandscape(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		TArray<FHoudiniBakedOutput>& InBakedOutputs,
		bool bInReplaceActors,
		bool bInReplaceAssets,
		const FString& BakePath,
		FHoudiniEngineOutputStats& BakeStats,
		TSet<FString> & ClearedLayers);

	static bool BakeLandscapeLayer(
		FHoudiniPackageParams& PackageParams, 
		UHoudiniLandscapeTargetLayerOutput& LayerOutput, 
		TArray<UPackage*>& OutPackagesToSave,
		FHoudiniEngineOutputStats& BakeStats,
		TSet<FString>& ClearedLayers);

	static bool RenameCookedToBakedLandscapes(const TArray<UHoudiniOutput*>& InOutputs, bool bReplaceExistingActors);

	static ULandscapeLayerInfoObject* CreateBakedLandscapeLayerInfoObject(
		const FHoudiniPackageParams& PackageParams, 
		ALandscape* Landscape, 
		ULandscapeLayerInfoObject* LandscapeLayerInfoObject, 
		TArray<UPackage*>& OutPackagesToSave,
		FHoudiniEngineOutputStats& BakeStats);
};

