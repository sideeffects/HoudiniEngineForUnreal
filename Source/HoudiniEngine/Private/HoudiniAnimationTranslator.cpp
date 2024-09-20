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

#include "HoudiniAnimationTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniInputObject.h"

#include "HoudiniEngineRuntimeUtils.h"

#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


bool
FHoudiniAnimationTranslator::IsMotionClipFrame(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, bool bRequiresLocalTransform)
{

	auto GetAttrInfo = [](const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, const char* AttrName, HAPI_AttributeOwner AttrOwner) -> HAPI_AttributeInfo
	{
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);
		HAPI_Result AttrInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			AttrName, AttrOwner, &AttrInfo);
		return AttrInfo;  
	};

	if (!GetAttrInfo(GeoId, PartId, "time", HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM).exists)
	{
		return false;
	}

	if (!GetAttrInfo(GeoId, PartId, "clipinfo", HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL).exists)
	{
		return false;
	}

	// Check for attributes inside this packed prim:
	// point attributes: localtransform, transform, path, name
	
	// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
	int NumInstancedParts = 1;
	TArray<HAPI_PartId> InstancedPartIds;
	InstancedPartIds.SetNumZeroed(NumInstancedParts);
	if ( FHoudiniApi::GetInstancedPartIds(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			InstancedPartIds.GetData(),
			0, NumInstancedParts ) != HAPI_RESULT_SUCCESS )
	{
		return false;
	}

	HAPI_PartId InstancedPartId = InstancedPartIds[0];
	if (bRequiresLocalTransform &&
		!GetAttrInfo(GeoId, InstancedPartId, "localtransform", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}

	if (!GetAttrInfo(GeoId, InstancedPartId, "transform", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}

	if (!GetAttrInfo(GeoId, InstancedPartId, "path", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}

	if (!GetAttrInfo(GeoId, InstancedPartId, "name", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT).exists)
	{
		return false;
	}
	
	return true;
}

bool 
FHoudiniAnimationTranslator::CreateAnimSequenceFromOutput(
	UHoudiniOutput* InOutput,
	const FHoudiniPackageParams& InPackageParams,
	UObject* InOuterComponent)
{

	//Loop over hgpo in Output cand
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects(); 
	return CreateAnimationFromMotionClip(InOutput, HGPOs, InPackageParams, InOuterComponent);
}

UAnimSequence*
FHoudiniAnimationTranslator::CreateNewAnimation(FHoudiniPackageParams& InPackageParams, const FHoudiniGeoPartObject& HGPO, const FString& InSplitIdentifier)
{
	// Update the current Obj/Geo/Part/Split IDs
	InPackageParams.ObjectId = HGPO.ObjectId;
	InPackageParams.GeoId = HGPO.GeoId;
	InPackageParams.PartId = HGPO.PartId;
	InPackageParams.SplitStr = InSplitIdentifier;

	UAnimSequence* NewAnimation = InPackageParams.CreateObjectAndPackage<UAnimSequence>();

	UE_LOG(LogTemp, Log, TEXT("Animation Path %s "), *NewAnimation->GetPathName());
	if (!IsValid(NewAnimation))
		return nullptr;

	FAssetRegistryModule::AssetCreated(NewAnimation);

	return NewAnimation;

}

HAPI_PartId FHoudiniAnimationTranslator::GetInstancedMeshPartID(const FHoudiniGeoPartObject& InstancerHGPO)
{
	constexpr int NumInstancedParts = 1;
	TArray<HAPI_PartId> InstancedPartIds;
	InstancedPartIds.SetNumZeroed(NumInstancedParts);
	if (FHoudiniApi::GetInstancedPartIds(
		FHoudiniEngine::Get().GetSession(), InstancerHGPO.GeoId, InstancerHGPO.PartId,
		InstancedPartIds.GetData(), 0, NumInstancedParts) != HAPI_RESULT_SUCCESS)
	{
		return -1;
	}

	return InstancedPartIds[0];
}

FString FHoudiniAnimationTranslator::GetUnrealSkeletonPath(const TArray<FHoudiniGeoPartObject>& HGPOs)
{
	// Try to find the HAPI_UNREAL_SKELETALMESH attribute on the geometry
	// We'll check both the topology frame and the first frame of the animation, just in case.

	// First check on the primitives
	const int NumHGPOs = FMath::Min(HGPOs.Num(), 2);
	for (int i = 0; i < NumHGPOs; i++)
	{
		const FHoudiniGeoPartObject& InstancerHGPO = HGPOs[i];
		
		// First, check whether we have it anywhere on the packed prim (as a detail, prim or point attribute).
		if (FHoudiniEngineUtils::HapiCheckAttributeExists(InstancerHGPO.GeoId, InstancerHGPO.PartId, HAPI_UNREAL_ATTRIB_SKELETON))
		{
			TArray<FString> SkeletonAttrData;
			FHoudiniHapiAccessor Accessor(InstancerHGPO.GeoId, InstancerHGPO.PartId, HAPI_UNREAL_ATTRIB_SKELETON);
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, SkeletonAttrData);

			if (SkeletonAttrData.Num() > 0)
			{
				return SkeletonAttrData[0];
			}
		}
	}

	// Check the content of the packed primitives
	for (int i = 0; i < NumHGPOs; i++)
	{
		const FHoudiniGeoPartObject& InstancerHGPO = HGPOs[i];
		
		const HAPI_PartId MeshPartId = GetInstancedMeshPartID(InstancerHGPO);
		if (MeshPartId < 0)
		{
			continue;
		}

		if (FHoudiniEngineUtils::HapiCheckAttributeExists(InstancerHGPO.GeoId, MeshPartId, HAPI_UNREAL_ATTRIB_SKELETON))
		{
			TArray<FString> SkeletonAttrData;
			FHoudiniHapiAccessor Accessor(InstancerHGPO.GeoId, MeshPartId, HAPI_UNREAL_ATTRIB_SKELETON);
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, SkeletonAttrData);
			if (SkeletonAttrData.Num() > 0)
			{
				return SkeletonAttrData[0];
			}
		}

	}
	
	return FString();
}

