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
#include "HoudiniEditorUnitTestUtils.h"
#include "Landscape.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include <HoudiniMeshTranslator.h>

enum EHoudiniMeshType
{
	None, Cube, Sphere, Torus
};

struct FHoudiniMeshCheck
{
	TArray<EHoudiniMeshType> LODMeshes;
    EHoudiniMeshType ComplexCollisionType = EHoudiniMeshType::None;
    bool bComponentIsVisible = true;
    int NumBoxCollisions = 0;
    int NumSphereCollisions = 0;
    int NumConvexCollisions = 0;
    int NumSphylCollisions = 0;
};

struct FHoudiniMeshCheckErrors
{
    FHoudiniMeshCheckErrors & operator+=(const FString & Error)
    {
	    Errors.Add(Error);
        return *this;
    }

    FHoudiniMeshCheckErrors & operator+=(const FHoudiniMeshCheckErrors& Error)
    {
	    this->Errors.Append(Error.Errors);
        return *this;
    }

	TArray<FString>  Errors;
};

struct FHoudiniTestSettings
{
	TArray<FString> CubeGroups;
    TArray<FString> SphereGroups;
    bool bPack = false;
};

struct FHoudiniMeshAutomationTest : public FHoudiniAutomationTest
{
    FHoudiniMeshAutomationTest(const FString& InName, const bool bInComplexTask)
        : FHoudiniAutomationTest(InName, bInComplexTask)
    {
    }

    void ExecuteMeshTest(TSharedPtr<FHoudiniTestContext> Context, const FHoudiniTestSettings & Settings, const FHoudiniMeshCheck& TestErrors);

    static TArray<UStaticMeshComponent*> GetStaticMeshes(TSharedPtr<FHoudiniTestContext> Context);
    static TSharedPtr<FHoudiniTestContext> LoadHDA(FAutomationTestBase* Test);

    void  CheckMesh(UStaticMeshComponent* StaticMeshComponent, const FHoudiniMeshCheck& Check);

    static FString GetCollisionTypeName(EHoudiniCollisionType Type);

    void CheckMeshType(const FStaticMeshLODResources& Resource, EHoudiniMeshType MeshType);

};


#endif

