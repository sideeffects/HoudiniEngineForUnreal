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
#include "Misc/StringFormatArg.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h"

	// Of course, Windows defines its own GetGeoInfo,
	// So we need to undefine that before including HoudiniApi.h to avoid collision...
	#ifdef GetGeoInfo
		#undef GetGeoInfo
	#endif
#endif

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineString.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniInput.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineRuntime.h"

#if WITH_EDITOR
	#include "SAssetSelectionWidget.h"
#endif

#include "HAPI/HAPI_Version.h"

#include "Misc/Paths.h"
#include "Editor/EditorEngine.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMeshSocket.h"
#include "Async/Async.h"
#include "BlueprintEditor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/MetaData.h"
#include "RawMesh.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
//#include "Kismet/BlueprintEditor.h"
#include "SSCSEditor.h"
#include "SSubobjectEditor.h"
#include "Engine/WorldComposition.h"

#if WITH_EDITOR
	#include "Interfaces/IMainFrameModule.h"
#endif

#include <vector>

#include "AssetRegistryModule.h"
#include "FileHelpers.h"
#include "Factories/WorldFactory.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR
	#include "EditorModeManager.h"
	#include "EditorModes.h"
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

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

const FString
FHoudiniEngineUtils::GetNodeErrorsWarningsAndMessages(const HAPI_NodeId& InNodeId)
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
FHoudiniEngineUtils::GetCookLog(TArray<UHoudiniAssetComponent*>& InHACs)
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
	for (auto& HAC : InHACs)
	{
		if (!IsValid(HAC))
			continue;

		// Get the node errors, warnings and messages
		FString NodeErrors = FHoudiniEngineUtils::GetNodeErrorsWarningsAndMessages(HAC->GetAssetId());
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
FHoudiniEngineUtils::GetAssetHelp(UHoudiniAssetComponent* HoudiniAssetComponent)
{
	FString HelpString = TEXT("");
	if (!HoudiniAssetComponent)
		return HelpString;

	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);
	HAPI_NodeId AssetId = HoudiniAssetComponent->GetAssetId();
	if (AssetId < 0)
		return HelpString;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), HelpString);

	if (!FHoudiniEngineString::ToFString(AssetInfo.helpTextSH, HelpString))
		return HelpString;

	if (!FHoudiniEngineString::ToFString(AssetInfo.helpURLSH, HelpString))
		return HelpString;

	if (HelpString.IsEmpty())
		HelpString = TEXT("No Asset Help Found");

	return HelpString;
}

void
FHoudiniEngineUtils::ConvertUnrealString(const FString & UnrealString, std::string & String)
{
	String = TCHAR_TO_UTF8(*UnrealString);
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
	HOUDINI_LOG_MESSAGE(TEXT(" = Package Id: %d"), InPackage->GetPackageId().ValueForDebugging());
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
FHoudiniEngineUtils::HapiGetWorkitemStateAsString(const HAPI_PDG_WorkitemState& InWorkitemState)
{
	switch (InWorkitemState)
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

	return FString::Printf(TEXT("Unknown HAPI_PDG_WorkitemState %d"), InWorkitemState);
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
			if (FHoudiniEngineRuntimeUtils::SetActorLabel(Actor, NewName))
			{
				HOUDINI_LOG_WARNING(TEXT("Called SetActorLabel(%s) on external actor %s instead of Rename : Explicit naming of an actor that is saved in its own external package is prone to cause name clashes when submitting the file.)"), NewName, *Actor->GetName());
			}
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
	UObject* ExistingObject = StaticFindObject(/*Class=*/ NULL, InActor->GetOuter(), *InName, true);
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

void
FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
	FHoudiniPackageParams& OutPackageParams,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FString &BakeFolder,
	const FString &ObjectName,
	const FString &HoudiniAssetName,
	const FString &HoudiniAssetActorName,
	EPackageReplaceMode InReplaceMode,
	bool bAutomaticallySetAttemptToLoadMissingPackages)
{
	OutPackageParams.GeoId = InIdentifier.GeoId;
	OutPackageParams.ObjectId = InIdentifier.ObjectId;
	OutPackageParams.PartId = InIdentifier.PartId;
	OutPackageParams.BakeFolder = BakeFolder;
	OutPackageParams.PackageMode = EPackageMode::Bake;
	OutPackageParams.ReplaceMode = InReplaceMode;
	OutPackageParams.HoudiniAssetName = HoudiniAssetName;
	OutPackageParams.HoudiniAssetActorName = HoudiniAssetActorName;
	OutPackageParams.ObjectName = ObjectName;
}

void
FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
	UWorld* const InWorldContext,
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniOutputObject& InOutputObject,
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

	// If InHoudiniAssetName was specified, use that, otherwise use the name of the UHoudiniAsset used by the
	// HoudiniAssetComponent
	FString HoudiniAssetName(TEXT(""));
	if (!InHoudiniAssetName.IsEmpty())
	{
		HoudiniAssetName = InHoudiniAssetName;
	}
	else if (IsValid(HoudiniAssetComponent) && IsValid(HoudiniAssetComponent->GetHoudiniAsset()))
	{
		HoudiniAssetName = HoudiniAssetComponent->GetHoudiniAsset()->GetName();
	}
	
	// If InHoudiniAssetActorName was specified, use that, otherwise use the name of the owner of HoudiniAssetComponent
	FString HoudiniAssetActorName(TEXT(""));
	if (!InHoudiniAssetActorName.IsEmpty())
	{
		HoudiniAssetActorName = InHoudiniAssetActorName;
	}
	else if (IsValid(HoudiniAssetComponent) && IsValid(HoudiniAssetComponent->GetOwner()))
	{
		HoudiniAssetActorName = HoudiniAssetComponent->GetOwner()->GetName();
	}

	const bool bHasBakeNameUIOverride = !InOutputObject.BakeName.IsEmpty(); 
	FillInPackageParamsForBakingOutput(
		OutPackageParams,
		InIdentifier,
		DefaultBakeFolder,
		bHasBakeNameUIOverride ? InOutputObject.BakeName : InDefaultObjectName,
		HoudiniAssetName,
		HoudiniAssetActorName,
		InReplaceMode,
		bAutomaticallySetAttemptToLoadMissingPackages);

	const TMap<FString, FString>& CachedAttributes = InOutputObject.CachedAttributes;
	TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
	OutPackageParams.UpdateTokensFromParams(InWorldContext, HoudiniAssetComponent, Tokens);
	OutResolver.SetCachedAttributes(CachedAttributes);
	OutResolver.SetTokensFromStringMap(Tokens);

#if defined(HOUDINI_ENGINE_DEBUG_BAKING) && HOUDINI_ENGINE_DEBUG_BAKING
	// Log the cached attributes and tokens for debugging
	OutResolver.LogCachedAttributesAndTokens();
#endif

	if (!bInSkipObjectNameResolutionAndUseDefault)
	{
		// Resolve the object name
		// TODO: currently the UI override is checked first (this should probably change so that attributes are used first)
		FString ObjectName;
		if (bHasBakeNameUIOverride)
		{
			ObjectName = InOutputObject.BakeName;
		}
		else
		{
			constexpr bool bForBake = true;
			ObjectName = OutResolver.ResolveOutputName(bForBake);
			if (ObjectName.IsEmpty())
				ObjectName = InDefaultObjectName;
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
		OutPackageParams.UpdateTokensFromParams(InWorldContext, HoudiniAssetComponent, Tokens);
		OutResolver.SetTokensFromStringMap(Tokens);

#if defined(HOUDINI_ENGINE_DEBUG_BAKING) && HOUDINI_ENGINE_DEBUG_BAKING
		// Log the final tokens
		OutResolver.LogCachedAttributesAndTokens();
#endif
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
	// Update / repopulate the foliage editor mode's mesh list if the foliage editor mode is active.
	// TODO: find a better way to do this, the relevant functions are in FEdModeFoliage and FFoliageEdModeToolkit are not API exported
	FEditorModeTools& EditorModeTools = GLevelEditorModeTools();
	if (EditorModeTools.IsModeActive(FBuiltinEditorModes::EM_Foliage))
	{
		EditorModeTools.DeactivateMode(FBuiltinEditorModes::EM_Foliage);
		EditorModeTools.ActivateMode(FBuiltinEditorModes::EM_Foliage);
		return true;
	}

	return false;
}

void
FHoudiniEngineUtils::GatherLandscapeInputs(
	UHoudiniAssetComponent* HAC,
	TArray<ALandscapeProxy*>& AllInputLandscapes,
	TArray<ALandscapeProxy*>& InputLandscapesToUpdate)
{
	if (!IsValid(HAC))
		return;

	int32 NumInputs = HAC->GetNumInputs();
	
	for (int32 InputIndex = 0; InputIndex < NumInputs; InputIndex++ )
	{
		UHoudiniInput* CurrentInput = HAC->GetInputAt(InputIndex);
		if (!CurrentInput)
			continue;
		
		if (CurrentInput->GetInputType() == EHoudiniInputType::World)
		{
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
		
		if (CurrentInput->GetInputType() != EHoudiniInputType::Landscape)
			continue;

		// Get the landscape input's landscape
		ALandscapeProxy* InputLandscape = Cast<ALandscapeProxy>(CurrentInput->GetInputObjectAt(0));
		if (!InputLandscape)
			continue;

		AllInputLandscapes.Add(InputLandscape);

		if (CurrentInput->GetUpdateInputLandscape())
		{
			InputLandscapesToUpdate.Add(InputLandscape);
		}
	}
}


UHoudiniAssetComponent*
FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(const UObject* Obj)
{
	if (!IsValid(Obj))
		return nullptr;

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
	bool bCustomPathFound = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->bUseCustomHoudiniLocation)
	{
		// Create full path to libHAPI binary.
		FString CustomHoudiniLocationPath = HoudiniRuntimeSettings->CustomHoudiniLocation.Path;
		if (!CustomHoudiniLocationPath.IsEmpty())
		{
			// Convert path to absolute if it is relative.
			if (FPaths::IsRelative(CustomHoudiniLocationPath))
				CustomHoudiniLocationPath = FPaths::ConvertRelativePathToFull(CustomHoudiniLocationPath);

			FString LibHAPICustomPath = FString::Printf(TEXT("%s/%s"), *CustomHoudiniLocationPath, *LibHAPIName);

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
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);

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
FHoudiniEngineUtils::IsHoudiniNodeValid(const HAPI_NodeId& NodeId)
{
	if (NodeId < 0)
		return false;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	bool ValidationAnswer = 0;

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
FHoudiniEngineUtils::DestroyHoudiniAsset(const HAPI_NodeId& AssetId)
{
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::DeleteNode(
		FHoudiniEngine::Get().GetSession(), AssetId))
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

	// Get the HDA's file path
	// We need to convert relative file path to absolute
	FString AssetFileName = HoudiniAsset->GetAssetFileName();
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
		// Load the asset from file.
		std::string AssetFileNamePlain;
		FHoudiniEngineUtils::ConvertUnrealString(InAssetFileName, AssetFileNamePlain);
		Result = FHoudiniApi::LoadAssetLibraryFromFile(
			FHoudiniEngine::Get().GetSession(), AssetFileNamePlain.c_str(), true, &OutAssetLibraryId);

	};

	// Lambda to load an HDA from memory
	auto LoadAssetFromMemory = [&Result, &OutAssetLibraryId](const UHoudiniAsset* InHoudiniAsset)
	{
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

	return true;
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
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.HasCloseButton(false);

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
FHoudiniEngineUtils::GetHoudiniAssetName(const HAPI_NodeId& AssetNodeId, FString & NameString)
{
	HAPI_AssetInfo AssetInfo;
	if (FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetNodeId, &AssetInfo) == HAPI_RESULT_SUCCESS)
	{
		FHoudiniEngineString HoudiniEngineString(AssetInfo.nameSH);
		return HoudiniEngineString.ToFString(NameString);
	}

	return false;
}

bool
FHoudiniEngineUtils::GetAssetPreset(const HAPI_NodeId& AssetNodeId, TArray< char > & PresetBuffer)
{
	PresetBuffer.Empty();

	HAPI_NodeId NodeId;
	HAPI_AssetInfo AssetInfo;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetNodeId, &AssetInfo))
	{
		NodeId = AssetInfo.nodeId;
	}
	else
		NodeId = AssetNodeId;

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

	PresetBuffer.SetNumZeroed(BufferLength);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPreset(
		FHoudiniEngine::Get().GetSession(), NodeId,
		&PresetBuffer[0], PresetBuffer.Num()), false);

	return true;
}

bool
FHoudiniEngineUtils::HapiGetAbsNodePath(const HAPI_NodeId& InNodeId, FString& OutPath)
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
FHoudiniEngineUtils::HapiGetNodePath(const HAPI_NodeId& InNodeId, const HAPI_NodeId& InRelativeToNodeId, FString& OutPath)
{
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
		// This is a SOP asset, just return the asset name in this case
		HAPI_AssetInfo AssetInfo;
		FHoudiniApi::AssetInfo_Init(&AssetInfo);
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
			FHoudiniEngine::Get().GetSession(), InHGPO.AssetId, &AssetInfo))
		{
			HAPI_NodeInfo AssetNodeInfo;
			FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInfo(
				FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &AssetNodeInfo))
			{				
				if (FHoudiniEngineString::ToFString(AssetNodeInfo.nameSH, NodePathTemp))
				{
					OutPath = FString::Printf(TEXT("%s_%d"), *NodePathTemp, InHGPO.PartId);
				}
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
FHoudiniEngineUtils::HapiGetObjectInfos(const HAPI_NodeId& InNodeId, TArray<HAPI_ObjectInfo>& OutObjectInfos, TArray<HAPI_Transform>& OutObjectTransforms)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), 
		InNodeId, &NodeInfo), false);

	int32 ObjectCount = 0;
	if (NodeInfo.type == HAPI_NODETYPE_SOP)
	{
		ObjectCount = 1;
		OutObjectInfos.SetNumUninitialized(1);
		FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[0]));

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeInfo.parentId, &OutObjectInfos[0]), false);

		// Use the identity transform
		OutObjectTransforms.SetNumUninitialized(1);
		FHoudiniApi::Transform_Init(&(OutObjectTransforms[0]));

		OutObjectTransforms[0].rotationQuaternion[3] = 1.0f;
		OutObjectTransforms[0].scale[0] = 1.0f;
		OutObjectTransforms[0].scale[1] = 1.0f;
		OutObjectTransforms[0].scale[2] = 1.0f;
		OutObjectTransforms[0].rstOrder = HAPI_SRT;
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
			// This OBJ has children
			// See if we should add ourself by looking for immediate display SOP 
			int32 ImmediateSOP = 0;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(), NodeInfo.id,
				HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_DISPLAY,
				false, &ImmediateSOP), false);

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
FHoudiniEngineUtils::IsObjNodeFullyVisible(const TSet<HAPI_NodeId>& AllObjectIds, const HAPI_NodeId& InRootNodeId, const HAPI_NodeId& InChildNodeId)
{
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
FHoudiniEngineUtils::IsSopNode(const HAPI_NodeId& NodeId)
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


bool FHoudiniEngineUtils::ContainsSopNodes(const HAPI_NodeId& NodeId)
{
	int ChildCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			HAPI_NODETYPE_SOP,
			HAPI_NODEFLAGS_ANY,
			false,
			&ChildCount
		),
		false
	);
	return ChildCount > 0;
}

