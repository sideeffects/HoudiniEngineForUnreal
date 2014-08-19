/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"
#include <stdint.h>


#define HOUDINI_TEST_LOG_MESSAGE( NameWithSpaces ) \
	do \
	{ \
		int32 OldCount = Count; \
		Count++; \
		HOUDINI_LOG_MESSAGE( \
			TEXT( NameWithSpaces ) TEXT( "omponent = 0x%x, Value = %d, RValue = %d, Count = %d -> %d || D = %d, B = %d, C = %d, T = %d" ), \
			this, Value, RememberedValue, OldCount, Count, bIsDefaultClass, bIsBlueprintGeneratedClass, \
			bIsBlueprintConstructionScriptClass, bIsBlueprintThumbnailSceneClass ); \
	} while(false)


UHoudiniTestComponent::UHoudiniTestComponent(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	Value(0),
	RememberedValue(0),
	bIsDefaultClass(false),
	bIsBlueprintGeneratedClass(false),
	bIsBlueprintConstructionScriptClass(false),
	bIsBlueprintThumbnailSceneClass(false)
{
	UObject* Archetype = PCIP.GetArchetype();
	UObject* Obj = PCIP.GetObject();

	if(Obj->GetOuter() && Obj->GetOuter()->GetName().StartsWith(TEXT("/Script")))
	{
		bIsDefaultClass = true;
	}
	else if(!Archetype && Obj->GetOuter() && Obj->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass()))
	{
		bIsBlueprintGeneratedClass = true;
	}
	else if(Obj->GetOuter() && Obj->GetOuter()->IsA(AActor::StaticClass()))
	{
		bIsBlueprintConstructionScriptClass = true;
	}
	else if(Obj->GetOuter() && Obj->GetOuter()->IsA(UPackage::StaticClass())
		&& Obj->GetOuter()->GetName().StartsWith(TEXT("/Engine/Transient")))
	{
		bIsBlueprintThumbnailSceneClass = true;
	}
	else
	{
		bIsBlueprintThumbnailSceneClass = false;
	}

	HOUDINI_TEST_LOG_MESSAGE( "Constructor,                          C" );
}


UHoudiniTestComponent::~UHoudiniTestComponent()
{
	HOUDINI_TEST_LOG_MESSAGE( "Destructor,                           C" );
}


void
UHoudiniTestComponent::OnComponentDestroyed()
{
	HOUDINI_TEST_LOG_MESSAGE( "  OnComponentDestroyed,               C" );

	UnsubscribeEditorDelegates();
}


void
UHoudiniTestComponent::BeginDestroy()
{
	Super::BeginDestroy();
}


void
UHoudiniTestComponent::FinishDestroy()
{
	Super::FinishDestroy();
}


bool
UHoudiniTestComponent::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy();
}


void
UHoudiniTestComponent::SubscribeEditorDelegates()
{
	// Add pre and post save delegates.
	//FEditorDelegates::PreSaveWorld.AddUObject(this, &UHoudiniTestComponent::OnPreSaveWorld);
	//FEditorDelegates::PostSaveWorld.AddUObject(this, &UHoudiniTestComponent::OnPostSaveWorld);

	// Add begin and end delegates for play-in-editor.
	//FEditorDelegates::BeginPIE.AddUObject(this, &UHoudiniTestComponent::OnPIEEventBegin);
	//FEditorDelegates::EndPIE.AddUObject(this, &UHoudiniTestComponent::OnPIEEventEnd);
}


void
UHoudiniTestComponent::UnsubscribeEditorDelegates()
{
	// Remove pre and post save delegates.
	//FEditorDelegates::PreSaveWorld.RemoveUObject(this, &UHoudiniTestComponent::OnPreSaveWorld);
	//FEditorDelegates::PostSaveWorld.RemoveUObject(this, &UHoudiniTestComponent::OnPostSaveWorld);

	// Remove begin and end delegates for play-in-editor.
	//FEditorDelegates::BeginPIE.RemoveUObject(this, &UHoudiniTestComponent::OnPIEEventBegin);
	//FEditorDelegates::EndPIE.RemoveUObject(this, &UHoudiniTestComponent::OnPIEEventEnd);
}


