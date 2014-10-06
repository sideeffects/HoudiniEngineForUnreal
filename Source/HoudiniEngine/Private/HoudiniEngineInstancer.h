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


class UStaticMesh;
class UObjectProperty;
class UInstancedStaticMeshComponent;
class FReferenceCollector;


struct FHoudiniEngineInstancer
{

public:

	/** Constructor. **/
	FHoudiniEngineInstancer(UStaticMesh* InStaticMesh);

public:

	/** Reference counting propagation. **/
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

public:

	/** Reset instancer information. **/
	void Reset();

	/** Set static mesh. **/
	void SetStaticMesh(UStaticMesh* InStaticMesh);

	/** Set object property. **/
	void SetObjectProperty(UObjectProperty* InObjectProperty);

	/** Set instanced component. **/
	void SetInstancedStaticMeshComponent(UInstancedStaticMeshComponent* InComponent);

	/** Add transformation for a new instance. **/
	void AddTransformation(const FTransform& Transform);

public:

	/** Return instanced component associated with this instancer. **/
	UInstancedStaticMeshComponent* GetInstancedStaticMeshComponent() const;

	/** Add instances for component corresponding to this instancer. Uses transform information. **/
	void AddInstancesToComponent();

	/** Sets component's static mesh. **/
	void SetComponentStaticMesh();

	/** Return static mesh. **/
	UStaticMesh* GetStaticMesh() const;

protected:

	/** Array of transformations for this instancer. **/
	TArray<FTransform> Transformations;

	/** Property which refers to this intancer. **/
	UObjectProperty* ObjectProperty;

	/** Static mesh object used by this instancer. **/
	UStaticMesh* StaticMesh;

	/** Original static mesh object used by this instancer. **/
	UStaticMesh* OriginalStaticMesh;

	/** Instanced component used by this instancer. **/
	UInstancedStaticMeshComponent* Component;

	/** Is set to true when this instancer is used and needs not to be deallocated. **/
	bool bIsUsed;
};
