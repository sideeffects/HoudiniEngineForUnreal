/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HAPI.h"
#include "HoudiniAssetComponent.generated.h"


class UClass;
class UProperty;
class UMaterial;
class FTransform;
class UStaticMesh;
class UHoudiniAsset;
class UObjectProperty;
class USplineComponent;
class UPhysicalMaterial;
class UHoudiniAssetInput;
class AHoudiniAssetActor;
class UStaticMeshComponent;
class UHoudiniAssetParameter;
class UHoudiniSplineComponent;
class UHoudiniAssetInstanceInput;
class UFoliageType_InstancedStaticMesh;

struct FPropertyChangedEvent;
struct FWalkableSlopeOverride;


namespace EHoudiniAssetComponentState
{
	/** This enumeration represents a state of component when it is serialized. **/

	enum Enum
	{
		/** Invalid type corresponds to state when component has been created, but is in invalid state. It had no **/
		/** Houdini asset assigned. Typically this will be the case when component instance is a default class    **/
		/** object or belongs to an actor instance which is a default class object also.                          **/
		Invalid,

		/** None type corresponds to state when component has been created, but corresponding Houdini asset has not **/
		/** been instantiated.                                                                                      **/
		None,

		/** This type corresponds to a state when component has been created, corresponding Houdini asset has been **/
		/** instantiated, and component has no pending asynchronous cook request.                                  **/
		Instantiated,

		/** This type corresponds to a state when component has been created, corresponding Houdini asset has been **/
		/** instantiated, and component has a pending asynchronous cook in progress.                               **/
		BeingCooked
	};
}


UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew)
class HOUDINIENGINE_API UHoudiniAssetComponent : public UPrimitiveComponent
{
	friend class FHoudiniMeshSceneProxy;
	friend class AHoudiniAssetActor;
	friend class FHoudiniAssetComponentDetails;

	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniAssetComponent();

/** Static mesh generation properties.**/
public:

	/** If true, the physics triangle mesh will use double sided faces when doing scene queries. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Double Sided Geometry"))
	uint32 bGeneratedDoubleSidedGeometry : 1;

	/** Physical material to use for simple collision on this body. Encodes information about density, friction etc. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Simple Collision Physical Material"))
	UPhysicalMaterial* GeneratedPhysMaterial;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Collision Complexity"))
	TEnumAsByte<enum ECollisionTraceFlag> GeneratedCollisionTraceFlag;

	/** Resolution of lightmap. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Light Map Resolution", FixedIncrement="4.0"))
	int32 GeneratedLightMapResolution;

	/** Bias multiplier for Light Propagation Volume lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Lpv Bias Multiplier", UIMin="0.0", UIMax="3.0"))
	float GeneratedLpvBiasMultiplier;

	/** Custom walkable slope setting for generated mesh's body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Walkable Slope Override"))
	FWalkableSlopeOverride GeneratedWalkableSlopeOverride;

	/** The light map coordinate index. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Light map coordinate index"))
	int32 GeneratedLightMapCoordinateIndex;

	/** True if mesh should use a less-conservative method of mip LOD texture factor computation. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Use Maximum Streaming Texel Ratio"))
	uint32 bGeneratedUseMaximumStreamingTexelRatio:1;

	/** Allows artists to adjust the distance where textures using UV 0 are streamed in/out. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Streaming Distance Multiplier"))
	float GeneratedStreamingDistanceMultiplier;

	/** Default settings when using this mesh for instanced foliage. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Foliage Default Settings"))
	UFoliageType_InstancedStaticMesh* GeneratedFoliageDefaultSettings;

	/** Array of user data stored with the asset. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Asset User Data"))
	TArray<UAssetUserData*> GeneratedAssetUserData;

public:

	/** Change the Houdini Asset used by this component. **/
	virtual void SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset);

	/** Return true if this component has no cooking or instantiation in progress. **/
	bool IsNotCookingOrInstantiating() const;

	/** Ticking function to check cooking / instatiation status. **/
	void TickHoudiniComponent();

	/** Used to differentiate native components from dynamic ones. **/
	void SetNative(bool InbIsNativeComponent);

	/** Return id of a Houdini asset. **/
	UFUNCTION(BlueprintCallable, Category=HoudiniAsset)
	int32 GetAssetId() const;

	/** Set id of a Houdini asset. **/
	void SetAssetId(HAPI_AssetId InAssetId);

	/** Return current referenced Houdini asset. **/
	UHoudiniAsset* GetHoudiniAsset() const;

	/** Return owner Houdini actor. **/
	AHoudiniAssetActor* GetHoudiniAssetActorOwner() const;

	/** Return true if this component contains Houdini logo geometry. **/
	bool ContainsHoudiniLogoGeometry() const;

	/** Callback used by parameters to notify component that their value is about to change. **/
	void NotifyParameterWillChange(UHoudiniAssetParameter* HoudiniAssetParameter);

	/** Callback used by parameters to notify component about their changes. **/
	void NotifyParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter);

	/** Notification used by spline visualizer to notify main Houdini asset component about spline change. **/
	void NotifyHoudiniSplineChanged(UHoudiniSplineComponent* HoudiniSplineComponent);

	/** Assign generation parameters to static mesh. **/
	void SetStaticMeshGenerationParameters(UStaticMesh* StaticMesh);

	/** Refresh editor's detail panel and update properties. **/
	void UpdateEditorProperties();

	/** Return all static meshes used by this component. For both instanced and uinstanced components. **/
	void GetAllUsedStaticMeshes(TArray<UStaticMesh*>& UsedStaticMeshes);

