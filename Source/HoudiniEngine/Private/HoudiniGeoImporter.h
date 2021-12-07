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

#include "HAPI/HAPI_Common.h"

#include "HoudiniGeoImporter.generated.h"

class UHoudiniOutput;

struct FHoudiniPackageParams;
struct FHoudiniStaticMeshGenerationProperties;
struct FMeshBuildSettings;

UCLASS()
class HOUDINIENGINE_API UHoudiniGeoImporter : public UObject
{
public:

	GENERATED_UCLASS_BODY()

	public:
		//UHoudiniGeoImporter();
		//~UHoudiniGeoImporter();
		
		void SetBakeRootFolder(const FString& InFolder) { BakeRootFolder = InFolder; };
		void SetOutputFilename(const FString& InOutFilename) { OutputFilename = InOutFilename; };

		TArray<UObject*>& GetOutputObjects() { return OutputObjects; };

		// BEGIN: Static API
		// Open a BGEO file: create a file node in HAPI and cook it
		static bool OpenBGEOFile(const FString& InBGEOFile, HAPI_NodeId& OutNodeId, bool bInUseWorldComposition=false);
		// Cook the file node specified by the valid NodeId.
		static bool CookFileNode(const HAPI_NodeId& InNodeId);
		// Extract the outputs for a given node ID
		static bool BuildAllOutputsForNode(const HAPI_NodeId& InNodeId, UObject* InOuter, TArray<UHoudiniOutput*>& InOldOutputs, TArray<UHoudiniOutput*>& OutNewOutputs, bool bInAddOutputsToRootSet=false);
		// Delete the HAPI node and remove InOutputs from the root set.
		static bool CloseBGEOFile(const HAPI_NodeId& InNodeId);
		// END: Static API

		// Import the BGEO file
		bool ImportBGEOFile(
			const FString& InBGEOFile, UObject* InParent, const FHoudiniPackageParams* InPackageParams=nullptr,
			const FHoudiniStaticMeshGenerationProperties* InStaticMeshGenerationProperties=nullptr,
			const FMeshBuildSettings* InMeshBuildSettings=nullptr);

		// 1. Start a HE session if needed
		static bool AutoStartHoudiniEngineSessionIfNeeded();
		// 2. Update our file members fromn the input file path
		bool SetFilePath(const FString& InFilePath);
		// 3. Creates a new file node and loads the bgeo file in HAPI
		bool LoadBGEOFileInHAPI(HAPI_NodeId& NodeId);
		// 4. Extract the outputs for a given node ID
		bool BuildOutputsForNode(const HAPI_NodeId& InNodeId, TArray<UHoudiniOutput*>& InOldOutputs, TArray<UHoudiniOutput*>& OutNewOutputs);
		// 5. Creates the static meshes object found in the output
		bool CreateStaticMeshes(
			TArray<UHoudiniOutput*>& InOutputs, UObject* InParent, FHoudiniPackageParams InPackageParams,
			const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties, const FMeshBuildSettings& InMeshBuildSettings);
		// 6. Create the output curves
		bool CreateCurves(TArray<UHoudiniOutput*>& InOutputs, UObject* InParent, FHoudiniPackageParams InPackageParams);
		// 7. Create the output landscapes
		bool CreateLandscapes(TArray<UHoudiniOutput*>& InOutputs, UObject* InParent, FHoudiniPackageParams InPackageParams);
		// 8. Create the output instancers
		bool CreateInstancers(TArray<UHoudiniOutput*>& InOutputs, UObject* InParent, FHoudiniPackageParams InPackageParams);
		// 9. Clean up the created node
		static bool DeleteCreatedNode(const HAPI_NodeId& InNodeId);

		static bool CreateInstancerOutputPartData(
			TArray<UHoudiniOutput*>& InOutputs,
			TMap<struct FHoudiniOutputObjectIdentifier, struct FHoudiniInstancedOutputPartData>& OutInstancedOutputPartData);

	private:

		//
		// Input file
		//
		// Path how the file we're currently loading
		FString SourceFilePath;
		// Absolute Path to the file
		FString AbsoluteFilePath;
		FString AbsoluteFileDirectory;
		// File Name / Extension
		FString FileName;
		FString FileExtension;
		
		
		//
		// Output file
		//
		// Output filename, if empty, will be set to the input filename
		FString OutputFilename;
		// Root Folder for storing the created files
		FString BakeRootFolder;

		//
		// Output Objects
		// 
		TArray<UObject*> OutputObjects;

		//TArray<UObject*> OutputStaticMeshes;
		//TArray<UObject*> OutputLandscapes;
		//TArray<UObject*> OutputInstancers;

};