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

#include "UnrealAnimationTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniInputObject.h"

#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "ReferenceSkeleton.h"
#include "Serialization/JsonSerializer.h"


bool
FUnrealAnimationTranslator::SetAnimationDataOnNode(
	UAnimSequence* Animation,
	HAPI_NodeId& NewNodeId)
{
	if (!IsValid(Animation))
		return false;

	return AddBoneTracksToNode(NewNodeId, Animation);

}


bool FUnrealAnimationTranslator::CreateInputNodeForAnimation(
	UAnimSequence* AnimSequence,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	const bool& bInputNodesCanBeDeleted)
{
	FString FinalInputNodeName = InputNodeName;

	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;

	{
		const FUnrealObjectInputOptions Options;
		return true;
	}
	return false;
}

bool
FUnrealAnimationTranslator::HapiCreateInputNodeForAnimation(
	UAnimSequence* Animation,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	bool ExportAllLODs,
	bool ExportSockets,
	bool ExportColliders,
	bool bInputNodesCanBeDeleted)
{
	// If we don't have a animation there's nothing to do.
	if (!IsValid(Animation))
		return false;

	// Input node name, defaults to InputNodeName, but can be changed by the new input system
	FString FinalInputNodeName = InputNodeName;

	// Find the node in new input system
	// Identifier will be the identifier for the entry created in this call of the function.
	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;
	{
		// Creates this input's identifier and input options
		const FUnrealObjectInputOptions Options;
		Identifier = FUnrealObjectInputIdentifier(Animation, Options, true);

		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId) && FUnrealObjectInputUtils::AreReferencedHAPINodesValid(Handle))
			{
				if (!bInputNodesCanBeDeleted)
				{
					// Make sure to prevent deletion of the input node if needed
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);
				}

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// We now need to create the nodes (since we couldn't find existing ones in the manager)
		// To do that, we can simply continue this function

		// Set InputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that InputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, InputNodeId))
				InputNodeId = -1;
		}
		else
		{
			InputNodeId = -1;
		}
	}

	// Node ID for the newly created node
	HAPI_NodeId NewNodeId = -1;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(
		FinalInputNodeName, NewNodeId, ParentNodeId), false);

	if (!FHoudiniEngineUtils::HapiCookNode(NewNodeId, nullptr, true))
		return false;

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;

	HAPI_NodeId PreviousInputNodeId = InputNodeId;

	// Update our input NodeId
	InputNodeId = NewNodeId;
	// Get our parent OBJ NodeID
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);

	// We have now created a valid new input node, delete the previous one
	if (PreviousInputNodeId >= 0)
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *InputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *InputNodeName);
		}
	}

	if (FUnrealAnimationTranslator::SetAnimationDataOnNode(Animation, NewNodeId))
	{
		// Commit the geo.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NewNodeId), false);

		//Create Output Node
		HAPI_NodeId OutputNodeId;
		HAPI_Result CreateResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("output"),
			TEXT("Output"), true, &OutputNodeId);

		// Create Point Wrangle node
		// This will convert matrix attributes to their proper type which HAPI doesn't seem to be translating correctly. 

		HAPI_NodeId AttribWrangleNodeId;
		{
			CreateResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("attribwrangle"),
				TEXT("convert_matrix"), true, &AttribWrangleNodeId);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				AttribWrangleNodeId, 0, NewNodeId, 0), false);

			// Construct a VEXpression to convert matrices.
			const FString FormatString = TEXT("3@transform = matrix3(f[]@in_transform);\n4@localtransform = matrix(f[]@in_localtransform); ");

			// Set the snippet parameter to the VEXpression.
			HAPI_ParmInfo ParmInfo;
			HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(AttribWrangleNodeId, "snippet", ParmInfo);
			if (ParmId != -1)
			{
				FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), AttribWrangleNodeId,
					TCHAR_TO_UTF8(*FormatString), ParmId, 0);
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"),
					*FHoudiniEngineUtils::GetErrorDescription());
			}
		}

		//Create Pack Node
		HAPI_NodeId PackNodeId;
		HAPI_Result CreatePackResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("pack"),
			TEXT("pack_data"), true, &PackNodeId);

		// Connect to point wrangle
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			PackNodeId, 0, AttribWrangleNodeId, 0), false);

		{
			const char* PackByNameOption = HAPI_UNREAL_PARAM_PACK_BY_NAME;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(), PackNodeId,
				PackByNameOption, 0, 1), false);
		}

		{
			const char* PackedFragmentsOption = HAPI_UNREAL_PARAM_PACKED_FRAGMENTS;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(), PackNodeId,
				PackedFragmentsOption, 0, 0), false);
		}

		HAPI_ParmInfo nameattributeParmInfo;
		HAPI_ParmId nameattributeParmId = FHoudiniEngineUtils::HapiFindParameterByName(PackNodeId, "nameattribute", nameattributeParmInfo);
		if (nameattributeParmId != -1)
		{
			FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), PackNodeId,
				"frame", nameattributeParmId, 0);
		}

		HAPI_ParmInfo transfer_attributesParmInfo;
		HAPI_ParmId transfer_attributesParmId = FHoudiniEngineUtils::HapiFindParameterByName(PackNodeId, "transfer_attributes", transfer_attributesParmInfo);
		if (transfer_attributesParmId != -1)
		{
			FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), PackNodeId,
				"time", transfer_attributesParmId, 0);
		}

		// Create Detail Wrangle node
		// This will copy the clipinfo dict to detail owner after pack (Pack doesn't want to transfer detail attributes) 

		HAPI_NodeId DetailWrangleNodeId;
		{
			CreateResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("attribwrangle"),
				TEXT("build_clipinfo"), true, &DetailWrangleNodeId);

			// Connect to Pack
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			DetailWrangleNodeId, 0, PackNodeId, 0), false);
			
			// Connect to Point Wrangle
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				DetailWrangleNodeId, 1, AttribWrangleNodeId, 0), false);

			// Set the class parameter to "detail"
			FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), DetailWrangleNodeId, "class", 0, 0);

			// Construct a VEXpression to convert matrices.
			const FString FormatString = TEXT(R"(d@clipinfo = detail(1, "clipinfo");)");

			// Set the snippet parameter to the VEXpression.
			HAPI_ParmInfo SnippetParmInfo;
			HAPI_ParmId SnippetParmId = FHoudiniEngineUtils::HapiFindParameterByName(DetailWrangleNodeId, "snippet", SnippetParmInfo);
			if (SnippetParmId != -1)
			{
				FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), DetailWrangleNodeId,
					TCHAR_TO_UTF8(*FormatString), SnippetParmId, 0);
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"),
					*FHoudiniEngineUtils::GetErrorDescription());
			}
		}

		// Connect output to detail wrangle (build_clipinfo)
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			OutputNodeId, 0, DetailWrangleNodeId, 0), false);

		FHoudiniEngineUtils::HapiCookNode(AttribWrangleNodeId, nullptr, true);
		FHoudiniEngineUtils::HapiCookNode(PackNodeId, nullptr, true);
	}

	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return true;
}

