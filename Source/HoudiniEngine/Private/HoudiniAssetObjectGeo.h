/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once

class FArchive;
class FReferenceCollector;
class FHoudiniMeshVertexBuffer;
class FHoudiniMeshVertexFactory;
class FHoudiniAssetObjectGeoPart;

class FHoudiniAssetObjectGeo
{
	friend class FHoudiniMeshSceneProxy;

public:

	/** Constructor. **/
	FHoudiniAssetObjectGeo(const FMatrix& InTransform);

	/** Destructor. **/
	virtual ~FHoudiniAssetObjectGeo();

public:

	/** Add a part to this asset geo. **/
	void AddGeoPart(FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart);

	/** Reference counting propagation. **/
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	/** Serialization. **/
	virtual void Serialize(FArchive& Ar);

	/** Retrieve list of vertices. **/
	TArray<FDynamicMeshVertex>& GetVertices();

	/** Add vertices of given triangle to list of vertices. **/
	void AddTriangleVertices(FHoudiniMeshTriangle& Triangle);

	/** Create rendering resources for this geo. **/
	void CreateRenderingResources();

	/** Release rendering resources used by this geo. **/
	void ReleaseRenderingResources();

	/** Return transform of this geo. **/
	const FMatrix& GetTransform() const;

protected:

	/** List of geo parts (these correspond to submeshes). Will always have at least one. **/
	TArray<FHoudiniAssetObjectGeoPart*> HoudiniAssetObjectGeoParts;

	/** Vertices used by this geo. **/
	TArray<FDynamicMeshVertex> Vertices;

	/** Transform for this part. **/
	FMatrix Transform;

	/** Corresponding Vertex buffer used by proxy object. Owned by render thread. Kept here for indexing. **/
	FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer;

	/** Corresponding Vertex factory used by proxy object. Owned by render thread. Kept here for indexing. **/
	FHoudiniMeshVertexFactory* HoudiniMeshVertexFactory;
};
