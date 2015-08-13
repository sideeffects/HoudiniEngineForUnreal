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
#include "HoudiniAssetInstanceInputField.generated.h"


class UStaticMesh;
class UHoudiniAssetComponent;
struct FHoudiniGeoPartObject;
class UInstancedStaticMeshComponent;


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstanceInputField : public UObject
{
	friend class UHoudiniAssetInstanceInput;

	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetInstanceInputField();

public:

	/** Create an instance of input field. **/
	static UHoudiniAssetInstanceInputField* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
		const FHoudiniGeoPartObject& HoudiniGeoPartObject, const FString& InstancePathName);

	/** Create an instance of input field from another input field. **/
	static UHoudiniAssetInstanceInputField* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, 
		UHoudiniAssetInstanceInputField* OtherInputField);

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void BeginDestroy();

public:

	/** Return geo part object associated with this field. **/
	const FHoudiniGeoPartObject& GetHoudiniGeoPartObject() const;

	/** Return original static mesh. **/
	UStaticMesh* GetOriginalStaticMesh() const;

	/** Return associated static mesh. **/
	UStaticMesh* GetStaticMesh() const;

	/** Set static mesh. **/
	void SetStaticMesh(UStaticMesh* InStaticMesh);

	/** Set thumbnail border used by static mesh used by this field. **/
	void AssignThumbnailBorder(TSharedPtr<SBorder> InThumbnailBorder);

	/** Get thumbnail border used by static mesh used by this field. **/
	TSharedPtr<SBorder> GetThumbnailBorder() const;

	/** Set combo button used by static mesh used by this field. **/
	void AssignComboButton(TSharedPtr<SComboButton> InComboButton);

	/** Get combo button used by static mesh used by this field. **/
	TSharedPtr<SComboButton> GetComboButton() const;

	/** Get rotator used by this field. **/
	const FRotator& GetRotationOffset() const;

	/** Set rotation offset used by this field. **/
	void SetRotationOffset(const FRotator& Rotator);

	/** Get scale used by this field. **/
	const FVector& GetScaleOffset() const;

	/** Set scale used by this field. **/
	void SetScaleOffset(const FVector& InScale);

	/** Return true if all fields are scaled linearly. **/
	bool AreOffsetsScaledLinearly() const;

	/** Set whether offsets are scaled linearly. **/
	void SetLinearOffsetScale(bool bEnabled);

	/** Return true if original static mesh is used. **/
	bool IsOriginalStaticMeshUsed() const;

	/** Return corresponding instanced static mesh component. **/
	UInstancedStaticMeshComponent* GetInstancedStaticMeshComponent() const;

	/** Return transformations of all instances. **/
	const TArray<FTransform>& GetInstancedTransforms() const;

	/** Recreates render states for instanced static mesh component. **/
	void RecreateRenderState();

	/** Recreates physics states for instanced static mesh component. **/
	void RecreatePhysicsState();

protected:

	/** Create instanced component for this field. **/
	void CreateInstancedComponent();

	/** Set transforms for this field. **/
	void SetInstanceTransforms(const TArray<FTransform>& ObjectTransforms);

	/** Update relative transform for this field. **/
	void UpdateRelativeTransform();

	/** Update instance transformations. **/
	void UpdateInstanceTransforms();

protected:

#if WITH_EDITOR

	/** Thumbnail border used by slate for this field. **/
	TSharedPtr<SBorder> ThumbnailBorder;

	/** Combo box element used by slate for this field. **/
	TSharedPtr<SComboButton> StaticMeshComboButton;

#endif

	/** Original static mesh used by the instancer. **/
	UStaticMesh* OriginalStaticMesh;

	/** Current used static mesh. **/
	UStaticMesh* StaticMesh;

	/** Used instanced static mesh component. **/
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent;

	/** Parent Houdini asset component. **/
	UHoudiniAssetComponent* HoudiniAssetComponent;

	/** Transforms, one for each instance. **/
	TArray<FTransform> InstancedTransforms;

	/** Corresponding geo part object. **/
	FHoudiniGeoPartObject HoudiniGeoPartObject;

	/** Instance path name. **/
	FString InstancePathName;

	/** Rotation offset for instanced component. **/
	FRotator RotationOffset;

	/** Scale offset for instanced component. **/
	FVector ScaleOffset;

	/** Whether to scale linearly for all fields. **/
	bool bScaleOffsetsLinearly;

	/** Flags used by this input field. **/
	uint32 HoudiniAssetInstanceInputFieldFlagsPacked;
};


/** Functor used to sort instance input fields. **/
struct HOUDINIENGINERUNTIME_API FHoudiniAssetInstanceInputFieldSortPredicate
{
	bool operator()(const UHoudiniAssetInstanceInputField& A, const UHoudiniAssetInstanceInputField& B) const;
};
