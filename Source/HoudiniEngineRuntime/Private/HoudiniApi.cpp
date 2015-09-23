/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
* COMMENTS:
*      This file is generated. Do not modify directly.
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniApi.h"


FHoudiniApi::CreateInProcessSessionFuncPtr
FHoudiniApi::CreateInProcessSession = nullptr;

FHoudiniApi::StartThriftSocketServerFuncPtr
FHoudiniApi::StartThriftSocketServer = nullptr;

FHoudiniApi::CreateThriftSocketSessionFuncPtr
FHoudiniApi::CreateThriftSocketSession = nullptr;

FHoudiniApi::StartThriftNamedPipeServerFuncPtr
FHoudiniApi::StartThriftNamedPipeServer = nullptr;

FHoudiniApi::CreateThriftNamedPipeSessionFuncPtr
FHoudiniApi::CreateThriftNamedPipeSession = nullptr;

FHoudiniApi::BindCustomImplementationFuncPtr
FHoudiniApi::BindCustomImplementation = nullptr;

FHoudiniApi::CreateCustomSessionFuncPtr
FHoudiniApi::CreateCustomSession = nullptr;

FHoudiniApi::IsSessionValidFuncPtr
FHoudiniApi::IsSessionValid = nullptr;

FHoudiniApi::CloseSessionFuncPtr
FHoudiniApi::CloseSession = nullptr;

FHoudiniApi::IsInitializedFuncPtr
FHoudiniApi::IsInitialized = nullptr;

FHoudiniApi::InitializeFuncPtr
FHoudiniApi::Initialize = nullptr;

FHoudiniApi::CleanupFuncPtr
FHoudiniApi::Cleanup = nullptr;

FHoudiniApi::GetEnvIntFuncPtr
FHoudiniApi::GetEnvInt = nullptr;

FHoudiniApi::GetSessionEnvIntFuncPtr
FHoudiniApi::GetSessionEnvInt = nullptr;

FHoudiniApi::GetStatusFuncPtr
FHoudiniApi::GetStatus = nullptr;

FHoudiniApi::GetStatusStringBufLengthFuncPtr
FHoudiniApi::GetStatusStringBufLength = nullptr;

FHoudiniApi::GetStatusStringFuncPtr
FHoudiniApi::GetStatusString = nullptr;

FHoudiniApi::GetCookingTotalCountFuncPtr
FHoudiniApi::GetCookingTotalCount = nullptr;

FHoudiniApi::GetCookingCurrentCountFuncPtr
FHoudiniApi::GetCookingCurrentCount = nullptr;

FHoudiniApi::ConvertTransformFuncPtr
FHoudiniApi::ConvertTransform = nullptr;

FHoudiniApi::ConvertMatrixToQuatFuncPtr
FHoudiniApi::ConvertMatrixToQuat = nullptr;

FHoudiniApi::ConvertMatrixToEulerFuncPtr
FHoudiniApi::ConvertMatrixToEuler = nullptr;

FHoudiniApi::ConvertTransformQuatToMatrixFuncPtr
FHoudiniApi::ConvertTransformQuatToMatrix = nullptr;

FHoudiniApi::ConvertTransformEulerToMatrixFuncPtr
FHoudiniApi::ConvertTransformEulerToMatrix = nullptr;

FHoudiniApi::PythonThreadInterpreterLockFuncPtr
FHoudiniApi::PythonThreadInterpreterLock = nullptr;

FHoudiniApi::GetStringBufLengthFuncPtr
FHoudiniApi::GetStringBufLength = nullptr;

FHoudiniApi::GetStringFuncPtr
FHoudiniApi::GetString = nullptr;

FHoudiniApi::GetTimeFuncPtr
FHoudiniApi::GetTime = nullptr;

FHoudiniApi::SetTimeFuncPtr
FHoudiniApi::SetTime = nullptr;

FHoudiniApi::GetTimelineOptionsFuncPtr
FHoudiniApi::GetTimelineOptions = nullptr;

FHoudiniApi::SetTimelineOptionsFuncPtr
FHoudiniApi::SetTimelineOptions = nullptr;

FHoudiniApi::IsAssetValidFuncPtr
FHoudiniApi::IsAssetValid = nullptr;

FHoudiniApi::LoadAssetLibraryFromFileFuncPtr
FHoudiniApi::LoadAssetLibraryFromFile = nullptr;

FHoudiniApi::LoadAssetLibraryFromMemoryFuncPtr
FHoudiniApi::LoadAssetLibraryFromMemory = nullptr;

FHoudiniApi::GetAvailableAssetCountFuncPtr
FHoudiniApi::GetAvailableAssetCount = nullptr;

FHoudiniApi::GetAvailableAssetsFuncPtr
FHoudiniApi::GetAvailableAssets = nullptr;

FHoudiniApi::InstantiateAssetFuncPtr
FHoudiniApi::InstantiateAsset = nullptr;

FHoudiniApi::CreateCurveFuncPtr
FHoudiniApi::CreateCurve = nullptr;

FHoudiniApi::CreateInputAssetFuncPtr
FHoudiniApi::CreateInputAsset = nullptr;

FHoudiniApi::DestroyAssetFuncPtr
FHoudiniApi::DestroyAsset = nullptr;

FHoudiniApi::GetAssetInfoFuncPtr
FHoudiniApi::GetAssetInfo = nullptr;

FHoudiniApi::CookAssetFuncPtr
FHoudiniApi::CookAsset = nullptr;

FHoudiniApi::InterruptFuncPtr
FHoudiniApi::Interrupt = nullptr;

FHoudiniApi::GetAssetTransformFuncPtr
FHoudiniApi::GetAssetTransform = nullptr;

FHoudiniApi::SetAssetTransformFuncPtr
FHoudiniApi::SetAssetTransform = nullptr;

