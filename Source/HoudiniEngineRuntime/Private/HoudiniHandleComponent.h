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
#include "HoudiniAssetParameterFloat.h"
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

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

private:
	template <class ASSET_PARM>
	class THandleParameter
	{
	public:
		THandleParameter()
			: AssetParameter(nullptr)
		{}

		template <typename VALUE>
		bool Bind(
			VALUE& OutValue,
			const char* CmpName,
			int32 InTupleIdx,
			const FString& HandleParmName,
			HAPI_ParmId AssetParamId,
			const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& Parameters )
		{			
			if (HandleParmName == CmpName)
			{
				if ( UHoudiniAssetParameter* const* FoundAbstractParm = Parameters.Find(AssetParamId) )
				{
					AssetParameter = Cast<ASSET_PARM>(*FoundAbstractParm);
					if ( AssetParameter )
					{
						auto Optional = AssetParameter->GetValue(InTupleIdx);
						if ( Optional.IsSet() )
						{
							TupleIdx = InTupleIdx;
							OutValue = static_cast<VALUE>( Optional.GetValue() );
							return true;
						}
					}
				}
			}

			return false;
		}

		template <typename VALUE>
		THandleParameter& operator+=(VALUE Delta)
		{
			if ( AssetParameter )
			{
				auto Optional = AssetParameter->GetValue(TupleIdx);
				if ( Optional.IsSet() )
				{
					AssetParameter->SetValue( Optional.GetValue() + Delta, TupleIdx );
				}
			}

			return *this;
		}
		
		ASSET_PARM* AssetParameter;
		int32 TupleIdx;
	};

	struct EXformParameter
	{
		enum Type
		{
			TX, TY, TZ,
			RX, RY, RZ,
			SX, SY, SZ,
			COUNT
		};		
	};

	typedef THandleParameter<UHoudiniAssetParameterFloat> FXformParameter;
	FXformParameter XformParms[EXformParameter::COUNT];
};