public:

	/** Locate static mesh by geo part object name. By default will use substring matching. **/
	bool LocateStaticMeshes(const FString& ObjectName, TMultiMap<FString, FHoudiniGeoPartObject>& InOutObjectsToInstance, bool bSubstring = true) const;

	/** Locate static mesh by geo part object id. **/
	bool LocateStaticMeshes(int32 ObjectToInstanceId, TArray<FHoudiniGeoPartObject>& InOutObjectsToInstance) const;

	/** Locate static mesh for a given geo part. **/
	UStaticMesh* LocateStaticMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const;

	/** Locate geo part object for given static mesh. Reverse map search. **/
	FHoudiniGeoPartObject LocateGeoPartObject(UStaticMesh* StaticMesh) const;

/** UObject methods. **/
public:

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostInitProperties() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

/** UActorComponent methods. **/
protected:

	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

/** USceneComponent methods. **/
private:

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnUpdateTransform(bool bSkipPhysicsMove) override;

/** FEditorDelegates delegates. **/
private:

	void OnPIEEventBegin(const bool bIsSimulating);
	void OnPIEEventEnd(const bool bIsSimulating);

private:

	/** Subscribe to Editor events. **/
	void SubscribeEditorDelegates();

	/** Unsubscribe from Editor events. **/
	void UnsubscribeEditorDelegates();

	/** Computes number of inputs for Houdini asset. **/
	void ComputeInputCount();

	/** Update rendering information. **/
	void UpdateRenderingInformation();

	/** Start ticking. **/
	void StartHoudiniTicking();

	/** Stop ticking. **/
	void StopHoudiniTicking();

	/** Assign actor label based on asset instance name. **/
	void AssignUniqueActorLabel();

	/** Reset all Houdini related information, the asset, cooking trackers, generated geometry, related state, etc. **/
	void ResetHoudiniResources();

	/** Create Static mesh resources. This will create necessary components for each mesh and update maps. **/
	void CreateObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap);

	/** Delete Static mesh resources. This will free static meshes and corresponding components. **/
	void ReleaseObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap);

	/** Create Static mesh resource which corresponds to Houdini logo. **/
	void CreateStaticMeshHoudiniLogoResource(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap);

	/** Create default preset buffer. **/
	void CreateDefaultPreset();

	/** Create curves. **/
	void CreateCurves(const TArray<FHoudiniGeoPartObject>& Curves);

	/** Clear all spline related resources. **/
	void ClearCurves();

	/** Create new parameters and attempt to reuse existing ones. **/
	bool CreateParameters();

	/** Clear all parameters. **/
	void ClearParameters();

	/** Unmark all changed parameters. **/
	void UnmarkChangedParameters();

	/** Upload changed parameters back to HAPI. **/
	void UploadChangedParameters();

	/** Upload changed transformation back to HAPI. **/
	void UploadChangedTransform();

	/** If parameters were loaded, they need to be updated with proper ids after HAPI instantiation. **/
	void UpdateLoadedParameter();

	/** Create inputs. Number of inputs for asset does not change. **/
	void CreateInputs();

	/** Clear all inputs. **/
	void ClearInputs();

	/** If inputs were loaded, they need to be updated and assigned geos need to be connected. **/
	void UpdateLoadedInputs();

	/** If curves were loaded, their points need to be uploaded. **/
	void UploadLoadedCurves();

	/** Create instance inputs. **/
	void CreateInstanceInputs(const TArray<FHoudiniGeoPartObject>& Instancers);

	/** Clear all instance inputs. **/
	void ClearInstanceInputs();

	/** Serialize parameters. **/
	void SerializeParameters(FArchive& Ar);

	/** Serialize inputs. **/
	void SerializeInputs(FArchive& Ar);

	/** Serialize instance inputs. **/
	void SerializeInstanceInputs(FArchive& Ar);

	/** Serialize curves. **/
	void SerializeCurves(FArchive& Ar);

	/** Used to perform post loading initializtion of curve / spline components. **/
	void PostLoadCurves();

	/** Used to perform post loading initialization on instance inputs. **/
	void PostLoadInitializeInstanceInputs();

	/** Remove static mesh and associated component and deallocate corresponding resources. **/
	void RemoveStaticMeshComponent(UStaticMesh* StaticMesh);

	/** Start asset instantiation task. **/
	void StartTaskAssetInstantiation(bool bLoadedComponent = false, bool bStartTicking = false);

	/** Start asset deletion task. **/
	void StartTaskAssetDeletion();

	/** Start asset cooking task. **/
	void StartTaskAssetCooking(bool bStartTicking = false);

	/** Start manual asset cooking task. **/
	void StartTaskAssetCookingManual();

	/** Start manual asset reset task. **/
	void StartTaskAssetResetManual();

