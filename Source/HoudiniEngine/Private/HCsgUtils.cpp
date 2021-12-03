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

#include "HCsgUtils.h"

#include "Engine/Engine.h"
#include "Engine/Polys.h"
#include "Engine/Selection.h"
#include "Materials/Material.h"
#include "Misc/FeedbackContext.h"

#include "ActorEditorUtils.h"
#include "Misc/ScopedSlowTask.h"


DEFINE_LOG_CATEGORY_STATIC(LogHCsgUtils, Log, All);

#if WITH_EDITOR
#include "Editor.h"
#endif

// Magic numbers.
#define THRESH_OPTGEOM_COPLANAR			(0.25)		/* Threshold for Bsp geometry optimization */
#define THRESH_OPTGEOM_COSIDAL			(0.25)		/* Threshold for Bsp geometry optimization */


UHCsgUtils::UHCsgUtils()
{
	// A TempModel is allocated for the HCsgUtils instance to avoid reallocation during inner loops.
	TempModel = NewObject<UModel>();
	TempModel->Initialize(nullptr, 1);

	/*GBspPoints = NewObject<UHBspPointsGrid>();
	GBspVectors = NewObject<UHBspPointsGrid>();*/
}


/*----------------------------------------------------------------------------
   CSG leaf filter callbacks.
----------------------------------------------------------------------------*/

void UHCsgUtils::AddBrushToWorldFunc( UModel* Model, int32 iNode, FPoly* EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
			FHBSPOps::bspAddNode (Model,iNode,ENodePlace,NF_IsNew,EdPoly, BspPoints, BspVectors);
			break;
		case F_COSPATIAL_FACING_OUT:
			if( !(EdPoly->PolyFlags & PF_Semisolid) )
				FHBSPOps::bspAddNode (Model,iNode,ENodePlace,NF_IsNew,EdPoly, BspPoints, BspVectors);
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_IN:
			break;
	}
}

void UHCsgUtils::AddWorldToBrushFunc( UModel* Model, int32 iNode, FPoly* EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
			// Only affect the world poly if it has been cut.
			if( EdPoly->PolyFlags & PF_EdCut )
				FHBSPOps::bspAddNode( GModel, GLastCoplanar, FHBSPOps::NODE_Plane, NF_IsNew, EdPoly, BspPoints, BspVectors );
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_IN:
		case F_COSPATIAL_FACING_OUT:
			// Discard original poly.
			GDiscarded++;
			if( GModel->Nodes[GNode].NumVertices )
			{
				GModel->Nodes[GNode].NumVertices = 0;
			}
			break;
	}
}

void UHCsgUtils::SubtractBrushFromWorldFunc( UModel* Model, int32 iNode, FPoly* EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch (Filter)
	{
		case F_OUTSIDE:
		case F_COSPATIAL_FACING_OUT:
		case F_COSPATIAL_FACING_IN:
		case F_COPLANAR_OUTSIDE:
			break;
		case F_COPLANAR_INSIDE:
		case F_INSIDE:
			EdPoly->Reverse();
			FHBSPOps::bspAddNode (Model,iNode,ENodePlace,NF_IsNew,EdPoly, BspPoints, BspVectors); // Add to Bsp back
			EdPoly->Reverse();
			break;
	}
}

void UHCsgUtils::SubtractWorldToBrushFunc( UModel* Model, int32 iNode, FPoly* EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
		case F_COSPATIAL_FACING_IN:
			// Only affect the world poly if it has been cut.
			if( EdPoly->PolyFlags & PF_EdCut )
				FHBSPOps::bspAddNode( GModel, GLastCoplanar, FHBSPOps::NODE_Plane, NF_IsNew, EdPoly, BspPoints, BspVectors);
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_OUT:
			// Discard original poly.
			GDiscarded++;
			if( GModel->Nodes[GNode].NumVertices )
			{
				GModel->Nodes[GNode].NumVertices = 0;
			}
			break;
	}
}

void UHCsgUtils::IntersectBrushWithWorldFunc( UModel* Model, int32 iNode, FPoly *EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
		case F_COSPATIAL_FACING_IN:
		case F_COSPATIAL_FACING_OUT:
			// Ignore.
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
			if( EdPoly->Fix()>=3 )
				new(GModel->Polys->Element)FPoly(*EdPoly);
			break;
	}
}

void UHCsgUtils::IntersectWorldWithBrushFunc( UModel *Model, int32 iNode, FPoly *EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
		case F_COSPATIAL_FACING_IN:
			// Ignore.
			break;
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_OUT:
			if( EdPoly->Fix() >= 3 )
				new(GModel->Polys->Element)FPoly(*EdPoly);
			break;
	}
}

void UHCsgUtils::DeIntersectBrushWithWorldFunc( UModel* Model, int32 iNode, FPoly* EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_INSIDE:
		case F_COPLANAR_INSIDE:
		case F_COSPATIAL_FACING_OUT:
		case F_COSPATIAL_FACING_IN:
			// Ignore.
			break;
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
			if( EdPoly->Fix()>=3 )
				new(GModel->Polys->Element)FPoly(*EdPoly);
			break;
	}
}

void UHCsgUtils::DeIntersectWorldWithBrushFunc( UModel* Model, int32 iNode, FPoly* EdPoly,
	EPolyNodeFilter Filter, FHBSPOps::ENodePlace ENodePlace, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	switch( Filter )
	{
		case F_OUTSIDE:
		case F_COPLANAR_OUTSIDE:
		case F_COSPATIAL_FACING_OUT:
			// Ignore.
			break;
		case F_COPLANAR_INSIDE:
		case F_INSIDE:
		case F_COSPATIAL_FACING_IN:
			if( EdPoly->Fix() >= 3 )
			{
				EdPoly->Reverse();
				new(GModel->Polys->Element)FPoly(*EdPoly);
				EdPoly->Reverse();
			}
			break;
	}
}

/*----------------------------------------------------------------------------
   CSG polygon filtering routine (calls the callbacks).
----------------------------------------------------------------------------*/

