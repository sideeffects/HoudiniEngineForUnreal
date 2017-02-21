/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_UNREAL_SPLINE = 6,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_KEEP_TRANSFORM = 7,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_MULTI_GEO_INPUT = 8,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_GEO_PART_NODE_PATH = 9,
    VER_HOUDINI_PLUGIN_SERIALIZATION_HOUDINI_SPLINE_TO_TRANSFORM = 10,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_UNREAL_SPLINE_RESOLUTION_PER_INPUT = 11,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_CUSTOM_LINKER = 12,  // added custom linker version to archives
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ACTOR_INSTANCING = 13,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_LANDSCAPES = 14,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_UNIT = 15,
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BAKENAME_OVERRIDE = 16,

    // -----<new versions can be added before this line>-------------------------------------------------
    // - this needs to be the last line (see note below)
    VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE_PLUS_ONE,
    VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION = VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE_PLUS_ONE - 1
};

struct FHoudiniCustomSerializationVersion
{
    // The GUID for this custom version number
    const static FGuid GUID;

private:
    FHoudiniCustomSerializationVersion() {}
};
