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

#pragma once
#include "HAPI.h"


struct HOUDINIENGINERUNTIME_API FHoudiniApi
{
	static void InitializeHAPI(void* LibraryHandle);
	static void FinalizeHAPI();
	static bool IsHAPIInitialized();

	typedef HAPI_Result (*CreateInProcessSessionFuncPtr)(HAPI_Session * session);
	static CreateInProcessSessionFuncPtr CreateInProcessSession;

	typedef HAPI_Result (*StartThriftSocketServerFuncPtr)(const HAPI_ThriftServerOptions * options, int port, HAPI_ProcessId * process_id);
	static StartThriftSocketServerFuncPtr StartThriftSocketServer;

	typedef HAPI_Result (*CreateThriftSocketSessionFuncPtr)(HAPI_Session * session, const char * host_name, int port, HAPI_ThriftTransportType transport_type);
	static CreateThriftSocketSessionFuncPtr CreateThriftSocketSession;

	typedef HAPI_Result (*StartThriftNamedPipeServerFuncPtr)(const HAPI_ThriftServerOptions * options, const char * pipe_name, HAPI_ProcessId * process_id);
	static StartThriftNamedPipeServerFuncPtr StartThriftNamedPipeServer;

	typedef HAPI_Result (*CreateThriftNamedPipeSessionFuncPtr)(HAPI_Session * session, const char * pipe_name, HAPI_ThriftTransportType transport_type);
	static CreateThriftNamedPipeSessionFuncPtr CreateThriftNamedPipeSession;

	typedef HAPI_Result (*BindCustomImplementationFuncPtr)(HAPI_SessionType session_type, const char * dll_path);
	static BindCustomImplementationFuncPtr BindCustomImplementation;

	typedef HAPI_Result (*CreateCustomSessionFuncPtr)(HAPI_SessionType session_type, void * session_info, HAPI_Session * session);
	static CreateCustomSessionFuncPtr CreateCustomSession;

	typedef HAPI_Result (*IsSessionValidFuncPtr)(const HAPI_Session * session);
	static IsSessionValidFuncPtr IsSessionValid;

	typedef HAPI_Result (*CloseSessionFuncPtr)(const HAPI_Session * session);
	static CloseSessionFuncPtr CloseSession;

	typedef HAPI_Result (*IsInitializedFuncPtr)(const HAPI_Session * session);
	static IsInitializedFuncPtr IsInitialized;

	typedef HAPI_Result (*InitializeFuncPtr)(const HAPI_Session * session, const HAPI_CookOptions * cook_options, HAPI_Bool use_cooking_thread, int cooking_thread_stack_size, const char * houdini_environment_files, const char * otl_search_path, const char * dso_search_path, const char * image_dso_search_path, const char * audio_dso_search_path);
	static InitializeFuncPtr Initialize;

	typedef HAPI_Result (*CleanupFuncPtr)(const HAPI_Session * session);
	static CleanupFuncPtr Cleanup;

	typedef HAPI_Result (*GetEnvIntFuncPtr)(HAPI_EnvIntType int_type, int * value);
	static GetEnvIntFuncPtr GetEnvInt;

	typedef HAPI_Result (*GetSessionEnvIntFuncPtr)(const HAPI_Session * session, HAPI_SessionEnvIntType int_type, int * value);
	static GetSessionEnvIntFuncPtr GetSessionEnvInt;

	typedef HAPI_Result (*GetStatusFuncPtr)(const HAPI_Session * session, HAPI_StatusType status_type, int * status);
	static GetStatusFuncPtr GetStatus;

	typedef HAPI_Result (*GetStatusStringBufLengthFuncPtr)(const HAPI_Session * session, HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity, int * buffer_length);
	static GetStatusStringBufLengthFuncPtr GetStatusStringBufLength;

	typedef HAPI_Result (*GetStatusStringFuncPtr)(const HAPI_Session * session, HAPI_StatusType status_type, char * string_value, int length);
	static GetStatusStringFuncPtr GetStatusString;

