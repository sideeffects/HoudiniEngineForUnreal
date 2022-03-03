/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HoudiniParameterDetails.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniParameterLabel.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniParameterFolder.h"
#include "HoudiniParameterFolderList.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterSeparator.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniInput.h"
#include "HoudiniAsset.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineDetails.h"
#include "SNewFilePathPicker.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "Math/UnitConversion.h"
#include "ScopedTransaction.h"
#include "EditorDirectories.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "SCurveEditorView.h"
#include "SAssetDropTarget.h"
#include "AssetThumbnail.h"

#include "Sound/SoundBase.h"
#include "Engine/SkeletalMesh.h"
#include "Particles/ParticleSystem.h"
#include "FoliageType.h"

#include "HoudiniInputDetails.h"

#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

int32 
SCustomizedButton::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedPtr<SWidget> Content = GetContent();

	// 0. Initialize Line Buffer.
	TArray<FVector2D> Line;
	Line.SetNum(2);

	//    Initialize Color buffer.
	FLinearColor Color = FLinearColor::White;

	// 1. Draw the radio button.
	if (bIsRadioButton)
	{
		// Construct the radio button circles exactly once,
		// All radio buttons share the same circles then
		if (FHoudiniEngineEditor::Get().GetHoudiniParameterRadioButtonPointsOuter().Num() != HOUDINI_RADIO_BUTTON_CIRCLE_SAMPLES_NUM_OUTER ||
			FHoudiniEngineEditor::Get().GetHoudiniParameterRadioButtonPointsInner().Num() != HOUDINI_RADIO_BUTTON_CIRCLE_SAMPLES_NUM_INNER)
		{
			ConstructRadioButtonCircles();
		}

		DrawRadioButton(AllottedGeometry, OutDrawElements, LayerId, bChosen);
	}

	// 2. Draw background color (if selected)
	if (bChosen)
	{
		Line[0].X = AllottedGeometry.Size.X - AllottedGeometry.Size.Y / 2.0f + 2.5f;
		Line[0].Y = Content->GetDesiredSize().Y / 2.0f;
		Line[1].X = AllottedGeometry.Size.Y / 2.0f - 2.5f;
		Line[1].Y = Content->GetDesiredSize().Y / 2.0f;

		Color = FLinearColor::White;
		Color.A = bIsRadioButton ? 0.05 : 0.1;

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, Color, true, AllottedGeometry.Size.Y);
	}

	// 3. Drawing square around the text
	{
		// Switch the point order for each line to save few value assignment cycles
		Line[0].X = 0.0f;
		Line[0].Y = 0.0f;
		Line[1].X = 0.0f;
		Line[1].Y = Content->GetDesiredSize().Y;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, FLinearColor::Black, true, 1.0f);

		//Line[0].X = 0.0f;
		//Line[0].Y = Content->GetDesiredSize().Y;
		Line[0].X = AllottedGeometry.Size.X;
		Line[0].Y = Content->GetDesiredSize().Y;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, bChosen ? FLinearColor::Gray : FLinearColor::Black, true, 1.0f);

		//Line[0].X = AllottedGeometry.Size.X;
		//Line[0].Y = Content->GetDesiredSize().Y;
		Line[1].X = AllottedGeometry.Size.X;
		Line[1].Y = 0.0f;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, FLinearColor::Black, true, 1.0f);     /* draw gray bottom line if this tab is selected, black otherwise*/

		//Line[0].X = AllottedGeometry.Size.X;
		//Line[0].Y = 0.0f;
		Line[0].X = 0.0f;
		Line[0].Y = 0.0f;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, FLinearColor::Black, true, 1.0f);
	}

	// 4. Draw child widget
	Content->Paint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	return LayerId;
};

void 
SCustomizedButton::ConstructRadioButtonCircles() const 
{
	TArray<FVector2D>& OuterPoints = FHoudiniEngineEditor::Get().GetHoudiniParameterRadioButtonPointsOuter();
	TArray<FVector2D>& InnerPoints = FHoudiniEngineEditor::Get().GetHoudiniParameterRadioButtonPointsInner();
	OuterPoints.Empty();
	InnerPoints.Empty();

	OuterPoints.SetNumZeroed(HOUDINI_RADIO_BUTTON_CIRCLE_SAMPLES_NUM_OUTER);
	InnerPoints.SetNumZeroed(8);

	// Construct outer circle
	int32 CurDegree = 0;
	int32 DegStep = 360 / HOUDINI_RADIO_BUTTON_CIRCLE_SAMPLES_NUM_OUTER;

	for (int32 Idx = 0; Idx < HOUDINI_RADIO_BUTTON_CIRCLE_SAMPLES_NUM_OUTER; ++Idx)
	{
		OuterPoints[Idx].X = HOUDINI_RADIO_BUTTON_CIRCLE_CENTER_X + 
			HOUDINI_RADIO_BUTTON_CIRCLE_RADIUS_OUTER * FMath::Sin(FMath::DegreesToRadians(CurDegree));
		OuterPoints[Idx].Y = HOUDINI_RADIO_BUTTON_CIRCLE_CENTER_X + 
			HOUDINI_RADIO_BUTTON_CIRCLE_RADIUS_OUTER * FMath::Cos(FMath::DegreesToRadians(CurDegree));

		CurDegree += DegStep;
	}

	// Construct inner circle
	CurDegree = 0;
	DegStep = 360 / HOUDINI_RADIO_BUTTON_CIRCLE_SAMPLES_NUM_INNER;
	for (int32 Idx = 0; Idx < 8; ++Idx) 
	{
		InnerPoints[Idx].X = HOUDINI_RADIO_BUTTON_CIRCLE_CENTER_X +
			HOUDINI_RADIO_BUTTON_CIRCLE_RADIUS_INNER * FMath::Sin(FMath::DegreesToRadians(CurDegree));
		InnerPoints[Idx].Y = HOUDINI_RADIO_BUTTON_CIRCLE_CENTER_X +
			HOUDINI_RADIO_BUTTON_CIRCLE_RADIUS_INNER * FMath::Cos(FMath::DegreesToRadians(CurDegree));

		CurDegree += DegStep;
	}
}

void 
SCustomizedButton::DrawRadioButton(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const bool& bSelected) const
{
	TArray<FVector2D>& OuterPoints = FHoudiniEngineEditor::Get().GetHoudiniParameterRadioButtonPointsOuter();
	TArray<FVector2D>& InnerPoints = FHoudiniEngineEditor::Get().GetHoudiniParameterRadioButtonPointsInner();
	if (OuterPoints.Num() <= 1 || InnerPoints.Num() <= 1)
		return;

	FLinearColor ColorNonSelected = FLinearColor::White;
	FLinearColor ColorSelected = FLinearColor::Yellow;

	// initialize line buffer
	TArray<FVector2D> Line;
	Line.SetNumZeroed(2);
	bool alternator = false;

	// Draw outer circle
	Line[0] = OuterPoints.Last();
	for (int32 Idx = 0; Idx < OuterPoints.Num(); ++Idx) 
	{
		// alternate the points order each time to some some assignment cycles
		if (alternator)
		{
			Line[0].X = OuterPoints[Idx].X;
			Line[0].Y = OuterPoints[Idx].Y;
		}
		else 
		{
			Line[1].X = OuterPoints[Idx].X;
			Line[1].Y = OuterPoints[Idx].Y;
		}

		alternator = !alternator;

		// Draw a line segment
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, ColorNonSelected, true, 1.0f);
	}

	// Draw inner circle
	alternator = false;
	Line[0] = InnerPoints.Last();
	for (int32 Idx = 0; Idx < InnerPoints.Num(); ++Idx)
	{
		// alternate the points order each time to some some assignment cycles
		if (alternator)
		{
			Line[0].X = InnerPoints[Idx].X;
			Line[0].Y = InnerPoints[Idx].Y;
		}
		else
		{
			Line[1].X = InnerPoints[Idx].X;
			Line[1].Y = InnerPoints[Idx].Y;
		}

		alternator = !alternator;

		// Draw a line segment
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, bSelected ? ColorSelected : ColorNonSelected, true, 3.0f);
	}
}

void
SCustomizedBox::SetHoudiniParameter(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;


	const bool bIsMultiparmInstanceHeader = MainParam->IsDirectChildOfMultiParm() && MainParam->GetChildIndex() == 0;

	switch (MainParam->GetParameterType())
	{
	case EHoudiniParameterType::Button:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_BUTTON_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_BUTTON;
	}
	break;

	case EHoudiniParameterType::ButtonStrip:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_BUTTONSTRIP_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_BUTTONSTRIP;
	}
	break;

	case EHoudiniParameterType::Color:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_COLOR_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_COLOR;
	}
	break;

	case EHoudiniParameterType::ColorRamp:
	{
		UHoudiniParameterRampColor const* const ColorRampParameter = Cast<UHoudiniParameterRampColor>(MainParam.Get());
		if (!IsValid(ColorRampParameter))
			return;

		MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_COLORRAMP;
		if (ColorRampParameter->CachedPoints.Num() > 0)
			MarginHeight = MarginHeight + HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_COLORRAMP_INSTANCE * (float)(ColorRampParameter->CachedPoints.Num() - 1);
	}
	break;

	case EHoudiniParameterType::File:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILE_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILE;
	}
	break;

	case EHoudiniParameterType::FileDir:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILEDIR_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILEDIR;
	}
	break;

	case EHoudiniParameterType::FileGeo:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILEGEO_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILEGEO;
	}
	break;

	case EHoudiniParameterType::FileImage:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILEIMAGE_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FILEIMAGE;
	}
	break;

	case EHoudiniParameterType::Float:
	{
		if (MainParam->GetTupleSize() == 3)
		{
			if (bIsMultiparmInstanceHeader)
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOAT_VEC3_MULTIPARMHEADER;
			else
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOAT_VEC3;
		}
		else
		{
			if (bIsMultiparmInstanceHeader)
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOAT_MULTIPARMHEADER
					+ (MainParam->GetTupleSize() - 1) * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOAT_INSTANCE_MULTIPARMHEADER;
			}
			else
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOAT
					+ (MainParam->GetTupleSize() - 1)* HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOAT_INSTANCE;
			}
		}
	}
	break;

	case EHoudiniParameterType::FloatRamp:
	{
		UHoudiniParameterRampFloat * FloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
		if (!IsValid(FloatRampParameter))
			return;

		MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOATRAMP;

		if (FloatRampParameter->CachedPoints.Num() > 0)
			MarginHeight = MarginHeight + Houdini_PARAMETER_UI_ROW_MARGIN_HEIGHT_FLOATRAMP_INSTANCE * (float)(FloatRampParameter->CachedPoints.Num() - 1);
	}
	break;

	case EHoudiniParameterType::Folder:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FOLDER_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FOLDER;
	}
	break;

	case EHoudiniParameterType::FolderList:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FOLDERLIST_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_FOLDERLIST;
	}
	break;

	case EHoudiniParameterType::Input:
	{
		UHoudiniParameterOperatorPath* InputParam = Cast<UHoudiniParameterOperatorPath>(MainParam);
		
		if (!IsValid(InputParam) || !InputParam->HoudiniInput.IsValid())
			break;

		UHoudiniInput* Input = InputParam->HoudiniInput.Get();
		
		if (!IsValid(Input))
			break;


		if (bIsMultiparmInstanceHeader)
		{
			switch (Input->GetInputType())
			{
			case EHoudiniInputType::Geometry:
			{
				int32 ExpandedTransformUIs = 0;
				for (int32 Idx = 0; Idx < Input->GetNumberOfInputObjects(); ++Idx)
				{
					if (Input->IsTransformUIExpanded(Idx))
						ExpandedTransformUIs += 1;
				}

				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRY_MULTIPARMHEADER
					+ Input->GetNumberOfInputObjects() * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRY_INSTANCE_MULTIPARMHEADER
					+ ExpandedTransformUIs * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRY_INSTANCE_TRANSFORM_MULTIPARMHEADER;
			}
			break;
			case EHoudiniInputType::Curve:
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE_MULTIPARMHEADER
					+ Input->GetNumberOfInputObjects() * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE_INSTANCE_MULTIPARMHEADER;
			}
			break;
			case EHoudiniInputType::Asset:
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_ASSET_MULTIPARMHEADER;
			}
			break;
			case EHoudiniInputType::Landscape:
			{
				if (Input->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_LANDSCAPE_MULTIPARMHEADER;
				else
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_LANDSCAPE_MESH_MULTIPARMHEADER;
			}
			break;
			case EHoudiniInputType::World:
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_WORLD_MULTIPARMHEADER;
			}
			break;
			case EHoudiniInputType::Skeletal:
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_SKELETAL_MULTIPARMHEADER;
			}
			break;
			case EHoudiniInputType::GeometryCollection:
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRYCOLLECTION_MULTIPARMHEADER;
			}
			
			default:
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_MULTIPARMHEADER;
				break;
			}
		}
		else
		{
			switch (Input->GetInputType())
			{
				case EHoudiniInputType::Geometry:
				{
					int32 ExpandedTransformUIs = 0;
					for (int32 Idx = 0; Idx < Input->GetNumberOfInputObjects(); ++Idx)
					{
						if (Input->IsTransformUIExpanded(Idx))
							ExpandedTransformUIs += 1;
					}

					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRY
						+ Input->GetNumberOfInputObjects() * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRY_INSTANCE
						+ ExpandedTransformUIs * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRY_INSTANCE_TRANSFORM;
				}
				break;
				case EHoudiniInputType::Curve:
				{
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE
						+ Input->GetNumberOfInputObjects() * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE_INSTANCE;
				}
				break;
				case EHoudiniInputType::Asset:
				{
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_ASSET;
				}
				break;
				case EHoudiniInputType::Landscape:
				{
					if (Input->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
						MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_LANDSCAPE;
					else
						MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_LANDSCAPE_MESH;
				}
				break;
				case EHoudiniInputType::World:
				{
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_WORLD;
				}
				break;
				case EHoudiniInputType::Skeletal:
				{
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_SKELETAL;
				}
				break;
				case EHoudiniInputType::GeometryCollection:
				{
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_GEOMETRYCOLLECTION;
				}
				break;
				default:
					MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT;
					break;
				
			}
		}	
	}
	break;

	case EHoudiniParameterType::Int:
	{
		if (MainParam->GetTupleSize() == 3)
		{
			if (bIsMultiparmInstanceHeader)
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INT_VEC3_MULTIPARMHEADER;
			else
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INT_VEC3;
		}
		else
		{
			if (bIsMultiparmInstanceHeader)
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INT_MULTIPARMHEADER + 
					(MainParam->GetTupleSize() - 1) * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INT_INSTANCE_MULTIPARMHEADER;
			}
			else
			{
				MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INT 
					+ (MainParam->GetTupleSize() - 1) * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INT_INSTANCE;
			}
		}
	}
		break;

	case EHoudiniParameterType::IntChoice: 
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INTCHOICE_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INTCHOICE;
	}
		break;

	case EHoudiniParameterType::Label:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_LABEL_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_LABEL;
	}
		break;

	case EHoudiniParameterType::MultiParm: 
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_MULTIPARM_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_MULTIPARM;
	}
		break;

	case EHoudiniParameterType::Separator:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_SEPARATOR_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_SEPARATOR;
		bIsSeparator = true;
	}
		break;

	case EHoudiniParameterType::String: 
	{
		if (bIsMultiparmInstanceHeader)
		{
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRING_MULTIPARMHEADER 
				+ (MainParam->GetTupleSize() - 1) * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRING_INSTANCE_MULTIPARMHEADER;
		}
		else
		{
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRING
				+ (MainParam->GetTupleSize() - 1) * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRING_INSTANCE;
		}
	}
		break;

	case EHoudiniParameterType::StringAssetRef: 
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRINGASSETREF_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRINGASSETREF;
	}
		break;

	case EHoudiniParameterType::StringChoice:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRINGCHOICE_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_STRINGCHOICE;
	}
		break;

	case EHoudiniParameterType::Toggle:
	{
		if (bIsMultiparmInstanceHeader)
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_TOGGLE_MULTIPARMHEADER;
		else
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_TOGGLE;
	}
		break;

	case EHoudiniParameterType::Invalid:
		MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INVALID;
		break;

	default:
		MarginHeight = 0.0f;
		break;
	}
}

float
SCustomizedBox::AddIndentation(const TWeakObjectPtr<UHoudiniParameter>& InParam, 
	const TMap<int32, TWeakObjectPtr<UHoudiniParameterMultiParm>>& InAllMultiParms, const TMap<int32, TWeakObjectPtr<UHoudiniParameter>>& InAllFoldersAndFolderLists)
{
	if (!InParam.IsValid())
		return 0.0f;

	bool bIsMainParmSimpleFolder = false;
	// Get if this Parameter is a simple / collapsible folder
	if (InParam->GetParameterType() == EHoudiniParameterType::Folder) 
	{
		UHoudiniParameterFolder* FolderParm = Cast<UHoudiniParameterFolder>(InParam);
		if (FolderParm)
			bIsMainParmSimpleFolder = !FolderParm->IsTab();
	}

	int32 ParentId = InParam->GetParentParmId();
	TWeakObjectPtr<UHoudiniParameter> CurParm = InParam;
	float Indentation = 0.0f;

	while (ParentId >= 0)
	{
		TWeakObjectPtr<UHoudiniParameter> ParentFolder;
		TWeakObjectPtr<UHoudiniParameterMultiParm> ParentMultiParm;

		if (InAllFoldersAndFolderLists.Contains(ParentId))
			ParentFolder = InAllFoldersAndFolderLists[ParentId];

		if (InAllMultiParms.Contains(ParentId))
			ParentMultiParm = InAllMultiParms[ParentId];

		// The parent is a folder, add one unit of indentation
		if (ParentFolder.IsValid())
		{
			// Update the parent parm id
			ParentId = ParentFolder->GetParentParmId();

			if (ParentFolder->GetParameterType() == EHoudiniParameterType::FolderList)
				continue;

			TWeakObjectPtr<UHoudiniParameterFolder> Folder = Cast<UHoudiniParameterFolder>(ParentFolder);
			
			if (!IsValidWeakPointer(Folder))
				continue;
			
			// update the current parm, find the parent of new cur param in the next round
			CurParm = Folder;
			Indentation += 1.0f;
		}
		// The parent is a multiparm
		else if (ParentMultiParm.IsValid())
		{
			// Update the parent parm id
			ParentId = ParentMultiParm->GetParentParmId();

			if (CurParm->GetChildIndex() == 0) 
			{
				Indentation += 0.0f;
			}
			else 
			{
				Indentation += 2.0f;
			}

			// update the current parm, find the parent of new cur param in the next round
			CurParm = ParentMultiParm;
		}
		else
		{
			// no folder/multiparm parent, end the loop
			ParentId = -1;
		}
	}


	float IndentationWidth = INDENTATION_UNIT_WIDTH * Indentation;

	// Add a base indentation to non simple/collapsible param
	// Since it needs more space to offset the arrow width
	if (!bIsMainParmSimpleFolder)
		IndentationWidth += NON_FOLDER_OFFSET_WIDTH;

	this->AddSlot().AutoWidth()
	[
		SNew(SBox).WidthOverride(IndentationWidth)
	];


	return IndentationWidth;
};

int32 
SCustomizedBox::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{

	SHorizontalBox::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// Initialize line buffer
	TArray<FVector2D> Line;
	Line.SetNumZeroed(2);
	// Initialize color buffer
	FLinearColor Color = FLinearColor::White;
	Color.A = 0.3;

	// draw the bottom line if this row is the tab folder list 
	if (bIsTabFolderListRow)
	{
		// Get the start position of the tabs bottom line (right bottom pt of the right most child widget)
		float VerticalLineStartPosX = 0.0f;
		float VerticalLineStartPosY = 0.0f;
		float BottomLineStartPosX = 0.0f;
		float BottomLineStartPosY = -1.0f;

		for (int32 Idx = 0; Idx < Children.Num(); ++Idx)
		{
			TSharedPtr<const SWidget> CurChild = Children.GetChildAt(Idx);
			if (!CurChild.IsValid())
				continue;

			if (Idx == 0)
			{
				VerticalLineStartPosX = CurChild->GetDesiredSize().X;
				VerticalLineStartPosY = CurChild->GetDesiredSize().Y;
			}

			BottomLineStartPosX += CurChild->GetDesiredSize().X;

			if (BottomLineStartPosY < 0.0f)
				BottomLineStartPosY= CurChild->GetDesiredSize().Y;
		}

		// Draw bottom line
		Line[0].X = BottomLineStartPosX;
		Line[0].Y = BottomLineStartPosY;
		Line[1].X = AllottedGeometry.Size.X;
		Line[1].Y = BottomLineStartPosY;

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, Color, true, 1.0f);
	}

	// Draw divider lines
	{
		Line[0].Y = -MarginHeight;
		Line[1].Y = AllottedGeometry.Size.Y + MarginHeight;

		int32 NumOfLinesToDraw = bIsTabFolderListRow ? DividerLinePositions.Num() - 1 : DividerLinePositions.Num();
		for (int32 Idx = 0; Idx < NumOfLinesToDraw; ++Idx) 
		{
			const float& CurDivider = DividerLinePositions[Idx];
			Line[0].X = CurDivider;
			Line[1].X = CurDivider;

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
				ESlateDrawEffect::None, Color, true, 1.0f);
		}

		// Draw the last inner most divider line differently when this the tabs' row.
		if (bIsTabFolderListRow && DividerLinePositions.Num() > 0) 
		{
			const float& TabDivider = DividerLinePositions.Last();
			Line[0].X = TabDivider;
			Line[1].X = TabDivider;
			Line[0].Y = 0.f;

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
				ESlateDrawEffect::None, Color, true, 1.0f);
		}
	}
	
	// Draw tab ending lines
	{
		float YPos = 0.0f;

		for (const float & CurEndingDivider : EndingDividerLinePositions) 
		{
			// Draw cur ending line (vertical)

			Line[0].X = CurEndingDivider;
			Line[0].Y = -2.3f;
			Line[1].X = CurEndingDivider;
			Line[1].Y = YPos;

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
				ESlateDrawEffect::None, Color, true, 1.0f);

			// Draw cur ending line (horizontal)

			// Line[0].X = CurEndingDivider;
			Line[0].Y = YPos;
			Line[1].X = AllottedGeometry.Size.X;
			// Line[1].Y = YPos;

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
				ESlateDrawEffect::None, Color, true, 1.0f);

			YPos += 2.0f;
		}
	}

	// Draw the separator line if this is the row of a separator parameter
	{
		if (bIsSeparator) 
		{
			Line[0].X = 25.f;
			if (DividerLinePositions.Num() > 0)
				Line[0].X += DividerLinePositions.Last();

			Line[0].Y = AllottedGeometry.Size.Y / 2.f;
			Line[1].X = AllottedGeometry.Size.X - 20.f;
			Line[1].Y = Line[0].Y;

			Color.A = 0.7;

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
				ESlateDrawEffect::None, Color, true, 1.5f);
		}
	}

	return LayerId;
};

void
SHoudiniFloatRampCurveEditor::Construct(const FArguments & InArgs)
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


	UCurveEditorSettings * CurveEditorSettings = GetSettings();
	if (CurveEditorSettings)
	{
		CurveEditorSettings->SetTangentVisibility(ECurveEditorTangentVisibility::NoTangents);
	}
}

void
SHoudiniColorRampCurveEditor::Construct(const FArguments & InArgs)
{
	SColorGradientEditor::Construct(SColorGradientEditor::FArguments()
		.ViewMinInput(InArgs._ViewMinInput)
		.ViewMaxInput(InArgs._ViewMaxInput)
	);
}


