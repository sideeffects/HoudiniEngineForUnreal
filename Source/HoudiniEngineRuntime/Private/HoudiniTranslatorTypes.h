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

#include "HoudiniEngineRuntimePrivatePCH.h"

class ALandscape;

// Used to cache data to use as a reference point during landscape construction
// The very first tile will calculate landscape 
struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeTileSizeInfo
{
	FHoudiniLandscapeTileSizeInfo();
	bool bIsCached;

	// Tile sizes
	int32 UnrealSizeX;
	int32 UnrealSizeY;
	int32 NumSectionsPerComponent;
	int32 NumQuadsPerSection;
};

// Used to cache the extent of the landscape so that it doesn't have to be recalculated
// for each landscape tile.
// The very first tile will calculate landscape 
struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeExtent
{
	FHoudiniLandscapeExtent();
	bool bIsCached;

	// Landscape extents (in quads)
	int32 MinX, MaxX, MinY, MaxY;
	int32 ExtentsX;
	int32 ExtentsY;
};

// Used to cache data to use as a reference point during landscape construction
// The very first tile will calculate a reference point as well as a component-space location.
// Every subsequent tile can then derive a component-space location from this location. 
struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeReferenceLocation
{
	FHoudiniLandscapeReferenceLocation();
	
	bool bIsCached;
	// Absolute section base coordinate for the cached tile.
	int32 SectionCoordX;
	int32 SectionCoordY;
	// Scaled location for the reference tile.
	float TileLocationX;
	float TileLocationY;
	// Transform of the main landscape actor.
	FTransform MainTransform;
};