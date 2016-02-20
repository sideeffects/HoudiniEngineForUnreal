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
#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"
#include "Curves/CurveBase.h"


UHoudiniAssetParameterRampCurveFloat::UHoudiniAssetParameterRampCurveFloat(
	const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetParameterRamp(nullptr)
{

}


void
UHoudiniAssetParameterRampCurveFloat::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	Super::OnCurveChanged(ChangedCurveEditInfos);

	check(HoudiniAssetParameterRamp);
	HoudiniAssetParameterRamp->OnCurveFloatChanged(this);
}


void
UHoudiniAssetParameterRampCurveFloat::SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp)
{
	HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
}


UHoudiniAssetParameterRampCurveColor::UHoudiniAssetParameterRampCurveColor(
	const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAssetParameterRamp(nullptr)
{

}


void
UHoudiniAssetParameterRampCurveColor::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	Super::OnCurveChanged(ChangedCurveEditInfos);

	check(HoudiniAssetParameterRamp);
	HoudiniAssetParameterRamp->OnCurveColorChanged(this);

	// Unfortunately this will not work as SColorGradientEditor is missing OnCurveChange callback calls.
	// This is most likely UE4 bug.
}


void
UHoudiniAssetParameterRampCurveColor::SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp)
{
	HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
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
	return SCurveEditor::OnMouseButtonUp(MyGeometry, MouseEvent);

	if(HoudiniAssetParameterRamp)
	{
		HoudiniAssetParameterRamp->OnMouseButtonUpOverCurveFloat();
	}
}


#endif


const EHoudiniAssetParameterRampKeyInterpolation::Type
UHoudiniAssetParameterRamp::DefaultSplineInterpolation = EHoudiniAssetParameterRampKeyInterpolation::MonotoneCubic;


const EHoudiniAssetParameterRampKeyInterpolation::Type
UHoudiniAssetParameterRamp::DefaultUnknownInterpolation = EHoudiniAssetParameterRampKeyInterpolation::Linear;


UHoudiniAssetParameterRamp::UHoudiniAssetParameterRamp(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	CurveObject(nullptr),
	bIsFloatRamp(true)
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

	// Generate curve points from HAPI data.
	GenerateCurvePoints();

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterRamp::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
	
	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SHoudiniAssetParameterRampCurveEditor> CurveEditor;

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
		]
	];

	// Set callback for curve editor events.
	if(CurveEditor.IsValid())
	{
		CurveEditor->SetParentRampParameter(this);
	}

	// If necessary, create the curve object.
	if(!CurveObject)
	{
		FName CurveAssetName = NAME_None;
		UPackage* CurveAssetPackage = BakeCreateCurvePackage(CurveAssetName, false);
		CurveObject = Cast<UCurveBase>(CurveEditor->CreateCurveObject(CurveClass, CurveAssetPackage, CurveAssetName));

		// Set parent parameter for callback on curve events.
		if(CurveObject && CurveObject->IsA(UHoudiniAssetParameterRampCurveFloat::StaticClass()))
		{
			Cast<UHoudiniAssetParameterRampCurveFloat>(CurveObject)->SetParentRampParameter(this);
		}
		else if(CurveObject && CurveObject->IsA(UHoudiniAssetParameterRampCurveColor::StaticClass()))
		{
			Cast<UHoudiniAssetParameterRampCurveColor>(CurveObject)->SetParentRampParameter(this);
		}
	}

	if(CurveObject)
	{
		// Register curve with the asset system.
		//FAssetRegistryModule::AssetCreated(CurveObject);
		//CurveAssetPackage->GetOutermost()->MarkPackageDirty();

		// Set curve values.
		GenerateCurvePoints();

		// Set the curve that is being edited.
		CurveEditor->SetCurveOwner(CurveObject, true);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	// Bypass multiparm widget creation.
	UHoudiniAssetParameter::CreateWidget(DetailCategoryBuilder);
}


UPackage*
UHoudiniAssetParameterRamp::BakeCreateCurvePackage(FName& CurveName, bool bBake)
{
	UPackage* Package = GetTransientPackage();

	if(!HoudiniAssetComponent)
	{
		CurveName = NAME_None;
		return Package;
	}

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	if(!HoudiniAsset)
	{
		CurveName = NAME_None;
		return Package;
	}

	FGuid BakeGUID = FGuid::NewGuid();
	FString CurveNameString = TEXT("");

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

		// Generate curve name.
		CurveNameString = HoudiniAsset->GetName() + TEXT("_") + ParameterName;

		// Generate unique package name.
		FString PackageName = TEXT("");

		if(bBake)
		{
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) + TEXT("/") +
				CurveNameString;
		}
		else
		{
			CurveNameString = CurveNameString + TEXT("_") + BakeGUIDString;
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOuter()->GetName()) + TEXT("/") +
				CurveNameString;
		}

		PackageName = PackageTools::SanitizePackageName(PackageName);

		// See if package exists, if it does, we need to regenerate the name.
		Package = FindPackage(nullptr, *PackageName);

		if(Package)
		{
			if(!bBake)
			{
				// Package does exist, there's a collision, we need to generate a new name.
				BakeGUID.Invalidate();
			}
		}
		else
		{
			// Create actual package.
			Package = CreatePackage(nullptr, *PackageName);
			break;
		}
	}

	CurveName = FName(*CurveNameString);
	return Package;
}

