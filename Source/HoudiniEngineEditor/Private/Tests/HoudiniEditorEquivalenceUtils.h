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
class UMaterialInterface;

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
struct FBodyInstance;
struct FStaticMaterial;

enum class EHoudiniOutputType : uint8;

class FHoudiniEditorEquivalenceUtils
{
public:
	static bool IsEquivalent(const UHoudiniAssetComponent* A, const UHoudiniAssetComponent* B);
	static bool IsEquivalent(const UHoudiniAsset* A, const UHoudiniAsset* B);
	static bool IsEquivalent(const UHoudiniParameter* A, const UHoudiniParameter* B);
	static bool IsEquivalent(const UHoudiniInput* A, const UHoudiniInput* B);
	static bool IsEquivalent(const UHoudiniInputObject* A, const UHoudiniInputObject* B);
	static bool IsEquivalent(const UHoudiniOutput* A, const UHoudiniOutput* B);
	static bool IsEquivalent(const UHoudiniHandleComponent* A, const UHoudiniHandleComponent* B);
	static bool IsEquivalent(const UHoudiniPDGAssetLink* A, const UHoudiniPDGAssetLink* B);
	
	// Struct equivalence
	static bool IsEquivalent(const FHoudiniStaticMeshGenerationProperties& A, const FHoudiniStaticMeshGenerationProperties& B);
	static bool IsEquivalent(const FHoudiniBakedOutput& A, const FHoudiniBakedOutput& B);
	static bool IsEquivalent(const FHoudiniGeoPartObject& A, const FHoudiniGeoPartObject& B);
	static bool IsEquivalent(const FHoudiniInstancedOutput& A, const FHoudiniInstancedOutput& B);
	static bool IsEquivalent(const FHoudiniBakedOutputObject& A, const FHoudiniBakedOutputObject& B);
	static bool IsEquivalent(const FHoudiniObjectInfo& A, const FHoudiniObjectInfo& B);
	static bool IsEquivalent(const FHoudiniGeoInfo& A, const FHoudiniGeoInfo& B);
	static bool IsEquivalent(const FHoudiniPartInfo& A, const FHoudiniPartInfo& B);
	static bool IsEquivalent(const FHoudiniVolumeInfo& A, const FHoudiniVolumeInfo& B);
	static bool IsEquivalent(const FHoudiniCurveInfo& A, const FHoudiniCurveInfo& B);
	static bool IsEquivalent(const FHoudiniMeshSocket& A, const FHoudiniMeshSocket& B);
	static bool IsEquivalent(const FHoudiniOutputObject& A, const FHoudiniOutputObject& B);
	static bool IsEquivalent(const FHoudiniCurveOutputProperties& A, const FHoudiniCurveOutputProperties& B);
	
	// Unreal equivalence
	static bool IsEquivalent(const FDirectoryPath& A, const FDirectoryPath& B);
	static bool IsEquivalent(const FMeshBuildSettings& A, const FMeshBuildSettings& B);
	static bool IsEquivalent(const FTransform& A, const FTransform& B);
	static bool IsEquivalent(const FBodyInstance& A, const FBodyInstance& B);
	static bool IsEquivalent(const FWalkableSlopeOverride& A, const FWalkableSlopeOverride& B);
	static bool IsEquivalent(const FColor& A, const FColor& B);
	static bool IsEquivalent(const FLinearColor& A, const FLinearColor& B);
	static bool IsEquivalent(const FVector& A, const FVector& B);
	static bool IsEquivalent(const FQuat& A, const FQuat& B);
	static bool IsEquivalent(const FIntVector& A, const FIntVector& B);
	static bool IsEquivalent(const FVector2D& A, const FVector2D& B);
	static bool IsEquivalent(const FStaticMaterial& A, const FStaticMaterial& B);
	static bool IsEquivalent(const FBox& A, const FBox& B);

	// Runtime UObjects:
	static bool IsEquivalent(const UObject * A, const UObject * B);
	static bool IsEquivalent(const UPhysicalMaterial * A, const UPhysicalMaterial * B);
	static bool IsEquivalent(const AActor * A, const AActor * B);
	static bool IsEquivalent(const ALandscapeProxy * A, const ALandscapeProxy * B);

