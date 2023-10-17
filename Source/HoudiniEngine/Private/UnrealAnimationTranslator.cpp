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

#include "UObject/TextProperty.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniInputObject.h"

#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "ReferenceSkeleton.h"


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
	const bool bUseRefCountedInputSystem = FUnrealObjectInputRuntimeUtils::IsRefCountedInputSystemEnabled();
	FString FinalInputNodeName = InputNodeName;

	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;

	if (bUseRefCountedInputSystem)
	{
		bool bDefaultImportAsReference = false;
		bool bDefaultImportAsReferenceRotScaleEnabled = false;
		const FUnrealObjectInputOptions Options(
			bDefaultImportAsReference,
			bDefaultImportAsReferenceRotScaleEnabled,
			false,
			false,
			false
		);


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
	const bool& ExportAllLODs /*=false*/,
	const bool& ExportSockets /*=false*/,
	const bool& ExportColliders /*=false*/,
	const bool& bInputNodesCanBeDeleted /*=true*/)
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
	const bool bUseRefCountedInputSystem = FUnrealObjectInputRuntimeUtils::IsRefCountedInputSystemEnabled();
	if (bUseRefCountedInputSystem)
	{
		// Creates this input's identifier and input options
		bool bDefaultImportAsReference = false;
		bool bDefaultImportAsReferenceRotScaleEnabled = false;
		const FUnrealObjectInputOptions Options(
			bDefaultImportAsReference,
			bDefaultImportAsReferenceRotScaleEnabled,
			false,
			false,
			false);

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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), NewNodeId), false);


		// Create a node.
		//HAPI_NodeId box_node_id;
		//FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), ParentNodeId, "box", nullptr, true, &box_node_id);
		//result = HAPI_CreateNode(
		//	hapiTestSession, network_node_id, "box", nullptr, true, &box_node_id);
		//HAPI_TEST_ASSERT(result == HAPI_RESULT_SUCCESS);

		//Create Output Node
		HAPI_NodeId OutputNodeId;
		HAPI_Result CreateResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("output"),
			TEXT("Output"), true, &OutputNodeId);



		//Create Wrangle to convert matrices
		HAPI_NodeId AttribWrangleNodeId;
		CreateResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("attribwrangle"),
			TEXT("convert_matrix"), true, &AttribWrangleNodeId);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			AttribWrangleNodeId, 0, NewNodeId, 0), false);

		// Construct a VEXpression to convert matrices.
		const FString FormatString = TEXT("3@transform = matrix3(f[]@worldtransform);4@localtransform = matrix(f[]@ltransform); ");
		//std::string VEXpression = TCHAR_TO_UTF8(*FString::Format(*FormatString,{ AttrName, PathName }));

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

		//Create Pack Node
		HAPI_NodeId PackNodeId;
		HAPI_Result CreatePackResult = FHoudiniEngineUtils::CreateNode(InputObjectNodeId, TEXT("pack"),
			TEXT("pack_data"), true, &PackNodeId);

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			OutputNodeId, 0, PackNodeId, 0), false);

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

		//HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		//	FHoudiniEngine::Get().GetSession(), AttribWrangleNodeId), false);

		//HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		//	FHoudiniEngine::Get().GetSession(), PackNodeId), false);

		//HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		//	FHoudiniEngine::Get().GetSession(), OutputNodeId), false);


		FHoudiniEngineUtils::HapiCookNode(AttribWrangleNodeId, nullptr, true);
		FHoudiniEngineUtils::HapiCookNode(PackNodeId, nullptr, true);

	}

	if (bUseRefCountedInputSystem)
	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}



	return true;
}

