/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 */

#ifndef __HAPI_COMMON_h__
#define __HAPI_COMMON_h__

#include "HAPI_API.h"

/////////////////////////////////////////////////////////////////////////////
// Defines

#define HAPI_POSITION_VECTOR_SIZE           3
#define HAPI_SCALE_VECTOR_SIZE              3
#define HAPI_SHEAR_VECTOR_SIZE              3
#define HAPI_NORMAL_VECTOR_SIZE             3
#define HAPI_QUATERNION_VECTOR_SIZE         4
#define HAPI_EULER_VECTOR_SIZE              3
#define HAPI_UV_VECTOR_SIZE                 2
#define HAPI_COLOR_VECTOR_SIZE              4
#define HAPI_CV_VECTOR_SIZE                 4

#define HAPI_PRIM_MIN_VERTEX_COUNT          1
#define HAPI_PRIM_MAX_VERTEX_COUNT          16

#define HAPI_INVALID_PARM_ID                -1

/// Common Default Attributes' Names
/// @{
#define HAPI_ATTRIB_POSITION                "P"
#define HAPI_ATTRIB_UV                      "uv"
#define HAPI_ATTRIB_UV2                     "uv2"
#define HAPI_ATTRIB_NORMAL                  "N"
#define HAPI_ATTRIB_TANGENT                 "tangentu"
#define HAPI_ATTRIB_TANGENT2                "tangentv"
#define HAPI_ATTRIB_COLOR                   "Cd"
#define HAPI_ATTRIB_NAME                    "name"
#define HAPI_ATTRIB_INSTANCE                "instance"
#define HAPI_ATTRIB_ROT                     "rot"
#define HAPI_ATTRIB_SCALE                   "scale"
/// @}

/// This is the name of the primitive group created from all the primitives
/// that are not in any user-defined group. This way, when you put all the
/// groups together you cover the entire mesh. This is important for some
/// clients where the mesh has to be defined in terms of submeshes that cover
/// the entire original mesh.
#define HAPI_UNGROUPED_GROUP_NAME           "__ungrouped_group"

/// Common image file format names (to use with the material extract APIs).
/// Note that you may still want to check if they are supported via
/// HAPI_GetSupportedImageFileFormats() since all formats are loaded
/// dynamically by Houdini on-demand so just because these formats are defined
/// here doesn't mean they are supported in your instance.
/// @{
#define HAPI_RAW_FORMAT_NAME                "HAPI_RAW" // HAPI-only Raw Format
#define HAPI_PNG_FORMAT_NAME                "PNG"
#define HAPI_JPEG_FORMAT_NAME               "JPEG"
#define HAPI_BMP_FORMAT_NAME                "Bitmap"
#define HAPI_TIFF_FORMAT_NAME               "TIFF"
#define HAPI_TGA_FORMAT_NAME                "Targa"
/// @}

/// Default image file format's name - used when the image generated and has
/// no "original" file format and the user does not specify a format to
/// convert to.
#define HAPI_DEFAULT_IMAGE_FORMAT_NAME      HAPI_PNG_FORMAT_NAME

/// Name of subnet OBJ node containing the global nodes.
#define HAPI_GLOBAL_NODES_NODE_NAME         "GlobalNodes"

/// Environment variables.
#define HAPI_ENV_HIP                        "HIP"
#define HAPI_ENV_JOB                        "JOB"
#define HAPI_ENV_CLIENT_NAME                "HAPI_CLIENT_NAME"

/// [HAPI_CACHE]
/// Common cache names. You can see these same cache names in the
/// Cache Manager window in Houdini (Windows > Cache Manager).
#define HAPI_CACHE_COP_COOK                 "COP Cook Cache"
#define HAPI_CACHE_COP_FLIPBOOK             "COP Flipbook Cache"
#define HAPI_CACHE_IMAGE                    "Image Cache"
#define HAPI_CACHE_OBJ                      "Object Transform Cache"
#define HAPI_CACHE_GL_TEXTURE               "OpenGL Texture Cache"
#define HAPI_CACHE_GL_VERTEX                "OpenGL Vertex Cache"
#define HAPI_CACHE_SOP                      "SOP Cache"
#define HAPI_CACHE_VEX                      "VEX File Cache"
/// [HAPI_CACHE]

/// [HAPI_InputCurve]
/// HAPI input curve attribute
#define HAPI_ATTRIB_INPUT_CURVE_COORDS                "hapi_input_curve_coords"
/// [HAPI_InputCurve]

// Make sure our enums and structs are usable without those keywords, as-is,
// in C.
#ifdef __cplusplus
    #define HAPI_C_ENUM_TYPEDEF( enum_name )
    #define HAPI_C_STRUCT_TYPEDEF( struct_name )
#else
    #define HAPI_C_ENUM_TYPEDEF( enum_name ) \
        typedef enum enum_name enum_name;
    #define HAPI_C_STRUCT_TYPEDEF( struct_name ) \
        typedef struct struct_name struct_name;
#endif // __cplusplus

/////////////////////////////////////////////////////////////////////////////
// Typedefs

// C has no bool.
#ifdef __cplusplus
    typedef bool HAPI_Bool;
#else
    typedef char HAPI_Bool;
#endif // __cplusplus

// x-bit Integers
// Thrift doesn't support unsigned integers, so we cast it as a 16-bit int, but only
// for automated code generation
#ifdef HAPI_AUTOGEN
    typedef signed char int8_t;
    typedef short int16_t;
    typedef long long int64_t;
    typedef short HAPI_UInt8; // 16-bit type for thrift
#else
    #include <stdint.h>
    #ifdef HAPI_THRIFT_ABI
        typedef int16_t HAPI_UInt8; 
    #else
        typedef uint8_t HAPI_UInt8;
        HAPI_STATIC_ASSERT(sizeof(HAPI_UInt8) == 1, unsupported_size_of_uint8);
    #endif
#endif

typedef int8_t HAPI_Int8;
HAPI_STATIC_ASSERT(sizeof(HAPI_Int8) == 1, unsupported_size_of_int8);
typedef int16_t HAPI_Int16;
HAPI_STATIC_ASSERT(sizeof(HAPI_Int16) == 2, unsupported_size_of_int16);
typedef int64_t HAPI_Int64;
HAPI_STATIC_ASSERT(sizeof(HAPI_Int64) == 8, unsupported_size_of_long);


// The process id has to be uint on Windows and int on any other platform.
#if ( defined _WIN32 || defined WIN32 )
    typedef unsigned int HAPI_ProcessId;
#else
    typedef int HAPI_ProcessId;
#endif

/// Has to be 64-bit.
typedef HAPI_Int64 HAPI_SessionId;

/// Use this with HAPI_GetString() to get the value.
/// See @ref HAPI_Fundamentals_Strings.
typedef int HAPI_StringHandle;

typedef int HAPI_AssetLibraryId;

/// See @ref HAPI_Nodes_Basics.
typedef int HAPI_NodeId;

/// Either get this from the ::HAPI_ParmInfo or ::HAPI_GetParmIdFromName().
/// See @ref HAPI_Parameters.
typedef int HAPI_ParmId;

/// Use this with ::HAPI_GetPartInfo().
/// See @ref HAPI_Parts.
typedef int HAPI_PartId;

/// Use this with PDG functions
typedef int HAPI_PDG_WorkitemId;

/// Use this with PDG functions
typedef int HAPI_PDG_GraphContextId;

/// When we load a HIP file, we associate a HIP file ID with the created nodes
/// so that they can be looked up later
typedef int HAPI_HIPFileId;

/////////////////////////////////////////////////////////////////////////////
// Enums

enum HAPI_License
{
    HAPI_LICENSE_NONE,
    HAPI_LICENSE_HOUDINI_ENGINE,
    HAPI_LICENSE_HOUDINI,
    HAPI_LICENSE_HOUDINI_FX,
    HAPI_LICENSE_HOUDINI_ENGINE_INDIE,
    HAPI_LICENSE_HOUDINI_INDIE,
    HAPI_LICENSE_HOUDINI_ENGINE_UNITY_UNREAL,
    HAPI_LICENSE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_License )

enum HAPI_StatusType
{
    HAPI_STATUS_CALL_RESULT,
    HAPI_STATUS_COOK_RESULT,
    HAPI_STATUS_COOK_STATE,
    HAPI_STATUS_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_StatusType )

enum HAPI_StatusVerbosity
{
    HAPI_STATUSVERBOSITY_0,
    HAPI_STATUSVERBOSITY_1,
    HAPI_STATUSVERBOSITY_2,

    /// Used for Results.  Equivalent to ::HAPI_STATUSVERBOSITY_2
    HAPI_STATUSVERBOSITY_ALL = HAPI_STATUSVERBOSITY_2,
    /// Used for Results.  Equivalent to ::HAPI_STATUSVERBOSITY_0
    HAPI_STATUSVERBOSITY_ERRORS = HAPI_STATUSVERBOSITY_0,
    /// Used for Results.  Equivalent to ::HAPI_STATUSVERBOSITY_1
    HAPI_STATUSVERBOSITY_WARNINGS = HAPI_STATUSVERBOSITY_1,
    /// Used for Results.  Equivalent to ::HAPI_STATUSVERBOSITY_2
    HAPI_STATUSVERBOSITY_MESSAGES = HAPI_STATUSVERBOSITY_2,
};
HAPI_C_ENUM_TYPEDEF( HAPI_StatusVerbosity )

enum HAPI_Result
{
    HAPI_RESULT_SUCCESS                                 = 0,
    HAPI_RESULT_FAILURE                                 = 1,
    HAPI_RESULT_ALREADY_INITIALIZED                     = 2,
    HAPI_RESULT_NOT_INITIALIZED                         = 3,
    HAPI_RESULT_CANT_LOADFILE                           = 4,
    HAPI_RESULT_PARM_SET_FAILED                         = 5,
    HAPI_RESULT_INVALID_ARGUMENT                        = 6,
    HAPI_RESULT_CANT_LOAD_GEO                           = 7,
    HAPI_RESULT_CANT_GENERATE_PRESET                    = 8,
    HAPI_RESULT_CANT_LOAD_PRESET                        = 9,
    HAPI_RESULT_ASSET_DEF_ALREADY_LOADED                = 10,

