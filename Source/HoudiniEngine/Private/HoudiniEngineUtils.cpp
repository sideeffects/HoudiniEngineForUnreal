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

#include "HoudiniEngineUtils.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h"

	// Of course, Windows defines its own GetGeoInfo,
	// So we need to undefine that before including HoudiniApi.h to avoid collision...
	#ifdef GetGeoInfo
		#undef GetGeoInfo
	#endif
#endif

#include "HAPI/HAPI_Version.h"

#include "HoudiniApi.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniCookable.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditorSettings.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineTimers.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniInput.h"
#include "HoudiniParameter.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniOutputTranslator.h"

#if WITH_EDITOR
	#include "SAssetSelectionWidget.h"
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "BlueprintEditor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/WorldComposition.h"
#include "Factories/WorldFactory.h"
#include "FileHelpers.h"
#include "FoliageEditUtility.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "InstancedFoliageActor.h"
#include "Interfaces/IPluginManager.h"
#include "LandscapeStreamingProxy.h"
#include "Misc/Paths.h"
#include "Misc/StringFormatArg.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SSCSEditor.h"
#include "SSubobjectEditor.h"
#include "UnrealEdGlobals.h"
#include "UObject/MetaData.h"
#include "Widgets/Notifications/SNotificationList.h"

#include <vector>

#include "HoudiniEngineAttributes.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
	#include "EditorFramework/AssetImportData.h"
	#include "EditorModeManager.h"
	#include "EditorModes.h"	
	#include "Interfaces/IMainFrameModule.h"
#endif
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

TAutoConsoleVariable<float> CVarHoudiniEngineMeshBuildTimer(
	TEXT("HoudiniEngine.MeshBuildTimer"),
	0.0,
	TEXT("When enabled, the plugin will output timings during the Mesh creation.\n")
);

FHoudiniEngineUtils::FOnHoudiniProxyMeshesRefinedDelegate FHoudiniEngineUtils::OnHoudiniProxyMeshesRefinedDelegate = FHoudiniEngineUtils::FOnHoudiniProxyMeshesRefinedDelegate();

// HAPI_Result strings
const FString kResultStringSuccess(TEXT("Success"));
const FString kResultStringFailure(TEXT("Generic Failure"));
const FString kResultStringAlreadyInitialized(TEXT("Already Initialized"));
const FString kResultStringNotInitialized(TEXT("Not Initialized"));
const FString kResultStringCannotLoadFile(TEXT("Unable to Load File"));
const FString kResultStringParmSetFailed(TEXT("Failed Setting Parameter"));
const FString kResultStringInvalidArgument(TEXT("Invalid Argument"));
const FString kResultStringCannotLoadGeo(TEXT("Uneable to Load Geometry"));
const FString kResultStringCannotGeneratePreset(TEXT("Uneable to Generate Preset"));
const FString kResultStringCannotLoadPreset(TEXT("Uneable to Load Preset"));
const FString kResultStringAssetDefAlrealdyLoaded(TEXT("Asset definition already loaded"));
const FString kResultStringNoLicenseFound(TEXT("No License Found"));
const FString kResultStringDisallowedNCLicenseFound(TEXT("Disallowed Non Commercial License found"));
const FString kResultStringDisallowedNCAssetWithCLicense(TEXT("Disallowed Non Commercial Asset With Commercial License"));
const FString kResultStringDisallowedNCAssetWithLCLicense(TEXT("Disallowed Non Commercial Asset With Limited Commercial License"));
const FString kResultStringDisallowedLCAssetWithCLicense(TEXT("Disallowed Limited Commercial Asset With Commercial License"));
const FString kResultStringDisallowedHengineIndieWith3PartyPlugin(TEXT("Disallowed Houdini Engine Indie With 3rd Party Plugin"));
const FString kResultStringAssetInvalid(TEXT("Invalid Asset"));
const FString kResultStringNodeInvalid(TEXT("Invalid Node"));
const FString kResultStringUserInterrupted(TEXT("User Interrupt"));
const FString kResultStringInvalidSession(TEXT("Invalid Session"));
const FString kResultStringUnknowFailure(TEXT("Unknown Failure"));

#define DebugTextLine TEXT("===================================") 

const int32
FHoudiniEngineUtils::PackageGUIDComponentNameLength = 12;

const int32
FHoudiniEngineUtils::PackageGUIDItemNameLength = 8;



// Maximum size of the data that can be sent via thrift
//#define THRIFT_MAX_CHUNKSIZE			100 * 1024 * 1024 // This is supposedly the current limit in thrift, but still seems to be too large
#define THRIFT_MAX_CHUNKSIZE			10 * 1024 * 1024
//#define THRIFT_MAX_CHUNKSIZE			2048 * 2048
//#define THRIFT_MAX_CHUNKSIZE_STRING		256 * 256

const FString
FHoudiniEngineUtils::GetErrorDescription(HAPI_Result Result)
{
	if (Result == HAPI_RESULT_SUCCESS)
	{
		return kResultStringSuccess;
	}
	else
	{
		switch (Result)
		{
		case HAPI_RESULT_FAILURE:
		{
			return kResultStringFailure;
		}

		case HAPI_RESULT_ALREADY_INITIALIZED:
		{
			return kResultStringAlreadyInitialized;
		}

		case HAPI_RESULT_NOT_INITIALIZED:
		{
			return kResultStringNotInitialized;
		}

		case HAPI_RESULT_CANT_LOADFILE:
		{
			return kResultStringCannotLoadFile;
		}

		case HAPI_RESULT_PARM_SET_FAILED:
		{
			return kResultStringParmSetFailed;
		}

		case HAPI_RESULT_INVALID_ARGUMENT:
		{
			return kResultStringInvalidArgument;
		}

		case HAPI_RESULT_CANT_LOAD_GEO:
		{
			return kResultStringCannotLoadGeo;
		}

		case HAPI_RESULT_CANT_GENERATE_PRESET:
		{
			return kResultStringCannotGeneratePreset;
		}

		case HAPI_RESULT_CANT_LOAD_PRESET:
		{
			return kResultStringCannotLoadPreset;
		}

		case HAPI_RESULT_ASSET_DEF_ALREADY_LOADED:
		{
			return kResultStringAssetDefAlrealdyLoaded;
		}

		case HAPI_RESULT_NO_LICENSE_FOUND:
		{
			return kResultStringNoLicenseFound;
		}

		case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND:
		{
			return kResultStringDisallowedNCLicenseFound;
		}

		case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE:
		{
			return kResultStringDisallowedNCAssetWithCLicense;
		}

		case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE:
		{
			return kResultStringDisallowedNCAssetWithLCLicense;
		}

		case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE:
		{
			return kResultStringDisallowedLCAssetWithCLicense;
		}

		case HAPI_RESULT_DISALLOWED_HENGINEINDIE_W_3PARTY_PLUGIN:
		{
			return kResultStringDisallowedHengineIndieWith3PartyPlugin;
		}

		case HAPI_RESULT_ASSET_INVALID:
		{
			return kResultStringAssetInvalid;
		}

		case HAPI_RESULT_NODE_INVALID:
		{
			return kResultStringNodeInvalid;
		}

		case HAPI_RESULT_USER_INTERRUPTED:
		{
			return kResultStringUserInterrupted;
		}

		case HAPI_RESULT_INVALID_SESSION:
		{
			return kResultStringInvalidSession;
		}

		default:
		{
			return kResultStringUnknowFailure;
		}
		};
	}
}

const FString
FHoudiniEngineUtils::GetStatusString(HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity)
{
	const HAPI_Session* SessionPtr = FHoudiniEngine::Get().GetSession();
	if (!SessionPtr)
	{
		// No valid session
		return FString(TEXT("No valid Houdini Engine session."));
	}

	int32 StatusBufferLength = 0;
	HAPI_Result Result = FHoudiniApi::GetStatusStringBufLength(
		SessionPtr, status_type, verbosity, &StatusBufferLength);

	if (Result == HAPI_RESULT_INVALID_SESSION)
	{
		// Let FHoudiniEngine know that the sesion is now invalid to "Stop" the invalid session
		// and clean things up
		FHoudiniEngine::Get().OnSessionLost();
	}

	if (StatusBufferLength > 0)
	{
		TArray< char > StatusStringBuffer;
		StatusStringBuffer.SetNumZeroed(StatusBufferLength);
		FHoudiniApi::GetStatusString(
			SessionPtr, status_type, &StatusStringBuffer[0], StatusBufferLength);

		return FString(UTF8_TO_TCHAR(&StatusStringBuffer[0]));
	}

	return FString(TEXT(""));
}

FString 
FHoudiniEngineUtils::HapiGetString(int32 StringHandle)
{
	int32 StringLength = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStringBufLength(
		FHoudiniEngine::Get().GetSession(), StringHandle, &StringLength))
	{
		return FString();
	}
		
	if (StringLength <= 0)
		return FString();
		
	std::vector<char> NameBuffer(StringLength, '\0');
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetString(
		FHoudiniEngine::Get().GetSession(),
		StringHandle, &NameBuffer[0], StringLength ) )
	{
		return FString();
	}

	return FString(std::string(NameBuffer.begin(), NameBuffer.end()).c_str());
}


const FString
FHoudiniEngineUtils::GetCookResult()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_RESULT, HAPI_STATUSVERBOSITY_MESSAGES);
}

const FString
FHoudiniEngineUtils::GetCookState()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS);
}

const FString
FHoudiniEngineUtils::GetErrorDescription()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_CALL_RESULT, HAPI_STATUSVERBOSITY_ERRORS);
}

const FString
FHoudiniEngineUtils::GetConnectionError()
{
	int32 ErrorLength = 0;
	FHoudiniApi::GetConnectionErrorLength(&ErrorLength);

	if(ErrorLength <= 0)
		return FString(TEXT(""));

	TArray<char> ConnectionStringBuffer;
	ConnectionStringBuffer.SetNumZeroed(ErrorLength);
	FHoudiniApi::GetConnectionError(&ConnectionStringBuffer[0], ErrorLength, true);

	return FString(UTF8_TO_TCHAR(&ConnectionStringBuffer[0]));
}

void
FHoudiniEngineUtils::MarkAllCookablesAsNeedInstantiation()
{
	// Notify all the Cookables that they need to re instantiate themselves in the new Houdini engine session.
	for (TObjectIterator<UHoudiniCookable> Itr; Itr; ++Itr)
	{
		UHoudiniCookable* HC = *Itr;
		if (!IsValid(HC))
			continue;

		HC->MarkAsNeedInstantiation();
	}
}

const FString
FHoudiniEngineUtils::GetNodeErrorsWarningsAndMessages(HAPI_NodeId InNodeId)
{
	int32 NodeErrorLength = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeNodeCookResult(
		FHoudiniEngine::Get().GetSession(), 
		InNodeId, HAPI_StatusVerbosity::HAPI_STATUSVERBOSITY_ALL, &NodeErrorLength))
	{
		NodeErrorLength = 0;
	}

	FString NodeError;
	if (NodeErrorLength > 0)
	{
		TArray<char> NodeErrorBuffer;
		NodeErrorBuffer.SetNumZeroed(NodeErrorLength);
		FHoudiniApi::GetComposedNodeCookResult(
			FHoudiniEngine::Get().GetSession(), &NodeErrorBuffer[0], NodeErrorLength);

		NodeError = FString(UTF8_TO_TCHAR(&NodeErrorBuffer[0]));
	}

	return NodeError;
}

const FString
FHoudiniEngineUtils::GetCookLog(const TArray<HAPI_NodeId>& InNodeIds)
{
	FString CookLog;

	// Get fetch cook status.
	FString CookResult = FHoudiniEngineUtils::GetCookResult();
	if (!CookResult.IsEmpty())
		CookLog += TEXT("Cook Results:\n") + CookResult + TEXT("\n\n");

	// Add the cook state
	FString CookState = FHoudiniEngineUtils::GetCookState();
	if (!CookState.IsEmpty())
		CookLog += TEXT("Cook State:\n") + CookState + TEXT("\n\n");

	// Error Description
	FString Error = FHoudiniEngineUtils::GetErrorDescription();
	if (!Error.IsEmpty())
		CookLog += TEXT("Error Description:\n") + Error + TEXT("\n\n");

	// Iterates on all the selected HAC and get their node errors
	for (auto& NodeId : InNodeIds)
	{
		if (NodeId < 0)
			continue;

		// Get the node errors, warnings and messages
		FString NodeErrors = FHoudiniEngineUtils::GetNodeErrorsWarningsAndMessages(NodeId);
		if (NodeErrors.IsEmpty())
			continue;

		CookLog += NodeErrors;
	}

	if (CookLog.IsEmpty())
	{
		// See if a failed HAPI initialization / invalid session is preventing us from getting the cook log
		if (!FHoudiniApi::IsHAPIInitialized())
		{
			CookLog += TEXT("\n\nThe Houdini Engine API Library (HAPI) has not been initialized properly.\n\n");
		}
		else
		{
			const HAPI_Session * SessionPtr = FHoudiniEngine::Get().GetSession();
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(SessionPtr))
			{
				CookLog += TEXT("\n\nThe current Houdini Engine Session is not valid.\n\n");
			}
			else if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsInitialized(SessionPtr))
			{
				CookLog += TEXT("\n\nThe current Houdini Engine Session has not been initialized properly.\n\n");
			}
		}

		if (!CookLog.IsEmpty())
		{
			CookLog += TEXT("Please try to restart the current Houdini Engine session via File > Restart Houdini Engine Session.\n\n");
		}
		else
		{
			CookLog = TEXT("\n\nThe cook log is empty...\n\n");
		}
	}

	return CookLog;
}

const FString
FHoudiniEngineUtils::GetAssetHelp(HAPI_NodeId InNodeId)
{
	FString HelpString = TEXT("");
	if (InNodeId < 0)
		return HelpString;

	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId, &AssetInfo), HelpString);

	if (FHoudiniEngineString::ToFString(AssetInfo.helpTextSH, HelpString))
		return HelpString;

	if (HelpString.IsEmpty())
		HelpString = TEXT("No Asset Help Found");

	return HelpString;
}

const FString
FHoudiniEngineUtils::GetAssetHelpURL(HAPI_NodeId InNodeId)
{
	FString HelpString = TEXT("");
	if (InNodeId < 0)
		return HelpString;

	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId, &AssetInfo), HelpString);

	// If we have a help url, use it first
	if (FHoudiniEngineString::ToFString(AssetInfo.helpURLSH, HelpString))
		return HelpString;

	return HelpString;
}

void
FHoudiniEngineUtils::ConvertUnrealString(const FString & UnrealString, std::string & String)
{
	String = H_TCHAR_TO_UTF8(*UnrealString);
}

UWorld*
FHoudiniEngineUtils::FindWorldInPackage(const FString& PackagePath, bool bCreateMissingPackage, bool& bOutCreatedPackage)
{
	AActor* Result = nullptr;
	UWorld* PackageWorld = nullptr;

	bOutCreatedPackage = false;
	
	// Try to load existing UWorld from the tile package path.
	UPackage* Package = FindPackage(nullptr, *PackagePath);
	if (!Package)
		Package = LoadPackage(nullptr, *PackagePath, LOAD_None);

	if (IsValid(Package))
	{
		PackageWorld = UWorld::FindWorldInPackage(Package);
	}
	else if (Package != nullptr)
	{
		// If the package is not valid (pending kill) rename it
		if (bCreateMissingPackage)
		{
			Package->Rename(
				*MakeUniqueObjectName(Package->GetOuter(), Package->GetClass(), FName(PackagePath + TEXT("_pending_kill"))).ToString());
		}
	}

	if (!IsValid(PackageWorld) && bCreateMissingPackage)
	{
		const FName ShortName(FPackageName::GetShortName(PackagePath));
		// The map for this tile does not exist. Create one
		UWorldFactory* Factory = NewObject<UWorldFactory>();
		Factory->WorldType = EWorldType::Inactive; // World that is being loaded but not currently edited by editor.
		PackageWorld = CastChecked<UWorld>(Factory->FactoryCreateNew(UWorld::StaticClass(), Package, ShortName, RF_Public | RF_Standalone, NULL, GWarn));

		if (IsValid(PackageWorld))
		{
			PackageWorld->PostEditChange();
			PackageWorld->MarkPackageDirty();

			if(FPackageName::IsValidLongPackageName(PackagePath))
			{
				const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackagePath);
				bool bSaved = FEditorFileUtils::SaveLevel(PackageWorld->PersistentLevel, *PackageFilename);
			}

			FAssetRegistryModule::AssetCreated(PackageWorld);

			bOutCreatedPackage = true;
		}
	}

	return PackageWorld;
}

bool 
FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			UWorld* CurrentWorld,
			const FString& PackagePath,
			bool bCreateMissingPackage,
			UWorld*& OutWorld,
			ULevel*& OutLevel,
			bool& bOutPackageCreated,
			bool& bPackageInWorld)
{
	UWorld* PackageWorld = FindWorldInPackage(PackagePath, bCreateMissingPackage, bOutPackageCreated);
	if (!IsValid(PackageWorld))
		return false;

	if (PackageWorld->PersistentLevel == CurrentWorld->PersistentLevel)
	{
		// The loaded world and the package world is one and the same.
		OutWorld = CurrentWorld;
		OutLevel = CurrentWorld->PersistentLevel;
		bPackageInWorld = true;
		return true;
	}
	
	if (CurrentWorld->GetLevels().Contains(PackageWorld->PersistentLevel))
	{
		// The package level is loaded into CurrentWorld.
		OutWorld = CurrentWorld;
		OutLevel = PackageWorld->PersistentLevel;
		bPackageInWorld = true;
		return true;
	}

	// The package level is not loaded at all. Send back the on-disk assets.
	OutWorld = PackageWorld;
	OutLevel = PackageWorld->PersistentLevel;
	bPackageInWorld = false;
	return true;
}

void 
FHoudiniEngineUtils::RescanWorldPath(UWorld* InWorld)
{
	FString WorldPath = FPaths::GetPath(InWorld->GetPathName());
	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
	TArray<FString> Packages;
	Packages.Add(WorldPath);
	AssetRegistry.ScanPathsSynchronous(Packages, true);
}

TArray<AActor*>
FHoudiniEngineUtils::FindActorsWithNameNoNumber(UClass* InClass, UWorld* InWorld, const FString& InActorName)
{
	TArray<AActor*> Results;

	for (TActorIterator<AActor> ActorIt(InWorld, InClass); ActorIt; ++ActorIt)
	{
		AActor * Actor = *ActorIt;
		if (Actor->GetFName().GetPlainNameString() == InActorName)
			Results.Add(Actor);
	}
	return Results;
}

AActor*
FHoudiniEngineUtils::FindOrRenameInvalidActorGeneric(UClass* InClass, UWorld* InWorld, const FString& InName, AActor*& OutFoundActor)
{
	// AActor* NamedActor = FindObject<AActor>(Outer, *InName, false);
	// Find ANY actor in the world matching the given name.
	AActor* NamedActor = FindActorInWorld<AActor>(InWorld, FName(InName));
	OutFoundActor = NamedActor;

	FString Suffix;
	if (IsValid(NamedActor))
	{
		if (NamedActor->GetClass()->IsChildOf(InClass))
		{
			return NamedActor;
		}
		else
		{
			// A previous actor that had the same name.
			Suffix = "_0"; 
		}
	}
	else
	{
		if (!NamedActor)
			return nullptr;
		else
			Suffix = "_pendingkill";
	}

	// Rename the invalid/previous actor
	const FName NewName = FHoudiniEngineUtils::RenameToUniqueActor(NamedActor, InName + Suffix);

	return nullptr;
}

void
FHoudiniEngineUtils::LogPackageInfo(const FString& InLongPackageName)
{
	LogPackageInfo( LoadPackage(nullptr, *InLongPackageName, 0) );
}

void FHoudiniEngineUtils::LogPackageInfo(const UPackage* InPackage)
{
	HOUDINI_LOG_MESSAGE(DebugTextLine);
	HOUDINI_LOG_MESSAGE(TEXT("= LogPackageInfo"));
	if (!IsValid(InPackage))
	{
		HOUDINI_LOG_MESSAGE(TEXT(" = Invalid package."));
		HOUDINI_LOG_MESSAGE(DebugTextLine);
		return;
	}

	HOUDINI_LOG_MESSAGE(TEXT(" = Filename: %s"), *(InPackage->GetLoadedPath().GetPackageName()));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	HOUDINI_LOG_MESSAGE(TEXT(" = Package Id: %s"), *(LexToString(InPackage->GetPackageId())));
#else
	HOUDINI_LOG_MESSAGE(TEXT(" = Package Id: %d"), InPackage->GetPackageId().ValueForDebugging());
#endif
	HOUDINI_LOG_MESSAGE(TEXT(" = File size: %d"), InPackage->GetFileSize());
	HOUDINI_LOG_MESSAGE(TEXT(" = Contains map: %d"), InPackage->ContainsMap());
	HOUDINI_LOG_MESSAGE(TEXT(" = Is Fully Loaded: %d"), InPackage->IsFullyLoaded());
	HOUDINI_LOG_MESSAGE(TEXT(" = Is Dirty: %d"), InPackage->IsDirty());

	FWorldTileInfo* WorldTileInfo = InPackage->GetWorldTileInfo();
	if (WorldTileInfo)
	{
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo - Position: %s"), *(WorldTileInfo->Position.ToString()));
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo - Absolute Position: %s"), *(WorldTileInfo->AbsolutePosition.ToString()));
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo - Bounds: %s"), *(WorldTileInfo->Bounds.ToString()));
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo - HidInTileView: %d"), WorldTileInfo->bHideInTileView);
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo - ZOrder: %d"), WorldTileInfo->ZOrder);
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo - Parent tile package: %s"), *(WorldTileInfo->ParentTilePackageName));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT(" = WorldTileInfo: NULL"));
	}

	HOUDINI_LOG_MESSAGE(DebugTextLine);
}

void 
FHoudiniEngineUtils::LogWorldInfo(const FString& InLongPackageName)
{
	UPackage* Package = LoadPackage(nullptr, *InLongPackageName, 0);
	UWorld* World = nullptr;

	if (IsValid(Package))
	{
		World = UWorld::FindWorldInPackage(Package);
	}

	LogWorldInfo(World);
}

void 
FHoudiniEngineUtils::LogWorldInfo(const UWorld* InWorld)
{
	 
	HOUDINI_LOG_MESSAGE(DebugTextLine);
	HOUDINI_LOG_MESSAGE(TEXT("= LogWorldInfo"));
	if (!IsValid(InWorld))
	{
		HOUDINI_LOG_MESSAGE(TEXT(" = Invalid world."));
		HOUDINI_LOG_MESSAGE(DebugTextLine);
		return;
	}

	// UWorld lacks const-correctness on certain accessors
	UWorld* NonConstWorld = const_cast<UWorld*>(InWorld);

	HOUDINI_LOG_MESSAGE(TEXT(" = Path Name: %s"), *(InWorld->GetPathName()));
	HOUDINI_LOG_MESSAGE(TEXT(" = Is Editor World: %d"), InWorld->IsEditorWorld() );
	HOUDINI_LOG_MESSAGE(TEXT(" = Is Game World: %d"), InWorld->IsGameWorld() );
	HOUDINI_LOG_MESSAGE(TEXT(" = Is Preview World: %d"), InWorld->IsPreviewWorld() );
	HOUDINI_LOG_MESSAGE(TEXT(" = Actor Count: %d"), NonConstWorld->GetActorCount() );
	HOUDINI_LOG_MESSAGE(TEXT(" = Num Levels: %d"), InWorld->GetNumLevels() );

	if (IsValid(InWorld->WorldComposition))
	{
		HOUDINI_LOG_MESSAGE(TEXT(" = Composition - Num Tiles: %d"), InWorld->WorldComposition->GetTilesList().Num() );
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT(" = World Composition NULL") );
	}
	
	

	HOUDINI_LOG_MESSAGE(DebugTextLine);
}

FString
FHoudiniEngineUtils::HapiGetEventTypeAsString(const HAPI_PDG_EventType& InEventType)
{
	switch (InEventType)
	{
	    case HAPI_PDG_EVENT_NULL:
	    	return TEXT("HAPI_PDG_EVENT_NULL");

	    case HAPI_PDG_EVENT_WORKITEM_ADD:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_ADD");
	    case HAPI_PDG_EVENT_WORKITEM_REMOVE:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_REMOVE");
	    case HAPI_PDG_EVENT_WORKITEM_STATE_CHANGE:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_STATE_CHANGE");

	    case HAPI_PDG_EVENT_WORKITEM_ADD_DEP:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_ADD_DEP");
	    case HAPI_PDG_EVENT_WORKITEM_REMOVE_DEP:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_REMOVE_DEP");

	    case HAPI_PDG_EVENT_WORKITEM_ADD_PARENT:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_ADD_PARENT");
	    case HAPI_PDG_EVENT_WORKITEM_REMOVE_PARENT:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_REMOVE_PARENT");

	    case HAPI_PDG_EVENT_NODE_CLEAR:
	    	return TEXT("HAPI_PDG_EVENT_NODE_CLEAR");

	    case HAPI_PDG_EVENT_COOK_ERROR:
	    	return TEXT("HAPI_PDG_EVENT_COOK_ERROR");
	    case HAPI_PDG_EVENT_COOK_WARNING:
	    	return TEXT("HAPI_PDG_EVENT_COOK_WARNING");

	    case HAPI_PDG_EVENT_COOK_COMPLETE:
	    	return TEXT("HAPI_PDG_EVENT_COOK_COMPLETE");

	    case HAPI_PDG_EVENT_DIRTY_START:
	    	return TEXT("HAPI_PDG_EVENT_DIRTY_START");
	    case HAPI_PDG_EVENT_DIRTY_STOP:
	    	return TEXT("HAPI_PDG_EVENT_DIRTY_STOP");

	    case HAPI_PDG_EVENT_DIRTY_ALL:
	    	return TEXT("HAPI_PDG_EVENT_DIRTY_ALL");

	    case HAPI_PDG_EVENT_UI_SELECT:
	    	return TEXT("HAPI_PDG_EVENT_UI_SELECT");

	    case HAPI_PDG_EVENT_NODE_CREATE:
	    	return TEXT("HAPI_PDG_EVENT_NODE_CREATE");
	    case HAPI_PDG_EVENT_NODE_REMOVE:
	    	return TEXT("HAPI_PDG_EVENT_NODE_REMOVE");
	    case HAPI_PDG_EVENT_NODE_RENAME:
	    	return TEXT("HAPI_PDG_EVENT_NODE_RENAME");
	    case HAPI_PDG_EVENT_NODE_CONNECT:
	    	return TEXT("HAPI_PDG_EVENT_NODE_CONNECT");
	    case HAPI_PDG_EVENT_NODE_DISCONNECT:
	    	return TEXT("HAPI_PDG_EVENT_NODE_DISCONNECT");

		 case HAPI_PDG_EVENT_WORKITEM_SET_INT:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_SET_INT");					// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_SET_FLOAT:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_SET_FLOAT");				// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_SET_STRING:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_SET_STRING");				// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_SET_FILE:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_SET_FILE");				// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_SET_PYOBJECT:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_SET_PYOBJECT");			// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_SET_GEOMETRY:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_SET_GEOMETRY");			// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_MERGE:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_MERGE");					// DEPRECATED 
	    case HAPI_PDG_EVENT_WORKITEM_RESULT:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_RESULT");

		case HAPI_PDG_EVENT_WORKITEM_PRIORITY:								// DEPRECATED 
			return TEXT("HAPI_PDG_EVENT_WORKITEM_PRIORITY");

	    case HAPI_PDG_EVENT_COOK_START:
	    	return TEXT("HAPI_PDG_EVENT_COOK_START");

	    case HAPI_PDG_EVENT_WORKITEM_ADD_STATIC_ANCESTOR:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_ADD_STATIC_ANCESTOR");
	    case HAPI_PDG_EVENT_WORKITEM_REMOVE_STATIC_ANCESTOR:
	    	return TEXT("HAPI_PDG_EVENT_WORKITEM_REMOVE_STATIC_ANCESTOR");

	    case HAPI_PDG_EVENT_NODE_PROGRESS_UPDATE:
	    	return TEXT("HAPI_PDG_EVENT_NODE_PROGRESS_UPDATE");

	    case HAPI_PDG_EVENT_BATCH_ITEM_INITIALIZED:
	    	return TEXT("HAPI_PDG_EVENT_BATCH_ITEM_INITIALIZED");

	    case HAPI_PDG_EVENT_ALL:
	    	return TEXT("HAPI_PDG_EVENT_ALL");
	    case HAPI_PDG_EVENT_LOG:
	    	return TEXT("HAPI_PDG_EVENT_LOG");

	    case HAPI_PDG_EVENT_SCHEDULER_ADDED:
	    	return TEXT("HAPI_PDG_EVENT_SCHEDULER_ADDED");
	    case HAPI_PDG_EVENT_SCHEDULER_REMOVED:
	    	return TEXT("HAPI_PDG_EVENT_SCHEDULER_REMOVED");
	    case HAPI_PDG_EVENT_SET_SCHEDULER:
	    	return TEXT("HAPI_PDG_EVENT_SET_SCHEDULER");

	    case HAPI_PDG_EVENT_SERVICE_MANAGER_ALL:
	    	return TEXT("HAPI_PDG_EVENT_SERVICE_MANAGER_ALL");

	    case HAPI_PDG_CONTEXT_EVENTS:
	    	return TEXT("HAPI_PDG_CONTEXT_EVENTS");
		default:
			break;
	}

	return FString::Printf(TEXT("Unknown HAPI_PDG_EventType %d"), InEventType);
}

FString
FHoudiniEngineUtils::HapiGetWorkItemStateAsString(const HAPI_PDG_WorkItemState& InWorkItemState)
{
	switch (InWorkItemState)
	{
		case HAPI_PDG_WORKITEM_UNDEFINED:
			return TEXT("HAPI_PDG_WORKITEM_UNDEFINED");
	    case HAPI_PDG_WORKITEM_UNCOOKED:
	    	return TEXT("HAPI_PDG_WORKITEM_UNCOOKED");
	    case HAPI_PDG_WORKITEM_WAITING:
	    	return TEXT("HAPI_PDG_WORKITEM_WAITING");
	    case HAPI_PDG_WORKITEM_SCHEDULED:
	    	return TEXT("HAPI_PDG_WORKITEM_SCHEDULED");
	    case HAPI_PDG_WORKITEM_COOKING:
	    	return TEXT("HAPI_PDG_WORKITEM_COOKING");
	    case HAPI_PDG_WORKITEM_COOKED_SUCCESS:
	    	return TEXT("HAPI_PDG_WORKITEM_COOKED_SUCCESS");
	    case HAPI_PDG_WORKITEM_COOKED_CACHE:
	    	return TEXT("HAPI_PDG_WORKITEM_COOKED_CACHE");
	    case HAPI_PDG_WORKITEM_COOKED_FAIL:
	    	return TEXT("HAPI_PDG_WORKITEM_COOKED_FAIL");
	    case HAPI_PDG_WORKITEM_COOKED_CANCEL:
	    	return TEXT("HAPI_PDG_WORKITEM_COOKED_CANCEL");
	    case HAPI_PDG_WORKITEM_DIRTY:
	    	return TEXT("HAPI_PDG_WORKITEM_DIRTY");
		default:
			break;
	}

	return FString::Printf(TEXT("Unknown HAPI_PDG_WorkItemState %d"), InWorkItemState);
}

// Centralized call to track renaming of objects
bool FHoudiniEngineUtils::RenameObject(UObject* Object, const TCHAR* NewName /*= nullptr*/, UObject* NewOuter /*= nullptr*/, ERenameFlags Flags /*= REN_None*/)
{
	check(Object);
	if (AActor* Actor = Cast<AActor>(Object))
	{
		if (Actor->IsPackageExternal())
		{
			// There should be no need to choose a specific name for an actor in Houdini Engine, instead setting its label should be enough.
			FHoudiniEngineRuntimeUtils::SetActorLabel(Actor, NewName);
			// Force to return false (make sure nothing in Houdini Engine plugin relies on actor being renamed to provided name)
			return false;
		}
	}
	return Object->Rename(NewName, NewOuter, Flags);
}

FName
FHoudiniEngineUtils::RenameToUniqueActor(AActor* InActor, const FString& InName)
{
	const FName NewName = MakeUniqueObjectName(InActor->GetOuter(), InActor->GetClass(), FName(InName));

	FHoudiniEngineUtils::RenameObject(InActor, *(NewName.ToString()));
	FHoudiniEngineRuntimeUtils::SetActorLabel(InActor, NewName.ToString());

	return NewName;
}

UObject* 
FHoudiniEngineUtils::SafeRenameActor(AActor* InActor, const FString& InName, bool UpdateLabel)
{
	check(InActor);

	UObject* PrevObj = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	UObject* ExistingObject = StaticFindObject(/*Class=*/ NULL, InActor->GetOuter(), *InName, EFindObjectFlags::ExactClass);
#else
	UObject* ExistingObject = StaticFindObject(/*Class=*/ NULL, InActor->GetOuter(), *InName, true);
#endif
	if (ExistingObject && ExistingObject != InActor)
	{
		// Rename the existing object
		const FName NewName = MakeUniqueObjectName(ExistingObject->GetOuter(), ExistingObject->GetClass(), FName(InName + TEXT("_old")));
		FHoudiniEngineUtils::RenameObject(ExistingObject, *(NewName.ToString()));
		PrevObj = ExistingObject;
	}

	FHoudiniEngineUtils::RenameObject(InActor, *InName);

	if (UpdateLabel)
	{
		//InActor->SetActorLabel(InName, true);
		FHoudiniEngineRuntimeUtils::SetActorLabel(InActor, InName);
		InActor->Modify(true);
	}

	return PrevObj;
}

