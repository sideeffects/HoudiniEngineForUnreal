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
//#include "HoudiniEngineEditor.generated.inl"


//--
const FName HoudiniEngineEditorAppIdentifier = FName(TEXT("HoudiniEngineEditorApp"));


//--
FHoudiniEngineEditorModule::FHoudiniEngineEditorModule()
{

}


//--
FHoudiniEngineEditorModule::~FHoudiniEngineEditorModule()
{

}


//--
void
FHoudiniEngineEditorModule::StartupModule()
{
#if HOUDINI_ENGINE_EDITOR_LOGGING

	UE_LOG(LogHoudiniEngineEditor, Log, TEXT("FHoudiniEngineEditorModule::StartupModule"));

#endif
}


//--
void
FHoudiniEngineEditorModule::ShutdownModule()
{
#if HOUDINI_ENGINE_EDITOR_LOGGING

	UE_LOG(LogHoudiniEngineEditor, Log, TEXT("FHoudiniEngineEditorModule::ShutdownModule"));

#endif
}


//--
TSharedPtr<FExtensibilityManager>
FHoudiniEngineEditorModule::GetMenuExtensibilityManager()
{
	return MenuExtensibilityManager;
}


//--
TSharedPtr<FExtensibilityManager>
FHoudiniEngineEditorModule::GetToolBarExtensibilityManager()
{
	return ToolBarExtensibilityManager;
}


//--
IMPLEMENT_MODULE(FHoudiniEngineEditorModule, HoudiniEngineEditor);
DEFINE_LOG_CATEGORY(LogHoudiniEngineEditor);
