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

#include "HoudiniPDGImporterMessages.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniRuntimeSettings.h"

#include "Engine/AssetUserData.h"
#include "Engine/StaticMesh.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
template <typename ObjectType>
ObjectType* ResolveSoftObjectPath(const FSoftObjectPath& InPath)
{
	if (!InPath.IsValid())
		return nullptr;

	UObject* Object = InPath.ResolveObject();
	if (!Object)
		Object = InPath.TryLoad();

	return Cast<ObjectType>(Object);
}

FSoftObjectPath MakeSoftObjectPath(UObject* InObject)
{
	return InObject ? FSoftObjectPath(InObject) : FSoftObjectPath();
}
}


FHoudiniPDGImportPackageParams::FHoudiniPDGImportPackageParams()
{
	SetPackageParams(FHoudiniPackageParams());
}

FHoudiniPDGImportPackageParams::FHoudiniPDGImportPackageParams(const FHoudiniPackageParams& InPackageParams)
{
	SetPackageParams(InPackageParams);
}

void FHoudiniPDGImportPackageParams::SetPackageParams(const FHoudiniPackageParams& InPackageParams)
{
	PackageMode = InPackageParams.PackageMode;
	ReplaceMode = InPackageParams.ReplaceMode;
	BakeFolder = InPackageParams.BakeFolder;
	TempCookFolder = InPackageParams.TempCookFolder;
	OuterPackagePath = MakeSoftObjectPath(InPackageParams.OuterPackage.Get());
	ObjectName = InPackageParams.ObjectName;
	HoudiniAssetName = InPackageParams.HoudiniAssetName;
	HoudiniAssetActorName = InPackageParams.HoudiniAssetActorName;
	ObjectId = InPackageParams.ObjectId;
	GeoId = InPackageParams.GeoId;
	PartId = InPackageParams.PartId;
	SplitStr = InPackageParams.SplitStr;
	ComponentGUID = InPackageParams.ComponentGUID;
	PDGTOPNetworkName = InPackageParams.PDGTOPNetworkName;
	PDGTOPNodeName = InPackageParams.PDGTOPNodeName;
	PDGWorkItemIndex = InPackageParams.PDGWorkItemIndex;
	PDGWorkResultArrayIndex = InPackageParams.PDGWorkResultArrayIndex;
	NameOverride = InPackageParams.NameOverride;
	FolderOverride = InPackageParams.FolderOverride;
	OverideEnabled = InPackageParams.OverideEnabled;
}

void FHoudiniPDGImportPackageParams::PopulatePackageParams(FHoudiniPackageParams& OutPackageParams) const
{
	UObject* KeepOuter = OutPackageParams.OuterPackage;
	OutPackageParams.PackageMode = PackageMode;
	OutPackageParams.ReplaceMode = ReplaceMode;
	OutPackageParams.BakeFolder = BakeFolder;
	OutPackageParams.TempCookFolder = TempCookFolder;
	OutPackageParams.OuterPackage = KeepOuter ? KeepOuter : ResolveSoftObjectPath<UObject>(OuterPackagePath);
	OutPackageParams.ObjectName = ObjectName;
	OutPackageParams.HoudiniAssetName = HoudiniAssetName;
	OutPackageParams.HoudiniAssetActorName = HoudiniAssetActorName;
	OutPackageParams.ObjectId = ObjectId;
	OutPackageParams.GeoId = GeoId;
	OutPackageParams.PartId = PartId;
	OutPackageParams.SplitStr = SplitStr;
	OutPackageParams.ComponentGUID = ComponentGUID;
	OutPackageParams.PDGTOPNetworkName = PDGTOPNetworkName;
	OutPackageParams.PDGTOPNodeName = PDGTOPNodeName;
	OutPackageParams.PDGWorkItemIndex = PDGWorkItemIndex;
	OutPackageParams.PDGWorkResultArrayIndex = PDGWorkResultArrayIndex;
	OutPackageParams.NameOverride = NameOverride;
	OutPackageParams.FolderOverride = FolderOverride;
	OutPackageParams.OverideEnabled = OverideEnabled;
}

FHoudiniPDGImportStaticMeshGenerationProperties::FHoudiniPDGImportStaticMeshGenerationProperties()
{
	SetStaticMeshGenerationProperties(FHoudiniStaticMeshGenerationProperties());
}

