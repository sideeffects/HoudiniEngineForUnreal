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

#include "UnrealFoliageTypeTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniInputTranslator.h"

#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"


bool
FUnrealFoliageTypeTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
	UFoliageType_InstancedStaticMesh* InFoliageType, 
	HAPI_NodeId& InputObjectNodeId,
	const FString& InputNodeName,
	const bool& ExportAllLODs,
	const bool& ExportSockets,
	const bool& ExportColliders)
{
	if (!IsValid(InFoliageType))
		return false;

	UStaticMesh* const InputSM = InFoliageType->GetStaticMesh();
	if (!IsValid(InputSM))
		return false;

	UStaticMeshComponent* const StaticMeshComponent = nullptr;
	bool bSuccess = HapiCreateInputNodeForStaticMesh(
		InputSM,
		InputObjectNodeId,
		InputNodeName,
		StaticMeshComponent,
		ExportAllLODs,
		ExportSockets,
		ExportColliders);

	if (bSuccess)
	{
		const int32 PartId = 0; 
		CreateHoudiniFoliageTypeAttributes(InFoliageType, InputObjectNodeId, PartId, HAPI_ATTROWNER_DETAIL);
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), InputObjectNodeId), false);
	}

	return bSuccess;
}

bool FUnrealFoliageTypeTranslator::CreateInputNodeForReference(
	UFoliageType* InFoliageType,
	HAPI_NodeId& InInputNodeId,
	const FString& InRef,
	const FString& InInputNodeName,
	const FTransform& InTransform,
	const bool& bImportAsReferenceRotScaleEnabled)
{
	bool bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(InInputNodeId, InRef, InInputNodeName, InTransform, bImportAsReferenceRotScaleEnabled);
	if (!bSuccess)
		return false;

	const int32 PartId = 0;
	if (CreateHoudiniFoliageTypeAttributes(InFoliageType, InInputNodeId, PartId, HAPI_ATTROWNER_POINT))
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), InInputNodeId), false);
		return true;
	}

	return false;
}

