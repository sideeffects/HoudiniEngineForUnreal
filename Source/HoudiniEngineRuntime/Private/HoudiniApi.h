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

	typedef HAPI_Result (*IsInitializedFuncPtr)();
	static IsInitializedFuncPtr IsInitialized;

	typedef HAPI_Result (*InitializeFuncPtr)(const char * otl_search_path, const char * dso_search_path, const HAPI_CookOptions * cook_options, HAPI_Bool use_cooking_thread, int cooking_thread_stack_size);
	static InitializeFuncPtr Initialize;

	typedef HAPI_Result (*CleanupFuncPtr)();
	static CleanupFuncPtr Cleanup;

	typedef HAPI_Result (*GetEnvIntFuncPtr)(HAPI_EnvIntType int_type, int * value);
	static GetEnvIntFuncPtr GetEnvInt;

	typedef HAPI_Result (*GetStatusFuncPtr)(HAPI_StatusType status_type, int * status);
	static GetStatusFuncPtr GetStatus;

	typedef HAPI_Result (*GetStatusStringBufLengthFuncPtr)(HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity, int * buffer_size);
	static GetStatusStringBufLengthFuncPtr GetStatusStringBufLength;

	typedef HAPI_Result (*GetStatusStringFuncPtr)(HAPI_StatusType status_type, char * buffer, int buffer_length);
	static GetStatusStringFuncPtr GetStatusString;

	typedef HAPI_Result (*GetCookingTotalCountFuncPtr)(int * count);
	static GetCookingTotalCountFuncPtr GetCookingTotalCount;

	typedef HAPI_Result (*GetCookingCurrentCountFuncPtr)(int * count);
	static GetCookingCurrentCountFuncPtr GetCookingCurrentCount;

	typedef HAPI_Result (*ConvertTransformFuncPtr)(HAPI_TransformEuler * transform_in_out, HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order);
	static ConvertTransformFuncPtr ConvertTransform;

	typedef HAPI_Result (*ConvertMatrixToQuatFuncPtr)(const float * mat, HAPI_RSTOrder rst_order, HAPI_Transform * transform_out);
	static ConvertMatrixToQuatFuncPtr ConvertMatrixToQuat;

	typedef HAPI_Result (*ConvertMatrixToEulerFuncPtr)(const float * mat, HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order, HAPI_TransformEuler * transform_out);
	static ConvertMatrixToEulerFuncPtr ConvertMatrixToEuler;

	typedef HAPI_Result (*ConvertTransformQuatToMatrixFuncPtr)(const HAPI_Transform * transform, float * matrix);
	static ConvertTransformQuatToMatrixFuncPtr ConvertTransformQuatToMatrix;

	typedef HAPI_Result (*ConvertTransformEulerToMatrixFuncPtr)(const HAPI_TransformEuler * transform, float * matrix);
	static ConvertTransformEulerToMatrixFuncPtr ConvertTransformEulerToMatrix;

	typedef HAPI_Result (*PythonThreadInterpreterLockFuncPtr)(HAPI_Bool locked);
	static PythonThreadInterpreterLockFuncPtr PythonThreadInterpreterLock;

	typedef HAPI_Result (*GetStringBufLengthFuncPtr)(HAPI_StringHandle string_handle, int * buffer_length);
	static GetStringBufLengthFuncPtr GetStringBufLength;

	typedef HAPI_Result (*GetStringFuncPtr)(HAPI_StringHandle string_handle, char * string_value, int buffer_length);
	static GetStringFuncPtr GetString;

	typedef HAPI_Result (*GetTimeFuncPtr)(float * time);
	static GetTimeFuncPtr GetTime;

	typedef HAPI_Result (*SetTimeFuncPtr)(float time);
	static SetTimeFuncPtr SetTime;

	typedef HAPI_Result (*GetTimelineOptionsFuncPtr)(HAPI_TimelineOptions * timeline_options);
	static GetTimelineOptionsFuncPtr GetTimelineOptions;

	typedef HAPI_Result (*SetTimelineOptionsFuncPtr)(const HAPI_TimelineOptions * timeline_options);
	static SetTimelineOptionsFuncPtr SetTimelineOptions;

	typedef HAPI_Result (*IsAssetValidFuncPtr)(HAPI_AssetId asset_id, int asset_validation_id, int * answer);
	static IsAssetValidFuncPtr IsAssetValid;

	typedef HAPI_Result (*LoadAssetLibraryFromFileFuncPtr)(const char * file_path, HAPI_Bool allow_overwrite, HAPI_AssetLibraryId* library_id);
	static LoadAssetLibraryFromFileFuncPtr LoadAssetLibraryFromFile;

	typedef HAPI_Result (*LoadAssetLibraryFromMemoryFuncPtr)(const char * library_buffer, int library_buffer_size, HAPI_Bool allow_overwrite, HAPI_AssetLibraryId * library_id);
	static LoadAssetLibraryFromMemoryFuncPtr LoadAssetLibraryFromMemory;

	typedef HAPI_Result (*GetAvailableAssetCountFuncPtr)(HAPI_AssetLibraryId library_id, int * asset_count);
	static GetAvailableAssetCountFuncPtr GetAvailableAssetCount;

	typedef HAPI_Result (*GetAvailableAssetsFuncPtr)(HAPI_AssetLibraryId library_id, HAPI_StringHandle * asset_names, int asset_count);
	static GetAvailableAssetsFuncPtr GetAvailableAssets;

	typedef HAPI_Result (*InstantiateAssetFuncPtr)(const char * asset_name, HAPI_Bool cook_on_load, HAPI_AssetId * asset_id);
	static InstantiateAssetFuncPtr InstantiateAsset;

	typedef HAPI_Result (*CreateCurveFuncPtr)(HAPI_AssetId * asset_id);
	static CreateCurveFuncPtr CreateCurve;

	typedef HAPI_Result (*CreateInputAssetFuncPtr)(HAPI_AssetId * asset_id, const char * name);
	static CreateInputAssetFuncPtr CreateInputAsset;

	typedef HAPI_Result (*DestroyAssetFuncPtr)(HAPI_AssetId asset_id);
	static DestroyAssetFuncPtr DestroyAsset;

	typedef HAPI_Result (*GetAssetInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_AssetInfo * asset_info);
	static GetAssetInfoFuncPtr GetAssetInfo;

	typedef HAPI_Result (*CookAssetFuncPtr)(HAPI_AssetId asset_id, const HAPI_CookOptions * cook_options);
	static CookAssetFuncPtr CookAsset;

	typedef HAPI_Result (*InterruptFuncPtr)();
	static InterruptFuncPtr Interrupt;

	typedef HAPI_Result (*GetAssetTransformFuncPtr)(HAPI_AssetId asset_id, HAPI_RSTOrder rst_order, HAPI_XYZOrder rot_order, HAPI_TransformEuler * transform);
	static GetAssetTransformFuncPtr GetAssetTransform;

	typedef HAPI_Result (*SetAssetTransformFuncPtr)(HAPI_AssetId asset_id, HAPI_TransformEuler * transform);
	static SetAssetTransformFuncPtr SetAssetTransform;

	typedef HAPI_Result (*GetInputNameFuncPtr)(HAPI_AssetId asset_id, int input_idx, int input_type, HAPI_StringHandle * name);
	static GetInputNameFuncPtr GetInputName;

	typedef HAPI_Result (*LoadHIPFileFuncPtr)(const char * file_name, HAPI_Bool cook_on_load);
	static LoadHIPFileFuncPtr LoadHIPFile;

	typedef HAPI_Result (*CheckForNewAssetsFuncPtr)(int * new_asset_count);
	static CheckForNewAssetsFuncPtr CheckForNewAssets;

	typedef HAPI_Result (*GetNewAssetIdsFuncPtr)(HAPI_AssetId * asset_ids);
	static GetNewAssetIdsFuncPtr GetNewAssetIds;

	typedef HAPI_Result (*SaveHIPFileFuncPtr)(const char * file_path);
	static SaveHIPFileFuncPtr SaveHIPFile;

	typedef HAPI_Result (*GetNodeInfoFuncPtr)(HAPI_NodeId node_id, HAPI_NodeInfo * node_info);
	static GetNodeInfoFuncPtr GetNodeInfo;

	typedef HAPI_Result (*GetGlobalNodesFuncPtr)(HAPI_GlobalNodes * global_nodes);
	static GetGlobalNodesFuncPtr GetGlobalNodes;

	typedef HAPI_Result (*GetParametersFuncPtr)(HAPI_NodeId node_id, HAPI_ParmInfo * parm_infos, int start, int length);
	static GetParametersFuncPtr GetParameters;

	typedef HAPI_Result (*GetParmInfoFuncPtr)(HAPI_NodeId node_id, HAPI_ParmId parm_id, HAPI_ParmInfo * parm_info);
	static GetParmInfoFuncPtr GetParmInfo;

	typedef HAPI_Result (*GetParmIdFromNameFuncPtr)(HAPI_NodeId node_id, const char * parm_name, HAPI_ParmId * parm_id);
	static GetParmIdFromNameFuncPtr GetParmIdFromName;

	typedef HAPI_Result (*GetParmInfoFromNameFuncPtr)(HAPI_NodeId node_id, const char * parm_name, HAPI_ParmInfo * parm_info);
	static GetParmInfoFromNameFuncPtr GetParmInfoFromName;

	typedef HAPI_Result (*GetParmIntValueFuncPtr)(HAPI_NodeId node_id, const char * parm_name, int index, int * value);
	static GetParmIntValueFuncPtr GetParmIntValue;

	typedef HAPI_Result (*GetParmIntValuesFuncPtr)(HAPI_NodeId node_id, int * values, int start, int length);
	static GetParmIntValuesFuncPtr GetParmIntValues;

	typedef HAPI_Result (*GetParmFloatValueFuncPtr)(HAPI_NodeId node_id, const char * parm_name, int index, float * value);
	static GetParmFloatValueFuncPtr GetParmFloatValue;

	typedef HAPI_Result (*GetParmFloatValuesFuncPtr)(HAPI_NodeId node_id, float * values, int start, int length);
	static GetParmFloatValuesFuncPtr GetParmFloatValues;

	typedef HAPI_Result (*GetParmStringValueFuncPtr)(HAPI_NodeId node_id, const char * parm_name, int index, HAPI_Bool evaluate, HAPI_StringHandle * value);
	static GetParmStringValueFuncPtr GetParmStringValue;

	typedef HAPI_Result (*GetParmStringValuesFuncPtr)(HAPI_NodeId node_id, HAPI_Bool evaluate, HAPI_StringHandle * values, int start, int length);
	static GetParmStringValuesFuncPtr GetParmStringValues;

	typedef HAPI_Result (*GetParmChoiceListsFuncPtr)(HAPI_NodeId node_id, HAPI_ParmChoiceInfo * parm_choices, int start, int length);
	static GetParmChoiceListsFuncPtr GetParmChoiceLists;

	typedef HAPI_Result (*SetParmIntValueFuncPtr)(HAPI_NodeId node_id, const char * parm_name, int index, int value);
	static SetParmIntValueFuncPtr SetParmIntValue;

	typedef HAPI_Result (*SetParmIntValuesFuncPtr)(HAPI_NodeId node_id, const int * values, int start, int length);
	static SetParmIntValuesFuncPtr SetParmIntValues;

	typedef HAPI_Result (*SetParmFloatValueFuncPtr)(HAPI_NodeId node_id, const char * parm_name, int index, float value);
	static SetParmFloatValueFuncPtr SetParmFloatValue;

	typedef HAPI_Result (*SetParmFloatValuesFuncPtr)(HAPI_NodeId node_id, const float * values, int start, int length);
	static SetParmFloatValuesFuncPtr SetParmFloatValues;

	typedef HAPI_Result (*SetParmStringValueFuncPtr)(HAPI_NodeId node_id, const char * value, HAPI_ParmId parm_id, int index);
	static SetParmStringValueFuncPtr SetParmStringValue;

	typedef HAPI_Result (*InsertMultiparmInstanceFuncPtr)(HAPI_NodeId node_id, HAPI_ParmId parm_id, int instance_position);
	static InsertMultiparmInstanceFuncPtr InsertMultiparmInstance;

	typedef HAPI_Result (*RemoveMultiparmInstanceFuncPtr)(HAPI_NodeId node_id, HAPI_ParmId parm_id, int instance_position);
	static RemoveMultiparmInstanceFuncPtr RemoveMultiparmInstance;

	typedef HAPI_Result (*GetHandleInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_HandleInfo * handle_infos, int start, int length);
	static GetHandleInfoFuncPtr GetHandleInfo;

	typedef HAPI_Result (*GetHandleBindingInfoFuncPtr)(HAPI_AssetId asset_id, int handle_index, HAPI_HandleBindingInfo * handle_infos, int start, int length);
	static GetHandleBindingInfoFuncPtr GetHandleBindingInfo;

	typedef HAPI_Result (*GetPresetBufLengthFuncPtr)(HAPI_NodeId node_id, HAPI_PresetType preset_type, const char * preset_name, int * buffer_length);
	static GetPresetBufLengthFuncPtr GetPresetBufLength;

	typedef HAPI_Result (*GetPresetFuncPtr)(HAPI_NodeId node_id, char * buffer, int buffer_length);
	static GetPresetFuncPtr GetPreset;

	typedef HAPI_Result (*SetPresetFuncPtr)(HAPI_NodeId node_id, HAPI_PresetType preset_type, const char * preset_name, const char * buffer, int buffer_length);
	static SetPresetFuncPtr SetPreset;

	typedef HAPI_Result (*GetObjectsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectInfo * object_infos, int start, int length);
	static GetObjectsFuncPtr GetObjects;

	typedef HAPI_Result (*GetObjectTransformsFuncPtr)(HAPI_AssetId asset_id, HAPI_RSTOrder rst_order, HAPI_Transform * transforms, int start, int length);
	static GetObjectTransformsFuncPtr GetObjectTransforms;

	typedef HAPI_Result (*GetInstanceTransformsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_RSTOrder rst_order, HAPI_Transform * transforms, int start, int length);
	static GetInstanceTransformsFuncPtr GetInstanceTransforms;

	typedef HAPI_Result (*SetObjectTransformFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, const HAPI_TransformEuler * transform);
	static SetObjectTransformFuncPtr SetObjectTransform;

	typedef HAPI_Result (*GetGeoInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GeoInfo * geo_info);
	static GetGeoInfoFuncPtr GetGeoInfo;

	typedef HAPI_Result (*GetPartInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_PartInfo * part_info);
	static GetPartInfoFuncPtr GetPartInfo;

	typedef HAPI_Result (*GetFaceCountsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * face_counts, int start, int length);
	static GetFaceCountsFuncPtr GetFaceCounts;

	typedef HAPI_Result (*GetVertexListFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * vertex_list, int start, int length);
	static GetVertexListFuncPtr GetVertexList;

	typedef HAPI_Result (*GetAttributeInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeOwner owner, HAPI_AttributeInfo * attr_info);
	static GetAttributeInfoFuncPtr GetAttributeInfo;

	typedef HAPI_Result (*GetAttributeNamesFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_AttributeOwner owner, HAPI_StringHandle * attribute_names, int count);
	static GetAttributeNamesFuncPtr GetAttributeNames;

	typedef HAPI_Result (*GetAttributeIntDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeInfo * attr_info, int * data, int start, int length);
	static GetAttributeIntDataFuncPtr GetAttributeIntData;

	typedef HAPI_Result (*GetAttributeFloatDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeInfo * attr_info, float * data, int start, int length);
	static GetAttributeFloatDataFuncPtr GetAttributeFloatData;

	typedef HAPI_Result (*GetAttributeStringDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const char * name, HAPI_AttributeInfo * attr_info, int * data, int start, int length);
	static GetAttributeStringDataFuncPtr GetAttributeStringData;

	typedef HAPI_Result (*GetGroupNamesFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GroupType group_type, HAPI_StringHandle * group_names, int group_count);
	static GetGroupNamesFuncPtr GetGroupNames;

	typedef HAPI_Result (*GetGroupMembershipFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_GroupType group_type, const char * group_name, int * membership, int start, int length);
	static GetGroupMembershipFuncPtr GetGroupMembership;

	typedef HAPI_Result (*SetGeoInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GeoInfo * geo_info);
	static SetGeoInfoFuncPtr SetGeoInfo;

	typedef HAPI_Result (*SetPartInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_PartInfo * part_info);
	static SetPartInfoFuncPtr SetPartInfo;

	typedef HAPI_Result (*SetFaceCountsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const int * face_counts, int start, int length);
	static SetFaceCountsFuncPtr SetFaceCounts;

	typedef HAPI_Result (*SetVertexListFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const int * vertex_list, int start, int length);
	static SetVertexListFuncPtr SetVertexList;

	typedef HAPI_Result (*AddAttributeFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo * attr_info);
	static AddAttributeFuncPtr AddAttribute;

	typedef HAPI_Result (*SetAttributeIntDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo * attr_info, const int * data, int start, int length);
	static SetAttributeIntDataFuncPtr SetAttributeIntData;

	typedef HAPI_Result (*SetAttributeFloatDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo * attr_info, const float * data, int start, int length);
	static SetAttributeFloatDataFuncPtr SetAttributeFloatData;

	typedef HAPI_Result (*SetAttributeStringDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * name, const HAPI_AttributeInfo *attr_info, const char ** data, int start, int length);
	static SetAttributeStringDataFuncPtr SetAttributeStringData;

	typedef HAPI_Result (*AddGroupFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GroupType group_type, const char * group_name);
	static AddGroupFuncPtr AddGroup;

	typedef HAPI_Result (*SetGroupMembershipFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_GroupType group_type, const char * group_name, int * membership, int start, int length);
	static SetGroupMembershipFuncPtr SetGroupMembership;

	typedef HAPI_Result (*CommitGeoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id);
	static CommitGeoFuncPtr CommitGeo;

	typedef HAPI_Result (*RevertGeoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id);
	static RevertGeoFuncPtr RevertGeo;

	typedef HAPI_Result (*ConnectAssetTransformFuncPtr)(HAPI_AssetId asset_id_from, HAPI_AssetId asset_id_to, int input_idx);
	static ConnectAssetTransformFuncPtr ConnectAssetTransform;

	typedef HAPI_Result (*DisconnectAssetTransformFuncPtr)(HAPI_AssetId asset_id, int input_idx);
	static DisconnectAssetTransformFuncPtr DisconnectAssetTransform;

	typedef HAPI_Result (*ConnectAssetGeometryFuncPtr)(HAPI_AssetId asset_id_from, HAPI_ObjectId object_id_from, HAPI_AssetId asset_id_to, int input_idx);
	static ConnectAssetGeometryFuncPtr ConnectAssetGeometry;

	typedef HAPI_Result (*DisconnectAssetGeometryFuncPtr)(HAPI_AssetId asset_id, int input_idx);
	static DisconnectAssetGeometryFuncPtr DisconnectAssetGeometry;

	typedef HAPI_Result (*GetMaterialIdsOnFacesFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_Bool * are_all_the_same, HAPI_MaterialId * material_ids, int start, int length);
	static GetMaterialIdsOnFacesFuncPtr GetMaterialIdsOnFaces;

	typedef HAPI_Result (*GetMaterialInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_MaterialInfo * material_info);
	static GetMaterialInfoFuncPtr GetMaterialInfo;

	typedef HAPI_Result (*RenderMaterialToImageFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_ShaderType shader_type);
	static RenderMaterialToImageFuncPtr RenderMaterialToImage;

	typedef HAPI_Result (*RenderTextureToImageFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_ParmId parm_id);
	static RenderTextureToImageFuncPtr RenderTextureToImage;

	typedef HAPI_Result (*GetSupportedImageFileFormatCountFuncPtr)(int * file_format_count);
	static GetSupportedImageFileFormatCountFuncPtr GetSupportedImageFileFormatCount;

	typedef HAPI_Result (*GetSupportedImageFileFormatsFuncPtr)(HAPI_ImageFileFormat *formats, int file_format_count);
	static GetSupportedImageFileFormatsFuncPtr GetSupportedImageFileFormats;

	typedef HAPI_Result (*GetImageInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_ImageInfo * image_info);
	static GetImageInfoFuncPtr GetImageInfo;

	typedef HAPI_Result (*SetImageInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, const HAPI_ImageInfo * image_info);
	static SetImageInfoFuncPtr SetImageInfo;

	typedef HAPI_Result (*GetImagePlaneCountFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, int * image_plane_count);
	static GetImagePlaneCountFuncPtr GetImagePlaneCount;

	typedef HAPI_Result (*GetImagePlanesFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, HAPI_StringHandle * image_planes, int image_plane_count);
	static GetImagePlanesFuncPtr GetImagePlanes;

	typedef HAPI_Result (*ExtractImageToFileFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, const char * image_file_format_name, const char * image_planes, const char * destination_folder_path, const char * destination_file_name, int * destination_file_path);
	static ExtractImageToFileFuncPtr ExtractImageToFile;

	typedef HAPI_Result (*ExtractImageToMemoryFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, const char * image_file_format_name, const char * image_planes, int * buffer_size);
	static ExtractImageToMemoryFuncPtr ExtractImageToMemory;

	typedef HAPI_Result (*GetImageMemoryBufferFuncPtr)(HAPI_AssetId asset_id, HAPI_MaterialId material_id, char * buffer, int buffer_size);
	static GetImageMemoryBufferFuncPtr GetImageMemoryBuffer;

	typedef HAPI_Result (*SetAnimCurveFuncPtr)(HAPI_NodeId node_id, HAPI_ParmId parm_id, int parm_index, const HAPI_Keyframe * curve_keyframes, int keyframe_count);
	static SetAnimCurveFuncPtr SetAnimCurve;

	typedef HAPI_Result (*SetTransformAnimCurveFuncPtr)(HAPI_NodeId node_id, HAPI_TransformComponent trans_comp, const HAPI_Keyframe *curve_keyframes, int keyframe_count);
	static SetTransformAnimCurveFuncPtr SetTransformAnimCurve;

	typedef HAPI_Result (*ResetSimulationFuncPtr)(HAPI_AssetId asset_id);
	static ResetSimulationFuncPtr ResetSimulation;

	typedef HAPI_Result (*GetVolumeInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeInfo * volume_info);
	static GetVolumeInfoFuncPtr GetVolumeInfo;

	typedef HAPI_Result (*GetFirstVolumeTileFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeTileInfo * tile);
	static GetFirstVolumeTileFuncPtr GetFirstVolumeTile;

	typedef HAPI_Result (*GetNextVolumeTileFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeTileInfo * tile);
	static GetNextVolumeTileFuncPtr GetNextVolumeTile;

	typedef HAPI_Result (*GetVolumeTileFloatDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeTileInfo * tile, float * values);
	static GetVolumeTileFloatDataFuncPtr GetVolumeTileFloatData;

	typedef HAPI_Result (*GetVolumeTileIntDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_VolumeTileInfo * tile, int * values);
	static GetVolumeTileIntDataFuncPtr GetVolumeTileIntData;

	typedef HAPI_Result (*SetVolumeInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_VolumeInfo * volume_info);
	static SetVolumeInfoFuncPtr SetVolumeInfo;

	typedef HAPI_Result (*SetVolumeTileFloatDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_VolumeTileInfo * tile, const float * values);
	static SetVolumeTileFloatDataFuncPtr SetVolumeTileFloatData;

	typedef HAPI_Result (*SetVolumeTileIntDataFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const HAPI_VolumeTileInfo * tile, const int * values);
	static SetVolumeTileIntDataFuncPtr SetVolumeTileIntData;

	typedef HAPI_Result (*GetCurveInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_CurveInfo * info);
	static GetCurveInfoFuncPtr GetCurveInfo;

	typedef HAPI_Result (*GetCurveCountsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * counts, int start, int length);
	static GetCurveCountsFuncPtr GetCurveCounts;

	typedef HAPI_Result (*GetCurveOrdersFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, int * orders, int start, int length);
	static GetCurveOrdersFuncPtr GetCurveOrders;

	typedef HAPI_Result (*GetCurveKnotsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, float * knots, int start, int length);
	static GetCurveKnotsFuncPtr GetCurveKnots;

	typedef HAPI_Result (*SetCurveInfoFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const HAPI_CurveInfo * info);
	static SetCurveInfoFuncPtr SetCurveInfo;

	typedef HAPI_Result (*SetCurveCountsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const int * counts, int start, int length);
	static SetCurveCountsFuncPtr SetCurveCounts;

	typedef HAPI_Result (*SetCurveOrdersFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const int * orders, int start, int length);
	static SetCurveOrdersFuncPtr SetCurveOrders;

	typedef HAPI_Result (*SetCurveKnotsFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, const float * knots, int start, int length);
	static SetCurveKnotsFuncPtr SetCurveKnots;

	typedef HAPI_Result (*SaveGeoToFileFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * file_name);
	static SaveGeoToFileFuncPtr SaveGeoToFile;

	typedef HAPI_Result (*LoadGeoFromFileFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * file_name);
	static LoadGeoFromFileFuncPtr LoadGeoFromFile;

	typedef HAPI_Result (*GetGeoSizeFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * format, int * size);
	static GetGeoSizeFuncPtr GetGeoSize;

	typedef HAPI_Result (*SaveGeoToMemoryFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, char * buffer, int size);
	static SaveGeoToMemoryFuncPtr SaveGeoToMemory;

	typedef HAPI_Result (*LoadGeoFromMemoryFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * format, const char * buffer, int size);
	static LoadGeoFromMemoryFuncPtr LoadGeoFromMemory;

	typedef HAPI_Result (*GetMaterialOnPartFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, HAPI_PartId part_id, HAPI_MaterialInfo * material_info);
	static GetMaterialOnPartFuncPtr GetMaterialOnPart;

	typedef HAPI_Result (*GetMaterialOnGroupFuncPtr)(HAPI_AssetId asset_id, HAPI_ObjectId object_id, HAPI_GeoId geo_id, const char * group_name, HAPI_MaterialInfo * material_info);
	static GetMaterialOnGroupFuncPtr GetMaterialOnGroup;

};
