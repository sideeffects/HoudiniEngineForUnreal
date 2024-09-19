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
#include "UObject/NameTypes.h"

class UAnimSequence;
class FUnrealObjectInputHandle;
struct FReferenceSkeleton;

struct HOUDINIENGINE_API FUnrealAnimationTranslator
{
	public:
		static bool CreateInputNodeForAnimation(
			UAnimSequence* AnimSequence,
			HAPI_NodeId& InputNodeId,
			const FString& InputNodeName,
			FUnrealObjectInputHandle& OutHandle,
			const bool& bInputNodesCanBeDeleted);

		static bool HapiCreateInputNodeForAnimation(
			UAnimSequence* Animation,
			HAPI_NodeId& InputObjectNodeId,
			const FString& InputNodeName,
			FUnrealObjectInputHandle& OutHandle,
			bool ExportAllLODs,
			bool ExportSockets,
			bool ExportColliders,
			bool bInputNodesCanBeDeleted);

		static bool SetAnimationDataOnNode(
			UAnimSequence* Animation,
			HAPI_NodeId& NewNodeId);

		static bool AddBoneTracksToNode(HAPI_NodeId& NewNodeId, UAnimSequence* Animation);

		static void GetComponentSpaceTransforms(TArray<FTransform>& OutResult, const FReferenceSkeleton& InRefSkeleton);
		static FTransform GetCompSpaceTransformForBone(const FReferenceSkeleton& InSkel, int32 InBoneIdx);
		static FString GetBonePathForBone(const FReferenceSkeleton& InSkel, int32 InBoneIdx);
		static FTransform GetCompSpacePoseTransformForBone(const TArray<FTransform>& Bones, const FReferenceSkeleton& InSkel, int32 InBoneIdx);
		static FTransform GetCompSpacePoseTransformForBoneMap(const TMap<int, FTransform>& BoneMap, const FReferenceSkeleton& InSkel, int32 InBoneIdx);

};
