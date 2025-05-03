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

#include "SHoudiniColorRamp.h"
#include "SHoudiniFloatRamp.h"

#include "Sound/SoundBase.h"
#include "Engine/SkeletalMesh.h"
#include "Particles/ParticleSystem.h"
#include "FoliageType.h"

#include "HoudiniInputDetails.h"

#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"
#include "DetailCategoryBuilder.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

// Copied from SDetailSingleItemRow.cpp
namespace DetailWidgetConstants
{
const FMargin LeftRowPadding(20.0f, 0.0f, 10.0f, 0.0f);
const FMargin RightRowPadding(12.0f, 0.0f, 2.0f, 0.0f);
}


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
		Line[0].X = AllottedGeometry.Size.X - AllottedGeometry.Size.Y / 2.0f + 10.0f;
		Line[0].Y = Content->GetDesiredSize().Y / 2.0f;
		Line[1].X = AllottedGeometry.Size.Y / 2.0f - 10.0f;
		Line[1].Y = Content->GetDesiredSize().Y / 2.0f;

		Color = FLinearColor::White;
		Color.A = bIsRadioButton ? 0.05 : 0.1;

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line,
			ESlateDrawEffect::None, Color, true, AllottedGeometry.Size.Y - 10.0f);
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
					case EHoudiniInputType::Curve:
					{
						MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE_MULTIPARMHEADER
							+ Input->GetNumberOfInputObjects() * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE_INSTANCE_MULTIPARMHEADER;
					}
					break;

					case EHoudiniInputType::Geometry:
					case EHoudiniInputType::World:
					default:
						MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_MULTIPARMHEADER;
						break;
				}
			}
			else
			{
				switch (Input->GetInputType())
				{
					case EHoudiniInputType::Curve:
					{
						MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE
							+ Input->GetNumberOfInputObjects() * HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INPUT_CURVE_INSTANCE;
					}
					break;

					case EHoudiniInputType::Geometry:
					case EHoudiniInputType::World:
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
		{
			MarginHeight = HOUDINI_PARAMETER_UI_ROW_MARGIN_HEIGHT_INVALID;
		}
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
FHoudiniParameterDetails::Debug()
{
	int Entry = 0;
	for(auto & StackEntry : FolderStack)
	{
		FString Output = FString::Format(TEXT("{0} "), { Entry  });
		for(int Index = 0; Index < StackEntry.Num(); Index++)
		{
			Output += StackEntry[Index]->GetParameterLabel();
			Output += TEXT(" ");
		}

		HOUDINI_LOG_MESSAGE(TEXT("%s\n"), *Output);
		Entry++;

	}
}

void
FHoudiniParameterDetails::CreateWidget(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams)
{
	// Uncomment this to debug printf the state of the Stack.
	//Debug();

	if (InJoinedParams.IsEmpty())
		return;

	const auto& FirstLinkedParams = InJoinedParams[0];

	if (FirstLinkedParams.IsEmpty())
		return;

	const TWeakObjectPtr<UHoudiniParameter>& InParam = FirstLinkedParams[0];
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

	TArray<FDetailWidgetRow*> Rows;

	switch (InParam->GetParameterType())
	{
		case EHoudiniParameterType::Float:
		case EHoudiniParameterType::Int:
		case EHoudiniParameterType::String:
		case EHoudiniParameterType::IntChoice:
		case EHoudiniParameterType::StringChoice:
		case EHoudiniParameterType::Separator:
		case EHoudiniParameterType::Color:
		case EHoudiniParameterType::Button:
		case EHoudiniParameterType::ButtonStrip:
		case EHoudiniParameterType::Label:
		case EHoudiniParameterType::Toggle:
		case EHoudiniParameterType::File:
		case EHoudiniParameterType::FileDir:
		case EHoudiniParameterType::FileGeo:
		case EHoudiniParameterType::FileImage:
			CreateJoinableWidget(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::FolderList:
			CreateWidgetFolderList(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::Folder:
			CreateWidgetFolder(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::MultiParm:
			CreateWidgetMultiParm(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::FloatRamp:
			CreateWidgetFloatRamp(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::ColorRamp:
			CreateWidgetColorRamp(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::Input:
			CreateWidgetOperatorPath(HouParameterCategory, InJoinedParams, Rows);
		break;

		case EHoudiniParameterType::Invalid:
			HandleUnsupportedParmType(HouParameterCategory, InJoinedParams);
		break;

		default:
			HandleUnsupportedParmType(HouParameterCategory, InJoinedParams);
		break;
	}
	
	uint32 MetaDataIndex = 0;
	for (FDetailWidgetRow* const Row : Rows)
	{
		if (!Row)
		{
			continue;
		}

		// Add meta data to all possible slots in the row
		const TSharedRef<SWidget> Widgets[] = {
			Row->ExtensionWidget.Widget,
			Row->NameWidget.Widget,
			Row->WholeRowWidget.Widget,
			Row->ValueWidget.Widget };

		for (auto Widget : Widgets)
		{
			AddMetaDataToAllDescendants(Widget, InParam->GetParameterName(), MetaDataIndex);
		}
	}

	// Remove a divider lines recursively if last joined parameter hits the end of a tab
	{
		const auto& LastLinkedParams = InJoinedParams.Last();
		if (LastLinkedParams.IsEmpty())
			return;

		const TWeakObjectPtr<UHoudiniParameter>& LastParam = LastLinkedParams[0];
		if (!IsValidWeakPointer(InParam))
			return;

		RemoveTabDividers(HouParameterCategory, LastParam);
	}
}

void FHoudiniParameterDetails::CreateJoinableWidget(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InJoinedParams);

	if (!Row)
		return;

	OutRows.Add(Row);

	const bool bIsJoined = InJoinedParams.Num() > 1;
	const bool bUseWholeRow = bIsJoined || UsesWholeRow(InJoinedParams[0]);
	FDetailWidgetDecl& Slot = (bUseWholeRow ? Row->WholeRowContent() : Row->ValueContent());

	// For the sake of simplicity we always have a container to hold joinable parameters, even when
	// not horizontally joining. We only want to use the customized box (for indentation of nested
	// widgets) when we are horizontally joining.
	TSharedPtr<SHorizontalBox> HorizontalJoinBox;
	if (bIsJoined)
	{
		HorizontalJoinBox = CreateCustomizedBox(InJoinedParams[0]);
	}
	else
	{
		SAssignNew(HorizontalJoinBox, SHorizontalBox);
	}

	if (!HorizontalJoinBox.IsValid())
	{
		return;
	}

	Slot.Widget = HorizontalJoinBox.ToSharedRef();

	for (const auto& LinkedParams : InJoinedParams)
	{
		if (LinkedParams.IsEmpty())
		{
			continue;
		}

		const auto& Param = LinkedParams[0];

		if (!IsValidWeakPointer(Param))
		{
			continue;
		}

		// The directory won't parse if parameter ids are -1
		if (Param->GetParmId() < 0)
		{
			continue;
		}

		TSharedRef LabelledParameter = SNew(SHoudiniLabelledParameter);
		LabelledParameter->SetEnabled(!Param->IsDisabled());

		const bool bUseLabel = IsLabelVisible(LinkedParams);

		// We only need a custom solution for displaying the name text block when horizontally
		// joining multiple parameters. Otherwise, we can use Unreal's provided columns in the
		// details panel.
		TSharedPtr<STextBlock> TextBlock = CreateNameTextBlock(LinkedParams);
		if (bIsJoined)
		{
			if (TextBlock.IsValid() && bUseLabel)
			{
				LabelledParameter->SetNameContent(TextBlock.ToSharedRef());
			}

			if (!Param->ShouldDisplay())
			{
				LabelledParameter->SetVisibility(EVisibility::Hidden);
			}
		}
		else if (bUseLabel)
		{
			TSharedPtr<SCustomizedBox> CustomizedBox = CreateCustomizedBox(LinkedParams);

			if (CustomizedBox.IsValid() && TextBlock.IsValid())
			{
				TextBlock->SetEnabled(!Param->IsDisabled());
				CustomizedBox->AddSlot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.AttachWidget(TextBlock.ToSharedRef());
				Row->NameContent().Widget = CustomizedBox.ToSharedRef();
			}
		}

		// Attach our labelled parameter widget to the horizontal box containg all joined parameters
		// on this row.
		{
			SHorizontalBox::FScopedWidgetSlotArguments SlotArguments = HorizontalJoinBox->AddSlot();
			if (!ShouldWidgetFill(Param->GetParameterType()))
			{
				// Make widget occupy minimum required space
				SlotArguments.AutoWidth();
			}
			if (Param->GetJoinNext())
			{
				// If there is a parameter to the right, add some padding in between
				SlotArguments.Padding(0.f, 0.f, HAPI_UNREAL_PADDING_HORIZONTAL_JOIN, 0.f);
			}
			SlotArguments.AttachWidget(LabelledParameter);
		}

		switch (Param->GetParameterType())
		{
		case EHoudiniParameterType::Int:
			CreateWidgetInt(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::Float:
			CreateWidgetFloat(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::String:
			CreateWidgetString(HouParameterCategory, LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::IntChoice:
		case EHoudiniParameterType::StringChoice:
			CreateWidgetChoice(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::Separator:
			CreateWidgetSeparator(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::Color:
			CreateWidgetColor(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::Button:
			CreateWidgetButton(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::ButtonStrip:
			CreateWidgetButtonStrip(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::Label:
			CreateWidgetLabel(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::Toggle:
			CreateWidgetToggle(LabelledParameter, LinkedParams);
			break;

		case EHoudiniParameterType::File:
		case EHoudiniParameterType::FileDir:
		case EHoudiniParameterType::FileGeo:
		case EHoudiniParameterType::FileImage:
			CreateWidgetFile(LabelledParameter, LinkedParams);
			break;

		default:
			break;
		}
	}
}

void
FHoudiniParameterDetails::CreateTabEndingRow(IDetailCategoryBuilder& HouParameterCategory)
{
	FDetailWidgetRow& Row = HouParameterCategory.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SCustomizedBox> TabEndingRow = SNew(SCustomizedBox);

	TabEndingRow->DividerLinePositions = DividerLinePositions;

	if (TabEndingRow.IsValid())
		CurrentTabEndingRow = TabEndingRow.Get();

	Row.WholeRowWidget.Widget = TabEndingRow.ToSharedRef();
	Row.WholeRowWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

TSharedPtr<SCustomizedBox>
FHoudiniParameterDetails::CreateCustomizedBox(
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	if (InParams.Num() <= 0)
		return nullptr;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return nullptr;

	TSharedPtr<SCustomizedBox> CustomizedBox = SNew(SCustomizedBox);

	CustomizedBox->DividerLinePositions = DividerLinePositions;
	CustomizedBox->SetHoudiniParameter(InParams);
	CustomizedBox->AddIndentation(MainParam.Get(), AllMultiParms, AllFoldersAndFolderLists);

	if (MainParam->IsDirectChildOfMultiParm())
	{
		// If it is head of an multiparm instance
		if (MainParam->GetChildIndex() == 0)
		{
			CreateWidgetMultiParmObjectButtons(CustomizedBox, InParams);
		}
	}


	return CustomizedBox;
}

TSharedPtr<STextBlock>
FHoudiniParameterDetails::CreateNameTextBlock(
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	if (InParams.Num() <= 0)
		return nullptr;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return nullptr;

	FString ParameterLabelString = MainParam->GetParameterLabel();

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
			ParameterLabelString += TEXT(" (") + FString::FromInt(CurrentMultiParmInstanceIndex + 1) + TEXT(")");
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(ParameterLabelString))
		.ToolTipText(GetParameterTooltip(MainParam))
		.Font(_GetEditorStyle().GetFontStyle(
			MainParam->IsDefault()
			? TEXT("PropertyWindow.NormalFont")
			: TEXT("PropertyWindow.BoldFont")));
}

void
FHoudiniParameterDetails::CreateNameWidget(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, bool WithLabel)
{
	if (!Row)
		return;

	TSharedPtr<SCustomizedBox> HorizontalBox = CreateCustomizedBox(InParams);
	TSharedPtr<STextBlock> TextBlock = CreateNameTextBlock(InParams);

	if (!HorizontalBox.IsValid())
		return;
	
	if (TextBlock.IsValid())
	{
		HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			TextBlock.ToSharedRef()
		];
	}

	Row->NameWidget.Widget = HorizontalBox.ToSharedRef();
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
			.Font(_GetEditorStyle().GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
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
			.Font(_GetEditorStyle().GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
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
						ColorRampParameter->SyncCachedPoints();
						ColorRampParameter->SetCaching(false);
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
						FloatRampParameter->SyncCachedPoints();
						FloatRampParameter->SetCaching(false);
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
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	if ((MainParam->GetParameterType() != EHoudiniParameterType::FloatRamp) && (MainParam->GetParameterType() != EHoudiniParameterType::ColorRamp))
		CheckBox->SetVisibility(EVisibility::Hidden);

	Row->NameWidget.Widget = HorizontalBox;
}

FDetailWidgetRow*
FHoudiniParameterDetails::CreateNestedRow(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	bool bDecreaseChildCount)
{
	if (InJoinedParams.IsEmpty())
		return nullptr;

	const auto& InParams = InJoinedParams[0];

	if (InParams.Num() <= 0)
		return nullptr;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return nullptr;

	bool bShouldDisplayRow = false;

	// Only display the row if, among all joined params, at least one is visible.
	for (const auto& LinkedParams : InJoinedParams)
	{
		if (LinkedParams.Num() <= 0)
			continue;

		const TWeakObjectPtr<UHoudiniParameter>& Param = LinkedParams[0];
		if (!IsValidWeakPointer(Param))
			continue;

		if (Param->ShouldDisplay())
		{
			bShouldDisplayRow = true;
			break;
		}
	}

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
		if (ParentMultiParm.IsValid() && ParentMultiParm->IsShown() && bShouldDisplayRow)
		{
			if (MainParam->GetParameterType() != EHoudiniParameterType::FolderList)
			{
				FString ParameterRowString = MainParam->GetParameterLabel() + " " + MainParam->GetParameterName();
				Row = &(HouParameterCategory.AddCustomRow(FText::FromString(ParameterRowString)));
			}
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

				// This row should be shown if its parent folder is shown.
				if (ParentFolder)
					bShouldDisplayRow &= (ParentFolder->IsTab() && ParentFolder->IsChosen()) || (!ParentFolder->IsTab() && ParentFolder->IsExpanded());

				if (bShouldDisplayRow)
				{
					if (MainParam->GetParameterType() != EHoudiniParameterType::FolderList)
					{
						FString ParameterRowString = MainParam->GetParameterLabel() + " " + MainParam->GetParameterName();
						Row = &(HouParameterCategory.AddCustomRow(FText::FromString(ParameterRowString)));
					}
				}
			}

			// prune the stack finally
			if (bDecreaseChildCount)
			{
				CurrentLayerFolderQueue[0]->GetChildCounter() -= InJoinedParams.Num();

				if (CurrentLayerFolderQueue[0]->GetChildCounter() < 1)
					PruneStack();
			}
		}
		// If this parameter is in the root dir, just create a row.
		else
		{
			if (bShouldDisplayRow)
			{
				if (MainParam->GetParameterType() != EHoudiniParameterType::FolderList)
				{
					FString ParameterRowString = MainParam->GetParameterLabel() + " " + MainParam->GetParameterName();
					Row = &(HouParameterCategory.AddCustomRow(FText::FromString(ParameterRowString)));
				}

			}
		}
	}

	if (Row)
		CurrentTabEndingRow = nullptr;

	if (Row)
	{
		Row->RowTag(*(MainParam->GetParameterName()));
	}

	return Row;
}

void
FHoudiniParameterDetails::HandleUnsupportedParmType(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

	if (InParams.Num() < 1)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	CreateNestedRow(HouParameterCategory, InJoinedParams);
}

void
FHoudiniParameterDetails::CreateWidgetFloat(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterFloat>> FloatParams;
	if (!CastParameters<UHoudiniParameterFloat>(InParams, FloatParams))
		return;

	if (FloatParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFloat>& MainParam = FloatParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

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

		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
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
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(_GetEditorStyle(), "NoBorder")
					.ClickMethod(EButtonClickMethod::MouseDown)
					.ToolTipText(LOCTEXT("FloatParameterLockButtonToolTip", "When locked, change the vector value uniformly."))
					.Visibility(EVisibility::Visible)
					[
						SNew(SImage)
						.Image(MainParam->IsUniformLocked() ? _GetEditorStyle().GetBrush("Icons.Lock") : _GetEditorStyle().GetBrush("Icons.Unlock"))
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

						FHoudiniEngineUtils::UpdateEditorProperties(true);

						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
					.ButtonStyle(_GetEditorStyle(), "NoBorder")
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
						.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
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
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(NumericEntryBox, SNumericEntryBox< float >)
					.AllowSpin(true)

					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

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
					.ButtonStyle(_GetEditorStyle(), "NoBorder")
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
						.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			];
		}
	}

	LabelledParameter->SetContent(VerticalBox);
}

void
FHoudiniParameterDetails::CreateWidgetInt(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterInt>> IntParams;
	if (!CastParameters<UHoudiniParameterInt>(InParams, IntParams))
		return;

	if (IntParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterInt>& MainParam = IntParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

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

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))

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
				.ButtonStyle(_GetEditorStyle(), "NoBorder")
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
					.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
		/*
		if (NumericEntryBox.IsValid())
			NumericEntryBox->SetEnabled(!MainParam->IsDisabled());
		*/
	}

	LabelledParameter->SetContent(VerticalBox);
}

void
FHoudiniParameterDetails::CreateWidgetString(
	IDetailCategoryBuilder& HouParameterCategory,
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterString>> StringParams;
	if (!CastParameters<UHoudiniParameterString>(InParams, StringParams))
		return;

	if (StringParams.Num() <= 0)
		return;
	
	const TWeakObjectPtr<UHoudiniParameterString>& MainParam = StringParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	bool bIsMultiLine = false;
	bool bIsUnrealRef = false;
	UClass* UnrealRefClass = UObject::StaticClass();

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	TMap<FString, FString>& Tags = MainParam->GetTags();
	if (Tags.Contains(HOUDINI_PARAMETER_STRING_REF_TAG) && FCString::Atoi(*Tags[HOUDINI_PARAMETER_STRING_REF_TAG]) == 1) 
	{
		bIsUnrealRef = true;

		if (Tags.Contains(HOUDINI_PARAMETER_STRING_REF_CLASS_TAG))
		{
			UClass* FoundClass = FHoudiniEngineRuntimeUtils::GetClassByName(Tags[HOUDINI_PARAMETER_STRING_REF_CLASS_TAG]);
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

			FHoudiniEngineUtils::UpdateEditorProperties(false);
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
			TArray<const UClass*> AllowedClasses;
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

			TSharedPtr<SEditableTextBox> EditableTextBox;
			TSharedPtr<SHorizontalBox> HorizontalBox;
			VerticalBox->AddSlot()
			.Padding(2, 2, 5, 2)
			.AutoHeight()
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				AssetData.AssetClassPath = UnrealRefClass->GetClassPathName();
#else
				AssetData.AssetClass = UnrealRefClass->GetFName();
#endif
			}
			
			constexpr int32 ThumbnailSize = 46;

			TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
				new FAssetThumbnail(AssetData, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

			TSharedPtr<SBorder> ThumbnailBorder;
			HorizontalBox->AddSlot()
			.Padding(0, 3, 5, 0)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.Padding(FMargin(0, 0, 4, 4))
				.BorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.Padding(1)
					[
						SAssignNew(ThumbnailBorder, SBorder)
						.Padding(0)
						.OnMouseDoubleClick_Lambda(
							[EditObject, Idx](const FGeometry&, const FPointerEvent&)
							{
								if (EditObject && GEditor) 
									GEditor->EditObject(EditObject);
							
								return FReply::Handled();
							})
						[
							SNew(SBox)
							.WidthOverride(ThumbnailSize)
							.HeightOverride(ThumbnailSize)
							[
								StaticMeshThumbnail->MakeThumbnailWidget()
							]
						]
					]
					+ SOverlay::Slot()
					[
						SNew(SImage)
							.Image(TAttribute<const FSlateBrush*>::Create(
								TAttribute<const FSlateBrush*>::FGetter::CreateLambda(
									[WeakThumbnailBorder = TWeakPtr<SBorder>(ThumbnailBorder)]()
									{
										TSharedPtr<SBorder> ThumbnailBorderPtr = WeakThumbnailBorder.Pin();
										if (ThumbnailBorderPtr.IsValid() && ThumbnailBorderPtr->IsHovered())
											return _GetEditorStyle().GetBrush(
												"PropertyEditor.AssetThumbnailBorderHovered");
										else
											return _GetEditorStyle().GetBrush(
												"PropertyEditor.AssetThumbnailBorder");
									}
								)
							))
							.Visibility(EVisibility::SelfHitTestInvisible)
					]
				]
			];

			FText MeshNameText = FText::GetEmpty();
			if (EditObject)
				MeshNameText = FText::FromString(EditObject->GetName());

			TSharedPtr<SComboButton> StaticMeshComboButton;

			TSharedPtr<SHorizontalBox> ButtonBox;
			HorizontalBox->AddSlot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					SAssignNew(StaticMeshComboButton, SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
						.Text(FText::FromName(AssetData.AssetName))
						.ToolTipText(FText::FromString(MainParam->GetValueAt(Idx)))
					]
				]
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					SAssignNew(ButtonBox, SHorizontalBox)
				]
			];

			// Create tooltip.
			FFormatNamedArguments Args;
			Args.Add(TEXT("Asset"), MeshNameText);
			FText StaticMeshTooltip = FText::Format(
				LOCTEXT(
					"BrowseToSpecificAssetInContentBrowser",
					"Browse to '{Asset}' in the content browser."),
				Args);
			
			// Button : Use selected in content browser
			ButtonBox->AddSlot()
			.AutoWidth()
			.Padding(1, 0, 3, 0)
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeUseSelectedButton(
					FSimpleDelegate::CreateLambda(
						[AllowedClasses, ChangeStringValueAt, Idx, StringParams]()
						{
							if (GEditor)
							{
								TArray<FAssetData> CBSelections;
								GEditor->GetContentBrowserSelections(CBSelections);

								if (CBSelections.IsEmpty())
								{
									return;
								}

								UObject* Object = CBSelections[0].GetAsset();
							
								if (!IsValid(Object))
								{
									return;
								}

								FString ReferenceStr = UHoudiniParameterString::GetAssetReference(Object);

								ChangeStringValueAt(ReferenceStr, Object, Idx, true, StringParams);
							}
						}),
					TAttribute<FText>(LOCTEXT(
						"GeometryInputUseSelectedAssetFromCB",
						"Use the currently selected asset from the content browser.")))
			];

			// Button : Browse Static Mesh
			ButtonBox->AddSlot()
			.AutoWidth()
			.Padding(1, 0, 3, 0)
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(
					FSimpleDelegate::CreateLambda([EditObject]()
					{
						if (GEditor && EditObject)
						{
							TArray<UObject*> Objects;
							Objects.Add(EditObject);
							GEditor->SyncBrowserToObjects(Objects);
						}
					}),
					TAttribute<FText>(StaticMeshTooltip))
			];
			TWeakPtr<SComboButton> WeakStaticMeshComboButton(StaticMeshComboButton);
			StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
				[AllowedClasses, WeakStaticMeshComboButton, ChangeStringValueAt, Idx, StringParams]()
			{
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
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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
						.ButtonStyle(_GetEditorStyle(), "NoBorder")
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
							.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
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
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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
						.ButtonStyle(_GetEditorStyle(), "NoBorder")
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
								.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
						]
					]
				]
			];  
		} 
		
	}

	LabelledParameter->SetContent(VerticalBox);
}

void
FHoudiniParameterDetails::CreateWidgetColor(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterColor>> ColorParams;
	if (!CastParameters<UHoudiniParameterColor>(InParams, ColorParams))
		return;

	if (ColorParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterColor>& MainParam = ColorParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
				PickerArgs.InitialColor = MainParam->GetColorValue(); 
#else
				PickerArgs.InitialColorOverride = MainParam->GetColorValue();
#endif
				PickerArgs.bOnlyRefreshOnOk = true;
				OpenColorPicker(PickerArgs);
				return FReply::Handled();
			})
	];

	LabelledParameter->SetContent(VerticalBox);
}

