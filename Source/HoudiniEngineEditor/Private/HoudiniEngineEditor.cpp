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
#include "HoudiniEngineScheduler.h"


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
	HOUDINI_EDITOR_LOG_MESSAGE(TEXT("Starting the Houdini Engine Editor module."));

	// Load Houdini Engine Runtime module.
	//IHoudiniEngine& HoudiniEngine = FModuleManager::LoadModuleChecked<FHoudiniEngine>("HoudiniEngine").Get();

	HoudiniEngineScheduler = new FHoudiniEngineScheduler();
	HoudiniEngineSchedulerThread = FRunnableThread::Create(HoudiniEngineScheduler, TEXT("HoudiniScheduler"), 0, TPri_Normal);

	// Store the instance.
	FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;
}


void
FHoudiniEngineEditor::ShutdownModule()
{
	HOUDINI_EDITOR_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine Editor module."));

	// Do scheduler and thread clean up.
	if(HoudiniEngineScheduler)
	{
		HoudiniEngineScheduler->Stop();
	}

	if(HoudiniEngineSchedulerThread)
	{
		//HoudiniEngineSchedulerThread->Kill(true);
		HoudiniEngineSchedulerThread->WaitForCompletion();

		delete HoudiniEngineSchedulerThread;
		HoudiniEngineSchedulerThread = nullptr;
	}

	if(HoudiniEngineScheduler)
	{
		delete HoudiniEngineScheduler;
		HoudiniEngineScheduler = nullptr;
	}
}
