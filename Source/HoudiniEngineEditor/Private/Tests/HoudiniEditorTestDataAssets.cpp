/*
* Copyright (c) <2026> Side Effects Software Inc.
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

#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorTestDataAssets.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniDataAssetTranslator.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniInput.h"
#include "HoudiniInputObject.h"
#include "HoudiniParameterInt.h"
#include "HoudiniOutput.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "UnrealDataAssetTranslator.h"
#include "Engine/DataAsset.h"
#include "JsonObjectConverter.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool GetPointStringAttribute(HAPI_NodeId NodeId, const TCHAR* AttributeName, FString& OutValue)
{
	TArray<FString> Values;
	FHoudiniHapiAccessor Accessor(NodeId, 0, AttributeName);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values, 0, 1) || Values.Num() != 1)
		return false;

	OutValue = Values[0];
	return true;
}

static bool GetPointDictionaryAttribute(HAPI_NodeId NodeId, const TCHAR* AttributeName, FString& OutValue)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, 0, TCHAR_TO_ANSI(AttributeName), HAPI_ATTROWNER_POINT, &AttributeInfo) != HAPI_RESULT_SUCCESS
		|| !AttributeInfo.exists || AttributeInfo.storage != HAPI_STORAGETYPE_DICTIONARY || AttributeInfo.count != 1)
	{
		return false;
	}

	HAPI_StringHandle StringHandle = -1;
	if (FHoudiniApi::GetAttributeDictionaryData(
		FHoudiniEngine::Get().GetSession(), NodeId, 0, TCHAR_TO_ANSI(AttributeName), &AttributeInfo, &StringHandle, 0, 1) != HAPI_RESULT_SUCCESS)
	{
		return false;
	}

	return FHoudiniEngineString::ToFString(StringHandle, OutValue);
}

static bool SetPointDictionaryAttribute(HAPI_NodeId NodeId, const TCHAR* AttributeName, const FString& Value)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniHapiAccessor Accessor(NodeId, 0, AttributeName);
	if (Accessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT) && AttributeInfo.exists)
	{
		if (FHoudiniApi::DeleteAttribute(
			FHoudiniEngine::Get().GetSession(), NodeId, 0, TCHAR_TO_ANSI(AttributeName), &AttributeInfo) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_ERROR(TEXT("Could not delete test dictionary attribute %s: %s"), AttributeName, *FHoudiniEngineUtils::GetErrorDescription());
			return false;
		}
	}

	if (!Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_DICTIONARY, 1, 1, &AttributeInfo))
	{
		HOUDINI_LOG_ERROR(TEXT("Could not add test dictionary attribute %s: %s"), AttributeName, *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	const bool bSuccess = Accessor.SetAttributeDictionary(AttributeInfo, TArray<FString>{ Value });
	if (!bSuccess)
		HOUDINI_LOG_ERROR(TEXT("Could not set test dictionary attribute %s: %s"), AttributeName, *FHoudiniEngineUtils::GetErrorDescription());
	return bSuccess;
}

static bool AddPointStringAttribute(HAPI_NodeId NodeId, const TCHAR* AttributeName, const FString& Value)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniHapiAccessor Accessor(NodeId, 0, AttributeName);
	return Accessor.AddAttribute(HAPI_ATTROWNER_POINT, HAPI_STORAGETYPE_STRING, 1, 1, &AttributeInfo)
		&& Accessor.SetAttributeData(AttributeInfo, TArray<FString>{ Value });
}

static bool AddDetailStringAttribute(HAPI_NodeId NodeId, const TCHAR* AttributeName, const FString& Value)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniHapiAccessor Accessor(NodeId, 0, AttributeName);
	return Accessor.AddAttribute(HAPI_ATTROWNER_DETAIL, HAPI_STORAGETYPE_STRING, 1, 1, &AttributeInfo)
		&& Accessor.SetAttributeData(AttributeInfo, TArray<FString>{ Value });
}

static bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutJsonObject)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
}

static bool AreJsonValuesEqual(const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right);

static bool AreJsonObjectsEqual(const TSharedPtr<FJsonObject>& Left, const TSharedPtr<FJsonObject>& Right)
{
	if (!Left.IsValid() || !Right.IsValid() || Left->Values.Num() != Right->Values.Num())
		return false;

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Left->Values)
	{
		const TSharedPtr<FJsonValue> RightValue = Right->TryGetField(Pair.Key);
		if (!RightValue.IsValid() || !AreJsonValuesEqual(Pair.Value, RightValue))
			return false;
	}

	return true;
}

static bool AreJsonValuesEqual(const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
{
	if (!Left.IsValid() || !Right.IsValid())
		return Left == Right;

	if (Left->Type != Right->Type)
	{
		if (Left->Type == EJson::Boolean && Right->Type == EJson::Number)
			return FMath::IsNearlyEqual(Right->AsNumber(), Left->AsBool() ? 1.0 : 0.0);
		if (Left->Type == EJson::Number && Right->Type == EJson::Boolean)
			return FMath::IsNearlyEqual(Left->AsNumber(), Right->AsBool() ? 1.0 : 0.0);
		return false;
	}

	switch (Left->Type)
	{
		case EJson::None:
		case EJson::Null:
			return true;
		case EJson::String:
			return Left->AsString() == Right->AsString();
		case EJson::Number:
			return FMath::IsNearlyEqual(Left->AsNumber(), Right->AsNumber());
		case EJson::Boolean:
			return Left->AsBool() == Right->AsBool();
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& LeftArray = Left->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& RightArray = Right->AsArray();
			if (LeftArray.Num() != RightArray.Num())
				return false;

			for (int32 Index = 0; Index < LeftArray.Num(); ++Index)
			{
				if (!AreJsonValuesEqual(LeftArray[Index], RightArray[Index]))
					return false;
			}
			return true;
		}
		case EJson::Object:
			return AreJsonObjectsEqual(Left->AsObject(), Right->AsObject());
		default:
			return false;
	}
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DataAssetBlackboardAttributes, "Houdini.UnitTests.Inputs.DataAsset.BlackboardAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DataAssetBlackboardAttributes::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDataAsset* DataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestBlackboard.TestBlackboard")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return false);

	// Complete the initial cook so the HDA's input objects have been created before assigning the Data Asset.
	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			// InputAsOutput passes the Data Asset point through to its output for verification below.
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			// A point-only Data Asset does not produce an Unreal output object, so inspect the
			// HDA's HAPI output node directly. This validates the actual pass-through result.
			TArray<HAPI_NodeId> OutputNodes;
			const bool bGatheredOutputs = FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes);
			HOUDINI_TEST_EQUAL_ON_FAIL(bGatheredOutputs, true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			const HAPI_NodeId NodeId = OutputNodes[0];
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			FString ActualValue;
			if (!GetPointStringAttribute(NodeId, TEXT("unreal_object_path"), ActualValue))
			{
				AddError(TEXT("The Data Asset output is missing unreal_object_path."));
				return true;
			}
			if (ActualValue != DataAsset->GetPathName())
			{
				AddError(FString::Printf(TEXT("Unexpected unreal_object_path. Expected '%s', got '%s'."), *DataAsset->GetPathName(), *ActualValue));
				return true;
			}

			if (!GetPointStringAttribute(NodeId, TEXT("unreal_data_asset_class"), ActualValue))
			{
				AddError(TEXT("The Data Asset output is missing unreal_data_asset_class."));
				return true;
			}
			if (ActualValue != DataAsset->GetClass()->GetPathName())
			{
				AddError(FString::Printf(TEXT("Unexpected unreal_data_asset_class. Expected '%s', got '%s'."), *DataAsset->GetClass()->GetPathName(), *ActualValue));
				return true;
			}

			// TestBlackboard stores its authored Blackboard entries in Keys. Array elements are exported
			// as indexed attributes, so verify both the count and an authored member of the first entry.
			FProperty* KeysProperty = DataAsset->GetClass()->FindPropertyByName(TEXT("Keys"));
			void* KeysValue = KeysProperty ? KeysProperty->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
			if (!KeysValue)
			{
				AddError(TEXT("TestBlackboard does not expose an editable Keys property."));
				return true;
			}

			FArrayProperty* KeysArrayProperty = CastField<FArrayProperty>(KeysProperty);
			FStructProperty* KeysStructProperty = KeysArrayProperty ? CastField<FStructProperty>(KeysArrayProperty->Inner) : nullptr;
			if (!KeysArrayProperty || !KeysStructProperty)
			{
				AddError(TEXT("TestBlackboard Keys is not an array of structs."));
				return true;
			}

			FScriptArrayHelper KeysArrayHelper(KeysArrayProperty, KeysValue);
			FHoudiniHapiAccessor KeysCountAccessor(NodeId, 0, "Keys_num");
			HAPI_AttributeInfo AttributeInfo;
			HOUDINI_TEST_EQUAL_ON_FAIL(KeysCountAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			TArray<int64> KeyCounts;
			HOUDINI_TEST_EQUAL_ON_FAIL(KeysCountAccessor.GetAttributeData(AttributeInfo, KeyCounts, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(KeyCounts.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(KeyCounts[0], KeysArrayHelper.Num());

			if (KeysArrayHelper.Num() > 0)
			{
				FProperty* EntryNameProperty = KeysStructProperty->Struct->FindPropertyByName(TEXT("EntryName"));
				void* EntryNameValue = EntryNameProperty ? EntryNameProperty->ContainerPtrToValuePtr<void>(KeysArrayHelper.GetRawPtr(0)) : nullptr;
				HOUDINI_TEST_NOT_NULL_ON_FAIL(EntryNameValue, return true);

				FString EntryNameAttribute = TEXT("Keys_0_") + EntryNameProperty->GetAuthoredName();
				HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, *EntryNameAttribute, ActualValue), true, return true);
				HOUDINI_TEST_EQUAL(ActualValue, CastFieldChecked<FNameProperty>(EntryNameProperty)->GetPropertyValue(EntryNameValue).ToString());
			}

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DataAssetBlueprintAttributes, "Houdini.UnitTests.Inputs.DataAsset.BlueprintAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DataAssetBlueprintAttributes::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDataAsset* DataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestDataAsset.TestDataAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return false);

	// Complete the initial cook so the HDA's input objects have been created before assigning the Data Asset.
	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			TArray<HAPI_NodeId> OutputNodes;
			const bool bGatheredOutputs = FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes);
			HOUDINI_TEST_EQUAL_ON_FAIL(bGatheredOutputs, true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			const HAPI_NodeId NodeId = OutputNodes[0];
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(static_cast<int>(NodeId), -1, return true);

			FString ActualStringValue;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, TEXT("unreal_object_path"), ActualStringValue), true, return true);
			HOUDINI_TEST_EQUAL(ActualStringValue, DataAsset->GetPathName());
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, TEXT("unreal_data_asset_class"), ActualStringValue), true, return true);
			HOUDINI_TEST_EQUAL(ActualStringValue, DataAsset->GetClass()->GetPathName());

			for (const FName PropertyName : { FName(TEXT("Value1")), FName(TEXT("Value2")), FName(TEXT("Value3")) })
			{
				FProperty* Property = DataAsset->GetClass()->FindPropertyByName(PropertyName);
				void* Value = Property ? Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Property, return true);
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Value, return true);

				const FString AttributeName = PropertyName.ToString();
				FHoudiniHapiAccessor Accessor(NodeId, 0, *AttributeName);
				if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
				{
					if (NumericProperty->IsFloatingPoint())
					{
						TArray<double> ActualValue;
						HOUDINI_TEST_EQUAL_ON_FAIL(Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, ActualValue, 0, 1), true, return true);
						HOUDINI_TEST_EQUAL_ON_FAIL(ActualValue.Num(), 1, return true);
						HOUDINI_TEST_EQUAL(ActualValue[0], NumericProperty->GetFloatingPointPropertyValue(Value));
					}
					else if (NumericProperty->IsInteger())
					{
						TArray<int64> ActualValue;
						HOUDINI_TEST_EQUAL_ON_FAIL(Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, ActualValue, 0, 1), true, return true);
						HOUDINI_TEST_EQUAL_ON_FAIL(ActualValue.Num(), 1, return true);
						HOUDINI_TEST_EQUAL(ActualValue[0], NumericProperty->GetSignedIntPropertyValue(Value));
					}
					else
					{
						AddError(FString::Printf(TEXT("Unsupported numeric property %s."), *PropertyName.ToString()));
						return true;
					}
				}
				else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
				{
					HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, *AttributeName, ActualStringValue), true, return true);
					HOUDINI_TEST_EQUAL(ActualStringValue, StringProperty->GetPropertyValue(Value));
				}
				else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
				{
					HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, *AttributeName, ActualStringValue), true, return true);
					HOUDINI_TEST_EQUAL(ActualStringValue, TextProperty->GetPropertyValue(Value).ToString());
				}
				else
				{
					AddError(FString::Printf(TEXT("Unsupported TestDataAsset property %s (%s)."), *PropertyName.ToString(), *Property->GetClass()->GetName()));
					return true;
				}
			}

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DataAssetTypedAttributes, "Houdini.UnitTests.Inputs.DataAsset.TypedAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DataAssetTypedAttributes::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UHoudiniEditorTestTypedDataAsset* DataAsset = NewObject<UHoudiniEditorTestTypedDataAsset>(GetTransientPackage());
	HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return false);
	DataAsset->AddToRoot();
	UHoudiniEditorTestTypedDataAsset* ChildDataAsset = NewObject<UHoudiniEditorTestTypedDataAsset>(GetTransientPackage());
	HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildDataAsset, return false);
	UDataAsset* MapKeyDataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestDataAsset.TestDataAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(MapKeyDataAsset, return false);
	ChildDataAsset->VectorValue = FVector(404.0, 505.0, 606.0);
	ChildDataAsset->ChildDataAsset = DataAsset;
	ChildDataAsset->AddToRoot();
	DataAsset->ChildDataAsset = ChildDataAsset;
	DataAsset->GenericDataAsset = ChildDataAsset;
	DataAsset->SoftChildDataAsset = ChildDataAsset;
	DataAsset->ChildDataAssetArray.Add(ChildDataAsset);
	FHoudiniEditorTestDataAssetContainer DataAssetContainer;
	DataAssetContainer.DataAsset = ChildDataAsset;
	DataAsset->DataAssetContainers.Add(DataAssetContainer);
	DataAsset->ChildDataAssetSet.Add(ChildDataAsset);
	DataAsset->ChildDataAssetMap.Add(TEXT("NestedDataAsset"), ChildDataAsset);
	DataAsset->DataAssetKeyMap.Add(MapKeyDataAsset, 42);

	// Complete the initial cook so the HDA's input objects have been created before assigning the Data Asset.
	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset, ChildDataAsset, MapKeyDataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			// InputAsOutput passes the Data Asset point through so the typed HAPI attributes can be inspected below.
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset, ChildDataAsset, MapKeyDataAsset]()
		{
			TArray<HAPI_NodeId> OutputNodes;
			HOUDINI_TEST_EQUAL_ON_FAIL(FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			const HAPI_NodeId NodeId = OutputNodes[0];
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniHapiAccessor Accessor(NodeId, 0, "VectorValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(Accessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL(AttributeInfo.storage, HAPI_STORAGETYPE_FLOAT);
			HOUDINI_TEST_EQUAL(AttributeInfo.typeInfo, HAPI_ATTRIBUTE_TYPE_VECTOR);
			HOUDINI_TEST_EQUAL(AttributeInfo.tupleSize, 3);
			TArray<float> VectorValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(Accessor.GetAttributeData(AttributeInfo, VectorValues, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(VectorValues.Num(), 3, return true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(VectorValues[0], 101.0f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(VectorValues[1], 202.0f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(VectorValues[2], 303.0f), true);

			FHoudiniHapiAccessor ColorAccessor(NodeId, 0, "ColorValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(ColorAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL(AttributeInfo.storage, HAPI_STORAGETYPE_UINT8);
			HOUDINI_TEST_EQUAL(AttributeInfo.tupleSize, 4);
			TArray<uint8> ColorValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(ColorAccessor.GetAttributeData(AttributeInfo, ColorValues, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(ColorValues.Num(), 4, return true);
			HOUDINI_TEST_EQUAL(ColorValues[0], 4);
			HOUDINI_TEST_EQUAL(ColorValues[1], 55);
			HOUDINI_TEST_EQUAL(ColorValues[2], 123);
			HOUDINI_TEST_EQUAL(ColorValues[3], 246);

			FHoudiniHapiAccessor LinearColorAccessor(NodeId, 0, "LinearColorValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(LinearColorAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL(AttributeInfo.storage, HAPI_STORAGETYPE_FLOAT);
			HOUDINI_TEST_EQUAL(AttributeInfo.tupleSize, 4);
			TArray<float> LinearColorValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(LinearColorAccessor.GetAttributeData(AttributeInfo, LinearColorValues, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(LinearColorValues.Num(), 4, return true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(LinearColorValues[0], 0.125f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(LinearColorValues[1], 0.25f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(LinearColorValues[2], 0.5f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(LinearColorValues[3], 0.75f), true);

			FString StructStringValue;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, TEXT("StructValue_StringValue"), StructStringValue), true, return true);
			HOUDINI_TEST_EQUAL(StructStringValue, TEXT("StructValue"));

			FHoudiniHapiAccessor NestedValueAccessor(NodeId, 0, "StructValue_NestedValue_IntegerValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedValueAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL(AttributeInfo.storage, HAPI_STORAGETYPE_INT64);
			TArray<int64> NestedValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedValueAccessor.GetAttributeData(AttributeInfo, NestedValues, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedValues.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(NestedValues[0], 789);

			FHoudiniHapiAccessor StructArrayCountAccessor(NodeId, 0, "StructArrayValue_num");
			HOUDINI_TEST_EQUAL_ON_FAIL(StructArrayCountAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL(AttributeInfo.storage, HAPI_STORAGETYPE_INT64);
			TArray<int64> StructArrayCounts;
			HOUDINI_TEST_EQUAL_ON_FAIL(StructArrayCountAccessor.GetAttributeData(AttributeInfo, StructArrayCounts, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(StructArrayCounts.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(StructArrayCounts[0], 2);

			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, TEXT("StructArrayValue_0_StringValue"), StructStringValue), true, return true);
			HOUDINI_TEST_EQUAL(StructStringValue, TEXT("FirstStructArrayValue"));
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, TEXT("StructArrayValue_1_StringValue"), StructStringValue), true, return true);
			HOUDINI_TEST_EQUAL(StructStringValue, TEXT("SecondStructArrayValue"));

			FHoudiniHapiAccessor StructArrayIntegerAccessor(NodeId, 0, "StructArrayValue_0_NestedValue_IntegerValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(StructArrayIntegerAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL(AttributeInfo.storage, HAPI_STORAGETYPE_INT64);
			TArray<int64> StructArrayIntegerValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(StructArrayIntegerAccessor.GetAttributeData(AttributeInfo, StructArrayIntegerValues, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(StructArrayIntegerValues.Num(), 1, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], 123);

			FHoudiniHapiAccessor NestedStructArrayCountAccessor(NodeId, 0, "StructArrayValue_0_NestedArrayValue_num");
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedStructArrayCountAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedStructArrayCountAccessor.GetAttributeData(AttributeInfo, StructArrayCounts, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayCounts[0], 2);

			FHoudiniHapiAccessor NestedStructArrayAccessor(NodeId, 0, "StructArrayValue_0_NestedArrayValue_1_IntegerValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedStructArrayAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(NestedStructArrayAccessor.GetAttributeData(AttributeInfo, StructArrayIntegerValues, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], 654);

			auto GetDataAssetPointIndex = [NodeId](const TCHAR* AttributeName, int32 PointIndex, int64& OutDataAssetPointIndex)
			{
				FHoudiniHapiAccessor ReferenceAccessor(NodeId, 0, AttributeName);
				HAPI_AttributeInfo ReferenceAttributeInfo;
				if (!ReferenceAccessor.GetInfo(ReferenceAttributeInfo, HAPI_ATTROWNER_POINT)
					|| ReferenceAttributeInfo.storage != HAPI_STORAGETYPE_INT64 || ReferenceAttributeInfo.tupleSize != 1)
				{
					return false;
				}

				TArray<int64> ReferenceValues;
				if (!ReferenceAccessor.GetAttributeData(ReferenceAttributeInfo, ReferenceValues, PointIndex, 1) || ReferenceValues.Num() != 1)
					return false;

				OutDataAssetPointIndex = ReferenceValues[0];
				return true;
			};

			int64 ChildDataAssetPointIndex = INDEX_NONE;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetDataAssetPointIndex(TEXT("ChildDataAsset"), 0, ChildDataAssetPointIndex), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetPointIndex >= 0, true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(GetDataAssetPointIndex(TEXT("ChildDataAssetArray_0"), 0, StructArrayIntegerValues[0]), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], ChildDataAssetPointIndex);

			FHoudiniHapiAccessor ChildDataAssetSetCountAccessor(NodeId, 0, "ChildDataAssetSet_num");
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetSetCountAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetSetCountAccessor.GetAttributeData(AttributeInfo, StructArrayCounts, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayCounts[0], 1);
			HOUDINI_TEST_EQUAL_ON_FAIL(GetDataAssetPointIndex(TEXT("ChildDataAssetSet_0"), 0, StructArrayIntegerValues[0]), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], ChildDataAssetPointIndex);

			FHoudiniHapiAccessor ChildDataAssetMapCountAccessor(NodeId, 0, "ChildDataAssetMap_num");
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetMapCountAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetMapCountAccessor.GetAttributeData(AttributeInfo, StructArrayCounts, 0, 1), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayCounts[0], 1);
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointStringAttribute(NodeId, TEXT("ChildDataAssetMap_0_key"), StructStringValue), true, return true);
			HOUDINI_TEST_EQUAL(StructStringValue, TEXT("NestedDataAsset"));
			HOUDINI_TEST_EQUAL_ON_FAIL(GetDataAssetPointIndex(TEXT("ChildDataAssetMap_0_value"), 0, StructArrayIntegerValues[0]), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], ChildDataAssetPointIndex);

			FHoudiniHapiAccessor ChildVectorAccessor(NodeId, 0, "VectorValue");
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildVectorAccessor.GetInfo(AttributeInfo, HAPI_ATTROWNER_POINT), true, return true);
			TArray<float> ChildVectorValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildVectorAccessor.GetAttributeData(AttributeInfo, ChildVectorValues, static_cast<int32>(ChildDataAssetPointIndex), 1), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildVectorValues.Num(), 3, return true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(ChildVectorValues[0], 404.0f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(ChildVectorValues[1], 505.0f), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(ChildVectorValues[2], 606.0f), true);

			HOUDINI_TEST_EQUAL_ON_FAIL(GetDataAssetPointIndex(TEXT("SoftChildDataAsset"), 0, StructArrayIntegerValues[0]), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], ChildDataAssetPointIndex);

			// The child points back to the root point rather than adding a third level of attributes.
			HOUDINI_TEST_EQUAL_ON_FAIL(GetDataAssetPointIndex(TEXT("ChildDataAsset"), static_cast<int32>(ChildDataAssetPointIndex), StructArrayIntegerValues[0]), true, return true);
			HOUDINI_TEST_EQUAL(StructArrayIntegerValues[0], 0);

			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			Input->SetUseDictionaryForDataAssetValues(true);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset, ChildDataAsset, MapKeyDataAsset]()
		{
			TArray<HAPI_NodeId> OutputNodes;
			HOUDINI_TEST_EQUAL_ON_FAIL(FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			FString DictionaryJson;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointDictionaryAttribute(
				OutputNodes[0], TEXT("unreal_data_asset_values"), DictionaryJson), true, return true);

			TSharedPtr<FJsonObject> RootValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(ParseJsonObject(DictionaryJson, RootValues), true, return true);
			const TSharedPtr<FJsonValue> ChildValuesField = RootValues->TryGetField(TEXT("ChildDataAsset"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildValuesField.Get(), return true);
			const TSharedPtr<FJsonObject> ChildValues = ChildValuesField->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildValues.Get(), return true);
			FString ChildDataAssetPath;
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildValues->TryGetStringField(TEXT("__object_path"), ChildDataAssetPath), true, return true);
			HOUDINI_TEST_EQUAL(ChildDataAssetPath, ChildDataAsset->GetPathName());

			const TSharedPtr<FJsonObject> GenericDataAsset = RootValues->GetObjectField(TEXT("GenericDataAsset"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(GenericDataAsset.Get(), return true);
			HOUDINI_TEST_EQUAL(GenericDataAsset->GetStringField(TEXT("__object_path")), ChildDataAsset->GetPathName());
			const TSharedPtr<FJsonValue> ChildVectorField = ChildValues->TryGetField(TEXT("VectorValue"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildVectorField.Get(), return true);
			const TSharedPtr<FJsonObject> ChildVectorValues = ChildVectorField->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildVectorValues.Get(), return true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(ChildVectorValues->GetNumberField(TEXT("X")), 404.0), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(ChildVectorValues->GetNumberField(TEXT("Y")), 505.0), true);
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(ChildVectorValues->GetNumberField(TEXT("Z")), 606.0), true);

			// The child points back to the root, so that entry contains only its path.
			const TSharedPtr<FJsonValue> LoopField = ChildValues->TryGetField(TEXT("ChildDataAsset"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(LoopField.Get(), return true);
			const TSharedPtr<FJsonObject> LoopValues = LoopField->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(LoopValues.Get(), return true);
			HOUDINI_TEST_EQUAL(LoopValues->Values.Num(), 1);
			HOUDINI_TEST_EQUAL(LoopValues->GetStringField(TEXT("__object_path")), DataAsset->GetPathName());

			const TSharedPtr<FJsonValue> SoftChildValuesField = RootValues->TryGetField(TEXT("SoftChildDataAsset"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(SoftChildValuesField.Get(), return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(SoftChildValuesField->AsObject().Get(), return true);

			const TArray<TSharedPtr<FJsonValue>>& ChildDataAssetArray = RootValues->GetArrayField(TEXT("ChildDataAssetArray"));
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetArray.Num(), 1, return true);
			const TSharedPtr<FJsonObject> ChildDataAssetArrayValue = ChildDataAssetArray[0]->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildDataAssetArrayValue.Get(), return true);
			HOUDINI_TEST_EQUAL(ChildDataAssetArrayValue->GetStringField(TEXT("__object_path")), ChildDataAsset->GetPathName());

			const TArray<TSharedPtr<FJsonValue>>& DataAssetContainers = RootValues->GetArrayField(TEXT("DataAssetContainers"));
			HOUDINI_TEST_EQUAL_ON_FAIL(DataAssetContainers.Num(), 1, return true);
			const TSharedPtr<FJsonObject> DataAssetContainer = DataAssetContainers[0]->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAssetContainer.Get(), return true);
			const TSharedPtr<FJsonObject> ContainedDataAsset = DataAssetContainer->GetObjectField(TEXT("DataAsset"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ContainedDataAsset.Get(), return true);
			HOUDINI_TEST_EQUAL(ContainedDataAsset->GetStringField(TEXT("__object_path")), ChildDataAsset->GetPathName());

			const TArray<TSharedPtr<FJsonValue>>& ChildDataAssetSet = RootValues->GetArrayField(TEXT("ChildDataAssetSet"));
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetSet.Num(), 1, return true);
			const TSharedPtr<FJsonObject> ChildDataAssetSetValue = ChildDataAssetSet[0]->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildDataAssetSetValue.Get(), return true);
			HOUDINI_TEST_EQUAL(ChildDataAssetSetValue->GetStringField(TEXT("__object_path")), ChildDataAsset->GetPathName());

			const TArray<TSharedPtr<FJsonValue>>& ChildDataAssetMap = RootValues->GetArrayField(TEXT("ChildDataAssetMap"));
			HOUDINI_TEST_EQUAL_ON_FAIL(ChildDataAssetMap.Num(), 1, return true);
			const TSharedPtr<FJsonObject> ChildDataAssetMapEntry = ChildDataAssetMap[0]->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildDataAssetMapEntry.Get(), return true);
			HOUDINI_TEST_EQUAL(ChildDataAssetMapEntry->GetStringField(TEXT("__key")), TEXT("NestedDataAsset"));
			const TSharedPtr<FJsonObject> ChildDataAssetMapValue = ChildDataAssetMapEntry->GetObjectField(TEXT("__value"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(ChildDataAssetMapValue.Get(), return true);
			HOUDINI_TEST_EQUAL(ChildDataAssetMapValue->GetStringField(TEXT("__object_path")), ChildDataAsset->GetPathName());

			const TArray<TSharedPtr<FJsonValue>>& DataAssetKeyMap = RootValues->GetArrayField(TEXT("DataAssetKeyMap"));
			HOUDINI_TEST_EQUAL_ON_FAIL(DataAssetKeyMap.Num(), 1, return true);
			const TSharedPtr<FJsonObject> DataAssetKeyMapEntry = DataAssetKeyMap[0]->AsObject();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAssetKeyMapEntry.Get(), return true);
			const TSharedPtr<FJsonObject> DataAssetKey = DataAssetKeyMapEntry->GetObjectField(TEXT("__key"));
			HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAssetKey.Get(), return true);
			HOUDINI_TEST_EQUAL(DataAssetKey->GetStringField(TEXT("__object_path")), MapKeyDataAsset->GetPathName());
			HOUDINI_TEST_EQUAL(FMath::IsNearlyEqual(DataAssetKeyMapEntry->GetNumberField(TEXT("__value")), 42.0), true);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [DataAsset, ChildDataAsset]()
		{
			ChildDataAsset->RemoveFromRoot();
			DataAsset->RemoveFromRoot();
			return true;
		}));

	return true;
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DataAssetPrimaryAssetArrayDictionary, "Houdini.UnitTests.Inputs.DataAsset.PrimaryAssetArrayDictionary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DataAssetPrimaryAssetArrayDictionary::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	// Keep this regression self-contained: every Species element is a primary Data Asset with a
	// populated Variant array, so the exported dictionary must keep each array element as an object.
	UHoudiniEditorTestPrimaryDataAssetArray* DataAsset = NewObject<UHoudiniEditorTestPrimaryDataAssetArray>(GetTransientPackage());
	UHoudiniEditorTestPrimaryDataAsset* FirstSpecies = NewObject<UHoudiniEditorTestPrimaryDataAsset>(GetTransientPackage());
	UHoudiniEditorTestPrimaryDataAsset* SecondSpecies = NewObject<UHoudiniEditorTestPrimaryDataAsset>(GetTransientPackage());
	UHoudiniEditorTestTypedDataAsset* Variant = NewObject<UHoudiniEditorTestTypedDataAsset>(GetTransientPackage());
	HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return false);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(FirstSpecies, return false);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(SecondSpecies, return false);
	HOUDINI_TEST_NOT_NULL_ON_FAIL(Variant, return false);
	DataAsset->AddToRoot();
	FirstSpecies->AddToRoot();
	SecondSpecies->AddToRoot();
	Variant->AddToRoot();
	FirstSpecies->Variant.Add(Variant);
	SecondSpecies->Variant.Add(Variant);
	DataAsset->Species = { FirstSpecies, SecondSpecies };

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			Input->SetUseDictionaryForDataAssetValues(true);
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			TArray<HAPI_NodeId> OutputNodes;
			HOUDINI_TEST_EQUAL_ON_FAIL(FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			FString DictionaryJson;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointDictionaryAttribute(
				OutputNodes[0], TEXT("unreal_data_asset_values"), DictionaryJson), true, return true);

			TSharedPtr<FJsonObject> RootValues;
			HOUDINI_TEST_EQUAL_ON_FAIL(ParseJsonObject(DictionaryJson, RootValues), true, return true);
			HOUDINI_TEST_EQUAL(RootValues->GetStringField(TEXT("__object_path")), DataAsset->GetPathName());

			FArrayProperty* SpeciesProperty = CastField<FArrayProperty>(DataAsset->GetClass()->FindPropertyByName(TEXT("Species")));
			FObjectPropertyBase* SpeciesElementProperty = SpeciesProperty ? CastField<FObjectPropertyBase>(SpeciesProperty->Inner) : nullptr;
			void* SpeciesValue = SpeciesProperty ? SpeciesProperty->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
			HOUDINI_TEST_NOT_NULL_ON_FAIL(SpeciesElementProperty, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(SpeciesValue, return true);

			FScriptArrayHelper SpeciesHelper(SpeciesProperty, SpeciesValue);
			const TArray<TSharedPtr<FJsonValue>>& SpeciesValues = RootValues->GetArrayField(TEXT("Species"));
			HOUDINI_TEST_EQUAL_ON_FAIL(SpeciesValues.Num(), SpeciesHelper.Num(), return true);

			for (int32 Index = 0; Index < SpeciesHelper.Num(); ++Index)
			{
				UDataAsset* ExpectedSpecies = Cast<UDataAsset>(
					SpeciesElementProperty->GetObjectPropertyValue(SpeciesHelper.GetRawPtr(Index)));

				const TSharedPtr<FJsonObject> SpeciesValuesObject = SpeciesValues[Index]->AsObject();
				HOUDINI_TEST_NOT_NULL_ON_FAIL(SpeciesValuesObject.Get(), return true);
				HOUDINI_TEST_EQUAL(SpeciesValuesObject->GetStringField(TEXT("__object_path")), ExpectedSpecies ? ExpectedSpecies->GetPathName() : FString(TEXT("None")));
				if (!ExpectedSpecies)
					continue;

				const TArray<TSharedPtr<FJsonValue>>& VariantValues = SpeciesValuesObject->GetArrayField(TEXT("Variant"));
				HOUDINI_TEST_EQUAL(VariantValues.Num() > 0, true);
				HOUDINI_TEST_NOT_NULL_ON_FAIL(VariantValues[0]->AsObject().Get(), return true);
			}

			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [DataAsset, FirstSpecies, SecondSpecies, Variant]()
		{
			Variant->RemoveFromRoot();
			SecondSpecies->RemoveFromRoot();
			FirstSpecies->RemoveFromRoot();
			DataAsset->RemoveFromRoot();
			return true;
		}));

	return true;
}
#endif

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DataAssetBlackboardDictionary, "Houdini.UnitTests.Inputs.DataAsset.BlackboardDictionary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DataAssetBlackboardDictionary::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDataAsset* DataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestBlackboard.TestBlackboard")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			Input->SetUseDictionaryForDataAssetValues(true);
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			TArray<HAPI_NodeId> OutputNodes;
			const bool bGatheredOutputs = FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes);
			HOUDINI_TEST_EQUAL_ON_FAIL(bGatheredOutputs, true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			const HAPI_NodeId NodeId = OutputNodes[0];
			FString ActualJson;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointDictionaryAttribute(NodeId, TEXT("unreal_data_asset_values"), ActualJson), true, return true);

			TSharedPtr<FJsonObject> ActualJsonObject;
			TSharedPtr<FJsonObject> ExpectedJsonObject = MakeShared<FJsonObject>();
			HOUDINI_TEST_EQUAL_ON_FAIL(ParseJsonObject(ActualJson, ActualJsonObject), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(FJsonObjectConverter::UStructToJsonObject(
				DataAsset->GetClass(), DataAsset, ExpectedJsonObject.ToSharedRef(), CPF_Edit, CPF_Transient | CPF_Deprecated,
				nullptr, EJsonObjectConversionFlags::SkipStandardizeCase), true, return true);
			ExpectedJsonObject->SetStringField(TEXT("__object_path"), DataAsset->GetPathName());
			ExpectedJsonObject->SetStringField(TEXT("__class_path"), DataAsset->GetClass()->GetPathName());
			TSharedRef<FJsonObject> ParentDataAssetValues = MakeShared<FJsonObject>();
			ParentDataAssetValues->SetStringField(TEXT("__object_path"), TEXT("None"));
			ExpectedJsonObject->SetObjectField(TEXT("Parent"), ParentDataAssetValues);
			HOUDINI_TEST_EQUAL(AreJsonObjectsEqual(ActualJsonObject, ExpectedJsonObject), true);

			// Dictionary mode replaces individual property attributes with the single JSON dictionary.
			HOUDINI_TEST_EQUAL(FHoudiniEngineUtils::HapiCheckAttributeExists(NodeId, 0, "Keys", HAPI_ATTROWNER_POINT), false);
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestInput_DataAssetBlueprintDictionary, "Houdini.UnitTests.Inputs.DataAsset.BlueprintDictionary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestInput_DataAssetBlueprintDictionary::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDataAsset* DataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestDataAsset.TestDataAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			Input->SetUseDictionaryForDataAssetValues(true);
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, DataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, DataAsset]()
		{
			TArray<HAPI_NodeId> OutputNodes;
			const bool bGatheredOutputs = FHoudiniEngineUtils::GatherAllAssetOutputs(
				Context->HAC->GetAssetId(), true, false, false, OutputNodes);
			HOUDINI_TEST_EQUAL_ON_FAIL(bGatheredOutputs, true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputNodes.Num(), 1, return true);

			const HAPI_NodeId NodeId = OutputNodes[0];
			FString ActualJson;
			HOUDINI_TEST_EQUAL_ON_FAIL(GetPointDictionaryAttribute(NodeId, TEXT("unreal_data_asset_values"), ActualJson), true, return true);

			TSharedPtr<FJsonObject> ActualJsonObject;
			TSharedPtr<FJsonObject> ExpectedJsonObject = MakeShared<FJsonObject>();
			HOUDINI_TEST_EQUAL_ON_FAIL(ParseJsonObject(ActualJson, ActualJsonObject), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(FJsonObjectConverter::UStructToJsonObject(
				DataAsset->GetClass(), DataAsset, ExpectedJsonObject.ToSharedRef(), CPF_Edit, CPF_Transient | CPF_Deprecated,
				nullptr, EJsonObjectConversionFlags::SkipStandardizeCase), true, return true);
			ExpectedJsonObject->SetStringField(TEXT("__object_path"), DataAsset->GetPathName());
			ExpectedJsonObject->SetStringField(TEXT("__class_path"), DataAsset->GetClass()->GetPathName());
			HOUDINI_TEST_EQUAL(AreJsonObjectsEqual(ActualJsonObject, ExpectedJsonObject), true);

			HOUDINI_TEST_EQUAL(FHoudiniEngineUtils::HapiCheckAttributeExists(NodeId, 0, "Value1", HAPI_ATTROWNER_POINT), false);
			HOUDINI_TEST_EQUAL(FHoudiniEngineUtils::HapiCheckAttributeExists(NodeId, 0, "Value2", HAPI_ATTROWNER_POINT), false);
			HOUDINI_TEST_EQUAL(FHoudiniEngineUtils::HapiCheckAttributeExists(NodeId, 0, "Value3", HAPI_ATTROWNER_POINT), false);
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput_DataAssetTwoAssets, "Houdini.UnitTests.Outputs.DataAsset.TwoAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput_DataAssetTwoAssets::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/DataAssets/Test_DataAssets"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 2, return true);

			TSet<int64> Value1Values;
			for (UHoudiniOutput* Output : Outputs)
			{
				HOUDINI_TEST_NOT_NULL_ON_FAIL(Output, return true);
				HOUDINI_TEST_EQUAL_ON_FAIL(Output->GetType(), EHoudiniOutputType::DataAsset, return true);

				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
				HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);

				for (TPair<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjectPair : OutputObjects)
				{
					UDataAsset* DataAsset = Cast<UDataAsset>(OutputObjectPair.Value.OutputObject);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(DataAsset->GetClass()->GetPathName(), TEXT("/Game/TestHDAs/DataAssets/TestDataClass.TestDataClass_C"), return true);

					// The HDA emits two distinct dictionaries. Verify their values after they have been applied
					// to the temporary Data Assets, rather than only checking that two objects were created.
					FProperty* Value1Property = DataAsset->GetClass()->FindPropertyByName(TEXT("Value1"));
					void* Value1 = Value1Property ? Value1Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
					FNumericProperty* NumericValue1Property = CastField<FNumericProperty>(Value1Property);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(NumericValue1Property, return true);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Value1, return true);

					int64 Value1AsInteger = NumericValue1Property->IsFloatingPoint()
						? FMath::RoundToInt64(NumericValue1Property->GetFloatingPointPropertyValue(Value1))
						: NumericValue1Property->GetSignedIntPropertyValue(Value1);
					HOUDINI_TEST_EQUAL_ON_FAIL(Value1AsInteger == 1 || Value1AsInteger == 2, true, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(Value1Values.Contains(Value1AsInteger), false, return true);
					Value1Values.Add(Value1AsInteger);

					FString ExpectedValue2 = Value1AsInteger == 1 ? TEXT("First Data Asset") : TEXT("Second Data Asset");
					FProperty* Value2Property = DataAsset->GetClass()->FindPropertyByName(TEXT("Value2"));
					void* Value2 = Value2Property ? Value2Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Value2Property, return true);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Value2, return true);

					if (FStrProperty* StringProperty = CastField<FStrProperty>(Value2Property))
					{
						HOUDINI_TEST_EQUAL_ON_FAIL(StringProperty->GetPropertyValue(Value2), ExpectedValue2, return true);
					}
					else if (FTextProperty* TextProperty = CastField<FTextProperty>(Value2Property))
					{
						HOUDINI_TEST_EQUAL_ON_FAIL(TextProperty->GetPropertyValue(Value2).ToString(), ExpectedValue2, return true);
					}
					else
					{
						AddError(FString::Printf(TEXT("Unsupported TestDataClass property Value2 (%s)."), *Value2Property->GetClass()->GetName()));
						return true;
					}

					FProperty* Value3Property = DataAsset->GetClass()->FindPropertyByName(TEXT("Value3"));
					void* Value3 = Value3Property ? Value3Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
					FNumericProperty* NumericValue3Property = CastField<FNumericProperty>(Value3Property);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(NumericValue3Property, return true);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(Value3, return true);

					double ExpectedValue3 = Value1AsInteger == 1 ? 10.5 : 20.5;
					if (NumericValue3Property->IsFloatingPoint())
					{
						HOUDINI_TEST_EQUAL_ON_FAIL(FMath::IsNearlyEqual(NumericValue3Property->GetFloatingPointPropertyValue(Value3), ExpectedValue3), true, return true);
					}
					else if (NumericValue3Property->IsInteger())
					{
						HOUDINI_TEST_EQUAL_ON_FAIL(NumericValue3Property->GetSignedIntPropertyValue(Value3), static_cast<int64>(ExpectedValue3), return true);
					}
					else
					{
						AddError(FString::Printf(TEXT("Unsupported TestDataClass property Value3 (%s)."), *Value3Property->GetClass()->GetName()));
						return true;
					}
				}
			}

			HOUDINI_TEST_EQUAL(Value1Values.Num(), 2);
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput_DataAssetAttributes, "Houdini.UnitTests.Outputs.DataAsset.Attributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput_DataAssetAttributes::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDataAsset* SourceDataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestDataAsset.TestDataAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceDataAsset, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SourceDataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			// Keep the default attribute representation so this round-trip exercises the non-dictionary output path.
			Input->SetUseDictionaryForDataAssetValues(false);
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, SourceDataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SourceDataAsset]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs[0]->GetType(), EHoudiniOutputType::DataAsset, return true);

			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Outputs[0]->GetOutputObjects();
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
			UDataAsset* OutputDataAsset = Cast<UDataAsset>(OutputObjects.CreateConstIterator()->Value.OutputObject);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputDataAsset, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputDataAsset->GetClass(), SourceDataAsset->GetClass(), return true);

			// Compare the properties exported by the default attribute mode. The Data Asset test class
			// intentionally covers a number, a string, and a floating-point number.
			for (FName PropertyName : { FName(TEXT("Value1")), FName(TEXT("Value2")), FName(TEXT("Value3")) })
			{
				FProperty* SourceProperty = SourceDataAsset->GetClass()->FindPropertyByName(PropertyName);
				FProperty* OutputProperty = OutputDataAsset->GetClass()->FindPropertyByName(PropertyName);
				void* SourceValue = SourceProperty ? SourceProperty->ContainerPtrToValuePtr<void>(SourceDataAsset) : nullptr;
				void* OutputValue = OutputProperty ? OutputProperty->ContainerPtrToValuePtr<void>(OutputDataAsset) : nullptr;
				HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceProperty, return true);
				HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputProperty, return true);
				HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceValue, return true);
				HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputValue, return true);

				if (FNumericProperty* SourceNumericProperty = CastField<FNumericProperty>(SourceProperty))
				{
					FNumericProperty* OutputNumericProperty = CastField<FNumericProperty>(OutputProperty);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputNumericProperty, return true);
					if (SourceNumericProperty->IsFloatingPoint())
					{
						HOUDINI_TEST_EQUAL_ON_FAIL(FMath::IsNearlyEqual(SourceNumericProperty->GetFloatingPointPropertyValue(SourceValue), OutputNumericProperty->GetFloatingPointPropertyValue(OutputValue)), true, return true);
					}
					else if (SourceNumericProperty->IsInteger())
					{
						HOUDINI_TEST_EQUAL_ON_FAIL(SourceNumericProperty->GetSignedIntPropertyValue(SourceValue), OutputNumericProperty->GetSignedIntPropertyValue(OutputValue), return true);
					}
					else
					{
						AddError(FString::Printf(TEXT("Unsupported numeric TestDataAsset property %s."), *PropertyName.ToString()));
						return true;
					}
				}
				else if (FStrProperty* SourceStringProperty = CastField<FStrProperty>(SourceProperty))
				{
					FStrProperty* OutputStringProperty = CastField<FStrProperty>(OutputProperty);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputStringProperty, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(SourceStringProperty->GetPropertyValue(SourceValue), OutputStringProperty->GetPropertyValue(OutputValue), return true);
				}
				else if (FTextProperty* SourceTextProperty = CastField<FTextProperty>(SourceProperty))
				{
					FTextProperty* OutputTextProperty = CastField<FTextProperty>(OutputProperty);
					HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputTextProperty, return true);
					HOUDINI_TEST_EQUAL_ON_FAIL(SourceTextProperty->GetPropertyValue(SourceValue).ToString(), OutputTextProperty->GetPropertyValue(OutputValue).ToString(), return true);
				}
				else
				{
					AddError(FString::Printf(TEXT("Unsupported TestDataAsset property %s (%s)."), *PropertyName.ToString(), *SourceProperty->GetClass()->GetName()));
					return true;
				}
			}

			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput_DataAssetDictionary, "Houdini.UnitTests.Outputs.DataAsset.Dictionary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput_DataAssetDictionary::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/Inputs/InputAsOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UDataAsset* SourceDataAsset = Cast<UDataAsset>(StaticLoadObject(
		UDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/TestDataAsset.TestDataAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(SourceDataAsset, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SourceDataAsset]()
		{
			UHoudiniInput* Input = Context->GetInputAt(0);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Input, return true);

			Input->SetUseDictionaryForDataAssetValues(true);
			Input->SetInputObjectAt(EHoudiniInputType::Geometry, 0, SourceDataAsset);
			Input->MarkChanged(true);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, SourceDataAsset]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs[0]->GetType(), EHoudiniOutputType::DataAsset, return true);

			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Outputs[0]->GetOutputObjects();
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
			UDataAsset* OutputDataAsset = Cast<UDataAsset>(OutputObjects.CreateConstIterator()->Value.OutputObject);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputDataAsset, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputDataAsset->GetClass(), SourceDataAsset->GetClass(), return true);

			// Compare the reflected JSON to cover every editable property carried by the dictionary.
			FString SourceJson;
			FString OutputJson;
			HOUDINI_TEST_EQUAL_ON_FAIL(FJsonObjectConverter::UStructToJsonObjectString(
				SourceDataAsset->GetClass(), SourceDataAsset, SourceJson, CPF_Edit, CPF_Transient | CPF_Deprecated, 0, nullptr, false), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(FJsonObjectConverter::UStructToJsonObjectString(
				OutputDataAsset->GetClass(), OutputDataAsset, OutputJson, CPF_Edit, CPF_Transient | CPF_Deprecated, 0, nullptr, false), true, return true);

			TSharedPtr<FJsonObject> SourceJsonObject;
			TSharedPtr<FJsonObject> OutputJsonObject;
			HOUDINI_TEST_EQUAL_ON_FAIL(ParseJsonObject(SourceJson, SourceJsonObject), true, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(ParseJsonObject(OutputJson, OutputJsonObject), true, return true);
			HOUDINI_TEST_EQUAL(AreJsonObjectsEqual(SourceJsonObject, OutputJsonObject), true);
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput_DataAssetNested, "Houdini.UnitTests.Outputs.DataAsset.Nested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput_DataAssetNested::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/DataAssets/Test_DataAssetNestedOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UHoudiniSubDataAsset* ExistingDataAsset = Cast<UHoudiniSubDataAsset>(StaticLoadObject(
		UHoudiniSubDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/Nested/TestSubAsset.TestSubAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(ExistingDataAsset, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "use_dictionary", 1, 0);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, ExistingDataAsset]()
		{
			UHoudiniCookable* Cookable = Context->GetCookable();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Cookable, return true);
			const TArray<UHoudiniOutput*>& Outputs = Cookable->GetOutputs();
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs[0]->GetType(), EHoudiniOutputType::DataAsset, return true);

			const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Outputs[0]->GetOutputObjects();
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 3, return true);

			UHoudiniTestNestedDataAsset* RootDataAsset = nullptr;
			TSet<UObject*> CookedOutputObjects;
			for (const TPair<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& Pair : OutputObjects)
			{
				UObject* OutputObject = Pair.Value.OutputObject.Get();
				HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputObject, return true);
				CookedOutputObjects.Add(OutputObject);
				if (UHoudiniTestNestedDataAsset* DataAsset = Cast<UHoudiniTestNestedDataAsset>(OutputObject))
					RootDataAsset = DataAsset;
			}
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset, return true);
			HOUDINI_TEST_EQUAL(RootDataAsset->Float, 42.0f);
			HOUDINI_TEST_EQUAL_ON_FAIL(RootDataAsset->Children.Num(), 4, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[0], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[1], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[2], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[3], return true);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[0]->Float, 1.0f);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[1]->Float, 2.0f);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[1], RootDataAsset->Children[2]);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[3], ExistingDataAsset);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(RootDataAsset), true);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(RootDataAsset->Children[0]), true);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(RootDataAsset->Children[1]), true);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(ExistingDataAsset), false);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, ExistingDataAsset]()
		{
			UHoudiniCookable* Cookable = Context->GetCookable();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Cookable, return true);

			FHoudiniBakeSettings BakeSettings;
			HOUDINI_TEST_EQUAL_ON_FAIL(Context->Bake(BakeSettings), true, return true);

			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs[0].BakedOutputObjects.Num(), 3, return true);

			UHoudiniTestNestedDataAsset* BakedRootDataAsset = nullptr;
			TSet<UObject*> BakedOutputObjects;
			for (const TPair<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& Pair : BakedOutputs[0].BakedOutputObjects)
			{
				UObject* BakedObject = Pair.Value.GetBakedObjectIfValid();
				HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedObject, return true);
				BakedOutputObjects.Add(BakedObject);
				if (UHoudiniTestNestedDataAsset* DataAsset = Cast<UHoudiniTestNestedDataAsset>(BakedObject))
					BakedRootDataAsset = DataAsset;
			}
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset, return true);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Float, 42.0f);
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedRootDataAsset->Children.Num(), 4, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[0], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[1], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[2], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[3], return true);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[0]->Float, 1.0f);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[1]->Float, 2.0f);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[1], BakedRootDataAsset->Children[2]);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[3], ExistingDataAsset);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(BakedRootDataAsset), true);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(BakedRootDataAsset->Children[0]), true);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(BakedRootDataAsset->Children[1]), true);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(ExistingDataAsset), false);
			const FString FirstChildPackageName = BakedRootDataAsset->Children[0]->GetOutermost()->GetName();
			HOUDINI_TEST_EQUAL(FPaths::GetPath(FirstChildPackageName), TEXT("/Game/HoudiniEngine/Bake/DataAssets"));
			HOUDINI_TEST_EQUAL(FPaths::GetCleanFilename(FirstChildPackageName).StartsWith(TEXT("DataAssetCustomName")), true);

			FHoudiniEngineBakeUtils::DeleteBakedOutputs(Cookable);
			HOUDINI_TEST_EQUAL(Context->GetBakedOutputs().Num(), 0);
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput_DataAssetNestedPointAttributes, "Houdini.UnitTests.Outputs.DataAsset.NestedPointAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput_DataAssetNestedPointAttributes::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/DataAssets/Test_DataAssetNestedOutput"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	UHoudiniSubDataAsset* ExistingDataAsset = Cast<UHoudiniSubDataAsset>(StaticLoadObject(
		UHoudiniSubDataAsset::StaticClass(), nullptr, TEXT("/Game/TestHDAs/DataAssets/Nested/TestSubAsset.TestSubAsset")));
	HOUDINI_TEST_NOT_NULL_ON_FAIL(ExistingDataAsset, return false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			SET_HDA_PARAMETER(Context, UHoudiniParameterInt, "use_dictionary", 0, 0);
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, ExistingDataAsset]()
		{
			UHoudiniCookable* Cookable = Context->GetCookable();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Cookable, return true);
			const TArray<UHoudiniOutput*>& Outputs = Cookable->GetOutputs();
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs[0]->GetType(), EHoudiniOutputType::DataAsset, return true);

			const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Outputs[0]->GetOutputObjects();
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 3, return true);

			UHoudiniTestNestedDataAsset* RootDataAsset = nullptr;
			TSet<UObject*> CookedOutputObjects;
			for (const TPair<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& Pair : OutputObjects)
			{
				UObject* OutputObject = Pair.Value.OutputObject.Get();
				HOUDINI_TEST_NOT_NULL_ON_FAIL(OutputObject, return true);
				CookedOutputObjects.Add(OutputObject);
				if (UHoudiniTestNestedDataAsset* DataAsset = Cast<UHoudiniTestNestedDataAsset>(OutputObject))
					RootDataAsset = DataAsset;
			}
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset, return true);
			HOUDINI_TEST_EQUAL(RootDataAsset->Float, 42.0f);
			HOUDINI_TEST_EQUAL_ON_FAIL(RootDataAsset->Children.Num(), 4, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[0], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[1], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[2], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RootDataAsset->Children[3], return true);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[0]->Float, 1.0f);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[1]->Float, 2.0f);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[1], RootDataAsset->Children[2]);
			HOUDINI_TEST_EQUAL(RootDataAsset->Children[3], ExistingDataAsset);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(RootDataAsset), true);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(RootDataAsset->Children[0]), true);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(RootDataAsset->Children[1]), true);
			HOUDINI_TEST_EQUAL(CookedOutputObjects.Contains(ExistingDataAsset), false);
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, ExistingDataAsset]()
		{
			UHoudiniCookable* Cookable = Context->GetCookable();
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Cookable, return true);

			FHoudiniBakeSettings BakeSettings;
			HOUDINI_TEST_EQUAL_ON_FAIL(Context->Bake(BakeSettings), true, return true);

			TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs[0].BakedOutputObjects.Num(), 3, return true);

			UHoudiniTestNestedDataAsset* BakedRootDataAsset = nullptr;
			TSet<UObject*> BakedOutputObjects;
			for (const TPair<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& Pair : BakedOutputs[0].BakedOutputObjects)
			{
				UObject* BakedObject = Pair.Value.GetBakedObjectIfValid();
				HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedObject, return true);
				BakedOutputObjects.Add(BakedObject);
				if (UHoudiniTestNestedDataAsset* DataAsset = Cast<UHoudiniTestNestedDataAsset>(BakedObject))
					BakedRootDataAsset = DataAsset;
			}
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset, return true);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Float, 42.0f);
			HOUDINI_TEST_EQUAL_ON_FAIL(BakedRootDataAsset->Children.Num(), 4, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[0], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[1], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[2], return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(BakedRootDataAsset->Children[3], return true);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[0]->Float, 1.0f);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[1]->Float, 2.0f);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[1], BakedRootDataAsset->Children[2]);
			HOUDINI_TEST_EQUAL(BakedRootDataAsset->Children[3], ExistingDataAsset);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(BakedRootDataAsset), true);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(BakedRootDataAsset->Children[0]), true);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(BakedRootDataAsset->Children[1]), true);
			HOUDINI_TEST_EQUAL(BakedOutputObjects.Contains(ExistingDataAsset), false);
			const FString FirstChildPackageName = BakedRootDataAsset->Children[0]->GetOutermost()->GetName();
			HOUDINI_TEST_EQUAL(FPaths::GetPath(FirstChildPackageName), TEXT("/Game/HoudiniEngine/Bake/DataAssets"));
			HOUDINI_TEST_EQUAL(FPaths::GetCleanFilename(FirstChildPackageName).StartsWith(TEXT("DataAssetCustomName")), true);

			FHoudiniEngineBakeUtils::DeleteBakedOutputs(Cookable);
			HOUDINI_TEST_EQUAL(Context->GetBakedOutputs().Num(), 0);
			return true;
		}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestOutput_DataAssetAttributeDefaults, "Houdini.UnitTests.Outputs.DataAsset.AttributeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestOutput_DataAssetAttributeDefaults::RunTest(const FString& Parameters)
{
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(
		this, TEXT("/Game/TestHDAs/DataAssets/Test_DataAssetAttributes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);
	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [Context]()
		{
			Context->StartCookingHDA();
			return true;
		}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->GetOutputs(Outputs);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs[0]->GetType(), EHoudiniOutputType::DataAsset, return true);

			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Outputs[0]->GetOutputObjects();
			HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 1, return true);
			UDataAsset* DataAsset = Cast<UDataAsset>(OutputObjects.CreateConstIterator()->Value.OutputObject);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(DataAsset, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(DataAsset->GetClass()->GetPathName(), TEXT("/Game/TestHDAs/DataAssets/TestDataClass.TestDataClass_C"), return true);

			// Value1 and Value2 have matching point attributes. Value3 is deliberately absent from the HDA
			// and must remain the class default rather than making the whole Data Asset output fail.
			FProperty* Value1Property = DataAsset->GetClass()->FindPropertyByName(TEXT("Value1"));
			FProperty* Value2Property = DataAsset->GetClass()->FindPropertyByName(TEXT("Value2"));
			FProperty* Value3Property = DataAsset->GetClass()->FindPropertyByName(TEXT("Value3"));
			void* Value1 = Value1Property ? Value1Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
			void* Value2 = Value2Property ? Value2Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
			void* Value3 = Value3Property ? Value3Property->ContainerPtrToValuePtr<void>(DataAsset) : nullptr;
			UDataAsset* DefaultDataAsset = DataAsset->GetClass()->GetDefaultObject<UDataAsset>();
			void* DefaultValue3 = Value3Property && DefaultDataAsset ? Value3Property->ContainerPtrToValuePtr<void>(DefaultDataAsset) : nullptr;
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Value1, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Value2, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Value3, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(DefaultValue3, return true);

			FNumericProperty* NumericValue1Property = CastField<FNumericProperty>(Value1Property);
			FStrProperty* StringValue2Property = CastField<FStrProperty>(Value2Property);
			FNumericProperty* NumericValue3Property = CastField<FNumericProperty>(Value3Property);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(NumericValue1Property, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(StringValue2Property, return true);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(NumericValue3Property, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(NumericValue1Property->GetSignedIntPropertyValue(Value1), static_cast<int64>(42), return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(StringValue2Property->GetPropertyValue(Value2), TEXT("Attribute Value"), return true);

			if (NumericValue3Property->IsFloatingPoint())
			{
				HOUDINI_TEST_EQUAL_ON_FAIL(FMath::IsNearlyEqual(NumericValue3Property->GetFloatingPointPropertyValue(Value3), NumericValue3Property->GetFloatingPointPropertyValue(DefaultValue3)), true, return true);
			}
			else if (NumericValue3Property->IsInteger())
			{
				HOUDINI_TEST_EQUAL_ON_FAIL(NumericValue3Property->GetSignedIntPropertyValue(Value3), NumericValue3Property->GetSignedIntPropertyValue(DefaultValue3), return true);
			}
			else
			{
				AddError(FString::Printf(TEXT("Unsupported TestDataClass property Value3 (%s)."), *Value3Property->GetClass()->GetName()));
			}

			return true;
		}));

	return true;
}

#endif
