/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once

class IHoudiniEngine : public IModuleInterface
{
public:

	/** Return Houdini logo brush. **/
	virtual TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const = 0;
};
