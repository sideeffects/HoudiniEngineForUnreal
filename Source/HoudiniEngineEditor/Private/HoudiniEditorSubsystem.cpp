// Fill out your copyright notice in the Description page of Project Settings.


#include "HoudiniEditorSubsystem.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditor.h"
#include "Engine/SkeletalMesh.h"
#include "UnrealMeshTranslator.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntime.h"

void UHoudiniEditorSubsystem::CreateSessionHDA()
{
    // Attempt to restart the session
    if (!FHoudiniEngine::Get().IsSessionSyncEnabled())
    {
	FHoudiniEngine::Get().RestartSession();
    }

    HAPI_Result result;
    HAPI_NodeId  UnrealContentNode = -1;
    result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), -1, "Object/geo", "UnrealContent", true, &UnrealContentNode);
    HAPI_NodeInfo MyGeoNodeInfo = FHoudiniApi::NodeInfo_Create();
    result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), UnrealContentNode, &MyGeoNodeInfo);

    object_node_id = UnrealContentNode;
}

void UHoudiniEditorSubsystem::SendStaticMeshToHoudini(HAPI_NodeId mesh_node_id,UStaticMesh* StaticMesh)
{
  
    int32 LODIndex = 0;
    FStaticMeshSourceModel& SrcModel = StaticMesh->GetSourceModel(LODIndex);
    //FUnrealMeshTranslator::CreateInputNodeForRawMesh(mesh_node_id, SrcModel, LODIndex, false, StaticMesh, nullptr);
    FMeshDescription* MeshDesc = nullptr;
    MeshDesc = StaticMesh->GetMeshDescription(LODIndex);
    FUnrealMeshTranslator::CreateInputNodeForMeshDescription(mesh_node_id, *MeshDesc, LODIndex, false, StaticMesh, nullptr);
    //bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(SM, InObject->InputNodeId, SMCName, SMC, bExportLODs, bExportSockets, bExportColliders);
   
    HAPI_Result result;
    result = FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), mesh_node_id, 1);

    HAPI_Result ResultCommit = FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), mesh_node_id);
}

void UHoudiniEditorSubsystem::SendSkeletalMeshToHoudini(HAPI_NodeId mesh_node_id,USkeletalMesh* SkelMesh)
{
    HAPI_Result result;
    FUnrealMeshTranslator::SetSkeletalMeshDataOnNode(SkelMesh, mesh_node_id);

    result = FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), mesh_node_id, 1);

    HAPI_Result ResultCommit = FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), mesh_node_id);
}


void UHoudiniEditorSubsystem::SendToHoudini(const TArray<FAssetData>& SelectedAssets)
{
    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Sending To Houdini!"));

    if (object_node_id < 0)
    {
	    CreateSessionHDA();
    }

    //FString SendNodePath = "/obj/UnrealContent";
    UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
    FString SendNodePath = HoudiniEditorSubsystem->NodeSync.SendNodePath;

    HAPI_NodeId mesh_node_id;
    HAPI_Result result;
    HAPI_NodeId  UnrealContentNode = -1;
    result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*SendNodePath), &UnrealContentNode);

    if ((result == HAPI_RESULT_SUCCESS) && (UnrealContentNode >= 0))
    {
        //HOUDINI_LOG_MESSAGE(TEXT("Sending Node To %s "), *SendNodePath);
        HOUDINI_LOG_MESSAGE(TEXT("Sending Node"));
    }
    else
    {
        FString Name = SendNodePath;
        Name.RemoveFromStart("/obj/");
        result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), -1, "Object/geo", TCHAR_TO_ANSI(*Name), true, &UnrealContentNode);
    }


    USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(SelectedAssets[0].GetAsset());

    if (SkelMesh)
    {
        FString SKName = TEXT("SkeletalMesh_") + SkelMesh->GetName();
        result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), UnrealContentNode, "null", TCHAR_TO_ANSI(*SKName), true, &mesh_node_id);
        SendSkeletalMeshToHoudini(mesh_node_id, SkelMesh);
    }
    else
    {
	    UStaticMesh* StaticMesh = Cast<UStaticMesh>(SelectedAssets[0].GetAsset());
	    if (StaticMesh)
	    {
            FString SKName = TEXT("StaticMesh_") + StaticMesh->GetName();
            result = FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), UnrealContentNode, "null", TCHAR_TO_ANSI(*SKName), true, &mesh_node_id);
	        SendStaticMeshToHoudini(mesh_node_id, StaticMesh);
	    }
    }
}

