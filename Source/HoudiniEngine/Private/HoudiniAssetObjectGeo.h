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

#include "HAPI.h"

class FArchive;
struct FBoxSphereBounds;
class FReferenceCollector;
class FHoudiniMeshVertexBuffer;
class FHoudiniMeshVertexFactory;
class FHoudiniAssetObjectGeoPart;
class UMaterial;

class FHoudiniAssetObjectGeo
{
	friend class FHoudiniEngine;
	friend struct FHoudiniEngineUtils;
	friend class UHoudiniAssetComponent;
	friend class FHoudiniMeshSceneProxy;

public:

	/** Constructor. **/
	FHoudiniAssetObjectGeo();
	FHoudiniAssetObjectGeo(const FMatrix& InTransform, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

	/** Destructor. **/
	virtual ~FHoudiniAssetObjectGeo();

public:

	/** Add a part to this asset geo. **/
	void AddGeoPart(FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart);

	/** Reference counting propagation. **/
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	/** Serialization. **/
	virtual void Serialize(FArchive& Ar);

	/** Create rendering resources for this geo. **/
	void CreateRenderingResources();

	/** Release rendering resources used by this geo. **/
	void ReleaseRenderingResources();

	/** Return true if rendering resources for this geo have been created. **/
	bool CheckRenderingResourcesCreated() const;

	/** Return transform of this geo. **/
	const FMatrix& GetTransform() const;

	/** Compute whether this geo uses multiple materials. **/
	void ComputeMultipleMaterialUsage();

	/** Returns true if this geo uses multiple materials, false otherwise. **/
	bool UsesMultipleMaterials() const;

	/** Collect textures used by parts. **/
	void CollectTextures(TArray<UTexture2D*>& Textures);

	/** Retrieve single material. **/
	UMaterial* GetSingleMaterial() const;

	/** Replace material on all parts with given material. **/
	void ReplaceMaterial(UMaterial* Material);

	/** Return true if this geometry is Houdini logo geometry. **/
	bool IsHoudiniLogo() const;

	/** Return aggegate bounding volume for this geo. **/
	const FBoxSphereBounds& GetAggregateBoundingVolume() const;

	/** Compute aggregate bounding volume from all parts. **/
	void ComputeAggregateBoundingVolume();

	/** Return number of positions. **/
	uint32 GetPositionCount() const;

protected:

	/** Set this geometry as Houdini logo geometry. **/
	void SetHoudiniLogo();

protected:

	/** List of geo parts (these correspond to submeshes). Will always have at least one. **/
	TArray<FHoudiniAssetObjectGeoPart*> HoudiniAssetObjectGeoParts;

	/** Raw vertex data for this geo. **/
	TArray<FVector> VertexPositions;
	TArray<FVector> VertexNormals;
	TArray<FVector> VertexColors;
	TArray<FVector2D> VertexTextureCoordinates[MAX_STATIC_TEXCOORDS];
	TArray<FPackedNormal> VertexPackedTangentXs;
	TArray<FPackedNormal> VertexPackedTangentZs;

	/** Transform for this part. **/
	FMatrix Transform;

	/** Bounding volume information for this geo - aggregate of all parts bounding volumes. **/
	FBoxSphereBounds AggregateBoundingVolume;

	/** Vertex buffers used by proxy object. owned by render thread. **/
	TArray<FHoudiniMeshVertexBuffer*> HoudiniMeshVertexBuffers;

	/** Corresponding Vertex factory used by proxy object. Owned by render thread. Kept here for indexing. **/
	FHoudiniMeshVertexFactory* HoudiniMeshVertexFactory;

	/** HAPI Object Id for this geometry. **/
	HAPI_ObjectId ObjectId;

	/** HAPI Geo Id for this geometry. **/
	HAPI_GeoId GeoId;
	
	/** HAPI Part Id for this geometry. **/
	HAPI_PartId PartId;

	/** Number of components using this geo. **/
	uint32 ComponentReferenceCount;

	/** Is set to true when submeshes use different materials. **/
	bool bMultipleMaterials;
	
	/** Is set to true when this geometry is a Houdini logo geometry. **/
	bool bHoudiniLogo;

	/** Is set to true when rendering resources have been initialized. **/
	bool bRenderingResourcesCreated;
};
