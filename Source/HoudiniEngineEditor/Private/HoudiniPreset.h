/*
* Copyright (c) <2023> Side Effects Software Inc.
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
#include "HoudiniEngineRuntimeCommon.h"
#include "HoudiniInput.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniToolData.h"

#include "HoudiniPreset.generated.h"

/**
 * This is a Houdini Tools package descriptor inside of UE, typically created
 * after a HoudiniToolsPackage has been imported into the UE project.
 */

class UHoudiniInput;
class UHoudiniAsset;
class UHoudiniParameterChoice;
class UHoudiniParameterColor;
class UHoudiniParameterToggle;
class UHoudiniParameterInt;
class UHoudiniParameterFloat;
class UHoudiniParameterString;
class UHoudiniParameterFile;
class UHoudiniParameterRampFloat;
class UHoudiniParameterRampColor;
class UHoudiniParameterOperatorPath;


UENUM()
enum class EHoudiniPresetValueType
{
	Invalid,
	Float,
	Int,
	String
};


USTRUCT()
struct FHoudiniPresetBase
{
	GENERATED_BODY()
	virtual ~FHoudiniPresetBase() {}

	virtual FString ToString() { return FString(); }

	virtual EHoudiniPresetValueType GetValueType() { return EHoudiniPresetValueType::Invalid; }
};


USTRUCT(BlueprintType)
struct FHoudiniPresetFloatValues : public FHoudiniPresetBase
{
	GENERATED_BODY()
	
	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::Float; }

	virtual FString ToString() override { return FString::JoinBy(Values, TEXT(", "), [](const float& Value) { return FString::SanitizeFloat(Value); }); }
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<float> Values;
};


USTRUCT(BlueprintType)
struct FHoudiniPresetIntValues : public FHoudiniPresetBase
{
	GENERATED_BODY()

	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::Int; }

	virtual FString ToString() override { return FString::JoinBy(Values, TEXT(", "), [](const float& Value) { return FString::FromInt(Value); }); }
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<int> Values;
};


USTRUCT(BlueprintType)
struct FHoudiniPresetStringValues : public FHoudiniPresetBase
{
	GENERATED_BODY()

	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::String; }

	virtual FString ToString() override { return FString::Join(Values, TEXT(", ")); }
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<FString> Values;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetRampFloatPoint
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	float Position = 0.0f;
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	float Value = 0.0f;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	EHoudiniRampInterpolationType Interpolation = EHoudiniRampInterpolationType::LINEAR;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetRampColorPoint
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	float Position = 0.0f;
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	FLinearColor Value = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	EHoudiniRampInterpolationType Interpolation = EHoudiniRampInterpolationType::LINEAR;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetRampFloatValues : public FHoudiniPresetBase
{
	GENERATED_BODY()

	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::String; }

	virtual FString ToString() override { return FString::Format(TEXT("%d float points."), { Points.Num() }); }

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<FHoudiniPresetRampFloatPoint> Points;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetRampColorValues : public FHoudiniPresetBase
{
	GENERATED_BODY()

	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::String; }

	virtual FString ToString() override { return FString::Format(TEXT("{0} color points."), { Points.Num() }); }

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<FHoudiniPresetRampColorPoint> Points;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetMultiParmValues : public FHoudiniPresetBase
{
	GENERATED_BODY()

	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::Int; }

	virtual FString ToString() override { return FString::Format(TEXT("Number of Elements: {0}"), { Count }); }

	UPROPERTY(EditAnywhere, Category = "Houdini Preset")
	int Count = 0;;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetGeometryInputObject : public FHoudiniPresetBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TSoftObjectPtr<UObject> InputObject;
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	FTransform Transform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FHoudiniPresetCurveInputObject : public FHoudiniPresetBase
{
	GENERATED_BODY()

	FHoudiniPresetCurveInputObject();

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	FTransform Transform = FTransform::Identity;
	
	// Curve Spline Component

	EHoudiniCurveType GetValidCurveType() const
	{
		if (CurveType == EHoudiniCurveType::Invalid)
		{
			return EHoudiniCurveType::Polygon;
		}
		return CurveType;
	}

	EHoudiniCurveMethod GetValidCurveMethod() const
	{
		if (CurveMethod == EHoudiniCurveMethod::Invalid)
		{
			return EHoudiniCurveMethod::CVs;
		}
		return CurveMethod;
	}

	EHoudiniCurveBreakpointParameterization GetValidCurveBreakpointParameterization() const
	{
		if (CurveBreakpointParameterization == EHoudiniCurveBreakpointParameterization::Invalid)
		{
			return EHoudiniCurveBreakpointParameterization::Uniform;
		}
		return CurveBreakpointParameterization;
	}
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<FTransform> CurvePoints;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bClosed = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bReversed = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	int32 CurveOrder = 2;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bIsHoudiniSplineVisible = true;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	EHoudiniCurveType CurveType = EHoudiniCurveType::Polygon;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	EHoudiniCurveMethod CurveMethod = EHoudiniCurveMethod::CVs;

	// Only used for new HAPI curve / breakpoints
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	EHoudiniCurveBreakpointParameterization CurveBreakpointParameterization = EHoudiniCurveBreakpointParameterization::Uniform;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bIsOutputCurve = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bCookOnCurveChanged = true;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bIsLegacyInputCurve = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bIsInputCurve = false;
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bIsEditableOutputCurve = false;
};


