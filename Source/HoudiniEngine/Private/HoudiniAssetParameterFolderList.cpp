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


UHoudiniAssetParameterFolderList::UHoudiniAssetParameterFolderList(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterFolderList::~UHoudiniAssetParameterFolderList()
{

}


UHoudiniAssetParameterFolderList*
UHoudiniAssetParameterFolderList::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, 
										 HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
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

	UHoudiniAssetParameterFolderList* HoudiniAssetParameterFolderList = NewObject<UHoudiniAssetParameterFolderList>(Outer);

	HoudiniAssetParameterFolderList->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterFolderList;
}


bool
UHoudiniAssetParameterFolderList::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter,
												  HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle folder and folder list types.
	if(HAPI_PARMTYPE_FOLDERLIST != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	return true;
}


void
UHoudiniAssetParameterFolderList::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	/*
	TSharedPtr<SSeparator> Separator;

	DetailCategoryBuilder.AddCustomRow(TEXT(""))
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
	*/
}

