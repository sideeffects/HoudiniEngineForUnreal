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


#include "HoudiniEditorTestMeshMisc.h"

#include "HoudiniApi.h"
#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "Misc/DefaultValueHelper.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMiscMeshes_ActorProperties, "Houdini.UnitTests.Mesh.ActorProperties",
                                         EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMiscMeshes_ActorProperties::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FString(TEXT("/Game/TestHDAs/Mesh/Misc/TestMeshActorProperties.umap"))));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);

			// We should have two outputs, two meshes
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);
			TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 2, return true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			FHoudiniBakeSettings BakeSettings;
			Context->Bake(BakeSettings);

			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
			// There should be two outputs as we have two meshes.
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 2, return true);

			// Go through each output and check we have two actors with one mesh component each.
			TArray<AActor*> Actors;
			for(auto& BakedOutput : BakedOutputs)
			{
				for(auto It : BakedOutput.BakedOutputObjects)
				{
					FHoudiniBakedOutputObject& OutputObject = It.Value;

					AActor* Actor = Cast<AActor>(StaticLoadObject(UObject::StaticClass(), nullptr, *OutputObject.Actor));
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Actor, continue);

					TArray<UStaticMeshComponent*> Components;
					Actor->GetComponents(Components);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components.Num(), 1, continue);
					HOUDINI_TEST_EQUAL_ON_FAIL(Components[0]->IsA<UStaticMeshComponent>(), 1, continue);

					Actors.Add(Actor);
				}
			}

			HOUDINI_TEST_EQUAL_ON_FAIL(Actors.Num(), 2, return false);

			TSet<FString> TagsToFind;
			TagsToFind.Add(TEXT("actor1"));
			TagsToFind.Add(TEXT("actor2"));

			TArray<FName> ActorPaths;

			for (AActor* Actor : Actors)
			{
				auto Tags = Actor->Tags;

				for(int32 i = 0; i < Tags.Num(); ++i)
				{
					if(Tags[i].ToString().StartsWith(TEXT("path:")))
					{
						ActorPaths.Add(Tags[i]);
						Tags.RemoveAt(i);
					}
				}

				// Should have one tag  left after finding the path.
				HOUDINI_TEST_EQUAL_ON_FAIL(Tags.Num(), 1, continue);

				FString* Find = TagsToFind.Find(Tags[0].ToString());
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Find, continue);
				FString Token = *Find;
				TagsToFind.Remove(Token);
			}

			// We should have found both tags, on seperate actors.
			HOUDINI_TEST_EQUAL(TagsToFind.Num(), 0);

			// We should have found two actor paths
			HOUDINI_TEST_EQUAL(ActorPaths.Num(), 2);

			return true;
		}));

	return true;
}

bool GetMaterialSlot(const FString& Input, int32& OutNumber, FString& OutString)
{
	// Find opening and closing brackets
	int32 OpenBracket = -1;
	int32 CloseBracket = -1;

	if(Input.FindChar('[', OpenBracket) && Input.FindChar(']', CloseBracket) && CloseBracket > OpenBracket)
	{
		FString NumberPart = Input.Mid(OpenBracket + 1, CloseBracket - OpenBracket - 1);
		FString StringPart = Input.Mid(CloseBracket + 1);

		// Convert number
		if(FDefaultValueHelper::ParseInt(NumberPart, OutNumber))
		{
			OutString = StringPart;
			return true;
		}
	}

	return false; // Failed to parse
}

bool GetParameterNameAndSlot(const FString& Input, int32& Slot, FString& Parameter)
{
	// Split on underscore
	FString Left;
	FString Right;
	if(Input.Split(TEXT("_"), &Left, &Right))
	{
		if(Left.IsNumeric())
		{
			Slot = FCString::Atoi(*Left);
			Parameter = Right;
			return true;
		}
	}

	return false; // parsing failed
}


struct FTestMeshMaterialGroup
{
	FString Material;
	TMap<FString, float> ScalarParameters;
	TMap<FString, FVector4d> VectorParameters;
	TMap<FString, FString> TextureParameters;
};