FHoudiniApi::GetInputNameFuncPtr
FHoudiniApi::GetInputName = nullptr;

FHoudiniApi::LoadHIPFileFuncPtr
FHoudiniApi::LoadHIPFile = nullptr;

FHoudiniApi::CheckForNewAssetsFuncPtr
FHoudiniApi::CheckForNewAssets = nullptr;

FHoudiniApi::GetNewAssetIdsFuncPtr
FHoudiniApi::GetNewAssetIds = nullptr;

FHoudiniApi::SaveHIPFileFuncPtr
FHoudiniApi::SaveHIPFile = nullptr;

FHoudiniApi::GetNodeInfoFuncPtr
FHoudiniApi::GetNodeInfo = nullptr;

FHoudiniApi::GetEditableNodeNetworksFuncPtr
FHoudiniApi::GetEditableNodeNetworks = nullptr;

FHoudiniApi::GetNodeNetworkChildrenFuncPtr
FHoudiniApi::GetNodeNetworkChildren = nullptr;

FHoudiniApi::CreateNodeFuncPtr
FHoudiniApi::CreateNode = nullptr;

FHoudiniApi::DeleteNodeFuncPtr
FHoudiniApi::DeleteNode = nullptr;

FHoudiniApi::RenameNodeFuncPtr
FHoudiniApi::RenameNode = nullptr;

FHoudiniApi::ConnectNodeInputFuncPtr
FHoudiniApi::ConnectNodeInput = nullptr;

FHoudiniApi::DisconnectNodeInputFuncPtr
FHoudiniApi::DisconnectNodeInput = nullptr;

FHoudiniApi::QueryNodeInputFuncPtr
FHoudiniApi::QueryNodeInput = nullptr;

FHoudiniApi::GetParametersFuncPtr
FHoudiniApi::GetParameters = nullptr;

FHoudiniApi::GetParmInfoFuncPtr
FHoudiniApi::GetParmInfo = nullptr;

FHoudiniApi::GetParmIdFromNameFuncPtr
FHoudiniApi::GetParmIdFromName = nullptr;

FHoudiniApi::GetParmInfoFromNameFuncPtr
FHoudiniApi::GetParmInfoFromName = nullptr;

FHoudiniApi::GetParmIntValueFuncPtr
FHoudiniApi::GetParmIntValue = nullptr;

FHoudiniApi::GetParmIntValuesFuncPtr
FHoudiniApi::GetParmIntValues = nullptr;

FHoudiniApi::GetParmFloatValueFuncPtr
FHoudiniApi::GetParmFloatValue = nullptr;

FHoudiniApi::GetParmFloatValuesFuncPtr
FHoudiniApi::GetParmFloatValues = nullptr;

FHoudiniApi::GetParmStringValueFuncPtr
FHoudiniApi::GetParmStringValue = nullptr;

FHoudiniApi::GetParmStringValuesFuncPtr
FHoudiniApi::GetParmStringValues = nullptr;

FHoudiniApi::GetParmChoiceListsFuncPtr
FHoudiniApi::GetParmChoiceLists = nullptr;

FHoudiniApi::SetParmIntValueFuncPtr
FHoudiniApi::SetParmIntValue = nullptr;

FHoudiniApi::SetParmIntValuesFuncPtr
FHoudiniApi::SetParmIntValues = nullptr;

FHoudiniApi::SetParmFloatValueFuncPtr
FHoudiniApi::SetParmFloatValue = nullptr;

FHoudiniApi::SetParmFloatValuesFuncPtr
FHoudiniApi::SetParmFloatValues = nullptr;

FHoudiniApi::SetParmStringValueFuncPtr
FHoudiniApi::SetParmStringValue = nullptr;

FHoudiniApi::InsertMultiparmInstanceFuncPtr
FHoudiniApi::InsertMultiparmInstance = nullptr;

FHoudiniApi::RemoveMultiparmInstanceFuncPtr
FHoudiniApi::RemoveMultiparmInstance = nullptr;

FHoudiniApi::GetHandleInfoFuncPtr
FHoudiniApi::GetHandleInfo = nullptr;

FHoudiniApi::GetHandleBindingInfoFuncPtr
FHoudiniApi::GetHandleBindingInfo = nullptr;

FHoudiniApi::GetPresetBufLengthFuncPtr
FHoudiniApi::GetPresetBufLength = nullptr;

FHoudiniApi::GetPresetFuncPtr
FHoudiniApi::GetPreset = nullptr;

FHoudiniApi::SetPresetFuncPtr
FHoudiniApi::SetPreset = nullptr;

FHoudiniApi::GetObjectsFuncPtr
FHoudiniApi::GetObjects = nullptr;

FHoudiniApi::GetObjectTransformsFuncPtr
FHoudiniApi::GetObjectTransforms = nullptr;

FHoudiniApi::GetInstanceTransformsFuncPtr
FHoudiniApi::GetInstanceTransforms = nullptr;

FHoudiniApi::SetObjectTransformFuncPtr
FHoudiniApi::SetObjectTransform = nullptr;

FHoudiniApi::GetGeoInfoFuncPtr
FHoudiniApi::GetGeoInfo = nullptr;

FHoudiniApi::GetPartInfoFuncPtr
FHoudiniApi::GetPartInfo = nullptr;

FHoudiniApi::GetFaceCountsFuncPtr
FHoudiniApi::GetFaceCounts = nullptr;

FHoudiniApi::GetVertexListFuncPtr
FHoudiniApi::GetVertexList = nullptr;

FHoudiniApi::GetAttributeInfoFuncPtr
FHoudiniApi::GetAttributeInfo = nullptr;

FHoudiniApi::GetAttributeNamesFuncPtr
FHoudiniApi::GetAttributeNames = nullptr;

FHoudiniApi::GetAttributeIntDataFuncPtr
FHoudiniApi::GetAttributeIntData = nullptr;

