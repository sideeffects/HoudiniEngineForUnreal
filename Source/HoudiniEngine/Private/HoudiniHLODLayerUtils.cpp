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
#include "HoudiniEngineAttributes.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

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
FHoudiniHLODLayerUtils::GetHLODLayers(HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_AttributeOwner Owner, int Index)
{
	TArray<FString> HLODNames;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_HLOD_LAYER);
	Accessor.GetAttributeData(Owner, HLODNames, Index, 1);
	if(HLODNames.IsEmpty())
		return {};

	FHoudiniHLODLayer Layer;
	Layer.Name = HLODNames[0];

	TArray<FHoudiniHLODLayer> Results;
	Results.Add(Layer);
	return Results;

}

TArray<FHoudiniHLODLayer>
FHoudiniHLODLayerUtils::GetHLODLayers(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	TArray<FHoudiniHLODLayer> Results;

	// Get a list of all groups this part MAY be a member of.
	TArray<FString> HLODNames;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_HLOD_LAYER);
	bool bResult = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, HLODNames);

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

	const FHoudiniHLODLayer & Layer = Layers[0];

	AddActorToHLOD(Actor, Layer.Name);
	
	if(ALandscape* Landscape = Cast<ALandscape>(Actor))
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Landscape->GetLandscapeInfo()->GetSortedStreamingProxies();
#elif ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Landscape->GetLandscapeInfo()->StreamingProxies;
#else
		TArray<TObjectPtr<ALandscapeStreamingProxy>> Proxies = Proxy->GetLandscapeInfo()->Proxies;
#endif
		for(TWeakObjectPtr<ALandscapeStreamingProxy> Child : Proxies)
		{
			ALandscapeStreamingProxy* LandscapeProxy = Child.Get();
			AddActorToHLOD(Cast<AActor>(LandscapeProxy), Layer.Name);
		}
	}

}

void FHoudiniHLODLayerUtils::SetVexCode(HAPI_NodeId VexNodeId, AActor * Actor)
{
	UHLODLayer* Layer = Actor->GetHLODLayer();

	// If there is no layer, just leave the empty node. Its not an error.
	if (!Layer)
		return;

	FString LayerName = Layer->GetPathName();

	FString VexCode = FString::Format(TEXT("s@{0} = \"{1}\";\n"), { TEXT(HAPI_UNREAL_ATTRIB_HLOD_LAYER), LayerName });

	// Set the snippet parameter to the code.
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(VexNodeId, "snippet", ParmInfo);
	if (ParmId != -1)
	{
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), VexNodeId, H_TCHAR_TO_UTF8(*VexCode), ParmId, 0);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"), *FHoudiniEngineUtils::GetErrorDescription());
	}
}

HAPI_NodeId FHoudiniHLODLayerUtils::AddHLODAttributes(AActor* Actor, HAPI_NodeId ParentNodeId, HAPI_NodeId InputNodeId)
{
	HAPI_NodeId VexNode = -1;

	// Create a group node.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),
		ParentNodeId,
		"attribwrangle",
		"hlod_layers",
		false,
		&VexNode),
		-1);

	// Hook the new node up to the input node.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(), VexNode, 0, InputNodeId, 0), -1);

	// Set the wrangle's class to prims
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), VexNode, "class", 0, 1), -1);

	// Set the Vex code which will generate the attributes.
	SetVexCode(VexNode, Actor);
	
	return VexNode;
}
