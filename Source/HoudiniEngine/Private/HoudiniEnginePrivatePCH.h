/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */
#pragma once

#define HOUDINI_DEBUG_LOGGING 0

#include "UnrealEd.h"

DECLARE_LOG_CATEGORY_EXTERN( LogHoudiniEngine, Log, All );

#include "IHoudiniEngine.h"
#include "CoreUObject.h"
#include "Engine.h"

// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.

#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniParmFloatComponent.h"