#endif

void
UHoudiniAssetParameterRamp::OnCurveFloatChanged(UHoudiniAssetParameterRampCurveFloat* CurveFloat)
{
	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetParameterRamp::OnCurveColorChanged(UHoudiniAssetParameterRampCurveColor* CurveColor)
{
	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetParameterRamp::OnMouseButtonUpOverCurveFloat()
{

}


void
UHoudiniAssetParameterRamp::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetParameterRamp* HoudiniAssetParameterRamp = Cast<UHoudiniAssetParameterRamp>(InThis);
	if(HoudiniAssetParameterRamp)
	{
		if(HoudiniAssetParameterRamp->CurveObject)
		{
			Collector.AddReferencedObject(HoudiniAssetParameterRamp->CurveObject, InThis);
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

	// Serialize the curve.
	Ar << CurveObject;
}


void
UHoudiniAssetParameterRamp::GenerateCurvePoints()
{
	if(!CurveObject)
	{
		return;
	}

	if(ChildParameters.Num() % 3 != 0)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Invalid Ramp parameter [%s] : Number of child parameters is not a tuple of 3."),
			*ParameterName);

		return;
	}

	if(CurveObject->IsA(UHoudiniAssetParameterRampCurveFloat::StaticClass()))
	{
		UHoudiniAssetParameterRampCurveFloat* CurveObjectFloat =
			Cast<UHoudiniAssetParameterRampCurveFloat>(CurveObject);

		CurveObjectFloat->ResetCurve();

		for(int32 ChildIdx = 0, ChildNum = GetRampKeyCount(); ChildIdx < ChildNum; ++ChildIdx)
		{
			UHoudiniAssetParameterFloat* ChildParamPosition =
				Cast<UHoudiniAssetParameterFloat>(ChildParameters[3 * ChildIdx + 0]);

			UHoudiniAssetParameterFloat* ChildParamValue =
				Cast<UHoudiniAssetParameterFloat>(ChildParameters[3 * ChildIdx + 1]);

			UHoudiniAssetParameterChoice* ChildParamInterpolation =
				Cast<UHoudiniAssetParameterChoice>(ChildParameters[3 * ChildIdx + 2]);

			if(!ChildParamPosition || !ChildParamValue || !ChildParamInterpolation)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Invalid Ramp parameter [%s] : One of child parameters is of invalid type."),
					*ParameterName);

				CurveObjectFloat->ResetCurve();
				return;
			}

			float CurveKeyPosition = ChildParamPosition->GetParameterValue(0, 0.0f);
			float CurveKeyValue = ChildParamValue->GetParameterValue(0, 0.0f);
			EHoudiniAssetParameterRampKeyInterpolation::Type RampKeyInterpolation =
				TranslateChoiceKeyInterpolation(ChildParamInterpolation);
			ERichCurveInterpMode RichCurveInterpMode = TranslateHoudiniRampKeyInterpolation(RampKeyInterpolation);

			FKeyHandle const KeyHandle = CurveObjectFloat->FloatCurve.AddKey(CurveKeyPosition, CurveKeyValue);
			CurveObjectFloat->FloatCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
		}
	}
	else if(CurveObject->IsA(UHoudiniAssetParameterRampCurveColor::StaticClass()))
	{
		UHoudiniAssetParameterRampCurveColor* CurveObjectColor =
			Cast<UHoudiniAssetParameterRampCurveColor>(CurveObject);

		CurveObjectColor->ResetCurve();

		for(int32 ChildIdx = 0, ChildNum = GetRampKeyCount(); ChildIdx < ChildNum; ++ChildIdx)
		{
			UHoudiniAssetParameterFloat* ChildParamPosition =
				Cast<UHoudiniAssetParameterFloat>(ChildParameters[3 * ChildIdx + 0]);

			UHoudiniAssetParameterColor* ChildParamColor =
				Cast<UHoudiniAssetParameterColor>(ChildParameters[3 * ChildIdx + 1]);

			UHoudiniAssetParameterChoice* ChildParamInterpolation =
				Cast<UHoudiniAssetParameterChoice>(ChildParameters[3 * ChildIdx + 2]);

			if(!ChildParamPosition || !ChildParamColor || !ChildParamInterpolation)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Invalid Ramp parameter [%s] : One of child parameters is of invalid type."),
					*ParameterName);

				CurveObjectColor->ResetCurve();
				return;
			}
		}
	}
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

