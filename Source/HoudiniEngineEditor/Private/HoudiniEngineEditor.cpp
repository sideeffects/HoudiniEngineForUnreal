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
FHoudiniEngineEditor::FHoudiniEngineEditor() :
HoudiniEngineAsset(NULL)
{

}


//--
FHoudiniEngineEditor::~FHoudiniEngineEditor()
{

}


//--
UHoudiniEngineAsset*
FHoudiniEngineEditor::GetHoudiniEngineAsset() const
{
	return HoudiniEngineAsset;
}


//--
void
FHoudiniEngineEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(HoudiniEngineAsset);
}


//--
void
FHoudiniEngineEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(TabManager);
}


//--
void
FHoudiniEngineEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);
}


//--
FName
FHoudiniEngineEditor::GetToolkitFName() const
{
	return FName("HoudiniEngineEditor");
}


//--
FText
FHoudiniEngineEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "HoudiniEngine Editor");
}


//--
FString
FHoudiniEngineEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "HoudiniEngineEditor").ToString();
}
