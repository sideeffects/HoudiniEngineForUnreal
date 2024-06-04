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
#include "UObject/ObjectMacros.h"
#include "HoudiniEngineRuntimeCommon.h"

#include "HoudiniInputTypes.generated.h"

// Forward declarations
class UHoudiniInput;

// Maintain an iterable list of houdini input types
static const EHoudiniInputType HoudiniInputTypeList[] = 
{
	EHoudiniInputType::Geometry,
	EHoudiniInputType::Curve,
	EHoudiniInputType::World
};

UENUM()
enum class EHoudiniXformType : uint8
{
	None,
	IntoThisObject,
	Auto
};

// Struct of input settings that affect object exporting to Houdini
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniInputObjectSettings
{
	GENERATED_BODY();

public:
	FHoudiniInputObjectSettings();
	
	// Construct from a HoudiniInput. Copies all supported properties.
	FHoudiniInputObjectSettings(UHoudiniInput const* InInput);

	UPROPERTY()
	EHoudiniXformType KeepWorldTransform;
	
	// Indicates that all the input objects are imported to Houdini as references instead of actual geo
	// (for Geo/World/Asset input types only)
	UPROPERTY()
	bool bImportAsReference;
	
	// Indicates that whether or not to add the rot / scale attributes for reference imports
	UPROPERTY()
	bool bImportAsReferenceRotScaleEnabled;
	
	// Indicates whether or not to add bbox attributes for reference imports
	UPROPERTY()
	bool bImportAsReferenceBboxEnabled;
	
	// Indicates whether or not to add material attributes for reference imports
	UPROPERTY()
	bool bImportAsReferenceMaterialEnabled;

	// Indicates that all LODs in the input should be marshalled to Houdini
	UPROPERTY()
	bool bExportLODs;
	
	// Indicates that all sockets in the input should be marshalled to Houdini
	UPROPERTY()
	bool bExportSockets;
	
	// Override property for preferring the Nanite fallback mesh when using a Nanite geometry as input
	UPROPERTY()
	bool bPreferNaniteFallbackMesh;

	// Indicates that all colliders in the input should be marshalled to Houdini
	UPROPERTY()
	bool bExportColliders;
	
	// Indicates that material parameters should be exported as attributes
	UPROPERTY()
	bool bExportMaterialParameters;

	// Set this to true to add rot and scale attributes on curve inputs.
	UPROPERTY()
	bool bAddRotAndScaleAttributesOnCurves;

	// Set this to true to use legacy (curve::1.0) input curves
	UPROPERTY()
	bool bUseLegacyInputCurves;
	
	// Resolution used when converting unreal splines to houdini curves
	UPROPERTY()
	float UnrealSplineResolution;

	// Indicates if the landscape should be exported as heightfield, mesh or points
	UPROPERTY()
	EHoudiniLandscapeExportType LandscapeExportType;

	// Is set to true when landscape input is set to selection only.
	UPROPERTY()
	bool bLandscapeExportSelectionOnly;

	// Is set to true when the automatic selection of landscape component is active
	UPROPERTY()
	bool bLandscapeAutoSelectComponent;

	// Is set to true when materials are to be exported.
	UPROPERTY()
	bool bLandscapeExportMaterials;

	// Is set to true when lightmap information export is desired.
	UPROPERTY()
	bool bLandscapeExportLighting;

	// Is set to true when uvs should be exported in [0,1] space.
	UPROPERTY()
	bool bLandscapeExportNormalizedUVs;

	// Is set to true when uvs should be exported for each tile separately.
	UPROPERTY()
	bool bLandscapeExportTileUVs;

	// If true, also export a landscape's splines
	UPROPERTY()
	bool bLandscapeAutoSelectSplines;

	// If true, send a separate control point cloud of the landscape splines control points.
	UPROPERTY()
	bool bLandscapeSplinesExportControlPoints;

	// If true, export left and right curves as well
	UPROPERTY()
	bool bLandscapeSplinesExportLeftRightCurves;

	// If true, export the spline mesh components of landscape splines
	UPROPERTY()
	bool bLandscapeSplinesExportSplineMeshComponents;

	// If true, the deformed meshes of all spline mesh components of an actor are merged into temporary input mesh.
	// If false, the meshes are sent individually.
	UPROPERTY()
	bool bMergeSplineMeshComponents;

	// If enabled, height data is exported per Edit Layer.
	UPROPERTY()
	bool bExportHeightDataPerEditLayer;

	// Export paint layers.
	UPROPERTY()
	bool bExportPaintLayersPerEditLayer;

	// Export paint layers.
	UPROPERTY()
	bool bExportMergedPaintLayers;

	// If enabled, level instances (and packed level actor) content is exported vs just exporting a single point
	// with attributes identifying the level instance / packed level actor.
	UPROPERTY()
	bool bExportLevelInstanceContent;

};
