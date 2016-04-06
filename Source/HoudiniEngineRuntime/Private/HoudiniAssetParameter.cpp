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
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniApi.h"
#include "HoudiniAssetInstance.h"
#include "HoudiniAssetParameterVersion.h"
#include "HoudiniEngineString.h"


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
#if WITH_EDITOR
	DetailCategoryBuilder(nullptr),
#endif
	HoudiniAssetComponent(nullptr),
	HoudiniAssetInstance(nullptr),
	ParentParameter(nullptr),
	NodeId(-1),
	ParmId(-1),
	ParmParentId(-1),
	ChildIndex(0),
	TupleSize(1),
	ValuesIndex(-1),
	MultiparmInstanceIndex(-1),
	ActiveChildParameter(0),
	bIsSpare(false),
	bIsDisabled(false),
	bChanged(false),
	bSliderDragged(false),
	bIsChildOfMultiparm(false),
	bIsSubstanceParameter(false),
	HoudiniAssetParameterVersion(VER_HOUDINI_ENGINE_PARAM_BASE)
{
	ParameterName = TEXT("");
	ParameterLabel = TEXT("");
}


UHoudiniAssetParameter::~UHoudiniAssetParameter()
{

}


bool
UHoudiniAssetParameter::CreateParameter(UHoudiniAssetInstance* InHoudiniAssetInstance,
	const FHoudiniParameterObject& HoudiniParameterObject)
{
	return true;
}


bool
UHoudiniAssetParameter::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// We need to reset child parameters.
	ResetChildParameters();

	// If parameter has changed, we do not need to recreate it.
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

	// If it is a Substance parameter, mark it as such.
	bIsSubstanceParameter = ParameterName.StartsWith(HAPI_UNREAL_PARAM_SUBSTANCE_PREFIX);

	// Set ids.
	SetNodeParmIds(InNodeId, ParmInfo.id);

	// Set parent id.
	ParmParentId = ParmInfo.parentId;

	// Set the index within parent.
	ChildIndex = ParmInfo.childIndex;

	// Set tuple count.
	TupleSize = ParmInfo.size;

	// Set the multiparm instance index.
	MultiparmInstanceIndex = ParmInfo.instanceNum;

	// Set spare flag.
	bIsSpare = ParmInfo.spare;

	// Set disabled flag.
	bIsDisabled = ParmInfo.disabled;

	// Set child of multiparm flag.
	bIsChildOfMultiparm = ParmInfo.isChildOfMultiParm;

	// Set component.
	HoudiniAssetComponent = InHoudiniAssetComponent;

	// Store parameter parent.
	ParentParameter = InParentParameter;

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameter::CreateWidget(IDetailCategoryBuilder& InDetailCategoryBuilder)
{
	// Store category builder.
	DetailCategoryBuilder = &InDetailCategoryBuilder;

	// Recursively create all child parameters.
	for(TArray<UHoudiniAssetParameter*>::TIterator IterParameter(ChildParameters); IterParameter; ++IterParameter)
	{
		(*IterParameter)->CreateWidget(InDetailCategoryBuilder);
	}
}


void
UHoudiniAssetParameter::CreateWidget(TSharedPtr<SVerticalBox> VerticalBox)
{
	// Default implementation does nothing.
}


bool
UHoudiniAssetParameter::IsColorPickerWindowOpen() const
{
	bool bOpenWindow = false;

	for(int32 ChildIdx = 0, ChildNum = ChildParameters.Num(); ChildIdx < ChildNum; ++ChildIdx)
	{
		UHoudiniAssetParameter* Parameter = ChildParameters[ChildIdx];
		if(Parameter)
		{
			bOpenWindow |= Parameter->IsColorPickerWindowOpen();
		}
	}

	return bOpenWindow;
}


#endif


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


void
UHoudiniAssetParameter::NotifyChildParametersCreated()
{
	// Default implementation does nothing.
}


