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

#include "HoudiniHandleComponentVisualizer.h"

#include "EditorViewportClient.h"

#include "HoudiniHandleTranslator.h"
#include "HoudiniAssetComponent.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

IMPLEMENT_HIT_PROXY(HHoudiniHandleVisProxy, HComponentVisProxy);

HHoudiniHandleVisProxy::HHoudiniHandleVisProxy(const UActorComponent * InComponent)
	: HComponentVisProxy(InComponent, HPP_Wireframe)
{}


FHoudiniHandleComponentVisualizerCommands::FHoudiniHandleComponentVisualizerCommands()
	: TCommands< FHoudiniHandleComponentVisualizerCommands >(
		"HoudiniHandleComponentVisualizer",
		FText::FromString("Houdini handle Component Visualizer"),
		NAME_None,
		FEditorStyle::GetStyleSetName())
{}

void
FHoudiniHandleComponentVisualizerCommands::RegisterCommands()
{}


FHoudiniHandleComponentVisualizer::FHoudiniHandleComponentVisualizer()
	: FComponentVisualizer()
	, EditedComponent(nullptr)
	, bEditing(false)
{
	FHoudiniHandleComponentVisualizerCommands::Register();
	VisualizerActions = MakeShareable(new FUICommandList);
}

FHoudiniHandleComponentVisualizer::~FHoudiniHandleComponentVisualizer()
{
	FHoudiniHandleComponentVisualizerCommands::Unregister();
}

void 
FHoudiniHandleComponentVisualizer::DrawVisualization(const UActorComponent * Component,
									const FSceneView * View, FPrimitiveDrawInterface * PDI) 
{
	const UHoudiniHandleComponent* HandleComponent = Cast<const UHoudiniHandleComponent>(Component);

	if (!HandleComponent)
		return;

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(HandleComponent->GetOuter());

	if (!HAC)
		return;


	static TMap<int32, const FLinearColor> ColorMapActive;
	static TMap<int32, const FLinearColor> ColorMapInactive;


	int32 AssetId = HAC->GetAssetId();

	if (!ColorMapActive.Contains(AssetId) || !ColorMapInactive.Contains(AssetId)) 
	{
		FLinearColor NewActiveColor = FLinearColor::MakeRandomColor();
		FLinearColor NewInactiveColor = NewActiveColor.CopyWithNewOpacity(0.1)/2.5f;

		ColorMapActive.Add(AssetId, NewActiveColor);
		ColorMapInactive.Add(AssetId, NewInactiveColor);
	}

	
	const FLinearColor& ActiveColor = ColorMapActive[AssetId];
	const FLinearColor& InactiveColor = ColorMapInactive[AssetId];

	bool IsActive = EditedComponent != nullptr;

	if (IsActive)
	{
		UHoudiniAssetComponent* EditedComponentParent = Cast<UHoudiniAssetComponent>(EditedComponent->GetOuter());
		IsActive &= EditedComponentParent && EditedComponentParent->GetAssetId() == HAC->GetAssetId();
	}

	// Draw point and set hit box for it.
	PDI->SetHitProxy(new HHoudiniHandleVisProxy(HandleComponent));
	{
		static const float GrabHandleSizeActive = 24.0f;
		static const float GrabHandleSizeInactive = 18.0f;

		PDI->DrawPoint(HandleComponent->GetComponentTransform().GetLocation(), IsActive ? ActiveColor : InactiveColor, IsActive ? GrabHandleSizeActive : GrabHandleSizeInactive, SDPG_Foreground);
	}

	if (HandleComponent->HandleType == EHoudiniHandleType::Bounder)
	{
		// draw the scale box
		FTransform BoxTransform = HandleComponent->GetComponentTransform();
		const float BoxRad = 50.f;
		const FBox Box(FVector(-BoxRad, -BoxRad, -BoxRad), FVector(BoxRad, BoxRad, BoxRad));
		DrawWireBox(PDI, BoxTransform.ToMatrixWithScale(), Box, IsActive ? ActiveColor : InactiveColor, SDPG_Foreground);
	}

	PDI->SetHitProxy(nullptr);
}

