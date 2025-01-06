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

#include "HAPI/HAPI_Common.h"
#include "HoudiniGeoPartObject.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/TextureDefines.h"
#include "Engine/EngineTypes.h"

#include <string>

#include "HoudiniMaterialTranslator.generated.h"

class UMaterial;
class UMaterialInterface;
class UMaterialExpression;
class UMaterialInstanceConstant;
class UTexture2D;
class UTexture;
class UPackage;
class UMaterialExpression;
class UMaterialExpressionMultiply;
class UMaterialExpressionVectorParameter;
class UMaterialExpressionScalarParameter;
class UMaterialExpressionVertexColor;
class UMaterialExpressionTextureSampleParameter2D;

struct FHoudiniPackageParams;
struct FCreateTexture2DParameters;
struct FHoudiniGenericAttribute;
struct FHoudiniMaterialIdentifier;

// Forward declared enums do not work with 4.24 builds on Linux with the Clang 8.0.1 toolchain: ISO C++ forbids forward references to 'enum' types
// enum TextureGroup;

// Enum of material parameter type for Houdini material parameter attributes.
UENUM()
enum class EHoudiniUnrealMaterialParameterType : uint8
{
	Invalid = 0,
	// The parameter is a standard parameter of the material instance (such as BlendMode, DiffuseBoost etc)
	StandardParameter,
	// The following are the types for custom parameters
	// A scalar material parameter
	Scalar,
	// A static switch (bool) parameter
	StaticSwitch,
	// A vector parameter (represented with a color)
	Vector,
	// A texture parameter (represented with a string)
	Texture,
};

// Material parameter data types from Houdini attributes
UENUM()
enum class EHoudiniUnrealMaterialParameterDataType : uint8
{
	Invalid = 0,
	// One byte, used for enums and bools
	Byte,
	// Floating point value (double is cast down to float)
	Float,
	// String
	String,
	// Vector represented as a RGB or RGBA linear color
	Vector
};

// Struct for storing material parameter type and value information from Houdini attributes after processing via
// FHoudiniMaterialTranslator::GetAndValidateMaterialInstanceParameterValue()
USTRUCT()
struct FHoudiniMaterialParameterValue
{
	GENERATED_BODY()

	FHoudiniMaterialParameterValue();

	// The parameter type (standard parameter, scalar, static switch, texture, vector)
	UPROPERTY()
	EHoudiniUnrealMaterialParameterType ParamType;

	// The data type (byte, float, string, vector)
	UPROPERTY()
	EHoudiniUnrealMaterialParameterDataType DataType; 

	// The value used if DataType is Byte
	UPROPERTY()
	uint8 ByteValue;

	// The value used if DataType is Float
	UPROPERTY()
	float FloatValue;

	// The value used if DataType is String
	UPROPERTY()
	FString StringValue;

	// The value used if DataType is Vector
	UPROPERTY()
	FLinearColor VectorValue;

	// Convenience functions for setting values
	void SetValue(const uint8 InValue);
	void SetValue(const float InValue);
	void SetValue(const FString& InValue);
	void SetValue(const FLinearColor& InValue);
	// Reset all *Value properties, other than the active DataType, to their default values.
	void CleanValue();
};

// Material info collected for each unreal_material / unreal_material_instance attribute value
USTRUCT()
struct HOUDINIENGINE_API FHoudiniMaterialInfo
{
	GENERATED_BODY()

	// Make a string that represents the map of MaterialInstanceParameters (used for hashing)
	FString MakeMaterialInstanceParametersSlug() const;

	// Make an identifier that uniquely identifies this material struct: includes the MaterialObjectPath,
	// bMakeMaterialInstance and the slug returned by MakeMaterialInstanceParametersSlug 
	FHoudiniMaterialIdentifier MakeIdentifier() const;

	// The object path the UMaterial
	UPROPERTY()
	FString MaterialObjectPath;

	// Whether a material instance should be created from MaterialObjectPath 
	UPROPERTY()
	bool bMakeMaterialInstance = false;

	// The material slot index
	UPROPERTY()
	int32 MaterialIndex = INDEX_NONE;

	// If bMakeMaterialInstance is true (a material instance must be made), then this is the material parameters to apply
	// to the instance.
	UPROPERTY()
	TMap<FName, FHoudiniMaterialParameterValue> MaterialInstanceParameters;
};


struct HOUDINIENGINE_API FHoudiniMaterialTranslator
{
public:

	// Helper function to handle difference with material expressions collection in UE5.1
	static void _AddMaterialExpression(
		UMaterial* InMaterial, UMaterialExpression* InMatExp);

