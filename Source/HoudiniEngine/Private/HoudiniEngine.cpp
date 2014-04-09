// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngine.generated.inl"
#include "HAPI.h"

class FHoudiniEngine : public IHoudiniEngine
{
	/** IModuleInterface implementation */
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;
};

IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine)
DEFINE_LOG_CATEGORY(LogHoudiniEngine);

//extern "C" __declspec(dllimport) int HAPI_IsInitialized();

void FHoudiniEngine::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	HAPI_CookOptions cook_options = HAPI_CookOptions_Create();
	HAPI_Result result = HAPI_Initialize("", "", cook_options, true, -1);
}


void FHoudiniEngine::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	HAPI_Cleanup();

}