FHoudiniPDGImportStaticMeshGenerationProperties::FHoudiniPDGImportStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties)
{
	SetStaticMeshGenerationProperties(InStaticMeshGenerationProperties);
}

void FHoudiniPDGImportStaticMeshGenerationProperties::SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties)
{
	bGeneratedDoubleSidedGeometry = InStaticMeshGenerationProperties.bGeneratedDoubleSidedGeometry;
	GeneratedPhysMaterialPath = MakeSoftObjectPath(InStaticMeshGenerationProperties.GeneratedPhysMaterial.Get());
	DefaultBodyInstanceCollisionProfileName = InStaticMeshGenerationProperties.DefaultBodyInstance.GetCollisionProfileName();
	DefaultBodyInstanceObjectType = InStaticMeshGenerationProperties.DefaultBodyInstance.GetObjectType();
	DefaultBodyInstanceCollisionEnabled = InStaticMeshGenerationProperties.DefaultBodyInstance.GetCollisionEnabled(false);
	DefaultBodyInstanceResponseToChannels = InStaticMeshGenerationProperties.DefaultBodyInstance.GetResponseToChannels();
	DefaultBodyInstancePhysMaterialOverridePath = MakeSoftObjectPath(InStaticMeshGenerationProperties.DefaultBodyInstance.GetPhysMaterialOverride());
	GeneratedCollisionTraceFlag = InStaticMeshGenerationProperties.GeneratedCollisionTraceFlag;
	GeneratedLightMapResolution = InStaticMeshGenerationProperties.GeneratedLightMapResolution;
	GeneratedWalkableSlopeOverride = InStaticMeshGenerationProperties.GeneratedWalkableSlopeOverride;
	GeneratedLightMapCoordinateIndex = InStaticMeshGenerationProperties.GeneratedLightMapCoordinateIndex;
	bGeneratedUseMaximumStreamingTexelRatio = InStaticMeshGenerationProperties.bGeneratedUseMaximumStreamingTexelRatio;
	GeneratedStreamingDistanceMultiplier = InStaticMeshGenerationProperties.GeneratedStreamingDistanceMultiplier;
	GeneratedFoliageDefaultSettingsPath = MakeSoftObjectPath(InStaticMeshGenerationProperties.GeneratedFoliageDefaultSettings.Get());

	GeneratedAssetUserDataPaths.Reset(InStaticMeshGenerationProperties.GeneratedAssetUserData.Num());
	for (UAssetUserData* AssetUserData : InStaticMeshGenerationProperties.GeneratedAssetUserData)
	{
		GeneratedAssetUserDataPaths.Add(MakeSoftObjectPath(AssetUserData));
	}
}

void FHoudiniPDGImportStaticMeshGenerationProperties::PopulateStaticMeshGenerationProperties(FHoudiniStaticMeshGenerationProperties& OutStaticMeshGenerationProperties) const
{
	OutStaticMeshGenerationProperties.bGeneratedDoubleSidedGeometry = bGeneratedDoubleSidedGeometry;
	OutStaticMeshGenerationProperties.GeneratedPhysMaterial = ResolveSoftObjectPath<UPhysicalMaterial>(GeneratedPhysMaterialPath);
	OutStaticMeshGenerationProperties.DefaultBodyInstance.SetCollisionProfileName(DefaultBodyInstanceCollisionProfileName);
	OutStaticMeshGenerationProperties.DefaultBodyInstance.SetObjectType(DefaultBodyInstanceObjectType.GetValue());
	OutStaticMeshGenerationProperties.DefaultBodyInstance.SetCollisionEnabled(DefaultBodyInstanceCollisionEnabled.GetValue(), false);
	OutStaticMeshGenerationProperties.DefaultBodyInstance.SetResponseToChannels(DefaultBodyInstanceResponseToChannels);
	OutStaticMeshGenerationProperties.DefaultBodyInstance.SetPhysMaterialOverride(ResolveSoftObjectPath<UPhysicalMaterial>(DefaultBodyInstancePhysMaterialOverridePath));
	OutStaticMeshGenerationProperties.GeneratedCollisionTraceFlag = GeneratedCollisionTraceFlag;
	OutStaticMeshGenerationProperties.GeneratedLightMapResolution = GeneratedLightMapResolution;
	OutStaticMeshGenerationProperties.GeneratedWalkableSlopeOverride = GeneratedWalkableSlopeOverride;
	OutStaticMeshGenerationProperties.GeneratedLightMapCoordinateIndex = GeneratedLightMapCoordinateIndex;
	OutStaticMeshGenerationProperties.bGeneratedUseMaximumStreamingTexelRatio = bGeneratedUseMaximumStreamingTexelRatio;
	OutStaticMeshGenerationProperties.GeneratedStreamingDistanceMultiplier = GeneratedStreamingDistanceMultiplier;
	OutStaticMeshGenerationProperties.GeneratedFoliageDefaultSettings = ResolveSoftObjectPath<UFoliageType_InstancedStaticMesh>(GeneratedFoliageDefaultSettingsPath);

	OutStaticMeshGenerationProperties.GeneratedAssetUserData.Reset(GeneratedAssetUserDataPaths.Num());
	for (const FSoftObjectPath& AssetUserDataPath : GeneratedAssetUserDataPaths)
	{
		if (UAssetUserData* AssetUserData = ResolveSoftObjectPath<UAssetUserData>(AssetUserDataPath))
		{
			OutStaticMeshGenerationProperties.GeneratedAssetUserData.Add(AssetUserData);
		}
	}
}

