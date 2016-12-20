/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
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

#include "HoudiniAssetInput.h"
#include "HoudiniGeoPartObject.h"

class AActor;
class UClass;
class FArchive;
class UTexture2D;
class UBlueprint;
class UStaticMesh;
class UHoudiniAsset;
class ALandscapeProxy;
class AHoudiniAssetActor;
class UMaterialExpression;
class UHoudiniAssetMaterial;
class UHoudiniAssetComponent;
class FHoudiniAssetObjectGeo;
class UInstancedStaticMeshComponent;
class USplineComponent;

struct FRawMesh;

DECLARE_STATS_GROUP( TEXT( "HoudiniEngine" ), STATGROUP_HoudiniEngine, STATCAT_Advanced );

struct HOUDINIENGINERUNTIME_API FHoudiniEngineUtils
{
    public:

        /** Used to control behavior of package baking helper functions */
        enum class EBakeMode
        {
            Intermediate,
            CreateNewAssets,
            ReplaceExisitingAssets
        };

        /** Return a string description of error from a given error code. **/
        static const FString GetErrorDescription( HAPI_Result Result );

        /** Return a string error description. **/
        static const FString GetErrorDescription();

        /** Return a string indicating cook state. **/
        static const FString GetCookState();

        /** Return a string representing cooking result. **/
        static const FString GetCookResult();

        /** Return true if module has been properly initialized. **/
        static bool IsInitialized();

        /** Return type of license used. **/
        static bool GetLicenseType( FString & LicenseType );

        /** Return true if we are running Houdini Engine Indie license. **/
        static bool IsLicenseHoudiniEngineIndie();

        /** Return necessary buffer size to store preset information for a given asset. **/
        static bool ComputeAssetPresetBufferLength( HAPI_NodeId AssetId, int32 & OutBufferLength );

        /** Sets preset data for a given asset. **/
        static bool SetAssetPreset( HAPI_NodeId AssetId, const TArray< char > & PresetBuffer );

        /** Gets preset data for a given asset. **/
        static bool GetAssetPreset( HAPI_NodeId AssetId, TArray< char > & PresetBuffer );

        /** Return true if asset is valid. **/
        static bool IsHoudiniAssetValid( HAPI_NodeId AssetId );

        /** Destroy asset, returns the status. **/
        static bool DestroyHoudiniAsset( HAPI_NodeId AssetId );

        /** HAPI : Get unique material SHOP name. **/
        static bool GetUniqueMaterialShopName( HAPI_NodeId AssetId, HAPI_NodeId MaterialId, FString & Name );

        /** HAPI : Convert Unreal string to ascii one. **/
        static void ConvertUnrealString( const FString & UnrealString, std::string & String );

        /** HAPI : Translate HAPI transform to Unreal one. **/
        static void TranslateHapiTransform( const HAPI_Transform & HapiTransform, FTransform & UnrealTransform );

        /** HAPI : Translate HAPI Euler transform to Unreal one. **/
        static void TranslateHapiTransform( const HAPI_TransformEuler & HapiTransformEuler, FTransform & UnrealTransform );

        /** HAPI : Translate Unreal transform to HAPI one. **/
        static void TranslateUnrealTransform( const FTransform & UnrealTransform, HAPI_Transform & HapiTransform );

        /** HAPI : Translate Unreal transform to HAPI Euler one. **/
        static void TranslateUnrealTransform( const FTransform & UnrealTransform, HAPI_TransformEuler & HapiTransformEuler );

        /** HAPI : Set current HAPI time. **/
        static bool SetCurrentTime( float CurrentTime );

        /** Return name of Houdini asset. **/
        static bool GetHoudiniAssetName( HAPI_NodeId AssetId, FString & NameString );

        /** Construct static meshes for a given Houdini asset. **/
        static bool CreateStaticMeshesFromHoudiniAsset(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesIn,
            TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesOut, FTransform & ComponentTransform );

