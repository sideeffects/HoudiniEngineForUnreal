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
#include "HoudiniGeoPartObject.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetComponent.generated.h"


class UClass;
class UProperty;
class UMaterial;
class UBlueprint;
class UStaticMesh;
class UHoudiniAsset;
class UObjectProperty;
class USplineComponent;
class UPhysicalMaterial;
class UHoudiniAssetInput;
class AHoudiniAssetActor;
class UHoudiniAssetHandle;
class UStaticMeshComponent;
class UHoudiniAssetParameter;
class UHoudiniHandleComponent;
class UHoudiniSplineComponent;
class UHoudiniAssetInstanceInput;
class UHoudiniAssetComponentMaterials;
class UFoliageType_InstancedStaticMesh;

struct FTransform;
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


UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"),
	ShowCategories=(Mobility), editinlinenew)
class HOUDINIENGINERUNTIME_API UHoudiniAssetComponent : public UPrimitiveComponent
{
	friend class AHoudiniAssetActor;
	friend struct FHoudiniEngineUtils;
	friend class FHoudiniMeshSceneProxy;
	friend class UHoudiniHandleComponent;
	friend class UHoudiniSplineComponent;

#if WITH_EDITOR

	friend class FHoudiniAssetComponentDetails;

#endif

	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniAssetComponent();

/** Static mesh generation properties.**/
public:

	/** If true, the physics triangle mesh will use double sided faces when doing scene queries. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Double Sided Geometry"))
	uint32 bGeneratedDoubleSidedGeometry : 1;

	/** Physical material to use for simple collision on this body. Encodes information about density, friction etc. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Simple Collision Physical Material"))
	UPhysicalMaterial* GeneratedPhysMaterial;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings, meta=(DisplayName="Collision Complexity"))
	TEnumAsByte<enum ECollisionTraceFlag> GeneratedCollisionTraceFlag;

	/** Resolution of lightmap. */
	UPROPERTY(EditAnywhere, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Light Map Resolution", FixedIncrement="4.0"))
	int32 GeneratedLightMapResolution;

	/** Bias multiplier for Light Propagation Volume lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Lpv Bias Multiplier", UIMin="0.0", UIMax="3.0"))
	float GeneratedLpvBiasMultiplier;

	/** Custom walkable slope setting for generated mesh's body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Walkable Slope Override"))
	FWalkableSlopeOverride GeneratedWalkableSlopeOverride;

	/** The light map coordinate index. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Light map coordinate index"))
	int32 GeneratedLightMapCoordinateIndex;

	/** True if mesh should use a less-conservative method of mip LOD texture factor computation. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Use Maximum Streaming Texel Ratio"))
	uint32 bGeneratedUseMaximumStreamingTexelRatio:1;

	/** Allows artists to adjust the distance where textures using UV 0 are streamed in/out. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Streaming Distance Multiplier"))
	float GeneratedStreamingDistanceMultiplier;

	/** Default settings when using this mesh for instanced foliage. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Foliage Default Settings"))
	UFoliageType_InstancedStaticMesh* GeneratedFoliageDefaultSettings;

	/** Array of user data stored with the asset. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=HoudiniGeneratedStaticMeshSettings,
		meta=(DisplayName="Asset User Data"))
	TArray<UAssetUserData*> GeneratedAssetUserData;

public:

	/** Change the Houdini Asset used by this component. **/
	virtual void SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset);

#if WITH_EDITOR

	/** Return true if this component has no cooking or instantiation in progress. **/
	bool IsInstantiatingOrCooking() const;

	/** Return true if this component's asset has been instantiated, but not cooked. **/
	bool HasBeenInstantiatedButNotCooked() const;

	/** Ticking function to check cooking / instatiation status. **/
	void TickHoudiniComponent();

	/** Ticking function to check whether UI update can be performed. This is necessary so that widget which has **/
	/** captured the mouse does not lose it. **/
	void TickHoudiniUIUpdate();

	/** Refresh editor's detail panel and update properties. **/
	void UpdateEditorProperties(bool bConditionalUpdate);

	/** Callback used by parameters to notify component that their value is about to change. **/
	void NotifyParameterWillChange(UHoudiniAssetParameter* HoudiniAssetParameter);