    HAPI_RESULT_NO_LICENSE_FOUND                        = 110,
    HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND             = 120,
    HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE      = 130,
    HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE     = 140,
    HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE      = 150,
    HAPI_RESULT_DISALLOWED_HENGINEINDIE_W_3PARTY_PLUGIN = 160,

    HAPI_RESULT_ASSET_INVALID                           = 200,
    HAPI_RESULT_NODE_INVALID                            = 210,

    HAPI_RESULT_USER_INTERRUPTED                        = 300,

    HAPI_RESULT_INVALID_SESSION                         = 400
};
HAPI_C_ENUM_TYPEDEF( HAPI_Result )

enum HAPI_ErrorCode
{
    HAPI_ERRORCODE_ASSET_DEF_NOT_FOUND                  = 1 << 0,
    HAPI_ERRORCODE_PYTHON_NODE_ERROR                    = 1 << 1
};
HAPI_C_ENUM_TYPEDEF( HAPI_ErrorCode )
typedef int HAPI_ErrorCodeBits;

enum HAPI_SessionType
{
    HAPI_SESSION_INPROCESS,
    HAPI_SESSION_THRIFT,
    HAPI_SESSION_CUSTOM1,
    HAPI_SESSION_CUSTOM2,
    HAPI_SESSION_CUSTOM3,
    HAPI_SESSION_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_SessionType )

enum HAPI_State
{
    /// Everything cook successfully without errors
    HAPI_STATE_READY,
    /// You should abort the cook.
    HAPI_STATE_READY_WITH_FATAL_ERRORS,
    /// Only some objects failed.
    HAPI_STATE_READY_WITH_COOK_ERRORS,
    HAPI_STATE_STARTING_COOK,
    HAPI_STATE_COOKING,
    HAPI_STATE_STARTING_LOAD,
    HAPI_STATE_LOADING,
    HAPI_STATE_MAX,

    HAPI_STATE_MAX_READY_STATE = HAPI_STATE_READY_WITH_COOK_ERRORS
};
HAPI_C_ENUM_TYPEDEF( HAPI_State )

enum HAPI_PackedPrimInstancingMode
{
    HAPI_PACKEDPRIM_INSTANCING_MODE_INVALID = -1,
    HAPI_PACKEDPRIM_INSTANCING_MODE_DISABLED,
    HAPI_PACKEDPRIM_INSTANCING_MODE_HIERARCHY,
    HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT,
    HAPI_PACKEDPRIM_INSTANCING_MODE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_PackedPrimInstancingMode )

enum HAPI_Permissions
{
    HAPI_PERMISSIONS_NON_APPLICABLE,
    HAPI_PERMISSIONS_READ_WRITE,
    HAPI_PERMISSIONS_READ_ONLY,
    HAPI_PERMISSIONS_WRITE_ONLY,
    HAPI_PERMISSIONS_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_Permissions )

enum HAPI_RampType
{
    HAPI_RAMPTYPE_INVALID = -1,
    HAPI_RAMPTYPE_FLOAT,
    HAPI_RAMPTYPE_COLOR,
    HAPI_RAMPTYPE_MAX,
};
HAPI_C_ENUM_TYPEDEF( HAPI_RampType )

/// Represents the data type of a parm.
/// As you can see, some of these high level types share the same underlying
/// raw data type. For instance, both string and file parameter types can be
/// represented with strings, yet semantically they are different. We will
/// group high level parameter types that share an underlying raw data type
/// together, so you can always check the raw data type of a parameter based
/// on its high level data type by checking a range of values.
enum HAPI_ParmType
{
    /// @{
    HAPI_PARMTYPE_INT = 0,
    HAPI_PARMTYPE_MULTIPARMLIST,
    HAPI_PARMTYPE_TOGGLE,
    HAPI_PARMTYPE_BUTTON,
    /// }@
    
    /// @{
    HAPI_PARMTYPE_FLOAT,
    HAPI_PARMTYPE_COLOR,
    /// @}

    /// @{
    HAPI_PARMTYPE_STRING,
    HAPI_PARMTYPE_PATH_FILE,
    HAPI_PARMTYPE_PATH_FILE_GEO,
    HAPI_PARMTYPE_PATH_FILE_IMAGE,
    /// @}

    HAPI_PARMTYPE_NODE,

    /// @{
    HAPI_PARMTYPE_FOLDERLIST,
    HAPI_PARMTYPE_FOLDERLIST_RADIO,
    /// @}

    /// @{
    HAPI_PARMTYPE_FOLDER,
    HAPI_PARMTYPE_LABEL,
    HAPI_PARMTYPE_SEPARATOR,
    HAPI_PARMTYPE_PATH_FILE_DIR,
    /// @}

    // Helpers

    /// Total number of supported parameter types.
    HAPI_PARMTYPE_MAX,

    HAPI_PARMTYPE_INT_START         = HAPI_PARMTYPE_INT,
    HAPI_PARMTYPE_INT_END           = HAPI_PARMTYPE_BUTTON,

    HAPI_PARMTYPE_FLOAT_START       = HAPI_PARMTYPE_FLOAT,
    HAPI_PARMTYPE_FLOAT_END         = HAPI_PARMTYPE_COLOR,

    HAPI_PARMTYPE_STRING_START      = HAPI_PARMTYPE_STRING,
    HAPI_PARMTYPE_STRING_END        = HAPI_PARMTYPE_NODE,

    HAPI_PARMTYPE_PATH_START        = HAPI_PARMTYPE_PATH_FILE,
    HAPI_PARMTYPE_PATH_END          = HAPI_PARMTYPE_PATH_FILE_IMAGE,

    HAPI_PARMTYPE_NODE_START        = HAPI_PARMTYPE_NODE,
    HAPI_PARMTYPE_NODE_END          = HAPI_PARMTYPE_NODE,

    HAPI_PARMTYPE_CONTAINER_START   = HAPI_PARMTYPE_FOLDERLIST,
    HAPI_PARMTYPE_CONTAINER_END     = HAPI_PARMTYPE_FOLDERLIST_RADIO,

    HAPI_PARMTYPE_NONVALUE_START    = HAPI_PARMTYPE_FOLDER,
    HAPI_PARMTYPE_NONVALUE_END      = HAPI_PARMTYPE_SEPARATOR
};
HAPI_C_ENUM_TYPEDEF( HAPI_ParmType )

/// Corresponds to the types as shown in the Houdini Type Properties
/// window and in DialogScript files.  Available on HAPI_ParmInfo
/// See: <a href="http://www.sidefx.com/docs/houdini/ref/windows/optype.html#parmtypes">Parameter types</a>
///
enum HAPI_PrmScriptType
{
    ///  "int", "integer"
    HAPI_PRM_SCRIPT_TYPE_INT = 0,
    HAPI_PRM_SCRIPT_TYPE_FLOAT,
    HAPI_PRM_SCRIPT_TYPE_ANGLE,
    HAPI_PRM_SCRIPT_TYPE_STRING,
    HAPI_PRM_SCRIPT_TYPE_FILE,
    HAPI_PRM_SCRIPT_TYPE_DIRECTORY,
    HAPI_PRM_SCRIPT_TYPE_IMAGE,
    HAPI_PRM_SCRIPT_TYPE_GEOMETRY,
    ///  "toggle", "embed"
    HAPI_PRM_SCRIPT_TYPE_TOGGLE,
    HAPI_PRM_SCRIPT_TYPE_BUTTON,
    HAPI_PRM_SCRIPT_TYPE_VECTOR2,
    ///  "vector", "vector3"
    HAPI_PRM_SCRIPT_TYPE_VECTOR3,
    HAPI_PRM_SCRIPT_TYPE_VECTOR4,
    HAPI_PRM_SCRIPT_TYPE_INTVECTOR2,
    ///  "intvector", "intvector3"
    HAPI_PRM_SCRIPT_TYPE_INTVECTOR3,
    HAPI_PRM_SCRIPT_TYPE_INTVECTOR4,
    HAPI_PRM_SCRIPT_TYPE_UV,
    HAPI_PRM_SCRIPT_TYPE_UVW,
    ///  "dir", "direction"
    HAPI_PRM_SCRIPT_TYPE_DIR,
    ///  "color", "rgb"
    HAPI_PRM_SCRIPT_TYPE_COLOR,
    ///  "color4", "rgba"
    HAPI_PRM_SCRIPT_TYPE_COLOR4,
    HAPI_PRM_SCRIPT_TYPE_OPPATH,
    HAPI_PRM_SCRIPT_TYPE_OPLIST,
    HAPI_PRM_SCRIPT_TYPE_OBJECT,
    HAPI_PRM_SCRIPT_TYPE_OBJECTLIST,
    HAPI_PRM_SCRIPT_TYPE_RENDER,
    HAPI_PRM_SCRIPT_TYPE_SEPARATOR,
    HAPI_PRM_SCRIPT_TYPE_GEOMETRY_DATA,
    HAPI_PRM_SCRIPT_TYPE_KEY_VALUE_DICT,
    HAPI_PRM_SCRIPT_TYPE_LABEL,
    HAPI_PRM_SCRIPT_TYPE_RGBAMASK,
    HAPI_PRM_SCRIPT_TYPE_ORDINAL,
    HAPI_PRM_SCRIPT_TYPE_RAMP_FLT,
    HAPI_PRM_SCRIPT_TYPE_RAMP_RGB,
    HAPI_PRM_SCRIPT_TYPE_FLOAT_LOG,
    HAPI_PRM_SCRIPT_TYPE_INT_LOG,
    HAPI_PRM_SCRIPT_TYPE_DATA,
    HAPI_PRM_SCRIPT_TYPE_FLOAT_MINMAX,
    HAPI_PRM_SCRIPT_TYPE_INT_MINMAX,
    HAPI_PRM_SCRIPT_TYPE_INT_STARTEND,
    HAPI_PRM_SCRIPT_TYPE_BUTTONSTRIP,
    HAPI_PRM_SCRIPT_TYPE_ICONSTRIP,

