/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once
#include "HoudiniAssetParameter.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#if WITH_EDITOR
#include "Styling/SlateTypes.h"
#include "Input/Reply.h"
#endif
#include "HoudiniAssetInput.generated.h"

class ALandscape;
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
        WorldInput,
        SkeletonInput
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

    /** return true if the attached component's materials have been modified **/
    bool HasComponentMaterialsChanged() const;

    /** rebuilds the SplineTransform array after reloading the asset **/
    void RebuildSplineTransformsArrayIfNeeded();

    /** Indicates that the components used are no longer valid and should be updated from the actor **/
    bool NeedsComponentUpdate() const;

    /** Selected mesh's Actor, for reference. **/
    TWeakObjectPtr<AActor> ActorPtr = nullptr;
    /** Selected mesh's component, for reference. **/
    class UStaticMeshComponent * StaticMeshComponent = nullptr;

    /** The selected mesh. **/
    class UStaticMesh * StaticMesh = nullptr;

    /** Spline Component **/
    class USplineComponent * SplineComponent = nullptr;

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
    HAPI_NodeId AssetId = -1;
    
    /** TranformType used to generate the asset **/
    int32 KeepWorldTransform = 2;

    /** Temporary variable holding serialization version. **/
    uint32 HoudiniAssetParameterVersion;

    /** Path Materials assigned on the SMC **/
    TArray<FString> MeshComponentsMaterials;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetInput : public UHoudiniAssetParameter
{
    GENERATED_UCLASS_BODY()

    friend struct FHoudiniParameterDetails;

    public:

        /** Create instance of this class for use as an input **/
        static UHoudiniAssetInput * Create( UObject * InPrimaryObject, int32 InInputIndex, HAPI_NodeId InNodeId );
        /** Create instance of this class for use as parameter **/
        static UHoudiniAssetInput* Create(
            UObject * InPrimaryObject,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId,
            const HAPI_ParmInfo & ParmInfo);

        /** Create this parameter from HAPI information - this implementation does nothing as this is not a true parameter. **/
        virtual bool CreateParameter(
            UObject * InPrimaryObject,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId,
            const HAPI_ParmInfo & ParmInfo) override;

        /** Sets the default input type from the input/parameter label **/
        void SetDefaultInputTypeFromLabel();

#if WITH_EDITOR
        virtual void PostEditUndo() override;

	// Note: This method is to be only used for testing or for presetting Houdini tools input!!
        void ForceSetInputObject( UObject * InObject, int32 AtIndex, bool CommitChange );

	// Clears all selected objects for all input types and revert back to the default geo input
        void ClearInputs();

        bool AddInputObject( UObject* ObjectToAdd );
#endif

        /** Upload parameter value to HAPI. **/
        virtual bool UploadParameterValue() override;

        /** Notification from a child parameter about its change. **/
        virtual void NotifyChildParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter ) override;

        /** UObject methods. **/
        virtual void BeginDestroy() override;
        virtual void Serialize( FArchive & Ar ) override;
        virtual void PostLoad() override;
        static void AddReferencedObjects( UObject * InThis, FReferenceCollector& Collector );

        /** Return id of connected asset id. **/
        HAPI_NodeId GetConnectedAssetId() const;

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

        /** Changes the input type to the new one **/
        bool ChangeInputType(const EHoudiniAssetInputType::Enum& newType);

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

        /** Returns the selected landscape Actor **/
        const ALandscape* GetLandscapeInput() const;

        /** Remove a specific element of the world outliner input selection */
        void RemoveWorldOutlinerInput( int32 AtIndex );

        EHoudiniAssetInputType::Enum GetChoiceIndex() const { return ChoiceIndex; }

        FText GetCurrentSelectionText() const;

        // Return the bounds of this input
        FBox GetInputBounds( const FVector& ParentLocation );

        /** Return true if this parameter has been changed. **/
        bool HasChanged() const override;

        /** Retruns true if at least one object in this input has more than 1 LOD **/
        bool HasLODs() const;

        /** Retruns true if at least one object in this input has sockets **/
        bool HasSockets() const;

        /** Returns the input's transform scale values **/
        TOptional< float > GetPositionX( int32 AtIndex ) const;
        TOptional< float > GetPositionY( int32 AtIndex ) const;
        TOptional< float > GetPositionZ( int32 AtIndex ) const;

        /** Sets the input's transform scale values **/
        void SetPositionX( float Value, int32 AtIndex );
        void SetPositionY( float Value, int32 AtIndex );
        void SetPositionZ( float Value, int32 AtIndex );

        /** Returns the input's transform rotation values **/
        TOptional< float > GetRotationRoll(int32 AtIndex) const;
        TOptional< float > GetRotationPitch(int32 AtIndex) const;
        TOptional< float > GetRotationYaw(int32 AtIndex) const;

        /** Sets the input's transform rotation value **/
        void SetRotationRoll(float Value, int32 AtIndex);
        void SetRotationPitch(float Value, int32 AtIndex);
        void SetRotationYaw(float Value, int32 AtIndex);

        /** Returns the input's transform scale values **/
        TOptional< float > GetScaleX(int32 AtIndex) const;
        TOptional< float > GetScaleY(int32 AtIndex) const;
        TOptional< float > GetScaleZ(int32 AtIndex) const;

        /** Sets the input's transform scale values **/
        void SetScaleX(float Value, int32 AtIndex);
        void SetScaleY(float Value, int32 AtIndex);
        void SetScaleZ(float Value, int32 AtIndex);

