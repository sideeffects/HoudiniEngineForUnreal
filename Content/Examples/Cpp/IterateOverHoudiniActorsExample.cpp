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


#include "IterateOverHoudiniActorsExample.h"

#include "EngineUtils.h"

#include "HoudiniAssetActor.h"
#include "HoudiniPublicAPI.h"
#include "HoudiniPublicAPIBlueprintLib.h"
#include "HoudiniPublicAPIAssetWrapper.h"


// Sets default values
AIterateOverHoudiniActorsExample::AIterateOverHoudiniActorsExample()
{
}

void AIterateOverHoudiniActorsExample::RunIterateOverHoudiniActorsExample_Implementation()
{
	// Get the API instance
	UHoudiniPublicAPI* const API = UHoudiniPublicAPIBlueprintLib::GetAPI();
	// Ensure we have a running Houdini Engine session
	if (!API->IsSessionValid())
		API->CreateSession();

	// Iterate over HoudiniAssetActors in the world (by default when instantiating an HDA in the world an
	// AHoudiniAssetActor is spawned, which has a component that manages the instantiated HDA node in Houdini Engine.
	UWorld* const OurWorld = GetWorld();
	for (TActorIterator<AHoudiniAssetActor> It(OurWorld); It; ++It)
	{
		AHoudiniAssetActor* const HDAActor = *It;
		if (!IsValid(HDAActor))
			continue;

		// Print the name of the actor
		UE_LOG(LogTemp, Display, TEXT("HDA Actor (Name, Label): %s, %s"), *(HDAActor->GetName()), *(HDAActor->GetActorLabel()));
		
		// Wrap it with the API
		UHoudiniPublicAPIAssetWrapper* const Wrapper = UHoudiniPublicAPIAssetWrapper::CreateWrapper(this, HDAActor);
		if (!IsValid(Wrapper))
			continue;
		
		// Print its parameters via the API
		TMap<FName, FHoudiniParameterTuple> ParameterTuples;
		if (!Wrapper->GetParameterTuples(ParameterTuples))
		{
			// The operation failed, log the error
			FString ErrorMessage;
			if (Wrapper->GetLastErrorMessage(ErrorMessage))
				UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMessage);

			continue;
		}

		UE_LOG(LogTemp, Display, TEXT("# Parameter Tuples: %d"), ParameterTuples.Num());
		for (const auto& Entry : ParameterTuples)
		{
			UE_LOG(LogTemp, Display, TEXT("\tParameter Tuple Name: %s"), *(Entry.Key.ToString()));

			const FHoudiniParameterTuple& ParameterTuple = Entry.Value;
			TArray<FString> Strings;
			FString Type;
			if (ParameterTuple.BoolValues.Num() > 0)
			{
				Type = TEXT("Bool");
				for (const bool Value : ParameterTuple.BoolValues)
				{
					Strings.Add(Value ? TEXT("1") : TEXT("0"));
				}
			}
			else if (ParameterTuple.FloatValues.Num() > 0)
			{
				Type = TEXT("Float");
				for (const float Value : ParameterTuple.FloatValues)
				{
					Strings.Add(FString::Printf(TEXT("%.4f"), Value));
				}
			}
			else if (ParameterTuple.Int32Values.Num() > 0)
			{
				Type = TEXT("Int32");
				for (const int32 Value : ParameterTuple.Int32Values)
				{
					Strings.Add(FString::Printf(TEXT("%d"), Value));
				}
			}
			else if (ParameterTuple.StringValues.Num() > 0)
			{
				Type = TEXT("String");
				Strings = ParameterTuple.StringValues;
			}

			if (Type.Len() == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("\t\tEmpty"));
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("\t\t%s Values: %s"), *Type, *FString::Join(Strings, TEXT("; ")));
			}
		}
	}
}