void
UHoudiniAssetParameter::NotifyChildParametersLoaded()
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
UHoudiniAssetParameter::SetParameterVariantValue(const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo)
{
	// Default implementation does nothing.
	return false;
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
	if(ParentParameter != InParentParameter)
	{
		ParentParameter = InParentParameter;

		if(ParentParameter)
		{
			// Retrieve parent parameter id. We ignore folder lists, they are artificial parents created by us.
			ParmParentId = ParentParameter->GetParmId();

			// Add this parameter to parent collection of child parameters.
			ParentParameter->AddChildParameter(this);
		}
		else
		{
			// Reset parent parm id.
			ParmParentId = -1;
		}
	}
}


bool
UHoudiniAssetParameter::IsChildParameter() const
{
	return ParentParameter != nullptr;
}


void
UHoudiniAssetParameter::AddChildParameter(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	if(HoudiniAssetParameter)
	{
		ChildParameters.Add(HoudiniAssetParameter);
	}
}


uint32
UHoudiniAssetParameter::GetTypeHash() const
{
	// We do hashing based on parameter name.
	return ::GetTypeHash(ParameterName);
}


HAPI_ParmId
UHoudiniAssetParameter::GetParmId() const
{
	return ParmId;
}


HAPI_ParmId
UHoudiniAssetParameter::GetParmParentId() const
{
	return ParmParentId;
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

	HoudiniAssetParameterVersion = VER_HOUDINI_ENGINE_PARAM_AUTOMATIC_VERSION;
	Ar << HoudiniAssetParameterVersion;

	Ar << HoudiniAssetParameterFlagsPacked;

	if(Ar.IsLoading())
	{
		bChanged = false;
	}

	Ar << ParameterName;
	Ar << ParameterLabel;

	Ar << NodeId;
	Ar << ParmId;

	Ar << ParmParentId;
	Ar << ChildIndex;

	Ar << TupleSize;
	Ar << ValuesIndex;
	Ar << MultiparmInstanceIndex;

	if(HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_ASSET_INSTANCE_MEMBER)
	{
		Ar << HoudiniAssetInstance;
	}

	if(Ar.IsTransacting())
	{
		Ar << HoudiniAssetComponent;
		Ar << ParentParameter;
	}
}


#if WITH_EDITOR

void
UHoudiniAssetParameter::PostEditUndo()
{
	Super::PostEditUndo();

	MarkPreChanged();
	MarkChanged();
}

#endif


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
	FHoudiniEngineString HoudiniEngineStringName(ParmInfo.nameSH);
	FHoudiniEngineString HoudiniEngineStringLabel(ParmInfo.labelSH);

	bool bresult = true;

	bresult |= HoudiniEngineStringName.ToFString(ParameterName);
	bresult |= HoudiniEngineStringLabel.ToFString(ParameterLabel);

	return bresult;
}


bool
UHoudiniAssetParameter::SetNameAndLabel(HAPI_StringHandle StringHandle)
{
	FHoudiniEngineString HoudiniEngineString(StringHandle);

	bool bresult = true;

	bresult |= HoudiniEngineString.ToFString(ParameterName);
	bresult |= HoudiniEngineString.ToFString(ParameterLabel);

	return bresult;
}


bool
UHoudiniAssetParameter::SetNameAndLabel(const FString& Name)
{
	ParameterName = Name;
	ParameterLabel = Name;

	return true;
}


void
UHoudiniAssetParameter::MarkPreChanged(bool bMarkAndTriggerUpdate)
{
#if WITH_EDITOR

	if(HoudiniAssetComponent && bMarkAndTriggerUpdate)
	{
		HoudiniAssetComponent->NotifyParameterWillChange(this);
	}

	if(ParentParameter && bMarkAndTriggerUpdate)
	{
		ParentParameter->NotifyChildParameterWillChange(this);
	}

#endif
}


