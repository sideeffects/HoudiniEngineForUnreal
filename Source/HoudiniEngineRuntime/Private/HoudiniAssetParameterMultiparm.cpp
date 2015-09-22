/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterMultiparm::UHoudiniAssetParameterMultiparm(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	Value(0),
	LastModificationType(RegularValueChange),
	LastRemoveAddInstanceIndex(-1)
{

}


UHoudiniAssetParameterMultiparm::~UHoudiniAssetParameterMultiparm()
{

}


UHoudiniAssetParameterMultiparm*
UHoudiniAssetParameterMultiparm::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterMultiparm* HoudiniAssetParameterMultiparm = NewObject<UHoudiniAssetParameterMultiparm>(Outer,
		UHoudiniAssetParameterMultiparm::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterMultiparm->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterMultiparm;
}


bool
UHoudiniAssetParameterMultiparm::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	if(HAPI_PARMTYPE_MULTIPARMLIST != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	// Get the actual value for this property.
	Value = 0;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(nullptr, InNodeId, &Value, ValuesIndex, 1))
	{
		return false;
	}

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterMultiparm::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
	
	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	TSharedPtr<SNumericEntryBox<int32> > NumericEntryBox;

	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(NumericEntryBox, SNumericEntryBox<int32>)
		.AllowSpin(true)

		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

		.Value(TAttribute<TOptional<int32> >::Create(TAttribute<TOptional<int32> >::FGetter::CreateUObject(this,
			&UHoudiniAssetParameterMultiparm::GetValue)))
		.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(this,
			&UHoudiniAssetParameterMultiparm::SetValue))
		.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateUObject(this,
			&UHoudiniAssetParameterMultiparm::SetValueCommitted))
	];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
	[
		PropertyCustomizationHelpers::MakeAddButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterMultiparm::AddElement),
			LOCTEXT("AddAnotherInstanceToolTip", "Add Another Instance"))
	];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
	[
		PropertyCustomizationHelpers::MakeRemoveButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterMultiparm::RemoveElement),
			LOCTEXT("RemoveLastInstanceToolTip", "Remove Last Instance"))
	];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
	[
		PropertyCustomizationHelpers::MakeEmptyButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterMultiparm::SetValue, 0),
			LOCTEXT("ClearAllInstanesToolTip", "Clear All Instances"))
	];

	if(NumericEntryBox.IsValid())
	{
		NumericEntryBox->SetEnabled(!bIsDisabled);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	Super::CreateWidget(DetailCategoryBuilder);
}

void
UHoudiniAssetParameterMultiparm::AddMultiparmInstance(int32 ChildMultiparmInstanceIndex)
{
	// Set the last modification type and instance index before the current state
	// is saved by Modify().
	LastModificationType = InstanceAdded;
	LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex - 1; // Added above the current one.

	// Record undo information.
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Adding instance"),
		HoudiniAssetComponent);
	Modify();

	MarkPreChanged();

	FHoudiniApi::InsertMultiparmInstance(
		FHoudiniEngine::Get().GetSession(), NodeId, ParmId, ChildMultiparmInstanceIndex);
	Value++;

	// Save the Redo modification type (should be the opposite operation to this one).
	LastModificationType = InstanceRemoved;
	LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex;

	// Mark this parameter as changed.
	MarkChanged();
}

void
UHoudiniAssetParameterMultiparm::RemoveMultiparmInstance(int32 ChildMultiparmInstanceIndex)
{
	// Set the last modification type and instance index before the current state
	// is saved by Modify().
	LastModificationType = InstanceRemoved;
	LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex;

	// Record undo information.
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Removing instance"),
		HoudiniAssetComponent);
	Modify();

	MarkPreChanged();

	FHoudiniApi::RemoveMultiparmInstance(
		FHoudiniEngine::Get().GetSession(), NodeId, ParmId, ChildMultiparmInstanceIndex);
	Value--;

	// Save the Redo modification type (should be the opposite operation to this one).
	LastModificationType = InstanceAdded;
	LastRemoveAddInstanceIndex = ChildMultiparmInstanceIndex;

	// Mark this parameter as changed.
	MarkChanged();
}

#endif

bool
UHoudiniAssetParameterMultiparm::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value, ValuesIndex, 1))
	{
		return false;
	}

	return Super::UploadParameterValue();
}

#if WITH_EDITOR

TOptional<int32>
UHoudiniAssetParameterMultiparm::GetValue() const
{
	return TOptional<int32>(Value);
}


void
UHoudiniAssetParameterMultiparm::SetValue(int32 InValue)
{
	if(Value != InValue)
	{
		LastModificationType = RegularValueChange;
		LastRemoveAddInstanceIndex = -1;

		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Changing a value"),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		Value = InValue;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


void
UHoudiniAssetParameterMultiparm::SetValueCommitted(int32 InValue, ETextCommit::Type CommitType)
{

}

void
UHoudiniAssetParameterMultiparm::AddElement()
{
	LastModificationType = RegularValueChange;
	LastRemoveAddInstanceIndex = -1;

	// Record undo information.
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Changing a value"),
		HoudiniAssetComponent);
	Modify();

	MarkPreChanged();

	Value++;

	MarkChanged();
}

void
UHoudiniAssetParameterMultiparm::RemoveElement()
{
	LastModificationType = RegularValueChange;
	LastRemoveAddInstanceIndex = -1;

	// Record undo information.
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterMultiparmChange", "Houdini Parameter Multiparm: Changing a value"),
		HoudiniAssetComponent);
	Modify();

	MarkPreChanged();

	Value--;

	MarkChanged();
}

#endif

void
UHoudiniAssetParameterMultiparm::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	if(Ar.IsTransacting())
	{
		SerializeEnumeration(Ar, LastModificationType);
		Ar << LastRemoveAddInstanceIndex;
	}

	if(Ar.IsLoading())
	{
		Value = 0;
	}

	Ar << Value;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterMultiparm::PostEditUndo()
{
	if(LastModificationType == InstanceAdded)
	{
		FHoudiniApi::RemoveMultiparmInstance(
			FHoudiniEngine::Get().GetSession(), NodeId, ParmId, LastRemoveAddInstanceIndex);
	}
	else if(LastModificationType == InstanceRemoved)
	{
		FHoudiniApi::InsertMultiparmInstance(
			FHoudiniEngine::Get().GetSession(), NodeId, ParmId, LastRemoveAddInstanceIndex);
	}

	Super::PostEditUndo();
}

#endif
