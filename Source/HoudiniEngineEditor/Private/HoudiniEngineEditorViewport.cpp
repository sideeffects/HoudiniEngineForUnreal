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

#include "HoudiniEngineEditorPrivatePCH.h"
#if 0

//--
SHoudiniEngineEditorViewport::~SHoudiniEngineEditorViewport()
{
	if(HoudiniEngineEditorViewportClient.IsValid())
	{
		HoudiniEngineEditorViewportClient->Viewport = NULL;
	}
}


//--
void
SHoudiniEngineEditorViewport::Construct(const FArguments& InArgs)
{

}


//--
void 
SHoudiniEngineEditorViewport::SetParentTab(TSharedRef<SDockTab> InParentTab)
{ 
	ParentTab = InParentTab; 
}


//--
TSharedRef<FEditorViewportClient> 
SHoudiniEngineEditorViewport::MakeEditorViewportClient()
{
	HoudiniEngineEditorViewportClient = MakeShareable( new FHoudiniEngineEditorViewportClient(HoudiniEngineEditor, PreviewScene, HoudiniEngineAsset, NULL) );

	HoudiniEngineEditorViewportClient->bSetListenerPosition = false;
	HoudiniEngineEditorViewportClient->SetRealtime( false );
	//HoudiniEngineEditorViewportClient->VisibilityDelegate.BindSP( this, &FHoudiniEngineEditorViewportClient::IsVisible );

	return HoudiniEngineEditorViewportClient.ToSharedRef();
}


//--
TSharedPtr<SWidget> 
SHoudiniEngineEditorViewport::MakeViewportToolbar()
{
	return NULL;
}

#endif
