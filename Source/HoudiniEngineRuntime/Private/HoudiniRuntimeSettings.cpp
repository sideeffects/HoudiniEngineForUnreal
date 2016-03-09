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
#include "HoudiniEngineUtils.h"


UHoudiniRuntimeSettings::UHoudiniRuntimeSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),

	/** Session options. **/
	SessionType(HRSST_InProcess),
	ServerHost(HAPI_UNREAL_SESSION_SERVER_HOST),
	ServerPort(HAPI_UNREAL_SESSION_SERVER_PORT),
	ServerPipeName(HAPI_UNREAL_SESSION_SERVER_PIPENAME),
	bStartAutomaticServer(HAPI_UNREAL_SESSION_SERVER_AUTOSTART),
	AutomaticServerTimeout(HAPI_UNREAL_SESSION_SERVER_TIMEOUT),

	/** Instantiation options. **/
	bShowMultiAssetDialog(true),

	/** Cooking options. **/
	bEnableCooking(true),
	bUploadTransformsToHoudiniEngine(true),
	bTransformChangeTriggersCooks(false),
	bDisplaySlateCookingNotifications(true),

	/** Parameter options. **/
	bTreatRampParametersAsMultiparms(false),

	/** Collision generation. **/
	CollisionGroupNamePrefix(TEXT(HAPI_UNREAL_GROUP_GEOMETRY_COLLISION)),
	RenderedCollisionGroupNamePrefix(TEXT(HAPI_UNREAL_GROUP_GEOMETRY_RENDERED_COLLISION)),

	/** Geometry marshalling. **/
	MarshallingAttributeTangent(TEXT(HAPI_UNREAL_ATTRIB_TANGENT)),
	MarshallingAttributeBinormal(TEXT(HAPI_UNREAL_ATTRIB_BINORMAL)),
	MarshallingAttributeMaterial(TEXT(HAPI_UNREAL_ATTRIB_MATERIAL)),
	MarshallingAttributeMaterialHole(TEXT(HAPI_UNREAL_ATTRIB_MATERIAL_HOLE)),
	MarshallingAttributeInstanceOverride(TEXT(HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE)),
	MarshallingAttributeFaceSmoothingMask(TEXT(HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK)),
	MarshallingAttributeLightmapResolution(TEXT(HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION)),
	MarshallingAttributeGeneratedMeshName(TEXT(HAPI_UNREAL_ATTRIB_GENERATED_MESH_NAME)),

	/** Geometry scaling. **/
	GeneratedGeometryScaleFactor(HAPI_UNREAL_SCALE_FACTOR_POSITION),
	TransformScaleFactor(HAPI_UNREAL_SCALE_FACTOR_TRANSLATION),
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
	RecomputeTangentsFlag(HRSRF_OnlyIfMissing),

	/** Custom Houdini location. **/
	bUseCustomHoudiniLocation(false)
{
	CustomHoudiniLocation.Path = TEXT("");
}


UHoudiniRuntimeSettings::~UHoudiniRuntimeSettings()
{

}


UProperty*
UHoudiniRuntimeSettings::LocateProperty(const FString& PropertyName) const
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
UHoudiniRuntimeSettings::SetPropertyReadOnly(const FString& PropertyName, bool bReadOnly)
{
	UProperty* Property = LocateProperty(PropertyName);
	if(Property)
	{
		if(bReadOnly)
		{
			Property->SetPropertyFlags(CPF_EditConst);
		}
		else
		{
			Property->ClearPropertyFlags(CPF_EditConst);
		}
	}
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

	// Disable UI elements depending on current session type.
#if WITH_EDITOR

	UpdateSessionUi();

#endif

	SetPropertyReadOnly(TEXT("CustomHoudiniLocation"), !bUseCustomHoudiniLocation);
}


#if WITH_EDITOR

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
	else if(Property->GetName() == TEXT("SessionType"))
	{
		UpdateSessionUi();
	}
	else if(Property->GetName() == TEXT("bUseCustomHoudiniLocation"))
	{
		SetPropertyReadOnly(TEXT("CustomHoudiniLocation"), !bUseCustomHoudiniLocation);
	}
	else if(Property->GetName() == TEXT("CustomHoudiniLocation"))
	{
		FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();
		FString& CustomHoudiniLocationPath = CustomHoudiniLocation.Path;
		FString LibHAPICustomPath = FString::Printf(TEXT("%s/%s"), *CustomHoudiniLocationPath, *LibHAPIName);

		// If path does not point to libHAPI location, we need to let user know.
		if(!FPaths::FileExists(LibHAPICustomPath))
		{
			FString MessageString = FString::Printf(TEXT("%s was not found in %s"), *LibHAPIName, 
				*CustomHoudiniLocationPath);

			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *MessageString,
				TEXT("Invalid Custom Location Specified, resetting."));

			CustomHoudiniLocationPath = TEXT("");
		}
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


void
UHoudiniRuntimeSettings::UpdateSessionUi()
{
	SetPropertyReadOnly(TEXT("ServerHost"), true);
	SetPropertyReadOnly(TEXT("ServerPort"), true);
	SetPropertyReadOnly(TEXT("ServerPipeName"), true);
	SetPropertyReadOnly(TEXT("bStartAutomaticServer"), true);
	SetPropertyReadOnly(TEXT("AutomaticServerTimeout"), true);

	bool bServerType = false;

	switch(SessionType)
	{
		case HRSST_Socket:
		{
			SetPropertyReadOnly(TEXT("ServerHost"), false);
			SetPropertyReadOnly(TEXT("ServerPort"), false);
			bServerType = true;
			break;
		}

		case HRSST_NamedPipe:
		{
			SetPropertyReadOnly(TEXT("ServerPipeName"), false);
			bServerType = true;
			break;
		}

		default:
		{
			break;
		}
	}

	if(bServerType)
	{
		SetPropertyReadOnly(TEXT("bStartAutomaticServer"), false);
		SetPropertyReadOnly(TEXT("AutomaticServerTimeout"), false);
	}
}

#endif


bool
UHoudiniRuntimeSettings::GetSettingsValue(const FString& PropertyName, std::string& PropertyValue)
{
	FString PropertyString = TEXT("");
	if(UHoudiniRuntimeSettings::GetSettingsValue(PropertyName, PropertyString))
	{
		FHoudiniEngineUtils::ConvertUnrealString(PropertyString, PropertyValue);
		return true;
	}

	return false;
}


bool
UHoudiniRuntimeSettings::GetSettingsValue(const FString& PropertyName, FString& PropertyValue)
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if(HoudiniRuntimeSettings)
	{
		UStrProperty* Property = Cast<UStrProperty>(HoudiniRuntimeSettings->LocateProperty(PropertyName));
		if(Property)
		{
			const void* ValueRaw = Property->ContainerPtrToValuePtr<void>(HoudiniRuntimeSettings);
			FString RetrievedPropertyValue = Property->GetPropertyValue(ValueRaw);

			if(!RetrievedPropertyValue.IsEmpty())
			{
				PropertyValue = RetrievedPropertyValue;
				return true;
			}
		}
	}

	return false;
}

