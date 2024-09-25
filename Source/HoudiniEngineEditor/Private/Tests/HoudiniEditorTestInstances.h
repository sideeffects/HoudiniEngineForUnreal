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
#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"

struct FHoudiniInstanceAutomationTest : public FHoudiniAutomationTest
{
    FHoudiniInstanceAutomationTest(const FString& InName, const bool bInComplexTask)
        : FHoudiniAutomationTest(InName, bInComplexTask)
    {
    }

	void CheckPositions(const TArray<FVector> & Positions);

    const static inline FString BakingHDA = TEXT("/Game/TestHDAs/Instances/Test_Instances");
    const static inline FString PDGHDA = TEXT("/Game/TestHDAs/Instances/Test_PDGInstances");
    const static inline FString InstancedMesh = TEXT("/Script/Engine.StaticMesh'/Game/TestObjects/SM_Cube.SM_Cube'");
    const static inline FString InstancedActor = TEXT("/Script/Engine.Blueprint'/Game/TestObjects/BP_Cube.BP_Cube'");
    const static inline FString PackedInstancesHDA = TEXT("/Game/TestHDAs/Instances/Test_PackedInstances");

    static TArray<FFoliageInstance> GetAllFoliageInstances(UWorld* InWorld, UFoliageType* FoliageType);
    static TArray<UFoliageType*> GetAllFoliageTypes(UWorld* InWorld);

};

#endif