bool
FHoudiniEngineUtils::ValidatePath(const FString& InPath, FText* OutInvalidPathReason)
{
	FString AbsolutePath;
	if (InPath.StartsWith("/Game/")) 
	{
		const FString RelativePath = FPaths::ProjectContentDir() + InPath.Mid(6, InPath.Len() - 6);
		AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
	}
	else 
	{
		AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InPath);
	}

	return FPaths::ValidatePath(AbsolutePath, OutInvalidPathReason); 
}

bool
FHoudiniEngineUtils::DoesFolderExist(const FString& InPath)
{
	FString AbsolutePath;
	if (InPath.StartsWith("/Game"))
	{
		const FString RelativePath = FPaths::ProjectContentDir() + InPath.Mid(6, InPath.Len() - 6);
		AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
	}
	else
	{
		AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InPath);
	}

	return FPaths::DirectoryExists(AbsolutePath);
}

void
FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
	FHoudiniPackageParams& OutPackageParams,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FString &BakeFolder,
	const FString &ObjectName,
	const FString &HoudiniAssetName,
	const FString &HoudiniAssetActorName,
	EPackageReplaceMode InReplaceMode,
	bool bAutomaticallySetAttemptToLoadMissingPackages,
	const TOptional<FGuid>& InComponentGuid)
{
	OutPackageParams.GeoId = InIdentifier.GeoId;
	OutPackageParams.ObjectId = InIdentifier.ObjectId;
	OutPackageParams.PartId = InIdentifier.PartId;
	OutPackageParams.SplitStr = InIdentifier.SplitIdentifier;
	OutPackageParams.BakeFolder = BakeFolder;
	OutPackageParams.PackageMode = EPackageMode::Bake;
	OutPackageParams.ReplaceMode = InReplaceMode;
	OutPackageParams.HoudiniAssetName = HoudiniAssetName;
	OutPackageParams.HoudiniAssetActorName = HoudiniAssetActorName;
	OutPackageParams.ObjectName = ObjectName;
	if (InComponentGuid.IsSet())
		OutPackageParams.ComponentGUID = InComponentGuid.GetValue();
}

void
FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
	UWorld* const InWorldContext,
	const UHoudiniCookable* InCookable,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const bool bInHasPreviousBakeData,
	const FString &InDefaultObjectName,
	FHoudiniPackageParams& OutPackageParams,
	FHoudiniAttributeResolver& OutResolver,
	const FString &InDefaultBakeFolder,
	EPackageReplaceMode InReplaceMode,
	const FString &InHoudiniAssetName,
	const FString &InHoudiniAssetActorName,
	bool bAutomaticallySetAttemptToLoadMissingPackages,
	bool bInSkipObjectNameResolutionAndUseDefault,
	bool bInSkipBakeFolderResolutionAndUseDefault)
{
	// Configure OutPackageParams with the default (UI value first then fallback to default from settings) object name
	// and bake folder. We use the "initial" PackageParams as a helper to populate tokens for the resolver.
	//
	// User specified attributes (eg unreal_bake_folder) are then resolved, with the defaults being those tokens configured
	// from the initial PackageParams. Once resolved, we updated the relevant fields in PackageParams
	// (ObjectName and BakeFolder), and update the resolver tokens with these final values.
	//
	// The resolver is then ready to be used to resolve the rest of the user attributes, such as unreal_level_path.
	//
	const FString DefaultBakeFolder = !InDefaultBakeFolder.IsEmpty() ? InDefaultBakeFolder :
		FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();

	const bool bIsHCValid = IsValid(InCookable);
	
	// If InHoudiniAssetName was specified, use that, otherwise use the name of the UHoudiniAsset used by the
	// HoudiniAssetComponent
	FString HoudiniAssetName(TEXT(""));
	if (!InHoudiniAssetName.IsEmpty())
	{
		HoudiniAssetName = InHoudiniAssetName;
	}
	else if (bIsHCValid)
	{
		HoudiniAssetName = InCookable->GetHoudiniAssetName();
	}

	// If InHoudiniAssetActorName was specified, use that, otherwise use the name of the owner of HoudiniAssetComponent
	FString HoudiniAssetActorName(TEXT(""));
	if (!InHoudiniAssetActorName.IsEmpty())
	{
		HoudiniAssetActorName = InHoudiniAssetActorName;
	}
	else if (bIsHCValid && IsValid(InCookable->GetOwner()))
	{
		HoudiniAssetActorName = InCookable->GetOwner()->GetActorNameOrLabel();
	}	

	// Get the HAC's GUID, if the HAC is valid
	TOptional<FGuid> CookableGuid;
	if (bIsHCValid)
		CookableGuid = InCookable->GetCookableGUID();

	const bool bHasBakeNameUIOverride = !InOutputObject.BakeName.IsEmpty();
	FillInPackageParamsForBakingOutput(
		OutPackageParams,
		InIdentifier,
		DefaultBakeFolder,
		bHasBakeNameUIOverride ? InOutputObject.BakeName : InDefaultObjectName,
		HoudiniAssetName,
		HoudiniAssetActorName,
		InReplaceMode,
		bAutomaticallySetAttemptToLoadMissingPackages,
		CookableGuid);

	// If ObjectName is empty and InDefaultObjectName are empty, generate a default via GetPackageName
	const FString DefaultObjectName = OutPackageParams.ObjectName.IsEmpty() && InDefaultObjectName.IsEmpty()
		? OutPackageParams.GetPackageName().TrimChar('_') : InDefaultObjectName;
	if (OutPackageParams.ObjectName.IsEmpty())
		OutPackageParams.ObjectName = DefaultObjectName;

	const TMap<FString, FString>& CachedAttributes = InOutputObject.CachedAttributes;
	TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
	OutPackageParams.UpdateTokensFromParams(InWorldContext, InCookable->GetComponent(), Tokens);
	OutResolver.SetCachedAttributes(CachedAttributes);
	OutResolver.SetTokensFromStringMap(Tokens);

#if defined(HOUDINI_ENGINE_DEBUG_BAKING) && HOUDINI_ENGINE_DEBUG_BAKING
	// Log the cached attributes and tokens for debugging
	OutResolver.LogCachedAttributesAndTokens();
#endif

	bool bUsedDefaultBakeName = !bHasBakeNameUIOverride;
	if (!bInSkipObjectNameResolutionAndUseDefault)
	{
		// Resolve the object name
		// TODO: currently the UI override is checked first (this should probably change so that attributes are used first)
		FString ObjectName;
		if (bHasBakeNameUIOverride)
		{
			ObjectName = InOutputObject.BakeName;
			bUsedDefaultBakeName = false;
		}
		else
		{
			constexpr bool bForBake = true;
			ObjectName = OutResolver.ResolveOutputName(bForBake, &bUsedDefaultBakeName);
			if (ObjectName.IsEmpty())
			{
				ObjectName = DefaultObjectName;
				bUsedDefaultBakeName = true;
			}
		}
		// Update the object name in the package params and also update its token
		OutPackageParams.ObjectName = ObjectName;
		OutResolver.SetToken("object_name", OutPackageParams.ObjectName);
	}

	if (!bInSkipBakeFolderResolutionAndUseDefault)
	{
		// Now resolve the bake folder
		const FString BakeFolder = OutResolver.ResolveBakeFolder();
		if (!BakeFolder.IsEmpty())
			OutPackageParams.BakeFolder = BakeFolder;
	}

	if (!bInSkipObjectNameResolutionAndUseDefault || !bInSkipBakeFolderResolutionAndUseDefault)
	{
		// Update the tokens from the package params
		OutPackageParams.UpdateTokensFromParams(InWorldContext, InCookable->GetComponent(), Tokens);
		OutResolver.SetTokensFromStringMap(Tokens);

#if defined(HOUDINI_ENGINE_DEBUG_BAKING) && HOUDINI_ENGINE_DEBUG_BAKING
		// Log the final tokens
		OutResolver.LogCachedAttributesAndTokens();
#endif
	}

	// If the default bake name is being used, and we haven't baked this output identifier on this output before,
	// then do not allow replacement bakes.
	if (bUsedDefaultBakeName && !bInHasPreviousBakeData && OutPackageParams.ReplaceMode == EPackageReplaceMode::ReplaceExistingAssets)
	{
		HOUDINI_BAKING_WARNING(TEXT(
			"[FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver] Disabling replace bake mode: "
			"default bake name is being used with no previous bake output for the object."));
		OutPackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;
	}
}

void
FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
	const FHoudiniPackageParams& InPackageParams,
	const UWorld* InWorldContext,
	const UObject* InOuterComponent,
	const TMap<FString, FString>& InCachedAttributes,
	const TMap<FString, FString>& InCachedTokens,
	FHoudiniPackageParams& OutPackageParams,
	FHoudiniAttributeResolver& OutResolver,
	bool bInSkipTempFolderResolutionAndUseDefault)
{
	// Populate OutPackageParams from InPackageParams and then update it by resolving user attributes using string tokens.
	//
	// User specified attributes (eg unreal_temp_folder) are then resolved, with the defaults being those tokens configured
	// from the initial PackageParams. Once resolved, we updated the relevant fields in PackageParams and update the
	// resolver tokens with these final values.
	//
	OutPackageParams = InPackageParams;
	
	TMap<FString, FString> Tokens = InCachedTokens;
	OutPackageParams.UpdateTokensFromParams(InWorldContext, InOuterComponent, Tokens);
	OutResolver.SetCachedAttributes(InCachedAttributes);
	OutResolver.SetTokensFromStringMap(Tokens);

	if (!bInSkipTempFolderResolutionAndUseDefault)
	{
		// Now resolve the temp folder
		const FString TempFolder = OutResolver.ResolveTempFolder();
		if (!TempFolder.IsEmpty())
			OutPackageParams.TempCookFolder = TempFolder;
	}

	if (!bInSkipTempFolderResolutionAndUseDefault)
	{
		// Update the tokens from the package params
		OutPackageParams.UpdateTokensFromParams(InWorldContext, InOuterComponent, Tokens);
		OutResolver.SetTokensFromStringMap(Tokens);
	}
}

bool
FHoudiniEngineUtils::RepopulateFoliageTypeListInUI()
{
	// When running this as a commandlet there is no UI, 
	// so GLevelEditorModeTools() is cranky.
	if (IsRunningCommandlet())
		return false;

	// Update / repopulate the foliage editor mode's mesh list if the foliage editor mode is active.
	// TODO: find a better way to do this, the relevant functions are in FEdModeFoliage and FFoliageEdModeToolkit are not API exported
	//
	// This used to deactivate Foliage then Activate it again. But this crashed in UE 5.0, so for now go back to
	// Placement mode.
	FEditorModeTools& EditorModeTools = GLevelEditorModeTools();
	if (EditorModeTools.IsModeActive(FBuiltinEditorModes::EM_Foliage))
	{
		EditorModeTools.DeactivateMode(FBuiltinEditorModes::EM_Foliage);
		EditorModeTools.ActivateMode(FBuiltinEditorModes::EM_Placement);
		return true;
	}

	return false;
}

void
FHoudiniEngineUtils::GatherLandscapeInputs(
	const TArray<TObjectPtr<UHoudiniInput>>& Inputs,
	TArray<ALandscapeProxy*>& AllInputLandscapes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherLandscapeInputs);

	for(auto CurrentInput : Inputs)
	{
		if (!CurrentInput)
			continue;
		
		if (CurrentInput->GetInputType() != EHoudiniInputType::World)
			continue;

		// Check if we have any landscapes as world inputs.
		CurrentInput->ForAllHoudiniInputObjects([&AllInputLandscapes](UHoudiniInputObject* InputObject)
		{
			UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InputObject);
			if (InputLandscape)
			{
				ALandscapeProxy* LandscapeProxy = InputLandscape->GetLandscapeProxy();
				if (IsValid(LandscapeProxy))
				{
					AllInputLandscapes.Add(LandscapeProxy);
				}
			}
		}, true);
	}
}

USceneComponent* 
FHoudiniEngineUtils::GetOuterSceneComponent(const UObject* Obj)
{
	if(!Obj)
		return nullptr;

	// TODO: ? test cookable?
	UObject* Outer = Obj->GetOuter();
	while (Outer)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(Outer);
		if(SceneComponent)
			return SceneComponent;
		Outer = Outer->GetOuter();
	}
	return nullptr;
}

UHoudiniCookable*
FHoudiniEngineUtils::GetOuterHoudiniCookable(const UObject* Obj)
{
	if (!IsValid(Obj))
		return nullptr;

	// Check the direct Outer
	UHoudiniCookable* OuterHC = Cast<UHoudiniCookable>(Obj->GetOuter());
	if (IsValid(OuterHC))
		return OuterHC;

	// Check the whole outer chain
	OuterHC = Obj->GetTypedOuter<UHoudiniCookable>();
	if (IsValid(OuterHC))
		return OuterHC;

	// Finally check if the Object itself is a HC
	UObject* NonConstObj = const_cast<UObject*>(Obj);
	OuterHC = Cast<UHoudiniCookable>(NonConstObj);
	if (IsValid(OuterHC))
		return OuterHC;

	return nullptr;
}

UHoudiniAssetComponent*
FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(const UObject* Obj)
{
	if (!IsValid(Obj))
		return nullptr;

	// Start by looking for a Cookable outer
	UHoudiniCookable* OuterHC = FHoudiniEngineUtils::GetOuterHoudiniCookable(Obj);
	if (IsValid(OuterHC))
		return Cast<UHoudiniAssetComponent>(OuterHC->GetComponent());

	// Check the direct Outer
	UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(Obj->GetOuter());
	if(IsValid(OuterHAC))
		return OuterHAC;

	// Check the whole outer chain
	OuterHAC = Obj->GetTypedOuter<UHoudiniAssetComponent>();
	if (IsValid(OuterHAC))
		return OuterHAC;

	// Finally check if the Object itself is a HaC
	UObject* NonConstObj = const_cast<UObject*>(Obj);
	OuterHAC = Cast<UHoudiniAssetComponent>(NonConstObj);
	if (IsValid(OuterHAC))
		return OuterHAC;

	return nullptr;
}


FString
FHoudiniEngineUtils::ComputeVersionString(bool ExtraDigit)
{
	// Compute Houdini version string.
	FString HoudiniVersionString = FString::Printf(
		TEXT("%d.%d.%s%d"), HAPI_VERSION_HOUDINI_MAJOR,
		HAPI_VERSION_HOUDINI_MINOR,
		(ExtraDigit ? (TEXT("0.")) : TEXT("")),
		HAPI_VERSION_HOUDINI_BUILD);

	// If we have a patch version, we need to append it.
	if (HAPI_VERSION_HOUDINI_PATCH > 0)
		HoudiniVersionString = FString::Printf(TEXT("%s.%d"), *HoudiniVersionString, HAPI_VERSION_HOUDINI_PATCH);
	return HoudiniVersionString;
}


void *
FHoudiniEngineUtils::LoadLibHAPI(FString & StoredLibHAPILocation)
{
	FString HFSPath = TEXT("");
	void * HAPILibraryHandle = nullptr;

	// Look up HAPI_PATH environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
	FString HFS_ENV_VAR = FPlatformMisc::GetEnvironmentVariable(TEXT("HAPI_PATH"));
	if (!HFS_ENV_VAR.IsEmpty())
		HFSPath = HFS_ENV_VAR;

	// Look up environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
	HFS_ENV_VAR = FPlatformMisc::GetEnvironmentVariable(TEXT("HFS"));
	if (!HFS_ENV_VAR.IsEmpty())
		HFSPath = HFS_ENV_VAR;

	// Get platform specific name of libHAPI.
	FString LibHAPIName = FHoudiniEngineRuntimeUtils::GetLibHAPIName();

	// If we have a custom location specified through settings, attempt to use that.
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	const UHoudiniEngineEditorSettings * HoudiniEngineEditorSettings = GetDefault< UHoudiniEngineEditorSettings >();
	bool bCustomPathFound = false;
	if (IsValid(HoudiniEngineEditorSettings) || IsValid(HoudiniRuntimeSettings))
	{
		bool bUseCustomPath = false;
		FString CustomHoudiniLocationPath;

		// The user can set a editor per-project user setting in UHoudiniEngineEditorSettings to determine if
		// the custom location should be disabled, read from the editor per-project user settings or read from the
		// per-project settings.
		if (HoudiniEngineEditorSettings && HoudiniEngineEditorSettings->UseCustomHoudiniLocation == EHoudiniEngineEditorSettingUseCustomLocation::Enabled)
		{
			bUseCustomPath = true;
			CustomHoudiniLocationPath = HoudiniEngineEditorSettings->CustomHoudiniLocation.Path;
		}
		else if ((!HoudiniEngineEditorSettings || HoudiniEngineEditorSettings->UseCustomHoudiniLocation == EHoudiniEngineEditorSettingUseCustomLocation::Project) &&
			HoudiniRuntimeSettings && HoudiniRuntimeSettings->bUseCustomHoudiniLocation)
		{
			bUseCustomPath = true;
			CustomHoudiniLocationPath = HoudiniRuntimeSettings->CustomHoudiniLocation.Path;
		}

		if (bUseCustomPath && !CustomHoudiniLocationPath.IsEmpty())
		{
			// Convert path to absolute if it is relative.
			if (FPaths::IsRelative(CustomHoudiniLocationPath))
				CustomHoudiniLocationPath = FPaths::ConvertRelativePathToFull(CustomHoudiniLocationPath);

			const FString LibHAPICustomPath = FString::Printf(TEXT("%s/%s"), *CustomHoudiniLocationPath, *LibHAPIName);

			if (FPaths::FileExists(LibHAPICustomPath))
			{
				HFSPath = CustomHoudiniLocationPath;
				bCustomPathFound = true;
			}
		}
	}

	// We have HFS environment variable defined (or custom location), attempt to load libHAPI from it.
	if (!HFSPath.IsEmpty())
	{
		if (!bCustomPathFound)
		{
#if PLATFORM_WINDOWS
			HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_WINDOWS);
#elif PLATFORM_MAC
			HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_MAC);
#elif PLATFORM_LINUX
			HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_LINUX);
#endif
		}

		// Create full path to libHAPI binary.
#if PLATFORM_MAC
		FString LibHAPIPath = FString::Printf(TEXT("%s/../Libraries/%s"), *HFSPath, *LibHAPIName);

		// TODO: Handle developer environment paths on macOS.
//		if (!FPaths::FileExists(LibHAPIPath))
//		    LibHAPIPath = FString::Printf(TEXT("%s/dsolib/%s"), *HFSPath, *LibHAPIName);

#else
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);
#endif

		if (FPaths::FileExists(LibHAPIPath))
		{
			// libHAPI binary exists at specified location, attempt to load it.
			FPlatformProcess::PushDllDirectory(*HFSPath);
#if PLATFORM_WINDOWS
			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
#elif PLATFORM_MAC || PLATFORM_LINUX
			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIPath);
#endif
			FPlatformProcess::PopDllDirectory(*HFSPath);

			// If library has been loaded successfully we can stop.
			if ( HAPILibraryHandle )
			{
				if (bCustomPathFound)
					HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from custom path %s"), *LibHAPIName, *HFSPath);
				else
					HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from HFS environment path %s"), *LibHAPIName, *HFSPath);

				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
	}

	// Otherwise, we will attempt to detect Houdini installation.
	FString HoudiniLocation = TEXT(HOUDINI_ENGINE_HFS_PATH);
	FString LibHAPIPath;

	// Compute Houdini version string.
	FString HoudiniVersionString = ComputeVersionString(false);

#if PLATFORM_WINDOWS

	// On Windows, we have also hardcoded HFS path in plugin configuration file; attempt to load from it.
	HFSPath = FString::Printf(TEXT("%s/%s"), *HoudiniLocation, HAPI_HFS_SUBFOLDER_WINDOWS);

	// Create full path to libHAPI binary.
	LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);

	if (FPaths::FileExists(LibHAPIPath))
	{
		FPlatformProcess::PushDllDirectory(*HFSPath);
		HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
		FPlatformProcess::PopDllDirectory(*HFSPath);

		if (HAPILibraryHandle)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from Plugin defined HFS path %s"), *LibHAPIName, *HFSPath);
			StoredLibHAPILocation = HFSPath;
			return HAPILibraryHandle;
		}
	}

	// As a second attempt, on Windows, we try to look up location of Houdini Engine in the registry.
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini Engine"), StoredLibHAPILocation, false);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// As a third attempt, we try to look up location of Houdini installation (not Houdini Engine) in the registry.
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini"), StoredLibHAPILocation, false);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// Do similar registry lookups for the 32 bits registry
	// Look for the Houdini Engine registry install path
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini Engine"), StoredLibHAPILocation, true);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// ... and for the Houdini registry install path
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini"), StoredLibHAPILocation, true);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// Finally, try to load from a hardcoded program files path.
	HoudiniLocation = FString::Printf(
		TEXT("C:\\Program Files\\Side Effects Software\\Houdini %s\\%s"), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_WINDOWS);

#else

#   if PLATFORM_MAC

	// Attempt to load from standard Mac OS X installation.
	HoudiniLocation = FString::Printf(
		TEXT("/Applications/Houdini/Houdini%s/Frameworks/Houdini.framework/Versions/Current/Libraries"), *HoudiniVersionString);

	// Fallback in case the previous one doesnt exist
	if (!FPaths::DirectoryExists(HoudiniLocation))
		HoudiniLocation = FString::Printf(
			TEXT("/Applications/Houdini/Houdini%s/Frameworks/Houdini.framework/Versions/%s/Libraries"), *HoudiniVersionString, *HoudiniVersionString);

	// Fallback in case we're using the steam version
	if (!FPaths::DirectoryExists(HoudiniLocation))
		HoudiniLocation = FString::Printf(
			TEXT("/Applications/Houdini/HoudiniIndieSteam/Frameworks/Houdini.framework/Versions/Current/Libraries"));

	// Backup Fallback in case we're using the steam version
	// (this could probably be removed as paths have changed)
	if (!FPaths::DirectoryExists(HoudiniLocation))
		HoudiniLocation = FString::Printf(
			TEXT("/Users/Shared/Houdini/HoudiniIndieSteam/Frameworks/Houdini.framework/Versions/Current/Libraries"));

#   elif PLATFORM_LINUX

	// Attempt to load from standard Linux installation.
	HoudiniLocation = FString::Printf(
		TEXT("/opt/hfs%s/%s"), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_LINUX);

#   endif

#endif

	// Create full path to libHAPI binary.
	LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HoudiniLocation, *LibHAPIName);

	if (FPaths::FileExists(LibHAPIPath))
	{
		FPlatformProcess::PushDllDirectory(*HoudiniLocation);
		HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIPath);
		FPlatformProcess::PopDllDirectory(*HoudiniLocation);

		if (HAPILibraryHandle)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from expected installation %s"), *LibHAPIName, *HoudiniLocation);
			StoredLibHAPILocation = HoudiniLocation;
			return HAPILibraryHandle;
		}
	}

	StoredLibHAPILocation = TEXT("");
	return HAPILibraryHandle;
}

bool
FHoudiniEngineUtils::IsInitialized()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::IsInitialized);
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	const HAPI_Session * SessionPtr = FHoudiniEngine::Get().GetSession();
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(SessionPtr))
		return false;

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsInitialized(SessionPtr))
		return false;

	return true;
}

bool
FHoudiniEngineUtils::IsHoudiniNodeValid(HAPI_NodeId NodeId)
{
	if (NodeId < 0)
		return false;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	bool ValidationAnswer = true;

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
	{
		return false;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsNodeValid(
		FHoudiniEngine::Get().GetSession(), NodeId,
		NodeInfo.uniqueHoudiniNodeId, &ValidationAnswer))
	{
		return false;
	}

	return ValidationAnswer;
}

bool
FHoudiniEngineUtils::HapiDisconnectAsset(HAPI_NodeId HostAssetId, int32 InputIndex)
{
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::DisconnectNodeInput(
		FHoudiniEngine::Get().GetSession(), HostAssetId, InputIndex), false);

	return true;
}

bool
FHoudiniEngineUtils::DestroyHoudiniAsset(HAPI_NodeId AssetId)
{
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::DeleteNode(
		FHoudiniEngine::Get().GetSession(), AssetId))
	{
		return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::DeleteHoudiniNode(HAPI_NodeId InNodeId)
{
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::DeleteNode(
		FHoudiniEngine::Get().GetSession(), InNodeId))
	{
		return true;
	}

	return false;
}

#if PLATFORM_WINDOWS
void *
FHoudiniEngineUtils::LocateLibHAPIInRegistry(
	const FString & HoudiniInstallationType,
	FString & StoredLibHAPILocation,
	bool LookIn32bitRegistry)
{
	auto FindDll = [&](const FString& InHoudiniInstallationPath)
	{
		FString HFSPath = FString::Printf(TEXT("%s/%s"), *InHoudiniInstallationPath, HAPI_HFS_SUBFOLDER_WINDOWS);

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, HAPI_LIB_OBJECT_WINDOWS);

		if (FPaths::FileExists(LibHAPIPath))
		{
			FPlatformProcess::PushDllDirectory(*HFSPath);
			void* HAPILibraryHandle = FPlatformProcess::GetDllHandle(HAPI_LIB_OBJECT_WINDOWS);
			FPlatformProcess::PopDllDirectory(*HFSPath);

			if (HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Loaded %s from Registry path %s"), HAPI_LIB_OBJECT_WINDOWS,
					*HFSPath);

				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
		return (void*)0;
	};

	FString HoudiniInstallationPath;
	FString HoudiniVersionString = ComputeVersionString(true);
	FString RegistryKey = FString::Printf(
		TEXT("Software\\%sSide Effects Software\\%s"),
		(LookIn32bitRegistry ? TEXT("WOW6432Node\\") : TEXT("")), *HoudiniInstallationType);

	if (FWindowsPlatformMisc::QueryRegKey(
		HKEY_LOCAL_MACHINE, *RegistryKey, *HoudiniVersionString, HoudiniInstallationPath))
	{
		FPaths::NormalizeDirectoryName(HoudiniInstallationPath);
		return FindDll(HoudiniInstallationPath);
	}

	return nullptr;
}
#endif

bool
FHoudiniEngineUtils::LoadHoudiniAsset(const UHoudiniAsset * HoudiniAsset, HAPI_AssetLibraryId& OutAssetLibraryId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::LoadHoudiniAsset);

	OutAssetLibraryId = -1;

	if (!IsValid(HoudiniAsset))
		return false;

	if (!FHoudiniEngineUtils::IsInitialized())
	{
		// If we're not initialized now, it likely means the session has been lost
		FHoudiniEngine::Get().OnSessionLost();
		return false;
	}

	// Get the preferences
	bool bMemoryCopyFirst = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bMemoryCopyFirst = HoudiniRuntimeSettings->bPreferHdaMemoryCopyOverHdaSourceFile;

	// Get the HDA's file path, using the AssetImportData if we have it
	FString AssetFileName = (HoudiniAsset->AssetImportData != nullptr) ? HoudiniAsset->AssetImportData->GetFirstFilename() : HoudiniAsset->GetAssetFileName();
	// We need to convert relative file path to absolute
	if (FPaths::IsRelative(AssetFileName))
		AssetFileName = FPaths::ConvertRelativePathToFull(AssetFileName);

	// We need to modify the file name for expanded .hdas
	FString FileExtension = FPaths::GetExtension(AssetFileName);
	if (FileExtension.Compare(TEXT("hdalibrary"), ESearchCase::IgnoreCase) == 0)
	{
		// the .hda directory is what we should be loading
		AssetFileName = FPaths::GetPath(AssetFileName);
	}

	//Check whether we can Load from file/memory
	bool bCanLoadFromMemory = (!HoudiniAsset->IsExpandedHDA() && HoudiniAsset->GetAssetBytesCount() > 0);
		
	// If the hda file exists, we can simply load it directly
	bool bCanLoadFromFile = false;
	if ( !AssetFileName.IsEmpty() )
	{
		if (FPaths::FileExists(AssetFileName)
			|| (HoudiniAsset->IsExpandedHDA() && FPaths::DirectoryExists(AssetFileName)))
		{
			bCanLoadFromFile = true;
		}
	}

	HAPI_Result Result = HAPI_RESULT_FAILURE;

	// Lambda to detect license issues
	auto CheckLicenseValid = [&AssetFileName](const HAPI_Result& Result)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::LoadHoudiniAsset - CheckLicenseValid);

		// HoudiniEngine acquires a license when creating/loading a node, not when creating a session
		if (Result >= HAPI_RESULT_NO_LICENSE_FOUND && Result < HAPI_RESULT_ASSET_INVALID)
		{
			FString ErrorDesc = GetErrorDescription(Result);
			HOUDINI_LOG_ERROR(TEXT("Error loading Asset %s: License failed: %s."), *AssetFileName, *ErrorDesc);

			// We must stop the session to prevent further attempts at loading an HDA
			// as this could lead to unreal becoming stuck and unresponsive due to license timeout
			FHoudiniEngine::Get().StopSession();

			// Set the HE status to "no license"
			FHoudiniEngine::Get().SetSessionStatus(EHoudiniSessionStatus::NoLicense);

			return false;
		}
		else
		{
			return true;
		}
	};

	// Lambda to load an HDA from file
	auto LoadAssetFromFile = [&Result, &OutAssetLibraryId](const FString& InAssetFileName)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::LoadHoudiniAsset - LoadAssetFromFile);

		// Load the asset from file.
		std::string AssetFileNamePlain;
		FHoudiniEngineUtils::ConvertUnrealString(InAssetFileName, AssetFileNamePlain);
		Result = FHoudiniApi::LoadAssetLibraryFromFile(
			FHoudiniEngine::Get().GetSession(), AssetFileNamePlain.c_str(), true, &OutAssetLibraryId);

	};

	// Lambda to load an HDA from memory
	auto LoadAssetFromMemory = [&Result, &OutAssetLibraryId](const UHoudiniAsset* InHoudiniAsset)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::LoadHoudiniAsset - LoadAssetFromMemory);

		// Load the asset from the cached memory buffer
		Result = FHoudiniApi::LoadAssetLibraryFromMemory(
			FHoudiniEngine::Get().GetSession(),
			reinterpret_cast<const char *>(InHoudiniAsset->GetAssetBytes()),
			InHoudiniAsset->GetAssetBytesCount(), 
			true,
			&OutAssetLibraryId);
	};

	if (!bMemoryCopyFirst)
	{
		// Load from File first
		if (bCanLoadFromFile)
		{
			LoadAssetFromFile(AssetFileName);

			// Detect license issues when loading the HDA
			if (!CheckLicenseValid(Result))
				return false;
		}
		
		// If we failed to load from file ...
		if (Result != HAPI_RESULT_SUCCESS)
		{
			// ... warn the user that we will be loading from memory.
			HOUDINI_LOG_WARNING(TEXT("Asset %s, loading from Memory: source asset file not found."), *AssetFileName);

			// Attempt to load from memory
			if (bCanLoadFromMemory)
			{
				LoadAssetFromMemory(HoudiniAsset);

				// Detect license issues when loading the HDA
				if (!CheckLicenseValid(Result))
					return false;
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Error loading Asset %s: source asset file not found and no memory copy available."), *AssetFileName);
				return false;
			}
		}
	}
	else
	{
		// Load from Memory first
		if(bCanLoadFromMemory)
		{
			LoadAssetFromMemory(HoudiniAsset);

			// Detect license issues when loading the HDA
			if (!CheckLicenseValid(Result))
				return false;
		}

		// If we failed to load from memory ...
		if (Result != HAPI_RESULT_SUCCESS)
		{
			// ... warn the user that we will be loading from file
			HOUDINI_LOG_WARNING(TEXT("Asset %s, loading from File: no memory copy available."), *AssetFileName);
			
			// Attempt to load from file
			if (bCanLoadFromFile)
			{
				LoadAssetFromFile(AssetFileName);

				// Detect license issues when loading the HDA
				if (!CheckLicenseValid(Result))
					return false;
			}
			else
			{
				HOUDINI_LOG_ERROR(TEXT("Error loading Asset %s: source asset file not found and no memory copy available."), *AssetFileName);
				return false;
			}
		}
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error loading asset library for %s: %s"), *AssetFileName, *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	return true;
}

bool
FHoudiniEngineUtils::GetSubAssetNames(
	const HAPI_AssetLibraryId& AssetLibraryId,
	TArray< HAPI_StringHandle >& OutAssetNames)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GetSubAssetNames);

	if (AssetLibraryId < 0)
		return false;

	int32 AssetCount = 0;
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	Result = FHoudiniApi::GetAvailableAssetCount(FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetCount);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_ERROR(TEXT("Error getting asset count: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	if (AssetCount <= 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find an asset."));
		return false;
	}

	OutAssetNames.SetNum(AssetCount);
	Result = FHoudiniApi::GetAvailableAssets(FHoudiniEngine::Get().GetSession(), AssetLibraryId, &OutAssetNames[0], AssetCount);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to retrieve sub asset names: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	if (!AssetCount)
	{
		HOUDINI_LOG_ERROR(TEXT("No assets found"));
		return false;
	}

	// Recipes show as subassets - and can't be instantiated by HAPI (even potentially crash?)
	// So, get all the subasset names - and remove the recipes (::Data/) from the list
	FString RecipeString = TEXT("::Data/");
	for (int32 n = OutAssetNames.Num() - 1; n >= 0; n--)
	{
		// Get the name string
		FHoudiniEngineString HapiStr(OutAssetNames[n]);
		FString AssetName;
		HapiStr.ToFString(AssetName, FHoudiniEngine::Get().GetSession());
		
		// If the HDA names matches the "recipes" substring - remove this subasset from the list to prevent its instantiation
		if (AssetName.Contains(RecipeString))
			OutAssetNames.RemoveAt(n);
	}

	return OutAssetNames.Num() > 0;
}