bool FHoudiniEngineUtils::GetOutputIndex(const HAPI_NodeId& InNodeId, int32& OutOutputIndex)
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
	const HAPI_NodeId& AssetId,
	const bool bUseOutputNodes,
	const bool bOutputTemplatedGeos,
	TArray<HAPI_NodeId>& OutOutputNodes)
{
	OutOutputNodes.Empty();
	
	// Ensure the asset has a valid node ID
	if (AssetId < 0)
	{
		return false;
	}

	// Get the AssetInfo
	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	// Get the Asset NodeInfo
	HAPI_NodeInfo AssetNodeInfo;
	FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetNodeInfo), false);

	FString CurrentAssetName;
	{
		FHoudiniEngineString hapiSTR(AssetInfo.nameSH);
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
		HOUDINI_CHECK_ERROR(FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(),
			AssetId, HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE,
			true, &EditableNodeCount));
	}
	
	// All editable nodes will be output, regardless
	// of whether the subnet is considered visible or not.
	if (EditableNodeCount > 0)
	{
		TArray<HAPI_NodeId> EditableNodeIds;
		EditableNodeIds.SetNumUninitialized(EditableNodeCount);
		HOUDINI_CHECK_ERROR(FHoudiniApi::GetComposedChildNodeList(
			FHoudiniEngine::Get().GetSession(), 
			AssetId, EditableNodeIds.GetData(), EditableNodeCount));

		for (int32 nEditable = 0; nEditable < EditableNodeCount; nEditable++)
		{
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
			if (CurrentEditableGeoInfo.type != HAPI_GEOTYPE_CURVE)
				continue;

			// Add this geo to the geo info array
			EditableGeoInfos.Add(CurrentEditableGeoInfo);
		}
	}

	const bool bIsSopAsset = AssetInfo.nodeId != AssetInfo.objectNodeId;
	bool bUseOutputFromSubnets = true;
	if (bAssetHasChildren)
	{
		if (FHoudiniEngineUtils::ContainsSopNodes(AssetInfo.nodeId))
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
		int NumObjSubnets;
		TArray<HAPI_NodeId> ObjectIds;
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::ComposeChildNodeList(
				FHoudiniEngine::Get().GetSession(),
				AssetId,
				HAPI_NODETYPE_OBJ,
				HAPI_NODEFLAGS_OBJ_SUBNET,
				true,
				&NumObjSubnets
				),
			false);

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

		for (const HAPI_NodeId& NodeId : ForceNodesToCook)
		{
			OutOutputNodes.AddUnique(NodeId);
		}
	}
	return true;
}

bool FHoudiniEngineUtils::GatherImmediateOutputGeoInfos(const HAPI_NodeId& InNodeId,
                                                        const bool bUseOutputNodes,
                                                        const bool bGatherTemplateNodes,
                                                        TArray<HAPI_GeoInfo>& OutGeoInfos,
                                                        TSet<HAPI_NodeId>& OutForceNodesCook
)
{
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
					for(const HAPI_NodeId& DisplayNodeId : DisplayNodeIds)
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
				
				for(const HAPI_NodeId& TemplateNodeId : TemplateNodeIds)
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
FHoudiniEngineUtils::HapiGetAssetTransform(const HAPI_NodeId& InNodeId, FTransform& OutTransform)
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
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_Transform & HapiTransform, FTransform & UnrealTransform)
{
	if ( HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM )
	{
		// Swap Y/Z, invert W
		FQuat ObjectRotation(
			HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[2],
			HapiTransform.rotationQuaternion[1], -HapiTransform.rotationQuaternion[3]);

		// Swap Y/Z and scale
		FVector ObjectTranslation(HapiTransform.position[0], HapiTransform.position[2], HapiTransform.position[1]);
		ObjectTranslation *= HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[2], HapiTransform.scale[1]);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
	else
	{
		FQuat ObjectRotation(
			HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[1],
			HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[3]);

		FVector ObjectTranslation(
			HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2]);
		ObjectTranslation *= HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2]);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
}