	typedef HAPI_Result (*ComposeNodeCookResultFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_StatusVerbosity verbosity, int * buffer_length);
	static ComposeNodeCookResultFuncPtr ComposeNodeCookResult;

	typedef HAPI_Result (*GetComposedNodeCookResultFuncPtr)(const HAPI_Session * session, char * string_value, int length);
	static GetComposedNodeCookResultFuncPtr GetComposedNodeCookResult;

	typedef HAPI_Result (*GetCookingTotalCountFuncPtr)(const HAPI_Session * session, int * count);
	static GetCookingTotalCountFuncPtr GetCookingTotalCount;

	typedef HAPI_Result (*GetCookingCurrentCountFuncPtr)(const HAPI_Session * session, int * count);
	static GetCookingCurrentCountFuncPtr GetCookingCurrentCount;

	typedef HAPI_Result (*ConvertTransformFuncPtr)(const HAPI_Session * session, const HAPI_TransformEuler * transform_in, HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order, HAPI_TransformEuler * transform_out);
	static ConvertTransformFuncPtr ConvertTransform;

	typedef HAPI_Result (*ConvertMatrixToQuatFuncPtr)(const HAPI_Session * session, const float * matrix, HAPI_RSTOrder rst_order, HAPI_Transform * transform_out);
	static ConvertMatrixToQuatFuncPtr ConvertMatrixToQuat;

	typedef HAPI_Result (*ConvertMatrixToEulerFuncPtr)(const HAPI_Session * session, const float * matrix, HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order, HAPI_TransformEuler * transform_out);
	static ConvertMatrixToEulerFuncPtr ConvertMatrixToEuler;

	typedef HAPI_Result (*ConvertTransformQuatToMatrixFuncPtr)(const HAPI_Session * session, const HAPI_Transform * transform, float * matrix);
	static ConvertTransformQuatToMatrixFuncPtr ConvertTransformQuatToMatrix;

	typedef HAPI_Result (*ConvertTransformEulerToMatrixFuncPtr)(const HAPI_Session * session, const HAPI_TransformEuler * transform, float * matrix);
	static ConvertTransformEulerToMatrixFuncPtr ConvertTransformEulerToMatrix;

	typedef HAPI_Result (*PythonThreadInterpreterLockFuncPtr)(const HAPI_Session * session, HAPI_Bool locked);
	static PythonThreadInterpreterLockFuncPtr PythonThreadInterpreterLock;

	typedef HAPI_Result (*GetStringBufLengthFuncPtr)(const HAPI_Session * session, HAPI_StringHandle string_handle, int * buffer_length);
	static GetStringBufLengthFuncPtr GetStringBufLength;

	typedef HAPI_Result (*GetStringFuncPtr)(const HAPI_Session * session, HAPI_StringHandle string_handle, char * string_value, int length);
	static GetStringFuncPtr GetString;

	typedef HAPI_Result (*GetTimeFuncPtr)(const HAPI_Session * session, float * time);
	static GetTimeFuncPtr GetTime;

	typedef HAPI_Result (*SetTimeFuncPtr)(const HAPI_Session * session, float time);
	static SetTimeFuncPtr SetTime;

	typedef HAPI_Result (*GetTimelineOptionsFuncPtr)(const HAPI_Session * session, HAPI_TimelineOptions * timeline_options);
	static GetTimelineOptionsFuncPtr GetTimelineOptions;

	typedef HAPI_Result (*SetTimelineOptionsFuncPtr)(const HAPI_Session * session, const HAPI_TimelineOptions * timeline_options);
	static SetTimelineOptionsFuncPtr SetTimelineOptions;

	typedef HAPI_Result (*IsAssetValidFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, int asset_validation_id, int * answer);
	static IsAssetValidFuncPtr IsAssetValid;

