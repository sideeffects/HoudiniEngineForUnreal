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

	/*
	UObject* HoudiniAsset = NULL;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Check if HAPI has been initialized.
	Result = HAPI_IsInitialized();
	if(HAPI_RESULT_SUCCESS == Result)
	{
		// Load the Houdini Engine asset library (OTL). This does not instantiate anything inside the Houdini scene.
		HAPI_AssetLibraryId AssetLibraryId = 0;
		Result = HAPI_LoadAssetLibraryFromMemory(reinterpret_cast<const char*>(Buffer), BufferEnd - Buffer, &AssetLibraryId);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We failed loading Houdini Engine asset library from memory, report.

			
			//Broadcast post import notification and return.
			FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);
			return HoudiniAsset;
		}

		// Retrieve number of assets contained in this library.
		int32 AssetCount = 0;
		Result = HAPI_GetAvailableAssetCount(AssetLibraryId, &AssetCount);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We failed to retrieve number of assets, report.


			//Broadcast post import notification and return.
			FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);
			return HoudiniAsset;
		}

		// Retrieve available assets. 
		std::vector<int> AssetNames;
		AssetNames.reserve(AssetCount);
		Result = HAPI_GetAvailableAssets(AssetLibraryId, &AssetNames[0], AssetCount);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			// We failed to retrieve available assets, report.

			//Broadcast post import notification and return.
			FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);
			return HoudiniAsset;
		}

		// For now we will load first asset only.
		int32 AssetFirstNameLength = 0;
		Result = HAPI_GetStringBufLength(AssetNames[0], &AssetFirstNameLength);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			// We failed to retrieve length of first asset's name, report.

			//Broadcast post import notification and return.
			FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);
			return HoudiniAsset;
		}

		// Retrieve name of first asset.
		std::vector<char> AssetFirstName;
		AssetFirstName.reserve(AssetFirstNameLength + 1);
		AssetFirstName[AssetFirstNameLength] = '\0';
		Result = HAPI_GetString(AssetNames[0], &AssetFirstName[0], AssetFirstNameLength);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			// We failed to retrieve asset name, report.
		
			//Broadcast post import notification and return.
			FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);
			return HoudiniAsset;
		}

		// Instantiate the asset.
		HAPI_AssetId AssetId = -1;
		Result = HAPI_InstantiateAsset(&AssetFirstName[0], true, &AssetId);
		if (HAPI_RESULT_SUCCESS != Result)
		{
			// We failed asset instantiation, report.

			//Broadcast post import notification and return.
			FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);
			return HoudiniAsset;
		}
		
		// Construct an empty Houdini Engine asset.
		HoudiniAsset = new(InParent, InName, Flags) UHoudiniAsset(FPostConstructInitializeProperties(), (const char*) &AssetFirstName[0], AssetId);	
	}
	else
	{
		// HAPI has not been initialized.
		HOUDINI_LOG_ERROR(TEXT(" Cannot import Houdini Engine asset, HAPI has not been initialized."));
	}
	*/

	// Create new Houdini asset object.
	UHoudiniAsset* HoudiniAsset = new(InParent, InName, Flags) UHoudiniAsset(FPostConstructInitializeProperties());

	// Initialize data.
	if(HoudiniAsset && !HoudiniAsset->InitializeStorage(Buffer, BufferEnd))
	{
		delete HoudiniAsset;
		HoudiniAsset = NULL;
	}

	// Broadcast notification that the new asset has been imported.
	FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);

	return HoudiniAsset;
}
