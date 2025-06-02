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
#include "HoudiniEngine.h"
#include "HoudiniPCGTranslator.h"
#include "HoudiniPCGDataObject.h"
#include "Landscape.h"
#include "PCGParamData.h"
#include "Async/Async.h"
#include "Components/InstancedStaticMeshComponent.h"

HOUDINI_PCG_DEFINE_LOG_CATEGORY();

FString FHoudiniPCGUtils::ParameterInputPinName = FString(TEXT("Parameters"));
FName FHoudiniPCGUtils::HDAInputObjectName = FName(FString(TEXT("object")));
FCriticalSection FHoudiniPCGUtils::CriticalSection;
EHoudiniPCGSessionStatus FHoudiniPCGUtils::SessionStatus;

void
FHoudiniPCGUtils::UnrealToHoudini(const FVector3d& UnrealVector, float HoudiniVector[3])
{
	HoudiniVector[0] = static_cast<float>(UnrealVector.X);
	HoudiniVector[1] = static_cast<float>(UnrealVector.Z);
	HoudiniVector[2] = static_cast<float>(UnrealVector.Y);
}

TArray<FHoudiniPCGObjectOutput>
FHoudiniPCGUtils::GetPCGOutputData(const FHoudiniBakedOutput* BakedOutput)
{
	TArray<FHoudiniPCGObjectOutput> Outputs;

	int ObjectIndex = 0;
	for(auto It : BakedOutput->BakedOutputObjects)
	{
		auto& BakedOutputObject = It.Value;

		FHoudiniPCGObjectOutput& PCGOutputObject = Outputs.Emplace_GetRef();
		PCGOutputObject.OutputObjectIndex = ObjectIndex;
		PCGOutputObject.ActorPath = BakedOutputObject.Actor;
		PCGOutputObject.ComponentPath = BakedOutputObject.BakedComponent;
		PCGOutputObject.ObjectPath = BakedOutputObject.BakedObject;

		if (PCGOutputObject.ActorPath.IsValid())
		{
			UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *PCGOutputObject.ActorPath.ToString());
			if(IsValid(LoadedObject))
				PCGOutputObject.OutputType = FHoudiniPCGUtils::GetTypeStringFromObject(LoadedObject);
		}

		if (PCGOutputObject.OutputType.IsEmpty())
		{
			if(PCGOutputObject.ObjectPath.IsValid())
			{
				UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *PCGOutputObject.ObjectPath.ToString());
				if(IsValid(LoadedObject))
					PCGOutputObject.OutputType = FHoudiniPCGUtils::GetTypeStringFromObject(LoadedObject);
			}
		}

		ObjectIndex++;
	}
	return Outputs;
}

FString FHoudiniPCGUtils::GetTypeStringFromObject(UObject * Object)
{
	if(Object->IsA<UStaticMesh>())
	{
		return TEXT("Mesh");
	}
	else if(Object->IsA<UHoudiniLandscapeTargetLayerOutput>() || Object->IsA<ALandscapeProxy>())
	{
		return TEXT("Landscape");
	}
	return TEXT("");
}

FString FHoudiniPCGUtils::GetTypeStringFromComponent(USceneComponent* Component)
{
	if(IsValid(Component))
	{
		if(IsValid(Component))
		{
			if(Component->IsA<UInstancedStaticMeshComponent>())
			{
				return TEXT("InstancedStaticMesh");
			}
		}
	}
	return TEXT("");
}


FString FHoudiniPCGUtils::GetTypeStringFromOutputObject(const FHoudiniOutputObject& OutputObject)
{
	if (IsValid(OutputObject.OutputObject))
	{
		FString Result = GetTypeStringFromObject(OutputObject.OutputObject);
		if(!Result.IsEmpty())
			return Result;
	}


	if (!OutputObject.OutputComponents.IsEmpty())
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(OutputObject.OutputComponents[0]);
		if (IsValid(SceneComponent))
		{
			FString Result = GetTypeStringFromComponent(SceneComponent);
			if(!Result.IsEmpty())
				return Result;
		}
	}

	return TEXT("");
}