FHoudiniPDGImportMeshBuildSettings::FHoudiniPDGImportMeshBuildSettings()
{
	SetMeshBuildSettings(FMeshBuildSettings());
}

FHoudiniPDGImportMeshBuildSettings::FHoudiniPDGImportMeshBuildSettings(const FMeshBuildSettings& InMeshBuildSettings)
{
	SetMeshBuildSettings(InMeshBuildSettings);
}

void FHoudiniPDGImportMeshBuildSettings::SetMeshBuildSettings(const FMeshBuildSettings& InMeshBuildSettings)
{
	bUseMikkTSpace = InMeshBuildSettings.bUseMikkTSpace;
	bRecomputeNormals = InMeshBuildSettings.bRecomputeNormals;
	bRecomputeTangents = InMeshBuildSettings.bRecomputeTangents;
	bComputeWeightedNormals = InMeshBuildSettings.bComputeWeightedNormals;
	bRemoveDegenerates = InMeshBuildSettings.bRemoveDegenerates;
	bBuildReversedIndexBuffer = InMeshBuildSettings.bBuildReversedIndexBuffer;
	bUseHighPrecisionTangentBasis = InMeshBuildSettings.bUseHighPrecisionTangentBasis;
	bUseFullPrecisionUVs = InMeshBuildSettings.bUseFullPrecisionUVs;
	bUseBackwardsCompatibleF16TruncUVs = InMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs;
	bGenerateLightmapUVs = InMeshBuildSettings.bGenerateLightmapUVs;
	bGenerateDistanceFieldAsIfTwoSided = InMeshBuildSettings.bGenerateDistanceFieldAsIfTwoSided;
	bSupportFaceRemap = InMeshBuildSettings.bSupportFaceRemap;
	MinLightmapResolution = InMeshBuildSettings.MinLightmapResolution;
	SrcLightmapIndex = InMeshBuildSettings.SrcLightmapIndex;
	DstLightmapIndex = InMeshBuildSettings.DstLightmapIndex;
	BuildScale3D = InMeshBuildSettings.BuildScale3D;
	DistanceFieldResolutionScale = InMeshBuildSettings.DistanceFieldResolutionScale;
	DistanceFieldReplacementMeshPath = MakeSoftObjectPath(InMeshBuildSettings.DistanceFieldReplacementMesh.Get());
	MaxLumenMeshCards = InMeshBuildSettings.MaxLumenMeshCards;
}

