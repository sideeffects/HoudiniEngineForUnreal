/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*/

#include "HoudiniDynamicMeshTranslator.h"

#include "HoudiniDataLayerUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniMeshTranslatorUtils.h"

#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "HoudiniHLODLayerUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

bool FHoudiniDynamicMeshTranslator::IsDynamicMeshOutput(const FHoudiniGeoPartObject& InHGPO)
{
	FHoudiniHapiAccessor Accessor(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_OUTPUT_DYNAMIC_MESH);
	int Value = 0;
	bool bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_DETAIL, Value);
	if (!bSuccess)
	{
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_PRIM, Value);
	}

	if (bSuccess)
		return Value != 0;

	if (!InHGPO.PartInfo.bIsInstanced)
		return false;

	HAPI_GeoInfo GeoInfo;
	FHoudiniApi::GeoInfo_Init(&GeoInfo);
	const HAPI_Result GetGeoInfoResult = FHoudiniApi::GetGeoInfo(
		FHoudiniEngine::Get().GetSession(), InHGPO.GeoId, &GeoInfo);
	bSuccess = GetGeoInfoResult == HAPI_RESULT_SUCCESS;
	if (!bSuccess)
		return false;

	for (int32 PartId = 0; PartId < GeoInfo.partCount; ++PartId)
	{
		if (PartId == InHGPO.PartId)
			continue;

		Accessor.Init(InHGPO.GeoId, PartId, HAPI_UNREAL_ATTRIB_OUTPUT_DYNAMIC_MESH);
		bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_DETAIL, Value);
		if (!bSuccess)
		{
			bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_PRIM, Value);
		}

		if (bSuccess && Value != 0)
			return true;
	}

	return false;
}

bool FHoudiniDynamicMeshTranslator::CreateDynamicMeshesFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutAssignmentMaterialMap,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterialMap,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
	UObject* InOuterComponent,
	bool bInForceRebuild,
	bool bInTreatExistingMaterialsAsUpToDate)
{
	if (!IsValid(InOuterComponent))
		return false;

	if (!bInForceRebuild && !InHGPO.bHasGeoChanged && !InHGPO.bHasPartChanged && InOutputObjects.Num() > 0)
	{
		OutOutputObjects = InOutputObjects;
		return true;
	}

	FHoudiniGeoPartObject HGPO = InHGPO;
	FHoudiniMeshTranslatorUtils::UpdateSplitGroups(HGPO);

	FHoudiniCachedMeshPart CachedMeshPart;
	if (!CachedMeshPart.ExtractHoudiniMeshPart(HGPO))
		return false;

	CachedMeshPart.PrepareSplitMeshBuildPlan(HGPO);

	FHoudiniMeshTranslationContext TranslationContext;
	TranslationContext.InputObjects = InOutputObjects;
	TranslationContext.OutputObjects = OutOutputObjects;
	TranslationContext.bTreatExistingMaterialsAsUpToDate = bInTreatExistingMaterialsAsUpToDate;
	TranslationContext.bForceRebuild = bInForceRebuild;

	FHoudiniPackageParams PackageParams = InPackageParams;
	PackageParams.ObjectId = HGPO.ObjectId;
	PackageParams.GeoId = HGPO.GeoId;
	PackageParams.PartId = HGPO.PartId;

	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> OutputAssignmentMaterials;
	bool bSuccess = FHoudiniMeshTranslatorUtils::CreateNeededMaterials(
		HGPO,
		CachedMeshPart,
		PackageParams,
		InOuterComponent,
		TranslationContext,
		OutputAssignmentMaterials,
		InOutAssignmentMaterialMap,
		InAllOutputMaterials);
	if (!bSuccess)
		return false;

	bSuccess = CreateDynamicMeshesFromSplitGroups(
		HGPO,
		CachedMeshPart,
		OutOutputObjects,
		InOuterComponent,
		OutputAssignmentMaterials,
		InReplacementMaterialMap);
	if (!bSuccess)
		return false;

	InOutAssignmentMaterialMap = MoveTemp(OutputAssignmentMaterials);
	return true;
}

