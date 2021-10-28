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

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "UnrealMeshTranslator.h"

class UFoliageType;
class UFoliageType_InstancedStaticMesh;
class UStaticMeshComponent;

struct HOUDINIENGINE_API FUnrealFoliageTypeTranslator : public FUnrealMeshTranslator
{
public:
	// HAPI : Marshaling, extract geometry and create input asset for it - return true on success
	static bool HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
		UFoliageType_InstancedStaticMesh* InFoliageType,
		HAPI_NodeId& InputObjectNodeId,
		const FString& InputNodeName,
		const bool& ExportAllLODs = false,
		const bool& ExportSockets = false,
		const bool& ExportColliders = false);

	// Create an input node that references the asset via InRef (unreal_instance).
	// Also calls CreateHoudiniFoliageTypeAttributes, to create the unreal_foliage attribute, as well as
	// unreal_uproperty_ attributes for the foliage type settings.
	static bool CreateInputNodeForReference(
		UFoliageType* InFoliageType,
		HAPI_NodeId& InInputNodeId,
		const FString& InRef,
		const FString& InInputNodeName,
		const FTransform& InTransform,
		const bool& bImportAsReferenceRotScaleEnabled);

protected:
	// Creates the unreal_foliage and unreal_uproperty_ attributes for the foliage type.
	static bool CreateHoudiniFoliageTypeAttributes(
		UFoliageType* InFoliageType, const int32& InNodeId, const int32& InPartId, HAPI_AttributeOwner InAttributeOwner);
};
