/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Chris Grebeldinger
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

// Deprecated per-class versions used to load old files
//
#define VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP 2

enum EHoudiniPluginSerializationVersion
{
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE = 5,

    // -----<new versions can be added before this line>-------------------------------------------------
    // - this needs to be the last line (see note below)
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE_PLUS_ONE,
    VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION = VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE_PLUS_ONE - 1
};
