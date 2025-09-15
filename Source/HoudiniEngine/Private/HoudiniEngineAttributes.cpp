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


#include "HoudiniEngineAttributes.h"
#include <type_traits>

#include "HoudiniEngine.h"
#include "HoudiniEngineTimers.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"

static TAutoConsoleVariable<float> CVarHoudiniEngineAccessorTimers(
	TEXT("HoudiniEngine.AccessorStats"),
	0.0,
	TEXT("When non-zero, the plugin will output stats about attributes. 1 == on, 2 == more detail.\n")
);

#define THRIFT_MAX_CHUNKSIZE			10 * 1024 * 1024

struct FHoudiniRawAttributeData
{
	// This structure is used to store data before it is converted to a different type.
	//
	// Only one of the following fields is valid at a time. Can't use a union due to ambiguous
	// destructors.

	TArray<uint8> RawDataUint8;
	TArray<int8> RawDataInt8;
	TArray<int16> RawDataInt16;
	TArray<int64> RawDataInt64;
	TArray<int> RawDataInt;
	TArray<float> RawDataFloat;
	TArray<double> RawDataDouble;
	TArray<FString> RawDataStrings;

};

FHoudiniHapiAccessor::FHoudiniHapiAccessor(HAPI_NodeId NodeId, HAPI_NodeId PartId, const char* Name)
{
	Init(NodeId, PartId, Name);
}

FHoudiniHapiAccessor::FHoudiniHapiAccessor(HAPI_NodeId NodeId, HAPI_NodeId PartId, const TCHAR* Name)
{
	Init(NodeId, PartId, H_TCHAR_TO_UTF8(Name));
}

void FHoudiniHapiAccessor::Init(HAPI_NodeId InNodeId, HAPI_NodeId InPartId, const char* InName)
{
	NodeId = InNodeId;
	PartId = InPartId;

	// Copy in the attribute name. Previously we just stored a copy of the pointer, but some
	// code used tempories which fell out of scope, so copy it to be safe.

	int Length = strlen(InName);
	AttributeName.SetNumZeroed(Length + 1);
	for (int Index = 0; Index < (Length + 1); Index++)
	{
		AttributeName[Index] = InName[Index];
	}
}

bool FHoudiniHapiAccessor::AddAttribute(HAPI_AttributeOwner InOwner, HAPI_StorageType InStorageType, int InTupleSize, int InCount, HAPI_AttributeInfo* OutAttrInfo)
{
	HAPI_AttributeInfo AttrInfo;
	FHoudiniApi::AttributeInfo_Init(&AttrInfo);
	AttrInfo.tupleSize = InTupleSize;
	AttrInfo.count = InCount;
	AttrInfo.exists = true;
	AttrInfo.owner = InOwner;
	AttrInfo.storage = InStorageType;
	AttrInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();
	auto bResult = FHoudiniApi::AddAttribute(Session, NodeId, PartId, AttributeName.GetData(), &AttrInfo);

	if (OutAttrInfo)
		*OutAttrInfo = AttrInfo;

	return (bResult == HAPI_RESULT_SUCCESS);
}


bool
FHoudiniHapiAccessor::GetInfo(HAPI_AttributeInfo& OutAttributeInfo, const HAPI_AttributeOwner InOwner)
{
	H_SCOPED_FUNCTION_TIMER()

	FHoudiniApi::AttributeInfo_Init(&OutAttributeInfo);

	const auto GetInfoLambda =
		[&](const HAPI_AttributeOwner Owner) -> bool
	{
		const HAPI_Result Result = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			PartId,
			AttributeName.GetData(),
			Owner,
			&OutAttributeInfo);

		if (Result != HAPI_RESULT_SUCCESS)
			OutAttributeInfo.exists = false;

		return Result == HAPI_RESULT_SUCCESS && OutAttributeInfo.exists;
	};

	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 OwnerIdx = 0; OwnerIdx < HAPI_ATTROWNER_MAX; ++OwnerIdx)
		{
			if (GetInfoLambda(static_cast<HAPI_AttributeOwner>(OwnerIdx)))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return GetInfoLambda(InOwner);
	}
}

FString FHoudiniHapiAccessor::ToString(int32 Number)
{
	return FString::Printf(TEXT("%d"), Number);
}

FString FHoudiniHapiAccessor::ToString(int64 Number)
{
	return FString::Printf(TEXT("%lld"), Number);
}

FString FHoudiniHapiAccessor::ToString(float Number)
{
	return FString::Printf(TEXT("%f"), Number);
}

FString FHoudiniHapiAccessor::ToString(double Number)
{
	return FString::Printf(TEXT("%f"), Number);
}

double FHoudiniHapiAccessor::ToDouble(const FString& Str)
{
	return FCString::Atof(*Str);
}

int FHoudiniHapiAccessor::ToInt(const FString& Str)
{
	return FCString::Atoi(*Str);
}

template<typename SrcType, typename DestType>
void FHoudiniHapiAccessor::Convert(const SrcType* SourceData, DestType* DestData, int IndexCount)
{
	for (int Index = 0; Index < IndexCount; Index++)
	{
		if constexpr (std::is_arithmetic_v<DestType>)
		{
			if constexpr (std::is_same_v<SrcType, FString>)
			{
				if constexpr  (std::is_same_v<DestType, float> || std::is_same_v<DestType, double>)
					DestData[Index] = static_cast<DestType>(ToDouble(SourceData[Index]));
				else
					DestData[Index] = static_cast<DestType>(ToInt(SourceData[Index]));
			}
			else
			{
				DestData[Index] = static_cast<DestType>(SourceData[Index]);
			}
		}
		else if constexpr (std::is_same_v<DestType,FString>)
		{
			if constexpr (std::is_same_v<SrcType, FString>)
			{
				DestData[Index] = SourceData[Index];
			}
			else
			{
				DestData[Index] = ToString(SourceData[Index]);
			}
		}
	}
}

template<typename DataType>
void FHoudiniHapiAccessor::ConvertFromRawData(const FHoudiniRawAttributeData & RawData, DataType* Data, size_t IndexCount)
{
	if (RawData.RawDataUint8.Num() > 0)
	{
		Convert(RawData.RawDataUint8.GetData(), Data, IndexCount);
	}
	else if (RawData.RawDataInt8.Num() > 0)
	{
		Convert(RawData.RawDataInt8.GetData(), Data, IndexCount);
	}
	else if(RawData.RawDataInt16.Num() > 0)
	{
		Convert(RawData.RawDataInt16.GetData(), Data, IndexCount);
	}
	else if (RawData.RawDataInt.Num() > 0)
	{
		Convert(RawData.RawDataInt.GetData(), Data, IndexCount);
	}
	else if (RawData.RawDataFloat.Num() > 0)
	{
		Convert(RawData.RawDataFloat.GetData(), Data, IndexCount);
	}
	else if (RawData.RawDataDouble.Num() > 0)
	{
		Convert(RawData.RawDataDouble.GetData(), Data, IndexCount);
	}
	else if (RawData.RawDataStrings.Num() > 0)
	{
		Convert(RawData.RawDataStrings.GetData(), Data, IndexCount);
	}
	else
	{
		// no data.
	}
}

