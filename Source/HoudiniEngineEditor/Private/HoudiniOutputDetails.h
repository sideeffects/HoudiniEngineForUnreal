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

#include "CoreMinimal.h"
#include "ContentBrowserDelegates.h"
#include "Materials/MaterialInterface.h"
#include "Components/Border.h"
#include "Components/ComboBox.h"


class IDetailCategoryBuilder;
class FDetailWidgetRow;
class UHoudiniOutput;
class UHoudiniAssetComponent;
class FAssetThumbnailPool;
class ALandscapeProxy;
class USplineComponent;
class UHoudiniLandscapePtr;
class UHoudiniLandscapeEditLayer;
class UHoudiniStaticMesh;
class UMaterialInterface;
class AGeometryCollectionActor;
class SBorder;
class SComboButton;

struct FHoudiniGeoPartObject;
struct FHoudiniOutputObjectIdentifier;
struct FHoudiniOutputObject;

enum class EHoudiniOutputType : uint8;
enum class EHoudiniLandscapeOutputBakeType : uint8;

class FHoudiniOutputDetails : public TSharedFromThis<FHoudiniOutputDetails, ESPMode::NotThreadSafe>
{
public:
	void CreateWidget(
		IDetailCategoryBuilder& HouInputCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniOutput>>& InOutputs);

	void CreateMeshOutputWidget(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput);

	void CreateCurveOutputWidget(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput);

	void CreateGeometryCollectionOutputWidget(
        IDetailCategoryBuilder& HouOutputCategory,
        const TWeakObjectPtr<UHoudiniOutput>& InOutput);
	
	void CreateStaticMeshAndMaterialWidgets(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const TWeakObjectPtr<UStaticMesh>& StaticMesh,
		FHoudiniOutputObjectIdentifier& OutputIdentifier,
		const FString BakeFolder,
		FHoudiniGeoPartObject& HoudiniGeoPartObject,
		const bool& bIsProxyMeshCurrent);

	void CreateProxyMeshAndMaterialWidgets(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const TWeakObjectPtr<UHoudiniStaticMesh>& ProxyMesh,
		FHoudiniOutputObjectIdentifier& OutputIdentifier,
		const FString BakeFolder,
		FHoudiniGeoPartObject& HoudiniGeoPartObject);

	void CreateCurveWidgets(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const TWeakObjectPtr<USceneComponent>& SplineComponent,
		FHoudiniOutputObject& OutputObject,
		FHoudiniOutputObjectIdentifier& OutputIdentifier,
		FHoudiniGeoPartObject& HoudiniGeoPartObject);

	void CreateGeometryCollectionWidgets(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const TWeakObjectPtr<AGeometryCollectionActor>& GeometryCollectionActor,
		FHoudiniOutputObject& OutputObject,
		FHoudiniOutputObjectIdentifier& OutputIdentifier,
		FHoudiniGeoPartObject& HoudiniGeoPartObject);
	
	void CreateLandscapeOutputWidget(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput);

	void CreateLandscapeOutputWidget_Helper(
		IDetailCategoryBuilder & HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const FHoudiniGeoPartObject & HGPO,
		const TWeakObjectPtr<UHoudiniLandscapePtr>& LandscapePointer,
		const FHoudiniOutputObjectIdentifier & OutputIdentifier);

	void CreateLandscapeEditLayerOutputWidget_Helper(
		IDetailCategoryBuilder & HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const FHoudiniGeoPartObject & HGPO,
		const TWeakObjectPtr<UHoudiniLandscapeEditLayer>& LandscapeEditLayer,
		const FHoudiniOutputObjectIdentifier & OutputIdentifier);

	void CreateInstancerOutputWidget(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput);

	void CreateDefaultOutputWidget(
		IDetailCategoryBuilder& HouOutputCategory,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput);

	static FText GetOutputTooltip(const TWeakObjectPtr<UHoudiniOutput>& MainOutput);
	static FText GetOutputDebugName(const TWeakObjectPtr<UHoudiniOutput>& InOutput);
	static FText GetOutputDebugDescription(const TWeakObjectPtr<UHoudiniOutput>& InOutput);

	static void OnBakeNameCommitted(
		const FText& Val, ETextCommit::Type TextCommitType,
		const TWeakObjectPtr<UHoudiniOutput>& InOutput,
		const FHoudiniOutputObjectIdentifier & InIdentifier);

	static void OnRevertBakeNameToDefault(
		const TWeakObjectPtr<UHoudiniOutput>& InOutput, 
		const FHoudiniOutputObjectIdentifier & InIdentifier);

	static void OnBakeOutputObject(
		const FString& InBakeName,
		UObject * BakedOutputObject,
		const FHoudiniOutputObjectIdentifier & OutputIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const FHoudiniGeoPartObject & HGPO,
		const UObject* OutputOwner,
		const FString & BakeFolder,
		const FString & TempCookFolder,
		const EHoudiniOutputType & Type,
		const EHoudiniLandscapeOutputBakeType & LandscapeBakeType,
		const TArray<UHoudiniOutput*>& InAllOutputs);

