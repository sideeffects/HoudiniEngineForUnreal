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
class UHoudiniAssetInput;
class AHoudiniAssetActor;
class UStaticMeshComponent;
class UHoudiniAssetParameter;
class UHoudiniAssetInstanceInput;

struct FPropertyChangedEvent;


namespace EHoudiniAssetComponentState
{
	/** This enumeration represents a state of component when it is serialized. **/

	enum Type
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


FORCEINLINE
FArchive&
operator<<(FArchive& Ar, EHoudiniAssetComponentState::Type& E)
{
	uint8 B = (uint8) E;
	Ar << B;

	if(Ar.IsLoading())
	{
		E = (EHoudiniAssetComponentState::Type) B;
	}

	return Ar;
}


UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class HOUDINIENGINE_API UHoudiniAssetComponent : public UPrimitiveComponent
{
	friend class FHoudiniMeshSceneProxy;
	friend class AHoudiniAssetActor;
	friend class FHoudiniAssetComponentDetails;

	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniAssetComponent();

public:

	/** Generated Static meshes used for rendering. **/
	UPROPERTY(VisibleInstanceOnly, EditFixedSize, NoClear, Transient, BlueprintReadOnly, Category=HoudiniAsset)
	TArray<UStaticMesh*> PreviewStaticMeshes;

	/** Houdini Asset associated with this component. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=HoudiniAsset)
	UHoudiniAsset* HoudiniAsset;

public:

	/** Change the Houdini Asset used by this component. **/
	virtual void SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset);

	/** Ticking function to check cooking / instatiation status. **/
	void TickHoudiniComponent();

	/** Ticking function used when asset is changed through proprety selection. **/
	void TickHoudiniAssetChange();

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

	/** Callback used by parameters to notify component about their changes. **/
	void NotifyParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter);

public:

	/** Locate static mesh by geo part object name. By default will use substring matching. **/
	bool LocateStaticMeshes(const FString& ObjectName, TMultiMap<FString, FHoudiniGeoPartObject>& InOutObjectsToInstance, bool bSubstring = true) const;

	/** Locate static mesh by geo part object id. **/
	bool LocateStaticMeshes(int ObjectToInstanceId, TArray<FHoudiniGeoPartObject>& InOutObjectsToInstance) const;

	/** Locate static mesh for a given geo part. **/
	UStaticMesh* LocateStaticMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const;

public: /** UObject methods. **/

	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreSave() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected: /** UActorComponent methods. **/

	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed() override;

private: /** USceneComponent methods. **/

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private: /** FEditorDelegates delegates. **/

	void OnPreSaveWorld(uint32 SaveFlags, class UWorld* World);
	void OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess);
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

	/** Refresh editor's detail panel and update properties. **/
	void UpdateEditorProperties();

	/** Start ticking. **/
	void StartHoudiniTicking();

	/** Stop ticking. **/
	void StopHoudiniTicking();

	/** Assign actor label based on asset instance name. **/
	void AssignUniqueActorLabel();

	/** Reset all Houdini related information, the asset, cooking trackers, generated geometry, related state, etc. **/
	void ResetHoudiniResources();

	/** Start delegate which is responsible for asset change. **/
	void StartHoudiniAssetChange();

	/** Stop delegate which is responsible for asset change. **/
	void StopHoudiniAssetChange();

	/** Create Static mesh resources. This will create necessary components for each mesh and update maps. **/
	void CreateObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap);

	/** Delete Static mesh resources. This will free static meshes and corresponding components. **/
	void ReleaseObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap);

	/** Create Static mesh resource which corresponds to Houdini logo. **/
	void CreateStaticMeshHoudiniLogoResource();

	/** Add Curve. **/
	bool AddAttributeCurve(const FHoudiniGeoPartObject& HoudiniGeoPartObject, TMap<FHoudiniGeoPartObject, USplineComponent*>& NewSplineComponents);

	/** Clear all spline related resources. **/
	void ClearAllCurves();

	/** Return parameter for a given node and param ids. Will return null if such parameter does not exist. **/
	UHoudiniAssetParameter* FindHoudiniAssetParameter(HAPI_NodeId NodeId, HAPI_ParmId ParmId) const;
	UHoudiniAssetParameter* FindHoudiniAssetParameter(uint32 HashValue) const;

	/** Remove parameter with a given hash node and param ids. **/
	void RemoveHoudiniAssetParameter(HAPI_NodeId NodeId, HAPI_ParmId ParmId);
	void RemoveHoudiniAssetParameter(uint32 HashValue);

	/** Create new parameters and attempt to reuse existing ones. **/
	bool CreateParameters();

	/** Clear all parameters. **/
	void ClearParameters();

	/** Upload changed parameters back to HAPI. **/
	void UploadChangedParameters();

	/** Create inputs. Number of inputs for asset does not change. **/
	void CreateInputs();

	/** Clear all inputs. **/
	void ClearInputs();

	/** Create instance inputs. **/
	void CreateInstanceInputs(const TArray<FHoudiniGeoPartObject>& Instancers);

	/** Clear all instance inputs. **/
	void ClearInstanceInputs();

	/** Serialize parameters. **/
	void SerializeParameters(FArchive& Ar);

private:

	/** This flag is used when Houdini engine is not initialized to display a popup message once. **/
	static bool bDisplayEngineNotInitialized;

protected:

	/** Parameters for this component's asset. **/
	TMap<uint32, UHoudiniAssetParameter*> Parameters;

	/** Inputs for this component's asset. **/
	TArray<UHoudiniAssetInput*> Inputs;

	/** Instance inputs for this component's asset. Object id is used as key. **/
	TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*> InstanceInputs;

	/** Map of HAPI objects and corresponding static meshes. Also map of static meshes and corresponding components. **/
	TMap<FHoudiniGeoPartObject, UStaticMesh*> StaticMeshes;
	TMap<UStaticMesh*, UStaticMeshComponent*> StaticMeshComponents;

	/** Map of curve / spline components. **/
	TMap<FHoudiniGeoPartObject, USplineComponent*> SplineComponents;

	/** Notification used by this component. **/
	TWeakPtr<SNotificationItem> NotificationPtr;

	/** GUID used to track asynchronous cooking requests. **/
	FGuid HapiGUID;

	/** Timer delegate, we use it for ticking during cooking or instantiation. **/
	FTimerDelegate TimerDelegateCooking;

	/** Timer delegate, used during asset change. This is necessary when asset is changed through properties. In this **/
	/** case we cannot update right away as it would require changing properties on which update was fired.			  **/
	FTimerDelegate TimerDelegateAssetChange;

	/** Used to store Houdini Asset when it is changing through a property action. **/
	UHoudiniAsset* ChangedHoudiniAsset;

	/** Id of corresponding Houdini asset. **/
	HAPI_AssetId AssetId;

	/** Used to delay notification updates for HAPI asynchronous work. **/
	double HapiNotificationStarted;

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
};

