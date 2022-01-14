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

#include "HAPI/HAPI_Common.h"
#include "Landscape.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HoudiniEngineOutputStats.h"
#include "HoudiniPackageParams.h"
#include "HoudiniTranslatorTypes.h"

class UHoudiniAssetComponent;
class ULandscapeLayerInfoObject;
struct FHoudiniGenericAttribute;
struct FHoudiniPackageParams;

struct HOUDINIENGINE_API FHoudiniLandscapeTranslator
{
	public:
		enum class LandscapeActorType : uint8
		{
			LandscapeActor = 0,
			LandscapeStreamingProxy = 1,
		};

		static bool CreateLandscape(
			UHoudiniOutput* InOutput,
			TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
			TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			USceneComponent* SharedLandscapeActorParent,
			const FString& DefaultLandscapeActorPrefix,
			UWorld* World,
			const TMap<FString, float>& LayerMinimums,
			const TMap<FString, float>& LayerMaximums,
			FHoudiniLandscapeExtent& LandscapeExtent,
			FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
			FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
			FHoudiniPackageParams InPackageParams,
			TSet<FString>& ClearedLayers,
			TArray<UPackage*>& OutCreatedPackages);

		static bool OutputLandscape_Generate(
			UHoudiniOutput* InOutput,
			TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
			TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			USceneComponent* SharedLandscapeActorParent,
			const FString& DefaultLandscapeActorPrefix,
			UWorld* World,
			const TMap<FString, float>& LayerMinimums,
			const TMap<FString, float>& LayerMaximums,
			FHoudiniLandscapeExtent& LandscapeExtent,
			FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
			FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
			TSet<FString>& ClearedLayers,
			FHoudiniPackageParams InPackageParams,
			TArray<UPackage*>& OutCreatedPackages);

		static bool OutputLandscape_GenerateTile(
			UHoudiniOutput* InOutput,
			TArray<FHoudiniOutputObjectIdentifier>& StaleOutputObjects,
			TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
			TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			USceneComponent* SharedLandscapeActorParent,
			const FString& DefaultLandscapeActorPrefix,
			UWorld* World,
			const TMap<FString, float>& LayerMinimums,
			const TMap<FString, float>& LayerMaximums,
			FHoudiniLandscapeExtent& LandscapeExtent,
			FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
			FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
			FHoudiniPackageParams InPackageParams,
			bool bHasEditLayers,
			const FString& InEditLayerName,
			const FName& InAfterLayerName,
			const TArray<FName>& AllLayerNames,
			TSet<FString>& ClearedLayers,
			TArray<UPackage*>& OutCreatedPackages,
			TSet<ALandscapeProxy*>& OutActiveLandscapes);

		// Outputting landscape as "editable layers" differs significantly from
		// landscape outputs in "temp mode". To avoid a bigger spaghetti mess, we're
		// dealing with modifying existing edit layers completely separately.

		static bool OutputLandscape_ModifyLayers(
			UHoudiniOutput* InOutput,
			TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
			TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			USceneComponent* SharedLandscapeActorParent,
			const FString& DefaultLandscapeActorPrefix,
			UWorld* World,
			const TMap<FString, float>& LayerMinimums,
			const TMap<FString, float>& LayerMaximums,
			FHoudiniLandscapeExtent& LandscapeExtent,
			FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
			FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
			FHoudiniPackageParams InPackageParams,
			TSet<FString>& ClearedLayers,
			TArray<UPackage*>& OutCreatedPackages);
	
		static bool OutputLandscape_ModifyLayer(
			UHoudiniOutput* InOutput,
			TArray<TWeakObjectPtr<AActor>>& CreatedUntrackedActors,
			TArray<ALandscapeProxy*>& InputLandscapesToUpdate,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			USceneComponent* SharedLandscapeActorParent,
			const FString& DefaultLandscapeActorPrefix,
			UWorld* World,
			const TMap<FString, float>& LayerMinimums,
			const TMap<FString, float>& LayerMaximums,
			FHoudiniLandscapeExtent& LandscapeExtent,
			FHoudiniLandscapeTileSizeInfo& LandscapeTileSizeInfo,
			FHoudiniLandscapeReferenceLocation& LandscapeReferenceLocation,
			FHoudiniPackageParams InPackageParams,
			const bool bHasEditLayers,
			const FName& EditLayerName,
			TSet<FString>& ClearedLayers,
			TArray<UPackage*>& OutCreatedPackages);

