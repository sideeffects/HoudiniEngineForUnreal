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

#include "HoudiniInputTypes.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniGeoPartObject.h"
#include "UnrealObjectInputRuntimeTypes.h"

#include "CoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "Engine/Brush.h"
#include "Engine/Polys.h"
#include "UObject/SoftObjectPtr.h"

#include "LandscapeSplineSegment.h"

#include "HoudiniInputObject.generated.h"

class UHoudiniPCGDataObject;
class UHoudiniPCGDataCollection;
class ULandscapeSplineControlPoint;
class UStaticMesh;
class USkeletalMesh;
class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UInstancedStaticMeshComponent;
class USplineComponent;
class UHoudiniAssetComponent;
class UHoudiniCookable;
class AActor;
class ALandscapeProxy;
class ABrush;
class UHoudiniInput;
class ALandscapeProxy;
class UModel;
class UHoudiniInput;
class UCameraComponent;
class ALevelInstance;
class APackedLevelActor;
class UHoudiniInputActor;
class UPCGData;

UENUM()
enum class EHoudiniInputObjectType : uint8
{
	Invalid,

	Object,
	StaticMesh,
	SkeletalMesh,
	SceneComponent,
	StaticMeshComponent,
	InstancedStaticMeshComponent,
	SplineComponent,
	HoudiniSplineComponent,
	HoudiniAssetComponent,		// TODO: Change to cookable?
	Actor,
	Landscape,
	Brush,
	CameraComponent,
	DataTable,
	HoudiniAssetActor,
	FoliageType_InstancedStaticMesh,
	GeometryCollection,
	GeometryCollectionComponent,
	GeometryCollectionActor_Deprecated,
	SkeletalMeshComponent,
	LandscapeSplineActor,
	LandscapeSplinesComponent,
	Blueprint,
	Animation,
	SplineMeshComponent,
	LevelInstance,
	PackedLevelActor,
	PCGData,
	Texture,
	HoudiniCookable
};


//-----------------------------------------------------------------------------------------------------------------------------
// UObjects input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputObject : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create the proper input object
	static UHoudiniInputObject * CreateTypedInputObject(UObject * InObject, UObject* InOuter, const FString& InParamName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	// Check whether two input objects match
	virtual bool Matches(const UHoudiniInputObject& Other) const;

	//
	static EHoudiniInputObjectType GetInputObjectTypeFromObject(UObject* InObject);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings);

	// Set the Transform of this input object. For geo inputs this is a transform offset, for actors the actor world
	// transform and for scene components the component world transform.
	void SetTransform(const FTransform& InTransform) { Transform = InTransform; }
	const FTransform& GetTransform() const { return Transform; }
	FTransform& GetTransform() { return Transform; }
	
	// Get the transform to use on the Obj node associated with this object in Houdini. The default implementation
	// calls GetTransform().
	virtual FTransform GetHoudiniObjectTransform() const { return GetTransform(); }

	// Invalidate and ask for the deletion of this input object's node
	virtual void InvalidateData();

	// UObject accessor
	virtual UObject* GetObject() const;

	// Indicates if this input has changed and should be updated
	virtual bool HasChanged() const { return bHasChanged; };

	// Indicates if this input has changed and should be updated
	virtual bool HasTransformChanged() const { return bTransformChanged; };

	// Indicates if this input needs to trigger an update
	virtual bool NeedsToTriggerUpdate() const { return bNeedsToTriggerUpdate; };

	virtual void MarkChanged(const bool& bInChanged);
	virtual void SetNeedsToTriggerUpdate(const bool& bInTriggersUpdate) { bNeedsToTriggerUpdate = bInTriggersUpdate; };
	virtual void MarkTransformChanged(const bool bInChanged) { bTransformChanged = bInChanged; SetNeedsToTriggerUpdate(bInChanged); };

	// Set the InputNodeId.
	void SetInputNodeId(int32 InInputNodeId);

	// If the ref counted system is enabled, then return the node via the InputNodeHandle, otherwise return the InputNodeId. 
	int32 GetInputNodeId() const;

	// Set the InputObjectNodeId
	void SetInputObjectNodeId(int32 InInputObjectNodeId);

	// If the ref counted system is enabled, then return the node via the InputNodeHandle, otherwise return the InputObjectNodeId. 
	int32 GetInputObjectNodeId() const;

	bool GetImportAsReference() const { return CachedInputSettings.bImportAsReference; };

	bool GetImportAsReferenceRotScaleEnabled() const { return CachedInputSettings.bImportAsReferenceRotScaleEnabled; };

	bool GetImportAsReferenceBboxEnabled() const { return CachedInputSettings.bImportAsReferenceBboxEnabled; };

	bool GetImportAsReferenceMaterialEnabled() const { return CachedInputSettings.bImportAsReferenceMaterialEnabled; };
	
	const TArray<FString>& GetMaterialReferences();

	// Formats Input Reference path strings obtained by UObject::GetFullName() of format:
	//`Material /path/to/asset` and adds single quotes: `Material'/path/to/asset'`
	static FString FormatAssetReference(FString AssetReference);

