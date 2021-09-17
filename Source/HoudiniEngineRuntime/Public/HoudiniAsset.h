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

#include "UObject/Object.h"
#include "HoudiniAsset.generated.h"

class UAssetImportData;

UCLASS(BlueprintType, EditInlineNew, config = Engine)
class HOUDINIENGINERUNTIME_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

	public:

		// UOBject functions
		virtual void FinishDestroy() override;
		virtual void Serialize(FArchive & Ar) override;
		virtual void GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const override;

		// Creates and initialize this asset from a given buffer / file.
		void CreateAsset(const uint8 * BufferStart, const uint8 * BufferEnd, const FString & InFileName);

		// Return buffer containing the raw Houdini OTL data.
		const uint8* GetAssetBytes() const;

		// Return path of the corresponding OTL/HDA file.
		const FString& GetAssetFileName() const;

		// Return the size in bytes of raw Houdini OTL data.
		uint32 GetAssetBytesCount() const;

		// Return true if this asset is a limited commercial asset.
		bool IsAssetLimitedCommercial() const;

		// Return true if this asset is a non commercial asset.
		bool IsAssetNonCommercial() const;

		// Return true if this asset is an expanded HDA (HDA dir)
		bool IsExpandedHDA() const;

	private:
		// Used to load old (version1) versions of HoudiniAssets
		void SerializeLegacy(FArchive & Ar);

	public:

		// Source filename of the OTL.
		UPROPERTY()
		FString AssetFileName;
		
#if WITH_EDITORONLY_DATA
		// Importing data and options used for this Houdini asset.
		UPROPERTY(Category = ImportSettings, VisibleAnywhere, Instanced)
		UAssetImportData * AssetImportData;
#endif

	private:

		// Buffer containing the raw HDA OTL data.
		UPROPERTY()
		TArray<uint8> AssetBytes;

		// Size in bytes of the raw HDA data.
		UPROPERTY()
		uint32 AssetBytesCount;

		// Indicates if this is a limited commercial asset.
		UPROPERTY()
		bool bAssetLimitedCommercial;

		// Indicates if this is a non-commercial license asset.
		UPROPERTY()
		bool bAssetNonCommercial;

		// Indicates if this is an expanded HDA file
		UPROPERTY()
		bool bAssetExpanded;
};