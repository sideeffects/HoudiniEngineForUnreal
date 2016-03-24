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


class UClass;
class FString;
class UObject;
struct HAPI_MaterialInfo;


struct HOUDINIENGINERUNTIME_API FHoudiniEngineSubstance
{

#if WITH_EDITOR

public:

	/** Used to locate and load (if found) Substance instance factory object. **/
	static UObject* LoadSubstanceInstanceFactory(UClass* InstanceFactoryClass, const FString& SubstanceMaterialName);

	/** Used to locate and load (if found) Substance graph instance object. **/
	static UObject* LoadSubstanceGraphInstance(UClass* GraphInstanceClass, UObject* InstanceFactory);

#endif

	/** Retrieve Substance RTTI classes we are interested in. **/
	static bool RetrieveSubstanceRTTIClasses(UClass*& InstanceFactoryClass, UClass*& GraphInstanceClass,
		UClass*& UtilityClass);

	/** HAPI: Check if material is a Substance material. If it is, return its name by reference. **/
	static bool GetSubstanceMaterialName(const HAPI_MaterialInfo& MaterialInfo, FString& SubstanceMaterialName);
};

