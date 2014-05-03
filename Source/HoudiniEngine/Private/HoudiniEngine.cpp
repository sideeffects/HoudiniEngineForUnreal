/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Damian Campeanu
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngine.generated.inl"


const FName FHoudiniEngine::HoudiniEngineAppIdentifier = FName(TEXT("HoudiniEngineApp"));

//--
void
FHoudiniEngine::StartupModule()
{
#if HOUDINI_ENGINE_LOGGING

	UE_LOG(LogHoudiniEngine, Log, TEXT("FHoudiniEngine::StartupModule"));

#endif
}


//--
void
FHoudiniEngine::ShutdownModule()
{
#if HOUDINI_ENGINE_LOGGING

	UE_LOG(LogHoudiniEngine, Log, TEXT("FHoudiniEngine::ShutdownModule"));

#endif
}


IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine);
DEFINE_LOG_CATEGORY(LogHoudiniEngine);
