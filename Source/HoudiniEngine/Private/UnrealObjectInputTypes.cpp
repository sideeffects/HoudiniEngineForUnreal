#include "UnrealObjectInputTypes.h"

#include "HoudiniDataLayerUtils.h"
#include "UnrealObjectInputManager.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniHLODLayerUtils.h"
#include "StaticMeshResources.h"

#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "WorldPartition/HLOD/HLODLayer.h"

HAPI_NodeId
EnsureHAPINodeExistsInternal(
	const HAPI_NodeId InParentNetworkNodeId,
	const FString& InOpTypeName,
	const FString& InNodeName,
	TArray<FUnrealObjectInputHAPINodeId>& InHAPINodeIds,
	const int32 InIndex)
{
	// Check that InNodeIdToConnectTo is valid
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InParentNetworkNodeId))
		return false;

	// Check if we already have a valid node, if not create it
	FUnrealObjectInputHAPINodeId ExistingNodeId;
	if (InHAPINodeIds.IsValidIndex(InIndex))
		ExistingNodeId = InHAPINodeIds[InIndex];
	if (!ExistingNodeId.IsValid())
	{
		static constexpr bool bCookOnCreation = false;
		HAPI_NodeId NewNodeId = -1;
		if (FHoudiniEngineUtils::CreateNode(
				InParentNetworkNodeId, InOpTypeName, InNodeName, bCookOnCreation, &NewNodeId) != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the node.
			HOUDINI_LOG_WARNING(
				TEXT("Failed to create %s node: %s"), *InNodeName, *FHoudiniEngineUtils::GetErrorDescription());
			return INDEX_NONE;
		}
		ExistingNodeId.Set(NewNodeId);
		while (InHAPINodeIds.Num() <= InIndex)
			InHAPINodeIds.AddDefaulted();
		InHAPINodeIds[InIndex] = ExistingNodeId;
	}

	return ExistingNodeId.GetHAPINodeId();
}


HAPI_NodeId
FUnrealObjectInputMaterialOverrides::EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(bUsePrimWrangle ? TEXT("attribwrangle") : TEXT("attribcreate"));
	const FString NodeName(TEXT("material_overrides")); 
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}

bool
FUnrealObjectInputMaterialOverrides::UpdateAsPrimWrangle(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(MeshComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	// Check that InNodeIdToConnectTo is valid
	if (!InNodeIdToConnectTo.IsValid())
		return false;

	const HAPI_NodeId HAPINodeIdtoConnectTo = InNodeIdToConnectTo.GetHAPINodeId();
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId MaterialOverridesNodeId = EnsureHAPINodeExists(
		FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdtoConnectTo));
	if (MaterialOverridesNodeId < 0)
		return false;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		Session,
		MaterialOverridesNodeId, 0, HAPINodeIdtoConnectTo, 0), false);

	// Set group to exclude applying the material overrides to collision geo
	HAPI_ParmInfo GroupParmInfo;
	HAPI_ParmId GroupParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "group", GroupParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(Session, MaterialOverridesNodeId, "* ^collision_*", GroupParmId, 0), false);
	// Set grouptype to primitive
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, MaterialOverridesNodeId, "grouptype", 0, 4), false);

	// Construct a VEXpression to set create and set material override attributes.
	// eg. s@unreal_material1 = 'MyPath/MyMaterial';
	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	TArray<FString> MaterialPaths;
	MaterialPaths.Reserve(NumMaterials);
	UMaterialInterface const* const DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	const FString DefaultMaterialPathName = IsValid(DefaultMaterial) ? DefaultMaterial->GetPathName() : TEXT("default");
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		UMaterialInterface const* const Material = MeshComponent->GetMaterial(MaterialIndex);
		MaterialPaths.Add(FString::Format(TEXT("\"{0}\""), { IsValid(Material) ? Material->GetPathName() : DefaultMaterialPathName }));
	}

	const FString MaterialPathsString = FString::Join(MaterialPaths, TEXT(",\n    "));
	FString VEXpression = \
R"(// Material overrides from component by slot index
string material_overrides[] = {
    {0}
};
// Number of overrides
int num_slots = len(material_overrides);

// Don't change any material assignments if we have no overrides
if (num_slots <= 0)
    return;

