/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "CoreMinimal.h"
#include "HoudiniToolTypes.generated.h"

USTRUCT(BlueprintType)
struct FCategoryRules
{
	GENERATED_BODY();

	// Include any tools that match the following patterns.
	UPROPERTY(EditAnywhere, Category="Category Patterns")
	TArray<FString> Include;

	// Any tools matching these patterns will be omitted from the inclusion set.
	UPROPERTY(EditAnywhere, Category="Category Patterns")
	TArray<FString> Exclude;
};

UENUM()
enum class EHoudiniToolType : uint8
{
	// For tools that generates geometry, and do not need input
	HTOOLTYPE_GENERATOR UMETA(DisplayName = "Generator"),

	// For tools that have a single input, the selection will be merged in that single input
	HTOOLTYPE_OPERATOR_SINGLE UMETA(DisplayName = "Operator (single)"),

	// For Tools that have multiple input, a single selected asset will be applied to each input
	HTOOLTYPE_OPERATOR_MULTI UMETA(DisplayName = "Operator (multiple)"),

	// For tools that needs to be applied each time for each single selected
	HTOOLTYPE_OPERATOR_BATCH UMETA(DisplayName = "Batch Operator")
};

UENUM()
enum class EHoudiniToolSelectionType : uint8
{
	// For tools that can be applied both to Content Browser and World selection
	HTOOL_SELECTION_ALL UMETA(DisplayName = "Content Browser AND World"),

	// For tools that can be applied only to World selection
	HTOOL_SELECTION_WORLD_ONLY UMETA(DisplayName = "World selection only"),

	// For tools that can be applied only to Content Browser selection
	HTOOL_SELECTION_CB_ONLY UMETA(DisplayName = "Content browser selection only")
};