FReply 
SHoudiniFloatRampCurveEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = SCurveEditor::OnMouseButtonUp(MyGeometry, MouseEvent);
	
	if (!IsValidWeakPointer(HoudiniFloatRampCurve))
		return Reply;

	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	FRichCurve& FloatCurve = HoudiniFloatRampCurve.Get()->FloatCurve;

	TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters = HoudiniFloatRampCurve.Get()->FloatRampParameters;

	if (FloatRampParameters.Num() < 1)
		return Reply;

	if (!IsValidWeakPointer(FloatRampParameters[0]))
		return Reply;
	
	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return Reply;

	// Do not allow modification when the parent HDA of the main param is being cooked.
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return Reply;

	// Push the points of the main float ramp param to other parameters
	FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampParameters);

	// Modification is based on the main parameter, use synced points if the main param is on auto update mode, use cached points otherwise.
	TArray<UHoudiniParameterRampFloatPoint*> & MainPoints = (MainParam->IsAutoUpdate() && bCookingEnabled) ? MainParam->Points : MainParam->CachedPoints;

	int32 NumMainPoints = MainPoints.Num();

	// On mouse button up handler handles point modification only
	if (FloatCurve.GetNumKeys() != NumMainPoints)
		return Reply;

	bool bNeedToRefreshEditor= false;

	for (int32 Idx = 0; Idx < NumMainPoints; ++Idx)
	{
		UHoudiniParameterRampFloatPoint* MainPoint = MainPoints[Idx];

		if (!MainPoint)
			continue;

		float& CurvePosition = FloatCurve.Keys[Idx].Time;
		float& CurveValue = FloatCurve.Keys[Idx].Value;

		// This point is modified
		if (MainPoint->GetPosition() != CurvePosition || MainPoint->GetValue() != CurveValue) 
		{

			// The editor needs refresh only if the main parameter is on manual mode, and has been modified
			if (!(MainParam->IsAutoUpdate() && bCookingEnabled))
				bNeedToRefreshEditor = true;

			// Iterate through the float ramp parameter of all selected HDAs.
			for (auto & NextRampFloat : FloatRampParameters) 
			{
				if (!IsValidWeakPointer(NextRampFloat))
					continue;

				UHoudiniParameterRampFloat* SelectedRampFloat = NextRampFloat.Get();

				if (!SelectedRampFloat)
					continue;

				// Do not modify the selected parameter if its parent HDA is being cooked
				if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedRampFloat))
					continue;

				if (SelectedRampFloat->IsAutoUpdate() && bCookingEnabled) 
				{
					// The selected float ramp parameter is on auto update mode, use its synced points.
					TArray<UHoudiniParameterRampFloatPoint*> &SelectedRampPoints = SelectedRampFloat->Points;

					if (SelectedRampPoints.IsValidIndex(Idx)) 
					{
						// Synced points in the selected ramp is more than or the same number as that in the main parameter,
						// modify the position and value of the synced point and mark them as changed.

						UHoudiniParameterRampFloatPoint*& ModifiedPoint = SelectedRampPoints[Idx];

						if (!ModifiedPoint)
							continue;

						if (ModifiedPoint->GetPosition() != CurvePosition && ModifiedPoint->PositionParentParm) 
						{
							ModifiedPoint->SetPosition(CurvePosition);
							ModifiedPoint->PositionParentParm->MarkChanged(true);
						}

						if (ModifiedPoint->GetValue() != CurveValue && ModifiedPoint->ValueParentParm) 
						{
							ModifiedPoint->SetValue(CurveValue);
							ModifiedPoint->ValueParentParm->MarkChanged(true);
						}
					}
					else 
					{
						// Synced points in the selected ramp is less than that in the main parameter
						// Since we have pushed the points of the main param to all of the selected ramps,
						// We need to modify the insert event.

						int32 IndexInEventsArray = Idx - SelectedRampPoints.Num();
						if (SelectedRampFloat->ModificationEvents.IsValidIndex(Idx))
						{
							UHoudiniParameterRampModificationEvent*& ModEvent = SelectedRampFloat->ModificationEvents[Idx];
							if (!ModEvent)
								continue;

							if (ModEvent->InsertPosition != CurvePosition)
								ModEvent->InsertPosition = CurvePosition;

							if (ModEvent->InsertFloat != CurveValue)
								ModEvent->InsertFloat = CurveValue;
						}

					}
				}
				else 
				{
					// The selected float ramp is on manual update mode, use the cached points.
					TArray<UHoudiniParameterRampFloatPoint*> &FloatRampCachedPoints = SelectedRampFloat->CachedPoints;

					// Since we have pushed the points in main param to all the selected float ramp, 
					// we need to modify the corresponding cached point in the selected float ramp.

					if (FloatRampCachedPoints.IsValidIndex(Idx))
					{
						UHoudiniParameterRampFloatPoint*& ModifiedCachedPoint = FloatRampCachedPoints[Idx];

						if (!ModifiedCachedPoint)
							continue;
						
						if (ModifiedCachedPoint->Position != CurvePosition)
						{
							ModifiedCachedPoint->Position = CurvePosition;
							SelectedRampFloat->bCaching = true;
							if (!bCookingEnabled)
							{
								//SelectedRampFloat->MarkChanged(true);
								if (ModifiedCachedPoint->PositionParentParm)
									ModifiedCachedPoint->PositionParentParm->MarkChanged(true);
							}
						}

						if (ModifiedCachedPoint->Value != CurveValue)
						{
							ModifiedCachedPoint->Value = CurveValue;
							SelectedRampFloat->bCaching = true;
							if (!bCookingEnabled)
							{
								//SelectedRampFloat->MarkChanged(true);
								if (ModifiedCachedPoint->ValueParentParm)
									ModifiedCachedPoint->ValueParentParm->MarkChanged(true);
							}
						}						
					}
				}
			}
		}
	}


	if (bNeedToRefreshEditor)
	{
		FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);
	}

	return Reply;
}

FReply 
SHoudiniFloatRampCurveEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = SCurveEditor::OnKeyDown(MyGeometry, InKeyEvent);

	if (InKeyEvent.GetKey().ToString() != FString("Enter"))
		return Reply;

	if (!HoudiniFloatRampCurve.IsValid() || !HoudiniFloatRampCurve.Get())
		return Reply;

	TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameters = HoudiniFloatRampCurve.Get()->FloatRampParameters;

	if (FloatRampParameters.Num() < 1)
		return Reply;

	if (!IsValidWeakPointer(FloatRampParameters[0]))
		return Reply;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return Reply;

	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	// Do nothing if the main param is on auto update mode
	if (MainParam->IsAutoUpdate() && bCookingEnabled)
		return Reply;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return Reply;

	// Push the points in the main float ramp to the float ramp parameters in all selected HDAs.
	FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampParameters);

	for (auto& NextFloatRamp : FloatRampParameters) 
	{
		if (!IsValidWeakPointer(NextFloatRamp))
			continue;

		UHoudiniParameterRampFloat* SelectedFloatRamp = NextFloatRamp.Get();

		if (!SelectedFloatRamp)
			continue;

		if (SelectedFloatRamp->IsAutoUpdate() && bCookingEnabled)
			continue;

		// Do not sync the selected parameter if its parent HDA is being cooked
		if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedFloatRamp))
			continue;

		// Sync the cached points if the selected float ramp parameter is on manual update mode
		//FHoudiniParameterDetails::SyncCachedFloatRampPoints(SelectedFloatRamp);
		SelectedFloatRamp->SyncCachedPoints();
	}

	return Reply;
}

void
UHoudiniFloatRampCurve::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) 
{
	Super::OnCurveChanged(ChangedCurveEditInfos);
	
	if (FloatRampParameters.Num() < 1)
		return;

	if (!IsValidWeakPointer(FloatRampParameters[0]))
		return;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return;

	// Do not allow modification when the parent HDA of the main param is being cooked
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	// Push all the points of the Main parameter to other parameters
	FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampParameters);

	// Modification is based on the Main Param, use synced points if the Main Param is on auto update mode, otherwise use its cached points.
	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	TArray<UHoudiniParameterRampFloatPoint*> & MainPoints = (MainParam->IsAutoUpdate() && bCookingEnabled) ? MainParam->Points : MainParam->CachedPoints;

	int32 NumMainPoints = MainPoints.Num();

	bool bNeedUpdateEditor = false;

	// OnCurveChanged handler handles point delete and insertion only

	// A point is deleted.
	if (FloatCurve.GetNumKeys() < NumMainPoints) 
	{
		// Find the index of the deleted point
		for (int32 Idx = 0; Idx < NumMainPoints; ++Idx) 
		{
			UHoudiniParameterRampFloatPoint* MainPoint = MainPoints[Idx];

			if (!MainPoint)
				continue;

			float CurPointPosition = MainPoint->GetPosition();
			float CurCurvePosition = -1.0f;

			if (FloatCurve.Keys.IsValidIndex(Idx))
				CurCurvePosition = FloatCurve.Keys[Idx].Time;

			// Delete the point at Idx
			if (CurCurvePosition != CurPointPosition) 
			{		
				// Iterate through all the float ramp parameter in all the selected HDAs
				for (auto & NextFloatRamp : FloatRampParameters) 
				{
					if (!IsValidWeakPointer(NextFloatRamp))
						continue;

					UHoudiniParameterRampFloat* SelectedFloatRamp = NextFloatRamp.Get();

					if (!SelectedFloatRamp)
						continue;

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedFloatRamp))
						continue;

					if (SelectedFloatRamp->IsAutoUpdate() && bCookingEnabled)
					{
						TArray<UHoudiniParameterRampFloatPoint*> & SelectedRampPoints = SelectedFloatRamp->Points;

						// The selected float ramp is on auto update mode:

						if (SelectedRampPoints.IsValidIndex(Idx)) 
						{
							// If the number of synced points of the selected float ramp is greater or equal to the number of points of that in the main param,
							// Create a Houdini engine manager event to delete the point at Idx of the selected float ramp;

							UHoudiniParameterRampFloatPoint* PointToDelete = SelectedRampPoints[Idx];

							if (!PointToDelete)
								continue;

							FHoudiniParameterDetails::CreateFloatRampParameterDeleteEvent(SelectedFloatRamp, PointToDelete->InstanceIndex);
							SelectedFloatRamp->MarkChanged(true);
						}
						else 
						{
							// If the number is smaller than that in the main param, since we have pushed all the points in the main param to the selected parameters,
							// delete the corresponding inserting event.

							int32 IdxInEventsArray = Idx - SelectedRampPoints.Num();
							if (SelectedFloatRamp->ModificationEvents.IsValidIndex(IdxInEventsArray)) 
								SelectedFloatRamp->ModificationEvents.RemoveAt(IdxInEventsArray);
						}
					}
					else 
					{
						// The selected float ramp is on manual update mode:
						// Since we have pushed all the points in main param to the cached points of the selected float ramp,
						// remove the corresponding points from the cached points array.

						if (SelectedFloatRamp->CachedPoints.IsValidIndex(Idx))
						{
							SelectedFloatRamp->CachedPoints.RemoveAt(Idx);
							SelectedFloatRamp->bCaching = true;
						}
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!(MainParam->IsAutoUpdate() && bCookingEnabled))
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	// A point is inserted
	else if (FloatCurve.GetNumKeys() > NumMainPoints)
	{
		// Find the index of the inserted point
		for (int32 Idx = 0; Idx < FloatCurve.GetNumKeys(); ++Idx) 
		{

			float CurPointPosition = -1.0f;
			float CurCurvePosition = FloatCurve.Keys[Idx].Time;

			if (MainPoints.IsValidIndex(Idx)) 
			{
				UHoudiniParameterRampFloatPoint*& MainPoint = MainPoints[Idx];

				if (!MainPoint)
					continue;

				CurPointPosition = MainPoint->GetPosition();
			}

			// Insert instance at Idx
			if (CurPointPosition != CurCurvePosition) 
			{
				// Iterate through the float ramp parameter of all selected HDAs.
				for (auto & NextFloatRamp : FloatRampParameters) 
				{
					if (!IsValidWeakPointer(NextFloatRamp))
						continue;

					UHoudiniParameterRampFloat* SelectedFloatRamp = NextFloatRamp.Get();

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedFloatRamp))
						continue;

					if (SelectedFloatRamp->IsAutoUpdate() && bCookingEnabled) 
					{
						// If the selected float ramp is on auto update mode:
						// Since we have pushed all the points of main parameter to the selected,
						// create a Houdini engine manager event to insert a point. 

						FHoudiniParameterDetails::CreateFloatRampParameterInsertEvent(
							SelectedFloatRamp, CurCurvePosition, FloatCurve.Keys[Idx].Value, EHoudiniRampInterpolationType::LINEAR);

						SelectedFloatRamp->MarkChanged(true);
					}
					else 
					{
						// If the selected float ramp is on manual update mode:
						// push a new point to the cached points array
						UHoudiniParameterRampFloatPoint* NewCachedPoint = NewObject<UHoudiniParameterRampFloatPoint>(SelectedFloatRamp, UHoudiniParameterRampFloatPoint::StaticClass());
						if (!NewCachedPoint)
							continue;

						NewCachedPoint->Position = CurCurvePosition;
						NewCachedPoint->Value = FloatCurve.Keys[Idx].Value;
						NewCachedPoint->Interpolation = EHoudiniRampInterpolationType::LINEAR;

						if (Idx >= SelectedFloatRamp->CachedPoints.Num())
							SelectedFloatRamp->CachedPoints.Add(NewCachedPoint);
						else
							SelectedFloatRamp->CachedPoints.Insert(NewCachedPoint, Idx);

						SelectedFloatRamp->bCaching = true;
						
						if (!bCookingEnabled)
							SelectedFloatRamp->MarkChanged(true);
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!(MainParam->IsAutoUpdate() && bCookingEnabled))
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	if (bNeedUpdateEditor)
	{
		FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);
	}

}


FReply
SHoudiniColorRampCurveEditor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) 
{
	FReply Reply = SColorGradientEditor::OnMouseButtonDown(MyGeometry, MouseEvent);

	if (HoudiniColorRampCurve.IsValid()) 
	{
		UHoudiniColorRampCurve* Curve = HoudiniColorRampCurve.Get();
		if (Curve)
			Curve->bEditing = true;
	}
	
	return Reply;
}

FReply 
SHoudiniColorRampCurveEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) 
{

	FReply Reply = SColorGradientEditor::OnMouseButtonUp(MyGeometry, MouseEvent);

	if (HoudiniColorRampCurve.IsValid()) 
	{
		UHoudiniColorRampCurve* Curve = HoudiniColorRampCurve.Get();

		if (Curve)
		{
			Curve->bEditing = false;
			Curve->OnColorRampCurveChanged(true);
		}
	}

	return Reply;

}

FReply
SHoudiniColorRampCurveEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = SColorGradientEditor::OnKeyDown(MyGeometry, InKeyEvent);

	if (InKeyEvent.GetKey().ToString() != FString("Enter"))
		return Reply;

	if (!HoudiniColorRampCurve.IsValid() || !HoudiniColorRampCurve.Get())
		return Reply;

	TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> &ColorRampParameters = HoudiniColorRampCurve.Get()->ColorRampParameters;

	if (ColorRampParameters.Num() < 1)
		return Reply;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0].Get();

	if (!MainParam)
		return Reply;

	// Do nothing if the main param is on auto update mode
	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();
	if (MainParam->IsAutoUpdate() && bCookingEnabled)
		return Reply;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return Reply;

	// Push the points in the main color ramp to the color ramp parameters in all selected HDAs.
	FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(ColorRampParameters);

	for (auto& NextColorRamp : ColorRampParameters) 
	{
		if (!IsValidWeakPointer(NextColorRamp))
			continue;

		UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

		if (!SelectedColorRamp)
			continue;

		if (SelectedColorRamp->IsAutoUpdate() && bCookingEnabled)
			continue;

		// Do not sync the selected parameter if its parent HDA is being cooked
		if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
			continue;

		// Sync the cached points if the selected color ramp is on manual update mode
		FHoudiniParameterDetails::SyncCachedColorRampPoints(SelectedColorRamp);
	}

	return Reply;
}

void 
UHoudiniColorRampCurve::OnCurveChanged(const TArray< FRichCurveEditInfo > & ChangedCurveEditInfos) 
{
	Super::OnCurveChanged(ChangedCurveEditInfos);

	OnColorRampCurveChanged();
}