//
// Handle a piece of a polygon that was filtered to a leaf.
//
void UHCsgUtils::FilterLeaf
(
	BspFilterFunc FilterFunc, 
	UModel* Model,
	int32 iNode, 
	FPoly* EdPoly, 
	FCoplanarInfo CoplanarInfo, 
	int32 LeafOutside, 
	FHBSPOps::ENodePlace ENodePlace,
	UHBspPointsGrid* BspPoints, 
	UHBspPointsGrid* BspVectors 
)
{
	EPolyNodeFilter FilterType;

	if( CoplanarInfo.iOriginalNode == INDEX_NONE )
	{
		// Processing regular, non-coplanar polygons.
		FilterType = LeafOutside ? F_OUTSIDE : F_INSIDE;
		(this->*FilterFunc)( Model, iNode, EdPoly, FilterType, ENodePlace, BspPoints, BspVectors );
	}
	else if( CoplanarInfo.ProcessingBack )
	{
		// Finished filtering polygon through tree in back of parent coplanar.
		DoneFilteringBack:
		if      ((!LeafOutside) && (!CoplanarInfo.FrontLeafOutside)) FilterType = F_COPLANAR_INSIDE;
		else if (( LeafOutside) && ( CoplanarInfo.FrontLeafOutside)) FilterType = F_COPLANAR_OUTSIDE;
		else if ((!LeafOutside) && ( CoplanarInfo.FrontLeafOutside)) FilterType = F_COSPATIAL_FACING_OUT;
		else if (( LeafOutside) && (!CoplanarInfo.FrontLeafOutside)) FilterType = F_COSPATIAL_FACING_IN;
		else
		{
			UE_LOG(LogHCsgUtils, Fatal, TEXT("FilterLeaf: Bad Locs"));
			return;
		}
		(this->*FilterFunc)( Model, CoplanarInfo.iOriginalNode, EdPoly, FilterType, FHBSPOps::NODE_Plane, BspPoints, BspVectors );
	}
	else
	{
		CoplanarInfo.FrontLeafOutside = LeafOutside;

		if( CoplanarInfo.iBackNode == INDEX_NONE )
		{
			// Back tree is empty.
			LeafOutside = CoplanarInfo.BackNodeOutside;
			goto DoneFilteringBack;
		}
		else
		{
			// Call FilterEdPoly to filter through the back.  This will result in
			// another call to FilterLeaf with iNode = leaf this falls into in the
			// back tree and EdPoly = the final EdPoly to insert.
			CoplanarInfo.ProcessingBack=1;
			FilterEdPoly( FilterFunc, Model, CoplanarInfo.iBackNode, EdPoly,CoplanarInfo, CoplanarInfo.BackNodeOutside, BspPoints, BspVectors );
		}
	}
}

//
// Filter an EdPoly through the Bsp recursively, calling FilterFunc
// for all chunks that fall into leaves.  FCoplanarInfo is used to
// handle the tricky case of double-recursion for polys that must be
// filtered through a node's front, then filtered through the node's back,
// in order to handle coplanar CSG properly.
//
void UHCsgUtils::FilterEdPoly
(
	BspFilterFunc	FilterFunc, 
	UModel			*Model,
	int32			    iNode, 
	FPoly			*EdPoly, 
	FCoplanarInfo	CoplanarInfo, 
	int32				Outside,
	UHBspPointsGrid* BspPoints, 
	UHBspPointsGrid* BspVectors
)
{
	int32            SplitResult,iOurFront,iOurBack;
	int32			   NewFrontOutside,NewBackOutside;

	FilterLoop:

	// Split em.
	FPoly TempFrontEdPoly,TempBackEdPoly;
	SplitResult = EdPoly->SplitWithPlane
	(
		Model->Points [Model->Verts[Model->Nodes[iNode].iVertPool].pVertex],
		Model->Vectors[Model->Surfs[Model->Nodes[iNode].iSurf].vNormal],
		&TempFrontEdPoly,
		&TempBackEdPoly,
		0
	);

	// Process split results.
	if( SplitResult == SP_Front )
	{
		Front:

		FBspNode *Node = &Model->Nodes[iNode];
		Outside        = Outside || Node->IsCsg();

		if( Node->iFront == INDEX_NONE )
		{
			FilterLeaf(FilterFunc,Model,iNode,EdPoly,CoplanarInfo,Outside,FHBSPOps::NODE_Front, BspPoints, BspVectors);
		}
		else
		{
			iNode = Node->iFront;
			goto FilterLoop;
		}
	}
	else if( SplitResult == SP_Back )
	{
		FBspNode *Node = &Model->Nodes[iNode];
		Outside        = Outside && !Node->IsCsg();

		if( Node->iBack == INDEX_NONE )
		{
			FilterLeaf( FilterFunc, Model, iNode, EdPoly, CoplanarInfo, Outside, FHBSPOps::NODE_Back, BspPoints, BspVectors);
		}
		else
		{
			iNode=Node->iBack;
			goto FilterLoop;
		}
	}
	else if( SplitResult == SP_Coplanar )
	{
		if( CoplanarInfo.iOriginalNode != INDEX_NONE )
		{
			// This will happen once in a blue moon when a polygon is barely outside the
			// coplanar threshold and is split up into a new polygon that is
			// is barely inside the coplanar threshold.  To handle this, just classify
			// it as front and it will be handled propery.
			FHBSPOps::GErrors++;
// 			UE_LOG(LogEditorBsp, Warning, TEXT("FilterEdPoly: Encountered out-of-place coplanar") );
			goto Front;
		}
		CoplanarInfo.iOriginalNode        = iNode;
		CoplanarInfo.iBackNode            = INDEX_NONE;
		CoplanarInfo.ProcessingBack       = 0;
		CoplanarInfo.BackNodeOutside      = Outside;
		NewFrontOutside                   = Outside;

		// See whether Node's iFront or iBack points to the side of the tree on the front
		// of this polygon (will be as expected if this polygon is facing the same
		// way as first coplanar in link, otherwise opposite).
		if( (FVector(Model->Nodes[iNode].Plane) | EdPoly->Normal) >= 0.0 )
		{
			iOurFront = Model->Nodes[iNode].iFront;
			iOurBack  = Model->Nodes[iNode].iBack;
			
			if( Model->Nodes[iNode].IsCsg() )
			{
				CoplanarInfo.BackNodeOutside = 0;
				NewFrontOutside              = 1;
			}
		}
		else
		{
			iOurFront = Model->Nodes[iNode].iBack;
			iOurBack  = Model->Nodes[iNode].iFront;
			
			if( Model->Nodes[iNode].IsCsg() )
			{
				CoplanarInfo.BackNodeOutside = 1; 
				NewFrontOutside              = 0;
			}
		}

		// Process front and back.
		if ((iOurFront==INDEX_NONE)&&(iOurBack==INDEX_NONE))
		{
			// No front or back.
			CoplanarInfo.ProcessingBack		= 1;
			CoplanarInfo.FrontLeafOutside	= NewFrontOutside;
			FilterLeaf
			(
				FilterFunc,
				Model,
				iNode,
				EdPoly,
				CoplanarInfo,
				CoplanarInfo.BackNodeOutside,
				FHBSPOps::NODE_Plane,
				BspPoints,
				BspVectors
			);
		}
		else if( iOurFront==INDEX_NONE && iOurBack!=INDEX_NONE )
		{
			// Back but no front.
			CoplanarInfo.ProcessingBack		= 1;
			CoplanarInfo.iBackNode			= iOurBack;
			CoplanarInfo.FrontLeafOutside	= NewFrontOutside;

			iNode   = iOurBack;
			Outside = CoplanarInfo.BackNodeOutside;
			goto FilterLoop;
		}
		else
		{
			// Has a front and maybe a back.

			// Set iOurBack up to process back on next call to FilterLeaf, and loop
			// to process front.  Next call to FilterLeaf will set FrontLeafOutside.
			CoplanarInfo.ProcessingBack = 0;

			// May be a node or may be INDEX_NONE.
			CoplanarInfo.iBackNode = iOurBack;

			iNode   = iOurFront;
			Outside = NewFrontOutside;
			goto FilterLoop;
		}
	}
	else if( SplitResult == SP_Split )
	{
		// Front half of split.
		if( Model->Nodes[iNode].IsCsg() )
		{
			NewFrontOutside = 1; 
			NewBackOutside  = 0;
		}
		else
		{
			NewFrontOutside = Outside;
			NewBackOutside  = Outside;
		}

		if( Model->Nodes[iNode].iFront==INDEX_NONE )
		{
			FilterLeaf
			(
				FilterFunc,
				Model,
				iNode,
				&TempFrontEdPoly,
				CoplanarInfo,
				NewFrontOutside,
				FHBSPOps::NODE_Front,
				BspPoints,
				BspVectors
			);
		}
		else
		{
			FilterEdPoly
			(
				FilterFunc,
				Model,
				Model->Nodes[iNode].iFront,
				&TempFrontEdPoly,
				CoplanarInfo,
				NewFrontOutside,
				BspPoints,
				BspVectors
			);
		}

		// Back half of split.
		if( Model->Nodes[iNode].iBack==INDEX_NONE )
		{
			FilterLeaf
			(
				FilterFunc,
				Model,
				iNode,
				&TempBackEdPoly,
				CoplanarInfo,
				NewBackOutside,
				FHBSPOps::NODE_Back,
				BspPoints,
				BspVectors
			);
		}
		else
		{
			FilterEdPoly
			(
				FilterFunc,
				Model,
				Model->Nodes[iNode].iBack,
				&TempBackEdPoly,
				CoplanarInfo,
				NewBackOutside,
				BspPoints,
				BspVectors
			);
		}
	}
}