#if WITH_EDITOR
	void SwitchUniformScaleLock() { bUniformScaleLocked = !bUniformScaleLocked; };
	bool IsUniformScaleLocked() const { return bUniformScaleLocked; };

	void PostEditUndo() override;
#endif

	virtual UHoudiniInputObject* DuplicateAndCopyState(UObject* DestOuter);
	virtual void CopyStateFrom(UHoudiniInputObject* InInput, bool bCopyAllProperties);

	// Set whether this object can delete Houdini nodes.
	virtual void SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes);
	bool CanDeleteHoudiniNodes() const { return bCanDeleteHoudiniNodes; }

	// If true then this input object uses the InputNodeHandle instead of InputNodeId and InputObjectNodeId if the
	// ref counted input system is enabled.
	void SetInputNodeHandleOverridesNodeIds(bool bInInputNodeHandleOverridesNodeIds) { bInputNodeHandleOverridesNodeIds = bInInputNodeHandleOverridesNodeIds; }
	bool InputNodeHandleOverridesNodeIds() const { return bInputNodeHandleOverridesNodeIds; }

	FGuid GetInputGuid() const { return Guid; }

	/**
	 * Populate OutChangedObjects with any output objects (this object and its children if it has any) that has changed
	 * or have invalid HAPI node ids.
	 * Any objects that have not changed and have valid HAPI node ids have their node ids added to
	 * OutNodeIdsOfUnchangedValidObjects.
	 *
	 * @return true if this object, or any of its child objects, were added to OutChangedObjects.
	 */
	virtual bool GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects);

protected:

	/**
	 * Returns true if this object type uses an input object (OBJ) node. This is not a recursive check, for objects
	 * with child objects (such as an actor with components), each child input object must be checked as well.
	 */
	virtual bool UsesInputObjectNode() const { return true; }

	virtual void BeginDestroy() override;

	// If this type of input object has materials, this function updates its list of string material references.
	void UpdateMaterialReferences();

public:

	// The object referenced by this input
	// This property should be protected. Don't access this directly. Use GetObject() / Update() instead.
	UPROPERTY()
	TSoftObjectPtr<UObject> InputObject;

	// The type of Object this input refers to
	UPROPERTY()
	EHoudiniInputObjectType Type;

	// Guid that uniquely identifies this input object.
	// Also useful to correlate inputs between blueprint component templates and instances.
	UPROPERTY(DuplicateTransient)
	FGuid Guid;

	// Handle to the entry that this input object uses in the new ref counted input system.
	FUnrealObjectInputHandle InputNodeHandle;

protected:

	// Indicates this input object has changed
	UPROPERTY(DuplicateTransient)
	bool bHasChanged;

	// Indicates this input object should trigger an input update/cook
	UPROPERTY(DuplicateTransient)
	bool bNeedsToTriggerUpdate;

	// Indicates that this input transform should be updated
	UPROPERTY(DuplicateTransient)
	bool bTransformChanged;

	// String References to the materials on the input object.
	// This is used when sending input by reference to Houdini.
	// These strings are in the form of: Material'/path/to/reference'
	UPROPERTY()
	TArray<FString> MaterialReferences;
	
	// Indicates if change the scale of Transfrom Offset of this object uniformly
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	bool bUniformScaleLocked;
#endif

	// The settings cached from the Input at the last Update() call.
	UPROPERTY()
	FHoudiniInputObjectSettings CachedInputSettings;

	UPROPERTY()
	bool bCanDeleteHoudiniNodes;

	// If true then this input object uses the InputNodeHandle instead of InputNodeId and InputObjectNodeId if the
	// ref counted input system is enabled.
	UPROPERTY()
	bool bInputNodeHandleOverridesNodeIds;

	// The object's transform/transform offset
	UPROPERTY()
	FTransform Transform;

public:
	// While the Transform property represents the transform, we need to keep a human readable version of the Rotation
	// ie. not just a Quaternion, or the UI becomes non-sensical to humans, since converting too and from Roll,Pitch,Yaw
	// and Quaternion ends up with different results.
	UPROPERTY()
	FRotator UserInputRotator;

private:
	// This input object's "main" (SOP) NodeId
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	int32 InputNodeId;

	// This input object's "container" (OBJ) NodeId
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	int32 InputObjectNodeId;
};


//-----------------------------------------------------------------------------------------------------------------------------
// UStaticMesh input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputStaticMesh : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()
		
