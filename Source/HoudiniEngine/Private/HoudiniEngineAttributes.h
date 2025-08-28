/*
* Copyright (c) <2024> Side Effects Software Inc.
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
#include "HoudiniEngineRuntimeCommon.h"
#include "HAPI/HAPI_Common.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineString.h"

class FHoudiniEngineIndexedStringMap;
struct FHoudiniRawAttributeData;

struct HOUDINIENGINE_API FHoudiniHapiAccessor
{
	// Public data. Can be set directly or use convenience functions below.

	HAPI_NodeId NodeId = -1;
	HAPI_PartId PartId = -1;
	TArray<char> AttributeName;// avoid std::string
	bool bAllowTypeConversion = true;
	bool bAllowMultiThreading = true;
	bool bCanBeArray = false;
	bool bCanRunLengthEncode = true;
	int MinElementsForRunLengthEncoding = 100 * 1000;

	// Initialization functions. use these functions to initialize the accessor.
	FHoudiniHapiAccessor(HAPI_NodeId NodeId, HAPI_NodeId PartId, const char* Name);
	FHoudiniHapiAccessor(HAPI_NodeId NodeId, HAPI_NodeId PartId, const TCHAR* Name);
	FHoudiniHapiAccessor() {}
	void Init(HAPI_NodeId InNodeId, HAPI_NodeId InPartId, const char * InName);

	// Create the attribute. Will fill in OutAttrInfo if supplied - You probably want this because calling
	// GetInfo() will fail if the node is not commited.
	bool AddAttribute(HAPI_AttributeOwner Owner, HAPI_StorageType StorageType, int TupleSize, int Count, HAPI_AttributeInfo * OutAttrInfo = nullptr);

	// Get HAPI_AttributeInfo from the accessor. 
	bool GetInfo(HAPI_AttributeInfo& OutAttributeInfo, HAPI_AttributeOwner InOwner = HAPI_ATTROWNER_INVALID);

	// Templated functions to return data.
	//
	// If the attribute is a different type from the templated data, conversion is optionally performed (bAllowTypeConversion)
	// If the attribute owner is HAPI_ATTROWNER_INVALID, all attribute owners are searched.
	// An optional tuple size can be specified.
	// Note these templates are explicitly defined in the .cpp file.

	template<typename DataType> HOUDINIENGINE_API bool GetAttributeData(HAPI_AttributeOwner Owner, TArray<DataType>& Results, int IndexStart =0, int IndexCount =-1);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeData(HAPI_AttributeOwner Owner, DataType* Results, int IndexStart = 0, int IndexCount = -1);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, TArray<DataType>& Results, int IndexStart = 0, int IndexCount = -1);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, DataType* Results, int IndexStart = 0, int IndexCount = -1);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeFirstValue(HAPI_AttributeOwner Owner, DataType & Result);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeArrayData(HAPI_AttributeOwner Owner, TArray<DataType>& InStringArray, TArray<int>& SizesFixedArray, int IndexStart = 0, int IndexCount = -1);

	template<typename DataType> HOUDINIENGINE_API bool GetAttributeArrayData(const HAPI_AttributeInfo& AttributeInfo, TArray<DataType>& InStringArray, TArray<int>& SizesFixedArray, int IndexStart = 0, int IndexCount = -1);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, TArray<DataType>& Results, int IndexStart =0, int IndexCount =-1);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int IndexStart, int IndexCount);
	template<typename DataType> HOUDINIENGINE_API bool GetAttributeDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int IndexStart, int IndexCount) const;

	bool GetAttributeStrings(HAPI_AttributeOwner Owner, FHoudiniEngineIndexedStringMap& StringMap, int IndexStart = 0, int IndexCount = -1);
	bool GetAttributeStrings(const HAPI_AttributeInfo& AttributeInfo, FHoudiniEngineIndexedStringMap& StringMap, int IndexStart = 0, int IndexCount = -1);

	//  Functions to set data, mostly templated on type.
	//
	//template<typename DataType> bool SetAttributeData(HAPI_AttributeOwner Owner, const TArray<DataType>& Data);
	template<typename DataType> bool SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DataType>& Data);
	template<typename DataType> bool SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int First = 0, int Count = -1) const;
	template<typename DataType> bool SetAttributeDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int First = 0, int Count = -1) const;
	template<typename DataType> bool SetAttributeUniqueData(const HAPI_AttributeInfo& AttributeInfo, const DataType& Data);
	template<typename DataType> bool SetAttributeArrayData(const HAPI_AttributeInfo& InAttributeInfo, const TArray<DataType>& InStringArray, const TArray<int>& SizesFixedArray);
	bool SetAttributeStringMap(const HAPI_AttributeInfo& AttributeInfo, const FHoudiniEngineIndexedStringMap& InIndexedStringMap);
	bool SetAttributeDictionary(const HAPI_AttributeInfo& InAttributeInfo, const TArray<FString>& JSONData);

	TArray<FString> GetAttributeNames(HAPI_AttributeOwner Owner);

	bool GetHeightFieldData(TArray<float>& Values, int IndexCount);
	bool GetHeightFieldDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, float* Results, int IndexStart, int IndexCount) const;

protected:
	//
	// Internal functions.

	int CalculateNumberOfTasks(const HAPI_AttributeInfo& AttributeInfo) const;
	int CalculateNumberOfTasks(int64 SizeInBytes, int NumSessions) const;
	int CalculateNumberOfSessions(const HAPI_AttributeInfo& AttributeInfo) const;

	template<typename DataType> bool GetAttributeDataMultiSession(const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int First, int Count);
	template<typename DataType> bool SetAttributeDataMultiSession(const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int First, int Count) const;

	// Internal functions for actually getting data from HAPI.No type conversion is performed.

	template<typename DataType> HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Data, int IndexStart, int IndexCount) const;
	template<typename DataType> HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Data, int* Sizes, int IndexStart, int IndexCount) const;

	// Internal functions for sending data to HAPI. No type conversion is performed.
	template<typename DataType> HAPI_Result SendHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int StartIndex, int IndexCount) const;

	// Data conversion functions.
	template<typename DataType> static void ConvertFromRawData(const FHoudiniRawAttributeData& RawData, DataType* Data, size_t Count);
	template<typename DataType> static void ConvertToRawData(HAPI_StorageType StorageType, FHoudiniRawAttributeData& RawData, const DataType* Data, size_t Count);

	template<typename SrcType, typename DestType> static void Convert(const SrcType* SourceData, DestType* DestData, int Count);
	static FString ToString(int32 Number);
	static FString ToString(int64 Number);
	static FString ToString(float Number);
	static FString ToString(double Number);
	static double ToDouble(const FString& Str);
	static int ToInt(const FString& Str);

	// Raw functions fetch data before/after any type conversion.
	bool GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data);
	bool GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data, int Start, int Count) const;
	bool SetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data, int IndexStart, int IndexCount) const;

	template<typename DataType> static HAPI_StorageType GetHapiType();
	static  int64 GetHapiSize(HAPI_StorageType StorageType);

	static bool IsHapiArrayType(HAPI_StorageType);
	static HAPI_StorageType GetTypeWithoutArray(HAPI_StorageType StorageType);


	template<typename TaskType>
	static bool ExecuteTasksWithSessions(TArray<TaskType>& Tasks, int NumSessions);

	template<typename DataType>
	HAPI_Result SendHapiDataRunLengthEncoded(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int StartIndex, int IndexCount) const;

};



