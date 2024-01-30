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

#include "HAPI/HAPI_Common.h"
#include "HoudiniEnginePrivatePCH.h"

#include "Containers/UnrealString.h"

class FJsonObject;
struct FHoudiniGeoPartObject;
struct FHoudiniPackageParams;
class UHoudiniOutput;
class UAnimSequence;

struct HOUDINIENGINE_API FHoudiniAnimationTranslator
{
public:
	// Check whether the given PartId looks like a frame from a motion clip
	static bool IsMotionClipFrame(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, bool bRequiresLocalTransform);

	static bool CreateAnimSequenceFromOutput(UHoudiniOutput* InOutput,  const FHoudiniPackageParams& InPackageParams, UObject* InOuterComponent);
	static bool CreateAnimationFromMotionClip(UHoudiniOutput* InOutput, const TArray<FHoudiniGeoPartObject>& HGPOs, const FHoudiniPackageParams& InPackageParams, UObject* InOuterComponent);
	static UAnimSequence* CreateNewAnimation(FHoudiniPackageParams& InPackageParams, const FHoudiniGeoPartObject& HGPO, const FString& InSplitIdentifier);

private:
	static HAPI_PartId GetInstancedMeshPartID(const FHoudiniGeoPartObject& InstancerHGPO);
	static FString GetUnrealSkeletonPath(const TArray<FHoudiniGeoPartObject>& HGPOs);
	static bool GetClipInfo(const FHoudiniGeoPartObject& InstancerHGPO, float& OutFrameRate);
	static bool GetFbxCustomAttributes(int GeoId, int MeshPartId, int RootBoneIndex, TSharedPtr<FJsonObject>& OutJSONObject);
	
	
};

