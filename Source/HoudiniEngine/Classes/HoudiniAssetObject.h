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
#include "HoudiniMeshVertexBuffer.h"
#include "HoudiniMeshVertexFactory.h"
#include "HoudiniAssetObject.generated.h"

class FArchive;
class UMaterial;
class FReferenceCollector;

UCLASS(config = Editor)
class HOUDINIENGINE_API UHoudiniAssetObject : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	//UHoudiniAssetObject(const FPostConstructInitializeProperties& PCIP);

public:

	/** Given a list of Houdini asset objects, free them and empty the list. **/
	static void ReleaseObjects(TArray<UHoudiniAssetObject*>& HoudiniAssetObjects);

public: /** UObject methods. **/

	virtual void Serialize(FArchive& Ar) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** Vertex factory transforms explicit vertex attributes from local to world space. **/
	FHoudiniMeshVertexFactory VertexFactory;

	/** Index buffer. **/
	FHoudiniMeshIndexBuffer IndexBuffer;

	/** Vertex buffer. **/
	FHoudiniMeshVertexBuffer* VertexBuffer;

	/** Bounding volume information for this asset object. **/
	FBoxSphereBounds BoundingVolume;

	/** Material referenced by this asset object. **/
	UMaterial* Material;
};