        /** Bake static mesh. **/
        static UStaticMesh * BakeStaticMesh(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const FHoudiniGeoPartObject & HoudiniGeoPartObject,
            UStaticMesh * StaticMesh );

        /** Bake blueprint. **/
        static UBlueprint * BakeBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent );

        /** Bake blueprint, instantiate and replace Houdini actor. **/
        static AActor * ReplaceHoudiniActorWithBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent );

        /** Extract position information from coords string. **/
        static void ExtractStringPositions( const FString & Positions, TArray< FVector > & OutPositions );

        /** Create string containing positions from a given vector of positions. **/
        static void CreatePositionsString( const TArray< FVector > & Positions, FString & PositionString );

        /** Given raw positions incoming from HAPI, convert them to Unreal's FVector and perform necessary flipping and **/
        /** scaling.                                                                                                    **/
        static void ConvertScaleAndFlipVectorData( const TArray< float > & DataRaw, TArray< FVector > & DataOut );

        /** Returns platform specific name of libHAPI. **/
        static FString HoudiniGetLibHAPIName();

        /** Load libHAPI and return handle to it, also store location of loaded libHAPI in passed argument. **/
        static void* LoadLibHAPI( FString & StoredLibHAPILocation );

        /** Helper function to count number of UV sets in raw mesh. **/
        static int32 CountUVSets( const FRawMesh & RawMesh );

        /** Helper function to extract copied Houdini actor from clipboard. **/
        static AHoudiniAssetActor * LocateClipboardActor( const AActor* IgnoreActor, const FString & ClipboardText );

        /** Update instances of a given instanced static mesh component. **/
        static void UpdateInstancedStaticMeshComponentInstances(
            USceneComponent * Component,
            const TArray< FTransform > & InstancedTransforms,
            const FRotator & RotationOffset,
            const FVector & ScaleOffset );

        /** Retrieves list of asset names contained within the HDA. **/
        static bool GetAssetNames(
            UHoudiniAsset * HoudiniAsset, HAPI_AssetLibraryId & AssetLibraryId,
            TArray< HAPI_StringHandle > & AssetNames );

        /** Resizes the HeightData so that it fits to UE4's size requirements **/
        static bool ResizeHeightDataForLandscape(
            TArray<uint16>& HeightData,
            int32& SizeX, int32& SizeY,
            int32& NumberOfSectionsPerComponent,
            int32& NumberOfQuadsPerSection );

        /** Resizes LayerData so that it fits the Landscape size **/
        static bool ResizeLayerDataForLandscape(
            TArray<uint8>& LayerData,
            const int32& SizeX, const int32& SizeY,
            const int32& NewSizeX, const int32& NewSizeY );

        /** HAPI : Return true if given asset id is valid. **/
        static bool IsValidAssetId( HAPI_NodeId AssetId );

        /** HAPI : Create curve for input. **/
        static bool HapiCreateCurveNode( HAPI_NodeId & CurveNodeId );

        /** HAPI : Retrieve the asset node's object transform. **/
        static bool HapiGetAssetTransform( HAPI_NodeId AssetId, FTransform & InTransform );

        /** HAPI : Retrieve Node id from given parameters. **/
        static bool HapiGetNodeId( HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId, HAPI_NodeId & NodeId );

        /** HAPI: Retrieve Path to the given Node, relative to the given Node */
        static bool HapiGetNodePath( HAPI_NodeId NodeId, HAPI_NodeId RelativeToNodeId, FString & OutPath );

        /** HAPI : Retrieve HAPI_ObjectInfo's from given asset node id. **/
        static bool HapiGetObjectInfos( HAPI_NodeId AssetId, TArray< HAPI_ObjectInfo > & ObjectInfos );

        /** HAPI : Retrieve object transforms from given asset node id. **/
        static bool HapiGetObjectTransforms( HAPI_NodeId AssetId, TArray< HAPI_Transform > & ObjectTransforms );

        /** HAPI : Marshalling, extract landscape geometry and upload it. Return true on success. **/
        static bool HapiCreateInputNodeForData(
            HAPI_NodeId HostAssetId,
            ALandscapeProxy * LandscapeProxy, HAPI_NodeId & ConnectedAssetId, bool bExportOnlySelected, bool bExportCurves,
            bool bExportMaterials, bool bExportFullGeometry, bool bExportLighting, bool bExportNormalizedUVs,
            bool bExportTileUVs );

        /** HAPI : Marshaling, extract geometry and create input asset for it - return true on success **/
        static bool HapiCreateInputNodeForData(
            HAPI_NodeId HostAssetId, 
            UStaticMesh * Mesh,
            HAPI_NodeId & ConnectedAssetId,
            class UStaticMeshComponent* StaticMeshComponent = nullptr );

        /** HAPI : Marshaling, extract geometry and create input asset for it - return true on success **/
        static bool HapiCreateInputNodeForData(
            HAPI_NodeId HostAssetId,
            TArray<UObject *>& InputObjects,
            HAPI_NodeId & ConnectedAssetId, TArray< HAPI_NodeId >& OutCreatedNodeIds );

        /** HAPI : Marshaling, extract geometry and create input asset for it - return true on success **/
        static bool HapiCreateInputNodeForData(
            HAPI_NodeId HostAssetId,
            TArray< FHoudiniAssetInputOutlinerMesh > & OutlinerMeshArray,
            HAPI_NodeId & ConnectedAssetId,
            const float& SplineResolution = -1.0f);

        /** HAPI : Marshaling, extract points from the Unreal Spline and create an input curve for it - return true on success **/
        static bool HapiCreateInputNodeForData(
            HAPI_NodeId HostAssetId,
            USplineComponent * SplineComponent,
            HAPI_NodeId & ConnectedAssetId,
            FHoudiniAssetInputOutlinerMesh& OutlinerMesh,
            const float& fSplineResolution = -1.0f);

        static bool HapiCreateCurveInputNodeForData(
            HAPI_NodeId HostAssetId,
            HAPI_NodeId & ConnectedAssetId,
            TArray<FVector>* Positions,
            TArray<FQuat>* Rotations = nullptr,
            TArray<FVector>* Scales3d = nullptr,
            TArray<float>* UniformScales = nullptr);

        /** HAPI : Marshaling, disconnect input asset from a given slot. **/
        static bool HapiDisconnectAsset( HAPI_NodeId HostAssetId, int32 InputIndex );

        /** HAPI : Marshaling, connect input asset to a given slot of host asset. **/
        static bool HapiConnectAsset(
            HAPI_NodeId AssetIdFrom, HAPI_NodeId ObjectIdFrom, HAPI_NodeId AssetIdTo, int32 InputIndex );

        /** HAPI : Set asset transform. **/
        static bool HapiSetAssetTransform( HAPI_NodeId AssetId, const FTransform & Transform );

        /** HAPI : Return all group names for a given Geo. **/
        static bool HapiGetGroupNames(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_GroupType GroupType, TArray< FString > & GroupNames );

        /** HAPI : Retrieve group membership. **/
        static bool HapiGetGroupMembership(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, HAPI_GroupType GroupType, const FString & GroupName,
            TArray< int32 > & GroupMembership );

        /** HAPI : Get group count by type. **/
        static int32 HapiGetGroupCountByType( HAPI_GroupType GroupType, HAPI_GeoInfo & GeoInfo );

        /** HAPI : Get element count by group type. **/
        static int32 HapiGetElementCountByGroupType( HAPI_GroupType GroupType, HAPI_PartInfo & PartInfo );

        /** HAPI : Check if object geo part has group membership. **/
        static bool HapiCheckGroupMembership(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject,
            HAPI_GroupType GroupType, const FString & GroupName );
        static bool HapiCheckGroupMembership(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, HAPI_GroupType GroupType, const FString & GroupName );

        /** HAPI : Check if given attribute exists. **/
        static bool HapiCheckAttributeExists(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, const char * Name, HAPI_AttributeOwner Owner );
        static bool HapiCheckAttributeExists(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name, HAPI_AttributeOwner Owner );

        /** HAPI : Get attribute data as float. **/
        static bool HapiGetAttributeDataAsFloat(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo, TArray< float > & Data,
            int32 TupleSize = 0 );

        static bool HapiGetAttributeDataAsFloat(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name,
            HAPI_AttributeInfo & ResultAttributeInfo, TArray< float > & Data, int32 TupleSize = 0 );

        /** HAPI : Get attribute data as integer. **/
        static bool HapiGetAttributeDataAsInteger(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, const char* Name, HAPI_AttributeInfo & ResultAttributeInfo, TArray< int32 > & Data,
            int32 TupleSize = 0 );

        static bool HapiGetAttributeDataAsInteger(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name,
            HAPI_AttributeInfo & ResultAttributeInfo, TArray< int32 > & Data, int32 TupleSize = 0 );

        /** HAPI : Get attribute data as string. **/
        static bool HapiGetAttributeDataAsString(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, const char * Name, HAPI_AttributeInfo & ResultAttributeInfo, TArray< FString > & Data,
            int32 TupleSize = 0 );

        static bool HapiGetAttributeDataAsString(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject, const char * Name,
            HAPI_AttributeInfo & ResultAttributeInfo, TArray< FString > & Data, int32 TupleSize = 0 );

        /** HAPI : Get parameter data as float. **/
        static bool HapiGetParameterDataAsFloat(
            HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue, float & Value );

        /** HAPI : Get parameter data as integer. **/
        static bool HapiGetParameterDataAsInteger(
            HAPI_NodeId NodeId, const std::string ParmName, int32 DefaultValue, int32 & Value );

        /** HAPI : Get parameter data as string. **/
        static bool HapiGetParameterDataAsString(
            HAPI_NodeId NodeId, const std::string ParmName,
            const FString & DefaultValue, FString & Value);

        /** HAPI: Get a parameter's unit. **/
        static bool HapiGetParameterUnit( const HAPI_NodeId& NodeId, const HAPI_ParmId& ParmId, FString& OutUnitString );

        /** HAPI : Retrieve names of all parameters. **/
        static void HapiRetrieveParameterNames( const TArray< HAPI_ParmInfo > & ParmInfos, TArray< FString > & Names );
        static void HapiRetrieveParameterNames( const TArray< HAPI_ParmInfo > & ParmInfos, TArray< std::string > & Names );

        /** HAPI : Look for a parameter by name or tag and returns its index. Returns -1 if not found. **/
        static int32 HapiFindParameterByNameOrTag( const HAPI_NodeId& NodeId, const std::string ParmName );
        static int32 HapiFindParameterByNameOrTag( const HAPI_NodeId& NodeId, const std::string ParmName, HAPI_ParmInfo& FoundParmInfo );
        static int32 HapiFindParameterByNameOrTag( const TArray< HAPI_ParmInfo > NodeParams, const HAPI_NodeId& NodeId, const std::string ParmName );

        /** HAPI : Retrieve a list of image planes. **/
        static bool HapiGetImagePlanes(
            HAPI_ParmId NodeParmId, const HAPI_MaterialInfo & MaterialInfo,
            TArray< FString > & ImagePlanes );

        /** HAPI : Extract image data. **/
        static bool HapiExtractImage(
            HAPI_ParmId NodeParmId, const HAPI_MaterialInfo & MaterialInfo,
            TArray< char > & ImageBuffer, const char * PlaneType, HAPI_ImageDataFormat ImageDataFormat,
            HAPI_ImagePacking ImagePacking, bool bRenderToImage );

        /** HAPI : Return true if given material is transparent. **/
        static bool HapiIsMaterialTransparent( const HAPI_MaterialInfo & MaterialInfo );

        /** HAPI : Create Unreal materials and necessary textures. Reuse existing materials, if they are not updated. **/
        static void HapiCreateMaterials(
            UHoudiniAssetComponent * HoudiniAssetComponent, const HAPI_AssetInfo & AssetInfo,
            const TSet< HAPI_NodeId > & UniqueMaterialIds, const TSet< HAPI_NodeId > & UniqueInstancerMaterialIds,
            TMap< FString, UMaterialInterface * > & Materials );