FTransform
FUnrealAnimationTranslator::GetCompSpaceTransformForBone(const FReferenceSkeleton& InSkel, int32 InBoneIdx)
{
	FTransform resultBoneTransform = InSkel.GetRefBonePose()[InBoneIdx];

	auto refBoneInfo = InSkel.GetRefBoneInfo();

	int32 Bone = InBoneIdx;
	while (Bone)
	{
		resultBoneTransform *= InSkel.GetRefBonePose()[refBoneInfo[Bone].ParentIndex];
		Bone = refBoneInfo[Bone].ParentIndex;
	}

	return resultBoneTransform;
}

FTransform
FUnrealAnimationTranslator::GetCompSpacePoseTransformForBoneMap(const TMap<int,FTransform>& BoneMap, const FReferenceSkeleton& InSkel, int32 InBoneIdx)
{
	FTransform resultBoneTransform = FTransform::Identity;
	if (BoneMap.Contains(InBoneIdx))
	{
		resultBoneTransform = BoneMap.FindChecked(InBoneIdx);
	}

	auto refBoneInfo = InSkel.GetRefBoneInfo();

	int32 Bone = InBoneIdx;
	while (Bone)
	{
		const int32 ParentIdx = refBoneInfo[Bone].ParentIndex;
		FTransform BoneTransform = FTransform::Identity;
		if (BoneMap.Contains(ParentIdx))
		{
			BoneTransform = BoneMap.FindChecked(ParentIdx);
		}

		resultBoneTransform *= BoneTransform;
		Bone = ParentIdx;
	}

	return resultBoneTransform;
}



