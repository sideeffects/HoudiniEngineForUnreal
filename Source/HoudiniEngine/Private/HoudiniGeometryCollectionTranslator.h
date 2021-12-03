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
#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "HoudiniAssetComponent.h"



#include "CoreMinimal.h"

class AGeometryCollectionActor;
class UGeometryCollection;
class UGeometryCollectionComponent;
struct FHoudiniGenericAttribute;

struct HOUDINIENGINE_API FHoudiniGeometryCollectionTranslator
{
	// Helper to contain geometry collection data
	struct FHoudiniGeometryCollectionPiece
	{
		// Set initially
		FHoudiniOutputObjectIdentifier* InstancerOutputIdentifier = nullptr;
		FHoudiniOutputObject* InstancerOutput = nullptr;
		int32 InstancedPartId = -1;
		int32 FractureIndex = -1;
		int32 ClusterIndex = -1;
		FString GeometryCollectionName = TEXT("");

		// Set On second pass
		// Specifies the geometry index in the geometry collection of this piece
		int32 GeometryIndex = -1;
	};

	// Helper struct to contain data regarding a single geometry collection.
	struct FHoudiniGeometryCollectionData
	{
		TArray<FHoudiniGeometryCollectionPiece> GeometryCollectionPieces;
		FHoudiniOutputObjectIdentifier Identifier;
		FHoudiniPackageParams PackParams;

		FHoudiniGeometryCollectionData(const FHoudiniOutputObjectIdentifier& InIdentifier, const FHoudiniPackageParams& InPackParams)
		{
			Identifier = InIdentifier;
			PackParams = InPackParams;
		}
	};
	
	public:
		static void SetupGeometryCollectionComponentFromOutputs(TArray<UHoudiniOutput*>& InAllOutputs,
			UObject* InOuterComponent, const FHoudiniPackageParams& InPackageParams, UWorld * InWorld);

		static bool GetFracturePieceAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, int& OutInt);
		static bool GetClusterPieceAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, int& OutInt);

		static bool GetGeometryCollectionNameAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, FString& OutName);

		static bool IsGeometryCollectionInstancer(const UHoudiniOutput* HoudiniOutput);
		static bool IsGeometryCollectionMesh(const UHoudiniOutput* HoudiniOutput);
		static bool IsGeometryCollectionInstancerPart(const HAPI_NodeId InstancerGeoId, const HAPI_PartId InstancerPartId);
	 	
		static AGeometryCollectionActor * CreateNewGeometryActor(UWorld * InWorld, const FString & InActorName, const FTransform InTransform);

		static bool GetGeometryCollectionNames(TArray<UHoudiniOutput*>& InAllOutputs, TSet<FString>& Names);
	

	private:

		static UGeometryCollectionComponent* CreateGeometryCollectionComponent(UObject *InOuterComponent);
	
		static bool RemoveAndDestroyComponent(UObject* InComponent);
	

		// Extracts all Geometry collection data from the outputs
		// Map is representing gc_name -> gc_data
		static bool GetGeometryCollectionData(const TArray<UHoudiniOutput*>& InAllOutputs, const FHoudiniPackageParams& InPackageParams, TMap<FString, FHoudiniGeometryCollectionData>& OutGeometryCollectionData);
	
		static void ApplyGeometryCollectionAttributes(UGeometryCollection* GeometryCollection, FHoudiniGeometryCollectionPiece FirstPiece);

		// Copied from GeometryCollectionConversion.h because they are from the editor module.
		static void AppendStaticMesh(
			const UStaticMesh* StaticMesh,
			const TArray<UMaterialInterface*>& Materials,
			const FTransform& StaticMeshTransform,
			UGeometryCollection* GeometryCollectionObject,
			const bool& ReindexMaterials = true,
			const int32& Level = 0);

		// Copied from FractureToolEmbed.h
		static void AddSingleRootNodeIfRequired(UGeometryCollection* GeometryCollectionObject);
	
};