	//
	static bool CreateHoudiniMaterials(
		const HAPI_NodeId& InNodeId,
		const FHoudiniPackageParams& InPackageParams,
		const TArray<int32>& InUniqueMaterialIds,
		const TArray<HAPI_MaterialInfo>& InUniqueMaterialInfos,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutMaterials,
		TArray<UMaterialInterface*> & OutMaterialArray,
		TArray<UPackage*>& OutPackages,
		const bool& bForceRecookAll,
		bool bInTreatExistingMaterialsAsUpToDate=false,
		bool bAddDefaultMaterial=true);

	//
	static bool CreateMaterialInstances(
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams,
		const TMap<FHoudiniMaterialIdentifier, FHoudiniMaterialInfo>& UniqueMaterialInstanceOverrides,
		const TArray<UPackage*>& InPackages,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InMaterials,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutMaterials,
		const bool& bForceRecookAll);


	// Helper for getting material parameters from Houdini attributes (detail + InAttributeOwner (prim or point)).
	static bool GetMaterialParameterAttributes(
		int32 InGeoId,
		int32 InPartId,
		HAPI_AttributeOwner InAttributeOwner,
		TArray<FHoudiniGenericAttribute>& OutAllMatParams,
		const int32 InAttributeIndex=-1);

	// Helper for getting material parameters for a material instance from attributes via HAPI
	static bool GetMaterialParameters(
		FHoudiniMaterialInfo& MaterialInfo,
		const TArray<FHoudiniGenericAttribute>& InAllMatParams,
		int32 InAttributeIndex);

	// Helper for getting material parameters for material instances from attributes via HAPI
	static bool GetMaterialParameters(
		TArray<FHoudiniMaterialInfo>& MaterialsByAttributeIndex,
		int32 InGeoId,
		int32 InPartId,
		HAPI_AttributeOwner InAttributeOwner);

	// Helper for CreateMaterialInstances so you don't have to sort unique face materials overrides
	static bool SortUniqueFaceMaterialOverridesAndCreateMaterialInstances(
		const TArray<FHoudiniMaterialInfo>& Materials,
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams,
		const TArray<UPackage*>& InPackages,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InMaterials,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutMaterials,
		const bool& bForceRecookAll);

	// Helper to check if MaterialParameter is supported by instances of MaterialInterface and to resolve the value to
	// apply from MaterialParameter. This is tightly coupled with UpdateMaterialInstanceParameter(): changes to the one
	// function likely require changes to the other!
	static bool GetAndValidateMaterialInstanceParameterValue(
		const FName& InMaterialParameterName,
		const FHoudiniGenericAttribute& MaterialParameterAttribute,
		const int32 InAttributeIndex,
		UMaterialInterface* MaterialInterface,
		FHoudiniMaterialParameterValue& OutMaterialParameterValue);

	// Update a material instance parameter. This is tightly coupled with GetAndValidateMaterialInstanceParameterValue():
	// changes to the one function likely require changes to the other!
	static bool UpdateMaterialInstanceParameter(
		const FName& InMaterialParameterName,
		const FHoudiniMaterialParameterValue& InMaterialParameterValue,
		UMaterialInstanceConstant* MaterialInstance,
		const TArray<UPackage*>& InPackages);

	static UTexture* FindGeneratedTexture(
		const FString& TextureString,
		const TArray<UPackage*>& InPackages);

	//
	static UPackage* CreatePackageForMaterial(
		const HAPI_NodeId& InMaterialNodeId,
		const FString& InMaterialName,
		const FHoudiniPackageParams& InPackageParams,
		FString& OutMaterialName);

	static void GetTextureAndExpression(
		UMaterialExpression*& MatInputExpression,
		const bool bLocateExpression,
		UTexture2D*& OutTexture,
		UMaterialExpressionTextureSampleParameter2D*& OutExpression);

	// Create a scalar expression in the material graph, reusing the existing expression if possible.
	static UMaterialExpressionScalarParameter* CreateScalarExpression(
		UMaterialExpression* ExistingExpression,
		UMaterial* Material,
		const EObjectFlags& ObjectFlag,
		const FString& GeneratingParameterName);

	// Create a vertex color expression in the material graph, reusing the existing expression if possible.
	static UMaterialExpressionVertexColor* CreateVertexColorExpression(
		UMaterialExpression* ExistingExpression,
		UMaterial* Material,
		const EObjectFlags& ObjectFlag,
		const FString& GeneratingParameterName);

	// Create a color expression in the material graph, reusing the existing expression if possible.
	static UMaterialExpressionVectorParameter* CreateColorExpression(
		UMaterialExpression* ExistingExpression,
		UMaterial* Material,
		const EObjectFlags& ObjectFlag);

	// Set a color expression's color to that of a HAPI color parameter.
	// Records the GeneratingParameterName if the HAPI parameter is found.
	// Returns true if successfully set, false otherwise.
	static bool SetColorExpression(
		const HAPI_NodeId& NodeId,
		const char* ParamName,
		const char* ParamTag,
		const char* ParamCPMConst,
		const char* ParamCPMDefault,
		const char* ParamCPMSwitch,
		UMaterialExpressionVectorParameter* ColorExpression,
		FString& GeneratingParameterName);