void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_TransformEuler & HapiTransformEuler, FTransform & UnrealTransform)
{
	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformEulerToMatrix(FHoudiniEngine::Get().GetSession(), &HapiTransformEuler, HapiMatrix);

	HAPI_Transform HapiTransformQuat;
	FMemory::Memzero< HAPI_Transform >(HapiTransformQuat);
	FHoudiniApi::ConvertMatrixToQuat(FHoudiniEngine::Get().GetSession(), HapiMatrix, HAPI_SRT, &HapiTransformQuat);

	FHoudiniEngineUtils::TranslateHapiTransform(HapiTransformQuat, UnrealTransform);
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(const FTransform & UnrealTransform, HAPI_Transform & HapiTransform)
{
	FMemory::Memzero< HAPI_Transform >(HapiTransform);
	HapiTransform.rstOrder = HAPI_SRT;

	FQuat UnrealRotation = UnrealTransform.GetRotation();
	FVector UnrealTranslation = UnrealTransform.GetTranslation();
	FVector UnrealScale = UnrealTransform.GetScale3D();

	if (HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM)
	{
		// Swap Y/Z, invert XYZ
		HapiTransform.rotationQuaternion[0] = -UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = -UnrealRotation.Z;
		HapiTransform.rotationQuaternion[2] = -UnrealRotation.Y;
		HapiTransform.rotationQuaternion[3] = UnrealRotation.W;

		// Swap Y/Z, scale
		HapiTransform.position[0] = UnrealTranslation.X / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransform.position[1] = UnrealTranslation.Z / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransform.position[2] = UnrealTranslation.Y / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		HapiTransform.scale[0] = UnrealScale.X;
		HapiTransform.scale[1] = UnrealScale.Z;
		HapiTransform.scale[2] = UnrealScale.Y;
	}
	else
	{
		HapiTransform.rotationQuaternion[0] = UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = UnrealRotation.Y;
		HapiTransform.rotationQuaternion[2] = UnrealRotation.Z;
		HapiTransform.rotationQuaternion[3] = UnrealRotation.W;

		HapiTransform.position[0] = UnrealTranslation.X;
		HapiTransform.position[1] = UnrealTranslation.Y;
		HapiTransform.position[2] = UnrealTranslation.Z;

		HapiTransform.scale[0] = UnrealScale.X;
		HapiTransform.scale[1] = UnrealScale.Y;
		HapiTransform.scale[2] = UnrealScale.Z;
	}
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(
	const FTransform & UnrealTransform,
	HAPI_TransformEuler & HapiTransformEuler)
{
	FMemory::Memzero< HAPI_TransformEuler >(HapiTransformEuler);

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
		HapiTransformEuler.rotationEuler[0] = -Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = -Rotator.Pitch;
		HapiTransformEuler.rotationEuler[2] = Rotator.Yaw;

		// Swap Y/Z, scale
		HapiTransformEuler.position[0] = UnrealTranslation.X / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransformEuler.position[1] = UnrealTranslation.Z / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransformEuler.position[2] = UnrealTranslation.Y / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		HapiTransformEuler.scale[0] = UnrealScale.X;
		HapiTransformEuler.scale[1] = UnrealScale.Z;
		HapiTransformEuler.scale[2] = UnrealScale.Y;
	}
	else
	{
		const FRotator Rotator = UnrealRotation.Rotator();
		HapiTransformEuler.rotationEuler[0] = Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = Rotator.Yaw;
		HapiTransformEuler.rotationEuler[2] = Rotator.Pitch;

		HapiTransformEuler.position[0] = UnrealTranslation.X;
		HapiTransformEuler.position[1] = UnrealTranslation.Y;
		HapiTransformEuler.position[2] = UnrealTranslation.Z;

		HapiTransformEuler.scale[0] = UnrealScale.X;
		HapiTransformEuler.scale[1] = UnrealScale.Y;
		HapiTransformEuler.scale[2] = UnrealScale.Z;
	}
}

bool
FHoudiniEngineUtils::UploadHACTransform(UHoudiniAssetComponent* HAC)
{
	if (!HAC || !HAC->bUploadTransformsToHoudiniEngine)
		return false;

	// Indicates the HAC has been fully loaded
	// TODO: Check! (replaces fullyloaded)
	if (!HAC->IsFullyLoaded())
		return false;

	if (HAC->GetAssetCookCount() > 0 && HAC->GetAssetId() >= 0)
	{
		if (!FHoudiniEngineUtils::HapiSetAssetTransform(HAC->GetAssetId(), HAC->GetComponentTransform()))
			return false;
	}

	HAC->SetHasComponentTransformChanged(false);

	return true;
}

bool
FHoudiniEngineUtils::HapiSetAssetTransform(const HAPI_NodeId& AssetId, const FTransform & Transform)
{
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
FHoudiniEngineUtils::HapiGetParentNodeId(const HAPI_NodeId& NodeId)
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
FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return;

	// TODO: Necessary??

#if WITH_EDITOR
	HAPI_NodeId AssetId = HAC->GetAssetId();
	if (AssetId < 0)
		return;

	AActor* OwnerActor = HAC->GetOwner();
	if (!OwnerActor)
		return;

	if (!OwnerActor->GetName().StartsWith(AHoudiniAssetActor::StaticClass()->GetName()))
		return;

	// Assign unique actor label based on asset name if it seems to have not been renamed already
	FString UniqueName;
	if (FHoudiniEngineUtils::GetHoudiniAssetName(AssetId, UniqueName))
		FActorLabelUtilities::SetActorLabelUnique(OwnerActor, UniqueName);
#endif
}

bool
FHoudiniEngineUtils::GetLicenseType(FString & LicenseType)
{
	LicenseType = TEXT("");
	HAPI_License LicenseTypeValue = HAPI_LICENSE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetSessionEnvInt(
		FHoudiniEngine::Get().GetSession(), HAPI_SESSIONENVINT_LICENSE,
		(int32 *)&LicenseTypeValue), false);

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

	case HAPI_LICENSE_MAX:
	default:
	{
		return false;
	}
	}

	return true;
}

// Check if the Houdini asset component (or parent HAC of a parameter) is being cooked
bool
FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(UObject* InObj) 
{
	if (!InObj)
		return false;

	UHoudiniAssetComponent* HoudiniAssetComponent = nullptr;

	if (InObj->IsA<UHoudiniAssetComponent>()) 
	{
		HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InObj);
	}
	else if (InObj->IsA<UHoudiniParameter>())
	{
		UHoudiniParameter* Parameter = Cast<UHoudiniParameter>(InObj);
		if (!Parameter)
			return false;

		HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(Parameter->GetOuter());
	}

	if (!HoudiniAssetComponent)
		return false;

	EHoudiniAssetState AssetState = HoudiniAssetComponent->GetAssetState();

	return AssetState >= EHoudiniAssetState::PreCook && AssetState <= EHoudiniAssetState::PostCook;
}

void
FHoudiniEngineUtils::UpdateEditorProperties(UObject* InObjectToUpdate, const bool& InForceFullUpdate)
{
	TArray<UObject*> ObjectsToUpdate;
	ObjectsToUpdate.Add(InObjectToUpdate);

	if (!IsInGameThread())
	{
		// We need to be in the game thread to trigger editor properties update
		AsyncTask(ENamedThreads::GameThread, [ObjectsToUpdate, InForceFullUpdate]()
		{
			FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
		});
	}
	else
	{
		// We're in the game thread, no need  for an async task
		FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
	}
}

void
FHoudiniEngineUtils::UpdateEditorProperties(TArray<UObject*> ObjectsToUpdate, const bool& InForceFullUpdate)
{
	if (!IsInGameThread())
	{
		// We need to be in the game thread to trigger editor properties update
		AsyncTask(ENamedThreads::GameThread, [ObjectsToUpdate, InForceFullUpdate]()
		{
			FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
		});
	}
	else
	{
		// We're in the game thread, no need  for an async task
		FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
	}
}

