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


struct FGeometry;
struct FSlateBrush;
struct FPointerEvent;


class UStaticMesh;
class IDetailLayoutBuilder;
class UHoudiniAssetComponent;


class FHoudiniAssetComponentDetails : public IDetailCustomization
{
public:

	/** Constructor. **/
	FHoudiniAssetComponentDetails();

public: /** IDetailCustomization methods. **/

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:

	/** Create an instance of this detail layout class. **/
	static TSharedRef<IDetailCustomization> MakeInstance();

private:

	/** Button click handlers. **/	
	FReply OnButtonClickedBakeSingle();

private:

	/** Helper method used to create widgets for generated static meshes. **/
	void CreateStaticMeshAndMaterialWidgets(IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Helper method used to create a single static mesh. **/
	void CreateSingleStaticMesh();

	/** Gets the border brush to show around thumbnails, changes when the user hovers on it. **/
	const FSlateBrush* GetThumbnailBorder(UStaticMesh* StaticMesh) const;

	/** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
	FReply OnStaticMeshDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent, UStaticMesh* StaticMesh);

	/** Handler for bake individual static mesh action. **/
	FReply OnBakeStaticMesh(UStaticMesh* StaticMesh, UHoudiniAssetComponent* HoudiniAssetComponent);

	/** Handler for bake all static meshes action. **/
	FReply OnBakeAllStaticMeshes();

private:

	/** Components which are being customized. **/
	TArray<UHoudiniAssetComponent*> HoudiniAssetComponents;

	/** Map of static meshes and corresponding thumbnail borders. **/
	TMap<UStaticMesh*, TSharedPtr<SBorder> > StaticMeshThumbnailBorders;

	/** Whether baking option is enabled. **/
	bool bEnableBaking;
};
