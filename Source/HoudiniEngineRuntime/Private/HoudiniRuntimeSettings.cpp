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

#include "HoudiniRuntimeSettings.h"


#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "Misc/Paths.h"
// #include "Internationalization/Internationalization.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE


FHoudiniStaticMeshGenerationProperties::FHoudiniStaticMeshGenerationProperties()
	: bGeneratedDoubleSidedGeometry(false)
	, GeneratedPhysMaterial(nullptr)
	, GeneratedCollisionTraceFlag(CTF_UseDefault)
	, GeneratedLightMapResolution(64)
	, GeneratedWalkableSlopeOverride()
	, GeneratedLightMapCoordinateIndex(1)
	, bGeneratedUseMaximumStreamingTexelRatio(false)
	, GeneratedStreamingDistanceMultiplier(1.0f)
	, GeneratedFoliageDefaultSettings(nullptr)
	, GeneratedAssetUserData()
{
	DefaultBodyInstance.SetCollisionProfileName("BlockAll");
}


UHoudiniRuntimeSettings::UHoudiniRuntimeSettings( const FObjectInitializer & ObjectInitializer )
	: Super( ObjectInitializer )
{
	// Session options.
	SessionType = HRSST_NamedPipe;
	NumSessions = 1;
	ServerHost = HAPI_UNREAL_SESSION_SERVER_HOST;
	ServerPort = HAPI_UNREAL_SESSION_SERVER_PORT;
	ServerPipeName = HAPI_UNREAL_SESSION_SERVER_PIPENAME;
	bStartAutomaticServer = HAPI_UNREAL_SESSION_SERVER_AUTOSTART;
	AutomaticServerTimeout = HAPI_UNREAL_SESSION_SERVER_TIMEOUT;
	bUseSessionSyncForAutomaticServer = false;

	SharedMemoryBufferSize = 500;
	bSharedMemoryBufferCyclic = true;
	bEnableSharedMemoryDataTransfer = false;

	bSyncWithHoudiniCook = true;
	bCookUsingHoudiniTime = true;
	bSyncViewport = false;
	bSyncHoudiniViewport = false;
	bSyncUnrealViewport = false;

	// Instantiating options.
	bShowMultiAssetDialog = true;
	bPreferHdaMemoryCopyOverHdaSourceFile = false;

	// Cooking options.
	bPauseCookingOnStart = false;
	bDisplaySlateCookingNotifications = true;
	DefaultTemporaryCookFolder = HAPI_UNREAL_DEFAULT_TEMP_COOK_FOLDER;
	DefaultBakeFolder = HAPI_UNREAL_DEFAULT_BAKE_FOLDER;
	bUsePresetsForParameters = true;

	// Parameter options
	//bTreatRampParametersAsMultiparms = false;

	// Custom Houdini location.
	bUseCustomHoudiniLocation = false;
	CustomHoudiniLocation.Path = TEXT("");
	HoudiniExecutable = HRSHE_Houdini;
	CustomHoudiniHomeLocation.Path = TEXT("");

	// Arguments for HAPI_Initialize
	CookingThreadStackSize = -1;

	// Landscape marshalling default values.
	MarshallingLandscapesUseDefaultUnrealScaling = false;
	MarshallingLandscapesUseFullResolution = true;
	MarshallingLandscapesForceMinMaxValues = false;
	MarshallingLandscapesForcedMinValue = -2000.0f;
	MarshallingLandscapesForcedMaxValue = 4553.0f;

	// Spline marshalling
	MarshallingSplineResolution = 50.0f;

	// Static mesh proxy refinement settings
	bEnableProxyStaticMesh = true;
	bShowDefaultMesh = true;
	bPreferNaniteFallbackMesh = false;

	bEnableProxyStaticMeshRefinementByTimer = false;
	ProxyMeshAutoRefineTimeoutSeconds = 10.0f;
	bEnableProxyStaticMeshRefinementOnPreSaveWorld = true;
	bEnableProxyStaticMeshRefinementOnPreBeginPIE = true;

	// Generated StaticMesh settings.
	bDoubleSidedGeometry = false;
	PhysMaterial = nullptr;
	DefaultBodyInstance.SetCollisionProfileName("BlockAll");
	CollisionTraceFlag = CTF_UseDefault;
	LightMapResolution = 32;
	LightMapCoordinateIndex = 1;
	bUseMaximumStreamingTexelRatio = false;
	StreamingDistanceMultiplier = 1.0f;
	GeneratedDistanceFieldResolutionScale = 0.0f;

	// Static Mesh build settings.
	bUseFullPrecisionUVs = false;
	SrcLightmapIndex = 0;
	DstLightmapIndex = 1;
	MinLightmapResolution = 64;
	bRemoveDegenerates = true;
	GenerateLightmapUVsFlag = HRSRF_OnlyIfMissing;
	RecomputeNormalsFlag = HRSRF_OnlyIfMissing;
	RecomputeTangentsFlag = HRSRF_OnlyIfMissing;
	bUseMikkTSpace = true;

	bComputeWeightedNormals = false;
	bBuildReversedIndexBuffer = true;
	bUseHighPrecisionTangentBasis = false;
	bGenerateDistanceFieldAsIfTwoSided = false;
	bSupportFaceRemap = false;
	//BuildScale3D = FVector(1.0f, 1.0f, 1.0f);
	DistanceFieldResolutionScale = 2.0f; // ue default is 1.0

	bPDGAsyncCommandletImportEnabled = false;

	// Curve inputs and editable output curves
	bAddRotAndScaleAttributesOnCurves = false;
	bUseLegacyInputCurves = true;

	// Houdini Tools
	HoudiniToolsSearchPath = { TEXT("/Game/HoudiniEngine/Tools") };
}