void FHoudiniEngineUtils::UpdateBlueprintEditor(UHoudiniAssetComponent* HAC)
{
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
FHoudiniEngineUtils::UpdateEditorProperties_Internal(TArray<UObject*> ObjectsToUpdate, const bool& bInForceFullUpdate)
{
	// TODO: Don't use this method. Prefer using IDetailLayoutBuilder::ForceRefreshDetails(). 
	// Example to correctly update details panel through IDetailCategoryBuilder / IDetailLayoutBuilder
	// IDetailCategoryBuilder &CategoryBuilder = StructBuilder.GetParentCategory();
    // IDetailLayoutBuilder &LayoutBuilder = CategoryBuilder.GetParentLayout();
    // LayoutBuilder.ForceRefreshDetails();

#if WITH_EDITOR	
	if (!bInForceFullUpdate)
	{
		// bNeedFullUpdate is false only when small changes (parameters value) have been made
		// We do not reselect the actor to avoid loosing the currently selected parameter
		if(GUnrealEd)
			GUnrealEd->UpdateFloatingPropertyWindows();

		return;
	}

	// We now want to get all the components/actors owning the objects to update
	TArray<USceneComponent*> AllSceneComponents;
	for (auto CurrentObject : ObjectsToUpdate)
	{
		if (!IsValid(CurrentObject))
			continue;

		// In some case, the object itself is the component
		USceneComponent* SceneComp = Cast<USceneComponent>(CurrentObject);
		if (!SceneComp)
		{
			SceneComp = Cast<USceneComponent>(CurrentObject->GetOuter());
		}

		if (IsValid(SceneComp))
		{
			AllSceneComponents.Add(SceneComp);
			continue;
		}
	}

	TArray<AActor*> AllActors;
	for (auto CurrentSceneComp : AllSceneComponents)
	{
		if (!IsValid(CurrentSceneComp))
			continue;

		AActor* Actor = CurrentSceneComp->GetOwner();
		if (IsValid(Actor))
			AllActors.Add(Actor);
	}

	// Updating the editor properties can be done in two ways, depending if we're in the BP editor or not
	// If we have a parent actor, we're not in the BP Editor, so update via the property editor module
	if (AllActors.Num() > 0)
	{
		// Get the property editor module
		FPropertyEditorModule& PropertyModule =
			FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

		// This will actually force a refresh of all the details view
		//PropertyModule.NotifyCustomizationModuleChanged();

		TArray<UObject*> SelectedActors;
		for (auto Actor : AllActors)
		{
			if (Actor && Actor->IsSelected())
				SelectedActors.Add(Actor);
		}

		if (SelectedActors.Num() > 0)
		{
			PropertyModule.UpdatePropertyViews(SelectedActors);
		}

		// We want to iterate on all the details panel
		static const FName DetailsTabIdentifiers[] =
		{
			"LevelEditorSelectionDetails",
			"LevelEditorSelectionDetails2",
			"LevelEditorSelectionDetails3",
			"LevelEditorSelectionDetails4"
		};

		for (const FName& DetailsPanelName : DetailsTabIdentifiers)
		{
			// Locate the details panel.
			TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);
			if (!DetailsView.IsValid())
			{
				// We have no details panel, nothing to update.
				continue;
			}

			// Get the selected actors for this details panels and check if one of ours belongs to it
			const TArray<TWeakObjectPtr<AActor>>& SelectedDetailActors = DetailsView->GetSelectedActors();
			bool bFoundActor = false;
			for (int32 ActorIdx = 0; ActorIdx < SelectedDetailActors.Num(); ActorIdx++)
			{
				TWeakObjectPtr<AActor> SelectedActor = SelectedDetailActors[ActorIdx];
				if (SelectedActor.IsValid() && AllActors.Contains(SelectedActor.Get()))
				{
					bFoundActor = true;
					break;
				}
			}
			
			// None of our actors belongs to this detail panel, no need to update it
			if (!bFoundActor)
				continue;

			// Refresh that details panels using its current selection
			TArray<UObject*> Selection;
			for (auto DetailsActor : SelectedDetailActors)
			{
				if (DetailsActor.IsValid())
					Selection.Add(DetailsActor.Get());
			}

			// Reset selected actors, force refresh and override the lock.
			DetailsView->SetObjects(SelectedActors, bInForceFullUpdate, true);

			if (GUnrealEd)
				GUnrealEd->UpdateFloatingPropertyWindows();
		}
	}
	else
	{
		// TODO: Do we need to do Blueprint Editor updates here or can we confine it to "post output processing"?
		
	}

	/*
	// Reset the full update flag
	if (bNeedFullUpdate)
		HAC->SetEditorPropertiesNeedFullUpdate(false);
	*/

	return;
#endif
}

