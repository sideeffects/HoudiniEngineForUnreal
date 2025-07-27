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

#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

IMPLEMENT_MODULE(FHoudiniEngineRuntime, HoudiniEngineRuntime);
DEFINE_LOG_CATEGORY(LogHoudiniEngineRuntime);

FHoudiniEngineRuntime *
FHoudiniEngineRuntime::HoudiniEngineRuntimeInstance = nullptr;


FHoudiniEngineRuntime &
FHoudiniEngineRuntime::Get()
{
	return *HoudiniEngineRuntimeInstance;
}


bool
FHoudiniEngineRuntime::IsInitialized()
{
	return FHoudiniEngineRuntime::HoudiniEngineRuntimeInstance != nullptr;
}


FHoudiniEngineRuntime::FHoudiniEngineRuntime()
{
}


void FHoudiniEngineRuntime::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	// Store the instance.
	FHoudiniEngineRuntime::HoudiniEngineRuntimeInstance = this;
}


void FHoudiniEngineRuntime::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FHoudiniEngineRuntime::HoudiniEngineRuntimeInstance = nullptr;
}


void 
FHoudiniEngineRuntime::MarkNodeIdAsPendingDelete(const int32& InNodeId, bool bDeleteParent)
{
	if (InNodeId >= 0) 
	{
		// FDebug::DumpStackTraceToLog();
		NodeIdsPendingDelete.AddUnique(InNodeId);

		if (bDeleteParent)
		{
			NodeIdsParentPendingDelete.AddUnique(InNodeId);
		}
	}
}


int32
FHoudiniEngineRuntime::GetNodeIdsPendingDeleteCount()
{
	if (!IsInitialized())
		return 0;

	FScopeLock ScopeLock(&CriticalSection);

	return NodeIdsPendingDelete.Num();
}


int32
FHoudiniEngineRuntime::GetNodeIdsPendingDeleteAt(const int32& Index)
{
	if (!IsInitialized())
		return -1;

	FScopeLock ScopeLock(&CriticalSection);

	if (!NodeIdsPendingDelete.IsValidIndex(Index))
		return -1;

	return NodeIdsPendingDelete[Index];
}


void
FHoudiniEngineRuntime::RemoveNodeIdPendingDeleteAt(const int32& Index)
{
	if (!IsInitialized())
		return;

	FScopeLock ScopeLock(&CriticalSection);
	if (!NodeIdsPendingDelete.IsValidIndex(Index))
		return;

	NodeIdsPendingDelete.RemoveAt(Index);
}


bool 
FHoudiniEngineRuntime::IsParentNodePendingDelete(const int32& NodeId) 
{
	return NodeIdsParentPendingDelete.Contains(NodeId);
}


void 
FHoudiniEngineRuntime::RemoveParentNodePendingDelete(const int32& NodeId) 
{
	if (NodeIdsParentPendingDelete.Contains(NodeId))
		NodeIdsParentPendingDelete.Remove(NodeId);
}


FString
FHoudiniEngineRuntime::GetDefaultTemporaryCookFolder() const
{
	// Get Runtime settings to get the Temp Cook Folder
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (!HoudiniRuntimeSettings)
		return HAPI_UNREAL_DEFAULT_TEMP_COOK_FOLDER;

	return HoudiniRuntimeSettings->DefaultTemporaryCookFolder;
}


FString
FHoudiniEngineRuntime::GetDefaultBakeFolder() const
{
	// Get Runtime settings to get the default bake Folder
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (!HoudiniRuntimeSettings)
		return HAPI_UNREAL_DEFAULT_BAKE_FOLDER;

	return HoudiniRuntimeSettings->DefaultBakeFolder;
}



int32
FHoudiniEngineRuntime::GetRegisteredHoudiniCookableCount()
{
	if (!IsInitialized())
		return 0;

	FScopeLock ScopeLock(&CriticalSection);
	return RegisteredHoudiniCookables.Num();
}


UHoudiniCookable*
FHoudiniEngineRuntime::GetRegisteredHoudiniCookableAt(const int32& Index)
{
	if (!IsInitialized())
		return nullptr;

	FScopeLock ScopeLock(&CriticalSection);
	if (!RegisteredHoudiniCookables.IsValidIndex(Index))
		return nullptr;

	TWeakObjectPtr<UHoudiniCookable> Ptr = RegisteredHoudiniCookables[Index];
	if (!Ptr.IsValid())
		return nullptr;

	if (Ptr.IsStale())
		return nullptr;

	return Ptr.Get();
}


void
FHoudiniEngineRuntime::CleanUpRegisteredHoudiniCookables()
{
	// Remove Stale and invalid Cookables
	FScopeLock ScopeLock(&CriticalSection);
	for (int Idx = RegisteredHoudiniCookables.Num() - 1; Idx >= 0; Idx--)
	{
		TWeakObjectPtr<UHoudiniCookable> Ptr = RegisteredHoudiniCookables[Idx];
		if (!Ptr.IsValid() || Ptr.IsStale())
		{
			UnRegisterHoudiniCookable(Idx);
			continue;
		}

		UHoudiniCookable* CurrentHC = Ptr.Get();
		if (!IsValid(CurrentHC))
		{
			UnRegisterHoudiniCookable(Idx);
			continue;
		}
	}
}


