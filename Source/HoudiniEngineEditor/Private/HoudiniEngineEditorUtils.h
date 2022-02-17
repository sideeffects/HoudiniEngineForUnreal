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

class FString;
class UObject;
class UHoudiniAsset;
class AActor;
class ULevel;
class USceneComponent;

enum class EHoudiniCurveType : int8;
enum class EHoudiniCurveMethod : int8;
enum class EHoudiniCurveBreakpointParameterization : int8;
enum class EHoudiniLandscapeOutputBakeType : uint8;
enum class EHoudiniToolType : uint8;
enum class EHoudiniToolSelectionType : uint8;

struct FHoudiniEngineEditorUtils
{
public:

	// Triggers an update the details panel
	//static void UpdateEditorProperties(UObject* ObjectToUpdate);

	// Helper function for accessing the current CB selection
	static int32 GetContentBrowserSelection(TArray< UObject* >& ContentBrowserSelection);

	// Helper function for accessing the current world selection
	static int32 GetWorldSelection(TArray< UObject* >& WorldSelection, bool bHoudiniAssetActorsOnly = false);

	static FString HoudiniCurveTypeToString(const EHoudiniCurveType& HoudiniCurveType);

	static FString HoudiniCurveMethodToString(const EHoudiniCurveMethod& HoudinCurveMethod);

	static FString HoudiniCurveBreakpointParameterizationToString(const EHoudiniCurveBreakpointParameterization& HoudiniCurveBreakpointParameterization);

	static FString HoudiniLandscapeOutputBakeTypeToString(const EHoudiniLandscapeOutputBakeType& LandscapeBakeType);

	// (for temporary, we need to figure out a way to access the output curve's info later)
	static FString HoudiniCurveMethodToUnrealCurveTypeString(const EHoudiniCurveType& HoudiniCurveType);

	static FTransform GetDefaulAssetSpawnTransform();

	static FTransform GetMeanWorldSelectionTransform();

	// Instantiate a HoudiniAsset at a given position. If InSpawnInLevelOverride is non-null, spawns in that level.
	// Otherwise if InSpawnInWorld, spawns in the current level of InSpawnInWorld. Lastly, if both are null, spawns
	// in the current level of the editor context world.
	static AActor* InstantiateHoudiniAssetAt(UHoudiniAsset* InHoudiniAsset, const FTransform& InTransform, UWorld* InSpawnInWorld=nullptr, ULevel* InSpawnInLevelOverride=nullptr);

	// Instantiate the given HDA, and handles the current CB/World selection
	static void InstantiateHoudiniAsset(UHoudiniAsset* InHoudiniAsset, const EHoudiniToolType& InType, const EHoudiniToolSelectionType& InSelectionType);

	// Helper function used to save all temporary packages when the level is saved
	static void SaveAllHoudiniTemporaryCookData(UWorld *InSaveWorld);

	// Deselect and reselect all selected actors to get rid of component not showing bug after create.
	static void ReselectSelectedActors();

	// Deselect and reselect InActor, but only if it is currently selected. Related to ReselectSelectedActors and
	// the component not showing bug after create.
	static void ReselectActorIfSelected(AActor* const InActor);

	// Deselect and reselect the owning actor of InComopnent, but only if the actor is currently selected. Related to
	// ReselectSelectedActors and the component not showing bug after create.
	static void ReselectComponentOwnerIfSelected(USceneComponent* const InComponent);

	// Gets the node name indent from the left with the specified number of spaces based on the path depth.
	static FString GetNodeNamePaddedByPathDepth(const FString& InNodeName, const FString& InNodePath, const uint8 Padding=4, const TCHAR PathSep='/');

	// Property change notifications
	// Call PostEditChangeChainProperty on InRootObject for the property at InPropertyPath relative to
	// InRootObject.
	static void NotifyPostEditChangeProperty(FName InPropertyPath, UObject* InRootObject);
};