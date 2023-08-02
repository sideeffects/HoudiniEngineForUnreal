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

#pragma once

#include "HoudiniAssetComponent.h"
#include "HoudiniPDGAssetLink.h"

#include "Modules/ModuleInterface.h"
#include "Misc/ScopeLock.h"
#include "UObject/WeakObjectPtrTemplates.h"

class HOUDINIENGINERUNTIME_API FHoudiniEngineRuntime : public IModuleInterface
{
	public:
		DECLARE_MULTICAST_DELEGATE(FOnToolOrPackageChanged)
	
		FHoudiniEngineRuntime();

		//
		// IModuleInterface methods.
		//
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		// Return singleton instance of Houdini Engine Runtime, used internally.
		static FHoudiniEngineRuntime & Get();

		// Return true if singleton instance has been created.
		static bool IsInitialized();

		//
		// Houdini Asset Component registry
		//
		// Ensure that the registered components are all still valid
		void CleanUpRegisteredHoudiniComponents();

		void RegisterHoudiniComponent(UHoudiniAssetComponent* HAC, bool bAllowArchetype=false);

		void UnRegisterHoudiniComponent(UHoudiniAssetComponent* HAC);
		void UnRegisterHoudiniComponent(const int32& ValidIdx);

		bool IsComponentRegistered(UHoudiniAssetComponent* HAC) const;
		int32 GetRegisteredHoudiniComponentCount();
		UHoudiniAssetComponent* GetRegisteredHoudiniComponentAt(const int32& Index);

		virtual TArray<TWeakObjectPtr<UHoudiniAssetComponent>>* GetRegisteredHoudiniComponents() { return &RegisteredHoudiniComponents; };
		
		//
		// Node deletion
		//
		void MarkNodeIdAsPendingDelete(const int32& InNodeId, bool bDeleteParent = false);

		int32 GetNodeIdsPendingDeleteCount();
		int32 GetNodeIdsPendingDeleteAt(const int32& Index);
		void RemoveNodeIdPendingDeleteAt(const int32& Index);

		bool IsParentNodePendingDelete(const int32& NodeId);

		void RemoveParentNodePendingDelete(const int32& NodeId);

		//
		//
		//

		// Returns the folder to be used for temporary cook content
		FString GetDefaultTemporaryCookFolder() const;

		// Returns the defualt folder used for baking
		FString GetDefaultBakeFolder() const;

		void BroadcastToolOrPackageChanged() const { OnToolOrPackageChanged.Broadcast(); }
	
		FOnToolOrPackageChanged& GetOnToolOrPackageChangedEvent() { return OnToolOrPackageChanged; }

	private:

		// Synchronization primitive. 
		FCriticalSection CriticalSection;

		// Singleton instance.
		static FHoudiniEngineRuntime * HoudiniEngineRuntimeInstance;

		// 
		TArray<TWeakObjectPtr<UHoudiniAssetComponent>> RegisteredHoudiniComponents;

		TArray<int32> NodeIdsPendingDelete;

		TArray<int32> NodeIdsParentPendingDelete;

		FOnToolOrPackageChanged OnToolOrPackageChanged;
};