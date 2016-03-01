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

enum EHoudiniGeoPartObjectVersion
{
	VER_HOUDINI_ENGINE_GEOPARTOBJECT_BASE = 0,

	// Serialization of instancer material, if it is available.
	VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_MATERIAL_NAME,

	// Serialization of attribute instancer material, if it is available.
	VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_ATTRIBUTE_MATERIAL_NAME,

	// -----<new versions can be added before this line>-------------------------------------------------
	// - this needs to be the last line (see note below)
	VER_HOUDINI_ENGINE_GEOPARTOBJECT_AUTOMATIC_VERSION_PLUS_ONE,
	VER_HOUDINI_ENGINE_GEOPARTOBJECT_AUTOMATIC_VERSION = VER_HOUDINI_ENGINE_GEOPARTOBJECT_AUTOMATIC_VERSION_PLUS_ONE - 1
};
