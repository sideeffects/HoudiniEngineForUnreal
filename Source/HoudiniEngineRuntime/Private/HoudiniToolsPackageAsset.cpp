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


#include "HoudiniToolsPackageAsset.h"

#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniToolsRuntimeUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectSaveContext.h"

UHoudiniToolsPackageAsset::UHoudiniToolsPackageAsset()
	: bReimportPackageDescription(false)
	, bExportPackageDescription(false)
	, bReimportToolsDescription(false)
	, bExportToolsDescription(false)
{
	
}

#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UHoudiniToolsPackageAsset, ExternalPackageDir))
	{
		UpdateProperties();
	}

	if (FModuleManager::Get().IsModuleLoaded("HoudiniEngineRuntime"))
	{
		const FHoudiniEngineRuntime& HoudiniEngineRuntime = FModuleManager::GetModuleChecked<FHoudiniEngineRuntime>("HoudiniEngineRuntime");
		HoudiniEngineRuntime.BroadcastToolOrPackageChanged();
	}
}
#endif

#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::MakeRelativePackagePath()
{
	const FString ProjectDir = FPaths::ProjectDir();
	FPaths::MakePathRelativeTo(ExternalPackageDir.Path, *ProjectDir);
}
#endif


#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::UpdateProperties()
{
	if (ExternalPackageDir.Path.Len()>0)
	{
		MakeRelativePackagePath();
	}
}
#endif


#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	UObject::PreSave(SaveContext);

	// Update our properties to ensure they are correct (relative paths, etc).
	UpdateProperties();
}
#endif

#if WITH_EDITOR 
void
UHoudiniToolsPackageAsset::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	UObject::PostSaveRoot(ObjectSaveContext);

	if (bExportPackageDescription)
	{
		// Export JSON description for this package.
		const FString JSONPath = FHoudiniToolsRuntimeUtils::GetHoudiniToolsPackageJSONPath(this);
		if (JSONPath.Len())
		{
			FHoudiniToolsRuntimeUtils::WriteJSONFromToolPackageAsset(this, JSONPath);
		}
	}
}
#endif

bool
UHoudiniToolsPackageAsset::Rename(const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags)
{
#if WITH_EDITOR
	// Warn if the name is incorrect
	if (InName != FHoudiniToolsRuntimeUtils::GetPackageUAssetName()) //&& Flags != REN_Test)
	{
		if (!FHoudiniToolsRuntimeUtils::ShowToolsPackageRenameConfirmDialog())
		{
			return false;
		}
	}		
#endif

	// Proceed with the renaming, but add a warning for good measure	
	HOUDINI_LOG_WARNING(TEXT("Renaming a HoudiniToolsPackage to anything but \"HoudiniToolsPackage\" disables it."));

	return Super::Rename(InName, NewOuter, Flags);
}


