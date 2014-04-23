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

#include "HoudiniEngineEditorPrivatePCH.h"


//--
UHoudiniEngineAssetFactory::UHoudiniEngineAssetFactory(const class FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{
	// This factory is repsonsible for manufacturing HoudiniEngine assets.
	SupportedClass = UHoudiniEngineAsset::StaticClass();

	// This factory creates new objects from scratch.
	bCreateNew = true;

	// This factory will open the editor for each new object.
	bEditAfterNew = true;

	// This factory will import objects from files.
	bEditorImport = true;

	// Add supported formats.
	Formats.Add(TEXT("otl;Houdini Engine Asset"));
}


//--
FText 
UHoudiniEngineAssetFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniEngineAssetFactoryDescription", "OTL Houdini Engine Asset");
}


//--
UObject* 
UHoudiniEngineAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UHoudiniEngineAsset* HoudiniEngineAsset = ConstructObject<UHoudiniEngineAsset>(Class, InParent, Name, Flags);
	return HoudiniEngineAsset;
}
