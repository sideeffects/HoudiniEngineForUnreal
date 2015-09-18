/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HoudiniGeoPartObject.h"
#include "HoudiniHandleComponent.generated.h"

class UHoudiniAssetInput;

UCLASS(config=Engine)
class HOUDINIENGINERUNTIME_API UHoudiniHandleComponent : public USceneComponent
{
	friend class UHoudiniAssetComponent;

#if WITH_EDITOR

	friend class FHoudiniHandleComponentVisualizer;

#endif

	GENERATED_UCLASS_BODY()

	virtual ~UHoudiniHandleComponent();

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditUndo() override;

public:
	bool Construct( HAPI_AssetId AssetId, int32 HandleIdx, const FString& HandleName,
					const HAPI_HandleInfo&, const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& );

protected:

private:
	template <class CONCRETE_PARM, typename VALUE>
	static bool BindHandleParameter(
		VALUE& OutValue,
		const char* CmpName,
		const FString& HandleParmName,
		HAPI_ParmId AssetParamId,
		const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& Parameters )
	{
		if (HandleParmName == CmpName)
		{
			if ( UHoudiniAssetParameter* const* FoundAbstractParm = Parameters.Find(AssetParamId) )
			{
				if ( CONCRETE_PARM* ConcreteParm = Cast<CONCRETE_PARM>(*FoundAbstractParm) )
				{
					auto Optional = ConcreteParm->GetValue(0);
					if ( Optional.IsSet() )
					{
						OutValue = static_cast<VALUE>( Optional.GetValue() );
						return true;
					}
				}
			}
		}

		return false;
	}
};
