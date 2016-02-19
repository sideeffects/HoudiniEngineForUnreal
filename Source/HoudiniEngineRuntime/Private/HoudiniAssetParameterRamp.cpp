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
	HoudiniAssetParameterRamp->OnCurveFloatChanged();
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
	HoudiniAssetParameterRamp->OnCurveColorChanged();

	// Unfortunately this will not work as SColorGradientEditor is missing OnCurveChange callback calls.
	// This is most likely UE4 bug.
}


void
UHoudiniAssetParameterRampCurveColor::SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp)
{
	HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
}


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
	TSharedPtr<SCurveEditor> CurveEditor;

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
			SAssignNew(CurveEditor, SCurveEditor)
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
UHoudiniAssetParameterRamp::OnCurveFloatChanged()
{
	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetParameterRamp::OnCurveColorChanged()
{
	MarkPreChanged();
	MarkChanged();
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