	/** Callback used by parameters to notify component about their changes. **/
	void NotifyParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter);

	/** Notification used by spline visualizer to notify main Houdini asset component about spline change. **/
	void NotifyHoudiniSplineChanged(UHoudiniSplineComponent* HoudiniSplineComponent);

	/** Assign generation parameters to static mesh. **/
	void SetStaticMeshGenerationParameters(UStaticMesh* StaticMesh);

	/** Used by Blueprint baking; create temporary actor and necessary components to bake a blueprint. **/
	AActor* CloneComponentsAndCreateActor();

	/** Return true if cooking is enabled for this component. **/
	bool IsCookingEnabled() const;

	/** Start asset instantiation task. **/
	void StartTaskAssetInstantiation(bool bLoadedComponent = false, bool bStartTicking = false);

	/** Start manual asset cooking task. **/
	void StartTaskAssetCookingManual();

#endif

	/** Used to differentiate native components from dynamic ones. **/
	void SetNative(bool InbIsNativeComponent);

	/** Return id of a Houdini asset. **/
	UFUNCTION(BlueprintCallable, Category=HoudiniAsset)
	int32 GetAssetId() const;

	/** Set id of a Houdini asset. **/
	void SetAssetId(HAPI_AssetId InAssetId);

	/** Return true if asset id is valid. **/
	bool HasValidAssetId() const;

	/** Return current referenced Houdini asset. **/
	UHoudiniAsset* GetHoudiniAsset() const;

	/** Return owner Houdini actor. **/
	AHoudiniAssetActor* GetHoudiniAssetActorOwner() const;

	/** Return true if this component contains Houdini logo geometry. **/
	bool ContainsHoudiniLogoGeometry() const;

	/** Return all static meshes used by this component. For both instanced and uinstanced components. **/
	void GetAllUsedStaticMeshes(TArray<UStaticMesh*>& UsedStaticMeshes);

	/** Return true if global setting scale factors are different from the ones used for this component. **/
	bool CheckGlobalSettingScaleFactors() const;

public:

	/** Locate static mesh by geo part object name. By default will use substring matching. **/
	bool LocateStaticMeshes(const FString& ObjectName,
		TMap<FString, TArray<FHoudiniGeoPartObject> >& InOutObjectsToInstance, bool bSubstring = true) const;

	/** Locate static mesh by geo part object id. **/
	bool LocateStaticMeshes(int32 ObjectToInstanceId, TArray<FHoudiniGeoPartObject>& InOutObjectsToInstance) const;

	/** Locate static mesh for a given geo part. **/
	UStaticMesh* LocateStaticMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const;

	/** Locate static mesh component for given static mesh. **/
	UStaticMeshComponent* LocateStaticMeshComponent(UStaticMesh* StaticMesh) const;

	/** Locate instanced static mesh components for given static mesh. **/
	bool LocateInstancedStaticMeshComponents(UStaticMesh* StaticMesh, TArray<UInstancedStaticMeshComponent*>& Components);

	/** Locate geo part object for given static mesh. Reverse map search. **/
	FHoudiniGeoPartObject LocateGeoPartObject(UStaticMesh* StaticMesh) const;

	/** Return true if this component is in playmode. **/
	bool IsPlayModeActive() const;

	/** Return component GUID. **/
	const FGuid& GetComponentGuid() const;

	/** If given material has been replaced for a given geo part object, return it. Otherwise return null. **/
	UMaterialInterface* GetReplacementMaterial(const FHoudiniGeoPartObject& HoudiniGeoPartObject, 
		const FString& MaterialName);

	/** If given material has been replaced for a given geo part object, return its name by reference. **/
	bool GetReplacementMaterialShopName(const FHoudiniGeoPartObject& HoudiniGeoPartObject, 
		UMaterialInterface* MaterialInterface, FString& MaterialName);

	/** Given a shop name return material assignment. **/
	UMaterial* GetAssignmentMaterial(const FString& MaterialName);

	/** Perform material replacement. **/
	bool ReplaceMaterial(const FHoudiniGeoPartObject& HoudiniGeoPartObject, UMaterialInterface* NewMaterialInterface,
		UMaterialInterface* OldMaterialInterface, int32 MaterialIndex);

	/** Remove material replacement. **/
	void RemoveReplacementMaterial(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const FString& MaterialName);

	/** Collect all Substance parameters. **/
	void CollectSubstanceParameters(TMap<FString, UHoudiniAssetParameter*>& SubstanceParameters) const;

	/** Collect all parameters of a given type. **/
	void CollectAllParametersOfType(UClass* ParameterClass, TMap<FString, UHoudiniAssetParameter*>& ClassParameters) const;

	/** Locate parameter by name. **/
	UHoudiniAssetParameter* FindParameter(const FString& ParameterName) const;