void
FHoudiniParameterDetails::CreateWidgetButton(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterButton>> ButtonParams;
	if (!CastParameters<UHoudiniParameterButton>(InParams, ButtonParams))
		return;

	if (ButtonParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterButton>& MainParam = ButtonParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

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

	LabelledParameter->SetContent(HorizontalBox);
}

void
FHoudiniParameterDetails::CreateWidgetButtonStrip(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterButtonStrip>> ButtonStripParams;
	if (!CastParameters<UHoudiniParameterButtonStrip>(InParams, ButtonStripParams))
		return;

	if (ButtonStripParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterButtonStrip>& MainParam = ButtonStripParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	auto OnButtonStateChanged = [MainParam, ButtonStripParams](ECheckBoxState NewState, int32 Idx) 
	{
	
		/*
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterButtonStripChange", "Houdini Parameter Button Strip: Changing value"),
			MainParam->GetOuter(), true);
		*/
		bool bChanged = false;

		for (auto & NextParam : ButtonStripParams)
		{
			if (!IsValidWeakPointer(NextParam))
				continue;

			if (NextParam->SetValueAt(NewState == ECheckBoxState::Checked, Idx)) 
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

	for (uint32 Idx = 0; Idx < MainParam->GetNumValues(); ++Idx) 
	{
		const FString* LabelString = MainParam->GetStringLabelAt(Idx);
		FText LabelText = LabelString ? FText::FromString(*LabelString) : FText();

		TSharedPtr<SCheckBox> Button;

		HorizontalBox->AddSlot().Padding(0).FillWidth(1.0f)
		[
			SAssignNew(Button, SCheckBox)
			.Style(_GetEditorStyle(), "Property.ToggleButton.Middle")
			.IsChecked(TAttribute<ECheckBoxState>::CreateLambda(
				[MainParam, Idx]() -> ECheckBoxState
				{
					if (!IsValidWeakPointer(MainParam))
					{
						return ECheckBoxState::Undetermined;
					}

					return MainParam->GetValueAt(Idx)
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				}))
			.OnCheckStateChanged_Lambda([OnButtonStateChanged, Idx](ECheckBoxState NewState)
			{
				OnButtonStateChanged(NewState, Idx);
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LabelText)
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		Button->SetColorAndOpacity(BgColor);
	}

	LabelledParameter->SetContent(HorizontalBox);
}

void
FHoudiniParameterDetails::CreateWidgetLabel(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterLabel>> LabelParams;
	if (!CastParameters<UHoudiniParameterLabel>(InParams, LabelParams))
		return;

	if (LabelParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterLabel>& MainParam = LabelParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	for (int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index)
	{
		FString NextLabelString = MainParam->GetStringAtIndex(Index);
		FText ParameterLabelText = FText::FromString(NextLabelString);

		FText ParamTooltipText = FText::FromString("Column " + FString::FromInt(Index) + ": " + NextLabelString);
		
		TSharedPtr<STextBlock> TextBlock;
		// Add Label UI.
		HorizontalBox->AddSlot()
		.Padding(1, 2, 16, 2)
		.AutoWidth()
		[
			SAssignNew(TextBlock, STextBlock)
			.Text(ParameterLabelText)
			.ToolTipText(ParamTooltipText)
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];
	}

	LabelledParameter->SetContent(HorizontalBox);
}

void
FHoudiniParameterDetails::CreateWidgetToggle(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterToggle>> ToggleParams;
	if (!CastParameters<UHoudiniParameterToggle>(InParams, ToggleParams))
		return;

	if (ToggleParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterToggle>& MainParam = ToggleParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	auto IsToggleCheckedLambda = [MainParam](int32 Index)
	{
		if(!IsValidWeakPointer(MainParam))
			return ECheckBoxState::Unchecked;

		if (Index >= MainParam->GetNumValues())
			return ECheckBoxState::Unchecked;

		if (MainParam->GetValueAt(Index))
			return ECheckBoxState::Checked;

		return ECheckBoxState::Unchecked;
	};

	auto OnToggleCheckStateChanged = [MainParam, ToggleParams](ECheckBoxState NewState, int32 Index) 
	{
		if (!IsValidWeakPointer(MainParam))
			return;

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
			];
	}

	LabelledParameter->SetContent(VerticalBox);
}

void
FHoudiniParameterDetails::CreateWidgetFile(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterFile>> FileParams;
	if (!CastParameters<UHoudiniParameterFile>(InParams, FileParams))
		return;

	if (FileParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterFile>& MainParam = FileParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

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
				.BrowseButtonImage(_GetEditorStyle().GetBrush("PropertyWindow.Button_Ellipsis"))
				.BrowseButtonStyle(_GetEditorStyle(), "HoverHintOnly")
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

	LabelledParameter->SetContent(VerticalBox);
}


void
FHoudiniParameterDetails::CreateWidgetChoice(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<UHoudiniParameterChoice>> ChoiceParams;
	if (!CastParameters<UHoudiniParameterChoice>(InParams, ChoiceParams))
		return;

	if (ChoiceParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterChoice>& MainParam = ChoiceParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

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
			.Font(_GetEditorStyle().GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
	];

	LabelledParameter->SetContent(HorizontalBox);
}

void
FHoudiniParameterDetails::CreateWidgetSeparator(
	const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	if (InParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	TSharedRef<SCustomizedBox> HorizontalBox = SNew(SCustomizedBox);

	HorizontalBox->DividerLinePositions = DividerLinePositions;
	HorizontalBox->SetHoudiniParameter(InParams);

	LabelledParameter->SetContent(HorizontalBox);
}

void
FHoudiniParameterDetails::CreateWidgetOperatorPath(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

	TArray<TWeakObjectPtr<UHoudiniParameterOperatorPath>> OperatorPathParams;
	if (!CastParameters<UHoudiniParameterOperatorPath>(InParams, OperatorPathParams))
		return;

	if (OperatorPathParams.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniParameterOperatorPath>& MainParam = OperatorPathParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = MainParam->HoudiniInput;
	if (!MainInput.IsValid())
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
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InJoinedParams);
	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FHoudiniInputDetails::CreateWidget(HouParameterCategory, EditedInputs, Row);

	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

	OutRows.Add(Row);
}

void
FHoudiniParameterDetails::CreateWidgetFloatRamp(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

	if (InParams.Num() < 1)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	if (MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp) 
	{
		UHoudiniParameterRampFloat* FloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
		if (FloatRampParameter)
		{
			CurrentRampFloat = FloatRampParameter;
			FDetailWidgetRow* Row = CreateWidgetRamp(HouParameterCategory, InJoinedParams);

			OutRows.Add(Row);
		}
	}
}

void
FHoudiniParameterDetails::CreateWidgetColorRamp(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

	if (InParams.Num() < 1)
		return;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return;

	if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		UHoudiniParameterRampColor* RampColor = Cast<UHoudiniParameterRampColor>(MainParam);
		if (RampColor)
		{
			CurrentRampColor = RampColor;
			FDetailWidgetRow* Row = CreateWidgetRamp(HouParameterCategory, InJoinedParams);
			OutRows.Add(Row);
		}
	}

}


FDetailWidgetRow*
FHoudiniParameterDetails::CreateWidgetRamp(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams)
{
	if (InJoinedParams.IsEmpty())
		return nullptr;

	const auto& InParams = InJoinedParams[0];

	if (InParams.Num() <= 0)
		return nullptr;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return nullptr;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InJoinedParams);
	if (!Row)
		return nullptr;

	// Create the standard parameter name widget with an added autoupdate checkbox.
	CreateNameWidgetWithAutoUpdate(Row, InParams, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);	
	if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		UHoudiniParameterRampColor *RampColorParam = Cast<UHoudiniParameterRampColor>(MainParam);
		if (!RampColorParam)
			return nullptr;

		TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> ColorRampParameters;
		CastParameters<UHoudiniParameterRampColor>(InParams, ColorRampParameters);

		Row->ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHoudiniColorRamp)
			.RampParameters(ColorRampParameters)
			.OnValueCommitted_Lambda(
				[]() { FHoudiniEngineUtils::UpdateEditorProperties(true); })
		];
	}
	else if(MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
	{
		UHoudiniParameterRampFloat *RampFloatParam = Cast<UHoudiniParameterRampFloat>(MainParam);
		if (!RampFloatParam)
			return nullptr;

		TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameters;
		CastParameters<UHoudiniParameterRampFloat>(InParams, FloatRampParameters);		
	
		Row->ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHoudiniFloatRamp)
				.RampParameters(FloatRampParameters)
				.OnValueCommitted_Lambda(
					[]() { FHoudiniEngineUtils::UpdateEditorProperties(true); })
		];
	}

	return Row;
}

void 
FHoudiniParameterDetails::CreateWidgetFolderList(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

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
		FDetailWidgetRow* TabRow = CreateNestedRow(HouParameterCategory, InJoinedParams, false);

		OutRows.Add(TabRow);
	}

	// When see a folder list, go depth first search at this step.
	// Push an empty queue to the stack.
	FolderStack.Add(TArray<UHoudiniParameterFolder*>());
}


void
FHoudiniParameterDetails::CreateWidgetFolder(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

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

	// NOTE: Andy: I'm not sure the above comment is correct anymore. However, we do need to special work if processing tabs.
	if (!MainParam->IsVisible() && MainParam->IsTab())
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

			CreateWidgetTabUIElements(HouParameterCategory, MainParam, OutRows);

			PruneStack();

			CurrentFolderList = nullptr;
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
				FDetailWidgetRow* FolderHeaderRow = CreateNestedRow(HouParameterCategory, InJoinedParams, false);
				CreateFolderHeaderUI(HouParameterCategory, FolderHeaderRow, InParams);

				OutRows.Add(FolderHeaderRow);
			}
		}
		// Case 1-2: The folder IS tabs.
		else 
		{
			CreateWidgetTab(HouParameterCategory, MainParam, ParentMultiParm->IsShown(), OutRows);
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
					FDetailWidgetRow* FolderHeaderRow = CreateNestedRow(HouParameterCategory, InJoinedParams, false);
					CreateFolderHeaderUI(HouParameterCategory, FolderHeaderRow, InParams);

					OutRows.Add(FolderHeaderRow);
				}

				MainParam->SetIsContentShown(bExpanded);
				MyFolderQueue.Add(MainParam.Get());
			}
			// Case 2-1-2: The folder IS in a tab menu.
			else 
			{
				bExpanded &= MainParam->IsChosen();

				CreateWidgetTab(HouParameterCategory, MainParam, ParentFolderVisible, OutRows);
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
				FDetailWidgetRow* FolderRow = CreateNestedRow(HouParameterCategory, InJoinedParams, false);
				CreateFolderHeaderUI(HouParameterCategory, FolderRow, InParams);
				OutRows.Add(FolderRow);

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
				CreateWidgetTab(HouParameterCategory, MainParam, true, OutRows); 
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
FHoudiniParameterDetails::CreateFolderHeaderUI(IDetailCategoryBuilder& HouParameterCategory, FDetailWidgetRow* HeaderRow, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams)
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
		.ButtonStyle(_GetEditorStyle(), "NoBorder")
		.ClickMethod(EButtonClickMethod::MouseDown)
		.Visibility(EVisibility::Visible)
		.OnClicked_Lambda([=, &HouParameterCategory]()
		{
			if (!IsValidWeakPointer(MainParam))
				return FReply::Handled();
			
			MainParam->ExpandButtonClicked();
			
			FHoudiniEngineUtils::UpdateEditorProperties(true);

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
		.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
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

				return _GetEditorStyle().GetBrush(ResourceName);
			}
		)
	));

	if(MainParam->GetFolderType() == EHoudiniFolderParameterType::Simple)
		ExpanderArrow->SetEnabled(false);

}

