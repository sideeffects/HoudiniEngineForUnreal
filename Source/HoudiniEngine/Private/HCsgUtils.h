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

#include "HBSPOps.h"
#include "Engine/Brush.h"
#include "Model.h"

#include "HCsgUtils.generated.h"

//USTRUCT()
//struct FHCsgContext
//{
//	GENERATED_BODY()
//
//	int32 Errors;
//	
//	
//	UPROPERTY()
//	class UModel* TempModel;
//
//	UPROPERTY()
//	class UModel* ConversionTempModel;
//};

// This HCsgUtils is one big fork of the codebase located UnrealEd/Private/EditorBsp.cpp.
// The main purpose was to remove parts of the code that store state in global/static variables as well
// as dependency on editor state (such as retrieving selected brushes).
UCLASS()
class HOUDINIENGINE_API UHCsgUtils : public UObject
{
	GENERATED_BODY()
public:

	UHCsgUtils();

	/**
	 * Builds up a model from a set of brushes. Used by RebuildLevel.
	 *
	 * @param Model					The model to be rebuilt.
	 * @param bSelectedBrushesOnly	Use all brushes in the current level or just the selected ones?.
	 * @param bTreatMovableBrushesAsStatic	Treat moveable brushes as static?.
	 */
	static void RebuildModelFromBrushes(UModel* Model, TArray<ABrush*>& Brushes, bool bTreatMovableBrushesAsStatic);

	/**
	 * Converts passed in brushes into a single static mesh actor. 
	 * Note: This replaces all the brushes with a single actor. This actor will not be attached to anything unless a single brush was converted.
	 *
	 * @param	InStaticMeshPackageName		The name to save the brushes to.
	 * @param	InBrushesToConvert			A list of brushes being converted.
	 *
	 * @return							Returns the newly created actor with the newly created static mesh.
	 */
	static UModel* BuildModelFromBrushes(TArray<ABrush*>& Brushes);

	/**
	 * Forked version of UEditorEngine::bspBrushCSG() from UnrealEd/Private/EditorBsp.cpp.
	 * 
	 * Apply the appropriate CSG operation required in order to compose the brush actor onto the given model.
	 *
	 * @param	Actor							The brush actor to apply.
	 * @param	Model							The model to apply the CSG operation to; typically the world's model.
	 * @param	PolyFlags						PolyFlags to set on brush's polys.
	 * @param	BrushType						The type of brush.
	 * @param	CSGOper							The CSG operation to perform.
	 * @param	bBuildBounds					If true, updates bounding volumes on Model for CSG_Add or CSG_Subtract operations.
	 * @param	bMergePolys						If true, coplanar polygons are merged for CSG_Intersect or CSG_Deintersect operations.
	 * @param	bReplaceNULLMaterialRefs		If true, replace NULL material references with a reference to the GB-selected material.
	 * @param	bShowProgressBar				If true, display progress bar for complex brushes
	 * @return									0 if nothing happened, 1 if the operation was error-free, or 1+N if N CSG errors occurred.
	 */
	int ComposeBrushCSG(
		ABrush*		Actor, 
		UModel*		Model, 
		uint32		PolyFlags, 
		EBrushType	BrushType,
		ECsgOper	CSGOper, 
		bool		bBuildBounds,
		bool		bMergePolys,
		bool		bReplaceNULLMaterialRefs,
		bool		bShowProgressBar, /*=true*/
		UHBspPointsGrid* BspPoints,
		UHBspPointsGrid* BspVectors
	);

protected:
	//
	// Status of filtered polygons:
	//
	enum EPolyNodeFilter
	{
		F_OUTSIDE				= 0, // Leaf is an exterior leaf (visible to viewers).
		F_INSIDE				= 1, // Leaf is an interior leaf (non-visible, hidden behind backface).
		F_COPLANAR_OUTSIDE		= 2, // Poly is coplanar and in the exterior (visible to viewers).
		F_COPLANAR_INSIDE		= 3, // Poly is coplanar and inside (invisible to viewers).
		F_COSPATIAL_FACING_IN	= 4, // Poly is coplanar, cospatial, and facing in.
		F_COSPATIAL_FACING_OUT	= 5, // Poly is coplanar, cospatial, and facing out.
	};


	//
	// Information used by FilterEdPoly.
	//
	class FCoplanarInfo
	{
	public:
		int32	iOriginalNode;
		int32   iBackNode;
		int	    BackNodeOutside;
		int	    FrontLeafOutside;
		int     ProcessingBack;
	};

