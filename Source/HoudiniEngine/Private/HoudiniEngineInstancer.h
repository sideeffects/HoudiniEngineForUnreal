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
class FTransform;
class UStaticMesh;
class UObjectProperty;
class FReferenceCollector;
class UInstancedStaticMeshComponent;


struct FHoudiniEngineInstancer
{
	friend class UHoudiniAssetComponent;

public:

	/** Constructor. **/
	FHoudiniEngineInstancer(UStaticMesh* InStaticMesh);

public:

	/** Reference counting propagation. **/
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** Serialization. **/
	void Serialize(FArchive& Ar);

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

	/** Add a bulk of transformations. **/
	void AddTransformations(const TArray<FTransform>& InTransforms);

	/** Marks this instancer as used or unused. **/
	void MarkUsed(bool bUsed);

public:

	/** Return instanced component associated with this instancer. **/
	UInstancedStaticMeshComponent* GetInstancedStaticMeshComponent() const;

	/** Add instances for component corresponding to this instancer. Uses transform information. **/
	void AddInstancesToComponent();

	/** Sets component's static mesh. **/
	void SetComponentStaticMesh();

	/** Return static mesh. **/
	UStaticMesh* GetStaticMesh() const;

	/** Return true if instancer is used. **/
	bool IsUsed() const;

	/** Return object property for managing this instancer's mesh. **/
	UObjectProperty* GetObjectProperty() const;

	/** Return original static mesh. **/
	UStaticMesh* GetOriginalStaticMesh() const;

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

	/** Input property name tied to this instancer. Used for serialization. **/
	FString ObjectPropertyName;

	/** Is set to true when this instancer is used and needs not to be deallocated. **/
	bool bIsUsed;
};