USTRUCT(BlueprintType)
struct FHoudiniPresetInputValue : public FHoudiniPresetBase
{
	GENERATED_BODY()

	virtual EHoudiniPresetValueType GetValueType() override { return EHoudiniPresetValueType::String; }

	virtual FString ToString() override { return FString(); }

	// Export Options
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bKeepWorldTransform = true;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bPackGeometryBeforeMerging = false;
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bExportInputAsReference = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bExportLODs = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bExportSockets = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bExportColliders = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bExportMaterialParameters = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bMergeSplineMeshComponents = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bPreferNaniteFallbackMesh = false;
	
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bUseMeshDescription = true;
	
	// Input properties

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	EHoudiniInputType InputType = EHoudiniInputType::Geometry;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bIsParameterInput = false;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	FString ParameterName;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	int32 InputIndex = -1;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<FHoudiniPresetGeometryInputObject> GeometryInputObjects;

	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TArray<FHoudiniPresetCurveInputObject> CurveInputObjects;
};


struct FHoudiniPresetHelpers
{
	static bool IsSupportedInputType(const EHoudiniInputType InputType);
	
	// GetParameterValues to return arrays of the type and a string version of the values.
	static bool GetParameterValues(const UHoudiniParameterInt* Param, TMap<FString,FHoudiniPresetIntValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterChoice* Param, TMap<FString,FHoudiniPresetIntValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterToggle* Param, TMap<FString,FHoudiniPresetIntValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterFloat* Param, TMap<FString,FHoudiniPresetFloatValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterColor* Param, TMap<FString,FHoudiniPresetFloatValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterString* Param, TMap<FString,FHoudiniPresetStringValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterFile* Param, TMap<FString,FHoudiniPresetStringValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterChoice* Param, TMap<FString,FHoudiniPresetStringValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterMultiParm* Param, TMap<FString, FHoudiniPresetMultiParmValues>& OutValues, FString& OutValueStr);


	// Ingest ramp parameters
	static bool GetParameterValues(const UHoudiniParameterRampFloat* Param, TMap<FString,FHoudiniPresetRampFloatValues>& OutValues, FString& OutValueStr);
	static bool GetParameterValues(const UHoudiniParameterRampColor* Param, TMap<FString,FHoudiniPresetRampColorValues>& OutValues, FString& OutValueStr);

	// Ingest input parameters
	static void GetGenericInput(UHoudiniInput* Input, bool bIsParameterInput, const FString& ParameterName, TArray<FHoudiniPresetInputValue>& OutValues);
	static void UpdateGenericInputSettings(FHoudiniPresetInputValue& Value, const UHoudiniInput* Input);
	static void UpdateFromGeometryInput(FHoudiniPresetInputValue& Value, const UHoudiniInput* Input);
	static void UpdateFromCurveInput(FHoudiniPresetInputValue& Value, const UHoudiniInput* Input);


	// Preset Helpers
	static void ApplyPresetParameterValues(const FHoudiniPresetIntValues& Values, UHoudiniParameterInt* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetIntValues& Values, UHoudiniParameterChoice* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetIntValues& Values, UHoudiniParameterToggle* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetFloatValues& Values, UHoudiniParameterFloat* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetFloatValues& Values, UHoudiniParameterColor* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetStringValues& Values, UHoudiniParameterString* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetStringValues& Values, UHoudiniParameterFile* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetStringValues& Values, UHoudiniParameterChoice* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetRampFloatValues& Values, UHoudiniParameterRampFloat* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetRampColorValues& Values, UHoudiniParameterRampColor* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetMultiParmValues& Values, UHoudiniParameterMultiParm* Param);
	static void ApplyPresetParameterValues(const FHoudiniPresetInputValue& PresetInput, UHoudiniInput* Input);

protected:
	static void ApplyPresetGeometryInput(const FHoudiniPresetInputValue& PresetInput, UHoudiniInput* Input);
	static void ApplyPresetCurveInput(const FHoudiniPresetInputValue& PresetInput, UHoudiniInput* Input);
};


