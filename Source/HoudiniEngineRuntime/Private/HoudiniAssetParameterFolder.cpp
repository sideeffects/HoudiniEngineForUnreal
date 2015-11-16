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
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterFolder::UHoudiniAssetParameterFolder(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterFolder::~UHoudiniAssetParameterFolder()
{

}


UHoudiniAssetParameterFolder*
UHoudiniAssetParameterFolder::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterFolder* HoudiniAssetParameterFolder = NewObject<UHoudiniAssetParameterFolder>(Outer,
		UHoudiniAssetParameterFolder::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterFolder->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterFolder;
}


bool
UHoudiniAssetParameterFolder::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle folder and folder list types.
	if(HAPI_PARMTYPE_FOLDER != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterFolder::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	if(ParentParameter && ParentParameter->IsActiveChildParameter(this))
	{
		Super::CreateWidget(DetailCategoryBuilder);
	}
}

#endif
