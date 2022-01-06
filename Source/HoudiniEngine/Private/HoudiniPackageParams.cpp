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

#include "HoudiniPackageParams.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniStringResolver.h"

#include "PackageTools.h"
#include "ObjectTools.h"
#include "Engine/StaticMesh.h"
#include "UObject/MetaData.h"

#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"

//
FHoudiniPackageParams::FHoudiniPackageParams()
{
	PackageMode = EPackageMode::CookToTemp;
	ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;

	TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
	BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();

	OuterPackage = nullptr;
	ObjectName = FString();
	HoudiniAssetName = FString();
	HoudiniAssetActorName = FString();

	ObjectId = 0;
	GeoId = 0;
	PartId = 0;
	SplitStr = 0;

	ComponentGUID.Invalidate();

	PDGTOPNetworkName.Empty();
	PDGTOPNodeName.Empty();
	PDGWorkItemIndex = INDEX_NONE;
	PDGWorkResultArrayIndex = INDEX_NONE;
}


//
FHoudiniPackageParams::~FHoudiniPackageParams()
{


}


// Returns the object flags corresponding to the current package mode
EObjectFlags 
FHoudiniPackageParams::GetObjectFlags() const 
{
	if (PackageMode == EPackageMode::CookToTemp)
		return RF_Public | RF_Standalone;
	else if (PackageMode == EPackageMode::Bake)
		return RF_Public | RF_Standalone;
	else
		return RF_NoFlags;
}

FString
FHoudiniPackageParams::GetPackageName() const
{
	if (!ObjectName.IsEmpty())
		return ObjectName;

	// If we have PDG infos, generate a name including them
	if (!PDGTOPNetworkName.IsEmpty() && !PDGTOPNodeName.IsEmpty() && PDGWorkResultArrayIndex >= 0)
	{
		return FString::Printf(
			TEXT("%s_%s_%s_%d_%d_%s"),
			*HoudiniAssetName, *PDGTOPNetworkName, *PDGTOPNodeName, PDGWorkResultArrayIndex, PartId, *SplitStr);
	}
	else
	{
		// Generate an object name using the HGPO IDs and the HDA name
		return FString::Printf(TEXT("%s_%d_%d_%d_%s"), *HoudiniAssetName, ObjectId, GeoId, PartId, *SplitStr);
	}
}

FString
FHoudiniPackageParams::GetPackagePath() const
{
	FString PackagePath = FString();
	switch (PackageMode)
	{
		case EPackageMode::CookToLevel_Invalid:
		case EPackageMode::CookToTemp:
		{
			// Temporary Folder
			PackagePath = TempCookFolder;
			//  Add a subdir for the HDA
			if (!HoudiniAssetName.IsEmpty())
				PackagePath += TEXT("/") + HoudiniAssetName;
			// Add a subdir using the owner component GUID if possible
			if (ComponentGUID.IsValid())
				PackagePath += TEXT("/") + ComponentGUID.ToString().Left(PACKAGE_GUID_LENGTH);
		}
		break;

		case EPackageMode::Bake:
		{
			PackagePath = BakeFolder;
		}
		break;
	}

	return PackagePath;
}

bool
FHoudiniPackageParams::GetBakeCounterFromBakedAsset(const UObject* InAsset, int32& OutBakeCounter)
{
	OutBakeCounter = 0;

	if (!IsValid(InAsset))
		return false;

	UPackage* Package = InAsset->GetPackage();
	// const FString PackagePathName = Package->GetPathName();
	// FString PackagePathNamePrefix;
	// FString BakeCountOrGUID;
	// if (!GetPackageNameWithoutBakeCounterOrGUIDSuffix(PackagePathName, PackagePathNamePrefix, BakeCountOrGUID))
	// 	PackagePathNamePrefix = PackagePathName;
	// 	
	// const FString ThisPackageNameBase = UPackageTools::SanitizePackageName(GetPackagePath() + TEXT("/") + GetPackageName());
	// if (!PackagePathNamePrefix.Equals(ThisPackageNameBase))
	// 	return false;
	//
	// // Not a valid counter suffix, could be a GUID suffix. Return true since the prefixes match.
	// if (BakeCountOrGUID.IsNumeric())
	// 	OutBakeCounter = FCString::Atoi(*BakeCountOrGUID);
	//
	// return true;

	if (!IsValid(Package))
		return false;

	UMetaData* MetaData = Package->GetMetaData();
	if (!IsValid(MetaData))
		return false;

	if (MetaData->RootMetaDataMap.Contains(HAPI_UNREAL_PACKAGE_META_BAKE_COUNTER))
	{
		FString BakeCounterStr = MetaData->RootMetaDataMap.FindChecked(HAPI_UNREAL_PACKAGE_META_BAKE_COUNTER);
		BakeCounterStr.TrimStartAndEndInline();
		if (BakeCounterStr.IsNumeric())
		{
			OutBakeCounter = FCString::Atoi(*BakeCounterStr);
			return true;
		}
	}

	return false;
}