void FHoudiniEngineUtils::UpdateBlueprintEditor_Internal(UHoudiniAssetComponent* HAC)
{
	//UHoudiniAssetComponent* HACTemplate = HAC->GetCachedTemplate();
	//UBlueprintGeneratedClass* OwnerBPClass = Cast<UBlueprintGeneratedClass>(HACTemplate->GetOuter());
	//if (!OwnerBPClass)
	//	return;

	///*
	//FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(FAssetEditorManager::Get().FindEditorForAsset(OwnerBPClass->ClassGeneratedBy, false));
	//if (!BlueprintEditor)
	//	return;
	//*/

	//// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
	//UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	//FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(AssetEditorSubsystem->FindEditorForAsset(OwnerBPClass->ClassGeneratedBy, false));
	//if (!BlueprintEditor)
	//	return;
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
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	if (InFloatData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	return FHoudiniEngineUtils::HapiSetAttributeFloatData(InFloatData.GetData(), InNodeId, InPartId, InAttributeName, InAttributeInfo);
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeFloatData(
	const float* InFloatData,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
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

			Result = FHoudiniApi::SetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InFloatData,
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
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	if (InIntData.Num() != InAttributeInfo.count * InAttributeInfo.tupleSize)
		return HAPI_RESULT_INVALID_ARGUMENT;

	return FHoudiniEngineUtils::HapiSetAttributeIntData(
		InIntData.GetData(), InNodeId, InPartId, InAttributeName, InAttributeInfo);
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeIntData(	
	const int32* InIntData,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
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

			Result = FHoudiniApi::SetAttributeIntData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, TCHAR_TO_ANSI(*InAttributeName),
				&InAttributeInfo, InIntData,
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
FHoudiniEngineUtils::HapiSetVertexList(
	const TArray<int32>& InVertexListData,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId)
{
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
				InNodeId, InPartId, InVertexListData.GetData(), ChunkStart, CurCount);

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
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId)
{
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
				InNodeId, InPartId, InFaceCounts.GetData(), ChunkStart, CurCount);

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
FHoudiniEngineUtils::HapiSetAttributeStringData(
	const FString& InString,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	TArray<FString> StringArray;
	StringArray.Add(InString);

	return HapiSetAttributeStringData(StringArray, InNodeId, InPartId, InAttributeName, InAttributeInfo);
}

HAPI_Result
FHoudiniEngineUtils::HapiSetAttributeStringData(
	const TArray<FString>& InStringArray, 
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo )
{
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
				&InAttributeInfo, StringDataArray.GetData(),
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
FHoudiniEngineUtils::HapiSetHeightFieldData(
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const TArray<float>& InFloatValues,
	const FString& InHeightfieldName)
{
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


HAPI_Result
FHoudiniEngineUtils::HapiGetHeightFieldData(
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	TArray<float>& OutFloatValues)
{
	int32 NumValues = OutFloatValues.Num();
	if (NumValues < 1)
		return HAPI_RESULT_INVALID_ARGUMENT;

	// float data
	float* HeightData = OutFloatValues.GetData();

	int32 ChunkSize = THRIFT_MAX_CHUNKSIZE;
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if (NumValues > ChunkSize)
	{
		// Get the heightfield data in chunks
		for (int32 ChunkStart = 0; ChunkStart < NumValues; ChunkStart += ChunkSize)
		{
			int32 CurCount = NumValues - ChunkStart > ChunkSize ? ChunkSize : NumValues - ChunkStart;

			Result = FHoudiniApi::GetHeightFieldData(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, InPartId, &HeightData[ChunkStart], ChunkStart, CurCount);

			if (Result != HAPI_RESULT_SUCCESS)
				break;
		}
	}
	else
	{
		Result = FHoudiniApi::GetHeightFieldData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, HeightData, 0, NumValues);
	}

	return Result;
}

char *
FHoudiniEngineUtils::ExtractRawString(const FString& InString)
{
	// Return an empty string instead of returning null to avoid potential crashes
	std::string ConvertedString("");
	if (!InString.IsEmpty())
		ConvertedString = TCHAR_TO_UTF8(*InString);

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
FHoudiniEngineUtils::AddHoudiniLogoToComponent(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return false;

	// No need to add another component if we already show the logo
	if (FHoudiniEngineUtils::HasHoudiniLogo(HAC))
		return true;

	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	UStaticMeshComponent * HoudiniLogoSMC = NewObject< UStaticMeshComponent >(
		HAC, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);

	if (!HoudiniLogoSMC)
		return false;

	HoudiniLogoSMC->SetStaticMesh(HoudiniLogoSM);
	HoudiniLogoSMC->SetVisibility(true);
	HoudiniLogoSMC->SetHiddenInGame(true);
	// Attach created static mesh component to our Houdini component.
	HoudiniLogoSMC->AttachToComponent(HAC, FAttachmentTransformRules::KeepRelativeTransform);
	HoudiniLogoSMC->RegisterComponent();

	return true;
}

bool
FHoudiniEngineUtils::RemoveHoudiniLogoFromComponent(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return false;

	// Get the Houdini Logo SM
	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : HAC->GetAttachChildren())
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
FHoudiniEngineUtils::HasHoudiniLogo(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return false;

	// Get the Houdini Logo SM
	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : HAC->GetAttachChildren())
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
	const HAPI_NodeId& GeoId,
	const HAPI_PartInfo& PartInfo,
	const FString& GroupName,
	const TArray<int32>& FullVertexList,
	TArray<int32>& NewVertexList,
	TArray<int32>& AllVertexList,
	TArray<int32>& AllFaceList,
	TArray<int32>& AllGroupFaceIndices,
	int32& FirstValidVertex,
	int32& FirstValidPrim,
	const bool& isPackedPrim)
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
		if (AllVertexList.IsValidIndex(LastVertexIdx))
		{
			AllVertexList[FirstVertexIdx] = 1;
			AllVertexList[SecondVertexIdx] = 1;
			AllVertexList[LastVertexIdx] = 1;
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
	const HAPI_NodeId& GeoId, const HAPI_PartId& PartId,
	const HAPI_GroupType& GroupType, const bool& isPackedPrim,
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

bool
FHoudiniEngineUtils::HapiGetGroupMembership(
	const HAPI_NodeId& GeoId, const HAPI_PartInfo& PartInfo,
	const HAPI_GroupType& GroupType, const FString & GroupName,
	TArray<int32>& OutGroupMembership, bool& OutAllEquals)
{
	int32 ElementCount = (GroupType == HAPI_GROUPTYPE_POINT) ? PartInfo.pointCount : PartInfo.faceCount;	
	if (ElementCount < 1)
		return false;
	OutGroupMembership.SetNum(ElementCount);

	OutAllEquals = false;
	std::string ConvertedGroupName = TCHAR_TO_UTF8(*GroupName);
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
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& OutAttributeInfo,
	TArray<float>& OutData,
	int32 InTupleSize,
	HAPI_AttributeOwner InOwner,
	const int32& InStartIndex,
	const int32& InCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FHoudiniEngineUtils::HapiGetAttributeDataAsFloat"));

	OutAttributeInfo.exists = false;

	// Reset container size.
	OutData.SetNumUninitialized(0);

	int32 OriginalTupleSize = InTupleSize;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				InGeoId, InPartId, InAttribName,
				(HAPI_AttributeOwner)AttrIdx, &AttributeInfo), false);

			if (AttributeInfo.exists)
				break;
		}
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(), 
			InGeoId, InPartId, InAttribName,
			InOwner, &AttributeInfo), false);
	}

	if (!AttributeInfo.exists)
		return false;

	if (OriginalTupleSize > 0)
		AttributeInfo.tupleSize = OriginalTupleSize;

	// Store the retrieved attribute information.
	OutAttributeInfo = AttributeInfo;

	// Handle partial reading of attributes
	int32 Start = 0;
	if (InStartIndex > 0 && InStartIndex < AttributeInfo.count)
		Start = InStartIndex;

	int32 Count = AttributeInfo.count;
	if (InCount > 0)
	{
		if ((Start + InCount) <= AttributeInfo.count)
			Count = InCount;
		else
			Count = AttributeInfo.count - Start;
	}

	if (AttributeInfo.storage == HAPI_STORAGETYPE_FLOAT)
	{
		// Allocate sufficient buffer for data.
		OutData.SetNum(Count * AttributeInfo.tupleSize);

		// Fetch the values
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			&AttributeInfo, -1, &OutData[0],
			Start, Count), false);

		return true;
	}
	else if (AttributeInfo.storage == HAPI_STORAGETYPE_INT)
	{
		// Expected Float, found an int, try to convert the attribute

		// Allocate sufficient buffer for data.
		TArray<int32> IntData;
		IntData.SetNum(Count * AttributeInfo.tupleSize);

		// Fetch the values
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeIntData(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			&AttributeInfo,	-1,	&IntData[0],
			Start, Count))
		{
			OutData.SetNum(IntData.Num());
			for (int32 Idx = 0; Idx < IntData.Num(); Idx++)
			{
				OutData[Idx] = (float)IntData[Idx];
			}

			HOUDINI_LOG_MESSAGE(TEXT("Attribute %s was expected to be a float attribute, its value had to be converted from integer."), *FString(InAttribName));
			return true;
		}
	}
	else if (AttributeInfo.storage == HAPI_STORAGETYPE_STRING)
	{
		// Expected Float, found a string, try to convert the attribute
		TArray<FString> StringData;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(
			InGeoId, InPartId, InAttribName,
			AttributeInfo, StringData,
			Start, Count))
		{
			bool bConversionError = false;
			OutData.SetNum(StringData.Num());
			for (int32 Idx = 0; Idx < StringData.Num(); Idx++)
			{
				if (StringData[Idx].IsNumeric())
					OutData[Idx] = FCString::Atof(*StringData[Idx]);
				else
					bConversionError = true;
			}

			if (!bConversionError)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Attribute %s was expected to be a float attribute, its value had to be converted from string."), *FString(InAttribName));
				return true;
			}
		}
	}

	HOUDINI_LOG_WARNING(TEXT("Found attribute %s, but it was expected to be a float attribute and is of an invalid type."), *FString(InAttribName));
	return false;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& OutAttributeInfo,
	TArray<int32>& OutData,
	const int32& InTupleSize,
	const HAPI_AttributeOwner& InOwner,
	const int32& InStartIndex,
	const int32& InCount)
{
	OutAttributeInfo.exists = false;

	// Reset container size.
	OutData.SetNumUninitialized(0);

	int32 OriginalTupleSize = InTupleSize;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				InGeoId, InPartId, InAttribName,
				(HAPI_AttributeOwner)AttrIdx, &AttributeInfo), false);

			if (AttributeInfo.exists)
				break;
		}
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			InOwner, &AttributeInfo), false);
	}

	if (!AttributeInfo.exists)
		return false;

	if (OriginalTupleSize > 0)
		AttributeInfo.tupleSize = OriginalTupleSize;

	// Store the retrieved attribute information.
	OutAttributeInfo = AttributeInfo;

	// Handle partial reading of attributes
	int32 Start = 0;
	if (InStartIndex > 0 && InStartIndex < AttributeInfo.count)
		Start = InStartIndex;

	int32 Count = AttributeInfo.count;
	if (InCount > 0)
	{
		if ((Start + InCount) <= AttributeInfo.count)
			Count = InCount;
		else
			Count = AttributeInfo.count - Start;
	}

	if (AttributeInfo.storage == HAPI_STORAGETYPE_INT)
	{
		// Allocate sufficient buffer for data.
		OutData.SetNum(Count * AttributeInfo.tupleSize);

		// Fetch the values
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			&AttributeInfo, -1, &OutData[0], Start, Count), false);

		return true;
	}
	else if (AttributeInfo.storage == HAPI_STORAGETYPE_FLOAT)
	{
		// Expected Int, found a float, try to convert the attribute

		// Allocate sufficient buffer for data.
		TArray<float> FloatData;
		FloatData.SetNum(Count * AttributeInfo.tupleSize);

		// Fetch the float values
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			&AttributeInfo, -1, &FloatData[0], Start, Count))
		{
			OutData.SetNum(FloatData.Num());
			for (int32 Idx = 0; Idx < FloatData.Num(); Idx++)
			{
				OutData[Idx] = (int32)FloatData[Idx];
			}

			HOUDINI_LOG_MESSAGE(TEXT("Attribute %s was expected to be an integer attribute, its value had to be converted from float."), *FString(InAttribName));

			return true;
		}
	}
	else if (AttributeInfo.storage == HAPI_STORAGETYPE_STRING)
	{
		// Expected Int, found a string, try to convert the attribute
		TArray<FString> StringData;
		if(FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(
			InGeoId, InPartId, InAttribName,
			AttributeInfo, StringData,
			Start, Count))
		{
			bool bConversionError = false;
			OutData.SetNum(StringData.Num());
			for (int32 Idx = 0; Idx < StringData.Num(); Idx++)
			{
				if (StringData[Idx].IsNumeric())
					OutData[Idx] = FCString::Atoi(*StringData[Idx]);
				else
					bConversionError = true;
			}

			if (!bConversionError)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Attribute %s was expected to be an integer attribute, its value had to be converted from string."), *FString(InAttribName));
				return true;
			}
		}
	}

	HOUDINI_LOG_WARNING(TEXT("Found attribute %s, but it was expected to be an integer attribute and is of an invalid type."), *FString(InAttribName));
	return false;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& OutAttributeInfo,
	TArray<FString>& OutData,
	int32 InTupleSize,
	HAPI_AttributeOwner InOwner,
	const int32& InStartIndex,
	const int32& InCount)
{
	OutAttributeInfo.exists = false;

	// Reset container size.
	OutData.SetNumUninitialized(0);

	int32 OriginalTupleSize = InTupleSize;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				InGeoId, InPartId, InAttribName,
				(HAPI_AttributeOwner)AttrIdx, &AttributeInfo), false);

			if (AttributeInfo.exists)
				break;
		}
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			InOwner, &AttributeInfo), false);
	}

	if (!AttributeInfo.exists)
		return false;

	// Store the retrieved attribute information.
	OutAttributeInfo = AttributeInfo;

	if (OriginalTupleSize > 0)
		AttributeInfo.tupleSize = OriginalTupleSize;

	// Handle partial reading of attributes
	int32 Start = 0;
	if (InStartIndex > 0 && InStartIndex < AttributeInfo.count)
		Start = InStartIndex;

	int32 Count = AttributeInfo.count;
	if (InCount > 0)
	{
		if ((Start + InCount) <= AttributeInfo.count)
			Count = InCount;
		else
			Count = AttributeInfo.count - Start;
	}

	if (AttributeInfo.storage == HAPI_STORAGETYPE_STRING)
	{
		return FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(
			InGeoId, InPartId, InAttribName, 
			AttributeInfo, OutData,
			Start, Count);
	}
	else if (AttributeInfo.storage == HAPI_STORAGETYPE_FLOAT)
	{
		// Expected string, found a float, try to convert the attribute
		
		// Allocate sufficient buffer for data.
		TArray<float> FloatData;
		FloatData.SetNum(Count * AttributeInfo.tupleSize);

		// Fetch the float values
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			&AttributeInfo, -1, &FloatData[0], 
			Start, Count))
		{
			OutData.SetNum(FloatData.Num());
			for (int32 Idx = 0; Idx < FloatData.Num(); Idx++)
			{
				OutData[Idx] = FString::SanitizeFloat(FloatData[Idx]);
			}

			HOUDINI_LOG_MESSAGE(TEXT("Attribute %s was expected to be a string attribute, its value had to be converted from float."), *FString(InAttribName));
			return true;
		}
	}
	else if (AttributeInfo.storage == HAPI_STORAGETYPE_INT)
	{
		// Expected String, found an int, try to convert the attribute
		
		// Allocate sufficient buffer for data.
		TArray<int32> IntData;
		IntData.SetNum(Count * AttributeInfo.tupleSize);

		// Fetch the values
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeIntData(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			&AttributeInfo, -1, &IntData[0],
			Start, Count))
		{
			OutData.SetNum(IntData.Num());
			for (int32 Idx = 0; Idx < IntData.Num(); Idx++)
			{
				OutData[Idx] = FString::FromInt(IntData[Idx]);
			}

			HOUDINI_LOG_MESSAGE(TEXT("Attribute %s was expected to be a string attribute, its value had to be converted from integer."), *FString(InAttribName));
			return true;
		}
	}
		
	HOUDINI_LOG_WARNING(TEXT("Found attribute %s, but it was expected to be a string attribute and is of an invalid type."), *FString(InAttribName));
	return false;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& InAttributeInfo,
	TArray<FString>& OutData,
	const int32& InStartIndex,
	const int32& InCount)
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
	const HAPI_NodeId& GeoId, const HAPI_PartId& PartId,
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
FHoudiniEngineUtils::IsAttributeInstancer(const HAPI_NodeId& GeoId, const HAPI_PartId& PartId, EHoudiniInstancerType& OutInstancerType)
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
FHoudiniEngineUtils::HapiGetParameterDataAsString(
	const HAPI_NodeId& NodeId, 
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
	const HAPI_NodeId& NodeId,
	const std::string& ParmName,
	const int32& DefaultValue,
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
	const HAPI_NodeId& NodeId,
	const std::string& ParmName,
	const float& DefaultValue,
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

bool FHoudiniEngineUtils::HapiGetAttributeIntOrIntArray(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId,
	const FString& AttribName, const HAPI_AttributeOwner& AttributeOwner, HAPI_AttributeInfo& OutAttributeInfo,
	TArray<int32>& OutData)
{
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId,
		TCHAR_TO_UTF8(*AttribName), AttributeOwner, &OutAttributeInfo), false);

	if (OutAttributeInfo.storage == HAPI_STORAGETYPE_INT)
	{
		OutData.SetNumZeroed(1);
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			TCHAR_TO_UTF8(*AttribName), &OutAttributeInfo, -1, OutData.GetData(), 0, 1), false);

		return true;
	}
	else if (OutAttributeInfo.storage == HAPI_STORAGETYPE_INT_ARRAY)
	{
		TArray<int32> ArraySizes;
		
		OutData.SetNumZeroed(OutAttributeInfo.totalArrayElements);
		ArraySizes.SetNumZeroed(OutAttributeInfo.totalArrayElements);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntArrayData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			TCHAR_TO_UTF8(*AttribName),
			&OutAttributeInfo,
			OutData.GetData(),
			OutAttributeInfo.totalArrayElements,
			ArraySizes.GetData(),
			0,
			OutAttributeInfo.count), false);
		
		if (OutData.Num() > 0)
		{
			return true;
		}
	}
	
	return false;
}

