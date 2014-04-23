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
#include "HoudiniEngine.generated.inl"


//--
const FName HoudiniEngineAppIdentifier = FName(TEXT("HoudiniEngineApp"));


//--
FHoudiniEngineModule::FHoudiniEngineModule()
{

}


//--
FHoudiniEngineModule::~FHoudiniEngineModule()
{

}


//--
void 
FHoudiniEngineModule::StartupModule()
{
#if HOUDINI_ENGINE_LOGGING

	UE_LOG(LogHoudiniEngine, Log, TEXT("FHoudiniEngineModule::StartupModule"));

#endif
}


//--
void 
FHoudiniEngineModule::ShutdownModule()
{
#if HOUDINI_ENGINE_LOGGING

	UE_LOG(LogHoudiniEngine, Log, TEXT("FHoudiniEngineModule::ShutdownModule"));

#endif
}


//--
TSharedPtr<FExtensibilityManager> 
FHoudiniEngineModule::GetMenuExtensibilityManager()
{ 
	return MenuExtensibilityManager; 
}


//--
TSharedPtr<FExtensibilityManager> 
FHoudiniEngineModule::GetToolBarExtensibilityManager()
{ 
	return ToolBarExtensibilityManager; 
}


//--
IMPLEMENT_MODULE(FHoudiniEngineModule, HoudiniEngine);
DEFINE_LOG_CATEGORY(LogHoudiniEngine);