bool
FHoudiniEngineUtils::OpenSubassetSelectionWindow(TArray<HAPI_StringHandle>& AssetNames, HAPI_StringHandle& OutPickedAssetName )
{
	OutPickedAssetName = -1;

	if (AssetNames.Num() <= 0)
		return false;

	// Default to the first asset
	OutPickedAssetName = AssetNames[0];
	
#if WITH_EDITOR
	// Present the user with a dialog for choosing which asset to instantiate.
	TSharedPtr<SWindow> ParentWindow;	
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		// Check if the main frame is loaded. When using the old main frame it may not be.
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (!ParentWindow.IsValid())
	{
		return false;
	}		

	TSharedPtr<SAssetSelectionWidget> AssetSelectionWidget;
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Select an asset to instantiate"))
		.ClientSize(FVector2D(640, 480))
		.SupportsMinimize(true)
		.SupportsMaximize(true)
		.HasCloseButton(true);

	Window->SetContent(SAssignNew(AssetSelectionWidget, SAssetSelectionWidget)
		.WidgetWindow(Window)
		.AvailableAssetNames(AssetNames));

	if (!AssetSelectionWidget->IsValidWidget())
	{
		return false;
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	int32 DialogPickedAssetName = AssetSelectionWidget->GetSelectedAssetName();
	if (DialogPickedAssetName != -1)
	{
		OutPickedAssetName = DialogPickedAssetName;
		return true;
	}
	else
	{
		return false;
	}
#endif

	return true;
}

/*
bool
FHoudiniEngineUtils::IsValidNodeId(HAPI_NodeId NodeId)
{
	return NodeId != -1;
}
*/

bool
FHoudiniEngineUtils::GetHoudiniAssetName(HAPI_NodeId InNodeId, FString& NameString)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GetHoudiniAssetName);

	if (InNodeId < 0)
		return false;

	HAPI_AssetInfo AssetInfo;
	if (FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &AssetInfo) == HAPI_RESULT_SUCCESS)
	{
		FHoudiniEngineString HoudiniEngineString(AssetInfo.nameSH);
		return HoudiniEngineString.ToFString(NameString);
	}
	else
	{
		// If the node is not an asset, return the node name
		HAPI_NodeInfo NodeInfo;
		if (FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &NodeInfo) == HAPI_RESULT_SUCCESS)
		{
			FHoudiniEngineString HoudiniEngineString(NodeInfo.nameSH);
			return HoudiniEngineString.ToFString(NameString);
		}
	}

	return false;
}

bool
FHoudiniEngineUtils::GetAssetPreset(HAPI_NodeId InNodeId, TArray<int8>& PresetBuffer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GetAssetPreset);
	PresetBuffer.Empty();

	// See if param presets usage is disabled
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bEnabled = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bUsePresetsForParameters : true;
	if (!bEnabled)
		return false;

	HAPI_NodeId NodeId;
	HAPI_AssetInfo AssetInfo;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId, &AssetInfo))
	{
		NodeId = AssetInfo.nodeId;
	}
	else
		NodeId = InNodeId;

	if (NodeId < 0)
		return false;

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

	if (BufferLength <= 0)
		return false;

	PresetBuffer.SetNumZeroed(BufferLength);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPreset(
		FHoudiniEngine::Get().GetSession(), NodeId,
		(char*)(PresetBuffer.GetData()), PresetBuffer.Num()), false);

	return true;
}


bool
FHoudiniEngineUtils::SetAssetPreset(HAPI_NodeId InNodeId, const TArray<int8>& PresetBuffer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::SetAssetPreset);
	if (InNodeId < 0)
		return false;

	// See if param presets usage is disabled
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bEnabled = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bUsePresetsForParameters : true;
	if (!bEnabled)
		return false;

	// If we have stored parameter preset - restore them
	HAPI_Result Res = FHoudiniApi::SetPreset(
		FHoudiniEngine::Get().GetSession(),
		InNodeId,
		HAPI_PRESETTYPE_BINARY,
		"hapi",
		(char*)(PresetBuffer.GetData()),
		PresetBuffer.Num());

	if (Res != HAPI_RESULT_SUCCESS)
		return false;

	return true;
}

bool
FHoudiniEngineUtils::HapiGetAbsNodePath(HAPI_NodeId InNodeId, FString& OutPath)
{
	// Retrieve Path to the given Node, relative to the other given Node
	if (InNodeId < 0)
		return false;

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InNodeId))
		return false;

	HAPI_StringHandle StringHandle;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodePath(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, -1, &StringHandle))
	{
		if(FHoudiniEngineString::ToFString(StringHandle, OutPath))
		{
			return true;
		}
	}
	return false;
}


bool
FHoudiniEngineUtils::HapiGetNodePath(HAPI_NodeId InNodeId, HAPI_NodeId InRelativeToNodeId, FString& OutPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiGetNodePath);

	// Retrieve Path to the given Node, relative to the other given Node
	if ((InNodeId < 0) || (InRelativeToNodeId < 0))
		return false;

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InNodeId))
		return false;

	HAPI_StringHandle StringHandle;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodePath(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, InRelativeToNodeId, &StringHandle))
	{
		if(FHoudiniEngineString::ToFString(StringHandle, OutPath))
		{
			return true;
		}
	}
	return false;
}

bool
FHoudiniEngineUtils::HapiGetNodePath(const FHoudiniGeoPartObject& InHGPO, FString& OutPath)
{
	// Do the HAPI query only on first-use
	if (!InHGPO.NodePath.IsEmpty())
		return true;

	FString NodePathTemp;
	if (InHGPO.AssetId == InHGPO.GeoId)
	{
		HAPI_NodeId NodeId = -1;

		// This is a SOP asset, just return the asset name in this case
		HAPI_AssetInfo AssetInfo;
		FHoudiniApi::AssetInfo_Init(&AssetInfo);
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
			FHoudiniEngine::Get().GetSession(), InHGPO.AssetId, &AssetInfo))
		{
			// Get the asset info node id
			NodeId = AssetInfo.nodeId;
		}
		else
		{
			// Not an asset, just use the node id directly
			NodeId = InHGPO.AssetId;
		}

		HAPI_NodeInfo AssetNodeInfo;
		FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), NodeId, &AssetNodeInfo))
		{
			if (FHoudiniEngineString::ToFString(AssetNodeInfo.nameSH, NodePathTemp))
			{
				OutPath = FString::Printf(TEXT("%s_%d"), *NodePathTemp, InHGPO.PartId);
			}
		}
	}
	else
	{
		// This is an OBJ asset, return the path to this geo relative to the asset
		if (FHoudiniEngineUtils::HapiGetNodePath(InHGPO.GeoId, InHGPO.AssetId, NodePathTemp))
		{
			OutPath = FString::Printf(TEXT("%s_%d"), *NodePathTemp, InHGPO.PartId);
		}
	}

	/*if (OutPath.IsEmpty())
	{
		OutPath = TEXT("Empty");
	}

	return NodePath;
	*/

	return !OutPath.IsEmpty();
}


bool
FHoudiniEngineUtils::HapiGetObjectInfos(
	HAPI_NodeId InNodeId,
	TArray<HAPI_ObjectInfo>& OutObjectInfos,
	TArray<HAPI_Transform>& OutObjectTransforms)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiGetObjectInfos);

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), 
		InNodeId, &NodeInfo), false);

	int32 ObjectCount = 0;
	if (NodeInfo.type == HAPI_NODETYPE_SOP)
	{
		// Add one object info
		ObjectCount = 1;
		OutObjectInfos.SetNumUninitialized(1);
		FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[0]));

		// Use the identity transform
		OutObjectTransforms.SetNumUninitialized(1);
		FHoudiniApi::Transform_Init(&(OutObjectTransforms[0]));

		OutObjectTransforms[0].rotationQuaternion[3] = 1.0f;
		OutObjectTransforms[0].scale[0] = 1.0f;
		OutObjectTransforms[0].scale[1] = 1.0f;
		OutObjectTransforms[0].scale[2] = 1.0f;
		OutObjectTransforms[0].rstOrder = HAPI_SRT;

		// Make sure our parent is an OBJ node
		HAPI_NodeId ParentId = NodeInfo.parentId;
		bool bParentIsObj = false;
		while (!bParentIsObj && ParentId >= 0)
		{
			HAPI_NodeInfo ParentNodeInfo;
			FHoudiniApi::NodeInfo_Init(&ParentNodeInfo);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
				FHoudiniEngine::Get().GetSession(),
				ParentId, &ParentNodeInfo), false);

			if (ParentNodeInfo.type == HAPI_NODETYPE_OBJ)
				bParentIsObj = true;
			else
				ParentId = ParentNodeInfo.parentId;
		}

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
			FHoudiniEngine::Get().GetSession(),
			ParentId, &OutObjectInfos[0]), false);
	}
	else if (NodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeObjectList(
			FHoudiniEngine::Get().GetSession(), InNodeId, nullptr, &ObjectCount), false);

		if (ObjectCount <= 0)
		{
			// This asset is an OBJ that has no object as children, use the object itself
			ObjectCount = 1;
			OutObjectInfos.SetNumUninitialized(1);
			FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[0]));

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
				FHoudiniEngine::Get().GetSession(), InNodeId,
				&OutObjectInfos[0]), false);

			// Use the identity transform
			OutObjectTransforms.SetNumUninitialized(1);
			FHoudiniApi::Transform_Init(&(OutObjectTransforms[0]));

			OutObjectTransforms[0].rotationQuaternion[3] = 1.0f;
			OutObjectTransforms[0].scale[0] = 1.0f;
			OutObjectTransforms[0].scale[1] = 1.0f;
			OutObjectTransforms[0].scale[2] = 1.0f;
			OutObjectTransforms[0].rstOrder = HAPI_SRT;
		}
		else
		{
			int32 ImmediateSOP = 0;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiGetObjectInfos-ComposeChildNodeList);

				// This OBJ has children
				// See if we should add ourself by looking for immediate display SOP 
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
					FHoudiniEngine::Get().GetSession(), NodeInfo.id,
					HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_DISPLAY,
					false, &ImmediateSOP), false);
			}

			bool bAddSelf = ImmediateSOP > 0;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeObjectList(
				FHoudiniEngine::Get().GetSession(), InNodeId, nullptr, &ObjectCount), false);

			// Increment the object count by one if we should add ourself
			OutObjectInfos.SetNumUninitialized(bAddSelf ? ObjectCount + 1 : ObjectCount);
			OutObjectTransforms.SetNumUninitialized(bAddSelf ? ObjectCount + 1 : ObjectCount);
			for (int32 Idx = 0; Idx < OutObjectInfos.Num(); Idx++)
			{
				FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[Idx]));
				FHoudiniApi::Transform_Init(&(OutObjectTransforms[Idx]));
			}

			// Get our object info in  0 if needed
			if (bAddSelf)
			{
				HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
					FHoudiniEngine::Get().GetSession(), InNodeId,
					&OutObjectInfos[0]), false);

				// Use the identity transform
				OutObjectTransforms[0].rotationQuaternion[3] = 1.0f;
				OutObjectTransforms[0].scale[0] = 1.0f;
				OutObjectTransforms[0].scale[1] = 1.0f;
				OutObjectTransforms[0].scale[2] = 1.0f;
				OutObjectTransforms[0].rstOrder = HAPI_SRT;
			}

			// Get the other object infos
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedObjectList(
				FHoudiniEngine::Get().GetSession(), InNodeId,
				&OutObjectInfos[bAddSelf ? 1 : 0], 0, ObjectCount), false);

			// Get the composed object transforms for the others (1 - Count)
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedObjectTransforms(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, HAPI_SRT, &OutObjectTransforms[bAddSelf ? 1 : 0], 0, ObjectCount), false);
		}
	}
	else
		return false;

	return true;
}

bool 
FHoudiniEngineUtils::IsObjNodeFullyVisible(const TSet<HAPI_NodeId>& AllObjectIds, HAPI_NodeId InRootNodeId, HAPI_NodeId InChildNodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::IsObjNodeFullyVisible);

	// Walk up the hierarchy from child to root.
	// If any node in that hierarchy is not in the "AllObjectIds" set, the OBJ node is considered to
	// be hidden.

	if (InChildNodeId == InRootNodeId)
		return true;
	
	HAPI_NodeId ChildNodeId = InChildNodeId;

	HAPI_ObjectInfo ChildObjInfo;
	HAPI_NodeInfo ChildNodeInfo;
	
	FHoudiniApi::ObjectInfo_Init(&ChildObjInfo);
	FHoudiniApi::NodeInfo_Init(&ChildNodeInfo);

	do
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjectInfo(
			FHoudiniEngine::Get().GetSession(), 
			ChildNodeId, &ChildObjInfo))
		{
			// If can't get info for this object, we can't say whether it's visible (or not).
			return false;
		}

		if (!ChildObjInfo.isVisible || ChildObjInfo.nodeId < 0)
		{
			// We have an object in the chain that is not visible. Return false immediately!
			return false;
		}

		if (ChildNodeId != InChildNodeId)
		{
			// Only perform this check for 'parents' of the incoming child node
			if ( !AllObjectIds.Contains(ChildNodeId))
			{
				// There is a non-object node in the hierarchy between the child and asset root, rendering the
				// child object node invisible.
				return false;
			}
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(),
			ChildNodeId, &ChildNodeInfo))
		{
			// Could not retrieve node info.
			return false;
		}

		// Go up the hierarchy.
		ChildNodeId = ChildNodeInfo.parentId;
	} while (ChildNodeId != InRootNodeId && ChildNodeId >= 0);

	// We have traversed the whole hierarchy up to the root and nothing indicated that
	// we _shouldn't_ be visible.
	return true;
}


bool
FHoudiniEngineUtils::HapiGetNodeType(HAPI_NodeId InNodeId, HAPI_NodeType& OutNodeType)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(),
		InNodeId,
		&NodeInfo
		),
		false
	);
	OutNodeType = NodeInfo.type;
	return true;
}


bool
FHoudiniEngineUtils::IsSopNode(HAPI_NodeId NodeId)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId,
		&NodeInfo
		),
		false
	);
	return NodeInfo.type == HAPI_NODETYPE_SOP;
}


bool FHoudiniEngineUtils::ContainsSopNodes(HAPI_NodeId NodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::ContainsSopNodes);
	int ChildCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			HAPI_NODETYPE_SOP,
			HAPI_NODEFLAGS_NON_BYPASS,
			false,
			&ChildCount
		),
		false
	);
	return ChildCount > 0;
}

bool FHoudiniEngineUtils::GetOutputIndex(HAPI_NodeId InNodeId, int32& OutOutputIndex)
{
	int TempValue = -1;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmIntValue(
			FHoudiniEngine::Get().GetSession(),
			InNodeId,
			TCHAR_TO_ANSI(TEXT("outputidx")),
			0,  // index
			&TempValue))
	{
		OutOutputIndex = TempValue;
		return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::GatherAllAssetOutputs(
	HAPI_NodeId AssetId,
	bool bUseOutputNodes,
	bool bOutputTemplatedGeos,
	bool bGatherEditableCurves,
	TArray<HAPI_NodeId>& OutOutputNodes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs);

	OutOutputNodes.Empty();
	
	// Ensure the asset has a valid node ID
	if (AssetId < 0)
		return false;

	// Get the AssetInfo
	HAPI_AssetInfo AssetInfo;
	bool bAssetInfoResult = false;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-GetAssetInfo);
		FHoudiniApi::AssetInfo_Init(&AssetInfo);
		bAssetInfoResult = HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
			FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo);
	}

	// Get the Asset NodeInfo
	HAPI_NodeInfo AssetNodeInfo;
	HAPI_Result NodeResult = HAPI_RESULT_FAILURE;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-GetNodeInfo);
		FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
		NodeResult = FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), AssetId, &AssetNodeInfo);
	}

	if (HAPI_RESULT_SUCCESS != NodeResult)
	{
		// Don't log invalid argument errors here
		if (NodeResult != HAPI_RESULT_INVALID_ARGUMENT)
			HOUDINI_CHECK_ERROR_RETURN(NodeResult, false);
		else
			return false;
	}

	// We only handle SOP and OBJ nodes here.
	if (AssetNodeInfo.type != HAPI_NODETYPE_SOP && AssetNodeInfo.type != HAPI_NODETYPE_OBJ)
		return false;

	FString CurrentAssetName;
	{
		FHoudiniEngineString hapiSTR(bAssetInfoResult ? AssetInfo.nameSH : AssetNodeInfo.nameSH);
		hapiSTR.ToFString(CurrentAssetName);
	}

	// In certain cases, such as PDG output processing we might end up with a SOP node instead of a
	// container. In that case, don't try to run child queries on this node. They will fail.
	const bool bAssetHasChildren = !(AssetNodeInfo.type == HAPI_NODETYPE_SOP && AssetNodeInfo.childNodeCount == 0);

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	TArray<HAPI_Transform> ObjectTransforms;
	if (!FHoudiniEngineUtils::HapiGetObjectInfos(AssetId, ObjectInfos, ObjectTransforms))
		return false;

	// Find the editable nodes in the asset.
	TArray<HAPI_GeoInfo> EditableGeoInfos;
	int32 EditableNodeCount = 0;
	if (bAssetHasChildren)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-ComposeChildNodeList);
		HOUDINI_CHECK_ERROR(FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			AssetId, HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE | HAPI_NODEFLAGS_NON_BYPASS,
			true, &EditableNodeCount));
	}
	
	// All editable nodes will be output, regardless
	// of whether the subnet is considered visible or not.
	if (EditableNodeCount > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-GetComposedChildNodeList);
		TArray<HAPI_NodeId> EditableNodeIds;
		EditableNodeIds.SetNumUninitialized(EditableNodeCount);
		HOUDINI_CHECK_ERROR(FHoudiniApi::GetComposedChildNodeList(
			FHoudiniEngine::Get().GetSession(), 
			AssetId, EditableNodeIds.GetData(), EditableNodeCount));

		for (int32 nEditable = 0; nEditable < EditableNodeCount; nEditable++)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-GetEditableGeoInfo);
			HAPI_GeoInfo CurrentEditableGeoInfo;
			FHoudiniApi::GeoInfo_Init(&CurrentEditableGeoInfo);
			HOUDINI_CHECK_ERROR(FHoudiniApi::GetGeoInfo(
				FHoudiniEngine::Get().GetSession(), 
				EditableNodeIds[nEditable], &CurrentEditableGeoInfo));

			// TODO: Check whether this display geo is actually being output
			//       Just because this is a display node doesn't mean that it will be output (it
			//       might be in a hidden subnet)
			
			// Do not process the main display geo twice!
			if (CurrentEditableGeoInfo.isDisplayGeo)
				continue;

			// We only handle editable curves for now
			if (CurrentEditableGeoInfo.type != HAPI_GEOTYPE_CURVE || !bGatherEditableCurves)
				continue;

			// Add this geo to the geo info array
			EditableGeoInfos.Add(CurrentEditableGeoInfo);
		}
	}

	const bool bIsSopAsset = bAssetInfoResult ? (AssetInfo.nodeId != AssetInfo.objectNodeId) : AssetNodeInfo.type == HAPI_NODETYPE_SOP;
	bool bUseOutputFromSubnets = true;
	if (bAssetHasChildren)
	{
		if (FHoudiniEngineUtils::ContainsSopNodes(bAssetInfoResult ? AssetInfo.nodeId : AssetNodeInfo.id))
		{
			// This HDA contains immediate SOP nodes. Don't look for subnets to output.
			bUseOutputFromSubnets = false;
		}
		else
		{
			// Assume we're using a subnet-based HDA
			bUseOutputFromSubnets = true;
		}
	}
	else
	{
		// This asset doesn't have any children. Don't try to find subnets.
		bUseOutputFromSubnets = false;
	}

	// Before we can perform visibility checks on the Object nodes, we have
	// to build a set of all the Object node ids. The 'AllObjectIds' act
	// as a visibility filter. If an Object node is not present in this
	// list, the content of that node will not be displayed (display / output / templated nodes).
	// NOTE that if the HDA contains immediate SOP nodes we will ignore
	// all subnets and only use the data outputs directly from the HDA.

	TSet<HAPI_NodeId> AllObjectIds;
	if (bUseOutputFromSubnets)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-GetComposedChildNodeList2);
		int NumObjSubnets;
		TArray<HAPI_NodeId> ObjectIds;
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				AssetId,
				HAPI_NODETYPE_OBJ,
				HAPI_NODEFLAGS_OBJ_SUBNET | HAPI_NODEFLAGS_NON_BYPASS,
				true,
				&NumObjSubnets
				),
			false);

		TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherAllAssetOutputs-GetComposedChildNodeList2);
		ObjectIds.SetNumUninitialized(NumObjSubnets);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetComposedChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				AssetId,
				ObjectIds.GetData(),
				NumObjSubnets
				),
			false);
		AllObjectIds.Append(ObjectIds);
	}
	else
	{
		AllObjectIds.Add(AssetInfo.objectNodeId);
	}
	
	// Iterate through all objects to determine visibility and
	// gather output nodes that needs to be cooked.
	int32 OutputIdx = 1;
	for (int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ObjectIdx++)
	{
		// Retrieve the object info
		const HAPI_ObjectInfo& CurrentHapiObjectInfo = ObjectInfos[ObjectIdx];

		// Determine whether this object node is fully visible.
		bool bObjectIsVisible = false;
		HAPI_NodeId GatherOutputsNodeId = -1; // Outputs will be gathered from this node.
		if (!bAssetHasChildren)
		{
			// If the asset doesn't have children, we have to gather outputs from the asset's parent in order to output
			// this asset node
			bObjectIsVisible = true;
			GatherOutputsNodeId = AssetNodeInfo.parentId;
		}
		else if (bIsSopAsset && CurrentHapiObjectInfo.nodeId == AssetInfo.objectNodeId)
		{
			// When dealing with a SOP asset, be sure to gather outputs from the SOP node, not the
			// outer object node.
			bObjectIsVisible = true;
			GatherOutputsNodeId = AssetInfo.nodeId;
		}
		else
		{
			bObjectIsVisible = FHoudiniEngineUtils::IsObjNodeFullyVisible(AllObjectIds, AssetId, CurrentHapiObjectInfo.nodeId);
			GatherOutputsNodeId = CurrentHapiObjectInfo.nodeId;
		}

		// Build an array of the geos we'll need to process
		// In most case, it will only be the display geo, 
		// but we may also want to process editable geos as well
		TArray<HAPI_GeoInfo> GeoInfos;

		// These node ids may need to be cooked in order to extract part counts.
		TSet<HAPI_NodeId> ForceNodesToCook;
		
		// Append the initial set of editable geo infos here
		// then clear the editable geo infos array since we
		// only want to process them once.
		GeoInfos.Append(EditableGeoInfos);
		EditableGeoInfos.Empty();

		if (bObjectIsVisible)
		{
			// NOTE: The HAPI_GetDisplayGeoInfo will not always return the expected Geometry subnet's
			//     Display flag geometry. If the Geometry subnet contains an Object subnet somewhere, the
			//     GetDisplayGeoInfo will sometimes fetch the display SOP from within the subnet which is
			//     not what we want.

			// Resolve and gather outputs (display / output / template nodes) from the GatherOutputsNodeId.
			FHoudiniEngineUtils::GatherImmediateOutputGeoInfos(GatherOutputsNodeId,
				bUseOutputNodes,
				bOutputTemplatedGeos,
				GeoInfos,
				ForceNodesToCook);
			
		} // if (bObjectIsVisible)

		for (HAPI_NodeId NodeId : ForceNodesToCook)
		{
			OutOutputNodes.AddUnique(NodeId);
		}
	}
	return true;
}

bool FHoudiniEngineUtils::GatherImmediateOutputGeoInfos(HAPI_NodeId InNodeId,
                                                        const bool bUseOutputNodes,
                                                        const bool bGatherTemplateNodes,
                                                        TArray<HAPI_GeoInfo>& OutGeoInfos,
                                                        TSet<HAPI_NodeId>& OutForceNodesCook
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GatherImmediateOutputGeoInfos);

	TSet<HAPI_NodeId> GatheredNodeIds;

	// NOTE: This function assumes that the incoming node is a Geometry container that contains immediate
	// outputs / display nodes / template nodes.

	// First we look for (immediate) output nodes (if bUseOutputNodes have been enabled).
	// If we didn't find an output node, we'll look for a display node.

	bool bHasOutputs = false;
	if (bUseOutputNodes)
	{
		int NumOutputs = -1;
		FHoudiniApi::GetOutputGeoCount(
			FHoudiniEngine::Get().GetSession(),
			InNodeId,
			&NumOutputs
			);

		if (NumOutputs > 0)
		{
			bHasOutputs = true;
			
			// -------------------------------------------------
			// Extract GeoInfo from the immediate output nodes.
			// -------------------------------------------------
			TArray<HAPI_GeoInfo> OutputGeoInfos;
			OutputGeoInfos.SetNumUninitialized(NumOutputs);
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetOutputGeoInfos(
				FHoudiniEngine::Get().GetSession(),
				InNodeId,
				OutputGeoInfos.GetData(),
				NumOutputs))
			{
				// Gather all the output nodes
				for (HAPI_GeoInfo& OutputGeoInfo : OutputGeoInfos)
				{
					// This geo should be output. Be sure to ignore any template flags. 
					OutputGeoInfo.isTemplated = false;
					OutGeoInfos.Add(OutputGeoInfo);
					GatheredNodeIds.Add(OutputGeoInfo.nodeId);
					OutForceNodesCook.Add(OutputGeoInfo.nodeId); // Ensure this output node gets cooked
				}
			}
		}
	}
	
	if (!bHasOutputs)
	{
		// If we didn't get any output data, pull our output data directly from the Display node.

		// ---------------------------------
		// Look for display nodes.
		// ---------------------------------
		int DisplayNodeCount = 0;
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			InNodeId,
			HAPI_NODETYPE_SOP,
			HAPI_NODEFLAGS_DISPLAY,
			false,
			&DisplayNodeCount
			))
		{
			if (DisplayNodeCount > 0)
			{
				// Retrieve all the display node ids
				TArray<HAPI_NodeId> DisplayNodeIds;
				DisplayNodeIds.SetNumUninitialized(DisplayNodeCount);
				if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetComposedChildNodeList(
						FHoudiniEngine::Get().GetSession(),
						InNodeId,
						DisplayNodeIds.GetData(),
						DisplayNodeCount
					))
				{
					HAPI_GeoInfo GeoInfo;
					FHoudiniApi::GeoInfo_Init(&GeoInfo);
					// Retrieve the Geo Infos for each display node
					for(HAPI_NodeId DisplayNodeId : DisplayNodeIds)
					{
						if (GatheredNodeIds.Contains(DisplayNodeId))
							continue; // This node has already been gathered from this subnet.
						
						if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetGeoInfo(
								FHoudiniEngine::Get().GetSession(),
								DisplayNodeId,
								&GeoInfo)
							)
						{
							// This geo should be output. Be sure to ignore any templated flags.
							GeoInfo.isTemplated = false;
							OutGeoInfos.Add(GeoInfo);
							GatheredNodeIds.Add(DisplayNodeId);
							// If this node doesn't have a part_id count, ensure it gets cooked.
							OutForceNodesCook.Add(DisplayNodeId);
						}
					}
				}
			} // if (DisplayNodeCount > 0)
		}
	} // if (!HasOutputNode)

	// Gather templated nodes.
	if (bGatherTemplateNodes)
	{
		int NumTemplateNodes = 0;
		// Gather all template nodes
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			InNodeId,
			HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_TEMPLATED,
			false,
			&NumTemplateNodes
			))
		{
			TArray<HAPI_NodeId> TemplateNodeIds;
			TemplateNodeIds.SetNumUninitialized(NumTemplateNodes);
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetComposedChildNodeList(
					FHoudiniEngine::Get().GetSession(),
					InNodeId,
					TemplateNodeIds.GetData(),
					NumTemplateNodes
					))
			{
				
				for(HAPI_NodeId TemplateNodeId : TemplateNodeIds)
				{
					if (GatheredNodeIds.Contains(TemplateNodeId))
					{
						continue; // This geometry has already been gathered.
					}

					HAPI_GeoInfo GeoInfo;
					FHoudiniApi::GeoInfo_Init(&GeoInfo);
					FHoudiniApi::GetGeoInfo(
							FHoudiniEngine::Get().GetSession(),
							TemplateNodeId,
							&GeoInfo
						);
					// For some reason the return type is always "HAPI_RESULT_INVALID_ARGUMENT", so we
					// just check the GeoInfo.type manually.
					if (GeoInfo.type != HAPI_GEOTYPE_INVALID)
					{
						// Add this template node to the gathered outputs.
						GatheredNodeIds.Add(TemplateNodeId);
						OutGeoInfos.Add(GeoInfo);
						// If this node doesn't have a part_id count, ensure it gets cooked.
						OutForceNodesCook.Add(TemplateNodeId);
					}
				}
			}
		}
	}
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAssetTransform(HAPI_NodeId InNodeId, FTransform& OutTransform)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId,
		&NodeInfo), false);

	HAPI_Transform HapiTransform;
	FHoudiniApi::Transform_Init(&HapiTransform);

	if (NodeInfo.type == HAPI_NODETYPE_SOP)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectTransform(
			FHoudiniEngine::Get().GetSession(), 
			NodeInfo.parentId, -1, HAPI_SRT, &HapiTransform), false);
	}
	else if (NodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectTransform(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, -1, HAPI_SRT, &HapiTransform), false);
	}
	else
		return false;

	// Convert HAPI transform to Unreal one.
	FHoudiniEngineUtils::TranslateHapiTransform(HapiTransform, OutTransform);

	return true;
}

