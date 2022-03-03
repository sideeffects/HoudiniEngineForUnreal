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

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "DetailWidgetRow.h"

#include "HoudiniPDGAssetLink.h"
#include "HoudiniEngineDetails.h"

class IDetailGroup;
class IDetailCategoryBuilder;

struct FWorkItemTally;
enum class EPDGLinkState : uint8;
enum class EHoudiniBGEOCommandletStatus : uint8;

// Convenience struct to hold a label and tooltip for widgets.
struct FTextAndTooltip
{
public:
	FTextAndTooltip(int32 InValue, const FString& InText);
	FTextAndTooltip(int32 InValue, const FString& InText, const FString &InTooltip);
	FTextAndTooltip(int32 InValue, FString&& InText);
	FTextAndTooltip(int32 InValue, FString&& InText, FString&& InTooltip);
	
	FString Text;
	
	FString ToolTip;

	int32 Value;
};

class FHoudiniPDGDetails : public TSharedFromThis<FHoudiniPDGDetails, ESPMode::NotThreadSafe>
{
	public:

		void CreateWidget(
			IDetailCategoryBuilder & HouPDGCategory,
			const TWeakObjectPtr<UHoudiniPDGAssetLink>&& InPDGAssetLink);
			//UHoudiniAssetComponent* InHAC);

		void AddPDGAssetWidget(
			IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink);

		void AddWorkItemStatusWidget(
			FDetailWidgetRow& InRow, const FString& TitleString, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InAssetLink, bool bInForSelectedNode);

		void AddPDGAssetStatus(
			IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink);

		void AddPDGCommandletStatus(
			IDetailCategoryBuilder& InPDGCategory, const EHoudiniBGEOCommandletStatus& InCommandletStatus);

		void AddTOPNetworkWidget(
			IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink);

		void AddTOPNodeWidget(
			IDetailGroup& InGroup, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink);

		static void RefreshPDGAssetLink(
			const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink);

		static void RefreshUI(
			const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, const bool& InFullUpdate = true);

		static void 
			CreatePDGBakeWidgets(IDetailCategoryBuilder& InPDGCategory, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink);
	protected:
		// Helper function for getting the work item tally and color
		static bool GetWorkItemTallyValueAndColor(
			const TWeakObjectPtr<UHoudiniPDGAssetLink>& InAssetLink, bool bInForSelectedNode, const FString& InTallyItemString,
			int32& OutValue, FLinearColor& OutColor);

		// Helper to get the status text for the selected TOP node, and the color with which to display it on the UI.
		// Returns false if the InPDGAssetLink is invalid, or there is no selected TOP node.
		static bool GetSelectedTOPNodeStatusAndColor(
			const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, FString& OutTOPNodeStatus, FLinearColor &OutTOPNodeStatusColor);

		// Helper to get asset link status and status color for UI
		static bool GetPDGStatusAndColor(
			const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink, FString& OutPDGStatusString, FLinearColor& OutPDGStatusColor);

		// Helper for getting the commandlet status text and color for the UI
		static void GetPDGCommandletStatus(FString& OutStatusString, FLinearColor& OutStatusColor);

		// Helper to check if the asset link state is Linked
		static FORCEINLINE bool IsPDGLinked(const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
		{
			return IsValidWeakPointer(InPDGAssetLink) && InPDGAssetLink->LinkState == EPDGLinkState::Linked;
		}

		// Helper for binding IsPDGLinked to a TAttribute<bool>
		static FORCEINLINE void BindDisableIfPDGNotLinked(TAttribute<bool> &InAttrToBind, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
		{
			InAttrToBind.Bind(
				TAttribute<bool>::FGetter::CreateLambda([InPDGAssetLink]()
				{
					return IsPDGLinked(InPDGAssetLink);
				})
			);
		}

		// Helper to disable a UI row if InPDGAssetLink is not linked
		static FORCEINLINE void DisableIfPDGNotLinked(FDetailWidgetRow& InRow, const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink)
		{
			BindDisableIfPDGNotLinked(InRow.IsEnabledAttr, InPDGAssetLink);
		}
	
	private:

		TArray<TSharedPtr<FTextAndTooltip>> TOPNetworksPtr;

		TArray<TSharedPtr<FTextAndTooltip>> TOPNodesPtr;

};