	typedef HAPI_Result (*LoadAssetLibraryFromFileFuncPtr)(const HAPI_Session * session, const char * file_path, HAPI_Bool allow_overwrite, HAPI_AssetLibraryId* library_id);
	static LoadAssetLibraryFromFileFuncPtr LoadAssetLibraryFromFile;

	typedef HAPI_Result (*LoadAssetLibraryFromMemoryFuncPtr)(const HAPI_Session * session, const char * library_buffer, int library_buffer_length, HAPI_Bool allow_overwrite, HAPI_AssetLibraryId * library_id);
	static LoadAssetLibraryFromMemoryFuncPtr LoadAssetLibraryFromMemory;

	typedef HAPI_Result (*GetAvailableAssetCountFuncPtr)(const HAPI_Session * session, HAPI_AssetLibraryId library_id, int * asset_count);
	static GetAvailableAssetCountFuncPtr GetAvailableAssetCount;

	typedef HAPI_Result (*GetAvailableAssetsFuncPtr)(const HAPI_Session * session, HAPI_AssetLibraryId library_id, HAPI_StringHandle * asset_names_array, int asset_count);
	static GetAvailableAssetsFuncPtr GetAvailableAssets;

	typedef HAPI_Result (*InstantiateAssetFuncPtr)(const HAPI_Session * session, const char * asset_name, HAPI_Bool cook_on_load, HAPI_AssetId * asset_id);
	static InstantiateAssetFuncPtr InstantiateAsset;

	typedef HAPI_Result (*CreateCurveFuncPtr)(const HAPI_Session * session, HAPI_AssetId * asset_id);
	static CreateCurveFuncPtr CreateCurve;

	typedef HAPI_Result (*CreateInputAssetFuncPtr)(const HAPI_Session * session, HAPI_AssetId * asset_id, const char * name);
	static CreateInputAssetFuncPtr CreateInputAsset;

	typedef HAPI_Result (*DestroyAssetFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id);
	static DestroyAssetFuncPtr DestroyAsset;

	typedef HAPI_Result (*GetAssetInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_AssetInfo * asset_info);
	static GetAssetInfoFuncPtr GetAssetInfo;

	typedef HAPI_Result (*CookAssetFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, const HAPI_CookOptions * cook_options);
	static CookAssetFuncPtr CookAsset;

	typedef HAPI_Result (*InterruptFuncPtr)(const HAPI_Session * session);
	static InterruptFuncPtr Interrupt;

	typedef HAPI_Result (*GetAssetTransformFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order, HAPI_TransformEuler * transform);
	static GetAssetTransformFuncPtr GetAssetTransform;

	typedef HAPI_Result (*SetAssetTransformFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, const HAPI_TransformEuler * transform);
	static SetAssetTransformFuncPtr SetAssetTransform;

	typedef HAPI_Result (*GetInputNameFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, int input_idx, int input_type, HAPI_StringHandle * name);
	static GetInputNameFuncPtr GetInputName;

	typedef HAPI_Result (*LoadHIPFileFuncPtr)(const HAPI_Session * session, const char * file_name, HAPI_Bool cook_on_load);
	static LoadHIPFileFuncPtr LoadHIPFile;

	typedef HAPI_Result (*CheckForNewAssetsFuncPtr)(const HAPI_Session * session, int * new_asset_count);
	static CheckForNewAssetsFuncPtr CheckForNewAssets;

	typedef HAPI_Result (*GetNewAssetIdsFuncPtr)(const HAPI_Session * session, HAPI_AssetId * asset_ids_array, int new_asset_count);
	static GetNewAssetIdsFuncPtr GetNewAssetIds;

	typedef HAPI_Result (*SaveHIPFileFuncPtr)(const HAPI_Session * session, const char * file_path, HAPI_Bool lock_nodes);
	static SaveHIPFileFuncPtr SaveHIPFile;

	typedef HAPI_Result (*GetNodeInfoFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_NodeInfo * node_info);
	static GetNodeInfoFuncPtr GetNodeInfo;

