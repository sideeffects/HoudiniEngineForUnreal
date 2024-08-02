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

#include "UnrealDataTableTranslator.h"

#include "UObject/TextProperty.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniInputObject.h"

#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "Engine/DataTable.h"

namespace
{
	template<typename T, typename P>
	TArray<T>
		PopulateTArray(TMap<FName, uint8*>::TConstIterator It,
			uint32 NumRows,
			uint32 NumComponents,
			uint32 Offset,
			uint32 ComponentSize,
			const TArray<uint32>& Order)
	{
		TArray<T> Values;
		Values.Reserve(NumRows * NumComponents);
		for (; It; ++It)
		{
			const uint8* Data = It.Value();
			for (uint32 Idx = 0; Idx < NumComponents; ++Idx)
			{
				Values.Add(P::GetPropertyValue(Data + Offset + Order[Idx] * ComponentSize));
			}

		}
		return Values;
	}

	template<typename T, typename P>
	TArray<T>
		PopulateTArray(TMap<FName, uint8*>::TConstIterator It,
			uint32 NumRows,
			uint32 NumComponents,
			uint32 Offset,
			uint32 ComponentSize)
	{
		TArray<uint32> Order;
		Order.Reserve(NumComponents);
		for (uint32 Idx = 0; Idx < NumComponents; ++Idx)
		{
			Order.Add(Idx);
		}
		return PopulateTArray<T, P>(It, NumRows, NumComponents, Offset, ComponentSize, Order);
	}

	TArray<int8>
		PopulateBoolArray(TMap<FName, uint8*>::TConstIterator It,
			const FBoolProperty& Prop,
			uint32 Count,
			uint32 Offset)
	{
		TArray<int8> Values;
		Values.Reserve(Count);
		for (; It; ++It)
		{
			const uint8* Data = It.Value();
			Values.Add(Prop.GetPropertyValue(Data + Offset));
		}
		return Values;
	}
};

bool FUnrealDataTableTranslator::CreateInputNodeForDataTable(
	UDataTable* DataTable,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	const bool& bInputNodesCanBeDeleted)
{
	FString FinalInputNodeName = InputNodeName;

	FUnrealObjectInputIdentifier Identifier;
	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;

	{
		const FUnrealObjectInputOptions Options;
		Identifier = FUnrealObjectInputIdentifier(DataTable, Options, true);

		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			{
				if (!bInputNodesCanBeDeleted)
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// Set InputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that InputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, InputNodeId))
				InputNodeId = -1;
		}
		else
		{
			InputNodeId = -1;
		}
	}

	// Create the input node
	const UScriptStruct* ScriptStruct = DataTable->GetRowStruct();
	if (!IsValid(ScriptStruct))
		return true;

	TArray<FName> RowNames = DataTable->GetRowNames();
	TArray<FString> ColTitles = DataTable->GetUniqueColumnTitles();
	TArray<FString> NiceNames = DataTable->GetColumnTitles();

	int32 NumRows = RowNames.Num();
	int32 NumAttributes = ColTitles.Num();
	if (NumRows <= 0 || NumAttributes <= 0)
		return true;

	HAPI_NodeId NewNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(FinalInputNodeName, NewNodeId, ParentNodeId), false);

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;

	HAPI_NodeId PreviousInputNodeId = InputNodeId;
	InputNodeId = NewNodeId;
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);

	if (PreviousInputNodeId >= 0)
	{
		HAPI_NodeId PreviousInputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputNodeId) != HAPI_RESULT_SUCCESS)
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);

		if (FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PreviousInputObjectNodeId) != HAPI_RESULT_SUCCESS)
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input object node for %s."), *FinalInputNodeName);
	}

	// Create a part
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = NumAttributes;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = NumRows;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &Part), false);

	{
		// Create point attribute info for P.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

		// Set the point's position
		TArray<float> Positions;
		Positions.SetNum(NumRows * 3);
		for (int32 RowIdx = 0; RowIdx < NumRows; RowIdx++)
		{
			Positions[RowIdx * 3] = 0.0f;
			Positions[RowIdx * 3 + 1] = (float)RowIdx;
			Positions[RowIdx * 3 + 2] = 0.0f;
		}

		FHoudiniHapiAccessor Accessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, Positions), false);
	}

	{
		// Create point attribute info for the path.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_OBJECT_PATH, &AttributeInfoPoint), false);

		// Get the object path
		FString ObjectPathName = DataTable->GetPathName();

		// Set the point's path attribute
		FHoudiniHapiAccessor Accessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_OBJECT_PATH);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoPoint, ObjectPathName), false);
	}

	{
		// Create point attribute info for data table RowTable class name
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWSTRUCT, &AttributeInfoPoint), false);

		// Get the object path
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FString RowStructName = DataTable->GetRowStructPathName().ToString();
#else
		FString RowStructName = DataTable->RowStruct->GetPathName();