    /// The following apply to HAPI_PARMTYPE_FOLDER type parms.
    /// Radio buttons Folder
    HAPI_PRM_SCRIPT_TYPE_GROUPRADIO = 1000,
    /// Collapsible Folder
    HAPI_PRM_SCRIPT_TYPE_GROUPCOLLAPSIBLE,
    /// Simple Folder
    HAPI_PRM_SCRIPT_TYPE_GROUPSIMPLE,
    /// Tabs Folder
    HAPI_PRM_SCRIPT_TYPE_GROUP
};
HAPI_C_ENUM_TYPEDEF( HAPI_PrmScriptType )

enum HAPI_ChoiceListType
{
     /// Parameter is not a menu
    HAPI_CHOICELISTTYPE_NONE,
    /// Menu Only, Single Selection
    HAPI_CHOICELISTTYPE_NORMAL,
    /// Mini Menu Only, Single Selection
    HAPI_CHOICELISTTYPE_MINI,
    /// Field + Single Selection Menu
    HAPI_CHOICELISTTYPE_REPLACE,
    /// Field + Multiple Selection Menu
    HAPI_CHOICELISTTYPE_TOGGLE
};
HAPI_C_ENUM_TYPEDEF( HAPI_ChoiceListType )

enum HAPI_PresetType
{
    HAPI_PRESETTYPE_INVALID = -1,
    /// Just the presets binary blob
    HAPI_PRESETTYPE_BINARY = 0,
    /// Presets blob within an .idx file format
    HAPI_PRESETTYPE_IDX,
    HAPI_PRESETTYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_PresetType )

enum HAPI_NodeType
{
    HAPI_NODETYPE_ANY       = -1,
    HAPI_NODETYPE_NONE      = 0,
    HAPI_NODETYPE_OBJ       = 1 << 0,
    HAPI_NODETYPE_SOP       = 1 << 1,
    HAPI_NODETYPE_CHOP      = 1 << 2,
    HAPI_NODETYPE_ROP       = 1 << 3,
    HAPI_NODETYPE_SHOP      = 1 << 4,
    HAPI_NODETYPE_COP       = 1 << 5,
    HAPI_NODETYPE_VOP       = 1 << 6,
    HAPI_NODETYPE_DOP       = 1 << 7,
    HAPI_NODETYPE_TOP       = 1 << 8
};
HAPI_C_ENUM_TYPEDEF( HAPI_NodeType )
typedef int HAPI_NodeTypeBits;

/// Flags used to filter compositions of node lists.
/// Flags marked 'Recursive Flag' will exclude children whos parent does not
/// satisfy the flag, even if the children themselves satisfy the flag.
enum HAPI_NodeFlags
{
    HAPI_NODEFLAGS_ANY          = -1,
    HAPI_NODEFLAGS_NONE         = 0,
    /// Recursive Flag
    HAPI_NODEFLAGS_DISPLAY      = 1 << 0,
    /// Recursive Flag
    HAPI_NODEFLAGS_RENDER       = 1 << 1,
    HAPI_NODEFLAGS_TEMPLATED    = 1 << 2,
    HAPI_NODEFLAGS_LOCKED       = 1 << 3,
    HAPI_NODEFLAGS_EDITABLE     = 1 << 4,
    HAPI_NODEFLAGS_BYPASS       = 1 << 5,
    HAPI_NODEFLAGS_NETWORK      = 1 << 6,

    /// OBJ Node Specific Flags
    HAPI_NODEFLAGS_OBJ_GEOMETRY = 1 << 7,
    HAPI_NODEFLAGS_OBJ_CAMERA   = 1 << 8,
    HAPI_NODEFLAGS_OBJ_LIGHT    = 1 << 9,
    HAPI_NODEFLAGS_OBJ_SUBNET   = 1 << 10,

    /// SOP Node Specific Flags
    /// Looks for "curve"
    HAPI_NODEFLAGS_SOP_CURVE    = 1 << 11,
    /// Looks for Guide Geometry
    HAPI_NODEFLAGS_SOP_GUIDE    = 1 << 12,

    /// TOP Node Specific Flags
    /// All TOP nodes except schedulers
    HAPI_NODEFLAGS_TOP_NONSCHEDULER = 1 << 13,

    HAPI_NODEFLAGS_NON_BYPASS   = 1 << 14 /// Nodes that are not bypassed
};
HAPI_C_ENUM_TYPEDEF( HAPI_NodeFlags )
typedef int HAPI_NodeFlagsBits;

enum HAPI_GroupType
{
    HAPI_GROUPTYPE_INVALID = -1,
    HAPI_GROUPTYPE_POINT,
    HAPI_GROUPTYPE_PRIM,
    HAPI_GROUPTYPE_EDGE,
    HAPI_GROUPTYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_GroupType )

enum HAPI_AttributeOwner
{
    HAPI_ATTROWNER_INVALID = -1,
    HAPI_ATTROWNER_VERTEX,
    HAPI_ATTROWNER_POINT,
    HAPI_ATTROWNER_PRIM,
    HAPI_ATTROWNER_DETAIL,
    HAPI_ATTROWNER_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_AttributeOwner )

enum HAPI_CurveType
{
    HAPI_CURVETYPE_INVALID = -1,
    HAPI_CURVETYPE_LINEAR,
    HAPI_CURVETYPE_NURBS,
    HAPI_CURVETYPE_BEZIER,
    HAPI_CURVETYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_CurveType )

enum HAPI_InputCurveMethod
{
    HAPI_CURVEMETHOD_INVALID = -1,
    HAPI_CURVEMETHOD_CVS,
    HAPI_CURVEMETHOD_BREAKPOINTS,
    HAPI_CURVEMETHOD_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_InputCurveMethod )

enum HAPI_InputCurveParameterization
{
    HAPI_CURVEPARAMETERIZATION_INVALID = -1,
    HAPI_CURVEPARAMETERIZATION_UNIFORM,
    HAPI_CURVEPARAMETERIZATION_CHORD,
    HAPI_CURVEPARAMETERIZATION_CENTRIPETAL,
    HAPI_CURVEPARAMETERIZATION_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_InputCurveParameterization )

enum HAPI_VolumeType
{
    HAPI_VOLUMETYPE_INVALID = -1,
    HAPI_VOLUMETYPE_HOUDINI,
    HAPI_VOLUMETYPE_VDB,
    HAPI_VOLUMETYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_VolumeType )

enum HAPI_VolumeVisualType
{
    HAPI_VOLUMEVISTYPE_INVALID = -1,
    HAPI_VOLUMEVISTYPE_SMOKE,
    HAPI_VOLUMEVISTYPE_RAINBOW,
    HAPI_VOLUMEVISTYPE_ISO,
    HAPI_VOLUMEVISTYPE_INVISIBLE,
    HAPI_VOLUMEVISTYPE_HEIGHTFIELD,
    HAPI_VOLUMEVISTYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_VolumeVisualType )

enum HAPI_StorageType
{
    HAPI_STORAGETYPE_INVALID = -1,

    HAPI_STORAGETYPE_INT,
    HAPI_STORAGETYPE_INT64,
    HAPI_STORAGETYPE_FLOAT,
    HAPI_STORAGETYPE_FLOAT64,
    HAPI_STORAGETYPE_STRING,
    HAPI_STORAGETYPE_UINT8,
    HAPI_STORAGETYPE_INT8,
    HAPI_STORAGETYPE_INT16,

    HAPI_STORAGETYPE_INT_ARRAY,
    HAPI_STORAGETYPE_INT64_ARRAY,
    HAPI_STORAGETYPE_FLOAT_ARRAY,
    HAPI_STORAGETYPE_FLOAT64_ARRAY,
    HAPI_STORAGETYPE_STRING_ARRAY,
    HAPI_STORAGETYPE_UINT8_ARRAY,
    HAPI_STORAGETYPE_INT8_ARRAY,
    HAPI_STORAGETYPE_INT16_ARRAY,

    HAPI_STORAGETYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_StorageType )

enum HAPI_AttributeTypeInfo
{
    HAPI_ATTRIBUTE_TYPE_INVALID = -1,
    /// Implicit type based on data
    HAPI_ATTRIBUTE_TYPE_NONE,
    /// Position
    HAPI_ATTRIBUTE_TYPE_POINT,
    /// Homogeneous position
    HAPI_ATTRIBUTE_TYPE_HPOINT,
    /// Direction vector
    HAPI_ATTRIBUTE_TYPE_VECTOR,
    /// Normal
    HAPI_ATTRIBUTE_TYPE_NORMAL,
    /// Color
    HAPI_ATTRIBUTE_TYPE_COLOR,
    /// Quaternion
    HAPI_ATTRIBUTE_TYPE_QUATERNION,
    /// 3x3 Matrix
    HAPI_ATTRIBUTE_TYPE_MATRIX3,
    /// Matrix
    HAPI_ATTRIBUTE_TYPE_MATRIX,
    /// Parametric interval
    HAPI_ATTRIBUTE_TYPE_ST,
    /// "Private" (hidden)
    HAPI_ATTRIBUTE_TYPE_HIDDEN,
    /// 2x2 Bounding box
    HAPI_ATTRIBUTE_TYPE_BOX2,
    /// 3x3 Bounding box
    HAPI_ATTRIBUTE_TYPE_BOX,
    /// Texture coordinate
    HAPI_ATTRIBUTE_TYPE_TEXTURE,
    HAPI_ATTRIBUTE_TYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_AttributeTypeInfo )

enum HAPI_GeoType
{
    HAPI_GEOTYPE_INVALID = -1,

    /// Most geos will be of this type which essentially means a geo
    /// not of the other types.
    HAPI_GEOTYPE_DEFAULT,