public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Nothing to add for Static Meshes?

	// StaticMesh accessor
	virtual class UStaticMesh* GetStaticMesh() const;
};



//-----------------------------------------------------------------------------------------------------------------------------
// USkeletalMesh input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputSkeletalMesh : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Nothing to add for SkeletalMesh Meshes?

	// StaticMesh accessor
	class USkeletalMesh* GetSkeletalMesh();
};

//-----------------------------------------------------------------------------------------------------------------------------
// UAnimSequence input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputAnimation : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Nothing to add for SkeletalMesh Meshes?

	// StaticMesh accessor
	class UAnimSequence* GetAnimation();
};



//-----------------------------------------------------------------------------------------------------------------------------
// UGeometryCollection input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputGeometryCollection : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// GeometryCollection accessor
	class UGeometryCollection* GetGeometryCollection();
};




//-----------------------------------------------------------------------------------------------------------------------------
// USceneComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputSceneComponent : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	virtual void UpdateTransform();

	// Get the cached world transform of this scene component relative to the cached transform of its owner actor.
	FTransform GetTransformRelativeToOwner() const { return Transform.GetRelativeTransform(ActorTransform); }

	// SceneComponent accessor
	class USceneComponent* GetSceneComponent();

	// Returns true if the attached actor's (parent) transform has been modified
	virtual bool HasActorTransformChanged() const;

	// Returns true if the attached component's transform has been modified
	virtual bool HasComponentTransformChanged() const;

	// Return true if the component itself has been modified
	virtual bool HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const;

	virtual FTransform GetHoudiniObjectTransform() const override;

	UHoudiniInputActor* GetParentInputActor() const { return ParentInputActor; }
	
protected:
	void SetParentInputActor(UHoudiniInputActor* InParentInputActor) { ParentInputActor = InParentInputActor; }

public:

	// This component's parent Actor transform
	UPROPERTY()
	FTransform ActorTransform = FTransform::Identity;

private:

	// The input object of the parent actor of the component
	UPROPERTY()
	TObjectPtr<UHoudiniInputActor> ParentInputActor;
};



//-----------------------------------------------------------------------------------------------------------------------------
// UStaticMeshComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputMeshComponent : public UHoudiniInputSceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// StaticMeshComponent accessor
	UStaticMeshComponent* GetStaticMeshComponent();

	// Get the referenced StaticMesh
	UStaticMesh* GetStaticMesh();

	// Returns true if the attached component's materials have been modified
	bool HasComponentMaterialsChanged() const;

	// Return true if SMC's static mesh has been modified
	virtual bool HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

public:

	// Keep track of the selected Static Mesh
	UPROPERTY()
	TSoftObjectPtr<class UStaticMesh> StaticMesh = nullptr;
};




//-----------------------------------------------------------------------------------------------------------------------------
// UInstancedStaticMeshComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputInstancedMeshComponent : public UHoudiniInputMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// InstancedStaticMeshComponent accessor
	UInstancedStaticMeshComponent* GetInstancedStaticMeshComponent();

	// Returns true if the instances have changed
	bool HasInstancesChanged() const;

	// Returns true if the attached component's transform has been modified
	virtual bool HasComponentTransformChanged() const override;
	
public:

	// Array of transform for this ISMC's instances
	UPROPERTY()
	TArray<FTransform> InstanceTransforms;	
};




//-----------------------------------------------------------------------------------------------------------------------------
// USplineComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputSplineComponent : public UHoudiniInputSceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// USplineComponent accessor
	USplineComponent* GetSplineComponent();

	// Return true if the component itself has been modified
	virtual bool HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

public:

	// Number of CVs used by the spline component, used to detect modification
	UPROPERTY()
	int32 NumberOfSplineControlPoints = -1;

	// Spline Length, used for fast detection of modifications of the spline..
	UPROPERTY()
	float SplineLength = -1.0f;

	// Spline resolution used to generate the asset, used to detect setting modification
	UPROPERTY()
	float SplineResolution = -1.0f;

	// Is the spline closed?
	UPROPERTY()
	bool SplineClosed = false;

	// Transforms of each of the spline's control points
	UPROPERTY()
	TArray<FTransform> SplineControlPoints;
};


//-----------------------------------------------------------------------------------------------------------------------------
// UGeometryCollectionComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputGeometryCollectionComponent : public UHoudiniInputSceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// GeometryCollection accessor
	class UGeometryCollectionComponent* GetGeometryCollectionComponent();
	class UGeometryCollection* GetGeometryCollection();
};

//-----------------------------------------------------------------------------------------------------------------------------
// UHoudiniInputSkeletalMeshComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputSkeletalMeshComponent : public UHoudiniInputSceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Skeletal Mesh accessor
	class USkeletalMeshComponent* GetSkeletalMeshComponent();
	class USkeletalMesh* GetSkeletalMesh();
};

