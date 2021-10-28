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

#include "HoudiniParameterFolderList.h"
#include "HoudiniParameterFolder.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

UHoudiniParameterFolderList::UHoudiniParameterFolderList(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer), bIsTabMenu(false), bIsTabsShown(false)
{
	ParmType = EHoudiniParameterType::FolderList;
}

UHoudiniParameterFolderList *
UHoudiniParameterFolderList::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterFolderList_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterFolderList::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterFolderList * HoudiniAssetParameter = NewObject< UHoudiniParameterFolderList >(
		InOuter, UHoudiniParameterFolderList::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::FolderList);

	HoudiniAssetParameter->bIsTabMenu = false;
	return HoudiniAssetParameter;
}

void
UHoudiniParameterFolderList::AddTabFolder(UHoudiniParameterFolder* InFolderParm)
{
	TabFolders.Add(InFolderParm);
}

bool 
UHoudiniParameterFolderList::IsTabParseFinished() const
{
	for (auto & CurTab : TabFolders)
	{
		if (!IsValid(CurTab))
			continue;

		if (!CurTab->IsTab())
			continue;

		// Go through visible tab only
		if (!CurTab->IsChosen())
			continue;

		if (CurTab->GetChildCounter() > 0)
			return false;
	}

	return true;
}

void 
UHoudiniParameterFolderList::RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& InputMapping)
{
	const int32 NumFolders = TabFolders.Num();
	for (int i = 0; i < NumFolders; i++)
	{
		UHoudiniParameter* FromParameter = TabFolders[i];

		if (!FromParameter)
			continue;

		UHoudiniParameterFolder* ToParameter = nullptr;
		if (InputMapping.Contains(FromParameter))
		{
			ToParameter = Cast<UHoudiniParameterFolder>(InputMapping.FindRef(FromParameter));
		}
		
		if (!ToParameter)
		{
			HOUDINI_LOG_WARNING(TEXT("[UHoudiniParameterFolderList::RemapParameters] Could not find mapping for existing parameter %s (%s)."), *(FromParameter->GetParameterName()), *(FromParameter->GetPathName()) );
		}

		TabFolders[i] = ToParameter;
	}
}
