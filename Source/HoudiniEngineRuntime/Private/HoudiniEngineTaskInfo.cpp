/*
* Copyright (c) <2017> Side Effects Software Inc.
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
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniApi.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

FHoudiniEngineTaskInfo::FHoudiniEngineTaskInfo()
    : Result( HAPI_RESULT_SUCCESS )
    , AssetId( -1 )
    , TaskType( EHoudiniEngineTaskType::None )
    , TaskState( EHoudiniEngineTaskState::None )
    , bLoadedComponent( false )
{}

FHoudiniEngineTaskInfo::FHoudiniEngineTaskInfo(
    HAPI_Result InResult, HAPI_AssetId InAssetId,
    EHoudiniEngineTaskType::Type InTaskType,
    EHoudiniEngineTaskState::Type InTaskState )
    : Result( InResult )
    , AssetId( InAssetId )
    , TaskType( InTaskType )
    , TaskState( InTaskState )
    , bLoadedComponent( false )
{}