    /// An exposed edit node.
    /// See @ref HAPI_IntermediateAssetsResults.
    HAPI_GEOTYPE_INTERMEDIATE,

    /// An input geo that can accept geometry from the host.
    /// See @ref HAPI_AssetInputs_MarshallingGeometryIntoHoudini.
    HAPI_GEOTYPE_INPUT,

    /// A curve.
    /// See @ref HAPI_Curves.
    HAPI_GEOTYPE_CURVE,

    HAPI_GEOTYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_GeoType )

enum HAPI_PartType
{
    HAPI_PARTTYPE_INVALID = -1,
    HAPI_PARTTYPE_MESH,
    HAPI_PARTTYPE_CURVE,
    HAPI_PARTTYPE_VOLUME,
    HAPI_PARTTYPE_INSTANCER,
    HAPI_PARTTYPE_BOX,
    HAPI_PARTTYPE_SPHERE,
    HAPI_PARTTYPE_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_PartType )

enum HAPI_InputType
{
    HAPI_INPUT_INVALID = -1,
    HAPI_INPUT_TRANSFORM,
    HAPI_INPUT_GEOMETRY,
    HAPI_INPUT_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_InputType )

enum HAPI_CurveOrders
{
    HAPI_CURVE_ORDER_VARYING = 0,
    HAPI_CURVE_ORDER_INVALID = 1,
    HAPI_CURVE_ORDER_LINEAR = 2,
    HAPI_CURVE_ORDER_QUADRATIC = 3,
    HAPI_CURVE_ORDER_CUBIC = 4,
};
HAPI_C_ENUM_TYPEDEF( HAPI_CurveOrders )

enum HAPI_TransformComponent
{
    HAPI_TRANSFORM_TX = 0,
    HAPI_TRANSFORM_TY,
    HAPI_TRANSFORM_TZ,
    HAPI_TRANSFORM_RX,
    HAPI_TRANSFORM_RY,
    HAPI_TRANSFORM_RZ,
    HAPI_TRANSFORM_QX,
    HAPI_TRANSFORM_QY,
    HAPI_TRANSFORM_QZ,
    HAPI_TRANSFORM_QW,
    HAPI_TRANSFORM_SX,
    HAPI_TRANSFORM_SY,
    HAPI_TRANSFORM_SZ
};
HAPI_C_ENUM_TYPEDEF( HAPI_TransformComponent )

enum HAPI_RSTOrder
{
    HAPI_TRS = 0,
    HAPI_TSR,
    HAPI_RTS,
    HAPI_RST,
    HAPI_STR,
    HAPI_SRT,

    HAPI_RSTORDER_DEFAULT = HAPI_SRT
};
HAPI_C_ENUM_TYPEDEF( HAPI_RSTOrder )

enum HAPI_XYZOrder
{
    HAPI_XYZ = 0,
    HAPI_XZY,
    HAPI_YXZ,
    HAPI_YZX,
    HAPI_ZXY,
    HAPI_ZYX,

    HAPI_XYZORDER_DEFAULT = HAPI_XYZ
};
HAPI_C_ENUM_TYPEDEF( HAPI_XYZOrder )

enum HAPI_ImageDataFormat
{
    HAPI_IMAGE_DATA_UNKNOWN = -1,
    HAPI_IMAGE_DATA_INT8,
    HAPI_IMAGE_DATA_INT16,
    HAPI_IMAGE_DATA_INT32,
    HAPI_IMAGE_DATA_FLOAT16,
    HAPI_IMAGE_DATA_FLOAT32,
    HAPI_IMAGE_DATA_MAX,

    HAPI_IMAGE_DATA_DEFAULT = HAPI_IMAGE_DATA_INT8
};
HAPI_C_ENUM_TYPEDEF( HAPI_ImageDataFormat )

enum HAPI_ImagePacking
{
    HAPI_IMAGE_PACKING_UNKNOWN = -1,
    HAPI_IMAGE_PACKING_SINGLE,  /// Single Channel
    HAPI_IMAGE_PACKING_DUAL,    /// Dual Channel
    HAPI_IMAGE_PACKING_RGB,     /// RGB
    HAPI_IMAGE_PACKING_BGR,     /// RGB Reversed
    HAPI_IMAGE_PACKING_RGBA,    /// RGBA
    HAPI_IMAGE_PACKING_ABGR,    /// RGBA Reversed
    HAPI_IMAGE_PACKING_MAX,

    HAPI_IMAGE_PACKING_DEFAULT3 = HAPI_IMAGE_PACKING_RGB,
    HAPI_IMAGE_PACKING_DEFAULT4 = HAPI_IMAGE_PACKING_RGBA
};
HAPI_C_ENUM_TYPEDEF( HAPI_ImagePacking )

/// Used with ::HAPI_GetEnvInt() to retrieve basic information
/// about the HAPI implementation currently being linked
/// against. Note that as of HAPI version 2.0, these enum values are
/// guaranteed never to change so you can reliably get this information from
/// any post-2.0 version of HAPI. The same goes for the actual
/// ::HAPI_GetEnvInt() API call.
enum HAPI_EnvIntType
{
    HAPI_ENVINT_INVALID = -1,

    /// The three components of the Houdini version that HAPI is
    /// expecting to link against.
    /// @{
    HAPI_ENVINT_VERSION_HOUDINI_MAJOR = 100,
    HAPI_ENVINT_VERSION_HOUDINI_MINOR = 110,
    HAPI_ENVINT_VERSION_HOUDINI_BUILD = 120,
    HAPI_ENVINT_VERSION_HOUDINI_PATCH = 130,
    /// @}

    /// The two components of the Houdini Engine (marketed) version.
    /// @{
    HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR = 200,
    HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR = 210,
    /// @}

    /// This is a monotonously increasing API version number that can be used
    /// to lock against a certain API for compatibility purposes. Basically,
    /// when this number changes code compiled against the HAPI.h methods
    /// might no longer compile. Semantic changes to the methods will also
    /// cause this version to increase. This number will be reset to 0
    /// every time the Houdini Engine version is bumped.
    HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API = 220,

    HAPI_ENVINT_MAX,
};
HAPI_C_ENUM_TYPEDEF( HAPI_EnvIntType )

/// This enum is to be used with ::HAPI_GetSessionEnvInt() to retrieve basic
/// session-specific information.
enum HAPI_SessionEnvIntType
{
    HAPI_SESSIONENVINT_INVALID = -1,

    /// License Type. See ::HAPI_License.
    HAPI_SESSIONENVINT_LICENSE = 100,

    HAPI_SESSIONENVINT_MAX
};
HAPI_C_ENUM_TYPEDEF( HAPI_SessionEnvIntType )

/// Identifies a memory cache 
enum HAPI_CacheProperty
{
    /// Current memory usage in MB. Setting this to 0 invokes
    /// a cache clear.
    HAPI_CACHEPROP_CURRENT,

    HAPI_CACHEPROP_HAS_MIN, /// True if it actually has a minimum size.
    HAPI_CACHEPROP_MIN, /// Min cache memory limit in MB.
    HAPI_CACHEPROP_HAS_MAX, /// True if it actually has a maximum size.
    HAPI_CACHEPROP_MAX, /// Max cache memory limit in MB.

    /// How aggressive to cull memory. This only works for:
    ///     - ::HAPI_CACHE_COP_COOK where:
    ///         0   ->  Never reduce inactive cache.
    ///         1   ->  Always reduce inactive cache.
    ///     - ::HAPI_CACHE_OBJ where:
    ///         0   ->  Never enforce the max memory limit.
    ///         1   ->  Always enforce the max memory limit.
    ///     - ::HAPI_CACHE_SOP where:
    ///         0   ->  When to Unload = Never
    ///                 When to Limit Max Memory = Never
    ///         1-2 ->  When to Unload = Based on Flag
    ///                 When to Limit Max Memory = Never
    ///         3-4 ->  When to Unload = Based on Flag
    ///                 When to Limit Max Memory = Always
    ///         5   ->  When to Unload = Always
    ///                 When to Limit Max Memory = Always
    HAPI_CACHEPROP_CULL_LEVEL,
};

HAPI_C_ENUM_TYPEDEF( HAPI_CacheProperty )

/// Type of sampling for heightfield
enum HAPI_HeightFieldSampling
{
    HAPI_HEIGHTFIELD_SAMPLING_CENTER,
    HAPI_HEIGHTFIELD_SAMPLING_CORNER
};
HAPI_C_ENUM_TYPEDEF( HAPI_HeightFieldSampling )

/// Used with PDG functions
enum HAPI_PDG_State
{
    HAPI_PDG_STATE_READY,
    HAPI_PDG_STATE_COOKING,
    HAPI_PDG_STATE_MAX,

    HAPI_PDG_STATE_MAX_READY_STATE = HAPI_PDG_STATE_READY
};
HAPI_C_ENUM_TYPEDEF( HAPI_PDG_State )

/// Used with PDG functions
enum HAPI_PDG_EventType
{
    /// An empty, undefined event.  Should be ignored.
    HAPI_PDG_EVENT_NULL,

    /// Sent when a new work item is added by a node
    HAPI_PDG_EVENT_WORKITEM_ADD,
    /// Sent when a work item is deleted from a node
    HAPI_PDG_EVENT_WORKITEM_REMOVE,
    /// Sent when a work item's state changes
    HAPI_PDG_EVENT_WORKITEM_STATE_CHANGE,

    /// Sent when a work item has a dependency added
    HAPI_PDG_EVENT_WORKITEM_ADD_DEP,
    /// Sent when a dependency is removed from a work item
    HAPI_PDG_EVENT_WORKITEM_REMOVE_DEP,

    /// Sent from dynamic work items that generate from a cooked item
    HAPI_PDG_EVENT_WORKITEM_ADD_PARENT,
    /// Sent when the parent item for a work item is deleted
    HAPI_PDG_EVENT_WORKITEM_REMOVE_PARENT,

