/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"
#include <vector>


UHoudiniAssetFactory::UHoudiniAssetFactory(const class FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = UHoudiniAsset::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Add supported formats.
	Formats.Add(TEXT("otl;Houdini Engine Asset"));
}


FText
UHoudiniAssetFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniAssetFactoryDescription", "Houdini Engine Asset");
}


UObject* 
UHoudiniAssetFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	// Broadcast notification that a new asset is being imported.
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	UHoudiniAsset* HoudiniAsset = nullptr;

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("CreateAsset failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));
		return HoudiniAsset;
	}

	// Calculate buffer size.
	uint32 AssetBytesCount = BufferEnd - Buffer;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Load asset library from given buffer.
	HAPI_AssetLibraryId AssetLibraryId = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_LoadAssetLibraryFromMemory(reinterpret_cast<const char*>(Buffer), AssetBytesCount, &AssetLibraryId), HoudiniAsset);

	// Retrieve number of assets contained in this library.
	int32 AssetCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssetCount(AssetLibraryId, &AssetCount), HoudiniAsset);

	// Retrieve available assets. 
	std::vector<int> AssetNames;
	AssetNames.reserve(AssetCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssets(AssetLibraryId, &AssetNames[0], AssetCount), HoudiniAsset);

	// If we have assets, instantiate first one.
	std::string AssetName;
	if(AssetCount && FHoudiniEngineUtils::GetAssetName(AssetNames[0], AssetName))
	{
		HAPI_AssetId AssetId = -1;
		bool CookOnLoad = true;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_InstantiateAsset(&AssetName[0], CookOnLoad, &AssetId), HoudiniAsset);

		// Create a Houdini asset and perform initialization.
		HoudiniAsset = new(InParent, InName, Flags) UHoudiniAsset(FPostConstructInitializeProperties());
		if(HoudiniAsset && !HoudiniAsset->InitializeAsset(AssetId, ANSI_TO_TCHAR(AssetName.c_str()), Buffer, BufferEnd))
		{
			HoudiniAsset->MarkPendingKill();
			HoudiniAsset = nullptr;
		}
	}

	// Broadcast notification that the new asset has been imported.
	FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);

	return HoudiniAsset;
}