	static bool IsEquivalent(const AHoudiniAssetActor* A, const AHoudiniAssetActor* B);
	static bool IsEquivalent(const UHoudiniInstancedActorComponent* A, const UHoudiniInstancedActorComponent* B);
	static bool IsEquivalent(const UHoudiniMeshSplitInstancerComponent* A, const UHoudiniMeshSplitInstancerComponent* B);
	static bool IsEquivalent(const UHoudiniParameterButton* A, const UHoudiniParameterButton* B);
	static bool IsEquivalent(const UHoudiniParameterButtonStrip* A, const UHoudiniParameterButtonStrip* B);
	static bool IsEquivalent(const UHoudiniParameterChoice* A, const UHoudiniParameterChoice* B);
	static bool IsEquivalent(const UHoudiniParameterColor* A, const UHoudiniParameterColor* B);
	static bool IsEquivalent(const UHoudiniParameterFile* A, const UHoudiniParameterFile* B);
	static bool IsEquivalent(const UHoudiniParameterFloat* A, const UHoudiniParameterFloat* B);
	static bool IsEquivalent(const UHoudiniParameterFolder* A, const UHoudiniParameterFolder* B);
	static bool IsEquivalent(const UHoudiniParameterFolderList* A, const UHoudiniParameterFolderList* B);
	static bool IsEquivalent(const UHoudiniParameterInt* A, const UHoudiniParameterInt* B);
	static bool IsEquivalent(const UHoudiniParameterLabel* A, const UHoudiniParameterLabel* B);
	static bool IsEquivalent(const UHoudiniParameterMultiParm* A, const UHoudiniParameterMultiParm* B);
	static bool IsEquivalent(const UHoudiniParameterOperatorPath* A, const UHoudiniParameterOperatorPath* B);
	static bool IsEquivalent(const UHoudiniParameterRampFloatPoint* A, const UHoudiniParameterRampFloatPoint* B);
	static bool IsEquivalent(const UHoudiniParameterRampColorPoint* A, const UHoudiniParameterRampColorPoint* B);
	static bool IsEquivalent(const UHoudiniParameterRampFloat* A, const UHoudiniParameterRampFloat* B);
	static bool IsEquivalent(const UHoudiniParameterRampColor* A, const UHoudiniParameterRampColor* B);
	static bool IsEquivalent(const UHoudiniParameterSeparator* A, const UHoudiniParameterSeparator* B);
	static bool IsEquivalent(const UHoudiniParameterString* A, const UHoudiniParameterString* B);
	static bool IsEquivalent(const UHoudiniParameterToggle* A, const UHoudiniParameterToggle* B);
	static bool IsEquivalent(const UHoudiniSplineComponent* A, const UHoudiniSplineComponent* B);
	static bool IsEquivalent(const UHoudiniStaticMesh* A, const UHoudiniStaticMesh* B);
	static bool IsEquivalent(const UHoudiniStaticMeshComponent* A, const UHoudiniStaticMeshComponent* B);
	static bool IsEquivalent(const UHoudiniLandscapePtr* A, const UHoudiniLandscapePtr* B);
	static bool IsEquivalent(const UHoudiniLandscapeEditLayer* A, const UHoudiniLandscapeEditLayer* B);
	static bool IsEquivalent(const UStaticMeshComponent* A, const UStaticMeshComponent* B);
	static bool IsEquivalent(const UMaterialInterface* A, const UMaterialInterface* B);
	static bool IsEquivalent(const UStaticMesh* A, const UStaticMesh* B);

private:
#if WITH_DEV_AUTOMATION_TESTS
	static void SetAutomationBase(FAutomationTestBase* Test);
	static FAutomationTestBase* TestBase;
#endif

	static bool TestExpressionErrorEnabled;

	static bool TestExpressionError(const bool Expression, const FString& Header, const FString& Subject);

	static void SetTestExpressionError(bool Enabled);
};