UCLASS(BlueprintType, hidecategories=(Object))
class HOUDINIENGINEEDITOR_API UHoudiniPreset : public UObject
{
	GENERATED_BODY()
public:

	UHoudiniPreset();

	#if WITH_EDITOR
		virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	#endif

	// The label for this preset
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	FString Name;

	// A description for this preset
	UPROPERTY(EditAnywhere, Category="Houdini Preset", meta=(MultiLine="true"))
	FString Description;

	// The HoudiniAsset linked to this preset.
	// Should this be a soft object pointer instead?
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	TObjectPtr<UHoudiniAsset> SourceHoudiniAsset;
	
	// Whether the revert all parameters on the HDA to their default values before applying this preset
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bRevertHDAParameters;

	// Whether to treat this preset as hidden (hide from preset menus and will be not be visible in HoudiniTools Panel). 
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bHidePreset;

	// Whether this preset can be applied to any HDA, or only the SourceHoudiniAsset.
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bApplyOnlyToSource;

	// Whether this preset be instantiated (using the SourceHoudiniAsset).
	UPROPERTY(EditAnywhere, Category="Houdini Preset")
	bool bCanInstantiate;

	// Cook and Bake Folders
	// We add toggles specifically for temp/bake folders since we might want to
	// control them separately respective options groups.
	
	UPROPERTY(EditAnywhere, Category="Cook Options")
	bool bApplyTemporaryCookFolder;
	
	UPROPERTY(EditAnywhere, Category="Cook Options")
	FString TemporaryCookFolder;
	
	UPROPERTY(EditAnywhere, Category="Bake options")
	bool bApplyBakeFolder;
	
	UPROPERTY(EditAnywhere, Category="Bake Options")
	FString BakeFolder;
	
	// Bake Options

	UPROPERTY(EditAnywhere, Category="Bake Options")
	bool bApplyBakeOptions;

	UPROPERTY(EditAnywhere, Category="Bake Options")
	EHoudiniEngineBakeOption HoudiniEngineBakeOption;

	UPROPERTY(EditAnywhere, Category="Bake Options", meta = (DisplayName = "Remove Cooked Output After Bake"))
	bool bRemoveOutputAfterBake;

	UPROPERTY(EditAnywhere, Category="Bake Options")
	bool bRecenterBakedActors;

	UPROPERTY(EditAnywhere, Category="Bake Options")
	bool bAutoBake;
	
	UPROPERTY(EditAnywhere, Category="Bake Options")
	bool bReplacePreviousBake;
	
	// Asset Options

	UPROPERTY(EditAnywhere, Category="Cook Options")
	bool bApplyAssetOptions;

	// Asset Options - Cook Triggers

	UPROPERTY(EditAnywhere, Category="Cook Options")
	bool bCookOnParameterChange;

	UPROPERTY(EditAnywhere, Category="Cook Options")
	bool bCookOnTransformChange;

	UPROPERTY(EditAnywhere, Category="Cook Options", meta = (DisplayName = "Cook on HDA Input Cook"))
	bool bCookOnAssetInputCook;

	// Asset Options - Outputs

	UPROPERTY(EditAnywhere, Category="Cook Options")
	bool bDoNotGenerateOutputs;

	UPROPERTY(EditAnywhere, Category="Cook Options")
	bool bUseOutputNodes;

	UPROPERTY(EditAnywhere, Category="Cook Options", meta = (DisplayName = "Use Templated Geos"))
	bool bOutputTemplateGeos;

	// Asset Options - Misc

	UPROPERTY(EditAnywhere, Category="Cook Options", meta = (DisplayName = "Push Transforms to Houdini"))
	bool bUploadTransformsToHoudiniEngine;

	UPROPERTY(EditAnywhere, Category="Cook Options", meta = (DisplayName = "Temp Landscape Layers"))
	bool bLandscapeUseTempLayers;

	
	// Parameters

