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

#include "HoudiniParameter.h"

#include "HoudiniParameterFolderList.generated.h"

class UHoudiniParameterFolder;

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterFolderList : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create instance of this class.
	static UHoudiniParameterFolderList * Create(
		UObject* Outer,
		const FString& ParamName);

	void AddTabFolder(UHoudiniParameterFolder* InFolderParm);

	FORCEINLINE
	TArray<UHoudiniParameterFolder*>& GetTabs() { return TabFolders; };

	FORCEINLINE
	bool IsTabMenu() const { return bIsTabMenu; };

	FORCEINLINE
	void SetIsTabMenu(const bool InIsTabMenu) { bIsTabMenu = InIsTabMenu; };

	FORCEINLINE
	bool IsTabsShown() const { return bIsTabsShown; };

	FORCEINLINE
	void SetTabsShown(const bool& bInTabsShown) { bIsTabsShown = bInTabsShown; };

	bool IsTabParseFinished() const;

	UPROPERTY()
	bool bIsTabMenu;

	UPROPERTY()
	bool bIsTabsShown;

	UPROPERTY()
	TArray<UHoudiniParameterFolder*> TabFolders;


	//------------------------------------------------------------------------------------------------
	// UHoudiniParameter overrides
	//------------------------------------------------------------------------------------------------
	virtual void RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& InputMapping) override;

};