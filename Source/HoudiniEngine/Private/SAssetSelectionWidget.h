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

#if WITH_EDITOR

#include "CoreMinimal.h"
//#include "Misc/Attribute.h"

#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "HAPI/HAPI_Common.h"

/** Slate widget used to pick an asset to instantiate from an HDA with multiple assets inside. **/
class SAssetSelectionWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetSelectionWidget)
		: _WidgetWindow(), _AvailableAssetNames()
	{}

	SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(TArray< HAPI_StringHandle >, AvailableAssetNames)
		SLATE_END_ARGS()

public:

	SAssetSelectionWidget();

public:

	/** Widget construct. **/
	void Construct(const FArguments & InArgs);

	/** Return true if cancel button has been pressed. **/
	bool IsCancelled() const;

	/** Return true if constructed widget is valid. **/
	bool IsValidWidget() const;

	/** Return selected asset name. **/
	int32 GetSelectedAssetName() const;

protected:

	/** Called when Ok button is pressed. **/
	FReply OnButtonOk();

	/** Called when Cancel button is pressed. **/
	FReply OnButtonCancel();

	/** Called when user picks an asset. **/
	FReply OnButtonAssetPick(int32 AssetName);

protected:

	/** Parent widget window. **/
	TWeakPtr< SWindow > WidgetWindow;

	/** List of available Houdini Engine asset names. **/
	TArray< HAPI_StringHandle > AvailableAssetNames;

	/** Selected asset name. **/
	int32 SelectedAssetName;

	/** Is set to true if constructed widget is valid. **/
	bool bIsValidWidget;

	/** Is set to true if selection process has been cancelled. **/
	bool bIsCancelled;

	/** Hide name space in the asset name **/
	bool bHideNameSpace;

	/** Hide version in the asset name **/
	bool bHideVersion;

	/** Hide node manager type in the asset name **/
	bool bHideNodeManager;
};

#endif