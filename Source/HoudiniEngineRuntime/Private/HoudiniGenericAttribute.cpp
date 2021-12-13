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

#include "HoudiniGenericAttribute.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"

#include "Engine/StaticMesh.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Landscape.h"

#include "PhysicsEngine/BodySetup.h"
#include "EditorFramework/AssetImportData.h"
#include "AI/Navigation/NavCollisionBase.h"

FHoudiniGenericAttributeChangedProperty::FHoudiniGenericAttributeChangedProperty()
	: Object()
	, Property(nullptr)
{
	
}

FHoudiniGenericAttributeChangedProperty::FHoudiniGenericAttributeChangedProperty(const FHoudiniGenericAttributeChangedProperty& InOther)
{
	*this = InOther;
}

FHoudiniGenericAttributeChangedProperty::FHoudiniGenericAttributeChangedProperty(UObject* InObject, FEditPropertyChain& InPropertyChain, FProperty* InProperty)
	: Object(InObject)
	, Property(InProperty)
{
	PropertyChain.Empty();
	for (const auto& Entry : InPropertyChain)
	{
		PropertyChain.AddTail(Entry);
	}
	if (InPropertyChain.GetActiveNode())
		PropertyChain.SetActivePropertyNode(InPropertyChain.GetActiveNode()->GetValue());
	if (InPropertyChain.GetActiveMemberNode())
		PropertyChain.SetActivePropertyNode(InPropertyChain.GetActiveMemberNode()->GetValue());
}

void
FHoudiniGenericAttributeChangedProperty::operator=(const FHoudiniGenericAttributeChangedProperty& InOther)
{
	Object = InOther.Object;
	Property = InOther.Property;

	PropertyChain.Empty();
	for (const auto& Entry : InOther.PropertyChain)
	{
		PropertyChain.AddTail(Entry);
	}
	if (InOther.PropertyChain.GetActiveNode())
		PropertyChain.SetActivePropertyNode(InOther.PropertyChain.GetActiveNode()->GetValue());
	if (InOther.PropertyChain.GetActiveMemberNode())
		PropertyChain.SetActivePropertyNode(InOther.PropertyChain.GetActiveMemberNode()->GetValue());
}

double
FHoudiniGenericAttribute::GetDoubleValue(int32 index) const
{
	if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return DoubleValues[index];
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return (double)IntValues[index];
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return FCString::Atod(*StringValues[index]);
	}

	return 0.0f;
}

void
FHoudiniGenericAttribute::GetDoubleTuple(TArray<double>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetDoubleValue(index * AttributeTupleSize + n);
}

int64
FHoudiniGenericAttribute::GetIntValue(int32 index) const
{
	if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return IntValues[index];
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return (int64)DoubleValues[index];
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return FCString::Atoi64(*StringValues[index]);
	}

	return 0;
}

void
FHoudiniGenericAttribute::GetIntTuple(TArray<int64>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetIntValue(index * AttributeTupleSize + n);
}

FString
FHoudiniGenericAttribute::GetStringValue(int32 index) const
{
	if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return StringValues[index];
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return FString::FromInt((int32)IntValues[index]);
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return FString::SanitizeFloat(DoubleValues[index]);
	}

	return FString();
}

void
FHoudiniGenericAttribute::GetStringTuple(TArray<FString>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetStringValue(index * AttributeTupleSize + n);
}

bool
FHoudiniGenericAttribute::GetBoolValue(int32 index) const
{
	if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return DoubleValues[index] == 0.0 ? false : true;
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return IntValues[index] == 0 ? false : true;
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return StringValues[index].Equals(TEXT("true"), ESearchCase::IgnoreCase) ? true : false;
	}

	return false;
}

void
FHoudiniGenericAttribute::GetBoolTuple(TArray<bool>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetBoolValue(index * AttributeTupleSize + n);
}

void*
FHoudiniGenericAttribute::GetData()
{
	if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.Num() > 0)
			return StringValues.GetData();
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.Num() > 0)
			return IntValues.GetData();
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.Num() > 0)
			return DoubleValues.GetData();
	}

	return nullptr;
}

void
FHoudiniGenericAttribute::SetDoubleValue(double InValue, int32 index)
{
	if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (!DoubleValues.IsValidIndex(index))
			DoubleValues.SetNum(index + 1);
		DoubleValues[index] = InValue;
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (!IntValues.IsValidIndex(index))
			IntValues.SetNum(index + 1);
		IntValues[index] = InValue;
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (!StringValues.IsValidIndex(index))
			StringValues.SetNum(index + 1);
		StringValues[index] = FString::SanitizeFloat(InValue);
	}
}

void
FHoudiniGenericAttribute::SetDoubleTuple(const TArray<double>& InTupleValues, int32 index)
{
	if (!InTupleValues.IsValidIndex(AttributeTupleSize - 1))
		return;

	for (int32 n = 0; n < AttributeTupleSize; n++)
		SetDoubleValue(InTupleValues[n], index * AttributeTupleSize + n);
}

void
FHoudiniGenericAttribute::SetIntValue(int64 InValue, int32 index)
{
	if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (!IntValues.IsValidIndex(index))
			IntValues.SetNum(index + 1);
		IntValues[index] = InValue;
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (!DoubleValues.IsValidIndex(index))
			DoubleValues.SetNum(index + 1);
		DoubleValues[index] = InValue;
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (!StringValues.IsValidIndex(index))
			StringValues.SetNum(index + 1);
		StringValues[index] = FString::Printf(TEXT("%lld"), InValue);
	}
}

void
FHoudiniGenericAttribute::SetIntTuple(const TArray<int64>& InTupleValues, int32 index)
{
	if (!InTupleValues.IsValidIndex(AttributeTupleSize - 1))
		return;

	for (int32 n = 0; n < AttributeTupleSize; n++)
		SetIntValue(InTupleValues[n], index * AttributeTupleSize + n);
}

void
FHoudiniGenericAttribute::SetStringValue(const FString& InValue, int32 index)
{
	if (AttributeType == EAttribStorageType::STRING)
	{
		if (!StringValues.IsValidIndex(index))
			StringValues.SetNum(index + 1);
		StringValues[index] = InValue;
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (!IntValues.IsValidIndex(index))
			IntValues.SetNum(index + 1);
		IntValues[index] = FCString::Atoi64(*InValue);
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (!DoubleValues.IsValidIndex(index))
			DoubleValues.SetNum(index + 1);
		DoubleValues[index] = FCString::Atod(*InValue);
	}
}

void
FHoudiniGenericAttribute::SetStringTuple(const TArray<FString>& InTupleValues, int32 index)
{
	if (!InTupleValues.IsValidIndex(AttributeTupleSize - 1))
		return;

	for (int32 n = 0; n < AttributeTupleSize; n++)
		SetStringValue(InTupleValues[n], index * AttributeTupleSize + n);
}

void
FHoudiniGenericAttribute::SetBoolValue(bool InValue, int32 index)
{
	if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (!DoubleValues.IsValidIndex(index))
			DoubleValues.SetNum(index + 1);
		DoubleValues[index] = InValue ? 1.0 : 0.0;
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (!IntValues.IsValidIndex(index))
			IntValues.SetNum(index + 1);
		IntValues[index] = InValue ? 1 : 0;
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (!StringValues.IsValidIndex(index))
			StringValues.SetNum(index + 1);
		StringValues[index] = InValue ? "true" : "false";
	}
}

void
FHoudiniGenericAttribute::SetBoolTuple(const TArray<bool>& InTupleValues, int32 index)
{
	if (!InTupleValues.IsValidIndex(AttributeTupleSize - 1))
		return;

	for (int32 n = 0; n < AttributeTupleSize; n++)
		SetBoolValue(InTupleValues[n], index * AttributeTupleSize + n);
}

