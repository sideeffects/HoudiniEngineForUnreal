/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once
#include <string>
#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "PhysicsEngine/BodySetup.h"
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
    HRSST_InProcess UMETA( Hidden ),

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

UENUM()
enum class EHoudiniToolType : uint8
{
    // For tools that generates geometry, and do not need input
    HTOOLTYPE_GENERATOR UMETA( DisplayName = "Generator" ),

    // For tools that have a single input, the selection will be merged in that single input
    HTOOLTYPE_OPERATOR_SINGLE UMETA( DisplayName = "Operator (single)" ),

    // For Tools that have multiple input, a single selected asset will be applied to each input
    HTOOLTYPE_OPERATOR_MULTI UMETA( DisplayName = "Operator (multiple)" ),

    // For tools that needs to be applied each time for each single selected
    HTOOLTYPE_OPERATOR_BATCH UMETA( DisplayName = "Batch Operator" )
};

UENUM()
enum class EHoudiniToolSelectionType : uint8
{
    // For tools that can be applied both to Content Browser and World selection
    HTOOL_SELECTION_ALL UMETA( DisplayName = "Content Browser AND World" ),

    // For tools that can be applied only to World selection
    HTOOL_SELECTION_WORLD_ONLY UMETA( DisplayName = "World selection only" ),

    // For tools that can be applied only to Content Browser selection
    HTOOL_SELECTION_CB_ONLY UMETA( DisplayName = "Content browser selection only" )
};

USTRUCT( BlueprintType )
struct FHoudiniToolDescription
{
    GENERATED_USTRUCT_BODY()

    /** Name of the tool */
    UPROPERTY(Category=Tool, EditAnywhere)
    FString Name;

    /** Type of the tool */
    UPROPERTY(Category = Tool, EditAnywhere)
    EHoudiniToolType Type;

    /** Selection Type of the tool */
    UPROPERTY(Category = Tool, EditAnywhere)
    EHoudiniToolSelectionType SelectionType;

    /** Tooltip shown on mouse hover */
    UPROPERTY( Category = Tool, EditAnywhere )
    FString ToolTip;

    /** Path to a custom icon */
    UPROPERTY( Category = Tool, EditAnywhere, meta = ( FilePathFilter = "png" ) )
    FFilePath IconPath;

    /** Houdini uasset */
    UPROPERTY(Category = Tool, EditAnywhere)
    TSoftObjectPtr < class UHoudiniAsset > HoudiniAsset;

    /** Houdini hda file path */
    FFilePath AssetPath;

    /** Clicking on help icon will bring up this URL */
    UPROPERTY( Category = Tool, EditAnywhere )
    FString HelpURL;
};


USTRUCT(BlueprintType)
struct FHoudiniToolDirectory
{
    GENERATED_USTRUCT_BODY()

    /** Name of the tool directory */
    UPROPERTY(Category = Tool, EditAnywhere)
    FString Name;

    /** Path of the tool directory */
    UPROPERTY(Category = Tool, EditAnywhere)
    FDirectoryPath Path;
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

        // Whether houdini engine cooking is paused or not upon initializing the plugin
        UPROPERTY( GlobalConfig, EditAnywhere, Category = Cooking )
        bool bPauseCookingOnStart;

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

        // Content folder storing all the temporary cook data
        UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
        FText TemporaryCookFolder;

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

        // Name of attribute set to the asset's source file path for inputs when marshalled to Houdini.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = GeometryMarshalling )
        FString MarshallingAttributeInputSourceFile;

        // Default resolution used when marshalling the Unreal Splines to HoudiniEngine (step in cm between CVs)
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        float MarshallingSplineResolution;

        // If true, generated Landscapes will be marshalled using default unreal scaling. 
        // Generated landscape will loose a lot of precision on the Z axis but will use the same transforms
        // as Unreal's default landscape
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        bool MarshallingLandscapesUseDefaultUnrealScaling;

        // If true, generated Landscapes will be using full precision for their ZAxis, 
        // allowing for more precision but preventing them from being sculpted higher/lower than their min/max.
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        bool MarshallingLandscapesUseFullResolution;

