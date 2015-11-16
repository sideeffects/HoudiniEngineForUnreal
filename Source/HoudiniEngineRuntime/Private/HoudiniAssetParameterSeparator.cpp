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
#include "HoudiniAssetParameterSeparator.h"
#include "HoudiniAssetComponent.h"


UHoudiniAssetParameterSeparator::UHoudiniAssetParameterSeparator(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterSeparator::~UHoudiniAssetParameterSeparator()
{

}


UHoudiniAssetParameterSeparator*
UHoudiniAssetParameterSeparator::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterSeparator* HoudiniAssetParameterSeparator = NewObject<UHoudiniAssetParameterSeparator>(Outer,
		UHoudiniAssetParameterSeparator::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterSeparator->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterSeparator;
}


bool
UHoudiniAssetParameterSeparator::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle separator type.
	if(HAPI_PARMTYPE_SEPARATOR != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.stringValuesIndex);

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterSeparator::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	TSharedPtr<SSeparator> Separator;

	DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0, 0, 5, 0)
		[
			SAssignNew(Separator, SSeparator)
			.Thickness(2.0f)
		]
	];

	if(Separator.IsValid())
	{
		Separator->SetEnabled(!bIsDisabled);
	}
}

#endif
