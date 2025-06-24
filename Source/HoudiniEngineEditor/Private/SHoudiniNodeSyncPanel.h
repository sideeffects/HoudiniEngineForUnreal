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
#include "HoudiniInput.h"
#include "HoudiniRuntimeSettings.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StructOnScope.h"

class SHoudiniNodeSyncPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHoudiniNodeSyncPanel)
        : _IsAssetEditor(false)
        {
        }
        SLATE_ATTRIBUTE(bool, IsAssetEditor)
    SLATE_END_ARGS();

    SHoudiniNodeSyncPanel();
    virtual ~SHoudiniNodeSyncPanel() override;

    void Construct( const FArguments& InArgs );

    FMenuBuilder Helper_CreateSelectionWidget();

    void RebuildSelectionView();

    //TSharedRef<SWidget> MakeMBSDetailsView();

    //TSharedRef<SWidget> MakeHSMGPDetailsView();

    //void SetIsAssetEditorPanel(const bool& IsAssetEditor) { bIsAssetEditorPanel = IsAssetEditor; };
    
private:

    TSharedPtr<SVerticalBox> ExportOptionsVBox;
    TSharedPtr<SVerticalBox> LandscapeOptionsVBox;
    TSharedPtr<SVerticalBox> LandscapeSplineOptionsVBox; 


    TSharedPtr<SVerticalBox> SelectionContainer;

    /*TSharedPtr<TStructOnScope<FMeshBuildSettings>> MBS_Ptr;
    TSharedPtr<TStructOnScope<FHoudiniStaticMeshGenerationProperties>> HSMGP_Ptr;*/

    FMenuBuilder SelectedActors;

    // Details view for MeshBuildSettings
    //TSharedPtr<IStructureDetailsView> MBS_DetailsView;
    // Details view for HoudiniStaticMeshGenerationProperties
    //TSharedPtr<IStructureDetailsView> HSMGP_DetailsView;

    bool bIsAssetEditorPanel = false;
};