FTransform
FUnrealAnimationTranslator::GetCompSpacePoseTransformForBone(const TArray<FTransform>& Bones, const FReferenceSkeleton& InSkel, int32 InBoneIdx)
{
	FTransform resultBoneTransform = Bones[InBoneIdx];

	auto refBoneInfo = InSkel.GetRefBoneInfo();

	int32 Bone = InBoneIdx;
	while (Bone)
	{
		//if root then use -90
		FTransform BoneTransform = Bones[refBoneInfo[Bone].ParentIndex];
		resultBoneTransform *= BoneTransform;
		Bone = refBoneInfo[Bone].ParentIndex;
	}

	return resultBoneTransform;
}


void
FUnrealAnimationTranslator::GetComponentSpaceTransforms(TArray<FTransform>& OutResult, const FReferenceSkeleton& InRefSkeleton)
{
	const int32 PoseNum = InRefSkeleton.GetRefBonePose().Num();
	OutResult.SetNum(PoseNum);

	for (int32 i = 0; i < PoseNum; i++)
	{
		OutResult[i] = FUnrealAnimationTranslator::GetCompSpaceTransformForBone(InRefSkeleton, i);

	}
}

FString FUnrealAnimationTranslator::GetBonePathForBone(const FReferenceSkeleton& InSkel, int32 InBoneIdx)
{
	FString BonePath;
	auto refBoneInfo = InSkel.GetRefBoneInfo();

	int32 Bone = InBoneIdx;
	TArray<int32> Indices;
	while (Bone)
	{
		Indices.Add(Bone);
		Bone = refBoneInfo[Bone].ParentIndex;
	}
	Indices.Add(0);

	Algo::Reverse(Indices);
	for (auto Idx : Indices)
	{
		BonePath += FString(TEXT("\x2f")) + refBoneInfo[Idx].Name.ToString();
	}

	return BonePath;
}


