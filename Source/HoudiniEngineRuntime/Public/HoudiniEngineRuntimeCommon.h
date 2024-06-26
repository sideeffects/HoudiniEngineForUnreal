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

#include "HoudiniEngineRuntimeCommon.generated.h"

UENUM()
enum class EHoudiniRampInterpolationType : int8
{
	InValid = -1,

	CONSTANT = 0,
	LINEAR = 1,
	CATMULL_ROM = 2,
	MONOTONE_CUBIC = 3,
	BEZIER = 4,
	BSPLINE = 5,
	HERMITE = 6
};

#if WITH_EDITORONLY_DATA
UENUM()
enum class EHoudiniEngineBakeOption : uint8
{
	ToActor,
	ToBlueprint
};
#endif

UENUM()
enum class EHoudiniEngineActorBakeOption : uint8
{
	OneActorPerComponent,
	OneActorPerHDA
};

UENUM()
enum class EHoudiniLandscapeOutputBakeType : uint8
{
	Detachment,
	BakeToImage,
	BakeToWorld,
	InValid,
};

UENUM()
enum class EHoudiniInputType : uint8
{
	Invalid = 0,

	Geometry = 1,

	Curve = 2,

	/* Asset = 3 - deprecated in Houdini 20 */
	
	/* Landscape = 4 - deprecated in Houdini 20 */
	
	World = 5

	/* Skeletal = 6 - deprecated in Houdini 20 */

	/* Geometry Collection = 7 - deprecated in Houdini 20 */

	// !! The next input type should be 8 !!
};

UENUM()
enum class EHoudiniOutputType : uint8
{
	Invalid,

	Mesh,
	Instancer,
	Landscape,
	Curve,
	Skeletal,
	GeometryCollection,
	DataTable,
	LandscapeSpline,
	AnimSequence
};

UENUM()
enum class EHoudiniCurveType : int8
{
	Invalid = -1,

	Polygon = 0,
	Nurbs = 1,
	Bezier = 2,
	Points = 3
};

UENUM()
enum class EHoudiniCurveMethod : int8
{
	Invalid = -1,

	CVs = 0,
	Breakpoints = 1,
	Freehand = 2
};

UENUM()
enum class EHoudiniCurveBreakpointParameterization : int8
{
	Invalid = -1,
	Uniform = 0,
	Chord = 1,
	Centripetal = 2
};

UENUM()
enum class EHoudiniLandscapeExportType : uint8
{
	Heightfield,
	Mesh,
	Points
};

#if WITH_EDITORONLY_DATA
UENUM()
enum class EPDGBakeSelectionOption : uint8
{
	All,
	SelectedNetwork,
	SelectedNode
};
#endif

#if WITH_EDITORONLY_DATA
UENUM()
enum class EPDGBakePackageReplaceModeOption : uint8
{
	CreateNewAssets,
	ReplaceExistingAssets
};
#endif

// When attempting to refine proxy mesh outputs it is a possible that a cook is needed. 
// This enum defines the possible return values on a request to refine proxies.
UENUM()
enum class EHoudiniProxyRefineResult : uint8
{
	Invalid,

	// Refinement (or cook if needed) failed
	Failed,
	// Refinement completed successfully
	Success,
	// Refinement was skipped, either it was not necessary or the operation was cancelled by the user
	Skipped
};

// When attempting to refine proxy mesh outputs it is a possible that a cook is needed. 
// This enum defines the possible return values on a request to refine proxies.
UENUM()
enum class EHoudiniProxyRefineRequestResult : uint8
{
	Invalid,

	// No refinement is needed
	None,
	// A cook is needed, refinement will commence automatically after the cook
	PendingCooks,
	// Successfully refined
	Refined
};

