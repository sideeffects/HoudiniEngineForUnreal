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

#pragma once

#include "LandscapeInfo.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Math/Box.h"

#if WITH_EDITOR
	#include "SSCSEditor.h"
	#include "ObjectTools.h"
	#include "Kismet2/ComponentEditorUtils.h"
	#include "Editor/Transactor.h"
#endif

class AActor;
class UWorld;
struct FHoudiniStaticMeshGenerationProperties;
struct FMeshBuildSettings;

template<class TClass>
class TSubclassOf;


struct HOUDINIENGINERUNTIME_API FHoudiniEngineRuntimeUtils
{
	public:

		// Return platform specific name of libHAPI.
		static FString GetLibHAPIName();

		// Returns default SM Generation Properties using the default settings values
		static FHoudiniStaticMeshGenerationProperties GetDefaultStaticMeshGenerationProperties();

		// Returns default SM Build Settings using the default settings values
		static FMeshBuildSettings GetDefaultMeshBuildSettings();

		// -----------------------------------------------
		// Bounding Box utilities
		// -----------------------------------------------

		// Collect all the bounding boxes form the specified list of actors. OutBBoxes will be emptied.
		static void GetBoundingBoxesFromActors(const TArray<AActor*> InActors, TArray<FBox>& OutBBoxes);

		// Collect actors that derive from the given class that intersect with the given array of bounding boxes.
		static bool FindActorsOfClassInBounds(UWorld* World, TSubclassOf<AActor> ActorType, const TArray<FBox>& BBoxes, const TArray<AActor*>* ExcludeActors, TArray<AActor*>& OutActors);

		// -----------------------------------------------
		// File path utilities
		// -----------------------------------------------

		// Joins paths by taking into account whether paths
		// successive paths are relative or absolute.
		// Truncate everything preceding an absolute path.
		// Taken and adapted from FPaths::Combine().
		template <typename... PathTypes>
		FORCEINLINE static FString JoinPaths(PathTypes&&... InPaths)
		{
			const TCHAR* Paths[] = { GetTCharPtr(Forward<PathTypes>(InPaths))... };
			const int32 NumPaths = UE_ARRAY_COUNT(Paths);

			FString Out = TEXT("");
			if (NumPaths <= 0)
				return Out;
			Out = Paths[NumPaths-1];
			// Process paths in reverse and terminate when we reach an absolute path. 
			for (int32 i=NumPaths-2; i >= 0; --i)
			{
				if (FCString::Strlen(Paths[i]) == 0)
					continue;
				if (Out[0] == '/')
				{
					// We already have an absolute path. Terminate.
					break;
				}
				Out = Paths[i] / Out;
			}
			if (Out.Len() > 0 && Out[0] != '/')
				Out = TEXT("/") + Out;
			return Out;
		}
		
		// ------------------------------------------------------------------
		// ObjectTools (Make some editor only ObjectTools functions available
		// in editor builds)
		// ------------------------------------------------------------------

		// Check/gather references to InObject.
		// Returns true if the function could execute (editor vs runtime)
		FORCEINLINE static bool GatherObjectReferencersForDeletion(UObject* InObject, bool& bOutIsReferenced, bool& bOutIsReferencedByUndo, FReferencerInformationList* OutMemoryReferences = nullptr, bool bInRequireReferencingProperties = false)
		{
#if WITH_EDITOR
			ObjectTools::GatherObjectReferencersForDeletion(InObject, bOutIsReferenced, bOutIsReferencedByUndo, OutMemoryReferences, bInRequireReferencingProperties);

			return true;
#else
			return false;
#endif
		}

		// Delete a single object from its package. Returns true if the object was deleted. In non-editor
		// builds this function returns false.
		FORCEINLINE static bool DeleteSingleObject(UObject* InObjectToDelete, bool bInPerformReferenceCheck=true)
		{
#if WITH_EDITOR
			return ObjectTools::DeleteSingleObject(InObjectToDelete, bInPerformReferenceCheck);
#else
			return false;
#endif
		}