	FReply OnRefineClicked(const TWeakObjectPtr<UObject> ObjectToRefine, const TWeakObjectPtr<UHoudiniOutput> InOutput);

	// Gets the border brush to show around thumbnails, changes when the user hovers on it.
	const FSlateBrush * GetThumbnailBorder(const TWeakObjectPtr<UObject> Mesh) const;
	const FSlateBrush * GetMaterialInterfaceThumbnailBorder(const TWeakObjectPtr<UObject> Mesh, int32 MaterialIdx) const;

	// Delegate used to detect if valid object has been dragged and dropped.
	bool OnMaterialInterfaceDraggedOver(TArrayView<FAssetData> InAssets) const;

	// Delegate used when a valid material has been drag and dropped on a mesh.
	void OnMaterialInterfaceDropped(
		const FDragDropEvent& InDragDropEvent,
		TArrayView<FAssetData> InAssets,
		const TWeakObjectPtr<UStaticMesh> InStaticMesh,
		const TWeakObjectPtr<UHoudiniOutput> InOutput,
		int32 MaterialIdx);

	void OnMaterialInterfaceDropped(
		const TWeakObjectPtr<UObject> InObject,
		const TWeakObjectPtr<UStaticMesh> InStaticMesh,
		const TWeakObjectPtr<UHoudiniOutput> InOutput,
		int32 MaterialIdx);

	// Delegate used when a valid material has been drag and dropped on a landscape.
	void OnMaterialInterfaceDropped(
		const FDragDropEvent& InDragDropEvent,
		TArrayView<FAssetData> InAssets,
		const TWeakObjectPtr<ALandscapeProxy> InLandscape,
		const TWeakObjectPtr<UHoudiniOutput> InOutput,
		int32 MaterialIdx);

	void OnMaterialInterfaceDropped(
		UObject* InObject,
		const TWeakObjectPtr<ALandscapeProxy> InLandscape,
		const TWeakObjectPtr<UHoudiniOutput> InOutput,
		int32 MaterialIdx);
	
	// Construct drop down menu content for material.
	TSharedRef<SWidget> OnGetMaterialInterfaceMenuContent(
		const TWeakObjectPtr<UMaterialInterface> MaterialInterface,
		const TWeakObjectPtr<UObject> OutputObject,
		const TWeakObjectPtr<UHoudiniOutput> InOutput,
		int32 MaterialIdx);

	// Delegate for handling selection in content browser.
	void OnMaterialInterfaceSelected(
		const FAssetData & AssetData,
		const TWeakObjectPtr<UObject> OutputObject,
		const TWeakObjectPtr<UHoudiniOutput> InOutput, 
		int32 MaterialIdx);

	// Delegate for handling Use CB selection arrow button clicked.
	void OnUseContentBrowserSelectedMaterialInterface(
		const TWeakObjectPtr<UObject> OutputObject,
		const TWeakObjectPtr<UHoudiniOutput> InOutput,
		int32 MaterialIdx);

	// Closes the combo button.
	void CloseMaterialInterfaceComboButton();

	// Browse to material interface.
	void OnBrowseTo(const TWeakObjectPtr<UObject> InObject);

	// Handler for reset material interface button.
	FReply OnResetMaterialInterfaceClicked(
		const TWeakObjectPtr<UStaticMesh> StaticMesh, const TWeakObjectPtr<UHoudiniOutput> InOutput, int32 MaterialIdx);

	FReply OnResetMaterialInterfaceClicked(
		const TWeakObjectPtr<ALandscapeProxy> InLandscape, const TWeakObjectPtr<UHoudiniOutput> InHoudiniOutput, int32 InMaterialIdx);

	// Handler for when static mesh thumbnail is double clicked. We open editor in this case.
	FReply OnThumbnailDoubleClick(
		const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent, const TWeakObjectPtr<UObject> Object);

	// Handler for bake individual static mesh action.
	// static FReply OnBakeStaticMesh(UStaticMesh * StaticMesh, UHoudiniAssetComponent * HoudiniAssetComponent, FHoudiniGeoPartObject& GeoPartObject);

private:

	// Map of meshes and corresponding thumbnail borders.
	TMap<TWeakObjectPtr<UObject>, TSharedPtr<SBorder>> OutputObjectThumbnailBorders;	
	// Map of meshes / material indices to thumbnail borders.
	TMap<TPair<TWeakObjectPtr<UObject>, int32>, TSharedPtr<SBorder>> MaterialInterfaceThumbnailBorders;
	// Map of meshes / material indices to combo elements.
	TMap<TPair<TWeakObjectPtr<UObject>, int32>, TSharedPtr<SComboButton>> MaterialInterfaceComboButtons;

	/** Delegate for filtering material interfaces. **/
	FOnShouldFilterAsset OnShouldFilterMaterialInterface;
};