bool
FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(
	UObject* InObject, const FHoudiniGenericAttribute& InPropertyAttribute, const int32& AtIndex,
	const bool bInDeferPostPropertyChangedEvents, TArray<FHoudiniGenericAttributeChangedProperty>* OutChangedProperties,
	const FFindPropertyFunctionType& InFindPropertyFunction)
{
	if (!IsValid(InObject))
		return false;

	// Get the Property name
	const FString& PropertyName = InPropertyAttribute.AttributeName;
	if (PropertyName.IsEmpty())
		return false;

	// Some Properties need to be handle and modified manually...
	if (PropertyName.Equals("CollisionProfileName", ESearchCase::IgnoreCase))
	{
		UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(InObject);
		if (IsValid(PC))
		{
			FString StringValue = InPropertyAttribute.GetStringValue(AtIndex);
			FName Value = FName(*StringValue);
			PC->SetCollisionProfileName(Value);

			// Patch the StaticMeshGenerationProperties on the HAC
			UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InObject);
			if (IsValid(HAC))
			{
				HAC->StaticMeshGenerationProperties.DefaultBodyInstance.SetCollisionProfileName(Value);
			}

			return true;
		}

		// If this is a UStaticMesh, set the default collision profile name on the static mesh.
		UStaticMesh* SM = Cast<UStaticMesh>(InObject);
		if (IsValid(SM))
		{
			FString StringValue = InPropertyAttribute.GetStringValue(AtIndex);
			FName Value = FName(*StringValue);
			SM->GetBodySetup()->DefaultInstance.SetCollisionProfileName(Value);

			return true;
		}
		
		return false;
	}

	if (PropertyName.Equals("CollisionEnabled", ESearchCase::IgnoreCase))
	{
		UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(InObject);
		if (IsValid(PC))
		{
			FString StringValue = InPropertyAttribute.GetStringValue(AtIndex);
			if (StringValue.Equals("NoCollision", ESearchCase::IgnoreCase))
			{
				PC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				return true;
			}
			else if (StringValue.Equals("QueryOnly", ESearchCase::IgnoreCase))
			{
				PC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				return true;
			}
			else if (StringValue.Equals("PhysicsOnly", ESearchCase::IgnoreCase))
			{
				PC->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
				return true;
			}
			else if (StringValue.Equals("QueryAndPhysics", ESearchCase::IgnoreCase))
			{
				PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				return true;
			}
			return false;
		}
	}

	// Specialize CastShadow to avoid paying the cost of finding property + calling Property change twice
	if (PropertyName.Equals("CastShadow", ESearchCase::IgnoreCase))
	{
		UPrimitiveComponent* Component = Cast< UPrimitiveComponent >(InObject);
		if (IsValid(Component))
		{
			bool Value = InPropertyAttribute.GetBoolValue(AtIndex);
			Component->SetCastShadow(Value);
			return true;
		}
		return false;
	}

	// Handle Component Tags manually here
	if (PropertyName.Contains("Tags"))
	{
		UActorComponent* AC = Cast< UActorComponent >(InObject);
		if (IsValid(AC))
		{
			FName NameAttr = FName(*InPropertyAttribute.GetStringValue(AtIndex));
			if (!AC->ComponentTags.Contains(NameAttr))
				AC->ComponentTags.Add(NameAttr);
			/*
			for (int nIdx = 0; nIdx < InPropertyAttribute.AttributeCount; nIdx++)
			{
				FName NameAttr = FName(*InPropertyAttribute.GetStringValue(nIdx));
				if (!AC->ComponentTags.Contains(NameAttr))
					AC->ComponentTags.Add(NameAttr);
			}
			*/
			return true;
		}
		return false;
	}
#if WITH_EDITOR
	// Handle landscape edit layers toggling
	if (PropertyName.Equals("EnableEditLayers", ESearchCase::IgnoreCase) 
		|| PropertyName.Equals("bCanHaveLayersContent", ESearchCase::IgnoreCase))
	{
		ALandscape* Landscape = Cast<ALandscape>(InObject);
		if (IsValid(Landscape))
		{
			if(InPropertyAttribute.GetBoolValue(AtIndex) != Landscape->CanHaveLayersContent())
				Landscape->ToggleCanHaveLayersContent();

			return true;
		}

		return false;
	}
#endif

	// Try to find the corresponding UProperty
	void* OutContainer = nullptr; 
	FProperty* FoundProperty = nullptr;
	UObject* FoundPropertyObject = nullptr;
	FEditPropertyChain FoundPropertyChain;

	if (InFindPropertyFunction)
	{
		bool bOutSkipDefaultIfPropertyNotFound = false;
		const bool bFoundProperty = InFindPropertyFunction(InObject, PropertyName, bOutSkipDefaultIfPropertyNotFound, FoundPropertyChain, FoundProperty, FoundPropertyObject, OutContainer);
		if (!bFoundProperty && bOutSkipDefaultIfPropertyNotFound)
			return false;
	}
// #if WITH_EDITOR
// 	// Try to match to source model properties when possible
// 	if (UStaticMesh* SM = Cast<UStaticMesh>(InObject))
// 	{
// 		if (IsValid(SM) && SM->GetNumSourceModels() > AtIndex)
// 		{
// 			bool bFoundProperty = false;
// 			TryToFindProperty(&SM->GetSourceModel(AtIndex), SM->GetSourceModel(AtIndex).StaticStruct(), PropertyName, FoundProperty, bFoundProperty, OutContainer);
// 			if (bFoundProperty)
// 			{
// 				FoundPropertyObject = InObject;
// 			}
// 		}
// 	}
// #endif

	if (!FoundProperty && !FindPropertyOnObject(InObject, PropertyName, FoundPropertyChain, FoundProperty, FoundPropertyObject, OutContainer))
		return false;

	// Set the member and active properties on the chain
	if (FoundPropertyChain.Num() > 0)
	{
		if (FoundPropertyChain.GetTail())
			FoundPropertyChain.SetActivePropertyNode(FoundPropertyChain.GetTail()->GetValue());
		if (FoundPropertyChain.GetHead())
			FoundPropertyChain.SetActiveMemberPropertyNode(FoundPropertyChain.GetHead()->GetValue());
	}
	
	// Modify the Property we found
	if (!ModifyPropertyValueOnObject(FoundPropertyObject, InPropertyAttribute, FoundPropertyChain, FoundProperty, OutContainer, AtIndex, bInDeferPostPropertyChangedEvents, OutChangedProperties))
		return false;

	return true;
}


