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
#pragma once

/** Unreal headers. **/
#include "CoreUObject.h"
#include "Engine.h"
#include "AssetTypeActions_Base.h"
#include "ModuleManager.h"
#include "UnrealEd.h"

/** HoudiniEngine Class headers. **/
#include "HoudiniEngineClasses.h"

/** HoudiniEngine Private headers. **/
#include "HoudiniEngine.h"
#include "HoudiniAssetTypeActions.h"

/** Other definitions. **/
#define HOUDINI_ENGINE_LOGGING 1
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All);
#define LOCTEXT_NAMESPACE "MonkeyPatch"