void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_Transform& HapiTransform, FTransform& UnrealTransform)
{
	if ( HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM )
	{
		// Swap Y/Z, invert W
		FQuat4d ObjectRotation(
			(double)HapiTransform.rotationQuaternion[0],
			(double)HapiTransform.rotationQuaternion[2],
			(double)HapiTransform.rotationQuaternion[1],
			(double)-HapiTransform.rotationQuaternion[3]);

		// Swap Y/Z and scale
		FVector3d ObjectTranslation(
			(double)HapiTransform.position[0],
			(double)HapiTransform.position[2],
			(double)HapiTransform.position[1]);
		ObjectTranslation *= HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		FVector3d ObjectScale3D(
			(double)HapiTransform.scale[0],
			(double)HapiTransform.scale[2],
			(double)HapiTransform.scale[1]);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
	else
	{
		FQuat4d ObjectRotation(
			(double)HapiTransform.rotationQuaternion[0],
			(double)HapiTransform.rotationQuaternion[1],
			(double)HapiTransform.rotationQuaternion[2],
			(double)HapiTransform.rotationQuaternion[3]);

		FVector3d ObjectTranslation(
			(double)HapiTransform.position[0],
			(double)HapiTransform.position[1],
			(double)HapiTransform.position[2]);
		ObjectTranslation *= HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		FVector3d ObjectScale3D(
			(double)HapiTransform.scale[0],
			(double)HapiTransform.scale[1],
			(double)HapiTransform.scale[2]);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
}

void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_TransformEuler& HapiTransformEuler, FTransform& UnrealTransform)
{
	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformEulerToMatrix(FHoudiniEngine::Get().GetSession(), &HapiTransformEuler, HapiMatrix);

	HAPI_Transform HapiTransformQuat;
	FMemory::Memzero< HAPI_Transform >(HapiTransformQuat);
	FHoudiniApi::ConvertMatrixToQuat(FHoudiniEngine::Get().GetSession(), HapiMatrix, HAPI_SRT, &HapiTransformQuat);

	FHoudiniEngineUtils::TranslateHapiTransform(HapiTransformQuat, UnrealTransform);
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(const FTransform& UnrealTransform, HAPI_Transform& HapiTransform)
{
	FMemory::Memzero< HAPI_Transform >(HapiTransform);
	HapiTransform.rstOrder = HAPI_SRT;

	FQuat4d UnrealRotation = UnrealTransform.GetRotation();
	FVector3d UnrealTranslation = UnrealTransform.GetTranslation();
	FVector3d UnrealScale = UnrealTransform.GetScale3D();

	if (HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM)
	{
		// Swap Y/Z, invert XYZ
		HapiTransform.rotationQuaternion[0] = (float)-UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = (float)-UnrealRotation.Z;
		HapiTransform.rotationQuaternion[2] = (float)-UnrealRotation.Y;
		HapiTransform.rotationQuaternion[3] = (float)UnrealRotation.W;

		// Swap Y/Z, scale
		HapiTransform.position[0] = (float)UnrealTranslation.X / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransform.position[1] = (float)UnrealTranslation.Z / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransform.position[2] = (float)UnrealTranslation.Y / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		HapiTransform.scale[0] = (float)UnrealScale.X;
		HapiTransform.scale[1] = (float)UnrealScale.Z;
		HapiTransform.scale[2] = (float)UnrealScale.Y;
	}
	else
	{
		HapiTransform.rotationQuaternion[0] = (float)UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = (float)UnrealRotation.Y;
		HapiTransform.rotationQuaternion[2] = (float)UnrealRotation.Z;
		HapiTransform.rotationQuaternion[3] = (float)UnrealRotation.W;

		HapiTransform.position[0] = (float)UnrealTranslation.X;
		HapiTransform.position[1] = (float)UnrealTranslation.Y;
		HapiTransform.position[2] = (float)UnrealTranslation.Z;

		HapiTransform.scale[0] = (float)UnrealScale.X;
		HapiTransform.scale[1] = (float)UnrealScale.Y;
		HapiTransform.scale[2] = (float)UnrealScale.Z;
	}
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(
	const FTransform& UnrealTransform,
	HAPI_TransformEuler& HapiTransformEuler)
{
	FHoudiniApi::TransformEuler_Init(&HapiTransformEuler);

	HapiTransformEuler.rstOrder = HAPI_SRT;
	HapiTransformEuler.rotationOrder = HAPI_XYZ;

	FQuat UnrealRotation = UnrealTransform.GetRotation();
	FVector UnrealTranslation = UnrealTransform.GetTranslation();
	FVector UnrealScale = UnrealTransform.GetScale3D();

	if (HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM)
	{
		// switch the quaternion to Y-up, LHR by Swapping Y/Z and negating W
		Swap(UnrealRotation.Y, UnrealRotation.Z);
		UnrealRotation.W = -UnrealRotation.W;
		const FRotator Rotator = UnrealRotation.Rotator();

		// Negate roll and pitch since they are actually RHR
		HapiTransformEuler.rotationEuler[0] = -(float)Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = -(float)Rotator.Pitch;
		HapiTransformEuler.rotationEuler[2] = (float)Rotator.Yaw;

		// Swap Y/Z, scale
		HapiTransformEuler.position[0] = (float)(UnrealTranslation.X) / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransformEuler.position[1] = (float)(UnrealTranslation.Z) / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransformEuler.position[2] = (float)(UnrealTranslation.Y) / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		HapiTransformEuler.scale[0] = (float)UnrealScale.X;
		HapiTransformEuler.scale[1] = (float)UnrealScale.Z;
		HapiTransformEuler.scale[2] = (float)UnrealScale.Y;
	}
	else
	{
		const FRotator Rotator = UnrealRotation.Rotator();
		HapiTransformEuler.rotationEuler[0] = (float)Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = (float)Rotator.Yaw;
		HapiTransformEuler.rotationEuler[2] = (float)Rotator.Pitch;

		HapiTransformEuler.position[0] = (float)UnrealTranslation.X;
		HapiTransformEuler.position[1] = (float)UnrealTranslation.Y;
		HapiTransformEuler.position[2] = (float)UnrealTranslation.Z;

		HapiTransformEuler.scale[0] = (float)UnrealScale.X;
		HapiTransformEuler.scale[1] = (float)UnrealScale.Y;
		HapiTransformEuler.scale[2] = (float)UnrealScale.Z;
	}
}

void
FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector(const TArray<float>& InRawData, TArray<FVector>& OutVectorData)
{
	OutVectorData.SetNum(InRawData.Num() / 3);

	for (int32 OutIndex = 0; OutIndex < OutVectorData.Num(); OutIndex++)
	{
		int32 InIndex = OutIndex * 3;

		// Swap Y/Z and scale meters to centimeters
		OutVectorData[OutIndex].X = (double)(InRawData[InIndex + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION);
		OutVectorData[OutIndex].Y = (double)(InRawData[InIndex + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION);
		OutVectorData[OutIndex].Z = (double)(InRawData[InIndex + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION);
	}
}

FVector3f
FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector3f(const FVector3f& InVector)
{
	FVector3f ConvertedPoint;
	ConvertedPoint.X = InVector.X * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	ConvertedPoint.Y = InVector.Z * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	ConvertedPoint.Z = InVector.Y * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	return ConvertedPoint;
}


void
FHoudiniEngineUtils::ConvertHoudiniScaleToUnrealVector(const TArray<float>& InRawData, TArray<FVector>& OutVectorData)
{
	OutVectorData.SetNum(InRawData.Num() / 3);

	for (int32 OutIndex = 0; OutIndex < OutVectorData.Num(); OutIndex++)
	{
		int32 InIndex = OutIndex * 3;

		// Just swap Y/Z
		OutVectorData[OutIndex].X = (double)InRawData[InIndex + 0];
		OutVectorData[OutIndex].Y = (double)InRawData[InIndex + 2];
		OutVectorData[OutIndex].Z = (double)InRawData[InIndex + 1];
	}
}

void
FHoudiniEngineUtils::ConvertHoudiniRotQuatToUnrealVector(const TArray<float>& InRawData, TArray<FVector>& OutVectorData)
{
	OutVectorData.SetNum(InRawData.Num() / 4);

	for (int32 OutIndex = 0; OutIndex < OutVectorData.Num(); OutIndex++)
	{
		int32 InIndex = OutIndex * 4;

		// Extract a quaternion: Swap Y/Z, invert W
		FQuat ObjectRotation(
			(double)InRawData[InIndex + 0],
			(double)InRawData[InIndex + 2],
			(double)InRawData[InIndex + 1],
			(double)-InRawData[InIndex + 3]);

		// Get Euler angles
		OutVectorData[OutIndex] = ObjectRotation.Euler();
	}
}

void
FHoudiniEngineUtils::ConvertHoudiniRotEulerToUnrealVector(const TArray<float>& InRawData, TArray<FVector>& OutVectorData)
{
	OutVectorData.SetNum(InRawData.Num() / 3);

	for (int32 OutIndex = 0; OutIndex < OutVectorData.Num(); OutIndex++)
	{
		int32 InIndex = OutIndex * 3;

		// Just swap Y/Z
		OutVectorData[OutIndex].X = (double)InRawData[InIndex + 0];
		OutVectorData[OutIndex].Y = (double)InRawData[InIndex + 2];
		OutVectorData[OutIndex].Z = (double)InRawData[InIndex + 1];
	}
}

bool
FHoudiniEngineUtils::UploadCookableTransform(UHoudiniCookable* HC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::UploadCookableTransform);

	if (!HC || !HC->IsComponentSupported())
		return false;

	if (!HC->ComponentData->bUploadTransformsToHoudiniEngine)
		return false;

	if (!IsValid(HC->ComponentData->Component.Get()))
		return false;

	// Indicates the Cookable has been fully loaded
	if (!HC->IsFullyLoaded())
		return false;

	if (HC->CookCount > 0 && HC->GetNodeId() >= 0)
	{
		if (!FHoudiniEngineUtils::HapiSetAssetTransform(HC->GetNodeId(), HC->ComponentData->Component->GetComponentTransform()))
			return false;
	}

	HC->SetHasComponentTransformChanged(false);

	return true;
}


bool
FHoudiniEngineUtils::HapiSetAssetTransform(HAPI_NodeId AssetId, const FTransform & Transform)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiSetAssetTransform);
	if (AssetId < 0)
		return false;

	// Translate Unreal transform to HAPI Euler one.
	HAPI_TransformEuler TransformEuler;
	FMemory::Memzero< HAPI_TransformEuler >(TransformEuler);
	FHoudiniEngineUtils::TranslateUnrealTransform(Transform, TransformEuler);

	// Get the NodeInfo
	HAPI_NodeInfo LocalAssetNodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), AssetId,
		&LocalAssetNodeInfo), false);

	if (LocalAssetNodeInfo.type == HAPI_NODETYPE_SOP)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(),
			LocalAssetNodeInfo.parentId,
			&TransformEuler), false);
	}
	else if (LocalAssetNodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(),
			AssetId, &TransformEuler), false);
	}
	else
		return false;

	return true;
}

HAPI_NodeId
FHoudiniEngineUtils::HapiGetParentNodeId(HAPI_NodeId NodeId)
{
	HAPI_NodeId ParentId = -1;
	if (NodeId >= 0)
	{
		HAPI_NodeInfo NodeInfo;
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
			ParentId = NodeInfo.parentId;
	}

	return ParentId;
}


// Assign a unique Actor Label if needed
void
FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded(HAPI_NodeId InNodeId, AActor* InActorOwner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded);

#if WITH_EDITOR
	if (InNodeId < 0)
		return;

	if (!InActorOwner)
		return;

	// Make sure we only create a unique name for a new Houdini Actor
	// We don't want to loose custom/manual names
	if (!InActorOwner->GetActorNameOrLabel().StartsWith(AHoudiniAssetActor::StaticClass()->GetName()))
		return;

	if (!InActorOwner->GetName().StartsWith(AHoudiniAssetActor::StaticClass()->GetName()))
		return;

	// Assign unique actor label based on asset name if it seems to have not been renamed already
	FString UniqueName;
	if (FHoudiniEngineUtils::GetHoudiniAssetName(InNodeId, UniqueName))
		FActorLabelUtilities::SetActorLabelUnique(InActorOwner, UniqueName);
#endif
}

bool
FHoudiniEngineUtils::GetLicenseType(FString & LicenseType)
{
	LicenseType = TEXT("");
	HAPI_License LicenseTypeValue = HAPI_LICENSE_NONE;

	if (FHoudiniEngine::Get().GetSession())
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetSessionEnvInt(
			FHoudiniEngine::Get().GetSession(), HAPI_SESSIONENVINT_LICENSE,
			(int32*)&LicenseTypeValue), false);
	}

	switch (LicenseTypeValue)
	{
		case HAPI_LICENSE_NONE:
		{
			LicenseType = TEXT("No License Acquired");
			break;
		}

		case HAPI_LICENSE_HOUDINI_ENGINE:
		{
			LicenseType = TEXT("Houdini Engine");
			break;
		}

		case HAPI_LICENSE_HOUDINI:
		{
			LicenseType = TEXT("Houdini");
			break;
		}

		case HAPI_LICENSE_HOUDINI_FX:
		{
			LicenseType = TEXT("Houdini FX");
			break;
		}

		case HAPI_LICENSE_HOUDINI_ENGINE_INDIE:
		{
			LicenseType = TEXT("Houdini Engine Indie");
			break;
		}

		case HAPI_LICENSE_HOUDINI_INDIE:
		{
			LicenseType = TEXT("Houdini Indie");
			break;
		}

		case HAPI_LICENSE_HOUDINI_ENGINE_UNITY_UNREAL:
		{
			LicenseType = TEXT("Houdini Engine for Unity/Unreal");
			break;
		}

		case HAPI_LICENSE_HOUDINI_EDUCATION:
		{
			LicenseType = TEXT("Houdini Education");
			break;
		}

		case HAPI_LICENSE_HOUDINI_ENGINE_EDUCATION:
		{
			LicenseType = TEXT("Houdini Engine Education");
			break;
		}

		case HAPI_LICENSE_MAX:
		default:
		{
			return false;
		}
	}

	return true;
}

// Check if the cookable (or parent cookable) is being cooked
bool
FHoudiniEngineUtils::IsHoudiniCookableCooking(UObject* InObj)
{
	if (!InObj)
		return false;

	UHoudiniCookable* Cookable = nullptr;
	if (InObj->IsA<UHoudiniCookable>())
	{
		Cookable = Cast<UHoudiniCookable>(InObj);
	}
	else
	{
		Cookable = Cast<UHoudiniCookable>(InObj->GetOuter());
	}

	if (!Cookable)
		return false;

	EHoudiniAssetState AssetState = Cookable->GetCurrentState();
	return AssetState >= EHoudiniAssetState::PreCook && AssetState <= EHoudiniAssetState::PostCook;
}

void
FHoudiniEngineUtils::UpdateEditorProperties(const bool bInForceFullUpdate)
{
	if (!IsInGameThread())
	{
		// We need to be in the game thread to trigger editor properties update
		AsyncTask(ENamedThreads::GameThread, [bInForceFullUpdate]()
		{
			FHoudiniEngineUtils::UpdateEditorProperties_Internal(bInForceFullUpdate);
		});
	}
	else
	{
		// We're in the game thread, no need  for an async task
		FHoudiniEngineUtils::UpdateEditorProperties_Internal(bInForceFullUpdate);
	}
}

void FHoudiniEngineUtils::UpdateBlueprintEditor(UHoudiniAssetComponent* HAC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::UpdateBlueprintEditor);

	if (!IsInGameThread())
	{
		// We need to be in the game thread to trigger editor properties update
		AsyncTask(ENamedThreads::GameThread, [HAC]()
		{
			FHoudiniEngineUtils::UpdateBlueprintEditor_Internal(HAC);
		});
	}
	else
	{
		// We're in the game thread, no need  for an async task
		FHoudiniEngineUtils::UpdateBlueprintEditor_Internal(HAC);
	}
}

void
FHoudiniEngineUtils::UpdateEditorProperties_Internal(const bool bInForceFullUpdate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::UpdateEditorProperties_Internal);

#if WITH_EDITOR
#define HOUDINI_USE_DETAILS_FOCUS_HACK 1

	// TODO: As an optimization, it might be worth adding an extra parameter to control if we 
	// update floating property windows. We need to do this whenever we update something visible in
	// actor details, such as adding/removing a component.
	if (GUnrealEd)
	{
		GUnrealEd->UpdateFloatingPropertyWindows();
	}

	if (!bInForceFullUpdate)
	{
		// bNeedFullUpdate is false only when small changes (parameters value) have been made
		// We do not refresh the details view to avoid loosing the currently selected parameter
		return;
	}

	// Get the property editor module
	FPropertyEditorModule& PropertyModule =
		FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	//
	// We want to iterate over all the details panels.
	// Note that Unreal can have up to 4 of them open at once!
	//
	// TODO: These shouldn't be hardcoded strings, but when building on Mac, we get linking errors
	//       when trying to use LevelEditorTabIds::LevelEditorSelectionDetails
	//
	/*
	static const FName DetailsTabIdentifiers[] = {
		"LevelEditorSelectionDetails",
		"LevelEditorSelectionDetails2",
		"LevelEditorSelectionDetails3",
		"LevelEditorSelectionDetails4" };
	*/

	TArray<FName> DetailsTabIdentifiers;
	DetailsTabIdentifiers.SetNum(4);
	DetailsTabIdentifiers[0] = FName("LevelEditorSelectionDetails");
	DetailsTabIdentifiers[1] = FName("LevelEditorSelectionDetails2");
	DetailsTabIdentifiers[2] = FName("LevelEditorSelectionDetails3");
	DetailsTabIdentifiers[3] = FName("LevelEditorSelectionDetails4");

	// Add the Houdini Asset editor identifiers to the Details tab array
	{
		TArray<FName> AssetEditorId = FHoudiniEngine::Get().GetAllHoudiniAssetEditorIdentifier();
		for (auto CurId : AssetEditorId)
			DetailsTabIdentifiers.Add(CurId);
	}

	for (const FName DetailsPanelName : DetailsTabIdentifiers)
	{
		// Locate the details panel.
		TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);

		if (!DetailsView.IsValid())
		{
			// We have no details panel, nothing to update.
			continue;
		}

#if HOUDINI_USE_DETAILS_FOCUS_HACK
		//
		// Unreal does not maintain focus on the currently focused widget after refreshing the 
		// details view. Since we are constantly refreshing the details view when tweaking 
		// parameters, users cannot navigate the UI via keyboard.
		//
		// HACK: Attach meta data to parameter widgets to make them identifiable. Before triggering
		//       a refresh, save the meta data of the currently focused widget. Then restore focus
		//       on the newly created widget using this meta data.
		//

		TSharedPtr<FHoudiniParameterWidgetMetaData> ParameterWidgetMetaData = 
			GetFocusedParameterWidgetMetaData(DetailsView);
#endif // HOUDINI_USE_DETAILS_FOCUS_HACK

		DetailsView->ForceRefresh();

#if HOUDINI_USE_DETAILS_FOCUS_HACK
		if (ParameterWidgetMetaData.IsValid())
		{
			FocusUsingParameterWidgetMetaData(
				DetailsView.ToSharedRef(),
				*ParameterWidgetMetaData);
		}
#endif // HOUDINI_USE_DETAILS_FOCUS_HACK
	}
#endif // WITH_EDITOR
}

TSharedPtr<FHoudiniParameterWidgetMetaData> 
FHoudiniEngineUtils::GetFocusedParameterWidgetMetaData(TSharedPtr<IDetailsView> DetailsView)
{
#if WITH_EDITOR
	if (!DetailsView.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();

	if (FocusedWidget.IsValid())
	{
		// Before we grab the meta data of the focused widget, we want to make sure that it is
		// inside our details view. To do this, check if any of its ancestors are the current
		// details view.
		for (auto Widget = FocusedWidget; Widget.IsValid(); Widget = Widget->GetParentWidget())
		{
			if (Widget == DetailsView)
			{
				return FocusedWidget->GetMetaData<FHoudiniParameterWidgetMetaData>();
			}
		}
	}
#endif // WITH_EDITOR

	return nullptr;
}

bool 
FHoudiniEngineUtils::FocusUsingParameterWidgetMetaData(
	TSharedRef<SWidget> AncestorWidget, 
	const FHoudiniParameterWidgetMetaData& ParameterWidgetMetaData)
{
#if WITH_EDITOR
	//
	// HACK: Manually tick the widget before accessing its children. We need to do this because
	//       refreshing a details view will only mark the child detail tree as dirty, without
	//       actually adding the newly created widgets as children.
	//
	//       - See SDetailsViewBase::RefreshTree which requests the refresh.
	//       - See STableViewBase::Tick which actually does the refresh.
	//
	//       As a result, Slate cannot construct a path to the new widgets we wish to focus, since
	//       before the tick, the widgets in our detail rows do not have a parent.
	//
	//       Unfortunately there doesn't seem to be a way to subscribe to a "post tick" event in
	//       Slate, so we resort to manually ticking these widgets.
	//
	//       We could also manually tick the entire Slate application, however we cannot control the
	//       delta time this way and this introduces a delay before the new widget is re-focused.
	//
	//       We use cached widget geometry with the hope that it is correct.
	//
	AncestorWidget->Tick(AncestorWidget->GetTickSpaceGeometry(), 0.f, 0.f);

	// Important: We use GetAllChildren and not GetChildren. 
	// Widgets might choose to not expose some of their children via GetChildren.
	FChildren* const Children = AncestorWidget->GetAllChildren();

	for (int32 i = 0; i < Children->Num(); ++i)
	{
		const TSharedRef<SWidget> Child = Children->GetChildAt(i);
		const TSharedPtr ChildMetaData = Child->GetMetaData<FHoudiniParameterWidgetMetaData>();

		if (ChildMetaData.IsValid() && ParameterWidgetMetaData == *ChildMetaData)
		{
			TSharedPtr<SWidget> WidgetToSelect = Child;

			//
			// Try focus the desired widget.
			// - If this fails, it is possible that Slate cannot construct a path to it.
			// - However, usually the parent can be focused. 
			// - Thus we go over all ancestors to try focus them.
			//
			// TODO: Explore the possibility of constructing the path to the widget manually.
			//       Maybe this would allow focusing widgets that currently cannot not be focused.
			//
			while (WidgetToSelect.IsValid())
			{
				if (FSlateApplication::Get().SetKeyboardFocus(WidgetToSelect))
				{
					return true;
				}

				WidgetToSelect = Child->GetParentWidget();
			}

			return false; // Failed to reselect keyboard focused widget!

		}

		if (FocusUsingParameterWidgetMetaData(Child, ParameterWidgetMetaData))
		{
			return true;
		}
	}
#endif // WITH_EDITOR

	return false;
}

void
FHoudiniEngineUtils::UpdateBlueprintEditor_Internal(UHoudiniAssetComponent* HAC)
{
	FBlueprintEditor* BlueprintEditor = FHoudiniEngineRuntimeUtils::GetBlueprintEditor(HAC);
	if (!BlueprintEditor)
		return;

	TSharedPtr<SSubobjectEditor> SSubObjEditor = BlueprintEditor->GetSubobjectEditor();
	if (SSubObjEditor.IsValid())
	{
		SSubObjEditor->UpdateTree(true);
		SSubObjEditor->DumpTree();
	}
	BlueprintEditor->RefreshMyBlueprint();

	//BlueprintEditor->RefreshMyBlueprint();
	//BlueprintEditor->RefreshInspector();
	//BlueprintEditor->RefreshEditors();
			
	// Also somehow reselect ?
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeFloatData(
	const TArray<float>& InFloatData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
	bool bAttemptRunLengthEncoding)
{
	if (InFloatData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InFloatData);

    return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}


HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeFloatData(
	const float* InFloatData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
	bool bAttemptRunLengthEncoding)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = HAPI_RESULT_FAILURE;

	if (bAttemptRunLengthEncoding)
	{
		TArray<int> RunLengths = RunLengthEncode(InFloatData, InAttributeInfo.tupleSize, InAttributeInfo.count);
		if (RunLengths.Num() != 0)
		{
			for(int Index = 0; Index < RunLengths.Num(); Index++)
			{
				int StartIndex = RunLengths[Index];
				int EndIndex = InAttributeInfo.count;
				if (Index != RunLengths.Num() - 1)
					EndIndex = RunLengths[Index + 1];

				const float* TupleValues = &InFloatData[StartIndex * InAttributeInfo.tupleSize];
				Result = FHoudiniApi::SetAttributeFloatUniqueData(FHoudiniEngine::Get().GetSession(), InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, TupleValues, InAttributeInfo.tupleSize, StartIndex, EndIndex - StartIndex);

				if (Result != HAPI_RESULT_SUCCESS)
					return  Result;
			}
			return HAPI_RESULT_SUCCESS;
		}
	}

	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribte in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InFloatData + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
		Result = FHoudiniApi::SetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InFloatData,
			0, InAttributeInfo.count);
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeIntData(
	const TArray<int32>& InIntData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
    bool bAttemptRunLengthEncoding)
{
	if (InIntData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InIntData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeFloatUniqueData(
	const float InFloatData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = FHoudiniApi::SetAttributeFloatUniqueData(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		TCHAR_TO_ANSI(*InAttributeName), &InAttributeInfo, &InFloatData, InAttributeInfo.tupleSize,
		0, InAttributeInfo.count);

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeIntUniqueData(
	const int32 InIntData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = FHoudiniApi::SetAttributeIntUniqueData(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		TCHAR_TO_ANSI(*InAttributeName), &InAttributeInfo, &InIntData, InAttributeInfo.tupleSize,
		0, InAttributeInfo.count);

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeIntData(	
	const int32* InIntData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
	bool bAttemptRunLengthEncoding)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	if (bAttemptRunLengthEncoding)
	{
        TArray<int> RunLengths = RunLengthEncode(InIntData, InAttributeInfo.tupleSize, InAttributeInfo.count);
        if (RunLengths.Num() != 0)
        {
            for (int Index = 0; Index < RunLengths.Num(); Index++)
            {
                int StartIndex = RunLengths[Index];
                int EndIndex = InAttributeInfo.count / InAttributeInfo.tupleSize;
                if (Index != RunLengths.Num() - 1)
                    EndIndex = RunLengths[Index + 1];

                const int* TupleValues = &InIntData[StartIndex * InAttributeInfo.tupleSize];
                HAPI_Result Result = FHoudiniApi::SetAttributeIntUniqueData(
                    FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
                    TCHAR_TO_ANSI(*InAttributeName), &InAttributeInfo, TupleValues, InAttributeInfo.tupleSize,
                    StartIndex, EndIndex - StartIndex);

                if (Result != HAPI_RESULT_SUCCESS)
                    return Result;
            }
            return HAPI_RESULT_SUCCESS;
        }
	}

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribte in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeIntData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InIntData + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
		Result = FHoudiniApi::SetAttributeIntData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InIntData,
			0, InAttributeInfo.count);
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUIntData(
	const TArray<int64>& InIntData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InIntData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUIntData(
	const int64* InIntData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InIntData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeInt8Data(
	const TArray<int8>& InByteData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	if (InByteData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InByteData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeInt8Data(
	const int8* InByteData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribte in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeInt8Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InByteData + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
		Result = FHoudiniApi::SetAttributeInt8Data(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InByteData,
			0, InAttributeInfo.count);
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUInt8Data(
	const TArray<uint8>& InByteData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	if (InByteData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InByteData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;

}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUInt8Data(
	const uint8* InByteData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribte in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeUInt8Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InByteData + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
		Result = FHoudiniApi::SetAttributeUInt8Data(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InByteData,
			0, InAttributeInfo.count);
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeInt16Data(
	const TArray<int16>& InShortData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InShortData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InShortData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeInt16Data(
	const int16* InShortData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribte in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeInt16Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InShortData + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
		Result = FHoudiniApi::SetAttributeInt16Data(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InShortData,
			0, InAttributeInfo.count);
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUInt16Data(
	const TArray<int32>& InShortData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InShortData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUInt16Data(
	const int32* InShortData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InShortData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeInt64Data(
	const TArray<int64>& InInt64Data,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	if (InInt64Data.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InInt64Data);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeInt64Data(
	const int64* InInt64Data,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

#if PLATFORM_LINUX
	TArray<HAPI_Int64> HData;
	HData.Reserve(InAttributeInfo.count * InAttributeInfo.tupleSize);
	if (sizeof(int64) != sizeof(HAPI_Int64))
	{
		for (int32 Idx = 0; Idx < InAttributeInfo.count * InAttributeInfo.tupleSize; ++Idx)
		{
			HData.Add(static_cast<HAPI_Int64>(InInt64Data[Idx]));
		}
	}
#endif

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribute in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;
#if PLATFORM_LINUX
			if (sizeof(int64) != sizeof(HAPI_Int64))
			{
				Result = FHoudiniApi::SetAttributeInt64Data(
					FHoudiniEngine::Get().GetSession(),
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, HData.GetData() + ChunkStart * InAttributeInfo.tupleSize,
					ChunkStart, CurCount);
			}
			else
			{
				Result = FHoudiniApi::SetAttributeInt64Data(
					FHoudiniEngine::Get().GetSession(),
					InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
					&InAttributeInfo, reinterpret_cast<const HAPI_Int64*>(InInt64Data + ChunkStart * InAttributeInfo.tupleSize),
					ChunkStart, CurCount);
			}
#else
			Result = FHoudiniApi::SetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InInt64Data + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);
#endif
			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
#if PLATFORM_LINUX
		if (sizeof(int64) != sizeof(HAPI_Int64))
		{
			Result = FHoudiniApi::SetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, HData.GetData(),
				0, InAttributeInfo.count);
		}
		else
		{
			Result = FHoudiniApi::SetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, reinterpret_cast<const HAPI_Int64*>(InInt64Data),
				0, InAttributeInfo.count);
		}
#else
		Result = FHoudiniApi::SetAttributeInt64Data(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InInt64Data,
			0, InAttributeInfo.count);
#endif
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeUInt64Data(
	const TArray<int64>& InInt64Data,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InInt64Data);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeDoubleData(
	const TArray<double>& InDoubleData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	if (InDoubleData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName));
	bool bSuccess = Accessor.SetAttributeData(InAttributeInfo, InDoubleData);

	return bSuccess ? HAPI_RESULT_SUCCESS : HAPI_RESULT_FAILURE;

}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeDoubleData(
	const double* InDoubleData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	if (InAttributeInfo.count <= 0 || InAttributeInfo.tupleSize < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE / InAttributeInfo.tupleSize;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Send the attribte in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeFloat64Data(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InDoubleData + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Send all the attribute values once
		Result = FHoudiniApi::SetAttributeFloat64Data(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, InDoubleData,
			0, InAttributeInfo.count);
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetVertexList(
	const TArray<int32>& InVertexListData,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId)
{
    H_SCOPED_FUNCTION_TIMER();

	int32 ListNum = InVertexListData.Num();
	if (ListNum < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;
		
	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE;
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (ListNum > ChunkSize)
	{
		// Send the vertex list in chunks
		for (int32 ChunkStart = 0; ChunkStart < ListNum; ChunkStart += ChunkSize)
		{
			int32 CurCount = ListNum - ChunkStart > ChunkSize ? ChunkSize : ListNum - ChunkStart;
			Result = FHoudiniApi::SetVertexList(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, InVertexListData.GetData() + ChunkStart, ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		Result = FHoudiniApi::SetVertexList(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, InVertexListData.GetData(), 0, InVertexListData.Num());
	}

	return Result;
}


HAPI_Result
FHoudiniEngineUtils::HapiSetFaceCounts(
	const TArray<int32>& InFaceCounts,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId)
{
    H_SCOPED_FUNCTION_TIMER();

	int32 FaceCountsNum = InFaceCounts.Num();
	if (FaceCountsNum < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE;
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (FaceCountsNum > ChunkSize)
	{
		// Send the vertex list in chunks
		for (int32 ChunkStart = 0; ChunkStart < FaceCountsNum; ChunkStart += ChunkSize)
		{
			int32 CurCount = FaceCountsNum - ChunkStart > ChunkSize ? ChunkSize : FaceCountsNum - ChunkStart;
			Result = FHoudiniApi::SetFaceCounts(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, InFaceCounts.GetData() + ChunkStart, ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		Result = FHoudiniApi::SetFaceCounts(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, InFaceCounts.GetData(), 0, InFaceCounts.Num());
	}

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeStringUniqueData(
	const FString& InString,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    return FHoudiniApi::SetAttributeStringUniqueData(
        FHoudiniEngine::Get().GetSession(),
        InNodeId,
        InPartId,
        TCHAR_TO_ANSI(*InAttributeName),
        &InAttributeInfo,
        TCHAR_TO_ANSI(*InString),
        InAttributeInfo.tupleSize,
        0,
        InAttributeInfo.count);
}


HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeStringMap(
	const FHoudiniEngineIndexedStringMap& InIndexedStringMap,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	FHoudiniEngineRawStrings IndexedRawStrings = InIndexedStringMap.GetRawStrings();
	TArray<int> IndexArray = InIndexedStringMap.GetIds();

	HAPI_Result Result = FHoudiniApi::SetAttributeIndexedStringData(
	    FHoudiniEngine::Get().GetSession(),
		InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
		&InAttributeInfo, IndexedRawStrings.RawStrings.GetData(), IndexedRawStrings.RawStrings.Num(), IndexArray.GetData(), 0, IndexArray.Num());

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeStringData(
	const TArray<FString>& InStringArray, 
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo )
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	TArray<const char *> StringDataArray;
	for (const auto& CurrentString : InStringArray)
	{
		// Append the converted string to the string array
		StringDataArray.Add(FHoudiniEngineUtils::ExtractRawString(CurrentString));
	}

	// Send strings in smaller chunks due to their potential size
	int32 ChunkSize = (THRIFT_MAX_CHUNKSIZE / 100) / InAttributeInfo.tupleSize;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Set the attributes in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;

			Result = FHoudiniApi::SetAttributeStringData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, StringDataArray.GetData() + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Set all the attribute values once
		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, StringDataArray.GetData(),
			0, InAttributeInfo.count);
	}

	// ExtractRawString allocates memory using malloc, free it!
	FreeRawStringMemory(StringDataArray);

	return Result;
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeStringArrayData(
	const TArray<FString>& InStringArray,
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo,
	const TArray<int>& SizesFixedArray)
{
    H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	TArray<const char*> StringDataArray;
	for (const auto& CurrentString : InStringArray)
	{
		// Append the converted string to the string array
		StringDataArray.Add(FHoudiniEngineUtils::ExtractRawString(CurrentString));
	}

	// Send strings in smaller chunks due to their potential size
	int32 ChunkSize = (THRIFT_MAX_CHUNKSIZE / 100) / InAttributeInfo.tupleSize;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Set the attributes in chunks
		int32 StringStart = 0;
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			int32 CurCount = SizesFixedArray.Num() - ChunkStart > ChunkSize ? ChunkSize : SizesFixedArray.Num() - ChunkStart;
			int32 NumSent = 0;
			for (int32 Idx = 0; Idx < CurCount; ++Idx)
			{
				NumSent += SizesFixedArray[Idx + ChunkStart * InAttributeInfo.tupleSize];
			}

			Result = FHoudiniApi::SetAttributeStringArrayData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, StringDataArray.GetData() + StringStart, NumSent,
				SizesFixedArray.GetData() + ChunkStart * InAttributeInfo.tupleSize, ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;

			StringStart += NumSent;
		}
	}
	else
	{
		// Set all the attribute values once
		Result = FHoudiniApi::SetAttributeStringArrayData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, StringDataArray.GetData(), StringDataArray.Num(),
			SizesFixedArray.GetData(),0, SizesFixedArray.Num());
	}

	// ExtractRawString allocates memory using malloc, free it!
	FreeRawStringMemory(StringDataArray);

	return Result;
}


HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeDictionaryData(const TArray<FString>& JSONData,
	HAPI_NodeId InNodeId, HAPI_PartId InPartId, const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(InAttributeName);

	TArray<const char *> RawStringData;
	for (const FString& Data : JSONData)
	{
		RawStringData.Add(FHoudiniEngineUtils::ExtractRawString(Data));
	}

	// Send strings in smaller chunks due to their potential size
	int32 ChunkSize = (THRIFT_MAX_CHUNKSIZE / 100) / InAttributeInfo.tupleSize;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (InAttributeInfo.count > ChunkSize)
	{
		// Set the attributes in chunks
		for (int32 ChunkStart = 0; ChunkStart < InAttributeInfo.count; ChunkStart += ChunkSize)
		{
			const int32 CurCount = InAttributeInfo.count - ChunkStart > ChunkSize ? ChunkSize : InAttributeInfo.count - ChunkStart;
	
			Result = FHoudiniApi::SetAttributeDictionaryData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, RawStringData.GetData() + ChunkStart * InAttributeInfo.tupleSize,
				ChunkStart, CurCount);
	
			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		// Set all the attribute values once
		Result = FHoudiniApi::SetAttributeDictionaryData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
			&InAttributeInfo, RawStringData.GetData(),
			0, RawStringData.Num());
	}

	// ExtractRawString allocates memory using malloc, free it!
	FreeRawStringMemory(RawStringData);

	return Result;
}


HAPI_Result
FHoudiniEngineUtils::HapiSetHeightFieldData(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	const TArray<float>& InFloatValues,
	const FString& InHeightfieldName)
{
    H_SCOPED_FUNCTION_TIMER();

	int32 NumValues = InFloatValues.Num();
	if (NumValues < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	// Get the volume name as std::string
	std::string NameStr;
	FHoudiniEngineUtils::ConvertUnrealString(InHeightfieldName, NameStr);

	// Get the Heighfield float data
	const float* HeightData = InFloatValues.GetData();

	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE;
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (NumValues > ChunkSize)
	{
		// Send the heightfield data in chunks
		for (int32 ChunkStart = 0; ChunkStart < NumValues; ChunkStart += ChunkSize)
		{
			int32 CurCount = NumValues - ChunkStart > ChunkSize ? ChunkSize : NumValues - ChunkStart;
			
			Result = FHoudiniApi::SetHeightFieldData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, NameStr.c_str(), &HeightData[ChunkStart], ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		Result = FHoudiniApi::SetHeightFieldData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, NameStr.c_str(), HeightData, 0, InFloatValues.Num());
	}

	return Result;
}


char *
FHoudiniEngineUtils::ExtractRawString(const FString& InString)
{
	// Return an empty string instead of returning null to avoid potential crashes
	std::string ConvertedString("");
	if (!InString.IsEmpty())
		ConvertedString = H_TCHAR_TO_UTF8(*InString);

	// Allocate space for unique string.
	int32 UniqueStringBytes = ConvertedString.size() + 1;
	char * UniqueString = static_cast<char *>(FMemory::Malloc(UniqueStringBytes));

	FMemory::Memzero(UniqueString, UniqueStringBytes);
	FMemory::Memcpy(UniqueString, ConvertedString.c_str(), ConvertedString.size());

	return UniqueString;
}

void
FHoudiniEngineUtils::FreeRawStringMemory(const char*& InRawString)
{
	if (InRawString == nullptr)
		return;

	// Do not attempt to free empty strings!
	if (!InRawString[0])
		return;

	FMemory::Free((void*)InRawString);
	InRawString = nullptr;
}

void
FHoudiniEngineUtils::FreeRawStringMemory(TArray<const char*>& InRawStringArray)
{
	// ExtractRawString allocates memory using malloc, free it!
	for (auto CurrentStrPtr : InRawStringArray)
	{
		FreeRawStringMemory(CurrentStrPtr);
	}
	InRawStringArray.Empty();
}

bool
FHoudiniEngineUtils::AddHoudiniLogoToComponent(USceneComponent* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	// No need to add another component if we already show the logo
	if (FHoudiniEngineUtils::HasHoudiniLogo(InComponent))
		return true;

	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	UStaticMeshComponent * HoudiniLogoSMC = NewObject<UStaticMeshComponent>(
		InComponent, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);

	if (!HoudiniLogoSMC)
		return false;

	HoudiniLogoSMC->SetStaticMesh(HoudiniLogoSM);
	HoudiniLogoSMC->SetVisibility(true);
	HoudiniLogoSMC->SetHiddenInGame(true);
	// Attach created static mesh component to our Houdini component.
	HoudiniLogoSMC->AttachToComponent(InComponent, FAttachmentTransformRules::KeepRelativeTransform);
	HoudiniLogoSMC->RegisterComponent();

	return true;
}

bool
FHoudiniEngineUtils::RemoveHoudiniLogoFromComponent(USceneComponent* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	// Get the Houdini Logo SM
	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : InComponent->GetAttachChildren())
	{
		if (!IsValid(CurrentSceneComp) || !CurrentSceneComp->IsA<UStaticMeshComponent>())
			continue;

		// Get the static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CurrentSceneComp);
		if (!IsValid(SMC))
			continue;

		// Check if the SMC is the Houdini Logo
		if (SMC->GetStaticMesh() != HoudiniLogoSM)
			continue;

		SMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SMC->UnregisterComponent();
		SMC->DestroyComponent();

		return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::HasHoudiniLogo(USceneComponent* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	// Get the Houdini Logo SM
	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : InComponent->GetAttachChildren())
	{
		if (!IsValid(CurrentSceneComp) || !CurrentSceneComp->IsA<UStaticMeshComponent>())
			continue;

		// Get the static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CurrentSceneComp);
		if (!IsValid(SMC))
			continue;

		// Check if the SMC is the Houdini Logo
		if (SMC->GetStaticMesh() == HoudiniLogoSM)
			return true;
	}

	return false;
}

int32
FHoudiniEngineUtils::HapiGetVertexListForGroup(
	HAPI_NodeId GeoId,
	const HAPI_PartInfo& PartInfo,
	const FString& GroupName,
	const TArray<int32>& FullVertexList,
	TArray<int32>& NewVertexList,
	TArray<int32>& UsedVertices,
	TArray<int32>& AllFaceList,
	TArray<int32>& AllGroupFaceIndices,
	int32& FirstValidVertex,
	int32& FirstValidPrim,
	bool isPackedPrim)
{
	int32 ProcessedWedges = 0;
	AllFaceList.Empty();
	FirstValidPrim = 0;
	FirstValidVertex = 0;

	NewVertexList.SetNumUninitialized(FullVertexList.Num());
	for(int32 n = 0; n < NewVertexList.Num(); n++)
		NewVertexList[n] = -1;

	// Get the faces membership for this group
	bool bAllEquals = false;
	TArray<int32> PartGroupMembership;
	if (!FHoudiniEngineUtils::HapiGetGroupMembership(
		GeoId, PartInfo, HAPI_GROUPTYPE_PRIM, GroupName, PartGroupMembership, bAllEquals))
		return false;

	// Go through all primitives.
	for (int32 FaceIdx = 0; FaceIdx < PartGroupMembership.Num(); ++FaceIdx)
	{
		if (PartGroupMembership[FaceIdx] <= 0)
		{
			// The face is not in the group, skip
			continue;
		}
		
		// Add the face's index.
		AllFaceList.Add(FaceIdx);

		// Get the index of this face's vertices
		int32 FirstVertexIdx = FaceIdx * 3;
		int32 SecondVertexIdx = FirstVertexIdx + 1;
		int32 LastVertexIdx = FirstVertexIdx + 2;

		// This face is a member of specified group.
		// Add all 3 vertices
		if (FullVertexList.IsValidIndex(LastVertexIdx))
		{
			NewVertexList[FirstVertexIdx] = FullVertexList[FirstVertexIdx];
			NewVertexList[SecondVertexIdx] = FullVertexList[SecondVertexIdx];
			NewVertexList[LastVertexIdx] = FullVertexList[LastVertexIdx];
		}

		// Mark these vertex indices as used.
		if (UsedVertices.IsValidIndex(LastVertexIdx))
		{
			UsedVertices[FirstVertexIdx] = 1;
			UsedVertices[SecondVertexIdx] = 1;
			UsedVertices[LastVertexIdx] = 1;
		}

		// Mark this face as used.
		if (AllGroupFaceIndices.IsValidIndex(FaceIdx))
			AllGroupFaceIndices[FaceIdx] = 1;

		if (ProcessedWedges == 0)
		{
			// Keep track of the first valid vertex/face indices for this group
			// This will be useful later on when extracting attributes
			FirstValidVertex = FirstVertexIdx;
			FirstValidPrim = FaceIdx;
		}

		ProcessedWedges += 3;
	}

	return ProcessedWedges;
}

bool
FHoudiniEngineUtils::HapiGetGroupNames(
	const HAPI_NodeId GeoId, const HAPI_PartId PartId,
	const HAPI_GroupType GroupType, const bool isPackedPrim,
	TArray<FString>& OutGroupNames)
{
	int32 GroupCount = 0;
	if (!isPackedPrim)
	{
		// Get group count on the geo
		HAPI_GeoInfo GeoInfo;
		FHoudiniApi::GeoInfo_Init(&GeoInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
			FHoudiniEngine::Get().GetSession(), GeoId, &GeoInfo), false);

		if (GroupType == HAPI_GROUPTYPE_POINT)
			GroupCount = GeoInfo.pointGroupCount;
		else if (GroupType == HAPI_GROUPTYPE_PRIM)
			GroupCount = GeoInfo.primitiveGroupCount;
	}
	else
	{
		// We need the group count for this packed prim
		int32 PointGroupCount = 0, PrimGroupCount = 0;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupCountOnPackedInstancePart(
			FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PointGroupCount, &PrimGroupCount), false);

		if (GroupType == HAPI_GROUPTYPE_POINT)
			GroupCount = PointGroupCount;
		else if (GroupType == HAPI_GROUPTYPE_PRIM)
			GroupCount = PrimGroupCount;
	}

	if (GroupCount <= 0)
		return true;

	TArray<int32> GroupNameStringHandles;
	GroupNameStringHandles.SetNumZeroed(GroupCount);
	if (!isPackedPrim)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupNames(
			FHoudiniEngine::Get().GetSession(),
			GeoId, GroupType, &GroupNameStringHandles[0], GroupCount), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupNamesOnPackedInstancePart(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, GroupType, &GroupNameStringHandles[0], GroupCount), false);
	}
	
	/*
	OutGroupNames.SetNum(GroupCount);
	for (int32 NameIdx = 0; NameIdx < GroupCount; ++NameIdx)
	{
		FString CurrentGroupName = TEXT("");
		FHoudiniEngineString::ToFString(GroupNameStringHandles[NameIdx], CurrentGroupName);
		OutGroupNames[NameIdx] = CurrentGroupName;
	}
	*/

	FHoudiniEngineString::SHArrayToFStringArray(GroupNameStringHandles, OutGroupNames);

	return true;
}

bool FHoudiniEngineUtils::HapiGetGroupMembership(
	HAPI_NodeId GeoId, const HAPI_PartId PartId,
	const HAPI_GroupType& GroupType, const FString& GroupName,
	int32 & OutGroupMembership, int Start, int Length)
{
	OutGroupMembership = 0;

	std::string ConvertedGroupName = H_TCHAR_TO_UTF8(*GroupName);

	bool AllEqual;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembership(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, GroupType, ConvertedGroupName.c_str(),
			&AllEqual, &OutGroupMembership, Start, Length), false);

	return true;
}

bool
FHoudiniEngineUtils::HapiGetGroupMembership(
	HAPI_NodeId GeoId, const HAPI_PartInfo& PartInfo,
	const HAPI_GroupType& GroupType, const FString & GroupName,
	TArray<int32>& OutGroupMembership, bool& OutAllEquals)
{
	int32 ElementCount = (GroupType == HAPI_GROUPTYPE_POINT) ? PartInfo.pointCount : PartInfo.faceCount;	
	if (ElementCount < 1)
		return false;
	OutGroupMembership.SetNum(ElementCount);

	OutAllEquals = false;
	std::string ConvertedGroupName = H_TCHAR_TO_UTF8(*GroupName);
	if (!PartInfo.isInstanced)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembership(
			FHoudiniEngine::Get().GetSession(), 
			GeoId, PartInfo.id, GroupType,ConvertedGroupName.c_str(),
			&OutAllEquals, &OutGroupMembership[0], 0, ElementCount), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembershipOnPackedInstancePart(
			FHoudiniEngine::Get().GetSession(), GeoId, PartInfo.id, GroupType,
			ConvertedGroupName.c_str(), &OutAllEquals, &OutGroupMembership[0], 0, ElementCount), false);
	}

	return true;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& InAttributeInfo,
	TArray<FString>& OutData,
	int32 InStartIndex,
	int32 InCount)
{
	if (!InAttributeInfo.exists)
		return false;

	// Handle partial reading of attributes
	int32 Start = 0;
	if (InStartIndex > 0 && InStartIndex < InAttributeInfo.count)
		Start = InStartIndex;

	int32 Count = InAttributeInfo.count;
	if (InCount > 0)
	{
		if ((Start + InCount) <= InAttributeInfo.count)
			Count = InCount;
		else
			Count = InAttributeInfo.count - Start;
	}

	// Extract the StringHandles
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNumUninitialized(Count * InAttributeInfo.tupleSize);
	for (int32 n = 0; n < StringHandles.Num(); n++)
		StringHandles[n] = -1;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(
		FHoudiniEngine::Get().GetSession(),
		InGeoId, InPartId, InAttribName, 
		&InAttributeInfo, &StringHandles[0], 
		Start, Count), false);

	// Set the output data size
	OutData.SetNum(StringHandles.Num());

	// Convert the StringHandles to FString.
	// using a map to minimize the number of HAPI calls
	FHoudiniEngineString::SHArrayToFStringArray(StringHandles, OutData);
	
	return true;
}


bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
	HAPI_NodeId GeoId, HAPI_PartId PartId,
	const char * AttribName, HAPI_AttributeOwner Owner)
{
	if (Owner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 OwnerIdx = 0; OwnerIdx < HAPI_ATTROWNER_MAX; OwnerIdx++)
		{
			if (HapiCheckAttributeExists(GeoId, PartId, AttribName, (HAPI_AttributeOwner)OwnerIdx))
			{
				return true;
			}
		}
	}
	else
	{
		HAPI_AttributeInfo AttribInfo;
		FHoudiniApi::AttributeInfo_Init(&AttribInfo);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttribName, Owner, &AttribInfo), false);

		return AttribInfo.exists;
	}

	return false;
}

bool
FHoudiniEngineUtils::IsAttributeInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, EHoudiniInstancerType& OutInstancerType)
{
	// Check for 
	// - HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE (unreal_instance) on points/detail
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, HAPI_ATTROWNER_POINT))
	{
		OutInstancerType = EHoudiniInstancerType::AttributeInstancer;
		return true;
	}

	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, HAPI_ATTROWNER_DETAIL))
	{
		OutInstancerType = EHoudiniInstancerType::AttributeInstancer;
		return true;
	}

	// - HAPI_UNREAL_ATTRIB_INSTANCE (instance) on points
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE, HAPI_ATTROWNER_POINT))
	{
		OutInstancerType = EHoudiniInstancerType::OldSchoolAttributeInstancer;
		return true;
	}

	return false;
}

