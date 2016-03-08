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
#include "HoudiniAssetParameterRamp.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"
#include "Curves/CurveBase.h"


UHoudiniAssetParameterRampCurveFloat::UHoudiniAssetParameterRampCurveFloat(
	const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetParameterRamp(nullptr)
{

}


#if WITH_EDITOR


void
UHoudiniAssetParameterRampCurveFloat::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	Super::OnCurveChanged(ChangedCurveEditInfos);

	if(HoudiniAssetParameterRamp)
	{
		HoudiniAssetParameterRamp->OnCurveFloatChanged(this);
	}
}


#endif


void
UHoudiniAssetParameterRampCurveFloat::SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp)
{
	HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
}


UHoudiniAssetParameterRampCurveColor::UHoudiniAssetParameterRampCurveColor(
	const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetParameterRamp(nullptr),
	ColorEvent(EHoudiniAssetParameterRampCurveColorEvent::None)
{

}


#if WITH_EDITOR


void
UHoudiniAssetParameterRampCurveColor::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	Super::OnCurveChanged(ChangedCurveEditInfos);

	if(HoudiniAssetParameterRamp)
	{
		HoudiniAssetParameterRamp->OnCurveColorChanged(this);
	}

	// Unfortunately this will not work as SColorGradientEditor is missing OnCurveChange callback calls.
	// This is most likely UE4 bug.
}


#endif


bool
UHoudiniAssetParameterRampCurveColor::Modify(bool bAlwaysMarkDirty)
{
	ColorEvent = GetEditorCurveTransaction();
	return Super::Modify(bAlwaysMarkDirty);
}


EHoudiniAssetParameterRampCurveColorEvent::Type
UHoudiniAssetParameterRampCurveColor::GetEditorCurveTransaction() const
{
	EHoudiniAssetParameterRampCurveColorEvent::Type TransactionType = EHoudiniAssetParameterRampCurveColorEvent::None;

#if WITH_EDITOR

	if(GEditor)
	{
		const FString& TransactionName = GEditor->GetTransactionName().ToString();

		if(TransactionName.Equals(TEXT("Move Gradient Stop")))
		{
			TransactionType = EHoudiniAssetParameterRampCurveColorEvent::MoveStop;
		}
		else if(TransactionName.Equals(TEXT("Add Gradient Stop")))
		{
			TransactionType = EHoudiniAssetParameterRampCurveColorEvent::AddStop;
		}
		else if(TransactionName.Equals(TEXT("Delete Gradient Stop")))
		{
			TransactionType = EHoudiniAssetParameterRampCurveColorEvent::RemoveStop;
		}
		else if(TransactionName.Equals(TEXT("Change Gradient Stop Time")))
		{
			TransactionType = EHoudiniAssetParameterRampCurveColorEvent::ChangeStopTime;
		}
		else if(TransactionName.Equals(TEXT("Change Gradient Stop Color")))
		{
			TransactionType = EHoudiniAssetParameterRampCurveColorEvent::ChangeStopColor;
		}
		else
		{
			TransactionType = EHoudiniAssetParameterRampCurveColorEvent::None;
		}
	}
	else
	{
		TransactionType = EHoudiniAssetParameterRampCurveColorEvent::None;
	}

#endif

	return TransactionType;
}


void
UHoudiniAssetParameterRampCurveColor::SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp)
{
	HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
}


EHoudiniAssetParameterRampCurveColorEvent::Type
UHoudiniAssetParameterRampCurveColor::GetColorEvent() const
{
	return ColorEvent;
}


void
UHoudiniAssetParameterRampCurveColor::ResetColorEvent()
{
	ColorEvent = EHoudiniAssetParameterRampCurveColorEvent::None;
}


bool
UHoudiniAssetParameterRampCurveColor::IsTickableInEditor() const
{
	return true;
}


bool
UHoudiniAssetParameterRampCurveColor::IsTickableWhenPaused() const
{
	return true;
}


