/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"


UHoudiniRuntimeSettings::UHoudiniRuntimeSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),

	/** Cooking options. **/
	bEnableCooking(true),
	bUploadTransformsToHoudiniEngine(false),
	bTransformChangeTriggersCooks(false),

	/** Collision generation. **/
	CollisionGroupNamePrefix(TEXT(HAPI_UNREAL_GROUP_GEOMETRY_COLLISION)),
	RenderedCollisionGroupNamePrefix(TEXT(HAPI_UNREAL_GROUP_GEOMETRY_RENDERED_COLLISION)),

	/** Geometry marshalling. **/
	MarshallingAttributeTangent(TEXT(HAPI_UNREAL_ATTRIB_TANGENT)),
	MarshallingAttributeBinormal(TEXT(HAPI_UNREAL_ATTRIB_BINORMAL)),
	MarshallingAttributeMaterial(TEXT(HAPI_UNREAL_ATTRIB_MATERIAL)),
	MarshallingAttributeFaceSmoothingMask(TEXT(HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK)),

	/** Geometry scaling. **/
	GeneratedGeometryScaleFactor(50.0f),
	TransformScaleFactor(50.0f),
	ImportAxis(HRSAI_Unreal),

	/** Generated StaticMesh settings. **/
	bDoubleSidedGeometry(false),
	PhysMaterial(nullptr),
	CollisionTraceFlag(CTF_UseDefault),
	LightMapResolution(32),
	LpvBiasMultiplier(1.0f),
	LightMapCoordinateIndex(1),
	bUseMaximumStreamingTexelRatio(false),
	StreamingDistanceMultiplier(1.0f),

	/** Static Mesh build settings. **/
	bUseFullPrecisionUVs(false),
	SrcLightmapIndex(0),
	DstLightmapIndex(1),
	MinLightmapResolution(64),
	bRemoveDegenerates(true),
	GenerateLightmapUVsFlag(HRSRF_OnlyIfMissing),
	RecomputeNormalsFlag(HRSRF_OnlyIfMissing),
	RecomputeTangentsFlag(HRSRF_OnlyIfMissing)
{

}


UHoudiniRuntimeSettings::~UHoudiniRuntimeSettings()
{

}


UProperty*
UHoudiniRuntimeSettings::LocateProperty(const FString& PropertyName)
{
	for(TFieldIterator<UProperty> PropIt(GetClass()); PropIt; ++PropIt)
	{
		UProperty* Property = *PropIt;

		if(Property->GetNameCPP() == PropertyName)
		{
			return Property;
		}
	}

	return nullptr;
}


void
UHoudiniRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Disable Collision generation options for now.
	{
		if(UProperty* Property = LocateProperty(TEXT("CollisionGroupNamePrefix")))
		{
			Property->SetPropertyFlags(CPF_EditConst);
		}

		if(UProperty* Property = LocateProperty(TEXT("RenderedCollisionGroupNamePrefix")))
		{
			Property->SetPropertyFlags(CPF_EditConst);
		}
	}
}


void
UHoudiniRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* Property = PropertyChangedEvent.MemberProperty;
	UProperty* LookupProperty = nullptr;

	if(!Property)
	{
		return;
	}

	if(Property->GetName() == TEXT("GeneratedGeometryScaleFactor"))
	{
		GeneratedGeometryScaleFactor = FMath::Clamp(GeneratedGeometryScaleFactor, KINDA_SMALL_NUMBER, 10000.0f);
	}
	else if(Property->GetName() == TEXT("TransformScaleFactor"))
	{
		GeneratedGeometryScaleFactor = FMath::Clamp(GeneratedGeometryScaleFactor, KINDA_SMALL_NUMBER, 10000.0f);
	}

	/*
	if(Property->GetName() == TEXT("bEnableCooking"))
	{
		// Cooking is disabled, we need to disable transform change triggers cooks option is as well.
		if(bEnableCooking)
		{
			LookupProperty = LocateProperty(TEXT("bUploadTransformsToHoudiniEngine"));
			if(LookupProperty)
			{
				LookupProperty->ClearPropertyFlags(CPF_EditConst);
			}

			LookupProperty = LocateProperty(TEXT("bTransformChangeTriggersCooks"));
			if(LookupProperty)
			{
				LookupProperty->ClearPropertyFlags(CPF_EditConst);
			}
		}
		else
		{
			LookupProperty = LocateProperty(TEXT("bUploadTransformsToHoudiniEngine"));
			if(LookupProperty)
			{
				LookupProperty->SetPropertyFlags(CPF_EditConst);
			}

			LookupProperty = LocateProperty(TEXT("bTransformChangeTriggersCooks"));
			if(LookupProperty)
			{
				LookupProperty->SetPropertyFlags(CPF_EditConst);
			}
		}
	}
	else if(Property->GetName() == TEXT("bUploadTransformsToHoudiniEngine"))
	{
		// If upload of transformations is disabled, then there's no sense in cooking asset on transformation change.
		if(bUploadTransformsToHoudiniEngine)
		{
			LookupProperty = LocateProperty(TEXT("bTransformChangeTriggersCooks"));
			if(LookupProperty)
			{
				LookupProperty->ClearPropertyFlags(CPF_EditConst);
			}
		}
		else
		{
			LookupProperty = LocateProperty(TEXT("bTransformChangeTriggersCooks"));
			if(LookupProperty)
			{
				LookupProperty->SetPropertyFlags(CPF_EditConst);
			}
		}
	}
	*/
}


void
UHoudiniRuntimeSettings::SetMeshBuildSettings(FMeshBuildSettings& MeshBuildSettings, FRawMesh& RawMesh) const
{
	// Removing degenerate triangles.
	MeshBuildSettings.bRemoveDegenerates = bRemoveDegenerates;

	// Recomputing normals.
	switch(RecomputeNormalsFlag)
	{
		case HRSRF_Always:
		{
			MeshBuildSettings.bRecomputeNormals = true;
			break;
		}

		case HRSRF_OnlyIfMissing:
		{
			MeshBuildSettings.bRecomputeNormals = (0 == RawMesh.WedgeTangentZ.Num());
			break;
		}

		case HRSRF_Nothing:
		default:
		{
			MeshBuildSettings.bRecomputeNormals = false;
			break;
		}
	}

	// Recomputing tangents.
	switch(RecomputeTangentsFlag)
	{
		case HRSRF_Always:
		{
			MeshBuildSettings.bRecomputeTangents = true;
			break;
		}

		case HRSRF_OnlyIfMissing:
		{
			MeshBuildSettings.bRecomputeTangents = (0 == RawMesh.WedgeTangentX.Num() || 0 == RawMesh.WedgeTangentY.Num());
			break;
		}

		case HRSRF_Nothing:
		default:
		{
			MeshBuildSettings.bRecomputeTangents = false;
			break;
		}
	}

	// Lightmap UV generation.
	bool bHasLightmapUVSet = (FHoudiniEngineUtils::CountUVSets(RawMesh) > 1);

	switch(GenerateLightmapUVsFlag)
	{
		case HRSRF_Always:
		{
			MeshBuildSettings.bGenerateLightmapUVs = true;
			break;
		}

		case HRSRF_OnlyIfMissing:
		{
			MeshBuildSettings.bGenerateLightmapUVs = !bHasLightmapUVSet;
			break;
		}

		case HRSRF_Nothing:
		default:
		{
			MeshBuildSettings.bGenerateLightmapUVs = false;
			break;
		}
	}
}
