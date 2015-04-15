/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once

#include "ModuleManager.h"
#include "EngineModule.h"
#include "Engine.h"

#include "HAPI.h"
#include "HAPI_Version.h"

/** Other definitions. **/
#define HOUDINI_ENGINE_LOGGING 1
DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngineEditor, Log, All);
#define LOCTEXT_NAMESPACE "HoudiniEngineEditor"

/** Definitions coming from UBT. **/
#ifndef HOUDINI_ENGINE_HFS_PATH
#define HOUDINI_ENGINE_HFS_PATH ""
#endif
