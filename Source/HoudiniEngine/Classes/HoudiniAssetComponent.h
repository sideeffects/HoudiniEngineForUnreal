/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HAPI.h"
#include "IHoudiniTaskCookAssetInstanceCallback.h"
#include "HoudiniAssetComponent.generated.h"

class UClass;
class UMaterial;
class FTransform;
class UHoudiniAsset;
class UHoudiniAssetInstance;
class FPrimitiveSceneProxy;
class FComponentInstanceDataCache;

struct FPropertyChangedEvent;

UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class HOUDINIENGINE_API UHoudiniAssetComponent : public UMeshComponent, public IHoudiniTaskCookAssetInstanceCallback
{
	friend class FHoudiniMeshSceneProxy;

	GENERATED_UCLASS_BODY()

	/** **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = HoudiniAsset, ReplicatedUsing = OnRep_HoudiniAsset)
	UHoudiniAsset* HoudiniAsset;
	
	/** **/
	UFUNCTION()
	void OnRep_HoudiniAsset(UHoudiniAsset* OldHoudiniAsset);

	/** Change the Houdini asset used by this instance. **/
	UFUNCTION(BlueprintCallable, Category = "Components|HoudiniAsset")
	virtual bool SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset);
		
public:

	/** Used to differentiate native components from dynamic ones. **/
	void SetNative(bool InbIsNativeComponent);

public: /** IHoudiniTaskCookAssetInstanceCallback methods. **/

	void NotifyAssetInstanceCookingFailed(UHoudiniAssetInstance* HoudiniAssetInstance, HAPI_Result Result);
	void NotifyAssetInstanceCookingFinished(UHoudiniAssetInstance* HoudiniAssetInstance, HAPI_AssetId AssetId, const std::string& AssetInternalName);

public: /** UObject methods. **/

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;

protected: /** UActorComponent methods. **/

	virtual void OnComponentCreated() OVERRIDE;
	virtual void OnComponentDestroyed() OVERRIDE;

	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;

	virtual void GetComponentInstanceData(FComponentInstanceDataCache& Cache) const OVERRIDE;
	virtual void ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache) OVERRIDE;

	//virtual void BeginDestroy() OVERRIDE;
	//virtual void FinishDestroy() OVERRIDE;

private: /** UPrimitiveComponent methods. **/

	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;

private: /** UMeshComponent methods. **/

	virtual int32 GetNumMaterials() const OVERRIDE;

private: /** UsceneComponent methods. **/

	//virtual bool MoveComponent( const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit=NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags ) OVERRIDE;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const OVERRIDE;

protected:

	/** Patch RTTI : patch class information for this component's class based on given Houdini Asset. **/
	void ReplaceClassInformation();

private:

	/** Patch RTTI : translate asset parameters to class properties and insert them into a given class instance. **/
	bool ReplaceClassProperties(UClass* ClassInstance);

	/** Patch RTTI : Create integer property. **/
	UProperty* CreatePropertyInt(UClass* ClassInstance, const FName& Name, int Count, int32 Value, uint32& Offset);
	UProperty* CreatePropertyFloat(UClass* ClassInstance, const FName& Name, int Count, float Value, uint32& Offset);
	UProperty* CreatePropertyToggle(UClass* ClassInstance, const FName& Name, int Count, bool bValue, uint32& Offset);
	
protected:
	
	/** Triangle data used for rendering in viewport / preview window. **/
	TArray<FHoudiniMeshTriangle> HoudiniMeshTris;

	/** Bounding volume information for current geometry. **/
	FBoxSphereBounds HoudiniMeshSphereBounds;

	/** Instance of Houdini Asset created by this component. **/
	UHoudiniAssetInstance* HoudiniAssetInstance;
	
	/** Is set to true when this component is native and false is when it is dynamic. **/
	bool bIsNativeComponent;
	
	/** **/
	UMaterial* Material;

private:

	/** Marker ~ beginning of scratch space. **/
	uint64 ScratchSpaceMarker;

	/** Scratch space buffer ~ used to store data for each property. **/
	char ScratchSpaceBuffer[HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE];
};