FHoudiniApi::GetAttributeFloatDataFuncPtr
FHoudiniApi::GetAttributeFloatData = nullptr;

FHoudiniApi::GetAttributeStringDataFuncPtr
FHoudiniApi::GetAttributeStringData = nullptr;

FHoudiniApi::GetGroupNamesFuncPtr
FHoudiniApi::GetGroupNames = nullptr;

FHoudiniApi::GetGroupMembershipFuncPtr
FHoudiniApi::GetGroupMembership = nullptr;

FHoudiniApi::GetInstancedPartIdsFuncPtr
FHoudiniApi::GetInstancedPartIds = nullptr;

FHoudiniApi::GetInstancerPartTransformsFuncPtr
FHoudiniApi::GetInstancerPartTransforms = nullptr;

FHoudiniApi::SetGeoInfoFuncPtr
FHoudiniApi::SetGeoInfo = nullptr;

FHoudiniApi::SetPartInfoFuncPtr
FHoudiniApi::SetPartInfo = nullptr;

FHoudiniApi::SetFaceCountsFuncPtr
FHoudiniApi::SetFaceCounts = nullptr;

FHoudiniApi::SetVertexListFuncPtr
FHoudiniApi::SetVertexList = nullptr;

FHoudiniApi::AddAttributeFuncPtr
FHoudiniApi::AddAttribute = nullptr;

FHoudiniApi::SetAttributeIntDataFuncPtr
FHoudiniApi::SetAttributeIntData = nullptr;

FHoudiniApi::SetAttributeFloatDataFuncPtr
FHoudiniApi::SetAttributeFloatData = nullptr;

FHoudiniApi::SetAttributeStringDataFuncPtr
FHoudiniApi::SetAttributeStringData = nullptr;

FHoudiniApi::AddGroupFuncPtr
FHoudiniApi::AddGroup = nullptr;

FHoudiniApi::SetGroupMembershipFuncPtr
FHoudiniApi::SetGroupMembership = nullptr;

FHoudiniApi::CommitGeoFuncPtr
FHoudiniApi::CommitGeo = nullptr;

FHoudiniApi::RevertGeoFuncPtr
FHoudiniApi::RevertGeo = nullptr;

FHoudiniApi::ConnectAssetTransformFuncPtr
FHoudiniApi::ConnectAssetTransform = nullptr;

FHoudiniApi::DisconnectAssetTransformFuncPtr
FHoudiniApi::DisconnectAssetTransform = nullptr;

FHoudiniApi::ConnectAssetGeometryFuncPtr
FHoudiniApi::ConnectAssetGeometry = nullptr;

FHoudiniApi::DisconnectAssetGeometryFuncPtr
FHoudiniApi::DisconnectAssetGeometry = nullptr;

FHoudiniApi::GetMaterialIdsOnFacesFuncPtr
FHoudiniApi::GetMaterialIdsOnFaces = nullptr;

FHoudiniApi::GetMaterialInfoFuncPtr
FHoudiniApi::GetMaterialInfo = nullptr;

FHoudiniApi::RenderTextureToImageFuncPtr
FHoudiniApi::RenderTextureToImage = nullptr;

FHoudiniApi::GetSupportedImageFileFormatCountFuncPtr
FHoudiniApi::GetSupportedImageFileFormatCount = nullptr;

FHoudiniApi::GetSupportedImageFileFormatsFuncPtr
FHoudiniApi::GetSupportedImageFileFormats = nullptr;

FHoudiniApi::GetImageInfoFuncPtr
FHoudiniApi::GetImageInfo = nullptr;

FHoudiniApi::SetImageInfoFuncPtr
FHoudiniApi::SetImageInfo = nullptr;

FHoudiniApi::GetImagePlaneCountFuncPtr
FHoudiniApi::GetImagePlaneCount = nullptr;

FHoudiniApi::GetImagePlanesFuncPtr
FHoudiniApi::GetImagePlanes = nullptr;

FHoudiniApi::ExtractImageToFileFuncPtr
FHoudiniApi::ExtractImageToFile = nullptr;

FHoudiniApi::ExtractImageToMemoryFuncPtr
FHoudiniApi::ExtractImageToMemory = nullptr;

FHoudiniApi::GetImageMemoryBufferFuncPtr
FHoudiniApi::GetImageMemoryBuffer = nullptr;

FHoudiniApi::SetAnimCurveFuncPtr
FHoudiniApi::SetAnimCurve = nullptr;

FHoudiniApi::SetTransformAnimCurveFuncPtr
FHoudiniApi::SetTransformAnimCurve = nullptr;

FHoudiniApi::ResetSimulationFuncPtr
FHoudiniApi::ResetSimulation = nullptr;

FHoudiniApi::GetVolumeInfoFuncPtr
FHoudiniApi::GetVolumeInfo = nullptr;

FHoudiniApi::GetFirstVolumeTileFuncPtr
FHoudiniApi::GetFirstVolumeTile = nullptr;

FHoudiniApi::GetNextVolumeTileFuncPtr
FHoudiniApi::GetNextVolumeTile = nullptr;

FHoudiniApi::GetVolumeVoxelFloatDataFuncPtr
FHoudiniApi::GetVolumeVoxelFloatData = nullptr;

FHoudiniApi::GetVolumeTileFloatDataFuncPtr
FHoudiniApi::GetVolumeTileFloatData = nullptr;

FHoudiniApi::GetVolumeVoxelIntDataFuncPtr
FHoudiniApi::GetVolumeVoxelIntData = nullptr;

FHoudiniApi::GetVolumeTileIntDataFuncPtr
FHoudiniApi::GetVolumeTileIntData = nullptr;

FHoudiniApi::SetVolumeInfoFuncPtr
FHoudiniApi::SetVolumeInfo = nullptr;

