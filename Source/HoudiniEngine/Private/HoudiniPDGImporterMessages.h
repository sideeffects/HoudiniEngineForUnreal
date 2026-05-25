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
#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/Guid.h"

#include "HAPI/HAPI_Common.h"

#include "HoudiniPackageParams.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniInstanceTranslator.h"

#include "HoudiniPDGImporterMessages.generated.h"

struct FHoudiniStaticMeshGenerationProperties;
struct FMeshBuildSettings;

// Message used to find/discover running commandlets
USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportBGEODiscoverMessage
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportBGEODiscoverMessage();

	FHoudiniPDGImportBGEODiscoverMessage(const FGuid& InCommandletGuid);
	
	// The GUID of the commandlet we are looking for
	UPROPERTY()
	FGuid CommandletGuid;
};

/**
 * UDP-safe copy of FHoudiniPackageParams used by PDG import messages.
 *
 * FHoudiniPackageParams keeps a reflected OuterPackage UObject pointer. This wrapper mirrors the serializable
 * fields explicitly and transports the outer package as an FSoftObjectPath so UdpMessaging does not deserialize
 * a hard UObject pointer on its worker thread.
 */
USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportPackageParams
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportPackageParams();
	FHoudiniPDGImportPackageParams(const FHoudiniPackageParams& InPackageParams);

	void SetPackageParams(const FHoudiniPackageParams& InPackageParams);
	void PopulatePackageParams(FHoudiniPackageParams& OutPackageParams) const;

	UPROPERTY()
	EPackageMode PackageMode;

	UPROPERTY()
	EPackageReplaceMode ReplaceMode;

	UPROPERTY()
	FString BakeFolder;

	UPROPERTY()
	FString TempCookFolder;

	UPROPERTY()
	FSoftObjectPath OuterPackagePath;

	UPROPERTY()
	FString ObjectName;

	UPROPERTY()
	FString HoudiniAssetName;

	UPROPERTY()
	FString HoudiniAssetActorName;

	UPROPERTY()
	int32 ObjectId;

	UPROPERTY()
	int32 GeoId;

	UPROPERTY()
	int32 PartId;

	UPROPERTY()
	FString SplitStr;

	UPROPERTY()
	FGuid ComponentGUID;

	UPROPERTY()
	FString PDGTOPNetworkName;

	UPROPERTY()
	FString PDGTOPNodeName;

	UPROPERTY()
	int32 PDGWorkItemIndex;

	UPROPERTY()
	int32 PDGWorkResultArrayIndex;

	UPROPERTY()
	FString NameOverride;

	UPROPERTY()
	FString FolderOverride;

	UPROPERTY()
	bool OverideEnabled;
};

/**
 * UDP-safe copy of the static mesh generation settings used by PDG import messages.
 *
 * The original settings include hard UObject references and an FBodyInstance, which can make UE's CBOR
 * deserializer call StaticFindObject() on the UdpMessaging worker thread. This wrapper mirrors the plain
 * settings and carries object references as soft paths so they can be resolved later on the import path.
 */
USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportStaticMeshGenerationProperties
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportStaticMeshGenerationProperties();
	FHoudiniPDGImportStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties);

	void SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties);
	void PopulateStaticMeshGenerationProperties(FHoudiniStaticMeshGenerationProperties& OutStaticMeshGenerationProperties) const;

	UPROPERTY()
	bool bGeneratedDoubleSidedGeometry;

	UPROPERTY()
	FSoftObjectPath GeneratedPhysMaterialPath;

	UPROPERTY()
	FName DefaultBodyInstanceCollisionProfileName;

	UPROPERTY()
	TEnumAsByte<enum ECollisionChannel> DefaultBodyInstanceObjectType;

	UPROPERTY()
	TEnumAsByte<ECollisionEnabled::Type> DefaultBodyInstanceCollisionEnabled;

	UPROPERTY()
	FCollisionResponseContainer DefaultBodyInstanceResponseToChannels;

	UPROPERTY()
	FSoftObjectPath DefaultBodyInstancePhysMaterialOverridePath;

	UPROPERTY()
	TEnumAsByte<enum ECollisionTraceFlag> GeneratedCollisionTraceFlag;

	UPROPERTY()
	int32 GeneratedLightMapResolution;

	UPROPERTY()
	FWalkableSlopeOverride GeneratedWalkableSlopeOverride;

	UPROPERTY()
	int32 GeneratedLightMapCoordinateIndex;

	UPROPERTY()
	bool bGeneratedUseMaximumStreamingTexelRatio;

	UPROPERTY()
	float GeneratedStreamingDistanceMultiplier;

	UPROPERTY()
	FSoftObjectPath GeneratedFoliageDefaultSettingsPath;

	UPROPERTY()
	TArray<FSoftObjectPath> GeneratedAssetUserDataPaths;
};

/**
 * UDP-safe copy of FMeshBuildSettings used by PDG import messages.
 *
 * FMeshBuildSettings contains a reflected UStaticMesh reference for DistanceFieldReplacementMesh. This wrapper
 * mirrors the remaining build settings and transports that mesh as an FSoftObjectPath to avoid deserializing a
 * hard UObject pointer in the UDP worker.
 */
USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportMeshBuildSettings
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportMeshBuildSettings();
	FHoudiniPDGImportMeshBuildSettings(const FMeshBuildSettings& InMeshBuildSettings);

	void SetMeshBuildSettings(const FMeshBuildSettings& InMeshBuildSettings);
	void PopulateMeshBuildSettings(FMeshBuildSettings& OutMeshBuildSettings) const;

	UPROPERTY()
	bool bUseMikkTSpace;

	UPROPERTY()
	bool bRecomputeNormals;

	UPROPERTY()
	bool bRecomputeTangents;

	UPROPERTY()
	bool bComputeWeightedNormals;

	UPROPERTY()
	bool bRemoveDegenerates;

	UPROPERTY()
	bool bBuildReversedIndexBuffer;

	UPROPERTY()
	bool bUseHighPrecisionTangentBasis;

	UPROPERTY()
	bool bUseFullPrecisionUVs;

	UPROPERTY()
	bool bUseBackwardsCompatibleF16TruncUVs;

	UPROPERTY()
	bool bGenerateLightmapUVs;

	UPROPERTY()
	bool bGenerateDistanceFieldAsIfTwoSided;

	UPROPERTY()
	bool bSupportFaceRemap;

	UPROPERTY()
	int32 MinLightmapResolution;

	UPROPERTY()
	int32 SrcLightmapIndex;

	UPROPERTY()
	int32 DstLightmapIndex;

	UPROPERTY()
	FVector BuildScale3D;

	UPROPERTY()
	float DistanceFieldResolutionScale;

	UPROPERTY()
	FSoftObjectPath DistanceFieldReplacementMeshPath;

	UPROPERTY()
	int32 MaxLumenMeshCards;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportBGEOMessage
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportBGEOMessage();

	FHoudiniPDGImportBGEOMessage(const FString& InFilePath, const FString& InName, const FHoudiniPackageParams& InPackageParams);

	FHoudiniPDGImportBGEOMessage(
		const FString& InFilePath, 
		const FString& InName, 
		const FHoudiniPackageParams& InPackageParams,
		HAPI_NodeId InTOPNodeId,
		HAPI_PDG_WorkItemId InWorkItemId);

	FHoudiniPDGImportBGEOMessage(
		const FString& InFilePath,
		const FString& InName,
		const FHoudiniPackageParams& InPackageParams,
		HAPI_NodeId InTOPNodeId,
		HAPI_PDG_WorkItemId InWorkItemId,
		const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties,
		const FMeshBuildSettings& InMeshBuildSettings);

	void SetPackageParams(const FHoudiniPackageParams& InPackageParams);

	void PopulatePackageParams(FHoudiniPackageParams &OutPackageParams) const;

	// BGEO file path
	UPROPERTY()
	FString FilePath;

	// PDG work item name
	UPROPERTY()
	FString Name;

	// TOP/PDG info
	// TOP node ID
	UPROPERTY()
	// HAPI_NodeId TOPNodeId;
	int32 TOPNodeId;

	// Work item id
	UPROPERTY()
	// HAPI_PDG_WorkItemId WorkItemId;
	int32 WorkItemId;

	// Package params for the asset. Uses a wire-safe copy that excludes UObject references.
	UPROPERTY()
	FHoudiniPDGImportPackageParams PackageParams;

	// Settings used during static mesh generation
	UPROPERTY()
	FHoudiniPDGImportStaticMeshGenerationProperties StaticMeshGenerationProperties;

	// Static mesh build settings used during mesh builds
	UPROPERTY()
	FHoudiniPDGImportMeshBuildSettings MeshBuildSettings;
};


UENUM()
enum class EHoudiniPDGImportBGEOResult : uint8
{
	// Create uassets from the bgeo completely failed.
	HPIBR_Failed UMETA(DisplayName="Failed"),

	// Successfully created uassets for all content in the bgeo
	HPIBR_Success UMETA(DisplayName = "Success"),

	// Some uassets were created, but there were unsupported objects in the bgeo as well
	HPIBR_PartialSuccess UMETA(DisplayName = "Partial Success"),

	HIBPR_MAX
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniGenericAttributes
{
public:
	GENERATED_BODY()

	FHoudiniGenericAttributes() {};
	FHoudiniGenericAttributes(const TArray<FHoudiniGenericAttribute>& InPropertyAttributes) : PropertyAttributes(InPropertyAttributes) {};
	FHoudiniGenericAttributes(TArray<FHoudiniGenericAttribute>&& InPropertyAttributes) : PropertyAttributes(InPropertyAttributes) {};

	UPROPERTY()
	TArray<FHoudiniGenericAttribute> PropertyAttributes;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportNodeOutputObject
{
public:
	GENERATED_BODY();

	UPROPERTY()
	FHoudiniOutputObjectIdentifier Identifier;

	UPROPERTY()
	FString PackagePath;

	UPROPERTY()
	FHoudiniGenericAttributes GenericAttributes;

	UPROPERTY()
	TMap<FString,FString> CachedAttributes;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportNodeOutput
{
public:
	GENERATED_BODY();

	UPROPERTY()
	TArray<FHoudiniGeoPartObject> HoudiniGeoPartObjects;

	UPROPERTY()
	TArray<FHoudiniPDGImportNodeOutputObject> OutputObjects;

	UPROPERTY()
	TArray<FHoudiniInstancerPartData> InstancedOutputPartData;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportBGEOResultMessage : public FHoudiniPDGImportBGEOMessage
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportBGEOResultMessage();

	FHoudiniPDGImportBGEOResultMessage(const FString& InFilePath, const FString& InName, const FHoudiniPackageParams& InPackageParams, const EHoudiniPDGImportBGEOResult& InImportResult);

	void operator=(const FHoudiniPDGImportBGEOMessage& InRHS) { (*static_cast<FHoudiniPDGImportBGEOMessage*>(this)) = InRHS; }

	// Result of the bgeo import -> uassets
	UPROPERTY()
	EHoudiniPDGImportBGEOResult ImportResult;

	UPROPERTY()
	TArray<FHoudiniPDGImportNodeOutput> Outputs;

};