#endif

		// Set the point's path attribute
		FHoudiniHapiAccessor Accessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWSTRUCT);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoPoint, RowStructName), false);
	}

	{
		// Manually set the attribute for the unique RowName.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWNAME, &AttributeInfoPoint), false);

		TArray<FString> Names;
		Names.Reserve(NumRows);
		for (auto KV : DataTable->GetRowMap())
		{
			Names.Add(KV.Key.ToString());
		}

		FHoudiniHapiAccessor Accessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWNAME);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, Names), false);
	}

	// Now set the attributes values for each "point" of the data table
	uint32 ColIdx = 0;
	auto ColIter(ColTitles.CreateConstIterator());
	// Skip the name column.
	++ColIter;
	for (; ColIter; ++ColIter)
	{
		++ColIdx;
		FString ColName = *ColIter;
		FString NiceName = NiceNames[ColIdx];

		// Validate struct variable names
		NiceName = NiceName.Replace(TEXT(" "), TEXT("_"));
		NiceName = NiceName.Replace(TEXT(":"), TEXT("_"));

		FString CurAttrName = TEXT(HAPI_UNREAL_ATTRIB_DATA_TABLE_PREFIX) + FString::FromInt(ColIdx) + TEXT("_") + NiceName;
		if (FHoudiniEngineUtils::SanitizeHAPIVariableName(CurAttrName))
		{
			FString OldName = TEXT(HAPI_UNREAL_ATTRIB_DATA_TABLE_PREFIX) + FString::FromInt(ColIdx) + TEXT("_") + NiceName;
			HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid row name (%s) renamed to %s."), *OldName, *CurAttrName);
		}

		FProperty* ColumnProp = ScriptStruct->FindPropertyByName(FName(*ColName));
		if (!ColumnProp)
		{
			HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Could not find property %s in struct %s."), *ColName, *ScriptStruct->GetFName().ToString());
			continue;
		}
		// Create a point attribute info
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = NumRows;
		// Set tuple size depending on prop type.
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_POINT;
		// Set the storage type depending on prop type.
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
		AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_NONE;

		uint32 Offset = ColumnProp->GetOffset_ForInternal();
		TMap<FName, uint8*>::TConstIterator It = DataTable->GetRowMap().CreateConstIterator();

		// Identify the type of the Property
		bool bIsBoolProperty = ColumnProp->IsA<FBoolProperty>();
		bool bIsTextProperty = ColumnProp->IsA<FTextProperty>();

		// For numeric properties, we want to treat enum separately to send them as string and not ints
		FNumericProperty* NumProp = CastField<FNumericProperty>(ColumnProp);
		bool bIsNumericProperty = NumProp != nullptr ? !NumProp->IsEnum() : false;
		bool bIsEnumProperty = NumProp != nullptr ? NumProp->IsEnum() : false;

		// Struct props 
		FStructProperty* StructProp = CastField<FStructProperty>(ColumnProp);
		bool bIsStructProperty = StructProp != nullptr;

		// We need to get all values for that attribute
		if (bIsBoolProperty)
		{
			AttributeInfo.tupleSize = 1;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT8;
			TArray<int8> Col = PopulateBoolArray(It, *CastField<FBoolProperty>(ColumnProp), NumRows, Offset);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
				TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

			FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
		}
		// Text treated separately because the method used for string fallback
		// doesn't cleanly convert this
		else if (bIsTextProperty)
		{
			AttributeInfo.tupleSize = 1;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			TArray<FString> Col;
			Col.Reserve(NumRows);
			for (; It; ++It)
			{
				FText* TextValue = ColumnProp->ContainerPtrToValuePtr<FText>(It.Value());
				if (!TextValue)
				{
					HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid transform value for property %s."), *ColumnProp->GetName());
					continue;
				}
				Col.Add(TextValue->ToString());
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
				TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

			FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
		}
		else if (bIsNumericProperty && !bIsEnumProperty)
		{
			AttributeInfo.tupleSize = 1;
			if (NumProp->IsInteger())
			{
				if (NumProp->IsA<FByteProperty>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_UINT8;
					TArray<uint8> Col = PopulateTArray<uint8, FByteProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
				else if (NumProp->IsA<FInt16Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT16;
					TArray<int16> Col = PopulateTArray<int16, FInt16Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);

				}
				else if (NumProp->IsA<FInt8Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT8;
					TArray<int8> Col = PopulateTArray<int8, FInt8Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
				else if (NumProp->IsA<FIntProperty>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT;
					TArray<int32> Col = PopulateTArray<int32, FIntProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
				// There is no uint16 datatype in Houdini, convert to int32
				else if (NumProp->IsA<FUInt16Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT;
					TArray<int32> Col = PopulateTArray<int32, FUInt16Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);

				}
				// There is no uint32 datatype in Houdini, convert to int64
				else if (NumProp->IsA<FUInt32Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
					TArray<int64> Col = PopulateTArray<int64, FUInt32Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
				// There is no uint64 datatype in Houdini, since there is
				// no int128, just force a conversion to signed int64
				else if (NumProp->IsA<FUInt64Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
					TArray<int64> Col = PopulateTArray<int64, FUInt64Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
				// Default to signed int64.
				else
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
					TArray<int64> Col = PopulateTArray<int64, FInt64Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
			}
			else if (NumProp->IsFloatingPoint())
			{
				if (NumProp->IsA<FFloatProperty>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
					TArray<float> Col = PopulateTArray<float, FFloatProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
				// Default to double precision floating point.
				else
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT64;
					TArray<double> Col = PopulateTArray<double, FDoubleProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
				}
			}
			else
			{
				FString ext;
				FString typ = ColumnProp->GetCPPMacroType(ext);
				HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Unrecognized child of FNumericProperty: %s %s"), *typ, *ext);
				continue;
			}
		}
		else
		{
			if (bIsStructProperty)
			{
				FName StructName = StructProp->Struct->GetFName();

				int32 NumComponents = -1;
				HAPI_StorageType Storage = HAPI_STORAGETYPE_INVALID;
				TArray<uint32> Order;
				if (StructName == NAME_Vector4f || StructName == NAME_LinearColor)
				{
					NumComponents = 4;
					Storage = HAPI_STORAGETYPE_FLOAT;
				}
				else if (StructName == NAME_Vector4d || StructName == NAME_Vector4)
				{
					NumComponents = 4;
					Storage = HAPI_STORAGETYPE_FLOAT64;
				}
				else if (StructName == NAME_Vector3f)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT;
				}
				else if (StructName == NAME_Rotator3f)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT;
					// Roll, Pitch, Yaw
					Order = { 1, 0, 2 };
				}
				else if (StructName == NAME_Vector || StructName == NAME_Vector3d)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT64;
				}
				else if (StructName == NAME_Rotator || StructName == NAME_Rotator3d)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT64;
					// Roll, Pitch, Yaw
					Order = { 1, 0, 2 };
				}
				else if (StructName == NAME_Vector2f)
				{
					NumComponents = 2;
					Storage = HAPI_STORAGETYPE_FLOAT;
				}
				else if (StructName == NAME_Vector2d || StructName == NAME_Vector2D)
				{
					NumComponents = 2;
					Storage = HAPI_STORAGETYPE_FLOAT64;
				}
				else if (StructName == NAME_Color)
				{
					NumComponents = 4;
					Storage = HAPI_STORAGETYPE_UINT8;
					// RGBA
					Order = { 2, 1, 0, 3 };
				}
				else if (StructName == NAME_Transform || StructName == NAME_Transform3d)
				{
					FTransform3d* Transform;
					TArray<double> RotValues;
					TArray<double> ScaleValues;
					TArray<double> TransValues;
					RotValues.Reserve(4 * NumRows);
					ScaleValues.Reserve(3 * NumRows);
					TransValues.Reserve(3 * NumRows);
					FString RotName = CurAttrName + TEXT("_rotation");
					FString ScaleName = CurAttrName + TEXT("_scale");
					FString TransName = CurAttrName + TEXT("_translate");
					for (; It; ++It)
					{
						Transform = ColumnProp->ContainerPtrToValuePtr<FTransform3d>(It.Value());
						if (!Transform)
						{
							HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid transform value for property %s."), *ColumnProp->GetName());
							continue;
						}
						const FQuat4d& Rot = Transform->GetRotation();
						RotValues.Add(Rot.X);
						RotValues.Add(Rot.Y);
						RotValues.Add(Rot.Z);
						RotValues.Add(Rot.W);
						const FVector& Scale = Transform->GetScale3D();
						ScaleValues.Add(Scale.X);
						ScaleValues.Add(Scale.Y);
						ScaleValues.Add(Scale.Z);
						const FVector& Trans = Transform->GetTranslation();
						TransValues.Add(Trans.X);
						TransValues.Add(Trans.Y);
						TransValues.Add(Trans.Z);
					}

					AttributeInfo.tupleSize = 4;
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT64;
					// Rotation
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*RotName), &AttributeInfo), false);

					FHoudiniHapiAccessor Accessor;
					Accessor.Init(InputNodeId, 0, TCHAR_TO_ANSI(*RotName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, RotValues), false);

					AttributeInfo.tupleSize = 3;

					// Scale

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*ScaleName), &AttributeInfo), false);

					Accessor.Init(InputNodeId, 0, TCHAR_TO_ANSI(*ScaleName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, ScaleValues), false);

					// Translate
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*TransName), &AttributeInfo), false);

					Accessor.Init(InputNodeId, 0, TCHAR_TO_ANSI(*TransName));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, TransValues), false);

					continue;
				}
				else if (StructName == NAME_Transform3f)
				{
					FTransform3f* Transform;
					TArray<float> RotValues;
					TArray<float> ScaleValues;
					TArray<float> TransValues;
					RotValues.Reserve(4 * NumRows);
					ScaleValues.Reserve(3 * NumRows);
					TransValues.Reserve(3 * NumRows);
					for (; It; ++It)
					{
						Transform = ColumnProp->ContainerPtrToValuePtr<FTransform3f>(It.Value());
						if (!Transform)
						{
							HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid transform value for property %s."), *ColumnProp->GetName());
							continue;
						}
						const FQuat4f& Rot = Transform->GetRotation();
						RotValues.Add(Rot.X);
						RotValues.Add(Rot.Y);
						RotValues.Add(Rot.Z);
						RotValues.Add(Rot.W);
						const FVector3f& Scale = Transform->GetScale3D();
						ScaleValues.Add(Scale.X);
						ScaleValues.Add(Scale.Y);
						ScaleValues.Add(Scale.Z);
						const FVector3f& Trans = Transform->GetTranslation();
						TransValues.Add(Trans.X);
						TransValues.Add(Trans.Y);
						TransValues.Add(Trans.Z);
					}

					AttributeInfo.tupleSize = 4;
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
					// Rotation
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName + *TEXT("_rotation")), &AttributeInfo), false);

					FString Name = CurAttrName + TEXT("_rotation");
					FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*Name));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, RotValues), false);

					AttributeInfo.tupleSize = 3;
					// Scale
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName + *TEXT("_scale")), &AttributeInfo), false);

					Name = CurAttrName + *TEXT("_scale");
					Accessor.Init(InputNodeId, 0, TCHAR_TO_ANSI(*Name));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, ScaleValues), false);

					// Translate
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName + *TEXT("_translate")), &AttributeInfo), false);

					Name = CurAttrName + TEXT("_translate");
					Accessor.Init(InputNodeId, 0, TCHAR_TO_ANSI(*Name));
					HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, TransValues), false);

					continue;
				}

				if (NumComponents != -1 && Storage != HAPI_STORAGETYPE_INVALID)
				{
					AttributeInfo.tupleSize = NumComponents;
					AttributeInfo.storage = Storage;
					// Bytes
					if (Storage == HAPI_STORAGETYPE_UINT8)
					{
						TArray<uint8> Col = Order.Num() == 0 ? PopulateTArray<uint8, FByteProperty>(It, NumRows, NumComponents, Offset, 1) : PopulateTArray<uint8, FByteProperty>(It, NumRows, NumComponents, Offset, 1, Order);
						HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
							FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
							TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

						FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
						HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
						continue;
					}
					// Floats
					else if (Storage == HAPI_STORAGETYPE_FLOAT)
					{
						TArray<float> Col = Order.Num() == 0 ? PopulateTArray<float, FFloatProperty>(It, NumRows, NumComponents, Offset, 4) : PopulateTArray<float, FFloatProperty>(It, NumRows, NumComponents, Offset, 4, Order);
						HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
							FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
							TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

						FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
						HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
						continue;
					}
					// Doubles
					else if (Storage == HAPI_STORAGETYPE_FLOAT64)
					{
						TArray<double> Col = Order.Num() == 0 ? PopulateTArray<double, FDoubleProperty>(It, NumRows, NumComponents, Offset, 8) : PopulateTArray<double, FDoubleProperty>(It, NumRows, NumComponents, Offset, 8, Order);
						HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
							FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
							TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

						FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
						HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);

						continue;
					}
					// Something else
					else
					{
						HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid component size %d for property %s."), ColumnProp->GetSize(), *ColumnProp->GetName());
						continue;
					}
				}
			}
			// Fallback to string if no match
			AttributeInfo.tupleSize = 1;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			TArray<FString> Col;
			Col.Reserve(NumRows);
			for (; It; ++It)
			{
				Col.Add(DataTableUtils::GetPropertyValueAsString(ColumnProp, It.Value(), EDataTableExportFlags()));
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
				TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

			FHoudiniHapiAccessor Accessor(InputNodeId, 0, TCHAR_TO_ANSI(*CurAttrName));
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfo, Col), false);
		}
	}

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(InputNodeId), false);
	if (!FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true))
		return false;

	{
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return true;
}
