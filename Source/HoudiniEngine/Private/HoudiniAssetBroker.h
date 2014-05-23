/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
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

class UObject;
class UActorComponent;

class FHoudiniAssetBroker : public IComponentAssetBroker
{
public: /** IComponentAssetBroker methods. **/

	/** Reports the asset class this broker knows how to handle. **/
	UClass* GetSupportedAssetClass() OVERRIDE;

	/** Assign the assigned asset to the supplied component. **/
	bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) OVERRIDE;

	/** Get the currently assigned asset from the component. **/
	UObject* GetAssetFromComponent(UActorComponent* InComponent) OVERRIDE;
};
