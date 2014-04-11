#pragma once

#include "HoudiniAssetComponent.generated.h"

USTRUCT()
struct FHoudiniParm
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	float FloatValue;
};

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

/** Structure storing Data for pre-computation */
USTRUCT()
struct FHoudiniAssetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 AssetId;

	UPROPERTY()
	FString AssetLibraryPath;

	TArray< FHoudiniMeshTriangle > HoudiniMeshTris;
};

/** Component Instance Data Cache */
class FHoudiniComponentInstanceData : public FComponentInstanceDataBase
{
public:
	static const FName InstanceDataTypeName;

	// Begin FComponentInstanceDataBase interface
	virtual FName GetDataTypeName() const OVERRIDE
	{
		return InstanceDataTypeName;
	}
	// End FComponentInstanceDataBase interface

	struct FHoudiniAssetData AssetData;
};

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UHoudiniAssetComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	void DoWork();
	int32 GetAssetId() const;

	UFUNCTION( BlueprintCallable, Category="Components|CustomMesh" )
	bool SetParmFloatValue( const FString& Name, float Value );

	UFUNCTION( BlueprintCallable, Category="Components|CustomMesh" )
	int32 InstantiateAssetFromPath( const FString& Path );

	UFUNCTION( BlueprintCallable, Category="Components|CustomMesh" )
	void SaveHIPFile( const FString& Path );

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Houdini Asset" )
	FString AssetLibraryPath;

private:

	void CookAsset( int32 asset_id );
	void SetChangedParms();

	// Begin UActorComponent interface.
	virtual void OnComponentCreated() OVERRIDE;
	virtual void OnComponentDestroyed() OVERRIDE;
	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;
	virtual void GetComponentInstanceData( FComponentInstanceDataCache& Cache ) const OVERRIDE;
	virtual void ApplyComponentInstanceData( const FComponentInstanceDataCache& Cache ) OVERRIDE;
	// End UActorComponent interface.

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const OVERRIDE;
	// End UMeshComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds( const FTransform & LocalToWorld ) const OVERRIDE;
	// Begin USceneComponent interface.

	/** */
	TArray< FHoudiniMeshTriangle > HoudiniMeshTris;
	TArray< FHoudiniParm > myChangedParms;
	int32 myAssetId;
	FString myAssetLibraryPath;
	bool myDataLoaded;

	friend class FHoudiniMeshSceneProxy;
};