template<typename DataType>
void FHoudiniHapiAccessor::ConvertToRawData(HAPI_StorageType StorageType, FHoudiniRawAttributeData& RawData, const DataType* Data, size_t IndexCount)
{
	switch(StorageType)
	{
	case HAPI_STORAGETYPE_UINT8:
		RawData.RawDataUint8.SetNum(IndexCount);
		Convert(Data, RawData.RawDataUint8.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_INT8:
		RawData.RawDataInt8.SetNum(IndexCount);
		Convert(Data, RawData.RawDataInt8.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_INT16:
		RawData.RawDataInt16.SetNum(IndexCount);
		Convert(Data, RawData.RawDataInt16.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_INT:
		RawData.RawDataInt.SetNum(IndexCount);
		Convert(Data, RawData.RawDataInt.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_INT64:
		RawData.RawDataInt64.SetNum(IndexCount);
		Convert(Data, RawData.RawDataInt64.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_FLOAT:
		RawData.RawDataFloat.SetNum(IndexCount);
		Convert(Data, RawData.RawDataFloat.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_FLOAT64:
		RawData.RawDataDouble.SetNum(IndexCount);
		Convert(Data, RawData.RawDataDouble.GetData(), IndexCount);
		break;

	case HAPI_STORAGETYPE_STRING:
		RawData.RawDataStrings.SetNum(IndexCount);
		Convert(Data, RawData.RawDataStrings.GetData(), IndexCount);
	default:
		break;
	}

}


struct FHoudiniAttributeTask
{
	int RawIndex;
	int Count;
	const FHoudiniHapiAccessor* Accessor;
	const HAPI_AttributeInfo * StorageInfo;
	const HAPI_Session* Session;
	bool bSuccess;

	static bool CanAbandon() { return false; }
	static void Abandon() {  }
	TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FHoudiniAttributeTask, STATGROUP_ThreadPoolAsyncTasks); }

};

int FHoudiniHapiAccessor::CalculateNumberOfSessions(const HAPI_AttributeInfo& AttributeInfo) const
{
	// GetStringBatchSize does not seem to function correctly with multisession. Arrays are slower with more than one session.
	if (AttributeInfo.storage == HAPI_STORAGETYPE_STRING || IsHapiArrayType(AttributeInfo.storage))
		return 1;

	int NumSessions = FHoudiniEngine::Get().GetNumSessions();

	if (!bAllowMultiThreading)
		NumSessions = 1;

	return NumSessions;
}

int FHoudiniHapiAccessor::CalculateNumberOfTasks(const HAPI_AttributeInfo& AttributeInfo) const
{
	int64 NumSessions = CalculateNumberOfSessions(AttributeInfo);
	int64 TotalSize = GetHapiSize(AttributeInfo.storage) * AttributeInfo.tupleSize * AttributeInfo.count;
	return CalculateNumberOfTasks(TotalSize, NumSessions);
}

int FHoudiniHapiAccessor::CalculateNumberOfTasks(int64 SizeInBytes, int NumSessions) const
{
	// By default assume one task per session.
	int64 NumTasks = NumSessions;

	// Check to see if each session has too much data.
	int64 SizePerSession = SizeInBytes / NumTasks;

	constexpr int ThriftChunkSize = THRIFT_MAX_CHUNKSIZE;

	int64 MaxSize = ThriftChunkSize;

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings->SessionType == EHoudiniRuntimeSettingsSessionType::HRSST_MemoryBuffer)
	{
		constexpr int64 OverheadSize = 1 * 1024 * 1024;
		MaxSize = HoudiniRuntimeSettings->SharedMemoryBufferSize * 1024 * 1024 - OverheadSize;
		if (MaxSize <= 0)
		{
			HOUDINI_LOG_ERROR(TEXT("Shared memory buffer size is too small."));
			return 0;
		}
	}

	if (SizePerSession > MaxSize)
	{
		NumTasks = (SizeInBytes + MaxSize - 1) / MaxSize;
	}
	return static_cast<int>(NumTasks);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int IndexStart, int IndexCount) const
{
	if (IndexCount == 0)
		return true;

	HAPI_StorageType StorageType = GetHapiType<DataType>();
	if (StorageType == GetTypeWithoutArray(AttributeInfo.storage))
	{
		HOUDINI_CHECK_ERROR_RETURN(FetchHapiData(Session, AttributeInfo, Results, IndexStart, IndexCount), false);
	}
	else
	{
		// Fetch data in Hapi format then convert it.
		FHoudiniRawAttributeData RawData;
		if (!GetRawAttributeData(Session, AttributeInfo, RawData, IndexStart, IndexCount))
			return false;

		ConvertFromRawData(RawData, Results, IndexCount * AttributeInfo.tupleSize);
	}
	return true;
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeArrayData(HAPI_AttributeOwner Owner, TArray<DataType>& Data, TArray<int>& Sizes, int IndexStart, int IndexCount)
{
	 const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();

	 HAPI_AttributeInfo AttrInfo;
	 if (!GetInfo(AttrInfo, Owner))
		 return false;

	return GetAttributeArrayData(AttrInfo, Data, Sizes, IndexStart, IndexCount);


}


template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeArrayData(const HAPI_AttributeInfo& AttributeInfo, TArray<DataType>& DataArray, TArray<int>& Sizes,int IndexStart, int IndexCount)
{
	if (IndexCount == -1)
		IndexCount = AttributeInfo.count;

	if (!IsHapiArrayType(AttributeInfo.storage))
	{
		HOUDINI_LOG_ERROR(TEXT("Not an array storage types"));
		return false;
	}
	DataArray.SetNum(AttributeInfo.totalArrayElements);
	Sizes.SetNum(IndexCount);

	HAPI_Result Result = FetchHapiDataArray(FHoudiniEngine::Get().GetSession(), AttributeInfo, DataArray.GetData(), Sizes.GetData(), IndexStart, IndexCount);
	return Result == HAPI_RESULT_SUCCESS;
}


template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, TArray<DataType>& Results, int IndexStart, int IndexCount)
{
	bool bDoTimings = CVarHoudiniEngineAccessorTimers.GetValueOnAnyThread() != 0.0;

	FHoudiniPerfTimer Timer(TEXT(""), bDoTimings);
	Timer.Start();

	if (!AttributeInfo.exists)
		return false;

	if (IndexCount == -1)
		IndexCount = AttributeInfo.count;

	int TotalCount;
	bool bSuccess = false;
	if (IsHapiArrayType(AttributeInfo.storage))
	{
		if(!bCanBeArray)
		{
			HOUDINI_LOG_ERROR(TEXT("Attribute was array, but this was not allowed: %hs"), this->AttributeName.GetData());
			return false;
		}

		if (IndexCount != 1)
		{
			// only fetch the first entry, or we'd end up with an array of arrays.
			HOUDINI_LOG_ERROR(TEXT("Attribute was array, but index count was not 1: %hs"), this->AttributeName.GetData());
			return false;
		}

		TArray<int> Sizes;
		Results.SetNum(AttributeInfo.totalArrayElements);
		bSuccess = GetAttributeArrayData(AttributeInfo, Results, Sizes, 0, 1);

	}
	else
	{
		TotalCount = IndexCount * AttributeInfo.tupleSize;
		Results.SetNum(TotalCount);

		bSuccess = GetAttributeData(AttributeInfo, Results.GetData(), IndexStart, IndexCount);

	}

	Timer.Stop();
	if((Timer.GetTime() > 0.0) && bDoTimings)
	{
		FString AttrText = this->AttributeName.GetData();
		double SizeInMb = (sizeof(DataType) * AttributeInfo.tupleSize * IndexCount) / 1000000.0;
		double MbPerSec = SizeInMb / Timer.GetTime();
		HOUDINI_LOG_MESSAGE(TEXT("Received %s, %.3f MB in %.3f seconds (%.3fMB/s)"), *AttrText, SizeInMb, Timer.GetTime(), MbPerSec);
	}

	return bSuccess;
}

template<typename DataType>
bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int IndexStart, int IndexCount)
{
	return GetAttributeDataMultiSession(AttributeInfo, Results, IndexStart, IndexCount);
}


template<typename TaskType>
bool FHoudiniHapiAccessor::ExecuteTasksWithSessions(TArray<TaskType> & Tasks, int NumSessions)
{
	// Create a pool of all available sessions.
	TArray<const HAPI_Session*> AvailableSessions;
	TSet< TaskType* > ActiveTasks;

	for (int Session = 0; Session < NumSessions; Session++)
	{
		const HAPI_Session* SessionToAdd = FHoudiniEngine::Get().GetSession(Session);
		AvailableSessions.Add(SessionToAdd);
	}

	// No sessions.
	if (AvailableSessions.IsEmpty())
		return false;

	for (int NextTaskToSTart = 0; NextTaskToSTart < Tasks.Num(); NextTaskToSTart++)
	{
		// Allocate sessions to tasks. We cannot assign a session to multiple tasks at the same
		// time or there will be conflicts. So grab a free session from the pool

		auto& Task = Tasks[NextTaskToSTart];

		if (AvailableSessions.IsEmpty())
		{
			// This should never happen.
			HOUDINI_LOG_ERROR(TEXT("Internal Error. No Available Houdini Sessions"));
			return false;
		}
		else
		{
			Task.GetTask().Session = AvailableSessions.Pop();
		}

		Task.StartBackgroundTask();
		ActiveTasks.Add(&Task);

		if (AvailableSessions.IsEmpty())
		{
			// We have to wait for a free session.
			bool bFreedSessions = false;
			while (!bFreedSessions)
			{
				for (auto It : ActiveTasks)
				{
					auto& ActiveTask = It;
					if (ActiveTask->IsDone())
					{
						ActiveTasks.Remove(ActiveTask);
						AvailableSessions.Add(ActiveTask->GetTask().Session);
						bFreedSessions = true;
						break;

					}
				}
			}
		}
	}

	// Wait for completion of all tasks
	bool bSuccess = true;
	for (int Task = 0; Task < Tasks.Num(); Task++)
	{
		Tasks[Task].EnsureCompletion();
		bSuccess &= Tasks[Task].GetTask().bSuccess;
	}
	return bSuccess;
}

struct FHoudiniHeightFieldGetTask : public  FHoudiniAttributeTask
{
	float* Results;

	void DoWork()
	{
		bSuccess = Accessor->GetHeightFieldDataViaSession(Session, *StorageInfo, Results, RawIndex, Count);
	}
};

bool FHoudiniHapiAccessor::GetHeightFieldDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, float* Results, int IndexStart, int IndexCount) const
{
	HAPI_Result Result = FHoudiniApi::GetHeightFieldData(Session, NodeId, PartId, Results, IndexStart, IndexCount);

	if(Result == HAPI_Result::HAPI_RESULT_SUCCESS)
		return true;

	// HAPI returned an error, handle it gracefully.
	HOUDINI_LOG_ERROR(TEXT("FHoudiniApi::GetHeightFieldData Failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(Result));

	return false;
}

bool FHoudiniHapiAccessor::GetHeightFieldData(TArray<float>& Results, int IndexCount)
{
	H_SCOPED_FUNCTION_TIMER();

	int64 TotalSize = Results.Num() * sizeof(Results[0]);
	int64 NumSessions = bAllowMultiThreading ? FHoudiniEngine::Get().GetNumSessions() : 1;
	int NumTasks = CalculateNumberOfTasks(TotalSize, NumSessions);
	// Task array.
	TArray<FAsyncTask<FHoudiniHeightFieldGetTask>> Tasks;
	Tasks.SetNum(NumTasks);

	Results.SetNumUninitialized(IndexCount);

	for(int64 TaskId = 0; TaskId < NumTasks; TaskId++)
	{
		// Fill a task, one per session.
		FHoudiniHeightFieldGetTask& Task = Tasks[TaskId].GetTask();

		int StartOffset = static_cast<int>(IndexCount * TaskId / NumTasks);
		int EndOffset = static_cast<int>(IndexCount * (TaskId + 1) / NumTasks);
		Task.Accessor = this;
		Task.RawIndex = StartOffset;
		Task.Results = Results.GetData() + StartOffset;
		Task.Count = EndOffset - StartOffset;
		Task.Session = nullptr;
	}

	bool bSuccess = ExecuteTasksWithSessions(Tasks, NumSessions);
	if (!bSuccess)
		Results.Empty();

	return bSuccess;
}

template<typename DataType>
struct FHoudiniAttributeGetTask : public  FHoudiniAttributeTask
{
	DataType* Results;

	void DoWork()
	{
		bSuccess = Accessor->GetAttributeDataViaSession(Session, *StorageInfo, Results, RawIndex, Count);
	}
};

template<typename DataType>
bool FHoudiniHapiAccessor::GetAttributeDataMultiSession(const HAPI_AttributeInfo& AttributeInfo, DataType * Results, int IndexStart, int IndexCount)
{
	// This is the actual main function for getting data.

	H_SCOPED_FUNCTION_DYNAMIC_LABEL(FString::Printf(TEXT("FHoudiniAttributeAccessor::GetAttributeDataMultiSession (%s)"), ANSI_TO_TCHAR(AttributeName.GetData())));

	if (!AttributeInfo.exists)
		return false;

	if (IndexCount == -1)
		IndexCount = AttributeInfo.count;

	int NumTasks = CalculateNumberOfTasks(AttributeInfo);
	int NumSessions = CalculateNumberOfSessions(AttributeInfo);
	// Task array.
	TArray<FAsyncTask<FHoudiniAttributeGetTask<DataType>>> Tasks;
	Tasks.SetNum(NumTasks);

	for(int TaskId = 0; TaskId < NumTasks; TaskId++)
	{
		// Fill a task, one per session.
		FHoudiniAttributeGetTask<DataType> & Task = Tasks[TaskId].GetTask();

		int StartOffset = IndexCount * TaskId / NumTasks;
		int EndOffset = IndexCount * (TaskId + 1) / NumTasks;
		Task.Accessor = this;
		Task.StorageInfo = &AttributeInfo;
		Task.RawIndex = StartOffset + IndexStart;
		Task.Results = Results + StartOffset * AttributeInfo.tupleSize;
		Task.Count = EndOffset - StartOffset;
		Task.Session = nullptr;
	}

	bool bSuccess = ExecuteTasksWithSessions(Tasks, NumSessions);

	return bSuccess;
}

template<typename DataType>
struct FHoudiniAttributeSetTask : public  FHoudiniAttributeTask
{
	const DataType* Input;

	void DoWork()
	{
		bSuccess = Accessor->SetAttributeDataViaSession(Session, *StorageInfo, Input, RawIndex, Count);
	}

};

template<typename DataType>
bool FHoudiniHapiAccessor::SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DataType>& Data)
{
	return SetAttributeData(AttributeInfo, Data.GetData());
}

template<typename DataType>
bool FHoudiniHapiAccessor::SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int IndexStart, int IndexCount) const
{
	return SetAttributeDataMultiSession(AttributeInfo, Data, IndexStart, IndexCount);
}

template<typename DataType>
bool FHoudiniHapiAccessor::SetAttributeDataMultiSession(const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int IndexStart, int IndexCount) const
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(FString::Printf(TEXT("FHoudiniAttributeAccessor::SetAttributeDataMultiSession (%s)"), ANSI_TO_TCHAR(AttributeName.GetData())));

	bool bDoTiming = CVarHoudiniEngineAccessorTimers.GetValueOnAnyThread() != 0.0;
	FHoudiniPerfTimer Timer(TEXT(""), bDoTiming);
	Timer.Start();

	if (IndexCount == -1)
		IndexCount = AttributeInfo.count;

	int NumTasks = CalculateNumberOfTasks(AttributeInfo);
	int NumSessions = CalculateNumberOfSessions(AttributeInfo);

	// Task array.
	TArray<FAsyncTask<FHoudiniAttributeSetTask<DataType>>> Tasks;
	Tasks.SetNum(NumTasks);

	for (int TaskId = 0; TaskId < NumTasks; TaskId++)
	{
		// Fill a task, one per session.
		FHoudiniAttributeSetTask<DataType>& Task = Tasks[TaskId].GetTask();

		int StartOffset = IndexCount * TaskId / NumTasks;
		int EndOffset = IndexCount * (TaskId + 1) / NumTasks;
		Task.Accessor = this;
		Task.StorageInfo = &AttributeInfo;
		Task.RawIndex = StartOffset + IndexStart;
		Task.Input = Data + StartOffset * AttributeInfo.tupleSize;
		Task.Count = EndOffset - StartOffset;
		Task.Session = nullptr;
	}

	bool bSuccess = ExecuteTasksWithSessions(Tasks, NumSessions);

	Timer.Stop();
	if((Timer.GetTime() > 0.0) && bDoTiming)
	{
		FString AttrText = this->AttributeName.GetData();
		double SizeInMb = (sizeof(DataType) * AttributeInfo.tupleSize * IndexCount) / 1000000.0;
		double MbPerSec = SizeInMb / Timer.GetTime();
		HOUDINI_LOG_MESSAGE(TEXT("Sent %s, %.3f MB in %.3f seconds (%.3fMB/s)"), *AttrText, SizeInMb, Timer.GetTime(), MbPerSec);
	}


	return bSuccess;
}