    /// A node event that indicates that node is about to have all its work items cleared
    HAPI_PDG_EVENT_NODE_CLEAR,

    /// Sent when an error is issued by the node
    HAPI_PDG_EVENT_COOK_ERROR,
    /// Sent when an warning is issued by the node
    HAPI_PDG_EVENT_COOK_WARNING,

    /// Sent for each node in the graph, when a cook completes
    HAPI_PDG_EVENT_COOK_COMPLETE,

    /// A node event indicating that one more items in the node will be dirtied
    HAPI_PDG_EVENT_DIRTY_START,
    /// A node event indicating that the node has finished dirtying items
    HAPI_PDG_EVENT_DIRTY_STOP,

    /// A event indicating that the entire graph is about to be dirtied
    HAPI_PDG_EVENT_DIRTY_ALL,

    /// A work item event that indicates the item has been selected in the TOPs UI
    HAPI_PDG_EVENT_UI_SELECT,

    /// Sent when a new node is created
    HAPI_PDG_EVENT_NODE_CREATE,
    /// Sent when a node was removed from the graph
    HAPI_PDG_EVENT_NODE_REMOVE,
    /// Sent when a node was renamed
    HAPI_PDG_EVENT_NODE_RENAME,
    /// Sent when a node was connected to another node
    HAPI_PDG_EVENT_NODE_CONNECT,
    /// Sent when a node is disconnected from another node
    HAPI_PDG_EVENT_NODE_DISCONNECT,

    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_SET_INT,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_SET_FLOAT,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_SET_STRING,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_SET_FILE,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_SET_PYOBJECT,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_SET_GEOMETRY,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_MERGE,
    /// Sent when an output file is added to a work item
    HAPI_PDG_EVENT_WORKITEM_RESULT,

    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_PRIORITY,
    /// Sent for each node in the graph, when a cook starts
    HAPI_PDG_EVENT_COOK_START,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_ADD_STATIC_ANCESTOR,
    /// Deprecated
    HAPI_PDG_EVENT_WORKITEM_REMOVE_STATIC_ANCESTOR,

    /// Deprecated
    HAPI_PDG_EVENT_NODE_PROGRESS_UPDATE,
    /// Deprecated
    HAPI_PDG_EVENT_BATCH_ITEM_INITIALIZED,
    /// A special enum that represents the OR of all event types
    HAPI_PDG_EVENT_ALL,
    ///  A special enum that represents the OR of both the `CookError` and `CookWarning` events
    HAPI_PDG_EVENT_LOG,

    /// Sent when a new scheduler is added to the graph
    HAPI_PDG_EVENT_SCHEDULER_ADDED,
    /// Sent when a scheduler is removed from the graph
    HAPI_PDG_EVENT_SCHEDULER_REMOVED,
    /// Deprecated
    HAPI_PDG_EVENT_SET_SCHEDULER,
    /// Deprecated
    HAPI_PDG_EVENT_SERVICE_MANAGER_ALL,

    HAPI_PDG_CONTEXT_EVENTS
};
HAPI_C_ENUM_TYPEDEF( HAPI_PDG_EventType )

/// Used with PDG functions
enum HAPI_PDG_WorkitemState
{
    HAPI_PDG_WORKITEM_UNDEFINED,
    HAPI_PDG_WORKITEM_UNCOOKED,
    HAPI_PDG_WORKITEM_WAITING,
    HAPI_PDG_WORKITEM_SCHEDULED,
    HAPI_PDG_WORKITEM_COOKING,
    HAPI_PDG_WORKITEM_COOKED_SUCCESS,
    HAPI_PDG_WORKITEM_COOKED_CACHE,
    HAPI_PDG_WORKITEM_COOKED_FAIL,
    HAPI_PDG_WORKITEM_COOKED_CANCEL,
    HAPI_PDG_WORKITEM_DIRTY
};
HAPI_C_ENUM_TYPEDEF( HAPI_PDG_WorkitemState )

/////////////////////////////////////////////////////////////////////////////
// Main API Structs

// GENERICS -----------------------------------------------------------------

/// A Transform with Quaternion rotation
struct HAPI_API HAPI_Transform
{
    float position[ HAPI_POSITION_VECTOR_SIZE ];
    float rotationQuaternion[ HAPI_QUATERNION_VECTOR_SIZE ];
    float scale[ HAPI_SCALE_VECTOR_SIZE ];
    float shear[ HAPI_SHEAR_VECTOR_SIZE ];

    HAPI_RSTOrder rstOrder;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_Transform )

/// A Transform with Euler rotation
struct HAPI_API HAPI_TransformEuler
{
    float position[ HAPI_POSITION_VECTOR_SIZE ];
    float rotationEuler[ HAPI_EULER_VECTOR_SIZE ];
    float scale[ HAPI_SCALE_VECTOR_SIZE ];
    float shear[ HAPI_SHEAR_VECTOR_SIZE ];

    HAPI_XYZOrder rotationOrder;
    HAPI_RSTOrder rstOrder;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_TransformEuler )

// SESSIONS -----------------------------------------------------------------

/// Identifies a session
struct HAPI_API HAPI_Session
{
    /// The type of session determines the which implementation will be
    /// used to communicate with the Houdini Engine library.
    HAPI_SessionType type;

    /// Some session types support multiple simultaneous sessions. This means
    /// that each session needs to have a unique identifier.
    HAPI_SessionId id;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_Session )

/// Options to configure a Thrift server being started from HARC.
struct HAPI_API HAPI_ThriftServerOptions
{
    /// Close the server automatically when all clients disconnect from it.
    HAPI_Bool autoClose;

    /// Timeout in milliseconds for waiting on the server to
    /// signal that it's ready to serve. If the server fails
    /// to signal within this time interval, the start server call fails
    /// and the server process is terminated.
    float timeoutMs;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_ThriftServerOptions )

// TIME ---------------------------------------------------------------------

/// Data for global timeline, used with ::HAPI_SetTimelineOptions()
struct HAPI_API HAPI_TimelineOptions
{
    float fps;

    float startTime;
    float endTime;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_TimelineOptions )

// ASSETS -------------------------------------------------------------------

/// Meta-data about an HDA, returned by ::HAPI_GetAssetInfo()
struct HAPI_API HAPI_AssetInfo
{
    /// Use the node id to get the asset's parameters.
    /// See @ref HAPI_Nodes_Basics.
    HAPI_NodeId nodeId;

    /// The objectNodeId differs from the regular nodeId in that for
    /// geometry based assets (SOPs) it will be the node id of the dummy
    /// object (OBJ) node instead of the asset node. For object based assets
    /// the objectNodeId will equal the nodeId. The reason the distinction
    /// exists is because transforms are always stored on the object node
    /// but the asset parameters may not be on the asset node if the asset
    /// is a geometry asset so we need both.
    HAPI_NodeId objectNodeId;

    /// It's possible to instantiate an asset without cooking it.
    /// See @ref HAPI_Assets_Cooking.
    HAPI_Bool hasEverCooked;

    HAPI_StringHandle nameSH; /// Instance name (the label + a number).
    HAPI_StringHandle labelSH; /// This is what any end user should be shown.
    HAPI_StringHandle filePathSH; /// Path to the .otl library file.
    HAPI_StringHandle versionSH; /// User-defined asset version.
    HAPI_StringHandle fullOpNameSH; /// Full asset name and namespace.
    HAPI_StringHandle helpTextSH; /// Asset help marked-up text.
    HAPI_StringHandle helpURLSH; /// Asset help URL.

    int objectCount; /// See @ref HAPI_Objects.
    int handleCount; /// See @ref HAPI_Handles.

    /// Transform inputs exposed by the asset. For OBJ assets this is the
    /// number of transform inputs on the OBJ node. For SOP assets, this is
    /// the singular transform input on the dummy wrapper OBJ node.
    /// See @ref HAPI_AssetInputs.
    int transformInputCount;

    /// Geometry inputs exposed by the asset. For SOP assets this is
    /// the number of geometry inputs on the SOP node itself. OBJ assets
    /// will always have zero geometry inputs.
    /// See @ref HAPI_AssetInputs.
    int geoInputCount;

    /// Geometry outputs exposed by the asset. For SOP assets this is
    /// the number of geometry outputs on the SOP node itself. OBJ assets
    /// will always have zero geometry outputs.
    /// See @ref HAPI_AssetInputs.
    int geoOutputCount;

    /// For incremental updates. Indicates whether any of the assets's
    /// objects have changed. Refreshed only during an asset cook.
    HAPI_Bool haveObjectsChanged;

    /// For incremental updates. Indicates whether any of the asset's
    /// materials have changed. Refreshed only during an asset cook.
    HAPI_Bool haveMaterialsChanged;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_AssetInfo )

/// Options which affect how nodes are cooked.
struct HAPI_API HAPI_CookOptions
{
    /// Normally, geos are split into parts in two different ways. First it
    /// is split by group and within each group it is split by primitive type.
    ///
    /// For example, if you have a geo with group1 covering half of the mesh
    /// and volume1 and group2 covering the other half of the mesh, all of
    /// curve1, and volume2 you will end up with 5 parts. First two parts
    /// will be for the half-mesh of group1 and volume1, and the last three
    /// will cover group2.
    ///
    /// This toggle lets you disable the splitting by group and just have
    /// the geo be split by primitive type alone. By default, this is true
    /// and therefore geos will be split by group and primitive type. If
    /// set to false, geos will only be split by primitive type.
    HAPI_Bool splitGeosByGroup;
    HAPI_StringHandle splitGroupSH;

    /// This toggle lets you enable the splitting by unique values
    /// of a specified attribute. By default, this is false and
    /// the geo be split as described above.
    /// as described above. If this is set to true, and  splitGeosByGroup
    /// set to false, mesh geos will be split on attribute values
    /// The attribute name to split on must be created with HAPI_SetCustomString
    /// and then the splitAttrSH handle set on the struct.
    HAPI_Bool splitGeosByAttribute;
    HAPI_StringHandle splitAttrSH;

