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


FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage()
	: FilePath()
	, Name()
	, TOPNodeId(-1)
	, WorkItemId(-1)
{
	StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	MeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
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
	StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	MeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath,
	const FString& InName,
	const FHoudiniPackageParams& InPackageParams,
	HAPI_NodeId InTOPNodeId,
	HAPI_PDG_WorkitemId InWorkItemId
)
	: FilePath(InFilePath)
	, Name(InName)
	, TOPNodeId(InTOPNodeId)
	, WorkItemId(InWorkItemId)
{
	SetPackageParams(InPackageParams);
	StaticMeshGenerationProperties = FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	MeshBuildSettings = FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath,
	const FString& InName,
	const FHoudiniPackageParams& InPackageParams,
	HAPI_NodeId InTOPNodeId,
	HAPI_PDG_WorkitemId InWorkItemId,
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
	PackageParams = InPackageParams;
	PackageParams.OuterPackage = nullptr;
}

void FHoudiniPDGImportBGEOMessage::PopulatePackageParams(FHoudiniPackageParams& OutPackageParams) const
{
	UObject* KeepOuter = OutPackageParams.OuterPackage;
	OutPackageParams = PackageParams;
	OutPackageParams.OuterPackage = KeepOuter;
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

