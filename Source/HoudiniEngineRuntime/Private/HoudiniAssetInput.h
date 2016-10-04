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

class ALandscapeProxy;
class UHoudiniSplineComponent;
class USplineComponent;

namespace EHoudiniAssetInputType
{
    enum Enum
    {
        GeometryInput = 0,
        AssetInput,
        CurveInput,
        LandscapeInput,
        WorldInput
    };
}

struct HOUDINIENGINERUNTIME_API FHoudiniAssetInputOutlinerMesh
{
    /** Serialization. **/
    void Serialize( FArchive & Ar );

    /** return true if the attached spline component has been modified **/
    bool HasSplineComponentChanged(float fCurrentSplineResolution) const;

    /** return true if the attached actor's transform has been modified **/
    bool HasActorTransformChanged() const;

    /** return true if the attached component's transform has been modified **/
    bool HasComponentTransformChanged() const;
    
    /** rebuilds the SplineTransform array after reloading the asset **/
    void RebuildSplineTransformsArrayIfNeeded();

    /** Selected mesh's Actor, for reference. **/
    AActor * Actor = nullptr;

    /** Selected mesh's component, for reference. **/
    UStaticMeshComponent * StaticMeshComponent = nullptr;

    /** The selected mesh. **/
    UStaticMesh * StaticMesh = nullptr;

    /** Spline Component **/
    USplineComponent * SplineComponent = nullptr;

    /** Number of CVs used by the spline component, used to detect modification **/
    int32 NumberOfSplineControlPoints = -1;

    /** Transform of the UnrealSpline CVs, used to detect modification of the spline (Rotation/Scale) **/
    TArray<FTransform> SplineControlPointsTransform;

    /** Spline Length, used to detect modification of the spline.. **/
    float SplineLength = -1.0f;

    /** Spline resolution used to generate the asset, used to detect setting modification **/
    float SplineResolution = -1.0f;

    /** Actor transform used to see if the transfrom changed since last marshal into Houdini. **/
    FTransform ActorTransform;

    /** Component transform used to see if the transform has changed since last marshalling **/
    FTransform ComponentTransform;

    /** Mesh's input asset id. **/
    HAPI_AssetId AssetId = -1;
    
    /** TranformType used to generate the asset **/
    int32 KeepWorldTransform = 2;

    /** Temporary variable holding serialization version. **/
    uint32 HoudiniAssetParameterVersion;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetInput : public UHoudiniAssetParameter
{
    GENERATED_UCLASS_BODY()

    public:

        /** Destructor. **/
        virtual ~UHoudiniAssetInput();

    public:

        /** Create intance of this class. **/
        static UHoudiniAssetInput * Create( UHoudiniAssetComponent * InHoudiniAssetComponent, int32 InInputIndex );

    public:

        /** Create this parameter from HAPI information - this implementation does nothing as this is not a true parameter. **/
        virtual bool CreateParameter(
            UHoudiniAssetComponent * InHoudiniAssetComponent,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId,
            const HAPI_ParmInfo & ParmInfo);

#if WITH_EDITOR

        /** Create widget for this parameter and add it to a given category. **/
        virtual void CreateWidget( IDetailCategoryBuilder & DetailCategoryBuilder );
        /** Create a single geometry widget for the given input object */
        void CreateGeometryWidget( int32 AtIndex, UObject* InputObject, TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool, TSharedRef<SVerticalBox> VerticalBox );

        virtual void PostEditUndo() override;

#endif

        /** Upload parameter value to HAPI. **/
        virtual bool UploadParameterValue();

        /** Notifaction from a child parameter about its change. **/
        virtual void NotifyChildParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter );

    /** UObject methods. **/
    public:

        virtual void BeginDestroy();
        virtual void Serialize( FArchive & Ar ) override;
        virtual void PostLoad() override;
        static void AddReferencedObjects( UObject * InThis, FReferenceCollector& Collector );

    public:

        /** Return id of connected asset id. **/
        HAPI_AssetId GetConnectedAssetId() const;

        /** Return true if connected asset is a geometry asset. **/
        bool IsGeometryAssetConnected() const;

        /** Return true if connected asset is an instantiated (actor) asset. **/
        bool IsInputAssetConnected() const;

        /** Return true if connected asset is a curve asset. **/
        bool IsCurveAssetConnected() const;

        /** Return true if connected asset is a landscape asset. **/
        bool IsLandscapeAssetConnected() const;

        /** Return true if connected asset is a merge of a world outliner set of inputs. **/
        bool IsWorldInputAssetConnected() const;

        /** Disconnect connected input asset. **/
        void DisconnectAndDestroyInputAsset();

        /** Called by attached spline component whenever its state changes. **/
        void OnInputCurveChanged();

        /** Forces a disconnect of the input asset actor. This is used by external actors, usually when they die. **/
        void ExternalDisconnectInputAssetActor();

        /** See if we need to instantiate the input asset. **/
        bool DoesInputAssetNeedInstantiation();