#if WITH_EDITOR

        /** Called when change of input actor selection. **/
        void OnInputActorSelected( AActor * Actor );

        /** Called when change of landscape selection. **/
        void OnLandscapeActorSelected(AActor * Actor);

    protected:
        FReply OnExpandInputTransform( int32 AtIndex );

        /** Delegate used when static mesh has been drag and dropped. **/
        void OnStaticMeshDropped( UObject * InObject, int32 AtIndex );

        /** Handler for when static mesh thumbnail is double clicked. We open editor in this case. **/
        FReply OnThumbnailDoubleClick( const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent, int32 AtIndex );

        /** Browse to static mesh. **/
        void OnStaticMeshBrowse( int32 AtIndex );

        /** Handler for reset static mesh button. **/
        FReply OnResetStaticMeshClicked( int32 AtIndex );

        /** Delegate used when skeleton mesh has been drag and dropped. **/
        void OnSkeletalMeshDropped( UObject * InObject, int32 AtIndex );

        /** Browse to skeletal mesh. **/
        void OnSkeletalMeshBrowse( int32 AtIndex );

        /** Handler for reset skeletal mesh button. **/
        FReply OnResetSkeletalMeshClicked( int32 AtIndex );

        /** Called when change of selection is triggered. **/
        void OnChoiceChange( TSharedPtr< FString > NewChoice );

        /** Called on actor picker selection to filter the actors by type. **/
        bool OnShouldFilterActor( const AActor * const Actor ) const;

        /** Called when actor selection changed. **/
        void OnActorSelected(AActor * Actor);

        /** Called when change of World Outliner selection in Actor Picker. **/
        void OnWorldOutlinerActorSelected( AActor * Actor );

        /** Check if input Actors have had their Transforms changed. **/
        void TickWorldOutlinerInputs();

        /** Update WorldOutliners Transform after they changed **/
        void UpdateWorldOutlinerTransforms(FHoudiniAssetInputOutlinerMesh& OutlinerMesh);

        /** Update WorldOutliners Materials after they changed **/
        void UpdateWorldOutlinerMaterials(FHoudiniAssetInputOutlinerMesh& OutlinerMesh);

        /** Removes invalid inputs or updates inputs with invalid components in InputOutlinerArray. **/
        /** Returns true when a change was made **/
        bool UpdateInputOulinerArray();

        /** Update InputOutlinerArray with the components found on actor **/
        /** NeedCleanUp indicates that existing inputs from Actor needs to be removed **/
        void UpdateInputOulinerArrayFromActor( AActor * Actor, const bool& NeedCleanUp );

        /** Called to append a slot to the list of geometry input objects */
        void OnAddToInputObjects();
        /** Called to empty the list of geometry input objects */
        void OnEmptyInputObjects();
        /** Called to append a slot to the list of skeleton input objects */
        void OnAddToSkeletonInputObjects();
        /** Called to empty the list of skeleton input objects */
        void OnEmptySkeletonInputObjects();