TArray<FHoudiniPCGObjectOutput>
FHoudiniPCGUtils::GetPCGOutputData(const UHoudiniOutput* HoudiniOutput)
{
	TArray<FHoudiniPCGObjectOutput> Outputs;

	int ObjectIndex = 0;
	for(auto It : HoudiniOutput->GetOutputObjects())
	{
		FHoudiniOutputObject& OutputObj = It.Value;

		FHoudiniPCGObjectOutput& PCGOutputObject = Outputs.Emplace_GetRef();
		PCGOutputObject.OutputObjectIndex = ObjectIndex;
		PCGOutputObject.OutputType = GetTypeStringFromOutputObject(OutputObj);

		if (UHoudiniLandscapeTargetLayerOutput * LandscapeOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObj.OutputObject.Get()))
		{
			if (IsValid(LandscapeOutput->Landscape))
				PCGOutputObject.ActorPath = LandscapeOutput->Landscape->GetPathName();
			else if (IsValid(LandscapeOutput->LandscapeProxy))
				PCGOutputObject.ActorPath = LandscapeOutput->LandscapeProxy->GetPathName();
		}
		else
		{
			if(OutputObj.OutputObject)
			{
				PCGOutputObject.ObjectPath = OutputObj.OutputObject.GetPathName();
			}

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

		}

		ObjectIndex++;
	}
	return Outputs;
}

bool
FHoudiniPCGUtils::HasPCGOutputs(const UHoudiniOutput* HoudiniOutput)
{
	for (auto & It : HoudiniOutput->GetOutputObjects())
	{
		auto & Object = It.Value;

		if (!IsValid(Object.OutputObject))
			continue;

		if(Object.OutputObject->IsA<UHoudiniPCGOutputData>())
			return true;
	}
	return false;
}

bool
FHoudiniPCGUtils::HasPCGOutputs(const FHoudiniBakedOutput* HoudiniOutput)
{
	for(auto& It : HoudiniOutput->BakedOutputObjects)
	{
		auto& Object = It.Value;

		if(IsValid(Object.PCGOutputData))
			return true;
	}
	return false;
}