//-----------------------------------------------------------------------------------------------------------------------------
// UHoudiniSplineComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputHoudiniSplineComponent : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	virtual UObject* GetObject() const override;

	virtual void MarkChanged(const bool& bInChanged) override;

	virtual void SetNeedsToTriggerUpdate(const bool& bInTriggersUpdate) override;

	// Indicates if this input has changed and should be updated
	virtual bool HasChanged() const override;

	// Indicates if this input needs to trigger an update
	virtual bool NeedsToTriggerUpdate() const override;

	// UHoudiniSplineComponent accessor
	UHoudiniSplineComponent* GetCurveComponent() const;

public:

	// The type of curve (polygon, NURBS, bezier)
	UPROPERTY()
	EHoudiniCurveType CurveType = EHoudiniCurveType::Polygon;

	// The curve's method (CVs, Breakpoint, Freehand)
	UPROPERTY()
	EHoudiniCurveMethod CurveMethod = EHoudiniCurveMethod::CVs;

	UPROPERTY()
	bool Reversed = false;


protected:
	
	// NOTE: We are using this reference to the component since the component, for now,
	// lives on the same actor as this input object. If we use a Soft Object Reference instead the editor
	// will complain about breaking references everytime we try to delete the actor.
	UPROPERTY(Instanced)
	TObjectPtr<UHoudiniSplineComponent> CachedComponent;
};



//-----------------------------------------------------------------------------------------------------------------------------
// UCameraComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputCameraComponent : public UHoudiniInputSceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// UCameraComponent accessor
	UCameraComponent* GetCameraComponent();

	// Return true if SMC's static mesh has been modified
	virtual bool HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

public:

	UPROPERTY()
	float FOV;

	UPROPERTY()
	float AspectRatio;

	UPROPERTY()
	//TEnumAsByte<ECameraProjectionMode::Type> ProjectionType;
	bool bIsOrthographic;
	
	UPROPERTY()
	float OrthoWidth;
	UPROPERTY()
	float OrthoNearClipPlane;
	UPROPERTY()
	float OrthoFarClipPlane;
	
};


//-----------------------------------------------------------------------------------------------------------------------------
// UHoudiniAssetComponent input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputHoudiniAsset : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Getter for the asset Id of the tracked HAC. Updated via Update().
	int32 GetNodeId() const { return NodeId; }

	// Cookable accessor
	UHoudiniCookable* GetHoudiniCookable();

	// UHoudiniAssetComponent accessor
	// TODO: Legacy - replaced by GetCookable
	UHoudiniAssetComponent* GetHoudiniAssetComponent();
public:

	// The output index of the node that we want to use as input
	UPROPERTY()
	int32 OutputIndex;

protected:

	// The node ID recorded at the last Update().
	UPROPERTY()
	int32 NodeId;

};



//-----------------------------------------------------------------------------------------------------------------------------
// AActor input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputActor : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Check whether the actor transform, or any of its components have transform changes.
	virtual bool HasActorTransformChanged() const;

	// Indicates this object is dirty and should be updated
	virtual void MarkChanged(const bool& bInChanged) override;

	// GUID for temp merged SM of all spline mesh components of this actor (if any)
	FGuid GetSplinesMeshPackageGuid() const { return GeneratedSplinesMeshPackageGuid; }

	// Getter for temp merged SM of all spline mesh components of this actor (if any)
	UStaticMesh* GetGeneratedSplineMesh() const { return GeneratedSplinesMesh; }

	// Setter for temp merged SM of all spline mesh components of this actor (if any)
	void SetGeneratedSplineMesh(UStaticMesh* const InSM) { GeneratedSplinesMesh = InSM; }

protected:
	virtual bool HasRootComponentTransformChanged() const;
	virtual bool HasComponentsTransformChanged() const;