//
// Regular entry into FilterEdPoly (so higher-level callers don't have to
// deal with unnecessary info). Filters starting at root.
//
void UHCsgUtils::BspFilterFPoly( BspFilterFunc FilterFunc, UModel *Model, FPoly *EdPoly, UHBspPointsGrid* BspPoints, UHBspPointsGrid* BspVectors )
{
	FCoplanarInfo StartingCoplanarInfo;
	StartingCoplanarInfo.iOriginalNode = INDEX_NONE;
	if( Model->Nodes.Num() == 0 )
	{
		// If Bsp is empty, process at root.
		(this->*FilterFunc)( Model, 0, EdPoly, Model->RootOutside ? F_OUTSIDE : F_INSIDE, FHBSPOps::NODE_Root, BspPoints, BspVectors );
	}
	else
	{
		// Filter through Bsp.
		FilterEdPoly( FilterFunc, Model, 0, EdPoly, StartingCoplanarInfo, Model->RootOutside, BspPoints, BspVectors );
	}
}

int UHCsgUtils::bspNodeToFPoly
(
	UModel* Model,
	int32 iNode,
	FPoly* EdPoly
)
{
	FPoly MasterEdPoly;

	FBspNode &Node     	= Model->Nodes[iNode];
	FBspSurf &Poly     	= Model->Surfs[Node.iSurf];
	FVert	 *VertPool	= &Model->Verts[ Node.iVertPool ];

	EdPoly->Base		= Model->Points [Poly.pBase];
	EdPoly->Normal      = Model->Vectors[Poly.vNormal];

	EdPoly->PolyFlags 	= Poly.PolyFlags & ~(PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized);
	EdPoly->iLinkSurf   = Node.iSurf;
	EdPoly->Material    = Poly.Material;

	EdPoly->Actor    	= Poly.Actor;
	EdPoly->iBrushPoly  = Poly.iBrushPoly;
	
	if( polyFindMaster(Model,Node.iSurf,MasterEdPoly) )
		EdPoly->ItemName  = MasterEdPoly.ItemName;
	else
		EdPoly->ItemName  = NAME_None;

	EdPoly->TextureU = Model->Vectors[Poly.vTextureU];
	EdPoly->TextureV = Model->Vectors[Poly.vTextureV];

	EdPoly->LightMapScale = Poly.LightMapScale;

	EdPoly->LightmassSettings = Model->LightmassSettings[Poly.iLightmassIndex];

	EdPoly->Vertices.Empty();

	for(int32 VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
	{
		new(EdPoly->Vertices) FVector(Model->Points[VertPool[VertexIndex].pVertex]);
	}

	if(EdPoly->Vertices.Num() < 3)
	{
		EdPoly->Vertices.Empty();
	}
	else
	{
		// Remove colinear points and identical points (which will appear
		// if T-joints were eliminated).
		EdPoly->RemoveColinears();
	}

	return EdPoly->Vertices.Num();
}

/*---------------------------------------------------------------------------------------
   World filtering.
---------------------------------------------------------------------------------------*/

//
// Filter all relevant world polys through the brush.
//
void UHCsgUtils::FilterWorldThroughBrush 
(
	UModel*		Model,
	UModel*		Brush,
	EBrushType	BrushType,
	ECsgOper	CSGOper,
	int32		    iNode,
	FSphere*	BrushSphere,
	UHBspPointsGrid* BspPoints, 
	UHBspPointsGrid* BspVectors 
)
{
	// Loop through all coplanars.
	while( iNode != INDEX_NONE )
	{
		// Get surface.
		int32 iSurf = Model->Nodes[iNode].iSurf;

		// Skip new nodes and their children, which are guaranteed new.
		if( Model->Nodes[iNode].NodeFlags & NF_IsNew )
			return;

		// Sphere reject.
		int DoFront = 1, DoBack = 1;
		if( BrushSphere )
		{
			float Dist = Model->Nodes[iNode].Plane.PlaneDot( BrushSphere->Center );
			DoFront    = (Dist >= -BrushSphere->W);
			DoBack     = (Dist <= +BrushSphere->W);
		}

		// Process only polys that aren't empty.
		FPoly TempEdPoly;
		if( DoFront && DoBack && (GEditor->bspNodeToFPoly(Model,iNode,&TempEdPoly)>0) )
		{
			TempEdPoly.Actor      = Model->Surfs[iSurf].Actor;
			TempEdPoly.iBrushPoly = Model->Surfs[iSurf].iBrushPoly;

			if( BrushType==Brush_Add || BrushType==Brush_Subtract )
			{
				// Add and subtract work the same in this step.
				GNode       = iNode;
				GModel  	= Model;
				GDiscarded  = 0;
				GNumNodes	= Model->Nodes.Num();

				// Find last coplanar in chain.
				GLastCoplanar = iNode;
				while( Model->Nodes[GLastCoplanar].iPlane != INDEX_NONE )
					GLastCoplanar = Model->Nodes[GLastCoplanar].iPlane;

				// Do the filter operation.
				BspFilterFPoly
				(
					BrushType==Brush_Add ? &UHCsgUtils::AddWorldToBrushFunc : &UHCsgUtils::SubtractWorldToBrushFunc,
					Brush,
					&TempEdPoly,
					BspPoints,
					BspVectors
				);
				
				if( GDiscarded == 0 )
				{
					// Get rid of all the fragments we added.
					Model->Nodes[GLastCoplanar].iPlane = INDEX_NONE;
					const bool bAllowShrinking = false;
					Model->Nodes.RemoveAt( GNumNodes, Model->Nodes.Num()-GNumNodes, bAllowShrinking );
				}
				else
				{
					// Tag original world poly for deletion; has been deleted or replaced by partial fragments.
					if( GModel->Nodes[GNode].NumVertices )
					{
						GModel->Nodes[GNode].NumVertices = 0;
					}
				}
			}
			else if( CSGOper == CSG_Intersect )
			{
				BspFilterFPoly( &UHCsgUtils::IntersectWorldWithBrushFunc, Brush, &TempEdPoly, BspPoints, BspVectors );
			}
			else if( CSGOper == CSG_Deintersect )
			{
				BspFilterFPoly( &UHCsgUtils::DeIntersectWorldWithBrushFunc, Brush, &TempEdPoly, BspPoints, BspVectors );
			}
		}

		// Now recurse to filter all of the world's children nodes.
		if( DoFront && (Model->Nodes[iNode].iFront != INDEX_NONE)) FilterWorldThroughBrush
		(
			Model,
			Brush,
			BrushType,
			CSGOper,
			Model->Nodes[iNode].iFront,
			BrushSphere,
			BspPoints,
			BspVectors
		);
		if( DoBack && (Model->Nodes[iNode].iBack != INDEX_NONE) ) FilterWorldThroughBrush
		(
			Model,
			Brush,
			BrushType,
			CSGOper,
			Model->Nodes[iNode].iBack,
			BrushSphere,
			BspPoints,
			BspVectors
		);
		iNode = Model->Nodes[iNode].iPlane;
	}
}

void UHCsgUtils::RebuildModelFromBrushes(UModel* Model, TArray<ABrush*>& Brushes, bool bTreatMovableBrushesAsStatic)
{
	if (!IsValid(Model))
		return;

	UHCsgUtils* CsgUtils = NewObject<UHCsgUtils>();
	int32 CsgErrors = 0;

	UHBspPointsGrid* BspPoints = UHBspPointsGrid::Create(50.0f, THRESH_POINTS_ARE_SAME);
	UHBspPointsGrid* BspVectors = UHBspPointsGrid::Create(1/16.0f, FMath::Max(THRESH_NORMALS_ARE_SAME, THRESH_VECTORS_ARE_NEAR));

	// Empty the model out.
	const int32 NumPoints = Model->Points.Num();
	const int32 NumNodes = Model->Nodes.Num();
	const int32 NumVerts = Model->Verts.Num();
	const int32 NumVectors = Model->Vectors.Num();
	const int32 NumSurfs = Model->Surfs.Num();

	Model->Modify();
	Model->EmptyModel(1, 1);

	// Reserve arrays an eighth bigger than the previous allocation
	Model->Points.Empty(NumPoints + NumPoints / 8);
	Model->Nodes.Empty(NumNodes + NumNodes / 8);
	Model->Verts.Empty(NumVerts + NumVerts / 8);
	Model->Vectors.Empty(NumVectors + NumVectors / 8);
	Model->Surfs.Empty(NumSurfs + NumSurfs / 8);

	// Build list of all static brushes, first structural brushes and portals
	TArray<ABrush*> StaticBrushes;
	for (ABrush* Brush : Brushes)
	{
		if ((Brush && (Brush->IsStaticBrush() || bTreatMovableBrushesAsStatic) && !FActorEditorUtils::IsABuilderBrush(Brush)) &&
			(!(Brush->PolyFlags & PF_Semisolid) || (Brush->BrushType != Brush_Add) || (Brush->PolyFlags & PF_Portal)))
		{
			StaticBrushes.Add(Brush);

			// Treat portals as solids for cutting.
			if (Brush->PolyFlags & PF_Portal)
			{
				Brush->PolyFlags = (Brush->PolyFlags & ~PF_Semisolid) | PF_NotSolid;
			}
		}
	}

	// Next append all detail brushes
	for (ABrush* Brush : Brushes)
	{
		if (Brush && Brush->IsStaticBrush() && !FActorEditorUtils::IsABuilderBrush(Brush) &&
			(Brush->PolyFlags & PF_Semisolid) && !(Brush->PolyFlags & PF_Portal) && (Brush->BrushType == Brush_Add))
		{
			StaticBrushes.Add(Brush);
		}
	}

	// Build list of dynamic brushes
	TArray<ABrush*> DynamicBrushes;
	if (!bTreatMovableBrushesAsStatic)
	{
		for (ABrush* DynamicBrush : Brushes)
		{
			if (DynamicBrush && DynamicBrush->Brush && !DynamicBrush->IsStaticBrush())
			{
				DynamicBrushes.Add(DynamicBrush);
			}
		}
	}

	FScopedSlowTask SlowTask(StaticBrushes.Num() + DynamicBrushes.Num());
	SlowTask.MakeDialogDelayed(3.0f);

	// Compose all static brushes
	for (ABrush* Brush : StaticBrushes)
	{
		SlowTask.EnterProgressFrame(1);
		Brush->Modify();
		int32 Errors = CsgUtils->ComposeBrushCSG(Brush, Model, Brush->PolyFlags, (EBrushType)Brush->BrushType, CSG_None, false, true, false, false, BspPoints, BspVectors);
		if (Errors > 1)
			CsgErrors += Errors - 1;
	}

	// Rebuild dynamic brush BSP's (if they weren't handled earlier)
	for (ABrush* DynamicBrush : DynamicBrushes)
	{
		SlowTask.EnterProgressFrame(1);
		UHBspPointsGrid* LocalBspPoints = UHBspPointsGrid::Create(50.0f, THRESH_POINTS_ARE_SAME);
		UHBspPointsGrid* LocalBspVectors = UHBspPointsGrid::Create(1 / 16.0f, FMath::Max(THRESH_NORMALS_ARE_SAME, THRESH_VECTORS_ARE_NEAR));

		FHBSPOps::csgPrepMovingBrush(DynamicBrush, LocalBspPoints, LocalBspVectors);
	}
}



UModel* UHCsgUtils::BuildModelFromBrushes(TArray<ABrush*>& Brushes)
{
	// Generally UModels are initialized using ABrush. Here we manually
	// initialize using relevant parts from
	UModel* OutModel = NewObject<UModel>();
	OutModel->SetFlags(RF_Transactional);
	OutModel->RootOutside = true;
	OutModel->EmptyModel(1,1);
	OutModel->UpdateVertices();

	if (!IsValid(OutModel))
		return nullptr;

	// Can we combine the brushes without modifying the actors here ...?

	//FVector Location(0.0f, 0.0f, 0.0f);
	//FRotator Rotation(0.0f, 0.0f, 0.0f);
	//for(int32 BrushesIdx = 0; BrushesIdx < Brushes.Num(); ++BrushesIdx )
	//{
	//	// Cache the location and rotation.
	//	Location = Brushes[BrushesIdx]->GetActorLocation();
	//	Rotation = Brushes[BrushesIdx]->GetActorRotation();


	//	// Leave the actor's rotation but move it to origin so the Static Mesh will generate correctly.
	//	Brushes[BrushesIdx]->TeleportTo(Location - InPivotLocation, Rotation, false, true);
	//}

	RebuildModelFromBrushes(OutModel, Brushes, true);
	//GEditor->bspBuildFPolys(OutModel, true, 0);

	//if (0 < ConversionTempModel->Polys->Element.Num())
	//{
	//	UStaticMesh* NewMesh = CreateStaticMeshFromBrush(Pkg, ObjName, NULL, ConversionTempModel);
	//	NewActor = FActorFactoryAssetProxy::AddActorForAsset( NewMesh );

	//	NewActor->Modify();

	//	NewActor->InvalidateLightingCache();
	//	NewActor->PostEditChange();
	//	NewActor->PostEditMove( true );
	//	NewActor->Modify();
	//	ULayersSubsystem* LayersSubsystem = GetEditorSubsystem<ULayersSubsystem>();
	//	LayersSubsystem->InitializeNewActorLayers(NewActor);

	//	// Teleport the new actor to the old location but not the old rotation. The static mesh is built to the rotation already.
	//	NewActor->TeleportTo(InPivotLocation, FRotator(0.0f, 0.0f, 0.0f), false, true);

	//	// Destroy the old brushes.
	//	for( int32 BrushIdx = 0; BrushIdx < InBrushesToConvert.Num(); ++BrushIdx )
	//	{
	//		LayersSubsystem->DisassociateActorFromLayers(InBrushesToConvert[BrushIdx]);
	//		GWorld->EditorDestroyActor( InBrushesToConvert[BrushIdx], true );
	//	}

	//	// Notify the asset registry
	//	FAssetRegistryModule::AssetCreated(NewMesh);
	//}

	//ConversionTempModel->EmptyModel(1, 1);
	//RebuildAlteredBSP();
	//RedrawLevelEditingViewports();

	//return NewActor;

	return OutModel;
}

int UHCsgUtils::ComposeBrushCSG
(
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
)
{
	uint32 NotPolyFlags = 0;
	int32 NumPolysFromBrush=0,i,j,ReallyBig;
	UModel* Brush = Actor->Brush;
	int32 Errors = 0;

	// Make sure we're in an acceptable state.
	if( !Brush )
	{
		return 0;
	}

	// Non-solid and semisolid stuff can only be added.
	if( BrushType != Brush_Add )
	{
		NotPolyFlags |= (PF_Semisolid | PF_NotSolid);
	}

	TempModel->EmptyModel(1,1);

	// Update status.
	ReallyBig = (Brush->Polys->Element.Num() > 200) && bShowProgressBar;
	if( ReallyBig )
	{
		FText Description = NSLOCTEXT("UnrealEd", "PerformingCSGOperation", "Performing CSG operation");
		
		if (BrushType != Brush_MAX)
		{	
			if (BrushType == Brush_Add)
			{
				Description = NSLOCTEXT("UnrealEd", "AddingBrushToWorld", "Adding brush to world");
			}
			else if (BrushType == Brush_Subtract)
			{
				Description = NSLOCTEXT("UnrealEd", "SubtractingBrushFromWorld", "Subtracting brush from world");
			}
		}
		else if (CSGOper != CSG_None)
		{
			if (CSGOper == CSG_Intersect)
			{
				Description = NSLOCTEXT("UnrealEd", "IntersectingBrushWithWorld", "Intersecting brush with world");
			}
			else if (CSGOper == CSG_Deintersect)
			{
				Description = NSLOCTEXT("UnrealEd", "DeintersectingBrushWithWorld", "Deintersecting brush with world");
			}
		}

		GWarn->BeginSlowTask( Description, true );
		// Transform original brush poly into same coordinate system as world
		// so Bsp filtering operations make sense.
		GWarn->StatusUpdate(0, 0, NSLOCTEXT("UnrealEd", "Transforming", "Transforming"));
	}


	//UMaterialInterface* SelectedMaterialInstance = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
	UMaterialInterface* SelectedMaterialInstance = nullptr;

	const FVector Scale = Actor->GetActorScale();
	const FRotator Rotation = Actor->GetActorRotation();
	const FVector Location = Actor->GetActorLocation();

	const bool bIsMirrored = (Scale.X * Scale.Y * Scale.Z < 0.0f);

	// Cache actor transform which is used for the geometry being built
	Brush->OwnerLocationWhenLastBuilt = Location;
	Brush->OwnerRotationWhenLastBuilt = Rotation;
	Brush->OwnerScaleWhenLastBuilt = Scale;
	Brush->bCachedOwnerTransformValid = true;

	for( i=0; i<Brush->Polys->Element.Num(); i++ )
	{
		FPoly& CurrentPoly = Brush->Polys->Element[i];

		// Set texture the first time.
		if ( bReplaceNULLMaterialRefs )
		{
			UMaterialInterface*& PolyMat = CurrentPoly.Material;
			if ( !PolyMat || PolyMat == UMaterial::GetDefaultMaterial(MD_Surface) )
			{
				PolyMat = SelectedMaterialInstance;
			}
		}

		// Get the brush poly.
		FPoly DestEdPoly = CurrentPoly;
		check(CurrentPoly.iLink<Brush->Polys->Element.Num());

		// Set its backward brush link.
		DestEdPoly.Actor       = Actor;
		DestEdPoly.iBrushPoly  = i;

		// Update its flags.
		DestEdPoly.PolyFlags = (DestEdPoly.PolyFlags | PolyFlags) & ~NotPolyFlags;

		// Set its internal link.
		if (DestEdPoly.iLink == INDEX_NONE)
		{
			DestEdPoly.iLink = i;
		}

		// Transform it.
		DestEdPoly.Scale( Scale );
		DestEdPoly.Rotate(FRotator3f(Rotation));
		DestEdPoly.Transform( Location );

		// Reverse winding and normal if the parent brush is mirrored
		if (bIsMirrored)
		{
			DestEdPoly.Reverse();
			DestEdPoly.CalcNormal();
		}

		// Add poly to the temp model.
		new(TempModel->Polys->Element)FPoly( DestEdPoly );
	}
	if( ReallyBig ) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "FilteringBrush", "Filtering brush") );

	// Pass the brush polys through the world Bsp.
	if( CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect )
	{
		// Empty the brush.
		Brush->EmptyModel(1,1);

		// Intersect and deintersect.
		for( i=0; i<TempModel->Polys->Element.Num(); i++ )
		{
			FPoly EdPoly = TempModel->Polys->Element[i];
			GModel = Brush;
			// TODO: iLink / iLinkSurf in EdPoly / TempModel->Polys->Element[i] ?
			BspFilterFPoly( CSGOper==CSG_Intersect ? &UHCsgUtils::IntersectBrushWithWorldFunc : &UHCsgUtils::DeIntersectBrushWithWorldFunc, Model,  &EdPoly, BspPoints, BspVectors );
		}
		NumPolysFromBrush = Brush->Polys->Element.Num();
	}
	else
	{
		// Add and subtract.
		TMap<int32, int32> SurfaceIndexRemap;
		for( i=0; i<TempModel->Polys->Element.Num(); i++ )
		{
			FPoly EdPoly = TempModel->Polys->Element[i];

			// Mark the polygon as non-cut so that it won't be harmed unless it must
			// be split, and set iLink so that BspAddNode will know to add its information
			// if a node is added based on this poly.
			EdPoly.PolyFlags &= ~(PF_EdCut);
			const int32* SurfaceIndexPtr = SurfaceIndexRemap.Find(EdPoly.iLink);
			if (SurfaceIndexPtr == nullptr)
			{
				const int32 NewSurfaceIndex = Model->Surfs.Num();
				SurfaceIndexRemap.Add(EdPoly.iLink, NewSurfaceIndex);
				EdPoly.iLinkSurf = TempModel->Polys->Element[i].iLinkSurf = NewSurfaceIndex;
			}
			else
			{
				EdPoly.iLinkSurf = TempModel->Polys->Element[i].iLinkSurf = *SurfaceIndexPtr;
			}

			// Filter brush through the world.
			BspFilterFPoly( BrushType==Brush_Add ? &UHCsgUtils::AddBrushToWorldFunc : &UHCsgUtils::SubtractBrushFromWorldFunc, Model, &EdPoly, BspPoints, BspVectors );
		}
	}
	if( Model->Nodes.Num() && !(PolyFlags & (PF_NotSolid | PF_Semisolid)) )
	{
		// Quickly build a Bsp for the brush, tending to minimize splits rather than balance
		// the tree.  We only need the cutting planes, though the entire Bsp struct (polys and
		// all) is built.

		/*FHBspPointsGrid* LevelModelPointsGrid = FHBspPointsGrid::GBspPoints;
		FHBspPointsGrid* LevelModelVectorsGrid = FHBspPointsGrid::GBspVectors;*/

		// For the bspBuild call, temporarily create a new pair of BspPointsGrids for the TempModel.
		UHBspPointsGrid* TempBspPoints = UHBspPointsGrid::Create(50.0f, THRESH_POINTS_ARE_SAME);
		UHBspPointsGrid* TempBspVectors = UHBspPointsGrid::Create(1 / 16.0f, FMath::Max(THRESH_NORMALS_ARE_SAME, THRESH_VECTORS_ARE_NEAR));
		/*FHBspPointsGrid::GBspPoints = BspPoints.Get();
		FHBspPointsGrid::GBspVectors = BspVectors.Get();*/

		if( ReallyBig ) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "BuildingBSP", "Building BSP") );
		
		FHBSPOps::bspBuild( TempModel, FHBSPOps::BSP_Lame, 0, 70, 1, 0, TempBspPoints, TempBspVectors );

		// Reinstate the original BspPointsGrids used for building the level Model.
		/*FHBspPointsGrid::GBspPoints = LevelModelPointsGrid;
		FHBspPointsGrid::GBspVectors = LevelModelVectorsGrid;*/

		if( ReallyBig ) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "FilteringWorld", "Filtering world") );
		GModel = Brush;
		TempModel->BuildBound();

		FSphere	BrushSphere = TempModel->Bounds.GetSphere();
		FilterWorldThroughBrush( Model, TempModel, BrushType, CSGOper, 0, &BrushSphere, BspPoints, BspVectors);
	}
	if( CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect )
	{
		if( ReallyBig ) GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "AdjustingBrush", "Adjusting brush") );

		// Link polys obtained from the original brush.
		for( i=NumPolysFromBrush-1; i>=0; i-- )
		{
			FPoly *DestEdPoly = &Brush->Polys->Element[i];
			for( j=0; j<i; j++ )
			{
				if( DestEdPoly->iLink == Brush->Polys->Element[j].iLink )
				{
					DestEdPoly->iLink = j;
					break;
				}
			}
			if( j >= i ) DestEdPoly->iLink = i;
		}

		// Link polys obtained from the world.
		for( i=Brush->Polys->Element.Num()-1; i>=NumPolysFromBrush; i-- )
		{
			FPoly *DestEdPoly = &Brush->Polys->Element[i];
			for( j=NumPolysFromBrush; j<i; j++ )
			{
				if( DestEdPoly->iLink == Brush->Polys->Element[j].iLink )
				{
					DestEdPoly->iLink = j;
					break;
				}
			}
			if( j >= i ) DestEdPoly->iLink = i;
		}
		Brush->Linked = 1;

		// Detransform the obtained brush back into its original coordinate system.
		for( i=0; i<Brush->Polys->Element.Num(); i++ )
		{
			FPoly *DestEdPoly = &Brush->Polys->Element[i];
			DestEdPoly->Transform(-Location);
			DestEdPoly->Rotate(FRotator3f(Rotation.GetInverse()));
			DestEdPoly->Scale(FVector(1.0f) / Scale);
			DestEdPoly->Fix();
			DestEdPoly->Actor		= NULL;
			DestEdPoly->iBrushPoly	= i;
		}
	}

	if( BrushType==Brush_Add || BrushType==Brush_Subtract )
	{
		// Clean up nodes, reset node flags.
		bspCleanup( Model );

		// Rebuild bounding volumes.
		if( bBuildBounds )
		{
			FHBSPOps::bspBuildBounds( Model );
		}
	}

	Brush->NumUniqueVertices = TempModel->Points.Num();
	// Release TempModel.
	TempModel->EmptyModel(1,1);
	
	// Merge coplanars if needed.
	if( CSGOper==CSG_Intersect || CSGOper==CSG_Deintersect )
	{
		if( ReallyBig )
		{
			GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "Merging", "Merging") );
		}
		if( bMergePolys )
		{
			bspMergeCoplanars( Brush, 1, 0 );
		}
	}
	if( ReallyBig )
	{
		GWarn->EndSlowTask();
	}

	return 1 + FHBSPOps::GErrors;
}