// If there is only one slot we can set all primitives to the override
if (num_slots == 1) {
    s@unreal_material = material_overrides[0];
    return;
}

// Each material attribute value is prefixed with the slot number
// get the slot number for the current primitive
string material = s@unreal_material;
string slot_str = re_find(r"\[\d+\]", material);
if (strlen(slot_str) == 0)
    return;
int material_slot = atoi(slot_str[1:-1]);
// Check that material_slot is in range
if (material_slot < 0 || material_slot >= num_slots)
	return;
// Set the material to the override from the component for slot # "material_slot"
// and keep the slot prefix
s@unreal_material = "[" + itoa(material_slot) + "]" + material_overrides[material_slot];)";
	VEXpression = FString::Format(*VEXpression, { MaterialPathsString });

	// aet the wrangle's class to primitives
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, MaterialOverridesNodeId, "class", 0, 1), false);

	// Set the snippet parameter to the VEXpression.
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "snippet", ParmInfo);
	if (ParmId != -1)
	{
		FHoudiniApi::SetParmStringValue(Session, MaterialOverridesNodeId,
			H_TCHAR_TO_UTF8(*VEXpression), ParmId, 0);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	return true;
}


bool
FUnrealObjectInputMaterialOverrides::UpdateAsPointAttribCreate(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(MeshComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	// Check that InNodeIdToConnectTo is valid
	if (!InNodeIdToConnectTo.IsValid())
		return false;

	const HAPI_NodeId HAPINodeIdToConnectTo = InNodeIdToConnectTo.GetHAPINodeId();
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId MaterialOverridesNodeId = EnsureHAPINodeExists(
		FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdToConnectTo));
	if (MaterialOverridesNodeId < 0)
		return false;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		Session,
		MaterialOverridesNodeId, 0, HAPINodeIdToConnectTo, 0), false);

	// Get the default material for in case we encounter invalid materials
	UMaterialInterface const* const DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	const FString DefaultMaterialPathName = IsValid(DefaultMaterial) ? DefaultMaterial->GetPathName() : TEXT("default");

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	FHoudiniApi::SetParmIntValue(
		Session, MaterialOverridesNodeId, "numattr", 0, NumMaterials);
	HAPI_ParmInfo ParmInfo;
	HAPI_PartId ParmId;

	for (int32 MatNum = 1; MatNum <= NumMaterials; ++MatNum)
	{
		UMaterialInterface const* const Material = MeshComponent->GetMaterial(MatNum - 1);
		FString MatName(TEXT(HAPI_UNREAL_ATTRIB_MATERIAL));
		if (NumMaterials > 1)
			MatName += FString::FromInt(MatNum - 1);

		// Get material path name
		const FString MaterialPathName = IsValid(Material) ? Material->GetPathName() : DefaultMaterialPathName;
	
		// parm name is one indexed
		ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, TCHAR_TO_ANSI(*FString::Printf(TEXT("name%d"), MatNum)), ParmInfo);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmStringValue(Session, MaterialOverridesNodeId, TCHAR_TO_ANSI(*MatName), ParmId, 0), false);
	
		// set attribute type to string (index 3)
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmIntValue(Session, MaterialOverridesNodeId, TCHAR_TO_ANSI(*FString::Printf(TEXT("type%d"), MatNum)), 0, 3), false);
	
		// set value to path of material
		ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, TCHAR_TO_ANSI(*FString::Printf(TEXT("string%d"), MatNum)), ParmInfo);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmStringValue(Session, MaterialOverridesNodeId, TCHAR_TO_ANSI(*MaterialPathName), ParmId, 0), false);
	}

	return true;
}


void
FUnrealObjectInputMaterialOverrides::SetMeshComponent(UMeshComponent* const InMeshComponent)
{
	if (InMeshComponent == MeshComponent)
		return;
	
	MeshComponent = InMeshComponent;
	MarkAsNeedsRebuild();
}


void
FUnrealObjectInputMaterialOverrides::SetUsePrimWrangle(const bool bInUsePrimWrangle)
{
	if (bInUsePrimWrangle == bUsePrimWrangle)
		return;
	bUsePrimWrangle = bInUsePrimWrangle;
	MarkAsNeedsRebuild();
}