FHoudiniApi::SetVolumeTileFloatDataFuncPtr
FHoudiniApi::SetVolumeTileFloatData = nullptr;

FHoudiniApi::SetVolumeTileIntDataFuncPtr
FHoudiniApi::SetVolumeTileIntData = nullptr;

FHoudiniApi::GetCurveInfoFuncPtr
FHoudiniApi::GetCurveInfo = nullptr;

FHoudiniApi::GetCurveCountsFuncPtr
FHoudiniApi::GetCurveCounts = nullptr;

FHoudiniApi::GetCurveOrdersFuncPtr
FHoudiniApi::GetCurveOrders = nullptr;

FHoudiniApi::GetCurveKnotsFuncPtr
FHoudiniApi::GetCurveKnots = nullptr;

FHoudiniApi::SetCurveInfoFuncPtr
FHoudiniApi::SetCurveInfo = nullptr;

FHoudiniApi::SetCurveCountsFuncPtr
FHoudiniApi::SetCurveCounts = nullptr;

FHoudiniApi::SetCurveOrdersFuncPtr
FHoudiniApi::SetCurveOrders = nullptr;

FHoudiniApi::SetCurveKnotsFuncPtr
FHoudiniApi::SetCurveKnots = nullptr;

FHoudiniApi::SaveGeoToFileFuncPtr
FHoudiniApi::SaveGeoToFile = nullptr;

FHoudiniApi::LoadGeoFromFileFuncPtr
FHoudiniApi::LoadGeoFromFile = nullptr;

FHoudiniApi::GetGeoSizeFuncPtr
FHoudiniApi::GetGeoSize = nullptr;

FHoudiniApi::SaveGeoToMemoryFuncPtr
FHoudiniApi::SaveGeoToMemory = nullptr;

FHoudiniApi::LoadGeoFromMemoryFuncPtr
FHoudiniApi::LoadGeoFromMemory = nullptr;

FHoudiniApi::GetMaterialOnPartFuncPtr
FHoudiniApi::GetMaterialOnPart = nullptr;

FHoudiniApi::GetMaterialOnGroupFuncPtr
FHoudiniApi::GetMaterialOnGroup = nullptr;


