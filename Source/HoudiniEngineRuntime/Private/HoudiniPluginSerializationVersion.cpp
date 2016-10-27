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

#include "HoudiniEngineRuntimePrivatePCH.h"

const FGuid FHoudiniCustomSerializationVersion::GUID( 0x1AB9CECC, 0x6913, 0x4875, 0x203d51fb );

// Register the custom version with core
FCustomVersionRegistration GRegisterHoudiniCustomVersion( FHoudiniCustomSerializationVersion::GUID, VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION, TEXT( "HoudiniUE4PluginVer" ) );