	typedef HAPI_Result (*GetEditableNodeNetworksFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_NodeId * node_networks_array, int count);
	static GetEditableNodeNetworksFuncPtr GetEditableNodeNetworks;

	typedef HAPI_Result (*GetNodeNetworkChildrenFuncPtr)(const HAPI_Session * session, HAPI_NodeId network_node_id, HAPI_NodeId * child_node_ids_array, int count);
	static GetNodeNetworkChildrenFuncPtr GetNodeNetworkChildren;

	typedef HAPI_Result (*CreateNodeFuncPtr)(const HAPI_Session * session, HAPI_NodeId parent_node_id, const char * operator_name, HAPI_NodeId * new_node_id);
	static CreateNodeFuncPtr CreateNode;

	typedef HAPI_Result (*DeleteNodeFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id);
	static DeleteNodeFuncPtr DeleteNode;

	typedef HAPI_Result (*RenameNodeFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * new_name);
	static RenameNodeFuncPtr RenameNode;

	typedef HAPI_Result (*ConnectNodeInputFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, int input_index, HAPI_NodeId node_id_to_connect);
	static ConnectNodeInputFuncPtr ConnectNodeInput;

	typedef HAPI_Result (*DisconnectNodeInputFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, int input_index);
	static DisconnectNodeInputFuncPtr DisconnectNodeInput;

	typedef HAPI_Result (*QueryNodeInputFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_to_query, int input_index, HAPI_NodeId * connected_node_id);
	static QueryNodeInputFuncPtr QueryNodeInput;

	typedef HAPI_Result (*GetParametersFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_ParmInfo * parm_infos_array, int start, int length);
	static GetParametersFuncPtr GetParameters;

	typedef HAPI_Result (*GetParmInfoFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_ParmId parm_id, HAPI_ParmInfo * parm_info);
	static GetParmInfoFuncPtr GetParmInfo;

	typedef HAPI_Result (*GetParmIdFromNameFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, HAPI_ParmId * parm_id);
	static GetParmIdFromNameFuncPtr GetParmIdFromName;

	typedef HAPI_Result (*GetParmInfoFromNameFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, HAPI_ParmInfo * parm_info);
	static GetParmInfoFromNameFuncPtr GetParmInfoFromName;

	typedef HAPI_Result (*GetParmIntValueFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, int index, int * value);
	static GetParmIntValueFuncPtr GetParmIntValue;

	typedef HAPI_Result (*GetParmIntValuesFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, int * values_array, int start, int length);
	static GetParmIntValuesFuncPtr GetParmIntValues;

	typedef HAPI_Result (*GetParmFloatValueFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, int index, float * value);
	static GetParmFloatValueFuncPtr GetParmFloatValue;

	typedef HAPI_Result (*GetParmFloatValuesFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, float * values_array, int start, int length);
	static GetParmFloatValuesFuncPtr GetParmFloatValues;

	typedef HAPI_Result (*GetParmStringValueFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, int index, HAPI_Bool evaluate, HAPI_StringHandle * value);
	static GetParmStringValueFuncPtr GetParmStringValue;

	typedef HAPI_Result (*GetParmStringValuesFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_Bool evaluate, HAPI_StringHandle * values_array, int start, int length);
	static GetParmStringValuesFuncPtr GetParmStringValues;

	typedef HAPI_Result (*GetParmChoiceListsFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_ParmChoiceInfo *parm_choices_array, int start, int length);
	static GetParmChoiceListsFuncPtr GetParmChoiceLists;

	typedef HAPI_Result (*SetParmIntValueFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, int index, int value);
	static SetParmIntValueFuncPtr SetParmIntValue;

	typedef HAPI_Result (*SetParmIntValuesFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const int * values_array, int start, int length);
	static SetParmIntValuesFuncPtr SetParmIntValues;

	typedef HAPI_Result (*SetParmFloatValueFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * parm_name, int index, float value);
	static SetParmFloatValueFuncPtr SetParmFloatValue;