void
FHoudiniApi::InitializeHAPI(void* LibraryHandle)
{
	if(!LibraryHandle) return;

	FHoudiniApi::CreateInProcessSession = (CreateInProcessSessionFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateInProcessSession"));
	FHoudiniApi::StartThriftSocketServer = (StartThriftSocketServerFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_StartThriftSocketServer"));
	FHoudiniApi::CreateThriftSocketSession = (CreateThriftSocketSessionFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateThriftSocketSession"));
	FHoudiniApi::StartThriftNamedPipeServer = (StartThriftNamedPipeServerFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_StartThriftNamedPipeServer"));
	FHoudiniApi::CreateThriftNamedPipeSession = (CreateThriftNamedPipeSessionFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateThriftNamedPipeSession"));
	FHoudiniApi::BindCustomImplementation = (BindCustomImplementationFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_BindCustomImplementation"));
	FHoudiniApi::CreateCustomSession = (CreateCustomSessionFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateCustomSession"));
	FHoudiniApi::IsSessionValid = (IsSessionValidFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_IsSessionValid"));
	FHoudiniApi::CloseSession = (CloseSessionFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CloseSession"));
	FHoudiniApi::IsInitialized = (IsInitializedFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_IsInitialized"));
	FHoudiniApi::Initialize = (InitializeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_Initialize"));
	FHoudiniApi::Cleanup = (CleanupFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_Cleanup"));
	FHoudiniApi::GetEnvInt = (GetEnvIntFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetEnvInt"));
	FHoudiniApi::GetSessionEnvInt = (GetSessionEnvIntFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetSessionEnvInt"));
	FHoudiniApi::GetStatus = (GetStatusFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetStatus"));
	FHoudiniApi::GetStatusStringBufLength = (GetStatusStringBufLengthFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetStatusStringBufLength"));
	FHoudiniApi::GetStatusString = (GetStatusStringFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetStatusString"));
	FHoudiniApi::GetCookingTotalCount = (GetCookingTotalCountFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetCookingTotalCount"));
	FHoudiniApi::GetCookingCurrentCount = (GetCookingCurrentCountFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetCookingCurrentCount"));
	FHoudiniApi::ConvertTransform = (ConvertTransformFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConvertTransform"));
	FHoudiniApi::ConvertMatrixToQuat = (ConvertMatrixToQuatFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConvertMatrixToQuat"));
	FHoudiniApi::ConvertMatrixToEuler = (ConvertMatrixToEulerFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConvertMatrixToEuler"));
	FHoudiniApi::ConvertTransformQuatToMatrix = (ConvertTransformQuatToMatrixFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConvertTransformQuatToMatrix"));
	FHoudiniApi::ConvertTransformEulerToMatrix = (ConvertTransformEulerToMatrixFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConvertTransformEulerToMatrix"));
	FHoudiniApi::PythonThreadInterpreterLock = (PythonThreadInterpreterLockFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_PythonThreadInterpreterLock"));
	FHoudiniApi::GetStringBufLength = (GetStringBufLengthFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetStringBufLength"));
	FHoudiniApi::GetString = (GetStringFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetString"));
	FHoudiniApi::GetTime = (GetTimeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetTime"));
	FHoudiniApi::SetTime = (SetTimeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetTime"));
	FHoudiniApi::GetTimelineOptions = (GetTimelineOptionsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetTimelineOptions"));
	FHoudiniApi::SetTimelineOptions = (SetTimelineOptionsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetTimelineOptions"));
	FHoudiniApi::IsAssetValid = (IsAssetValidFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_IsAssetValid"));
	FHoudiniApi::LoadAssetLibraryFromFile = (LoadAssetLibraryFromFileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_LoadAssetLibraryFromFile"));
	FHoudiniApi::LoadAssetLibraryFromMemory = (LoadAssetLibraryFromMemoryFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_LoadAssetLibraryFromMemory"));
	FHoudiniApi::GetAvailableAssetCount = (GetAvailableAssetCountFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAvailableAssetCount"));
	FHoudiniApi::GetAvailableAssets = (GetAvailableAssetsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAvailableAssets"));
	FHoudiniApi::InstantiateAsset = (InstantiateAssetFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_InstantiateAsset"));
	FHoudiniApi::CreateCurve = (CreateCurveFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateCurve"));
	FHoudiniApi::CreateInputAsset = (CreateInputAssetFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateInputAsset"));
	FHoudiniApi::DestroyAsset = (DestroyAssetFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_DestroyAsset"));
	FHoudiniApi::GetAssetInfo = (GetAssetInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAssetInfo"));
	FHoudiniApi::CookAsset = (CookAssetFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CookAsset"));
	FHoudiniApi::Interrupt = (InterruptFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_Interrupt"));
	FHoudiniApi::GetAssetTransform = (GetAssetTransformFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAssetTransform"));
	FHoudiniApi::SetAssetTransform = (SetAssetTransformFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetAssetTransform"));
	FHoudiniApi::GetInputName = (GetInputNameFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetInputName"));
	FHoudiniApi::LoadHIPFile = (LoadHIPFileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_LoadHIPFile"));
	FHoudiniApi::CheckForNewAssets = (CheckForNewAssetsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CheckForNewAssets"));
	FHoudiniApi::GetNewAssetIds = (GetNewAssetIdsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetNewAssetIds"));
	FHoudiniApi::SaveHIPFile = (SaveHIPFileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SaveHIPFile"));
	FHoudiniApi::GetNodeInfo = (GetNodeInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetNodeInfo"));
	FHoudiniApi::GetEditableNodeNetworks = (GetEditableNodeNetworksFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetEditableNodeNetworks"));
	FHoudiniApi::GetNodeNetworkChildren = (GetNodeNetworkChildrenFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetNodeNetworkChildren"));
	FHoudiniApi::CreateNode = (CreateNodeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CreateNode"));
	FHoudiniApi::DeleteNode = (DeleteNodeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_DeleteNode"));
	FHoudiniApi::RenameNode = (RenameNodeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_RenameNode"));
	FHoudiniApi::ConnectNodeInput = (ConnectNodeInputFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConnectNodeInput"));
	FHoudiniApi::DisconnectNodeInput = (DisconnectNodeInputFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_DisconnectNodeInput"));
	FHoudiniApi::QueryNodeInput = (QueryNodeInputFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_QueryNodeInput"));
	FHoudiniApi::GetParameters = (GetParametersFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParameters"));
	FHoudiniApi::GetParmInfo = (GetParmInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmInfo"));
	FHoudiniApi::GetParmIdFromName = (GetParmIdFromNameFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmIdFromName"));
	FHoudiniApi::GetParmInfoFromName = (GetParmInfoFromNameFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmInfoFromName"));
	FHoudiniApi::GetParmIntValue = (GetParmIntValueFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmIntValue"));
	FHoudiniApi::GetParmIntValues = (GetParmIntValuesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmIntValues"));
	FHoudiniApi::GetParmFloatValue = (GetParmFloatValueFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmFloatValue"));
	FHoudiniApi::GetParmFloatValues = (GetParmFloatValuesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmFloatValues"));
	FHoudiniApi::GetParmStringValue = (GetParmStringValueFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmStringValue"));
	FHoudiniApi::GetParmStringValues = (GetParmStringValuesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmStringValues"));
	FHoudiniApi::GetParmChoiceLists = (GetParmChoiceListsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetParmChoiceLists"));
	FHoudiniApi::SetParmIntValue = (SetParmIntValueFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetParmIntValue"));
	FHoudiniApi::SetParmIntValues = (SetParmIntValuesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetParmIntValues"));
	FHoudiniApi::SetParmFloatValue = (SetParmFloatValueFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetParmFloatValue"));
	FHoudiniApi::SetParmFloatValues = (SetParmFloatValuesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetParmFloatValues"));
	FHoudiniApi::SetParmStringValue = (SetParmStringValueFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetParmStringValue"));
	FHoudiniApi::InsertMultiparmInstance = (InsertMultiparmInstanceFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_InsertMultiparmInstance"));
	FHoudiniApi::RemoveMultiparmInstance = (RemoveMultiparmInstanceFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_RemoveMultiparmInstance"));
	FHoudiniApi::GetHandleInfo = (GetHandleInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetHandleInfo"));
	FHoudiniApi::GetHandleBindingInfo = (GetHandleBindingInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetHandleBindingInfo"));
	FHoudiniApi::GetPresetBufLength = (GetPresetBufLengthFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetPresetBufLength"));
	FHoudiniApi::GetPreset = (GetPresetFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetPreset"));
	FHoudiniApi::SetPreset = (SetPresetFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetPreset"));
	FHoudiniApi::GetObjects = (GetObjectsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetObjects"));
	FHoudiniApi::GetObjectTransforms = (GetObjectTransformsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetObjectTransforms"));
	FHoudiniApi::GetInstanceTransforms = (GetInstanceTransformsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetInstanceTransforms"));
	FHoudiniApi::SetObjectTransform = (SetObjectTransformFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetObjectTransform"));
	FHoudiniApi::GetGeoInfo = (GetGeoInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetGeoInfo"));
	FHoudiniApi::GetPartInfo = (GetPartInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetPartInfo"));
	FHoudiniApi::GetFaceCounts = (GetFaceCountsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetFaceCounts"));
	FHoudiniApi::GetVertexList = (GetVertexListFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetVertexList"));
	FHoudiniApi::GetAttributeInfo = (GetAttributeInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAttributeInfo"));
	FHoudiniApi::GetAttributeNames = (GetAttributeNamesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAttributeNames"));
	FHoudiniApi::GetAttributeIntData = (GetAttributeIntDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAttributeIntData"));
	FHoudiniApi::GetAttributeFloatData = (GetAttributeFloatDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAttributeFloatData"));
	FHoudiniApi::GetAttributeStringData = (GetAttributeStringDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetAttributeStringData"));
	FHoudiniApi::GetGroupNames = (GetGroupNamesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetGroupNames"));
	FHoudiniApi::GetGroupMembership = (GetGroupMembershipFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetGroupMembership"));
	FHoudiniApi::GetInstancedPartIds = (GetInstancedPartIdsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetInstancedPartIds"));
	FHoudiniApi::GetInstancerPartTransforms = (GetInstancerPartTransformsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetInstancerPartTransforms"));
	FHoudiniApi::SetGeoInfo = (SetGeoInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetGeoInfo"));
	FHoudiniApi::SetPartInfo = (SetPartInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetPartInfo"));
	FHoudiniApi::SetFaceCounts = (SetFaceCountsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetFaceCounts"));
	FHoudiniApi::SetVertexList = (SetVertexListFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetVertexList"));
	FHoudiniApi::AddAttribute = (AddAttributeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_AddAttribute"));
	FHoudiniApi::SetAttributeIntData = (SetAttributeIntDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetAttributeIntData"));
	FHoudiniApi::SetAttributeFloatData = (SetAttributeFloatDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetAttributeFloatData"));
	FHoudiniApi::SetAttributeStringData = (SetAttributeStringDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetAttributeStringData"));
	FHoudiniApi::AddGroup = (AddGroupFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_AddGroup"));
	FHoudiniApi::SetGroupMembership = (SetGroupMembershipFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetGroupMembership"));
	FHoudiniApi::CommitGeo = (CommitGeoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_CommitGeo"));
	FHoudiniApi::RevertGeo = (RevertGeoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_RevertGeo"));
	FHoudiniApi::ConnectAssetTransform = (ConnectAssetTransformFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConnectAssetTransform"));
	FHoudiniApi::DisconnectAssetTransform = (DisconnectAssetTransformFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_DisconnectAssetTransform"));
	FHoudiniApi::ConnectAssetGeometry = (ConnectAssetGeometryFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ConnectAssetGeometry"));
	FHoudiniApi::DisconnectAssetGeometry = (DisconnectAssetGeometryFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_DisconnectAssetGeometry"));
	FHoudiniApi::GetMaterialIdsOnFaces = (GetMaterialIdsOnFacesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetMaterialIdsOnFaces"));
	FHoudiniApi::GetMaterialInfo = (GetMaterialInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetMaterialInfo"));
	FHoudiniApi::RenderTextureToImage = (RenderTextureToImageFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_RenderTextureToImage"));
	FHoudiniApi::GetSupportedImageFileFormatCount = (GetSupportedImageFileFormatCountFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetSupportedImageFileFormatCount"));
	FHoudiniApi::GetSupportedImageFileFormats = (GetSupportedImageFileFormatsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetSupportedImageFileFormats"));
	FHoudiniApi::GetImageInfo = (GetImageInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetImageInfo"));
	FHoudiniApi::SetImageInfo = (SetImageInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetImageInfo"));
	FHoudiniApi::GetImagePlaneCount = (GetImagePlaneCountFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetImagePlaneCount"));
	FHoudiniApi::GetImagePlanes = (GetImagePlanesFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetImagePlanes"));
	FHoudiniApi::ExtractImageToFile = (ExtractImageToFileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ExtractImageToFile"));
	FHoudiniApi::ExtractImageToMemory = (ExtractImageToMemoryFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ExtractImageToMemory"));
	FHoudiniApi::GetImageMemoryBuffer = (GetImageMemoryBufferFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetImageMemoryBuffer"));
	FHoudiniApi::SetAnimCurve = (SetAnimCurveFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetAnimCurve"));
	FHoudiniApi::SetTransformAnimCurve = (SetTransformAnimCurveFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetTransformAnimCurve"));
	FHoudiniApi::ResetSimulation = (ResetSimulationFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_ResetSimulation"));
	FHoudiniApi::GetVolumeInfo = (GetVolumeInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetVolumeInfo"));
	FHoudiniApi::GetFirstVolumeTile = (GetFirstVolumeTileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetFirstVolumeTile"));
	FHoudiniApi::GetNextVolumeTile = (GetNextVolumeTileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetNextVolumeTile"));
	FHoudiniApi::GetVolumeVoxelFloatData = (GetVolumeVoxelFloatDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetVolumeVoxelFloatData"));
	FHoudiniApi::GetVolumeTileFloatData = (GetVolumeTileFloatDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetVolumeTileFloatData"));
	FHoudiniApi::GetVolumeVoxelIntData = (GetVolumeVoxelIntDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetVolumeVoxelIntData"));
	FHoudiniApi::GetVolumeTileIntData = (GetVolumeTileIntDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetVolumeTileIntData"));
	FHoudiniApi::SetVolumeInfo = (SetVolumeInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetVolumeInfo"));
	FHoudiniApi::SetVolumeTileFloatData = (SetVolumeTileFloatDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetVolumeTileFloatData"));
	FHoudiniApi::SetVolumeTileIntData = (SetVolumeTileIntDataFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetVolumeTileIntData"));
	FHoudiniApi::GetCurveInfo = (GetCurveInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetCurveInfo"));
	FHoudiniApi::GetCurveCounts = (GetCurveCountsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetCurveCounts"));
	FHoudiniApi::GetCurveOrders = (GetCurveOrdersFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetCurveOrders"));
	FHoudiniApi::GetCurveKnots = (GetCurveKnotsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetCurveKnots"));
	FHoudiniApi::SetCurveInfo = (SetCurveInfoFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetCurveInfo"));
	FHoudiniApi::SetCurveCounts = (SetCurveCountsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetCurveCounts"));
	FHoudiniApi::SetCurveOrders = (SetCurveOrdersFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetCurveOrders"));
	FHoudiniApi::SetCurveKnots = (SetCurveKnotsFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SetCurveKnots"));
	FHoudiniApi::SaveGeoToFile = (SaveGeoToFileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SaveGeoToFile"));
	FHoudiniApi::LoadGeoFromFile = (LoadGeoFromFileFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_LoadGeoFromFile"));
	FHoudiniApi::GetGeoSize = (GetGeoSizeFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetGeoSize"));
	FHoudiniApi::SaveGeoToMemory = (SaveGeoToMemoryFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_SaveGeoToMemory"));
	FHoudiniApi::LoadGeoFromMemory = (LoadGeoFromMemoryFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_LoadGeoFromMemory"));
	FHoudiniApi::GetMaterialOnPart = (GetMaterialOnPartFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetMaterialOnPart"));
	FHoudiniApi::GetMaterialOnGroup = (GetMaterialOnGroupFuncPtr) FPlatformProcess::GetDllExport(LibraryHandle, TEXT("HAPI_GetMaterialOnGroup"));
}


void
FHoudiniApi::FinalizeHAPI()
{
	FHoudiniApi::CreateInProcessSession = nullptr;
	FHoudiniApi::StartThriftSocketServer = nullptr;
	FHoudiniApi::CreateThriftSocketSession = nullptr;
	FHoudiniApi::StartThriftNamedPipeServer = nullptr;
	FHoudiniApi::CreateThriftNamedPipeSession = nullptr;
	FHoudiniApi::BindCustomImplementation = nullptr;
	FHoudiniApi::CreateCustomSession = nullptr;
	FHoudiniApi::IsSessionValid = nullptr;
	FHoudiniApi::CloseSession = nullptr;
	FHoudiniApi::IsInitialized = nullptr;
	FHoudiniApi::Initialize = nullptr;
	FHoudiniApi::Cleanup = nullptr;
	FHoudiniApi::GetEnvInt = nullptr;
	FHoudiniApi::GetSessionEnvInt = nullptr;
	FHoudiniApi::GetStatus = nullptr;
	FHoudiniApi::GetStatusStringBufLength = nullptr;
	FHoudiniApi::GetStatusString = nullptr;
	FHoudiniApi::GetCookingTotalCount = nullptr;
	FHoudiniApi::GetCookingCurrentCount = nullptr;
	FHoudiniApi::ConvertTransform = nullptr;
	FHoudiniApi::ConvertMatrixToQuat = nullptr;
	FHoudiniApi::ConvertMatrixToEuler = nullptr;
	FHoudiniApi::ConvertTransformQuatToMatrix = nullptr;
	FHoudiniApi::ConvertTransformEulerToMatrix = nullptr;
	FHoudiniApi::PythonThreadInterpreterLock = nullptr;
	FHoudiniApi::GetStringBufLength = nullptr;
	FHoudiniApi::GetString = nullptr;
	FHoudiniApi::GetTime = nullptr;
	FHoudiniApi::SetTime = nullptr;
	FHoudiniApi::GetTimelineOptions = nullptr;
	FHoudiniApi::SetTimelineOptions = nullptr;
	FHoudiniApi::IsAssetValid = nullptr;
	FHoudiniApi::LoadAssetLibraryFromFile = nullptr;
	FHoudiniApi::LoadAssetLibraryFromMemory = nullptr;
	FHoudiniApi::GetAvailableAssetCount = nullptr;
	FHoudiniApi::GetAvailableAssets = nullptr;
	FHoudiniApi::InstantiateAsset = nullptr;
	FHoudiniApi::CreateCurve = nullptr;
	FHoudiniApi::CreateInputAsset = nullptr;
	FHoudiniApi::DestroyAsset = nullptr;
	FHoudiniApi::GetAssetInfo = nullptr;
	FHoudiniApi::CookAsset = nullptr;
	FHoudiniApi::Interrupt = nullptr;
	FHoudiniApi::GetAssetTransform = nullptr;
	FHoudiniApi::SetAssetTransform = nullptr;
	FHoudiniApi::GetInputName = nullptr;
	FHoudiniApi::LoadHIPFile = nullptr;
	FHoudiniApi::CheckForNewAssets = nullptr;
	FHoudiniApi::GetNewAssetIds = nullptr;
	FHoudiniApi::SaveHIPFile = nullptr;
	FHoudiniApi::GetNodeInfo = nullptr;
	FHoudiniApi::GetEditableNodeNetworks = nullptr;
	FHoudiniApi::GetNodeNetworkChildren = nullptr;
	FHoudiniApi::CreateNode = nullptr;
	FHoudiniApi::DeleteNode = nullptr;
	FHoudiniApi::RenameNode = nullptr;
	FHoudiniApi::ConnectNodeInput = nullptr;
	FHoudiniApi::DisconnectNodeInput = nullptr;
	FHoudiniApi::QueryNodeInput = nullptr;
	FHoudiniApi::GetParameters = nullptr;
	FHoudiniApi::GetParmInfo = nullptr;
	FHoudiniApi::GetParmIdFromName = nullptr;
	FHoudiniApi::GetParmInfoFromName = nullptr;
	FHoudiniApi::GetParmIntValue = nullptr;
	FHoudiniApi::GetParmIntValues = nullptr;
	FHoudiniApi::GetParmFloatValue = nullptr;
	FHoudiniApi::GetParmFloatValues = nullptr;
	FHoudiniApi::GetParmStringValue = nullptr;
	FHoudiniApi::GetParmStringValues = nullptr;
	FHoudiniApi::GetParmChoiceLists = nullptr;
	FHoudiniApi::SetParmIntValue = nullptr;
	FHoudiniApi::SetParmIntValues = nullptr;
	FHoudiniApi::SetParmFloatValue = nullptr;
	FHoudiniApi::SetParmFloatValues = nullptr;
	FHoudiniApi::SetParmStringValue = nullptr;
	FHoudiniApi::InsertMultiparmInstance = nullptr;
	FHoudiniApi::RemoveMultiparmInstance = nullptr;
	FHoudiniApi::GetHandleInfo = nullptr;
	FHoudiniApi::GetHandleBindingInfo = nullptr;
	FHoudiniApi::GetPresetBufLength = nullptr;
	FHoudiniApi::GetPreset = nullptr;
	FHoudiniApi::SetPreset = nullptr;
	FHoudiniApi::GetObjects = nullptr;
	FHoudiniApi::GetObjectTransforms = nullptr;
	FHoudiniApi::GetInstanceTransforms = nullptr;
	FHoudiniApi::SetObjectTransform = nullptr;
	FHoudiniApi::GetGeoInfo = nullptr;
	FHoudiniApi::GetPartInfo = nullptr;
	FHoudiniApi::GetFaceCounts = nullptr;
	FHoudiniApi::GetVertexList = nullptr;
	FHoudiniApi::GetAttributeInfo = nullptr;
	FHoudiniApi::GetAttributeNames = nullptr;
	FHoudiniApi::GetAttributeIntData = nullptr;
	FHoudiniApi::GetAttributeFloatData = nullptr;
	FHoudiniApi::GetAttributeStringData = nullptr;
	FHoudiniApi::GetGroupNames = nullptr;
	FHoudiniApi::GetGroupMembership = nullptr;
	FHoudiniApi::GetInstancedPartIds = nullptr;
	FHoudiniApi::GetInstancerPartTransforms = nullptr;
	FHoudiniApi::SetGeoInfo = nullptr;
	FHoudiniApi::SetPartInfo = nullptr;
	FHoudiniApi::SetFaceCounts = nullptr;
	FHoudiniApi::SetVertexList = nullptr;
	FHoudiniApi::AddAttribute = nullptr;
	FHoudiniApi::SetAttributeIntData = nullptr;
	FHoudiniApi::SetAttributeFloatData = nullptr;
	FHoudiniApi::SetAttributeStringData = nullptr;
	FHoudiniApi::AddGroup = nullptr;
	FHoudiniApi::SetGroupMembership = nullptr;
	FHoudiniApi::CommitGeo = nullptr;
	FHoudiniApi::RevertGeo = nullptr;
	FHoudiniApi::ConnectAssetTransform = nullptr;
	FHoudiniApi::DisconnectAssetTransform = nullptr;
	FHoudiniApi::ConnectAssetGeometry = nullptr;
	FHoudiniApi::DisconnectAssetGeometry = nullptr;
	FHoudiniApi::GetMaterialIdsOnFaces = nullptr;
	FHoudiniApi::GetMaterialInfo = nullptr;
	FHoudiniApi::RenderTextureToImage = nullptr;
	FHoudiniApi::GetSupportedImageFileFormatCount = nullptr;
	FHoudiniApi::GetSupportedImageFileFormats = nullptr;
	FHoudiniApi::GetImageInfo = nullptr;
	FHoudiniApi::SetImageInfo = nullptr;
	FHoudiniApi::GetImagePlaneCount = nullptr;
	FHoudiniApi::GetImagePlanes = nullptr;
	FHoudiniApi::ExtractImageToFile = nullptr;
	FHoudiniApi::ExtractImageToMemory = nullptr;
	FHoudiniApi::GetImageMemoryBuffer = nullptr;
	FHoudiniApi::SetAnimCurve = nullptr;
	FHoudiniApi::SetTransformAnimCurve = nullptr;
	FHoudiniApi::ResetSimulation = nullptr;
	FHoudiniApi::GetVolumeInfo = nullptr;
	FHoudiniApi::GetFirstVolumeTile = nullptr;
	FHoudiniApi::GetNextVolumeTile = nullptr;
	FHoudiniApi::GetVolumeVoxelFloatData = nullptr;
	FHoudiniApi::GetVolumeTileFloatData = nullptr;
	FHoudiniApi::GetVolumeVoxelIntData = nullptr;
	FHoudiniApi::GetVolumeTileIntData = nullptr;
	FHoudiniApi::SetVolumeInfo = nullptr;
	FHoudiniApi::SetVolumeTileFloatData = nullptr;
	FHoudiniApi::SetVolumeTileIntData = nullptr;
	FHoudiniApi::GetCurveInfo = nullptr;
	FHoudiniApi::GetCurveCounts = nullptr;
	FHoudiniApi::GetCurveOrders = nullptr;
	FHoudiniApi::GetCurveKnots = nullptr;
	FHoudiniApi::SetCurveInfo = nullptr;
	FHoudiniApi::SetCurveCounts = nullptr;
	FHoudiniApi::SetCurveOrders = nullptr;
	FHoudiniApi::SetCurveKnots = nullptr;
	FHoudiniApi::SaveGeoToFile = nullptr;
	FHoudiniApi::LoadGeoFromFile = nullptr;
	FHoudiniApi::GetGeoSize = nullptr;
	FHoudiniApi::SaveGeoToMemory = nullptr;
	FHoudiniApi::LoadGeoFromMemory = nullptr;
	FHoudiniApi::GetMaterialOnPart = nullptr;
	FHoudiniApi::GetMaterialOnGroup = nullptr;
}


bool
FHoudiniApi::IsHAPIInitialized()
{
	return FHoudiniApi::IsInitialized != nullptr;
}
