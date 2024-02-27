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

#include "HoudiniBakeLevelInstanceUtils.h"
#include "Editor.h"
#include "HoudiniEngineUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "HoudiniOutput.h"
#include "HoudiniEngineBakeUtils.h"

ILevelInstanceInterface * FHoudiniBakeLevelInstanceUtils::CreateLevelInstance(
	const FHoudiniLevelInstanceParams& Params, 
	TArray<AActor*> & Actors,
	const FString& BakeFolder,
	FHoudiniBakedObjectData& BakedObjectData)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    ULevelInstanceSubsystem* LevelInstanceSubsystem = 
		GEditor->GetEditorWorldContext().World()->GetSubsystem<ULevelInstanceSubsystem>();

	// Convert out input params to Unreal's structure.
    FNewLevelInstanceParams CreationParams;
	CreationParams.Type = ELevelInstanceCreationType::PackedLevelActor; //Params.Type;
	CreationParams.LevelPackageName = BakeFolder + TEXT("/") + Params.OutputName;
	CreationParams.PivotType = ELevelInstancePivotType::CenterMinZ;
	CreationParams.PivotActor = nullptr;
	CreationParams.bAlwaysShowDialog = false;
	CreationParams.TemplateWorld = nullptr;
	CreationParams.LevelInstanceClass = ALevelInstance::StaticClass();

	FString LongPackageName =  FPackageName::FilenameToLongPackageName(CreationParams.LevelPackageName);

	// Make sure the proposed package name is valid, if not Unreal will crash.
	FString PackageName;
	bool bValidPackage = FPackageName::TryConvertLongPackageNameToFilename(CreationParams.LevelPackageName, PackageName, FPackageName::GetMapPackageExtension());
	if (!bValidPackage)
	{
		HOUDINI_LOG_ERROR(TEXT("Cannot create Level Instance due to invalid packaged name %s"), *CreationParams.LevelPackageName);
		return nullptr;
	}

	ILevelInstanceInterface* Result = LevelInstanceSubsystem->CreateLevelInstanceFrom(Actors, CreationParams);
	
    return Result;
#else
	return nullptr;
#endif
}

uint32 GetTypeHash(const FHoudiniLevelInstanceParams& Params)
{
	return (GetTypeHash((int32)Params.Type) + 23 * GetTypeHash(Params.OutputName));
}

bool operator==(const FHoudiniLevelInstanceParams & p1, const FHoudiniLevelInstanceParams & p2)
{
	return p1.Type == p2.Type && p1.OutputName == p2.OutputName;
}

const UHoudiniOutput*
FHoudiniBakeLevelInstanceUtils::GetHoudiniObject(
	const FHoudiniOutputObjectIdentifier Id, 
	const TArray<UHoudiniOutput*>& CookedOutputs)
{
	for (int Index = 0; Index < CookedOutputs.Num(); Index++)
	{
		if (CookedOutputs[Index]->GetOutputObjects().Contains(Id))
			return CookedOutputs[Index];
	}
	return nullptr;
}

bool
FHoudiniBakeLevelInstanceUtils::CreateLevelInstances(
	UHoudiniAssetComponent * HAC, 
	const TArray<FHoudiniEngineBakedActor>& BakedActors,
	const FString& BakeFolder,
	FHoudiniBakedObjectData& BakedObjectData)
{
	TArray<UHoudiniOutput*> CookedOutputs;
	HAC->GetOutputs(CookedOutputs);

	//----------------------------------------------------------------------------------------------------------------------------------
	// Go through all baked actors and find any which have valid information about level instancing. Build a list of level
	// actors which share the same level instance information.
	//----------------------------------------------------------------------------------------------------------------------------------

	TMap< FHoudiniLevelInstanceParams, TArray<AActor*> > ActorsPerInstance;
	TMap<AActor*, int> ActorToBakedActor;

	for (int Index = 0; Index < BakedActors.Num(); Index++)
	{
		const FHoudiniEngineBakedActor & BakedActor = BakedActors[Index];

		const UHoudiniOutput* CookedOutput = GetHoudiniObject(BakedActor.OutputObjectIdentifier, CookedOutputs);
		HOUDINI_CHECK_RETURN(CookedOutput, false);

		const FHoudiniOutputObject* CookedObject = CookedOutput->GetOutputObjects().Find(BakedActor.OutputObjectIdentifier);

		AActor* Actor = BakedActor.Actor;
		if (!Actor || CookedObject->LevelInstanceParams.OutputName.IsEmpty())
			continue;

		const FHoudiniLevelInstanceParams LevelInstanceParams = CookedObject->LevelInstanceParams;

		auto& Array = ActorsPerInstance.FindOrAdd(LevelInstanceParams);
		Array.Add(Actor);
		ActorToBakedActor.Add(Actor, Index);
	}

	//----------------------------------------------------------------------------------------------------------------------------------
	// Create each level instance
	//----------------------------------------------------------------------------------------------------------------------------------

	for(auto & It : ActorsPerInstance)
	{
		CreateLevelInstance(It.Key, It.Value, BakeFolder, BakedObjectData);
	}
	return true;

}