template<typename DataType>
HAPI_Result FHoudiniHapiAccessor::SendHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int StartIndex, int IndexCount) const
{
	TArray<int> RunLengths;

	if (bCanRunLengthEncode && (AttributeInfo.tupleSize * IndexCount >= MinElementsForRunLengthEncoding))
		RunLengths = FHoudiniEngineUtils::RunLengthEncode(Data, AttributeInfo.tupleSize, IndexCount);

	HAPI_Result Result = HAPI_RESULT_FAILURE;

	bool bDoTimings = CVarHoudiniEngineAccessorTimers.GetValueOnAnyThread() == 2.0;

	FString TimerName = UTF8_TO_TCHAR(this->AttributeName.GetData());
	FHoudiniPerfTimer Timer(FString::Printf(TEXT("Transmission Time %s"), *TimerName), bDoTimings);

	Timer.Start();

	if (RunLengths.Num() > 0)
	{
		// Send run length encoded data.

		for (int Index = 0; Index < RunLengths.Num(); Index++)
		{
			int StartRLEIndex = RunLengths[Index];
			int EndRLEIndex = IndexCount;

			int RLEStart = StartIndex + StartRLEIndex;
			int RLECount = EndRLEIndex - StartRLEIndex;

			if (Index != RunLengths.Num() - 1)
				EndRLEIndex = RunLengths[Index + 1];

			const DataType* TupleValues = &Data[StartRLEIndex * AttributeInfo.tupleSize];

			if constexpr (std::is_same_v<DataType, float>)
			{
				Result = FHoudiniApi::SetAttributeFloatUniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, TupleValues, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, double>)
			{
				Result = FHoudiniApi::SetAttributeFloat64UniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, TupleValues, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, uint8>)
			{
				Result = FHoudiniApi::SetAttributeUInt8UniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, TupleValues, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, int8>)
			{
				Result = FHoudiniApi::SetAttributeInt8UniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, TupleValues, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, int16>)
			{
				Result = FHoudiniApi::SetAttributeInt16UniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, TupleValues, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, int>)
			{
				Result = FHoudiniApi::SetAttributeIntUniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, TupleValues, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, int64>)
			{
				const HAPI_Int64* Hapi64Data = reinterpret_cast<const HAPI_Int64*>(TupleValues); // worked around for some Linux variations.
				Result = FHoudiniApi::SetAttributeInt64UniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Hapi64Data, AttributeInfo.tupleSize, RLEStart, RLECount);
			}
			else if constexpr (std::is_same_v<DataType, FString>)
			{
				TArray<const char*> StringDataArray;
				for (int StringIndex = 0; StringIndex < AttributeInfo.tupleSize; StringIndex++)
					StringDataArray.Add(FHoudiniEngineUtils::ExtractRawString(TupleValues[Index]));

				Result = FHoudiniApi::SetAttributeStringUniqueData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, StringDataArray[0], AttributeInfo.tupleSize, RLEStart, RLECount);

				// ExtractRawString allocates memory using malloc, free it!
				FHoudiniEngineUtils::FreeRawStringMemory(StringDataArray);

			}

			if (Result != HAPI_RESULT_SUCCESS)
				return Result;
		}
		Result = HAPI_RESULT_SUCCESS;
	}
	else
	{
		if constexpr (std::is_same_v<DataType, float>)
		{
			Result = FHoudiniApi::SetAttributeFloatData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Data, StartIndex, IndexCount);;
		}
		else if constexpr (std::is_same_v<DataType, double>)
		{
			Result = FHoudiniApi::SetAttributeFloat64Data(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Data, StartIndex, IndexCount);;
		}
		else if constexpr (std::is_same_v<DataType, uint8>)
		{
			Result = FHoudiniApi::SetAttributeUInt8Data(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Data, StartIndex, IndexCount);
		}
		else if constexpr (std::is_same_v<DataType, int8>)
		{
			Result =  FHoudiniApi::SetAttributeInt8Data(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Data, StartIndex, IndexCount);
		}
		else if constexpr (std::is_same_v<DataType, int16>)
		{
			Result = FHoudiniApi::SetAttributeInt16Data(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Data, StartIndex, IndexCount);
		}
		else if constexpr (std::is_same_v<DataType, int>)
		{
			Result = FHoudiniApi::SetAttributeIntData(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Data, StartIndex, IndexCount);
		}
		else if constexpr (std::is_same_v<DataType, int64>)
		{
			const HAPI_Int64* Hapi64Data = reinterpret_cast<const HAPI_Int64*>(Data); // worked around for some Linux variations.
			Result = FHoudiniApi::SetAttributeInt64Data(Session, NodeId, PartId, AttributeName.GetData(), &AttributeInfo, Hapi64Data, StartIndex, IndexCount);
		}
		else if constexpr (std::is_same_v<DataType, FString>)
		{
			TArray<const char*> StringDataArray;
			for (int Index = 0; Index < IndexCount; Index++)
			{
				auto& CurrentString = Data[Index];

				// Append the converted string to the string array
				StringDataArray.Add(FHoudiniEngineUtils::ExtractRawString(CurrentString));
			}

			// Set all the attribute values once
			Result = FHoudiniApi::SetAttributeStringData(
				Session,
				NodeId, PartId, AttributeName.GetData(),
				&AttributeInfo, StringDataArray.GetData(), StartIndex, IndexCount);

			// ExtractRawString allocates memory using malloc, free it!
			FHoudiniEngineUtils::FreeRawStringMemory(StringDataArray);
		}
	}

	Timer.Stop();

	return Result;
}

