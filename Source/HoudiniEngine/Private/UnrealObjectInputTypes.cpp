#include "UnrealObjectInputTypes.h"
#include "UnrealObjectInputManager.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"

#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


HAPI_NodeId
EnsureHAPINodeExistsInternal(
	const HAPI_NodeId InParentNetworkNodeId,
	const FString& InOpTypeName,
	const FString& InNodeName,
	TArray<int32>& InHAPINodeIds,
	const int32 InIndex)
{
	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return INDEX_NONE;

	// Check that InNodeIdToConnectTo is valid
	if (!Manager->IsHAPINodeValid(InParentNetworkNodeId))
		return false;

	// Check if we already have a valid node, if not create it
	HAPI_NodeId ExistingNodeId = InHAPINodeIds.IsValidIndex(InIndex) ? InHAPINodeIds[InIndex] : INDEX_NONE;
	if (ExistingNodeId <= 0 || !Manager->IsHAPINodeValid(ExistingNodeId))
	{
		static constexpr bool bCookOnCreation = false;
		if (FHoudiniEngineUtils::CreateNode(
				InParentNetworkNodeId, InOpTypeName, InNodeName, bCookOnCreation, &ExistingNodeId) != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the node.
			HOUDINI_LOG_WARNING(
				TEXT("Failed to create %s node: %s"), *InNodeName, *FHoudiniEngineUtils::GetErrorDescription());
			return INDEX_NONE;
		}
		while (InHAPINodeIds.Num() <= InIndex)
			InHAPINodeIds.Emplace(-1);
		InHAPINodeIds[InIndex] = ExistingNodeId;
	}

	return ExistingNodeId;
}


HAPI_NodeId
FUnrealObjectInputMaterialOverrides::EnsureHAPINodeExists(const HAPI_NodeId InParentNetworkNodeId)
{
	const FString OpTypeName(bUsePrimWrangle ? TEXT("attribwrangle") : TEXT("attribcreate"));
	const FString NodeName(TEXT("material_overrides")); 
	return EnsureHAPINodeExistsInternal(InParentNetworkNodeId, OpTypeName, NodeName, HAPINodeIds, 0);
}

bool
FUnrealObjectInputMaterialOverrides::UpdateAsPrimWrangle(const int32 InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(MeshComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	// Check that InNodeIdToConnectTo is valid
	if (!Manager->IsHAPINodeValid(InNodeIdToConnectTo))
		return false;
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId MaterialOverridesNodeId = EnsureHAPINodeExists(FHoudiniEngineUtils::HapiGetParentNodeId(InNodeIdToConnectTo));
	if (MaterialOverridesNodeId < 0)
		return false;

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(),
		MaterialOverridesNodeId, 0, InNodeIdToConnectTo, 0), false);

	// Set group to exclude applying the material overrides to collision geo and sockets
	HAPI_ParmInfo GroupParmInfo;
	HAPI_ParmId GroupParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "group", GroupParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, "* ^collision_* ^socket_imported", GroupParmId, 0), false);

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

	// Set the wrangle's class to primitives
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, "class", 0, 1), false);

	// Set the snippet parameter to the VEXpression.
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "snippet", ParmInfo);
	if (ParmId != -1)
	{
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId,
			TCHAR_TO_UTF8(*VEXpression), ParmId, 0);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	return true;
}


