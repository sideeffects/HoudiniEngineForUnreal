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
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetInstanceInput.generated.h"


class AActor;
class UStaticMesh;
struct FHoudiniGeoPartObject;
class UInstancedStaticMeshComponent;
class UHoudiniAssetInstanceInputField;


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstanceInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetInstanceInput();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetInstanceInput* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
		const FHoudiniGeoPartObject& HoudiniGeoPartObject);

	/** Create instance from another input. **/
	static UHoudiniAssetInstanceInput* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetInstanceInput* InstanceInput);

public:

	/** Create this instance input. **/
	bool CreateInstanceInput();	

	/** Recreates render states for used instanced static mesh components. **/
	void RecreateRenderStates();

	/** Recreates physics states for used instanced static mesh components. **/
	void RecreatePhysicsStates();

	/** Update material for a given mesh and index. **/
	void UpdateStaticMeshMaterial(UStaticMesh* OtherStaticMesh, int32 MaterialIdx,
		UMaterialInterface* MaterialInterface);

/** UHoudiniAssetParameter methods. **/
public:

	/** Create this parameter from HAPI information - this implementation does nothing as this is not	**/
	/** a true parameter.																				**/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

	/** Set component for this parameter. **/
	virtual void SetHoudiniAssetComponent(UHoudiniAssetComponent* InHoudiniAssetComponent) override;

#if WITH_EDITOR

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder);    

#endif

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue();

/** UObject methods. **/
public:

	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:

	/** Return true if this is an attribute instancer. **/
	bool IsAttributeInstancer() const;

	/** Return true if this is an object instancer. Attribute instancer and object instancers are	**/
	/** not mutually exclusive.																		**/
	bool IsObjectInstancer() const;

#if WITH_EDITOR

	/** Clone all used instance static mesh components and and attach them to provided actor. **/
	void CloneComponentsAndAttachToActor(AActor* Actor);

#endif

protected:

	/** Retrieve all transforms for a given path. Used by attribute instancer. **/
	void GetPathInstaceTransforms(const FString& ObjectInstancePath, const TArray<FString>& PointInstanceValues,
		const TArray<FTransform>& Transforms, TArray<FTransform>& OutTransforms);

protected:

	/** Checks existance of special instance attribute for this instancer. **/
	static bool CheckInstanceAttribute(HAPI_AssetId AssetId, const FHoudiniGeoPartObject& GeoPartObject);

protected:

	/** Locate field which matches given criteria. Return null if not found. **/
	UHoudiniAssetInstanceInputField* LocateInputField(const FHoudiniGeoPartObject& GeoPartObject,
		const FString& InstancePathName);

	/** Locate or create (if it does not exist) an input field. **/
	void CreateInstanceInputField(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
		const TArray<FTransform>& ObjectTransforms, const FString& InstancePathName,
		const TArray<UHoudiniAssetInstanceInputField*>& OldInstanceInputFields, 
		TArray<UHoudiniAssetInstanceInputField*>& NewInstanceInputFields);

protected:

#if WITH_EDITOR

	/** Delegate used when static mesh has been drag and dropped. **/
	void OnStaticMeshDropped(UObject* InObject, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
		int32 Idx, int32 VariationIdx);

	/** Delegate used to detect if valid object has been dragged and dropped. **/
	bool OnStaticMeshDraggedOver(const UObject* InObject) const;

	/** Gets the border brush to show around thumbnails, changes when the user hovers on it. **/
	const FSlateBrush* GetStaticMeshThumbnailBorder(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
		int32 Idx, int32 VariationIdx) const;

	/** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
	FReply OnThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent, UObject* Object);

	/** Construct drop down menu content for static mesh. **/
	TSharedRef<SWidget> OnGetStaticMeshMenuContent(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
		int32 Idx, int32 VariationIdx);

	/** Delegate for handling selection in content browser. **/
	void OnStaticMeshSelected(const FAssetData& AssetData, 
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx, int32 VariationIdx);

	/** Closes the combo button. **/
	void CloseStaticMeshComboButton();

	/** Browse to static mesh. **/
	void OnStaticMeshBrowse(UStaticMesh* StaticMesh);

	/** Handler for reset static mesh button. **/
	FReply OnResetStaticMeshClicked(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, 
									int32 Idx, int32 VariationIdx);

	/** Handler for adding instance variation **/
	FReply OnAddInstanceVariation(UHoudiniAssetInstanceInputField * InstanceInputField, int32 Index);

	/** Handler for removing instance variation **/
	FReply OnRemoveInstanceVariation();

	/** Get rotation components for given index. **/
	TOptional<float> GetRotationRoll(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;
	TOptional<float> GetRotationPitch(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;
	TOptional<float> GetRotationYaw(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;

	/** Set rotation components for given index. **/
	void SetRotationRoll(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);
	void SetRotationPitch(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);
	void SetRotationYaw(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);

	/** Get scale components for a given index. **/
	TOptional<float> GetScaleX(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;
	TOptional<float> GetScaleY(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;
	TOptional<float> GetScaleZ(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;

	/** Set scale components for a given index. **/
	void SetScaleX(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);
	void SetScaleY(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);
	void SetScaleZ(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);

	/** Return true if given index must scale linearly. **/
	ECheckBoxState IsChecked(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const;

	/** Set option for whether scale should be linear. **/
	void CheckStateChanged(ECheckBoxState NewState, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField);

#endif

protected:

	/** List of fields created by this instance input. **/
	TArray<UHoudiniAssetInstanceInputField*> InstanceInputFields;

#if WITH_EDITOR

	/** Delegate for filtering static meshes. **/
	FOnShouldFilterAsset OnShouldFilterStaticMesh;
    

#endif

	/** Corresponding geo part object. **/
	FHoudiniGeoPartObject HoudiniGeoPartObject;

	/** Id of an object to instance. **/
	HAPI_ObjectId ObjectToInstanceId;

	/** Flags used by this input. **/
	union
	{
		struct
		{
			/* Set to true if this is an attribute instancer. **/
			uint32 bIsAttributeInstancer : 1;
		};

		uint32 HoudiniAssetInstanceInputFlagsPacked;
	};
};