template<typename DataType>
bool FHoudiniHapiAccessor::SetAttributeDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int StartIndex, int IndexCount) const
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(FString::Printf(TEXT("FHoudiniAttributeAccessor::SetAttributeDataMultiSession (%s)"), ANSI_TO_TCHAR(AttributeName.GetData())));

	if (IndexCount == 0)
		return true;

	HAPI_StorageType StorageType = GetHapiType<DataType>();
	if (StorageType == AttributeInfo.storage)
	{
		// No conversion is necessary so send the data directly.
		SendHapiData(Session, AttributeInfo, Data, StartIndex, IndexCount);
	}
	else
	{
		// Fetch data in Hapi format then convert it.
		FHoudiniRawAttributeData RawData;

		ConvertToRawData(AttributeInfo.storage, RawData, Data, IndexCount * AttributeInfo.tupleSize);
		if (!SetRawAttributeData(Session, AttributeInfo, RawData, StartIndex, IndexCount))
			return false;


	}
	return true;
}

template<typename DataType>
bool SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DataType>& Data)
{
	return SetAttributeData(AttributeInfo, Data.GetData(), 0, Data.Num());
}

bool FHoudiniHapiAccessor::GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data)
{
	return GetRawAttributeData(Session, AttributeInfo, Data, 0, AttributeInfo.count);
}