/** UObject methods. **/
public:

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;

#endif

	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
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
	virtual void OnUpdateTransform(bool bSkipPhysicsMove, ETeleportType Teleport) override;

private:

	/** Computes number of inputs for Houdini asset. **/
	void ComputeInputCount();

	/** Update rendering information. **/
	void UpdateRenderingInformation();

	/** Re-attach components after loading or copying. **/
	void PostLoadReattachComponents();

#if WITH_EDITOR

	/** Called after each cook. **/
	void PostCook(bool bCookError = false);

	/** Remove all attached components. **/
	void RemoveAllAttachedComponents();

	/** If we are being copied from a component, transfer necessary state. **/
	void OnComponentClipboardCopy(UHoudiniAssetComponent* HoudiniAssetComponent);

	/** Play mode delegate events. **/
	void OnPIEEventBegin(const bool bIsSimulating);
	void OnPIEEventEnd(const bool bIsSimulating);

	/** Delegate for asset post import. **/
	void OnAssetPostImport(UFactory* Factory, UObject* Object);

	/** Delegate to handle drag and drop events. **/
	void OnApplyObjectToActor(UObject* ObjectToApply, AActor* ActorToApplyTo);

	/** Subscribe to Editor events. **/
	void SubscribeEditorDelegates();

	/** Unsubscribe from Editor events. **/
	void UnsubscribeEditorDelegates();

	/** Start cooking / instantiation ticking. **/
	void StartHoudiniTicking();

	/** Stop cooking / instantiation ticking. **/
	void StopHoudiniTicking();

	/** Start UI update ticking. **/
	void StartHoudiniUIUpdateTicking();

	/** Stop UI update ticking. **/
	void StopHoudiniUIUpdateTicking();

	/** Assign actor label based on asset instance name. **/
	void AssignUniqueActorLabel();

	/** Reset all Houdini related information, the asset, cooking trackers, generated geometry, related state, etc. **/
	void ResetHoudiniResources();

	/** Start manual asset reset task. **/
	void StartTaskAssetResetManual();

	/** Start manual asset rebuild task. **/
	void StartTaskAssetRebuildManual();

	/** Start asset deletion task. **/
	void StartTaskAssetDeletion();

	/** Start asset cooking task. **/
	void StartTaskAssetCooking(bool bStartTicking = false);

	/** Create default preset buffer. **/
	void CreateDefaultPreset();

	/** Create curves. **/
	void CreateCurves(const TArray<FHoudiniGeoPartObject>& Curves);

	/** Create new parameters and attempt to reuse existing ones. **/
	bool CreateParameters();

	/** Create handles.**/
	bool CreateHandles();

	/** Unmark all changed parameters. **/
	void UnmarkChangedParameters();

	/** Upload changed parameters back to HAPI. **/
	void UploadChangedParameters();

	/** If parameters were loaded, they need to be updated with proper ids after HAPI instantiation. **/
	void UpdateLoadedParameters();

	/** Create inputs. Number of inputs for asset does not change. **/
	void CreateInputs();

	/** If inputs were loaded, they need to be updated and assigned geos need to be connected. **/
	void UpdateLoadedInputs();

	/** If curves were loaded, their points need to be uploaded. **/
	void UploadLoadedCurves();

	/** Create instance inputs. **/
	void CreateInstanceInputs(const TArray<FHoudiniGeoPartObject>& Instancers);

	/** Duplicate all parameters. Used during copying. **/
	void DuplicateParameters(UHoudiniAssetComponent* DuplicatedHoudiniComponent);

	/** Duplicate all handles. Used during copying. **/
	void DuplicateHandles(UHoudiniAssetComponent* DuplicatedHoudiniComponent);

	/** Duplicate inputs. Used during copying. **/
	void DuplicateInputs(UHoudiniAssetComponent* DuplicatedHoudiniComponent);

	/** Duplicate instance inputs. Used during copying. **/
	void DuplicateInstanceInputs(UHoudiniAssetComponent* DuplicatedHoudiniComponent);

