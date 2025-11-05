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

#include "HoudiniAssetFactory.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAsset.h"
#include "HoudiniToolsPackageAsset.h"
#include "HoudiniToolsEditor.h"
#include "HoudiniEngineUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetFactory::UHoudiniAssetFactory(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = UHoudiniAsset::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will not open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Factory does not import objects from text.
	bText = false;

	// Add supported formats.
	Formats.Add(TEXT("otl;Houdini Engine Asset"));
	Formats.Add(TEXT("otllc;Houdini Engine Limited Commercial Asset"));
	Formats.Add(TEXT("otlnc;Houdini Engine Non-Commercial Asset"));
	Formats.Add(TEXT("hda;Houdini Engine Asset"));
	Formats.Add(TEXT("hdalc;Houdini Engine Limited Commercial Asset"));
	Formats.Add(TEXT("hdanc;Houdini Engine Non-Commercial Asset"));
	Formats.Add(TEXT("hdalibrary;Houdini Engine Expanded Asset"));
}

bool
UHoudiniAssetFactory::DoesSupportClass(UClass * Class)
{
	return Class == SupportedClass;
}

FText
UHoudiniAssetFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniAssetFactoryDescription", "Houdini Engine Asset");
}

UObject *
UHoudiniAssetFactory::FactoryCreateBinary(
	UClass * InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject * Context, const TCHAR * Type, const uint8 *& Buffer,
	const uint8 * BufferEnd, FFeedbackContext * Warn )
{
	// Broadcast notification that a new asset is being imported.
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

	// Create a new asset.
	UHoudiniAsset * HoudiniAsset = NewObject<UHoudiniAsset>(InParent, InName, Flags);

	// Create reimport information.
	UAssetImportData * AssetImportData = HoudiniAsset->AssetImportData;
	if (!AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(HoudiniAsset, UAssetImportData::StaticClass());
		HoudiniAsset->AssetImportData = AssetImportData;
	}
	AssetImportData->Update(UFactory::GetCurrentFilename());

	// Import with the sanitized filename from the AssetImportData
	FString SanitizedFileName = AssetImportData->GetSourceData().SourceFiles.Num() > 0 ? AssetImportData->GetSourceData().SourceFiles[0].RelativeFilename : UFactory::GetCurrentFilename();
	HoudiniAsset->CreateAsset(Buffer, BufferEnd, SanitizedFileName);

	// Import optional external data for the HoudiniAsset.

	// We always import external JSON / Image data if it is available.
	// The Reimport action has to determine whether it wants to preserve existing data, or use the new data. We won't
	// be implementing that policy here.
	FHoudiniToolsEditor::ImportExternalHoudiniAssetData(HoudiniAsset);

	// Broadcast notification that the new asset has been imported.
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, HoudiniAsset);

	return HoudiniAsset;
}

UObject*
UHoudiniAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// "houdini.hdalibrary" files (expanded hda / hda folder) need a special treatment,
	// but ".hda" files can be loaded normally
	FString FileExtension = FPaths::GetExtension(Filename);
	if (FileExtension.Compare(TEXT("hdalibrary"), ESearchCase::IgnoreCase) != 0)
	{
		return Super::FactoryCreateFile(InClass, InParent, InName, Flags, Filename, Parms, Warn, bOutOperationCanceled);
	}

	// Make sure the file name is sections.list
	FString NameOfFile = FPaths::GetBaseFilename(Filename);
	if (NameOfFile.Compare(TEXT("houdini"), ESearchCase::IgnoreCase) != 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load file '%s'. File is not a valid extended HDA."), *Filename);
		return nullptr;
	}

	// Make sure that the proper .list file is loaded
	FString PathToFile = FPaths::GetPath(Filename);
	if (PathToFile.Find(TEXT(".hda")) != (PathToFile.Len() - 4))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load file '%s'. File is not a valid extended HDA."), *Filename);
		return nullptr;
	}

	FString NewFilename = PathToFile;
	FString NewFileNameNoHDA = FPaths::GetBaseFilename(PathToFile);
	FName NewIname = FName(*NewFileNameNoHDA);
	FString NewFileExtension = FPaths::GetExtension(NewFilename);

	// load as binary
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *Filename))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load file '%s' to array"), *Filename);
		return nullptr;
	}

	Data.Add(0);
	ParseParms(Parms);
	const uint8* Ptr = &Data[0];

	return FactoryCreateBinary(InClass, InParent, NewIname, Flags, nullptr, *NewFileExtension, Ptr, Ptr + Data.Num() - 1, Warn);
}

bool
UHoudiniAssetFactory::CanReimport(UObject * Obj, TArray< FString > & OutFilenames)
{
	UHoudiniAsset * HoudiniAsset = Cast<UHoudiniAsset>(Obj);
	if (HoudiniAsset)
	{
		UAssetImportData * AssetImportData = HoudiniAsset->AssetImportData;
		if (AssetImportData)
			OutFilenames.Add(AssetImportData->GetFirstFilename());
		else
			OutFilenames.Add(TEXT(""));

		return true;
	}

	return false;
}

void
UHoudiniAssetFactory::SetReimportPaths(UObject * Obj, const TArray< FString > & NewReimportPaths)
{
	UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >(Obj);
	if (HoudiniAsset && (1 == NewReimportPaths.Num()))
		HoudiniAsset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
}

EReimportResult::Type
UHoudiniAssetFactory::Reimport(UObject * Obj)
{
	
	UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >(Obj);
	if (HoudiniAsset && HoudiniAsset->AssetImportData)
	{
		// Make sure file is valid and exists.
		const FString & Filename = HoudiniAsset->AssetImportData->GetFirstFilename();

		if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
			return EReimportResult::Failed;

		// TODO: Preserve existing ToolData after import, if ToolPackage->bImportToolDescription == true.
		UHoudiniToolData* PrevToolData = HoudiniAsset->HoudiniToolData;

		if (UFactory::StaticImportObject(
			HoudiniAsset->GetClass(), HoudiniAsset->GetOuter(), *HoudiniAsset->GetName(),
			RF_Public | RF_Standalone, *Filename, NULL, this))
		{
			UHoudiniToolsPackageAsset* ToolPackage = FHoudiniToolsEditor::FindOwningToolsPackage(HoudiniAsset);

			if (IsValid(ToolPackage) && !ToolPackage->bReimportToolsDescription && IsValid(PrevToolData))
			{
				// We need to preserve the old data.
				UHoudiniToolData* NewToolData = FHoudiniToolsEditor::GetOrCreateHoudiniToolData(HoudiniAsset);
				NewToolData->CopyFrom(*PrevToolData);

				NewToolData->UpdateOwningAssetThumbnail();
				
				HOUDINI_LOG_WARNING(TEXT("Reimported HoudiniAsset (%s) but ignored external data due to package settings."), *HoudiniAsset->GetName());
				const FString Msg = FString::Format(TEXT("Reimported HoudiniAsset ({0}) but ignored external data (JSON/Icon) due to package settings."), { HoudiniAsset->GetName() });
				FHoudiniEngineUtils::CreateSlateNotification(Msg);
			}
			
			if (HoudiniAsset->GetOuter())
				HoudiniAsset->GetOuter()->MarkPackageDirty();
			else
				HoudiniAsset->MarkPackageDirty();

			return EReimportResult::Succeeded;
		}
	}

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Asset reimport has failed."));
	return EReimportResult::Failed;
}

#undef LOCTEXT_NAMESPACE
