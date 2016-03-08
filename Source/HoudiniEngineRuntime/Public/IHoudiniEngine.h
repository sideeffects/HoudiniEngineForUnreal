/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once


class UMaterial;
class UStaticMesh;
class ISlateStyle;
class UHoudiniAsset;
struct FHoudiniEngineNotificationInfo;
struct FHoudiniEngineTask;
struct FHoudiniEngineTaskInfo;
struct HAPI_Session;


class IHoudiniEngine : public IModuleInterface
{
public:

#if WITH_EDITOR

	/** Return Houdini logo brush. **/
	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const = 0;

#endif

	/** Return static mesh reprensenting Houdini logo. **/
	virtual UStaticMesh* GetHoudiniLogoStaticMesh() const = 0;

	/** Return default material. **/
	virtual UMaterial* GetHoudiniDefaultMaterial() const = 0;

	/** Return Houdini digital asset used for bgeo file loading. **/
	virtual UHoudiniAsset* GetHoudiniBgeoAsset() const = 0;

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
};
