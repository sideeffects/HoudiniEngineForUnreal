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
#include "HoudiniAssetComponent.generated.h"

class UHoudiniAsset;

//UCLASS(HeaderGroup=Component, ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), dependson=ULightmassPrimitiveSettingsObject, editinlinenew, meta=(BlueprintSpawnableComponent))
UCLASS(HeaderGroup=Component, ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class HOUDINIENGINE_API UHoudiniAssetComponent : public UMeshComponent
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

private: /** UActorComponent methods. **/

	virtual void OnComponentCreated() OVERRIDE;
	virtual void OnComponentDestroyed() OVERRIDE;

	virtual void GetComponentInstanceData(FComponentInstanceDataCache& Cache) const OVERRIDE;
	virtual void ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache) OVERRIDE;

private: /** UPrimitiveComponent methods. **/

	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;

private: /** UMeshComponent methods. **/

	virtual int32 GetNumMaterials() const OVERRIDE;

private: /** UsceneComponent methods. **/

	//virtual FBoxSphereBounds CalcBounds( const FTransform & LocalToWorld ) const OVERRIDE;

private:

	TArray<FHoudiniMeshTriangle> HoudiniMeshTris;
};