bool
FHoudiniHandleComponentVisualizer::VisProxyHandleClick(
	FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bEditing = false;

	bAllowTranslate = false;
	bAllowRotation = false;
	bAllowScale = false;
	

	if (VisProxy && VisProxy->Component.IsValid())
	{
		const UHoudiniHandleComponent * Component =
			CastChecked< const UHoudiniHandleComponent >(VisProxy->Component.Get());

		const TArray<UHoudiniHandleParameter*> &XformParms = Component->XformParms;

		if (!Component->CheckHandleValid())
			return bEditing;

		EditedComponent = const_cast<UHoudiniHandleComponent *>(Component);

		if (Component)
		{
			if (VisProxy->IsA(HHoudiniHandleVisProxy::StaticGetType()))
				bEditing = true;

			bAllowTranslate =
				XformParms[int32(EXformParameter::TX)]->AssetParameter ||
				XformParms[int32(EXformParameter::TY)]->AssetParameter ||
				XformParms[int32(EXformParameter::TZ)]->AssetParameter;

			bAllowRotation =
				XformParms[int32(EXformParameter::RX)]->AssetParameter ||
				XformParms[int32(EXformParameter::RY)]->AssetParameter ||
				XformParms[int32(EXformParameter::RZ)]->AssetParameter;

			bAllowScale =
				XformParms[int32(EXformParameter::SX)]->AssetParameter ||
				XformParms[int32(EXformParameter::SY)]->AssetParameter ||
				XformParms[int32(EXformParameter::SZ)]->AssetParameter;
		}
	}

	return bEditing;
}

void 
FHoudiniHandleComponentVisualizer::EndEditing() 
{
	EditedComponent = nullptr;
}

bool
FHoudiniHandleComponentVisualizer::GetWidgetLocation(
	const FEditorViewportClient * ViewportClient,
	FVector & OutLocation) const
{
	if (EditedComponent)
	{
		OutLocation = EditedComponent->GetComponentTransform().GetLocation();
		return true;
	}

	return false;
}

bool
FHoudiniHandleComponentVisualizer::GetCustomInputCoordinateSystem(
	const FEditorViewportClient * ViewportClient,
	FMatrix & OutMatrix) const
{
	if (EditedComponent && ViewportClient->GetWidgetMode() == UE::Widget::EWidgetMode::WM_Scale)
	{
		OutMatrix = FRotationMatrix::Make(EditedComponent->GetComponentTransform().GetRotation());
		return true;
	}
	else
	{
		return false;
	}
}

bool
FHoudiniHandleComponentVisualizer::HandleInputDelta(
	FEditorViewportClient * ViewportClient, FViewport * Viewport,
	FVector& DeltaTranslate, FRotator & DeltaRotate, FVector & DeltaScale)
{
	if (!EditedComponent)
		return false;

	if (!DeltaTranslate.IsZero() && bAllowTranslate)
	{
		EditedComponent->SetRelativeLocation(EditedComponent->GetRelativeTransform().GetLocation() + DeltaTranslate);
	}

	if (!DeltaRotate.IsZero() && bAllowRotation)
	{
		EditedComponent->SetRelativeRotation(DeltaRotate.Quaternion() * EditedComponent->GetRelativeTransform().GetRotation());
	}

	if (!DeltaScale.IsZero() && bAllowScale)
	{
		EditedComponent->SetRelativeScale3D(EditedComponent->GetRelativeTransform().GetScale3D() + DeltaScale);
	}

	return true;
}

bool
FHoudiniHandleComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (EditedComponent)
	{
		if (Key == EKeys::LeftMouseButton && Event == IE_Released)
		{
			if (GEditor)
				GEditor->RedrawLevelEditingViewports(true);

			FHoudiniHandleTranslator::UpdateTransformParameters(EditedComponent);
		}

	}
	return false;
}

#undef LOCTEXT_NAMESPACE