bool
FHoudiniPackageParams::GetGUIDFromTempAsset(const UObject* InAsset, FString& OutGUID)
{
	if (!InAsset)
		return false;

	UPackage* Package = InAsset->GetPackage();
	if (!IsValid(Package))
		return false;

	UMetaData* MetaData = Package->GetMetaData();
	if (!IsValid(MetaData))
		return false;

	if (MetaData->RootMetaDataMap.Contains(HAPI_UNREAL_PACKAGE_META_TEMP_GUID))
	{
		OutGUID = MetaData->RootMetaDataMap.FindChecked(HAPI_UNREAL_PACKAGE_META_TEMP_GUID);
		OutGUID.TrimStartAndEndInline();
		if (!OutGUID.IsEmpty())
			return true;
	}

	return false;
}

FString
FHoudiniPackageParams::GetPackageNameExcludingBakeCounter(const UObject* InAsset)
{
	if (!IsValid(InAsset))
		return FString();

	UPackage* Package = InAsset->GetPackage();
	if (!IsValid(Package))
		return FString();
	
	FString PackageName = FPaths::GetCleanFilename(Package->GetPathName());
	int32 BakeCounter = 0;
	if (GetBakeCounterFromBakedAsset(InAsset, BakeCounter))
	{
		const FString BakeCounterSuffix = FString::Printf(TEXT("_%d"), BakeCounter);
		if (PackageName.EndsWith(BakeCounterSuffix))
			PackageName = PackageName.Mid(0, PackageName.Len() - BakeCounterSuffix.Len());
	}

	return PackageName;
}

bool
FHoudiniPackageParams::MatchesPackagePathNameExcludingBakeCounter(const UObject* InAsset) const
{
	if (!IsValid(InAsset))
		return false;

	UPackage* Package = InAsset->GetPackage();
	if (!IsValid(Package))
		return false;
	
	const FString InAssetPackagePathName = FPaths::GetPath(Package->GetPathName()) + TEXT("/") + GetPackageNameExcludingBakeCounter(InAsset);
	const FString ThisPackagePathName = UPackageTools::SanitizePackageName(GetPackagePath() + TEXT("/") + GetPackageName());
	return InAssetPackagePathName.Equals(ThisPackagePathName);
}

bool
FHoudiniPackageParams::HasMatchingPackageDirectories(UObject const* const InAsset) const
{
	if (!IsValid(InAsset))
		return false;

	UPackage const* const Package = InAsset->GetPackage();
	if (!IsValid(Package))
		return false;

	const FString InAssetPackageDirectory = FPaths::GetPath(Package->GetPathName());
	const FString ThisPackageDirectory = UPackageTools::SanitizePackageName(GetPackagePath());
	return InAssetPackageDirectory.Equals(ThisPackageDirectory);
}

FString
FHoudiniPackageParams::GetPackageNameExcludingGUID(const UObject* InAsset)
{
	if (!IsValid(InAsset))
		return FString();

	UPackage* Package = InAsset->GetPackage();
	if (!IsValid(Package))
		return FString();
	
	FString PackageName = FPaths::GetCleanFilename(Package->GetPathName());
	FString GUIDStr;
	if (GetGUIDFromTempAsset(InAsset, GUIDStr))
	{
		if (PackageName.EndsWith(TEXT("_") + GUIDStr))
			PackageName = PackageName.Mid(0, PackageName.Len() - GUIDStr.Len() - 1);
	}

	return PackageName;
}