bool 
FHoudiniEngineUtils::IsValidDataTable(HAPI_NodeId GeoId, HAPI_PartId PartId)
{
	HAPI_PartInfo PartInfo;
	HAPI_Result Error = FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(),
		GeoId, PartId, &PartInfo);
	if (Error != HAPI_RESULT_SUCCESS)
	{
		return false;
	}
	TArray<HAPI_StringHandle> AttribNameHandles;
	AttribNameHandles.SetNum(PartInfo.attributeCounts[HAPI_ATTROWNER_POINT]);
	Error = FHoudiniApi::GetAttributeNames(FHoudiniEngine::Get().GetSession(),
		GeoId,
		PartId,
		HAPI_ATTROWNER_POINT,
		AttribNameHandles.GetData(),
		PartInfo.attributeCounts[HAPI_ATTROWNER_POINT]);
	if (Error != HAPI_RESULT_SUCCESS)
	{
		return false;
	}
	TArray<FString> AttribNames;
	FHoudiniEngineString::SHArrayToFStringArray(AttribNameHandles, AttribNames);
	for (const FString & Name : AttribNames)
	{
		if (Name.StartsWith(HAPI_UNREAL_ATTRIB_DATA_TABLE_PREFIX) && Name != HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWNAME && Name != HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWSTRUCT)
		{
			return true;
		}
	}

	return false;
}

bool
FHoudiniEngineUtils::IsLandscapeSpline(HAPI_NodeId GeoId, HAPI_PartId PartId)
{
	// Check for 
	// - HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE on points/prim/detail with true/non-zero value
	TArray<int32> OutData;
	FHoudiniHapiAccessor Accessor(GeoId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1,  OutData, 0, 1);

	if (!bSuccess)
	{
		return false;
	}

	return OutData.Num() > 0 && static_cast<bool>(OutData[0]);
}

bool 
FHoudiniEngineUtils::IsValidHeightfield(HAPI_NodeId GeoId, HAPI_PartId PartId)
{
	// Make sure the volume is a heightfield by ensuring we have 
	// the "volvis" detail attribute
	return FHoudiniEngineUtils::HapiCheckAttributeExists(
		GeoId, PartId, "volvis", HAPI_ATTROWNER_DETAIL);
}

bool
FHoudiniEngineUtils::HapiGetParameterDataAsString(
	HAPI_NodeId NodeId, 
	const std::string& ParmName,
	const FString& DefaultValue,
	FString& OutValue)
{
	OutValue = DefaultValue;

	// Try to find the parameter by name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), false);
	
	if (ParmId < 0)
		return false;

	// Get the param info...
	HAPI_ParmInfo FoundParamInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParamInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParamInfo), false);

	// .. and value
	HAPI_StringHandle StringHandle;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmStringValues(
		FHoudiniEngine::Get().GetSession(), NodeId, false,
		&StringHandle, FoundParamInfo.stringValuesIndex, 1), false);

	// Convert the string handle to FString
	return FHoudiniEngineString::ToFString(StringHandle, OutValue);
}

bool 
FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
	HAPI_NodeId NodeId,
	const std::string& ParmName,
	int32 DefaultValue,
	int32& OutValue)
{
	OutValue = DefaultValue;	

	// Try to find the parameter by its name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), false);
		
	if (ParmId < 0)
		return false;

	// Get the param info...
	HAPI_ParmInfo FoundParmInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParmInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParmInfo), false);
	
	// .. and value
	int32 Value = DefaultValue;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIntValues(
		FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		FoundParmInfo.intValuesIndex, 1), false);

	OutValue = Value;

	return true;
}


bool
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(
	HAPI_NodeId NodeId,
	const std::string& ParmName,
	float DefaultValue,
	float& OutValue)
{
	OutValue = DefaultValue;

	// Try to find the parameter by its name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), false);

	if (ParmId < 0)
		return false;

	// Get the param info...
	HAPI_ParmInfo FoundParmInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParmInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParmInfo), false);

	// .. and value
	float Value = DefaultValue;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmFloatValues(
		FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		FoundParmInfo.floatValuesIndex, 1), false);

	OutValue = Value;

	return true;
}

HAPI_ParmId
FHoudiniEngineUtils::HapiFindParameterByName(HAPI_NodeId InNodeId, const std::string& InParmName, HAPI_ParmInfo& OutFoundParmInfo)
{
	// Try to find the parameter by its name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, InParmName.c_str(), &ParmId), -1);

	if (ParmId < 0)
		return -1;

	FHoudiniApi::ParmInfo_Init(&OutFoundParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, ParmId, &OutFoundParmInfo), -1);

	return ParmId;
}

HAPI_ParmId
FHoudiniEngineUtils::HapiFindParameterByTag(HAPI_NodeId InNodeId, const std::string& InParmTag, HAPI_ParmInfo& OutFoundParmInfo)
{
	// Try to find the parameter by its tag
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmWithTag(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, InParmTag.c_str(), &ParmId), -1);

	if (ParmId < 0)
		return -1;

	FHoudiniApi::ParmInfo_Init(&OutFoundParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, ParmId, &OutFoundParmInfo), -1);

	return ParmId;
}

int32
FHoudiniEngineUtils::HapiGetAttributeOfType(
	HAPI_NodeId GeoId,
	HAPI_NodeId PartId,
	const HAPI_AttributeOwner& AttributeOwner,
	const HAPI_AttributeTypeInfo& AttributeType,
	TArray<HAPI_AttributeInfo>& MatchingAttributesInfo,
	TArray<FString>& MatchingAttributesName)
{
	int32 NumberOfAttributeFound = 0;

	// Get the part infos
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId, &PartInfo), NumberOfAttributeFound);

	// Get All attribute names for that part
	int32 nAttribCount = PartInfo.attributeCounts[AttributeOwner];

	TArray<HAPI_StringHandle> AttribNameSHArray;
	AttribNameSHArray.SetNum(nAttribCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId, AttributeOwner,
		AttribNameSHArray.GetData(), nAttribCount), NumberOfAttributeFound);

	TArray<FString> AttribNameArray;
	FHoudiniEngineString::SHArrayToFStringArray(AttribNameSHArray, AttribNameArray);

	// Iterate on all the attributes, and get their part infos to get their type
	for (int32 Idx = 0; Idx < AttribNameArray.Num(); Idx++)
	{
		FString HapiString = AttribNameArray[Idx];

		// ... then the attribute info
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, H_TCHAR_TO_UTF8(*HapiString),
			AttributeOwner, &AttrInfo))
			continue;

		if (!AttrInfo.exists)
			continue;

		// ... check the type
		if (AttrInfo.typeInfo != AttributeType)
			continue;

		MatchingAttributesInfo.Add(AttrInfo);
		MatchingAttributesName.Add(HapiString);

		NumberOfAttributeFound++;
	}

	return NumberOfAttributeFound;
}

HAPI_PartInfo
FHoudiniEngineUtils::ToHAPIPartInfo(const FHoudiniPartInfo& InHPartInfo)
{
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);

	PartInfo.id = InHPartInfo.PartId;
	//PartInfo.nameSH = InHPartInfo.Name;

	switch (InHPartInfo.Type)
	{
		case EHoudiniPartType::Mesh:
			PartInfo.type = HAPI_PARTTYPE_MESH;
			break;
		case EHoudiniPartType::Curve:
			PartInfo.type = HAPI_PARTTYPE_CURVE;
			break;
		case EHoudiniPartType::Instancer:
			PartInfo.type = HAPI_PARTTYPE_INSTANCER;
			break;
		case EHoudiniPartType::Volume:
			PartInfo.type = HAPI_PARTTYPE_VOLUME;
			break;
		default:
		case EHoudiniPartType::Invalid:
			PartInfo.type = HAPI_PARTTYPE_INVALID;
			break;
	}

	PartInfo.faceCount = InHPartInfo.FaceCount;
	PartInfo.vertexCount = InHPartInfo.VertexCount;
	PartInfo.pointCount = InHPartInfo.PointCount;

	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = InHPartInfo.PointAttributeCounts;
	PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX] = InHPartInfo.VertexAttributeCounts;
	PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM] = InHPartInfo.PrimitiveAttributeCounts;
	PartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL] = InHPartInfo.DetailAttributeCounts;

	PartInfo.isInstanced = InHPartInfo.bIsInstanced;

	PartInfo.instancedPartCount = InHPartInfo.InstancedPartCount;
	PartInfo.instanceCount = InHPartInfo.InstanceCount;

	PartInfo.hasChanged = InHPartInfo.bHasChanged;

	return PartInfo;
}

int32
FHoudiniEngineUtils::AddMeshSocketsToArray_DetailAttribute(
	HAPI_NodeId GeoId,
	HAPI_PartId PartId,
	TArray< FHoudiniMeshSocket >& AllSockets,
	bool isPackedPrim)
{
	int32 FoundSocketCount = 0;

	// Attributes we are interested in:
	TArray<float> Positions;

	// Rotation
	bool bHasRotation = false;
	TArray<float> Rotations;

	// Scale
	bool bHasScale = false;
	TArray<float> Scales;

	// Socket Name
	bool bHasNames = false;
	TArray<FString> Names;

	// Socket Actor
	bool bHasActors = false;
	TArray<FString> Actors;

	// Socket Tags
	bool bHasTags = false;
	TArray<FString> Tags;

	// Lambda function for creating the socket and adding it to the array
	// Shared between the by Attribute / by Group methods	
	auto AddSocketToArray = [&](int32 PointIdx)
	{
		FHoudiniMeshSocket CurrentSocket;
		FVector currentPosition = FVector::ZeroVector;
		if (Positions.IsValidIndex(PointIdx * 3 + 2))
		{
			currentPosition.X = Positions[PointIdx * 3] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Y = Positions[PointIdx * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Z = Positions[PointIdx * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		FVector currentScale = FVector::OneVector;
		if (bHasScale && Scales.IsValidIndex(PointIdx * 3 + 2))
		{
			currentScale.X = Scales[PointIdx * 3];
			currentScale.Y = Scales[PointIdx * 3 + 2];
			currentScale.Z = Scales[PointIdx * 3 + 1];
		}

		FQuat currentRotation = FQuat::Identity;
		if (bHasRotation && Rotations.IsValidIndex(PointIdx * 4 + 3))
		{
			currentRotation.X = Rotations[PointIdx * 4];
			currentRotation.Y = Rotations[PointIdx * 4 + 2];
			currentRotation.Z = Rotations[PointIdx * 4 + 1];
			currentRotation.W = -Rotations[PointIdx * 4 + 3];
		}

		if (bHasNames && Names.IsValidIndex(PointIdx))
			CurrentSocket.Name = Names[PointIdx];

		if (bHasActors && Actors.IsValidIndex(PointIdx))
			CurrentSocket.Actor = Actors[PointIdx];

		if (bHasTags && Tags.IsValidIndex(PointIdx))
			CurrentSocket.Tag = Tags[PointIdx];

		// If the scale attribute wasn't set on all socket, we might end up
		// with a zero scale socket, avoid that.
		if (currentScale == FVector::ZeroVector)
			currentScale = FVector::OneVector;

		CurrentSocket.Transform.SetLocation(currentPosition);
		CurrentSocket.Transform.SetRotation(currentRotation);
		CurrentSocket.Transform.SetScale3D(currentScale);

		// We want to make sure we're not adding the same socket multiple times
		AllSockets.AddUnique(CurrentSocket);

		FoundSocketCount++;

		return true;
	};


	// Lambda function for reseting the arrays/attributes
	auto ResetArraysAndAttr = [&]()
	{
		Positions.Empty();

		bHasRotation = false;
		Rotations.Empty();

		bHasScale = false;
		Scales.Empty();

		bHasNames = false;
		Names.Empty();

		bHasActors = false;
		Actors.Empty();

		bHasTags = false;
		Tags.Empty();
	};

	//-------------------------------------------------------------------------
	// FIND SOCKETS BY DETAIL ATTRIBUTES
	//-------------------------------------------------------------------------

	int32 SocketIdx = 0;
	bool HasSocketAttributes = true;	
	while (HasSocketAttributes)
	{
		// Build the current socket's prefix
		FString SocketAttrPrefix = TEXT(HAPI_UNREAL_ATTRIB_MESH_SOCKET_PREFIX) + FString::FromInt(SocketIdx);

		// Reset the arrays and attributes
		ResetArraysAndAttr();

		// Retrieve position data.
		FString SocketPosAttr = SocketAttrPrefix + TEXT("_pos");

		FHoudiniHapiAccessor Accessor(GeoId, PartId, TCHAR_TO_ANSI(*SocketPosAttr));
		bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, Positions);
		if (!bSuccess)
		{
			// No need to keep looking for socket attributes
			HasSocketAttributes = false;
			break;
		}

		// Retrieve rotation data.
		FString SocketRotAttr = SocketAttrPrefix + TEXT("_rot");

		Accessor.Init(GeoId, PartId, TCHAR_TO_ANSI(*SocketRotAttr));
		if (Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, Rotations))
			bHasRotation = true;

		// Retrieve scale data.
		FString SocketScaleAttr = SocketAttrPrefix + TEXT("_scale");
		Accessor.Init(GeoId, PartId, TCHAR_TO_ANSI(*SocketScaleAttr));
		if (Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, Scales))
			bHasScale = true;

		// Retrieve mesh socket names.
		FString SocketNameAttr = SocketAttrPrefix + TEXT("_name");
		Accessor.Init(GeoId, PartId, TCHAR_TO_ANSI(*SocketNameAttr));
		bHasNames = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, Names);

		// Retrieve mesh socket actor.
		FString SocketActorAttr = SocketAttrPrefix + TEXT("_actor");
		Accessor.Init(GeoId, PartId, TCHAR_TO_ANSI(*SocketActorAttr));
		bHasActors = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, Actors);

		// Retrieve mesh socket tags.
		FString SocketTagAttr = SocketAttrPrefix + TEXT("_tag");
		Accessor.Init(GeoId, PartId, TCHAR_TO_ANSI(*SocketTagAttr));
		bHasTags = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, Tags);

		// Add the socket to the array
		AddSocketToArray(0);

		// Try to find the next socket
		SocketIdx++;
	}

	return FoundSocketCount;
}


int32
FHoudiniEngineUtils::AddMeshSocketsToArray_Group(
	HAPI_NodeId GeoId,
	HAPI_PartId PartId,
	TArray<FHoudiniMeshSocket>& AllSockets,
	bool isPackedPrim)
{
	TArray<float> Positions;
	bool bHasRotation = false;
	TArray<float> Rotations;
	bool bHasScale = false;
	TArray<float> Scales;
	bool bHasNormals = false;
	TArray<float> Normals;
	bool bHasNames = false;
	TArray<FString> Names;
	bool bHasActors = false;
	TArray<FString> Actors;
	bool bHasTags = false;
	TArray<FString> Tags;

	// Lambda function for creating the socket and adding it to the array
	// Shared between the by Attribute / by Group methods
	int32 FoundSocketCount = 0;
	auto AddSocketToArray = [&](int32 PointIdx)
	{
		FHoudiniMeshSocket CurrentSocket;
		FVector currentPosition = FVector::ZeroVector;
		if (Positions.IsValidIndex(PointIdx * 3 + 2))
		{
			currentPosition.X = Positions[PointIdx * 3] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Y = Positions[PointIdx * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Z = Positions[PointIdx * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		FVector currentScale = FVector::OneVector;
		if (bHasScale && Scales.IsValidIndex(PointIdx * 3 + 2))
		{
			currentScale.X = Scales[PointIdx * 3];
			currentScale.Y = Scales[PointIdx * 3 + 2];
			currentScale.Z = Scales[PointIdx * 3 + 1];
		}

		FQuat currentRotation = FQuat::Identity;
		if (bHasRotation && Rotations.IsValidIndex(PointIdx * 4 + 3))
		{
			currentRotation.X = Rotations[PointIdx * 4];
			currentRotation.Y = Rotations[PointIdx * 4 + 2];
			currentRotation.Z = Rotations[PointIdx * 4 + 1];
			currentRotation.W = -Rotations[PointIdx * 4 + 3];
		}
		else if (bHasNormals && Normals.IsValidIndex(PointIdx * 3 + 2))
		{
			FVector vNormal;
			vNormal.X = Normals[PointIdx * 3];
			vNormal.Y = Normals[PointIdx * 3 + 2];
			vNormal.Z = Normals[PointIdx * 3 + 1];

			if (vNormal != FVector::ZeroVector)
				currentRotation = FQuat::FindBetween(FVector::UpVector, vNormal);
		}

		if (bHasNames && Names.IsValidIndex(PointIdx))
			CurrentSocket.Name = Names[PointIdx];

		if (bHasActors && Actors.IsValidIndex(PointIdx))
			CurrentSocket.Actor = Actors[PointIdx];

		if (bHasTags && Tags.IsValidIndex(PointIdx))
			CurrentSocket.Tag = Tags[PointIdx];

		// If the scale attribute wasn't set on all socket, we might end up
		// with a zero scale socket, avoid that.
		if (currentScale == FVector::ZeroVector)
			currentScale = FVector::OneVector;

		CurrentSocket.Transform.SetLocation(currentPosition);
		CurrentSocket.Transform.SetRotation(currentRotation);
		CurrentSocket.Transform.SetScale3D(currentScale);

		// We want to make sure we're not adding the same socket multiple times
		AllSockets.AddUnique(CurrentSocket);

		FoundSocketCount++;

		return true;
	};


	// Lambda function for reseting the arrays/attributes
	auto ResetArraysAndAttr = [&]()
	{
		// Position
		Positions.Empty();

		// Rotation
		bHasRotation = false;
		Rotations.Empty();

		// Scale
		bHasScale = false;
		Scales.Empty();

		// When using socket groups, we can also get the sockets rotation from the normal
		bHasNormals = false;
		Normals.Empty();

		// Socket Name
		bHasNames = false;
		Names.Empty();

		// Socket Actor
		bHasActors = false;
		Actors.Empty();

		// Socket Tags
		bHasTags = false;
		Tags.Empty();
	};

	//-------------------------------------------------------------------------
	// FIND SOCKETS BY POINT GROUPS
	//-------------------------------------------------------------------------

	// Get object / geo group memberships for primitives.
	TArray<FString> GroupNames;
	if (!FHoudiniEngineUtils::HapiGetGroupNames(
		GeoId, PartId, HAPI_GROUPTYPE_POINT, isPackedPrim, GroupNames))
	{
		HOUDINI_LOG_MESSAGE(TEXT("GetMeshSocketList: Geo [%d] Part [%d] non-fatal error reading point group names"), GeoId, PartId);
	}

	// First, we want to make sure we have at least one socket group before continuing
	bool bHasSocketGroup = false;
	for (int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx)
	{
		const FString & GroupName = GroupNames[GeoGroupNameIdx];
		if (GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX_OLD, ESearchCase::IgnoreCase))
		{
			bHasSocketGroup = true;
			break;
		}
	}

	if (!bHasSocketGroup)
		return FoundSocketCount;

	// Get the part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PartInfo))
		return false;

	// Reset the data arrays and attributes
	ResetArraysAndAttr();	

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_POSITION);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Positions))
		return false;

	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_ROTATION);
	if (Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Rotations))
		bHasRotation = true;

	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NORMAL);
	if (Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Normals))
		bHasNormals = true;

	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_SCALE);
	if (Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Scales))
		bHasScale = true;

	// Retrieve mesh socket names.
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME);
	bHasNames = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Names);
	if (!bHasNames)
	{
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME_OLD);
		bHasNames = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Names);
	}

	//  Retrieve mesh actors
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR);
	bHasActors = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Actors);
	if (!bHasActors)
	{
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR_OLD);
		bHasActors = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Actors);
	}

	// Retrieve mesh socket tags.
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG);
	bHasTags = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Tags);
	if (!bHasTags)
	{
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG_OLD);
		bHasTags = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Tags);
	}

	// Extracting Sockets vertices
	for (int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx)
	{
		const FString & GroupName = GroupNames[GeoGroupNameIdx];
		if (!GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX, ESearchCase::IgnoreCase)
			&& !GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX_OLD, ESearchCase::IgnoreCase))
			continue;

		bool AllEquals = false;
		TArray< int32 > PointGroupMembership;
		FHoudiniEngineUtils::HapiGetGroupMembership(
			GeoId, PartInfo, HAPI_GROUPTYPE_POINT, GroupName, PointGroupMembership, AllEquals);

		// Go through all primitives.
		for (int32 PointIdx = 0; PointIdx < PointGroupMembership.Num(); ++PointIdx)
		{
			if (PointGroupMembership[PointIdx] == 0)
			{
				if (AllEquals)
					break;
				else
					continue;
			}

			// Add the corresponding socket to the array
			AddSocketToArray(PointIdx);
		}
	}

	return FoundSocketCount;
}