    /// For meshes only, this is enforced by convexing the mesh. Use -1
    /// to avoid convexing at all and get some performance boost.
    int maxVerticesPerPrimitive;

    /// For curves only.
    /// If this is set to true, then all curves will be refined to a linear
    /// curve and you can no longer access the original CVs.  You can control
    /// the refinement detail via ::HAPI_CookOptions::curveRefineLOD.
    /// If it's false, the curve type (NURBS, Bezier etc) will be left as is.
    HAPI_Bool refineCurveToLinear;

    /// Controls the number of divisions per unit distance when refining
    /// a curve to linear.  The default in Houdini is 8.0.
    float curveRefineLOD;

    /// If this option is turned on, then we will recursively clear the
    /// errors and warnings (and messages) of all nodes before performing
    /// the cook.
    HAPI_Bool clearErrorsAndWarnings;

    /// Decide whether to recursively cook all templated geos or not.
    HAPI_Bool cookTemplatedGeos;

    /// Decide whether to split points by vertex attributes. This takes
    /// all vertex attributes and tries to copy them to their respective
    /// points. If two vertices have any difference in their attribute values,
    /// the corresponding point is split into two points. This is repeated
    /// until all the vertex attributes have been copied to the points.
    ///
    /// With this option enabled, you can reduce the total number of vertices
    /// on a game engine side as sharing of attributes (like UVs) is optimized.
    /// To make full use of this feature, you have to think of Houdini points
    /// as game engine vertices (sharable). With this option OFF (or before
    /// this feature existed) you had to map Houdini vertices to game engine
    /// vertices, to make sure all attribute values are accounted for.
    HAPI_Bool splitPointsByVertexAttributes;

    /// Choose how you want the cook to handle packed primitives.
    /// The default is: ::HAPI_PACKEDPRIM_INSTANCING_MODE_DISABLED
    HAPI_PackedPrimInstancingMode packedPrimInstancingMode;

    /// Choose which special part types should be handled. Unhandled special
    /// part types will just be refined to ::HAPI_PARTTYPE_MESH.
    HAPI_Bool handleBoxPartTypes;
    HAPI_Bool handleSpherePartTypes;

    /// If enabled, sets the ::HAPI_PartInfo::hasChanged member during the
    /// cook.  If disabled, the member will always be true.  Checking for
    /// part changes can be expensive, so there is a potential performance
    /// gain when disabled.
    HAPI_Bool checkPartChanges;


    /// This toggle lets you enable the caching of the mesh topology.
    /// By default, this is false.  If this is set to true, cooking a mesh 
    /// geometry will update only the topology if the number of points changed.
    /// Use this to get better performance on deforming meshes.
    HAPI_Bool cacheMeshTopology;

    /// For internal use only. :)
    int extraFlags;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_CookOptions )

// NODES --------------------------------------------------------------------

/// Meta-data for a Houdini Node
struct HAPI_API HAPI_NodeInfo
{
    HAPI_NodeId id;
    HAPI_NodeId parentId;
    HAPI_StringHandle nameSH;
    HAPI_NodeType type;

    /// Always true unless the asset's definition has changed due to loading
    /// a duplicate asset definition and from another OTL asset library
    /// file OR deleting the OTL asset library file used by this node's asset.
    HAPI_Bool isValid;

    /// Total number of cooks of this node.
    int totalCookCount;

    /// Use this unique id to grab the OP_Node pointer for this node.
    /// If you're linking against the C++ HDK, include the OP_Node.h header
    /// and call OP_Node::lookupNode().
    int uniqueHoudiniNodeId;

    /// This is the internal node path in the Houdini scene graph. This path
    /// is meant to be abstracted away for most client purposes but for
    /// advanced uses it can come in handy.
    HAPI_StringHandle internalNodePathSH;

    /// Total number of parameters this asset has exposed. Includes hidden
    /// parameters.
    /// See @ref HAPI_Parameters.
    int parmCount;

    /// Number of values. A single parameter may have more than one value so
    /// this number is more than or equal to ::HAPI_NodeInfo::parmCount.
    /// @{
    int parmIntValueCount;
    int parmFloatValueCount;
    int parmStringValueCount;
    /// @}

    /// The total number of choices among all the combo box parameters.
    /// See @ref HAPI_Parameters_ChoiceLists.
    int parmChoiceCount;

    /// The number of child nodes. This is 0 for all nodes that are not
    /// node networks.
    int childNodeCount;

    /// The number of inputs this specific node has.
    int inputCount;

    /// The number of outputs this specific node has.
    int outputCount;

    /// Nodes created via scripts or via ::HAPI_CreateNode() will be have
    /// this set to true. Only such nodes can be deleted using
    /// ::HAPI_DeleteNode().
    HAPI_Bool createdPostAssetLoad;

    /// Indicates if this node will change over time
    HAPI_Bool isTimeDependent;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_NodeInfo )

// PARAMETERS ---------------------------------------------------------------

///
/// Contains parameter information like name, label, type, and size.
///
struct HAPI_API HAPI_ParmInfo
{
    /// The parent id points to the id of the parent parm
    /// of this parm. The parent parm is something like a folder.
    HAPI_ParmId id;

    /// Parameter id of the parent of this parameter.
    HAPI_ParmId parentId;

    /// Child index within its immediate parent parameter.
    int childIndex;

    /// The HAPI type of the parm
    HAPI_ParmType type;

    /// The Houdini script-type of the parm
    HAPI_PrmScriptType scriptType;

    /// Some parameter types require additional type information.
    ///     - File path parameters will indicate what file extensions they
    ///       expect in a space-separated list of wild-cards. This is set
    ///       in the Operator Type Properties using the File Pattern
    ///       parameter property.
    ///       For example, for filtering by PNG and JPG only: "*.png *.jpg"
    HAPI_StringHandle typeInfoSH;

    /// For the majority of parameter types permission will not be applicable.
    /// For file path parameters these permissions will indicate how the
    /// asset plans to use the file: whether it will only read it, only write
    /// to it, or both. This is set in the Operator Type Properties using
    /// the Browse Mode parameter property.
    HAPI_Permissions permissions;

    /// Number of tags on this paramter.
    int tagCount;

    /// Tuple size. For scalar parameters this value is 1, but for vector
    /// parameters this value can be greater.  For example, a 3 vector would
    /// have a size of 3. For folders and folder lists, this value is the
    /// number of children they own.
    int size;

    /// Any ::HAPI_ParmType can be a choice list. If this is set to
    /// ::HAPI_CHOICELISTTYPE_NONE, than this parameter is NOT a choice list.
    /// Otherwise, the parameter is a choice list of the indicated type.
    /// See @ref HAPI_Parameters_ChoiceLists.
    HAPI_ChoiceListType choiceListType;

    /// Any ::HAPI_ParmType can be a choice list. If the parameter is a
    /// choice list, this tells you how many choices it currently has.
    /// Note that some menu parameters can have a dynamic number of choices
    /// so it is important that this count is re-checked after every cook.
    /// See @ref HAPI_Parameters_ChoiceLists.
    int choiceCount;

    /// Note that folders are not real parameters in Houdini so they do not
    /// have names. The folder names given here are generated from the name
    /// of the folderlist (or switcher) parameter which is a parameter. The
    /// folderlist parameter simply defines how many of the "next" parameters
    /// belong to the first folder, how many of the parameters after that
    /// belong to the next folder, and so on. This means that folder names
    /// can change just by reordering the folders around so don't rely on
    /// them too much. The only guarantee here is that the folder names will
    /// be unique among all other parameter names.
    HAPI_StringHandle nameSH;

    /// The label string for the parameter
    HAPI_StringHandle labelSH;

    /// If this parameter is a multiparm instance than the
    /// ::HAPI_ParmInfo::templateNameSH will be the hash-templated parm name,
    /// containing #'s for the parts of the name that use the instance number.
    /// Compared to the ::HAPI_ParmInfo::nameSH, the ::HAPI_ParmInfo::nameSH
    /// will be the ::HAPI_ParmInfo::templateNameSH but with the #'s
    /// replaced by the instance number. For regular parms, the
    /// ::HAPI_ParmInfo::templateNameSH is identical to the
    /// ::HAPI_ParmInfo::nameSH.
    HAPI_StringHandle templateNameSH;

    /// The help string for this parameter
    HAPI_StringHandle helpSH;

    /// Whether min/max exists for the parameter values.
    /// @{
    HAPI_Bool hasMin;
    HAPI_Bool hasMax;
    HAPI_Bool hasUIMin;
    HAPI_Bool hasUIMax;
    /// @}

    /// Parameter value range, shared between int and float parameters.
    /// @{
    float min;
    float max;
    float UIMin;
    float UIMax;
    /// @}

    /// Whether this parm should be hidden from the user entirely. This is
    /// mostly used to expose parameters as asset meta-data but not allow the
    /// user to directly modify them.
    HAPI_Bool invisible;

    /// Whether this parm should appear enabled or disabled.
    HAPI_Bool disabled;

    /// If true, it means this parameter doesn't actually exist on the node
    /// in Houdini but was added by Houdini Engine as a spare parameter.
    /// This is just for your information. The behaviour of this parameter
    /// is not any different than a non-spare parameter.
    HAPI_Bool spare;

    HAPI_Bool joinNext;  /// Whether this parm should be on the same line as
                    /// the next parm.
    HAPI_Bool labelNone; /// Whether the label should be displayed.

    /// The index to use to look into the values array in order to retrieve
    /// the actual value(s) of this parameter.
    /// @{
    int intValuesIndex;
    int floatValuesIndex;
    int stringValuesIndex;
    int choiceIndex;
    /// @}

    /// If this is a ::HAPI_PARMTYPE_NODE, this tells you what node types
    /// this parameter accepts.
    HAPI_NodeType inputNodeType;

    /// The node input parameter could have another subtype filter specified,
    /// like "Object: Geometry Only". In this case, this value will specify
    /// that extra filter. If the filter demands a node that HAPI does not
    /// support, both this and ::HAPI_ParmInfo::inputNodeType will be set to
    /// NONE as such a node is not settable through HAPI.
    HAPI_NodeFlags inputNodeFlag;