	//
	// Generic filter function called by BspFilterEdPolys.  A and B are pointers
	// to any integers that your specific routine requires (or NULL if not needed).
	//
	typedef void (UHCsgUtils::*BspFilterFunc)
	(
		UModel* Model,
		int32 iNode,
		FPoly* EdPoly,
		EPolyNodeFilter Leaf,
		FHBSPOps::ENodePlace ENodePlace,
		UHBspPointsGrid* BspPoints, 
		UHBspPointsGrid* BspVectors
	);

	//
	// State shared between bspBrushCSG and AddWorldToBrushFunc.  These are very
	// tightly tied into the function AddWorldToBrush, not for general use.
	//
	int32 GDiscarded;		// Number of polys discarded and not added.
	int32 GNode;			// Node AddBrushToWorld is adding to.
	int32 GLastCoplanar;	// Last coplanar beneath GNode at start of AddWorldToBrush.
	int32 GNumNodes;		// Number of Bsp nodes at start of AddWorldToBrush.
	
	UPROPERTY()
	UModel* GModel;			// Level map Model we're adding to.

	UPROPERTY()
	class UModel* TempModel;

	//// Globals removed from FBspPointsGrid
	//UPROPERTY()
	//UHBspPointsGrid* GBspPoints;

	//UPROPERTY()
	//UHBspPointsGrid* GBspVectors;

	/*struct BspFilterOp {
		void Apply(UHCsgUtils* Obj, UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, ENodePlace ENodePlace ) {};
	};*/

	//
	// Handle a piece of a polygon that was filtered to a leaf.
	//
	void FilterLeaf(
		BspFilterFunc FilterFunc, 
		UModel* Model,
		int32 iNode, 
		FPoly* EdPoly, 
		FCoplanarInfo CoplanarInfo, 
		int32 LeafOutside, 
		FHBSPOps::ENodePlace ENodePlace,
		UHBspPointsGrid* BspPoints, 
		UHBspPointsGrid* BspVectors
	);

	//
	// Function to filter an EdPoly through the Bsp, calling a callback
	// function for all chunks that fall into leaves.
	//
	void FilterEdPoly
	(
		BspFilterFunc	FilterFunc, 
		UModel			*Model,
		int32			    iNode, 
		FPoly			*EdPoly, 
		FCoplanarInfo	CoplanarInfo, 
		int32				Outside,
		UHBspPointsGrid* BspPoints, 
		UHBspPointsGrid* BspVectors
	);

	//
	// Regular entry into FilterEdPoly (so higher-level callers don't have to
	// deal with unnecessary info). Filters starting at root.
	//
	void BspFilterFPoly
	( 
		BspFilterFunc FilterFunc, 
		UModel *Model, 
		FPoly *EdPoly, 
		UHBspPointsGrid* BspPoints, 
		UHBspPointsGrid* BspVectors 
	);

	
	int bspNodeToFPoly
	(
		UModel* Model,
		int32 iNode,
		FPoly* EdPoly
	);


	//----------------------------------------------------------------------------
	// World Filtering
	//----------------------------------------------------------------------------

	//
	// Filter all relevant world polys through the brush.
	//
	void FilterWorldThroughBrush
	(
		UModel* Model,
		UModel* Brush,
		EBrushType BrushType,
		ECsgOper CSGOper,
		int32 iNode,
		FSphere* BrushSphere,
		UHBspPointsGrid* BspPoints, 
		UHBspPointsGrid* BspVectors 
	);

	//----------------------------------------------------------------------------
	// CSG leaf filter callbacks / operations.
	// ---------------------------------------------------------------------------
	void AddBrushToWorldFunc( UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void AddWorldToBrushFunc( UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void SubtractBrushFromWorldFunc( UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void SubtractWorldToBrushFunc( UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void IntersectBrushWithWorldFunc( UModel* Model, int32 iNode, FPoly *EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void IntersectWorldWithBrushFunc( UModel *Model, int32 iNode, FPoly *EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void DeIntersectBrushWithWorldFunc( UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );
	void DeIntersectWorldWithBrushFunc( UModel* Model, int32 iNode, FPoly* EdPoly, EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors );


	//----------------------------------------------------------------------------
	// Forked various functions located in: EditorBsp.cpp, EditorCsg.cpp
	//----------------------------------------------------------------------------
	static int TryToMerge( FPoly *Poly1, FPoly *Poly2 );
	static void MergeCoplanars( UModel* Model, int32* PolyList, int32 PolyCount );
	void bspMergeCoplanars( UModel* Model, bool RemapLinks, bool MergeDisparateTextures );
	bool polyFindMaster(UModel* InModel, int32 iSurf, FPoly &Poly);
	static void CleanupNodes( UModel *Model, int32 iNode, int32 iParent );
	void bspCleanup( UModel *Model );

};