EHoudiniPCGInputType
FHoudiniPCGUtils::GetInputType(const UPCGData* PCGData)
{
	if(PCGData->IsA<UPCGPointData>())
		return EHoudiniPCGInputType::PCGData;
	else if(PCGData->IsA<UPCGParamData>())
	{
		const UPCGMetadata* Metadata = PCGData->ConstMetadata();

		const FPCGMetadataAttribute<FSoftObjectPath>* ObjectAttrs = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(Metadata->GetConstAttribute(HDAInputObjectName));
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

TArray<FString>
FHoudiniPCGUtils::GetValueAsString(const TArray<FString> & Defaults, const FHoudiniPCGAttributes& Attributes, int Index)
{
	// Always return at least one result.
	TArray<FString> Result = Defaults;
	if(Result.IsEmpty())
		Result.SetNum(1);

	if(Attributes.Strings)
	{
		Result[0] = Attributes.Strings->GetValueFromItemKey(Index);
	}
	else if(Attributes.Names)
	{
		Result[0] = Attributes.Names->GetValueFromItemKey(Index).ToString();
	}
	else if(Attributes.SoftObjectPaths)
	{
		Result[0] = Attributes.SoftObjectPaths->GetValueFromItemKey(Index).ToString();
	}
	else if(Attributes.SoftClassPaths)
	{
		Result[0] = Attributes.SoftObjectPaths->GetValueFromItemKey(Index).ToString();
	}
	else if (Attributes.Floats)
	{
		Result[0] = FString::SanitizeFloat(Attributes.Floats->GetValueFromItemKey(Index));
	}
	else if (Attributes.Int32s)
	{
		Result[0] = FString::FromInt(Attributes.Int32s->GetValueFromItemKey(Index));
	}
	else if(Attributes.Int64s)
	{
		Result[0] = FString::FromInt(static_cast<int>(Attributes.Int64s->GetValueFromItemKey(Index)));
	}
	else if(Attributes.Bools)
	{
		Result[0] = Attributes.Bools->GetValueFromItemKey(Index) ? FString(TEXT("1")) : FString(TEXT("0"));
	}
	return Result;
}

TArray<int>
FHoudiniPCGUtils::GetValueAsInt(const TArray<int> & Defaults, const FHoudiniPCGAttributes& Attributes, int Index)
{
	// Always return at least one result.
	TArray<int> Result = Defaults;
	if(Result.IsEmpty())
		Result.SetNum(1);

	if(Attributes.Strings)
	{
		FString StringValue = Attributes.Strings->GetValueFromItemKey(Index);
		Result[0] = FCString::Atoi(*StringValue);
	}
	else if(Attributes.Floats)
	{
		float FloatValue = Attributes.Floats->GetValueFromItemKey(Index);
		Result[0] = static_cast<int>(FloatValue);
	}
	else if(Attributes.Doubles)
	{
		Result[0] = static_cast<int>(Attributes.Doubles->GetValueFromItemKey(Index));
	}
	else if(Attributes.Int32s)
	{
		Result[0] = Attributes.Int32s->GetValueFromItemKey(Index);
	}
	else if(Attributes.Int64s)
	{
		Result[0] = static_cast<int32>(Attributes.Int64s->GetValueFromItemKey(Index));
	}
	return Result;
}

TArray<float>
FHoudiniPCGUtils::GetValueAsFloat(const TArray<float>& DefaultValues, const FHoudiniPCGAttributes& Attributes, int RowIndex)
{
	// Always return at least one result.
	TArray<float> Result = DefaultValues;
	if(Result.IsEmpty())
		Result.SetNum(1);

	if(Attributes.Strings)
	{
		FString StringValue = Attributes.Strings->GetValueFromItemKey(RowIndex);
		Result[0] = FCString::Atof(*StringValue);
	}
	else if(Attributes.Floats)
	{
		Result[0] = Attributes.Floats->GetValueFromItemKey(RowIndex);
	}
	else if(Attributes.Doubles)
	{
		Result[0] = static_cast<float>(Attributes.Doubles->GetValueFromItemKey(RowIndex));
	}
	else if(Attributes.Int32s)
	{
		int IntValue = Attributes.Int32s->GetValueFromItemKey(RowIndex);
		Result[0] = static_cast<float>(IntValue);
	}
	else if(Attributes.Int64s)
	{
		int Int64Value = Attributes.Int64s->GetValueFromItemKey(RowIndex);
		Result[0] = static_cast<float>(Int64Value);
	}
	else if(Attributes.Bools)
	{
		Result[0] = Attributes.Bools->GetValueFromItemKey(RowIndex) ? 1.0f : 0.0f;
	}
	else if(Attributes.Vector2ds)
	{
		// NOTE: We do not convert UE->H space because we don't know what this represents. a point? a vector?
		// something else?
		FVector2d Vec = Attributes.Vector2ds->GetValueFromItemKey(RowIndex);
		Result.SetNum(2);
		Result[0] = Vec.X;
		Result[1] = Vec.Y;
	}
	else if (Attributes.Vector3ds)
	{
		// NOTE: We do not convert UE->H space because we don't know what this represents. a point? a vector?
		// something else?
		FVector3d Vec = Attributes.Vector3ds->GetValueFromItemKey(RowIndex);
		Result.SetNum(3);
		Result[0] = Vec.X;
		Result[1] = Vec.Y;
		Result[2] = Vec.Z;
	}
	else if(Attributes.Vector4ds)
	{
		// NOTE: We do not convert UE->H space because we don't know what this represents. a point? a vector?
		// something else?
		FVector4d Vec = Attributes.Vector4ds->GetValueFromItemKey(RowIndex);
		Result.SetNum(4);
		Result[0] = Vec.X;
		Result[1] = Vec.Y;
		Result[2] = Vec.Z;
		Result[3] = Vec.W;
	}
	else if(Attributes.Quats)
	{
		// NOTE: We do not convert UE->H space because we don't know what this represents. a point? a vector?
		// something else?
		FQuat Quat = Attributes.Quats->GetValueFromItemKey(RowIndex);
		FVector4d Vec = FHoudiniPCGUtils::UnrealToHoudiniQuat(Quat);
		Result.SetNum(4);
		Result[0] = Vec.X;
		Result[1] = Vec.Y;
		Result[2] = Vec.Z;
		Result[3] = Vec.W;
	}
	else if(Attributes.Rotators)
	{
		FRotator Rotator = Attributes.Rotators->GetValueFromItemKey(RowIndex);
		Result.SetNum(3);
		Result[0] = Rotator.Roll;
		Result[1] = Rotator.Yaw;
		Result[2] = Rotator.Pitch;
	}

	return Result;
}

FHoudiniPCGAttributes::FHoudiniPCGAttributes(const UPCGMetadata* Metadata, const FName& ParameterName)
{
	// Cache off all attribute types we might be interested in.
	this->Int32s = Metadata->GetConstTypedAttribute<int>(ParameterName);
	this->Int64s = Metadata->GetConstTypedAttribute<int64>(ParameterName);
	this->Vector2ds = Metadata->GetConstTypedAttribute<FVector2d>(ParameterName);
	this->Vector3ds = Metadata->GetConstTypedAttribute<FVector>(ParameterName);
	this->Vector4ds = Metadata->GetConstTypedAttribute<FVector4d>(ParameterName);
	this->Floats = Metadata->GetConstTypedAttribute<float>(ParameterName);
	this->Doubles = Metadata->GetConstTypedAttribute<double>(ParameterName);
	this->Strings = Metadata->GetConstTypedAttribute<FString>(ParameterName);
	this->Bools = Metadata->GetConstTypedAttribute<bool>(ParameterName);
	this->Rotators = Metadata->GetConstTypedAttribute<FRotator>(ParameterName);
	this->Names = Metadata->GetConstTypedAttribute<FName>(ParameterName);
	this->SoftObjectPaths = Metadata->GetConstTypedAttribute<FSoftObjectPath>(ParameterName);
	this->SoftClassPaths = Metadata->GetConstTypedAttribute<FSoftClassPath>(ParameterName);
	this->Quats = Metadata->GetConstTypedAttribute<FQuat>(ParameterName);
	this->Bools = Metadata->GetConstTypedAttribute<bool>(ParameterName);
	this->NumRows = Metadata->GetItemCountForChild();
	this->Name = ParameterName.ToString();
}

FString
FHoudiniPCGUtils::GetHDAInputName(int Index)
{
	// Name of the pin that exposes HDA input "Index".
	FString PinName = FString::Printf(TEXT("Input %d"), Index);
	return PinName;
}

FVector3d
FHoudiniPCGUtils::HoudiniToUnrealPosition(float HoudiniVector[3])
{
	FVector3d Position;
	Position.X = HoudiniVector[0] * 100.0;
	Position.Y = HoudiniVector[2] * 100.0;
	Position.Z = HoudiniVector[1] * 100.0;
	return Position;
}

FVector3d
FHoudiniPCGUtils::HoudiniToUnrealVector(float HoudiniVector[3])
{
	FVector3d Position;
	Position.X = HoudiniVector[0];
	Position.Y = HoudiniVector[2];
	Position.Z = HoudiniVector[1];
	return Position;
}

FQuat
FHoudiniPCGUtils::HoudiniToUnrealQuat(float HoudiniQuat[4])
{
	FQuat Result(HoudiniQuat[0], HoudiniQuat[2], HoudiniQuat[1], -HoudiniQuat[3]);
	return Result;
}

FVector4d
FHoudiniPCGUtils::UnrealToHoudiniQuat(const FQuat& Quat)
{
	FVector4d Result;
	Result[0] = Quat.X;
	Result[1] = Quat.Z;
	Result[2] = Quat.Y;
	Result[3] = -Quat.W;
	return Result;
}

void
FHoudiniPCGUtils::LogVisualWarning(const FPCGContext* Context, const FString& WarningMessage)
{
	HOUDINI_LOG_ERROR(TEXT("Warning: %s"), *WarningMessage);
	FText Text = FText::FromString(WarningMessage);
	PCGE_LOG_C(Warning, GraphAndLog, Context, Text);
}

void  
FHoudiniPCGUtils::LogVisualError(const FPCGContext* Context, const TArray<FString>& ErrorMessages)  
{  
   FString CombinedErrors = FString::Join(ErrorMessages, TEXT("\n"));  
   LogVisualError(Context, *CombinedErrors);
}

void
FHoudiniPCGUtils::LogVisualError(const FPCGContext* Context,  const FString& ErrorMessage)
{
	HOUDINI_LOG_ERROR(TEXT("Error: %s"), *ErrorMessage);
	FText Text = FText::FromString(ErrorMessage);
	PCGE_LOG_C(Error, GraphAndLog, Context, Text);
}

EHoudiniPCGSessionStatus
FHoudiniPCGUtils::StartSession()
{
	if(FHoudiniEngine::Get().GetSession())
	{
		SessionStatus = EHoudiniPCGSessionStatus::PCGSessionStatus_Created;
		return SessionStatus;
	}
	bool bSuccess = FHoudiniEngine::Get().RestartSession(false);
	SessionStatus = bSuccess ? EHoudiniPCGSessionStatus::PCGSessionStatus_Created : EHoudiniPCGSessionStatus::PCGSessionStatus_Error;

	if(bSuccess)
	{
		HOUDINI_PCG_MESSAGE(TEXT("Session Created..."));
	}
	else
	{

		HOUDINI_PCG_ERROR(TEXT("Session Not Created..."));
	}
	return SessionStatus;
}

EHoudiniPCGSessionStatus
FHoudiniPCGUtils::StartSessionAsync()
{
	FScopeLock Lock(&CriticalSection);

	if (FHoudiniEngine::Get().GetSession())
	{
		SessionStatus = EHoudiniPCGSessionStatus::PCGSessionStatus_Created;
		return SessionStatus;
	}

	if (SessionStatus == EHoudiniPCGSessionStatus::PCGSessionStatus_Created)
	{
		// Make sure the session is still good.
		if(!FHoudiniEngine::Get().GetSession())
		{
			SessionStatus = EHoudiniPCGSessionStatus::PCGSessionStatus_None;
			HOUDINI_PCG_MESSAGE(TEXT("Houdini Session Lost..."));
		}
		else
		{
			return SessionStatus;
		}
	}

	if (SessionStatus == EHoudiniPCGSessionStatus::PCGSessionStatus_None)
	{
		HOUDINI_PCG_MESSAGE(TEXT("No Unreal-Houdini Session found, will try to establish one."));
		EHoudiniPCGSessionStatus* SessionStatusPtr = &SessionStatus;
		SessionStatus = EHoudiniPCGSessionStatus::PCGSessionStatus_Creating;
		Async(EAsyncExecution::ThreadPool, [SessionStatusPtr]()
		{
			bool bConnected = FHoudiniEngine::Get().ConnectSession(false);
			if(bConnected)
			{
				HOUDINI_PCG_MESSAGE(TEXT("Connection to existing Houdini Session."));
				SessionStatus = EHoudiniPCGSessionStatus::PCGSessionStatus_Created;
				return;
			}


			bool bSuccess = FHoudiniEngine::Get().RestartSession(false);
			*SessionStatusPtr = bSuccess ? EHoudiniPCGSessionStatus::PCGSessionStatus_Created : EHoudiniPCGSessionStatus::PCGSessionStatus_Error;
			if (bSuccess)
			{
				HOUDINI_PCG_MESSAGE(TEXT("Session Created."));
			}
			else
			{

				HOUDINI_PCG_ERROR(TEXT("Session Not Created."));
			}
		});
	}

	return SessionStatus;
}

UPCGComponent* FHoudiniPCGUtils::GetSourceComponent(FPCGContext* Context)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6

	return Cast<UPCGComponent>(Context->ExecutionSource.Get());
#else
	return Context->SourceComponent.IsValid() ? Context->SourceComponent.Get() : nullptr;
#endif

}

