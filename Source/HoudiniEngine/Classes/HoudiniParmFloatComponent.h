#pragma once

#include "HoudiniParmFloatComponent.generated.h"

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UHoudiniParmFloatComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Houdini Parameter" )
	float Value;

private:

};