bool FHoudiniDynamicMeshTranslator::CreateDynamicMeshesFromSplitGroups(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	UObject* InOuterComponent,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials)
{
	bool bSuccess = true;
	for (TPair<FString, FHoudiniSplitGroupMesh>& Pair : InOutCachedMeshPart.MeshesToBuild.Meshes)
	{
		// Custom complex collision is represented by an auxiliary static mesh. Dynamic Mesh
		// output currently supports simple collision only, so do not emit that extra payload.
		if (!Pair.Value.CustomCollisionOwner.IsEmpty())
			continue;

		bSuccess &= CreateDynamicMeshFromSplitGroup(
			InHGPO,
			InOutCachedMeshPart,
			Pair.Value,
			OutOutputObjects,
			InOuterComponent,
			InOutOutputAssignmentMaterials,
			InReplacementMaterials);
	}

	return bSuccess;
}

bool FHoudiniDynamicMeshTranslator::CreateDynamicMeshFromSplitGroup(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	FHoudiniSplitGroupMesh& InOutSplitMeshData,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
	UObject* InOuterComponent,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials)
{
	if (InOutSplitMeshData.LODRenders.IsEmpty())
	{
		HOUDINI_LOG_WARNING(TEXT("Dynamic Mesh split has no render group."));
		return true;
	}

	// Dynamic meshes have no LOD representation. Use the base render split, matching the split-mesh output identity.
	const int32 BaseRenderIndex = InOutSplitMeshData.LODRenders[0];
	const FString& SplitGroupName = InOutSplitMeshData.SplitMeshData[BaseRenderIndex].SplitGroupName;
	const EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(SplitGroupName);
	const FHoudiniOutputObjectIdentifier OutputIdentifier = FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(
		InHGPO, InOutCachedMeshPart, SplitGroupName, SplitType);

	const FName DynamicMeshName = MakeUniqueObjectName(
		InOuterComponent,
		UDynamicMesh::StaticClass(),
		FName(*InOutSplitMeshData.SplitMeshData[BaseRenderIndex].StaticMeshName));
	UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>(InOuterComponent, DynamicMeshName, RF_Transactional);
	if (!IsValid(DynamicMesh))
		return false;

	TArray<TObjectPtr<UMaterialInterface>> OutputMaterials;
	TArray<int32> FaceMaterialIndices;
	BuildMaterialSlots(
		InHGPO,
		InOutCachedMeshPart,
		SplitGroupName,
		InOutOutputAssignmentMaterials,
		InReplacementMaterials,
		OutputMaterials,
		FaceMaterialIndices);

	if (!BuildDynamicMesh(InHGPO, InOutCachedMeshPart, SplitGroupName, FaceMaterialIndices, *DynamicMesh))
		return false;

	FHoudiniOutputObject& OutputObject = OutOutputObjects.FindOrAdd(OutputIdentifier);
	OutputObject.bProxyIsCurrent = false;
	OutputObject.OutputObject = DynamicMesh;
	OutputObject.OutputMaterials = MoveTemp(OutputMaterials);
	OutputObject.DynamicMeshSimpleCollision = FHoudiniMeshTranslatorUtils::BuildAggregateCollision(
		InHGPO, InOutCachedMeshPart, InOutSplitMeshData);
	OutputObject.bIsImplicit = false;
	OutputObject.bIsInvisibleCollisionMesh = !InOutSplitMeshData.bIsVisible;
	OutputObject.DataLayers = FHoudiniDataLayerUtils::GetDataLayers(InHGPO.GeoId, InHGPO.PartId);
	OutputObject.HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(InHGPO.GeoId, InHGPO.PartId);
	FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(
		InHGPO,
		InOutCachedMeshPart,
		SplitGroupName,
		OutputObject.CachedAttributes,
		OutputObject.CachedTokens);

	return true;
}