bool FHoudiniEngineUtils::HapiGetAttributeFloatOrFloatArray(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, const FString & AttribName,
	const HAPI_AttributeOwner& AttributeOwner, HAPI_AttributeInfo& OutAttributeInfo, TArray<float>& OutData)
{

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId,
		TCHAR_TO_UTF8(*AttribName), AttributeOwner, &OutAttributeInfo), false);

	if (OutAttributeInfo.storage == HAPI_STORAGETYPE_FLOAT)
	{
		OutData.SetNumZeroed(1);
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			TCHAR_TO_UTF8(*AttribName), &OutAttributeInfo, -1, (float*)OutData.GetData(), 0, 1), false);

		return true;
	}
	else if (OutAttributeInfo.storage == HAPI_STORAGETYPE_FLOAT_ARRAY)
	{
		TArray<int32> FloatDataSizes;
		
		OutData.SetNumZeroed(OutAttributeInfo.totalArrayElements);
		FloatDataSizes.SetNumZeroed(OutAttributeInfo.totalArrayElements);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatArrayData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId,
			TCHAR_TO_UTF8(*AttribName),
			&OutAttributeInfo,
			(float*)OutData.GetData(),
			OutAttributeInfo.totalArrayElements,
			FloatDataSizes.GetData(),
			0,
			OutAttributeInfo.count), false);
		
		if (OutData.Num() > 0)
		{
			return true;
		}
	}
	
	return false;
}

HAPI_ParmId
FHoudiniEngineUtils::HapiFindParameterByName(const HAPI_NodeId& InNodeId, const std::string& InParmName, HAPI_ParmInfo& OutFoundParmInfo)
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
FHoudiniEngineUtils::HapiFindParameterByTag(const HAPI_NodeId& InNodeId, const std::string& InParmTag, HAPI_ParmInfo& OutFoundParmInfo)
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
	const HAPI_NodeId& GeoId,
	const HAPI_NodeId& PartId,
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
			GeoId, PartId, TCHAR_TO_UTF8(*HapiString),
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
	const HAPI_NodeId& GeoId,
	const HAPI_PartId& PartId,
	TArray< FHoudiniMeshSocket >& AllSockets,
	const bool& isPackedPrim)
{
	int32 FoundSocketCount = 0;

	// Attributes we are interested in.
	// Position
	TArray<float> Positions;
	HAPI_AttributeInfo AttribInfoPositions;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

	// Rotation
	bool bHasRotation = false;
	TArray<float> Rotations;
	HAPI_AttributeInfo AttribInfoRotations;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

	// Scale
	bool bHasScale = false;
	TArray<float> Scales;
	HAPI_AttributeInfo AttribInfoScales;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

	// Socket Name
	bool bHasNames = false;
	TArray<FString> Names;
	HAPI_AttributeInfo AttribInfoNames;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

	// Socket Actor
	bool bHasActors = false;
	TArray<FString> Actors;
	HAPI_AttributeInfo AttribInfoActors;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

	// Socket Tags
	bool bHasTags = false;
	TArray<FString> Tags;
	HAPI_AttributeInfo AttribInfoTags;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);

	// Lambda function for creating the socket and adding it to the array
	// Shared between the by Attribute / by Group methods	
	auto AddSocketToArray = [&](const int32& PointIdx)
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
		// Position
		Positions.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

		// Rotation
		bHasRotation = false;
		Rotations.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

		// Scale
		bHasScale = false;
		Scales.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

		// Socket Name
		bHasNames = false;
		Names.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

		// Socket Actor
		bHasActors = false;
		Actors.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

		// Socket Tags
		bHasTags = false;
		Tags.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);
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
		if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId, PartId, TCHAR_TO_ANSI(*SocketPosAttr),
			AttribInfoPositions, Positions, 0, HAPI_ATTROWNER_DETAIL))
			break;

		if (!AttribInfoPositions.exists)
		{
			// No need to keep looking for socket attributes
			HasSocketAttributes = false;
			break;
		}

		// Retrieve rotation data.
		FString SocketRotAttr = SocketAttrPrefix + TEXT("_rot");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketRotAttr), AttribInfoRotations, Rotations, 0, HAPI_ATTROWNER_DETAIL))
			bHasRotation = true;

		// Retrieve scale data.
		FString SocketScaleAttr = SocketAttrPrefix + TEXT("_scale");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketScaleAttr), AttribInfoScales, Scales, 0, HAPI_ATTROWNER_DETAIL))
			bHasScale = true;

		// Retrieve mesh socket names.
		FString SocketNameAttr = SocketAttrPrefix + TEXT("_name");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketNameAttr), AttribInfoNames, Names))
			bHasNames = true;

		// Retrieve mesh socket actor.
		FString SocketActorAttr = SocketAttrPrefix + TEXT("_actor");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketActorAttr), AttribInfoActors, Actors))
			bHasActors = true;

		// Retrieve mesh socket tags.
		FString SocketTagAttr = SocketAttrPrefix + TEXT("_tag");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketTagAttr), AttribInfoTags, Tags))
			bHasTags = true;

		// Add the socket to the array
		AddSocketToArray(0);

		// Try to find the next socket
		SocketIdx++;
	}

	return FoundSocketCount;
}


int32
FHoudiniEngineUtils::AddMeshSocketsToArray_Group(
	const HAPI_NodeId& GeoId,
	const HAPI_PartId& PartId,
	TArray<FHoudiniMeshSocket>& AllSockets,
	const bool& isPackedPrim)
{
	// Attributes we are interested in.
	// Position
	TArray<float> Positions;
	HAPI_AttributeInfo AttribInfoPositions;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

	// Rotation
	bool bHasRotation = false;
	TArray<float> Rotations;
	HAPI_AttributeInfo AttribInfoRotations;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

	// Scale
	bool bHasScale = false;
	TArray<float> Scales;
	HAPI_AttributeInfo AttribInfoScales;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

	// We can also get the sockets rotation from the normal
	bool bHasNormals = false;
	TArray<float> Normals;
	HAPI_AttributeInfo AttribInfoNormals;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNormals);

	// Socket Name
	bool bHasNames = false;
	TArray<FString> Names;
	HAPI_AttributeInfo AttribInfoNames;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

	// Socket Actor
	bool bHasActors = false;
	TArray<FString> Actors;
	HAPI_AttributeInfo AttribInfoActors;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

	// Socket Tags
	bool bHasTags = false;
	TArray<FString> Tags;
	HAPI_AttributeInfo AttribInfoTags;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);

	// Lambda function for creating the socket and adding it to the array
	// Shared between the by Attribute / by Group methods
	int32 FoundSocketCount = 0;
	auto AddSocketToArray = [&](const int32& PointIdx)
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
		FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

		// Rotation
		bHasRotation = false;
		Rotations.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

		// Scale
		bHasScale = false;
		Scales.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

		// When using socket groups, we can also get the sockets rotation from the normal
		bHasNormals = false;
		Normals.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoNormals);

		// Socket Name
		bHasNames = false;
		Names.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

		// Socket Actor
		bHasActors = false;
		Actors.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

		// Socket Tags
		bHasTags = false;
		Tags.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);
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

	// Retrieve position data.
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions))
		return false;

	// Retrieve rotation data.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_ROTATION, AttribInfoRotations, Rotations))
		bHasRotation = true;

	// Retrieve normal data.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals))
		bHasNormals = true;

	// Retrieve scale data.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_SCALE, AttribInfoScales, Scales))
		bHasScale = true;

	// Retrieve mesh socket names.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, AttribInfoNames, Names))
		bHasNames = true;
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME_OLD, AttribInfoNames, Names))
		bHasNames = true;

	// Retrieve mesh socket actor.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR, AttribInfoActors, Actors))
		bHasActors = true;
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR_OLD, AttribInfoActors, Actors))
		bHasActors = true;

	// Retrieve mesh socket tags.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG, AttribInfoTags, Tags))
		bHasTags = true;
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG_OLD, AttribInfoTags, Tags))
		bHasTags = true;

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
	const bool& CleanImportSockets)
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
	const HAPI_NodeId& NodeId, 
	const HAPI_PartId& PartId,
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
	const HAPI_NodeId& NodeId,
	const HAPI_PartId& PartId, 
	const TArray<FName>& Tags )
{
	if (Tags.Num() <= 0)
		return true;

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


bool
FHoudiniEngineUtils::SanitizeHAPIVariableName(FString& String)
{
	// Only keep alphanumeric characters, underscores
	// Also, if the first character is a digit, append an underscore at the beginning
	TArray<TCHAR>& StrArray = String.GetCharArray();
	if (StrArray.Num() <= 0)
		return false;

	for (auto& CurChar : StrArray)
	{
		const bool bIsValid = (CurChar >= TEXT('A') && CurChar <= TEXT('Z'))
			|| (CurChar >= TEXT('a') && CurChar <= TEXT('z'))
			|| (CurChar >= TEXT('0') && CurChar <= TEXT('9'))
			|| (CurChar == TEXT('_')) || (CurChar == TEXT('\0'));

		if(bIsValid)
			continue;

		CurChar = TEXT('_');
	}

	if (StrArray.Num() > 0)
	{
		TCHAR FirstChar = StrArray[0];
		if (FirstChar >= TEXT('0') && FirstChar <= TEXT('9'))
			StrArray.Insert(TEXT('_'), 0);
	}

	return true;
}

bool
FHoudiniEngineUtils::GetUnrealTagAttributes(
	const HAPI_NodeId& GeoId, const HAPI_PartId& PartId, TArray<FName>& OutTags)
{
	FString TagAttribBase = TEXT("unreal_tag_");
	bool bAttributeFound = true;
	int32 TagIdx = 0;
	while (bAttributeFound)
	{
		FString CurrentTagAttr = TagAttribBase + FString::FromInt(TagIdx++);
		bAttributeFound = HapiCheckAttributeExists(GeoId, PartId, TCHAR_TO_UTF8(*CurrentTagAttr), HAPI_ATTROWNER_PRIM);
		if (!bAttributeFound)
			break;

		// found the unreal_tag_X attribute, get its value and add it to the array
		FString TagValue = FString();

		// Create an AttributeInfo
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		TArray<FString> StringData;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId, TCHAR_TO_UTF8(*CurrentTagAttr),
			AttributeInfo, StringData, 1, HAPI_ATTROWNER_PRIM, 0, 1))
		{
			if (StringData.Num() > 0)
				TagValue = StringData[0];
		}

		FName NameTag = *TagValue;
		OutTags.Add(NameTag);
	}

	return true;
}