UHoudiniRuntimeSettings::~UHoudiniRuntimeSettings()
{}


FProperty *
UHoudiniRuntimeSettings::LocateProperty(const FString & PropertyName) const
{
	for (TFieldIterator< FProperty > PropIt(GetClass()); PropIt; ++PropIt)
	{
		FProperty * Property = *PropIt;

		if (Property->GetNameCPP() == PropertyName)
			return Property;
	}

	return nullptr;
}


void
UHoudiniRuntimeSettings::SetPropertyReadOnly(const FString & PropertyName, bool bReadOnly)
{
	FProperty * Property = LocateProperty(PropertyName);
	if (Property)
	{
		if (bReadOnly)
			Property->SetPropertyFlags(CPF_EditConst);
		else
			Property->ClearPropertyFlags(CPF_EditConst);
	}
}


void
UHoudiniRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Set Collision generation options as read only
	{
		if (FProperty* Property = LocateProperty(TEXT("CollisionGroupNamePrefix")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("RenderedCollisionGroupNamePrefix")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("UCXCollisionGroupNamePrefix")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("UCXRenderedCollisionGroupNamePrefix")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("SimpleCollisionGroupNamePrefix")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("SimpleRenderedCollisionGroupNamePrefix")))
			Property->SetPropertyFlags(CPF_EditConst);
	}

	// Set marshalling attributes options as read only
	{
		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeMaterial")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeMaterialHole")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeInstanceOverride")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeFaceSmoothingMask")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeLightmapResolution")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeGeneratedMeshName")))
			Property->SetPropertyFlags(CPF_EditConst);

		if (FProperty* Property = LocateProperty(TEXT("MarshallingAttributeInputMeshName")))
			Property->SetPropertyFlags(CPF_EditConst);
	}

	/*
		// Set Cook Folder as read-only
		{
			if ( FProperty* Property = LocateProperty( TEXT( "TemporaryCookFolder" ) ) )
				Property->SetPropertyFlags( CPF_EditConst );
		}
	*/

	// Set Landscape forced min/max as read only when not overriden
	if (!MarshallingLandscapesForceMinMaxValues)
	{
		if (FProperty* Property = LocateProperty(TEXT("MarshallingLandscapesForcedMinValue")))
			Property->SetPropertyFlags(CPF_EditConst);
		if (FProperty* Property = LocateProperty(TEXT("MarshallingLandscapesForcedMaxValue")))
			Property->SetPropertyFlags(CPF_EditConst);
	}

	// Disable UI elements depending on current session type.
#if WITH_EDITOR

	UpdateSessionUI();

#endif // WITH_EDITOR
}


