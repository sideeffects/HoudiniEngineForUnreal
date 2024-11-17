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

#include "HoudiniEditorTestLandscapes.h"

#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterToggle.h"
#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/HeightField.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "LandscapeEdit.h"
#include <HoudiniLandscapeUtils.h>

TArray<FString> FHoudiniEditorTestLandscapes::CheckLandscapeValues(
	TArray<float> & Results, 
	TArray<float> & ExpectedResults, 
	const FIntPoint & Size, 
	float AbsError, int MaxErrors)
{
	TArray<FString> Errors;
	
	for(int Y = 0; Y < Size.Y; Y++)
	{
		for (int X = 0; X < Size.X; X++)
		{
			int ResultIndex = X + Y * Size.X;
			int ExpectedIndex = X + Y * Size.X;
			float ExpectedValue = ExpectedResults[ExpectedIndex];

			float AbsDiff = FMath::Abs(ExpectedValue - Results[ResultIndex]);
			if (AbsDiff > AbsError)
			{
				FString Error = FString::Printf(TEXT("(%d, %d) Expected %.2f but got %.2f"), X, Y, ExpectedValue, Results[ResultIndex]);
				Errors.Add(Error);
				if (Errors.Num() == MaxErrors)
				{
					FString Terminator = FString(TEXT("... skipping additional Height Field Checks..."));
					Errors.Add(Terminator);
					return Errors;
				}
			}
		}
	}
	return Errors;
}

TArray<float> FHoudiniEditorTestLandscapes::GetLandscapeHeightValues(ALandscape * LandscapeActor)
{
	FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
	FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);

	TArray<float> HoudiniValues;
	TArray<uint16> Values;
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		auto EditLayer = LandscapeActor->GetLayerConst(0);
#else
		auto EditLayer = LandscapeActor->GetLayer(0);
#endif
		int NumPoints = LandscapeVertSize.X * LandscapeVertSize.Y;
		Values.SetNum(NumPoints);

		FScopedSetLandscapeEditingLayer Scope(LandscapeActor, EditLayer->Guid, [&] {});

		FLandscapeEditDataInterface LandscapeEdit(LandscapeActor->GetLandscapeInfo());
		LandscapeEdit.SetShouldDirtyPackage(false);
		LandscapeEdit.GetHeightDataFast(0, 0, LandscapeVertSize.X - 1, LandscapeVertSize.Y - 1, Values.GetData(), 0);
	}

	FTransform LandscapeTransform = LandscapeActor->GetActorTransform();

	HoudiniValues.SetNum(Values.Num());

	float ZScale = LandscapeTransform.GetScale3D().Z / 100.0f;
	for (int Index = 0; Index < Values.Num(); Index++)
	{
		// https://docs.unrealengine.com/4.27/en-US/BuildingWorlds/Landscape/TechnicalGuide/
		float HoudiniValue = ZScale * (static_cast<float>(Values[Index]) - 32768.0f) / 128.0f;
		HoudiniValues[Index] = HoudiniValue;
	}

	return HoudiniValues;
}

TArray<float> FHoudiniEditorTestLandscapes::CreateExpectedHeightValues(const FIntPoint & ExpectedSize, float HeightScale)
{
	TArray<float> ExpectedResults;
	ExpectedResults.SetNum(ExpectedSize.X * ExpectedSize.Y);
	for (int Y = 0; Y < ExpectedSize.Y; Y++)
	{
		for (int X = 0; X < ExpectedSize.X; X++)
		{
			int Index = X + Y * ExpectedSize.X;

			// This line should mimic what the height field wrangle node is doing.
			ExpectedResults[Index] = HeightScale * (Y/* * 2 + Y*/);
		}
	}
	return ExpectedResults;
}

float FHoudiniEditorTestLandscapes::GetMin(const TArray<float>& Values)
{
	float MinValue = TNumericLimits<float>::Max();
	for( float Value : Values)
		MinValue = FMath::Min(MinValue, Value);
	return MinValue;
}