void
UHoudiniAssetParameterRampCurveColor::Tick(float DeltaTime)
{
	if(HoudiniAssetParameterRamp)
	{

#if WITH_EDITOR

		if(GEditor && !GEditor->IsTransactionActive())
		{
			switch(ColorEvent)
			{
				case EHoudiniAssetParameterRampCurveColorEvent::ChangeStopTime:
				case EHoudiniAssetParameterRampCurveColorEvent::ChangeStopColor:
				{
					// If color picker is open, we need to wait until it is closed.
					TSharedPtr<SWindow> ActiveTopLevelWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
					if(ActiveTopLevelWindow.IsValid())
					{
						const FString& ActiveTopLevelWindowTitle = ActiveTopLevelWindow->GetTitle().ToString();
						if(ActiveTopLevelWindowTitle.Equals(TEXT("Color Picker")))
						{
							return;
						}
					}
				}

				default:
				{
					break;
				}
			}
		}

		// Notify parent ramp parameter that color has changed.
		HoudiniAssetParameterRamp->OnCurveColorChanged(this);

#endif

	}
	else
	{
		// If we are ticking for whatever reason, stop.
		ResetColorEvent();
	}
}


TStatId
UHoudiniAssetParameterRampCurveColor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHoudiniAssetParameterRampCurveColor, STATGROUP_Tickables);
}


bool
UHoudiniAssetParameterRampCurveColor::IsTickable() const
{
#if WITH_EDITOR

	if(GEditor)
	{
		return EHoudiniAssetParameterRampCurveColorEvent::None != ColorEvent;
	}

#endif

	return false;
}


#if WITH_EDITOR


/** We need to inherit from curve editor in order to get subscription to mouse events. **/
class HOUDINIENGINERUNTIME_API SHoudiniAssetParameterRampCurveEditor : public SCurveEditor
{

public:

	SLATE_BEGIN_ARGS(SHoudiniAssetParameterRampCurveEditor)
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(10.0f)
		, _ViewMinOutput(0.0f)
		, _ViewMaxOutput(1.0f)
		, _XAxisName()
		, _YAxisName()
		, _HideUI(true)
		, _DrawCurve(true)
		, _InputSnap(0.1f)
		, _OutputSnap(0.05f)
		, _SnappingEnabled(false)
		, _TimelineLength(5.0f)
		, _DesiredSize(FVector2D::ZeroVector)
		, _AllowZoomOutput(true)
		, _AlwaysDisplayColorCurves(false)
		, _ZoomToFitVertical(true)
		, _ZoomToFitHorizontal(true)
		, _ShowZoomButtons(true)
		, _ShowInputGridNumbers(true)
		, _ShowOutputGridNumbers(true)
		, _ShowCurveSelector(true)
		, _GridColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
	{}
		SLATE_ATTRIBUTE(float, ViewMinInput)
		SLATE_ATTRIBUTE(float, ViewMaxInput)
		SLATE_ATTRIBUTE(float, ViewMinOutput)
		SLATE_ATTRIBUTE(float, ViewMaxOutput)
		SLATE_ARGUMENT(TOptional<FString>, XAxisName)
		SLATE_ARGUMENT(TOptional<FString>, YAxisName)
		SLATE_ARGUMENT(bool, HideUI)
		SLATE_ARGUMENT(bool, DrawCurve)
		SLATE_ATTRIBUTE(float, InputSnap)
		SLATE_ATTRIBUTE(float, OutputSnap)
		SLATE_ATTRIBUTE(bool, SnappingEnabled)
		SLATE_ATTRIBUTE(float, TimelineLength)
		SLATE_ATTRIBUTE(FVector2D, DesiredSize)
		SLATE_ARGUMENT(bool, AllowZoomOutput)
		SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
		SLATE_ARGUMENT(bool, ZoomToFitVertical)
		SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
		SLATE_ARGUMENT(bool, ShowZoomButtons)
		SLATE_ARGUMENT(bool, ShowInputGridNumbers)
		SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
		SLATE_ARGUMENT(bool, ShowCurveSelector)
		SLATE_ARGUMENT(FLinearColor, GridColor)
	SLATE_END_ARGS()

public:

	/** Widget construction. **/
	void Construct(const FArguments& InArgs);

protected:

	/** Handle mouse up events. **/
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

public:

	/** Set parent ramp parameter. **/
	void SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp);

protected:

	/** Parent ramp parameter. **/
	UHoudiniAssetParameterRamp* HoudiniAssetParameterRamp;
};


void
SHoudiniAssetParameterRampCurveEditor::Construct(const FArguments& InArgs)
{
	SCurveEditor::Construct(SCurveEditor::FArguments()
		.ViewMinInput(InArgs._ViewMinInput)
		.ViewMaxInput(InArgs._ViewMaxInput)
		.ViewMinOutput(InArgs._ViewMinOutput)
		.ViewMaxOutput(InArgs._ViewMaxOutput)
		.XAxisName(InArgs._XAxisName)
		.YAxisName(InArgs._YAxisName)
		.HideUI(InArgs._HideUI)
		.DrawCurve(InArgs._DrawCurve)
		.TimelineLength(InArgs._TimelineLength)
		.AllowZoomOutput(InArgs._AllowZoomOutput)
		.ShowInputGridNumbers(InArgs._ShowInputGridNumbers)
		.ShowOutputGridNumbers(InArgs._ShowOutputGridNumbers)
		.ShowZoomButtons(InArgs._ShowZoomButtons)
		.ZoomToFitHorizontal(InArgs._ZoomToFitHorizontal)
		.ZoomToFitVertical(InArgs._ZoomToFitVertical)
		);

	HoudiniAssetParameterRamp = nullptr;

	UCurveEditorSettings* CurveEditorSettings = GetSettings();
	if(CurveEditorSettings)
	{
		CurveEditorSettings->SetCurveVisibility(ECurveEditorCurveVisibility::AllCurves);
		CurveEditorSettings->SetTangentVisibility(ECurveEditorTangentVisibility::NoTangents);
	}
}


void
SHoudiniAssetParameterRampCurveEditor::SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp)
{
	HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
}


FReply
SHoudiniAssetParameterRampCurveEditor::OnMouseButtonUp(const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	FReply Reply = SCurveEditor::OnMouseButtonUp(MyGeometry, MouseEvent);

	if(HoudiniAssetParameterRamp)
	{
		HoudiniAssetParameterRamp->OnCurveEditingFinished();
	}

	return Reply;
}


#endif


const EHoudiniAssetParameterRampKeyInterpolation::Type
UHoudiniAssetParameterRamp::DefaultSplineInterpolation = EHoudiniAssetParameterRampKeyInterpolation::MonotoneCubic;


const EHoudiniAssetParameterRampKeyInterpolation::Type
UHoudiniAssetParameterRamp::DefaultUnknownInterpolation = EHoudiniAssetParameterRampKeyInterpolation::Linear;


UHoudiniAssetParameterRamp::UHoudiniAssetParameterRamp(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetParameterRampCurveFloat(nullptr),
	HoudiniAssetParameterRampCurveColor(nullptr),
	bIsFloatRamp(true),
	bIsCurveChanged(false),
	bIsCurveUploadRequired(false)
{

}


UHoudiniAssetParameterRamp::~UHoudiniAssetParameterRamp()
{

}


UHoudiniAssetParameterRamp*
UHoudiniAssetParameterRamp::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterRamp* HoudiniAssetParameterRamp =
		NewObject<UHoudiniAssetParameterRamp>(Outer, UHoudiniAssetParameterRamp::StaticClass(), NAME_None,
			RF_Public | RF_Transactional);

	HoudiniAssetParameterRamp->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);

	return HoudiniAssetParameterRamp;
}


bool
UHoudiniAssetParameterRamp::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	if(HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType)
	{
		bIsFloatRamp = true;
	}
	else if(HAPI_RAMPTYPE_COLOR == ParmInfo.rampType)
	{
		bIsFloatRamp = false;
	}
	else
	{
		return false;
	}

	return true;
}


