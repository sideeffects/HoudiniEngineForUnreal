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

void FHoudiniEngineStatusManager::StartCancelling(UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus.Status = EHoudiniStatusManagerStatus::Cancelling;
	CookableStatus.StartTime = FPlatformTime::Seconds();
	CookableStatus.bCancelledDueToChange = false;
}

void FHoudiniEngineStatusManager::EndCancelling(UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);
	FHoudiniCookableStatus& CookableStatus = CurrentStatuses.FindOrAdd(Cookable);
	CookableStatus.Status = EHoudiniStatusManagerStatus::Cancelled;
	CookableStatus.bCancelledDueToChange = Cookable
		&& (Cookable->GetCancelReason() == EHoudiniCookableCancelReason::CancelledDueToChange
			|| Cookable->GetCancelReason() == EHoudiniCookableCancelReason::CancelledDueToChangeAndRestart);
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
	const EHoudiniStatusManagerStatus Status = Cookable && Cookable->IsLocked()
		? EHoudiniStatusManagerStatus::Locked
		: CookableStatus ? CookableStatus->Status : EHoudiniStatusManagerStatus::Idle;

	EHoudiniSessionStatus SessionStatus = FHoudiniEngine::Get().GetSessionStatus();
	if (Status != EHoudiniStatusManagerStatus::Locked
		&& (!CookableStatus || SessionStatus != EHoudiniSessionStatus::Connected))
	{
		this->ClearStatus(Cookable);
		FHoudiniEngine::Get().GetSessionStatusAndColor(OutStatusString, OutStatusColor);
		
		return;
	}

	FLinearColor Cyan(0.0f, 1.0f, 1.0f);

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

	switch (Status)
	{
	case EHoudiniStatusManagerStatus::Locked:
		{
			OutStatusColor = FLinearColor(0.0f, 1.0f, 1.0f);
			OutStatusString = TEXT("Locked");
		}
		break;
	case EHoudiniStatusManagerStatus::Cooking:
		{
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
	case EHoudiniStatusManagerStatus::Cancelling:
		{
			OutStatusColor = FLinearColor::Yellow;

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
			TStringBuilder<256> StringBuilder;
#else
			FStringBuilderBase StringBuilder;
#endif
			const EHoudiniCookableCancelReason CancelReason = Cookable
				? Cookable->GetCancelReason()
				: EHoudiniCookableCancelReason::None;

			switch (CancelReason)
			{
				case EHoudiniCookableCancelReason::None:
				{
					StringBuilder.Append(TEXT("Cancelling"));
					break;
				}

				case EHoudiniCookableCancelReason::CancelledDueToChange:
				case EHoudiniCookableCancelReason::CancelledDueToChangeAndRestart:
				{
					StringBuilder.Append(TEXT("Cancelling due to inputs/parameter change.."));
					break;
				}

				case EHoudiniCookableCancelReason::UserCancelled:
				default:
				{
					StringBuilder.Append(TEXT("Cancelling.."));
					break;
				}
			}

			double DeltaTime = FPlatformTime::Seconds() - CookableStatus->StartTime;

			int TotalDotCount = 3;
			int32 DotCount = static_cast<int32>(FMath::Fmod(DeltaTime, static_cast<double>(TotalDotCount)));
			for (int32 Dot = 0; Dot < TotalDotCount; Dot++)
				if (Dot < DotCount)
					StringBuilder.Append(TEXT("."));
				else
					StringBuilder.Append(TEXT(" "));

			switch (CancelReason)
			{
				case EHoudiniCookableCancelReason::None:
				{
					break;
				}

				case EHoudiniCookableCancelReason::CancelledDueToChangeAndRestart:
				{
					StringBuilder.Append(TEXT("\nwill restart cooking after cancellation"));
					break;
				}

				case EHoudiniCookableCancelReason::CancelledDueToChange:
				case EHoudiniCookableCancelReason::UserCancelled:
				default:
				{
					break;
				}
			}

			OutStatusString = StringBuilder.ToString();
		}
		break;
	case EHoudiniStatusManagerStatus::Cancelled:
		{
			OutStatusColor = FLinearColor::Yellow;
			OutStatusString = CookableStatus->bCancelledDueToChange
				? TEXT("Cook Cancelled due to inputs/parameter change")
				: TEXT("Cancelled");
		}
		break;

	case EHoudiniStatusManagerStatus::PDGExecuting:
	{
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
	case EHoudiniStatusManagerStatus::Instantiating:
		{
			OutStatusColor = Cyan;
			OutStatusString = TEXT("Instantiating...");
		}
		break;
	default:
		{
			OutStatusColor = Cyan;
			OutStatusString = TEXT("Processing...");
		}
		break;

	break;
	}
}

FText FHoudiniEngineStatusManager::GetSessionStatusTooltip(const UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);

	FHoudiniCookableStatus* CookableStatus = CurrentStatuses.Find(Cookable);
	const EHoudiniStatusManagerStatus Status = Cookable && Cookable->IsLocked()
		? EHoudiniStatusManagerStatus::Locked
		: CookableStatus ? CookableStatus->Status : EHoudiniStatusManagerStatus::Idle;

	const EHoudiniSessionStatus SessionStatus = FHoudiniEngine::Get().GetSessionStatus();
	if (Status != EHoudiniStatusManagerStatus::Locked
		&& (!CookableStatus || SessionStatus != EHoudiniSessionStatus::Connected))
	{
		switch (SessionStatus)
		{
		case EHoudiniSessionStatus::NotStarted:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionNotStartedTooltip", "The Houdini Engine session has not been started.");
		case EHoudiniSessionStatus::Connected:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionConnectedTooltip", "The Houdini Engine session is connected and ready.");
		case EHoudiniSessionStatus::Stopped:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionStoppedTooltip", "The Houdini Engine session has been stopped.");
		case EHoudiniSessionStatus::Failed:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionFailedTooltip", "The Houdini Engine session failed to start or connect.");
		case EHoudiniSessionStatus::Lost:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionLostTooltip", "The connection to the Houdini Engine session was lost.");
		case EHoudiniSessionStatus::NoLicense:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionNoLicenseTooltip", "Houdini Engine could not acquire a license for this session.");
		case EHoudiniSessionStatus::Connecting:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionConnectingTooltip", "Houdini Engine is connecting to a session.");
		case EHoudiniSessionStatus::None:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionDisabledTooltip", "The Houdini Engine session is disabled in the plugin settings.");
		case EHoudiniSessionStatus::Invalid:
		default:
			return NSLOCTEXT("HoudiniEngineStatusManager", "SessionInvalidTooltip", "The Houdini Engine session is invalid.");
		}
	}

	switch (Status)
	{
	case EHoudiniStatusManagerStatus::Locked:
		return NSLOCTEXT("HoudiniEngineStatusManager", "LockedTooltip", "This Houdini Asset Component is locked. Click the lock icon at the right of the Cook button bar to unlock it.");
	case EHoudiniStatusManagerStatus::Instantiating:
		return NSLOCTEXT("HoudiniEngineStatusManager", "InstantiatingTooltip", "Houdini Engine is creating this Houdini Asset Component.");
	case EHoudiniStatusManagerStatus::Instantiated:
		return NSLOCTEXT("HoudiniEngineStatusManager", "InstantiatedTooltip", "This Houdini Asset Component was created successfully.");
	case EHoudiniStatusManagerStatus::Cooking:
		return NSLOCTEXT("HoudiniEngineStatusManager", "CookingTooltip", "Houdini Engine is cooking this Houdini Asset Component.");
	case EHoudiniStatusManagerStatus::Cancelling:
		return NSLOCTEXT("HoudiniEngineStatusManager", "CancellingTooltip", "Houdini Engine is cancelling the active cook.\nThis may take some time if an individual SOP takes a long time to complete.");
	case EHoudiniStatusManagerStatus::Cancelled:
		return NSLOCTEXT("HoudiniEngineStatusManager", "CancelledTooltip", "The Houdini Asset Component cook was cancelled.");
	case EHoudiniStatusManagerStatus::CookComplete:
		return NSLOCTEXT("HoudiniEngineStatusManager", "CookCompleteTooltip", "The Houdini Asset Component cook has completed. Click to view its cook log.");
	case EHoudiniStatusManagerStatus::Baking:
		return NSLOCTEXT("HoudiniEngineStatusManager", "BakingTooltip", "Houdini Engine is baking this Houdini Asset Component.");
	case EHoudiniStatusManagerStatus::BakingComplete:
		return NSLOCTEXT("HoudiniEngineStatusManager", "BakingCompleteTooltip", "The Houdini Asset Component bake has completed. Click to view its cook log.");
	case EHoudiniStatusManagerStatus::PDGExecuting:
		return NSLOCTEXT("HoudiniEngineStatusManager", "PDGExecutingTooltip", "PDG is executing work items for this Houdini Asset Component.");
	case EHoudiniStatusManagerStatus::PDGComplete:
		return NSLOCTEXT("HoudiniEngineStatusManager", "PDGCompleteTooltip", "PDG has completed work for this Houdini Asset Component. Click to view its cook log.");
	case EHoudiniStatusManagerStatus::Idle:
	default:
		return NSLOCTEXT("HoudiniEngineStatusManager", "IdleTooltip", "The Houdini Asset Component is idle.");
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