bool
FUnrealObjectInputMaterialOverrides::Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component, destroy all nodes and return false
	if (!IsValid(MeshComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	// Remove existing nodes if rebuilding
	if (bNeedsRebuild)
		DestroyHAPINodes();
	
	bool bSuccess = false;
	if (!bUsePrimWrangle)
		bSuccess = UpdateAsPointAttribCreate(InNodeIdToConnectTo);
	else
		bSuccess = UpdateAsPrimWrangle(InNodeIdToConnectTo);

	if (bSuccess)
		bNeedsRebuild = false;

	return bSuccess;
}


void
FUnrealObjectInputPhysicalMaterialOverride::SetPrimitiveComponent(UPrimitiveComponent* const InPrimitiveComponent)
{
	if (InPrimitiveComponent == PrimitiveComponent)
		return;
	PrimitiveComponent = InPrimitiveComponent;
	MarkAsNeedsRebuild();
}


void
FUnrealObjectInputPhysicalMaterialOverride::SetAttributeOwner(const HAPI_AttributeOwner InAttributeOwner)
{
	if (InAttributeOwner == AttributeOwner)
		return;
	AttributeOwner = InAttributeOwner;
	MarkAsNeedsRebuild();
}


HAPI_NodeId
FUnrealObjectInputPhysicalMaterialOverride::EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(TEXT("attribcreate"));
	const FString NodeName(TEXT("physical_material_override")); 
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}


bool
FUnrealObjectInputPhysicalMaterialOverride::Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// Check that InNodeIdToConnectTo is valid
	if (!InNodeIdToConnectTo.IsValid())
		return false;

	if (!IsValid(PrimitiveComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	// Remove existing nodes if rebuilding
	if (bNeedsRebuild)
		DestroyHAPINodes();

	const HAPI_NodeId HAPINodeIdToConnectTo = InNodeIdToConnectTo.GetHAPINodeId();
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId PhysMatOverrideNodeId = EnsureHAPINodeExists(
		FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdToConnectTo));
	if (PhysMatOverrideNodeId < 0)
		return false;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		Session, PhysMatOverrideNodeId, 0, HAPINodeIdToConnectTo, 0), false);

	// Set the number of attributes: 1
	FHoudiniApi::SetParmIntValue(Session, PhysMatOverrideNodeId, "numattr", 0, 1);

	// Get the attribute class
	int AttrClass = 1;
	switch (AttributeOwner)
	{
		case HAPI_ATTROWNER_VERTEX:
			AttrClass = 3;
			break;
	    case HAPI_ATTROWNER_POINT:
			AttrClass = 2;
			break;
	    case HAPI_ATTROWNER_PRIM:
			AttrClass = 1;
			break;
	    case HAPI_ATTROWNER_DETAIL:
			AttrClass = 0;
			break;
		default:
			HOUDINI_LOG_WARNING(TEXT("Unsupported value for attribute class: %d"), AttributeOwner);
			return false;
	}
	
	UPhysicalMaterial const* PhysMat = nullptr;
	PhysMat = PrimitiveComponent->BodyInstance.GetSimplePhysicalMaterial();
	
	// If the material is invalid then the path is empty string and we disable the attribute on the node
	FString MaterialPath("");
	bool bEnable = false;
	if (IsValid(PhysMat) && PhysMat != GEngine->DefaultPhysMaterial)
	{
		// If the material is valid get its path
		MaterialPath = PhysMat->GetPathName();
		bEnable = true;
	}

	// Set enable
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, PhysMatOverrideNodeId, "enable1", 0, bEnable), false);

	// Set attribute class
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, PhysMatOverrideNodeId, "class1", 0, AttrClass), false);

	// Set the attribcreate attribute name
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(PhysMatOverrideNodeId, "name1", ParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(Session, PhysMatOverrideNodeId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL, ParmId, 0), false);

	// set attribute type to string (index 3)
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, PhysMatOverrideNodeId, "type1", 0, 3), false);

	// set value to path of material
	ParmId = FHoudiniEngineUtils::HapiFindParameterByName(PhysMatOverrideNodeId, "string1", ParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(Session, PhysMatOverrideNodeId, TCHAR_TO_ANSI(*MaterialPath), ParmId, 0), false);

	bNeedsRebuild = false;

	return true;
}


