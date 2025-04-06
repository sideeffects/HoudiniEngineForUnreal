/*
* Copyright (c) <2025> Side Effects Software Inc.
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
#include "HoudiniEnginePrivatePCH.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "HoudiniPCGTranslator.generated.h"

class UHoudiniOutput;
class UPCGMetadata;
class UPCGParamData;

UCLASS()
class UHoudiniPCGOutputData : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<UPCGParamData> DetailsParams = nullptr;

	UPROPERTY()
	TObjectPtr<UPCGParamData> PrimsParams = nullptr;

	UPROPERTY()
	TObjectPtr<UPCGParamData> VertexParams = nullptr;

	UPROPERTY()
	TObjectPtr<UPCGPointData> PointParams = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UPCGSplineData>> SplineParams;

};


struct HOUDINIENGINE_API FHoudiniPCGTranslator
{
public:
	static void CreatePCGFromOutput(UHoudiniOutput* CurOutput);
	static UHoudiniPCGOutputData* CreatePCGParamsOutput(UHoudiniOutput* CurOutput);
	static UHoudiniPCGOutputData* CreatePCGSplinesOutput(UHoudiniOutput* CurOutput);

	static bool IsPCGOutput(HAPI_NodeId NodeId, HAPI_PartId PartId);
private:
	static UPCGParamData * CreatePCGAttributes(HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner);
	static void CreatePCGAttributes(UPCGMetadata* MetaData, TArray<FString>& Attributes, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner);

	static UPCGPointData* CreatePCGPointData(HAPI_NodeId NodeId, HAPI_PartId PartId);

	static void CreatePCGInt32Attribute(UPCGMetadata* Metadata, const TArray<int64>& EntryKeys, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, FName AttrName);
	static void CreatePCGInt64Attribute(UPCGMetadata* Metadata, const TArray<int64>& EntryKeys, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, FName AttrName);
	static void CreatePCGFloatAttribute(UPCGMetadata* Metadata, const TArray<int64>& EntryKeys, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, FName AttrName);
	static void CreatePCGDoubleAttribute(UPCGMetadata* Metadata, const TArray<int64>& EntryKeys, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, FName AttrName);
	static void CreatePCGStringAttribute(UPCGMetadata* Metadata, const TArray<int64>& EntryKeys, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, FName AttrName);
};