void 
UHoudiniColorRampCurve::OnColorRampCurveChanged(bool bModificationOnly) 
{
	// Array is always true in this case
	// if (!FloatCurves)
	// 	return;

	if (ColorRampParameters.Num() < 1)
		return;

	if (!IsValidWeakPointer(ColorRampParameters[0]))
		return;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0].Get();

	if (!MainParam)
		return;

	// Do not allow modification when the parent HDA of the main param is being cooked
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	// Push all the points of the main parameter to other parameters
	FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(ColorRampParameters);

	// Modification is based on the Main Param, use synced points if the Main Param is on auto update mode,otherwise use its cached points.
	bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();
	TArray<UHoudiniParameterRampColorPoint*> & MainPoints = (MainParam->IsAutoUpdate() && bCookingEnabled) ? MainParam->Points : MainParam->CachedPoints;

	int32 NumMainPoints = MainPoints.Num();

	bool bNeedUpdateEditor = false;

	// OnCurveChanged handler of color ramp curve editor handles point delete, insert and color change

	// A point is deleted
	if (FloatCurves->GetNumKeys() < NumMainPoints)
	{
		if (bModificationOnly)
			return;

		// Find the index of the deleted point
		for (int32 Idx = 0; Idx < NumMainPoints; ++Idx)
		{
			UHoudiniParameterRampColorPoint* MainPoint = MainPoints[Idx];

			if (!MainPoint)
				continue;

			float CurPointPosition = MainPoint->GetPosition();
			float CurCurvePosition = -1.0f;

			if (FloatCurves[0].Keys.IsValidIndex(Idx))
				CurCurvePosition = FloatCurves[0].Keys[Idx].Time;

			// Delete the point at Idx
			if (CurCurvePosition != CurPointPosition)
			{
				// Iterate through all the color ramp parameter in all the selected HDAs
				for (auto & NextColorRamp : ColorRampParameters)
				{
					if (!IsValidWeakPointer(NextColorRamp))
						continue;

					UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

					if (!SelectedColorRamp)
						continue;

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
						continue;

					if (SelectedColorRamp->IsAutoUpdate() && bCookingEnabled)
					{
						TArray<UHoudiniParameterRampColorPoint*> & SelectedRampPoints = SelectedColorRamp->Points;

						// The selected color ramp is on auto update mode:

						if (SelectedRampPoints.IsValidIndex(Idx))
						{
							// If the number of synced points of the selected color ramp is greater or equal to the number of points of that in the main param,
							// create a Houdini engine manager event to delete the point at Idx of the selected float ramp;

							UHoudiniParameterRampColorPoint* PointToDelete = SelectedRampPoints[Idx];

							if (!PointToDelete)
								continue;

							FHoudiniParameterDetails::CreateColorRampParameterDeleteEvent(SelectedColorRamp, PointToDelete->InstanceIndex);
							SelectedColorRamp->MarkChanged(true);
						}
						else
						{
							// If the number is smaller than that in the main param, since we have pushed all the points in the main param to the selected parameters,
							// delete the corresponding inserting event.

							int32 IdxInEventsArray = Idx - SelectedRampPoints.Num();
							if (SelectedColorRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
								SelectedColorRamp->ModificationEvents.RemoveAt(IdxInEventsArray);
						}
					}
					else
					{
						// The selected color ramp is on manual update mode:
						// Since we have pushed all the points in main param to the cached points of the selected float ramp,
						// remove the corresponding points from the cached points array
						if (SelectedColorRamp->CachedPoints.IsValidIndex(Idx))
						{
							SelectedColorRamp->CachedPoints.RemoveAt(Idx);
							SelectedColorRamp->bCaching = true;
						}
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!(MainParam->IsAutoUpdate() && bCookingEnabled))
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	// A point is inserted
	else if (FloatCurves[0].GetNumKeys() > NumMainPoints)
	{

		if (bModificationOnly)
			return;

		// Find the index of the inserted point
		for (int32 Idx = 0; Idx < FloatCurves[0].GetNumKeys(); ++Idx)
		{

			float CurPointPosition = -1.0f;
			float CurCurvePosition = FloatCurves[0].Keys[Idx].Time;

			if (MainPoints.IsValidIndex(Idx))
			{
				UHoudiniParameterRampColorPoint*& MainPoint = MainPoints[Idx];

				if (!MainPoint)
					continue;

				CurPointPosition = MainPoint->GetPosition();
			}

			// Insert a point at Idx
			if (CurPointPosition != CurCurvePosition)
			{
				// Get the interpolation value of inserted color point

				FLinearColor ColorPrev = FLinearColor::Black;
				FLinearColor ColorNext = FLinearColor::White;
				float PositionPrev = 0.0f;
				float PositionNext = 1.0f;

				if (MainParam->IsAutoUpdate() && bCookingEnabled)
				{
					// Try to get its previous point's color
					if (MainParam->Points.IsValidIndex(Idx - 1))
					{
						ColorPrev = MainParam->Points[Idx - 1]->GetValue();
						PositionPrev = MainParam->Points[Idx - 1]->GetPosition();
					}

					// Try to get its next point's color
					if (MainParam->Points.IsValidIndex(Idx))
					{
						ColorNext = MainParam->Points[Idx]->GetValue();
						PositionNext = MainParam->Points[Idx]->GetPosition();
					}
				}
				else
				{
					// Try to get its previous point's color
					if (MainParam->CachedPoints.IsValidIndex(Idx - 1))
					{
						ColorPrev = MainParam->CachedPoints[Idx - 1]->GetValue();
						PositionPrev = MainParam->CachedPoints[Idx - 1]->GetPosition();
					}

					// Try to get its next point's color
					if (MainParam->CachedPoints.IsValidIndex(Idx))
					{
						ColorNext = MainParam->CachedPoints[Idx]->GetValue();
						PositionNext = MainParam->CachedPoints[Idx]->GetPosition();
					}
				}

				float TotalWeight = FMath::Abs(PositionNext - PositionPrev);
				float PrevWeight = FMath::Abs(CurCurvePosition - PositionPrev);
				float NextWeight = FMath::Abs(PositionNext - CurCurvePosition);

				FLinearColor InsertedColor = ColorPrev * (PrevWeight / TotalWeight) + ColorNext * (NextWeight / TotalWeight);

				// Iterate through the color ramp parameter of all selected HDAs.
				for (auto & NextColorRamp : ColorRampParameters)
				{
					if (!IsValidWeakPointer(NextColorRamp))
						continue;

					UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

					if (!SelectedColorRamp)
						continue;

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
						continue;

					if (SelectedColorRamp->IsAutoUpdate() && bCookingEnabled)
					{
						// If the selected color ramp is on auto update mode:
						// Since we have pushed all the points of main parameter to the selected,
						// create a Houdini engine manager event to insert a point.

						FHoudiniParameterDetails::CreateColorRampParameterInsertEvent(
							SelectedColorRamp, CurCurvePosition, InsertedColor, EHoudiniRampInterpolationType::LINEAR);

						SelectedColorRamp->MarkChanged(true);
					}
					else
					{
						// If the selected color ramp is on manual update mode:
						// Push a new point to the cached points array
						UHoudiniParameterRampColorPoint* NewCachedPoint = NewObject<UHoudiniParameterRampColorPoint>(SelectedColorRamp, UHoudiniParameterRampColorPoint::StaticClass());
						if (!NewCachedPoint)
							continue;

						NewCachedPoint->Position = CurCurvePosition;
						NewCachedPoint->Value = InsertedColor;
						NewCachedPoint->Interpolation = EHoudiniRampInterpolationType::LINEAR;

						if (Idx >= SelectedColorRamp->CachedPoints.Num())
							SelectedColorRamp->CachedPoints.Add(NewCachedPoint);
						else
							SelectedColorRamp->CachedPoints.Insert(NewCachedPoint, Idx);

						SelectedColorRamp->bCaching = true;
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!MainParam->IsAutoUpdate() && bCookingEnabled)
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	// A point's color is changed
	else
	{
		if (bEditing)
			return;

		for (int32 Idx = 0; Idx < NumMainPoints; ++Idx)
		{
			UHoudiniParameterRampColorPoint* MainPoint = MainPoints[Idx];

			if (!MainPoint)
				continue;

			// Only handle color change
			{
				float CurvePosition = FloatCurves[0].Keys[Idx].Time;
				float PointPosition = MainPoint->GetPosition();

				FLinearColor CurveColor = FLinearColor::Black;
				FLinearColor PointColor = MainPoint->GetValue();

				CurveColor.R = FloatCurves[0].Keys[Idx].Value;
				CurveColor.G = FloatCurves[1].Keys[Idx].Value;
				CurveColor.B = FloatCurves[2].Keys[Idx].Value;

				// Color is changed at Idx
				if (CurveColor != PointColor || CurvePosition != PointPosition)
				{
					// Iterate through the all selected color ramp parameters
					for (auto & NextColorRamp : ColorRampParameters)
					{
						if (!IsValidWeakPointer(NextColorRamp))
							continue;

						UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

						if (!SelectedColorRamp)
							continue;

						// Do not modify the selected parameter if its parent HDA is being cooked
						if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
							continue;

						if (SelectedColorRamp->IsAutoUpdate() && bCookingEnabled)
						{
							// The selected color ramp parameter is on auto update mode 

							if (SelectedColorRamp->Points.IsValidIndex(Idx))
							{
								// If the number of synced points in the selected color ramp is more or equal to that in the main parameter:
								// Modify the corresponding synced point of the selected color ramp, and marked it as changed.

								UHoudiniParameterRampColorPoint* Point = SelectedColorRamp->Points[Idx];

								if (!Point)
									continue;

								if (Point->GetValue() != CurveColor && Point->ValueParentParm)
								{
									Point->SetValue(CurveColor);
									Point->ValueParentParm->MarkChanged(true);
								}

								if (Point->GetPosition() != CurvePosition && Point->PositionParentParm)
								{
									Point->SetPosition(CurvePosition);
									Point->PositionParentParm->MarkChanged(true);
								}
							}
							else
							{
								// If the number of synced points in the selected color ramp is less than that in the main parameter:
								// Since we have push the points in the main parameter to all selected parameters, 
								// we need to modify the corresponding insert event.

								int32 IdxInEventsArray = Idx - SelectedColorRamp->Points.Num();

								if (SelectedColorRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
								{
									UHoudiniParameterRampModificationEvent* Event = SelectedColorRamp->ModificationEvents[IdxInEventsArray];

									if (!Event)
										continue;

									if (Event->InsertColor != CurveColor)
										Event->InsertColor = CurveColor;

									if (Event->InsertPosition != CurvePosition)
										Event->InsertPosition = CurvePosition;
								}
							}
						}
						else
						{
							// The selected color ramp is on manual update mode
							// Since we have push the points in the main parameter to all selected parameters,
							// modify the corresponding point in the cached points array of the selected color ramp.
							if (SelectedColorRamp->CachedPoints.IsValidIndex(Idx))
							{
								UHoudiniParameterRampColorPoint* CachedPoint = SelectedColorRamp->CachedPoints[Idx];

								if (!CachedPoint)
									continue;

								if (CachedPoint->Value != CurveColor)
								{
									CachedPoint->Value = CurveColor;
									bNeedUpdateEditor = true;
								}

								if (CachedPoint->Position != CurvePosition)
								{
									CachedPoint->Position = CurvePosition;
									SelectedColorRamp->bCaching = true;
									bNeedUpdateEditor = true;
								}
							}
						}
					}
				}
			}
		}
	}


	if (bNeedUpdateEditor)
	{
		FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);
	}
}

template< class T >
bool FHoudiniParameterDetails::CastParameters(
	const TArray<UHoudiniParameter*>& InParams, TArray<T*>& OutCastedParams )
{
	for (auto CurrentParam : InParams)
	{
		T* CastedParam = Cast<T>(CurrentParam);
		if (IsValid(CastedParam))
			OutCastedParams.Add(CastedParam);
	}

	return (OutCastedParams.Num() == InParams.Num());
}

template< class T >
bool FHoudiniParameterDetails::CastParameters(
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, TArray<TWeakObjectPtr<T>>& OutCastedParams )
{
	for (const auto& CurrentParam : InParams)
	{
		if (!IsValidWeakPointer(CurrentParam))
			continue;
		
		T* CastedParam = Cast<T>(CurrentParam.Get());
		if (IsValid(CastedParam))
			OutCastedParams.Add(CastedParam);
	}

	return (OutCastedParams.Num() == InParams.Num());
}

void
FHoudiniParameterDetails::CreateWidget(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
{
	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& InParam = InParams[0];
	if (!IsValidWeakPointer(InParam))
		return;

	// The directory won't parse if parameter ids are -1
	// simply return 
	if (InParam->GetParmId() < 0)
		return;

	if (CurrentRampFloat) 
	{
		// CreateWidgetFloatRamp(HouParameterCategory, InParams);
		// If this parameter is a part of the last float ramp, skip it
		if (InParam->GetIsChildOfMultiParm() && InParam->GetParentParmId() == CurrentRampFloat->GetParmId())
			return;

		// This parameter is not part of the last float ramp (we've passed all of its points/instances), reset
		// CurrentRampFloat in order to continue normal processing of parameters
		CurrentRampFloat = nullptr;
	}
	if (CurrentRampColor) 
	{
		// CreateWidgetColorRamp(HouParameterCategory, InParams);
		// if this parameter is a part of the last color ramp, skip it
		if (InParam->GetIsChildOfMultiParm() && InParam->GetParentParmId() == CurrentRampColor->GetParmId())
			return;
		
		// This parameter is not part of the last color ramp (we've passed all of its points/instances), reset
		// CurrentRampColor in order to continue normal processing of parameters
		CurrentRampColor = nullptr;
	}

	switch (InParam->GetParameterType())
	{
		case EHoudiniParameterType::Float:
		{				
			CreateWidgetFloat(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Int:
		{
			CreateWidgetInt(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::String:
		{
			CreateWidgetString(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::IntChoice:
		case EHoudiniParameterType::StringChoice:
		{
			CreateWidgetChoice(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Separator:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterSeparator>> SepParams;
			if (CastParameters<UHoudiniParameterSeparator>(InParams, SepParams))
			{
				bool bEnabled = InParams.IsValidIndex(0) ? !SepParams[0]->IsDisabled() : true;
				CreateWidgetSeparator(HouParameterCategory, InParams, bEnabled);
			}
		}
		break;

		case EHoudiniParameterType::Color:
		{
			CreateWidgetColor(HouParameterCategory, InParams);	
		}
		break;

		case EHoudiniParameterType::Button:
		{
			CreateWidgetButton(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::ButtonStrip:
		{
			CreateWidgetButtonStrip(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Label:
		{
			CreateWidgetLabel(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Toggle:
		{
			CreateWidgetToggle(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::File:
		case EHoudiniParameterType::FileDir:
		case EHoudiniParameterType::FileGeo:
		case EHoudiniParameterType::FileImage:
		{
			CreateWidgetFile(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::FolderList: 
		{	
			CreateWidgetFolderList(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Folder: 
		{
			CreateWidgetFolder(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::MultiParm: 
		{
			CreateWidgetMultiParm(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::FloatRamp: 
		{
			CreateWidgetFloatRamp(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::ColorRamp: 
		{
			CreateWidgetColorRamp(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Input:
		{
			CreateWidgetOperatorPath(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Invalid:
		{
			HandleUnsupportedParmType(HouParameterCategory, InParams);
		}
		break;

		default:
		{
			HandleUnsupportedParmType(HouParameterCategory, InParams);
		}
		break;
	}

	// Remove a divider lines recurrsively if current parameter hits the end of a tabs
	RemoveTabDividers(HouParameterCategory, InParam);

}

void 
FHoudiniParameterDetails::CreateTabEndingRow(IDetailCategoryBuilder & HouParameterCategory) 
{
	FDetailWidgetRow & Row = HouParameterCategory.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SCustomizedBox> TabEndingRow = SNew(SCustomizedBox);

	TabEndingRow->DividerLinePositions = DividerLinePositions;

	if (TabEndingRow.IsValid())
		CurrentTabEndingRow = TabEndingRow.Get();

	Row.WholeRowWidget.Widget = TabEndingRow.ToSharedRef();
	Row.WholeRowWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniParameterDetails::CreateNameWidget(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams, bool WithLabel)
{
	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	if (!Row)
		return;

	TSharedRef< SCustomizedBox > HorizontalBox = SNew(SCustomizedBox);
	
	HorizontalBox->DividerLinePositions = DividerLinePositions;
	HorizontalBox->SetHoudiniParameter(InParams);
	HorizontalBox->AddIndentation(MainParam.Get(), AllMultiParms, AllFoldersAndFolderLists);
	

	if (MainParam->IsDirectChildOfMultiParm()) 
	{
		FString ParameterLabelStr = MainParam->GetParameterLabel();

		// If it is head of an multiparm instance
		if (MainParam->GetChildIndex() == 0)
		{
			int32 CurrentMultiParmInstanceIndex = 0;
			if (MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
			{
				MultiParmInstanceIndices[MainParam->GetParentParmId()] += 1;
				CurrentMultiParmInstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];
			}
			ParameterLabelStr += TEXT(" (") + FString("") + FString::FromInt(CurrentMultiParmInstanceIndex + 1) + TEXT(")");

			CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
		}


		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();
		HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}
	else 
	{
		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(MainParam->GetParameterLabel()) : FText::GetEmpty();
		HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}

	Row->NameWidget.Widget = HorizontalBox;
}

void
FHoudiniParameterDetails::CreateNameWidgetWithAutoUpdate(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams, bool WithLabel)
{
	if (!Row)
		return;

	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	FString ParameterLabelStr = MainParam->GetParameterLabel();
	TSharedRef<SCustomizedBox> HorizontalBox = SNew(SCustomizedBox);
	HorizontalBox->DividerLinePositions = DividerLinePositions;
	HorizontalBox->SetHoudiniParameter(InParams);
	HorizontalBox->AddIndentation(MainParam, AllMultiParms, AllFoldersAndFolderLists);

	TSharedPtr<SVerticalBox> VerticalBox;
	HorizontalBox->AddSlot()
	[
		SAssignNew(VerticalBox, SVerticalBox)
	];

	if (MainParam->IsDirectChildOfMultiParm())
	{
		// If it is head of an multiparm instance
		if (MainParam->GetChildIndex() == 0)
		{
			int32 CurrentMultiParmInstanceIndex = 0;
			if (MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
			{
				MultiParmInstanceIndices[MainParam->GetParentParmId()] += 1;
				CurrentMultiParmInstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];
			}

			ParameterLabelStr += TEXT(" (") + FString::FromInt(CurrentMultiParmInstanceIndex + 1) + TEXT(")");

			CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
		}

		if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
		{
			UHoudiniParameterRampColor* RampParameter = Cast<UHoudiniParameterRampColor>(MainParam);
			if (RampParameter)
			{
				if (RampParameter->bCaching)
					ParameterLabelStr += "*";
			}
		}

		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();

		VerticalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}
	else
	{
		// TODO: Refactor me...extend 'auto/manual update' to all parameters? (It only applies to color and float ramps for now.)
		bool bParamNeedUpdate = false;
		if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
		{
			UHoudiniParameterRampColor* RampParameter = Cast<UHoudiniParameterRampColor>(MainParam);
			if (RampParameter)
				bParamNeedUpdate = RampParameter->bCaching;
		}
		else if (MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
		{
			UHoudiniParameterRampFloat* RampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
			if (RampParameter)
				bParamNeedUpdate = RampParameter->bCaching;
		}

		if (bParamNeedUpdate)
			ParameterLabelStr += "*";

		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();
		
		VerticalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}

	auto IsAutoUpdateChecked = [MainParam]()
	{
		if (!IsValidWeakPointer(MainParam))
			return ECheckBoxState::Unchecked;

		return MainParam->IsAutoUpdate() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	auto OnAutoUpdateCheckBoxStateChanged = [MainParam, InParams, bCookingEnabled](ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			for (auto & NextSelectedParam : InParams)
			{
				if (!IsValidWeakPointer(NextSelectedParam))
					continue;

				if (NextSelectedParam->IsAutoUpdate() && bCookingEnabled)
					continue;

				// Do not allow mode change when the Houdini asset component is cooking
				if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(NextSelectedParam.Get()))
					continue;

				switch (MainParam->GetParameterType())
				{
					case EHoudiniParameterType::ColorRamp:
					{
						UHoudiniParameterRampColor* ColorRampParameter = Cast<UHoudiniParameterRampColor>(NextSelectedParam);

						if (!ColorRampParameter)
							continue;

						// Do not sync the selected color ramp parameter if its parent HDA is being cooked
						if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(ColorRampParameter))
							continue;

						// Sync the Cached curve points at update mode switch.
						FHoudiniParameterDetails::SyncCachedColorRampPoints(ColorRampParameter);
					}
					break;

					case EHoudiniParameterType::FloatRamp:
					{
						UHoudiniParameterRampFloat* FloatRampParameter = Cast<UHoudiniParameterRampFloat>(NextSelectedParam);

						if (!FloatRampParameter)
							continue;

						// Do not sync the selected float ramp parameter if its parent HDA is being cooked
						if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(FloatRampParameter))
							continue;

						// Sync the Cached curve points at update mode switch.
						//FHoudiniParameterDetails::SyncCachedFloatRampPoints(FloatRampParameter);
						FloatRampParameter->SyncCachedPoints();
					}
					break;

					default:
						break;
				}

				NextSelectedParam->SetAutoUpdate(true);
			}
		}
		else
		{
			for (auto & NextSelectedParam : InParams)
			{
				if (!IsValidWeakPointer(NextSelectedParam))
					continue;

				if (!(NextSelectedParam->IsAutoUpdate() && bCookingEnabled))
					continue;

				// Do not allow mode change when the Houdini asset component is cooking
				if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(NextSelectedParam.Get()))
					continue;

				NextSelectedParam->SetAutoUpdate(false);
			}
		}
	};

	// Auto update check box
	TSharedPtr<SCheckBox> CheckBox;

	VerticalBox->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Left)
	[
		SNew(SHorizontalBox) 

		+ SHorizontalBox::Slot()
		[
			SAssignNew(CheckBox, SCheckBox)
			.OnCheckStateChanged_Lambda([OnAutoUpdateCheckBoxStateChanged](ECheckBoxState NewState)
			{
				OnAutoUpdateCheckBoxStateChanged(NewState);
			})
			.IsChecked_Lambda([IsAutoUpdateChecked]()
			{
				return IsAutoUpdateChecked();
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AutoUpdate", "Auto-update"))
				.ToolTipText(LOCTEXT("AutoUpdateTip", "When enabled, this parameter will automatically update its value while editing. Turning this off will allow you to more easily update it, and the update can be pushed by checking the toggle again."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	if ((MainParam->GetParameterType() != EHoudiniParameterType::FloatRamp) && (MainParam->GetParameterType() != EHoudiniParameterType::ColorRamp))
		CheckBox->SetVisibility(EVisibility::Hidden);

	Row->NameWidget.Widget = HorizontalBox;
}

FDetailWidgetRow*
FHoudiniParameterDetails::CreateNestedRow(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, bool bDecreaseChildCount)
{
	if (InParams.Num() <= 0)
		return nullptr;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];

	if (!IsValidWeakPointer(MainParam))
		return nullptr;

	// Created row for the current parameter (if there is not a row created, do not show the parameter).
	FDetailWidgetRow* Row = nullptr;

	// Current parameter is in a multiparm instance (directly)
	if (MainParam->IsDirectChildOfMultiParm())
	{
		int32 ParentMultiParmId = MainParam->GetParentParmId();

		// If this is a folder param, its folder list parent parm is the multiparm
		if (MainParam->GetParameterType() == EHoudiniParameterType::Folder) 
		{
			if (!AllFoldersAndFolderLists.Contains(MainParam->GetParentParmId()))	// This should not happen
				return nullptr;

			UHoudiniParameterFolderList* ParentFolderList = Cast<UHoudiniParameterFolderList>(AllFoldersAndFolderLists[MainParam->GetParentParmId()]);
			if (!IsValid(ParentFolderList))
				return nullptr;			// This should not happen

			ParentMultiParmId = ParentFolderList->GetParentParmId();
		}

		if (!AllMultiParms.Contains(ParentMultiParmId)) // This should not happen normally
			return nullptr;

		// Get the parent multiparm
		const TWeakObjectPtr<UHoudiniParameterMultiParm>& ParentMultiParm = AllMultiParms[ParentMultiParmId];

		// The parent multiparm is visible.
		if (ParentMultiParm.IsValid() && ParentMultiParm->IsShown() && MainParam->ShouldDisplay())
		{
			if (MainParam->GetParameterType() != EHoudiniParameterType::FolderList)
				Row = &(HouParameterCategory.AddCustomRow(FText::GetEmpty()));
		}

	}
	// This item is not a direct child of a multiparm.
	else
	{
		bool bIsFolder = MainParam->GetParameterType() == EHoudiniParameterType::Folder;

		// If this parameter is a folder, its parent folder should be the second top of the stack
		int32 NestedMinStackDepth = bIsFolder ? 1 : 0;

		// Current parameter is inside a folder.
		if (FolderStack.Num() > NestedMinStackDepth)
		{
			// If the current parameter is a folder, we take the top second queue on the stack, since the top one represents itself.
			// Otherwise take the top queue on the stack.
			TArray<UHoudiniParameterFolder*> & CurrentLayerFolderQueue = bIsFolder ?
				FolderStack[FolderStack.Num() - 2] : FolderStack.Last();

			if (CurrentLayerFolderQueue.Num() <= 0)		// Error state
				return nullptr;

			bool bParentFolderVisible = CurrentLayerFolderQueue[0]->IsContentShown();

			bool bIsSelectedTabVisible = false;

			// If its parent folder is visible, display current parameter,
			// Otherwise, just prune the stacks.
			if (bParentFolderVisible)
			{
				int32 ParentFolderId = MainParam->GetParentParmId();

				// If the current parameter is a folder, its parent is a folderlist.
				// So we need to continue to get the parent of the folderlist.
				if (MainParam->GetParameterType() == EHoudiniParameterType::Folder) 
				{
					if (AllFoldersAndFolderLists.Contains(ParentFolderId))
						ParentFolderId = AllFoldersAndFolderLists[ParentFolderId]->GetParentParmId();
					else
						return nullptr;   // error state
				}

				UHoudiniParameterFolder* ParentFolder = nullptr;

				if (AllFoldersAndFolderLists.Contains(ParentFolderId))
					ParentFolder = Cast<UHoudiniParameterFolder>(AllFoldersAndFolderLists[ParentFolderId]);

				bool bShouldDisplayRow = MainParam->ShouldDisplay();

				// This row should be shown if its parent folder is shown.
				if (ParentFolder)
					bShouldDisplayRow &= (ParentFolder->IsTab() && ParentFolder->IsChosen()) || (!ParentFolder->IsTab() && ParentFolder->IsExpanded());

				if (bShouldDisplayRow)
				{
					if (MainParam->GetParameterType() != EHoudiniParameterType::FolderList)
						Row = &(HouParameterCategory.AddCustomRow(FText::GetEmpty()));					
				}
			}

			// prune the stack finally
			if (bDecreaseChildCount)
			{
				CurrentLayerFolderQueue[0]->GetChildCounter() -= 1;
				PruneStack();
			}
		}
		// If this parameter is in the root dir, just create a row.
		else
		{
			if (MainParam->ShouldDisplay())
			{
				if (MainParam->GetParameterType() != EHoudiniParameterType::FolderList)
					Row = &(HouParameterCategory.AddCustomRow(FText::GetEmpty()));
			}
		}
	}

	if (!MainParam->IsVisible())
		return nullptr;

	
	if (Row)
		CurrentTabEndingRow = nullptr;

	return Row;
}

void
FHoudiniParameterDetails::HandleUnsupportedParmType(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
{
	if (InParams.Num() < 1)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;
	
	CreateNestedRow(HouParameterCategory, InParams);
}

void
FHoudiniParameterDetails::CreateWidgetFloat(
	IDetailCategoryBuilder & HouParameterCategory,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams )
{
	TArray<TWeakObjectPtr<UHoudiniParameterFloat>> FloatParams;
	if (!CastParameters<UHoudiniParameterFloat>(InParams, FloatParams))
		return;

	if (FloatParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFloat>& MainParam = FloatParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	// Helper function to find a unit from a string (name or abbreviation) 
	auto ParmUnit = FUnitConversion::UnitFromString(*(MainParam->GetUnit()));
	EUnit Unit = EUnit::Unspecified;
	if (FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet())
		Unit = ParmUnit.GetValue();

	TSharedPtr<INumericTypeInterface<float>> paramTypeInterface;
	paramTypeInterface = MakeShareable(new TNumericUnitTypeInterface<float>(Unit));
	
	// Lambdas for slider begin
	auto SliderBegin = [](const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
	{
		if (FloatParams.Num() == 0)
			return;

		if (!IsValidWeakPointer(FloatParams[0]))
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
			FloatParams[0]->GetOuter());

		for (int Idx = 0; Idx < FloatParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(FloatParams[Idx]))
				continue;
			
			FloatParams[Idx]->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEnd = [](const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < FloatParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(FloatParams[Idx]))
				continue;

			FloatParams[Idx]->MarkChanged(true);
		}
	};

	// Lambdas for changing the parameter value
	auto ChangeFloatValueAt = [](const float& Value, const int32& Index, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
	{
		if (FloatParams.Num() == 0)
			return;

		if (!IsValidWeakPointer(FloatParams[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
			FloatParams[0]->GetOuter() );

		bool bChanged = false;
		for (int Idx = 0; Idx < FloatParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(FloatParams[Idx]))
				continue;

			FloatParams[Idx]->Modify();
			if (FloatParams[Idx]->SetValueAt(Value, Index))
			{
				// Only mark the param has changed if DoChange is true!!!
				if(DoChange)
					FloatParams[Idx]->MarkChanged(true);
				bChanged = true;
			}
		}

		if (!bChanged || !DoChange)
		{
			// Cancel the transaction if no parameter's value has actually been changed
			Transaction.Cancel();
		}		
	};

	auto RevertToDefault = [](const int32& TupleIndex, const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
	{
		if (FloatParams.Num() == 0)
			return FReply::Handled();

		if (!IsValidWeakPointer(FloatParams[0]))
			return FReply::Handled();
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Revert to default value"),
			FloatParams[0]->GetOuter());

		if (TupleIndex < 0) 
		{
			for (int32 Idx = 0; Idx < FloatParams.Num(); Idx++) 
			{
				if (!IsValidWeakPointer(FloatParams[Idx]))
					continue;

				if (FloatParams[Idx]->IsDefault())
					continue;

				FloatParams[Idx]->RevertToDefault(-1);
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < FloatParams.Num(); Idx++)
			{
				if (!IsValidWeakPointer(FloatParams[Idx]))
					continue;

				if (FloatParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
					continue;

				FloatParams[Idx]->RevertToDefault(TupleIndex);
			}
		}
		return FReply::Handled();
	};
	

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	//TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);
	if (MainParam->GetTupleSize() == 3)
	{
		// Should we swap Y and Z fields (only relevant for Vector3)
		// Ignore the swapping if that parameter has the noswap tag
		bool SwapVector3 = !MainParam->GetNoSwap();

		auto ChangeFloatValueUniformly = [FloatParams, ChangeFloatValueAt](const float& Val, const bool& bDoChange) 
		{
			ChangeFloatValueAt(Val, 0, bDoChange, FloatParams);
			ChangeFloatValueAt(Val, 1, bDoChange, FloatParams);
			ChangeFloatValueAt(Val, 2, bDoChange, FloatParams);
		};

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVectorInputBox)
				.bColorAxisLabels(true)
				.AllowSpin(true)
				.X(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, 0)))
				.Y(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, SwapVector3 ? 2 : 1)))
				.Z(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, SwapVector3 ? 1 : 2)))
				.OnXCommitted_Lambda( [ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val, ETextCommit::Type TextCommitType)
				{ 
					if (MainParam->IsUniformLocked())
						ChangeFloatValueUniformly(Val, true);
					else
						ChangeFloatValueAt( Val, 0, true, FloatParams);
				})
				.OnYCommitted_Lambda( [ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val, ETextCommit::Type TextCommitType)
				{
					if (MainParam->IsUniformLocked())
						ChangeFloatValueUniformly(Val, true);
					else
						ChangeFloatValueAt( Val, SwapVector3 ? 2 : 1, true, FloatParams); 
				})
				.OnZCommitted_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val, ETextCommit::Type TextCommitType)
				{
					if (MainParam->IsUniformLocked())
						ChangeFloatValueUniformly(Val, true);
					else
						ChangeFloatValueAt( Val, SwapVector3 ? 1 : 2, true, FloatParams); 
				})
				.OnXChanged_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val)
				{
					if (MainParam->IsUniformLocked())
						ChangeFloatValueUniformly(Val, false);
					else
						ChangeFloatValueAt(Val, 0, false, FloatParams);
				})
				.OnYChanged_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val)
				{
					if (MainParam->IsUniformLocked())
						ChangeFloatValueUniformly(Val, false);
					else
						ChangeFloatValueAt(Val, SwapVector3 ? 2 : 1, false, FloatParams);
				})
				.OnZChanged_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val)
				{
					if (MainParam->IsUniformLocked())
						ChangeFloatValueUniformly(Val, false);
					else
						ChangeFloatValueAt(Val, SwapVector3 ? 1 : 2, false, FloatParams);
				})
				.OnBeginSliderMovement_Lambda([SliderBegin, FloatParams]() { SliderBegin(FloatParams); })
				.OnEndSliderMovement_Lambda([SliderEnd, FloatParams](const float NewValue) { SliderEnd(FloatParams); })
				.TypeInterface(paramTypeInterface)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("FloatParameterLockButtonToolTip", "When locked, change the vector value uniformly."))
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(MainParam->IsUniformLocked() ? FEditorStyle::GetBrush("Genericlock") : FEditorStyle::GetBrush("GenericUnlock"))
					]
					.OnClicked_Lambda([FloatParams, MainParam]()
					{
						if (!IsValidWeakPointer(MainParam))
							return FReply::Handled();

						for (auto & CurParam : FloatParams) 
						{
							if (!IsValidWeakPointer(CurParam))
								continue;

							CurParam->SwitchUniformLock();
						}

						FHoudiniEngineUtils::UpdateEditorProperties(MainParam.Get(), true);

						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.Visibility_Lambda([FloatParams]()
					{
						for (auto & SelectedParam : FloatParams)
						{
							if (!IsValidWeakPointer(SelectedParam))
								continue;

							if (!SelectedParam->IsDefault())
								return EVisibility::Visible;
						}

						return EVisibility::Hidden;
					})
					.OnClicked_Lambda([FloatParams, RevertToDefault]() { return RevertToDefault(-1, FloatParams); })
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			]
		];
	}
	else
	{
		for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericEntryBox;
			VerticalBox->AddSlot()
			.Padding(2, 2, 5, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(NumericEntryBox, SNumericEntryBox< float >)
					.AllowSpin(true)

					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

					.MinValue(MainParam->GetMin())
					.MaxValue(MainParam->GetMax())

					.MinSliderValue(MainParam->GetUIMin())
					.MaxSliderValue(MainParam->GetUIMax())

					.Value(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, Idx)))
					.OnValueChanged_Lambda([ChangeFloatValueAt, Idx, FloatParams](float Val) { ChangeFloatValueAt(Val, Idx, false, FloatParams); })
					.OnValueCommitted_Lambda([ChangeFloatValueAt, Idx, FloatParams](float Val, ETextCommit::Type TextCommitType) {	ChangeFloatValueAt(Val, Idx, true, FloatParams); })
					.OnBeginSliderMovement_Lambda([SliderBegin, FloatParams]() { SliderBegin(FloatParams); })
					.OnEndSliderMovement_Lambda([SliderEnd, FloatParams](const float NewValue) { SliderEnd(FloatParams); })
					.SliderExponent(MainParam->IsLogarithmic() ?8.0f : 1.0f)
					.TypeInterface(paramTypeInterface)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.OnClicked_Lambda([Idx, FloatParams, RevertToDefault]() { return RevertToDefault(Idx, FloatParams); })
					.Visibility_Lambda([Idx, FloatParams]()
					{
						for (auto & SelectedParam :FloatParams)
						{
							if (!IsValidWeakPointer(SelectedParam))
								continue;

							if (!SelectedParam->IsDefaultValueAtIndex(Idx))
								return EVisibility::Visible;
						}

						return EVisibility::Hidden;
					})
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			];
		}
	}

	Row->ValueWidget.Widget =VerticalBox;

	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void
FHoudiniParameterDetails::CreateWidgetInt(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterInt>> IntParams;
	if (!CastParameters<UHoudiniParameterInt>(InParams, IntParams))

	if (IntParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterInt>& MainParam = IntParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	// Helper function to find a unit from a string (name or abbreviation) 
	auto ParmUnit = FUnitConversion::UnitFromString(*(MainParam->GetUnit()));
	EUnit Unit = EUnit::Unspecified;
	if (FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet())
		Unit = ParmUnit.GetValue();

	TSharedPtr<INumericTypeInterface<int32>> paramTypeInterface;
	paramTypeInterface = MakeShareable(new TNumericUnitTypeInterface<int32>(Unit));

	// Lambda for slider begin
	auto SliderBegin = [](const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
	{
		if (IntParams.Num() == 0)
			return;

		if (!IsValidWeakPointer(IntParams[0]))
			return;
		
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
			IntParams[0]->GetOuter());

		for (int Idx = 0; Idx < IntParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(IntParams[Idx]))
				continue;
			
			IntParams[Idx]->Modify();
		}
	};
	
	// Lambda for slider end
	auto SliderEnd = [](const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < IntParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(IntParams[Idx]))
				continue;
			
			IntParams[Idx]->MarkChanged(true);
		}
	};
	
	// Lambda for changing the parameter value
	auto ChangeIntValueAt = [](const int32& Value, const int32& Index, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
	{
		if (IntParams.Num() == 0)
			return;

		if (!IsValidWeakPointer(IntParams[0]))
			return;
		
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
			IntParams[0]->GetOuter());

		bool bChanged = false;
		for (int Idx = 0; Idx < IntParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(IntParams[Idx]))
				continue;

			IntParams[Idx]->Modify();
			if (IntParams[Idx]->SetValueAt(Value, Index)) 
			{
				// Only mark the param has changed if DoChange is true!!!
				if (DoChange)
					IntParams[Idx]->MarkChanged(true);
				bChanged = true;
			}
		}

		if (!bChanged || !DoChange)
		{
			// Cancel the transaction if there is no param has actually been changed
			Transaction.Cancel();
		}
	};

	auto RevertToDefault = [](const int32& TupleIndex, const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
	{
		for (int32 Idx = 0; Idx < IntParams.Num(); Idx++) 
		{
			if (!IsValidWeakPointer(IntParams[Idx]))
				continue;

			if (IntParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
				continue;

			IntParams[Idx]->RevertToDefault(TupleIndex);
		}
			
		return FReply::Handled();
	};

	for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(true)

				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(MainParam->GetMin())
				.MaxValue(MainParam->GetMax())

				.MinSliderValue(MainParam->GetUIMin())
				.MaxSliderValue(MainParam->GetUIMax())

				.Value( TAttribute<TOptional<int32>>::Create(TAttribute<TOptional<int32>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterInt::GetValue, Idx)))
				.OnValueChanged_Lambda( [=](int32 Val) { ChangeIntValueAt(Val, Idx, false, IntParams); } )
				.OnValueCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType) { ChangeIntValueAt(Val, Idx, true, IntParams); })
				.OnBeginSliderMovement_Lambda( [=]() { SliderBegin(IntParams); })
				.OnEndSliderMovement_Lambda([=](const float NewValue) { SliderEnd(IntParams); })
				.SliderExponent(MainParam->IsLogarithmic() ? 8.0f : 1.0f)
				.TypeInterface(paramTypeInterface)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility_Lambda([Idx, IntParams]()
				{
					for (auto & NextSelectedParam : IntParams) 
					{
						if (!IsValidWeakPointer(NextSelectedParam))
							continue;
	
						if (!NextSelectedParam->IsDefaultValueAtIndex(Idx))
							return EVisibility::Visible;
					}

					return EVisibility::Hidden;
				})
				.OnClicked_Lambda([Idx, IntParams, RevertToDefault]() { return RevertToDefault(Idx, IntParams); })
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
		/*
		if (NumericEntryBox.IsValid())
			NumericEntryBox->SetEnabled(!MainParam->IsDisabled());
		*/
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetString( IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterString>> StringParams;
	if (!CastParameters<UHoudiniParameterString>(InParams, StringParams))
		return;

	if (StringParams.Num() <= 0)
		return;
	
	const TWeakObjectPtr<UHoudiniParameterString>& MainParam = StringParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	bool bIsMultiLine = false;
	bool bIsUnrealRef = false;
	UClass* UnrealRefClass = UObject::StaticClass();

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	TMap<FString, FString>& Tags = MainParam->GetTags();
	if (Tags.Contains(HOUDINI_PARAMETER_STRING_REF_TAG) && FCString::Atoi(*Tags[HOUDINI_PARAMETER_STRING_REF_TAG]) == 1) 
	{
		bIsUnrealRef = true;

		if (Tags.Contains(HOUDINI_PARAMETER_STRING_REF_CLASS_TAG))
		{
			UClass * FoundClass = FindObject<UClass>(ANY_PACKAGE, *Tags[HOUDINI_PARAMETER_STRING_REF_CLASS_TAG]);
			if (FoundClass != nullptr)
			{
				UnrealRefClass = FoundClass;
			}
		}
	}

	if (Tags.Contains(HOUDINI_PARAMETER_STRING_MULTILINE_TAG)) 
	{
		bIsMultiLine = true;
	}

	for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		// Lambda for changing the parameter value
		auto ChangeStringValueAt = [](const FString& Value, UObject* ChosenObj, const int32& Index, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniParameterString>>& StringParams)
		{
			if (StringParams.Num() == 0)
				return;

			if (!IsValidWeakPointer(StringParams[0]))
				return;
			
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterSrtingChange", "Houdini Parameter String: Changing a value"),
				StringParams[0]->GetOuter());

			bool bChanged = false;
			for (int Idx = 0; Idx < StringParams.Num(); Idx++)
			{
				if (!IsValidWeakPointer(StringParams[Idx]))
					continue;

				StringParams[Idx]->Modify();
				if (StringParams[Idx]->SetValueAt(Value, Index)) 
				{
					StringParams[Idx]->MarkChanged(true);
					bChanged = true;
				}

				StringParams[Idx]->SetAssetAt(ChosenObj, Index);
			}

			if (!bChanged || !DoChange)
			{
				// Cancel the transaction if there is no param actually has been changed
				Transaction.Cancel();
			}

			FHoudiniEngineUtils::UpdateEditorProperties(StringParams[0].Get(), false);
		};

		auto RevertToDefault = [](const int32& TupleIndex, const TArray<TWeakObjectPtr<UHoudiniParameterString>>& StringParams)
		{
			for (int32 Idx = 0; Idx < StringParams.Num(); Idx++) 
			{
				if (!IsValidWeakPointer(StringParams[Idx]))
					continue;

				if (StringParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
					continue;

				StringParams[Idx]->RevertToDefault(TupleIndex);
			}
				
			return FReply::Handled();
		};

		if (bIsUnrealRef)
		{
			TSharedPtr<SEditableTextBox> EditableTextBox;
			TSharedPtr<SHorizontalBox> HorizontalBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2)
			[
				SNew(SAssetDropTarget)
				.OnAreAssetsAcceptableForDrop_Lambda([UnrealRefClass](TArrayView<FAssetData> InAssets)
				{
					return InAssets[0].GetAsset()->IsA(UnrealRefClass);
				})
				.OnAssetsDropped_Lambda([=](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					// Get the asset reference string for this object
					UObject* InObject = InAssets[0].GetAsset();
					FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InObject);
					ChangeStringValueAt(ReferenceStr, InObject, Idx, true, StringParams);
				})
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
				]
			];

			// Thumbnail
			// Get thumbnail pool for this builder.
			TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouParameterCategory.GetParentLayout().GetThumbnailPool();
			
			// Create a thumbnail for the selected object / class
			UObject* EditObject = nullptr;
			const FString AssetPath = MainParam->GetValueAt(Idx);
			EditObject = StaticFindObject(nullptr, nullptr, *AssetPath, true);
			
			FAssetData AssetData;
			if (IsValid(EditObject))
			{
				AssetData = FAssetData(EditObject);
			}
			else
			{
				AssetData.AssetClass = UnrealRefClass->GetFName();
			}
			
			TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
				new FAssetThumbnail(AssetData, 64, 64, AssetThumbnailPool));

			TSharedPtr<SBorder> ThumbnailBorder;
			HorizontalBox->AddSlot().Padding(0.f, 0.f, 2.f, 0.f).AutoWidth()
			[
				SAssignNew(ThumbnailBorder, SBorder)
				.OnMouseDoubleClick_Lambda([EditObject, Idx](const FGeometry&, const FPointerEvent&)
				{
					if (EditObject && GEditor) 
						GEditor->EditObject(EditObject);
					
					return FReply::Handled();
				})
				.Padding(5.f)
				[
					SNew(SBox)
					.WidthOverride(64)
					.HeightOverride(64)
					[
						StaticMeshThumbnail->MakeThumbnailWidget()
					]
				]
			];

			TWeakPtr<SBorder> WeakThumbnailBorder(ThumbnailBorder);
			ThumbnailBorder->SetBorderImage(TAttribute<const FSlateBrush *>::Create(
				TAttribute<const FSlateBrush *>::FGetter::CreateLambda(
					[WeakThumbnailBorder]()
					{
						TSharedPtr<SBorder> ThumbnailBorderPtr = WeakThumbnailBorder.Pin();
						if (ThumbnailBorderPtr.IsValid() && ThumbnailBorderPtr->IsHovered())
							return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
						else
							return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
					}
				)
			));

			FText MeshNameText = FText::GetEmpty();
			//if (InputObject)
			//	MeshNameText = FText::FromString(InputObject->GetName());

			TSharedPtr<SComboButton> StaticMeshComboButton;

			TSharedPtr<SHorizontalBox> ButtonBox;
			HorizontalBox->AddSlot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SAssignNew(ButtonBox, SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(StaticMeshComboButton, SComboButton)
						.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
						.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
						.ContentPadding(2.0f)
						.ButtonContent()
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
							.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
							.Text(FText::FromName(AssetData.AssetName))
							.ToolTipText(FText::FromString(MainParam->GetValueAt(Idx)))
						]
					]
				]
			];
			
			TWeakPtr<SComboButton> WeakStaticMeshComboButton(StaticMeshComboButton);
			StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
				[UnrealRefClass, WeakStaticMeshComboButton, ChangeStringValueAt, Idx, StringParams]()
			{
				TArray<const UClass *> AllowedClasses;
				if (UnrealRefClass != UObject::StaticClass())
				{
					// Use the class specified by the user
					AllowedClasses.Add(UnrealRefClass);
				}
				else
				{
					// Using UObject would list way too many assets, and take a long time to open the menu,
					// so we need to reestrict the classes a bit
					AllowedClasses.Add(UStaticMesh::StaticClass());
					AllowedClasses.Add(UHoudiniAsset::StaticClass());
					AllowedClasses.Add(USkeletalMesh::StaticClass());
					AllowedClasses.Add(UBlueprint::StaticClass());
					AllowedClasses.Add(UMaterialInterface::StaticClass());
					AllowedClasses.Add(UTexture::StaticClass());
					AllowedClasses.Add(ULevel::StaticClass());
					AllowedClasses.Add(UStreamableRenderAsset::StaticClass());
					AllowedClasses.Add(USoundBase::StaticClass());
					AllowedClasses.Add(UParticleSystem::StaticClass());
					AllowedClasses.Add(UFoliageType::StaticClass());
				}

				TArray<UFactory *> NewAssetFactories;
				return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
					FAssetData(nullptr),
					true,
					AllowedClasses,
					NewAssetFactories,
					FOnShouldFilterAsset(),
					FOnAssetSelected::CreateLambda(
						[WeakStaticMeshComboButton, ChangeStringValueAt, Idx, StringParams](const FAssetData & AssetData)
						{
							TSharedPtr<SComboButton> StaticMeshComboButtonPtr = WeakStaticMeshComboButton.Pin();
							if (StaticMeshComboButtonPtr.IsValid())
							{
								StaticMeshComboButtonPtr->SetIsOpen(false);

								UObject * Object = AssetData.GetAsset();
								// Get the asset reference string for this object
								// !! Accept null objects to allow clearing the asset picker !!
								FString ReferenceStr = UHoudiniParameterString::GetAssetReference(Object);

								ChangeStringValueAt(ReferenceStr, Object, Idx, true, StringParams);
							}
						}
					),
					FSimpleDelegate::CreateLambda([]() {}));
				})
			);
		}
		else if (bIsMultiLine) 
		{
			TSharedPtr< SMultiLineEditableTextBox > MultiLineEditableTextBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SNew(SAssetDropTarget)
				.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
					{return true;})
				.OnAssetsDropped_Lambda([=](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					// Get the asset reference string for this object
					FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InAssets[0].GetAsset());

					FString NewString = ReferenceStr;
					if (StringParams[0]->GetValueAt(Idx).Len() > 0)
						NewString = StringParams[0]->GetValueAt(Idx) + "\n" + NewString;

					ChangeStringValueAt(NewString, nullptr, Idx, true, StringParams);
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top).MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SAssignNew(MultiLineEditableTextBox, SMultiLineEditableTextBox)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(FText::FromString(MainParam->GetValueAt(Idx)))
						.OnTextCommitted_Lambda([=](const FText& Val, ETextCommit::Type TextCommitType) { ChangeStringValueAt(Val.ToString(), nullptr, Idx, true, StringParams); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.ContentPadding(0)
						.Visibility_Lambda([Idx, StringParams]()
						{
							for (auto & NextSelectedParam : StringParams) 
							{
								if (!IsValidWeakPointer(NextSelectedParam))
									continue;

								if (!NextSelectedParam->IsDefaultValueAtIndex(Idx))
									return EVisibility::Visible;
							}

							return EVisibility::Hidden;
						})
						.OnClicked_Lambda([Idx, StringParams, RevertToDefault]() { return RevertToDefault(Idx, StringParams); })
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
						]
					]
				]
			];
		}
		else
		{		
			TSharedPtr< SEditableTextBox > EditableTextBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2)
			[
				SNew(SAssetDropTarget)
				.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
					{return true;})
				.OnAssetsDropped_Lambda([=](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					// Get the asset reference string for this object
					FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InAssets[0].GetAsset());

					ChangeStringValueAt(ReferenceStr, nullptr, Idx, true, StringParams);
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SAssignNew(EditableTextBox, SEditableTextBox)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(FText::FromString(MainParam->GetValueAt(Idx)))
						.OnTextCommitted_Lambda([=](const FText& Val, ETextCommit::Type TextCommitType) 
							{ ChangeStringValueAt(Val.ToString(), nullptr, Idx, true, StringParams); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.ContentPadding(0)
						.Visibility_Lambda([Idx, StringParams]()
						{
							for (auto & NextSelectedParam : StringParams)
							{
								if (!IsValidWeakPointer(NextSelectedParam))
									continue;

								if (!NextSelectedParam->IsDefaultValueAtIndex(Idx))
									return EVisibility::Visible;
							}	

							return EVisibility::Hidden;
						})
						.OnClicked_Lambda([Idx, StringParams, RevertToDefault]()
							{ return RevertToDefault(Idx, StringParams); })
						[
							SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
						]
					]
				]
			];  
		} 
		
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetColor(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterColor>> ColorParams;
	if (!CastParameters<UHoudiniParameterColor>(InParams, ColorParams))
		return;

	if (ColorParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterColor>& MainParam = ColorParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;
		// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	bool bHasAlpha = (MainParam->GetTupleSize() == 4);

	// Add color picker UI.
	TSharedPtr<SColorBlock> ColorBlock;
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ColorBlock, SColorBlock)
		.Color(MainParam->GetColorValue())
		.ShowBackgroundForAlpha(bHasAlpha)
		.OnMouseButtonDown_Lambda([this, ColorParams, MainParam, bHasAlpha](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
			{
				FColorPickerArgs PickerArgs;
				PickerArgs.ParentWidget = FSlateApplication::Get().GetActiveTopLevelWindow();
				PickerArgs.bUseAlpha = bHasAlpha;
				PickerArgs.DisplayGamma = TAttribute< float >::Create(
					TAttribute< float >::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
				PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([MainParam, ColorParams](FLinearColor InColor) 
				{
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniParameterColorChange", "Houdini Parameter Color: Changing value"),
						MainParam->GetOuter(), true);

					bool bChanged = false;
					for (auto & Param : ColorParams) 
					{
						if (!IsValidWeakPointer(Param))
							continue;

						Param->Modify();
						if (Param->SetColorValue(InColor)) 
						{
							Param->MarkChanged(true);
							bChanged = true;
						}
					}

					// cancel the transaction if there is actually no value changed
					if (!bChanged)
					{
						Transaction.Cancel();
					}
				});
				PickerArgs.InitialColorOverride = MainParam->GetColorValue();
				PickerArgs.bOnlyRefreshOnOk = true;
				OpenColorPicker(PickerArgs);
				return FReply::Handled();
			})
	];

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void 
FHoudiniParameterDetails::CreateWidgetButton(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterButton>> ButtonParams;
	if (!CastParameters<UHoudiniParameterButton>(InParams, ButtonParams))
		return;

	if (ButtonParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterButton>& MainParam = ButtonParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());
	FText ParameterTooltip = GetParameterTooltip(MainParam);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SButton> Button;

	// Add button UI.
	HorizontalBox->AddSlot().Padding(1, 2, 4, 2)
	[
		SAssignNew(Button, SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(ParameterLabelText)
		.ToolTipText(ParameterTooltip)
		.OnClicked(FOnClicked::CreateLambda( [MainParam, ButtonParams]()
		{
			for (auto & Param : ButtonParams) 
			{
				if (!IsValidWeakPointer(Param))
					continue;

				// There is no undo redo operation for button
				Param->MarkChanged(true);
			}

			return FReply::Handled();
		}))
	];

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void 
FHoudiniParameterDetails::CreateWidgetButtonStrip(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterButtonStrip>> ButtonStripParams;
	if (!CastParameters<UHoudiniParameterButtonStrip>(InParams, ButtonStripParams))
		return;

	if (ButtonStripParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterButtonStrip>& MainParam = ButtonStripParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	if (!Row)
		return;

	auto OnButtonStateChanged = [MainParam, ButtonStripParams](ECheckBoxState NewState, int32 Idx) 
	{
	
		/*
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterButtonStripChange", "Houdini Parameter Button Strip: Changing value"),
			MainParam->GetOuter(), true);
		*/
		int32 StateInt = NewState == ECheckBoxState::Checked ? 1 : 0;
		bool bChanged = false;

		for (auto & NextParam : ButtonStripParams)
		{
			if (!IsValidWeakPointer(NextParam))
				continue;

			if (!NextParam->Values.IsValidIndex(Idx))
				continue;

			//NextParam->Modify();
			if (NextParam->SetValueAt(Idx, StateInt)) 
			{
				NextParam->MarkChanged(true);
				bChanged = true;
			}
		}

		//if (!bChanged)
		//	Transaction.Cancel();
	
	};


	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());
	FText ParameterTooltip = GetParameterTooltip(MainParam);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	FLinearColor BgColor(0.53f, 0.81f, 0.82f, 1.0f);   // Sky Blue Backgroud color

	for (int32 Idx = 0; Idx < MainParam->Count; ++Idx) 
	{
		if (!MainParam->Values.IsValidIndex(Idx) || !MainParam->Labels.IsValidIndex(Idx))
			continue;

		bool bPressed = MainParam->Values[Idx] > 0;
		FText LabelText = FText::FromString(MainParam->Labels[Idx]);

		TSharedPtr<SCheckBox> Button;

		HorizontalBox->AddSlot().Padding(0).FillWidth(1.0f)
		[
			SAssignNew(Button, SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
			.IsChecked(bPressed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([OnButtonStateChanged, Idx](ECheckBoxState NewState)
			{
				OnButtonStateChanged(NewState, Idx);
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LabelText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		Button->SetColorAndOpacity(BgColor);
	}

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.MaxDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void
FHoudiniParameterDetails::CreateWidgetLabel(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterLabel>> LabelParams;
	if (!CastParameters<UHoudiniParameterLabel>(InParams, LabelParams))
		return;

	if (LabelParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterLabel>& MainParam = LabelParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	TSharedRef <SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for (int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index) 
	{
		FString NextLabelString = MainParam->GetStringAtIndex(Index);
		FText ParameterLabelText = FText::FromString(NextLabelString);
		
		TSharedPtr<STextBlock> TextBlock;

		// Add Label UI.
		VerticalBox->AddSlot().Padding(1, 2, 4, 2)
		[
			SAssignNew(TextBlock, STextBlock).Text(ParameterLabelText)
		];
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void 
FHoudiniParameterDetails::CreateWidgetToggle(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterToggle>> ToggleParams;
	if (!CastParameters<UHoudiniParameterToggle>(InParams, ToggleParams))
		return;

	if (ToggleParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterToggle>& MainParam = ToggleParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	auto IsToggleCheckedLambda = [MainParam](int32 Index)
	{
		if (Index >= MainParam->GetNumValues())
			return ECheckBoxState::Unchecked;

		if (MainParam->GetValueAt(Index))
			return ECheckBoxState::Checked;

		return ECheckBoxState::Unchecked;
	};

	auto OnToggleCheckStateChanged = [MainParam, ToggleParams](ECheckBoxState NewState, int32 Index) 
	{
		if (Index >= MainParam->GetNumValues())
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterToggleChange", "Houdini Parameter Toggle: Changing value"),
			MainParam->GetOuter(), true);

		bool bState = (NewState == ECheckBoxState::Checked);

		bool bChanged = false;
		for (auto & Param : ToggleParams) 
		{
			if (!IsValidWeakPointer(Param))
				continue;

			Param->Modify();
			if (Param->SetValueAt(bState, Index)) 
			{
				bChanged = true;
				Param->MarkChanged(true);
			}
		}

		// Cancel the transaction if no parameter has actually been changed
		if (!bChanged)
		{
			Transaction.Cancel();
		}
	};

	for (int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index) 
	{
		TSharedPtr< SCheckBox > CheckBox;
		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
			[
				SAssignNew(CheckBox, SCheckBox)
				.OnCheckStateChanged_Lambda([OnToggleCheckStateChanged, Index](ECheckBoxState NewState) {
					OnToggleCheckStateChanged(NewState, Index);

				})
				.IsChecked_Lambda([IsToggleCheckedLambda, Index]() {
					return IsToggleCheckedLambda(Index);
				})
				.Content()
				[
					SNew(STextBlock)
					.Text(ParameterLabelText)
					.ToolTipText(GetParameterTooltip(MainParam))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			];
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void FHoudiniParameterDetails::CreateWidgetFile(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterFile>> FileParams;
	if (!CastParameters<UHoudiniParameterFile>(InParams, FileParams))
		return;

	if (FileParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFile>& MainParam = FileParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	FString FileTypeWidgetFilter = TEXT("All files (*.*)|*.*");
	if (!MainParam->GetFileFilters().IsEmpty())
		FileTypeWidgetFilter = FString::Printf(TEXT("%s files (%s)|%s"), *MainParam->GetFileFilters(), *MainParam->GetFileFilters(), *MainParam->GetFileFilters());

	FString BrowseWidgetDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);

	TMap<FString, FString>& Tags = MainParam->GetTags();
	if (Tags.Contains(HAPI_PARAM_TAG_DEFAULT_DIR)) 
	{
		if(!Tags[HAPI_PARAM_TAG_DEFAULT_DIR].IsEmpty())
		{
			FString DefaultDir = Tags[HAPI_PARAM_TAG_DEFAULT_DIR];
			if(FPaths::DirectoryExists(DefaultDir))
				BrowseWidgetDirectory = DefaultDir;
		}
	}

	auto UpdateCheckRelativePath = [MainParam](const FString & PickedPath) 
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(MainParam->GetOuter());
		if (MainParam->GetOuter() && !PickedPath.IsEmpty() && FPaths::IsRelative(PickedPath))
		{
			// Check if the path is relative to the UE4 project
			FString AbsolutePath = FPaths::ConvertRelativePathToFull(PickedPath);
			if (FPaths::FileExists(AbsolutePath))
			{
				return AbsolutePath;
			}
			
			// Check if the path is relative to the asset
			if (IsValid(HoudiniAssetComponent))
			{
				if (IsValid(HoudiniAssetComponent->HoudiniAsset))
				{
					FString AssetFilePath = FPaths::GetPath(HoudiniAssetComponent->HoudiniAsset->AssetFileName);
					if (FPaths::FileExists(AssetFilePath))
					{
						FString UpdatedFileWidgetPath = FPaths::Combine(*AssetFilePath, *PickedPath);
						if (FPaths::FileExists(UpdatedFileWidgetPath))
						{
							return UpdatedFileWidgetPath;
						}
					}
				}
			}
		}

		return PickedPath;
	};

	for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		FString FileWidgetPath = MainParam->GetValueAt(Idx);
		FString FileWidgetBrowsePath = BrowseWidgetDirectory;

		if (!FileWidgetPath.IsEmpty())
		{
			FString FileWidgetDirPath = FPaths::GetPath(FileWidgetPath);
			if (!FileWidgetDirPath.IsEmpty())
				FileWidgetBrowsePath = FileWidgetDirPath;
		}
					
		bool IsDirectoryPicker = MainParam->GetParameterType() == EHoudiniParameterType::FileDir;
		bool bIsNewFile = !MainParam->IsReadOnly();

		FText BrowseTooltip = LOCTEXT("FileButtonToolTipText", "Choose a file from this computer");
		if (IsDirectoryPicker)
			BrowseTooltip = LOCTEXT("DirButtonToolTipText", "Choose a directory from this computer");

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.0f).MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				SNew(SNewFilePathPicker)
				.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
				.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.BrowseButtonToolTip(BrowseTooltip)
				.BrowseDirectory(FileWidgetBrowsePath)
				.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
				.FilePath(FileWidgetPath)
				.FileTypeFilter(FileTypeWidgetFilter)
				.IsNewFile(bIsNewFile)
				.IsDirectoryPicker(IsDirectoryPicker)
				.ToolTipText_Lambda([MainParam]()
				{
					// return the current param value as a tooltip
					FString FileValue = MainParam.IsValid() ? MainParam->GetValueAt(0) : FString();
					return FText::FromString(FileValue);
				})
				.OnPathPicked(FOnPathPicked::CreateLambda([MainParam, FileParams, UpdateCheckRelativePath, Idx](const FString & PickedPath) 
				{
					if (MainParam->GetNumValues() <= Idx)
						return;
				
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniParameterFileChange", "Houdini Parameter File: Changing a file path"),
						MainParam->GetOuter(), true);

					bool bChanged = false;

					for (auto & Param : FileParams) 
					{
						if (!IsValidWeakPointer(Param))
							continue;

						Param->Modify();
						if (Param->SetValueAt(UpdateCheckRelativePath(PickedPath), Idx)) 
						{
							bChanged = true;
							Param->MarkChanged(true);
						}	
					}

					// Cancel the transaction if no value has actually been changed
					if (!bChanged) 
					{
						Transaction.Cancel();
					}
				}))
			]
		];

	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}


void
FHoudiniParameterDetails::CreateWidgetChoice(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterChoice>> ChoiceParams;
	if (!CastParameters<UHoudiniParameterChoice>(InParams, ChoiceParams))
		return;

	if (ChoiceParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterChoice>& MainParam = ChoiceParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	// Lambda for changing the parameter value
	auto ChangeSelectionLambda = [ChoiceParams](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType) 
	{
		if (!NewChoice.IsValid())
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterChoiceChange", "Houdini Parameter Choice: Changing selection"),
			ChoiceParams[0]->GetOuter());

		const int32 NewIntValue = ChoiceParams[0]->GetIntValueFromLabel(*NewChoice);

		bool bChanged = false;
		for (int Idx = 0; Idx < ChoiceParams.Num(); Idx++)
		{
			if (!IsValidWeakPointer(ChoiceParams[Idx]))
				continue;

			ChoiceParams[Idx]->Modify();
			if (ChoiceParams[Idx]->SetIntValue(NewIntValue)) 
			{
				bChanged = true;
				ChoiceParams[Idx]->MarkChanged(true);
				ChoiceParams[Idx]->UpdateStringValueFromInt();
			}
		}

		if (!bChanged)
		{
			// Cancel the transaction if no parameter was changed
			Transaction.Cancel();
		}
	};

	// 
	MainParam->UpdateChoiceLabelsPtr();
	TArray<TSharedPtr<FString>>* OptionSource = MainParam->GetChoiceLabelsPtr();
	TSharedPtr<FString> IntialSelec;
	if (OptionSource && OptionSource->IsValidIndex(MainParam->GetIntValueIndex()))
	{
		IntialSelec = (*OptionSource)[MainParam->GetIntValueIndex()];
	}

	TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBox;
	HorizontalBox->AddSlot().Padding( 2, 2, 5, 2 )
	[
		SAssignNew( ComboBox, SComboBox< TSharedPtr< FString > > )
		.OptionsSource(OptionSource)
		.InitiallySelectedItem(IntialSelec)
		.OnGenerateWidget_Lambda(
			[]( TSharedPtr< FString > InItem ) 
			{
				return SNew(STextBlock).Text(FText::FromString(*InItem));
			})
		.OnSelectionChanged_Lambda(
			[ChangeSelectionLambda](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				ChangeSelectionLambda(NewChoice, SelectType);
			})
		[
			SNew(STextBlock)
			.Text_Lambda([MainParam]() { return FText::FromString(MainParam->GetLabel()); })
			.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
	];

	if ( ComboBox.IsValid() )
		ComboBox->SetEnabled( !MainParam->IsDisabled() );

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetSeparator(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, const bool& InIsEnabled)
{
	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	TSharedRef<SCustomizedBox> HorizontalBox = SNew(SCustomizedBox);

	HorizontalBox->DividerLinePositions = DividerLinePositions;
	HorizontalBox->SetHoudiniParameter(InParams);

	Row->WholeRowWidget.Widget = HorizontalBox;
}

void 
FHoudiniParameterDetails::CreateWidgetOperatorPath(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterOperatorPath>> OperatorPathParams;
	if (!CastParameters<UHoudiniParameterOperatorPath>(InParams, OperatorPathParams))
		return;

	if (OperatorPathParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterOperatorPath>& MainParam = OperatorPathParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = MainParam->HoudiniInput;
	if (!IsValidWeakPointer(MainInput))
		return;

	// Build an array of edited inputs for multi edition
	TArray<TWeakObjectPtr<UHoudiniInput>> EditedInputs;
	EditedInputs.Add(MainInput);

	// Add the corresponding inputs found in the other HAC
	for (int LinkedIdx = 1; LinkedIdx < OperatorPathParams.Num(); LinkedIdx++)
	{
		UHoudiniInput* LinkedInput = OperatorPathParams[LinkedIdx]->HoudiniInput.Get();
		if (!IsValid(LinkedInput))
			continue;

		// Linked params should match the main param! If not try to find one that matches
		if (!LinkedInput->Matches(*MainInput))
			continue;

		EditedInputs.Add(LinkedInput);
	}

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);
	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FHoudiniInputDetails::CreateWidget(HouParameterCategory, EditedInputs, Row);

	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void
FHoudiniParameterDetails::CreateWidgetFloatRamp(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams) 
{
	if (InParams.Num() < 1)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// TODO: remove this once we have verified that updating the Points and CachedPoints arrays in
	// TODO: HoudiniParameterTranslator::BuildAllParameters() (via RampParam->UpdatePointsArray()) is sufficient.
	// //  Parsing a float ramp: 1->(2->3->4)*->5  //
	// switch (MainParam->GetParameterType()) 
	// {
	// 	//*****State 1: Float Ramp*****//
	// 	case EHoudiniParameterType::FloatRamp:
	// 	{
	// 		UHoudiniParameterRampFloat* FloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
	// 		if (FloatRampParameter) 
	// 		{
	// 			CurrentRampFloat = FloatRampParameter;
	// 			CurrentRampFloatPointsArray.Empty();
	//
	// 			CurrentRampParameterList = InParams;
	//
	// 			FDetailWidgetRow *Row = CreateWidgetRampCurveEditor(HouParameterCategory, InParams);
	// 			CurrentRampRow = Row;
	// 		}
	// 		break;
	// 	}
	//
	// 	case EHoudiniParameterType::Float:
	// 	{
	// 		UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(MainParam);
	// 		if (FloatParameter)
	// 		{
	// 			bool bCreateNewPoint = true;
	// 			if (CurrentRampFloatPointsArray.Num() > 0) 
	// 			{
	// 				UHoudiniParameterRampFloatPoint* LastPtInArr = CurrentRampFloatPointsArray.Last();
	// 				if (LastPtInArr && !LastPtInArr->ValueParentParm)
	// 					bCreateNewPoint = false;
	// 			}
	//
	// 			//*****State 2: Float Parameter (position)*****//
	// 			if (bCreateNewPoint)
	// 			{
	// 				UHoudiniParameterRampFloatPoint* NewRampFloatPoint = nullptr;
	//
	// 				int32 PointIndex = CurrentRampFloatPointsArray.Num();
	// 				if (CurrentRampFloat->Points.IsValidIndex(PointIndex))
	// 				{
	//
	// 					// TODO: We should reuse existing point objects, if they exist. Currently
	// 					// this causes results in unexpected behaviour in other parts of this detail code.
	// 					// Give this code a bit of an overhaul at some point.
	// 					// NewRampFloatPoint = CurrentRampFloat->Points[PointIndex];
	// 				}
	//
	// 				if (!NewRampFloatPoint)
	// 				{
	// 					// Create a new float ramp point, and add its pointer to the current float points buffer array.
	// 					NewRampFloatPoint = NewObject< UHoudiniParameterRampFloatPoint >(CurrentRampFloat, FName(), CurrentRampFloat->GetMaskedFlags(RF_PropagateToSubObjects));
	//
	// 				}
	//
	// 				CurrentRampFloatPointsArray.Add(NewRampFloatPoint);
	//
	// 				if (FloatParameter->GetNumberOfValues() <= 0)
	// 					return;
	// 				// Set the float ramp point's position parent parm, and value
	// 				NewRampFloatPoint->PositionParentParm = FloatParameter;
	// 				NewRampFloatPoint->SetPosition(FloatParameter->GetValuesPtr()[0]);
	// 			}
	// 			//*****State 3: Float Parameter (value)*****//
	// 			else 
	// 			{
	// 				if (FloatParameter->GetNumberOfValues() <= 0)
	// 					return;
	// 				// Get the last point in the buffer array
	// 				if (CurrentRampFloatPointsArray.Num() > 0)
	// 				{
	// 					// Set the last inserted float ramp point's float parent parm, and value
	// 					UHoudiniParameterRampFloatPoint* LastAddedFloatRampPoint = CurrentRampFloatPointsArray.Last();
	// 					LastAddedFloatRampPoint->ValueParentParm = FloatParameter;
	// 					LastAddedFloatRampPoint->SetValue(FloatParameter->GetValuesPtr()[0]);
	// 				}
	// 			}
	// 		}
	//
	// 		break;
	// 	}
	// 	//*****State 4: Choice parameter*****//
	// 	case EHoudiniParameterType::IntChoice:
	// 	{
	// 		UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(MainParam);
	// 		if (ChoiceParameter && CurrentRampFloatPointsArray.Num() > 0)
	// 		{
	// 			// Set the last inserted float ramp point's interpolation parent parm, and value
	// 			UHoudiniParameterRampFloatPoint* LastAddedFloatRampPoint = CurrentRampFloatPointsArray.Last();
	//
	// 			LastAddedFloatRampPoint->InterpolationParentParm = ChoiceParameter;
	// 			LastAddedFloatRampPoint->SetInterpolation(UHoudiniParameter::GetHoudiniInterpMethodFromInt(ChoiceParameter->GetIntValue()));
	//
	// 			// Set the index of this point in the multi parm.
	// 			LastAddedFloatRampPoint->InstanceIndex = CurrentRampFloatPointsArray.Num() - 1;
	// 		}
	//
	//
	// 		//*****State 5: All ramp points have been parsed, finish!*****//
	// 		if (CurrentRampFloatPointsArray.Num() >= (int32)CurrentRampFloat->MultiParmInstanceCount)
	// 		{
	// 			CurrentRampFloatPointsArray.Sort([](const UHoudiniParameterRampFloatPoint& P1, const UHoudiniParameterRampFloatPoint& P2) {
	// 				return P1.Position < P2.Position;
	// 			});
	//
	// 			CurrentRampFloat->Points = CurrentRampFloatPointsArray;
	//
	// 			// Not caching, points are synced, update cached points
	// 			if (!CurrentRampFloat->bCaching)
	// 			{
	// 				const int32 NumPoints = CurrentRampFloat->Points.Num();
	// 				CurrentRampFloat->CachedPoints.SetNum(NumPoints);
	// 				for (int32 i = 0; i < NumPoints; ++i)
	// 				{
	// 					UHoudiniParameterRampFloatPoint* FromPoint = CurrentRampFloat->Points[i];
	// 					UHoudiniParameterRampFloatPoint* ToPoint = CurrentRampFloat->CachedPoints[i];
	// 					ToPoint = nullptr;
	// 					check(FromPoint)
	// 					if (!ToPoint)
	// 					{
	// 						ToPoint = FromPoint->DuplicateAndCopyState(CurrentRampFloat, RF_NoFlags, CurrentRampFloat->GetMaskedFlags(RF_PropagateToSubObjects));
	// 					}
	// 					else
	// 					{
	// 						ToPoint->CopyStateFrom(FromPoint, true);
	// 					}
	// 					CurrentRampFloat->CachedPoints[i] = ToPoint;
	// 				}
	// 			}
	//
	// 			CreateWidgetRampPoints(HouParameterCategory, CurrentRampRow, CurrentRampFloat, CurrentRampParameterList);
	//
	// 			CurrentRampFloat->SetDefaultValues();
	//
	// 			CurrentRampFloat = nullptr;
	// 			CurrentRampRow = nullptr;
	// 		}
	//
	// 		break;
	// 	}
	//
	// 	default:
	// 		break;
	// }

	//*****Float Ramp*****//
	if (MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp) 
	{
		UHoudiniParameterRampFloat* FloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
		if (FloatRampParameter) 
		{
			CurrentRampFloat = FloatRampParameter;
			FDetailWidgetRow *Row = CreateWidgetRampCurveEditor(HouParameterCategory, InParams);
			CreateWidgetRampPoints(HouParameterCategory, Row, FloatRampParameter, InParams);
			//FloatRampParameter->SetDefaultValues();
		}
	}
}

void
FHoudiniParameterDetails::CreateWidgetColorRamp(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
{
	if (InParams.Num() < 1)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// TODO: remove this once we have verified that updating the Points and CachedPoints arrays in
	// TODO: HoudiniParameterTranslator::BuildAllParameters() (via RampParam->UpdatePointsArray()) is sufficient.
	// //  Parsing a color ramp: 1->(2->3->4)*->5  //
	// switch (MainParam->GetParameterType())
	// {
	// 	//*****State 1: Color Ramp*****//
	// 	case EHoudiniParameterType::ColorRamp:
	// 	{
	// 		UHoudiniParameterRampColor* RampColor = Cast<UHoudiniParameterRampColor>(MainParam);
	// 		if (RampColor) 
	// 		{
	// 			CurrentRampColor = RampColor;
	// 			CurrentRampColorPointsArray.Empty();
	//
	// 			CurrentRampParameterList = InParams;
	//
	// 			FDetailWidgetRow *Row = CreateWidgetRampCurveEditor(HouParameterCategory, InParams);
	// 			CurrentRampRow = Row;
	// 		}
	//
	// 		break;
	// 	}
	// 	//*****State 2: Float parameter*****//
	// 	case EHoudiniParameterType::Float:
	// 	{
	// 		UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(MainParam);
	// 		if (FloatParameter) 
	// 		{
	// 			// Create a new color ramp point, and add its pointer to the current color points buffer array.
	// 			UHoudiniParameterRampColorPoint* NewRampColorPoint = nullptr;
	// 			int32 PointIndex = CurrentRampColorPointsArray.Num();
	//
	// 			if (CurrentRampColor->Points.IsValidIndex(PointIndex))
	// 			{
	// 				NewRampColorPoint = CurrentRampColor->Points[PointIndex];
	// 			}
	// 				
	// 			if (!NewRampColorPoint)
	// 			{
	// 				NewRampColorPoint = NewObject< UHoudiniParameterRampColorPoint >(CurrentRampColor, FName(), CurrentRampColor->GetMaskedFlags(RF_PropagateToSubObjects));
	// 			}
	//
	// 			CurrentRampColorPointsArray.Add(NewRampColorPoint);
	//
	// 			if (FloatParameter->GetNumberOfValues() <= 0)
	// 				return;
	// 			// Set the color ramp point's position parent parm, and value
	// 			NewRampColorPoint->PositionParentParm = FloatParameter;
	// 			NewRampColorPoint->SetPosition(FloatParameter->GetValuesPtr()[0]);
	// 		}
	//
	// 		break;
	// 	}
	// 	//*****State 3: Color parameter*****//
	// 	case EHoudiniParameterType::Color:
	// 	{
	// 		UHoudiniParameterColor* ColorParameter = Cast<UHoudiniParameterColor>(MainParam);
	// 		if (ColorParameter && CurrentRampColorPointsArray.Num() > 0) 
	// 		{
	// 			// Set the last inserted color ramp point's color parent parm, and value
	// 			UHoudiniParameterRampColorPoint* LastAddedColorRampPoint = CurrentRampColorPointsArray.Last();
	// 			LastAddedColorRampPoint->ValueParentParm = ColorParameter;
	// 			LastAddedColorRampPoint->SetValue(ColorParameter->GetColorValue());
	// 		}
	//
	// 		break;
	// 	}
	// 	//*****State 4: Choice Parameter*****//
	// 	case EHoudiniParameterType::IntChoice: 
	// 	{
	// 		UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(MainParam);
	// 		if (ChoiceParameter) 
	// 		{
	// 			// Set the last inserted color ramp point's interpolation parent parm, and value
	// 			UHoudiniParameterRampColorPoint*& LastAddedColorRampPoint = CurrentRampColorPointsArray.Last();
	//
	// 			LastAddedColorRampPoint->InterpolationParentParm = ChoiceParameter;
	// 			LastAddedColorRampPoint->SetInterpolation(UHoudiniParameter::GetHoudiniInterpMethodFromInt(ChoiceParameter->GetIntValue()));
	//
	// 			// Set the index of this point in the multi parm.
	// 			LastAddedColorRampPoint->InstanceIndex = CurrentRampColorPointsArray.Num() - 1;
	// 		}
	//
	//
	// 		//*****State 5: All ramp points have been parsed, finish!*****//
	// 		if (CurrentRampColorPointsArray.Num() >= (int32)CurrentRampColor->MultiParmInstanceCount) 
	// 		{
	// 			CurrentRampColorPointsArray.Sort([](const UHoudiniParameterRampColorPoint& P1, const UHoudiniParameterRampColorPoint& P2) 
	// 			{
	// 				return P1.Position < P2.Position;
	// 			});
	//
	// 			CurrentRampColor->Points = CurrentRampColorPointsArray;
	//
	// 			// Not caching, points are synced, update cached points
	// 			
	// 			if (!CurrentRampColor->bCaching) 
	// 			{
	// 				const int32 NumPoints = CurrentRampColor->Points.Num();
	// 				CurrentRampColor->CachedPoints.SetNum(NumPoints);
	//
	// 				for (int32 i = 0; i < NumPoints; ++i)
	// 				{
	// 					UHoudiniParameterRampColorPoint* FromPoint = CurrentRampColor->Points[i];
	// 					UHoudiniParameterRampColorPoint* ToPoint = CurrentRampColor->CachedPoints[i];
	//
	// 					if (!ToPoint)
	// 					{
	// 						ToPoint = FromPoint->DuplicateAndCopyState(CurrentRampColor, RF_NoFlags, CurrentRampColor->GetMaskedFlags(RF_PropagateToSubObjects));
	// 					}
	// 					else
	// 					{
	// 						ToPoint->CopyStateFrom(FromPoint, true);
	// 					}
	// 					CurrentRampColor->CachedPoints[i] = ToPoint;
	// 				}
	// 			}
	// 			
	// 		
	// 			CreateWidgetRampPoints(HouParameterCategory, CurrentRampRow, CurrentRampColor, CurrentRampParameterList);
	//
	// 			CurrentRampColor->SetDefaultValues();
	//
	// 			CurrentRampColor = nullptr;
	// 			CurrentRampRow = nullptr;
	// 		}
	//
	// 		break;
	// 	}
	//
	// 	default:
	// 		break;
	// }

	//*****Color Ramp*****//
	if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		UHoudiniParameterRampColor* RampColor = Cast<UHoudiniParameterRampColor>(MainParam);
		if (RampColor) 
		{
			CurrentRampColor = RampColor;
			FDetailWidgetRow *Row = CreateWidgetRampCurveEditor(HouParameterCategory, InParams);
			CreateWidgetRampPoints(HouParameterCategory, Row, RampColor, InParams);
			//RampColor->SetDefaultValues();
		}
	}

}


FDetailWidgetRow*
FHoudiniParameterDetails::CreateWidgetRampCurveEditor(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
{
	if (InParams.Num() <= 0)
		return nullptr;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return nullptr;
	
	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);
	if (!Row)
		return nullptr;

	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

	// Create the standard parameter name widget with an added autoupdate checkbox.
	CreateNameWidgetWithAutoUpdate(Row, InParams, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);	
	if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		UHoudiniParameterRampColor *RampColorParam = Cast<UHoudiniParameterRampColor>(MainParam);
		if (!RampColorParam)
			return nullptr;
		
		TSharedPtr<SHoudiniColorRampCurveEditor> ColorGradientEditor;
		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SBorder)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(ColorGradientEditor, SHoudiniColorRampCurveEditor)
				.ViewMinInput(0.0f)
				.ViewMaxInput(1.0f)
			]
		];

		if (!ColorGradientEditor.IsValid())
			return nullptr;

		// Avoid showing tooltips inside of the curve editor
		ColorGradientEditor->EnableToolTipForceField(true);

		CurrentRampParameterColorCurve = NewObject<UHoudiniColorRampCurve>(
				GetTransientPackage(), UHoudiniColorRampCurve::StaticClass(), NAME_None, RF_Transactional | RF_Public);

		if (!CurrentRampParameterColorCurve)
			return nullptr;

		CreatedColorRampCurves.Add(CurrentRampParameterColorCurve);

		// Add the ramp curve to root to avoid garabage collected.
		CurrentRampParameterColorCurve->AddToRoot();

		TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> ColorRampParameters;
		CastParameters<UHoudiniParameterRampColor>(InParams, ColorRampParameters);

		for (auto NextColorRamp : ColorRampParameters)
		{
			CurrentRampParameterColorCurve->ColorRampParameters.Add(NextColorRamp);
		}
		ColorGradientEditor->HoudiniColorRampCurve = CurrentRampParameterColorCurve;

		// Clear default curve points
		for (int Idx = 0; Idx < 4; ++Idx)
		{
			FRichCurve& RichCurve = (CurrentRampParameterColorCurve->FloatCurves)[Idx];
			if (RichCurve.GetNumKeys() > 0)
				RichCurve.Keys.Empty();
		}
		ColorGradientEditor->SetCurveOwner(CurrentRampParameterColorCurve);
		CreatedColorGradientEditors.Add(ColorGradientEditor);
	}
	else if(MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
	{
		UHoudiniParameterRampFloat *RampFloatParam = Cast<UHoudiniParameterRampFloat>(MainParam);
		if (!RampFloatParam)
			return nullptr;

		TSharedPtr<SHoudiniFloatRampCurveEditor> FloatCurveEditor;
		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SBorder)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(FloatCurveEditor, SHoudiniFloatRampCurveEditor)
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
				.XAxisName(FString("X"))
				.YAxisName(FString("Y"))
				.ShowCurveSelector(false)

			]
		];

		if (!FloatCurveEditor.IsValid())
			return nullptr;

		// Avoid showing tooltips inside of the curve editor
		FloatCurveEditor->EnableToolTipForceField(true);

		CurrentRampParameterFloatCurve = NewObject<UHoudiniFloatRampCurve>(
				GetTransientPackage(), UHoudiniFloatRampCurve::StaticClass(), NAME_None, RF_Transactional | RF_Public);

		if (!CurrentRampParameterFloatCurve)
			return nullptr;

		CreatedFloatRampCurves.Add(CurrentRampParameterFloatCurve);

		// Add the ramp curve to root to avoid garbage collected
		CurrentRampParameterFloatCurve->AddToRoot();

		TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameters;
		CastParameters<UHoudiniParameterRampFloat>(InParams, FloatRampParameters);		
		for (auto NextFloatRamp : FloatRampParameters)
		{
			CurrentRampParameterFloatCurve->FloatRampParameters.Add(NextFloatRamp);
		}
		FloatCurveEditor->HoudiniFloatRampCurve = CurrentRampParameterFloatCurve;

		FloatCurveEditor->SetCurveOwner(CurrentRampParameterFloatCurve, true);
		CreatedFloatCurveEditors.Add(FloatCurveEditor);
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.MaxDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	return Row;
}


void 
FHoudiniParameterDetails::CreateWidgetRampPoints(IDetailCategoryBuilder& CategoryBuilder, FDetailWidgetRow* Row, UHoudiniParameter* InParameter, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams) 
{
	if (!Row || !InParameter)
		return;

	if (InParams.Num() < 1)
		return;
	
	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	TWeakObjectPtr<UHoudiniParameterRampFloat> MainFloatRampParameter; 
	TWeakObjectPtr<UHoudiniParameterRampColor> MainColorRampParameter;

	TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameterList;
	TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> ColorRampParameterList;
	if (MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
	{
		MainFloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);

		if (!IsValidWeakPointer(MainFloatRampParameter))
			return;

		if (!CastParameters<UHoudiniParameterRampFloat>(InParams, FloatRampParameterList))
			return;
	}
	else if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		MainColorRampParameter = Cast<UHoudiniParameterRampColor>(MainParam);

		if (!IsValidWeakPointer(MainColorRampParameter))
			return;

		if (!CastParameters<UHoudiniParameterRampColor>(InParams, ColorRampParameterList))
			return;
	}
	else 
	{
		return;
	}

	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	// Lambda for computing the float point to be inserted
	auto GetInsertFloatPointLambda = [MainFloatRampParameter](
		const int32& InsertAtIndex, 
		float& OutPosition,
		float& OutValue, 
		EHoudiniRampInterpolationType& OutInterpType) mutable
	{
		if (!IsValidWeakPointer(MainFloatRampParameter))
			return;

		float PrevPosition = 0.0f;
		float NextPosition = 1.0f;
		
		TArray<UHoudiniParameterRampFloatPoint*> &CurrentPoints = MainFloatRampParameter->Points;
		TArray<UHoudiniParameterRampFloatPoint*> &CachedPoints = MainFloatRampParameter->CachedPoints;

		const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();
		int32 NumPoints = 0;
		if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled)
		{
			NumPoints = CurrentPoints.Num();
		}
		else
		{
			MainFloatRampParameter->SetCaching(true);
			NumPoints = CachedPoints.Num();
		}

		if (InsertAtIndex >= NumPoints)
		{
			// Insert at the end
			if (NumPoints > 0)
			{
				UHoudiniParameterRampFloatPoint* PrevPoint = nullptr;
				if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled)
					PrevPoint = CurrentPoints.Last();
				else
					PrevPoint = CachedPoints.Last();

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}
			}
		}
		else if (InsertAtIndex <= 0)
		{
			// Insert at the beginning
			if (NumPoints > 0)
			{
				UHoudiniParameterRampFloatPoint* NextPoint = nullptr;
				if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled)
					NextPoint = CurrentPoints[0];
				else
					NextPoint = CachedPoints[0];

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
					OutInterpType = NextPoint->GetInterpolation();
				}
			}
		}
		else
		{
			// Insert in the middle
			if (NumPoints > 1)
			{
				UHoudiniParameterRampFloatPoint* PrevPoint = nullptr;
				UHoudiniParameterRampFloatPoint* NextPoint = nullptr;

				if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled) 
				{
					PrevPoint = CurrentPoints[InsertAtIndex - 1];
					NextPoint = CurrentPoints[InsertAtIndex];
				}
				else 
				{
					PrevPoint = CachedPoints[InsertAtIndex - 1];
					NextPoint = CachedPoints[InsertAtIndex];
				}

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
				}

				if (PrevPoint && NextPoint)
				{
					OutValue = (PrevPoint->GetValue() + NextPoint->GetValue()) / 2.0;
				}
			}
		}

		OutPosition = (PrevPosition + NextPosition) / 2.0f;
	};


	// Lambda for computing the color point to be inserted
	auto GetInsertColorPointLambda = [MainColorRampParameter](
		const int32& InsertAtIndex,
		float& OutPosition,
		FLinearColor& OutColor,
		EHoudiniRampInterpolationType& OutInterpType) mutable
	{
		if (!IsValidWeakPointer(MainColorRampParameter))
			return;

		float PrevPosition = 0.0f;
		float NextPosition = 1.0f;

		TArray<UHoudiniParameterRampColorPoint*> &CurrentPoints = MainColorRampParameter->Points;
		TArray<UHoudiniParameterRampColorPoint*> &CachedPoints = MainColorRampParameter->CachedPoints;

		const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();
		int32 NumPoints = 0;
		if (MainColorRampParameter->IsAutoUpdate() && bCookingEnabled)
			NumPoints = CurrentPoints.Num();
		else
			NumPoints = CachedPoints.Num();

		if (InsertAtIndex >= NumPoints)
		{
			// Insert at the end
			if (NumPoints > 0)
			{
				UHoudiniParameterRampColorPoint* PrevPoint = nullptr;

				if (MainColorRampParameter->IsAutoUpdate() && bCookingEnabled)
					PrevPoint = CurrentPoints.Last();
				else
					PrevPoint = CachedPoints.Last();

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}
			}
		}
		else if (InsertAtIndex <= 0)
		{
			// Insert at the beginning
			if (NumPoints > 0)
			{
				UHoudiniParameterRampColorPoint* NextPoint = nullptr;

				if (MainColorRampParameter->IsAutoUpdate() && bCookingEnabled)
					NextPoint = CurrentPoints[0];
				else
					NextPoint = CachedPoints[0];

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
					OutInterpType = NextPoint->GetInterpolation();
				}
			}
		}
		else
		{
			// Insert in the middle
			if (NumPoints > 1)
			{
				UHoudiniParameterRampColorPoint* PrevPoint = nullptr;
				UHoudiniParameterRampColorPoint* NextPoint = nullptr;

				if (MainColorRampParameter->IsAutoUpdate() && bCookingEnabled) 
				{
					PrevPoint = CurrentPoints[InsertAtIndex - 1];
					NextPoint = CurrentPoints[InsertAtIndex];
				}
				else 
				{
					PrevPoint = CachedPoints[InsertAtIndex - 1];
					NextPoint = CachedPoints[InsertAtIndex];
				}

				if (PrevPoint) 
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
				}

				if (PrevPoint && NextPoint) 
				{
					OutColor = (PrevPoint->GetValue() + NextPoint->GetValue()) / 2.0;
				}
			}
		}

		OutPosition = (PrevPosition + NextPosition) / 2.0f;
	};
	
	int32 RowIndex = 0;
	auto InsertRampPoint_Lambda = [GetInsertColorPointLambda, GetInsertFloatPointLambda, &CategoryBuilder, bCookingEnabled](
		const TWeakObjectPtr<UHoudiniParameterRampFloat>& MainRampFloat, 
		const TWeakObjectPtr<UHoudiniParameterRampColor>& MainRampColor, 
		const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> &RampFloatList,
		const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> &RampColorList,
		const int32& Index) mutable 
	{
		if (MainRampFloat.IsValid())
		{
			float InsertPosition = 0.0f;
			float InsertValue = 1.0f;
			EHoudiniRampInterpolationType InsertInterp = EHoudiniRampInterpolationType::LINEAR;

			GetInsertFloatPointLambda(Index, InsertPosition, InsertValue, InsertInterp);

			FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(RampFloatList);

			for (auto & NextFloatRamp : RampFloatList)
			{
				if (!IsValidWeakPointer(NextFloatRamp))
					continue;
				
				if (NextFloatRamp->IsAutoUpdate() && bCookingEnabled)
				{
					CreateFloatRampParameterInsertEvent(
						NextFloatRamp.Get(), InsertPosition, InsertValue, InsertInterp);

					NextFloatRamp->MarkChanged(true);
				}
				else
				{
					UHoudiniParameterRampFloatPoint* NewCachedPoint = NewObject<UHoudiniParameterRampFloatPoint>
						(NextFloatRamp.Get(), UHoudiniParameterRampFloatPoint::StaticClass());
					NewCachedPoint->Position = InsertPosition;
					NewCachedPoint->Value = InsertValue;
					NewCachedPoint->Interpolation = InsertInterp;

					NextFloatRamp->CachedPoints.Add(NewCachedPoint);
					NextFloatRamp->bCaching = true;
					if (!bCookingEnabled)
					{
						// If cooking is not enabled, be sure to mark this parameter as changed
						// so that it triggers an update once cooking is enabled again.
						NextFloatRamp->MarkChanged(true);
					}
				}
			}

			if (!(MainRampFloat->IsAutoUpdate() && bCookingEnabled))
			{
				CategoryBuilder.GetParentLayout().ForceRefreshDetails();
			}

		}
		else if (MainRampColor.IsValid())
		{
			float InsertPosition = 0.0f;
			FLinearColor InsertColor = FLinearColor::Black;
			EHoudiniRampInterpolationType InsertInterp = EHoudiniRampInterpolationType::LINEAR;

			GetInsertColorPointLambda(Index, InsertPosition, InsertColor, InsertInterp);

			FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(RampColorList);
			for (auto& NextColorRamp : RampColorList)
			{
				if (!IsValidWeakPointer(NextColorRamp))
					continue;
				
				if (NextColorRamp->IsAutoUpdate() && bCookingEnabled)
				{
					CreateColorRampParameterInsertEvent(
						NextColorRamp.Get(), InsertPosition, InsertColor, InsertInterp);

					NextColorRamp->MarkChanged(true);
				}
				else
				{
					UHoudiniParameterRampColorPoint* NewCachedPoint = NewObject<UHoudiniParameterRampColorPoint>
						(NextColorRamp.Get(), UHoudiniParameterRampColorPoint::StaticClass());
					NewCachedPoint->Position = InsertPosition;
					NewCachedPoint->Value = InsertColor;
					NewCachedPoint->Interpolation = InsertInterp;

					NextColorRamp->CachedPoints.Add(NewCachedPoint);
					NextColorRamp->bCaching = true;
					if (!bCookingEnabled)
						NextColorRamp->MarkChanged(true);
				}
			}

			if (!(MainRampColor->IsAutoUpdate() && bCookingEnabled))
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampColor.Get(), true);
			}
		}
	};

	auto DeleteRampPoint_Lambda = [bCookingEnabled](
		const TWeakObjectPtr<UHoudiniParameterRampFloat>& MainRampFloat,
		const TWeakObjectPtr<UHoudiniParameterRampColor>& MainRampColor, 
		const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> &FloatRampList,
		const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> &ColorRampList,
		const int32& Index) mutable
	{
		if (MainRampFloat.IsValid())
		{
			FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampList);

			for (auto& NextFloatRamp : FloatRampList)
			{
				if (!IsValidWeakPointer(NextFloatRamp))
					continue;
				
				if (NextFloatRamp->IsAutoUpdate() && bCookingEnabled)
				{
					if (NextFloatRamp->Points.Num() == 0)
						return;

					UHoudiniParameterRampFloatPoint* PointToDelete = nullptr;

					if (Index == -1)
						PointToDelete = NextFloatRamp->Points.Last();
					else if (NextFloatRamp->Points.IsValidIndex(Index))
						PointToDelete = NextFloatRamp->Points[Index];
					
					if (!PointToDelete)
						return;

					const int32 & InstanceIndexToDelete = PointToDelete->InstanceIndex;

					CreateFloatRampParameterDeleteEvent(NextFloatRamp.Get(), InstanceIndexToDelete);
					NextFloatRamp->MarkChanged(true);
				}
				else
				{
					if (NextFloatRamp->CachedPoints.Num() == 0)
						return;

					if (Index == -1) 
						NextFloatRamp->CachedPoints.Pop();
					else if (NextFloatRamp->CachedPoints.IsValidIndex(Index))
						NextFloatRamp->CachedPoints.RemoveAt(Index);
					else
						return;

					NextFloatRamp->bCaching = true;
					if (!bCookingEnabled)
						NextFloatRamp->MarkChanged(true);
				}
			}

			if (!(MainRampFloat->IsAutoUpdate() && bCookingEnabled))
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampFloat.Get(), true);
			}
		}
		else
		{
			FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(ColorRampList);

			for (auto& NextColorRamp : ColorRampList)
			{
				if (!IsValidWeakPointer(NextColorRamp))
					continue;
				
				if (NextColorRamp->IsAutoUpdate() && bCookingEnabled)
				{
					if (NextColorRamp->Points.Num() == 0)
						return;

					UHoudiniParameterRampColorPoint* PointToRemove = nullptr;

					if (Index == -1)
						PointToRemove = NextColorRamp->Points.Last();
					else if (NextColorRamp->Points.IsValidIndex(Index))
						PointToRemove = NextColorRamp->Points[Index];

					if (!PointToRemove)
						return;

					const int32 & InstanceIndexToDelete = PointToRemove->InstanceIndex;

					CreateColorRampParameterDeleteEvent(NextColorRamp.Get(), InstanceIndexToDelete);

					NextColorRamp->MarkChanged(true);
				}
				else
				{
					if (NextColorRamp->CachedPoints.Num() == 0)
						return;

					if (Index == -1)
						NextColorRamp->CachedPoints.Pop();
					else if (NextColorRamp->CachedPoints.IsValidIndex(Index))
						NextColorRamp->CachedPoints.RemoveAt(Index);
					else
						return;

					NextColorRamp->bCaching = true;
					if (!bCookingEnabled)
						NextColorRamp->MarkChanged(true);
				}
			}

			if (!(MainRampColor->IsAutoUpdate() && bCookingEnabled))
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampColor.Get(), true);
			}
		}
	};


	TSharedRef<SVerticalBox> VerticalBox = StaticCastSharedRef<SVerticalBox>(Row->ValueWidget.Widget);

	TSharedPtr<SUniformGridPanel> GridPanel;
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SAssignNew(GridPanel, SUniformGridPanel)	
	];

	//AllUniformGridPanels.Add(GridPanel.Get());

	GridPanel->SetSlotPadding(FMargin(2.f, 2.f, 5.f, 3.f));	
	GridPanel->AddSlot(0, RowIndex)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Position")))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))		
	];

	FString ValueString = TEXT("Value");
	if (!MainFloatRampParameter.IsValid())
		ValueString = TEXT("Color");

	GridPanel->AddSlot(1, RowIndex)
	[
		SNew(STextBlock)
			.Text(FText::FromString(ValueString))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	GridPanel->AddSlot(2, RowIndex)
	[
		SNew(STextBlock)
			.Text(FText::FromString(TEXT("Interp.")))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	
	GridPanel->AddSlot(3, RowIndex)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(3.f, 0.f)
		.MaxWidth(35.f)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(
				FSimpleDelegate::CreateLambda([InsertRampPoint_Lambda, MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, bCookingEnabled]() mutable
				{
					int32 InsertAtIndex = -1;
					if (MainFloatRampParameter.IsValid()) 
					{
						if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled)
							InsertAtIndex = MainFloatRampParameter->Points.Num();
						else
							InsertAtIndex = MainFloatRampParameter->CachedPoints.Num();
					}
					else if (MainColorRampParameter.IsValid()) 
					{
						if (MainColorRampParameter->IsAutoUpdate() && bCookingEnabled)
							InsertAtIndex = MainColorRampParameter->Points.Num();
						else
							InsertAtIndex = MainColorRampParameter->CachedPoints.Num();
					}

					InsertRampPoint_Lambda(MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, InsertAtIndex);
				}),
				LOCTEXT("AddRampPoint", "Add a ramp point to the end"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(3.f, 0.f)
		.MaxWidth(35.f)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeRemoveButton(
				FSimpleDelegate::CreateLambda([DeleteRampPoint_Lambda, MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList]() mutable
				{
					DeleteRampPoint_Lambda(
						MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, -1);
				}),
				LOCTEXT("DeleteRampPoint", "Delete the last ramp point"), true)
		]
		
	];	
	
	EUnit Unit = EUnit::Unspecified;
	TSharedPtr<INumericTypeInterface<float>> paramTypeInterface;
	paramTypeInterface = MakeShareable(new TNumericUnitTypeInterface<float>(Unit));

	int32 PointCount = 0;
	// Use Synced points on auto update mode
	// Use Cached points on manual update mode
	if (MainFloatRampParameter.IsValid())
	{
		if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled)
			PointCount = MainFloatRampParameter->Points.Num();
		else
			PointCount = MainFloatRampParameter->CachedPoints.Num();
	}

	if (MainColorRampParameter.IsValid())
	{
		if (MainColorRampParameter->IsAutoUpdate())
			PointCount = MainColorRampParameter->Points.Num();
		else
			PointCount = MainColorRampParameter->CachedPoints.Num();
	}

	// Lambda function for changing a ramp point
	auto OnPointChangeCommit = [bCookingEnabled](
		const TWeakObjectPtr<UHoudiniParameterRampFloat>& MainRampFloat, const TWeakObjectPtr<UHoudiniParameterRampColor>& MainRampColor, 
		const TWeakObjectPtr<UHoudiniParameterRampFloatPoint>& MainRampFloatPoint, const TWeakObjectPtr<UHoudiniParameterRampColorPoint>& MainRampColorPoint,
		const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> &RampFloatList, const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> &RampColorList, 
		const int32& Index, const FString& ChangedDataName, 
		const float& NewPosition, const float& NewFloat, 
		const FLinearColor& NewColor, 
		const EHoudiniRampInterpolationType& NewInterpType) mutable
	{
		if (MainRampFloat.IsValid() && MainRampFloatPoint.IsValid())
		{
			if (ChangedDataName == FString("position") && MainRampFloatPoint->GetPosition() == NewPosition)
				return;
			if (ChangedDataName == FString("value") && MainRampFloatPoint->GetValue() == NewFloat)
				return;
			if (ChangedDataName == FString("interp") && MainRampFloatPoint->GetInterpolation() == NewInterpType)
				return;

			FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(RampFloatList);
			for (auto NextFloatRamp : RampFloatList)
			{
				if (!IsValidWeakPointer(NextFloatRamp))
					continue;

				if (NextFloatRamp->IsAutoUpdate() && bCookingEnabled)
				{
					if (NextFloatRamp->Points.IsValidIndex(Index))
					{
						UHoudiniParameterRampFloatPoint* CurrentFloatRampPoint = NextFloatRamp->Points[Index];
						if (!CurrentFloatRampPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							if (!CurrentFloatRampPoint->PositionParentParm)
								continue;

							CurrentFloatRampPoint->SetPosition(NewPosition);
							CurrentFloatRampPoint->PositionParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("value")) 
						{
							if (!CurrentFloatRampPoint->PositionParentParm)
								continue;

							CurrentFloatRampPoint->SetValue(NewFloat);
							CurrentFloatRampPoint->ValueParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("interp"))
						{
							if (!CurrentFloatRampPoint->InterpolationParentParm)
								continue;

							CurrentFloatRampPoint->SetInterpolation(NewInterpType);
							CurrentFloatRampPoint->InterpolationParentParm->MarkChanged(true);
						}
					}
					else
					{
						int32 IdxInEventsArray = Index - NextFloatRamp->Points.Num();
						if (NextFloatRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
						{
							UHoudiniParameterRampModificationEvent* Event = NextFloatRamp->ModificationEvents[IdxInEventsArray];
							if (!Event)
								continue;

							if (ChangedDataName == FString("position")) 
							{
								Event->InsertPosition = NewPosition;
							}
							else if (ChangedDataName == FString("value"))
							{
								Event->InsertFloat = NewFloat;
							}
							else if (ChangedDataName == FString("interp")) 
							{
								Event->InsertInterpolation = NewInterpType;
							}
						}
					}
				}
				else
				{
					if (NextFloatRamp->CachedPoints.IsValidIndex(Index))
					{
						UHoudiniParameterRampFloatPoint* CachedPoint = NextFloatRamp->CachedPoints[Index];

						if (!CachedPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							CachedPoint->Position = NewPosition;
						}
						else if (ChangedDataName == FString("value"))
						{
							CachedPoint->Value = NewFloat;
						}
						else if (ChangedDataName == FString("interp"))
						{
							CachedPoint->Interpolation = NewInterpType;
						}
						
						NextFloatRamp->bCaching = true;
					}
				}

				if (!(MainRampFloat->IsAutoUpdate() && bCookingEnabled))
					FHoudiniEngineUtils::UpdateEditorProperties(MainRampFloat.Get(), true);
			}
		}
		else if (MainRampColor.IsValid() && MainRampColorPoint.IsValid())
		{
			if (ChangedDataName == FString("position") && MainRampColorPoint->GetPosition() == NewPosition)
				return;
			
			if (ChangedDataName == FString("value") && MainRampColorPoint->GetValue() == NewColor)
				return;
			
			if (ChangedDataName == FString("interp") && MainRampColorPoint->GetInterpolation() == NewInterpType)
				return;

			FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(RampColorList);
			for (auto NextColorRamp : RampColorList)
			{
				if (!IsValidWeakPointer(NextColorRamp))
					continue;

				if (NextColorRamp->IsAutoUpdate() && bCookingEnabled)
				{
					if (NextColorRamp->Points.IsValidIndex(Index))
					{
						UHoudiniParameterRampColorPoint* CurrentColorRampPoint = NextColorRamp->Points[Index];
						if (!CurrentColorRampPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							if (!CurrentColorRampPoint->PositionParentParm)
								continue;

							CurrentColorRampPoint->SetPosition(NewPosition);
							CurrentColorRampPoint->PositionParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("value"))
						{
							if (!CurrentColorRampPoint->PositionParentParm)
								continue;

							CurrentColorRampPoint->SetValue(NewColor);
							CurrentColorRampPoint->ValueParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("interp"))
						{
							if (!CurrentColorRampPoint->InterpolationParentParm)
								continue;

							CurrentColorRampPoint->SetInterpolation(NewInterpType);
							CurrentColorRampPoint->InterpolationParentParm->MarkChanged(true);
						}
					}
					else
					{
						int32 IdxInEventsArray = Index - NextColorRamp->Points.Num();
						if (NextColorRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
						{
							UHoudiniParameterRampModificationEvent* Event = NextColorRamp->ModificationEvents[IdxInEventsArray];
							if (!Event)
								continue;

							if (ChangedDataName == FString("position"))
							{
								Event->InsertPosition = NewPosition;
							}
							else if (ChangedDataName == FString("value"))
							{
								Event->InsertColor = NewColor;
							}
							else if (ChangedDataName == FString("interp"))
							{
								Event->InsertInterpolation = NewInterpType;
							}

						}
					}
				}
				else
				{
					if (NextColorRamp->CachedPoints.IsValidIndex(Index))
					{
						UHoudiniParameterRampColorPoint* CachedPoint = NextColorRamp->CachedPoints[Index];

						if (!CachedPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							CachedPoint->Position = NewPosition;
						}
						else if (ChangedDataName == FString("value"))
						{
							CachedPoint->Value = NewColor;
						}
						else if (ChangedDataName == FString("interp"))
						{
							CachedPoint->Interpolation = NewInterpType;
						}

						NextColorRamp->bCaching = true;
					}
				}

				if (!(MainRampColor->IsAutoUpdate() && bCookingEnabled))
					FHoudiniEngineUtils::UpdateEditorProperties(MainRampColor.Get(), true);
			}
		}
	};

	for (int32 Index = 0; Index < PointCount; ++Index) 
	{
		UHoudiniParameterRampFloatPoint* NextFloatRampPoint = nullptr;
		UHoudiniParameterRampColorPoint* NextColorRampPoint = nullptr;		
		
		if (MainFloatRampParameter.IsValid())
		{
			if (MainFloatRampParameter->IsAutoUpdate() && bCookingEnabled)
				NextFloatRampPoint = MainFloatRampParameter->Points[Index];
			else
				NextFloatRampPoint = MainFloatRampParameter->CachedPoints[Index];
		}
		if (MainColorRampParameter.IsValid())
		{
			if (MainColorRampParameter->IsAutoUpdate() && bCookingEnabled)
				NextColorRampPoint = MainColorRampParameter->Points[Index];
			else
				NextColorRampPoint = MainColorRampParameter->CachedPoints[Index];
		}

		if (!NextFloatRampPoint && !NextColorRampPoint)
			continue;
		
		RowIndex += 1;
		
		float CurPos = 0.f;
		if (NextFloatRampPoint)
			CurPos = NextFloatRampPoint->Position;
		else
			CurPos = NextColorRampPoint->Position;
		

		GridPanel->AddSlot(0, RowIndex)
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(true)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Value(CurPos)
			.OnValueChanged_Lambda([](float Val) {})
			.OnValueCommitted_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](float Val, ETextCommit::Type TextCommitType) mutable
			{
				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint,
					FloatRampParameterList, ColorRampParameterList,
					Index, FString("position"),
					Val, float(-1.0),
					FLinearColor(),
					EHoudiniRampInterpolationType::LINEAR);
			})
			.OnBeginSliderMovement_Lambda([]() {})
			.OnEndSliderMovement_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](const float Val) mutable
			{
				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint,
					FloatRampParameterList, ColorRampParameterList,
					Index, FString("position"),
					Val, float(-1.0),
					FLinearColor(),
					EHoudiniRampInterpolationType::LINEAR);
			})
			.SliderExponent(1.0f)
			.TypeInterface(paramTypeInterface)
		];

		if (NextFloatRampPoint)
		{
			GridPanel->AddSlot(1, RowIndex)
			[
				SNew(SNumericEntryBox< float >)
				.AllowSpin(true)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Value(NextFloatRampPoint->Value)
				.OnValueChanged_Lambda([](float Val){})
				.OnValueCommitted_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](float Val, ETextCommit::Type TextCommitType) mutable
				{
					OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
						NextFloatRampPoint, NextColorRampPoint,
						FloatRampParameterList, ColorRampParameterList,
						Index, FString("value"),
						float(-1.0), Val,
						FLinearColor(),
						EHoudiniRampInterpolationType::LINEAR);
				})
				.OnBeginSliderMovement_Lambda([]() {})
				.OnEndSliderMovement_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, 
					NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](const float Val) mutable
				{
					OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
						NextFloatRampPoint, NextColorRampPoint,
						FloatRampParameterList, ColorRampParameterList,
						Index, FString("value"),
						float(-1.0), Val,
						FLinearColor(),
						EHoudiniRampInterpolationType::LINEAR);
				})
				.SliderExponent(1.0f)
				.TypeInterface(paramTypeInterface)
			];
		}
		else if (NextColorRampPoint)
		{	
			auto OnColorChangeLambda = [OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, 
				NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](FLinearColor InColor) mutable
			{
				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
						NextFloatRampPoint, NextColorRampPoint,
						FloatRampParameterList, ColorRampParameterList,
						Index, FString("value"),
						float(-1.0), float(-1.0),
						InColor,
						EHoudiniRampInterpolationType::LINEAR);
			};

			// Add color picker UI.
			//TSharedPtr<SColorBlock> ColorBlock;
			GridPanel->AddSlot(1, RowIndex)
			[
				SNew(SColorBlock)
				.Color(NextColorRampPoint->Value)
				.OnMouseButtonDown( FPointerEventHandler::CreateLambda(
					[NextColorRampPoint, OnColorChangeLambda](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
					{
						if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
							return FReply::Unhandled();

						FColorPickerArgs PickerArgs;
						PickerArgs.bUseAlpha = true;
						PickerArgs.DisplayGamma = TAttribute< float >::Create(
							TAttribute< float >::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda(OnColorChangeLambda);
						FLinearColor InitColor = NextColorRampPoint->Value;
						PickerArgs.InitialColorOverride = InitColor;
						PickerArgs.bOnlyRefreshOnOk = true;
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					}))
			];
		}

		int32 CurChoice = 0;
		if (NextFloatRampPoint)
			CurChoice = (int)NextFloatRampPoint->Interpolation;
		else
			CurChoice = (int)NextColorRampPoint->Interpolation;

		TSharedPtr <SComboBox<TSharedPtr< FString >>> ComboBoxCurveMethod;
		GridPanel->AddSlot(2, RowIndex)
		[
			SAssignNew(ComboBoxCurveMethod, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniParameterRampInterpolationMethodLabels())
			.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniParameterRampInterpolationMethodLabels())[CurChoice])
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnSelectionChanged_Lambda(
				[OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter,
				NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, 
				ColorRampParameterList, Index](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType) mutable
			{
				EHoudiniRampInterpolationType NewInterpType = UHoudiniParameter::GetHoudiniInterpMethodFromString(*NewChoice.Get());

				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint,
					FloatRampParameterList, ColorRampParameterList,
					Index, FString("interp"),
					float(-1.0), float(-1.0),
					FLinearColor(),
					NewInterpType);
			})
			[
				SNew(STextBlock)
				.Text_Lambda([NextFloatRampPoint, NextColorRampPoint]()
				{
					EHoudiniRampInterpolationType CurInterpType = EHoudiniRampInterpolationType::InValid;
					if (NextFloatRampPoint)
						CurInterpType = NextFloatRampPoint->GetInterpolation();
					else
						CurInterpType = NextColorRampPoint->GetInterpolation();

					return FText::FromString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(CurInterpType));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
		
		GridPanel->AddSlot(3, RowIndex)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(3.f, 0.f)
			.MaxWidth(35.f)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateLambda(
					[InsertRampPoint_Lambda, MainFloatRampParameter, 
					MainColorRampParameter, FloatRampParameterList, 
					ColorRampParameterList, Index]() mutable
					{
						InsertRampPoint_Lambda(MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, Index);
					}),
					LOCTEXT("AddRampPoint", "Add a ramp point before this point"), true)
			]
			+ SHorizontalBox::Slot()
			.Padding(3.f, 0.f)
			.MaxWidth(35.f)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda(
					[DeleteRampPoint_Lambda, MainFloatRampParameter,
					MainColorRampParameter, FloatRampParameterList,
					ColorRampParameterList, Index]() mutable
					{
						DeleteRampPoint_Lambda(MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, Index);
					}),
					LOCTEXT("DeleteFloatRampPoint", "Delete this ramp point"), true)
			]
		];		
		
		if (MainFloatRampParameter.IsValid() && CurrentRampParameterFloatCurve)
		{
			ERichCurveInterpMode RichCurveInterpMode = UHoudiniParameter::EHoudiniRampInterpolationTypeToERichCurveInterpMode(NextFloatRampPoint->GetInterpolation());
			FRichCurve & RichCurve = CurrentRampParameterFloatCurve->FloatCurve;
			FKeyHandle const KeyHandle = RichCurve.AddKey(NextFloatRampPoint->GetPosition(), NextFloatRampPoint->GetValue());
			RichCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
		}

		if (MainColorRampParameter.IsValid() && CurrentRampParameterColorCurve)
		{
			ERichCurveInterpMode RichCurveInterpMode = UHoudiniParameter::EHoudiniRampInterpolationTypeToERichCurveInterpMode(NextColorRampPoint->GetInterpolation());
			for (int32 CurveIdx = 0; CurveIdx < 4; ++CurveIdx)
			{
				FRichCurve & RichCurve = CurrentRampParameterColorCurve->FloatCurves[CurveIdx];

				FKeyHandle const KeyHandle = RichCurve.AddKey(NextColorRampPoint->GetPosition(), NextColorRampPoint->GetValue().Component(CurveIdx));
				RichCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
			}
		}
	}	

	if (MainFloatRampParameter.IsValid())
		GridPanel->SetEnabled(!MainFloatRampParameter->IsDisabled());

	if (MainColorRampParameter.IsValid())
		GridPanel->SetEnabled(!MainColorRampParameter->IsDisabled());	
}