bool
FHoudiniAnimationTranslator::GetClipInfo(const FHoudiniGeoPartObject& InstancerHGPO, float& OutFrameRate)
{
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(InstancerHGPO.GeoId, InstancerHGPO.PartId, "clipinfo", HAPI_ATTROWNER_DETAIL))
	{
		HAPI_AttributeInfo ClipAttrInfo;
		FHoudiniApi::AttributeInfo_Init(&ClipAttrInfo);

		HAPI_Result PointInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InstancerHGPO.GeoId, InstancerHGPO.PartId,
			"clipinfo", HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, &ClipAttrInfo);

		if (!ClipAttrInfo.exists)
		{
			return false;
		}

		TArray<HAPI_StringHandle> StringHandles;
		StringHandles.SetNum(ClipAttrInfo.count);
		
		if (FHoudiniApi::GetAttributeDictionaryData(
				FHoudiniEngine::Get().GetSession(),
				InstancerHGPO.GeoId, InstancerHGPO.PartId,
				"clipinfo",
				&ClipAttrInfo,
				&StringHandles[0],
				0, ClipAttrInfo.count
				) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}

		int StringLength = 0;
		if (FHoudiniApi::GetStringBufLength(
				FHoudiniEngine::Get().GetSession(),
				StringHandles[0],
				&StringLength) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}
		
		if (StringLength == 0)
		{
			return false;
		}

		TArray<char> ClipInfoStrBuffer;
		ClipInfoStrBuffer.SetNum(StringLength);
		if (FHoudiniApi::GetString(
				FHoudiniEngine::Get().GetSession(),
				StringHandles[0],
				&ClipInfoStrBuffer[0],
				StringLength) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}

		// At this point we have the clipinfo dict as a JSON string. Time to parse it! 
		FString ClipInfoStr = FString(ANSI_TO_TCHAR(&ClipInfoStrBuffer[0]));

		// Parse it as JSON
		TSharedPtr<FJsonObject> JSONObject;
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( ClipInfoStr );
		if (!FJsonSerializer::Deserialize(Reader, JSONObject) || !JSONObject.IsValid())
		{
			return false;
		}
		
		if (!JSONObject.IsValid())
		{
			return false;
		}

		// We're only interested in the frame rate, for now.
		JSONObject->TryGetNumberField(TEXT("rate"), OutFrameRate);
	}

	return true;
}

