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

#include "HoudiniEnginePrivatePCH.h"

FHoudiniAssetComponentInstanceData::FHoudiniAssetComponentInstanceData(const UHoudiniAssetComponent* SourceComponent) :
	FComponentInstanceDataBase(SourceComponent)
{

}


FHoudiniAssetComponentInstanceData::~FHoudiniAssetComponentInstanceData()
{

}


bool
FHoudiniAssetComponentInstanceData::MatchesComponent(const UActorComponent* Component) const
{
	return true;
}