	typedef HAPI_Result (*SetParmFloatValuesFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const float * values_array, int start, int length);
	static SetParmFloatValuesFuncPtr SetParmFloatValues;

	typedef HAPI_Result (*SetParmStringValueFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, const char * value, HAPI_ParmId parm_id, int index);
	static SetParmStringValueFuncPtr SetParmStringValue;

	typedef HAPI_Result (*InsertMultiparmInstanceFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_ParmId parm_id, int instance_position);
	static InsertMultiparmInstanceFuncPtr InsertMultiparmInstance;

	typedef HAPI_Result (*RemoveMultiparmInstanceFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_ParmId parm_id, int instance_position);
	static RemoveMultiparmInstanceFuncPtr RemoveMultiparmInstance;

	typedef HAPI_Result (*GetHandleInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_HandleInfo * handle_infos_array, int start, int length);
	static GetHandleInfoFuncPtr GetHandleInfo;

	typedef HAPI_Result (*GetHandleBindingInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, int handle_index, HAPI_HandleBindingInfo * handle_binding_infos_array, int start, int length);
	static GetHandleBindingInfoFuncPtr GetHandleBindingInfo;

	typedef HAPI_Result (*GetPresetBufLengthFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_PresetType preset_type, const char * preset_name, int * buffer_length);
	static GetPresetBufLengthFuncPtr GetPresetBufLength;

	typedef HAPI_Result (*GetPresetFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, char * buffer, int buffer_length);
	static GetPresetFuncPtr GetPreset;

	typedef HAPI_Result (*SetPresetFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_PresetType preset_type, const char * preset_name, const char * buffer, int buffer_length);
	static SetPresetFuncPtr SetPreset;

	typedef HAPI_Result (*GetObjectsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectInfo * object_infos_array, int start, int length);
	static GetObjectsFuncPtr GetObjects;

	typedef HAPI_Result (*GetObjectTransformsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_RSTOrder rst_order, HAPI_Transform * transforms_array, int start, int length);
	static GetObjectTransformsFuncPtr GetObjectTransforms;

	typedef HAPI_Result (*GetInstanceTransformsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_RSTOrder rst_order, HAPI_Transform * transforms_array, int start, int length);
	static GetInstanceTransformsFuncPtr GetInstanceTransforms;

	typedef HAPI_Result (*SetObjectTransformFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, const HAPI_TransformEuler * transform);
	static SetObjectTransformFuncPtr SetObjectTransform;

	typedef HAPI_Result (*GetGeoInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GeoInfo * geo_info);
	static GetGeoInfoFuncPtr GetGeoInfo;

	typedef HAPI_Result (*GetPartInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartInfo * part_info);
	static GetPartInfoFuncPtr GetPartInfo;

	typedef HAPI_Result (*GetFaceCountsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * face_counts_array, int start, int length);
	static GetFaceCountsFuncPtr GetFaceCounts;

	typedef HAPI_Result (*GetVertexListFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * vertex_list_array, int start, int length);
	static GetVertexListFuncPtr GetVertexList;

	typedef HAPI_Result (*GetAttributeInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeOwner owner, HAPI_AttributeInfo * attr_info);
	static GetAttributeInfoFuncPtr GetAttributeInfo;

	typedef HAPI_Result (*GetAttributeNamesFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_AttributeOwner owner, HAPI_StringHandle * attribute_names_array, int count);
	static GetAttributeNamesFuncPtr GetAttributeNames;

	typedef HAPI_Result (*GetAttributeIntDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeInfo * attr_info, int * data_array, int start, int length);
	static GetAttributeIntDataFuncPtr GetAttributeIntData;

	typedef HAPI_Result (*GetAttributeFloatDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeInfo * attr_info, float * data_array, int start, int length);
	static GetAttributeFloatDataFuncPtr GetAttributeFloatData;

	typedef HAPI_Result (*GetAttributeStringDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeInfo * attr_info, int * data_array, int start, int length);
	static GetAttributeStringDataFuncPtr GetAttributeStringData;

