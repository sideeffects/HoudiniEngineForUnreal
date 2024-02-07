/*
* Copyright (c) <2022> Side Effects Software Inc.
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

#include "HAPI/HAPI.h"

#include "HoudiniPackageParams.h"

class UDataTable;

struct HOUDINIENGINE_API FHoudiniDataTableTranslator
{
	static bool BuildDataTable(const FHoudiniGeoPartObject& HGPO,
		UHoudiniOutput* CurOutput,
		FHoudiniPackageParams& PackageParams);

	static bool GenerateRowNames(int32 GeoId,
		int32 PartId,
		int32 NumRows,
		TArray<FString>& RowNames);

	static bool SetOutputPath(int32 GeoId,
		int32 PartId,
		HAPI_AttributeInfo& AttribInfo,
		FString& DataTableName,
		FString& DataTableFolder);

	static bool GetAttribNames(int32 GeoId,
		int32 PartId,
		int32 NumAttributes,
		HAPI_AttributeOwner AttribOwner,
		TArray<FString>& AttribNames);

	static bool GetRowStructAttrib(int32 GeoId,
		int32 PartId,
		HAPI_AttributeInfo& AttribInfo,
		UScriptStruct*& RowStruct,
		FString& RowStructName);

	static bool CreateRowStruct(const FHoudiniGeoPartObject& HGPO,
		UUserDefinedStruct*& NewStruct,
		FGuid& DefaultPropId,
		FHoudiniPackageParams PackageParams);

	static bool PopulatePropList(const UScriptStruct* RowStruct,
		TMap<FString, FProperty*>& ActualProps,
		TMap<FString, int32>& PropIndices);

	struct TransformComponents
	{
		FString Rotation = "";
		FString Translate = "";
		FString Scale = "";
	};

	static bool FindTransformAttribs(const TArray<FString>& AttribNames,
		TMap<FString, TransformComponents>& TransformCandidates,
		TSet<FString>& TransformAttribs);

	static bool CreateTransformProp(const FString& TAttrib,
		UUserDefinedStruct* NewStruct,
		int32& AssignedIdx,
		TMap<FString, FGuid>& CreatedIds);

	static bool ApplyTransformProp(const FString& TAttrib,
		const TransformComponents& Comps,
		const TMap<FString, int32>& PropIndices,
		TMap<FString, FProperty*>& ActualProps,
		TMap<FString, FProperty*>& FoundProps);

	static bool CreateRowStructProp(int32 GeoId,
		int32 PartId,
		const FString& AttribName,
		const FString& Fragment,
		HAPI_AttributeOwner AttribOwner,
		HAPI_AttributeInfo& AttribInfo,
		UUserDefinedStruct* NewStruct,
		int32& AssignedIdx,
		TMap<FString, FGuid>& CreatedIds,
		TMap<FString, HAPI_AttributeInfo>& FoundInfos);

	static bool ApplyRowStructProp(int32 GeoId,
		int32 PartId,
		const FString& AttribName,
		const FString& Fragment,
		HAPI_AttributeOwner AttribOwner,
		HAPI_AttributeInfo& AttribInfo,
		TMap<FString, FProperty*>& ActualProps,
		TMap<FString, FProperty*>& FoundProps,
		TMap<FString, int32>& PropIndices,
		TMap<FString, HAPI_AttributeInfo>& FoundInfos);

	static bool RemoveDefaultProp(UUserDefinedStruct* NewStruct,
		FGuid& DefaultPropId,
		int32& StructSize,
		TMap<FString, FGuid>& CreatedIds,
		const TSet<FString>& TransformAttribs,
		const TMap<FString, TransformComponents>& TransformCandidates,
		TMap<FString, FProperty*>& FoundProps);

	static bool PopulateRowData(int32 GeoId,
		int32 PartId,
		const TMap<FString, FProperty*>& FoundProps,
		const TMap<FString, HAPI_AttributeInfo>& FoundInfos,
		int32 StructSize,
		int32 NumRows,
		uint8* RowData);

	static UDataTable* CreateDataTable(const FHoudiniGeoPartObject& HGPO,
		int32 NumRows,
		const TArray<FString>& RowNames,
		int32 StructSize,
		UScriptStruct* RowStruct,
		uint8* RowData,
		FHoudiniPackageParams PackageParams);

private:
	static const FString DEFAULT_PROP_PREFIX;
	static const TMap<HAPI_StorageType, TArray<FName>> UE_TYPE_NAMES;
	static const TMap<HAPI_StorageType, TArray<UScriptStruct*>> VECTOR_STRUCTS;

	static void ExtractIndex(const FString& Fragment,
		int32& AttrIdx,
		int32& Slice);

	static bool IsValidType(const HAPI_AttributeInfo& AttribInfo,
		const FProperty* Prop);

	static void DeletePreviousOutput(UHoudiniOutput* CurOutput);
};