bool
FUnrealFoliageTypeTranslator::CreateHoudiniFoliageTypeAttributes(UFoliageType* InFoliageType, const int32& InNodeId, const int32& InPartId, HAPI_AttributeOwner InAttributeOwner)
{
	if (InNodeId < 0)
		return false;

	bool bSuccess = true;

	// Create attribute for unreal_foliage
	HAPI_AttributeInfo AttributeInfoUnrealFoliage;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoUnrealFoliage);
	AttributeInfoUnrealFoliage.tupleSize = 1;
	AttributeInfoUnrealFoliage.count = 1;
	AttributeInfoUnrealFoliage.exists = true;
	AttributeInfoUnrealFoliage.owner = InAttributeOwner;
	AttributeInfoUnrealFoliage.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoUnrealFoliage.originalOwner = HAPI_ATTROWNER_INVALID;

	// Create the new attribute
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, InPartId, HAPI_UNREAL_ATTRIB_FOLIAGE_INSTANCER, &AttributeInfoUnrealFoliage))
	{
		// The New attribute has been successfully created, set its value
		int UnrealFoliage = 1;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeIntData(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, InPartId, HAPI_UNREAL_ATTRIB_FOLIAGE_INSTANCER, &AttributeInfoUnrealFoliage,
			&UnrealFoliage, 0, 1))
		{
			bSuccess = false;
		}
	}

	if (!bSuccess)
		return false;

	// Foliage type properties that should be sent to Houdini as unreal_uproperty_ attributes.
	static TArray<FName> FoliageTypePropertyNames({
		// float
		// FName("Density"),
		// FName("DensityAdjustmentFactor"),
		// FName("Radius"),
		// FName("SingleInstanceModeRadius"),
		FName("AlignMaxAngle"),
		FName("RandomPitchAngle"),
		FName("MinimumLayerWeight"),
		FName("MinimumExclusionLayerWeight"),
		FName("CollisionRadius"),
		FName("ShadeRadius"),
		FName("InitialSeedDensity"),
		FName("AverageSpreadDistance"),
		FName("SpreadVariance"),
		FName("MaxInitialSeedOffset"),
		FName("MaxInitialAge"),
		FName("MaxAge"),
		FName("OverlapPriority"),

		// bool
		// FName("bSingleInstanceModeOverrideRadius"),
		FName("bCanGrowInShade"),
		FName("bSpawnsInShade"),
	
		// int32
		FName("OverriddenLightMapRes"),
		FName("CustomDepthStencilValue"),
		FName("TranslucencySortPriority"),
		FName("NumSteps"),
		FName("SeedsPerStep"),
		FName("DistributionSeed"),
		FName("ChangeCount"),
		FName("VirtualTextureCullMips"),

		// uint32
		FName("AlignToNormal"),
		FName("RandomYaw"),
		FName("CollisionWithWorld"),
		FName("CastShadow"),
		FName("bAffectDynamicIndirectLighting"),
		FName("bAffectDistanceFieldLighting"),
		FName("bCastDynamicShadow"),
		FName("bCastStaticShadow"),
		FName("bCastShadowAsTwoSided"),
		FName("bReceivesDecals"),
		FName("bOverrideLightMapRes"),
		FName("bUseAsOccluder"),
		FName("bRenderCustomDepth"),
		FName("ReapplyDensity"),
		FName("ReapplyRadius"),
		FName("ReapplyAlignToNormal"),
		FName("ReapplyRandomYaw"),
		FName("ReapplyScaling"),
		FName("ReapplyScaleX"),
		FName("ReapplyScaleY"),
		FName("ReapplyScaleZ"),
		FName("ReapplyRandomPitchAngle"),
		FName("ReapplyGroundSlope"),
		FName("ReapplyHeight"),
		FName("ReapplyLandscapeLayers"),
		FName("ReapplyZOffset"),
		FName("ReapplyCollisionWithWorld"),
		FName("ReapplyVertexColorMask"),
		FName("bEnableDensityScaling"),
		FName("bEnableDiscardOnLoad"),

		// enums
		// FName("Scaling"),
		FName("LightmapType"),

		// FFloatInterval
		// FName("ScaleX"),
		// FName("ScaleY"),
		// FName("ScaleZ"),
		FName("ZOffset"),
		FName("GroundSlopeAngle"),
		FName("Height"),
		FName("ProceduralScale"),

		// FVector
		FName("CollisionScale"),
		FName("LowBoundOriginRadius")});

	EAttribOwner AttribOwner;
	switch (InAttributeOwner)
	{
		case HAPI_ATTROWNER_POINT:
			AttribOwner = EAttribOwner::Point;
			break;
		case HAPI_ATTROWNER_VERTEX:
			AttribOwner = EAttribOwner::Vertex;
			break;
		case HAPI_ATTROWNER_PRIM:
			AttribOwner = EAttribOwner::Prim;
			break;
		case HAPI_ATTROWNER_DETAIL:
			AttribOwner = EAttribOwner::Detail;
			break;
		case HAPI_ATTROWNER_INVALID:
		case HAPI_ATTROWNER_MAX:
		default:
			HOUDINI_LOG_WARNING(TEXT("Unsupported Attribute Owner: %d"), InAttributeOwner);
			return false;
	}
	FHoudiniGenericAttribute GenericAttribute;
	GenericAttribute.AttributeCount = 1;
	GenericAttribute.AttributeOwner = AttribOwner;
	
	// Reserve enough space in the arrays (we only have a single point (or all are detail attributes), so attribute
	// count is 1, but the tuple size could be up to 10 for transforms
	GenericAttribute.DoubleValues.Reserve(10);
	GenericAttribute.IntValues.Reserve(10);
	GenericAttribute.StringValues.Reserve(10);

	for (const FName& PropertyName : FoliageTypePropertyNames)
	{
		const FString PropertyNameStr = PropertyName.ToString();
		GenericAttribute.AttributeName = FString::Printf(TEXT("unreal_uproperty_%s"), *PropertyNameStr);
		// Find the property on the foliage type instance
		FProperty* FoundProperty = nullptr;
		UObject* FoundPropertyObject = nullptr;
		void* Container = nullptr;
		FEditPropertyChain FoundPropertyChain;
		if (!FHoudiniGenericAttribute::FindPropertyOnObject(
				InFoliageType,
				PropertyNameStr,
				FoundPropertyChain,
				FoundProperty,
				FoundPropertyObject,
				Container))
			continue;

		if (!FHoudiniGenericAttribute::GetAttributeTupleSizeAndStorageFromProperty(
				InFoliageType,
				FoundProperty,
				Container,
				GenericAttribute.AttributeTupleSize,
				GenericAttribute.AttributeType))
			continue;
		
		const int32 AtIndex = 0;
		if (!FHoudiniGenericAttribute::GetPropertyValueFromObject(
				InFoliageType,
				FoundProperty,
				Container,
				GenericAttribute,
				AtIndex))
			continue;

		FHoudiniEngineUtils::SetGenericPropertyAttribute(InNodeId, InPartId, GenericAttribute);
	}

	return bSuccess;
}
