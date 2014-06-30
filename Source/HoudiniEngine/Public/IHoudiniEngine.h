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

struct FHoudiniEngineNotificationInfo;
struct FHoudiniEngineTask;

class IHoudiniEngine : public IModuleInterface
{
public:

	/** Return Houdini logo brush. **/
	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const = 0;

	/** Register task for execution. **/
	virtual void AddTask(const FHoudiniEngineTask& Task) = 0;
	
	/** Add new notification item. **/
	//virtual void AddNotification(FHoudiniEngineNotificationInfo* Notification) = 0;

	/** Remove existing notification item. **/
	//virtual void RemoveNotification(FHoudiniEngineNotificationInfo* Notification) = 0;

	/** Update notification. **/
	//virtual void UpdateNotification(FHoudiniEngineNotificationInfo* Notification) = 0;
};