bool
FHoudiniGenericAttribute::FindPropertyOnObject(
	UObject* InObject,
	const FString& InPropertyName,
	FEditPropertyChain& InPropertyChain,
	FProperty*& OutFoundProperty,
	UObject*& OutFoundPropertyObject,
	void*& OutContainer)
{
#if WITH_EDITOR
	if (!IsValid(InObject))
		return false;

	if (InPropertyName.IsEmpty())
		return false;

	UClass* ObjectClass = InObject->GetClass();
	if (!IsValid(ObjectClass))
		return false;

	// Set the result pointer to null
	OutContainer = nullptr;
	OutFoundProperty = nullptr;
	OutFoundPropertyObject = InObject;

	bool bPropertyHasBeenFound = false;
	FHoudiniGenericAttribute::TryToFindProperty(
		InObject,
		ObjectClass,
		InPropertyName,
		InPropertyChain,
		OutFoundProperty,
		bPropertyHasBeenFound,
		OutContainer);

	/*
	// TODO: Parsing needs to be made recursively!
	// Iterate manually on the properties, in order to handle StructProperties correctly
	for (TFieldIterator<FProperty> PropIt(ObjectClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* CurrentProperty = *PropIt;
		if (!CurrentProperty)
			continue;

		FString DisplayName = CurrentProperty->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
		FString Name = CurrentProperty->GetName();

		// If the property name contains the uprop attribute name, we have a candidate
		if (Name.Contains(InPropertyName) || DisplayName.Contains(InPropertyName))
		{
			OutFoundProperty = CurrentProperty;

			// If it's an equality, we dont need to keep searching
			if ((Name == InPropertyName) || (DisplayName == InPropertyName))
			{
				bPropertyHasBeenFound = true;
				break;
			}
		}

		// StructProperty need to be a nested struct
		//if (UStructProperty* StructProperty = Cast< UStructProperty >(CurrentProperty))
		//	bPropertyHasBeenFound = TryToFindInStructProperty(InObject, InPropertyName, StructProperty, OutFoundProperty, OutStructContainer);
		//else if (UArrayProperty* ArrayProperty = Cast< UArrayProperty >(CurrentProperty))
		//	bPropertyHasBeenFound = TryToFindInArrayProperty(InObject, InPropertyName, ArrayProperty, OutFoundProperty, OutStructContainer);

		// Handle StructProperty
		FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty);
		if (StructProperty)
		{
			// Walk the structs' properties and try to find the one we're looking for
			UScriptStruct* Struct = StructProperty->Struct;
			if (!IsValid(Struct))
				continue;

			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* Property = *It;
				if (!Property)
					continue;

				DisplayName = Property->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
				Name = Property->GetName();

				// If the property name contains the uprop attribute name, we have a candidate
				if (Name.Contains(InPropertyName) || DisplayName.Contains(InPropertyName))
				{
					// We found the property in the struct property, we need to keep the ValuePtr in the object
					// of the structProp in order to be able to access the property value afterwards...
					OutFoundProperty = Property;
					OutStructContainer = StructProperty->ContainerPtrToValuePtr< void >(InObject, 0);

					// If it's an equality, we dont need to keep searching
					if ((Name == InPropertyName) || (DisplayName == InPropertyName))
					{
						bPropertyHasBeenFound = true;
						break;
					}
				}
			}
		}

		if (bPropertyHasBeenFound)
			break;
	}

	if (bPropertyHasBeenFound)
		return true;
	*/

	// Try with FindField??
	if (!OutFoundProperty)
		OutFoundProperty = FindFProperty<FProperty>(ObjectClass, *InPropertyName);

	// Try with FindPropertyByName ??
	if (!OutFoundProperty)
		OutFoundProperty = ObjectClass->FindPropertyByName(*InPropertyName);

	// We found the Property we were looking for
	if (OutFoundProperty)
		return true;

	// Handle common properties nested in classes
	// Static Meshes
	UStaticMesh* SM = Cast<UStaticMesh>(InObject);
	if (IsValid(SM))
	{
		if (SM->GetBodySetup() && FindPropertyOnObject(
			SM->GetBodySetup(), InPropertyName, InPropertyChain, OutFoundProperty, OutFoundPropertyObject, OutContainer))
		{
			return true;
		}

		if (SM->AssetImportData && FindPropertyOnObject(
			SM->AssetImportData, InPropertyName, InPropertyChain, OutFoundProperty, OutFoundPropertyObject, OutContainer))
		{
			return true;
		}

		if (SM->GetNavCollision() && FindPropertyOnObject(
			SM->GetNavCollision(), InPropertyName, InPropertyChain, OutFoundProperty, OutFoundPropertyObject, OutContainer))
		{
			return true;
		}
	}

	// For Actors, parse their components
	AActor* Actor = Cast<AActor>(InObject);
	if (IsValid(Actor))
	{
		TArray<USceneComponent*> AllComponents;
		Actor->GetComponents<USceneComponent>(AllComponents, true);

		int32 CompIdx = 0;
		for (USceneComponent * SceneComponent : AllComponents)
		{
			if (!IsValid(SceneComponent))
				continue;

			if (FindPropertyOnObject(
				SceneComponent, InPropertyName, InPropertyChain, OutFoundProperty, OutFoundPropertyObject, OutContainer))
			{
				return true;
			}
		}
	}

	// We found the Property we were looking for
	if (OutFoundProperty)
		return true;

#endif
	return false;
}


bool
FHoudiniGenericAttribute::TryToFindProperty(
	void* InContainer,
	UStruct* InStruct,
	const FString& InPropertyName,
	FEditPropertyChain& InPropertyChain,
	FProperty*& OutFoundProperty,
	bool& bOutPropertyHasBeenFound,
	void*& OutContainer)
{
#if WITH_EDITOR
	if (!InContainer)
		return false;
	
	if (!IsValid(InStruct))
		return false;

	if (InPropertyName.IsEmpty())
		return false;

	// Iterate manually on the properties, in order to handle StructProperties correctly
	for (TFieldIterator<FProperty> PropIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* CurrentProperty = *PropIt;
		if (!CurrentProperty)
			continue;

		FString DisplayName = CurrentProperty->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
		FString Name = CurrentProperty->GetName();

		// If the property name contains the uprop attribute name, we have a candidate
		if (Name.Contains(InPropertyName) || DisplayName.Contains(InPropertyName))
		{
			OutFoundProperty = CurrentProperty;
			OutContainer = InContainer;

			// If it's an equality, we dont need to keep searching anymore
			if ((Name == InPropertyName) || (DisplayName == InPropertyName))
			{
				bOutPropertyHasBeenFound = true;
				InPropertyChain.AddTail(OutFoundProperty);
				break;
			}
		}

		// Do a recursive parsing for StructProperties
		FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty);
		if (StructProperty)
		{
			// Walk the structs' properties and try to find the one we're looking for
			UScriptStruct* Struct = StructProperty->Struct;
			if (!IsValid(Struct))
				continue;

			InPropertyChain.AddTail(StructProperty);
			TryToFindProperty(
				StructProperty->ContainerPtrToValuePtr<void>(InContainer, 0),
				Struct,
				InPropertyName,
				InPropertyChain,
				OutFoundProperty,
				bOutPropertyHasBeenFound,
				OutContainer);
			if (!bOutPropertyHasBeenFound)
				InPropertyChain.RemoveNode(InPropertyChain.GetTail());
		}

		if (bOutPropertyHasBeenFound)
			break;
	}

	if (bOutPropertyHasBeenFound)
		return true;

	// We found the Property we were looking for
	if (OutFoundProperty)
		return true;

#endif
	return false;
}


bool
FHoudiniGenericAttribute::HandlePostEditChangeProperty(UObject* InObject, FEditPropertyChain& InPropertyChain, FProperty* InProperty)
{
#if WITH_EDITOR
	if (InPropertyChain.Num() == 0)
	{
		FPropertyChangedEvent Evt(InProperty);
		InObject->PostEditChangeProperty(Evt);
		AActor* InOwner = Cast<AActor>(InObject);
		if (IsValid(InOwner))
		{
			// If we are setting properties on an Actor component, we want to notify the
			// actor of the changes too since the property change might be handled in the actor's
			// PostEditChange callbacks (one such an example occurs when changing the material for a decal actor).
			InOwner->PostEditChangeProperty(Evt);
		}

		return true;
	}
	else if (InPropertyChain.Num() > 0)
	{
		FPropertyChangedEvent Evt(InPropertyChain.GetActiveNode() ? InPropertyChain.GetActiveNode()->GetValue() : nullptr);
		FPropertyChangedChainEvent ChainEvt(InPropertyChain, Evt);
		InObject->PostEditChangeChainProperty(ChainEvt);
		AActor* InOwner = Cast<AActor>(InObject);
		if (IsValid(InOwner))
		{
			// If we are setting properties on an Actor component, we want to notify the
			// actor of the changes too since the property change might be handled in the actor's
			// PostEditChange callbacks (one such an example occurs when changing the material for a decal actor).
			InOwner->PostEditChangeChainProperty(ChainEvt);
		}

		return true;
	}
	return false;
#endif
	return true;
}

