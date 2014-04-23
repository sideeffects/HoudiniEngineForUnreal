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

#pragma once
#include "IHoudiniEngineModule.h"
#include "ModuleManager.h"

class FHoudiniEngineModule : public IHoudiniEngineModule
{
public:
	
	FHoudiniEngineModule();
	virtual ~FHoudiniEngineModule();

public:

	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;

public:

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager();
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager();

private:

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};