bool
FHoudiniEngineUtils::AddMeshSocketsToStaticMesh(
	UStaticMesh* StaticMesh,
	TArray<FHoudiniMeshSocket >& AllSockets,
	bool CleanImportSockets)
{
	if (!IsValid(StaticMesh))
		return false;

	// Remove the sockets from the previous cook!
	if (CleanImportSockets)
	{
		StaticMesh->Sockets.RemoveAll([=](UStaticMeshSocket* Socket) { return Socket ? Socket->bSocketCreatedAtImport : true; });
	}

	if (AllSockets.Num() <= 0)
		return true;

	// Having sockets with empty names can lead to various issues, so we'll create one now
	for (int32 Idx = 0; Idx < AllSockets.Num(); ++Idx) 
	{
		// Assign the unnamed sockets with default names
		if (AllSockets[Idx].Name.IsEmpty())
			AllSockets[Idx].Name = TEXT("Socket ") + FString::FromInt(Idx);
	}

	// ensure the socket names are unique. (Unreal will use the first socket if multiple socket have the same name)
	for (int32 Idx_i = 0; Idx_i < AllSockets.Num(); ++Idx_i) 
	{
		int32 Count = 0;
		for (int32 Idx_j = Idx_i + 1; Idx_j < AllSockets.Num(); ++Idx_j) 
		{
			if (AllSockets[Idx_i].Name.Equals(AllSockets[Idx_j].Name)) 
			{
				Count += 1;
				AllSockets[Idx_j].Name = AllSockets[Idx_j].Name + "_" + FString::FromInt(Count);
			}
		}
	}

	// Clear all the sockets of the output static mesh.
	StaticMesh->Sockets.Empty();

	for (int32 nSocket = 0; nSocket < AllSockets.Num(); nSocket++)
	{
		// Create a new Socket
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(StaticMesh);
		if (!IsValid(Socket))
			continue;

		Socket->RelativeLocation = AllSockets[nSocket].Transform.GetLocation();
		Socket->RelativeRotation = FRotator(AllSockets[nSocket].Transform.GetRotation());
		Socket->RelativeScale = AllSockets[nSocket].Transform.GetScale3D();
		Socket->SocketName = FName(*AllSockets[nSocket].Name);

		// Socket Tag
		FString Tag;
		if (!AllSockets[nSocket].Tag.IsEmpty())
			Tag = AllSockets[nSocket].Tag;

		// The actor will be stored temporarily in the socket's Tag as we need a StaticMeshComponent to add an actor to the socket
		Tag += TEXT("|") + AllSockets[nSocket].Actor;

		Socket->Tag = Tag;
		Socket->bSocketCreatedAtImport = true;

		StaticMesh->Sockets.Add(Socket);
	}

	return true;
}

bool
FHoudiniEngineUtils::CreateAttributesFromTags(
	HAPI_NodeId NodeId, 
	HAPI_PartId PartId,
	const TArray<FName>& Tags )
{
	if (Tags.Num() <= 0)
		return false;

	HAPI_Result Result = HAPI_RESULT_FAILURE;

	// Get the destination part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo), false);

	bool NeedToCommitGeo = false;
	for (int32 TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
	{
		FString TagString;
		Tags[TagIdx].ToString(TagString);
		SanitizeHAPIVariableName(TagString);
		
		// Create a primitive attribute for the tag
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

		AttributeInfo.count = PartInfo.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
		AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_NONE;

		FString AttributeName = TEXT(HAPI_UNREAL_ATTRIB_TAG_PREFIX) + FString::FromInt(TagIdx);
		AttributeName.RemoveSpacesInline();

		Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo);

		if (Result != HAPI_RESULT_SUCCESS)
			continue;

		TArray<const char*> TagStr;
		TagStr.Add(FHoudiniEngineUtils::ExtractRawString(TagString));

		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo,
			TagStr.GetData(), 0, AttributeInfo.count);

		if (HAPI_RESULT_SUCCESS == Result)
			NeedToCommitGeo = true;

		// Free memory for allocated by ExtractRawString
		FHoudiniEngineUtils::FreeRawStringMemory(TagStr);
	}

	return NeedToCommitGeo;
}

bool
FHoudiniEngineUtils::CreateGroupsFromTags(
	HAPI_NodeId NodeId,
	HAPI_PartId PartId, 
	const TArray<FName>& Tags )
{
	if (Tags.Num() <= 0)
		return false;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	
	// Get the destination part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(),	NodeId, PartId, &PartInfo), false);

	bool NeedToCommitGeo = false;
	for (int32 TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
	{
		FString TagString;
		Tags[TagIdx].ToString(TagString);
		SanitizeHAPIVariableName(TagString);
		
		const char * TagStr = FHoudiniEngineUtils::ExtractRawString(TagString);

		// Create a primitive group for this tag
		if ( HAPI_RESULT_SUCCESS == FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, TagStr) )
		{
			// Set the group's Memberships
			TArray<int> GroupArray;
			GroupArray.SetNumUninitialized(PartInfo.faceCount);
			for (int32 n = 0; n < GroupArray.Num(); n++)
				GroupArray[n] = 1;

			if ( HAPI_RESULT_SUCCESS == FHoudiniApi::SetGroupMembership(
				FHoudiniEngine::Get().GetSession(),
				NodeId, PartId, HAPI_GROUPTYPE_PRIM, TagStr,
				GroupArray.GetData(), 0, PartInfo.faceCount) )
			{
				NeedToCommitGeo = true;
			}
		}

		// Free memory allocated by ExtractRawString()
		FHoudiniEngineUtils::FreeRawStringMemory(TagStr);
	}

	return NeedToCommitGeo;
}

/*
bool
FHoudiniEngineUtils::ForceValidVariableNameInline(FString& InOutString)
{
	// Reproduces the behaviour of UT_String::forceValidVariableName()
	// Return true if a change occurred
	bool bHasChanged = false;

	// Replace any special character by `_`
	for (TCHAR& CurrentChar : InOutString)
	{
		if (!FChar::IsAlnum(CurrentChar) && !FChar::IsUnderscore(CurrentChar))
		{
			CurrentChar = '_';
			bHasChanged = true;
		}
	}

	// If the string starts with a digit, clean it up by prefixing an _.
	if (FChar::IsDigit(InOutString[0]))
	{
		InOutString.InsertAt(0, '_');
		bHasChanged = true;
	}

	return bHasChanged;
}
*/

bool
FHoudiniEngineUtils::SanitizeHAPIVariableName(FString& String)
{
	// Only keep alphanumeric characters, underscores
	// Also, if the first character is a digit, append an underscore at the beginning
	TArray<TCHAR>& StrArray = String.GetCharArray();
	if (StrArray.Num() <= 0)
		return false;

	bool bHasChanged = false;
	for (auto& CurChar : StrArray)
	{
		const bool bIsValid = (CurChar >= TEXT('A') && CurChar <= TEXT('Z'))
			|| (CurChar >= TEXT('a') && CurChar <= TEXT('z'))
			|| (CurChar >= TEXT('0') && CurChar <= TEXT('9'))
			|| (CurChar == TEXT('_')) || (CurChar == TEXT('\0'));

		if(bIsValid)
			continue;

		CurChar = TEXT('_');
		bHasChanged = true;
	}

	if (StrArray.Num() > 0)
	{
		TCHAR FirstChar = StrArray[0];
		if (FirstChar >= TEXT('0') && FirstChar <= TEXT('9'))
		{
			StrArray.Insert(TEXT('_'), 0);
			bHasChanged = true;
		}
	}

	return bHasChanged;
}


int32
FHoudiniEngineUtils::GetGenericAttributeList(
	HAPI_NodeId InGeoNodeId,
	HAPI_PartId InPartId,
	const FString& InGenericAttributePrefix,
	TArray<FHoudiniGenericAttribute>& OutFoundAttributes,
	const HAPI_AttributeOwner& AttributeOwner,
	int32 InAttribIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GetGenericAttributeList);
	
	// Get the part info to get the attribute counts for the specified owner
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), InGeoNodeId, InPartId, &PartInfo), false);
	
	int32 nAttribCount = PartInfo.attributeCounts[AttributeOwner];

	// Get all attribute names for that part
	TArray<HAPI_StringHandle> AttribNameSHArray;
	AttribNameSHArray.SetNum(nAttribCount);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeNames(
		FHoudiniEngine::Get().GetSession(),
		InGeoNodeId, InPartId, AttributeOwner,
		AttribNameSHArray.GetData(), nAttribCount))
	{
		return 0;
	}	

	// For everything but detail attribute,
	// if an attribute index was specified, only extract the attribute value for that specific index
	// if not, extract all values for the given attribute
	bool HandleSplit = false;
	int32 AttribIndex = -1;
	if ((AttributeOwner != HAPI_ATTROWNER_DETAIL) && (InAttribIndex != -1))
	{
		// The index has already been specified so we'll use it
		HandleSplit = true;
		AttribIndex = InAttribIndex;
	}

	int32 FoundCount = 0;
	for (int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx)
	{
		int32 AttribNameSH = (int32)AttribNameSHArray[Idx];
		FString AttribName = TEXT("");
		FHoudiniEngineString::ToFString(AttribNameSH, AttribName);
		if (!AttribName.StartsWith(InGenericAttributePrefix, ESearchCase::IgnoreCase))
			continue;

		// Get the Attribute Info
		HAPI_AttributeInfo AttribInfo;
		FHoudiniApi::AttributeInfo_Init(&AttribInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InGeoNodeId, InPartId,
			H_TCHAR_TO_UTF8(*AttribName), AttributeOwner, &AttribInfo))
		{
			// failed to get that attribute's info
			continue;
		}

		int32 AttribStart = 0;
		int32 AttribCount = AttribInfo.count;
		if (HandleSplit)
		{
			// For split primitives, we need to only get only one value for the proper split prim
			// Make sure that the split index is valid
			if (AttribIndex >= 0 && AttribIndex < AttribInfo.count)
			{
				AttribStart = AttribIndex;
				AttribCount = 1;
			}
		}
		
		//
		FHoudiniGenericAttribute CurrentGenericAttribute;
		// Remove the generic attribute prefix
		CurrentGenericAttribute.AttributeName = AttribName.Right(AttribName.Len() - InGenericAttributePrefix.Len());

		CurrentGenericAttribute.AttributeOwner = (EAttribOwner)AttribInfo.owner;

		// Get the attribute type and tuple size
		CurrentGenericAttribute.AttributeType = (EAttribStorageType)AttribInfo.storage;
		CurrentGenericAttribute.AttributeCount = AttribInfo.count;
		CurrentGenericAttribute.AttributeTupleSize = AttribInfo.tupleSize;

		if (CurrentGenericAttribute.AttributeType == EAttribStorageType::FLOAT64)
		{
			// Initialize the value array
			CurrentGenericAttribute.DoubleValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeFloat64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				H_TCHAR_TO_UTF8(*AttribName), &AttribInfo, 0,
				CurrentGenericAttribute.DoubleValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}
		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::FLOAT)
		{
			// Initialize the value array
			TArray<float> FloatValues;
			FloatValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				H_TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				0, FloatValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}

			// Convert them to double
			CurrentGenericAttribute.DoubleValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);
			for (int32 n = 0; n < FloatValues.Num(); n++)
				CurrentGenericAttribute.DoubleValues[n] = (double)FloatValues[n];

		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::INT64)
		{
#if PLATFORM_LINUX
			// On Linux, we unfortunately cannot guarantee that int64 and HAPI_Int64
			// are of the same type, to properly read the value, we must first check the 
			// size, then either cast them (if sizes match) or convert the values (if sizes don't match)
			if (sizeof(int64) != sizeof(HAPI_Int64))
			{
				// int64 and HAPI_Int64 are of different size, we need to cast
				TArray<HAPI_Int64> HAPIIntValues;
				HAPIIntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

				// Get the value(s)
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInt64Data(
					FHoudiniEngine::Get().GetSession(),
					InGeoNodeId, InPartId,
					H_TCHAR_TO_UTF8(*AttribName), &AttribInfo,
					0, HAPIIntValues.GetData(),
					AttribStart, AttribCount))
				{
					// failed to get that attribute's data
					continue;
				}

				// Convert them to int64
				CurrentGenericAttribute.IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);
				for (int32 n = 0; n < HAPIIntValues.Num(); n++)
					CurrentGenericAttribute.IntValues[n] = (int64)HAPIIntValues[n];
			}
			else
			{
				// Initialize the value array
				CurrentGenericAttribute.IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

				// Get the value(s) with a reinterpret_cast since sizes match
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInt64Data(
					FHoudiniEngine::Get().GetSession(),
					InGeoNodeId, InPartId,
					H_TCHAR_TO_UTF8(*AttribName), &AttribInfo,
					0, reinterpret_cast<HAPI_Int64*>(CurrentGenericAttribute.IntValues.GetData()),
					AttribStart, AttribCount))
				{
					// failed to get that attribute's data
					continue;
				}
			}
#else
			// Initialize the value array
			CurrentGenericAttribute.IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				H_TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				0, CurrentGenericAttribute.IntValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}
#endif
		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::INT)
		{
			// Initialize the value array
			TArray<int32> IntValues;
			IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeIntData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				H_TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				0, IntValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}

			// Convert them to int64
			CurrentGenericAttribute.IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);
			for (int32 n = 0; n < IntValues.Num(); n++)
				CurrentGenericAttribute.IntValues[n] = (int64)IntValues[n];

		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::STRING)
		{
			// Initialize a string handle array
			TArray<HAPI_StringHandle> HapiSHArray;
			HapiSHArray.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the string handle(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeStringData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				H_TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				HapiSHArray.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}

			// Convert the String Handles to FStrings
			// using a map to minimize the number of HAPI calls
			FHoudiniEngineString::SHArrayToFStringArray(HapiSHArray, CurrentGenericAttribute.StringValues);
		}
		else
		{
			// Unsupported type, skipping!
			continue;
		}

		// We can add the UPropertyAttribute to the array
		OutFoundAttributes.Add(CurrentGenericAttribute);
		FoundCount++;
	}

	return FoundCount;
}


bool
FHoudiniEngineUtils::GetGenericPropertiesAttributes(
	HAPI_NodeId InGeoNodeId,
	HAPI_PartId InPartId,
	const bool InbFindDetailAttributes,
	int32 InFirstValidPrimIndex,
	int32 InFirstValidVertexIndex,
	int32 InFirstValidPointIndex,
	TArray<FHoudiniGenericAttribute>& OutPropertyAttributes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::GetGenericPropertiesAttributes);

	int32 FoundCount = 0;
	// List all the generic property detail attributes ...
	if (InbFindDetailAttributes)
	{
		FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
			InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_DETAIL);
	}

	// .. then the primitive property attributes for the given prim
	if (InFirstValidPrimIndex != INDEX_NONE)
	{
		FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
			InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_PRIM, InFirstValidPrimIndex);
	}

	if (InFirstValidVertexIndex != INDEX_NONE)
	{
		// .. then finally, point uprop attributes for the given point
		FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
			InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_VERTEX, InFirstValidVertexIndex);
	}

	if (InFirstValidPointIndex != INDEX_NONE)
	{
		// .. then finally, point uprop attributes for the given point
		FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
			InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_POINT, InFirstValidPointIndex);
	}

	return FoundCount > 0;
}

bool
FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
	UObject* InObject,
	const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes,
	int32 AtIndex,
	bool bInDeferPostEditChangePropertyCalls,
	const FHoudiniGenericAttribute::FFindPropertyFunctionType& InProcessFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::UpdateGenericPropertiesAttributes);
	if (!IsValid(InObject))
		return false;

	// Iterate over the found Property attributes
	TArray<FHoudiniGenericAttributeChangedProperty> ChangedProperties;
	if (bInDeferPostEditChangePropertyCalls)
		ChangedProperties.Reserve(InAllPropertyAttributes.Num());
	
	int32 NumSuccess = 0;
	for (const auto& CurrentPropAttribute : InAllPropertyAttributes)
	{
		// Update the current Property Attribute
		if (!FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(InObject, CurrentPropAttribute, AtIndex, bInDeferPostEditChangePropertyCalls, &ChangedProperties, InProcessFunction))
			continue;

		// Success!
		NumSuccess++;
#if defined(HOUDINI_ENGINE_LOGGING)
		const FString ClassName = InObject->GetClass() ? InObject->GetClass()->GetName() : TEXT("Object");
		const FString ObjectName = InObject->GetName();
		HOUDINI_LOG_MESSAGE(TEXT("Modified UProperty %s on %s named %s"), *CurrentPropAttribute.AttributeName, *ClassName, *ObjectName);
#endif
	}

	// Handle call PostEditChangeProperty if we deferred the calls
	if (bInDeferPostEditChangePropertyCalls && ChangedProperties.Num() > 0)
	{
		TMap<UObject*, bool> PostEditChangePropertyCalledPerObject;
		for (FHoudiniGenericAttributeChangedProperty& ChangeData : ChangedProperties)
		{
			if (!ChangeData.Property || !IsValid(ChangeData.Object))
				continue;

			// Log that we are calling PostEditChangeProperty for the object / property chain
			if (ChangeData.PropertyChain.Num() == 0)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Calling deferred PostEditChangeProperty for %s on %s named %s"),
					*ChangeData.Property->GetName(),
					*(ChangeData.Object->GetClass() ? ChangeData.Object->GetClass()->GetName() : TEXT("Object")),
					*(ChangeData.Object->GetName()));
			}
			else if (ChangeData.PropertyChain.Num() > 0)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Calling deferred PostEditChangeProperty for %s on %s named %s"),
					*FString::JoinBy(ChangeData.PropertyChain, TEXT("."), [](FProperty const* const Property)
					{
						if (!Property)
							return FString();
						return Property->GetName();
					}),
					*(ChangeData.Object->GetClass() ? ChangeData.Object->GetClass()->GetName() : TEXT("Object")),
					*(ChangeData.Object->GetName()));
			}

			// Record if we successfully called PostEditChangeProperty at least once for each changed object
			const bool bPostEditChangePropertyCalled = FHoudiniGenericAttribute::HandlePostEditChangeProperty(ChangeData.Object, ChangeData.PropertyChain, ChangeData.Property);
			if (bPostEditChangePropertyCalled)
				PostEditChangePropertyCalledPerObject.Add(ChangeData.Object, true);
			else
				PostEditChangePropertyCalledPerObject.FindOrAdd(ChangeData.Object, false);
		}

		// For each changed object where we did not call PostEditChangeProperty, call PostEditChange
		for (const auto& Entry : PostEditChangePropertyCalledPerObject)
		{
			if (Entry.Value)
				continue;
			
			UObject* const ChangedObject = Entry.Key;
			if (!IsValid(ChangedObject))
				continue;
			
			ChangedObject->PostEditChange();
			AActor* const OwnerActor = Cast<AActor>(ChangedObject->GetOuter());
			if (OwnerActor)
			{
				OwnerActor->PostEditChange();
			}
		}
	}

	return (NumSuccess > 0);
}

bool
FHoudiniEngineUtils::SetGenericPropertyAttribute(
	HAPI_NodeId InGeoNodeId,
	HAPI_PartId InPartId,
	const FHoudiniGenericAttribute& InPropertyAttribute)
{
	HAPI_AttributeOwner AttribOwner;
	switch (InPropertyAttribute.AttributeOwner)
	{
		case EAttribOwner::Point:
			AttribOwner = HAPI_ATTROWNER_POINT;
			break;
		case EAttribOwner::Vertex:
			AttribOwner = HAPI_ATTROWNER_VERTEX;
			break;
		case EAttribOwner::Prim:
			AttribOwner = HAPI_ATTROWNER_PRIM;
			break;
		case EAttribOwner::Detail:
			AttribOwner = HAPI_ATTROWNER_DETAIL;
			break;
		case EAttribOwner::Invalid:
		default:
			HOUDINI_LOG_WARNING(TEXT("Unsupported Attribute Owner: %d"), InPropertyAttribute.AttributeOwner);
			return false;
	}

	// Create the attribute via HAPI
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.tupleSize = InPropertyAttribute.AttributeTupleSize;
	AttributeInfo.count = InPropertyAttribute.AttributeCount;
	AttributeInfo.exists = true;
	AttributeInfo.owner = AttribOwner;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	switch(InPropertyAttribute.AttributeType)
	{
		case (EAttribStorageType::INT):
			AttributeInfo.storage = HAPI_STORAGETYPE_INT;
			break;
		case (EAttribStorageType::INT64):
			AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
			break;
		case (EAttribStorageType::FLOAT):
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
			break;
		case (EAttribStorageType::FLOAT64):
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT64;
			break;
		case (EAttribStorageType::STRING):
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			break;
		case (EAttribStorageType::Invalid):
		default:
			HOUDINI_LOG_WARNING(TEXT("Unsupported Attribute Storage Type: %d"), InPropertyAttribute.AttributeType);
			return false;
	}

	// Create the new attribute
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName), &AttributeInfo))
	{
		return false;
	}

	// The New attribute has been successfully created, set its value
	switch (InPropertyAttribute.AttributeType)
	{
		case EAttribStorageType::INT:
		{
			TArray<int> TempArray;
			TempArray.Reserve(InPropertyAttribute.IntValues.Num());
			for (auto Value : InPropertyAttribute.IntValues)
			{
				TempArray.Add(static_cast<int>(Value));
			}
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeIntData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName), &AttributeInfo,
				TempArray.GetData(), 0, AttributeInfo.count))
			{
				HOUDINI_LOG_WARNING(TEXT("Could not set attribute %s"), *InPropertyAttribute.AttributeName);
			}
			break;
		}

		case EAttribStorageType::INT64:
		{
#if PLATFORM_LINUX
			// On Linux, we unfortunately cannot guarantee that int64 and HAPI_Int64 are of the same type,
			TArray<HAPI_Int64> HAPIIntValues;
			HAPIIntValues.SetNumZeroed(InPropertyAttribute.IntValues.Num());
			for (int32 n = 0; n < HAPIIntValues.Num(); n++)
				HAPIIntValues[n] = (HAPI_Int64)InPropertyAttribute.IntValues[n];

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName), &AttributeInfo,
				HAPIIntValues.GetData(), 0, AttributeInfo.count))
			{
				HOUDINI_LOG_WARNING(TEXT("Could not set attribute %s"), *InPropertyAttribute.AttributeName);
			}
#else
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName), &AttributeInfo,
				InPropertyAttribute.IntValues.GetData(), 0, AttributeInfo.count))
			{
				HOUDINI_LOG_WARNING(TEXT("Could not set attribute %s"), *InPropertyAttribute.AttributeName);
			}
#endif
			break;
		}

		case EAttribStorageType::FLOAT:
		{
			
			TArray<float> TempArray;
			TempArray.Reserve(InPropertyAttribute.DoubleValues.Num());
			for (auto Value : InPropertyAttribute.DoubleValues)
			{
				TempArray.Add(static_cast<float>(Value));
			}
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName), &AttributeInfo,
				TempArray.GetData(), 0, AttributeInfo.count))
			{
				HOUDINI_LOG_WARNING(TEXT("Could not set attribute %s"), *InPropertyAttribute.AttributeName);
			}
			break;
		}

		case EAttribStorageType::FLOAT64:
		{
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeFloat64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName), &AttributeInfo,
				InPropertyAttribute.DoubleValues.GetData(), 0, AttributeInfo.count))
			{
				HOUDINI_LOG_WARNING(TEXT("Could not set attribute %s"), *InPropertyAttribute.AttributeName);
			}
			break;
		}

		case EAttribStorageType::STRING:
		{
			FHoudiniHapiAccessor Accessor(InGeoNodeId, InPartId, TCHAR_TO_ANSI(*InPropertyAttribute.AttributeName));
			if (!Accessor.SetAttributeData(AttributeInfo, InPropertyAttribute.StringValues))
			{
				HOUDINI_LOG_WARNING(TEXT("Could not set attribute %s"), *InPropertyAttribute.AttributeName);
			}
			break;
		}

		default:
			// Unsupported storage type
			HOUDINI_LOG_WARNING(TEXT("Unsupported storage type: %d"), InPropertyAttribute.AttributeType);
			break;
	}

	return true;
}


TArray<FName>
FHoudiniEngineUtils::GetDefaultActorTags(const AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return {};
	}

	 return InActor->GetClass()->GetDefaultObject<AActor>()->Tags;
}


TArray<FName>
FHoudiniEngineUtils::GetDefaultComponentTags(const UActorComponent* InComponent)
{
	if (!IsValid(InComponent))
	{
		return {};
	}

	 return InComponent->GetClass()->GetDefaultObject<UActorComponent>()->ComponentTags;
}


void
FHoudiniEngineUtils::ApplyTagsToActorOnly(const TArray<FHoudiniGenericAttribute>& GenericPropertyAttributes,
                                             TArray<FName>& OutActorTags)
{
	for (const FHoudiniGenericAttribute& Attribute : GenericPropertyAttributes)
	{
		if (Attribute.AttributeName.StartsWith("ActorTag") || Attribute.AttributeName.StartsWith("Tag"))
		{
			OutActorTags.AddUnique(FName(Attribute.GetStringValue()));
		}
	}
}


void
FHoudiniEngineUtils::ApplyTagsToActorAndComponents(AActor* InActor, bool bKeepActorTags, const TArray<FHoudiniGenericAttribute>& GenericPropertyAttributes)
{
	auto ForEachComponentFn = [InActor](TFunctionRef<void(UActorComponent*)> Fn)
	{
		for (UActorComponent* Component : InActor->GetComponents())
		{
			if (!IsValid(Component))
			{
				continue;
			}
			Fn(Component);
		}
	};
	
	if (!bKeepActorTags)
	{
		InActor->Tags = FHoudiniEngineUtils::GetDefaultActorTags(InActor);
		ForEachComponentFn([](UActorComponent* Component)
		{
			Component->ComponentTags = FHoudiniEngineUtils::GetDefaultComponentTags(Component);
		});
	}


	for (const FHoudiniGenericAttribute& Attribute : GenericPropertyAttributes)
	{

		bool bApplyTagToActor = false;
		bool bApplyTagToMainComponent = false;
		bool bApplyTagToAllComponents = false;

		if (Attribute.AttributeName.StartsWith("ActorTag"))
		{
			bApplyTagToActor = true;
		}
		if (Attribute.AttributeName.StartsWith("MainComponentTag"))
		{
			bApplyTagToMainComponent = true;
		}
		if (Attribute.AttributeName.StartsWith("ComponentTag"))
		{
			bApplyTagToAllComponents = true;
		}
		if (Attribute.AttributeName.StartsWith("Tag"))
		{
			bApplyTagToActor = true;
			bApplyTagToAllComponents = true;
		}
		
		if (bApplyTagToActor)
		{
			InActor->Tags.AddUnique(FName(Attribute.GetStringValue()));
		}

		if (bApplyTagToAllComponents)
		{
			ForEachComponentFn([&Attribute](UActorComponent* Component)
			{
				Component->ComponentTags.AddUnique(FName(Attribute.GetStringValue()));
			});
		}
		else if(bApplyTagToMainComponent)
		{
			InActor->GetRootComponent()->ComponentTags.AddUnique(FName(Attribute.GetStringValue()));
		}
	}
}


bool
FHoudiniEngineUtils::IsKeepTagsEnabled(const TArray<FHoudiniGeoPartObject>& InHGPOs)
{
	for (const FHoudiniGeoPartObject& CurHGPO : InHGPOs)
	{
		if (CurHGPO.bKeepTags)
		{
			return true;
		}
	}
	return false;
}


bool
FHoudiniEngineUtils::IsKeepTagsEnabled(const FHoudiniGeoPartObject* InHGPO)
{
	if (InHGPO)
	{
		return InHGPO->bKeepTags;
	}
	
	return false;
}


void
FHoudiniEngineUtils::KeepOrClearComponentTags(
	UActorComponent* ActorComponent,
	const TArray<FHoudiniGeoPartObject>& InHGPOs)
{
	if (!IsValid(ActorComponent))
	{
		return;
	}
	const bool bKeepTags = IsKeepTagsEnabled(InHGPOs);
	KeepOrClearComponentTags(ActorComponent, bKeepTags);
}


void
FHoudiniEngineUtils::KeepOrClearComponentTags(UActorComponent* ActorComponent, const FHoudiniGeoPartObject* InHGPO)
{
	if (!IsValid(ActorComponent))
	{
		return;
	}
	const bool bKeepTags = IsKeepTagsEnabled(InHGPO);
	KeepOrClearComponentTags(ActorComponent, bKeepTags);
}


void
FHoudiniEngineUtils::KeepOrClearComponentTags(UActorComponent* ActorComponent, bool bKeepTags)
{
	if (!bKeepTags)
	{
		// Ensure that we revert existing tags to their default state if this is an actor component.
		const UActorComponent* DefaultComponent = ActorComponent->GetClass()->GetDefaultObject<UActorComponent>();
		ActorComponent->ComponentTags = DefaultComponent->ComponentTags;
	}
}


void
FHoudiniEngineUtils::KeepOrClearActorTags(AActor* Actor, bool bApplyToActor, bool bApplyToComponents, const FHoudiniGeoPartObject* InHGPO)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::KeepOrClearActorTags);
	if (!IsValid(Actor))
	{
		return;
	}

	if (InHGPO && InHGPO->bKeepTags)
	{
		return;
	}

	if (bApplyToActor)
	{
		// Revert actor tags to their default value
		Actor->Tags = GetDefaultActorTags(Actor);
	}

	if (bApplyToComponents)
	{
		// Revert all component tags to their default value
		const TSet<UActorComponent*>& Components = Actor->GetComponents();
		for (UActorComponent* Component : Components)
		{
			if (!IsValid(Component))
			{
				continue;
			}

			// Ensure that we revert existing tags
			const UActorComponent* DefaultComponent = Component->GetClass()->GetDefaultObject<UActorComponent>();
			Component->ComponentTags = DefaultComponent->ComponentTags;
		}
	}
}

void
FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
	UPackage * Package, UObject * Object, const FString& Key, const FString& Value)
{
	if (!IsValid(Package))
		return;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	Package->GetMetaData().SetValue(Object, *Key, *Value);
#else
	UMetaData * MetaData = Package->GetMetaData();
	if (IsValid(MetaData))
		MetaData->SetValue(Object, *Key, *Value);
#endif
}


bool
FHoudiniEngineUtils::AddLevelPathAttribute(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	ULevel* InLevel,
	int32 InCount,
	const HAPI_AttributeOwner& InAttrOwner)
{
	if (InNodeId < 0 || InCount <= 0)
		return false;

	if (!IsValid(InLevel))
		return false;

	// Extract the level path from the level
	FString LevelPath = InLevel->GetPathName();

	// We just want the path up to the first point
	int32 DotIndex;
	if (LevelPath.FindChar('.', DotIndex))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		LevelPath.LeftInline(DotIndex, EAllowShrinking::No);
#else
		LevelPath.LeftInline(DotIndex, false);
#endif

	// Marshall in level path.
	HAPI_AttributeInfo AttributeInfoLevelPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLevelPath);
	AttributeInfoLevelPath.count = InCount;
	AttributeInfoLevelPath.tupleSize = 1;
	AttributeInfoLevelPath.exists = true;
	AttributeInfoLevelPath.owner = InAttrOwner;
	AttributeInfoLevelPath.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoLevelPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		HAPI_UNREAL_ATTRIB_LEVEL_PATH, &AttributeInfoLevelPath);

	if (HAPI_RESULT_SUCCESS == Result)
	{
		// Set the attribute's string data
		FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LEVEL_PATH);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoLevelPath, LevelPath), false);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_level_path attribute for mesh: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());

		return false;
	}

	return true;
}


bool
FHoudiniEngineUtils::AddActorPathAttribute(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	AActor* InActor,
	int32 InCount,
	const HAPI_AttributeOwner& InAttrOwner)
{
	if (InNodeId < 0 || InCount <= 0)
		return false;

	if (!IsValid(InActor))
		return false;

	// Extract the actor path
	FString ActorPath = InActor->GetPathName();

	// Marshall in Actor path.
	HAPI_AttributeInfo AttributeInfoActorPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoActorPath);
	AttributeInfoActorPath.count = InCount;
	AttributeInfoActorPath.tupleSize = 1;
	AttributeInfoActorPath.exists = true;
	AttributeInfoActorPath.owner = InAttrOwner;
	AttributeInfoActorPath.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoActorPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		HAPI_UNREAL_ATTRIB_ACTOR_PATH, &AttributeInfoActorPath);

	if (HAPI_RESULT_SUCCESS == Result)
	{
		// Set the attribute's string data
		FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_ACTOR_PATH);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoActorPath, ActorPath), false);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_actor_path attribute for mesh: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());

		return false;
	}

	return true;
}