private:

	/** This flag is used when Houdini engine is not initialized to display a popup message once. **/
	static bool bDisplayEngineNotInitialized;

	/** This flag is used when Hapi version mismatch is detected (between defined and running versions. **/
	static bool bDisplayEngineHapiVersionMismatch;

public:

	/** Houdini Asset associated with this component. **/
	UHoudiniAsset* HoudiniAsset;

protected:

	/** Parameters for this component's asset, indexed by parameter name. **/
	TMap<FString, UHoudiniAssetParameter*> Parameters;

	/** Inputs for this component's asset. **/
	TArray<UHoudiniAssetInput*> Inputs;

	/** Instance inputs for this component's asset. Object id is used as key. **/
	TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*> InstanceInputs;

	/** Map of HAPI objects and corresponding static meshes. Also map of static meshes and corresponding components. **/
	TMap<FHoudiniGeoPartObject, UStaticMesh*> StaticMeshes;
	TMap<UStaticMesh*, UStaticMeshComponent*> StaticMeshComponents;

	/** Map of curve / spline components. **/
	TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*> SplineComponents;

	/** Buffer to hold preset data for serialization purposes. Used only during serialization. **/
	TArray<char> PresetBuffer;

	/** Buffer to hold default preset for reset purposes. **/
	TArray<char> DefaultPresetBuffer;

	/** Notification used by this component. **/
	TWeakPtr<SNotificationItem> NotificationPtr;

	/** GUID used to track asynchronous cooking requests. **/
	FGuid HapiGUID;

	/** Timer delegate, we use it for ticking during cooking or instantiation. **/
	FTimerDelegate TimerDelegateCooking;

	/** Timer delegate, used during asset change. This is necessary when asset is changed through properties. In this **/
	/** case we cannot update right away as it would require changing properties on which update was fired.			  **/
	FTimerDelegate TimerDelegateAssetChange;

	/** Id of corresponding Houdini asset. **/
	HAPI_AssetId AssetId;

	/** Used to delay notification updates for HAPI asynchronous work. **/
	double HapiNotificationStarted;

	/** Enables cooking for this Houdini Asset. **/
	bool bEnableCooking;

	/** Enables uploading of transformation changes back to Houdini Engine. **/
	bool bUploadTransformsToHoudiniEngine;

	/** Enables cooking upon transformation changes. **/
	bool bTransformChangeTriggersCooks;

	/** Is set to true when this component contains Houdini logo geometry. **/
	bool bContainsHoudiniLogoGeometry;

	/** Is set to true when this component is native and false is when it is dynamic. **/
	bool bIsNativeComponent;

	/** Is set to true when this component belongs to a preview actor. **/
	bool bIsPreviewComponent;

	/** Is set to true if this component has been loaded. **/
	bool bLoadedComponent;

	/** Is set to true when component is loaded and no instantiation / cooking is necessary. **/
	bool bLoadedComponentRequiresInstantiation;

	/** Is set to true when asset has been instantiated, but not cooked. **/
	bool bInstantiated;

	/** Is set to true when PIE mode is on (either play or simulate.) **/
	bool bIsPlayModeActive;

	/** Is set to true when one of the parameters for this component has been modified. This will trigger recook. **/
	bool bParametersChanged;

	/** Is set to true when curve information has changed and requires reuploading. This will trigger recook. **/
	bool bCurveChanged;

	/** Is set to true when transformation has changed, used for asset recooking on transformation change. **/
	bool bComponentTransformHasChanged;

	/** Is set to true when undo is being performed. **/
	bool bUndoRequested;

	/** Is set to true when manual recook is requested. **/
	bool bManualRecook;
};