void
FUnrealObjectInputActorAsReference::SetActor(AActor* const InActor)
{
	if (InActor == Actor)
		return;
	
	Actor = InActor;
	MarkAsNeedsRebuild();
}


HAPI_NodeId
FUnrealObjectInputActorAsReference::EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(TEXT("attribwrangle"));
	const FString NodeName(TEXT("actor_reference_attributes")); 
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}


bool
FUnrealObjectInputActorAsReference::Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(Actor))
	{
		DestroyHAPINodes();
		return false;
	}

	// Check that InNodeIdToConnectTo is valid
	if (!InNodeIdToConnectTo.IsValid())
		return false;

	const HAPI_NodeId HAPINodeIdToConnectTo = InNodeIdToConnectTo.GetHAPINodeId();
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId MaterialOverridesNodeId = EnsureHAPINodeExists(
		FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdToConnectTo));
	if (MaterialOverridesNodeId < 0)
		return false;

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		Session, MaterialOverridesNodeId, 0, HAPINodeIdToConnectTo, 0), false);

	// Extract the level path from the level
	FString LevelPath("");
	if (IsValid(Actor->GetLevel()))
	{
		LevelPath = Actor->GetLevel()->GetPathName();
		// We just want the path up to the first point
		int32 DotIndex;
		if (LevelPath.FindChar('.', DotIndex))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			LevelPath.LeftInline(DotIndex, EAllowShrinking::No);
#else
			LevelPath.LeftInline(DotIndex, false);
#endif
	}

	// Construct a VEXpression to set create and set level path and actor path attributes
	const FString VEXpressionFormat = \
R"(s@{0} = "{1}";
s@{2} = "{3}";)";
	const FString VEXpression = FString::Format(*VEXpressionFormat, {
		HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPath,
		HAPI_UNREAL_ATTRIB_ACTOR_PATH, Actor->GetPathName()
	});

	// Set the wrangle's class to points
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, MaterialOverridesNodeId, "class", 0, 2), false);

	// Set the snippet parameter to the VEXpression.
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "snippet", ParmInfo);
	if (ParmId != -1)
	{
		FHoudiniApi::SetParmStringValue(Session, MaterialOverridesNodeId, H_TCHAR_TO_UTF8(*VEXpression), ParmId, 0);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	return true;
}

void
FUnrealObjectInputDataLayer::SetActor(AActor* const InActor)
{
	if (InActor == Actor)
		return;

	Actor = InActor;
	MarkAsNeedsRebuild();
}


HAPI_NodeId
FUnrealObjectInputDataLayer::EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(TEXT("attribwrangle"));
	const FString NodeName(TEXT("unreal_data_layers"));
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}


bool
FUnrealObjectInputDataLayer::Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(Actor))
	{
		DestroyHAPINodes();
		return false;
	}

	// Check that InNodeIdToConnectTo is valid
	if (!InNodeIdToConnectTo.IsValid())
		return false;

	const HAPI_NodeId HAPINodeIdToConnectTo = InNodeIdToConnectTo.GetHAPINodeId();

	// Check if we already have a valid node, if not create it
	const HAPI_NodeId VexNodeId = EnsureHAPINodeExists(
		FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdToConnectTo));
	if (VexNodeId < 0)
		return false;

	const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		Session, VexNodeId, 0, HAPINodeIdToConnectTo, 0), false);

	FHoudiniDataLayerUtils::SetVexCode(VexNodeId, Actor);

	return true;
}


void
FUnrealObjectInputHLODAttributes::SetActor(AActor* const InActor)
{
	if (InActor == Actor)
		return;

	Actor = InActor;
	MarkAsNeedsRebuild();
}


HAPI_NodeId
FUnrealObjectInputHLODAttributes::EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(TEXT("attribwrangle"));
	const FString NodeName(TEXT("unreal_hlod_attributes"));
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}