	typedef HAPI_Result (*GetGroupNamesFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GroupType group_type, HAPI_StringHandle * group_names_array, int group_count);
	static GetGroupNamesFuncPtr GetGroupNames;

	typedef HAPI_Result (*GetGroupMembershipFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_GroupType group_type, const char * group_name, int * membership_array, int start, int length);
	static GetGroupMembershipFuncPtr GetGroupMembership;

	typedef HAPI_Result (*GetInstancedPartIdsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartId * instanced_parts_array, int start, int length);
	static GetInstancedPartIdsFuncPtr GetInstancedPartIds;

	typedef HAPI_Result (*GetInstancerPartTransformsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_RSTOrder rst_order, HAPI_Transform * transforms_array, int start, int length);
	static GetInstancerPartTransformsFuncPtr GetInstancerPartTransforms;

	typedef HAPI_Result (*SetGeoInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GeoInfo * geo_info);
	static SetGeoInfoFuncPtr SetGeoInfo;

	typedef HAPI_Result (*SetPartInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_PartInfo * part_info);
	static SetPartInfoFuncPtr SetPartInfo;

	typedef HAPI_Result (*SetFaceCountsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const int * face_counts_array, int start, int length);
	static SetFaceCountsFuncPtr SetFaceCounts;

	typedef HAPI_Result (*SetVertexListFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const int * vertex_list_array, int start, int length);
	static SetVertexListFuncPtr SetVertexList;

	typedef HAPI_Result (*AddAttributeFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo * attr_info);
	static AddAttributeFuncPtr AddAttribute;

	typedef HAPI_Result (*SetAttributeIntDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo * attr_info, const int * data_array, int start, int length);
	static SetAttributeIntDataFuncPtr SetAttributeIntData;

	typedef HAPI_Result (*SetAttributeFloatDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo * attr_info, const float * data_array, int start, int length);
	static SetAttributeFloatDataFuncPtr SetAttributeFloatData;

	typedef HAPI_Result (*SetAttributeStringDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo *attr_info, const char ** data_array, int start, int length);
	static SetAttributeStringDataFuncPtr SetAttributeStringData;

	typedef HAPI_Result (*AddGroupFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GroupType group_type, const char * group_name);
	static AddGroupFuncPtr AddGroup;

	typedef HAPI_Result (*SetGroupMembershipFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GroupType group_type, const char * group_name, const int * membership_array, int start, int length);
	static SetGroupMembershipFuncPtr SetGroupMembership;

	typedef HAPI_Result (*CommitGeoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id);
	static CommitGeoFuncPtr CommitGeo;

	typedef HAPI_Result (*RevertGeoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id);
	static RevertGeoFuncPtr RevertGeo;

	typedef HAPI_Result (*ConnectAssetTransformFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id_from, HAPI_AssetId asset_id_to, int input_idx);
	static ConnectAssetTransformFuncPtr ConnectAssetTransform;

	typedef HAPI_Result (*DisconnectAssetTransformFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, int input_idx);
	static DisconnectAssetTransformFuncPtr DisconnectAssetTransform;

	typedef HAPI_Result (*ConnectAssetGeometryFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id_from, HAPI_ObjectId object_id_from, HAPI_AssetId asset_id_to, int input_idx);
	static ConnectAssetGeometryFuncPtr ConnectAssetGeometry;

	typedef HAPI_Result (*DisconnectAssetGeometryFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, int input_idx);
	static DisconnectAssetGeometryFuncPtr DisconnectAssetGeometry;

	typedef HAPI_Result (*GetMaterialIdsOnFacesFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_Bool * are_all_the_same, HAPI_MaterialId * material_ids_array, int start, int length);
	static GetMaterialIdsOnFacesFuncPtr GetMaterialIdsOnFaces;

	typedef HAPI_Result (*GetMaterialInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_MaterialInfo * material_info);
	static GetMaterialInfoFuncPtr GetMaterialInfo;