void
FHoudiniParameterDetails::CreateWidgetTabUIElements(
	IDetailCategoryBuilder& HouParameterCategory, 
	const TWeakObjectPtr<UHoudiniParameterFolder>& InFolder,
	TArray<FDetailWidgetRow*>& OutRows)
{
	UHoudiniParameterFolder* const Folder = InFolder.Get();
	TArray<UHoudiniParameterFolder*>& FolderQueue = FolderStack.Last();

	if (CurrentFolderListSize > 1)
		return;

	// Do not draw anything for empty Tabs!
	// This would create extra lines in the Param UI, and adds extra dividers to the following parameters.
	if (CurrentTabs.IsEmpty())
		return;
	
	// The tabs belong to current folder list
	UHoudiniParameterFolderList* CurrentTabMenuFolderList = CurrentFolderList;

	// Create a row (UI) for current tabs
	TSharedPtr<SCustomizedBox> HorizontalBox;
	FDetailWidgetRow& Row = HouParameterCategory.AddCustomRow(FText::GetEmpty())
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

	for (auto& CurTab : CurrentTabs)
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

				FHoudiniEngineUtils::UpdateEditorProperties(true);
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

	OutRows.Add(&Row);
}

void FHoudiniParameterDetails::CreateWidgetTab(
	IDetailCategoryBuilder& HouParameterCategory,
	const TWeakObjectPtr<UHoudiniParameterFolder>& InFolder,
	const bool bIsShown,
	TArray<FDetailWidgetRow*>& OutRows)
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

	CreateWidgetTabUIElements(HouParameterCategory, InFolder, OutRows);
}