bool
FUnrealAnimationTranslator::AddBoneTracksToNode(HAPI_NodeId& NewNodeId, UAnimSequence* Animation)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2

	//read animation data
	UAnimSequence* AnimSequence = CastChecked<UAnimSequence>(Animation);
	const USkeleton* Skeleton = Animation->GetSkeleton();
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const FString SkeletonPathName = Skeleton->GetPathName();
	//
	const IAnimationDataModel* DataModel = Animation->GetDataModel();
	TArray<const FAnimatedBoneAttribute*> CustomAttributes;
	CustomAttributes.Reset();
	//
	TArray<FName> BonesTrackNames;
	DataModel->GetBoneTrackNames(BonesTrackNames);
	FFrameRate FrameRate = DataModel->GetFrameRate();
	const float FrameRateInterval = DataModel->GetFrameRate().AsInterval();

	TMap<FName, TArray<FTransform>> TrackMap;  //For Each Bone, Store Array of transforms (one for each key)
	int32 TotalTrackKeys = 0;
	//Iterate over BoneTracks
	for (FName TrackName : BonesTrackNames)
	{
		//const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[BoneIndex];
		TArray<FTransform> ThisTrackTransforms;
		DataModel->GetBoneTrackTransforms(TrackName, ThisTrackTransforms);
		FString DetailAttributeName = FString::Printf(TEXT("%s_transforms"), *TrackName.ToString());
		TotalTrackKeys = ThisTrackTransforms.Num();
		TArray<float> TrackFloatKeys;
		TrackFloatKeys.SetNumZeroed(TotalTrackKeys * 20);
		TrackMap.Add(TrackName, ThisTrackTransforms);
	}
	TArray<float> Points;
	TArray<float> LocalTransformData;  //for pCaptData property
	LocalTransformData.SetNumZeroed(4 * 4 * BonesTrackNames.Num() * (TotalTrackKeys + 1));  //4x4 matrix  // adding extra key for append

	TArray<float> WorldTransformData;
	WorldTransformData.SetNumZeroed(3 * 3 * BonesTrackNames.Num() * (TotalTrackKeys + 1));  //3x3 matrix

	TArray<float> PoseLocationData;
	TArray<float> PoseRotationData;
	TArray<FVector> PoseScaleData;
	PoseScaleData.SetNumZeroed(BonesTrackNames.Num() * (TotalTrackKeys + 1));

	TArray<float> LocalLocationData;
	TArray<float> LocalRotationData;
	TArray<FVector> LocalScaleData;
	LocalScaleData.SetNumZeroed(BonesTrackNames.Num() * (TotalTrackKeys + 1));


	TArray<int32> PrimIndices;
	int PrimitiveCount = 0;
	int BoneCount = BonesTrackNames.Num();
	int RefBoneCount = RefSkeleton.GetRefBoneInfo().Num();
	TArray<FString> BoneNames;
	TArray<FString> BonePaths;
	TArray<FString> UnrealSkeletonPaths;
	
	TArray< TMap<int, FTransform>> Frames;
	TMap<int, int> BoneIndexCounterMap;
	// AnimCurve data is stored in the fbx_custom_attributes dictionary which is stored on root joints for each frame
	// For any joint that is not the "root" join, the dict can be empty.
	// Note that we have to add an extra frame to the data for the topology frame.
	TArray<FString> FbxCustomAttributes;
	FbxCustomAttributes.SetNum(BonesTrackNames.Num() * (TotalTrackKeys+1));

	// Map Bone Indexes (from skeleton) to indexes for output data
	// since animated bones are typically a subset of the bones from the skeleton.
	int RootBoneIndex = INDEX_NONE;
	{
		int BoneNameCounter = 0;
		for (FName AnimBoneName : BonesTrackNames)
		{
			int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(AnimBoneName);
			BoneIndexCounterMap.Add(BoneIndex, BoneNameCounter);
			if (AnimBoneName == "root")
			{
				RootBoneIndex = BoneNameCounter;
			}
			BoneNameCounter++;
		}
	}

	for (int KeyFrame = 0; KeyFrame < TotalTrackKeys; KeyFrame++)
	{
		int32 BoneIndex = 0;
		TArray<FTransform> KeyBoneTransforms;
		//for each bone
		TMap<int, FTransform> SingleFrame;
		for (const FMeshBoneInfo& MeshBoneInfo : RefSkeleton.GetRefBoneInfo())
		{
			//add that tracks keys
			if (TrackMap.Contains(MeshBoneInfo.Name))
			{
				TArray<FTransform>& TransformArray = TrackMap[MeshBoneInfo.Name];
				FTransform KeyBoneTransform = TransformArray[KeyFrame];
				KeyBoneTransforms.Add(KeyBoneTransform);
				SingleFrame.Add(BoneIndex, KeyBoneTransform);
			}
			
			BoneIndex++;
		}

		if (RootBoneIndex != INDEX_NONE)
		{
			// Sample anim curves and store the data on the root bone for this keyframe.
			// Note that we're skipping over the topology frame (hence the Keyframe+1).
			const int DataIndex = (KeyFrame+1)*BoneCount + RootBoneIndex;
			const float SampleTime = KeyFrame * FrameRateInterval;

			// Sample anim curve data for the root bone and store it in a JSON object for easy serialization.
			TSharedPtr<FJsonObject> JSONObject = MakeShareable(new FJsonObject);

			// Sample all the curves for the current time value
			const FAnimationCurveData& CurveData = DataModel->GetCurveData();
			TArray<FFloatCurve> FloatCurves = CurveData.FloatCurves;
			for (const FFloatCurve& Curve : FloatCurves)
			{
				float Sample = Curve.Evaluate(SampleTime);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
				JSONObject->SetNumberField(Curve.GetName().ToString(), Sample);
#else
				JSONObject->SetNumberField(Curve.Name.DisplayName.ToString(), Sample);
#endif
			}
			
			FbxCustomAttributes[DataIndex] = FHoudiniEngineUtils::JSONToString(JSONObject);
		}
		
		Frames.Add(SingleFrame);
	}
	TArray<int32> FrameIndexData;
	TArray<float> TimeData;
	

	bool AppendFirstFrame = true;  //topology frame
	TMap<FName, FMeshBoneInfo> BoneInfos;

	for (const FMeshBoneInfo& MeshBoneInfo : Skeleton->GetReferenceSkeleton().GetRefBoneInfo())
	{
		BoneInfos.Add(MeshBoneInfo.Name, MeshBoneInfo);
	}
	
	//For Each Key Frame ( eg 1-60)
	// We'll be using the FrameOffset to inject the first frame twice for the MotionClip topology frame.
	int FrameOffset = 0;
	for (int FrameIndex = 0; FrameIndex < TotalTrackKeys+1; FrameIndex++)
	{
		if (FrameIndex > 0)
		{
			FrameOffset = -1; 
		}
		
		int32 BoneRefIndex = 0;
		int32 BoneDataIndex = 0;
		//for each bone
		for (FName KeyBoneName : BonesTrackNames)
		{
			BoneRefIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(KeyBoneName);
			const FMeshBoneInfo& MeshBoneInfo = BoneInfos.FindChecked(KeyBoneName);
			//add that tracks keys
			if (TrackMap.Contains(MeshBoneInfo.Name))
			{
				FTransform PoseTransform = FUnrealAnimationTranslator::GetCompSpacePoseTransformForBoneMap(Frames[FrameIndex+FrameOffset], RefSkeleton, BoneRefIndex);

				//alt
				FTransform LocalBoneTransform = FTransform::Identity;
				TMap<int, FTransform>& BoneTransformMap = Frames[FrameIndex+FrameOffset];
				if (!BoneTransformMap.Contains(BoneRefIndex))
				{
					LocalBoneTransform = BoneTransformMap.FindChecked(BoneRefIndex);
				}

				TArray<FTransform>& TransformArray = TrackMap[MeshBoneInfo.Name];

				FVector PoseLocation = PoseTransform.GetLocation();
				Points.Add(PoseLocation.X * 0.01);
				Points.Add(PoseLocation.Z * 0.01);  //swapping and scaling
				Points.Add(PoseLocation.Y * 0.01);
				PoseLocationData.Add(PoseLocation.X);
				PoseLocationData.Add(PoseLocation.Y);
				PoseLocationData.Add(PoseLocation.Z);
				
				// Adapted from FHoudiniEngineUtils::TranslateUnrealTransform
				// Convert the Component Space bone transform from LHCS (Z-Up) to RHCS (Y-Up), and add a 90 degree
				// rotation around the X-axis to generate data that matches the FBX Anim Importer SOP (otherwise we're
				// going to have round tripping nightmares.
				FQuat Q = PoseTransform.GetRotation();
				Q = FQuat(Q.X, Q.Z, Q.Y, -Q.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });

				// We need to do some swizzling and sign manipulation in order to convert our Quaternion to Euler representation
				FRotator PoseR = Q.Rotator();
				PoseR = FRotator(-PoseR.Pitch, PoseR.Yaw, -PoseR.Roll);
				PoseRotationData.Add(PoseR.Roll);
				PoseRotationData.Add(PoseR.Pitch);
				PoseRotationData.Add(PoseR.Yaw);
				PoseScaleData.Add(PoseTransform.GetScale3D());

				//Preconvert Rotation
				FQuat LocalQ = LocalBoneTransform.GetRotation();
				LocalQ = FQuat(LocalQ.X, LocalQ.Z, LocalQ.Y, -LocalQ.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
				//LocalQ = FQuat(LocalQ.X, LocalQ.Z, LocalQ.Y, -LocalQ.W);
				LocalBoneTransform.SetRotation(LocalQ);

				//Preconvert translation
				FVector LocalLocation = LocalBoneTransform.GetLocation();
				LocalLocationData.Add(LocalLocation.X);
				LocalLocationData.Add(LocalLocation.Z);
				LocalLocationData.Add(-LocalLocation.Y);
				//reassemble transform
				LocalBoneTransform.SetLocation(LocalLocation);
				
				{
					FName BoneName = RefSkeleton.GetBoneName(BoneRefIndex);
					int32 ParentBoneIndex = 0;
					if (BoneRefIndex > 0)
					{
						ParentBoneIndex = RefSkeleton.GetParentIndex(BoneRefIndex);
					}

					FTransform ParentPoseTransform = GetCompSpacePoseTransformForBoneMap(Frames[FrameIndex+FrameOffset], RefSkeleton, ParentBoneIndex);
					
					FQuat QBone = PoseTransform.GetRotation();
					QBone = FQuat(QBone.X, QBone.Z, QBone.Y, -QBone.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
					PoseLocation = PoseTransform.GetLocation();
					PoseLocation = FVector(PoseLocation.X, PoseLocation.Z, PoseLocation.Y);
					FTransform PoseConverted = FTransform(QBone, PoseLocation);
					

					// Get the pose-space transform for the PARENT BONE, and transform it to Houdini-space
					FQuat QParent = ParentPoseTransform.GetRotation();
					QParent = FQuat(QParent.X, QParent.Z, QParent.Y, -QParent.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
					FVector ParentLocation = ParentPoseTransform.GetLocation();
					ParentLocation = FVector(ParentLocation.X, ParentLocation.Z, ParentLocation.Y);

					FTransform ParentConverted = FTransform(QParent, ParentLocation);

					FMatrix FinalLocalTransform;
					if (BoneRefIndex == 0)
					{
						FinalLocalTransform = PoseConverted.ToMatrixWithScale() * 0.01;
					}
					else
					{
						// Compute the LocalTransform in our "houdini space"
						FinalLocalTransform = (PoseConverted * ParentConverted.Inverse()).ToMatrixWithScale();
					}

					//--------------------4x4LocalTransform 
					int32 LocalDataIndex = (FrameIndex) * BonesTrackNames.Num() * 4 * 4 + (BoneDataIndex * 4 * 4);

					LocalTransformData[LocalDataIndex + 0] = FinalLocalTransform.M[0][0];
					LocalTransformData[LocalDataIndex + 1] = FinalLocalTransform.M[0][1];
					LocalTransformData[LocalDataIndex + 2] = FinalLocalTransform.M[0][2];
					LocalTransformData[LocalDataIndex + 3] = FinalLocalTransform.M[0][3];
					LocalTransformData[LocalDataIndex + 4] = FinalLocalTransform.M[1][0];
					LocalTransformData[LocalDataIndex + 5] = FinalLocalTransform.M[1][1];
					LocalTransformData[LocalDataIndex + 6] = FinalLocalTransform.M[1][2];
					LocalTransformData[LocalDataIndex + 7] = FinalLocalTransform.M[1][3];
					LocalTransformData[LocalDataIndex + 8] = FinalLocalTransform.M[2][0];
					LocalTransformData[LocalDataIndex + 9] = FinalLocalTransform.M[2][1];
					LocalTransformData[LocalDataIndex + 10] = FinalLocalTransform.M[2][2];
					LocalTransformData[LocalDataIndex + 11] = FinalLocalTransform.M[2][3];
					LocalTransformData[LocalDataIndex + 12] = FinalLocalTransform.M[3][0];
					LocalTransformData[LocalDataIndex + 13] = FinalLocalTransform.M[3][1];
					LocalTransformData[LocalDataIndex + 14] = FinalLocalTransform.M[3][2];
					LocalTransformData[LocalDataIndex + 15] = FinalLocalTransform.M[3][3];
				}


				FTransform FirstRotationConversion = FTransform(FRotator(-90, 0, 0), FVector(0.0, 0.0, 0.0), FVector(1, 1, 1));
				FTransform BoneTransformConverted = LocalBoneTransform * FirstRotationConversion;

				FRotator LocalR = BoneTransformConverted.GetRotation().Rotator();
				LocalR.Roll += 180;
				LocalRotationData.Add(LocalR.Pitch);
				LocalRotationData.Add(LocalR.Roll);
				LocalRotationData.Add(LocalR.Yaw);

				FMatrix M44Pose = FRotationMatrix::Make(Q.Rotator()) * 0.01;

				int32 WorldDataIndex = (FrameIndex) * BonesTrackNames.Num() * 3 * 3 + (BoneDataIndex * 3 * 3);
				WorldTransformData[WorldDataIndex + 0] = M44Pose.M[0][0];
				WorldTransformData[WorldDataIndex + 1] = M44Pose.M[0][1];
				WorldTransformData[WorldDataIndex + 2] = M44Pose.M[0][2];
				WorldTransformData[WorldDataIndex + 3] = M44Pose.M[1][0];
				WorldTransformData[WorldDataIndex + 4] = M44Pose.M[1][1];
				WorldTransformData[WorldDataIndex + 5] = M44Pose.M[1][2];
				WorldTransformData[WorldDataIndex + 6] = M44Pose.M[2][0];
				WorldTransformData[WorldDataIndex + 7] = M44Pose.M[2][1];
				WorldTransformData[WorldDataIndex + 8] = M44Pose.M[2][2];

				// WorldTransformData[i * OutNames.Num() * 3 * 3] = BoneIndex;
				const float TimeValue = FrameIndex > 0 ? (FrameIndex-1) * FrameRateInterval : 0.f;
				if (BoneRefIndex > 0)
				{
					const int32 ParentDataIndex = BoneIndexCounterMap.FindChecked(MeshBoneInfo.ParentIndex);
					PrimIndices.Add((FrameIndex * BoneCount) + ParentDataIndex);
					PrimIndices.Add((FrameIndex * BoneCount) + BoneDataIndex);
					FrameIndexData.Add(FrameIndex);
					// The topology frame (FrameIndex = 0) and the first anim frame (FrameIndex = 1) Should have time = 0.
					TimeData.Add(TimeValue);
					PrimitiveCount++;
				}
				BoneNames.Add(MeshBoneInfo.Name.ToString());
				BonePaths.Add(GetBonePathForBone(RefSkeleton, BoneRefIndex));
				UnrealSkeletonPaths.Add(SkeletonPathName);
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Missing Bone"));
			}
			BoneDataIndex++;
		}

	}

	//----------------------------------------
	// Create part.
	//----------------------------------------
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = PrimIndices.Num();
	Part.faceCount = PrimitiveCount;
	Part.pointCount = Points.Num() / 3;
	Part.type = HAPI_PARTTYPE_MESH;

	HAPI_Result ResultPartInfo = FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0, &Part);

	//----------------------------------------
	// Create point attribute info.
	//----------------------------------------
	HAPI_AttributeInfo AttributeInfoPoint;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
	AttributeInfoPoint.count = Part.pointCount;
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	//Position Data
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint,
		(float*)Points.GetData(), 0, AttributeInfoPoint.count), false);

	//vertex list.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, PrimIndices.GetData(), 0, PrimIndices.Num()), false);

	//FaceCounts
	TArray<int32> FaceCounts;
	FaceCounts.SetNumUninitialized(Part.faceCount);
	for (int32 n = 0; n < Part.faceCount; n++)
		FaceCounts[n] = 2;


	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
		FaceCounts, NewNodeId, 0), false);


	// Create point attribute info for the bone name.
	HAPI_AttributeInfo BoneNameInfo;
	FHoudiniApi::AttributeInfo_Init(&BoneNameInfo);
	BoneNameInfo.count = Part.pointCount;
	BoneNameInfo.tupleSize = 1;
	BoneNameInfo.exists = true;
	BoneNameInfo.owner = HAPI_ATTROWNER_POINT;
	BoneNameInfo.storage = HAPI_STORAGETYPE_STRING;
	BoneNameInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	FString BoneNameAttributeName = FString::Printf(TEXT("name"));

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		TCHAR_TO_ANSI(*BoneNameAttributeName), &BoneNameInfo), false);

	FHoudiniHapiAccessor Accessor(NewNodeId, 0, TCHAR_TO_ANSI(*BoneNameAttributeName));
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(BoneNameInfo, BoneNames), false);

	//  Create point attribute info for the bone path.
	HAPI_AttributeInfo AttributeInfoName;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoName);
	AttributeInfoName.count = Part.pointCount;
	AttributeInfoName.tupleSize = 1;
	AttributeInfoName.exists = true;
	AttributeInfoName.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoName.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoName.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, "path", &AttributeInfoName), false);


	Accessor.Init(NewNodeId, 0, "path");
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoName, BonePaths), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// unreal_skeleton
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo UnrealSkeletonInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoName);
	UnrealSkeletonInfo.count = Part.pointCount;
	UnrealSkeletonInfo.tupleSize = 1;
	UnrealSkeletonInfo.exists = true;
	UnrealSkeletonInfo.owner = HAPI_ATTROWNER_POINT;
	UnrealSkeletonInfo.storage = HAPI_STORAGETYPE_STRING;
	UnrealSkeletonInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_SKELETON, &UnrealSkeletonInfo), false);


	Accessor.Init(NewNodeId, 0, HAPI_UNREAL_ATTRIB_SKELETON);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(UnrealSkeletonInfo, UnrealSkeletonPaths), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// Time
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo TimeInfo;
	FHoudiniApi::AttributeInfo_Init(&TimeInfo);
	TimeInfo.count = PrimitiveCount;
	TimeInfo.tupleSize = 1;
	TimeInfo.exists = true;
	TimeInfo.owner = HAPI_ATTROWNER_PRIM;
	TimeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	TimeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), 
		NewNodeId, 0, "time", &TimeInfo), false);

	//Position Data
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, "time", &TimeInfo, (float*)TimeData.GetData(), 0, TimeInfo.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// FrameIndex
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo FrameIndexInfo;
	FHoudiniApi::AttributeInfo_Init(&FrameIndexInfo);
	FrameIndexInfo.count = PrimitiveCount;
	FrameIndexInfo.tupleSize = 1;
	FrameIndexInfo.exists = true;
	FrameIndexInfo.owner = HAPI_ATTROWNER_PRIM;
	FrameIndexInfo.storage = HAPI_STORAGETYPE_INT;
	FrameIndexInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"frame", &FrameIndexInfo), false);

	//Position Data
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, "frame", &FrameIndexInfo,
		(int32*)FrameIndexData.GetData(), 0, FrameIndexInfo.count), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// LocalTransform (4x4)
	//---------------------------------------------------------------------------------------------------------------------

	HAPI_AttributeInfo LocalTransformInfo;
	FHoudiniApi::AttributeInfo_Init(&LocalTransformInfo);
	LocalTransformInfo.count = Part.pointCount;
	LocalTransformInfo.tupleSize = 1;
	LocalTransformInfo.exists = true;
	LocalTransformInfo.owner = HAPI_ATTROWNER_POINT;
	LocalTransformInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
	LocalTransformInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	LocalTransformInfo.totalArrayElements = LocalTransformData.Num();
	LocalTransformInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_MATRIX3;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"in_localtransform", &LocalTransformInfo), false);

	TArray<int32> SizesLocalTransformArray;
	for (int i = 0; i < Part.pointCount; i++)
	{
		SizesLocalTransformArray.Add(4 * 4);
	}
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), 
		NewNodeId, 0, "in_localtransform", &LocalTransformInfo, LocalTransformData.GetData(),
		LocalTransformData.Num(), SizesLocalTransformArray.GetData(), 0, SizesLocalTransformArray.Num()), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// WorldTransform3x3
	//---------------------------------------------------------------------------------------------------------------------

	HAPI_AttributeInfo WorldTransformInfo;
	FHoudiniApi::AttributeInfo_Init(&WorldTransformInfo);
	WorldTransformInfo.count = Part.pointCount;
	WorldTransformInfo.tupleSize = 1;
	WorldTransformInfo.exists = true;
	WorldTransformInfo.owner = HAPI_ATTROWNER_POINT;
	WorldTransformInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
	WorldTransformInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	WorldTransformInfo.totalArrayElements = WorldTransformData.Num();
	WorldTransformInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_MATRIX3;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), NewNodeId, 
		0, "in_transform", &WorldTransformInfo), false);

	TArray<int32> SizesWorldTransformArray;
	for (int i = 0; i < Part.pointCount; i++)
	{
		SizesWorldTransformArray.Add(3 * 3);
	}
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), 
		NewNodeId, 0, "in_transform", &WorldTransformInfo, WorldTransformData.GetData(),
		WorldTransformData.Num(), SizesWorldTransformArray.GetData(), 0, SizesWorldTransformArray.Num()), false);

	//--------------------------------------------------------------------------------------------------------------------- 
	// fbx_custom_attributes
	//---------------------------------------------------------------------------------------------------------------------
	HAPI_AttributeInfo CustomAttrsInfo;
	FHoudiniApi::AttributeInfo_Init(&CustomAttrsInfo);
	CustomAttrsInfo.count = Part.pointCount;
	CustomAttrsInfo.tupleSize = 1;
	CustomAttrsInfo.exists = true;
	CustomAttrsInfo.owner = HAPI_ATTROWNER_POINT;
	CustomAttrsInfo.storage = HAPI_STORAGETYPE_DICTIONARY;
	CustomAttrsInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"fbx_custom_attributes", &CustomAttrsInfo), false);

	Accessor.Init(NewNodeId, 0, "fbx_custom_attributes");
	Accessor.SetAttributeDictionary(CustomAttrsInfo, FbxCustomAttributes);


	//--------------------------------------------------------------------------------------------------------------------- 
	// clipinfo
	//---------------------------------------------------------------------------------------------------------------------

	// Output the JSON to a String
	const float AnimDuration = AnimSequence->GetPlayLength();
	const float FrameRateDecimal = FrameRate.AsDecimal();
	
	FString ClipInfoString = TEXT( R"({ "name":"{0}", "range":[{1}, {2}], "rate":{3}, "source_range":[{4}, {5}], "source_rate":{6} })" );
	ClipInfoString = FString::Format(*ClipInfoString, {
		AnimSequence->GetName(), // name
		0.f, // range[0]
		AnimDuration, // range[1]
		FrameRateDecimal, // rate
		0.f, // source_range[0]
		AnimDuration, // source_range[1]
		FrameRateDecimal // source_rate
	});
	
	HAPI_AttributeInfo ClipInfo;
	FHoudiniApi::AttributeInfo_Init(&ClipInfo);
	ClipInfo.count = 1; 
	ClipInfo.tupleSize = 1;
	ClipInfo.exists = true;
	ClipInfo.owner = HAPI_ATTROWNER_DETAIL;
	ClipInfo.storage = HAPI_STORAGETYPE_DICTIONARY;
	ClipInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, "clipinfo", &ClipInfo), false);

	TArray<FString> ClipInfoData = { ClipInfoString };

	Accessor.Init(NewNodeId, 0, "clipinfo");
	Accessor.SetAttributeDictionary(ClipInfo, ClipInfoData);;
	
#endif
	return true;
}


