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

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "MessageEndpoint.h"

#include "HoudiniEngine.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniPDGImporterMessages.h"
#include "HoudiniGeoPartObject.h"

#include "HoudiniGeoImportCommandlet.generated.h"

class FSocket;

class UHoudiniGeoImporter;
class UHoudiniOutput;

struct FHoudiniPackageParams;

enum class EHoudiniGeoImportCommandletMode : uint8
{
	// Unspecified
	None,
	// Import of specified file
	SpecifiedFiles,
	// Directory watch mode
	Watch,
	// Listen mode (via PDGManager)
	Listen
};

struct FDiscoveredFileData
{
public:
	FDiscoveredFileData() : FileName(), bImportNextTick(false), ImportAttempts(0), bImported(false) {}

	FDiscoveredFileData(const FString& InFileName, bool bInImportNextTick=false) : FileName(InFileName), bImportNextTick(bInImportNextTick), ImportAttempts(0), bImported(false) {}

	FDiscoveredFileData(FString&& InFileName, bool bInImportNextTick=false) : FileName(InFileName), bImportNextTick(bInImportNextTick), ImportAttempts(0), bImported(false) {}
	
	// Full/absolute file path
	FString FileName;

	// Try to import this file on the next tick
	bool bImportNextTick;

	// Number of attempts at importing this file
	uint32 ImportAttempts;

	// The file has been imported successfully
	bool bImported;
};

UCLASS()
class HOUDINIENGINE_API UHoudiniGeoImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	UHoudiniGeoImportCommandlet();

	void PrintUsage() const;

	/**
	* Entry point for your commandlet
	*
	* @param Params the string containing the parameters for the commandlet
	*/
	virtual int32 Main(const FString& Params) override;

	void HandleImportBGEOMessage(
		const struct FHoudiniPDGImportBGEOMessage& InMessage, 
		const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	void HandleDirectoryChanged(const TArray<struct FFileChangeData>& InFileChangeDatas);

protected:

	void PopulatePackageParams(const FString& InBGEOFilename, FHoudiniPackageParams& OutPackageParams);

	bool StartHoudiniEngineSession();

	bool IsHoudiniEngineSessionRunning() { return FHoudiniEngine::Get().GetSession() != nullptr; };

	int32 MainLoop();

	int32 ImportBGEO(
		const FString& InFilename, 
		const FHoudiniPackageParams& InPackageParams, 
		TArray<UHoudiniOutput*>& OutOutputs,
		const FHoudiniStaticMeshGenerationProperties* InStaticMeshGenerationProperties=nullptr,
		const FMeshBuildSettings* InMeshBuildSettings=nullptr,
		TMap<FHoudiniOutputObjectIdentifier, TArray<FHoudiniGenericAttribute>>* OutGenericAttributes=nullptr,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* OutInstancedOutputPartData=nullptr);

	void TickDiscoveredFiles();

private:

	// Messaging end point for receiving messages from PDG manager
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> PDGEndpoint;

	// The messaging address of the manager
	FMessageAddress ManagerAddress;

	// Unique ID of the commandlet.
	FGuid Guid;

	// The proc handle of our owner (if in listen mode, quit when the owner stops running).
	FProcHandle OwnerProcHandle;

	// TODO: Map so that we can watch multiple directories?
	// Directory to watch
	FString DirectoryToWatch;
	// Handle if we are watching a directory for changes.
	FDelegateHandle DirectoryWatcherHandle;

	// Keep track of files discovered by the watcher, and their state
	TMap<FString, FDiscoveredFileData> DiscoveredFiles;

	// Mode in which commandlet is running
	EHoudiniGeoImportCommandletMode Mode;
	
	// Bake outputs via FHoudiniEngineBakeUtils
	bool bBakeOutputs;
};