void
UHoudiniAssetParameter::MarkChanged(bool bMarkAndTriggerUpdate)
{
	// Set changed flag.
	bChanged = true;

#if WITH_EDITOR

	// Notify component about change.
	if(HoudiniAssetComponent && bMarkAndTriggerUpdate)
	{
		HoudiniAssetComponent->NotifyParameterChanged(this);
	}

	// Notify parent parameter about change.
	if(ParentParameter && bMarkAndTriggerUpdate)
	{
		ParentParameter->NotifyChildParameterChanged(this);
	}

#endif
}


void
UHoudiniAssetParameter::UnmarkChanged()
{
	bChanged = false;
}


void
UHoudiniAssetParameter::ResetChildParameters()
{
	ChildParameters.Empty();
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


int32
UHoudiniAssetParameter::GetActiveChildParameter() const
{
	return ActiveChildParameter;
}


bool
UHoudiniAssetParameter::HasChildParameters() const
{
	return ChildParameters.Num() > 0;
}


bool
UHoudiniAssetParameter::IsSubstanceParameter() const
{
	return bIsSubstanceParameter;
}


bool
UHoudiniAssetParameter::IsActiveChildParameter(UHoudiniAssetParameter* ChildParam) const
{
	if(ChildParam && (ActiveChildParameter < ChildParameters.Num()))
	{
		return (ChildParameters[ActiveChildParameter] == ChildParam);
	}

	return false;
}


UHoudiniAssetComponent*
UHoudiniAssetParameter::GetHoudiniAssetComponent() const
{
	return HoudiniAssetComponent;
}


UHoudiniAssetParameter*
UHoudiniAssetParameter::GetParentParameter() const
{
	return ParentParameter;
}


UHoudiniAssetInstance*
UHoudiniAssetParameter::GetAssetInstance() const
{
	return HoudiniAssetInstance;
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


#if WITH_EDITOR

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
UHoudiniAssetParameter::CreateNameWidget(FDetailWidgetRow& Row, bool bLabel)
{
	FText ParameterLabelText = FText::FromString(GetParameterLabel());
	const FText& FinalParameterLabelText = bLabel ? ParameterLabelText : FText::GetEmpty();

	if(bIsChildOfMultiparm && ParentParameter)
	{
		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		TSharedRef<SWidget> ClearButton = PropertyCustomizationHelpers::MakeClearButton(
			FSimpleDelegate::CreateUObject(
				(UHoudiniAssetParameterMultiparm*) ParentParameter,
				&UHoudiniAssetParameterMultiparm::RemoveMultiparmInstance,
				MultiparmInstanceIndex),
			LOCTEXT("RemoveMultiparmInstanceToolTip", "Remove"));
		TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton(
			FSimpleDelegate::CreateUObject(
				(UHoudiniAssetParameterMultiparm*) ParentParameter,
				&UHoudiniAssetParameterMultiparm::AddMultiparmInstance,
				MultiparmInstanceIndex),
			LOCTEXT("InsertBeforeMultiparmInstanceToolTip", "Insert Before"));

		if(ChildIndex != 0)
		{
			AddButton.Get().SetVisibility(EVisibility::Hidden);
			ClearButton.Get().SetVisibility(EVisibility::Hidden);
		}

		HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			ClearButton
		];

		HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f)
		[
			AddButton
		];

		HorizontalBox->AddSlot().Padding(2, 5, 5, 2)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(ParameterLabelText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

		Row.NameWidget.Widget = HorizontalBox;
	}
	else
	{
		Row.NameWidget.Widget = SNew(STextBlock)
								.Text(FinalParameterLabelText)
								.ToolTipText(ParameterLabelText)
								.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	}
}

#endif


void
UHoudiniAssetParameter::SetNodeId(HAPI_NodeId InNodeId)
{
	NodeId = InNodeId;
}


bool
UHoudiniAssetParameter::IsSpare() const
{
	return bIsSpare;
}


bool
UHoudiniAssetParameter::IsDisabled() const
{
	return bIsDisabled;
}
