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

#include "CoreGlobals.h"
#include "UObject/WeakObjectPtr.h"

class UHoudiniCookable;

struct FHoudiniLogRecord
{
	ELogVerbosity::Type Verbosity;
	FString LogMessage;
	TWeakObjectPtr<UHoudiniCookable> Cookable;
};

enum EHoudiniStatusManagerStatus
{
	Idle,				// With not connected, or connected before first cook.
	Instantiating,		// Instantiating
	Instantiated,
	Cooking,			// A Cook is in progress.
	CookComplete,		// Cooking complete.
	Baking,				// A Bake is in progress.
	BakingComplete,		// Baking complete.
	PDGExecuting,		// PDG is in progress.
	PDGComplete			// PDG is complete.
};


struct FHoudiniCookableStatus
{
	TArray<FHoudiniLogRecord> Logs;
	int NumErrors = 0;
	int NumWarnings = 0;
	EHoudiniStatusManagerStatus Status = EHoudiniStatusManagerStatus::Idle;
	double StartTime = 0.0;

};

class HOUDINIENGINERUNTIME_API FHoudiniStatusManager
{
public:
	virtual ~FHoudiniStatusManager() {}
	static FHoudiniStatusManager* Get();

	virtual void OnSessionLost() {};

	virtual void StartInstantiating(UHoudiniCookable* Cookable) {};
	virtual void EndInstantiating(UHoudiniCookable* Cookable, bool bSuccess) {};

	virtual void StartCooking(UHoudiniCookable* Cookable) {};
	virtual void EndCooking(UHoudiniCookable* Cookable, bool bSuccess) {};

	virtual void StartBaking(UHoudiniCookable* Cookable) {};
	virtual void EndBaking(UHoudiniCookable* Cookable, bool bSuccess) {};

	virtual void StartPDG(UHoudiniCookable* Cookable) {};
	virtual void EndPDG(UHoudiniCookable* Cookable, bool bSuccess) {};

	virtual void ClearStatus(const UHoudiniCookable* Cookable) {};

	virtual void AddLog(const TCHAR* V, ELogVerbosity::Type Verbosity) {};

	virtual void GetSessionStatusAndColor(const UHoudiniCookable* Cookable, FString& OutStatusString, FLinearColor& OutStatusColor) {};

	virtual FString GetLogs(const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs) { return FString(); };

	void SetActiveCookable(UHoudiniCookable* Cookable) ;
	UHoudiniCookable* GetActiveCookable() const;
	void ClearActiveCookable();

	// BEWARE! DO Not change the name of these functions, I tried "LogError" and got all kind of weird macro expansion bugs.
	static void ErrorLog(const TCHAR* Format, ...);
	static void WarningLog(const TCHAR* Format, ...);

	TWeakObjectPtr<UHoudiniCookable> ActiveCookable;

protected:
	static FHoudiniStatusManager* Instance;
	static FCriticalSection Mutex; // logs come in from different threads, we provide protection.
	TMap<UHoudiniCookable*, FHoudiniCookableStatus> CurrentStatuses;
};

