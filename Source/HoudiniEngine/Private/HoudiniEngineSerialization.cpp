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

#include "HoudiniEnginePrivatePCH.h"

FHoudiniEngineSerializedProperty::FHoudiniEngineSerializedProperty(EHoudiniEngineProperty::Type InType) :
	Type(InType),
	Flags(0u),
	ArrayDim(1),
	ElementSize(4),
	Offset(0)
{

}


FHoudiniEngineSerializedProperty::FHoudiniEngineSerializedProperty(EHoudiniEngineProperty::Type InType, 
																   const FString& InName, uint64 InFlags, int32 InArrayDim,
																   int32 InElementSize, int32 InOffset) :
	Name(InName),
	Type(InType),
	Flags(InFlags),
	ArrayDim(InArrayDim),
	ElementSize(InElementSize),
	Offset(InOffset)
{

}