        /** Get the HoudiniAssetComponent for the input asset. **/
        UHoudiniAssetComponent* GetConnectedInputAssetComponent();
        
        /** Invalidate all connected node ids */
        void InvalidateNodeIds();

        /** Duplicates the data from the input curve properly **/
        void DuplicateCurves(UHoudiniAssetInput * OriginalInput);

        FORCEINLINE const TArray< FHoudiniAssetInputOutlinerMesh >& GetWorldOutlinerInputs() const { return InputOutlinerMeshArray; }
        
        /** Remove a specific element of the world outliner input selection */
        void RemoveWorldOutlinerInput( int32 AtIndex );

        EHoudiniAssetInputType::Enum GetChoiceIndex() const { return ChoiceIndex; }

    protected:

#if WITH_EDITOR

        /** Delegate used when static mesh has been drag and dropped. **/
        void OnStaticMeshDropped( UObject * InObject, int32 AtIndex );

        /** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
        FReply OnThumbnailDoubleClick( const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent, int32 AtIndex );

        /** Browse to static mesh. **/
        void OnStaticMeshBrowse( int32 AtIndex );

        /** Handler for reset static mesh button. **/
        FReply OnResetStaticMeshClicked( int32 AtIndex );

        /** Helper method used to generate choice entry widget. **/
        TSharedRef< SWidget > CreateChoiceEntryWidget( TSharedPtr< FString > ChoiceEntry );

        /** Called when change of selection is triggered. **/
        void OnChoiceChange( TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType );

        /** Called on input actor selection to filter the actors by type. **/
        bool OnInputActorFilter( const AActor * const Actor ) const;

        /** Called when change of input actor selection. **/
        void OnInputActorSelected( AActor * Actor );

        /** Called when the input actor selection combo is closed. **/
        void OnInputActorCloseComboButton();

        /** Called when the input actor selection combo is used. **/
        void OnInputActorUse();

        /** Called when change of landscape selection. **/
        void OnLandscapeActorSelected( AActor * Actor );

        /** Called when change of World Outliner selection in Actor Picker. **/
        void OnWorldOutlinerActorSelected( AActor * Actor );

        /** Check if input Actors have had their Transforms changed. **/
        void TickWorldOutlinerInputs();

	/** Update WorldOutliners Transform after they changed **/
	void UpdateWorldOutlinerTransforms(FHoudiniAssetInputOutlinerMesh& OutlinerMesh);

        /** Called to append a slot to the list of input objects */
        void OnAddToInputObjects();
        /** Called to empty the list of input objects */
        void OnEmptyInputObjects();

#endif

        /** Called to retrieve the name of selected item. **/
        FText HandleChoiceContentText() const;

    protected:

        /** Connect the input asset in Houdini. **/
        void ConnectInputAssetActor();

        /** Disconnect the input asset in Houdini. **/
        void DisconnectInputAssetActor();

        /** Connect the landscape asset in Houdini. **/
        void ConnectLandscapeActor();

        /** Disconnect the landscape asset in Houdini. **/
        void DisconnectLandscapeActor();

        /** Extract curve parameters and update the attached spline component. **/
        void UpdateInputCurve();

        /** Clear input curve parameters. **/
        void ClearInputCurveParameters();

        /** Disconnect the input curve component but do not destroy the curve. **/
        void DisconnectInputCurve();

        /** Destroy input curve object. **/
        void DestroyInputCurve();

        /** Create necessary resources for this input. **/
        void CreateWidgetResources();
        

        /** Updates the input's Object Merge Transform type  **/
        bool UpdateObjectMergeTransformType();

        /** Returns the default value for this input Transform Type, 0 = none / 1 = IntoThisObject **/
        uint32 GetDefaultTranformTypeValue() const;

        /** Returns the input object at index or nullptr */
        UObject* GetInputObject( int32 AtIndex ) const;

    protected:

        /** Parameters used by a curve input asset. **/
        TMap< FString, UHoudiniAssetParameter * > InputCurveParameters;

        /** Choice labels for this property. **/
        TArray< TSharedPtr< FString > > StringChoiceLabels;

#if WITH_EDITOR

        /** Combo element used for input type selection. **/
        TSharedPtr< SComboBox< TSharedPtr< FString> > > InputTypeComboBox;

        /** Delegate for filtering static meshes. **/
        FOnShouldFilterAsset OnShouldFilterStaticMesh;

        /** Check if state of landscape selection checkbox has changed. **/
        void CheckStateChangedExportOnlySelected( ECheckBoxState NewState );

        /** Return checked state of landscape selection checkbox. **/
        ECheckBoxState IsCheckedExportOnlySelected() const;

        /** Check if state of landscape curves checkbox has changed. **/
        void CheckStateChangedExportCurves( ECheckBoxState NewState );

        /** Return checked state of landscape curves checkbox. **/
        ECheckBoxState IsCheckedExportCurves() const;

        /** Check if state of landscape full geometry checkbox has changed. **/
        void CheckStateChangedExportFullGeometry( ECheckBoxState NewState );

