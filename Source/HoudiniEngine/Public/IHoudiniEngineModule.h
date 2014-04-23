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
#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"

extern const FName HoudiniEngineAppIdentifier;

class IHoudiniEngineModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:

	//virtual TSharedRef<IHoudiniEngineEditor> CreateHoudiniEngineEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UFont* Font) = 0;
};
