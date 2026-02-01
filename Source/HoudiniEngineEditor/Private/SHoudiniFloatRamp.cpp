/*
* Copyright (c) <2024> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "SHoudiniFloatRamp.h"

#include "HoudiniParameterChoice.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterRamp.h"
#include "Editor/CurveEditor/Public/CurveEditorSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniEngineUtils.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
SHoudiniFloatRampCurveEditor::Construct(const FArguments& InArgs)
{
	Curve = NewObject<UCurveFloat>(
		GetTransientPackage(),
		UCurveFloat::StaticClass(),
		NAME_None,
		RF_Transactional | RF_Public);

	if (!Curve)
	{
		return;
	}

	// Add the ramp curve to root to avoid garbage collected
	Curve->AddToRoot();

	OnUpdateCurveDelegateHandle = Curve->OnUpdateCurve.AddRaw(
		this, &SHoudiniFloatRampCurveEditor::OnUpdateCurve);

	RampView = InArgs._RampView;
	OnCurveChangedDelegate = InArgs._OnCurveChanged;

	SCurveEditor::Construct(
		SCurveEditor::FArguments()
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
		.ZoomToFitVertical(InArgs._ZoomToFitVertical));

	this->SetRequireFocusToZoom(true);

	UCurveEditorSettings* CurveEditorSettings = GetSettings();
	if (CurveEditorSettings)
	{
		CurveEditorSettings->SetTangentVisibility(ECurveEditorTangentVisibility::NoTangents);
	}

	// Avoid showing tooltips inside of the curve editor
	EnableToolTipForceField(true);

	SetCurveOwner(Curve);

	RefreshCurveKeys();
}

SHoudiniFloatRampCurveEditor::~SHoudiniFloatRampCurveEditor()
{
	if (Curve)
	{
		SetCurveOwner(nullptr);

		Curve->OnUpdateCurve.Remove(OnUpdateCurveDelegateHandle);

		// Remove the ramp curve to root so it can be garbage collected
		Curve->RemoveFromRoot();
	}
}

void 
SHoudiniFloatRampCurveEditor::OnUpdateCurve(UCurveBase*, EPropertyChangeType::Type)
{	
	if (bIsMouseButtonDown)
	{
		return; // See comment in declaration of bIsMouseButtonDown
	}

	OnCurveChanged();
}

void 
SHoudiniFloatRampCurveEditor::RefreshCurveKeys()
{
	if (!RampView.IsValid())
	{
		return;
	}

	if (!Curve)
	{
		return;
	}

	FRichCurve& FloatCurve = Curve->FloatCurve;

	FloatCurve.Reset();

	const int32 PointCount = RampView->GetPointCount();
	for (int32 i = 0; i < PointCount; ++i)
	{
		ERichCurveInterpMode RichCurveInterpMode =
			UHoudiniParameter::EHoudiniRampInterpolationTypeToERichCurveInterpMode(
				RampView->GetRampPointInterpolationType(i).GetValue());

		const FKeyHandle KeyHandle = FloatCurve.AddKey(
			RampView->GetRampPointPosition(i).GetValue(),
			RampView->GetRampPointValue(i).GetValue());

		FloatCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
	}
}

TOptional<int32>
SHoudiniFloatRampCurveEditor::GetNumCurveKeys() const
{
	if (!Curve)
	{
		return {};
	}

	return Curve->FloatCurve.GetNumKeys();
}

TOptional<float>
SHoudiniFloatRampCurveEditor::GetCurveKeyPosition(const int32 Index) const
{
	if (!Curve)
	{
		return {};
	}

	if (!Curve->FloatCurve.Keys.IsValidIndex(Index))
	{
		return {};
	}

	return Curve->FloatCurve.Keys[Index].Time;
}

TOptional<float>
SHoudiniFloatRampCurveEditor::GetCurveKeyValue(const int32 Index) const
{
	if (!Curve)
	{
		return {};
	}

	if (!Curve->FloatCurve.Keys.IsValidIndex(Index))
	{
		return {};
	}

	return Curve->FloatCurve.Keys[Index].Value;
}

TOptional<ERichCurveInterpMode>
SHoudiniFloatRampCurveEditor::GetCurveKeyInterpolationType(const int32 Index) const
{
	if (!Curve)
	{
		return {};
	}

	if (!Curve->FloatCurve.Keys.IsValidIndex(Index))
	{
		return {};
	}

	return Curve->FloatCurve.Keys[Index].InterpMode.GetValue();
}

FReply
SHoudiniFloatRampCurveEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsMouseButtonDown = false;

	return SCurveEditor::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SHoudiniFloatRampCurveEditor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsMouseButtonDown = true;

	return SCurveEditor::OnMouseButtonDown(MyGeometry, MouseEvent);;
}

TSharedRef<SWidget> SHoudiniFloatRamp::ConstructRampPointValueWidget(const int32 Index)
{
	if (!RampView.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SNumericEntryBox<float>)
		.AllowSpin(true)
		.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.Value(RampView->GetRampPointValue(Index).Get(0.f))
		.OnValueChanged_Lambda([](float Val) {})
		.OnValueCommitted_Lambda(
			[this, Index](float Val, ETextCommit::Type TextCommitType) 
			{
				// For some reason I can't figure out, UE sends a second commit event with type Default, which has the old
				// value, causing the first commit to be reset. So ignore it.
				if (TextCommitType == ETextCommit::Type::Default)
					return;

				if (OnPointValueCommit(Index, Val))
				{
					OnValueCommitted.ExecuteIfBound();
				}
			})
		.OnBeginSliderMovement_Lambda([]() {})
		.OnEndSliderMovement_Lambda(
			[this, Index](const float Val)
			{
				if (OnPointValueCommit(Index, Val))
				{
					OnValueCommitted.ExecuteIfBound();
				}
			})
		.SliderExponent(1.0f);
}

#undef LOCTEXT_NAMESPACE