float FHoudiniEditorTestLandscapes::GetMax(const TArray<float>& Values)
{
	float MaxValue = TNumericLimits<float>::Min();
	for (float Value : Values)
		MaxValue = FMath::Max(MaxValue, Value);

	return MaxValue;
}

TArray<float> FHoudiniEditorTestLandscapes::Resize(TArray<float>& In, const FIntPoint& OriginalSize, const FIntPoint& NewSize)
{
	TArray<float> Result;
	Result.SetNum(NewSize.X * NewSize.Y);

	float XScale = float(OriginalSize.X - 1) / float(NewSize.X - 1);
	float YScale = float(OriginalSize.Y - 1) / float(NewSize.Y - 1);

	for (int32 Y = 0; Y < NewSize.Y; ++Y)
	{
		for (int32 X = 0; X < NewSize.X; ++X)
		{
			float OldX = X * XScale;
			float OldY = Y * YScale;
			int32 X0 = FMath::FloorToInt(OldX);
			int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, OriginalSize.X - 1);
			int32 Y0 = FMath::FloorToInt(OldY);
			int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, OriginalSize.Y - 1);
			float Original00 = In[X0 * OriginalSize.Y + Y0];
			float Original10 = In[X1 * OriginalSize.Y + Y0];
			float Original01 = In[X0 * OriginalSize.Y + Y1];
			float Original11 = In[X1 * OriginalSize.Y + Y1];
			float NewValue = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
			Result[X * NewSize.Y + Y] = NewValue;

		}
	}
	return Result;
}


TArray<float>
FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(const FIntPoint& ExpectedSize)
{
	TArray<float> Results;
	Results.SetNum(ExpectedSize.X * ExpectedSize.Y);

	FIntPoint Mid(ExpectedSize.X / 2, ExpectedSize.Y / 2);


	for(int Y = 0; Y > ExpectedSize.Y; Y++)
	{
		for (int X = 0; X > ExpectedSize.X; X++)
		{
			int Index = X + Y * ExpectedSize.X;
			if (X < Mid.X && Y < Mid.Y)
				Results[Index] = 1.0f;
			else
				Results[Index] = 0.0f;
		}

	}
	return Results;
}

TArray<float>
FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer2Values(const FIntPoint& ExpectedSize)
{
	TArray<float> Results;
	Results.SetNum(ExpectedSize.X * ExpectedSize.Y);

	FIntPoint Mid(ExpectedSize.X / 2, ExpectedSize.Y / 2);

	for (int Y = 0; Y > ExpectedSize.Y; Y++)
	{
		for (int X = 0; X > ExpectedSize.X; X++)
		{
			int Index = X + Y * ExpectedSize.X;
			if (X >= Mid.X && Y >= Mid.Y)
				Results[Index] = 1.0f;
			else
				Results[Index] = 0.0f;
		}

	}
	return Results;
}

ULandscapeLayerInfoObject* 
FHoudiniEditorTestLandscapes::GetLayerInfo(ALandscape* LandscapeActor, const FString& LayerName)
{
	ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();
	int TargetLayerIndex = LandscapeInfo->GetLayerInfoIndex(FName(LayerName), LandscapeActor);
	if (TargetLayerIndex == -1)
		return nullptr;

	FLandscapeInfoLayerSettings LayersSetting = LandscapeInfo->Layers[TargetLayerIndex];
	ULandscapeLayerInfoObject* LayerInfo = LayersSetting.LayerInfoObj;
	return LayerInfo;
}

