/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Engine/AssetUserData.h"
#include "PhysicsEngine/BodyInstance.h"

#include "HoudiniRuntimeSettings.generated.h"

class UFoliageType_InstancedStaticMesh;

UENUM()
enum EHoudiniRuntimeSettingsSessionType
{
	// In process session.
	HRSST_InProcess UMETA(Hidden),

	// TCP socket connection to Houdini Engine server.
	HRSST_Socket UMETA(DisplayName = "TCP socket"),

	// Connection to Houdini Engine server via pipe connection.
	HRSST_NamedPipe UMETA(DisplayName = "Named pipe or domain socket"),

	// No session, prevents license/Engine cook
	HRSST_None UMETA(DisplayName = "None"),

	HRSST_MAX
};


UENUM()
enum EHoudiniRuntimeSettingsRecomputeFlag
{
	// Recompute always.
	HRSRF_Always UMETA(DisplayName = "Always"),

	// Recompute only if missing.
	HRSRF_OnlyIfMissing UMETA(DisplayName = "Only if missing"),

	// Do not recompute.
	HRSRF_Never UMETA(DisplayName = "Never"),

	HRSRF_MAX,
};

UENUM()
enum EHoudiniExecutableType
{
	// Houdini
	HRSHE_Houdini UMETA(DisplayName = "Houdini"),

	// Houdini FX
	HRSHE_HoudiniFX UMETA(DisplayName = "Houdini FX"),

	// Houdini Core
	HRSHE_HoudiniCore UMETA(DisplayName = "Houdini Core"),

	// Houdini Indie
	HRSHE_HoudiniIndie UMETA(DisplayName = "Houdini Indie"),
};

USTRUCT(BlueprintType)
struct HOUDINIENGINERUNTIME_API FHoudiniStaticMeshGenerationProperties
{
	GENERATED_USTRUCT_BODY()

	// Constructor
	FHoudiniStaticMeshGenerationProperties();

	public:

	/** If true, the physics triangle mesh will use double sided faces when doing scene queries. */
	UPROPERTY(EditAnywhere, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Double Sided Geometry"))
	uint32 bGeneratedDoubleSidedGeometry : 1;

	/** Physical material to use for simple collision on this body. Encodes information about density, friction etc. */
	UPROPERTY(EditAnywhere, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Simple Collision Physical Material"))
	UPhysicalMaterial * GeneratedPhysMaterial;

	/** Default properties of the body instance, copied into objects on instantiation, was URB_BodyInstance */
	UPROPERTY(EditAnywhere, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (FullyExpand = "true"))
	struct FBodyInstance DefaultBodyInstance;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate. */
	UPROPERTY(EditAnywhere, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Collision Complexity"))
	TEnumAsByte<enum ECollisionTraceFlag> GeneratedCollisionTraceFlag;

	/** Resolution of lightmap. */
	UPROPERTY(EditAnywhere, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Light Map Resolution", FixedIncrement = "4.0"))
	int32 GeneratedLightMapResolution;

	/** Custom walkable slope setting for generated mesh's body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Walkable Slope Override"))
	FWalkableSlopeOverride GeneratedWalkableSlopeOverride;

	/** The light map coordinate index. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Light map coordinate index"))
	int32 GeneratedLightMapCoordinateIndex;

	/** True if mesh should use a less-conservative method of mip LOD texture factor computation. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Use Maximum Streaming Texel Ratio"))
	uint32 bGeneratedUseMaximumStreamingTexelRatio : 1;

	/** Allows artists to adjust the distance where textures using UV 0 are streamed in/out. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Streaming Distance Multiplier"))
	float GeneratedStreamingDistanceMultiplier;

	/** Default settings when using this mesh for instanced foliage. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Foliage Default Settings"))
	UFoliageType_InstancedStaticMesh* GeneratedFoliageDefaultSettings = nullptr;

	/** Array of user data stored with the asset. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "HoudiniMeshGeneration | StaticMeshGeneration", meta = (DisplayName = "Asset User Data"))
	TArray<UAssetUserData*> GeneratedAssetUserData;
};


UCLASS(config = Engine, defaultconfig)
class HOUDINIENGINERUNTIME_API UHoudiniRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	public:

		// Destructor.
		virtual ~UHoudiniRuntimeSettings();

		// 
		virtual void PostInitProperties() override;

#if WITH_EDITOR
		virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

protected:

		// Locate property of this class by name.
		FProperty * LocateProperty(const FString & PropertyName) const;

		// Make specified property read only.
		void SetPropertyReadOnly(const FString & PropertyName, bool bReadOnly = true);

#if WITH_EDITOR
		// Update session ui elements.
		void UpdateSessionUI();
#endif

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Session options.
		//-------------------------------------------------------------------------------------------------------------
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		TEnumAsByte<enum EHoudiniRuntimeSettingsSessionType> SessionType;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		FString ServerHost;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		int32 ServerPort;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		FString ServerPipeName;

		// Whether to automatically start a HARS process
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		bool bStartAutomaticServer;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		float AutomaticServerTimeout;

		// If enabled, changes made in Houdini, when connected to Houdini running in Session Sync mode will be automatically be pushed to Unreal.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session)
		bool bSyncWithHoudiniCook;
			
		// If enabled, the Houdini Timeline time will be used to cook assets.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session)
		bool bCookUsingHoudiniTime;

		// Enable when wanting to sync the Houdini and Unreal viewport when using Session Sync.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session)
		bool bSyncViewport;

		// If enabled, Houdini's viewport will be synchronized to Unreal's when using Session Sync.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session, meta = (DisplayName = "Sync the Houdini Viewport to Unreal's viewport.", EditCondition = "bSyncViewport"))
		bool bSyncHoudiniViewport;
		
		// If enabled, Unreal's viewport will be synchronized to Houdini's when using Session Sync.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session, meta = (DisplayName = "Sync the Unreal Viewport to Houdini's viewport", EditCondition = "bSyncViewport"))
		bool bSyncUnrealViewport;
		
		//-------------------------------------------------------------------------------------------------------------
		// Instantiating options.
		//-------------------------------------------------------------------------------------------------------------

		// Whether to ask user to select an asset when instantiating an HDA with multiple assets inside. If disabled, will always instantiate first asset.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Instantiating)
		bool bShowMultiAssetDialog;

		// When enabled, the plugin will always instantiate the memory copy of a HDA stored in the .uasset file 
		// instead of using the latest version of the HDA file itself.
		// This helps ensuring consistency between users when using HDAs, but will not work with expanded HDAs.
		// When disabled, the plugin will always instantiate the latest version of the source HDA file if it is 
		// available, and will fallback to the memory copy if the source file cannot be found
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Instantiating)
		bool bPreferHdaMemoryCopyOverHdaSourceFile;

		//-------------------------------------------------------------------------------------------------------------
		// Cooking options.
		//-------------------------------------------------------------------------------------------------------------

		// Whether houdini engine cooking is paused or not upon initializing the plugin
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		bool bPauseCookingOnStart;

		// Whether to display instantiation and cooking Slate notifications.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		bool bDisplaySlateCookingNotifications;

		// Default content folder storing all the temporary cook data (Static meshes, materials, textures, landscape layer infos...)
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		FString DefaultTemporaryCookFolder;

		// Default content folder used when baking houdini asset data to native unreal objects
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		FString DefaultBakeFolder;

		//-------------------------------------------------------------------------------------------------------------
		// Parameter options.
		//-------------------------------------------------------------------------------------------------------------

		/* Deprecated!
		// Forces the treatment of ramp parameters as multiparms.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Parameters)
		bool bTreatRampParametersAsMultiparms;
		*/

		//-------------------------------------------------------------------------------------------------------------
		// Geometry Marshalling
		//-------------------------------------------------------------------------------------------------------------

		// If true, generated Landscapes will be marshalled using default unreal scaling. 
		// Generated landscape will loose a lot of precision on the Z axis but will use the same transforms
		// as Unreal's default landscape
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Landscape - Use default Unreal scaling."))
		bool MarshallingLandscapesUseDefaultUnrealScaling;

		// If true, generated Landscapes will be using full precision for their ZAxis, 
		// allowing for more precision but preventing them from being sculpted higher/lower than their min/max.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Landscape - Use full resolution for data conversion."))
		bool MarshallingLandscapesUseFullResolution;

		// If true, the min/max values used to convert heightfields to landscape will be forced values
		// This is usefull when importing multiple landscapes from different HDAs
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Landscape - Force Min/Max values for data conversion"))
		bool MarshallingLandscapesForceMinMaxValues;

		// The minimum value to be used for Landscape conversion when MarshallingLandscapesForceMinMaxValues is enabled
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Landscape - Forced min value"))
		float MarshallingLandscapesForcedMinValue;

		// The maximum value to be used for Landscape conversion when MarshallingLandscapesForceMinMaxValues is enabled
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Landscape - Forced max value"))
		float MarshallingLandscapesForcedMaxValue;

		// If this is enabled, additional rot & scale attributes are added on curve inputs
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Curves - Add rot & scale attributes on curve inputs"))
		bool bAddRotAndScaleAttributesOnCurves;

		// If this is enabled, additional rot & scale attributes are added on curve inputs
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Curves - Use Legacy Input Curves"))
		bool bUseLegacyInputCurves;
	
		// Default resolution used when converting Unreal Spline Components to Houdini Curves (step in cm between control points, 0 only send the control points)
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeometryMarshalling", meta = (DisplayName = "Curves - Default spline resolution (cm)"))
		float MarshallingSplineResolution;

		//-------------------------------------------------------------------------------------------------------------
		// Static Mesh Options
		//-------------------------------------------------------------------------------------------------------------

		// For StaticMesh outputs: should a fast proxy be created first?
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Static Mesh", meta = (DisplayName = "Enable Proxy Static Mesh"))
		bool bEnableProxyStaticMesh;

		// For static mesh outputs and socket actors: should spawn a default actor if the reference is invalid?
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Static Mesh", meta = (DisplayName = "Show Default Mesh"))
		bool bShowDefaultMesh;

		// If fast proxy meshes are being created, must it be baked as a StaticMesh after a period of no updates?
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Refine Proxy Static Meshes After a Timeout", EditCondition = "bEnableProxyStaticMesh"))
		bool bEnableProxyStaticMeshRefinementByTimer;

		// If the option to automatically refine the proxy mesh via a timer has been selected, this controls the timeout in seconds.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Proxy Mesh Auto Refine Timeout Seconds", EditCondition = "bEnableProxyStaticMesh && bEnableProxyStaticMeshRefinementByTimer"))
		float ProxyMeshAutoRefineTimeoutSeconds;

		// Automatically refine proxy meshes to UStaticMesh before the map is saved
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Refine Proxy Static Meshes When Saving a Map", EditCondition = "bEnableProxyStaticMesh"))
		bool bEnableProxyStaticMeshRefinementOnPreSaveWorld;

		// Automatically refine proxy meshes to UStaticMesh before starting a play in editor session
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Refine Proxy Static Meshes On PIE", EditCondition = "bEnableProxyStaticMesh"))
		bool bEnableProxyStaticMeshRefinementOnPreBeginPIE;

		//-------------------------------------------------------------------------------------------------------------
		// Generated StaticMesh settings.
		//-------------------------------------------------------------------------------------------------------------

		/// If true, the physics triangle mesh will use double sided faces for new Houdini Assets when doing scene queries.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Double Sided Geometry"))
		uint32 bDoubleSidedGeometry : 1;

		/// Physical material to use for simple collision of new Houdini Assets. Encodes information about density, friction etc.
		UPROPERTY(EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Simple Collision Physical Material"))
		UPhysicalMaterial * PhysMaterial;

		/// Default properties of the body instance
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (FullyExpand = "true"))
		struct FBodyInstance DefaultBodyInstance;

		/// Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate for new Houdini Assets.
		UPROPERTY(GlobalConfig, VisibleDefaultsOnly, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Collision Complexity"))
		TEnumAsByte<enum ECollisionTraceFlag> CollisionTraceFlag;

		/// Resolution of lightmap for baked lighting.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Light Map Resolution", FixedIncrement = "4.0"))
		int32 LightMapResolution;

		/// Bias multiplier for Light Propagation Volume lighting for new Houdini Assets.
		UPROPERTY(GlobalConfig, EditAnywhere, BlueprintReadOnly, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Lpv Bias Multiplier", UIMin = "0.0", UIMax = "3.0"))
		float LpvBiasMultiplier;

		/// Default Mesh distance field resolution, setting it to 0 will prevent the mesh distance field generation while editing the asset
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Distance Field Resolution Scale", UIMin = "0.0", UIMax = "100.0"))
		float GeneratedDistanceFieldResolutionScale;

		/// Custom walkable slope setting for bodies of new Houdini Assets.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Walkable Slope Override"))
		FWalkableSlopeOverride WalkableSlopeOverride;

		/// The UV coordinate index of lightmap 
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Light map coordinate index"))
		int32 LightMapCoordinateIndex;

		/// True if mesh should use a less-conservative method of mip LOD texture factor computation for new Houdini Assets.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Use Maximum Streaming Texel Ratio"))
		uint32 bUseMaximumStreamingTexelRatio : 1;

		/// Allows artists to adjust the distance where textures using UV 0 are streamed in/out for new Houdini Assets.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Streaming Distance Multiplier"))
		float StreamingDistanceMultiplier;

		/// Default settings when using new Houdini Asset mesh for instanced foliage.
		UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Foliage Default Settings"))
		UFoliageType_InstancedStaticMesh * FoliageDefaultSettings;

		/// Array of user data stored with the new Houdini Asset.
		UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "GeneratedStaticMeshSettings", meta = (DisplayName = "Asset User Data"))
		TArray<UAssetUserData *> AssetUserData;

		//-------------------------------------------------------------------------------------------------------------
		// Static Mesh build settings. 
		//-------------------------------------------------------------------------------------------------------------

		// If true, UVs will be stored at full floating point precision.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings")
		bool bUseFullPrecisionUVs;

		// Source UV set for generated lightmap.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Source Lightmap Index"))
		int32 SrcLightmapIndex;

		// Destination UV set for generated lightmap.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Destination Lightmap Index"))
		int32 DstLightmapIndex;

		// Target lightmap resolution to for generated lightmap.  Determines the padding between UV shells in a packed lightmap.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings")
		int32 MinLightmapResolution;

		// If true, degenerate triangles will be removed.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings")
		bool bRemoveDegenerates;

		// Lightmap UV generation
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Generate Lightmap UVs"))
		TEnumAsByte<enum EHoudiniRuntimeSettingsRecomputeFlag> GenerateLightmapUVsFlag;

		// Normals generation
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Recompute Normals"))
		TEnumAsByte<enum EHoudiniRuntimeSettingsRecomputeFlag> RecomputeNormalsFlag;

		// Tangents generation
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Recompute Tangents"))
		TEnumAsByte<enum EHoudiniRuntimeSettingsRecomputeFlag> RecomputeTangentsFlag;

		// If true, recomputed tangents and normals will be calculated using MikkT Space.  This method does require properly laid out UVs though otherwise you'll get a degenerate tangent warning
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Generate Using MikkT Space"))
		bool bUseMikkTSpace;

		// Required for PNT tessellation but can be slow. Recommend disabling for larger meshes.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "StaticMeshBuildSettings")
		bool bBuildAdjacencyBuffer;

		// If true, we will use the surface area and the corner angle of the triangle as a ratio when computing the normals.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMeshBuildSettings")
		uint8 bComputeWeightedNormals : 1;

		// Required to optimize mesh in mirrored transform. Double index buffer size.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMeshBuildSettings")
		uint8 bBuildReversedIndexBuffer : 1;

		// If true, Tangents will be stored at 16 bit vs 8 bit precision.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMeshBuildSettings")
		uint8 bUseHighPrecisionTangentBasis : 1;

		// Scale to apply to the mesh when allocating the distance field volume texture.
		// The default scale is 1, which is assuming that the mesh will be placed unscaled in the world.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMeshBuildSettings")
		float DistanceFieldResolutionScale;

		// Whether to generate the distance field treating every triangle hit as a front face.
		// When enabled prevents the distance field from being discarded due to the mesh being open, but also lowers Distance Field AO quality.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Two-Sided Distance Field Generation"))
		uint8 bGenerateDistanceFieldAsIfTwoSided : 1;

		// Enable the Physical Material Mask
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMeshBuildSettings", meta = (DisplayName = "Enable Physical Material Mask"))
		uint8 bSupportFaceRemap : 1;

		//-------------------------------------------------------------------------------------------------------------
		// PDG Commandlet import
		//-------------------------------------------------------------------------------------------------------------
		// Is the PDG commandlet enabled? 
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "PDG Settings", Meta = (DisplayName = "Async Importer Enabled"))
		bool bPDGAsyncCommandletImportEnabled;

		//-------------------------------------------------------------------------------------------------------------
		// Legacy
		//-------------------------------------------------------------------------------------------------------------
		// Whether to enable backward compatibility
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Legacy", Meta = (DisplayName = "Enable backward compatibility with Version 1"))
		bool bEnableBackwardCompatibility;

		// Automatically rebuild legacy HAC
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Legacy", meta = (DisplayName = "Automatically rebuild legacy Houdini Asset Components", EditCondition = "bEnableBackwardCompatibility"))
		bool bAutomaticLegacyHDARebuild;

		//-------------------------------------------------------------------------------------------------------------
		// Custom Houdini Location
		//-------------------------------------------------------------------------------------------------------------
		// Whether to use custom Houdini location.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniLocation, Meta = (DisplayName = "Use custom Houdini location (requires restart)"))
		bool bUseCustomHoudiniLocation;

		// Custom Houdini location (where HAPI library is located).
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniLocation, Meta = (DisplayName = "Custom Houdini location"))
		FDirectoryPath CustomHoudiniLocation;

		// Select the Houdini executable to be used when opening session sync or opening hip files
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniLocation, Meta = (DisplayName = "Houdini Executable"))
		TEnumAsByte <enum EHoudiniExecutableType> HoudiniExecutable;

		//-------------------------------------------------------------------------------------------------------------
		// HAPI_Initialize
		//-------------------------------------------------------------------------------------------------------------
		// Evaluation thread stack size in bytes.  -1 for default 
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		int32 CookingThreadStackSize;

		// List of paths to Houdini-compatible .env files (; separated on Windows, : otherwise)
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString HoudiniEnvironmentFiles;

		// Path to find other OTL/HDA files
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString OtlSearchPath;

		// Sets HOUDINI_DSO_PATH
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString DsoSearchPath;

		// Sets HOUDINI_IMAGE_DSO_PATH
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString ImageDsoSearchPath;

		// Sets HOUDINI_AUDIO_DSO_PATH
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString AudioDsoSearchPath;
};