bool
FHoudiniGenericAttribute::ModifyPropertyValueOnObject(
	UObject* InObject,
	const FHoudiniGenericAttribute& InGenericAttribute,
	FEditPropertyChain& InPropertyChain, 
	FProperty* FoundProperty,
	void* InContainer,
	const int32& InAtIndex,
	const bool bInDeferPostPropertyChangedEvents,
	TArray<FHoudiniGenericAttributeChangedProperty>* OutChangedProperties)
{
	if (!IsValid(InObject) || !FoundProperty)
		return false;

	// Determine the container to use (either InContainer if specified, or InObject)
	void* Container = InContainer ? InContainer : InObject;

	// Property class name, used for logging
	const FString PropertyClassName = FoundProperty->GetClass() ? FoundProperty->GetClass()->GetName() : TEXT("Unknown");

	// Initialize using the found property
	FProperty* InnerProperty = FoundProperty;

	AActor* InOwner = Cast<AActor>(InObject->GetOuter());
	bool bPropertyValueChanged = false;
	bool bPostEditChangePropertyCalled = false;

	auto OnPrePropertyChanged = [InObject, &InPropertyChain](FProperty* InProperty)
	{
		if (!InProperty)
			return;
		
#if WITH_EDITOR
		if (InPropertyChain.Num() == 0)
		{
			InObject->PreEditChange(InProperty);
		}
		else if (InPropertyChain.Num() > 0)
		{
			InObject->PreEditChange(InPropertyChain);
		}
#endif
	};

	auto OnPropertyChanged = [InObject, &InPropertyChain, InOwner, bInDeferPostPropertyChangedEvents, OutChangedProperties, &InGenericAttribute, &bPostEditChangePropertyCalled, &bPropertyValueChanged](FProperty* InProperty)
	{
		bPropertyValueChanged = true;
		
		if (!InProperty)
			return;
		
#if WITH_EDITOR
		if (InPropertyChain.Num() == 0)
		{
			HOUDINI_LOG_MESSAGE(
				TEXT("Modified UProperty %s (searched for %s) on %s named %s"),
				*InProperty->GetName(),
				*InGenericAttribute.AttributeName,
				*(InObject->GetClass() ? InObject->GetClass()->GetName() : TEXT("Object")),
				*(InObject->GetName()));

			if (bInDeferPostPropertyChangedEvents)
			{
				if (OutChangedProperties)
				{
					FHoudiniGenericAttributeChangedProperty ChangeData;
					ChangeData.PropertyChain.AddTail(InProperty);
					ChangeData.Property = InProperty;
					ChangeData.Object = InObject;
					OutChangedProperties->Add(ChangeData);
				}
			}
			else
			{
				bPostEditChangePropertyCalled = HandlePostEditChangeProperty(InObject, InPropertyChain, InProperty);
			}
		}
		else if (InPropertyChain.Num() > 0)
		{
			HOUDINI_LOG_MESSAGE(
				TEXT("Modified UProperty %s (searched for %s) on %s named %s"),
				*FString::JoinBy(InPropertyChain, TEXT("."), [](FProperty const* const Property)
				{
					if (!Property)
						return FString();
					return Property->GetName();
				}),
				*InGenericAttribute.AttributeName,
				*(InObject->GetClass() ? InObject->GetClass()->GetName() : TEXT("Object")),
				*(InObject->GetName()));

			if (bInDeferPostPropertyChangedEvents)
			{
				if (OutChangedProperties)
				{
					FHoudiniGenericAttributeChangedProperty ChangeData;
					for (FProperty* Property : InPropertyChain)
						ChangeData.PropertyChain.AddTail(Property);
					
					if (InPropertyChain.GetActiveNode())
						ChangeData.PropertyChain.SetActivePropertyNode(InPropertyChain.GetActiveNode()->GetValue());
					if (InPropertyChain.GetActiveMemberNode())
						ChangeData.PropertyChain.SetActiveMemberPropertyNode(InPropertyChain.GetActiveMemberNode()->GetValue());

					ChangeData.Property = InProperty;
					ChangeData.Object = InObject;
					OutChangedProperties->Add(ChangeData);
				}
			}
			else
			{
				bPostEditChangePropertyCalled = HandlePostEditChangeProperty(InObject, InPropertyChain, InProperty);
			}
		}
#endif
	};

	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FoundProperty);
	TSharedPtr<FScriptArrayHelper_InContainer> ArrayHelper;
	if (ArrayProperty)
	{
		InnerProperty = ArrayProperty->Inner;
		ArrayHelper = MakeShareable<FScriptArrayHelper_InContainer>(new FScriptArrayHelper_InContainer(ArrayProperty, Container));
	}

	// TODO: implement support for array attributes received from Houdini
	
	// Get the "proper" AtIndex in the flat array by using the attribute tuple size
	// TODO: fix the issue when changing array of tuple properties!
	const int32 TupleSize = InGenericAttribute.AttributeTupleSize;
	int32 AtIndex = InAtIndex * TupleSize;
	FFieldClass* PropertyClass = InnerProperty->GetClass();
	if (PropertyClass->IsChildOf(FNumericProperty::StaticClass()) || PropertyClass->IsChildOf(FBoolProperty::StaticClass()) ||
		PropertyClass->IsChildOf(FStrProperty::StaticClass()) || PropertyClass->IsChildOf(FNameProperty::StaticClass()))
	{
		// Supported non-struct properties

		// If the attribute from Houdini has a tuple size > 1, we support setting it on arrays on the unreal side
		// For example: a 3 float from Houdini can be set as a TArray<float> in Unreal.
		
		// If this is an ArrayProperty, ensure that it is at least large enough for our tuple
		// TODO: should we just set this to exactly our tuple size?
		if (ArrayHelper.IsValid())
			ArrayHelper->ExpandForIndex(TupleSize - 1);

		for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
		{
			void* ValuePtr = nullptr;
			if (ArrayHelper.IsValid())
			{
				ValuePtr = ArrayHelper->GetRawPtr(TupleIndex);
			}
			else
			{
				// If this is not an ArrayProperty, it could be a fixed (standard C/C++ array), check the ArrayDim
				// on the property to determine if our TupleIndex is in range, if not, give up, we cannot set any more
				// of our tuple indices on this property.
				if (TupleIndex >= InnerProperty->ArrayDim)
					break;

				ValuePtr = InnerProperty->ContainerPtrToValuePtr<void*>(Container, TupleIndex);
			}

			if (ValuePtr)
			{
				// Handle each property type that we support
				if (PropertyClass->IsChildOf(FNumericProperty::StaticClass()))
				{
					// Numeric properties are supported as floats and ints, and can also be set from a received string
					FNumericProperty* const Property = CastField<FNumericProperty>(InnerProperty);
					if (InGenericAttribute.AttributeType == EAttribStorageType::STRING)
					{
						const FString NewValue = InGenericAttribute.GetStringValue(AtIndex + TupleIndex);
						const FString CurrentValue = Property->GetNumericPropertyValueToString(ValuePtr);
						if (NewValue != CurrentValue)
						{
							OnPrePropertyChanged(Property);
							Property->SetNumericPropertyValueFromString(ValuePtr, *NewValue);
							OnPropertyChanged(Property);
						}
					}
					else if (Property->IsFloatingPoint())
					{
						const double NewValue = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex);
						const double CurrentValue = Property->GetFloatingPointPropertyValue(ValuePtr);
						if (NewValue != CurrentValue)
						{
							OnPrePropertyChanged(Property);
							Property->SetFloatingPointPropertyValue(ValuePtr, NewValue);
							OnPropertyChanged(Property);
						}
					}
					else if (Property->IsInteger())
					{
						const int64 NewValue = InGenericAttribute.GetIntValue(AtIndex + TupleIndex);
						const int64 CurrentValue = Property->GetSignedIntPropertyValue(ValuePtr);
						if (NewValue != CurrentValue)
						{
							OnPrePropertyChanged(Property);
							Property->SetIntPropertyValue(ValuePtr, NewValue);
							OnPropertyChanged(Property);
						}
					}
					else
					{
						HOUDINI_LOG_WARNING(TEXT("Unsupported numeric property for %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
						return false;
					}
				}
				else if (PropertyClass->IsChildOf(FBoolProperty::StaticClass()))
				{
					FBoolProperty* const Property = CastField<FBoolProperty>(InnerProperty);
					const bool NewValue = InGenericAttribute.GetBoolValue(AtIndex + TupleIndex);
					const bool CurrentValue = Property->GetPropertyValue(ValuePtr);
					if (NewValue != CurrentValue)
					{
						OnPrePropertyChanged(Property);
						Property->SetPropertyValue(ValuePtr, NewValue);
						OnPropertyChanged(Property);
					}
				}
				else if (PropertyClass->IsChildOf(FStrProperty::StaticClass()))
				{
					FStrProperty* const Property = CastField<FStrProperty>(InnerProperty);
					const FString NewValue = InGenericAttribute.GetStringValue(AtIndex + TupleIndex);
					const FString CurrentValue = Property->GetPropertyValue(ValuePtr);
					if (NewValue != CurrentValue)
					{
						OnPrePropertyChanged(Property);
						Property->SetPropertyValue(ValuePtr, NewValue);
						OnPropertyChanged(Property);
					}
				}
				else if (PropertyClass->IsChildOf(FNameProperty::StaticClass()))
				{
					FNameProperty* const Property = CastField<FNameProperty>(InnerProperty);
					const FName NewValue = FName(*InGenericAttribute.GetStringValue(AtIndex + TupleIndex));
					const FName CurrentValue = Property->GetPropertyValue(ValuePtr);
					if (NewValue != CurrentValue)
					{
						OnPrePropertyChanged(Property);
						Property->SetPropertyValue(ValuePtr, NewValue);
						OnPropertyChanged(Property);
					}
				}
				else
				{
					HOUDINI_LOG_WARNING(TEXT("Unsupported property for %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
					return false;
				}
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Could net get a valid value ptr for uproperty %s (Class %s), tuple index %i"), *InGenericAttribute.AttributeName, *PropertyClassName, TupleIndex);
				if (TupleIndex == 0)
					return false;
			}
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty))
	{
		// struct properties

		// If we receive an attribute with tuple size > 1 and the target is an Unreal struct property, then we set
		// as many of the values as we can in the struct. For example: a 4-float received from Houdini where the
		// target is an FVector, the FVector.X, Y and Z would be set from the 4-float indices 0-2. Index 3 from the
		// 4-float would then be ignored.
		
		const int32 TupleIndex = 0;
		// If this is an array property, ensure it has enough space
		// TODO: should we just set the array size to 1 for non-arrays and to the array size for arrays (once we support array attributes from Houdini)?
		//		 vs just ensuring there is enough space (and then potentially leaving previous/old data behind?)
		if (ArrayHelper.IsValid())
			ArrayHelper->ExpandForIndex(TupleIndex);

		void* PropertyValue = nullptr;
		if (ArrayHelper.IsValid())
			PropertyValue = ArrayHelper->GetRawPtr(TupleIndex);
		else
			PropertyValue = InnerProperty->ContainerPtrToValuePtr<void>(Container, TupleIndex);

		if (PropertyValue)
		{
			const FName PropertyName = StructProperty->Struct->GetFName();
			if (PropertyName == NAME_Vector)
			{
				// Found a vector property, fill it with up to 3 tuple values
				FVector& Vector = *static_cast<FVector*>(PropertyValue);
				FVector NewVector = FVector::ZeroVector;
				NewVector.X = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 0);
				if (InGenericAttribute.AttributeTupleSize > 1)
					NewVector.Y = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					NewVector.Z = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 2);

				if (NewVector != Vector)
				{
					OnPrePropertyChanged(StructProperty);
					Vector = NewVector;
					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == NAME_Transform)
			{
				// Found a transform property fill it with the attribute tuple values
				FVector Translation;
				Translation.X = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 0);
				if (InGenericAttribute.AttributeTupleSize > 1)
					Translation.Y = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					Translation.Z = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 2);

				FQuat Rotation;
				if (InGenericAttribute.AttributeTupleSize > 3)
					Rotation.W = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 3);
				if (InGenericAttribute.AttributeTupleSize > 4)
					Rotation.X = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 4);
				if (InGenericAttribute.AttributeTupleSize > 5)
					Rotation.Y = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 5);
				if (InGenericAttribute.AttributeTupleSize > 6)
					Rotation.Z = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 6);

				FVector Scale(1, 1, 1);
				if (InGenericAttribute.AttributeTupleSize > 7)
					Scale.X = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 7);
				if (InGenericAttribute.AttributeTupleSize > 8)
					Scale.Y = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 8);
				if (InGenericAttribute.AttributeTupleSize > 9)
					Scale.Z = InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 9);

				FTransform& Transform = *static_cast<FTransform*>(PropertyValue);
				const FTransform NewTransform(Rotation, Translation, Scale);

				if (!NewTransform.Equals(Transform))
				{
					OnPrePropertyChanged(StructProperty);
					Transform = NewTransform;
					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == NAME_Color)
			{
				FColor& Color = *static_cast<FColor*>(PropertyValue);
				FColor NewColor = FColor::Black;
				NewColor.R = (int8)InGenericAttribute.GetIntValue(AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					NewColor.G = (int8)InGenericAttribute.GetIntValue(AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					NewColor.B = (int8)InGenericAttribute.GetIntValue(AtIndex + TupleIndex + 2);
				if (InGenericAttribute.AttributeTupleSize > 3)
					NewColor.A = (int8)InGenericAttribute.GetIntValue(AtIndex + TupleIndex + 3);

				if (NewColor != Color)
				{
					OnPrePropertyChanged(StructProperty);
					Color = NewColor;
					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == NAME_LinearColor)
			{
				FLinearColor& LinearColor = *static_cast<FLinearColor*>(PropertyValue);
				FLinearColor NewLinearColor = FLinearColor::Black;
				NewLinearColor.R = (float)InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					NewLinearColor.G = (float)InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					NewLinearColor.B = (float)InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 2);
				if (InGenericAttribute.AttributeTupleSize > 3)
					NewLinearColor.A = (float)InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 3);

				if (NewLinearColor != LinearColor)
				{
					OnPrePropertyChanged(StructProperty);
					LinearColor = NewLinearColor;
					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == "Int32Interval")
			{
				FInt32Interval& Interval = *static_cast<FInt32Interval*>(PropertyValue);
				FInt32Interval NewInterval;
				NewInterval.Min = (int32)InGenericAttribute.GetIntValue(AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					NewInterval.Max = (int32)InGenericAttribute.GetIntValue(AtIndex + TupleIndex + 1);

				if (NewInterval.Min != Interval.Min || NewInterval.Max != Interval.Max)
				{
					OnPrePropertyChanged(StructProperty);
					Interval = NewInterval;
					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == "FloatInterval")
			{
				FFloatInterval& Interval = *static_cast<FFloatInterval*>(PropertyValue);
				FFloatInterval NewInterval;
				NewInterval.Min = (float)InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					NewInterval.Max = (float)InGenericAttribute.GetDoubleValue(AtIndex + TupleIndex + 1);

				if (NewInterval.Min != Interval.Min || NewInterval.Max != Interval.Max)
				{
					OnPrePropertyChanged(StructProperty);
					Interval = NewInterval;
					OnPropertyChanged(StructProperty);
				}
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("For uproperty %s (Class %s): unsupported struct property type: %s"), *InGenericAttribute.AttributeName, *PropertyClassName, *PropertyName.ToString());
				return false;
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could net get a valid value ptr for uproperty %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
			return false;
		}
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InnerProperty))
	{
		// OBJECT PATH PROPERTY
		const int32 TupleIndex = 0;
		// If this is an array property, ensure it has enough space
		// TODO: should we just set the array size to 1 for non-arrays or to the array size for arrays (once we support array attributes from Houdini)?
		//		 vs just ensuring there is enough space (and then potentially leaving previous/old data behind?)
		if (ArrayHelper.IsValid())
			ArrayHelper->ExpandForIndex(TupleIndex);

		FString Value = InGenericAttribute.GetStringValue(AtIndex + TupleIndex);
		void* ValuePtr = nullptr;
		if (ArrayHelper.IsValid())
			ValuePtr = ArrayHelper->GetRawPtr(TupleIndex);
		else
			ValuePtr = InnerProperty->ContainerPtrToValuePtr<FString>(Container, TupleIndex);
		
		if (ValuePtr)
		{
			// Using TryLoad() over LoadSynchronous() as the later could crash with invalid path
			UObject* ValueObject = nullptr;
			FSoftObjectPath ObjPath(Value);
			if(ObjPath.IsValid())
			{
				ValueObject = ObjPath.TryLoad();
			}

			// Ensure the ObjectProperty class matches the ValueObject that we just loaded
			if (!ValueObject || (ValueObject && ValueObject->IsA(ObjectProperty->PropertyClass)))
			{
				UObject const* const CurrentValue = ObjectProperty->GetObjectPropertyValue(ValuePtr);
				if (CurrentValue != ValueObject)
				{
					OnPrePropertyChanged(ObjectProperty);
					ObjectProperty->SetObjectPropertyValue(ValuePtr, ValueObject);
					OnPropertyChanged(ObjectProperty);
				}
			}
			else
			{
				HOUDINI_LOG_WARNING(
					TEXT("Could net set object property %s: ObjectProperty's object class (%s) does not match referenced object class (%s)!"),
					*InGenericAttribute.AttributeName, *(ObjectProperty->PropertyClass->GetName()), IsValid(ValueObject) ? *(ValueObject->GetClass()->GetName()) : TEXT("NULL"));
				return false;
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could net get a valid value ptr for uproperty %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
			return false;
		}
	}
	else
	{
		// Property was found, but is of an unsupported type
		HOUDINI_LOG_WARNING(TEXT("Unsupported UProperty Class: %s found for uproperty %s"), *PropertyClassName, *InGenericAttribute.AttributeName);
		return false;
	}

	if (bPropertyValueChanged && !bPostEditChangePropertyCalled && !bInDeferPostPropertyChangedEvents)
	{
#if WITH_EDITOR
		InObject->PostEditChange();
		if (InOwner)
		{
			InOwner->PostEditChange();
		}
#endif
	}

	return true;
}

bool
FHoudiniGenericAttribute::GetPropertyValueFromObject(
	UObject* InObject,
	FProperty* InFoundProperty,
	void* InContainer,
	FHoudiniGenericAttribute& InGenericAttribute,
	const int32& InAtIndex)
{
	if (!IsValid(InObject) || !InFoundProperty)
		return false;

	// Determine the container to use (either InContainer if specified, or InObject)
	void* Container = InContainer ? InContainer : InObject;

	// Property class name, used for logging
	const FString PropertyClassName = InFoundProperty->GetClass() ? InFoundProperty->GetClass()->GetName() : TEXT("Unknown");

	// Initialize using the found property
	FProperty* InnerProperty = InFoundProperty;

	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InFoundProperty);
	TSharedPtr<FScriptArrayHelper_InContainer> ArrayHelper;
	if (ArrayProperty)
	{
		InnerProperty = ArrayProperty->Inner;
		ArrayHelper = MakeShareable<FScriptArrayHelper_InContainer>(new FScriptArrayHelper_InContainer(ArrayProperty, Container));
	}

	// TODO: implement support for array attributes received from Houdini
	
	// Get the "proper" AtIndex in the flat array by using the attribute tuple size
	// TODO: fix the issue when changing array of tuple properties!
	const int32 TupleSize = InGenericAttribute.AttributeTupleSize;
	int32 AtIndex = InAtIndex * TupleSize;
	FFieldClass* PropertyClass = InnerProperty->GetClass();
	if (PropertyClass->IsChildOf(FNumericProperty::StaticClass()) || PropertyClass->IsChildOf(FBoolProperty::StaticClass()) ||
		PropertyClass->IsChildOf(FStrProperty::StaticClass()) || PropertyClass->IsChildOf(FNameProperty::StaticClass()))
	{
		// Supported non-struct properties

		// If the attribute from Houdini has a tuple size > 1, we support getting it on arrays on the unreal side
		// For example: a 3 float in Houdini can be set from a TArray<float> in Unreal.
		
		for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
		{
			void* ValuePtr = nullptr;
			if (ArrayHelper.IsValid())
			{
				// Check that we are not out of range
				if (TupleIndex >= ArrayHelper->Num())
					break;
				
				ValuePtr = ArrayHelper->GetRawPtr(TupleIndex);
			}
			else
			{
				// If this is not an ArrayProperty, it could be a fixed (standard C/C++ array), check the ArrayDim
				// on the property to determine if our TupleIndex is in range, if not, give up, we cannot get any more
				// of our tuple indices from this property.
				if (TupleIndex >= InnerProperty->ArrayDim)
					break;

				ValuePtr = InnerProperty->ContainerPtrToValuePtr<void*>(Container, TupleIndex);
			}

			if (ValuePtr)
			{
				// Handle each property type that we support
				if (PropertyClass->IsChildOf(FNumericProperty::StaticClass()))
				{
					// Numeric properties are supported as floats and ints, and can also be set from a received string
					FNumericProperty* const Property = CastField<FNumericProperty>(InnerProperty);
					if (InGenericAttribute.AttributeType == EAttribStorageType::STRING)
					{
						InGenericAttribute.SetStringValue(Property->GetNumericPropertyValueToString(ValuePtr), AtIndex + TupleIndex);
					}
					else if (Property->IsFloatingPoint())
					{
						InGenericAttribute.SetDoubleValue(Property->GetFloatingPointPropertyValue(ValuePtr), AtIndex + TupleIndex);
					}
					else if (Property->IsInteger())
					{
						InGenericAttribute.SetIntValue(Property->GetSignedIntPropertyValue(ValuePtr), AtIndex + TupleIndex);
					}
					else
					{
						HOUDINI_LOG_WARNING(TEXT("Unsupported numeric property for %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
						return false;
					}
				}
				else if (PropertyClass->IsChildOf(FBoolProperty::StaticClass()))
				{
					FBoolProperty* const Property = CastField<FBoolProperty>(InnerProperty);
					InGenericAttribute.SetBoolValue(Property->GetPropertyValue(ValuePtr), AtIndex + TupleIndex);
				}
				else if (PropertyClass->IsChildOf(FStrProperty::StaticClass()))
				{
					FStrProperty* const Property = CastField<FStrProperty>(InnerProperty);
					InGenericAttribute.SetStringValue(Property->GetPropertyValue(ValuePtr), AtIndex + TupleIndex);
				}
				else if (PropertyClass->IsChildOf(FNameProperty::StaticClass()))
				{
					FNameProperty* const Property = CastField<FNameProperty>(InnerProperty);
					InGenericAttribute.SetStringValue(Property->GetPropertyValue(ValuePtr).ToString(), AtIndex + TupleIndex);
				}
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Could net get a valid value ptr for uproperty %s (Class %s), tuple index %i"), *InGenericAttribute.AttributeName, *PropertyClassName, TupleIndex);
				if (TupleIndex == 0)
					return false;
			}
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty))
	{
		// struct properties

		// Set as many as the tuple values as we can from the Unreal struct.
		
		const int32 TupleIndex = 0;

		void* PropertyValue = nullptr;
		if (ArrayHelper.IsValid())
		{
			if (ArrayHelper->IsValidIndex(TupleIndex))
				PropertyValue = ArrayHelper->GetRawPtr(TupleIndex);
		}
		else if (TupleIndex < InnerProperty->ArrayDim)
		{
			PropertyValue = InnerProperty->ContainerPtrToValuePtr<void>(Container, TupleIndex);
		}

		if (PropertyValue)
		{
			const FName PropertyName = StructProperty->Struct->GetFName();
			if (PropertyName == NAME_Vector)
			{
				// Found a vector property, fill it with up to 3 tuple values
				const FVector& Vector = *static_cast<FVector*>(PropertyValue);
				InGenericAttribute.SetDoubleValue(Vector.X, AtIndex + TupleIndex + 0);
				if (InGenericAttribute.AttributeTupleSize > 1)
					InGenericAttribute.SetDoubleValue(Vector.Y, AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					InGenericAttribute.SetDoubleValue(Vector.Z, AtIndex + TupleIndex + 2);
			}
			else if (PropertyName == NAME_Transform)
			{
				// Found a transform property fill it with the attribute tuple values
				const FTransform& Transform = *static_cast<FTransform*>(PropertyValue);
				const FVector Translation = Transform.GetTranslation();
				const FQuat Rotation = Transform.GetRotation();
				const FVector Scale = Transform.GetScale3D();

				InGenericAttribute.SetDoubleValue(Translation.X, AtIndex + TupleIndex + 0);
				if (InGenericAttribute.AttributeTupleSize > 1)
					InGenericAttribute.SetDoubleValue(Translation.Y, AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					InGenericAttribute.SetDoubleValue(Translation.Z, AtIndex + TupleIndex + 2);

				if (InGenericAttribute.AttributeTupleSize > 3)
					InGenericAttribute.SetDoubleValue(Rotation.W, AtIndex + TupleIndex + 3);
				if (InGenericAttribute.AttributeTupleSize > 4)
					InGenericAttribute.SetDoubleValue(Rotation.X, AtIndex + TupleIndex + 4);
				if (InGenericAttribute.AttributeTupleSize > 5)
					InGenericAttribute.SetDoubleValue(Rotation.Y, AtIndex + TupleIndex + 5);
				if (InGenericAttribute.AttributeTupleSize > 6)
					InGenericAttribute.SetDoubleValue(Rotation.Z, AtIndex + TupleIndex + 6);

				if (InGenericAttribute.AttributeTupleSize > 7)
					InGenericAttribute.SetDoubleValue(Scale.X, AtIndex + TupleIndex + 7);
				if (InGenericAttribute.AttributeTupleSize > 8)
					InGenericAttribute.SetDoubleValue(Scale.Y, AtIndex + TupleIndex + 8);
				if (InGenericAttribute.AttributeTupleSize > 9)
					InGenericAttribute.SetDoubleValue(Scale.Z, AtIndex + TupleIndex + 9);
			}
			else if (PropertyName == NAME_Color)
			{
				const FColor& Color = *static_cast<FColor*>(PropertyValue);
				InGenericAttribute.SetIntValue(Color.R, AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					InGenericAttribute.SetIntValue(Color.G, AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					InGenericAttribute.SetIntValue(Color.B, AtIndex + TupleIndex + 2);
				if (InGenericAttribute.AttributeTupleSize > 3)
					InGenericAttribute.SetIntValue(Color.A, AtIndex + TupleIndex + 3);
			}
			else if (PropertyName == NAME_LinearColor)
			{
				const FLinearColor& LinearColor = *static_cast<FLinearColor*>(PropertyValue);
				InGenericAttribute.SetDoubleValue(LinearColor.R, AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					InGenericAttribute.SetDoubleValue(LinearColor.G, AtIndex + TupleIndex + 1);
				if (InGenericAttribute.AttributeTupleSize > 2)
					InGenericAttribute.SetDoubleValue(LinearColor.B, AtIndex + TupleIndex + 2);
				if (InGenericAttribute.AttributeTupleSize > 3)
					InGenericAttribute.SetDoubleValue(LinearColor.A, AtIndex + TupleIndex + 3);
			}
			else if (PropertyName == "Int32Interval")
			{
				const FInt32Interval& Interval = *static_cast<FInt32Interval*>(PropertyValue);
				InGenericAttribute.SetIntValue(Interval.Min, AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					InGenericAttribute.SetIntValue(Interval.Max, AtIndex + TupleIndex + 1);
			}
			else if (PropertyName == "FloatInterval")
			{
				const FFloatInterval& Interval = *static_cast<FFloatInterval*>(PropertyValue);
				InGenericAttribute.SetDoubleValue(Interval.Min, AtIndex + TupleIndex);
				if (InGenericAttribute.AttributeTupleSize > 1)
					InGenericAttribute.SetDoubleValue(Interval.Max, AtIndex + TupleIndex + 1);
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("For uproperty %s (Class %s): unsupported struct property type: %s"), *InGenericAttribute.AttributeName, *PropertyClassName, *PropertyName.ToString());
				return false;
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could net get a valid value ptr for uproperty %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
			return false;
		}
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InnerProperty))
	{
		// OBJECT PATH PROPERTY
		const int32 TupleIndex = 0;

		void* ValuePtr = nullptr;
		if (ArrayHelper.IsValid())
		{
			if (ArrayHelper->IsValidIndex(TupleIndex))
				ValuePtr = ArrayHelper->GetRawPtr(TupleIndex);
		}
		else if (TupleIndex < InnerProperty->ArrayDim)
		{
			ValuePtr = InnerProperty->ContainerPtrToValuePtr<void>(Container, TupleIndex);
		}

		if (ValuePtr)
		{
			UObject* ValueObject = ObjectProperty->GetObjectPropertyValue(ValuePtr);
			const TSoftObjectPtr<UObject> ValueObjectPtr = ValueObject;
			InGenericAttribute.SetStringValue(ValueObjectPtr.ToString(), AtIndex + TupleIndex);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could net get a valid value ptr for uproperty %s (Class %s)"), *InGenericAttribute.AttributeName, *PropertyClassName);
			return false;
		}
	}
	else
	{
		// Property was found, but is of an unsupported type
		HOUDINI_LOG_WARNING(TEXT("Unsupported UProperty Class: %s found for uproperty %s"), *PropertyClassName, *InGenericAttribute.AttributeName);
		return false;
	}

	return true;
}

bool
FHoudiniGenericAttribute::GetAttributeTupleSizeAndStorageFromProperty(
	UObject* InObject,
	FProperty* InFoundProperty,
	void* InContainer,
	int32& OutAttributeTupleSize,
	EAttribStorageType& OutAttributeStorageType)
{
	if (!IsValid(InObject) || !InFoundProperty)
		return false;

	// Determine the container to use (either InContainer if specified, or InObject)
	void* Container = InContainer ? InContainer : InObject;

	// Property class name, used for logging
	const FString PropertyClassName = InFoundProperty->GetClass() ? InFoundProperty->GetClass()->GetName() : TEXT("Unknown");

	// Initialize using the found property
	FProperty* InnerProperty = InFoundProperty;

	// FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InFoundProperty);
	// TSharedPtr<FScriptArrayHelper_InContainer> ArrayHelper;
	// if (ArrayProperty)
	// {
	// 	InnerProperty = ArrayProperty->Inner;
	// 	ArrayHelper = MakeShareable<FScriptArrayHelper_InContainer>(new FScriptArrayHelper_InContainer(ArrayProperty, Container));
	// }

	FFieldClass* PropertyClass = InnerProperty->GetClass();
	if (PropertyClass->IsChildOf(FNumericProperty::StaticClass()) || PropertyClass->IsChildOf(FBoolProperty::StaticClass()) ||
		PropertyClass->IsChildOf(FStrProperty::StaticClass()) || PropertyClass->IsChildOf(FNameProperty::StaticClass()))
	{
		// Supported non-struct properties

		// Here we cannot really do better than tuple size of 1 (since we need to support arrays in the future, we
		// cannot just assume array size == tuple size going to Houdini)
		OutAttributeTupleSize = 1;
		
		// Handle each property type that we support
		if (PropertyClass->IsChildOf(FNumericProperty::StaticClass()))
		{
			// Numeric properties are supported as floats and ints, and can also be set from a received string
			FNumericProperty* const Property = CastField<FNumericProperty>(InnerProperty);
			if (Property->IsFloatingPoint())
			{
				OutAttributeStorageType = EAttribStorageType::FLOAT;
			}
			else if (Property->IsInteger())
			{
				OutAttributeStorageType = EAttribStorageType::INT;
			}
			else
			{
				HOUDINI_LOG_WARNING(TEXT("Unsupported numeric property for %s (Class %s)"), *Property->GetName(), *PropertyClassName);
				return false;
			}
		}
		else if (PropertyClass->IsChildOf(FBoolProperty::StaticClass()))
		{
			OutAttributeStorageType = EAttribStorageType::INT;
		}
		else if (PropertyClass->IsChildOf(FStrProperty::StaticClass()))
		{
			OutAttributeStorageType = EAttribStorageType::STRING;
		}
		else if (PropertyClass->IsChildOf(FNameProperty::StaticClass()))
		{
			OutAttributeStorageType = EAttribStorageType::STRING;
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty))
	{
		// struct properties

		const FName PropertyName = StructProperty->Struct->GetFName();
		if (PropertyName == NAME_Vector)
		{
			OutAttributeTupleSize = 3;
			OutAttributeStorageType = EAttribStorageType::FLOAT;
		}
		else if (PropertyName == NAME_Transform)
		{
			OutAttributeTupleSize = 10;
			OutAttributeStorageType = EAttribStorageType::FLOAT;
		}
		else if (PropertyName == NAME_Color)
		{
			OutAttributeTupleSize = 4;
			OutAttributeStorageType = EAttribStorageType::INT;
		}
		else if (PropertyName == NAME_LinearColor)
		{
			OutAttributeTupleSize = 4;
			OutAttributeStorageType = EAttribStorageType::FLOAT;
		}
		else if (PropertyName == "Int32Interval")
		{
			OutAttributeTupleSize = 2;
			OutAttributeStorageType = EAttribStorageType::INT;
		}
		else if (PropertyName == "FloatInterval")
		{
			OutAttributeTupleSize = 2;
			OutAttributeStorageType = EAttribStorageType::FLOAT;
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("For uproperty %s (Class %s): unsupported struct property type: %s"), *InFoundProperty->GetName(), *PropertyClassName, *PropertyName.ToString());
			return false;
		}
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InnerProperty))
	{
		OutAttributeTupleSize = 1;
		OutAttributeStorageType = EAttribStorageType::STRING;
	}
	else
	{
		// Property was found, but is of an unsupported type
		HOUDINI_LOG_WARNING(TEXT("Unsupported UProperty Class: %s found for uproperty %s"), *PropertyClassName, *InFoundProperty->GetName());
		return false;
	}

	return true;
}

/*
bool
FHoudiniEngineUtils::TryToFindInStructProperty(
	UObject* Object, FString UPropertyNameToFind, UStructProperty* StructProperty, UProperty*& FoundProperty, void*& StructContainer )
{
	if ( !StructProperty || !Object )
		return false;

	// Walk the structs' properties and try to find the one we're looking for
	UScriptStruct* Struct = StructProperty->Struct;
	for (TFieldIterator< UProperty > It(Struct); It; ++It)
	{
		UProperty* Property = *It;
		if ( !Property )
			continue;

		FString DisplayName = It->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
		FString Name = It->GetName();

		// If the property name contains the uprop attribute name, we have a candidate
		if ( Name.Contains( UPropertyNameToFind ) || DisplayName.Contains( UPropertyNameToFind ) )
		{
			// We found the property in the struct property, we need to keep the ValuePtr in the object
			// of the structProp in order to be able to access the property value afterwards...
			FoundProperty = Property;
			StructContainer = StructProperty->ContainerPtrToValuePtr< void >( Object, 0);

			// If it's an equality, we dont need to keep searching
			if ( ( Name == UPropertyNameToFind ) || ( DisplayName == UPropertyNameToFind ) )
				return true;
		}

		if ( FoundProperty )
			continue;

		UStructProperty* NestedStruct = Cast<UStructProperty>( Property );
		if ( NestedStruct && TryToFindInStructProperty( Object, UPropertyNameToFind, NestedStruct, FoundProperty, StructContainer ) )
			return true;

		UArrayProperty* ArrayProp = Cast<UArrayProperty>( Property );
		if ( ArrayProp && TryToFindInArrayProperty( Object, UPropertyNameToFind, ArrayProp, FoundProperty, StructContainer ) )
			return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::TryToFindInArrayProperty(
	UObject* Object, FString UPropertyNameToFind, UArrayProperty* ArrayProperty, UProperty*& FoundProperty, void*& StructContainer )
{
	if ( !ArrayProperty || !Object )
		return false;

	UProperty* Property = ArrayProperty->Inner;
	if ( !Property )
		return false;

	FString DisplayName = Property->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
	FString Name = Property->GetName();

	// If the property name contains the uprop attribute name, we have a candidate
	if ( Name.Contains( UPropertyNameToFind ) || DisplayName.Contains( UPropertyNameToFind ) )
	{
		// We found the property in the struct property, we need to keep the ValuePtr in the object
		// of the structProp in order to be able to access the property value afterwards...
		FoundProperty = Property;
		StructContainer = ArrayProperty->ContainerPtrToValuePtr< void >( Object, 0);

		// If it's an equality, we dont need to keep searching
		if ( ( Name == UPropertyNameToFind ) || ( DisplayName == UPropertyNameToFind ) )
			return true;
	}

	if ( !FoundProperty )
	{
		UStructProperty* NestedStruct = Cast<UStructProperty>( Property );
		if ( NestedStruct && TryToFindInStructProperty( ArrayProperty, UPropertyNameToFind, NestedStruct, FoundProperty, StructContainer ) )
			return true;

		UArrayProperty* ArrayProp = Cast<UArrayProperty>( Property );
		if ( ArrayProp && TryToFindInArrayProperty( ArrayProperty, UPropertyNameToFind, ArrayProp, FoundProperty, StructContainer ) )
			return true;
	}

	return false;
}
*/