int32
FHoudiniEngineUtils::GetGenericAttributeList(
	const HAPI_NodeId& InGeoNodeId,
	const HAPI_PartId& InPartId,
	const FString& InGenericAttributePrefix,
	TArray<FHoudiniGenericAttribute>& OutFoundAttributes,
	const HAPI_AttributeOwner& AttributeOwner,
	const int32& InAttribIndex)
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
			TCHAR_TO_UTF8(*AttribName), AttributeOwner, &AttribInfo))
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
				TCHAR_TO_UTF8(*AttribName), &AttribInfo, 0,
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
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
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
					TCHAR_TO_UTF8(*AttribName), &AttribInfo,
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
					TCHAR_TO_UTF8(*AttribName), &AttribInfo,
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
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
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
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
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
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
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
FHoudiniEngineUtils::GetGenericPropertiesAttributes(const HAPI_NodeId& InGeoNodeId, const HAPI_PartId& InPartId,
	const bool InbFindDetailAttributes, const int32& InFirstValidPrimIndex, const int32& InFirstValidVertexIndex, const int32& InFirstValidPointIndex,
	TArray<FHoudiniGenericAttribute>& OutPropertyAttributes)
{
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
FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(UObject* InObject,
	const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes,
	const bool bInDeferPostEditChangePropertyCalls,
	const FHoudiniGenericAttribute::FFindPropertyFunctionType& InProcessFunction)
{
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
		constexpr int32 AtIndex = 0;
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
	const HAPI_NodeId& InGeoNodeId,
	const HAPI_PartId& InPartId,
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
			if (HAPI_RESULT_SUCCESS != FHoudiniEngineUtils::HapiSetAttributeStringData(
				InPropertyAttribute.StringValues,
				InGeoNodeId,
				InPartId,
				InPropertyAttribute.AttributeName,
				AttributeInfo))
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

void
FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
	UPackage * Package, UObject * Object, const FString& Key, const FString& Value)
{
	if (!IsValid(Package))
		return;

	UMetaData * MetaData = Package->GetMetaData();
	if (IsValid(MetaData))
		MetaData->SetValue(Object, *Key, *Value);
}


bool
FHoudiniEngineUtils::AddLevelPathAttribute(
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	ULevel* InLevel,
	const int32& InCount)
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
		LevelPath.LeftInline(DotIndex, false);

	// Get name of attribute used for level path
	std::string MarshallingAttributeLevelPath = HAPI_UNREAL_ATTRIB_LEVEL_PATH;

	// Marshall in level path.
	HAPI_AttributeInfo AttributeInfoLevelPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLevelPath);
	AttributeInfoLevelPath.count = InCount;
	AttributeInfoLevelPath.tupleSize = 1;
	AttributeInfoLevelPath.exists = true;
	AttributeInfoLevelPath.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoLevelPath.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoLevelPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		MarshallingAttributeLevelPath.c_str(), &AttributeInfoLevelPath);

	if (HAPI_RESULT_SUCCESS == Result)
	{
		// Convert the FString to a cont char * array
		std::string LevelPathCStr = TCHAR_TO_ANSI(*LevelPath);
		const char* LevelPathCStrRaw = LevelPathCStr.c_str();
		TArray<const char*> PrimitiveAttrs;
		PrimitiveAttrs.AddUninitialized(InCount);
		for (int32 Idx = 0; Idx < InCount; ++Idx)
		{
			PrimitiveAttrs[Idx] = LevelPathCStrRaw;
		}

		// Set the attribute's string data
		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId,
			MarshallingAttributeLevelPath.c_str(), &AttributeInfoLevelPath,
			PrimitiveAttrs.GetData(), 0, AttributeInfoLevelPath.count);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_level_path attribute for mesh: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());
	}

	return true;
}


bool
FHoudiniEngineUtils::AddActorPathAttribute(
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	AActor* InActor,
	const int32& InCount)
{
	if (InNodeId < 0 || InCount <= 0)
		return false;

	if (!IsValid(InActor))
		return false;

	// Extract the actor path
	FString ActorPath = InActor->GetPathName();

	// Get name of attribute used for Actor path
	std::string MarshallingAttributeActorPath = HAPI_UNREAL_ATTRIB_ACTOR_PATH;

	// Marshall in Actor path.
	HAPI_AttributeInfo AttributeInfoActorPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoActorPath);
	AttributeInfoActorPath.count = InCount;
	AttributeInfoActorPath.tupleSize = 1;
	AttributeInfoActorPath.exists = true;
	AttributeInfoActorPath.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoActorPath.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoActorPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		MarshallingAttributeActorPath.c_str(), &AttributeInfoActorPath);

	if (HAPI_RESULT_SUCCESS == Result)
	{
		// Convert the FString to a cont char * array
		std::string ActorPathCStr = TCHAR_TO_ANSI(*ActorPath);
		const char* ActorPathCStrRaw = ActorPathCStr.c_str();
		TArray<const char*> PrimitiveAttrs;
		PrimitiveAttrs.AddUninitialized(InCount);
		for (int32 Idx = 0; Idx < InCount; ++Idx)
		{
			PrimitiveAttrs[Idx] = ActorPathCStrRaw;
		}

		// Set the attribute's string data
		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId,
			MarshallingAttributeActorPath.c_str(), &AttributeInfoActorPath,
			PrimitiveAttrs.GetData(), 0, AttributeInfoActorPath.count);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_actor_path attribute for mesh: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());
	}

	return true;
}