void FHoudiniDynamicMeshTranslator::BuildMaterialSlots(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FString& InSplitGroupName,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
	TArray<TObjectPtr<UMaterialInterface>>& OutMaterials,
	TArray<int32>& OutFaceMaterialIndices)
{
	const FPreparedHoudiniMeshSplit& PreparedSplit = InOutCachedMeshPart.Splits.FindChecked(InSplitGroupName);
	OutFaceMaterialIndices.SetNumZeroed(PreparedSplit.FaceIndices.Num());

	UMaterialInterface* const DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial(InHGPO.bIsTemplated).Get();
	auto ApplyReplacement = [&InReplacementMaterials](const FHoudiniMaterialIdentifier& InIdentifier, UMaterialInterface* InMaterial)
	{
		const TObjectPtr<UMaterialInterface>* ReplacementMaterial = InReplacementMaterials.Find(InIdentifier);
		return ReplacementMaterial && *ReplacementMaterial ? ReplacementMaterial->Get() : InMaterial;
	};
	auto AddMaterial = [&OutMaterials](UMaterialInterface* InMaterial, int32 InRequestedIndex)
	{
		if (InRequestedIndex == INDEX_NONE)
		{
			const int32 ExistingIndex = OutMaterials.IndexOfByKey(InMaterial);
			if (ExistingIndex != INDEX_NONE)
				return ExistingIndex;

			return OutMaterials.Add(InMaterial);
		}

		while (OutMaterials.Num() <= InRequestedIndex)
			OutMaterials.Add(nullptr);
		OutMaterials[InRequestedIndex] = InMaterial;
		return InRequestedIndex;
	};

	const TArray<int32>& FaceMaterialIds = InOutCachedMeshPart.GetFaceMaterialIds(InHGPO);
	const TArray<FHoudiniMaterialInfo>& FaceMaterialOverrides = InOutCachedMeshPart.GetFaceMaterialOverrides(InHGPO);
	for (int32 FaceIndex = 0; FaceIndex < PreparedSplit.FaceIndices.Num(); ++FaceIndex)
	{
		const int32 SourceFaceIndex = PreparedSplit.FaceIndices[FaceIndex];
		UMaterialInterface* Material = DefaultMaterial;
		int32 MaterialIndex = INDEX_NONE;

		if (FaceMaterialOverrides.IsValidIndex(SourceFaceIndex))
		{
			const FHoudiniMaterialInfo& MaterialInfo = FaceMaterialOverrides[SourceFaceIndex];
			const FHoudiniMaterialIdentifier MaterialIdentifier = MaterialInfo.MakeIdentifier();
			FString MaterialName = MaterialInfo.MaterialObjectPath;
			FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(MaterialName, MaterialIndex);

			if (const TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(MaterialIdentifier))
				Material = *FoundMaterial;
			else if (!MaterialName.IsEmpty())
			{
				Material = Cast<UMaterialInterface>(StaticLoadObject(
					UMaterialInterface::StaticClass(), nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr));
				if (Material)
					InOutOutputAssignmentMaterials.Add(MaterialIdentifier, Material);
			}

			Material = ApplyReplacement(MaterialIdentifier, Material);
		}
		else if (FaceMaterialIds.IsValidIndex(SourceFaceIndex))
		{
			FString MaterialPathName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			const bool bIsHoudiniMaterial = FHoudiniMaterialTranslator::GetMaterialRelativePath(
				InHGPO.AssetId, FaceMaterialIds[SourceFaceIndex], MaterialPathName);
			const FHoudiniMaterialIdentifier MaterialIdentifier(MaterialPathName, bIsHoudiniMaterial);
			if (const TObjectPtr<UMaterialInterface>* FoundMaterial = InOutOutputAssignmentMaterials.Find(MaterialIdentifier))
				Material = *FoundMaterial;

			Material = ApplyReplacement(MaterialIdentifier, Material);
		}

		OutFaceMaterialIndices[FaceIndex] = AddMaterial(Material, MaterialIndex);
	}

	if (OutMaterials.IsEmpty())
		OutMaterials.Add(DefaultMaterial);
}