		static ALandscapeProxy* FindExistingLandscapeActor_Bake(
			UWorld* InWorld,
			UHoudiniOutput* InOutput,
			const TArray<ALandscapeProxy *>& ValidLandscapes,
			int32 UnrealLandscapeSizeX,
			int32 UnrealLandscapeSizeY,
			const FString& InActorName,
			const FString& InPackagePath, // Package path to search if not found in the world
			UWorld*& OutWorld,
			ULevel*& OutLevel,
			bool& bCreatedPackage); 

		static ALandscapeProxy* FindTargetLandscapeProxy(
			const FString& ActorName,
			UWorld* World,
			const TArray<ALandscapeProxy*>& LandscapeInputs
			);

		// Iterate through the HGPO's and collect the (non-empty) edit layer names retrieved by the Houdini Output Translator.
		// Return false if there are no edit layers (InEditLayers will also be emptied). 
		static bool GetEditLayersFromOutput(UHoudiniOutput* InOutput, TArray<FString>& InEditLayers);

		static void GetLandscapePaintLayers(ALandscape* Landscape, TMap<FName, int32>& EditLayers);

	protected:

		static bool IsLandscapeInfoCompatible(
			const ULandscapeInfo* LandscapeInfo,
			const int32 NumSectionsX,
			const int32 NumSectionsY);

		static bool IsLandscapeTileCompatible(
			const ALandscapeProxy* TileActor,
			const int32 InTileSizeX,
			const int32 InTileSizeY,
			const int32 InNumSectionsPerComponent,
			const int32 InNumQuadsPerSection);

		static bool IsLandscapeTypeCompatible(
			const AActor* Actor,
			LandscapeActorType ActorType);

		static bool PopulateLandscapeExtents(
			FHoudiniLandscapeExtent& Extent,
			const ULandscapeInfo* LandscapeInfo
			);

		static bool UpdateLandscapeMaterialLayers(
			ALandscape* InLandscape,
			const TArray<FLandscapeImportLayerInfo>& LayerInfos,
			TMap<FName, int32>& OutPaintLayers,
			bool bHasEditLayers,
			const FName& LayerName
			);

		// static bool ClearLandscapeLayers(
		// 	ALandscape* InLandscape,
		// 	const TArray<FLandscapeImportLayerInfo>& LayerInfos,
		// 	TSet<FString>& ClearedLayers,
		// 	bool bHasEditLayer,
		// 	const FString& EditLayerName
		// 	);

		/**
	     * Find a ALandscapeProxy actor that can be reused. It is important
	     * to note that the request landscape actor could not be found, 
	     * `OutWorld` and `OutLevel` should be used to spawn the
	     * new landscape actor.
	     * @returns ALandscapeProxy* if found. Otherwise, returns nullptr.
	     */
		static ALandscapeProxy* FindExistingLandscapeActor(
			UWorld* InWorld,
			UHoudiniOutput* InOutput,
			const TArray<ALandscapeProxy *>& ValidLandscapes,
			int32 UnrealLandscapeSizeX,
			int32 UnrealLandscapeSizeY,
			const FString& InActorName,
			const FString& InPackagePath, // Package path to search if not found in the world
			UWorld*& OutWorld,
			ULevel*& OutLevel,
			bool& bCreatedPackage,
			const EPackageMode& InPackageMode);

		static ALandscapeProxy* FindExistingLandscapeActor_Temp(
			UWorld* InWorld,
			UHoudiniOutput* InOutput,
			const TArray<ALandscapeProxy *>& ValidLandscapes,
			int32 UnrealLandscapeSizeX,
			int32 UnrealLandscapeSizeY,
			const FString& InActorName,
			const FString& InPackagePath, // Package path to search if not found in the world
			UWorld*& OutWorld,
			ULevel*& OutLevel,
			bool& bCreatedPackage);

