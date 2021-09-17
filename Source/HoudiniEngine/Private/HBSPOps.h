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
#include "Engine/Brush.h"
#include "Engine/Polys.h"

#include "HBSPOps.generated.h"

class AVolume;
class UModel;

// This codebase have been localised from UnrealEd/HBSPOps to remove static/global variables. 
class FHBSPOps
{
public:
	FHBSPOps();

	/** Quality level for rebuilding Bsp. */
	enum EBspOptimization
	{
		BSP_Lame,
		BSP_Good,
		BSP_Optimal
	};

	/** Possible positions of a child Bsp node relative to its parent (for BspAddToNode) */
	enum ENodePlace 
	{
		NODE_Back		= 0, // Node is in back of parent              -> Bsp[iParent].iBack.
		NODE_Front		= 1, // Node is in front of parent             -> Bsp[iParent].iFront.
		NODE_Plane		= 2, // Node is coplanar with parent           -> Bsp[iParent].iPlane.
		NODE_Root		= 3, // Node is the Bsp root and has no parent -> Bsp[0].
	};

	static void csgPrepMovingBrush( ABrush* Actor, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	static void csgCopyBrush( ABrush* Dest, ABrush* Src, uint32 PolyFlags, EObjectFlags ResFlags, bool bNeedsPrep, bool bCopyPosRotScale, bool bAllowEmpty, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	static ABrush*	csgAddOperation( ABrush* Actor, uint32 PolyFlags, EBrushType BrushType, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );

	static int32 bspAddVector( UModel* Model, FVector* V, bool Exact, UHBspPointsGrid* BspVectors );
	static int32 bspAddPoint( UModel* Model, FVector* V, bool Exact, UHBspPointsGrid* BspPoints );
	static void bspBuild( UModel* Model, enum EBspOptimization Opt, int32 Balance, int32 PortalBias, int32 RebuildSimplePolys, int32 iNode, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	static void bspRefresh( UModel* Model, bool NoRemapSurfs );

	static void bspBuildBounds( UModel* Model );

	static void bspValidateBrush( UModel* Brush, bool ForceValidate, bool DoStatusUpdate );
	static void bspUnlinkPolys( UModel* Brush );
	static int32 bspAddNode( UModel* Model, int32 iParent, enum ENodePlace NodePlace, uint32 NodeFlags, FPoly* EdPoly, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );

	/**
	 * Rebuild some brush internals
	 */
	static void RebuildBrush(UModel* Brush, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors);

	static FPoly BuildInfiniteFPoly( UModel* Model, int32 iNode );

	/**
	 * Rotates the specified brush's vertices.
	 */
	static void RotateBrushVerts(ABrush* Brush, const FRotator& Rotation, bool bClearComponents, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors);

	/** Called when an AVolume shape is changed*/
	static void HandleVolumeShapeChanged(AVolume& Volume, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors);

	/** Errors encountered in Csg operation. */
	static int32 GErrors;
	static bool GFastRebuild;

protected:
	static void SplitPolyList
	(
		UModel				*Model,
		int32                 iParent,
		FHBSPOps::ENodePlace NodePlace,
		int32                 NumPolys,
		FPoly				**PolyList,
		EBspOptimization	Opt,
		int32					Balance,
		int32					PortalBias,
		int32					RebuildSimplePolys,
		UHBspPointsGrid* BspPoints,
		UHBspPointsGrid* BspVectors
	);
};


struct FHBspPointsKey
{
	int32 X;
	int32 Y;
	int32 Z;

	FHBspPointsKey(int32 InX, int32 InY, int32 InZ)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{}

	friend FORCEINLINE bool operator == (const FHBspPointsKey& A, const FHBspPointsKey& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FHBspPointsKey& Key)
	{
		return HashCombine(static_cast<uint32>(Key.X), HashCombine(static_cast<uint32>(Key.Y), static_cast<uint32>(Key.Z)));
	}
};

struct FHBspIndexedPoint
{
	FHBspIndexedPoint(const FVector& InPoint, int32 InIndex)
		: Point(InPoint)
		, Index(InIndex)
	{}

	FVector Point;
	int32 Index;
};


struct FHBspPointsGridItem
{
	TArray<FHBspIndexedPoint, TInlineAllocator<16>> IndexedPoints;
};


// Represents a sparse granular 3D grid into which points are added for quick (~O(1)) lookup.
// The 3D space is divided into a grid with a given granularity.
// Points are considered to have a given radius (threshold) and are added to the grid cube they fall in, and to up to seven neighbours if they overlap.
UCLASS()
class HOUDINIENGINE_API UHBspPointsGrid : public UObject
{
	GENERATED_BODY()
protected:
	
	UHBspPointsGrid() {}

public:
	// Create a new instance of this grid with the given arguments.
	static UHBspPointsGrid* Create(float InGranularity, float InThreshold, int32 InitialSize = 0);

	void Clear(int32 InitialSize = 0);

	int32 FindOrAddPoint(const FVector& Point, int32 Index, float Threshold);

	static FORCEINLINE int32 GetAdjacentIndexIfOverlapping(int32 GridIndex, float GridPos, float GridThreshold);

private:
	float OneOverGranularity;
	float Threshold;

	typedef TMap<FHBspPointsKey, FHBspPointsGridItem> FGridMap;
	FGridMap GridMap;
};
