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

#include "SSelectHoudiniPathDialog.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEnginePrivatePCH.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineUtils.h"

// dlg?
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/ITypedTableView.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Visibility.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STableRow.h"

class FUICommandList;
class ITableRow;
class SWidget;
class UClass;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "HoudiniNodeTreeview"


SSelectHoudiniPathDialog::SSelectHoudiniPathDialog()
	: UserResponse(EAppReturnType::Cancel),
	bSingleSelectionOnly(false)
{

}

void
SSelectHoudiniPathDialog::Construct(const FArguments& InArgs)
{
	FolderPath = InArgs._InitialPath;
	if (FolderPath.IsEmpty())
	{
		FolderPath = FText::FromString(TEXT("/Game"));
	}

	bSingleSelectionOnly = InArgs._SingleSelection;

	// Split the initial folder path to multiple strings if needed
	// This will allow us to re-select previously selected nodes.
	FString FolderPathStr = FolderPath.ToString();
	FolderPathStr.ParseIntoArray(SplitFolderPath, TEXT(";"), true);

	// If we're in single selection mode - only keep the first node path!
	if (bSingleSelectionOnly && SplitFolderPath.Num() > 1)
	{
		FolderPath = FText::FromString(SplitFolderPath[0]);
		SplitFolderPath.Empty();
		SplitFolderPath.Add(FolderPath.ToString());
	}
	
	auto IsSessionValid = []()
	{
		return (HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(FHoudiniEngine::Get().GetSession()));
	};

	auto IsTreeViewVisible = [IsSessionValid]()
	{
		if (IsSessionValid())
			return EVisibility::All;
		else
			return EVisibility::Hidden;
	};

	// Create the Network info and fill all the node hierarchy for it
	FillHoudiniNetworkInfo();

	//Create the treeview
	HoudiniNodeTreeView = SNew(SHoudiniNodeTreeView)
		.HoudiniNetworkInfo(MakeShared<FHoudiniNetworkInfo>(NetworkInfo))
		.SingleSelection(bSingleSelectionOnly);
	
	SWindow::Construct(SWindow::FArguments()
	.Title(InArgs._TitleText)
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	.IsTopmostWindow(true)
	.ClientSize(FVector2D(450, 450))
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(_GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([IsSessionValid, this]()
					{
							if (IsSessionValid())
							{
								if(bSingleSelectionOnly)
									return LOCTEXT("SelectPath", "Select a Houdini Node...");
								else
									return LOCTEXT("SelectPath", "Select Houdini Nodes...");
							}
							else
								return LOCTEXT("SelectPathInvalid", "\nNo valid Houdini Engine session...\n");
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(2.0f)
					.Visibility_Lambda(IsTreeViewVisible)
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SCheckBox)
							.HAlign(HAlign_Center)
							.OnCheckStateChanged(HoudiniNodeTreeView.Get(), &SHoudiniNodeTreeView::OnToggleSelectAll)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f, 3.0f, 6.0f, 3.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectHoudiniNodePath_SelectAll", "All"))
						]
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("SelectHoudiniNodePath_ExpandAll", "Expand All"))
						.OnClicked(HoudiniNodeTreeView.Get(), &SHoudiniNodeTreeView::OnExpandAll)
					]
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("SelectHoudiniNodePath_CollapseAll", "Collapse All"))
						.OnClicked(HoudiniNodeTreeView.Get(), &SHoudiniNodeTreeView::OnCollapseAll)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBox)
					[
						HoudiniNodeTreeView.ToSharedRef()
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(5)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(_GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(_GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(_GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(_GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("OK", "OK"))
				.OnClicked(this, &SSelectHoudiniPathDialog::OnButtonClick, IsSessionValid() ? EAppReturnType::Ok : EAppReturnType::Cancel)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(_GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SSelectHoudiniPathDialog::OnButtonClick, EAppReturnType::Cancel)
			]
		]
	]);
}

EAppReturnType::Type
SSelectHoudiniPathDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

const FText&
SSelectHoudiniPathDialog::GetFolderPath() const
{
	return FolderPath;
}


