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
// Serialization of parameter name map.
#define VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP 2
// Serialization of instancer material, if it is available.
#define VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_MATERIAL_NAME 1
// Serialization of attribute instancer material, if it is available.
#define VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_ATTRIBUTE_MATERIAL_NAME 2
// Landscape serialization in asset inputs.
#define VER_HOUDINI_ENGINE_PARAM_LANDSCAPE_INPUT 1
// Asset instance member.
#define VER_HOUDINI_ENGINE_PARAM_ASSET_INSTANCE_MEMBER 2
// World Outliner inputs.
#define VER_HOUDINI_ENGINE_PARAM_WORLD_OUTLINER_INPUT 3

enum EHoudiniPluginSerializationVersion
{
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE = 5,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_UNREAL_SPLINE,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_KEEP_TRANSFORM,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_MULTI_GEO_INPUT,

    // -----<new versions can be added before this line>-------------------------------------------------
    // - this needs to be the last line (see note below)
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE_PLUS_ONE,
    VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION = VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE_PLUS_ONE - 1
};