void 
FHoudiniParameterDetails::CreateWidgetFolderList(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterFolderList>> FolderListParams;
	if (!CastParameters<UHoudiniParameterFolderList>(InParams, FolderListParams))
		return;

	if (FolderListParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFolderList>& MainParam = FolderListParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Add this folder list to the folder map
	AllFoldersAndFolderLists.Add(MainParam->GetParmId(), MainParam);

	MainParam->GetTabs().Empty();

	// A folder list will be followed by all its child folders, 
	// so set the CurrentFolderListSize to the tuple size, we'll process such many folder parameters right after
	CurrentFolderListSize = MainParam->GetTupleSize(); 

	if (MainParam->IsDirectChildOfMultiParm())
		MultiParmInstanceIndices.Add(MainParam->GetParmId(), -1);

	if (CurrentFolderListSize <= 0)		// There should not be empty folder list, this will not happen normally
		return;

	// The following folders belong to current folder list
	CurrentFolderList = MainParam.Get();

	// If the tab is either a tabs or radio button and the parameter is visible 
	if (MainParam->IsTabMenu() && MainParam->ShouldDisplay())
	{
		// Set the current tabs to be not shown by default now. CreateWidgetTab will decide if the tabs is shown.
		CurrentFolderList->SetTabsShown(false);

		// Create a row to hold tab buttons if the folder list is a tabs or radio button 

		// CreateNestedRow does not actually create a row for tabs, it is responsible to prune the folder stack.
		// ( CreateWidgetTab will be responsible to create a row according to the visibility of its outer level folders )
		FDetailWidgetRow* TabRow = CreateNestedRow(HouParameterCategory, InParams, false);
		
	}

	// When see a folder list, go depth first search at this step.
	// Push an empty queue to the stack.
	FolderStack.Add(TArray<UHoudiniParameterFolder*>());
}


void
FHoudiniParameterDetails::CreateWidgetFolder(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterFolder>> FolderParams;
	if (!CastParameters<UHoudiniParameterFolder>(InParams, FolderParams))
		return;

	if (FolderParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFolder>& MainParam = FolderParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	if (!IsValid(CurrentFolderList))	// This should not happen
		return;

	// If a folder is invisible, its children won't be listed by HAPI. 
	// So just reduce FolderListSize by 1, reduce the child counter of its parent folder by 1 if necessary, 
	// and prune the stack in such case.
	if (!MainParam->IsVisible())
	{
		CurrentFolderListSize -= 1;

		if (CurrentFolderListSize == 0)
		{						
			if (FolderStack.Num() > 1)
			{
				TArray<UHoudiniParameterFolder*> &ParentFolderQueue = FolderStack[FolderStack.Num() - 2];
				if (ParentFolderQueue.Num() > 0 && IsValid(ParentFolderQueue[0]))
					ParentFolderQueue[0]->GetChildCounter() -= 1;
			}

			PruneStack();
		}

		return;
	}

	// We expect 'TupleSize' children param of this folder after finish processing all the child folders of cur folderlist
	MainParam->ResetChildCounter();

	// Add this folder to the folder map
	AllFoldersAndFolderLists.Add(MainParam->GetParmId(), MainParam);

	// Set the parent param to current folderList. 
	// it was parent multiparm's id if this folder is a child of a multiparms. 
	// This will cause problem if the folder is inside of a multiparm
	MainParam->SetParentParmId(CurrentFolderList->GetParmId());
	

	// Case 1: The folder is a direct child of a multiparm.
	if (MainParam->IsDirectChildOfMultiParm())
	{
		if (FolderStack.Num() <= 0)      // This should not happen
			return;

		// Get its parent multiparm first
		TWeakObjectPtr<UHoudiniParameterMultiParm> ParentMultiParm;
		{
			UHoudiniParameterFolderList * ParentFolderList = nullptr;
			if (!AllFoldersAndFolderLists.Contains(MainParam->GetParentParmId()))
				return; 

			ParentFolderList = Cast<UHoudiniParameterFolderList>(AllFoldersAndFolderLists[MainParam->GetParentParmId()]);
			
			if (!ParentFolderList)
				return;

			if (AllMultiParms.Contains(ParentFolderList->GetParentParmId()))
				ParentMultiParm = AllMultiParms[ParentFolderList->GetParentParmId()];

			if (!ParentMultiParm.IsValid())	// This should not happen
				return;
		}
	
		bool bShown = ParentMultiParm->IsShown();

		// Case 1-1: The folder is NOT tabs
		if (!MainParam->IsTab())
		{
			bShown = MainParam->IsExpanded() && bShown;

			// If the parent multiparm is shown.
			if (ParentMultiParm->IsShown())
			{
				FDetailWidgetRow* FolderHeaderRow = CreateNestedRow(HouParameterCategory, InParams, false);
				CreateFolderHeaderUI(FolderHeaderRow, InParams);
			}
		}
		// Case 1-2: The folder IS tabs.
		else 
		{
			CreateWidgetTab(HouParameterCategory, MainParam, ParentMultiParm->IsShown());
		}

		// Push the folder to the queue if it is not a tab folder
		// This step is handled by CreateWidgetTab() if it is tabs
		if ((!MainParam->IsTab() || !ParentMultiParm->IsShown()) && MainParam->GetTupleSize() > 0)
		{
			TArray<UHoudiniParameterFolder*> & MyQueue = FolderStack.Last();
			MainParam->SetIsContentShown(bShown);
			MyQueue.Add(MainParam.Get());
		}
	}

	// Case 2: The folder is NOT a direct child of a multiparm.
	else 
	{
		// Case 2-1: The folder is in another folder.
		if (FolderStack.Num() > 1 && CurrentFolderListSize > 0)
		{
			TArray<UHoudiniParameterFolder*> & MyFolderQueue = FolderStack.Last();
			TArray<UHoudiniParameterFolder*> & ParentFolderQueue = FolderStack[FolderStack.Num() - 2];

			if (ParentFolderQueue.Num() <= 0)	//This should happen
				return;

			// Peek the folder queue of the last layer to get its parent folder parm.
			bool ParentFolderVisible = ParentFolderQueue[0]->IsContentShown();

			// If this folder is expanded (selected if is tabs)
			bool bExpanded = ParentFolderVisible;

			// Case 2-1-1: The folder is NOT in a tab menu.
			if (!MainParam->IsTab()) 
			{
				bExpanded &= MainParam->IsExpanded();
			
				// The parent folder is visible.
				if (ParentFolderVisible)
				{
					// Add the folder header UI.
					FDetailWidgetRow* FolderHeaderRow = CreateNestedRow(HouParameterCategory, InParams, false);
					CreateFolderHeaderUI(FolderHeaderRow, InParams);
				}

				MainParam->SetIsContentShown(bExpanded);
				MyFolderQueue.Add(MainParam.Get());
			}
			// Case 2-1-2: The folder IS in a tab menu.
			else 
			{
				bExpanded &= MainParam->IsChosen();

				CreateWidgetTab(HouParameterCategory, MainParam, ParentFolderVisible);
			}
		}
		// Case 2-2: The folder is in the root.
		else 
		{
			bool bExpanded = true;

			// Case 2-2-1: The folder is NOT under a tab menu.
			if (!MainParam->IsTab()) 
			{
				if (FolderStack.Num() <= 0)		// This will not happen
					return;

				// Create Folder header under root.
				FDetailWidgetRow* FolderRow = CreateNestedRow(HouParameterCategory, InParams, false);
				CreateFolderHeaderUI(FolderRow, InParams);

				if (FolderStack.Num() == 0) // This should not happen
					return;

				TArray<UHoudiniParameterFolder*>& MyFolderQueue = FolderStack[0];
				bExpanded &= MainParam->IsExpanded();
				MainParam->SetIsContentShown(bExpanded);
				MyFolderQueue.Add(MainParam.Get());
			}
			// Case 2-2-2: The folder IS under a tab menu.
			else
			{
				// Tabs in root is always visible
				CreateWidgetTab(HouParameterCategory, MainParam, true); 
			}
		}	
	}
	
	CurrentFolderListSize -= 1;

	// Prune the stack if finished parsing current folderlist
	if (CurrentFolderListSize == 0)
	{		
		if (FolderStack.Num() > 1 && !MainParam->IsDirectChildOfMultiParm())
		{
			TArray<UHoudiniParameterFolder*> & ParentFolderQueue = FolderStack[FolderStack.Num() - 2];
			if (ParentFolderQueue.Num() > 0 && IsValid(ParentFolderQueue[0]))
				ParentFolderQueue[0]->GetChildCounter() -= 1;
		}

		PruneStack();

		CurrentFolderList = nullptr;
	}
}

void
FHoudiniParameterDetails::CreateFolderHeaderUI(FDetailWidgetRow* HeaderRow, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
{
	if (!HeaderRow)	// The folder is invisible.
		return;

	TArray<TWeakObjectPtr<UHoudiniParameterFolder>> FolderParams;
	if (!CastParameters<UHoudiniParameterFolder>(InParams, FolderParams))
		return;

	if (FolderParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFolder>& MainParam = FolderParams[0];

	if (!IsValidWeakPointer(MainParam))
		return;

	TSharedPtr<SVerticalBox> VerticalBox;

	FString LabelStr = MainParam->GetParameterLabel();

	TSharedPtr<SCustomizedBox> HorizontalBox;
	TSharedPtr<SButton> ExpanderArrow;
	TSharedPtr<SImage> ExpanderImage;

	HeaderRow->NameWidget.Widget = SAssignNew(HorizontalBox, SCustomizedBox);

	HorizontalBox->AddIndentation(MainParam, AllMultiParms, AllFoldersAndFolderLists);
	HorizontalBox->DividerLinePositions = DividerLinePositions;
	HorizontalBox->SetHoudiniParameter(InParams);

	if (MainParam->IsDirectChildOfMultiParm() && MainParam->GetChildIndex() == 1) 
	{
		int32 CurrentMultiParmInstanceIndex = 0;
		if (MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
		{
			MultiParmInstanceIndices[MainParam->GetParentParmId()] += 1;
			CurrentMultiParmInstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];
			LabelStr = LabelStr + TEXT(" (")  + FString::FromInt(CurrentMultiParmInstanceIndex) + TEXT(")");
		}

		CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
	}

	HorizontalBox->AddSlot().Padding(1.0f).VAlign(VAlign_Center).AutoWidth()
	[
		SAssignNew(ExpanderArrow, SButton)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.ClickMethod(EButtonClickMethod::MouseDown)
		.Visibility(EVisibility::Visible)
		.OnClicked_Lambda([=]()
		{
			if (!IsValidWeakPointer(MainParam))
				return FReply::Handled();
			
			MainParam->ExpandButtonClicked();
			
			FHoudiniEngineUtils::UpdateEditorProperties(MainParam.Get(), true);

			return FReply::Handled();
		})
		[
			SAssignNew(ExpanderImage, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];

	
	FText LabelText = FText::FromString(LabelStr);

	HorizontalBox->AddSlot().Padding(1.0f).VAlign(VAlign_Center).AutoWidth()
	[
		SNew(STextBlock)
		.Text(LabelText)
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	TWeakPtr<SButton> WeakExpanderArrow(ExpanderArrow);
	ExpanderImage->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			TAttribute<const FSlateBrush*>::FGetter::CreateLambda([MainParam, WeakExpanderArrow]() 
			{
				FName ResourceName;
				TSharedPtr<SButton> ExpanderArrowPtr = WeakExpanderArrow.Pin();

				bool bIsExpanded = false;
				if (IsValidWeakPointer(MainParam))
				{
					bIsExpanded = MainParam->IsExpanded();
				}
				
				if (bIsExpanded)
				{
					ResourceName = ExpanderArrowPtr.IsValid() && ExpanderArrowPtr->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
				}
				else
				{
					ResourceName = ExpanderArrowPtr.IsValid() && ExpanderArrowPtr->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
				}

				return FEditorStyle::GetBrush(ResourceName);
			}
		)
	));

	if(MainParam->GetFolderType() == EHoudiniFolderParameterType::Simple)
		ExpanderArrow->SetEnabled(false);

}

void FHoudiniParameterDetails::CreateWidgetTab(IDetailCategoryBuilder & HouParameterCategory, const TWeakObjectPtr<UHoudiniParameterFolder>& InFolder, const bool& bIsShown)
{
	if (!InFolder.IsValid() || !CurrentFolderList)
		return;

	if (FolderStack.Num() <= 0)	// error state
		return;

	UHoudiniParameterFolder* const Folder = InFolder.Get();
	TArray<UHoudiniParameterFolder*> & FolderQueue = FolderStack.Last();

	// Cache all tabs of current tab folder list.
	CurrentFolderList->AddTabFolder(Folder);

	// If the tabs is not shown, just push the folder param into the queue.
	if (!bIsShown)
	{
		InFolder->SetIsContentShown(bIsShown);
		FolderQueue.Add(Folder);
		return;
	}
	
	// tabs currently being processed
	CurrentTabs.Add(Folder);

	if (CurrentFolderListSize > 1)
		return;

	// The tabs belong to current folder list
	UHoudiniParameterFolderList* CurrentTabMenuFolderList = CurrentFolderList;

	// Create a row (UI) for current tabs
	TSharedPtr<SCustomizedBox> HorizontalBox;
	FDetailWidgetRow &Row = HouParameterCategory.AddCustomRow(FText::GetEmpty())
	[
		SAssignNew(HorizontalBox, SCustomizedBox)
	]; 

	// Put current tab folder list param into an array
	TArray<TWeakObjectPtr<UHoudiniParameter>> CurrentTabMenuFolderListArr;
	CurrentTabMenuFolderListArr.Add(CurrentTabMenuFolderList);

	HorizontalBox->SetHoudiniParameter(CurrentTabMenuFolderListArr);
	DividerLinePositions.Add(HorizontalBox->AddIndentation(InFolder, AllMultiParms, AllFoldersAndFolderLists));
	HorizontalBox->DividerLinePositions = DividerLinePositions;

	float DesiredHeight = 0.0f;
	float DesiredWidth = 0.0f;

	// Process all tabs of current folder list at once when done.

	for (auto & CurTab : CurrentTabs)
	{
		if (!IsValid(CurTab))
			continue;

		CurTab->SetIsContentShown(CurTab->IsChosen());
		FolderQueue.Add(CurTab);

		auto OnTabClickedLambda = [CurrentTabMenuFolderList, CurTab, &HouParameterCategory]() 
		{
			if (CurrentTabMenuFolderList)
			{
				if (!CurrentTabMenuFolderList->bIsTabMenu || CurrentTabMenuFolderList->TabFolders.Num() <= 0)
					return FReply::Handled();

				if (CurTab->IsChosen())
					return FReply::Handled();

				CurTab->SetChosen(true);

				for (UHoudiniParameterFolder* NextFolder : CurrentTabMenuFolderList->TabFolders)
				{
					if (CurTab->GetParmId() != NextFolder->GetParmId() && NextFolder->IsChosen())
						NextFolder->SetChosen(false);
				}

				HouParameterCategory.GetParentLayout().ForceRefreshDetails();
			}

			return FReply::Handled();
		};

		FString FolderLabelString = TEXT("   ") + CurTab->GetParameterLabel();
		if (CurTab->GetFolderType() == EHoudiniFolderParameterType::Radio)
			FolderLabelString = TEXT("      ") + FolderLabelString;

		bool bChosen = CurTab->IsTab() && CurTab->IsChosen();

		TSharedPtr<SCustomizedButton> CurCustomizedButton;

		HorizontalBox->AddSlot().VAlign(VAlign_Bottom)
		.AutoWidth()
		.Padding(0.f)
		.HAlign(HAlign_Left)
		[
			SAssignNew(CurCustomizedButton, SCustomizedButton)
			.OnClicked_Lambda(OnTabClickedLambda)
			.Content()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FolderLabelString))
			]
		];

		CurCustomizedButton->bChosen = bChosen;
		CurCustomizedButton->bIsRadioButton = CurTab->GetFolderType() == EHoudiniFolderParameterType::Radio;

		DesiredHeight = CurCustomizedButton->GetDesiredSize().Y;
		DesiredWidth += CurCustomizedButton->GetDesiredSize().X;
	}

	HorizontalBox->bIsTabFolderListRow = true;

	Row.WholeRowWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	// Set the current tabs to be shown, since slate widgets have been created
	CurrentTabMenuFolderList->SetTabsShown(true);

	// Clear the temporary tabs
	CurrentTabs.Empty();
}

void
FHoudiniParameterDetails::CreateWidgetMultiParm(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams) 
{
	TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>> MultiParmParams;
	if (!CastParameters<UHoudiniParameterMultiParm>(InParams, MultiParmParams))
		return;

	if (MultiParmParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterMultiParm>& MainParam = MultiParmParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	// Add current multiparm parameter to AllmultiParms map
	AllMultiParms.Add(MainParam->GetParmId(), MainParam);

	// Create a new detail row
	FDetailWidgetRow * Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row) 
	{
		MainParam->SetIsShown(false);
		return;
	}

	MainParam->SetIsShown(true);

	MultiParmInstanceIndices.Add(MainParam->GetParmId(), -1);

	CreateNameWidget(Row, InParams, true);

	auto OnInstanceValueChangedLambda = [MainParam](int32 InValue) 
	{
		if (InValue < 0)
			return;

		if (MainParam->SetNumElements(InValue))
			MainParam->MarkChanged(true);
	};

	// Add multiparm UI.
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;
	int32 NumericalCount = MainParam->MultiParmInstanceCount;
	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
			.AllowSpin(true)

		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateLambda([OnInstanceValueChangedLambda](int32 InValue) {
				OnInstanceValueChangedLambda(InValue);
		}))
		.Value(NumericalCount)
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams]()
	{
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterMultiParamAddInstance", "Houdini Parameter Multi Parameter: Adding an instance"),
			MainParam->GetOuter(), true);

		for (auto& Param : MultiParmParams)
		{
			if (!IsValidWeakPointer(Param))
				continue;

			// Add a reverse step for redo/undo
			Param->MultiParmInstanceLastModifyArray.Add(EHoudiniMultiParmModificationType::Removed);

			Param->MarkChanged(true);
			Param->Modify();

			if (Param->MultiParmInstanceLastModifyArray.Num() > 0)
				Param->MultiParmInstanceLastModifyArray.RemoveAt(Param->MultiParmInstanceLastModifyArray.Num() - 1);

			Param->InsertElement();

		}
	}),
				LOCTEXT("AddMultiparmInstanceToolTipAddLastInstance", "Add an Instance"), true)
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			// Remove the last multiparm instance
			PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams]()
	{

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterMultiParamDeleteInstance", "Houdini Parameter Multi Parameter: Deleting an instance"),
			MainParam->GetOuter(), true);

		for (auto & Param : MultiParmParams)
		{
			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = Param->MultiParmInstanceLastModifyArray;
			int32 RemovedIndex = LastModifiedArray.Num() - 1;
			while (LastModifiedArray.IsValidIndex(RemovedIndex) && LastModifiedArray[RemovedIndex] == EHoudiniMultiParmModificationType::Removed)
				RemovedIndex -= 1;

			// Add a reverse step for redo/undo
			EHoudiniMultiParmModificationType PreviousModType = EHoudiniMultiParmModificationType::None;
			if (LastModifiedArray.IsValidIndex(RemovedIndex))
			{
				PreviousModType = LastModifiedArray[RemovedIndex];
				LastModifiedArray[RemovedIndex] = EHoudiniMultiParmModificationType::Inserted;
			}

			Param->MarkChanged(true);

			Param->Modify();

			if (LastModifiedArray.IsValidIndex(RemovedIndex))
			{
				LastModifiedArray[RemovedIndex] = PreviousModType;
			}

			Param->RemoveElement(RemovedIndex);
		}

	}),
				LOCTEXT("RemoveLastMultiParamLastToolTipRemoveLastInstance", "Remove the last instance"), true)

		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams]()
	{
		
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterMultiParamDeleteAllInstances", "Houdini Parameter Multi Parameter: Deleting all instances"),
			MainParam->GetOuter(), true);

		for (auto & Param : MultiParmParams)
		{
			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = Param->MultiParmInstanceLastModifyArray;
			TArray<int32> IndicesToReverse;

			for (int32 Index = 0; Index < LastModifiedArray.Num(); ++Index)
			{
				if (LastModifiedArray[Index] == EHoudiniMultiParmModificationType::None)
				{
					LastModifiedArray[Index] = EHoudiniMultiParmModificationType::Inserted;
					IndicesToReverse.Add(Index);
				}
			}

			Param->MarkChanged(true);

			Param->Modify();

			for (int32 & Index : IndicesToReverse)
			{
				if (LastModifiedArray.IsValidIndex(Index))
					LastModifiedArray[Index] = EHoudiniMultiParmModificationType::None;
			}


			Param->EmptyElements();
		}

	}),
				LOCTEXT("HoudiniParameterRemoveAllMultiparmInstancesToolTip", "Remove all instances"), true)
		];

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniParameterDetails::CreateWidgetMultiParmObjectButtons(TSharedPtr<SHorizontalBox> HorizontalBox, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	
	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];

	if (!IsValidWeakPointer(MainParam))
		return;

	if (!HorizontalBox || !AllMultiParms.Contains(MainParam->GetParentParmId()) || !MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
		return;

	const TWeakObjectPtr<UHoudiniParameterMultiParm>& MainParentMultiParm = AllMultiParms[MainParam->GetParentParmId()];

	if (!IsValidWeakPointer(MainParentMultiParm))
		return;

	if (!MainParentMultiParm->IsShown())
		return;

	// push all parent multiparm of the InParams to the array
	TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>> ParentMultiParams;
	for (auto & InParam : InParams) 
	{
		if (!IsValidWeakPointer(InParam))
			continue;

		if (!MultiParmInstanceIndices.Contains(InParam->GetParentParmId()))
			continue;

		if (InParam->GetChildIndex() == 0)
		{
			const TWeakObjectPtr<UHoudiniParameterMultiParm>& ParentMultiParm = AllMultiParms[InParam->GetParentParmId()];

			if (ParentMultiParm.IsValid())
				ParentMultiParams.Add(ParentMultiParm);
		}
	}


	int32 InstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];

	TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([ParentMultiParams, InstanceIndex]()
	{
		for (auto & ParentParam : ParentMultiParams)
		{
			// Add button call back
			if (!IsValidWeakPointer(ParentParam))
				continue;

			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = ParentParam->MultiParmInstanceLastModifyArray;

			if (!LastModifiedArray.IsValidIndex(InstanceIndex))
					continue;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterMultiParmAddBeforeCurInstance", "Houdini Parameter Multi Parm: Adding an instance"),
				ParentParam->GetOuter(), true);


			int32 Index = InstanceIndex;

			// Add a reverse step for undo/redo
			if (Index >= LastModifiedArray.Num())
				LastModifiedArray.Add(EHoudiniMultiParmModificationType::Removed);
			else
				LastModifiedArray.Insert(EHoudiniMultiParmModificationType::Removed, Index);

			ParentParam->MarkChanged(true);
			ParentParam->Modify();

			if (Index >= LastModifiedArray.Num() - 1 && LastModifiedArray.Num())
				LastModifiedArray.RemoveAt(LastModifiedArray.Num() - 1);
			else
				LastModifiedArray.RemoveAt(Index);

			ParentParam->InsertElementAt(InstanceIndex);
			
		}
	}),
		LOCTEXT("HoudiniParameterMultiParamAddBeforeCurrentInstanceToolTip", "Insert an instance before this instance"));


	TSharedRef<SWidget> RemoveButton = PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([ParentMultiParams, InstanceIndex]()
	{
		for (auto & ParentParam : ParentMultiParams) 
		{
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterMultiParmDeleteCurInstance", "Houdini Parameter Multi Parm: Deleting an instance"),
				ParentParam->GetOuter(), true);


			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = ParentParam->MultiParmInstanceLastModifyArray;

			int32 Index = InstanceIndex;
			EHoudiniMultiParmModificationType PreviousModType = EHoudiniMultiParmModificationType::None;
			while (LastModifiedArray.IsValidIndex(Index) && LastModifiedArray[Index] == EHoudiniMultiParmModificationType::Removed)
			{
				Index -= 1;
			}

			if (LastModifiedArray.IsValidIndex(Index))
			{
				PreviousModType = LastModifiedArray[Index];
				LastModifiedArray[Index] = EHoudiniMultiParmModificationType::Inserted;
			}

			ParentParam->MarkChanged(true);

			ParentParam->Modify();

			if (LastModifiedArray.IsValidIndex(Index))
			{
				LastModifiedArray[Index] = PreviousModType;
			}

			ParentParam->RemoveElement(InstanceIndex);
		}

	}),
		LOCTEXT("HoudiniParameterMultiParamDeleteCurrentInstanceToolTip", "Remove an instance"), true);


	HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f)[AddButton];
	HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f)[RemoveButton];

	int32 StartIdx = MainParam->GetParameterType() == EHoudiniParameterType::Folder ? 1 : 0;
	if (MainParam->GetChildIndex() != StartIdx)
	{
		AddButton.Get().SetVisibility(EVisibility::Hidden);
		RemoveButton.Get().SetVisibility(EVisibility::Hidden);
	}
	
}