#endif

        /** Called to retrieve the name of selected item. **/
        FText HandleChoiceContentText() const;

        /** Connect the input asset in Houdini. **/
        void ConnectInputAssetActor();

        /** Disconnect the input asset in Houdini. **/
        void DisconnectInputAssetActor();

        /** Connect the landscape asset in Houdini. **/
        void ConnectLandscapeActor();

        /** Disconnect the landscape asset in Houdini. **/
        void DisconnectLandscapeActor();

        /** Extract curve parameters and update the attached spline component. **/
        bool UpdateInputCurve();

        /** Clear input curve parameters. **/
        void ClearInputCurveParameters();

        /** Disconnect the input curve component but do not destroy the curve. **/
        void DisconnectInputCurve();

        /** Destroy input curve object. **/
        void DestroyInputCurve();

        /** Create necessary resources for this input. **/
        void CreateWidgetResources();

        /** Connects input node to asset or sets the object path parameter, returns true on success */
        bool ConnectInputNode();

        /** Updates the input's Object Merge Transform type  **/
        bool UpdateObjectMergeTransformType();

        /** Update's the input's object merge Pack before merging value **/
        bool UpdateObjectMergePackBeforeMerge();

        /** Returns the default value for this input Transform Type, 0 = none / 1 = IntoThisObject **/
        uint32 GetDefaultTranformTypeValue() const;

        /** Returns the geometry input object at index or nullptr */
        UObject* GetInputObject( int32 AtIndex ) const;

        /** Returns the skeleton input object at index or nullptr */
        UObject* GetSkeletonInputObject(int32 AtIndex) const;

        /** Returns the input transform at index or the identity transform */
        FTransform GetInputTransform( int32 AtIndex ) const;

        /** Returns primary object cast to HoudiniAssetComponent  */
        class UHoudiniAssetComponent* GetHoudiniAssetComponent();
        const class UHoudiniAssetComponent* GetHoudiniAssetComponent() const;

        /** Get the node id of the asset we are associated with */
        HAPI_NodeId GetAssetId() const;

        /** Parameters used by a curve input asset. **/
        TMap< FString, UHoudiniAssetParameter * > InputCurveParameters;

        /** Choice labels for this property. **/
        TArray< TSharedPtr< FString > > StringChoiceLabels;

#if WITH_EDITOR

        /** Check if state of landscape selection checkbox has changed. **/
        void CheckStateChangedExportOnlySelected( ECheckBoxState NewState );

        /** Return checked state of landscape selection checkbox. **/
        ECheckBoxState IsCheckedExportOnlySelected() const;

        /** Check if state of landscape auto selection checkbox has changed. **/
        void CheckStateChangedAutoSelectLandscape(ECheckBoxState NewState);

        /** Return checked state of landscape auto selection checkbox. **/
        ECheckBoxState IsCheckedAutoSelectLandscape() const;

        /** Check if state of landscape curves checkbox has changed. **/
        void CheckStateChangedExportCurves( ECheckBoxState NewState );

        /** Return checked state of landscape curves checkbox. **/
        ECheckBoxState IsCheckedExportCurves() const;

        /** Check if the state of the export landscape as mesh checkbox has changed. **/
        void CheckStateChangedExportAsMesh( ECheckBoxState NewState );

        /** Return checked state of the export landscape as mesh checkbox. **/
        ECheckBoxState IsCheckedExportAsMesh() const;

        /** Check if the state of the export landscape as heightfield checkbox has changed. **/
        void CheckStateChangedExportAsHeightfield( ECheckBoxState NewState );

        /** Return checked state of the export landscape as heightfield checkbox. **/
        ECheckBoxState IsCheckedExportAsHeightfield() const;

        /** Check if the state of the export landscape as points checkbox has changed. **/
        void CheckStateChangedExportAsPoints( ECheckBoxState NewState );

        /** Return checked state of the export landscape as points checkbox. **/
        ECheckBoxState IsCheckedExportAsPoints() const;

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
        void CheckStateChangedKeepWorldTransform( ECheckBoxState NewState );

        /** Return checked state of transform type checkbox. **/
        ECheckBoxState IsCheckedKeepWorldTransform() const;

        /** Check if state of the export LODs checkbox has changed. **/
        void CheckStateChangedExportAllLODs( ECheckBoxState NewState );

        /** Return checked state of export LODs checkbox. **/
        ECheckBoxState IsCheckedExportAllLODs() const;

        /** Check if state of the export sockets checkbox has changed. **/
        void CheckStateChangedExportSockets(ECheckBoxState NewState);

        /** Return checked state of export sockets checkbox. **/
        ECheckBoxState IsCheckedExportSockets() const;

        /** Handler for landscape recommit button. **/
        FReply OnButtonClickRecommit();

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

        /** Check if state of the transform type checkbox has changed. **/
        void CheckStateChangedPackBeforeMerge( ECheckBoxState NewState );

        /** Return checked state of landscape tile uv checkbox. **/
        ECheckBoxState IsCheckedPackBeforeMerge() const;

        /** Callbacks for geo array UI buttons */
        void OnInsertGeo( int32 AtIndex );
        void OnDeleteGeo( int32 AtIndex );
        void OnDuplicateGeo( int32 AtIndex );

