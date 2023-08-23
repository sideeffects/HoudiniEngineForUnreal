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

#include "HoudiniHLODLayerUtils.h"

#include "HoudiniEngine.h"
#include "WorldPartition/HLOD/HLODLayer.h"

void FHoudiniHLODLayerUtils::AddActorToHLOD(AActor* Actor, const FString& AssetRef)
{
#if WITH_EDITORONLY_DATA
	UHLODLayer* HLODLayer = Cast<UHLODLayer>(
		StaticLoadObject(
			UHLODLayer::StaticClass(),
			nullptr,
			*AssetRef,
			nullptr,
			LOAD_NoWarn,
			nullptr));

	Actor->SetHLODLayer(HLODLayer);
#endif
}

TArray<FHoudiniHLODLayer>
FHoudiniHLODLayerUtils::GetHLODLayers(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	TArray<FHoudiniHLODLayer> Results;

	// Get a list of all groups this part MAY be a member of.
	TArray<FString> HLODNames;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	bool bResult = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		NodeId, PartId, 
		HAPI_UNREAL_ATTRIB_HLOD_LAYER, 
		AttributeInfo, HLODNames);

	for (FString HLODLayerName : HLODNames)
	{
		FHoudiniHLODLayer Layer;

		Layer.Name = HLODLayerName;
		Results.Add(Layer);
	}

	return Results;
}

void FHoudiniHLODLayerUtils::ApplyHLODLayersToActor(const FHoudiniPackageParams& Params, AActor* Actor, const TArray<FHoudiniHLODLayer>& Layers)
{
	UWorld* World = Actor->GetWorld();

	if (Layers.Num() == 0)
		return;

	for (auto& Layer : Layers)
	{
		AddActorToHLOD(Actor, Layer.Name);
	}
}

bool FHoudiniHLODLayerUtils::AddHLODAttributes(AActor* Actor, HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	UHLODLayer * Layer = Actor->GetHLODLayer();
	if (!Layer)
		return false;

	FString LayerName = Layer->GetPathName();

	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), NodeId, 0, &Part);

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	AttributeInfo.count = Part.faceCount;
	AttributeInfo.tupleSize = 1;
	AttributeInfo.exists = true;
	AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), NodeId, PartId, HAPI_UNREAL_ATTRIB_HLOD_LAYER, &AttributeInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(LayerName,NodeId,PartId, HAPI_UNREAL_ATTRIB_HLOD_LAYER, AttributeInfo), false);

	return true;
}
