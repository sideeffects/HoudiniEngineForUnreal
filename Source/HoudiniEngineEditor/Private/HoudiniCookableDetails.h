/*
* Copyright (c) <2025> Side Effects Software Inc.
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

#include "HoudiniEngineDetails.h"
#include "HoudiniOutputDetails.h"
#include "HoudiniPDGDetails.h"
#include "HoudiniParameterDetails.h"
#include "HoudiniRuntimeSettings.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "IDetailCustomization.h"


class UHoudiniCookable;
class UHoudiniAssetComponent;
class UStaticMesh;

class FHoudiniCookableDetails : public IDetailCustomization
{
public:

	// Constructor.
	FHoudiniCookableDetails();

	// IDetailCustomization methods.
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	// Create an instance of this detail layout class.
	static TSharedRef<IDetailCustomization> MakeInstance();

	void CreateHoudiniEngineDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString(),
		const EHoudiniDetailsFlags& DetailsFlags = EHoudiniDetailsFlags::Defaults);

	void CreateHoudiniAssetDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	void CreateNodeSyncDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString());

	void CreatePDGDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString());

	void CreateParameterDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString());

	void CreateHandleDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString());

	void CreateInputDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString());

	void CreateOutputDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		const FString& MultiSelectionIdentifier = FString());

	void CreateProxyDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	void CreateMeshBuildSettingsDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	void CreateMeshGenerationDetails(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	void CreateMeshConversonSettings(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

private:

	// Handler for double clicking the static mesh thumbnail, opens the editor.
	FReply OnThumbnailDoubleClick(
		const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent, UObject* Object);

	// Structure holding the output's details
	TSharedPtr<FHoudiniOutputDetails, ESPMode::NotThreadSafe> OutputDetails;

	// Structure holding the parameter's details
	TSharedPtr<FHoudiniParameterDetails, ESPMode::NotThreadSafe> ParameterDetails;

	// Structure holding the PDG Asset Link's details
	TSharedPtr<FHoudiniPDGDetails, ESPMode::NotThreadSafe> PDGDetails;

	// Structure holding the HoudiniAsset details
	TSharedPtr<FHoudiniEngineDetails, ESPMode::NotThreadSafe> HoudiniEngineDetails;

	TArray<TSharedPtr<FString>> CollisionTraceFlagsAsString;
};
