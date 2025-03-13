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

struct FHoudiniPCGAttribute
{
	FString Name;
	HAPI_AttributeInfo AttributeInfo;
	EPCGMetadataTypes PCGStorageType = EPCGMetadataTypes::Unknown;
};

struct FHoudiniPCGNodeName
{
	FString UnLoopedName;
	FString LoopedName;
};

struct FHoudiniPCGObjectOutput
{
	int OutputIndex = -1;
	FSoftObjectPath ComponentPath;
	FSoftObjectPath ActorPath;
	FSoftObjectPath ObjectPath;
};

enum class EHoudiniPCGInputType
{
	None,
	UnrealObjects,
	PCGData
};

struct FHoudiniPCGAttributes
{
	FHoudiniPCGAttributes(const UPCGMetadata* Metadata, const FName & ParameterNames);

	const FPCGMetadataAttribute<int >* Ints;
	const FPCGMetadataAttribute<float>* Floats;
	const FPCGMetadataAttribute<double>* Doubles;
	const FPCGMetadataAttribute<FString>* Strings;
	const FPCGMetadataAttribute<FSoftObjectPath>* SoftObjectPaths;
	int NumRows;
};

class UHoudiniPCGCookable;

class HOUDINIENGINE_API FHoudiniPCGUtils
{
public:

	static FString ParameterInputPinName;;

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

	static bool HasPCGOutputs(const UHoudiniOutput* HoudiniOutputs);

	static TArray<FHoudiniPCGObjectOutput> GetPCGOutputData(const UHoudiniOutput * HoudiniOutput);

	static bool GetValueAsString(FString& Result, int Index, const FHoudiniPCGAttributes & Attributes);
	static bool GetValueAsInt(int& Result, int Index, const FHoudiniPCGAttributes& Attributes);
	static bool GetValueAsFloat(float& Result, int Index, const FHoudiniPCGAttributes& Attributes);
};
