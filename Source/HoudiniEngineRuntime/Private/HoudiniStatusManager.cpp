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

#include "HoudiniStatusManager.h"
#include "HoudiniCookable.h"

FHoudiniStatusManager* FHoudiniStatusManager::Instance = nullptr;
FCriticalSection FHoudiniStatusManager::Mutex;

FHoudiniStatusManager* FHoudiniStatusManager::Get()
{
	if (Instance == nullptr)
	{
		// This should never happen as the derived class is initialized on module startup.
		Instance = new FHoudiniStatusManager();
	}
	return Instance;
}

void FHoudiniStatusManager::ErrorLog(const TCHAR* Format, ...)
{
	if(Instance)
	{
		va_list Args;
		va_start(Args, Format);
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
		TStringBuilder<256> Builder;
#else
		FStringBuilderBase Builder;
#endif
		Builder.AppendV(Format, Args);
		Instance->AddLog(Builder.ToString(), ELogVerbosity::Error);
		va_end(Args);
	}
}

void FHoudiniStatusManager::WarningLog(const TCHAR* Format, ...)
{
	if(Instance)
	{
		va_list Args;
		va_start(Args, Format);
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 7)
		TStringBuilder<256> Builder;
#else
		FStringBuilderBase Builder;
#endif
		Builder.AppendV(Format, Args);
		Instance->AddLog(Builder.ToString(), ELogVerbosity::Warning);
		va_end(Args);
	}
}


void FHoudiniStatusManager::SetActiveCookable(UHoudiniCookable* Cookable)
{
	FScopeLock Lock(&Mutex);
	ActiveCookable = Cookable;
}
void FHoudiniStatusManager::ClearActiveCookable()
{
	SetActiveCookable(nullptr);
}

UHoudiniCookable* 
FHoudiniStatusManager::GetActiveCookable() const
{
	return ActiveCookable.IsValid() ? ActiveCookable.Get() : nullptr;
}