bool
FHoudiniEngineUtils::AddLandscapeTypeAttribute(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	AActor* InActor,
	int32 InCount)
{
	HOUDINI_CHECK_RETURN(IsValid(InActor), false);

	// Currently we only add an attribute for landscaping streaming proxies.
	bool bIsStreamingProxy = InActor->IsA(ALandscapeStreamingProxy::StaticClass());
	if (!bIsStreamingProxy) return false;
	
	HAPI_AttributeInfo AttributeInfoActorPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoActorPath);
	AttributeInfoActorPath.count = InCount;
	AttributeInfoActorPath.tupleSize = 1;
	AttributeInfoActorPath.exists = true;
	AttributeInfoActorPath.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoActorPath.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoActorPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_STREAMING_PROXY, &AttributeInfoActorPath);

	if (Result == HAPI_RESULT_SUCCESS )
	{
		// Set the attribute's string data

		FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_STREAMING_PROXY);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoActorPath, 1), false);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_actor_path attribute for mesh: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}
	return true;
}

void
FHoudiniEngineUtils::CreateSlateNotification(
	const FString& NotificationString, float NotificationExpire, float NotificationFadeOut )
{
#if WITH_EDITOR
	// Trying to display SlateNotifications while in a background thread will crash UE
	if (!IsInGameThread() && !IsInSlateThread() && !IsInAsyncLoadingThread())
		return;	

	// Check whether we want to display Slate notifications.
	bool bDisplaySlateCookingNotifications = true;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

	if (!bDisplaySlateCookingNotifications)
		return;

	FText NotificationText = FText::FromString(NotificationString);
	FNotificationInfo Info(NotificationText);

	Info.bFireAndForget = true;
	Info.FadeOutDuration = NotificationFadeOut;
	Info.ExpireDuration = NotificationExpire;

	TSharedPtr<FSlateDynamicImageBrush> HoudiniBrush = FHoudiniEngine::Get().GetHoudiniEngineLogoBrush();
	if (HoudiniBrush.IsValid())
		Info.Image = HoudiniBrush.Get();

	FSlateNotificationManager::Get().AddNotification(Info);
#endif

	return;
}

FString
FHoudiniEngineUtils::GetHoudiniEnginePluginDir()
{
	FString EnginePluginDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine/");
	if (FPaths::FileExists(EnginePluginDir + "HoudiniEngine.uplugin"))
		return EnginePluginDir;

	FString ProjectPluginDir = FPaths::ProjectPluginsDir() / TEXT("Runtime/HoudiniEngine/");
	if (FPaths::FileExists(ProjectPluginDir + "HoudiniEngine.uplugin"))
		return ProjectPluginDir;

	TSharedPtr<IPlugin> HoudiniPlugin = IPluginManager::Get().FindPlugin(TEXT("HoudiniEngine"));
	FString PluginBaseDir = HoudiniPlugin.IsValid() ? HoudiniPlugin->GetBaseDir() : EnginePluginDir;
	if (FPaths::DirectoryExists(PluginBaseDir))
		return PluginBaseDir;

	HOUDINI_LOG_WARNING(TEXT("Could not find the Houdini Engine plugin's directory"));

	return EnginePluginDir;
}


HAPI_Result
FHoudiniEngineUtils::CreateNode(
	HAPI_NodeId InParentNodeId,
	const FString& InOperatorName,
	const FString& InNodeLabel,
	HAPI_Bool bInCookOnCreation,
	HAPI_NodeId* OutNewNodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::CreateNode);

	// Call HAPI::CreateNode
	HAPI_Result Result = FHoudiniApi::CreateNode(
		FHoudiniEngine::Get().GetSession(),
		InParentNodeId, H_TCHAR_TO_UTF8(*InOperatorName), H_TCHAR_TO_UTF8(*InNodeLabel), bInCookOnCreation, OutNewNodeId);

	// Return now if CreateNode failed
	if (Result != HAPI_RESULT_SUCCESS)
		return Result;
		
	// Loop on the cook_state status until it's ready
	int CurrentStatus = HAPI_State::HAPI_STATE_STARTING_LOAD;
	while (CurrentStatus > HAPI_State::HAPI_STATE_MAX_READY_STATE)
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(),
			HAPI_StatusType::HAPI_STATUS_COOK_STATE, &CurrentStatus))
		{
			// Exit the loop if GetStatus somehow fails
			break;
		}
	}

	if (CurrentStatus == HAPI_STATE_READY_WITH_FATAL_ERRORS)
	{
		// Fatal errors - failed
		HOUDINI_LOG_ERROR(TEXT("Failed to create node %s - %s"), *InOperatorName, *InNodeLabel);
		return HAPI_RESULT_FAILURE;
	}
	else if (CurrentStatus == HAPI_STATE_READY_WITH_COOK_ERRORS)
	{
		// Mention the errors - still return success
		HOUDINI_LOG_WARNING(TEXT("Cook errors when creating node %s - %s"), *InOperatorName, *InNodeLabel);
	}

	return HAPI_RESULT_SUCCESS;
}


int32
FHoudiniEngineUtils::HapiGetCookCount(HAPI_NodeId InNodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiGetCookCount);

	// To reduce the "cost" of the call on big HDAs - limit or search to non bypassed SOP/OBJ nodes
	int32 CookCount = -1;
	if (HAPI_RESULT_FAILURE == FHoudiniApi::GetTotalCookCount(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, HAPI_NODETYPE_OBJ | HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_NON_BYPASS, true, &CookCount))
	{
		return -1;
	}

	return CookCount;
}

bool
FHoudiniEngineUtils::GetLevelPathAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	TArray<FString>& OutLevelPaths,
	HAPI_AttributeOwner InAttributeOwner,
	int32 InStartIndex,
	int32 InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_level_path
	// ---------------------------------------------

	FHoudiniHapiAccessor Accessor(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	bool bSuccess = Accessor.GetAttributeData(InAttributeOwner, 1, OutLevelPaths, InStartIndex, InCount);

	if (bSuccess && OutLevelPaths.Num() > 0)
		return true;

	OutLevelPaths.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetLevelPathAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	FString& OutLevelPath,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	if (InPointIndex >= 0)
	{
		if (GetLevelPathAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutLevelPath = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (GetLevelPathAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutLevelPath = StringData[0];
				return true;
			}
		}
	}

	if (GetLevelPathAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutLevelPath = StringData[0];
			return true;
		}
	}
	
	OutLevelPath.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetOutputNameAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId, 
	TArray<FString>& OutOutputNames,
	int32 InStartIndex,
	int32 InCount)
{
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, OutOutputNames, InStartIndex, InCount);
	if (bSuccess && OutOutputNames.Num() > 0)
		return true;

	OutOutputNames.Empty();

	Accessor.Init(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, OutOutputNames, InStartIndex, InCount);
	if (bSuccess && OutOutputNames.Num() > 0)
		return true;

	OutOutputNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetOutputNameAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	FString& OutOutputName,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2);
	bool bSuccess;

	// HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2
	if (InPointIndex >= 0)
	{
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, StringData, InPointIndex, Count);
		if (bSuccess && StringData.Num() > 0)
		{
			OutOutputName = StringData[0];
			return true;
		}
	}

	if (InPrimIndex >= 0)
	{
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, StringData, InPrimIndex, Count);
		if (bSuccess && StringData.Num() > 0)
		{
			OutOutputName = StringData[0];
			return true;
		}
	}

	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, 1, StringData, 0, Count);
	if (bSuccess && StringData.Num() > 0)
	{
		OutOutputName = StringData[0];
		return true;
	}

	Accessor.Init(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1);

	// HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1
	if (InPointIndex >= 0)
	{
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, StringData, InPointIndex, Count);
		if (bSuccess && StringData.Num() > 0)
		{
			OutOutputName = StringData[0];
			return true;
		}
	}

	if (InPrimIndex >= 0)
	{
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, StringData, InPrimIndex, Count);
		if (bSuccess && StringData.Num() > 0)
		{
			OutOutputName = StringData[0];
			return true;
		}
	}

	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, 1, StringData, 0, Count);
	if (bSuccess && StringData.Num() > 0)
	{
		OutOutputName = StringData[0];
		return true;
	}

	OutOutputName.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeNameAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId, 
	TArray<FString>& OutBakeNames,
	const HAPI_AttributeOwner& InAttribOwner,
	int32 InStartIndex,
	int32 InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_name
	// ---------------------------------------------

	FHoudiniHapiAccessor Accessor(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_NAME);
	bool bSuccess = Accessor.GetAttributeData(InAttribOwner, 1, OutBakeNames, InStartIndex, InCount);

	if (bSuccess && OutBakeNames.Num() > 0)
		return true;

	OutBakeNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeNameAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId, 
	FString& OutBakeName,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	if (InPointIndex >= 0)
	{
		if (GetBakeNameAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeName = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (GetBakeNameAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeName = StringData[0];
				return true;
			}
		}
	}

	if (GetBakeNameAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutBakeName = StringData[0];
			return true;
		}
	}
	
	OutBakeName.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetTileAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	TArray<int32>& OutTileValues,
	const HAPI_AttributeOwner& InAttribOwner,
	int32 InStart,
	int32 InCount)
{
	// ---------------------------------------------
	// Attribute: tile
	// ---------------------------------------------


	FHoudiniHapiAccessor Accessor(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, OutTileValues, InStart, InCount);
	if (bSuccess)
	{
		if (OutTileValues.Num() > 0)
			return true;
	}

	OutTileValues.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetTileAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	int32& OutTileValue,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<int32> IntData;

	if (InPointIndex >= 0)
	{
		if (GetTileAttribute(InGeoId, InPartId, IntData, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (IntData.Num() > 0)
			{
				OutTileValue = IntData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (GetTileAttribute(InGeoId, InPartId, IntData, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (IntData.Num() > 0)
			{
				OutTileValue = IntData[0];
				return true;
			}
		}
	}

	if (GetTileAttribute(InGeoId, InPartId, IntData, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (IntData.Num() > 0)
		{
			OutTileValue = IntData[0];
			return true;
		}
	}
	
	return false;
}

bool
FHoudiniEngineUtils::GetEditLayerName(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	FString& EditLayerName,
	const HAPI_AttributeOwner& InAttribOwner)
{
	// ---------------------------------------------
	// Attribute: tile
	// ---------------------------------------------
	HAPI_AttributeInfo AttribInfo;
	FHoudiniApi::AttributeInfo_Init(&AttribInfo);

	TArray<FString> StrData;

	FHoudiniHapiAccessor Accessor(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME);
	bool bSuccess = Accessor.GetAttributeData(InAttribOwner, 1, StrData);

	if (bSuccess && StrData.Num() > 0)
	{
		EditLayerName = StrData[0];
		return true;
	}

	EditLayerName = FString();
	return false;
}

bool FHoudiniEngineUtils::HasEditLayerName(HAPI_NodeId InGeoId, HAPI_PartId InPartId,
	const HAPI_AttributeOwner& InAttribOwner)
{
	// ---------------------------------------------
	// Attribute: unreal_landscape_
	// ---------------------------------------------

	return FHoudiniEngineUtils::HapiCheckAttributeExists(
		InGeoId, InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME,
		InAttribOwner);
}

bool
FHoudiniEngineUtils::GetTempFolderAttribute(
	HAPI_NodeId InNodeId,
	const HAPI_AttributeOwner& InAttributeOwner,
	TArray<FString>& OutTempFolder,
	HAPI_PartId InPartId,
	int32 InStart,
	int32 InCount)
{
	OutTempFolder.Empty();

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_TEMP_FOLDER);
	bool bSuccess = Accessor.GetAttributeData(InAttributeOwner, 1, OutTempFolder, InStart, InCount);

	if (bSuccess && OutTempFolder.Num() > 0)
		return true;

	OutTempFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetTempFolderAttribute(
	HAPI_NodeId InGeoId,
	FString& OutTempFolder,
	HAPI_PartId InPartId,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;
	if (GetTempFolderAttribute(InGeoId, HAPI_ATTROWNER_PRIM, StringData, InPartId, InPrimIndex, Count))
	{
		if (StringData.Num() > 0)
		{
			OutTempFolder = StringData[0];
			return true;
		}
	}

	if (GetTempFolderAttribute(InGeoId, HAPI_ATTROWNER_DETAIL, StringData, InPartId, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutTempFolder = StringData[0];
			return true;
		}
	}
	
	OutTempFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeFolderAttribute(
	HAPI_NodeId InNodeId,
	const HAPI_AttributeOwner& InAttributeOwner,
	TArray<FString>& OutBakeFolder,
	HAPI_PartId InPartId,
	int32 InStart,
	int32 InCount)
{
	OutBakeFolder.Empty();

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_FOLDER);
	bool bSuccess = Accessor.GetAttributeData(InAttributeOwner, 1, OutBakeFolder, InStart, InCount);

	if (bSuccess && OutBakeFolder.Num() > 0)
		return true;

	OutBakeFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeFolderAttribute(
	HAPI_NodeId InGeoId,
	TArray<FString>& OutBakeFolder,
	HAPI_PartId InPartId,
	int32 InStart,
	int32 InCount)
{
	OutBakeFolder.Empty();

	if (GetBakeFolderAttribute(InGeoId, HAPI_ATTROWNER_PRIM, OutBakeFolder, InPartId, InStart, InCount))
	{
		if (OutBakeFolder.Num() > 0)
			return true;
	}

	if (GetBakeFolderAttribute(InGeoId, HAPI_ATTROWNER_DETAIL, OutBakeFolder, InPartId, InStart, InCount))
	{
		if (OutBakeFolder.Num() > 0)
			return true;
	}
	
	OutBakeFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeFolderAttribute(
	const HAPI_NodeId InGeoId,
	const HAPI_PartId InPartId,
	FString& OutBakeFolder,
	const int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;
	if (InPrimIndex >= 0)
	{
		if (GetBakeFolderAttribute(InGeoId, HAPI_ATTROWNER_PRIM, StringData, InPartId, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeFolder = StringData[0];
				return true;
			}
		}
	}

	if (GetBakeFolderAttribute(InGeoId, HAPI_ATTROWNER_POINT, StringData, InPartId, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutBakeFolder = StringData[0];
			return true;
		}
	}

	if (GetBakeFolderAttribute(InGeoId, HAPI_ATTROWNER_DETAIL, StringData, InPartId, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutBakeFolder = StringData[0];
			return true;
		}
	}
	
	OutBakeFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeActorAttribute(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	TArray<FString>& OutBakeActorNames,
	const HAPI_AttributeOwner& InAttributeOwner,
	int32 InStart,
	int32 InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_actor
	// ---------------------------------------------

	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_ACTOR);
	bool bSuccess = Accessor.GetAttributeData(InAttributeOwner, 1, OutBakeActorNames, InStart, InCount);

	if (bSuccess && OutBakeActorNames.Num() > 0)
		return true;

	OutBakeActorNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeActorAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	FString& OutBakeActorName,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	if (InPointIndex >= 0)
	{
		if (GetBakeActorAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeActorName = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (GetBakeActorAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeActorName = StringData[0];
				return true;
			}
		}
	}

	if (GetBakeActorAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutBakeActorName = StringData[0];
			return true;
		}
	}
	
	OutBakeActorName.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeActorClassAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	TArray<FString>& OutBakeActorClassNames,
	const HAPI_AttributeOwner& InAttributeOwner,
	int32 InStart,
	int32 InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_actor
	// ---------------------------------------------

	FHoudiniHapiAccessor Accessor(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS);
	bool bSuccess = Accessor.GetAttributeData(InAttributeOwner, 1, OutBakeActorClassNames, InStart, InCount);

	if (bSuccess && OutBakeActorClassNames.Num() > 0)
		return true;

	OutBakeActorClassNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeActorClassAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	FString& OutBakeActorClassName,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	if (InPointIndex >= 0)
	{
		if (GetBakeActorClassAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeActorClassName = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (GetBakeActorClassAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeActorClassName = StringData[0];
				return true;
			}
		}
	}

	if (GetBakeActorClassAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutBakeActorClassName = StringData[0];
			return true;
		}
	}
	
	OutBakeActorClassName.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	TArray<FString>& OutBakeOutlinerFolders,
	const HAPI_AttributeOwner& InAttributeOwner,
	int32 InStart,
	int32 InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_outliner_folder
	// ---------------------------------------------

	FHoudiniHapiAccessor Accessor(InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER);
	bool bSuccess = Accessor.GetAttributeData(InAttributeOwner, 1, OutBakeOutlinerFolders, InStart, InCount);
	if (bSuccess && OutBakeOutlinerFolders.Num() > 0)
		return true;

	OutBakeOutlinerFolders.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(
	HAPI_NodeId InGeoId,
	HAPI_PartId InPartId,
	FString& OutBakeOutlinerFolder,
	int32 InPointIndex,
	int32 InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	if (InPointIndex >= 0)
	{
		if (GetBakeOutlinerFolderAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeOutlinerFolder = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (GetBakeOutlinerFolderAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutBakeOutlinerFolder = StringData[0];
				return true;
			}
		}
	}

	if (GetBakeOutlinerFolderAttribute(InGeoId, InPartId, StringData, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutBakeOutlinerFolder = StringData[0];
			return true;
		}
	}
	
	OutBakeOutlinerFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::MoveActorToLevel(AActor* InActor, ULevel* InDesiredLevel)
{
	if (!InActor || !InDesiredLevel)
		return false;

	ULevel* PreviousLevel = InActor->GetLevel();
	if (PreviousLevel == InDesiredLevel)
		return true;

	UWorld* CurrentWorld = InActor->GetWorld();
	if(CurrentWorld)
		CurrentWorld->RemoveActor(InActor, true);

	//Set the outer of Actor to NewLevel
	FHoudiniEngineUtils::RenameObject(InActor, (const TCHAR*)0, InDesiredLevel);
	InDesiredLevel->Actors.Add(InActor);

	return true;
}

HAPI_Result
FHoudiniEngineUtils::HapiCommitGeo(HAPI_NodeId InNodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiCommitGeo);
	return FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), InNodeId);
}

bool
FHoudiniEngineUtils::HapiCookNode(HAPI_NodeId InNodeId, HAPI_CookOptions* InCookOptions, bool bWaitForCompletion)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::HapiCookNode);

	// Check for an invalid node id
	if (InNodeId < 0)
		return false;

	// No Cook Options were specified, use the default one
	if (InCookOptions == nullptr)
	{
		// Use the default cook options
		HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
			FHoudiniEngine::Get().GetSession(), InNodeId, &CookOptions), false);
	}
	else
	{
		// Use the provided CookOptions
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
			FHoudiniEngine::Get().GetSession(), InNodeId, InCookOptions), false);
	}

	// If we don't need to wait for completion, return now
	if (!bWaitForCompletion)
		return true;

	// Wait for the cook to finish
	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	while (true)
	{
		// Get the current cook status
		int Status = HAPI_STATE_STARTING_COOK;
		HOUDINI_CHECK_ERROR_GET(&Result, FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status));

		if (Status == HAPI_STATE_READY)
		{
			// The cook has been successful.
			return true;
		}
		else if (Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
		{
			// There was an error while cooking the node.
			//FString CookResultString = FHoudiniEngineUtils::GetCookResult();
			//HOUDINI_LOG_ERROR();
			return false;
		}

		// We want to yield a bit.
		FPlatformProcess::Sleep(0.1f);
	}
}

HAPI_NodeId FHoudiniEngineUtils::CreateInputHapiNode(const FString& InNodeLabel, HAPI_NodeId InParentNodeId)
{
	HAPI_NodeId OutNodeId = INDEX_NONE;
	HAPI_Result Result = CreateInputNode(InNodeLabel, OutNodeId, InParentNodeId);
	if(Result == HAPI_Result::HAPI_RESULT_SUCCESS)
		return OutNodeId;
	else
		return INDEX_NONE;
}

HAPI_Result
FHoudiniEngineUtils::CreateInputNode(const FString& InNodeLabel, HAPI_NodeId& OutNodeId, const int32 InParentNodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::CreateInputNode);

	HAPI_NodeId NodeId = -1;
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();

	if (InParentNodeId < 0)
	{
		const HAPI_Result Result = FHoudiniApi::CreateInputNode(Session, -1, &NodeId, H_TCHAR_TO_UTF8(*InNodeLabel));
		if (Result != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::CreateInputNode]: CreateInputNode failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			return Result;
		}
		
		OutNodeId = NodeId;
		return Result;
	}
	
	if (!IsHoudiniNodeValid(InParentNodeId))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::CreateInputNode]: InParentNodeId (%d) is not valid."), InParentNodeId);
		return HAPI_RESULT_NODE_INVALID;
	}

	const FString NodeLabel = TEXT("input_") + InNodeLabel;
	constexpr bool bCookOnCreation = true;
	HAPI_NodeId ObjectNodeId = -1;
	HAPI_Result Result = CreateNode(InParentNodeId, TEXT("geo"), NodeLabel, bCookOnCreation, &ObjectNodeId);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::CreateInputNode]: CreateNode failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return Result;
	}
	Result = CreateNode(ObjectNodeId, TEXT("null"), NodeLabel, bCookOnCreation, &NodeId);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::CreateInputNode]: CreateNode failed: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return Result;
	}
	
	OutNodeId = NodeId;
	return Result;
}

bool
FHoudiniEngineUtils::HapiConnectNodeInput(int32 InNodeId, int32 InputIndex, int32 InNodeIdToConnect, int32 OutputIndex, int32 InXFormType)
{
	// Connect the node ids
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(), InNodeId, InputIndex, InNodeIdToConnect, OutputIndex), false);

	// When connecting two nodes that are NOT in the same subnet,
	// HAPI creates an object merge node for the connection
	// See if we have specified a TransformType for that object merge!
	if(InXFormType <= 0 || InXFormType <= 2)
	{
		HAPI_NodeId ObjMergeNodeId = -1;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::QueryNodeInput(
			FHoudiniEngine::Get().GetSession(), InNodeId, InputIndex, &ObjMergeNodeId), false);

		// Set the transform value to "None"
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), ObjMergeNodeId, H_TCHAR_TO_UTF8(TEXT("xformtype")), 0, InXFormType), false);
	}

	return true;
}


FString
FHoudiniEngineUtils::JSONToString(const TSharedPtr<FJsonObject>& JSONObject)
{
	FString OutputString;
	const TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JSONObject.ToSharedRef(), Writer);
	return OutputString;
}


bool
FHoudiniEngineUtils::JSONFromString(const FString& JSONString, TSharedPtr<FJsonObject>& OutJSONObject)
{
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( JSONString );
	if (!FJsonSerializer::Deserialize(Reader, OutJSONObject) || !OutJSONObject.IsValid())
	{
		return false;
	}

	return true;
}


bool
FHoudiniEngineUtils::UpdateMeshPartUVSets(
	const int GeoId,
	const int PartId,
	bool bRemoveUnused,
	TArray<TArray<float>>& OutPartUVSets,
	TArray<HAPI_AttributeInfo>& OutAttribInfoUVSets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniEngineUtils::UpdateMeshPartUVSets);

	// Only Retrieve uvs if necessary
	if (OutPartUVSets.Num() > 0)
		return true;

	OutPartUVSets.SetNum(MAX_STATIC_TEXCOORDS);
	OutAttribInfoUVSets.SetNum(MAX_STATIC_TEXCOORDS);

	// The second UV set should be called uv2, but we will still check if need to look for a uv1 set.
	// If uv1 exists, we'll look for uv, uv1, uv2 etc.. if not we'll look for uv, uv2, uv3 etc..
	bool bUV1Exists = FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, "uv1");

	// Retrieve UVs.
	for (int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
	{
		FString UVAttributeName = HAPI_UNREAL_ATTRIB_UV;
		if (TexCoordIdx > 0)
			UVAttributeName += FString::Printf(TEXT("%d"), bUV1Exists ? TexCoordIdx : TexCoordIdx + 1);

		FHoudiniApi::AttributeInfo_Init(&OutAttribInfoUVSets[TexCoordIdx]);

		FHoudiniHapiAccessor Accessor(GeoId, PartId, TCHAR_TO_ANSI(*UVAttributeName));
		Accessor.GetInfo(OutAttribInfoUVSets[TexCoordIdx], HAPI_ATTROWNER_INVALID);
		OutAttribInfoUVSets[TexCoordIdx].tupleSize = 2;
		Accessor.GetAttributeData(OutAttribInfoUVSets[TexCoordIdx], OutPartUVSets[TexCoordIdx]);
	}

	// Also look for 16.5 uvs (attributes with a Texture type) 
	// For that, we'll have to iterate through ALL the attributes and check their types
	TArray<FString> FoundAttributeNames;
	TArray<HAPI_AttributeInfo> FoundAttributeInfos;
	for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		FHoudiniEngineUtils::HapiGetAttributeOfType(
			GeoId, PartId, (HAPI_AttributeOwner)AttrIdx, 
			HAPI_ATTRIBUTE_TYPE_TEXTURE, FoundAttributeInfos, FoundAttributeNames);
	}

	if (FoundAttributeInfos.Num() <= 0)
		return true;

	// We found some additionnal uv attributes
	int32 AvailableIdx = 0;
	for (int32 attrIdx = 0; attrIdx < FoundAttributeInfos.Num(); attrIdx++)
	{
		// Ignore the old uvs
		if (FoundAttributeNames[attrIdx] == TEXT("uv")
			|| FoundAttributeNames[attrIdx] == TEXT("uv1")
			|| FoundAttributeNames[attrIdx] == TEXT("uv2")
			|| FoundAttributeNames[attrIdx] == TEXT("uv3")
			|| FoundAttributeNames[attrIdx] == TEXT("uv4")
			|| FoundAttributeNames[attrIdx] == TEXT("uv5")
			|| FoundAttributeNames[attrIdx] == TEXT("uv6")
			|| FoundAttributeNames[attrIdx] == TEXT("uv7")
			|| FoundAttributeNames[attrIdx] == TEXT("uv8"))
			continue;

		HAPI_AttributeInfo CurrentAttrInfo = FoundAttributeInfos[attrIdx];
		if (!CurrentAttrInfo.exists)
			continue;

		// Look for the next available index in the return arrays
		for (; AvailableIdx < OutAttribInfoUVSets.Num(); AvailableIdx++)
		{
			if (!OutAttribInfoUVSets[AvailableIdx].exists)
				break;
		}

		// We are limited to MAX_STATIC_TEXCOORDS uv sets!
		// If we already have too many uv sets, skip the rest
		if ((AvailableIdx >= MAX_STATIC_TEXCOORDS) || (AvailableIdx >= OutAttribInfoUVSets.Num()))
		{
			HOUDINI_LOG_WARNING(TEXT("Too many UV sets found. Unreal only supports %d , skipping the remaining uv sets."), (int32)MAX_STATIC_TEXCOORDS);
			break;
		}

		// Force the tuple size to 2 ?
		CurrentAttrInfo.tupleSize = 2;

		// Add the attribute infos we found
		OutAttribInfoUVSets[AvailableIdx] = CurrentAttrInfo;

		// Allocate sufficient buffer for the attribute's data.
		OutPartUVSets[AvailableIdx].SetNumUninitialized(CurrentAttrInfo.count * CurrentAttrInfo.tupleSize);

		// Get the texture coordinates
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, H_TCHAR_TO_UTF8(*(FoundAttributeNames[attrIdx])),
			&OutAttribInfoUVSets[AvailableIdx], -1,
			&OutPartUVSets[AvailableIdx][0], 0, CurrentAttrInfo.count))
		{
			// Something went wrong when trying to access the uv values, invalidate this set
			OutAttribInfoUVSets[AvailableIdx].exists = false;
		}
	}

	// Remove unused UV sets
	if (bRemoveUnused)
	{
		for (int32 Idx = OutPartUVSets.Num() - 1; Idx >= 0; Idx--)
		{
			if (OutPartUVSets[Idx].Num() > 0)
				continue;

			OutPartUVSets.RemoveAt(Idx);
		}
	}

	return true;
}


void
FHoudiniEngineUtils::ForceDeleteObject(UObject* Object)
{
	// This function came into existence to ensure Data Tables are fully deleted before recooking.
	// Just normally destroying Data Tables doesn't remove some internal data, which causes problems
	// when recreating a package with the same name.

	if (!IsValid(Object))
		return;

	// Make sure object is loaded before we destroyed it.
	if (IsValid(Object->GetPackage()) && !Object->GetPackage()->IsFullyLoaded()) 
		Object->GetPackage()->FullyLoad();

	// First we must nullify references, or DeleteSingleObject will do nothing.
	TArray<UObject*> Objects = { Object };
	ObjectTools::ForceReplaceReferences(nullptr, Objects);

	// Now delete the object.
	const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(Object, false);

	// CollectGarbage so we don't get stale objects.
	if (bDeleteSucceeded)
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
}

TArray<FString> FHoudiniEngineUtils::GetAttributeNames(const HAPI_Session* Session, HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner)
{
	HAPI_PartInfo PartInfo;
	TArray<FString> Results;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(Session, NodeId, PartId, &PartInfo), Results);

	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(PartInfo.attributeCounts[Owner]);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(Session, NodeId, PartId, Owner, StringHandles.GetData(), StringHandles.Num()), Results);

	FHoudiniEngineString::SHArrayToFStringArray(StringHandles, Results, Session);

	return Results;
}

TMap< HAPI_AttributeOwner, TArray<FString>> FHoudiniEngineUtils::GetAllAttributeNames(const HAPI_Session* Session, HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	TMap< HAPI_AttributeOwner, TArray<FString>> Results;

	Results.Add(HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX, GetAttributeNames(Session, NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX));
	Results.Add(HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, GetAttributeNames(Session, NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT));
	Results.Add(HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM, GetAttributeNames(Session, NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM));
	Results.Add(HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, GetAttributeNames(Session, NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL));

	return Results;
}

void FHoudiniEngineUtils::DumpNode(const FString& NodePath)
{
	HAPI_NodeId UnrealContentNodeId = -1;
	HAPI_Result result = FHoudiniApi::GetNodeFromPath(
		FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*NodePath), &UnrealContentNodeId);
	if (result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_DISPLAY(TEXT("Failed to get node from path: %s"), *NodePath);
		return;
	}
	FString Output = DumpNode(UnrealContentNodeId);
	HOUDINI_LOG_DISPLAY(TEXT("%s"), *Output);
}

#define H_CASE_ENUM_TO_STRING(X) case X: return TEXT(#X);

FString FHoudiniEngineUtils::NodeTypeToString(HAPI_NodeType NodeType)
{
	switch (NodeType)
	{
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_ANY)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_NONE)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_OBJ)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_SOP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_CHOP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_ROP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_SHOP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_COP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_VOP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_DOP)
		H_CASE_ENUM_TO_STRING(HAPI_NODETYPE_TOP)
	default:
		return TEXT("Unknown");
	}
}

FString FHoudiniEngineUtils::PartTypeToString(HAPI_PartType PartType)
{
	switch (PartType)
	{
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_INVALID);
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_MESH);
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_CURVE)
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_VOLUME);
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_INSTANCER);
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_BOX);
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_SPHERE);
		H_CASE_ENUM_TO_STRING(HAPI_PARTTYPE_MAX);
	default:
		return TEXT("Unknown");
	}
}

FString FHoudiniEngineUtils::AttributeTypeToString(HAPI_AttributeTypeInfo AttributeType)
{
	switch (AttributeType)
	{
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_INVALID);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_NONE);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_POINT);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_HPOINT);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_VECTOR);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_NORMAL);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_COLOR);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_QUATERNION);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_MATRIX3);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_MATRIX);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_ST);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_HIDDEN);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_BOX2);
		H_CASE_ENUM_TO_STRING(HAPI_ATTRIBUTE_TYPE_BOX);
	default:
		return TEXT("Unknown");
	}
}

FString FHoudiniEngineUtils::StorageTypeToString(HAPI_StorageType StorageType)
{
	switch (StorageType)
	{
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INVALID);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT64);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_FLOAT);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_FLOAT64);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_STRING);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_UINT8);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT8);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT16);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_DICTIONARY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT64_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_FLOAT_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_FLOAT64_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_STRING_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_UINT8_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT8_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_INT16_ARRAY);
		H_CASE_ENUM_TO_STRING(HAPI_STORAGETYPE_DICTIONARY_ARRAY);
	default: return TEXT("Unknown");
	}
}

FString FHoudiniEngineUtils::CurveTypeToString(HAPI_CurveType CurveType)
{
	switch (CurveType)
	{
		H_CASE_ENUM_TO_STRING(HAPI_CURVETYPE_INVALID)
		H_CASE_ENUM_TO_STRING(HAPI_CURVETYPE_LINEAR)
		H_CASE_ENUM_TO_STRING(HAPI_CURVETYPE_NURBS)
		H_CASE_ENUM_TO_STRING(HAPI_CURVETYPE_BEZIER)
		H_CASE_ENUM_TO_STRING(HAPI_CURVETYPE_MAX)
	default:
		return TEXT("Unknown");
	}
}

FString FHoudiniEngineUtils::RSTOrderToString(HAPI_RSTOrder RstOrder)
{
	switch (RstOrder)
	{
		H_CASE_ENUM_TO_STRING(HAPI_TRS)
		H_CASE_ENUM_TO_STRING(HAPI_TSR)
		H_CASE_ENUM_TO_STRING(HAPI_RST)
		H_CASE_ENUM_TO_STRING(HAPI_RTS)
		H_CASE_ENUM_TO_STRING(HAPI_STR)
		H_CASE_ENUM_TO_STRING(HAPI_SRT)
	default:
		return TEXT("Unknown");
	}
}

#undef H_CASE_ENUM_TO_STRING

