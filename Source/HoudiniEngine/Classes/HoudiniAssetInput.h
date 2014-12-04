/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Brokers define connection between assets (for example in content
 * browser) and actors.
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
#include "HoudiniAssetInput.generated.h"


class UHoudiniSplineComponent;


namespace EHoudiniAssetInputType
{
	enum Enum
	{
		GeometryInput = 0,
		CurveInput
	};
}


UCLASS()
class HOUDINIENGINE_API UHoudiniAssetInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetInput();

public:

	/** Create sintance of this class. **/
	static UHoudiniAssetInput* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, int32 InInputIndex);

public:

	/** Called to destroy connected Houdini asset, if there's one. **/
	void DestroyHoudiniAssets();

public:

	/** Create this parameter from HAPI information - this implementation does nothing as this is not a true parameter. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue();

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:

	/** Return id of connected asset id. **/
	HAPI_AssetId GetConnectedAssetId() const;

	/* Return id of curve input asset. **/
	HAPI_AssetId GetCurveAssetId() const;

	/** Return id of geometry input asset. **/
	HAPI_AssetId GetGeometryAssetId() const;

	/** Return true if connected asset is a geometry asset. **/
	bool IsGeometryAssetConnected() const;

	/** Return true if connected asset is a curve asset. **/
	bool IsCurveAssetConnected() const;

	/** Disconnect connected input asset. **/
	void DisconnectInputAsset();

	/** Connect input asset. **/
	void ConnectInputAsset();

	/** Called by attached spline component whenever its state changes. **/
	void OnInputCurveChanged();

protected:

	/** Delegate used when static mesh has been drag and dropped. **/
	void OnStaticMeshDropped(UObject* InObject);

	/** Delegate used to detect if valid object has been dragged and dropped. **/
	bool OnStaticMeshDraggedOver(const UObject* InObject) const;

	/** Gets the border brush to show around thumbnails, changes when the user hovers on it. **/
	const FSlateBrush* GetStaticMeshThumbnailBorder() const;

	/** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
	FReply OnThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Construct drop down menu content for static mesh. **/
	TSharedRef<SWidget> OnGetStaticMeshMenuContent();

	/** Delegate for handling selection in content browser. **/
	void OnStaticMeshSelected(const FAssetData& AssetData);

	/** Closes the combo button. **/
	void CloseStaticMeshComboButton();

	/** Browse to static mesh. **/
	void OnStaticMeshBrowse();

	/** Handler for reset static mesh button. **/
	FReply OnResetStaticMeshClicked();

	/** Helper method used to generate choice entry widget. **/
	TSharedRef<SWidget> CreateChoiceEntryWidget(TSharedPtr<FString> ChoiceEntry);

	/** Called when change of selection is triggered. **/
	void OnChoiceChange(TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType);

	/** Called to retrieve the name of selected item. **/
	FString HandleChoiceContentText() const;

protected:

	/** Extract curve parameters and update the attached spline component. **/
	void UpdateInputCurve();

	/** Called to destroy connected geometry input and its asset. **/
	void DestroyGeometryInputAsset();

	/** Called to destroy connected curve input and its asset. **/
	void DestroyCurveInputAsset();

protected:

	/** Choice labels for this property. **/
	TArray<TSharedPtr<FString> > StringChoiceLabels;

	/** Thumbnail border used by static mesh. **/
	TSharedPtr<SBorder> StaticMeshThumbnailBorder;

	/** Combo element used by static mesh. **/
	TSharedPtr<SComboButton> StaticMeshComboButton;

	/** Delegate for filtering static meshes. **/
	FOnShouldFilterAsset OnShouldFilterStaticMesh;

	/** Widget used for dragging and input. **/
	TSharedPtr<SAssetSearchBox> InputWidget;

	/** Value of choice option. **/
	FString ChoiceStringValue;

	/** Object which is used for geometry input. **/
	UObject* InputObject;

	/** Houdini spline component which is used for curve input. **/
	UHoudiniSplineComponent* InputCurve;

	/** Id of geometry asset. **/
	HAPI_AssetId GeometryAssetId;

	/** Id of connected curve. **/
	HAPI_AssetId CurveAssetId;

	/** Id of currently connected asset. **/
	HAPI_AssetId ConnectedAssetId;

	/** Index of this input. **/
	int32 InputIndex;

	/** Choice selection. **/
	EHoudiniAssetInputType::Enum ChoiceIndex;

	/** Is set to true when static mesh used for geometry input has changed. **/
	bool bStaticMeshChanged;
};
