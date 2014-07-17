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
#include "HoudiniMeshIndexBuffer.h"

class FHoudiniMeshIndexBuffer;

class FHoudiniAssetObjectGeoPart
{
	friend class FHoudiniMeshSceneProxy;

public:

	/** Constructor. **/
	FHoudiniAssetObjectGeoPart(const TArray<int32>& InIndices);

	/** Destructor. **/
	virtual ~FHoudiniAssetObjectGeoPart();

public:

	/** Serialization. **/
	virtual void Serialize(FArchive& Ar);

	/** Reference counting propagation - we need this in order to keep material reference, since we are not UObject derived. **/
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	/** Create rendering resources for this part. **/
	void CreateRenderingResources();

	/** Release rendering resources used by this part. **/
	void ReleaseRenderingResources();

protected:

	/** Array of stored indices for this part. **/
	TArray<int32> Indices;

	/** Material for this part. **/
	UMaterial* Material;

	/** Corresponding Index buffer used by proxy object. Owned by render thread. Kept here for indexing. **/
	FHoudiniMeshIndexBuffer* HoudiniMeshIndexBuffer;

	/** Bounding volume information for this part. **/
	FBoxSphereBounds BoundingVolume;
};