        // If true, the min/max values used to convert heightfields to landscape will be forced values
        // This is usefull when importing multiple landscapes from different HDAs
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        bool MarshallingLandscapesForceMinMaxValues;
        // The minimum value to be used for Landscape conversion when MarshallingLandscapesForceMinMaxValues is enabled
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        float MarshallingLandscapesForcedMinValue;
        // The maximum value to be used for Landscape conversion when MarshallingLandscapesForceMinMaxValues is enabled
        UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
        float MarshallingLandscapesForcedMaxValue;

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

        //* Default properties of the body instance
        UPROPERTY(EditAnywhere, Category = GeneratedStaticMeshSettings, meta = ( FullyExpand = "true" ))
        struct FBodyInstance DefaultBodyInstance;

        //* Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate for new Houdini Assets.
        UPROPERTY(
            GlobalConfig, VisibleDefaultsOnly, Category = GeneratedStaticMeshSettings,
            Meta = ( DisplayName = "Collision Complexity" ) )
        TEnumAsByte< enum ECollisionTraceFlag > CollisionTraceFlag;

        // Resolution of lightmap for baked lighting.
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

        // The UV coordinate index of lightmap 
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

        // Source UV set for generated lightmap.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName = "Source Lightmap Index" ) )
        int32 SrcLightmapIndex;

        // Destination UV set for generated lightmap.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName = "Destination Lightmap Index" ) )
        int32 DstLightmapIndex;

        // Target lightmap resolution to for generated lightmap.  Determines the padding between UV shells in a packed lightmap.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings )
        int32 MinLightmapResolution;

        // If true, degenerate triangles will be removed.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings )
        bool bRemoveDegenerates;

        // Lightmap UV generation
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName = "Generate Lightmap UVs" ) )
        TEnumAsByte< enum EHoudiniRuntimeSettingsRecomputeFlag > GenerateLightmapUVsFlag;

        // Normals generation
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName="Recompute Normals" ) )
        TEnumAsByte< enum EHoudiniRuntimeSettingsRecomputeFlag > RecomputeNormalsFlag;

        // Tangents generation
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName="Recompute Tangents" ) )
        TEnumAsByte< enum EHoudiniRuntimeSettingsRecomputeFlag > RecomputeTangentsFlag;

        // If true, recomputed tangents and normals will be calculated using MikkT Space.  This method does require properly laid out UVs though otherwise you'll get a degenerate tangent warning
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings, Meta = ( DisplayName="Generate Using MikkT Space" ) )
        bool bUseMikkTSpace;

        // Required for PNT tessellation but can be slow. Recommend disabling for larger meshes.
        UPROPERTY( GlobalConfig, EditAnywhere, Category = StaticMeshBuildSettings )
        bool bBuildAdjacencyBuffer;

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

    /** Custom Houdini Tools **/
    public:
        /** Don't add Houdini Tools to Placement Editor Mode */
        UPROPERTY( GlobalConfig, EditAnywhere, Category = CustomHoudiniTools )
        bool bHidePlacementModeHoudiniTools;

        UPROPERTY(GlobalConfig, EditAnywhere, Category = CustomHoudiniTools)
        TArray<FHoudiniToolDirectory> CustomHoudiniToolsLocation;

    /** Arguments for HAPI_Initialize */
    public:
        // Evaluation thread stack size in bytes.  -1 for default 
        UPROPERTY( GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization )
            int32 CookingThreadStackSize;
        // List of paths to Houdini-compatible .env files (; separated on Windows, : otherwise)
        UPROPERTY( GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization )
            FString HoudiniEnvironmentFiles;
        // Path to find other OTL/HDA files
        UPROPERTY( GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization )
            FString OtlSearchPath;
        // Sets HOUDINI_DSO_PATH
        UPROPERTY( GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization )
            FString DsoSearchPath;
        // Sets HOUDINI_IMAGE_DSO_PATH
        UPROPERTY( GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization )
            FString ImageDsoSearchPath;
        // Sets HOUDINI_AUDIO_DSO_PATH
        UPROPERTY( GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization )
            FString AudioDsoSearchPath;
};