TArray<float>
FHoudiniEditorTestLandscapes::GetLandscapePaintLayerValues(ALandscape* LandscapeActor, const FString& LayerName)
{
	FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
	FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
	TArray<float> HoudiniValues;

	ULandscapeInfo * LandscapeInfo = LandscapeActor->GetLandscapeInfo();
	ULandscapeLayerInfoObject* LayerInfo = GetLayerInfo(LandscapeActor, LayerName);

	// Calc the X/Y size in points

	// extracting the uint8 values from the layer
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.SetShouldDirtyPackage(false); // Ensure we're not triggering a checkout, as we're just reading data

	TArray<uint8> Values;
	Values.AddZeroed(LandscapeVertSize.X * LandscapeVertSize.Y);
	LandscapeEdit.GetWeightDataFast(LayerInfo, 0, 0, LandscapeVertSize.X, LandscapeVertSize.Y, Values.GetData(), 0);

	// Convert to floats
	HoudiniValues.SetNum(Values.Num());
	for(int Index = 0; Index < Values.Num(); Index++)
		HoudiniValues[Index] = Values[Index] / 255.0f;

	return HoudiniValues;
	
}

TArray<float> FHoudiniEditorTestLandscapes::GetLandscapeEditLayerValues(ALandscape* LandscapeActor, const FString& EditLayer, const FString& TargetLayer, const FIntPoint& Size)
{
	FHoudiniExtents Extents;

	Extents.Min = FIntPoint(0, 0);
	Extents.Max = Size;

	TArray<uint8> Values = FHoudiniLandscapeUtils::GetLayerData(LandscapeActor, Extents, FName(*EditLayer), FName(*TargetLayer));

	// Convert to floats
	TArray<float> HoudiniValues;
	HoudiniValues.SetNum(Values.Num());
	for(int Index = 0; Index < Values.Num(); Index++)
		HoudiniValues[Index] = Values[Index] / 255.0f;

	return HoudiniValues;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_Simple, "Houdini.UnitTests.Landscapes.SimpleLandscape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_Simple::RunTest(const FString & Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Landscape/Test_Landscapes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	constexpr int LandscapeSize = 64;
	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeSize]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 1);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "grid_size", 1, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterFloat, "height_scale", 1.0f, 0);
			Context->StartCookingHDA();
			return true;
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeSize]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one output.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

			// Fetch the output as a landscape..
			TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
			HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
			ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

			const FIntPoint ExpectedGridSize = FIntPoint(LandscapeSize, LandscapeSize);

			// Check the size of the landscape is correct.
			FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
			FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
			HOUDINI_TEST_EQUAL(LandscapeVertSize, ExpectedGridSize);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedHeightValues(ExpectedGridSize, 1.0f);
			TArray<float> HoudiniValues = FHoudiniEditorTestLandscapes::GetLandscapeHeightValues(LandscapeActor);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(HoudiniValues, ExpectedResults, ExpectedGridSize);
			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, for(auto & Error : Errors) this->AddError(Error); );

			FBox Bounds = LandscapeActor->GetLoadedBounds();

			float MinValue = FHoudiniEditorTestLandscapes::GetMin(ExpectedResults);
			float MaxValue = FHoudiniEditorTestLandscapes::GetMax(ExpectedResults);
			
			FVector3d ExpectedSize((ExpectedGridSize.X - 1) * 100.0f, (ExpectedGridSize.Y - 1) * 100.0f, (MaxValue - MinValue) * 100.0f);
			FVector3d ActualSize = Bounds.GetSize();
			HOUDINI_TEST_EQUAL(ActualSize.X, ExpectedSize.X);
			HOUDINI_TEST_EQUAL(ActualSize.Y, ExpectedSize.Y);
			HOUDINI_TEST_EQUAL(ActualSize.Z, ExpectedSize.Z);

			float ZCenter = 100.0f * (MaxValue - MinValue) * 0.5f;

			FVector3d ExpectedCenter = FVector3d(0.0f, 0.0f, ZCenter);
			FVector3d ActualCenter = Bounds.GetCenter();

			float Tolerance = ZCenter * 0.001f;
			HOUDINI_TEST_EQUAL(ActualCenter, ExpectedCenter, Tolerance);

			return true;
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_GridSize, "Houdini.UnitTests.Landscapes.ResizedLandscape", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_GridSize::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test resizing of landscapes when the original Houdini height field does not fit in an Unreal landscape.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Landscape/Test_Landscapes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	const FIntPoint HeightFieldSize(143,63); 
	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, HeightFieldSize]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", HeightFieldSize.X, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", HeightFieldSize.Y, 1);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "grid_size", 1, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterFloat, "height_scale", 1.0f, 0);
			Context->StartCookingHDA();
			return true;
		}));


		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, HeightFieldSize]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one output.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

			// Fetch the output as a landscape..
			TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
			HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
			ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

			const FIntPoint ExpectedGridSize = FIntPoint(148, 64);

			// Check the size of the landscape is correct.
			FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
			FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
			HOUDINI_TEST_EQUAL_ON_FAIL(LandscapeVertSize, ExpectedGridSize, return true);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedHeightValues(HeightFieldSize, 1.0f);
			ExpectedResults = FHoudiniEditorTestLandscapes::Resize(ExpectedResults, HeightFieldSize, ExpectedGridSize);

			TArray<float> HoudiniValues = FHoudiniEditorTestLandscapes::GetLandscapeHeightValues(LandscapeActor);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(HoudiniValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for (auto& Error : Errors) 
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );

			FBox Bounds = LandscapeActor->GetLoadedBounds();

			float MinValue = FHoudiniEditorTestLandscapes::GetMin(ExpectedResults);
			float MaxValue = FHoudiniEditorTestLandscapes::GetMax(ExpectedResults);

			FVector3d ExpectedSize((HeightFieldSize.X - 1) * 100.0f, (HeightFieldSize.Y - 1) * 100.0f, (MaxValue - MinValue) * 100.0f);
			FVector3d ActualSize = Bounds.GetSize();
			HOUDINI_TEST_EQUAL(ActualSize.X, ExpectedSize.X);
			HOUDINI_TEST_EQUAL(ActualSize.Y, ExpectedSize.Y);
			HOUDINI_TEST_EQUAL(ActualSize.Z, ExpectedSize.Z);

			float ZCenter = 100.0f * (MaxValue - MinValue) * 0.5f;

			FVector3d ExpectedCenter = FVector3d(0.0f, 0.0f, ZCenter);
			FVector3d ActualCenter = Bounds.GetCenter();

			float Tolerance = ZCenter * 0.001f;
			HOUDINI_TEST_EQUAL(ActualCenter, ExpectedCenter, Tolerance);


			return true;
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_TargetLayers, "Houdini.UnitTests.Landscapes.TargetLayers", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_TargetLayers::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Landscape/Test_Landscapes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	const FIntPoint HeightFieldSize(63, 63);
	
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, HeightFieldSize]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", HeightFieldSize.X, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", HeightFieldSize.Y, 1);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "grid_size", 1, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterFloat, "height_scale", 1.0f, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "paint_layer_1", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "paint_layer_2", true, 0);
		// TODO: Added test for visibility layer too
		Context->StartCookingHDA();
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, HeightFieldSize]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have 1 Output with 3 objects.
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
		HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 3);
		ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

		const FIntPoint ExpectedGridSize = FIntPoint(63, 63);

		// Check the size of the landscape is correct.
		FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
		FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
		HOUDINI_TEST_EQUAL_ON_FAIL(LandscapeVertSize, ExpectedGridSize, return true);

		// Check paint layer 1.
		{
			FString LayerName = TEXT("paint_layer1");

			ULandscapeLayerInfoObject* LayerInfo = FHoudiniEditorTestLandscapes::GetLayerInfo(LandscapeActor, LayerName);

			FString TempFolder = Context->HAC->GetTemporaryCookFolderOrDefault();
			FString ObjectPath = LayerInfo->GetPathName();
			HOUDINI_TEST_EQUAL(FHoudiniEditorUnitTestUtils::IsTemporary(Context->HAC, ObjectPath), true);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(HeightFieldSize);
			TArray<float> GeneratedValues = FHoudiniEditorTestLandscapes::GetLandscapePaintLayerValues(LandscapeActor, LayerName);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(GeneratedValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for (auto& Error : Errors)
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );
		}

		// Check paint layer 2.
		{
			FString LayerName = TEXT("paint_layer2");

			ULandscapeLayerInfoObject* LayerInfo = FHoudiniEditorTestLandscapes::GetLayerInfo(LandscapeActor, LayerName);

			FString TempFolder = Context->HAC->GetTemporaryCookFolderOrDefault();
			FString ObjectPath = LayerInfo->GetPathName();
			HOUDINI_TEST_EQUAL(FHoudiniEditorUnitTestUtils::IsTemporary(Context->HAC, ObjectPath), true);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(HeightFieldSize);
			TArray<float> GeneratedValues = FHoudiniEditorTestLandscapes::GetLandscapePaintLayerValues(LandscapeActor, LayerName);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(GeneratedValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for (auto& Error : Errors)
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );
		}

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_EditLayers, "Houdini.UnitTests.Landscapes.EditLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_EditLayers::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Landscape/Test_Landscapes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	const FIntPoint HeightFieldSize(63, 63);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, HeightFieldSize]()
	{
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", HeightFieldSize.X, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", HeightFieldSize.Y, 1);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "grid_size", 1, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterFloat, "height_scale", 1.0f, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "paint_layer_1", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "paint_layer_2", true, 0);
		SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterToggle, "height_edit_layer", true, 0);
		// TODO: Added test for visibility layer too
		Context->StartCookingHDA();
		return true;
	}));


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, HeightFieldSize]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		// We should have 1 Output with 3 objects.
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);
		TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
		HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 3);
		ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

		const FIntPoint ExpectedGridSize = FIntPoint(63, 63);

		// Check the size of the landscape is correct.
		FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
		FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
		HOUDINI_TEST_EQUAL_ON_FAIL(LandscapeVertSize, ExpectedGridSize, return true);

		// Check paint layer 1.
		{
			FString LayerName = TEXT("paint_layer1");

			ULandscapeLayerInfoObject* LayerInfo = FHoudiniEditorTestLandscapes::GetLayerInfo(LandscapeActor, LayerName);

			FString TempFolder = Context->HAC->GetTemporaryCookFolderOrDefault();
			FString ObjectPath = LayerInfo->GetPathName();
			HOUDINI_TEST_EQUAL(FHoudiniEditorUnitTestUtils::IsTemporary(Context->HAC, ObjectPath), true);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(HeightFieldSize);
			TArray<float> GeneratedValues = FHoudiniEditorTestLandscapes::GetLandscapePaintLayerValues(LandscapeActor, LayerName);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(GeneratedValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for(auto& Error : Errors)
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );
		}

		// Check paint layer 2.
		{
			FString LayerName = TEXT("paint_layer2");

			ULandscapeLayerInfoObject* LayerInfo = FHoudiniEditorTestLandscapes::GetLayerInfo(LandscapeActor, LayerName);

			FString TempFolder = Context->HAC->GetTemporaryCookFolderOrDefault();
			FString ObjectPath = LayerInfo->GetPathName();
			HOUDINI_TEST_EQUAL(FHoudiniEditorUnitTestUtils::IsTemporary(Context->HAC, ObjectPath), true);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(HeightFieldSize);
			TArray<float> GeneratedValues = FHoudiniEditorTestLandscapes::GetLandscapePaintLayerValues(LandscapeActor, LayerName);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(GeneratedValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for(auto& Error : Errors)
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );
		}

		// check Edit Layer
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			HOUDINI_TEST_EQUAL(LandscapeActor->GetLayerCount(), 1);
			HOUDINI_TEST_EQUAL(LandscapeActor->GetLayerConst(0)->Name.ToString(), FString(TEXT("Edit Layer")));
