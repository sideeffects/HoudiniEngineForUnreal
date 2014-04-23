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
#define HOUDINI_ENGINE_EDITOR_LOGGING 1
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineEditor, Log, All);
#define LOCTEXT_NAMESPACE "HoudiniEngineEditor"

/** Unreal headers **/
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "AssetRegistryModule.h"
#include "ModuleManager.h"
#include "CoreUObject.h"
#include "Engine.h"

/** HoudiniEngineEditor Class headers **/
#include "HoudiniEngineEditorClasses.h"
#include "HoudiniEngineClasses.h"

/** HoudiniEngineEditor Private headers **/
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorModule.h"