		/**
	     * Attempt the given ALandscapeActor to the outer HAC. Note
	     * that certain package modes (such as Bake) may choose not to do so.
	     * @returns ALandscapeProxy* if found. Otherwise, returns nullptr.
	     */
		static void SetLandscapeActorAsOutput(
			UHoudiniOutput* InOutput,
			TArray<FHoudiniOutputObjectIdentifier> &StaleOutputObjects,
			TSet<ALandscapeProxy*>& OutActiveLandscapes,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			const TMap<FString,FString>& OutputAttributes,
			const TMap<FString,FString>& OutputArguments,
			const FName& EditLayerName,
			ALandscape* SharedLandscapeActor,
			USceneComponent* SharedLandscapeActorParent,
			bool bCreatedMainLandscape,
			const FHoudiniOutputObjectIdentifier& Identifier,
			ALandscapeProxy* LandscapeActor,
			const EPackageMode InPackageMode);

		static void SetLandscapeActorAsOutput_Bake(
			UHoudiniOutput* InOutput,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			const TMap<FString, FString>& OutputAttributes,
			const TMap<FString, FString>& OutputArguments,
			const FName& EditLayerName,
			ALandscape* SharedLandscapeActor,
			USceneComponent* SharedLandscapeActorParent,
			bool bCreatedMainLandscape,
			const FHoudiniOutputObjectIdentifier& Identifier,
			ALandscapeProxy* LandscapeActor);

		static void SetLandscapeActorAsOutput_Temp(
			UHoudiniOutput* InOutput,
			TArray<FHoudiniOutputObjectIdentifier> &StaleOutputObjects,
			TSet<ALandscapeProxy*>& OutActiveLandscapes,
			const TArray<ALandscapeProxy*>& InAllInputLandscapes,
			const TMap<FString, FString>& OutputAttributes,
			const TMap<FString, FString>& OutputArguments,
			const FName& EditLayerName,
			ALandscape* SharedLandscapeActor,
			USceneComponent* SharedLandscapeActorParent,
			bool bCreatedMainLandscape,
			const FHoudiniOutputObjectIdentifier& Identifier,
			ALandscapeProxy* LandscapeActor);

		/**
		 * Attach the given actor the HoudiniAssetComponent that 
		 * owns `InOutput`, if any.
		 * @returns True if the actor was attached. Otherwise, return false.
		 */
		static bool AttachActorToHAC(UHoudiniOutput* InOutput, AActor* InActor);

		/** 
		 * Get the actor name suffix to be used in the specific packaging mode. 
		 * @returns Suffix for actor names, return as an FString.
		 */
		static FString GetActorNameSuffix(const EPackageMode& InPackageMode);


		// Helpers to get rid of repetitive boilerplate.
		static void DoPreEditChangeProperty(UObject* Obj, FName PropertyName);
		static void DoPostEditChangeProperty(UObject* Obj, FName PropertyName);

	public:


		static const FHoudiniGeoPartObject* GetHoudiniHeightFieldFromOutput(
			UHoudiniOutput* InOutput,
			const bool bMatchEditLayer,
			const FName& EditLayerName);

		static void GetHeightfieldsLayersFromOutput(
			const ::UHoudiniOutput* InOutput,
			const ::FHoudiniGeoPartObject& Heightfield,
			const bool bMatchEditLayer, const ::FName& EditLayerName, TArray<const FHoudiniGeoPartObject*>& FoundLayers);

		static bool GetHoudiniHeightfieldVolumeInfo(const FHoudiniGeoPartObject* HGPO, HAPI_VolumeInfo& VolumeInfo);

		static bool GetHoudiniHeightfieldFloatData(
			const FHoudiniGeoPartObject* HGPO,
			TArray<float> &OutFloatArr,
			float &OutFloatMin,
			float &OutFloatMax);

		static bool CalcLandscapeSizeFromHeightfieldSize(
			const int32& HoudiniSizeX,
			const int32& HoudiniSizeY,
			int32& UnrealSizeX,
			int32& UnrealSizeY,
			int32& NumSectionsPerComponent,
			int32& NumQuadsPerSection);

		static bool ConvertHeightfieldDataToLandscapeData(
			const TArray< float >& HeightfieldFloatValues,
			const FHoudiniVolumeInfo& HeightfieldVolumeInfo,
			const int32& FinalXSize,
			const int32& FinalYSize,
			float FloatMin,
			float FloatMax,
			TArray< uint16 >& IntHeightData,
			FTransform& LandscapeTransform,
			const bool NoResize = false,
			const bool bOverrideZScale = false,
			const float CustomZScale = 100.f,
			const bool bIsAdditive = false);