void UHoudiniEditorSubsystem::DumpSessionInfo()
{
    HAPI_Result result;

    HOUDINI_LOG_MESSAGE(TEXT("network_node_id %i "), object_node_id);
    
    // Get the Display Geo's info
    HAPI_GeoInfo DisplayHapiGeoInfo;
    FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
    result = FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &DisplayHapiGeoInfo);

    FString DisplayName;
    FHoudiniEngineString HoudiniEngineString(DisplayHapiGeoInfo.nameSH);
    HoudiniEngineString.ToFString(DisplayName);
    
    HOUDINI_LOG_MESSAGE(TEXT("DisplayGeo %s NodeID %i  PartCount %i "), *DisplayName, DisplayHapiGeoInfo.nodeId, DisplayHapiGeoInfo.partCount );

}

void UHoudiniEditorSubsystem::Fetch()
{
    SendToUnreal(NodeSync.UnrealAssetName, NodeSync.UnrealPathName);
}

void UHoudiniEditorSubsystem::SendToUnreal(FString PackageName, FString PackageFolder, int MaxInfluences, bool ImportNormals)
{
    GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Importing To Unreal!"));

    //FString SendNodePath = "/obj/UnrealContent";
    UHoudiniEditorSubsystem* HoudiniEditorSubsystem = GEditor->GetEditorSubsystem<UHoudiniEditorSubsystem>();
    FString FetchNodePath = HoudiniEditorSubsystem->NodeSync.FetchNodePath;

    HAPI_Result result;
    HAPI_NodeId  UnrealContentNode = -1;

    if (HoudiniEditorSubsystem->NodeSync.UseDisplayFlag)
    {
        HAPI_GeoInfo DisplayHapiGeoInfo;
        FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
        result = FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &DisplayHapiGeoInfo);
        UnrealContentNode = DisplayHapiGeoInfo.nodeId;
    }
    else
    {
        result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_ANSI(*FetchNodePath), &UnrealContentNode);

        if ((result != HAPI_RESULT_SUCCESS) || (UnrealContentNode < 0))
        {
            //HOUDINI_LOG_MESSAGE(TEXT("Sending Node To %s "), *SendNodePath);
            HOUDINI_LOG_MESSAGE(TEXT("Invalid FetchNodePath"));
            return;
        }
    }
    result = FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), UnrealContentNode, nullptr);

    //Determine if Skeletal by presence of capt_xforms detail attribute
    int32 PartId = 0;  //multiple parts broken only do first part to get working
    HAPI_AttributeInfo CaptXFormsInfo;
    FHoudiniApi::AttributeInfo_Init(&CaptXFormsInfo);

    HAPI_Result CaptXFormsInfoResult = FHoudiniApi::GetAttributeInfo(
	FHoudiniEngine::Get().GetSession(),
    UnrealContentNode, PartId,
	"capt_xforms", HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, &CaptXFormsInfo);

    if (CaptXFormsInfo.exists)
    {
	    SendSkeletalMeshToUnreal(UnrealContentNode, PackageName, PackageFolder, MaxInfluences, ImportNormals);
    }
    else
    {
	    SendStaticMeshToUnreal(UnrealContentNode, PackageName, PackageFolder);
    }
}

