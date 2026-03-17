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
#include "HoudiniStatusManager.h"

class UHoudiniCookable;

class HOUDINIENGINE_API FHoudiniEngineStatusManager : public FHoudiniStatusManager
{
public:
	static void Initialize();

	virtual ~FHoudiniEngineStatusManager() override {}

	virtual void OnSessionLost() override;

	virtual void StartInstantiating(UHoudiniCookable* Cookable) override;
	virtual void EndInstantiating(UHoudiniCookable* Cookable, bool bSuccess) override;

	virtual void StartCooking(UHoudiniCookable * Cookable) override;
	virtual void EndCooking(UHoudiniCookable* Cookable, bool bSuccess) override;

	virtual void StartBaking(UHoudiniCookable* Cookable) override;
	virtual void EndBaking(UHoudiniCookable* Cookable, bool bSuccess) override;

	virtual void StartPDG(UHoudiniCookable* Cookable) override;
	virtual void EndPDG(UHoudiniCookable* Cookable, bool bSuccess) override;

	virtual void ClearStatus(const UHoudiniCookable* Cookable) override;

	virtual void AddLog(const TCHAR* V, ELogVerbosity::Type Verbosity);

	virtual void GetSessionStatusAndColor(const UHoudiniCookable * Cookable, FString& OutStatusString, FLinearColor& OutStatusColor) override;

	virtual FString GetLogs(const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs) override;
};

class HOUDINIENGINE_API FHoudiniStatusManagerHandle
{
	// RAII Handle for settings/clearing Active Cookable.

public:
	FHoudiniStatusManagerHandle(UHoudiniCookable* Cookable)
	{
		auto Instance = FHoudiniEngineStatusManager::Get();
		if(Instance != nullptr)
		{
			Prev = FHoudiniEngineStatusManager::Get()->GetActiveCookable();
			FHoudiniEngineStatusManager::Get()->SetActiveCookable(Cookable);
		}
	}

	~FHoudiniStatusManagerHandle()
	{
		auto Instance = FHoudiniEngineStatusManager::Get();
		if(Instance != nullptr)
		{
			Instance->SetActiveCookable(Prev.IsValid() ? Prev.Get() : nullptr);
		}
	}
	TWeakObjectPtr<UHoudiniCookable> Prev;

};




