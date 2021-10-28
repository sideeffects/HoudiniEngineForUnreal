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

#include "HoudiniAssetComponent.h"

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


int32 
FHoudiniEngineRuntime::GetRegisteredHoudiniComponentCount()
{ 
	if (!IsInitialized())
		return 0;

	FScopeLock ScopeLock(&CriticalSection);	
	return RegisteredHoudiniComponents.Num();
}


UHoudiniAssetComponent*
FHoudiniEngineRuntime::GetRegisteredHoudiniComponentAt(const int32& Index)
{
	if (!IsInitialized())
		return nullptr;

	FScopeLock ScopeLock(&CriticalSection);

	if (!RegisteredHoudiniComponents.IsValidIndex(Index))
		return nullptr;	

	TWeakObjectPtr<UHoudiniAssetComponent> Ptr = RegisteredHoudiniComponents[Index];
	if (!Ptr.IsValid())
		return nullptr;

	if (Ptr.IsStale())
		return nullptr;

	return Ptr.Get();
}


void
FHoudiniEngineRuntime::CleanUpRegisteredHoudiniComponents()
{
	// Remove Stale and invalid components
	FScopeLock ScopeLock(&CriticalSection);
	for (int Idx = RegisteredHoudiniComponents.Num() - 1; Idx >= 0; Idx--)
	{
		TWeakObjectPtr<UHoudiniAssetComponent> Ptr = RegisteredHoudiniComponents[Idx];
		if ( !Ptr.IsValid() || Ptr.IsStale() )
		{
			UnRegisterHoudiniComponent(Idx);
			continue;
		}

		UHoudiniAssetComponent* CurrentHAC = Ptr.Get();
		if (!IsValid(CurrentHAC))
		{
			UnRegisterHoudiniComponent(Idx);
			continue;
		}
	}
}


bool
FHoudiniEngineRuntime::IsComponentRegistered(UHoudiniAssetComponent* HAC) const
{
	// No need for duplicates
	if (HAC && RegisteredHoudiniComponents.Find(HAC) != INDEX_NONE)
		return true;

	return false;
}


void
FHoudiniEngineRuntime::RegisterHoudiniComponent(UHoudiniAssetComponent* HAC, bool bAllowArchetype)
{
	if (!FHoudiniEngineRuntime::IsInitialized())
		return;

	if (!IsValid(HAC))
		return;

	// RF_Transient indicates a temporary/preview object
	// No need to instantiate/cook those in Houdini
	// RF_ArchetypeObject is the template for blueprinted HDA, so we need to be able to register those.
	if (HAC->HasAnyFlags(RF_Transient) || (HAC->HasAnyFlags(RF_ArchetypeObject) && !bAllowArchetype) || HAC->HasAnyFlags(RF_ClassDefaultObject))
		return;

	// No need for duplicates
	if (IsComponentRegistered(HAC))
		return;

	HOUDINI_BP_MESSAGE(TEXT("[FHoudiniEngineRuntime::RegisterHoudiniComponent] HAC: %s"), *(HAC->GetPathName()) );

	// Before adding, clean up the all ready registered
	CleanUpRegisteredHoudiniComponents();

	// Add the new component
	{
		FScopeLock ScopeLock(&CriticalSection);
		RegisteredHoudiniComponents.Add(HAC);
	}

	HAC->NotifyHoudiniRegisterCompleted();
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


void
FHoudiniEngineRuntime::UnRegisterHoudiniComponent(UHoudiniAssetComponent* HAC)
{
	if (!IsInitialized())
		return;

	if (!IsValid(HAC))
		return;

	// Calling GetPathName here may lead to some crashes due to invalid outers...
	//HOUDINI_LOG_DISPLAY(TEXT("[FHoudiniEngineRuntime::UnRegisterHoudiniComponent] HAC: %s"), *(HAC->GetPathName()) );

	FScopeLock ScopeLock(&CriticalSection);

	int32 FoundIdx = RegisteredHoudiniComponents.Find(HAC);
	if (!RegisteredHoudiniComponents.IsValidIndex(FoundIdx))
		return;
	HAC->NotifyHoudiniPreUnregister();
	UnRegisterHoudiniComponent(FoundIdx);
	HAC->NotifyHoudiniPostUnregister();
}


void
FHoudiniEngineRuntime::UnRegisterHoudiniComponent(const int32& ValidIndex)
{
	if (!IsInitialized())
		return;

	FScopeLock ScopeLock(&CriticalSection);

	TWeakObjectPtr<UHoudiniAssetComponent> Ptr = RegisteredHoudiniComponents[ValidIndex];
	if (Ptr.IsValid(true, false))
	{
		UHoudiniAssetComponent* HAC = Ptr.Get();
		if (HAC && HAC->CanDeleteHoudiniNodes())
		{
			MarkNodeIdAsPendingDelete(HAC->GetAssetId(), true);
		}
	}
	
	RegisteredHoudiniComponents.RemoveAt(ValidIndex);
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

#undef LOCTEXT_NAMESPACE