UPackage*
FHoudiniPackageParams::CreatePackageForObject(FString& OutPackageName, int32 InBakeCounterStart) const
{
	// GUID/counter used to differentiate with existing package
	int32 BakeCounter = InBakeCounterStart;
	FGuid CurrentGuid = FGuid::NewGuid();

	// Get the appropriate package path/name for this object
	FString PackageName = GetPackageName();
	FString PackagePath = GetPackagePath();
	   
	// Iterate until we find a suitable name for the package
	UPackage * NewPackage = nullptr;
	while (true)
	{
		OutPackageName = PackageName;

		// Append the Bake guid/counter to the object name if needed
		if (BakeCounter > 0)
		{
			OutPackageName += (PackageMode == EPackageMode::Bake)
				? TEXT("_") + FString::FromInt(BakeCounter)
				: TEXT("_") + CurrentGuid.ToString().Left(PACKAGE_GUID_LENGTH);
		}

		// Build the final package name
		FString FinalPackageName = PackagePath + TEXT("/") + OutPackageName;
		// Sanitize package name.
		FinalPackageName = UPackageTools::SanitizePackageName(FinalPackageName);

		// If we are set to create new assets, check if a package named similarly already exists
		if (ReplaceMode == EPackageReplaceMode::CreateNewAssets)
		{
			UPackage* FoundPackage = FindPackage(nullptr, *FinalPackageName);
			if (FoundPackage == nullptr)
			{
				// Package might not be in memory, check if it exists on disk
				FoundPackage = LoadPackage(nullptr, *FinalPackageName, LOAD_Verify | LOAD_NoWarn);
			}
			
			if (IsValid(FoundPackage))
			{
				// we need to generate a new name for it
				CurrentGuid = FGuid::NewGuid();
				BakeCounter++;
				continue;
			}
		}

		// Create actual package.
		NewPackage = CreatePackage(*FinalPackageName);
		if (IsValid(NewPackage))
		{
			// Record bake counter / temp GUID in package metadata
			UMetaData* MetaData = NewPackage->GetMetaData();
			if (IsValid(MetaData))
			{
				if (PackageMode == EPackageMode::Bake)
				{
					// HOUDINI_LOG_MESSAGE(TEXT("Recording bake counter in package metadata: %d"), BakeCounter);
					MetaData->RootMetaDataMap.Add(
						HAPI_UNREAL_PACKAGE_META_BAKE_COUNTER, FString::FromInt(BakeCounter));
				}
				else if (CurrentGuid.IsValid())
				{
					const FString GuidStr = CurrentGuid.ToString().Left(PACKAGE_GUID_LENGTH);
					// HOUDINI_LOG_MESSAGE(TEXT("Recording temp guid in package metadata: %s"), *GuidStr);
					MetaData->RootMetaDataMap.Add(HAPI_UNREAL_PACKAGE_META_TEMP_GUID, GuidStr);
				}

				// Store the Component GUID that generated the package, as well as the package name and meta key
				// indicating that the package was generated by Houdini
				{
					const FString ComponentGuidStr = ComponentGUID.ToString();
					MetaData->RootMetaDataMap.Add(HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID, ComponentGuidStr);

					MetaData->RootMetaDataMap.Add(HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
					MetaData->RootMetaDataMap.Add(HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *OutPackageName);
				}
			}
		}
		
		break;
	}

	return NewPackage;
}


// Fixes link error with the template function under
void TemplateFixer()
{
	FHoudiniPackageParams PP;
	UStaticMesh* SM = PP.CreateObjectAndPackage<UStaticMesh>();
	UHoudiniStaticMesh* HSM = PP.CreateObjectAndPackage<UHoudiniStaticMesh>();
	UGeometryCollection* GC = PP.CreateObjectAndPackage<UGeometryCollection>();
	//UMaterial* Mat = PP.CreateObjectAndPackage<UMaterial>();
	//UTexture2D* Text = PP.CreateObjectAndPackage<UTexture2D>();
}

template<typename T>
T* FHoudiniPackageParams::CreateObjectAndPackage()
{
	// Create the package for the object
	FString NewObjectName;
	UPackage* Package = CreatePackageForObject(NewObjectName);
	if (!IsValid(Package))
		return nullptr;

	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(NewObjectName);

	T* ExistingTypedObject = FindObject<T>(Package, *NewObjectName);
	UObject* ExistingObject = FindObject<UObject>(Package, *NewObjectName);

	if (IsValid(ExistingTypedObject))
	{
		// An object of the appropriate type already exists, update it!
		ExistingTypedObject->PreEditChange(nullptr);
	}
	else if (ExistingObject != nullptr)
	{
		// Replacing an object of a different type, Delete it first.
		const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);
		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Create a package for each mesh
			Package = CreatePackageForObject(NewObjectName);
			if (!IsValid(Package))
				return nullptr;
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}

	T* const CreatedObject = NewObject<T>(Package, FName(*SanitizedObjectName), GetObjectFlags());

	// Add meta information to the new object in the package.
	if (IsValid(CreatedObject))
	{
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			Package, CreatedObject, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			Package, CreatedObject, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *NewObjectName);
		
		const FString ComponentGuidStr = ComponentGUID.ToString();
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
			Package, CreatedObject, HAPI_UNREAL_PACKAGE_META_COMPONENT_GUID, ComponentGuidStr);
	}

	return CreatedObject;
}