/*----------------------------------------------------------------------------
   EdPoly building and compacting.
----------------------------------------------------------------------------*/

//
// Trys to merge two polygons.  If they can be merged, replaces Poly1 and emptys Poly2
// and returns 1.  Otherwise, returns 0.
//
int UHCsgUtils::TryToMerge( FPoly *Poly1, FPoly *Poly2 )
{
	// Find one overlapping point.
	int32 Start1=0, Start2=0;
	for( Start1=0; Start1<Poly1->Vertices.Num(); Start1++ )
		for( Start2=0; Start2<Poly2->Vertices.Num(); Start2++ )
			if( FVector::PointsAreSame(Poly1->Vertices[Start1], Poly2->Vertices[Start2]) )
				goto FoundOverlap;
	return 0;
	FoundOverlap:

	// Wrap around trying to merge.
	int32 End1  = Start1;
	int32 End2  = Start2;
	int32 Test1 = Start1+1; if (Test1>=Poly1->Vertices.Num()) Test1 = 0;
	int32 Test2 = Start2-1; if (Test2<0)                   Test2 = Poly2->Vertices.Num()-1;
	if( FVector::PointsAreSame(Poly1->Vertices[Test1],Poly2->Vertices[Test2]) )
	{
		End1   = Test1;
		Start2 = Test2;
	}
	else
	{
		Test1 = Start1-1; if (Test1<0)                   Test1=Poly1->Vertices.Num()-1;
		Test2 = Start2+1; if (Test2>=Poly2->Vertices.Num()) Test2=0;
		if( FVector::PointsAreSame(Poly1->Vertices[Test1],Poly2->Vertices[Test2]) )
		{
			Start1 = Test1;
			End2   = Test2;
		}
		else return 0;
	}

	// Build a new edpoly containing both polygons merged.
	FPoly NewPoly = *Poly1;
	NewPoly.Vertices.Empty();
	int32 Vertex = End1;
	for( int32 i=0; i<Poly1->Vertices.Num(); i++ )
	{
		new(NewPoly.Vertices) FVector(Poly1->Vertices[Vertex]);
		if( ++Vertex >= Poly1->Vertices.Num() )
			Vertex=0;
	}
	Vertex = End2;
	for( int32 i=0; i<(Poly2->Vertices.Num()-2); i++ )
	{
		if( ++Vertex >= Poly2->Vertices.Num() )
			Vertex=0;
		new(NewPoly.Vertices) FVector(Poly2->Vertices[Vertex]);
	}

	// Remove colinear vertices and check convexity.
	if( NewPoly.RemoveColinears() )
	{
		*Poly1 = NewPoly;
		Poly2->Vertices.Empty();
		return true;
	}
	else return 0;
}