bool
FHoudiniEngineUtils::ContainsInvalidLightmapFaces(const FRawMesh & RawMesh, int32 LightmapSourceIdx)
{
	const TArray< FVector2f > & LightmapUVs = RawMesh.WedgeTexCoords[LightmapSourceIdx];
	const TArray< uint32 > & Indices = RawMesh.WedgeIndices;

	if (LightmapUVs.Num() != Indices.Num())
	{
		// This is invalid raw mesh; by design we consider that it contains invalid lightmap faces.
		return true;
	}

	for (int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
	{
		const FVector2f& uv0 = LightmapUVs[Idx + 0];
		const FVector2f& uv1 = LightmapUVs[Idx + 1];
		const FVector2f& uv2 = LightmapUVs[Idx + 2];

		if (uv0 == uv1 && uv1 == uv2)
		{
			// Detect invalid lightmap face, can stop.
			return true;
		}
	}

	// Otherwise there are no invalid lightmap faces.
	return false;
}

void
FHoudiniEngineUtils::CreateSlateNotification(
	const FString& NotificationString, const float& NotificationExpire, const float& NotificationFadeOut )
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
	FString EnginePluginDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(EnginePluginDir))
		return EnginePluginDir;

	FString ProjectPluginDir = FPaths::ProjectPluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(ProjectPluginDir))
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
	const HAPI_NodeId& InParentNodeId,
	const FString& InOperatorName,
	const FString& InNodeLabel,
	const HAPI_Bool& bInCookOnCreation,
	HAPI_NodeId* OutNewNodeId)
{
	// Call HAPI::CreateNode
	HAPI_Result Result = FHoudiniApi::CreateNode(
		FHoudiniEngine::Get().GetSession(),
		InParentNodeId, TCHAR_TO_UTF8(*InOperatorName), TCHAR_TO_UTF8(*InNodeLabel), bInCookOnCreation, OutNewNodeId);

	// Return now if CreateNode fialed
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
FHoudiniEngineUtils::HapiGetCookCount(const HAPI_NodeId& InNodeId)
{
	int32 CookCount = -1;

	FHoudiniApi::GetTotalCookCount(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_ANY, true, &CookCount);

	/*
	// TODO:
	// Use HAPI_GetCookingTotalCount() when available
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);

	int32 CookCount = -1;
	HAPI_Result Result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &NodeInfo);
	
	if (Result != HAPI_RESULT_FAILURE)
	{
		if (NodeInfo.type != HAPI_NODETYPE_OBJ)
		{
			// For SOP assets, get the cook count straight from the Asset Node
			CookCount = NodeInfo.totalCookCount;
		}
		else
		{
			// For OBJ nodes, get the cook count from the display geos
			// Retrieve information about each object contained within our asset.
			TArray< HAPI_ObjectInfo > ObjectInfos;
			if (!FHoudiniEngineUtils::HapiGetObjectInfos(InNodeId, ObjectInfos))
				return false;

			for (auto CurrentHapiObjectInfo : ObjectInfos)
			{
				// Get the Display Geo's info				
				HAPI_GeoInfo DisplayHapiGeoInfo;
				FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetDisplayGeoInfo(
					FHoudiniEngine::Get().GetSession(), CurrentHapiObjectInfo.nodeId, &DisplayHapiGeoInfo))
				{
					continue;
				}

				HAPI_NodeInfo DisplayNodeInfo;
				FHoudiniApi::NodeInfo_Init(&DisplayNodeInfo);
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), DisplayHapiGeoInfo.nodeId, &DisplayNodeInfo))
				{
					continue;
				}

				CookCount += DisplayNodeInfo.totalCookCount;
			}
		}
	}
	*/

	return CookCount;
}

bool
FHoudiniEngineUtils::GetLevelPathAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	TArray<FString>& OutLevelPaths,
	HAPI_AttributeOwner InAttributeOwner,
	const int32& InStartIndex,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_level_path
	// ---------------------------------------------
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_LEVEL_PATH, 
		AttributeInfo, OutLevelPaths, 1, InAttributeOwner, InStartIndex, InCount))
	{
		if (OutLevelPaths.Num() > 0)
			return true;
	}

	OutLevelPaths.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetLevelPathAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	FString& OutLevelPath,
	const int32& InPointIndex,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId, 
	TArray<FString>& OutOutputNames,
	const int32& InStartIndex,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_output_name
	// ---------------------------------------------
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, 
		AttributeInfo, OutOutputNames, 1, HAPI_ATTROWNER_INVALID, InStartIndex, InCount))
	{
		if (OutOutputNames.Num() > 0)
			return true;
	}

	OutOutputNames.Empty();
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1,
		AttributeInfo, OutOutputNames, 1, HAPI_ATTROWNER_INVALID, InStartIndex, InCount))
	{
		if (OutOutputNames.Num() > 0)
			return true;
	}

	OutOutputNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetOutputNameAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	FString& OutOutputName,
	const int32& InPointIndex,
	const int32& InPrimIndex)
{
	constexpr int32 Count = 1;
	TArray<FString> StringData;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	// HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2
	if (InPointIndex >= 0)
	{
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, 
			AttributeInfo, StringData, 1, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutOutputName = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, 
			AttributeInfo, StringData, 1, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutOutputName = StringData[0];
				return true;
			}
		}
	}

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, 
		AttributeInfo, StringData, 1, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutOutputName = StringData[0];
			return true;
		}
	}
	
	// HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1
	if (InPointIndex >= 0)
	{
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1, 
			AttributeInfo, StringData, 1, HAPI_ATTROWNER_POINT, InPointIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutOutputName = StringData[0];
				return true;
			}
		}
	}

	if (InPrimIndex >= 0)
	{
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1, 
			AttributeInfo, StringData, 1, HAPI_ATTROWNER_PRIM, InPrimIndex, Count))
		{
			if (StringData.Num() > 0)
			{
				OutOutputName = StringData[0];
				return true;
			}
		}
	}

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1, 
		AttributeInfo, StringData, 1, HAPI_ATTROWNER_DETAIL, 0, Count))
	{
		if (StringData.Num() > 0)
		{
			OutOutputName = StringData[0];
			return true;
		}
	}

	OutOutputName.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeNameAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId, 
	TArray<FString>& OutBakeNames,
	const HAPI_AttributeOwner& InAttribOwner,
	const int32& InStartIndex,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_name
	// ---------------------------------------------
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_NAME, 
		AttributeInfo, OutBakeNames, 1, InAttribOwner, InStartIndex, InCount))
	{
		if (OutBakeNames.Num() > 0)
			return true;
	}

	OutBakeNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeNameAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId, 
	FString& OutBakeName,
	const int32& InPointIndex,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	TArray<int32>& OutTileValues,
	const HAPI_AttributeOwner& InAttribOwner,
	const int32& InStart,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: tile
	// ---------------------------------------------
	HAPI_AttributeInfo AttribInfoTile;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE,
		AttribInfoTile,	OutTileValues, 0, InAttribOwner, InStart, InCount))
	{
		if (OutTileValues.Num() > 0)
			return true;
	}

	OutTileValues.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetTileAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	int32& OutTileValue,
	const int32& InPointIndex,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	FString& EditLayerName,
	const HAPI_AttributeOwner& InAttribOwner)
{
	// ---------------------------------------------
	// Attribute: tile
	// ---------------------------------------------
	HAPI_AttributeInfo AttribInfo;
	FHoudiniApi::AttributeInfo_Init(&AttribInfo);

	TArray<FString> StrData;
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME,
		AttribInfo,
		StrData,
		0,
		InAttribOwner))
	{
		if (StrData.Num() > 0)
		{
			EditLayerName = StrData[0];
			return true;
		}
	}

	EditLayerName = FString();
	return false;
}

bool FHoudiniEngineUtils::HasEditLayerName(const HAPI_NodeId& InGeoId, const HAPI_PartId& InPartId,
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
	const HAPI_NodeId& InNodeId,
	const HAPI_AttributeOwner& InAttributeOwner,
	TArray<FString>& OutTempFolder,
	const HAPI_PartId& InPartId,
	const int32& InStart,
	const int32& InCount)
{
	OutTempFolder.Empty();

	HAPI_AttributeInfo TempFolderAttribInfo;
	FHoudiniApi::AttributeInfo_Init(&TempFolderAttribInfo);
	if (HapiGetAttributeDataAsString(
		InNodeId, InPartId, HAPI_UNREAL_ATTRIB_TEMP_FOLDER,
		TempFolderAttribInfo, OutTempFolder, 1, InAttributeOwner,
		InStart, InCount))
	{
		if (OutTempFolder.Num() > 0)
			return true;
	}

	OutTempFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetTempFolderAttribute(
	const HAPI_NodeId& InGeoId,
	FString& OutTempFolder,
	const HAPI_PartId& InPartId,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_AttributeOwner& InAttributeOwner,
	TArray<FString>& OutBakeFolder,
	const HAPI_PartId& InPartId,
	const int32& InStart,
	const int32& InCount)
{
	OutBakeFolder.Empty();
	
	HAPI_AttributeInfo BakeFolderAttribInfo;
	FHoudiniApi::AttributeInfo_Init(&BakeFolderAttribInfo);
	if (HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_FOLDER,
		BakeFolderAttribInfo, OutBakeFolder, 1, InAttributeOwner,
		InStart, InCount))
	{
		if (OutBakeFolder.Num() > 0)
			return true;
	}

	OutBakeFolder.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeFolderAttribute(
	const HAPI_NodeId& InGeoId,
	TArray<FString>& OutBakeFolder,
	const HAPI_PartId& InPartId,
	const int32& InStart,
	const int32& InCount)
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
	const HAPI_NodeId& InGeoId,
	FString& OutBakeFolder,
	const HAPI_PartId& InPartId,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	TArray<FString>& OutBakeActorNames,
	const HAPI_AttributeOwner& InAttributeOwner,
	const int32& InStart,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_actor
	// ---------------------------------------------
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_ACTOR,
		AttributeInfo, OutBakeActorNames, 1, InAttributeOwner, InStart, InCount))
	{
		if (OutBakeActorNames.Num() > 0)
			return true;
	}

	OutBakeActorNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeActorAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	FString& OutBakeActorName,
	const int32& InPointIndex,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	TArray<FString>& OutBakeActorClassNames,
	const HAPI_AttributeOwner& InAttributeOwner,
	const int32& InStart,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_actor
	// ---------------------------------------------
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS,
		AttributeInfo, OutBakeActorClassNames, 1, InAttributeOwner, InStart, InCount))
	{
		if (OutBakeActorClassNames.Num() > 0)
			return true;
	}

	OutBakeActorClassNames.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeActorClassAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	FString& OutBakeActorClassName,
	const int32& InPointIndex,
	const int32& InPrimIndex)
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
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	TArray<FString>& OutBakeOutlinerFolders,
	const HAPI_AttributeOwner& InAttributeOwner,
	const int32& InStart,
	const int32& InCount)
{
	// ---------------------------------------------
	// Attribute: unreal_bake_outliner_folder
	// ---------------------------------------------
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InGeoId, InPartId, HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, 
		AttributeInfo, OutBakeOutlinerFolders, 1, InAttributeOwner, InStart, InCount))
	{
		if (OutBakeOutlinerFolders.Num() > 0)
			return true;
	}

	OutBakeOutlinerFolders.Empty();
	return false;
}

bool
FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	FString& OutBakeOutlinerFolder,
	const int32& InPointIndex,
	const int32& InPrimIndex)
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

bool
FHoudiniEngineUtils::HapiCookNode(const HAPI_NodeId& InNodeId, HAPI_CookOptions* InCookOptions, const bool& bWaitForCompletion)
{
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

#undef LOCTEXT_NAMESPACE