#endif

        /** Value of choice option. **/
        FString ChoiceStringValue;

        /** Objects used for geometry input. **/
        TArray<UObject *> InputObjects;

        /** Houdini spline component which is used for curve input. **/
        class UHoudiniSplineComponent * InputCurve;

        /** Houdini asset component pointer of the input asset (actor). **/
        class UHoudiniAssetComponent * InputAssetComponent;

        /** Landscape actor used for input. **/
        class ALandscapeProxy * InputLandscapeProxy;

        /** List of selected meshes and actors from the World Outliner. **/
        TArray< FHoudiniAssetInputOutlinerMesh > InputOutlinerMeshArray;

        /** Objects used for skeletal mesh input. **/
        TArray<UObject *> SkeletonInputObjects;

        /** Id of currently connected asset. **/
        HAPI_NodeId ConnectedAssetId;

        /** The ids of the assets connected to the input for GeometryInput mode */
        /** or the ids of the volume connected to the input for Landscape mode (heightfield) */
        TArray< HAPI_NodeId > CreatedInputDataAssetIds;

        /** Index of this input. **/
        int32 InputIndex;

        /** Choice selection. **/
        EHoudiniAssetInputType::Enum ChoiceIndex;

        /** Timer handle, this timer is used for seeing if input Actors have changed. **/
        FTimerHandle WorldOutlinerTimerHandle;

        /** Timer delegate, we use it for ticking to see if input Actors have changed. **/
        FTimerDelegate WorldOutlinerTimerDelegate;

        float UnrealSplineResolution;

        /** Indicates that the OutlinerInputs have just been loaded and needs to be updated **/
        bool OutlinerInputsNeedPostLoadInit;

        /** Array containing the transform corrections for the assets in a geometry input **/
        TArray< FTransform > InputTransforms;

        /** Is the transform UI expanded ? **/
        TArray< bool > TransformUIExpanded;

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
                uint32 bLandscapeExportSelectionOnly : 1;

                /** Is set to true when landscape curves are to be exported. **/
                uint32 bLandscapeExportCurves : 1;

                /** Is set to true when the landscape is to be exported as a mesh, not just points. **/
                uint32 bLandscapeExportAsMesh : 1;

                /** Is set to true when materials are to be exported. **/
                uint32 bLandscapeExportMaterials : 1;

                /** Is set to true when lightmap information export is desired. **/
                uint32 bLandscapeExportLighting : 1;

                /** Is set to true when uvs should be exported in [0,1] space. **/
                uint32 bLandscapeExportNormalizedUVs : 1;

                /** Is set to true when uvs should be exported for each tile separately. **/
                uint32 bLandscapeExportTileUVs : 1;

                /** Is set to true when being used as an object-path parameter instead of an input */
                uint32 bIsObjectPathParameter : 1;

                /** Is set to true when this input's Transform Type is set to NONE, 2 will use the input's default value **/
                uint32 bKeepWorldTransform : 2;

                /** Is set to true when the landscape is to be exported as a heightfield **/
                uint32 bLandscapeExportAsHeightfield : 1;

                /** Is set to true when the automatic selection of landscape component is active **/
                uint32 bLandscapeAutoSelectComponent : 1;

                /** Indicates that the geometry must be packed before merging it into the input **/
                uint32 bPackBeforeMerge : 1;

                /** Indicates that all LODs in the input should be marshalled to Houdini **/
                uint32 bExportAllLODs : 1;

                /** Indicates that all sockets in the input should be marshalled to Houdini **/
                uint32 bExportSockets : 1;
            };

            uint32 HoudiniAssetInputFlagsPacked;
        };
};

/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive & operator<<(
    FArchive & Ar, FHoudiniAssetInputOutlinerMesh & HoudiniAssetInputOutlinerMesh );
