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
	HOUDINI_LOG_MESSAGE(TEXT("UHoudiniAssetFactory is creating an asset, Factory = 0x%0.8p, Parent = 0x%0.8p"), this, InParent);

	// Broadcast notification that a new asset is being imported.
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	// Create a new asset.
	UHoudiniAsset* HoudiniAsset = new(InParent, InName, Flags) UHoudiniAsset(FPostConstructInitializeProperties(), Buffer, BufferEnd, UFactory::CurrentFilename);

	// Broadcast notification that the new asset has been imported.
	FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);

	return HoudiniAsset;
}