template<typename DataType>
HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;

	HAPI_Result Result = HAPI_RESULT_FAILURE;

	if constexpr (std::is_same_v<DataType, float>)
	{
		Result = FHoudiniApi::GetAttributeFloatData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, double>)
	{
		Result = FHoudiniApi::GetAttributeFloat64Data(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, uint8>)
	{
		Result = FHoudiniApi::GetAttributeUInt8Data(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int8>)
	{
		Result = FHoudiniApi::GetAttributeInt8Data(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int16>)
	{
		Result = FHoudiniApi::GetAttributeInt16Data(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int>)
	{
		Result = FHoudiniApi::GetAttributeIntData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int64>)
	{
		HAPI_Int64* Hapi64Data = reinterpret_cast<HAPI_Int64*>(Data); // worked around for some Linux variations.
		Result = FHoudiniApi::GetAttributeInt64Data(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, -1, Hapi64Data, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, FString>)
	{
		TArray<HAPI_StringHandle> StringHandles;
		StringHandles.SetNum(IndexCount * TempAttributeInfo.tupleSize);

		Result = FHoudiniApi::GetAttributeStringData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, StringHandles.GetData(), IndexStart, IndexCount);

		if (Result == HAPI_RESULT_SUCCESS)
			FHoudiniEngineString::SHArrayToFStringArray(StringHandles, Data, Session);
	}

	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}


template<typename DataType>
HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	if constexpr (std::is_same_v<DataType, float>)
	{
		Result = FHoudiniApi::GetAttributeFloatArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, double>)
	{
		Result = FHoudiniApi::GetAttributeFloat64ArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, uint8>)
	{
		Result = FHoudiniApi::GetAttributeUInt8ArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int8>)
	{
		Result = FHoudiniApi::GetAttributeInt8ArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int16>)
	{
		Result = FHoudiniApi::GetAttributeInt16ArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int>)
	{
		Result = FHoudiniApi::GetAttributeIntArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, int64>)
	{
		HAPI_Int64* Hapi64Data = reinterpret_cast<HAPI_Int64*>(Data); // worked around for some Linux variations.
		Result = FHoudiniApi::GetAttributeInt64ArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, Hapi64Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	}
	else if constexpr (std::is_same_v<DataType, FString>)
	{
		TArray<HAPI_StringHandle> StringHandles;
		StringHandles.SetNum(TempAttributeInfo.totalArrayElements);

		Result = FHoudiniApi::GetAttributeStringArrayData(Session, NodeId, PartId, AttributeName.GetData(), &TempAttributeInfo, StringHandles.GetData(), AttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);

		if (Result == HAPI_RESULT_SUCCESS)
			FHoudiniEngineString::SHArrayToFStringArray(StringHandles, Data, Session);
	}



	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

bool FHoudiniHapiAccessor::GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data, int IndexStart, int IndexCount) const
{
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	int TotalCount = AttributeInfo.count * AttributeInfo.tupleSize;

	// Make a copy of the Attribute Info because it can be modified during fetching of data.
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;

	int NumElement = IndexCount * AttributeInfo.tupleSize;
	int NumArrayElements = AttributeInfo.totalArrayElements;

	TArray<int> Sizes;

	switch(AttributeInfo.storage)
	{
	case HAPI_STORAGETYPE_UINT8:
		Data.RawDataUint8.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataUint8.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT8:		
		Data.RawDataInt8.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt8.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT16:
		Data.RawDataInt16.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt16.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT64:
		Data.RawDataInt64.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt64.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT:
		Data.RawDataInt.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT:
		Data.RawDataFloat.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataFloat.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT64:
		Data.RawDataDouble.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataDouble.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_STRING:
		Data.RawDataStrings.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataStrings.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_UINT8_ARRAY:
		Data.RawDataUint8.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataUint8.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT8_ARRAY:
		Data.RawDataInt8.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt8.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT16_ARRAY:
		Data.RawDataInt16.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt16.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT64_ARRAY:
		Data.RawDataInt64.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt64.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT_ARRAY:
		Data.RawDataInt.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT_ARRAY:
		Data.RawDataFloat.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataFloat.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT64_ARRAY:
		Data.RawDataDouble.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataDouble.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_STRING_ARRAY:
		Data.RawDataStrings.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataStrings.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;

	default:
		return false;
	}
	
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;

	return Result == HAPI_RESULT_SUCCESS;
}

bool FHoudiniHapiAccessor::SetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data, int IndexStart, int IndexCount) const
{
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	int TotalCount = AttributeInfo.count * AttributeInfo.tupleSize;

	// Make a copy of the Attribute Info because it can be modified during fetching of data.
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;

	int NumElement = IndexCount * AttributeInfo.tupleSize;

	switch (AttributeInfo.storage)
	{
	case HAPI_STORAGETYPE_UINT8:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataUint8.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT8:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataInt8.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT16:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataInt16.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT64:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataInt64.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataInt.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataFloat.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT64:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataDouble.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_STRING:
		Result = SendHapiData(Session, AttributeInfo, Data.RawDataStrings.GetData(), IndexStart, IndexCount);
		break;
	default:
		return false;
	}

	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;

	return Result == HAPI_RESULT_SUCCESS;
}


template<typename DataType>
HAPI_StorageType FHoudiniHapiAccessor::GetHapiType()
{
	if constexpr (std::is_same_v<DataType, float>)
	{
		return HAPI_STORAGETYPE_FLOAT;
	}
	else if constexpr (std::is_same_v<DataType, double>)
	{
		return HAPI_STORAGETYPE_FLOAT64;
	}
	else if constexpr (std::is_same_v<DataType, int>)
	{
		return HAPI_STORAGETYPE_INT;
	}
	else if constexpr (std::is_same_v<DataType, int8>)
	{
		return HAPI_STORAGETYPE_INT8;
	}
	else if constexpr (std::is_same_v<DataType, int16>)
	{
		return HAPI_STORAGETYPE_INT16;
	}
	else if constexpr (std::is_same_v<DataType, uint8>)
	{
		return HAPI_STORAGETYPE_UINT8;
	}
	else if constexpr (std::is_same_v<DataType, int64>)
	{
		return HAPI_STORAGETYPE_INT64;
	}
	else if constexpr (std::is_same_v<DataType, FString>)
	{
		return HAPI_STORAGETYPE_STRING;
	}
	else
	{
		return HAPI_STORAGETYPE_INVALID;
	}
}

template<> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, TArray<FVector3f>& Results, int IndexStart, int IndexCount)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	if (AttrInfo.tupleSize != 3)
	{
		HOUDINI_LOG_ERROR(TEXT("Tried to get a Vector3f, but tuple size is not 3"));
		return false;
	}
	Results.SetNum(AttrInfo.count);

	return GetAttributeData(AttrInfo, (float *)Results.GetData(), IndexStart, IndexCount);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, TArray<DataType>& Results, int IndexStart, int IndexCount)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	return GetAttributeData(AttrInfo, Results, IndexStart, IndexCount);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, DataType* Results, int IndexStart, int IndexCount)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;
	return GetAttributeData(AttrInfo, Results, IndexStart, IndexCount);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeFirstValue(HAPI_AttributeOwner Owner, DataType& Result)
{
	return GetAttributeData(Owner, &Result, 0, 1);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, TArray<DataType>& Results, int IndexStart, int IndexCount)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	AttrInfo.tupleSize = MaxTuples;

	return GetAttributeData(AttrInfo, Results, IndexStart, IndexCount);

}
template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, DataType* Results, int IndexStart, int IndexCount)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	AttrInfo.tupleSize = MaxTuples;

	return GetAttributeData(AttrInfo, Results, IndexStart, IndexCount);
}

