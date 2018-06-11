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
*/

#pragma once

#include "Modules/ModuleInterface.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAPI_Common.h"
#include "HoudiniGeoPartObject.h"


class UMaterial;
class UStaticMesh;
class ISlateStyle;
class UHoudiniAsset;
struct FHoudiniEngineNotificationInfo;
struct FHoudiniEngineTask;
struct FHoudiniEngineTaskInfo;
struct HAPI_Session;
struct FHoudiniCookParams;
class ALandscape;


class IHoudiniEngine : public IModuleInterface
{
public:

#if WITH_EDITOR

    /** Return Houdini logo brush. **/
    virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const = 0;

#endif

    /** Return static mesh reprensenting Houdini logo. **/
    virtual TWeakObjectPtr<UStaticMesh> GetHoudiniLogoStaticMesh() const = 0;

    /** Return default material. **/
    virtual TWeakObjectPtr<UMaterial> GetHoudiniDefaultMaterial() const = 0;

    /** Return Houdini digital asset used for bgeo file loading. **/
    virtual TWeakObjectPtr<UHoudiniAsset> GetHoudiniBgeoAsset() const = 0;

    /** Return true if HAPI version mismatch is detected (between defined and running versions). **/
    virtual bool CheckHapiVersionMismatch() const = 0;

    /** Return current HAPI state. **/
    virtual HAPI_Result GetHapiState() const = 0;

    /** Set HAPI state. **/
    virtual void SetHapiState(HAPI_Result Result) = 0;

    /** Return location of libHAPI binary. **/
    virtual const FString& GetLibHAPILocation() const = 0;

    /** Register task for execution. **/
    virtual void AddTask(const FHoudiniEngineTask& Task) = 0;

    /** Register task info. **/
    virtual void AddTaskInfo(const FGuid HapIGUID, const FHoudiniEngineTaskInfo& TaskInfo) = 0;

    /** Remove task info. **/
    virtual void RemoveTaskInfo(const FGuid HapIGUID) = 0;

    /** Retrieve task info. **/
    virtual bool RetrieveTaskInfo(const FGuid HapIGUID, FHoudiniEngineTaskInfo& TaskInfo) = 0;

    /** Retrieve HAPI session. **/
    virtual const HAPI_Session* GetSession() const = 0;

    /** Construct static meshes for a given Houdini asset. **/
    virtual bool CookNode(
        HAPI_NodeId AssetId, FHoudiniCookParams& HoudiniCookParams,
        bool ForceRebuildStaticMesh, bool ForceRecookAll,
        const TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesIn,
        TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshesOut,
        TMap< FHoudiniGeoPartObject, ALandscape * >& LandscapesIn,
        TMap< FHoudiniGeoPartObject, ALandscape * >& LandscapesOut,
        TMap< FHoudiniGeoPartObject, USceneComponent * >& InstancersIn,
        TMap< FHoudiniGeoPartObject, USceneComponent * >& InstancersOut,
        USceneComponent* ParentComponent,
        FTransform & ComponentTransform ) = 0;
};
