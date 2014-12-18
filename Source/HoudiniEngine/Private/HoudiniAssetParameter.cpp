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


uint32
GetTypeHash(const UHoudiniAssetParameter* HoudiniAssetParameter)
{
	if(HoudiniAssetParameter)
	{
		return HoudiniAssetParameter->GetTypeHash();
	}
	
	return 0;
}



UHoudiniAssetParameter::UHoudiniAssetParameter(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetComponent(nullptr),
	ParentParameter(nullptr),
	NodeId(-1),
	ParmId(-1),
	TupleSize(1),
	ValuesIndex(-1),
	bChanged(false)
{
	ParameterName = TEXT("");
	ParameterLabel = TEXT("");

	ParentParameterName = TEXT("");
}


UHoudiniAssetParameter::~UHoudiniAssetParameter()
{

}


bool
UHoudiniAssetParameter::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, 
										HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// If parameter has has changed, we do not need to recreate it.
	if(bChanged)
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

	// Assign a unique parameter name for easier debugging times.
	//AssignUniqueParameterName();

	// Set ids.
	SetNodeParmIds(InNodeId, ParmInfo.id);

	// Set tuple count.
	TupleSize = ParmInfo.size;

	// Set component.
	HoudiniAssetComponent = InHoudiniAssetComponent;

	// Store parameter parent.
	ParentParameter = InParentParameter;

	return true;
}


void
UHoudiniAssetParameter::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	// Default implementation does nothing.
}


void
UHoudiniAssetParameter::CreateWidget(TSharedPtr<SVerticalBox> VerticalBox)
{
	// Default implementation does nothing.
}


void
UHoudiniAssetParameter::NotifyChildParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	// Default implementation does nothing.
}


void
UHoudiniAssetParameter::NotifyChildParameterWillChange(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	// Default implementation does nothing.
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
UHoudiniAssetParameter::SetHoudiniAssetComponent(UHoudiniAssetComponent* InHoudiniAssetComponent)
{
	HoudiniAssetComponent = InHoudiniAssetComponent;
}


void
UHoudiniAssetParameter::SetParentParameter(UHoudiniAssetParameter* InParentParameter)
{
	ParentParameter = InParentParameter;
}


uint32
UHoudiniAssetParameter::GetTypeHash() const
{
	// We do hashing based on parameter name.
	return ::GetTypeHash(ParameterName);
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

	// Serialize name of parent.
	bool bParentPresent = (ParentParameter != nullptr);
	Ar << bParentPresent;

	if(bParentPresent)
	{
		Ar << ParentParameterName;
	}

	// Component will be assigned separately upon loading.

	Ar << ParameterName;
	Ar << ParameterLabel;

	Ar << NodeId;
	Ar << ParmId;

	Ar << TupleSize;
	Ar << ValuesIndex;

	if(Ar.IsLoading())
	{
		bChanged = false;
	}
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
	if(!UHoudiniAssetParameter::RetrieveParameterName(ParmInfo, ParameterName))
	{
		return false;
	}

	// If there's no label, use name for label.
	if(!UHoudiniAssetParameter::RetrieveParameterLabel(ParmInfo, ParameterLabel))
	{
		ParameterLabel = ParameterName;
	}

	return true;
}


bool
UHoudiniAssetParameter::SetNameAndLabel(HAPI_StringHandle StringHandle)
{
	if(FHoudiniEngineUtils::GetHoudiniString(StringHandle, ParameterName))
	{
		ParameterLabel = ParameterName;
		return true;
	}
	
	return false;
}


bool
UHoudiniAssetParameter::RetrieveParameterName(const HAPI_ParmInfo& ParmInfo, FString& RetrievedName)
{
	return RetrieveParameterString(ParmInfo.nameSH, RetrievedName);
}


bool
UHoudiniAssetParameter::RetrieveParameterLabel(const HAPI_ParmInfo& ParmInfo, FString& RetrievedLabel)
{
	return RetrieveParameterString(ParmInfo.labelSH, RetrievedLabel);
}


bool
UHoudiniAssetParameter::RetrieveParameterString(HAPI_StringHandle StringHandle, FString& RetrievedString)
{
	// Retrieve length of this parameter's name.
	int32 ParmNameLength = 0;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HOUDINI_CHECK_ERROR(&Result, FHoudiniApi::GetStringBufLength(StringHandle, &ParmNameLength));
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

	HOUDINI_CHECK_ERROR(&Result, FHoudiniApi::GetString(StringHandle, &NameBuffer[0], ParmNameLength));
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
UHoudiniAssetParameter::MarkPreChanged()
{
	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->NotifyParameterWillChange(this);
	}

	if(ParentParameter)
	{
		ParentParameter->NotifyChildParameterWillChange(this);
	}
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

	// Notify parent parameter about change.
	if(ParentParameter)
	{
		ParentParameter->NotifyChildParameterChanged(this);
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


const FString&
UHoudiniAssetParameter::GetParameterName() const
{
	return ParameterName;
}


const FString&
UHoudiniAssetParameter::GetParameterLabel() const
{
	return ParameterLabel;
}


const FString&
UHoudiniAssetParameter::GetParentParameterName() const
{
	return ParentParameterName;
}


void
UHoudiniAssetParameter::AssignUniqueParameterName()
{
	FString CurrentName = GetName();

	if(CurrentName != TEXT("None"))
	{
		FString ClassName = GetClass()->GetName();
		FString NewName = FString::Printf(TEXT("%s_%s"), *ClassName, *ParameterLabel);
		NewName = ObjectTools::SanitizeObjectName(NewName);

		Rename(*NewName);
	}
}


void
UHoudiniAssetParameter::SetNodeId(HAPI_NodeId InNodeId)
{
	NodeId = InNodeId;
}


void
UHoudiniAssetParameter::PrintParameterInfo()
{
	HOUDINI_LOG_MESSAGE(TEXT("Parameter Change: %s"), *ParameterName);
}