#if WITH_EDITOR
void
UHoudiniRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty * Property = PropertyChangedEvent.MemberProperty;
	FProperty * LookupProperty = nullptr;

	if (!Property)
		return;
	if (Property->GetName() == TEXT("SessionType"))
		UpdateSessionUI();
	else if (Property->GetName() == TEXT("CustomHoudiniLocation"))
	{
		if (!FHoudiniEngineRuntimeUtils::CheckCustomHoudiniLocation(CustomHoudiniLocation.Path))
			CustomHoudiniLocation.Path = TEXT("");
	}
	else if (Property->GetName() == TEXT("MarshallingLandscapesForceMinMaxValues"))
	{
		// Set Landscape forced min/max as read only when not overriden
		if (!MarshallingLandscapesForceMinMaxValues)
		{
			if (FProperty* MinProperty = LocateProperty(TEXT("MarshallingLandscapesForcedMinValue")))
				MinProperty->SetPropertyFlags(CPF_EditConst);
			if (FProperty* MaxProperty = LocateProperty(TEXT("MarshallingLandscapesForcedMaxValue")))
				MaxProperty->SetPropertyFlags(CPF_EditConst);
		}
		else
		{
			if (FProperty* MinProperty = LocateProperty(TEXT("MarshallingLandscapesForcedMinValue")))
				MinProperty->ClearPropertyFlags(CPF_EditConst);
			if (FProperty* MaxProperty = LocateProperty(TEXT("MarshallingLandscapesForcedMaxValue")))
				MaxProperty->ClearPropertyFlags(CPF_EditConst);
		}
	}

	/*
	if ( Property->GetName() == TEXT( "bEnableCooking" ) )
	{
		// Cooking is disabled, we need to disable transform change triggers cooks option is as well.
		if ( bEnableCooking )
		{
			LookupProperty = LocateProperty( TEXT( "bUploadTransformsToHoudiniEngine" ) );
			if ( LookupProperty )
				LookupProperty->ClearPropertyFlags( CPF_EditConst );

			LookupProperty = LocateProperty( TEXT( "bTransformChangeTriggersCooks" ) );
			if ( LookupProperty )
				LookupProperty->ClearPropertyFlags( CPF_EditConst );
		}
		else
		{
			LookupProperty = LocateProperty( TEXT( "bUploadTransformsToHoudiniEngine" ) );
			if ( LookupProperty )
				LookupProperty->SetPropertyFlags( CPF_EditConst );

			LookupProperty = LocateProperty( TEXT( "bTransformChangeTriggersCooks" ) );
			if ( LookupProperty )
				LookupProperty->SetPropertyFlags( CPF_EditConst );
		}
	}
	else if ( Property->GetName() == TEXT( "bUploadTransformsToHoudiniEngine" ) )
	{
		// If upload of transformations is disabled, then there's no sense in cooking asset on transformation change.
		if ( bUploadTransformsToHoudiniEngine )
		{
			LookupProperty = LocateProperty( TEXT( "bTransformChangeTriggersCooks" ) );
			if ( LookupProperty )
				LookupProperty->ClearPropertyFlags( CPF_EditConst );
		}
		else
		{
			LookupProperty = LocateProperty( TEXT( "bTransformChangeTriggersCooks" ) );
			if ( LookupProperty )
				LookupProperty->SetPropertyFlags( CPF_EditConst );
		}
	}
	*/
}



void
UHoudiniRuntimeSettings::UpdateSessionUI()
{
	SetPropertyReadOnly(TEXT("ServerHost"), true);
	SetPropertyReadOnly(TEXT("ServerPort"), true);
	SetPropertyReadOnly(TEXT("ServerPipeName"), true);
	SetPropertyReadOnly(TEXT("bStartAutomaticServer"), true);
	SetPropertyReadOnly(TEXT("AutomaticServerTimeout"), true);
	SetPropertyReadOnly(TEXT("SharedMemoryBufferSize"), true);
	SetPropertyReadOnly(TEXT("bSharedMemoryBufferCyclic"), true);
	SetPropertyReadOnly(TEXT("bEnableSharedMemoryDataTransfer"), true);

	bool bServerType = false;

	switch (SessionType)
	{
	case HRSST_Socket:
	{
		SetPropertyReadOnly(TEXT("ServerHost"), false);
		SetPropertyReadOnly(TEXT("ServerPort"), false);
		SetPropertyReadOnly(TEXT("bEnableSharedMemoryDataTransfer"), false);
		bServerType = true;
		break;
	}

	case HRSST_NamedPipe:
	{
		SetPropertyReadOnly(TEXT("ServerPipeName"), false);
		SetPropertyReadOnly(TEXT("bEnableSharedMemoryDataTransfer"), false);
		bServerType = true;
		break;
	}

	case HRSST_MemoryBuffer:
	{
		SetPropertyReadOnly(TEXT("ServerPipeName"), false);
		SetPropertyReadOnly(TEXT("SharedMemoryBufferSize"), false);
		SetPropertyReadOnly(TEXT("bSharedMemoryBufferCyclic"), false);
		//SetPropertyReadOnly(TEXT("bEnableSharedMemoryDataTransfer"), true);

		bServerType = true;
		break;
	}

	default:
		break;
	}

	if (bServerType)
	{
		SetPropertyReadOnly(TEXT("bStartAutomaticServer"), false);
		SetPropertyReadOnly(TEXT("AutomaticServerTimeout"), false);
	}

	if (bEnableSharedMemoryDataTransfer)
	{
		SetPropertyReadOnly(TEXT("SharedMemoryBufferSize"), false);
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
