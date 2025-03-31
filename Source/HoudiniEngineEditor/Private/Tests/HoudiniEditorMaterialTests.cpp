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

#include "HoudiniEditorMaterialTests.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HoudiniCookable.h"
#include "HoudiniEditorTestUtils.h"
#include "HoudiniEditorUnitTestUtils.h"

#include "Engine/EngineTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Misc/AutomationTest.h"
#include "TextureResource.h"

FString FHoudiniEditorMaterialTests::EquivalenceTestMapName = TEXT("Materials");
FString FHoudiniEditorMaterialTests::TestHDAPath = TEXT("/Game/TestHDAs/Materials/");

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMaterialTest_Material_Simple, "Houdini.Editor.Materials.Material_Simple", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMaterialTest_Material_Simple::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [this]
	{
		const FString MapName = FHoudiniEditorMaterialTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Material_Simple");
		const FString HDAAssetPath = FHoudiniEditorMaterialTests::TestHDAPath + TEXT("Material_Simple");
		const FHoudiniActorTestSettings Settings = {};
		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, Settings, nullptr);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMaterialTest_Material_Maps, "Houdini.Editor.Materials.Material_Maps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMaterialTest_Material_Maps::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [this]
	{
		const FString MapName = FHoudiniEditorMaterialTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("Material_Maps");
		const FString HDAAssetPath = FHoudiniEditorMaterialTests::TestHDAPath + TEXT("Material_Maps");
		const FHoudiniActorTestSettings Settings = {};
		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, Settings, nullptr);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMaterialTest_MaterialAttributes_Common, "Houdini.Editor.Materials.MaterialAttributes_Common", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMaterialTest_MaterialAttributes_Common::RunTest(const FString & Parameters)
{
	FHoudiniEditorTestUtils::InitializeTests(this, [this]
	{
		const FString MapName = FHoudiniEditorMaterialTests::EquivalenceTestMapName;
		const FString ActorName = TEXT("MaterialAttributes_Common");
		const FString HDAAssetPath = FHoudiniEditorMaterialTests::TestHDAPath + TEXT("MaterialAttributes_Common");
		const FHoudiniActorTestSettings Settings = {};

		FHoudiniEditorTestUtils::RunOrSetupDifferentialTest(this, MapName, HDAAssetPath, ActorName, Settings, nullptr);
	});

	return true;
}

IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(HoudiniEditorMaterialTest_Material_Textures, "Houdini.Editor.Materials.Material_Textures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorMaterialTest_Material_Textures::RunTest(const FString& Parameters)
{
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, FHoudiniEditorMaterialTests::TestHDAPath + TEXT("Material_Textures"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->SetProxyMeshEnabled(false);

	// Start cooking the HDA.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		Context->StartCookingHDA();
		return true;
	}));

	// Check that the material has been created correctly.
	AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
	{
		// Lambda which opens a texture expression's raw texture data and checks the values of some pixels.
		auto CheckPixelTestCases = [this](UMaterialExpression* Expression, TArray<FHoudiniEditorMaterialTests::FTexturePixelTestCase> PixelTestCases)
		{
			// Grab the texture from the texture expression.
			UMaterialExpressionTextureSampleParameter2D* TextureExpression = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(TextureExpression, return true);
			UTexture2D* Texture = Cast<UTexture2D>(TextureExpression->Texture);
			HOUDINI_TEST_NOT_NULL_ON_FAIL(Texture, return true);

			// The texture needs these settings, otherwise RawData->Lock() will fail.
			Texture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
			Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
			Texture->SRGB = false;
			Texture->UpdateResource();

			// Get the texture's first mipmap (i.e. the full resolution texture).
			HOUDINI_TEST_NOT_EQUAL_ON_FAIL(Texture->GetNumMips(), 0, return true);
			FTexture2DMipMap* MipMap = &Texture->GetPlatformData()->Mips[0];
			HOUDINI_TEST_NOT_NULL_ON_FAIL(MipMap, return true);

			// Get a pointer to the mipmap's raw data.
			FByteBulkData* RawData = &MipMap->BulkData;
			HOUDINI_TEST_NOT_NULL_ON_FAIL(RawData, return true);
			HOUDINI_TEST_EQUAL_ON_FAIL(RawData->IsUnlocked(), true, return true);
			FColor* FormattedImageData = static_cast<FColor*>( RawData->Lock( LOCK_READ_ONLY ) );

			if (!FormattedImageData)
			{
				// Unlocks the raw data if the image data wasn't casted successfully.
				if (RawData->IsLocked())
					RawData->Unlock();
				// FormattedImageData is nullptr here; this macro fails on purpose.
				HOUDINI_TEST_NOT_NULL_ON_FAIL(FormattedImageData, return true);
			}

			// Check that the test cases pass.
			bool AllCorrect = true;
			for (auto& PixelTestCase : PixelTestCases)
			{
				int32 Index = PixelTestCase.y * MipMap->SizeX + PixelTestCase.x;
				HOUDINI_TEST_EQUAL_ON_FAIL(FormattedImageData[Index].R, PixelTestCase.r, AllCorrect = false);
				HOUDINI_TEST_EQUAL_ON_FAIL(FormattedImageData[Index].G, PixelTestCase.g, AllCorrect = false);
				HOUDINI_TEST_EQUAL_ON_FAIL(FormattedImageData[Index].B, PixelTestCase.b, AllCorrect = false);
				HOUDINI_TEST_EQUAL_ON_FAIL(FormattedImageData[Index].A, PixelTestCase.a, AllCorrect = false);
			}

			RawData->Unlock();

			return AllCorrect;
		};

		// We should have one output.
		TArray<UHoudiniOutput*> Outputs;
		Context->GetOutputs(Outputs);

		HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

		// And the one output should have a static mesh.
		TArray<UStaticMeshComponent*> StaticMeshOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithComponent<UStaticMeshComponent>(Outputs);
		HOUDINI_TEST_EQUAL_ON_FAIL(StaticMeshOutputs.Num(), 1, return true);
		UStaticMeshComponent* Mesh = StaticMeshOutputs[0];

		// And that static meshs hould have one material.
		HOUDINI_TEST_EQUAL_ON_FAIL(Mesh->GetNumMaterials(), 1, return true);
		UMaterial* Material = Cast<UMaterial>(Mesh->GetMaterial(0));
		HOUDINI_TEST_NOT_NULL_ON_FAIL(Material, return true);

		// Grab the relevant expressions attached to the material.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		UMaterialEditorOnlyData* MaterialEditorOnly = Material->GetEditorOnlyData();
		FColorMaterialInput& MatInputDiffuse = MaterialEditorOnly->BaseColor;
		FScalarMaterialInput& MatInputMetallic = MaterialEditorOnly->Metallic;
		FScalarMaterialInput& MatInputSpecular = MaterialEditorOnly->Specular;
		FScalarMaterialInput& MatInputRoughness = MaterialEditorOnly->Roughness;
		FColorMaterialInput& MatInputEmissive = MaterialEditorOnly->EmissiveColor;
		FScalarMaterialInput& MatInputOpacity = MaterialEditorOnly->Opacity;
		FVectorMaterialInput& MatInputNormal = MaterialEditorOnly->Normal;
#else
		FColorMaterialInput& MatInputDiffuse = Material->BaseColor;
		FScalarMaterialInput& MatInputMetallic = Material->Metallic;
		FScalarMaterialInput& MatInputSpecular = Material->Specular;
		FScalarMaterialInput& MatInputRoughness = Material->Roughness;
		FColorMaterialInput& MatInputEmissive = Material->EmissiveColor;
		FScalarMaterialInput& MatInputOpacity = Material->Opacity;
		FVectorMaterialInput& MatInputNormal = Material->Normal;
#endif

		// Check that every expression exists.
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputDiffuse.Expression, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputMetallic.Expression, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputSpecular.Expression, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputRoughness.Expression, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputEmissive.Expression, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputOpacity.Expression, return true);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MatInputNormal.Expression, return true);

		// Find the multiply expression attached to the diffuse texture, and the constant diffuse expression.
		UMaterialExpressionMultiply* FirstMultiplyExpressionDiffuse = Cast<UMaterialExpressionMultiply>(MatInputDiffuse.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(FirstMultiplyExpressionDiffuse, return true);
		UMaterialExpressionMultiply* SecondMultiplyExpressionDiffuse = Cast<UMaterialExpressionMultiply>(FirstMultiplyExpressionDiffuse->A.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(SecondMultiplyExpressionDiffuse, return true);
		UMaterialExpressionVectorParameter* ConstantDiffuse = Cast<UMaterialExpressionVectorParameter>(SecondMultiplyExpressionDiffuse->A.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(ConstantDiffuse, return true);

		// Find the constant roughness expression.
		UMaterialExpressionScalarParameter* ConstantRoughness = Cast<UMaterialExpressionScalarParameter>(MatInputRoughness.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(ConstantRoughness, return true);

		// Find the multiply expression attached to the emissive texture.
		UMaterialExpressionMultiply* FirstMultiplyExpressionEmissive = Cast<UMaterialExpressionMultiply>(MatInputEmissive.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(FirstMultiplyExpressionEmissive, return true);

		// Find the constant opacity expression.
		UMaterialExpressionMultiply* MultiplyExpressionOpacity = Cast<UMaterialExpressionMultiply>(MatInputOpacity.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(MultiplyExpressionOpacity, return true);
		UMaterialExpressionScalarParameter* ConstantOpacity = Cast<UMaterialExpressionScalarParameter>(MultiplyExpressionOpacity->B.Expression);
		HOUDINI_TEST_NOT_NULL_ON_FAIL(ConstantOpacity, return true);

		// Check the constant diffuse value.
		// Note that the values on the HDA are equal to 0.333, not 0.333... repeating.
		const float Tolerance = 0.00001f;
		HOUDINI_TEST_EQUALISH(ConstantDiffuse->DefaultValue.R, 0.333f, Tolerance);
		HOUDINI_TEST_EQUALISH(ConstantDiffuse->DefaultValue.G, 0.333f, Tolerance);
		HOUDINI_TEST_EQUALISH(ConstantDiffuse->DefaultValue.B, 0.333f, Tolerance);
		HOUDINI_TEST_EQUALISH(ConstantDiffuse->DefaultValue.A, 1.0f, Tolerance);

		// Diffuse is the butterfly image pulled using a File COP.
		TArray<FHoudiniEditorMaterialTests::FTexturePixelTestCase> DiffuseTestCases = {
			{0, 0, 0, 0, 0, 0},
			{65, 85, 85, 89, 68, 213},
			{81, 109, 87, 72, 59, 255},
			{388, 378, 216, 92, 92, 255},
			{468, 425, 72, 76, 56, 207}
		};
		HOUDINI_TEST_EQUAL(CheckPixelTestCases(FirstMultiplyExpressionDiffuse->B.Expression, DiffuseTestCases), true);

		// Metallic is the butterfly image pulled directly from file.
		TArray<FHoudiniEditorMaterialTests::FTexturePixelTestCase> MetallicTestCases = {
			{0, 0, 0, 0, 0, 255},
			{65, 85, 85, 89, 66, 255},
			{81, 109, 85, 70, 56, 255},
			{388, 378, 217, 90, 92, 255},
			{468, 425, 70, 74, 53, 255}
		};
		HOUDINI_TEST_EQUAL(CheckPixelTestCases(MatInputMetallic.Expression, MetallicTestCases), true);

		// Specular is the default Worley Noise COP.
		TArray<FHoudiniEditorMaterialTests::FTexturePixelTestCase> SpecularTestCases = {
			{0, 0, 115, 115, 115, 255},
			{912, 194, 28, 28, 28, 255},
			{483, 540, 221, 221, 221, 255},
			{657, 826, 114, 114, 114, 255},
			{1023, 1023, 118, 118, 118, 255}
		};
		HOUDINI_TEST_EQUAL(CheckPixelTestCases(MatInputSpecular.Expression, SpecularTestCases), true);

		// Check the constant roughness value.
		HOUDINI_TEST_EQUALISH(ConstantRoughness->DefaultValue, 0.5f, Tolerance);

		// Emissive is a red to black Ramp COP.
		TArray<FHoudiniEditorMaterialTests::FTexturePixelTestCase> EmissiveTestCases = {
			{0, 0, 255, 0, 0, 255},
			{221, 788, 228, 0, 0, 255},
			{541, 301, 181, 0, 0, 255},
			{956, 744, 74, 0, 0, 255},
			{1023, 1023, 0, 0, 0, 255}
		};
		HOUDINI_TEST_EQUAL(CheckPixelTestCases(FirstMultiplyExpressionEmissive->B.Expression, EmissiveTestCases), true);

		// Check the constant opacity value.
		HOUDINI_TEST_EQUALISH(ConstantOpacity->DefaultValue, 1.0f, Tolerance);

		// Normal is the butterfly image pulled directly from file.
		TArray<FHoudiniEditorMaterialTests::FTexturePixelTestCase> NormalTestCases = {
			{0, 0, 0, 0, 0, 255},
			{65, 85, 85, 89, 66, 255},
			{81, 109, 85, 70, 56, 255},
			{388, 378, 217, 90, 92, 255},
			{468, 425, 70, 74, 53, 255}
		};
		HOUDINI_TEST_EQUAL(CheckPixelTestCases(MatInputNormal.Expression, NormalTestCases), true);

		return true;
	}));

	return true;
}

#endif