bool
FHoudiniAnimationTranslator::GetFbxCustomAttributes(
	int GeoId,
	int MeshPartId,
	int RootBoneIndex,
	TSharedPtr<FJsonObject>& OutJSONObject)
{
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, MeshPartId, "fbx_custom_attributes", HAPI_ATTROWNER_POINT))
	{
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);

		HAPI_Result PointInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, MeshPartId,
			"fbx_custom_attributes", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, &AttrInfo);

		if (!AttrInfo.exists)
		{
			return false;
		}

		TArray<HAPI_StringHandle> StringHandles;
		StringHandles.SetNum(AttrInfo.count);
		
		if (FHoudiniApi::GetAttributeDictionaryData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, MeshPartId,
				"fbx_custom_attributes",
				&AttrInfo,
				&StringHandles[0],
				0, AttrInfo.count
				) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}

		if (!StringHandles.IsValidIndex(RootBoneIndex))
		{
			HOUDINI_LOG_WARNING(TEXT("Could not retrieve 'fbx_custom_attributes'. Invalid root bone index."));
			return false;
		}

		int StringLength = 0;
		if (FHoudiniApi::GetStringBufLength(
				FHoudiniEngine::Get().GetSession(),
				StringHandles[RootBoneIndex],
				&StringLength) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}
		
		if (StringLength == 0)
		{
			return false;
		}

		TArray<char> StrBuffer;
		StrBuffer.SetNum(StringLength);
		if (FHoudiniApi::GetString(
				FHoudiniEngine::Get().GetSession(),
				StringHandles[0],
				&StrBuffer[0],
				StringLength) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}

		// At this point we have the dict as a JSON string. Time to parse it! 
		const FString JSONData = FString(ANSI_TO_TCHAR(&StrBuffer[0]));

		// Parse it as JSON
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( JSONData );
		if (!FJsonSerializer::Deserialize(Reader, OutJSONObject) || !OutJSONObject.IsValid())
		{
			return false;
		}
		
		if (!OutJSONObject.IsValid())
		{
			return false;
		}
		
		return true;
	}

	return false;
}

