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

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/*
 *TBD;
 *FHoudiniEditorTestUtils: replace call of utility functions like FHoudiniEditorTestUtils::InstantiateAsset by their public API equivalent.
 **/

// General
class UHoudiniAssetComponent;
class UHoudiniAsset;
class UHoudiniParameter;
class UHoudiniInput;
class UHoudiniOutput;
class UHoudiniHandleComponent;
class UHoudiniPDGAssetLink;
class UHoudiniInputObject;

// Runtime objects:
class AHoudiniAssetActor;
class UHoudiniInstancedActorComponent;
class UHoudiniMeshSplitInstancerComponent;
class UHoudiniParameterButton;
class UHoudiniParameterButtonStrip;
class UHoudiniParameterChoice;
class UHoudiniParameterColor;
class UHoudiniParameterFile;
class UHoudiniParameterFloat;
class UHoudiniParameterFolder;
class UHoudiniParameterFolderList;
class UHoudiniParameterInt;
class UHoudiniParameterLabel;
class UHoudiniParameterMultiParm;
class UHoudiniParameterOperatorPath;
class UHoudiniParameterRampFloatPoint;
class UHoudiniParameterRampColorPoint;
class UHoudiniParameterRampFloat;
class UHoudiniParameterRampColor;
class UHoudiniParameterSeparator;
class UHoudiniParameterString;
class UHoudiniParameterToggle;
class UHoudiniSplineComponent;
class UHoudiniStaticMesh;
class UHoudiniStaticMeshComponent;
class UHoudiniLandscapePtr;
class UHoudiniLandscapeEditLayer;
class UHoudiniLandscapeEditLayer;
class ALandscapeProxy;

// structs
struct FHoudiniBakedOutput;
struct FHoudiniStaticMeshGenerationProperties;
struct FHoudiniGeoPartObject;
struct FHoudiniInstancedOutput;
struct FHoudiniBakedOutputObject;
struct FHoudiniObjectInfo;
struct FHoudiniGeoInfo;
struct FHoudiniPartInfo;
struct FHoudiniVolumeInfo;
struct FHoudiniCurveInfo;
struct FHoudiniMeshSocket;
struct FHoudiniOutputObject;
struct FHoudiniCurveOutputProperties;

enum class EHoudiniOutputType : uint8;

class FHoudiniEditorEquivalenceUtils
{
public:
	static bool IsEquivalent(UHoudiniAssetComponent* A, UHoudiniAssetComponent* B);
	static bool IsEquivalent(UHoudiniAsset* A, UHoudiniAsset* B);
    static bool IsEquivalent(UHoudiniParameter* A, UHoudiniParameter* B);
	static bool IsEquivalent(UHoudiniInput* A, UHoudiniInput* B);
	static bool IsEquivalent(UHoudiniInputObject* A, UHoudiniInputObject* B);
	static bool IsEquivalent(UHoudiniOutput* A, UHoudiniOutput* B);
	static bool IsEquivalent(UHoudiniHandleComponent* A, UHoudiniHandleComponent* B);
	static bool IsEquivalent(UHoudiniPDGAssetLink* A, UHoudiniPDGAssetLink* B);

	
    // Struct equivalence
	static bool IsEquivalent(FHoudiniStaticMeshGenerationProperties& A, FHoudiniStaticMeshGenerationProperties& B);
	static bool IsEquivalent(FHoudiniBakedOutput& A, FHoudiniBakedOutput& B);
	static bool IsEquivalent(FHoudiniGeoPartObject& A, FHoudiniGeoPartObject& B);
	static bool IsEquivalent(FHoudiniInstancedOutput& A, FHoudiniInstancedOutput& B);
	static bool IsEquivalent(FHoudiniBakedOutputObject& A, FHoudiniBakedOutputObject& B);
	static bool IsEquivalent(FHoudiniObjectInfo& A, FHoudiniObjectInfo& B);
	static bool IsEquivalent(FHoudiniGeoInfo& A, FHoudiniGeoInfo& B);
	static bool IsEquivalent(FHoudiniPartInfo& A, FHoudiniPartInfo& B);
	static bool IsEquivalent(FHoudiniVolumeInfo& A, FHoudiniVolumeInfo& B);
	static bool IsEquivalent(FHoudiniCurveInfo& A, FHoudiniCurveInfo& B);
	static bool IsEquivalent(FHoudiniMeshSocket& A, FHoudiniMeshSocket& B);
	static bool IsEquivalent(FHoudiniOutputObject& A, FHoudiniOutputObject& B);
	static bool IsEquivalent(FHoudiniCurveOutputProperties& A, FHoudiniCurveOutputProperties& B);
	