public:
	//
	virtual bool ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings=nullptr) const;

	// Return true if any content of this actor has possibly changed (for example geometry edits on a 
	// Brush or changes on procedurally generated content).
	// NOTE: This is more generally applicable and could be moved to the HoudiniInputObject class.
	virtual bool HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const;

	// AActor accessor
	AActor* GetActor() const;

	const TArray<UHoudiniInputSceneComponent*>& GetActorComponents() const { return ActorComponents; }

	// The number of components added with the last call to Update
	int32 GetLastUpdateNumComponentsAdded() const { return LastUpdateNumComponentsAdded; }
	// The number of components remove with the last call to Update	
	int32 GetLastUpdateNumComponentsRemoved() const { return LastUpdateNumComponentsRemoved; }

	/**
	 * Populate OutChangedObjects with any output objects (this object and its children if it has any) that has changed
	 * or have invalid HAPI node ids.
	 * Any objects that have not changed and have valid HAPI node is have their node ids added to
	 * OutNodeIdsOfUnchangedValidObjects.
	 *
	 * @return true if this object, or any of its child objects, were added to OutChangedObjects.
	 */
	virtual bool GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects) override;

	// Returns the number of spline mesh components after the last update.
	int32 GetNumSplineMeshComponents() const { return NumSplineMeshComponents; }

	bool UsedMergeSplinesMeshAtLastTranslate() const { return bUsedMergeSplinesMeshAtLastTranslate; }
	void SetUsedMergeSplinesMeshAtLastTranslate(bool bInValue) { bUsedMergeSplinesMeshAtLastTranslate = bInValue; }

	virtual void InvalidateData() override;

	virtual void InvalidateSplinesMeshData();

	UPROPERTY()
	int32 SplinesMeshObjectNodeId;

	UPROPERTY()
	int32 SplinesMeshNodeId;

	FUnrealObjectInputHandle SplinesMeshInputNodeHandle;

protected:

	virtual bool UsesInputObjectNode() const override;

	// The actor's components that can be sent as inputs
	UPROPERTY()
	TArray<TObjectPtr<UHoudiniInputSceneComponent>> ActorComponents;

	// The USceneComponents the actor had the last time we called Update (matches the ones in ActorComponents).
	UPROPERTY()
	TSet<TSoftObjectPtr<UObject>> ActorSceneComponents;

	// The number of components added with the last call to Update
	UPROPERTY()
	int32 LastUpdateNumComponentsAdded;

	// The number of components remove with the last call to Update
	UPROPERTY()
	int32 LastUpdateNumComponentsRemoved;

	// The number of spline mesh components in ActorComponents the last time this object was updated.
	UPROPERTY()
	int32 NumSplineMeshComponents;

	// Package GUID for temp static mesh for the merged spline mesh components (if any).
	UPROPERTY()
	FGuid GeneratedSplinesMeshPackageGuid;

	// The merged static mesh generated for the spline mesh components.
	UPROPERTY()
	TObjectPtr<UStaticMesh> GeneratedSplinesMesh;

	// True if the merged spline mesh was sent at the last translation.
	UPROPERTY()
	bool bUsedMergeSplinesMeshAtLastTranslate;
};


//-----------------------------------------------------------------------------------------------------------------------------
// ALevelInstance input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputLevelInstance: public UHoudiniInputActor
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//

	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	virtual bool HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

	virtual bool HasTransformChanged() const override;

	ALevelInstance* GetLevelInstance() const;

	virtual bool ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings) const override { return false; }

	const TMap<TSoftObjectPtr<AActor>, TObjectPtr<UHoudiniInputObject>>& GetTrackedActorObjects() const { return TrackedActorObjects; }

	virtual void InvalidateData() override;

private:
	UPROPERTY()
	TMap<TSoftObjectPtr<AActor>, TObjectPtr<UHoudiniInputObject>> TrackedActorObjects;

	UPROPERTY()
	int32 NumActorsAddedLastUpdate;

	UPROPERTY()
	int32 NumActorsRemovedLastUpdate;

};


//-----------------------------------------------------------------------------------------------------------------------------
// ALandscapeProxy input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputLandscape : public UHoudiniInputActor
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	virtual bool HasActorTransformChanged() const override;

	virtual bool ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings=nullptr) const override;

	virtual void MarkTransformChanged(const bool bInChanged) override;

	virtual bool HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

	virtual FTransform GetHoudiniObjectTransform() const override;

	// ALandscapeProxy accessor
	ALandscapeProxy* GetLandscapeProxy() const;

	void SetLandscapeProxy(UObject* InLandscapeProxy);

	// The number of landscape components that was processed. If this count changes, .e.g, levels have been
	// loaded / unloaded then the input content has changed.
	UPROPERTY()
	int32 CachedNumLandscapeComponents;

protected:
	virtual bool UsesInputObjectNode() const override { return true; }

	// Count the number of landscape components that are currently registered with LandscapeInfo, i.e., loaded into
	// the current world.
	virtual int32 CountLandscapeComponents() const;
};



//-----------------------------------------------------------------------------------------------------------------------------
// ABrush input
//-----------------------------------------------------------------------------------------------------------------------------
// Cache info for a brush in order to determine whether it has changed.

#define BRUSH_HASH_SURFACE_PROPERTIES 0

