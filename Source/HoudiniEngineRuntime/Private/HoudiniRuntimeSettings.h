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

struct FRawMesh;
class UAssetUserData;
class UPhysicalMaterial;
struct FMeshBuildSettings;
struct FPropertyChangedEvent;
class UFoliageType_InstancedStaticMesh;

UENUM()
enum EHoudiniRuntimeSettingsSessionType
{
    // In process session.
    HRSST_InProcess UMETA( DisplayName = "In process" ),

    // TCP socket connection to Houdini Engine server.
    HRSST_Socket UMETA( DisplayName = "TCP socket" ),

    // Connection to Houdini Engine server via pipe connection.
    HRSST_NamedPipe UMETA( DisplayName = "Named pipe or domain socket" ),

    HRSST_MAX,
};

UENUM()
enum EHoudiniRuntimeSettingsRecomputeFlag
{
    // Recompute always.
    HRSRF_Always UMETA( DisplayName = "Always" ),

    // Recompute only if missing.
    HRSRF_OnlyIfMissing UMETA( DisplayName = "Only if missing" ),

    // Do not recompute.
    HRSRF_Nothing UMETA( DisplayName = "Never" ),

    HRSRF_MAX,
};

UENUM()
enum EHoudiniRuntimeSettingsAxisImport
{
    // Use Unreal coordinate system.
    HRSAI_Unreal UMETA( DisplayName = "Unreal" ),

    // Use Houdini coordinate system.
    HRSAI_Houdini UMETA( DisplayName = "Houdini" ),

    HRSAI_MAX,
};

UCLASS( config = Engine, defaultconfig )
class HOUDINIENGINERUNTIME_API UHoudiniRuntimeSettings : public UObject
{
    GENERATED_UCLASS_BODY()

    public:

        /** Destructor. **/
        virtual ~UHoudiniRuntimeSettings();

    /** UObject methods. **/
    public:

        virtual void PostInitProperties() override;

#if WITH_EDITOR

        virtual void PostEditChangeProperty( FPropertyChangedEvent & PropertyChangedEvent ) override;

#endif // WITH_EDITOR

    protected:

        /** Locate property of this class by name. **/
        UProperty * LocateProperty( const FString & PropertyName ) const;

        /** Make specified property read only. **/
        void SetPropertyReadOnly( const FString & PropertyName, bool bReadOnly = true );

#if WITH_EDITOR

        /** Update session ui elements. **/
        void UpdateSessionUi();

#endif // WITH_EDITOR

    public:

#if WITH_EDITOR

        /** Fill static mesh build settings structure based on assigned settings. **/
        void SetMeshBuildSettings( FMeshBuildSettings & MeshBuildSettings, FRawMesh & RawMesh ) const;

#endif // WITH_EDITOR

    public:

        /** Retrieve a string settings value. **/
        static bool GetSettingsValue( const FString & PropertyName, std::string & PropertyValue );
        static bool GetSettingsValue( const FString & PropertyName, FString & PropertyValue );

    /** Session options. **/
    public:

        /** Session Type: Change requires editor restart */
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Session )
        TEnumAsByte< enum EHoudiniRuntimeSettingsSessionType > SessionType;

        UPROPERTY( GlobalConfig, EditAnywhere, Category = Session )
        FString ServerHost;

        UPROPERTY( GlobalConfig, EditAnywhere, Category = Session )
        int32 ServerPort;

        UPROPERTY( GlobalConfig, EditAnywhere, Category = Session )
        FString ServerPipeName;

        /** Whether to automatically start a HARS process */
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Session )
        bool bStartAutomaticServer;

        UPROPERTY( GlobalConfig, EditAnywhere, Category = Session )
        float AutomaticServerTimeout;

    /** Instantiation options. **/
    public:

        // Whether to ask user to select an asset when instantiating an HDA with multiple assets inside. If disabled, will always instantiate first asset.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Instantiating )
        bool bShowMultiAssetDialog;

    /** Cooking options. **/
    public:

        // Enables cooking on parameter or input change for new Houdini Assets.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Cooking )
        bool bEnableCooking;

        // Enables uploading of transformation changes back to Houdini Engine for new Houdini Assets.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Cooking )
        bool bUploadTransformsToHoudiniEngine;

        // Enables cooking upon transformation changes for new Houdini Assets.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Cooking )
        bool bTransformChangeTriggersCooks;

        // Whether to display instantiation and cooking Slate notifications.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Cooking )
        bool bDisplaySlateCookingNotifications;

        // Curves will only cook on mouse release.
        UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
        bool bCookCurvesOnMouseRelease;

    /** Parameter options. **/
    public:

        // Will force treatment of ramp parameters as multiparms.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Parameters )
        bool bTreatRampParametersAsMultiparms;

    /** Collision generation. **/
    public:

        // Group name prefix used for collision geometry generation.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = CollisionGeneration )
        FString CollisionGroupNamePrefix;

        // Group name prefix used for rendered collision geometry generation.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = CollisionGeneration )
        FString RenderedCollisionGroupNamePrefix;

        // Group name prefix used for UCX collision geometry generation.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = CollisionGeneration)
        FString UCXCollisionGroupNamePrefix;

        // Group name prefix used for rendered UBX collision geometry generation.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = CollisionGeneration)
        FString UCXRenderedCollisionGroupNamePrefix;

	// Group name prefix used for simple collision geometry generation.
	// The type can be added after this: _box, _sphere, _capsule, _kdop10X, _kdop10Y, _kdop10Z, _kdop18, _kdop26 ...
	UPROPERTY( GlobalConfig, EditAnywhere, Category = CollisionGeneration)
	FString SimpleCollisionGroupNamePrefix;

	// Group name prefix used for rendered UBX collision geometry generation.
	// The type can be added after this: _box, _sphere, _capsule, _kdop10X, _kdop10Y, _kdop10Z, _kdop18, _kdop26 ...
	UPROPERTY( GlobalConfig, EditAnywhere, Category = CollisionGeneration)
	FString SimpleRenderedCollisionGroupNamePrefix;

    /** Geometry marshalling. **/
    public:

        // Name of attribute used for marshalling Unreal tangents.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeTangent;

        // Name of attribute used for marshalling Unreal binormals.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeBinormal;

        // Name of attribute used for marshalling Unreal materials.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeMaterial;

        // Name of attribute used for marshalling Unreal hole materials.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeMaterialHole;

        // Name of attribute used for marshalling Unreal instances.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeInstanceOverride;

        // Name of attribute used for marshalling Unreal face smoothing masks.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeFaceSmoothingMask;

        // Name of attribute used for marshalling light map resolution.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeLightmapResolution;

        // Name of attribute used to set generated mesh name.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeGeneratedMeshName;

        // Name of attribute set to the path of mesh asset inputs when marshalled to Houdini.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeInputMeshName;

        // Default resolution used when marshalling the Unreal Splines to HoudiniEngine (step in cm betweem CVs)
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        float MarshallingSplineResolution;

        // If true, generated Landscapes will be using full precision for their ZAxis, 
        // allowing for more precision but preventing them from being sculpted higher/lower than their min/max.
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        bool MarshallingLandscapesUseFullResolution;

    /** Geometry scaling. **/
    public:

        // Scale factor of generated Houdini geometry.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryScalingAndImport )
        float GeneratedGeometryScaleFactor;

        // Scale factor of Houdini transformations.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryScalingAndImport )
        float TransformScaleFactor;

        // Which coordinate system to use.
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryScalingAndImport )
        TEnumAsByte< enum EHoudiniRuntimeSettingsAxisImport > ImportAxis;

    /** Generated StaticMesh settings. **/
    public:

        // If true, the physics triangle mesh will use double sided faces for new Houdini Assets when doing scene queries.
        UPROPERTY(
            GlobalConfig, EditAnywhere, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Double Sided Geometry" ) )
        uint32 bDoubleSidedGeometry : 1;

        // Physical material to use for simple collision of new Houdini Assets. Encodes information about density, friction etc.
        UPROPERTY(
	    EditAnywhere, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Simple Collision Physical Material" ) )
        UPhysicalMaterial * PhysMaterial;

        //* Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, VisibleDefaultsOnly, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Collision Complexity" ) )
        TEnumAsByte< enum ECollisionTraceFlag > CollisionTraceFlag;

        // Resolution of lightmap for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, EditAnywhere, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Light Map Resolution", FixedIncrement = "4.0" ) )
        int32 LightMapResolution;

        // Bias multiplier for Light Propagation Volume lighting for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Lpv Bias Multiplier", UIMin = "0.0", UIMax = "3.0" ) )
        float LpvBiasMultiplier;

        /** Default Mesh distance field resolution, setting it to 0 will prevent the mesh distance field generation while editing the asset **/
        UPROPERTY(
            GlobalConfig, EditAnywhere, Category = GeneratedStaticMeshSettings,
            Meta = (DisplayName = "Distance Field Resolution Scale", UIMin = "0.0", UIMax = "100.0"))
            float GeneratedDistanceFieldResolutionScale;

        // Custom walkable slope setting for bodies of new Houdini Assets.
        UPROPERTY(
            GlobalConfig, EditAnywhere, AdvancedDisplay, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Walkable Slope Override" ) )
        FWalkableSlopeOverride WalkableSlopeOverride;

        // The light map coordinate index for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, EditAnywhere, AdvancedDisplay, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Light map coordinate index" ) )
        int32 LightMapCoordinateIndex;

        // True if mesh should use a less-conservative method of mip LOD texture factor computation for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, EditAnywhere, AdvancedDisplay, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Use Maximum Streaming Texel Ratio" ) )
        uint32 bUseMaximumStreamingTexelRatio:1;

        // Allows artists to adjust the distance where textures using UV 0 are streamed in/out for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, EditAnywhere, AdvancedDisplay, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Streaming Distance Multiplier" ) )
        float StreamingDistanceMultiplier;

        // Default settings when using new Houdini Asset mesh for instanced foliage.
        UPROPERTY(
	    EditAnywhere, AdvancedDisplay, Instanced, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Foliage Default Settings" ) )
        UFoliageType_InstancedStaticMesh * FoliageDefaultSettings;

        // Array of user data stored with the new Houdini Asset.
        UPROPERTY(
	    EditAnywhere, AdvancedDisplay, Instanced, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Asset User Data" ) )
        TArray< UAssetUserData * > AssetUserData;

    /** Static Mesh build settings. **/
    public:

        // If true, UVs will be stored at full floating point precision.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings )
        bool bUseFullPrecisionUVs;

        // Source UV set for lightmaps.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName = "Source Lightmap Index" ) )
        int32 SrcLightmapIndex;

        // Destination UV set for lightmap.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName = "Destination Lightmap Index" ) )
        int32 DstLightmapIndex;

        // Min lightmap resolution value.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings )
        int32 MinLightmapResolution;

        // If true, degenerate triangles will be removed.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings )
        bool bRemoveDegenerates;

        // Action to take when lightmaps are missing.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName = "Generate Lightmap UVs" ) )
        TEnumAsByte< enum EHoudiniRuntimeSettingsRecomputeFlag > GenerateLightmapUVsFlag;

        // Action to take when normals are missing.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName="Recompute Normals" ) )
        TEnumAsByte< enum EHoudiniRuntimeSettingsRecomputeFlag > RecomputeNormalsFlag;

        // Action to take when tangents are missing.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName="Recompute Tangents" ) )
        TEnumAsByte< enum EHoudiniRuntimeSettingsRecomputeFlag > RecomputeTangentsFlag;

    /** Custom Houdini location. **/
    public:

        // Whether to use custom Houdini location.
        UPROPERTY(
            GlobalConfig, EditAnywhere, Category = HoudiniLocation,
            Meta = ( DisplayName = "Use custom Houdini location (requires restart)" ) )
        bool bUseCustomHoudiniLocation;

        // Custom Houdini location (where HAPI library is located).
        UPROPERTY(
            GlobalConfig, EditAnywhere, Category = HoudiniLocation,
            Meta = ( DisplayName = "Custom Houdini location" ) )
        FDirectoryPath CustomHoudiniLocation;

};