void
FHoudiniParameterDetails::CreateWidgetMultiParm(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
	TArray<FDetailWidgetRow*>& OutRows)
{
	if (InJoinedParams.IsEmpty())
		return;

	const auto& InParams = InJoinedParams[0];

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
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InJoinedParams);

	if (!Row)
	{
		MainParam->SetIsShown(false);
		return;
	}

	MainParam->SetIsShown(true);

	MultiParmInstanceIndices.Add(MainParam->GetParmId(), -1);

	CreateNameWidget(Row, InParams, true);

	auto OnInstanceValueChangedLambda = [MainParam](int32 InValue, ETextCommit::Type CommitType)
	{
		if (CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
			return;

		if (InValue < 0)
			return;

		if (MainParam->SetNumElements(InValue))
			MainParam->MarkChanged(true);
	};

	// Add multiparm UI.
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;

	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
			.AllowSpin(true)

		.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateLambda([OnInstanceValueChangedLambda](int32 InValue, ETextCommit::Type CommitType) {
				OnInstanceValueChangedLambda(InValue, CommitType);
		}))
		.Value(MainParam->MultiParmInstanceCount)
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

	OutRows.Add(Row);
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

bool
FHoudiniParameterDetails::ShouldJoinNext(const UHoudiniParameter& InParam)
{
	switch (InParam.GetParameterType())
	{
	// Special case where we do not want to support joining a widget that is joinable in Houdini.
	// For example, input parameter widget is a much larger widget in Unreal than in Houdini.
	case EHoudiniParameterType::Input:
		return false;
	default:
		return InParam.GetJoinNext();
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

bool FHoudiniParameterDetails::IsLabelVisible(
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	if (InParams.Num() <= 0)
		return false;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return false;

	switch (MainParam->GetParameterType())
	{
	case EHoudiniParameterType::Separator:
	case EHoudiniParameterType::Button:
		return false;
	default:
		return MainParam->IsLabelVisible();
	}
}

bool FHoudiniParameterDetails::UsesWholeRow(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	if (InParams.Num() <= 0)
		return false;

	const TWeakObjectPtr<UHoudiniParameter>& MainParam = InParams[0];
	if (!IsValidWeakPointer(MainParam))
		return false;

	switch (MainParam->GetParameterType())
	{
	case EHoudiniParameterType::Separator:
		return true;
	default:
		return false;
	}
}

bool FHoudiniParameterDetails::ShouldWidgetFill(EHoudiniParameterType ParameterType)
{
	switch (ParameterType)
	{
	case EHoudiniParameterType::IntChoice:
	case EHoudiniParameterType::StringChoice:
	case EHoudiniParameterType::Color:
	case EHoudiniParameterType::Button:
	case EHoudiniParameterType::ButtonStrip:
	case EHoudiniParameterType::Label:
	case EHoudiniParameterType::Toggle:
		return false;

	case EHoudiniParameterType::Int:
	case EHoudiniParameterType::Float:
	case EHoudiniParameterType::String:
	case EHoudiniParameterType::Separator:
	case EHoudiniParameterType::File:
	case EHoudiniParameterType::FileDir:
	case EHoudiniParameterType::FileGeo:
	case EHoudiniParameterType::FileImage:
	default:
		return true;
	}
}

void FHoudiniParameterDetails::AddMetaDataToAllDescendants(const TSharedRef<SWidget> AncestorWidget, const FString& UniqueName, uint32& Index)
{
	// Important: We use GetAllChildren and not GetChildren. 
	// Widgets might choose to not expose some of their children via GetChildren.
	FChildren* const Children = AncestorWidget->GetAllChildren();

	if (!Children)
	{
		return;
	}

	for (int32 i = 0; i < Children->Num(); ++i)
	{
		const TSharedRef<SWidget> Child = Children->GetChildAt(i);
		AddMetaDataToAllDescendants(Child, UniqueName, Index);
		Child->AddMetadata(MakeShared<FHoudiniParameterWidgetMetaData>(UniqueName, Index++));
	}
}

void SHoudiniLabelledParameter::Construct(const FArguments& InArgs)
{
	bEnableContentPadding = InArgs._NameContent.Widget != SNullWidget::NullWidget;

	ContentPadding = TAttribute<FMargin>::CreateLambda(
		[this]()
		{
			return bEnableContentPadding
				? DetailWidgetConstants::RightRowPadding
				: FMargin();
		});

	SHorizontalBox::Construct(
		SHorizontalBox::FArguments()
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			InArgs._NameContent.Widget
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(ContentPadding)
		[
			InArgs._Content.Widget
		]);
}

void SHoudiniLabelledParameter::SetContent(TSharedRef<SWidget> InContent)
{
	GetSlot(1)
	[
		InContent
	];
}

void SHoudiniLabelledParameter::SetNameContent(TSharedRef<SWidget> InNameContent)
{
	bEnableContentPadding = InNameContent != SNullWidget::NullWidget;
	GetSlot(0)
	[
		InNameContent
	];
}


#undef LOCTEXT_NAMESPACE