	// Connect expressions A, B, and optionally C with multiply expressions.
	// If C is not provided, creates one multiply (A*B). Otherwise, creates two ((A*B)*C).
	// Returns the last multiply expression in the chain.
	static UMaterialExpressionMultiply* CreateMultiplyExpressions(
		UMaterialExpression* MatInputExpression,
		UMaterialExpression* ExpressionA,
		UMaterialExpression* ExpressionB,
		UMaterialExpression* ExpressionC,
		UMaterial* Material,
		int32& MaterialNodeY,
		const EObjectFlags& ObjectFlag);

	// Creates a scalar parameter expression from a HAPI float parameter.
	// Returns true if the expression was successfully created. False otherwise.
	static bool CreateScalarExpressionFromFloatParam(
		HAPI_NodeId Node,
		const char* ParamName,
		const char* ParamTag,
		const char* ParamCPMConst,
		const char* ParamCPMDefault,
		const char* ParamCPMSwitch,
		UMaterialExpression*& ExistingExpression,
		UMaterial* Material,
		int32& MaterialNodeY,
		const EObjectFlags& ObjectFlag);

	// Positions the expressions in the material graph.
	static void PositionExpression(
		UMaterialExpression* Expression,
		int32& MaterialNodeY,
		const float HorizontalPositionScale);

	// Determines if world space normals are required for the material created from this node.
	static bool RequiresWorldSpaceNormals(HAPI_NodeId HapiMaterial);

	// Returns a unique name for a given material, its relative path (to the asset)
	static bool GetMaterialRelativePath(
		const HAPI_NodeId& InAssetId, const HAPI_MaterialInfo& InMaterialNodeInfo, FString& OutRelativePath);
	static bool GetMaterialRelativePath(
		const HAPI_NodeId& InAssetId, const HAPI_NodeId& InMaterialNodeId, FString& OutRelativePath);

	// Finds a HAPI parameter (which represents a constant value for a plane) based on its name/tag.
	// Returns its ParmId, ParmInfo, and sets the GeneratingParameterName.
	static HAPI_ParmId FindConstantParam(
		const HAPI_NodeId& NodeId,
		const char* Name,
		const char* Tag,
		const char* CPMConst,
		const char* CPMDefault,
		const char* CPMSwitch,
		HAPI_ParmInfo& Info,
		FString& GeneratingParameterName);

	// Finds a HAPI texture parameter based on its name/tag.
	// Returns its ParmId, ParmInfo, and sets the GeneratingParameterName.
	static HAPI_ParmId FindTextureParam(
		const HAPI_NodeId& NodeId,
		const char* Name,
		const char* NameEnabled,
		const char* Tag,
		const char* TagEnabled,
		const char* CPMName,
		const char* CPMSwitch,
		HAPI_ParmInfo& TextureInfo,
		FString& GeneratingParameterName);

	// Returns true if a texture parameter was found
	// Ensures that the texture is not disabled via the "UseTexture" Parm name/tag
	static bool FindTextureParamByNameOrTag(
		const HAPI_NodeId& InNodeId,
		const std::string& InTextureParmName,
		const std::string& InUseTextureParmName,
		const bool& bFindByTag,
		const bool& bIsCPM,
		HAPI_ParmId& OutParmId,
		HAPI_ParmInfo& OutParmInfo);

protected:

	// Helper function to locate first Material expression of given class within given expression subgraph.
	static UMaterialExpression* MaterialLocateExpression(UMaterialExpression* Expression, UClass* ExpressionClass);

	// Assigns a texture to an expression in the Unreal material graph.
	// Returns true if the expression is successfully created, false otherwise.
	static bool CreateTextureExpression(
		UTexture2D* Texture,
		UMaterialExpressionTextureSampleParameter2D*& TextureExpression,
		UMaterialExpression*& MatInputExpression,
		const bool SetMatInputExpression,
		UMaterial* Material,
		const EObjectFlags ObjectFlag,
		const FString& GeneratingParameterName,
		const EMaterialSamplerType SamplerType);

	// Create various material components.
	static bool CreateMaterialComponentDiffuse(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName, 
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentNormal(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentSpecular(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentRoughness(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentMetallic(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentEmissive(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentOpacity(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

	static bool CreateMaterialComponentOpacityMask(
		const HAPI_NodeId& InAssetId,
		const FString& InMaterialName,
		const HAPI_MaterialInfo& InMaterialInfo,
		const FHoudiniPackageParams& InPackageParams,
		UMaterial* Material,
		TArray<UPackage*>& OutPackages,
		int32& MaterialNodeY);

public:

	// Material node construction offsets.
	static const int32 MaterialExpressionNodeX;
	static const int32 MaterialExpressionNodeY;
	static const int32 MaterialExpressionNodeStepX;
	static const int32 MaterialExpressionNodeStepY;
};