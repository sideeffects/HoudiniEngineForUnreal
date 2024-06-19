
#include "HoudiniFoliageUtils.h"

#include "FoliageType.h"
#include "InstancedFoliage.h"
#include "EngineUtils.h"

void
FHoudiniFoliageUtils::RemoveFoliageTypeFromWorld(UWorld* World, UFoliageType* FoliageType)
{

#if WITH_EDITOR
	// Avoid needlessly dirtying every FoliageInstanceActor by calling RemoveFoliageType() with null
	if (!IsValid(FoliageType))
		return;

	for (TActorIterator<AInstancedFoliageActor> ActorIt(World, AInstancedFoliageActor::StaticClass()); ActorIt; ++ActorIt)
	{
		AInstancedFoliageActor* IFA = *ActorIt;

		// Check to see if the IFA actually contains the FoliageType, then remove it. Do this so we
		// don't dirty IFAs needlessly because Unreal doesn't check for this and always marks the actor as dirty.
		FFoliageInfo* Info = IFA->FindInfo(FoliageType);
		if (Info != nullptr)
		{
			IFA->RemoveFoliageType(&FoliageType, 1);
		}
	}
#endif
}

