/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "AssetTypeActions_Base.h"
#include "HoudiniToolsPackageAsset.h"

class UClass;
class UObject;
class UHoudiniAsset;

enum class EHoudiniToolType : uint8;

// TODO: UE5.2 uses Asset Definitions instead of Asset Actions. See AssetDefinitions.h for a conversion guide.

class FAssetTypeActions_HoudiniToolsPackageAsset : public FAssetTypeActions_Base
{	
	public:

		// FAssetTypeActions_Base methods.
		virtual FText GetName() const override;
		virtual FColor GetTypeColor() const override;
		virtual UClass* GetSupportedClass() const override;
		virtual uint32 GetCategories() override;
		
		virtual bool HasActions(const TArray< UObject * > & InObjects) const override;
		virtual void GetActions(const TArray< UObject * > & InObjects, class FMenuBuilder & MenuBuilder) override;

	protected:

		// Handler for reimport option.
		void ExecuteReimport(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> PackageAssets);
		void ExecuteReimportDescriptionOnly(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> PackageAssets);
		void ExecuteImportNewHDAs(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> PackageAssets);

		// Handler for find in explorer option
		void ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniToolsPackageAsset>> PackageAssets);
};
