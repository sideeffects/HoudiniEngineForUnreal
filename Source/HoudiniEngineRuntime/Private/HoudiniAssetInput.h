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
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetInput.generated.h"


class UHoudiniSplineComponent;


namespace EHoudiniAssetInputType
{
	enum Enum
	{
		GeometryInput = 0,
		AssetInput,
		CurveInput
	};
}


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetInput();

public:

	/** Create intance of this class. **/
	static UHoudiniAssetInput* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, int32 InInputIndex);

public:

	/** Called to destroy connected Houdini asset, if there's one. **/
	//void DestroyHoudiniAssets();

public:

	/** Create this parameter from HAPI information - this implementation does nothing as this is not a true parameter. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

#if WITH_EDITOR

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder);

#endif

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue();

	/** Notifaction from a child parameter about its change. **/
	virtual void NotifyChildParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter);

/** UObject methods. **/
public:

	virtual void BeginDestroy();
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:

	/** Return id of connected asset id. **/
	HAPI_AssetId GetConnectedAssetId() const;

	/** Return true if connected asset is a geometry asset. **/
	bool IsGeometryAssetConnected() const;

	/** Return true if connected asset is a curve asset. **/
	bool IsCurveAssetConnected() const;

	/** Disconnect connected input asset. **/
	void DisconnectAndDestroyInputAsset();

	/** Called by attached spline component whenever its state changes. **/
	void OnInputCurveChanged();

protected:

#if WITH_EDITOR

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

#endif

	/** Called to retrieve the name of selected item. **/
	FText HandleChoiceContentText() const;

protected:

	/** Extract curve parameters and update the attached spline component. **/
	void UpdateInputCurve();

	/** Clear input curve parameters. **/
	void ClearInputCurveParameters();

	/** Destroy input curve object. **/
	void DestroyInputCurve();

	/** Create necessary resources for this input. **/
	void CreateWidgetResources();

protected:

	/** Parameters used by a curve input asset. **/
	TMap<FString, UHoudiniAssetParameter*> InputCurveParameters;

	/** Choice labels for this property. **/
	TArray<TSharedPtr<FString> > StringChoiceLabels;

#if WITH_EDITOR

	/** Thumbnail border used by static mesh. **/
	TSharedPtr<SBorder> StaticMeshThumbnailBorder;

	/** Combo element used by static mesh. **/
	TSharedPtr<SComboButton> StaticMeshComboButton;

	/** Delegate for filtering static meshes. **/
	FOnShouldFilterAsset OnShouldFilterStaticMesh;

#endif

	/** Value of choice option. **/
	FString ChoiceStringValue;

	/** Object which is used for geometry input. **/
	UObject* InputObject;

	/** Houdini spline component which is used for curve input. **/
	UHoudiniSplineComponent* InputCurve;

	/** Id of currently connected asset. **/
	HAPI_AssetId ConnectedAssetId;

	/** Index of this input. **/
	int32 InputIndex;

	/** Choice selection. **/
	EHoudiniAssetInputType::Enum ChoiceIndex;

	/** Flags used by this input. **/
	union
	{
		struct
		{
			/** Is set to true when static mesh used for geometry input has changed. **/
			uint32 bStaticMeshChanged : 1;

			/** Is set to true when choice switches to curve mode. **/
			uint32 bSwitchedToCurve : 1;

			/** Is set to true if this parameter has been loaded. **/
			uint32 bLoadedParameter : 1;
		};

		uint32 HoudiniAssetInputFlagsPacked;
	};
};