bool FHoudiniHapiAccessor::SetAttributeStringMap(const HAPI_AttributeInfo& AttributeInfo, const FHoudiniEngineIndexedStringMap& InIndexedStringMap)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(FString::Printf(TEXT("FHoudiniAttributeAccessor::SetAttributeStringMap (%s)"), ANSI_TO_TCHAR(AttributeName.GetData())));

	FHoudiniEngineRawStrings IndexedRawStrings = InIndexedStringMap.GetRawStrings();
	TArray<int> IndexArray = InIndexedStringMap.GetIds();

	HAPI_Result Result = FHoudiniApi::SetAttributeIndexedStringData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, PartId, AttributeName.GetData(),
		&AttributeInfo, IndexedRawStrings.RawStrings.GetData(), IndexedRawStrings.RawStrings.Num(), IndexArray.GetData(), 0, IndexArray.Num());

	return Result == HAPI_RESULT_SUCCESS;
}

template<typename DataType> bool FHoudiniHapiAccessor::SetAttributeUniqueData(const HAPI_AttributeInfo& AttributeInfo, const DataType& Data)
{
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	FHoudiniRawAttributeData RawData;
	ConvertToRawData(AttributeInfo.storage, RawData, &Data, 1);

	switch(AttributeInfo.storage)
	{
	case HAPI_STORAGETYPE_FLOAT:
		Result = FHoudiniApi::SetAttributeFloatUniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			RawData.RawDataFloat.GetData(), AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;

	case HAPI_STORAGETYPE_FLOAT64:
		Result = FHoudiniApi::SetAttributeFloat64UniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			RawData.RawDataDouble.GetData(), AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;

	case HAPI_STORAGETYPE_INT8:
		Result = FHoudiniApi::SetAttributeInt8UniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			RawData.RawDataInt8.GetData(), AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;

	case HAPI_STORAGETYPE_UINT8:
		Result = FHoudiniApi::SetAttributeUInt8UniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			RawData.RawDataUint8.GetData(), AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;

	case HAPI_STORAGETYPE_INT16:
		Result = FHoudiniApi::SetAttributeInt16UniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			RawData.RawDataInt16.GetData(), AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;

	case HAPI_STORAGETYPE_INT:
		Result = FHoudiniApi::SetAttributeIntUniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			RawData.RawDataInt.GetData(), AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;

	case HAPI_STORAGETYPE_INT64:
	{
		HAPI_Int64* Hapi64Data = reinterpret_cast<HAPI_Int64*>(RawData.RawDataInt64.GetData()); // worked around for some Linux variations.
		Result = FHoudiniApi::SetAttributeInt64UniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			Hapi64Data, AttributeInfo.tupleSize, 0, AttributeInfo.count);
		break;
	}
	case HAPI_STORAGETYPE_STRING:
		Result = FHoudiniApi::SetAttributeStringUniqueData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			TCHAR_TO_ANSI(*RawData.RawDataStrings[0]), 1, 0, AttributeInfo.count);
		break;
	default:
		Result = HAPI_RESULT_FAILURE;
	}
	return Result == HAPI_RESULT_SUCCESS;

}

