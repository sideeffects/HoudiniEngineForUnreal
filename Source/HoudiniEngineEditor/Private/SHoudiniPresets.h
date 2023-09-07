/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#pragma once

#include "CoreMinimal.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniPreset.h"

class SHoudiniCreatePresetFromHDA : public SCompoundWidget //, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SHoudiniCreatePresetFromHDA )
		: _HoudiniAssetComponent(nullptr)
	{}
	SLATE_ATTRIBUTE(TWeakObjectPtr<UHoudiniAssetComponent>, HoudiniAssetComponent)
	SLATE_END_ARGS();

	SHoudiniCreatePresetFromHDA();

	// Create a dialog to create a preset from the given HoudiniAssetComponent
	static void CreateDialog(TWeakObjectPtr<UHoudiniAssetComponent> HAC);
	
	void Construct( const FArguments& InArgs );
	
protected:
	static TSharedRef<SWindow> CreateFloatingWindow(
		const FText& WindowTitle,
		const FVector2D InitialSize=FVector2D(400, 550)
		);

	void OnLabelColumnResized(float NewSize)
	{
		const float TotalSize = LabelColumnWidth + NameColumnWidth;
		LabelColumnWidth = NewSize;
		// NameColumnWidth = TotalSize - LabelColumnWidth; // New width of the name column 
	}

	void OnNameColumnResized(float NewSize)
	{
		NameColumnWidth = NewSize;
	}
	
	void OnValueColumnResized(float NewSize)
	{
		ValueColumnWidth = NewSize;
		// NameColumnWidth = 1.0f - NewSize;
	}

	float GetLabelColumnWidth() const
	{
		return LabelColumnWidth;
	}

	float GetNameColumnWidth() const
	{
		return NameColumnWidth;
	}

	float GetValueColumnWidth() const
	{
		return ValueColumnWidth;
		// return 1.0f - NameColumnWidth - LabelColumnWidth;
	}

	ECheckBoxState GetParameterCheckState(const FString& ParamName) const;
	void OnParameterCheckStateChanged(ECheckBoxState CheckState, const FString& ParamName);

	ECheckBoxState GetInputCheckState(const int32 InputIndex) const;
	void OnInputCheckStateChanged(ECheckBoxState CheckState, const int32 InputIndex);

	void OnPresetNameChanged(const FText& NewText) { PresetName = NewText.ToString(); bIsValidPresetPath = IsValidPresetPath(); }
	void OnPresetNameCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { PresetName = NewText.ToString(); bIsValidPresetPath = IsValidPresetPath(); }
	
	void OnPresetLabelChanged(const FText& NewText) { PresetLabel = NewText.ToString(); }
	void OnPresetLabelCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { PresetLabel = NewText.ToString(); }

	void OnPresetDescriptionChanged(const FText& NewText) { PresetDescription = NewText.ToString(); }
	void OnPresetDescriptionCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { PresetDescription = NewText.ToString(); }

	ECheckBoxState GetApplyOnlyToSourceCheckState() const { return bApplyOnlyToSource ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnApplyOnlyToSourceCheckStateChanged(ECheckBoxState NewState) { bApplyOnlyToSource = NewState == ECheckBoxState::Checked; }

	ECheckBoxState GetCanBeInstantiatedCheckState() const { return bCanInstantiate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnCanBeInstantiatedCheckStateChanged(ECheckBoxState NewState) { bCanInstantiate = NewState == ECheckBoxState::Checked; }

	ECheckBoxState GetRevertHDAParametersCheckState() const { return bRevertHDAParameters ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnRevertHDAParametersCheckStateChanged(ECheckBoxState NewState) { bRevertHDAParameters = NewState == ECheckBoxState::Checked; }

	FString GetSourceAssetPath() const;
	void OnPresetAssetChanged(const FAssetData& AssetData);

	FString GetPresetAssetBasePath() const;
	FString GetPresetAssetPath() const;
	FText GetPresetAssetPathText() const { return FText::FromString(GetPresetAssetPath()); }
	bool IsValidPresetPath();

	EVisibility GetErrorVisibility() const;
	FText GetErrorText() const;

	void SelectAllParameters();

	FReply HandleCreatePresetClicked();
	FReply HandleCancelClicked();

	void CloseWindow();


	// Houdini Asset components from which we'll be creating a preset 
	TWeakObjectPtr<UHoudiniAssetComponent> HoudiniAssetComponent;

	FString PresetName;
	FString PresetLabel;
	FString PresetDescription;
	TWeakObjectPtr<UHoudiniAsset> SourceHoudiniAsset;

	bool bIsValidPresetPath;

	bool bApplyOnlyToSource;
	bool bRevertHDAParameters;
	bool bCanInstantiate;

	bool bApplyTempCookFolder;
	bool bApplyBakeFolder;
	bool bApplyBakeOptions;
	bool bApplyAssetOptions;
	bool bApplyStaticMeshGenSettings;
	bool bApplyProxyMeshGenSettings;

	FText PresetNameError;

	float LabelColumnWidth;
	float NameColumnWidth;
	float ValueColumnWidth;

	// Parameters and inputs that should be kept for this preset
	bool bSelectAll;
	TSet<FString> KeepParameters;
	TSet<int> KeepInputs;
	
	TMap<FString, FHoudiniPresetFloatValues> FloatValues;
	TMap<FString, FHoudiniPresetIntValues> IntValues;
	TMap<FString, FHoudiniPresetStringValues> StringValues;
	TMap<FString, FHoudiniPresetRampFloatValues> RampFloatValues;
	TMap<FString, FHoudiniPresetRampColorValues> RampColorValues;
	TArray<FHoudiniPresetInputValue> InputValues;
};

	