void
FHoudiniParameterDetails::PruneStack()
{
	for (int32 StackItr = FolderStack.Num() - 1; StackItr >= 0; --StackItr)
	{
		TArray<UHoudiniParameterFolder*> &CurrentQueue = FolderStack[StackItr];

		for (int32 QueueItr = CurrentQueue.Num() - 1; QueueItr >= 0; --QueueItr)
		{
			UHoudiniParameterFolder * CurrentFolder = CurrentQueue[QueueItr];
			if (!IsValid(CurrentFolder))
				continue;

			if (CurrentFolder->GetChildCounter() == 0)
			{
				CurrentQueue.RemoveAt(QueueItr);
			}
		}

		if (CurrentQueue.Num() == 0)
		{
			FolderStack.RemoveAt(StackItr);
		}
	}
}

FText
FHoudiniParameterDetails::GetParameterTooltip(const TWeakObjectPtr<UHoudiniParameter>& InParam)
{
	if (!IsValidWeakPointer(InParam))
		return FText();

	// Tooltip starts with Label (name)
	FString Tooltip = InParam->GetParameterLabel() + TEXT(" (") + InParam->GetParameterName() + TEXT(")");

	// Append the parameter type
	FString ParmTypeStr = GetParameterTypeString(InParam->GetParameterType(), InParam->GetTupleSize());
	if (!ParmTypeStr.IsEmpty())
		Tooltip += TEXT("\n") + ParmTypeStr;

	// If the parameter has some help, append it
	FString Help = InParam->GetParameterHelp();
	if (!Help.IsEmpty())
		Tooltip += TEXT("\n") + Help;

	// If the parameter has an expression, append it
	if (InParam->HasExpression())
	{
		FString Expr = InParam->GetExpression();
		if (!Expr.IsEmpty())
			Tooltip += TEXT("\nExpression: ") + Expr;
	}

	return FText::FromString(Tooltip);
}