void
UHoudiniAssetParameterRamp::NotifyChildParametersCreated()
{
	if(bIsCurveUploadRequired)
	{
		bIsCurveChanged = true;
		OnCurveEditingFinished();
		bIsCurveUploadRequired = false;
	}
	else
	{
		GenerateCurvePoints();

#if WITH_EDITOR

		if(HoudiniAssetParameterRampCurveColor)
		{
			HoudiniAssetComponent->UpdateEditorProperties(false);
		}

#endif

	}
}


#if WITH_EDITOR

void
UHoudiniAssetParameterRamp::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
	
	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);
	CurveEditor.Reset();

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	FString CurveAxisTextX = TEXT("");
	FString CurveAxisTextY = TEXT("");
	UClass* CurveClass = nullptr;

	if(bIsFloatRamp)
	{
		CurveAxisTextX = TEXT(HAPI_UNREAL_RAMP_FLOAT_AXIS_X);
		CurveAxisTextY = TEXT(HAPI_UNREAL_RAMP_FLOAT_AXIS_Y);
		CurveClass = UHoudiniAssetParameterRampCurveFloat::StaticClass();
	}
	else
	{
		CurveAxisTextX = TEXT(HAPI_UNREAL_RAMP_COLOR_AXIS_X);
		CurveAxisTextY = TEXT(HAPI_UNREAL_RAMP_COLOR_AXIS_Y);
		CurveClass = UHoudiniAssetParameterRampCurveColor::StaticClass();
	}

	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SNew(SBorder)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(CurveEditor, SHoudiniAssetParameterRampCurveEditor)
			.ViewMinInput(0.0f)
			.ViewMaxInput(1.0f)
			.HideUI(true)
			.DrawCurve(true)
			.ViewMinInput(0.0f)
			.ViewMaxInput(1.0f)
			.ViewMinOutput(0.0f)
			.ViewMaxOutput(1.0f)
			.TimelineLength(1.0f)
			.AllowZoomOutput(false)
			.ShowInputGridNumbers(false)
			.ShowOutputGridNumbers(false)
			.ShowZoomButtons(false)
			.ZoomToFitHorizontal(false)
			.ZoomToFitVertical(false)
			.XAxisName(CurveAxisTextX)
			.YAxisName(CurveAxisTextY)
			.ShowCurveSelector(false)
		]
	];

	// Set callback for curve editor events.
	if(CurveEditor.IsValid())
	{
		CurveEditor->SetParentRampParameter(this);
	}

	if(bIsFloatRamp)
	{
		if(!HoudiniAssetParameterRampCurveFloat)
		{
			HoudiniAssetParameterRampCurveFloat = Cast<UHoudiniAssetParameterRampCurveFloat>(
				NewObject<UHoudiniAssetParameterRampCurveFloat>(
					HoudiniAssetComponent, UHoudiniAssetParameterRampCurveFloat::StaticClass(), NAME_None,
						RF_Transactional | RF_Public));

			HoudiniAssetParameterRampCurveFloat->SetParentRampParameter(this);
		}

		// Set curve values.
		GenerateCurvePoints();

		// Set the curve that is being edited.
		CurveEditor->SetCurveOwner(HoudiniAssetParameterRampCurveFloat, true);
	}
	else
	{
		if(!HoudiniAssetParameterRampCurveColor)
		{
			HoudiniAssetParameterRampCurveColor = Cast<UHoudiniAssetParameterRampCurveColor>(
				NewObject<UHoudiniAssetParameterRampCurveColor>(
					HoudiniAssetComponent, UHoudiniAssetParameterRampCurveColor::StaticClass(), NAME_None,
						RF_Transactional | RF_Public));

			HoudiniAssetParameterRampCurveColor->SetParentRampParameter(this);
		}

		// Set curve values.
		GenerateCurvePoints();

		// Set the curve that is being edited.
		CurveEditor->SetCurveOwner(HoudiniAssetParameterRampCurveColor, true);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	// Bypass multiparm widget creation.
	UHoudiniAssetParameter::CreateWidget(DetailCategoryBuilder);
}


#endif


