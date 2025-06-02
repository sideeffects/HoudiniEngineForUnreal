/* Copyright (c) <2025> Side Effects Software Inc.
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

#pragma once
#include "HoudiniOutput.h"
#include "HoudiniPCGComponent.h"
#include "PCGManagedResource.h"
#include "Data/PCGPointData.h"
#include "HAPI/HAPI_Common.h"


class UHoudiniInput;

struct FHoudiniPCGObjectOutput
{
	// Data to be output on the output pin of the executed node.
	int OutputObjectIndex = 0;
	FSoftObjectPath ComponentPath;
	FSoftObjectPath ActorPath;
	FSoftObjectPath ObjectPath;
	FString OutputType;
};

enum class EHoudiniPCGInputType
{
	None,
	UnrealObjects,
	PCGData
};

enum class EHoudiniPCGSessionStatus
{
	PCGSessionStatus_None,
	PCGSessionStatus_Creating,
	PCGSessionStatus_Created,
	PCGSessionStatus_Error
};
struct FHoudiniPCGAttributes
{
	FHoudiniPCGAttributes(const UPCGMetadata* Metadata, const FName & ParameterNames);

	FString Name;
	int NumRows;
	const FPCGMetadataAttribute<float>* Floats;
	const FPCGMetadataAttribute<double>* Doubles;
	const FPCGMetadataAttribute<int32>* Int32s;
	const FPCGMetadataAttribute<int64>* Int64s;
	const FPCGMetadataAttribute<FVector2d>* Vector2ds;
	const FPCGMetadataAttribute<FVector>* Vector3ds;
	const FPCGMetadataAttribute<FVector4d>* Vector4ds;
	const FPCGMetadataAttribute<FQuat>* Quats;
	const FPCGMetadataAttribute<FString>* Strings;
	const FPCGMetadataAttribute<bool>* Bools;
	const FPCGMetadataAttribute<FRotator>* Rotators;
	const FPCGMetadataAttribute<FName>* Names;
	const FPCGMetadataAttribute<FSoftObjectPath>* SoftObjectPaths;
	const FPCGMetadataAttribute<FSoftClassPath>* SoftClassPaths;
};

class UHoudiniPCGCookable;

class HOUDINIENGINE_API FHoudiniPCGUtils
{
public:

	static FString ParameterInputPinName;;
	static FName HDAInputObjectName;

	static EHoudiniPCGSessionStatus StartSessionAsync();
	static EHoudiniPCGSessionStatus StartSession();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Conversion functions.
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static void UnrealToHoudini(const FVector3d& UnrealVector, float HoudiniVector[3]);
	static FVector4d UnrealToHoudiniQuat(const FQuat& Quat);
	static FVector3d HoudiniToUnrealPosition(float HoudiniVector[3]);
	static FVector3d HoudiniToUnrealVector(float HoudiniVector[3]);
	static FQuat HoudiniToUnrealQuat(float HoudiniVector[4]);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Input Functions.
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static EHoudiniPCGInputType GetInputType(const UPCGData* TaggedData);

	static FString GetHDAInputName(int Index);

	static bool HasPCGOutputs(const UHoudiniOutput* HoudiniOutput);
	static bool HasPCGOutputs(const FHoudiniBakedOutput* HoudiniOutput);

	static TArray<FHoudiniPCGObjectOutput> GetPCGOutputData(const UHoudiniOutput * HoudiniOutput);
	static TArray<FHoudiniPCGObjectOutput> GetPCGOutputData(const FHoudiniBakedOutput * BakedOutput);
	static TArray<FHoudiniPCGObjectOutput> GetPCGOutputData(const FTOPWorkResult* WorkItem);

	static TArray<FString> GetValueAsString(const TArray<FString>& DefaultValues, const FHoudiniPCGAttributes & Attributes, int RowIndex);
	static TArray<int> GetValueAsInt(const TArray<int>& DefaultValues, const FHoudiniPCGAttributes& Attributes, int RowIndex);
	static TArray<float> GetValueAsFloat(const TArray<float> & DefaultValues, const FHoudiniPCGAttributes& Attributes, int RowIndex);

	static void LogVisualWarning(const FPCGContext* Context, const FString& ErrorText);
	static void LogVisualError(const FPCGContext* Context, const FString & ErrorText);
	static void LogVisualError(const FPCGContext* Context, const TArray<FString> & Errors);

	static EHoudiniPCGSessionStatus SessionStatus;

	static FString GetTypeStringFromOutputObject(const FHoudiniOutputObject & OutputObject);

	static FString GetTypeStringFromObject(UObject* Object);
	static FString GetTypeStringFromComponent(USceneComponent* Component);

	static UPCGComponent* GetSourceComponent(FPCGContext* Context);

private:
	// Synchronization primitive. 
	static FCriticalSection CriticalSection;

};
