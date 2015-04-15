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
#include "HoudiniEngineEditor.h"


const FName
FHoudiniEngineEditor::HoudiniEngineEditorAppIdentifier = FName(TEXT("HoudiniEngineEditorApp"));


IMPLEMENT_MODULE(FHoudiniEngineEditor, HoudiniEngineEditor);
DEFINE_LOG_CATEGORY(LogHoudiniEngineEditor);


FHoudiniEngineEditor*
FHoudiniEngineEditor::HoudiniEngineEditorInstance = nullptr;


FHoudiniEngineEditor&
FHoudiniEngineEditor::Get()
{
    return *HoudiniEngineEditorInstance;
}


bool
FHoudiniEngineEditor::IsInitialized()
{
    return FHoudiniEngineEditor::HoudiniEngineEditorInstance != nullptr;
}


void
FHoudiniEngineEditor::StartupModule()
{

}


void
FHoudiniEngineEditor::ShutdownModule()
{
    
}
