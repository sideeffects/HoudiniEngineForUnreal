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
#include "HoudiniAssetParameter2.h"
#include "HoudiniAssetParameter2Version.h"
#include "HoudiniAssetInstance.h"


uint32
GetTypeHash(const UHoudiniAssetParameter2* HoudiniAssetParameter)
{
	if(HoudiniAssetParameter)
	{
		return HoudiniAssetParameter->GetTypeHash();
	}

	return 0;
}


UHoudiniAssetParameter2::UHoudiniAssetParameter2(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
#if WITH_EDITOR
	DetailCategoryBuilder(nullptr),
#endif
	HoudiniAssetInstance(nullptr),
	UHoudiniAssetParentParameter(nullptr),
	ParameterName(TEXT("")),
	ParameterLabel(TEXT("")),
	ChildIndex(0),
	ParameterSize(1),
	MultiparmInstanceIndex(-1),
	ActiveChildParameter(0),
	bIsSpare(false),
	bIsEnabled(true),
	bIsVisible(true),
	bChanged(false),
	bSliderDragged(false),
	bIsChildOfMultiparm(false),
	bIsSubstanceParameter(false),
	HoudiniAssetParameterVersion(VER_HOUDINI_ENGINE_PARAM2_BASE)
{

}


UHoudiniAssetParameter2::~UHoudiniAssetParameter2()
{

}


bool
UHoudiniAssetParameter2::CreateParameter(UHoudiniAssetInstance* InHoudiniAssetInstance,
	const FHoudiniParameterObject& InHoudiniParameterObject)
{
	// Store parameter object and instance.
	HoudiniParameterObject = InHoudiniParameterObject;
	HoudiniAssetInstance = InHoudiniAssetInstance;

	// Reset child parameters.
	ChildParameters.Empty();

	// If parameter has changed, we do not need to recreate it.
	if(bChanged)
	{
		return false;
	}

	bIsVisible = HoudiniParameterObject.HapiIsVisible();
	bIsEnabled = HoudiniParameterObject.HapiIsEnabled();
	bIsSpare = HoudiniParameterObject.HapiIsSpare();
	bIsSubstanceParameter = HoudiniParameterObject.HapiIsSubstance();

	HoudiniParameterObject.HapiGetLabel(ParameterLabel);
	HoudiniParameterObject.HapiGetName(ParameterName);

	ChildIndex = HoudiniParameterObject.HapiGetChildIndex();
	ParameterSize = HoudiniParameterObject.HapiGetSize();
	MultiparmInstanceIndex = HoudiniParameterObject.HapiGetMultiparmInstanceIndex();

	return true;
}


void
UHoudiniAssetParameter2::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	HoudiniAssetParameterVersion = VER_HOUDINI_ENGINE_PARAM2_AUTOMATIC_VERSION;
	Ar << HoudiniAssetParameterVersion;

	Ar << HoudiniAssetParameterFlagsPacked;

	Ar << HoudiniParameterObject;
}


#if WITH_EDITOR

void
UHoudiniAssetParameter2::CreateWidget(IDetailCategoryBuilder& InDetailCategoryBuilder)
{
	// Store category builder.
	DetailCategoryBuilder = &InDetailCategoryBuilder;

	// Recursively create all child parameters.
	for(TArray<UHoudiniAssetParameter2*>::TIterator IterParameter(ChildParameters); IterParameter; ++IterParameter)
	{
		(*IterParameter)->CreateWidget(InDetailCategoryBuilder);
	}
}


void
UHoudiniAssetParameter2::CreateWidget(TSharedPtr<SVerticalBox> VerticalBox)
{
	// Default implementation does nothing.
}


bool
UHoudiniAssetParameter2::IsColorPickerWindowOpen() const
{
	bool bOpenWindow = false;

	for(int32 ChildIdx = 0, ChildNum = ChildParameters.Num(); ChildIdx < ChildNum; ++ChildIdx)
	{
		UHoudiniAssetParameter2* Parameter = ChildParameters[ChildIdx];
		if(Parameter)
		{
			bOpenWindow |= Parameter->IsColorPickerWindowOpen();
		}
	}

	return bOpenWindow;
}

#endif


uint32
UHoudiniAssetParameter2::GetTypeHash() const
{
	// We do hashing based on parameter name.
	return ::GetTypeHash(ParameterName);
}


bool
UHoudiniAssetParameter2::HasChanged() const
{
	return bChanged;
}


const FString&
UHoudiniAssetParameter2::GetParameterName() const
{
	return ParameterName;
}


const FString&
UHoudiniAssetParameter2::GetParameterLabel() const
{
	return ParameterLabel;
}


UHoudiniAssetInstance*
UHoudiniAssetParameter2::GetAssetInstance() const
{
	return HoudiniAssetInstance;
}


int32
UHoudiniAssetParameter2::GetSize() const
{
	return ParameterSize;
}


bool
UHoudiniAssetParameter2::IsArray() const
{
	return ParameterSize > 1;
}