FString
FHoudiniParameterDetails::GetParameterTypeString(const EHoudiniParameterType& InType, const int32& InTupleSize)
{
	FString ParamStr;

	switch (InType)
	{
	case EHoudiniParameterType::Button:
		ParamStr = TEXT("Button");
		break;

	case EHoudiniParameterType::ButtonStrip:
		ParamStr = TEXT("Button Strip");
		break;

	case EHoudiniParameterType::Color:
	{
		if (InTupleSize == 4)
			ParamStr = TEXT("Color with Alpha");
		else
			ParamStr = TEXT("Color");
	}
	break;

	case EHoudiniParameterType::ColorRamp:
		ParamStr = TEXT("Color Ramp");
		break;

	case EHoudiniParameterType::File:
		ParamStr = TEXT("File (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::FileDir:
		ParamStr = TEXT("File Dir (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::FileGeo:
		ParamStr = TEXT("File Geo (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::FileImage:
		ParamStr = TEXT("File Image (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::Float:
		ParamStr = TEXT("Float (VEC") + FString::FromInt(InTupleSize) + TEXT(")");
		break;

	case EHoudiniParameterType::FloatRamp:
		ParamStr = TEXT("Float Ramp");
		break;

	case EHoudiniParameterType::Folder:
	case EHoudiniParameterType::FolderList:
		break;

	case EHoudiniParameterType::Input:
		ParamStr = TEXT("Opearator Path");
		break;

	case EHoudiniParameterType::Int:
		ParamStr = TEXT("Integer (VEC") + FString::FromInt(InTupleSize) + TEXT(")");
		break;

	case EHoudiniParameterType::IntChoice:
		ParamStr = TEXT("Int Choice");
		break;

	case EHoudiniParameterType::Label:
		ParamStr = TEXT("Label (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::MultiParm:
		ParamStr = TEXT("MultiParm");
		break;

	case EHoudiniParameterType::Separator:
		break;

	case EHoudiniParameterType::String:
		ParamStr = TEXT("String (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::StringAssetRef:
		ParamStr = TEXT("String Asset Ref (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::StringChoice:
		ParamStr = TEXT("String Choice");
		break;

	case EHoudiniParameterType::Toggle:
		ParamStr = TEXT("Toggle (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	default:
		ParamStr = TEXT("invalid parameter type");
		break;
	}


	return ParamStr;
}

void
FHoudiniParameterDetails::SyncCachedColorRampPoints(UHoudiniParameterRampColor* ColorRampParameter) 
{
	if (!ColorRampParameter)
		return;

	// Do not sync when the Houdini asset component is cooking
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(ColorRampParameter))
		return;

	TArray<UHoudiniParameterRampColorPoint*> &CachedPoints = ColorRampParameter->CachedPoints;
	TArray<UHoudiniParameterRampColorPoint*> &CurrentPoints = ColorRampParameter->Points;

	bool bCurveNeedsUpdate = false;
	bool bRampParmNeedsUpdate = false;

	int32 Idx = 0;

	while (Idx < CachedPoints.Num() && Idx < CurrentPoints.Num())
	{
		UHoudiniParameterRampColorPoint* CachedPoint = CachedPoints[Idx];
		UHoudiniParameterRampColorPoint* CurrentPoint = CurrentPoints[Idx];

		if (!CachedPoint || !CurrentPoint)
			continue;

		if (CachedPoint->GetPosition() != CurrentPoint->GetPosition())
		{
			if (CurrentPoint->PositionParentParm)
			{
				CurrentPoint->SetPosition(CachedPoint->GetPosition());
				CurrentPoint->PositionParentParm->MarkChanged(true);
				bCurveNeedsUpdate = true;
			}
		}

		if (CachedPoint->GetValue() != CurrentPoint->GetValue())
		{
			if (CurrentPoint->ValueParentParm)
			{
				CurrentPoint->SetValue(CachedPoint->GetValue());
				CurrentPoint->ValueParentParm->MarkChanged(true);
				bCurveNeedsUpdate = true;
			}
		}

		if (CachedPoint->GetInterpolation() != CurrentPoint->GetInterpolation())
		{
			if (CurrentPoint->InterpolationParentParm)
			{
				CurrentPoint->SetInterpolation(CachedPoint->GetInterpolation());
				CurrentPoint->InterpolationParentParm->MarkChanged(true);
				bCurveNeedsUpdate = true;
			}
		}

		Idx += 1;
	}

	// Insert points
	for (int32 IdxCachedPointLeft = Idx; IdxCachedPointLeft < CachedPoints.Num(); ++IdxCachedPointLeft)
	{
		UHoudiniParameterRampColorPoint* CachedPoint = CachedPoints[IdxCachedPointLeft];

		CreateColorRampParameterInsertEvent(
			ColorRampParameter, CachedPoint->Position, CachedPoint->Value, CachedPoint->Interpolation);

		bCurveNeedsUpdate = true;
		bRampParmNeedsUpdate = true;

	}

	// Delete points
	for (int32 IdxCurrentPointLeft = Idx; IdxCurrentPointLeft < CurrentPoints.Num(); ++IdxCurrentPointLeft)
	{
		ColorRampParameter->RemoveElement(IdxCurrentPointLeft);

		UHoudiniParameterRampColorPoint* Point = CurrentPoints[IdxCurrentPointLeft];

		if (!Point)
			continue;

		CreateColorRampParameterDeleteEvent(ColorRampParameter, Point->InstanceIndex);

		bCurveNeedsUpdate = true;
		bRampParmNeedsUpdate = true;
	}


	ColorRampParameter->MarkChanged(bRampParmNeedsUpdate);
}

//void 
//FHoudiniParameterDetails::SyncCachedFloatRampPoints(UHoudiniParameterRampFloat* FloatRampParameter) 
//{
//	if (!FloatRampParameter)
//		return;
//
//	// Do not sync when the Houdini asset component is cooking
//	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(FloatRampParameter))
//		return;
//
//	TArray<UHoudiniParameterRampFloatPoint*> &CachedPoints = FloatRampParameter->CachedPoints;
//	TArray<UHoudiniParameterRampFloatPoint*> &CurrentPoints = FloatRampParameter->Points;
//
//	int32 Idx = 0;
//
//	while (Idx < CachedPoints.Num() && Idx < CurrentPoints.Num())
//	{
//		UHoudiniParameterRampFloatPoint* &CachedPoint = CachedPoints[Idx];
//		UHoudiniParameterRampFloatPoint* &CurrentPoint = CurrentPoints[Idx];
//
//		if (!CachedPoint || !CurrentPoint)
//			continue;
//
//		if (CachedPoint->GetPosition() != CurrentPoint->GetPosition()) 
//		{
//			if (CurrentPoint->PositionParentParm) 
//			{
//				CurrentPoint->SetPosition(CachedPoint->GetPosition());
//				CurrentPoint->PositionParentParm->MarkChanged(true);
//			}
//		}
//
//		if (CachedPoint->GetValue() != CurrentPoint->GetValue()) 
//		{
//			if (CurrentPoint->ValueParentParm) 
//			{
//				CurrentPoint->SetValue(CachedPoint->GetValue());
//				CurrentPoint->ValueParentParm->MarkChanged(true);
//			}
//		}
//
//		if (CachedPoint->GetInterpolation() != CurrentPoint->GetInterpolation()) 
//		{
//			if (CurrentPoint->InterpolationParentParm) 
//			{
//				CurrentPoint->SetInterpolation(CachedPoint->GetInterpolation());
//				CurrentPoint->InterpolationParentParm->MarkChanged(true);
//			}
//		}
//
//		Idx += 1;
//	}
//
//	// Insert points
//	for (int32 IdxCachedPointLeft = Idx; IdxCachedPointLeft < CachedPoints.Num(); ++IdxCachedPointLeft) 
//	{
//		UHoudiniParameterRampFloatPoint *&CachedPoint = CachedPoints[IdxCachedPointLeft];
//		if (!CachedPoint)
//			continue;
//
//		CreateFloatRampParameterInsertEvent(
//			FloatRampParameter, CachedPoint->Position, CachedPoint->Value, CachedPoint->Interpolation);
//
//		FloatRampParameter->MarkChanged(true);
//	}
//
//	// Remove points
//	for (int32 IdxCurrentPointLeft = Idx; IdxCurrentPointLeft < CurrentPoints.Num(); ++IdxCurrentPointLeft) 
//	{
//		FloatRampParameter->RemoveElement(IdxCurrentPointLeft);
//
//		UHoudiniParameterRampFloatPoint* Point = CurrentPoints[IdxCurrentPointLeft];
//
//		if (!Point)
//			continue;
//
//		CreateFloatRampParameterDeleteEvent(FloatRampParameter, Point->InstanceIndex);
//
//		FloatRampParameter->MarkChanged(true);
//	}
//}

void 
FHoudiniParameterDetails::CreateFloatRampParameterDeleteEvent(UHoudiniParameterRampFloat* InParam, const int32 &InDeleteIndex) 
{
	if (!IsValid(InParam))
		return;

	UHoudiniParameterRampModificationEvent* DeleteEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!DeleteEvent)
		return;

	DeleteEvent->SetFloatRampEvent();
	DeleteEvent->SetDeleteEvent();
	DeleteEvent->DeleteInstanceIndex = InDeleteIndex;

	InParam->ModificationEvents.Add(DeleteEvent);
}

void
FHoudiniParameterDetails::CreateColorRampParameterDeleteEvent(UHoudiniParameterRampColor* InParam, const int32 &InDeleteIndex)
{
	if (!IsValid(InParam))
		return;

	UHoudiniParameterRampModificationEvent* DeleteEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!DeleteEvent)
		return;

	DeleteEvent->SetColorRampEvent();
	DeleteEvent->SetDeleteEvent();
	DeleteEvent->DeleteInstanceIndex = InDeleteIndex;

	InParam->ModificationEvents.Add(DeleteEvent);
}

void 
FHoudiniParameterDetails::CreateFloatRampParameterInsertEvent(UHoudiniParameterRampFloat* InParam,
	const float& InPosition, const float& InValue, const EHoudiniRampInterpolationType &InInterp) 
{
	if (!IsValid(InParam))
		return;

	UHoudiniParameterRampModificationEvent* InsertEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!InsertEvent)
		return;

	InsertEvent->SetFloatRampEvent();
	InsertEvent->SetInsertEvent();
	InsertEvent->InsertPosition = InPosition;
	InsertEvent->InsertFloat = InValue;
	InsertEvent->InsertInterpolation = InInterp;

	InParam->ModificationEvents.Add(InsertEvent);
}

void 
FHoudiniParameterDetails::CreateColorRampParameterInsertEvent(UHoudiniParameterRampColor* InParam,
	const float& InPosition, const FLinearColor& InColor, const EHoudiniRampInterpolationType &InInterp) 
{
	if (!IsValid(InParam))
		return;

	UHoudiniParameterRampModificationEvent* InsertEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!InsertEvent)
		return;

	InsertEvent->SetColorRampEvent();
	InsertEvent->SetInsertEvent();
	InsertEvent->InsertPosition = InPosition;
	InsertEvent->InsertColor = InColor;
	InsertEvent->InsertInterpolation = InInterp;

	InParam->ModificationEvents.Add(InsertEvent);
}

void
FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(const TArray<UHoudiniParameterRampFloat*>& FloatRampParameters)
{
	if (FloatRampParameters.Num() < 1)
		return;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0];

	if (!IsValid(MainParam))
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	for (int32 Idx = 1; Idx < FloatRampParameters.Num(); ++Idx) 
	{
		UHoudiniParameterRampFloat* NextFloatRampParameter = FloatRampParameters[Idx];

		if (!IsValid(NextFloatRampParameter))
			continue;

		FHoudiniParameterDetails::ReplaceFloatRampParameterPointsWithMainParameter(MainParam, NextFloatRampParameter);
	}
}

void 
FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters) 
{
	if (FloatRampParameters.Num() < 1)
		return;

	if (!IsValidWeakPointer(FloatRampParameters[0]))
		return;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;


	for (int32 Idx = 1; Idx < FloatRampParameters.Num(); ++Idx)
	{
		if (!IsValidWeakPointer(FloatRampParameters[Idx]))
			continue;

		UHoudiniParameterRampFloat* NextFloatRampParameter = FloatRampParameters[Idx].Get();

		if (!NextFloatRampParameter)
			continue;

		FHoudiniParameterDetails::ReplaceFloatRampParameterPointsWithMainParameter(MainParam, NextFloatRampParameter);
	}

}

void
FHoudiniParameterDetails:: ReplaceFloatRampParameterPointsWithMainParameter(UHoudiniParameterRampFloat* Param, UHoudiniParameterRampFloat* MainParam)
{
	if (!Param || !MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(Param))
		return;

	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	// Use Synced points if the MainParam is on auto update mode
	// Use Cached points if the Mainparam is on manual update mode

	TArray<UHoudiniParameterRampFloatPoint*> & MainPoints = (MainParam->IsAutoUpdate() && bCookingEnabled) ? MainParam->Points : MainParam->CachedPoints;

	if (Param->IsAutoUpdate() && bCookingEnabled)
	{
		TArray<UHoudiniParameterRampFloatPoint*> & Points = Param->Points;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampFloatPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampFloatPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (MainPoint->GetPosition() != Point->GetPosition())
			{
				if (Point->PositionParentParm)
				{
					Point->SetPosition(MainPoint->GetPosition());
					Point->PositionParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetValue() != Point->GetValue())
			{
				if (Point->ValueParentParm)
				{
					Point->SetValue(MainPoint->GetValue());
					Point->ValueParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetInterpolation() != Point->GetInterpolation())
			{
				if (Point->InterpolationParentParm)
				{
					Point->SetInterpolation(MainPoint->GetInterpolation());
					Point->InterpolationParentParm->MarkChanged(true);
				}
			}

			PointIdx += 1;
		}

		int32 PointInsertIdx = PointIdx;
		int32 PointDeleteIdx = PointIdx;

		// skip the pending modification events
		for (auto & Event : Param->ModificationEvents)
		{
			if (!Event)
				continue;

			if (Event->IsInsertEvent())
				PointInsertIdx += 1;

			if (Event->IsDeleteEvent())
				PointDeleteIdx += 1;
		}

		// There are more points in MainPoints array
		for (; PointInsertIdx < MainPoints.Num(); ++PointInsertIdx)
		{
			UHoudiniParameterRampFloatPoint*& NextMainPoint = MainPoints[PointInsertIdx];

			if (!NextMainPoint)
				continue;

			FHoudiniParameterDetails::CreateFloatRampParameterInsertEvent(Param,
				NextMainPoint->GetPosition(), NextMainPoint->GetValue(), NextMainPoint->GetInterpolation());

			Param->MarkChanged(true);
		}

		// There are more points in Points array
		for (; PointDeleteIdx < Points.Num(); ++PointDeleteIdx)
		{
			UHoudiniParameterRampFloatPoint*& NextPoint = Points[PointDeleteIdx];

			if (!NextPoint)
				continue;

			FHoudiniParameterDetails::CreateFloatRampParameterDeleteEvent(Param, NextPoint->InstanceIndex);

			Param->MarkChanged(true);
		}

	}
	else
	{
		TArray<UHoudiniParameterRampFloatPoint*> &Points = Param->CachedPoints;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampFloatPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampFloatPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (Point->Position != MainPoint->Position)
			{
				Point->Position = MainPoint->Position;
				Param->bCaching = true;
				if (!bCookingEnabled)
				{
					if (Point->InterpolationParentParm)
						Point->PositionParentParm->MarkChanged(true);
					Param->MarkChanged(true);
				}
			}

			if (Point->Value != MainPoint->Value)
			{
				Point->Value = MainPoint->Value;
				Param->bCaching = true;
				if (!bCookingEnabled)
				{
					if (Point->ValueParentParm)
						Point->ValueParentParm->MarkChanged(true);
					Param->MarkChanged(true);
				}
			}

			if (Point->Interpolation != MainPoint->Interpolation)
			{
				Point->Interpolation = MainPoint->Interpolation;
				Param->bCaching = true;
				if (!bCookingEnabled)
				{
					if (Point->InterpolationParentParm)
						Point->InterpolationParentParm->MarkChanged(true);
					Param->MarkChanged(true);
				}
			}

			PointIdx += 1;
		}

		// There are more points in MainPoints array
		for (int32 MainPointsLeftIdx = PointIdx; MainPointsLeftIdx < MainPoints.Num(); ++MainPointsLeftIdx)
		{
			UHoudiniParameterRampFloatPoint* NextMainPoint = MainPoints[MainPointsLeftIdx];

			if (!NextMainPoint)
				continue;

			UHoudiniParameterRampFloatPoint* NewCachedPoint = NewObject<UHoudiniParameterRampFloatPoint>(Param, UHoudiniParameterRampFloatPoint::StaticClass());

			if (!NewCachedPoint)
				continue;

			NewCachedPoint->Position = NextMainPoint->GetPosition();
			NewCachedPoint->Value = NextMainPoint->GetValue();
			NewCachedPoint->Interpolation = NextMainPoint->GetInterpolation();

			Points.Add(NewCachedPoint);

			Param->bCaching = true;
		}

		// there are more points in Points array
		for (int32 PointsLeftIdx = PointIdx; PointIdx < Points.Num(); ++PointIdx)
		{
			Points.Pop();
			Param->bCaching = true;
		}
	}

}


void 
FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(const TArray<UHoudiniParameterRampColor*>& ColorRampParameters) 
{
	if (ColorRampParameters.Num() < 1)
		return;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0];

	if (!IsValid(MainParam))
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	for (int32 Idx = 1; Idx < ColorRampParameters.Num(); ++Idx) 
	{
		UHoudiniParameterRampColor* NextColorRampParam = ColorRampParameters[Idx];

		if (!IsValid(NextColorRampParam))
			continue;

		FHoudiniParameterDetails::ReplaceColorRampParameterPointsWithMainParameter(MainParam, NextColorRampParam);
	}
}