//
// Merge all polygons in coplanar list that can be merged convexly.
//
void UHCsgUtils::MergeCoplanars( UModel* Model, int32* PolyList, int32 PolyCount )
{
	int32 MergeAgain = 1;
	while( MergeAgain )
	{
		MergeAgain = 0;
		for( int32 i=0; i<PolyCount; i++ )
		{
			FPoly& Poly1 = Model->Polys->Element[PolyList[i]];
			if( Poly1.Vertices.Num() > 0 )
			{
				for( int32 j=i+1; j<PolyCount; j++ )
				{
					FPoly& Poly2 = Model->Polys->Element[PolyList[j]];
					if( Poly2.Vertices.Num() > 0 )
					{
						if( TryToMerge( &Poly1, &Poly2 ) )
							MergeAgain=1;
					}
				}
			}
		}
	}
}

void UHCsgUtils::bspMergeCoplanars( UModel* Model, bool RemapLinks, bool MergeDisparateTextures )
{
	int32 OriginalNum = Model->Polys->Element.Num();

	// Mark all polys as unprocessed.
	for( int32 i=0; i<Model->Polys->Element.Num(); i++ )
		Model->Polys->Element[i].PolyFlags &= ~PF_EdProcessed;

	// Find matching coplanars and merge them.
	FMemMark Mark(FMemStack::Get());
	int32* PolyList = new(FMemStack::Get(),Model->Polys->Element.Num())int32;
	int32 n=0;
	for( int32 i=0; i<Model->Polys->Element.Num(); i++ )
	{
		FPoly* EdPoly = &Model->Polys->Element[i];
		if( EdPoly->Vertices.Num()>0 && !(EdPoly->PolyFlags & PF_EdProcessed) )
		{
			int32 PolyCount         =  0;
			PolyList[PolyCount++] =  i;
			EdPoly->PolyFlags    |= PF_EdProcessed;
			for( int32 j=i+1; j<Model->Polys->Element.Num(); j++ )
			{
				FPoly* OtherPoly = &Model->Polys->Element[j];
				if( OtherPoly->iLink == EdPoly->iLink && OtherPoly->Vertices.Num() )
				{
					float Dist = (OtherPoly->Vertices[0] - EdPoly->Vertices[0]) | EdPoly->Normal;
					if
					(	Dist>-0.001
					&&	Dist<0.001
					&&	(OtherPoly->Normal|EdPoly->Normal)>0.9999
					&&	(MergeDisparateTextures
						||	(	FVector::PointsAreNear(OtherPoly->TextureU,EdPoly->TextureU,THRESH_VECTORS_ARE_NEAR)
							&&	FVector::PointsAreNear(OtherPoly->TextureV,EdPoly->TextureV,THRESH_VECTORS_ARE_NEAR) ) ) )
					{
						OtherPoly->PolyFlags |= PF_EdProcessed;
						PolyList[PolyCount++] = j;
					}
				}
			}
			if( PolyCount > 1 )
			{
				MergeCoplanars( Model, PolyList, PolyCount );
				n++;
			}
		}
	}
//	UE_LOG(LogEditorBsp, Log,  TEXT("Found %i coplanar sets in %i"), n, Model->Polys->Element.Num() );
	Mark.Pop();

	// Get rid of empty EdPolys while remapping iLinks.
	FMemMark Mark2(FMemStack::Get());
	int32 j=0;
	int32* Remap = new(FMemStack::Get(),Model->Polys->Element.Num())int32;
	for( int32 i=0; i<Model->Polys->Element.Num(); i++ )
	{
		if( Model->Polys->Element[i].Vertices.Num() )
		{
			Remap[i] = j;
			Model->Polys->Element[j] = Model->Polys->Element[i];
			j++;
		}
	}
	Model->Polys->Element.RemoveAt( j, Model->Polys->Element.Num()-j );
	if( RemapLinks )
	{
		for( int32 i=0; i<Model->Polys->Element.Num(); i++ )
		{
			if (Model->Polys->Element[i].iLink != INDEX_NONE)
			{
				CA_SUPPRESS(6001); // warning C6001: Using uninitialized memory '*Remap'.
				Model->Polys->Element[i].iLink = Remap[Model->Polys->Element[i].iLink];
			}
		}
	}
// 	UE_LOG(LogEditorBsp, Log,  TEXT("BspMergeCoplanars reduced %i->%i"), OriginalNum, Model->Polys->Element.Num() );
	Mark2.Pop();
}