	typedef HAPI_Result (*RenderTextureToImageFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_ParmId parm_id);
	static RenderTextureToImageFuncPtr RenderTextureToImage;

	typedef HAPI_Result (*GetSupportedImageFileFormatCountFuncPtr)(const HAPI_Session * session, int * file_format_count);
	static GetSupportedImageFileFormatCountFuncPtr GetSupportedImageFileFormatCount;

	typedef HAPI_Result (*GetSupportedImageFileFormatsFuncPtr)(const HAPI_Session * session, HAPI_ImageFileFormat * formats_array, int file_format_count);
	static GetSupportedImageFileFormatsFuncPtr GetSupportedImageFileFormats;

	typedef HAPI_Result (*GetImageInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_ImageInfo * image_info);
	static GetImageInfoFuncPtr GetImageInfo;

	typedef HAPI_Result (*SetImageInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, const HAPI_ImageInfo * image_info);
	static SetImageInfoFuncPtr SetImageInfo;

	typedef HAPI_Result (*GetImagePlaneCountFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, int * image_plane_count);
	static GetImagePlaneCountFuncPtr GetImagePlaneCount;

	typedef HAPI_Result (*GetImagePlanesFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_StringHandle * image_planes_array, int image_plane_count);
	static GetImagePlanesFuncPtr GetImagePlanes;

	typedef HAPI_Result (*ExtractImageToFileFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, const char * image_file_format_name, const char * image_planes, const char * destination_folder_path, const char * destination_file_name, int * destination_file_path);
	static ExtractImageToFileFuncPtr ExtractImageToFile;

	typedef HAPI_Result (*ExtractImageToMemoryFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, const char * image_file_format_name, const char * image_planes, int * buffer_size);
	static ExtractImageToMemoryFuncPtr ExtractImageToMemory;

	typedef HAPI_Result (*GetImageMemoryBufferFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_MaterialId material_id, char * buffer, int length);
	static GetImageMemoryBufferFuncPtr GetImageMemoryBuffer;

	typedef HAPI_Result (*SetAnimCurveFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_ParmId parm_id, int parm_index, const HAPI_Keyframe * curve_keyframes_array, int keyframe_count);
	static SetAnimCurveFuncPtr SetAnimCurve;

	typedef HAPI_Result (*SetTransformAnimCurveFuncPtr)(const HAPI_Session * session, HAPI_NodeId node_id, HAPI_TransformComponent trans_comp, const HAPI_Keyframe * curve_keyframes_array, int keyframe_count);
	static SetTransformAnimCurveFuncPtr SetTransformAnimCurve;

	typedef HAPI_Result (*ResetSimulationFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id);
	static ResetSimulationFuncPtr ResetSimulation;

	typedef HAPI_Result (*GetVolumeInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeInfo * volume_info);
	static GetVolumeInfoFuncPtr GetVolumeInfo;

	typedef HAPI_Result (*GetFirstVolumeTileFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeTileInfo * tile);
	static GetFirstVolumeTileFuncPtr GetFirstVolumeTile;

	typedef HAPI_Result (*GetNextVolumeTileFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeTileInfo * tile);
	static GetNextVolumeTileFuncPtr GetNextVolumeTile;

	typedef HAPI_Result (*GetVolumeVoxelFloatDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int x_index, int y_index, int z_index, float * values_array, int value_count);
	static GetVolumeVoxelFloatDataFuncPtr GetVolumeVoxelFloatData;

	typedef HAPI_Result (*GetVolumeTileFloatDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, float fill_value, const HAPI_VolumeTileInfo * tile, float * values_array, int length);
	static GetVolumeTileFloatDataFuncPtr GetVolumeTileFloatData;

	typedef HAPI_Result (*GetVolumeVoxelIntDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int x_index, int y_index, int z_index, int * values_array, int value_count);
	static GetVolumeVoxelIntDataFuncPtr GetVolumeVoxelIntData;