#else
			HOUDINI_TEST_EQUAL(LandscapeActor->LandscapeLayers.Num(), 1);
			HOUDINI_TEST_EQUAL(LandscapeActor->LandscapeLayers[0].Name.ToString(), FString(TEXT("Edit Layer")));
#endif
		}
		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_ModifyExisting, "Houdini.UnitTests.Landscapes.ModifyExisting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_ModifyExisting::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	FString MapName = TEXT("/Game/TestObjects/Landscapes/Test_ModifyLandscape"); 
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, MapName));

	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	// Find Landscape Actor
	ALandscape* LandscapeActor = nullptr;
	for(TActorIterator<AActor> ActorItr(Context->World, ALandscape::StaticClass()); ActorItr; ++ActorItr)
	{
		AActor* FoundActor = *ActorItr;
		if(FoundActor)
		{
			LandscapeActor = Cast<ALandscape>(FoundActor);
			break;
		}
	}


	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	})); 

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeActor]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->HAC->GetOutputs(Outputs);

		const FIntPoint ExpectedGridSize = { 190, 190 };


		// Check the size of the landscape is correct.
		FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
		FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
		HOUDINI_TEST_EQUAL_ON_FAIL(LandscapeVertSize, ExpectedGridSize, return true);

		// Check paint layer 1.
		{
			FString EditLayer = TEXT("Edit Layer 2");
			FString LayerName = TEXT("paint_layer2");

			ULandscapeLayerInfoObject* LayerInfo = FHoudiniEditorTestLandscapes::GetLayerInfo(LandscapeActor, LayerName);

			FString TempFolder = Context->HAC->GetTemporaryCookFolderOrDefault();
			FString ObjectPath = LayerInfo->GetPathName();
			HOUDINI_TEST_EQUAL(FHoudiniEditorUnitTestUtils::IsTemporary(Context->HAC, ObjectPath), false);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(ExpectedGridSize);
			TArray<float> GeneratedValues = FHoudiniEditorTestLandscapes::GetLandscapeEditLayerValues(LandscapeActor, EditLayer, LayerName, ExpectedGridSize);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(GeneratedValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for(auto& Error : Errors)
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );
		}

		// Check paint layer 2.
		{
			FString EditLayer = TEXT("Edit Layer 1");
			FString LayerName = TEXT("paint_layer1");

			ULandscapeLayerInfoObject* LayerInfo = FHoudiniEditorTestLandscapes::GetLayerInfo(LandscapeActor, LayerName);

			FString TempFolder = Context->HAC->GetTemporaryCookFolderOrDefault();
			FString ObjectPath = LayerInfo->GetPathName();
			HOUDINI_TEST_EQUAL(FHoudiniEditorUnitTestUtils::IsTemporary(Context->HAC, ObjectPath), false);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedPaintLayer1Values(ExpectedGridSize);
			TArray<float> GeneratedValues = FHoudiniEditorTestLandscapes::GetLandscapeEditLayerValues(LandscapeActor, EditLayer, LayerName, ExpectedGridSize);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckLandscapeValues(GeneratedValues, ExpectedResults, ExpectedGridSize, 1.0f);
			for(auto& Error : Errors)
				this->AddError(Error);

			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, return true; );
		}

		// check Edit Layer
		{


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			HOUDINI_TEST_EQUAL(LandscapeActor->GetLayerCount(), 3);
			HOUDINI_TEST_EQUAL(LandscapeActor->GetLayerConst(0)->Name.ToString(), FString(TEXT("Layer")));
			HOUDINI_TEST_EQUAL(LandscapeActor->GetLayerConst(1)->Name.ToString(), FString(TEXT("Edit Layer 1")));
			HOUDINI_TEST_EQUAL(LandscapeActor->GetLayerConst(2)->Name.ToString(), FString(TEXT("Edit Layer 2")));
#else
			HOUDINI_TEST_EQUAL(LandscapeActor->LandscapeLayers.Num(), 3);
			HOUDINI_TEST_EQUAL(LandscapeActor->LandscapeLayers[0].Name.ToString(), FString(TEXT("Layer")));
			HOUDINI_TEST_EQUAL(LandscapeActor->LandscapeLayers[1].Name.ToString(), FString(TEXT("Edit Layer 1")));
			HOUDINI_TEST_EQUAL(LandscapeActor->LandscapeLayers[2].Name.ToString(), FString(TEXT("Edit Layer 2")));
#endif


		}
		return true;
	}));

	return true;
}
#endif

