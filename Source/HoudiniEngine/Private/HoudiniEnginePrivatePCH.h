// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

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