FString FHoudiniEngineUtils::HapiTransformToString(HAPI_Transform Transform)
{
	FStringBuilderBase Output;
	Output.Appendf(TEXT("P: %f, %f, %f "), Transform.position[0], Transform.position[1], Transform.position[2]);
	Output.Appendf(TEXT("Q: %f, %f, %f, %f "), Transform.rotationQuaternion[0], Transform.rotationQuaternion[1],
	               Transform.rotationQuaternion[2], Transform.rotationQuaternion[3]);
	Output.Appendf(TEXT("S: %f, %f, %f "), Transform.scale[0], Transform.scale[1], Transform.scale[2]);
	Output.Appendf(TEXT("SH: %f, %f, %f "), Transform.shear[0], Transform.shear[1], Transform.shear[2]);
	Output.Appendf(TEXT("RST Order: %s\n"), *RSTOrderToString(Transform.rstOrder));
	return Output.ToString();
}

FString FHoudiniEngineUtils::DumpNode(HAPI_NodeId NodeId)
{
	if (NodeId == INDEX_NONE)
		return TEXT("Invalid Node ID\n");

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);

	HAPI_Result Result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo);
	if(Result != HAPI_RESULT_SUCCESS)
		return FString::Printf(TEXT("Failed to get node info: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());

	FStringBuilderBase Output;


	Output.Appendf(TEXT("Node ID: %d\n"), NodeId);
	Output.Appendf(TEXT("    Name: %s\n"), *FHoudiniEngineString(NodeInfo.nameSH).ToFString());
	Output.Appendf(TEXT("    Type: %s\n"), *NodeTypeToString(NodeInfo.type));

	// Get GeoInfo for this node
	HAPI_GeoInfo GeoInfo;
	FHoudiniApi::GeoInfo_Init(&GeoInfo);
	Result = FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), NodeId, &GeoInfo);
	if(Result != HAPI_RESULT_SUCCESS)
	{
		Output.Appendf(TEXT("    No GeoInfo, reason: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());
		return Output.ToString();
	}

	Output.Append(TEXT("    Part Count: %d\n"), GeoInfo.partCount);

	for (int PartIndex = 0; PartIndex < GeoInfo.partCount; PartIndex++)
	{
		DumpPart(NodeId, PartIndex, Output);
	}
	return Output.ToString();
}

FString FHoudiniEngineUtils::DumpAttribute(HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner,
                                           const FString& Name)
{
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	HAPI_Result Result = FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		TCHAR_TO_ANSI(*Name), Owner, &AttributeInfo);
	if(Result != HAPI_RESULT_SUCCESS)
	{
		return FString::Printf(TEXT("Failed to get attribute info: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());
	}

	FStringBuilderBase Output;
	Output.Appendf(TEXT("            Storage: %s\n"), *StorageTypeToString(AttributeInfo.storage));
	Output.Appendf(TEXT("            Type: %s\n"), *AttributeTypeToString(AttributeInfo.typeInfo));
	Output.Appendf(TEXT("            Tuple Size: %d\n"), AttributeInfo.tupleSize);
	Output.Appendf(TEXT("            Count: %d\n"), AttributeInfo.count);
	Output.Appendf(TEXT("            Total Array Elements: %d\n"), AttributeInfo.totalArrayElements);
	return Output.ToString();
}


void FHoudiniEngineUtils::DumpPart(HAPI_NodeId NodeId, HAPI_PartId PartId, FStringBuilderBase& Output)
{
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HAPI_Result Result = FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo);
	if(Result != HAPI_RESULT_SUCCESS)
	{
		Output.Appendf(TEXT("    Failed to get part info: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());
		return;
	}

	Output.Appendf(TEXT("Part %d\n"), PartId);
	Output.Appendf(TEXT("    Part Name: %s\n"), *FHoudiniEngineString(PartInfo.nameSH).ToFString());
	Output.Appendf(TEXT("    Part Type: %s\n"), *PartTypeToString(PartInfo.type));
	Output.Appendf(TEXT("    Part Face Count: %d\n"), PartInfo.faceCount);
	Output.Appendf(TEXT("    Part Vertex Count: %d\n"), PartInfo.vertexCount);
	Output.Appendf(TEXT("    Part Point Count: %d\n"), PartInfo.pointCount);
	Output.Appendf(TEXT("    Part Vertex Attribute Count: %d\n"), PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX]);
	Output.Appendf(TEXT("    Part Point Attribute Count: %d\n"), PartInfo.attributeCounts[HAPI_ATTROWNER_POINT]);
	Output.Appendf(TEXT("    Part Primitive Attribute Count: %d\n"), PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM]);
	Output.Appendf(TEXT("    Part Detail Attribute Count: %d\n"), PartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL]);
	Output.Appendf(TEXT("    Part Is Instanced: %d\n"), PartInfo.isInstanced ? 1 : 0);
	Output.Appendf(TEXT("    Instance Count: %d\n"), PartInfo.instanceCount);
	Output.Appendf(TEXT("    Instance Part Count: %d\n"), PartInfo.instancedPartCount ? 1 : 0);

	switch (PartInfo.type)
	{
	case HAPI_PARTTYPE_CURVE:
		HAPI_CurveInfo CurveInfo;
		FHoudiniApi::CurveInfo_Init(&CurveInfo);
		Result = FHoudiniApi::GetCurveInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &CurveInfo);
		if(Result != HAPI_RESULT_SUCCESS)
		{
			Output.Appendf(TEXT("    Failed to get curve info: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());
			return;
		}
		Output.Appendf(TEXT("    Curve:\n"));
		Output.Appendf(TEXT("        Curve Type: %s\n"), *CurveTypeToString(CurveInfo.curveType));
		Output.Appendf(TEXT("        Curve Count: %d\n"), CurveInfo.curveCount);
		Output.Appendf(TEXT("        Vertex Count: %d\n"), CurveInfo.vertexCount);
		Output.Appendf(TEXT("        Knot Count: %d\n"), CurveInfo.knotCount);
		Output.Appendf(TEXT("        Periodic: %d\n"), CurveInfo.isPeriodic ? 1 : 0);
		Output.Appendf(TEXT("        Rational: %d\n"), CurveInfo.isRational ? 1 : 0);
		Output.Appendf(TEXT("        Order: %d\n"), CurveInfo.order);
		Output.Appendf(TEXT("        Has Knots: %d\n"), CurveInfo.hasKnots);
		Output.Appendf(TEXT("        Is Closed: %d\n"), CurveInfo.isClosed ? 1 : 0);
		break;
	case HAPI_PARTTYPE_VOLUME:
		HAPI_VolumeInfo VolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&VolumeInfo);
		Result = FHoudiniApi::GetVolumeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &VolumeInfo);
		if(Result != HAPI_RESULT_SUCCESS)
		{
			Output.Appendf(TEXT("    Failed to get volume info: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());
			return;
		}
		Output.Appendf(TEXT("    Volume:\n"));
		Output.Appendf(TEXT("        X Length: %d\n"), VolumeInfo.xLength);
		Output.Appendf(TEXT("        Y Length: %d\n"), VolumeInfo.yLength);
		Output.Appendf(TEXT("        Z Length: %d\n"), VolumeInfo.zLength);
		Output.Appendf(TEXT("        Tuple Size: %d\n"), VolumeInfo.tupleSize);
		Output.Appendf(TEXT("        Storage: %s\n"), *StorageTypeToString(VolumeInfo.storage));
		Output.Appendf(TEXT("        Tile Size: %d\n"), VolumeInfo.tileSize);
		Output.Appendf(TEXT("        Has Taper: %d\n"), VolumeInfo.hasTaper);
		Output.Appendf(TEXT("        X Taper: %f\n"), VolumeInfo.xTaper);
		Output.Appendf(TEXT("        Y Taper: %f\n"), VolumeInfo.yTaper);
		break;

	case HAPI_PARTTYPE_INSTANCER:
		{
			TArray<HAPI_NodeId> InstancedPartIds;
			InstancedPartIds.SetNum(PartInfo.instancedPartCount);

			Result = FHoudiniApi::GetInstancedPartIds(FHoudiniEngine::Get().GetSession(),
				NodeId, PartId, InstancedPartIds.GetData(), 0, PartInfo.instancedPartCount);
			if(Result != HAPI_RESULT_SUCCESS)
			{
				Output.Appendf(TEXT("    Failed to get instanced part ids: %s\n"), *FHoudiniEngineUtils::GetErrorDescription());
				return;
			}

			Output.Append(TEXT("    Instance Ids: "));
			for (int Index = 0; Index < InstancedPartIds.Num(); Index++)
			{
				Output.Appendf(TEXT("%d "), InstancedPartIds[Index]);
			}
			Output.Appendf(TEXT("\n"));
		}
		break;
	default:
		break;
	}

	TArray<FString> AttrNames = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), 
		NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX);

	for (int32 AttrIdx = 0; AttrIdx < AttrNames.Num(); ++AttrIdx)
	{
		Output.Appendf(TEXT("        Vertex Attribute: %s\n"), *AttrNames[AttrIdx]);
		Output.Append(DumpAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_VERTEX, AttrNames[AttrIdx]));
	}

	AttrNames = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
	                                                   HAPI_AttributeOwner::HAPI_ATTROWNER_POINT);
	for (int32 AttrIdx = 0; AttrIdx < AttrNames.Num(); ++AttrIdx)
	{
		Output.Appendf(TEXT("        Point Attribute: %s\n"), *AttrNames[AttrIdx]);
		Output.Append(DumpAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT, AttrNames[AttrIdx]));
	}

	AttrNames = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
	                                                   HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM);
	for (int32 AttrIdx = 0; AttrIdx < AttrNames.Num(); ++AttrIdx)
	{
		Output.Appendf(TEXT("        Prims Attribute: %s\n"), *AttrNames[AttrIdx]);
		Output.Append(DumpAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM, AttrNames[AttrIdx]));
	}

	AttrNames = FHoudiniEngineUtils::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
	                                                   HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL);
	for (int32 AttrIdx = 0; AttrIdx < AttrNames.Num(); ++AttrIdx)
	{
		Output.Appendf(TEXT("        Detail Attribute: %s\n"), *AttrNames[AttrIdx]);
		Output.Append(DumpAttribute(NodeId, PartId, HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, AttrNames[AttrIdx]));
	}
}

EHoudiniProxyRefineRequestResult
FHoudiniEngineUtils::RefineHoudiniProxyMeshActorArrayToStaticMeshes(const TArray<AHoudiniAssetActor*>& InActorsToRefine, bool bSilent)
{
	const bool bRefineAll = true;
	const bool bOnPreSaveWorld = false;
	UWorld* OnPreSaveWorld = nullptr;
	const bool bOnPreBeginPIE = false;

	// First find the Cookables that have meshes that we must refine
	TArray<UHoudiniCookable*> CookablesToRefine;
	TArray<UHoudiniCookable*> CookablesToCook;
	// Cookables that would be candidates for refinement/cooking, but have errors
	TArray<UHoudiniCookable*> SkippedCookables;
	for(const AHoudiniAssetActor* HoudiniAssetActor : InActorsToRefine)
	{
		if(!IsValid(HoudiniAssetActor))
			continue;

		UHoudiniCookable* HoudiniCookable = HoudiniAssetActor->GetHoudiniCookable();
		if(!IsValid(HoudiniCookable))
			continue;

		// Check if we should consider this component for proxy mesh refinement or cooking, based on its settings and
		// flags passed to the function.
		TriageHoudiniCookablesForProxyMeshRefinement(HoudiniCookable, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE, CookablesToRefine, CookablesToCook, SkippedCookables);
	}

	return RefineTriagedHoudiniProxyMeshesToStaticMeshes(
		CookablesToRefine,
		CookablesToCook,
		SkippedCookables,
		bSilent,
		bRefineAll,
		bOnPreSaveWorld,
		OnPreSaveWorld,
		bOnPreBeginPIE
	);
}


void
FHoudiniEngineUtils::TriageHoudiniCookablesForProxyMeshRefinement(
	UHoudiniCookable* InHC,
	bool bRefineAll,
	bool bOnPreSaveWorld,
	UWorld* OnPreSaveWorld,
	bool bOnPreBeginPIE,
	TArray<UHoudiniCookable*>& OutToRefine,
	TArray<UHoudiniCookable*>& OutToCook,
	TArray<UHoudiniCookable*>& OutSkipped)
{
	if(!IsValid(InHC))
		return;

	// Make sure that the cookable's World and Owner are valid
	AActor* Owner = InHC->GetOwner();
	if(!IsValid(Owner))
		return;

	UWorld* World = InHC->GetWorld();

	// No need to return here if we're just starting PIE
	if(bOnPreSaveWorld && !IsValid(World))
		return;

	if(bOnPreSaveWorld && OnPreSaveWorld && OnPreSaveWorld != World)
		return;

	// Check if we should consider this component for proxy mesh refinement based on its settings and
	// flags passed to the function
	if(bRefineAll ||
		(bOnPreSaveWorld && InHC->IsProxyStaticMeshRefinementOnPreSaveWorldEnabled()) ||
		(bOnPreBeginPIE && InHC->IsProxyStaticMeshRefinementOnPreBeginPIEEnabled()))
	{
		TArray<UPackage*> ProxyMeshPackagesToSave;
		TArray<UHoudiniCookable*> CookablesWithProxiesToSave;

		if(InHC->HasAnyCurrentProxyOutput())
		{
			// Get the state of the asset and check if it is cooked
			// If it is not cook, request a cook. We can only build the UStaticMesh
			// if the data from the cook is available
			// If the state is not pre-cook, or None (cooked), then the state is invalid,
			// log an error and skip the component
			bool bNeedsRebuildOrDelete = false;
			bool bUnsupportedState = false;
			const bool bCookedDataAvailable = InHC->IsHoudiniCookedDataAvailable(bNeedsRebuildOrDelete, bUnsupportedState);
			if(bCookedDataAvailable)
			{
				OutToRefine.Add(InHC);
				CookablesWithProxiesToSave.Add(InHC);
			}
			else if(!bUnsupportedState && !bNeedsRebuildOrDelete)
			{
				InHC->MarkAsNeedCook();
				// Force the output of the cook to be directly created as a UStaticMesh and not a proxy
				InHC->SetNoProxyMeshNextCookRequested(true);
				OutToCook.Add(InHC);
				CookablesWithProxiesToSave.Add(InHC);
			}
			else
			{
				OutSkipped.Add(InHC);
				const EHoudiniAssetState State = InHC->GetCurrentState();
				HOUDINI_LOG_ERROR(TEXT("Could not refine %s, the asset is in an unsupported state: %s"), *(InHC->GetPathName()), *(UEnum::GetValueAsString(State)));
			}
		}
		else if(InHC->HasAnyProxyOutput())
		{
			// If the HC has non-current proxies, destroy them
			// TODO: Make this its own command?
			const uint32 NumOutputs = InHC->GetNumOutputs();
			for(uint32 Index = 0; Index < NumOutputs; ++Index)
			{
				UHoudiniOutput* Output = InHC->GetOutputAt(Index);
				if(!IsValid(Output))
					continue;

				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
				for(auto& CurrentPair : OutputObjects)
				{
					FHoudiniOutputObject& CurrentOutputObject = CurrentPair.Value;
					if(!CurrentOutputObject.bProxyIsCurrent)
					{
						// The proxy is not current, delete it and its component
						USceneComponent* FoundProxyComponent = Cast<USceneComponent>(CurrentOutputObject.ProxyComponent);
						if(IsValid(FoundProxyComponent))
						{
							// Remove from the HoudiniAssetActor
							if(FoundProxyComponent->GetOwner())
								FoundProxyComponent->GetOwner()->RemoveOwnedComponent(FoundProxyComponent);

							FoundProxyComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
							FoundProxyComponent->UnregisterComponent();
							FoundProxyComponent->DestroyComponent();
						}

						UObject* ProxyObject = CurrentOutputObject.ProxyObject;
						if(!IsValid(ProxyObject))
							continue;

						// Just mark the object as garbage and his package as dirty
						// Do not save the package automatically - as will cause crashes in PIE
						ProxyObject->MarkAsGarbage();
						ProxyObject->MarkPackageDirty();
					}
				}
			}
		}

		for(UHoudiniCookable* const HC : CookablesWithProxiesToSave)
		{
			const uint32 NumOutputs = HC->GetNumOutputs();
			for(uint32 Index = 0; Index < NumOutputs; ++Index)
			{
				UHoudiniOutput* Output = HC->GetOutputAt(Index);
				if(!IsValid(Output))
					continue;

				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
				for(auto& CurrentPair : OutputObjects)
				{
					FHoudiniOutputObject& CurrentOutputObject = CurrentPair.Value;
					if(CurrentOutputObject.bProxyIsCurrent && CurrentOutputObject.ProxyObject)
					{
						UPackage* const Package = CurrentOutputObject.ProxyObject->GetPackage();
						if(IsValid(Package) && Package->IsDirty())
							ProxyMeshPackagesToSave.Add(Package);
					}
				}
			}
		}

		if(ProxyMeshPackagesToSave.Num() > 0)
		{
			TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			FEditorFileUtils::PromptForCheckoutAndSave(ProxyMeshPackagesToSave, true, false);
		}
	}
}


EHoudiniProxyRefineRequestResult
FHoudiniEngineUtils::RefineTriagedHoudiniProxyMeshesToStaticMeshes(
	const TArray<UHoudiniCookable*>& InCookablesToRefine,
	const TArray<UHoudiniCookable*>& InCookablesToCook,
	const TArray<UHoudiniCookable*>& InSkippedCookables,
	bool bInSilent,
	bool bInRefineAll,
	bool bInOnPreSaveWorld,
	UWorld* InOnPreSaveWorld,
	bool bInOnPrePIEBeginPlay)
{
	// Slate notification text
	FString Notification = TEXT("Refining Houdini proxy meshes to static meshes...");

	const uint32 NumCookablesToCook = InCookablesToCook.Num();
	const uint32 NumCookablesToRefine = InCookablesToRefine.Num();
	const uint32 NumCookablesToProcess = NumCookablesToCook + NumCookablesToRefine;

	TArray<UHoudiniCookable*> SuccessfulCookables;
	TArray<UHoudiniCookable*> FailedCookables;
	TArray<UHoudiniCookable*> SkippedCookables(InSkippedCookables);

	auto AllowPlayInEditorRefinementFn = [&bInOnPrePIEBeginPlay, &InCookablesToCook, &InCookablesToRefine](bool bEnabled, bool bRefinementDone) {
		if(bInOnPrePIEBeginPlay)
		{
			// Flag the cookables that need cooking / refinement as cookable in PIE mode. 
			// No other cooking will be allowed.
			// Once refinement is done, we'll unset these flags again.
			SetAllowPlayInEditorRefinement(InCookablesToCook, true);
			SetAllowPlayInEditorRefinement(InCookablesToRefine, true);
			if(bRefinementDone)
			{
				// Don't tick during PIE. We'll resume ticking when PIE is stopped.
				FHoudiniEngine::Get().StopTicking(true, false);
			}
		}
		};

	AllowPlayInEditorRefinementFn(true, false);

	if(NumCookablesToProcess > 0)
	{
		// The task progress pointer is potentially going to be shared with a background thread and tasks
		// on the main thread, so make it thread safe
		TSharedPtr<FSlowTask, ESPMode::ThreadSafe> TaskProgress = MakeShareable(new FSlowTask((float)NumCookablesToProcess, FText::FromString(Notification)));
		TaskProgress->Initialize();
		if(!bInSilent)
			TaskProgress->MakeDialog(true);

		// Iterate over the Cookables for which we can build UStaticMesh, and build the meshes
		bool bCancelled = false;
		for(uint32 ComponentIndex = 0; ComponentIndex < NumCookablesToRefine; ++ComponentIndex)
		{
			UHoudiniCookable* Cookable = InCookablesToRefine[ComponentIndex];
			TaskProgress->EnterProgressFrame(1.0f);
			const bool bDestroyProxies = true;
			FHoudiniOutputTranslator::BuildStaticMeshesOnHoudiniProxyMeshOutputs(Cookable, bDestroyProxies);

			SuccessfulCookables.Add(Cookable);

			bCancelled = TaskProgress->ShouldCancel();
			if(bCancelled)
			{
				for(uint32 SkippedIndex = ComponentIndex + 1; SkippedIndex < NumCookablesToRefine; ++SkippedIndex)
				{
					SkippedCookables.Add(InCookablesToRefine[ComponentIndex]);
				}
				break;
			}
		}

		if(bCancelled && NumCookablesToCook > 0)
		{
			for(UHoudiniCookable* const HC : InCookablesToCook)
			{
				SkippedCookables.Add(HC);
			}
		}

		if(NumCookablesToCook > 0 && !bCancelled)
		{
			// Now use an async task to check on the progress of the cooking Cookables
			Async(EAsyncExecution::Thread, [InCookablesToCook, TaskProgress, NumCookablesToProcess,
				bInOnPreSaveWorld, InOnPreSaveWorld,
				SuccessfulCookables, FailedCookables, SkippedCookables]() {
					RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(
						InCookablesToCook, TaskProgress, NumCookablesToProcess, bInOnPreSaveWorld, InOnPreSaveWorld,
						SuccessfulCookables, FailedCookables, SkippedCookables);
				});

			// We have to wait for cook(s) before completing refinement
			return EHoudiniProxyRefineRequestResult::PendingCooks;
		}
		else
		{
			RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(
				NumCookablesToProcess, TaskProgress.Get(), bCancelled, bInOnPreSaveWorld, InOnPreSaveWorld,
				SuccessfulCookables, FailedCookables, SkippedCookables);

			// We didn't have to cook anything, so refinement is complete.
			AllowPlayInEditorRefinementFn(false, true);
			return EHoudiniProxyRefineRequestResult::Refined;
		}
	}

	// Nothing to refine
	AllowPlayInEditorRefinementFn(false, true);
	return EHoudiniProxyRefineRequestResult::None;
}

void
FHoudiniEngineUtils::RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(
	const uint32 InNumTotalCookables,
	FSlowTask* const InTaskProgress,
	const bool bCancelled,
	const bool bOnPreSaveWorld,
	UWorld* const InOnPreSaveWorld,
	const TArray<UHoudiniCookable*>& InSuccessfulCookables,
	const TArray<UHoudiniCookable*>& InFailedCookables,
	const TArray<UHoudiniCookable*>& InSkippedCookables)
{
	FString Notification;
	const uint32 NumSkippedCookables = InSkippedCookables.Num();
	const uint32 NumFailedToCook = InFailedCookables.Num();
	if(NumSkippedCookables + NumFailedToCook > 0)
	{
		if(bCancelled)
		{
			Notification = FString::Printf(TEXT("Refinement cancelled after completing %d / %d cookables. The remaining Cookables were skipped, in an invalid state, or could not be cooked. See the log for details."), NumSkippedCookables + NumFailedToCook, InNumTotalCookables);
		}
		else
		{
			Notification = FString::Printf(TEXT("Failed to refine %d / %d Cookables, the Cookables were in an invalid state, and were either not cooked or could not be cooked. See the log for details."), NumSkippedCookables + NumFailedToCook, InNumTotalCookables);
		}
		FHoudiniEngineUtils::CreateSlateNotification(Notification);
		HOUDINI_LOG_ERROR(TEXT("%s"), *Notification);
	}
	else if(InNumTotalCookables > 0)
	{
		Notification = TEXT("Done: Refining Houdini proxy meshes to static meshes.");
		HOUDINI_LOG_MESSAGE(TEXT("%s"), *Notification);
	}
	if(InTaskProgress)
	{
		InTaskProgress->Destroy();
	}
	if(bOnPreSaveWorld && InSuccessfulCookables.Num() > 0)
	{
		FDelegateHandle& OnPostSaveWorldHandle = FHoudiniEngineUtils::GetOnPostSaveWorldRefineProxyMeshesHandle();
		if(OnPostSaveWorldHandle.IsValid())
		{
			if(FEditorDelegates::PostSaveWorldWithContext.Remove(OnPostSaveWorldHandle))
				OnPostSaveWorldHandle.Reset();
		}

		// Save the dirty static meshes in InSuccessfulCookables OnPostSaveWorld
		// TODO: Remove? This may not be necessary now as we save all dirty temporary cook data in 
		// PostSaveWorldWithContext() already (Static Meshes, Materials...)
		OnPostSaveWorldHandle = FEditorDelegates::PostSaveWorldWithContext.AddLambda(
			[InSuccessfulCookables, bOnPreSaveWorld, InOnPreSaveWorld](UWorld* InWorld, FObjectPostSaveContext InContext)
			{
				if(bOnPreSaveWorld && InOnPreSaveWorld && InOnPreSaveWorld != InWorld)
					return;

				RefineProxyMeshesHandleOnPostSaveWorld(InSuccessfulCookables, InContext.GetSaveFlags(), InWorld, InContext.SaveSucceeded());

				FDelegateHandle& OnPostSaveWorldHandle = FHoudiniEngineUtils::GetOnPostSaveWorldRefineProxyMeshesHandle();
				if(OnPostSaveWorldHandle.IsValid())
				{
					if(FEditorDelegates::PostSaveWorldWithContext.Remove(OnPostSaveWorldHandle))
						OnPostSaveWorldHandle.Reset();
				}
			});
	}

	SetAllowPlayInEditorRefinement(InSuccessfulCookables, false);
	SetAllowPlayInEditorRefinement(InFailedCookables, false);
	SetAllowPlayInEditorRefinement(InSkippedCookables, false);

	// Broadcast refinement result per cookable
	for(UHoudiniCookable* const HC : InSuccessfulCookables)
	{
		if(OnHoudiniProxyMeshesRefinedDelegate.IsBound())
			OnHoudiniProxyMeshesRefinedDelegate.Broadcast(HC, EHoudiniProxyRefineResult::Success);
	}
	for(UHoudiniCookable* const HC : InFailedCookables)
	{
		if(OnHoudiniProxyMeshesRefinedDelegate.IsBound())
			OnHoudiniProxyMeshesRefinedDelegate.Broadcast(HC, EHoudiniProxyRefineResult::Failed);
	}
	for(UHoudiniCookable* const HC : InSkippedCookables)
	{
		if(OnHoudiniProxyMeshesRefinedDelegate.IsBound())
			OnHoudiniProxyMeshesRefinedDelegate.Broadcast(HC, EHoudiniProxyRefineResult::Skipped);
	}

	// Update details to display the new inputs
	FHoudiniEngineUtils::UpdateEditorProperties(true);
}

void
FHoudiniEngineUtils::RefineProxyMeshesHandleOnPostSaveWorld(const TArray<UHoudiniCookable*>& InSuccessfulCookables, uint32 InSaveFlags, UWorld* InWorld, bool bInSuccess)
{
	TArray<UPackage*> PackagesToSave;

	for(UHoudiniCookable* HC : InSuccessfulCookables)
	{
		if(!IsValid(HC))
			continue;

		const int32 NumOutputs = HC->GetNumOutputs();
		for(int32 Index = 0; Index < NumOutputs; ++Index)
		{
			UHoudiniOutput* Output = HC->GetOutputAt(Index);
			if(!IsValid(Output))
				continue;

			if(Output->GetType() != EHoudiniOutputType::Mesh)
				continue;

			for(auto& OutputObjectPair : Output->GetOutputObjects())
			{
				UObject* Obj = OutputObjectPair.Value.OutputObject;
				if(!IsValid(Obj))
					continue;

				UStaticMesh* SM = Cast<UStaticMesh>(Obj);
				if(!SM)
					continue;

				UPackage* Package = SM->GetOutermost();
				if(!IsValid(Package))
					continue;

				if(Package->IsDirty() && Package->IsFullyLoaded() && Package != GetTransientPackage())
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
}

void
FHoudiniEngineUtils::RefineHoudiniProxyMeshesToStaticMeshesWithCookInBackgroundThread(
	const TArray<UHoudiniCookable*>& InCookablesToCook,
	TSharedPtr<FSlowTask, ESPMode::ThreadSafe> InTaskProgress,
	const uint32 InNumCookablesToProcess,
	bool bInOnPreSaveWorld,
	UWorld* InOnPreSaveWorld,
	const TArray<UHoudiniCookable*>& InSuccessfulCookables,
	const TArray<UHoudiniCookable*>& InFailedCookables,
	const TArray<UHoudiniCookable*>& InSkippedCookables)
{
	// Copy to a double linked list so that we can loop through
	// to check progress of each component and remove it easily
	// if it has completed/failed
	TDoubleLinkedList<UHoudiniCookable*> CookList;
	for(UHoudiniCookable* HC : InCookablesToCook)
	{
		CookList.AddTail(HC);
	}

	// Add the successfully cooked Cookables to the incoming successful Cookables (previously refined)
	TArray<UHoudiniCookable*> SuccessfulCookables(InSuccessfulCookables);
	TArray<UHoudiniCookable*> FailedCookables(InFailedCookables);
	TArray<UHoudiniCookable*> SkippedCookables(InSkippedCookables);

	bool bCancelled = false;
	uint32 NumFailedToCook = 0;
	while(CookList.Num() > 0 && !bCancelled)
	{
		TDoubleLinkedList<UHoudiniCookable*>::TDoubleLinkedListNode* Node = CookList.GetHead();
		while(Node && !bCancelled)
		{
			TDoubleLinkedList<UHoudiniCookable*>::TDoubleLinkedListNode* Next = Node->GetNextNode();
			UHoudiniCookable* HC = Node->GetValue();

			if(IsValid(HC))
			{
				const EHoudiniAssetState State = HC->GetCurrentState();
				const EHoudiniAssetStateResult ResultState = HC->GetCurrentStateResult();
				bool bUpdateProgress = false;
				if(State == EHoudiniAssetState::None)
				{
					// Cooked, count as success, remove node
					CookList.RemoveNode(Node);
					SuccessfulCookables.Add(HC);
					bUpdateProgress = true;
				}
				else if(ResultState != EHoudiniAssetStateResult::None && ResultState != EHoudiniAssetStateResult::Working)
				{
					// Failed, remove node
					HOUDINI_LOG_ERROR(TEXT("Failed to cook %s to obtain static mesh."), *(HC->GetPathName()));
					CookList.RemoveNode(Node);
					FailedCookables.Add(HC);
					bUpdateProgress = true;
					NumFailedToCook++;
				}

				if(bUpdateProgress && InTaskProgress.IsValid())
				{
					// Update progress only on the main thread, and check for cancellation request
					bCancelled = Async(EAsyncExecution::TaskGraphMainThread, [InTaskProgress]() {
						InTaskProgress->EnterProgressFrame(1.0f);
						return InTaskProgress->ShouldCancel();
						}).Get();
				}
			}
			else
			{
				SkippedCookables.Add(HC);
				CookList.RemoveNode(Node);
			}

			Node = Next;
		}
		FPlatformProcess::Sleep(0.01f);
	}

	if(bCancelled)
	{
		HOUDINI_LOG_WARNING(TEXT("Mesh refinement cancelled while waiting for %d Cookables to cook."), CookList.Num());
		// Mark any remaining HCs in the cook list as skipped
		TDoubleLinkedList<UHoudiniCookable*>::TDoubleLinkedListNode* Node = CookList.GetHead();
		while(Node)
		{
			TDoubleLinkedList<UHoudiniCookable*>::TDoubleLinkedListNode* const Next = Node->GetNextNode();
			UHoudiniCookable* HC = Node->GetValue();
			if(HC)
				SkippedCookables.Add(HC);
			CookList.RemoveNode(Node);
			Node = Next;
		}
	}

	// Cooking is done, or failed, display the notifications on the main thread
	Async(EAsyncExecution::TaskGraphMainThread, [InNumCookablesToProcess, InTaskProgress, bCancelled,
		bInOnPreSaveWorld, InOnPreSaveWorld,
		SuccessfulCookables, FailedCookables, SkippedCookables]()
		{
			RefineHoudiniProxyMeshesToStaticMeshesNotifyDone(
				InNumCookablesToProcess, InTaskProgress.Get(), bCancelled,
				bInOnPreSaveWorld, InOnPreSaveWorld,
				SuccessfulCookables, FailedCookables, SkippedCookables);
		});
}

FDelegateHandle FHoudiniEngineUtils::OnPostSaveWorldRefineProxyMeshesHandle = FDelegateHandle();

void
FHoudiniEngineUtils::SetAllowPlayInEditorRefinement(
	const TArray<UHoudiniCookable*>& InCookables,
	bool bEnabled)
{
#if WITH_EDITORONLY_DATA
	for(UHoudiniCookable* Cookable : InCookables)
	{
		Cookable->SetAllowPlayInEditorRefinement(false);
	}
#endif
}

FHoudiniPerfTimer::FHoudiniPerfTimer(const FString & InText, bool bPrint)
{
	TotalTime = 0.0;
	CurrentStart = -1.0;
	Text = InText;
	bPrintStats = bPrint;
}

FHoudiniPerfTimer::~FHoudiniPerfTimer()
{
	if (CurrentStart >= 0)
	{
		Stop();
	}

	if(bPrintStats && !Text.IsEmpty())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Timer: %-20s %23f secs."), *Text, TotalTime);
	}
}


double FHoudiniPerfTimer::GetTime()
{
	return TotalTime;
}

void FHoudiniPerfTimer::Start()
{
	CurrentStart = FPlatformTime::Seconds();

}

void FHoudiniPerfTimer::Stop()
{
	if(CurrentStart >= 0)
	{
		TotalTime += FPlatformTime::Seconds() - CurrentStart;
	}
	CurrentStart = -1.0;

}

TArray<char> HoudiniTCHARToUTF(const TCHAR* Text)
{
	int32 Length = FCString::Strlen(Text);
	TArray<char> Result;
	Result.SetNumZeroed(Length + 1);

	FTCHARToUTF8_Convert::Convert(&Result[0], Length, Text, Length);

	return Result;
}

#undef LOCTEXT_NAMESPACE