    /// See @ref HAPI_Parameters_MultiParms.
    /// @{
    HAPI_Bool isChildOfMultiParm;

    int instanceNum; /// The index of the instance in the multiparm.
    int instanceLength; /// The number of parms in a multiparm instance.
    int instanceCount; /// The number of instances in a multiparm.

    /// First instance's ::HAPI_ParmInfo::instanceNum. Either 0 or 1.
    int instanceStartOffset;

    HAPI_RampType rampType;
    /// @}
    
    /// Provides the raw condition string which is used to evaluate the
    /// the visibility of a parm
    HAPI_StringHandle visibilityConditionSH;

    /// Provides the raw condition string which is used to evalute whether
    /// a parm is enabled or disabled
    HAPI_StringHandle disabledConditionSH;

    /// Whether or not the "Use Menu Item Token As Value" checkbox was checked in a integer menu item.
    HAPI_Bool useMenuItemTokenAsValue;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_ParmInfo )

/// Meta-data for a combo-box / choice parm
struct HAPI_API HAPI_ParmChoiceInfo
{
    HAPI_ParmId parentParmId;
    HAPI_StringHandle labelSH;

    /// This evaluates to the value of the token associated with the label
    /// applies to string menus only.
    HAPI_StringHandle valueSH;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_ParmChoiceInfo )

// HANDLES ------------------------------------------------------------------

///
/// Contains handle information such as the type of handle
/// (translate, rotate, scale, softxform ...etc) and the number of
/// parameters the current handle is bound to.
///
struct HAPI_API HAPI_HandleInfo
{
    HAPI_StringHandle nameSH;
    HAPI_StringHandle typeNameSH;

    int bindingsCount;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_HandleInfo )

///
/// Contains binding information that maps the handle parameter to
/// the asset parameter. The index is only used for int and float vector
/// and colour parms.
///
struct HAPI_API HAPI_HandleBindingInfo
{
    HAPI_StringHandle handleParmNameSH;
    HAPI_StringHandle  assetParmNameSH;

    HAPI_ParmId assetParmId;
    int assetParmIndex;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_HandleBindingInfo )

// OBJECTS ------------------------------------------------------------------

/// Meta-data for an OBJ Node
struct HAPI_API HAPI_ObjectInfo
{
    HAPI_StringHandle nameSH;

    /// @deprecated This member is no longer used
    HAPI_StringHandle objectInstancePathSH;

    /// For incremental updates. Indicates whether the object's transform
    /// has changed. Refreshed only during an asset cook.
    HAPI_Bool hasTransformChanged;

    /// For incremental updates. Indicates whether any of the object's
    /// geometry nodes have changed. Refreshed only during an asset cook.
    HAPI_Bool haveGeosChanged;

    /// Whether the object is hidden and should not be shown. Some objects
    /// should be hidden but still brought into the host environment, for
    /// example those used only for instancing.
    /// See @ref HAPI_Instancing.
    HAPI_Bool isVisible;

    /// See @ref HAPI_Instancing.
    HAPI_Bool isInstancer;

    /// Determine if this object is being instanced. Normally, this implies
    /// that while this object may not be visible, it should still be
    /// brought into the host application because it is needed by an instancer.
    /// See @ref HAPI_Instancing.
    HAPI_Bool isInstanced;

    /// @deprecated No longer used.  See @ref HAPI_Geos
    int geoCount;

    /// Use the node id to get the node's parameters.
    /// Using the HDK, you can also get the raw node C++ pointer for this
    /// object's internal node.
    /// See @ref HAPI_Nodes_Basics.
    HAPI_NodeId nodeId;

    /// If the object is an instancer, this variable gives the object id of
    /// the object that should be instanced.
    /// See @ref HAPI_Instancing.
    HAPI_NodeId objectToInstanceId;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_ObjectInfo )

// GEOMETRY -----------------------------------------------------------------

/// Meta-data for a SOP Node
struct HAPI_API HAPI_GeoInfo
{
    HAPI_GeoType type;
    HAPI_StringHandle nameSH;

    /// Use the node id to get the node's parameters.
    /// Using the HDK, you can also get the raw node C++ pointer for this
    /// object's internal node.
    HAPI_NodeId nodeId;

    /// Whether the SOP node has been exposed by dragging it into the
    /// editable nodes section of the asset definition.
    HAPI_Bool isEditable;

    /// Has the templated flag turned on which means "expose as read-only".
    HAPI_Bool isTemplated;

    /// Final Result (Display SOP).
    HAPI_Bool isDisplayGeo;

    /// For incremental updates.
    HAPI_Bool hasGeoChanged;

    /// @deprecated This variable is deprecated and should no longer be used.
    /// Materials are now separate from parts. They are maintained at the
    /// asset level so you only need to check if the material itself has
    /// changed via ::HAPI_MaterialInfo::hasChanged instead of the material
    /// on the part.
    HAPI_Bool hasMaterialChanged;

    /// Groups.
    /// @{
    int pointGroupCount;
    int primitiveGroupCount;
    int edgeGroupCount;
    /// @}

    /// Total number of parts this geometry contains.
    /// See @ref HAPI_Parts.
    int partCount;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_GeoInfo )

/// Meta-data describing a Geo Part
struct HAPI_API HAPI_PartInfo
{
    /// Id to identify this part relative to it's Geo
    HAPI_PartId id;
    /// String handle for the name of the part
    HAPI_StringHandle nameSH;
    HAPI_PartType type;

    int faceCount;
    int vertexCount;
    /// Number of points. Note that this is NOT the number
    /// of "positions" as "points" may imply. If your
    /// geometry has 3 points then set this to 3 and not 3*3.
    int pointCount;

    int attributeCounts[ HAPI_ATTROWNER_MAX ];

    /// If this is true, don't display this part. Load its data but then
    /// instance it where the corresponding instancer part tells you to
    /// instance it.
    HAPI_Bool isInstanced;

    /// The number of parts that this instancer part is instancing.
    /// For example, if we're instancing a curve and a box, they would come
    /// across as two parts, hence this count would be two.
    /// Call ::HAPI_GetInstancedPartIds() to get the list of ::HAPI_PartId.
    int instancedPartCount;

    /// The number of instances that this instancer part is instancing.
    /// Using the same example as with ::HAPI_PartInfo::instancedPartCount,
    /// if I'm instancing the merge of a curve and a box 5 times, this count
    /// would be 5. To be clear, all instanced parts are instanced the same
    /// number of times and with the same transform for each instance.
    /// Call ::HAPI_GetInstancerPartTransforms() to get the transform of
    /// each instance.
    int instanceCount;

    /// If this is false, the underlying attribute data appear to match that of
    /// the previous cook.  In this case you may be able to re-used marshaled 
    /// data from the previous cook.
    HAPI_Bool hasChanged;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_PartInfo )

/// Meta-data describing an attribute
/// See @ref HAPI_Attributes.
struct HAPI_API HAPI_AttributeInfo
{
    HAPI_Bool exists;

    HAPI_AttributeOwner owner;
    HAPI_StorageType storage;

    /// When converting from the Houdini native GA geometry format to the
    /// GT geometry format HAPI uses, some attributes might change owners.
    /// For example, in Houdini GA curves can have points shared by
    /// vertices but the GT format only supports curve vertices
    /// (no points). This means that if you had point attributes on a curve
    /// in Houdini, when it comes out of HAPI those point attributes will now
    /// be vertex attributes. In this case, the ::HAPI_AttributeInfo::owner
    /// will be set to ::HAPI_ATTROWNER_VERTEX but the
    /// ::HAPI_AttributeInfo::originalOwner will be ::HAPI_ATTROWNER_POINT.
    HAPI_AttributeOwner originalOwner;

    /// Number of attributes. This count will match the number of values
    /// given the owner. For example, if the owner is ::HAPI_ATTROWNER_VERTEX
    /// this count will be the same as the ::HAPI_PartInfo::vertexCount.
    /// To be clear, this is not the number of values in the attribute, rather
    /// it is the number of attributes. If your geometry has three 3D points
    /// then this count will be 3 (not 3*3) while the
    /// ::HAPI_AttributeInfo::tupleSize will be 3.
    int count;

    /// Number of values per attribute.
    /// Note that this is NOT the memory size of the attribute. It is the
    /// number of values per attributes. Multiplying this by the
    /// size of the ::HAPI_AttributeInfo::storage will give you the memory
    /// size per attribute.
    int tupleSize;

    /// Total number of elements for an array attribute.
    /// An array attribute can be thought of as a 2 dimensional array where
    /// the 2nd dimension can vary in size for each element in the 1st
    /// dimension. Therefore this returns the total number of values in
    /// the entire array.
    /// This should be used to determine the total storage
    /// size needed by multiplying with ::HAPI_AttributeInfo::storage.
    /// Note that this will be 0 for a non-array attribute.
    HAPI_Int64 totalArrayElements;

    /// Attribute type info
    /// This is used to help identify the type of data stored in an attribute.
    /// Using the type is recommended over using just an attribute's name to identify
    /// its purpose.
    HAPI_AttributeTypeInfo typeInfo;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_AttributeInfo )

// MATERIALS ----------------------------------------------------------------

struct HAPI_API HAPI_MaterialInfo
{
    /// This is the HAPI node id for the SHOP node this material is attached
    /// to. Use it to get access to the parameters (which contain the
    /// texture paths).
    /// IMPORTANT: When the ::HAPI_MaterialInfo::hasChanged is true this
    /// @p nodeId could have changed. Do not assume ::HAPI_MaterialInfo::nodeId
    /// will never change for a specific material.
    HAPI_NodeId nodeId;

    HAPI_Bool exists;

    HAPI_Bool hasChanged;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_MaterialInfo )

