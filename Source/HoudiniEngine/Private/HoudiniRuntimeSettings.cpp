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

#include "HoudiniEnginePrivatePCH.h"


UHoudiniRuntimeSettings::UHoudiniRuntimeSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),

	/** Cooking options. **/
	bEnableCooking(true),
	bUploadTransformsToHoudiniEngine(true),
	bTransformChangeTriggersCooks(false),

	/** Collision generation. **/
	CollisionGroupName(TEXT(HAPI_UNREAL_GROUP_GEOMETRY_COLLISION)),
	RenderedCollisionGroupName(TEXT(HAPI_UNREAL_GROUP_GEOMETRY_RENDERED_COLLISION)),

	/** Geometry marshalling. **/
	MarshallingAttributeTangent(TEXT(HAPI_UNREAL_ATTRIB_TANGENT)),
	MarshallingAttributeBinormal(TEXT(HAPI_UNREAL_ATTRIB_BINORMAL)),
	MarshallingAttributeMaterial(TEXT(HAPI_UNREAL_ATTRIB_MATERIAL)),
	MarshallingAttributeFaceSmoothingMask(TEXT(HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK)),

	/** Generated StaticMesh settings. **/
	bDoubleSidedGeometry(false),
	PhysMaterial(nullptr),
	CollisionTraceFlag(CTF_UseDefault),
	LightMapResolution(32),
	LpvBiasMultiplier(1.0f),
	LightMapCoordinateIndex(1),
	bUseMaximumStreamingTexelRatio(false),
	StreamingDistanceMultiplier(1.0f)
{
#if WITH_EDITORONLY_DATA
	if(!IsRunningCommandlet())
	{

	}
#endif
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
		if(UProperty* Property = LocateProperty(TEXT("CollisionGroupName")))
		{
			Property->SetPropertyFlags(CPF_EditConst);
		}

		if(UProperty* Property = LocateProperty(TEXT("RenderedCollisionGroupName")))
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