void
UHoudiniAssetParameterRamp::OnCurveFloatChanged(UHoudiniAssetParameterRampCurveFloat* CurveFloat)
{
	if(!CurveFloat)
	{
		return;
	}

	FRichCurve& RichCurve = CurveFloat->FloatCurve;

	if(RichCurve.GetNumKeys() < MultiparmValue)
	{
		// Keys have been removed.
		bIsCurveUploadRequired = true;
		RemoveElements(MultiparmValue - RichCurve.GetNumKeys());
	}
	else if(RichCurve.GetNumKeys() > MultiparmValue)
	{
		// Keys have been added.
		bIsCurveUploadRequired = true;
		AddElements(RichCurve.GetNumKeys() - MultiparmValue);
	}
	else
	{
		// We have curve point modification.
		bIsCurveChanged = true;
	}
}


void
UHoudiniAssetParameterRamp::OnCurveColorChanged(UHoudiniAssetParameterRampCurveColor* CurveColor)
{
	if(!CurveColor)
	{
		return;
	}

	EHoudiniAssetParameterRampCurveColorEvent::Type ColorEvent = CurveColor->GetColorEvent();
	switch(ColorEvent)
	{
		case EHoudiniAssetParameterRampCurveColorEvent::AddStop:
		{
			bIsCurveUploadRequired = true;
			AddElement();
			break;
		}

		case EHoudiniAssetParameterRampCurveColorEvent::RemoveStop:
		{
			bIsCurveUploadRequired = true;
			RemoveElement();
			break;
		}

		case EHoudiniAssetParameterRampCurveColorEvent::ChangeStopTime:
		case EHoudiniAssetParameterRampCurveColorEvent::ChangeStopColor:
		case EHoudiniAssetParameterRampCurveColorEvent::MoveStop:
		{
			// We have curve point modification.
			bIsCurveChanged = true;
			OnCurveEditingFinished();
			break;
		}

		default:
		{
			break;
		}
	}

	CurveColor->ResetColorEvent();
}


void
UHoudiniAssetParameterRamp::OnCurveEditingFinished()
{
	if(bIsCurveChanged)
	{
		if(MultiparmValue * 3 != ChildParameters.Num())
		{
			return;
		}

		bIsCurveChanged = false;

		if(HoudiniAssetParameterRampCurveFloat)
		{
			FRichCurve& RichCurve = HoudiniAssetParameterRampCurveFloat->FloatCurve;

			MarkPreChanged();

			// We need to update ramp key positions.
			for(int32 KeyIdx = 0, KeyNum = RichCurve.GetNumKeys(); KeyIdx < KeyNum; ++KeyIdx)
			{
				UHoudiniAssetParameterFloat* ChildParamPosition = nullptr;
				UHoudiniAssetParameterFloat* ChildParamValue = nullptr;
				UHoudiniAssetParameterChoice* ChildParamInterpolation = nullptr;

				if(!GetRampKeysCurveFloat(KeyIdx, ChildParamPosition, ChildParamValue, ChildParamInterpolation))
				{
					continue;
				}

				const FRichCurveKey& RichCurveKey = RichCurve.Keys[KeyIdx];

				ChildParamPosition->SetValue(RichCurveKey.Time, 0, false, false);
				ChildParamValue->SetValue(RichCurveKey.Value, 0, false, false);

				EHoudiniAssetParameterRampKeyInterpolation::Type RichCurveKeyInterpolation =
					TranslateUnrealRampKeyInterpolation(RichCurveKey.InterpMode);

				ChildParamInterpolation->SetValueInt((int32) RichCurveKeyInterpolation, false, false);
			}

			MarkChanged();
		}
		else if(HoudiniAssetParameterRampCurveColor)
		{
			FRichCurve& RichCurveR = HoudiniAssetParameterRampCurveColor->FloatCurves[0];
			FRichCurve& RichCurveG = HoudiniAssetParameterRampCurveColor->FloatCurves[1];
			FRichCurve& RichCurveB = HoudiniAssetParameterRampCurveColor->FloatCurves[2];
			FRichCurve& RichCurveA = HoudiniAssetParameterRampCurveColor->FloatCurves[3];

			MarkPreChanged();

			// We need to update ramp key positions.
			for(int32 KeyIdx = 0, KeyNum = RichCurveR.GetNumKeys(); KeyIdx < KeyNum; ++KeyIdx)
			{
				UHoudiniAssetParameterFloat* ChildParamPosition = nullptr;
				UHoudiniAssetParameterColor* ChildParamColor = nullptr;
				UHoudiniAssetParameterChoice* ChildParamInterpolation = nullptr;

				if(!GetRampKeysCurveColor(KeyIdx, ChildParamPosition, ChildParamColor, ChildParamInterpolation))
				{
					continue;
				}

				const FRichCurveKey& RichCurveKeyR = RichCurveR.Keys[KeyIdx];
				const FRichCurveKey& RichCurveKeyG = RichCurveG.Keys[KeyIdx];
				const FRichCurveKey& RichCurveKeyB = RichCurveB.Keys[KeyIdx];
				//const FRichCurveKey& RichCurveKeyA = RichCurveA.Keys[KeyIdx];

				ChildParamPosition->SetValue(RichCurveKeyR.Time, 0, false, false);

				FLinearColor KeyColor(RichCurveKeyR.Value, RichCurveKeyG.Value, RichCurveKeyB.Value, 1.0f);
				ChildParamColor->OnPaintColorChanged(KeyColor, false, false);

				EHoudiniAssetParameterRampKeyInterpolation::Type RichCurveKeyInterpolation =
					TranslateUnrealRampKeyInterpolation(RichCurveKeyR.InterpMode);

				ChildParamInterpolation->SetValueInt((int32) RichCurveKeyInterpolation, false, false);
			}

			MarkChanged();
		}
	}
}