#endif

	/** Clear all spline related resources. **/
	void ClearCurves();

	/** Clear all parameters. **/
	void ClearParameters();

	/** Clear handles. **/
	void ClearHandles();

	/** Clear all inputs. **/
	void ClearInputs();

	/** Clear all instance inputs. **/
	void ClearInstanceInputs();

	/** Inform downstream assets that we are dieing. **/
	void ClearDownstreamAssets();

	/** Delete Static mesh resources. This will free static meshes and corresponding components. **/
	void ReleaseObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap, 
		bool bDeletePackages = false);

	/** Return true if given object is referenced locally only, by objects generated and owned by this component. **/
	bool IsObjectReferencedLocally(UStaticMesh* StaticMesh, FReferencerInformationList& Referencers) const;

public:

	/** Add to the list of dependent downstream assets that have this asset as an asset input. **/
	void AddDownstreamAsset(UHoudiniAssetComponent* InDownstreamAssetComponent, int32 InInputIndex);

	/** Remove from the list of dependent downstream assets that have this asset as an asset input. **/
	void RemoveDownstreamAsset(UHoudiniAssetComponent* InDownstreamAssetComponent, int32 InInputIndex);

	/** Create Static mesh resources. This will create necessary components for each mesh and update maps. **/
	void CreateObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap);

	/** Delete Static mesh resources. This will free static meshes and corresponding components. **/
	void ReleaseObjectGeoPartResources(bool bDeletePackages = false);

	/** Create Static mesh resource which corresponds to Houdini logo. **/
	void CreateStaticMeshHoudiniLogoResource(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMesDhMap);

	/** Serialize inputs. **/
	void SerializeInputs(FArchive& Ar);

	/** Serialize instance inputs. **/
	void SerializeInstanceInputs(FArchive& Ar);

	/** Used to perform post loading initialization on instance inputs. **/
	void PostLoadInitializeInstanceInputs();

	/** Used to perform post loading initialization of parameters. **/
	void PostLoadInitializeParameters();

	/** Remove static mesh and associated component and deallocate corresponding resources. **/
	void RemoveStaticMeshComponent(UStaticMesh* StaticMesh);

private:

	/** This flag is used when Houdini engine is not initialized to display a popup message once. **/
	static bool bDisplayEngineNotInitialized;

	/** This flag is used when Hapi version mismatch is detected (between defined and running versions. **/
	static bool bDisplayEngineHapiVersionMismatch;

public:

	/** Houdini Asset associated with this component. **/
	UHoudiniAsset* HoudiniAsset;

protected:

	/** Previous asset, if it has been changed through transaction. **/
	UHoudiniAsset* PreviousTransactionHoudiniAsset;

	/** Parameters for this component's asset, indexed by parameter id. **/
	TMap<HAPI_ParmId, UHoudiniAssetParameter*> Parameters;

	/** Parameters for this component's asset, indexed by name for fast look up. **/
	TMap<FString, UHoudiniAssetParameter*> ParameterByName;

	/** Inputs for this component's asset. **/
	TArray<UHoudiniAssetInput*> Inputs;

	/** Instance inputs for this component's asset. Object id is used as key. **/
	TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*> InstanceInputs;

	/** List of dependent downstream asset connections that have this asset as an asset input. **/
	TMap<UHoudiniAssetComponent*, TSet<int32>> DownstreamAssetConnections;

	/** Map of HAPI objects and corresponding static meshes. Also map of static meshes and corresponding components. **/
	TMap<FHoudiniGeoPartObject, UStaticMesh*> StaticMeshes;
	TMap<UStaticMesh*, UStaticMeshComponent*> StaticMeshComponents;

	/** Map of asset handle components. **/
	typedef TMap<FString, UHoudiniHandleComponent*> FHandleComponentMap;
	FHandleComponentMap HandleComponents;

	/** Map of curve / spline components. **/
	TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*> SplineComponents;

	/** Material assignments. **/
	UHoudiniAssetComponentMaterials* HoudiniAssetComponentMaterials;

	/** Buffer to hold preset data for serialization purposes. Used only during serialization. **/
	TArray<char> PresetBuffer;

	/** Buffer to hold default preset for reset purposes. **/
	TArray<char> DefaultPresetBuffer;