	// Unreal equivalence
	static bool IsEquivalent(FDirectoryPath& A, FDirectoryPath& B);
	static bool IsEquivalent(FMeshBuildSettings& A, FMeshBuildSettings& B);
	static bool IsEquivalent(FTransform& A, FTransform& B);
	static bool IsEquivalent(FBodyInstance& A, FBodyInstance& B);
	static bool IsEquivalent(FWalkableSlopeOverride& A, FWalkableSlopeOverride& B);
	static bool IsEquivalent(FColor& A, FColor& B);
	static bool IsEquivalent(FLinearColor& A, FLinearColor& B);
	static bool IsEquivalent(FVector& A, FVector& B);
	static bool IsEquivalent(FIntVector& A, FIntVector& B);
	static bool IsEquivalent(FVector2D& A, FVector2D& B);
	static bool IsEquivalent(FStaticMaterial& A, FStaticMaterial& B);
	static bool IsEquivalent(FBox& A, FBox& B);

	// Runtime UObjects:
	static bool IsEquivalent(UObject * A, UObject * B);
	static bool IsEquivalent(UPhysicalMaterial * A, UPhysicalMaterial * B);
	static bool IsEquivalent(AActor * A, AActor * B);
	static bool IsEquivalent(ALandscapeProxy * A, ALandscapeProxy * B);

	static bool IsEquivalent(AHoudiniAssetActor* A, AHoudiniAssetActor* B);
	static bool IsEquivalent(UHoudiniInstancedActorComponent* A, UHoudiniInstancedActorComponent* B);
	static bool IsEquivalent(UHoudiniMeshSplitInstancerComponent* A, UHoudiniMeshSplitInstancerComponent* B);
	static bool IsEquivalent(UHoudiniParameterButton* A, UHoudiniParameterButton* B);
	static bool IsEquivalent(UHoudiniParameterButtonStrip* A, UHoudiniParameterButtonStrip* B);
	static bool IsEquivalent(UHoudiniParameterChoice* A, UHoudiniParameterChoice* B);
	static bool IsEquivalent(UHoudiniParameterColor* A, UHoudiniParameterColor* B);
	static bool IsEquivalent(UHoudiniParameterFile* A, UHoudiniParameterFile* B);
	static bool IsEquivalent(UHoudiniParameterFloat* A, UHoudiniParameterFloat* B);
	static bool IsEquivalent(UHoudiniParameterFolder* A, UHoudiniParameterFolder* B);
	static bool IsEquivalent(UHoudiniParameterFolderList* A, UHoudiniParameterFolderList* B);
	static bool IsEquivalent(UHoudiniParameterInt* A, UHoudiniParameterInt* B);
	static bool IsEquivalent(UHoudiniParameterLabel* A, UHoudiniParameterLabel* B);
	static bool IsEquivalent(UHoudiniParameterMultiParm* A, UHoudiniParameterMultiParm* B);
	static bool IsEquivalent(UHoudiniParameterOperatorPath* A, UHoudiniParameterOperatorPath* B);
	static bool IsEquivalent(UHoudiniParameterRampFloatPoint* A, UHoudiniParameterRampFloatPoint* B);
	static bool IsEquivalent(UHoudiniParameterRampColorPoint* A, UHoudiniParameterRampColorPoint* B);
	static bool IsEquivalent(UHoudiniParameterRampFloat* A, UHoudiniParameterRampFloat* B);
	static bool IsEquivalent(UHoudiniParameterRampColor* A, UHoudiniParameterRampColor* B);
	static bool IsEquivalent(UHoudiniParameterSeparator* A, UHoudiniParameterSeparator* B);
	static bool IsEquivalent(UHoudiniParameterString* A, UHoudiniParameterString* B);
	static bool IsEquivalent(UHoudiniParameterToggle* A, UHoudiniParameterToggle* B);
	static bool IsEquivalent(UHoudiniSplineComponent* A, UHoudiniSplineComponent* B);
	static bool IsEquivalent(UHoudiniStaticMesh* A, UHoudiniStaticMesh* B);
	static bool IsEquivalent(UHoudiniStaticMeshComponent* A, UHoudiniStaticMeshComponent* B);
	static bool IsEquivalent(UHoudiniLandscapePtr* A, UHoudiniLandscapePtr* B);
	static bool IsEquivalent(UHoudiniLandscapeEditLayer* A, UHoudiniLandscapeEditLayer* B);
	static bool IsEquivalent(UStaticMeshComponent* A, UStaticMeshComponent* B);
	static bool IsEquivalent(UMaterialInterface* A, UMaterialInterface* B);
	static bool IsEquivalent(UStaticMesh* A, UStaticMesh* B);


private:
#if WITH_DEV_AUTOMATION_TESTS
	static void SetAutomationBase(FAutomationTestBase* Test);
	static FAutomationTestBase* TestBase;
#endif

	static bool TestExpressionErrorEnabled;

	static bool TestExpressionError(const bool Expression, const FString& Header, const FString& Subject);

	static void SetTestExpressionError(bool Enabled);
};