bool FHoudiniHapiAccessor::SetAttributeDictionary(const HAPI_AttributeInfo& InAttributeInfo, const TArray<FString>& JSONData)
{
	TArray<const char*> RawStringData;
	for (const FString& Data : JSONData)
	{
		RawStringData.Add(FHoudiniEngineUtils::ExtractRawString(Data));
	}

	// Send strings in smaller chunks due to their potential size
	int32 ChunkSize = (THRIFT_MAX_CHUNKSIZE / 100) / InAttributeInfo.tupleSize;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Set the attributes in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			const int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeDictionaryData(
				FHoudiniEngine::Get().GetSession(),
				NodeId, PartId, AttributeName.GetData(),
				&InAttributeInfo, RawStringData.GetData() + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Set all the attribute values once
		Result = FHoudiniApi::SetAttributeDictionaryData(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, AttributeName.GetData(),
			&InAttributeInfo, RawStringData.GetData(),
			0, RawStringData.Num());
	}

	// ExtractRawString allocates memory using malloc, free it!
	FHoudiniEngineUtils::FreeRawStringMemory(RawStringData);

	return Result == HAPI_RESULT_SUCCESS;
}

template<typename DataType>
bool FHoudiniHapiAccessor::SetAttributeArrayData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DataType>& DataArray, const TArray<int>& SizesFixedArray)
{
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	if constexpr (std::is_same_v<DataType, float>)
	{
		Result = FHoudiniApi::SetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			DataArray.GetData(), DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());
	}
	else if constexpr (std::is_same_v<DataType, double>)
	{
		Result = FHoudiniApi::SetAttributeFloat64ArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			DataArray.GetData(), DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());

	}
	else if constexpr (std::is_same_v<DataType, int>)
	{
		Result = FHoudiniApi::SetAttributeIntArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			DataArray.GetData(), DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());
	}
	else if constexpr (std::is_same_v<DataType, int8>)
	{
		Result = FHoudiniApi::SetAttributeInt8ArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			DataArray.GetData(), DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());
	}
	else if constexpr (std::is_same_v<DataType, int16>)
	{
		Result = FHoudiniApi::SetAttributeInt16ArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			DataArray.GetData(), DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());
	}
	else if constexpr (std::is_same_v<DataType, uint8>)
	{
		Result = FHoudiniApi::SetAttributeUInt8ArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			DataArray.GetData(), DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());
	}
	else if constexpr (std::is_same_v<DataType, int64>)
	{
		const HAPI_Int64 * Hapi64Data = reinterpret_cast<const HAPI_Int64*>(DataArray.GetData()); // worked around for some Linux variations.
		Result = FHoudiniApi::SetAttributeInt64ArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId, AttributeName.GetData(), &AttributeInfo,
			Hapi64Data, DataArray.Num(), SizesFixedArray.GetData(), 0, SizesFixedArray.Num());
	}
	else if constexpr (std::is_same_v<DataType, FString>)
	{
		TArray<const char*> StringDataArray;
		for (const auto& CurrentString : DataArray)
		{
			// Append the converted string to the string array
			StringDataArray.Add(FHoudiniEngineUtils::ExtractRawString(CurrentString));
		}

		// Set all the attribute values once
		Result = FHoudiniApi::SetAttributeStringArrayData(
				FHoudiniEngine::Get().GetSession(),
				NodeId, PartId, AttributeName.GetData(),
				&AttributeInfo, StringDataArray.GetData(), StringDataArray.Num(),
				SizesFixedArray.GetData(), 0, SizesFixedArray.Num());

		// ExtractRawString allocates memory using malloc, free it!
		FHoudiniEngineUtils::FreeRawStringMemory(StringDataArray);
	}
	else
	{
		return HAPI_STORAGETYPE_INVALID;
	}
	return Result == HAPI_RESULT_SUCCESS;
}