bool FHoudiniDynamicMeshTranslator::BuildDynamicMesh(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FString& InSplitGroupName,
	const TArray<int32>& InFaceMaterialIndices,
	UDynamicMesh& OutDynamicMesh)
{
	const FPreparedHoudiniMeshSplit* PreparedSplit = InOutCachedMeshPart.Splits.Find(InSplitGroupName);
	if (!PreparedSplit || PreparedSplit->VertexCount % 3 != 0 || PreparedSplit->VertexList.Num() % 3 != 0)
	{
		HOUDINI_LOG_WARNING(
			TEXT("Creating Dynamic Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%s] has an invalid vertex count - skipping."),
			InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, *InSplitGroupName);
		return false;
	}

	const TArray<int32>& SplitVertexList = PreparedSplit->VertexList;
	TArray<int32> PartToDynamicMeshVertex;
	PartToDynamicMeshVertex.Init(INDEX_NONE, SplitVertexList.Num());

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	TArray<int32> DynamicMeshTriangleIds;
	TArray<UE::Geometry::FIndex3i> TriangleSourceCorners;
	TArray<int32> TriangleSourceFaceIndices;
	bool bHasInvalidData = false;
	for (int32 VertexIndex = 0; VertexIndex < SplitVertexList.Num(); VertexIndex += 3)
	{
		if (SplitVertexList[VertexIndex] == INDEX_NONE)
			continue;

		int32 TriangleVertexIndices[3] =
		{
			SplitVertexList[VertexIndex],
			SplitVertexList[VertexIndex + 1],
			SplitVertexList[VertexIndex + 2]
		};

		if (!PartToDynamicMeshVertex.IsValidIndex(TriangleVertexIndices[0])
			|| !PartToDynamicMeshVertex.IsValidIndex(TriangleVertexIndices[1])
			|| !PartToDynamicMeshVertex.IsValidIndex(TriangleVertexIndices[2]))
		{
			bHasInvalidData = true;
			continue;
		}

		for (int32 CornerIndex = 0; CornerIndex < UE_ARRAY_COUNT(TriangleVertexIndices); ++CornerIndex)
		{
			const int32 PartVertexIndex = TriangleVertexIndices[CornerIndex];
			int32& DynamicMeshVertexIndex = PartToDynamicMeshVertex[PartVertexIndex];
			if (DynamicMeshVertexIndex != INDEX_NONE)
				continue;

			const TArray<float>& Positions = InOutCachedMeshPart.GetPositions(InHGPO);
			if (!Positions.IsValidIndex(PartVertexIndex * 3 + 2))
			{
				bHasInvalidData = true;
				break;
			}

			DynamicMeshVertexIndex = DynamicMesh.AppendVertex(FVector3d(
				Positions[PartVertexIndex * 3] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
				Positions[PartVertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION,
				Positions[PartVertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION));
		}

		if (bHasInvalidData)
			continue;

		// Houdini and Unreal use opposite winding orders after the Y/Z axis conversion.
		const int32 DynamicMeshTriangleId = DynamicMesh.AppendTriangle(
			PartToDynamicMeshVertex[TriangleVertexIndices[0]],
			PartToDynamicMeshVertex[TriangleVertexIndices[2]],
			PartToDynamicMeshVertex[TriangleVertexIndices[1]]);
		if (DynamicMeshTriangleId < 0)
		{
			bHasInvalidData = true;
		}
		else
		{
			// TransferPartAttributesToSplit compacts the attributes to only the
			// corners belonging to this split. Keep indices into that compacted
			// data rather than the original part vertex list: collision and LOD
			// splits leave gaps in SplitVertexList.
			int32 SplitTriangleIndex = DynamicMeshTriangleIds.Num();
			DynamicMeshTriangleIds.Add(DynamicMeshTriangleId);
			TriangleSourceCorners.Add(UE::Geometry::FIndex3i(
				SplitTriangleIndex * 3,
				SplitTriangleIndex * 3 + 2,
				SplitTriangleIndex * 3 + 1));
			TriangleSourceFaceIndices.Add(SplitTriangleIndex);
		}
	}

	if (bHasInvalidData)
	{
		HOUDINI_LOG_WARNING(
			TEXT("Creating Dynamic Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%s] has invalid topology or positions."),
			InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, *InSplitGroupName);
	}

	if (DynamicMesh.TriangleCount() == 0)
		return false;

	// TransferPartAttributesToSplit() removes the -1 corners that belong to
	// other split groups. Attribute arrays therefore have one entry per corner
	// in DynamicMeshTriangleIds, not per entry in SplitVertexList.
	int32 NumSplitCorners = DynamicMeshTriangleIds.Num() * 3;

	TArray<float> SplitNormals;
	const TArray<float>& PartNormals = InOutCachedMeshPart.GetNormals(InHGPO, true);
	const HAPI_AttributeInfo& NormalAttributeInfo = InOutCachedMeshPart.GetNormalAttributeInfo(InHGPO);
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitVertexList,
		NormalAttributeInfo,
		PartNormals,
		SplitNormals);
	const bool bHasNormals = SplitNormals.Num() == NumSplitCorners * 3;

	TArray<TArray<float>> SplitUVSets;
	const TArray<HAPI_AttributeInfo>& UVAttributeInfos = InOutCachedMeshPart.GetUVSetAttributeInfos(InHGPO);
	const TArray<TArray<float>>& UVSets = InOutCachedMeshPart.GetUVSets(InHGPO);
	SplitUVSets.SetNum(UVSets.Num());
	int32 NumUVLayers = 0;
	for (int32 UVIndex = 0; UVIndex < UVSets.Num(); ++UVIndex)
	{
		if (UVAttributeInfos.IsValidIndex(UVIndex))
		{
			FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
				SplitVertexList,
				UVAttributeInfos[UVIndex],
				UVSets[UVIndex],
				SplitUVSets[UVIndex]);
		}

		if (SplitUVSets[UVIndex].Num() == NumSplitCorners * 2)
			NumUVLayers = UVIndex + 1;
	}

	TArray<float> SplitColors;
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitVertexList,
		InOutCachedMeshPart.GetColorAttributeInfo(InHGPO),
		InOutCachedMeshPart.GetColors(InHGPO),
		SplitColors);
	TArray<float> SplitAlphas;
	FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
		SplitVertexList,
		InOutCachedMeshPart.GetAlphaAttributeInfo(InHGPO),
		InOutCachedMeshPart.GetAlphas(InHGPO),
		SplitAlphas);
	const HAPI_AttributeInfo& ColorAttributeInfo = InOutCachedMeshPart.GetColorAttributeInfo(InHGPO);
	const bool bHasColors = ColorAttributeInfo.exists
		&& ColorAttributeInfo.tupleSize >= 3
		&& SplitColors.Num() == NumSplitCorners * ColorAttributeInfo.tupleSize;
	const bool bHasAlphas = SplitAlphas.Num() == NumSplitCorners;

	DynamicMesh.EnableAttributes();
	UE::Geometry::FDynamicMeshAttributeSet* Attributes = DynamicMesh.Attributes();
	// Keep one UV channel even when Houdini did not provide UVs. Static Mesh conversion
	// requires at least one channel and treats the unset overlay triangles as (0, 0).
	Attributes->SetNumUVLayers(FMath::Max(1, NumUVLayers));
	Attributes->EnableMaterialID();
	if (bHasColors || bHasAlphas)
		Attributes->EnablePrimaryColors();

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = bHasNormals ? Attributes->PrimaryNormals() : nullptr;
	UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = (bHasColors || bHasAlphas) ? Attributes->PrimaryColors() : nullptr;
	TMap<int32, TArray<TPair<FVector3f, int32>>> NormalElementsByVertex;
	auto GetOrCreateNormalElement = [&NormalElementsByVertex, &SplitNormals, NormalOverlay](int32 DynamicMeshVertexId, int32 SourceCorner)
	{
		FVector3f Normal(
			SplitNormals[SourceCorner * 3],
			SplitNormals[SourceCorner * 3 + 2],
			SplitNormals[SourceCorner * 3 + 1]);
		TArray<TPair<FVector3f, int32>>& NormalElements = NormalElementsByVertex.FindOrAdd(DynamicMeshVertexId);
		for (TPair<FVector3f, int32>& Element : NormalElements)
		{
			if (Element.Key.Equals(Normal))
				return Element.Value;
		}

		int32 NormalElementId = NormalOverlay->AppendElement(Normal);
		NormalOverlay->SetParentVertex(NormalElementId, DynamicMeshVertexId);
		NormalElements.Add(TPair<FVector3f, int32>(Normal, NormalElementId));
		return NormalElementId;
	};
	for (int32 TriangleIndex = 0; TriangleIndex < DynamicMeshTriangleIds.Num(); ++TriangleIndex)
	{
		const UE::Geometry::FIndex3i& SourceCorners = TriangleSourceCorners[TriangleIndex];
		const int32 DynamicMeshTriangleId = DynamicMeshTriangleIds[TriangleIndex];
		if (NormalOverlay)
		{
			UE::Geometry::FIndex3i TriangleVertices = DynamicMesh.GetTriangle(DynamicMeshTriangleId);
			NormalOverlay->SetTriangle(DynamicMeshTriangleId, UE::Geometry::FIndex3i(
				GetOrCreateNormalElement(TriangleVertices.A, SourceCorners.A),
				GetOrCreateNormalElement(TriangleVertices.B, SourceCorners.B),
				GetOrCreateNormalElement(TriangleVertices.C, SourceCorners.C)));
		}

		for (int32 UVIndex = 0; UVIndex < NumUVLayers; ++UVIndex)
		{
			UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = Attributes->GetUVLayer(UVIndex);
			const TArray<float>& SplitUVSet = SplitUVSets[UVIndex];
			if (SplitUVSet.Num() != NumSplitCorners * 2)
				continue;

			auto AppendUV = [&SplitUVSet](int32 SourceCorner)
			{
				return FVector2f(SplitUVSet[SourceCorner * 2], 1.0f - SplitUVSet[SourceCorner * 2 + 1]);
			};
			UVOverlay->SetTriangle(DynamicMeshTriangleId, UE::Geometry::FIndex3i(
				UVOverlay->AppendElement(AppendUV(SourceCorners.A)),
				UVOverlay->AppendElement(AppendUV(SourceCorners.B)),
				UVOverlay->AppendElement(AppendUV(SourceCorners.C))));
		}

		if (ColorOverlay)
		{
			auto AppendColor = [&SplitColors, &SplitAlphas, &ColorAttributeInfo, bHasAlphas](int32 SourceCorner)
			{
				FVector4f Color = FVector4f::One();
				if (ColorAttributeInfo.exists)
				{
					Color.X = FMath::Clamp(SplitColors[SourceCorner * ColorAttributeInfo.tupleSize], 0.0f, 1.0f);
					Color.Y = FMath::Clamp(SplitColors[SourceCorner * ColorAttributeInfo.tupleSize + 1], 0.0f, 1.0f);
					Color.Z = FMath::Clamp(SplitColors[SourceCorner * ColorAttributeInfo.tupleSize + 2], 0.0f, 1.0f);
					if (ColorAttributeInfo.tupleSize >= 4)
						Color.W = FMath::Clamp(SplitColors[SourceCorner * ColorAttributeInfo.tupleSize + 3], 0.0f, 1.0f);
				}
				if (bHasAlphas)
					Color.W = FMath::Clamp(SplitAlphas[SourceCorner], 0.0f, 1.0f);
				return Color;
			};
			ColorOverlay->SetTriangle(DynamicMeshTriangleId, UE::Geometry::FIndex3i(
				ColorOverlay->AppendElement(AppendColor(SourceCorners.A)),
				ColorOverlay->AppendElement(AppendColor(SourceCorners.B)),
				ColorOverlay->AppendElement(AppendColor(SourceCorners.C))));
		}

		const int32 SourceFaceIndex = TriangleSourceFaceIndices[TriangleIndex];
		const int32 MaterialIndex = InFaceMaterialIndices.IsValidIndex(SourceFaceIndex) ? InFaceMaterialIndices[SourceFaceIndex] : 0;
		Attributes->GetMaterialID()->SetValue(DynamicMeshTriangleId, MaterialIndex);
	}

	if (!bHasNormals)
		UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(Attributes->PrimaryNormals());

	OutDynamicMesh.SetMesh(MoveTemp(DynamicMesh));
	return true;
}