void
UHoudiniTestComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(Before),     C" );

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.MemberProperty->GetName() == TEXT("Value"))
	{
		RememberedValue = Value + 5;
	}

	HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(After),      C" );
}


void
UHoudiniTestComponent::OnRegister()
{
	Super::OnRegister();
	HOUDINI_TEST_LOG_MESSAGE( "  OnRegister,                         C" );
}


void
UHoudiniTestComponent::OnUnregister()
{
	Super::OnUnregister();
	HOUDINI_TEST_LOG_MESSAGE( "  OnUnregister,                       C" );
}


void
UHoudiniTestComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();
	HOUDINI_TEST_LOG_MESSAGE( "  OnComponentCreated,                 C" );
}


FName
UHoudiniTestComponent::GetComponentInstanceDataType() const
{
	// Called before we throw away components during RerunConstructionScripts, to cache any data we wish to persist across that operation.
	//HOUDINI_LOG_MESSAGE(TEXT("Requesting data type for caching, Component = 0x%x, HoudiniAsset = 0x%x"), this, HoudiniAsset);

	//return Super::GetComponentInstanceDataType();
	return FName(TEXT("HoudiniTestComponent"));
}


TSharedPtr<class FComponentInstanceDataBase>
UHoudiniTestComponent::GetComponentInstanceData() const
{
	HOUDINI_TEST_LOG_MESSAGE( "  GetComponentInstanceData,           C" );

	TSharedPtr<FHoudiniTestComponentInstanceData> InstanceData = MakeShareable(new FHoudiniTestComponentInstanceData(this));
	InstanceData->Value = Value;
	return InstanceData;
}


void
UHoudiniTestComponent::ApplyComponentInstanceData(TSharedPtr<class FComponentInstanceDataBase> ComponentInstanceData)
{
	HOUDINI_TEST_LOG_MESSAGE( "  ApplyComponentInstanceData(Before), C" );

	Super::ApplyComponentInstanceData(ComponentInstanceData);

	check(ComponentInstanceData.IsValid());
	TSharedPtr<FHoudiniTestComponentInstanceData> InstanceData =
		StaticCastSharedPtr<FHoudiniTestComponentInstanceData>(ComponentInstanceData);

	Value = InstanceData->Value;

	SubscribeEditorDelegates();

	HOUDINI_TEST_LOG_MESSAGE( "  ApplyComponentInstanceData(After),  C" );
}


void
UHoudiniTestComponent::OnPreSaveWorld(uint32 SaveFlags, class UWorld* World)
{
	HOUDINI_TEST_LOG_MESSAGE( "  OnPreSaveWorld,                     C" );
}


void
UHoudiniTestComponent::OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess)
{
	HOUDINI_TEST_LOG_MESSAGE( "  OnPostSaveWorld,                    C" );
}


void
UHoudiniTestComponent::OnPIEEventBegin(const bool bIsSimulating)
{
	HOUDINI_TEST_LOG_MESSAGE( "  OnPIEEventBegin,                    C" );
}


void
UHoudiniTestComponent::OnPIEEventEnd(const bool bIsSimulating)
{
	HOUDINI_TEST_LOG_MESSAGE( "  OnPIEEventEnd,                      C" );
}


void
UHoudiniTestComponent::PreSave()
{
	Super::PreSave();
	HOUDINI_TEST_LOG_MESSAGE( "  PreSave,                            C" );
}


void
UHoudiniTestComponent::PostLoad()
{
	Super::PostLoad();
	SubscribeEditorDelegates();
	HOUDINI_TEST_LOG_MESSAGE( "  PostLoad,                           C" );
}


void
UHoudiniTestComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsSaving())
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Saving),                  C" );
	}
	else if(Ar.IsLoading())
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - Before),        C" );
	}

	Ar << RememberedValue;

	/*if(Ar.IsLoading())
	{
		if(Value > 0)
		{
			RememberedValue = Value + 5;
		}
	}*/

	if(Ar.IsLoading())
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - After),         C" );
	}
}

#undef HOUDINI_TEST_LOG_MESSAGE
