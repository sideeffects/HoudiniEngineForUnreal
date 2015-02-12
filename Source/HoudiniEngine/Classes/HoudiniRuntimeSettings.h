/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Brokers define connection between assets (for example in content
 * browser) and actors.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HoudiniRuntimeSettings.generated.h"


class UAssetUserData;
class UPhysicalMaterial;
class UFoliageType_InstancedStaticMesh;


UCLASS(config = Engine, defaultconfig)
class HOUDINIENGINE_API UHoudiniRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniRuntimeSettings();

/** UObject methods. **/
public:

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

protected:

	/** Locate property of this class by name. **/
	UProperty* LocateProperty(const FString& PropertyName);

/** Cooking options. **/
public:

	// Enables cooking on parameter or input change for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Cooking)
	bool bEnableCooking;

	// Enables uploading of transformation changes back to Houdini Engine for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Cooking)
	bool bUploadTransformsToHoudiniEngine;

	// Enables cooking upon transformation changes for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Cooking)
	bool bTransformChangeTriggersCooks;

/** Collision generation. **/
public:

	// Group name prefix used for collision geometry generation. 
	UPROPERTY(GlobalConfig, EditAnywhere, Category=CollisionGeneration)
	FString CollisionGroupNamePrefix;

	// Group name prefix used for rendered collision geometry generation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=CollisionGeneration)
	FString RenderedCollisionGroupNamePrefix;

/** Geometry marshalling. **/
public:

	// Name of attribute used for marshalling Unreal tangents.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeometryMarshalling)
	FString MarshallingAttributeTangent;

	// Name of attribute used for marshalling Unreal binormals.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeometryMarshalling)
	FString MarshallingAttributeBinormal;

	// Name of attribute used for marshalling Unreal materials.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeometryMarshalling)
	FString MarshallingAttributeMaterial;

	// Name of attribute used for marshalling Unreal face smoothing masks.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeometryMarshalling)
	FString MarshallingAttributeFaceSmoothingMask;

/** Geometry scaling. **/

	// Scale factor of generated Houdini geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeometryScaling)
	float GeneratedGeometryScaleFactor;

	// Scale factor of Houdini transformations.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeometryScaling)
	float TransformScaleFactor;

/** Generated StaticMesh settings. **/
public:

	// If true, the physics triangle mesh will use double sided faces for new Houdini Assets when doing scene queries.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Double Sided Geometry"))
	uint32 bDoubleSidedGeometry : 1;

	// Physical material to use for simple collision of new Houdini Assets. Encodes information about density, friction etc.
	UPROPERTY(EditAnywhere, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Simple Collision Physical Material"))
	UPhysicalMaterial* PhysMaterial;

	//* Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate for new Houdini Assets.
	UPROPERTY(GlobalConfig, VisibleDefaultsOnly, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Collision Complexity"))
	TEnumAsByte<enum ECollisionTraceFlag> CollisionTraceFlag;

	// Resolution of lightmap for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Light Map Resolution", FixedIncrement="4.0"))
	int32 LightMapResolution;

	// Bias multiplier for Light Propagation Volume lighting for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Lpv Bias Multiplier", UIMin="0.0", UIMax="3.0"))
	float LpvBiasMultiplier;

	// Custom walkable slope setting for bodies of new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Walkable Slope Override"))
	FWalkableSlopeOverride WalkableSlopeOverride;

	// The light map coordinate index for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Light map coordinate index"))
	int32 LightMapCoordinateIndex;

	// True if mesh should use a less-conservative method of mip LOD texture factor computation for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Use Maximum Streaming Texel Ratio"))
	uint32 bUseMaximumStreamingTexelRatio:1;

	// Allows artists to adjust the distance where textures using UV 0 are streamed in/out for new Houdini Assets.
	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Streaming Distance Multiplier"))
	float StreamingDistanceMultiplier;

	// Default settings when using new Houdini Asset mesh for instanced foliage.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Foliage Default Settings"))
	UFoliageType_InstancedStaticMesh* FoliageDefaultSettings;

	// Array of user data stored with the new Houdini Asset.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=GeneratedStaticMeshSettings, meta=(DisplayName="Asset User Data"))
	TArray<UAssetUserData*> AssetUserData;

public:

#if WITH_EDITORONLY_DATA
#endif

};
