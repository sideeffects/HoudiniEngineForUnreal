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

#pragma once

#include "HoudiniAssetComponent.h"

namespace EHoudiniEngineTaskType
{
    enum Type
    {
        None,

        /** This type corresponds to Houdini asset instantiation (without cooking). **/
        AssetInstantiation,

        /** This type corresponds to Houdini asset cooking request. **/
        AssetCooking,

        /** This type is used for asynchronous asset deletion. **/
        AssetDeletion
    };
}

class UHoudiniAsset;
class UHoudiniAssetComponent;

struct FHoudiniEngineTask
{
    /** Constructors. **/
    FHoudiniEngineTask();
    FHoudiniEngineTask( EHoudiniEngineTaskType::Type InTaskType, FGuid InHapiGUID );

    /** GUID of this request. **/
    FGuid HapiGUID;

    /** Type of this task. **/
    EHoudiniEngineTaskType::Type TaskType;

    /** Houdini asset for instantiation. **/
    TWeakObjectPtr< UHoudiniAsset > Asset;

    /** Houdini asset component for cooking. **/
    TWeakObjectPtr< UHoudiniAssetComponent > AssetComponent;

    /** Name of the actor requesting this task. **/
    FString ActorName;

    /** Asset Id. **/
    HAPI_NodeId AssetId;

    /** Library Id. **/
    HAPI_AssetLibraryId AssetLibraryId;

    /** HAPI name of the asset. **/
    int32 AssetHapiName;

    /** Is set to true if component has been loaded. **/
    bool bLoadedComponent;
};
