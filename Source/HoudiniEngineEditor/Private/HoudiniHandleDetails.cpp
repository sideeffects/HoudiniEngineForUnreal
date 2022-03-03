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

#include "HoudiniHandleDetails.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniHandleTranslator.h"
#include "HoudiniHandleComponentVisualizer.h"
#include "HoudiniEngineDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniHandleDetails::CreateWidget(IDetailCategoryBuilder & HouHandleCategory, const TArray<TWeakObjectPtr<UHoudiniHandleComponent>> &InHandles)
{

	if (InHandles.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniHandleComponent>& MainHandle = InHandles[0];

	if (!IsValidWeakPointer(MainHandle))
		return;


	FString HandleNameStr = MainHandle->GetHandleName() + TEXT(" (") + GetHandleTypeString(MainHandle->GetHandleType()) + TEXT(" )");
	FName HandleName = FName(*HandleNameStr);
	IDetailGroup& Group = HouHandleCategory.AddGroup(HandleName, FText::FromString(HandleNameStr), false, false);

	// Create a widget row for this handle
	FDetailWidgetRow& Row = Group.AddWidgetRow();

	CreateNameWidget(Row);

	// Create value widget

	TSharedRef<SVerticalBox> ValueWidgetVerticalBox = SNew(SVerticalBox);

	// Translate
	auto OnLocationChangedLambda = [MainHandle](float Val, int32 Axis)
	{
		if (!IsValidWeakPointer(MainHandle))
			return;

		FVector Location = MainHandle->GetRelativeTransform().GetLocation();

		if (Axis == 0)
			Location.X = Val;
		else if (Axis == 1)
			Location.Y = Val;
		else
			Location.Z = Val;

		MainHandle->SetRelativeLocation(Location);
		FHoudiniHandleTranslator::UpdateTransformParameters(MainHandle.Get());
	};

	auto RevertLocationToDefault = [MainHandle]() 
	{
		if (!IsValidWeakPointer(MainHandle))
			return FReply::Handled();

		FVector DefaultLocation = FVector(0.f, 0.f, 0.f);
		MainHandle->SetRelativeLocation(DefaultLocation);
		FHoudiniHandleTranslator::UpdateTransformParameters(MainHandle.Get());

		return FReply::Handled();
	};

	ValueWidgetVerticalBox->AddSlot().Padding(2.0f, 2.0f, 5.0f, 2.0f).VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.X_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetLocation().X; })
			.Y_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetLocation().Y; })
			.Z_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetLocation().Z; })
			.OnXCommitted_Lambda([OnLocationChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnLocationChangedLambda(Val, 0);
			})
			.OnYCommitted_Lambda([OnLocationChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnLocationChangedLambda(Val, 1);
			})
			.OnZCommitted_Lambda([OnLocationChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnLocationChangedLambda(Val, 2);
			})
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility_Lambda([MainHandle]() 
			{
				if (!IsValidWeakPointer(MainHandle))
					return EVisibility::Hidden;
				
				if (MainHandle->GetRelativeLocation() == FVector::ZeroVector)
					return EVisibility::Hidden;

				return EVisibility::Visible;
			})
			.OnClicked_Lambda(RevertLocationToDefault)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];


	// Rotation 
	auto OnRotationChangedLambda = [MainHandle](float Val, int32 Axis)
	{
		if (!IsValidWeakPointer(MainHandle))
			return;

		FQuat Rotation = MainHandle->GetRelativeTransform().GetRotation();

		if (Axis == 0)
			Rotation.X = Val;
		else if (Axis == 1)
			Rotation.Y = Val;
		else
			Rotation.Z = Val;

		MainHandle->SetRelativeRotation(Rotation);
		FHoudiniHandleTranslator::UpdateTransformParameters(MainHandle.Get());
	};

	auto RevertRotationToDefault = [MainHandle]() 
	{
		if (!IsValidWeakPointer(MainHandle))
			return FReply::Handled();

		MainHandle->SetRelativeRotation(FQuat::Identity);
		FHoudiniHandleTranslator::UpdateTransformParameters(MainHandle.Get());

		return FReply::Handled();
	};

	ValueWidgetVerticalBox->AddSlot().Padding(2.0f, 2.0f, 5.0f, 2.0f).VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.X_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetRotation().X; })
			.Y_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetRotation().Y; })
			.Z_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetRotation().Z; })
			.OnXCommitted_Lambda([OnRotationChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnRotationChangedLambda(Val, 0);
			})
			.OnXCommitted_Lambda([OnRotationChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnRotationChangedLambda(Val, 1);
			})
			.OnXCommitted_Lambda([OnRotationChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnRotationChangedLambda(Val, 2);
			})
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility_Lambda([MainHandle]() 
			{
				if (!IsValidWeakPointer(MainHandle))
					return EVisibility::Hidden;

				if (MainHandle->GetRelativeTransform().GetRotation() == FQuat::Identity)
					return EVisibility::Hidden;

				return EVisibility::Visible;
			})
			.OnClicked_Lambda(RevertRotationToDefault)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];


	// Scale 
	auto OnScaleChangedLambda = [MainHandle](float Val, int32 Axis)
	{
		if (!IsValidWeakPointer(MainHandle))
			return;

		FVector Scale = MainHandle->GetRelativeTransform().GetScale3D();

		if (Axis == 0)
			Scale.X = Val;
		else if (Axis == 1)
			Scale.Y = Val;
		else
			Scale.Z = Val;

		MainHandle->SetRelativeScale3D(Scale);
		FHoudiniHandleTranslator::UpdateTransformParameters(MainHandle.Get());
	};

	auto RevertScaleToDefault = [MainHandle]() 
	{
		if (!IsValidWeakPointer(MainHandle))
			return FReply::Handled();

		MainHandle->SetRelativeScale3D(FVector::OneVector);

		return FReply::Handled();
	};

	ValueWidgetVerticalBox->AddSlot().Padding(2.0f, 2.0f, 5.0f, 2.0f).VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SVectorInputBox)
			.bColorAxisLabels(true)
			.X_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetScale3D().X; })
			.Y_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetScale3D().Y; })
			.Z_Lambda([MainHandle]() {return MainHandle->GetRelativeTransform().GetScale3D().Z; })
			.OnXCommitted_Lambda([OnScaleChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnScaleChangedLambda(Val, 0);
			})
			.OnXCommitted_Lambda([OnScaleChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnScaleChangedLambda(Val, 1);
			})
			.OnXCommitted_Lambda([OnScaleChangedLambda](float Val, ETextCommit::Type TextCommitType)
			{
				OnScaleChangedLambda(Val, 2);
			})
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility_Lambda([MainHandle]() 
			{
				if (!IsValidWeakPointer(MainHandle))
					return EVisibility::Hidden;

				if (MainHandle->GetRelativeTransform().GetScale3D() == FVector::OneVector)
					return EVisibility::Hidden;

				return EVisibility::Visible;
			})
			.OnClicked_Lambda(RevertScaleToDefault)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];
	

	Row.ValueWidget.Widget = ValueWidgetVerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);


	auto OnMouseEnterLambda = [MainHandle](const FGeometry& Geometry, const FPointerEvent& Event)
	{
		if (!IsValidWeakPointer(MainHandle))
			return;

		TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(UHoudiniHandleComponent::StaticClass()->GetFName());
		TSharedPtr<FHoudiniHandleComponentVisualizer> HandleVisualizer = StaticCastSharedPtr<FHoudiniHandleComponentVisualizer>(Visualizer);

		if (HandleVisualizer.IsValid())
		{
			HandleVisualizer->SetEditedComponent(MainHandle.Get());
		}
	};

	auto OnMouseLeaveLambda = [MainHandle](const FPointerEvent& Event)
	{
		if (!IsValidWeakPointer(MainHandle))
			return;

		TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(UHoudiniHandleComponent::StaticClass()->GetFName());
		TSharedPtr<FHoudiniHandleComponentVisualizer> HandleVisualizer = StaticCastSharedPtr<FHoudiniHandleComponentVisualizer>(Visualizer);

		if (HandleVisualizer.IsValid())
		{
			HandleVisualizer->SetEditedComponent(nullptr);
		}
	};

	// Set on mouse leave UI widget callback functions
	Row.NameWidget.Widget->SetOnMouseEnter(FNoReplyPointerEventHandler::CreateLambda(OnMouseEnterLambda));
	Row.ValueWidget.Widget->SetOnMouseEnter(FNoReplyPointerEventHandler::CreateLambda(OnMouseEnterLambda));

	Row.NameWidget.Widget->SetOnMouseLeave(FSimpleNoReplyPointerEventHandler::CreateLambda(OnMouseLeaveLambda));
	Row.ValueWidget.Widget->SetOnMouseLeave(FSimpleNoReplyPointerEventHandler::CreateLambda(OnMouseLeaveLambda));
}

void 
FHoudiniHandleDetails::CreateNameWidget(FDetailWidgetRow& Row) 
{
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	VerticalBox->AddSlot().AutoHeight().Padding(2.0f, 5.0f, 5.0f, 5.0f).VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(FText::FromString("Translate"))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	VerticalBox->AddSlot().AutoHeight().Padding(2.0f, 5.0f, 5.0f, 5.0f).VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(FText::FromString("Rotation"))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	VerticalBox->AddSlot().AutoHeight().Padding(2.0f, 5.0f, 5.0f, 5.0f).VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(FText::FromString("Scale"))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];


	Row.NameWidget.Widget = VerticalBox;
}

FString 
FHoudiniHandleDetails::GetHandleTypeString(const EHoudiniHandleType& HandleType) 
{
	switch (HandleType) 
	{
	case EHoudiniHandleType::Bounder:
		return FString("Bounder");
	case EHoudiniHandleType::Xform:
		return FString("Xform");
	case EHoudiniHandleType::Unsupported:
		return FString("Unsupported");

	default:
		break;
	}

	return FString("");
}

#undef LOCTEXT_NAMESPACE