#if WITH_EDITOR

	/** Notification used by this component. **/
	TWeakPtr<SNotificationItem> NotificationPtr;

	/** Component from which this component has been copied. **/
	UHoudiniAssetComponent* CopiedHoudiniComponent;

#endif

	/** Unique GUID created by component. **/
	FGuid ComponentGUID;

	/** GUID used to track asynchronous cooking requests. **/
	FGuid HapiGUID;

	/** Delegate handle returned by Begin play mode in editor delegate. **/
	FDelegateHandle DelegateHandleBeginPIE;

	/** Delegate handle returned by End play mode in editor delegate. **/
	FDelegateHandle DelegateHandleEndPIE;

	/** Delegate handle returned by editor asset post import delegate. **/
	FDelegateHandle DelegateHandleAssetPostImport;

	/** Delegate to handle editor viewport drag and drop events. **/
	FDelegateHandle DelegateHandleApplyObjectToActor;

	/** Timer handle, this timer is used for cooking. **/
	FTimerHandle TimerHandleCooking;

	/** Timer delegate, we use it for ticking during cooking or instantiation. **/
	FTimerDelegate TimerDelegateCooking;

	/** Timer handle, this timer is used for UI updates. **/
	FTimerHandle TimerHandleUIUpdate;

	/** Timer delegate, we use it for checking if details panel update can be performed. **/
	FTimerDelegate TimerDelegateUIUpdate;

	/** Id of corresponding Houdini asset. **/
	HAPI_AssetId AssetId;

	/** Scale factor used for generated geometry of this component. **/
	float GeneratedGeometryScaleFactor;

	/** Scale factor used for geo transforms of this component. **/
	float TransformScaleFactor;

	/** Import axis. **/
	EHoudiniRuntimeSettingsAxisImport ImportAxis;

	/** Used to delay notification updates for HAPI asynchronous work. **/
	double HapiNotificationStarted;

	/** Number of times this asset has been cooked. **/
	int32 AssetCookCount;

	/** Flags used by Houdini component. **/
	union
	{
		struct
		{
			/** Enables cooking for this Houdini Asset. **/
			uint32 bEnableCooking : 1;

			/** Enables uploading of transformation changes back to Houdini Engine. **/
			uint32 bUploadTransformsToHoudiniEngine : 1;

			/** Enables cooking upon transformation changes. **/
			uint32 bTransformChangeTriggersCooks : 1;

			/** Is set to true when this component contains Houdini logo geometry. **/
			uint32 bContainsHoudiniLogoGeometry : 1;

			/** Is set to true when this component is native and false is when it is dynamic. **/
			uint32 bIsNativeComponent : 1;

			/** Is set to true when this component belongs to a preview actor. **/
			uint32 bIsPreviewComponent : 1;

			/** Is set to true if this component has been loaded. **/
			uint32 bLoadedComponent : 1;

			/** Is set to true when PIE mode is on (either play or simulate.) **/
			uint32 bIsPlayModeActive : 1;

			/** Is set to true when component should set time and cook in play mode. **/
			uint32 bTimeCookInPlaymode : 1;

			/** Is set to true when Houdini materials are used. **/
			uint32 bUseHoudiniMaterials : 1;

			/** Is set to true when cooking this asset will trigger cooks of downstream connected assets. **/
			uint32 bCookingTriggersDownstreamCooks : 1;
		};

		uint32 HoudiniAssetComponentFlagsPacked;
	};

	/** Transient flags used by Houdini component. **/
	union
	{
		struct
		{
			/** Is set to true when component has been created as result of copying / import. **/
			uint32 bComponentCopyImported : 1;

			/** Is set when asset is changed during transaction. **/
			uint32 bTransactionAssetChange : 1;

			/** Is set to true when we are waiting for upstream input assets to instantiate. **/
			uint32 bWaitingForUpstreamAssetsToInstantiate : 1;

			/** Is set to true when one of the parameters has been modified. This will trigger recook. **/
			uint32 bParametersChanged : 1;

			/** Is set to true when transformation has changed, used for asset recooking on transformation change. **/
			uint32 bComponentTransformHasChanged : 1;

			/** Is set to true when component is loaded and requires instantiation. **/
			uint32 bLoadedComponentRequiresInstantiation : 1;
		};

		uint32 HoudiniAssetComponentTransientFlagsPacked;
	};

	/** Temporary variable holding component serialization version. **/
	uint32 HoudiniAssetComponentVersion;
};