bool UHCsgUtils::polyFindMaster(UModel* InModel, int32 iSurf, FPoly &Poly)
{
	FBspSurf &Surf = InModel->Surfs[iSurf];
	if( !Surf.Actor || !Surf.Actor->Brush->Polys->Element.IsValidIndex(Surf.iBrushPoly) )
	{
		return false;
	}
	else
	{
		Poly = Surf.Actor->Brush->Polys->Element[Surf.iBrushPoly];
		return true;
	}
}

void UHCsgUtils::CleanupNodes( UModel *Model, int32 iNode, int32 iParent )
{
	FBspNode *Node = &Model->Nodes[iNode];

	// Transactionally empty vertices of tag-for-empty nodes.
	Node->NodeFlags &= ~(NF_IsNew | NF_IsFront | NF_IsBack);

	// Recursively clean up front, back, and plane nodes.
	if( Node->iFront != INDEX_NONE ) CleanupNodes( Model, Node->iFront, iNode );
	if( Node->iBack  != INDEX_NONE ) CleanupNodes( Model, Node->iBack , iNode );
	if( Node->iPlane != INDEX_NONE ) CleanupNodes( Model, Node->iPlane, iNode );

	// Reload Node since the recusive call aliases it.
	Node = &Model->Nodes[iNode];

	// If this is an empty node with a coplanar, replace it with the coplanar.
	if( Node->NumVertices==0 && Node->iPlane!=INDEX_NONE )
	{
		FBspNode* PlaneNode = &Model->Nodes[ Node->iPlane ];

		// Stick our front, back, and parent nodes on the coplanar.
		if( (Node->Plane | PlaneNode->Plane) >= 0.0 )
		{
			PlaneNode->iFront  = Node->iFront;
			PlaneNode->iBack   = Node->iBack;
		}
		else
		{
			PlaneNode->iFront  = Node->iBack;
			PlaneNode->iBack   = Node->iFront;
		}

		if( iParent == INDEX_NONE )
		{
			// This node is the root.
			*Node                  = *PlaneNode;   // Replace root.
			PlaneNode->NumVertices = 0;            // Mark as unused.
		}
		else
		{
			// This is a child node.
			FBspNode *ParentNode = &Model->Nodes[iParent];

			if      ( ParentNode->iFront == iNode ) ParentNode->iFront = Node->iPlane;
			else if ( ParentNode->iBack  == iNode ) ParentNode->iBack  = Node->iPlane;
			else if ( ParentNode->iPlane == iNode ) ParentNode->iPlane = Node->iPlane;
			else UE_LOG(LogHCsgUtils, Fatal, TEXT("CleanupNodes: Parent and child are unlinked"));
		}
	}
	else if( Node->NumVertices == 0 && ( Node->iFront==INDEX_NONE || Node->iBack==INDEX_NONE ) )
	{
		// Delete empty nodes with no fronts or backs.
		// Replace empty nodes with only fronts.
		// Replace empty nodes with only backs.
		int32 iReplacementNode;
		if     ( Node->iFront != INDEX_NONE ) iReplacementNode = Node->iFront;
		else if( Node->iBack  != INDEX_NONE ) iReplacementNode = Node->iBack;
		else                                  iReplacementNode = INDEX_NONE;

		if( iParent == INDEX_NONE )
		{
			// Root.
			if( iReplacementNode == INDEX_NONE )
			{
				Model->Nodes.Empty();
			}
			else
			{
				*Node = Model->Nodes[iReplacementNode];
			}
		}
		else
		{
			// Regular node.
			FBspNode *ParentNode = &Model->Nodes[iParent];

			if     ( ParentNode->iFront == iNode ) ParentNode->iFront = iReplacementNode;
			else if( ParentNode->iBack  == iNode ) ParentNode->iBack  = iReplacementNode;
			else if( ParentNode->iPlane == iNode ) ParentNode->iPlane = iReplacementNode;
			else UE_LOG(LogHCsgUtils, Fatal, TEXT("CleanupNodes: Parent and child are unlinked"));
		}
	}
}


void UHCsgUtils::bspCleanup( UModel *Model )
{
	if( Model->Nodes.Num() > 0 ) 
		CleanupNodes( Model, 0, INDEX_NONE );
}