/// Describes an image format, used with ::HAPI_GetSupportedImageFileFormats()
struct HAPI_API HAPI_ImageFileFormat
{
    HAPI_StringHandle nameSH;
    HAPI_StringHandle descriptionSH;
    HAPI_StringHandle defaultExtensionSH;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_ImageFileFormat )

/// Data for an image, used with ::HAPI_GetImageInfo() and ::HAPI_SetImageInfo()
struct HAPI_API HAPI_ImageInfo
{
    /// Unlike the other members of this struct changing imageFileFormatNameSH
    /// and giving this struct back to ::HAPI_SetImageInfo() nothing will happen.
    /// Use this member variable to derive which image file format will be used
    /// by the ::HAPI_ExtractImageToFile() and ::HAPI_ExtractImageToMemory()
    /// functions if called with image_file_format_name set to NULL. This way,
    /// you can decide whether to ask for a file format conversion (slower) or
    /// not (faster).
    /// (Read-only)
    HAPI_StringHandle imageFileFormatNameSH; 

    int xRes;
    int yRes;

    HAPI_ImageDataFormat dataFormat;

    HAPI_Bool interleaved; /// ex: true = RGBRGBRGB, false = RRRGGGBBB
    HAPI_ImagePacking packing;

    /// Adjust the gamma of the image. For anything less than
    /// ::HAPI_IMAGE_DATA_INT16, you probably want to leave this as 2.2.
    double gamma;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_ImageInfo )

// ANIMATION ----------------------------------------------------------------

/// Data for a single Key Frame
struct HAPI_API HAPI_Keyframe
{
    float time;
    float value;
    float inTangent;
    float outTangent;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_Keyframe )

// VOLUMES ------------------------------------------------------------------

///
/// This represents a volume primitive but does not contain the actual voxel
/// values, which can be retrieved on a per-tile basis.
///
/// See @ref HAPI_Volumes.
///
struct HAPI_API HAPI_VolumeInfo
{
    HAPI_StringHandle nameSH;

    HAPI_VolumeType type;

    /// Each voxel is identified with an index. The indices will range
    /// between:
    /// [ ( minX, minY, minZ ), ( minX+xLength, minY+yLength, minZ+zLength ) )
    /// @{
    int xLength;
    int yLength;
    int zLength;
    int minX;
    int minY;
    int minZ;
    /// @}

    /// Number of values per voxel.
    /// The tuple size field is 1 for scalars and 3 for vector data.
    int tupleSize;

    /// Can be either ::HAPI_STORAGETYPE_INT or ::HAPI_STORAGETYPE_FLOAT.
    HAPI_StorageType storage;

    /// The dimensions of each tile.
    /// This can be 8 or 16, denoting an 8x8x8 or 16x16x16 tiles.
    int tileSize;

    /// The transform of the volume with respect to the lengths.
    /// The volume may be positioned anywhere in space.
    HAPI_Transform transform;

    /// Denotes special situations where the volume tiles are not perfect
    /// cubes, but are tapered instead.
    HAPI_Bool hasTaper;

    /// If there is taper involved, denotes the amount of taper involved.
    /// @{
    float xTaper;
    float yTaper;
    /// @}
};
HAPI_C_STRUCT_TYPEDEF( HAPI_VolumeInfo )

///
/// A HAPI_VolumeTileInfo represents an cube subarray of the volume.
/// The size of each dimension is ::HAPI_VolumeInfo::tileSize
/// bbox [(minX, minY, minZ), (minX+tileSize, minY+tileSize, minZ+tileSize))
///
struct HAPI_API HAPI_VolumeTileInfo
{
    int minX;
    int minY;
    int minZ;
    HAPI_Bool isValid;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_VolumeTileInfo )

///
/// Describes the visual settings of a volume.
///
struct HAPI_API HAPI_VolumeVisualInfo
{
    HAPI_VolumeVisualType type;
    float iso;
    float density;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_VolumeVisualInfo )

// CURVES -------------------------------------------------------------------

///
/// Represents the meta-data associated with a curve mesh (a number
/// of curves of the same type).
struct HAPI_API HAPI_CurveInfo
{
    HAPI_CurveType curveType;

    /// The number of curves contained in this curve mesh.
    int curveCount;

    /// The number of control vertices (CVs) for all curves.
    int vertexCount;

    /// The number of knots for all curves.
    int knotCount;

    /// Whether the curves in this curve mesh are periodic (closed by appending a new point)
    HAPI_Bool isPeriodic;

    /// Whether the curves in this curve mesh are rational.
    HAPI_Bool isRational;

    /// Order of 1 is invalid. 0 means there is a varying order.
    int order;

    /// Whether the curve has knots.
    HAPI_Bool hasKnots;

    /// Similar to isPeriodic, but creates a polygon instead of a separate point
    HAPI_Bool isClosed;
};
HAPI_C_STRUCT_TYPEDEF(HAPI_CurveInfo)

// Curve info dealing specifically with input curves
struct HAPI_API HAPI_InputCurveInfo
{
    /// The desired curve type of the curve
    /// Note that this is NOT necessarily equal to the value in HAPI_CurveInfo
    /// in the case of curve refinement
    HAPI_CurveType curveType;

    /// The desired order for your input curve
    /// This is your desired order, which may differ from HAPI_CurveInfo
    /// as it will do range checks and adjust the actual order accordingly
    int order;

    /// Whether or not the curve is closed
    /// May differ from HAPI_CurveInfo::isPeriodic depending on the curveType
    /// (e.g. A NURBs curve is never technically closed according to HAPI_CurveInfo)
    HAPI_Bool closed;

    /// Whether or not to reverse the curve input
    HAPI_Bool reverse;

    // Input method type (CVs or Brekapoints)
    HAPI_InputCurveMethod inputMethod;

    // Parameterization - Only used when inputMethod is BREAKPOINTS
    HAPI_InputCurveParameterization breakpointParameterization;

};
HAPI_C_STRUCT_TYPEDEF(HAPI_InputCurveInfo)

// BASIC PRIMITIVES ---------------------------------------------------------

/// Data for a Box Part
struct HAPI_API HAPI_BoxInfo
{
    float center[ HAPI_POSITION_VECTOR_SIZE ];
    float size[ HAPI_SCALE_VECTOR_SIZE ];
    float rotation[ HAPI_EULER_VECTOR_SIZE ];
};
HAPI_C_STRUCT_TYPEDEF( HAPI_BoxInfo )

/// Data for a Sphere Part
struct HAPI_API HAPI_SphereInfo
{
    float center[ HAPI_POSITION_VECTOR_SIZE ];
    float radius;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_SphereInfo )

// PDG Structs --------------------------------------------------------------

/// Data associated with a PDG Event
struct HAPI_API HAPI_PDG_EventInfo
{
    HAPI_NodeId nodeId;                     /// id of related node
    HAPI_PDG_WorkitemId workitemId;         /// id of related workitem
    HAPI_PDG_WorkitemId dependencyId;       /// id of related workitem dependency
    int currentState;                       /// ::HAPI_PDG_WorkitemState value of current state for state change
    int lastState;                          /// ::HAPI_PDG_WorkitemState value of last state for state change
    int eventType;                          /// ::HAPI_PDG_EventType event type
    HAPI_StringHandle msgSH;                /// String handle of the event message (> 0 if there is a message)
};
HAPI_C_STRUCT_TYPEDEF( HAPI_PDG_EventInfo )

/// Info for a PDG Workitem
struct HAPI_API HAPI_PDG_WorkitemInfo
{
    int index;                    /// index of the workitem
    int numResults;		          /// number of results reported
    HAPI_StringHandle nameSH;     /// name of the workitem
};
HAPI_C_STRUCT_TYPEDEF( HAPI_PDG_WorkitemInfo )

/// Data for a PDG file result
struct HAPI_API HAPI_PDG_WorkitemResultInfo
{
    int resultSH;		      /// result string data
    int resultTagSH;		  /// result tag
    HAPI_Int64 resultHash;	  /// hash value of result
};
HAPI_C_STRUCT_TYPEDEF( HAPI_PDG_WorkitemResultInfo )

// SESSIONSYNC --------------------------------------------------------------

///
/// Contains the information for synchronizing viewport between Houdini
/// and other applications. When SessionSync is enabled, Houdini will
/// update this struct with its viewport state. It will also update
/// its own viewport if this struct has changed.
/// The data stored is in Houdini's right-handed Y-up coordinate system.
///
struct HAPI_API HAPI_Viewport
{
    /// The world position of the viewport camera's pivot.
    float position[ HAPI_POSITION_VECTOR_SIZE ];

    /// The direction of the viewport camera stored as a quaternion.
    float rotationQuaternion[ HAPI_QUATERNION_VECTOR_SIZE ];

    /// The offset from the pivot to the viewport camera's
    /// actual world position.
    float offset;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_Viewport )

///
/// Contains the information for synchronizing general SessionSync
/// state between Houdini and other applications. When SessionSync
/// is enabled, Houdini will update this struct with its state.
/// It will also update its internal state if this struct has
/// changed.
///
struct HAPI_API HAPI_SessionSyncInfo
{
    /// Specifies whether Houdini's current time is used for Houdini Engine
    /// cooks. This is automatically enabled in SessionSync where
    /// Houdini's viewport forces cooks to use Houdini's current time.
    /// This is disabled in non-SessionSync mode, but can be toggled to
    /// override default behaviour.
    HAPI_Bool cookUsingHoudiniTime;

    /// Specifies whether viewport synchronization is enabled. If enabled,
    /// in SessionSync, Houdini will update its own viewport using
    /// ::HAPI_Viewport.
    HAPI_Bool syncViewport;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_SessionSyncInfo )

/// Configuration options for Houdini's compositing context
struct HAPI_API HAPI_CompositorOptions
{
    /// Specifies the maximum allowed width of an image in the compositor
    int maximumResolutionX;

    /// Specifies the maximum allowed height of an image in the compositor
    int maximumResolutionY;
};
HAPI_C_STRUCT_TYPEDEF( HAPI_CompositorOptions )

#endif // __HAPI_COMMON_h__
