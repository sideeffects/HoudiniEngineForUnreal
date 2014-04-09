// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HoudiniAssetComponent.generated.h"

USTRUCT(BlueprintType)
struct FHoudiniMeshTriangle
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Triangle)
	FVector Vertex0;

	UPROPERTY(EditAnywhere, Category=Triangle)
	FVector Vertex1;

	UPROPERTY(EditAnywhere, Category=Triangle)
	FVector Vertex2;
};

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UHoudiniAssetComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Set the geometry to use on this triangle mesh */
	UFUNCTION(BlueprintCallable, Category="Components|CustomMesh")
	bool InstantiateAssetFromPath(const FString& Path);

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Asset Id" )
	int32 AssetId;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Asset Library Path" )
	FString AssetLibraryPath;

private:

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const OVERRIDE;
	// End UMeshComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// Begin USceneComponent interface.

	/** */
	TArray<FHoudiniMeshTriangle> HoudiniMeshTris;

	friend class FHoudiniMeshSceneProxy;
};
