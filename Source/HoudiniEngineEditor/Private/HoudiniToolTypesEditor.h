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
#include "HoudiniToolsEditor.h"
#include "HoudiniToolTypesEditor.generated.h"

class UHoudiniAsset;
class UHoudiniPreset;

// Class describing the various properties for a Houdini Tool
// adding a UClass was necessary to use the PropertyEditor window
UCLASS( EditInlineNew )
class UHoudiniToolEditorProperties : public UObject
{
	GENERATED_BODY()

	public:

		UHoudiniToolEditorProperties();

		/** Name of the tool */
		UPROPERTY( Category = Tool, EditAnywhere )
		FString Name;

		/** Type of the tool */
		UPROPERTY( Category = Tool, EditAnywhere )
		EHoudiniToolType Type;

		/** Selection Type of the tool */
		UPROPERTY( Category = Tool, EditAnywhere )
		EHoudiniToolSelectionType SelectionType;

		/** Tooltip shown on mouse hover */
		UPROPERTY( Category = Tool, EditAnywhere, meta=(MultiLine=true) )
		FString ToolTip;

		// Type of tool that these properties represent.
		UPROPERTY()
		EHoudiniPackageToolType ToolType;
	
		/** Houdini Asset path **/
		UPROPERTY(Category = Tool, EditAnywhere, meta = (FilePathFilter="hda", EditCondition="ToolType==EHoudiniPackageToolType::HoudiniAsset", EditConditionHides))
		FFilePath AssetPath;

		/** Clicking on help icon will bring up this URL */
		UPROPERTY( Category = Tool, EditAnywhere )
		FString HelpURL;
	
		/** This will ensure that the cached icon for this HDA is removed. Note that if an IconPath has been
		 * specified, this option will have no effect.
		 */
		UPROPERTY( Category = Tool, EditAnywhere )
		bool bClearCachedIcon;
	
		/** Import a new icon for this HDA. If this field is left blank, we will look for an
		 * icon next to the HDA with matching name.
		 */
		UPROPERTY( Category = Tool, EditAnywhere, meta = (FilePathFilter = "png") )
		FFilePath IconPath;


		UPROPERTY()
		UHoudiniAsset* HoudiniAsset;

		UPROPERTY()
		UHoudiniPreset* HoudiniPreset; 
};