FTransform
FUnrealAnimationTranslator::GetCompSpaceTransformForBone(const FReferenceSkeleton& InSkel, const int32& InBoneIdx)
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
FUnrealAnimationTranslator::GetCompSpacePoseTransformForBone(const TArray<FTransform>& Bones, const FReferenceSkeleton& InSkel, const int32& InBoneIdx)
{
	//FTransform resultBoneTransform = InSkel.GetRefBonePose()[InBoneIdx];
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

FString FUnrealAnimationTranslator::GetBonePathForBone(const TArray<FTransform>& Bones, const FReferenceSkeleton& InSkel, const int32& InBoneIdx)
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

	//BonePath += FString(TEXT("\x2f")) + refBoneInfo[0].Name.ToString();
	return BonePath;
}


bool FUnrealAnimationTranslator::AddBoneTracksToNode(HAPI_NodeId& NewNodeId, UAnimSequence* Animation)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2

	//read animation data
	UAnimSequence* AnimSequence = CastChecked<UAnimSequence>(Animation);
	USkeleton* Skeleton = Animation->GetSkeleton();
	USkeletalMesh* PreviewMesh = Skeleton ? Skeleton->GetAssetPreviewMesh(AnimSequence) : NULL;
	const FReferenceSkeleton& RefSkeleton = PreviewMesh->GetRefSkeleton();
	TArray<FTransform> ComponentSpaceTransforms;
	FUnrealAnimationTranslator::GetComponentSpaceTransforms(ComponentSpaceTransforms, RefSkeleton);  // In BindPose

	const IAnimationDataModel* DataModel = Animation->GetDataModel();
	TArray<const FAnimatedBoneAttribute*> CustomAttributes;
	CustomAttributes.Reset();

	//Working :-)
	TArray<FName> OutNames;
	DataModel->GetBoneTrackNames(OutNames);
	float FrameRate = DataModel->GetFrameRate().AsInterval();

	TMap<FName, TArray<FTransform>> TrackMap;  //For Each Bone, Store Array of transforms (one for each key)
	int32 TotalTrackKeys = 0;
	//Iterate over BoneTracks
	for (FName TrackName : OutNames)
	{
		//const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[BoneIndex];
		TArray<FTransform> ThisTrackTransforms;
		DataModel->GetBoneTrackTransforms(TrackName, ThisTrackTransforms);
		FString DetailAttributeName = FString::Printf(TEXT("%s_transforms"), *TrackName.ToString());
		TotalTrackKeys = ThisTrackTransforms.Num();
		TArray<float> TrackFloatKeys;
		TrackFloatKeys.SetNumZeroed(TotalTrackKeys * 20);
		int FrameIndex = 0;
		/*for (FTransform transform : ThisTrackTransforms)
		{
		}*/
		TrackMap.Add(TrackName, ThisTrackTransforms);
		FrameIndex++;
	}
	TArray<float> Points;
	TArray<float> LocalTransformData;  //for pCaptData property
	LocalTransformData.SetNumZeroed(4 * 4 * OutNames.Num() * (TotalTrackKeys + 1));  //4x4 matrix  // adding extra key for append

	TArray<float> WorldTransformData;
	WorldTransformData.SetNumZeroed(3 * 3 * OutNames.Num() * (TotalTrackKeys + 1));  //3x3 matrix

	TArray<float> PoseLocationData;
	//PoseLocationData.SetNumZeroed(3 * OutNames.Num() * (TotalTrackKeys));
	TArray<float> PoseRotationData;
	//PoseRotationData.SetNumZeroed(OutNames.Num() * (TotalTrackKeys));
	TArray<FVector> PoseScaleData;
	PoseScaleData.SetNumZeroed(OutNames.Num() * (TotalTrackKeys + 1));

	TArray<float> LocalLocationData;
	//LocalLocationData.SetNumZeroed(OutNames.Num() * (TotalTrackKeys));
	TArray<float> LocalRotationData;
	//LocalRotationData.SetNumZeroed(OutNames.Num() * (TotalTrackKeys));
	TArray<FVector> LocalScaleData;
	LocalScaleData.SetNumZeroed(OutNames.Num() * (TotalTrackKeys + 1));


	TArray<int32> PrimIndices;
	int PrimitiveCount = 0;
	int BoneCount = OutNames.Num();
	int RefBoneCount = RefSkeleton.GetRefBoneInfo().Num();
	TArray<FString> BoneNames;
	TArray<FString> BonePaths;
	//Points.SetNumZeroed(OutNames.Num() * TotalTrackKeys * 3);

	// 
	TMap<int, TArray<FTransform>> FrameMap;  //For Each KeyFrame, Store Array of transforms (one for each bone)
	for (int KeyFrame = 0; KeyFrame < TotalTrackKeys; KeyFrame++)
	{
		int32 BoneIndex = 0;
		TArray<FTransform> KeyBoneTransforms;
		//for each bone
		for (const FMeshBoneInfo& MeshBoneInfo : RefSkeleton.GetRefBoneInfo())
		{
			//add that tracks keys
			if (TrackMap.Contains(MeshBoneInfo.Name))
			{
				TArray<FTransform>& TransformArray = TrackMap[MeshBoneInfo.Name];
				FTransform KeyBoneTransform = TransformArray[KeyFrame];
				KeyBoneTransforms.Add(KeyBoneTransform);

			}
			BoneIndex++;
		}
		FrameMap.Add(KeyFrame, KeyBoneTransforms);
	}
	TArray<int32> FrameIndexData;
	TArray<float> TimeData;
	//TimeData.SetNumZeroed(OutNames.Num()* (TotalTrackKeys));
	//TimeData.SetNumZeroed(PrimitiveCount);

	bool AppendFirstFrame = true;
	if (AppendFirstFrame)
	{
		//For Each Key Frame ( eg 1-60)
		for (int i = 0; i < 1; i++)
		{
			int32 BoneIndex = 0;

			//for each bone
			for (const FMeshBoneInfo& MeshBoneInfo : RefSkeleton.GetRefBoneInfo())
			{
				//add that tracks keys
				if (TrackMap.Contains(MeshBoneInfo.Name))
				{
					UE_LOG(LogTemp, Log, TEXT("[CLS::AddBoneTracksToNode] Calculating Pose Transform for bone: %s"), *MeshBoneInfo.Name.ToString());
					//check BoneIndex is correct
					FTransform PoseTransform = FUnrealAnimationTranslator::GetCompSpacePoseTransformForBone(FrameMap[i], RefSkeleton, BoneIndex);

					//alt
					const TArray<FTransform>& LocalBones = FrameMap[i];
					FTransform LocalBoneTransform = LocalBones[BoneIndex];

					TArray<FTransform>& TransformArray = TrackMap[MeshBoneInfo.Name];

					//FVector Location = TransformArray[i].GetLocation();
					FVector PoseLocation = PoseTransform.GetLocation();
					Points.Add(PoseLocation.X * 0.01);
					Points.Add(PoseLocation.Z * 0.01);  //swapping and scaling
					Points.Add(PoseLocation.Y * 0.01);
					PoseLocationData.Add(PoseLocation.X);
					PoseLocationData.Add(PoseLocation.Y);
					PoseLocationData.Add(PoseLocation.Z);
					PoseScaleData.Add(PoseTransform.GetScale3D());


					//--------------------4x4LocalTransform Matrix 
					int32 LocalOffSet = i * OutNames.Num() * 4 * 4 + (BoneIndex * 4 * 4);

					//Preconvert Rotation
					FQuat LocalQ = LocalBoneTransform.GetRotation();
					LocalQ = FQuat(LocalQ.X, LocalQ.Z, LocalQ.Y, -LocalQ.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
					//LocalBoneTransform.SetRotation(LocalQ);
					LocalBoneTransform.SetRotation(FQuat::MakeFromEuler({ 0.f, 0.f, 0.f }));//first frame is zero

					//Preconvert translation
					FVector LocalLocation = LocalBoneTransform.GetLocation();
					LocalLocationData.Add(LocalLocation.X * 0.01);
					LocalLocationData.Add(LocalLocation.Z * 0.01);
					LocalLocationData.Add(LocalLocation.Y * 0.01);
					//reassemble transform
					//LocalBoneTransform.SetLocation(LocalLocation);
					LocalBoneTransform.SetLocation(FVector::ZeroVector);//first frame is zero


					FMatrix M44Local = LocalBoneTransform.ToMatrixWithScale();
					//FMatrix M44 = transform.ToMatrixWithScale();
					FMatrix M44LocalInverse = M44Local.Inverse();  //see pCaptData property
					//TODO Validate inverse and conversion

					int32 row = 0;
					int32 col = 0;
					for (int32 idx = 0; idx < 16; idx++)
					{
						//XFormsData[16 * BoneIndex + i] = M44.M[row][col];
						LocalTransformData[LocalOffSet + idx] = M44Local.M[row][col];
						col++;
						if (col > 3)
						{
							row++;
							col = 0;
						}
					}
					//-------------------4x4LocalTransform Matrix 

					//FQuat LocalQ = LocalBoneTransform.GetRotation();
					//LocalQ = FQuat(LocalQ.X, LocalQ.Z, LocalQ.Y, -LocalQ.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });

					//LocalBoneTransform.SetRotation(LocalQ);

					//FMatrix M44Local = FRotationMatrix::Make(LocalQ.Rotator());
					//LocalTransformData[i * OutNames.Num() * 4 * 4 + (BoneIndex * 4 * 4)] = BoneIndex;


					// ----------------------------------------------------------------------------------------------------
					// <VA>
					// ----------------------------------------------------------------------------------------------------
					// Adapted from FHoudiniEngineUtils::TranslateUnrealTransform
					// Convert the Component Space bone transform from LHCS (Z-Up) to RHCS (Y-Up), and add a 90 degree
					// rotation around the X-axis to generate data that matches the FBX Anim Importer SOP (otherwise we're
					// going to have round tripping nightmares.
					FQuat PoseQ = PoseTransform.GetRotation();
					PoseQ = FQuat(PoseQ.X, PoseQ.Z, PoseQ.Y, -PoseQ.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
					// We need to do some swizzling and sign manipulation in order to convert our Quaternion to Euler representation

					FRotator PoseR = PoseQ.Rotator();
					PoseR = FRotator(-PoseR.Pitch, PoseR.Yaw, -PoseR.Roll);
					PoseRotationData.Add(PoseR.Roll);
					PoseRotationData.Add(PoseR.Pitch);
					PoseRotationData.Add(PoseR.Yaw);

					// ----------------------------------------------------------------------------------------------------
					// <VA>
					// ----------------------------------------------------------------------------------------------------
					// We convert our quaternion, but don't take into account the Euler swizzle ? Math is weird.
					FMatrix M44Pose = FRotationMatrix::Make(PoseQ.Rotator());
					//TODO Validate inverse and conversion

					//Transpose Matrix
					//int32 FrameIndex = 0;
						//XFormsData[16 * BoneIndex + i] = M44.M[row][col];
						//LocalTransformData[OffSet + idx] = M44.M[row][col];
						//FrameIndex++;
					int32 WorldOffSet = i * OutNames.Num() * 3 * 3 + (BoneIndex * 3 * 3);
					UE_LOG(LogTemp, Log, TEXT("[CLS::AddBoneTracksToNode] BoneIndex: %d, WorldOffset: %d"), BoneIndex, WorldOffSet);
					UE_LOG(LogTemp, Log, TEXT("[CLS::AddBoneTracksToNode] WorldTransformData: %s"), *M44Pose.ToString());
					WorldTransformData[WorldOffSet + 0] = M44Pose.M[0][0];
					WorldTransformData[WorldOffSet + 1] = M44Pose.M[0][1];
					WorldTransformData[WorldOffSet + 2] = M44Pose.M[0][2];
					WorldTransformData[WorldOffSet + 3] = M44Pose.M[1][0];
					WorldTransformData[WorldOffSet + 4] = M44Pose.M[1][1];
					WorldTransformData[WorldOffSet + 5] = M44Pose.M[1][2];
					WorldTransformData[WorldOffSet + 6] = M44Pose.M[2][0];
					WorldTransformData[WorldOffSet + 7] = M44Pose.M[2][1];
					WorldTransformData[WorldOffSet + 8] = M44Pose.M[2][2];

					if (BoneIndex > 0)
					{
						PrimIndices.Add((i * BoneCount) + MeshBoneInfo.ParentIndex);
						PrimIndices.Add((i * BoneCount) + BoneIndex);
						FrameIndexData.Add(i);
						TimeData.Add(i * FrameRate);
						PrimitiveCount++;
					}
					BoneNames.Add(MeshBoneInfo.Name.ToString());
					BonePaths.Add(FUnrealAnimationTranslator::GetBonePathForBone(FrameMap[i], RefSkeleton, BoneIndex));
					BoneIndex++;
				}
				else
				{
					HOUDINI_LOG_WARNING(TEXT("Missing Bone"));
				}
			}
		}
	}//AppendFirstFrame

	// 
	//For Each Key Frame ( eg 1-60)
	for (int i = 0; i < TotalTrackKeys; i++)
	{
		int32 BoneIndex = 0;

		//for each bone
		for (const FMeshBoneInfo& MeshBoneInfo : RefSkeleton.GetRefBoneInfo())
		{
			//add that tracks keys
			if (TrackMap.Contains(MeshBoneInfo.Name))
			{
				UE_LOG(LogTemp, Log, TEXT("[CLS::AddBoneTracksToNode] Calculating Pose Transform for bone: %s"), *MeshBoneInfo.Name.ToString());
				//check BoneIndex is correct
				FTransform PoseTransform = GetCompSpacePoseTransformForBone(FrameMap[i], RefSkeleton, BoneIndex);

				//alt
				const TArray<FTransform>& LocalBones = FrameMap[i];
				FTransform LocalBoneTransform = LocalBones[BoneIndex];

				TArray<FTransform>& TransformArray = TrackMap[MeshBoneInfo.Name];

				//FVector Location = TransformArray[i].GetLocation();
				FVector PoseLocation = PoseTransform.GetLocation();
				Points.Add(PoseLocation.X * 0.01);
				Points.Add(PoseLocation.Z * 0.01);  //swapping and scaling
				Points.Add(PoseLocation.Y * 0.01);
				PoseLocationData.Add(PoseLocation.X);
				PoseLocationData.Add(PoseLocation.Y);
				PoseLocationData.Add(PoseLocation.Z);

				// ----------------------------------------------------------------------------------------------------
				// <VA>
				// ----------------------------------------------------------------------------------------------------
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

				//--------------------4x4LocalTransform 
				int32 LocalOffSet = (i + 1) * OutNames.Num() * 4 * 4 + (BoneIndex * 4 * 4);//adding 1 because of firstframe append

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

				if (BoneIndex == 0)
				{
					LocalBoneTransform.SetScale3D(FVector(0.01, 0.01, 0.01));
				}
				else
				{
					LocalBoneTransform.SetScale3D(FVector(1, 1, 1));
				}

				FMatrix M44Local = LocalBoneTransform.ToMatrixWithScale();
				FMatrix M44LocalInverse = M44Local.Inverse();  //see pCaptData property

				int32 row = 0;
				int32 col = 0;
				for (int32 idx = 0; idx < 16; idx++)
				{
					LocalTransformData[LocalOffSet + idx] = M44Local.M[row][col];
					col++;
					if (col > 3)
					{
						row++;
						col = 0;
					}
				}
				if (true)
				{
					FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
					int32 ParentBoneIndex = 0;
					if (BoneIndex > 0)
					{
						ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
						FName ParentBoneName = RefSkeleton.GetBoneName(ParentBoneIndex);
					}
					FTransform ParentPoseTransform = GetCompSpacePoseTransformForBone(FrameMap[i], RefSkeleton, ParentBoneIndex);

					//FTransform PoseTransform = GetCompSpacePoseTransformForBone(FrameMap[i], RefSkeleton, BoneIndex);
					FQuat QBone = PoseTransform.GetRotation();
					QBone = FQuat(QBone.X, QBone.Z, QBone.Y, -QBone.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
					PoseLocation = PoseTransform.GetLocation();
					PoseLocation = FVector(PoseLocation.X, PoseLocation.Z, PoseLocation.Y);
					FTransform PoseConverted = FTransform(QBone, PoseLocation);

					//FMatrix BonePose = FRotationMatrix::Make(QBone.Rotator());

					// Get the pose-space transform for the PARENT BONE, and transform it to Houdini-space
					//FTransform ParentPoseTransform = GetCompSpacePoseTransformForBone(FrameMap[i], RefSkeleton, BoneIndex);
					FQuat QParent = ParentPoseTransform.GetRotation();
					QParent = FQuat(QParent.X, QParent.Z, QParent.Y, -QParent.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
					FVector ParentLocation = ParentPoseTransform.GetLocation();
					ParentLocation = FVector(ParentLocation.X, ParentLocation.Z, ParentLocation.Y);
					//FMatrix ParentPose = FRotationMatrix::Make(QParent.Rotator());

					FTransform ParentConverted = FTransform(QParent, ParentLocation);


					//FMatrix M44Local = LocalBoneTransform.ToMatrixWithScale();

					FMatrix FinalLocalTransform;
					if (BoneIndex == 0)
					{
						FinalLocalTransform = PoseConverted.ToMatrixWithScale() * 0.01;
					}
					else
					{
						// Compute the LocalTransform in our "houdini space"
						FinalLocalTransform = (PoseConverted * ParentConverted.Inverse()).ToMatrixWithScale();
					}


					LocalTransformData[LocalOffSet + 0] = FinalLocalTransform.M[0][0];
					LocalTransformData[LocalOffSet + 1] = FinalLocalTransform.M[0][1];
					LocalTransformData[LocalOffSet + 2] = FinalLocalTransform.M[0][2];
					LocalTransformData[LocalOffSet + 3] = FinalLocalTransform.M[0][3];
					LocalTransformData[LocalOffSet + 4] = FinalLocalTransform.M[1][0];
					LocalTransformData[LocalOffSet + 5] = FinalLocalTransform.M[1][1];
					LocalTransformData[LocalOffSet + 6] = FinalLocalTransform.M[1][2];
					LocalTransformData[LocalOffSet + 7] = FinalLocalTransform.M[1][3];
					LocalTransformData[LocalOffSet + 8] = FinalLocalTransform.M[2][0];
					LocalTransformData[LocalOffSet + 9] = FinalLocalTransform.M[2][1];
					LocalTransformData[LocalOffSet + 10] = FinalLocalTransform.M[2][2];
					LocalTransformData[LocalOffSet + 11] = FinalLocalTransform.M[2][3];
					LocalTransformData[LocalOffSet + 12] = FinalLocalTransform.M[3][0];
					LocalTransformData[LocalOffSet + 13] = FinalLocalTransform.M[3][1];
					LocalTransformData[LocalOffSet + 14] = FinalLocalTransform.M[3][2];
					LocalTransformData[LocalOffSet + 15] = FinalLocalTransform.M[3][3];

					//ZZZ Test Undo
					TMap<FName, FTransform> ZZZComponentSpaceTransforms;
					ZZZComponentSpaceTransforms.Add(BoneName, FTransform(FinalLocalTransform));
				}



				FTransform FirstRotationConversion = FTransform(FRotator(-90, 0, 0), FVector(0.0, 0.0, 0.0), FVector(1, 1, 1));
				FTransform BoneTransformConverted = LocalBoneTransform * FirstRotationConversion;

				FRotator LocalR = BoneTransformConverted.GetRotation().Rotator();
				LocalR.Roll += 180;
				LocalRotationData.Add(LocalR.Pitch);
				LocalRotationData.Add(LocalR.Roll);
				LocalRotationData.Add(LocalR.Yaw);


				// ----------------------------------------------------------------------------------------------------
				// <VA>
				// ----------------------------------------------------------------------------------------------------
				// We convert our quaternion, but don't take into account the Euler swizzle ? Math is weird.
				FMatrix M44Pose = FRotationMatrix::Make(Q.Rotator()) * 0.01;



				//TODO Validate inverse and conversion

				//Transpose Matrix
				//int32 FrameIndex = 0;
					//XFormsData[16 * BoneIndex + i] = M44.M[row][col];
					//LocalTransformData[OffSet + idx] = M44.M[row][col];
					//FrameIndex++;
				int32 WorldOffSet = (i + 1) * OutNames.Num() * 3 * 3 + (BoneIndex * 3 * 3);//adding 1 because of firstframe append
				UE_LOG(LogTemp, Log, TEXT("[CLS::AddBoneTracksToNode] BoneIndex: %d, WorldOffset: %d"), BoneIndex, WorldOffSet);
				UE_LOG(LogTemp, Log, TEXT("[CLS::AddBoneTracksToNode] WorldTransformData: %s"), *M44Pose.ToString());
				WorldTransformData[WorldOffSet + 0] = M44Pose.M[0][0];
				WorldTransformData[WorldOffSet + 1] = M44Pose.M[0][1];
				WorldTransformData[WorldOffSet + 2] = M44Pose.M[0][2];
				WorldTransformData[WorldOffSet + 3] = M44Pose.M[1][0];
				WorldTransformData[WorldOffSet + 4] = M44Pose.M[1][1];
				WorldTransformData[WorldOffSet + 5] = M44Pose.M[1][2];
				WorldTransformData[WorldOffSet + 6] = M44Pose.M[2][0];
				WorldTransformData[WorldOffSet + 7] = M44Pose.M[2][1];
				WorldTransformData[WorldOffSet + 8] = M44Pose.M[2][2];

				// WorldTransformData[i * OutNames.Num() * 3 * 3] = BoneIndex;
				if (BoneIndex > 0)
				{
					PrimIndices.Add((i * BoneCount) + MeshBoneInfo.ParentIndex);
					PrimIndices.Add((i * BoneCount) + BoneIndex);
					FrameIndexData.Add(i + 1); //already appended a frame
					TimeData.Add(i * FrameRate);
					PrimitiveCount++;
				}
				BoneNames.Add(MeshBoneInfo.Name.ToString());
				BonePaths.Add(GetBonePathForBone(FrameMap[i], RefSkeleton, BoneIndex));
				BoneIndex++;
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Missing Bone"));
			}
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

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	//Position Data
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint,
		(float*)Points.GetData(), 0, AttributeInfoPoint.count), false);

	//vertex list.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(
		FHoudiniEngine::Get().GetSession(),
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

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		BoneNames, NewNodeId, 0, TCHAR_TO_ANSI(*BoneNameAttributeName), BoneNameInfo), false);

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


	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
		BonePaths, NewNodeId, 0, "path", AttributeInfoName), false);

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

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"time", &TimeInfo), false);

	//Position Data
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NewNodeId, 0, "time", &TimeInfo,
		(float*)TimeData.GetData(), 0, TimeInfo.count), false);

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
		"ltransform", &LocalTransformInfo), false);

	TArray<int32> SizesLocalTransformArray;
	for (int i = 0; i < Part.pointCount; i++)
	{
		SizesLocalTransformArray.Add(4 * 4);
	}
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "ltransform", &LocalTransformInfo, LocalTransformData.GetData(),
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

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
		"worldtransform", &WorldTransformInfo), false);

	TArray<int32> SizesWorldTransformArray;
	for (int i = 0; i < Part.pointCount; i++)
	{
		SizesWorldTransformArray.Add(3 * 3);
	}
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
		FHoudiniEngine::Get().GetSession(), NewNodeId,
		0, "worldtransform", &WorldTransformInfo, WorldTransformData.GetData(),
		WorldTransformData.Num(), SizesWorldTransformArray.GetData(), 0, SizesWorldTransformArray.Num()), false);

	//Iterate over BoneTracks
	for (FName TrackName : OutNames)
	{
		TArray<FTransform> ThisTrackTransforms;
		DataModel->GetBoneTrackTransforms(TrackName, ThisTrackTransforms);
		FString DetailAttributeName = FString::Printf(TEXT("%s_transforms"), *TrackName.ToString());
		TotalTrackKeys = ThisTrackTransforms.Num();
		TArray<float> TrackFloatKeys;  //for pCaptData property
		TrackFloatKeys.SetNumZeroed(TotalTrackKeys * 20);

		HAPI_AttributeInfo TrackInfo;
		FHoudiniApi::AttributeInfo_Init(&TrackInfo);
		TrackInfo.count = 1;
		TrackInfo.tupleSize = 20;  //The pCaptData property property contains exactly 20 floats
		TrackInfo.exists = true;
		TrackInfo.owner = HAPI_ATTROWNER_DETAIL;
		TrackInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
		TrackInfo.originalOwner = HAPI_ATTROWNER_DETAIL;
		TrackInfo.totalArrayElements = TrackFloatKeys.Num(); //(keys * 20)
		TrackInfo.typeInfo = HAPI_AttributeTypeInfo::HAPI_ATTRIBUTE_TYPE_NONE;

		int FrameIndex = 0;
		for (FTransform transform : ThisTrackTransforms)
		{
			//set key data
			FMatrix M44 = transform.ToMatrixWithScale();
			FMatrix M44Inverse = M44.Inverse();  //see pCaptData property
			//TODO Validate inverse and conversion

			int32 row = 0;
			int32 col = 0;
			for (int32 i = 0; i < 16; i++)
			{
				//XFormsData[16 * BoneIndex + i] = M44.M[row][col];
				TrackFloatKeys[20 * FrameIndex + i] = M44Inverse.M[row][col];
				col++;
				if (col > 3)
				{
					row++;
					col = 0;
				}
			}
			TrackFloatKeys[20 * FrameIndex + 16] = 1.0f;//Top height
			TrackFloatKeys[20 * FrameIndex + 17] = 1.0f;//Bottom Height
			TrackFloatKeys[20 * FrameIndex + 18] = 1.0f;//Ratio of (top x radius of tube)/(bottom x radius of tube) adjusted for orientation
			TrackFloatKeys[20 * FrameIndex + 19] = 1.0f;//Ratio of (top z radius of tube)/(bottom z radius of tube) adjusted for orientation
			FrameIndex++;
		}
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			TCHAR_TO_ANSI(*DetailAttributeName), &TrackInfo), false);

		TArray<int32> SizesTrackDataArray;
		SizesTrackDataArray.Add(TotalTrackKeys);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
			FHoudiniEngine::Get().GetSession(), NewNodeId,
			0, TCHAR_TO_ANSI(*DetailAttributeName), &TrackInfo, TrackFloatKeys.GetData(),
			TrackInfo.totalArrayElements, SizesTrackDataArray.GetData(), 0, TrackInfo.count), false);

	}
#endif
	return true;
}