void 
FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>>& ColorRampParameters) 
{
	if (ColorRampParameters.Num() < 1)
		return;

	if (!IsValidWeakPointer(ColorRampParameters[0]))
		return;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0].Get();

	if (!MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	for (int32 Idx = 1; Idx < ColorRampParameters.Num(); ++Idx)
	{
		if (!IsValidWeakPointer(ColorRampParameters[Idx]))
			continue;

		UHoudiniParameterRampColor* NextColorRampParameter = ColorRampParameters[Idx].Get();

		if (!NextColorRampParameter)
			continue;

		FHoudiniParameterDetails::ReplaceColorRampParameterPointsWithMainParameter(MainParam, NextColorRampParameter);

	}

}

void 
FHoudiniParameterDetails::ReplaceColorRampParameterPointsWithMainParameter(UHoudiniParameterRampColor* Param, UHoudiniParameterRampColor* MainParam) 
{
	if (!Param || !MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(Param))
		return;

	const bool bCookingEnabled = FHoudiniEngine::Get().IsCookingEnabled();

	// Use Synced points if the MainParam is on auto update mode
	// Use Cached points if the Mainparam is on manual update mode
	TArray<UHoudiniParameterRampColorPoint*> & MainPoints = (MainParam->IsAutoUpdate() && bCookingEnabled) ? MainParam->Points : MainParam->CachedPoints;
	if (Param->IsAutoUpdate() && bCookingEnabled)
	{
		TArray<UHoudiniParameterRampColorPoint*> & Points = Param->Points;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampColorPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampColorPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (MainPoint->GetPosition() != Point->GetPosition())
			{
				if (Point->PositionParentParm)
				{
					Point->SetPosition(MainPoint->GetPosition());
					Point->PositionParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetValue() != Point->GetValue())
			{
				if (Point->ValueParentParm)
				{
					Point->SetValue(MainPoint->GetValue());
					Point->ValueParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetInterpolation() != Point->GetInterpolation())
			{
				if (Point->InterpolationParentParm)
				{
					Point->SetInterpolation(MainPoint->GetInterpolation());
					Point->InterpolationParentParm->MarkChanged(true);
				}
			}

			PointIdx += 1;

		}

		int32 PointInsertIdx = PointIdx;
		int32 PointDeleteIdx = PointIdx;

		// skip the pending modification events
		for (auto & Event : Param->ModificationEvents)
		{
			if (!Event)
				continue;

			if (Event->IsInsertEvent())
				PointInsertIdx += 1;

			if (Event->IsDeleteEvent())
				PointDeleteIdx += 1;
		}

		// There are more points in MainPoints array
		for (; PointInsertIdx < MainPoints.Num(); ++PointInsertIdx)
		{
			UHoudiniParameterRampColorPoint*& NextMainPoint = MainPoints[PointInsertIdx];
			if (!NextMainPoint)
				continue;

			FHoudiniParameterDetails::CreateColorRampParameterInsertEvent(Param,
				NextMainPoint->GetPosition(), NextMainPoint->GetValue(), NextMainPoint->GetInterpolation());

			Param->MarkChanged(true);
		}

		// There are more points in Points array
		for (; PointDeleteIdx < Points.Num(); ++PointDeleteIdx)
		{
			UHoudiniParameterRampColorPoint*& NextPoint = Points[PointDeleteIdx];
			if (!NextPoint)
				continue;

			FHoudiniParameterDetails::CreateColorRampParameterDeleteEvent(Param, NextPoint->InstanceIndex);

			Param->MarkChanged(true);
		}
	}
	else
	{
		TArray<UHoudiniParameterRampColorPoint*> &Points = Param->CachedPoints;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampColorPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampColorPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (Point->Position != MainPoint->Position)
			{
				Point->Position = MainPoint->Position;
				Param->bCaching = true;
				if (!bCookingEnabled)
				{
					if (Point->PositionParentParm)
						Point->PositionParentParm->MarkChanged(true);
					Param->MarkChanged(true);
				}
			}

			if (Point->Value != MainPoint->Value)
			{
				Point->Value = MainPoint->Value;
				Param->bCaching = true;
				if (!bCookingEnabled)
				{
					if (Point->ValueParentParm)
						Point->ValueParentParm->MarkChanged(true);
					Param->MarkChanged(true);
				}
			}

			if (Point->Interpolation != MainPoint->Interpolation)
			{
				Point->Interpolation = MainPoint->Interpolation;
				Param->bCaching = true;
				if (!bCookingEnabled)
				{
					if (Point->InterpolationParentParm)
						Point->InterpolationParentParm->MarkChanged(true);
					Param->MarkChanged(true);
				}
			}

			PointIdx += 1;
		}

		// There are more points in Main Points array.
		for (int32 MainPointsLeftIdx = PointIdx; MainPointsLeftIdx < MainPoints.Num(); ++MainPointsLeftIdx)
		{
			UHoudiniParameterRampColorPoint* NextMainPoint = MainPoints[MainPointsLeftIdx];

			if (!NextMainPoint)
				continue;

			UHoudiniParameterRampColorPoint* NewCachedPoint = NewObject<UHoudiniParameterRampColorPoint>(Param, UHoudiniParameterRampColorPoint::StaticClass());
			if (!NewCachedPoint)
				continue;

			NewCachedPoint->Position = NextMainPoint->Position;
			NewCachedPoint->Value = NextMainPoint->Value;
			NewCachedPoint->Interpolation = NextMainPoint->Interpolation;

			Points.Add(NewCachedPoint);

			Param->bCaching = true;
		}

		// There are more points in Points array
		for (int32 PointsleftIdx = PointIdx; PointIdx < MainPoints.Num(); ++PointsleftIdx)
		{
			Points.Pop();

			Param->bCaching = true;
		}
	}
}

