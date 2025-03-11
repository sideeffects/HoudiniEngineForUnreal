/*
* Copyright (c) <2025> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#include "HoudiniPCGUtils.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniPCGNode.h"
#include "HoudiniPCGCookable.h"
#include "HoudiniInput.h"
#include "ConnectionDrawingPolicy.h"
#include "HoudiniPCGTranslator.h"
#include "HoudiniPCGInputObject.h"
#include "PCGParamData.h"

const FName HDAInputObject = FName(FString(TEXT("hda_input")));

FString FHoudiniPCGUtils::ParameterInputPinName = FString(TEXT("Parameters"));

void FHoudiniPCGUtils::UnrealToHoudini(const FVector3d& UnrealVector, float HoudiniVector[3])
{
	HoudiniVector[0] = static_cast<float>(UnrealVector.X);
	HoudiniVector[1] = static_cast<float>(UnrealVector.Z);
	HoudiniVector[2] = static_cast<float>(UnrealVector.Y);
}

TArray<FHoudiniPCGObjectOutput> FHoudiniPCGUtils::GetPCGOutputData(const UHoudiniOutput* HoudiniOutput)
{
	TArray<FHoudiniPCGObjectOutput> Outputs;

	int ObjectIndex = 0;
	for(auto It : HoudiniOutput->GetOutputObjects())
	{
		FHoudiniOutputObject& OutputObj = It.Value;

		FHoudiniPCGObjectOutput& PCGOutputObject = Outputs.Emplace_GetRef();
		PCGOutputObject.OutputIndex = ObjectIndex;
		if(OutputObj.OutputObject)
			PCGOutputObject.ObjectPath = OutputObj.OutputObject.GetPathName();
		if(OutputObj.OutputComponents.Num() > 0)
		{
			PCGOutputObject.ComponentPath = OutputObj.OutputComponents[0].GetPathName();
			PCGOutputObject.ActorPath = OutputObj.OutputComponents[0]->GetOuter()->GetPathName();
		}
		else if(OutputObj.ProxyComponent)
		{
			PCGOutputObject.ComponentPath = OutputObj.ProxyComponent.GetPathName();
			if(OutputObj.ProxyObject)
				PCGOutputObject.ObjectPath = OutputObj.ProxyObject.GetPackage().GetPathName();
		}
		else if(OutputObj.OutputActors.Num() > 0)
		{
			PCGOutputObject.ActorPath = OutputObj.OutputActors[0]->GetPathName();
		}
		ObjectIndex++;
	}
	return Outputs;
}

bool FHoudiniPCGUtils::HasPCGOutputs(const UHoudiniOutput* HoudiniOutput)
{
	for (auto & It : HoudiniOutput->GetOutputObjects())
	{
		auto & Object = It.Value;

		if(Object.OutputObject->IsA<UHoudiniPCGOutputData>())
			return true;
	}
	return false;
}

EHoudiniPCGInputType FHoudiniPCGUtils::GetInputType(const UPCGData* PCGData)
{
	if(PCGData->IsA<UPCGPointData>())
		return EHoudiniPCGInputType::PCGData;
	else if(PCGData->IsA<UPCGParamData>())
	{
		const UPCGMetadata* Metadata = PCGData->ConstMetadata();

		const FPCGMetadataAttribute<FSoftObjectPath>* ObjectAttrs = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(Metadata->GetConstAttribute(HDAInputObject));
		if(ObjectAttrs)
			return EHoudiniPCGInputType::UnrealObjects;
		else
			return EHoudiniPCGInputType::PCGData;
	}
	else
	{
		return EHoudiniPCGInputType::None;
	}
}

bool UHoudiniPCGCookable::ApplyInputAsPCGData(UHoudiniInput* HoudiniInput, const UPCGData* PCGData)
{
	bool bInputsChanged = false;

	if (HoudiniInput->GetInputType() != EHoudiniInputType::PCGInput)
	{
		bInputsChanged = true;
		bool bOutBlueprintStructureModified;
		HoudiniInput->SetInputType(EHoudiniInputType::PCGInput, bOutBlueprintStructureModified);
	}

	int ExistingObjectCount = 0;
	ExistingObjectCount += HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);
	ExistingObjectCount += HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::Curve);
	ExistingObjectCount += HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::World);

	if (ExistingObjectCount > 0)
	{
		// Previous input used non-PCG type, so we must clear them and reupload.
		bInputsChanged = true;
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Curve, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, 0);
	}

	UHoudiniPCGInputObject* NewInputData = NewObject<UHoudiniPCGInputObject>();
	NewInputData->Initialize(PCGData);

	if (HoudiniInput->GetNumberOfInputObjects(EHoudiniInputType::PCGInput) == 1)
	{
		UHoudiniPCGInputObject * Prev = Cast<UHoudiniPCGInputObject>(HoudiniInput->GetInputObjectAt(0));
		if (!IsValid(Prev))
		{
			bInputsChanged = true;
		}
		else
		{
			bInputsChanged = (*Prev != *NewInputData);
		}
	}
	else
	{
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::PCGInput, 1);
		bInputsChanged = true;
	}

	if (bInputsChanged)
	{
		HoudiniInput->SetInputObjectAt(EHoudiniInputType::PCGInput, 0, NewInputData);
	}
	return false;
}

bool UHoudiniPCGCookable::ApplyInputAsUnrealObjects(UHoudiniInput* HoudiniInput, const UPCGMetadata* Metadata)
{
	FHoudiniPCGAttributes Attributes(Metadata, HDAInputObject);


	// Extract all soft object paths from the PCG node inputs.

	int NumRows = Attributes.NumRows;
	TArray<FString> NewInputPaths;
	NewInputPaths.Reserve(NumRows);
	for(int Row = 0; Row < NumRows; Row++)
	{
		FString Path;
		FHoudiniPCGUtils::GetValueAsString(Path, Row, Attributes);
		NewInputPaths.Add(Path);
	}
	NewInputPaths.Sort();

	// Geta list of current input objects.

	TArray<FString> CurrentInputObjects;
	for(int Index = 0; Index < HoudiniInput->GetNumberOfInputObjects(); Index++)
	{
		CurrentInputObjects.Add(HoudiniInput->GetInputObjectAt(Index)->GetPathName());
	}
	CurrentInputObjects.Sort();

	// if inputs changed, set them
	bool bInputsChanged = (CurrentInputObjects != NewInputPaths);
	if(bInputsChanged)
	{
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Curve, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, 0);
		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::PCGInput, 0);

		TArray<UObject*> WorldObjects;
		TArray<UObject*> GeometryObjects;

		for(int Index = 0; Index < NewInputPaths.Num(); Index++)
		{
			UObject* InputObject = StaticLoadObject(UObject::StaticClass(), nullptr, *NewInputPaths[Index]);
			if(InputObject->IsA<AActor>())
				WorldObjects.Add(InputObject);
			else
				GeometryObjects.Add(InputObject);
		}

		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::World, WorldObjects.Num());
		for(int Index = 0; Index < WorldObjects.Num(); Index++)
		{
			HoudiniInput->SetInputObjectAt(EHoudiniInputType::World, Index, WorldObjects[Index]);
		}

		HoudiniInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, GeometryObjects.Num());
		for(int Index = 0; Index < GeometryObjects.Num(); Index++)
		{
			HoudiniInput->SetInputObjectAt(EHoudiniInputType::Geometry, Index, GeometryObjects[Index]);
		}
	}

	return bInputsChanged;
}

bool FHoudiniPCGUtils::GetValueAsString(FString& Result, int Index, const FHoudiniPCGAttributes& Attributes)
{
	if(Attributes.Strings)
	{
		Result = Attributes.Strings->GetValueFromItemKey(Index);
		return true;
	}
	else if(Attributes.SoftObjectPaths)
	{
		Result = Attributes.SoftObjectPaths->GetValueFromItemKey(Index).ToString();
		return true;
	}
	else if (Attributes.Floats)
	{
		Result = FString::SanitizeFloat(Attributes.Floats->GetValueFromItemKey(Index));
		return true;
	}
	else if (Attributes.Ints)
	{
		Result = FString::FromInt(Attributes.Ints->GetValueFromItemKey(Index));
		return true;
	}
	return false;
}

bool FHoudiniPCGUtils::GetValueAsInt(int& Result, int Index, const FHoudiniPCGAttributes& Attributes)
{
	if(Attributes.Strings)
	{
		FString StringValue = Attributes.Strings->GetValueFromItemKey(Index);
		Result = FCString::Atoi(*StringValue);
		return true;
	}
	else if(Attributes.Floats)
	{
		float FloatValue = Attributes.Floats->GetValueFromItemKey(Index);
		Result = static_cast<int>(FloatValue);
		return true;
	}
	else if(Attributes.Doubles)
	{
		Result = static_cast<int>(Attributes.Doubles->GetValueFromItemKey(Index));
		return true;
	}
	else if(Attributes.Ints)
	{
		Result = Attributes.Ints->GetValueFromItemKey(Index);
		return true;
	}
	return false;
}

bool FHoudiniPCGUtils::GetValueAsFloat(float& Result, int Index, const FHoudiniPCGAttributes& Attributes)
{
	if(Attributes.Strings)
	{
		FString StringValue = Attributes.Strings->GetValueFromItemKey(Index);
		Result = FCString::Atof(*StringValue);
		return true;
	}
	else if(Attributes.Floats)
	{
		Result = Attributes.Floats->GetValueFromItemKey(Index);
		return true;
	}
	else if(Attributes.Doubles)
	{
		Result = static_cast<float>(Attributes.Doubles->GetValueFromItemKey(Index));
		return true;
	}
	else if(Attributes.Ints)
	{
		int IntValue = Attributes.Ints->GetValueFromItemKey(Index);
		Result = static_cast<float>(IntValue);
		return true;
	}
	return false;
}

FHoudiniPCGAttributes::FHoudiniPCGAttributes(const UPCGMetadata* Metadata, const FName& ParameterName)
{
	// Cache off all attribute types we might be interest in.
	this->Ints = Metadata->GetConstTypedAttribute<int>(ParameterName);
	this->Floats = Metadata->GetConstTypedAttribute<float>(ParameterName);
	this->Doubles = Metadata->GetConstTypedAttribute<double>(ParameterName);
	this->Strings = Metadata->GetConstTypedAttribute<FString>(ParameterName);
	this->SoftObjectPaths = Metadata->GetConstTypedAttribute<FSoftObjectPath>(ParameterName);
	this->NumRows = Metadata->GetItemCountForChild();
}

FString FHoudiniPCGUtils::GetHDAInputName(int Index)
{
	// Name of the pin that exposes HDA input "Index".
	FString PinName = FString::Printf(TEXT("Input %d"), Index);
	return PinName;
}

FVector3d FHoudiniPCGUtils::HoudiniToUnrealPosition(float HoudiniVector[3])
{
	FVector3d Position;
	Position.X = HoudiniVector[0] * 100.0;
	Position.Y = HoudiniVector[2] * 100.0;
	Position.Z = HoudiniVector[1] * 100.0;
	return Position;
}

FVector3d FHoudiniPCGUtils::HoudiniToUnrealVector(float HoudiniVector[3])
{
	FVector3d Position;
	Position.X = HoudiniVector[0];
	Position.Y = HoudiniVector[2];
	Position.Z = HoudiniVector[1];
	return Position;
}

FQuat FHoudiniPCGUtils::HoudiniToUnrealQuat(float HoudiniQuat[4])
{
	FQuat Result(HoudiniQuat[0], HoudiniQuat[2], HoudiniQuat[1], -HoudiniQuat[3]);
	return Result;
}

FVector4d FHoudiniPCGUtils::UnrealToHoudiniQuat(const FQuat& Quat)
{
	FVector4d Result;
	Result[0] = Quat.X;
	Result[1] = Quat.Z;
	Result[2] = Quat.Y;
	Result[3] = -Quat.W;
	return Result;
}

void FHoudiniPCGUtils::ResetPCGSession()
{
	UHoudiniPCGCookableCache::InvalidateAllCaches();
}