	UPROPERTY(EditAnywhere, Category="Houdini Parameters")
	TMap<FString, FHoudiniPresetFloatValues> FloatParameters;

	UPROPERTY(EditAnywhere, Category="Houdini Parameters")
	TMap<FString, FHoudiniPresetIntValues> IntParameters;

	UPROPERTY(EditAnywhere, Category="Houdini Parameters")
	TMap<FString, FHoudiniPresetStringValues> StringParameters;

	UPROPERTY(EditAnywhere, Category="Houdini Parameters")
	TMap<FString, FHoudiniPresetRampFloatValues> RampFloatParameters;
	
	UPROPERTY(EditAnywhere, Category="Houdini Parameters")
	TMap<FString, FHoudiniPresetRampColorValues> RampColorParameters;

	UPROPERTY(EditAnywhere, Category = "Houdini Parameters")
	TMap<FString, FHoudiniPresetMultiParmValues> MultiParmParameters;

	// Inputs
	
	UPROPERTY(EditAnywhere, Category="Houdini Parameters")
	TArray<FHoudiniPresetInputValue> InputParameters;

	// Static Mesh Generation Settings

	UPROPERTY(EditAnywhere, Category="HoudiniMeshGeneration")
	bool bApplyStaticMeshGenSettings;

	UPROPERTY(EditAnywhere, Category="HoudiniMeshGeneration")
	FHoudiniStaticMeshGenerationProperties StaticMeshGenerationProperties;
	
	UPROPERTY(EditAnywhere, Category = "HoudiniMeshGeneration")
	FMeshBuildSettings StaticMeshBuildSettings;
	
	// Proxy Mesh Gen Settings

	UPROPERTY(EditAnywhere, Category="HoudiniProxyMeshGeneration")
	bool bApplyProxyMeshGenSettings;

	UPROPERTY(EditAnywhere, Category = "HoudiniProxyMeshGeneration")
	bool bOverrideGlobalProxyStaticMeshSettings;

	UPROPERTY(EditAnywhere, Category = "HoudiniProxyMeshGeneration", meta = (DisplayName="Enable Proxy Static Mesh"))
	bool bEnableProxyStaticMeshOverride;
	
	UPROPERTY(EditAnywhere, Category = "HoudiniProxyMeshGeneration", meta = (DisplayName="Refine Proxy Static Meshes After a Timeout"))
	bool bEnableProxyStaticMeshRefinementByTimerOverride;
	
	UPROPERTY(EditAnywhere, Category = "HoudiniProxyMeshGeneration", meta = (DisplayName="Proxy Mesh Auto Refine Timeout Seconds"))
	float ProxyMeshAutoRefineTimeoutSecondsOverride;
	
	UPROPERTY(EditAnywhere, Category = "HoudiniProxyMeshGeneration", meta = (DisplayName="Refine Proxy Static Meshes When Saving a Map"))
	bool bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;
	
	UPROPERTY(EditAnywhere, Category = "HoudiniProxyMeshGeneration", meta = (DisplayName="Refine Proxy Static Meshes On PIE"))
	bool bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;

	// Image settings (cop HDAs only)
	UPROPERTY(EditAnywhere, Category="HoudiniImageData")
	bool bApplyImageData;

	UPROPERTY(EditAnywhere, Category = "HoudiniImageData", meta = (DisplayName="Override Default Resolution"))
	bool bOverrideDefaultResolution;

	UPROPERTY(EditAnywhere, Category = "HoudiniImageData", meta = (DisplayName="Resolution Override"))
	FIntPoint ResolutionOverride;

	UPROPERTY(EditAnywhere, Category = "HoudiniImageData", meta = (DisplayName="Override Pixel Scale"))
	bool bOverridePixelScale;

	UPROPERTY(EditAnywhere, Category = "HoudiniImageData", meta = (DisplayName="Pixel Scale"))
	float PixelScale;
		
	UPROPERTY(EditAnywhere, Category = "HoudiniImageData", meta = (DisplayName="Generate Material"))
	bool bGenerateMaterial;

	UPROPERTY(EditAnywhere, Category = "HoudiniImageData", meta = (DisplayName="Source Material"))
	TObjectPtr<UMaterialInterface> MaterialToInstance = nullptr;
	
	
	// Raw image data of the icon to be displayed
	UPROPERTY()
	FHImageData IconImageData;

	// Callback for when this preset has been instantiated.
	// Used for testing.
	TArray< TFunction<void(const UHoudiniPreset*, UObject *)> > PostInstantiationCallbacks;


};