//Creates SkelatalMesh and Skeleton Assets and Packages, and adds them to OutputObjects
bool FHoudiniAnimationTranslator::CreateAnimationFromMotionClip(UHoudiniOutput* InOutput, const TArray<FHoudiniGeoPartObject>& HGPOs, const FHoudiniPackageParams& InPackageParams, UObject* InOuterComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniAnimationTranslator::CreateAnimationFromMotionClip);

	if (HGPOs.Num() == 0)
	{
		HOUDINI_LOG_WARNING(TEXT("Could not translate MotionClip. No Geo Part Objects."));
		return false;
	}

	// Process the topology frame.
	// Note: We require here that all the HGPO's that we receive in this function be packed primitives (instancers).
	
	const FHoudiniGeoPartObject& TopologyHGPO = HGPOs[0];
	const HAPI_PartId TopologyPartId = GetInstancedMeshPartID(TopologyHGPO);

	if (TopologyPartId < 0)
	{
		HOUDINI_LOG_WARNING(TEXT("Could not translate MotionClip. Invalid topology frame."));
		return false;
	}
	
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier(
		TopologyHGPO.ObjectId, TopologyHGPO.GeoId, TopologyPartId, "");
	OutputObjectIdentifier.PartName = TopologyHGPO.PartName;

	FString SkeletonAssetPathName = GetUnrealSkeletonPath(HGPOs);
	if (SkeletonAssetPathName.IsEmpty())
	{
		HOUDINI_LOG_WARNING(TEXT("Could not translate MotionClip. Empty Skeleton path."));
		return false;
	}

	const FSoftObjectPath SkeletonAssetPath(SkeletonAssetPathName);
	USkeleton* MySkeleton = nullptr;
	MySkeleton = Cast<USkeleton>(SkeletonAssetPath.TryLoad());
	if (!IsValid(MySkeleton))
	{
		HOUDINI_LOG_WARNING(TEXT("Could not translate MotionClip. Missing '%s' attribute."), *FString(HAPI_UNREAL_ATTRIB_SKELETON));
		return false;
	}

	// Process the topology frame.
	// Collect Bone names from motion clip topology frame
	
	HAPI_AttributeInfo BoneNameInfo;
	FHoudiniApi::AttributeInfo_Init(&BoneNameInfo);
	HAPI_Result AttributeInfoResult = FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		TopologyHGPO.GeoId, TopologyPartId,
		"name", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, &BoneNameInfo);

	TArray<FString> BoneNameData;

	FHoudiniHapiAccessor Accessor(TopologyHGPO.GeoId, TopologyPartId, "name");
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, BoneNameData);

	if (BoneNameData.Num() <= 0)
	{
		HOUDINI_LOG_WARNING(TEXT("Could not translate MotionClip. Missing BoneName data."));
		return false;
	}

	// Retrieve the frame rate from the ClipInfo dict
	float FrameRate = 30;
	GetClipInfo(TopologyHGPO, FrameRate);

	//Get unique Set of Bones
	TSet<FName> BoneNames;
	for (auto BoneString : BoneNameData)
	{
		BoneNames.Add(FName(BoneString));
	}

	// Process the packed primitives 
	// Iterate over the packed primitives, retrieve each frame's anim data

	// For each frame, we'll retrieve the world transforms of each bone, transform them into
	// Unreal space, then compute the local transforms.

	TMap <int, TMap <FName, FTransform>> FrameBoneTransformMap;  //For each frame, store bones and LOCAL transforms

	// Per-bone tracks for Pos, Rot and Scale as needed by the AnimController. 
	TMap<FName, TArray<FVector3f>> BonesPosTrack;
	TMap<FName, TArray<FQuat4f>> BonesRotTrack;
	TMap<FName, TArray<FVector3f>> BonesScaleTrack;
	TMap<FString, TArray<FRichCurveKey>> FbxCustomAttributes;

	const int NumHGPOs = HGPOs.Num();
	
	for (int i = 1; i < NumHGPOs; i++)
	{
		const FHoudiniGeoPartObject& InstancerHGPO = HGPOs[i];
		HAPI_PartId MeshPartId = GetInstancedMeshPartID(InstancerHGPO);
		if (MeshPartId < 0)
		{
			continue;
		}

		const int GeoId = InstancerHGPO.GeoId;
		const int PartId = MeshPartId;
		const int FrameIndex = i - 1;
		const double FrameTime = (i-1) / static_cast<double>(FrameRate);
		
		// Retrieve the WorldTransform for all the points in this Mesh (frame)
		// Note, we need to retrieve "P" for the translation, and "transform" for the rotation/scale.

		// Retrieve Position (World Space)
		HAPI_AttributeInfo PointInfo;
		FHoudiniApi::AttributeInfo_Init(&PointInfo);

		HAPI_Result PointInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			HAPI_UNREAL_ATTRIB_POSITION, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, &PointInfo);

		TArray<FVector3f> PositionData;
		PositionData.SetNum(PointInfo.count);  //dont need * PositionInfo.tupleSize, its already a vector container
		FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), GeoId, PartId, HAPI_UNREAL_ATTRIB_POSITION, &PointInfo, -1, (float*)&PositionData[0], 0, PointInfo.count);

		// Retrieve Rotation matrix (World Space)
		HAPI_AttributeInfo WorldTransformInfo;
		FHoudiniApi::AttributeInfo_Init(&WorldTransformInfo);

		HAPI_Result WorldTransformInfoResult = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			"transform", HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, &WorldTransformInfo);
		
		TArray<float> WorldTransformData;
		TArray<int> WorldTransformSizesFixedArray;
		WorldTransformData.SetNum(WorldTransformInfo.count * WorldTransformInfo.tupleSize);
		WorldTransformSizesFixedArray.SetNum(WorldTransformInfo.count);
		HAPI_Result WorldTransformDataResult = FHoudiniApi::GetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), GeoId, PartId, "transform", &WorldTransformInfo, &WorldTransformData[0], WorldTransformInfo.count * WorldTransformInfo.tupleSize, &WorldTransformSizesFixedArray[0], 0, WorldTransformInfo.count);

		//Build the World Space transforms for the bones
		
		TMap <FName, FTransform> BoneWSTransforms;

		// Iterate over all the joints to collect and convert their transform data and collect them into per-bone tracks

		// Find the RootBoneIndex so that we can extract the fbx_custom_attributes from that point
		int RootBoneIndex = INDEX_NONE;
		
		for (int BoneIndex = 0; BoneIndex < PointInfo.count; BoneIndex++ )
		{
			FString BoneString = BoneNameData[BoneIndex];
			FName BoneName = FName(BoneString);

			if (BoneString == "root")
			{
				RootBoneIndex = BoneIndex;
			}
			
			//Read in 3x3 into Matrix, and append the translation
			FMatrix M44Pose;  //this is unconverted houdini space
			M44Pose.M[0][0] = WorldTransformData[9 * BoneIndex + 0];
			M44Pose.M[0][1] = WorldTransformData[9 * BoneIndex + 1];
			M44Pose.M[0][2] = WorldTransformData[9 * BoneIndex + 2];
			M44Pose.M[0][3] = 0;
			M44Pose.M[1][0] = WorldTransformData[9 * BoneIndex + 3];
			M44Pose.M[1][1] = WorldTransformData[9 * BoneIndex + 4];
			M44Pose.M[1][2] = WorldTransformData[9 * BoneIndex + 5];
			M44Pose.M[1][3] = 0;
			M44Pose.M[2][0] = WorldTransformData[9 * BoneIndex + 6];
			M44Pose.M[2][1] = WorldTransformData[9 * BoneIndex + 7];
			M44Pose.M[2][2] = WorldTransformData[9 * BoneIndex + 8];
			M44Pose.M[2][3] = 0;
			M44Pose.M[3][0] = PositionData[BoneIndex].X;
			M44Pose.M[3][1] = PositionData[BoneIndex].Y;
			M44Pose.M[3][2] = PositionData[BoneIndex].Z;
			M44Pose.M[3][3] = 1;

			FTransform PoseTransform = FTransform(M44Pose);//this is in Houdini Space
			//Now convert to unreal
			FQuat PoseQ = PoseTransform.GetRotation();
			FQuat ConvertedPoseQ = FQuat(PoseQ.X, PoseQ.Z, PoseQ.Y, -PoseQ.W) * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
			FVector PoseT = PoseTransform.GetLocation();
			FVector ConvertedPoseT = FVector(PoseT.X, PoseT.Z, PoseT.Y);
			FVector PoseS = PoseTransform.GetScale3D();
			FTransform UnrealPoseTransform = FTransform(ConvertedPoseQ, ConvertedPoseT * 100, PoseS * 100);
			
			BoneWSTransforms.Add(BoneName, UnrealPoseTransform);
		}

		// Convert the bones to local transforms, and append them to the bone tracks for the current frame
		TMap<FName, FTransform> BoneTrack = FrameBoneTransformMap.FindOrAdd(FrameIndex);

		const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
		for (auto& Elem : BoneWSTransforms)
		{
			const FName & CurrentBoneName = Elem.Key;
			const FTransform & GlobalBoneTransform = Elem.Value;
			const int32 BoneRefIndex = RefSkeleton.FindBoneIndex(CurrentBoneName);
			FTransform LocalBoneTransform;

			if (BoneRefIndex > 0)
			{
				int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneRefIndex);
				const FName & ParentBoneName = RefSkeleton.GetBoneName(ParentBoneIndex);
				const FTransform & ParentCSXform = *BoneWSTransforms.Find(ParentBoneName);
				LocalBoneTransform = GlobalBoneTransform * ParentCSXform.Inverse();
			}
			else
			{
				LocalBoneTransform = GlobalBoneTransform;
			}

			TArray<FVector3f>& KeyPosArray = BonesPosTrack.FindOrAdd(CurrentBoneName);
			FVector Pos = LocalBoneTransform.GetLocation();
			KeyPosArray.Add(FVector3f(Pos));

			TArray<FQuat4f>& KeyRotArray = BonesRotTrack.FindOrAdd(CurrentBoneName);
			FQuat Q = LocalBoneTransform.GetRotation();
			KeyRotArray.Add(FQuat4f(Q));
			
			TArray<FVector3f>& KeyScaleArray = BonesScaleTrack.FindOrAdd(CurrentBoneName);
			FVector Scale = LocalBoneTransform.GetScale3D();
			KeyScaleArray.Add(FVector3f(Scale));
		}

		if (RootBoneIndex != INDEX_NONE)
		{
			// Collect the fbx_custom_attributes for this frame
			TSharedPtr<FJsonObject> JsonObject;
			if (GetFbxCustomAttributes(GeoId, PartId, RootBoneIndex, JsonObject))
			{
				TArray<FString> Keys;
				JsonObject->Values.GetKeys(Keys);
				for (const FString& Key : Keys)
				{
					if (double Value; JsonObject->TryGetNumberField(Key, Value))
					{
						TArray<FRichCurveKey>& CurveData = FbxCustomAttributes.FindOrAdd(Key);
						CurveData.Add(FRichCurveKey(FrameTime, static_cast<float>(Value)));
					}
				}
			}
		}
	}

	FHoudiniOutputObject& OutputObject = InOutput->GetOutputObjects().FindOrAdd(OutputObjectIdentifier);
	FHoudiniPackageParams PackageParams = InPackageParams;
	UAnimSequence* AnimSequence = CreateNewAnimation(PackageParams, TopologyHGPO, OutputObjectIdentifier.SplitIdentifier);
	OutputObject.OutputObject = AnimSequence;
	OutputObject.bProxyIsCurrent = false;

	AnimSequence->ResetAnimation();
	AnimSequence->SetSkeleton(MySkeleton);
	AnimSequence->ImportFileFramerate = FrameRate;
	AnimSequence->ImportResampleFramerate = FrameRate;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	// Initialize data model
	IAnimationDataController& AnimController = AnimSequence->GetController();
	AnimController.InitializeModel();

	AnimController.OpenBracket(NSLOCTEXT("MyNamespace", "InitializeAnimation", "Initialize New Anim Sequence"));
	{
		// This line is to set actual frame rate
		AnimController.SetFrameRate(FFrameRate(FrameRate, 1), true);

		//AnimController.SetPlayLength(33.0f, true);
		const int32 NumKeys = NumHGPOs-1;
		AnimController.SetNumberOfFrames(NumKeys - 1);

		//rgc
		bool bShouldTransact = true;
		for (const FName& Bone : BoneNames)  //loop over all bones
		{
			//QUESTION:  What to do if no tracks for bone? Skip?
			if (BonesPosTrack.Contains(Bone) && BonesRotTrack.Contains(Bone) && BonesScaleTrack.Contains(Bone))
			{
				AnimController.AddBoneCurve(Bone, bShouldTransact);
				AnimController.SetBoneTrackKeys(Bone, *BonesPosTrack.Find(Bone), *BonesRotTrack.Find(Bone), *BonesScaleTrack.Find(Bone), bShouldTransact);
			}
			else
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
				HOUDINI_LOG_ERROR(TEXT("Bone '%s' is missing from bone tracks."), *Bone.ToString());
#else
				HOUDINI_LOG_ERROR(TEXT("Bone '%s' is missing from bone tracks."), Bone);
#endif
			}
		}

		// Set the fbx_custom_attributes as anim curves.
		for (const auto& Entry : FbxCustomAttributes )
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			FAnimationCurveIdentifier CurveId(FName(Entry.Key), ERawCurveTrackTypes::RCT_Float);
#else
			FSmartName NewName;
			MySkeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, FName(Entry.Key), NewName);
			FAnimationCurveIdentifier CurveId(NewName, ERawCurveTrackTypes::RCT_Float);
