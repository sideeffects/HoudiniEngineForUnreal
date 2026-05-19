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

#include "HoudiniEngineStatusManager.h"
#include "HoudiniEngine.h"
#include "HoudiniPDGAssetLink.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HoudiniCookable.h"

void FHoudiniEngineStatusManager::OnSessionLost()
{
	this->CurrentStatuses = {};
}

void FHoudiniEngineStatusManager::Initialize()
{
	Instance = new FHoudiniEngineStatusManager();
}

void FHoudiniEngineStatusManager::ClearStatus(const UHoudiniCookable* Cookable) 
{
	FHoudiniCookableStatus* CookableStatus = CurrentStatuses.Find(Cookable);
	if(CookableStatus)
		*CookableStatus = {};
}

void FHoudiniEngineStatusManager::StartInstantiating(UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus = {};
	CookableStatus.Status = EHoudiniStatusManagerStatus::Instantiating;
	CookableStatus.StartTime = FPlatformTime::Seconds();
}

void FHoudiniEngineStatusManager::EndInstantiating(UHoudiniCookable* Cookable, bool bSuccess)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus.Status = EHoudiniStatusManagerStatus::Instantiated;
	if(!bSuccess)
	{
		if(CookableStatus.NumErrors == 0)
		{
			// Add a generic message if we failed but no other errors were logged.
			AddLog(TEXT("Instantiating Failed."), ELogVerbosity::Type::Error);
		}
	}
}

void FHoudiniEngineStatusManager::StartCooking(UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus = {};
	CookableStatus.Status = EHoudiniStatusManagerStatus::Cooking;
	CookableStatus.StartTime = FPlatformTime::Seconds();
}

void FHoudiniEngineStatusManager::EndCooking(UHoudiniCookable* Cookable, bool bSuccess)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus.Status = EHoudiniStatusManagerStatus::CookComplete;
	if (!bSuccess)
	{
		if(CookableStatus.NumErrors == 0)
		{
			// Add a generic message if we failed but no other errors were logged.
			AddLog(TEXT("Cooking Failed."), ELogVerbosity::Type::Error);
		}
	}
}

void FHoudiniEngineStatusManager::StartBaking(UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus = {};
	CookableStatus.Status = EHoudiniStatusManagerStatus::Baking;
	CookableStatus.StartTime = FPlatformTime::Seconds();
}

void FHoudiniEngineStatusManager::EndBaking(UHoudiniCookable* Cookable, bool bSuccess)
{
	FScopeLock Lock(&Mutex);

	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus.Status = EHoudiniStatusManagerStatus::BakingComplete;
	if(!bSuccess)
	{
		if(CookableStatus.NumErrors == 0)
		{
			// Add a generic message if we failed but no other errors were logged.
			AddLog(TEXT("Baking Failed."), ELogVerbosity::Type::Error);
		}
	}
}


void FHoudiniEngineStatusManager::StartPDG(UHoudiniCookable* Cookable)
{
	// PDG Needs some fixes before this will work. Bug 150505
#if 0
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus = {};
	CookableStatus.Status = EHoudiniStatusManagerStatus::PDGExecuting;
	CookableStatus.StartTime = FPlatformTime::Seconds();
#endif
}

void FHoudiniEngineStatusManager::EndPDG(UHoudiniCookable* Cookable, bool bSuccess)
{
	// PDG Needs some fixes before this will work. Bug 150505
#if 0
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus.Status = EHoudiniStatusManagerStatus::PDGComplete;
	if(!bSuccess)
	{

		if(CookableStatus.NumErrors == 0)
		{
			// Add a generic message if we failed but no other errors were logged.
			AddLog(TEXT("PDG Failed."), ELogVerbosity::Type::Error);
		}
	}
#endif
}


void FHoudiniEngineStatusManager::AddLog(const TCHAR* V, ELogVerbosity::Type Verbosity)
{
	FScopeLock Lock(&Mutex);

	FHoudiniCookableStatus* CookableStatus = CurrentStatuses.Find(GetActiveCookable());
	if (!CookableStatus)
	{
		CookableStatus = &CurrentStatuses.Add(GetActiveCookable(), {});

	}
	FHoudiniLogRecord& Record = CookableStatus->Logs.Emplace_GetRef();
	Record.Verbosity = Verbosity;
	Record.LogMessage = V;
	Record.Cookable = GetActiveCookable();

	switch (Verbosity)
	{
	case ELogVerbosity::Type::Warning:
		CookableStatus->NumWarnings++;
		break;
	case ELogVerbosity::Type::Error:
		CookableStatus->NumErrors++;
		break;
	default:
		break;
	}
}