#if WITH_EDITOR

        /** Create various material components. **/
        static bool CreateMaterialComponentDiffuse(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentNormal(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentSpecular(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentRoughness(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentMetallic(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentEmissive(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentOpacity(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

        static bool CreateMaterialComponentOpacityMask(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo, const HAPI_NodeInfo & NodeInfo,
            const TArray< HAPI_ParmInfo > & NodeParams, int32 & MaterialNodeY );

#endif // WITH_EDITOR

        /** HAPI : Retrieve instance transforms for a specified geo object. **/
        static bool HapiGetInstanceTransforms(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, TArray< FTransform > & Transforms );

        static bool HapiGetInstanceTransforms(
            const FHoudiniGeoPartObject & HoudiniGeoPartObject,
            TArray< FTransform > & Transforms );

        /** HAPI : Given vertex list, retrieve new vertex list for a specified group.                                   **/
        /** Return number of processed valid index vertices for this split.                                             **/
        static int32 HapiGetVertexListForGroup(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId, HAPI_NodeId GeoId,
            HAPI_PartId PartId, const FString & GroupName, const TArray< int32 > & FullVertexList,
            TArray< int32 > & NewVertexList, TArray< int32 > & AllVertexList, TArray< int32 > & AllFaceList,
            TArray< int32 > & AllCollisionFaceIndices );

        /** HAPI : Retrieves the mesh sockets list for the current part							**/
        static int32 GetMeshSocketList(
            HAPI_NodeId AssetId, HAPI_NodeId ObjectId,
            HAPI_NodeId GeoId, HAPI_PartId PartId,
            TArray< FTransform >& AllSockets,
            TArray< FString >& AllSocketsName,
            TArray< FString >& AllSocketsActors );

        /** Add the mesh sockets in the list to the specified StaticMesh						**/
        static bool AddMeshSocketsToStaticMesh(
            UStaticMesh* StaticMesh,
            FHoudiniGeoPartObject& HoudiniGeoPartObject,
            TArray< FTransform >& AllSockets,
            TArray< FString >& AllSocketsNames,
            TArray< FString >& AllSocketsActors );

        /** Add the actor stored in the socket tag to the socket for the given static mesh component 			**/
        static bool AddActorsToMeshSocket( UStaticMeshSocket* Socket, UStaticMeshComponent* StaticMeshComponent );

        /** Add the mesh aggregate collision geo to the specified StaticMesh						**/
        static bool AddAggregateCollisionGeometryToStaticMesh(
            UStaticMesh* StaticMesh,
            FHoudiniGeoPartObject& HoudiniGeoPartObject,
            FKAggregateGeom& AggregateCollisionGeo );

        /** Create a package for given component for static mesh baking. **/
        static UPackage * BakeCreateStaticMeshPackageForComponent(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const FHoudiniGeoPartObject & HoudiniGeoPartObject,
            FString & MeshName, FGuid & BakeGUID, EBakeMode BakeMode );

#if WITH_EDITOR

        /** Duplicate a given static mesh. This will create a new package for it. This will also create necessary       **/
        /** materials and textures and their corresponding packages. **/
        static UStaticMesh * DuplicateStaticMeshAndCreatePackage(
            const UStaticMesh * StaticMesh, UHoudiniAssetComponent * Component,
            const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode );

        /** Bake output meshes and materials to packages and create corresponding actors in the scene */
        static void BakeHoudiniActorToActors( UHoudiniAssetComponent * HoudiniAssetComponent, bool SelectNewActors );

        /** Get a candidate for baking to outliner input workflow */
        static class UHoudiniAssetInput* GetInputForBakeHoudiniActorToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent );

        /** Returns true if the conditions are met for Bake to Input action ( 1 static mesh output and first input is world outliner with a static mesh) */
        static bool GetCanComponentBakeToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent );

        /** Bakes output meshes and materials to packages and sets them on an input */
        static void BakeHoudiniActorToOutlinerInput( UHoudiniAssetComponent * HoudiniAssetComponent );
#endif

    protected:

#if PLATFORM_WINDOWS
    
        /** Attempt to locate libHAPI on Windows in the registry. Return handle if located and return location. **/
        static void* LocateLibHAPIInRegistry(
            const FString & HoudiniInstallationType, const FString & HoudiniVersionString, FString & StoredLibHAPILocation );

#endif

        /** Create a package for given component for blueprint baking. **/
        static UPackage * BakeCreateBlueprintPackageForComponent(
            UHoudiniAssetComponent * HoudiniAssetComponent, FString & BlueprintName );

        /** Create a package for a given component for material. **/
        static UPackage * BakeCreateMaterialPackageForComponent(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const HAPI_MaterialInfo & MaterialInfo, FString & MaterialName, EBakeMode BakeMode );
        static UPackage * BakeCreateMaterialPackageForComponent(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const FString & MaterialInfoDescriptor, FString & MaterialName, EBakeMode BakeMode );

        /** Create a package for a given component for texture. **/
        static UPackage * BakeCreateTexturePackageForComponent(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const HAPI_MaterialInfo & MaterialInfo, const FString & TextureType,
            FString & TextureName, EBakeMode BakeMode );
        static UPackage * BakeCreateTexturePackageForComponent(
            UHoudiniAssetComponent * HoudiniAssetComponent,
            const FString & TextureInfoDescriptor, const FString & TextureType,
            FString & TextureName, EBakeMode BakeMode );

        static bool CheckPackageSafeForBake( UPackage* Package, FString& FoundAssetName );

        /** Helper function to extract colors and store them in a given RawMesh. Returns number of wedges. **/
        static int32 TransferRegularPointAttributesToVertices(
            const TArray< int32 > & VertexList,
            const HAPI_AttributeInfo & AttribInfo, TArray< float > & Data );

#if WITH_EDITOR

        /** Duplicate a given material. This will create a new package for it. This will also create necessary textures **/
        /** and their corresponding packages. **/
        static UMaterial * DuplicateMaterialAndCreatePackage(
            UMaterial * Material, UHoudiniAssetComponent * Component,
            const FString & SubMaterialName, EBakeMode BakeMode );

        /** Duplicate a given texture. This will create a new package for it. **/
        static UTexture2D * DuplicateTextureAndCreatePackage(
            UTexture2D * Texture, UHoudiniAssetComponent * Component,
            const FString & SubTextureName, EBakeMode BakeMode );

        /** Replace duplicated texture with a new copy within a given sampling expression. **/
        static void ReplaceDuplicatedMaterialTextureSample(
            UMaterialExpression * MaterialExpression,
            UHoudiniAssetComponent * Component, EBakeMode BakeMode );

        /** Returns true if the supplied static mesh has unbaked (not backed by a .uasset) mesh or material */
        static bool StaticMeshRequiresBake( const UStaticMesh * StaticMesh );

        /** Helper for baking to actors */
        static TArray< AActor* > BakeHoudiniActorToActors_StaticMeshes( UHoudiniAssetComponent * HoudiniAssetComponent, 
            TMap< const UStaticMeshComponent*, FHoudiniGeoPartObject >& SMComponentToPart );
        /** Helper for baking to actors */
        static TArray< AActor* > BakeHoudiniActorToActors_InstancedActors( UHoudiniAssetComponent * HoudiniAssetComponent,
            TMap< const class UHoudiniInstancedActorComponent*, FHoudiniGeoPartObject >& ComponentToPart );
#endif

        /** Add Houdini meta information to package for a given object. **/
        static void AddHoudiniMetaInformationToPackage(
            UPackage * Package, UObject * Object, const TCHAR * Key, const TCHAR * Value );

        /** Retrieve item name from Houdini meta information. **/
        static bool GetHoudiniGeneratedNameFromMetaInformation(
            UPackage * Package, UObject * Object, FString & HoudiniName );

#if WITH_EDITOR

        /** Helper routine to check if Raw Mesh contains degenerate triangles. **/
        static bool ContainsDegenerateTriangles( const FRawMesh & RawMesh );

        /** Helper routine to count number of degenerate triangles. **/
        static int32 CountDegenerateTriangles( const FRawMesh & RawMesh );

        /** Helper routine to check invalid lightmap faces. **/
        static bool ContainsInvalidLightmapFaces( const FRawMesh & RawMesh, int32 LightmapSourceIdx );

#endif // WITH_EDITOR

        /** Helper function to extract a raw name from a given Fstring. Caller is responsible for clean up. **/
        static char * ExtractRawName( const FString & Name );

        /** Create helper array of material names, we use it for marshalling. **/
        static void CreateFaceMaterialArray(
            const TArray< FStaticMaterial > & Materials,
            const TArray< int32 > & FaceMaterialIndices,
            TArray< char * > & OutStaticMeshFaceMaterials );

        /** Delete helper array of material names. **/
        static void DeleteFaceMaterialArray( TArray< char * > & OutStaticMeshFaceMaterials );

        /** Return a specified HAPI status string. **/
        static const FString GetStatusString( HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity );

        /** Extract all unique material ids for all geo object parts. **/
        static bool ExtractUniqueMaterialIds(
            const HAPI_AssetInfo & AssetInfo, TSet< HAPI_NodeId > & MaterialIds,
            TSet< HAPI_NodeId > & InstancerMaterialIds,
            TMap< FHoudiniGeoPartObject, HAPI_NodeId > & InstancerMaterialMap );

        /** Helper function to locate first Material expression of given class within given expression subgraph. **/
        static UMaterialExpression * MaterialLocateExpression( UMaterialExpression * Expression, UClass * ExpressionClass );

        /** Pick vertex color from texture mip level. **/
        static FColor PickVertexColorFromTextureMip(
            const uint8 * MipBytes, FVector2D & UVCoord, int32 MipWidth, int32 MipHeight );

    protected:

#if WITH_EDITOR

        /** Create a texture from given information. **/
        static UTexture2D * CreateUnrealTexture(
            UTexture2D * ExistingTexture, const HAPI_ImageInfo & ImageInfo,
            UPackage * Package, const FString & TextureName,
            const TArray< char > & ImageBuffer, const FString & TextureType,
            const FCreateTexture2DParameters & TextureParameters, TextureGroup LODGroup );

        /** Reset streams used by the given RawMesh. **/
        static void ResetRawMesh( FRawMesh & RawMesh );

#endif // WITH_EDITOR

    public:

        /** How many GUID symbols are used for package component name generation. **/
        static const int32 PackageGUIDComponentNameLength;

        /** How many GUID symbols are used for package item name generation. **/
        static const int32 PackageGUIDItemNameLength;

        /** Material node construction offsets. **/
        static const int32 MaterialExpressionNodeX;
        static const int32 MaterialExpressionNodeY;
        static const int32 MaterialExpressionNodeStepX;
        static const int32 MaterialExpressionNodeStepY;
};
