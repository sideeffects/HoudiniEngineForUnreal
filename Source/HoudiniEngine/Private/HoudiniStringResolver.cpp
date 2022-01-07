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

#include "HoudiniStringResolver.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineRuntime.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HAL/FileManager.h"

void FHoudiniStringResolver::GetTokensAsStringMap(TMap<FString,FString>& OutTokens) const
{
	for (auto& Elem : CachedTokens)
	{
		OutTokens.Add(Elem.Key, Elem.Value.StringValue);
	}
}

FString FHoudiniStringResolver::SanitizeTokenValue(const FString& InValue)
{
	// Replace {} characters with __
	FString OutString = InValue;
	OutString.ReplaceInline(ANSI_TO_TCHAR("{"), ANSI_TO_TCHAR("__"));
	OutString.ReplaceInline(ANSI_TO_TCHAR("}"), ANSI_TO_TCHAR("__"));

	return OutString;
}

void FHoudiniStringResolver::SetToken(const FString& InName, const FString& InValue)
{
	CachedTokens.Add(InName, SanitizeTokenValue(InValue));
}

void FHoudiniStringResolver::SetTokensFromStringMap(const TMap<FString, FString>& InTokens, bool bClearTokens)
{
	if (bClearTokens)
	{
		CachedTokens.Empty();
	}

	for (auto& Elem : InTokens)
	{
		CachedTokens.Add(Elem.Key, SanitizeTokenValue(Elem.Value));
	}
}



FString FHoudiniStringResolver::ResolveString(
	const FString& InString) const
{
	const FString Result = FString::Format(*InString, CachedTokens);
	return Result;
}

//void FHoudiniStringResolver::SetCurrentWorld(UWorld* InWorld)
//{
//	SetAttribute("world", InWorld->GetPathName());
//}

FString FHoudiniAttributeResolver::ResolveAttribute(
	const FString& InAttrName,
	const FString& InDefaultValue,
	const bool bInUseDefaultIfAttrValueEmpty) const
{
	if (!CachedAttributes.Contains(InAttrName))
	{
		return ResolveString(InDefaultValue);
	}
	const FString AttrStr = CachedAttributes.FindChecked(InAttrName);
	const FString AttrValue = ResolveString(AttrStr);
	
	if (bInUseDefaultIfAttrValueEmpty && AttrValue.IsEmpty())
		return ResolveString(InDefaultValue);
	return AttrValue;
}

//FString FHoudiniStringResolver::GetTempFolderArgument() const
//{
//	// The actual temp directory should have been supplied externally
//	if (Tokens.Contains(TEXT("temp")))
//		return Tokens.FindChecked(TEXT("temp"));
//
//	UE_LOG(LogTemp, Warning, TEXT("[GetBakeFolderArgument] Could not find 'temp' argument. Using fallback value."));
//	return TEXT("/Game/Content/HoudiniEngine/Temp"); // Fallback value
//}
//
//FString FHoudiniStringResolver::GetBakeFolderArgument() const
//{
//	// The actual bake directory should have been supplied externally
//	if (Tokens.Contains(TEXT("bake")))
//		return Tokens.FindChecked(TEXT("bake"));
//	UE_LOG(LogTemp, Warning, TEXT("[GetBakeFolderArgument] Could not find 'bake' argument. Using fallback value."));
//	return TEXT("/Game/Content/HoudiniEngine/Bake"); // Fallback value
//}
//
//FString FHoudiniStringResolver::GetOutputFolderForPackageMode(EPackageMode PackageMode) const
//{
//	switch (PackageMode)
//	{
//	case EPackageMode::Bake:
//		return GetBakeFolderArgument();
//	case EPackageMode::CookToLevel_Invalid:
//	case EPackageMode::CookToTemp:
//		return GetTempFolderArgument();
//	}
//	return "";
//}

 void FHoudiniAttributeResolver::SetCachedAttributes(const TMap<FString,FString>& Attributes)
 { 
	 CachedAttributes = Attributes; 
 }

