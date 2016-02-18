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
#include "HoudiniAssetParameterRampColor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterRampColor::UHoudiniAssetParameterRampColor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterRampColor::~UHoudiniAssetParameterRampColor()
{

}


UHoudiniAssetParameterRampColor*
UHoudiniAssetParameterRampColor::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterRampColor* HoudiniAssetParameterRampColor =
		NewObject<UHoudiniAssetParameterRampColor>(Outer, UHoudiniAssetParameterRampColor::StaticClass(), NAME_None,
			RF_Public | RF_Transactional);

	HoudiniAssetParameterRampColor->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);

	return HoudiniAssetParameterRampColor;
}


bool
UHoudiniAssetParameterRampColor::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle color ramp parameters.
	if(HAPI_PARMTYPE_MULTIPARMLIST != ParmInfo.type || HAPI_RAMPTYPE_COLOR != ParmInfo.rampType)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterRampColor::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);
}

#endif
