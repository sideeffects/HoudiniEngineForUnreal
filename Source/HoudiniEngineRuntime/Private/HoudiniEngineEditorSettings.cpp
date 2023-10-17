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

#include "HoudiniEngineEditorSettings.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE


UHoudiniEngineEditorSettings::UHoudiniEngineEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("HoudiniEngine");

#if WITH_EDITORONLY_DATA
	// Custom Houdini location.
	UseCustomHoudiniLocation = EHoudiniEngineEditorSettingUseCustomLocation::Project;
	CustomHoudiniLocation.Path = TEXT("");
#endif
}

#if WITH_EDITOR
FText UHoudiniEngineEditorSettings::GetSectionText() const
{
	return LOCTEXT("HoudiniEngineUserSettingsDisplayName", "Houdini Engine");
}
#endif	// WITH_EDITOR

#if WITH_EDITOR
void UHoudiniEngineEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	
	static FName NAME_UserToolCategories = GET_MEMBER_NAME_CHECKED(UHoudiniEngineEditorSettings, UserToolCategories);
	
	if (PropertyChangedEvent.Property->GetFName() == NAME_UserToolCategories)
	{
		OnUserToolCategoriesChanged.Broadcast();
	}
}

void
UHoudiniEngineEditorSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeChainProperty(PropertyChangedEvent);

	static FName NAME_UserToolCategories = GET_MEMBER_NAME_CHECKED(UHoudiniEngineEditorSettings, UserToolCategories);

	const FName& HeadName = PropertyChangedEvent.PropertyChain.GetHead()->GetValue()->GetFName();

	if (HeadName == NAME_UserToolCategories)
	{
		OnUserToolCategoriesChanged.Broadcast();
	}
	else if (HeadName == GET_MEMBER_NAME_CHECKED(UHoudiniEngineEditorSettings, CustomHoudiniLocation))
	{
		if (!FHoudiniEngineRuntimeUtils::CheckCustomHoudiniLocation(CustomHoudiniLocation.Path))
			CustomHoudiniLocation.Path = TEXT("");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
