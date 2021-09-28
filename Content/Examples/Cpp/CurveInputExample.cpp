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

#include "CurveInputExample.h"

#include "Engine/StaticMesh.h"

#include "HoudiniAsset.h"
#include "HoudiniPublicAPI.h"
#include "HoudiniPublicAPIBlueprintLib.h"
#include "HoudiniPublicAPIAssetWrapper.h"
#include "HoudiniPublicAPIInputTypes.h"

ACurveInputExample::ACurveInputExample()
	: AssetWrapper(nullptr)
{
	
}

void ACurveInputExample::RunCurveInputExample_Implementation()
{
	// Get the API instance
	UHoudiniPublicAPI* const API = UHoudiniPublicAPIBlueprintLib::GetAPI();
	// Ensure we have a running session
	if (!API->IsSessionValid())
		API->CreateSession();
	// Load our HDA uasset
	UHoudiniAsset* const ExampleHDA = Cast<UHoudiniAsset>(StaticLoadObject(UHoudiniAsset::StaticClass(), nullptr, TEXT("/HoudiniEngine/Examples/hda/copy_to_curve_1_0.copy_to_curve_1_0")));
	// Create an API wrapper instance for instantiating the HDA and interacting with it
	AssetWrapper = API->InstantiateAsset(ExampleHDA, FTransform::Identity);
	if (IsValid(AssetWrapper))
	{
		// Pre-instantiation is the earliest point where we can set parameter values
		AssetWrapper->GetOnPreInstantiationDelegate().AddUniqueDynamic(this, &ACurveInputExample::SetInitialParameterValues);
		// Post-instantiation is the earliest point where we can set inputs
		AssetWrapper->GetOnPostInstantiationDelegate().AddUniqueDynamic(this, &ACurveInputExample::SetInputs);
		// After a cook and after the plugin has created/updated objects/assets from the node's outputs
		AssetWrapper->GetOnPostProcessingDelegate().AddUniqueDynamic(this, &ACurveInputExample::PrintOutputs);
	}
}

void ACurveInputExample::SetInitialParameterValues_Implementation(UHoudiniPublicAPIAssetWrapper* InWrapper)
{
	// Uncheck the upvectoratstart parameter
	InWrapper->SetBoolParameterValue(TEXT("upvectoratstart"), false);

	// Set the scale to 0.2
	InWrapper->SetFloatParameterValue(TEXT("scale"), 0.2f);

	// Since we are done with setting the initial values, we can unbind from the delegate
	InWrapper->GetOnPreInstantiationDelegate().RemoveDynamic(this, &ACurveInputExample::SetInitialParameterValues);
}

void ACurveInputExample::SetInputs_Implementation(UHoudiniPublicAPIAssetWrapper* InWrapper)
{
	// Create an empty geometry input
	UHoudiniPublicAPIGeoInput* const GeoInput = Cast<UHoudiniPublicAPIGeoInput>(InWrapper->CreateEmptyInput(UHoudiniPublicAPIGeoInput::StaticClass()));
	// Load the cube static mesh asset
	UStaticMesh* const Cube = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	// Set the input object array for our geometry input, in this case containing only the cube
	GeoInput->SetInputObjects({Cube});

	// Set the input on the instantiated HDA via the wrapper
	InWrapper->SetInputAtIndex(0, GeoInput);

	// Create an empty curve input
	UHoudiniPublicAPICurveInput* const CurveInput = Cast<UHoudiniPublicAPICurveInput>(InWrapper->CreateEmptyInput(UHoudiniPublicAPICurveInput::StaticClass()));
	// Create the curve input object
	UHoudiniPublicAPICurveInputObject* const CurveObject = NewObject<UHoudiniPublicAPICurveInputObject>(CurveInput);
	// Make it a Nurbs curve
	CurveObject->SetCurveType(EHoudiniPublicAPICurveType::Nurbs);
	// Set the points of the curve, for this example we create a helix consisting of 100 points
	TArray<FTransform> CurvePoints;
	CurvePoints.Reserve(100);
	for (int32 i = 0; i < 100; ++i)
	{
		const float t = i / 20.0f * PI * 2.0f;
		const float x = 100.0f * cos(t);
		const float y = 100.0f * sin(t);
		const float z = i;
		CurvePoints.Emplace(FTransform(FVector(x, y, z)));
	}
	CurveObject->SetCurvePoints(CurvePoints);
	// Set the curve wrapper as an input object
	CurveInput->SetInputObjects({CurveObject});
	// Copy the input data to the HDA as node input 1
	InWrapper->SetInputAtIndex(1, CurveInput);

	// Since we are done with setting the initial values, we can unbind from the delegate
	InWrapper->GetOnPostInstantiationDelegate().RemoveDynamic(this, &ACurveInputExample::SetInputs);
}

void ACurveInputExample::PrintOutputs_Implementation(UHoudiniPublicAPIAssetWrapper* InWrapper)
{
	// Print out all outputs generated by the HDA
	const int32 NumOutputs = InWrapper->GetNumOutputs();
	UE_LOG(LogTemp, Log, TEXT("NumOutputs: %d"), NumOutputs);
	if (NumOutputs > 0)
	{
		for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
		{
			TArray<FHoudiniPublicAPIOutputObjectIdentifier> Identifiers;
			InWrapper->GetOutputIdentifiersAt(OutputIndex, Identifiers);
			UE_LOG(LogTemp, Log, TEXT("\toutput index: %d"), OutputIndex);
			UE_LOG(LogTemp, Log, TEXT("\toutput type: %d"), InWrapper->GetOutputTypeAt(OutputIndex));
			UE_LOG(LogTemp, Log, TEXT("\tnum_output_objects: %d"), Identifiers.Num());
			if (Identifiers.Num() > 0)
			{
				for (const FHoudiniPublicAPIOutputObjectIdentifier& Identifier : Identifiers)
				{
					UObject* const OutputObject = InWrapper->GetOutputObjectAt(OutputIndex, Identifier);
					UObject* const OutputComponent = InWrapper->GetOutputComponentAt(OutputIndex, Identifier);
					const bool bIsProxy = InWrapper->IsOutputCurrentProxyAt(OutputIndex, Identifier);
					UE_LOG(LogTemp, Log, TEXT("\t\tidentifier: %s_%s"), *(Identifier.PartName), *(Identifier.SplitIdentifier));
					UE_LOG(LogTemp, Log, TEXT("\t\toutput_object: %s"), IsValid(OutputObject) ? *(OutputObject->GetFName().ToString()) : TEXT("None"))
					UE_LOG(LogTemp, Log, TEXT("\t\toutput_component: %s"), IsValid(OutputComponent) ? *(OutputComponent->GetFName().ToString()) : TEXT("None"))
					UE_LOG(LogTemp, Log, TEXT("\t\tis_proxy: %d"), bIsProxy)
					UE_LOG(LogTemp, Log, TEXT(""))
				}
			}
		}
	}
}
