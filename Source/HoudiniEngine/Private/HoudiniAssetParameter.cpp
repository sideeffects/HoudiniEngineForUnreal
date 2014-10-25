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


UHoudiniAssetParameter::UHoudiniAssetParameter(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	HoudiniAssetComponent(nullptr),
	NodeId(-1),
	ParmId(-1),
	TupleSize(1),
	ValuesIndex(-1),
	bChanged(false)
{

}


UHoudiniAssetParameter::~UHoudiniAssetParameter()
{

}


bool
UHoudiniAssetParameter::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// If parameter has valid ids and has changed, we do not need to recreate it.
	if(HasValidNodeParmIds() && bChanged)
	{
		return false;
	}

	// If parameter is invisible, we cannot create it.
	if(!IsVisible(ParmInfo))
	{
		return false;
	}

	// Set name and label.
	if(!SetNameAndLabel(ParmInfo))
	{
		return false;
	}

	// Set ids.
	SetNodeParmIds(InNodeId, ParmInfo.id);

	// Set tuple count.
	TupleSize = ParmInfo.size;

	// Set component.
	HoudiniAssetComponent = InHoudiniAssetComponent;

	return true;
}


void
UHoudiniAssetParameter::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{

}


bool
UHoudiniAssetParameter::UploadParameterValue()
{
	// Mark this parameter as no longer changed.
	bChanged = false;

	return true;
}


bool
UHoudiniAssetParameter::HasChanged() const
{
	return bChanged;
}


void
UHoudiniAssetParameter::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetParameter* HoudiniAssetParameter = Cast<UHoudiniAssetParameter>(InThis);
	if(HoudiniAssetParameter)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetParameter->HoudiniAssetComponent;
		if(HoudiniAssetComponent)
		{
			Collector.AddReferencedObject(HoudiniAssetComponent, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetParameter::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
}


void
UHoudiniAssetParameter::SetNodeParmIds(HAPI_NodeId InNodeId, HAPI_ParmId InParmId)
{
	NodeId = InNodeId;
	ParmId = InParmId;
}


bool
UHoudiniAssetParameter::HasValidNodeParmIds() const
{
	return ((-1 != NodeId) && (-1 != ParmId));
}


bool
UHoudiniAssetParameter::IsVisible(const HAPI_ParmInfo& ParmInfo) const
{
	return ParmInfo.invisible == false;
}


bool
UHoudiniAssetParameter::SetNameAndLabel(const HAPI_ParmInfo& ParmInfo)
{
	// If we cannot retrieve name, there's nothing to do.
	if(!RetrieveParameterName(ParmInfo, Name))
	{
		return false;
	}

	// If there's no label, use name for label.
	if(!RetrieveParameterLabel(ParmInfo, Label))
	{
		Label = Name;
	}

	return true;
}


bool
UHoudiniAssetParameter::RetrieveParameterName(const HAPI_ParmInfo& ParmInfo, FString& RetrievedName) const
{
	return RetrieveParameterString(ParmInfo.nameSH, RetrievedName);
}


bool
UHoudiniAssetParameter::RetrieveParameterLabel(const HAPI_ParmInfo& ParmInfo, FString& RetrievedLabel) const
{
	return RetrieveParameterString(ParmInfo.labelSH, RetrievedLabel);
}


bool
UHoudiniAssetParameter::RetrieveParameterString(HAPI_StringHandle StringHandle, FString& RetrievedString) const
{
	// Retrieve length of this parameter's name.
	int32 ParmNameLength = 0;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HOUDINI_CHECK_ERROR(&Result, HAPI_GetStringBufLength(StringHandle, &ParmNameLength));
	if(HAPI_RESULT_SUCCESS != Result)
	{
		// We have encountered an error retrieving length of this parameter's name.
		return false;
	}

	// If length of name of this parameter is zero, we cannot continue either.
	if(!ParmNameLength)
	{
		return false;
	}

	// Retrieve name for this parameter.
	TArray<char> NameBuffer;
	NameBuffer.SetNumZeroed(ParmNameLength);

	HOUDINI_CHECK_ERROR(&Result, HAPI_GetString(StringHandle, &NameBuffer[0], ParmNameLength));
	if(HAPI_RESULT_SUCCESS != Result)
	{
		// We have encountered an error retrieving the name of this parameter.
		return false;
	}

	// Convert HAPI string to Unreal string.
	RetrievedString = UTF8_TO_TCHAR(&NameBuffer[0]);
	return true;
}


void
UHoudiniAssetParameter::MarkChanged()
{
	// Set changed flag.
	bChanged = true;

	// Notify component about change.
	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->NotifyParameterChanged(this);
	}
}


int32
UHoudiniAssetParameter::GetTupleSize() const
{
	return TupleSize;
}


bool
UHoudiniAssetParameter::IsArray() const
{
	return TupleSize > 1;
}


void
UHoudiniAssetParameter::SetValuesIndex(int32 InValuesIndex)
{
	ValuesIndex = InValuesIndex;
}