void UHoudiniEditorSubsystem::SendSkeletalMeshToUnreal(HAPI_NodeId NodeId, FString PackageName, FString PackageFolder, int MaxInfluences = 1, bool ImportNormals = false)
{
    HAPI_Result result;

    //HAPI_GeoInfo DisplayHapiGeoInfo;
    //FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
    //result = FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &DisplayHapiGeoInfo);

    HAPI_NodeInfo MyNodeInfo = FHoudiniApi::NodeInfo_Create();
    result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &MyNodeInfo);
    FString NodeName = FHoudiniEngineUtils::HapiGetString(MyNodeInfo.nameSH);

    // Get the AssetInfo
    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
    result = FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), NodeId, &AssetInfo);

    FString UniqueName;
    FHoudiniEngineUtils::GetHoudiniAssetName(AssetInfo.nodeId, UniqueName);

    // Get the ObjectInfo
    HAPI_ObjectInfo ObjectInfo;
    FHoudiniApi::ObjectInfo_Init(&ObjectInfo);
    result = FHoudiniApi::GetObjectInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &ObjectInfo);

    FString CurrentAssetName;
    {
	FHoudiniEngineString hapiSTR(AssetInfo.nameSH);
	hapiSTR.ToFString(CurrentAssetName);
    }

    FString CurrentObjectName;
    {
	FHoudiniEngineString hapiSTR(ObjectInfo.nameSH);
	hapiSTR.ToFString(CurrentObjectName);
    }

    //Create a skeletal mesh
    int32 PartId = 0;  //multiple parts broken only do first part to get working
    FString FullPackageName = PackageFolder + TEXT("/") + PackageName;
    SKBuildSettings skBuildSettings;
    skBuildSettings.CurrentObjectName = NodeName;
    skBuildSettings.GeoId = NodeId;
    skBuildSettings.PartId = PartId;
    skBuildSettings.OverwriteSkeleton = NodeSync.OverwriteSkeleton;
    skBuildSettings.SkeletonAssetPath = NodeSync.SkeletonAssetPath;

    FHoudiniMeshTranslator::CreateSKAssetAndPackage(skBuildSettings, NodeId, PartId, FullPackageName, MaxInfluences, ImportNormals);
    TArray<FSkeletalMaterial> Materials;
    FSkeletalMaterial Mat;
    Materials.Add(Mat);
    Materials.Add(Mat);
    FHoudiniMeshTranslator::BuildSKFromImportData(skBuildSettings, Materials);
    //BuildSKFromImportData(SkeletalMeshImportData, Materials, MySkeleton, PackageName, NewMesh, Package);
}