		// Collects garbage and marks truely empty packages for delete
		// Returns true if the function could execute (editor vs runtime)
		FORCEINLINE static bool CleanupAfterSuccessfulDelete(const TArray<UPackage*>& InObjectsDeletedSuccessfully, bool bInPerformReferenceCheck=true)
		{
#if WITH_EDITOR
			ObjectTools::CleanupAfterSuccessfulDelete(InObjectsDeletedSuccessfully, bInPerformReferenceCheck);
			return true;
#else
			return false;
#endif
		}

		// Deletes a single object. Returns true if the object was deleted. 
		// The object is only deleted if there are no references to it.
		// If the package is on disk then bOutPackageIsInMemoryOnly is false and the CleanUpAfterSuccessfulDelete
	    // must be called on the package after execution of this function.
		static bool SafeDeleteSingleObject(UObject* const InObjectToDelete, UPackage*& OutPackage, bool& bOutPackageIsInMemoryOnly);

		// Deletes and cleans up on disk empty-packages for the objects in InObjectsToDelete.
		// Objects are popped from InObjectsToDelete as they are processed (ran into cases where objects are garbage collected
	    // before we can properly delete them and cleanup their packages, so we tend to pass in a UPROPERTY based TArray
		// that holds references to UObject to prevent garbage collection until we can delete them in this function)
		// OutObjectsNotDeleted can optionally be used to return objects that could not be deleted.
		// The function returns the number of objects that were deleted.
		static int32 SafeDeleteObjects(TArray<UObject*>& InObjectsToDelete, TArray<UObject*>* OutObjectsNotDeleted=nullptr);

		// -------------------------------------------------
		// Type utilities
		// -------------------------------------------------

		// Taken from here: https://answers.unrealengine.com/questions/330496/conversion-of-enum-to-string.html
		// Return the string representation of an enum value.
		template<typename T>
		static FString EnumToString(const FString& EnumName, const T Value)
		{
			UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName);
			return *(Enum ? Enum->GetNameStringByValue(static_cast<uint8>(Value)) : "null");
		}

		template<typename T>
		static FString EnumToString(const T Value)
		{
			return UEnum::GetValueAsString(Value);
		}

		// -------------------------------------------------
		// Blueprint utilities
		// -------------------------------------------------
#if WITH_EDITOR
		// This function contains an excerpt from UEditorUtilities::CopyActorProperties()
		// for specifically dealing with copying properties between components as well as propagating
		// property changes to archetype instances.
		static int32 CopyComponentProperties(UActorComponent* FromComponent, UActorComponent* ToComponent, const EditorUtilities::FCopyOptions& Options);

		// Get the SCSEditor for the given HoudiniAssetComponent
		static FBlueprintEditor* GetBlueprintEditor(const UObject* InObject); 

		static void MarkBlueprintAsStructurallyModified(UActorComponent* ComponentTemplate);
		static void MarkBlueprintAsModified(UActorComponent* ComponentTemplate);
#endif

		// -------------------------------------------------
		// Landscape utilities
		// -------------------------------------------------

		static FTransform CalculateHoudiniLandscapeTransform(ULandscapeInfo* LandscapeInfo);

		// -------------------------------------------------
		// Editor Helpers
		// -------------------------------------------------
#if WITH_EDITOR
		static bool SetActorLabel(AActor* Actor, const FString& ActorLabel);

		static void DoPostEditChangeProperty(UObject* Obj, FName PropertyName);

		static void DoPostEditChangeProperty(UObject* Obj, FProperty* Property);

		static void PropagateObjectDeltaChangeToArchetypeInstance(UObject* InObject, const FTransactionObjectDeltaChange& DeltaChange);
		
		template<class T>
		static TSet<USceneComponent*> PropagateDefaultValueChange(USceneComponent* InSceneComponentTemplate, const FName& PropertyName, const T& OldValue, const T& NewValue)
		{
			TSet<USceneComponent*> UpdatedInstances;
			FComponentEditorUtils::PropagateDefaultValueChange(InSceneComponentTemplate, FindFieldChecked<FProperty>(InSceneComponentTemplate->GetClass(), PropertyName), OldValue, NewValue, UpdatedInstances);
			return UpdatedInstances;
		}

		// Perform this operation on the given archetype as well as each archetype instance. If the given
		// object in not an archetype instance, then don't do anything.
		static void ForAllArchetypeInstances(UObject* Archetype, TFunctionRef<void(UObject* Obj)> Operation);