#endif
			if (Entry.Value.Num() && AnimController.AddCurve(CurveId))
			{
				
				AnimController.SetCurveKeys(CurveId, Entry.Value);
			}
		}
		
		AnimController.NotifyPopulated();
	}
	AnimController.CloseBracket();

	// Update the property attributes on the component, using the attributes already stored on the HGPO.
	if (TopologyHGPO.GenericPropertyAttributes.Num() > 0)
	{
		// Default priority for any attribute is 0.
		// Lower values will be processed first. Higher values will be processed later.
		int Priority = 1;
		TMap<FString, int> AttributePriority;
		AttributePriority.Add("RetargetSourceAsset", Priority);
		Priority++;
		AttributePriority.Add("AdditiveAnimType", Priority);
		Priority++;
		AttributePriority.Add("RefPoseType", Priority);
		AttributePriority.Add("BasePoseType", Priority);
		Priority++;
		AttributePriority.Add("RefPoseSeq", Priority);
		AttributePriority.Add("BasePoseAnimation", Priority);
		Priority++;
		AttributePriority.Add("RefFrameIndex", Priority);
		
		TArray<FHoudiniGenericAttribute> SortedAttributes = TopologyHGPO.GenericPropertyAttributes;
		Algo::Sort(SortedAttributes, [&AttributePriority](const FHoudiniGenericAttribute& LHS, const FHoudiniGenericAttribute& RHS) -> bool
		{
			int LPriority = 0;
			int RPriority = 0;
			if (AttributePriority.Contains(LHS.GetStringValue()))
			{
				LPriority = AttributePriority.FindChecked(LHS.GetStringValue());
			}

			if (AttributePriority.Contains(RHS.GetStringValue()))
			{
				RPriority = AttributePriority.FindChecked(RHS.GetStringValue());
			}
			
			return LPriority < RPriority;
		});
		
		FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(AnimSequence, SortedAttributes);
	}

#endif

	return true;
}
