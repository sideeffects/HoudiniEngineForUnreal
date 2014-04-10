#pragma once

#include "HoudiniAssetComponent.h"
#include "HoudiniAssetActor.generated.h"

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS()
class AHoudiniAssetActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Houdini Asset" )
	FString AssetLibraryPath;

private:

	virtual void PostActorCreated() OVERRIDE;
	virtual void PostRegisterAllComponents() OVERRIDE;

	TSubobjectPtr< UHoudiniAssetComponent > myAssetComponent;
	int32 myAssetId;
};