USTRUCT()
struct FHoudiniBrushInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ABrush> BrushActor;
	UPROPERTY()
	FTransform CachedTransform;
	UPROPERTY()
	FVector CachedOrigin;
	UPROPERTY()
	FVector CachedExtent;
	UPROPERTY()
	TEnumAsByte<EBrushType> CachedBrushType;
	
	UPROPERTY()
	uint64 CachedSurfaceHash;

	bool HasChanged() const;

	static int32 GetNumVertexIndicesFromModel(const UModel* Model);

	FHoudiniBrushInfo();
	FHoudiniBrushInfo(ABrush* InBrushActor);

	template <class T>
	inline void HashCombine(uint64& s, const T & v) const
	{
		//std::hash<T> h;
		s^= ::GetTypeHash(v) + 0x9e3779b9 + (s<< 6) + (s>> 2);
	}

	inline void HashCombine(uint64& s, const FVector3f & V) const
	{
		HashCombine(s, V.X);
		HashCombine(s, V.Y);
		HashCombine(s, V.Z);
	}

	inline void CombinePolyHash(uint64& Hash, const FPoly& Poly) const
	{
		HashCombine(Hash, Poly.Base);
		HashCombine(Hash, Poly.TextureU);
		HashCombine(Hash, Poly.TextureV);
		HashCombine(Hash, Poly.Normal);
		// Do not add addresses to the hash, otherwise it would force a recook every unreal session!
		// HashCombine(Hash, (uint64)(Poly.Material));
	}
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputBrush : public UHoudiniInputActor
{
	GENERATED_BODY()

public:

	UHoudiniInputBrush();

	// Factory function
	static UHoudiniInputBrush* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//----------------------------------------------------------------------
	// UHoudiniInputActor Interface - Begin
	//----------------------------------------------------------------------

	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// Indicates if this input has changed and should be updated
	virtual bool HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

	// Indicates if this input has changed and should be updated
	virtual bool HasChanged() const override { return (!bIgnoreInputObject) && bHasChanged; };

	// Indicates if this input has changed and should be updated
	virtual bool HasTransformChanged() const override { return (!bIgnoreInputObject) && bTransformChanged; };
	
	virtual bool HasActorTransformChanged() const override;

	virtual bool NeedsToTriggerUpdate() const override { return (!bIgnoreInputObject) && bNeedsToTriggerUpdate; };

	//----------------------------------------------------------------------
	// UHoudiniInputActor Interface - End
	//----------------------------------------------------------------------


	// ABrush accessor
	ABrush* GetBrush() const;

	UModel* GetCachedModel() const;

	// Check whether any of the brushes, or their transforms, used to generate this model have changed.
	bool HasBrushesChanged(const TArray<ABrush*>& InBrushes) const;

	// Cache the combined model as well as the input brushes.
	void UpdateCachedData(UModel* InCombinedModel, const TArray<ABrush*>& InBrushes);

	// Returns whether this input object should be ignored when uploading objects to Houdini.
	// This mechanism could be implemented on UHoudiniInputObject.
	bool ShouldIgnoreThisInput();


	// Find only the subtractive brush actors that intersect with the InputObject (Brush actor) bounding box but
	// excluding any selector bounds actors.
	static bool FindIntersectingSubtractiveBrushes(const UHoudiniInputBrush* InputBrush, TArray<ABrush*>& OutBrushes);

protected:
	virtual bool UsesInputObjectNode() const override { return true; }

	UPROPERTY()
	TArray<FHoudiniBrushInfo> BrushesInfo;
	
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UModel> CombinedModel;

	UPROPERTY()
	bool bIgnoreInputObject;

	UPROPERTY()
	TEnumAsByte<EBrushType> CachedInputBrushType;
};

//-----------------------------------------------------------------------------------------------------------------------------
// UPCGData input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputPCGData : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	// DataTable accessor

	UHoudiniPCGDataCollection* GetPCGData() const;
};

//-----------------------------------------------------------------------------------------------------------------------------
// UDataTable input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputDataTable : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	// DataTable accessor
	class UDataTable* GetDataTable() const;
};

//-----------------------------------------------------------------------------------------------------------------------------
// UFoliageType_InstancedStaticMesh input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputFoliageType_InstancedStaticMesh : public UHoudiniInputStaticMesh
{
	GENERATED_UCLASS_BODY()
		
public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	// UHoudiniInputObject overrides
	
	//
	virtual void Update(UObject * InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// StaticMesh accessor
	virtual class UStaticMesh* GetStaticMesh() const override;
};

//-----------------------------------------------------------------------------------------------------------------------------
// Blueprint input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputBlueprint : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//
	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	virtual bool HasComponentsTransformChanged() const;

public:

	// Return true if any content of this actor has possibly changed (for example geometry edits on a 
	// Brush or changes on procedurally generated content).
	// NOTE: This is more generally applicable and could be moved to the HoudiniInputObject class.
	virtual bool HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const;

	// UBlueprint accessor
	UBlueprint* GetBlueprint() const;

	const TArray<UHoudiniInputSceneComponent*>& GetComponents() const { return BPComponents; }

	// The number of components added with the last call to Update
	int32 GetLastUpdateNumComponentsAdded() const { return LastUpdateNumComponentsAdded; }
	// The number of components remove with the last call to Update	
	int32 GetLastUpdateNumComponentsRemoved() const { return LastUpdateNumComponentsRemoved; }

	/**
	 * Populate OutChangedObjects with any output objects (this object and its children if it has any) that has changed
	 * or have invalid HAPI node ids.
	 * Any objects that have not changed and have valid HAPI node is have their node ids added to
	 * OutNodeIdsOfUnchangedValidObjects.
	 *
	 * @return true if this object, or any of its child objects, were added to OutChangedObjects.
	 */
	virtual bool GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects) override;

	virtual void InvalidateData() override;

