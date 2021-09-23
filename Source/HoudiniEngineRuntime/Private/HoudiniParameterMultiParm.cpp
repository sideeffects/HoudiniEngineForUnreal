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

#include "HoudiniParameterMultiParm.h"

UHoudiniParameterMultiParm::UHoudiniParameterMultiParm(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer), bIsShown(false), InstanceStartOffset(0)
{
	// TODO Proper Init
	ParmType = EHoudiniParameterType::MultiParm;
}

UHoudiniParameterMultiParm *
UHoudiniParameterMultiParm::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterMultiParm_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterMultiParm::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterMultiParm * HoudiniAssetParameter = NewObject< UHoudiniParameterMultiParm >(
		InOuter, UHoudiniParameterMultiParm::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(EHoudiniParameterType::MultiParm);
	//HoudiniAssetParameter->UpdateFromParmInfo(InParentParameter, InNodeId, ParmInfo);

	HoudiniAssetParameter->DefaultInstanceCount = -1;

	return HoudiniAssetParameter;
}

bool
UHoudiniParameterMultiParm::SetValue(const int32& InValue)
{
	if (InValue == Value)
		return false;
	
	Value = InValue;

	return true;
}

void
UHoudiniParameterMultiParm::InsertElement()
{
	if (MultiParmInstanceCount > 0 && MultiParmInstanceLastModifyArray.Num() == 0)
		InitializeModifyArray();

	MultiParmInstanceLastModifyArray.Add(EHoudiniMultiParmModificationType::Inserted);
}

void
UHoudiniParameterMultiParm::InsertElementAt(int32 Index) 
{
	if (MultiParmInstanceCount > 0 && MultiParmInstanceLastModifyArray.Num() == 0)
		InitializeModifyArray();

	if (Index >= MultiParmInstanceLastModifyArray.Num())
		MultiParmInstanceLastModifyArray.Add(EHoudiniMultiParmModificationType::Inserted);
	else
		MultiParmInstanceLastModifyArray.Insert(EHoudiniMultiParmModificationType::Inserted, Index);
}

/** Decrement value, used by Slate. **/
void 
UHoudiniParameterMultiParm::RemoveElement(int32 Index) 
{
	if (MultiParmInstanceCount > 0 && MultiParmInstanceLastModifyArray.Num() == 0)
		InitializeModifyArray();

	// Remove the last element
	if (Index == -1) 
	{
		Index = MultiParmInstanceLastModifyArray.Num() - 1;
		while (MultiParmInstanceLastModifyArray.IsValidIndex(Index) && MultiParmInstanceLastModifyArray[Index] == EHoudiniMultiParmModificationType::Removed)
			Index -= 1;
	}

	if (MultiParmInstanceLastModifyArray.IsValidIndex(Index)) 
	{
		// If the removed is a to be inserted instance, simply remove it.
		if (MultiParmInstanceLastModifyArray[Index] == EHoudiniMultiParmModificationType::Inserted)
			MultiParmInstanceLastModifyArray.RemoveAt(Index);
		// Otherwise mark it as to be removed.
		else
			MultiParmInstanceLastModifyArray[Index] = EHoudiniMultiParmModificationType::Removed;
	}
}

void 
UHoudiniParameterMultiParm::EmptyElements() 
{
	if (MultiParmInstanceCount > 0 && MultiParmInstanceLastModifyArray.Num() == 0)
		InitializeModifyArray();

	for (int32 Index = MultiParmInstanceLastModifyArray.Num() - 1; Index >= 0; --Index)
	{
		// If the removed is a to be inserted instance, simply remove it.
		// Interation starts from the tail, so that the indices won't be changed by element removal.
		if (MultiParmInstanceLastModifyArray[Index] == EHoudiniMultiParmModificationType::Inserted)
			MultiParmInstanceLastModifyArray.RemoveAt(Index);
		else // Otherwise mark it as to be removed.
			MultiParmInstanceLastModifyArray[Index] = EHoudiniMultiParmModificationType::Removed;
	}
}

int32
UHoudiniParameterMultiParm::GetNextInstanceCount() const
{
	if (MultiParmInstanceCount > 0 && MultiParmInstanceLastModifyArray.Num() == 0)
	{
		return MultiParmInstanceCount;
	}
	
	int32 CurrentInstanceCount = 0;
	// First determine how many instances the multi parm would have based on the current values in
	// MultiParmInstanceLastModifyArray
	for (const EHoudiniMultiParmModificationType& ModificationType : MultiParmInstanceLastModifyArray)
	{
		switch (ModificationType)
		{
		case EHoudiniMultiParmModificationType::Inserted:
		case EHoudiniMultiParmModificationType::Modified:
		case EHoudiniMultiParmModificationType::None:
			CurrentInstanceCount++;
			break;
		case EHoudiniMultiParmModificationType::Removed:
			// Removed indices don't add to CurrentInstanceCount 
			break;
		}
	}

	return CurrentInstanceCount;
}

bool
UHoudiniParameterMultiParm::SetNumElements(const int32 InInstanceCount)
{
	if (MultiParmInstanceCount > 0 && MultiParmInstanceLastModifyArray.Num() == 0)
		InitializeModifyArray();

	// // Log the MultiParmInstanceLastModifyArray before the modification
	// HOUDINI_LOG_WARNING(TEXT("MultiParmInstanceLastModifyArray (before): "));
	// for (const EHoudiniMultiParmModificationType Modification : MultiParmInstanceLastModifyArray)
	// {
	// 	HOUDINI_LOG_WARNING(TEXT("\t%s"), *UEnum::GetValueAsString(Modification));
	// }

	const int32 TargetInstanceCount = InInstanceCount >= 0 ? InInstanceCount : 0;
	const int32 CurrentInstanceCount = GetNextInstanceCount();
	bool bModified = false;
	if (CurrentInstanceCount > TargetInstanceCount)
	{
		// Remove entries from the end of the array
		for (int32 Count = CurrentInstanceCount; Count > TargetInstanceCount; --Count)
		{
			RemoveElement(-1);
		}

		bModified = true;
	}
	else if (CurrentInstanceCount < TargetInstanceCount)
	{
		// Insert new instances at the end
		for (int32 Count = CurrentInstanceCount; Count < TargetInstanceCount; ++Count)
		{
			InsertElement();
		}

		bModified = true;
	}

	// // Log the MultiParmInstanceLastModifyArray after the modification
	// HOUDINI_LOG_WARNING(TEXT("MultiParmInstanceLastModifyArray (after): "));
	// for (const EHoudiniMultiParmModificationType Modification : MultiParmInstanceLastModifyArray)
	// {
	// 	HOUDINI_LOG_WARNING(TEXT("\t%s"), *UEnum::GetValueAsString(Modification));
	// }
	
	return bModified;
}

void
UHoudiniParameterMultiParm::InitializeModifyArray() 
{
	for (uint32 Index = 0; Index < MultiParmInstanceCount; ++Index) 
	{
		MultiParmInstanceLastModifyArray.Add(EHoudiniMultiParmModificationType::None);
	}
}

bool 
UHoudiniParameterMultiParm::IsDefault() const 
{
	//UE_LOG(LogTemp, Warning, TEXT("%d, %d"), MultiParmInstanceNum, MultiParmInstanceCount);
	return DefaultInstanceCount == MultiParmInstanceCount;
}

void 
UHoudiniParameterMultiParm::SetDefaultInstanceCount(int32 InCount)
{
	if (DefaultInstanceCount >= 0)
		return;

	DefaultInstanceCount = InCount;
}
