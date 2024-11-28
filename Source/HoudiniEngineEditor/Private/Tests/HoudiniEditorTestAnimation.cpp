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

#include "HoudiniEditorTestAnimation.h"

#include "EditorAnimUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "Animation/SkeletalMeshActor.h"
#include "Chaos/HeightField.h"
#include "Animation/Skeleton.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "Engine/SkinnedAssetCommon.h"
#endif
#include "Animation/AnimSequence.h"


TMap<FName, TArray<FTransform>> FHoudiniEditorTestAnimationUtils::GetAnimationTransforms(const UAnimSequence* AnimSequence)
{
	const IAnimationDataModel* DataModel = AnimSequence->GetDataModel();

	TArray<FName> BonesTrackNames;
	DataModel->GetBoneTrackNames(BonesTrackNames);

	TMap<FName, TArray<FTransform>> TrackMap;  //For Each Bone, Store Array of transforms (one for each key)

	//Iterate over BoneTracks
	for (FName TrackName : BonesTrackNames)
	{
		//const FMeshBoneInfo& CurrentBone = RefSkeleton.GetRefBoneInfo()[BoneIndex];
		TArray<FTransform> ThisTrackTransforms;
		DataModel->GetBoneTrackTransforms(TrackName, ThisTrackTransforms);
		FString DetailAttributeName = FString::Printf(TEXT("%s_transforms"), *TrackName.ToString());
		TArray<float> TrackFloatKeys;
		TrackMap.Add(TrackName, ThisTrackTransforms);
	}
	return TrackMap;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestAnimationRoundtrip, "Houdini.UnitTests.Animation.Roundtrip", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestAnimationRoundtrip::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestAnimationUtils::AnimationRoundtripHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	UAnimSequence* OrigAnimSequence = LoadObject<UAnimSequence>(Context->World, TEXT("/Script/Engine.SkeletalMesh'/Game/TestObjects/Animation/MM_Walk_Fwd.MM_Walk_Fwd'"));

	HOUDINI_TEST_NOT_NULL_ON_FAIL(OrigAnimSequence, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, OrigAnimSequence]()
	{
		bool bChanged = true;

		UHoudiniInput * Input = Context->HAC->GetInputAt(0);

		Input->InsertInputObjectAt(EHoudiniInputType::Geometry, 0);

		UHoudiniInputObject* CurrentInputObjectWrapper = Input->GetHoudiniInputObjectAt(0);
		if (CurrentInputObjectWrapper)
			CurrentInputObjectWrapper->Modify();

		Input->Modify();
		Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, OrigAnimSequence);
		Input->MarkChanged(true);

		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have two outputs, two meshes
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs[0]->GetType(), EHoudiniOutputType::AnimSequence, return true);
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, OrigAnimSequence]()
	{
		FHoudiniBakeSettings BakeSettings;

		FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(Context->HAC, BakeSettings, Context->HAC->HoudiniEngineBakeOption, Context->HAC->bRemoveOutputAfterBake);

		auto BakedOutputs = Context->HAC->GetBakedOutputs();

		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);

		auto BakedObjects = BakedOutputs[0].BakedOutputObjects;
		TArray<FHoudiniBakedOutputObject> BakeOutputObjects;
		BakedObjects.GenerateValueArray(BakeOutputObjects);
		HOUDINI_TEST_EQUAL_ON_FAIL(BakeOutputObjects.Num(), 1, return true);

		UAnimSequence* AnimSequence  = Cast<UAnimSequence>(StaticLoadObject(UObject::StaticClass(), nullptr, *BakeOutputObjects[0].BakedObject));

		HOUDINI_TEST_NOT_NULL_ON_FAIL(AnimSequence, return true);

		TMap<FName, TArray<FTransform>> OrigAnim = FHoudiniEditorTestAnimationUtils::GetAnimationTransforms(OrigAnimSequence);
		TMap<FName, TArray<FTransform>> NewAnim = FHoudiniEditorTestAnimationUtils::GetAnimationTransforms(AnimSequence);


		HOUDINI_TEST_EQUAL_ON_FAIL(OrigAnim.Num(), NewAnim.Num(), return true);

		TArray<FName> BoneNames;
		OrigAnim.GetKeys(BoneNames);

		for(const FName & Name : BoneNames)
		{
			TArray<FTransform> * Transforms = NewAnim.Find(Name);
			if (!Transforms)
			{
				FString ErrorString = FString::Printf(TEXT("Missing bone %s in new Animation"), *Name.ToString());
				AddError(ErrorString);
				return true;
			}

			TArray<FTransform>* OrigTransforms = OrigAnim.Find(Name);
			if (OrigTransforms->Num() != Transforms->Num())
			{
				FString ErrorString = FString::Printf(TEXT("Mismatched transforms %s in new Animation"), *Name.ToString());
				AddError(ErrorString);
				return true;
			}

			for (int Index = 0; Index < OrigTransforms->Num(); Index++)
			{
				FTransform * OrigTransform = &(*OrigTransforms)[Index];
				FTransform * Transform = &(*Transforms)[Index];

				if (!OrigTransform->GetLocation().Equals(Transform->GetLocation(), 0.01f))
				{
					FString Error = FString::Printf(TEXT("Bone %s Location Differs: Original %s Cooked %s"), *Name.ToString(),
						*OrigTransform->GetLocation().ToString(),
						*Transform->GetLocation().ToString());

					AddError(Error);
				}

				if (!OrigTransform->GetRotation().Equals(Transform->GetRotation(), 0.01f))
				{
					FString Error = FString::Printf(TEXT("Bone %s Rotation Differs: Original %s Cooked %s"), *Name.ToString(),
						*OrigTransform->GetRotation().ToString(),
						*Transform->GetRotation().ToString());

					AddError(Error);
				}

				if (!OrigTransform->GetScale3D().Equals(Transform->GetScale3D(), 0.01f))
				{
					FString Error = FString::Printf(TEXT("Bone %s Scale Differs: Original %s Cooked %s"), *Name.ToString(),
						*OrigTransform->GetScale3D().ToString(),
						*Transform->GetScale3D().ToString());

					AddError(Error);
				}
			}
		}
		return true;
	}));
	return true;
}


#endif