void
SSelectHoudiniPathDialog::FillHoudiniNodeInfo(FHoudiniNodeInfoPtr InNodeInfo)
{
	if (!InNodeInfo.IsValid())
		return;

	if (InNodeInfo->NodeId < 0)
		return;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeInfo->NodeId, &NodeInfo))
	{
		// Invalid node ?
		return;
	}

	// Node Name
	InNodeInfo->NodeName = FHoudiniEngineUtils::HapiGetString(NodeInfo.nameSH);

	// Node Path
	FHoudiniEngineUtils::HapiGetAbsNodePath(InNodeInfo->NodeId, InNodeInfo->NodeHierarchyPath);

	// Do we need to initialise import?
	// This is not necessary if import was set to true by our parent
	if (!InNodeInfo->bImportNode)
	{
		// See if our path is in the current split folder path
		if (!InNodeInfo->NodeHierarchyPath.IsEmpty() && SplitFolderPath.Contains(InNodeInfo->NodeHierarchyPath))
			InNodeInfo->bImportNode = true;
	}

	bool bLookForChildrens = false;
	switch (NodeInfo.type)
	{
	case HAPI_NODETYPE_ANY:
	case HAPI_NODETYPE_NONE:
		InNodeInfo->NodeType = TEXT("INVALID");
		return;
		break;

	case HAPI_NODETYPE_OBJ:
		InNodeInfo->NodeType = TEXT("OBJ");
		bLookForChildrens = true;
		break;

	case HAPI_NODETYPE_SOP:
		InNodeInfo->NodeType = TEXT("SOP");
		bLookForChildrens = true;
		break;

	case HAPI_NODETYPE_CHOP:
		InNodeInfo->NodeType = TEXT("CHOP");
		break;

	case HAPI_NODETYPE_ROP:
		InNodeInfo->NodeType = TEXT("ROP");
		break;

	case HAPI_NODETYPE_SHOP:
		InNodeInfo->NodeType = TEXT("SHOP");
		break;

	case HAPI_NODETYPE_COP2:
		InNodeInfo->NodeType = TEXT("COP2");
		bLookForChildrens = true;
		break;

	case HAPI_NODETYPE_VOP:
		InNodeInfo->NodeType = TEXT("VOP");
		break;

	case HAPI_NODETYPE_DOP:
		InNodeInfo->NodeType = TEXT("DOP");
		break;

	case HAPI_NODETYPE_TOP:
		InNodeInfo->NodeType = TEXT("TOP");
		bLookForChildrens = true;
		break;

	case HAPI_NODETYPE_COP:
		InNodeInfo->NodeType = TEXT("COP");
		bLookForChildrens = true;
		break;
	}

	// Should we look at this node's children ?
	InNodeInfo->Childrens.SetNum(0);
	if (!bLookForChildrens)
		return;

	// See if this node has children ?
	int32 ChildrenCount = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(), InNodeInfo->NodeId, HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_NON_BYPASS, false, &ChildrenCount))
	{
		// ?
		return;
	}

	if (ChildrenCount <= 0)
		return;

	TArray<HAPI_NodeId> ChildrenNodeIds;
	ChildrenNodeIds.SetNumUninitialized(ChildrenCount);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetComposedChildNodeList(
		FHoudiniEngine::Get().GetSession(), InNodeInfo->NodeId, ChildrenNodeIds.GetData(), ChildrenCount))
	{
		return;
	}

	// Initialize the node hierarchy
	InNodeInfo->Childrens.SetNum(ChildrenCount);

	for (int32 Idx = 0; Idx < ChildrenCount; Idx++)
	{
		InNodeInfo->Childrens[Idx] = MakeShared<FHoudiniNodeInfo>();
		InNodeInfo->Childrens[Idx]->NodeId = ChildrenNodeIds[Idx];
		InNodeInfo->Childrens[Idx]->bIsRootNode = false;
		InNodeInfo->Childrens[Idx]->Parent = InNodeInfo;

		// Initialize our children's import value to us
		// They should be marked as imported if we are
		InNodeInfo->Childrens[Idx]->bImportNode = InNodeInfo->bImportNode;

		FillHoudiniNodeInfo(InNodeInfo->Childrens[Idx]);
	}
}

void 
SSelectHoudiniPathDialog::FillHoudiniNetworkInfo()
{
	// Start everything from /obj
	FString RootNodePath = "/obj";
	HAPI_NodeId RootNodeId = -1;
	HAPI_Result result = FHoudiniApi::GetNodeFromPath(
		FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*RootNodePath), &RootNodeId);

	HAPI_NodeInfo RootNodeInfo;
	FHoudiniApi::NodeInfo_Init(&RootNodeInfo);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), RootNodeId, &RootNodeInfo))
	{
		// No root ?
		return;
	}

	// See if this OBJ has children ?
	int32 Count = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(), RootNodeId, HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_NON_BYPASS, false, &Count))
	{
		// ?
		return;
	}

	if (Count <= 0)
		return;

	TArray<HAPI_NodeId> ChildrenNodeIds;
	ChildrenNodeIds.SetNumUninitialized(Count);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetComposedChildNodeList(
		FHoudiniEngine::Get().GetSession(), RootNodeId, ChildrenNodeIds.GetData(), Count))
	{
		return;
	}

	// Initialize the node hierarchy
	NetworkInfo.RootNodesInfos.SetNum(Count);

	for (int32 Idx = 0; Idx < Count; Idx++)
	{
		NetworkInfo.RootNodesInfos[Idx] = MakeShared<FHoudiniNodeInfo>();
		NetworkInfo.RootNodesInfos[Idx]->NodeId = ChildrenNodeIds[Idx];
		NetworkInfo.RootNodesInfos[Idx]->bIsRootNode = true;
		NetworkInfo.RootNodesInfos[Idx]->bImportNode = false;
		NetworkInfo.RootNodesInfos[Idx]->Parent = NULL;
		FillHoudiniNodeInfo(NetworkInfo.RootNodesInfos[Idx]);
	}
}

/*
void
SSelectHoudiniPathDialog::OnPathChange(const FString& NewPath)
{
	FolderPath = FText::FromString(NewPath);
}
*/


void
SSelectHoudiniPathDialog::UpdateNodePathFromTreeView(FHoudiniNodeInfoPtr& InNodeInfo, FString& OutPath)
{
	if (!InNodeInfo.IsValid())
		return;

	if (InNodeInfo->bImportNode)
	{
		if (!OutPath.IsEmpty())
			OutPath.Append(";");

		// Import this node
		OutPath.Append(InNodeInfo->NodeHierarchyPath);
	}
	else
	{
		// Look for node to import in this node's children
		for (auto& CurrentNodeInfoPtr : InNodeInfo->Childrens)
		{
			UpdateNodePathFromTreeView(CurrentNodeInfoPtr, OutPath);
		}
	}
}



FReply
SSelectHoudiniPathDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	// Update the folder path on OK
	FString NewPath = FString();
	if (ButtonID == EAppReturnType::Ok)
	{
		for (auto& CurrentNodeInfoPtr : NetworkInfo.RootNodesInfos)
		{
			UpdateNodePathFromTreeView(CurrentNodeInfoPtr, NewPath);
		}

		FolderPath = FText::FromString(NewPath);
	}

	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

