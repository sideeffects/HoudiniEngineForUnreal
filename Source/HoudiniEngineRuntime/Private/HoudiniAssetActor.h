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
#include "HoudiniAssetActor.generated.h"

class UHoudiniAssetComponent;

UCLASS(hidecategories=(Input), ConversionRoot, meta=(ChildCanTick))
class HOUDINIENGINERUNTIME_API AHoudiniAssetActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = HoudiniAssetActor, VisibleAnywhere, BlueprintReadOnly,
		meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|HoudiniAsset"))
	UHoudiniAssetComponent* HoudiniAssetComponent;

public:

	/** Return true if this actor is used for preview. **/
	bool IsUsedForPreview() const;

	/** Return actor component. **/
	UHoudiniAssetComponent* GetHoudiniAssetComponent() const;

	/** Reset actor's playtime. This time is used to set time and cook when in playmode. **/
	void ResetHoudiniCurrentPlaytime();

	/** Return actor's playtime. **/
	float GetHoudiniCurrentPlaytime() const;

/** AActor methods. **/
public:

	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR

	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) override;
	virtual bool ShouldExport() override;

#endif

protected:

	/** Used to track current playtime. **/
	float CurrentPlayTime;
};