        /** Return checked state of landscape full geometry checkbox. **/
        ECheckBoxState IsCheckedExportFullGeometry() const;

        /** Check if state of landscape materials checkbox has changed. **/
        void CheckStateChangedExportMaterials( ECheckBoxState NewState );

        /** Return checked state of landscape materials checkbox. **/
        ECheckBoxState IsCheckedExportMaterials() const;

        /** Check if state of landscape lighting checkbox has changed. **/
        void CheckStateChangedExportLighting( ECheckBoxState NewState );

        /** Return checked state of landscape lighting checkbox. **/
        ECheckBoxState IsCheckedExportLighting() const;

        /** Check if state of landscape normalized uv checkbox has changed. **/
        void CheckStateChangedExportNormalizedUVs( ECheckBoxState NewState );

        /** Return checked state of landscape normalized uv checkbox. **/
        ECheckBoxState IsCheckedExportNormalizedUVs() const;

        /** Check if state of landscape tile uv checkbox has changed. **/
        void CheckStateChangedExportTileUVs( ECheckBoxState NewState );

        /** Return checked state of landscape tile uv checkbox. **/
        ECheckBoxState IsCheckedExportTileUVs() const;

	/** Check if state of the transform type checkbox has changed. **/
	void CheckStateChangedKeepWorldTransform(ECheckBoxState NewState);

	/** Return checked state of transform type checkbox. **/
	ECheckBoxState IsCheckedKeepWorldTransform() const;

        /** Handler for landscape recommit button. **/
        FReply OnButtonClickRecommit();

        /** Handler for World Outliner input selection button. **/
        FReply OnButtonClickSelectActors();

        /** Start world outliner Actor transform monitor ticking. **/
        void StartWorldOutlinerTicking();

        /** Stop world outliner Actor transform monitor ticking. **/
        void StopWorldOutlinerTicking();

        /** Set value of the SplineResolution for world outliners, used by Slate. **/
        void SetSplineResolutionValue(float InValue);

        /** Return value of the SplineResolution for world outliners. **/
        TOptional< float > GetSplineResolutionValue() const;

        /** Should the SplineResolution box be enabled?**/
        bool IsSplineResolutionEnabled() const;

        /** Reset the spline resolution to default **/
        FReply OnResetSplineResolutionClicked();

#endif

        /** Value of choice option. **/
        FString ChoiceStringValue;

        /** Objects used for geometry input. **/
        TArray<UObject *> InputObjects;

        /** Houdini spline component which is used for curve input. **/
        UHoudiniSplineComponent * InputCurve;

        /** Houdini asset component pointer of the input asset (actor). **/
        UHoudiniAssetComponent * InputAssetComponent;

        /** Landscape actor used for input. **/
        ALandscapeProxy * InputLandscapeProxy;

        /** List of selected meshes and actors from the World Outliner. **/
        TArray< FHoudiniAssetInputOutlinerMesh > InputOutlinerMeshArray;

        /** Id of currently connected asset. **/
        HAPI_AssetId ConnectedAssetId;

        /** The ids of the assets connected to the input for GeometryInput mode */
        TArray< HAPI_NodeId > GeometryInputAssetIds;

        /** Index of this input. **/
        int32 InputIndex;

        /** Choice selection. **/
        EHoudiniAssetInputType::Enum ChoiceIndex;

        /** Timer handle, this timer is used for seeing if input Actors have changed. **/
        FTimerHandle WorldOutlinerTimerHandle;

        /** Timer delegate, we use it for ticking to see if input Actors have changed. **/
        FTimerDelegate WorldOutlinerTimerDelegate;

        float UnrealSplineResolution;

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

                /** Is set to true if the asset input is actually connected inside Houdini. **/
                uint32 bInputAssetConnectedInHoudini : 1;

                /** Is set to true when landscape input is set to selection only. **/
                uint32 bLandscapeInputSelectionOnly : 1;

                /** Is set to true when landscape curves are to be exported. **/
                uint32 bLandscapeExportCurves : 1;

                /** Is set to true when full geometry needs to be exported. **/
                uint32 bLandscapeExportFullGeometry : 1;

                /** Is set to true when materials are to be exported. **/
                uint32 bLandscapeExportMaterials : 1;

                /** Is set to true when lightmap information export is desired. **/
                uint32 bLandscapeExportLighting : 1;

                /** Is set to true when uvs should be exported in [0,1] space. **/
                uint32 bLandscapeExportNormalizedUVs : 1;

                /** Is set to true when uvs should be exported for each tile separately. **/
                uint32 bLandscapeExportTileUVs : 1;

		/** Is set to true when this input's Transform Type is set to NONE, 2 will use the input's default value **/
		uint32 bKeepWorldTransform : 2;
            };

            uint32 HoudiniAssetInputFlagsPacked;
        };
};

/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive & operator<<(
    FArchive & Ar, FHoudiniAssetInputOutlinerMesh & HoudiniAssetInputOutlinerMesh );