struct FTestMesh
{
	TArray<FTestMeshMaterialGroup> Materials;
};


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestMiscMeshes_MaterialProperties, "Houdini.UnitTests.Mesh.MaterialProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestMiscMeshes_MaterialProperties::RunTest(const FString& Parameters)
{

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FString(TEXT("/Game/TestHDAs/Mesh/Misc/TestActorMaterials.umap"))));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
			{
				TArray<UHoudiniOutput*> Outputs;
				Context->GetOutputs(Outputs);

				// We should have two outputs, two meshes
				HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

				const TArray<FHoudiniGeoPartObject>& Parts = Outputs[0]->GetHoudiniGeoPartObjects();

				HOUDINI_TEST_EQUAL_ON_FAIL(Parts.Num(), 1, return true);


				const FHoudiniGeoPartObject& Part = Parts[0];

				const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();


				HAPI_PartInfo PartInfo;
				HAPI_Result Result = FHoudiniApi::GetPartInfo(Session, Part.GeoId, Part.PartId, &PartInfo);
				HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return true);

				HAPI_Bool AllEqual = false;
				TArray<int> LOD1GroupMembership;
				LOD1GroupMembership.SetNum(PartInfo.faceCount);


				Result = FHoudiniApi::GetGroupMembership(Session,
					Part.GeoId, Part.PartId,
					HAPI_GroupType::HAPI_GROUPTYPE_PRIM,
					"lod1",
					&AllEqual, LOD1GroupMembership.GetData(), 0, PartInfo.faceCount);

				FString Error = FHoudiniEngineUtils::GetErrorDescription();

				HOUDINI_TEST_EQUAL_ON_FAIL(Result, HAPI_RESULT_SUCCESS, return true);


				TArray<FString> MaterialNames;

				FHoudiniHapiAccessor MaterialAccessor(Part.GeoId, Part.PartId, "unreal_material");
				bool bSuccess = MaterialAccessor.GetAttributeData(HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM, MaterialNames);
				HOUDINI_TEST_EQUAL_ON_FAIL(bSuccess, true, return true);

				FTestMesh Mesh;

				for(int FaceIndex = 0; FaceIndex < MaterialNames.Num(); FaceIndex++)
				{
					int LodIndex = LOD1GroupMembership[FaceIndex] != 0 ? 1 : 0;

					FString ActualMaterialName;
					int Slot;
					GetMaterialSlot(MaterialNames[FaceIndex], Slot, ActualMaterialName);

					if(!Mesh.Materials.IsValidIndex(Slot))
						Mesh.Materials.SetNum(Slot + 1);

					FTestMeshMaterialGroup& Material = Mesh.Materials[Slot];

					Material.Material = ActualMaterialName;
				}

				TArray<FString> AttributeNames = FHoudiniEngineUtils::GetAttributeNames(Session, Part.GeoId, Part.PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM);

				for(FString& AttrName : AttributeNames)
				{
					FString Prefix = TEXT("unreal_material_parameter_");
					if(!AttrName.StartsWith(Prefix))
						continue;

					FString TruncatedParamName = AttrName;
					TruncatedParamName.RemoveFromStart(Prefix);

					FString ParamName;
					int Slot = 0;
					GetParameterNameAndSlot(TruncatedParamName, Slot, ParamName);

					std::string CStr = TCHAR_TO_ANSI(*AttrName);
					FHoudiniHapiAccessor Accessor(Part.GeoId, Part.PartId, CStr.c_str());
					HAPI_AttributeInfo Info;
					Accessor.GetInfo(Info, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM);

					TArray<float> Values;
					TArray<FString> StringValues;
					if(!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, Values))
					{
						Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, StringValues);
					}

					for (int Index = 0; Index < PartInfo.faceCount; Index++)
					{
						auto& Material = Mesh.Materials[Slot];

						int FaceSlot = 0;
						FString FaceMaterial;
						GetMaterialSlot(MaterialNames[Index], FaceSlot, FaceMaterial);
						if(Slot != FaceSlot)
							continue;

						const FString& MaterialName = MaterialNames[Index];

						if (Info.storage == HAPI_STORAGETYPE_FLOAT && Info.tupleSize == 1)
						{
							float Value = Values[Index];
							auto* ParamPtr = Material.ScalarParameters.Find(ParamName);
							if(!ParamPtr)
								Material.ScalarParameters.Add(ParamName, Value);
						}

						else if (Info.storage == HAPI_STORAGETYPE_FLOAT && Info.tupleSize == 4)
						{
							FVector4d Value = FVector4d(Values[Index * 4 + 0], Values[Index * 4 + 1], Values[Index * 4 + 2], Values[Index * 4 + 3]);
							auto* ParamPtr = Material.VectorParameters.Find(ParamName);
							if(!ParamPtr)
								Material.VectorParameters.Add(ParamName, Value);
						}
						else if (Info.storage == HAPI_STORAGETYPE_STRING && Info.tupleSize == 1)
						{
							FString Value = StringValues[Index];
							auto* ParamPtr = Material.TextureParameters.Find(ParamName);
							if(!ParamPtr)
								Material.TextureParameters.Add(ParamName, Value);
						}
						else
						{
							HOUDINI_LOG_ERROR(TEXT("Invalid storage type, failing test."));
							HOUDINI_TEST_EQUAL_ON_FAIL(true, false, return true);
						}
					}
			}

				{
					HOUDINI_TEST_NOT_EQUAL_ON_FAIL(Mesh.Materials[0].VectorParameters.IsEmpty(), true, return true);
					auto It = Mesh.Materials[0].VectorParameters.CreateConstIterator();
					HOUDINI_TEST_EQUAL(It->Value, FVector4d(0.0f, 0.0f, 1.0, 0.0));
				}
				{
					HOUDINI_TEST_NOT_EQUAL_ON_FAIL(Mesh.Materials[1].VectorParameters.IsEmpty(), true, return true);
					auto It = Mesh.Materials[1].VectorParameters.CreateConstIterator();
					HOUDINI_TEST_EQUAL(It->Value, FVector4d(1.0f, 0.0f, 0.0, 0.0));
				}
				{
					HOUDINI_TEST_NOT_EQUAL_ON_FAIL(Mesh.Materials[2].VectorParameters.IsEmpty(), true, return true);
					auto It = Mesh.Materials[2].VectorParameters.CreateConstIterator();
					HOUDINI_TEST_EQUAL(It->Value, FVector4d(0.5f, 0.0f, 0.0, 0.0));
				}

			return true;
		}));

	return true;
}

