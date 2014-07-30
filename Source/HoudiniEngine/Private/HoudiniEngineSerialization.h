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