void
UHoudiniAssetParameterRamp::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetParameterRamp* HoudiniAssetParameterRamp = Cast<UHoudiniAssetParameterRamp>(InThis);
	if(HoudiniAssetParameterRamp)
	{
		if(HoudiniAssetParameterRamp->HoudiniAssetParameterRampCurveFloat)
		{
			Collector.AddReferencedObject(HoudiniAssetParameterRamp->HoudiniAssetParameterRampCurveFloat, InThis);
		}

		if(HoudiniAssetParameterRamp->HoudiniAssetParameterRampCurveColor)
		{
			Collector.AddReferencedObject(HoudiniAssetParameterRamp->HoudiniAssetParameterRampCurveColor, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetParameterRamp::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << HoudiniAssetParameterRampCurveFloat;
	Ar << HoudiniAssetParameterRampCurveColor;

	Ar << bIsFloatRamp;

	if(Ar.IsLoading())
	{
		bIsCurveChanged = false;
		bIsCurveUploadRequired = false;
	}
}


void
UHoudiniAssetParameterRamp::PostLoad()
{
	Super::PostLoad();

	if(HoudiniAssetParameterRampCurveFloat)
	{
		HoudiniAssetParameterRampCurveFloat->SetParentRampParameter(this);
	}

	if(HoudiniAssetParameterRampCurveColor)
	{
		HoudiniAssetParameterRampCurveColor->SetParentRampParameter(this);
	}
}


void
UHoudiniAssetParameterRamp::BeginDestroy()
{

#if WITH_EDITOR

	if(CurveEditor.IsValid())
	{
		CurveEditor->SetCurveOwner(nullptr);
		CurveEditor.Reset();
	}

#endif

	Super::BeginDestroy();
}


void
UHoudiniAssetParameterRamp::GenerateCurvePoints()
{
	if(ChildParameters.Num() % 3 != 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Invalid Ramp parameter [%s] : Number of child parameters is not a tuple of 3."),
			*ParameterName);

		return;
	}

	if(HoudiniAssetParameterRampCurveFloat)
	{
		HoudiniAssetParameterRampCurveFloat->ResetCurve();

		for(int32 ChildIdx = 0, ChildNum = GetRampKeyCount(); ChildIdx < ChildNum; ++ChildIdx)
		{
			UHoudiniAssetParameterFloat* ChildParamPosition = nullptr;
			UHoudiniAssetParameterFloat* ChildParamValue = nullptr;
			UHoudiniAssetParameterChoice* ChildParamInterpolation = nullptr;

			if(!GetRampKeysCurveFloat(ChildIdx, ChildParamPosition, ChildParamValue, ChildParamInterpolation))
			{
				HoudiniAssetParameterRampCurveColor->ResetCurve();
				return;
			}

			float CurveKeyPosition = ChildParamPosition->GetParameterValue(0, 0.0f);
			float CurveKeyValue = ChildParamValue->GetParameterValue(0, 0.0f);
			EHoudiniAssetParameterRampKeyInterpolation::Type RampKeyInterpolation =
				TranslateChoiceKeyInterpolation(ChildParamInterpolation);
			ERichCurveInterpMode RichCurveInterpMode = TranslateHoudiniRampKeyInterpolation(RampKeyInterpolation);

			FRichCurve& RichCurve = HoudiniAssetParameterRampCurveFloat->FloatCurve;

			FKeyHandle const KeyHandle = RichCurve.AddKey(CurveKeyPosition, CurveKeyValue);
			RichCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
		}
	}
	else if(HoudiniAssetParameterRampCurveColor)
	{
		HoudiniAssetParameterRampCurveColor->ResetCurve();

		for(int32 ChildIdx = 0, ChildNum = GetRampKeyCount(); ChildIdx < ChildNum; ++ChildIdx)
		{
			UHoudiniAssetParameterFloat* ChildParamPosition = nullptr;
			UHoudiniAssetParameterColor* ChildParamColor = nullptr;
			UHoudiniAssetParameterChoice* ChildParamInterpolation = nullptr;

			if(!GetRampKeysCurveColor(ChildIdx, ChildParamPosition, ChildParamColor, ChildParamInterpolation))
			{
				HoudiniAssetParameterRampCurveColor->ResetCurve();
				return;
			}

			float CurveKeyPosition = ChildParamPosition->GetParameterValue(0, 0.0f);
			FLinearColor CurveKeyValue = ChildParamColor->GetColor();
			EHoudiniAssetParameterRampKeyInterpolation::Type RampKeyInterpolation =
				TranslateChoiceKeyInterpolation(ChildParamInterpolation);
			ERichCurveInterpMode RichCurveInterpMode = TranslateHoudiniRampKeyInterpolation(RampKeyInterpolation);

			for(int CurveIdx = 0; CurveIdx < 4; ++CurveIdx)
			{
				FRichCurve& RichCurve = HoudiniAssetParameterRampCurveColor->FloatCurves[CurveIdx];

				FKeyHandle const KeyHandle =
					RichCurve.AddKey(CurveKeyPosition, CurveKeyValue.Component(CurveIdx));
				RichCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
			}
		}
	}
}


bool
UHoudiniAssetParameterRamp::GetRampKeysCurveFloat(int32 Idx, UHoudiniAssetParameterFloat*& Position,
	UHoudiniAssetParameterFloat*& Value, UHoudiniAssetParameterChoice*& Interp) const
{
	Position = nullptr;
	Value = nullptr;
	Interp = nullptr;

	int32 NumChildren = ChildParameters.Num();

	if(3 * Idx + 0 < NumChildren)
	{
		Position = Cast<UHoudiniAssetParameterFloat>(ChildParameters[3 * Idx + 0]);
	}

	if(3 * Idx + 1 < NumChildren)
	{
		Value = Cast<UHoudiniAssetParameterFloat>(ChildParameters[3 * Idx + 1]);
	}

	if(3 * Idx + 2 < NumChildren)
	{
		Interp = Cast<UHoudiniAssetParameterChoice>(ChildParameters[3 * Idx + 2]);
	}

	return Position != nullptr && Value != nullptr && Interp != nullptr;
}


bool
UHoudiniAssetParameterRamp::GetRampKeysCurveColor(int32 Idx, UHoudiniAssetParameterFloat*& Position,
	UHoudiniAssetParameterColor*& Value, UHoudiniAssetParameterChoice*& Interp) const
{
	Position = nullptr;
	Value = nullptr;
	Interp = nullptr;

	int32 NumChildren = ChildParameters.Num();

	if(3 * Idx + 0 < NumChildren)
	{
		Position = Cast<UHoudiniAssetParameterFloat>(ChildParameters[3 * Idx + 0]);
	}

	if(3 * Idx + 1 < NumChildren)
	{
		Value = Cast<UHoudiniAssetParameterColor>(ChildParameters[3 * Idx + 1]);
	}

	if(3 * Idx + 2 < NumChildren)
	{
		Interp = Cast<UHoudiniAssetParameterChoice>(ChildParameters[3 * Idx + 2]);
	}

	return Position != nullptr && Value != nullptr && Interp != nullptr;
}


int32
UHoudiniAssetParameterRamp::GetRampKeyCount() const
{
	int32 ChildParamCount = ChildParameters.Num();

	if(ChildParamCount % 3 != 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Invalid Ramp parameter [%s] : Number of child parameters is not a tuple of 3."),
			*ParameterName);

		return 0;
	}

	return ChildParamCount / 3;
}


