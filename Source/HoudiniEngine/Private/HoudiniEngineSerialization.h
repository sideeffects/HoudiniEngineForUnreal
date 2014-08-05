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

namespace EHoudiniEngineProperty
{
	/** This enumeration represents property types supported by our serialization system. **/

	enum Type
	{
		None,

		Integer,
		Float,
		String,
		Enumeration,
		Boolean,
		Color
	};
}


FORCEINLINE
FArchive&
operator<<(FArchive& Ar, EHoudiniEngineProperty::Type& E)
{
	uint8 B = (uint8)E;
	Ar << B;

	if (Ar.IsLoading())
	{
		E = (EHoudiniEngineProperty::Type) B;
	}

	return Ar;
}


namespace EHoudiniAssetComponentState
{
	/** This enumeration represents a state of component when it is serialized. **/

	enum Type
	{
		/** Invalid type corresponds to state when component has been created, but is in invalid state. It had no **/
		/** Houdini asset assigned. Typically this will be the case when component instance is a default class    **/
		/** object or belongs to an actor instance which is a default class object also.                          **/
		Invalid,

		/** None type corresponds to state when component has been created, but corresponding Houdini asset has not **/
		/** been instantiated.                                                                                      **/
		None,

		/** This type corresponds to a state when component has been created, corresponding Houdini asset has been **/
		/** instantiated, and component has no pending asynchronous cook request.                                  **/
		Instantiated,

		/** This type corresponds to a state when component has been created, corresponding Houdini asset has been **/
		/** instantiated, and component has a pending asynchronous cook in progress.                               **/
		BeingCooked
	};
}


FORCEINLINE
FArchive&
operator<<(FArchive& Ar, EHoudiniAssetComponentState::Type& E)
{
	uint8 B = (uint8) E;
	Ar << B;

	if(Ar.IsLoading())
	{
		E = (EHoudiniAssetComponentState::Type) B;
	}

	return Ar;
}

struct FHoudiniEngineSerializedProperty
{
	/** Constructors. **/
	FHoudiniEngineSerializedProperty(EHoudiniEngineProperty::Type InType);
	FHoudiniEngineSerializedProperty(EHoudiniEngineProperty::Type InType, const FString& InName, uint64 InFlags,
									 int32 InArrayDim, int32 InElementSize, int32 InOffset, bool bInChanged);

	/** Meta information for this property. **/
	TMap<FName, FString> Meta;

	/** Name of this property. **/
	FString Name;

	/** Type of this property. **/
	EHoudiniEngineProperty::Type Type;

	/** Property flags. **/
	uint64 Flags;

	/** Dimensions of this property, anything larger than 1 is an array. **/
	int32 ArrayDim;

	/** Size of each element. **/
	int32 ElementSize;

	/** Offset of this property. **/
	int32 Offset;

	/** Whether this property has been changed and requires reupload back to Houdini. **/
	bool bChanged;
};