	typedef HAPI_Result (*GetVolumeTileIntDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int fill_value, const HAPI_VolumeTileInfo * tile, int * values_array, int length);
	static GetVolumeTileIntDataFuncPtr GetVolumeTileIntData;

	typedef HAPI_Result (*SetVolumeInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_VolumeInfo * volume_info);
	static SetVolumeInfoFuncPtr SetVolumeInfo;

	typedef HAPI_Result (*SetVolumeTileFloatDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_VolumeTileInfo * tile, const float * values_array, int length);
	static SetVolumeTileFloatDataFuncPtr SetVolumeTileFloatData;

	typedef HAPI_Result (*SetVolumeTileIntDataFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_VolumeTileInfo * tile, const int * values_array, int length);
	static SetVolumeTileIntDataFuncPtr SetVolumeTileIntData;

	typedef HAPI_Result (*GetCurveInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_CurveInfo * info);
	static GetCurveInfoFuncPtr GetCurveInfo;

	typedef HAPI_Result (*GetCurveCountsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * counts_array, int start, int length);
	static GetCurveCountsFuncPtr GetCurveCounts;

	typedef HAPI_Result (*GetCurveOrdersFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * orders_array, int start, int length);
	static GetCurveOrdersFuncPtr GetCurveOrders;

	typedef HAPI_Result (*GetCurveKnotsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, float * knots_array, int start, int length);
	static GetCurveKnotsFuncPtr GetCurveKnots;

	typedef HAPI_Result (*SetCurveInfoFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const HAPI_CurveInfo * info);
	static SetCurveInfoFuncPtr SetCurveInfo;

	typedef HAPI_Result (*SetCurveCountsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const int * counts_array, int start, int length);
	static SetCurveCountsFuncPtr SetCurveCounts;

	typedef HAPI_Result (*SetCurveOrdersFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const int * orders_array, int start, int length);
	static SetCurveOrdersFuncPtr SetCurveOrders;

	typedef HAPI_Result (*SetCurveKnotsFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const float * knots_array, int start, int length);
	static SetCurveKnotsFuncPtr SetCurveKnots;

	typedef HAPI_Result (*GetBoxInfoFuncPtr)(const HAPI_Session * session, HAPI_NodeId geo_node_id, HAPI_PartId part_id, HAPI_BoxInfo * box_info);
	static GetBoxInfoFuncPtr GetBoxInfo;

	typedef HAPI_Result (*GetSphereInfoFuncPtr)(const HAPI_Session * session, HAPI_NodeId geo_node_id, HAPI_PartId part_id, HAPI_SphereInfo * sphere_info);
	static GetSphereInfoFuncPtr GetSphereInfo;

	typedef HAPI_Result (*SaveGeoToFileFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * file_name);
	static SaveGeoToFileFuncPtr SaveGeoToFile;

	typedef HAPI_Result (*LoadGeoFromFileFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * file_name);
	static LoadGeoFromFileFuncPtr LoadGeoFromFile;

	typedef HAPI_Result (*GetGeoSizeFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * format, int * size);
	static GetGeoSizeFuncPtr GetGeoSize;

	typedef HAPI_Result (*SaveGeoToMemoryFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, char * buffer, int length);
	static SaveGeoToMemoryFuncPtr SaveGeoToMemory;

	typedef HAPI_Result (*LoadGeoFromMemoryFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * format, const char * buffer, int length);
	static LoadGeoFromMemoryFuncPtr LoadGeoFromMemory;

	typedef HAPI_Result (*GetMaterialOnPartFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_MaterialInfo * material_info);
	static GetMaterialOnPartFuncPtr GetMaterialOnPart;

	typedef HAPI_Result (*GetMaterialOnGroupFuncPtr)(const HAPI_Session * session, HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * group_name, HAPI_MaterialInfo * material_info);
	static GetMaterialOnGroupFuncPtr GetMaterialOnGroup;

};