bool
FHoudiniEngineRuntime::IsCookableRegistered(UHoudiniCookable* HC) const
{
	// No need for duplicates
	if (HC && RegisteredHoudiniCookables.Find(HC) != INDEX_NONE)
		return true;

	return false;
}


void
FHoudiniEngineRuntime::RegisterHoudiniCookable(UHoudiniCookable* HC, bool bAllowArchetype)
{
	if (!FHoudiniEngineRuntime::IsInitialized())
		return;

	if (!IsValid(HC))
		return;

	// RF_Transient indicates a temporary/preview object
	// No need to instantiate/cook those in Houdini
	// RF_ArchetypeObject is the template for blueprinted HDA, so we need to be able to register those.
	if (HC->HasAnyFlags(RF_Transient) || (HC->HasAnyFlags(RF_ArchetypeObject) && !bAllowArchetype) || HC->HasAnyFlags(RF_ClassDefaultObject))
		return;

	// No need for duplicates
	if (IsCookableRegistered(HC))
		return;

	HOUDINI_BP_MESSAGE(TEXT("[FHoudiniEngineRuntime::RegisterHoudiniCookable] HAC: %s"), *(HC->GetPathName()));

	// Before adding, clean up the all ready registered
	CleanUpRegisteredHoudiniCookables();

	// Add the new Cookable
	{
		FScopeLock ScopeLock(&CriticalSection);
		RegisteredHoudiniCookables.Add(HC);
	}

	HC->NotifyHoudiniRegisterCompleted();
}



void
FHoudiniEngineRuntime::UnRegisterHoudiniCookable(UHoudiniCookable* HC)
{
	if (!IsInitialized())
		return;

	if (!IsValid(HC))
		return;

	if (RegisteredHoudiniCookables.IsEmpty())
		return;

	// Calling GetPathName here may lead to some crashes due to invalid outers...
	//HOUDINI_LOG_DISPLAY(TEXT("[FHoudiniEngineRuntime::UnRegisterHoudiniCookable] HC: %s"), *(HC->GetPathName()) );

	FScopeLock ScopeLock(&CriticalSection);

	/*	
	int32 FoundIdx = -1;
	for (int32 n = RegisteredHoudiniCookables.Num() - 1; n >= 0; n--)
	{
		TWeakObjectPtr<UHoudiniCookable>& CurHC = RegisteredHoudiniCookables[n];
		if (!CurHC.IsValid(true) || CurHC.IsStale(false))
		{
			// Remove stale/invalid HAC from Array?
			RegisteredHoudiniCookables.RemoveAt(n);
			continue;
		}

		if (CurHC.Get() == HC)
			FoundIdx = n;
	}

	if (FoundIdx < 0 || !RegisteredHoudiniCookables.IsValidIndex(FoundIdx))
		return;
	*/

	int32 FoundIdx = -1;
	for(int nIdx = 0; nIdx < RegisteredHoudiniCookables.Num(); nIdx++)
	{
		TWeakObjectPtr<UHoudiniCookable> Ptr = RegisteredHoudiniCookables[nIdx];
		if(!Ptr.IsValid(true, true))
			continue;

		UHoudiniCookable* CurrentHC = Ptr.GetEvenIfUnreachable();
		if(CurrentHC && CurrentHC == HC)
		{
			FoundIdx = nIdx;
			break;
		}
	}

	if (FoundIdx != -1)
	{
		HC->NotifyHoudiniPreUnregister();
		UnRegisterHoudiniCookable(FoundIdx);
		HC->NotifyHoudiniPostUnregister();
	}
}


void
FHoudiniEngineRuntime::UnRegisterHoudiniCookable(const int32& ValidIndex)
{
	if (!IsInitialized())
		return;

	FScopeLock ScopeLock(&CriticalSection);
	TWeakObjectPtr<UHoudiniCookable> Ptr = RegisteredHoudiniCookables[ValidIndex];

	if (Ptr.IsValid(true, true))
	{
		UHoudiniCookable* HC = Ptr.GetEvenIfUnreachable();
		if (HC && HC->CanDeleteHoudiniNodes() && HC->GetNodeId() >= 0)
		{
			MarkNodeIdAsPendingDelete(HC->GetNodeId(), true);
			HC->SetNodeId(INDEX_NONE);
		}
	}

	RegisteredHoudiniCookables.RemoveAt(ValidIndex);
}

static TAutoConsoleVariable<int32> CVarHoudiniPCGLogging(
	TEXT("Houdini.PCGLogging"),
	0, // default value
	TEXT("Enable (1) or disable (0) PCG Logging."),
	ECVF_Default
);

bool IsHoudiniPCGLoggingEnabled()
{
	bool Enabled = CVarHoudiniPCGLogging.GetValueOnAnyThread() != 0;
	return Enabled;
}
#undef LOCTEXT_NAMESPACE