protected:

	virtual bool UsesInputObjectNode() const override;

	// The BP's components that can be sent as inputs
	UPROPERTY()
		TArray<TObjectPtr<UHoudiniInputSceneComponent>> BPComponents;

	// The USceneComponents the BP had the last time we called Update (matches the ones in BPComponents).
	UPROPERTY()
		TSet<TSoftObjectPtr<UObject>> BPSceneComponents;

	// The number of components added with the last call to Update
	UPROPERTY()
		int32 LastUpdateNumComponentsAdded;

	// The number of components remove with the last call to Update
	UPROPERTY()
		int32 LastUpdateNumComponentsRemoved;
};

//-----------------------------------------------------------------------------------------------------------------------------
// APackedLevelActor input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputPackedLevelActor: public UHoudiniInputActor
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	//

	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	virtual bool HasContentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

	APackedLevelActor* GetPackedLevelActor() const;

	virtual bool ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings) const override;

	UHoudiniInputBlueprint* GetBlueprintInputObject() const { return BlueprintInputObject; }

	virtual void InvalidateData() override;
	
private:
	UPROPERTY()
	TObjectPtr<UHoudiniInputBlueprint> BlueprintInputObject;

};

//-----------------------------------------------------------------------------------------------------------------------------
// ALandscapeSplinesActor input
//-----------------------------------------------------------------------------------------------------------------------------

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputLandscapeSplineActor : public UHoudiniInputActor
{
	GENERATED_UCLASS_BODY()

public:
	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	// ULandscapeSplinesComponent accessor
	class ALandscapeSplineActor* GetLandscapeSplineActor() const;

	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	/**
	 * Returns true if InComponent (a component of the input actor) should be tracked by the input system.
	 * @param InComponent The component to check for tracking. 
	 * @return true if InComponent should be tracked.
	 */
	virtual bool ShouldTrackComponent(UActorComponent const* InComponent, const FHoudiniInputObjectSettings* InSettings=nullptr) const override;
};

//-----------------------------------------------------------------------------------------------------------------------------
// ULandscapeSplinesComponent input
//-----------------------------------------------------------------------------------------------------------------------------

/**
 * Struct for caching landscape spline control points. ULandscapeSplineControlPoint cannot be duplicated with
 * an outer other than ULandscapeSplinesComponent. * 
 */
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeSplineControlPointData
{
	GENERATED_BODY();

	FHoudiniLandscapeSplineControlPointData();

	// Copied properties from ULandscapeSplineControlPoint for which we want to track changes
	
	/** Location in Landscape-space */
	UPROPERTY()
	FVector Location;

	/** Rotation of tangent vector at this point (in landscape-space) */
	UPROPERTY()
	FRotator Rotation;

	/** Half-Width of the spline at this point. */
	UPROPERTY()
	float Width;

#if WITH_EDITORONLY_DATA
	/** Vertical offset of the spline segment mesh. Useful for a river's surface, among other things. */
	UPROPERTY()
	float SegmentMeshOffset;

	/**
	 * Name of blend layer to paint when applying spline to landscape
	 * If "none", no layer is painted
	 */
	UPROPERTY()
	FName LayerName;

	/** If the spline is above the terrain, whether to raise the terrain up to the level of the spline when applying it to the landscape. */
	UPROPERTY()
	uint32 bRaiseTerrain:1;

	/** If the spline is below the terrain, whether to lower the terrain down to the level of the spline when applying it to the landscape. */
	UPROPERTY()
	uint32 bLowerTerrain:1;

	/** Mesh to use on the control point */
	UPROPERTY()
	TObjectPtr<UStaticMesh> Mesh;

	/** Overrides mesh's materials */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	/** Scale of the control point mesh */
	UPROPERTY()
	FVector MeshScale;
#endif
	
};