EHoudiniAssetParameterRampKeyInterpolation::Type
UHoudiniAssetParameterRamp::TranslateChoiceKeyInterpolation(UHoudiniAssetParameterChoice* ChoiceParam) const
{
	EHoudiniAssetParameterRampKeyInterpolation::Type ChoiceInterpolationValue =
		UHoudiniAssetParameterRamp::DefaultUnknownInterpolation;

	if(ChoiceParam)
	{
		if(ChoiceParam->IsStringChoiceList())
		{
			const FString& ChoiceValueString = ChoiceParam->GetParameterValueString();

			if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_CONSTANT)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::Constant;
			}
			else if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_LINEAR)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::Linear;
			}
			else if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_CATMULL_ROM)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::CatmullRom;
			}
			else if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_MONOTONE_CUBIC)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::MonotoneCubic;
			}
			else if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_BEZIER)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::Bezier;
			}
			else if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_B_SPLINE)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::BSpline;
			}
			else if(ChoiceValueString.Equals(TEXT(HAPI_UNREAL_RAMP_KEY_INTERPOLATION_HERMITE)))
			{
				ChoiceInterpolationValue = EHoudiniAssetParameterRampKeyInterpolation::Hermite;
			}
		}
		else
		{
			int ChoiceValueInt = ChoiceParam->GetParameterValueInt();
			if(ChoiceValueInt >= 0 || ChoiceValueInt <= 6)
			{
				ChoiceInterpolationValue = (EHoudiniAssetParameterRampKeyInterpolation::Type) ChoiceValueInt;
			}
		}
	}

	return ChoiceInterpolationValue;
}