void FHoudiniPDGImportMeshBuildSettings::PopulateMeshBuildSettings(FMeshBuildSettings& OutMeshBuildSettings) const
{
	OutMeshBuildSettings.bUseMikkTSpace = bUseMikkTSpace;
	OutMeshBuildSettings.bRecomputeNormals = bRecomputeNormals;
	OutMeshBuildSettings.bRecomputeTangents = bRecomputeTangents;
	OutMeshBuildSettings.bComputeWeightedNormals = bComputeWeightedNormals;
	OutMeshBuildSettings.bRemoveDegenerates = bRemoveDegenerates;
	OutMeshBuildSettings.bBuildReversedIndexBuffer = bBuildReversedIndexBuffer;
	OutMeshBuildSettings.bUseHighPrecisionTangentBasis = bUseHighPrecisionTangentBasis;
	OutMeshBuildSettings.bUseFullPrecisionUVs = bUseFullPrecisionUVs;
	OutMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs = bUseBackwardsCompatibleF16TruncUVs;
	OutMeshBuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	OutMeshBuildSettings.bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;
	OutMeshBuildSettings.bSupportFaceRemap = bSupportFaceRemap;
	OutMeshBuildSettings.MinLightmapResolution = MinLightmapResolution;
	OutMeshBuildSettings.SrcLightmapIndex = SrcLightmapIndex;
	OutMeshBuildSettings.DstLightmapIndex = DstLightmapIndex;
	OutMeshBuildSettings.BuildScale3D = BuildScale3D;
	OutMeshBuildSettings.DistanceFieldResolutionScale = DistanceFieldResolutionScale;
	OutMeshBuildSettings.DistanceFieldReplacementMesh = ResolveSoftObjectPath<UStaticMesh>(DistanceFieldReplacementMeshPath);
	OutMeshBuildSettings.MaxLumenMeshCards = MaxLumenMeshCards;
}
FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage()
	: FilePath()
	, Name()
	, TOPNodeId(-1)
	, WorkItemId(-1)
{
	StaticMeshGenerationProperties.SetStaticMeshGenerationProperties(FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties());
	MeshBuildSettings.SetMeshBuildSettings(FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings());
}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath, 
	const FString& InName, 
	const FHoudiniPackageParams& InPackageParams
)
	: FilePath(InFilePath)
	, Name(InName)
	, TOPNodeId(-1)
	, WorkItemId(-1)
{
	SetPackageParams(InPackageParams);
	StaticMeshGenerationProperties.SetStaticMeshGenerationProperties(FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties());
	MeshBuildSettings.SetMeshBuildSettings(FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings());
}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath,
	const FString& InName,
	const FHoudiniPackageParams& InPackageParams,
	HAPI_NodeId InTOPNodeId,
	HAPI_PDG_WorkItemId InWorkItemId
)
	: FilePath(InFilePath)
	, Name(InName)
	, TOPNodeId(InTOPNodeId)
	, WorkItemId(InWorkItemId)
{
	SetPackageParams(InPackageParams);
	StaticMeshGenerationProperties.SetStaticMeshGenerationProperties(FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties());
	MeshBuildSettings.SetMeshBuildSettings(FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings());
}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath,
	const FString& InName,
	const FHoudiniPackageParams& InPackageParams,
	HAPI_NodeId InTOPNodeId,
	HAPI_PDG_WorkItemId InWorkItemId,
	const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties,
	const FMeshBuildSettings& InMeshBuildSettings
)
	: FilePath(InFilePath)
	, Name(InName)
	, TOPNodeId(InTOPNodeId)
	, WorkItemId(InWorkItemId)
	, StaticMeshGenerationProperties(InStaticMeshGenerationProperties)
	, MeshBuildSettings(InMeshBuildSettings)
{
	SetPackageParams(InPackageParams);
}

void FHoudiniPDGImportBGEOMessage::SetPackageParams(const FHoudiniPackageParams& InPackageParams)
{
	PackageParams.SetPackageParams(InPackageParams);
}

void FHoudiniPDGImportBGEOMessage::PopulatePackageParams(FHoudiniPackageParams& OutPackageParams) const
{
	PackageParams.PopulatePackageParams(OutPackageParams);
}

FHoudiniPDGImportBGEOResultMessage::FHoudiniPDGImportBGEOResultMessage()
	: ImportResult(EHoudiniPDGImportBGEOResult::HPIBR_Failed)
{

}

FHoudiniPDGImportBGEOResultMessage::FHoudiniPDGImportBGEOResultMessage(
	const FString& InFilePath, 
	const FString& InName, 
	const FHoudiniPackageParams& InPackageParams, 
	const EHoudiniPDGImportBGEOResult& InImportResult
)
	: FHoudiniPDGImportBGEOMessage(InFilePath, InName, InPackageParams)
	, ImportResult(InImportResult)
{
}

FHoudiniPDGImportBGEODiscoverMessage::FHoudiniPDGImportBGEODiscoverMessage()
	: CommandletGuid()
{
	
}

FHoudiniPDGImportBGEODiscoverMessage::FHoudiniPDGImportBGEODiscoverMessage(const FGuid& InCommandletGuid)
	: CommandletGuid(InCommandletGuid)
{
	
}