bool
FUnrealObjectInputMaterialOverrides::UpdateAsPointAttribCreate(int32 InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(MeshComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	// Check that InNodeIdToConnectTo is valid
	if (!Manager->IsHAPINodeValid(InNodeIdToConnectTo))
		return false;
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId MaterialOverridesNodeId = EnsureHAPINodeExists(FHoudiniEngineUtils::HapiGetParentNodeId(InNodeIdToConnectTo));
	if (MaterialOverridesNodeId < 0)
		return false;

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(),
		MaterialOverridesNodeId, 0, InNodeIdToConnectTo, 0), false);

	// Set group to exclude applying the material overrides to collision geo and sockets
	HAPI_ParmInfo GroupParmInfo;
	HAPI_ParmId GroupParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "group", GroupParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, "* ^collision_* ^socket_imported", GroupParmId, 0), false);

	// Get the default material for in case we encounter invalid materials
	UMaterialInterface const* const DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	const FString DefaultMaterialPathName = IsValid(DefaultMaterial) ? DefaultMaterial->GetPathName() : TEXT("default");

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, "numattr", 0, NumMaterials);
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
			FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, TCHAR_TO_ANSI(*MatName), ParmId, 0), false);
	
		// set attribute type to string (index 3)
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, TCHAR_TO_ANSI(*FString::Printf(TEXT("type%d"), MatNum)), 0, 3), false);
	
		// set value to path of material
		ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, TCHAR_TO_ANSI(*FString::Printf(TEXT("string%d"), MatNum)), ParmInfo);
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, TCHAR_TO_ANSI(*MaterialPathName), ParmId, 0), false);
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
FUnrealObjectInputMaterialOverrides::Update(int32 InNodeIdToConnectTo)
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
FUnrealObjectInputPhysicalMaterialOverride::Update(int32 InNodeIdToConnectTo)
{
	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	// Check that InNodeIdToConnectTo is valid
	if (!Manager->IsHAPINodeValid(InNodeIdToConnectTo))
		return false;

	if (!IsValid(PrimitiveComponent))
	{
		DestroyHAPINodes();
		return false;
	}

	// Remove existing nodes if rebuilding
	if (bNeedsRebuild)
		DestroyHAPINodes();

	// Check if we already have a valid node, if not create it
	const HAPI_NodeId PhysMatOverrideNodeId = EnsureHAPINodeExists(FHoudiniEngineUtils::HapiGetParentNodeId(InNodeIdToConnectTo));
	if (PhysMatOverrideNodeId < 0)
		return false;

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(),
		PhysMatOverrideNodeId, 0, InNodeIdToConnectTo, 0), false);

	// Set the number of attributes: 1
	FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, "numattr", 0, 1);

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
	
	// We don't have C++ API access to the override property. We have to get it via the property system.
	// We can't just call GetSimplePhysicalMaterial() since that has fallback checks if the override is not set and
	// will at least return GEngine->DefaultPhysMaterial, so it does not answer the question of "is there a component
	// level override?"
	UPhysicalMaterial const* PhysMat = nullptr;
	// We cannot use GET_MEMBER_NAME_CHECKED() since the property is protected
	const FName OverridePropertyName(TEXT("PhysMaterialOverride"));
	FObjectProperty const* const PhysMatOverrideProperty = FindFProperty<FObjectProperty>(FBodyInstance::StaticStruct(), OverridePropertyName);
	ensureMsgf(PhysMatOverrideProperty != nullptr, TEXT("Could not find '%s' property on FBodyInstance, has it been removed/deprecated?"), *OverridePropertyName.ToString());
	if (PhysMatOverrideProperty)
	{
		UObject const* const PhysMatObject = PhysMatOverrideProperty->GetObjectPropertyValue_InContainer(&PrimitiveComponent->BodyInstance);
		// If the override is set to a valid UPhysicalMaterial then fetch the effective simple physical material from
		// component's body instance
		if (IsValid(PhysMatObject) && PhysMatObject->IsA<UPhysicalMaterial>())
			PhysMat = PrimitiveComponent->BodyInstance.GetSimplePhysicalMaterial();
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Could not find physical material override: could not find property '%s' on FBodyInstance."), *OverridePropertyName.ToString());
	}
	
	// If the material is invalid then the path is empty string and we disable the attribute on the node
	FString MaterialPath("");
	bool bEnable = false;
	if (IsValid(PhysMat))
	{
		// If the material is valid get its path
		MaterialPath = PhysMat->GetPathName();
		bEnable = true;
	}

	// Set group to exclude applying the physical material override to sockets (only really applicable when AttrClass is points)
	HAPI_ParmInfo GroupParmInfo;
	HAPI_ParmId GroupParmId = FHoudiniEngineUtils::HapiFindParameterByName(PhysMatOverrideNodeId, "group", GroupParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, "* ^socket_imported", GroupParmId, 0), false);

	// Set enable
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, "enable1", 0, bEnable), false);

	// Set attribute class
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, "class1", 0, AttrClass), false);

	// Set the attribcreate attribute name
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(PhysMatOverrideNodeId, "name1", ParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, HAPI_UNREAL_ATTRIB_SIMPLE_PHYSICAL_MATERIAL, ParmId, 0), false);

	// set attribute type to string (index 3)
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, "type1", 0, 3), false);

	// set value to path of material
	ParmId = FHoudiniEngineUtils::HapiFindParameterByName(PhysMatOverrideNodeId, "string1", ParmInfo);
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), PhysMatOverrideNodeId, TCHAR_TO_ANSI(*MaterialPath), ParmId, 0), false);

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
FUnrealObjectInputActorAsReference::Update(int32 InNodeIdToConnectTo)
{
	// If we don't have a valid mesh component destroy the nodes and return false
	if (!IsValid(Actor))
	{
		DestroyHAPINodes();
		return false;
	}

	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	// Check that InNodeIdToConnectTo is valid
	if (!Manager->IsHAPINodeValid(InNodeIdToConnectTo))
		return false;
	
	// Check if we already have a valid node, if not create it
	const HAPI_NodeId MaterialOverridesNodeId = EnsureHAPINodeExists(FHoudiniEngineUtils::HapiGetParentNodeId(InNodeIdToConnectTo));
	if (MaterialOverridesNodeId < 0)
		return false;

	// Connect our input to InNodeIdToConnectTo's output
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
		FHoudiniEngine::Get().GetSession(),
		MaterialOverridesNodeId, 0, InNodeIdToConnectTo, 0), false);

	// Extract the level path from the level
	FString LevelPath("");
	if (IsValid(Actor->GetLevel()))
	{
		LevelPath = Actor->GetLevel()->GetPathName();
		// We just want the path up to the first point
		int32 DotIndex;
		if (LevelPath.FindChar('.', DotIndex))
			LevelPath.LeftInline(DotIndex, false);
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
		FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId, "class", 0, 2), false);

	// Set the snippet parameter to the VEXpression.
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(MaterialOverridesNodeId, "snippet", ParmInfo);
	if (ParmId != -1)
	{
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), MaterialOverridesNodeId,
			TCHAR_TO_UTF8(*VEXpression), ParmId, 0);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	return true;
}