/**
 * Struct for caching landscape spline control points. ULandscapeSplineSegment cannot be duplicated with
 * an outer other than ULandscapeSplinesComponent. * 
 */
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniLandscapeSplineSegmentData
{
	GENERATED_BODY();

	FHoudiniLandscapeSplineSegmentData();
	
	// Copied properties from ULandscapeSplineSegment for which we want to track changes

#if WITH_EDITORONLY_DATA
	/**
	 * Name of blend layer to paint when applying spline to landscape
	 * If "none", no layer is painted
	 */
	UPROPERTY()
	FName LayerName;

	/** If the spline is above the terrain, whether to raise the terrain up to the level of the spline when applying it to the landscape. */
	UPROPERTY()
	uint32 bRaiseTerrain:1;

	/** If the spline is below the terrain, whether to lower the terrain down to the level of the spline when applying it to the landscape. */
	UPROPERTY()
	uint32 bLowerTerrain:1;

	/** Spline meshes from this list are used in random order along the spline. */
	UPROPERTY()
	TArray<FLandscapeSplineMeshEntry> SplineMeshes;
#endif
	
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputLandscapeSplinesComponent : public UHoudiniInputSceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// ULandscapeSplinesComponent accessor
	class ULandscapeSplinesComponent* GetLandscapeSplinesComponent() const;

	virtual bool HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

	const TArray<FHoudiniLandscapeSplineControlPointData>& GetCachedControlPoints() const { return CachedControlPoints; }

	const TArray<FHoudiniLandscapeSplineSegmentData>& GetCachedSegments() const { return CachedSegments; }

	const TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& GetControlPointIdMap() const { return ControlPointIdMap; }
	void SetControlPointIdMap(const TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>& InControlPointIdMap) { ControlPointIdMap = InControlPointIdMap; }
	void SetControlPointIdMap(TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32>&& InControlPointIdMap) { ControlPointIdMap = InControlPointIdMap; }

	int32 GetNextControlPointId() const { return NextControlPointId; }
	void SetNextControlPointId(int32 InNextControlPointId) { NextControlPointId = InNextControlPointId; }

protected:
	/** A copy of the control points of the spline the last time this object was updated. */
	UPROPERTY()
	TArray<FHoudiniLandscapeSplineControlPointData> CachedControlPoints;

	/** A copy of the segments of the landscape spline the last time this was object was updated. */
	UPROPERTY()
	TArray<FHoudiniLandscapeSplineSegmentData> CachedSegments;

	/** The generated ids for the control points from the last time this object was updated. */
	UPROPERTY()
	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32> ControlPointIdMap;

	/** The id to assign the next new control point we encounter. */
	UPROPERTY()
	int32 NextControlPointId;

};

//-----------------------------------------------------------------------------------------------------------------------------
// USplineMeshComponent input
//-----------------------------------------------------------------------------------------------------------------------------

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputSplineMeshComponent : public UHoudiniInputMeshComponent
{
	GENERATED_UCLASS_BODY()

public:
	//
	static UHoudiniInputObject* Create(UObject* InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings, UHoudiniInputActor* InParentInputActor=nullptr);

	//
	virtual void Update(UObject* InObject, const FHoudiniInputObjectSettings& InSettings) override;

	// USplineMeshComponent accessor
	class USplineMeshComponent* GetSplineMeshComponent() const;

	virtual bool HasComponentChanged(const FHoudiniInputObjectSettings& InSettings) const override;

	const FGuid& GetMeshPackageGuid() const { return MeshPackageGuid; }

	TObjectPtr<UStaticMesh> GetGeneratedMesh() { return GeneratedMesh; }

	void SetGeneratedMesh(UStaticMesh* const InMesh) { GeneratedMesh = InMesh; }

protected:
	UPROPERTY()
	FGuid MeshPackageGuid;

	UPROPERTY()
	TObjectPtr<UStaticMesh> GeneratedMesh;
	
	UPROPERTY()
	TEnumAsByte<ESplineMeshAxis::Type> CachedForwardAxis;
	
	UPROPERTY()
	FSplineMeshParams CachedSplineParams;

	UPROPERTY()
	FVector CachedSplineUpDir;
	
	UPROPERTY()
	float CachedSplineBoundaryMax;
	
	UPROPERTY()
	float CachedSplineBoundaryMin;
	
	UPROPERTY()
	uint8 CachedbSmoothInterpRollScale:1;
};

//-----------------------------------------------------------------------------------------------------------------------------
// UTexture2D input
//-----------------------------------------------------------------------------------------------------------------------------
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInputTexture : public UHoudiniInputObject
{
	GENERATED_UCLASS_BODY()

public:

	//
	static UHoudiniInputObject* Create(UObject * InObject, UObject* InOuter, const FString& InName, const FHoudiniInputObjectSettings& InInputSettings);

	// Texture2D accessor
	class UTexture2D* GetTexture() const;
};
