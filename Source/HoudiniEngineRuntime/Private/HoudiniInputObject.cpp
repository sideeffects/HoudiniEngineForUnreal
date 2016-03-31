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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniInputObject.h"
#include "HoudiniInputObjectVersion.h"

FHoudiniInputObject::FHoudiniInputObject(int32 InInputIndex) :
	InputIndex(InInputIndex),
	HoudiniInputObjectFlagsPacked(0u),
	HoudiniInputObjectVersion(VER_HOUDINI_ENGINE_INPUTOBJECT_BASE)
{

}


FHoudiniInputObject::FHoudiniInputObject(const FHoudiniInputObject& HoudiniInputObject) :
	InputIndex(HoudiniInputObject.InputIndex),
	HoudiniInputObjectFlagsPacked(HoudiniInputObject.HoudiniInputObjectFlagsPacked),
	HoudiniInputObjectVersion(HoudiniInputObject.HoudiniInputObjectVersion)
{

}