bool FHoudiniHapiAccessor::IsHapiArrayType(HAPI_StorageType StorageType)
{
	switch(StorageType)
	{
		case HAPI_STORAGETYPE_UINT8_ARRAY:
		case HAPI_STORAGETYPE_INT8_ARRAY:
		case HAPI_STORAGETYPE_INT16_ARRAY:
		case HAPI_STORAGETYPE_INT64_ARRAY:
		case HAPI_STORAGETYPE_INT_ARRAY:
		case HAPI_STORAGETYPE_FLOAT_ARRAY:
		case HAPI_STORAGETYPE_FLOAT64_ARRAY:
		case HAPI_STORAGETYPE_STRING_ARRAY:
			return true;
	default:
			return false;
	}
}

int64 FHoudiniHapiAccessor::GetHapiSize(HAPI_StorageType StorageType)
{
	switch (StorageType)
	{
	case HAPI_STORAGETYPE_UINT8:
		return sizeof(uint8);
	case HAPI_STORAGETYPE_INT8:
		return sizeof(int8);
	case HAPI_STORAGETYPE_INT16:
		return sizeof(int16);
	case HAPI_STORAGETYPE_INT64:
		return sizeof(int64);
	case HAPI_STORAGETYPE_INT:
		return sizeof(int32);
	case HAPI_STORAGETYPE_FLOAT:
		return sizeof(float);
	case HAPI_STORAGETYPE_FLOAT64:
		return sizeof(int64);
	case HAPI_STORAGETYPE_STRING:
		return sizeof(int); // Use int for string handles.
	default:
		return 1;
	}
}

HAPI_StorageType FHoudiniHapiAccessor::GetTypeWithoutArray(HAPI_StorageType StorageType)
{
	if (StorageType >= HAPI_STORAGETYPE_INT_ARRAY && StorageType <= HAPI_STORAGETYPE_DICTIONARY_ARRAY)
		return static_cast<HAPI_StorageType>(StorageType - HAPI_STORAGETYPE_INT_ARRAY);
	else
		return StorageType;
}

bool FHoudiniHapiAccessor::GetAttributeStrings(HAPI_AttributeOwner Owner, FHoudiniEngineIndexedStringMap& StringArray, int IndexStart, int IndexCount)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	return GetAttributeStrings(AttrInfo, StringArray, IndexStart, IndexCount);
}



bool FHoudiniHapiAccessor::GetAttributeStrings(const HAPI_AttributeInfo& InAttrInfo, FHoudiniEngineIndexedStringMap& StringArray, int IndexStart, int IndexCount)
{
	StringArray = {};

	int Count = IndexCount == -1 ? InAttrInfo.count : IndexCount;

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(Count);
	HAPI_AttributeInfo AttrInfo = InAttrInfo;

	if (AttrInfo.storage == HAPI_STORAGETYPE_STRING)
	{
		auto Result = FHoudiniApi::GetAttributeStringData(Session, NodeId, PartId, AttributeName.GetData(), &AttrInfo, StringHandles.GetData(), IndexStart, Count);

		if (Result != HAPI_RESULT_SUCCESS)
			return false;

		StringArray.InitializeFromStringHandles(StringHandles);
	}
	else
	{
		TArray<FString> Strings;
		Strings.SetNum(Count);
		if (!GetAttributeDataViaSession(Session, InAttrInfo, Strings.GetData(), IndexStart, Count))
			return false;

		TMap<FString, int> HandleIndices;
		StringArray.Ids.SetNum(Strings.Num());
		StringArray.Strings.Empty();

		for(int Index = 0; Index < Strings.Num(); Index++)
		{
			int * StringIndex = HandleIndices.Find(Strings[Index]);
			if (StringIndex)
			{
				StringArray.Ids[Index] = *StringIndex;
			}
			else
			{
				StringArray.Ids[Index] = StringArray.Strings.Num();
				HandleIndices.Add(Strings[Index]) = StringArray.Strings.Num();
				StringArray.Strings.Add(Strings[Index]);
			}
		}

	}
	return true;
}

TArray<FString> FHoudiniHapiAccessor::GetAttributeNames(HAPI_AttributeOwner Owner)
{
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo);


	int AttrCount = PartInfo.attributeCounts[Owner];
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(AttrCount);

	FHoudiniApi::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId, Owner, StringHandles.GetData(), AttrCount);

	TArray<FString> Results;
	Results.SetNum(StringHandles.Num());
	for(int Index = 0; Index < Results.Num(); Index++)
	{
		FHoudiniEngineString::ToFString(StringHandles[Index], Results[Index], FHoudiniEngine::Get().GetSession());
	}
	return Results;
}


#define IMPLEMENT_HOUDINI_ACCESSOR(DATA_TYPE)\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeFirstValue(HAPI_AttributeOwner Owner, DATA_TYPE& Result);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, TArray<DATA_TYPE>& Results, int IndexStart, int IndexCount);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, DATA_TYPE * Results, int IndexStart, int IndexCount);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int TupleSize, TArray<DATA_TYPE>& Results, int IndexStart, int IndexCount);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int TupleSize, DATA_TYPE * Results, int IndexStart, int IndexCount);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, TArray<DATA_TYPE>& Results, int IndexStart , int IndexCount);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const DATA_TYPE* Data, int IndexStart, int IndexCount) const;\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::SetAttributeDataViaSession(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DATA_TYPE* Data, int IndexStart, int IndexCount) const;\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DATA_TYPE>& Data);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::SetAttributeUniqueData(const HAPI_AttributeInfo& AttributeInfo, const DATA_TYPE& Data);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::SetAttributeArrayData(const HAPI_AttributeInfo& InAttributeInfo, const TArray<DATA_TYPE>& InStringArray, const TArray<int>& SizesFixedArray);\
	template HOUDINIENGINE_API bool FHoudiniHapiAccessor::GetAttributeArrayData(HAPI_AttributeOwner Owner, TArray<DATA_TYPE>& StringArray, TArray<int>& SizesFixedArray, int IndexStart, int IndexCount);

IMPLEMENT_HOUDINI_ACCESSOR(uint8);
IMPLEMENT_HOUDINI_ACCESSOR(int8);
IMPLEMENT_HOUDINI_ACCESSOR(int16);
IMPLEMENT_HOUDINI_ACCESSOR(int64);
IMPLEMENT_HOUDINI_ACCESSOR(int);
IMPLEMENT_HOUDINI_ACCESSOR(float);
IMPLEMENT_HOUDINI_ACCESSOR(double);
IMPLEMENT_HOUDINI_ACCESSOR(FString);