void FHoudiniEngineStatusManager::GetSessionStatusAndColor(const UHoudiniCookable* Cookable, FString& OutStatusString, FLinearColor& OutStatusColor)
{
	FScopeLock Lock(&Mutex);

	FHoudiniCookableStatus* CookableStatus = CurrentStatuses.Find(Cookable);

	EHoudiniSessionStatus SessionStatus = FHoudiniEngine::Get().GetSessionStatus();
	if(!CookableStatus || SessionStatus != EHoudiniSessionStatus::Connected)
	{
		this->ClearStatus(Cookable);
		FHoudiniEngine::Get().GetSessionStatusAndColor(OutStatusString, OutStatusColor);
		
		return;
	}

	auto GetWarnings = [](int Count) -> FString
		{
			if(Count == 1)
			{
				return FString(TEXT("1 Warning"));
			}
			else
			{
				return FString::Printf(TEXT("%d Warnings"), Count);
			}

		};

	auto GetErrors = [](int Count)
		{
			if(Count == 1)
			{
				return FString(TEXT("1 Error"));
			}
			else
			{
				return FString::Printf(TEXT("%d Errors"), Count);
			}
		};

	switch (CookableStatus->Status)
	{
	case EHoudiniStatusManagerStatus::Cooking:
		{
			FLinearColor Cyan(0.0f, 1.0f, 1.0f);
			OutStatusColor = Cyan;

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
			TStringBuilder<256> StringBuilder;
#else
			FStringBuilderBase StringBuilder;
#endif
			StringBuilder.Append(TEXT("Cooking..."));

			double DeltaTime = FPlatformTime::Seconds() - CookableStatus->StartTime;

			int TotalDotCount = 3;
			int32 DotCount = static_cast<int32>(FMath::Fmod(DeltaTime, static_cast<double>(TotalDotCount)));
			for(int32 Dot = 0; Dot < TotalDotCount; Dot++)
				if (Dot < DotCount)
					StringBuilder.Append(TEXT("."));
				else
					StringBuilder.Append(TEXT(" "));

			OutStatusString = StringBuilder.ToString();
		}
		break;

	case EHoudiniStatusManagerStatus::PDGExecuting:
	{
		FLinearColor Cyan(0.0f, 1.0f, 1.0f);
		OutStatusColor = Cyan;

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
		TStringBuilder<256> StringBuilder;
#else
		FStringBuilderBase StringBuilder;
#endif
		StringBuilder.Append(TEXT("PDG Executing..."));

		const UHoudiniPDGAssetLink* AssetLink = Cookable->GetPDGData()->PDGAssetLink;
		if (AssetLink)
		{
			int Waiting = AssetLink->WorkItemTally.NumWaitingWorkItems();
			int Complete = AssetLink->WorkItemTally.NumWorkItems();

			if (Complete > 0)
			{
				int Percent = 100 - (Waiting * 100 / Complete);
				StringBuilder.Append(FString::Printf(TEXT("%d Complete"), Percent));
			}
		}
		OutStatusString = StringBuilder.ToString();
	}
	break;

	case EHoudiniStatusManagerStatus::Baking:
		{
			FLinearColor Cyan(0.0f, 1.0f, 1.0f);
			OutStatusColor = Cyan;

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
			TStringBuilder<256> StringBuilder;
#else
			FStringBuilderBase StringBuilder;
#endif
			StringBuilder.Append(TEXT("Baking..."));

			double DeltaTime = FPlatformTime::Seconds() - CookableStatus->StartTime;

			int TotalDotCount = 5;
			int32 DotCount = static_cast<int32>(FMath::Fmod(DeltaTime, static_cast<double>(TotalDotCount)));
			for(int32 Dot = 0; Dot < TotalDotCount; Dot++)
				if(Dot < DotCount)
					StringBuilder.Append(TEXT("."));
				else
					StringBuilder.Append(TEXT(" "));

			OutStatusString = StringBuilder.ToString();
		}
		break;
	case EHoudiniStatusManagerStatus::Idle:
		{
			FHoudiniEngine::Get().GetSessionStatusAndColor(OutStatusString, OutStatusColor);
		}
		break;
	case EHoudiniStatusManagerStatus::CookComplete:
		{
			if(CookableStatus->NumErrors > 0)
			{
				OutStatusColor = FLinearColor(1.0f, 0.5f, 0.5f);
				OutStatusString = FString::Printf(TEXT("Cook Unsuccessful. %s and %s"), *GetErrors(CookableStatus->NumErrors), *GetWarnings(CookableStatus->NumWarnings));
			}
			else if(CookableStatus->NumWarnings > 0)
			{
				OutStatusColor = FLinearColor::Yellow;
				OutStatusString = FString::Printf(TEXT("Cook Completed with %s"), *GetWarnings(CookableStatus->NumWarnings));
			}
			else
			{
				OutStatusColor = FLinearColor::Green;
				OutStatusString = TEXT("HDA Successfully Cooked");
			}
		}
		break;
	case EHoudiniStatusManagerStatus::Instantiated:
	{
		if(CookableStatus->NumErrors > 0)
		{
			OutStatusColor = FLinearColor(1.0f, 0.5f, 0.5f);
			OutStatusString = FString::Printf(TEXT("Instantiated. %s and %s"), *GetErrors(CookableStatus->NumErrors), *GetWarnings(CookableStatus->NumWarnings));
		}
		else if(CookableStatus->NumWarnings > 0)
		{
			OutStatusColor = FLinearColor::Yellow;
			OutStatusString = FString::Printf(TEXT("Instantiated with %s"), *GetWarnings(CookableStatus->NumWarnings));
		}
		else
		{
			OutStatusColor = FLinearColor::Green;
			OutStatusString = TEXT("Instantiated");
		}
	}
	break;
	case EHoudiniStatusManagerStatus::BakingComplete:
		{
			if(CookableStatus->NumErrors > 0)
			{
				OutStatusColor = FLinearColor::Red;
				OutStatusString = FString::Printf(TEXT("Cook Unsuccessful. %s and %s"), *GetErrors(CookableStatus->NumErrors), *GetWarnings(CookableStatus->NumWarnings));
			}
			else if(CookableStatus->NumWarnings > 0)
			{
				OutStatusColor = FLinearColor::Yellow;
				OutStatusString = FString::Printf(TEXT("Cook Completed with %s"), *GetWarnings(CookableStatus->NumWarnings));
			}
			else
			{
				OutStatusColor = FLinearColor::Green;
				OutStatusString = TEXT("HDA Successfully Baked");
			}
		}
		break;
	case EHoudiniStatusManagerStatus::PDGComplete:
	{
		if(CookableStatus->NumErrors > 0)
		{
			OutStatusColor = FLinearColor::Red;
			OutStatusString = FString::Printf(TEXT("PDG Unsuccessful. %s and %s"), *GetErrors(CookableStatus->NumErrors), *GetWarnings(CookableStatus->NumWarnings));
		}
		else if(CookableStatus->NumWarnings > 0)
		{
			OutStatusColor = FLinearColor::Yellow;
			OutStatusString = FString::Printf(TEXT("PDG Completed with %s"), *GetWarnings(CookableStatus->NumWarnings));
		}
		else
		{
			OutStatusColor = FLinearColor::Green;
			OutStatusString = TEXT("PDG Successfully Complete");
		}
	}
	break;
	}
}

FString FHoudiniEngineStatusManager::GetLogs(const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs)
{
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
	TStringBuilder<256> StringBuilder;
#else
	FStringBuilderBase StringBuilder;
#endif

	for (auto & Cookable : InHCs)
	{

		FHoudiniCookableStatus* CookableStatus = CurrentStatuses.Find(Cookable.Get());
		if(CookableStatus)
		{
			for(auto Record : CookableStatus->Logs)
			{
				if(Record.Verbosity == ELogVerbosity::Type::Error)
				{
					StringBuilder.Append(TEXT("Error: "));
				}
				else if(Record.Verbosity == ELogVerbosity::Type::Warning)
				{
					StringBuilder.Append(TEXT("Warning: "));
				}


				StringBuilder.Append(Record.LogMessage);
				StringBuilder.Append(TEXT("\n"));
			}
		}
	}
	return StringBuilder.ToString();
}