void UHoudiniEditorSubsystem::SendStaticMeshToUnreal(HAPI_NodeId NodeId, FString PackageName, FString PackageFolder)
{
    HAPI_Result result;
    // Get the Display Geo's info
    HAPI_GeoInfo DisplayHapiGeoInfo;
    FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
    
    
    //result = FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &DisplayHapiGeoInfo);
    result = FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), NodeId, &DisplayHapiGeoInfo);
    if (result != HAPI_RESULT_SUCCESS)
    {
        HOUDINI_LOG_MESSAGE(TEXT("GetDisplayGeoInfo FAILURE trying to get object_node_id %i "), NodeId);
        return;
    }




    result = FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), NodeId, nullptr);

    HAPI_NodeInfo MyNodeInfo = FHoudiniApi::NodeInfo_Create();
    result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &MyNodeInfo);
    FString NodeName = FHoudiniEngineUtils::HapiGetString(MyNodeInfo.nameSH);

    // Get the AssetInfo
    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
    result = FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), NodeId, &AssetInfo);

    // Get the ObjectInfo
    HAPI_ObjectInfo ObjectInfo;
    FHoudiniApi::ObjectInfo_Init(&ObjectInfo);
    result = FHoudiniApi::GetObjectInfo(FHoudiniEngine::Get().GetSession(), object_node_id, &ObjectInfo);

    //for (int32 PartId = 0; PartId < GeoInfo.partCount; ++PartId)
    int32 PartId = 0;  //multiple parts broken only do first part to get working
    {
	// Get part information.
	HAPI_PartInfo CurrentHapiPartInfo;
	FHoudiniApi::PartInfo_Init(&CurrentHapiPartInfo);

	result = FHoudiniApi::GetPartInfo(
	    FHoudiniEngine::Get().GetSession(), NodeId, PartId, &CurrentHapiPartInfo);


	FString CurrentAssetName;
	{
	    FHoudiniEngineString hapiSTR(AssetInfo.nameSH);
	    hapiSTR.ToFString(CurrentAssetName);
	}

	FString CurrentObjectName;
	{
	    FHoudiniEngineString hapiSTR(ObjectInfo.nameSH);
	    hapiSTR.ToFString(CurrentObjectName);
	}

	FString CurrentGeoName;
	{
	    FHoudiniEngineString hapiSTR(MyNodeInfo.nameSH);
	    hapiSTR.ToFString(CurrentGeoName);
	}

	EHoudiniPartType CurrentPartType = EHoudiniPartType::Invalid;

	switch (CurrentHapiPartInfo.type)
	{
	    case HAPI_PARTTYPE_BOX:
	    case HAPI_PARTTYPE_SPHERE:
	    case HAPI_PARTTYPE_MESH:
	    {
		CurrentPartType = EHoudiniPartType::Mesh;
	    }
	}
	// Build the HGPO corresponding to this part
	FHoudiniGeoPartObject currentHGPO;
	currentHGPO.AssetId = AssetInfo.nodeId;
	currentHGPO.AssetName = CurrentAssetName;

	currentHGPO.ObjectId = ObjectInfo.nodeId;
	currentHGPO.ObjectName = CurrentObjectName;

	currentHGPO.GeoId = NodeId;

	currentHGPO.PartId = CurrentHapiPartInfo.id;

	currentHGPO.Type = CurrentPartType;
	currentHGPO.InstancerType = EHoudiniInstancerType::Invalid;

	currentHGPO.TransformMatrix = FTransform::Identity;;

	currentHGPO.NodePath = TEXT("");

	currentHGPO.bIsVisible = ObjectInfo.isVisible && !CurrentHapiPartInfo.isInstanced;
	currentHGPO.bIsEditable = DisplayHapiGeoInfo.isEditable;
	currentHGPO.bIsInstanced = CurrentHapiPartInfo.isInstanced;
	// Never consider a display geo as templated!
	currentHGPO.bIsTemplated = DisplayHapiGeoInfo.isDisplayGeo ? false : DisplayHapiGeoInfo.isTemplated;

	currentHGPO.bHasGeoChanged = DisplayHapiGeoInfo.hasGeoChanged;
	currentHGPO.bHasPartChanged = CurrentHapiPartInfo.hasChanged;
	currentHGPO.bHasMaterialsChanged = DisplayHapiGeoInfo.hasMaterialChanged;
	currentHGPO.bHasTransformChanged = ObjectInfo.hasTransformChanged;

	// Copy the HAPI info caches 
	FHoudiniObjectInfo CurrentObjectInfo;
	FHoudiniOutputTranslator::CacheObjectInfo(ObjectInfo, CurrentObjectInfo);
	currentHGPO.ObjectInfo = CurrentObjectInfo;
	FHoudiniGeoInfo CurrentGeoInfo;
	FHoudiniOutputTranslator::CacheGeoInfo(DisplayHapiGeoInfo, CurrentGeoInfo);
	currentHGPO.GeoInfo = CurrentGeoInfo;
	FHoudiniPartInfo CurrentPartInfo;
	FHoudiniOutputTranslator::CachePartInfo(CurrentHapiPartInfo, CurrentPartInfo);
	currentHGPO.PartInfo = CurrentPartInfo;
	TArray<FHoudiniMeshSocket> PartMeshSockets;
	currentHGPO.AllMeshSockets = PartMeshSockets;

	//Package Parameters
	FHoudiniPackageParams PackageParams;
	PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
	PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

	PackageParams.BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
	PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

	//PackageParams.OuterPackage = HAC->GetComponentLevel();
	//PackageParams.HoudiniAssetName = HAC->GetHoudiniAsset() ? HAC->GetHoudiniAsset()->GetName() : FString();
	//PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetName();
	//PackageParams.ComponentGUID = HAC->GetComponentGUID();
	PackageParams.ObjectName = FString();
	PackageParams.NameOverride = PackageName;
	PackageParams.FolderOverride = PackageFolder;
	PackageParams.OverideEnabled = true;

	FHoudiniMeshTranslator CurrentTranslator;
	CurrentTranslator.ForceRebuild = true;
	CurrentTranslator.SetHoudiniGeoPartObject(currentHGPO);
	//CurrentTranslator.SetInputObjects(InOutputObjects);
	//CurrentTranslator.SetOutputObjects(OutOutputObjects);
	//CurrentTranslator.SetInputAssignmentMaterials(AssignmentMaterialMap);
	//CurrentTranslator.SetAllOutputMaterials(InAllOutputMaterials);
	//CurrentTranslator.SetReplacementMaterials(ReplacementMaterialMap);
	CurrentTranslator.SetPackageParams(PackageParams, true);
	//CurrentTranslator.SetTreatExistingMaterialsAsUpToDate(bInTreatExistingMaterialsAsUpToDate);
	//CurrentTranslator.SetStaticMeshGenerationProperties(InSMGenerationProperties);
	//CurrentTranslator.SetStaticMeshBuildSettings(InSMBuildSettings);

	// TODO: Fetch from settings/HAC
	CurrentTranslator.DefaultMeshSmoothing = 1;

	CurrentTranslator.CreateStaticMesh_RawMesh();	
    }
}