#endif

	/**
	// * Set the value on an UObject using reflection.
	// * @param	Object			The object to copy the value into.
	// * @param	PropertyName	The name of the property to set.
	// * @param	Value			The value to assign to the property.
	// *
	// * @return true if the value was set correctly
	// */
	//template <typename ObjectType, typename ValueType>
	//static bool SetPropertyValue(ObjectType* Object, FName PropertyName, ValueType Value)
	//{
	//	// Get the property addresses for the source and destination objects.
	//	FProperty* Property = FindFieldChecked<FProperty>(ObjectType::StaticClass(), PropertyName);

	//	// Get the property addresses for the object
	//	ValueType* SourceAddr = Property->ContainerPtrToValuePtr<ValueType>(Object);

	//	if ( SourceAddr == NULL )
	//	{
	//		return false;
	//	}

	//	if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
	//	{
	//		FEditPropertyChain PropertyChain;
	//		PropertyChain.AddHead(Property);
	//		Object->PreEditChange(PropertyChain);
	//	}

	//	// Set the value on the destination object.
	//	*SourceAddr = Value;

	//	if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
	//	{
	//		FPropertyChangedEvent PropertyEvent(Property);
	//		Object->PostEditChangeProperty(PropertyEvent);
	//	}

	//	return true;
	//}
#if WITH_EDITOR
	template <typename ObjectType, typename ValueType>
	static bool SetTemplatePropertyValue(ObjectType* Object, FName PropertyName, ValueType Value)
	{
		// Get the property addresses for the source and destination objects.
		FProperty* Property = FindFieldChecked<FProperty>(ObjectType::StaticClass(), PropertyName);

		// Get the property addresses for the object
		ValueType* SourceAddr = Property->ContainerPtrToValuePtr<ValueType>(Object);

		if ( SourceAddr == NULL )
		{
			return false;
		}

		if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(Property);
			((UObject*)Object)->PreEditChange(PropertyChain);
		}

		// Set the value on the destination object.
		if (*SourceAddr != Value)
		{
			TSet<USceneComponent*> UpdatedInstances;
			*SourceAddr = Value;
			PropagateDefaultValueChange(Object, Property, *SourceAddr, Value, UpdatedInstances);
		}

		if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FPropertyChangedEvent PropertyEvent(Property);
			Object->PostEditChangeProperty(PropertyEvent);
		}

		return true;
	}
#endif

#if WITH_EDITOR
	// Bool specialization
	template <typename ObjectType>
	static bool SetTemplatePropertyValue(ObjectType* Object, FName PropertyName, bool NewBool)
	{
		// Get the property addresses for the source and destination objects.
		FBoolProperty* BoolProperty = FindFieldChecked<FBoolProperty>(ObjectType::StaticClass(), PropertyName);
		check(BoolProperty);

		// Get the property addresses for the object
		const int32 PropertyOffset = INDEX_NONE;
		void* CurrentValue = PropertyOffset == INDEX_NONE ? BoolProperty->ContainerPtrToValuePtr<void>(Object) : ((uint8*)Object + PropertyOffset);
		check(CurrentValue);
		
		const bool CurrentBool = BoolProperty->GetPropertyValue(CurrentValue);

		// if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(BoolProperty);
			((UObject*)Object)->PreEditChange(PropertyChain);
		}

		// Set the value on the destination object.
		if (CurrentBool != NewBool)
		{
			TSet<USceneComponent*> UpdatedInstances;
			BoolProperty->SetPropertyValue(CurrentValue, NewBool);
			FComponentEditorUtils::PropagateDefaultValueChange(Object, BoolProperty, CurrentBool, NewBool, UpdatedInstances);
		}

		// if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FPropertyChangedEvent PropertyEvent(BoolProperty);
			Object->PostEditChangeProperty(PropertyEvent);
		}

		return true;
	}
#endif

	protected:
		// taken from FPaths::GetTCharPtr
		FORCEINLINE static const TCHAR* GetTCharPtr(const TCHAR* Ptr)
		{
			return Ptr;
		}
		FORCEINLINE static const TCHAR* GetTCharPtr(const FString& Str)
		{
			return *Str;
		}
};