ERichCurveInterpMode
UHoudiniAssetParameterRamp::TranslateHoudiniRampKeyInterpolation(
	EHoudiniAssetParameterRampKeyInterpolation::Type KeyInterpolation) const
{
	switch(KeyInterpolation)
	{
		case EHoudiniAssetParameterRampKeyInterpolation::Constant:
		{
			return ERichCurveInterpMode::RCIM_Constant;
		}

		case EHoudiniAssetParameterRampKeyInterpolation::Linear:
		{
			return ERichCurveInterpMode::RCIM_Linear;
		}

		default:
		{
			break;
		}
	}

	return ERichCurveInterpMode::RCIM_Cubic;
}


EHoudiniAssetParameterRampKeyInterpolation::Type
UHoudiniAssetParameterRamp::TranslateUnrealRampKeyInterpolation(ERichCurveInterpMode RichCurveInterpMode) const
{
	switch(RichCurveInterpMode)
	{
		case ERichCurveInterpMode::RCIM_Constant:
		{
			return EHoudiniAssetParameterRampKeyInterpolation::Constant;
		}

		case ERichCurveInterpMode::RCIM_Linear:
		{
			return EHoudiniAssetParameterRampKeyInterpolation::Linear;
		}

		case ERichCurveInterpMode::RCIM_Cubic:
		{
			return UHoudiniAssetParameterRamp::DefaultSplineInterpolation;
		}

		case ERichCurveInterpMode::RCIM_None:
		default:
		{
			break;
		}
	}

	return UHoudiniAssetParameterRamp::DefaultUnknownInterpolation;
}