		static bool ResizeHeightDataForLandscape(
			TArray<uint16>& HeightData,
			const int32& SizeX,
			const int32& SizeY,
			const int32& NewSizeX,
			const int32& NewSizeY,
			FVector& LandscapeResizeFactor,
			FVector& LandscapePositionOffset);

		static bool CreateOrUpdateLandscapeLayerData(
			const TArray<const FHoudiniGeoPartObject*>& FoundLayers,
			const FHoudiniGeoPartObject& HeightField,
			const int32& LandscapeXSize,
			const int32& LandscapeYSize,
			const TMap<FString, float> &GlobalMinimums,
			const TMap<FString, float> &GlobalMaximums,
			TArray<FLandscapeImportLayerInfo>& OutLayerInfos,
			bool bIsUpdate,
			bool bDefaultNoWeightBlending,
			const FHoudiniPackageParams& InTilePackageParams,
			const FHoudiniPackageParams& InLayerPackageParams,
			TArray<UPackage*>& OutCreatedPackages,
			TMap<FName, const FHoudiniGeoPartObject*>& OutLayerObjectMapping
			);

		static void UpdateLayerProperties(
			ALandscape* TargetLandscape,
			const FLandscapeLayer* TargetLayer,
			const FLandscapeImportLayerInfo& InLayerInfo,
			const TMap<FName, const FHoudiniGeoPartObject*>& LayerObjectMapping
			);

		static bool GetNonWeightBlendedLayerNames(
			const FHoudiniGeoPartObject& HeightfieldGeoPartObject,
			TArray<FString>& NonWeightBlendedLayerNames);

		static bool GetIsLayerSubtractive(
			const FHoudiniGeoPartObject& HeightfieldGeoPartObject,
			int& LayerCompositeMode);

		static bool GetIsLayerWeightBlended(
			const FHoudiniGeoPartObject& HeightfieldGeoPartObject,
			bool& bIsLayerNoWeightBlended);

		static bool IsUnitLandscapeLayer(
			const FHoudiniGeoPartObject& LayerGeoPartObject);

		// Return the height min/max values for all 
		static bool CalcHeightGlobalZminZMax(
			const TArray<UHoudiniOutput*>& AllOutputs,
			float& OutGlobalMin,
			float& OutGlobalMax);

		// Returns the min/max values per layer/volume for an array of volumes/heightfields
		static void CalcHeightfieldsArrayGlobalZMinZMax(
			const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
			TMap<FString, float>& GlobalMinimums,
			TMap<FString, float>& GlobalMaximums,
			bool bShouldEmptyMaps=true);

		// Iterate over layers for the heightfields and retrieve min/max values
		// from attributes, otherwise return default values.
		static void GetLayersZMinZMax(
			const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
			TMap<FString, float>& GlobalMinimums,
			TMap<FString, float>& GlobalMaximums);

		static bool ConvertHeightfieldLayerToLandscapeLayer(
			const TArray<float>& FloatLayerData,
			const int32& HoudiniXSize,
			const int32& HoudiniYSize,
			const float& LayerMin,
			const float& LayerMax,
			const int32& LandscapeXSize,
			const int32& LandscapeYSize,
			TArray<uint8>& LayerData,
			const bool& NoResize = false);

		static bool ResizeLayerDataForLandscape(
			TArray< uint8 >& LayerData,
			const int32& SizeX,
			const int32& SizeY,
			const int32& NewSizeX,
			const int32& NewSizeY);

		// static ALandscapeProxy * CreateLandscape(
		// 	const TArray< uint16 >& IntHeightData,
		// 	const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
		// 	const FTransform& LandscapeTransform,
		// 	const int32& XSize,
		// 	const int32& YSize,
		// 	const int32& NumSectionPerLandscapeComponent,
		// 	const int32& NumQuadsPerLandscapeSection,
		// 	UMaterialInterface* LandscapeMaterial,
		// 	UMaterialInterface* LandscapeHoleMaterial,
		// 	const bool& CreateLandscapeStreamingProxy,
		// 	bool bNeedCreateNewWorld,
		// 	UWorld* SpawnWorld,
		// 	FHoudiniPackageParams InPackageParams,
		// 	bool& bOutCreatedNewMap);