bool
FUnrealObjectInputHLODAttributes::Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(Actor))
	{
		DestroyHAPINodes();
		return false;
	}

	// Check that InNodeIdToConnectTo is valid
	if (!InNodeIdToConnectTo.IsValid())
		return false;

	const HAPI_NodeId HAPINodeIdToConnectTo = InNodeIdToConnectTo.GetHAPINodeId();

	// Check if we already have a valid node, if not create it
	const HAPI_NodeId VexNodeId = EnsureHAPINodeExists(FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdToConnectTo));

	if (VexNodeId < 0)
		return false;

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, VexNodeId, 0, HAPINodeIdToConnectTo, 0), false);

	// Set the wrangle's class to prims
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, VexNodeId, "class", 0, 1), false);

	FHoudiniHLODLayerUtils::SetVexCode(VexNodeId, Actor);

	return true;
}

HAPI_NodeId
FUnrealObjectInputActorProperties::EnsureHAPINodeExists(HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(TEXT("attribwrangle"));
	const FString NodeName(TEXT("actor_properties"));
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}

TArray<FHoudiniMeshMaterialInfo> FUnrealObjectInputActorProperties::GetMaterialOverrides(UStaticMeshComponent* MeshComponent)
{
	TArray<FHoudiniMeshMaterialInfo> MaterialInfos;
	MaterialInfos.SetNum(MeshComponent->GetNumMaterials());

	for(int Index = 0; Index < MeshComponent->GetNumMaterials(); Index++)
	{
		UMaterialInterface* Material = MeshComponent->OverrideMaterials.IsValidIndex(Index) ? MeshComponent->OverrideMaterials[Index] : nullptr;
		if(!IsValid(Material))
			continue;

		TArray<FMaterialParameterInfo> MateriaParamInfos;
		TArray<FGuid> MaterialParamGuids;

		Material->GetAllScalarParameterInfo(MateriaParamInfos, MaterialParamGuids);

		for(auto& CurScalarParam : MateriaParamInfos)
		{
			FHoudiniMaterialParameter<float>& Entry = MaterialInfos[Index].Scalars.Emplace_GetRef();
			Entry.Name = CurScalarParam.Name.ToString();
			Material->GetScalarParameterValue(CurScalarParam, Entry.Value);
		}

		Material->GetAllVectorParameterInfo(MateriaParamInfos, MaterialParamGuids);

		for(auto& CurVectorParam : MateriaParamInfos)
		{
			FHoudiniMaterialParameter<FLinearColor>& Entry = MaterialInfos[Index].Vectors.Emplace_GetRef();
			Entry.Name = CurVectorParam.Name.ToString();
			Material->GetVectorParameterValue(CurVectorParam, Entry.Value);
		}

		Material->GetAllTextureParameterInfo(MateriaParamInfos, MaterialParamGuids);

		for(auto& CurTextureParam : MateriaParamInfos)
		{
			FHoudiniMaterialParameter<FString>& Entry = MaterialInfos[Index].Textures.Emplace_GetRef();
			Entry.Name = CurTextureParam.Name.ToString();

			UTexture* Texture = nullptr;
			Material->GetTextureParameterValue(CurTextureParam, Texture);

			Entry.Value = IsValid(Texture) ? Texture->GetPathName() : TEXT("");
		}

	}

	return MaterialInfos;
}
bool
FUnrealObjectInputActorProperties::Update(const FUnrealObjectInputHAPINodeId& InNodeIdToConnectTo)
{
	// This modifier uses Vex code to set actor properties. This potentially includes material
	// parameters which may be set on a per-actor basis.

	// If we don't have a valid mesh component destroy the nodes and return false
	if(!IsValid(MeshComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	TArray<FName> Tags = MeshComponent->GetOwner()->Tags;
	TArray<UActorComponent*> Components;

	// Check that InNodeIdToConnectTo is valid
	if(!InNodeIdToConnectTo.IsValid())
		return false;

	HAPI_NodeId HAPINodeIdToConnectTo = InNodeIdToConnectTo.GetHAPINodeId();

	HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId(HAPINodeIdToConnectTo);

	// Check if we already have a valid node, if not create it
	HAPI_NodeId VexNodeId = EnsureHAPINodeExists(ParentId);

	if(VexNodeId < 0)
		return false;

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(Session, VexNodeId, 0, HAPINodeIdToConnectTo, 0), false);

	// Set the wrangle's class to prims
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(Session, VexNodeId, "class", 0, 1), false);

	// Build up Vex code for actor tags

	FStringBuilderBase Builder;

	for(auto Tag : Tags)
	{

		Builder.Appendf(TEXT("setprimgroup(0, \"%s\", @primnum, 1, \"set\");\n"), *Tag.ToString());
	}

	// Various paths we want to export...

	FString ActorPath = MeshComponent->GetOwner()->GetPathName();
	Builder.Appendf(TEXT("s@%s=\"%s\";\n"), TEXT(HAPI_UNREAL_ATTRIB_ACTOR_PATH), *ActorPath);

	FString LevelPath = MeshComponent->GetOwner()->GetLevel()->GetPathName();
	Builder.Appendf(TEXT("s@%s=\"%s\";\n"), TEXT(HAPI_UNREAL_ATTRIB_LEVEL_PATH), *LevelPath);


	// Material parameters found on SMCs.

	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(MeshComponent);

	if (SMC && IsValid(SMC->GetStaticMesh()))
	{
		// If we have a valid Static Mesh Component, get material overrides.

		TArray<FHoudiniMeshMaterialInfo> MaterialInfos = GetMaterialOverrides(SMC);

		int LODIndex = 0;
		UStaticMesh* StaticMesh = SMC->GetStaticMesh();

		const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[LODIndex];
		const int32 NumSections = LODResource.Sections.Num();

		auto MakeHoudiniParamName = [&](int Index, const FString& Name)
			{
				// Make sure the attribute name will be valid
				FString SanitizedName = Name;
				FHoudiniEngineUtils::SanitizeHAPIVariableName(SanitizedName);

				FString ParamPrefix = MaterialInfos.Num() == 1 ? "" : FString::FromInt(Index) + FString("_");
				return FString::Printf(TEXT("unreal_material_parameter_%s%s"), *ParamPrefix, *SanitizedName);
			};

		for(int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			const FStaticMeshSection& Section = LODResource.Sections[SectionIndex];

			FHoudiniMeshMaterialInfo* MaterialInfo = (MaterialInfos.IsValidIndex(Section.MaterialIndex)) ? &MaterialInfos[Section.MaterialIndex] : nullptr;
			if(!MaterialInfo)
				continue;

			int FirstTriangle = Section.FirstIndex / 3;
			int LastTriangle = FirstTriangle + Section.NumTriangles;

			bool bDoSnippet = !MaterialInfo->Scalars.IsEmpty() || !MaterialInfo->Vectors.IsEmpty() || !MaterialInfo->Textures.IsEmpty();

			if(bDoSnippet)
			{
				Builder.Append(FString::Printf(TEXT("if (@primnum >= %d && @primnum < %d)\n"), FirstTriangle, LastTriangle));
				Builder.Append(TEXT("{\n"));

				for(auto Scalar : MaterialInfo->Scalars)
				{
					FString AttributeName = MakeHoudiniParamName(Section.MaterialIndex, Scalar.Name);
					Builder.Append(FString::Printf(TEXT("    f@%s = %.17g;\n"), *AttributeName, Scalar.Value));
				}

				for(auto Vector : MaterialInfo->Vectors)
				{
					FString AttributeName = MakeHoudiniParamName(Section.MaterialIndex, Vector.Name);
					Builder.Append(FString::Printf(TEXT("    p@%s = { "), *AttributeName));
					Builder.Append(FString::Printf(TEXT(" %.17g, "), Vector.Value.R));
					Builder.Append(FString::Printf(TEXT(" %.17g, "), Vector.Value.G));
					Builder.Append(FString::Printf(TEXT(" %.17g, "), Vector.Value.B));
					Builder.Append(FString::Printf(TEXT(" %.17g };\n"), Vector.Value.A));
				}

				for(auto Texture : MaterialInfo->Textures)
				{
					FString AttributeName = MakeHoudiniParamName(Section.MaterialIndex, Texture.Name);
					Builder.Append(FString::Printf(TEXT("    f@%s = %s;\n"), *AttributeName, *Texture.Value));
				}

				Builder.Append(TEXT("}\n"));
			}
		}
	}

	FString VexCode = Builder.ToString();
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(VexNodeId, "snippet", ParmInfo);
	FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), VexNodeId, H_TCHAR_TO_UTF8(*VexCode), ParmId, 0);

	return true;
}
