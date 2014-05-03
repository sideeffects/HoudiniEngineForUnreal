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
	bEditAfterNew = true;

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
UHoudiniAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UHoudiniAsset* HoudiniAsset = ConstructObject<UHoudiniAsset>(Class, InParent, Name, Flags);
	return HoudiniAsset;
}


UObject* 
UHoudiniAssetFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	//UObject* HoudiniAsset = NULL;
	UObject* HoudiniAsset = new(InParent, InName, Flags) UHoudiniAsset(FPostConstructInitializeProperties());

	FEditorDelegates::OnAssetPostImport.Broadcast(this, HoudiniAsset);

	return HoudiniAsset;
}