		static ALandscapeProxy* CreateLandscapeTileInWorld(
			const TArray< uint16 >& IntHeightData,
			const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
			const FTransform& TileTransform,
			const FIntPoint& TileLocation,
			const int32& XSize,
			const int32& YSize,
			const int32& NumSectionPerLandscapeComponent,
			const int32& NumQuadsPerLandscapeSection,
			UMaterialInterface* LandscapeMaterial,
			UMaterialInterface* LandscapeHoleMaterial,
			UPhysicalMaterial* LandscapePhysicalMaterial,
			const FString& LandscapeTileActorName,
			LandscapeActorType ActorType,
			ALandscape* SharedLandscapeActor, // Landscape containing shared stated for streaming proxies
			UWorld* InWorld, // World in which to spawn
			ULevel* InLevel, // Level, contained in World, in which to spawn.
			FHoudiniPackageParams InPackageParams,
			bool bHasEditLayers,
			const FName& EditLayerName,
			const FName& AfterLayerName);

		// Destroy the given landscape and all its proxies
		static void DestroyLandscape(ALandscape* Landscape);

protected:
		/** 
		 * Calculate the location of a landscape tile.
		 * This location is typically used in conjunction with ALandscapeProxy::SetAbsoluteSectionBase().
		 */
		static void CalculateTileLocation(
			int32 NumSectionsPerComponent,
			int32 NumQuadsPerSection,
			const FTransform& InTileTransform,
			FHoudiniLandscapeReferenceLocation& RefLoc,
			FTransform& OutLandscapeTransform,
			FIntPoint& OutTileLocation);

public:

		static void GetLandscapeMaterials(
			const FHoudiniGeoPartObject& InHeightHGPO,
			const FHoudiniPackageParams& InPackageParams,
			UMaterialInterface*& OutLandscapeMaterial,
			UMaterialInterface*& OutLandscapeHoleMaterial,
			UPhysicalMaterial*& OutLandscapePhysicalMaterial);

		static bool GetLandscapeComponentExtentAttributes(
			const FHoudiniGeoPartObject& HoudiniGeoPartObject,
			int32& MinX,
			int32& MaxX,
			int32& MinY,
			int32& MaxY);

		static ULandscapeLayerInfoObject* FindOrCreateLandscapeLayerInfoObject(
			const FString& InLayerName,
			const FString& InPackagePath,
			const FString& InPackageName,
			UPackage*& OutPackage,
			bool& bCreatedPackaged);

		static bool EnableWorldComposition();

		static bool GetGenericPropertiesAttributes(
			const HAPI_NodeId& InGeoNodeId,
			const HAPI_PartId& InPartId,
			const int32& InPrimIndex,
			TArray<FHoudiniGenericAttribute>& OutPropertyAttributes);
		
		static bool UpdateGenericPropertiesAttributes(
			UObject* InObject,
			const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes);

		static bool BackupLandscapeToImageFiles(
			const FString& BaseName, ALandscapeProxy* Landscape);

		static bool RestoreLandscapeFromImageFiles(ALandscapeProxy* LandscapeProxy);

		static UPhysicalMaterial* GetLandscapePhysicalMaterial(const FHoudiniGeoPartObject& InLayerHGPO);

		static ULandscapeLayerInfoObject* GetLandscapeLayerInfoForLayer(const FHoudiniGeoPartObject& InLayerHGPO, const FName& InLayerName);

		// Find or create the given layer. Optionally position it after the 'AfterLayerName'.
		static int32 FindOrCreateEditLayer(ALandscape* Landscape, FName LayerName, FName AfterLayerName);
		static bool SetActiveLandscapeLayer(ALandscape* Landscape, int32 LayerIndex);

	private:

		static bool ImportLandscapeData(
			ULandscapeInfo* LandscapeInfo,
			const FString& Filename,
			const FString& LayerName,
			ULandscapeLayerInfoObject* LayerInfoObject = nullptr);

		static UTexture2D* CreateUnrealTexture(
			const FHoudiniPackageParams& InPackageParams,
			const FString& LayerName,
			const int32& InXSize,
			const int32& InYSize,
			const TArray<float>& InFloatBuffer,
			const float& InMin,
			const float& InMax);

		static UTexture2D* CreateUnrealTexture(
			const FHoudiniPackageParams& InPackageParams,
			const FString& LayerName,
			const int32& InXSize,
			const int32& InYSize,
			const TArray<uint8>& IntBuffer);
};