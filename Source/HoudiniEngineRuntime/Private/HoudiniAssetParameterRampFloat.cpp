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
#include "HoudiniAssetParameterRampFloat.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterRampFloat::UHoudiniAssetParameterRampFloat(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterRampFloat::~UHoudiniAssetParameterRampFloat()
{

}


UHoudiniAssetParameterRampFloat*
UHoudiniAssetParameterRampFloat::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UObject* Outer = InHoudiniAssetComponent;
	if(!Outer)
	{
		Outer = InParentParameter;
		if(!Outer)
		{
			// Must have either component or parent not null.
			check(false);
		}
	}

	UHoudiniAssetParameterRampFloat* HoudiniAssetParameterRampFloat =
		NewObject<UHoudiniAssetParameterRampFloat>(Outer, UHoudiniAssetParameterRampFloat::StaticClass(), NAME_None,
			RF_Public | RF_Transactional);

	HoudiniAssetParameterRampFloat->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);

	return HoudiniAssetParameterRampFloat;
}


bool
UHoudiniAssetParameterRampFloat::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle float ramp parameters.
	if(HAPI_PARMTYPE_MULTIPARMLIST != ParmInfo.type || HAPI_RAMPTYPE_FLOAT != ParmInfo.rampType)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterRampFloat::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);
}

#endif
