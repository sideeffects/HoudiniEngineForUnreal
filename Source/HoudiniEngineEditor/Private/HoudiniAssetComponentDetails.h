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

#include "HoudiniGeoPartObject.h"


struct FGeometry;
struct FSlateBrush;
struct FPointerEvent;

class FAssetData;
class UStaticMesh;
class IDetailLayoutBuilder;
class UHoudiniAssetComponent;


/** Hashing function for our pair. **/
uint32 GetTypeHash(TPair<UStaticMesh*, int32> Pair);


class FHoudiniAssetComponentDetails : public IDetailCustomization
{
public:

	/** Constructor. **/
	FHoudiniAssetComponentDetails();

	/** Destructor. **/
	virtual ~FHoudiniAssetComponentDetails();

/** IDetailCustomization methods. **/
public:

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:

	/** Create an instance of this detail layout class. **/
	static TSharedRef<IDetailCustomization> MakeInstance();

private:

	/** Helper method used to create widgets for generated static meshes. **/
	void CreateStaticMeshAndMaterialWidgets(IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Helper method used to create widget for Houdini asset. **/
	void CreateHoudiniAssetWidget(IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Gets the border brush to show around thumbnails, changes when the user hovers on it. **/
	const FSlateBrush* GetStaticMeshThumbnailBorder(UStaticMesh* StaticMesh) const;
	const FSlateBrush* GetMaterialInterfaceThumbnailBorder(UStaticMesh* StaticMesh, int32 MaterialIdx) const;

	/** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
	FReply OnThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent, UObject* Object);

	/** Handler for bake individual static mesh action. **/
	FReply OnBakeStaticMesh(UStaticMesh* StaticMesh, UHoudiniAssetComponent* HoudiniAssetComponent);

	/** Handler for bake all static meshes action. **/
	FReply OnBakeAllStaticMeshes();

	/** Handler for recook action. **/
	FReply OnRecookAsset();

	/** Handler for rebuild action. **/
	FReply OnRebuildAsset();

	/** Handler for reset action. **/
	FReply OnResetAsset();

	/** Handler for fetch log action. **/
	FReply OnFetchCookLog();

	/** Handler for bake blueprint action. **/
	FReply OnBakeBlueprint();

	/** Handler for bake blueprint replace action. **/
	FReply OnBakeBlueprintReplace();

	/** Handler for fetch asset help action. **/
	FReply OnFetchAssetHelp(UHoudiniAssetComponent* HoudiniAssetComponent);

	/** Delegate used to detect if valid object has been dragged and dropped. **/
	bool OnMaterialInterfaceDraggedOver(const UObject* InObject) const;

	/** Delegate used when valid material has been drag and dropped. **/
	void OnMaterialInterfaceDropped(UObject* InObject, UStaticMesh* StaticMesh,
		FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx);

	/** Construct drop down menu content for material. **/
	TSharedRef<SWidget> OnGetMaterialInterfaceMenuContent(UMaterialInterface* MaterialInterface,
		UStaticMesh* StaticMesh, FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx);

	/** Delegate for handling selection in content browser. **/
	void OnMaterialInterfaceSelected(const FAssetData& AssetData, UStaticMesh* StaticMesh,
		FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx);

	/** Closes the combo button. **/
	void CloseMaterialInterfaceComboButton();

	/** Browse to material interface. **/
	void OnMaterialInterfaceBrowse(UMaterialInterface* MaterialInterface);

	/** Handler for reset material interface button. **/
	FReply OnResetMaterialInterfaceClicked(UStaticMesh* StaticMesh, FHoudiniGeoPartObject* HoudiniGeoPartObject,
		int32 MaterialIdx);

	/** Delegate used when Houdini asset has been drag and dropped. **/
	void OnHoudiniAssetDropped(UObject* InObject);

	/** Delegate used to detect if valid object has been dragged and dropped. **/
	bool OnHoudiniAssetDraggedOver(const UObject* InObject) const;

	/** Gets the border brush to show around thumbnails, changes when the user hovers on it. **/
	const FSlateBrush* GetHoudiniAssetThumbnailBorder() const;

	/** Construct drop down menu content for Houdini asset. **/
	TSharedRef<SWidget> OnGetHoudiniAssetMenuContent();

	/** Delegate for handling selection in content browser. **/
	void OnHoudiniAssetSelected(const FAssetData& AssetData);

	/** Closes the combo button. **/
	void CloseHoudiniAssetComboButton();

	/** Browse to Houdini asset. **/
	void OnHoudiniAssetBrowse();

	/** Handler for reset Houdini asset button. **/
	FReply OnResetHoudiniAssetClicked();

	/** Checks whether checkbox is checked. **/
	ECheckBoxState
		IsCheckedComponentSettingCooking(UHoudiniAssetComponent* HoudiniAssetComponent) const;

	ECheckBoxState
		IsCheckedComponentSettingUploadTransform(UHoudiniAssetComponent* HoudiniAssetComponent) const;

	ECheckBoxState
		IsCheckedComponentSettingTransformCooking(UHoudiniAssetComponent* HoudiniAssetComponent) const;

	ECheckBoxState
		IsCheckedComponentSettingCookInPlaymode(UHoudiniAssetComponent* HoudiniAssetComponent) const;

	ECheckBoxState
		IsCheckedComponentSettingUseHoudiniMaterials(UHoudiniAssetComponent* HoudiniAssetComponent) const;

	ECheckBoxState
		IsCheckedComponentSettingCookingTriggersDownstreamCooks(UHoudiniAssetComponent* HoudiniAssetComponent) const;

	/** Handle change in Checkbox. **/
	void CheckStateChangedComponentSettingCooking(ECheckBoxState NewState,
		UHoudiniAssetComponent* HoudiniAssetComponent);

	void CheckStateChangedComponentSettingUploadTransform(ECheckBoxState NewState,
		UHoudiniAssetComponent* HoudiniAssetComponent);

	void CheckStateChangedComponentSettingTransformCooking(ECheckBoxState NewState,
		UHoudiniAssetComponent* HoudiniAssetComponent);

	void CheckStateChangedComponentSettingCookInPlaymode(ECheckBoxState NewState,
		UHoudiniAssetComponent* HoudiniAssetComponent);

	void CheckStateChangedComponentSettingUseHoudiniMaterials(ECheckBoxState NewState,
		UHoudiniAssetComponent* HoudiniAssetComponent);

	void CheckStateChangedComponentSettingCookingTriggersDownstreamCooks(ECheckBoxState NewState,
		UHoudiniAssetComponent* HoudiniAssetComponent);

private:

	/** Components which are being customized. **/
	TArray<UHoudiniAssetComponent*> HoudiniAssetComponents;

	/** Map of static meshes and corresponding thumbnail borders. **/
	TMap<UStaticMesh*, TSharedPtr<SBorder> > StaticMeshThumbnailBorders;

	/** Map of static meshes / material indices to combo elements. **/
	TMap<TPair<UStaticMesh*, int32>, TSharedPtr<SComboButton> > MaterialInterfaceComboButtons;

	/** Map of static meshes / material indices to thumbnail borders. **/
	TMap<TPair<UStaticMesh*, int32>, TSharedPtr<SBorder> > MaterialInterfaceThumbnailBorders;

	/** Delegate for filtering material interfaces. **/
	FOnShouldFilterAsset OnShouldFilterMaterialInterface;

	/** Thumbnail border used by Houdini asset. **/
	TSharedPtr<SBorder> HoudiniAssetThumbnailBorder;

	/** Combo element used by Houdini asset. **/
	TSharedPtr<SComboButton> HoudiniAssetComboButton;

	/** Delegate for filtering Houdini assets. **/
	FOnShouldFilterAsset OnShouldFilterHoudiniAsset;

	/** Whether baking option is enabled. **/
	bool bEnableBaking;
};