void FHoudiniAttributeResolver::SetAttribute(const FString& InName, const FString& InValue)
{
	CachedAttributes.Add(InName, InValue);
}

FString FHoudiniAttributeResolver::ResolveFullLevelPath() const
{
	FString OutputFolder = TEXT("/Game/Content/HoudiniEngine/Temp");

	const FStringFormatArg* BaseDir = CachedTokens.Find(TEXT("out_basedir"));
	if (BaseDir)
		OutputFolder = BaseDir->StringValue;

	FString LevelPathAttr = ResolveAttribute(HAPI_UNREAL_ATTRIB_LEVEL_PATH, TEXT("{out}"));
	if (LevelPathAttr.IsEmpty())
		return OutputFolder;

	return FHoudiniEngineRuntimeUtils::JoinPaths(OutputFolder, LevelPathAttr);
}

FString FHoudiniAttributeResolver::ResolveOutputName(const bool bInForBake) const
{
	FString OutputAttribName;

	if (CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
	{
		OutputAttribName = HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2;
	}
	else if (bInForBake && CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_NAME))
	{
		OutputAttribName = HAPI_UNREAL_ATTRIB_BAKE_NAME;
	}
	else
	{
		OutputAttribName = HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1;
	}

	return ResolveAttribute(OutputAttribName, TEXT("{object_name}"));
}

FString FHoudiniAttributeResolver::ResolveBakeFolder() const
{
	const FString DefaultBakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
	
	constexpr bool bUseDefaultIfAttrValueEmpty = true;
	FString BakeFolder = ResolveAttribute(
		HAPI_UNREAL_ATTRIB_BAKE_FOLDER, TEXT("{bake}"), bUseDefaultIfAttrValueEmpty);
	if (BakeFolder.IsEmpty())
		return DefaultBakeFolder;

	//if (BakeFolder.StartsWith("Game/"))
	//{
	//	BakeFolder = "/" + BakeFolder;
	//}

	//FString AbsoluteOverridePath;
	//if (BakeFolder.StartsWith("/Game/"))
	//{
	//	const FString RelativePath = FPaths::ProjectContentDir() + BakeFolder.Mid(6, BakeFolder.Len() - 6);
	//	AbsoluteOverridePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
	//}
	//else
	//{
	//	if (!BakeFolder.IsEmpty())
	//		AbsoluteOverridePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*BakeFolder);
	//}

	//// Check Validity of the path
	//if (AbsoluteOverridePath.IsEmpty())
	//{
	//	return DefaultBakeFolder;
	//}

	return BakeFolder;
}

FString FHoudiniAttributeResolver::ResolveTempFolder() const
{
	const FString DefaultTempFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
	
	constexpr bool bUseDefaultIfAttrValueEmpty = true;
	const FString TempFolder = ResolveAttribute(
		HAPI_UNREAL_ATTRIB_TEMP_FOLDER, TEXT("{temp}"), bUseDefaultIfAttrValueEmpty);
	if (TempFolder.IsEmpty())
		return DefaultTempFolder;

	return TempFolder;
}

void FHoudiniAttributeResolver::LogCachedAttributesAndTokens() const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("=================="));
	Lines.Add(TEXT("Cached Attributes:"));
	Lines.Add(TEXT("=================="));
	for (const auto& Entry : CachedAttributes)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"), *(Entry.Key), *(Entry.Value)));
	}

	Lines.Add(TEXT("=============="));
	Lines.Add(TEXT("Cached Tokens:"));
	Lines.Add(TEXT("=============="));
	for (const auto& Entry : CachedTokens)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"), *(Entry.Key), *(Entry.Value.StringValue)));
	}
	
	HOUDINI_LOG_DISPLAY(TEXT("%s"), *FString::Join(Lines, TEXT("\n")));
}

