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

#include "Runtime/Launch/Resources/Version.h"

#include "HoudiniEditorTestCOP.h"
#include "HoudiniCookable.h"
//#include "HoudiniParameterToggle.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "TextureResource.h"

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestCOPs_Out, "Houdini.UnitTests.COPs.TextureOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext  | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestCOPs_Out::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestCOP::COPHDA, FTransform::Identity, false));
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

		// We should have 4 texture outputs
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 4, return true);
		TArray<UTexture2D*> OutputObjects = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UTexture2D>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 4, return true);

		// DIFFUSE
		TArray<int> DiffusePixel = {
			0,0,		114,68,39,255,
			128,128,	119,72,43,255,
			666,666,	120,72,43,255,
			999,999,	117,70,43,255
		};
		HOUDINI_TEST_EQUAL(FHoudiniEditorTestCOP::CheckColorPixelsInTexture(OutputObjects[0], DiffusePixel), true);

		// NORMAL
		TArray<int> NormalPixel = {
			1,1,		128,126,255,255,
			42,42,		159,245,164,255,
			512,512,	157,230,197,255,
			666,999,	192,211,199,255
		};
		HOUDINI_TEST_EQUAL(FHoudiniEditorTestCOP::CheckColorPixelsInTexture(OutputObjects[1], NormalPixel), true);

		// DISPLACEMENT
		TArray<int> DisplacementPixel = {
			2,2,		0,0,0,255,
			99,99,		66,66,66,255,
			666,512,	24,24,24,255,
			999,666,	236,236,236,255
		};
		HOUDINI_TEST_EQUAL(FHoudiniEditorTestCOP::CheckColorPixelsInTexture(OutputObjects[2], DisplacementPixel), true);

		// ROUGH
		TArray<int> RoughPixel = {
			3,3,		236, 236, 236, 255,
			88,88,		236, 236, 236, 255,
			555,666,	236, 236, 236, 255,
			888,999,	236, 236, 236, 255
		};
		HOUDINI_TEST_EQUAL(FHoudiniEditorTestCOP::CheckColorPixelsInTexture(OutputObjects[3], RoughPixel), true);

		return true;
	}));

	return true;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestCOPs_Bake, "Houdini.UnitTests.COPs.TextureBake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestCOPs_Bake::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestCOP::COPHDA, FTransform::Identity, false));
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

		// We should have 4 texture outputs
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 4, return true);
		TArray<UTexture2D*> OutputObjects = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UTexture2D>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputObjects.Num(), 4, return true);
		return true;
	}));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Bake the textures
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		FHoudiniBakeSettings BakeSettings;
		
		// Bake to Assets
		UHoudiniCookable* TestHC = Context->GetCookable();
		TestHC->SetHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToAsset);

		Context->Bake(BakeSettings);

		TArray<FHoudiniBakedOutput>& BakedOutputs = Context->GetBakedOutputs();
		
		// There should be 4 baked outputs
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedOutputs.Num(), 4, return true);

		// Go through each output and check we have 4 textures
		TArray<UTexture2D*> BakedTextures;
		for (auto& BakedOutput : BakedOutputs)
		{
			for (auto It : BakedOutput.BakedOutputObjects)
			{
				FHoudiniBakedOutputObject& OutputObject = It.Value;
				UTexture2D* Tex = Cast<UTexture2D>(OutputObject.GetBakedObjectIfValid());
				if (IsValid(Tex))
					BakedTextures.Add(Tex);
			}
		}
		HOUDINI_TEST_EQUAL_ON_FAIL(BakedTextures.Num(), 4, return true);

		return true;
	}));

	return true;
}



IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestCOPs_ImageData, "Houdini.UnitTests.COPs.ImageData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestCOPs_ImageData::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorTestCOP::COPHDA, FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		// Set Image Data properties on the Cookable
		UHoudiniCookable* TestHC = Context->GetCookable();
		UCookableImageData* ImageData = TestHC ? TestHC->GetImageData() : nullptr;

		HOUDINI_TEST_EQUAL_ON_FAIL(ImageData != nullptr, true, return false);

		// Enable generate materials
		ImageData->bGenerateMaterial = true;

		// Change texture resolution
		ImageData->bOverrideDefaultResolution = true;
		ImageData->ResolutionOverride = FIntPoint(512,512);

		// Change Pixel scale
		ImageData->bOverridePixelScale = true;
		ImageData->PixelScale = 8.0f;

		// Change Data Format
		ImageData->ImageDataFormat = EHoudiniEngineImageDataFormat::Float32;

		// re-cook the HDA with updated properties
		Context->StartCookingHDA();
		return true;
	}));

	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		// We should have 5 outputs: 4 textures and a material
		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 5, return true);
		
		TArray<UTexture2D*> OutputTextures = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UTexture2D>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputTextures.Num(), 4, return true);

		// The textures should be 512x512, float 32
		for (UTexture2D* CurTexture : OutputTextures)
		{
			// size X
			HOUDINI_TEST_EQUAL_ON_FAIL(CurTexture->GetSizeX(), 512, return true);
			// size Y
			HOUDINI_TEST_EQUAL_ON_FAIL(CurTexture->GetSizeY(), 512, return true);
			// pixel format
			HOUDINI_TEST_EQUAL_ON_FAIL(CurTexture->GetPixelFormat(), EPixelFormat::PF_A32B32G32R32F, return true);
		}

		/*
		// Due to pixel scale - neighbouring pixels should be identical
		TArray<int> Pixels = {
			1, 1, 47, 15, 5, 255,
			3, 3, 47, 15, 5, 255
		};
		HOUDINI_TEST_EQUAL(FHoudiniEditorTestCOP::CheckColorPixelsInTexture(OutputTextures[0], Pixels), true);
		*/

		TArray<UMaterialInterface*> OutputMaterials = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UMaterialInterface>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(OutputMaterials.Num(), 1, return true);

		UMaterialInterface* Mat = OutputMaterials.Num() > 0 ? OutputMaterials[0] : nullptr;
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Mat, return true);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 6
		TArray<UTexture*> UsedTextures;
		Mat->GetUsedTextures(UsedTextures);

		// Material should at least use 4 textures
		HOUDINI_TEST_EQUAL_ON_FAIL(UsedTextures.Num() >= 4, true, return true);

		for (UTexture2D* CurTexture : OutputTextures)
		{
			// Make sure all our output textures are used by the output material
			UTexture* CurTextureCast = Cast<UTexture>(CurTexture);
			HOUDINI_TEST_EQUAL_ON_FAIL(UsedTextures.Contains(CurTextureCast), true, return true;);
		}
#endif

		return true;
	}));

	return true;
}


bool
FHoudiniEditorTestCOP::CheckColorPixelsInTexture(UTexture2D* InTexture, TArray<int> InExpectedValues)
{
	if (!InTexture)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - invalid texture"));	
		return false;
	}

	if (InTexture->GetNumMips() < 1)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - no mip"));
		return false;
	}

	// The texture needs these settings, otherwise RawData->Lock() will fail.
	InTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	InTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	InTexture->UpdateResource();

	FTexturePlatformData* PlatformData = InTexture->GetPlatformData();
	if (!PlatformData)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - no platform data"));
		return false;
	}

	if (PlatformData->Mips.Num() <= 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - no platform data mips"));
		return false;
	}

	FTexture2DMipMap* MipMap = &InTexture->GetPlatformData()->Mips[0];
	if (!MipMap)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - invalid mip0"));
		return false;
	}

	// Get a pointer to the mipmap's raw pixel data.
	FByteBulkData* RawData = &MipMap->BulkData;
	if (!RawData || RawData->IsLocked())
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - invalid/locked raw data"));
		return false;
	}

	const FColor* ImageColors = static_cast<const FColor*>(RawData->LockReadOnly());
	if (!ImageColors)
	{
		// Unlocks the raw data if the image data wasn't casted successfully.
		if (RawData->IsLocked())
			RawData->Unlock();

		HOUDINI_LOG_ERROR(TEXT("Unable to read texture! - unable to lock raw data"));

		return false;
	}

	if(InExpectedValues.Num() % 6 != 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Invalid expected values"));
		return false;
	}

	bool AllValid = true;
	for (int Idx = 0; Idx < InExpectedValues.Num(); Idx+=6)
	{
		int AtX = InExpectedValues[Idx];
		int AtY = InExpectedValues[Idx+1];
		FColor ExpectedColorValue(InExpectedValues[Idx + 2], InExpectedValues[Idx + 3], InExpectedValues[Idx + 4] , InExpectedValues[Idx + 5]);

		// Access the texture's buffer via PlatformData/Mip0
		int AtIndex = AtX + AtY * InTexture->GetSizeX();
		FColor TextureColorValue = ImageColors[AtIndex];
				
		if (TextureColorValue != ExpectedColorValue)
		{
			AllValid = false;

			// Pixel values do not match the expected color
			HOUDINI_LOG_ERROR(TEXT("At (X=%d, Y=%d) - expected %d,%d,%d,%d and read %d,%d,%d,%d"),
				AtX, AtY,
				ExpectedColorValue.R, ExpectedColorValue.G, ExpectedColorValue.B, ExpectedColorValue.A,
				TextureColorValue.R, TextureColorValue.G, TextureColorValue.B, TextureColorValue.A);
		}		
	}

	// Clean up.
	RawData->Unlock();

	return AllValid;
}
#endif