// Check recursively if a parameter hits the end of a visible tabs
void
FHoudiniParameterDetails::RemoveTabDividers(IDetailCategoryBuilder& HouParameterCategory, const TWeakObjectPtr<UHoudiniParameter>& InParam)
{
	if (!IsValidWeakPointer(InParam))
		return;

	// When the paramId is invalid, the directory won't parse.
	// So simply return the function
	if (InParam->GetParmId() < 0)
		return;

	// Do not end the tab if this param is a non empty parent type, leave it to its children
	EHoudiniParameterType ParmType = InParam->GetParameterType();
	if ((ParmType == EHoudiniParameterType::FolderList ||
		 ParmType == EHoudiniParameterType::Folder) && InParam->GetTupleSize() > 0)
		return;

	if (ParmType == EHoudiniParameterType::MultiParm)
	{
		UHoudiniParameterMultiParm* InMultiParm = Cast<UHoudiniParameterMultiParm>(InParam);
		if (!InMultiParm)
			return;

		if (InMultiParm->MultiParmInstanceCount *  InMultiParm->MultiParmInstanceLength > 0)
			return;
	}

	int32 ParentParamId = InParam->GetParentParmId();
	TWeakObjectPtr<UHoudiniParameter> CurParam = InParam;

	while (AllFoldersAndFolderLists.Contains(ParentParamId) || AllMultiParms.Contains(ParentParamId))
	{		
		if (AllMultiParms.Contains(ParentParamId))
		{
			// The parent is a multiparm
			const TWeakObjectPtr<UHoudiniParameterMultiParm>& ParentMultiParm = AllMultiParms[ParentParamId];
			if (!IsValidWeakPointer(ParentMultiParm))
				return;

			if (ParentMultiParm->MultiParmInstanceCount * ParentMultiParm->MultiParmInstanceLength - 1 == CurParam->GetChildIndex())
			{
				ParentParamId = ParentMultiParm->GetParentParmId();
				CurParam = ParentMultiParm;

				continue;
			}
			else
			{
				// return directly if the parameter is not the last child param of the multiparm
				return;
			}
		}
		else 
		{
			// The parent is a folder or folderlist
			TWeakObjectPtr<UHoudiniParameter> ParentFolderParam = AllFoldersAndFolderLists[ParentParamId];
			CurParam = ParentFolderParam;

			if (!IsValidWeakPointer(ParentFolderParam))
				return;

			if (ParentFolderParam->GetParameterType() == EHoudiniParameterType::Folder) 
			{
				// The parent is a folder
				ParentParamId = ParentFolderParam->GetParentParmId();		
				continue;
			}
			else
			{
				// The parent is a folderlist
				UHoudiniParameterFolderList const* const ParentFolderList = Cast<UHoudiniParameterFolderList>(ParentFolderParam);
				if (!IsValid(ParentFolderList))
					return;

				if (ParentFolderList->IsTabMenu() && ParentFolderList->IsTabsShown() && ParentFolderList->IsTabParseFinished() && DividerLinePositions.Num() > 0)
				{
					if (!CurrentTabEndingRow)
						CreateTabEndingRow(HouParameterCategory);

					if (CurrentTabEndingRow && CurrentTabEndingRow->DividerLinePositions.Num() > 0)
					{
						CurrentTabEndingRow->EndingDividerLinePositions.Add(DividerLinePositions.Top());
						CurrentTabEndingRow->DividerLinePositions.Pop();
					}

					DividerLinePositions.Pop();

					ParentParamId = ParentFolderList->GetParentParmId();
				}
				else
				{
					return;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
